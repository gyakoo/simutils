

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
#pragma warning(disable:4100 4005 4996)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
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
#include <algorithm>


enum
{
  FORMAT_PNG=0,
  FORMAT_DEM=1
};

struct fltThreadPool;
struct fltThreadTask;
double fltGetTime();

template<typename T> T fltClamp(T v, T _m, T _M){ return v<_m?_m:(v>_M?_M:v);}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct fltExtent
{
  double extent_max[3];
  double extent_min[3];

  fltExtent(){invalidate();}
  void setZero()
  { 
    for ( int i = 0; i < 3; ++i ) 
      extent_max[i] = extent_min[i] = 0.0; 
  }

  bool isValid()
  {
    double dim[3];
    for ( int i = 0; i < 3; ++i )
      if ( extent_max[i] < extent_min[i] )
      {
        dim[i]=extent_max[i]-extent_min[i];
        return false;
      }
    return dim[0]!=0.0 || dim[1]!=0.0 || dim[2]!=0;
  }

  void getDimension(double* dim)
  {
    for ( int i = 0; i < 3; ++i )
      dim[i]=extent_max[i]-extent_min[i];
  }

  void getCenter(double* xyz) 
  { 
    for ( int i = 0; i < 3; ++i ) 
      xyz[i] = extent_min[i] + (extent_max[i]-extent_min[i])*0.5f; 
  }

  void invalidate()
  {
    for ( int i = 0; i < 3; ++i )
    {
      extent_max[i] = -DBL_MAX;
      extent_min[i] = DBL_MAX;
    }
  }
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct fltNodeFillParam
{
  flt* of;
  uint8_t* lumdata;
  fltExtent* xtent;
  double* dim;
  int width, height;

  double *v1, *from1;
  double *v2, *from2;
  double step;
  const double* mapping;
};
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct fltThreadTask
{
  enum f2dTaskType { TASK_NONE=-1, TASK_FLT2ELEV=0};
  f2dTaskType type;
  std::string filename;

  fltThreadTask():type(TASK_NONE){}
  fltThreadTask(f2dTaskType tt, const std::string& f)
    :type(tt), filename(f){}
  
  void runTask(fltThreadPool* tp)
  {
    tp=tp;
    switch( type )
    {
    case TASK_FLT2ELEV: flt2elev(tp); break;
    }
  }

  void computeExtent(flt* of, fltExtent* xtent, flt_node* node);
  void computeNodeExtent(flt* of, fltExtent* xtent, flt_node* node);
  void flt2elev(fltThreadPool* tp);
  void flt2png(fltThreadPool* tp, flt* of);
  void flt2dem(fltThreadPool* tp, flt* of){ tp=0; of=0; throw "Not implemented"; }
  bool findSpecificNode(flt_node* node, const std::string& lodname, int nodetype, flt_node** outNode);
  void fillNodeTris(flt_node* node, fltNodeFillParam* p);
  void fillTri(double* v0, double* v1, double* v2, fltNodeFillParam* p);
  void fillTriStep(double t, fltNodeFillParam* p);
  void fillLineStep(double t, double* a, double* d, double* xyz);
  void mapPoint(double* xyz, fltNodeFillParam* p);
  double computeStep(double* v0, double *v1, double* v2, fltNodeFillParam* p);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct fltThreadPool
{
  void init(int numthreads)
  {
    deinit();
    finish=false;
    workingTasks=0;
    threads.reserve(numthreads);
    for(int i = 0; i < numthreads; ++i)
      threads.push_back(new std::thread(&fltThreadPool::runcode,this));  
    res[0]=res[1]=-1;
  }

  void runcode()
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

  bool getNextTask(fltThreadTask* t)
  {
    std::lock_guard<std::mutex> lock(mtxTasks);
    if ( tasks.empty() ) return false;
    *t = tasks.back();
    tasks.pop_back();
    return true;
  }

  void addNewTask(const fltThreadTask& task)
  {
    // not done yet, process
    std::lock_guard<std::mutex> lock(mtxTasks);
    tasks.push_front( task );
  }

  void deinit()
  {
    finish = true;
    for (std::thread* t : threads)
    {
      t->join();
      delete t;
    }
    threads.clear();
    tasks.clear();
  }

  bool isWorking(){ return !tasks.empty() || workingTasks>0; }

  std::vector<std::thread*> threads;
  std::deque<fltThreadTask> tasks;  
  std::mutex mtxTasks;
  std::atomic_int workingTasks;
  std::string nodename;
  int res[2];
  volatile int format;
  bool finish;
  int nodetype;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltThreadTask::computeNodeExtent(flt* of, fltExtent* xtent, flt_node* node)
{
  if ( !node ) return;
  if ( node->ndx_pairs_count )
  {
    // for all batches
    fltu32 start,end;
    double *v;
    fltu32 hashe,index;
    double* vtxarray=(double*)of->pal->vtx_array; // 3 consecutive xyz doubles (see opts when loading flt)
    for (fltu32 i=0;i<node->ndx_pairs_count;++i)
    {
      // get the batch start/end vertices
      FLTGET32(node->ndx_pairs[i],start,end);
      // for all this batch indices, compute extent
      for (fltu32 j=start;j<end; ++j)
      {
        FLTGET32(of->indices->data[j],hashe,index);
        v = vtxarray + (index*3);
        for (int k=0;k<3;++k)
        {
          if ( v[k]<xtent->extent_min[k] ) xtent->extent_min[k] = v[k];
          if ( v[k]>xtent->extent_max[k] ) xtent->extent_max[k] = v[k];
        }
      }
    }
  }

  flt_node* child = node->child_head;
  while (child)
  {
    computeNodeExtent(of,xtent,child);
    child = child->next;
  }
}

void fltThreadTask::computeExtent(flt* of, fltExtent* xtent, flt_node* node)
{
  xtent->invalidate();
  // compute extent
  if ( of->pal && of->pal->vtx_array && of->pal->vtx_count)
  {
    if ( !node )
    {
      // we assume 3 doubles consecutive (see opts)
      double* xyz=(double*)of->pal->vtx_array;
      xtent->invalidate();
      int j;
      for ( size_t i = 0; i < of->pal->vtx_count; ++i, xyz+=3 )
      {
        for (j=0;j<3;++j)
        {
          if ( xyz[j]<xtent->extent_min[j] ) xtent->extent_min[j] = xyz[j];
          if ( xyz[j]>xtent->extent_max[j] ) xtent->extent_max[j] = xyz[j];
        }
      }
    }
    else 
    {
      computeNodeExtent(of, xtent, node);
    }
  }
}

void fltThreadTask::flt2elev(fltThreadPool* tp)
{
  // compute extent
  flt_opts opts = {0};
  opts.pflags = FLT_OPT_PAL_VERTEX | FLT_OPT_PAL_VTX_POSITION;
  opts.hflags = FLT_OPT_HIE_ALL_NODES;
  opts.dfaces_size = 1543;
  flt of = {0};

  // load file
  flt_load_from_filename(filename.c_str(), &of, &opts);

  switch ( tp->format )
  {
  case FORMAT_PNG: flt2png(tp,&of); break;
  case FORMAT_DEM: flt2dem(tp,&of); break;
  }

  flt_release(&of);
}

bool fltThreadTask::findSpecificNode(flt_node* node, const std::string& lodname, int nodetype, flt_node** outNode)
{
  if ( !node ) return false;
  if ( (nodetype==-1 || nodetype == flt_get_op_from_node_type(node->type)) && lodname == node->name )
  {
    *outNode = node;
    return true;
  }

  flt_node* child=node->child_head;
  while ( child )
  {
    if ( findSpecificNode(child,lodname,nodetype,outNode) )
      return true;
    child = child->next;
  }
  return false;
}

double fltThreadTask::computeStep(double* v0, double *v1, double* v2, fltNodeFillParam* p)
{
  return 0.005;
}

void fltThreadTask::fillLineStep(double t, double* a, double* d, double* xyz)
{
  xyz[0]=a[0]+t*d[0];
  xyz[1]=a[1]+t*d[1];
  xyz[2]=a[2]+t*d[2];
}

void fltThreadTask::mapPoint(double* xyz, fltNodeFillParam* p)
{
  int x, y;
  uint8_t z, oldz;
  int offs;

  x = (int)( (xyz[0]-p->xtent->extent_min[0]) * p->mapping[0] ); x = fltClamp(x,0,p->width-1);
  y = (int)( (xyz[1]-p->xtent->extent_min[1]) * p->mapping[1] ); y = fltClamp(y,0,p->height-1);
  z = (uint8_t)(int)( (xyz[2]-p->xtent->extent_min[2]) * p->mapping[2]);
  offs = y*p->width+x;
  oldz = p->lumdata[offs];
  if ( z > oldz ) p->lumdata[offs] = z;
}

void fltThreadTask::fillTriStep(double t, fltNodeFillParam* p)
{
  int i;
  double s=0.0;
  double a[3], b[3];
  double xyz[3];

  // start/end of path on both edges of triangle at step t
  for ( i = 0; i < 3; ++i )
  {
    a[i] = p->v1[i] + t * p->from1[i];
    b[i] = p->v2[i] + t * p->from2[i];
  }

  // direction from edge to edge
  double d[3]={b[0]-a[0], b[1]-a[1], b[2]-a[2]};

  // marching the ray from edge to edge
  while ( s <= 1.0 )
  {
    fillLineStep(s,a,d,xyz); // gets the point

    // every point xyz to the heightmap
    mapPoint(xyz,p);

    // advance in ray
    s += p->step;
  }
}

void fltThreadTask::fillTri(double* v0, double* v1, double* v2, fltNodeFillParam* p)
{
  int i;

  // directions to v0 from v1 and v2
  double from1[3], from2[3];
  for ( i = 0; i < 3; ++i )
  {
    from1[i]=v0[i]-v1[i];
    from2[i]=v0[i]-v2[i];
  }

  // preparing parameter
  p->v1 = v1; p->from1 = from1;
  p->v2 = v2; p->from2 = from2;
  p->step = computeStep(v0,v1,v2,p);
  
  // test- deleteme
//   mapPoint(v0,p);
//   mapPoint(v1,p);
//   mapPoint(v2,p);
//   return;

  // goes from 0 to 1 to go filling the triangle form edge to edge
  double t = 0.0;
  while ( t <= 1.0 )
  { 
    fillTriStep(t, p);
    t += p->step;
  }
  fillTriStep(1.0, p);
}

void fltThreadTask::fillNodeTris(flt_node* node, fltNodeFillParam* p)
{
  if ( !node ) return;
  flt* of = p->of;

  // if we got triangles here in this node
  if ( node->ndx_pairs_count )
  {
    // for all batches
    fltu32 start,end;
    double *v0, *v1, *v2;
    double* vtxarray=(double*)of->pal->vtx_array; // 3 consecutive xyz doubles (see opts when loading flt)
    fltu32 hashe, index;
    for (fltu32 i=0;i<node->ndx_pairs_count;++i)
    {
      // get the batch start/end vertices
      FLTGET32(node->ndx_pairs[i],start,end);

      // every tree vertices is a triangle, indices to vertices in of->pal->vtx_array
      for (fltu32 j=start;j<end; j+=3)
      {
        FLTGET32(of->indices->data[j],hashe,index);
        v0 = vtxarray + (index*3);

        if ( (j+1)>=end ) break;
        FLTGET32(of->indices->data[j+1],hashe,index);
        v1 = vtxarray + (index*3);

        if ( (j+2)>=end ) break;
        FLTGET32(of->indices->data[j+2],hashe,index);
        v2 = vtxarray + (index*3);

        // fill this triangle
        fillTri(v0,v1,v2,p);
      }
    }
  }

  flt_node* child = node->child_head;
  while ( child )
  {
    fillNodeTris(child,p);
    child = child->next;
  }
}

void fltThreadTask::flt2png(fltThreadPool* tp, flt* of)
{
  if ( !of->hie )
  {
    fprintf( stderr, "No nodes loaded in flt: %s\n", filename.c_str() );
    return;
  }

  // look for specific node if user passed non-empty parameter
  flt_node* specificNode = of->hie->node_root;
  if ( !tp->nodename.empty() )
    findSpecificNode(of->hie->node_root, tp->nodename, tp->nodetype, &specificNode);

  fltExtent xtent;
  computeExtent(of,&xtent,specificNode);

  // grayscale, one byte per pixel
  double dim[3];
  xtent.getDimension(dim);
  const int WIDTH = tp->res[0]<=0 ? (int)dim[0] : tp->res[0];
  const int HEIGHT = tp->res[1]<=0 ? (int)dim[1] : tp->res[1];
  const double MAPPING[3] = { 1.0/dim[0]*WIDTH, 1.0/dim[1]*HEIGHT, 1.0/dim[2] * 255.0 };
  const int numpixels = WIDTH*HEIGHT;
  if ( numpixels<=0 )
  {
    fprintf(stderr, "Num pixels invalid for: %s. dim=%g %g %g\n", filename.c_str(), dim[0], dim[1], dim[2]);
    return;
  }

  uint8_t* lumdata = (uint8_t*)calloc( 1, numpixels );
  if ( !lumdata ){ fprintf( stderr, "out of mem: %s\n", filename.c_str() ); return; }
    
  if ( !specificNode )
  {
    fprintf( stderr, "Invalid node to look geometry. %s\n", filename.c_str() );
    return;
  }

  // go through all triangles under node
  fltNodeFillParam p;
  p.of=of;
  p.lumdata=lumdata;
  p.xtent=&xtent;
  p.dim=dim;
  p.width = WIDTH;
  p.height = HEIGHT;
  p.mapping = MAPPING;
  fillNodeTris(specificNode,&p);

  std::string pngname=filename+ ".png";
  stbi_write_png(pngname.c_str(), WIDTH, HEIGHT, 1, lumdata, WIDTH );
  free(lumdata);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void doProcess(const std::vector<std::string>& files, int format, const std::string& nodename, int nodetype, int w, int h)
{
  fltThreadPool tp;
  tp.init(std::thread::hardware_concurrency()*2);
  tp.format = format;
  tp.nodename = nodename;
  tp.nodetype = nodetype;
  tp.res[0]=w;
  tp.res[1]=h;

  // one task per file
  for ( const std::string& f : files )
    tp.addNewTask(fltThreadTask(fltThreadTask::TASK_FLT2ELEV, f) );

  // wait until cores finished
  double t0=fltGetTime();
  const char* cha="|\\-/";
  int c=0;
  do
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    fprintf(stderr, "\r                                 \r");
    fprintf(stderr, "Processing: %c", cha[c%4]);
    ++c;
  } while ( tp.isWorking() );
  printf( "\n\nTime : %g secs\n\n", (fltGetTime()-t0)/1000.0);
  tp.deinit();
}

void fltGetFilesFromWildcard(const std::string& wildcard, std::vector<std::string>& outfiles)
{
#if defined(_WIN32) || defined(_WIN64)
  char filterTxt[MAX_PATH];  
  sprintf_s( filterTxt, ".\\%s", wildcard.c_str() );
  char* basepath = flt_path_base(filterTxt);
  // files for this directory
  WIN32_FIND_DATAA ffdata;
  HANDLE hFind = FindFirstFileA( filterTxt, &ffdata );
  if ( hFind != INVALID_HANDLE_VALUE )
  {
    do
    {
      if ( ffdata.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE )
      {
        outfiles.push_back( std::string(basepath)+ffdata.cFileName );
      }
    }while ( FindNextFile(hFind,&ffdata)!=0 );
    FindClose(hFind);
  }
  flt_free(basepath);
#else
#error "Implement this function for other platforms"
#endif
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int main(int argc, const char** argv)
{
  int format=FORMAT_PNG;
  std::string nodename;
  int nodetype=-1;
  std::vector<std::string> files;
  std::string wildcard;
  int w=-1, h=-1;
  for ( int i = 1; i < argc; ++i )
  {
    if ( argv[i][0]=='-' )
    {
      switch ( argv[i][1] )
      {
        case 'f': if (i+1<argc) format=atoi(argv[++i]); break;
        case 'o': if (i+1<argc) nodetype=atoi(argv[++i]); break;
        case 'n': if (i+1<argc) nodename=argv[++i]; break;
        case 'w': if (i+1<argc) wildcard=argv[++i]; break;
        case 'd': 
          if (i+1<argc) w=atoi(argv[++i]); 
          if (i+1<argc) h=atoi(argv[++i]); 
          break;
        default : printf ( "Unknown option -%c\n", argv[i][1]); 
      }
    }
    else
      files.push_back( argv[i] );
  }

  if ( format < FORMAT_PNG || format > FORMAT_DEM )
    format=FORMAT_PNG;

  if ( !wildcard.empty() )
    fltGetFilesFromWildcard(wildcard, files);

  if ( !files.empty() )
  {
    doProcess(files, format, nodename, nodetype, w, h);
  }
  else
  {
    char* program=flt_path_basefile(argv[0]);
    printf("%s: Creates elevation file out of a FLT.\n\nBy default it converts all geometry in the FLT unless a LOD node name is specified\n\n", program );    
    printf("Usage: $ %s <options> <flt_files> \nOptions:\n", program );    
    printf("\t -f N         : Format. 0:PNG grayscale heightmap 1:DEM. Default=0\n");
    printf("\t -o N         : Optional Node Opcode where to get the geometry from. Default=none=Any type\n");
    printf("\t -n <name>    : Optional Node Name where to get the geometry from. Default=none=All nodes\n");
    printf("\t -d <w> <h>   : Optional. Force this resolution for the final image, w=width, h=height.\n" );
    printf("\t -w <wildcard>: Wildcard for files, i.e. flight*.flt\n" );
    printf("\nExamples:\n" );
    printf("\tConvert all geometry of the flt files, force 512x512 image\n" );
    printf("\t $ %s -d 512 512 -w flight*.flt\n\n", program);
    printf("\tConvert tile into a grayscale image, only geometry under LOD name \"l1\" is converted:\n" );
    printf("\t $ %s -f 0 -n l1 -o 73 flight1_1.flt\n\n", program);    
    
    flt_free(program);
  }
  
  return 0;
}