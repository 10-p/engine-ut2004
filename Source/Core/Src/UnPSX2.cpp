/*=============================================================================
	UnPSX2: PSX2 port of UnVcWin32.cpp.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

#ifdef __PSX2_EE__

// Standard includes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>
#include <time.h>

// System includes.
#include <unistd.h>
#include <utime.h>
#include <sys/time.h>
#include <sys/stat.h>

// Core includes.
#include "CorePrivate.h"

// PSX2 includes.
#include <eekernel.h>
#include <sifdev.h>
#include <sifrpc.h>
#include <sifcmd.h>

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

extern char**         environ;
ANSICHAR              GModule[32];
static bool           HandlingSignal = 0;
static volatile QWORD GCycAcc        = 0;
static INT            GFrameCount    = 0;

/*-----------------------------------------------------------------------------
	USystem.
-----------------------------------------------------------------------------*/

//
// System manager.
//
static void Recurse()
{
	guard(Recurse);
	Recurse();
	unguard;
}
USystem::USystem()
:	SavePath	( E_NoInit )
,	CachePath	( E_NoInit )
,	CacheExt	( E_NoInit )
,	Paths		( E_NoInit )
,	Suppress	( E_NoInit )
,   CacheRecordPath( E_NoInit )
{}
void USystem::StaticConstructor()
{
	guard(USystem::StaticConstructor);

	new(GetClass(),TEXT("PurgeCacheDays"),      RF_Public)UIntProperty   (CPP_PROPERTY(PurgeCacheDays    ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("SavePath"),            RF_Public)UStrProperty   (CPP_PROPERTY(SavePath          ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("CachePath"),           RF_Public)UStrProperty   (CPP_PROPERTY(CachePath         ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("CacheExt"),            RF_Public)UStrProperty   (CPP_PROPERTY(CacheExt          ), TEXT("Options"), CPF_Config );
//	new(GetClass(),TEXT("CacheRecordPath"),     RF_Public)UStrProperty   (CPP_PROPERTY(CacheRecordPath   ), TEXT("Options"), CPF_Config );

	UArrayProperty* A = new(GetClass(),TEXT("Paths"),RF_Public)UArrayProperty( CPP_PROPERTY(Paths), TEXT("Options"), CPF_Config );
	A->Inner = new(A,TEXT("StrProperty0"),RF_Public)UStrProperty;

	UArrayProperty* B = new(GetClass(),TEXT("Suppress"),RF_Public)UArrayProperty( CPP_PROPERTY(Suppress), TEXT("Options"), CPF_Config );
	B->Inner = new(B,TEXT("NameProperty0"),RF_Public)UNameProperty;

	UArrayProperty* C = new(GetClass(),TEXT("CacheRecordPath"),RF_Public)UArrayProperty( CPP_PROPERTY(CacheRecordPath), TEXT("Options"), CPF_Config );
	C->Inner = new(C,TEXT("StrProperty1"),RF_Public)UStrProperty;

	unguard;
}
UBOOL USystem::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( ParseCommand(&Cmd,TEXT("EXIT")) )
	{
		Ar.Log( TEXT("Closing by request") );
		appRequestExit( 0 );
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("DEBUG") ) )
	{
		if( ParseCommand(&Cmd,TEXT("CRASH")) )
		{
			appErrorf( TEXT("%s"), TEXT("Unreal crashed at your request") );
			return 1;
		}
		else if( ParseCommand( &Cmd, TEXT("GPF") ) )
		{
			Ar.Log( TEXT("Unreal crashing with voluntary GPF") );
			*(int *)NULL = 123;
			return 1;
		}
		else if( ParseCommand( &Cmd, TEXT("RECURSE") ) )
		{
			Ar.Logf( TEXT("Recursing") );
			Recurse();
			return 1;
		}
		else if( ParseCommand( &Cmd, TEXT("EATMEM") ) )
		{
			Ar.Log( TEXT("Eating up all available memory") );
			while( 1 )
			{
				void* Eat = GMalloc->Realloc(NULL,65536,TEXT("EatMem"));
				memset( Eat, 0, 65536 );
			}
			return 1;
		}
		else return 0;
	}
	else return 0;
}
IMPLEMENT_CLASS(USystem);

/*-----------------------------------------------------------------------------
	Exit.
-----------------------------------------------------------------------------*/

//
// Immediate exit.
//
CORE_API void appRequestExit( UBOOL Force )
{
	GIsRequestingExit = 1;
}

/*-----------------------------------------------------------------------------
	Clipboard.
-----------------------------------------------------------------------------*/

CORE_API void ClipboardCopy( const TCHAR* Str )
{
	// Does not need to be implemented.
}
CORE_API void ClipboardPaste( FString& Result )
{
	// Does not need to be implemented.
}

/*-----------------------------------------------------------------------------
	Shared libraries.
-----------------------------------------------------------------------------*/

//
// Load a library.
//
void* appGetDllHandle( const TCHAR* Filename )
{
	return (void*)-1;
}

//
// Free a library.
//
void appFreeDllHandle( void* DllHandle )
{
	// Does not need to be implemented.
}

//
// Lookup the address of a shared library function.
//
void* appGetDllExport( void* DllHandle, const TCHAR* ProcName )
{
	// Does not need to be implemented.
}

//
// Break the debugger.
//
void appDebugBreak()
{
	// Does not need to be implemented.
}

//
// IsDebuggerPresent()
//
UBOOL IsDebuggerPresent()
{
	// Does not need to be implemented.
	return 0;
}

/*-----------------------------------------------------------------------------
	External processes.
-----------------------------------------------------------------------------*/

void* appCreateProc( const TCHAR* URL, const TCHAR* Parms )
{
	// Does not need to be implemented.
}

UBOOL appGetProcReturnCode( void* ProcHandle, INT* ReturnCode )
{
	// Does not need to be implemented.
	return 0;
}

/*-----------------------------------------------------------------------------
	Timing.
-----------------------------------------------------------------------------*/

//
// String timestamp.
//
CORE_API const TCHAR* appTimestamp()
{
	// Does not need to be implemented.
	return TEXT("");
}

//
// Get file time.
//
CORE_API DWORD appGetTime( const TCHAR* Filename )
{
	// Does not need to be implemented.
	return 0;
}

//
// Get time in seconds.
//
//CORE_API FTime appSecondsSlow()
CORE_API DOUBLE appSecondsSlow()
{
	return appSeconds();
}

//CORE_API FTime appSeconds()
CORE_API DOUBLE appSeconds()
{
	// Based on 300 MHz PlayStation 2; overflows in 512 days.
	DWORD CurCyc=0;
	asm volatile("mfc0 %0,$9\n" : "=r" (CurCyc) );
	QWORD NewCyc = GCycAcc + CurCyc;
	return (DOUBLE)(NewCyc*14.0f/FIXTIME + (NewCyc*95.0f/(300.0f*FIXTIME)));
}
CORE_API DWORD appCycles()
{
	DWORD CurCyc=0;
	asm volatile("mfc0 %0,$9\n" : "=r" (CurCyc) );
	return GCycAcc+CurCyc;
}
int BlankHandler(int ca)
{
	DWORD CurCyc=0,NewCyc=0;
	asm volatile("mfc0 %0,$9\n" : "=r" (CurCyc) );
	asm volatile("mtc0 %0,$9\n" : : "r" (NewCyc) );
	GCycAcc += CurCyc;
//	ExitHandler();
}

//
// Return the system time.
//
CORE_API void appSystemTime( INT& Year, INT& Month, INT& DayOfWeek, INT& Day, INT& Hour, INT& Min, INT& Sec, INT& MSec )
{
	// Does not need to be implemented.
}

CORE_API void appSleep( FLOAT Seconds )
{
	guard(appSleep);
//	FTime CurTime = appSeconds();
	DOUBLE CurTime = appSeconds();
	while(appSeconds() - CurTime < Seconds);
	unguard;
}

/*-----------------------------------------------------------------------------
	Link functions.
-----------------------------------------------------------------------------*/

//
// Launch a uniform resource locator (i.e. http://www.epicgames.com/unreal).
// This is expected to return immediately as the URL is launched by another
// task.
//
void appLaunchURL( const TCHAR* URL, const TCHAR* Parms, FString* Error )
{
	// Does not need to be implemented.
}

/*-----------------------------------------------------------------------------
	File finding.
-----------------------------------------------------------------------------*/

//
// Clean out the file cache.
//
static INT GetFileAgeDays( const TCHAR* Filename )
{
	// Does not need to be implemented.
	return 0;
}

CORE_API void appCleanFileCache()
{
	// Does not need to be implemented.
}

/*-----------------------------------------------------------------------------
	Guids.
-----------------------------------------------------------------------------*/

//
// Create a new globally unique identifier.
//
CORE_API FGuid appCreateGuid()
{
	guard(appCreateGuid);
	FGuid Result;
//	appGetGUID( (void*)&Result );
	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	Clipboard.
-----------------------------------------------------------------------------*/

static FString ClipboardText;
CORE_API void appClipboardCopy( const TCHAR* Str )
{
	guard(appClipboardCopy);
	ClipboardText = FString( Str );
	unguard;
}

CORE_API FString appClipboardPaste()
{
	guard(appClipboardPaste);
	return ClipboardText;
	unguard;
}

/*-----------------------------------------------------------------------------
	Command line.
-----------------------------------------------------------------------------*/

// Get startup directory.
CORE_API const TCHAR* appBaseDir()
{
	guard(appBaseDir);
	static TCHAR BaseDir[32]=TEXT("");
	BaseDir[0] = '\0';
	return BaseDir;
	unguard;
}

// Get computer name.
CORE_API const TCHAR* appComputerName()
{
	return "Computer";
}

// Get user name.
CORE_API const TCHAR* appUserName()
{
	return "User";
}

// Get launch package base name.
CORE_API const TCHAR* appPackage()
{
	return GModule;
}

/*-----------------------------------------------------------------------------
	App init/exit.
-----------------------------------------------------------------------------*/

//
// Platform specific initialization.
//
void appPlatformPreInit()
{
	// Does not need to be implemented.
}
void appPlatformInit()
{
	guard(appPlatformInit);

	// System initialization.
	GSys = new USystem;
	GSys->AddToRoot();
	for( INT i=0; i<GSys->Suppress.Num(); i++ )
		GSys->Suppress(i).SetFlags( RF_Suppress );

	// CPU speed.
	GSecondsPerCycle = 1.0f / 300000000.0f;

	// Determine # of Cycles per HSync.
	AddIntcHandler(INTC_VBLANK_S, BlankHandler, 0);
	EnableIntc(INTC_VBLANK_S);

	unguard;
}
void appPlatformPreExit()
{
	// Does not need to be implemented.
}
void appPlatformExit()
{
	// Does not need to be implemented.
}
void appEnableFastMath( UBOOL Enable )
{
	// Does not need to be implemented.
}

/*-----------------------------------------------------------------------------
	Pathnames.
-----------------------------------------------------------------------------*/

// Convert pathname to Unix format.
char* appUnixPath( const TCHAR* Path )
{
	guard(appUnixPath);
	TCHAR* UnixPath = appStaticString1024();
	TCHAR* Cur = UnixPath;
	appStrncpy( UnixPath, Path, 1024 );
	while( Cur = strchr( Cur, '\\' ) )
		*Cur = '/';
	return UnixPath;
	unguard;
}

/*-----------------------------------------------------------------------------
	Networking.
-----------------------------------------------------------------------------*/

DWORD appGetLocalIP( void )
{
	// Does not need to be implemented.
	return 0;
}

/*-----------------------------------------------------------------------------
	String functions.
-----------------------------------------------------------------------------*/

int stricmp( const char* s, const char* t )
{
	int	i;
	for( i = 0; tolower(s[i]) == tolower(t[i]); i++ )
		if( s[i] == '\0' )
			return 0;
	return s[i] - t[i];
}

int strnicmp( const char* s, const char* t, int n )
{
	int	i;
	if( n <= 0 )
		return 0;
	for( i = 0; tolower(s[i]) == tolower(t[i]); i++ )
		if( (s[i] == '\0') || (i == n - 1) )
			return 0;
	return s[i] - t[i];
}

char* strupr( char* s )
{
	int	i;
	for( i = 0; s[i] != '\0'; i++ )
		s[i] = toupper(s[i]);
	return s;
}

/*-----------------------------------------------------------------------------
	Shift-JIS2 conversion functions
-----------------------------------------------------------------------------*/

unsigned short ascii_special[33][2] = {
	{0x8140, 32},		/*   */
	{0x8149, 33},		/* ! */
	{0x8168, 34},		/* " */
	{0x8194, 35},		/* # */
	{0x8190, 36},		/* $ */
	{0x8193, 37},		/* % */
	{0x8195, 38},		/* & */
	{0x8166, 39},		/* ' */
	{0x8169, 40},		/* ( */
	{0x816a, 41},		/* ) */
	{0x8196, 42},		/* * */
	{0x817b, 43},		/* + */
	{0x8143, 44},		/* , */
	{0x817c, 45},		/* - */
	{0x8144, 46},		/* . */
	{0x815e, 47},		/* / */
	{0x8146, 58},		/* : */
	{0x8147, 59},		/* ; */
	{0x8171, 60},		/* < */
	{0x8181, 61},		/* = */
	{0x8172, 62},		/* > */
	{0x8148, 63},		/* ? */
	{0x8197, 64},		/* @ */
	{0x816d, 91},		/* [ */
	{0x818f, 92},		/* \ */
	{0x816e, 93},		/* ] */
	{0x814f, 94},		/* ^ */
	{0x8151, 95},		/* _ */
	{0x8165, 96},		/* ` */
	{0x816f, 123},		/* { */
	{0x8162, 124},		/* | */
	{0x8170, 125},		/* } */
	{0x8150, 126},		/* ~ */
};

static unsigned short ascii_table[3][2] = {
	{0x824f, 0x30},	/* 0-9  */
	{0x8260, 0x41},	/* A-Z  */
	{0x8281, 0x61},	/* a-z  */
};

char appSjis2Ascii(const unsigned char *character)
{
	unsigned char byte1 = (unsigned char)*character;
	unsigned char byte2 = (unsigned char)*(character + 1);
    char output;
	int i;


	if(byte1 == 0x81 || byte1 == 0x82)
	{
		if(byte1 == 0x82)
		{
			if(((byte2 >= 0x4f) && (byte2 <= 0x59)) || ((byte2 >= 0x60) && (byte2 <= 0x7a)))
				output = byte2 - 0x1f;

			else if((byte2 >= 0x81) && (byte2 <= 0x9b))
				output = byte2 - 0x20;

			else
			{
    			return 0;
			}
		}
		
		else
		{
			for(i = 0; i < 33; i++)
			{
				if(byte2 == (ascii_special[i][0] & 0x00ff))
				{			 
					output = ascii_special[i][1];
					break;
				}
			}
			
			if(i == 33)
			{
				output = 0;
				return 0;
			}
		}
  	}
	
	else
	{
    	return 0;
	}
    
    return output;	
}
void appSjis2AsciiString(const unsigned char *title, char *string)
{
	int i = 0;
	int length;
	char temp;

	length = strlen((char *)title) / 2;

	for(i = 0; i < length; i++)
	{
		if((temp = appSjis2Ascii(title)) == 0)
		{
			strcpy(string, ".Kanji.");
			i = 7;
			break;
		}

		string[i] = temp;
		title += 2;
  	}
	string[i] = 0x00;
}
int appIsSjis(unsigned char *title)
{
    if(((*title >= 129) && (*title <= 159)) || ((*title >= 224) && (*title <= 239)))
    	return 1;
    
    else if(*title < 129)
    	return 0;
    
    else
    	return -1;
}
long appIsAscii(char *c)
{
    if(!(*c >> 7))	
    	return 1;
    else
    	return 0;
}
u_short appAscii2Sjis(unsigned char ascii_code)
{
	u_short sjis_code = 0;
	unsigned char stmp;
	unsigned char stmp2 = 0;

	if((ascii_code >= 0x20) && (ascii_code <= 0x2f))
		stmp2 = 1;
	
	else if((ascii_code >= 0x30) && (ascii_code <= 0x39))
		stmp = 0;
	
	else if((ascii_code >= 0x3a) && (ascii_code <= 0x40))
		stmp2 = 11;
	
	else if((ascii_code >= 0x41) && (ascii_code <= 0x5a))
		stmp = 1;
	
	else if((ascii_code >= 0x5b) && (ascii_code <= 0x60))
		stmp2 = 37;
	
	else if((ascii_code >= 0x61) && (ascii_code <= 0x7a))
		stmp = 2;
	
	else if((ascii_code >= 0x7b) && (ascii_code <= 0x7e))
		stmp2 = 63;
	
	else 
	{
		printf("bad ASCII code 0x%x\n", ascii_code);
		return 0;
	}

	if (stmp2)
	   	sjis_code = ascii_special[ascii_code - 0x20 - (stmp2 - 1)][0];
	else
		sjis_code = ascii_table[stmp][0] + ascii_code - ascii_table[stmp][1];

	return sjis_code;
}
void appAsciiString2Sjis(const u_char *input, u_short *output)
{
	int i=0;
	int len;
	u_short sjis;
	u_char temp1, temp2;

	len = strlen((char *)input);
	for(i = 0; i < len; i++)
	{
		sjis = appAscii2Sjis(input[i]);
		temp1 = sjis;  
		temp2 = sjis >> 8;
		output[i] = temp2 | (temp1 << 8);
   	}

	output[i] = 0x0000;
}
short appSwapShort(u_short input)
{
	u_char temp1, temp2;
	return (input >> 8) | (input << 8);
}

void appGetGUID( void* GUID )
{ }

const void EdLoadErrorf( INT Type, const TCHAR* Fmt, ... )
{ }

/*-----------------------------------------------------------------------------
	Signal Handling
-----------------------------------------------------------------------------*/
#if !__MWERKS__
jmp_buf __Context::Env;
struct sigaction __Context::Act_SIGHUP;
struct sigaction __Context::Act_SIGQUIT;
struct sigaction __Context::Act_SIGILL;
struct sigaction __Context::Act_SIGTRAP;
struct sigaction __Context::Act_SIGIOT;
struct sigaction __Context::Act_SIGBUS;
struct sigaction __Context::Act_SIGFPE;
struct sigaction __Context::Act_SIGSEGV;
struct sigaction __Context::Act_SIGTERM;
void __Context::StaticInit() {}
void __Context::HandleSignal( int Sig ) {longjmp( Env, 1 );}
#endif

/*-----------------------------------------------------------------------------
	File Streaming.
-----------------------------------------------------------------------------*/

UBOOL FFileStream::Read( INT StreamId, INT Bytes )
{
/*
	if ( Streams[StreamId].Handle && Streams[StreamId].Data )
	{
		switch ( Streams[StreamId].Type )
		{
			case ST_Regular:	
			{
				INT Count=0;
				UBOOL RetVal = ReadFile( Streams[StreamId].Handle, Streams[StreamId].Data, Bytes, (DWORD*) &Count, NULL );
				if (RetVal)
					Streams[StreamId].Data = (BYTE*)(Streams[StreamId].Data) + Count;
				if (Count != Bytes)
					Streams[StreamId].EndOfFile = 1;
				return RetVal;
			}
			case ST_Ogg:
			{
#ifndef _XBOX
				long Count = 0;
				while ( Count < Bytes )
				{
					long Read = ov_read( 
						(OggVorbis_File*) Streams[StreamId].TDD, 
						((char*) Streams[StreamId].Data) + Count, 
						Bytes - Count, 
						0,
						2,
						1, 
						&Streams[StreamId].FileSeek
					);
					if ( Read == 0 )
					{
						// Filling the rest of the buffer with silence. Note that memset is used for reentrance reasons.
						memset( ((char*) Streams[StreamId].Data) + Count, 0 , Bytes - Count );
						Streams[StreamId].EndOfFile = 1;
						return false;
					}
					else if ( Read < 0 )
					{
						return false;
					}
					else
					{
						Count += Read;
					}
				}
#endif
				return true;
			}
			default:
					return false;
		}
	}
	else
	{
		return false;
	}
*/
	return false;
}
	
UBOOL FFileStream::Create( INT StreamId, const TCHAR* Filename )
{
/*
	switch ( Streams[StreamId].Type )
	{
		case ST_Regular:
		{
			DWORD  Access    = GENERIC_READ;
			DWORD  WinFlags  = FILE_SHARE_READ;
			DWORD  Create    = OPEN_EXISTING;

#ifndef _XBOX
			HANDLE Handle    = TCHAR_CALL_OS(
				CreateFileW( Filename, Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL ), 
				CreateFileA( TCHAR_TO_ANSI(Filename), Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL ) 
			);
#else
			HANDLE Handle    = CreateFile( TCHAR_TO_ANSI(Filename), Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL );
#endif

			if( Handle == INVALID_HANDLE_VALUE )
			{
				Streams[StreamId].Handle = NULL;
				return false;
			}
			else
			{
				Streams[StreamId].Handle = Handle;
					return true;
			}
		}
		case ST_Ogg:
		{
#ifndef _XBOX
			void* Handle = TCHAR_CALL_OS( _wfopen(Filename,TEXT("rb")), fopen(TCHAR_TO_ANSI(Filename),"rb") );
			if ( Handle == NULL ) 
				return false;
			INT Error = ov_open( (FILE*) Handle, (OggVorbis_File*) Streams[StreamId].TDD, NULL, 0 );
			if (Error < 0)
				return false;
			else
			{
				Streams[StreamId].Handle = Handle;
				return true;
			}
#else
			return false;
#endif
		}
		default:
			return false;
	}
*/
	return false;
}
	
UBOOL FFileStream::Destroy( INT StreamId )
{
/*
	switch ( Streams[StreamId].Type )
	{
		case ST_Regular:
		{	
			if ( Streams[StreamId].Handle )
				CloseHandle( Streams[StreamId].Handle );
			Streams[StreamId].TDD		= NULL;
			Streams[StreamId].Handle	= NULL;
			Streams[StreamId].Used		= 0;
			return true;
		}
		case ST_Ogg:
		{
#ifndef _XBOX
			if ( Streams[StreamId].TDD )
				ov_clear( (OggVorbis_File*) Streams[StreamId].TDD );
			Streams[StreamId].TDD		= NULL;
			Streams[StreamId].Handle	= NULL;
			Streams[StreamId].Used		= 0;
			return true;
#else	
			return true;
#endif
		}
		default:
			return false;
	}
*/
	return false;
}

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

