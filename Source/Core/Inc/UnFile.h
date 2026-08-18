/*=============================================================================
	UnFile.h: General-purpose file utilities.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

// Global variables.
CORE_API extern DWORD GCRCTable[];

/*----------------------------------------------------------------------------
	Byte order conversion.
----------------------------------------------------------------------------*/

// Bitfields.

#if FORCE_EXTREME_PACKING   // --ryan.
	#define NEXT_BITFIELD(b) ((BYTE) (((BYTE) b)<<1))
	#define FIRST_BITFIELD   (1)
#endif

#ifndef NEXT_BITFIELD
	#if __INTEL_BYTE_ORDER__
		#define NEXT_BITFIELD(b) ((b)<<1)
		#define FIRST_BITFIELD   (1)
	#else
		#define NEXT_BITFIELD(b) ((b)>>1)
		#define FIRST_BITFIELD   (0x80000000)
	#endif
#endif

#if __INTEL_BYTE_ORDER__
	#define INTEL_ORDER16(x)   (x)
	#define INTEL_ORDER32(x)   (x)
	#define INTEL_ORDER64(x)   (x)
#else

    // These macros are not safe to use unless data is UNSIGNED!
	#define INTEL_ORDER16_unsigned(x)   ((((x)>>8)&0xff)+ (((x)<<8)&0xff00))
	#define INTEL_ORDER32_unsigned(x)   (((x)>>24) + (((x)>>8)&0xff00) + (((x)<<8)&0xff0000) + ((x)<<24))

    static inline _WORD INTEL_ORDER16(_WORD val)
    {
        #if MACOSX
        register _WORD retval;
        __asm__ volatile("lhbrx %0,0,%1"
                : "=r" (retval)
                : "r" (&val)
                );
        return retval;
        #else
        return(INTEL_ORDER16_unsigned(val));
        #endif
    }

    static inline SWORD INTEL_ORDER16(SWORD val)
    {
        _WORD uval = *((_WORD *) &val);
        uval = INTEL_ORDER16(uval);
        return( *((SWORD *) &uval) );
    }

    static inline DWORD INTEL_ORDER32(DWORD val)
    {
        #if MACOSX
        register DWORD retval;
        __asm__ __volatile__ (
            "lwbrx %0,0,%1"
                : "=r" (retval)
                : "r" (&val)
        );
        return retval;
        #else
        return(INTEL_ORDER32_unsigned(val));
        #endif
    }

    static inline INT INTEL_ORDER32(INT val)
    {
        DWORD uval = *((DWORD *) &val);
        uval = INTEL_ORDER32(uval);
        return( *((INT *) &uval) );
    }

	static inline QWORD INTEL_ORDER64(QWORD x)
	{
		/* Separate into high and low 32-bit values and swap them */
		DWORD l = (DWORD) (x & 0xFFFFFFFF);
		DWORD h = (DWORD) ((x >> 32) & 0xFFFFFFFF);
	    return( (((QWORD) (INTEL_ORDER32(l))) << 32) |
		         ((QWORD) (INTEL_ORDER32(h))) );
	}
#endif


/*-----------------------------------------------------------------------------
	Stats.
-----------------------------------------------------------------------------*/

#if DO_STAT
	#define STAT(x) x
#else
	#define STAT(x) {}
#endif

/*-----------------------------------------------------------------------------
	Global init and exit.
-----------------------------------------------------------------------------*/

CORE_API void appInit( const TCHAR* InPackage, const TCHAR* InCmdLine, FMalloc* InMalloc, FOutputDevice* InLog, FOutputDeviceError* InError, FFeedbackContext* InWarn, FFileManager* InFileManager, FConfigCache*(*ConfigFactory)(), UBOOL RequireConfig );
CORE_API void appPreExit();
CORE_API void appExit();

/*-----------------------------------------------------------------------------
	Logging and critical errors.
-----------------------------------------------------------------------------*/

CORE_API void appRequestExit( UBOOL Force );

