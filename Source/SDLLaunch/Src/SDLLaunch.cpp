/*=============================================================================
	SDLLaunch.cpp: Game launcher.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Ryan C. Gordon, based on ut2003's win32 Launch.cpp.
=============================================================================*/

#include <time.h>  // dodging a macro clash with Unix C runtime headers...

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#include <emscripten/html5.h>
#include <AL/al.h>

// Mutes the listener while the tab is hidden; the engine keeps ticking.
static EM_BOOL on_visibility_change(int eventType,
    const EmscriptenVisibilityChangeEvent* e, void* userData)
{
    if (e->hidden) {
        alListenerf(AL_GAIN, 0.0f);
    } else {
        alListenerf(AL_GAIN, 1.0f);
    }
    return EM_TRUE;
}

#ifdef __EMSCRIPTEN_PTHREADS__
// Yield to the worker's event loop once per frame: the worker-owned OffscreenCanvas is composited when the
// worker's task ends, and browser events and console commands are dispatched between tasks. It resumes from a
// 0 ms timer because a timer task interleaves with rendering (a MessagePort task does not). EM_ASYNC_JS names
// the import __asyncjs__ufront_yield_frame, which Asyncify's default import list (__asyncjs__*) covers.
EM_ASYNC_JS(void, ufront_yield_frame, (void), {
    await new Promise((resolve) => { setTimeout(resolve, 0); });
});
#endif
#endif

#if defined(__ANDROID__)
// Route the engine's stdout/stderr (printf + FOutputDeviceAnsiError) to logcat. Android blocks
// `setprop log.redirect-stdio` under SELinux, so a pipe + reader thread tags each line "UT2004".
#include <android/log.h>
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
static int GAndroidStdioPipe[2] = { -1, -1 };
static void* AndroidStdioLogThread(void*)
{
    char buf[1024];
    ssize_t n;
    while ((n = read(GAndroidStdioPipe[0], buf, sizeof(buf) - 1)) > 0)
    {
        while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) n--;
        buf[n] = '\0';
        if (n > 0)
            __android_log_write(ANDROID_LOG_INFO, "UT2004", buf);
    }
    return NULL;
}
static void AndroidRedirectStdioToLogcat()
{
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
    if (pipe(GAndroidStdioPipe) != 0) return;
    dup2(GAndroidStdioPipe[1], STDOUT_FILENO);
    dup2(GAndroidStdioPipe[1], STDERR_FILENO);
    pthread_t t;
    if (pthread_create(&t, NULL, AndroidStdioLogThread, NULL) == 0)
        pthread_detach(t);
}
#endif

#include "SDLLaunchPrivate.h"


#if __STATIC_LINK
#include "Engine.h"

#include "UnRender.h"
#include "UnNullRenderDevice.h"
#define SDL2DRV_REGISTRATION_ONLY
#include "SDL2Drv.h"	// the only viewport driver
#if UE_RENDER_OPENGLDRV
#include "OpenGLDrv.h"
#endif
#if UE_RENDER_GL4ESDRV
#include "GL4ESDrv.h"
#endif
#include "UnNet.h"
#include "UnFractal.h"
#include "ALAudio.h"
#include "UnIpDrv.h"
#include "UnTcpNetDriver.h"
#ifdef __EMSCRIPTEN__
#include "WebRTCDrv.h"
#include "WebRTCDrvClasses.h"
#ifdef UE_WITH_PROXYNET
#include "ProxyNetDrv.h"
#include "ProxyNetDrvClasses.h"
#endif
#endif
#include "UnIpDrvCommandlets.h"
#include "UnIpDrvNative.h"
#include "XInterface.h"
#include "UWeb.h"
#include "UWebNative.h"
#include "OnslaughtPrivate.h"
// void autoInitializeRegistrantsEditor(INT &Lookup); // the Editor is not built

#if USE_PIXOMATIC
extern "C" { void autoInitializeRegistrantsPixoDrv(void); }
#define AUTO_INITIALIZE_REGISTRANTS_PIXODRV autoInitializeRegistrantsPixoDrv();
#endif

#endif


#if MACOSX  // GameRanger stuff...didn't want to include the header... --ryan.
extern "C"
{
    unsigned char GRCheckFileForCmd(void);  // uchar is "Boolean".
    void GRGetWaitingCmd(void);
    unsigned char GRIsHostCmd(void);
    unsigned char GRIsJoinCmd(void);
    void GRHostReady(void);
    void GRHostClosed(void);
    unsigned short GRGetPortNumber(void);
    char *GRGetJoinAddressStr(void);

    // internal to Unreal:
    int GameRangerClientsWaiting = 0;
    void AlertGameRangerHostReady(void);
}


void AlertGameRangerHostReady(void)
{
    guard(AlertGameRangerHostReady);

    if (!GameRangerClientsWaiting)
        return;

    debugf( TEXT("Alerting GameRanger clients that host is ready.") );

	guard(GRHostReady);
	GRHostReady();
	unguard;

    GameRangerClientsWaiting = 0;
    unguard;
}
#endif



// True iff the render-device class is compiled into this build (its UClass registered at startup).
static UBOOL IsRenderDeviceAvailable( const TCHAR* Device )
{
	FString Path = Device;
	INT Dot = Path.InStr( TEXT("."), 1 );
	const TCHAR* ClassName = ( Dot != INDEX_NONE ) ? ((*Path) + Dot + 1) : (*Path);
	return UObject::StaticFindObject( UClass::StaticClass(), ANY_PACKAGE, ClassName ) != NULL;
}

/*-----------------------------------------------------------------------------
	Global variables.
-----------------------------------------------------------------------------*/

// General.
extern "C" {TCHAR GPackage[64]=TEXT("SDLLaunch");}
static void SetInitialConfiguration();

// Memory allocator.
#ifdef _DEBUG
	#include "FMallocDebug.h"
	FMallocDebug Malloc;
#else
	#include "FMallocAnsi.h"
	FMallocAnsi Malloc;
#endif

// Log file: FOutputDeviceFile writes the log file and echoes each line to stdout (the browser console on the web).
#include "FOutputDeviceFile.h"
FOutputDeviceFile Log;

// Error handler.
#include "FOutputDeviceAnsiError.h"
FOutputDeviceAnsiError Error;

// Feedback.
#include "FFeedbackContextAnsi.h"
FFeedbackContextAnsi Warn;

// File manager.
#include "FFileManagerUnix.h"
FFileManagerUnix FileManager;

// Config.
#include "FConfigCacheIni.h"

/*-----------------------------------------------------------------------------
	mainline
-----------------------------------------------------------------------------*/

// note that this is really Unix specific, and not really SDL specific,
//  except the splash screen.  --ryan.
// (Actually, most of this code isn't Unix specific, either.)
#define USE_SDL_SPLASH 0

#if (!USE_SDL_SPLASH)

static void InitSplash(const TCHAR *filename) {}
static void ExitSplash(void) {}

#else

#include "SDL.h"

static void InitSplash(const TCHAR *filename)
{
    if (filename == NULL)
        return;

    if (SDL_Init(SDL_INIT_VIDEO) == -1)
        return;

    SDL_Surface *bmp = SDL_LoadBMP(appToAnsi(filename));
    if (bmp == NULL)
        return;

    putenv("SDL_WINDOW_POS=center");        // legacy.
    putenv("SDL_VIDEO_WINDOW_POS=center");  // new in SDL CVS.
    SDL_Surface *screen = SDL_SetVideoMode(bmp->w, bmp->h, 0, SDL_NOFRAME);
    putenv("SDL_WINDOW_POS=nopref");  // !!! FIXME: is there a portable way to delete values?
    putenv("SDL_VIDEO_WINDOW_POS=nopref");  // !!! FIXME: is there a portable way to delete values?

    if (screen != NULL)
    {
        SDL_BlitSurface(bmp, NULL, screen, NULL);
        SDL_Flip(screen);
    }

    SDL_FreeSurface(bmp);
}


