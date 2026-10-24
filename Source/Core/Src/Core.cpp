/*=============================================================================
	Core.cpp: Unreal core.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	Temporary startup objects.
-----------------------------------------------------------------------------*/

// Error malloc.
class FMallocError : public FMalloc
{
	void Called( const TCHAR* Str )
		{appErrorf( TEXT("Called %s before memory init"), Str );}
	void* Malloc( size_t Count, const TCHAR* Tag )
		{Called(TEXT("appMalloc"));return NULL;}
	void* Realloc( void* Original, size_t Count, const TCHAR* Tag )
		{Called(TEXT("appRealloc"));return NULL;}
	void Free( void* Original )
		{Called(TEXT("appFree"));}
	void DumpAllocs()
		{Called(TEXT("appDumpAllocs"));}
	void HeapCheck()
		{Called(TEXT("appHeapCheck"));}
	void Init()
		{Called(TEXT("FMallocError::Init"));}
	void Exit()
		{Called(TEXT("FMallocError::Exit"));}
} MallocError;

// Error file manager.
class FFileManagerError : public FFileManager
{
public:
	FArchive* CreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
		{appErrorf(TEXT("Called FFileManagerError::CreateFileReader")); return 0;}
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
		{appErrorf(TEXT("Called FFileManagerError::CreateFileWriter")); return 0;}
	INT FileSize( const TCHAR* Filename )
		{return -1;}
	UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
		{return 0;}
	UBOOL IsReadOnly( const TCHAR* Filename ) // gam
		{return 0;}
	DWORD Copy( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0, DWORD Compress=FILECOPY_Normal, FCopyProgress* Progress=NULL )
		{return COPY_MiscFail;}
	UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 )
		{return 0;}
	SQWORD GetGlobalTime( const TCHAR* Filename )
		{return 0;}
	UBOOL SetGlobalTime( const TCHAR* Filename )
		{return 0;}
	UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )
		{return 0;}
	UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )
		{return 0;}
	TArray<FString> FindFiles( const TCHAR* Filename, UBOOL Files, UBOOL Directories )
		{return TArray<FString>();}
	UBOOL SetDefaultDirectory( const TCHAR* Filename )
		{return 0;}
	FString GetDefaultDirectory()
		{return TEXT("");}
	// gam ---
	INT CompareFileTimes( const TCHAR* FileA, const TCHAR* FileB )
    	{return 0;}
	// --- gam

} FileError;

// Critical error output device.
class CORE_API FErrorOutError : public FOutputDeviceError
{
public:
	void Serialize( const TCHAR* V, EName Event )
	{}
	void HandleError()
	{}
} ErrorOutError;

// Log output device.
class CORE_API FLogOutError : public FOutputDevice
{
public:
	void Serialize( const TCHAR* V, EName Event )
	{}
} LogOutError;

// Exception thrower.
class CORE_API FThrowOut : public FOutputDevice
{
public:
	void Serialize( const TCHAR* V, EName Event )
	{
		// Print the error before the throw: if nothing catches it, this line is the only trace of it.
		printf("[ENGINE-ERROR] %s\n", appToAnsi(V));
		throw( V );
	}
} ThrowOut;

// Null output device.
class CORE_API FNullOutError : public FOutputDevice
{
public:
	void Serialize( const TCHAR* V, enum EName Event )
	{}
} NullOutError;

// Dummy saver.
class CORE_API FArchiveDummySave : public FArchive
{
public:
	FArchiveDummySave() { ArIsSaving = 1; }
} GArchiveDummySave;

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