CORE_API void VARARGS appFailAssert( const ANSICHAR* Expr, const ANSICHAR* File, INT Line );
CORE_API void VARARGS appUnwindf( const TCHAR* Fmt, ... );
CORE_API const TCHAR* appGetSystemErrorMessage( INT Error=0 );
CORE_API const void appDebugMessagef( const TCHAR* Fmt, ... );
CORE_API const UBOOL appMsgf( INT Type, const TCHAR* Fmt, ... );
CORE_API const void appGetLastError( void );
CORE_API const void EdClearLoadErrors();
CORE_API const void EdLoadErrorf( INT Type, const TCHAR* Fmt, ... );
CORE_API void appDebugBreak();
CORE_API UBOOL appIsDebuggerPresent();

#define debugf				GLog->Logf
#define appErrorf			GError->Logf

#if DO_GUARD_SLOW
	#define debugfSlow		GLog->Logf
	#define appErrorfSlow	GError->Logf
#else
	#define debugfSlow		GNull->Logf
	#define appErrorfSlow	GNull->Logf
#endif

/*-----------------------------------------------------------------------------
	Misc.
-----------------------------------------------------------------------------*/

CORE_API void* appGetDllHandle( const TCHAR* DllName );
// ufront 2.37: load a genuine EXTERNAL library (OpenAL, a driver), as opposed to an engine package.
// The distinction is not pedantic -- appGetDllHandle answers "is this package in the exe?", and on a
// static Windows build that answer is always yes; pointing it at a real file name instead made it
// LoadLibrary the retail 2004 DLLs that sit in every UT2004 System\ directory. Returns NULL when the
// library is genuinely absent, which callers are expected to handle (ALAudio degrades to no sound).
CORE_API void* appGetSystemDllHandle( const TCHAR* DllName );
CORE_API void appFreeDllHandle( void* DllHandle );
CORE_API void* appGetDllExport( void* DllHandle, const TCHAR* ExportName );
CORE_API void appLaunchURL( const TCHAR* URL, const TCHAR* Parms=NULL, FString* Error=NULL );
CORE_API void* appCreateProc( const TCHAR* URL, const TCHAR* Parms );
CORE_API UBOOL appGetProcReturnCode( void* ProcHandle, INT* ReturnCode );
CORE_API class FGuid appCreateGuid();
CORE_API void appCreateTempFilename( const TCHAR* Path, TCHAR* Result256 );
CORE_API void appCleanFileCache();
CORE_API UBOOL appFindPackageFile( const TCHAR* In, const FGuid* Guid, TCHAR* Out, INT GenerationLevel=0 );
CORE_API UBOOL appCheckPackageFileGUID( const TCHAR* InFilename, const FGuid* InGuid );
CORE_API INT appCreateBitmap( const TCHAR* Pattern, INT Width, INT Height, DWORD* Data, FFileManager* FileManager = GFileManager ); // gam

/*-----------------------------------------------------------------------------
	Clipboard.
-----------------------------------------------------------------------------*/

CORE_API void appClipboardCopy( const TCHAR* Str );
CORE_API FString appClipboardPaste();

/*-----------------------------------------------------------------------------
	Guard macros for call stack display.
-----------------------------------------------------------------------------*/

//
// guard/unguardf/unguard macros.
// For showing calling stack when errors occur in major functions.
// Meant to be enabled in release builds.
//

#if ((_DEBUG) && (!DO_PORTABLE_GUARD_BLOCKS)) || !DO_GUARD || __UNIX__
  #if __PSX2_EE__ || __GCN__ || __UNIX__
    #define guard(func)			{
  #else
    #define guard(func)			{static const TCHAR __FUNC_NAME__[]=TEXT(#func);
  #endif
  #define unguard				}
  #define unguardf(msg)		}

