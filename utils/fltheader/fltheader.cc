
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

struct fltThreadPool
{
  void init();
  void runcode();
  bool getNextTask(fltThreadTask* t);
  void addNewTask(const fltThreadTask& task);
  void deinit();
  bool isWorking(){ return !tasks.empty() || workingTasks>0; }

  std::vector<std::thread*> threads;
  std::deque<fltThreadTask> tasks;
  std::mutex mtxTasks;
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
  case TASK_FLT    : flt_load_from_filename(filename.c_str(), of, opts); break;
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

void print_header(flt* of, flt_header* cmp)
{
  flt_header* h = of->header;
  if ( !h ) return;
  for ( int i = 0; i < 32; ++i ) 
    if ( h->dtime[i]=='\r' || h->dtime[i]=='\n' )  
      h->dtime[i]=' ';

  printf( "\n%s%s\n", of->filename, cmp ? " (diff)" : "" );
  if ( !cmp || cmp && strcmp(cmp->ascii, h->ascii)!= 0 )            printf( "\tAscii            : %s\n", h->ascii);
  if ( !cmp || cmp && cmp->format_rev!=h->format_rev )              printf( "\tFormat Rev.      : %d\n", h->format_rev);
  if ( !cmp || cmp && cmp->edit_rev!=h->edit_rev)                   printf( "\tEdit Rev.        : %d\n", h->edit_rev);
  if ( !cmp || cmp && strcmp(cmp->dtime, h->dtime)!= 0 )            printf( "\tDateTime         : %s\n", h->dtime);
  if ( !cmp || cmp && cmp->unit_mult!=h->unit_mult )                printf( "\tUnit Multiplier  : %d\n", h->unit_mult);
  if ( !cmp || cmp && cmp->v_coords_units!=h->v_coords_units)       printf( "\tVertex Units     : %s\n", flt_get_vcoords_units_name(h->unit_mult));
  if ( !cmp || cmp && cmp->flags!=h->flags)                         printf( "\tFlags            : %s\n", flt_get_header_flags_name(h->flags));
  if ( !cmp || cmp && cmp->proj_type!=h->proj_type)                 printf( "\tProjection Type  : %s\n", flt_get_proj_name(h->proj_type) );
  if ( !cmp || cmp && cmp->db_orig!=h->db_orig)                     printf( "\tDatabase Origin  : %s\n", flt_get_db_origin_name(h->db_orig) );
  if ( !cmp || cmp && cmp->orig_lat!=h->orig_lat)                   printf( "\tOrigin Lat       : %g\n", h->orig_lat );
  if ( !cmp || cmp && cmp->orig_lon!=h->orig_lon)                   printf( "\tOrigin Lon       : %g\n", h->orig_lon );
  if ( !cmp || cmp && cmp->sw_db_x!=h->sw_db_x)                     printf( "\tSouthWest DB X   : %g\n", h->sw_db_x );
  if ( !cmp || cmp && cmp->sw_db_y!=h->sw_db_y)                     printf( "\tSouthWest DB Y   : %g\n", h->sw_db_y );
  if ( !cmp || cmp && cmp->d_db_x!=h->d_db_x)                       printf( "\tDelta X          : %g\n", h->d_db_x );
  if ( !cmp || cmp && cmp->d_db_y!=h->d_db_y)                       printf( "\tDelta Y          : %g\n", h->d_db_y );
  if ( !cmp || cmp && cmp->d_db_z!=h->d_db_z)                       printf( "\tDelta Z          : %g\n", h->d_db_z );
  if ( !cmp || cmp && cmp->sw_corner_lat!=h->sw_corner_lat)         printf( "\tSouthWest C.  Lat: %g\n", h->sw_corner_lat);
  if ( !cmp || cmp && cmp->sw_corner_lon!=h->sw_corner_lon)         printf( "\tSouthWest C.  Lon: %g\n", h->sw_corner_lon);
  if ( !cmp || cmp && cmp->ne_corner_lat!=h->ne_corner_lat)         printf( "\tNorthEast C.  Lat: %g\n", h->ne_corner_lat);
  if ( !cmp || cmp && cmp->ne_corner_lon!=h->ne_corner_lon)         printf( "\tNorthEast C.  Lon: %g\n", h->ne_corner_lon);
  if ( !cmp || cmp && cmp->lbt_upp_lat!=h->lbt_upp_lat)             printf( "\tLambert Upper Lat: %g\n", h->lbt_upp_lat);
  if ( !cmp || cmp && cmp->lbt_low_lat!=h->lbt_low_lat)             printf( "\tLambert Lower Lat: %g\n", h->lbt_low_lat);
  if ( !cmp || cmp && cmp->utm_zone!=h->utm_zone )                  printf( "\tUTM Zone         : %d\n", h->utm_zone);
  if ( !cmp || cmp && cmp->radius!=h->radius)                       printf( "\tRadius           : %g\n", h->radius );
  if ( !cmp || cmp && cmp->earth_ellip_model!=h->earth_ellip_model) printf( "\tEarth Ellipsoid M: %s\n", flt_get_earth_ellip_name(h->earth_ellip_model) );  
  if ( !cmp || cmp && cmp->earth_major_axis!=h->earth_major_axis)   printf( "\tEarth Major Axis : %g\n", h->earth_major_axis);
  if ( !cmp || cmp && cmp->earth_minor_axis!=h->earth_minor_axis)   printf( "\tEarth Minor Axis : %g\n", h->earth_minor_axis);

  if ( of->hie )
  {
    flt_node_extref* xref = of->hie->extref_head;
    while (xref)
    {
      print_header(xref->of, cmp ? cmp : h);
      xref = xref->next_extref;
    }
  }
}

void read_with_callbacks_mt(const std::vector<std::string>& files, bool recurse)
{
  fltThreadPool tp;
  tp.numThreads = std::thread::hardware_concurrency()*2;
  tp.init();
    
  // configuring read options
  flt_opts* opts=(flt_opts*)flt_calloc(1,sizeof(flt_opts));
  flt* of=(flt*)flt_calloc(1,sizeof(flt));
  opts->pflags = 0;
  opts->hflags = FLT_OPT_HIE_HEADER | (recurse ? FLT_OPT_HIE_EXTREF : 0 );
  opts->dfaces_size = 3079;//1543;
  opts->cb_extref = recurse ? fltCallbackExtRef : FLT_NULL;
  opts->cb_user_data = &tp;

  // master file task
  tp.nfiles=1;
  fltThreadTask task(fltThreadTask::TASK_FLT, files[0].c_str(), of, opts);
  tp.addNewTask(task);  

  // wait until cores finished
  //double t0=fltGetTime();
  do
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));    
  } while ( tp.isWorking() );

  print_header(of, NULL);
  
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
  std::vector<std::string> files;
  for ( int i = 1; i < argc; ++i )
  {
    if ( argv[i][0]=='-' )
    {
      switch ( argv[i][1] )
      {
        case 'r': recurse = true; break;
        default : printf ( "Unknown option -%c\n", argv[i][1]); 
      }
    }
    else
      files.push_back( argv[i] );
  }

  if ( !files.empty() )
  {
    read_with_callbacks_mt(files,recurse);
  }
  else
  {
    char* program=flt_path_basefile(argv[0]);
    printf("%s: Prints the header of a FLT file\n\n", program );    
    printf("Usage: $ %s <options> <flt_files> \nOptions:\n", program );    
    printf("\t -r   : Recursive. Resolve all external references recursively and print the distinct information\n");
    printf("\nExamples:\n" );
    printf("\tHeader of the master:\n" );
    printf("\t  $ %s master.flt\n\n", program);    
    printf("\tHeader of the master and different info from children:\n" );
    printf("\t  $ %s -r master.flt\n\n", program );
    flt_free(program);
  }
  
  return 0;
}