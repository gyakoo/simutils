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

#ifndef _SGI_RGB_H_
#define _SGI_RGB_H_

/////////////////////
  
/* 
TODO
  - Add support for BPC==2
  - Add correct support for 1 channel (B/W)
*/

#ifdef __cplusplus
extern "C" {
#endif

  // Loads a SGI RGB format from filename
  // returns the raw data in BGRA format (1 byte per channel, 4 channels per pixel).
  // returns in x/y the original image dimension and the original bytes per channel in 'comp'
  // returns NULL when image could not be loaded, and in 'x' the reason code.
  unsigned char* sgirgb_load(const char* filename, int* x, int* y, int* comp);

#ifdef __cplusplus
};
#endif

#ifdef SGIRGB_IMPLEMENTATION
#define SGIRGB_MAGIC (474)
#define SGIRGB_ST_VERBATIM 0
#define SGIRGB_ST_RLE 1
#define SGIRGB_CMAP_NORMAL 0
#define SGIRGB_CMAP_DITHERED 1
#define SGIRGB_CMAP_SCREEN 2
#define SGIRGB_CMAP_COLORMAP 3

#define SGIRGB_ERR_WRONG_FILE 1
#define SGIRGB_ERR_ID 2
#define SGIRGB_ERR_BPC_NOT_SUPPORTED 3
#define SGIRGB_ERR_WRONG_STORAGE 4
#define SGIRGB_ERR_OUT_MEM 5
#define SGIRGB_ERR_WRONG_CHANNELS 6
#define SGIRGB_ERR_WRONG_COLORMAP 7

#if defined(sgirgb_malloc) && defined(sgirgb_free)
// ok
#elif !defined(sgirgb_malloc) && !defined(sgirgb_free)
// ok
#else
#error "Must define all or none of sgirgb_malloc, sgirgb_free"
#endif

#ifndef sgirgb_malloc
#define sgirgb_malloc(sz) malloc(sz)
#endif

#ifndef sgirgb_free
#define sgirgb_free(p) free(p)
#endif

#if defined(WIN32) || defined(_WIN32) || (defined(sgi) && defined(unix) && defined(_MIPSEL)) || (defined(sun) && defined(unix) && !defined(_BIG_ENDIAN)) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN)) || (defined(__APPLE__) && defined(__LITTLE_ENDIAN__)) || (defined( _PowerMAXOS ) && (BYTE_ORDER == LITTLE_ENDIAN ))
void sgirgb_swap16(void* d)
{ 
  unsigned char* b=(unsigned char*)d;
  const unsigned char t = b[0];
  b[0]=b[1]; b[1]=t;
}
void sgirgb_swap32(void* d)
{ 
  unsigned char* b=(unsigned char*)d;
  const unsigned char t0 = b[0], t1 = b[1];
  b[0]=b[3]; b[1]=b[2];
  b[3]=t0; b[2]=t1;
}
#elif (defined(sgi) && defined(unix) && defined(_MIPSEB)) || (defined(sun) && defined(unix) && defined(_BIG_ENDIAN)) || defined(vxw) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN)) || ( defined(__APPLE__) && defined(__BIG_ENDIAN__)) || (defined( _PowerMAXOS ) && (BYTE_ORDER == BIG_ENDIAN) )
#define sgirgb_swap16(d)
#define sgirgb_swap32(d)
#else
#  error unknown endian type
#endif

typedef struct sgiRGBHeader
{
  unsigned short magic;     // IRIS image file magic number
  unsigned char storage;    // Storage format
  unsigned char	bpc;        // Number of bytes per pixel channel
  unsigned short dimension; // Number of dimensions
  unsigned short xsize;     // X size in pixels
  unsigned short ysize;     // Y size in pixels
  unsigned short channels;  // number of channels (planes)
  int pixmin;			          // Minimum pixel value
  int pixmax;			          // Maximum pixel value
  char dummy[4];		        // reserved
  char imagename[80];	      // Image name
  int colormap;		          // Colormap ID
  char dummy2[404];	        // reserved
}sgiRGBHeader;


unsigned char* sgi_close_and_return(FILE* f, unsigned char* d)
{ 
  if ( f ) fclose(f); 
  if ( d ) sgirgb_free(d);
  return 0; 
}

unsigned int sgi_compute_filesize_to_end(FILE* f, char restore)
{
  unsigned int s;
  const int cur=ftell(f);
  fseek(f,0,SEEK_END);
  s=ftell(f)-cur;
  if ( restore )
    fseek(f,cur,SEEK_SET);
  return s;
}

