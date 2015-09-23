// The MIT License (MIT)
// 
// Copyright (c) 2015 Manu Marin / @gyakoo
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

// -= Summary =-
//
// This utility will output a height field in any of the supported formats (eHFFormat).
// The projected plane by default is XY and the height Z. (in the future it'll be configurable).
// It will exploit multicore by creating multiple threads, one thread per flt file.
// It can run basically in two modes, normal mode and mosaic mode (-m).
// For normal mode, it'll output a single height field file per loaded flt file.
// For mosaic mode, it'll output a global height field containing all the flt files.
// Several files can be specified in command line or by using DOS style wildcards (-w)
// Also, by default it will create a heightfield of the dimensions depending on the input flt file
//  extent, but can be forced to a specific dimension with -d option. Also if one of the dimension
//  is forced to 0, will be computed keeping the aspect ratio of the original flt file extent.
// See printUsage() for more information.

#define NOMINMAX
#if defined(_DEBUG) && !defined(_WIN64)
#include <vld.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#define FLT_IMPLEMENTATION
#include <flt.h>
#pragma warning(disable:4100 4005 4996)
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <cstdint>
#include <string>
#include <mutex>
#include <atomic>
#include <deque>
#include <thread>
#include <vector>

// forwards
struct ThreadPool;

#pragma region Structures Declaration  *************************************************************
//////////////////////////////////////////////////////////////////////////
// Supported output formats for the Height Field
//////////////////////////////////////////////////////////////////////////
enum eHFFormat
{
  HFFORMAT_PNG=0,
  HFFORMAT_DEM=1,
  HFFORMAT_RAW=2,
  HFFORMAT_MAX=3,
};

//////////////////////////////////////////////////////////////////////////
// Triangle culling
//////////////////////////////////////////////////////////////////////////
enum eCulling
{
  CULLING_NONE=0,
  CULLING_CW=1,
  CULLING_CCW=2
};

//////////////////////////////////////////////////////////////////////////
// Represents the extension of a Flt file. The axis-aligned bounding box.
//////////////////////////////////////////////////////////////////////////
struct FltExtent
{
  double extent_max[3];
  double extent_min[3];

  FltExtent(){invalidate();}

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
// Information about a tile
//////////////////////////////////////////////////////////////////////////
struct FltTileInfo
{
  FltTileInfo()
    : of(nullptr), fltnode(nullptr)
  { }

  void setFromMosaic(const FltTileInfo& full)
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
  flt* of;              // flt in memory
  flt_node* fltnode;    // node below which the geometry will be taken

  FltExtent xtent;      // extent of the flt geometry
  double dim[3];        // width/depth/height of the extent
  double mapping[3];    // to map point in flt to heightfield coordinates
  int width, height;    // of the heightfield
};

//////////////////////////////////////////////////////////////////////////
// it's the height values in memory. The data will depend on the depth
// which can be 8, 16 or 32 bits.
//////////////////////////////////////////////////////////////////////////
struct HeightField
{
  HeightField() : data(nullptr), width(0), height(0), depth(8) { }

  uint8_t* data;
  int width;
  int height;
  int depth;
};

//////////////////////////////////////////////////////////////////////////
// Base class for a task executed by the thread pool in any of the available
// threads. run() method has to be implemented and release() is optional.
//////////////////////////////////////////////////////////////////////////
struct Task
{
  Task(const std::string& fname):filename(fname)
  {
  }

  virtual void run(ThreadPool* tp) = 0;
  virtual void release(){}

  std::string filename;
};

//////////////////////////////////////////////////////////////////////////
// This task will do the whole process for a single tile, from loading the
// flt file until outputting it into a file. These tasks don't have any
// dependencies on others.
//////////////////////////////////////////////////////////////////////////
struct TaskSingleFile : public Task
{
  TaskSingleFile(const std::string& f): Task(f)
  {
  }
  virtual void release(){ delete this; }  
  virtual void run(ThreadPool* tp);
};

//////////////////////////////////////////////////////////////////////////
// This task can do two type of processes depending on state data.
// - STATE_LOADFLT: Will load the flt file into memory
// - STATE_FILL: Will fill the (shared) heightfield and unload the flt. 
//      This state will depend on the LOADFLT and the mosaic image
//      generation process done in main thread.
// Main thread will enqueue these tasks accordingly with the pipeline.
//////////////////////////////////////////////////////////////////////////
struct TaskMosaicTile : public Task
{
  enum eState
  {
    STATE_UNKNOWN=-1,
    STATE_LOADFLT=0,
    STATE_FILL,     
  };

