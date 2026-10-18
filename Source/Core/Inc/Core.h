/*=============================================================================
	Core.h: Unreal core public header file.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#ifndef _INC_CORE
#define _INC_CORE

/*----------------------------------------------------------------------------
	Low level includes.
----------------------------------------------------------------------------*/

// API definition.
#ifndef CORE_API
#define CORE_API DLL_IMPORT
#endif

#include <typeinfo>

// OS X doesn't define __GNUG__, apparently.  --ryan.
#if ((defined __GNUC__) && (!defined __GNUG__))
  #define __GNUG__ 1
#endif

// Unwanted Intel C++ level 4 warnings/ remarks.
#if __ICL
#pragma warning(disable : 873)
#pragma warning(disable : 981)
#pragma warning(disable : 522)
#pragma warning(disable : 271)
#pragma warning(disable : 424)
#pragma warning(disable : 193)
#pragma warning(disable : 444)
#pragma warning(disable : 440)
#pragma warning(disable : 171)
#pragma warning(disable : 1125)
#pragma warning(disable : 488)
#pragma warning(disable : 858)
#pragma warning(disable : 82)
#pragma warning(disable : 1)
#pragma warning(disable : 177)
#pragma warning(disable : 279)
#endif

// Build options.
#include "UnBuild.h"

#if !__PSX2_EE__ && !__GCN__

#define FTime DOUBLE

#else
// Time (needed so that the PS2 doesn't use doubles)
// Note that this is only used on the PC in UTexture::LastUpdateTime
#define FIXTIME 4294967296.f
class FTime
{
#if __GNUG__
#define TIMETYP long long
#else // __MWERKS__
#define TIMETYP unsigned long
#endif
public:

	        FTime      ()               {v=0;}
	        FTime      (float f)        {v=(TIMETYP)(f*FIXTIME);}
//	        FTime      (double d)       {v=(TIMETYP)(d*FIXTIME);}
	        FTime      (TIMETYP i,int)  {v=i;}
	float   GetFloat   ()               {return v/FIXTIME;}
	FTime   operator+  (float f) const  {return FTime(v+(TIMETYP)(f*FIXTIME),0);}
	float   operator-  (FTime t) const  {return (v-t.v)/FIXTIME;}
	FTime   operator*  (float f) const  {return FTime(v*f);}
	FTime   operator/  (float f) const  {return FTime(v/f);}
	FTime&  operator+= (float f)        {v=v+(TIMETYP)(f*FIXTIME); return *this;}
	FTime&  operator*= (float f)        {v=(TIMETYP)(v*f); return *this;}
	FTime&  operator/= (float f)        {v=(TIMETYP)(v/f); return *this;}
	int     operator== (FTime t)        {return v==t.v;}
	int     operator!= (FTime t)        {return v!=t.v;}
	int     operator>  (FTime t)        {return v>t.v;}
	FTime&  operator=  (const FTime& t) {v=t.v; return *this;}
	TIMETYP v;
};
#endif

// Are we on a console or not?
#if defined(_XBOX) || defined(__PSX2_EE__) || defined(__GCN__)
#define CONSOLE 1
#endif

#if _MSC_VER || __INTEL_COMPILER
	#define SUPPORTS_PRAGMA_PACK 1
#else
    #ifndef SUPPORTS_PRAGMA_PACK
	    #define SUPPORTS_PRAGMA_PACK 0
    #endif
#endif

#ifdef _WIN64
  #if 1 //!!! FIXME: Where is this defined? ((defined _AMD64) || (defined AMD64))
    #ifndef __x86_64__
      #define __x86_64__ 1
    #endif
  #else
    #error Not a Hammer build? You might have problems.
  #endif
#endif

#if (defined __x86_64__) || (defined __aarch64__) || (defined __LP64__)
    // 64-bit if x86_64 (Unix/_WIN64-mapped above), arm64 (__aarch64__), or any LP64 target.
    // wasm32 (web) is ILP32 and stays PLATFORM_32BITS.
    #define PLATFORM_64BITS 1
#else
    #define PLATFORM_32BITS 1
