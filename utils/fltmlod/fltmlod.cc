

#if defined(_DEBUG) && !defined(_WIN64)
#include <vld.h>
#endif

#include <stdio.h>
#include <stdlib.h>
//#define FLT_NO_OPNAMES
//#define FLT_LEAN_FACES
//#define FLT_ALIGNED
#define FLT_UNIQUE_FACES
#define FLT_WRITER
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
#include <tinyxml2.h>
#pragma warning(disable:4100 4005 4996)

// forwards
struct fltThreadPool;

struct fltExtent
{
  double extent_max[3];
  double extent_min[3];

  void setZero()
  {
    for ( int i = 0; i < 3; ++i )
      extent_max[i]=extent_min[i]=0.0;
  }

  void invalidate()
  {
    for ( int i = 0; i < 3; ++i )
    {
      extent_max[i] = -DBL_MAX;
      extent_min[i] = DBL_MAX;
    }
  }

  void getCenter(double* xyz)
  {
    for ( int i = 0; i < 3; ++i )
      xyz[i] = extent_min[i] + (extent_max[i]-extent_min[i])*0.5f;
  }
};

struct lod_def
{
  std::string id;
  double range[2];
};

struct lod_tile
{
  std::string id;
  std::vector<std::string> files;
};

struct tile_def
{
  std::string filename;
  fltExtent extent;
  std::vector<lod_tile> lodtiles;  
};

struct mlod_def
{
  std::map<std::string, lod_def*> lods;
  std::vector<tile_def*> tiles;

  void release()
  {
    for ( auto it = lods.begin(); it != lods.end(); ++it ) delete it->second;
    for (size_t i = 0; i < tiles.size(); ++i ) delete tiles[i];
    lods.clear();
    tiles.clear();
  }
};

// types
struct fltThreadTask
{
  enum f2dTaskType { TASK_NONE=-1, TASK_MAKE_TILE=0, TASK_COMP_EXTENT=1};
  f2dTaskType type;
  tile_def* tiledef;
  std::string fileForExtent;

  fltThreadTask():type(TASK_NONE), tiledef(nullptr){}
  fltThreadTask(f2dTaskType tt, tile_def* tdef):type(tt), tiledef(tdef){}
  fltThreadTask(f2dTaskType tt, const std::string& file):type(tt), tiledef(nullptr), fileForExtent(file){}

  void runTask(fltThreadPool* tp);
  void makeTile(fltThreadPool* tp);
  void compExtent(fltThreadPool* tp);
};

struct fltThreadPool
{
  void init(int numThreads, const std::string& cachefile);
  void deinit();
  void runcode();
  bool getNextTask(fltThreadTask* t);
  void addNewTask(const fltThreadTask& task);
  bool isWorking(){ return !tasks.empty() || workingTasks>0; }
  void addExtent(const std::string& file, const fltExtent& xtent);
  void getExtent(const std::string& file, fltExtent& xtent);
  void loadCache();
  void waitForCompExtentTasks();

  std::vector<std::thread*> threads;
  std::deque<fltThreadTask> tasks;
  std::map<std::string, fltExtent> extents;
  std::mutex mtxTasks;
  std::mutex mtxExtents;
  std::atomic_int workingTasks;
  std::atomic_int compExtentTasks;
  std::atomic_bool doneTotalExtents;
  mlod_def def;
  std::string outdir;
  std::string indir;
  std::string cachefile;
  FILE* hcache;
  bool finish;  
  bool forcewrite;
};

void fltThreadPool::init(int numThreads, const std::string& cachefile)
{
  hcache = nullptr;
  deinit();
  finish=false;
  threads.reserve(numThreads);
  workingTasks=0;
  compExtentTasks=0;
  for(int i = 0; i < numThreads; ++i)
    threads.push_back(new std::thread(&fltThreadPool::runcode,this));
  this->cachefile = cachefile;
  doneTotalExtents = false;
  forcewrite = false;
  loadCache();
}


void fltThreadPool::deinit()
{
  finish = true;
  for (std::thread* t : threads)
  {
    t->join();
    delete t;
  }
  threads.clear();
  tasks.clear();
  def.release();
  if ( hcache ) { fclose(hcache); hcache=nullptr; }
}

