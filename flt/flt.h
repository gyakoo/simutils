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

/*
DOCUMENTATION 

- Define FLT_NO_OPNAMES to skip opcode name strings
- Define FLT_IMPLEMENTATION before including this in *one* C/CPP file to expand the implementation
- Define the supported version in FLT_VERSION. Maximum supported version is FLT_GREATER_SUPPORTED_VERSION
*/

#ifndef _FLT_H_
#define _FLT_H_

/////////////////////

#ifdef _MSC_VER
#pragma warning(disable:4244 4100 4996)
#endif
/* 
Version check
*/
#define FLT_OK 0
#define FLT_ERR_FOPEN 1
#define FLT_ERR_OPREAD 2
#define FLT_ERR_VERSION 3

#define FLT_GREATER_SUPPORTED_VERSION 1640

#ifndef FLT_VERSION
#define FLT_VERSION FLT_GREATER_SUPPORTED_VERSION
#endif

#if FLT_VERSION > FLT_GREATER_SUPPORTED_VERSION
#error Openflight FLT version not supported. Max. supported version FLT_LAST_VERSION_SUPPORTED
#endif

#ifdef __cplusplus
extern "C" {
#endif

  typedef struct flt_opts
  {
  }flt_opts;

  typedef struct flt
  {
    int errcode;
  }flt;

    // Load openflight information into of with given options
  bool flt_load_from_filename(const char* filename, flt* of, flt_opts* opts);

    // Returns reason of the error code. Define FLT_LONG_ERR_MESSAGES for longer texts.
  const char* flt_get_err_reason(int errcode);



#ifdef __cplusplus
};

// FLT Opcodes
#define FLT_OP_DONTCARE 0
#define FLT_OP_HEADER 1
#define FLT_OP_MAX 154

#endif

#ifdef FLT_IMPLEMENTATION
#if defined(flt_malloc) && defined(flt_free)
// ok
#elif !defined(flt_malloc) && !defined(flt_free)
// ok
#else
#error "Must define all or none of flt_malloc, flt_free"
#endif

#ifndef flt_malloc
#define flt_malloc(sz) malloc(sz)
#endif

#ifndef flt_free
#define flt_free(p) free(p)
#endif

#if defined(WIN32) || defined(_WIN32) || (defined(sgi) && defined(unix) && defined(_MIPSEL)) || (defined(sun) && defined(unix) && !defined(_BIG_ENDIAN)) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN)) || (defined(__APPLE__) && defined(__LITTLE_ENDIAN__)) || (defined( _PowerMAXOS ) && (BYTE_ORDER == LITTLE_ENDIAN ))
#define FLT_LITTLE_ENDIAN
#elif (defined(sgi) && defined(unix) && defined(_MIPSEB)) || (defined(sun) && defined(unix) && defined(_BIG_ENDIAN)) || defined(vxw) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN)) || ( defined(__APPLE__) && defined(__BIG_ENDIAN__)) || (defined( _PowerMAXOS ) && (BYTE_ORDER == BIG_ENDIAN) )
#define FLT_BIG_ENDIAN
#else
#  error unknown endian type
#endif

typedef unsigned char  flt_u8;
typedef unsigned short flt_u16;
typedef unsigned int   flt_u32;
typedef char  flt_i8;
typedef short flt_i16;
typedef int   flt_i32;

#ifdef FLT_LITTLE_ENDIAN
void flt_swap16(void* d)
{ 
  unsigned char* b=(unsigned char*)d;
  const unsigned char t = b[0];
  b[0]=b[1]; b[1]=t;
}

void flt_swap32(void* d)
{ 
  unsigned char* b=(unsigned char*)d;
  const unsigned char t0 = b[0], t1 = b[1];
  b[0]=b[3]; b[1]=b[2];
  b[3]=t0;   b[2]=t1;
}

void flt_swap64(void* d)
{
  unsigned char* b=(unsigned char*)d;
  const unsigned char t0=b[0], t1=b[1], t2=b[2], t3=b[3];
  b[0]=b[7]; b[1]=b[7]; b[2]=b[5]; b[3]=b[4];
  b[7]=t0;   b[6]=t1;   b[5]=t2;   b[4]=t3;
}
#else
# define flt_swap16(d)
# define flt_swap32(d)
# define flt_swap64(d)
#endif

#define flt_offsetto(n,t)  ((int)( (unsigned char*)&((t*)(0))->n - (unsigned char*)(0) ))
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
typedef struct flt_end_desc
{
  flt_i8 bits;
  flt_i8 count;
  flt_u16 offs;
}flt_end_desc;

