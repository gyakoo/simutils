

#if defined(_DEBUG) && !defined(_WIN64)
#include <vld.h>
#endif

#include <stdio.h>
#include <stdlib.h>
//#define FLT_NO_OPNAMES
//#define FLT_LEAN_FACES
//#define FLT_ALIGNED
//#define FLT_UNIQUE_FACES
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

void prepare_hie(flt_hie* hie)
{
  
}

bool write_files(const mlod_def& def, const std::string& outdir, bool force)
{
  // write every tile file in a separated task
  return true;
}


void xml_parse_lod_tilenames(const std::string& text, std::vector<std::string>* list)
{
  if ( text.empty() ) return;
  std::string tmp=text;
  size_t i=0,j=0;

  while ( i < tmp.length() )
  {
    if ( tmp[i]==',' ) tmp[i]=0;
  }
#error Estaba partiendo por comas los nombres.
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
      return -1;
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
  printf( "\t -?      : This help screen\n");
  printf( "\t -d <dir>: Output directory of lod tile files. Default=current\n" );  
  printf( "\t -f      : Force file overwriting. Default=none\n" );
  printf( "\nExamples:\n" );
  printf( "\tCreates LOD tile files in current directory forcing overwrite:\n" );
  printf( "\t  $ %s -f my_lods.xml\n\n", program);  
  flt_free(program);
}

int main(int argc, const char** argv)
{
  std::string xmlfile, outdir;
  bool force=false;
  for ( int i = 1; i < argc; ++i )
  {
    if ( argv[i][0]=='-' )
    {
      char c=argv[i][1];
      switch (c)
      {
      case '?':
        print_usage(argv[0]);
        return -1;
        break;
      case 'f':
        force=true; 
        break;
      break;            
      case 'd':
        if (i+1<argc) outdir = argv[++i];
      break;
      }
    }
    else
      xmlfile=argv[i];
  }

  if ( xmlfile.empty() )
  {
    fprintf( stderr, "No input xml file defined\n" );
    print_usage(argv[0]);
    return -1;
  }

  mlod_def def;
  if ( !read_xml(xmlfile, &def) )
  {
    fprintf( stderr, "Cannot read xml file: %s\n", xmlfile.c_str() );
    return -1;
  }

  if ( !write_files(def, outdir, force) )
  {
    fprintf( stderr, "Cannot write files\n" );
    return -1;
  }

  def.release();

  return 0;
}