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

#ifdef __cplusplus
extern "C" {
#endif

  typedef unsigned char  visu8;
  typedef unsigned short visu16;
  typedef unsigned int   visu32;
  typedef unsigned long long visu64;
  typedef char  visi8;
  typedef short visi16;
  typedef int   visi32;
  typedef long long visi64;
  typedef viscalar vis3[3];
  typedef viscalar vis4[4];
  typedef viscalar vis3x3[9];
  typedef viscalar vis4x4[16];

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  //                                COMMON (HEADER)
  ////////////////////////////////////////////////////////////////////////////////////////////////////
  typedef struct vis;
  typedef struct vis_opts
  {
    int   width;
    int   height;
    char* title;
#if (defined(VIS_DX11) || defined(VIS_DX12))
    int   use_warpdevice;
    int   debug_layer;
    HWND  hwnd;
    HINSTANCE hInstance;
#endif
  }vis_opts;

  typedef struct vis_camera
  {
    vis4x4  proj;
    vis3x3  view;
    vis3    pos;
  }vis_camera;

  int vis_init(vis** vi, vis_opts* opts);
  int vis_begin_frame(vis* vi);
  void vis_render_frame(vis* vi);
  int vis_end_frame(vis* vi);
  void vis_release(vis** vi);

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
//                                IMPLEMENTATION - COMMON
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

int vis_begin_frame(vis* vi)
{
  return vis_begin_frame_plat(vi);
}

void vis_render_frame(vis* vi)
{
  return vis_render_frame_plat(vi);
}

int vis_end_frame(vis* vi)
{
  return vis_end_frame_plat(vi);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(VIS_DX11) || defined(VIS_DX12)
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);
int vwin_create_window(vis_opts* opts)
{
  // Initialize the window class.
  WNDCLASSEXA windowClass = { 0 };
  windowClass.cbSize = sizeof(WNDCLASSEX);
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = WindowProc;
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

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
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