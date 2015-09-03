
#pragma warning(disable:4100 4005)

#if defined(_DEBUG) && !defined(_WIN64)
#include <vld.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#define FLT_NO_OPNAMES
#define FLT_LEAN_FACES
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
void fltExtractTasksFromRecursiveWildcard(fltThreadPool* tp, const std::string& path, bool recurse);

struct fltThreadTask
{
  enum fltTaskType { TASK_NONE=-1, TASK_FLT=0};
  fltTaskType type;

  std::string filename;
  std::string basename;
  flt* of;
  flt_opts* opts;

  fltThreadTask():type(TASK_NONE), of(NULL), opts(NULL){}
  fltThreadTask(fltTaskType tt, const std::string& f, flt* of=0, flt_opts* opts=0)
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

  //std::vector<std::string> searchpaths;
  std::vector<int> opcodes;
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
    case TASK_FLT: 
    {
      flt_load_from_filename(filename.c_str(), of, opts); 

      // check the opcodes
      bool allok=true;
      for ( size_t i = 0; i < tp->opcodes.size(); ++i )
      {
        if ( opts->countable[tp->opcodes[i]] == 0 )
        {
          allok = false;
          break;
        }
      }
      // releasing memory
      flt_release(of);
      flt_safefree(of);
      flt_safefree(opts->countable);
      flt_safefree(opts);

      if ( allok )
        printf( "%s\n", filename.c_str() );

    }break;
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

void read_with_callbacks_mt(bool recurse, const std::vector<int>& opcodes, const std::string& dir)
{
  fltThreadPool tp;
  tp.numThreads = std::thread::hardware_concurrency()*2;
  tp.init();
  tp.opcodes = opcodes;

  double t0=fltGetTime();
  fltExtractTasksFromRecursiveWildcard(&tp,dir,recurse);

  // wait until cores finished
  do
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));    
  } while ( tp.isWorking() );

  printf( "Time: %g secs.\n", (fltGetTime()-t0)/1000.0);
  tp.deinit();
}

int print_usage(const char* p)
{
  char* program=flt_path_basefile(p);
  printf("%s: Find Openflight files with specific attributes\n\n", program);
  printf("Usage: $ %s <options> \nOptions:\n", program );    
  printf("\t -r          : Recursive search\n" );
  printf("\t -o 0,1,...  : Comma separated values of opcodes to look for it. Only if the file contains all of them, will show\n" );  
  printf("\t -d dir      : Starting directory where to look at\n" );
  printf("\nExamples:\n" );
  printf("\tFind all FLT files with LOD and MESH records.\n" );
  printf("\t  $ %s -r -d D:\\flightdata\\ -o 73,84\n\n", program);
  flt_free(program);
  return -1;
}

void addop(std::vector<int>& v, const char* ops)
{
  int op = atoi(ops);
  if ( op >= FLT_OP_HEADER && op <= FLT_OP_MAX)
    v.push_back(op);
}

int main(int argc, const char** argv)
{
  bool recurse=false;
  std::vector<int> opcodes; opcodes.reserve(8);
  std::string dir;
  char c;
  for ( int i = 1; i < argc; ++i )
  {
    if ( argv[i][0]=='-' )
    {
      c = argv[i][1];
      switch ( c)
      {
      case 'r': recurse = true;break;
      case 'd': if ( i+1<argc ) dir = argv[++i];break;
      case 'o': 
        {
          if ( i+1<argc )
          {
            char* originalstr = strdup(argv[++i]);
            char* str = originalstr;
            const char* last=str;
            while (*str)
            {
              if ( *str==',')
              {
                addop(opcodes, last);
                *str=0;
                last=str+1;
              }
              ++str;
            }
            if ( last != str )
              addop(opcodes,last);
            free(originalstr);
          }
        }break;
      case '?': return print_usage(argv[0]);
      }
    }    
  }

  if ( opcodes.empty() )
  {
    printf( "No opcode selected\n\n");
    return print_usage(argv[0]);
  }
  else
  {
    read_with_callbacks_mt(recurse, opcodes, dir);  
  }
  return 0;
}

bool fltEndsWithSlash(const std::string& pathname)
{
  return pathname.back()=='\\' || pathname.back()=='/';
}

bool fltExtensionIs(const char* filename, const char* ext)
{
  const size_t len1=strlen(filename);
  const size_t len2=strlen(ext);
  const char* str=strstr(filename,ext);
  return ( str && int(str-filename+1)==int(len1-len2+1));
}

void fltExtractTasksFromRecursiveWildcard(fltThreadPool* tp, const std::string& path, bool recurse)
{
#if defined(_WIN32) || defined(_WIN64)
  char filterTxt[MAX_PATH];  
  sprintf_s( filterTxt, fltEndsWithSlash(path) ? "%s%s" : "%s\\%s", path.c_str(), "*.flt" );

  // files for this directory
  //int spathndx=(int)tp->searchpaths.size();
  //tp->searchpaths.push_back( fltEndsWithSlash(path) ? path : (path+"\\") );
  WIN32_FIND_DATAA ffdata;
  HANDLE hFind = FindFirstFileA( filterTxt, &ffdata );
  if ( hFind != INVALID_HANDLE_VALUE )
  {
    do
    {
      if ( ffdata.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE )
      {
        if ( fltExtensionIs(ffdata.cFileName, ".flt") )
        {
          // configuring read options
          flt_opts* opts=(flt_opts*)flt_calloc(1,sizeof(flt_opts));
          flt* of=(flt*)flt_calloc(1,sizeof(flt));
          opts->pflags = FLT_OPT_PAL_ALL;
          opts->hflags = FLT_OPT_HIE_ALL_NODES | FLT_OPT_HIE_HEADER | FLT_OPT_HIE_GO_THROUGH;
          opts->cb_user_data = &tp;
          opts->countable = (fltatom32*)flt_calloc(1, sizeof(fltatom32)*FLT_OP_MAX);

          // add task for this file
          ++tp->nfiles;
          fltThreadTask task(fltThreadTask::TASK_FLT, ffdata.cFileName, of, opts);
          tp->addNewTask(task);  
        }
      }
    }while ( FindNextFile(hFind,&ffdata)!=0 );
    FindClose(hFind);
  }

  if ( !recurse ) 
    return;

  // recursively all subdirectories
  sprintf_s(filterTxt, "%s\\*", path.c_str() );
  hFind = FindFirstFileA(filterTxt, &ffdata);
  if ( hFind != INVALID_HANDLE_VALUE )
  {
    do
    {
      if ( ffdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY )
      {
        if ( strncmp( ffdata.cFileName, ".", 1) && strncmp(ffdata.cFileName,"..",2) )
        {
          sprintf_s(filterTxt, "%s\\%s", path.c_str(), ffdata.cFileName);
          if ( _access_s(filterTxt,06) == 0 )
          {
            fltExtractTasksFromRecursiveWildcard(tp, filterTxt, recurse);
          }
        }
      }
    }while ( FindNextFile(hFind,&ffdata)!=0 );
    FindClose(hFind);
  }

#else
#error "Implement this function for other platforms"
#endif
}