unsigned char* sgirgb_load(const char* filename, int* x, int* y, int* comp)
{
  sgiRGBHeader header;
  FILE* f;
  unsigned int numpixels,row,j,k;
  size_t offset;
  unsigned char* outData=0;
  unsigned char* channels=0, *rch, *gch, *bch, *ach;
  unsigned int *tabstart=0, tabentries, rlebuffsize;   // rle
  unsigned char *rlebuff, *iptr, *optr;                               // rle
  unsigned char pixel;                                                // rle
  
  if (x) *x=SGIRGB_ERR_WRONG_FILE;
  if ( !filename ) return NULL;
  f= fopen(filename, "rb");
  if ( !f ) return NULL;
  
  // parse header
  if ( fread(&header, 1, sizeof(sgiRGBHeader),f) != sizeof(sgiRGBHeader) )
    return sgi_close_and_return(f,outData);
  sgirgb_swap16(&header.magic);
  sgirgb_swap16(&header.dimension);
  sgirgb_swap16(&header.xsize);
  sgirgb_swap16(&header.ysize);
  sgirgb_swap16(&header.channels);
  sgirgb_swap32(&header.pixmax);
  sgirgb_swap32(&header.pixmin);
  sgirgb_swap32(&header.colormap);

  // checking validity
  k=0;
  if (header.magic!=SGIRGB_MAGIC) k=SGIRGB_ERR_ID;
  else if (header.bpc!=1 )        k=SGIRGB_ERR_BPC_NOT_SUPPORTED;
  else if (header.channels>4)     k=SGIRGB_ERR_WRONG_CHANNELS;
  else if (header.storage!=SGIRGB_ST_VERBATIM && header.storage!=SGIRGB_ST_RLE) k=SGIRGB_ERR_WRONG_STORAGE;
  else if (header.colormap!=SGIRGB_CMAP_NORMAL) k=SGIRGB_ERR_WRONG_COLORMAP;
  if ( k )
  {
    if (x) *x=(int)k;
    return sgi_close_and_return(f,outData);
  }
  
  // allocate memory for result image
  numpixels=header.xsize*header.ysize;  
  outData = (unsigned char*)sgirgb_malloc(numpixels<<2); // 4 bytes per pixel (rgba)
  if (!outData )
  {
    if (x) *x=SGIRGB_ERR_OUT_MEM;
    return sgi_close_and_return(f,outData);
  }

  // allocate channels separately for temp process
  channels = (unsigned char*)sgirgb_malloc( numpixels << 2);  
  if (!channels)
  {
    if (x) *x=SGIRGB_ERR_OUT_MEM;
    return sgi_close_and_return(f,outData);
  }
  rch = channels + (numpixels*0);
  gch = channels + (numpixels*1);
  bch = channels + (numpixels*2);
  ach = channels + (numpixels*3);

  // depending on storage type
  if ( header.storage==SGIRGB_ST_VERBATIM ) // raw mode
  {
    // read all channels consecutive
    offset=0;
    while(offset<numpixels && !feof(f))
      offset+=fread(channels+offset,1,(numpixels<<2)-offset,f);

    //if ( feof(f) && offset<(numpixels<<2) ){ if (x) *x=SGIRGB_ERR_WRONG_FILE; return sgi_close_and_return(f,outData); }
  }
  else
  {
    // RLE compression tables
    // read in tables (alloc + fread)
    tabentries = header.ysize*header.channels; // num entries in table
    k = tabentries*sizeof(unsigned int); // size in bytes
    tabstart = (unsigned int *)sgirgb_malloc( k ); 
    if (!tabstart){ if(x) *x=SGIRGB_ERR_OUT_MEM; return sgi_close_and_return(f,outData); }
    offset=0; 
    while (offset<k && !feof(f)) offset+=fread(tabstart+offset,1,k-offset,f);
    //if ( feof(f) && offset<k ){ if (x) *x=SGIRGB_ERR_WRONG_FILE; sgirgb_free(tabstart); return sgi_close_and_return(f,outData); }
    fseek(f,k,SEEK_CUR); // table lengths not needed
    
    // reads in rle buffer (rest of the file)
    rlebuffsize = sgi_compute_filesize_to_end(f,1);
    if ( !rlebuffsize ) { if (x) *x=SGIRGB_ERR_WRONG_FILE; sgirgb_free(tabstart); return sgi_close_and_return(f,outData);}
    rlebuff=(unsigned char*)sgirgb_malloc(rlebuffsize);
    if ( !rlebuff ){ if(x) *x=SGIRGB_ERR_OUT_MEM; sgirgb_free(tabstart); return sgi_close_and_return(f,outData); }
    offset=0;
    while (offset<rlebuffsize && !feof(f)) offset+=fread(rlebuff+offset,1,rlebuffsize-offset,f);
    //if (feof(f) && offset<rlebuffsize){ if(x) *x=SGIRGB_ERR_WRONG_FILE; sgirgb_free(tabstart); sgirgb_free(rlebuff); return sgi_close_and_return(f,outData); }

    // decode RLE
    // first all scanlines of channel 0, then all of channel 1 ... 
    optr = channels;
    for (row=0;row<tabentries;++row)
    {
      // Input rle: offset
      sgirgb_swap32(&tabstart[row]);
      k = tabstart[row] - (sizeof(sgiRGBHeader)+(tabentries*sizeof(unsigned int)*2)); // from offset 0 in mem
      iptr = rlebuff + k;

      // expand this channel's scanline 'row'
      while (1)
      {
        pixel = *iptr++;
        k = pixel & 0x7f;
        if (!k) break;
        if (pixel & 0x80)
        {
          while (k--)  *optr++ = *iptr++;
        }
        else
        {
          pixel = *iptr++;
          while (k--)  *optr++ = pixel;
        }
      }
    }

    sgirgb_free(rlebuff);
    sgirgb_free(tabstart);
  }

  // filling rest of channels with 0/255
  for (j=header.channels;j<4;++j)
    memset(channels+(j*numpixels), j==3 ? 255:0, numpixels);

  // outputting BGRA packed format (it's flipped Y)  
  offset=0;
  for (row=0;row<header.ysize;++row)
  {
    k = header.xsize*(header.ysize-row-1);
    for (j=0;j<header.xsize;++j,++k)
    {
      outData[offset++]=bch[k]; // B
      outData[offset++]=gch[k]; // G
      outData[offset++]=rch[k]; // R
      outData[offset++]=ach[k]; // A
    }
  }

  // deallocating  channels
  sgirgb_free(channels);

  if (x) *x = header.xsize;
  if (y) *y = header.ysize;
  if (comp) *comp = header.channels;
  fclose(f);
  return outData;
}

#endif
#endif