/*
The MIT License (MIT)

Copyright (c) 2015 Manu Marin / @gyakoo

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
 */
#ifdef _MSC_VER
#pragma warning(disable:4244 4100 4996)
#include <windows.h>
#endif

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
#include <nvtt/nvtt.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define SGIRGB_IMPLEMENTATION
#include <flt_sgirgb.h>

#define F2D_DDS_Q_LOW       (1<<0)
#define F2D_DDS_Q_MED       (1<<1)
#define F2D_DDS_Q_HIGH      (1<<2)
#define F2D_DDS_GEN_MIPMAPS (1<<3)
#define F2D_DDS_VERBOSE     (1<<4)
#define F2D_DDS_FORCEOUT    (1<<5)
#define F2D_DDS_ALPHAFMT_AUTO (1<<6)
#define F2D_DDS_ALPHAFMT_BC1  (1<<7)
#define F2D_DDS_ALPHAFMT_BC2  (1<<8)
#define F2D_DDS_ALPHAFMT_BC3  (1<<9)

#define F2D_OP_EXTREF 63
#define F2D_OP_TEXTUREPAL 64
#define F2D_AVG_SAMPLES 16

#define F2D_MAX_SUPPORTED_THREADS 512
#define F2D_VERSION_STR "0.1"

// forward decls
struct f2dThreadPool;
struct f2dContext;
double f2dGetTime();
int f2dParseCmd(int argc, const char** argv, f2dContext* f2d);
int f2dClamp(int v, int m, int M);
bool f2dConvertToDDS(const char* inFile, const char* outFile, int flags);
bool f2dExtensionIs(const char* filename, const char* ext);
bool f2dExtensionIsImg(const char* filename);
bool f2dExtensionIsSGI(const char* filename);
void f2dChangeExtension(std::string& fname, const char* ext);
bool f2dExtractBaseName(const std::string& fname, std::string& outbase);
bool f2dExtractBasePath(const std::string& pathname, std::string& outbasepath);
bool f2dEndsWithSlash(const std::string& pathname);
int f2dPrintUsage(const char* pgr);
void f2dComputeAlphaVariance(unsigned char* data, unsigned int numpixels, int* outAValues, float* variance, float* mean);
void f2dExtractTasksFromRecursiveWildcard(f2dThreadPool* f2dTP, const std::string& path);
bool f2dFileExists(const char* filename);
const char* f2dDDSFormatToStr(nvtt::Format fmt);

// types
struct f2dThreadTask
{
  enum f2dTaskType { TASK_UNKNOWN=-1, TASK_FLT=0, TASK_IMG=1 };
  f2dTaskType type;
  std::string filename;
  int pathNdx; // if != -1, uses specific path for file

  f2dThreadTask():pathNdx(-1){}
  f2dThreadTask(f2dTaskType tt, const std::string& f, int pn):type(tt), filename(f), pathNdx(pn){}
  int runTask(f2dThreadPool* tp);
  FILE* TryOpen(const std::string& filename, f2dThreadPool* tp, std::string* outpath=0, bool retry=true);
  int parseFlt(const std::string& filename, f2dThreadPool* tp);
  int convertImg(const std::string& filename, f2dThreadPool* tp);
};

struct f2dContext
{
  f2dContext() : numThreads(0),ddsAlphaFmt(2), ddsQuality(0)
    , verbose(false), ddsForce(false), ddsGenMipmaps(false)
  {
  }
  std::set<std::string> fltFiles;    
  std::string wildcard; // if empty, no recurse wildcard
  int numThreads;
  int ddsQuality;
  int ddsAlphaFmt;
  bool verbose;
  bool ddsForce;
  bool ddsGenMipmaps;
};

struct f2dThreadPool
{
  void init()
  {
    deinit();
    finish=false;
    threads.reserve(ctx.numThreads);
    for(int i = 0; i < ctx.numThreads; ++i)
      threads.push_back(new std::thread(&f2dThreadPool::runcode,this));
    workingTasks=0;
  }

