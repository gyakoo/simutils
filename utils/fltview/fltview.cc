
#ifdef _DEBUG
#include <vld.h>
#endif
// #define REPORTMEMLEAKS // empty by default
// #ifdef _MSC_VER
// # define _CRTDBG_MAP_ALLOC
// # include <crtdbg.h>
// # ifdef _DEBUG
// #   undef REPORTMEMLEAKS
// #   define REPORTMEMLEAKS _CrtDumpMemoryLeaks();
// # endif
// #endif

#include <stdio.h>
#include <stdlib.h>
#define FLT_NO_OPNAMES
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

#pragma warning(disable:4100)
struct fltThreadPool;
struct fltThreadPoolContext;
struct fltThreadTask;

// types
struct fltThreadTask
{
  enum f2dTaskType { TASK_UNKNOWN=-1, TASK_FLT=0, TASK_IMG=1 };
  f2dTaskType type;

  std::string filename;
  flt* of;
  flt_opts* opts;

  fltThreadTask():type(TASK_UNKNOWN), of(NULL), opts(NULL){}
  fltThreadTask(f2dTaskType tt, const std::string& f, flt* of, flt_opts* opts)
    :type(tt), filename(f), of(of), opts(opts){}
  fltThreadTask(const fltThreadTask& t)
  {
    *this = t;
  }
  void operator=(const fltThreadTask& t)
  {
    type=t.type;
    filename = t.filename;
    of = t.of;
    opts = t.opts;
  }
  
  void runTask(fltThreadPool* tp);
};


struct fltThreadPoolContext
{
  fltThreadPoolContext() : numThreads(0), verbose(false)
  {
  }
  //std::set<std::string> fltFiles;    
  //std::string wildcard; // if empty, no recurse wildcard
  int numThreads;
  bool verbose;  
};

struct fltThreadPool
{
  void init();
  void runcode();
  bool getNextTask(fltThreadTask* t);
  void addNewTask(const fltThreadTask& task);
  void deinit();

  fltThreadPoolContext ctx;
  std::vector<std::string> searchpaths;
  std::vector<std::thread*> threads;
  std::deque<fltThreadTask> tasks;
  std::set<std::string> addedFiles;
  std::mutex mtxTasks;
  std::mutex mtxFiles;
  std::atomic_int workingTasks;
  bool finish;
};

double fltGetTime();
bool fltExtensionIsImg(const char* filename);
bool fltExtensionIs(const char* filename, const char* ext);

int callback_extref(flt_node_extref* extref, flt* of, void* userdata)
{
  char* basefile = flt_path_basefile(extref->name);
  std::string finalname; finalname.reserve(64);
  if ( basefile )
  {
    if ( of->ctx->basepath ) finalname+=of->ctx->basepath;
    if ( !flt_path_endsok(of->ctx->basepath) ) finalname+="/";
  }
  finalname+=extref->name;

  // check if reference has been loaded already
  extref->of = (struct flt*)flt_dict_get(of->ctx->dict, finalname.c_str());
  if ( !extref->of ) // does not exist, creates one, loads it and put into dict
  {
    extref->of = (flt*)flt_calloc(1,sizeof(flt));
    extref->of->ctx = of->ctx;
    flt_dict_insert(of->ctx->dict, finalname.c_str(), extref->of);
    fltThreadTask task(fltThreadTask::TASK_FLT, finalname, extref->of, of->ctx->opts); //  read ref in parallel
    reinterpret_cast<fltThreadPool*>(userdata)->addNewTask(task);
  }
  else // file already loaded. uses and inc reference count
  {
    flt_atomic_inc(&extref->of->ref);
  }
  flt_safefree(basefile);
  return 1;
}

