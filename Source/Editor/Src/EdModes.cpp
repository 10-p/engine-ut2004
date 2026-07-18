/*=============================================================================
	EdModes.cpp: Classes which represent "modes" in the editor
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "EditorPrivate.h"

/*------------------------------------------------------------------------------
	UEdMode.
------------------------------------------------------------------------------*/

UEdMode::UEdMode()
{
}

UEdMode::~UEdMode()
{
}

// A mouse button was pushed down.
void UEdMode::MouseButtonDown( UViewport* InViewport, DWORD InButtons, INT InX, INT InY )
{
	guard(UEdMode::MouseButtonDown);

	check(0);	// Sanity check, should never be in here
	/*
	InButtons &= ~InViewport->Buttons;
	InViewport->Buttons |= InButtons;

	//if( InButtons&MOUSE_Left ) { debugf(TEXT("DOWN : MOUSE_Left")); }
	//if( InButtons&MOUSE_Middle ) { debugf(TEXT("DOWN : MOUSE_Middle")); }
	//if( InButtons&MOUSE_Right ) { debugf(TEXT("DOWN : MOUSE_Right")); }

	GEdModeTools->ClickStartScreen = GEdModeTools->ClickEndScreen = FVector( InX, InY, 0 );

	//FLevelSceneNode SN( InViewport );
	//GEdModeTools->ClickStartWorld = SN.Deproject( FPlane( InX, InY, 0, 1) );
	*/

	unguard;
}

// A mouse button was let up.
void UEdMode::MouseButtonUp( UViewport* InViewport, DWORD InButtons, INT InX, INT InY )
{
	guard(UEdMode::MouseButtonUp);

	check(0);	// Sanity check, should never be in here
	/*
	InViewport->Buttons &= ~InButtons;

	//if( InButtons&MOUSE_Left ) { debugf(TEXT("UP : MOUSE_Left")); }
	//if( InButtons&MOUSE_Middle ) { debugf(TEXT("UP : MOUSE_Middle")); }
	//if( InButtons&MOUSE_Right ) { debugf(TEXT("UP : MOUSE_Right")); }

	GEdModeTools->ClickEndScreen = FVector( InX, InY, 0 );

	//FLevelSceneNode SN( InViewport );
	//GEdModeTools->ClickEndWorld = SN.Deproject( FPlane( InX, InY, 0, 1) );
	*/

	unguard;
}

// A mouse button was double clicked.
void UEdMode::MouseButtonDblClick( UViewport* InViewport, DWORD InButtons, INT InX, INT InY )
{
	guard(UEdMode::MouseButtonDblClick);
	check(0);	// Sanity check, should never be in here
	unguard;
}

// The mouse was moved.
void UEdMode::MouseMoved( UViewport* InViewport, DWORD InButtons, INT InDeltaX, INT InDeltaY )
{
	guard(UEdMode::MouseMoved);

	check(0);	// Sanity check, should never be in here
	/*
	FVector Delta(0,0,0);
	FRotator DeltaRot(0,0,0);

	GEdModeTools->CalcFreeMoveRot( InViewport, InDeltaX, InDeltaY, Delta, DeltaRot );
	Delta *= GEditor->MovementSpeed;

	if( !(InButtons & (MOUSE_Ctrl | MOUSE_Shift) ) )
	{
		// Move camera.
		GEdModeTools->ViewportMoveRotCamera( InViewport, Delta, DeltaRot );
	}
	else
	{
		// Move actors.
		GEdModeTools->MoveActors( InViewport, GEditor->Level, Delta, DeltaRot, 1, (InButtons & MOUSE_Shift) ? InViewport->Actor : NULL );
	}
	*/

	unguard;
}

