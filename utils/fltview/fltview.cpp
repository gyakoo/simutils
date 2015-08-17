#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <algorithm>

#define FLT_IMPLEMENTATION
#include <flt.h>

typedef struct temp
{
  int a;
  double b;
  char c;
  float d;
  int e;
}temp;

#define po(n,t) printf( "offset to " #n ": %d\n", flt_offsetto(n,t) );

int main(int argc, const char** argv)
{
  flt_opts opts;
  flt of;
  if ( flt_load_from_filename( "../titanic/TITANIC.flt", &of, &opts) )
  {

  }
  return 0;
}

/*
#include "nflt.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <set>
#include <map>
#include <vector>
#include <string>
#include <algorithm>


struct FileNode
{
  int references; // references to this file
  int noObjects;  // no objects this file has
  char vformats;  // vertex formats it has in the vertex palette
  int noTexRefs;  // no texture refences this file has

  FileNode():references(1), noObjects(0), vformats(0), noTexRefs(0){}
};

std::string dataDir;
std::string curFile;
typedef std::map<std::string,FileNode*> MapFileRefs;
typedef std::map<std::string,int> MapRefs;
MapFileRefs fileRefs;
MapRefs textureRefs;
unsigned int maxVPalSize=0;
bool verbose=false;
int maxRefLevel=0;

int minBytesToRead(unsigned short len, size_t slen)
{
  return size_t(len) < slen ? (int)len : (int)slen;
}

void dumpFlt(const char* pathname, int refLevel)
{
  if ( refLevel > maxRefLevel )
    maxRefLevel = refLevel;

  auto it = fileRefs.find(pathname);
  if ( it != fileRefs.end() )
  {
    ++it->second->references;
    return;
  }
  FileNode* fnode = new FileNode;
  fileRefs[pathname] = fnode;
  curFile = pathname;

  FILE* fltFile = fopen(pathname, "rb");
  if ( !fltFile )
    return;

  printf( "%s\n", pathname );
  nfltRecordType rtype;
  while ( fread( &rtype, sizeof(nfltRecordType), 1, fltFile) == 1 )
  {
    NFLT_ENDIAN_16(rtype.opcode); NFLT_ENDIAN_16(rtype.length); 
    rtype.length -= 4;
    if ( verbose )
    {
      for ( int i = 0; i < refLevel; ++i ) printf( "  " );
      printf( "[%03d] [%s] (%d bytes) %s\n", rtype.opcode, nfltGetOpcodeName(rtype.opcode), rtype.length, nfltIsOpcodeObsolete(rtype.opcode)?"OBSOLETE":"" );
    }
    switch ( rtype.opcode )
    {
    case nfltRecHeader::OPCODE:
      {
        nfltRecHeader header;
        rtype.length -= fread( &header, 1, minBytesToRead(rtype.length,sizeof(nfltRecHeader)), fltFile );
        NFLT_ENDIAN_16(header.nextGroupNodeID);
        NFLT_ENDIAN_16(header.nextObjectNodeID);
      }break;
    case nfltRecObject::OPCODE:
      {
        nfltRecObject obj;
        rtype.length -= fread( &obj, 1, minBytesToRead(rtype.length,sizeof(nfltRecObject)), fltFile);
        if ( verbose )
        {
          for ( int i = 0; i < refLevel+1; ++i ) printf( "  " );
          printf("%s\n",obj.name);
        }
        ++fnode->noObjects;
      }break;
    case nfltRecExternalReference::OPCODE:
      {
        nfltRecExternalReference extRef;
        rtype.length -= fread( &extRef, 1, minBytesToRead(rtype.length,sizeof(nfltRecExternalReference)), fltFile );
        char refPathName[512];
        sprintf(refPathName, "%s%s", dataDir.c_str(), extRef.path );
        dumpFlt(refPathName, refLevel+1);
      }break;
    case nfltRecVertexPalette::OPCODE:
      {
        unsigned int length=0;
        rtype.length -= fread( &length, 1, minBytesToRead( rtype.length, sizeof(unsigned int)), fltFile );
        NFLT_ENDIAN_32(length);
        length-=8;
        if ( length > maxVPalSize ) maxVPalSize = length;
        if ( verbose )
          printf( "Vertex Palette: %u\n", length);
      }break;
    case nfltRecTexturePalette::OPCODE:
      {
        nfltRecTexturePalette texPal;
        rtype.length -= fread( &texPal, 1, minBytesToRead(rtype.length, sizeof(nfltRecTexturePalette)), fltFile );
        if ( verbose )
        {
          for ( int i = 0; i < refLevel+1; ++i ) printf( "  " );
          printf( "%s\n", texPal.filenameTexPattern );
        }
        textureRefs[texPal.filenameTexPattern]++;
        fnode->noTexRefs++;
      }break;
    case 68: case 69: case 70: case 71:
      {
        fnode->vformats |= (1<<(71-rtype.opcode));
      }break;
    case nfltRecMeshNode::OPCODE:
      printf( "Mesh Node\n" );
      break;
    }


    if ( rtype.length > 0 )
      fseek( fltFile, rtype.length, SEEK_CUR);    
  }

  fclose(fltFile);
}

void extractDir(const char* pathname, std::string& outDir)
{
  int len = strlen(pathname);  
  while ( len>=0 )
  {
    if ( pathname[len] == '\\' || pathname[len] == '/' )
      break;
    --len;
  }

  ++len;
  outDir.reserve(len);
  for ( int i = 0; i < len; ++i )
    outDir+=pathname[i];
}

int main(int argc, const char** argv)
{
  if ( argc < 2 )
  {
    printf( "Usage: %s flt_file\n", argv[0] );
    return -1;
  }
  verbose = argc>2 && strcmp(argv[2],"-v")==0;
  std::string masterFile = argv[1];
  extractDir(masterFile.c_str(), dataDir);
  printf( "Parsing...\n" );
  dumpFlt(masterFile.c_str(), 0);

  printf( "\nEXTERNAL REFS\n" );
  unsigned int acumExtRefs=0;
  for ( auto it=fileRefs.begin(); it != fileRefs.end(); ++it )
  {
    char fmt = it->second->vformats;
    printf( "%s [Refs=%d|%d] [Nobjs=%d] [%d:", it->first.c_str(), it->second->references, it->second->noTexRefs, it->second->noObjects, fmt);
    if ( fmt&0x01 ) printf( "CT,");
    if ( fmt&0x02 ) printf( "CNT,");
    if ( fmt&0x04 ) printf( "CN," );
    if ( fmt&0x08 ) printf( "C" );
    printf( "]\n" );
    acumExtRefs += it->second->references;
    delete it->second;
  }
  unsigned int acumTexRefs = 0;
  printf( "\nTEXTURE REFS\n" );
  for ( auto it=textureRefs.begin(); it != textureRefs.end(); ++it )
  {
    printf( "%s [%d]\n", it->first.c_str(), it->second);
    acumTexRefs += it->second;
  }

  printf( "\n");
  printf( "No. external references=%u\n", acumExtRefs);
  printf( "No. texture references=%u\n", acumTexRefs);
  printf( "Max. Vertex Palette Size=%u bytes\n", maxVPalSize);
  printf( "Max. Reference Depth=%d\n", maxRefLevel);

  return 0;
}
*/