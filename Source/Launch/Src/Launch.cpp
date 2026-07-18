/*=============================================================================
	Launch.cpp: Game launcher.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "LaunchPrivate.h"
#include "UnEngineWin.h"

#ifndef _DEBUG
#pragma pack(push,8)
#include <DbgHelp.h>
#pragma pack(pop)
#endif

#if __STATIC_LINK
#include "Engine.h"
#include "UnRender.h"
#include "UnNullRenderDevice.h"
#include "WinDrv.h"
#include "OpenGLDrv.h"

#ifndef _WIN64
//#include "D3DDrv.h"
extern "C" { void autoInitializeRegistrantsD3DDrv(void); }
#define AUTO_INITIALIZE_REGISTRANTS_D3DDRV autoInitializeRegistrantsD3DDrv();
#endif

//#include "D3D9Drv.h"
extern "C" { void autoInitializeRegistrantsD3D9Drv(void); }
#define AUTO_INITIALIZE_REGISTRANTS_D3D9DRV autoInitializeRegistrantsD3D9Drv();

#include "UnNet.h"
#include "UnFractal.h"
#include "ALAudio.h"
#include "UnIpDrv.h"
#include "UnTcpNetDriver.h"
#include "UnIpDrvCommandlets.h"
#include "UnIpDrvNative.h"
#include "XInterface.h"
#include "UWeb.h"
#include "UWebNative.h"
#include "OnslaughtPrivate.h"
void autoInitializeRegistrantsEditor(INT &Lookup);

#if USE_PIXOMATIC
extern "C" { void autoInitializeRegistrantsPixoDrv(void); }
#define AUTO_INITIALIZE_REGISTRANTS_PIXODRV autoInitializeRegistrantsPixoDrv();
#endif

//!!! FIXME: linker removes this as deadcode on MacOS X without an explicit reference...
extern "C" {
extern BYTE GLoadedCore;
extern BYTE GLoadedEngine;
extern BYTE GLoadedWinDrv;
extern BYTE GLoadedFire;
extern BYTE GLoadedALAudio;
extern BYTE GLoadedD3D9Drv;
extern BYTE GLoadedOpenGLDrv;
extern BYTE GLoadedIpDrv;
extern BYTE GLoadedXInterface;
extern BYTE GLoadedXGame;
extern BYTE GLoadedUWeb;
extern BYTE GLoadedOnslaught;
extern BYTE GLoadedEditor;

#ifndef WIN64
extern BYTE GLoadedD3DDrv;
#endif
#if USE_PIXOMATIC
extern BYTE GLoadedPixoDrv;
#endif
}

#endif


/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

// General.
extern "C" {HINSTANCE hInstance;}
extern "C" {TCHAR GPackage[64]=TEXT("Launch");}
static void SetInitialConfiguration();

// Memory allocator.
// !!! FIXME: Why does FMallocWindows not work on Win64?  --ryan.
#ifdef _WIN64
  #include <malloc.h>
  #include "FMallocAnsi.h"
  FMallocAnsi Malloc;
#else
  #ifdef _DEBUG
	#include "FMallocDebug.h"
	FMallocDebug Malloc;
  #else
	#include "FMallocWindows.h"
	FMallocWindows Malloc;
  #endif
#endif

// Log file.
#include "FOutputDeviceFile.h"
FOutputDeviceFile Log;

// Error handler.
#include "FOutputDeviceWindowsError.h"
FOutputDeviceWindowsError Error;

// Feedback.
#include "FFeedbackContextWindows.h"
FFeedbackContextWindows Warn;

// File manager.
#include "FFileManagerWindows.h"
FFileManagerWindows FileManager;

// Config.
#include "FConfigCacheIni.h"

/*-----------------------------------------------------------------------------
	WinMain.
-----------------------------------------------------------------------------*/