#elif DO_PORTABLE_GUARD_BLOCKS
  #define MAX_PORTABLE_GUARD_BLOCKS 8192

  class FString;

  class CORE_API UnGuardBlock
  {
  public:
      UnGuardBlock(const char *text)
      {
          if (++GuardIndex < MAX_PORTABLE_GUARD_BLOCKS)
              GuardTexts[GuardIndex] = text;
          //printf("--> %d %s\n", (int) GuardIndex, text);
      }

      ~UnGuardBlock(void)
      {
          //printf("<-- %d %s\n", (int) GuardIndex, GuardTexts[GuardIndex]);
          GuardIndex--;
      }

      static void GetBackTrace(FString &str);

  private:
      static INT GuardIndex;
      static const char *GuardTexts[MAX_PORTABLE_GUARD_BLOCKS];
  };

  #define guard(func)   { UnGuardBlock __FUNC_NAME__(#func);
  #define unguard       }
  #define unguardf(msg) }

#else
  #define guard(func)			{static const TCHAR __FUNC_NAME__[]=TEXT(#func); try{
  #define unguard				}catch(TCHAR*Err){throw Err;}catch(...){appUnwindf(TEXT("%s"),__FUNC_NAME__); throw;}}
  #define unguardf(msg)			}catch(TCHAR*Err){throw Err;}catch(...){appUnwindf(TEXT("%s"),__FUNC_NAME__); appUnwindf msg; throw;}}
#endif


//
// guardSlow/unguardfSlow/unguardSlow macros.
// For showing calling stack when errors occur in performance-critical functions.
// Meant to be disabled in release builds.
//
#if defined(_DEBUG) || !DO_GUARD || !DO_GUARD_SLOW
	#define guardSlow(func)		{
	#define unguardfSlow(msg)	}
	#define unguardSlow			}
	#define unguardfSlow(msg)	}
#else
	#define guardSlow(func)		guard(func)
	#define unguardSlow			unguard
	#define unguardfSlow(msg)	unguardf(msg)
#endif

//
// For throwing string-exceptions which safely propagate through guard/unguard.
//
CORE_API void VARARGS appThrowf( const TCHAR* Fmt, ... );

/*-----------------------------------------------------------------------------
	Check macros for assertions.
-----------------------------------------------------------------------------*/

// gam ---

//
// "check" expressions are only evaluated if enabled.
// "verify" expressions are always evaluated, but only cause an error if enabled.
//
#if DO_CHECK
	#define check(expr)  {if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ );}
	#define verify(expr) {if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ );}
#else
	#define check(expr) {}
	#define verify(expr) if(expr){}
#endif

//
// Check for development only.
//
#if DO_CHECK_SLOW
	#define checkSlow(expr)  {if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ );}
	#define verifySlow(expr) {if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ );}
#else
	#define checkSlow(expr) {}
	#define verifySlow(expr) if(expr){}
#endif

// --- gam

/*-----------------------------------------------------------------------------
	Timing macros.
-----------------------------------------------------------------------------*/

//
// Normal timing.
//

// gam ---
#if DO_CLOCK
    #define clock(Timer)   { Timer -= appCycles();}
    #define unclock(Timer) { Timer += appCycles()-12;}
#else
    #define clock(Timer) {}
    #define unclock(Timer) {}
#endif

//
// Performance critical timing.
//
#if DO_CLOCK_SLOW
	#define clockSlow(Timer) { if( GIsClocking ) Timer-=appCycles();}
	#define unclockSlow(Timer) { if( GIsClocking ) Timer+=appCycles()-12;}
#else
	#define clockSlow(Timer) {}
	#define unclockSlow(Timer) {}
#endif
// --- gam

/*-----------------------------------------------------------------------------
	Text format.
-----------------------------------------------------------------------------*/

CORE_API FString appFormat( FString Src, const TMultiMap<FString,FString>& Map );

/*-----------------------------------------------------------------------------
	Localization.
-----------------------------------------------------------------------------*/

