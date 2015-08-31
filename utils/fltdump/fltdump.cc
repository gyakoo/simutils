
#pragma warning(disable:4100 4005)

#if defined(_DEBUG) && !defined(_WIN64)
#include <vld.h>
#endif

#include <stdio.h>
#include <stdlib.h>
//#define FLT_NO_OPNAMES
//#define FLT_LEAN_FACES
//#define FLT_ALIGNED
//#define FLT_UNIQUE_FACES
#define FLT_IMPLEMENTATION
#include <flt.h>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <io.h>
#include <string>
#include <map>
#include <set>
#include <mutex>
#include <atomic>
#include <deque>
#include <thread>
#include <vector>

struct fltThreadPool;
struct fltThreadPoolContext;
struct fltThreadTask;

struct fltImgAttr
{
  int width, height;
  int depth, size;
};

typedef std::map<std::string, fltImgAttr> fltImgMap;

// types
struct fltThreadTask
{
  enum f2dTaskType { TASK_NONE=-1, TASK_FLT=0, TASK_IMGATTR=1};
  f2dTaskType type;

  std::string filename;
  std::string basename;
  flt* of;
  flt_opts* opts;

  fltThreadTask():type(TASK_NONE), of(NULL), opts(NULL){}
  fltThreadTask(f2dTaskType tt, const std::string& f, flt* of=0, flt_opts* opts=0)
    :type(tt), filename(f), of(of), opts(opts){}
  
  void runTask(fltThreadPool* tp);
};


struct fltThreadPool
{
  void init();
  void runcode();
  bool getNextTask(fltThreadTask* t);
  void addNewTask(const fltThreadTask& task);
  void addNewImageAttr(const std::string& fname, const fltImgAttr& att);
  void deinit();
  bool isWorking(){ return !tasks.empty() || workingTasks>0; }

  fltImgMap imgattrs;
  std::vector<std::string> searchpaths;
  std::vector<std::thread*> threads;
  std::deque<fltThreadTask> tasks;
  std::mutex mtxTasks;
  std::mutex mtxFiles;
  std::mutex mtxImgAttrs;
  std::atomic_int workingTasks;
  std::atomic_int nfiles;
  int numThreads;
  bool finish;
  bool inspectTexture;
};

double fltGetTime();
bool fltExtensionIsImg(const char* filename);
bool fltExtensionIs(const char* filename, const char* ext);
void fltXmlOfPrint(flt* of, int d, std::set<uint64_t>& done, int* tot_nodes, fltThreadPool* tp);
bool fltReadImageAttr(const char* filename, fltImgAttr* attr);

void fltThreadPool::init()
{
  deinit();
  finish=false;
  threads.reserve(numThreads);
  workingTasks=0;
  nfiles=0;
  for(int i = 0; i < numThreads; ++i)
    threads.push_back(new std::thread(&fltThreadPool::runcode,this));
  inspectTexture=false;
}

void fltThreadPool::runcode()
{
  fltThreadTask task;
  while (!finish)
  {
    if ( getNextTask(&task) )
    {
      ++workingTasks;
      try
      {
        task.runTask(this);
      }catch(...)
      {
        OutputDebugStringA("Exception\n");
      }
      --workingTasks;
    }
    else
      std::this_thread::sleep_for(std::chrono::milliseconds(2));
  }
}

bool fltThreadPool::getNextTask(fltThreadTask* t)
{
  std::lock_guard<std::mutex> lock(mtxTasks);
  if ( tasks.empty() ) return false;
  *t = tasks.back();
  tasks.pop_back();
  return true;
}

void fltThreadPool::addNewTask(const fltThreadTask& task)
{
  // not done yet, process
  std::lock_guard<std::mutex> lock(mtxTasks);
  tasks.push_front( task );
}

void fltThreadPool::addNewImageAttr(const std::string& fname, const fltImgAttr& att)
{
  std::lock_guard<std::mutex> lock(mtxImgAttrs);
  imgattrs[fname]=att;
}

void fltThreadPool::deinit()
{
  finish = true;
  for each (std::thread* t in threads)
  {
    t->join();
    delete t;
  }
  threads.clear();
  tasks.clear();
}