//
// Guarded main loop.
//
static TCHAR* CmdLine = NULL;
static void GuardedMain()
{
	// Init core.
	GIsClient = 1;
	appInit( GPackage, CmdLine, &Malloc, &Log, &Error, &Warn, &FileManager, FConfigCacheIni::Factory, 1 );

	// Launch the bug report monitor.
#ifndef _DEBUG
#if 0
	const TCHAR* Parameters[] = { TEXT("BugReport"), appItoa(GetCurrentProcessId()), NULL };
	_wspawnv(
		_P_NOWAIT,
		TEXT("BugReport"),
		Parameters
		);
#endif
#endif

#if __STATIC_LINK
        // Clean lookups.
        for( INT k=0; k<ARRAY_COUNT(GNativeLookupFuncs); k++ )
            GNativeLookupFuncs[k] = NULL;

        INT Lookup = 0;
        // Core natives.
        GNativeLookupFuncs[Lookup++] = &FindCoreUObjectNative;
        GNativeLookupFuncs[Lookup++] = &FindCoreUCommandletNative;

        // auto-generated lookups and statics
        AUTO_INITIALIZE_REGISTRANTS_ENGINE;
        AUTO_INITIALIZE_REGISTRANTS_WINDRV;
        AUTO_INITIALIZE_REGISTRANTS_FIRE;
        AUTO_INITIALIZE_REGISTRANTS_ALAUDIO;
        #ifndef WIN64
        AUTO_INITIALIZE_REGISTRANTS_D3DDRV;
        #endif
        AUTO_INITIALIZE_REGISTRANTS_D3D9DRV;
        AUTO_INITIALIZE_REGISTRANTS_OPENGLDRV;
        #if USE_PIXOMATIC
        AUTO_INITIALIZE_REGISTRANTS_PIXODRV;
        #endif
        AUTO_INITIALIZE_REGISTRANTS_IPDRV;
        RegisterNamesXInterface();
        AUTO_INITIALIZE_REGISTRANTS_XINTERFACE;
        AUTO_INITIALIZE_REGISTRANTS_XGAME;
        AUTO_INITIALIZE_REGISTRANTS_UWEB;
        RegisterNamesOnslaught();
        AUTO_INITIALIZE_REGISTRANTS_ONSLAUGHT;
        autoInitializeRegistrantsEditor(Lookup);

        GNativeLookupFuncs[Lookup++] = &FindUWebUWebResponseNative;
        GNativeLookupFuncs[Lookup++] = &FindUWebUWebRequestNative;
        check( Lookup < ARRAY_COUNT(GNativeLookupFuncs) );


//!!! FIXME: linker removes this as deadcode on MacOS X without an explicit reference...
static char blah[16];
sprintf(blah, "%d", (int) GLoadedCore);
sprintf(blah, "%d", (int) GLoadedEngine);
sprintf(blah, "%d", (int) GLoadedWinDrv);
sprintf(blah, "%d", (int) GLoadedFire);
sprintf(blah, "%d", (int) GLoadedALAudio);
sprintf(blah, "%d", (int) GLoadedD3D9Drv);
sprintf(blah, "%d", (int) GLoadedOpenGLDrv);
sprintf(blah, "%d", (int) GLoadedIpDrv);
sprintf(blah, "%d", (int) GLoadedXInterface);
sprintf(blah, "%d", (int) GLoadedXGame);
sprintf(blah, "%d", (int) GLoadedUWeb);
sprintf(blah, "%d", (int) GLoadedOnslaught);
sprintf(blah, "%d", (int) GLoadedEditor);

#ifndef WIN64
sprintf(blah, "%d", (int) GLoadedD3DDrv);
#endif
#if USE_PIXOMATIC
sprintf(blah, "%d", (int) GLoadedPixoDrv);
#endif
#endif


	// Init mode.
	GIsServer     = 1;
	GIsClient     = !ParseParam(appCmdLine(),TEXT("SERVER"));
	GIsEditor     = 0;
	GIsScriptable = 1;
	GLazyLoad     = !GIsClient || ParseParam(appCmdLine(),TEXT("LAZY"));

	// Figure out whether to show log or splash screen.
	UBOOL ShowLog = ParseParam(appCmdLine(),TEXT("LOG"));

	TCHAR LogoFName[1024];
	if( !Parse( appCmdLine(), TEXT("-USERLOGO="), LogoFName, 1024 ) )
		appSprintf( LogoFName, TEXT("%sLogo.bmp"), GPackage );

	FString Filename = FString(TEXT("..\\Help")) * LogoFName;

	if ( appStrlen(GModPath)>0 )
		Filename = FString(GModPath) * FString(TEXT("Help")) * GModName + TEXT("Logo.bmp");

	if( GFileManager->FileSize(*Filename)<0 )
		Filename = FString(TEXT("..\\Help")) * GPackage + TEXT("Logo.bmp"); // gam

	appStrcpy( GPackage, appPackage() );
	if( !ShowLog && !ParseParam(appCmdLine(),TEXT("server")) && !appStrfind(appCmdLine(),TEXT("TestRenDev")) )
    {
        debugf(TEXT("Splash screen filename is '%s'"), *Filename);
		InitSplash( *Filename );
    }

	// Set sane default values on initial run.
	INT FirstRun = 0;
	GConfig->GetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );
	if( GIsClient && (FirstRun==0) && !ParseParam(appCmdLine(),TEXT("BENCHMARK")) )
		SetInitialConfiguration();

	// Cheat Protection

	FString fsA = TEXT("ll");
	FString fsC = TEXT("eicnpogdrirenve");
	FString fsB = TEXT(".d");


	if ( ParseParam(appCmdLine(), TEXT("TIMELOAD")) )
		GIsRequestingExit = 1;

	// Exit if wanted.
	if( GIsRequestingExit )
	{
		ExitSplash();
		appPreExit();
		return; // appExit will be called outside guarded code
	}

	// Init windowing.
	InitWindowing();

	// Create log window, but only show it if ShowLog.
	GLogWindow = new WLog( Log.Filename, Log.LogAr, TEXT("GameLog") );
	GLogWindow->OpenWindow( ShowLog, 0 );
	GLogWindow->Log( NAME_Title, LocalizeGeneral(TEXT("Start"),GPackage) );
	if( GIsClient )
		SetPropX( *GLogWindow, TEXT("IsBrowser"), (HANDLE)1 );

	// Benchmarking/ movie recording.
	GIsBenchmarking		= ParseParam(appCmdLine(),TEXT("BENCHMARK"));
	GIsRecordingMovie	= ParseParam(appCmdLine(),TEXT("RECORDMOVIE"));
	
	// Fixed timestep.
	FLOAT FixedFPS		= 0.f;
	Parse(appCmdLine(),TEXT("FIXEDFPS="),FixedFPS);
	
	if( GIsRecordingMovie || GIsBenchmarking || FixedFPS > 0.f )
	{
		GUseFixedTimeStep	= 1;
		GFixedTimeStep		= FixedFPS > 0.f ? 1.f / FixedFPS : 1.f / 30.f;
	}

	// Render device command line options.
	if( ParseParam(appCmdLine(),TEXT("SOFTWARE")) )
	{
		GConfig->SetString( TEXT("Engine.Engine"		), TEXT("RenderDevice"		), TEXT("PixoDrv.PixoRenderDevice"	));

		GConfig->SetString( TEXT("WinDrv.WindowsClient"	), TEXT("Decals"			), TEXT("False"						));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"	), TEXT("DecoLayers"		), TEXT("False"						));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"	), TEXT("Coronas"			), TEXT("False"						));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"	), TEXT("Projectors"		), TEXT("False"						));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"	), TEXT("NoFractalAnim"		), TEXT("True"						));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"	), TEXT("NoDynamicLights"	), TEXT("True"						));
		GConfig->SetString( TEXT("UnrealGame.UnrealPawn"), TEXT("bPlayerShadows"	), TEXT("False"						), GUserIni );
		GConfig->SetString( TEXT("Engine.Vehicle"		), TEXT("bVehicleShadows"	), TEXT("False"						), GUserIni );
	}


	if( ParseParam(appCmdLine(),TEXT("OPENGL")) )
		GConfig->SetString( TEXT("Engine.Engine"		), TEXT("RenderDevice"		), TEXT("OpenGLDrv.OpenGLRenderDevice"	));

	if( ParseParam(appCmdLine(),TEXT("D3D")) || ParseParam(appCmdLine(),TEXT("D3D8")) )
		GConfig->SetString( TEXT("Engine.Engine"		), TEXT("RenderDevice"		), TEXT("D3DDrv.D3DRenderDevice"	));

	if( ParseParam(appCmdLine(),TEXT("D3D9")) )
		GConfig->SetString( TEXT("Engine.Engine"		), TEXT("RenderDevice"		), TEXT("D3D9Drv.D3D9RenderDevice"	));

	if( ParseParam(appCmdLine(),TEXT("NULLRENDER")) )
		GConfig->SetString( TEXT("Engine.Engine"		), TEXT("RenderDevice"		), TEXT("Engine.NullRenderDevice"	));

	// No Direct3D 8 on Win64, so force it to D3D9Drv if someone is using
	//  a legacy config, etc.
	#ifdef _WIN64
	if (appStricmp(GConfig->GetStr(TEXT("Engine.Engine"),TEXT("RenderDevice"),TEXT("System")),TEXT("D3DDrv.D3DRenderDevice"))==0)
	{
		if( !ParseParam(appCmdLine(),TEXT("NoForceD3D9Drv")) )
		{
			debugf(TEXT("Your ini had D3DDrv...Forcing use of D3D9Drv instead."));
			GConfig->SetString(TEXT("Engine.Engine"), TEXT("RenderDevice"), TEXT("D3D9Drv.D3D9RenderDevice"));
		}
	}
	#endif

	// Don't update ini files if benchmarking.
	if( GIsBenchmarking || ParseParam(appCmdLine(),TEXT("NOINI")) || ParseParam(appCmdLine(),TEXT("EXITAFTERDEMO")) )
	{
		GConfig->Detach( GIni );
		GConfig->Detach( GUserIni );
	}

	// Ugly resolution overriding code.
	FString ScreenWidth;
	FString ScreenHeight;
	UBOOL	OverrideResolution = false;

	//!!vogel: TODO: clean up this code :)
	if( ParseParam(appCmdLine(),TEXT("320x240")) )
	{
		ScreenWidth			= TEXT("320");
		ScreenHeight		= TEXT("240");
		OverrideResolution	= 1;
	}
	if( ParseParam(appCmdLine(),TEXT("512x384")) )
	{
		ScreenWidth			= TEXT("512");
		ScreenHeight		= TEXT("384");
		OverrideResolution	= 1;
	}
	if( ParseParam(appCmdLine(),TEXT("640x480")) )
	{
		ScreenWidth			= TEXT("640");
		ScreenHeight		= TEXT("480");
		OverrideResolution	= 1;
	}
	if( ParseParam(appCmdLine(),TEXT("800x600")) )
	{
		ScreenWidth			= TEXT("800");
		ScreenHeight		= TEXT("600");
		OverrideResolution	= 1;
	}
	if( ParseParam(appCmdLine(),TEXT("1024x768")) )
	{
		ScreenWidth			= TEXT("1024");
		ScreenHeight		= TEXT("768");
		OverrideResolution	= 1;
	}
	if( ParseParam(appCmdLine(),TEXT("1280x960")) )
	{
		ScreenWidth			= TEXT("1280");
		ScreenHeight		= TEXT("960");
		OverrideResolution	= 1;
	}

	// @@ Cheat Protection


    #if !__STATIC_LINK
			// Crc - Cryptic as possible
			
				TArray<FString> fn;
				new(fn)FString(TEXT(""));
				new(fn)FString(TEXT(""));
				new(fn)FString(TEXT(""));

				INT ch=0;
				for (INT Pss=0;Pss<6;Pss++)
				{
					fn(0) = FString::Printf(TEXT("%s%c"),*fn(0),fsC[ch++]);
					if (Pss<5)
						fn(1) = FString::Printf(TEXT("%s%c"),*fn(1),fsC[ch++]);
					if (Pss<4)
						fn(2) = FString::Printf(TEXT("%s%c"),*fn(2),fsC[ch++]);
				}

				fn(0) = FString::Printf(TEXT("%s%s%s"),*fn(0),*fsB,*fsA);
				fn(1) = FString::Printf(TEXT("%s%s%s"),*fn(1),*fsB,*fsA);
				fn(2) = FString::Printf(TEXT("%s%s%s"),*fn(2),*fsB,*fsA);

				USHORT C = 0xFFFF;
				USHORT T;
				USHORT Q;
			
				for (int i=0;i<fn.Num();i++)
				{
					FArchive* Ar = GFileManager->CreateFileReader( *fn(i) );
                    if (Ar == NULL)
                        continue;

					int B2R;

					BYTE* Bfr = (BYTE*)appMalloc(32767, TEXT(""));
					while ( Ar->Tell() < Ar->TotalSize() )
					{
						B2R = Ar->TotalSize() - Ar->Tell();
						if (B2R>32767)
							B2R=32767;

						Ar->Serialize(Bfr, B2R);
						for (int j=0;j<B2R;j++)
						{
							T = C ^ Bfr[j];
							C >>= 8;
							Q = ((T ^ (T << 4)))<<8;
							C ^=  Q;
							Q >>= 5;
							C ^=  Q;
							Q >>= 7;
							C ^=  Q;
						}

					}
					delete Ar;
				}
				GMD5[1] = (BYTE)(C & 0x00FF);
				GMD5[2] = (BYTE)(C >> 8);
    #endif


	// ----- End Cheat Protection

	if( ParseParam(appCmdLine(),TEXT("1280x1024")) )
	{
		ScreenWidth			= TEXT("1280");
		ScreenHeight		= TEXT("1024");
		OverrideResolution	= 1;
	}
	if( ParseParam(appCmdLine(),TEXT("1600x1200")) )
	{
		ScreenWidth			= TEXT("1600");
		ScreenHeight		= TEXT("1200");
		OverrideResolution	= 1;
	}

	if( OverrideResolution )
	{
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullscreenViewportX"), *ScreenWidth  );
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullscreenViewportY"), *ScreenHeight );
	}


	// Validate cd key
	if( !ValidateCDKey() )
	{
		GIsRequestingExit = 1;
		ExitSplash();
		appMsgf( 0, LocalizeError(TEXT("InvalidCDKey"),TEXT("Engine")) );
	}
	else
	{
		// Init engine.
		UEngine* Engine = InitEngine();
		if( Engine )
		{
#if DEMOVERSION
			Engine->DummyFunctionToBreakCompatibility(1);
#endif
			Engine->AnotherDummyFunctionToBreakCompatibility(1);
			Engine->ReallyBreakCompatibility(1);
			GLogWindow->Log( NAME_Title, LocalizeGeneral(TEXT("Run"),GPackage) );

			// Hide splash screen.
			ExitSplash();

			// Optionally Exec an exec file
			FString Temp;
			if( Parse(appCmdLine(), TEXT("EXEC="), Temp) )
			{
				Temp = FString(TEXT("exec ")) + Temp;
				if( Engine->Client && Engine->Client->Viewports.Num() && Engine->Client->Viewports(0) )
					Engine->Client->Viewports(0)->Exec( *Temp, *GLogWindow );
			}

			// Start main engine loop, including the Windows message pump.
			if( !GIsRequestingExit )
				MainLoop( Engine );
		}
	}

	// Clean shutdown.
	GFileManager->Delete(TEXT("Running.ini"),0,0);
	RemovePropX( *GLogWindow, TEXT("IsBrowser") );
	GLogWindow->Log( NAME_Title, LocalizeGeneral(TEXT("Exit"),GPackage) );
	delete GLogWindow;
	appPreExit();
}

