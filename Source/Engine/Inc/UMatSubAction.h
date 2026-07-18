/*=============================================================================
	UMatSubAction.h.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
	
virtual void PostEditChange();

virtual UBOOL Update( FLOAT InScenePct, class ASceneManager* InSM );
virtual FString GetStatString();
FString GetStatusDesc();
virtual UBOOL IsRunning();

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

