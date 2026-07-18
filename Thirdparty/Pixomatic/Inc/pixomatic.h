//-----------------------------------------------------------------------------
// FILE: PIXOMATIC.H
//
// Copyright (c) 2002 RAD Game Tools, Inc. All rights reserved.
//-----------------------------------------------------------------------------

#ifndef _PIXOMATIC_H_
#define _PIXOMATIC_H_

#if defined (WIN32) && !defined (_WIN32)
#define _WIN32
#endif

// Switch the import/export state of the functions depending on who's
// compiling, and what compiler we're using
#define PIXO_DYNIMP(ret)        __declspec(dllimport) ret __stdcall
#define PIXO_DYNEXP(ret)        __declspec(dllexport) ret __stdcall
#ifdef _WIN32
#define PIXO_CALLBACK(ret) ret  __cdecl
#else
#define PIXO_CALLBACK(ret) ret  __attribute__((cdecl))
#endif
#define PIXO_DYNIMPDATA(type)   __declspec(dllimport) type
#define PIXO_DYNEXPDATA(type)   __declspec(dllexport) type

#if defined(_LINUX) || !defined(_WIN32)

#define BUILDING_PIXO_STATIC        1

#endif

#if (BUILDING_PIXO || BUILDING_PIXO_STATIC)
  #if BUILDING_PIXO_STATIC
    #define PIXO_DYNLINK(ret) ret
    #define PIXO_DYNLINKDATA(type) type
  #else
    #define PIXO_DYNLINK(ret) PIXO_DYNEXP(ret)
    #define PIXO_DYNLINKDATA(type) PIXO_DYNEXPDATA(type)
  #endif
#else
  #define PIXO_DYNLINK(ret) PIXO_DYNIMP(ret)
  #define PIXO_DYNLINKDATA(type) PIXO_DYNIMPDATA(type)
#endif

#define PIXO_U8      unsigned char
#define PIXO_U16     unsigned short
#define PIXO_U32     unsigned int
#define PIXO_U64     unsigned __int64

#define PIXO_S8      char
#define PIXO_S16     signed short
#define PIXO_S32     signed int
#define PIXO_S64     signed __int64

#define PIXO_F32     float
#define PIXO_F64     double