//
// Exception handling.
//
static TCHAR MiniDumpFilenameW[64] = TEXT("");
static char  MiniDumpFilenameA[64] = "";		// can't use TCHAR_TO_ANSI in exception handler

#ifndef _DEBUG
static INT CreateMiniDump( LPEXCEPTION_POINTERS ExceptionInfo )
{
	HANDLE FileHandle	= TCHAR_CALL_OS( 
								CreateFileW( MiniDumpFilenameW, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL ), 
								CreateFileA( MiniDumpFilenameA, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL ) 
						);

	if( FileHandle )
	{
		MINIDUMP_EXCEPTION_INFORMATION DumpExceptionInfo;
	
		DumpExceptionInfo.ThreadId			= GetCurrentThreadId();
		DumpExceptionInfo.ExceptionPointers	= ExceptionInfo;
		DumpExceptionInfo.ClientPointers	= true;

		MiniDumpWriteDump( GetCurrentProcess(), GetCurrentProcessId(), FileHandle, MiniDumpNormal, &DumpExceptionInfo, NULL, NULL );
		CloseHandle( FileHandle );
	}

	return EXCEPTION_EXECUTE_HANDLER;
}
#endif

//
// Main entry point.
// This is an example of how to initialize and launch the engine.
//
INT WINAPI WinMain( HINSTANCE hInInstance, HINSTANCE hPrevInstance, char*, INT nCmdShow )
{
	// Remember instance.
	INT ErrorLevel = 0;
	GIsStarted     = 1;
	hInstance      = hInInstance;
	CmdLine		   = GetCommandLine();
	appStrcpy( GPackage, appPackage() );

	// Set up minidump filename.
	appStrcpy( MiniDumpFilenameW, TEXT("minidump-v") );
	appStrcat( MiniDumpFilenameW, appItoa( ENGINE_VERSION ) );
	appStrcat( MiniDumpFilenameW, TEXT(".dmp") );
	strcpy( MiniDumpFilenameA, TCHAR_TO_ANSI( MiniDumpFilenameW ) );

	#if 0 //defined(_WIN64) && defined(_DEBUG)
		freopen("stdout.txt", "w", stdout);
		setbuf(stdout, NULL);
		freopen("stderr.txt", "w", stderr);
		setbuf(stderr, NULL);
	#endif

	// See if this should be passed to another instances.
	if
	(	!appStrfind(CmdLine,TEXT("NewWindow"))
	&&	!appStrfind(CmdLine,TEXT("changevideo"))
	&&	!appStrfind(CmdLine,TEXT("TestRenDev"))
	&&	!appStrfind(CmdLine,TEXT("COMMAND=")) )
	{
		TCHAR ClassName[256];
		MakeWindowClassName(ClassName,TEXT("WLog"));
		for( HWND hWnd=NULL; ; )
		{
			hWnd = TCHAR_CALL_OS(FindWindowExW(hWnd,NULL,ClassName,NULL),FindWindowExA(hWnd,NULL,TCHAR_TO_ANSI(ClassName),NULL));
			if( !hWnd )
				break;
			if( GetPropX(hWnd,TEXT("IsBrowser")) )
			{
				while( *CmdLine && *CmdLine!=' ' )
					CmdLine++;
				if( *CmdLine==' ' )
					CmdLine++;
				COPYDATASTRUCT CD;
				DWORD_PTR Result;
				CD.dwData = WindowMessageOpen;
				CD.cbData = (appStrlen(CmdLine)+1)*sizeof(TCHAR*);
				CD.lpData = const_cast<TCHAR*>( CmdLine );
				SendMessageTimeout( hWnd, WM_COPYDATA, (WPARAM)NULL, (LPARAM)&CD, SMTO_ABORTIFHUNG|SMTO_BLOCK, 30000, &Result );
				GIsStarted = 0;
				return 0;
			}
		}
	}

	// Begin guarded code.
	GIsGuarded = 1;
#ifndef _DEBUG
	__try
	{
#endif
		GuardedMain();
#ifndef _DEBUG
	}
	__except( CreateMiniDump( GetExceptionInformation() ) )
	{
		// Crashed.
		ErrorLevel = 1;
		Error.HandleError();
	}
#endif
	GIsGuarded = 0;

	// Final shut down.
	appExit();
	GIsStarted = 0;
	return ErrorLevel;
}

