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

*/

#ifndef _CIGI_H_
#define _CIGI_H_

/////////////////////

#ifdef _MSC_VER
#pragma warning(disable:4244 4100 4996 4706 4091 4310)
#endif

// Error codes
#define CIGI_OK 0


#ifdef __cplusplus  // PUBLIC API
extern "C" {
#endif

#if defined(CIGI_VERSION_3_3) // *********************************************************************************************************************

  typedef struct cigi_opts
  {

  };

  typedef struct cigi // it's a cigi session
  {
    // entities, articulated parts
    // database, terrain, geodetic coordinates, 
    // atmospherics, weather conditions
    // view, view group, inset views, multichannel, warping, blending, sensors    
    // symbols or HUD
    // packet management (packets, handlers, custom packets)
    // requests -> responses
    // communication layer/wrapper
    // synchronization and persistence
  }cigi;
  
#elif defined(CIGI_VERSION_4_0) // *******************************************************************************************************************
  typedef struct cigi
  {

  }cigi;

#endif

  //////////////////////////////////////////////////////////////////////////
  //        Plublic COMMON API
  //////////////////////////////////////////////////////////////////////////
  int cigi_create(cigi* ci, cigi_opts* opts);
  int cigi_release(cigi* ci);

#ifdef __cplusplus
};
#endif

#ifdef CIGI_IMPLEMENTATION
#if defined(__x86_64__) || defined(_M_X64)  ||  defined(__aarch64__)   || defined(__64BIT__) || \
  defined(__mips64)     || defined(__powerpc64__) || defined(__ppc64__)
#	define CIGI_PLATFORM_64
# define CIGI_ALIGNMENT 16
#else
#	define CIGI_PLATFORM_32
# define CIGI_ALIGNMENT 8
#endif //

#if defined(_DEBUG) || defined(DEBUG)
#ifdef _MSC_VER
#define CIGI_BREAK { __debugbreak(); }
#else
#define CIGI_BREAK { raise(SIGTRAP); }
#endif

#define CIGI_ASSERT(c) { if (!(c)){ CIGI_BREAK; } }
#else
#define CIGI_BREAK {(void*)0;}
#define CIGI_ASSERT(c)
#endif

#ifdef _MSC_VER
#define CIGI_BREAK_ALWAYS { __debugbreak(); }
#else
#define CIGI_BREAK_ALWAYS { raise(SIGTRAP); }
#endif


// Memory functions override
#if defined(cigi_malloc) && defined(cigi_free) && defined(cigi_calloc) && defined(cigi_realloc)
// ok, all defined
#elif !defined(cigi_malloc) && !defined(cigi_free) && !defined(cigi_calloc) && !defined(cigi_realloc)
// ok, none defined
#else
#error "Must define all or none of cigi_malloc, cigi_free, cigi_calloc, cigi_realloc"
#endif

#ifdef CIGI_ALIGNED
#   ifndef cigi_malloc
#     define cigi_malloc(sz) _aligned_malloc(sz,CIGI_ALIGNMENT)
#   endif
#   ifndef cigi_free
#     define cigi_free(p) _aligned_free(p)
#   endif
#   ifndef cigi_calloc
#     define cigi_calloc(count,size) cigi_aligned_calloc(count,size,  CIGI_ALIGNMENT)
#   endif
#   ifndef cigi_realloc
#     define cigi_realloc(p,sz) _aligned_realloc(p,sz,CIGI_ALIGNMENT)
#   endif
#else
#   ifndef cigi_malloc
#     define cigi_malloc(sz) malloc(sz)
#   endif
#   ifndef cigi_free
#     define cigi_free(p) free(p)
#   endif
#   ifndef cigi_calloc
#     define cigi_calloc(count,size) calloc(count,size)
#   endif
#   ifndef cigi_realloc
#     define cigi_realloc(p,sz) realloc(p,sz)
#   endif
#endif
#define cigi_strdup(s) cigi_strdup_internal(s)
#define cigi_safefree(p) { if (p){ cigi_free(p); (p)=0;} }

