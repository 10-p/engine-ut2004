#ifndef _MEPRECISION_H /* -*- mode: C; -*- */
#define _MEPRECISION_H

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $
   Date: $Date: 2002/10/11 14:58:41 $ - Revision: $Revision: 1.86.2.12.4.1 $

   This software and its accompanying manuals have been developed
   by MathEngine PLC ("MathEngine") and the copyright and all other
   intellectual property rights in them belong to MathEngine. All
   rights conferred by law (including rights under international
   copyright conventions) are reserved to MathEngine. This software
   may also incorporate information which is confidential to
   MathEngine.

   Save to the extent permitted by law, or as otherwise expressly
   permitted by MathEngine, this software and the manuals must not
   be copied (in whole or in part), re-arranged, altered or adapted
   in any way without the prior written consent of the Company. In
   addition, the information contained in the software may not be
   disseminated without the prior written consent of MathEngine.

 */

/** @file
 * Precision and platform dependent globals
 */

#include <math.h>
#include <float.h>
#include <MeCall.h>

/*
  This header is intended to manage different precisions on different
  platforms, for different products.

  Simply comment in or out the following lines to switch between
  precisions. This only has an effect for platforms on which you get a
  choice, namely Windows and Linux. The default is single precision for
  PlayStation2, Windows and Linux, and double precision for Irix.

  Note that changing the precision used for compilation requires that
  you link with an appropriate set of Toolkit libraries that use the
  specified precision as well. This will require that you receive and
  install the alternate precision set of libraries from MathEngine.
  It will also require that you link against this alternate set of
  libraries. This may require that you change your library path
  manually in a Visual Studio project file (Visual Studio users) or
  that you change appropriate environment settings (for command-line
  builds).

*/

#if __GNUC__
    /*
        DANGER! This will only work on PS2 for _initialized_ _global_ or
        _static_ variables. It won't work for automatic ones, or for non
        initialized ones. See the discussion in:

            https://WWW.devNet.SCEA.com/download/technotes/ps2faq.html

        under "ALIGNMENT".
    */
#   define MeALIGNDATA(T,V,N)   T V __attribute__ ((aligned(N)))
#endif

/*
    This requires at least Visual C 6.0 with SP5 and the Processor Pack
*/
#if _MSC_VER
#   ifndef _MSC_FULL_VER
#       error '__declspec(align(N))' not supported by older Visual C versions
#   else
#       define MeALIGNDATA(T,V,N) __declspec(align(N)) T V
#   endif
#endif

/*
    CodeWarrior does support GNU C compatible '__attribute__',
    even if it is almost undocumented and one should use really
    '#pragma align(N)' instead; but the latter does not have the
    right behaviour for us.
*/
#if __MWERKS__
#   if (defined PS2 || defined NGC)
#       define MeALIGNDATA(T,V,N) T V __attribute__ ((aligned(N)))
#   else
#       error '__attribute__' not supported by this CodeWarrior platform
#   endif
#endif

#ifndef MeALIGNDATA
#   error 'MeALIGNDATA(T,V,N)' is undefined for this platform
#endif

#if 0
#   ifndef _ME_API_DOUBLE
#       define _ME_API_DOUBLE 1
#   endif
#endif

/*
    Explicit boolean type:
        0 means false, anything else means true.
*/

typedef int                     MeBool;
#define MEFALSE                 (0)

/*
    Unsigned integral type the same size as a pointer, so we can do bitwise
    operations on it.
     NB stdint doesn't seem to exist on Windows, so we'll have to do something
        a bit more careful for WIN64. Bug #1836.
*/
#if defined PS2
#ifndef __MWERKS__
#   include <eetypes.h>
#   define MeUintPtr            u_int
#else
#   define MeUintPtr            unsigned int
#endif

#elif defined NGC
#ifndef __MWERKS__
#   include <sys/types.h>
#endif
#   include <stddef.h>
#   define MeUintPtr            MeU32

#elif defined TRIMEDIA
    /* ToDo: Is this right? */
#   include <stddef.h>
#   define MeUintPtr            ptrdiff_t

#elif defined WIN32
#   define MeUintPtr            MeU32

