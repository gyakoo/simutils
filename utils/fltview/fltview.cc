
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
//#define FLT_NO_OPNAMES
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
void fltPrint(flt* of, int d, std::set<uint64_t>& done, int* tot_nodes);

int fltCallbackExtRef(flt_node_extref* extref, flt* of, void* userdata)
{
  char* finalname = flt_extref_prepare(extref,of);
  if ( finalname )
  {
    fltThreadTask task(fltThreadTask::TASK_FLT, finalname, extref->of, of->ctx->opts); //  read ref in parallel
    reinterpret_cast<fltThreadPool*>(userdata)->addNewTask(task);
    flt_safefree(finalname);
  }
  return 1;
}

void read_with_callbacks_mt(const char* filename)
{
  double t0=fltGetTime();
  flt_opts* opts=(flt_opts*)flt_calloc(1,sizeof(flt_opts));
  flt* of=(flt*)flt_calloc(1,sizeof(flt));

  fltThreadPool tp;
  tp.ctx.numThreads = std::thread::hardware_concurrency();
  tp.init();

//   fltu16 vtxstream[4]={ 0 };
//   vtxstream[0]=flt_vtx_stream_enc(FLT_VTX_POSITION , 0,12); 
//   vtxstream[1]=flt_vtx_stream_enc(FLT_VTX_COLOR    ,12, 4);
//   vtxstream[2]=flt_vtx_stream_enc(FLT_VTX_NORMAL   ,16,12);
//   vtxstream[3]=0;

  // configuring read options
  opts->palflags = FLT_OPT_PAL_VERTEX | FLT_OPT_PAL_VTX_SOURCE;
  opts->hieflags = FLT_OPT_HIE_HEADER | FLT_OPT_HIE_ALL;
  opts->cb_extref = fltCallbackExtRef;
  opts->cb_user_data = &tp;

  // master file task
  fltThreadTask task(fltThreadTask::TASK_FLT, filename, of, opts);
  tp.addNewTask(task);  

  // wait until cores finished
  do
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(75));    
  } while ( !tp.tasks.empty() || tp.workingTasks>0 );

  char tmp[256]; sprintf_s(tmp, "Time: %g secs", (fltGetTime()-t0)/1000.0);
  printf( "\n%s\n",tmp);
  printf("nfaces total : %d\n", TOTALNFACES);
  MessageBoxA(NULL,tmp,"continue",MB_OK);
  flt_release(of);
  flt_safefree(of);
  flt_safefree(opts);

  tp.deinit();
}

