

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
struct fltThreadTask;
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
struct fltNodeFillParam
{
  flt* of;
  uint8_t* heightdata;
  fltExtent xtent;
  double dim[3];
  int width, height;
  fltThreadPool* tp;

  double *v1, *from1;
  double *v2, *from2;
  double step;
  const double* mapping;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
std::map<std::string, fltNodeFillParam> mosaicmap;
std::mutex mtxMosaic;
int mosaicCols=-1;
int mosaicRows=-1;
void addMosaicInfo(const std::string& fname, const fltNodeFillParam& info)
{
  std::lock_guard<std::mutex> lock(mtxMosaic);
  mosaicmap[fname]=info;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
struct fltThreadTask
{
  enum f2dTaskType { TASK_NONE=-1, TASK_FLT2ELEV=0};
  f2dTaskType type;
  std::string filename;

  fltThreadTask():type(TASK_NONE){}
  fltThreadTask(f2dTaskType tt, const std::string& f)
    :type(tt), filename(f){}
  
  void runTask(fltThreadPool* tp)
  {
    tp=tp;
    switch( type )
    {
    case TASK_FLT2ELEV: flt2elev(tp); break;
    }
  }

  void computeExtent(flt* of, fltExtent* xtent, flt_node* node);
  void computeNodeExtent(flt* of, fltExtent* xtent, flt_node* node);
  void flt2elev(fltThreadPool* tp);
  void flt2png(fltThreadPool* tp, flt* of);
  void flt2height(fltThreadPool* tp, flt* of);
  void flt2dem(fltThreadPool* tp, flt* of){ tp=0; of=0; throw "Not implemented"; }
  bool findSpecificNode(flt_node* node, const std::string& lodname, int nodetype, flt_node** outNode);
  void fillNodeTris(flt_node* node, fltNodeFillParam* p);
  void fillTri(double* v0, double* v1, double* v2, fltNodeFillParam* p);
  void fillTriStep(double t, fltNodeFillParam* p);
  void fillLineStep(double t, double* a, double* d, double* xyz);
  void mapXY(double _x, double _y, int* x, int *y, fltNodeFillParam* p);
  void mapPoint(double* xyz, fltNodeFillParam* p);
  double computeStep(double* v0, double *v1, double* v2, fltNodeFillParam* p);
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
    res[0]=res[1]=-1;
  }

  void runcode()
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

  bool getNextTask(fltThreadTask* t)
  {
    std::lock_guard<std::mutex> lock(mtxTasks);
    if ( tasks.empty() ) return false;
    *t = tasks.back();
    tasks.pop_back();
    return true;
  }

  void addNewTask(const fltThreadTask& task)
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
    tasks.clear();
  }

  bool isWorking(){ return !tasks.empty() || workingTasks>0; }

  std::vector<std::thread*> threads;
  std::deque<fltThreadTask> tasks;    
  std::mutex mtxTasks;
  std::atomic_int workingTasks;
  std::atomic_int numtris;
  std::atomic_int numtrisbf;
  std::string nodename;
  int res[2];
  volatile int format;
  bool finish;
  int nodetype;
  int culling;
  int depth;
  std::string mosaic;
};

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void fltThreadTask::computeNodeExtent(flt* of, fltExtent* xtent, flt_node* node)
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

void fltThreadTask::computeExtent(flt* of, fltExtent* xtent, flt_node* node)
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

void fltThreadTask::flt2elev(fltThreadPool* tp)
{
  // compute extent
  flt_opts opts = {0};
  opts.pflags = FLT_OPT_PAL_VERTEX | FLT_OPT_PAL_VTX_POSITION;
  opts.hflags = FLT_OPT_HIE_ALL_NODES;
  opts.dfaces_size = 1543;
  flt of = {0};

  // load file
  flt_load_from_filename(filename.c_str(), &of, &opts);

  switch ( tp->format )
  {
  case FORMAT_DEM: flt2dem(tp,&of); break;
  
  case FORMAT_PNG: 
  case FORMAT_RAW: flt2height(tp,&of); break;
  }

  flt_release(&of);
}

bool fltThreadTask::findSpecificNode(flt_node* node, const std::string& lodname, int nodetype, flt_node** outNode)
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