#define cigi_min(a,b) ((a)<(b)?(a):(b))
#define cigi_max(a,b) ((a)>(b)?(a):(b))
#define cigi_offsetto(n,t)  ( (uint8_t*)&((t*)(0))->n - (uint8_t*)(0) )
#define CIGI_NULL (0)
#define CIGI_TRUE (1)
#define CIGI_FALSE (0)
#define CIGIMAKE16(hi8,lo8)    ( ((uint16_t)(hi8)<<8)   | (uint16_t)(lo8) )
#define CIGIMAKE32(hi16,lo16)  ( ((uint32_t)(hi16)<<16) | (uint32_t)(lo16) )
#define CIGIMAKE64(hi32,lo32)  ( ((uint64_t)(hi32)<<32) | (uint64_t)(lo32) )
#define CIGIGETLO16(u32)       ( (uint16_t)((u32)&0xffff) )
#define CIGIGETHI16(u32)       ( (uint16_t)((u32)>>16) )
#define CIGIGETLO32(u64)       ( (uint32_t)((u64)&0xffffffff) )
#define CIGIGETHI32(u64)       ( (uint32_t)((u64)>>32) )

#define CIGIGET16(u32,hi16,lo16) { (lo16)=CIGIGETLO16(u32); (hi16)=CIGIGETHI16(u32); }
#define CIGIGET32(u64,hi32,lo32) { (lo32)=CIGIGETLO32(u64); (hi32)=CIGIGETHI32(u64); }


#if defined(WIN32) || defined(_WIN32) || (defined(sgi) && defined(unix) && defined(_MIPSEL)) || (defined(sun) && defined(unix) && !defined(_BIG_ENDIAN)) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN)) || (defined(__APPLE__) && defined(__LITTLE_ENDIAN__)) || (defined( _PowerMAXOS ) && (BYTE_ORDER == LITTLE_ENDIAN ))
#define CIGI_LITTLE_ENDIAN
#elif (defined(sgi) && defined(unix) && defined(_MIPSEB)) || (defined(sun) && defined(unix) && defined(_BIG_ENDIAN)) || defined(vxw) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN)) || ( defined(__APPLE__) && defined(__BIG_ENDIAN__)) || (defined( _PowerMAXOS ) && (BYTE_ORDER == BIG_ENDIAN) )
#define CIGI_BIG_ENDIAN
#else
#  error unknown endian type
#endif

#ifdef CIGI_LITTLE_ENDIAN
void cigi_swap16(void* d)
{ 
  unsigned char* b=(unsigned char*)d;
  const unsigned char t = b[0];
  b[0]=b[1]; b[1]=t;
}

void cigi_swap32(void* d)
{ 
  unsigned char* b=(unsigned char*)d;
  const unsigned char t0 = b[0], t1 = b[1];
  b[0]=b[3]; b[1]=b[2];
  b[3]=t0;   b[2]=t1;
}

void cigi_swap64(void* d)
{
  unsigned char* b=(unsigned char*)d;
  const unsigned char t0=b[0], t1=b[1], t2=b[2], t3=b[3];
  b[0]=b[7]; b[1]=b[6]; b[2]=b[5]; b[3]=b[4];
  b[4]=t3;   b[5]=t2;   b[6]=t1;   b[7]=t0;   
}
#else
# define cigi_swap16(d)
# define cigi_swap32(d)
# define cigi_swap64(d)
#endif

#if defined(CIGI_VERSION_3_3) // *********************************************************************************************************************

typedef struct cigi_packet_base
{

}cigi_packet_base;

#elif defined(CIGI_VERSION_4_0) // *******************************************************************************************************************


#endif



#endif // CIGI_IMPLEMENTATION

#endif // _CIGI_H_