void print_hie(const char* filename)
{
  double t0;
  flt_opts* opts=(flt_opts*)flt_calloc(1,sizeof(flt_opts));
  flt* of=(flt*)flt_calloc(1,sizeof(flt));

  // actual read
  opts->palflags |= FLT_OPT_PAL_ALL;
  opts->hieflags |= /*FLT_OPT_HIE_RESERVED |  FLT_OPT_HIE_NO_NAMES|*/FLT_OPT_HIE_EXTREF_RESOLVE|FLT_OPT_HIE_ALL;
  t0=fltGetTime(); 

  flt_load_from_filename(filename,of,opts);

  std::set<uint64_t> done;
  int counterctx=0;
  fltPrint(of,0,done, &counterctx);

  printf("maxvlist: %d\n", maxvlist );
  printf("nfaces total : %d\n", TOTALNFACES);
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
  opts->hieflags = /*FLT_OPT_HIE_HEADER | */ FLT_OPT_HIE_ALL | FLT_OPT_HIE_EXTREF_RESOLVE;

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
  // img not supported, don't create task
  if ( task.type==fltThreadTask::TASK_IMG && !fltExtensionIsImg(task.filename.c_str()) )
    return;

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
  case TASK_FLT: flt_load_from_filename(filename.c_str(), of, opts); break;
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

void fltIndent(int d){ for ( int i = 0; i < d; ++i ) printf( "  " ); }
void fltPrintNode(flt_node* n, int d)
{
  static const char* names[]={"BASE","EXTREF","GROUP","OBJECT","MESH","LOD"};
  if ( !n ) return;

  // my siblings and children
  do
  {
    fltIndent(d); printf( "(%s) %s (%d/%d) %s\n", names[n->type], n->name?n->name:"", n->nfaces, n->nindices, n->nindices/3!=n->nfaces ? "**":"" );
    fltPrintNode(n->child_head,d+1);
    n = n->next;
  }while(n);
}

void fltPrint(flt* of, int d, std::set<uint64_t>& done, int* tot_nodes)
{
  // header
  printf( "\n" );
  fltIndent(d); printf( "%s\n", of->filename);
  fltIndent(d); printf( "Records: %d\n", of->ctx->rec_count );
  if ( of->hie && of->hie->node_count) 
  { 
    fltIndent(d); printf( "Nodes: %d\n", of->hie->node_count); 
    *tot_nodes += of->hie->node_count; 
  }

  // palettes
  if ( of->pal )
  {
    if(of->pal->vtx_count){ fltIndent(d); printf( "Vertices: %d\n", of->pal->vtx_count);}
    if(of->pal->tex_count)
    { 
      fltIndent(d); printf( "Textures: %d\n", of->pal->tex_count);
      flt_pal_tex* tex = of->pal->tex_head;
      while (tex)
      {
        fltIndent(d+1); printf( "%s\n", tex->name );
        tex = tex->next;
      } 
    }
  }
  
  // hierarchy and recursive print
  if ( of->hie )
  {
    // hierarchy
    fltPrintNode(of->hie->node_root,d);

    // flat list of extrefs
    if ( of->hie->extref_count)
    {
      fltIndent(d); printf( "Ext.References: %d\n", of->hie->extref_count );
      flt_node_extref* extref = of->hie->extref_head;
      while ( extref )
      {
        if ( done.find( (uint64_t)extref->of ) == done.end() )
        {
          fltPrint(extref->of, d+1, done,tot_nodes);
          done.insert( (uint64_t)extref->of );
        }
        extref= (flt_node_extref*)extref->next_extref;
      }
    }
  }
}

int main(int argc, const char** argv)
{
  print_hie("../../../data/camp/master.flt");
  //read_with_callbacks_mt("../../../data/utah/master.flt");
  //read_with_callbacks_mt("../../../data/camp/master.flt");
  //read_with_resolve("../../../data/camp/master.flt");  

  //testdict("../../../data/words.txt");
  
  return 0;
}


// void testdict(const char* fname)
// {
//   char line[512];
//   std::vector<std::string> words; words.reserve(30000);
//   FILE* f = fopen(fname, "rt");
//   if ( !f ) return;
//   while (fgets(line,512,f))
//     words.push_back(line);
//   fclose(f);
//   const int niters=300;
// 
//   double t0 = fltGetTime();
//   std::vector<std::string>::iterator it;
//   for (int j=0;j<niters;++j)
//   {
//     std::map<std::string,void*> stdhash;
//     size_t i=0;
//     for ( it=words.begin(); it != words.end(); ++it, ++i)
//       stdhash[*it] = (void*)i;
//   }
//   printf( "stdhash: %g sec\n", (fltGetTime()-t0)/niters/1000.0);
// 
//   t0 = fltGetTime();
//   for (int j=0;j<niters;++j)
//   {
//     flt_dict* dict;
//     flt_dict_create(49157,0,&dict);
//     size_t i=0;
//     for ( it=words.begin(); it != words.end(); ++it, ++i)
//       flt_dict_insert(dict,it->c_str(),(void*)i);
//     flt_dict_destroy(&dict);
//   }
//   printf( "flt_dict: %g sec\n", (fltGetTime()-t0)/niters/1000.0);
// }
// 