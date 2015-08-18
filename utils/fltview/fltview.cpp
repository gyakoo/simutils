#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <algorithm>

#define FLT_IMPLEMENTATION
#include <flt.h>

void print_flt(flt* of)
{
  flt_pal_tex* pt = of->pal_tex;
  while ( pt )
  {
    printf( "%s [%d] %dx%d\n", pt->name, pt->patt_ndx, pt->xy_loc[0], pt->xy_loc[1] );
    pt = pt->next;
  }
}

int main(int argc, const char** argv)
{
  flt_opts opts={0};
  flt of={0};
  fltu16 vtxstream[]={ 
    flt_vtx_stream_enc(FLT_VTX_POSITION ,24, 0), 
    flt_vtx_stream_enc(FLT_VTX_COLOR    , 4,24),
    flt_vtx_stream_enc(FLT_VTX_NORMAL   ,12,28),
    0
  };

  // configuring read options (we want header, texture palettes and vertices with pos/col/norm format)
  opts.flags |= FLT_OPT_HEADER;
  opts.palflags |= FLT_OPT_PAL_TEXTURE | FLT_OPT_PAL_VERTEX;
  opts.vtxstream = vtxstream;

  // actual read
  if ( flt_load_from_filename( "../../../data/titanic/TITANIC.flt", &of, &opts)==FLT_OK )
  {
    // printing texture records
    print_flt(&of);

    flt_release(&of);
  }
  return 0;
}
