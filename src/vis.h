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

#ifndef _VIS_H_
#define _VIS_H_

#if defined(VIS_GUI) && !defined(VIS_DX12)
#include <imgui.h>
# if defined(VIS_DX11)
# include <imgui_impl_dx11.h>
# elif defined(VIS_GL)
# include <imgui_impl_glfw.h>
# endif
#endif

#ifndef vis_malloc 
# define vis_malloc(sz) malloc(sz)
#endif

#ifndef vis_free
# define vis_free(ptr) free(ptr)
#endif

#ifndef vis_realloc
# define vis_realloc(ptr,sz) realloc(ptr,sz);
#endif

#ifndef vis_calloc
# define vis_calloc(c,sz) calloc(c,sz)
#endif

#if defined(_DEBUG) || defined(DEBUG)
# ifdef _MSC_VER
# define VIS_BREAK { __debugbreak(); }
# else
# define VIS_BREAK { raise(SIGTRAP); }
# endif
#else
# define VIS_BREAK
#endif

#define vis_unused(p) (p)=(p)
#define vis_safefree(p) { if (p){ vis_free(p); (p)=0; } }
#define vis_saferelease(p){ if (p){ p->Release(); (p)=0;} }
#ifdef VIS_NO_MEMOUT_CHECK
#define vis_mem_check(p)
#else
#define vis_mem_check(p) { if ( !(p) ) { VIS_BREAK; } }
#endif

#define VIS_TRUE  (1)
#define VIS_FALSE (0)
#define VIS_NULL  (0)

//// ERROR CODES /////
#define VIS_OK (0)
#define VIS_FAIL (-1)

#ifndef viscalar
#define viscalar float
#endif

//////////////////////////////////////////////////////////////////////////////
//////////////////////////// TYPES          //////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
#if defined(VIS_DX11) || defined(VIS_DX12) || defined(VIS_VK) || defined(VIS_GL)
#define VIS_MAX_RT 8
#endif

#define VIS_NONE 0
#define VIS_ENABLED 1
#define VIS_DISABLED 0
#define VIS_LOAD_SOURCE_MEMORY 0
#define VIS_LOAD_SOURCE_FILE 1

#define VIS_TYPE_VERTEXBUFFER 1
#define VIS_TYPE_INDEXBUFFER 2
#define VIS_TYPE_SHADER 3
#define VIS_TYPE_PIPELINE 4
#define VIS_TYPE_INPUT_LAYOUT 5
#define VIS_TYPE_SHADER_LAYOUT 6
#define VIS_TYPE_COMMAND_LIST 7
#define VIS_TYPE_FENCE 8
#define VIS_TYPE_STAGING_RES 9
#define VIS_TYPE_RENDER_TARGET 10