#endif

#if ((PLATFORM_64BITS) && (!SUPPORTS_PRAGMA_PACK))
#error You really need SUPPORTS_PRAGMA_PACK on 64-bit systems.
#endif


// Compiler specific include.
#if __GCN__
	#ifdef EMU
		typedef char TCHAR;
		#include "UnGCNEmu.h"
	#else
		#include <string.h>
		#include "UnMWerks.h" // Using UnVcWin32.h for GCN emulator, should be UnGnuG.h or something.
	#endif
#elif _MSC_VER
	#include "UnVcWin32.h"
#elif __GNUG__
	#include <string.h>
	#include "UnGnuG.h"
#elif __MWERKS__
	#include <string.h>
	#include "UnMWerks.h"
#elif __ICC
	#if __LINUX_X86__
		#include <string.h>
		#include "UnGnuG.h"
	#else
		#error Not yet supported
	#endif
#else
	#error Unknown Compiler
#endif


// OS specific include.
// OS specific include. The header follows the source: UnUnix.cpp is the platform layer for every
// non-MSVC target, Windows/MinGW included, so its prototypes (appGetLocalIP, appUnixPath, ...) are
// visible there too.
#if __UNIX__ || defined(__MINGW32__)
	#include "UnUnix.h"
#ifndef __MWERKS__
	#include <signal.h>
#endif
#endif

// SSE or not?
//
// UT_MSVC_NO_MMX: this compiler has no MMX intrinsics (MSVC x64, which dropped `__m64`). Epic wrote
// second copies of three inner loops for it that read a packed `__m128i` back through `m128i_u32`, an
// MSVC extension. GCC keeps MMX on x86-64 and its `__m128i` has no members, so a GCC build (MinGW, which
// defines `_WIN64` too) keeps the MMX loops.
#if defined(_WIN64) && defined(_MSC_VER)
  #define UT_MSVC_NO_MMX 1
#else
  #define UT_MSVC_NO_MMX 0
#endif

#if ((__INTEL__) && (!NO_SSE_SUPPORT) && !defined(UT_NO_X86_INTRIN))
  #define __HAS_SSE__ 1
  #if defined(__GNUC__)
    #include <xmmintrin.h>
    #include <mmintrin.h>
    // Compatibility shim for Intel fvec.h types/functions used by engine SSE code.
    typedef __m128 F32vec4;
    #define _MM_ALIGN16 __attribute__((aligned(16)))
    inline __m128 cmple(__m128 a, __m128 b) { return _mm_cmple_ps(a, b); }
    inline __m128 cmpgt(__m128 a, __m128 b) { return _mm_cmpgt_ps(a, b); }
    inline int move_mask(__m128 a) { return _mm_movemask_ps(a); }
  #else
    #include <fvec.h>
  #endif
  #include <emmintrin.h>
#endif

// Global constants.
enum {MAXBYTE		= 0xff       };
enum {MAXWORD		= 0xffffU    };
enum {MAXDWORD		= 0xffffffffU};
enum {MAXSBYTE		= 0x7f       };
enum {MAXSWORD		= 0x7fff     };
enum {MAXINT		= 0x7fffffff };
enum {INDEX_NONE	= -1         };
enum {UNICODE_BOM   = 0xfeff     };
enum ENoInit {E_NoInit = 0};

enum ERunningOS 
{ 
	OS_WIN95	= 0,
	OS_WIN98,
	OS_WINME,
	OS_WIN2K,
	OS_WINXP,
	OS_WINNT,
	OS_LINUX_X86,
	OS_FREEBSD_X86,
	OS_LINUX_X86_64,
	OS_MAC_OSX_PPC,
	OS_WIN64_AMD,
	OS_UNKNOWN	= 255
};

enum ERunningCPU
{	
	CPU_INTEL	= 0,
	CPU_AMD,
	CPU_AMD64,
	CPU_PPC,
	CPU_UNKNOWN	= 255
};