// Clicked on something
void UEdMode::Clicked( const FHitCause* InHitCause, HHitProxy* InHitProxy )
{
	guard(UEdMode::Clicked);

	check(0);	// Sanity check, should never be in here
	/*
	// If InHitProxy is NULL, the click was on the backdrop
	if( !InHitProxy )
	{
		// Align all cameras on the click location.
		if( InHitCause->Buttons == (MOUSE_Ctrl|MOUSE_Middle) )
		{
			GEditor->Exec( *FString::Printf( TEXT("CAMERA ALIGN X=%1.2f Y=%1.2f Z=%1.2f"), GEdModeTools->ClickLocation.X, GEdModeTools->ClickLocation.Y, GEdModeTools->ClickLocation.Z ) );
		}

		// Add the current actor class
		if( InHitCause->Buttons == MOUSE_Left && InHitCause->Viewport->Input->KeyDown(IK_A) )
		{
			if( GEditor->CurrentClass )
			{
				TCHAR Cmd[256];
				appSprintf( Cmd, TEXT("ACTOR ADD CLASS=%s"), GEditor->CurrentClass->GetName() );
				GEditor->Exec( Cmd );
			}
		}
		
		// Add a light
		if( InHitCause->Buttons == MOUSE_Left && InHitCause->Viewport->Input->KeyDown(IK_L) )
		{
			GEditor->Exec( TEXT("ACTOR ADD CLASS=LIGHT") );
		}

		// Popup menus
		if( InHitCause->Buttons == MOUSE_Right )
		{
			if( InHitCause->Viewport->IsOrtho() )
				GEditor->EdCallback( EDC_RtClickWindowCanAdd, 0, (DWORD)&(InHitCause->Viewport->OrigCursor) );
			else
				GEditor->EdCallback( EDC_RtClickWindow, 0, (DWORD)&(InHitCause->Viewport->OrigCursor) );
		}

		// Clicking on the backdrop (while not holding ctrl) will deselect everything
		if( InHitCause->Buttons == MOUSE_Left )
		{
			if( !(InHitCause->Buttons & MOUSE_Ctrl) )
			{
				GEditor->Trans->Begin( TEXT("Select None"), false ); // gam
				GEditor->SelectNone( InHitCause->Viewport->Actor->GetLevel(), 1 );
				GEditor->Trans->End();
			}
		}
	}
	else
	{
		// Clicking on an actor ...
		AActor* Actor = InHitProxy->GetActor();
		if( Actor )
		{
			GEditor->Trans->Begin( TEXT("Select Actor"), false ); // gam

			if( !(InHitCause->Buttons & MOUSE_Ctrl) )
			{
				GEditor->SelectNone( InHitCause->Viewport->Actor->GetLevel(), 1 );
				GEditor->SelectActor( InHitCause->Viewport->Actor->GetLevel(), Actor );
			}
			else
				GEditor->SelectActor( InHitCause->Viewport->Actor->GetLevel(), Actor, !Actor->bSelected );

			GEditor->Trans->End();
		}

		// Clicking on a BSP surface ...
		if( InHitProxy->IsA( TEXT("HBspSurf") ) )
		{
			UModel*   Model = InHitCause->Viewport->Actor->GetLevel()->Model;
			FBspSurf& Surf  = Model->Surfs(((HBspSurf*)InHitProxy)->iSurf);

			GEditor->Trans->Begin( TEXT("Select BSP Surface"), false ); // gam

			if( !(InHitCause->Buttons & MOUSE_Ctrl) )
			{
				GEditor->SelectNone( InHitCause->Viewport->Actor->GetLevel(), 1 );
				GEditor->SelectBSPSurf( InHitCause->Viewport->Actor->GetLevel(), ((HBspSurf*)InHitProxy)->iSurf );
			}
			else
				GEditor->SelectBSPSurf( InHitCause->Viewport->Actor->GetLevel(), ((HBspSurf*)InHitProxy)->iSurf, !(Surf.PolyFlags & PF_Selected) );

			GEditor->Trans->End();
		}
	}
	*/

	unguard;
}

/*------------------------------------------------------------------------------
	UEdModeCamera.
------------------------------------------------------------------------------*/

UEdModeCamera::UEdModeCamera()
{
	BitmapName = TEXT("ModeCamera");
	ModeName = TEXT("CAMERAMOVE");
	ToolTip = TEXT("Camera (move camera/actors freely)");
}

UEdModeCamera::~UEdModeCamera()
{
}

/*------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------*/