  TaskMosaicTile(const std::string& f): Task(f), state(STATE_UNKNOWN)
  {
  }

  virtual void run(ThreadPool* tp);

  eState state;       // what this task does next
  flt* of;            // flt file in memory
  FltTileInfo tinfo;  // information about how to fill the heightmap with this tile
  HeightField img;    // target shared heightmap

private:
  void stateLoadFlt(ThreadPool* tp);
  void stateFill(ThreadPool* tp);
};

//////////////////////////////////////////////////////////////////////////
// Has the configuration about the process. Shared configuration data.
// Its values are accessed read-only by the threads
//////////////////////////////////////////////////////////////////////////
struct ElevationConfig
{
  ElevationConfig(){ reset(); }

  void reset()
  {
    nodename.clear();
    basepath.clear();
    wildcard.clear();
    mosaicMode=false;
    forceDim[0]=forceDim[1]=-1;
    format=HFFORMAT_PNG;
    nodetype=-1;
    culling=CULLING_CCW;
    depth=8;
    recurseDir=false;
  }
  bool validate();
  bool parseCLI(int argc, const char** argv, std::vector<std::string>& outFiles);

  std::string nodename;   // optional name of the node that we use to get the geometry 
  std::string basepath;   // (mosaic mode) base path where to save the single mosaic heightfield
  std::string wildcard;   // optional wildcard to gather flt files
  bool recurseDir;        // recurse directories when using wildcards
  bool mosaicMode;        // true to activate single heightmap mosaic
  int forceDim[2];        // width/height of heightmap image or -1 if use flt files extent
  eHFFormat format;       // format for heightfield
  int nodetype;           // optional node type used when looking for nodename for geometry
  eCulling culling;       // optional culling for triangles
  int depth;              // depth of heightmap image. 8, 16 or 32 bits
};

//////////////////////////////////////////////////////////////////////////
// These global functions are thread safe and called from different threads
//////////////////////////////////////////////////////////////////////////
struct Helper
{
public:
    // Will open and parse the flt file. Also computes the extent of it and looks for the optional given node with geometry
  static bool loadFlt(const std::string& fltfile, const ElevationConfig& config, flt** outOf, flt_node** outNode, FltExtent* outXtent);

    // Compute the information to be used for geometry filling the heightfield
  static bool computeTileInfo(const ElevationConfig& config, const FltExtent& xtent, FltTileInfo* outTInfo);

    // Creates an empty heightfield
  static bool allocImage(const ElevationConfig& config, const FltTileInfo& tinfo, HeightField* outImg);

    // Fills the heightfield image with height values of the flt in memory and config.
  static bool fillImage(const ElevationConfig& config, const FltTileInfo& tinfo, flt* of, HeightField& img);

    // Outputs the heightfield to disk
  static bool saveImage(const ElevationConfig& config, const HeightField& img, const std::string& filename );

    // Destroys the heightfield
  static bool deallocImage(HeightField* img);

    // Unload the flt file from memory
  static bool unloadFlt(flt** of);

    // Returns current time since process started in milliseconds
  static double getCurrentTime();

    // Returns the extension with . of the given format
  static const char* getFormatExtension(eHFFormat fmt);

    // Gets all files specified by the DOS-style wildcard
  static void gatherFilesFromWildcard(const std::string& wildcard, bool recurse, std::vector<std::string>& outfiles);

    // Force value in range
  template<typename T> static T clamp(T v, T _m, T _M){ return v<_m?_m:(v>_M?_M:v);}

    // Cross products of vectors v0->v1 and v0->v2. Used to know if triangle v0,v1,v2 is CW or CCW
  static double inline crossProductTriangle(double* v0, double* v1, double* v2);

