/*=============================================================================
	UnCheats.cpp: the position cheats WHEREAMI and TELEPORT. ufront port; not
	Epic's. Ported from engine-uc2 (Source/Engine/Src/UC2Cheats.cpp, the same engine
	family); the names, the format and the guard are the cross-engine spec of
	engine-uc2's docs/CHEAT-COMMANDS.md, shared by every engine of the frontend.

	Console commands (the console, a key binding, the web's ue_exec_command):
	  WHEREAMI                         the local player's position and view:
	                                   "<x> <y> <z> <yaw> <pitch>" (x y z: world units, one
	                                   decimal, the pawn's location; yaw 0..65535, pitch
	                                   -32768..32767: Unreal rotation units, the controller's
	                                   view rotation), on the console, on the screen
	                                   (ClientMessage) and in the log as
	                                   "Cheat: WHEREAMI <x> <y> <z> <yaw> <pitch>"
	  TELEPORT <x> <y> <z> [<yaw> [<pitch>]]
	                                   puts the local player's pawn there (WHEREAMI's output
	                                   pasted back gives the same place and view); without yaw
	                                   and pitch the view is kept. TELEPORT with NO arguments is
	                                   Epic's CheatManager.Teleport (to the surface looked at),
	                                   unchanged.

	The guard: allowed for the local player of a standalone game (UT2004's own rule: the
	CheatManager, which PlayerController.AddCheats creates only for NM_Standalone) and for
	the local player of a listen server (the host is the server); every network client and
	a dedicated server refuse: "Cheat: <COMMAND> refused: ..." in the log and on the
	console. Test harnesses set [Engine.GameEngine] AllowNetCheats=True in that process's
	ini (-ini:System:...); it must never be in a shipped ini. Every command that runs logs
	one "Cheat: <COMMAND> <arguments>" line.

	Called first by UPlayer::Exec (the console runs script exec functions before the
	engine's commands, and CheatManager.Teleport would take TELEPORT; with arguments it is
	ours) and by UGameEngine::Exec (the web's ue_exec_command reaches the engine only).
=============================================================================*/

#include "EnginePrivate.h"

// The log carries no log-name prefix in this engine, so "Cheat: " is part of the text, on NAME_Log (never
// suppressed). The console (Ar) gets the short form, unless it is the log itself (the test channel's "exec").
static void CheatLog( FOutputDevice& Ar, const TCHAR* Text )
{
	debugf( NAME_Log, TEXT("Cheat: %s"), Text );
	if( &Ar != GLog )
		Ar.Log( Text );
}

static APlayerController* FirstLocalPlayer( UEngine* Engine )
{
	if( Engine && Engine->Client && Engine->Client->Viewports.Num() )
		return Engine->Client->Viewports(0)->Actor;
	return NULL;
}

//
// The rule without logging anything. PC: the local player the command came from (NULL: the
// first viewport's, if any). Out: the net mode, and whether only the test key allowed it.
//
static UBOOL CheatsWouldAllow( UEngine* Engine, APlayerController* PC, INT* OutNetMode, UBOOL* OutByKey )
{
	guard(CheatsWouldAllow);
	if( !PC )
		PC = FirstLocalPlayer( Engine );
	INT NetMode = NM_DedicatedServer;
	if( PC && PC->Level )
		NetMode = PC->Level->NetMode;
	else if( Engine && Engine->IsA(UGameEngine::StaticClass()) && ((UGameEngine*)Engine)->GLevel && ((UGameEngine*)Engine)->GLevel->GetLevelInfo() )
		NetMode = ((UGameEngine*)Engine)->GLevel->GetLevelInfo()->NetMode;
	if( OutNetMode )
		*OutNetMode = NetMode;
	if( OutByKey )
		*OutByKey = 0;
	if( PC && ( PC->CheatManager || NetMode==NM_ListenServer ) )
		return 1;
	UBOOL NetCheats = 0;
	GConfig->GetBool( TEXT("Engine.GameEngine"), TEXT("AllowNetCheats"), NetCheats );
	if( NetCheats && OutByKey )
		*OutByKey = 1;
	return NetCheats;
	unguard;
}