// Unicode or single byte character set mappings.
#ifdef _UNICODE
	#ifndef _TCHAR_DEFINED
		typedef UNICHAR  TCHAR;
		typedef UNICHARU TCHARU;
	#endif

    #ifndef _TEXT_DEFINED
	#undef TEXT
	#define TEXT(s) L##s
    #endif

    #ifndef _US_DEFINED
	#undef US
	#define US FString(L"")
    #endif

	inline TCHAR    FromAnsi   ( ANSICHAR In ) { return (BYTE)In;                        }
	inline TCHAR    FromUnicode( UNICHAR In  ) { return In;                              }
	inline ANSICHAR ToAnsi     ( TCHAR In    ) { return (_WORD)In<0x100 ? In : MAXSBYTE; }
	inline UNICHAR  ToUnicode  ( TCHAR In    ) { return In;                              }
#else
	#ifndef _TCHAR_DEFINED
		typedef ANSICHAR  TCHAR;
		typedef ANSICHARU TCHARU;
	#elif defined(_WIN32)
		// MinGW's <tchar.h>, reached through the Win32 headers, sets _TCHAR_DEFINED and supplies TCHAR
		// (== char here, NO_UNICODE_OS_SUPPORT), but not UE's TCHARU, which the branch above skipped.
		typedef ANSICHARU TCHARU;
	#endif
	#undef TEXT
	#define TEXT(s) s
	#undef US
	#define US FString("")
	inline TCHAR    FromAnsi   ( ANSICHAR In ) { return In;                              }
	inline TCHAR    FromUnicode( UNICHAR In  ) { return (_WORD)In<0x100 ? In : MAXSBYTE; }
	inline ANSICHAR ToAnsi     ( TCHAR In    ) { return (_WORD)In<0x100 ? In : MAXSBYTE; }
	inline UNICHAR  ToUnicode  ( TCHAR In    ) { return (BYTE)In;                        }
#endif

#if __UNIX__
// !!! FIXME: dirty, dirty, dirty.  --ryan.
extern CORE_API TCHAR *GUnixSpawnOnExit;
#endif

/*----------------------------------------------------------------------------
	Forward declarations.
----------------------------------------------------------------------------*/

// Objects.
class	UObject;
class		UExporter;
class		UFactory;
class		UField;
class			UConst;
class			UEnum;
class			UProperty;
class				UByteProperty;
class				UIntProperty;
class				UPointerProperty;
class				UBoolProperty;
class				UFloatProperty;
class				UObjectProperty;
class					UClassProperty;
class				UNameProperty;
class				UStructProperty;
class               UStrProperty;
class               UArrayProperty;
class				UDelegateProperty;
class			UStruct;
class				UFunction;
class				UState;
class					UClass;
class		ULinker;
class			ULinkerLoad;
class			ULinkerSave;
class		UPackage;
class		USubsystem;
class			USystem;
class		UTextBuffer;
class       URenderDevice;
class		UPackageMap;
class		UDebugger; //DEBUGGER

// Structs.
class FName;
class FArchive;
class FCompactIndex;
class FExec;
class FGuid;
class FMemCache;
class FMemStack;
class FPackageInfo;
class FTransactionBase;
class FUnknown;
class FRepLink;
class FArray;
class FLazyLoader;
class FString;
class FMalloc;

// Templates.
template<class T> class TArray;
template<class T> class TTransArray;
template<class T> class TLazyArray;
template<class TK, class TI> class TMap;
template<class TK, class TI> class TMultiMap;

// Globals.
CORE_API extern class FOutputDevice* GNull;

// EName definition.
#include "UnNames.h"

#include "UnForcePacking_begin.h"

/*-----------------------------------------------------------------------------
	Abstract interfaces.
-----------------------------------------------------------------------------*/

// An output device.
class CORE_API FOutputDevice
{
public:
	// FOutputDevice interface.
	virtual void Serialize( const TCHAR* V, EName Event )=0;
	virtual void Flush(){};

	// Simple text printing.
	void Log( const TCHAR* S );
	void Log( enum EName Type, const TCHAR* S );
	void Log( const FString& S );
	void Log( enum EName Type, const FString& S );
	void Logf( const TCHAR* Fmt, ... );
	void Logf( enum EName Type, const TCHAR* Fmt, ... );
};