#pragma pack(push,1)
typedef struct flt_op
{ 
  flt_u16 op;
  flt_u16 length; 
}flt_op;

typedef struct flt_header
{
  flt_i8    ascii[8];
  flt_i32   format_rev;
  flt_i32   edit_rev;
  flt_i8    dtime[32];
  flt_i16   n_group_nid;
  flt_i16   n_lod_nid;
  flt_i16   n_obj_nid;
  flt_i16   n_face_nid;
  flt_i16   unit_mult;
  flt_i8    v_coords_units;
  flt_i8    texwhite_new_faces;
  flt_i32   flags;
  flt_i32   reserved0[6];
  flt_i32   proj_type;
  flt_i32   reserved1[7];
  flt_i16   n_dof_nid;
  flt_i16   v_storage_type;
  flt_i32   db_orig;
  double    sw_db_x;
  double    sw_db_y;
  double    d_db_x;
  double    d_db_y;
  flt_i16   n_sound_nid;
  flt_i16   n_path_nid;
  flt_i32   reserved2[2];
  flt_i16   n_clip_nid;
  flt_i16   n_text_nid;
  flt_i16   n_bsp_nid;
  flt_i16   n_switch_nid;
  flt_i32   reserved3;
  double    sw_corner_lat;
  double    sw_corner_lon;
  double    ne_corner_lat;
  double    ne_corner_lon;
  double    orig_lat;
  double    orig_lon;
  double    lbt_upp_lat;
  double    lbt_low_lat;
  flt_i16   n_lightsrc_nid;
  flt_i16   n_lightpnt_nid;
  flt_i16   n_road_nid;
  flt_i16   n_cat_nid;
  flt_i16   reserved4[4];
  flt_i32   earth_ellip_model;
  flt_i16   n_adapt_nid;
  flt_i16   n_curve_nid;
  flt_i16   utm_zone;
  flt_i8    reserved5[6];
  double    d_db_z;
  double    radius;
  flt_i16   n_mesh_nid;
  flt_i16   n_lightpnt_sys_nid;
  flt_i32   reserved6;
  double    earth_major_axis;
  double    earth_minor_axis;
}flt_header;
#pragma pack (pop)

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
bool flt_err(int err, flt* of, FILE* f);
bool flt_oph(flt_u16 op, flt_op* data, FILE* f);
bool flt_opcode(flt_u16 op, void* data, FILE* f);
void flt_swap_desc(void* data, flt_end_desc* desc);
void flt_swap_rec(void* data, flt_u16 op);
void flt_swap_op_header(void* data);
const char* flt_get_op_name(flt_u16 opcode);