static void ExitSplash(void)
{
    // Don't quit: SDLDrv will have created an OpenGL window at this point.
    //SDL_QuitSubSystem(SDL_INIT_VIDEO);
}
#endif


// !!! FIXME: This is largely cut-and-paste from UnEngineWin.h. Unify! --ryan.
//
// Unreal's main message loop.  All windows in Unreal receive messages
// somewhere below this function on the stack.
//

class CMainLoop
{
public:
	inline CMainLoop( UEngine* InEngine, UBOOL InIncludeMessaging = true );
	inline ~CMainLoop(void);

	inline void	RunLoop(void);
	inline bool	Finished(void) const	{return !GIsRunning || GIsRequestingExit;}

private:
	UEngine *		Engine;
	UBOOL			IncludeMessaging;

	DWORD			ThreadId;
	HANDLE			hThread;
	DOUBLE			OldTime;
	DOUBLE			SecondStartTime;
	INT				TickCount;
	DWORD			LastFrameCycles;

	// Benchmarking.
	INT				BMFrames;
	INT				BMDiscardedFrames;
	INT				BMMaxFrames;
	FLOAT			BMSeconds;
	TArray<FString> BMStrings;
	TArray<FLOAT>	BMFrameTimes;

	// Movie recording.
	UBOOL			RecordingMovie;

};

inline CMainLoop::CMainLoop( UEngine* InEngine, UBOOL InIncludeMessaging )
:	Engine(InEngine), IncludeMessaging(InIncludeMessaging)
{
	guard(MainLoopCtor);
	check(Engine);

	// Enter main loop.
    #if 0
	guard(EnterMainLoop);
	if( GLogWindow )
		GLogWindow->SetExec( Engine );
	unguard;
    #endif

	// Loop while running.
	GIsRunning			= 1;
	//ThreadId			= GetCurrentThreadId();
	//hThread				= GetCurrentThread();
	OldTime				= appSeconds();
	SecondStartTime		= OldTime;
	TickCount			= 0;

	// Benchmarking.
	BMFrames			= 0;
	BMDiscardedFrames	= 10;
	BMSeconds			= 0.0f;
	
	BMMaxFrames			= 0; // gam
	Parse(appCmdLine(),TEXT("SECONDS="),BMMaxFrames);
	BMMaxFrames			*= 30;
	
	for( INT i=0; i<(BMMaxFrames-BMDiscardedFrames+1); i++ )
		new(BMStrings) FString(TEXT(""));
	if( BMMaxFrames )
		BMFrameTimes.Add(BMMaxFrames-BMDiscardedFrames+1);

	LastFrameCycles		= appCycles();

	// Seed random number generator with constant seed for benchmarking.
	if( GIsBenchmarking )
		appRandInit( 0 );

	// Movie recording.
	RecordingMovie	= ParseParam(appCmdLine(),TEXT("RECORDMOVIE"));
		
	unguard;
}

inline void	CMainLoop::RunLoop(void)
{
	// Ever really needed ? - Erik
	if( Finished() ) 
		return;

	// Clear stats (will also update old stats).
	GStats.Clear();

    if( GIsBenchmarking )
	{
		if( BMFrames == BMDiscardedFrames )
			BMSeconds = 0.0f;
		else if( BMFrames > BMMaxFrames )
			appRequestExit(0);
	}

	// Update the world.
	DOUBLE NewTime   = appSeconds();
	FLOAT  DeltaTime = NewTime - OldTime;

	guard(UpdateWorld);
	if( GUseFixedTimeStep )
	{
		GDeltaTime = GFixedTimeStep;
		GCurrentTime += GFixedTimeStep;
		if( BMFrames < BMMaxFrames )
			BMSeconds += DeltaTime;
		BMFrames++;
	}
	else
	{
		GCurrentTime = NewTime;
		GDeltaTime = DeltaTime;
	}

	//Update.
	if( GDeltaTime < 0.f )
		GDeltaTime = 1 / 20.f;
	Engine->Tick( GDeltaTime );
	// sjs --- engine::tick may load a new map and cause the timing to be reset (this is a good thing)
	if( appSeconds() < NewTime )
           SecondStartTime = NewTime = GCurrentTime = appSeconds();
	// --- sjs
	if( GWindowManager )
		GWindowManager->Tick( GDeltaTime );

	TickCount++;
	OldTime = NewTime;

	if( OldTime > SecondStartTime + 1 )
	{
		if( GUseFixedTimeStep )
			Engine->CurrentTickRate = 1 / GFixedTimeStep;
		else
			Engine->CurrentTickRate = (FLOAT)TickCount / (OldTime - SecondStartTime);
		SecondStartTime = OldTime;
		TickCount = 0;
	}
	unguard;

	// Enforce optional maximum tick rate.
	UBOOL LimiterSlept = 0;	// the web yields below only when this limiter did not sleep
	guard(EnforceTickRate);
	if( !GUseFixedTimeStep && IncludeMessaging )
	{
		FLOAT MaxTickRate = Engine->GetMaxTickRate();
		// [Engine.Engine] FrameRateCap=<fps> caps a game with no tick rate of its own (a standalone game or a listen
		// server, where GetMaxTickRate returns 0). Absent or <= 0: uncapped.
		static FLOAT MaxFps = -1.f;
		if( MaxFps < 0.f )
		{
			MaxFps = 0.f;
			GConfig->GetFloat( TEXT("Engine.Engine"), TEXT("FrameRateCap"), MaxFps );
			MaxFps = Max( 0.f, MaxFps );
			debugf( NAME_Log, TEXT("ufront: client frame rate cap %.0f fps%s"), MaxFps, MaxFps > 0.f ? TEXT("") : TEXT(" (uncapped)") );
		}
		if( MaxFps > 0.f && (MaxTickRate <= 0.f || MaxFps < MaxTickRate) )
			MaxTickRate = MaxFps;
		if( MaxTickRate>0.0 )
		{
			FLOAT Delta = (1.0/MaxTickRate) - (appSeconds()-OldTime);
			appSleep( Max(0.f,Delta) );
			LimiterSlept = 1;
		}
	}
	unguard;

#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
	// Return to this worker's event loop once per frame. GetMaxTickRate() is 0 for a standalone game, so without
	// this yield the loop never returns to JS and postMessage (console commands) and the browser events SDL
	// forwards are never dispatched. Skipped when the limiter above already slept.
	guard(WorkerYield);
	if( GUseFixedTimeStep || !IncludeMessaging || !LimiterSlept )
		// The yield is also the point where the frame is presented.
		ufront_yield_frame();
	unguard;
#endif

	if( IncludeMessaging ) 
	{
		// Handle all incoming messages.
		guard(MessagePump);
		Engine->Client->Viewports(0)->UpdateInput(0, 0);
		unguard;
	}

    #if 0
	// If editor thread doesn't have the focus, don't suck up too much CPU time.
	if( GIsEditor && IncludeMessaging )
	{
		guard(ThrottleEditor);
		static UBOOL HadFocus=1;
		UBOOL HasFocus = (GetWindowThreadProcessId(GetForegroundWindow(),NULL) == ThreadId );
		if( HadFocus && !HasFocus )
		{
			// Drop our priority to speed up whatever is in the foreground.
			SetThreadPriority( hThread, THREAD_PRIORITY_BELOW_NORMAL );
		}
		else if( HasFocus && !HadFocus )
		{
			// Boost our priority back to normal.
			SetThreadPriority( hThread, THREAD_PRIORITY_NORMAL );
		}
		if( !HasFocus )
		{
			// Surrender the rest of this timeslice.
			Sleep(0);
		}
		HadFocus = HasFocus;
		unguard;
	}
    #endif

	if( RecordingMovie ) //&& BMFrames > 120)
		Engine->Client->Viewports(0)->Exec(TEXT("shot"));

	GStats.DWORDStats( GEngineStats.STATS_Frame_TotalCycles ) = appCycles() - LastFrameCycles;
	GStats.DWORDStats( GEngineStats.STATS_Game_ScriptCycles ) = GScriptCycles;
	GScriptCycles	= 0;
	LastFrameCycles = appCycles();

	if( GIsBenchmarking )
	{
		if( BMFrames == 1 )
		{
			// Get descriptions.
			GStats.UpdateString( BMStrings(0), 1 );
			BMFrameTimes(0) = 0.f;
		}
		else
		{
			// Get frame stats. (Index already implies the +1 for descriptions)
			INT BMStringsIndex = Clamp( BMFrames - BMDiscardedFrames, 2, BMMaxFrames - BMDiscardedFrames );
			GStats.UpdateString( BMStrings(BMStringsIndex - 1), 0 );
			BMFrameTimes( BMStringsIndex - 1 ) = GStats.DWORDStats( GEngineStats.STATS_Frame_TotalCycles ) * GSecondsPerCycle * 1000.f;
		}
	}
}