#elif defined LINUX 
#   include <stdint.h>
#   define MeUintPtr            uintptr_t

#elif defined MACOSX
#   include <stdint.h>
#   define MeUintPtr            uintptr_t

#elif defined IRIX
#   include <inttypes.h>
#   define MeUintPtr            uintptr_t

#endif

/*
    In the following, 'MeALIGNTO' _must_ be defined without
    parenthesis around its value.
*/

#if defined PS2

#   define MeALIGNTO            16

    /** MathEngine floating point number */
    typedef float               MeReal;
    /* ACME renderer/OpenGL specific */
    typedef float               AcmeReal;
#   define MEINFINITY           ((MeReal) FLT_MAX)
#   define APPEND_F_OR_D(ID)    ID##f
#   define APPEND_FV_OR_DV(ID)  ID##fv

#ifndef __MWERKS__
    typedef u_char              MeU8;
    typedef u_short             MeU16;
    typedef u_int               MeU32;
    typedef u_long              MeU64;
    typedef u_long128           MeU128;

    typedef char                MeI8;
    typedef short               MeI16;
    typedef int                 MeI32;
    typedef long                MeI64;
    typedef long128             MeI128;
#else
    typedef char  unsigned      MeU8;
    typedef short unsigned      MeU16;
    typedef int   unsigned      MeU32;
    typedef long  unsigned      MeU64;
    typedef u_long128           MeU128;

    typedef char     signed     MeI8;
    typedef short    signed     MeI16;
    typedef int      signed     MeI32;
    typedef long     signed     MeI64;
    typedef long128             MeI128;
#endif

#elif defined NGC
#   define MeALIGNTO            16
    
    /** MathEngine floating point number */
    typedef float               MeReal;
    /* ACME renderer/OpenGL specific */
    typedef float               AcmeReal;
#   define MEINFINITY           ((MeReal) FLT_MAX)
#   define APPEND_F_OR_D(ID)    ID##f
#   define APPEND_FV_OR_DV(ID)  ID##fv

/* Integer types of explicit length */
    
    typedef char signed         MeI8;
    typedef short signed        MeI16;
    typedef int signed          MeI32;
    typedef long long signed    MeI64;
    typedef struct {
        int signed v[4];
    }                           MeI128;

    typedef char  unsigned      MeU8;
    typedef short unsigned      MeU16;
    typedef int   unsigned      MeU32;
    typedef long long unsigned  MeU64;
    typedef struct{
        int unsigned v[4];
    }                           MeU128;

    
#elif defined TRIMEDIA

#   define MeALIGNTO            4

    /** MathEngine floating point number */
    typedef float               MeReal;
    /* ACME renderer/OpenGL specific */
    typedef float               AcmeReal;
#   define MEINFINITY           ((MeReal) FLT_MAX)
#   define APPEND_F_OR_D(ID)    ID##f
#   define APPEND_FV_OR_DV(ID)  ID##fv

    typedef unsigned char       MeU8;
    typedef unsigned short      MeU16;
    typedef unsigned int        MeU32;
    typedef unsigned long       MeU64;
    typedef MeU32               MeU128[4];

    typedef char                MeI8;
    typedef short               MeI16;
    typedef int                 MeI32;
    typedef long                MeI64;
    typedef MeI32               MeI128[4];

#elif defined WIN32

#   define MeALIGNTO            16

    /*
        On Windows we may want to choose the precision.
        The default is single precision.
    */

#   ifdef _ME_API_DOUBLE
        /** MathEngine floating point number */
        typedef double              MeReal;
        /* ACME renderer/OpenGL specific */
        typedef double              AcmeReal;
#       define MEINFINITY           ((MeReal) DBL_MAX)
#       define APPEND_F_OR_D(ID)    ID##d
#       define APPEND_FV_OR_DV(ID)  ID##dv
#   else
        /** MathEngine floating point number */
        typedef float               MeReal;
        /* ACME renderer/OpenGL specific */
        typedef float               AcmeReal;
