/*=============================================================================
	UnPSX2: PSX2 port of UnVcWin32.cpp.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Brandon Reinhart.
=============================================================================*/

#ifdef __GCN__

// Standard includes.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <float.h>

#include <dolphin/os.h>

// Core includes.
#include "CorePrivate.h"


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

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
	return NULL;
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
	return NULL;
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
	OSTime Time = OSGetTime();
	return (DOUBLE)Time / (DOUBLE)OS_TIMER_CLOCK;
//	return OSTicksToSeconds(Time);
}
CORE_API DWORD appCycles()
{
	OSTime Time = OSGetTime();
	return OSTicksToMicroseconds(Time);
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
	return TEXT("System");
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
	GSecondsPerCycle = 1.0f / 1000000.0f;

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

void appGetGUID( void* GUID )
{ }

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

CORE_API const void EdLoadErrorf( INT Type, const TCHAR* Fmt, ... )
{ }


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

