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

(Info)
- This single-hader library is intended to parse FLT files into memory in a thread-safe manner, 
  so you might call the public functions from different threads without problems.
- The Max Openflight Version supported is in FLT_GREATER_SUPPORTED_VERSION.
- Use the options to filter out the parsing and for other options, so the callbacks.

(Defines)
- Define flt_malloc/flt_calloc/flt_free/flt_strdup for custom memory management functions
- Define FLT_NO_OPNAMES to skip opcode name strings
- Define FLT_IMPLEMENTATION before including this in *one* C/CPP file to expand the implementation
- Define the supported version in FLT_VERSION. Maximum supported version is FLT_GREATER_SUPPORTED_VERSION
- Define FLT_NO_MEMOUT_CHECK to skip out-of-mem error

(Additional info)
- This library is thread-safe, all the data is in stack. If there's some static, it should be read-only const
- For the vertex palette, enough memory is reserved to avoid dynamic further allocations. This is done by
    dividing the size of the palette by the smallest possible vertex, to compute an upper bound of no. of
    vertices. Then allocate this no. of vertices for the bigger possible vertex. 
    Worst case of unused memory would be if all vertices in the palette are the smallest one.
- Nodes are usually stored in list

(Important ToDo)
- Parsing into callbacks, no memory allocated.
- 
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
#define FLT_ERR_MEMOUT 4
#define FLT_ERR_READBEYOND_REC 5

#define FLT_GREATER_SUPPORTED_VERSION 1640
#ifndef FLT_VERSION
#define FLT_VERSION FLT_GREATER_SUPPORTED_VERSION
#endif

#if FLT_VERSION > FLT_GREATER_SUPPORTED_VERSION
#error Openflight FLT version not supported. Max. supported version FLT_LAST_VERSION_SUPPORTED
#endif

// FLT Options
// palette flags (filter parsing of palettes)
#define FLT_OPT_PAL_NAMETABLE     (1<<0) // name table
#define FLT_OPT_PAL_COLOR         (1<<1) // color 
#define FLT_OPT_PAL_MATERIAL      (1<<2) // material
#define FLT_OPT_PAL_MATERIALEXT   (1<<3) // extended material
#define FLT_OPT_PAL_TEXTURE       (1<<4) // texture
#define FLT_OPT_PAL_TEXTUREMAPP   (1<<5) // texture mapping
#define FLT_OPT_PAL_SOUND         (1<<6) // sound
#define FLT_OPT_PAL_LIGHTSRC      (1<<7) // light source
#define FLT_OPT_PAL_LIGHTPNT      (1<<8) // light point
#define FLT_OPT_PAL_LIGHTPNTANIM  (1<<9) // light point animation
#define FLT_OPT_PAL_LINESTYLE     (1<<10) // line style
#define FLT_OPT_PAL_SHADER        (1<<11) // shader
#define FLT_OPT_PAL_EYEPNT_TCKPLN (1<<12) // eyepoint and trackplane
#define FLT_OPT_PAL_LINKAGE       (1<<13) // linkage
#define FLT_OPT_PAL_EXTGUID       (1<<14) // extension GUID
#define FLT_OPT_PAL_VERTEX        (1<<15) // vertex
#define FLT_OPT_PAL_VTX_SOURCE    (1<<16) // use original vertex format

//hierarchy/node flags (filter parsing of nodes and type of hierarchy)
#define FLT_OPT_HIE_HEADER        (1<<0) // read header
#define FLT_OPT_HIE_FLAT          (1<<1) // flat hierarchy (if skipped, will use full)
#define FLT_OPT_HIE_GROUP         (1<<2)
#define FLT_OPT_HIE_OBJECT        (1<<3)
#define FLT_OPT_HIE_MESH          (1<<4)
#define FLT_OPT_HIE_LIGHTPNT      (1<<5)
#define FLT_OPT_HIE_LIGHTPNTSYS   (1<<6)
#define FLT_OPT_HIE_DOF           (1<<7)
#define FLT_OPT_HIE_EXTREF        (1<<8)
#define FLT_OPT_HIE_LOD           (1<<9)
#define FLT_OPT_HIE_SOUND         (1<<10)
#define FLT_OPT_HIE_LIGHTSRC      (1<<11)
#define FLT_OPT_HIE_ROADSEG       (1<<12)
#define FLT_OPT_HIE_ROADCONST     (1<<13)
#define FLT_OPT_HIE_ROADPATH      (1<<14)
#define FLT_OPT_HIE_CLIPREG       (1<<15)
#define FLT_OPT_HIE_TEXT          (1<<16)
#define FLT_OPT_HIE_SWITCH        (1<<17)
#define FLT_OPT_HIE_CAT           (1<<18)
#define FLT_OPT_HIE_CURVE         (1<<19)
#define FLT_OPT_HIE_EXTREF_RESOLVE (1<<20) // unless this bit is set, ext refs aren't resolved

// Vertex semantic (you can use the original vertex format or force another, see flt_opts)
#define FLT_VTX_POSITION 0
#define FLT_VTX_COLOR 1
#define FLT_VTX_NORMAL 2
#define FLT_VTX_UV0 3