#       define MEINFINITY           ((MeReal) FLT_MAX)
#       define APPEND_F_OR_D(ID)    ID##f
#       define APPEND_FV_OR_DV(ID)  ID##fv
#   endif

    typedef unsigned __int8     MeU8;
    typedef unsigned __int16    MeU16;
    typedef unsigned __int32    MeU32;
    typedef unsigned __int64    MeU64;
    typedef struct {
        unsigned __int32 v[4];
    }                           MeU128;

    typedef __int8              MeI8;
    typedef __int16             MeI16;
    typedef __int32             MeI32;
    typedef __int64             MeI64;
    typedef struct {
        __int32 v[4];
    }                           MeI128;

#elif defined LINUX

#   define MeALIGNTO            16

    /*
      On Linux we may want to choose the precision.
      The default is single precision
    */

#   include <sys/types.h>

#   ifdef _ME_API_DOUBLE
        /** MathEngine floating point number */
        typedef double              MeReal;
        /* ACME renderer/OpenGL specific */
        typedef double              AcmeReal;
#       define MEINFINITY           ((MeReal) DBL_MAX)
#       define APPEND_F_OR_D(ID)    ID##d
#       define APPEND_FV_OR_DV(ID)  ID##dv
#   else
        /** MathEngine floating point number */
        typedef float               MeReal;
        /* ACME renderer/OpenGL specific */
        typedef float               AcmeReal;
#       define MEINFINITY           ((MeReal) FLT_MAX)
#       define APPEND_F_OR_D(ID)    ID##f
#       define APPEND_FV_OR_DV(ID)  ID##fv

#   endif

    typedef u_int8_t            MeU8;
    typedef u_int16_t           MeU16;
    typedef u_int32_t           MeU32;
#   ifdef __GNUC__
        typedef u_int64_t       MeU64;
#   else
        typedef struct {
            u_int32_t v[2];
        }                       MeU64;
#   endif
    typedef struct {
        u_int32_t v[4];
    }                           MeU128;

    typedef int8_t              MeI8;
    typedef int16_t             MeI16;
    typedef int32_t             MeI32;
#   ifdef __GNUC__
        typedef int64_t         MeI64;
#   else
        typedef struct {
            int32_t v[2];
        }                       MeI64;
#   endif
    typedef struct {
        int32_t v[4];
    }                           MeI128;

#elif defined MACOSX

#   define MeALIGNTO            16

    /*
      On macos we may want to choose the precision.
      The default is single precision
    */

#   include <sys/types.h>

#   ifdef _ME_API_DOUBLE
        /** MathEngine floating point number */
        typedef double              MeReal;
        /* ACME renderer/OpenGL specific */
        typedef double              AcmeReal;
#       define MEINFINITY           ((MeReal) DBL_MAX)
#       define APPEND_F_OR_D(ID)    ID##d
#       define APPEND_FV_OR_DV(ID)  ID##dv
#   else
        /** MathEngine floating point number */
        typedef float               MeReal;
        /* ACME renderer/OpenGL specific */
        typedef float               AcmeReal;
#       define MEINFINITY           ((MeReal) FLT_MAX)
#       define APPEND_F_OR_D(ID)    ID##f
#       define APPEND_FV_OR_DV(ID)  ID##fv

#   endif

    typedef u_int8_t            MeU8;
    typedef u_int16_t           MeU16;
    typedef u_int32_t           MeU32;
#   ifdef __GNUC__
        typedef u_int64_t       MeU64;
#   else
        typedef struct {
            u_int32_t v[2];
        }                       MeU64;
#   endif
    typedef struct {
        u_int32_t v[4];
    }                           MeU128;

    typedef int8_t              MeI8;
    typedef int16_t             MeI16;
    typedef int32_t             MeI32;
#   ifdef __GNUC__
        typedef int64_t         MeI64;
#   else
        typedef struct {
            int32_t v[2];
        }                       MeI64;
#   endif
    typedef struct {
        int32_t v[4];
    }                           MeI128;

#elif defined IRIX

#   define MeALIGNTO            8

    /* IRIX is always double precision */

    /* we need some way of telling */
#   ifndef _ME_API_DOUBLE
#      define _ME_API_DOUBLE
#   endif

#   include <sys/types.h>

    /** MathEngine floating point number */
    typedef double              MeReal;
    /* For ACME renderer/OpenGL. */
    typedef double              AcmeReal;