#define VIS_FORMAT_UNKNOWN 0
#define VIS_FORMAT_R32G32B32A32_TYPELESS 1
#define VIS_FORMAT_R32G32B32A32_FLOAT 2
#define VIS_FORMAT_R32G32B32A32_UINT 3
#define VIS_FORMAT_R32G32B32A32_SINT 4
#define VIS_FORMAT_R32G32B32_TYPELESS 5
#define VIS_FORMAT_R32G32B32_FLOAT 6
#define VIS_FORMAT_R32G32B32_UINT 7
#define VIS_FORMAT_R32G32B32_SINT 8
#define VIS_FORMAT_R16G16B16A16_TYPELESS 9
#define VIS_FORMAT_R16G16B16A16_FLOAT 10
#define VIS_FORMAT_R16G16B16A16_UNORM 11
#define VIS_FORMAT_R16G16B16A16_UINT 12
#define VIS_FORMAT_R16G16B16A16_SNORM 13
#define VIS_FORMAT_R16G16B16A16_SINT 14
#define VIS_FORMAT_R32G32_TYPELESS 15
#define VIS_FORMAT_R32G32_FLOAT 16
#define VIS_FORMAT_R32G32_UINT 17
#define VIS_FORMAT_R32G32_SINT 18
#define VIS_FORMAT_R32G8X24_TYPELESS 19
#define VIS_FORMAT_D32_FLOAT_S8X24_UINT 20
#define VIS_FORMAT_R32_FLOAT_X8X24_TYPELESS 21
#define VIS_FORMAT_X32_TYPELESS_G8X24_UINT 22
#define VIS_FORMAT_R10G10B10A2_TYPELESS 23
#define VIS_FORMAT_R10G10B10A2_UNORM 24
#define VIS_FORMAT_R10G10B10A2_UINT 25
#define VIS_FORMAT_R11G11B10_FLOAT 26
#define VIS_FORMAT_R8G8B8A8_TYPELESS 27
#define VIS_FORMAT_R8G8B8A8_UNORM 28
#define VIS_FORMAT_R8G8B8A8_UNORM_SRGB 29
#define VIS_FORMAT_R8G8B8A8_UINT 30
#define VIS_FORMAT_R8G8B8A8_SNORM 31
#define VIS_FORMAT_R8G8B8A8_SINT 32
#define VIS_FORMAT_R16G16_TYPELESS 33
#define VIS_FORMAT_R16G16_FLOAT 34
#define VIS_FORMAT_R16G16_UNORM 35
#define VIS_FORMAT_R16G16_UINT 36
#define VIS_FORMAT_R16G16_SNORM 37
#define VIS_FORMAT_R16G16_SINT 38
#define VIS_FORMAT_R32_TYPELESS 39
#define VIS_FORMAT_D32_FLOAT 40
#define VIS_FORMAT_R32_FLOAT 41
#define VIS_FORMAT_R32_UINT 42
#define VIS_FORMAT_R32_SINT 43
#define VIS_FORMAT_R24G8_TYPELESS 44
#define VIS_FORMAT_D24_UNORM_S8_UINT 45
#define VIS_FORMAT_R24_UNORM_X8_TYPELESS 46
#define VIS_FORMAT_X24_TYPELESS_G8_UINT 47
#define VIS_FORMAT_R8G8_TYPELESS 48
#define VIS_FORMAT_R8G8_UNORM 49
#define VIS_FORMAT_R8G8_UINT 50
#define VIS_FORMAT_R8G8_SNORM 51
#define VIS_FORMAT_R8G8_SINT 52
#define VIS_FORMAT_R16_TYPELESS 53
#define VIS_FORMAT_R16_FLOAT 54
#define VIS_FORMAT_D16_UNORM 55
#define VIS_FORMAT_R16_UNORM 56
#define VIS_FORMAT_R16_UINT 57
#define VIS_FORMAT_R16_SNORM 58
#define VIS_FORMAT_R16_SINT 59
#define VIS_FORMAT_R8_TYPELESS 60
#define VIS_FORMAT_R8_UNORM 61
#define VIS_FORMAT_R8_UINT 62
#define VIS_FORMAT_R8_SNORM 63
#define VIS_FORMAT_R8_SINT 64
#define VIS_FORMAT_A8_UNORM 65
#define VIS_FORMAT_R1_UNORM 66
#define VIS_FORMAT_R9G9B9E5_SHAREDEXP 67
#define VIS_FORMAT_R8G8_B8G8_UNORM 68
#define VIS_FORMAT_G8R8_G8B8_UNORM 69
#define VIS_FORMAT_BC1_TYPELESS 70
#define VIS_FORMAT_BC1_UNORM 71
#define VIS_FORMAT_BC1_UNORM_SRGB 72
#define VIS_FORMAT_BC2_TYPELESS 73
#define VIS_FORMAT_BC2_UNORM 74
#define VIS_FORMAT_BC2_UNORM_SRGB 75
#define VIS_FORMAT_BC3_TYPELESS 76
#define VIS_FORMAT_BC3_UNORM 77
#define VIS_FORMAT_BC3_UNORM_SRGB 78
#define VIS_FORMAT_BC4_TYPELESS 79
#define VIS_FORMAT_BC4_UNORM 80
#define VIS_FORMAT_BC4_SNORM 81
#define VIS_FORMAT_BC5_TYPELESS 82
#define VIS_FORMAT_BC5_UNORM 83
#define VIS_FORMAT_BC5_SNORM 84
#define VIS_FORMAT_B5G6R5_UNORM 85
#define VIS_FORMAT_B5G5R5A1_UNORM 86
#define VIS_FORMAT_B8G8R8A8_UNORM 87
#define VIS_FORMAT_B8G8R8X8_UNORM 88
#define VIS_FORMAT_R10G10B10_XR_BIAS_A2_UNORM 89
#define VIS_FORMAT_B8G8R8A8_TYPELESS 90
#define VIS_FORMAT_B8G8R8A8_UNORM_SRGB 91
#define VIS_FORMAT_B8G8R8X8_TYPELESS 92
#define VIS_FORMAT_B8G8R8X8_UNORM_SRGB 93
#define VIS_FORMAT_BC6H_TYPELESS 94
#define VIS_FORMAT_BC6H_UF16 95
#define VIS_FORMAT_BC6H_SF16 96
#define VIS_FORMAT_BC7_TYPELESS 97
#define VIS_FORMAT_BC7_UNORM 98
#define VIS_FORMAT_BC7_UNORM_SRGB 99
#define VIS_FORMAT_AYUV 100
#define VIS_FORMAT_Y410 101
#define VIS_FORMAT_Y416 102
#define VIS_FORMAT_NV12 103
#define VIS_FORMAT_P010 104
#define VIS_FORMAT_P016 105
#define VIS_FORMAT_420_OPAQUE 106
#define VIS_FORMAT_YUY2 107
#define VIS_FORMAT_Y210 108
#define VIS_FORMAT_Y216 109
#define VIS_FORMAT_NV11 110
#define VIS_FORMAT_AI44 111
#define VIS_FORMAT_IA44 112
#define VIS_FORMAT_P8 113
#define VIS_FORMAT_A8P8 114
#define VIS_FORMAT_B4G4R4A4_UNORM 115
#define VIS_FORMAT_P208 130
#define VIS_FORMAT_V208 131
#define VIS_FORMAT_V408 132

