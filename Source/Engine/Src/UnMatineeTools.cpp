/*=============================================================================
	UnMatineeTools.cpp: Tools for the Matinee system
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "EnginePrivate.h"

#define IP_MARKER_SZ	6

/*------------------------------------------------------------------------------
	FMatineeTools.

	A helper class to store the state of the various matinee tools.
------------------------------------------------------------------------------*/

FMatineeTools::FMatineeTools()
{
	CurrentScene = NULL;
	CurrentAction = NULL;
	CurrentSubAction = NULL;
	SceneScrollPos = SceneScrollMax = ActionScrollPos = ActionScrollMax = SubActionScrollPos = SubActionScrollMax = 0;
}

FMatineeTools::~FMatineeTools()
{
}

void FMatineeTools::Init()
{
	guard(FMatineeTools::Init);

	IntPoint = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.S_MatineeIP") ));					check(IntPoint);
	IntPointSel = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.S_MatineeIPSel") ));				check(IntPointSel);
	TimeMarker = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.S_MatineeTimeMarker") ));			check(TimeMarker);
	ActionCamMove = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.S_ActionCamMove") ));			check(ActionCamMove);
	ActionCamPause = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.S_ActionCamPause") ));		check(ActionCamPause);
	PathLinear = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.S_PathLinear") ));				check(PathLinear);
	PathBezier = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.S_PathBezier") ));				check(PathBezier);
	BezierHandle = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.S_BezierHandle") ));			check(BezierHandle);
	SubActionIndicator = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.SubActionIndicator") ));	check(SubActionIndicator);

	unguard;
}

ASceneManager* FMatineeTools::SetCurrent( UEngine* InEngine, ULevel* InLevel, FString InName )
{
	guard(FMatineeTools::SetCurrent);

	CurrentScene = NULL;

	// Find the specified scene
	for( INT x = 0 ; x < InLevel->Actors.Num() ; ++x )
	{
		ASceneManager* SM = Cast<ASceneManager>(InLevel->Actors(x));
		if( SM && InName == *SM->Tag )
		{
			CurrentScene = SM;
			return SetCurrent( InEngine, InLevel, SM );
		}
	}

	return NULL;

	unguard;
}

ASceneManager* FMatineeTools::SetCurrent( UEngine* InEngine, ULevel* InLevel, ASceneManager* InScene )
{
	guard(FMatineeTools::SetCurrent);

	CurrentScene = InScene;

	if( CurrentScene && CurrentScene->Actions.Num() )
		SetCurrentAction( CurrentScene->Actions(0) );
	else
		SetCurrentAction( NULL );

	InEngine->EdCallback( EDC_RefreshEditor, 1, ERefreshEditor_Matinee );

	return CurrentScene;

	unguard;
}

// Locates the specific action in the array and returns it's index (-1 if not found)

INT FMatineeTools::GetActionIdx( ASceneManager* InSM, UMatAction* InMatAction )
{
	guard(FMatineeTools::GetActionIdx);

	if( InSM )
		for( INT x = 0 ; x < InSM->Actions.Num() ; ++x )
			if( InSM->Actions(x) == InMatAction )
				return x;

	return -1;

	unguard;
}

// Locates the specific subaction in the array and returns it's index (-1 if not found)

INT FMatineeTools::GetSubActionIdx( UMatSubAction* InMatSubAction )
{
	guard(FMatineeTools::GetSubActionIdx);

	UMatAction* MA = GetCurrentAction();
	if( MA )
		for( INT x = 0 ; x < MA->SubActions.Num() ; ++x )
			if( MA->SubActions(x) == InMatSubAction )
				return x;

	return -1;

	unguard;
}

INT FMatineeTools::GetPathStyle( UMatAction* InMA )
{
	INT PathStyle;

	if( Cast<UActionPause>(InMA) )
		PathStyle = PATHSTYLE_Linear;
	else
		PathStyle = Cast<UActionMoveCamera>(InMA)->PathStyle;

	return PathStyle;
}

// Takes an interpolation point and fills in the location array for the path
// which leads from the current interpolation point to the next one.