CORE_API const TCHAR* Localize( const TCHAR* Section, const TCHAR* Key, const TCHAR* Package=GPackage, const TCHAR* LangExt=NULL, UBOOL Optional=0 );
CORE_API const TCHAR* LocalizeError( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
CORE_API const TCHAR* LocalizeProgress( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
CORE_API const TCHAR* LocalizeQuery( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
CORE_API const TCHAR* LocalizeGeneral( const TCHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
CORE_API void LocalizeBig( const TCHAR* Section, const TCHAR* Key, const TCHAR* Package=GPackage, const TCHAR* LangExt=NULL, UBOOL Optional=0, TCHAR* Result = NULL, INT Size = 0 );


#if UNICODE
	CORE_API const TCHAR* Localize( const ANSICHAR* Section, const ANSICHAR* Key, const TCHAR* Package=GPackage, const TCHAR* LangExt=NULL, UBOOL Optional=0 );
	CORE_API const TCHAR* LocalizeError( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
	CORE_API const TCHAR* LocalizeProgress( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
	CORE_API const TCHAR* LocalizeQuery( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
	CORE_API const TCHAR* LocalizeGeneral( const ANSICHAR* Key, const TCHAR* Package, const TCHAR* LangExt=NULL );
#endif

CORE_API UBOOL appIsOnConsole(); // gam

/*-----------------------------------------------------------------------------
	File functions.
-----------------------------------------------------------------------------*/

// File utilities.
CORE_API const TCHAR* appFExt( const TCHAR* Filename );
CORE_API UBOOL appUpdateFileModTime( TCHAR* Filename );
CORE_API FString appGetGMTRef();

/*-----------------------------------------------------------------------------
	OS functions.
-----------------------------------------------------------------------------*/

CORE_API const TCHAR* appCmdLine();
CORE_API const TCHAR* appBaseDir();
CORE_API const TCHAR* appPackage();
CORE_API const TCHAR* appComputerName();
CORE_API const TCHAR* appUserName();

/*-----------------------------------------------------------------------------
	Timing functions.
-----------------------------------------------------------------------------*/

#if !DEFINED_appCycles
CORE_API DWORD appCycles();
#endif

#if !DEFINED_appSeconds
CORE_API DOUBLE appSeconds();
#endif

// sjs ---
#if !DEFINED_appResetTimer
CORE_API void appResetTimer();
#endif
// --- sjs

CORE_API void appSystemTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec );
CORE_API const TCHAR* appTimestamp();
CORE_API DOUBLE appSecondsSlow();
CORE_API void appSleep( FLOAT Seconds );

/*-----------------------------------------------------------------------------
	Character type functions.
-----------------------------------------------------------------------------*/

inline TCHAR appToUpper( TCHAR c )
{
	return (c<'a' || c>'z') ? (c) : (c+'A'-'a');
}
inline TCHAR appToLower( TCHAR c )
{
	return (c<'A' || c>'Z') ? (c) : (c+'a'-'A');
}
// gam ---
inline UBOOL appIsUpper( TCHAR c )
{
	return (c>='A' && c<='Z');
}
inline UBOOL appIsLower( TCHAR c )
{
	return (c>='a' && c<='z');
}
// --- gam
inline UBOOL appIsAlpha( TCHAR c )
{
	return (c>='a' && c<='z') || (c>='A' && c<='Z');
}
inline UBOOL appIsDigit( TCHAR c )
{
	return c>='0' && c<='9';
}
inline UBOOL appIsAlnum( TCHAR c )
{
	return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9');
}
// gam ---
#include <ctype.h>
#if UNICODE
#include <wctype.h>
#endif
inline UBOOL appIsSpace( TCHAR c )
{
#if UNICODE
    return( iswspace(c) );
#else
    return( isspace(c) );
#endif
}
// --- gam

/*-----------------------------------------------------------------------------
	String functions.
-----------------------------------------------------------------------------*/
#define STATICSTRINGLENGTH 1024 // rjp

CORE_API const ANSICHAR* appToAnsi( const TCHAR* Str, ANSICHAR* ACh=NULL, INT maxlen = STATICSTRINGLENGTH );
CORE_API const UNICHAR* appToUnicode( const TCHAR* Str, UNICHAR* UCh=NULL, INT maxlen = STATICSTRINGLENGTH );
CORE_API const TCHAR* appFromAnsi( const ANSICHAR* Str, TCHAR* TCh=NULL, INT maxlen = STATICSTRINGLENGTH );
CORE_API const TCHAR* appFromUnicode( const UNICHAR* Str, TCHAR* TCh=NULL, INT maxlen = STATICSTRINGLENGTH );
CORE_API UBOOL appIsPureAnsi( const TCHAR* Str );

CORE_API TCHAR* appStrcpy( TCHAR* Dest, const TCHAR* Src );
CORE_API INT appStrcpy( const TCHAR* String );
CORE_API INT appStrlen( const TCHAR* String );
CORE_API TCHAR* appStrstr( const TCHAR* String, const TCHAR* Find );
CORE_API TCHAR* appStrchr( const TCHAR* String, INT c );
CORE_API TCHAR* appStrcat( TCHAR* Dest, const TCHAR* Src );
CORE_API INT appStrcmp( const TCHAR* String1, const TCHAR* String2 );
CORE_API INT appStricmp( const TCHAR* String1, const TCHAR* String2 );
CORE_API INT appStrncmp( const TCHAR* String1, const TCHAR* String2, INT Count );
CORE_API TCHAR* appStaticString1024();
CORE_API ANSICHAR* appAnsiStaticString1024();

CORE_API const TCHAR* appSpc( int Num );
CORE_API TCHAR* appStrncpy( TCHAR* Dest, const TCHAR* Src, int Max);
CORE_API TCHAR* appStrncat( TCHAR* Dest, const TCHAR* Src, int Max);
CORE_API TCHAR* appStrupr( TCHAR* String );
CORE_API const TCHAR* appStrfind(const TCHAR* Str, const TCHAR* Find);
CORE_API DWORD appStrCrc( const TCHAR* Data );
CORE_API DWORD appStrCrcCaps( const TCHAR* Data );
CORE_API INT appAtoi( const TCHAR* Str );
CORE_API TCHAR* appItoa( const INT Num );
CORE_API FLOAT appAtof( const TCHAR* Str );
CORE_API QWORD appStrtoq( const TCHAR* Start, TCHAR** End, INT Base );
CORE_API TCHAR* appQtoa( QWORD Num, INT Base=10 );
CORE_API INT appStrtoi( const TCHAR* Start, TCHAR** End, INT Base );
CORE_API INT appStrnicmp( const TCHAR* A, const TCHAR* B, INT Count );
CORE_API INT appSprintf( TCHAR* Dest, const TCHAR* Fmt, ... );
CORE_API void appTrimSpaces( ANSICHAR* String);
CORE_API INT appStrPrefix( const TCHAR* Str, const TCHAR* Prefix ); // gam

#if UNICODE
	#define appSSCANF	swscanf
#else
	#define appSSCANF	sscanf
#endif

typedef int QSORT_RETURN;
typedef QSORT_RETURN(CDECL* QSORT_COMPARE)( const void* A, const void* B );
CORE_API void appQsort( void* Base, INT Num, INT Width, QSORT_COMPARE Compare );

//
// Case insensitive string hash function.
//
inline DWORD appStrihash( const TCHAR* Data )
{
	DWORD Hash=0;
	while( *Data )
	{
		TCHAR Ch = appToUpper(*Data++);
		BYTE  B  = Ch;
		Hash     = ((Hash >> 8) & 0x00FFFFFF) ^ GCRCTable[(Hash ^ B) & 0x000000FF];
#if UNICODE
		B        = Ch>>8;
		Hash     = ((Hash >> 8) & 0x00FFFFFF) ^ GCRCTable[(Hash ^ B) & 0x000000FF];
#endif
	}
	return Hash;
}

/*-----------------------------------------------------------------------------
	Parsing functions.
-----------------------------------------------------------------------------*/

CORE_API UBOOL ParseCommand( const TCHAR** Stream, const TCHAR* Match );
CORE_API UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, class FName& Name );
CORE_API UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, DWORD& Value );
CORE_API UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, class FGuid& Guid );
CORE_API UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, TCHAR* Value, INT MaxLen, TCHAR* AllowedChars = NULL );
CORE_API UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, BYTE& Value );
CORE_API UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, SBYTE& Value );
CORE_API UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, _WORD& Value );
CORE_API UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, SWORD& Value );
CORE_API UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, FLOAT& Value );
CORE_API UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, INT& Value );
CORE_API UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, FString& Value, TCHAR* AllowedChars = NULL );
CORE_API UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, QWORD& Value );
CORE_API UBOOL Parse( const TCHAR* Stream, const TCHAR* Match, SQWORD& Value );
CORE_API UBOOL ParseUBOOL( const TCHAR* Stream, const TCHAR* Match, UBOOL& OnOff );
CORE_API UBOOL ParseObject( const TCHAR* Stream, const TCHAR* Match, class UClass* Type, class UObject*& DestRes, class UObject* InParent );
CORE_API UBOOL ParseLine( const TCHAR** Stream, TCHAR* Result, INT MaxLen, UBOOL Exact=0 );
CORE_API UBOOL ParseLine( const TCHAR** Stream, FString& Resultd, UBOOL Exact=0 );
// ufront 2.38 — is this command-line token a GLOBAL OPTION (`Key=Value`) rather than a URL?
//
// ★ The startup code takes the FIRST token of the command line as the map/URL to browse, skipping it
// only when it starts with '-'. That was enough while every launcher happened to pass a flag first —
// and it silently stopped being enough the moment one did not: 2.37 dropped `-nohomedir` from the web
// launch argv (the engine defaults to it now), which promoted `CPUSPEED=3000` — a global option every
// web launch carries — to argv[0]. The engine dutifully tried to load it AS A MAP, failed, took the
// fail-soft path, and never reached the main menu at all: the plain "just start the game" launch
// showed a black screen.
// A `Key=Value` token is an option wherever it sits; a URL that carries options spells them after a
// '?' (`DM-Rankin?Game=X`), which is exactly what tells the two apart.
CORE_API UBOOL appIsGlobalOptionToken( const TCHAR* Token );
// ufront 2.38 — apply every `-ini:<File>:[<Section>]:<Key>=<Value>` token on the command line.
//
// ★ The shipped launcher emits `-ini:User:[DefaultPlayer]:Name=<player>` for the Network panel's Player
// Name box, and until 2.38 NOTHING in any of the four engines read it: the name only ever took effect via
// a travel URL's `?Name=`, so hosting or joining from the game's OWN menus always logged you in as
// "Player". `User` / `System` are the config cache's generic file names — no ini path is hardcoded.
// Call once, right after GConfig->Init.
CORE_API void appApplyIniOverrides();
CORE_API UBOOL ParseToken( const TCHAR*& Str, TCHAR* Result, INT MaxLen, UBOOL UseEscape );
CORE_API UBOOL ParseToken( const TCHAR*& Str, FString& Arg, UBOOL UseEscape );
CORE_API FString ParseToken( const TCHAR*& Str, UBOOL UseEscape );
CORE_API void ParseNext( const TCHAR** Stream );
CORE_API UBOOL ParseParam( const TCHAR* Stream, const TCHAR* Param );

