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
- This single-hader library is intended to parse FLT files into memory in a thread-safe way.
- The Max Openflight Version supported is in FLT_GREATER_SUPPORTED_VERSION.
- Use the options to filter out the parsing and for other options, so the callbacks.

(Multithread)
- For multithread purposes, a flt_context object is created and passed down during parsing
- So you might call the public functions from different threads without problems.
- You might use a callback for external references and then throw a thread task from there
  with a created new flt* object reusing the flt_context.dict object. 
- The flt_context.dict object can be shared among threads, it's contained with a critical section, and
  used to avoid reading same flt ref file twice.

(Defines)
- Define flt_malloc/flt_calloc/flt_free/flt_strdup for custom memory management functions
- Define FLT_NO_OPNAMES to skip opcode name strings
- Define FLT_IMPLEMENTATION before including this in *one* C/CPP file to expand the implementation
- Define the supported version in FLT_VERSION. Maximum supported version is FLT_GREATER_SUPPORTED_VERSION
- Define FLT_NO_MEMOUT_CHECK to skip out-of-mem error
- Define FLT_HASHTABLE_SIZE for a different default hash table size. (http://planetmath.org/goodhashtableprimes)
- Define FLT_STACKARRAY_SIZE for a different default stack array size (when passing flt_opts.stacksize=0)
- Define FLT_COMPACT_FACES for a smaller footprint version of face record with most important data. see flt_face.
- Define FLT_DICTFACES_SIZE for a different default hash table size for faces when passing flt_opts.dfaces_size=0.

(Additional info)
- This library is thread-safe, all the data is in stack. If there's some static, it should be read-only const
- For the vertex palette, enough memory is reserved to avoid dynamic further allocations. This is done by
    dividing the size of the palette by the smallest possible vertex, to compute an upper bound of no. of
    vertices. Then allocate this no. of vertices for the bigger possible vertex. 
    Worst case of unused memory would be if all vertices in the palette are the smallest one.
- 

(Important ToDo)
- Parsing into callbacks, no memory allocated. Parsing from memory or specific reader.
- Add memory pools for nodes, perhaps the client code can save stats after a first read and use that data for next reads.
- Callbacks for nodes before to get added to graph.
- 
*/

#ifndef _FLT_H_
#define _FLT_H_

/////////////////////

#ifdef _MSC_VER
#include <Windows.h>
#pragma warning(disable:4244 4100 4996 4706 4091)
#endif

/* 
 * Error codes
*/
#define FLT_OK 0
#define FLT_ERR_FOPEN 1
#define FLT_ERR_OPREAD 2
#define FLT_ERR_VERSION 3
#define FLT_ERR_MEMOUT 4
#define FLT_ERR_READBEYOND_REC 5
#define FLT_ERR_ALREADY 6

#define FLT_GREATER_SUPPORTED_VERSION 1640
#ifndef FLT_VERSION
#define FLT_VERSION FLT_GREATER_SUPPORTED_VERSION
#endif

#if FLT_VERSION > FLT_GREATER_SUPPORTED_VERSION
#error Openflight FLT version not supported. Max. supported version FLT_LAST_VERSION_SUPPORTED
#endif

//////////////////////////////////////////////////////////////////////////
// FLT OPTIONS
//////////////////////////////////////////////////////////////////////////
// palette flags (filter parsing of palettes)
#define FLT_OPT_PAL_NAMETABLE     (1<<0) // first node: name table
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
#define FLT_OPT_PAL_VERTEX        (1<<15) // last node: vertex

#define FLT_OPT_PAL_VTX_SOURCE    (1<<16) // use original vertex format
#define FLT_OPT_PAL_ALL           (0x7fff)// from bit 0 to 15

//hierarchy/node flags (filter parsing of nodes and type of hierarchy)
#define FLT_OPT_HIE_GROUP         (1<<0) // node first
#define FLT_OPT_HIE_OBJECT        (1<<1)
#define FLT_OPT_HIE_MESH          (1<<2)
#define FLT_OPT_HIE_LIGHTPNT      (1<<3)
#define FLT_OPT_HIE_LIGHTPNTSYS   (1<<4)
#define FLT_OPT_HIE_DOF           (1<<5)
#define FLT_OPT_HIE_EXTREF        (1<<6)
#define FLT_OPT_HIE_LOD           (1<<7)
#define FLT_OPT_HIE_SOUND         (1<<8)
#define FLT_OPT_HIE_LIGHTSRC      (1<<9)
#define FLT_OPT_HIE_ROADSEG       (1<<10)
#define FLT_OPT_HIE_ROADCONST     (1<<11)
#define FLT_OPT_HIE_ROADPATH      (1<<12)
#define FLT_OPT_HIE_CLIPREG       (1<<13)
#define FLT_OPT_HIE_TEXT          (1<<14)
#define FLT_OPT_HIE_SWITCH        (1<<15)
#define FLT_OPT_HIE_CAT           (1<<16)
#define FLT_OPT_HIE_CURVE         (1<<17) 
#define FLT_OPT_HIE_FACE          (1<<18) // node last
#define FLT_OPT_HIE_HEADER          (1<<20) // read header
#define FLT_OPT_HIE_NO_NAMES        (1<<21) // don't store names
#define FLT_OPT_HIE_EXTREF_RESOLVE  (1<<22) // unless this bit is set, ext refs aren't resolved
#define FLT_OPT_HIE_RESERVED        (1<<23) // not used
#define FLT_OPT_HIE_COMMENTS        (1<<24) // include comments 
#define FLT_OPT_HIE_ALL           (0x7ffff) // from bit 0 to 18


//////////////////////////////////////////////////////////////////////////
// Vertex semantic (you can use the original vertex format or force another, see flt_opts)
#define FLT_VTX_POSITION 0
#define FLT_VTX_COLOR 1
#define FLT_VTX_NORMAL 2
#define FLT_VTX_UV0 3

//////////////////////////////////////////////////////////////////////////
// FLT Record Opcodes
#define FLT_OP_DONTCARE 0
#define FLT_OP_HEADER 1
#define FLT_OP_GROUP 2
#define FLT_OP_OBJECT 4
#define FLT_OP_FACE 5
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
#define FLT_OP_VERTEX_LIST 72
#define FLT_OP_LOD 73
#define FLT_OP_MESH 84
#define FLT_OP_MAX 154

//////////////////////////////////////////////////////////////////////////
// FLT Node types
#define FLT_NODE_BASE   0
#define FLT_NODE_EXTREF 1 //FLT_OP_EXTREF
#define FLT_NODE_GROUP  2 //FLT_OP_GROUP
#define FLT_NODE_OBJECT 3 //FLT_OP_OBJECT
#define FLT_NODE_MESH   4 //FLT_OP_MESH
#define FLT_NODE_LOD    5 //FLT_OP_LOD
#define FLT_NODE_FACE   6 //FLT_OP_FACE
#define FLT_NODE_MAX    7 

#ifdef __cplusplus
extern "C" {
#endif
  typedef unsigned char  fltu8;
  typedef unsigned short fltu16;
  typedef unsigned int   fltu32;
  typedef char  flti8;
  typedef short flti16;
  typedef int   flti32;
  typedef volatile long fltatom32;

  typedef struct flt;
  typedef struct flt_opts;
  typedef struct flt_header;
  typedef struct flt_context;
  typedef struct flt_pal_tex;
  typedef struct flt_node;
  typedef struct flt_node_group;
  typedef struct flt_node_extref;
  typedef struct flt_node_object;
  typedef struct flt_node_lod;
  typedef struct flt_node_face;
  typedef struct flt_face;
  typedef int (*flt_callback_texture)(struct flt_pal_tex* texpal, struct flt* of, void* user_data);
  typedef int (*flt_callback_extref)(struct flt_node_extref* extref, struct flt* of, void* user_data);
  
    // Load openflight information into of with given options
  int flt_load_from_filename(const char* filename, struct flt* of, struct flt_opts* opts);

    // If the extref is already loaded, references it (inc ref count) and returns NULL. 
    // Otherwise, extref not loaded yet, creates a new flt for it and returns the pathname for the extref.
  char* flt_extref_prepare(struct flt_node_extref* extref, struct flt* of);

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
    fltu16 stacksize;                         // optional stack size (no of elements). 0 to use FLT_STACKARRAY_SIZE    
    fltu32 dfaces_size;                       // optional dict faces hash table size. 0 to use FLT_DICTFACES_SIZE

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

  typedef struct flt_hie
  {
    struct flt_node_extref* extref_head;      // list of external references    
    struct flt_node* node_root;               // 
    fltu16 extref_count;                      
    fltu32 node_count;
  }flt_hie;

  typedef struct flt
  {
    char* filename;                           // filename used to load or null if no load from file
    struct flt_header* header;                // header           (when FLT_OPT_HIE_HEADER)
    struct flt_palettes* pal;                 // palettes         (when any FLT_OPT_PAL_* flag is set)    
    struct flt_hie* hie;                      // hierarchy 
    fltatom32 ref;

    int errcode;                              // error code (see flt_get_err_reason)
    struct flt_context* ctx;                  // internal parsing context data (set null)
  }flt;
  
  typedef struct flt_op
  { 
    fltu16 op;
    fltu16 length; 
  }flt_op;

  typedef struct flt_pal_tex
  {
    char* name;
    flti32 patt_ndx;
    flti32 xy_loc[2];  
    struct flt_pal_tex* next;
  }flt_pal_tex;
  
  typedef struct flt_node
  {
    char*  name;
    struct flt_node* next;
    struct flt_node* child_head;
    struct flt_node* child_tail;  // last for shortcut adding
    fltu32 face_count;
    fltu32 index_count;
    fltu16 type;                  // one of FLT_NODE_*
    fltu16 child_count;           // number of children
  }flt_node;

  typedef struct flt_face
  {
#ifndef FLT_COMPACT_FACES
    flti32 ir_color;              // IR color code
    flti32 ir_mat;                // IR material code
    fltu32 abgr;                  // packed color
    flti16 texbase_pat;           // texture index
    flti16 texdetail_pat;         // detail texture index
    flti16 mat_pat;               // material index
    flti16 smc_id;                // surface material code
    flti16 feat_id;               // feature id
    flti16 transp;                // 0 opaque, 65536 clear
    fltu16 cname_ndx;             // color index
    fltu16 cnamealt_ndx;          // color secondary index
    fltu16 texmapp_ndx;           // texture mapping index
    fltu16 shader_ndx;            // shader index
    fltu16 flags;                 // terrain/no color/no altcolor/pck color/terrain cutout/hidden/roofline
    fltu8 draw_type;              // 0..10
    fltu8 billb;                  //
    fltu8 light_mode;
    fltu8 lod_gen;
    fltu8 linestyle_ndx;
#else
    fltu32 abgr;                  
    flti16 texbase_pat;           
    flti16 texdetail_pat;         
    flti16 mat_pat;                   
    fltu16 shader_ndx;            
    fltu16 flags;                    
    fltu8 billb;                  
#endif
  }flt_face;

  typedef struct flt_node_extref
  {
    struct flt_node base;
    struct flt_node_extref* next_extref;
    struct flt* of; // only resolved when FLT_OPT_HIE_EXTREF_RESOLVE
    flti32  flags;
    flti16  view_asbb;
  }flt_node_extref;

  typedef struct flt_node_object
  {
    struct flt_node base;
    flti16 flags;
    flti16 priority;
    flti16 transp;
  }flt_node_object;

  typedef struct flt_node_group
  {
    struct flt_node base;    
    fltu32 loop_count;
    float loop_dur;
    float last_fr_dur;
    flti16 priority;
    flti16 flags;
  }flt_node_group;

  typedef struct flt_node_mesh
  {
    struct flt_node base;

  }flt_node_mesh;

  typedef struct flt_node_lod
  {
    struct flt_node base;
    double switch_in;
    double switch_out;
    double cnt_coords[3];
    double trans_range;
    double sig_size;
    flti32 flags;
  }flt_node_lod;

  typedef struct flt_node_face
  {
    struct flt_node base;
    fltu32 face_hash; // used to access flt_context.dictfaces
  }flt_node_face;

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

#ifdef __cplusplus
};
#endif // cplusplus

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef FLT_IMPLEMENTATION

#if defined(_DEBUG) || defined(DEBUG)
#ifdef _MSC_VER
#define FLT_BREAK { __debugbreak(); }
#else
#define FLT_BREAK { raise(SIGTRAP); }
#endif
#else
#define FLT_BREAK {(void*)0;}
#endif

// Memory functions override
#if defined(flt_malloc) && defined(flt_free) && defined(flt_calloc) && defined(flt_strdup) && defined(flt_realloc)
// ok, all defined
#elif !defined(flt_malloc) && !defined(flt_free) && !defined(flt_calloc) && !defined(flt_strdup) && !defined(flt_realloc)
// ok, none defined
#else
#error "Must define all or none of flt_malloc, flt_free, flt_calloc, flt_realloc, flt_strdup"
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
#ifndef flt_realloc
#define flt_realloc(p,sz) realloc(p,sz)
#endif

#define flt_safefree(p) { if (p){ flt_free(p); (p)=0;} }

#ifdef FLT_NO_MEMOUT_CHECK
#define flt_mem_check(p,e)
#define flt_mem_check2(p,e)
#else
#define flt_mem_check(p,e) { if ( !(p) ) { e = FLT_ERR_MEMOUT; return -1; } }
#define flt_mem_check2(p,of){ if ( !(p) ) return flt_err(FLT_ERR_MEMOUT,of); }
#endif

#define flt_min(a,b) ((a)<(b)?(a):(b))
#define flt_max(a,b) ((a)>(b)?(a):(b))
#define FLT_NULL (0)
#define FLT_TRUE (1)
#define FLT_FALSE (0)
#define flt_optional 

#ifndef FLT_HASHTABLE_SIZE 
#define FLT_HASHTABLE_SIZE 3079
#endif

#ifndef FLT_STACKARRAY_SIZE
#define FLT_STACKARRAY_SIZE 32
#endif

#ifndef FLT_ARRAY_INITCAP
#define FLT_ARRAY_INITCAP 1024
#endif

#ifndef FLT_DICTFACES_SIZE 
#define FLT_DICTFACES_SIZE 6151 
//#define FLT_DICTFACES_SIZE 12289
//#define FLT_DICTFACES_SIZE 24593
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
#define flt_getswapu32(dst,offs) { u32=(fltu32*)(ctx->tmpbuff+offs); flt_swap32(u32); (dst)=*u32; }
#define flt_getswapi32(dst,offs) { i32=(flti32*)(ctx->tmpbuff+offs); flt_swap32(i32); (dst)=*i32; }
#define flt_getswapu16(dst,offs) { u16=(fltu16*)(ctx->tmpbuff+offs); flt_swap16(u16); (dst)=*u16; }
#define flt_getswapi16(dst,offs) { i16=(flti16*)(ctx->tmpbuff+offs); flt_swap16(i16); (dst)=*i16; }
#define flt_getswapflo(dst,offs) { flo=(float*)(ctx->tmpbuff+offs); flt_swap32(flo); (dst)=*flo; }
#define flt_getswapdbl(dst,offs) { dbl=(double*)(ctx->tmpbuff+offs); flt_swap64(dbl); (dst)=*dbl; }

////////////////////////////////////////////////////////////////////////////////////////////////
// Internal structures
////////////////////////////////////////////////////////////////////////////////////////////////
typedef flti32 (*flt_rec_reader)(flt_op* oh, flt* of);
typedef struct flt_end_desc
{
  flti8 bits;
  flti8 count;
  fltu16 offs;
}flt_end_desc;

// context data used while parsing
typedef struct flt_context
{
  char tmpbuff[256];
  FILE* f;
  struct flt_pal_tex* pal_tex_last;
  struct flt_node_extref* node_extref_last;
  struct flt_opts* opts;
  struct flt_dict* dict;
  struct flt_dict* dictfaces;
  struct flt_stack* stack;
  char* basepath;
  fltu32 vtx_offset;
  fltu32 rec_count;
  fltu32 cur_depth;
  fltu16 op_last;        // immediate last  
}flt_context;

////////////////////////////////////////////////
// Critical section / Atomic operations
////////////////////////////////////////////////
typedef struct flt_critsec;
struct flt_critsec* flt_critsec_create();
void flt_critsec_destroy(struct flt_critsec* cs);
void flt_critsec_enter(struct flt_critsec* cs);
void flt_critsec_leave(struct flt_critsec* cs);
long flt_atomic_dec(fltatom32* c);
long flt_atomic_inc(fltatom32* c);

////////////////////////////////////////////////
// Dictionary 
////////////////////////////////////////////////
typedef unsigned long (*flt_dict_hash)(const unsigned char*, int);
typedef int (*flt_dict_keycomp)(const char*, const char*, int);
typedef struct flt_dict_node
{
  char* key;
  unsigned long keyhash;
  void* value;
  struct flt_dict_node* next;
}flt_dict_node;

typedef struct flt_dict
{
  struct flt_dict_node** hasht;
  struct flt_critsec* cs;
  flt_dict_hash hashf;
  flt_dict_keycomp keycomp;
  fltatom32 ref;
  int capacity; // no of entries in hash table
  int count; // no of actual elements
}flt_dict;

unsigned long flt_dict_hash_djb2(const unsigned char* str, int extra);
unsigned long flt_dict_hash_face_djb2(const unsigned char* str, int size);
int flt_dict_keycomp_string(const char* a, const char* b, int extra);
int flt_dict_keycomp_face(const char* a, const char* b, int size);
void flt_dict_create(int capacity, int create_cs, flt_dict** dict, flt_dict_hash hashf, flt_dict_keycomp kcomp);
void flt_dict_destroy(flt_dict** dict, int free_elms);
void* flt_dict_get(flt_dict* dict, const char* key, flt_optional int size, flt_optional unsigned long hash);
flt_dict_node* flt_dict_create_node(const char* key, unsigned long keyhash, void* value, int size);
int flt_dict_insert(flt_dict* dict, const char* key, void* value, flt_optional int size, flt_optional unsigned long hash);

////////////////////////////////////////////////
// Stack
////////////////////////////////////////////////
typedef struct flt_stack 
{
  flt_node** entries; // fixed size array (enough with a good upper bound estimation)
  int count;
  int capacity;
}flt_stack;

int flt_stack_create(flt_stack** s, int capacity);
void flt_stack_destroy(flt_stack** s);
void flt_stack_clear(flt_stack* s);
void flt_stack_push(flt_stack* s, flt_node* value);
flt_node* flt_stack_top(flt_stack* s);
flt_node* flt_stack_top_not_null(flt_stack* s);
flt_node* flt_stack_pop(flt_stack* s);
void flt_stack_set_top(flt_stack* s, flt_node* value);

////////////////////////////////////////////////
// Dynamic Array
////////////////////////////////////////////////
struct flt_array;
typedef void (*flt_array_growpolicy)(flt_array*);
typedef struct flt_array
{
  void** data;
  int size;
  int capacity;
  flt_array_growpolicy growpolicy;
}flt_array;

int flt_array_create(flt_array** arr, int capacity, flt_array_growpolicy grow);
void flt_array_destroy(flt_array** arr);
void flt_array_push_back(flt_array* arr, void* elem);
void* flt_array_pop_back(flt_array* arr);
void* flt_array_at(flt_array* arr, int index);
void flt_array_clear(flt_array* arr);
void flt_array_grow_double(flt_array* arr);

////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declarations of functions
////////////////////////////////////////////////////////////////////////////////////////////////
int flt_err(int err, flt* of);
int flt_read_ophead(fltu16 op, flt_op* data, FILE* f);
void flt_swap_desc(void* data, flt_end_desc* desc);
const char* flt_get_op_name(fltu16 opcode);
fltu32 flt_vtx_write_sem(fltu8* outdata, fltu16* vstream, double* xyz, fltu32 abgr, float* normal, float* uv);
void flt_vtx_write(flt* of, fltu16* stream, double* xyz, fltu32 abgr, float* uv, float* normal);
void flt_node_add(flt* of, flt_node* node);
void flt_node_add_child(flt_node* parent, flt_node* node);
FILE* flt_fopen(const char* filename, flt* of);
char* flt_path_base(const char* filaname);
char* flt_path_basefile(const char* filename);
int flt_path_endsok(const char* filename);
flt_node* flt_node_create(flt* of, int nodetype, const char* name);
void flt_release_node(flt_node* n);
void flt_resolve_all_extref(flt* of);

FLT_RECORD_READER(flt_reader_header);                 // FLT_OP_HEADER
FLT_RECORD_READER(flt_reader_pushlv);                 // FLT_OP_PUSH_LEVEL
FLT_RECORD_READER(flt_reader_face);                   // FLT_OP_FACE
FLT_RECORD_READER(flt_reader_poplv);                  // FLT_OP_POP_LEVEL
FLT_RECORD_READER(flt_reader_group);                  // FLT_OP_GROUP
FLT_RECORD_READER(flt_reader_lod);                    // FLT_OP_LOD
FLT_RECORD_READER(flt_reader_mesh);                   // FLT_OP_MESH
FLT_RECORD_READER(flt_reader_object);                 // FLT_OP_OBJECT
FLT_RECORD_READER(flt_reader_pal_tex);                // FLT_OP_PAL_TEXTURE
FLT_RECORD_READER(flt_reader_longid);                 // FLT_OP_LONGID
FLT_RECORD_READER(flt_reader_extref);                 // FLT_OP_EXTREF
FLT_RECORD_READER(flt_reader_pal_vertex);             // FLT_OP_PAL_VERTEX
FLT_RECORD_READER(flt_reader_vtx_color);              // FLT_OP_VERTEX_COLOR
FLT_RECORD_READER(flt_reader_vtx_color_normal);       // FLT_OP_VERTEX_COLOR_NORMAL
FLT_RECORD_READER(flt_reader_vtx_color_uv);           // FLT_OP_VERTEX_COLOR_UV
FLT_RECORD_READER(flt_reader_vtx_color_normal_uv);    // FLT_OP_VERTEX_COLOR_NORMAL_UV
FLT_RECORD_READER(flt_reader_vertex_list);            // FLT_OP_VERTEX_LIST
static float flt_zerovec[4]={0};
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
int flt_err(int err, flt* of)
{
  flt_context* ctx = of->ctx;

  of->errcode = err;
  if ( ctx ) 
  { 
    if ( ctx->f ) fclose(ctx->f); ctx->f=0;
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

fltatom32 TOTALNFACES=0;
fltatom32 TOTALUNIQUEFACES=0; 
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void print_i(int d){ while ((d--)>0) printf( "    "); }

int flt_load_from_filename(const char* filename, flt* of, flt_opts* opts)
{
  flt_op oh;
  flti32 skipbytes;  
  flt_rec_reader readtab[FLT_OP_MAX]={0};
  fltu32 countable[FLT_OP_MAX]={0};
  flt_context* ctx;
  char use_pal=0, use_node=0;  

  // preparing internal obj (references and shared dict)
  ctx = (flt_context*)flt_calloc(1,sizeof(flt_context));
  ctx->opts = opts;  
  if (of->ctx ) // reuse some stuff from input of->context
  {
    if ( of->ctx->dict )
    {
      ctx->dict = of->ctx->dict;
      flt_atomic_inc(&of->ctx->dict->ref);
    }
    ctx->cur_depth = of->ctx->cur_depth;
  }
  of->ctx = ctx;
  if ( !ctx->dict ) // if not sharing dict
    flt_dict_create(FLT_HASHTABLE_SIZE,1,&ctx->dict, flt_dict_hash_djb2, flt_dict_keycomp_string);
  flt_atomic_inc(&of->ref); // increments this of reference
  flt_stack_create(&ctx->stack,ctx->opts->stacksize); // creates nodes stack
  flt_dict_create(opts->dfaces_size ? opts->dfaces_size: FLT_DICTFACES_SIZE,0,&ctx->dictfaces,flt_dict_hash_face_djb2, flt_dict_keycomp_face);
  
  // opening file
  ctx->f = flt_fopen(filename, of);
  if ( !ctx->f ) return flt_err(FLT_ERR_FOPEN, of);

  //printf( "%s\n", filename );
  // configuring reading
  readtab[FLT_OP_HEADER] = flt_reader_header;         // always read header
  readtab[FLT_OP_PAL_VERTEX] = flt_reader_pal_vertex; // always read vertex palette header for skipping at least
  if ( opts->palflags & FLT_OPT_PAL_TEXTURE ) { readtab[FLT_OP_PAL_TEXTURE] = flt_reader_pal_tex; use_pal=FLT_TRUE; }  
  if ( opts->hieflags & FLT_OPT_HIE_EXTREF )  { readtab[FLT_OP_EXTREF] = flt_reader_extref; use_node=FLT_TRUE; }
  if ( opts->hieflags & FLT_OPT_HIE_OBJECT )  { readtab[FLT_OP_OBJECT] = flt_reader_object; use_node=FLT_TRUE; }
  if ( opts->hieflags & FLT_OPT_HIE_GROUP )   { readtab[FLT_OP_GROUP] = flt_reader_group; use_node=FLT_TRUE; }
  if ( opts->hieflags & FLT_OPT_HIE_LOD )     { readtab[FLT_OP_LOD] = flt_reader_lod; use_node=FLT_TRUE; }
  if ( opts->hieflags & FLT_OPT_HIE_MESH )    { readtab[FLT_OP_MESH] = flt_reader_mesh; use_node=FLT_TRUE; }
  if ( opts->hieflags & FLT_OPT_HIE_FACE )    { readtab[FLT_OP_FACE] = flt_reader_face; readtab[FLT_OP_VERTEX_LIST]= flt_reader_vertex_list; use_node=FLT_TRUE;}
  if ( opts->palflags & FLT_OPT_PAL_VERTEX ) // if reading vertices
  {
    readtab[FLT_OP_VERTEX_COLOR] = flt_reader_vtx_color;
    readtab[FLT_OP_VERTEX_COLOR_NORMAL] = flt_reader_vtx_color_normal;
    readtab[FLT_OP_VERTEX_COLOR_UV] = flt_reader_vtx_color_uv;
    readtab[FLT_OP_VERTEX_COLOR_NORMAL_UV] = flt_reader_vtx_color_normal_uv;
    use_pal=FLT_TRUE;
  }

  if (use_pal)
  {
    of->pal = (flt_palettes*)flt_calloc(1,sizeof(flt_palettes));
    flt_mem_check2(of->pal,of);
  }

  if (use_node)
  {
    if (!(ctx->opts->hieflags & FLT_OPT_HIE_NO_NAMES))
      readtab[FLT_OP_LONGID] = flt_reader_longid;
    readtab[FLT_OP_PUSHLEVEL] = flt_reader_pushlv;
    readtab[FLT_OP_POPLEVEL] = flt_reader_poplv;

    // hierarchy (root should be always)
    of->hie = (flt_hie*)flt_calloc(1,sizeof(flt_hie));
    flt_mem_check2(of->hie, of);
    of->hie->node_root = flt_node_create(of, FLT_NODE_BASE, "root");
    flt_mem_check2(of->hie->node_root, of);
    flt_stack_push(ctx->stack, of->hie->node_root);
  }

  // ---------------------------- TEMP
  if (opts->hieflags & FLT_OPT_HIE_RESERVED) { print_i(ctx->cur_depth); printf( "Begin Extref %s\n", of->filename ); }

  // Reading Loop!
  while ( flt_read_ophead(FLT_OP_DONTCARE, &oh, ctx->f) )
  {
    // ---------------------------- TEMP
    {
      ++countable[oh.op];
      if (opts->hieflags & FLT_OPT_HIE_RESERVED)
      {
        if ( oh.op == FLT_OP_PUSHLEVEL )
        {
          print_i(ctx->cur_depth);
          ++ctx->cur_depth;
        }
        else if ( oh.op == FLT_OP_POPLEVEL ) 
        {
          --ctx->cur_depth;
          print_i(ctx->cur_depth);
        }
        else print_i(ctx->cur_depth);
        printf( "(%03d) %s\n", oh.op, flt_get_op_name(oh.op) );
      }
    } // ---------------------------- TEMP

    ++ctx->rec_count;
    // if reader function available, use it
    skipbytes = readtab[oh.op] ? readtab[oh.op](&oh, of) : oh.length-sizeof(flt_op);    
    ctx->op_last = oh.op;

    // if returned negative, it's an error, if positive, we skip until next record
    if ( skipbytes < 0 )        return flt_err(FLT_ERR_READBEYOND_REC,of);
    else if ( skipbytes > 0 )   fseek(ctx->f,skipbytes,SEEK_CUR); 
  }

  if (opts->hieflags & FLT_OPT_HIE_EXTREF_RESOLVE && of->hie)
    flt_resolve_all_extref(of);  

  // ---------------------------- TEMP
  if (opts->hieflags & FLT_OPT_HIE_RESERVED) { print_i(ctx->cur_depth); printf( "End Extref %s\n", of->filename ); }

  // TEMPORARY
//   {
//     printf( "%s\n", of->filename );
//     for (oh.op=0;oh.op<FLT_OP_MAX;++oh.op)
//     {
//       if ( countable[oh.op] )
//         printf( "(%02d) %s : %d\n", oh.op, flt_get_op_name(oh.op), countable[oh.op] );
//     }
//   }

  flt_stack_pop(ctx->stack); // root
  flt_stack_destroy(&ctx->stack); // no needed anymore (also released in flt_release)
  return flt_err(FLT_OK,of);
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
char* flt_extref_prepare(struct flt_node_extref* extref, struct flt* of)
{
  char* basefile=FLT_NULL;
  flt_context* oldctx = of->ctx;

  if ( !extref || !of ) 
    return FLT_NULL;

  // check if reference has been loaded already
  extref->of = (struct flt*)flt_dict_get(of->ctx->dict, extref->base.name, FLT_NULL, FLT_NULL);
  if ( !extref->of ) // does not exist, creates one, register in dict and passes dict
  {
    extref->of = (flt*)flt_calloc(1,sizeof(flt));
    extref->of->ctx = of->ctx; 
    flt_dict_insert(of->ctx->dict, extref->base.name, extref->of, FLT_NULL, FLT_NULL);

    // Creates the full path for the external reference (uses same base path as parent)
    basefile = flt_path_basefile(extref->base.name);  
    if ( basefile )
    {
      *oldctx->tmpbuff=0; // thread-safety: resolve has to be called from parent thread who found the extref
      if ( oldctx->basepath )
      {
        strcat(oldctx->tmpbuff, oldctx->basepath );
        if ( !flt_path_endsok(oldctx->basepath) ) strcat(oldctx->tmpbuff, "/");
        strcat(oldctx->tmpbuff, basefile);
      }
      else
        strcpy(oldctx->tmpbuff,extref->base.name);
    }

    basefile = (char*)flt_realloc(basefile,strlen(oldctx->tmpbuff)+1);    
    strcpy(basefile,oldctx->tmpbuff);
  }
  else 
  {
    // file already loaded. uses and inc reference count
    flt_atomic_inc(&extref->of->ref);
  }

  
  return basefile;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_resolve_all_extref(flt* of)
{
  flt_context* ctx=of->ctx;
  flt_opts* opts=ctx->opts;
  char* basefile;
  flt_node_extref* extref=of->hie->extref_head;

  while(extref)
  {
    basefile = flt_extref_prepare(extref,of);
    if ( basefile )
    {
      if ( flt_load_from_filename( basefile, extref->of, opts) != FLT_OK )
        flt_safefree(extref->of);
      flt_safefree(basefile);
    }
    extref=(flt_node_extref*)extref->next_extref;
  }
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
  flt_context* ctx=of->ctx;
  int leftbytes=oh->length-sizeof(flt_op);
  flti32 format_rev=0;

  if ( ctx->opts->hieflags & FLT_OPT_HIE_HEADER ) 
  {
    // if storing header, read it entirely
    of->header = (flt_header*)flt_malloc(sizeof(flt_header));
    flt_mem_check(of->header, of->errcode);
    leftbytes -= (int)fread(of->header, 1, flt_min(leftbytes,sizeof(flt_header)), ctx->f);  
    flt_swap_desc(of->header,desc); // endianess
    format_rev = of->header->format_rev;
  }
  else
  {
    // if no header needed, just read the version
    fseek(ctx->f,8,SEEK_CUR); leftbytes-=8;
    leftbytes -= (int)fread(&format_rev,1,4,ctx->f);
    flt_swap32(&format_rev);
  }

  // checking version
  if ( format_rev > FLT_VERSION ) 
  { 
    leftbytes = -1; 
    of->errcode=FLT_ERR_VERSION; 
  }

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_pushlv)
{
  flt_context* ctx=of->ctx;
  int leftbytes = oh->length-sizeof(flt_op);

  flt_stack_push(ctx->stack,FLT_NULL);

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_poplv)
{
  flt_context* ctx=of->ctx;
  int leftbytes = oh->length-sizeof(flt_op);  
  flt_stack_pop(ctx->stack);

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_pal_tex)
{
  int leftbytes=oh->length-sizeof(flt_op);
  flt_context* ctx = of->ctx;
  flt_opts* opts=ctx->opts;
  flti32* i32;

  flt_pal_tex* newpt = (flt_pal_tex*)flt_calloc(1,sizeof(flt_pal_tex));
  flt_mem_check(newpt, of->errcode);

  leftbytes -= (int)fread(ctx->tmpbuff, 1, flt_min(leftbytes,220), ctx->f);
  newpt->name = flt_strdup(ctx->tmpbuff);
  flt_getswapi32(newpt->patt_ndx, 200);
  flt_getswapi32(newpt->xy_loc[0], 204);
  flt_getswapi32(newpt->xy_loc[1], 208);

  if ( opts->cb_texture ) 
    opts->cb_texture(newpt,of,opts->cb_user_data); // callback?

  // adding node to list (use last if defined, or the beginning)
  if ( ctx->pal_tex_last )
    ctx->pal_tex_last->next = newpt;
  else
    of->pal->tex_head = newpt;

  ctx->pal_tex_last = newpt;
  ++of->pal->tex_count;
  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_extref)
{
  flt_context* ctx = of->ctx;
  flt_opts* opts=ctx->opts;
  flt_node_extref* newextref=0;
  int leftbytes = oh->length-sizeof(flt_op);
  flti32* i32;
  fltu16* u16;
  
  leftbytes -= (int)fread(ctx->tmpbuff,1,flt_min(leftbytes,210),ctx->f);
  {
    // creates
    newextref = (flt_node_extref*)flt_node_create(of, FLT_NODE_EXTREF, ctx->tmpbuff);
    flt_mem_check(newextref, of->errcode);    
    
    // set values
    flt_getswapi32(newextref->flags,204);
    flt_getswapu16(newextref->view_asbb, 208);

    // add to hierarchy
    flt_node_add(of,(flt_node*)newextref);

    // callback?
    if ( opts->cb_extref ) 
      opts->cb_extref(newextref, of, opts->cb_user_data);
  }

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_object)
{
  flt_context* ctx=of->ctx;
  int leftbytes = oh->length-sizeof(flt_op);
  flt_node_object* newobj;
  fltu32* u32;
  flti16* i16;

  // read and create node
  leftbytes -= (int)fread(ctx->tmpbuff,1,flt_min(leftbytes,24),ctx->f);
  {
    newobj = (flt_node_object*)flt_node_create(of, FLT_NODE_OBJECT, ctx->tmpbuff);
    flt_mem_check(newobj, of->errcode);

    // set values
    flt_getswapu32(newobj->flags, 8);
    flt_getswapi16(newobj->priority,12);
    flt_getswapi16(newobj->transp,14);

    // add to hierarchy
    flt_node_add(of,(flt_node*)newobj);
  }
  
  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_group)
{
  flt_context* ctx=of->ctx;
  int leftbytes = oh->length-sizeof(flt_op);
  flt_node_group* group;
  fltu32* u32;
  fltu16* u16;
  float* flo;

  leftbytes -= (int)fread(ctx->tmpbuff,1,flt_min(leftbytes,40),ctx->f);
  {
    group = (flt_node_group*)flt_node_create(of,FLT_NODE_GROUP,ctx->tmpbuff);
    flt_mem_check(group,of->errcode);

    flt_getswapu16(group->priority,8);
    flt_getswapu32(group->flags,12);
    flt_getswapu32(group->loop_count,28);
    flt_getswapflo(group->loop_dur,32);
    flt_getswapflo(group->last_fr_dur,36);

    flt_node_add(of,(flt_node*)group);
  }
  return leftbytes;
}
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_lod)
{
  flt_context* ctx=of->ctx;
  int leftbytes = oh->length-sizeof(flt_op);
  flt_node_lod* lod;
  fltu32* u32;
  double *dbl, *tgdbl;
  int i;

  leftbytes -= (int)fread(ctx->tmpbuff,1,flt_min(leftbytes,76),ctx->f);
  {
    lod = (flt_node_lod*)flt_node_create(of,FLT_NODE_LOD,ctx->tmpbuff);
    flt_mem_check(lod,of->errcode);

    flt_getswapdbl(lod->switch_in,12);
    flt_getswapdbl(lod->switch_out,20);    
    flt_getswapu32(lod->flags,32);
    dbl+=2; tgdbl=lod->cnt_coords;
    for (i=0;i<5;++i,++dbl){ flt_swap32(dbl); tgdbl[i]=*dbl; }
    
    flt_node_add(of,(flt_node*)lod);
  }
  return leftbytes;
}
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_mesh)
{
  flt_context* ctx=of->ctx;
  int leftbytes = oh->length-sizeof(flt_op);
  flt_node* n= (flt_node*)flt_calloc(1,sizeof(flt_node));

  leftbytes -= (int)fread(ctx->tmpbuff,1,leftbytes,ctx->f);

  flt_free(n);

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_longid)
{
  flt_context* ctx=of->ctx;
  int leftbytes = oh->length-sizeof(flt_op);

  flt_node* top = flt_stack_top(ctx->stack);
  if ( top )
  {
    leftbytes -= (int)fread(ctx->tmpbuff, 1, flt_min(leftbytes,512),ctx->f);
    top->name = (char*)flt_realloc(top->name,strlen(ctx->tmpbuff)+1);
    flt_mem_check(top->name,of->errcode);
    strcpy(top->name,ctx->tmpbuff);
  }
  
  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_pal_vertex)
{
  flt_context* ctx=of->ctx;
  flt_opts* opts=ctx->opts;
  int leftbytes, vmaxcount;
  fltu32 vtxsize;

  // record header
  leftbytes = oh->length - sizeof(flt_op);
  leftbytes -= (int)fread(&vmaxcount,1,flt_min(leftbytes,4),ctx->f);
  flt_swap32(&vmaxcount); 

  // read vertices from palette?
  if ( opts->palflags & FLT_OPT_PAL_VERTEX )
  {
    // no of vertices. Use smallest vtx record to have an upper bound of no. vertices in mem/file
    vmaxcount -= sizeof(flt_op)+4;
    vmaxcount /= 36; 

    vtxsize = flt_vtx_size(opts->vtxstream);

    // uses the source vertex format (is variable format)?
    if ( !vtxsize || (opts->palflags & FLT_OPT_PAL_VTX_SOURCE) )
    {
      opts->palflags |= FLT_OPT_PAL_VTX_SOURCE;
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
  else
  {
    leftbytes = vmaxcount-sizeof(flt_op)-4;
  }

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_vtx_color)
{
  static fltu16 srcstream[]={1536, 4376, 0};
  flt_context* ctx=of->ctx;
  fltu16* stream= ( ctx->opts->palflags & FLT_OPT_PAL_VTX_SOURCE ) ? srcstream : FLT_NULL;
  int leftbytes=oh->length-sizeof(flt_op);  
  double* coords; 
  flti32* abgr;

  // read data in
  leftbytes -= (int)fread(ctx->tmpbuff,1,flt_min(leftbytes,36),ctx->f);
  coords=(double*)(ctx->tmpbuff+4); flt_swap64(coords); flt_swap64(coords+1); flt_swap64(coords+2);
  abgr=(flti32*)(ctx->tmpbuff+28); flt_swap32(abgr);

  // stream for POSITION/COLOR
  flt_vtx_write(of,stream,coords,*abgr,flt_zerovec,flt_zerovec);

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_vtx_color_normal)
{
  static fltu16 srcstream[]={1536, 4376, 8988, 0};
  flt_context* ctx=of->ctx;
  fltu16* stream= ( ctx->opts->palflags & FLT_OPT_PAL_VTX_SOURCE ) ? srcstream : FLT_NULL;
  int leftbytes=oh->length-sizeof(flt_op);  
  double* coords;
  float* normal;
  flti32* abgr;

  // read data in
  leftbytes -= (int)fread(ctx->tmpbuff,1,flt_min(leftbytes,52),ctx->f);
  coords=(double*)(ctx->tmpbuff+4); flt_swap64(coords); flt_swap64(coords+1); flt_swap64(coords+2);
  normal=(float*)(ctx->tmpbuff+28); flt_swap32(normal); flt_swap32(normal+1); flt_swap32(normal+2);
  abgr=(flti32*)(ctx->tmpbuff+40); flt_swap32(abgr);

  // stream for POSITION/COLOR/NORMAL
  flt_vtx_write(of,stream,coords,*abgr,flt_zerovec,normal);

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_vtx_color_uv)
{
  static fltu16 srcstream[]={1536, 4376, 12828, 0};
  flt_context* ctx=of->ctx;
  fltu16* stream= (ctx->opts->palflags & FLT_OPT_PAL_VTX_SOURCE ) ? srcstream : FLT_NULL;
  int leftbytes=oh->length-sizeof(flt_op);  
  double* coords;
  float* uv;
  flti32* abgr;

  // read data in
  leftbytes -= (int)fread(ctx->tmpbuff,1,flt_min(leftbytes,40),ctx->f);
  coords=(double*)(ctx->tmpbuff+4); flt_swap64(coords); flt_swap64(coords+1); flt_swap64(coords+2);
  uv=(float*)(ctx->tmpbuff+28); flt_swap32(uv); flt_swap32(uv+1);
  abgr=(flti32*)(ctx->tmpbuff+36); flt_swap32(abgr);

  // stream for POSITION/COLOR/UV0
  flt_vtx_write(of,stream,coords,*abgr,uv,flt_zerovec);

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_vtx_color_normal_uv)
{
  static fltu16 srcstream[]={1536, 4376, 8988, 12840, 0};
  flt_context* ctx=of->ctx;
  fltu16* stream=( ctx->opts->palflags & FLT_OPT_PAL_VTX_SOURCE ) ? srcstream : FLT_NULL;
  int leftbytes=oh->length-sizeof(flt_op);  
  double* coords;
  float* uv;
  float* normal;
  flti32* abgr;

  // read data in
  leftbytes -= (int)fread(ctx->tmpbuff,1,flt_min(leftbytes,60),ctx->f);
  coords=(double*)(ctx->tmpbuff+4); flt_swap64(coords); flt_swap64(coords+1); flt_swap64(coords+2);
  normal=(float*)(ctx->tmpbuff+28); flt_swap32(normal); flt_swap32(normal+1); flt_swap32(normal+2);
  uv=(float*)(ctx->tmpbuff+40); flt_swap32(uv); flt_swap32(uv+1);
  abgr=(flti32*)(ctx->tmpbuff+48); flt_swap32(abgr);

  // stream for POSITION/COLOR/NORMAL/UV0
  flt_vtx_write(of,stream,coords,*abgr,uv,normal);

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_face)
{
  flt_context* ctx=of->ctx;
  int leftbytes = oh->length-sizeof(flt_op);
  flti32* i32; flti16* i16; fltu16* u16; 
  unsigned long facehash;
  flt_face tmpf={0};
  flt_face* face=&tmpf;
  flt_node_face* nodeface;
  flt_atomic_inc(&TOTALNFACES);

  leftbytes -= (int)fread(ctx->tmpbuff,1,flt_min(leftbytes,76),ctx->f);

  flt_getswapi16(face->abgr,52);
  flt_getswapi16(face->texbase_pat, 22);
  flt_getswapi16(face->texdetail_pat, 24);
  flt_getswapi16(face->mat_pat, 26);
  flt_getswapi16(face->shader_ndx,74);
  flt_getswapi32(face->flags,40);
  face->billb = (fltu8)*(ctx->tmpbuff+21);
#ifndef FLT_COMPACT_FACES
  flt_getswapi32(face->ir_color,8);
  flt_getswapi32(face->ir_mat,32);
  flt_getswapi16(face->smc_id,28);
  flt_getswapi16(face->feat_id,30);
  flt_getswapi16(face->transp, 36);
  flt_getswapu16(face->cname_ndx,16);
  flt_getswapu16(face->cnamealt_ndx,18);
  flt_getswapu16(face->texmapp_ndx,60);
  face->draw_type = *(fltu8*)(ctx->tmpbuff+14);
  face->light_mode = *(fltu8*)(ctx->tmpbuff+44);
  face->lod_gen = *(fltu8*)(ctx->tmpbuff+38);
  face->linestyle_ndx = *(fltu8*)(ctx->tmpbuff+39);
#endif

  
  // we store the face hash in every face node
  facehash = flt_dict_hash_face_djb2((const unsigned char*)face,sizeof(flt_face));

  // if it's unique, add it to dictionary of faces. 
  if ( !flt_dict_get(ctx->dictfaces, (const char*) face, sizeof(flt_face), facehash ) )
  {
    face = (flt_face*)flt_malloc(sizeof(flt_face));  
    flt_mem_check(face,of->errcode);
    *face = tmpf;
    flt_atomic_inc(&TOTALUNIQUEFACES);
    flt_dict_insert(ctx->dictfaces, (const char*)face, face, sizeof(flt_face), facehash);
  }


  // creating node
  nodeface = (flt_node_face*)flt_node_create(of, FLT_NODE_FACE,ctx->tmpbuff);
  flt_mem_check(nodeface,of->errcode);
  nodeface->face_hash = facehash;
  flt_node_add(of, (flt_node*)nodeface);
  flt_node* parent = flt_stack_top_not_null(ctx->stack);
  if ( parent )
    ++parent->face_count;

  return leftbytes;
}

fltu32 maxvlist=0;
////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_vertex_list)
{
  flt_context* ctx=of->ctx;
  int leftbytes = oh->length-sizeof(flt_op);
  fltu32 n_inds;
  fltu32* inds;
  fltu32 i;

  n_inds = (oh->length-4)/4;

  if ( n_inds > maxvlist )
    maxvlist=n_inds;

  flt_node* top = flt_stack_top_not_null(ctx->stack);
  if ( top )
  {
    top->index_count+=n_inds;
  }
//   if ( n_inds!=3 && n_inds!=4 && (n_inds%3!=0) && (n_inds%4!=0))
//   {
//     FLT_BREAK;
//     if ( n_inds )
//     {
//       inds = (fltu32*)flt_malloc(sizeof(fltu32)*n_inds);
//       leftbytes -= (int)fread(inds,1,sizeof(fltu32)*n_inds,ctx->f);
//       for (i=0;i<n_inds;++i) { flt_swap32(inds+i); }
//       if ( of->pal && of->pal->vtx )
//       {
// 
//       }
//       flt_free(inds);
//     }
//   }
  //   flt_node* top = flt_stack_top(ctx->stack);
  //   if ( top )
  //   {
  //     leftbytes -= (int)fread(ctx->tmpbuff, 1, flt_min(leftbytes,512),ctx->f);
  //     top->name = (char*)flt_realloc(top->name,strlen(ctx->tmpbuff)+1);
  //     flt_mem_check(top->name,of->errcode);
  //     strcpy(top->name,ctx->tmpbuff);
  //   }

  return leftbytes;
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

  // palette list
  if ( of->pal )
  {
    // texture pal
    pt=of->pal->tex_head; 
    while (pt)
    { 
      pn=pt->next; 
      flt_safefree(pt->name);
      flt_free(pt); 
      pt=pn; 
    } 

    // vertex palette
    flt_safefree(of->pal->vtx);
    if ( (int)of->pal->vtx_offsets<-512 || (int)of->pal->vtx_offsets>0 ) 
      flt_safefree(of->pal->vtx_offsets); // be sure it does not encode a fixed vertex size (negative)

    flt_safefree(of->pal);
  }

  // nodes
  if ( of->hie )
  {
    // releasing node hierarchy (all except extrefs)
    flt_release_node(of->hie->node_root);
    flt_safefree(of->hie->node_root);
    flt_safefree(of->hie);
  }

  // context
  if ( of->ctx )
  {
    // dict of faces
    flt_dict_destroy(&of->ctx->dictfaces,FLT_TRUE);

    // stack
    flt_stack_destroy(&of->ctx->stack);

    // dict
    if ( of->ctx->dict && flt_atomic_dec(&of->ctx->dict->ref)<=0 )
      flt_dict_destroy(&of->ctx->dict, 0);

    // finally context
    flt_safefree(of->ctx->basepath);
    flt_safefree(of->ctx);
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_release_node(flt_node* n)
{
  flt_node *next, *cur;
  flt_node_extref* eref;
  
  if (!n) return;
  
  flt_safefree(n->name);  

  // children
  cur=next=n->child_head;
  while (cur)
  {
    next=cur->next;
    flt_release_node(cur);
    flt_free(cur);
    cur=next;
  }

  // external ref node to consider to release a flt* reference
  if ( n->type == FLT_NODE_EXTREF )
  {
    eref=(flt_node_extref*)n;
    if ( eref->of && flt_atomic_dec(&eref->of->ref)<=0 )
    {
      flt_release(eref->of);
      flt_free(eref->of);        
    }
    eref=FLT_NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_node_add_child(flt_node* parent, flt_node* node)
{
  if ( !parent )  // always node_root
    FLT_BREAK;

  if ( !parent->child_head )
  {
    parent->child_tail = parent->child_head = node;
  }
  else 
  {
    parent->child_tail->next = node;
    parent->child_tail = node;
  }

  ++parent->child_count;  
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_node_add(flt* of, flt_node* node)
{
  flt_context* ctx = of->ctx;
  flt_node *parent=0;
  flt_node_extref* er;

  if (ctx->stack->count)
  {
    // pop and then look for parent
    flt_stack_pop(ctx->stack);
    parent = flt_stack_top_not_null(ctx->stack);      

    // add to parent
    flt_node_add_child(parent,node);

    // restore stack
    flt_stack_push(ctx->stack, node );
  }
  else
  {
    FLT_BREAK; // shouldn't be here as there's always the node_root
  }

  if ( of->hie ) 
  {
    ++of->hie->node_count;

    // check if it's a extref node, then we maintain an additional flat list structure
    if ( node->type==FLT_NODE_EXTREF )
    {
      er = (flt_node_extref*)node;
      if ( ctx->node_extref_last )
        ctx->node_extref_last->next_extref = er; // if shortcut to last one
      else
        of->hie->extref_head = er; // if starting list
      ++of->hie->extref_count;
      ctx->node_extref_last = er;
    }
  }

}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
flt_node* flt_node_create(flt* of, int nodetype, const char* name)
{
  flt_node* n=0;
  static int nodesizes[FLT_NODE_MAX]={sizeof(flt_node), sizeof(flt_node_extref), sizeof(flt_node_group),
    sizeof(flt_node_object), sizeof(flt_node_mesh), sizeof(flt_node_lod), sizeof(flt_node_face)};  
  int size=nodesizes[nodetype];

  n=(flt_node*)flt_calloc(1,size);
  if (n)
  {
    n->type = nodetype;
    if ( (!(of->ctx->opts->hieflags & FLT_OPT_HIE_NO_NAMES) && name && *name) || nodetype==FLT_NODE_EXTREF)
      n->name = flt_strdup(name);
  }
  return n;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_vtx_write(flt* of, fltu16* stream, double* xyz, fltu32 abgr, float* uv, float* normal)
{
  flt_context* ctx=of->ctx;  
  flt_opts* opts=ctx->opts;
  fltu8* vertex;

  if ( !stream ) stream = opts->vtxstream;
  // offset to current vertex
  vertex = of->pal->vtx + ctx->vtx_offset;

  // if format source used, update array and use the source stream
  // use source. gets offset to next vertex, saves pos+color, advance offset accordingly
  if ( opts->palflags & FLT_OPT_PAL_VTX_SOURCE )
    of->pal->vtx_offsets[of->pal->vtx_count] = ctx->vtx_offset;

  ctx->vtx_offset += flt_vtx_write_sem(vertex, stream, xyz, abgr, normal, uv);   
  ++of->pal->vtx_count;
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
  case FLT_ERR_ALREADY: return "Already parsed and registered in the context dictionary";
  }
#else
  switch ( errcode )
  {
  case FLT_OK         : return "Ok"; 
  case FLT_ERR_FOPEN  : return "File error"; 
  case FLT_ERR_VERSION: return "Version unsupported";
  case FLT_ERR_MEMOUT : return "Out of mem";
  case FLT_ERR_READBEYOND_REC: return "Read beyond record"; 
  case FLT_ERR_ALREADY: return "Already parsed";
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
  const float invc=1.0f/255.f;

  while (*vstream)
  {
    // this semantic
    flt_vtx_stream_dec(*vstream,&tgt_sem,&offs,&size);
    tsize+=size;
    switch(tgt_sem)
    {
    case FLT_VTX_POSITION: // support for float/double positions
      floats = (float*)(outdata+offs);
      switch(size)
      {
      case 12: for (i=0;i<3;++i) floats[i]=(float)xyz[i]; break; // expecting 3 floats
      case 16: for (i=0;i<3;++i) floats[i]=(float)xyz[i]; floats[3]=1.0f; break; // expecting 4 floats
      case 24: memcpy(outdata+offs,xyz,24); break; // expecting 3 doubles (same as flt)
      case 32: memcpy(outdata+offs,xyz,24); *((double*)(outdata+offs+24)) = 1.0; break; // expecting 4 doubles
      }
      break;
    case FLT_VTX_COLOR : 
      switch(size)
      {
      case 4 : *(fltu32*)(outdata+offs)=abgr; break; // expecting packed color
      case 12: floats=(float*)(outdata+offs); // expecting 3 floats normalized color
        floats[0]=(abgr>>24)*invc;        floats[1]=((abgr>>16)&0xff)*invc; 
        floats[2]=((abgr>>8)&0xff)*invc;  floats[3]=(abgr&0xff)*invc;
        break;
      }
      
      break;
    case FLT_VTX_NORMAL: floats = (float*)(outdata+offs); for (i=0;i<3;++i) floats[i]=normal[i]; break; // 3 normals
    case FLT_VTX_UV0   : floats = (float*)(outdata+offs); for (i=0;i<(int)(size/sizeof(float));++i) floats[i]=i<2 ? uv[i] : 0.0f; break; // 2 or 3 uv comps
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
  size_t len;
  if ( !filename) return FLT_NULL;

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
  flt_context* ctx=of->ctx;  
  flt_opts* opts=ctx->opts;
  fltu32 i=0;  
  char* basefile;
  const char* spath;

  strcpy(ctx->tmpbuff,filename);
  ctx->f = fopen(ctx->tmpbuff,"rb");
  if( !ctx->f && opts->search_paths ) // failed original, use search paths if valid
  {
    // get the base file name
    basefile = flt_path_basefile(filename);
    while ( basefile ) // infinite if base file, but breaking in
    {
      spath = opts->search_paths[i];
      if ( !spath ) break;
      *ctx->tmpbuff='\0'; // empty the target
      strcat(ctx->tmpbuff, spath);
      if ( !flt_path_endsok(spath) ) strcat(ctx->tmpbuff, "/" );      
      strcat(ctx->tmpbuff, basefile);
      ctx->f = fopen(ctx->tmpbuff,"rb");
      if (ctx->f) break;
      ++i;
    }
    flt_safefree(basefile);
  }

  // if open ok, saves correct filename and base path
  if ( ctx->f )
  {
    if ( !of->filename )
      of->filename = flt_strdup(ctx->tmpbuff);  
    ctx->basepath = flt_path_base(ctx->tmpbuff);
  }
  return ctx->f;
}

////////////////////////////////////////////////////////////////////////////////////////////////
// extract the filename from a path
////////////////////////////////////////////////////////////////////////////////////////////////
char* flt_path_basefile(const char* filename)
{
  const char *ptr;
  size_t len;

  if ( !filename ) return FLT_NULL;
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

  if ( ptr==filename+len ) return FLT_NULL;
  return flt_strdup(ptr);  
}

////////////////////////////////////////////////////////////////////////////////////////////////
// returns 1 if path ends with / or \
////////////////////////////////////////////////////////////////////////////////////////////////
int flt_path_endsok(const char* filename)
{
  size_t len;
  const char* ptr;

  if ( !filename || !*filename ) return FLT_FALSE;
  len = strlen(filename);
  ptr = filename+len-1;
  while (ptr!=filename && (*ptr==' ' || *ptr=='\t') )
    --ptr;
  return (*ptr=='\\' || *ptr=='/') ? FLT_TRUE : FLT_FALSE;
}

////////////////////////////////////////////////////////////////////////////////////////////////
//                                DICTIONARY
////////////////////////////////////////////////////////////////////////////////////////////////
// djb2
unsigned long flt_dict_hash_djb2(const unsigned char* str, int unused) 
{
  unused;
  unsigned long hash = 5381;
  int c;
  while (c = *str++) hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  return hash;
}

unsigned long flt_dict_hash_face_djb2(const unsigned char* data, int size)
{
  unsigned long hash = 5381;
  int c,i;
  for (i=0;i<size;++i)
  {
    c=*data++;
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return hash;
}

int flt_dict_keycomp_string(const char* a, const char* b, int extra)
{
  return strcmp(a,b);
}

int flt_dict_keycomp_face(const char* a, const char* b, int size)
{
  return memcmp(a,b,size);
}

// create_cs==1 to create a critical section object
void flt_dict_create(int capacity, int create_cs, flt_dict** dict, flt_dict_hash hashf, flt_dict_keycomp keycomp)
{
  flt_dict* d = (flt_dict*)flt_malloc(sizeof(flt_dict));
  d->cs = create_cs ? flt_critsec_create() : FLT_NULL;
  d->hasht = (flt_dict_node**)flt_calloc(capacity,sizeof(flt_dict_node*));
  d->capacity = capacity;
  d->count = 0;
  d->hashf = hashf;
  d->keycomp = keycomp;
  d->ref = 1;
  *dict = d;
}

void flt_dict_destroy(flt_dict** dict, int free_elms)
{
  int i;
  flt_dict_node *n, *nn;

  if ( (*dict)->cs ) flt_critsec_enter((*dict)->cs);
  
  // entries and their lists
  for (i=0;i<(*dict)->capacity;++i)
  {
    n=(*dict)->hasht[i];
    while (n)
    {
      nn=n->next;
      if ( free_elms )
        flt_safefree(n->value);
      flt_free(n->key);
      flt_free(n);
      n=nn;
    }
  }
  flt_safefree((*dict)->hasht);
  if ( (*dict)->cs ) 
  {
    flt_critsec_leave((*dict)->cs);
    flt_critsec_destroy((*dict)->cs);
  }
  flt_safefree(*dict);
}

void* flt_dict_get(flt_dict* dict, const char* key, int size, flt_optional unsigned long hash)
{
  flt_dict_node *n;
  const unsigned long hashk = !hash ? dict->hashf((const unsigned char*)key,size) : hash;
  const int entry = hashk % dict->capacity;

  if ( dict->cs ) flt_critsec_enter(dict->cs);
  n = dict->hasht[entry];  
  while(n)
  {
    if ( n->keyhash == hashk && dict->keycomp(n->key,key,size)==0 ){ break; }
    n=n->next;
  }
  if ( dict->cs ) flt_critsec_leave(dict->cs);
  return n ? n->value : FLT_NULL;
}

flt_dict_node* flt_dict_create_node(const char* key, unsigned long keyhash, void* value, int size)
{
  flt_dict_node *n = (flt_dict_node*)flt_malloc(sizeof(flt_dict_node));
  if ( size ) // we got size, just alloc+memcpy the key
  {
    n->key = (char*)flt_malloc(size);
    memcpy(n->key,key,size);
  }
  else
  {
    n->key = flt_strdup(key); // 0 size is for strings
  }
  n->keyhash = keyhash;
  n->value = value;
  n->next = FLT_NULL;
  return n;
}

int flt_dict_insert(flt_dict* dict, const char* key, void* value, int size, flt_optional unsigned long hash)
{
  flt_dict_node *n, *ln;
  unsigned long hashk;
  int entry;

  if ( !value || !dict ) 
    return FLT_FALSE; // must insert non-null value.  
  hashk = !hash ? dict->hashf((const unsigned char*)key, size) : hash;
  if ( dict->cs ) flt_critsec_enter(dict->cs); // acquire
  entry = hashk % dict->capacity;
  n=dict->hasht[entry];
  ++dict->count;
  if ( !n )
  {
    dict->hasht[entry]=flt_dict_create_node(key,hashk,value,size); // not found in entry, add it
  }
  else
  {
    ln=n;
    // entry collision: append to the end of list: new entry, just hash collision || or already exists element
    while (n)
    {
      // same keyhash and same keyname (exists)
      if ( n->keyhash == hashk && dict->keycomp(n->key,key,size)==0 ) { n->value = value; ln=0; --dict->count; break; }
      ln=n;
      n=n->next;
    }    
    if (ln) ln->next = flt_dict_create_node(key,hashk,value,size);
  }

  if ( dict->cs ) flt_critsec_leave(dict->cs);
  return FLT_TRUE;
}

////////////////////////////////////////////////////////////////////////////////////////////////
//                            CRITICAL SECTION / ATOMIC
////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef _MSC_VER
typedef struct flt_critsec
{
  CRITICAL_SECTION cs;
}flt_critsec;

flt_critsec* flt_critsec_create()
{
  flt_critsec* cs=(flt_critsec*)flt_malloc(sizeof(flt_critsec));
  InitializeCriticalSection(&cs->cs);
  return cs;
}

void flt_critsec_destroy(flt_critsec* cs)
{
  DeleteCriticalSection(&cs->cs);
  flt_free(cs);
}

void flt_critsec_enter(flt_critsec* cs)
{
  EnterCriticalSection(&cs->cs);
}

void flt_critsec_leave(flt_critsec* cs)
{
  LeaveCriticalSection(&cs->cs);
}

long flt_atomic_dec(fltatom32* c)
{
  return InterlockedDecrement(c);
}

long flt_atomic_inc(fltatom32* c)
{
  return InterlockedIncrement(c);
}

#else // perhaps pthread/gcc builtings ? // add other platforms here

typedef struct flt_critsec
{
  pthread_mutex_t mtx;
}flt_critsec;

flt_critsec* flt_critsec_create()
{
  flt_critsec* cs;
  cs=(flt_critsec*)flt_malloc(sizeof(flt_critsec));
  pthread_mutex_init(&cs->mtx, ... );
  return cs;
}

void flt_critsec_destroy(flt_critsec* cs)
{
  pthread_mutex_destroy(&cs->mtx);
  flt_free(cs);
}

void flt_critsec_enter(flt_critsec* cs)
{
  pthread_mutex_lock(&cs->mtx);
}

void flt_critsec_leave(flt_critsec* cs)
{
  pthread_mutex_unlock(&cs->mtx);
}

long flt_atomic_dec(fltatom32* c)
{
  return __sync_sub_and_fetch(c,1);
}

long flt_atomic_inc(fltatom32* c)
{
  return __sync_add_and_fetch(c,1);
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////
//                                STACK
////////////////////////////////////////////////////////////////////////////////////////////////
int flt_stack_create(flt_stack** s, int capacity)
{
  flt_stack* _s = (flt_stack*)flt_calloc(1,sizeof(flt_stack));
  if ( !_s ) return FLT_FALSE;  
  if (!capacity) 
    capacity = FLT_STACKARRAY_SIZE;

  _s->entries = (flt_node**)flt_malloc(sizeof(flt_node*)*capacity);
  if (!_s->entries) return  FLT_FALSE;
  _s->capacity = capacity;

  *s = _s;
  return FLT_TRUE;
}

void flt_stack_destroy(flt_stack** s)
{
  if ( !s || !*s ) return;
#ifdef _DEBUG
  if ( (*s)->count )
    printf( "Stack destroyed with %d elements\n", (*s)->count);
#endif
  flt_safefree((*s)->entries);
  flt_safefree((*s));
}

void flt_stack_clear(flt_stack* s)
{
  s->count=0;
}

void flt_stack_push(flt_stack* s, flt_node* value)
{
  if (s->count >= s->capacity )
  {
    FLT_BREAK;
    return;
  }
  s->entries[s->count++]=value;
}

flt_node* flt_stack_top(flt_stack* s)
{
  return s->count ? s->entries[s->count-1] : FLT_NULL;
}

flt_node* flt_stack_top_not_null(flt_stack* s)
{
  int i=s->count-1;
  
  while (i>=0)
  {
    if ( s->entries[i] ) return s->entries[i];
    --i;
  }
  return FLT_NULL;
}

flt_node* flt_stack_pop(flt_stack* s)
{
  flt_node* v=FLT_NULL;

  if (s->count)
  {
    v=s->entries[--s->count];
  }
  return v;
}

void flt_stack_set_top(flt_stack* s, flt_node* value)
{
  if (s->count)
  {
    s->entries[s->count-1]=value;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////
//                                    ARRAY  
////////////////////////////////////////////////////////////////////////////////////////////////
int flt_array_create(flt_array** arr, int capacity, flt_array_growpolicy grow)
{
  *arr = (flt_array*)flt_calloc(1,sizeof(flt_array));
  if ( !*arr ) return FLT_FALSE;
  (*arr)->capacity = capacity>0?capacity:FLT_ARRAY_INITCAP;
  (*arr)->size = 0;
  (*arr)->data = (void**)flt_malloc(sizeof(void*)*capacity);
  if ( !(*arr)->data ) { flt_array_destroy(arr); return FLT_FALSE; }
  if ( !grow ) grow = flt_array_grow_double;
  (*arr)->growpolicy = grow;
  return FLT_TRUE;
}

void flt_array_destroy(flt_array** arr)
{
  if ( !arr || !*arr ) return;
  flt_safefree((*arr)->data);
  flt_free(*arr);
  *arr=FLT_NULL;
}

void flt_array_grow_double(flt_array* arr)
{
  int newcap = arr->capacity*2;
  arr->data = (void**)flt_realloc(arr->data, sizeof(void*)*newcap);
  if( !arr->data ) { FLT_BREAK; return; }
  arr->capacity = newcap;
}

void flt_array_push_back(flt_array* arr, void* elem)
{
  if (arr->size>=arr->capacity)
    arr->growpolicy(arr);    
  arr->data[arr->size++]=elem;
}

void* flt_array_pop_back(flt_array* arr)
{
  if ( arr->size==0 ) return FLT_NULL;
  return arr->data[--arr->size];
}

void* flt_array_at(flt_array* arr, int index)
{
  return index >= 0 && index < arr->size ? arr->data[index] : FLT_NULL;
}

void flt_array_clear(flt_array* arr)
{
  arr->size=0;
}



/*
bool nfltIsOpcodeObsolete(unsigned short opcode)
{
return opcode==3 || (opcode>=6 && opcode<=9) || (opcode>=12 && opcode<=13)
|| (opcode>=16 && opcode<=17) || (opcode>=40 && opcode<=48) || opcode==51
|| (opcode>=65 && opcode<=66) || opcode==77;
}

*/

#endif

#ifdef _MSC_VER
#pragma warning(default:4244 4100 4996 4706 4091)
#endif

#endif