    // globals
  static std::atomic_int g_numtris;   // number of triangles processed
  static std::atomic_int g_numtrisbf; // number of triangles culled

private:
    // Compute extent of geometry under node
  static void computeNodeExtent(flt* of, FltExtent* xtent, flt_node* node);

    // Compute extent of whole flt if node==null. (more optimal version)
  static void computeExtent(flt* of, FltExtent* xtent, flt_node* node);

    // Look for the lod given the name and the type.
  static bool findSpecificNode(flt_node* node, const std::string& lodname, int nodetype, flt_node** outNode);

    // Helper to log a message and return specific error
  template<typename T> static T retErr(T res, const char* format, ...);
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
struct FltFillCpu
{
public:
  struct fillParams
  {
    double v1[3]; double from1[3];
    double v2[3]; double from2[3];
    double step;
  };

  FltFillCpu(const ElevationConfig& config, const FltTileInfo& tinfo, flt* of, HeightField& img)
    : config(config), tinfo(tinfo), of(of), img(img)
  {
  }
  FltFillCpu& operator =(const FltFillCpu& o){}

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

  const ElevationConfig& config;
  const FltTileInfo& tinfo;
  flt* of;
  HeightField& img;
  fillParams fp;  
};

#pragma endregion

#pragma region Thread Pool *************************************************************************
//////////////////////////////////////////////////////////////////////////
// List of threads running and executing tasks.
// This implementation does a spin loop with a sleep when no task is found.
// Has to be changed for a condition variable (event/semaphore).
//////////////////////////////////////////////////////////////////////////
struct ThreadPool
{
public:
  // Creates numthreads threads and begin running
  void init(int numthreads)
  {
    deinit();
    finish=false;
    workingTasks=0;    
    threads.reserve(numthreads);
    for(int i = 0; i < numthreads; ++i)
      threads.push_back(new std::thread(&ThreadPool::runcode,this));  
  }