/*-----------------------------------------------------------------------------
	Math functions.
-----------------------------------------------------------------------------*/

#if !DEFINED_appMathIntrinsics
CORE_API DOUBLE appExp( DOUBLE Value );
CORE_API DOUBLE appLoge( DOUBLE Value );
CORE_API DOUBLE appFmod( DOUBLE A, DOUBLE B );
CORE_API DOUBLE appSin( DOUBLE Value );
CORE_API DOUBLE appAsin( DOUBLE Value );
CORE_API DOUBLE appCos( DOUBLE Value );
CORE_API DOUBLE appAcos( DOUBLE Value );
CORE_API DOUBLE appTan( DOUBLE Value );
CORE_API DOUBLE appAtan( DOUBLE Value );
CORE_API DOUBLE appAtan2( DOUBLE Y, DOUBLE X );
CORE_API DOUBLE appSqrt( DOUBLE Value );
CORE_API DOUBLE appPow( DOUBLE A, DOUBLE B );
CORE_API UBOOL appIsNan( DOUBLE Value );
CORE_API void appRandInit( INT Seed );
CORE_API INT appRand();
CORE_API FLOAT appFrand();
CORE_API INT appRound( FLOAT Value );
CORE_API INT appFloor( FLOAT Value );
CORE_API INT appCeil( FLOAT Value );
CORE_API FLOAT appFractional( FLOAT Value );
#endif

