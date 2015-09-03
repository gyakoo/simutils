
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
#include <functional>
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
void fltXmlOfPrint(flt* of, int d, std::set<uint64_t>& done, int* tot_nodes, fltThreadPool* tp, bool allInfo);
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
  case TASK_FLT    : 
    flt_load_from_filename(filename.c_str(), of, opts); 
    break;
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

void fltCountNode(flt_node* n, fltu32* tris)
{
  if ( !n ) return;
  
  while (n)
  {
    if ( n->ndx_pairs_count )
    {
      fltu32 start,end;
      for (fltu32 i=0;i<n->ndx_pairs_count;++i)
      {
        FLTGET32(n->ndx_pairs[i],start,end);
        *tris += (end-start+1)/3;
      }
    }
    else
    {
      fltCountNode(n->child_head,tris);
    }
    n = n->next;
  }
  
}

void fltCountOf(flt* of, fltu32* tris)
{
  if ( !of || !of->hie ) return;

  fltCountNode(of->hie->node_root,tris);  
}

void fltXmlIndent(int d){ for ( int i = 0; i < d; ++i ) printf( "  " ); }
void fltXmlNodePrint(flt_node* n, fltThreadPool* tp, int d, bool allInfo)
{
  static const char* names[FLT_NODE_MAX]={"none", "root", "xref", "group", "object", "mesh", "lod", "face", "vlist", "switch"};
  static char tmp[512];
  if ( !n ) return;

  // my siblings and children
  do
  {
    *tmp = 0;
    if ( n->name && *n->name ) 
      sprintf_s(tmp," name=\"%s\"",n->name);
    
    bool hasAttrChildren=false;
    switch(n->type)
    {
    case FLT_NODE_LOD:
      {
      flt_node_lod* lod = (flt_node_lod*)n;
      sprintf_s(tmp, "%s s_in_out=\"%g %g\" center=\"%g %g %g\" trange=\"%g\" ssize=\"%g\" flags=\"0x%08x\"", tmp, lod->switch_in, lod->switch_out,
        lod->cnt_coords[0],lod->cnt_coords[1],lod->cnt_coords[2], lod->trans_range, lod->sig_size, lod->flags );
      }break;
    case FLT_NODE_SWITCH:
      {
        flt_node_switch* swi=(flt_node_switch*)n;
        sprintf_s(tmp, "%s curmask=\"%d\" maskcount=\"%d\" wpm=\"%d\"", tmp, swi->cur_mask, swi->mask_count, swi->wpm);
        hasAttrChildren=true;
      }break;
    }
    
    if ( n->ndx_pairs_count ) 
    {
      fltu32 tris=0;
      fltCountNode(n,&tris);
      if ( tris ) sprintf_s( tmp, "%s tris=\"%d\"", tmp, tris);
      hasAttrChildren = true;      
    }

    // if no specific attributes or not children, just print tag and close
    if ( !hasAttrChildren && !n->child_count )
    {
      fltXmlIndent(d); printf( "<%s%s/>\n", names[n->type], tmp);
    }
    else
    {
      fltXmlIndent(d); printf( "<%s%s>\n", names[n->type], tmp);
      // specific attributes
      if ( n->ndx_pairs_count )
      {
        fltu32 start,end;     
        for (fltu32 i=0;i<n->ndx_pairs_count;++i)
        {
          FLTGET32(n->ndx_pairs[i],start,end);
          fltXmlIndent(d+1); printf( "<batch ndx_start=\"%d\" ndx_end=\"%d\" />\n", start,end );
        }
      }

      if ( n->type == FLT_NODE_SWITCH )
      {
        flt_node_switch* swi=(flt_node_switch*)n;
        if ( swi->mask_count && swi->maskwords )
        {
          fltXmlIndent(d+1); printf( "<wordmasks>" );           
          int end = swi->mask_count*swi->wpm;
          for ( int i = 0; i < end; ++i ) printf( "0x%08x ", swi->maskwords[i]);
          printf( "</wordmasks>\n" );
        }
      }
      
      // children nodes
      if ( n->child_count)
        fltXmlNodePrint(n->child_head, tp, d+1, allInfo);

      // closing node tag
      fltXmlIndent(d); printf( "</%s>\n", names[n->type]);
    }
    n = n->next;
  }while(n);
}

