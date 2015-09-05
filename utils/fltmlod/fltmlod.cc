

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
#pragma warning(disable:4100 4005 4996)

struct lod
{
  flt_node_group* groupnode;
  std::vector<std::string> xrefs;
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

void open_add_lod(const std::string& lff, int i, std::vector<lod>& lods)
{
  char tmp[512];
  sprintf_s(tmp, lff.c_str(), i);
  FILE* f=fopen(tmp,"rt");
  if ( !f ) return;
  lod l;
  char* start;
  l.groupnode = (flt_node_group*)flt_node_create(0,FLT_NODE_GROUP,FLT_NULL);
  double sio[2]={0};
  double cen[3]={0};
  double ran=0.0;
  while (fgets(tmp,512,f))
  {
    start = tmp;
    while (*start==' '||*start=='\t')
      ++start;
    if (*start)
    {
      for (int i=0;i<512;++i){ if (tmp[i]=='\n'||tmp[i]=='\r'){ tmp[i]=0;break; } }
      l.xrefs.push_back(start);
    }
  }
  fclose(f);
  lods.push_back(l);
}

void prepare_hie(flt_hie* hie, const std::string& lff)
{
  std::vector<lod> lods;
  for ( int i = 0; i < 10; ++i )
    open_add_lod(lff,i,lods);

  // root -> g1 -> lod -> {glod0, glod1, glod2...} -> {xrefs...}
  hie->node_root = flt_node_create(0,FLT_NODE_BASE,"root");
  flt_node_group* group = (flt_node_group*)flt_node_create(0,FLT_NODE_GROUP,"g1");
  hie->node_root->child_head = (flt_node*)group;
  flt_node_lod* lodnode = (flt_node_lod*)flt_node_create(0,FLT_NODE_LOD, "lod");
  group->base.child_head = (flt_node*)lodnode;
  lodnode->switch_in = s_in;
  lodnode->switch_out = s_out;
  for(int i=0;i<3;++i ) lodnode->cnt_coords[i] = center[i];
  lodnode->trans_range = trange;
  lodnode->sig_size = sigsize;

  flt_node_group* last=FLT_NULL;
  for ( size_t i = 0; i < lods.size();++i)
  {
    lod& l = lods[i];
    
    // if first group among children
    if ( !last )
    {
      last = l.groupnode;
      lodnode->base.child_head = (flt_node*)last;
    }
    else
    {
      // it's next sibling group
      last->base.next = (flt_node*)l.groupnode;
      last = l.groupnode;
    }

    // all children xrefs
    flt_node_extref* lastxref=FLT_NULL;
    for ( size_t j = 0; j < l.xrefs.size(); ++j )
    {
      flt_node_extref* newxref= (flt_node_extref*)flt_node_create(0,FLT_NODE_EXTREF,FLT_NULL);
      newxref->base.type = FLT_NODE_EXTREF;
      if ( !lastxref )
      {
        lastxref = newxref;
        last->base.child_head = (flt_node*)lastxref;
      }
      else
      {
        lastxref->base.next = (flt_node*)newxref;
        lastxref = newxref;
      }
      lastxref->base.name = flt_strdup(l.xrefs[j].c_str());
    }
  }
}

void write_file(const std::string& outfile, const std::string& lff)
{
  flt* of=(flt*)flt_calloc(1,sizeof(flt));
  of->filename=flt_strdup(outfile.c_str());
  of->header = (flt_header*)flt_calloc(1,sizeof(flt_header));
  prepare_header(of->header);
  of->hie = (flt_hie*)flt_calloc(1,sizeof(flt_hie));
  prepare_hie(of->hie, lff);

  if ( !flt_write_to_filename(of) )
    printf( "Error\n" );

  flt_release(of);
  flt_free(of);
}

void print_usage(const char* p)
{
  char* program=flt_path_basefile(p);
  printf( "%s: Makes a master Openflight out of LOD files as xrefs\n", program );
  printf( "\tIt works by reading lod files from 0..9\n\n");
  printf( "Usage: $ %s [-o outfile]\n\n", program );
  printf( "Options:\n");
  printf( "\t -?        : This help screen\n");
  printf( "\t -l lodff  : LOD Files printf Format. Something like lod%%i.txt\n");
  printf( "\t -o        : Output file. default=master.flt\n" );  
  printf( "\nExamples:\n" );
  printf( "\tCreates a master out of lod[n].txt files in the current folder:\n" );
  printf( "\t  $ %s -o mymaster.flt\n\n", program);
  printf( "\tCreates a master out of these lod files:\n" );
  printf( "\t  $ %s -l C:\\mylod_%%i.txt -o C:\\master.flt\n\n", program);
  flt_free(program);
}

int main(int argc, const char** argv)
{
  std::string outfile, lodfilefmt;
  char c;
  for ( int i = 1; i < argc; ++i )
  {
    if ( argv[i][0]=='-' )
    {
      c=argv[i][1];
      switch (c)
      {
      case '?':
        print_usage(argv[0]);
        return -1;
        break;
      case 'o':
        if (i+1<argc) outfile = argv[++i];
      break;            
      case 'l':
        if (i+1<argc) lodfilefmt = argv[++i];
      break;
      }
    }    
  }
  if ( outfile.empty() )
    outfile = "master.flt";
  if ( lodfilefmt.empty() )
    lodfilefmt = "lod%d.txt";
  write_file(outfile, lodfilefmt);
  return 0;
}