void fltThreadPool::loadCache()
{
  FILE* f = fopen(cachefile.c_str(), "rt");
  if ( f )
  {
    char line[512];
    char fname[256];
    fltExtent xtent;
    while (fgets(line,512,f))
    {
      if ( !*line ) continue;
      sscanf(line, "%s %lf %lf %lf %lf %lf %lf", fname, xtent.extent_min, xtent.extent_min+1, xtent.extent_min+2
        , xtent.extent_max, xtent.extent_max+1, xtent.extent_max+2 );
      if ( !*fname ) continue;
      addExtent(fname,xtent);
    }
    fclose(f);
  }

  // re-write same info into a new cache file created for new cache entries
  hcache = fopen(cachefile.c_str(), "wt");
  if ( hcache )
  {
    for (auto it:extents)
    {
      fltExtent& x=it.second;
      fprintf( hcache, "%s %g %g %g %g %g %g\n", it.first.c_str(), x.extent_min[0], x.extent_min[1], x.extent_min[2],
        x.extent_max[0], x.extent_max[1], x.extent_max[2] );
    }
  }
}

void fltThreadPool::waitForCompExtentTasks()
{
  while ( compExtentTasks > 0 )
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  
  std::lock_guard<std::mutex> lock(mtxExtents);
  // once we know no more comp extent tasks are running, we can safely close the cache file
  if ( hcache )
  {
    fclose(hcache);
    hcache = nullptr;
  }

  // we also compute total tile extents per Tile, by encompassing children ones
  if ( !doneTotalExtents )
  {
    for ( auto itTileDef : def.tiles )
    {
      itTileDef->extent.invalidate();
      for ( auto itLodTile : itTileDef->lodtiles )
      {
        for ( auto itFile : itLodTile.files )
        {
          auto itExt = extents.find(itFile);
          if ( itExt != extents.end() )
          {
            for ( int i = 0; i < 3; ++i )
            {
              if ( itExt->second.extent_min[i] < itTileDef->extent.extent_min[i] )
                itTileDef->extent.extent_min[i] = itExt->second.extent_min[i];
              if ( itExt->second.extent_max[i] > itTileDef->extent.extent_max[i] )
                itTileDef->extent.extent_max[i] = itExt->second.extent_max[i];
            }
          }
        }
      }
    }
    doneTotalExtents = true;
  }
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
  std::lock_guard<std::mutex> lock(mtxTasks);
  tasks.push_front( task );
}

void fltThreadPool::addExtent(const std::string& file, const fltExtent& xtent)
{
  std::lock_guard<std::mutex> lock(mtxExtents);
  extents[file] = xtent;
  if ( hcache )
  {
    fprintf( hcache, "%s %g %g %g %g %g %g\n", file.c_str(), 
      xtent.extent_min[0], xtent.extent_min[1], xtent.extent_min[2],
      xtent.extent_max[0], xtent.extent_max[1], xtent.extent_max[2] );
  }
}

void fltThreadPool::getExtent(const std::string& file, fltExtent& xtent)
{
  // locking not necessary as we're sure to process TASK_MAKE_TILE tasks
  // after all setExtent calls have been made.
  std::lock_guard<std::mutex> lock(mtxExtents);
  auto it = extents.find(file);
  if ( it != extents.end() )
    xtent = it->second;
  else
    xtent.setZero();
}

void prepare_header(flt_header* h)
{
  strcpy_s( h->ascii, "db" );
  h->format_rev = 1560;
  h->edit_rev = 0;
  strcpy_s( h->dtime, "Sat Aug 28 03:42:36 2010" );
  h->unit_mult = 1;
  h->v_coords_units = 0;
  h->texwhite_new_faces=0;
  h->flags = 0x80000000;
  h->proj_type=0;
  h->v_storage_type = 1;
  h->db_orig = 100;    
}

flt_node* make_lod_hie(tile_def* tiledef, lod_tile& lt, lod_def* loddef, fltThreadPool* tp)
{
  // LOD -> G1 ->* [LOD -> XREF]
  // parent lod node
  flt_node_lod* lodnode = (flt_node_lod*)flt_node_create(0,FLT_NODE_LOD, lt.id.c_str());
  lodnode->switch_in = loddef->range[0];
  lodnode->switch_out= loddef->range[1];
  tiledef->extent.getCenter(lodnode->cnt_coords);
  
  // one only group under parent lod
  flt_node* g1 = flt_node_create(0,FLT_NODE_GROUP, "g1");
  flt_node_add_child((flt_node*)lodnode, g1);

  // one lod node per children tile under g1
  fltExtent xtent;
  for ( const std::string& tilefile : lt.files )
  {
    flt_node_lod* lodchild = (flt_node_lod*)flt_node_create(0,FLT_NODE_LOD,tilefile.c_str());
    lodchild->switch_in = loddef->range[0];
    lodchild->switch_out = loddef->range[1];
    tp->getExtent(tilefile,xtent);
    xtent.getCenter(lodchild->cnt_coords);

    // each children lod node with a child group->xref to the tile itself
    flt_node* g2 = flt_node_create(0,FLT_NODE_GROUP, "g2");
    flt_node* xref = flt_node_create(0,FLT_NODE_EXTREF,tilefile.c_str());
    flt_node_add_child(g2, xref);
    flt_node_add_child((flt_node*)lodchild, g2);

    // children lod under g1
    flt_node_add_child(g1,(flt_node*)lodchild);
  }

  return (flt_node*)lodnode;
}