#   define MEINFINITY           ((MeReal) DBL_MAX)
#   define APPEND_F_OR_D(ID)    ID##d
#   define APPEND_FV_OR_DV(ID)  ID##dv

    typedef uint8_t             MeU8;
    typedef uint16_t            MeU16;
    typedef uint32_t            MeU32;
    typedef uint64_t            MeU64;
    typedef struct {
        uint32_t v[4];
    }                           MeU128;

    typedef int8_t              MeI8;
    typedef int16_t             MeI16;
    typedef int32_t             MeI32;
    typedef int64_t             MeI64;
    typedef struct {
        int32_t v[4];
    }                           MeI128;

#   ifdef IRIX_O32
#       define true             1
#       define false            0
        typedef unsigned        bool;
#   endif

#elif defined ELATE

#   define MeALIGNTO            16

    /*
      On Elate we may want to choose the precision.
      The default is single precision.
    */

#   include <sys/types.h>

#   ifdef _ME_API_DOUBLE
        /** MathEngine floating point number */
        typedef double              MeReal;
        /* ACME renderer/OpenGL specific */
        typedef double              AcmeReal;
#       define MEINFINITY           ((MeReal) DBL_MAX)
#       define APPEND_F_OR_D(ID)    ID##d
#       define APPEND_FV_OR_DV(ID)  ID##dv
#   else /* _ME_API_DOUBLE */
        /** MathEngine floating point number */
        typedef float               MeReal;
        /* ACME renderer/OpenGL specific */
        typedef float               AcmeReal;
#       define MEINFINITY           ((MeReal) FLT_MAX)
#       define APPEND_F_OR_D(ID)    ID##f
#       define APPEND_FV_OR_DV(ID)  ID##fv
#   endif

    typedef char                    MeI8;
    typedef short                   MeI16;
    typedef int                     MeI32;
    typedef long                    MeI64;
    typedef struct { MeU64 v[4]; }  MeI128;

    typedef unsigned char           MeU8;
    typedef unsigned short          MeU16;
    typedef unsigned int            MeU32;
    typedef unsigned long           MeU64;
    typedef struct { MeI64 v[2]; }  MeU128;

#endif

#ifndef MeRealIsValid
#   ifndef ME_API_DOUBLE
#       define MeRealIsValid(f)     ((*(unsigned *) &(f) & 0x7f800000u) \
                                        != 0x7f800000u)
#   else
#       error MeRealIsValid is not (yet) defined for 'double'
#   endif
#endif

/*
    Vector and matrix typedefs
*/

/** MathEngine 3 long MeReal vector. */
#if defined PS2 || defined NGC
    typedef MeReal              MeVector3[4] /* __attribute__((aligned(16))) */;
#else
    typedef MeReal              MeVector3[3];
#endif

/** MathEngine 3 long AcmeReal vector */

#if defined PS2 || defined NGC
    typedef AcmeReal            AcmeVector3[4] __attribute__((aligned(16)));
#else
    typedef AcmeReal            AcmeVector3[3];
#endif

/** MathEngine 4 long MeReal vector */
#if defined PS2 || defined NGC
    typedef MeReal              MeVector4[4] /* __attribute__((aligned(16))) */;
#else
    typedef MeReal              MeVector4[4];
#endif
    
typedef MeReal                  *MeVector3Ptr;
typedef MeReal                  *MeVector4Ptr;

/** MathEngine 3x3 MeReal matrix */
typedef MeVector3               MeMatrix3[3];
typedef MeVector3               *MeMatrix3Ptr;

/** MathEngine 4x4 MeReal matrix */
typedef MeVector4               MeMatrix4[4];
typedef MeVector4               *MeMatrix4Ptr;

/*
  The following precision, or floating point error tolerance, values are
  expected to depend on the floating-point accuracy (float, double etc.)
  being used, and not specifically on the platform.
*/

#ifdef _ME_API_DOUBLE
    /** Maximum achievable fractional accuracy. */
#   define ME_MIN_EPSILON               (2.8e-17)
    /** "High" fractional accuracy. */