#define VIS_BLEND_ZERO            1
#define VIS_BLEND_ONE             2
#define VIS_BLEND_SRC_COLOR       3
#define VIS_BLEND_INV_SRC_COLOR   4
#define VIS_BLEND_SRC_ALPHA       5
#define VIS_BLEND_INV_SRC_ALPHA   6
#define VIS_BLEND_DEST_ALPHA      7
#define VIS_BLEND_INV_DEST_ALPHA  8
#define VIS_BLEND_DEST_COLOR      9
#define VIS_BLEND_INV_DEST_COLOR  10
#define VIS_BLEND_SRC_ALPHA_SAT   11
#define VIS_BLEND_BLEND_FACTOR    14
#define VIS_BLEND_INV_BLEND_FACTOR 15
#define VIS_BLEND_SRC1_COLOR       16
#define VIS_BLEND_INV_SRC1_COLOR   17
#define VIS_BLEND_SRC1_ALPHA       18
#define VIS_BLEND_INV_SRC1_ALPHA   19

#define VIS_BLEND_OP_ADD          1
#define VIS_BLEND_OP_SUBTRACT     2
#define VIS_BLEND_OP_REV_SUBTRACT 3
#define VIS_BLEND_OP_MIN          4
#define VIS_BLEND_OP_MAX          5

#define VIS_LOGIC_OP_CLEAR            0
#define VIS_LOGIC_OP_SET              1
#define VIS_LOGIC_OP_COPY             2
#define VIS_LOGIC_OP_COPY_INVERTED    3
#define VIS_LOGIC_OP_NOOP             4
#define VIS_LOGIC_OP_INVERT           5
#define VIS_LOGIC_OP_AND              6
#define VIS_LOGIC_OP_NAND             7
#define VIS_LOGIC_OP_OR               8
#define VIS_LOGIC_OP_NOR              9
#define VIS_LOGIC_OP_XOR              10
#define VIS_LOGIC_OP_EQUIV            11
#define VIS_LOGIC_OP_AND_REVERSE      12
#define VIS_LOGIC_OP_AND_INVERTED     13
#define VIS_LOGIC_OP_OR_REVERSE       14
#define VIS_LOGIC_OP_OR_INVERTED      15

#define VIS_STAGE_IA (1<<0) // Input Assembler
#define VIS_STAGE_VS (1<<1) // Vertex Shader
#define VIS_STAGE_HS (1<<2) // Hull Shader
#define VIS_STAGE_DS (1<<3) // Domain Shader
#define VIS_STAGE_GS (1<<4) // Geometry Shader
#define VIS_STAGE_PS (1<<5) // Pixel Shader
#define VIS_STAGE_SO (1<<6) // Stream Out