void fltThreadTask::runTask(fltThreadPool* tp)
{
  tp=tp;
  switch( type )
  {
  case TASK_FLT    : flt_load_from_filename(filename.c_str(), of, opts); break;
  case TASK_IMGATTR: 
    {
      fltImgAttr attr;
      if ( fltReadImageAttr(filename.c_str(), &attr) )
        tp->addNewImageAttr(basename.c_str(), attr);
    }break;
  }
}

bool fltExtensionIs(const char* filename, const char* ext)
{
  const size_t len1=strlen(filename);
  const size_t len2=strlen(ext);
  const char* str=strstr(filename,ext);
  return ( str && int(str-filename+1)==int(len1-len2+1));
}

bool fltExtensionIsImg(const char* filename)
{
  static const char* supported[9]={".rgb", ".rgba", ".sgi", ".dds", ".jpg", ".jpeg", ".png", ".tga", ".bmp"};
  for (int i=0;i<9;++i)
  {
    if ( fltExtensionIs(filename,supported[i]) )
      return true;
  }
  return false;
}

double fltGetTime()
{
#ifdef _MSC_VER
  double f;
  LARGE_INTEGER t, freq;
  QueryPerformanceFrequency(&freq);
  f=1000.0/(double)(freq.QuadPart);
  QueryPerformanceCounter(&t);
  return (double)(t.QuadPart)*f;
#else
  return 0.0;
#endif
}

void fltCountNode(flt_node* n, int* tris)
{
  if ( !n ) return;
  
  while (n)
  {
#ifdef FLT_UNIQUE_FACES
    if ( n->ndx_pairs_count )
    {
      fltu32 start,end;
      for (fltu32 i=0;i<n->ndx_pairs_count;++i)
      {
        FLTGET32(n->ndx_pairs[i],start,end);
        *tris += (end-start+1)/3;
      }
    }
#else
    if (n->type == FLT_NODE_VLIST)
    {
      flt_node_vlist* vl = (flt_node_vlist*)n;
      *tris += vl->count-2;
    }
#endif
    else
    {
      fltCountNode(n->child_head,tris);
    }
    n = n->next;
  }
  
}

void fltCountOf(flt* of, int* tris)
{
  if ( !of || !of->hie ) return;

  fltCountNode(of->hie->node_root,tris);  
}


void fltXmlIndent(int d){ for ( int i = 0; i < d; ++i ) printf( "  " ); }
void fltXmlNodePrint(flt_node* n, int d)
{
  static const char* names[FLT_NODE_MAX]={"none","base","xref","gro","obj","mes","lod","fac", "vli", "swi"};
  static char tmp[256];
  static fltu32 start,end;
  if ( !n ) return;
  // my siblings and children
  do
  {
    fltXmlIndent(d); 
    if ( n->child_count == 0 )
    {
      if ( n->type==FLT_NODE_VLIST )
      {
        flt_node_vlist* vl=(flt_node_vlist*)n;
        printf( "<%s inds=\"%d\"/>\n", names[n->type], vl->count);
      }
      else
      {
		    *tmp = 0;
        if ( n->name && *n->name ) 
          sprintf_s(tmp,"name=\"%s\"",n->name);

#ifdef FLT_UNIQUE_FACES
        if (n->ndx_pairs_count)
        {
          int inds=0;
          for ( fltu32 i=0;i<n->ndx_pairs_count;++i ) { FLTGET32(n->ndx_pairs[i],start,end); inds += end-start+1; }
          sprintf_s(tmp, "%s inds=\"%d\"", tmp, inds);
        }
#endif
        if ( *tmp ) printf( "<%s %s/>\n", names[n->type], tmp);
        else printf( "<%s/>\n", names[n->type]);
      }
    }
    else
    {
      if ( n->name && *n->name ) printf( "<%s name=\"%s\">\n", names[n->type], n->name);
      else printf( "<%s>\n", names[n->type]);
    }

    fltXmlNodePrint(n->child_head,d+1);
    if ( n->child_count )
    {
      fltXmlIndent(d); printf( "</%s>\n", names[n->type]);
    }
    n = n->next;
  }while(n);
}

