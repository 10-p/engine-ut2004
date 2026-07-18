/*=============================================================================
	UnMatineeTools.h: Tools for the Matinee system
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "UnForcePacking_begin.h"

/*-----------------------------------------------------------------------------
	In-Editor Matinee-driven animation preview resources.
-----------------------------------------------------------------------------*/
/*
Action_PLAYANIM
			->  BaseAnim     <-
             -> BlendInTime
             -> BlendOUtTime
             -> AnimRate     <-
             -> AnimIterations
             -> bLoopAnim    <-
*/

// Actions.
struct FPreviewAction
{
	FLOAT   StartTime;
	FLOAT	EndTime;
	FName   BaseAnimSeq;
	FLOAT   AnimRate;
	FLOAT   StartFrame;
	UBOOL   bLoopAnim;

	friend FArchive& operator<<(FArchive& Ar, FPreviewAction& P)
	{
		return Ar
			<< P.StartTime
			<< P.EndTime
			<< P.BaseAnimSeq
			<< P.AnimRate
			<< P.StartFrame
			<< P.bLoopAnim;
	}
};

// Actors and their list of timed actions for a scene.
struct FPreviewCastActor
{
	AActor*   SourceActor;
	AActor*   TempActor;
	UMesh*    CurrentMesh;
	INT       SourceActorIndex;
	INT       TempActorIndex;
	UBOOL	  bOnDisplay;
	UBOOL     bIsHidden;
	TArray<FPreviewAction> Actions;

	friend FArchive& operator<<(FArchive& Ar, FPreviewCastActor& P)
	{
		return Ar
			<< P.SourceActor
			<< P.TempActor
			<< P.CurrentMesh
			<< P.SourceActorIndex
			<< P.TempActorIndex
			<< P.bOnDisplay
			<< P.bIsHidden
			<< P.Actions;
	}
};
//
struct FPreviewCinActorList
{
	INT   PreviewWindowNum;
	INT   Initialized;
	void* CurrentScenePtr;
	TArray<FPreviewCastActor> CastList;

	friend FArchive& operator<<(FArchive& Ar, FPreviewCinActorList& P)
	{
		return Ar
			<< P.PreviewWindowNum
			<< P.Initialized
			//<< P.CurrentScene
			<< P.CastList;
	}
};


class ENGINE_API FMatineeTools
{
public:
	enum { NUM_SAMPLE_POINTS=100 };		// The number of location/rotation samples we use
	enum { SHOW_ROT_INTERVAL=10 };		// Rotation will be shown on the path every SHOW_ROT_INTERVAL sample locations
	enum { MAT_SCENE_HEIGHT=38 };
	enum { MAT_ACTION_HEIGHT=44 };
	enum { MAT_SUBACTION_HEIGHT=48 };

	// Constructor.
	FMatineeTools();
	virtual ~FMatineeTools();

	void Init();

	UTexture *IntPoint, *IntPointSel, *TimeMarker, *ActionCamMove, *ActionCamPause,
		*PathLinear, *PathBezier, *BezierHandle, *SubActionIndicator;

	FPreviewCinActorList PCAList; // Cinematic-preview-actor list.

	void GetSamples( ASceneManager* InSM, UMatAction* InAction, TArray<FVector>* InLocations );
	UMatAction* GetNextAction( ASceneManager* InSM, UMatAction* InAction );
	UMatAction* GetNextMovementAction( ASceneManager* InSM, UMatAction* InAction );
	UMatAction* GetPrevAction( ASceneManager* InSM, UMatAction* InAction );
	UMatAction* GetActionFromIP( ASceneManager* InSM, AInterpolationPoint* InIP );

	ASceneManager* SetCurrent( UEngine* InEngine, ULevel* InLevel, FString InName );
	ASceneManager* SetCurrent( UEngine* InEngine, ULevel* InLevel, ASceneManager* InScene );
	INT GetPathStyle( UMatAction* InMA );

	ASceneManager* CurrentScene;		// The current scene
	inline ASceneManager* GetCurrent() { return CurrentScene; }

	INT SceneScrollPos, SceneScrollMax;
	INT ActionScrollPos, ActionScrollMax;
	INT SubActionScrollPos, SubActionScrollMax;

	UMatAction* CurrentAction;			// The current action
	UMatAction* SetCurrentAction( UMatAction* InAction )
	{
		CurrentAction = InAction;

		if( CurrentAction && CurrentAction->SubActions.Num() )
			SetCurrentSubAction( CurrentAction->SubActions(0) );
		else
			SetCurrentSubAction( NULL );

		return CurrentAction;
	}
	inline UMatAction* GetCurrentAction() { return CurrentAction; }
	INT GetActionIdx( ASceneManager* InSM, UMatAction* InMatAction );

	UMatSubAction* CurrentSubAction;	// The current subaction
	UMatSubAction* SetCurrentSubAction( UMatSubAction* InSubAction )
	{
		CurrentSubAction = InSubAction;
		return CurrentSubAction;
	}
	inline UMatSubAction* GetCurrentSubAction() { return CurrentSubAction; }
	INT GetSubActionIdx( UMatSubAction* InSubMatAction );

	FString GetOrientationDesc( INT InOrientation );

	// Stats
	INT ScenesExecuting;
};

#include "UnForcePacking_end.h"

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