CORE_API FMemStack				GMem;							/* Global memory stack */
CORE_API FOutputDevice*			GLog=&LogOutError;				/* Regular logging */
CORE_API FOutputDeviceError*	GError=&ErrorOutError;			/* Critical errors */
CORE_API FOutputDevice*			GNull=&NullOutError;			/* Log to nowhere */
CORE_API FOutputDevice*			GThrow=&ThrowOut;				/* Exception thrower */
CORE_API FFeedbackContext*		GWarn=NULL;						/* User interaction and non critical warnings */
CORE_API FConfigCache*			GConfig=NULL;					/* Configuration database cache */
CORE_API FTransactionBase*		GUndo=NULL;						/* Transaction tracker, non-NULL when a transaction is in progress */
CORE_API FOutputDevice*			GLogHook=NULL;					/* Launch log output hook */
CORE_API FExec*					GExec=NULL;						/* Launch command-line exec hook */
CORE_API FMalloc*				GMalloc=&MallocError;			/* Memory allocator */
CORE_API FFileManager*			GFileManager=&FileError;		/* File manager */
CORE_API USystem*				GSys=NULL;						/* System control code */
CORE_API UProperty*				GProperty;						/* Property for UnrealScript interpretter */
CORE_API BYTE*					GPropAddr;						/* Property address for UnrealScript interpreter */
CORE_API UObject*				GPropObject;					/* Object with Property for UnrealScript interpreter */
CORE_API DWORD					GRuntimeUCFlags=0;				/* Property for storing flags between calls to bytecode functions */
CORE_API USubsystem*			GWindowManager=NULL;			/* Window update routine called once per tick */
CORE_API TCHAR					GErrorHist[4096]=TEXT("");		/* For building call stack text dump in guard/unguard mechanism */
CORE_API TCHAR					GYes[64]=TEXT("Yes");			/* Localized "yes" text */
CORE_API TCHAR					GNo[64]=TEXT("No");				/* Localized "no" text */
CORE_API TCHAR					GTrue[64]=TEXT("True");			/* Localized "true" text */
CORE_API TCHAR					GFalse[64]=TEXT("False");		/* Localized "false" text */
CORE_API TCHAR					GNone[64]=TEXT("None");			/* Localized "none" text */
CORE_API TCHAR                  GCdPath[256]=TEXT("");			/* Cd path, if any */
CORE_API DOUBLE					GSecondsPerCycle=1.0;			/* Seconds per CPU cycle for this PC */
CORE_API DOUBLE					GTempDouble=0.0;				/* Used during development for timing */
CORE_API void					(*GTempFunc)(void*)=NULL;		/* Used during development for debug hooks */
CORE_API QWORD					GTicks=1;						/* Number of non-persistent ticks thus far in this level, for profiling */
CORE_API INT					GScriptCycles;					/* Times script execution CPU cycles per tick */
CORE_API DWORD					GPageSize=4096;					/* Operating system page size */
CORE_API DWORD					GProcessorCount=1;				/* Number of CPUs in this PC */
CORE_API DWORD					GPhysicalMemory=16384*1024;		/* Bytes of physical memory in this PC */
CORE_API DWORD					GUglyHackFlags=0;               /* Flags for passing around globally hacked stuff */
CORE_API UBOOL					GHackNoDepthTestRenderTarget = 0; /* Hack to fix OpenGL render targets. */
CORE_API INT					GCDKS=0;						/* Defines the CDKey system's state */
CORE_API TCHAR					GBFKey[256]=TEXT("A1s46H#^d*cxNd85TN&DJ*(*X>?~@^*d");/* Defines the BlowFish key to use */
CORE_API DOUBLE					GBFTime;						/* How long ago was this key set */
CORE_API BYTE					GMD5[16];						/* Stores the GlobalMD5 of the game */
CORE_API UBOOL					GIsScriptable=0;				/* Whether script execution is allowed */
CORE_API UBOOL					GIsEditor=0;					/* Whether engine was launched for editing */
CORE_API UBOOL					GIsUCC=0;						/* Is UCC running? */
CORE_API UBOOL					GEdShowFogInViewports=1;		/* Show distance fog in viewports? */
CORE_API UBOOL					GEdSelectionLock=0;				/* Are selections locked? (you can't select/deselect additional actors) */
CORE_API UBOOL					GIsClient=0;					/* Whether engine was launched as a client */
CORE_API UBOOL					GIsServer=0;					/* Whether engine was launched as a server, true if GIsClient */
CORE_API UBOOL					GIsCriticalError=0;				/* An appError() has occured */
CORE_API UBOOL					GIsStarted=0;					/* Whether execution is happening from within main()/WinMain() */
CORE_API UBOOL					GIsGuarded=0;					/* Whether execution is happening within main()/WinMain()'s try/catch handler */
CORE_API UBOOL					GIsRunning=0;					/* Whether execution is happening within MainLoop() */
CORE_API UBOOL					GIsGarbageCollecting=0;			/* Whether we are inside garbage collection */
CORE_API UBOOL					GIsSlowTask=0;					/* Whether there is a slow task in progress */
CORE_API UBOOL					GIsRequestingExit=0;			/* Indicates that MainLoop() should be exited at the end of the current iteration */
CORE_API UBOOL					GIsStrict=0;					/* Causes all UnrealScript execution warnings to be fatal errors */
CORE_API UBOOL					GUfrontLog=0;					/* -UFRONTLOG: the port's diagnostic and repeated log lines are written too */
CORE_API UBOOL					GScriptEntryTag=0;				/* Number of recursive UnrealScript calls currently on the stack */
CORE_API UBOOL					GLazyLoad=0;					/* Whether TLazyLoad arrays should be lazy-loaded or not */
CORE_API UBOOL					GShowBuildLabel=1;				/* Used to show/hide the build label from private builds */
CORE_API UBOOL					GIsClocking=0;					/* Whether stats clocking is enabled */
CORE_API FGlobalMath			GMath;							/* Math code */
CORE_API FArchive*				GDummySave=&GArchiveDummySave;	/* No-op save archive */
CORE_API FLOAT					GAudioMaxRadiusMultiplier=100.f;/* Max distance = Radius * GAudioMaxRadiusMultiplier */
CORE_API FLOAT					GAudioDefaultRadius=80.f;		/* Default radius for PlayOwnedSound */
CORE_API TArray<FEdLoadError>	GEdLoadErrors;					/* For keeping track of load errors in the editor */
CORE_API UDebugger*				GDebugger=NULL;					//DEBUGGER /* Unrealscript Debugger */
CORE_API UBOOL                  GTransientNaming=0;             // sjs - enable this to generate unique names for transient objects
CORE_API UBOOL                  GIsSoaking=0;                   // gam - I like lentils.
CORE_API UBOOL					GIsBenchmarking=0;				/* Whether we are in benchmark mode or not */
CORE_API QWORD      		    GBaseCycles=0;                  // sjs - used for appSeconds delta
CORE_API QWORD					GMakeCacheIDIndex=0;			/* Cache ID */
// gam ---
CORE_API TCHAR                  GBuildLabel[1024]				= TEXT("No Label");
CORE_API TCHAR                  GMachineOS[1024]				= TEXT("No OS");
CORE_API TCHAR                  GMachineCPU[1024]				= TEXT("No CPU");
CORE_API TCHAR                  GMachineVideo[1024]				= TEXT("No Video");
// --- gam
CORE_API TCHAR					GIni[1024]						= TEXT("");
CORE_API TCHAR					GUserIni[1024]					= TEXT("");
CORE_API FLOAT					NEAR_CLIPPING_PLANE				= 10.0f;
CORE_API FLOAT					FAR_CLIPPING_PLANE				= 65536.f;
CORE_API UBOOL					GIsOpenGL						= 0;
CORE_API UBOOL					GIsOpenGLRevComps				= 0;
CORE_API UBOOL					GIsPixomatic					= 0;
CORE_API ERunningOS				GRunningOS						= OS_UNKNOWN;
CORE_API DWORD					GMemoryTag						= 0;
CORE_API DWORD					GAllocatedMemory				= 0;
CORE_API QWORD					GFileManagerBytesRead			= 0;
CORE_API FLOAT					GFileManagerLoadingTime			= 0;
CORE_API FLOAT					GFileManagerSeekTime			= 0;
CORE_API FLOAT					GFileManagerMiscTime			= 0;
CORE_API FLOAT					GDeltaTime						= 0;
CORE_API UBOOL					GUseFixedTimeStep				= 0;
CORE_API FLOAT					GFixedTimeStep					= 0;
CORE_API UBOOL					GIsRecordingMovie				= 0;
CORE_API DOUBLE					GCurrentTime					= 0;
CORE_API ERunningCPU			GRunningCPU						= CPU_UNKNOWN;
CORE_API DWORD					GGPUDeviceID					= 0;
CORE_API DWORD					GGPUVendorID					= 0;