CORE_API void appSRandInit( INT );
CORE_API FLOAT appSRand();

/*-----------------------------------------------------------------------------
	Array functions.
-----------------------------------------------------------------------------*/

// Core functions depending on TArray and FString.
CORE_API UBOOL appLoadFileToArray( TArray<BYTE>& Result, const TCHAR* Filename, FFileManager* FileManager=GFileManager, DWORD ReadFlags=0, UBOOL bLogErrors=0 );
CORE_API UBOOL appLoadFileToString( FString& Result, const TCHAR* Filename, FFileManager* FileManager=GFileManager, DWORD ReadFlags=0, UBOOL bLogErrors=0 );
CORE_API UBOOL appSaveArrayToFile( const TArray<BYTE>& Array, const TCHAR* Filename, FFileManager* FileManager=GFileManager, DWORD WriteFlags=0, UBOOL bLogErrors=0 );
CORE_API UBOOL appSaveStringToFile( const FString& String, const TCHAR* Filename, FFileManager* FileManager=GFileManager, DWORD WriteFlags=0, UBOOL bLogErrors=0 );

/*-----------------------------------------------------------------------------
	Memory functions.
-----------------------------------------------------------------------------*/

CORE_API void* appMemmove( void* Dest, const void* Src, INT Count );
CORE_API INT appMemcmp( const void* Buf1, const void* Buf2, INT Count );
CORE_API UBOOL appMemIsZero( const void* V, int Count );
CORE_API DWORD appMemCrc( const void* Data, INT Length, DWORD CRC=0 );
CORE_API void appMemswap( void* Ptr1, void* Ptr2, DWORD Size );
CORE_API void appMemset( void* Dest, INT C, INT Count );

