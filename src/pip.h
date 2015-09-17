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
- By default expands UDP/TCP both code, unless you specify PIP_NOUDP or PIP_NOTCP
- Define PIP_IMPLEMENTATION to expand the implementation code
- By default it uses IPv4 but you can define PIP_IPV6
- 
- http://tangentsoft.net/wskfaq/articles/bsd-compatibility.html
*/

#ifndef _PIP_H
#define _PIP_H

#ifdef _MSC_VER
#pragma warning(disable:4244 4100 4996 4706 4091 4310)
#endif

#if defined(PIP_IPV6)
#ifdef PIP_IPV4
#undef PIP_IPV4
#endif
#elif !defined(PIP_IPV4)
#define PIP_IPV4
#endif

#ifdef __cplusplus  // PUBLIC API
extern "C" {
#endif

#define PIP_OPT_BLOCKING 1
#define PIP_OPT_TIMEOUT 2
#define PIP_OPT_REUSEADDR 3
#define PIP_OPT_BROADCAST 4

  // Common API
  typedef struct pip_addr
  {

  }pip_addr;

  int32_t pip_make_addr( pip_addr* out_addr );

  // ONLY UDP API -----------------------------
#ifndef PIP_NOUDP
  typedef struct pip_udp
  {

  }pip_udp;

  int32_t pip_udp_create(pip_udp* udp);
  int32_t pip_udp_release(pip_udp* udp);
  int32_t pip_udp_bind(pip_udp* udp, pip_addr* addr);
  int32_t pip_udp_set_opt(pip_udp* udp, uint32_t opt, void* val);
  int32_t pip_udp_send_to(pip_udp* udp, const void* in_data, uint32_t size, pip_addr* addr);
  int32_t pip_udp_recv_from(pip_udp* udp, void* out_data, uint32_t max_size, pip_addr* addr);

  int32_t pip_udp_multicast_join(pip_udp* udp, pip_addr* addr);
  int32_t pip_udp_multicast_leave(pip_udp* udp, pip_addr* addr);
#endif

    // ONLY TCP API -----------------------------
#ifndef PIP_NOTCP
  typedef struct pip_tcp
  {

  }pip_tcp;

  typedef struct pip_tcp_opts
  {

  }pip_tcp_opts;

  int pip_tcp_create(pip_tcp* tcp, pip_tcp_opts* opts);
  int pip_tcp_release(pip_tcp* tcp);
#endif

#ifdef __cplusplus
};
#endif

#ifdef PIP_IMPLEMENTATION
#if defined(__x86_64__) || defined(_M_X64)  ||  defined(__aarch64__)   || defined(__64BIT__) || \
  defined(__mips64)     || defined(__powerpc64__) || defined(__ppc64__)
#	define IP_PLATFORM_64
# define IP_ALIGNMENT 16
#else
#	define IP_PLATFORM_32
# define IP_ALIGNMENT 8
#endif //

#if defined(_DEBUG) || defined(DEBUG)
#ifdef _MSC_VER
#define IP_BREAK { __debugbreak(); }
#else
#define IP_BREAK { raise(SIGTRAP); }
#endif

#define IP_ASSERT(c) { if (!(c)){ IP_BREAK; } }
#else
#define IP_BREAK {(void*)0;}
#define IP_ASSERT(c)
#endif

#ifdef _MSC_VER
#define IP_BREAK_ALWAYS { __debugbreak(); }
#else
#define IP_BREAK_ALWAYS { raise(SIGTRAP); }
#endif


// Memory functions override
#if defined(ip_malloc) && defined(ip_free) && defined(ip_calloc) && defined(ip_realloc)
// ok, all defined
#elif !defined(ip_malloc) && !defined(ip_free) && !defined(ip_calloc) && !defined(ip_realloc)
// ok, none defined
#else
#error "Must define all or none of ip_malloc, ip_free, ip_calloc, ip_realloc"
#endif