// Relies on COM being initialized.
extern DWORD GetPrimaryAdapterVideoMemory();

// Query the HW for caps.
#include <ddraw.h>
#include <d3d8.h>
static void SetInitialConfiguration()
{
	guard(SetInitialConfiguration);
	//!!vogel: currently only queries primary device though it will be used by default anyways.

#ifndef _WIN64  // !!! FIXME: Need DirectX support on Win64!  --ryan.

	// Variables.
	HRESULT Result;
	INT		OnboardVideoMemory	= MAXINT;
	UBOOL	HardwareTL			= true,
			DeviceCapsValid		= false;
	FLOAT	CPUSpeed			= 0.000001 / GSecondsPerCycle;
	D3DADAPTER_IDENTIFIER8		DeviceIdentifier;
	D3DCAPS8					DeviceCaps8;

	// Get memory status.
	MEMORYSTATUS MemoryStatus; 
	MemoryStatus.dwLength = sizeof(MemoryStatus);
	GlobalMemoryStatus(&MemoryStatus);

	INT TotalMemory = MemoryStatus.dwTotalPhys / 1024 / 1024;
	debugf(NAME_Init, TEXT("Physical Memory: %i MByte"), TotalMemory);

	// Query D3D7 DirectDraw for available local video memory.
	guard(GetAvailableVidMem);
	LPDIRECTDRAW DirectDraw;

	appMemzero( &DeviceCaps8, sizeof(DeviceCaps8) ); // gam

	if( FAILED(Result=DirectDrawCreate( NULL, &DirectDraw, NULL ) ) )
		debugf(NAME_Init, TEXT("D3D Driver: Couldn't query amount of local video memory"));
	else 
	{
		LPDIRECTDRAW7 DirectDraw7;
		if( FAILED(Result=DirectDraw->QueryInterface(IID_IDirectDraw7, (LPVOID*) &DirectDraw7) ) ) 
			debugf(NAME_Init, TEXT("D3D Device: Couldn't query amount of local video memory"));
		else
		{			
			DDSCAPS2	ddsCaps2; 
			DWORD		dwTotal; 
			DWORD		dwFree;

			// See Tom Forsyth's post to DXDev
			// http://discuss.microsoft.com/SCRIPTS/WA-MSD.EXE?A2=ind0203a&L=directxdev&D=1&F=&S=&P=12849

			appMemzero( &ddsCaps2, sizeof(ddsCaps2) );
			ddsCaps2.dwCaps = DDSCAPS_3DDEVICE | DDSCAPS_LOCALVIDMEM | DDSCAPS_TEXTURE | DDSCAPS_VIDEOMEMORY;
			DirectDraw7->GetAvailableVidMem(&ddsCaps2, &dwTotal, &dwFree);

			if( dwTotal )
				OnboardVideoMemory = Min<DWORD>( OnboardVideoMemory, dwTotal );

			appMemzero( &ddsCaps2, sizeof(ddsCaps2) );
			ddsCaps2.dwCaps = DDSCAPS_LOCALVIDMEM | DDSCAPS_TEXTURE | DDSCAPS_VIDEOMEMORY;
			DirectDraw7->GetAvailableVidMem(&ddsCaps2, &dwTotal, &dwFree);

			if( dwTotal )
				OnboardVideoMemory = Min<DWORD>( OnboardVideoMemory, dwTotal );

			appMemzero( &ddsCaps2, sizeof(ddsCaps2) );
			ddsCaps2.dwCaps = DDSCAPS_OFFSCREENPLAIN | DDSCAPS_LOCALVIDMEM | DDSCAPS_VIDEOMEMORY;
			DirectDraw7->GetAvailableVidMem(&ddsCaps2, &dwTotal, &dwFree);

			if( dwTotal )
				OnboardVideoMemory = Min<DWORD>( OnboardVideoMemory, dwTotal );

			DirectDraw7->Release();
		}
		DirectDraw->Release();
	}
	unguard;

	CoInitialize(NULL);
	DWORD WMIVideoMemory = GetPrimaryAdapterVideoMemory();
	OnboardVideoMemory = WMIVideoMemory ? Min<DWORD>( WMIVideoMemory - 4 * 1024 * 1024, OnboardVideoMemory ) : OnboardVideoMemory;
	CoUninitialize();

	// As GetAvailableVidMem is called before entering fullscreen the numbers are slightly less
	// as it reports "local video memory" - "memory used for desktop". Though this shouldn't matter 
	// that much as the user can always override it and as the default values should be conservative.
	if( OnboardVideoMemory != MAXINT )
	{
		OnboardVideoMemory = OnboardVideoMemory / 1024 / 1024;
		debugf( NAME_Init, TEXT("D3D Device: Video memory on board: %i [%i]"), OnboardVideoMemory, WMIVideoMemory / 1024 / 1024 );
	}

	GConfig->SetString( TEXT("Engine.Engine"), TEXT("DetectedVideoMemory"), *FString::Printf(TEXT("%i"),1 << appCeilLogTwo(OnboardVideoMemory)) );

	// Query D3D8 device for SW vs HW T&L and DXT1.
	// Card specific defaults.
	guard(GetD3DDeviceCaps);
	IDirect3D8* Direct3D8 = Direct3DCreate8(D3D_SDK_VERSION);
	if( Direct3D8 )
	{
		if( SUCCEEDED(Result = Direct3D8->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &DeviceCaps8) ) )
		{
			DeviceCapsValid = true;
			if( (DeviceCaps8.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) == 0 )
				HardwareTL = 0;
			// We require at least 8 vertex streams.
			if( DeviceCaps8.MaxStreams < 8 )
				HardwareTL = 0;
		}
		else
			debugf(NAME_Init, TEXT("D3D Device: Couldn't query SW vs HW T&L"));

		// Lower detail settings if DXT3 isn't supported
		if(FAILED(Direct3D8->CheckDeviceFormat(D3DADAPTER_DEFAULT,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,0,D3DRTYPE_TEXTURE,D3DFMT_DXT3)))
			OnboardVideoMemory /= 2;

		if( FAILED( Direct3D8->GetAdapterIdentifier(D3DADAPTER_DEFAULT,D3DENUM_NO_WHQL_LEVEL,&DeviceIdentifier) ) )
			appMemzero( &DeviceIdentifier, sizeof(DeviceIdentifier) );

		Direct3D8->Release();
	}
	else
	{
		appErrorf(NAME_FriendlyError, TEXT("Please install DirectX 8.1b or later (see Release Notes for instructions on how to obtain it)"));
		debugf(NAME_Init, TEXT("D3D Device: Couldn't query SW vs HW T&L"));
		appMemzero( &DeviceIdentifier, sizeof(DeviceIdentifier) );
	}
	unguard;

	// Caps specific settings.
	if( DeviceCapsValid )
	{
		if( DeviceCaps8.MaxSimultaneousTextures < 4 )
		{
			GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("DetailTextures"		),	TEXT("False"		));		
		}
	}

	// Rough generic card defaults.
	if( OnboardVideoMemory < 8 )
	{
		// 8 MByte or less
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullScreenViewportX"		),	TEXT("320"			));

		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullScreenViewportX"		),	TEXT("320"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullScreenViewportY"		),	TEXT("240"			));

		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailInterface"		),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailTerrain"		),	TEXT("VeryLow"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWeaponSkin"	),	TEXT("VeryLow"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailPlayerSkin"	),	TEXT("VeryLow"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWorld"			),	TEXT("Low"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailRenderMap"		),	TEXT("VeryLow"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailLightmap"		),	TEXT("VeryLow"		));

		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Decals"						),	TEXT("False"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("DecoLayers"					),	TEXT("False"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Coronas"					),	TEXT("False"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Projectors"					),	TEXT("False"		));

		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("UseTrilinear"				),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("UseCubemaps"				),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("HighDetailActors"			),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("SuperHighDetailActors"	),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("Use16bit"					),	TEXT("True"			));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("Use16bitTextures"			),	TEXT("True"			));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("DetailTextures"			),	TEXT("False"		));		

		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("NoFractalAnim"				),	TEXT("True"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("NoDynamicLights"			),	TEXT("True"			));
		GConfig->SetString( TEXT("UnrealGame.UnrealPawn"),TEXT("bPlayerShadows"				),	TEXT("False"		), GUserIni );
		GConfig->SetString( TEXT("Engine.Vehicle"		),TEXT("bVehicleShadows"			), TEXT("False"			), GUserIni );
	}
	else if( OnboardVideoMemory <= 16 )
	{
		// 16 MByte
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullScreenViewportX"		),	TEXT("512"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullScreenViewportY"		),	TEXT("384"			));

		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailInterface"		),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailTerrain"		),	TEXT("Low"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWeaponSkin"	),	TEXT("Low"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailPlayerSkin"	),	TEXT("Low"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWorld"			),	TEXT("VeryLow"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailRenderMap"		),	TEXT("Low"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailLightmap"		),	TEXT("Low"			));

		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Decals"						),	TEXT("False"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("DecoLayers"					),	TEXT("False"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Coronas"					),	TEXT("False"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Projectors"					),	TEXT("False"		));

		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("UseTrilinear"				),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("UseCubemaps"				),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("HighDetailActors"			),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("SuperHighDetailActors"	),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("Use16bit"					),	TEXT("True"			));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("Use16bitTextures"			),	TEXT("True"			));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("DetailTextures"			),	TEXT("False"		));		

		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("NoFractalAnim"				),	TEXT("True"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("NoDynamicLights"			),	TEXT("True"			));
		GConfig->SetString( TEXT("UnrealGame.UnrealPawn"),TEXT("bPlayerShadows"				),	TEXT("False"		), GUserIni );
		GConfig->SetString( TEXT("Engine.Vehicle"		),TEXT("bVehicleShadows"			),	TEXT("False"		), GUserIni );
	}
	else if( OnboardVideoMemory <= 32 )
	{
		// 32 MByte
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailInterface"		),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailTerrain"		),	TEXT("Lower"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWeaponSkin"	),	TEXT("Lower"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailPlayerSkin"	),	TEXT("Low"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWorld"			),	TEXT("Low"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailRenderMap"		),	TEXT("Lower"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailLightmap"		),	TEXT("Lower"		));

		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("DecoLayers"					),	TEXT("False"		));

		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("UseTrilinear"				),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("HighDetailActors"			),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("SuperHighDetailActors"	),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("Use16bit"					),	TEXT("True"			));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("DetailTextures"			),	TEXT("False"		));
	}
	else if( OnboardVideoMemory <= 64 )
	{
		// 64 MByte
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailInterface"		),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailTerrain"		),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWeaponSkin"	),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailPlayerSkin"	),	TEXT("Lower"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWorld"			),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailRenderMap"		),	TEXT("Lower"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailLightmap"		),	TEXT("Normal"		));

		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("UseTrilinear"				),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("SuperHighDetailActors"	),	TEXT("False"		));
	}
	else