#define VIS_REG_TEXTURE (1<<0)      // SRV
#define VIS_REG_MULTISHARED (1<<1)  // UAV
#define VIS_REG_CBUFFER (1<<2)      // CBV
#define VIS_REG_SAMPLER (1<<3)      // Sampler

#define VIS_FILL_WIREFRAME 0
#define VIS_FILL_SOLID 1

#define VIS_CULL_NONE 0
#define VIS_CULL_FRONT 1
#define VIS_CULL_BACK 2

#define VIS_PRIM_UNDEFINED  0
#define VIS_PRIM_POINT      1
#define VIS_PRIM_LINE       2
#define VIS_PRIM_TRIANGLE   3
#define VIS_PRIM_PATCH      4
#define VIS_PRIMTOPO_POINT_LIST 10
#define VIS_PRIMTOPO_TRIANGLE_LIST 30
#define VIS_PRIMTOPO_TRIANGLE_STRIP 31
#define VIS_PRIMTOPO_LINE_LIST 20
#define VIS_PRIMTOPO_LINE_STRIP 21

#define VIS_COMPFUNC_NEVER 1
#define VIS_COMPFUNC_LESS 2
#define VIS_COMPFUNC_EQUAL 3
#define VIS_COMPFUNC_LESS_EQUAL 4
#define VIS_COMPFUNC_GREATER 5
#define VIS_COMPFUNC_NOT_EQUAL 6
#define VIS_COMPFUNC_GREATER_EQUAL 7
#define VIS_COMPFUNC_ALWAYS 8

#define VIS_STENCIL_OP_KEEP 1
#define VIS_STENCIL_OP_ZERO 2
#define VIS_STENCIL_OP_REPLACE 3
#define VIS_STENCIL_OP_INCR_SAT 4
#define VIS_STENCIL_OP_DECR_SAT 5
#define VIS_STENCIL_OP_INVERT 6
#define VIS_STENCIL_OP_INCR 7
#define VIS_STENCIL_OP_DECR 8

#define VIS_MSAA_QUALITY_STANDARD 0xffffffff
#define VIS_MSAA_QUALITY_CENTER 0xfffffffe

#define VIS_CLS_SHADER_LAYOUT 0
#define VIS_CLS_VIEWPORTS 1
#define VIS_CLS_SCISSORS 2
#define VIS_CLS_RENDER_TARGETS 3
#define VIS_CLS_PRIM_TOPOLOGY 4
#define VIS_CLS_VERTEX_BUFFER 5
#define VIS_CLS_CLEAR_RT 6
#define VIS_CLS_DRAW 7
#define VIS_CLS_DRAW_INDEXED 8