inline CMainLoop::~CMainLoop(void)
{
	guard(CMainLoopDtor);

	GIsRunning = 0;
	if( GIsBenchmarking )
	{
		INT Frames		= BMMaxFrames - BMDiscardedFrames; 
		UBOOL LogLowFPS = ParseParam(appCmdLine(),TEXT("ONLYLOGLOWFPS"));
		FLOAT LowFPS	= 100.f; // in ms

		// Level.
		FString LevelName = Engine->Client->Viewports(0)->Actor->GetViewTarget()->XLevel->GetPathName();
		LevelName = LevelName.LeftChop( 8 );

		// Get time & date.
		INT Year, Month, DayOfWeek, Day, Hour, Minutes, Sec, MSec;
		appSystemTime( Year, Month, DayOfWeek, Day, Hour, Minutes, Sec, MSec );
		FString DateTime = FString::Printf(TEXT("%i-%02i-%02i-%02i-%02i-%02i"),Year,Month,Day,Hour,Minutes,Sec);

		// Machine Details.
		FString MachineString = FString::Printf(TEXT("%s\r\n%s\r\n%s\r\n%s\r\n\r\n%s\r\n\r\n"),GBuildLabel,GMachineOS,GMachineCPU,GMachineVideo,appCmdLine());
		FString OutputString = MachineString;
		FString LowFPSString = TEXT("");

		// Count how many frames take more than LowFPS ms.
		if( LogLowFPS )
		{
			INT Count = 0;
			for( INT i=1; i<Frames; i++ )	
				if( BMFrameTimes(i) > LowFPS )
					Count++;
			
			LowFPSString = FString::Printf(TEXT("High frametimes: %i / %i == %f percent \r\n\r\n"), Count, Frames, 100.f * FLOAT(Count) / Frames ); 
			OutputString += LowFPSString;
		}

		// Determine min/ max framerate and score.
		FLOAT	MinFPS = 1000.f,
				MaxFPS = 0.f,
				AvgFPS = Frames / BMSeconds,
				Score  = 0.f;

		// Check for FRAMETIMECAP command line option.
		FLOAT FPSCap = 0;
		Parse(appCmdLine(),TEXT("MAXFPS="),FPSCap);
		if( FPSCap <= 0 )
			FPSCap = 80;
		FLOAT FrameTimeCap = 1000.f / FPSCap;

		for( INT i=1; i<Frames; i++ )
		{
			// Calculate min/max.
			FLOAT FPS = 1000.f / BMFrameTimes(i);
			MinFPS = Min( MinFPS, FPS );
			MaxFPS = Max( MaxFPS, FPS );
			// Calculate score.
			Score += Max( BMFrameTimes(i), FrameTimeCap ) ;
		}

		Score = Frames / Score * 1000.f;

		// Output detailed results to a file.
		INT LastRand = appRand();
		for( INT i=0; i<Frames; i++ )
		{
			if( LogLowFPS && i!=0 )
			{
				if( BMFrameTimes(i) > LowFPS )
					OutputString += BMStrings(i);
			}
			else
				OutputString += BMStrings(i);
		}
		TCHAR File[1024];
		appSprintf( File, TEXT("..\\Benchmark\\CSVs\\stats-%s.csv"), *DateTime );
		appSaveStringToFile( OutputString, File );

		// Output average framerate.
		OutputString = TEXT("");
		appLoadFileToString( OutputString, TEXT("..\\Benchmark\\benchmark.log") );
		OutputString += FString::Printf(TEXT("%f / %f / %f fps -- Score = %f        rand[%i]\r\n"), MinFPS, AvgFPS, MaxFPS, Score, LastRand );
		appSaveStringToFile( OutputString, TEXT("..\\Benchmark\\benchmark.log") );
		OutputString = MachineString;
		if( LogLowFPS )
			OutputString += LowFPSString;
		OutputString += FString::Printf(TEXT("%f / %f / %f fps         rand[%i]\r\nScore = %f\r\n"), MinFPS, AvgFPS, MaxFPS, LastRand, Score );
		appSaveStringToFile( OutputString, *FString::Printf(TEXT("..\\Benchmark\\Results\\avgfps-%s.log"), *DateTime ) );
	
		// Output low framerate stats.
		OutputString = TEXT("");
		appLoadFileToString( OutputString, TEXT("..\\Benchmark\\lowframerate.log") );
		OutputString += FString::Printf(TEXT("%s\r\n%f / %f / %f fps\r\nScore = %f\r\n%s\r\n"), *LevelName, MinFPS, AvgFPS, MaxFPS, Score, *LowFPSString );
		appSaveStringToFile( OutputString, TEXT("..\\Benchmark\\lowframerate.log") );

		// Output average for benchmark launcher.
		if( ParseParam(appCmdLine(),TEXT("UPT") ) )
		{
			OutputString = FString::Printf(TEXT("%f"), AvgFPS );
			appSaveStringToFile( OutputString, TEXT("dummy.ben") );
		}

		GLog->Flush();
		GFileManager->Copy( *FString::Printf(TEXT("..\\Benchmark\\Logs\\ut2004-%s.log"),*DateTime), TEXT("ut2004.log") );
	}

	// Exit main loop.
    #if 0
	guard(ExitMainLoop);
	if( GLogWindow )
		GLogWindow->SetExec( NULL );
	GExec = NULL;
	unguard;
    #endif

	unguard;
}

#ifdef __EMSCRIPTEN__
// The engine behind ue_exec_command and the WebRTC listen-server status push (WebRTCNetDriver.cpp).
UEngine* GEngineForExec = NULL;

// ue_exec_command (vendor/glue/ue_abi.h): runs a console command and returns its captured output. TCHAR is
// ANSICHAR on the web (NO_UNICODE_OS_SUPPORT), so no encoding adapter is needed. The text is held in a static
// FString, valid until the next call (the JS side reads it during the ccall); before the engine exists the
// result is an empty string, never NULL.
extern "C" char* EMSCRIPTEN_KEEPALIVE ue_exec_command( const char* Cmd )
{
	static FString Captured;
	Captured = TEXT("");
	if( GEngineForExec && Cmd )
	{
		FStringOutputDevice Out;
		GEngineForExec->Exec( Cmd, Out );
		Captured = Out;
	}
	return (char*)(*Captured);
}
#endif