void prepare_hie(flt_hie* hie, tile_def* tiledef, fltThreadPool* tp)
{
  hie->node_root = flt_node_create(0, FLT_NODE_BASE, "root");
  flt_node_group* g0 = (flt_node_group*)flt_node_create(0, FLT_NODE_GROUP, "g0" );
  flt_node_add_child(hie->node_root,(flt_node*)g0);

  // plain list of children, one for each lod (another option would be a binary try like)
  for ( auto& lt : tiledef->lodtiles )
  {
    auto itlod = tp->def.lods.find(lt.id);
    if ( itlod == tp->def.lods.end() ){ fprintf( stderr, "Can't find LOD id=%s for %s\n", lt.id.c_str(), tiledef->filename.c_str() ); continue; }
    flt_node* lodn = make_lod_hie(tiledef,lt,itlod->second, tp);
    flt_node_add_child((flt_node*)g0,lodn);
  }
}

bool file_exists(const std::string& file)
{
  DWORD ft = GetFileAttributesA(file.c_str());
  if (ft == INVALID_FILE_ATTRIBUTES) return false;
  return (ft & FILE_ATTRIBUTE_NORMAL)!=0;
}

void make_master(const std::string& masterfile, fltThreadPool* tp)
{
  if ( masterfile.empty() )
    return;
  std::string finaloutfile = tp->outdir + masterfile;
  if ( file_exists(finaloutfile) && !tp->forcewrite )
  {
    fprintf( stderr, "File exists and no force write (-f) specified: %s\n", finaloutfile.c_str() );
    return;
  }
  flt of={0};
  of.filename = flt_strdup(finaloutfile.c_str());
  of.header = (struct flt_header*)flt_calloc(1, sizeof(flt_header));
  prepare_header(of.header);
  of.hie = (struct flt_hie*)flt_calloc(1,sizeof(flt_hie));
  of.hie->node_root = flt_node_create(0,FLT_NODE_BASE, "root");
  flt_node* g0 = flt_node_create(0, FLT_NODE_GROUP, "g0" );
  flt_node_add_child(of.hie->node_root, g0);

  // one xref per tile to create
  for ( auto itTileDef : tp->def.tiles )
  {
    flt_node* xref = flt_node_create(0,FLT_NODE_EXTREF, itTileDef->filename.c_str());
    flt_node_add_child(g0,xref);
  }
  flt_write_to_filename(&of);
  flt_release(&of);
}

void fltThreadTask::makeTile(fltThreadPool* tp)
{
  // spin loop to wait for all computing extent tasks to be finished
  tp->waitForCompExtentTasks();
  std::string finaloutfile = tp->outdir + tiledef->filename;
  if ( file_exists(finaloutfile) && !tp->forcewrite )
  {
    fprintf( stderr, "File exists and no force write (-f) specified: %s\n", finaloutfile.c_str() );
    return;
  }
  flt of = {0};
  of.filename = flt_strdup(finaloutfile.c_str());
  of.header = (struct flt_header*)flt_calloc(1, sizeof(flt_header));
  prepare_header(of.header);  
  of.hie = (struct flt_hie*)flt_calloc(1,sizeof(flt_hie));
  prepare_hie(of.hie, tiledef, tp);
  flt_write_to_filename(&of);
  flt_release(&of);
}