#ifndef DEFINED_appMemcpy
CORE_API void appMemcpy( void* Dest, const void* Src, INT Count );
#endif

#ifndef DEFINED_appMemzero
CORE_API void appMemzero( void* Dest, INT Count );
#endif

//
// C style memory allocation stubs.
//
#define appMalloc     GMalloc->Malloc
#define appFree       GMalloc->Free
#define appRealloc    GMalloc->Realloc

//
// C++ style memory allocation.
//
// UT_REPLACEMENT_ALLOC -- ufront 2.19c. These SIX are the REPLACEABLE global forms, and a
// replacement operator new/delete may not be `inline` (clang says so: -Winline-new-delete).
// Declared this way they are emitted OUT-OF-LINE as weak symbols whenever the compiler does not
// inline them -- which at -O0 is always, in every one of the 110 engine objects that allocate.
// wasm-ld then resolves the PROGRAM-WIDE operator new to one of those, so Emscripten's own
// WasmFS static initializer (priority 100, long before appInit) allocates through GMalloc, which
// at that moment is still MallocError and returns NULL -- make_shared builds its control block on
// nothing, `weak_this` is never set, and OpenFileState::create dies in shared_from_this() with
// bad_weak_ptr. That is the entire "-O0 aborts during static init" bug.
//
// At -O3 the same definitions are inlined everywhere and never emitted, so system code keeps
// libc++'s operator new. Forcing the inline makes -O0 behave EXACTLY as -O3 already did --
// engine allocations go to appMalloc, system allocations go to libc++ -- rather than inventing a
// third behaviour. Verified: 110 engine objects emit `_Znwm` at -O0 before this, 0 after; the -O3
// wasm is unchanged. The placement form `operator new(size_t, const TCHAR*)` is NOT a replacement
// function and is deliberately left alone.
//
#define UT_REPLACEMENT_ALLOC __attribute__((always_inline))
inline void* operator new( size_t Size, const TCHAR* Tag )
{
	guardSlow(new);
// !!! FIXME: Don't cast to DWORD! Fix appMalloc()'s declaration! --ryan.
	return appMalloc( (DWORD) Size, Tag );
	unguardSlow;
}
UT_REPLACEMENT_ALLOC inline void* operator new( size_t Size )
{
	guardSlow(new);
// !!! FIXME: Don't cast to DWORD! Fix appMalloc()'s declaration! --ryan.
	return appMalloc( (DWORD) Size, TEXT("new") );
	unguardSlow;
}
UT_REPLACEMENT_ALLOC inline void operator delete( void* Ptr )
{
	guardSlow(delete);
	appFree( Ptr );
	unguardSlow;
}
UT_REPLACEMENT_ALLOC inline void operator delete( void* Ptr, size_t )
{
	guardSlow(delete);
	appFree( Ptr );
	unguardSlow;
}