static void MainLoop( UEngine* Engine )
{
	guard(MainLoop);
	check(Engine);

	CMainLoop* theLoop = new CMainLoop(Engine);
	while (!theLoop->Finished()) 
	{
		theLoop->RunLoop();
	}
	
	delete theLoop;

	unguard;
}


// !!! FIXME: This is largely cut-and-paste from UnEngineWin.h. Unify! --ryan.
static UEngine* InitEngine()
{
	guard(InitEngine);
    appResetTimer(); // sjs
	DOUBLE LoadTime = appSeconds();

	// Set exec hook.
	GExec = NULL;

	// First-run menu.
	INT FirstRun=0;
	GConfig->GetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );

    // Replace any driver the ini names but this build lacks (IsRenderDeviceAvailable: the class is registered),
    // so one binary boots from a retail Windows ini (WinDrv + D3D) or any earlier one. A device the build has is
    // left alone. -NoReplaceViewport / -NoReplaceRenderDevice keep the ini's choice, missing or not.
    if( !ParseParam(appCmdLine(),TEXT("NoReplaceViewport")) )
    {
        // Viewport client: SDL2Drv is the one every build has.
        const TCHAR* Viewport = GConfig->GetStr(TEXT("Engine.Engine"),TEXT("ViewportManager"),TEXT("System"));
        if( !IsRenderDeviceAvailable(Viewport) && IsRenderDeviceAvailable(TEXT("SDL2Drv.SDL2Client")) )
        {
            debugf(TEXT("ViewportManager=%s is not in this build -> %s"), Viewport, TEXT("SDL2Drv.SDL2Client"));
            GConfig->SetString(TEXT("Engine.Engine"), TEXT("ViewportManager"), TEXT("SDL2Drv.SDL2Client"));
        }
    }

    if( !ParseParam(appCmdLine(),TEXT("NoReplaceRenderDevice")) )
    {
        // Render device: desktop GL leads where it is compiled in (native); GL4ES is the only one on the web and
        // Android.
        const TCHAR* Device = GConfig->GetStr(TEXT("Engine.Engine"),TEXT("RenderDevice"),TEXT("System"));
        if( !IsRenderDeviceAvailable(Device) )
        {
            static const TCHAR* const DevicePreference[] = { TEXT("OpenGLDrv.OpenGLRenderDevice"),
                                                             TEXT("GL4ESDrv.GL4ESRenderDevice") };
            for( INT i=0; i<ARRAY_COUNT(DevicePreference); i++ )
            {
                if( IsRenderDeviceAvailable(DevicePreference[i]) )
                {
                    debugf(TEXT("RenderDevice=%s is not in this build -> %s"), Device, DevicePreference[i]);
                    GConfig->SetString(TEXT("Engine.Engine"), TEXT("RenderDevice"), DevicePreference[i]);
                    break;
                }
            }
        }
    }

    // Audio device: the build's ALAudio, or no sound (UseSound=False) when there is none, instead of the fatal
    // LOAD_NoFail in UEngine::InitAudio.
    {
        const TCHAR* Audio = GConfig->GetStr(TEXT("Engine.Engine"),TEXT("AudioDevice"),TEXT("System"));
        if( *Audio && !IsRenderDeviceAvailable(Audio) )
        {
            if( IsRenderDeviceAvailable(TEXT("ALAudio.ALAudioSubsystem")) )
            {
                debugf(TEXT("AudioDevice=%s is not in this build -> ALAudio.ALAudioSubsystem"), Audio);
                GConfig->SetString(TEXT("Engine.Engine"), TEXT("AudioDevice"), TEXT("ALAudio.ALAudioSubsystem"));
            }
            else
            {
                debugf(NAME_Warning, TEXT("AudioDevice=%s is not in this build and no audio device is -> no sound"), Audio);
                GConfig->SetBool(TEXT("Engine.Engine"), TEXT("UseSound"), 0);
            }
        }
    }

#ifdef __EMSCRIPTEN__
    // A stock IpDrv.TcpNetDriver becomes the WebRTC driver: browsers have no BSD sockets.
    {
        const TCHAR* CurrentNetDrv = GConfig->GetStr(TEXT("Engine.Engine"),TEXT("NetworkDevice"),TEXT("System"));
        if( appStricmp(CurrentNetDrv,TEXT("IpDrv.TcpNetDriver"))==0 )
        {
            debugf(TEXT("[WebRTC] Auto-switching NetworkDevice from TcpNetDriver to WebRTCNetDriver (Emscripten build)."));
            GConfig->SetString(TEXT("Engine.Engine"), TEXT("NetworkDevice"), TEXT("WebRTCDrv.WebRTCNetDriver"));
        }
        else
        {
            debugf(TEXT("[WebRTC] NetworkDevice='%s' (no auto-switch needed)"), CurrentNetDrv);
        }
    }
#endif

	if( ParseParam(appCmdLine(),TEXT("FirstRun")) )
		FirstRun=0;
	if( FirstRun<220 )
	{
		// Migrate savegames.
		TArray<FString> Saves = GFileManager->FindFiles( TEXT("../Save/*.usa"), 1, 0 );
		for( TArray<FString>::TIterator It(Saves); It; ++It )
		{
			INT Pos = appAtoi(**It+4);
			FString Section = TEXT("UnrealShare.UnrealSlotMenu");
			FString Key     = FString::Printf(TEXT("SlotNames[%i]"),Pos);
			if( appStricmp(GConfig->GetStr(*Section,*Key,TEXT("user")),TEXT(""))==0 )
				GConfig->SetString(*Section,*Key,TEXT("Saved game"),TEXT("user"));
		}
	}

	// EXEC from command-line
	FString Command;
	if( Parse(appCmdLine(),TEXT("consolecommand="), Command) )
	{
        if (GExec == NULL)
        {
            debugf(TEXT("Can't use consolecommand"));
        }
        else
        {
		    debugf(TEXT("Executing console command %s"),*Command);
    		GExec->Exec( *Command, *GLog );
        }
		return NULL;
	}

	// Test render device.
	FString Device;
	if( Parse(appCmdLine(),TEXT("testrendev="),Device) )
	{
		debugf(TEXT("Detecting %s"),*Device);
		try
		{
			UClass* Cls = LoadClass<URenderDevice>( NULL, *Device, NULL, 0, NULL );
			GConfig->SetInt(*Device,TEXT("DescFlags"),RDDESCF_Incompatible);
			GConfig->Flush(0);
			if( Cls )
			{
				URenderDevice* RenDev = ConstructObject<URenderDevice>(Cls);
				if( RenDev )
				{
					if( RenDev->Init() )
					{
						debugf(TEXT("Successfully detected %s"),*Device);
					}
					else
					{
						delete RenDev;
						RenDev = NULL;
					}
				}
			}
		} catch( ... ) {}
		FArchive* Ar = GFileManager->CreateFileWriter(TEXT("Detected.ini"),0);
		if( Ar )
			delete Ar;
		return NULL;
	}

    // gam ---
    if( ParseParam(appCmdLine(),TEXT("debugging")) && (GFileManager->FileSize(TEXT("Running.ini")) >= 0) )
        GFileManager->Delete(TEXT("Running.ini"),0,0);
    // --- gam

    #if 0  // nope. --ryan.
	// Config UI.
	guard(ConfigUI);
	if( !GIsEditor && GIsClient )
	{
		WConfigWizard D;
		WWizardPage* Page = NULL;
		if( ParseParam(appCmdLine(),TEXT("safe")) || appStrfind(appCmdLine(),TEXT("readini")) )
		{
			Page = new WConfigPageSafeMode(&D);
			D.Title=LocalizeGeneral(TEXT("SafeMode"),TEXT("Startup"));
		}
		else if( FirstRun<ENGINE_VERSION )
		{
			GConfig->SetString(TEXT("Engine.Engine"),TEXT("RenderDevice"),TEXT("D3DDrv.D3DRenderDevice"));
		}
		//else if( !AlreadyRunning && GFileManager->FileSize(TEXT("Running.ini"))>=0 )
		//	{Page = new WConfigPageSafeMode(&D); D.Title=LocalizeGeneral(TEXT("RecoveryMode"),TEXT("Startup"));}
		if( Page )
		{
			ExitSplash();
			D.Advance( Page );
			if( !D.DoModal() )
				return NULL;
			InitSplash(NULL);
		}
	}
	unguard;
    #endif

	// Create is-running semaphore file.
	FArchive* Ar = GFileManager->CreateFileWriter(TEXT("Running.ini"),0);
	if( Ar )
		delete Ar;

	// Update first-run.
	if( FirstRun<ENGINE_VERSION )
		FirstRun = ENGINE_VERSION;
	GConfig->SetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );


	// Create the global engine object.
	UClass* EngineClass;
	if( !GIsEditor )
	{
		// Create game engine.
		EngineClass = UObject::StaticLoadClass( UGameEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.GameEngine"), NULL, LOAD_NoFail, NULL );
	}
	else
	{
		// Editor.
		EngineClass = UObject::StaticLoadClass( UEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );
	}
	UEngine* Engine = ConstructObject<UEngine>( EngineClass );
	Engine->Init();
	debugf( TEXT("Startup time: %f seconds"), appSeconds()-LoadTime );

	return Engine;
	unguard;
}