////////////////////////////////////////////////////////////////////////////////////////////////
bool flt_err(int err, flt* of, FILE* f)
{
  of->errcode = err;
  if ( f )
    fclose(f);
  return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////
const char* flt_get_err_reason(int errcode)
{
#ifdef FLT_LONG_ERR_MESSAGES
  switch ( errcode )
  {
  case FLT_OK         : return "Ok"; 
  case FLT_ERR_FOPEN  : return "IO Error opening/reading file or not found"; 
  }
#else
  switch ( errcode )
  {
  case FLT_OK         : return "Ok"; 
  case FLT_ERR_FOPEN  : return "File error"; 
  }
#endif
  return "Unknown";
}

////////////////////////////////////////////////////////////////////////////////////////////////
bool flt_oph(flt_u16 op, flt_op* data, FILE* f)
{
  if ( fread(data,1,sizeof(flt_op),f) != sizeof(flt_op) )
    return false;
  flt_swap16(&data->op); flt_swap16(&data->length);
  return ( data->op == op );
}

////////////////////////////////////////////////////////////////////////////////////////////////
bool flt_opcode(flt_u16 op, void* data, FILE* f)
{
  flt_op oh;
  flt_u16 len;

  if  (!flt_oph(op,&oh,f) ) return false;
  len = oh.length>sizeof(flt_op)?oh.length-sizeof(flt_op):0;
  if ( fread(data, 1, len, f) != len) return false;
  flt_swap_rec(data,op);
  return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////
bool flt_load_from_filename(const char* filename, flt* of, flt_opts* opts)
{
  FILE* f;
  flt_header h;
  flt_op oh;
  flt_u16 leftLength;

  // opening file and reading header for version check
  f = fopen(filename, "rb");
  if ( !f ) return flt_err(FLT_ERR_FOPEN, of, f);
  if ( !flt_opcode(FLT_OP_HEADER, &h, f) )  return flt_err(FLT_ERR_OPREAD, of, f);
  if ( h.format_rev > FLT_VERSION )         return flt_err(FLT_ERR_VERSION, of, f);


  while ( !feof(f) )
  {
    flt_oph(FLT_OP_DONTCARE, &oh,f);     
    leftLength = oh.length>sizeof(flt_op) ? oh.length - sizeof(flt_op) : 0;
    if ( oh.op != 68 && oh.op != 69 && oh.op!=70 )
      printf( "(%d) %s\n", oh.op, flt_get_op_name(oh.op));
    fseek(f,leftLength,SEEK_CUR);
  }

  return flt_err(FLT_OK,of,f);
}

////////////////////////////////////////////////////////////////////////////////////////////////
void flt_swap_op_header(void* data)
{
  flt_end_desc  desc[]={ 
    {32, 2, flt_offsetto(format_rev,flt_header)},
    {16, 5, flt_offsetto(n_group_nid,flt_header)},
    {32,15, flt_offsetto(flags,flt_header)},
    {16, 2, flt_offsetto(n_dof_nid,flt_header)},
    {32, 1, flt_offsetto(db_orig,flt_header)},
    {64, 4, flt_offsetto(sw_db_x,flt_header)},
    {16, 2, flt_offsetto(n_sound_nid,flt_header)},
    {16, 4, flt_offsetto(n_clip_nid,flt_header)},
    {64, 8, flt_offsetto(sw_corner_lat,flt_header)},
    {16, 5, flt_offsetto(n_lightsrc_nid,flt_header)},
    {32, 1, flt_offsetto(earth_ellip_model,flt_header)},
    {16, 3, flt_offsetto(n_adapt_nid,flt_header)},
    {64, 2, flt_offsetto(d_db_z,flt_header)},
    {16, 2, flt_offsetto(n_mesh_nid,flt_header)},
    {64, 2, flt_offsetto(earth_major_axis,flt_header)},
    {NULL,NULL,NULL}
  };
  flt_swap_desc(data,desc);
}

void flt_swap_rec(void* data, flt_u16 op)
{
  switch (op)
  {
    case FLT_OP_HEADER: flt_swap_op_header(data); break;
    break;
  }
}
void flt_swap_desc(void* data, flt_end_desc* desc)
{
  unsigned char* _data = (unsigned char*)data;
  int i;
  unsigned int offs;

  while (desc->bits)
  {
    offs = desc->offs;
    for (i=0;i<desc->count;++i, offs+=desc->bits>>3)
    {
      switch(desc->bits)
      {
      case 16: flt_swap16( _data+offs ); break;
      case 32: flt_swap32( _data+offs ); break;
      case 64: flt_swap64( _data+offs ); break;
      }
    }
    desc++;
  }
};

const char* flt_get_op_name(flt_u16 opcode)
{
#ifndef FLT_NO_OPNAMES
  static const char* names[FLT_OP_MAX] = {
    "",                                                                   // 0
    "Header",                                                             // 1
    "Group",                                                              // 2
    "Level of Detail",                                                    // 3 (obsolete)
    "Object",                                                             // 4
    "Face",                                                               // 5
    "Vertex with ID",                                                     // 6 (obsolete)
    "Short Vertex w/o ID",                                                // 7 (obsolete) 
    "Vertex with Color",                                                  // 8 (obsolete)
    "Vertex with Color and Normal",                                       // 9 (obsolete)
    "Push Level",                                                         // 10
    "Pop Level",                                                          // 11
    "Translate",                                                          // 12 (obsolete)
    "Degree of Freedom",                                                  // 13 (obsolete)
    "Degree of Freedom",                                                  // 14
    "",                                                                   // 15
    "Instance Reference",                                                 // 16 (obsolete)  
    "Instance Definition",                                                // 17 (obsolete)
    "",                                                                   // 18
    "Push Subface",                                                       // 19
    "Pop Subface",                                                        // 20
    "Push Extension",                                                     // 21
    "Pop Extension",                                                      // 22
    "Continuation",                                                       // 23
    "", "", "", "", "", "", "",                                           // 24-30
    "Comment",                                                            // 31 
    "Color Palette",                                                      // 32
    "Long ID",                                                            // 33
    "", "", "", "", "", "",                                               // 34-39
    "Translate",                                                          // 40 (obsolete)
    "Rotate about Point",                                                 // 41 (obsolete)
    "Rotate about Edge",                                                  // 42 (obsolete)
    "Scale",                                                              // 43 (obsolete)
    "Translate",                                                          // 44 (obsolete)
    "Scale nonuniform",                                                   // 45 (obsolete)
    "Rotate about Point",                                                 // 46 (obsolete)
    "Rotate and/or Scale to Point",                                       // 47 (obsolete)
    "Put",                                                                // 48 (obsolete)
    "Matrix",                                                             // 49
    "Vector",                                                             // 50
    "Bounding Box",                                                       // 51 (obsolete)
    "Multitexture",                                                       // 52
    "UV List",                                                            // 53
    "",                                                                   // 54
    "Binary Separating Plane",                                            // 55
    "", "", "", "",                                                       // 56-59
    "Replicate",                                                          // 60
    "Instance Reference",                                                 // 61
    "Instance Definition",                                                // 62
    "External Reference",                                                 // 63
    "Texture Palette",                                                    // 64
    "Eyepoint Palete",                                                    // 65 (obsolete)
    "Material Palette",                                                   // 66 (obsolete)
    "Vertex Palette",                                                     // 67
    "Vertex with Color",                                                  // 68
    "Vertex with Color and Normal",                                       // 69
    "Vertex with Color, Normal and UV",                                   // 70
    "Vertex with Color and UV",                                           // 71
    "Vertex List",                                                        // 72
    "Level of Detail",                                                    // 73
    "Bounding Box",                                                       // 74
    "",                                                                   // 75
    "Rotate About Edge",                                                  // 76
    "Scale",                                                              // 77 (obsolete)
    "Translate",                                                          // 78
    "Scale",                                                              // 79
    "Rotate About Point",                                                 // 80
    "Rotate and/or Scale to Point",                                       // 81
    "Put",                                                                // 82
    "Eyepoint and Trackplane Palette",                                    // 83
    "Mesh",                                                               // 84
    "Local Vertex Pool",                                                  // 85
    "Mesh Primitive",                                                     // 86
    "Road Segment",                                                       // 87
    "Road Zone",                                                          // 88
    "Morph Vertex List",                                                  // 89
    "Linkage Palette",                                                    // 90
    "Sound",                                                              // 91
    "Road Path",                                                          // 92
    "Sound Palette",                                                      // 93
    "General Matrix",                                                     // 94
    "Text",                                                               // 95
    "Switch",                                                             // 96
    "Line Style Palette",                                                 // 97
    "Clip Region",                                                        // 98
    "",                                                                   // 99
    "Extension",                                                          // 100
    "Light Source",                                                       // 101
    "Light Source Palette",                                               // 102
    "Reserved",                                                           // 103
    "Reserved",                                                           // 104
    "Bounding Sphere",                                                    // 105
    "Bounding Cylinder",                                                  // 106
    "Bounding Convex Hull",                                               // 107
    "Bounding Volume Center",                                             // 108
    "Bounding Volume Orientation",                                        // 109
    "Reserved",                                                           // 110
    "Light Point",                                                        // 111
    "Texture Mapping Palette",                                            // 112
    "Material Palette",                                                   // 113
    "Name Table",                                                         // 114
    "Continuously Adaptive Terrain (CAT)",                                // 115
    "CAT Data",                                                           // 116
    "Reserved",                                                           // 117
    "Reserved",                                                           // 118
    "Bounding Histogram",                                                 // 119
    "Reserved",                                                           // 120
    "Reserved",                                                           // 121
    "Push Attribute",                                                     // 123
    "Pop Attribute",                                                      // 124
    "Reserved",                                                           // 125
    "Curve",                                                              // 126
    "Road Construction",                                                  // 127
    "Light Point Appearance Palette",                                     // 128
    "Light Point Animation Palette",                                      // 129
    "Indexed Light Point",                                                // 130
    "Light Point System",                                                 // 131
    "Indexed String",                                                     // 132
    "Shader Palette",                                                     // 133
    "Reserved",                                                           // 134
    "Extended Material Header",                                           // 135
    "Extended Material Ambient",                                          // 136
    "Extended Material Diffuse",                                          // 137
    "Extended Material Specular",                                         // 138
    "Extended Material Emissive",                                         // 139
    "Extended Material Alpha",                                            // 140
    "Extended Material Light Map",                                        // 141
    "Extended Material Normal Map",                                       // 142
    "Extended Material Bump Map",                                         // 143
    "Reserved",                                                           // 144
    "Extended Material Shadow Map",                                       // 145
    "Reserved",                                                           // 146
    "Extended Material Reflection Map",                                   // 147
    "Extension GUID Palette",                                             // 148
    "Extension Field Boolean",                                            // 149
    "Extension Field Integer",                                            // 150
    "Extension Field Float",                                              // 151
    "Extension Field Double",                                             // 152
    "Extension Field String",                                             // 153
    "Extension Field XML String",                                         // 154
  };
  return (opcode<=FLT_OP_MAX) ? names[opcode] : "Unknown";
#else
return "";
#endif
}

#endif
#endif