void read_with_callbacks_mt(const char* filename, fltThreadPool* tp)
{
  double t0;
  flt_opts* opts=(flt_opts*)flt_calloc(1,sizeof(flt_opts));
  flt* of=(flt*)flt_calloc(1,sizeof(flt));
  int err;

//   fltu16 vtxstream[4]={ 0 };
//   vtxstream[0]=flt_vtx_stream_enc(FLT_VTX_POSITION , 0,12); 
//   vtxstream[1]=flt_vtx_stream_enc(FLT_VTX_COLOR    ,12, 4);
//   vtxstream[2]=flt_vtx_stream_enc(FLT_VTX_NORMAL   ,16,12);
//   vtxstream[3]=0;

  // configuring read options
  opts->palflags =FLT_OPT_PAL_VERTEX | FLT_OPT_PAL_VTX_SOURCE;
  opts->hieflags = FLT_OPT_HIE_HEADER | FLT_OPT_HIE_FLAT | FLT_OPT_HIE_EXTREF;// | FLT_OPT_HIE_EXTREF_RESOLVE;
  opts->cb_extref = callback_extref;
  opts->cb_user_data = tp;

  // actual read
  t0=fltGetTime();

  fltThreadTask task(fltThreadTask::TASK_FLT, filename, of, opts);
  tp->addNewTask(task);  
  do
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));    
  } while ( !tp->tasks.empty() || tp->workingTasks>0 );

  printf( "\nTime: %g secs\n", (fltGetTime()-t0)/1000.0 );
  MessageBoxA(NULL,"continue","continue",MB_OK);
  flt_release(of);
  flt_safefree(of);
  flt_safefree(opts);
}

void read_with_resolve(const char* filename)
{
  double t0;
  flt_opts* opts=(flt_opts*)flt_calloc(1,sizeof(flt_opts));
  flt* of=(flt*)flt_calloc(1,sizeof(flt));

  //   fltu16 vtxstream[4]={ 0 };
  //   vtxstream[0]=flt_vtx_stream_enc(FLT_VTX_POSITION , 0,12); 
  //   vtxstream[1]=flt_vtx_stream_enc(FLT_VTX_COLOR    ,12, 4);
  //   vtxstream[2]=flt_vtx_stream_enc(FLT_VTX_NORMAL   ,16,12);
  //   vtxstream[3]=0;

  // configuring read options
  opts->palflags = 0;//FLT_OPT_PAL_VERTEX | FLT_OPT_PAL_VTX_SOURCE;
  opts->hieflags = /*FLT_OPT_HIE_HEADER | */FLT_OPT_HIE_FLAT | FLT_OPT_HIE_EXTREF | FLT_OPT_HIE_EXTREF_RESOLVE;

  // actual read
  t0=fltGetTime();

  flt_load_from_filename(filename,of,opts);
  
  printf( "\nTime: %g secs\n", (fltGetTime()-t0)/1000.0 );
  flt_release(of);
  flt_safefree(of);
  flt_safefree(opts);
}


void fltThreadPool::init()
{
  deinit();
  finish=false;
  threads.reserve(ctx.numThreads);
  workingTasks=0;
  for(int i = 0; i < ctx.numThreads; ++i)
    threads.push_back(new std::thread(&fltThreadPool::runcode,this));
}

void fltThreadPool::runcode()
{
  fltThreadTask task;
  while (!finish)
  {
    if ( getNextTask(&task) )
    {
      //OutputDebugStringA("Reading: " );OutputDebugStringA(task.filename.c_str());OutputDebugStringA("\n");
      ++workingTasks;
      try
      {
        task.runTask(this);
      }catch(...)
      {
        OutputDebugStringA("Exception\n");
      }
      --workingTasks;
      //OutputDebugStringA("Done: " );OutputDebugStringA(task.filename.c_str());OutputDebugStringA("\n");
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
  // img not supported, don't create task
  if ( task.type==fltThreadTask::TASK_IMG && !fltExtensionIsImg(task.filename.c_str()) )
    return;

  if ( task.of->ctx && !task.of->ctx->dict )
  {
    int a=0;
  }
  // check if file is done already
  {
    std::lock_guard<std::mutex> lock(mtxFiles);
    if ( addedFiles.find(task.filename)!=addedFiles.end() )
      return;
    addedFiles.insert(task.filename);
  }

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
  switch( type )
  {
  case TASK_FLT:
    flt_load_from_filename(filename.c_str(), of, opts);
    break;
  case TASK_IMG: break;
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

int main(int argc, const char** argv)
{
  fltThreadPool tp;
  tp.ctx.numThreads = std::thread::hardware_concurrency();
  tp.init();
  read_with_callbacks_mt("../../../data/camp/master.flt", &tp);
  //read_with_resolve("../../../data/camp/master.flt");
  tp.deinit();
  return 0;
}