void fltXmlOfPrint(flt* of, int d, std::set<uint64_t>& done, int* tot_nodes, fltThreadPool* tp)
{
  if ( !of || of->loaded != FLT_LOADED ) return;
  
  fltXmlIndent(d); printf( "<file name=\"%s\">\n", of->filename);
  if ( of->header )
  {
    fltXmlIndent(d+1); printf( "<header ascii=\"%s\">\n", of->header->ascii);
    fltXmlIndent(d+2); printf( "<format_rev>%d</format_rev>\n", of->header->format_rev );
    fltXmlIndent(d+2); printf( "<edit_rev>%d</edit_rev>\n", of->header->edit_rev );
    fltXmlIndent(d+2); printf( "<dtime>%s</dtime>\n", of->header->dtime );
    fltXmlIndent(d+2); printf( "<unit_mult>%d</unit_mult>\n", of->header->unit_mult );
    fltXmlIndent(d+2); printf( "<v_coords_units>%s</v_coords_units>\n", flt_get_vcoords_units_name(of->header->v_coords_units) );
    fltXmlIndent(d+2); printf( "<flags>0x%08x</flags>\n", of->header->flags );
    fltXmlIndent(d+2); printf( "<proj_type>%s</proj_type>\n", flt_get_proj_name(of->header->proj_type) );
    fltXmlIndent(d+2); printf( "<db_orig>%s</db_orig>\n", flt_get_db_origin_name(of->header->db_orig) );
    fltXmlIndent(d+2); printf( "<sw_db_x>%g</sw_db_x>\n", of->header->sw_db_x );
    fltXmlIndent(d+2); printf( "<sw_db_y>%g</sw_db_y>\n", of->header->sw_db_y );
    fltXmlIndent(d+2); printf( "<d_db_x>%g</d_db_x>\n", of->header->d_db_x );
    fltXmlIndent(d+2); printf( "<d_db_y>%g</d_db_y>\n", of->header->d_db_y );
    fltXmlIndent(d+2); printf( "<sw_corner_lat>%g</sw_corner_lat>\n", of->header->sw_corner_lat );
    fltXmlIndent(d+2); printf( "<sw_corner_lon>%g</sw_corner_lon>\n", of->header->sw_corner_lon );
    fltXmlIndent(d+2); printf( "<ne_corner_lat>%g</ne_corner_lat>\n", of->header->ne_corner_lat );
    fltXmlIndent(d+2); printf( "<ne_corner_lon>%g</ne_corner_lon>\n", of->header->ne_corner_lon );
    fltXmlIndent(d+2); printf( "<orig_lat>%g</orig_lat>\n", of->header->orig_lat );
    fltXmlIndent(d+2); printf( "<orig_lon>%g</orig_lon>\n", of->header->orig_lon );
    fltXmlIndent(d+2); printf( "<lbt_upp_lat>%g</lbt_upp_lat>\n", of->header->lbt_upp_lat );
    fltXmlIndent(d+2); printf( "<lbt_low_lat>%g</lbt_low_lat>\n", of->header->lbt_low_lat );
    fltXmlIndent(d+2); printf( "<earth_ellip_model>%s</earth_ellip_model>\n", flt_get_earth_ellip_name(of->header->earth_ellip_model) );
    fltXmlIndent(d+2); printf( "<utm_zone>%d</utm_zone>\n", of->header->utm_zone );
    fltXmlIndent(d+2); printf( "<d_db_z>%g</d_db_z>\n", of->header->d_db_z );
    fltXmlIndent(d+2); printf( "<radius>%g</radius>\n", of->header->radius );
    fltXmlIndent(d+2); printf( "<earth_major_axis>%g</earth_major_axis>\n", of->header->earth_major_axis );
    fltXmlIndent(d+2); printf( "<earth_minor_axis>%g</earth_minor_axis>\n", of->header->earth_minor_axis );
    fltXmlIndent(d+1); printf( "</header>\n");
  }

  // counters
  fltXmlIndent(d+1); printf( "<counters>\n");
  {
    fltXmlIndent(d+2); printf( "<records>%d</records>\n", of->ctx->rec_count );
    if ( of->hie && of->hie->node_count) 
    { 
      fltXmlIndent(d+2); printf( "<nodes>%d</nodes>\n", of->hie->node_count); 
      *tot_nodes += of->hie->node_count; 
    }
    if ( of->pal )
    {
      if ( of->pal->vtx_count )
      {
        fltXmlIndent(d+2); printf( "<vertices>%d</vertices>\n", of->pal->vtx_count);
      }

      int tris=0;
      fltCountOf(of,&tris);
      if ( tris )
      {
        fltXmlIndent(d+2); printf( "<triangles>%d</triangles>\n", tris);
      }

      if ( of->pal->tex_count )
      {
        fltXmlIndent(d+2); printf( "<textures>%d</textures>\n", of->pal->tex_count);
      }
    }
  }
  fltXmlIndent(d+1); printf( "</counters>\n");

  // palettes
  if ( of->pal )
  {
    if(of->pal->tex_count)
    { 
      flt_pal_tex* tex = of->pal->tex_head;
      fltImgMap::iterator it;
      fltXmlIndent(d+1); printf( "<textures>\n");
      while (tex)
      {
        it = tp->imgattrs.find(tex->name);
        if ( it != tp->imgattrs.end() )
        {
          fltXmlIndent(d+2); 
          if ( it->second.depth>=0 )
            printf( "<tex w=\"%d\" h=\"%d\" d=\"%d\" size=\"%d\">%s</tex>\n" , it->second.width, it->second.height, it->second.depth, it->second.size, tex->name);
          else
            printf( "<tex w=\"%d\" h=\"%d\" f=\"DXT%d\" size=\"%d\">%s</tex>\n" , it->second.width, it->second.height, -it->second.depth, it->second.size, tex->name);
        }
        else
        {
          fltXmlIndent(d+2); printf( "<tex>%s</tex>\n", tex->name );
        }
        tex = tex->next;
      } 
      fltXmlIndent(d+1); printf( "</textures>\n");
    }
  }
  
  // hierarchy and recursive print
  if ( of->hie )
  {
    // hierarchy
    fltXmlNodePrint(of->hie->node_root,d+1);

    // flat list of extrefs
    if ( of->hie->extref_count)
    {
      fltXmlIndent(d+1); printf( "<xrefs>\n" );
      flt_node_extref* extref = of->hie->extref_head;
      while ( extref )
      {
        if ( done.find( (uint64_t)extref->of ) == done.end() )
        {
          fltXmlOfPrint(extref->of, d+2, done,tot_nodes,tp);
          done.insert( (uint64_t)extref->of );
        }
        extref= (flt_node_extref*)extref->next_extref;
      }
      fltXmlIndent(d+1); printf( "</xrefs>\n" );
    }
  }
  fltXmlIndent(d); printf( "</file>\n");
}

