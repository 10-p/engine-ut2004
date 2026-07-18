/*=============================================================================
	UnActor.cpp: Actor list functions.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	UPlayer object implementation.
-----------------------------------------------------------------------------*/

UPlayer::UPlayer()
{}
void UPlayer::Serialize( FArchive& Ar )
{
	guard(UPlayer::Serialize);
	Super::Serialize( Ar );
	unguard;
}
void UPlayer::Destroy()
{
	guard(UPlayer::Destroy);
	if( GIsRunning && Actor )
	{
		ULevel* Level = Actor->GetLevel();
		checkSlow(Level);
		Actor->Player = NULL;
		if( Level && Actor->RendMap != REN_Prefab && Actor->RendMap != REN_PrefabCompiled )		// !!hack - I don't know why this crashes when it's a REN_Prefab
			Level->DestroyActor( Actor, 1 );
		Actor = NULL;
	}
	Super::Destroy();
	unguard;
}

UBOOL UPlayer::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UPlayer::Exec);
	if ( !Actor )
		return 0;
	if( Actor->GetLevel()->Exec(Cmd,Ar) )
	{
		return 1;
	}
	if( Actor->Level && Actor->Level->Game && Actor->Level->Game->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
	{
		return 1;
	}
	if( Actor->myHUD && Actor->myHUD->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
	{
		return 1;
	}
	if( Actor->CheatManager && Actor->CheatManager->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
	{
		return 1;
	}
	if( Actor->AdminManager && Actor->AdminManager->ScriptConsoleExec(Cmd, Ar, Actor->Pawn) )
	{
		return 1;
	}
	if( Actor->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
	{
		return 1;
	}
	if( Actor->PlayerInput && Actor->PlayerInput->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
	{
		return 1;
	}
	if( Actor->Pawn )
	{
		if( Actor->Pawn->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
			return 1;
		if( Actor->Pawn->Weapon && Actor->Pawn->Weapon->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
			return 1;
		if( Actor->Pawn->SelectedItem && Actor->Pawn->SelectedItem->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
				return 1;
	}
	if( Actor->GetLevel()->Engine->Exec(Cmd,Ar) )
	{
		return 1;
	}

	for ( INT i = 0; i < LocalInteractions.Num(); i++ )
		if ( LocalInteractions(i) && LocalInteractions(i)->ScriptConsoleExec(Cmd,Ar,Actor->Pawn) )
			return 1;
	
	return 0;
	unguard;
}
IMPLEMENT_CLASS(UPlayer);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

