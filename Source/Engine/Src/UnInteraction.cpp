/*=============================================================================
	UnInteraction.cpp: See .UC for for info
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Joe Wilcox
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UInteraction);
IMPLEMENT_CLASS(UConsole);
IMPLEMENT_CLASS(UBaseGUIController);

//
// Initialize the state system
//
void UInteraction::execInitialize( FFrame& Stack, RESULT_DECL )
{
	guard(UInteraction::_Init);

	P_FINISH;

	// Init scripting.
	InitExecution();

	// Signal the Initialized event in script

	eventInitialized();

	unguardexec;
}

void UInteraction::execConsoleCommand( FFrame& Stack, RESULT_DECL )
{
	guardSlow(UInteraction::execConsoleCommand);

	P_GET_STR(S);
	P_FINISH;

	if (!Master)
	{
		GWarn->Logf(TEXT("Attempt to execute a console command through an Interaction that is without a master"));
		return;
	}

	if ( S.Len() > 1000 )
		S = S.Left(1000);

	if (ViewportOwner)
		*(DWORD*)Result = Master->Exec( *S, *ViewportOwner);
	else
		*(DWORD*)Result = Master->Exec( *S, *Master->Client->Viewports(0));

	unguardexecSlow;
}

// Converts a world location in to Screen X/Y coordinates

void UInteraction::execWorldToScreen( FFrame& Stack, RESULT_DECL )
{
	guard(UInteraction::execWorldToScreen);

	P_GET_VECTOR(Location);							// Location holds the vector to project

	UViewport* View = (UViewport*) ViewportOwner;
	
	if (!View)
		View = Master->Client->Viewports(0);

	APlayerController*	CameraActor = View->Actor;
	

	P_GET_VECTOR_OPTX(CameraLocation, CameraActor->Location);	// CameraLocation and CameraRotation can be used
	P_GET_ROTATOR_OPTX(CameraRotation, CameraActor->Rotation);	// to project to any viewpoint
	P_FINISH;

	AActor*		ViewActor = NULL;

	CameraActor->eventPlayerCalcView(ViewActor,CameraLocation,CameraRotation);

	FCameraSceneNode	SceneNode(View,&View->RenderTarget,ViewActor, CameraLocation, CameraRotation,CameraActor->FovAngle);
	FCanvasUtil			CanvasUtil(&View->RenderTarget,View->RI);

	*(FVector*)Result = CanvasUtil.ScreenToCanvas.TransformFVector(SceneNode.Project(Location));


	unguard;
}

// Translate X/Y screen coordinates in to a world vector

void UInteraction::execScreenToWorld( FFrame& Stack, RESULT_DECL )
{
	guard(UInteraction::execWorldToScreen);

	P_GET_VECTOR(Location);	// Location holds the X/Y coords to Deporject

	UViewport*			View = (UViewport*) ViewportOwner;

	if (!View)
		View = Master->Client->Viewports(0);


	APlayerController*	CameraActor = View->Actor;


	P_GET_VECTOR_OPTX(CameraLocation, CameraActor->Location);	// CameraLocation and CameraRotation can be used
	P_GET_ROTATOR_OPTX(CameraRotation, CameraActor->Rotation);	// to deproject from any viewpoint

	P_FINISH;


	AActor*				ViewActor = NULL;

	CameraActor->eventPlayerCalcView(ViewActor,CameraLocation,CameraRotation);

	FCameraSceneNode	SceneNode(View,&View->RenderTarget,ViewActor, CameraLocation, CameraRotation, CameraActor->FovAngle);
	FCanvasUtil			CanvasUtil(&View->RenderTarget,View->RI);

	FPlane	P(CanvasUtil.CanvasToScreen.TransformFVector(Location),NEAR_CLIPPING_PLANE);
	FVector Direction = SceneNode.Deproject(P);
	Direction = Direction - CameraLocation;
	Direction.SafeNormal();
	*(FVector*)Result = Direction;

	unguard;
}

void  UInteraction::NativeMessage(const FString Msg, FLOAT MsgLife)
{
}

UBOOL UInteraction::NativeKeyType(BYTE& iKey, TCHAR Unicode )
{
	return false;
}

UBOOL UInteraction::NativeKeyEvent(BYTE& iKey, BYTE& State, FLOAT Delta )
{
	return false;
}

void  UInteraction::NativeTick(FLOAT DeltaTime)
{
}

void  UInteraction::NativePreRender(UCanvas* Canvas)
{
}

void  UInteraction::NativePostRender(UCanvas* Canvas)
{
}