#ifdef __cplusplus
extern "C" {
#endif

  typedef viscalar vis3[3];
  typedef viscalar vis4[4];
  typedef viscalar vis3x3[9];
  typedef viscalar vis4x4[16];

  struct vis;
  ////////////////////////////////////////////////
  typedef struct vis_rect
  {
    float x, y;
    float width, height;
  }vis_rect;

  typedef struct vis_viewport
  {
    vis_rect rect;
    float depth_min;
    float depth_max;
  }vis_viewport;

  ////////////////////////////////////////////////
  typedef struct vis_opts
  {
    int   width;
    int   height;
    char* title;
#if defined(VIS_DX11) || defined(VIS_DX12)
    int   use_warpdevice;
    HWND  hwnd;
    HINSTANCE hInstance;
#endif
  }vis_opts;

  ////////////////////////////////////////////////
  typedef struct vis_camera
  {
    vis4x4  proj;
    vis3x3  view;
    vis3    pos;
  }vis_camera;
  
  ////////////////////////////////////////////////
  typedef struct vis_id
  {
    uint16_t type;
    uint16_t subtype;
    uint32_t value;
  }vis_id;

  typedef vis_id vis_resource;
  typedef vis_id vis_staging;
  ////////////////////////////////////////////////
  typedef struct vis_input_element
  {
    const char* semantic;
    uint32_t sem_index;
    uint32_t format;
    uint32_t slot;   
    uint32_t offset;
  }vis_input_element;

  ////////////////////////////////////////////////
  typedef struct vis_shader_registers
  {
    uint32_t reg_type;    // VIS_REG*
    uint32_t base_reg;    
    uint32_t reg_count;
    uint32_t visibility;  // VIS_STAGE* flags
  }vis_shader_registers;

  typedef struct vis_shader_layout
  {
    vis_shader_registers* reg_desc;
    uint32_t reg_desc_count;
    uint32_t stage_flags;             // VIS_STAGE* flags
  }vis_shader_layout;

  ////////////////////////////////////////////////
  typedef struct vis_blend_target
  {
    uint8_t enabled;            // bool
    uint8_t blend_src;          // VIS_BLEND*
    uint8_t blend_dst;          // VIS_BLEND*
    uint8_t blend_op;           // VIS_BLEND_OP*
    uint8_t blendalpha_src;     // VIS_BLEND*
    uint8_t blendalpha_dst;     // VIS_BLEND*
    uint8_t blendalpha_op;      // VIS_BLEND_OP*
    uint8_t logic_op_enabled;   // bool
    uint8_t logic_op;           // VIS_LOGIC_OP
    uint8_t rt_writemask;       // 0-0xff
  }vis_blend_target;

  typedef struct vis_blend_state
  {
    uint8_t alpha_to_coverage;                // bool
    uint8_t independent_blend;                // bool  
    vis_blend_target  blend_rt[VIS_MAX_RT];
  }vis_blend_state;

  ////////////////////////////////////////////////
  typedef struct vis_raster_state
  {
    uint8_t fill_mode;            // VIS_FILL*
    uint8_t cull_mode;            // VIS_CULL*
    uint8_t is_front_ccw;         // bool
    int32_t depth_bias;           //
    float depth_bias_clamp;       //
    float slope_scaled_depth_bias;//
    uint8_t depth_clip_enabled;   // bool
    uint8_t msaa_enabled;         // bool
    uint8_t aaline_enabled;       // bool
    uint8_t force_sample_count;   // 0-12
    uint8_t conservative_enabled; // bool
  }vis_raster_state;

  ////////////////////////////////////////////////
  typedef struct vis_stencil_op
  {
    uint8_t op_fail;        // VIS_STENCIL_OP_*
    uint8_t op_depth_fail;  // VIS_STENCIL_OP_*
    uint8_t op_pass_op;     // VIS_STENCIL_OP_*
    uint8_t compfunc;       // VIS_COMPFUNC_*
  }vis_stencil_op;

  typedef struct vis_depth_stencil_state
  {
    uint8_t depth_enabled;    // bool
    uint8_t depth_write_all;  // bool
    uint8_t depth_compfunc;   // VIS_COMPFUNC_*

    uint8_t stencil_enabled;    // bool
    uint8_t stencil_read_mask;  // 0-0xff
    uint8_t stencil_write_mask; // 0-0xff
    vis_stencil_op op_front;
    vis_stencil_op op_back;
  }vis_depth_stencil_state;

  ////////////////////////////////////////////////
  typedef struct vis_stream_out_state
  {
    uint8_t enabled;
    uint32_t unused;
  }vis_stream_out_state;

  ////////////////////////////////////////////////
  typedef struct vis_pipeline_state
  {
    vis_resource shader_layout;    
    vis_resource vs;
    vis_resource ps;
    vis_resource ds;
    vis_resource hs;
    vis_resource gs;
    vis_input_element* vertex_layout;
    uint32_t vertex_layout_count;
    vis_stream_out_state stream_out_state;
    vis_blend_state blend_state;
    vis_raster_state raster_state;
    vis_depth_stencil_state depth_stencil_state;
    uint8_t depth_stencil_format;                 // VIS_FORMAT_*
    uint32_t blend_sample_mask;
    uint8_t primitive;                            // VIS_PRIM*
    uint32_t rt_count;                            
    uint8_t rt_formats[VIS_MAX_RT];               // VIS_FORMAT_*      
    uint8_t msaa_count;                           // multisamples per pixel
    uint32_t msaa_quality;                        // can be also VIS_MSAA_QUALITY_*
    uint32_t gpu_node;                            // 0 for single-gpu.
    uint8_t debug_enabled;                        // bool
  }vis_pipeline_state;

  ////////////////////////////////////////////////
  typedef struct vis_shader_bytecode
  {
    uint8_t* data;
    uint32_t size;
  }vis_sbc;

  ////////////////////////////////////////////////
  typedef struct vis_cmd_clear
  {
    vis_resource* rts;
    uint32_t num_rts;
    vis4 clear_color;
  }vis_cmd_clear;

  typedef struct vis_cmd_draw
  {
    uint32_t start_vertex;
    uint32_t num_verts;
    uint32_t start_instance;
    uint32_t num_instances;
  }vis_cmd_draw;
  
  ////////////////////////////////////////////////
  int vis_init(vis** vi, vis_opts* opts);
  int vis_begin_frame(vis* vi);
  void vis_render_frame(vis* vi);
  int vis_end_frame(vis* vi);
  void vis_release(vis** vi);
  
  ////////////////////////////////////////////////
  vis_resource vis_create_resource(vis* vi, uint16_t type, void* resData, uint32_t flags);  
  void vis_release_resource(vis* vi, vis_resource resource);

  ////////////////////////////////////////////////
  uint32_t vis_shader_compile(vis* vi, uint32_t loadSrc, void* srcData, uint32_t srcSize, vis_shader_bytecode* outByteCode );
  void vis_pipeline_state_set_default(vis* vi, vis_pipeline_state* pstate);

  ////////////////////////////////////////////////
  uint32_t vis_command_list_record(vis* vi, vis_resource cmd_list);
  uint32_t vis_command_list_close(vis* vi, vis_resource cmd_list);
  uint32_t vis_command_list_reset(vis* vi, vis_resource cmd_list);
  uint32_t vis_command_list_set(vis* vi, vis_resource cmd_list, uint32_t cls_state, void* data_state, int32_t flags);
  vis_staging vis_command_list_resource_update(vis* vi, vis_resource cmd_list, vis_resource res, void* inData, uint32_t inSize);
  uint32_t vis_command_list_release_update(vis* vi, vis_resource cmd_list, vis_staging stag);
  uint32_t vis_command_list_execute(vis* vi, vis_resource* cmd_lists, uint32_t count);

  ////////////////////////////////////////////////
  vis_id vis_sync_gpu_to_signal(vis* vi);
  void vis_sync_cpu_wait_for_signal(vis* vi, vis_id signal);
  void vis_sync_cpu_callback_when_signal(vis* vi);

  ////////////////////////////////////////////////
  void vis_rect_make(vis_rect* r, float x, float y, float w, float h);
  int vis_res_valid(vis_resource res);

  ////////////////////////////////////////////////
  uint32_t vis_get_back_buffer_count(vis* vi);
  void* vis_get_back_buffer(vis* vi, uint32_t index);
#ifdef __cplusplus
};
#endif

