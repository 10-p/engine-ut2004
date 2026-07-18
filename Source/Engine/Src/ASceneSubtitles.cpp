/*=============================================================================
	ASceneSubtitles.cpp: Unreal Tournament 2004 subtitles
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Laurent Delayen
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	Subtitles Implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(ASceneSubtitles);

void ASceneSubtitles::execProcessEvent( FFrame& Stack, RESULT_DECL )
{
	guard(ASceneSubtitles::execProcessEvent);
	P_GET_BYTE( Mode )
	P_FINISH;

	ProcessEvent( ESST_Mode(Mode) );

	unguardexec;
}

void ASceneSubtitles::ProcessEvent( ESST_Mode Mode )
{
	guard(ASceneSubtitles::ProcessEvent);

	if ( Mode == ESST_SkipToNextLine )
	{
		CurrentIndex++;
		//debugf(TEXT("ASceneSubtitles::ProcessEvent CurrentIndex:%i"), CurrentIndex);
	}

	unguard;
}