double fltThreadTask::computeStep(double* v0, double *v1, double* v2, fltNodeFillParam* p)
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

void fltThreadTask::fillLineStep(double t, double* a, double* d, double* xyz)
{
  xyz[0]=a[0]+t*d[0];
  xyz[1]=a[1]+t*d[1];
  xyz[2]=a[2]+t*d[2];
}

void fltThreadTask::mapXY(double _x, double _y, int* x, int *y, fltNodeFillParam* p)
{
  *x = (int)( (_x - p->xtent.extent_min[0]) * p->mapping[0] ); *x = fltClamp(*x,0,p->width-1);
  *y = (int)( (_y - p->xtent.extent_min[1]) * p->mapping[1] ); *y = fltClamp(*y,0,p->height-1);
}

template<typename T>
void mapPoint_t(double zval, double zmin, double mapping, int offs, T* data)
{
  T z, oldz;
  z = (T)(int)( (zval-zmin)*mapping);
  oldz = data[offs];
  if ( z > oldz ) data[offs] = z;
}

void fltThreadTask::mapPoint(double* xyz, fltNodeFillParam* p)
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

void fltThreadTask::fillTriStep(double t, fltNodeFillParam* p)
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

// It's a very unoptimized way to raster a triangle.
// Use Bresenham algorithm instead, but adapted to get the Z of the original triangle
// to paint, so Bresenham 3D.
// Example: http://www.mathworks.com/matlabcentral/fileexchange/21057-3d-bresenhams-line-generation/content/bresenham_line3d.m
// Right now it computes the smallest step 
void fltThreadTask::fillTri(double* v0, double* v1, double* v2, fltNodeFillParam* p)
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

#define cpoly_zcross(x0,y0, x1, y1, x2, y2) (((x1)-(x0))*((y2)-(y1)) - ((y1)-(y0))*((x2)-(x1)))
void fltThreadTask::fillNodeTris(flt_node* node, fltNodeFillParam* p)
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

const char* fltFormatExt(int fmt)
{
  switch ( fmt )
  {
    case FORMAT_RAW: return ".raw";
    case FORMAT_PNG: return ".png";
  }
  return ".heightmap";
}

void fltWriteFormat(int format, const char* finalname, int w, int h, int d, uint8_t* data)
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

void fltThreadTask::flt2height(fltThreadPool* tp, flt* of)
{
  if ( !of->hie )
  {
    fprintf( stderr, "No nodes loaded in flt: %s\n", filename.c_str() );
    return;
  }

  // look for specific node if user passed non-empty parameter
  flt_node* specificNode = of->hie->node_root;
  if ( !tp->nodename.empty() )
    findSpecificNode(of->hie->node_root, tp->nodename, tp->nodetype, &specificNode);

  fltExtent xtent;
  computeExtent(of,&xtent,specificNode);

  // grayscale, one byte per pixel
  double dim[3];
  xtent.getDimension(dim);
  const double AR = dim[0]/dim[1];
  const double IAR= dim[1]/dim[0];
  int WIDTH = tp->res[0]<0 ? (int)dim[0] : tp->res[0];
  int HEIGHT = tp->res[1]<0 ? (int)dim[1] : tp->res[1];
  if (WIDTH==0 && HEIGHT==0) WIDTH = (int)dim[0];
  if (WIDTH==0) WIDTH=(int)(AR*HEIGHT);
  if (HEIGHT==0) HEIGHT=(int)(IAR*WIDTH);
  double maxz=255.0;
  switch(tp->depth) // because 1<<depth does not work in some archs for depth=32
  {
  case 8: maxz=255.0; break;
  case 16: maxz=65535.0; break;
  case 32: maxz=4294967295.0; break;
  }
  double MAPPING[3] = { 1.0/dim[0]*WIDTH, 1.0/dim[1]*HEIGHT, 1.0 };
  if ( dim[2] > maxz )
    MAPPING[2]=1.0/dim[2] * maxz;
  const int numpixels = WIDTH*HEIGHT;
  if ( numpixels<=0 )
  {
    fprintf(stderr, "Num pixels invalid for: %s. dim=%g %g %g\n", filename.c_str(), dim[0], dim[1], dim[2]);
    return;
  }

  uint8_t* heightdata = (uint8_t*)calloc( 1, numpixels*(tp->depth/8) );
  if ( !heightdata ){ fprintf( stderr, "out of mem: %s\n", filename.c_str() ); return; }

  if ( !specificNode )
  {
    fprintf( stderr, "Invalid node to look geometry. %s\n", filename.c_str() );
    return;
  }

  // go through all triangles under node
  fltNodeFillParam p;
  p.of=of;
  p.heightdata=heightdata;
  p.xtent=xtent;
  p.dim[0]=dim[0]; p.dim[1]=dim[1]; p.dim[2]=dim[2];
  p.width = WIDTH;
  p.height = HEIGHT;
  p.mapping = MAPPING;
  p.tp = tp;
  fillNodeTris(specificNode,&p);

  std::string finalname(filename);
  if ( !tp->nodename.empty() )
    finalname+="_"+tp->nodename;
  finalname+=fltFormatExt(tp->format);

  // if no mosaic, not write this heightmap and keep it in memory yet
  // it can be very heavy to have many big textures in memory, so there's left an implementation
  // that if we force width/height (-d option) then we can know beforehand the total size of the 
  // big texture and do the blitting in separated thread and deallocate here.
  if ( tp->mosaic.empty() )
  {
    fltWriteFormat(tp->format, finalname.c_str(), WIDTH, HEIGHT, tp->depth, heightdata);
    free(heightdata);
  }
  else
  {
    // we save this tile info (contain the actual height data)
    p.mapping=nullptr;
    p.v1 = p.v2 = nullptr;
    p.from1 = p.from2 = nullptr;
    addMosaicInfo(filename,p);
  }
}