#if !DEMOVERSION
	if( TotalMemory <= 128 )
#endif
	{
		// 128 MByte of RAM
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailInterface"		),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailTerrain"		),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWeaponSkin"	),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWorld"			),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailRenderMap"		),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailLightmap"		),	TEXT("Normal"		));

		// Sorry - messy due to #if's
		if( TotalMemory <= 128 )
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailPlayerSkin"),	TEXT("Lower"));
		else
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailPlayerSkin"),	TEXT("Normal"));
	}
#if !DEMOVERSION
	else if( OnboardVideoMemory <= 196 )
	{
		// 128 MByte
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailInterface"		),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailTerrain"		),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWeaponSkin"	),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailPlayerSkin"	),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWorld"			),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailRenderMap"		),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailLightmap"		),	TEXT("Normal"		));
	}
	else
	{
		// 256 MByte and more
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailInterface"		),	TEXT("Higher"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailTerrain"		),	TEXT("Higher"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWeaponSkin"	),	TEXT("Higher"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailPlayerSkin"	),	TEXT("Higher"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWorld"			),	TEXT("Higher"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailRenderMap"		),	TEXT("Higher"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailLightmap"		),	TEXT("Higher"		));
	}
#endif

	// Generic CPU defaults.
	if( CPUSpeed < 1200 )
	{
		GConfig->SetString( TEXT("WinDrv.WindowsClient"		), TEXT("Coronas"				),	TEXT("False"	));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"		), TEXT("Decals"				),	TEXT("False"	));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"		), TEXT("DecoLayers"			),	TEXT("False"	));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"		), TEXT("NoFractalAnim"			),	TEXT("True"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"		), TEXT("NoDynamicLights"		),	TEXT("True"		));

		GConfig->SetString( TEXT("UnrealGame.UnrealPawn"	), TEXT("bPlayerShadows"		),	TEXT("False"	), GUserIni );
		GConfig->SetString( TEXT("Engine.Vehicle"			), TEXT("bVehicleShadows"		),	TEXT("False"	), GUserIni );

		GConfig->SetString( TEXT("Engine.LevelInfo"			), TEXT("PhysicsDetailLevel"	),	TEXT("PDL_Low"	));

		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"	), TEXT("HighDetailActors"		),	TEXT("False"	));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"	), TEXT("SuperHighDetailActors"	),	TEXT("False"	));
	}
	else if( (CPUSpeed < 2000) || (OnboardVideoMemory <= 64) )
	{
		if( !HardwareTL )
		{
			GConfig->SetString( TEXT("WinDrv.WindowsClient"	), TEXT("Coronas"				),	TEXT("False"	));
			GConfig->SetString( TEXT("WinDrv.WindowsClient"	), TEXT("NoFractalAnim"			),	TEXT("True"		));

			GConfig->SetString( TEXT("UnrealGame.UnrealPawn"), TEXT("bPlayerShadows"		),	TEXT("False"	), GUserIni );

			GConfig->SetString( TEXT("Engine.LevelInfo"		), TEXT("PhysicsDetailLevel"	),	TEXT("PDL_Low"	));

			GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("HighDetailActors"		),	TEXT("False"	));
		}

		GConfig->SetString( TEXT("Engine.Vehicle"			), TEXT("bVehicleShadows"		),	TEXT("False"	), GUserIni );


		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"	), TEXT("SuperHighDetailActors"	),	TEXT("False"	));
		GConfig->SetString( TEXT("Engine.LevelInfo"			), TEXT("PhysicsDetailLevel"	),	TEXT("PDL_Medium"));
	}
	else if( CPUSpeed < 2800 )
	{
		if( !HardwareTL )
		{
			GConfig->SetString( TEXT("UnrealGame.UnrealPawn"), TEXT("bPlayerShadows"		),	TEXT("False"	), GUserIni );
			GConfig->SetString( TEXT("Engine.Vehicle"		), TEXT("bVehicleShadows"		),	TEXT("False"	), GUserIni );
			GConfig->SetString( TEXT("Engine.LevelInfo"		), TEXT("PhysicsDetailLevel"	),	TEXT("PDL_Medium"));
		}
	}

	// ATI
	if( DeviceIdentifier.VendorId==0x1002 )
	{
	}
	// 3dfx
	else if( DeviceIdentifier.VendorId==0x121A )
	{
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullScreenViewportX"	),	TEXT("640"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullScreenViewportY"	),	TEXT("480"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Projectors"				),	TEXT("False"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("NoDynamicLights"		),	TEXT("True"			));
	}
	// Intel
	else if( DeviceIdentifier.VendorId==0x8086 )
	{
	}