void fltXmlPrintFace(int d, fltu32 facehash, flt_face* face)
{
  fltXmlIndent(d); printf( "<face hash=\"0x%08x\">\n", facehash );
  {
    fltXmlIndent(d+1); printf( "<tex_base>%d</tex_base>\n", face->texbase_pat);
    fltXmlIndent(d+1); printf( "<tex_detail>%d</tex_detail>\n", face->texdetail_pat);
    fltXmlIndent(d+1); printf( "<abgr>0x%08x</abgr>\n", face->abgr);
    fltXmlIndent(d+1); printf( "<shader>%d</shader>\n", face->shader_ndx);
    fltXmlIndent(d+1); printf( "<material>%d</material>\n", face->mat_pat);
    fltXmlIndent(d+1); printf( "<billboard>%d</billboard>\n", face->billb);
    fltXmlIndent(d+1); printf( "<flags>0x%08x</flags>\n", face->flags);
#ifndef FLT_LEAN_FACES
    fltXmlIndent(d+1); printf( "<ir_color>0x%08x</ir_color>\n", face->ir_color);
    fltXmlIndent(d+1); printf( "<ir_mat>%d</ir_mat>\n", face->ir_mat);  
    fltXmlIndent(d+1); printf( "<tex_mapp>%d</tex_mapp>\n", face->texmapp_ndx);  
    fltXmlIndent(d+1); printf( "<smc>%d</smc>\n", face->smc_id);
    fltXmlIndent(d+1); printf( "<feat>%d</feat>\n", face->feat_id);
    fltXmlIndent(d+1); printf( "<transp>%d</transp>\n", face->transp);
    fltXmlIndent(d+1); printf( "<cname>%d</cname>\n", face->cname_ndx);
    fltXmlIndent(d+1); printf( "<cname_alt>%d</cname_alt>\n", face->cnamealt_ndx);
    fltXmlIndent(d+1); printf( "<draw>%s</draw>\n", flt_get_face_drawtype(face->draw_type));
    fltXmlIndent(d+1); printf( "<light>%s</light>\n", flt_get_face_lightmode(face->light_mode));
    fltXmlIndent(d+1); printf( "<lodgen>%d</lodgen>\n", face->lod_gen);
    fltXmlIndent(d+1); printf( "<linestyle>%d</linestyle>\n", face->linestyle_ndx);
#endif
  }
  
  fltXmlIndent(d); printf( "</face>\n");
}

const char* fltGetSizeStr(unsigned int size)
{
  static char tmp[128];
  *tmp=0;
  if ( size < 1024 )
    sprintf_s(tmp, "%d b", size);
  else if ( size < (1<<20) )
    sprintf_s(tmp, "%.2f Kb", size/1024.0f);
  else if ( size < (1<<30) )
    sprintf_s(tmp, "%.2f Mb", size/(1024.0f*1024.0f));
  else 
    sprintf_s(tmp, "%.2f Gb", size/(1024.0f*1024.0f*1024.0f));
  return tmp;
}