  void runcode()
  {
    f2dThreadTask task;
    while (!finish)
    {
      if ( getNextTask(&task) )
      {
        ++workingTasks;
        task.runTask(this);
        --workingTasks;
      }
      else
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
  }

  bool getNextTask(f2dThreadTask* t)
  {
    std::lock_guard<std::mutex> lock(mtxTasks);
    if ( tasks.empty() ) return false;
    *t = tasks.back();
    tasks.pop_back();
    return true;
  }

  void addNewTask(f2dThreadTask::f2dTaskType type, const std::string& filename, int spndx=-1)
  {
    // img not supported, don't create task
    if ( type==f2dThreadTask::TASK_IMG && ( f2dExtensionIs(filename.c_str(), ".dds") || !f2dExtensionIsImg(filename.c_str()) ) )
      return;

    // check if file is done already
    {
      std::lock_guard<std::mutex> lock(mtxFiles);
      if ( addedFiles.find(filename)!=addedFiles.end() )
        return;
      addedFiles.insert(filename);
    }

    // not done yet, process
    std::lock_guard<std::mutex> lock(mtxTasks);
    tasks.push_front( f2dThreadTask(type,filename,spndx) );
  }

  void deinit()
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

  f2dContext ctx;
  std::vector<std::string> searchpaths;
  std::vector<std::thread*> threads;
  std::deque<f2dThreadTask> tasks;
  std::set<std::string> addedFiles;
  std::mutex mtxTasks;
  std::mutex mtxFiles;
  std::atomic_int workingTasks;
  bool finish;
};

double f2dGetTime()
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

int f2dClamp(int v, int m, int M)
{
  return (v<m)? m : (v>M?M:v);
}

bool f2dExtractBaseName(const std::string& fname, std::string& outbase)
{
  bool r=false;
  outbase=fname;
  size_t p = fname.find_last_of('/');
  if ( p==std::string::npos ) p = fname.find_last_of('\\');
  if ( p!=std::string::npos ) { outbase = fname.substr(p+1,fname.length()); r=true; }
  return r;
}

bool f2dExtractBasePath(const std::string& pathname, std::string& outbasepath)
{
  outbasepath=pathname;  
  while ( !f2dEndsWithSlash(outbasepath) && !outbasepath.empty()) 
    outbasepath.pop_back();
  return !outbasepath.empty();
}

bool f2dEndsWithSlash(const std::string& pathname)
{
  return pathname.back()=='\\' || pathname.back()=='/';
}

int f2dPrintUsage(const char* pgr)
{
  std::string program;  
  f2dExtractBaseName(pgr,program);

  printf( "flt2dds: Converts imagery data from Openflight to DirectX DDS files\n\n" );
  printf( "version %s\n", F2D_VERSION_STR );
  printf( "Usage: $ %s <options> <file0> [<file1>...]\nOptions:\n", program.c_str());
  printf( "\t -?   : This help screen\n" );
  printf( "\t -v   : Enable verbose mode. default=disabled\n" );
  printf( "\t -f   : Force DDS write even though the target DDS file already exists. default=disabled\n" );
  printf( "\t -m   : Generate mip maps. default=no\n" );
  printf( "\t -q N : DDS out quality (0:low 1:medium 2:high). default=0\n" );
  printf( "\t -a N : Alpha format (0:Auto 1:BC1 2:BC2 3:BC3). Auto uses variance of image, slower. default=2:BC2\n");
  printf( "\t -t N : Force N threads. Values 0..%d (0:Max HW threads). default=0:Max HW threads\n", F2D_MAX_SUPPORTED_THREADS);
  printf( "\t -r w : Converts recursively all files matching wildcard 'w' using the input file paths.\n\n" );
  printf( "\t <file0>...: Files can be either any image file (rgb, png...) or FLT files where to look at\n");
  printf( "\t Supported image formats: rgb, rgba, sgi, jpg, jpeg, png, tga, bmp, psd, gif, hdr, pic, pnm\n" );
  printf( "\nExamples:\n" );
  printf( "\tConverts all images pointed recursively by master.flt with quality medium, forcing overwrite dds\n" );
  printf( "\t  $ %s -q 1 -f -v master.flt\n\n", program.c_str());
  printf( "\tConverts these three images\n" );
  printf( "\t  $ %s image1.png image2.jpg image3.rgba\n\n", program.c_str());
  printf( "\tConverts all rgb files recursively from directory c:\\flight\\ (note to add \\ at end)\n" );
  printf( "\t  $ %s -r *.rgb c:\\flight\\\n\n", program.c_str() );
  printf( "\tConverts images from these flt files\n" );
  printf( "\t  $ %s models/tank.flt models/plane.flt\n\n", program.c_str());
  return 0;
}

// read configuration from command line
int f2dParseCmd(int argc, const char** argv, f2dContext* f2d)
{
  int i;
  if ( argc<2 )
    return f2dPrintUsage(argv[0]);

  for (i=1;i<argc;++i)
  {
    if (argv[i][0]=='-' )
    {
      if ( argv[i][1]=='q' && argv[i][2]==0 )
        f2d->ddsQuality = (i+1)<argc ? atoi(argv[++i]) : 0;
      else if ( argv[i][1]=='a' && argv[i][2]==0 )
        f2d->ddsAlphaFmt = (i+1)<argc ? atoi(argv[++i]) : 2;
      else if ( argv[i][1]=='t' && argv[i][2]==0 )
        f2d->numThreads = (i+1)<argc ? atoi(argv[++i]) : 0;
      else if ( argv[i][1]=='r' && argv[i][2]==0 )
        f2d->wildcard = (i+1)<argc ? argv[++i] : 0;
      else if ( argv[i][1]=='v')
        f2d->verbose=true;
      else if ( argv[i][1]=='h' || argv[i][1]=='?')
        return f2dPrintUsage(argv[0]);
      else if ( argv[i][1]=='f' )
        f2d->ddsForce=true;
      else if ( argv[i][1]=='m' )
        f2d->ddsGenMipmaps=true;
    }
    else
    {
      f2d->fltFiles.insert( argv[i] );
    }
  }

  // checks
  f2d->ddsQuality = f2dClamp(f2d->ddsQuality, 0, 2);
  f2d->ddsAlphaFmt= f2dClamp(f2d->ddsAlphaFmt, 0, 3);
  f2d->numThreads = f2dClamp(f2d->numThreads, 0, F2D_MAX_SUPPORTED_THREADS);
  if ( f2d->numThreads==0 ) f2d->numThreads = std::thread::hardware_concurrency();
  return 1;
}

int main(int argc, const char** argv)
{
  f2dThreadPool f2dTP;  
  double startTime = f2dGetTime();

  // load the master.flt or the specified flt files + read configuration
  if ( !f2dParseCmd(argc,argv,&f2dTP.ctx) )
    return 1;

  // create thread pool
  f2dTP.init();

  // add base paths
  std::string basepath; 
  for each(auto& f in f2dTP.ctx.fltFiles)
    if ( f2dExtractBasePath(f,basepath) ) 
      f2dTP.searchpaths.push_back(basepath);

  // check if normal operation or wilcard mode
  if ( f2dTP.ctx.wildcard.empty() )
  {
    // no recursive wildcard mode, uses input files as first tasks
    for each(auto& f in f2dTP.ctx.fltFiles)
    {
      if ( f2dExtensionIs(f.c_str(),".flt") )   f2dTP.addNewTask(f2dThreadTask::TASK_FLT, f);
      else if ( f2dExtensionIsImg(f.c_str()) )  f2dTP.addNewTask(f2dThreadTask::TASK_IMG, f);
    }
  }
  else
  {
    std::string path; path.reserve(MAX_PATH);
    if ( f2dTP.searchpaths.empty() ) GetCurrentDirectoryA(MAX_PATH-1, &path[0]);
    else path=*f2dTP.searchpaths.begin();

    // recursive wildcard mode
    f2dExtractTasksFromRecursiveWildcard(&f2dTP, path);
  }

  // wait before processing in main thread
  int ct=0; // for big files at least shows something when verbose and stuck reading
  do
  {
    if ( !f2dTP.ctx.verbose )
    {
      printf ( "\r                                                 \r");
      const float pct = f2dTP.addedFiles.size() ? (float)f2dTP.tasks.size() / f2dTP.addedFiles.size()*100.0f : 0.0f;
      double td=(f2dGetTime()-startTime)/1000.0;
      int t=(int)td;
      double avg = td/(f2dTP.addedFiles.size()-f2dTP.tasks.size());
      int s=t%60, m=(t/60)%60, h=(t/3600);
      int eta=(int)(avg*f2dTP.tasks.size());
      int es=eta%60, em=(eta/60)%60, eh=(eta/3600);
      printf("%02d:%02d:%02d - (%.0f%%) %d - ETA: %02d:%02d:%02d", h,m,s,100.0f-pct, f2dTP.tasks.size(), eh,em,es );
    }
    {
      ct++;
      if (ct>2)
      {
        printf("Current tasks: %d\n", (int)f2dTP.workingTasks);
        ct=0;
      }
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));    
  }while ( !f2dTP.tasks.empty() || f2dTP.workingTasks>0 );

