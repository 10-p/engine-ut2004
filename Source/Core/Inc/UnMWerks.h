/*=============================================================================
	UnGnuG.h: Unreal definitions for Gnu G++. Unfinished. Unsupported.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*----------------------------------------------------------------------------
	Platform compiler definitions.
----------------------------------------------------------------------------*/

#ifdef __LINUX_X86__
	#define __UNIX__  1
	#define __LINUX__ 1
	#define __INTEL__ 1
	#define __INTEL_BYTE_ORDER__ 1
	#undef ASM
	#undef ASM3DNOW
	#undef ASMKNI
	#define ASMLINUX 1
	#define COMPILER "Compiled with GNU g++ ("__VERSION__")"
#elif __PSX2_EE__
	#define __UNIX__ 1
	#define __LINUX__ 1
	#define __INTEL__ 1
	#define __INTEL_BYTE_ORDER__ 1
	#undef ASM
	#undef ASM3DNOW
	#undef ASMKNI
	#undef ASMLINUX
	#define ASMPSX2 1
	#define COMPILER "Compiled with PSX2-EE g++ ("__VERSION__")"
	// Force use single precision
	#define tanh(x) tanhf(x)
	#define ceil(x) ceilf(x)
	#define fabs(x) fabsf(x)
	#define floor(x) floorf(x)
	#define cosh(x) coshf(x)
	#define sinh(x) sinhf(x)
	#define exp(x) expf(x)
	#define ldexp(x,y) ldexpf(x,y)
	#define log(x) logf(x)
	#define log10(x) log10f(x)
	#define pow(x,y) powf(x,y)
	#define sqrt(x) sqrtf(x)
	#define fmod(x,y) fmodf(x,y)
	#define sin( x ) sinf( x )
	#define cos( x ) cosf( x )
	#define tan( x ) tanf( x )
	#define asin( x ) asinf( x )
	#define acos( x ) acosf( x )
	#define atan( x ) atanf( x )
	#define atan2( x, y ) atan2f( x, y )
#elif __GCN__
	#define __UNIX__ 1
	#define __LINUX__ 1
//	#define __INTEL__ 0
//	#define __INTEL_BYTE_ORDER__ 0
	#undef ASM
	#undef ASM3DNOW
	#undef ASMKNI
	#undef ASMLINUX
	#undef ASMPSX2
	#define COMPILER "Compiled for GCN ("__VERSION__")"
#else
	#error Unsupported platform.
#endif

/*----------------------------------------------------------------------------
	Platform specifics types and defines.
----------------------------------------------------------------------------*/

// Undo any Windows defines.
#undef BYTE
#undef WORD
#undef DWORD
#undef INT
#undef FLOAT
#undef MAXBYTE
#undef MAXWORD
#undef MAXDWORD
#undef MAXINT
#undef VOID
#undef CDECL

// Make sure HANDLE is defined.
#define HANDLE DWORD
#define HINSTANCE DWORD

// Sizes.
enum {DEFAULT_ALIGNMENT = 16}; // Default boundary to align memory allocations on.
enum {CACHE_LINE_SIZE   = 32}; // Cache line size.
//#define GCC_PACK(n) __attribute__((packed,aligned(n)))
//#define GCC_MOVE_ALIGN(n) __attribute__((aligned(n))) __attribute__((section (".bss")))
//#define GCC_ALIGN(n) __attribute__((aligned(n)))
#define GCC_PACK(n) __attribute__((aligned(n)))
#define GCC_MOVE_ALIGN(n)
#define GCC_ALIGN(n)

// Optimization macros
#define DISABLE_OPTIMIZATION
#define ENABLE_OPTIMIZATION

// Function type macros.
#define DLL_IMPORT
#define DLL_EXPORT
#define DLL_EXPORT_CLASS
#define VARARGS
#define CDECL
#define STDCALL
#define FORCEINLINE /* Force code to be inline */
#define ZEROARRAY 1/* Zero-length arrays in structs */
#define __cdecl

// Variable arguments.
#define GET_VARARGS(msg,len,lastarg,fmt)	\
{	\
	va_list ArgPtr;	\
	va_start( ArgPtr, lastarg );	\
	vsprintf( msg, fmt, ArgPtr );	\
	va_end( ArgPtr );	\
}

#define GET_VARARGS_RESULT(msg,len,lastarg,fmt,result)	\
{	\
	va_list ArgPtr;	\
	va_start( ArgPtr, lastarg );	\
	result = vsprintf( msg, fmt, ArgPtr );	\
	va_end( ArgPtr );	\
}