// Error device.
class CORE_API FOutputDeviceError : public FOutputDevice
{
public:
	virtual void HandleError()=0;
};

// Memory allocator.
class CORE_API FMalloc
{
public:
	virtual void* Malloc( size_t Count, const TCHAR* Tag )=0;
	virtual void* Realloc( void* Original, size_t Count, const TCHAR* Tag )=0;
	virtual void Free( void* Original )=0;
	virtual void DumpAllocs()=0;
	virtual void HeapCheck()=0;
	virtual void Init()=0;
	virtual void Exit()=0;
};

// Configuration database cache.
class FConfigCache
{
public:
	virtual UBOOL GetBool( const TCHAR* Section, const TCHAR* Key, UBOOL& Value, const TCHAR* Filename=NULL )=0;
	virtual UBOOL GetInt( const TCHAR* Section, const TCHAR* Key, INT& Value, const TCHAR* Filename=NULL )=0;
	virtual UBOOL GetFloat( const TCHAR* Section, const TCHAR* Key, FLOAT& Value, const TCHAR* Filename=NULL )=0;
	virtual UBOOL GetString( const TCHAR* Section, const TCHAR* Key, TCHAR* Value, INT Size, const TCHAR* Filename=NULL )=0;
	virtual UBOOL GetString( const TCHAR* Section, const TCHAR* Key, class FString& Str, const TCHAR* Filename=NULL )=0;
	virtual const TCHAR* GetStr( const TCHAR* Section, const TCHAR* Key, const TCHAR* Filename=NULL )=0;
	virtual UBOOL GetSection( const TCHAR* Section, TCHAR* Value, INT Size, const TCHAR* Filename=NULL )=0;
	virtual TMultiMap<FString,FString>* GetSectionPrivate( const TCHAR* Section, UBOOL Force, UBOOL Const, const TCHAR* Filename=NULL )=0;
	virtual void EmptySection( const TCHAR* Section, const TCHAR* Filename=NULL )=0;
	virtual UBOOL GetSectionNames( TArray<FString>& Results, const TCHAR* Filename=NULL )=0;
	virtual void SetBool( const TCHAR* Section, const TCHAR* Key, UBOOL Value, const TCHAR* Filename=NULL )=0;
	virtual void SetInt( const TCHAR* Section, const TCHAR* Key, INT Value, const TCHAR* Filename=NULL )=0;
	virtual void SetFloat( const TCHAR* Section, const TCHAR* Key, FLOAT Value, const TCHAR* Filename=NULL )=0;
	virtual void SetString( const TCHAR* Section, const TCHAR* Key, const TCHAR* Value, const TCHAR* Filename=NULL, UBOOL UniqueKey = 1 )=0; // gam
	virtual void Flush( UBOOL Read, const TCHAR* Filename=NULL )=0;
	virtual void UnloadFile( const TCHAR* Filename )=0;
	virtual void Detach( const TCHAR* Filename )=0;
	virtual void Init( const TCHAR* InSystem, const TCHAR* InUser, UBOOL RequireConfig )=0;
	virtual void Exit()=0;
	virtual void Dump( FOutputDevice& Ar )=0;
	virtual ~FConfigCache() {};
};

// Any object that is capable of taking commands.
class CORE_API FExec
{
public:
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar )=0;
};

// Notification hook.
class CORE_API FNotifyHook
{
public:
	virtual void NotifyDestroy( void* Src ) {}
	virtual void NotifyPreChange( void* Src ) {}
	virtual void NotifyPostChange( void* Src ) {}
	virtual void NotifyExec( void* Src, const TCHAR* Cmd ) {}
};

// Interface for returning a context string.
class FContextSupplier
{
public:
	virtual FString GetContext()=0;
};

