#include <stdio.h>
#include <stdlib.h>
#ifdef _MSC_VER
#include <windows.h>
#endif
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

void print_flt(flt* of, double tim)
{
  flt_pal_tex* pt = of->pal->pal_tex;

  printf( "Time: %g\n", tim/1000.0);
  while ( pt )
  {
    printf( "%s [%d] %dx%d\n", pt->name, pt->patt_ndx, pt->xy_loc[0], pt->xy_loc[1] );
    pt = pt->next;
  }

  printf( "No. vertices: %d\n", of->pal->pal_vtx_count );
}

void read_with_custom_vertex_format(const char* filename)
{
  double t0;
  flt_opts opts={0};
  flt of={0};
  fltu16 vtxstream[4]={ 0 };
  vtxstream[0]=flt_vtx_stream_enc(FLT_VTX_POSITION , 0,12); 
  vtxstream[1]=flt_vtx_stream_enc(FLT_VTX_COLOR    ,12, 4);
  vtxstream[2]=flt_vtx_stream_enc(FLT_VTX_NORMAL   ,16,12);
  vtxstream[3]=0;

  // configuring read options (we want header, texture palettes and vertices with pos/col/norm format)
  opts.flags |= FLT_OPT_HEADER;
  opts.flags |= FLT_OPT_PAL_TEXTURE | FLT_OPT_PAL_VERTEX;
  opts.vtxstream = vtxstream;

  // actual read
  t0=fltGetTime();
  if ( flt_load_from_filename( filename, &of, &opts)==FLT_OK )
  {
    // printing texture records
    print_flt(&of, fltGetTime()-t0);

    flt_release(&of);
  }
}

void read_with_orig_vertex_format(const char* filename)
{
  double t0;
  flt_opts opts={0};
  flt of={0};

  // configuring read options (we want header, texture palettes and vertices with pos/col/norm format)
  opts.flags |= FLT_OPT_PAL_VERTEX;
  opts.flags |= FLT_OPT_VTX_FORMATSOURCE;

  // actual read
  t0=fltGetTime();
  if ( flt_load_from_filename( filename, &of, &opts)==FLT_OK )
  {
    // printing texture records
    print_flt(&of, fltGetTime()-t0);

    flt_release(&of);
  }
}

int main(int argc, const char** argv)
{
  read_with_custom_vertex_format("../../../data/titanic/TITANIC.flt"); 
  read_with_orig_vertex_format("../../../data/titanic/TITANIC.flt");
  return 0;
}
