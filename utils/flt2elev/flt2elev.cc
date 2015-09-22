

#define NOMINMAX
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


//////////////////////////////////////////////////////////////////////////
// Misc types, functions, globals and forward declarations
//////////////////////////////////////////////////////////////////////////
enum
{
  FORMAT_PNG=0,
  FORMAT_DEM=1,
  FORMAT_RAW=2,
  FORMAT_MAX=3,

  CULLING_NONE=0,
  CULLING_CW=1,
  CULLING_CCW=2
};

enum eMosaicTileState
{
  MOSAICTILESTATE_UNKNOWN=-1,
  MOSAICTILESTATE_LOADFLT=0,
  MOSAICTILESTATE_FILL,
};

// forwards
struct fltThreadPool;
double fltGetTime();
const char* fltFormatExt(int fmt);
void fltGetFilesFromWildcard(const std::string& wildcard, std::vector<std::string>& outfiles);
template<typename T> T fltClamp(T v, T _m, T _M){ return v<_m?_m:(v>_M?_M:v);}

// globals
std::atomic_int g_numtris;
std::atomic_int g_numtrisbf;


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct fltExtent
{
  double extent_max[3];
  double extent_min[3];

  fltExtent(){invalidate();}

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

  void getDimension(double* dim) const 
  {
    for ( int i = 0; i < 3; ++i )
      dim[i]=extent_max[i]-extent_min[i];
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
struct fltTileInfo
{
  fltTileInfo()
    : of(nullptr), fltnode(nullptr)
  { }

  void setFromMosaic(const fltTileInfo& full)
  {
    xtent = full.xtent;
    for ( int i = 0; i < 3; ++i )
    {
      dim[i]=full.dim[i];
      mapping[i]= full.mapping[i];
    }
    width = full.width;
    height= full.height;
  }

  std::string fltfile;
  flt* of;
  flt_node* fltnode;

  fltExtent xtent;
  double dim[3];
  double mapping[3];
  int width, height;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct fltImage
{
  fltImage() : data(nullptr), width(0), height(0), depth(8) { }

  uint8_t* data;
  int width;
  int height;
  int depth;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct fltThreadBaseTask
{
  fltThreadBaseTask(const std::string& fname):filename(fname)
  {
  }

  virtual void runTask(fltThreadPool* tp) = 0;
  virtual void release(){}

  std::string filename;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct fltTaskSingleTile : public fltThreadBaseTask
{
  fltTaskSingleTile(const std::string& f): fltThreadBaseTask(f)
  {
  }
  virtual void release(){ delete this; }  
  virtual void runTask(fltThreadPool* tp);
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct fltTaskMosaicTile : public fltThreadBaseTask
{
  fltTaskMosaicTile(const std::string& f): fltThreadBaseTask(f), state(MOSAICTILESTATE_UNKNOWN)
  {
  }

  virtual void runTask(fltThreadPool* tp);

  eMosaicTileState state;
  flt* of;
  fltTileInfo tinfo;
  fltImage img;

private:
  void stateLoadFlt(fltThreadPool* tp);
  void stateFill(fltThreadPool* tp);
};


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct fltElevConfig
{
  fltElevConfig()
    : mosaicMode(false), format(FORMAT_PNG), nodetype(0), culling(CULLING_CCW)
    , depth(8)
  {
    forceDim[0]=forceDim[1]=-1;
  }

  std::string nodename;
  std::string basepath;
  bool mosaicMode;
  int forceDim[2];
  int format;
  int nodetype;
  int culling;
  int depth;
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
  }

  void runcode()
  {
    fltThreadBaseTask* task=nullptr;
    while (!finish)
    {
      task=getNextTask();
      if ( task )
      {
        ++workingTasks;
        try
        {
          task->runTask(this);
        }catch(...)
        {
          OutputDebugStringA("Exception\n");
        }
        --workingTasks;
        task->release();
      }
      else
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }

  fltThreadBaseTask* getNextTask()
  {
    std::lock_guard<std::mutex> lock(mtxTasks);
    if ( tasks.empty() ) return nullptr;
    fltThreadBaseTask* t = tasks.back();
    tasks.pop_back();
    return t;
  }

  void addNewTask(fltThreadBaseTask* task)
  {
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
    for (fltThreadBaseTask* t : tasks)
      delete t;
    tasks.clear();
  }

  bool isWorking(){ return !tasks.empty() || workingTasks>0; }
  void waitForTasks(int ms=250)
  {
    do
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    } while ( isWorking() );
  }

  std::vector<std::thread*> threads;
  std::deque<fltThreadBaseTask*> tasks;
  std::mutex mtxTasks;
  std::atomic_int workingTasks;  
  fltElevConfig config;  
  bool finish;
};

//////////////////////////////////////////////////////////////////////////
// It does not have memory contention in mosaic mode, in other words, 
// it's not thread safe. Two threads might write at the same location 
// of the mosaic image.
// It's because we need to read the height before write a pixel.
// We should rely on a small amount of collision (race conditions) because
// most of the tiles are disjoint so different memory heightfield location.
// But in any case, we might lock the access to a pixel *only* when it's
// in an overlapped region that can be computed beforehand.
// When GPU version if implemented, will use same concept.
//////////////////////////////////////////////////////////////////////////
struct fltCPUFill
{
public:
  struct fillParams
  {
    double v1[3]; double from1[3];
    double v2[3]; double from2[3];
    double step;
  };

  fltCPUFill(const fltElevConfig& config, const fltTileInfo& tinfo, flt* of, fltImage& img)
    : config(config), tinfo(tinfo), of(of), img(img)
  {
  }
  fltCPUFill& operator =(const fltCPUFill& o){}

  bool fill(flt_node* node);
private:
  void fillNodeTris(flt_node* node);
  void fillTri(double* v0, double* v1, double* v2);
  void fillTriStep(double t);
  void fillMapPoint(double* xyz);
  double fillComputeStep(double* v0, double *v1, double* v2);
  void fillLineStep(double t, double* a, double* d, double* xyz);
  void fillMapXY(double _x, double _y, int* x, int *y);
  template<typename T>
  void fillMapPoint_template(double zval, double zmin, double mapping, int offs, T* data);

  const fltElevConfig& config;
  const fltTileInfo& tinfo;
  flt* of;
  fltImage& img;
  fillParams fp;  
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct fltTasks
{
public:
  static bool loadFlt(const std::string& fltfile, const fltElevConfig& config, flt** outOf, flt_node** outNode, fltExtent* outXtent);
  static bool computeTileInfo(const fltElevConfig& config, const fltExtent& xtent, fltTileInfo* outTInfo);
  static bool allocImage(const fltElevConfig& config, const fltTileInfo& tinfo, fltImage* outImg);
  static bool fillImage(const fltElevConfig& config, const fltTileInfo& tinfo, flt* of, fltImage& img);
  static bool saveImage(const fltElevConfig& config, const fltImage& img, const std::string& filename );
  static bool deallocImage(fltImage* img);
  static bool unloadFlt(flt** of);

private:
  static void computeNodeExtent(flt* of, fltExtent* xtent, flt_node* node);
  static void computeExtent(flt* of, fltExtent* xtent, flt_node* node);
  static bool findSpecificNode(flt_node* node, const std::string& lodname, int nodetype, flt_node** outNode);
  template<typename T> static T retErr(T res, const char* format, ...);
};

#pragma region fltTasks
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
template<typename T>
T fltTasks::retErr(T res, const char* format, ...)
{
  va_list args;
  fprintf( stderr, "Error: " );
  va_start(args, format);
  vfprintf( stderr, format, args);
  va_end(args);
  return res;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltTasks::computeNodeExtent(flt* of, fltExtent* xtent, flt_node* node)
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltTasks::computeExtent(flt* of, fltExtent* xtent, flt_node* node)
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool fltTasks::loadFlt(const std::string& fltfile, const fltElevConfig& config, flt** outOf, flt_node** outNode, fltExtent* outXtent)
{
  // load
  flt_opts opts = {0};
  opts.pflags = FLT_OPT_PAL_VERTEX | FLT_OPT_PAL_VTX_POSITION;
  opts.hflags = FLT_OPT_HIE_ALL_NODES;
  opts.dfaces_size = 1543;
  *outOf = (flt*)calloc(1,sizeof(flt));
  if ( !*outOf ) 
    return retErr( false, "Out of memory allocating flt: %s\n", fltfile.c_str() );

  // load file
  if ( flt_load_from_filename(fltfile.c_str(), *outOf, &opts) != FLT_OK )
    return retErr( false, "Error loading %s\n", fltfile.c_str() );

  // contains nodes?
  if ( !(*outOf)->hie )
    return retErr( false, "No nodes loaded in flt: %s\n", fltfile.c_str() );

  // look for specific node if user passed non-empty parameter
  *outNode = (*outOf)->hie->node_root;
  if ( !config.nodename.empty() )
    findSpecificNode( (*outOf)->hie->node_root, config.nodename, config.nodetype, outNode);

  if ( !(*outNode) )
    return retErr( false, "Invalid node to look geometry. %s\n", fltfile.c_str() );

  // compute dimension of image
  computeExtent( (*outOf), outXtent, *outNode);
  if ( !outXtent->isValid() )
    return retErr( false, "Error xtent of flt file: %s\n", fltfile.c_str() );
  
  return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool fltTasks::computeTileInfo(const fltElevConfig& config, const fltExtent& xtent, fltTileInfo* outTInfo)
{
  double dim[3];
  xtent.getDimension(dim);
  const double AR = dim[0]/dim[1];
  const double IAR= dim[1]/dim[0];
  int WIDTH = config.forceDim[0]<0 ? (int)dim[0] : config.forceDim[0];
  int HEIGHT = config.forceDim[1]<0 ? (int)dim[1] : config.forceDim[1];
  if (WIDTH==0 && HEIGHT==0) WIDTH = (int)dim[0];
  if (WIDTH==0) WIDTH=(int)(AR*HEIGHT);
  if (HEIGHT==0) HEIGHT=(int)(IAR*WIDTH);
  const double maxz= (config.depth==32) ? 4294967295.0 : (double)( (1<<config.depth)-1 ); // 1<<32 not work fine on some compilers
  const double MAPPING[3] = { 1.0/dim[0]*WIDTH, 1.0/dim[1]*HEIGHT, (dim[2]>maxz) ? (maxz/dim[2]) : (1.0) };

  // save tile info
  outTInfo->width = WIDTH;
  outTInfo->height = HEIGHT;
  for ( int i = 0; i < 3; ++i )
  {
    outTInfo->dim[i]=dim[i]; 
    outTInfo->mapping[i] = MAPPING[i];
  }
  return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool fltTasks::unloadFlt(flt** of)
{
  if ( !of || !*of ) return false;
  flt_release(*of);
  free(*of);
  *of = nullptr;
  return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool fltTasks::allocImage(const fltElevConfig& config, const fltTileInfo& tinfo, fltImage* outImg)
{
  int numpixels = tinfo.width * tinfo.height;
  outImg->data = (uint8_t*)calloc( 1, numpixels*(config.depth/8) );
  if ( !outImg->data )
    return retErr( false, "out of mem: %s\n", tinfo.fltfile.c_str() );
  outImg->width = tinfo.width;
  outImg->height = tinfo.height;
  outImg->depth = config.depth;
  return true;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool fltTasks::deallocImage(fltImage* img)
{
  if (img->data)
  {
    free(img->data);
    img->width = img->height = img->depth = 0;
    return true;
  }
  return false;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool fltTasks::fillImage(const fltElevConfig& config, const fltTileInfo& tinfo, flt* of, fltImage& img)
{
  return fltCPUFill(config, tinfo, of, img).fill(tinfo.fltnode);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool fltTasks::saveImage(const fltElevConfig& config, const fltImage& img, const std::string& filename )
{
  bool res = false;
  switch ( config.format )
  {
  case FORMAT_PNG: 
    res = stbi_write_png(filename.c_str(), img.width, img.height, 1, img.data, img.width) != 0;
  break;
  case FORMAT_RAW:
    {
      FILE* f = fopen(filename.c_str(), "wb");
      size_t total=img.width * img.height * (img.depth>>3);
      size_t written=0;
      if ( f )
      {
        while (written < total)
          written += fwrite(img.data+written, 1, total-written, f);
        fclose(f);
        res = true;
      }
    }break;
  }
  return res;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool fltTasks::findSpecificNode(flt_node* node, const std::string& lodname, int nodetype, flt_node** outNode)
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

#pragma endregion

#pragma region CPU Fill
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool fltCPUFill::fill(flt_node* node)
{
  if ( !node ) return false;
  fillNodeTris(node);
  return true;
}
//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
double fltCPUFill::fillComputeStep(double* v0, double *v1, double* v2)
{
  double mins=DBL_MAX;
  for ( int i = 0; i < 2; ++i )
  {
    double x = i==0?v1[0]:v2[0];
    double y = i==0?v1[1]:v2[1];    
    int xy0[2]; fillMapXY(v0[0],v0[1], xy0, xy0+1);
    int xy1[2]; fillMapXY(x,y, xy1, xy1+1);
    double pa=(double)(xy1[0]-xy0[0]);
    double pb=(double)(xy1[1]-xy0[1]);    
    double lp = sqrt(pa*pa + pb*pb);
    if ( lp != 0.0 )
    {
      double s = 1.0 / lp;
      if ( s < mins ) mins = s;
    }
  }
  return ( mins < DBL_MAX ) ? mins*0.5f : 0.005;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltCPUFill::fillLineStep(double t, double* a, double* d, double* xyz)
{
  xyz[0]=a[0]+t*d[0];
  xyz[1]=a[1]+t*d[1];
  xyz[2]=a[2]+t*d[2];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltCPUFill::fillMapXY(double _x, double _y, int* x, int *y)
{
  const fltExtent& xtent = tinfo.xtent;
  *x = (int)( (_x - xtent.extent_min[0]) * tinfo.mapping[0] ); 
  *x = fltClamp(*x,0,img.width-1);
  *y = (int)( (_y - xtent.extent_min[1]) * tinfo.mapping[1] ); 
  *y = fltClamp(*y,0,img.height-1);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
template<typename T>
void fltCPUFill::fillMapPoint_template(double zval, double zmin, double mapping, int offs, T* data)
{
  T z, oldz;
  z = (T)(int)( (zval-zmin)*mapping);
  oldz = data[offs];
  if ( z > oldz ) data[offs] = z;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltCPUFill::fillMapPoint(double* xyz)
{
  int x, y;
  int offs;
  fillMapXY(xyz[0], xyz[1], &x, &y);
  offs = y * img.width + x;
  switch ( config.depth )
  {
  case  8:fillMapPoint_template<uint8_t> (xyz[2], tinfo.xtent.extent_min[2], tinfo.mapping[2], offs, (uint8_t* )img.data); break;
  case 16:fillMapPoint_template<uint16_t>(xyz[2], tinfo.xtent.extent_min[2], tinfo.mapping[2], offs, (uint16_t*)img.data); break;
  case 32:fillMapPoint_template<uint32_t>(xyz[2], tinfo.xtent.extent_min[2], tinfo.mapping[2], offs, (uint32_t*)img.data); break;
  }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltCPUFill::fillTriStep(double t)
{
  int i;
  double s=0.0;
  double a[3], b[3];
  double xyz[3];

  // start/end of path on both edges of triangle at step t
  for ( i = 0; i < 3; ++i )
  {
    a[i] = fp.v1[i] + t * fp.from1[i];
    b[i] = fp.v2[i] + t * fp.from2[i];
  }

  // direction from edge to edge
  double d[3]={b[0]-a[0], b[1]-a[1], b[2]-a[2]};

  // marching the ray from edge to edge
  while ( s <= 1.0 )
  {
    fillLineStep(s,a,d,xyz); // gets the point

    // every point xyz to the heightmap
    fillMapPoint(xyz);

    // advance in ray
    s += fp.step;
  }
}

//////////////////////////////////////////////////////////////////////////
// It's a very unoptimized way to raster a triangle.
// Use Bresenham algorithm instead, but adapted to get the Z of the original triangle
// to paint, so Bresenham 3D.
// Example: http://www.mathworks.com/matlabcentral/fileexchange/21057-3d-bresenhams-line-generation/content/bresenham_line3d.m
// Right now it computes the smallest step 
 void fltCPUFill::fillTri(double* v0, double* v1, double* v2)
{
  int i;

  // directions to v0 from v1 and v2
  for ( i = 0; i < 3; ++i )
  {
    fp.v1[i]=v1[i];
    fp.v2[i]=v2[i];
    fp.from1[i]=v0[i]-v1[i];
    fp.from2[i]=v0[i]-v2[i];
  }

  // step depending on the triangle
  fp.step = fillComputeStep(v0,v1,v2);
  
  // goes from 0 to 1 to go filling the triangle form edge to edge
  double t = 0.0;
  while ( t <= 1.0 )
  { 
    fillTriStep(t);
    t += fp.step;
  }
  fillTriStep(1.0);
}

/////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
double inline zcross(double* v0, double* v1, double* v2)
{
  // #define cpoly_zcross(x0,y0, x1, y1, x2, y2) (((x1)-(x0))*((y2)-(y1)) - ((y1)-(y0))*((x2)-(x1)))
  return (v1[0]-v0[0])*(v2[1]-v1[1]) - (v1[1]-v0[1])*(v2[0]-v1[0]);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltCPUFill::fillNodeTris(flt_node* node)
{
  if ( !node ) return;
  flt* of = tinfo.of;

  // if we got triangles here in this node
  if ( node->ndx_pairs_count )
  {
    // for all batches
    fltu32 start,end;
    double *v0, *v1, *v2;
    double* vtxarray=(double*)of->pal->vtx_array; // 3 consecutive xyz doubles (see opts when loading flt)
    fltu32 hashe, index;
    double cross;
    for (fltu32 i=0;i<node->ndx_pairs_count;++i)
    {
      // get the batch start/end vertices
      FLTGET32(node->ndx_pairs[i],start,end);

      // every tree vertices is a triangle, indices to vertices in of->pal->vtx_array
      for (fltu32 j=start;j<end; j+=3)
      {
        FLTGET32(of->indices->data[j],hashe,index);
        v0 = vtxarray + (index*3);

        if ( (j+1)>end ) break;
        FLTGET32(of->indices->data[j+1],hashe,index);
        v1 = vtxarray + (index*3);

        if ( (j+2)>end ) break;
        FLTGET32(of->indices->data[j+2],hashe,index);
        v2 = vtxarray + (index*3);

        // check if is cw or ccw
        cross = zcross(v0,v1,v2);

        // fill this triangle
        const int c=config.culling;
        if ( c == CULLING_NONE || (c==CULLING_CW && cross < 0.0) || (c==CULLING_CCW && cross > 0.0) )
        {
          fillTri(v0,v1,v2);
          ++g_numtris;
        }
        else
          ++g_numtrisbf;
      }
    }
  }

  flt_node* child = node->child_head;
  while ( child )
  {
    fillNodeTris(child);
    child = child->next;
  }
}

#pragma endregion


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltTaskSingleTile::runTask(fltThreadPool* tp)
{
  flt* of = nullptr;
  fltTileInfo tinfo;
  fltImage outImg;

  // loading openflight and cache some info
  if ( !fltTasks::loadFlt(filename, tp->config, &of, &tinfo.fltnode, &tinfo.xtent) ) return;
  tinfo.fltfile = filename;
  tinfo.of = of;

  // tile and mapping image info/dimensions
  fltTasks::computeTileInfo(tp->config, tinfo.xtent, &tinfo);

  // allocate the data
  if ( !fltTasks::allocImage(tp->config, tinfo, &outImg) ) return;

  // fill data with heights
  fltTasks::fillImage(tp->config, tinfo, of, outImg);

  // unloading openflight info, don't need it anymore
  fltTasks::unloadFlt(&of);
  tinfo.of = nullptr;

  // saving image tile
  std::string finalname(filename);
  if ( !tp->config.nodename.empty() ) finalname+="_"+tp->config.nodename;
  finalname+=fltFormatExt(tp->config.format);
  fltTasks::saveImage(tp->config, outImg, finalname);

  // deallocating image
  fltTasks::deallocImage(&outImg);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltTaskMosaicTile::runTask(fltThreadPool* tp)
{
  switch(state)
  {
  case MOSAICTILESTATE_LOADFLT: stateLoadFlt(tp); break;
  case MOSAICTILESTATE_FILL: stateFill(tp); break;
  }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltTaskMosaicTile::stateLoadFlt(fltThreadPool* tp)
{
  if ( !fltTasks::loadFlt(filename, tp->config, &of, &tinfo.fltnode, &tinfo.xtent) ) return;
  tinfo.fltfile = filename;
  tinfo.of = of;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltTaskMosaicTile::stateFill(fltThreadPool* tp)
{
  fltTasks::fillImage(tp->config, tinfo, of, img);

  // we can safely unload flt once it's used
  fltTasks::unloadFlt(&of);
  tinfo.of = nullptr;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void doMosaicTilesProcess(fltThreadPool& tp, const std::vector<std::string>& files)
{
  // create one task per file
  std::vector<fltTaskMosaicTile*> mosaicTasks;
  double t0 = fltGetTime();

  fprintf( stderr, "Generating mosaic...\n" );

  mosaicTasks.reserve(files.size());
  for ( const std::string& f: files )
    mosaicTasks.push_back( new fltTaskMosaicTile(f) );

  fprintf( stderr, "Loading %d tiles...\n", files.size() );
  // Set the task to load flt and add to the pool
  for ( fltTaskMosaicTile* task : mosaicTasks )
  {
    task->state = MOSAICTILESTATE_LOADFLT;
    tp.addNewTask(task);
  }
  tp.waitForTasks(); // -- barrier

  fprintf( stderr, "Generating big mosaic image...\n" );
  // once all flt are loaded, compute total extension
  fltTileInfo fullTInfo;
  for ( fltTaskMosaicTile* task : mosaicTasks )
  {
    for (int i=0; i<3; ++i )
    {
      fullTInfo.xtent.extent_min[i] = std::min( task->tinfo.xtent.extent_min[i], fullTInfo.xtent.extent_min[i] );
      fullTInfo.xtent.extent_max[i] = std::max( task->tinfo.xtent.extent_max[i], fullTInfo.xtent.extent_max[i] );
    }
  }

  // all tiles will have same info computed out of the full extent, generate big mosaic height field
  fltImage fullImg;
  fltTasks::computeTileInfo(tp.config, fullTInfo.xtent, &fullTInfo);
  fltTasks::allocImage(tp.config, fullTInfo, &fullImg);

  fprintf( stderr, "Filling mosaic image with tiles...\n" );
  // now in parallel we're going filling every tile into the big mosaic image
  for ( fltTaskMosaicTile* task : mosaicTasks )
  {
    task->state = MOSAICTILESTATE_FILL;
    task->tinfo.setFromMosaic(fullTInfo);
    task->img = fullImg;
    tp.addNewTask(task);
  }
  tp.waitForTasks(); // -- barrier

  // saving image tile
  std::string finalname(tp.config.basepath + "/mosaic");
  finalname+=fltFormatExt(tp.config.format);
  fltTasks::saveImage(tp.config, fullImg, finalname);

  // deallocating image and removing persistent tasks
  fltTasks::deallocImage(&fullImg);
  for ( fltTaskMosaicTile* task: mosaicTasks )
    delete task;

  printf( "\n\nTime : %g secs\n", (fltGetTime()-t0)/1000.0);
  printf( "Triangles (ok/culled): %d/%d\n", g_numtris, g_numtrisbf);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void doSingleTilesProcess(fltThreadPool& tp, const std::vector<std::string>& files)
{
  // one task per file
  for ( const std::string& f : files )
    tp.addNewTask(new fltTaskSingleTile(f));

  // wait until cores finished
  double t0=fltGetTime();
  fprintf( stderr, "Processing...\n" );
  tp.waitForTasks();
  printf( "\n\nTime : %g secs\n", (fltGetTime()-t0)/1000.0);
  printf( "Triangles (ok/culled): %d/%d\n", g_numtris, g_numtrisbf);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int printUsage( const char* exec ) 
{
  char* program=flt_path_basefile(exec);
  printf("%s: Creates elevation file out of a FLT.\n\nBy default it converts all geometry in the FLT unless a LOD node name is specified\n\n", program );    
  printf("Usage: $ %s <options> <flt_files> \nOptions:\n", program );    
  printf("\t -f N         : Format. 0:PNG 1:DEM 2:RAW. Default=0\n");
  printf("\t -z N         : Depth bits. 8/16/32\n");
  printf("\t -o N         : Optional Node Opcode where to get the geometry from. Default=none=Any type\n");
  printf("\t -n <name>    : Optional Node Name where to get the geometry from. Default=none=All nodes\n");
  printf("\t -d <w> <h>   : Optional. Force this resolution for the final image, w=width, h=height. 0 to get aspect-ratio\n" );    
  printf("\t -w <wildcard>: Wildcard for files, i.e. flight*.flt\n" );
  printf("\t -c <culling> : 0:none 1:cw 2:ccw. Default=2\n");
  printf("\t -m           : Enable Mosaic mode. All in one image. Default=disabled\n" );
  printf("\t -g           : Enable GPU acceleration. Default=disabled\n" ); // will be removed and will use GPU acceleration where available by default
  printf("\nExamples:\n" );
  printf("\tConvert all geometry of the flt files, force 512x512 image\n" );
  printf("\t $ %s -d 512 512 -w flight*.flt\n\n", program);
  printf("\tConvert tile into a grayscale image, only geometry under LOD name \"l1\" is converted:\n" );
  printf("\t $ %s -f 0 -n l1 -o 73 flight1_1.flt\n\n", program);
  printf("\tConvert into raw format 32 bit forcing 512 width (height will be computed\n");
  printf("\t $ %s -f 2 -d 512 0 -z 32 flight1_1.flt\n\n", program);
  printf("\tCreates a big heightmap image with all of them\n");
  printf("\t $ %s -m -w flight?_?.flt\n\n", program);
  flt_free(program);
  return -1;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
int main(int argc, const char** argv)
{
  int format=FORMAT_PNG;
  std::string nodename;
  int nodetype=-1;
  int culling=CULLING_CCW;
  int depth=8;
  std::vector<std::string> files;
  std::string wildcard;
  bool mosaicMode=false;

  // gathering configuration from parameters
  int w=-1, h=-1;
  for ( int i = 1; i < argc; ++i )
  {
    if ( argv[i][0]=='-' )
    {
      switch ( tolower(argv[i][1]) )
      {
        case 'f': if (i+1<argc) format=atoi(argv[++i]); break;
        case 'o': if (i+1<argc) nodetype=atoi(argv[++i]); break;
        case 'n': if (i+1<argc) nodename=argv[++i]; break;
        case 'w': if (i+1<argc) wildcard=argv[++i]; break;
        case 'c': if (i+1<argc) culling=atoi(argv[++i]); break;
        case 'z': if (i+1<argc) depth=atoi(argv[++i]); break;
        case 'm': mosaicMode=true; break;
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

  // checking parameters
  if ( format < FORMAT_PNG || format >= FORMAT_MAX )   format=FORMAT_PNG;
  if ( format == FORMAT_DEM )
  {
    fprintf( stderr, "Not implemented DEM format\n" );
    return -1;
  }
  if (culling< CULLING_NONE || culling > CULLING_CCW) culling=CULLING_CCW;
  if ( depth != 8 && depth != 16 && depth != 32 ) depth = 8;
  if ( format == FORMAT_PNG && depth != 8 )
  {
    fprintf(stderr, "Forcing 8 bits depth for PNG format\n");
    depth = 8;
  }

  // gathering files if wildcard
  if ( !wildcard.empty() )
    fltGetFilesFromWildcard(wildcard, files);
  
  // returning if not files
  if ( files.empty() )
    return printUsage(argv[0]);

  // creates the threadpool and initializes with configuration
  fltThreadPool tp;
  tp.init(std::thread::hardware_concurrency()*2);
  tp.config.format = format;
  tp.config.nodename = nodename;
  tp.config.nodetype = nodetype;
  tp.config.forceDim[0]=w;
  tp.config.forceDim[1]=h;
  tp.config.culling=culling;
  tp.config.depth = depth;
  tp.config.mosaicMode = mosaicMode;
  if ( mosaicMode )
  {
    // saving the first file base path for mosaic output image
    char* basepath = flt_path_base(files[0].c_str());
    if ( basepath )
    {
      tp.config.basepath = basepath;
      flt_free(basepath);
    }
  }
  g_numtris = 0;
  g_numtrisbf = 0;
  // Single or Mosaic mode
  if ( mosaicMode )
    doMosaicTilesProcess(tp, files);    
  else
    doSingleTilesProcess(tp, files);

  // finishes thread pool
  tp.deinit();  
  return 0;
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
const char* fltFormatExt(int fmt)
{
  switch ( fmt )
  {
  case FORMAT_RAW: return ".raw";
  case FORMAT_PNG: return ".png";
  }
  return ".heightmap";
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltGetFilesFromWildcard(const std::string& wildcard, std::vector<std::string>& outfiles)
{
#if defined(_WIN32) || defined(_WIN64)
  char filterTxt[MAX_PATH];  
  sprintf_s( filterTxt, "%s", wildcard.c_str() );
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