// FLT Record Opcodes
#define FLT_OP_DONTCARE 0
#define FLT_OP_HEADER 1
#define FLT_OP_PUSHLEVEL 10
#define FLT_OP_POPLEVEL 11
#define FLT_OP_PUSHSUBFACE 19
#define FLT_OP_POPSUBFACE 20
#define FLT_OP_PUSHEXTENSION 21
#define FLT_OP_POPEXTENSION 22
#define FLT_OP_COMMENT 31
#define FLT_OP_LONGID 33
#define FLT_OP_EXTREF 63
#define FLT_OP_PAL_TEXTURE 64
#define FLT_OP_PAL_VERTEX 67
#define FLT_OP_VERTEX_COLOR 68
#define FLT_OP_VERTEX_COLOR_NORMAL 69
#define FLT_OP_VERTEX_COLOR_NORMAL_UV 70
#define FLT_OP_VERTEX_COLOR_UV 71
#define FLT_OP_MAX 154

#ifdef __cplusplus
extern "C" {
#endif
  typedef unsigned char  fltu8;
  typedef unsigned short fltu16;
  typedef unsigned int   fltu32;
  typedef char  flti8;
  typedef short flti16;
  typedef int   flti32;

  typedef struct flt;
  typedef struct flt_opts;
  typedef struct flt_header;
  typedef struct flt_pal_tex;
  typedef struct flt_node_extref;
  typedef int (*flt_callback_texture)(struct flt_pal_tex* texpal, struct flt* of, void* user_data);
  typedef int (*flt_callback_extref)(struct flt_node_extref* extref, struct flt* of, void* user_data);
  

  // Load openflight information into of with given options
  int flt_load_from_filename(const char* filename, struct flt* of, struct flt_opts* opts);

  // Deallocates all memory
  void flt_release(struct flt* of);

  // Returns reason of the error code. Define FLT_LONG_ERR_MESSAGES for longer texts.
  const char* flt_get_err_reason(int errcode);

  // Encodes a vertex stream semantic word with the semantic and the offset
  fltu16 flt_vtx_stream_enc(fltu8 semantic, fltu8 offset, fltu8 size);

  // Decodes a vertex stream semantic word
  void flt_vtx_stream_dec(fltu16 stream, fltu8* semantic, fltu8* offset, fltu8* size);

  // Computes the size of a vertex stream in bytes
  fltu32 flt_vtx_size(fltu16* vstr);

  // Parsing options
  typedef struct flt_opts
  {
    fltu32 palflags;                          // palette flags        (FLT_OPT_PAL_*)
    fltu32 hieflags;                          // hierarchy/node flags (FLT_OPT_HIE_*)
    fltu16* vtxstream;                        // optional custom forced vertex format or null for original
    const char** search_paths;                // optional custom array of search paths ordered. last element should be null.
    flt_callback_extref   cb_extref;          // optional callback when an external ref is found
    flt_callback_texture  cb_texture;         // optional callback when a texture entry is found
    void* cb_user_data;                       // optional data to pass to callbacks
  }flt_opts;

  // Palettes information
  typedef struct flt_palettes
  {
    struct flt_pal_tex* tex_head;             // list of textures
    fltu32 tex_count;                         // no of textures
    fltu8* vtx;                               // buffer of consecutive vertices
    flti32* vtx_offsets;                      // two consecutive offset says the stride
    fltu32 vtx_count;                         // no. of vertices
  }flt_palettes;

  // Used when specified full hierarchy parsing (default)
  typedef struct flt_hie_full
  {
    int reserved;
  }flt_hie_full;

  // Used when specified flat hierarchy parsing (FLT_OPT_HIE_FLAT)
  typedef struct flt_hie_flat
  {
    struct flt_node_extref* extref_head;      // list of external references
    fltu32 extref_count;                      // external references count
  }flt_hie_flat;

  typedef struct flt
  {
    char* filename;                           // filename used to load or null if no load from file
    struct flt_header* header;                // header           (when FLT_OPT_HIE_HEADER)
    struct flt_palettes* pal;                 // palettes         (when any FLT_OPT_PAL_* flag is set)
    struct flt_hie_full* hie_full;            // hierarchy full   (default, only if any FLT_OPT_HIE_* is set)
    struct flt_hie_flat* hie_flat;            // hierarchy flat   (when FLT_OPT_HIE_FLAT)

    int errcode;                              // error code (see flt_get_err_reason)
    void* reserved;                           // internal parsing context data
  }flt;
  
  #pragma pack(push,1)
  typedef struct flt_op
  { 
    fltu16 op;
    fltu16 length; 
  }flt_op;

  typedef struct flt_pal_tex
  {
    char name[200];
    flti32 patt_ndx;
    flti32 xy_loc[2];  
    struct flt_pal_tex* next;
  }flt_pal_tex;

  typedef struct flt_node_extref
  {
    char    name[200];
    flti32  reserved0;
    flti32  flags;
    flti16  view_asbb;
    struct flt_node_extref* next;
    struct flt* of; // only resolved when FLT_OPT_HIE_EXTREF_RESOLVE
  }flt_node_extref;

  typedef struct flt_header
  {
    flti8    ascii[8];
    flti32   format_rev;
    flti32   edit_rev;
    flti8    dtime[32];
    flti16   n_group_nid;
    flti16   n_lod_nid;
    flti16   n_obj_nid;
    flti16   n_face_nid;
    flti16   unit_mult;
    flti8    v_coords_units;
    flti8    texwhite_new_faces;
    flti32   flags;
    flti32   reserved0[6];
    flti32   proj_type;
    flti32   reserved1[7];
    flti16   n_dof_nid;
    flti16   v_storage_type;
    flti32   db_orig;
    double   sw_db_x;
    double   sw_db_y;
    double   d_db_x;
    double   d_db_y;
    flti16   n_sound_nid;
    flti16   n_path_nid;
    flti32   reserved2[2];
    flti16   n_clip_nid;
    flti16   n_text_nid;
    flti16   n_bsp_nid;
    flti16   n_switch_nid;
    flti32   reserved3;
    double   sw_corner_lat;
    double   sw_corner_lon;
    double   ne_corner_lat;
    double   ne_corner_lon;
    double   orig_lat;
    double   orig_lon;
    double   lbt_upp_lat;
    double   lbt_low_lat;
    flti16   n_lightsrc_nid;
    flti16   n_lightpnt_nid;
    flti16   n_road_nid;
    flti16   n_cat_nid;
    flti16   reserved4[4];
    flti32   earth_ellip_model;
    flti16   n_adapt_nid;
    flti16   n_curve_nid;
    flti16   utm_zone;
    flti8    reserved5[6];
    double   d_db_z;
    double   radius;
    flti16   n_mesh_nid;
    flti16   n_lightpnt_sys_nid;
    flti32   reserved6;
    double   earth_major_axis;
    double   earth_minor_axis;
  }flt_header;
#pragma pack (pop)


#ifdef __cplusplus
};
#endif // cplusplus

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef FLT_IMPLEMENTATION