// A context for displaying modal warning messages.
class CORE_API FFeedbackContext : public FOutputDevice
{
public:
	virtual UBOOL YesNof( const TCHAR* Fmt, ... )=0;
	virtual void BeginSlowTask( const TCHAR* Task, UBOOL StatusWindow )=0;
	virtual void EndSlowTask()=0;
	virtual UBOOL VARARGS StatusUpdatef( INT Numerator, INT Denominator, const TCHAR* Fmt, ... )=0;
	virtual void SetContext( FContextSupplier* InSupplier )=0;
	virtual void MapCheck_Show() {};
	virtual void MapCheck_ShowConditionally() {};
	virtual void MapCheck_Hide() {};
	virtual void MapCheck_Clear() {};
	virtual void MapCheck_Add( INT InType, void* InActor, const TCHAR* InMessage ) {};

    // gam ---
	INT WarningCount;
	INT ErrorCount;
    // --- gam

    // gam ---
    FFeedbackContext()
    : ErrorCount( 0 )
	, WarningCount( 0 )
    {}
    // --- gam
};

// Class for handling undo/redo transactions among objects.
typedef void( *STRUCT_AR )( FArchive& Ar, void* TPtr );
typedef void( *STRUCT_DTOR )( void* TPtr );
class CORE_API FTransactionBase
{
public:
	virtual void SaveObject( UObject* Object )=0;
	virtual void SaveArray( UObject* Object, FArray* Array, INT Index, INT Count, INT Oper, INT ElementSize, STRUCT_AR Serializer, STRUCT_DTOR Destructor )=0;
	virtual void Apply()=0;
};

// File manager.
enum EFileTimes
{
	FILETIME_Create      = 0,
	FILETIME_LastAccess  = 1,
	FILETIME_LastWrite   = 2,
};
enum EFileWrite
{
	FILEWRITE_NoFail            = 0x01,
	FILEWRITE_NoReplaceExisting = 0x02,
	FILEWRITE_EvenIfReadOnly    = 0x04,
	FILEWRITE_Unbuffered        = 0x08,
	FILEWRITE_Append			= 0x10,
	FILEWRITE_AllowRead         = 0x20,
};
enum EFileRead
{
	FILEREAD_NoFail             = 0x01,
};
enum ECopyCompress
{
	FILECOPY_Normal				= 0x00,
	FILECOPY_Compress			= 0x01,
	FILECOPY_Decompress			= 0x02,
};
enum ECopyResult
{
	COPY_OK						= 0x00,
	COPY_MiscFail				= 0x01,
	COPY_ReadFail				= 0x02,
	COPY_WriteFail				= 0x03,
	COPY_CompFail				= 0x04,
	COPY_DecompFail				= 0x05,
	COPY_Canceled				= 0x06,
};
#define COMPRESSED_EXTENSION	TEXT(".uz2")

struct FCopyProgress
{
	virtual UBOOL Poll( FLOAT Fraction )=0;
};


CORE_API const TCHAR* appBaseDir();