// Unsigned base types.
typedef unsigned char		BYTE;		// 8-bit  unsigned.
typedef unsigned short		_WORD;		// 16-bit unsigned.
typedef unsigned int		DWORD;		// 32-bit unsigned.
typedef unsigned long long	QWORD;		// 64-bit unsigned.
#ifdef __GCN__
typedef unsigned int		OWORD;// __attribute__ ((mode (TI)));
#else
typedef unsigned int		OWORD __attribute__ ((mode (TI)));
#endif

// Signed base types.
typedef	signed char			SBYTE;		// 8-bit  signed.
typedef signed short		SWORD;		// 16-bit signed.
typedef signed int  		INT;		// 32-bit signed.
typedef signed long long	SQWORD;		// 64-bit signed.

// Character types.
typedef char			    ANSICHAR;	// An ANSI character.
typedef unsigned short      UNICHAR;	// A unicode character.
typedef unsigned char		ANSICHARU;	// An ANSI character.
typedef unsigned short      UNICHARU;	// A unicode character.

// Other base types.
typedef signed int			UBOOL;		// Boolean 0 (false) or 1 (true).
typedef float				FLOAT;		// 32-bit IEEE floating point.
#ifdef __PSX2_EE__
typedef float				DOUBLE;		// 32-bit IEEE floating point.
#else
typedef double				DOUBLE;		// 64-bit IEEE double.
#endif
#ifdef __GCN__
typedef unsigned long		size_t;
typedef unsigned long       SIZE_T;     // Corresponds to C SIZE_T.
#else
typedef unsigned int		size_t;
typedef unsigned int        SIZE_T;     // Corresponds to C SIZE_T.
#endif

// Bitfield type.
#if FORCE_EXTREME_PACKING
typedef BYTE                BITFIELD;	// For bitfields.
#else
typedef unsigned int        BITFIELD;	// For bitfields.
#endif

// Make sure characters are unsigned.
#ifdef __CHAR_UNSIGNED__
	#error "Bad compiler option: Characters must be signed"
#endif

// Strings.
#if __UNIX__
#define LINE_TERMINATOR TEXT("\n")
#define PATH_SEPARATOR TEXT("/")
#define DLLEXT TEXT(".so")
#else
#define LINE_TERMINATOR TEXT("\r\n")
#define PATH_SEPARATOR TEXT("\\")
#define DLLEXT TEXT(".dll")
#endif

// NULL.
#undef NULL
#define NULL 0

// Package implementation.
#define IMPLEMENT_PACKAGE_PLATFORM(pkgname) \
	BYTE GLoaded##pkgname;

// Platform support options.
#define PLATFORM_NEEDS_ARRAY_NEW 1
#define FORCE_ANSI_LOG           0

// OS unicode function calling.
#define TCHAR_CALL_OS(funcW,funcA) (funcA)
#define TCHAR_TO_ANSI(str) str
#define ANSI_TO_TCHAR(str) str

// !! Fixme: This is a workaround.
#define GCC_OPT_INLINE

// Memory
#define appAlloca(size) __alloca((size+7)&~7)

extern CORE_API DOUBLE GSecondsPerCycle;
CORE_API DOUBLE appSecondsSlow();

//
// Round a floating point number to an integer.
// Note that (int+.5) is rounded to (int+1).
//
#define DEFINED_appRound 1
inline INT appRound( FLOAT F )
{
	return (INT)(F);
}

//
// Converts to integer equal to or less than.
//
#define DEFINED_appFloor 1
inline INT appFloor( FLOAT F )
{
	static FLOAT Half=0.5f;
	return (INT)(F - Half);
}

//
// CPU cycles, related to GSecondsPerCycle.
//
#if ASMLINUX
#define DEFINED_appCycles 1
inline DWORD appCycles()
{
	DWORD r;
	asm("rdtsc" : "=a" (r) : "d" (r));
	return r;
}
#endif

//
// Seconds, arbitrarily based.
//
#if ASMLINUX
#define DEFINED_appSeconds 1
inline DOUBLE appSeconds()
{
	DWORD L,H;
	asm("rdtsc" : "=a" (L), "=d" (H));	
	return ((DOUBLE)L +  4294967296.0 * (DOUBLE)H) * GSecondsPerCycle;
}
#endif

//
// Memory zero.
//
#define DEFINED_appMemzero 1
inline void appMemzero( void* Dest, INT Count )
{
	memset( Dest, 0, Count );
}

/*----------------------------------------------------------------------------
	Globals.
----------------------------------------------------------------------------*/

// System identification.
extern "C"
{
	extern HINSTANCE      hInstance;
	extern CORE_API UBOOL GIsMMX;
	extern CORE_API UBOOL GIsPentiumPro;
	extern CORE_API UBOOL GIsKatmai;
	extern CORE_API UBOOL GIsK6;
	extern CORE_API UBOOL GIs3DNow;
}

// Module name
extern ANSICHAR GModule[32];

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

