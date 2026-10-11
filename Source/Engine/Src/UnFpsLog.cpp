/*=============================================================================
	UnFpsLog.cpp: the frame rate in the log, one line per interval. ufront port; not
	Epic's. Ported from engine-uc2 (Source/Engine/Src/UC2FpsLog.cpp, its F-2); the same
	setting, command and line in every engine of the frontend. Quiet by default.

	Why: STAT FPS draws the frame rate on the screen only; a benchmark run by a tool, a
	headless run and a browser run need it in the log, and a mean alone hides the slow
	frames players notice.

	Setting (ini; set from outside with -ini:System:[Engine.GameEngine]:FpsLogSeconds=5):
	  [Engine.GameEngine] FpsLogSeconds=<seconds>   0 (default) = off
	Console command (also the web's ue_exec_command):
	  FPSLOG <seconds>   start a new interval now and log every <seconds> (0 stops it)
	  FPSLOG             say what it is set to

	A line, logged under the name FPS at the end of each interval:
	  FPS: <secs> s <frames> frames <fps> fps; frame ms avg <a> min <b> max <c> p99 <d>; 1% low <e> fps; <W>x<H>
	A frame is the wall time between two calls of UGameEngine::Tick: the whole main-loop
	iteration (tick, draw, present, the launcher's limiter and, on the web, the yield).
	1% low = the frame rate of the average of the slowest 1% of the frames (at least one).
=============================================================================*/

#include "EnginePrivate.h"

static FName FpsLogName()
{
	// RF_Native keeps a run-time name out of the purge of unreferenced names.
	static FName Name;
	if( Name==NAME_None )
	{
		Name = FName( TEXT("FPS") );
		Name.SetFlags( RF_Native );
	}
	return Name;
}

static FLOAT			GFpsLogSeconds = -1.f;	// -1: the ini not read yet
static DOUBLE			GFpsLogStart   = 0.0;	// start of the interval (0: the next frame starts one)
static DOUBLE			GFpsLogLast    = 0.0;	// the previous Tick
static TArray<FLOAT>	GFpsLogFrames;			// frame lengths of this interval, seconds

static QSORT_RETURN CDECL CompareFrameDesc( const void* A, const void* B )
{
	const FLOAT X = *(const FLOAT*)A, Y = *(const FLOAT*)B;
	return X > Y ? -1 : X < Y ? 1 : 0;
}

static void FpsLogRestart()
{
	GFpsLogStart = 0.0;
	GFpsLogLast  = 0.0;
	GFpsLogFrames.Empty();
}

static void FpsLogReport( UEngine* Engine, DOUBLE Seconds )
{
	const INT N = GFpsLogFrames.Num();
	if( N <= 0 || Seconds <= 0.0 )
		return;
	TArray<FLOAT> Sorted( N );
	DOUBLE Sum = 0.0;
	for( INT i=0; i<N; i++ )
	{
		Sorted(i) = GFpsLogFrames(i);
		Sum += GFpsLogFrames(i);
	}
	appQsort( &Sorted(0), N, sizeof(FLOAT), CompareFrameDesc );
	const INT Slow = Max( 1, N / 100 );
	DOUBLE SlowSum = 0.0;
	for( INT i=0; i<Slow; i++ )
		SlowSum += Sorted(i);
	const INT P99 = Min( N-1, (INT)(N * 0.01f) );	// descending: index 1% from the slow end
	INT SizeX = 0, SizeY = 0;
	if( Engine->Client && Engine->Client->Viewports.Num() && Engine->Client->Viewports(0) )
	{
		SizeX = Engine->Client->Viewports(0)->SizeX;
		SizeY = Engine->Client->Viewports(0)->SizeY;
	}
	debugf( (EName)FpsLogName().GetIndex(),
		TEXT("%.2f s %i frames %.1f fps; frame ms avg %.2f min %.2f max %.2f p99 %.2f; 1%% low %.1f fps; %ix%i"),
		Seconds, N, N / Seconds,
		1000.0 * Sum / N, 1000.f * Sorted(N-1), 1000.f * Sorted(0), 1000.f * Sorted(P99),
		SlowSum > 0.0 ? Slow / SlowSum : 0.0,
		SizeX, SizeY );
}

// Called once per UGameEngine::Tick.
ENGINE_API void UfrontFpsLogTick( UEngine* Engine )
{
	if( GFpsLogSeconds < 0.f )
	{
		GFpsLogSeconds = 0.f;
		GConfig->GetFloat( TEXT("Engine.GameEngine"), TEXT("FpsLogSeconds"), GFpsLogSeconds );
		GFpsLogSeconds = Max( 0.f, GFpsLogSeconds );
		if( GFpsLogSeconds > 0.f )
			debugf( NAME_Init, TEXT("FpsLogSeconds=%g: the frame rate is logged every %g s"), GFpsLogSeconds, GFpsLogSeconds );
	}
	if( GFpsLogSeconds <= 0.f )
		return;
	const DOUBLE Now = appSeconds();
	if( GFpsLogStart == 0.0 )
	{
		GFpsLogStart = GFpsLogLast = Now;
		return;
	}
	GFpsLogFrames.AddItem( (FLOAT)(Now - GFpsLogLast) );
	GFpsLogLast = Now;
	if( Now - GFpsLogStart >= GFpsLogSeconds )
	{
		FpsLogReport( Engine, Now - GFpsLogStart );
		GFpsLogStart = Now;
		GFpsLogFrames.Empty( GFpsLogFrames.Num() );
	}
}

// FPSLOG [seconds]
ENGINE_API UBOOL UfrontFpsLogExec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	if( !ParseCommand( &Cmd, TEXT("FPSLOG") ) )
		return 0;
	if( *Cmd )
	{
		GFpsLogSeconds = Max( 0.f, appAtof( Cmd ) );
		FpsLogRestart();
	}
	Ar.Logf( TEXT("FPSLOG: %g s (0 = off)"), Max( 0.f, GFpsLogSeconds ) );
	return 1;
}