class CORE_API FFileManager
{
public:
	virtual void Init(UBOOL Startup) {}
	virtual FArchive* CreateFileReader( const TCHAR* Filename, DWORD ReadFlags=0, FOutputDevice* Error=GNull )=0;
	virtual FArchive* CreateFileWriter( const TCHAR* Filename, DWORD WriteFlags=0, FOutputDevice* Error=GNull )=0;
	virtual INT FileSize( const TCHAR* Filename )=0;
	virtual UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )=0;
	virtual UBOOL IsReadOnly( const TCHAR* Filename )=0; // gam
	virtual DWORD Copy( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0, DWORD Compress=FILECOPY_Normal, FCopyProgress* Progress=NULL )=0;
	virtual UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 )=0;
	virtual SQWORD GetGlobalTime( const TCHAR* Filename )=0;
	virtual UBOOL SetGlobalTime( const TCHAR* Filename )=0;
	virtual UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )=0;
	virtual UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )=0;
	virtual TArray<FString> FindFiles( const TCHAR* Filename, UBOOL Files, UBOOL Directories )=0;
	virtual UBOOL SetDefaultDirectory( const TCHAR* Filename )=0;
	virtual FString GetDefaultDirectory()=0;
	// gam ---
	virtual FString ExpandPath( const TCHAR* Path );
	virtual INT CompareFileTimes( const TCHAR* FileA, const TCHAR* FileB )=0;
    // --- gam
    virtual const TCHAR *CalcHomeDir(void) { return(appBaseDir()); }

	// rjp --
	virtual UBOOL GetDriveLetters( FString& Drives ) const { return false; }
	virtual UBOOL IsCaseSensitive()                  const { return true;  }
	// -- rjp

	// Get file's last modification time. Return 0 if can't be determined, 1 if it can.
	//  Set output vars:
	//   Hours: 0-23
	//   Minutes: 0-59
	//   Secs: 0-59
	//   Day: as a day of the month (1 - 31)
	//   Month: as month of year (1 - 12)
	//   Year: Full 4+ digits.
	virtual UBOOL GetFileLastModTime( const TCHAR* FakeFilename, DWORD &Hours, DWORD &Minutes, DWORD &Secs, DWORD &Day, DWORD &Month, DWORD &Year ) { return 0; }

	// Where user files (log, User.ini, saves, cache) land, already resolved and with no side effects
	// (unlike CalcHomeDir, which mkdirs). A manager with no home-dir redirect writes next to the game.
	// Last in the class, so every earlier vtable slot stays where it was.
	virtual const TCHAR *GetUserDir(void) { return(appBaseDir()); }
};

//
// File Streaming.
//

class CORE_API FCriticalSection
{
public:
	FCriticalSection();
	~FCriticalSection();

	void Lock();
	void Unlock();

private:
	void* Handle;
};

class CORE_API FScopeCriticalSection
{
public:
	FScopeCriticalSection(FCriticalSection* InCriticalSection);
	~FScopeCriticalSection();

private:
	FCriticalSection* CriticalSection;
};


// gethostbyname() returns a static buffer that is overwritten
//  each call, so we need to serialize access to it so that correct
//  results make it back for each query. This is largely for IpDrv, but
//  UnUnix.cpp uses gethostbyname(), too, so it's in Core. --ryan.
CORE_API extern FCriticalSection *gethostbyname_mutex;


class FEdLoadError;

#include "UnForcePacking_end.h"

/*----------------------------------------------------------------------------
	Global variables.
----------------------------------------------------------------------------*/