// void fltThreadTask::flt2png(fltThreadPool* tp, flt* of)
// {
//   if ( !of->hie )
//   {
//     fprintf( stderr, "No nodes loaded in flt: %s\n", filename.c_str() );
//     return;
//   }
// 
//   // look for specific node if user passed non-empty parameter
//   flt_node* specificNode = of->hie->node_root;
//   if ( !tp->nodename.empty() )
//     findSpecificNode(of->hie->node_root, tp->nodename, tp->nodetype, &specificNode);
// 
//   fltExtent xtent;
//   computeExtent(of,&xtent,specificNode);
// 
//   // grayscale, one byte per pixel
//   double dim[3];
//   xtent.getDimension(dim);
//   const double AR = dim[0]/dim[1];
//   const double IAR= dim[1]/dim[0];
//   int WIDTH = tp->res[0]<0 ? (int)dim[0] : tp->res[0];
//   int HEIGHT = tp->res[1]<0 ? (int)dim[1] : tp->res[1];
//   if (WIDTH==0 && HEIGHT==0) WIDTH = (int)dim[0];
//   if (WIDTH==0) WIDTH=(int)(AR*HEIGHT);
//   if (HEIGHT==0) HEIGHT=(int)(IAR*WIDTH);
//   const double MAPPING[3] = { 1.0/dim[0]*WIDTH, 1.0/dim[1]*HEIGHT, 1.0/dim[2] * 255.0 };
//   const int numpixels = WIDTH*HEIGHT;
//   if ( numpixels<=0 )
//   {
//     fprintf(stderr, "Num pixels invalid for: %s. dim=%g %g %g\n", filename.c_str(), dim[0], dim[1], dim[2]);
//     return;
//   }
// 
//   uint8_t* lumdata = (uint8_t*)calloc( 1, numpixels );
//   if ( !lumdata ){ fprintf( stderr, "out of mem: %s\n", filename.c_str() ); return; }
//     
//   if ( !specificNode )
//   {
//     fprintf( stderr, "Invalid node to look geometry. %s\n", filename.c_str() );
//     return;
//   }
// 
//   // go through all triangles under node
//   fltNodeFillParam p;
//   p.of=of;
//   p.lumdata=lumdata;
//   p.xtent=&xtent;
//   p.dim=dim;
//   p.width = WIDTH;
//   p.height = HEIGHT;
//   p.mapping = MAPPING;
//   p.tp = tp;
//   fillNodeTris(specificNode,&p);
// 
//   std::string pngname(filename);
//   if ( !tp->nodename.empty() )
//     pngname+="_"+tp->nodename;
//   pngname+=".png";
//   stbi_write_png(pngname.c_str(), WIDTH, HEIGHT, 1, lumdata, WIDTH );
//   free(lumdata);
// }

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