#if defined(flt_malloc) && defined(flt_free) && defined(flt_calloc) && defined(flt_strdup)
// ok
#elif !defined(flt_malloc) && !defined(flt_free) && !defined(flt_calloc) && !defined(flt_strdup)
// ok
#else
#error "Must define all or none of flt_malloc, flt_free, flt_calloc"
#endif

#ifndef flt_malloc
#define flt_malloc(sz) malloc(sz)
#endif
#ifndef flt_free
#define flt_free(p) free(p)
#endif
#ifndef flt_calloc
#define flt_calloc(count,size) calloc(count,size)
#endif
#ifndef flt_strdup
#define flt_strdup(s) strdup(s)
#endif

#define flt_safefree(p) { if (p){ flt_free(p); (p)=0;} }

#ifdef FLT_NO_MEMOUT_CHECK
#define flt_mem_check(p,e)
#define flt_mem_check2(p,e)
#else
#define flt_mem_check(p,e) { if ( !(p) ) { e = FLT_ERR_MEMOUT; return -1; } }
#define flt_mem_check2(p,of){ if ( !(p) ) return flt_err(FLT_ERR_MEMOUT,of); }
#endif

#if defined(WIN32) || defined(_WIN32) || (defined(sgi) && defined(unix) && defined(_MIPSEL)) || (defined(sun) && defined(unix) && !defined(_BIG_ENDIAN)) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN)) || (defined(__APPLE__) && defined(__LITTLE_ENDIAN__)) || (defined( _PowerMAXOS ) && (BYTE_ORDER == LITTLE_ENDIAN ))
#define FLT_LITTLE_ENDIAN
#elif (defined(sgi) && defined(unix) && defined(_MIPSEB)) || (defined(sun) && defined(unix) && defined(_BIG_ENDIAN)) || defined(vxw) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN)) || ( defined(__APPLE__) && defined(__BIG_ENDIAN__)) || (defined( _PowerMAXOS ) && (BYTE_ORDER == BIG_ENDIAN) )
#define FLT_BIG_ENDIAN
#else
#  error unknown endian type
#endif

////////////////////////////////////////////////////////////////////////////////////////////////
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

#define flt_offsetto(n,t)  ( (fltu8*)&((t*)(0))->n - (fltu8*)(0) )
#define FLT_RECORD_READER(name) flti32 name(flt_op* oh, flt* of)

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
typedef flti32 (*flt_rec_reader)(flt_op* oh, flt* of);
typedef struct flt_end_desc
{
  flti8 bits;
  flti8 count;
  fltu16 offs;
}flt_end_desc;

// context data used while parsing
typedef struct flt_internal
{
  FILE* f;
  flt_pal_tex* pal_tex_last;
  flt_node_extref* node_extref_last;
  flt_opts* opts;
  fltu32 vtx_offset;
  char* basepath;
  char tmpbuff[1024];
}flt_internal;


////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
int flt_err(int err, flt* of);
int flt_read_ophead(fltu16 op, flt_op* data, FILE* f);
void flt_swap_desc(void* data, flt_end_desc* desc);
const char* flt_get_op_name(fltu16 opcode);
fltu32 flt_vtx_write_sem(fltu8* outdata, fltu16* vstream, double* xyz, fltu32 abgr, float* normal, float* uv);
void flt_vtx_write_PC(flt* of, double* xyz, fltu32 abgr);
void flt_vtx_write_PCN(flt* of, double* xyz, fltu32 abgr, float* normal);
void flt_vtx_write_PCT(flt* of, double* xyz, fltu32 abgr, float* uv);
void flt_vtx_write_PCNT(flt* of, double* xyz, fltu32 abgr, float* uv, float* normal);
void flt_node_extref_add(flt* of, flt_node_extref* node);
void flt_release_hie_flat(flt_hie_flat* flat);
void flt_release_hie_full(flt_hie_full* full);
FILE* flt_fopen(const char* filename, flt* of);
char* flt_path_base(const char* filaname);
char* flt_path_basefile(const char* filename);
int flt_path_endsok(const char* filename);
#define flt_min(a,b) ((a)<(b)?(a):(b))
#define flt_max(a,b) ((a)>(b)?(a):(b))