  // Every thread's running method. Called from different threads.
  void runcode()
  {
    Task* task=nullptr;
    while (!finish)
    {
      task=getNextTask();
      if ( task )
      {
        ++workingTasks;
        task->run(this);
        --workingTasks;
        task->release();
      }
      else
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
  }

  // gets the next task to process or null if no avail
  Task* getNextTask()
  {
    std::lock_guard<std::mutex> lock(mtxTasks);
    if ( tasks.empty() ) return nullptr;
    Task* t = tasks.back();
    tasks.pop_back();
    return t;
  }

  // enqueue a new task
  void addNewTask(Task* task)
  {
    std::lock_guard<std::mutex> lock(mtxTasks);
    tasks.push_front( task );
  }

  // waits for all threads and remove them
  void deinit()
  {
    finish = true; // force finish runcode loop of threads
    for (std::thread* t : threads)
    {
      t->join();
      delete t;
    }
    threads.clear();

    // be sure rest of tasks are deallocated
    for (Task* t : tasks) 
      delete t;
    tasks.clear();
  }

  // returns true if there are threads working on tasks yet
  bool isWorking()
  { 
    return !tasks.empty() || workingTasks>0; 
  }

  // make the caller threads to wait in a spin loop until no tasks
  void waitForTasks(int ms=250)
  {
    do
    {
      std::this_thread::sleep_for(std::chrono::milliseconds(ms));
    } while ( isWorking() );
  }

  // getter/setter of the global configuration
  ElevationConfig& getConfig(){ return config; }
  void setConfig(const ElevationConfig& cfg){ config = cfg; }

private:
  std::vector<std::thread*> threads;  // collection of threads
  std::deque<Task*> tasks;            // current queue of tasks
  std::mutex mtxTasks;                // mutex to access the tasks thread-safe
  std::atomic_int workingTasks;       // number of current running tasks
  ElevationConfig config;             // execution configuration
  std::atomic_bool finish;
};
#pragma endregion

#pragma region Tasks *******************************************************************************
void TaskSingleFile::run(ThreadPool* tp)
{
  flt* of = nullptr;
  FltTileInfo tinfo;
  HeightField outImg;
  ElevationConfig& config = tp->getConfig();

  // loading openflight and cache some info
  if ( !Helper::loadFlt(filename, config, &of, &tinfo.fltnode, &tinfo.xtent) ) 
  {
    fprintf( stderr, "Error loading: %s\n", filename.c_str() );
    return;
  }
  tinfo.fltfile = filename;
  tinfo.of = of;

  // tile and mapping image info/dimensions
  Helper::computeTileInfo(config, tinfo.xtent, &tinfo);

  // allocate the data
  if ( !Helper::allocImage(config, tinfo, &outImg) )
  {
    fprintf( stderr, "Error allocating image: %s\n", filename.c_str() );
    return;
  }

  // fill data with heights
  Helper::fillImage(config, tinfo, of, outImg);

  // unloading openflight info, don't need it anymore
  Helper::unloadFlt(&of);
  tinfo.of = nullptr;

  // saving image tile
  std::string finalname(filename);
  if ( !config.nodename.empty() ) finalname+="_"+config.nodename;
  finalname+=Helper::getFormatExtension(config.format);
  if ( ! Helper::saveImage(config, outImg, finalname) )
    fprintf( stderr, "Error saving image: %s\n", finalname.c_str() );

  // deallocating image
  Helper::deallocImage(&outImg);
}

void TaskMosaicTile::run(ThreadPool* tp)
{
  switch(state)
  {
  case STATE_LOADFLT: stateLoadFlt(tp); break;
  case STATE_FILL   : stateFill(tp); break;
  }
}

void TaskMosaicTile::stateLoadFlt(ThreadPool* tp)
{
  if ( !Helper::loadFlt(filename, tp->getConfig(), &of, &tinfo.fltnode, &tinfo.xtent) ) return;
  tinfo.fltfile = filename;
  tinfo.of = of;
}

void TaskMosaicTile::stateFill(ThreadPool* tp)
{
  Helper::fillImage(tp->getConfig(), tinfo, of, img);

  // we can safely unload flt once it's used
  Helper::unloadFlt(&of);
  tinfo.of = nullptr;
}

#pragma endregion

#pragma region Helper functions ********************************************************************
std::atomic_int Helper::g_numtris;
std::atomic_int Helper::g_numtrisbf;

template<typename T>
T Helper::retErr(T res, const char* format, ...)
{
  va_list args;
  fprintf( stderr, "Error: " );
  va_start(args, format);
  vfprintf( stderr, format, args);
  va_end(args);
  return res;
}

void Helper::computeNodeExtent(flt* of, FltExtent* xtent, flt_node* node)
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

void Helper::computeExtent(flt* of, FltExtent* xtent, flt_node* node)
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

bool Helper::loadFlt(const std::string& fltfile, const ElevationConfig& config, flt** outOf, flt_node** outNode, FltExtent* outXtent)
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

bool Helper::computeTileInfo(const ElevationConfig& config, const FltExtent& xtent, FltTileInfo* outTInfo)
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
  const bool doHeightScale = config.depth==8 || (dim[2]>maxz); // only for 8-bit we scale up. for the rest only scale down.
  const double MAPPING[3] = { 1.0/dim[0]*WIDTH, 1.0/dim[1]*HEIGHT, doHeightScale ? maxz/dim[2] : 1.0 };

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

bool Helper::unloadFlt(flt** of)
{
  if ( !of || !*of ) return false;
  flt_release(*of);
  free(*of);
  *of = nullptr;
  return true;
}

bool Helper::allocImage(const ElevationConfig& config, const FltTileInfo& tinfo, HeightField* outImg)
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

bool Helper::deallocImage(HeightField* img)
{
  if (img->data)
  {
    free(img->data);
    img->width = img->height = img->depth = 0;
    return true;
  }
  return false;
}

bool Helper::fillImage(const ElevationConfig& config, const FltTileInfo& tinfo, flt* of, HeightField& img)
{
  return FltFillCpu(config, tinfo, of, img).fill(tinfo.fltnode);
}

bool Helper::saveImage(const ElevationConfig& config, const HeightField& img, const std::string& filename )
{
  bool res = false;
  switch ( config.format )
  {
  case HFFORMAT_PNG: 
    res = stbi_write_png(filename.c_str(), img.width, img.height, 1, img.data, img.width) != 0;
  break;
  case HFFORMAT_RAW:
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

bool Helper::findSpecificNode(flt_node* node, const std::string& lodname, int nodetype, flt_node** outNode)
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

double Helper::getCurrentTime()
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

const char* Helper::getFormatExtension(eHFFormat fmt)
{
  const char* extensions[HFFORMAT_MAX]={".png", ".dem", ".raw" };
  return (fmt>=0 && fmt <HFFORMAT_MAX) ? extensions[fmt] : ".heightmap";
}

double inline Helper::crossProductTriangle(double* v0, double* v1, double* v2)
{
  return (v1[0]-v0[0])*(v2[1]-v1[1]) - (v1[1]-v0[1])*(v2[0]-v1[0]);
}

void Helper::gatherFilesFromWildcard(const std::string& wildcard, bool recurse, std::vector<std::string>& outfiles)
{
#if defined(_WIN32) || defined(_WIN64)
  char filterTxt[MAX_PATH];  
  sprintf_s( filterTxt, "%s", wildcard.c_str() );
  char* basepath = flt_path_base(filterTxt);
  std::string bpath = basepath ? basepath : "./";
  // files for this directory
  WIN32_FIND_DATAA ffdata;
  HANDLE hFind = FindFirstFileA( filterTxt, &ffdata );
  if ( hFind != INVALID_HANDLE_VALUE )
  {
    do
    {
      if ( ffdata.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE )
      {
        outfiles.push_back( bpath+ffdata.cFileName );
      }
    }while ( FindNextFile(hFind,&ffdata)!=0 );
    FindClose(hFind);
  }
  flt_safefree(basepath);
#else
#error "Implement this function for other platforms"
#endif
}

#pragma endregion

#pragma region CPU Fill ****************************************************************************

bool FltFillCpu::fill(flt_node* node)
{
  if ( !node ) return false;
  fillNodeTris(node);
  return true;
}

double FltFillCpu::fillComputeStep(double* v0, double *v1, double* v2)
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

void FltFillCpu::fillLineStep(double t, double* a, double* d, double* xyz)
{
  xyz[0]=a[0]+t*d[0];
  xyz[1]=a[1]+t*d[1];
  xyz[2]=a[2]+t*d[2];
}

void FltFillCpu::fillMapXY(double _x, double _y, int* x, int *y)
{
  const FltExtent& xtent = tinfo.xtent;
  *x = (int)( (_x - xtent.extent_min[0]) * tinfo.mapping[0] ); 
  *x = Helper::clamp(*x,0,img.width-1);
  *y = (int)( (_y - xtent.extent_min[1]) * tinfo.mapping[1] ); 
  *y = Helper::clamp(*y,0,img.height-1);
}

template<typename T>
void FltFillCpu::fillMapPoint_template(double zval, double zmin, double mapping, int offs, T* data)
{
  T z, oldz;
  z = (T)(int)( (zval-zmin)*mapping);
  oldz = data[offs];
  if ( z > oldz ) data[offs] = z;
}

void FltFillCpu::fillMapPoint(double* xyz)
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

void FltFillCpu::fillTriStep(double t)
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
 void FltFillCpu::fillTri(double* v0, double* v1, double* v2)
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

void FltFillCpu::fillNodeTris(flt_node* node)
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
    bool doFill;
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
        
        // fill this triangle
        const eCulling c = config.culling;
        doFill = c==CULLING_NONE;
        if ( !doFill )
        {
          // check if is cw or ccw
          cross = Helper::crossProductTriangle(v0,v1,v2);
          doFill = (c==CULLING_CW && cross < 0.0) || (c==CULLING_CCW && cross > 0.0);
        }

        if ( doFill )
        {
          fillTri(v0,v1,v2);
          ++Helper::g_numtris;
        }
        else
          ++Helper::g_numtrisbf;
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

#pragma region Global Main Thread Functions ********************************************************

void doPrintStats(double t0)
{
  printf( "\n\nTime : %g secs\n", (Helper::getCurrentTime()-t0)/1000.0);
  printf( "Triangles (ok/culled): %d/%d\n", Helper::g_numtris, Helper::g_numtrisbf);
}

void doMosaicTilesProcess(ThreadPool& tp, const std::vector<std::string>& files)
{
  // create one task per file
  std::vector<TaskMosaicTile*> mosaicTasks;
  ElevationConfig& config = tp.getConfig();
  double t0 = Helper::getCurrentTime();

  fprintf( stderr, "Generating mosaic...\n" );

  mosaicTasks.reserve(files.size());
  for ( const std::string& f: files )
    mosaicTasks.push_back( new TaskMosaicTile(f) );

  fprintf( stderr, "Loading %d tiles...\n", files.size() );
  // Set the task to load flt and add to the pool
  for ( TaskMosaicTile* task : mosaicTasks )
  {
    task->state = TaskMosaicTile::STATE_LOADFLT;
    tp.addNewTask(task);
  }
  tp.waitForTasks(); // -- barrier

  fprintf( stderr, "Generating big mosaic image...\n" );
  // once all flt are loaded, compute total extension
  FltTileInfo fullTInfo;
  for ( TaskMosaicTile* task : mosaicTasks )
  {
    for (int i=0; i<3; ++i )
    {
      fullTInfo.xtent.extent_min[i] = std::min( task->tinfo.xtent.extent_min[i], fullTInfo.xtent.extent_min[i] );
      fullTInfo.xtent.extent_max[i] = std::max( task->tinfo.xtent.extent_max[i], fullTInfo.xtent.extent_max[i] );
    }
  }

  // all tiles will have same info computed out of the full extent, generate big mosaic height field
  HeightField fullImg;
  Helper::computeTileInfo(config, fullTInfo.xtent, &fullTInfo);
  Helper::allocImage(config, fullTInfo, &fullImg);

  fprintf( stderr, "Filling mosaic image with tiles...\n" );
  // now in parallel we're going filling every tile into the big mosaic image
  for ( TaskMosaicTile* task : mosaicTasks )
  {
    task->state = TaskMosaicTile::STATE_FILL;
    task->tinfo.setFromMosaic(fullTInfo);
    task->img = fullImg;
    tp.addNewTask(task);
  }
  tp.waitForTasks(); // -- barrier

  // saving image tile
  std::string finalname(config.basepath + "/mosaic");
  finalname+=Helper::getFormatExtension(config.format);
  Helper::saveImage(config, fullImg, finalname);

  // deallocating image and removing persistent tasks
  Helper::deallocImage(&fullImg);
  for ( TaskMosaicTile* task: mosaicTasks )
    delete task;

  doPrintStats(t0);  
}

void doSingleTilesProcess(ThreadPool& tp, const std::vector<std::string>& files)
{
  // one task per file
  for ( const std::string& f : files )
    tp.addNewTask(new TaskSingleFile(f));

  // wait until cores finished
  double t0=Helper::getCurrentTime();
  fprintf( stderr, "Processing %d tiles...\n", files.size() );
  tp.waitForTasks();
  doPrintStats(t0);
}

int printUsage( const char* exec ) 
{
  char* program=flt_path_basefile(exec);
  printf("%s: Creates elevation file out of a FLT.\n\nBy default it converts all geometry in the FLT unless a LOD node name is specified\n\n", program );    
  printf("Usage: $ %s <options> <flt_files> \nOptions:\n", program );    
  printf("\t -f N         : Format. 0:PNG 1:DEM 2:RAW. Default=0\n");
  printf("\t -z N         : Depth bits. 8/16/32\n");
  printf("\t -o N         : Optional Node Opcode where to get the geometry from. Default=none=Any type\n");
  printf("\t -n <name>    : Optional Node Name where to get the geometry from. Default=none=All nodes\n");
  printf("\t -d <w> <h>   : Optional. Force resolution for the final image, w=width, h=height. 0 for aspect-ratio\n" );    
  printf("\t -w <wildcard>: Wildcard for files, i.e. flight*.flt\n" );
  printf("\t -r           : Recurse directories for wildcard. Default=false\n" );
  printf("\t -c <culling> : 0:none 1:cw 2:ccw. Default=2\n");
  printf("\t -m           : Enable Mosaic mode. All in one image. Default=disabled\n" );
  printf("\t -g           : Enable GPU acceleration. Default=disabled\n" ); // will be removed and will use GPU acceleration where available by default
  printf("\nExamples:\n" );
  printf("\tConvert all geometry of the flt files to pngs, force 512x512 images\n" );
  printf("\t $ %s -d 512 512 -w flight*.flt\n\n", program);
  printf("\tConvert tile into a grayscale image, only geometry under LOD name \"l1\" is converted:\n" );
  printf("\t $ %s -f 0 -n l1 -o 73 flight1_1.flt\n\n", program);
  printf("\tConvert into raw format 32 bit forcing 512 width (height will be computed)\n");
  printf("\t $ %s -f 2 -d 512 0 -z 32 flight1_1.flt\n\n", program);
  printf("\tCreates a big heightmap image with all of them\n");
  printf("\t $ %s -m -w flight?_?.flt\n\n", program);
  flt_free(program);
  return -1;
}
#pragma endregion

#pragma region Elevation Configuration *************************************************************

bool ElevationConfig::validate() 
{
  // checking parameters
  if ( format < HFFORMAT_PNG || format >= HFFORMAT_MAX )   
    format=HFFORMAT_PNG;

  if ( format == HFFORMAT_DEM )
  {
    fprintf( stderr, "Not implemented DEM format\n" );
    return false;
  }

  if (culling< CULLING_NONE || culling > CULLING_CCW) 
    culling=CULLING_CCW;

  if ( depth != 8 && depth != 16 && depth != 32 ) 
    depth = 8;

  if ( format == HFFORMAT_PNG && depth != 8 )
  {
    fprintf(stderr, "Forcing 8 bits depth for PNG format\n");
    depth = 8;
  }
  return true;
}

bool ElevationConfig::parseCLI(int argc, const char** argv, std::vector<std::string>& outFiles)
{
  reset();

  // gathering configuration from parameters
  for ( int i = 1; i < argc; ++i )
  {
    if ( argv[i][0]=='-' )
    {
      switch ( tolower(argv[i][1]) )
      {
      case 'f': if (i+1<argc) format=static_cast<eHFFormat>(atoi(argv[++i])); break;
      case 'o': if (i+1<argc) nodetype=atoi(argv[++i]); break;
      case 'n': if (i+1<argc) nodename=argv[++i]; break;
      case 'w': if (i+1<argc) wildcard=argv[++i]; break;
      case 'c': if (i+1<argc) culling= static_cast<eCulling>(atoi(argv[++i])); break;
      case 'z': if (i+1<argc) depth=atoi(argv[++i]); break;
      case 'm': mosaicMode=true; break;
      case 'r': recurseDir=true; break;
      case 'd': 
        if (i+1<argc) forceDim[0]=atoi(argv[++i]); 
        if (i+1<argc) forceDim[1]=atoi(argv[++i]); 
        break;
      default : printf ( "Unknown option -%c\n", argv[i][1]); 
      }
    }
    else
      outFiles.push_back( argv[i] );
  }

  if ( !validate() ) 
    return false;
  
  // gathering files if wildcard
  if ( !wildcard.empty() )
    Helper::gatherFilesFromWildcard(wildcard, recurseDir, outFiles);

  // getting base path for mosaic heightfield 
  if ( mosaicMode && !outFiles.empty())
  {
    // saving the first file base path for mosaic output image
    char* tmp_basepath = flt_path_base(outFiles.begin()->c_str());
    if ( tmp_basepath )
    {
      basepath = tmp_basepath;
      flt_free(tmp_basepath);
    }
  }
  return true;
}
#pragma endregion

int main(int argc, const char** argv)
{
  ElevationConfig config;
  std::vector<std::string> files;

  // get config from command line
  if ( !config.parseCLI(argc, argv, files) )
    return printUsage(argv[0]);
  
  // returning if not files
  if ( files.empty() )
    return printUsage(argv[0]);

  // creates the threadpool and initializes with configuration
  ThreadPool tp;
  tp.init(std::thread::hardware_concurrency()*2); // optimal no of threads?
  tp.setConfig(config);  
  Helper::g_numtris = 0;
  Helper::g_numtrisbf = 0;

  // Single or Mosaic mode
  if ( config.mosaicMode )
    doMosaicTilesProcess(tp, files);    
  else
    doSingleTilesProcess(tp, files);

  // finishes thread pool
  tp.deinit();  
  return 0;
}