  // deinit (join threads)
  f2dTP.deinit();

  // some stats info
  {
    int t=(int)( (f2dGetTime()-startTime)/1000.0 );
    int s=t%60, m=(t/60)%60, h=(t/3600);
    printf( "\nDuration: %02d:%02d:%02d\n", h,m,s);

    int cflt=0, cimg=0;
    for each(auto& i in f2dTP.addedFiles)
    {
      if ( f2dExtensionIs(i.c_str(),".flt") ) ++cflt;
      else ++cimg;
    }
    printf( "%d flt files, %d image files\n", cflt, cimg);
  }
  return EXIT_SUCCESS;
}

int f2dThreadTask::runTask(f2dThreadPool* tp)
{
  int result=0;
  switch( type )
  {
  case TASK_FLT: result=parseFlt(this->filename.c_str(), tp); break;
  case TASK_IMG: result=convertImg(this->filename.c_str(), tp); break;
  }
  return result;
}

int f2dThreadTask::convertImg(const std::string& filename, f2dThreadPool* tp)
{
  std::string outpath;
  FILE* f=TryOpen(filename,tp,&outpath);
  if (f)
  {
    fclose(f);
    std::string basename=filename;
    if ( !outpath.empty() ) f2dExtractBaseName(filename,basename);
    std::string infname; infname.reserve(outpath.length()+basename.length());
    infname.append(outpath); infname.append(basename);
    std::string outfname(infname);
    f2dChangeExtension(outfname, ".dds");
    int flags = (1<<tp->ctx.ddsQuality);
    flags |= tp->ctx.verbose?F2D_DDS_VERBOSE:0;
    flags |= tp->ctx.ddsForce?F2D_DDS_FORCEOUT:0;
    flags |= 1<<(tp->ctx.ddsAlphaFmt+6);
    flags |= tp->ctx.ddsGenMipmaps ? F2D_DDS_GEN_MIPMAPS : 0;
    return f2dConvertToDDS(infname.c_str(), outfname.c_str(),flags) ? 1 : 0;
  }
  else
  {
    if ( tp->ctx.verbose )
      printf( "Could not open %s\n", filename.c_str() );
  }
  return 0;
}