void getRowCol(const std::string& name, const std::string& ms, int* row, int* col)
{
  size_t i=0,j=0;
  size_t w;
  char tmp[64]={0};
  int nc,mc,nmc;
  int t;
  *row = *col = -1;
  for (;i<ms.length();++i,++j)
  {
    mc = tolower(ms[i]);
    nc = tolower(name[j]);
    if ( nc!=mc && mc!='$') return; // name and ms should match
    if ( mc=='$' )
    {
      if ( i >= ms.length()-1 ) return;
      mc=tolower(ms[++i]);
      if ( mc == 'r' || mc == 'c' )
      {
        nmc=ms[++i];
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

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
bool computeMosaicRowCols(const std::string& mosaic)
{
  mosaicRows=mosaicCols=-1;
  int r,c;
  for (auto it:mosaicmap)
  {
    getRowCol(it.first, mosaic, &r, &c);
    if ( r>mosaicRows ) mosaicRows = r;
    if ( c>mosaicCols ) mosaicCols = c;
  }
  ++mosaicRows; ++mosaicCols;
  return mosaicCols>0 && mosaicRows>0;
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void generateMosaic(const std::string& name, fltThreadPool* tp)
{
  // compute size for mosaic image
  {
    
  }

  // for each image tile in mosaicmap
  //    compute the row/colum out of mosaic wildcard and tile name
  //    blit the image into the mosaic row/colum
  //    deallocate tile
  //    
  // save the mosaic image into the destination format
}

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////
void doProcess(const std::vector<std::string>& files, int format, const std::string& nodename, 
               int nodetype, int w, int h, int culling, int depth, const std::string& mosaic)
{
  fltThreadPool tp;
  tp.init(std::thread::hardware_concurrency()*2);
  tp.format = format;
  tp.nodename = nodename;
  tp.nodetype = nodetype;
  tp.res[0]=w;
  tp.res[1]=h;
  tp.culling=culling;
  tp.depth = depth;
  tp.mosaic = mosaic;

  // one task per file
  for ( const std::string& f : files )
    tp.addNewTask(fltThreadTask(fltThreadTask::TASK_FLT2ELEV, f) );

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

  // we finished individual tiles, we generate the big mosaic height image if enabled
  if ( !tp.mosaic.empty() && !mosaicmap.empty() )
  {
    generateMosaic("mosaic", &tp);
  }
  printf( "\n\nTime : %g secs\n", (fltGetTime()-t0)/1000.0);
  printf( "Triangles (ok/culled): %d/%d\n", tp.numtris, tp.numtrisbf);
  tp.deinit();
}

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
int main(int argc, const char** argv)
{
  int format=FORMAT_PNG;
  std::string nodename;
  int nodetype=-1;
  int culling=CULLING_CCW;
  int depth=8;
  std::vector<std::string> files;
  std::string wildcard;
  std::string mosaic;

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
        case 'm': if (i+1<argc) mosaic=argv[++i]; break;
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

  if ( format < FORMAT_PNG || format >= FORMAT_MAX )   format=FORMAT_PNG;
  if (culling< CULLING_NONE || culling > CULLING_CCW) culling=CULLING_CCW;
  if ( depth != 8 && depth != 16 && depth != 32 ) depth = 8;

  if ( format == FORMAT_PNG && depth != 8 )
  {
    fprintf(stderr, "Forcing 8 bits depth for PNG format\n");
    depth = 8;
  }

  if ( !wildcard.empty() )
    fltGetFilesFromWildcard(wildcard, files);
  
  if ( !files.empty() )
  {
    // generate template mosaic map with names if proceed
    if ( !mosaic.empty() )
    {
      for ( auto s :files ) addMosaicInfo(s,fltNodeFillParam());
      if ( !computeMosaicRowCols(mosaic) )
      {
        fprintf( stderr, "Invalid columns/rows found (%d/%d) with mosaic wildcard: %s\n", mosaicCols, mosaicRows, mosaic.c_str());
        return -1;
      }
    }
    doProcess(files, format, nodename, nodetype, w, h, culling, depth, mosaic);
  }
  else
  {
    char* program=flt_path_basefile(argv[0]);
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
  }
  
  return 0;
}