void FMatineeTools::GetSamples( ASceneManager* InSM, UMatAction* InAction, TArray<FVector>* InLocations )
{
	guard(FMatineeTools::GetSamples);

	UMatAction* NextAction = GetNextAction( InSM, InAction );

	if( InAction && NextAction
		&& (InAction != NextAction)
		&& InAction->IntPoint
		&& NextAction->IntPoint )
	{
		FVector Points[4]; // Location 1, Ctrl Point 1, Ctrl Point 2, Location 2
		Points[0] = InAction->IntPoint->Location;
		Points[3] = NextAction->IntPoint->Location;

		// If the path is linear, set the controls points to be 1/3 and 2/3 of the way along the
		// vector between the locations.  This will result in an even distribution of positions.
		if( GetPathStyle( NextAction )==PATHSTYLE_Linear )
		{
			FVector Dir = Points[3] - Points[0];
			FLOAT Length = Dir.Size();
			Dir.Normalize();
			Points[1] = Points[0] + (Dir * (Length/3.f) );
			Points[2] = Points[3] - (Dir * (Length/3.f) );
		}
		else
		{
			Points[1] = InAction->IntPoint->Location + InAction->StartControlPoint;
			Points[2] = NextAction->IntPoint->Location + NextAction->EndControlPoint;
		}

		// ---
		// LOCATIONS
		// ---

		FBezier bz;
		NextAction->PathLength = bz.Evaluate( &Points[0], FMatineeTools::NUM_SAMPLE_POINTS, InLocations );
	}

	unguard;
}

// Find the action that follows the specified one.

UMatAction* FMatineeTools::GetNextAction( ASceneManager* InSM, UMatAction* InAction )
{
	guard(FMatineeTools::GetNextAction);

	if( !InSM ) return NULL;

	UMatAction* Next = InAction;

	INT idx = GetActionIdx( InSM, InAction );
	if( idx < InSM->Actions.Num()-1 )
		Next = InSM->Actions( idx+1 );
	else
		Next = InSM->Actions( 0 );

	return Next;

	unguard;
}

// Does the same thing as "GetNextAction" but only considers actions which move the camera.

UMatAction* FMatineeTools::GetNextMovementAction( ASceneManager* InSM, UMatAction* InAction )
{
	guard(FMatineeTools::GetNextMovementAction);

	if( !InSM ) return NULL;

	UMatAction* Next = GetNextAction( InSM, InAction );
	while( !Cast<UActionMoveCamera>(Next) )
		Next = GetNextAction( InSM, InAction );

	return Next;

	unguard;
}

// Find the action that precedes the specified one.

UMatAction* FMatineeTools::GetPrevAction( ASceneManager* InSM, UMatAction* InAction )
{
	guard(FMatineeTools::GetPrevAction);

	if( !InSM ) return NULL;

	UMatAction* Prev = InAction;

	INT idx = GetActionIdx( InSM, InAction );
	if( idx > 0 )
		Prev = InSM->Actions( idx-1 );
	else
		Prev = InSM->Actions( InSM->Actions.Num()-1 );

	return Prev;

	unguard;
}

// Finds the action that corresponds to the specified interpolation point

UMatAction* FMatineeTools::GetActionFromIP( ASceneManager* InSM, AInterpolationPoint* InIP )
{
	for( INT x = 0 ; x < InSM->Actions.Num() ; x++ )
		if( InSM->Actions(x)->IntPoint == InIP )
			return InSM->Actions(x);

	return NULL;
}

FString FMatineeTools::GetOrientationDesc( INT InOrientation )
{
	FString Desc;

	if( InOrientation == CAMORIENT_LookAtActor )		Desc = TEXT("Look at Actor");
	else if( InOrientation == CAMORIENT_FacePath )		Desc = TEXT("Face Path");
	else if( InOrientation == CAMORIENT_Interpolate )	Desc = TEXT("Interpolation");
	else if( InOrientation == CAMORIENT_Dolly )			Desc = TEXT("Dolly");
	else												Desc = TEXT("None");

	return Desc;
}

/*------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------*/