FLT_RECORD_READER(flt_reader_header);                 // FLT_OP_HEADER
FLT_RECORD_READER(flt_reader_pal_tex);                // FLT_OP_PAL_TEXTURE
FLT_RECORD_READER(flt_reader_extref);                 // FLT_OP_EXTREF
FLT_RECORD_READER(flt_reader_pal_vertex);             // FLT_OP_PAL_VERTEX
FLT_RECORD_READER(flt_reader_vtx_color);              // FLT_OP_VERTEX_COLOR
FLT_RECORD_READER(flt_reader_vtx_color_normal);       // FLT_OP_VERTEX_COLOR_NORMAL
FLT_RECORD_READER(flt_reader_vtx_color_uv);           // FLT_OP_VERTEX_COLOR_UV
FLT_RECORD_READER(flt_reader_vtx_color_normal_uv);    // FLT_OP_VERTEX_COLOR_NORMAL_UV

static float flt_zerovec[4]={0};
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
int flt_err(int err, flt* of)
{
  flt_internal* fltint = (flt_internal*)of->reserved;
  of->errcode = err;
  if ( fltint ) 
  { 
    if ( fltint->f ) fclose(fltint->f); fltint->f=0; 
    flt_safefree(fltint->basepath);
  }
  if ( err != FLT_OK ) flt_release(of);
  return err;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
int flt_read_ophead(fltu16 op, flt_op* data, FILE* f)
{
  if ( fread(data,1,sizeof(flt_op),f) != sizeof(flt_op) )
    return 0;
  flt_swap16(&data->op); flt_swap16(&data->length);
  return op==FLT_OP_DONTCARE || data->op == op;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
int flt_load_from_filename(const char* filename, flt* of, flt_opts* opts)
{
  flt_op oh;
  int skipbytes;  
  flt_rec_reader optable[FLT_OP_MAX]={0};
  flt_internal fltint={0};
  char usepalette=0;

  // preparing internal obj
  memset(of,0,sizeof(flt));
  fltint.opts = opts;
  of->reserved = &fltint;  

  // opening file
  fltint.f = flt_fopen(filename, of);
  if ( !fltint.f ) return flt_err(FLT_ERR_FOPEN, of);    
  
  // configuring reading
  optable[FLT_OP_HEADER] = flt_reader_header;         // always read header
  optable[FLT_OP_PAL_VERTEX] = flt_reader_pal_vertex; // always read vertex palette header for skipping at least
  if ( opts->palflags & FLT_OPT_PAL_TEXTURE ) { optable[FLT_OP_PAL_TEXTURE] = flt_reader_pal_tex; usepalette=1; }  
  if ( opts->hieflags & FLT_OPT_HIE_EXTREF )  { optable[FLT_OP_EXTREF] = flt_reader_extref; }
  if ( opts->palflags & FLT_OPT_PAL_VERTEX ) // if reading vertices
  {
    optable[FLT_OP_VERTEX_COLOR] = flt_reader_vtx_color;
    optable[FLT_OP_VERTEX_COLOR_NORMAL] = flt_reader_vtx_color_normal;
    optable[FLT_OP_VERTEX_COLOR_UV] = flt_reader_vtx_color_uv;
    optable[FLT_OP_VERTEX_COLOR_NORMAL_UV] = flt_reader_vtx_color_normal_uv;
    usepalette=1;
  }
  if (usepalette)
  {
    of->pal = (flt_palettes*)flt_calloc(1,sizeof(flt_palettes));
    flt_mem_check2(of->pal,of);
  }

  // hierarchy mode
  if ( opts->hieflags & FLT_OPT_HIE_FLAT )
  {
    of->hie_flat = (flt_hie_flat*)flt_calloc(1,sizeof(flt_hie_flat));
    flt_mem_check2(of->hie_flat, of);
  }
  else
  {
    of->hie_full = (flt_hie_full*)flt_calloc(1,sizeof(flt_hie_full));
    flt_mem_check2(of->hie_full, of);
  }
  
  // reading loop
  while ( flt_read_ophead(FLT_OP_DONTCARE, &oh, fltint.f) )
  {
    // if reader function available, use it
    skipbytes = optable[oh.op] ? optable[oh.op](&oh, of) : oh.length-sizeof(flt_op);
    
    // if returned negative, it's an error, if positive, we skip until next record
    if ( skipbytes < 0 )        return flt_err(FLT_ERR_READBEYOND_REC,of);
    else if ( skipbytes > 0 )   fseek(fltint.f,skipbytes,SEEK_CUR); 
  }

  return flt_err(FLT_OK,of);
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_header)
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
    {0,0,0}
  };
  flt_internal* fltint=(flt_internal*)of->reserved;
  int readbytes=oh->length-sizeof(flt_op);
  flti32 format_rev=0;

  if ( fltint->opts->hieflags & FLT_OPT_HIE_HEADER ) 
  {
    // if storing header, read it entirely
    of->header = (flt_header*)flt_malloc(sizeof(flt_header));
    flt_mem_check(of->header, of->errcode);
    readbytes -= fread(of->header, 1, flt_min(readbytes,sizeof(flt_header)), fltint->f);  
    flt_swap_desc(of->header,desc); // endianess
    format_rev = of->header->format_rev;
  }
  else
  {
    // if no header needed, just read the version
    fseek(fltint->f,8,SEEK_CUR); readbytes-=8;
    readbytes -= fread(&format_rev,1,4,fltint->f);
    flt_swap32(&format_rev);
  }

  // checking version
  if ( format_rev > FLT_VERSION ) { readbytes = -1; of->errcode=FLT_ERR_VERSION; }
  return readbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_pal_tex)
{
  flt_end_desc desc[]={
    {32, 3, flt_offsetto(patt_ndx,flt_pal_tex)},
    {0,0,0}
  };
  int readbytes;
  flt_internal* fltint = (flt_internal*)of->reserved;
  flt_pal_tex* newpt = (flt_pal_tex*)flt_calloc(1,sizeof(flt_pal_tex));
  flt_mem_check(newpt, of->errcode);

  readbytes = oh->length-sizeof(flt_op);
  readbytes -= fread(newpt, 1, flt_min(readbytes,sizeof(flt_pal_tex)), fltint->f);
  flt_swap_desc(newpt, desc);
  if ( fltint->opts->cb_texture ) 
    fltint->opts->cb_texture(newpt,of,fltint->opts->cb_user_data); // callback?

  // adding node to list (use last if defined, or the beginning)
  if ( fltint->pal_tex_last )
    fltint->pal_tex_last->next = newpt;
  else
    of->pal->tex_head = newpt;

  fltint->pal_tex_last = newpt;
  ++of->pal->tex_count;
  return readbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_extref)
{
  char* basefile;
  flt_end_desc desc[]={
    {32, 1, flt_offsetto(flags,flt_node_extref)},
    {16, 1, flt_offsetto(view_asbb,flt_node_extref)},
    {0,0,0}
  };
  int readbytes;
  flt_internal* fltint = (flt_internal*)of->reserved;
  flt_node_extref* newextref = (flt_node_extref*)flt_calloc(1,sizeof(flt_node_extref));
  flt_mem_check(newextref, of->errcode);

  readbytes = oh->length-sizeof(flt_op);
  readbytes -= fread(newextref, 1, flt_min(readbytes,sizeof(flt_node_extref)), fltint->f);
  flt_swap_desc(newextref, desc);
  if ( fltint->opts->cb_extref ) 
    fltint->opts->cb_extref(newextref, of, fltint->opts->cb_user_data); // callback?

  // add node (that'll take care of hierarchy)
  flt_node_extref_add(of,newextref);

  // if resolving external references...
  if (fltint->opts->hieflags & FLT_OPT_HIE_EXTREF_RESOLVE )
  {
    newextref->of = (flt*)flt_malloc(sizeof(flt));
    flt_mem_check(newextref->of,of->errcode);

    // build new path name using basepath
    basefile = flt_path_basefile(newextref->name);
    if ( basefile )
    {
      *fltint->tmpbuff='\0';
      if ( fltint->basepath )
      {
        strcat(fltint->tmpbuff, fltint->basepath );
        if ( !flt_path_endsok(fltint->basepath) ) strcat(fltint->tmpbuff, "/");
        strcat(fltint->tmpbuff, basefile);
      }
      else
        strcpy(fltint->tmpbuff,newextref->name);
    }
    if ( flt_load_from_filename( fltint->tmpbuff, newextref->of, fltint->opts) != FLT_OK )
      flt_safefree(newextref->of);
    flt_safefree(basefile);
  }
  return readbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_pal_vertex)
{
  flt_internal* fltint=(flt_internal*)of->reserved;
  int readbytes, vmaxcount;
  fltu32 vtxsize;

  // record header
  fread(&vmaxcount,4,1,fltint->f);
  flt_swap32(&vmaxcount); 
  readbytes = oh->length - sizeof(flt_op) - 4;

  // read vertices from palette?
  if ( fltint->opts->palflags & FLT_OPT_PAL_VERTEX )
  {
    // no of vertices. Use smallest vtx record to have an upper bound of no. vertices in mem/file
    vmaxcount -= sizeof(flt_op)+4;
    vmaxcount /= 36; 

    vtxsize = flt_vtx_size(fltint->opts->vtxstream);

    // uses the source vertex format (is variable format)?
    if ( !vtxsize || (fltint->opts->palflags & FLT_OPT_PAL_VTX_SOURCE) )
    {
      fltint->opts->palflags |= FLT_OPT_PAL_VTX_SOURCE;
      // allocates for the biggest format size to be sure we have enough memory
      of->pal->vtx = (fltu8*)flt_malloc( vmaxcount * 48 ); 
      flt_mem_check(of->pal->vtx, of->errcode);
      // array of offsets. for the format of vertex i, it is the format to the associated vertex size (offset_next - offset)
      of->pal->vtx_offsets = (flti32*)flt_malloc( vmaxcount * sizeof(flti32) );
      flt_mem_check(of->pal->vtx_offsets, of->errcode);
    }
    else 
    {
      // force to use the passed in vertex stream format
      of->pal->vtx = (fltu8*)flt_malloc( vmaxcount * vtxsize );
      flt_mem_check(of->pal->vtx, of->errcode);
      // in the pointer of offsets stores the negative of the vertex size
      of->pal->vtx_offsets = (flti32*)(-(flti32)vtxsize);
    }
  }

  return readbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_vtx_color)
{
  flt_internal* fltint=(flt_internal*)of->reserved;
  int readbytes=oh->length-sizeof(flt_op);  
  double* coords; 
  flti32* abgr;

  // read data in
  readbytes -= fread(fltint->tmpbuff,1,flt_min(readbytes,36),fltint->f);
  coords=(double*)(fltint->tmpbuff+4); flt_swap64(coords); flt_swap64(coords+1); flt_swap64(coords+2);
  abgr=(flti32*)(fltint->tmpbuff+28); flt_swap32(abgr);

  // write  and advances count and offset
  flt_vtx_write_PC(of, coords, *abgr );

  return readbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_vtx_color_normal)
{
  flt_internal* fltint=(flt_internal*)of->reserved;
  int readbytes=oh->length-sizeof(flt_op);  
  double* coords;
  float* normal;
  flti32* abgr;

  // read data in
  readbytes -= fread(fltint->tmpbuff,1,flt_min(readbytes,52),fltint->f);
  coords=(double*)(fltint->tmpbuff+4); flt_swap64(coords); flt_swap64(coords+1); flt_swap64(coords+2);
  normal=(float*)(fltint->tmpbuff+28); flt_swap32(normal); flt_swap32(normal+1); flt_swap32(normal+2);
  abgr=(flti32*)(fltint->tmpbuff+40); flt_swap32(abgr);

  // write  and advances count and offset
  flt_vtx_write_PCN(of, coords, *abgr, normal);

  return readbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_vtx_color_uv)
{
  flt_internal* fltint=(flt_internal*)of->reserved;
  int readbytes=oh->length-sizeof(flt_op);  
  double* coords;
  float* uv;
  flti32* abgr;

  // read data in
  readbytes -= fread(fltint->tmpbuff,1,flt_min(readbytes,40),fltint->f);
  coords=(double*)(fltint->tmpbuff+4); flt_swap64(coords); flt_swap64(coords+1); flt_swap64(coords+2);
  uv=(float*)(fltint->tmpbuff+28); flt_swap32(uv); flt_swap32(uv+1);
  abgr=(flti32*)(fltint->tmpbuff+36); flt_swap32(abgr);

  // write  and advances count and offset
  flt_vtx_write_PCT(of, coords, *abgr, uv);

  return readbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_vtx_color_normal_uv)
{
  flt_internal* fltint=(flt_internal*)of->reserved;
  int readbytes=oh->length-sizeof(flt_op);  
  double* coords;
  float* uv;
  float* normal;
  flti32* abgr;

  // read data in
  readbytes -= fread(fltint->tmpbuff,1,flt_min(readbytes,60),fltint->f);
  coords=(double*)(fltint->tmpbuff+4); flt_swap64(coords); flt_swap64(coords+1); flt_swap64(coords+2);
  normal=(float*)(fltint->tmpbuff+28); flt_swap32(normal); flt_swap32(normal+1); flt_swap32(normal+2);
  uv=(float*)(fltint->tmpbuff+40); flt_swap32(uv); flt_swap32(uv+1);
  abgr=(flti32*)(fltint->tmpbuff+48); flt_swap32(abgr);

  // write  and advances count and offset
  flt_vtx_write_PCNT(of, coords, *abgr, uv, normal);

  return readbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_release(flt* of)
{
  flt_pal_tex* pt, *pn;

  if ( !of ) return;
  // header
  flt_safefree(of->filename);
  flt_safefree(of->header);

  // texture palette list
  if ( of->pal )
  {
    pt=pn=of->pal->tex_head; while (pt){ pn=pt->next; flt_free(pt); pt=pn; } 
    // vertex palette
    flt_safefree(of->pal->vtx);
    if ( (int)of->pal->vtx_offsets<-512 || (int)of->pal->vtx_offsets>0 ) 
      flt_safefree(of->pal->vtx_offsets); // be sure it does not encode a fixed vertex size (negative)
    flt_safefree(of->pal);
  }

  // hierarchy
  flt_release_hie_flat(of->hie_flat);
  flt_release_hie_full(of->hie_full);
  flt_safefree(of->hie_full);
  flt_safefree(of->hie_flat);
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_release_hie_flat(flt_hie_flat* flat)
{
  flt_node_extref *ec, *en;
  if ( !flat ) return;

  // external reference nodes
  ec=en=flat->extref_head; 
  while (ec)
  { 
    en=ec->next;     
    flt_release(ec->of); flt_safefree(ec->of); // resolved extref if applies
    flt_free(ec); 
    ec=en; 
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_release_hie_full(flt_hie_full* full)
{
  if ( !full ) return;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_node_extref_add(flt* of, flt_node_extref* node)
{
  flt_internal* fltint = (flt_internal*)of->reserved;
  if ( of->hie_full )
  {

  }
  else
  {
    if ( fltint->node_extref_last )
      fltint->node_extref_last->next = node; // if shortcut to last one
    else
      of->hie_flat->extref_head = node; // if starting list
    ++of->hie_flat->extref_count;
  }
  fltint->node_extref_last = node;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_vtx_write(flt* of, fltu16* stream, double* xyz, fltu32 abgr, float* uv, float* normal)
{
  flt_internal* fltint=(flt_internal*)of->reserved;  
  fltu8* vertex;

  if ( !stream ) stream = fltint->opts->vtxstream;
  // offset to current vertex
  vertex = of->pal->vtx + fltint->vtx_offset;

  // if format source used, update array and use the source stream
  // use source. gets offset to next vertex, saves pos+color, advance offset accordingly
  if ( fltint->opts->palflags & FLT_OPT_PAL_VTX_SOURCE )
    of->pal->vtx_offsets[of->pal->vtx_count] = fltint->vtx_offset;

  fltint->vtx_offset += flt_vtx_write_sem(vertex, stream, xyz, abgr, normal, uv);   
  ++of->pal->vtx_count;
}
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_vtx_write_PC(flt* of, double* xyz, fltu32 abgr)
{
  flt_internal* fltint=(flt_internal*)of->reserved;  
  // stream for POSITION/COLOR
  static fltu16 srcstream[]={1536, 4376, 0};
  fltu16* stream= ( fltint->opts->palflags & FLT_OPT_PAL_VTX_SOURCE ) ? srcstream : NULL;
  flt_vtx_write(of,stream,xyz,abgr,flt_zerovec, flt_zerovec);
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_vtx_write_PCN(flt* of, double* xyz, fltu32 abgr, float* normal)
{ 
  flt_internal* fltint=(flt_internal*)of->reserved;  
  // stream for POSITION/COLOR/NORMAL
  static fltu16 srcstream[]={1536, 4376, 8988, 0};
  fltu16* stream= ( fltint->opts->palflags & FLT_OPT_PAL_VTX_SOURCE ) ? srcstream : NULL;
  flt_vtx_write(of,stream,xyz,abgr,flt_zerovec,normal);
}
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_vtx_write_PCT(flt* of, double* xyz, fltu32 abgr, float* uv)
{
  flt_internal* fltint=(flt_internal*)of->reserved;  
  // stream for POSITION/COLOR/UV0
  static fltu16 srcstream[]={1536, 4376, 12828, 0};
  fltu16* stream= (fltint->opts->palflags & FLT_OPT_PAL_VTX_SOURCE ) ? srcstream : NULL;
  flt_vtx_write(of,stream,xyz,abgr,uv,flt_zerovec);
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_vtx_write_PCNT(flt* of, double* xyz, fltu32 abgr, float* uv, float* normal)
{
  flt_internal* fltint=(flt_internal*)of->reserved;  
  // stream for POSITION/COLOR/NORMAL/UV0
  static fltu16 srcstream[]={1536, 4376, 8988, 12840, 0};
  fltu16* stream=( fltint->opts->palflags & FLT_OPT_PAL_VTX_SOURCE ) ? srcstream : NULL;
  flt_vtx_write(of,stream,xyz,abgr,uv,normal);
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
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
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
const char* flt_get_op_name(fltu16 opcode)
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

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
const char* flt_get_err_reason(int errcode)
{
#ifdef FLT_LONG_ERR_MESSAGES
  switch ( errcode )
  {
  case FLT_OK         : return "Ok"; 
  case FLT_ERR_FOPEN  : return "IO Error opening/reading file or not found"; 
  case FLT_ERR_VERSION: return "Version error. Max. supported version FLT_GREATER_SUPPORTED_VERSION";
  case FLT_ERR_MEMOUT : return "Running out of memory. Malloc returns null";
  case FLT_ERR_READBEYOND_REC: return "Read beyond record. Skip bytes is negative. Version error?";
  }
#else
  switch ( errcode )
  {
  case FLT_OK         : return "Ok"; 
  case FLT_ERR_FOPEN  : return "File error"; 
  case FLT_ERR_VERSION: return "Version unsupported";
  case FLT_ERR_MEMOUT : return "Out of mem";
  case FLT_ERR_READBEYOND_REC: return "Read beyond record"; 
  }
#endif
  return "Unknown";
}

////////////////////////////////////////////////////////////////////////////////////////////////
// Encodes/decodes a vertex stream semantic word with the semantic, size and offset
// 4 msb for semantic, and 6 bits for size and 6 for offset
////////////////////////////////////////////////////////////////////////////////////////////////
fltu16 flt_vtx_stream_enc(fltu8 semantic, fltu8 offset, fltu8 size)
{
  return (fltu16)( (fltu16)(semantic<<12) | (fltu16)((size<<6)&0x0fc0) | (fltu16)(offset&0x003f) );
}

void flt_vtx_stream_dec(fltu16 sem, fltu8* semantic, fltu8* offset, fltu8* size)
{
  *semantic = (fltu8)(sem>>12);
  *size = (fltu8)( (sem&0x0fc0)>>6 );
  *offset=(fltu8)( sem&0x003f );
}

////////////////////////////////////////////////////////////////////////////////////////////////
// returns the number of bytes needed for the passed vertex stream format 
////////////////////////////////////////////////////////////////////////////////////////////////
fltu32 flt_vtx_size(fltu16* vstr)
{
  fltu8 sem,offs,size;
  fltu32 totalsize=0;

  if ( !vstr ) return 0;
  while (*vstr)
  {
    flt_vtx_stream_dec(*vstr,&sem,&offs,&size);
    totalsize+=size;
    ++vstr;
  }
  return totalsize;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
fltu32 flt_vtx_write_sem(fltu8* outdata, fltu16* vstream, double* xyz, fltu32 abgr, float* normal, float* uv)
{
  fltu32 tsize=0;
  fltu8 tgt_sem,offs,size;
  float* floats;
  int i;

  while (*vstream)
  {
    // this semantic
    flt_vtx_stream_dec(*vstream,&tgt_sem,&offs,&size);
    tsize+=size;
    switch(tgt_sem)
    {
    case FLT_VTX_POSITION: // support for float/double positions
      switch(size)
      {
      case 12: floats = (float*)(outdata+offs); for (i=0;i<3;++i) floats[i]=(float)xyz[i]; break; // expecting 3 floats
      case 16: floats = (float*)(outdata+offs); for (i=0;i<3;++i) floats[i]=(float)xyz[i]; floats[3]=1.0f; break; // expecting 4 floats
      case 24: memcpy(outdata+offs,xyz,24); break; // expecting 3 doubles (same as flt)
      case 32: memcpy(outdata+offs,xyz,24); *((double*)(outdata+offs+24)) = 1.0; break; // expecting 4 doubles
      }
      break;
    case FLT_VTX_COLOR : *(fltu32*)(outdata+offs)=abgr; break;
    case FLT_VTX_NORMAL: floats = (float*)(outdata+offs); for (i=0;i<3;++i) floats[i]=(float)normal[i]; break;
    case FLT_VTX_UV0   : floats = (float*)(outdata+offs); for (i=0;i<2;++i) floats[i]=(float)uv[i]; break;
    }
    ++vstream;
  }
  return tsize;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// extract base path from filename (caller should free the returned pointer)
////////////////////////////////////////////////////////////////////////////////////////////////
char* flt_path_base(const char* filename)
{
  char *ret, *ptr;
  int len;
  if ( !filename) return 0;

  len = strlen(filename);
  ret=flt_strdup(filename);
  ptr=ret+len;
  while (ptr!=ret)
  {
    if ( *ptr=='\\' || *ptr=='/' )
    {
      // weren't not last
      if ( ptr+1 != ret+len ) *(ptr+1)='\0';
      break;
    }
    --ptr;
  }
  return ret;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// try to open the filename, and if couldn't then uses the search paths...
////////////////////////////////////////////////////////////////////////////////////////////////
FILE* flt_fopen(const char* filename, flt* of)
{
  flt_internal* fltint=(flt_internal*)of->reserved;  
  fltu32 i=0;  
  char* basefile;
  const char* spath;

  strcpy(fltint->tmpbuff,filename);
  fltint->f = fopen(fltint->tmpbuff,"rb");
  if( !fltint->f && fltint->opts->search_paths ) // failed original, use search paths if valid
  {
    // get the base file name
    basefile = flt_path_basefile(filename);
    while ( basefile ) // infinite if base file, but breaking in
    {
      spath = fltint->opts->search_paths[i];
      if ( !spath ) break;
      *fltint->tmpbuff='\0'; // empty the target
      strcat(fltint->tmpbuff, spath);
      if ( !flt_path_endsok(spath) ) strcat(fltint->tmpbuff, "/" );      
      strcat(fltint->tmpbuff, basefile);
      fltint->f = fopen(fltint->tmpbuff,"rb");
      if (fltint->f) break;
      ++i;
    }
    flt_safefree(basefile);
  }

  // if open ok, saves correct filename and base path
  if ( fltint->f )
  {
    of->filename = flt_strdup(fltint->tmpbuff);  
    fltint->basepath = flt_path_base(fltint->tmpbuff);
  }
  return fltint->f;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// extract the filename from a path
////////////////////////////////////////////////////////////////////////////////////////////////
char* flt_path_basefile(const char* filename)
{
  const char *ptr;
  int len;

  if ( !filename ) return NULL;
  len = strlen(filename);
  ptr = filename + len;
  while ( ptr != filename )
  {
    if ( *ptr=='\\' || *ptr=='/' )
    {
      ++ptr;
      break;
    }
    --ptr;
  }

  if ( ptr==filename+len ) return NULL;
  return flt_strdup(ptr);  
}

////////////////////////////////////////////////////////////////////////////////////////////////
// returns 1 if path ends with / or \
////////////////////////////////////////////////////////////////////////////////////////////////
int flt_path_endsok(const char* filename)
{
  int len;
  const char* ptr;

  if ( !filename || !*filename ) return 0;
  len = strlen(filename);
  ptr = filename+len-1;
  while (ptr!=filename && (*ptr==' ' || *ptr=='\t') )
    --ptr;
  return (*ptr=='\\' || *ptr=='/') ? 1 : 0;
}

/*

struct nfltRecObject
{
  enum{OPCODE=4};
  char     name[8];
  int32_t  flags;
  int16_t  priority;
  int16_t  transparency;
  int16_t  sfx1;
  int16_t  sfx2;
  int16_t  significance;
  int16_t  reserved;
};

struct nfltRecFace
{
  enum{OPCODE=5};
  char      name[8];
  int32_t   irColorCode;
  int16_t   priority;
  char      drawType;
  char      texWhite;
  uint16_t  colorNameNdx;
  uint16_t  altColorNameNdx;
  char      reserved0;
  char      templateBBoard;
  int16_t   detailTexturePatternNdx;
  int16_t   texturePatternNdx;
  int16_t   materialNdx;
  int16_t   surfaceMaterialCode;
  int16_t   featureID;
  int32_t   irMaterialCode;
  uint16_t  transparency;
  char      lodGenControl;
  char      lineStyleNdx;
  int32_t   flags;
  unsigned char   lightMode;
  char      reserved1[7];
  uint32_t  primaryPackedABGR;
  uint32_t  alternatePackedABGR;
  int16_t   textureMappingNdx;
  int16_t   reserved2;
  uint32_t  primaryColorNdx;
  uint32_t  alternateColorNdx;
  int16_t   reserved3;
  int16_t   shaderNdx;
};


bool nfltIsOpcodeObsolete(unsigned short opcode)
{
return opcode==3 || (opcode>=6 && opcode<=9) || (opcode>=12 && opcode<=13)
|| (opcode>=16 && opcode<=17) || (opcode>=40 && opcode<=48) || opcode==51
|| (opcode>=65 && opcode<=66) || opcode==77;
}

*/

#endif
#endif