void fltXmlOfPrint(flt* of, int d, std::set<uint64_t>& done, int* tot_nodes, fltThreadPool* tp, bool allInfo)
{
  if ( !of || of->loaded != FLT_LOADED ) return;
  
  fltXmlIndent(d); printf( "<file name=\"%s\">\n", of->filename);

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
      fltu32 tris=0;
      fltCountOf(of,&tris);
      if (of->pal->vtx_count) { fltXmlIndent(d + 2); printf("<vertices>%d</vertices>\n", of->pal->vtx_count); }
      if ( tris ){ fltXmlIndent(d+2); printf( "<triangles>%d</triangles>\n", tris); }
      if ( of->pal->tex_count ){ fltXmlIndent(d+2); printf( "<textures>%d</textures>\n", of->pal->tex_count); }      
    }
  }
  fltXmlIndent(d+1); printf( "</counters>\n");

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

  // hierarchy and recursive print
  if ( of->hie )
  {
    // hierarchy
    fltXmlNodePrint(of->hie->node_root, tp, d+1,allInfo);

    // flat list of extrefs
    if ( of->hie->extref_count)
    {
      fltXmlIndent(d+1); printf( "<xrefs>\n" );
      flt_node_extref* extref = of->hie->extref_head;
      while ( extref )
      {
        if ( done.find( (uint64_t)extref->of ) == done.end() )
        {
          fltXmlOfPrint(extref->of, d+2, done,tot_nodes,tp, allInfo);
          done.insert( (uint64_t)extref->of );
        }
        extref= (flt_node_extref*)extref->next_extref;
      }
      fltXmlIndent(d+1); printf( "</xrefs>\n" );
    }
  }

  // palettes
  if ( of->pal )
  {
    // textures
    if(of->pal->tex_count)
    { 
      flt_pal_tex* tex = of->pal->tex_head;
      fltu32 totalsize=0;
      fltImgMap::iterator it;
      while (tex)
      {
        it = tp->imgattrs.find(tex->name);
        if ( it != tp->imgattrs.end() ) totalsize += it->second.size; 
        tex = tex->next;
      }

      tex = of->pal->tex_head;
      fltXmlIndent(d+1); printf( "<textures totalsize=\"%s\">\n", fltGetSizeStr(totalsize));
      while (tex)
      {
        it = tp->imgattrs.find(tex->name);
        if ( it != tp->imgattrs.end() )
        {
          fltXmlIndent(d+2); 
          if ( it->second.depth>=0 )
            printf( "<tex w=\"%d\" h=\"%d\" d=\"%d\" size=\"%s\">%s</tex>\n" , it->second.width, it->second.height, it->second.depth, fltGetSizeStr(it->second.size), tex->name);
          else
            printf( "<tex w=\"%d\" h=\"%d\" f=\"DXT%d\" size=\"%s\">%s</tex>\n" , it->second.width, it->second.height, -it->second.depth, fltGetSizeStr(it->second.size), tex->name);
        }
        else
        {
          fltXmlIndent(d+2); printf( "<tex>%s</tex>\n", tex->name );
        }
        tex = tex->next;
      } 
      fltXmlIndent(d+1); printf( "</textures>\n");
    }

    // verbose info
    if ( allInfo )
    {
      // vertices
      if (of->pal && of->pal->vtx_array)
      {
        const bool hasPosition = (of->ctx->opts->pflags & FLT_OPT_PAL_VTX_POSITION) != 0;
        const bool singlePos = (of->ctx->opts->pflags & FLT_OPT_PAL_VTX_POSITION_SINGLE) != 0;
        const bool hasNormal = (of->ctx->opts->pflags & FLT_OPT_PAL_VTX_NORMAL) != 0;
        const bool hasUv = (of->ctx->opts->pflags & FLT_OPT_PAL_VTX_UV) != 0;
        const bool hasColor = (of->ctx->opts->pflags & FLT_OPT_PAL_VTX_COLOR) != 0;
        int semn=0;
        char sem[5] = { 0 };
        if ( hasPosition ) sem[semn++]=singlePos?'p':'P';
        if ( hasNormal ) sem[semn++]='N';
        if ( hasUv ) sem[semn++]='T';
        if ( hasColor ) sem[semn++]='C';
        fltXmlIndent(d + 1); printf("<vertices type=\"array\" count=\"%d\" semantic=\"%s\">\n", of->pal->vtx_count, sem);

        fltu32 offs;
        double* pd;
        float* pf;
        fltu32* pc;
        char tmp[512];
        fltu8* vtx=of->pal->vtx_array;
        fltu32 vsize = flt_compute_vertex_size(of->ctx->opts->pflags);
        for (fltu32 i = 0; i < of->pal->vtx_count; ++i, vtx += vsize)
        {
          offs = 0;
          if (hasPosition)
          {
            if (singlePos) { pf = (float*)(vtx); sprintf_s(tmp, "P=\"%g %g %g\"", pf[0], pf[1], pf[2]); offs += sizeof(float) * 3; }
            else { pd = (double*)(vtx); sprintf_s(tmp, "P=\"%g %g %g\"", pd[0], pd[1], pd[2]); offs += sizeof(double) * 3; }
          }
          if (hasNormal)
          {
            pf = (float*)(vtx + offs);
            sprintf_s(tmp, "%s N=\"%g %g %g\"", tmp, pf[0], pf[1], pf[2]);
            offs += sizeof(float) * 3;
          }
          if (hasUv)
          {
            pf = (float*)(vtx + offs);
            sprintf_s(tmp, "%s T=\"%g %g\"", tmp, pf[0], pf[1]);
            offs += sizeof(float) * 2;
          }
          if ( hasColor)
          { 
            pc = (fltu32*)(vtx + offs);
            sprintf_s(tmp, "%s C=\"0x%08x\"", tmp, *pc);
          }
          fltXmlIndent(d + 2); printf("<v %s />\n", tmp);
        }
        fltXmlIndent(d + 1); printf("</vertices>\n");
      }

      // indices
      if ( of->indices && of->indices->size )
      {
        fltXmlIndent(d+1); printf( "<indices count=\"%d\">\n", of->indices->size);
        fltXmlIndent(d+2); printf( "<values>\n");
        fltu32 hashe,indexvalue;
        for ( fltu32 i=0;i<of->indices->size;++i)
        {
          FLTGET32(of->indices->data[i],hashe,indexvalue);
          printf( "%d ",indexvalue);
        }
        printf("\n");
        fltXmlIndent(d+2); printf( "</values>\n");

        fltXmlIndent(d+2); printf( "<facehashes>\n");

        FLTGET32(of->indices->data[0],hashe,indexvalue);
        fltu32 lasthashe=hashe;
        fltu32 i=1,lasti=0;
        do
        {
          FLTGET32(of->indices->data[i],hashe,indexvalue);
          if ( hashe != lasthashe )
          {
            flt_dict_gethe(of->ctx->dictfaces,hashe,&indexvalue);
            fltXmlIndent(d+3); printf( "<facehash count=\"%d\" hash=\"0x%08x\" />\n", i-lasti, indexvalue);
            lasthashe=hashe;
            lasti=i;
          }
          ++i;
        }while (i<of->indices->size);

        // last element (or unique)
        flt_dict_gethe(of->ctx->dictfaces,hashe,&indexvalue);
        fltXmlIndent(d+3); printf( "<facehash count=\"%d\" hash=\"0x%08x\" />\n", i-lasti, indexvalue);

        fltXmlIndent(d+2); printf( "</facehashes>\n");
        fltXmlIndent(d+1); printf( "</indices>\n");
      }

      // faces
      if ( of->ctx->dictfaces )
      {
        fltXmlIndent(d+1); printf( "<faces>\n" );
        for ( int i = 0; i < of->ctx->dictfaces->capacity; ++i )
        {
          flt_dict_node* dn = of->ctx->dictfaces->hasht[i];          
          while (dn)
          {
            fltXmlPrintFace(d+2, dn->keyhash, (flt_face*)dn->value);
            dn = dn->next;
          }
        }
        fltXmlIndent(d+1); printf( "</faces>\n" );
      }
    }
  }

  fltXmlIndent(d); printf( "</file>\n");
}