void fltXmlPrint(flt* of, flt_opts* opts, int nfiles, double tim, fltThreadPool* tp)
{
  std::set<uint64_t> done;
  int counterctx=0;
  
  printf("<root>\n");

  fltXmlIndent(1); printf("<stats>\n");
  fltXmlIndent(2); printf("<loadtime>%.4g</loadtime>\n",tim);
  fltXmlIndent(2); printf("<files>%d</files>\n", nfiles);
  fltXmlIndent(2); printf("<faces>%d</faces>\n", TOTALNFACES);
#ifdef FLT_UNIQUE_FACES
  fltXmlIndent(2); printf("<unique_faces>%d</unique_faces>\n", TOTALUNIQUEFACES);
#endif
  fltXmlIndent(2); printf("<indices>%d</indices>\n", TOTALINDICES);
  fltXmlIndent(2); printf("<opcodes>\n");
  if ( opts->countable )
  {
    for ( int i = 0; i < FLT_OP_MAX; ++i )
    {
      if ( opts->countable[i] )
      {
        fltXmlIndent(3); printf( "<op_%03d name=\"%s\">%d</op_%03d>\n", i, flt_get_op_name((fltu16)i), opts->countable[i], i);
      }
    }
  }
  fltXmlIndent(2); printf("</opcodes>\n");
  fltXmlIndent(1); printf("</stats>\n");

  fltXmlOfPrint(of,1,done,&counterctx, tp);

  printf( "</root>\n");
}