#   define ME_SMALL_EPSILON             (1.0e-14)
    /** "Moderate" fractional accuracy. */
#   define ME_MEDIUM_EPSILON            (1.0e-7)
#else
    /** Maximum achievable fractional accuracy. */
#   define ME_MIN_EPSILON               (1.2e-7f)
    /** "High" fractional accuracy. */
#   define ME_SMALL_EPSILON             (1.0e-6f)
    /** "Moderate" fractional accuracy. */
#   define ME_MEDIUM_EPSILON            (1.0e-3f)
#endif

/*
  Some macros which test whether floating-point numbers are "nearly"
  equal or "close" to zero:
*/
#define ME_IS_ZERO_TOL(x, e)            (MeFabs((x)) < (e))
#define ME_IS_ZERO(x)                   (MeFabs((x)) < ME_SMALL_EPSILON)
#define ME_ARE_EQUAL_TOL(x,y,e)         (ME_IS_ZERO_TOL((y),(e)) \
                                            ? ME_IS_ZERO_TOL((x),(e)) \
                                            : MeFabs(((x)-(y))/(y)) < (e))
#define ME_ARE_EQUAL(x,y)               ME_ARE_EQUAL_TOL((x),(y),ME_SMALL_EPSILON)
/*
  Some extensions to the above macros to handle the comparison of small vectors:
 */
#define ME_IS_ZERO_3VEC_TOL(x,e)        (ME_IS_ZERO_TOL((x)[0],(e)) \
                                            && ME_IS_ZERO_TOL(x[1],(e)) \
                                            && ME_IS_ZERO_TOL(x[2],(e)))
#define ME_IS_ZERO_4VEC_TOL(x,e)        (ME_IS_ZERO_3VEC_TOL((x),(e)) \
                                            && ME_IS_ZERO_TOL((x)[3],(e)))
#define ME_ARE_EQUAL_3VEC_TOL(x,y,e)    (ME_ARE_EQUAL_TOL((x)[0],(y)[0],(e)) \
                                            && ME_ARE_EQUAL_TOL((x)[1],(y)[1],(e)) \
                                            && ME_ARE_EQUAL_TOL((x)[2],(y)[2],(e)))
#define ME_ARE_EQUAL_4VEC_TOL(x,y,e)    (ME_ARE_EQUAL_3VEC_TOL((x),(y),(e)) \
                                            && ME_ARE_EQUAL_TOL((x)[3],(y)[3],(e)))
#define ME_IS_ZERO_3VEC(x)              ME_IS_ZERO_3VEC_TOL((x),ME_SMALL_EPSILON)
#define ME_IS_ZERO_4VEC(x)              ME_IS_ZERO_4VEC_TOL((x),ME_SMALL_EPSILON)
#define ME_ARE_EQUAL_3VEC(x,y)          ME_ARE_EQUAL_3VEC_TOL((x),(y),ME_SMALL_EPSILON)
#define ME_ARE_EQUAL_4VEC(x,y)          ME_ARE_EQUAL_4VEC_TOL((x),(y),ME_SMALL_EPSILON)

/*
  Generic maths functions and defines
*/

#define MeSqr(x)                ((x)*(x))

#if defined PS2

#   define MeSqrt               MEEEsqrt
#   define MeSin                sinf
#   define MeAsin               asinf
#   define MeCos                cosf
#   define MeAcos               acosf
#   define MeTan                tanf
#   define MeAtan               atanf
#   define MeAtan2              atan2f
#   define MeFabs               MEEEfabs
#   define MeRecip(x)           ((MeReal)(1.0)/(x))
#   define MeRecipSqrt          MEEErsqrt
#   define ME_PI                (3.14159265358979323846f)

   /* These definitions are valid on any MIPS IV compliant
      architecture. */

    static inline float MEEEsqrt(float x)
    {
        float rc;
        __asm__ __volatile__("    sqrt.s    %0,%1"
            : "=f" (rc) : "f" (x) );
        return rc;
    }

    static inline float MEEErsqrt(float x)
    {
        float rc;
        __asm__ __volatile__("    rsqrt.s    %0,%1,%2"
            : "=f"(rc) : "f"(1.0f),"f"(x));
        return rc;
    }

    static inline float MEEEfabs(float n)
    {
        float rc;
        __asm__ __volatile__("    abs.s    %0,%1"
            : "=f"(rc) : "f"(n));
        return rc;
    }