void fltXmlPrint(flt* of, flt_opts* opts, int nfiles, double tim, fltThreadPool* tp, bool allInfo)
{
  std::set<uint64_t> done;
  int counterctx=0;
  
  printf("<root>\n");

  fltXmlIndent(1); printf("<stats>\n");
  fltXmlIndent(2); printf("<loadtime>%.4g</loadtime>\n",tim);
  fltXmlIndent(2); printf("<files>%d</files>\n", nfiles);
  fltXmlIndent(2); printf("<faces>%d</faces>\n", TOTALNFACES);
  fltXmlIndent(2); printf("<unique_faces>%d</unique_faces>\n", TOTALUNIQUEFACES);
  fltXmlIndent(2); printf("<indices>%d</indices>\n", TOTALINDICES);
  if ( opts->countable )
  {
    fltXmlIndent(2); printf("<opcodes>\n");
    for ( int i = 0; i < FLT_OP_MAX; ++i )
    {
      if ( opts->countable[i] )
      {
        fltXmlIndent(3); printf( "<op_%03d name=\"%s\" count=\"%d\" />\n", i, flt_get_op_name((fltu16)i), opts->countable[i], i);
      }
    }
    fltXmlIndent(2); printf("</opcodes>\n");
  }
  fltXmlIndent(1); printf("</stats>\n");

  fltXmlOfPrint(of,1,done,&counterctx, tp, allInfo);

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


void read_with_callbacks_mt(const std::vector<std::string>& files, bool inspectTex, bool allInfo, bool recurse, fltu32 vtxmask)
{
  fltThreadPool tp;
  tp.numThreads = std::thread::hardware_concurrency()*2;
  tp.init();
  tp.inspectTexture=inspectTex;
    
  // configuring read options
  flt_opts* opts=(flt_opts*)flt_calloc(1,sizeof(flt_opts));
  flt* of=(flt*)flt_calloc(1,sizeof(flt));
  opts->pflags = FLT_OPT_PAL_TEXTURE | FLT_OPT_PAL_VERTEX | vtxmask;
  opts->hflags = FLT_OPT_HIE_ALL_NODES | FLT_OPT_HIE_HEADER;
  opts->dfaces_size = 3079;//1543;
  opts->cb_texture = fltCallbackTexture;
  opts->cb_extref = recurse ? fltCallbackExtRef : FLT_NULL;
  opts->cb_user_data = &tp;
  opts->countable = (fltatom32*)flt_calloc(1, sizeof(fltatom32)*FLT_OP_MAX);

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

  // dumping info to xml
  fltXmlPrint(of, opts, tp.nfiles, (fltGetTime()-t0)/1000.0, &tp, allInfo);

  // releasing memory
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
  bool inspectTex=false;  
  bool allInfo=false;
  bool recurse = false;
  std::vector<std::string> files;
  fltu32 vtxmask=0;
  for ( int i = 1; i < argc; ++i )
  {
    if ( argv[i][0]=='-' )
    {
      switch ( argv[i][1] )
      {
      case 't': inspectTex=true; break;
      case 'a': allInfo=true; break;
      case 'r': recurse = true; break;
      case 'v': 
      {
        if ( i+1 < argc )
        {
          int j=0; char c;
          while ( (c=argv[i+1][j++]) )
          {
            switch (tolower(c))
            {
            case 'p': vtxmask |= FLT_OPT_PAL_VTX_POSITION; break;
            case 'n': vtxmask |= FLT_OPT_PAL_VTX_NORMAL; break;
            case 't': vtxmask |= FLT_OPT_PAL_VTX_UV; break;
            case 'c': vtxmask |= FLT_OPT_PAL_VTX_COLOR; break;
            case 's': vtxmask |= FLT_OPT_PAL_VTX_POSITION_SINGLE; break;
            }
          }
          ++i;
        }
      }break;
      default : printf ( "Unknown option -%c\n", argv[i][1]); 
      }
    }
    else
      files.push_back( argv[i] );
  }

  if ( !files.empty() )
  {
    if ( !vtxmask ) { vtxmask = FLT_OPT_PAL_VTX_POSITION; }
    read_with_callbacks_mt(files, inspectTex, allInfo, recurse, vtxmask);
  }
  else
  {
    char* program=flt_path_basefile(argv[0]);
    printf("flt2xml: Dumps (Unique Faces version) Openflight metadata in xml format\n\n" );    
    printf("Usage: $ %s <options> <flt_file> \nOptions:\n", program );    
    printf("\t -t   : Inspect texture headers.\n" );    
    printf("\t -a   : Dump all information (vertices/faces)\n");
    printf("\t -r   : Recursive. Resolve all external references recursively\n");
    printf("\t -v pntcs: Vertex mask. p=position n=normal t=uv c=color s=single precision\n\n" );
    printf("Supported image formats: rgb, rgba, sgi, jpg, jpeg, png, tga, bmp, dds\n" );
    printf("\nExamples:\n" );
    printf("\tDump all information recursively into a xml file:\n" );
    printf("\t  $ %s master.flt -t -a -r > alldb.xml\n\n", program);    
    printf("\tOnly writes structure info into xml:\n" );
    printf("\t  $ %s model.flt > model.xml\n\n", program );
    printf("\tDump only single precision Position and Normals of model.flt:\n" );
    printf("\t  $ %s -a -v pn tank.flt > tank.xml\n\n", program);    
    flt_free(program);
  }
  
  return 0;
}