// Core globals.
CORE_API extern FMemStack				GMem;
CORE_API extern FOutputDevice*			GLog;
CORE_API extern FOutputDevice*			GNull;
CORE_API extern FOutputDevice*		    GThrow;
CORE_API extern FOutputDeviceError*		GError;
CORE_API extern FFeedbackContext*		GWarn;
CORE_API extern FConfigCache*			GConfig;
CORE_API extern FTransactionBase*		GUndo;
CORE_API extern FOutputDevice*			GLogHook;
CORE_API extern FExec*					GExec;
CORE_API extern FMalloc*				GMalloc;
CORE_API extern FFileManager*			GFileManager;
CORE_API extern USystem*				GSys;
CORE_API extern UProperty*				GProperty;
CORE_API extern BYTE*					GPropAddr;
CORE_API extern UObject*				GPropObject;
CORE_API extern DWORD					GRuntimeUCFlags;
CORE_API extern USubsystem*				GWindowManager;
CORE_API extern TCHAR				    GErrorHist[4096];
CORE_API extern TCHAR                   GTrue[64], GFalse[64], GYes[64], GNo[64], GNone[64];
CORE_API extern TCHAR					GCdPath[];
CORE_API extern	DOUBLE					GSecondsPerCycle;
CORE_API extern	DOUBLE					GTempDouble;
CORE_API extern void					(*GTempFunc)(void*);
CORE_API extern QWORD					GTicks;
CORE_API extern INT                     GScriptCycles;
CORE_API extern DWORD					GPageSize;
CORE_API extern DWORD					GProcessorCount;
CORE_API extern DWORD					GPhysicalMemory;
CORE_API extern DWORD					GUglyHackFlags;
CORE_API extern UBOOL					GHackNoDepthTestRenderTarget;
CORE_API extern INT						GCDKS;
CORE_API extern TCHAR					GBFKey[256];
CORE_API extern DOUBLE					GBFTime;			
CORE_API extern BYTE					GMD5[16];
CORE_API extern UBOOL					GIsScriptable;
CORE_API extern UBOOL					GIsEditor;
CORE_API extern UBOOL					GIsUCC;
CORE_API extern UBOOL					GEdShowFogInViewports;
CORE_API extern UBOOL					GEdSelectionLock;
CORE_API extern UBOOL					GIsClient;
CORE_API extern UBOOL					GIsServer;
CORE_API extern UBOOL					GIsCriticalError;
CORE_API extern UBOOL					GIsStarted;
CORE_API extern UBOOL					GIsRunning;
CORE_API extern UBOOL					GIsGarbageCollecting;
CORE_API extern UBOOL					GIsSlowTask;
CORE_API extern UBOOL					GIsGuarded;
CORE_API extern UBOOL					GIsRequestingExit;
CORE_API extern UBOOL					GIsStrict;
CORE_API extern UBOOL                   GScriptEntryTag;
CORE_API extern UBOOL                   GLazyLoad;
CORE_API extern UBOOL					GUnicode;
CORE_API extern UBOOL					GUnicodeOS;
CORE_API extern UBOOL					GShowBuildLabel; // gam
CORE_API extern UBOOL					GIsClocking; 
CORE_API extern class FGlobalMath		GMath;
CORE_API extern class FArchive*         GDummySave;
CORE_API extern FLOAT					GAudioMaxRadiusMultiplier; //!! vogel: TODO: merge audio globals into one struct
CORE_API extern FLOAT					GAudioDefaultRadius;
CORE_API extern TArray<FEdLoadError>	GEdLoadErrors;
CORE_API extern	UDebugger*				GDebugger; //DEBUGGER
CORE_API extern UBOOL                   GTransientNaming; // sjs
CORE_API extern UBOOL                   GIsSoaking; // gam
CORE_API extern UBOOL					GIsBenchmarking;
CORE_API extern QWORD					GBaseCycles; // sjs
CORE_API extern QWORD					GMakeCacheIDIndex;
// gam ---
CORE_API extern TCHAR                   GBuildLabel[1024];
CORE_API extern TCHAR                   GMachineOS[1024];
CORE_API extern TCHAR                   GMachineCPU[1024];
CORE_API extern TCHAR                   GMachineVideo[1024];
// --- gam
CORE_API extern	TCHAR					GIni[1024];
CORE_API extern	TCHAR					GUserIni[1024];
CORE_API extern FLOAT					NEAR_CLIPPING_PLANE;
CORE_API extern FLOAT					FAR_CLIPPING_PLANE;
CORE_API extern UBOOL					GIsOpenGL;
CORE_API extern UBOOL					GIsOpenGLRevComps;
CORE_API extern UBOOL					GIsPixomatic;
CORE_API extern ERunningOS				GRunningOS;
CORE_API extern DWORD					GMemoryTag;
CORE_API extern DWORD					GAllocatedMemory;
CORE_API extern	QWORD					GFileManagerBytesRead;
CORE_API extern FLOAT					GFileManagerLoadingTime;
CORE_API extern FLOAT					GFileManagerSeekTime;
CORE_API extern FLOAT					GFileManagerMiscTime;
CORE_API extern FLOAT					GDeltaTime;
CORE_API extern	UBOOL					GUseFixedTimeStep;
CORE_API extern FLOAT					GFixedTimeStep;
CORE_API extern	UBOOL					GIsRecordingMovie;
CORE_API extern	DOUBLE					GCurrentTime;
CORE_API extern ERunningCPU				GRunningCPU;
CORE_API extern DWORD					GGPUDeviceID;
CORE_API extern DWORD					GGPUVendorID;

CORE_API extern TCHAR					GModName[256];		// Name of the currently active mod
CORE_API extern TCHAR					GModPath[1024];		// Path to the currently active mod
CORE_API extern TCHAR					GModDef[1024];		// Full filename of the Definition file for the active mod