// try to open from all available paths if not in the default
FILE* f2dThreadTask::TryOpen(const std::string& filename, f2dThreadPool* tp, std::string* outpath, bool retry/*=true*/)
{
  FILE* f=NULL;

  // if specific path, uses that
  if ( pathNdx>=0 && pathNdx < (int)tp->searchpaths.size() )
  {
    std::string specificFname;
    const std::string& spath = tp->searchpaths[pathNdx];
    specificFname.reserve(spath.size()+filename.size()+4);
    specificFname.append(spath);
    specificFname.append(filename);
    f = fopen(specificFname.c_str(), "rb");
    if (f) 
    {
      if (outpath) {f2dExtractBasePath(specificFname,*outpath);}
      return f;
    }
  }
  f = fopen(filename.c_str(), "rb");

  // check if couldn't open, try with search paths
  std::string tmpfname; 
  tmpfname.reserve(filename.length()+64);
  std::vector<std::string>::iterator itp = tp->searchpaths.begin();
  while ( !f && itp!=tp->searchpaths.end() )
  {
    tmpfname.append(*itp);
    if (itp->back()!='/' && itp->back()!='\\' ) 
      tmpfname.append("/");
    tmpfname.append(filename);
    f = fopen(tmpfname.c_str(), "rb");
    if ( f && outpath ) { f2dExtractBasePath(tmpfname,*outpath);}
    tmpfname.clear();
    ++itp;
  }

  // if it could't open it, try again with only the image name 
  if ( !f && retry )
  {
    std::string basename;
    if ( f2dExtractBaseName(filename,basename) )
      f = TryOpen(basename.c_str(),tp,outpath,false);
  }

  return f;
}

