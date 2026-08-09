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

// Visibility change callback: mute audio when tab is hidden (D-16, BRWSR-03).
// The engine keeps ticking -- only audio is silenced to prevent jarring sound
// when the user switches browser tabs.
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
// ufront 2.13.7 (option E) — YIELD ONCE PER FRAME, BUT WITHOUT setTimeout's CLAMP.
//
// The engine thread MUST return to its event loop every frame: with explicitSwapControl and no
// -sOFFSCREEN_FRAMEBUFFER, emscripten_webgl_commit_frame is a no-op ("swap is implicit") and a
// worker-owned OffscreenCanvas is composited when the worker's task ENDS. Yielding less often was
// tried and MEASURED: at a 100 ms yield the screen updated 9.8 times a second — exactly the yield
// rate — while the engine cheerfully reported 152 fps, i.e. it was counting frames nobody ever saw,
// and the heavy demos regressed for real. So frequency is fixed; only COST is available.
//
// appSleep(0) -> emscripten_sleep(0) pays TWO costs. The Asyncify unwind/rewind of the engine stack is
// unavoidable here. The second is pure waste: emscripten_sleep resumes from setTimeout, and NO browser
// honours a 0 ms timer — the HTML spec's timer clamp floors it at >= 1 ms (worse once nested). We do not
// want a timer at all; we want the shortest possible task boundary.
//
// A MessagePort message is exactly that: a macrotask, dispatched on the next turn, with NO clamp. Same
// task boundary the compositor needs, minus the timer floor. (This is what emscripten_set_immediate uses
// internally — libeventloop.js:57-80 — but that is a callback API, and the engine loop needs to BLOCK
// here, so EM_ASYNC_JS + Asyncify is the shape that fits.)
//
// EM_ASYNC_JS names its import __asyncjs__ufront_yield_frame, and link.py's DEFAULT_ASYNCIFY_IMPORTS is
// ['__asyncjs__*'], so this is registered with Asyncify automatically — no ASYNCIFY_IMPORTS edit.
EM_ASYNC_JS(void, ufront_yield_frame, (void), {
    let y = globalThis.__ufrontYield;
    if (!y) {
        // One channel for the process; resolvers queue in FIFO order, so a message always wakes the
        // caller that posted it. Created lazily because this runs on the engine's pthread.
        const ch = new MessageChannel();
        const q = [];
        ch.port1.onmessage = () => { const r = q.shift(); if (r) r(); };
        ch.port1.start();
        y = globalThis.__ufrontYield = { ch, q };
    }
    await new Promise((resolve) => { y.q.push(resolve); y.ch.port2.postMessage(0); });
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
#include "SDL2Drv.h"	// ufront 2.34: the only viewport driver — SDL 1.2 / SDLDrv is retired
#if UT_RENDER_OPENGLDRV
#include "OpenGLDrv.h"
#endif
#if UT_RENDER_GL4ESDRV
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
#endif
#include "UnIpDrvCommandlets.h"
#include "UnIpDrvNative.h"
#include "XInterface.h"
#include "UWeb.h"
#include "UWebNative.h"
#include "OnslaughtPrivate.h"
// void autoInitializeRegistrantsEditor(INT &Lookup); // D-10: Editor excluded

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

// Log file. ufront (dual-sink logging): FOutputDeviceFile on BOTH platforms (was FOutputDeviceStdout,
// console-only, on web). It writes a log FILE (MEMFS on web) AND echoes each line to stdout via the echo
// in its Serialize -> Module.print -> console.log on web / the terminal on native. Two independent sinks
// mean a silent divergence is caught by the e2e log-parity check, instead of leaving you blind (the
// ut2004 native "stall" misdiagnosis). Matches ut99 (2.8) / unreal1 (2.6e). (A separate native GLog-wiring
// quirk means the native launcher's GLog isn't reaching this device yet -- tracked; web is unaffected,
// as ut99/unreal1 prove FOutputDeviceFile+echo reaches the browser console.)
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
	guard(EnforceTickRate);
	if( !GUseFixedTimeStep && IncludeMessaging )
	{
		FLOAT MaxTickRate = Engine->GetMaxTickRate();
		if( MaxTickRate>0.0 )
		{
			FLOAT Delta = (1.0/MaxTickRate) - (appSeconds()-OldTime);
			appSleep( Max(0.f,Delta) );
		}
	}
	unguard;

#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
	// ufront 2.13.2 — PUMP THIS THREAD'S EVENT LOOP once per frame.
	//
	// UGameEngine::GetMaxTickRate() returns 0 for a standalone client (no NetDriver) — i.e. the whole
	// menu and every offline match — so the limiter above calls appSleep for a NETWORK client only, and
	// this loop is otherwise a tight loop that never returns to JS. That was survivable while the engine
	// owned the browser's main thread and the host called _ue_exec_command directly. On a worker it is
	// not: `postMessage` (the ONLY route left for a console command, since the export must run on the
	// thread that owns the engine) is delivered on an event-loop turn, and so are the browser events SDL
	// forwards here. Measured: with no yield the engine ran and rendered perfectly, while `ueExec` and a
	// live canvas resize were both silently swallowed — the messages were queued and never dispatched.
	//
	// appSleep(0) is the yield (emscripten_sleep -> a 0 ms setTimeout that Asyncify unwinds to), and it
	// is skipped whenever the limiter already slept, so a net client keeps exactly one yield per frame
	// and its frame budget is untouched. Non-threaded builds are byte-identical: this whole block is
	// compiled out. (2.13.3 drops ASYNCIFY, at which point this yield needs a different mechanism —
	// noted there.)
	guard(WorkerYield);
	if( GUseFixedTimeStep || !IncludeMessaging || Engine->GetMaxTickRate() <= 0.f )
		// ufront 2.13.7 (option E): same yield, EVERY frame, exactly as 2.13.2 — only the resume is
		// scheduled by an unclamped MessagePort macrotask instead of a >= 1 ms setTimeout. Frequency is
		// deliberately unchanged: the yield is the frame's present point (see ufront_yield_frame above).
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
// Engine pointer for deferred WebRTC connect (webrtc_deferred_open in WebRTCNetDriver.cpp) and for
// ue_exec_command below.
UEngine* GWebRTCEngine = NULL;

// ADR-10: run a console command and RETURN its captured output (not fire-and-forget). Declared in the
// frozen ABI (vendor/glue/ue_abi.h) for EVERY engine; unreal1 (SDLLaunch.cpp) and ut99 (Launch.cpp) have
// implemented it since 2.6c/2.7d, but this engine's "impl lands at first rebuild" never happened — so the
// ut2004 wasm shipped without the export and the E2E had to reach for `?map=` autostart instead. This is
// the same implementation as the two UE1 launchers, verbatim: it needs no encoding adapter because the
// Emscripten build compiles with NO_UNICODE_OS_SUPPORT=1 (CMakeLists.txt), i.e. TCHAR is ANSICHAR here,
// exactly as on UE1.
//
// FStringOutputDevice is a string-capturing FOutputDevice (UnTemplate.h); the text is held in a static
// FString so the returned pointer stays valid until the next call (the JS side reads it synchronously
// during the ccall). Returns an empty string before the engine exists, never NULL.
extern "C" char* EMSCRIPTEN_KEEPALIVE ue_exec_command( const char* Cmd )
{
	static FString Captured;
	Captured = TEXT("");
	if( GWebRTCEngine && Cmd )
	{
		FStringOutputDevice Out;
		GWebRTCEngine->Exec( Cmd, Out );
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

    // ★ ufront 2.34 — "if we have the wrong driver, select a NON-NULL driver" (owner's rule). Every one of
    // the substitutions below used to be keyed on WHICH BACKEND WAS COMPILED (`#if USE_SDL12`), which was
    // only ever a proxy for "is that class in this binary". Now that SDL 1.2 is retired and ONE binary
    // carries both GL render devices, the backend can no longer answer the question — so ask it directly.
    // IsRenderDeviceAvailable() is a StaticFindObject over the already-registered classes, which is exactly
    // what "is it in this binary" means for a static build.
    //
    // The rule in every case: a device the ini names and this build HAS is left alone; anything else is
    // replaced by the best REAL alternative, in preference order. This is what lets one binary boot from a
    // retail Windows ini (WinDrv + D3D), from a previous gl4es run's ini, or from a legacy one.
    if( !ParseParam(appCmdLine(),TEXT("NoForceSDLDrv")) )
    {
        // Viewport client: WinDrv (retail Windows) and a stale SDLDrv both land on whatever SDL client
        // this build actually has.
        const TCHAR* Viewport = GConfig->GetStr(TEXT("Engine.Engine"),TEXT("ViewportManager"),TEXT("System"));
        if( !IsRenderDeviceAvailable(Viewport) )
        {
            static const TCHAR* const ViewportPreference[] = { TEXT("SDL2Drv.SDL2Client"), TEXT("SDLDrv.SDLClient") };
            for( INT i=0; i<ARRAY_COUNT(ViewportPreference); i++ )
            {
                if( IsRenderDeviceAvailable(ViewportPreference[i]) )
                {
                    debugf(TEXT("ufront 2.34: ViewportManager=%s is not in this build -> %s"), Viewport, ViewportPreference[i]);
                    GConfig->SetString(TEXT("Engine.Engine"), TEXT("ViewportManager"), ViewportPreference[i]);
                    break;
                }
            }
        }
    }

    if( !ParseParam(appCmdLine(),TEXT("NoForceGL4ES")) )
    {
        // Render device: D3DDrv/D3D9Drv (Windows retail), GLES3Drv (never built) and a driver from the
        // other backend all resolve to the best GL device present. Desktop GL leads where it exists — it
        // is the native path; GL4ES is a translation layer, which is the right answer only when it is the
        // only one compiled in (the web and Android builds).
        const TCHAR* Device = GConfig->GetStr(TEXT("Engine.Engine"),TEXT("RenderDevice"),TEXT("System"));
        if( !IsRenderDeviceAvailable(Device) )
        {
            static const TCHAR* const DevicePreference[] = { TEXT("OpenGLDrv.OpenGLRenderDevice"),
                                                             TEXT("GL4ESDrv.GL4ESRenderDevice") };
            for( INT i=0; i<ARRAY_COUNT(DevicePreference); i++ )
            {
                if( IsRenderDeviceAvailable(DevicePreference[i]) )
                {
                    debugf(TEXT("ufront 2.34: RenderDevice=%s is not in this build -> %s"), Device, DevicePreference[i]);
                    GConfig->SetString(TEXT("Engine.Engine"), TEXT("RenderDevice"), DevicePreference[i]);
                    break;
                }
            }
        }
    }

    // Auto-switch from TcpNetDriver to WebRTCNetDriver on Emscripten (per D-05).
    // TcpNetDriver uses BSD sockets which don't work in browsers.
#ifdef __EMSCRIPTEN__
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
	Render device selection (ufront 2.9).
-----------------------------------------------------------------------------*/

//
// ufront 2.9 -- unified render-device selection (native + web), replacing the old ad-hoc
// -SOFTWARE / -OPENGL / -NULLRENDER block. Each -*RENDERER switch names a render device and (for the
// two GL drivers, which are coupled 1:1 to an SDL viewport manager) its matching ViewportManager.
//
// ONLY an explicit -*RENDERER switch acts here. With no switch, the render device is left to the shipped
// ini + the existing auto-switch guards in InitEngine() (WinDrv->SDL, D3D->OpenGL, OpenGL/GLES3->GL4ES on
// SDL2) -- those guards already normalize any persisted/default choice to the built stack, i.e. they ARE
// this engine's "else persisted-ini / else preference-order" precedence, so we neither duplicate nor
// fight them. This keeps the no-switch boot byte-for-byte what it is today (the E2E-proven default).
//
// Availability is checked with StaticFindObject on the class' short name (ANY_PACKAGE): a render device
// statically linked into this build registered its UClass at startup (AUTO_INITIALIZE_REGISTRANTS_*, run
// in main() before this; every preset is __STATIC_LINK). We deliberately do NOT use
// LoadClass<URenderDevice> (the -testrendev path's approach): the web build disables exception CATCHING,
// so LoadClass on an absent package appThrowf's -> uncatchable Aborted() (the load-bearing 2.9a lesson).
// StaticFindObject only inspects already-registered objects -- no file load, no throw. A switch whose
// driver isn't compiled into this preset (legacy SDL1+OpenGL vs gl4es/web SDL2+GL4ES are build-time
// mutually exclusive; Pixomatic is never built) logs "render device X not available" and no-ops -> the
// game still boots on the default (the canary). This ALSO fixes the old latent -SOFTWARE ->
// PixoDrv.PixoRenderDevice fatal (setting an uninstantiable device then aborting at load).
//
// A switch that stays selected through Engine->Init() (its class wasn't rewritten to a fallback) is
// persisted via PersistRenderChoiceIfSwitched() (GConfig->Flush after Init) so it sticks next boot -- the
// persisted value flows back untouched because the auto-switch guards leave GL4ES/OpenGL/Null alone. The
// UE2 render-device key is "RenderDevice" (vs UE1's Game/Windowed/RenderDevice trio).
//
// ufront 2.34: hoisted above InitEngine — the ini normalisation there asks the same question the
// -*RENDERER switches do ("is this class in this binary?"), so both use this one probe.
struct FRenderSwitch { const TCHAR* Param; const TCHAR* Device; const TCHAR* Viewport; };
static const FRenderSwitch GRenderSwitches[] =
{
	// -RENDERER switch           render device                          coupled SDL viewport (NULL = keep current)
	{ TEXT("GL4ESRENDERER"),    TEXT("GL4ESDrv.GL4ESRenderDevice"),   TEXT("SDL2Drv.SDL2Client") }, // SDL2 (web / gl4es default)
	// ufront 2.34: OpenGLDrv now rides the SAME SDL2 client as GL4ES — the SDL 1.2 pairing was never a
	// property of the driver, only of the stack it happened to ship in (OpenGLDrv/CMakeLists.txt has had
	// the SDL2 branch all along). Both GL devices live in one binary; this switch picks between them.
	{ TEXT("OPENGLRENDERER"),   TEXT("OpenGLDrv.OpenGLRenderDevice"), TEXT("SDL2Drv.SDL2Client") }, // desktop GL on SDL2
	{ TEXT("SOFTWARERENDERER"), TEXT("PixoDrv.PixoRenderDevice"),     NULL                       }, // Pixomatic (never built -> log-noop)
	{ TEXT("NULLRENDERER"),     TEXT("Engine.NullRenderDevice"),      NULL                       }, // GPU-free (always available)
};

// The class a -*RENDERER switch requested (empty if none) -- persisted after a successful Init().
static FString GRequestedRenderDevice;


// Apply the software-renderer quality downgrades (kept verbatim from the old -SOFTWARE path). Only reached
// when PixoDrv is actually selected (i.e. available) -- dead in every current preset (Pixomatic isn't
// built), but wired so a future Pixomatic build gets the low-detail config the software rasterizer needs.
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

// Choose the render device from a -*RENDERER switch, BEFORE Engine->Init() (replaces the old ad-hoc
// -SOFTWARE/-OPENGL/-NULLRENDER block). Sets the "RenderDevice" key and, for the GL drivers, the coupled
// ViewportManager (SDL1<->OpenGL, SDL2<->GL4ES) so we never hand the InitEngine() guards a mismatched
// viewport to "fix" out from under a valid selection. First present switch wins.
static void SelectRenderDevice()
{
	for( INT i=0; i<ARRAY_COUNT(GRenderSwitches); i++ )
	{
		const FRenderSwitch& Sw = GRenderSwitches[i];
		if( ParseParam( appCmdLine(), Sw.Param ) )
		{
			if( IsRenderDeviceAvailable( Sw.Device ) )
			{
				debugf( TEXT("ufront 2.9: -%s selects render device %s"), Sw.Param, Sw.Device );
				GConfig->SetString( TEXT("Engine.Engine"), TEXT("RenderDevice"), Sw.Device );
				if( Sw.Viewport )
					GConfig->SetString( TEXT("Engine.Engine"), TEXT("ViewportManager"), Sw.Viewport );
				if( appStricmp( Sw.Param, TEXT("SOFTWARERENDERER") )==0 )
					ApplySoftwareRendererConfig();
				GRequestedRenderDevice = Sw.Device; // persist iff it survives Init (PersistRenderChoiceIfSwitched)
			}
			else
			{
				debugf( NAME_Warning, TEXT("ufront 2.9: render device %s not available (not compiled into this build) -> keeping the default; -%s ignored"), Sw.Device, Sw.Param );
			}
			return; // first -*RENDERER switch present wins
		}
	}
}

// After Engine->Init(): persist a -*RENDERER switch choice so it sticks next boot. The render device inits
// LAZILY (main loop, at level bring-up), not within Engine->Init(), so we can't inspect the active RenDev
// here; instead confirm the engine didn't rewrite RenderDevice to a fallback during Init -- if it still
// equals our (compiled, selected) request, persist it. Flush(0) writes the ini, which survives reload via
// the user-data IDB layer on the web (and the on-disk ini natively).
static void PersistRenderChoiceIfSwitched()
{
	if( GRequestedRenderDevice.Len()==0 )
		return;
	const TCHAR* Active = GConfig->GetStr( TEXT("Engine.Engine"), TEXT("RenderDevice"), TEXT("System") );
	if( appStricmp( Active, *GRequestedRenderDevice )==0 )
	{
		GConfig->Flush( 0 );
		debugf( TEXT("ufront 2.9: persisted render device %s as the default (sticks next boot)"), *GRequestedRenderDevice );
	}
	else
	{
		debugf( NAME_Warning, TEXT("ufront 2.9: render device changed from %s to %s during init -> not persisted"), *GRequestedRenderDevice, Active );
	}
}

//
// Main entry point.
// This is an example of how to initialize and launch the engine.
//
// ufront 2.34 — the READ_IMPLIES_EXEC re-exec below is a LINUX KERNEL mechanism (personality(2)); MinGW
// has no <sys/personality.h> and Windows has no equivalent knob. Narrow the guard to Linux so the
// Windows build compiles Pixomatic; whether its JIT can execute out of a DEP-protected heap there is a
// separate question, and one this leaves open rather than pre-judging.
#if defined(USE_PIXOMATIC) && defined(__linux__)
// ufront 2.12 Part B: headers for the READ_IMPLIES_EXEC re-exec below (Pixomatic executable-heap fix).
#include <sys/personality.h>
#include <unistd.h>
#include <strings.h>
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
#if defined(USE_PIXOMATIC) && defined(__linux__)
    // ufront 2.12 Part B — Pixomatic executable-heap fix. Pixomatic (Michael Abrash's JIT software rasterizer)
    // generates x86 code at runtime and executes it out of malloc'd heap buffers — including via an internal
    // drawverts path we cannot hook (not PixoAllocCodeBuffer, which our PixoCodeExec.c already RWX-overrides).
    // In 2004 the heap was executable, so this worked; a modern kernel marks the heap NX, so the JIT'd code
    // faults on instruction fetch when -SOFTWARERENDERER is active (SIGSEGV, si_addr == the JIT IP in a rw-p
    // [heap] page). The kernel makes the brk heap executable when the process has the READ_IMPLIES_EXEC
    // personality, but a modern kernel only grants that for a *missing* PT_GNU_STACK marker (binutils always
    // emits one, and an explicit executable-stack marker no longer triggers it). So set the personality
    // explicitly and re-exec ourselves once (personality applies to the whole freshly-exec'd process, which is
    // what makes the later brk heap executable). Scoped to the Pixomatic software renderer ONLY: no effect on
    // the GL4ES/OpenGL binaries, and web/plain/karma builds don't define USE_PIXOMATIC so this compiles out
    // entirely (byte-identical). NB: an executable heap is a security downgrade, accepted only for this
    // experimental software-renderer path.
    {
        UBOOL bWantPixo = 0;
        for( int i=1; i<argc; i++ )
            if( strcasecmp(argv[i], "-SOFTWARERENDERER")==0 ) { bWantPixo = 1; break; }
        int persNow = personality(0xffffffffUL);
        if( bWantPixo && persNow != -1 && !(persNow & READ_IMPLIES_EXEC) )
        {
            if( personality((unsigned long)persNow | READ_IMPLIES_EXEC) != -1 )
                execv("/proc/self/exe", argv);   // re-exec with the new personality; on failure fall through
        }
    }
#endif
    appArgv0(argv[0]);

	TCHAR CmdLine[1024] = TEXT("");
    if (argc >= 2)
    {
	    appStrcpy(CmdLine, appFromAnsi(argv[1]));
	    for (INT i = 2; i < argc; i++)
    	{
       		appStrcat(CmdLine, TEXT(" "));
    		appStrcat(CmdLine, appFromAnsi(argv[i]));
    	}
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
#if UT_RENDER_OPENGLDRV
        AUTO_INITIALIZE_REGISTRANTS_OPENGLDRV;
#endif
#if UT_RENDER_GL4ESDRV
        AUTO_INITIALIZE_REGISTRANTS_GL4ESDRV;
#endif
        #if USE_PIXOMATIC
        AUTO_INITIALIZE_REGISTRANTS_PIXODRV;
        #endif
        AUTO_INITIALIZE_REGISTRANTS_IPDRV;
#ifdef __EMSCRIPTEN__
        AUTO_INITIALIZE_REGISTRANTS_WEBRTCDRV;
#endif
        RegisterNamesXInterface();
        AUTO_INITIALIZE_REGISTRANTS_XINTERFACE;
        AUTO_INITIALIZE_REGISTRANTS_XGAME;
        AUTO_INITIALIZE_REGISTRANTS_UWEB;
        RegisterNamesOnslaught();
        AUTO_INITIALIZE_REGISTRANTS_ONSLAUGHT;
        // autoInitializeRegistrantsEditor(Lookup); // D-10: Editor excluded

        GNativeLookupFuncs[Lookup++] = &FindUWebUWebResponseNative;
        GNativeLookupFuncs[Lookup++] = &FindUWebUWebRequestNative;
        check( Lookup < ARRAY_COUNT(GNativeLookupFuncs) );


//!!! FIXME: linker removes this as deadcode on MacOS X without an explicit reference...
char blah;
#if USE_PIXOMATIC
extern BYTE GLoadedPixoDrv; snprintf(&blah, 1, "%d", (int) GLoadedPixoDrv);
#endif
extern BYTE GLoadedSDL2Drv; snprintf(&blah, 1, "%d", (int) GLoadedSDL2Drv);
#if UT_RENDER_OPENGLDRV
extern BYTE GLoadedOpenGLDrv; snprintf(&blah, 1, "%d", (int) GLoadedOpenGLDrv);
#endif
#if UT_RENDER_GL4ESDRV
extern BYTE GLoadedGL4ESDrv; snprintf(&blah, 1, "%d", (int) GLoadedGL4ESDrv);
#endif
extern BYTE GLoadedALAudio; snprintf(&blah, 1, "%d", (int) GLoadedALAudio);
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

		// Render device command line options (ufront 2.9 -- unified -*RENDERER switch selection; see
		// SelectRenderDevice() above). Replaces the old ad-hoc -SOFTWARE/-OPENGL/-NULLRENDER block:
		//   -GL4ESRENDERER   -> GL4ESDrv.GL4ESRenderDevice  (+ SDL2Drv.SDL2Client; web/gl4es default)
		//   -OPENGLRENDERER  -> OpenGLDrv.OpenGLRenderDevice (+ SDLDrv.SDLClient; legacy preset)
		//   -SOFTWARERENDERER-> PixoDrv.PixoRenderDevice     (Pixomatic absent -> log-noops; fixes the
		//                                                     old latent -SOFTWARE fatal, keeps its downgrades)
		//   -NULLRENDERER    -> Engine.NullRenderDevice      (always available; GPU-free)
		// A switch for a driver not compiled into this preset logs "not available" and no-ops (the game
		// still boots on the default). No switch -> the shipped ini + InitEngine() auto-switch guards decide.
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

		// ufront 2.9: now that Engine->Init() has run, persist a -*RENDERER switch choice iff it stuck
		// (the engine didn't rewrite RenderDevice to a fallback) so the pick sticks next boot.
		if( Engine )
			PersistRenderChoiceIfSwitched();

#ifdef __EMSCRIPTEN__
		GWebRTCEngine = Engine;
#endif

		if( Engine )
		{
			// Hide splash screen.
			ExitSplash();

#ifdef __EMSCRIPTEN__
			// Register browser integration callbacks (BRWSR-02, BRWSR-03).
			// Visibility change: mute audio when tab is hidden (D-16).
			emscripten_set_visibilitychange_callback(
			    nullptr, EM_TRUE, on_visibility_change);
			// Canvas resize: handled by CSS (100vw x 100vh in splash.css) and
			// SDL2's built-in Emscripten resize handling. SDL2 automatically
			// receives WINDOWEVENT_RESIZED events when the canvas size changes,
			// so no explicit resize callback is needed here.
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