/*-----------------------------------------------------------------------------
	Render device selection.
-----------------------------------------------------------------------------*/

//
// Each -*RENDERER switch names a render device and, for the GL devices, the SDL2 viewport manager it runs on.
// Only an explicit switch acts here; with none, the ini and the driver replacement in InitEngine() decide.
// Availability is a StaticFindObject on the registered classes (no package load, so no throw). A switch whose
// device is not in this build logs "not available" and is ignored. A choice that survives Engine->Init() is
// written to the ini by PersistRenderChoiceIfSwitched(), so it sticks next boot.
struct FRenderSwitch { const TCHAR* Param; const TCHAR* Device; const TCHAR* Viewport; };
static const FRenderSwitch GRenderSwitches[] =
{
	// -RENDERER switch           render device                          coupled SDL viewport (NULL = keep current)
	{ TEXT("GL4ESRENDERER"),    TEXT("GL4ESDrv.GL4ESRenderDevice"),   TEXT("SDL2Drv.SDL2Client") }, // GLES2/WebGL through gl4es
	{ TEXT("OPENGLRENDERER"),   TEXT("OpenGLDrv.OpenGLRenderDevice"), TEXT("SDL2Drv.SDL2Client") }, // desktop GL
	{ TEXT("SOFTWARERENDERER"), TEXT("PixoDrv.PixoRenderDevice"),     NULL                       }, // Pixomatic (32-bit x86 native)
	{ TEXT("NULLRENDERER"),     TEXT("Engine.NullRenderDevice"),      NULL                       }, // GPU-free (always available)
};

// The class a -*RENDERER switch requested (empty if none) -- persisted after a successful Init().
static FString GRequestedRenderDevice;


// The low-detail settings the software renderer needs; applied when -SOFTWARERENDERER selects PixoDrv.
static void ApplySoftwareRendererConfig()
{
	// Write to whichever viewport driver is active.
	const TCHAR* ActiveVP = GConfig->GetStr(TEXT("Engine.Engine"),TEXT("ViewportManager"),TEXT("System"));
	GConfig->SetString( ActiveVP, TEXT("Decals"			), TEXT("False"						));
	GConfig->SetString( ActiveVP, TEXT("DecoLayers"		), TEXT("False"						));
	GConfig->SetString( ActiveVP, TEXT("Coronas"			), TEXT("False"						));
	GConfig->SetString( ActiveVP, TEXT("Projectors"		), TEXT("False"						));
	GConfig->SetString( ActiveVP, TEXT("NoFractalAnim"		), TEXT("True"						));
	GConfig->SetString( ActiveVP, TEXT("NoDynamicLights"	), TEXT("True"						));
	GConfig->SetString( TEXT("UnrealGame.UnrealPawn"), TEXT("bPlayerShadows"	), TEXT("False"						), GUserIni );
	GConfig->SetString( TEXT("Engine.Vehicle"		), TEXT("bVehicleShadows"	), TEXT("False"						), GUserIni );
}

// Choose the render device from a -*RENDERER switch before Engine->Init(): sets RenderDevice and, for the GL
// devices, the matching ViewportManager. The first switch present wins.
static void SelectRenderDevice()
{
	for( INT i=0; i<ARRAY_COUNT(GRenderSwitches); i++ )
	{
		const FRenderSwitch& Sw = GRenderSwitches[i];
		if( ParseParam( appCmdLine(), Sw.Param ) )
		{
			if( IsRenderDeviceAvailable( Sw.Device ) )
			{
				debugf( TEXT("-%s selects render device %s"), Sw.Param, Sw.Device );
				GConfig->SetString( TEXT("Engine.Engine"), TEXT("RenderDevice"), Sw.Device );
				if( Sw.Viewport )
					GConfig->SetString( TEXT("Engine.Engine"), TEXT("ViewportManager"), Sw.Viewport );
				if( appStricmp( Sw.Param, TEXT("SOFTWARERENDERER") )==0 )
					ApplySoftwareRendererConfig();
				GRequestedRenderDevice = Sw.Device; // persist iff it survives Init (PersistRenderChoiceIfSwitched)
			}
			else
			{
				debugf( NAME_Warning, TEXT("render device %s not available (not compiled into this build) -> keeping the default; -%s ignored"), Sw.Device, Sw.Param );
			}
			return; // first -*RENDERER switch present wins
		}
	}
}

// After Engine->Init(): persist a -*RENDERER choice when the engine did not replace it during Init (the device
// itself initialises later, at level bring-up). Flush(0) writes the ini (the web host persists it).
static void PersistRenderChoiceIfSwitched()
{
	if( GRequestedRenderDevice.Len()==0 )
		return;
	const TCHAR* Active = GConfig->GetStr( TEXT("Engine.Engine"), TEXT("RenderDevice"), TEXT("System") );
	if( appStricmp( Active, *GRequestedRenderDevice )==0 )
	{
		GConfig->Flush( 0 );
		debugf( TEXT("persisted render device %s as the default (sticks next boot)"), *GRequestedRenderDevice );
	}
	else
	{
		debugf( NAME_Warning, TEXT("render device changed from %s to %s during init -> not persisted"), *GRequestedRenderDevice, Active );
	}
}

//
// Main entry point.
// This is an example of how to initialize and launch the engine.
//
#if defined(USE_PIXOMATIC) && defined(__linux__)
// The READ_IMPLIES_EXEC re-exec below uses Linux's personality(2).
#include <sys/personality.h>
#include <unistd.h>
#include <errno.h>