#ifdef __cplusplus
extern "C" {
#endif

#define PIXO_RASTERIZATION_STATE_STRINGS_PIPELINE_NORMAL 0x10
#define PIXO_RASTERIZATION_STATE_STRINGS_PIPELINE_STENCIL 0x20
typedef enum PIXO_RASTERIZATION_PIPELINE
{
    PIXO_RASTERIZATION_PIPELINE_STANDARD = 0x00000000,
    PIXO_RASTERIZATION_PIPELINE_STENCIL_MODIFY = 0x80000000
} PIXO_RASTERIZATION_PIPELINE;
typedef enum PIXO_ZCOMPARE
{
    PIXO_ZCOMPARE_ALWAYS = 0x00000000,
    PIXO_ZCOMPARE_GE = 0x00000001,
    PIXO_ZCOMPARE_LE = 0x00000002,
    PIXO_ZCOMPARE_GT = 0x00000003,
    PIXO_ZCOMPARE_LT = 0x00000004,
    PIXO_ZCOMPARE_EQ = 0x00000005,
    PIXO_ZCOMPARE_NE = 0x00000006
} PIXO_ZCOMPARE;
typedef enum PIXO_ZWRITE
{
    PIXO_ZWRITE_OFF = 0x00000000,
    PIXO_ZWRITE_ON = 0x00000008
} PIXO_ZWRITE;
typedef enum PIXO_TEX_FILTER
{
    PIXO_TEX_FILTER_POINT = 0x00000000,
    PIXO_TEX_FILTER_BILINEAR = 0x00000010,
    PIXO_TEX_FILTER_FAST_TWO_POINT = 0x00000020,
    PIXO_TEX_FILTER_FAST_FOUR_POINT = 0x00000030
} PIXO_TEX_FILTER;
typedef enum PIXO_TEX_FORMAT
{
    PIXO_TEX_FORMAT_NONE = 0x00000000,
    PIXO_TEX_FORMAT_ARGB8888 = 0x00000040,
    PIXO_TEX_FORMAT_PALETTIZED = 0x00000080
} PIXO_TEX_FORMAT;
typedef enum PIXO_TEX_BORDER
{
    PIXO_TEX_BORDER_WRAP = 0x00000000,
    PIXO_TEX_BORDER_CLAMP = 0x00000100,
    PIXO_TEX_BORDER_CLAMP_LARGE = 0x00000300
} PIXO_TEX_BORDER;
typedef enum PIXO_DIFFUSE
{
    PIXO_DIFFUSE_NONE = 0x00000000,
    PIXO_DIFFUSE_FLAT = 0x00010000,
    PIXO_DIFFUSE_GOURAUD = 0x00020000
} PIXO_DIFFUSE;
typedef enum PIXO_FOG
{
    PIXO_FOG_OFF = 0x00000000,
    PIXO_FOG_ON = 0x00040000
} PIXO_FOG;
typedef enum PIXO_SPECULAR
{
    PIXO_SPECULAR_OFF = 0x00000000,
    PIXO_SPECULAR_ON = 0x00080000
} PIXO_SPECULAR;
typedef enum PIXO_SCOMPARE
{
    PIXO_SCOMPARE_ALWAYS = 0x00000000,
    PIXO_SCOMPARE_GE = 0x00100000,
    PIXO_SCOMPARE_LE = 0x00200000,
    PIXO_SCOMPARE_GT = 0x00300000,
    PIXO_SCOMPARE_LT = 0x00400000,
    PIXO_SCOMPARE_EQ = 0x00500000,
    PIXO_SCOMPARE_NE = 0x00600000
} PIXO_SCOMPARE;
typedef enum PIXO_ZBUFFER_TYPE
{
    PIXO_ZBUFFER_TYPE_16 = 0x00000000,
    PIXO_ZBUFFER_TYPE_24 = 0x00800000,
    PIXO_ZBUFFER_TYPE_24S = 0x01800000
} PIXO_ZBUFFER_TYPE;
typedef enum PIXO_DOT3
{
    PIXO_DOT3_OFF = 0x00000000,
    PIXO_DOT3_WITH_LIGHTVEC = 0x02000000,
    PIXO_DOT3_WITH_SPECULAR = 0x04000000
} PIXO_DOT3;
typedef enum PIXO_MULTIPLY_SCALE
{
    PIXO_MULTIPLY_SCALE_FULL_RANGE_OFF = 0x00000000,
    PIXO_MULTIPLY_SCALE_FULL_RANGE_ON = 0x08000000
} PIXO_MULTIPLY_SCALE;
typedef enum PIXO_FIXED_MODULATION
{
    PIXO_FIXED_MODULATION_1X = 0x00000000,
    PIXO_FIXED_MODULATION_2X = 0x10000000,
    PIXO_FIXED_MODULATION_FACTOR = 0x20000000
} PIXO_FIXED_MODULATION;
typedef enum PIXO_DOT3_BIAS
{
    PIXO_DOT3_BIAS_128_IS_POINT_5 = 0x00000000,
    PIXO_DOT3_BIAS_128_IS_ZERO = 0x00000002
} PIXO_DOT3_BIAS;
typedef enum PIXO_RASTEROP
{
    PIXO_RASTEROP_ONExSRC_PLUS_ZEROxDST = 0x00000000,
    PIXO_RASTEROP_ONExSRC_PLUS_ONExDST = 0x00000004,
    PIXO_RASTEROP_ZEROxSRC_PLUS_ONExDST = 0x00000008,
    PIXO_RASTEROP_ZEROxSRC_PLUS_SRCxDST = 0x0000000c,
    PIXO_RASTEROP_ALPHABLEND_ALPHAxSRC_PLUS_ONEMINUSALPHAxDST = 0x00000010,
    PIXO_RASTEROP_ALPHABLEND_ONExSRC_PLUS_ONEMINUSALPHAxDST = 0x00000014,
    PIXO_RASTEROP_ALPHABLEND_ALPHAxSRC_PLUS_ONExDST = 0x00000018,
    PIXO_RASTEROP_USER_CODE = 0x0000001c,
    PIXO_RASTEROP_SOURCE_PASSTHROUGH = 0x00000000,
    PIXO_RASTEROP_NONE = 0x00000008
} PIXO_RASTEROP;
typedef enum PIXO_DIFFUSE_ALPHAOP
{
    PIXO_DIFFUSE_ALPHAOP_MODULATE = 0x00000000,
    PIXO_DIFFUSE_ALPHAOP_USE_DIFFUSE = 0x00000040,
    PIXO_DIFFUSE_ALPHAOP_USE_FRAG = 0x00000080
} PIXO_DIFFUSE_ALPHAOP;
typedef enum PIXO_TEX1_ALPHAOP
{
    PIXO_TEX1_ALPHAOP_MODULATE = 0x00000000,
    PIXO_TEX1_ALPHAOP_ADD = 0x00000100,
    PIXO_TEX1_ALPHAOP_USE_FRAG = 0x00000200,
    PIXO_TEX1_ALPHAOP_USE_TEX1 = 0x00000300,
    PIXO_TEX1_ALPHAOP_FASTPATH_UNDEFINED_RESULT = 0x00000400
} PIXO_TEX1_ALPHAOP;
typedef enum PIXO_ACOMPARE
{
    PIXO_ACOMPARE_ALWAYS = 0x00000000,
    PIXO_ACOMPARE_GE = 0x00000800,
    PIXO_ACOMPARE_LE = 0x00001000,
    PIXO_ACOMPARE_GT = 0x00001800,
    PIXO_ACOMPARE_LT = 0x00002000,
    PIXO_ACOMPARE_EQ = 0x00002800,
    PIXO_ACOMPARE_NE = 0x00003000,
    PIXO_ACOMPARE_NE_ZERO = 0x00003800
} PIXO_ACOMPARE;
typedef enum PIXO_TEX1_RGBOP
{
    PIXO_TEX1_RGBOP_MODULATE = 0x00000000,
    PIXO_TEX1_RGBOP_ADD = 0x00004000,
    PIXO_TEX1_RGBOP_USE_FRAG = 0x00008000,
    PIXO_TEX1_RGBOP_USE_TEX1 = 0x0000c000,
    PIXO_TEX1_RGBOP_ADD_SIGNED = 0x00010000,
    PIXO_TEX1_RGBOP_BLEND_FRAG_ALPHA = 0x00020000,
    PIXO_TEX1_RGBOP_BLEND_ONE_MINUS_FRAG_ALPHA = 0x00024000,
    PIXO_TEX1_RGBOP_BLEND_TEX1_ALPHA = 0x00028000,
    PIXO_TEX1_RGBOP_BLEND_ONE_MINUS_TEX1_ALPHA = 0x0002c000,
    PIXO_TEX1_RGBOP_BLEND_DIFFUSE_ALPHA = 0x00030000,
    PIXO_TEX1_RGBOP_BLEND_ONE_MINUS_DIFFUSE_ALPHA = 0x00034000
} PIXO_TEX1_RGBOP;
typedef enum PIXO_TEX1_PIPELINE_POSITION
{
    PIXO_TEX1_PIPELINE_POSITION_BEFORE_DIFFUSE = 0x00000000,
    PIXO_TEX1_PIPELINE_POSITION_AFTER_DIFFUSE = 0x00040000
} PIXO_TEX1_PIPELINE_POSITION;
typedef enum PIXO_ZPASS
{
    PIXO_ZPASS_NONE = 0x00000000,
    PIXO_ZPASS_SET = 0x00000008,
    PIXO_ZPASS_ADD = 0x00000010,
    PIXO_ZPASS_XOR = 0x00000018,
    PIXO_ZPASS_INC = 0x00000020,
    PIXO_ZPASS_DEC = 0x00000028
} PIXO_ZPASS;
typedef enum PIXO_ZFAIL
{
    PIXO_ZFAIL_NONE = 0x00000000,
    PIXO_ZFAIL_SET = 0x00000040,
    PIXO_ZFAIL_ADD = 0x00000080,
    PIXO_ZFAIL_XOR = 0x000000c0,
    PIXO_ZFAIL_INC = 0x00000100,
    PIXO_ZFAIL_DEC = 0x00000140
} PIXO_ZFAIL;

typedef struct PIXO_PIPELINE_STATE_FLAGS PIXO_PIPELINE_STATE_FLAGS;

typedef struct PIXO_RASTERIZATION_MODE_STRINGS PIXO_RASTERIZATION_MODE_STRINGS;

typedef struct PIXO_RASTERIZATION_STATE_STRINGS PIXO_RASTERIZATION_STATE_STRINGS;
#define PIXO_MAKEARGB0(a, r, g, b) ((PIXO_ARGB0)((((a)&0xff)<<24) | (((r)&0xff)<<16) | (((g)&0xff)<<8) | ((b)&0xff)))
#define PIXO_VERTEXCALLBACK_CALCULATE_OBJECTSPACECOORDINATES 1
#define PIXO_MAX_STREAMS 8
#define PIXO_STREAM_POS(_count) (0x40000000|((_count)<<8)|0)
#define PIXO_STREAM_POSSCREEN(_count) (0x41000000|((_count)<<8)|0)
#define PIXO_STREAM_TEX(_texnum, _count) (0x00000000|((_count)<<8)|((_texnum)?9:5))
#define PIXO_STREAM_POSXY (0x40000000|(2<<8)|0)
#define PIXO_STREAM_POSXYZ (0x40000000|(3<<8)|0)
#define PIXO_STREAM_POSXYZW (0x40000000|(4<<8)|0)
#define PIXO_STREAM_POSSCREENXY (0x41000000|(2<<8)|0)
#define PIXO_STREAM_POSSCREENXYZ (0x41000000|(3<<8)|0)
#define PIXO_STREAM_POSSCREENXYZRHW (0x41000000|(4<<8)|0)
#define PIXO_STREAM_DIFFUSE (0x00000000|(1<<8)|3)
#define PIXO_STREAM_SPECULAR (0x00000000|(1<<8)|4)
#define PIXO_STREAM_DIFFUSE_ABGR (0x80000000|(1<<8)|3)
#define PIXO_STREAM_SPECULAR_ABGR (0x80000000|(1<<8)|4)
#define PIXO_STREAM_TEX0 (0x00000000|(2<<8)|5)
#define PIXO_STREAM_TEX1 (0x00000000|(2<<8)|9)
#define PIXO_STREAM_TEXSTR0 (0x00000000|(3<<8)|5)
#define PIXO_STREAM_TEXSTR1 (0x00000000|(3<<8)|9)
#define PIXO_STREAM_TEXSTRQ0 (0x00000000|(4<<8)|5)
#define PIXO_STREAM_TEXSTRQ1 (0x00000000|(4<<8)|9)
#define PIXO_STREAM_NORMAL (0x00000000|(3<<8)|13)
#define PIXO_STREAM_POINTSIZE (0x00000000|(1<<8)|17)
#define PIXO_STREAM_SKIP(_x) (0x10000000|(((_x)&0xffff)<<8))
#define PIXO_STREAM_SETSTREAM(_x) (0x20000000|(((_x)&0x7)<<8))
#define PIXO_STREAM_END 0xffffffff
#define PIXO_TRANSFORM_MATRIX_IDENTITY ((PIXO_F32 *)0)
typedef PIXO_U32 PIXO_ARGB0;
typedef PIXO_U16 PIXO_FIXED12_4;
typedef PIXO_U8 PIXO_CODEBUFFER;
typedef enum PIXO_ZCLIPRANGE
{
    PIXO_ZCLIPRANGE_MINUS1_1,
    PIXO_ZCLIPRANGE_0_1
} PIXO_ZCLIPRANGE;
typedef enum PIXO_TEX_PROJECTED
{
    PIXO_TEX_PROJECTED_OFF,
    PIXO_TEX_PROJECTED_ON
} PIXO_TEX_PROJECTED;
typedef enum PIXO_TEXGEN
{
    PIXO_TEXGEN_OFF,
    PIXO_TEXGEN_UNPROJECTED_XYZW,
    PIXO_TEXGEN_PROJECTED_XYZ1
} PIXO_TEXGEN;
typedef enum PIXO_PRIMITIVETYPES
{
    PIXO_TRIANGLELIST,
    PIXO_TRIANGLESTRIP,
    PIXO_TRIANGLEFAN,
    PIXO_QUADLIST,
    PIXO_POLYGON,
    PIXO_POINTLIST,
    PIXO_LINELIST,
    PIXO_LINESTRIP
} PIXO_PRIMITIVETYPES;
typedef enum PIXO_CULLINGPOLARITYTOGGLE
{
    PIXO_CULLINGPOLARITYTOGGLE_OFF,
    PIXO_CULLINGPOLARITYTOGGLE_ON
} PIXO_CULLINGPOLARITYTOGGLE;
typedef enum PIXO_VERTEXDISTANCEFOG
{
    PIXO_VERTEXDISTANCEFOG_OFF,
    PIXO_VERTEXDISTANCEFOG_ON
} PIXO_VERTEXDISTANCEFOG;
typedef enum PIXO_VERTEXDISTANCEFOGTYPE
{
    PIXO_VERTEXDISTANCEFOG_DEVICESPACE,
    PIXO_VERTEXDISTANCEFOG_CAMERASPACE,
    PIXO_VERTEXDISTANCEFOG_W_CAMERASPACE
} PIXO_VERTEXDISTANCEFOGTYPE;
typedef enum PIXO_FASTZSPRETEST
{
    PIXO_FASTZSPRETEST_OFF,
    PIXO_FASTZSPRETEST_ON
} PIXO_FASTZSPRETEST;
typedef enum PIXO_NORMALS
{
    PIXO_NORMALS_OFF,
    PIXO_NORMALS_ON
} PIXO_NORMALS;
typedef enum PIXO_CULL
{
    PIXO_CULL_CCW,
    PIXO_CULL_CW,
    PIXO_CULL_NONE
} PIXO_CULL;
typedef enum PIXO_ALPHATEST
{
    PIXO_ALPHATEST_OFF = 0,
    PIXO_ALPHATEST_ON = 1
} PIXO_ALPHATEST;
typedef enum PIXO_BOTTOMUPRENDERING
{
    PIXO_BOTTOMUPRENDERING_OFF,
    PIXO_BOTTOMUPRENDERING_ON
} PIXO_BOTTOMUPRENDERING;
typedef enum PIXO_WIREFRAME
{
    PIXO_WIREFRAME_OFF,
    PIXO_WIREFRAME_ON_FILL,
    PIXO_WIREFRAME_ON
} PIXO_WIREFRAME;
typedef enum PIXO_POLYGONMIPMAP
{
    PIXO_POLYGONMIPMAP_OFF,
    PIXO_POLYGONMIPMAP_ON
} PIXO_POLYGONMIPMAP;
typedef enum PIXO_SCALEPOINTSIZEBYRHW
{
    PIXO_SCALEPOINTSIZEBYRHW_OFF,
    PIXO_SCALEPOINTSIZEBYRHW_ON
} PIXO_SCALEPOINTSIZEBYRHW;
typedef enum PIXO_TRIANGLESTRIPEARLYDEGENERATECHECK
{
    PIXO_TRIANGLESTRIPEARLYDEGENERATECHECK_OFF,
    PIXO_TRIANGLESTRIPEARLYDEGENERATECHECK_ON
} PIXO_TRIANGLESTRIPEARLYDEGENERATECHECK;
typedef enum PIXO_CLEARTARGETS
{
    PIXO_CLEARFRAMEBUFFER = 0x1,
    PIXO_CLEARSTENCIL = 0x2,
    PIXO_CLEARZBUFFER = 0x4
} PIXO_CLEARTARGETS;

typedef struct PIXO_VERTEX_SCREEN_POS PIXO_VERTEX_SCREEN_POS;

typedef struct PIXO_CLIP_INFO PIXO_CLIP_INFO;

typedef struct PIXO_VERT_CACHE PIXO_VERT_CACHE;

typedef struct PIXO_VERTEX PIXO_VERTEX;

typedef struct PIXO_STATECODEBUFFER PIXO_STATECODEBUFFER;

typedef struct PIXO_STREAMDEFCODEBUFFER PIXO_STREAMDEFCODEBUFFER;
typedef enum PIXO_BUFFEROPEN_FLAGS
{
    PIXO_BUF_2XZOOM = 0x00000001,
    PIXO_BUF_2XZOOM_VERTICAL = 0x00008001,
    PIXO_BUF_2XANTIALIAS = 0x00000002,
    PIXO_BUF_FORCEDDRAW = 0x00000004,
    PIXO_BUF_FORCEGDI = 0x00000008,
    PIXO_BUF_2XZOOM_NO_BILINEAR = 0x80000000
} PIXO_BUFFEROPEN_FLAGS;
typedef enum PIXO_BUFFERLOCK_FLAGS
{
    PIXO_BUF_LOCK_COMPOSITE = 0x00000001
} PIXO_BUFFERLOCK_FLAGS;

typedef struct PIXO_BUF PIXO_BUF;
#define PIXO_MEM_SPEED_MAX 12
#define PIXO_CPUID_DO_MEMTESTS 0x1
typedef enum PIXO_CPU_FEATURES_FLAGS
{
    PIXO_FEATURE_CPUID = 0x00000001,
    PIXO_FEATURE_STD_FEATURES = 0x00000002,
    PIXO_FEATURE_EXT_FEATURES = 0x00000004,
    PIXO_FEATURE_TSC = 0x00000008,
    PIXO_FEATURE_MMX = 0x00000010,
    PIXO_FEATURE_CMOV = 0x00000020,
    PIXO_FEATURE_3DNOW = 0x00000040,
    PIXO_FEATURE_3DNOWEXT = 0x00000080,
    PIXO_FEATURE_MMXEXT = 0x00000100,
    PIXO_FEATURE_SSEFP = 0x00000200,
    PIXO_FEATURE_SSE2 = 0x00000400,
    PIXO_FEATURE_HTT = 0x00000800
} PIXO_CPU_FEATURES_FLAGS;
typedef enum PIXO_CPU_FEATURES_TYPES
{
    PIXO_CPU_Unknown = 0x00000000,
    PIXO_CPU_GenuineIntel = 0x10000000,
    PIXO_CPU_Intel_486 = 0x00000001,
    PIXO_CPU_Intel_Pentium = 0x00000002,
    PIXO_CPU_Intel_PII_PIII = 0x00000003,
    PIXO_CPU_Intel_PIV = 0x00000004,
    PIXO_CPU_AuthenticAMD = 0x20000000,
    PIXO_CPU_AMD_486 = 0x00000001,
    PIXO_CPU_AMD_K5 = 0x00000002,
    PIXO_CPU_AMD_K6 = 0x00000003,
    PIXO_CPU_AMD_K7 = 0x00000004,
    PIXO_CPU_CyrixInstead = 0x30000000,
    PIXO_CPU_Cyrix_MGX = 0x00000001,
    PIXO_CPU_Cyrix_Pentium = 0x00000002,
    PIXO_CPU_Cyrix_III = 0x00000003
} PIXO_CPU_FEATURES_TYPES;

typedef struct PIXO_CPU_FEATURES PIXO_CPU_FEATURES;
typedef PIXO_CALLBACK(void) PIXO_VERTEXCALLBACK(PIXO_VERTEX * vert,
                                                PIXO_CLIP_INFO * clip_info);
typedef PIXO_CALLBACK(PIXO_S32) PIXO_DRAWTRIANGLECALLBACK(PIXO_VERTEX const * verts);

struct PIXO_PIPELINE_STATE_FLAGS
{
    PIXO_U32 LowPart;
    PIXO_U32 HighPart;
};

struct PIXO_RASTERIZATION_MODE_STRINGS
{
    PIXO_S8 const * name;
    PIXO_U32 value;
};

struct PIXO_RASTERIZATION_STATE_STRINGS
{
    PIXO_S8 const * name;
    PIXO_U32 mask;
    PIXO_U32 WordPart;
    PIXO_S32 pipeline_mode;
    PIXO_RASTERIZATION_MODE_STRINGS const * modeinfo;
};

struct PIXO_VERTEX_SCREEN_POS
{
    PIXO_FIXED12_4 X,  Y;
    PIXO_F32 Z,  RHW;
};

struct PIXO_CLIP_INFO
{
    PIXO_F32 posxyzw[4];
    PIXO_U16 clip_flags;
    PIXO_S16 pad;
};

struct PIXO_VERT_CACHE
{
    PIXO_VERTEX_SCREEN_POS screen_pos;
    PIXO_CLIP_INFO ci;
};

struct PIXO_VERTEX
{
    PIXO_FIXED12_4 X;
    PIXO_FIXED12_4 Y;
    PIXO_F32 Z;
    PIXO_F32 RHW;
    PIXO_ARGB0 Diffuse;
    PIXO_ARGB0 Specular;
    PIXO_F32 S0, T0, R0,  Q0;
    PIXO_F32 S1, T1, R1,  Q1;
    PIXO_F32 Nx, Ny,  Nz;
    PIXO_F32 const * ObjectSpaceCoordinates;
    PIXO_F32 PointSize;
    PIXO_F32 RHW2;
    PIXO_S32 processed;
    PIXO_F32 Project_Q0,  Project_Q1;
    PIXO_CLIP_INFO * ci;
    PIXO_S32 Index;
};

struct PIXO_STATECODEBUFFER
{
    PIXO_PIPELINE_STATE_FLAGS flags;
    PIXO_CODEBUFFER * code;
    PIXO_CODEBUFFER * pZAndStencilPretestCode;
};

struct PIXO_STREAMDEFCODEBUFFER
{
    PIXO_CODEBUFFER * drawprim_code;
    PIXO_CODEBUFFER * drawindexedprim_code;
    PIXO_U32 stream_pitch[PIXO_MAX_STREAMS];
    PIXO_S32 stream_pos_type;
    PIXO_S32 stream_pos_num;
    PIXO_S32 stream_pos_start;
};

struct PIXO_BUF
{
    void * Buffer;
    PIXO_S32 BufferWidth;
    PIXO_S32 BufferHeight;
    PIXO_S32 BufferPitch;
    PIXO_S32 dsty;
    PIXO_S32 dibwidth;
    PIXO_S32 dibheight;
    PIXO_S32 dibpitch;
    PIXO_S32 flags;
    void * dibdc;
    void * buffer;
    void * gdiblitfunc;
    PIXO_S32 scaled;
    PIXO_S32 orig_width;
    PIXO_S32 orig_height;
    void * dibinfo;
    void * dibh;
    void * diboldbitmap;
    void * wnd;
    void * ddraw;
    void * ddsurf;
    void * blitfunc;
    PIXO_S32 surface_fmt;
    void * ddclipper;
    void * rgndata;
    PIXO_S32 bltnumber;
    PIXO_F32 ddtime;
    PIXO_F32 dibtime;
    PIXO_S32 blt_type;
    PIXO_S32 no_bilinear;
    PIXO_U32 bitcount;
    PIXO_U32 redmask;
    PIXO_U32 greenmask;
    PIXO_U32 bluemask;
};

struct PIXO_CPU_FEATURES
{
    PIXO_U32 cpu_type;
    PIXO_U32 features;
    PIXO_U32 cpu_speed;
    PIXO_U32 _1st_level_cache_size;
    PIXO_U32 _2nd_level_cache_size;
    PIXO_U32 _cache_sizes_reported_by_cpu;
    PIXO_U32 _2nd_level_speed;
    PIXO_U32 _main_mem_speed;
    PIXO_U32 mem_speed[PIXO_MEM_SPEED_MAX];
    PIXO_U32 processor_type;
    PIXO_U32 family;
    PIXO_U32 model;
    PIXO_U32 stepping_id;
    PIXO_U32 extended_family;
    PIXO_U32 extended_model;
    PIXO_S8 vendor[13];
    PIXO_S8 processor_name[48];
};

/* bb_rstates.h bindings: */
PIXO_DYNLINK(void) PixoSetWStateZCompare(PIXO_ZCOMPARE state);
PIXO_DYNLINK(PIXO_ZCOMPARE) PixoGetWStateZCompare(void);
PIXO_DYNLINK(void) PixoSetWStateZWrite(PIXO_ZWRITE state);
PIXO_DYNLINK(PIXO_ZWRITE) PixoGetWStateZWrite(void);
PIXO_DYNLINK(void) PixoSetWStateDiffuse(PIXO_DIFFUSE state);
PIXO_DYNLINK(PIXO_DIFFUSE) PixoGetWStateDiffuse(void);
PIXO_DYNLINK(void) PixoSetWStateFog(PIXO_FOG state);
PIXO_DYNLINK(PIXO_FOG) PixoGetWStateFog(void);
PIXO_DYNLINK(void) PixoSetWStateSpecular(PIXO_SPECULAR state);
PIXO_DYNLINK(PIXO_SPECULAR) PixoGetWStateSpecular(void);
PIXO_DYNLINK(void) PixoSetWStateSCompare(PIXO_SCOMPARE state);
PIXO_DYNLINK(PIXO_SCOMPARE) PixoGetWStateSCompare(void);
PIXO_DYNLINK(void) PixoSetWStateDot3(PIXO_DOT3 state);
PIXO_DYNLINK(PIXO_DOT3) PixoGetWStateDot3(void);
PIXO_DYNLINK(void) PixoSetWStateMultiplyScale(PIXO_MULTIPLY_SCALE state);
PIXO_DYNLINK(PIXO_MULTIPLY_SCALE) PixoGetWStateMultiplyScale(void);
PIXO_DYNLINK(void) PixoSetWStateFixedModulation(PIXO_FIXED_MODULATION state);
PIXO_DYNLINK(PIXO_FIXED_MODULATION) PixoGetWStateFixedModulation(void);
PIXO_DYNLINK(void) PixoSetWStateDot3Bias(PIXO_DOT3_BIAS state);
PIXO_DYNLINK(PIXO_DOT3_BIAS) PixoGetWStateDot3Bias(void);
PIXO_DYNLINK(void) PixoSetWStateRasterop(PIXO_RASTEROP state);
PIXO_DYNLINK(PIXO_RASTEROP) PixoGetWStateRasterop(void);
PIXO_DYNLINK(void) PixoSetWStateDiffuseAlphaop(PIXO_DIFFUSE_ALPHAOP state);
PIXO_DYNLINK(PIXO_DIFFUSE_ALPHAOP) PixoGetWStateDiffuseAlphaop(void);
PIXO_DYNLINK(void) PixoSetWStateTex1Alphaop(PIXO_TEX1_ALPHAOP state);
PIXO_DYNLINK(PIXO_TEX1_ALPHAOP) PixoGetWStateTex1Alphaop(void);
PIXO_DYNLINK(void) PixoSetWStateACompare(PIXO_ACOMPARE state);
PIXO_DYNLINK(PIXO_ACOMPARE) PixoGetWStateACompare(void);
PIXO_DYNLINK(void) PixoSetWStateTex1Rgbop(PIXO_TEX1_RGBOP state);
PIXO_DYNLINK(PIXO_TEX1_RGBOP) PixoGetWStateTex1Rgbop(void);
PIXO_DYNLINK(void) PixoSetWStateTex1PipelinePosition(PIXO_TEX1_PIPELINE_POSITION state);
PIXO_DYNLINK(PIXO_TEX1_PIPELINE_POSITION) PixoGetWStateTex1PipelinePosition(void);
PIXO_DYNLINK(void) PixoSetWStateZPass(PIXO_ZPASS state);
PIXO_DYNLINK(PIXO_ZPASS) PixoGetWStateZPass(void);
PIXO_DYNLINK(void) PixoSetWStateZFail(PIXO_ZFAIL state);
PIXO_DYNLINK(PIXO_ZFAIL) PixoGetWStateZFail(void);
PIXO_DYNLINK(PIXO_RASTERIZATION_STATE_STRINGS const *) PixoGetRasterizationStateStrings(void);

/* rast1.h bindings: */
PIXO_DYNLINK(PIXO_S32) PixoStartup(PIXO_ZCLIPRANGE cliprange);
PIXO_DYNLINK(void) PixoShutdown(void);
PIXO_DYNLINK(PIXO_ZCLIPRANGE) PixoGetZClipRangeType(void);
PIXO_DYNLINK(PIXO_ARGB0) PixoMakeArgb04fv(PIXO_F32 * v);
PIXO_DYNLINK(PIXO_ARGB0) PixoMakeArgb04f(PIXO_F32 alpha,
                                         PIXO_F32 red,
                                         PIXO_F32 green,
                                         PIXO_F32 blue);
PIXO_DYNLINK(void) PixoBeginTextureSpecification(PIXO_S32 TexNum,
                                                 PIXO_TEX_FORMAT TexFormat,
                                                 PIXO_S32 Width,
                                                 PIXO_S32 Height,
                                                 PIXO_S32 MipLevelCount);
PIXO_DYNLINK(void) PixoSetMipImage(PIXO_S32 TexNum,
                                   PIXO_S32 MipLevelIndex,
                                   void * Pixels);
PIXO_DYNLINK(void) PixoEndTextureSpecification(PIXO_S32 TexNum);
PIXO_DYNLINK(void) PixoSetTexture(PIXO_S32 TexNum,
                                  PIXO_TEX_FORMAT TexFormat,
                                  PIXO_S32 Width,
                                  PIXO_S32 Height,
                                  void * Pixels);
PIXO_DYNLINK(void) PixoGetTextureSpecification(PIXO_S32 TexNum,
                                               PIXO_TEX_FORMAT * TexFormat,
                                               PIXO_S32 * Width,
                                               PIXO_S32 * Height,
                                               PIXO_S32 * MipLevelCount);
PIXO_DYNLINK(void *) PixoGetMipImage(PIXO_S32 TexNum,
                                     PIXO_S32 MipLevelIndex);
PIXO_DYNLINK(void) PixoSetWStateTextureBorder(PIXO_S32 TexNum,
                                              PIXO_TEX_BORDER state);
PIXO_DYNLINK(PIXO_TEX_BORDER) PixoGetWStateTextureBorder(PIXO_S32 TexNum);
PIXO_DYNLINK(void) PixoSetWStateTextureFilter(PIXO_S32 TexNum,
                                              PIXO_TEX_FILTER state);
PIXO_DYNLINK(PIXO_TEX_FILTER) PixoGetWStateTextureFilter(PIXO_S32 TexNum);
PIXO_DYNLINK(void) PixoSetWStateTextureProjected(PIXO_S32 TexNum,
                                                 PIXO_TEX_PROJECTED state);
PIXO_DYNLINK(PIXO_TEX_PROJECTED) PixoGetWStateTextureProjected(PIXO_S32 TexNum);
PIXO_DYNLINK(void) PixoSetTexGen(PIXO_S32 TexNum,
                                 PIXO_TEXGEN state);
PIXO_DYNLINK(PIXO_TEXGEN) PixoGetTexGen(PIXO_S32 TexNum);
PIXO_DYNLINK(void) PixoSetTexCoordIndex(PIXO_S32 TexNum,
                                        PIXO_S32 Index);
PIXO_DYNLINK(PIXO_S32) PixoGetTexCoordIndex(PIXO_S32 TexNum);
PIXO_DYNLINK(void) PixoSetMipMapLodBias(PIXO_S32 TexNum,
                                        PIXO_F32 Bias);
PIXO_DYNLINK(PIXO_F32) PixoGetMipMapLodBias(PIXO_S32 TexNum);
PIXO_DYNLINK(void) PixoSetVertexCallback(PIXO_VERTEXCALLBACK * pfn,
                                         PIXO_U32 flags);
PIXO_DYNLINK(PIXO_VERTEXCALLBACK *) PixoGetVertexCallback(PIXO_U32 * flags);
PIXO_DYNLINK(void) PixoBeginPrimitive(PIXO_PRIMITIVETYPES Primitive);
PIXO_DYNLINK(void) PixoEndPrimitive(void);
PIXO_DYNLINK(void) PixoDiffuse1up(PIXO_ARGB0 argb0);
PIXO_DYNLINK(void) PixoSpecular1up(PIXO_ARGB0 argb0);
PIXO_DYNLINK(void) PixoTexCoord2f(PIXO_S32 TexNum,
                                  PIXO_F32 s,
                                  PIXO_F32 t);
PIXO_DYNLINK(void) PixoTexCoord3f(PIXO_S32 TexNum,
                                  PIXO_F32 s,
                                  PIXO_F32 t,
                                  PIXO_F32 r);
PIXO_DYNLINK(void) PixoTexCoord4f(PIXO_S32 TexNum,
                                  PIXO_F32 s,
                                  PIXO_F32 t,
                                  PIXO_F32 r,
                                  PIXO_F32 q);
PIXO_DYNLINK(void) PixoTexCoord2fv(PIXO_S32 TexNum,
                                   PIXO_F32 const * v);
PIXO_DYNLINK(void) PixoTexCoord3fv(PIXO_S32 TexNum,
                                   PIXO_F32 const * v);
PIXO_DYNLINK(void) PixoTexCoord4fv(PIXO_S32 TexNum,
                                   PIXO_F32 const * v);
PIXO_DYNLINK(void) PixoVertex2fv(PIXO_F32 const * v);
PIXO_DYNLINK(void) PixoVertex3fv(PIXO_F32 const * v);
PIXO_DYNLINK(void) PixoVertex4fv(PIXO_F32 const * v);
PIXO_DYNLINK(void) PixoVertex2f(PIXO_F32 x,
                                PIXO_F32 y);
PIXO_DYNLINK(void) PixoVertex3f(PIXO_F32 x,
                                PIXO_F32 y,
                                PIXO_F32 z);
PIXO_DYNLINK(void) PixoVertex4f(PIXO_F32 x,
                                PIXO_F32 y,
                                PIXO_F32 z,
                                PIXO_F32 w);
PIXO_DYNLINK(void) PixoVertexScreen2fv(PIXO_F32 const * v);
PIXO_DYNLINK(void) PixoVertexScreen3fv(PIXO_F32 const * v);
PIXO_DYNLINK(void) PixoVertexScreen2f(PIXO_F32 x,
                                      PIXO_F32 y);
PIXO_DYNLINK(void) PixoVertexScreen3f(PIXO_F32 x,
                                      PIXO_F32 y,
                                      PIXO_F32 z);
PIXO_DYNLINK(void) PixoVertexCachedElement(PIXO_S32 index);
PIXO_DYNLINK(PIXO_ARGB0) PixoGetVertexDiffuse1up(void);
PIXO_DYNLINK(PIXO_ARGB0) PixoGetVertexSpecular1up(void);
PIXO_DYNLINK(void) PixoGetVertexTexCoord2f(PIXO_S32 TexNum,
                                           PIXO_F32 * s,
                                           PIXO_F32 * t);
PIXO_DYNLINK(void) PixoGetVertexTexCoord3f(PIXO_S32 TexNum,
                                           PIXO_F32 * s,
                                           PIXO_F32 * t,
                                           PIXO_F32 * q);
PIXO_DYNLINK(void) PixoGetVertexTexCoord4f(PIXO_S32 TexNum,
                                           PIXO_F32 * s,
                                           PIXO_F32 * t,
                                           PIXO_F32 * r,
                                           PIXO_F32 * q);
PIXO_DYNLINK(void) PixoNormal3fv(PIXO_F32 const * v);
PIXO_DYNLINK(void) PixoNormal3f(PIXO_F32 x,
                                PIXO_F32 y,
                                PIXO_F32 z);
PIXO_DYNLINK(void) PixoGetNormal3f(PIXO_F32 * x,
                                   PIXO_F32 * y,
                                   PIXO_F32 * z);
PIXO_DYNLINK(void) PixoDebugLine3f(PIXO_F32 x0,
                                   PIXO_F32 y0,
                                   PIXO_F32 z0,
                                   PIXO_F32 x1,
                                   PIXO_F32 y1,
                                   PIXO_F32 z1,
                                   PIXO_ARGB0 argb0);
PIXO_DYNLINK(void) PixoDrawText3f(PIXO_F32 x,
                                  PIXO_F32 y,
                                  PIXO_F32 z,
                                  PIXO_F32 size,
                                  PIXO_ARGB0 textcolor,
                                  PIXO_ARGB0 backcolor,
                                  PIXO_S8 const * str,
                                  PIXO_S32 len);
PIXO_DYNLINK(void) PixoSetStreamDefinition(PIXO_U32 const * StreamDefinition);
PIXO_DYNLINK(void) PixoSetStream(PIXO_S32 Stream,
                                 void const * StreamData);
PIXO_DYNLINK(void *) PixoGetStream(PIXO_S32 Stream);
PIXO_DYNLINK(void) PixoDrawPrimitiveStream(PIXO_PRIMITIVETYPES Primitive,
                                           PIXO_S32 VertexCount);
PIXO_DYNLINK(PIXO_VERT_CACHE *) PixoSetVertexCache(PIXO_VERT_CACHE * VertCache,
                                                   PIXO_S32 count);
PIXO_DYNLINK(PIXO_VERT_CACHE *) PixoGetVertexCache(PIXO_S32 * count);
PIXO_DYNLINK(PIXO_S32) PixoTransformVertices(PIXO_VERT_CACHE * VertCache,
                                             PIXO_S32 VertexStart,
                                             PIXO_S32 VertexCount);
PIXO_DYNLINK(void) PixoIndexedDrawPrimitiveStream(PIXO_PRIMITIVETYPES Primitive,
                                                  PIXO_U16 const * Indices,
                                                  PIXO_S32 IndexCount);
PIXO_DYNLINK(void) PixoDrawPointspriteList(PIXO_VERT_CACHE * VertCache,
                                           PIXO_S32 VertexCount);
PIXO_DYNLINK(PIXO_CODEBUFFER *) PixoAllocCodeBuffer(PIXO_U32 bytes);
PIXO_DYNLINK(void) PixoFreeCodeBuffer(PIXO_CODEBUFFER * buf);
PIXO_DYNLINK(PIXO_U32) PixoGetMaximumStateCodeBufferSize(void);
PIXO_DYNLINK(PIXO_STATECODEBUFFER *) PixoBuildStateCodeBuffer(PIXO_CODEBUFFER * dest,
                                                              PIXO_PIPELINE_STATE_FLAGS * flags,
                                                              PIXO_U32 * bytes_used);
PIXO_DYNLINK(void) PixoSetStateCodeBuffer(PIXO_STATECODEBUFFER * code_buf);
PIXO_DYNLINK(PIXO_U32) PixoGetMaximumStreamDefCodeBufferSize(void);
PIXO_DYNLINK(PIXO_STREAMDEFCODEBUFFER *) PixoBuildStreamDefCodeBuffer(PIXO_CODEBUFFER * dest,
                                                                      PIXO_U32 const * StreamDefinition,
                                                                      PIXO_U32 * bytes_used);
PIXO_DYNLINK(void) PixoSetStreamDefCodeBuffer(PIXO_STREAMDEFCODEBUFFER * code_buf);
PIXO_DYNLINK(PIXO_STREAMDEFCODEBUFFER *) PixoGetStreamDefCodeBuffer(void);
PIXO_DYNLINK(PIXO_STREAMDEFCODEBUFFER **) PixoBuildStreamDefCodeBufferArray(PIXO_U32 const ** StreamDefinition,
                                                                            PIXO_U32 count,
                                                                            PIXO_U32 * bytes_used);
PIXO_DYNLINK(void) PixoSetViewport(PIXO_S32 x,
                                   PIXO_S32 y,
                                   PIXO_S32 width,
                                   PIXO_S32 height);
PIXO_DYNLINK(void) PixoGetViewport(PIXO_S32 * x,
                                   PIXO_S32 * y,
                                   PIXO_S32 * width,
                                   PIXO_S32 * height);
PIXO_DYNLINK(void) PixoSetCullingPolarityToggle(PIXO_CULLINGPOLARITYTOGGLE mode);
PIXO_DYNLINK(PIXO_CULLINGPOLARITYTOGGLE) PixoGetCullingPolarityToggle(void);
PIXO_DYNLINK(void) PixoSetWorldTransformMatrix(PIXO_F32 const * mat);
PIXO_DYNLINK(void) PixoSetViewTransformMatrix(PIXO_F32 const * mat);
PIXO_DYNLINK(void) PixoSetProjectionTransformMatrix(PIXO_F32 const * mat);
PIXO_DYNLINK(PIXO_F32 const *) PixoGetWorldTransformMatrix(void);
PIXO_DYNLINK(PIXO_F32 const *) PixoGetViewTransformMatrix(void);
PIXO_DYNLINK(PIXO_F32 const *) PixoGetProjectionTransformMatrix(void);
PIXO_DYNLINK(PIXO_F32 const *) PixoGetConcatenatedTransformMatrix(void);
PIXO_DYNLINK(PIXO_F32 const *) PixoGetConcatenatedTransformMatrixInverse(void);
PIXO_DYNLINK(PIXO_F32 *) PixoMatrixMultiply4x4(PIXO_F32 * mat,
                                               PIXO_F32 const * a,
                                               PIXO_F32 const * b);
PIXO_DYNLINK(void) PixoSetZClipRange(PIXO_F32 zmin,
                                     PIXO_F32 zmax);
PIXO_DYNLINK(void) PixoGetZClipRange(PIXO_F32 * zmin,
                                     PIXO_F32 * zmax);
PIXO_DYNLINK(void) PixoSetTextureTransformMatrix(PIXO_S32 TexNum,
                                                 PIXO_F32 const * mat);
PIXO_DYNLINK(PIXO_F32 const *) PixoGetTextureTransformMatrix(PIXO_S32 TexNum);
PIXO_DYNLINK(void) PixoZEnable(void);
PIXO_DYNLINK(void) PixoZDisable(void);
PIXO_DYNLINK(PIXO_S32) PixoIsZEnabled(void);
PIXO_DYNLINK(void) PixoBlendEnable(void);
PIXO_DYNLINK(void) PixoBlendDisable(void);
PIXO_DYNLINK(PIXO_S32) PixoIsBlendEnabled(void);
PIXO_DYNLINK(void) PixoATestEnable(void);
PIXO_DYNLINK(void) PixoATestDisable(void);
PIXO_DYNLINK(PIXO_S32) PixoIsATestEnabled(void);
PIXO_DYNLINK(void) PixoSetVertexDistanceFog(PIXO_VERTEXDISTANCEFOG mode,
                                            PIXO_F32 FogStart,
                                            PIXO_F32 FogEnd,
                                            PIXO_VERTEXDISTANCEFOGTYPE Type);
PIXO_DYNLINK(PIXO_VERTEXDISTANCEFOG) PixoGetVertexDistanceFog(PIXO_F32 * FogStart,
                                                              PIXO_F32 * FogEnd,
                                                              PIXO_VERTEXDISTANCEFOGTYPE * Type);
PIXO_DYNLINK(void) PixoSetFastZSPretest(PIXO_FASTZSPRETEST mode);
PIXO_DYNLINK(PIXO_FASTZSPRETEST) PixoGetFastZSPretest(void);
PIXO_DYNLINK(void) PixoSetDepthRange(PIXO_F32 ZNear,
                                     PIXO_F32 ZFar);
PIXO_DYNLINK(void) PixoGetDepthRange(PIXO_F32 * ZNear,
                                     PIXO_F32 * ZFar);
PIXO_DYNLINK(void) PixoSetPolygonZBias(PIXO_F32 ZBias);
PIXO_DYNLINK(PIXO_F32) PixoGetPolygonZBias(void);
PIXO_DYNLINK(void) PixoSetNormals(PIXO_NORMALS mode);
PIXO_DYNLINK(PIXO_NORMALS) PixoGetNormals(void);
PIXO_DYNLINK(void) PixoSetDot3LightVector(PIXO_F32 x,
                                          PIXO_F32 y,
                                          PIXO_F32 z);
PIXO_DYNLINK(void) PixoGetDot3LightVector(PIXO_F32 * x,
                                          PIXO_F32 * y,
                                          PIXO_F32 * z);
PIXO_DYNLINK(void) PixoSetModulationFactor(PIXO_F32 a,
                                           PIXO_F32 r,
                                           PIXO_F32 g,
                                           PIXO_F32 b);
PIXO_DYNLINK(void) PixoGetModulationFactor(PIXO_F32 * a,
                                           PIXO_F32 * r,
                                           PIXO_F32 * g,
                                           PIXO_F32 * b);
PIXO_DYNLINK(void) PixoSetAlphaRefValue(PIXO_S32 Alpharef);
PIXO_DYNLINK(PIXO_S32) PixoGetAlphaRefValue(void);
PIXO_DYNLINK(void) PixoSetUserRasteropCode(void * pCode,
                                           PIXO_S32 Len);
PIXO_DYNLINK(void) PixoGetUserRasteropCode(void ** ppCode,
                                           PIXO_S32 * pLen);
PIXO_DYNLINK(void) PixoSetCullMode(PIXO_CULL mode);
PIXO_DYNLINK(PIXO_CULL) PixoGetCullMode(void);
PIXO_DYNLINK(void) PixoSetWStateAlphatest(PIXO_ALPHATEST state);
PIXO_DYNLINK(PIXO_ALPHATEST) PixoGetWStateAlphatest(void);
PIXO_DYNLINK(void) PixoSetBottomUpRendering(PIXO_BOTTOMUPRENDERING mode);
PIXO_DYNLINK(PIXO_BOTTOMUPRENDERING) PixoGetBottomUpRendering(void);
PIXO_DYNLINK(void) PixoSetWireFrame(PIXO_WIREFRAME mode,
                                    PIXO_ARGB0 color);
PIXO_DYNLINK(PIXO_WIREFRAME) PixoGetWireFrame(PIXO_ARGB0 * color);
PIXO_DYNLINK(void) PixoSetPolygonMipMap(PIXO_POLYGONMIPMAP mode);
PIXO_DYNLINK(PIXO_POLYGONMIPMAP) PixoGetPolygonMipMap(void);
PIXO_DYNLINK(void) PixoSetPointSize(PIXO_F32 size);
PIXO_DYNLINK(PIXO_F32) PixoGetPointSize(void);
PIXO_DYNLINK(void) PixoSetMaxPointSize(PIXO_F32 size);
PIXO_DYNLINK(PIXO_F32) PixoGetMaxPointSize(void);
PIXO_DYNLINK(void) PixoSetScalePointSizeByRHW(PIXO_SCALEPOINTSIZEBYRHW mode);
PIXO_DYNLINK(PIXO_SCALEPOINTSIZEBYRHW) PixoGetScalePointSizeByRHW(void);
PIXO_DYNLINK(void) PixoSetFogColor(PIXO_ARGB0 color);
PIXO_DYNLINK(PIXO_ARGB0) PixoGetFogColor(void);
PIXO_DYNLINK(void) PixoSetWStatePixelCountAddress(PIXO_S32 * PixelCountAddr);
PIXO_DYNLINK(PIXO_S32 *) PixoGetWStatePixelCountAddress(void);
PIXO_DYNLINK(void) PixoSetStencilRef(PIXO_U32 val);
PIXO_DYNLINK(PIXO_U32) PixoGetStencilRef(void);
PIXO_DYNLINK(void) PixoSetStencilValZPass(PIXO_U32 val);
PIXO_DYNLINK(PIXO_U32) PixoGetStencilValZPass(void);
PIXO_DYNLINK(void) PixoSetStencilValZFail(PIXO_U32 val);
PIXO_DYNLINK(PIXO_U32) PixoGetStencilValZFail(void);
PIXO_DYNLINK(void) PixoSetStencilValZOnOrientation(PIXO_S32 val);
PIXO_DYNLINK(PIXO_S32) PixoGetStencilValZOnOrientation(void);
PIXO_DYNLINK(PIXO_RASTERIZATION_PIPELINE) PixoGetRasterizationPipeline(void);
PIXO_DYNLINK(PIXO_RASTERIZATION_PIPELINE) PixoSelectRasterizationPipeline(PIXO_RASTERIZATION_PIPELINE mode);
PIXO_DYNLINK(void) PixoSetTriangleStripEarlyDegenerateCheck(PIXO_TRIANGLESTRIPEARLYDEGENERATECHECK mode);
PIXO_DYNLINK(PIXO_TRIANGLESTRIPEARLYDEGENERATECHECK) PixoGetTriangleStripEarlyDegenerateCheck(void);
PIXO_DYNLINK(void) PixoSetTriangleCallback(PIXO_DRAWTRIANGLECALLBACK * pfn);
PIXO_DYNLINK(PIXO_DRAWTRIANGLECALLBACK *) PixoGetTriangleCallback(void);
PIXO_DYNLINK(void) PixoSetTexPaletteTableEntry(PIXO_S32 TexNum,
                                               PIXO_S32 entry,
                                               PIXO_ARGB0 * colors,
                                               PIXO_S32 count_bytes);
PIXO_DYNLINK(PIXO_ARGB0 *) PixoGetTexPaletteTable(PIXO_S32 TexNum,
                                                  PIXO_S32 entry);
PIXO_DYNLINK(PIXO_PIPELINE_STATE_FLAGS *) PixoGetRasterizationStateFlags(void);
PIXO_DYNLINK(void) PixoSetZBuffer(void * ZBuffer,
                                  PIXO_U32 Pitch,
                                  PIXO_ZBUFFER_TYPE ZBufferType);
PIXO_DYNLINK(void *) PixoGetZBuffer(PIXO_U32 * Pitch,
                                    PIXO_ZBUFFER_TYPE * ZBufferType);
PIXO_DYNLINK(void) PixoSetZBufferType(PIXO_ZBUFFER_TYPE ZBufferType);
PIXO_DYNLINK(PIXO_ZBUFFER_TYPE) PixoGetZBufferType(void);
PIXO_DYNLINK(void) PixoSetFrameBuffer(void * FrameBuffer,
                                      PIXO_U32 Pitch);
PIXO_DYNLINK(void *) PixoGetFrameBuffer(PIXO_U32 * Pitch);
PIXO_DYNLINK(void) PixoClearRect(PIXO_S32 flags,
                                 PIXO_S32 x,
                                 PIXO_S32 y,
                                 PIXO_S32 w,
                                 PIXO_S32 h,
                                 PIXO_ARGB0 Color,
                                 PIXO_F32 Z,
                                 PIXO_U32 Stencil);
PIXO_DYNLINK(void) PixoClearViewport(PIXO_S32 flags,
                                     PIXO_ARGB0 Color,
                                     PIXO_F32 Z,
                                     PIXO_U32 Stencil);
PIXO_DYNLINK(void) PixoDrawTextSetCharRange(PIXO_U32 startchar,
                                            PIXO_U32 endchar);
PIXO_DYNLINK(PIXO_U32) PixoDrawText(void * dest,
                                    PIXO_S32 dx,
                                    PIXO_S32 dy,
                                    PIXO_S32 dwidth,
                                    PIXO_S32 dheight,
                                    PIXO_S32 dpitch,
                                    PIXO_ARGB0 textcolor,
                                    PIXO_ARGB0 backcolor,
                                    PIXO_S8 const * str,
                                    PIXO_S32 len);
PIXO_DYNLINK(PIXO_U32) PixoGetTextHeight(void);
PIXO_DYNLINK(PIXO_U32) PixoGetTextWidth(PIXO_S8 const * str,
                                        PIXO_S32 len);
PIXO_DYNLINK(void) PixoBufferFill16(void * dest,
                                    PIXO_S32 x,
                                    PIXO_S32 y,
                                    PIXO_S32 w,
                                    PIXO_S32 h,
                                    PIXO_S32 pitch,
                                    PIXO_U16 val);
PIXO_DYNLINK(void) PixoBufferFill32(void * dest,
                                    PIXO_S32 x,
                                    PIXO_S32 y,
                                    PIXO_S32 w,
                                    PIXO_S32 h,
                                    PIXO_S32 pitch,
                                    PIXO_U32 val);
PIXO_DYNLINK(void) PixoMemSet32(void * dst,
                                PIXO_U32 val,
                                PIXO_S32 count_bytes);
PIXO_DYNLINK(void) PixoMemSet16(void * dst,
                                PIXO_U16 val,
                                PIXO_S32 count_bytes);
PIXO_DYNLINK(void) PixoBilinear2xStretchARGB8888(void * dest,
                                                 PIXO_S32 dx,
                                                 PIXO_S32 dy,
                                                 PIXO_S32 dpitch,
                                                 void * src,
                                                 PIXO_S32 sx,
                                                 PIXO_S32 sy,
                                                 PIXO_S32 sw,
                                                 PIXO_S32 sh,
                                                 PIXO_S32 spitch);
PIXO_DYNLINK(void) Pixo2xStretchARGB8888(void * dest,
                                         PIXO_S32 dx,
                                         PIXO_S32 dy,
                                         PIXO_S32 dpitch,
                                         void * src,
                                         PIXO_S32 sx,
                                         PIXO_S32 sy,
                                         PIXO_S32 sw,
                                         PIXO_S32 sh,
                                         PIXO_S32 spitch);
PIXO_DYNLINK(void) PixoBilinear2xStretchVerticalARGB8888(void * dest,
                                                         PIXO_S32 dx,
                                                         PIXO_S32 dy,
                                                         PIXO_S32 dpitch,
                                                         void * src,
                                                         PIXO_S32 sx,
                                                         PIXO_S32 sy,
                                                         PIXO_S32 sw,
                                                         PIXO_S32 sh,
                                                         PIXO_S32 spitch);
PIXO_DYNLINK(void) Pixo2xStretchVerticalARGB8888(void * dest,
                                                 PIXO_S32 dx,
                                                 PIXO_S32 dy,
                                                 PIXO_S32 dpitch,
                                                 void * src,
                                                 PIXO_S32 sx,
                                                 PIXO_S32 sy,
                                                 PIXO_S32 sw,
                                                 PIXO_S32 sh,
                                                 PIXO_S32 spitch);
PIXO_DYNLINK(void) PixoBilinear2xShrinkARGB8888(void * dest,
                                                PIXO_S32 dx,
                                                PIXO_S32 dy,
                                                PIXO_S32 dpitch,
                                                void * src,
                                                PIXO_S32 sx,
                                                PIXO_S32 sy,
                                                PIXO_S32 sw,
                                                PIXO_S32 sh,
                                                PIXO_S32 spitch);
PIXO_DYNLINK(void) PixoBlitARGB8888toARGB8888(void * dest,
                                              PIXO_S32 dx,
                                              PIXO_S32 dy,
                                              PIXO_S32 dpitch,
                                              void * src,
                                              PIXO_S32 sx,
                                              PIXO_S32 sy,
                                              PIXO_S32 sw,
                                              PIXO_S32 sh,
                                              PIXO_S32 spitch);
PIXO_DYNLINK(void) PixoBlitARGB8888toRGB888(void * dest,
                                            PIXO_S32 dx,
                                            PIXO_S32 dy,
                                            PIXO_S32 dpitch,
                                            void * src,
                                            PIXO_S32 sx,
                                            PIXO_S32 sy,
                                            PIXO_S32 sw,
                                            PIXO_S32 sh,
                                            PIXO_S32 spitch);
PIXO_DYNLINK(void) PixoBlitARGB8888toRGB565(void * dest,
                                            PIXO_S32 dx,
                                            PIXO_S32 dy,
                                            PIXO_S32 dpitch,
                                            void * src,
                                            PIXO_S32 sx,
                                            PIXO_S32 sy,
                                            PIXO_S32 sw,
                                            PIXO_S32 sh,
                                            PIXO_S32 spitch);
PIXO_DYNLINK(void) PixoBlitARGB8888toXRGB1555(void * dest,
                                              PIXO_S32 dx,
                                              PIXO_S32 dy,
                                              PIXO_S32 dpitch,
                                              void * src,
                                              PIXO_S32 sx,
                                              PIXO_S32 sy,
                                              PIXO_S32 sw,
                                              PIXO_S32 sh,
                                              PIXO_S32 spitch);
PIXO_DYNLINK(void) PixoAlphaBlitARGB8888(void * dest,
                                         PIXO_S32 dx,
                                         PIXO_S32 dy,
                                         PIXO_S32 dpitch,
                                         void * src,
                                         PIXO_S32 sx,
                                         PIXO_S32 sy,
                                         PIXO_S32 sw,
                                         PIXO_S32 sh,
                                         PIXO_S32 spitch);
PIXO_DYNLINK(void) PixoAlphaBlitRGB8888(void * dest,
                                        PIXO_S32 dx,
                                        PIXO_S32 dy,
                                        PIXO_S32 dpitch,
                                        void * src,
                                        PIXO_S32 sx,
                                        PIXO_S32 sy,
                                        PIXO_S32 sw,
                                        PIXO_S32 sh,
                                        PIXO_S32 spitch,
                                        PIXO_F32 alpha);
PIXO_DYNLINK(void) PixoAlphaFillARGB8888(void * dest,
                                         PIXO_S32 dx,
                                         PIXO_S32 dy,
                                         PIXO_S32 dw,
                                         PIXO_S32 dh,
                                         PIXO_S32 dpitch,
                                         PIXO_ARGB0 argb0);
PIXO_DYNLINK(void) PixoStretchBlitARGB8888(void * dest,
                                           PIXO_S32 dx,
                                           PIXO_S32 dy,
                                           PIXO_S32 dw,
                                           PIXO_S32 dh,
                                           PIXO_S32 dpitch,
                                           void * src,
                                           PIXO_S32 sx,
                                           PIXO_S32 sy,
                                           PIXO_S32 sw,
                                           PIXO_S32 sh,
                                           PIXO_S32 spitch);
PIXO_DYNLINK(void) PixoBilinearStretchBlitARGB8888(void * dest,
                                                   PIXO_S32 dx,
                                                   PIXO_S32 dy,
                                                   PIXO_S32 dw,
                                                   PIXO_S32 dh,
                                                   PIXO_S32 dpitch,
                                                   void * src,
                                                   PIXO_S32 sx,
                                                   PIXO_S32 sy,
                                                   PIXO_S32 sw,
                                                   PIXO_S32 sh,
                                                   PIXO_S32 spitch);
PIXO_DYNLINK(void) PixoSetStretchBlitARGB8888Code(void * pCode,
                                                  PIXO_S32 Len);
PIXO_DYNLINK(void) PixoGetStretchBlitARGB8888Code(void ** ppCode,
                                                  PIXO_S32 * pLen);
PIXO_DYNLINK(void) PixoConvertS3TC1ToARGB8888(void * dest,
                                              void * src,
                                              PIXO_U32 width,
                                              PIXO_U32 height);
PIXO_DYNLINK(void) PixoConvertS3TC2or3ToARGB8888(void * dest,
                                                 void * src,
                                                 PIXO_U32 width,
                                                 PIXO_U32 height);
PIXO_DYNLINK(void) PixoConvertS3TC4or5ToARGB8888(void * dest,
                                                 void * src,
                                                 PIXO_U32 width,
                                                 PIXO_U32 height);

/* bb_buf.h bindings: */
PIXO_DYNLINK(PIXO_BUF * ) PixoBufferOpen(void * wnd,
                                         PIXO_S32 width,
                                         PIXO_S32 height,
                                         PIXO_S32 flags);
PIXO_DYNLINK(void) PixoBufferClose(PIXO_BUF * pbuf);
PIXO_DYNLINK(PIXO_S32) PixoBufferLock(PIXO_BUF * pbuf,
                                      PIXO_S32 flags);
PIXO_DYNLINK(void) PixoBufferUnlock(PIXO_BUF * pbuf);
PIXO_DYNLINK(void) PixoBufferBlit(PIXO_BUF * pbuf,
                                  void * hdc,
                                  PIXO_S32 dx,
                                  PIXO_S32 dy);
PIXO_DYNLINK(void) PixoBufferBlitRect(PIXO_BUF * pbuf,
                                      void * hdc,
                                      PIXO_S32 dx,
                                      PIXO_S32 dy,
                                      PIXO_S32 sx,
                                      PIXO_S32 sy,
                                      PIXO_S32 sw,
                                      PIXO_S32 sh);
PIXO_DYNLINK(PIXO_S32) PixoTimeBufferBlit(PIXO_BUF * pbuf,
                                          void * hdc,
                                          PIXO_S32 x,
                                          PIXO_S32 y);
PIXO_DYNLINK(PIXO_S32) PixoSetDirectDraw(void * lpDirectDraw,
                                         void * lpPrimary);

/* cpuid.h bindings: */
PIXO_DYNLINK(PIXO_U32) PixoGetCPUIDFeatures(PIXO_CPU_FEATURES * cpu_features,
                                            PIXO_U32 flags);
#ifdef __cplusplus
}
#endif

#endif // _PIXOMATIC_H_

