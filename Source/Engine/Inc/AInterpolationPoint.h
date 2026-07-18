/*=============================================================================
	AInterpolationPoint.h.
	Copyright 2001-2002 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

FCoords GetInterpolatedPosition(FCoords OldCoords, FVector StartControlPoint, FLOAT PhysAlpha, INT PauseNum, FVector OldLocation);
FCoords GetDesiredRotationAtPosition(INT PosOffset, INT PauseNum, FVector NewLocation, FVector OldLocation);
FCoords GetDesiredRotationAtPause(INT PauseNum, FVector NewLocation, FVector OldLocation);
FCoords GetViewCoords(INT PauseNum,FVector NewLocation);
AActor* FindViewTarget(INT PauseNum);
virtual void PostEditChange();
virtual void PostEditMove();
void RenderEditorSelected(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA);