// Whether this launch runs Pixomatic, decided as the launch decides: the first -*RENDERER switch whose device
// this build has (SelectRenderDevice's table and order), else the ini's [Engine.Engine] RenderDevice (InitEngine
// never replaces a device with Pixomatic); -testrendev= names the one device it tests. Needs appInit and the
// static registrants.
static UBOOL LaunchRunsPixomatic()
{
	const TCHAR* Pixo = TEXT("PixoDrv.PixoRenderDevice");
	if( !IsRenderDeviceAvailable( Pixo ) )
		return 0;
	FString Tested;
	if( Parse( appCmdLine(), TEXT("testrendev="), Tested ) )
		return appStricmp( *Tested, Pixo )==0;
	for( INT i=0; i<ARRAY_COUNT(GRenderSwitches); i++ )
	{
		if( ParseParam( appCmdLine(), GRenderSwitches[i].Param ) )
		{
			if( IsRenderDeviceAvailable( GRenderSwitches[i].Device ) )
				return appStricmp( GRenderSwitches[i].Device, Pixo )==0;
			break;
		}
	}
	return appStricmp( GConfig->GetStr( TEXT("Engine.Engine"), TEXT("RenderDevice"), TEXT("System") ), Pixo )==0;
}

// Pixomatic's JIT executes code from malloc'd heap buffers (including a drawverts path PixoCodeExec.c cannot
// hook), and a modern kernel maps the heap NX. With the READ_IMPLIES_EXEC personality the brk heap is
// executable, so set it and re-exec once (it applies to a freshly exec'd process). Decided from the resolved
// render device, since a persisted -SOFTWARERENDERER choice selects Pixomatic from the ini. An executable heap
// is a security downgrade, accepted only for the software renderer. Called after appInit, before any window.
static void ReexecIfPixomaticNeedsAnExecutableHeap( char** argv )
{
	if( !LaunchRunsPixomatic() )
		return;
	int persNow = personality( 0xffffffffUL );
	if( persNow != -1 && (persNow & READ_IMPLIES_EXEC) )
	{
		debugf( TEXT("Pixomatic runs with an executable heap (READ_IMPLIES_EXEC)") );
		return;
	}
	debugf( TEXT("the render device is Pixomatic -> re-exec with READ_IMPLIES_EXEC (its JIT runs code from the heap)") );
	if( persNow != -1 && personality( (unsigned long)persNow | READ_IMPLIES_EXEC ) != -1 )
		execv( "/proc/self/exe", argv );
	debugf( NAME_Warning, TEXT("the re-exec failed (errno %i) -> Pixomatic runs on an NX heap and will likely crash"), errno );
}
#endif
#if defined(__ANDROID__)
// Android: SDLActivity dlsym's "SDL_main" from libUT2004.so and JNI-calls it. extern "C" +
// explicit default visibility guarantees the symbol resolves for dlsym.
extern "C" __attribute__((visibility("default"))) int SDL_main(int argc, char **argv)
#else
int main(int argc, char **argv)
#endif
{
#if defined(__ANDROID__)
    AndroidRedirectStdioToLogcat();
#endif
    appArgv0(argv[0]);

	// Up to 4096 characters (the size of Core's GCmdLine); a token that does not fit is dropped whole, with a
	// warning once the log is up.
	TCHAR CmdLine[4096] = TEXT("");
	UBOOL CmdLineCut = 0;
	for (INT i = 1; i < argc; i++)
	{
		const TCHAR* Arg = appFromAnsi(argv[i]);
		if (appStrlen(CmdLine) + appStrlen(Arg) + 2 >= ARRAY_COUNT(CmdLine))
		{
			CmdLineCut = 1;
			break;
		}
		if (i > 1)
			appStrcat(CmdLine, TEXT(" "));
		appStrcat(CmdLine, Arg);
	}

#if MACOSX  // GameRanger support.  --ryan.
    if (GRCheckFileForCmd())
        GRGetWaitingCmd();

    if (GRIsHostCmd())
    {
        if (CmdLine[0])
            appStrcat(CmdLine, TEXT(" "));
        appStrcat(CmdLine, TEXT("-MainMenu=GUI2K4.UT2K4GamePageMP"));
        GameRangerClientsWaiting = 1;
    }
    else if (GRIsJoinCmd())
    {
        TCHAR cmd[64];
        const TCHAR *addr = appFromAnsi(GRGetJoinAddressStr());
        appSprintf(cmd, TEXT("%s:%u"), addr, (unsigned int) GRGetPortNumber());
        if (CmdLine[0])
            appStrcat(CmdLine, TEXT(" "));
        appStrcat(CmdLine, cmd);
    }
#endif

	// Remember instance.
	INT ErrorLevel = 0;
	GIsStarted     = 1;

    // Initialize GModule.
	strcpy( GModule, appToAnsi(TEXT("UT2004")) );
	appStrcpy( GPackage, appPackage() );

	// Begin guarded code.
#ifndef _DEBUG
	try
	{
#endif

		// Init core.
		GIsClient = GIsGuarded = 1;
		appInit( GPackage, CmdLine, &Malloc, &Log, &Error, &Warn, &FileManager, FConfigCacheIni::Factory, 1 );
		if( CmdLineCut )
			debugf( NAME_Warning, TEXT("The command line is longer than %i characters: the arguments after '%s' were dropped"), ARRAY_COUNT(CmdLine), CmdLine + Max(0, appStrlen(CmdLine) - 40) );

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
        AUTO_INITIALIZE_REGISTRANTS_SDL2DRV;
        AUTO_INITIALIZE_REGISTRANTS_FIRE;
        AUTO_INITIALIZE_REGISTRANTS_ALAUDIO;
#if UE_RENDER_OPENGLDRV
        AUTO_INITIALIZE_REGISTRANTS_OPENGLDRV;
#endif
#if UE_RENDER_GL4ESDRV
        AUTO_INITIALIZE_REGISTRANTS_GL4ESDRV;
#endif
        #if USE_PIXOMATIC
        AUTO_INITIALIZE_REGISTRANTS_PIXODRV;
        #endif
        AUTO_INITIALIZE_REGISTRANTS_IPDRV;
#ifdef __EMSCRIPTEN__
        AUTO_INITIALIZE_REGISTRANTS_WEBRTCDRV;
#ifdef UE_WITH_PROXYNET
        AUTO_INITIALIZE_REGISTRANTS_PROXYNETDRV;
#endif
#endif
        RegisterNamesXInterface();
        AUTO_INITIALIZE_REGISTRANTS_XINTERFACE;
        AUTO_INITIALIZE_REGISTRANTS_XGAME;
        AUTO_INITIALIZE_REGISTRANTS_UWEB;
        RegisterNamesOnslaught();
        AUTO_INITIALIZE_REGISTRANTS_ONSLAUGHT;
        // autoInitializeRegistrantsEditor(Lookup); // the Editor is not built

        GNativeLookupFuncs[Lookup++] = &FindUWebUWebResponseNative;
        GNativeLookupFuncs[Lookup++] = &FindUWebUWebRequestNative;
        check( Lookup < ARRAY_COUNT(GNativeLookupFuncs) );


//!!! FIXME: linker removes this as deadcode on MacOS X without an explicit reference...
char blah;
#if USE_PIXOMATIC
extern BYTE GLoadedPixoDrv; snprintf(&blah, 1, "%d", (int) GLoadedPixoDrv);
#endif
extern BYTE GLoadedSDL2Drv; snprintf(&blah, 1, "%d", (int) GLoadedSDL2Drv);
#if UE_RENDER_OPENGLDRV
extern BYTE GLoadedOpenGLDrv; snprintf(&blah, 1, "%d", (int) GLoadedOpenGLDrv);
#endif
#if UE_RENDER_GL4ESDRV
extern BYTE GLoadedGL4ESDrv; snprintf(&blah, 1, "%d", (int) GLoadedGL4ESDrv);
#endif
extern BYTE GLoadedALAudio; snprintf(&blah, 1, "%d", (int) GLoadedALAudio);
#endif

#if defined(USE_PIXOMATIC) && defined(__linux__)
		// The ini is read and the render-device classes are registered: decide whether this launch runs
		// Pixomatic, and re-exec with an executable heap if so (before the splash opens any window).
		ReexecIfPixomaticNeedsAnExecutableHeap( argv );
#endif

		// Init mode.
		GIsServer     = 1;
		GIsClient     = !ParseParam(appCmdLine(),TEXT("SERVER"));
		GIsEditor     = 0;
		GIsScriptable = 1;
		GLazyLoad     = !GIsClient || ParseParam(appCmdLine(),TEXT("LAZY"));

		// Figure out whether to show log or splash screen.
		UBOOL ShowLog = ParseParam(CmdLine,TEXT("LOG"));

		TCHAR *LogoFName = appStaticString1024();
		if( !Parse( appCmdLine(), TEXT("-USERLOGO="), LogoFName, 1024 ) )
			appSprintf( LogoFName, TEXT("%sLogo.bmp"), GPackage );

		//FString Filename = FString(TEXT("../Help")) * GPackage + TEXT("Logo.bmp");
		FString Filename = FString(TEXT("../Help")) * LogoFName;
		if ( appStrlen(GModPath)>0 )
			Filename = FString(GModPath) * FString(TEXT("Help")) * GModName + TEXT("Logo.bmp");
		if( GFileManager->FileSize(*Filename)<0 )
			Filename = FString(TEXT("../Help")) * GPackage + TEXT("Logo.bmp"); // gam
		//appStrcpy( GPackage, appPackage() );
		if( (!ShowLog) &&
            (!ParseParam(CmdLine,TEXT("server"))) &&
            (!ParseParam(CmdLine,TEXT("nosplash"))) &&
            !appStrfind(CmdLine,TEXT("TestRenDev")) )
			InitSplash( *Filename );

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

		// -GL4ESRENDERER / -OPENGLRENDERER / -SOFTWARERENDERER / -NULLRENDERER (SelectRenderDevice).
		SelectRenderDevice();

		// Don't update ini files if benchmarking.
		if( GIsBenchmarking || ParseParam(appCmdLine(),TEXT("NOINI")) || ParseParam(appCmdLine(),TEXT("EXITAFTERDEMO")) )
		{
			GConfig->Detach( GIni );
			GConfig->Detach( GUserIni );
		}

		// Ugly resolution overriding code.
		FString ScreenWidth;
		FString ScreenHeight;
		UBOOL	OverrideResolution = 0;

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
		if( ParseParam(appCmdLine(),TEXT("1280x1024")) )
		{
			ScreenWidth			= TEXT("1280");
			ScreenHeight		= TEXT("1024");
			OverrideResolution	= 1;
		}
		if( ParseParam(appCmdLine(),TEXT("1600x1024")) )
		{
			ScreenWidth			= TEXT("1600");
			ScreenHeight		= TEXT("1024");
			OverrideResolution	= 1;
		}
		if( ParseParam(appCmdLine(),TEXT("1600x1200")) )
		{
			ScreenWidth			= TEXT("1600");
			ScreenHeight		= TEXT("1200");
			OverrideResolution	= 1;
		}
		if( ParseParam(appCmdLine(),TEXT("1920x1200")) )
		{
			ScreenWidth			= TEXT("1600");
			ScreenHeight		= TEXT("1200");
			OverrideResolution	= 1;
		}

		if( OverrideResolution )
		{
			const TCHAR* ActiveVP2 = GConfig->GetStr(TEXT("Engine.Engine"),TEXT("ViewportManager"),TEXT("System"));
			GConfig->SetString( ActiveVP2, TEXT("FullscreenViewportX"), *ScreenWidth  );
			GConfig->SetString( ActiveVP2, TEXT("FullscreenViewportY"), *ScreenHeight );
		}

		// Set sane default values on initial run.
		INT FirstRun = 0;
		GConfig->GetInt( TEXT("FirstRun"), TEXT("FirstRun"), FirstRun );
		if( GIsClient && (FirstRun==0) )
			SetInitialConfiguration();

		// Create log window, but only show it if ShowLog.
        #if 0
		GLogWindow = new WLog( Log.Filename, Log.LogAr, TEXT("GameLog") );
		GLogWindow->OpenWindow( ShowLog, 0 );
		GLogWindow->Log( NAME_Title, LocalizeGeneral(TEXT("Start"),GPackage) );
		if( GIsClient )
			SetPropX( *GLogWindow, TEXT("IsBrowser"), (HANDLE)1 );
        #endif

		#if ((MACOSX) && (!DEMOVERSION))
		extern int IsDiscInserted(void);
		if( !IsDiscInserted() )
		{
		    GIsRequestingExit = 1;
			//English
            appMsgf( 0, TEXT("Please insert the UT2004 disc and restart the game") );
            // German
			//appMsgf( 0, TEXT("Bitte legen Sie die UT2004-DVD ein und starten Sie das Spiel erneut") );
		}
		#endif

		// Init engine.
		UEngine* Engine = NULL;
		if (!GIsRequestingExit)
			Engine = InitEngine();

		// Persist a -*RENDERER choice that survived Engine->Init().
		if( Engine )
			PersistRenderChoiceIfSwitched();

#ifdef __EMSCRIPTEN__
		GEngineForExec = Engine;
#endif

		if( Engine )
		{
			// Hide splash screen.
			ExitSplash();

#ifdef __EMSCRIPTEN__
			// Mute audio while the tab is hidden.
			emscripten_set_visibilitychange_callback(
			    nullptr, EM_TRUE, on_visibility_change);
			// Canvas resizes reach SDL2 as WINDOWEVENT_RESIZED; no callback is needed here.
#endif

			// Optionally Exec an exec file
			FString Temp;
			if( Parse(CmdLine, TEXT("EXEC="), Temp) )
			{
				Temp = FString(TEXT("exec ")) + Temp;
				if( Engine->Client && Engine->Client->Viewports.Num() && Engine->Client->Viewports(0) )
					Engine->Client->Viewports(0)->Exec( *Temp, *GLog );
			}

#ifdef BETAEXPIRE
			if ( time(NULL) > (BETAEXPIRE + 30 * 24 * 60 * 60) )
			{
				GIsRequestingExit = 1;
				appMsgf( 0, TEXT("This beta has expired.") );
				while (1) _exit(42);
			}
#endif

			// Validate cd key
			if( !ValidateCDKey() )
			{
				GIsRequestingExit = 1;
				appMsgf( 0, LocalizeError(TEXT("InvalidCDKey"),TEXT("Engine")) );
			}

			// Start main engine loop, including the Windows message pump.
			if( !GIsRequestingExit )
				MainLoop( Engine );
		}

		#if MACOSX
		guard(GRHostClosed);
		GRHostClosed();
		unguard;
		#endif

		// Clean shutdown.
		GFileManager->Delete(TEXT("Running.ini"),0,0);

        #if 0
		RemovePropX( *GLogWindow, TEXT("IsBrowser") );
        #endif

		GLog->Log( NAME_Title, LocalizeGeneral(TEXT("Exit"),GPackage) );

		appPreExit();
		GIsGuarded = 0;
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
	return((int) ErrorLevel);
}


// Query the HW for caps.
static void SetInitialConfiguration()
{
	guard(SetInitialConfiguration);

    // !!! FIXME: Be nice to get this into Linux in some form...  --ryan.

#if 0
	//!!vogel: currently only queries primary device though it will be used by default anyways.

	// Variables.
	HRESULT Result;
	INT		OnboardVideoMemory	= MAXINT;
	UBOOL	HardwareTL			= true,
			DeviceCapsValid		= false;
	FLOAT	CPUSpeed			= 0.000001 / GSecondsPerCycle;
	D3DADAPTER_IDENTIFIER8		DeviceIdentifier;
	D3DCAPS8					DeviceCaps8;

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

	// As GetAvailableVidMem is called before entering fullscreen the numbers are slightly less
	// as it reports "local video memory" - "memory used for desktop". Though this shouldn't matter 
	// that much as the user can always override it and as the default values should be conservative.
	if( OnboardVideoMemory != MAXINT )
	{
		OnboardVideoMemory = OnboardVideoMemory / 1024 / 1024;
		debugf( NAME_Init, TEXT("D3D Device: Video memory on board: %i"), OnboardVideoMemory );
	}

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
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullScreenViewportX"	),	TEXT("320"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullScreenViewportY"	),	TEXT("240"			));

		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailInterface"	),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailTerrain"	),	TEXT("VeryLow"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWeaponSkin"),	TEXT("VeryLow"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailPlayerSkin"),	TEXT("VeryLow"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWorld"		),	TEXT("Low"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailRenderMap"	),	TEXT("VeryLow"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailLightmap"	),	TEXT("VeryLow"		));

		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Decals"					),	TEXT("False"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("DecoLayers"				),	TEXT("False"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Coronas"				),	TEXT("False"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Projectors"				),	TEXT("False"		));

		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("UseTrilinear"			),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("UseCubemaps"			),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("HighDetailActors"		),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("SuperHighDetailActors"),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("Use16bit"				),	TEXT("True"			));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("Use16bitTextures"		),	TEXT("True"			));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("DetailTextures"		),	TEXT("False"		));		

		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("NoFractalAnim"			),	TEXT("True"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("NoDynamicLights"		),	TEXT("True"			));
		GConfig->SetString( TEXT("XGame.xPawn")			, TEXT("bPlayerShadows"			),	TEXT("False"		), GUserIni );
	}
	else if( OnboardVideoMemory <= 16 )
	{
		// 16 MByte
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullScreenViewportX"	),	TEXT("512"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("FullScreenViewportY"	),	TEXT("384"			));

		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailInterface"	),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailTerrain"	),	TEXT("Low"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWeaponSkin"),	TEXT("Low"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailPlayerSkin"),	TEXT("Low"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWorld"		),	TEXT("VeryLow"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailRenderMap"	),	TEXT("Low"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailLightmap"	),	TEXT("Low"			));

		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Decals"					),	TEXT("False"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("DecoLayers"				),	TEXT("False"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Coronas"				),	TEXT("False"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Projectors"				),	TEXT("False"		));

		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("UseTrilinear"			),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("UseCubemaps"			),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("HighDetailActors"		),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("SuperHighDetailActors"),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("Use16bit"				),	TEXT("True"			));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("Use16bitTextures"		),	TEXT("True"			));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("DetailTextures"		),	TEXT("False"		));		

		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("NoFractalAnim"			),	TEXT("True"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("NoDynamicLights"		),	TEXT("True"			));
		GConfig->SetString( TEXT("XGame.xPawn")			, TEXT("bPlayerShadows"			),	TEXT("False"		), GUserIni );
	}
	else if( OnboardVideoMemory <= 32 )
	{
		// 32 MByte
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailInterface"	),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailTerrain"	),	TEXT("Lower"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWeaponSkin"),	TEXT("Lower"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailPlayerSkin"),	TEXT("Low"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWorld"		),	TEXT("Low"			));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailRenderMap"	),	TEXT("Lower"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailLightmap"	),	TEXT("Lower"		));

		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("DecoLayers"				),	TEXT("False"		));

		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("UseTrilinear"			),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("HighDetailActors"		),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("SuperHighDetailActors"),	TEXT("False"		));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("Use16bit"				),	TEXT("True"			));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"),	TEXT("DetailTextures"		),	TEXT("False"		));
	}
	else if( OnboardVideoMemory <= 64 )
	{
		// 64 MByte
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailInterface"	),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailTerrain"	),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWeaponSkin"),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailPlayerSkin"),	TEXT("Lower"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWorld"		),	TEXT("Normal"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailRenderMap"	),	TEXT("Lower"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailLightmap"	),	TEXT("Normal"		));

		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("SuperHighDetailActors"),	TEXT("False"		));
	}
	else if( OnboardVideoMemory <= 128 )
	{
		// 128 MByte
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailInterface"	),	TEXT("Higher"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailTerrain"	),	TEXT("Higher"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWeaponSkin"),	TEXT("Higher"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailPlayerSkin"),	TEXT("Higher"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWorld"		),	TEXT("Higher"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailRenderMap"	),	TEXT("Higher"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailLightmap"	),	TEXT("Higher"		));
	}
	else
	{
		// 256 MByte and more
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailInterface"	),	TEXT("UltraHigh"	));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailTerrain"	),	TEXT("UltraHigh"	));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWeaponSkin"),	TEXT("UltraHigh"	));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailPlayerSkin"),	TEXT("UltraHigh"	));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailWorld"		),	TEXT("UltraHigh"	));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailRenderMap"	),	TEXT("UltraHigh"	));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("TextureDetailLightmap"	),	TEXT("UltraHigh"	));

		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("UseCompressedLightmaps"	),	TEXT("False"	));
	}

	// Generic CPU defaults.
	if( CPUSpeed < 700 )
	{
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Coronas"		),	TEXT("False"	));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Decals"			),	TEXT("False"	));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("DecoLayers"		),	TEXT("False"	));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("NoFractalAnim"	),	TEXT("True"		));
		GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("NoDynamicLights"),	TEXT("True"			));

		GConfig->SetString( TEXT("XGame.xPawn")			, TEXT("bPlayerShadows"	),	TEXT("False"	), GUserIni );

		GConfig->SetString( TEXT("Engine.LevelInfo")	, TEXT("PhysicsDetailLevel"		),	TEXT("PDL_Low"	));

		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("HighDetailActors"		),	TEXT("False"	));
		GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("SuperHighDetailActors"),	TEXT("False"	));
	}
	else if( CPUSpeed < 1000 )
	{
		if( !HardwareTL )
		{
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("Coronas"		),	TEXT("False"	));
			GConfig->SetString( TEXT("WinDrv.WindowsClient"), TEXT("NoFractalAnim"	),	TEXT("True"		));

			GConfig->SetString( TEXT("XGame.xPawn")			, TEXT("bPlayerShadows"	),	TEXT("False"	), GUserIni );

			GConfig->SetString( TEXT("Engine.LevelInfo")	, TEXT("PhysicsDetailLevel"		),	TEXT("PDL_Low"	));

			GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("HighDetailActors"		),	TEXT("False"	));
			GConfig->SetString( TEXT("D3DDrv.D3DRenderDevice"), TEXT("SuperHighDetailActors"),	TEXT("False"	));
		}
		else
			GConfig->SetString( TEXT("Engine.LevelInfo"), TEXT("PhysicsDetailLevel"		),	TEXT("PDL_Medium"	));
	}
	else if( CPUSpeed < 1500 )
	{
		if( !HardwareTL )
		{
			GConfig->SetString( TEXT("XGame.xPawn"), TEXT("bPlayerShadows"		),	TEXT("False"	), GUserIni );
			GConfig->SetString( TEXT("Engine.LevelInfo"), TEXT("PhysicsDetailLevel"		),	TEXT("PDL_Medium"	));
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
#endif
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