//////////////////////////////////////////////////////////////////////////
// This callback is a good place to build a full path to the external reference
// It can be done by building a dictionary with a recursive listing of *.flt
// under the original filename (master) directory, and here build the finalname
//////////////////////////////////////////////////////////////////////////
int fltCallbackExtRef(flt_node_extref* extref, flt* of, void* userdata)
{
  char* finalname = flt_extref_prepare(extref,of);
  if ( finalname )
  {
    fltThreadPool* tp=reinterpret_cast<fltThreadPool*>(userdata);
    fltThreadTask task(fltThreadTask::TASK_FLT, finalname, extref->of, of->ctx->opts); //  read ref in parallel
    tp->addNewTask(task);
    flt_safefree(finalname);
    ++tp->nfiles;
  }
  return 1;
}

int fltCallbackTexture(struct flt_pal_tex* texpal, struct flt* of, void* user_data)
{
  fltThreadPool* tp=reinterpret_cast<fltThreadPool*>(user_data);

  if ( !fltExtensionIsImg(texpal->name) || !tp->inspectTexture )
    return 1;

  std::string fname = of->ctx->basepath;
  if ( !flt_path_endsok(fname.c_str()) ) fname += "/";
  fname += texpal->name;

  fltImgMap::iterator it = tp->imgattrs.find(texpal->name);
  if ( it == tp->imgattrs.end() )
  {
    fltThreadTask task(fltThreadTask::TASK_IMGATTR, fname); //  read tex in parallel
    task.basename=texpal->name;
    tp->addNewTask(task);
  }
  return 1;
}


void read_with_callbacks_mt(const std::vector<std::string>& files, bool inspectTex)
{
  fltThreadPool tp;
  tp.numThreads = std::thread::hardware_concurrency()*2;
  tp.init();
  tp.inspectTexture=inspectTex;

  fltu16 vtxstream[]={
   flt_vtx_stream_enc(FLT_VTX_POSITION , 0,12) 
//   ,flt_vtx_stream_enc(FLT_VTX_COLOR    ,12, 4)
//   ,flt_vtx_stream_enc(FLT_VTX_NORMAL   ,16,12)
//  ,flt_vtx_stream_enc(FLT_VTX_UV0      ,12, 8)
  ,FLT_NULL};

  // configuring read options
  flt_opts* opts=(flt_opts*)flt_calloc(1,sizeof(flt_opts));
  flt* of=(flt*)flt_calloc(1,sizeof(flt));
  opts->palflags = FLT_OPT_PAL_ALL;
  opts->hieflags = FLT_OPT_HIE_ALL_NODES | FLT_OPT_HIE_HEADER;
  opts->dfaces_size = 1543;
  opts->vtxstream = vtxstream;
  opts->cb_texture = fltCallbackTexture;
  opts->cb_extref = fltCallbackExtRef;
  opts->cb_user_data = &tp;
  opts->countable = (fltu32*)flt_calloc(1, sizeof(fltu32)*FLT_OP_MAX);

  // master file task
  tp.nfiles=1;
  fltThreadTask task(fltThreadTask::TASK_FLT, files[0].c_str(), of, opts);
  tp.addNewTask(task);  

  // wait until cores finished
  double t0=fltGetTime();
  do
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));    
  } while ( tp.isWorking() );

  // printing
  fltXmlPrint(of, opts, tp.nfiles, (fltGetTime()-t0)/1000.0, &tp);

  flt_release(of);
  flt_safefree(of);
  flt_safefree(opts->countable);
  flt_safefree(opts);

  tp.deinit();
}

int fltCountBits(int i)
{
  i = i - ((i >> 1) & 0x55555555);
  i = (i & 0x33333333) + ((i >> 2) & 0x33333333);
  return (((i + (i >> 4)) & 0x0F0F0F0F) * 0x01010101) >> 24;
}