void fltThreadTask::compExtent(fltThreadPool* tp)
{
  tp->compExtentTasks++;
  flt_opts opts = {0};
  opts.pflags = FLT_OPT_PAL_VERTEX | FLT_OPT_PAL_VTX_POSITION;
  opts.hflags = FLT_OPT_HIE_ALL_NODES | FLT_OPT_HIE_HEADER;
  opts.dfaces_size = 1543;
  flt of = {0};
  
  // load file
  std::string finalpath = tp->indir + fileForExtent;
  flt_load_from_filename(finalpath.c_str(), &of, &opts);

  // compute extent
  if ( of.pal && of.pal->vtx_array && of.pal->vtx_count)
  {
    // we assume 3 doubles consecutive (see opts)
    double* xyz=(double*)of.pal->vtx_array;
    fltExtent xtent; xtent.invalidate();
    int j;
    for ( size_t i = 0; i < of.pal->vtx_count; ++i, xyz+=3 )
    {
      for (j=0;j<3;++j)
      {
        if ( xyz[j]<xtent.extent_min[j] ) xtent.extent_min[j] = xyz[j];
        if ( xyz[j]>xtent.extent_max[j] ) xtent.extent_max[j] = xyz[j];
      }
    }

    // add extent
    tp->addExtent(fileForExtent, xtent);
  }

  flt_release(&of);
  tp->compExtentTasks--;
}

