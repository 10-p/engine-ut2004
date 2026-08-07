/*=============================================================================
	UnDemoRec.h: Unreal demo recording.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jack Porter.
=============================================================================*/

#include "UnForcePacking_begin.h"

class UDemoRecDriver;
class UDemoRecConnection;

/*-----------------------------------------------------------------------------
	UDemoRecConnection.
-----------------------------------------------------------------------------*/

//
// Simulated network connection for recording and playing back game sessions.
//
class ENGINE_API UDemoRecConnection : public UNetConnection
{
	DECLARE_CLASS(UDemoRecConnection,UNetConnection,CLASS_Config|CLASS_Transient,Engine)
	NO_DEFAULT_CONSTRUCTOR(UDemoRecConnection)

	// Constructors.
	void StaticConstructor();
	UDemoRecConnection( UNetDriver* InDriver, const FURL& InURL );

	// UNetConnection interface.
	FString LowLevelGetRemoteAddress();
	FString LowLevelDescribe();
	void LowLevelSend( void* Data, INT Count );
	INT IsNetReady( UBOOL Saturate );
	void FlushNet();
	void HandleClientPlayer( APlayerController* PC );

	// UDemoRecConnection functions.
	UDemoRecDriver* GetDriver();
};

/*-----------------------------------------------------------------------------
	UDemoRecDriver.
-----------------------------------------------------------------------------*/

//
// Simulated network driver for recording and playing back game sessions.
//
class ENGINE_API UDemoRecDriver : public UNetDriver
{
	DECLARE_CLASS(UDemoRecDriver,UNetDriver,CLASS_Transient|CLASS_Config,Engine)

	// Variables.
	FString			DemoFilename;
	FStringNoInit	DemoSpectatorClass;
	FArchive*		FileAr;
	UBOOL			NoFrameCap;
	UBOOL			DemoEnded;
	UBOOL			Loop;
	UBOOL			bNoRender;
	DOUBLE			PlaybackStartTime;
	DOUBLE			LastFrameTime;
	INT				FrameNum;
	INT				InitialFrameStart;
	FLOAT			LastDeltaTime;
	FURL            LoopURL;
	DOUBLE			LastClientRecordTime; // for capping client-side demo recording FPS.
	FLOAT			DemoRecMultiFrameDeltaTime;

	// --- timedemo FRAME-TIME DISTRIBUTION (ufront step 2.30) ---------------------------------------
	// The timedemo has always reported a MEAN, and a mean cannot see the thing players complain about:
	// "it drops to ~15 fps at times" is a statement about the TAIL, and a run that is smooth at 60 and
	// a run that alternates 120/20 can report the same average. These accumulate a fixed-bucket
	// histogram of per-frame wall time plus the worst frame, which costs one appSeconds() and one
	// increment per frame and needs no allocation. Only maintained when NoFrameCap (i.e. `?timedemo`),
	// so ordinary demo playback is byte-for-byte unaffected.
	enum { DEMO_FT_BUCKETS = 8 };
	DOUBLE			LastTimedFrameStart;
	INT				FrameTimeBuckets[DEMO_FT_BUCKETS];
	INT				FrameTimeSamples;
	DOUBLE			WorstFrameMs;

	// Constructors.
	void StaticConstructor();
	UDemoRecDriver();

	// UNetDriver interface.
	void LowLevelDestroy();
	FString LowLevelGetNetworkNumber();
	UBOOL InitConnect( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error );
	UBOOL InitListen( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error );
	void TickDispatch( FLOAT DeltaTime );
    virtual UBOOL IsDemoDriver() { return 1; } // sjs

	// FExec interface.
	INT Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );

	// UDemoRecDriver functions.
	UBOOL InitBase( UBOOL Connect, FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error );
	ULevel* GetLevel();
	void SpawnDemoRecSpectator( UNetConnection* Connection );
	UBOOL UpdateDemoTime( FLOAT* DeltaTime, FLOAT TimeDilation );
};

#include "UnForcePacking_end.h"

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