#if MACOSX
// MacOS version (so we can limit detection code to one place in Core.) --ryan.
CORE_API extern INT						GMacOSVer;
#endif

// Demo Recording

CORE_API extern UBOOL					GDemoPlayback;	// True if a demo is being replayed

CORE_API extern TCHAR					GCPUType[255]; // Obscured: Make sure the DLL name is within this string

// Per module globals.
extern "C" DLL_EXPORT TCHAR GPackage[];

// Normal includes.
#include "UnFile.h"			// Low level utility code.
#include "UnObjVer.h"		// Object version info.
#include "UnArc.h"			// Archive class.
#include "UnTemplate.h"     // Dynamic arrays.
#include "UnName.h"			// Global name subsystem.
#include "UnStack.h"		// Script stack definition.
#include "UnObjBas.h"		// Object base class.
#include "UnCoreNet.h"		// Core networking.
#include "UnCorObj.h"		// Core object class definitions.
#include "UnClass.h"		// Class definition.
#include "UnType.h"			// Base property type.
#include "UnScript.h"		// Script class.
#include "UFactory.h"		// Factory definition.
#include "UExporter.h"		// Exporter definition.
#include "UnCache.h"		// Cache based memory management.
#include "UnMem.h"			// Stack based memory management.
#include "UnCId.h"          // Cache ID's.
#include "UnBits.h"         // Bitstream archiver.
#include "UnMath.h"         // Vector math functions.

// Worker class for tracking loading errors in the editor
class CORE_API FEdLoadError
{
public:
	FEdLoadError()
	{}
	FEdLoadError( INT InType, TCHAR* InDesc )
	{
		Type = InType;
		Desc = InDesc;
	}
	~FEdLoadError()
	{}

	// The types of things that could be missing.
	enum
	{
		TYPE_FILE		= 0,	// A totally missing file
		TYPE_RESOURCE	= 1,	// Texture/Sound/StaticMesh/etc
	};

	INT Type;		// TYPE_
	FString Desc;	// Description of the error

	UBOOL operator==( const FEdLoadError& LE ) const
	{
		return Type==LE.Type && Desc==LE.Desc;
	}
	FEdLoadError& operator=( const FEdLoadError Other )
	{
		Type = Other.Type;
		Desc = Other.Desc;
		return *this;
	}
};

//
// Archive for counting memory usage.
//
class CORE_API FArchiveCountMem : public FArchive
{
public:
	FArchiveCountMem( UObject* Src )
	: Num(0), Max(0)
	{
		Src->Serialize( *this );
	}
	SIZE_T GetNum()
	{
		return Num;
	}
	SIZE_T GetMax()
	{
		return Max;
	}
	void CountBytes( SIZE_T InNum, SIZE_T InMax )
	{
		Num += InNum;
		Max += InMax;
	}
protected:
	SIZE_T Num, Max;
};

enum
{
	MCTYPE_ERROR	= 0,
	MCTYPE_WARNING	= 1,
	MCTYPE_NOTE		= 2
};

typedef struct {
	INT Type;
	AActor* Actor;
	FString Message;
} MAPCHECK;

// A convenience to allow referring to axis' by name instead of number
enum EAxis
{
	AXIS_X	= 0,
	AXIS_Y	= 1,
	AXIS_Z	= 2,
};

// Very basic abstract debugger class.
class UDebugger //DEBUGGER
{
public:
	virtual void  DebugInfo( const UObject* Debugee, const FFrame* Stack, BYTE OpCode, INT LineNumber, INT InputPos )=0;
	virtual void  NotifyGC()=0;	// Called when garbage collection has begun
	virtual void  NotifyAccessedNone()=0;
	virtual UBOOL NotifyAssertionFailed( const INT LineNumber )=0;
	virtual UBOOL NotifyInfiniteLoop()=0;
};

#include "UnCoreNative.h"

#ifdef __PSX2_EE__
	#include "UnPSX2.h"
#endif

#ifdef __GCN__
	#include "UnGCN.h"
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
#endif