#if PLATFORM_NEEDS_ARRAY_NEW
inline void* operator new[]( size_t Size, const TCHAR* Tag )
{
	guardSlow(new);
// !!! FIXME: Don't cast to DWORD! Fix appMalloc()'s declaration! --ryan.
	return appMalloc( (DWORD) Size, Tag );
	unguardSlow;
}
UT_REPLACEMENT_ALLOC inline void* operator new[]( size_t Size )
{
	guardSlow(new);
// !!! FIXME: Don't cast to DWORD! Fix appMalloc()'s declaration! --ryan.
	return appMalloc( (DWORD) Size, TEXT("new") );
	unguardSlow;
}
UT_REPLACEMENT_ALLOC inline void operator delete[]( void* Ptr )
{
	guardSlow(delete);
	appFree( Ptr );
	unguardSlow;
}
UT_REPLACEMENT_ALLOC inline void operator delete[]( void* Ptr, size_t )
{
	guardSlow(delete);
	appFree( Ptr );
	unguardSlow;
}
#endif

/*-----------------------------------------------------------------------------
	Math.
-----------------------------------------------------------------------------*/

CORE_API BYTE appCeilLogTwo( DWORD Arg );

/*-----------------------------------------------------------------------------
	MD5 functions.
-----------------------------------------------------------------------------*/

//
// MD5 Context.
//
struct FMD5Context
{
	DWORD state[4];
	DWORD count[2];
	BYTE buffer[64];
};

//
// MD5 functions.
//!!it would be cool if these were implemented as subclasses of
// FArchive.
//
CORE_API void appMD5Init( FMD5Context* context );
CORE_API void appMD5Update( FMD5Context* context, BYTE* input, INT inputLen );
CORE_API void appMD5Update32( FMD5Context* context, INT val);
CORE_API void appMD5Update32( FMD5Context* context, DWORD val);
CORE_API void appMD5Update16( FMD5Context* context, _WORD val);
CORE_API void appMD5Final( BYTE* digest, FMD5Context* context );
CORE_API void appMD5Transform( DWORD* state, BYTE* block );
CORE_API void appMD5Encode( BYTE* output, DWORD* input, INT len );
CORE_API void appMD5Decode( DWORD* output, BYTE* input, INT len );

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

