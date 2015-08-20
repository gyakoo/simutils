#include <stdio.h>
#include <stdlib.h>
#define FLT_NO_OPNAMES
#define FLT_IMPLEMENTATION
#include <flt.h>

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

void pind(int ind)
{
  int i;
  for (i=0;i<ind;++i) printf("    "); 
}

void print_flt(flt* of, double tim, int ind)
{
  flt_pal_tex *pt;
  flt_node_extref* extref;

  if ( !of ) return;

  pind(ind); printf( "%s\n", of->filename);
  if ( tim>0.0 ) { pind(ind); printf( "Time: %g\n", tim/1000.0); }

  if ( of->pal )
  {
    pt = of->pal->tex_head;
    while ( pt )
    {
      pind(ind); 
      printf( "%s [%d] %dx%d\n", pt->name, pt->patt_ndx, pt->xy_loc[0], pt->xy_loc[1] );
      pt = pt->next;
    }
    pind(ind); printf( "No. vertices: %d\n", of->pal->vtx_count );
  }

  if ( of->hie_flat )
  {
    extref = of->hie_flat->extref_head;
    while (extref)
    {
      print_flt(extref->of,-1.0, ind+1);
      extref= extref->next;
    }
  }
}

void read_with_custom_vertex_format(const char* filename)
{
  double t0;
  flt_opts opts={0};
  flt of={0};
  int err;
  fltu16 vtxstream[4]={ 0 };
  vtxstream[0]=flt_vtx_stream_enc(FLT_VTX_POSITION , 0,12); 
  vtxstream[1]=flt_vtx_stream_enc(FLT_VTX_COLOR    ,12, 4);
  vtxstream[2]=flt_vtx_stream_enc(FLT_VTX_NORMAL   ,16,12);
  vtxstream[3]=0;

  // configuring read options (we want header, texture palettes and vertices with pos/col/norm format)
  opts.palflags |= FLT_OPT_PAL_TEXTURE | FLT_OPT_PAL_VERTEX;
  opts.hieflags |= FLT_OPT_HIE_HEADER;
  opts.vtxstream = vtxstream;

  // actual read
  t0=fltGetTime();
  err=flt_load_from_filename( filename, &of, &opts);
  if ( err==FLT_OK )
  {
    // printing texture records
    print_flt(&of, fltGetTime()-t0,0);

    flt_release(&of);
  }
  else
  {
    printf( "Error: %s\n", flt_get_err_reason(err) );
  }
}

void read_with_orig_vertex_format(const char* filename)
{
  double t0;
  flt_opts opts={0};
  flt of={0};
  int err;

  // configuring read options (we want header, texture palettes and vertices with pos/col/norm format)
  opts.palflags = FLT_OPT_PAL_VERTEX | FLT_OPT_PAL_VTX_SOURCE;
  opts.hieflags = FLT_OPT_HIE_FLAT | FLT_OPT_HIE_EXTREF;

  // actual read
  t0=fltGetTime();
  err=flt_load_from_filename( filename, &of, &opts);
  if ( err==FLT_OK )
  {
    // printing texture records
    print_flt(&of, fltGetTime()-t0,0);

    flt_release(&of);
  }
  else
  {
    printf( "Error: %s\n", flt_get_err_reason(err) );
  }
}

int callback_extref(flt_node_extref* extref, flt* of, void* userdata)
{
  printf( "Reading %s from: %s ...\n", extref->name, of->filename );
  return 1;
}

void read_with_callbacks_and_resolve(const char* filename)
{
  double t0;
  flt_opts opts={0};
  flt of={0};
  int err;

  // configuring read options
  opts.palflags = FLT_OPT_PAL_VERTEX | FLT_OPT_PAL_VTX_SOURCE;
  opts.hieflags = FLT_OPT_HIE_HEADER | FLT_OPT_HIE_FLAT | FLT_OPT_HIE_EXTREF | FLT_OPT_HIE_EXTREF_RESOLVE;
  opts.cb_extref = callback_extref;
  opts.cb_user_data = (void*)1;

  // actual read
  t0=fltGetTime();
  err=flt_load_from_filename( filename, &of, &opts);
  if ( err==FLT_OK )
  {
    // printing texture records
    print_flt(&of, fltGetTime()-t0,0);

    flt_release(&of);
  }
  else
  {
    printf( "Error: %s\n", flt_get_err_reason(err) );
  }

}

void read_with_search_paths()
{
  double t0;
  flt_opts opts={0};
  flt of={0};
  const char* spaths[]={ "c:\\tmp", "D:/simulation/", "../../../data/titanic/", 0 };
  int err;

  // configuring read options
  opts.palflags = 0;
  opts.hieflags = FLT_OPT_HIE_FLAT;
  opts.search_paths = spaths;
  // actual read
  t0=fltGetTime();
  err=flt_load_from_filename( "../titanic.flt", &of, &opts);
  if ( err==FLT_OK )
  {
    // printing texture records
    print_flt(&of, fltGetTime()-t0,0);

    flt_release(&of);
  }
  else
  {
    printf( "Error: %s\n", flt_get_err_reason(err) );
  }
}

int main(int argc, const char** argv)
{
  //read_with_custom_vertex_format("../../../data/titanic/TITANIC.flt"); 
  //read_with_orig_vertex_format("../../../data/titanic/TITANIC.flt");
  //read_with_callbacks_and_resolve("../../../data/camp/master.flt");
  read_with_callbacks_and_resolve("../../../data/camp/master.flt");
  //read_with_search_paths();

  return 0;
}
