#ifndef _NFLT_FLTPARSER_H_
#define _NFLT_FLTPARSER_H_
#include <stdint.h>
//==-------------------------------------------------------------------==//
/* configuration */
//==-------------------------------------------------------------------==//
#define NFLT_MAX_OPCODE (nfltRecExtensionFieldXMLString::OPCODE)
#define NFLT_FLT_PARSER_VERSION 1604

//==-------------------------------------------------------------------==//
/* general macros */
//==-------------------------------------------------------------------==//
#define NFLT_OK 0

//==-------------------------------------------------------------------==//
/* macros for endian swapping ptr to ptr */
//==-------------------------------------------------------------------==//
#define _NFLT_ENDIAN_16( a, b )	((uint8_t*)(a))[0] = ((uint8_t*)(b))[1], \
  ((uint8_t*)(a))[1] = ((uint8_t*)(b))[0];

#define _NFLT_ENDIAN_32( a, b )	((uint8_t*)(a))[0] = ((uint8_t*)(b))[3], \
  ((uint8_t*)(a))[1] = ((uint8_t*)(b))[2], \
  ((uint8_t*)(a))[2] = ((uint8_t*)(b))[1], \
  ((uint8_t*)(a))[3] = ((uint8_t*)(b))[0];

#define _NFLT_ENDIAN_64( a, b )	((uint8_t*)(a))[0] = ((uint8_t*)(b))[7], \
  ((uint8_t*)(a))[1] = ((uint8_t*)(b))[6], \
  ((uint8_t*)(a))[2] = ((uint8_t*)(b))[5], \
  ((uint8_t*)(a))[3] = ((uint8_t*)(b))[4], \
  ((uint8_t*)(a))[4] = ((uint8_t*)(b))[3], \
  ((uint8_t*)(a))[5] = ((uint8_t*)(b))[2], \
  ((uint8_t*)(a))[6] = ((uint8_t*)(b))[1], \
  ((uint8_t*)(a))[7] = ((uint8_t*)(b))[0];


#if defined(WIN32) || defined(_WIN32) || (defined(sgi) && defined(unix) && defined(_MIPSEL)) || (defined(sun) && defined(unix) && !defined(_BIG_ENDIAN)) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __LITTLE_ENDIAN)) || (defined(__APPLE__) && defined(__LITTLE_ENDIAN__)) || (defined( _PowerMAXOS ) && (BYTE_ORDER == LITTLE_ENDIAN ))
#  define NFLT_IS_LITTLE_ENDIAN
#elif (defined(sgi) && defined(unix) && defined(_MIPSEB)) || (defined(sun) && defined(unix) && defined(_BIG_ENDIAN)) || defined(vxw) || (defined(__BYTE_ORDER) && (__BYTE_ORDER == __BIG_ENDIAN)) || ( defined(__APPLE__) && defined(__BIG_ENDIAN__)) || (defined( _PowerMAXOS ) && (BYTE_ORDER == BIG_ENDIAN) )
#  define NFLT_IS_BIG_ENDIAN
#else
#  error nfltParser - unknown endian type
#endif

/////////////////////

#ifdef NFLT_IS_LITTLE_ENDIAN
#define NFLT_ENDIAN_16( d ) { uint16_t tmp; _NFLT_ENDIAN_16( &tmp, &(d) ); (d) = tmp; }
#define NFLT_ENDIAN_32( d ) { uint32_t tmp; _NFLT_ENDIAN_32( &tmp, &(d) ); (d) = tmp; }
#define NFLT_ENDIAN_32r( d ) { float tmp; _NFLT_ENDIAN_32( &tmp, &(d) ); (d) = tmp; }
#define NFLT_ENDIAN_64( d ) { double tmp; _NFLT_ENDIAN_64( &tmp, &(d) ); (d) = tmp; }
#define NFLT_ENDIAN_64r( d ) { double tmp; _NFLT_ENDIAN_64( &tmp, &(d) ); (d) = tmp; }
#else
#define NFLT_ENDIAN_16( d ) 
#define NFLT_ENDIAN_32( d )
#define NFLT_ENDIAN_32r( d )
#define NFLT_ENDIAN_64( d )
#define NFLT_ENDIAN_64r( d )
#endif

//==-------------------------------------------------------------------==//
//==-------------------------------------------------------------------==//
#pragma pack(push,1)
struct nfltRecordType
{
  uint16_t opcode;
  uint16_t length;
};

