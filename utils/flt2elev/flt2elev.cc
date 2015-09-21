

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

struct fltThreadPool;
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
struct fltTileInfo
{
  fltTileInfo()
    : of(nullptr), heightdata(nullptr), tp(nullptr), fltnode(nullptr)
    , v1(nullptr), from1(nullptr), v2(nullptr), from2(nullptr)
    , mapping(nullptr)
  { }

  flt* of;
  uint8_t* heightdata;
  fltExtent xtent;
  double dim[3];
  int width, height;
  int rect[4]; // left,top,width,height
  fltThreadPool* tp;
  flt_node* fltnode;
  double *v1, *from1;
  double *v2, *from2;
  double step;
  const double* mapping;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct fltMosaic
{
  int mosaicCols;
  int mosaicRows;
  std::string mosaicWc;

  fltMosaic():mosaicCols(-1), mosaicRows(-1){}

  void getRowCol(const std::string& name, int* row, int* col)
  {
    size_t i=0,j=0;
    char tmp[64]={0};
    int nc,mc,nmc;
    int t;
    *row = *col = -1;
    for (;i<mosaicWc.length();++i,++j)
    {
      mc = tolower(mosaicWc[i]);
      nc = tolower(name[j]);
      if ( nc!=mc && mc!='$') return; // name and mosaicwc should match
      if ( mc=='$' )
      {
        if ( i >= mosaicWc.length()-1 ) return;
        mc=tolower(mosaicWc[++i]);
        if ( mc == 'r' || mc == 'c' )
        {
          nmc=mosaicWc[++i];
          t=0;
          while (j<name.length() && tolower(name[j])!=nmc) 
          {
            tmp[t++]=name[j];
            ++j;
          }
          tmp[t]=0;
          int* w = mc=='r' ? row : col;
          *w = atoi(tmp);
        }
      }
    }
  }

  void generateMosaic(const std::string& name, fltThreadPool* tp)
  {
    if ( mosaicWc.empty() )
      return;

    int widthpixels=-1, heightpixels = -1;
    // compute size for mosaic image
    {
      // get the biggest tile
      int maxwidth=-1, maxheight=-1;
      
      widthpixels = maxwidth*mosaicCols;
      heightpixels= maxheight*mosaicRows;
    }

    if ( widthpixels <= 0 || heightpixels <= 0 )
      return;



    // for each image tile in mosaicmap
    //    compute the row/colum out of mosaic wildcard and tile name
    //    blit the image into the mosaic row/colum
    //    deallocate tile
    //    
    // save the mosaic image into the destination format
  }

};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct fltThreadBaseTask
{
  fltThreadBaseTask(const std::string& fname):filename(fname)
  {
  }

  virtual void runTask(fltThreadPool* tp) = 0;

  std::string filename;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct fltTaskFlt2Elev : public fltThreadBaseTask
{
  fltTaskFlt2Elev(const std::string& f)
    : fltThreadBaseTask(f)
  {
  }
  
  virtual void runTask(fltThreadPool* tp);

  void doLoadExtent(fltThreadPool* tp);
  void computeExtent(flt* of, fltExtent* xtent, flt_node* node);
  void computeNodeExtent(flt* of, fltExtent* xtent, flt_node* node);
  void flt2elev(fltThreadPool* tp);
  void doAllocateSingleTile(fltThreadPool* tp);
  void flt2dem(fltThreadPool* tp, flt* of){ tp=0; of=0; throw "Not implemented"; }
  bool findSpecificNode(flt_node* node, const std::string& lodname, int nodetype, flt_node** outNode);
  void fillNodeTris(flt_node* node, fltTileInfo* p);
  void fillTri(double* v0, double* v1, double* v2, fltTileInfo* p);
  void fillTriStep(double t, fltTileInfo* p);
  void fillLineStep(double t, double* a, double* d, double* xyz);
  void mapXY(double _x, double _y, int* x, int *y, fltTileInfo* p);
  void mapPoint(double* xyz, fltTileInfo* p);
  double computeStep(double* v0, double *v1, double* v2, fltTileInfo* p);
  void writeFileFormat(int format, const char* finalname, int w, int h, int d, uint8_t* data);
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
    numtris = 0;
    numtrisbf = 0;
    threads.reserve(numthreads);
    for(int i = 0; i < numthreads; ++i)
      threads.push_back(new std::thread(&fltThreadPool::runcode,this));  
    forceDim[0]=forceDim[1]=-1;
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

  bool getTileInfo(const std::string& name, fltTileInfo& outTi)
  {
    std::lock_guard<std::mutex> lock(mtxTileInfos);
    auto it=tileinfos.find(name);
    if ( it != tileinfos.end() )
    {
      outTi = it->second;
      return true;
    }
    return false;
  }

  void addTileInfo(const std::string& fname, const fltTileInfo& ti)
  {
    std::lock_guard<std::mutex> lock(mtxTileInfos);
    tileinfos[fname] = ti;
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

  std::vector<std::thread*> threads;
  std::deque<fltThreadBaseTask*> tasks;
  std::mutex mtxTasks;
  std::map<std::string, fltTileInfo> tileinfos;
  std::mutex mtxTileInfos;
  std::atomic_int workingTasks;
  std::atomic_int numtris;
  std::atomic_int numtrisbf;
  std::string nodename;
  fltMosaic mosaic;
  int forceDim[2];
  int format;
  bool finish;
  int nodetype;
  int culling;
  int depth;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltTaskFlt2Elev::computeNodeExtent(flt* of, fltExtent* xtent, flt_node* node)
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
void fltTaskFlt2Elev::computeExtent(flt* of, fltExtent* xtent, flt_node* node)
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
void fltTaskFlt2Elev::doLoadExtent(fltThreadPool* tp)
{
  // compute extent
  flt_opts opts = {0};
  opts.pflags = FLT_OPT_PAL_VERTEX | FLT_OPT_PAL_VTX_POSITION;
  opts.hflags = FLT_OPT_HIE_ALL_NODES;
  opts.dfaces_size = 1543;
  flt* of = (flt*)calloc(1,sizeof(flt));
  if ( !of ) return;

  // load file
  flt_load_from_filename(filename.c_str(), of, &opts);

  // it
  if ( !of->hie )
  {
    fprintf( stderr, "No nodes loaded in flt: %s\n", filename.c_str() );
    return;
  }

  // look for specific node if user passed non-empty parameter
  flt_node* specificNode = of->hie->node_root;
  if ( !tp->nodename.empty() )
    findSpecificNode(of->hie->node_root, tp->nodename, tp->nodetype, &specificNode);

  if ( !specificNode )
  {
    fprintf( stderr, "Invalid node to look geometry. %s\n", filename.c_str() );
    return;
  }

  // compute dimension of image
  fltExtent xtent;
  computeExtent(of,&xtent,specificNode);
  double dim[3];
  xtent.getDimension(dim);
  const double AR = dim[0]/dim[1];
  const double IAR= dim[1]/dim[0];
  int WIDTH = tp->forceDim[0]<0 ? (int)dim[0] : tp->forceDim[0];
  int HEIGHT = tp->forceDim[1]<0 ? (int)dim[1] : tp->forceDim[1];
  if (WIDTH==0 && HEIGHT==0) WIDTH = (int)dim[0];
  if (WIDTH==0) WIDTH=(int)(AR*HEIGHT);
  if (HEIGHT==0) HEIGHT=(int)(IAR*WIDTH);
  const double maxz= (tp->depth==32) ? 4294967295.0 : (double)( (1<<tp->depth)-1 ); // 1<<32 not work fine
  const double MAPPING[3] = { 1.0/dim[0]*WIDTH, 1.0/dim[1]*HEIGHT, (dim[2]>maxz) ? (1.0/dim[2] * maxz) : (1.0) };
  
  // save tile info
  fltTileInfo p;
  p.of=of;
  p.xtent=xtent;
  p.dim[0]=dim[0]; p.dim[1]=dim[1]; p.dim[2]=dim[2];
  p.width = WIDTH;
  p.height = HEIGHT;
  p.mapping = MAPPING;
  p.tp = tp;
  p.fltnode = specificNode;
  tp->addTileInfo(filename,p);
}


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltTaskFlt2Elev::runTask(fltThreadPool* tp)
{
  doLoadExtent(tp);
  doAllocateSingleTile(tp);
  doReleaseFlt(tp);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool fltTaskFlt2Elev::findSpecificNode(flt_node* node, const std::string& lodname, int nodetype, flt_node** outNode)
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
double fltTaskFlt2Elev::computeStep(double* v0, double *v1, double* v2, fltTileInfo* p)
{
  double mins=DBL_MAX;
  for ( int i = 0; i < 2; ++i )
  {
    double x = i==0?v1[0]:v2[0];
    double y = i==0?v1[1]:v2[1];    
    int xy0[2]; mapXY(v0[0],v0[1], xy0, xy0+1, p);
    int xy1[2]; mapXY(x,y, xy1, xy1+1, p);
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
void fltTaskFlt2Elev::fillLineStep(double t, double* a, double* d, double* xyz)
{
  xyz[0]=a[0]+t*d[0];
  xyz[1]=a[1]+t*d[1];
  xyz[2]=a[2]+t*d[2];
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltTaskFlt2Elev::mapXY(double _x, double _y, int* x, int *y, fltTileInfo* p)
{
  *x = (int)( (_x - p->xtent.extent_min[0]) * p->mapping[0] ); *x = fltClamp(*x,0,p->width-1);
  *y = (int)( (_y - p->xtent.extent_min[1]) * p->mapping[1] ); *y = fltClamp(*y,0,p->height-1);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
template<typename T>
void mapPoint_t(double zval, double zmin, double mapping, int offs, T* data)
{
  T z, oldz;
  z = (T)(int)( (zval-zmin)*mapping);
  oldz = data[offs];
  if ( z > oldz ) data[offs] = z;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltTaskFlt2Elev::mapPoint(double* xyz, fltTileInfo* p)
{
  int x, y;
  int offs;
  mapXY(xyz[0], xyz[1], &x, &y, p);  
  offs = y*p->width+x;
  switch ( p->tp->depth )
  {
  case 8 :mapPoint_t<uint8_t>(xyz[2], p->xtent.extent_min[2], p->mapping[2], offs, (uint8_t*)p->heightdata); break;
  case 16:mapPoint_t<uint16_t>(xyz[2], p->xtent.extent_min[2], p->mapping[2], offs, (uint16_t*)p->heightdata); break;
  case 32:mapPoint_t<uint32_t>(xyz[2], p->xtent.extent_min[2], p->mapping[2], offs, (uint32_t*)p->heightdata); break;
  }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltTaskFlt2Elev::fillTriStep(double t, fltTileInfo* p)
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

//////////////////////////////////////////////////////////////////////////
// It's a very unoptimized way to raster a triangle.
// Use Bresenham algorithm instead, but adapted to get the Z of the original triangle
// to paint, so Bresenham 3D.
// Example: http://www.mathworks.com/matlabcentral/fileexchange/21057-3d-bresenhams-line-generation/content/bresenham_line3d.m
// Right now it computes the smallest step 
void fltTaskFlt2Elev::fillTri(double* v0, double* v1, double* v2, fltTileInfo* p)
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
  
  // goes from 0 to 1 to go filling the triangle form edge to edge
  double t = 0.0;
  while ( t <= 1.0 )
  { 
    fillTriStep(t, p);
    t += p->step;
  }
  fillTriStep(1.0, p);
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
#define cpoly_zcross(x0,y0, x1, y1, x2, y2) (((x1)-(x0))*((y2)-(y1)) - ((y1)-(y0))*((x2)-(x1)))
void fltTaskFlt2Elev::fillNodeTris(flt_node* node, fltTileInfo* p)
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
        cross = cpoly_zcross(v0[0],v0[1],v1[0],v1[1], v2[0], v2[1]);

        // fill this triangle
        const int c=p->tp->culling;
        if ( c == CULLING_NONE || 
           (c==CULLING_CW && cross < 0.0) || (c==CULLING_CCW && cross > 0.0) )
        {
          fillTri(v0,v1,v2,p);
          ++p->tp->numtris;
        }
        else
          ++p->tp->numtrisbf;
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
void fltTaskFlt2Elev::writeFileFormat(int format, const char* finalname, int w, int h, int d, uint8_t* data)
{
  switch ( format )
  {
  case FORMAT_PNG: stbi_write_png(finalname, w, h, 1, data, w); break;
  case FORMAT_RAW:
    {
      FILE* f = fopen(finalname, "wb");
      size_t total=w*h*(d>>3);
      size_t written=0;
      if ( f )
      {
        while (written < total)
          written += fwrite(data+written, 1, total-written, f);
        fclose(f);
      }
    }break;
  }
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltTaskFlt2Elev::doAllocateSingleTile(fltThreadPool* tp)
{
  fltTileInfo ti;
  while ( !tp->getTileInfo(filename, ti) )
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
  // allocate tile memory
  uint8_t* heightdata = (uint8_t*)calloc( 1, numpixels*(tp->depth/8) );
  if ( !heightdata ){ fprintf( stderr, "out of mem: %s\n", filename.c_str() ); return; }

  // go through all triangles under node  
  fillNodeTris(specificNode,&p);

  // saving tile
  std::string finalname(filename);
  if ( !tp->nodename.empty() ) finalname+="_"+tp->nodename;
  finalname+=fltFormatExt(tp->format);
  writeFileFormat(tp->format, finalname.c_str(), WIDTH, HEIGHT, tp->depth, heightdata);
  free(heightdata);
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
void doMosaicTilesProcess(fltThreadPool& tp, const std::vector<std::string>& files)
{

}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void doSingleTilesProcess(fltThreadPool& tp, const std::vector<std::string>& files)
{
  // one task per file
  for ( const std::string& f : files )
    tp.addNewTask(new fltTaskFlt2Elev(f));

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

  printf( "\n\nTime : %g secs\n", (fltGetTime()-t0)/1000.0);
  printf( "Triangles (ok/culled): %d/%d\n", tp.numtris, tp.numtrisbf);
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
  printf("\t -m <mosaic>  : Mosaic big texture. Specify the column $c and row $r to glue all images in one big single one\n" );
  printf("\nExamples:\n" );
  printf("\tConvert all geometry of the flt files, force 512x512 image\n" );
  printf("\t $ %s -d 512 512 -w flight*.flt\n\n", program);
  printf("\tConvert tile into a grayscale image, only geometry under LOD name \"l1\" is converted:\n" );
  printf("\t $ %s -f 0 -n l1 -o 73 flight1_1.flt\n\n", program);
  printf("\tConvert into raw format 32 bit forcing 512 width (height will be computed\n");
  printf("\t $ %s -f 2 -d 512 0 -z 32 flight1_1.flt\n\n", program);
  printf("\tCreates a big heightmap image with all of them\n");
  printf("\t $ %s -w flight?_?.flt -m flight$r_$c.flt\n\n", program);
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
  std::string mosaicWc;

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
        case 'm': if (i+1<argc) mosaicWc=argv[++i]; break;
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
    return;
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
  tp.format = format;
  tp.nodename = nodename;
  tp.nodetype = nodetype;
  tp.forceDim[0]=w;
  tp.forceDim[1]=h;
  tp.culling=culling;
  tp.depth = depth;
  tp.mosaic.mosaicWc = mosaicWc;

  // Single or Mosaic mode
  if ( mosaicWc.empty() )
    doSingleTilesProcess(tp, files);
  else
    doMosaicTilesProcess(tp, files);    

  // finishes thread pool
  tp.deinit();  
  return 0;
}