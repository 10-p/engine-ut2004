/*=============================================================================
	DebuggerLaunch.cpp: Debugger/Game launcher 
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
	* Hijacked for UDebugger purposes by Lucas Alonso, Demiurge Studios.
=============================================================================*/

#include "DebuggerLaunchPrivate.h"
#include "UnEngineWin.h"

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

// General.
extern "C" {HINSTANCE hInstance;}
extern "C" {TCHAR GPackage[64]=TEXT("Launch");}

// Memory allocator.
#if 0
	#include "FMallocDebug.h"
	FMallocDebug Malloc;
#else
	#include "FMallocWindows.h"
	FMallocWindows Malloc;
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
// Main entry point.
// This is an example of how to initialize and launch the engine.
//
INT WINAPI WinMain( HINSTANCE hInInstance, HINSTANCE hPrevInstance, char*, INT nCmdShow )
{
	// Remember instance.
	INT ErrorLevel = 0;
	GIsStarted     = 1;
	hInstance      = hInInstance;
	const TCHAR* CmdLine = GetCommandLine();
	if( !Parse( CmdLine, TEXT("INI="), GPackage, ARRAY_COUNT(GPackage) ) )
		appSprintf( GPackage, TEXT("UT2004") );

	// Begin guarded code.
#ifndef _DEBUG
	try
	{
#endif
		// Init core.
		GIsClient = GIsGuarded = 1;
		appInit( GPackage, CmdLine, &Malloc, &Log, &Error, &Warn, &FileManager, FConfigCacheIni::Factory, 1 );

		// Init mode.
		GIsServer     = 1;
		GIsClient     = !ParseParam(appCmdLine(),TEXT("SERVER"));
		GIsEditor     = 0;
		GIsScriptable = 1;
		GLazyLoad     = !GIsClient || ParseParam(appCmdLine(),TEXT("LAZY"));
		GTransientNaming = 1;

		// Splash screen is no more...

		// Figure out whether to show log or splash screen.
/*		UBOOL ShowLog = ParseParam(CmdLine,TEXT("LOG"));
		FString Filename = FString(TEXT("..\\Help\\DebuggerLogo.bmp"));
		if( GFileManager->FileSize(*Filename)<0 )
			Filename = TEXT("..\\Help\\DebuggerLogo.bmp");
		appStrcpy( GPackage, appPackage() );
		if( !ShowLog && !ParseParam(CmdLine,TEXT("server")) && !appStrfind(CmdLine,TEXT("TestRenDev")) )
		{
			// hack to get this function to work
			GIsEditor = TRUE;
			InitSplash( *Filename );
			GIsEditor = FALSE;
		}*/

		// Init windowing.
		InitWindowing();

		WLog* LogWindow = NULL;
		UBOOL bLogWindow = ParseParam(CmdLine,TEXT("logwindow"));
		if ( bLogWindow )
		{
			// Create log window, but only show it if ShowLog.
			LogWindow = new WLog( Log.Filename, Log.LogAr, TEXT("GameLog") );
			LogWindow->OpenWindow( 1, 0 );
			LogWindow->Log( NAME_Title, LocalizeGeneral(TEXT("Start"),GPackage) );
			if( GIsClient )
				SetPropX( *LogWindow, TEXT("IsBrowser"), (HANDLE)1 );
		}

		UDebuggerCore* DebuggerCore = new UDebuggerCore(LogWindow);
		GDebugger = DebuggerCore;
		DebuggerCore->Initialize();

		UEngine* Engine = InitEngine();

		if ( !ParseParam(CmdLine, TEXT("EXTERNAL")) )
            DebuggerCore->LoadEditPackages();

		if( Engine )
		{
			// Optionally Exec an exec file
			FString Temp;
			if( Parse(CmdLine, TEXT("EXEC="), Temp) )
			{
				Temp = FString(TEXT("exec ")) + Temp;
				if( Engine->Client && Engine->Client->Viewports.Num() && Engine->Client->Viewports(0) )
					Engine->Client->Viewports(0)->Exec( *Temp, *GLogWindow );
			}

			// Hide splash screen.
			// hack to get this to work
			GIsEditor = TRUE;
			ExitSplash();
			GIsEditor = FALSE;

			// Start main engine loop, including the Windows message pump.
			if( !GIsRequestingExit )
				MainLoop( Engine );

		}

		// Clean shutdown.
		if ( LogWindow )
		{
			RemovePropX( *LogWindow, TEXT("IsBrowser") );
			delete LogWindow;
		}

		LogWindow = NULL;

		if ( GDebugger )
		{
			DebuggerCore->Close();
			delete DebuggerCore;
		}

		DebuggerCore = NULL;
		GDebugger = NULL;

		GFileManager->Delete(TEXT("Running.ini"),0,0);

		GIsGuarded = 0;
		
		appPreExit();
#ifndef _DEBUG
	}
	catch( ... )
	{
		// Crashed.
		ErrorLevel = 1;
		Error.HandleError();
	}
#endif

	// Final shut down.
	appExit();
	GIsStarted = 0;
	return ErrorLevel;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