//
// May cheats run in this process now? Logs the refusal, or the "Cheat:" line of the command that runs.
//
static UBOOL CheatsAllowed( UEngine* Engine, APlayerController* PC, const TCHAR* Command, const TCHAR* Args, FOutputDevice& Ar )
{
	guard(CheatsAllowed);
	// The local player of a standalone game (UT2004's own rule: the CheatManager, PlayerController.AddCheats) and the
	// local player of a LISTEN server (the host is the server's authority). Every network client and a dedicated
	// server: refused, unless the test key [Engine.GameEngine] AllowNetCheats opens them for a test harness.
	UBOOL ByKey = 0;
	UBOOL Allowed = CheatsWouldAllow( Engine, PC, NULL, &ByKey );
	const TCHAR* Why = ByKey ? TEXT(" (AllowNetCheats: a test setting)") : TEXT("");
	if( !Allowed )
	{
		CheatLog( Ar, *FString::Printf( TEXT("%s refused: cheats are only for a standalone game or the host of a listen server"), Command ) );
		return 0;
	}
	while( Args && *Args==' ' )
		Args++;
	debugf( NAME_Log, TEXT("Cheat: %s %s%s"), Command, Args ? Args : TEXT(""), Why );
	return 1;
	unguard;
}

//
// WHEREAMI and TELEPORT <x> <y> <z> [<yaw> [<pitch>]]. Called first by UPlayer::Exec (so that
// TELEPORT with coordinates is not taken by CheatManager.Teleport, which has no parameters)
// and by UGameEngine::Exec (the web's ue_exec_command, which reaches the engine only).
//
ENGINE_API UBOOL UfrontCheatExec( UEngine* Engine, APlayerController* PC, const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UfrontCheatExec);
	const TCHAR* Str = Cmd;
	if( ParseCommand( &Str, TEXT("WHEREAMI") ) )
	{
		if( !PC )
			PC = FirstLocalPlayer( Engine );
		if( !PC || !PC->Pawn )
		{
			if( CheatsAllowed( Engine, PC, TEXT("WHEREAMI"), TEXT(""), Ar ) )
				CheatLog( Ar, TEXT("WHEREAMI failed: no local player with a pawn") );
			return 1;
		}
		FVector L = PC->Pawn->Location;
		INT Yaw = PC->Rotation.Yaw & 65535;
		INT Pitch = PC->Rotation.Pitch & 65535;
		if( Pitch >= 32768 )
			Pitch -= 65536;
		FString Pos = FString::Printf( TEXT("%.1f %.1f %.1f %i %i"), L.X, L.Y, L.Z, Yaw, Pitch );
		if( !CheatsAllowed( Engine, PC, TEXT("WHEREAMI"), *Pos, Ar ) )
			return 1;
		if( &Ar != GLog )
			Ar.Log( *Pos );
		PC->eventClientMessage( Pos, NAME_None );
		return 1;
	}
	if( ParseCommand( &Str, TEXT("TELEPORT") ) )
	{
		// TELEPORT alone: Epic's CheatManager.Teleport (UPlayer::Exec goes on to the script)
		FString Tok[5];
		INT N = 0;
		const TCHAR* Args = Str;
		while( N<5 && ParseToken( Str, Tok[N], 0 ) )
			N++;
		if( N==0 )
			return 0;
		if( !PC )
			PC = FirstLocalPlayer( Engine );
		if( !CheatsAllowed( Engine, PC, TEXT("TELEPORT"), Args, Ar ) )
			return 1;
		if( N<3 )
		{
			CheatLog( Ar, TEXT("TELEPORT failed: usage TELEPORT <x> <y> <z> [<yaw> [<pitch>]]") );
			return 1;
		}
		if( !PC || !PC->Pawn || !PC->GetLevel() || PC->Level->NetMode==NM_Client )
		{
			CheatLog( Ar, TEXT("TELEPORT failed: no local pawn this process moves (a network client cannot)") );
			return 1;
		}
		FVector To( appAtof(*Tok[0]), appAtof(*Tok[1]), appAtof(*Tok[2]) );
		APawn* P = PC->Pawn;
		UBOOL Moved = PC->GetLevel()->FarMoveActor( P, To, 0, 0 );
		P->Velocity = FVector(0,0,0);
		P->Acceleration = FVector(0,0,0);
		if( N>=4 )
		{
			INT Yaw = appAtoi( *Tok[3] );
			INT Pitch = N>=5 ? appAtoi( *Tok[4] ) : 0;
			P->Rotation = FRotator( 0, Yaw, 0 );
			PC->Rotation = FRotator( Pitch, Yaw, 0 );
		}
		FString Pos = FString::Printf( TEXT("%.1f %.1f %.1f %i %i"), P->Location.X, P->Location.Y, P->Location.Z,
			PC->Rotation.Yaw & 65535, (PC->Rotation.Pitch & 65535) >= 32768 ? (PC->Rotation.Pitch & 65535) - 65536 : (PC->Rotation.Pitch & 65535) );
		CheatLog( Ar, *FString::Printf( TEXT("TELEPORT %s: %s"), Moved ? TEXT("ok") : TEXT("failed (no room there)"), *Pos ) );
		return 1;
	}
	return 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