struct nfltRecHeader
{
  enum{ OPCODE=1 };
  char      ascii[8];
  int32_t   formatRevLevel;
  int32_t   editRevLevel;
  char      dateTime[32];
  int16_t   nextGroupNodeID;
  int16_t   nextLODNodeID;
  int16_t   nextObjectNodeID;
  int16_t   nextFaceNodeID;
  int16_t   unitMultiplier;
  char      vertexCoordUnits;
  char      texwhiteNewFaces;
  int32_t   flags;
  int32_t   reserved0[6];
  int32_t   projType;
  int32_t   reserved1[7];
  int16_t   nextDOFNodeID;
  int16_t   vertexStorageType;
  int32_t   dbOrigin;
  double    southwestDBX;
  double    southwestDBY;
  double    deltaDBX;
  double    deltaDBY;
  int16_t   nextSoundNodeID;
  int16_t   nextPathNodeID;
  int32_t   reserved2[2];
  int16_t   nextClipNodeID;
  int16_t   nextTextNodeID;
  int16_t   nextBSPNodeID;
  int16_t   nextSwitchNodeID;
  int32_t   reserved3;
  double    southwestCornerLat;
  double    southwestCornerLon;
  double    northeastCornerLat;
  double    northeastCornerLon;
  double    originLat;
  double    originLon;
  double    lambertUpperLat;
  double    lambertlowerLat;
  int16_t   nextLightSourceNodeID;
  int16_t   nextLightPointNodeID;
  int16_t   nextRoadNodeID;
  int16_t   nextCATNodeID;
  int16_t   reserved4[4];
  int32_t   earthEllipsoidModel;
  int16_t   nextAdaptiveNodeID;
  int16_t   nextCurveNodeID;
  int16_t   utmZone;
  char      reserved5[6];
  double    deltaDBZ;
  double    radius;
  int16_t   nextMeshNodeID;
  int16_t   nextLightPointSystemID;
  int32_t   reserved6;
  double    earthMajorAxis;
  double    earthMinorAxis;
  int32_t   reserved7;
};

struct nfltRecObject
{
  enum{OPCODE=4};
  char     name[8];
  int32_t  flags;
  int16_t  priority;
  int16_t  transparency;
  int16_t  sfx1;
  int16_t  sfx2;
  int16_t  significance;
  int16_t  reserved;
};

struct nfltRecFace
{
  enum{OPCODE=5};
  char      name[8];
  int32_t   irColorCode;
  int16_t   priority;
  char      drawType;
  char      texWhite;
  uint16_t  colorNameNdx;
  uint16_t  altColorNameNdx;
  char      reserved0;
  char      templateBBoard;
  int16_t   detailTexturePatternNdx;
  int16_t   texturePatternNdx;
  int16_t   materialNdx;
  int16_t   surfaceMaterialCode;
  int16_t   featureID;
  int32_t   irMaterialCode;
  uint16_t  transparency;
  char      lodGenControl;
  char      lineStyleNdx;
  int32_t   flags;
  unsigned char   lightMode;
  char      reserved1[7];
  uint32_t  primaryPackedABGR;
  uint32_t  alternatePackedABGR;
  int16_t   textureMappingNdx;
  int16_t   reserved2;
  uint32_t  primaryColorNdx;
  uint32_t  alternateColorNdx;
  int16_t   reserved3;
  int16_t   shaderNdx;
};

struct nfltRecVertexPalette
{
  enum{ OPCODE=67 };
  int32_t  length;
  void*    vertexPoolBuffer;
};

struct nfltRecTexturePalette
{
  enum { OPCODE=64 };
  char    filenameTexPattern[200];
  int32_t texturePatternIndex;
  int32_t locationXY[2];
};

struct nfltRecExternalReference
{
  enum{ OPCODE=63 };
  char    path[200];
  int32_t reserved0;
  int32_t flags;
  int16_t viewAsBB;
  int16_t reserved1;
};

struct nfltRecExtensionFieldXMLString
{
  enum { OPCODE=154 };
};

struct nfltRecMeshNode
{
  enum { OPCODE = 84 };
};
#pragma pack(pop)


//==-------------------------------------------------------------------==//
// FUNCTIONS 
//==-------------------------------------------------------------------==//
struct nfltContext;
struct nfltNode;
struct nfltFile;

const char* nfltGetOpcodeName(uint16_t opcode);
bool nfltIsOpcodeObsolete(uint16_t opcode);

int nfltBuildGraph( const char* filename, nfltNode** root );
void nfltDeleteGraph(nfltNode** root);
uint16_t nfltNodeGetOpcode(nfltNode* node);
int nfltNodeReadRecord(nfltNode* node, uint16_t opcode, nfltContext* context);

#endif