#if MACOSX
CORE_API INT					GMacOSVer						= 0x0000;
#endif

CORE_API UBOOL					GDemoPlayback					= false;

CORE_API TCHAR					GModName[256]						= TEXT("");
CORE_API TCHAR					GModPath[1024]						= TEXT("");
CORE_API TCHAR					GModDef[1024]						= TEXT("");


CORE_API TCHAR					GCPUType[255]					= TEXT("oggvorbisUTV2004DefOpenAL32dinterfaceImpersonatorLib_rdIFC23libMySQLMSVCR71pixomatic_debugCorevorbisfileEngineWindowIpDrvUWebXGameFireEditorALAudioD3DDrvXInterfaceWinrvMasterServerOpenGLDrvPixoDrvOnslaughtD3D9DrvGUIDesigner");

// Unicode.
#if UNICODE
CORE_API UBOOL GUnicode=1;
CORE_API UBOOL GUnicodeOS=0;
#else
CORE_API UBOOL GUnicode=0;
CORE_API UBOOL GUnicodeOS=0;
#endif

// System identification.

#ifndef GIsMMX
CORE_API UBOOL GIsMMX=0;
#endif

#ifndef GIsSSE
CORE_API UBOOL GIsSSE=0;
#endif

#ifndef GIsAltivec
CORE_API UBOOL GIsAltivec=0; // PowerPC only. --ryan.
#endif

