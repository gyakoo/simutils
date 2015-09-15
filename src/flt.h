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
- Define flt_malloc/flt_calloc/flt_free/flt_realloc for custom memory management functions
- Define FLT_NO_OPNAMES to skip opcode name strings
- Define FLT_IMPLEMENTATION before including this in *one* C/CPP file to expand the implementation
- Define the supported version in FLT_VERSION. Maximum supported version is FLT_GREATER_SUPPORTED_VERSION
- Define FLT_NO_MEMOUT_CHECK to skip out-of-mem error
- Define FLT_HASHTABLE_SIZE for a different default hash table size. (http://planetmath.org/goodhashtableprimes)
- Define FLT_STACKARRAY_SIZE for a different default stack array size (when passing flt_opts.stacksize=0)
- Define FLT_COMPACT_FACES for a smaller footprint version of face record with most important data. see flt_face.
- Define FLT_DICTFACES_SIZE for a different default hash table size for faces when passing flt_opts.dfaces_size=0.
- Define FLT_UNIQUE_FACE for an optimized way to store faces (develop more doc about this)
- Define FLT_ALIGNED to use aligned version of malloc/free (when not implementing custom flt_malloc/...)
- Define FLT_INDICES_SIZE for a default initial capacity of the global indices array (when using FLT_UNIQUE_FACES only)
          and flt_opts.indices_size=0.
- Define FLT_TEXTURE_ATTRIBS_IN_NODE to keep width/height/depth attributes in each flt_text_pal node

(Additional info)
- Calls to load functions are thread safe
- For the vertex palette (FLT_OPT_PAL_VERTEX) the whole vertex palete is stored directly into the buffer in memory

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
#include <Windows.h> // mainly for the critical section object
#pragma warning(disable:4244 4100 4996 4706 4091 4310)
#endif

// Error codes
#define FLT_OK 0
#define FLT_ERR_FOPEN 1
#define FLT_ERR_OPREAD 2
#define FLT_ERR_VERSION 3
#define FLT_ERR_MEMOUT 4
#define FLT_ERR_READBEYOND_REC 5
#define FLT_ERR_ALREADY 6

// Versioning
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
#define FLT_OPT_PAL_ALL           (0xffff)// from bit 0 to 15

#define FLT_OPT_PAL_VTX_POSITION  (1<<16)
#define FLT_OPT_PAL_VTX_NORMAL    (1<<17)
#define FLT_OPT_PAL_VTX_UV        (1<<18)
#define FLT_OPT_PAL_VTX_COLOR     (1<<19)
#define FLT_OPT_PAL_VTX_POSITION_SINGLE (1<<20)
#define FLT_OPT_PAL_VTX_MASK      (FLT_OPT_PAL_VTX_POSITION|FLT_OPT_PAL_VTX_NORMAL|FLT_OPT_PAL_VTX_UV|FLT_OPT_PAL_VTX_COLOR)

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
#define FLT_OPT_HIE_ALL_NODES     (0x7ffff) // from bit 0 to 18

#define FLT_OPT_HIE_HEADER          (1<<19) // read header
#define FLT_OPT_HIE_NO_NAMES        (1<<20) // don't store names nor longids
#define FLT_OPT_HIE_EXTREF_RESOLVE  (1<<21) // xrefs aren't resolved by default, set this bit to make it
#define FLT_OPT_HIE_COMMENTS        (1<<22) // include comments 
#define FLT_OPT_HIE_GO_THROUGH      (1<<23)

#define FLT_OPT_HIE_RESERVED1       (1<<30) // not used
#define FLT_OPT_HIE_RESERVED2       (1<<31)

//////////////////////////////////////////////////////////////////////////
#define FLT_NOTLOADED 0
#define FLT_LOADING 1
#define FLT_LOADED 2

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
#define FLT_OP_CONTINUATION 23
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
#define FLT_OP_LOCAL_VERTEX_POOL 85
#define FLT_OP_MESH_PRIMITIVE 86
#define FLT_OP_SWITCH 96
#define FLT_OP_MAX 154

//////////////////////////////////////////////////////////////////////////
// FLT Node types
#define FLT_NODE_NONE   0
#define FLT_NODE_BASE   1
#define FLT_NODE_EXTREF 2 //FLT_OP_EXTREF
#define FLT_NODE_GROUP  3 //FLT_OP_GROUP
#define FLT_NODE_OBJECT 4 //FLT_OP_OBJECT
#define FLT_NODE_MESH   5 //FLT_OP_MESH
#define FLT_NODE_LOD    6 //FLT_OP_LOD
#define FLT_NODE_FACE   7 //FLT_OP_FACE
#define FLT_NODE_VLIST  8 //FLT_OP_VERTEX_LIST
#define FLT_NODE_SWITCH 9 // FLT_OP_SWITCH
#define FLT_NODE_MAX    10

#ifdef __cplusplus
extern "C" {
#endif
  typedef unsigned char  fltu8;
  typedef unsigned short fltu16;
  typedef unsigned int   fltu32;
  typedef unsigned long long fltu64;
  typedef char  flti8;
  typedef short flti16;
  typedef int   flti32;
  typedef long long flti64;
  typedef long fltatom32;

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
  typedef struct flt_node_switch;
  typedef struct flt_node_face;
  typedef struct flt_face;
  typedef struct flt_array;
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

#ifdef FLT_WRITER
  int flt_write_to_filename(struct flt* of);
#endif
  
  // Parsing options
  typedef struct flt_opts
  {
    fltu32 pflags;                          // palette flags        (FLT_OPT_PAL_*)
    fltu32 hflags;                          // hierarchy/node flags (FLT_OPT_HIE_*)
    fltu16 stacksize;                         // optional stack size (no of elements). 0 to use FLT_STACKARRAY_SIZE    
    fltu32 dfaces_size;                       // optional dict faces hash table size. 0 to use FLT_DICTFACES_SIZE
    fltu32 indices_size;                      // optional array initial capacity for indices. 0 to use FLT_INDICES_SIZE

    const char** search_paths;                // optional custom array of search paths ordered. last element should be null.
    flt_callback_extref   cb_extref;          // optional callback when an external ref is found
    flt_callback_texture  cb_texture;         // optional callback when a texture entry is found
    fltatom32* countable;                        // optional to get back counters for opcodes
    void* cb_user_data;                       // optional data to pass to callbacks
  }flt_opts;

  // Palettes information
  typedef struct flt_palettes
  {
    struct flt_pal_tex* tex_head;             // list of textures
    fltu32 tex_count;                         // no of textures
    fltu8* vtx_buff;                               // buffer of consecutive vertices
    fltu8* vtx_array;
    fltu32 vtx_count;
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
    fltu32 loaded;                            // 0:not loaded 1:loading 2:loaded
    fltatom32 ref;
#ifdef FLT_UNIQUE_FACES
    flt_array* indices;
#endif

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
#ifdef FLT_TEXTURE_ATTRIBS_IN_NODE
    fltu16 width;
    fltu16 height;
    fltu16 depth;
#endif
  }flt_pal_tex;
  
  typedef struct flt_node
  {
    char*  name;
    struct flt_node* next;
    struct flt_node* child_head;
    struct flt_node* child_tail;  // last for shortcut adding
#ifdef FLT_UNIQUE_FACES
    fltu64* ndx_pairs;
    fltu32 ndx_pairs_count;
#endif
    fltu16 type;                  // one of FLT_NODE_*
    fltu16 child_count;           // number of children
  }flt_node;

  typedef struct flt_face
  {
#ifndef FLT_LEAN_FACES
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
    char* name;
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

#ifndef FLT_LEAN_FACES
# define FLT_FACESIZE_HASH (sizeof(flt_face)-sizeof(char*))
#else
# define FLT_FACESIZE_HASH sizeof(flt_face)
#endif

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

  typedef struct flt_mesh_vb
  {
    fltu32 semantic;
    fltu32 count;
    fltu8* vertices;
  }flt_mesh_vb;

  typedef struct flt_node_mesh
  {
    struct flt_node base;
    struct flt_face attribs;
    struct flt_mesh_vb* vb;
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

  typedef struct flt_node_switch
  {
    struct flt_node base;    
    fltu32 cur_mask;
    fltu32 mask_count;
    fltu32 wpm;         // words per mask
    fltu32* maskwords;    
  }flt_node_switch;

#ifndef FLT_UNIQUE_FACES
  typedef struct flt_node_face
  {
    struct flt_node base;
    struct flt_face face;
  }flt_node_face;
#endif

  typedef struct flt_node_vlist
  {
    struct flt_node base;
    fltu32* indices;
    fltu32 count;
  }flt_node_vlist;

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
#if defined(__x86_64__) || defined(_M_X64)  ||  defined(__aarch64__)   || defined(__64BIT__) || \
  defined(__mips64)     || defined(__powerpc64__) || defined(__ppc64__)
#	define FLT_PLATFORM_64
# define FLT_ALIGNMENT 16
#else
#	define FLT_PLATFORM_32
# define FLT_ALIGNMENT 8
#endif //

#if defined(_DEBUG) || defined(DEBUG)
#ifdef _MSC_VER
#define FLT_BREAK { __debugbreak(); }
#else
#define FLT_BREAK { raise(SIGTRAP); }
#endif

#define FLT_ASSERT(c) { if (!(c)){ FLT_BREAK; } }
#else
#define FLT_BREAK {(void*)0;}
#define FLT_ASSERT(c)
#endif

#ifdef _MSC_VER
#define FLT_BREAK_ALWAYS { __debugbreak(); }
#else
#define FLT_BREAK_ALWAYS { raise(SIGTRAP); }
#endif


// Memory functions override
#if defined(flt_malloc) && defined(flt_free) && defined(flt_calloc) && defined(flt_realloc)
// ok, all defined
#elif !defined(flt_malloc) && !defined(flt_free) && !defined(flt_calloc) && !defined(flt_realloc)
// ok, none defined
#else
#error "Must define all or none of flt_malloc, flt_free, flt_calloc, flt_realloc"
#endif

#ifdef FLT_ALIGNED
#   ifndef flt_malloc
#     define flt_malloc(sz) _aligned_malloc(sz,FLT_ALIGNMENT)
#   endif
#   ifndef flt_free
#     define flt_free(p) _aligned_free(p)
#   endif
#   ifndef flt_calloc
#     define flt_calloc(count,size) flt_aligned_calloc(count,size,FLT_ALIGNMENT)
#   endif
#   ifndef flt_realloc
#     define flt_realloc(p,sz) _aligned_realloc(p,sz,FLT_ALIGNMENT)
#   endif
#else
#   ifndef flt_malloc
#     define flt_malloc(sz) malloc(sz)
#   endif
#   ifndef flt_free
#     define flt_free(p) free(p)
#   endif
#   ifndef flt_calloc
#     define flt_calloc(count,size) calloc(count,size)
#   endif
#   ifndef flt_realloc
#     define flt_realloc(p,sz) realloc(p,sz)
#   endif
#endif
#define flt_strdup(s) flt_strdup_internal(s)
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
#define fltopt 
#define FLTMAKE16(hi8,lo8)    ( ((fltu16)(hi8)<<8)   | (fltu16)(lo8) )
#define FLTMAKE32(hi16,lo16)  ( ((fltu32)(hi16)<<16) | (fltu32)(lo16) )
#define FLTMAKE64(hi32,lo32)  ( ((fltu64)(hi32)<<32) | (fltu64)(lo32) )
#define FLTGETLO16(u32)       ( (fltu16)((u32)&0xffff) )
#define FLTGETHI16(u32)       ( (fltu16)((u32)>>16) )
#define FLTGETLO32(u64)       ( (fltu32)((u64)&0xffffffff) )
#define FLTGETHI32(u64)       ( (fltu32)((u64)>>32) )

#define FLTGET16(u32,hi16,lo16) { (lo16)=FLTGETLO16(u32); (hi16)=FLTGETHI16(u32); }
#define FLTGET32(u64,hi32,lo32) { (lo32)=FLTGETLO32(u64); (hi32)=FLTGETHI32(u64); }

#ifndef FLT_HASHTABLE_SIZE 
#define FLT_HASHTABLE_SIZE 3079       // this size is for the *shared* dict of flt file names
#endif

#ifndef FLT_STACKARRAY_SIZE           // fixed stack size, 32 is a good maximum stack depth
#define FLT_STACKARRAY_SIZE 32
#endif

#ifndef FLT_ARRAY_INITCAP            // init capacity for any created array when passed capacity=0
#define FLT_ARRAY_INITCAP 1024
#endif

#ifndef FLT_DICTFACES_SIZE          // this size is for the dict of faces for *every* flt file
//#define FLT_DICTFACES_SIZE 769
#define FLT_DICTFACES_SIZE 1543
//#define FLT_DICTFACES_SIZE 3079       
//#define FLT_DICTFACES_SIZE 6151 
//#define FLT_DICTFACES_SIZE 12289
//#define FLT_DICTFACES_SIZE 24593
#endif

#ifndef FLT_INDICES_SIZE
#define FLT_INDICES_SIZE 4096       // this is the default initial capacity of the indices array for *every* flt
#endif                              // only used with FLT_UNIQUE_FACES and when flt_opts.indices_size=0


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
  b[0]=b[7]; 
  b[1]=b[6]; 
  b[2]=b[5]; 
  b[3]=b[4];
  b[4]=t3;
  b[5]=t2;   
  b[6]=t1;   
  b[7]=t0;   
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
  char tmpbuff[512];
  FILE* f;
  struct flt_pal_tex* pal_tex_last;
  struct flt_node_extref* node_extref_last;
  struct flt_opts* opts;
  struct flt_dict* dict;
#ifdef FLT_UNIQUE_FACES
  struct flt_dict* dictfaces;
#endif
  struct flt_stack* stack;
  char* basepath;
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
flti32 flt_atomic_dec(fltatom32* c);
flti32 flt_atomic_inc(fltatom32* c);
void flt_atomic_add(fltatom32* c, fltu32 val);


////////////////////////////////////////////////
// Dictionary 
////////////////////////////////////////////////
typedef fltu32 (*flt_dict_hash)(const unsigned char*, int);
typedef int (*flt_dict_keycomp)(const char*, const char*, int);
typedef void (*flt_dict_visitor)(char* key, void* value, void* userdata);
typedef struct flt_dict_node
{
  char* key;
  fltu32 keyhash;
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


fltu32 flt_dict_hash_djb2(const unsigned char* str, int extra);
fltu32 flt_dict_hash_face_djb2(const unsigned char* str, int size);
int flt_dict_keycomp_string(const char* a, const char* b, int extra);
int flt_dict_keycomp_face(const char* a, const char* b, int size);
void flt_dict_create(int capacity, int create_cs, flt_dict** dict, flt_dict_hash hashf, flt_dict_keycomp kcomp);
void flt_dict_destroy(flt_dict** dict, int free_elms, flt_dict_visitor destructor);
void* flt_dict_gethe(flt_dict* dict, fltu32 hashe, fltu32* hashkey); // returns given entry+offset in hash
void* flt_dict_geth(flt_dict* dict, const char* key, fltopt int size, fltu32 hash, fltu32* hashe); // returns given a hash
void* flt_dict_get(flt_dict* dict, const char* key, fltopt int size);
flt_dict_node* flt_dict_create_node(const char* key, fltu32 keyhash, void* value, int size);
int flt_dict_insert(flt_dict* dict, const char* key, void* value, fltopt int size, fltopt fltu32 hash, fltopt fltu32* hashentry);
void flt_dict_visit(flt_dict*dict, flt_dict_visitor visitor, void* userdata);

////////////////////////////////////////////////
// Stack
////////////////////////////////////////////////
#ifdef FLT_PLATFORM_64
typedef struct flt_stack_entry
{
  flt_node* node;
  fltu32 val32;
}flt_stack_entry;
typedef flt_stack_entry flt_stack_type;
#else
typedef fltu64 flt_stack_type;
#endif
typedef struct flt_stack 
{
  flt_stack_type* entries; // fixed size array (enough with a good upper bound estimation)
  int count;
  int capacity;
}flt_stack;

int flt_stack_create(flt_stack** s, int capacity);
void flt_stack_destroy(flt_stack** s);
void flt_stack_push32(flt_stack* s, fltu32 val);
void flt_stack_pushn(flt_stack* s, flt_node* value);
flt_node* flt_stack_topn(flt_stack* s);
flt_node* flt_stack_topn_not_null(flt_stack* s);
fltu32 flt_stack_top32_not_null(flt_stack* s);
flt_node* flt_stack_popn(flt_stack* s);

////////////////////////////////////////////////
// Dynamic Array
////////////////////////////////////////////////
#ifdef FLT_UNIQUE_FACES
#define flt_array_type fltu64
struct flt_array;
typedef void (*flt_array_growpolicy)(flt_array*);
typedef struct flt_array
{
  flt_array_type* data;
  fltu32 size;
  fltu32 capacity;
  flt_array_growpolicy growpolicy;
}flt_array;

int flt_array_create(flt_array** arr, fltu32 capacity, flt_array_growpolicy grow);
void flt_array_destroy(flt_array** arr);
void flt_array_push_back(flt_array* arr, flt_array_type elem);
flt_array_type flt_array_pop_back(flt_array* arr);
flt_array_type flt_array_at(flt_array* arr, fltu32 index);
void flt_array_clear(flt_array* arr);
void flt_array_grow_double(flt_array* arr);
void flt_array_ensure(flt_array* arr, fltu32 count_new_elements); // makes sure there's room for new elements
#endif

////////////////////////////////////////////////////////////////////////////////////////////////
// Forward declarations of functions
////////////////////////////////////////////////////////////////////////////////////////////////
char* flt_strdup_internal(const char* str);
#ifdef FLT_ALIGNED
void* flt_aligned_calloc(size_t nelem, size_t elsize, size_t alignment);
#endif
int flt_err(int err, flt* of);
int flt_read_ophead(fltu16 op, flt_op* data, FILE* f);
void flt_swap_desc(void* data, flt_end_desc* desc);
void flt_node_add(flt* of, flt_node* node);
void flt_node_add_child(flt_node* parent, flt_node* node);
FILE* flt_fopen(const char* filename, flt* of);
char* flt_path_base(const char* filaname);
char* flt_path_basefile(const char* filename);
int flt_path_endsok(const char* filename);
flt_node* flt_node_create(fltu32 hieflags, int nodetype, const char* name);
void flt_release_node(flt_node* n);
void flt_resolve_all_extref(flt* of);
#ifdef FLT_UNIQUE_FACES
void flt_face_destroy_name(char* key, void* faceptr, void* userdata);
#endif
void flt_vertex_write(flt* of, fltu32 vtxoffset);
fltu32 flt_compute_vertex_size(fltu32 palopts);


FLT_RECORD_READER(flt_reader_header);                 // FLT_OP_HEADER
FLT_RECORD_READER(flt_reader_pushlv);                 // FLT_OP_PUSH_LEVEL
FLT_RECORD_READER(flt_reader_face);                   // FLT_OP_FACE
FLT_RECORD_READER(flt_reader_poplv);                  // FLT_OP_POP_LEVEL
FLT_RECORD_READER(flt_reader_group);                  // FLT_OP_GROUP
FLT_RECORD_READER(flt_reader_lod);                    // FLT_OP_LOD
FLT_RECORD_READER(flt_reader_mesh);                   // FLT_OP_MESH
FLT_RECORD_READER(flt_reader_local_vertex_pool);      // FLT_OP_LOCAL_VERTEX_POOL
FLT_RECORD_READER(flt_reader_mesh_primitive);         // FLT_OP_MESH_PRIMITIVE
FLT_RECORD_READER(flt_reader_object);                 // FLT_OP_OBJECT
FLT_RECORD_READER(flt_reader_pal_tex);                // FLT_OP_PAL_TEXTURE
FLT_RECORD_READER(flt_reader_longid);                 // FLT_OP_LONGID
FLT_RECORD_READER(flt_reader_extref);                 // FLT_OP_EXTREF
FLT_RECORD_READER(flt_reader_pal_vertex);             // FLT_OP_PAL_VERTEX
FLT_RECORD_READER(flt_reader_vertex_list);            // FLT_OP_VERTEX_LIST
FLT_RECORD_READER(flt_reader_switch);                 // FLT_OP_SWITCH
FLT_RECORD_READER(flt_reader_continuation);           // FLT_OP_CONTINUATION
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
fltatom32 TOTALINDICES=0;

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
int flt_load_from_filename(const char* filename, flt* of, flt_opts* opts)
{
  flt_op oh;
  flti32 skipbytes;  
  flt_rec_reader readtab[FLT_OP_MAX]={0};  
  flt_context* ctx;
  char use_pal=0, use_node=0;  

  of->loaded = FLT_LOADING;
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
#ifdef FLT_UNIQUE_FACES
  flt_dict_create(opts->dfaces_size ? opts->dfaces_size: FLT_DICTFACES_SIZE,0,&ctx->dictfaces,flt_dict_hash_face_djb2, flt_dict_keycomp_face);
  flt_array_create(&of->indices, opts->indices_size ? opts->indices_size : FLT_INDICES_SIZE, flt_array_grow_double);
#endif
  // opening file
  ctx->f = flt_fopen(filename, of);
  if ( !ctx->f ) return flt_err(FLT_ERR_FOPEN, of);

  // configuring reading
  readtab[FLT_OP_HEADER] = flt_reader_header;         // always read header
  readtab[FLT_OP_PAL_VERTEX] = flt_reader_pal_vertex; // always read vertex palette header for skipping at least
  readtab[FLT_OP_CONTINUATION] = flt_reader_continuation; // always prepared to continue
  if ( opts->pflags & FLT_OPT_PAL_VERTEX )  { use_pal = FLT_TRUE; }
  if ( opts->pflags & FLT_OPT_PAL_TEXTURE ) { readtab[FLT_OP_PAL_TEXTURE] = flt_reader_pal_tex; use_pal=FLT_TRUE; }  
  if ( opts->hflags & FLT_OPT_HIE_EXTREF )  { readtab[FLT_OP_EXTREF] = flt_reader_extref; use_node=FLT_TRUE; }
  if ( opts->hflags & FLT_OPT_HIE_OBJECT )  { readtab[FLT_OP_OBJECT] = flt_reader_object; use_node=FLT_TRUE; }
  if ( opts->hflags & FLT_OPT_HIE_GROUP )   { readtab[FLT_OP_GROUP] = flt_reader_group; use_node=FLT_TRUE; }
  if ( opts->hflags & FLT_OPT_HIE_LOD )     { readtab[FLT_OP_LOD] = flt_reader_lod; use_node=FLT_TRUE; }
  if ( opts->hflags & FLT_OPT_HIE_FACE )    { readtab[FLT_OP_FACE] = flt_reader_face; readtab[FLT_OP_VERTEX_LIST]= flt_reader_vertex_list; use_node=FLT_TRUE;}  
  if (opts->hflags & FLT_OPT_HIE_SWITCH)    { readtab[FLT_OP_SWITCH] = flt_reader_switch; use_node = FLT_TRUE; }
  if ( opts->hflags & FLT_OPT_HIE_MESH )    
  { 
    readtab[FLT_OP_MESH] = flt_reader_mesh; 
    readtab[FLT_OP_LOCAL_VERTEX_POOL] = flt_reader_local_vertex_pool;
    readtab[FLT_OP_MESH_PRIMITIVE] = flt_reader_mesh_primitive;
    use_node=FLT_TRUE; 
  }  
  if (use_pal)
  {
    of->pal = (flt_palettes*)flt_calloc(1,sizeof(flt_palettes));
    flt_mem_check2(of->pal,of);
  }

  if (use_node)
  {
    if (!(ctx->opts->hflags & FLT_OPT_HIE_NO_NAMES))
      readtab[FLT_OP_LONGID] = flt_reader_longid;
    readtab[FLT_OP_PUSHLEVEL] = flt_reader_pushlv;
    readtab[FLT_OP_POPLEVEL] = flt_reader_poplv;

    // hierarchy (root should be always)
    of->hie = (flt_hie*)flt_calloc(1,sizeof(flt_hie));
    flt_mem_check2(of->hie, of);
    of->hie->node_root = flt_node_create(of->ctx->opts->hflags, FLT_NODE_BASE, "root");
    flt_mem_check2(of->hie->node_root, of);
    flt_stack_pushn(ctx->stack, of->hie->node_root);
  }

  // Reading Loop!
  while ( flt_read_ophead(FLT_OP_DONTCARE, &oh, ctx->f) )
  {
    ++ctx->rec_count;
    // if reader function available, use it
    skipbytes = ( readtab[oh.op] && !(opts->hflags&FLT_OPT_HIE_GO_THROUGH) ) ? readtab[oh.op](&oh, of) : oh.length-sizeof(flt_op);    
    ctx->op_last = oh.op;

    if ( opts->countable )
      flt_atomic_inc(opts->countable+oh.op);

    // if returned negative, it's an error, if positive, we skip until next record
    if ( skipbytes < 0 )        return flt_err(FLT_ERR_READBEYOND_REC,of);
    else if ( skipbytes > 0 )   fseek(ctx->f,skipbytes,SEEK_CUR); 
  }

  if (opts->hflags & FLT_OPT_HIE_EXTREF_RESOLVE && of->hie)
    flt_resolve_all_extref(of);  

  flt_stack_popn(ctx->stack); // root
  flt_stack_destroy(&ctx->stack); // no needed anymore (also released in flt_release)
  if (of->pal && of->pal->vtx_array) 
    flt_safefree(of->pal->vtx_buff); // not needed the buffer anymore if we go the array

  of->loaded = FLT_LOADED;
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
  extref->of = (struct flt*)flt_dict_get(of->ctx->dict, extref->base.name, 0);
  if ( !extref->of ) // does not exist, creates one, register in dict and passes dict
  {
    extref->of = (flt*)flt_calloc(1,sizeof(flt));
    extref->of->ctx = of->ctx; 
    flt_dict_insert(of->ctx->dict, extref->base.name, extref->of, FLT_NULL, FLT_NULL, FLT_NULL);

    // Creates the full path for the external reference (uses same base path as parent)
    if (oldctx->basepath)
    {
      *oldctx->tmpbuff=0; 
      strcat(oldctx->tmpbuff, oldctx->basepath);
      if ( !flt_path_endsok(oldctx->basepath) ) strcat(oldctx->tmpbuff, "/");
      strcat(oldctx->tmpbuff, extref->base.name);
      basefile = flt_strdup(oldctx->tmpbuff);    
    }
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
  int i;

  if ( ctx->opts->hflags & FLT_OPT_HIE_HEADER ) 
  {
    // if storing header, read it entirely
    of->header = (flt_header*)flt_malloc(sizeof(flt_header));
    flt_mem_check(of->header, of->errcode);
    leftbytes -= (int)fread(of->header, 1, flt_min(leftbytes,sizeof(flt_header)), ctx->f);  
    flt_swap_desc(of->header,desc); // endianess
    format_rev = of->header->format_rev;
    // remove dtime line breaks
    for (i=0;i<32;++i) if (of->header->dtime[i]=='\n' || of->header->dtime[i]=='\r') of->header->dtime[i]=' ';
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

  flt_stack_pushn(ctx->stack,FLT_NULL);

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_poplv)
{
  flt_context* ctx=of->ctx;
  int leftbytes = oh->length-sizeof(flt_op);  
  flt_stack_popn(ctx->stack);

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
    newextref = (flt_node_extref*)flt_node_create(of->ctx->opts->hflags, FLT_NODE_EXTREF, ctx->tmpbuff);
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
    newobj = (flt_node_object*)flt_node_create(of->ctx->opts->hflags, FLT_NODE_OBJECT, ctx->tmpbuff);
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
    group = (flt_node_group*)flt_node_create(of->ctx->opts->hflags,FLT_NODE_GROUP,ctx->tmpbuff);
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
    lod = (flt_node_lod*)flt_node_create(of->ctx->opts->hflags,FLT_NODE_LOD,ctx->tmpbuff);
    flt_mem_check(lod,of->errcode);

    flt_getswapdbl(lod->switch_in,12);
    flt_getswapdbl(lod->switch_out,20);    
    flt_getswapu32(lod->flags,32);
    dbl+=2; tgdbl=lod->cnt_coords;
    for (i=0;i<5;++i,++dbl){ flt_swap64(dbl); tgdbl[i]=*dbl; }
    
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
  fltu32* u32; flti32* i32; fltu16* u16; flti16* i16;
  flt_face* attr;

  leftbytes -= (int)fread(ctx->tmpbuff,1,leftbytes,ctx->f);
  flt_node_mesh* mesh = (flt_node_mesh*)flt_node_create(ctx->opts->hflags, FLT_NODE_MESH, ctx->tmpbuff);
  flt_mem_check(mesh,of->errcode);
  attr=&mesh->attribs;

  // same as face record but different offsets
  attr->billb = (fltu8)*(ctx->tmpbuff+25);
  flt_getswapi16(attr->texbase_pat, 28);
  flt_getswapi16(attr->texdetail_pat, 26);
  flt_getswapi16(attr->mat_pat, 30);
  flt_getswapi32(attr->flags,44);
  flt_getswapu32(attr->abgr,52);
  flt_getswapu16(attr->shader_ndx,78);
#ifndef FLT_LEAN_FACES  
  flt_getswapi32(attr->ir_color,12);
  flt_getswapi32(attr->ir_mat,36);
  flt_getswapi16(attr->smc_id,32);
  flt_getswapi16(attr->feat_id,34);
  flt_getswapi16(attr->transp, 40);
  flt_getswapu16(attr->cname_ndx,20);
  flt_getswapu16(attr->cnamealt_ndx,22);
  flt_getswapu16(attr->texmapp_ndx,64);
  attr->draw_type = *(fltu8*)(ctx->tmpbuff+18);
  attr->light_mode = *(fltu8*)(ctx->tmpbuff+48);
  attr->lod_gen = *(fltu8*)(ctx->tmpbuff+42);
  attr->linestyle_ndx = *(fltu8*)(ctx->tmpbuff+43);
#endif

  flt_node_add(of, (flt_node*)mesh);

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_local_vertex_pool)
{
  flt_context* ctx=of->ctx;
  int leftbytes = oh->length-sizeof(flt_op);
  fltu32* u32;

  FLT_ASSERT(leftbytes != 0xffff); // there'll be a continuation record, do something!
  flt_node_mesh* mesh = (flt_node_mesh*)flt_stack_topn_not_null(ctx->stack);
  FLT_ASSERT(mesh->base.type==FLT_NODE_MESH);
  if ( mesh->base.type==FLT_NODE_MESH )
  {
    mesh->vb = (flt_mesh_vb*)flt_malloc(sizeof(flt_mesh_vb));
    flt_mem_check(mesh->vb,of->errcode);
    leftbytes -= (int)fread(ctx->tmpbuff,1,sizeof(fltu32)*2,ctx->f);
    flt_getswapu32(mesh->vb->count,0);
    flt_getswapu32(mesh->vb->semantic,4);
  }
  fprintf( stderr, "LOCAL VERTEX POOL NOT SUPPORTED\n" );
  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_mesh_primitive)
{
  //flt_context* ctx=of->ctx;
  int leftbytes = oh->length-sizeof(flt_op);
  //fltu32* u32; flti32* i32; fltu16* u16; flti16* i16;
  fprintf( stderr, "MESH PRIMITIVE NOT SUPPORTED\n" );

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_longid)
{
  flt_context* ctx=of->ctx;
  int leftbytes = oh->length-sizeof(flt_op);

  flt_node* top = flt_stack_topn(ctx->stack);
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
  int leftbytes, palbytes;
  fltu32 vsize = 0;

  // record header
  leftbytes = oh->length - sizeof(flt_op);
  leftbytes -= (int)fread(&palbytes,1,flt_min(leftbytes,4),ctx->f);
  flt_swap32(&palbytes); 
  palbytes -= sizeof(flt_op) + 4;
  leftbytes = palbytes; // size of palette minus header and marker
  
  // read vertices from palette?
  if ( palbytes && opts->pflags & FLT_OPT_PAL_VERTEX )
  {
    // saves the whole vertex
    of->pal->vtx_buff = (fltu8*)flt_malloc( palbytes ); 
    flt_mem_check(of->pal->vtx_buff, of->errcode);
    leftbytes -= (int)fread(of->pal->vtx_buff, 1, palbytes, ctx->f);

    vsize = flt_compute_vertex_size(opts->pflags);
    if (vsize)
    {
      palbytes /= 40; // upper bound of vertices
      of->pal->vtx_array = (fltu8*)flt_malloc(palbytes*vsize);
      flt_mem_check(of->pal->vtx_array, of->errcode);
      of->pal->vtx_count = 0;
    }
  }

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
#pragma warning(disable:4101 4189) // use under gcc: #pragma GCC diagnostic
FLT_RECORD_READER(flt_reader_face)
{
  flt_context* ctx=of->ctx;
  flt_node *parent, *sibling;
  int leftbytes = oh->length-sizeof(flt_op);
  flti32* i32; flti16* i16; fltu16* u16; fltu32* u32;
#ifdef FLT_UNIQUE_FACES
  fltu32 facehash;
  fltu32 hashentry;
  flt_face *lface, *lfacelast;
#else
  flt_node_face* nodeface;
#endif
  flt_face tmpf={0};
  flt_face* face=&tmpf;
  flt_atomic_inc(&TOTALNFACES);

  leftbytes -= (int)fread(ctx->tmpbuff,1,flt_min(leftbytes,76),ctx->f);

  face->billb = (fltu8)*(ctx->tmpbuff+21);
  flt_getswapi16(face->texbase_pat, 22);
  flt_getswapi16(face->texdetail_pat, 24);
  flt_getswapi16(face->mat_pat, 26);
  flt_getswapi32(face->flags,40);
  flt_getswapu32(face->abgr,52);
  flt_getswapu16(face->shader_ndx,74);
#ifndef FLT_LEAN_FACES  
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

#ifdef FLT_UNIQUE_FACES
  // if we compiled for face palettes, let's do the hash thing 
  // we store the face hash in every face node
  facehash=flt_dict_hash_face_djb2((const unsigned char*)face, FLT_FACESIZE_HASH);
  // any face with same hash exists?
  face=(flt_face*)flt_dict_geth(ctx->dictfaces, (const char*) face, FLT_FACESIZE_HASH, facehash, &hashentry);
  if ( !face )
  {
    flt_atomic_inc(&TOTALUNIQUEFACES);
    // if it's unique, add it to dictionary of faces. 
    face = (flt_face*)flt_malloc(sizeof(flt_face));
    flt_mem_check(face,of->errcode);    
    *face = tmpf;
#ifndef FLT_LEAN_FACES
    if ( !(ctx->opts->hflags & FLT_OPT_HIE_NO_NAMES) && *ctx->tmpbuff )
      face->name = flt_strdup(ctx->tmpbuff);
#endif
    flt_dict_insert(ctx->dictfaces, (const char*)face, face, FLT_FACESIZE_HASH, facehash, &hashentry);
  }  

  flt_stack_popn(ctx->stack);
  flt_stack_push32(ctx->stack, hashentry);
#else
  // creating node for every face
  nodeface = (flt_node_face*)flt_node_create(of->ctx->opts->hflags, FLT_NODE_FACE,ctx->tmpbuff);
  flt_mem_check(nodeface,of->errcode);
  // whole face info in node
  nodeface->face = tmpf;
  flt_node_add(of, (flt_node*)nodeface);
  flt_atomic_inc(&TOTALUNIQUEFACES);
#endif
  

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_vertex_list)
{
  flt_context* ctx=of->ctx;
#ifndef FLT_UNIQUE_FACES
  flt_node_vlist* vlistnode;
#endif
  int leftbytes = oh->length-sizeof(flt_op);
  fltu32 n_inds=(oh->length-4)>>2;
  fltu32 i;

#ifdef FLT_UNIQUE_FACES
  const fltu32 max_ninds_read = sizeof(ctx->tmpbuff)/4; // max no of indices can be read at once
  fltu32* inds=(fltu32*)ctx->tmpbuff; // indices array
  fltu32 vtxoffset;
  fltu64* pair;
  fltu32 thisndxstart,thisndxend,ndxstart,ndxend;
  flt_node* parentn;

  if ( !n_inds || !leftbytes ) return 0;

  // we get the top not null, which is my hash entry number for the unique face in the dict
  FLT_ASSERT(n_inds<max_ninds_read);
  fltu32 hashe = flt_stack_top32_not_null(ctx->stack);
  if ( hashe != 0xffffffff )
  {
    // get the parent node from stack
    parentn=flt_stack_topn_not_null(ctx->stack);
    FLT_ASSERT(parentn && "Vertex list with no parent node, skipping");
    if ( parentn )
    {
      thisndxstart=of->indices->size;

      // make sure indices array has enough memory for new indices
      flt_array_ensure(of->indices, n_inds); 

      // only read the capacity of our temporary buffer
      FLT_ASSERT(n_inds <= max_ninds_read); // increase tmpbuff or ignore other indices      
      n_inds = flt_min(n_inds,max_ninds_read);
      leftbytes -= (int)fread(ctx->tmpbuff, 1, n_inds<<2,ctx->f);

      // TRIANGULATE HERE FOR > 3 INDICES !!
      // we got this batch of indices, let's swap it and encode it with the hash entry code
      for (i=0;i<n_inds;++i)
      {
        vtxoffset = inds[i];
        flt_swap32(&vtxoffset);
        vtxoffset -= sizeof(flt_op) + 4; // correct offset
        
        // if there's an array, then the vertices have to be compressed (remove boilerplate opcode data)
        if (of->pal->vtx_array)
        {
          // get the opcode in vertex opcodes buffer
          if (*(fltu16*)(of->pal->vtx_buff + vtxoffset) != 0)
          {
            flt_vertex_write(of, vtxoffset);
          }
          vtxoffset = *(fltu32*)(of->pal->vtx_buff + vtxoffset + 2);
        }
        flt_array_push_back(of->indices, FLTMAKE64(hashe,vtxoffset));
      }

      thisndxend = of->indices->size-1; // last index

      // if no pairs (start/end) creates one
      if (!parentn->ndx_pairs)
      {
        pair = parentn->ndx_pairs = (fltu64*)flt_malloc(sizeof(fltu64));        
        parentn->ndx_pairs_count=1;
      }
      else
      {
        // there are pairs, check if we need to add one more or concat to last one
        pair = parentn->ndx_pairs + (parentn->ndx_pairs_count-1);
        FLTGET32(*pair, ndxstart, ndxend);
        if ( ndxend+1 == thisndxstart )
        {
          // reuse last pair, same start, new end
          thisndxstart=ndxstart;
        }
        else
        {
          // add new pair (regrow array with one more element) (should i double capacity?)
          i=parentn->ndx_pairs_count;
          parentn->ndx_pairs = (fltu64*)flt_realloc(parentn->ndx_pairs, sizeof(fltu64)*(i+1));
          pair += i; // set the last pair
          parentn->ndx_pairs_count= i+1;
        }
      }

      *pair = FLTMAKE64(thisndxstart,thisndxend);
    }
  }  
#else
  // using normal vertex list node, create the node, create the indices array and add the node
  if ( n_inds )
  {
    vlistnode = (flt_node_vlist*)flt_node_create(of->ctx->opts->hflags, FLT_NODE_VLIST, FLT_NULL);
    vlistnode->count = n_inds;
    vlistnode->indices = (fltu32*)flt_malloc(sizeof(fltu32)*n_inds);
    flt_mem_check(vlistnode->indices,of->errcode);
    leftbytes -= (int)fread(vlistnode->indices,1,sizeof(fltu32)*n_inds,ctx->f);
    for (i=0;i<n_inds;++i) { flt_swap32(vlistnode->indices+i); }    
    flt_node_add(of, (flt_node*)vlistnode);
  }
#endif

  flt_atomic_add(&TOTALINDICES, n_inds);
  return leftbytes;
}
#pragma warning(default:4101 4189)

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_switch)
{
  flt_context* ctx = of->ctx;
  flt_node_switch* switchnode;
  int leftbytes = oh->length - sizeof(flt_op);
  fltu32* u32;
  fltu32 i,end;

  leftbytes -= (int)fread(ctx->tmpbuff,1,leftbytes,ctx->f);  
  switchnode = (flt_node_switch*)flt_node_create(ctx->opts->hflags, FLT_NODE_SWITCH, ctx->tmpbuff);
  flt_mem_check(switchnode,of->errcode);

  flt_getswapu32(switchnode->cur_mask,12);
  flt_getswapu32(switchnode->mask_count,16);
  flt_getswapu32(switchnode->wpm,20);
    
  end = switchnode->mask_count * switchnode->wpm;
  switchnode->maskwords = (fltu32*)flt_calloc(1,sizeof(fltu32)*end);
  flt_mem_check(switchnode->maskwords,of->errcode);
  u32 = (fltu32*)(ctx->tmpbuff+24);
  for (i=0;i<end;++i,++u32)
  {
    flt_swap32(u32);
    switchnode->maskwords[i]=*u32;
  }
  flt_node_add(of,(flt_node*)switchnode);

  return leftbytes;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
FLT_RECORD_READER(flt_reader_continuation)
{
  flt_context* ctx=of->ctx;
  int leftbytes = oh->length-sizeof(flt_op);
  
  fltu8* contdata = (fltu8*)flt_malloc(leftbytes);
  leftbytes -= (int)fread(contdata,1,leftbytes,ctx->f);

  FLT_BREAK_ALWAYS; // !! NOT IMPLEMENTED YET !! 
  
  // it will depend on the last record
  switch ( ctx->op_last )
  {
  case FLT_OP_LOCAL_VERTEX_POOL: break;
  case FLT_OP_MESH_PRIMITIVE: break;
  case FLT_OP_VERTEX_LIST: break;
  }
  
  flt_free(contdata);
  return leftbytes;
}


////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
fltu32 flt_compute_vertex_size(fltu32 palopts)
{
  fltu32 size = 0;

  if (palopts & FLT_OPT_PAL_VTX_POSITION)
  {
    if (palopts & FLT_OPT_PAL_VTX_POSITION_SINGLE)
      size += sizeof(float) * 3;
    else
      size += sizeof(double) * 3;
  }

  if (palopts & FLT_OPT_PAL_VTX_NORMAL) size += sizeof(float) * 3;
  if (palopts & FLT_OPT_PAL_VTX_UV)     size += sizeof(float) * 2;
  if (palopts & FLT_OPT_PAL_VTX_COLOR)  size += sizeof(fltu32);

  return size;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_vertex_write(flt* of, fltu32 vtxoffset)
{
  const fltu32 vsize = flt_compute_vertex_size(of->ctx->opts->pflags);
  const fltu8* invtx = of->pal->vtx_buff + vtxoffset;
  fltu16 op = *(fltu16*)(invtx);
  fltu8* outvtx = of->pal->vtx_array + vsize*of->pal->vtx_count;
  const fltu32 flags = of->ctx->opts->pflags;
  double *xyz = FLT_NULL;
  float *normal = FLT_NULL;
  float *uv = FLT_NULL;
  fltu32 *abgr = FLT_NULL;
  double *outd;
  float *outf;
  fltu32 *outc;
  fltu32 offs;

  flt_swap16(&op);
  // getting input data
  switch (op)
  {
  case FLT_OP_VERTEX_COLOR:
    xyz = (double*)(invtx + 8);
    abgr = (fltu32*)(invtx + 32);
    break;
  case FLT_OP_VERTEX_COLOR_NORMAL:
    xyz = (double*)(invtx + 8);
    normal = (float*)(invtx + 32);
    abgr = (fltu32*)(invtx + 44);
    break;
  case FLT_OP_VERTEX_COLOR_UV:
    xyz = (double*)(invtx + 8);
    uv = (float*)(invtx + 32);
    abgr = (fltu32*)(invtx + 40);
    break;
  case FLT_OP_VERTEX_COLOR_NORMAL_UV:
    xyz = (double*)(invtx + 8);
    normal = (float*)(invtx + 32);
    uv = (float*)(invtx + 44);
    abgr = (fltu32*)(invtx + 52);
    break;
  }

  // swaping byte ordering
  flt_swap64(xyz + 0); flt_swap64(xyz + 1); flt_swap64(xyz + 2);
  flt_swap32(abgr);
  if (normal) { flt_swap32(normal); flt_swap32(normal + 1); }
  else { normal = flt_zerovec; }
  if (uv) { flt_swap32(uv); flt_swap32(uv + 1); }
  else { uv = flt_zerovec; }

  // outputting data
  offs = 0;
  if (flags & FLT_OPT_PAL_VTX_POSITION)
  {
    if (flags & FLT_OPT_PAL_VTX_POSITION_SINGLE)
    {
      outf = (float*)(outvtx + offs);
      outf[0] = (float)xyz[0]; outf[1] = (float)xyz[1]; outf[2] = (float)xyz[2];
      offs += sizeof(float) * 3;
    }
    else
    {
      outd = (double*)(outvtx + offs);
      outd[0] = xyz[0]; outd[1] = xyz[1]; outd[2] = xyz[2];
      offs += sizeof(double) * 3;
    }
  }

  if (flags & FLT_OPT_PAL_VTX_NORMAL)
  {
    outf = (float*)(outvtx + offs);
    outf[0] = normal[0]; outf[1] = normal[1]; outf[2] = normal[2];
    offs += sizeof(float) * 3;
  }

  if (flags & FLT_OPT_PAL_VTX_UV)
  {
    outf = (float*)(outvtx + offs);
    outf[0] = uv[0]; outf[1] = uv[1];
    offs += sizeof(float) * 2;
  }

  if (flags & FLT_OPT_PAL_VTX_COLOR)
  {
    outc = (fltu32*)(outvtx + offs);
    *outc = *abgr;
  }

  *(fltu16*)(invtx) = 0;
  *(fltu32*)(invtx + sizeof(fltu16)) = of->pal->vtx_count;
  ++of->pal->vtx_count;
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
#ifdef FLT_UNIQUE_FACES
  flt_array_destroy(&of->indices);
#endif

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
    flt_safefree(of->pal->vtx_buff);    
    flt_safefree(of->pal->vtx_array);

    flt_safefree(of->pal);
  }

  // nodes
  if ( of->hie )
  {
    // releasing node hierarchy
    flt_release_node(of->hie->node_root);
    flt_safefree(of->hie->node_root);
    flt_safefree(of->hie);
  }

  // context
  if ( of->ctx )
  {
#ifdef FLT_UNIQUE_FACES
    // dict of faces
    if ( of->ctx->dictfaces )
      flt_dict_destroy(&of->ctx->dictfaces, FLT_TRUE, flt_face_destroy_name);
#endif
    // stack
    if ( of->ctx->stack )
      flt_stack_destroy(&of->ctx->stack);

    // dict
    if ( of->ctx->dict && flt_atomic_dec(&of->ctx->dict->ref)<=0 )
      flt_dict_destroy(&of->ctx->dict, FLT_FALSE, FLT_NULL);

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
  flt_node_switch* swi;
  flt_node_mesh* mesh;

#if !defined(FLT_UNIQUE_FACES) && !defined(FLT_LEAN_FACES)
  flt_node_face* nodeface;
#endif
#ifndef FLT_UNIQUE_FACES
  flt_node_vlist* vlist;
#endif

  if (!n) return;
  
  flt_safefree(n->name);  

#ifdef FLT_UNIQUE_FACES
  flt_safefree(n->ndx_pairs);
#endif

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
  switch ( n->type )
  {
    case FLT_NODE_EXTREF:
    {
      eref=(flt_node_extref*)n;
      if ( eref->of && flt_atomic_dec(&eref->of->ref)<=0 )
      {
        flt_release(eref->of);
        flt_free(eref->of);        
      }
      eref=FLT_NULL;
    }break;
    case FLT_NODE_MESH:
    {
      mesh = (flt_node_mesh*)n;
      if ( mesh->vb )
      {
        flt_safefree(mesh->vb->vertices);
        flt_safefree(mesh->vb);
      }
    }break;
    case FLT_NODE_SWITCH:
    {
      swi = (flt_node_switch*)n;
      flt_safefree(swi->maskwords);
    }break;
#if !defined(FLT_UNIQUE_FACES) && !defined(FLT_LEAN_FACES) // if there are node of large faces, release name
    case FLT_NODE_FACE:
      {
        nodeface = (flt_node_face*)n;        
        flt_safefree(nodeface->face.name);
      }break;
#endif
#ifndef FLT_UNIQUE_FACES
    case FLT_NODE_VLIST:
    {
      vlist=(flt_node_vlist*)n;
      flt_safefree(vlist->indices);
    }break;
#endif
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef FLT_UNIQUE_FACES
void flt_face_destroy_name(char* key, void* faceptr, void* userdata)
{
  key=key;//unused
  userdata=userdata;
#ifndef FLT_LEAN_FACES
  flt_face* f = (flt_face*)faceptr;
  flt_safefree(f->name);
#endif
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void flt_node_add_child(flt_node* parent, flt_node* node)
{
  FLT_ASSERT(parent && "There should be at least a node-root. some unwanted pop was done");

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
    flt_stack_popn(ctx->stack);
    parent = flt_stack_topn_not_null(ctx->stack);      

    // add to parent
    flt_node_add_child(parent,node);

    // restore stack
    flt_stack_pushn(ctx->stack, node );
  }
  else
    FLT_BREAK;// "shouldn't be here as there's always the node_root"

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
flt_node* flt_node_create(fltu32 hieflags, int nodetype, const char* name)
{
  flt_node* n=0;
  static int nodefacesize = 
#ifdef FLT_UNIQUE_FACES
    0;
#else
    sizeof(flt_node_face);
#endif
  static int nodesizes[FLT_NODE_MAX]={0,sizeof(flt_node), sizeof(flt_node_extref), sizeof(flt_node_group),
    sizeof(flt_node_object), sizeof(flt_node_mesh), sizeof(flt_node_lod), nodefacesize, sizeof(flt_node_vlist), sizeof(flt_node_switch)};
  int size=nodesizes[nodetype];

  n=(flt_node*)flt_calloc(1,size);
  if (n)
  {
    n->type = nodetype;
    if ( (!(hieflags & FLT_OPT_HIE_NO_NAMES) || nodetype==FLT_NODE_EXTREF) && name && *name )
      n->name = flt_strdup(name);
  }
  return n;
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
const char* flt_get_proj_name(fltu32 proj)
{
  static char tmp[16];
#ifndef FLT_NO_OPNAMES
  const char* names[]={"Flat Earth", "Trapezoidal", "Round Earth", "Lambert", "UTM", "Geodetic", "Geocentric"};
  return proj<7?names[proj]:itoa(proj,tmp,10);
#else
  return itoa(proj,tmp,10);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
const char* flt_get_db_origin_name(fltu32 db_orig)
{
  static char tmp[16];
#ifndef FLT_NO_OPNAMES
  switch(db_orig)
  {
  case 100: return "OpenFlight";
  case 200: return "DIG I/II";
  case 300: return "Evans and Sutherland CT5A/CT6";
  case 400: return "PSP DIG";
  case 600: return "General Electric CIV/CV/PT2000";
  case 700: return "Evans and Sutherland GDF";
  default: return itoa(db_orig,tmp,10);
  }
#else
  return itoa(db_orig,tmp,10);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
const char* flt_get_vcoords_units_name(fltu8 vcu)
{
  static char tmp[16];
#ifndef FLT_NO_OPNAMES
  static const char* names[]={"Meters", "Kilometers", "", "", "Feet", "Inches", "", "", "Nautical miles"};
  return vcu<9?names[vcu]:itoa(vcu,tmp,10);
#else
  return itoa(vcu,tmp,10);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
const char* flt_get_earth_ellip_name(int eem)
{
  static char tmp[16];
#ifndef FLT_NO_OPNAMES
  if ( eem==-1 ) return "User Defined";
  static const char* names[]={"WGS 1984", "WGS 1972", "Bessel", "Clarke 1866", "NAD1927"};
  return eem>=0 && eem<5?names[eem]:itoa(eem,tmp,10);
#else
  return itoa(eem,tmp,10);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
const char* flt_get_face_drawtype(fltu16 drawtype)
{
  static char tmp[16];
#ifndef FLT_NO_OPNAMES
  static const char* names[]={"Draw Solid Culling", "Draw Solid DSided", "Draw Wireframe Close", 
    "Draw Wireframe", "Sorround WF AltColor","", "", "",  "Omni light", "Unidir light", "Bidir light"};
  return drawtype<11?names[drawtype]:itoa(drawtype,tmp,10);
#else
  return itoa(drawtype,tmp,10);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
const char* flt_get_face_lightmode(fltu16 lm)
{
  static char tmp[16];
#ifndef FLT_NO_OPNAMES
  static const char* names[]={"Flat", "Gouraud", "Lit", "Lit-Gouraud"};
  return lm<4?names[lm]:itoa(lm,tmp,10);
#else
  return itoa(lm,tmp,10);
#endif
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
const char* flt_get_header_flags_name(flti32 flags)
{
  static char tmp[64];
#ifndef FLT_NO_OPNAMES
  *tmp='['; *(tmp+1)=0;  
  if ( flags & (1<<31) )  sprintf(tmp, "%s%s", tmp, "0" );
  if ( flags & (1<<30) )  sprintf(tmp, "%s | %s", tmp, "1");
  if ( flags & (1<<29) )  sprintf(tmp, "%s | %s", tmp, "2" );
  sprintf(tmp, "%s]", tmp );
  return tmp;
#else
  return itoa(flags,tmp,16);
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
// custom strdup implementation making use of optionally user defined malloc
////////////////////////////////////////////////////////////////////////////////////////////////
char* flt_strdup_internal(const char* str)
{
  const int len=(int)strlen(str);
  char* outstr=(char*)flt_malloc(len+1);
  memcpy(outstr, str, len+1);
  return outstr;
}

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef FLT_ALIGNED
void* flt_aligned_calloc(size_t nelem, size_t elsize, size_t alignment)
{
  void* mem;
  elsize *= nelem;
  mem=_aligned_malloc(elsize,alignment);
  if (mem)
    memset(mem,0,elsize);
  return mem;
}
#endif
////////////////////////////////////////////////////////////////////////////////////////////////
//                                DICTIONARY
////////////////////////////////////////////////////////////////////////////////////////////////
// djb2
fltu32 flt_dict_hash_djb2(const unsigned char* str, int unused) 
{
  unused;
  fltu32 hash = 5381;
  int c;
  while (c = *str++) hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  return hash;
}

fltu32 flt_dict_hash_face_djb2(const unsigned char* data, int size)
{
  fltu32 hash = 5381;
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

void flt_dict_visit(flt_dict*dict, flt_dict_visitor visitor, void* userdata)
{
  int i;
  flt_dict_node *n;

  if ( dict->cs ) flt_critsec_enter(dict->cs);
  i=dict->capacity;
  while(i--)
  {
    n=dict->hasht[i];
    while (n)
    {
      visitor(n->key, n->value, userdata);
      n=n->next;
    }
  }
  if ( dict->cs ) flt_critsec_leave(dict->cs);
}

void flt_dict_destroy(flt_dict** dict, int free_elms, flt_dict_visitor destructor)
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
      if ( destructor )
        destructor(n->key, n->value, FLT_NULL);
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

// caller saves hash computation and possible comparison in flat list
void* flt_dict_gethe(flt_dict* dict, fltu32 hashe, fltu32* hashkey)
{
  flt_dict_node *n;
  fltu16 entry, entryoff;
  FLTGET16(hashe,entryoff,entry);
  if ( dict->cs ) flt_critsec_enter(dict->cs);
  n = dict->hasht[entry];  
  while(entryoff--)
    n=n->next;  
  if ( dict->cs ) flt_critsec_leave(dict->cs);
  if(hashkey) *hashkey=n->keyhash;
  return n->value;
}

// caller can save hash computation
void* flt_dict_geth(flt_dict* dict, const char* key, int size, fltu32 hash, fltu32* hashe)
{
  flt_dict_node *n;
  const int entry = hash % dict->capacity;
  fltu16 hoff=0;

  if ( dict->cs ) flt_critsec_enter(dict->cs);
  n = dict->hasht[entry];  
  while(n)
  {
    if ( /*n->keyhash == hash &&*/ dict->keycomp(n->key,key,size)==0 ){ break; }
    ++hoff;
    n=n->next;
  }
  if ( dict->cs ) flt_critsec_leave(dict->cs);
  *hashe = FLTMAKE32(hoff,(fltu16)entry);
  return n ? n->value : FLT_NULL;
}

// this perform hash computation
void* flt_dict_get(flt_dict* dict, const char* key, int size)
{
  fltu32 he;
  return flt_dict_geth(dict,key,size,dict->hashf((const unsigned char*)key,size),&he);
}

flt_dict_node* flt_dict_create_node(const char* key, fltu32 keyhash, void* value, int size)
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

int flt_dict_insert(flt_dict* dict, const char* key, void* value, fltopt int size, fltopt fltu32 hash, fltopt fltu32* hashentry)
{
  flt_dict_node *n, *ln;
  fltu32 hashk;
  int entry;
  fltu16 hoff=0;

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
      ++hoff;
      ln=n;
      n=n->next;
    }    
    if (ln) ln->next = flt_dict_create_node(key,hashk,value,size);
  }

  if ( hashentry )
    *hashentry = FLTMAKE32( hoff, (fltu16)entry ) ;

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

flti32 flt_atomic_dec(fltatom32* c)
{
  return InterlockedDecrement(c);
}

flti32 flt_atomic_inc(fltatom32* c)
{
  return InterlockedIncrement(c);
}

void flt_atomic_add(fltatom32* c, fltu32 val)
{
  InterlockedExchangeAdd(c,(LONG)val);
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

flti32 flt_atomic_dec(fltatom32* c)
{
  return __sync_sub_and_fetch(c,1);
}

flti32 flt_atomic_inc(fltatom32* c)
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

  _s->entries = (flt_stack_type*)flt_malloc(sizeof(flt_stack_type)*capacity);
  if (!_s->entries) return  FLT_FALSE;
  _s->capacity = capacity;

  *s = _s;
  return FLT_TRUE;
}

void flt_stack_destroy(flt_stack** s)
{
  if ( !s || !*s ) return;

  flt_safefree((*s)->entries);
  flt_safefree((*s));
}

void flt_stack_push32(flt_stack* s, fltu32 val)
{
#ifdef FLT_PLATFORM_64  
  flt_stack_type* se;
#endif
  FLT_ASSERT(s->count < s->capacity && "stack ran out of memory, increase FLT_STACK_SIZE");

#ifdef FLT_PLATFORM_64
  se = s->entries+(s->count++);
  se->val32 = val;
  se->node = FLT_NULL;
#else
  s->entries[s->count++]=FLTMAKE64(FLT_NODE_NONE,val);
#endif
}

void flt_stack_pushn(flt_stack* s, flt_node* node)
{
#ifdef FLT_PLATFORM_64  
  flt_stack_type* se;
#endif
  FLT_ASSERT(s->count < s->capacity && "stack ran out of memory, increase FLT_STACK_SIZE");

#ifdef FLT_PLATFORM_64  
  se = s->entries+(s->count++);
  se->val32=0; 
  se->node=node;
#else
  s->entries[s->count++]=node ? FLTMAKE64(node->type,node) : 0;
#endif
}

flt_node* flt_stack_topn(flt_stack* s)
{
  flt_node* ret=FLT_NULL;
#ifdef FLT_PLATFORM_64
  if (s->count)
    ret = s->entries[s->count-1].node;
#else
  fltu32 type,val;
  if (s->count)
  {
    FLTGET32(s->entries[s->count-1],type,val);
    if (type)
      ret=(flt_node*)val;
  }
#endif
  return ret;
}

fltu32 flt_stack_top32_not_null(flt_stack* s)
{
  int i=s->count-1;
#ifdef FLT_PLATFORM_64
  while(i>=0)
  {
    if ( s->entries[i].val32 && !s->entries[i].node )
      return s->entries[i].val32;
    --i;
  }
#else
  fltu32 type,val;

  while (i>=0)
  {
    if ( s->entries[i] )
    {
      FLTGET32(s->entries[i],type,val);
      if (type==FLT_NODE_NONE)
        return val;
    }
    --i;
  }
#endif
  return 0xffffffff;
}


flt_node* flt_stack_topn_not_null(flt_stack* s)
{
  int i=s->count-1;
#ifdef FLT_PLATFORM_64
  while(i>=0)
  {
    if ( s->entries[i].node )
      return s->entries[i].node;
    --i;
  }
#else
  fltu32 type,val;
  
  while (i>=0)
  {
    if ( s->entries[i] )
    {
      FLTGET32(s->entries[i],type,val);
      if (type!=FLT_NODE_NONE)
        return (flt_node*)val;
    }
    --i;
  }
#endif
  return FLT_NULL;
}

flt_node* flt_stack_popn(flt_stack* s)
{
  flt_node* v=FLT_NULL;
#ifdef FLT_PLATFORM_64
  if (s->count)
    v=s->entries[--s->count].node;
#else
  fltu32 type,val;
  if (s->count)
  {
    --s->count;
    FLTGET32(s->entries[s->count],type,val);
    if (type)
      v = (flt_node*)val;
  }
#endif
  return v;
}

////////////////////////////////////////////////////////////////////////////////////////////////
//                                    ARRAY  
////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef FLT_UNIQUE_FACES
int flt_array_create(flt_array** arr, fltu32 capacity, flt_array_growpolicy grow)
{
  *arr = (flt_array*)flt_calloc(1,sizeof(flt_array));
  if ( !*arr ) return FLT_FALSE;
  (*arr)->capacity = capacity>0?capacity:FLT_ARRAY_INITCAP;
  (*arr)->size = 0;
  (*arr)->data = (flt_array_type*)flt_malloc(sizeof(flt_array_type)*capacity);
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
  fltu32 newcap = arr->capacity*2;
  arr->data = (flt_array_type*)flt_realloc(arr->data, sizeof(flt_array_type)*newcap);
  if( !arr->data ) { FLT_BREAK; return; }
  arr->capacity = newcap;
}

void flt_array_push_back(flt_array* arr, flt_array_type elem)
{
  if (arr->size>=arr->capacity)
    arr->growpolicy(arr);    
  arr->data[arr->size++]=elem;
}

flt_array_type flt_array_pop_back(flt_array* arr)
{
  if ( arr->size==0 ) return FLT_NULL;
  return arr->data[--arr->size];
}

flt_array_type flt_array_at(flt_array* arr, fltu32 index)
{
  return index >= 0 && index < arr->size ? arr->data[index] : FLT_NULL;
}

void flt_array_clear(flt_array* arr)
{
  arr->size=0;
}

void flt_array_ensure(flt_array* arr, fltu32 cout_new_elements)
{
  const fltu32 newcap = arr->size+cout_new_elements;
  if ( newcap <= arr->capacity ) return;
  arr->capacity = newcap;
  flt_array_grow_double(arr); // ensure makes use directly of grow_double policy
}
#endif

/*
bool nfltIsOpcodeObsolete(unsigned short opcode)
{
return opcode==3 || (opcode>=6 && opcode<=9) || (opcode>=12 && opcode<=13)
|| (opcode>=16 && opcode<=17) || (opcode>=40 && opcode<=48) || opcode==51
|| (opcode>=65 && opcode<=66) || opcode==77;
}

*/

////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
#ifdef FLT_WRITER
int flt_write_8(struct flt* of, void* _u8)
{
  fltu8 u8 = *(fltu8*)(_u8);  
  return (int)fwrite(&u8,1,sizeof(fltu8),of->ctx->f);
}

int flt_write_16(struct flt* of, void* _u16)
{
  fltu16 u16 = *(fltu16*)(_u16);
  flt_swap16(&u16);
  return (int)fwrite(&u16,1,sizeof(fltu16),of->ctx->f);
}

int flt_write_32(struct flt* of, void* _u32)
{
  fltu32 u32= *(fltu32*)(_u32);
  flt_swap32(&u32);
  return (int)fwrite(&u32,1,sizeof(fltu32),of->ctx->f);
}

int flt_write_64(struct flt* of, void* _u64)
{
  fltu64 u64= *(fltu64*)(_u64);
  flt_swap64(&u64);
  return (int)fwrite(&u64,1,sizeof(fltu64),of->ctx->f);
}

int flt_write_char(struct flt* of, const char* _c, int len)
{
  const int inlen = _c ? strlen(_c) : 0;
  const int minlen=flt_min(inlen,len-1);
  int i=0;
  int ret=0;
  char zero=0;
  
  for (;i<minlen;++i)
    ret += (int)fwrite(_c+i,1,1,of->ctx->f);
  
  for ( ;i<len;++i)
    ret += (int)fwrite(&zero,1,1,of->ctx->f);
  
  return ret;
}

int flt_write_op(struct flt* of, fltu16 op, fltu16 len)
{
  int r=0;
  flt_swap16(&op); flt_swap16(&len);
  r+=(int)fwrite(&op,1,sizeof(fltu16),of->ctx->f);
  r+=(int)fwrite(&len,1,sizeof(fltu16),of->ctx->f);
  return r;
}

void flt_write_header(struct flt* of)
{
  int i;
  fltu32 u32=0;
  flt_header* h =of->header;
  flti32 written=0;

  written = flt_write_op(of,FLT_OP_HEADER,324);
  written+=flt_write_char(of,h->ascii,8);
  written+=flt_write_32(of,&h->format_rev);
  written+=flt_write_32(of,&h->edit_rev);
  written+=flt_write_char(of,h->dtime,32);
  written+=flt_write_16(of,&h->n_group_nid);
  written+=flt_write_16(of,&h->n_lod_nid);
  written+=flt_write_16(of,&h->n_obj_nid);
  written+=flt_write_16(of,&h->n_face_nid);
  written+=flt_write_16(of,&h->unit_mult);
  written+=flt_write_8(of,&h->v_coords_units);
  written+=flt_write_8(of,&h->texwhite_new_faces);
  written+=flt_write_32(of,&h->flags);
  for (i=0; i<6; ++i) written+=flt_write_32(of,&u32);
  written+=flt_write_32(of,&h->proj_type);
  for (i=0; i<7; ++i) written+=flt_write_32(of,&u32);
  written+=flt_write_16(of,&h->n_dof_nid);
  written+=flt_write_16(of,&h->v_storage_type);
  written+=flt_write_32(of,&h->db_orig);
  written+=flt_write_64(of,&h->sw_db_x);
  written+=flt_write_64(of,&h->sw_db_y);
  written+=flt_write_64(of,&h->d_db_x);
  written+=flt_write_64(of,&h->d_db_y);
  written+=flt_write_16(of,&h->n_sound_nid);
  written+=flt_write_16(of,&h->n_path_nid);
  for (i=0; i<2; ++i) written+=flt_write_32(of,&u32);
  written+=flt_write_16(of,&h->n_clip_nid);
  written+=flt_write_16(of,&h->n_text_nid);
  written+=flt_write_16(of,&h->n_bsp_nid);
  written+=flt_write_16(of,&h->n_switch_nid);
  written+=flt_write_32(of,&u32);
  written+=flt_write_64(of,&h->sw_corner_lat);
  written+=flt_write_64(of,&h->sw_corner_lon);
  written+=flt_write_64(of,&h->ne_corner_lat);
  written+=flt_write_64(of,&h->ne_corner_lon);
  written+=flt_write_64(of,&h->orig_lat);
  written+=flt_write_64(of,&h->orig_lon);
  written+=flt_write_64(of,&h->lbt_upp_lat);
  written+=flt_write_64(of,&h->lbt_low_lat);
  written+=flt_write_16(of,&h->n_lightsrc_nid);
  written+=flt_write_16(of,&h->n_lightpnt_nid);
  written+=flt_write_16(of,&h->n_road_nid);
  written+=flt_write_16(of,&h->n_cat_nid);
  for (i=0; i<2; ++i) written+=flt_write_32(of,&u32);
  written+=flt_write_32(of,&h->earth_ellip_model);
  written+=flt_write_16(of,&h->n_adapt_nid);
  written+=flt_write_16(of,&h->n_curve_nid);
  written+=flt_write_16(of,&h->utm_zone);
  written+=flt_write_char(of,(char*)h->reserved5,6);
  written+=flt_write_64(of, &h->d_db_z);
  written+=flt_write_64(of, &h->radius);
  written+=flt_write_16(of,&h->n_mesh_nid);
  written+=flt_write_16(of,&h->n_lightpnt_sys_nid);
  written+=flt_write_32(of,&h->reserved6);
  written+=flt_write_64(of, &h->earth_major_axis);
  written+=flt_write_64(of, &h->earth_minor_axis);

  FLT_ASSERT(written==324);
}

void flt_write_extref(struct flt* of, flt_node_extref* xref)
{
  fltu32 u32=0;
  fltu16 u16=0;
  flti32 written=0;

  written+=flt_write_op(of, FLT_OP_EXTREF, 216);
  written+=flt_write_char(of, xref->base.name,200);
  written+=flt_write_32(of, &u32);
  written+=flt_write_32(of, &xref->flags);
  written+=flt_write_16(of, &xref->view_asbb);
  written+=flt_write_16(of, &u16);

  FLT_ASSERT(written==216);
}

void flt_write_group(struct flt* of, flt_node_group* group)
{
  fltu32 u32=0; fltu16 u16=0; fltu8 u8=0;
  flti32 written=0;

  written+=flt_write_op(of, FLT_OP_GROUP,44);
  written+=flt_write_char(of,group->base.name,8);
  written+=flt_write_16(of,&group->priority);
  written+=flt_write_16(of,&u16);
  u32=group->flags;
  written+=flt_write_32(of,&u32);
  written+=flt_write_16(of,&u16);
  written+=flt_write_16(of,&u16);
  written+=flt_write_16(of,&u16);
  written+=flt_write_8(of,&u8);
  written+=flt_write_8(of,&u8);
  written+=flt_write_32(of,&u32);
  written+=flt_write_32(of,&group->loop_count);
  written+=flt_write_32(of,&group->loop_dur);
  written+=flt_write_32(of,&group->last_fr_dur);
  FLT_ASSERT(written==44);
}

void flt_write_lod(struct flt* of, flt_node_lod* lod)
{
  fltu32 u32=0; fltu16 u16=0;
  flti32 written=0;

  written+=flt_write_op(of,FLT_OP_LOD,80);
  written+=flt_write_char(of,lod->base.name,8);
  written+=flt_write_32(of,&u32);
  written+=flt_write_64(of,&lod->switch_in);
  written+=flt_write_64(of,&lod->switch_out);
  written+=flt_write_16(of,&u16);
  written+=flt_write_16(of,&u16);
  written+=flt_write_32(of,&lod->flags);
  written+=flt_write_64(of,&lod->cnt_coords[0]);
  written+=flt_write_64(of,&lod->cnt_coords[1]);
  written+=flt_write_64(of,&lod->cnt_coords[2]);
  written+=flt_write_64(of,&lod->trans_range);
  written+=flt_write_64(of,&lod->sig_size);
  
  FLT_ASSERT(written==80);
}

void flt_write_object(struct flt* of, flt_node_object* obj)
{

}

void flt_write_mesh(struct flt* of, flt_node_mesh* mesh)
{

}

void flt_write_face(struct flt* of, flt_node_face* face)
{

}

void flt_write_vlist(struct flt* of, flt_node_vlist* vlist)
{

}

void flt_write_switch(struct flt* of, flt_node_switch* swi)
{

}

void flt_write_node(struct flt* of, struct flt_node* n)
{
  if ( !n ) return;
  flt_write_op(of,FLT_OP_PUSHLEVEL,4);

  while (n)
  {
    switch ( n->type )
    {
    case FLT_NODE_EXTREF: flt_write_extref(of,(flt_node_extref*)n); break;
    case FLT_NODE_GROUP: flt_write_group(of,(flt_node_group*)n); break;
    case FLT_NODE_OBJECT: flt_write_object(of,(flt_node_object*)n);break; 
    case FLT_NODE_MESH: flt_write_mesh(of,(flt_node_mesh*)n);break;
    case FLT_NODE_LOD: flt_write_lod(of,(flt_node_lod*)n); break;
    case FLT_NODE_FACE: flt_write_face(of,(flt_node_face*)n);break;
    case FLT_NODE_VLIST: flt_write_vlist(of,(flt_node_vlist*)n);break;
    case FLT_NODE_SWITCH: flt_write_switch(of,(flt_node_switch*)n);break;
    }

    flt_write_node(of, n->child_head);
    n=n->next;
  }

  flt_write_op(of,FLT_OP_POPLEVEL,4);
}

int flt_write_to_filename(struct flt* of)
{
  flt_node* rootnode=FLT_NULL;

  if ( !of->ctx )
    of->ctx = (flt_context*)flt_calloc(1,sizeof(flt_context));

  of->ctx->f = fopen(of->filename, "wb");
  if ( !of->ctx->f ) return flt_err(FLT_ERR_FOPEN,of); 

  // header
  if ( of->header )
  {
    flt_write_header(of);
  }

  // palettes here

  // hierarchy of nodes
  if ( of->hie )
  {
    if ( of->hie->node_root )
    {
      if ( of->hie->node_root->type == FLT_NODE_BASE )
        rootnode = of->hie->node_root->child_head;
      else
        rootnode = of->hie->node_root;
    }

    if ( rootnode )
      flt_write_node(of,rootnode);
  }

  fclose(of->ctx->f);
  return FLT_TRUE;
}
#endif

#endif

#ifdef _MSC_VER
#pragma warning(default:4244 4100 4996 4706 4091 4310)
#endif

#endif