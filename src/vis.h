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

#if defined(VIS_DX11)
#pragma warning(disable:4005)
# include <d3d11.h>
# include <d3dx11.h>
# include <d3dcompiler.h>
# include <xnamath.h>
# include <xinput.h>
# pragma warning(default:4005)
#elif defined(VIS_GL)
# include <glfw3.h>
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
#define VIS_ERR_INIT (1)

#ifdef __cplusplus
extern "C" {
#endif
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
//                                  DIRECTX 11 (HEADER)
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(VIS_DX11)

  typedef struct vis
  {
    ID3D11Device* d3d11device;
  }vis;

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
//                                  OPENGL (HEADER)
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
#elif defined(VIS_GL)

  typedef struct vis
  {
    GLFWwindow* window;
    const GLFWvidmode* mode;
  }vis;


#endif // VIS_DX11 else VIS_GL

////////////////////////////////////////////////////////////////////////////////////////////////////
//                                COMMON (HEADER)
////////////////////////////////////////////////////////////////////////////////////////////////////
  typedef struct vis_opts
  {
    int   width;
    int   height;
    char* title;
  }vis_opts;

  int vis_init(vis** vi, vis_opts* opts);
  int vis_begin_frame(vis* vi);
  void vis_frame(vis* vi);
  int vis_end_frame(vis* vi);
  void vis_release(vis** vi);

/* ************************************************************************************************ */
/* ************************************************************************************************ */
/* ************************************************************************************************ */
/* ************************************************************************************************ */
  #ifdef __cplusplus
};
#endif

#ifdef VIS_IMPLEMENTATION

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
//                                  IMPLEMENTATION - DX11
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
#if defined(VIS_DX11)

int vis_init_platform(vis* vi, vis_opts* opts)
{
  return VIS_FAIL;
}

void vis_release_platform(vis* vi)
{
  vis_unused(vi);
}

////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
//                                  IMPLEMENTATION - OPENGL
////////////////////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////////
#elif defined(VIS_GL)
void vis_gl_frame(GLFWwindow* window, int w, int h);

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
int vis_init_platform(vis* vi, vis_opts* opts)
{
  GLint i=1;

  if (!glfwInit())
    return VIS_ERR_INIT;

  vi->mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
  vi->window = glfwCreateWindow(opts->width, opts->height, opts->title, NULL, NULL);
  if (!vi->window)
  {
    glfwTerminate();
    return VIS_ERR_INIT;
  }
  glfwSetFramebufferSizeCallback(vi->window, vis_gl_frame);
  glfwMakeContextCurrent(vi->window);
  glfwSwapInterval(1);

  glEnable(GL_POINT_SMOOTH);
  glEnable(GL_LINE_SMOOTH);  
  glClearColor (0.0, 0.0, 0.0, 0.0);
  glShadeModel (GL_SMOOTH);

  glLoadIdentity();
//   glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
//   glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
//   glLightfv(GL_LIGHT0, GL_POSITION, light_position);
  glLightModeliv(GL_LIGHT_MODEL_TWO_SIDE, &i ); // two-sided lighting

  glEnable(GL_LIGHTING);
  glEnable(GL_LIGHT0);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_NORMALIZE);

  return VIS_OK;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void vis_release_platform(vis* vi)
{
  glfwDestroyWindow(vi->window);
  glfwTerminate();
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void vis_gl_frame(GLFWwindow* window, int w, int h)
{
  int width = 0, height = 0;

  vis_unused(w); vis_unused(h);
  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  glfwGetFramebufferSize(window, &width, &height);
  glViewport(0, 0, width, height);
  glClearColor(20.0f/255.0f, 20.0f/255.0f, 90.0f/255.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_TEXTURE_2D);
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-10, 10, -10, 10, -1, 1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  //glDisable(GL_DEPTH_TEST);
  glColor4ub(240,240,240,255);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);  
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
int vis_begin_frame_platform(vis* vi)
{
  if ( glfwWindowShouldClose(vi->window) )
    return VIS_FAIL;

  return VIS_OK;
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
void vis_frame_platform(vis* vi)
{
  vis_gl_frame(vi->window,0,0);
}

///////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////
int vis_end_frame_platform(vis* vi)
{
  glfwSwapBuffers(vi->window);
  glfwPollEvents();
  return VIS_OK;
}

#endif // VIS_GL

////////////////////////////////////////////////////////////////////////////////////////////////////
//                                IMPLEMENTATION - COMMON
////////////////////////////////////////////////////////////////////////////////////////////////////
int vis_init(vis** vi, vis_opts* opts)
{
  *vi = (vis*)vis_malloc(sizeof(vis));
  vis_mem_check(*vi);
  return vis_init_platform(*vi,opts);
}

void vis_release(vis** vi)
{
  vis_release_platform(*vi);
  vis_safefree(*vi);
}

int vis_begin_frame(vis* vi)
{
  return vis_begin_frame_platform(vi);
}

void vis_frame(vis* vi)
{
  return vis_frame_platform(vi);
}

int vis_end_frame(vis* vi)
{
  return vis_end_frame_platform(vi);
}

#endif // VIS_IMPLEMENTATION

#endif // _VIS_H_