// For development.
UBOOL GNoGC=0;
UBOOL GCheckConflicts=0;
UBOOL GExitPurge=0;

// gam ---
// Converts a path like "..\Textures\MyFile.utx" to "C:\UT2003\Textures\MyFile.utx"
// DOES NOT convert a path like "..\Textures\..\MyFile.utx" to "C:\UT2003\MyFile.utx"
FString FFileManager::ExpandPath( const TCHAR* Path )
{
    guard(FFileManager::ExpandPath);
    
    if( appStrchr( Path, ':' ) != NULL )
        return( Path );

    if( ( Path[0] == '\\' ) || ( Path[0] == '/' ) )
        return( Path );
    
    FString NewPath = GetDefaultDirectory();
    const TCHAR* ParsePoint = Path;

    {
        INT SlashPoint = Max( NewPath.InStr( TEXT("\\"), 1 ), NewPath.InStr( TEXT("/"), 1 ) );

        if( SlashPoint == NewPath.Len() - 1 )
            NewPath = NewPath.Left( SlashPoint );
    }

    for(;;)
    {
        if( (appStrncmp( ParsePoint, TEXT("..\\"), 3 ) == 0) || (appStrncmp( ParsePoint, TEXT("../"), 3 ) == 0) )
        {
            INT SlashPoint = Max( NewPath.InStr( TEXT("\\"), 1 ), NewPath.InStr( TEXT("/"), 1 ) );

            if( SlashPoint > 0 )
                NewPath = NewPath.Left( SlashPoint );

            ParsePoint += 3;
        }
        else if( (appStrncmp( ParsePoint, TEXT(".\\"), 2 ) == 0) || (appStrncmp( ParsePoint, TEXT("./"), 2 ) == 0) )
            ParsePoint += 2;
        else
        {
            return( NewPath + PATH_SEPARATOR + ParsePoint );        
        }
    }        
    
    unguard;
}
// --- gam

/*-----------------------------------------------------------------------------
	Package implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_PACKAGE(Core);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