/* ************************************************************************************************ */
/* ************************************************************************************************ */
/* ************************************************************************************************ */
/* ************************************************************************************************ */

////////////////////////////////////////////////////////////////////////////////////////////////////
//                          Definitions to be used by specific _dx11.h _dx12.h _gl.h
////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(VIS_DX11) || defined(VIS_DX12)
int vwin_create_window(vis_opts* opts);
#endif

#ifdef VIS_IMPLEMENTATION
static vis_resource VIS_RES_INVALID = { 0xffff, 0xffff, 0xffffffff };
#endif

typedef struct vis_array
{
  void** data;
  uint32_t size;
  uint32_t capacity;
}vis_array;

int32_t vis_array_create(vis_array** arr, uint32_t capacity);
void vis_array_destroy(vis_array** arr);
void vis_array_push_back(vis_array* arr, void* elem);
void* vis_array_pop_back(vis_array* arr);
void* vis_array_at(vis_array* arr, uint32_t index);
void vis_array_clear(vis_array* arr);
void vis_array_grow_double(vis_array* arr);
void vis_array_ensure(vis_array* arr, uint32_t count_new_elements); // makes sure there's room for new elements


////////////////////////////////////////////////////////////////////////////////////////////////////
//                              PLATFORM SPECIFIC IMPLEMENTATION
////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(VIS_DX11)
#pragma warning(disable:4005)
# include "vis_dx11.h"
# pragma warning(default:4005)
#elif defined(VIS_GL)
# include "vis_gl.h"
#elif defined(VIS_DX12)
#include "vis_dx12.h"
#endif


#ifdef VIS_IMPLEMENTATION
////////////////////////////////////////////////////////////////////////////////////////////////////
//                                COMMON IMPLEMENTATION
////////////////////////////////////////////////////////////////////////////////////////////////////
int vis_init(vis** vi, vis_opts* opts)
{
  *vi = (vis*)vis_malloc(sizeof(vis));
  vis_mem_check(*vi);
  return vis_init_plat(*vi, opts);
}