int f2dThreadTask::parseFlt(const std::string& filename, f2dThreadPool* tp)
{
  char extRefName[200];
  unsigned short recordHeader[2];
  FILE* f=NULL;

  if ( filename.empty() )
    return 0;

  f=TryOpen(filename,tp);

  if ( !f )
  {
    if ( tp->ctx.verbose )
      printf( "Could not open %s\n", filename.c_str() );
    return 0;
  }

  if ( tp->ctx.verbose )
    printf( "Parsing %s\n", filename.c_str() );
  while ( fread( &recordHeader, 2, 2, f) == 2 )
  {
    sgirgb_swap16(&recordHeader[0]);
    sgirgb_swap16(&recordHeader[1]);
    recordHeader[1]-=4;
    // only look for external references (other flt files) and texture palette files (image references)
    switch ( recordHeader[0] )
    {
    case F2D_OP_EXTREF:
      recordHeader[1] -= (unsigned short)fread( &extRefName,1, sizeof(extRefName), f);
      tp->addNewTask(TASK_FLT, extRefName);
      break;    

    case F2D_OP_TEXTUREPAL:
      {
        recordHeader[1] -= (unsigned short)fread( &extRefName,1, sizeof(extRefName), f);
        tp->addNewTask(TASK_IMG, extRefName);
      }break;
    }

    if ( recordHeader[1] > 0 )
      fseek(f, recordHeader[1], SEEK_CUR);
  }

  fclose(f);
  return 1;
}

bool f2dExtensionIs(const char* filename, const char* ext)
{
  const size_t len1=strlen(filename);
  const size_t len2=strlen(ext);
  const char* str=strstr(filename,ext);
  return ( str && int(str-filename+1)==int(len1-len2+1));
}

bool f2dExtensionIsImg(const char* filename)
{
  if ( f2dExtensionIsSGI(filename) ) 
    return true;
  static const char* supported[10]={".jpg", ".jpeg", ".png", ".tga", ".bmp", ".psd", ".gif", ".hdr", ".pic", ".pnm"};
  for (int i=0;i<10;++i)
  {
    if ( f2dExtensionIs(filename,supported[i]) )
      return true;
  }
  return false;
}

bool f2dExtensionIsSGI(const char* filename)
{
  return f2dExtensionIs(filename,".rgb") || f2dExtensionIs(filename,".rgba") || f2dExtensionIs(filename,".sgi");
}

void f2dChangeExtension(std::string& fname, const char* ext)
{
  size_t d = fname.find_last_of('.');
  if ( d != std::string::npos )
  {
    fname = fname.substr(0,d);
    if ( *ext != '.' ) fname+='.';
    fname.append(ext);
  }
}

