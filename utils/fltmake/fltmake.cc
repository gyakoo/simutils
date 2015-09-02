

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
  flt_node_lod* newnode;
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

void open_add_lod(int i, std::vector<lod>& lods)
{
  char tmp[512];
  sprintf_s(tmp, "lod%d.txt", i);
  FILE* f=fopen(tmp,"rt");
  if ( !f ) return;
  lod l;
  char* start;
  l.newnode = (flt_node_lod*)flt_node_create(0,FLT_NODE_LOD,FLT_NULL);
  while (fgets(tmp,512,f))
  {
    start = tmp;
    while (*start==' '||*start=='\t')
      ++start;
    if (*start)
    {
      for (int i=0;i<512;++i){ if (tmp[i]=='\n'||tmp[i]=='\r'){ tmp[i]=0;break; } }
      if ( *start == '$' )
      {
        if (!strncmp(start, "$switchinout",12))
          sscanf(start+13,"%lf %lf", &l.newnode->switch_in, &l.newnode->switch_out);
        else if (!strncmp(start,"$cent_coords",12) )
          sscanf(start+13,"%lf %lf %lf", l.newnode->cnt_coords, l.newnode->cnt_coords+1, l.newnode->cnt_coords+2);
        else if (!strncmp(start,"$trans_range",12) )
          sscanf(start+13,"%lf", &l.newnode->trans_range);
        else if (!strncmp(start,"$signif_size",12) )
          sscanf(start+13,"%lf", &l.newnode->sig_size);
        else if (!strncmp(start,"$hexad_flags",12) )
          sscanf(start+13,"%x", &l.newnode->flags);
      }
      else
      {
        l.xrefs.push_back(start);
      }
    }
  }
  fclose(f);
  lods.push_back(l);
}

void prepare_hie(flt_hie* hie)
{
  std::vector<lod> lods;
  for ( int i = 0; i < 10; ++i )
    open_add_lod(i,lods);

  hie->node_root = flt_node_create(0,FLT_NODE_BASE,FLT_NULL);
  flt_node_group* group = (flt_node_group*)flt_node_create(0,FLT_NODE_GROUP,FLT_NULL);
  hie->node_root->child_head = (flt_node*)group;
  flt_node_lod* last=FLT_NULL;
  for ( size_t i = 0; i < lods.size();++i)
  {
    lod& l = lods[i];
    // lod node
    if ( !last )
    {
      last = l.newnode;
      group->base.child_head = (flt_node*)last;
    }
    else
    {
      last->base.next = (flt_node*)l.newnode;
      last = l.newnode;
    }

    // all children xrefs
    flt_node_extref* lastxref=FLT_NULL;
    for ( size_t j = 0; j < l.xrefs.size(); ++j )
    {
      flt_node_extref* newxref= (flt_node_extref*)flt_calloc(1,sizeof(flt_node_extref));
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
      lastxref->base.name = flt_strdup(l.xrefs[i].c_str());
    }
  }
}

void write_file(const std::string& outfile)
{
  flt* of=(flt*)flt_calloc(1,sizeof(flt));
  of->filename=flt_strdup(outfile.c_str());
  of->header = (flt_header*)flt_calloc(1,sizeof(flt_header));
  prepare_header(of->header);
  of->hie = (flt_hie*)flt_calloc(1,sizeof(flt_hie));
  prepare_hie(of->hie);

  flt_write_to_filename(of);

  flt_release(of);
  flt_free(of);
}

void print_usage(const char* p)
{
  char* program=flt_path_basefile(p);
  printf( "%s: Makes a master Openflight out of LOD files as xrefs\n", program );
  printf( "\t It works by reading lod[n].txt files from current folder. n=0..9\n\n");
  printf( "Usage: $ %s [-o outfile]\n", program );    
  printf( "\n" );
  flt_free(program);
}

int main(int argc, const char** argv)
{
  std::string outfile;
  for ( int i = 1; i < argc; ++i )
  {
    if ( argv[i][0]=='-' )
    {
      if ( argv[i][1]=='o' && i+1<argc)
      {
        outfile = argv[i+1];
        ++i;
      }
      else if ( argv[i][0]== '?')
      {
        print_usage(argv[0]);
        return -1;
      }
    }
  }
  if ( outfile.empty() )
    outfile = "master.flt";
  write_file(outfile);    
  return 0;
}