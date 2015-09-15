
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
#include <algorithm>

struct fltThreadPool;
struct fltThreadPoolContext;
struct fltThreadTask;

struct fltThreadTask
{
  enum f2dTaskType { TASK_NONE=-1, TASK_FLT=0};
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

struct fltXRefExtent
{
  std::string name;
  double extent_max[3];
  double extent_min[3];
  fltu64 vertices;
};

struct fltThreadPool
{
  void init();
  void runcode();
  bool getNextTask(fltThreadTask* t);
  void addNewTask(const fltThreadTask& task);
  void deinit();
  void incExtent(const std::string& xrefname, double* _xmin, double* _xmax, const fltu64& v);
  bool isWorking(){ return !tasks.empty() || workingTasks>0; }

  std::vector<std::thread*> threads;
  std::deque<fltThreadTask> tasks;  
  std::vector<fltXRefExtent> xref_extents;
  std::mutex mtxTasks;
  std::mutex mtxExt;
  std::atomic_int workingTasks;
  std::atomic_int nfiles;
  int numThreads;
  bool finish;
};

double fltGetTime();

void fltThreadPool::init()
{
  deinit();
  finish=false;
  threads.reserve(numThreads);
  workingTasks=0;
  nfiles=0;
  // first item in xref_extents is the total (accum)
  fltXRefExtent total;  
  for ( int i = 0; i < 3; ++i ){ total.extent_min[i]=DBL_MAX;  total.extent_max[i]=-DBL_MAX; }
  total.vertices=0;
  total.name = "Total";
  xref_extents.push_back(total);
  for(int i = 0; i < numThreads; ++i)
    threads.push_back(new std::thread(&fltThreadPool::runcode,this));  
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

void fltThreadPool::incExtent(const std::string& xrefname, double* _exmin, double* _exmax, const fltu64& v)
{
  std::lock_guard<std::mutex> lock(mtxExt);

  // incrementing total data
  fltXRefExtent& total = xref_extents[0];
  for ( int i = 0; i < 3; ++i )
  {
    if ( _exmin[i]<total.extent_min[i] ) total.extent_min[i]=_exmin[i];
    if ( _exmax[i]>total.extent_max[i] ) total.extent_max[i]=_exmax[i];
  }
  total.vertices += v;

  // register a new xref data
  fltXRefExtent xrefextent;
  xrefextent.name=xrefname;
  xrefextent.vertices = v;
  for (int i = 0; i < 3; ++i )
  {
    xrefextent.extent_max[i] = _exmax[i];
    xrefextent.extent_min[i] = _exmin[i];
  }
  xref_extents.push_back(xrefextent);
}

void fltThreadTask::runTask(fltThreadPool* tp)
{
  tp=tp;
  switch( type )
  {
  case TASK_FLT    : 
    flt_load_from_filename(filename.c_str(), of, opts); 
    if ( of->pal && of->pal->vtx_array && of->pal->vtx_count)
    {
      // we assume 3 doubles consecutive (see opts)
      double* xyz=(double*)of->pal->vtx_array;
      double exmin[3]={DBL_MAX, DBL_MAX, DBL_MAX};
      double exmax[3]={-DBL_MAX, -DBL_MAX, -DBL_MAX};
      int j;
      for ( size_t i = 0; i < of->pal->vtx_count; ++i, xyz+=3 )
      {
        for (j=0;j<3;++j)
        {
          if ( xyz[j]<exmin[j] ) exmin[j] = xyz[j];
          if ( xyz[j]>exmax[j] ) exmax[j] = xyz[j];
        }
      }
      char* basefile = flt_path_basefile(filename.c_str());
      tp->incExtent(basefile, exmin,exmax, of->pal->vtx_count);
      flt_safefree(basefile);
      //flt_release(of); // releasing a node ref not working yet
      flt_safefree(of->pal->vtx_array);      
    }
    break;
  }
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

bool xrefSorter( const fltXRefExtent& a, const fltXRefExtent& b )
{
  return a.name < b.name;
}

void read_with_callbacks_mt(const std::vector<std::string>& files, bool recurse, bool cacheformat)
{
  fltThreadPool tp;
  tp.numThreads = std::thread::hardware_concurrency()*2;
  tp.init();
    
  // configuring read options
  flt_opts* opts=(flt_opts*)flt_calloc(1,sizeof(flt_opts));
  flt* of=(flt*)flt_calloc(1,sizeof(flt));
  opts->pflags = FLT_OPT_PAL_TEXTURE | FLT_OPT_PAL_VERTEX | FLT_OPT_PAL_VTX_POSITION;
  opts->hflags = FLT_OPT_HIE_ALL_NODES | FLT_OPT_HIE_HEADER;
  opts->dfaces_size = 3079;//1543;
  opts->cb_extref = recurse ? fltCallbackExtRef : FLT_NULL;
  opts->cb_user_data = &tp;

  // master file task
  tp.nfiles=1;
  fltThreadTask task(fltThreadTask::TASK_FLT, files[0].c_str(), of, opts);
  tp.addNewTask(task);  

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
  fprintf( stderr, "\n" );
  if ( !cacheformat )
  {
    printf( "Units (Header) : %s\n", flt_get_vcoords_units_name(of->header->v_coords_units) );
    printf( "Radius (Header): %g\n", of->header->radius );
    printf( "Component Order: XYZ\n" );
    printf( "XRefs          : %d\n", tp.xref_extents.size()-1 );
    printf( "Time           : %g secs\n\n", (fltGetTime()-t0)/1000.0);
  }

  // keep out of sorting the first one (total)
  std::sort(tp.xref_extents.begin()+1, tp.xref_extents.end(), xrefSorter);
  
  // printing all x refs + total [0]
  double extension[3];
  double sqcmp[3];
  double cent[3];
  double radxy, radxyz;
  for (size_t x = 0; x < tp.xref_extents.size(); ++x )
  {
    const fltXRefExtent& ext=tp.xref_extents[x];
    for ( int i = 0; i < 3; ++i )
    {
      extension[i] = fabs(ext.extent_max[i]-ext.extent_min[i]);
      sqcmp[i]=extension[i]*extension[i];
      cent[i] = ext.extent_min[i] + extension[i]*0.5f;
    }
    radxy = sqrt(sqcmp[0]+sqcmp[1]);
    radxyz= sqrt(sqcmp[0]+sqcmp[1]+sqcmp[2]);

    if ( !cacheformat )
    {
      printf( "\n[%s]\n", ext.name.c_str() );
      printf( "Vertices       : %I64u\n", ext.vertices);
      printf( "Extension      : %g, %g, %g\n", extension[0], extension[1], extension[2] );
      printf( "Radius X/Y     : %g\n", radxy*0.5);
      printf( "Radius BSphere : %g\n", radxyz*0.5);
      printf( "Center         : %g %g %g\n", cent[0], cent[1], cent[2]);
      printf( "Min            : %g, %g, %g\n", ext.extent_min[0], ext.extent_min[1], ext.extent_min[2] );
      printf( "Max            : %g, %g, %g\n", ext.extent_max[0], ext.extent_max[1], ext.extent_max[2] );  
    }
    else
    {
      printf( "%s %g %g %g %g %g %g\n", ext.name.c_str(), ext.extent_min[0], ext.extent_min[1], ext.extent_min[2],
        ext.extent_max[0], ext.extent_max[1], ext.extent_max[2] );
    }
  }

  // releasing memory
  flt_release(of);
  flt_safefree(of);
  flt_safefree(opts->countable);
  flt_safefree(opts);
  tp.deinit();
}

int main(int argc, const char** argv)
{
  bool recurse = false;
  bool cacheformat = false;
  std::vector<std::string> files;
  for ( int i = 1; i < argc; ++i )
  {
    if ( argv[i][0]=='-' )
    {
      switch ( argv[i][1] )
      {
        case 'r': recurse = true; break;
        case 'c': cacheformat = true; break;
        default : fprintf ( stderr, "Unknown option -%c\n", argv[i][1]); 
      }
    }
    else
      files.push_back( argv[i] );
  }

  if ( !files.empty() )
  {
    read_with_callbacks_mt(files, recurse, cacheformat);
  }
  else
  {
    char* program=flt_path_basefile(argv[0]);
    fprintf(stderr, "%s: Compute extents of the bounding box containing all geometry in a FLT file\n\n", program );    
    fprintf(stderr, "Usage: $ %s <options> <flt_files> \nOptions:\n", program );    
    fprintf(stderr, "\t -r   : Recursive. Resolve all external references recursively\n");
    fprintf(stderr, "\t -c   : Cache file format. One line per entry with min and max only\n" );
    fprintf(stderr, "\nExamples:\n" );
    fprintf(stderr, "\tExtent of all the FLT files and their references in plain format:\n" );
    fprintf(stderr, "\t  $ %s -r -c master.flt > out.txt\n\n", program);    
    fprintf(stderr, "\tExtent of a model FLT file:\n" );
    fprintf(stderr, "\t  $ %s model.flt\n\n", program );
    flt_free(program);
  }
  
  return 0;
}