bool f2dFileExists(const char* filename)
{
#if defined(_WIN32) || defined(_WIN64)
  return _access_s(filename,00)==0;
#else
  // use stat instead?
  FILE* ftmp=fopen(filename,"rb");
  if ( ftmp )
  {
    fclose(ftmp);
    return true;
  }
  return false;
#endif
}

// computes alpha channel variance of an image, first computes the histogram to check how many different alpha values are.
// assumes 4 channels in data, numpixels>0
void f2dComputeAlphaVariance(unsigned char* data, unsigned int numpixels, int* outAValues, float* variance, float* mean)
{
  unsigned int i;
  unsigned char alpha;
  unsigned int* ha;
  unsigned int ahisto[256]={0};
  float s;

  // histogram
  *outAValues=0;
  *variance=0.0f;
  *mean=0.0f;
  if (!numpixels ) return;

  unsigned char* alphaCh = data+3;
  for (i=0;i<numpixels;++i, alphaCh+=4)
  {
    alpha=*alphaCh;
    ha=ahisto+alpha;
    if (!*ha ){ *ha=1; ++*outAValues; } else { ++*ha; }
  }

  // if only 1 different value, return here
  if ( *outAValues == 1 ) return;

  // mean
  for (i=0;i<256;++i) *mean += i*ahisto[i];
  *mean /= numpixels;

  // std dev 
  *variance=0.0f;
  for (i=0;i<256;++i) 
  {
    s = (float)i - *mean;
    *variance += ahisto[i]*s*s;
  }
  *variance = sqrtf(*variance/numpixels);
}

const char* f2dDDSFormatToStr(nvtt::Format fmt)
{
  switch ( fmt )
  {
  case nvtt::Format_BC1: return "BC1";
  case nvtt::Format_BC2: return "BC2";
  case nvtt::Format_BC3: return "BC3";
  }
  return "Other";
}