void vis_release(vis** vi)
{
  vis_release_plat(*vi);
  vis_safefree(*vi);
}

void vis_rect_make(vis_rect* r, float x, float y, float w, float h)
{
  r->x = x; r->y = y;
  r->width = w; r->height = h;
}

int vis_res_valid(vis_resource res)
{
  const int r = (*(uint64_t*)&res == 0xffffffffffffffff) ? VIS_TRUE : VIS_FALSE;
  return r;
}

///////////////////////////////////////////////////////////////////
//                  WINDOWS - vwin_create_window
///////////////////////////////////////////////////////////////////
#if defined(VIS_DX11) || defined(VIS_DX12)
LRESULT CALLBACK vwin_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
int vwin_create_window(vis_opts* opts)
{
  // Initialize the window class.
  WNDCLASSEXA windowClass = { 0 };
  windowClass.cbSize = sizeof(WNDCLASSEX);
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = vwin_proc;
  windowClass.hInstance = opts->hInstance;
  windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
  windowClass.lpszClassName = "vis_window_class";
  RegisterClassEx(&windowClass);

  RECT windowRect = { 0, 0, static_cast<LONG>(opts->width), static_cast<LONG>(opts->height) };
  AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

  // Create the window and store a handle to it.
  opts->hwnd = CreateWindowExA(NULL,
    "vis_window_class",
    opts->title,
    WS_OVERLAPPEDWINDOW,
    opts->width,
    opts->height,
    windowRect.right - windowRect.left,
    windowRect.bottom - windowRect.top,
    NULL,		// We have no parent window, NULL.
    NULL,		// We aren't using menus, NULL.
    opts->hInstance,
    NULL);		// We aren't using multiple windows, NULL.

  ShowWindow(opts->hwnd, SW_SHOW);
  return VIS_OK;
}

///////////////////////////////////////////////////////////////////////
//                        WINDOWS - vwin_proc
///////////////////////////////////////////////////////////////////////
LRESULT CALLBACK vwin_proc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  // Handle destroy/shutdown messages.
  switch (message)
  {
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }

  // Handle any messages the switch statement didn't.
  return DefWindowProc(hWnd, message, wParam, lParam);
}
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
//                                        DYNAMIC ARRAY IMPLEMENTATION
////////////////////////////////////////////////////////////////////////////////////////////////////
int32_t vis_array_create(vis_array** arr, uint32_t capacity)
{
  *arr = (vis_array*)vis_calloc(1, sizeof(vis_array));
  if (!*arr) return VIS_FALSE;
  (*arr)->capacity = capacity>0 ? capacity : 256;
  (*arr)->size = 0;
  (*arr)->data = (void**)vis_malloc(sizeof(void*)*capacity);
  if (!(*arr)->data) { vis_array_destroy(arr); return VIS_FALSE; }
  return VIS_TRUE;
}

void vis_array_destroy(vis_array** arr)
{
  if (!arr || !*arr) return;
  vis_safefree((*arr)->data);
  vis_free(*arr);
  *arr = VIS_NULL;
}

void vis_array_grow_double(vis_array* arr)
{
  uint32_t newcap = arr->capacity * 2;
  arr->data = (void**)vis_realloc(arr->data, sizeof(void*)*newcap);
  if (!arr->data) { VIS_BREAK; return; }
  arr->capacity = newcap;
}

void vis_array_push_back(vis_array* arr, void* elem)
{
  if (arr->size >= arr->capacity)
    vis_array_grow_double(arr);
  arr->data[arr->size++] = elem;
}

void* vis_array_pop_back(vis_array* arr)
{
  if (arr->size == 0) return VIS_NULL;
  return arr->data[--arr->size];
}

void* vis_array_at(vis_array* arr, uint32_t index)
{
  return index >= 0 && index < arr->size ? arr->data[index] : VIS_NULL;
}

void vis_array_clear(vis_array* arr)
{
  arr->size = 0;
}

void vis_array_ensure(vis_array* arr, uint32_t cout_new_elements)
{
  const uint32_t newcap = arr->size + cout_new_elements;
  if (newcap <= arr->capacity) return;
  arr->capacity = newcap;
  vis_array_grow_double(arr); // ensure makes use directly of grow_double policy
}

#endif // VIS_IMPLEMENTATION

#endif // _VIS_H_