#ifdef IP_ALIGNED
#   ifndef ip_malloc
#     define ip_malloc(sz) _aligned_malloc(sz,IP_ALIGNMENT)
#   endif
#   ifndef ip_free
#     define ip_free(p) _aligned_free(p)
#   endif
#   ifndef ip_calloc
#     define ip_calloc(count,size) ip_aligned_calloc(count,size,  IP_ALIGNMENT)
#   endif
#   ifndef ip_realloc
#     define ip_realloc(p,sz) _aligned_realloc(p,sz,IP_ALIGNMENT)
#   endif
#else
#   ifndef ip_malloc
#     define ip_malloc(sz) malloc(sz)
#   endif
#   ifndef ip_free
#     define ip_free(p) free(p)
#   endif
#   ifndef ip_calloc
#     define ip_calloc(count,size) calloc(count,size)
#   endif
#   ifndef ip_realloc
#     define ip_realloc(p,sz) realloc(p,sz)
#   endif
#endif
#define ip_strdup(s) ip_strdup_internal(s)
#define ip_safefree(p) { if (p){ ip_free(p); (p)=0;} }

#define ip_min(a,b) ((a)<(b)?(a):(b))
#define ip_max(a,b) ((a)>(b)?(a):(b))
#define ip_offsetto(n,t)  ( (uint8_t*)&((t*)(0))->n - (uint8_t*)(0) )
#define IP_NULL (0)
#define IP_TRUE (1)
#define IP_FALSE (0)
#define IP_MAKE16(hi8,lo8)    ( ((uint16_t)(hi8)<<8)   | (uint16_t)(lo8) )
#define IP_MAKE32(hi16,lo16)  ( ((uint32_t)(hi16)<<16) | (uint32_t)(lo16) )
#define IP_MAKE64(hi32,lo32)  ( ((uint64_t)(hi32)<<32) | (uint64_t)(lo32) )
#define IP_GETLO16(u32)       ( (uint16_t)((u32)&0xffff) )
#define IP_GETHI16(u32)       ( (uint16_t)((u32)>>16) )
#define IP_GETLO32(u64)       ( (uint32_t)((u64)&0xffffffff) )
#define IP_GETHI32(u64)       ( (uint32_t)((u64)>>32) )
#define IP_GET16(u32,hi16,lo16) { (lo16)=IP_GETLO16(u32); (hi16)=IP_GETHI16(u32); }
#define IP_GET32(u64,hi32,lo32) { (lo32)=IP_GETLO32(u64); (hi32)=IP_GETHI32(u64); }


#if defined(WIN32) || defined(_WIN32) || (defined(sgi) && defined(unix) && defined(_MIPSEL)) || (defined(sun) && defined(unix) && !defined(_BIG_ENDIAN)) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN)) || (defined(__APPLE__) && defined(__LITTLE_ENDIAN__)) || (defined( _PowerMAXOS ) && (BYTE_ORDER == LITTLE_ENDIAN ))
#define IP_LITTLE_ENDIAN
#elif (defined(sgi) && defined(unix) && defined(_MIPSEB)) || (defined(sun) && defined(unix) && defined(_BIG_ENDIAN)) || defined(vxw) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN)) || ( defined(__APPLE__) && defined(__BIG_ENDIAN__)) || (defined( _PowerMAXOS ) && (BYTE_ORDER == BIG_ENDIAN) )
#define IP_BIG_ENDIAN
#else
#  error unknown endian type
#endif

#ifdef IP_LITTLE_ENDIAN
void ip_swap16(void* d)
{ 
  unsigned char* b=(unsigned char*)d;
  const unsigned char t = b[0];
  b[0]=b[1]; b[1]=t;
}

void ip_swap32(void* d)
{ 
  unsigned char* b=(unsigned char*)d;
  const unsigned char t0 = b[0], t1 = b[1];
  b[0]=b[3]; b[1]=b[2];
  b[3]=t0;   b[2]=t1;
}

void ip_swap64(void* d)
{
  unsigned char* b=(unsigned char*)d;
  const unsigned char t0=b[0], t1=b[1], t2=b[2], t3=b[3];
  b[0]=b[7]; b[1]=b[6]; b[2]=b[5]; b[3]=b[4];
  b[4]=t3;   b[5]=t2;   b[6]=t1;   b[7]=t0;   
}
#else
# define ip_swap16(d)
# define ip_swap32(d)
# define ip_swap64(d)
#endif

#endif // PIP_IMPLEMENTATION

#endif // _P_IP_H