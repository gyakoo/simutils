
#pragma warning(disable:4100 4005)

#if defined(_DEBUG) && !defined(_WIN64)
#include <vld.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#define FLT_NO_OPNAMES
#define FLT_LEAN_FACES
//#define FLT_ALIGNED
#define FLT_UNIQUE_FACES
#define FLT_IMPLEMENTATION
#include <flt.h>

#define VIS_IMPLEMENTATION
#include <vis.h>
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

// types
struct fltThreadTask
{
  enum f2dTaskType { TASK_NONE=-1, TASK_FLT=0};
  f2dTaskType type;

  std::string filename;
  flt* of;
  flt_opts* opts;

  fltThreadTask():type(TASK_NONE), of(NULL), opts(NULL){}
  fltThreadTask(f2dTaskType tt, const std::string& f, flt* of, flt_opts* opts)
    :type(tt), filename(f), of(of), opts(opts){}
  
  void runTask(fltThreadPool* tp);
};


struct fltThreadPool
{
  void init();
  void runcode();
  bool getNextTask(fltThreadTask* t);
  void addNewTask(const fltThreadTask& task);
  void deinit();
  bool isWorking(){ return !tasks.empty() || workingTasks>0; }

  std::vector<std::string> searchpaths;
  std::vector<std::thread*> threads;
  std::deque<fltThreadTask> tasks;
  std::mutex mtxTasks;
  std::mutex mtxFiles;
  std::atomic_int workingTasks;
  std::atomic_int nfiles;
  int numThreads;
  bool finish;
};

double fltGetTime();
bool fltExtensionIsImg(const char* filename);
bool fltExtensionIs(const char* filename, const char* ext);

void fltThreadPool::init()
{
  deinit();
  finish=false;
  threads.reserve(numThreads);
  workingTasks=0;
  nfiles=0;
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

void fltThreadTask::runTask(fltThreadPool* tp)
{
  tp=tp;
  switch( type )
  {
  case TASK_FLT: flt_load_from_filename(filename.c_str(), of, opts); break;
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
  static const char* supported[13]={".rgb", ".rgba", ".sgi", ".jpg", ".jpeg", ".png", ".tga", ".bmp", ".psd", ".gif", ".hdr", ".pic", ".pnm"};
  for (int i=0;i<13;++i)
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

void read_with_callbacks_mt(const char* filename)
{
  double t0=fltGetTime();
  flt_opts* opts=(flt_opts*)flt_calloc(1,sizeof(flt_opts));
  flt* of=(flt*)flt_calloc(1,sizeof(flt));

  fltThreadPool tp;
  tp.numThreads = std::thread::hardware_concurrency();
  tp.init();

  // configuring read options
  opts->palflags = FLT_OPT_PAL_ALL;
  opts->hieflags = FLT_OPT_HIE_ALL_NODES;
  opts->dfaces_size = 1543;
  opts->cb_extref = fltCallbackExtRef;
  opts->cb_user_data = &tp;

  // master file task
  tp.nfiles=1;
  fltThreadTask task(fltThreadTask::TASK_FLT, filename, of, opts);
  tp.addNewTask(task);  

  // wait until cores finished
  do
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));    
  } while ( tp.isWorking() );

  char tmp[256]; 
  sprintf_s(tmp, "Time: %.4g secs", (fltGetTime()-t0)/1000.0);
  printf( "\n%s\n",tmp);
  printf("nfiles total: %d\n", tp.nfiles);
  printf("nfaces total : %d\n", TOTALNFACES);
#ifdef FLT_UNIQUE_FACES
  printf("nfaces unique: %d\n", TOTALUNIQUEFACES);
#endif
  printf("nindices total: %d\n", TOTALINDICES);

  {
    vis* v;
    vis_opts opts;
    opts.width = 1024;
    opts.height= 768;
    opts.title="fltview";
    if ( vis_init(&v,&opts) == VIS_OK )
    {
      while( vis_begin_frame(v) == VIS_OK )
      {
        vis_render_frame(v);
        vis_end_frame(v);
      }
      vis_release(&v);
    }
  }
  flt_release(of);
  flt_safefree(of);
  flt_safefree(opts);

  tp.deinit();
}

int main(int argc, const char** argv)
{
  argc=argc;
  argv=argv;
  read_with_callbacks_mt("../../../data/utah/master.flt");
  return 0;
}