#if 0
	// Kyro
	else if( DeviceIdentifier.DeviceId==0x010 )
	{
	}
#endif
	// NVIDIA
	else if( DeviceIdentifier.VendorId==0x10DE )
	{
		// TNT/ GF2/ GF4MX can't handle required stage setup for most projectors.
		// if( DeviceCaps8.MaxSimultaneousTextures == 2 )
		//	GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Projectors"				),	TEXT("False"		));

		// TNT
		if( DeviceIdentifier.DeviceId == 0x0020 )
		{
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("DecoLayers"				),	TEXT("False"		));
		}
		// TNT 2
		if( DeviceIdentifier.DeviceId >= 0x0028 && DeviceIdentifier.DeviceId <= 0x002F )
		{
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("DecoLayers"				),	TEXT("False"		));
		}
		// GeForce
		if( DeviceIdentifier.DeviceId >= 0x0100 && DeviceIdentifier.DeviceId <= 0x0103 )
		{
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("DecoLayers"				),	TEXT("False"		));
		}
		// GeForce 2 MX
		if( DeviceIdentifier.DeviceId >= 0x0110 && DeviceIdentifier.DeviceId <= 0x0113 )
		{
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("DecoLayers"				),	TEXT("False"		));
		}
		// GeForce 2
		if( DeviceIdentifier.DeviceId >= 0x0150 && DeviceIdentifier.DeviceId <= 0x0153 )
		{
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("DecoLayers"				),	TEXT("False"		));
		}
		// GeForce 4 MX
		if( DeviceIdentifier.DeviceId >= 0x0170 && DeviceIdentifier.DeviceId <= 0x0179 )
		{		
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("DecoLayers"				),	TEXT("False"		));
		}
		// GeForce 3
		if( DeviceIdentifier.DeviceId >= 0x0200 && DeviceIdentifier.DeviceId <= 0x0203 )
		{
		}
		// GeForce 4 Ti
		if( DeviceIdentifier.DeviceId >= 0x0250 && DeviceIdentifier.DeviceId <= 0x0253 )
		{
		}
	}
	// Matrox
	else if( DeviceIdentifier.VendorId==0x102B )
	{
	}
	// Trident
	else if( DeviceIdentifier.VendorId==0x1023 )
	{
	}
	// SiS
	else if( DeviceIdentifier.VendorId==0x1039 )
	{
	}
	// Generic
	else
	{
	}

	if( TotalMemory < 64 )
	{
		// 64 MByte
		GConfig->SetString( TEXT("Engine.GameEngine"		), TEXT("CacheSizeMegs"		),	TEXT("16"		));
		GConfig->SetString( TEXT("ALAudio.ALAudioSubsystem"	), TEXT("LowQualitySound"	),	TEXT("True"		));
		GConfig->SetString( TEXT("Engine.LevelInfo"			), TEXT("bLowSoundDetail"	),	TEXT("True"		));
	}
	else if( TotalMemory < 128 )
	{
		// 128 MByte
		GConfig->SetString( TEXT("Engine.GameEngine"		), TEXT("CacheSizeMegs"		),	TEXT("24"		));
		GConfig->SetString( TEXT("ALAudio.ALAudioSubsystem"	), TEXT("LowQualitySound"	),	TEXT("True"		));
		GConfig->SetString( TEXT("Engine.LevelInfo"			), TEXT("bLowSoundDetail"	),	TEXT("True"		));
	}
	else if( TotalMemory < 192 )
	{
		// 192 MByte
	}
	else if( TotalMemory < 256 )
	{
		// 256 MByte
	}
	else if ( TotalMemory > 600 )
	{
		GConfig->SetString( TEXT("Engine.LevelInfo"			), TEXT("bShouldPreload"	),	TEXT("True"		));
		GConfig->SetString( TEXT("Engine.LevelInfo"			), TEXT("bDesireSkinPreload"	),	TEXT("True"		));
	}

#endif

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