#pragma warning(disable:4996)
bool fltReadImageAttr(const char* filename, fltImgAttr* attr)
{
  attr->width=attr->height=0;
  attr->depth=attr->size=0;
  char c;
  unsigned short u16;
  FILE* f = fopen(filename,"rb");
  if ( !f ) return false;

  if ( fltExtensionIs(filename,".rgb") || fltExtensionIs(filename,".rgba") || fltExtensionIs(filename,".sgi") )
  {
    fseek(f,6,SEEK_SET);    
    fread(&u16, 1, 2, f); flt_swap16(&u16); attr->width=u16;
    fread(&u16, 1, 2, f); flt_swap16(&u16); attr->height=u16;    
    fread(&u16, 1, 2, f); flt_swap16(&u16); attr->depth=u16<<3;
  }
  else if ( fltExtensionIs(filename, ".dds") )
  {
    fseek(f,12,SEEK_SET);
    fread(&attr->height, 1, 4, f);
    fread(&attr->width, 1, 4, f);
    fseek(f,12+44+4,SEEK_CUR);
    unsigned int flags, fourcc, rgbbc, masks[4];
    fread(&flags,1,4,f);
    fread(&fourcc,1,4,f);
    fread(&rgbbc,1,4,f);
    fread(masks,4,4,f);
    if (flags & 0x4 )
    {
      const char* fcc=(const char*)&fourcc;
      attr->depth = -(fcc[3]-'1'+1);
    }
    else if ( (flags & 0x1) || (flags & 0x40) )
    {
      unsigned int allm=masks[0]|masks[1]|masks[2]|masks[3];
      attr->depth = fltCountBits(allm);
    }
  }
  else if ( fltExtensionIs(filename, ".jpeg") || fltExtensionIs(filename,".jpg") )
  {
    unsigned char tmp[2048];    
    int r = fread(tmp,1,2048,f);
    int i = 0;
    while ((i++)<r)
    {
      if ( tmp[i]!=0xff ) continue;
      ++i; if ( i>=r ) break;
      switch(tmp[i])
      {
      case 0xc0:
      case 0xc1:
      case 0xc2:        
        i+=4; if ( i>=r ) break;
        u16 = *(unsigned short*)(tmp+i);flt_swap16(&u16); attr->width=u16;i+=2;
        u16= *(unsigned short*)(tmp+i);flt_swap16(&u16); attr->height=u16;i+=2;
        attr->depth= ( *(unsigned char*)(tmp+i) ) << 3;
        i=r;
        break;
      }
    }
  }
  else if (fltExtensionIs(filename, ".png") )
  {
    fseek(f, 16, SEEK_SET);
    fread(&attr->width, 1, 4, f); flt_swap32(&attr->width);
    fread(&attr->height, 1, 4, f); flt_swap32(&attr->height);
    fread(&c,1,1,f); attr->depth = c<<3;
  }
  else if (fltExtensionIs(filename, ".tga"))
  {
    fseek(f,12,SEEK_SET);
    fread(&u16, 1, 2, f); attr->width=u16;
    fread(&u16, 1, 2, f); attr->height=u16;
    fread(&c,1,1,f); attr->depth = c;
  }
  else if (fltExtensionIs(filename,".bmp"))
  {
    fseek(f,14,SEEK_SET);
    fread(&u16, 1, 2, f); attr->width=u16;
    fread(&u16, 1, 2, f); attr->height=u16;
    fseek(f,2,SEEK_CUR);
    fread(&u16, 1, 2, f); attr->depth=u16;
  }

  fseek(f,0,SEEK_END);
  attr->size = ftell(f);
  fclose(f);
  return true;
}
#pragma warning(default:4996)


int main(int argc, const char** argv)
{
  int c = fltCountBits(0xffffffff);
  c = fltCountBits(0xff00);
  c = fltCountBits(0xffff);

  bool inspectTex=false;  
  std::vector<std::string> files;
  for ( int i = 1; i < argc; ++i )
  {
    if ( argv[i][0]=='-' )
    {
      if ( argv[i][1]=='t' )
        inspectTex = true;
    }
    else
      files.push_back( argv[i] );
  }

  if ( !files.empty() )
  {
    read_with_callbacks_mt(files, inspectTex);
  }
  else
  {
    char* program=flt_path_basefile(argv[0]);
#ifdef FLT_UNIQUE_FACES
    printf( "fltdumpu: Dumps (Unique Faces version) Openflight metadata in xml format\n\n" );    
#else
    printf( "fltdump: Dumps Openflight metadata in xml format\n\n" );    
#endif
    printf( "Usage: $ %s <option> <flt_file> \nOptions:\n", program );    
    printf( "\t -t   : Inspect texture headers.\n" );    
    printf( "\t Supported image formats: rgb, rgba, sgi, jpg, jpeg, png, tga, bmp, dds\n" );
    printf( "\nExamples:\n" );
    printf( "\tDump all information into a file:\n" );
    printf( "\t  $ %s master.flt -t > out.xml\n\n", program);    
    flt_free(program);
  }
  
  return 0;
}