void fltThreadTask::runTask(fltThreadPool* tp)
{
  switch( type )
  {
  case TASK_MAKE_TILE: makeTile(tp); break;
  case TASK_COMP_EXTENT: compExtent(tp); break;
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

void ensure_dir_exists(const std::string& dir)
{
  CreateDirectoryA( dir.c_str(), nullptr );
}

bool write_files(const std::string& cachefile, const mlod_def& def, 
                 const std::string& outdir, const std::string& indir, 
                 const std::string& masterfile, bool force)
{
  fltThreadPool tp;
  // init thread pool
  tp.init(std::thread::hardware_concurrency()*2, cachefile);
  tp.def = def;
  tp.outdir = outdir;
  tp.indir = indir;
  tp.forcewrite = force;

  ensure_dir_exists(tp.outdir);

  // create one task per referenced subtiles to compute their extents
  for ( auto itTileDef : tp.def.tiles )
  {
    for ( auto itLodTile : itTileDef->lodtiles )
    {
      for ( auto itFile : itLodTile.files )
      {
        // only adds if not exists already
        if ( tp.extents.find(itFile) == tp.extents.end() )
        {
          fltThreadTask task(fltThreadTask::TASK_COMP_EXTENT, itFile);
          tp.addNewTask(task);
        }
      }
    }
  }

  // create one task per tile to be written
  for ( size_t i = 0; i < tp.def.tiles.size(); ++i )
  {
    fltThreadTask task(fltThreadTask::TASK_MAKE_TILE, tp.def.tiles[i]);
    tp.addNewTask(task);
  }

  // create master in main thread
  make_master(masterfile, &tp);

  // wait until tasks finished
  double t0=fltGetTime();
  do
  {
    std::this_thread::sleep_for(std::chrono::milliseconds(100));    
  } while ( tp.isWorking() );

  printf( "Time: %g secs\n", (fltGetTime()-t0)/1000.0 );
  tp.deinit();
  return true;
}

void trim_back(std::string& str)
{
  while ( !str.empty() && str.back()==' ') str.pop_back();
}

void path_append_dir_sep(std::string& str)
{
  if ( str.back() != '\\' && str.back() != '/' ) str += '/';
}

void xml_parse_lod_tilenames(const std::string& text, std::vector<std::string>* list)
{
  if ( text.empty() ) return;
  std::string tmp=text;
  size_t i=0;
  char c=0;
  std::string single;
  while ( i < tmp.length() )
  {
    c = tmp[i];    
    if ( c==',' )
    {
      ++i;
      if ( !single.empty() )
      {
        trim_back(single);
        list->push_back(single);
      }
      single.clear();
      continue;
    }
    if ( c!='\r' && c!='\n' && c!='\t' && !(single.empty() && c==' ') )
    {
      single += c;
    } 
    ++i;
  }
  if (!single.empty() ) { trim_back(single); list->push_back(single); }
}

bool read_xml(const std::string& xmlfile, mlod_def* def)
{
  using namespace tinyxml2;
  tinyxml2::XMLDocument doc;
  if ( doc.LoadFile( xmlfile.c_str() ) != XML_NO_ERROR ) 
    return false;
  XMLElement* root = doc.FirstChildElement("fltmlod");
  if ( !root )
  {
    fprintf( stderr, "No root node in xml\n" );
    return false;
  }

  // -- Reading <lods> --
  {
    XMLElement* lods = root->FirstChildElement("lods");
    if ( lods )
    {
      XMLElement* lod = lods->FirstChildElement("lod");
      while ( lod )
      {
        const char* id= lod->Attribute("id");
        if ( id )
        {
          lod_def* ldef = new lod_def;
          ldef->id = id;
          ldef->range[0] = lod->DoubleAttribute("in");
          ldef->range[1] = lod->DoubleAttribute("out");
          def->lods[id] = ldef;
        }
        else
          fprintf(stderr, "<lod> has no id attribute\n" );
        lod = lod->NextSiblingElement("lod");
      }
    }
    if ( def->lods.empty() )
    {
      fprintf( stderr, "No lods defined in xml\n" );
      return false;
    }
  }

  
  // reading <tiles>
  {
    XMLElement* tiles = root->FirstChildElement("tiles");
    if ( tiles )
    {
      for ( XMLElement* tile = tiles->FirstChildElement("tile"); tile; tile = tile->NextSiblingElement("tile") )
      {
        const char* fname = tile->Attribute("name");
        if ( !fname ){ fprintf( stderr, "No filename defined for <tile>\n" ); continue; }

        tile_def* tiledef = new tile_def;
        tiledef->filename = fname;
        for ( XMLElement* tilelod= tile->FirstChildElement("lod"); tilelod; tilelod=tilelod->NextSiblingElement("lod") )
        {
          const char* id=tilelod->Attribute("id");
          if ( !id ) { fprintf(stderr, "No id defined for <lod> in tile %s\n", fname); continue; }
          lod_tile lodtile;
          lodtile.id = id;
          xml_parse_lod_tilenames(tilelod->GetText(), &lodtile.files);
          if ( lodtile.files.empty() ){ fprintf(stderr, "No tile names in lod id=%s for tile %s\n", id, fname); continue;}
          tiledef->lodtiles.push_back(lodtile);
        }
        def->tiles.push_back( tiledef );          
      }
    }
  }

  return true;
}

void print_usage(const char* p)
{
  char* program=flt_path_basefile(p);
  printf( "%s: Makes LOD tile node files out of a XML specification\n\n", program );
  printf( "Usage: $ %s [options] <in_xml_file>\n\n", program );
  printf( "Options:\n");
  printf( "\t -?        : This help screen\n");
  printf( "\t -o <dir>  : Output directory of lod tile files. Default=Base path of <in_xml_file>\n" );
  printf( "\t -i <dir>  : Input directory of tile files. Default=Base path of <in_xml_file>\n");
  printf( "\t -f        : Force file overwriting. Default=none\n" );
  printf( "\t -m <file> : Optionally specify a master file to be created with plain xrefs to created tiles. Default=none\n");
  printf( "\nExamples:\n" );
  printf( "\tCreates LOD tile files in current directory forcing overwrite:\n" );
  printf( "\t  $ %s -f my_lods.xml\n\n", program);  
  flt_free(program);
}

int main(int argc, const char** argv)
{
  std::string xmlfile, outdir, indir, cachefile, masterfile;
  bool force=false;
  for ( int i = 1; i < argc; ++i )
  {
    if ( argv[i][0]=='-' )
    {
      char c=argv[i][1];
      switch (c)
      {
      case '?': print_usage(argv[0]); return -1;
      case 'f': force=true; break;            
      case 'o': if (i+1<argc) outdir = argv[++i]; break;
      case 'i': if (i+1<argc) indir= argv[++i]; break;
      case 'c': if (i+1<argc) cachefile=argv[++i]; break;
      case 'm': if (i+1<argc) masterfile=argv[++i]; break;
      }
    }
    else
    {
      xmlfile=argv[i];
    }
  }

  if ( xmlfile.empty() )
  {
    fprintf( stderr, "No input xml file defined\n" );
    print_usage(argv[0]);
    return -1;
  }
  
  if ( cachefile.empty() )
  {
    cachefile = xmlfile + ".cache";
  }

  if ( outdir.empty() || indir.empty() )
  {
    char* base = flt_path_base(xmlfile.c_str());
    if ( outdir.empty() ) outdir = base;
    if ( indir.empty() ) indir = base;
    flt_safefree(base);
  }

  path_append_dir_sep(outdir);
  path_append_dir_sep(indir);

  mlod_def def;
  if ( !read_xml(xmlfile, &def) )
  {
    fprintf( stderr, "Cannot read xml file: %s\n", xmlfile.c_str() );
    return -1;
  }

  if ( !write_files(cachefile, def, outdir, indir, masterfile, force) )
  {
    fprintf( stderr, "Cannot write files\n" );
    return -1;
  }
  
  return 0;
}