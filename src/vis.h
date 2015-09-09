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
# define vis_break { __debugbreak(); }
# else
# define vis_break { raise(SIGTRAP); }
# endif
#else
# define vis_break
#endif

#define vis_unused(p) (p)=(p)
#define vis_safefree(p) { if (p){ vis_free(p); (p)=0; } }
#define vis_saferelease(p){ if (p){ p->Release(); (p)=0;} }
#ifdef VIS_NO_MEMOUT_CHECK
#define vis_mem_check(p)
#else
#define vis_mem_check(p) { if ( !(p) ) { vis_break; } }
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
//////////////////////////// RESOURCE TYPES //////////////////////////////////
//////////////////////////////////////////////////////////////////////////////
#define VIS_NOFLAGS 0
#define VIS_LOAD_SOURCE_MEMORY 0
#define VIS_LOAD_SOURCE_FILE 1

#define VIS_TYPE_VERTEXBUFFER 1
#define VIS_TYPE_INDEXBUFFER 2
#define VIS_TYPE_SHADER 3
#define VIS_TYPE_PIPELINE 4
#define VIS_TYPE_INPUT_LAYOUT 5
#define VIS_TYPE_GPU_LAYOUT 6
#define VIS_TYPE_COMMAND_LIST 7
#define VIS_TYPE_FENCE 8
#define VIS_TYPE_STAGING_RES 9

#ifdef __cplusplus
extern "C" {
#endif

  typedef viscalar vis3[3];
  typedef viscalar vis4[4];
  typedef viscalar vis3x3[9];
  typedef viscalar vis4x4[16];

  struct vis;
  ////////////////////////////////////////////////
  typedef struct vis_opts
  {
    int   width;
    int   height;
    char* title;
#if (defined(VIS_DX11) || defined(VIS_DX12))
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
  typedef struct vis_pipeline_desc
  {

  }vis_pipeline_desc;

  ////////////////////////////////////////////////
  typedef struct vis_id
  {
    uint16_t type;
    uint16_t subtype;
    uint32_t value;
  };

  typedef struct vis_sbc;

  typedef vis_id vis_resource;  
  typedef vis_id vis_staging;

  ////////////////////////////////////////////////
  int vis_init(vis** vi, vis_opts* opts);
  int vis_begin_frame(vis* vi);
  void vis_render_frame(vis* vi);
  int vis_end_frame(vis* vi);
  void vis_release(vis** vi);
  
  ////////////////////////////////////////////////
  vis_resource vis_create_resource(vis* vi, uint16_t type, void* resData, uint32_t flags);  

  ////////////////////////////////////////////////
  uint32_t vis_shader_compile(vis* vi, uint32_t loadSrc, void* srcData, uint32_t srcSize, vis_sbc** outByteCode );
  void vis_release_sbc(vis* vi, vis_sbc** byteCode);

  ////////////////////////////////////////////////
  uint32_t vis_command_list_record(vis* vi, vis_command_list cl);
  vis_staging vis_command_list_resource_update(vis* vi, vis_command_list cl, vis_resource res, void* inData, uint32_t inSize);
  uint32_t vis_command_list_release_update(vis* vi, vis_staging stag);
  uint32_t vis_command_list_close(vis* vi, vis_command_list cl);
  uint32_t vis_command_list_execute(vis* vi, vis_command_list* lists, uint32_t count);

  ////////////////////////////////////////////////
  void vis_sync_gpu_to_signal();
  void vis_sync_cpu_wait_for_signal();
  void vis_sync_cpu_callback_when_signal();

  ////////////////////////////////////////////////
  void vis_release_descriptor();
  void vis_release_resource();
  void vis_release_pipeline();
  void vis_release_command_list();


  /* ************************************************************************************************ */
  /* ************************************************************************************************ */
  /* ************************************************************************************************ */
  /* ************************************************************************************************ */
#ifdef __cplusplus
};
#endif

#if defined(VIS_DX11) || defined(VIS_DX12)
int vwin_create_window(vis_opts* opts);
#endif

#if defined(VIS_DX11)
#pragma warning(disable:4005)
# include "vis_dx11.h"
# pragma warning(default:4005)
#elif defined(VIS_GL)
# include <glfw3.h>
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

///////////////////////////////////////////////////////////////////
// WINDOWS - vwin_create_window
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
// WINDOWS - vwin_proc
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


#endif // VIS_IMPLEMENTATION

#endif // _VIS_H_