// https://code.google.com/p/nvidia-texture-tools/wiki/ApiDocumentation
bool f2dConvertToDDS(const char* inFile, const char* outFile, int flags)
{
  int inX,inY,inC;
  unsigned char* inData=0;

  // when not forced, then we check if the target dds exists and exit if so
  if ( !(flags&F2D_DDS_FORCEOUT) )
  {
    if ( f2dFileExists(outFile) )
    {
      if (flags&F2D_DDS_VERBOSE)
        printf ( "%s exists\n", outFile );
      return true;
    }      
  }

  // We're going to expect 4 channels
  if ( f2dExtensionIsSGI(inFile) )
    inData = sgirgb_load(inFile,&inX, &inY, &inC); // rgb rgba sgi ((Automatically returns in BGRA format))
  else
  {
    inData = stbi_load(inFile, &inX, &inY, &inC, 4); // jpg png tga bmp psd gif hdr pic pnm
    if ( !inData )
    {
      if ( flags&F2D_DDS_VERBOSE ) printf( "%s\n", stbi_failure_reason() );
      inX = -1;
    }
    else
    {
      // stbi_load returns RGBA, converts to BGRA that expects nvtt by default (swapping r and b)
      unsigned char *rch = inData, *bch = inData+2, tmp;
      const unsigned int numpixels = inX*inY;
      for (unsigned int i = 0; i < numpixels; ++i, rch+=4, bch+=4)
      {
        tmp=*rch; *rch=*bch; *bch=tmp;
      }
    }
  }
  
  // could not load
  if ( !inData )
  {
    if (flags&F2D_DDS_VERBOSE)
      printf( "Could not load (err=%d): %s\n", inX, inFile);
    return false;
  }

  // nvtt DDS compression options
  nvtt::Quality q;
  if ( flags & F2D_DDS_Q_HIGH ) q=nvtt::Quality_Highest;
  else if (flags & F2D_DDS_Q_MED ) q=nvtt::Quality_Normal;
  else q=nvtt::Quality_Fastest;
  nvtt::InputOptions inputOptions;
  inputOptions.setTextureLayout(nvtt::TextureType_2D, inX, inY);
  inputOptions.setMipmapData(inData,inX,inY);
  inputOptions.setMipmapGeneration( (flags&F2D_DDS_GEN_MIPMAPS)!=0 );

  nvtt::OutputOptions outputOptions;
  outputOptions.setFileName(outFile);

  nvtt::CompressionOptions compressionOptions;
  bool hasAlpha = inC==4; // alpha if source image had 4 channels
  if ( inC == 3 ) compressionOptions.setPixelFormat(24, 0xff, 0xff00, 0xff0000, 0);
  compressionOptions.setQuality(q);

  nvtt::Format format=nvtt::Format_BC1;  
  // if it has alpha
  if ( hasAlpha )
  {
    format = nvtt::Format_BC2;
    if      (flags&F2D_DDS_ALPHAFMT_BC1 ) format = nvtt::Format_BC1; // forced bc1
    else if (flags&F2D_DDS_ALPHAFMT_BC2 ) format = nvtt::Format_BC2; // forced bc2
    else if (flags&F2D_DDS_ALPHAFMT_BC3 ) format = nvtt::Format_BC3; // forced bc3
    else if (flags&F2D_DDS_ALPHAFMT_AUTO)
    {
      int numAValues=0;
      float var,mean;
      f2dComputeAlphaVariance(inData,inX*inY,&numAValues, &var, &mean);
      if ( numAValues<=2 ) format = nvtt::Format_BC1;   // one or two single values
      else if ( var < 50.0f ) format = nvtt::Format_BC2;  // more alpha values, but not too much variance, use 4bit for alpha
      else format=nvtt::Format_BC3;
    }
  }
  compressionOptions.setFormat(format);
  
  // crashes when inC==3 (jpg for example) even when req_comp in stbi_load is 4 
  nvtt::Compressor compressor;
  compressor.enableCudaAcceleration(false);
  if (flags&F2D_DDS_VERBOSE)
    printf( "Compressing %s - %dx%d %d => %s\n", inFile, inX, inX, inC, f2dDDSFormatToStr(format) );

  stbi_image_free(inData);//don't needed anymore during conversion
  bool result = compressor.process(inputOptions, compressionOptions, outputOptions);

  return result;
}

void f2dExtractTasksFromRecursiveWildcard(f2dThreadPool* f2dTP, const std::string& path)
{
#if defined(_WIN32) || defined(_WIN64)
  char filterTxt[MAX_PATH];  
  sprintf_s( filterTxt, f2dEndsWithSlash(path) ? "%s%s" : "%s\\%s", path.c_str(), f2dTP->ctx.wildcard.c_str() );

  // files for this directory
  int spathndx=(int)f2dTP->searchpaths.size();
  f2dTP->searchpaths.push_back( f2dEndsWithSlash(path) ? path : (path+"\\") );
  WIN32_FIND_DATAA ffdata;
  HANDLE hFind = FindFirstFileA( filterTxt, &ffdata );
  if ( hFind != INVALID_HANDLE_VALUE )
  {
    do
    {
      if ( ffdata.dwFileAttributes & FILE_ATTRIBUTE_ARCHIVE )
      {
        f2dThreadTask::f2dTaskType tt = f2dThreadTask::TASK_UNKNOWN;
        if ( f2dExtensionIs(ffdata.cFileName, ".flt") ) tt=f2dThreadTask::TASK_FLT;
        else if (f2dExtensionIsImg(ffdata.cFileName) ) tt=f2dThreadTask::TASK_IMG;
        if ( tt != f2dThreadTask::TASK_UNKNOWN )
          f2dTP->addNewTask(tt, ffdata.cFileName,spathndx);
      }
    }while ( FindNextFile(hFind,&ffdata)!=0 );
    FindClose(hFind);
  }

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
            f2dExtractTasksFromRecursiveWildcard(f2dTP, filterTxt);
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