#elif defined NGC

#   define MeSqrt               sqrt
#   define MeSin                sinf
#   define MeAsin               asinf
#   define MeCos                cosf
#   define MeAcos               acosf
#   define MeTan                tanf
#   define MeAtan               atanf
#   define MeAtan2              atan2f
#   define MeFabs               fabs
#   define MeRecip(x)           ((MeReal)(1.0)/(x))
#   define MeRecipSqrt(x)       ((MeReal)(1.0)/sqrt(x)) 
#   define ME_PI                (3.14159265358979323846f)

#elif defined TRIMEDIA

#   define MeSqrt               sqrt
#   define MeSin                sin
#   define MeAsin               asin
#   define MeCos                cos
#   define MeAcos               acos
#   define MeTan                tan
#   define MeAtan               atan
#   define MeAtan2              atan2
#   define MeFabs               fabs
#   define MeRecip(x)           ((MeReal)(1.0)/(x))
#   define MeRecipSqrt(x)       ((MeReal)(1.0)/sqrt(x))
#   define ME_PI                (3.14159265358979323846f)

#elif defined MACOSX

#   define MeSqrt               sqrt
#   define MeSin                sin
#   define MeAsin               asin
#   define MeCos                cos
#   define MeAcos               acos
#   define MeTan                tan
#   define MeAtan               atan
#   define MeAtan2              atan2
#   define MeFabs               fabs
#   define MeRecip(x)           ((MeReal)(1.0)/(x))
#   define MeRecipSqrt(x)       ((MeReal)(1.0)/sqrt(x))
#   define ME_PI                (3.14159265358979323846f)

#elif defined _ME_API_DOUBLE

#   define MeSqrt               sqrt
#   define MeSin                sin
#   define MeAsin               asin
#   define MeCos                cos
#   define MeAcos               acos
#   define MeTan                tan
#   define MeAtan               tan
#   define MeAtan2              atan2
#   define MeFabs               fabs
#   define MeRecip(x)           (1.0f/(x))
#   define MeRecipSqrt(x)       (1.0f/sqrt(x))
#   define ME_PI                (3.14159265358979323846)

#elif (defined _MSC_VER && ! defined __cplusplus) || defined ELATE

    /*
      The MS Compiler does not really have a single precision math library,
      and nor does Elate, so we might as well just punt to double precision
      functions.
    */
#   define MeSqrt(x)            (MeReal)(sqrt((double) (x)))
#   define MeSin(x)             (MeReal)(sin((double) (x)))
#   define MeAsin(x)            (MeReal)(asin((double) (x)))
#   define MeCos(x)             (MeReal)(cos((double) (x)))
#   define MeAcos(x)            (MeReal)(acos((double) (x)))
#   define MeTan(x)             (MeReal)(tan((double) (x)))
#   define MeAtan(x)            (MeReal)(tan((double) (x)))
#   define MeAtan2(x,y)         (MeReal)(atan2((double) (x),(double) (y)))
#   define MeFabs(x)            (MeReal)(fabs((double) (x)))
#   define MeRecip(x)           (MeReal)(1.0f/(x))
#   define MeRecipSqrt(x)       (MeReal)(1.0f/sqrt((double)(x)))
#   define ME_PI                (MeReal)(3.14159265358979323846f)

#else

#   define MeSqrt               sqrtf
#   define MeSin                sinf
#   define MeAsin               asinf
#   define MeCos                cosf
#   define MeAcos               acosf
#   define MeTan                tanf
#   define MeAtan               atanf
#   define MeAtan2              atan2f
#   define MeFabs               fabsf
#   define MeRecip(x)           (1.0f/(x))
#   define MeRecipSqrt(x)       (1.0f/sqrtf(x))
#   define ME_PI                (3.14159265358979323846f)

#endif

#ifdef _ME_API_DOUBLE
#   ifndef _BUILD_VANILLA
#       define _BUILD_VANILLA
#   endif
#endif

#endif
