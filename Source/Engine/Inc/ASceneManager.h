/*=============================================================================
	ASceneManager.h.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
	
virtual void PostBeginPlay();
virtual UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );
virtual void PostEditChange();
virtual void CheckForErrors();

FLOAT GetTotalSceneTime();
FVector GetLocation( TArray<FVector>* InSampleLocations, FLOAT InScenePct );
FRotator GetRotation( TArray<FVector>* InSampleLocations, FLOAT InScenePct, FVector InViewerLocation, FRotator InViewerRotation, UMatAction* InMA, UBOOL bUseBlending = 1 );
void ChangeOrientation( FOrientation InOrientation );
void PreparePath();
void DeletePathSamples();
UMatAction* GetActionFromPct( FLOAT InPctSceneComplete );
FLOAT GetActionPctFromScenePct( FLOAT InScenePct );
void SceneStarted();
void SceneEnded();
void UpdateViewerFromPct( FLOAT InScenePct );
void SetSceneStartTime();
void SetCurrentTime( float InTime );
void RefreshSubActions( float InScenePct );
void InitializePreviewActors();
void CleanupPreviewActors();
void UpdatePreviewActors( FLOAT InScenePct );
void StraightenBezierHandles();

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

