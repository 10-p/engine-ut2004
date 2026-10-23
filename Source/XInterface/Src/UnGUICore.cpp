/*=============================================================================
	UnGUICore.cpp: See .UC for for info
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Contains the core GUI classes

Revision history:
	* Created by Michel Comeau
	* Revised and branched from UnGUI.cpp by Ron Prestenback
=============================================================================*/

#include "XInterface.h"
#include "Engine.h"
#include "FConfigCacheIni.h"

IMPLEMENT_CLASS(UGUIController);
IMPLEMENT_CLASS(UGUI);
IMPLEMENT_CLASS(UGUIFont);
IMPLEMENT_CLASS(UGUIStyles);
IMPLEMENT_CLASS(UGUIComponent);
IMPLEMENT_CLASS(UGUIMultiComponent);
IMPLEMENT_CLASS(UPropertyManagerBase);

void UGUIController::GetCursorPos( INT& ptX, INT& ptY )
{
	guard(UGUIController::GetCursorPosition);

	UViewport* Viewport = Cast<UViewport>(ViewportOwner);
	if ( Viewport )
	{
		if ( Viewport->IsFullscreen() )
		{
			ptX = MouseX, ptY = MouseY;
			return;
		}

		Viewport->UpdateMousePosition();
		ptX = (INT) Viewport->WindowsMouseX;
		ptY = (INT) Viewport->WindowsMouseY;
	}
	unguard;
}

void GUIappSprintf( INT MaxLen, TCHAR* Dest, const TCHAR* Fmt, ... )
{
	GET_VARARGS( Dest, MaxLen, Fmt, Fmt );
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIController
// =======================================================================================================================================================
// =======================================================================================================================================================

UBOOL UGUIController::DesignKeyEvent( BYTE iKey, BYTE State, FLOAT delta )
{
	guard(UGUIController::DesignKeyEvent);

	if ( !bModAuthor )
		return false;

	if ( iKey == IK_D && State == IST_Release && KeyDown[IK_Ctrl] && KeyDown[IK_Alt] )
	{
		bDesignMode = !bDesignMode;
		SaveConfig();

		if ( bDesignMode && ActivePage )
			SetMoveControl(ActivePage);
		else SetMoveControl(NULL);

		ResetInput();
		return true;
	}

	if ( iKey == IK_E && State == IST_Release && KeyDown[IK_Ctrl] && KeyDown[IK_Alt] )
	{
		if ( DesignerVisible() )
			ShowProperties(NULL);
		else ShowProperties(ActivePage);

		ResetInput();
		return true;
	}

	if ( !bDesignMode && !DesignerVisible() )
		return false;


	// Events that should happen in interactive mode - can be overridden by holding Ctrl
	if ( bInteractiveMode || KeyDown[IK_Ctrl] )
	{
		// Events that should happen on key release
		if ( State == IST_Release )
		{
			switch ( iKey )
			{
			case IK_H:
				bHighlightCurrent = !bHighlightCurrent;
				SaveConfig();
				return true;

			case IK_I:
				bInteractiveMode = !bInteractiveMode;
				return true;

			case IK_Tab:
				if ( KeyDown[IK_Alt] )
					return true;

				{
					UGUIComponent* OldMoveCtrl = MoveControl;
					if ( !ActivePage->SpecialHit(1) || MoveControl == OldMoveCtrl )
					{
						if ( ContextMenu && ContextMenu->SpecialHit(1) )
							return true;

						if ( MouseOver && MouseOver->SpecialHit(1) )
							return true;

						SetMoveControl(NULL);
					}
				}

				return true;

			case IK_P:
				bDrawFullPaths = !bDrawFullPaths;
				SaveConfig();
				return true;

			case IK_LeftMouse:
				if ( KeyDown[IK_Ctrl] || (bInteractiveMode && !bIgnoreNextRelease) )
				{
					if ( MoveControl && !MoveControl->WithinBounds(MouseX,MouseY) )
						SetMoveControl(ActiveControl);
					else
					{
						UGUIComponent* OldMoveCtrl = MoveControl;
						if ( !ActivePage->SpecialHit(0) || MoveControl == OldMoveCtrl )
						{
							if ( ContextMenu && ContextMenu->SpecialHit(1) )
							{
								bIgnoreNextRelease = 0;
								return true;
							}

							if ( MouseOver && MouseOver->SpecialHit(1) )
							{
								bIgnoreNextRelease = 0;
								return true;
								}

							SetMoveControl(NULL);
						}
					}
				}

				bIgnoreNextRelease = 0;
				return true;

			case IK_U: // update the current component's values
				if ( DesignerVisible() && MoveControl )
				{
					ShowProperties(MoveControl);
					return true;
				}

			// Insert additional handlers here
			}
		}

		// Events that happen on key press
		else if ( State == IST_Press )
		{
			if ( iKey == IK_LeftMouse )
			{
				// Make sure the mouse is within bounds
				INT ptX, ptY;
				GetCursorPos( ptX, ptY );
				if ( ptX > ResX || ptY > ResY || ptX < 0 || ptY < 0 )
				{
					// Out of bounds
					ResetInput();
					return true;
				}
			}

			if ( MoveControl )
			{
				FLOAT AL = MoveControl->ActualLeft(), AT = MoveControl->ActualTop(), AW = MoveControl->ActualWidth(), AH = MoveControl->ActualHeight();
		
				FLOAT scale = AltPressed ? 5.0 : 1.0;

				// Handle positioning
				switch ( iKey )
				{
				case IK_Left:           eventMoveFocused(MoveControl, -1,0,0,0,ResX,ResY, scale); return true; 
				case IK_Right:          eventMoveFocused(MoveControl, 1,0,0,0,ResX,ResY, scale);	 return true; 
				case IK_Up:             eventMoveFocused(MoveControl, 0,-1,0,0,ResX,ResY, scale); return true; 
				case IK_Down:           eventMoveFocused(MoveControl, 0,1,0,0,ResX,ResY, scale);	 return true; 
				case IK_GreyPlus:       eventMoveFocused(MoveControl, 0,0,1,0,ResX,ResY, scale);  return true;
				case IK_GreyMinus:      eventMoveFocused(MoveControl, 0,0,-1,0,ResX,ResY, scale); return true;
				case IK_Equals:         eventMoveFocused(MoveControl, 0,0,0,1,ResX,ResY, scale);	 return true; 
				case IK_Minus:          eventMoveFocused(MoveControl, 0,0,0,-1,ResX,ResY, scale); return true;
				case IK_MouseWheelUp:   SetMoveControl(MoveControl == ActivePage ? ActiveControl : MoveControl->MenuOwner);     return true;
				case IK_MouseWheelDown: SetMoveControl(MoveControl->GetFocused() ? MoveControl->GetFocused() : FocusedControl); return true;
				case IK_C:
					if ( AltPressed )
					{
						// convert abs value to scaled value
						MoveControl->bScaleToParent=1;
						MoveControl->bBoundToParent=1;
					}

					MoveControl->WinHeight = MoveControl->RelativeHeight( AH );
					MoveControl->WinWidth = MoveControl->RelativeWidth( AW );
					MoveControl->WinLeft = MoveControl->RelativeLeft( AL );
					MoveControl->WinTop = MoveControl->RelativeTop( AT );

					appClipboardCopy( *(FString::Printf(TEXT("\t\tWinWidth=%f%s\t\tWinHeight=%f%s\t\tWinLeft=%f%s\t\tWinTop=%f%s"), MoveControl->WinWidth, LINE_TERMINATOR, MoveControl->WinHeight, LINE_TERMINATOR, MoveControl->WinLeft, LINE_TERMINATOR, MoveControl->WinTop, LINE_TERMINATOR)));
					return true;

				case IK_X:	// Export all properties
					if ( AltPressed )
					{
						// convert abs value to scaled value
						MoveControl->bScaleToParent=1;
						MoveControl->bBoundToParent=1;
					}

					MoveControl->WinHeight = MoveControl->RelativeHeight( AH );
					MoveControl->WinWidth = MoveControl->RelativeWidth( AW );
					MoveControl->WinLeft = MoveControl->RelativeLeft( AL );
					MoveControl->WinTop = MoveControl->RelativeTop( AT );

					FStringOutputDevice Ar;

					MoveControl->SetFlags( RF_TagImp );
					UExporter::ExportToOutputDevice( MoveControl, NULL, Ar, TEXT("T3D"), 4 );
					MoveControl->ClearFlags( RF_TagImp );

					appClipboardCopy( *Ar );
					return true;
				}
			}
		}

	// Handle moving and resizing of controls
		else if ( State == IST_Axis && MoveControl )
		{
			bForceMouseCheck = false;
			FLOAT dX = delta, dY = delta;

			FLOAT Scale = ViewportOwner->bShowWindowsMouse ? 1.f : MenuMouseSens;

			if ( iKey == IK_MouseX )
			{
				if ( (KeyDown[IK_Alt] && !bInteractiveMode) || (bInteractiveMode && KeyDown[IK_LeftMouse]) )
				{
					// move component
					MouseX += (dX * MenuMouseSens);
					eventMoveFocused(MoveControl, 1, 0, 0, 0, ResX, ResY, dX * Scale);
					bIgnoreNextRelease = 1;
					return true;
				}

				else if ( bInteractiveMode && KeyDown[IK_RightMouse] )
				{
					// resize component
					MouseX += (dX * MenuMouseSens);
					eventMoveFocused(MoveControl, 0, 0, 1, 0, ResX, ResY, dX * Scale);
					return true;
				}
			}
			else if ( iKey == IK_MouseY )
			{
				if ( (KeyDown[IK_Alt] && !bInteractiveMode) || (bInteractiveMode && KeyDown[IK_LeftMouse]) )
				{
					// move component
					MouseY -= (dY * MenuMouseSens);
					eventMoveFocused(MoveControl, 0, -1, 0, 0, ResX, ResY, dY * Scale);
					bIgnoreNextRelease = 1;
					return true;
				}

				else if ( bInteractiveMode && KeyDown[IK_RightMouse] )
				{
					// resize component
					MouseY -= (dY * MenuMouseSens);
					eventMoveFocused(MoveControl, 0, 0, 0, -1, ResX, ResY, dY * Scale);
					return true;
				}
			}
		}

		// Events that should only happen in interactive mode
		if ( bInteractiveMode )
		{
		}

		// These are the keys which should be swallowed entirely while in design mode
		if ( iKey == IK_LeftMouse || iKey == IK_RightMouse || iKey == IK_Tab || iKey == IK_Left || iKey == IK_Right ||
			 iKey == IK_Up || iKey == IK_Down )
			return true;
	}

	return false;
	unguard;
}


void UGUIController::execCheckForECE( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIController::execCheckForECE);
	P_FINISH;

	*(UBOOL*)Result = ( GFileManager->FileSize(TEXT("ONSlaughtbp.u")) > 0 );

	unguardexec;
}

void UGUIController::execProfile( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIController::execProfile);
	P_GET_STR(ProfileName);
	P_FINISH;

	Profile(ProfileName);
	unguardexec;
}

void UGUIController::Profile( FString& ProfileName )
{
	guard(UGUIController::Profilers);

	INT i = 0;
	for (; i < Profilers.Num(); i++)
		if ( Profilers(i).ProfileName == ProfileName )
			break;

	if ( i == Profilers.Num() )
	{
		FProfileStruct Prof;
		appMemzero(&Prof,sizeof(FProfileStruct));

		Prof.ProfileName = ProfileName;
		Prof.ProfileSeconds = appSeconds();

		INT j = Profilers.AddZeroed();
		Profilers(j) = Prof;
		debugf(NAME_Timer, TEXT("Beginning profile for %s ( %f )"), *ProfileName, Prof.ProfileSeconds);
	}

	else
	{
		FLOAT CurTime = appSeconds();
		debugf(NAME_Timer, TEXT("Ending profile for %s ( %f ) Total length: %f ms."), *Profilers(i).ProfileName, CurTime, (CurTime - Profilers(i).ProfileSeconds)*1000);

		Profilers.Remove(i);
	}
	unguard;
}

void UGUIController::ResetInput()
{
	guard(UGUIController::ResetInput);

	for ( INT i = 0; i < IK_MAX; i++ )
		KeyDown[i] = 0;

	CtrlPressed = 0;
	AltPressed = 0;
	ShiftPressed = 0;

	RepeatKey = 0;
	RepeatDelta = 0;
	RepeatTime = 0;

	bIgnoreNextRelease = 0;
	bIgnoreUntilPress = 0;
	bForceMouseCheck = 0;

	if ( FocusedControl && FocusedControl->MenuState != MSAT_Focused )
		FocusedControl->eventMenuStateChange(MSAT_Focused);

	if ( ActiveControl && ActiveControl->MenuState != MSAT_Blurry )
		ActiveControl->eventMenuStateChange(MSAT_Blurry);

	unguard;
}

void UGUIController::execResetInput( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIController::execResetInput);

	P_FINISH;

	ResetInput();

	unguardexec;
}

void UGUIController::execLaunchURL( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIController::execProfile);
	P_GET_STR(TargetURL);
	P_FINISH;

	if (TargetURL.Caps().Left(9) == TEXT("UT2004://") )
	{
		FString NewURL = FString::Printf(TEXT("open %s"),*TargetURL);
		Master->Client->Viewports(0)->Exec(*NewURL);
		return;
	}

	if (TargetURL.Caps().Left(7) != TEXT("HTTP://") )
	{
		GWarn->Logf(TEXT("Bad URL [%s]"), *TargetURL);
		return;
	}

	guard(ExternalURL);

	if ( Master->Client->Viewports(0)->IsFullscreen() )
		Master->Client->Viewports(0)->Exec(TEXT("togglefullscreen"));

	appLaunchURL( *TargetURL, TEXT("") );

	unguard;

	unguardexec;
}

#ifdef UCONST_Counter
void UGUIComponent::ResetCounter()
{
	OnPreDrawCount = 0;
	PreDrawCount = 0;
	DrawCount = 0;
	OnDrawCount = 0;
	OnRenderCount = 0;
	OnRenderedCount = 0;

	if ( ContextMenu )
		ContextMenu->ResetCounter();

	if ( ToolTip )
		ToolTip->ResetCounter();
}

void UGUIMultiComponent::ResetCounter()
{
	Super::ResetCounter();
	for ( INT i = 0; i < Controls.Num(); i++ )
	{
		if ( Controls(i) )
			Controls(i)->ResetCounter();
	}
}

void UGUIMultiOptionList::ResetCounter()
{
	Super::ResetCounter();
	for ( INT i = 0; i < Elements.Num(); i++)
		if ( Elements(i) )
			Elements(i)->ResetCounter();
}

void UGUITabControl::ResetCounter()
{
	Super::ResetCounter();
	for ( INT i = 0; i < TabStack.Num(); i++ )
		if ( TabStack(i) )
			TabStack(i)->ResetCounter();
}

#endif
void  UGUIController::NativeMessage(const FString Msg, FLOAT MsgLife)
{
}

// NativeTick is responsible for faking mouse button repeats and setting the fade value for
// the text cursor.  

void  UGUIController::NativeTick(FLOAT DeltaTime)
{
	if ( (ViewportOwner==NULL) || (ViewportOwner->Actor==NULL) || (ViewportOwner->Actor->Level==NULL) )
		return;

	// Use real seconds for TimeBased
	// TODO: Check if other/all UInteraction need to use RealTime
	DeltaTime /= ViewportOwner->Actor->Level->TimeDilation;
	RenderDelta = DeltaTime;

	// Update the activity timer
	if ( MouseOver )
		MouseOver->delegateTick( DeltaTime );

	UGUIPage* TestPage = ActivePage;
	INT LastPage = 0;

	// Disable bRenderLevel here if this is supposed to cover whole viewport.
	if ( Master && Master->Client && Master->Client->Viewports.Num() )
	{
		UViewport* Viewport = CastChecked<UViewport>(Master->Client->Viewports(0));
		INT compnum = MenuStack.Num() - 1;
		for (INT i = compnum; i >= 0; i--)
		{
			TestPage = MenuStack(i);
			if ( TestPage )
			{
				if ( TestPage->bRequire640x480 || !TestPage->bRenderWorld )
				{
					Viewport->Canvas->bRenderLevel = TestPage->bRenderWorld && !(TestPage->bRequire640x480&&(GameResolution.Len()>0));
					LastPage = i;
					break;
				}
			}
		}
	}

	// Update all of the button arrays
	if (RepeatKey)
	{
		RepeatTime-=DeltaTime;
		if ( RepeatTime <= 0 )
		{
			BYTE Hold=IST_Hold;
			NativeKeyEvent(RepeatKey, Hold, RepeatDelta);
			RepeatTime=ButtonRepeatDelay;
		}
	}

	CursorFade+= 255 * (DeltaTime) * CursorStep;
	if (CursorFade<=0.0)
	{
		CursorFade=0;
		CursorStep = 1;
	}
	else if (CursorFade>=255.0)
	{
		CursorFade=255;
		CursorStep = -1;
	}

	FastCursorFade+= 8192 * (DeltaTime) * FastCursorStep;
	if (FastCursorFade<=0.0)
	{
		FastCursorFade=0;
		FastCursorStep = 1;
	}
	else if (FastCursorFade>=255.0)
	{
		FastCursorFade=255;
		FastCursorStep = -1;
	}

	for ( INT i = MenuStack.Num() - 1; i >= LastPage; i-- )
	{
		TestPage = MenuStack(i);
		if ( TestPage )
			TestPage->UpdateTimers(DeltaTime);
	}
}

// Pre Render is used for sizing/positioning

void  UGUIController::NativePreRender(UCanvas* Canvas)
{
	guard(GUIController::NativePreRender);

	if ( !ViewportOwner || Canvas == NULL )
		return;

	UViewport* Viewport = CastChecked<UViewport>(ViewportOwner);
	ResX = Viewport->SizeX;
	ResY = Viewport->SizeY;

	// If we are using the Windows Mouse.. pass it along before any rendering
	Viewport->bShowWindowsMouse = Viewport->bWindowsMouseAvailable;

	// Move through the stack and Draw each Page.  Each page will then itterate though all of
	// it's components and render them

	for (INT i=0;i < MenuStack.Num(); i++)
	{
		if ( MenuStack(i) )
		{
#ifdef UCONST_DoCounter
			MenuStack(i)->ResetCounter();
#endif
			MenuStack(i)->PreDraw(Canvas);
		}
	}

	if (ContextMenu != NULL)
	{
		ContextMenu->SaveCanvasState(Canvas);
		ContextMenu->PreDraw(Canvas);
		ContextMenu->RestoreCanvasState(Canvas);
	}

	if ( MouseOver != NULL )
	{
		MouseOver->SaveCanvasState(Canvas);
		MouseOver->PreDraw(Canvas);
		MouseOver->RestoreCanvasState(Canvas);
	}

	unguard;
}

// NativePostRender passes render control to each component.  It starts with all components
// on the Active page, and then continues until all components are rendered

void  UGUIController::NativePostRender(UCanvas* Canvas)
{

	guard(GUIController::NativePostRender);

	if ( !Canvas || !ViewportOwner )
		return;

	// Build a deltatime for animation
/*
	FLOAT CurrentTime = Master->Client->Viewports(0)->CurrentTime;
	RenderDelta = CurrentTime - LastRenderTime;
	LastRenderTime = CurrentTime;
*/

	Canvas->CurX = 0;
	Canvas->ColorModulate = FPlane (1.0, 1.0, 1.0, 1.0);	// Fix their hack
	Canvas->Style=1;

	UGUIPage* page = NULL;

	// If any page has bRenderWorld set to false, don't render any other menus behind it
	INT i = MenuStack.Num() - 1;
	for ( ;i > 0; i-- )
	{
		page = MenuStack(i);
		if ( page && !page->bRenderWorld )
			break;
	}

	// Move through the stack and Draw each Page.  Each page will then itterate though all of
	// it's components and render them
	for (; i >= 0 && i < MenuStack.Num(); i++)
	{
		page = MenuStack(i);
		if (page==NULL)
		{
			GWarn->Logf(NAME_Warning, TEXT("MenuStack out of sync"));
			break;
		}
		
		// By default, use the active page's inactive fade color
		if (page != ActivePage && bModulateStackedMenus)
			Canvas->ColorModulate = ActivePage ? ActivePage->InactiveFadeColor : page->InactiveFadeColor;
		else
			Canvas->ColorModulate = FPlane (1.0f, 1.0f, 1.0f, 1.0f);

		page->Draw(Canvas);
	}

	if (ContextMenu != NULL)
	{
		ContextMenu->SaveCanvasState(Canvas);
		ContextMenu->Draw(Canvas);
		ContextMenu->RestoreCanvasState(Canvas);
	}

	if ( MouseOver != NULL )
	{
		MouseOver->SaveCanvasState(Canvas);
		MouseOver->Draw(Canvas);
		MouseOver->RestoreCanvasState(Canvas);
	}

	if (bDesignMode)
		RenderDesignMode( Canvas );
	else if ( MoveControl && DesignerVisible() )
	{
		if ( ActivePage )
			ActivePage->SaveCanvasState(Canvas);

		Canvas->Color = FColor(255,0,0,255);
		Canvas->DrawTileStretched(WhiteBorder, MoveControl->Bounds[0], MoveControl->Bounds[1],
				MoveControl->Bounds[2]-MoveControl->Bounds[0], MoveControl->Bounds[3]-MoveControl->Bounds[1]);

		if ( ActivePage )
			ActivePage->RestoreCanvasState(Canvas);
	}

	// While a game pad moves the cursor the system cursor is hidden: the GUI draws its own at the system cursor's place.
	UViewport* OwnerViewport = Cast<UViewport>(ViewportOwner);
	const UBOOL PadCursor = ViewportOwner->bShowWindowsMouse && OwnerViewport && OwnerViewport->bPadCursor;
	if ( PadCursor )
	{
		MouseX = ViewportOwner->WindowsMouseX;
		MouseY = ViewportOwner->WindowsMouseY;
	}

	// Render the Mouse Cursor if we are not using the windows cusor.  Otherwise set the cursor index
	if (!ViewportOwner->bShowWindowsMouse || PadCursor)
	{
		
		UMaterial* MouseMat;
		FVector MouseOffset;

		if (ActiveControl!=NULL)
		{
			MouseMat = MouseCursors(ActiveControl->MouseCursorIndex);
			MouseOffset = MouseCursorOffset(ActiveControl->MouseCursorIndex);
		}
		else
		{
			MouseMat = MouseCursors(0);
			MouseOffset = MouseCursorOffset(0);
		}

		if (MouseMat!=NULL)
		{

			FColor OldColor         = Canvas->Color;
			BYTE   OldCanvasStyle   = Canvas->Style;
			FPlane OldColorModulate = Canvas->ColorModulate;

			Canvas->Color = FColor(255,255,255,255);
			Canvas->Style = STY_Alpha;
			Canvas->ColorModulate = FPlane (1.0f, 1.0f, 1.0f, 1.0f);


			FLOAT MouseScale = Max<FLOAT>(Canvas->SizeX / 640, 1.f);
			if (!bFixedMouseSize)
			{
				if ( ActiveControl!=NULL && ActiveControl->bAcceptsInput && !(ActiveControl->IsA(UGUIPage::StaticClass()) || ActiveControl->IsA(UGUIPanel::StaticClass())) )
					MouseScale=1.33f;
			}

			FLOAT MX = Clamp<FLOAT>(MouseX, 0.f, Canvas->SizeX);
			FLOAT MY = Clamp<FLOAT>(MouseY, 0.f, Canvas->SizeY);

			FLOAT MouseSizeX = MouseMat->MaterialUSize()*MouseScale;
			FLOAT MouseSizeY = MouseMat->MaterialVSize()*MouseScale;

			INT DrawMouseX = MX - (MouseOffset.X * MouseSizeX);
			INT DrawMouseY = MY - (MouseOffset.Y * MouseSizeY);

			MouseCursorBounds.X1 = DrawMouseX;
			MouseCursorBounds.Y1 = DrawMouseY;
			MouseCursorBounds.X2 = DrawMouseX + MouseSizeX;
			MouseCursorBounds.Y2 = DrawMouseY + MouseSizeY;

			// Clamp the software mouse
			LastMouseX = MouseX;
			LastMouseY = MouseY;

			MouseX = MX;
			MouseY = MY;

			// If the Window's mouse is available, add code here to use it instead		

			Canvas->DrawTile
			(
				MouseMat,
				DrawMouseX,
				DrawMouseY,
				MouseSizeX,
				MouseSizeY,
				0,
				0,
				MouseMat->MaterialUSize(),
				MouseMat->MaterialVSize(),
				Canvas->Z,
				Canvas->Color.Plane(),
				FPlane(0.0f,0.0f,0.0f,0.0f)
			);

			Canvas->ColorModulate = OldColorModulate;
			Canvas->Color = OldColor;
			Canvas->Style = OldCanvasStyle;
		}
	}
	else
	{
		LastMouseX = MouseX;
		LastMouseY = MouseY;

		MouseX = ViewportOwner->WindowsMouseX;
		MouseY = ViewportOwner->WindowsMouseY;

		FLOAT MX = Clamp<FLOAT>(MouseX, 0, Canvas->SizeX);
		FLOAT MY = Clamp<FLOAT>(MouseY, 0, Canvas->SizeY);

		MouseCursorBounds.X1 = MX;
		MouseCursorBounds.X2 = MX + 5.f;
		MouseCursorBounds.Y1 = MY;
		MouseCursorBounds.Y2 = MY + 5.f;

		if (ActiveControl!=NULL)
			ViewportOwner->SelectedCursor = ActiveControl->MouseCursorIndex;
	}

	unguard;

}

void UGUIController::RenderDesignMode( UCanvas* Canvas )
{
	guard(UGUIController::RenderDesignMode);

	Canvas->CurX = 0;
	Canvas->CurY = 0;

	FPlane OldModulate = Canvas->ColorModulate, Empty;
	FColor OldColor = Canvas->Color, Black(0,0,0,255);
	BYTE OldStyle = Canvas->Style;

	UGUIFont* MyFont = eventGetMenuFont(TEXT("SmallTextFont"));
	if (MyFont!=NULL)
	{
		UFont* CFont = MyFont->eventGetFont(800);
		if (CFont==NULL)	
			return;

		Canvas->Font = CFont;
	}

	TCHAR Position[64];

	// Display help
	if ( KeyDown[IK_F1] )
	{
		INT X = 0, Y = 0;
		FLOAT XL = 0.f, YL = 0.f;
		const TCHAR* Hint, *Desc;

		Canvas->ClippedStrLen(Canvas->Font, 1.0, 1.0, XL, YL, TEXT("Qq"));
		for ( INT i = 0; i < DesignModeHints.Num(); i++ )
		{
			Hint = *DesignModeHints(i).Key, Desc = *DesignModeHints(i).Description;
			Canvas->DrawTile(DefaultPens[0], X, Y, ResX, YL, 0, 0, 32, 32, 0.f, Black, Empty);
			Canvas->DrawTextJustified( TXTA_Left, X, Y, ResX, Y + YL, TEXT("%s%s"), Hint, Desc );
			Y += YL;
		}
	}
	else
	{
		// Draw state
		FLOAT XL = 0.f, YL = 0.f;
		appSprintf( Position, TEXT("E:%i  I:%i  H:%i  P:%i"), DesignerVisible(), bInteractiveMode, bHighlightCurrent, bDrawFullPaths );

		Canvas->ClippedStrLen(Canvas->Font, 1.0, 1.0, XL, YL, Position);
		Canvas->DrawTile(DefaultPens[0], 0, 0, XL, YL, 0, 0, 32, 32, 0.f, Black, Empty);
		Canvas->DrawTextJustified( TXTA_Left, 0, 0, XL, YL, TEXT("%s"), Position);
	}

	// Draw the mouse cursor position in the upper right corner of the screen
	INT X = Canvas->ClipX, Y=0;
	FLOAT XL = 0.0f, YL = 0.0f;
	appSprintf( Position, TEXT("X:%.2f Y:%.2f"), MouseX, MouseY );
	
	Canvas->ClippedStrLen( Canvas->Font, 1.0, 1.0, XL, YL, Position );
	Canvas->DrawTile(DefaultPens[0], X - XL, Y, XL, YL, 0, 0, 32, 32, 0.0f, Black,Empty);
	Canvas->DrawTextJustified( 2, 0, Y, X, YL, TEXT("%s"), Position );


	if ( bHighlightCurrent )
	{
		TCHAR MenuPath[2048];
		TArray<FString> Lines;
		INT i = 0;

		// Set the starting position
		Y = Canvas->ClipY;

		// Don't display this data if shift is pressed
		if ( !ShiftPressed )
		{
			Y -= YL;
			Canvas->DrawTile(DefaultPens[0], 0,Y,Canvas->SizeX, YL,0,0,32,32, 0.0f, Black,Empty);

			if (FocusedControl)
			{
				GUIappSprintf( ARRAY_COUNT(MenuPath), MenuPath, TEXT("Focused - MS:%i DS:%i CM:%i TO:%i %s%s"),FocusedControl->MenuState,FocusedControl->DropState,FocusedControl->bCaptureMouse, FocusedControl->bTabStop ? FocusedControl->TabOrder : -1, bDrawFullPaths ? FocusedControl->GetMenuPath() : FocusedControl->GetName(), *FocusedControl->eventAdditionalDebugString() );

				Lines.Empty();
				Canvas->WrapStringToArray( MenuPath, &Lines, Canvas->SizeX );

				for ( i = Lines.Num() - 1; i > 0; i-- )
				{
					Canvas->DrawTextJustified(0, 0, Y, Canvas->SizeX, Y + YL, TEXT("...: %s"), *Lines(i));

					Y -= YL;
					Canvas->DrawTile(DefaultPens[0], 0,Y,Canvas->SizeX, YL,0,0,32,32, 0.0f, Black,Empty);
				}
				Canvas->DrawTextJustified(0,0,Y,Canvas->SizeX, Y + YL, *Lines(i));
			}
			else
				Canvas->DrawTextJustified(0,0,Y,Canvas->SizeX,Y + YL, TEXT("Focused - None"));

			Y -= YL;
			Canvas->DrawTile(DefaultPens[0], 0,Y,Canvas->SizeX, YL,0,0,32,32, 0.0f, Black,Empty);

			// Draw the data for the ActiveControl & FocusedControl
			if (ActiveControl!=NULL)
			{
				GUIappSprintf(ARRAY_COUNT(MenuPath), MenuPath, TEXT("Active - MS:%i DS:%i CM:%i TO:%i (%f,%f,%f,%f) %s%s"),ActiveControl->MenuState,ActiveControl->DropState,ActiveControl->bCaptureMouse,ActiveControl->bTabStop ? ActiveControl->TabOrder : -1,ActiveControl->ActualLeft(),ActiveControl->ActualTop(),ActiveControl->ActualWidth(),ActiveControl->ActualHeight(), bDrawFullPaths ? ActiveControl->GetMenuPath() : ActiveControl->GetName(),*ActiveControl->eventAdditionalDebugString());

				Lines.Empty();
				Canvas->WrapStringToArray( MenuPath, &Lines, Canvas->SizeX );

				for ( i = Lines.Num() - 1; i > 0; i-- )
				{
					Canvas->DrawTextJustified(0, 0, Y, Canvas->SizeX, Y + YL, TEXT("...: %s"), *Lines(i));
					Y -= YL;
					Canvas->DrawTile(DefaultPens[0], 0,Y,Canvas->SizeX, YL,0,0,32,32, 0.0f, Black,Empty);
				}
				Canvas->DrawTextJustified(0,0,Y,Canvas->SizeX, Y + YL, *Lines(i));
			}
			else
				Canvas->DrawTextJustified(0,0,Y,Canvas->SizeX,Y + YL, TEXT("Active - None"));

			// Draw the drag-n-drop control
			if ( DropSource || DropTarget )
			{
				Y -= YL;
				Canvas->DrawTile( DefaultPens[0], 0, Y, Canvas->SizeX, YL,0,0,32,32,0.0f,Black,Empty);

				GUIappSprintf( ARRAY_COUNT(MenuPath),MenuPath,TEXT("DropTarget: %s DS:%i%s"), DropTarget ? (bDrawFullPaths ? DropTarget->GetMenuPath() : DropTarget->GetName()) : TEXT("None"), DropTarget ? DropTarget->DropState : -1, DropTarget ? *DropTarget->eventAdditionalDebugString() : TEXT(""));

				Lines.Empty();
				Canvas->WrapStringToArray( MenuPath, &Lines, Canvas->SizeX );

				for ( i = Lines.Num() - 1; i > 0; i-- )
				{
					Canvas->DrawTextJustified(0, 0, Y, Canvas->SizeX, Y + YL, TEXT("...: %s"), *Lines(i));
					Y -= YL;
					Canvas->DrawTile(DefaultPens[0], 0,Y,Canvas->SizeX, YL,0,0,32,32, 0.0f, Black,Empty);
				}
				Canvas->DrawTextJustified(0,0,Y,Canvas->SizeX, Y + YL, *Lines(i));

				Y -= YL;
				Canvas->DrawTile( DefaultPens[0], 0, Y, Canvas->SizeX, YL,0,0,32,32,0.0f,Black,Empty);

				GUIappSprintf( ARRAY_COUNT(MenuPath), MenuPath, TEXT("DropSource: %s DS:%i%s"), DropSource ? (bDrawFullPaths ? DropSource->GetMenuPath() : DropSource->GetName()) : TEXT("None"), DropSource ? DropSource->DropState : -1, DropSource ? *DropSource->eventAdditionalDebugString() : TEXT(""));

				Lines.Empty();
				Canvas->WrapStringToArray( MenuPath, &Lines, Canvas->SizeX );

				for ( i = Lines.Num() - 1; i > 0; i-- )
				{
					Canvas->DrawTextJustified(0, 0, Y, Canvas->SizeX, Y + YL, TEXT("...: %s"), *Lines(i));
					Y -= YL;
					Canvas->DrawTile(DefaultPens[0], 0,Y,Canvas->SizeX, YL,0,0,32,32, 0.0f, Black,Empty);
				}
				Canvas->DrawTextJustified(0,0,Y,Canvas->SizeX, Y + YL, *Lines(i));

			}

			// Draw the MoveControl data
			if (MoveControl)
			{
				Y -= YL;
				Canvas->DrawTile(DefaultPens[0], 0,Y,Canvas->SizeX,YL,0,0,32,32, 0.0f, Black,Empty);
				Canvas->DrawTextJustified(2,0,Y,Canvas->SizeX,Y+YL,TEXT("%s %s %s MS:%i (%f %f %f %f) %s :Move"),
					bDrawFullPaths ? MoveControl->GetMenuPath(MenuPath) : MoveControl->GetName(),
					MoveControl->Style ? MoveControl->Style->GetName() : TEXT("None"),
					MoveControl->Style && MoveControl->Style->Fonts[MoveControl->MenuState + (5 * MoveControl->FontScale)]
						? *((MoveControl->Style->Fonts[MoveControl->MenuState+(5*MoveControl->FontScale)])->KeyName) : TEXT("None"),
					MoveControl->MenuState,
					MoveControl->ActualLeft(),MoveControl->ActualTop(),MoveControl->ActualWidth(), MoveControl->ActualHeight(),
					*MoveControl->eventAdditionalDebugString());

				Canvas->Color = FColor(255,0,0,255);
			
				// Draw the bounding rectangle
				Canvas->DrawTileStretched(WhiteBorder, MoveControl->Bounds[0], MoveControl->Bounds[1],
								MoveControl->Bounds[2]-MoveControl->Bounds[0], MoveControl->Bounds[3]-MoveControl->Bounds[1]);

				Canvas->Color = OldColor;
				Canvas->ColorModulate = OldModulate;
				Canvas->Style = OldStyle;
				
			}
		}

		// Draw the focus chain
		if (ActivePage!=NULL && CtrlPressed && AltPressed)
		{
			FLOAT X=0;
			Y=0;			

			UGUIComponent* C = ActivePage;

			while ( C )
			{
				Canvas->DrawTile(DefaultPens[0], 0,Y,Canvas->SizeX,Y+YL,0,0,32,32, 0.0f, Black,Empty);				
				Canvas->DrawTextJustified(0,X,Y,Canvas->SizeX,Y+YL,TEXT("%s TabOrder:%i State:%i Vis:%i Style:%s"),bDrawFullPaths ? C->GetMenuPath(MenuPath) : C->GetName(),C->bTabStop?C->TabOrder:-1, C->MenuState,C->bVisible,C->Style?C->Style->GetName():TEXT("None"));

				C = C->GetFocused();
				X += 5;
				Y += YL;
			}
		}
	}

	else if ( MoveControl && !ShiftPressed )
	{
		Canvas->Color = FColor(255,0,0,255);
		
			// Draw the bounding rectangle
		Canvas->DrawTileStretched(WhiteBorder, MoveControl->Bounds[0], MoveControl->Bounds[1],
				MoveControl->Bounds[2]-MoveControl->Bounds[0], MoveControl->Bounds[3]-MoveControl->Bounds[1]);
	}

	Canvas->Color = OldColor;
	Canvas->ColorModulate = OldModulate;
	Canvas->Style = OldStyle;

	unguard;
}

void UGUIController::execResetKeyboard( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIController::execResetKeyboard);

	P_FINISH;

	UViewport* Viewport = Cast<UViewport>(ViewportOwner);

	if( Viewport && Viewport->Input )
		ResetConfig(Viewport->Input->GetClass());

	unguardexec;
}

void UGUIController::execGetMapList(FFrame& Stack, RESULT_DECL )
{
	guard(GUIController::execGetMapList);

	P_GET_STR(Prefix);
	P_GET_OBJECT(UGUIList, List);
	P_GET_UBOOL_OPTX(bDecoText,0);
	P_FINISH;

	const TArray<FMapRecord>* Records = (TArray<FMapRecord>*)(Cast<UCacheManager>(UCacheManager::StaticClass()->GetDefaultObject())->GetRecords(TEXT("Map")));
	check(Records);

	List->Elements.Empty();
	for (INT i = 0; i < Records->Num(); i++)
	{
		if ((*Records)(i).Acronym == Prefix || Prefix == TEXT(""))
		{
			INT j = List->Elements.AddZeroed();
			List->Elements(j).Item = (*Records)(i).MapName;
			if ( bDecoText )
				List->Elements(j).ExtraData = UxUtil::LoadDecoText( *((*Records)(i).TextName) );
		}
	}
	List->ItemCount = List->Elements.Num();

	unguardexec;
}

void UGUIController::execGetWeaponList(FFrame& Stack, RESULT_DECL )
{
	guard(GUIController::execGetWeaponList);

	P_GET_TARRAY_REF(WeaponClass, UClass*);
	P_GET_TARRAY_REF(WeaponDesc, FString);
	P_FINISH

	WeaponClass->Empty();
	WeaponDesc->Empty();

	TArray<FRegistryObjectInfo> RegList;
	GetRegistryObjects( RegList, UClass::StaticClass(), AWeapon::StaticClass(), 0 );

	for(INT i=0; i<RegList.Num(); i++)
	{
		UClass* weapClass = (UClass*)(StaticLoadObject(UClass::StaticClass(), NULL, *(RegList(i).Object), NULL, LOAD_NoWarn | LOAD_Quiet, NULL ));

		if(!weapClass)
			continue;

		AWeapon* w = (AWeapon*)weapClass->GetDefaultObject();

		if(!w || w->bNotInPriorityList)
			continue;

		// Add class and description to output arrays
		WeaponClass->AddItem( weapClass );

		int k = WeaponDesc->AddZeroed();
		(*WeaponDesc)(k) = RegList(i).Description;
	}

	unguardexec;
}

void UGUIController::execGetOGGList(FFrame& Stack, RESULT_DECL )
{
	guard(GUIController::execGetOGGList);

	P_GET_TARRAY_REF(OGGList, FString);
	P_FINISH

	OGGList->Empty();
	TArray<FString> FileList = GFileManager->FindFiles( TEXT("..\\Music\\*.OGG"), 1, 0 );

	for (int i=0;i<FileList.Num();i++)
	{
		int k = OGGList->AddZeroed();
		(*OGGList)(k) = FileList(i);
	}
	
	unguardexec;
}

void UGUIController::execGetDEMList(FFrame& Stack, RESULT_DECL )
{
	guard(GUIController::execGetDEMList);

	P_GET_TARRAY_REF(DEMList, FString);
	P_FINISH

	DEMList->Empty();
	TArray<FString> FileList = GFileManager->FindFiles( TEXT("..\\Demos\\*.demo4"), 1, 0 );

	for (int i=0;i<FileList.Num();i++)
	{
		int k = DEMList->AddZeroed();
		(*DEMList)(k) = FileList(i);
	}
	
	unguardexec;
}

void UGUIController::execGetDEMHeader(FFrame& Stack, RESULT_DECL )
{
	guard(GUIController::execGetDEMHeader);
	P_GET_STR(DemoFilename);
	P_GET_STR_REF(MapName);
	P_GET_STR_REF(GameType);
	P_GET_INT_REF(ScoreLimit);
	P_GET_INT_REF(TimeLimit);
	P_GET_INT_REF(ClientSide);
	P_GET_STR_REF(RecordedBy);
	P_GET_STR_REF(TimeStamp);
	P_GET_STR_REF(ReqPackages);
	P_FINISH;

	if ( DemoFilename == TEXT("") )
	{
		*MapName = TEXT("No demo name specified for GetDemoHeader() !");
		*(UBOOL*)Result = 0;
		return;
	}

	class FArchive* FileAr;
	FString DemFilename = FString::Printf(TEXT("..\\Demos\\%s"),*DemoFilename);
	FileAr = GFileManager->CreateFileReader( *DemFilename );
	if( !FileAr || FileAr->TotalSize() <= 17 )
	{
		*MapName = FString::Printf(TEXT("Could not load %s"),*DemFilename);
		*(UBOOL*)Result = false;
		return;
	}

	// Skip the new Demo Info Header

	FileAr->Seek(17);
	INT HeaderCheck;
	*FileAr << HeaderCheck;

	if (HeaderCheck != 1038)
	{
		delete FileAr;
		FileAr = NULL;
		*MapName = FString::Printf(TEXT("Demo %s is not a valid Demo"),*DemoFilename);
		*(UBOOL*)Result = false;
		return;
	}

	*FileAr << *MapName;
	*FileAr << *GameType;
	*FileAr << *ScoreLimit;
	*FileAr << *TimeLimit;
	*FileAr << *ClientSide;
	*FileAr << *RecordedBy;
	*FileAr << *TimeStamp;

	FString FileName;
	FGuid	GUID;
	INT		PkgCount, Gen,Cnt=0;
	TCHAR	Out[256];

	*FileAr << PkgCount;

	for (INT i=0;i<PkgCount;i++)
	{
		*FileAr << FileName;
		*FileAr << GUID;
		*FileAr << Gen;

		if ( !appFindPackageFile(*FileName,(const FGuid*)&GUID,Out,Gen) )
		{
			if (Cnt==0)
				*ReqPackages += FileName;
			else
				*ReqPackages += FString::Printf(TEXT(", %s"),*FileName);

			Cnt++;
		}

	}

	delete FileAr;
	FileAr = NULL;

	*(UBOOL*)Result = true;

	unguardexec;
}

void UGUIController::execGetOwnageList(FFrame& Stack, RESULT_DECL )
{
	guard(GUIController::execGetOwnageList);

	P_GET_TARRAY_REF(RLevel, INT);
	P_GET_TARRAY_REF(MName, FString);
	P_GET_TARRAY_REF(MDesc, FString);
	P_GET_TARRAY_REF(MURL, FString);
	P_FINISH

	RLevel->Empty();
	MName->Empty();
	MDesc->Empty();
	MURL->Empty();

	class FArchive* FileAr;
	FString Filename = TEXT("..\\system\\ownagemaps.dat");
	FileAr = GFileManager->CreateFileReader( *Filename );

	if (!FileAr)			// No Files
		return;

	INT Revision;
	FString MapName, MapDesc, MapURL;	

	while ( !FileAr->AtEnd() )
	{
		*FileAr << Revision << MapName << MapDesc << MapURL;
		
		INT i;
		i = RLevel->AddZeroed();
		(*RLevel)(i) = Revision;

		i = MName->AddZeroed();
		(*MName)(i) = MapName;

		i = MDesc->AddZeroed();
		(*MDesc)(i) = MapDesc;

		i = MURL->AddZeroed();
		(*MURL)(i) = MapURL;
	}

	delete FileAr;
	FileAr = NULL;

	unguardexec;
}

void UGUIController::execSaveOwnageList(FFrame& Stack, RESULT_DECL )
{
	guard(GUIController::execSaveOwnageList);

	P_GET_TARRAY(OwnageMaps, FeOwnageMap);
	P_FINISH

	class FArchive* FileAr;
	FString Filename = TEXT("..\\system\\ownagemaps.dat");
	FileAr = GFileManager->CreateFileWriter( *Filename );

	if (!FileAr)			// Can't Create
		return;

	for (INT Index=0;Index<OwnageMaps.Num();Index++)
		*FileAr << OwnageMaps(Index).RLevel << OwnageMaps(Index).MapName << OwnageMaps(Index).MapDesc << OwnageMaps(Index).MapURL;

	delete FileAr;
	FileAr = NULL;

	unguardexec;
}

// returns a list of profiles on the disk
void UGUIController::execGetProfileList(FFrame& Stack, RESULT_DECL )
{
	guard(GUIController::execGetProfileList);

	P_GET_STR(Prefix);
	P_GET_TARRAY_REF(ProfileNames, FString);
	P_FINISH;

	TCHAR Wildcard[256]; 
	appSprintf( Wildcard, TEXT("*.uvx") );  // defined in xDataObject.cpp

	ProfileNames->Empty();

	for( INT DoCD=0; DoCD<1+(GCdPath[0]!=0); DoCD++ )
	{
		for( INT i=0; i<GSys->Paths.Num(); i++ )
		{
			if( appStrstr( *GSys->Paths(i), Wildcard ) )
			{
				TCHAR Tmp[256]=TEXT("");
				if( DoCD )
				{
					appStrcat( Tmp, GCdPath );
					appStrcat( Tmp, TEXT("System") PATH_SEPARATOR );
				}
				appStrcat( Tmp, *GSys->Paths(i) ); 
				*appStrstr( Tmp, Wildcard )=0;
				appStrcat( Tmp, *Prefix );
				appStrcat( Tmp, Wildcard );
				TArray<FString>	TheseNames = GFileManager->FindFiles(Tmp,1,0);
				for( INT j=0; j<TheseNames.Num(); j++ )
				{
					INT k=ProfileNames->AddZeroed();
					(*ProfileNames)(k) = TheseNames(j).Left(TheseNames(j).Len()-4);
				}
			}
		}
	}

	unguardexec;
}

void UGUIController::execSetRenderDevice( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIController::execSetRenderDevice);

	P_GET_STR(NewRenderClass);
	P_FINISH;

	GConfig->SetString( TEXT("Engine.Engine"), TEXT("RenderDevice"), *NewRenderClass );
	*(UBOOL*)Result = 1;

	unguardexec;
}

void UGUIController::execResetDesigner( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIController::execResetDesigner);

	P_FINISH;
	ResetDesigner();

	unguardexec;
}

void UGUIController::execGetCurrentRes(FFrame& Stack, RESULT_DECL )
{
	guard(GUIController::execGetCurrentRes);

	P_FINISH;
	*(FString*)Result = FString::Printf(TEXT("%ix%i"), ResX, ResY);

	unguardexec;
}

void UGUIController::execGetMainMenuClass(FFrame& Stack, RESULT_DECL )
{
	guard(GUIController::execMainMenuClass);

	P_FINISH;

	*(FString*)Result = *Cast<UGameEngine>(UGameEngine::StaticClass()->GetDefaultObject())->MainMenuClass;

	unguardexec;
}
void UGUIController::execGetMenuFont(FFrame& Stack, RESULT_DECL )
{
	guard(GUIController::execGetMenuFont);

	P_GET_STR(FontName);
	P_FINISH;

	if (FontStack.Num()==0)
	{
		*(UObject**) Result = NULL;
		return;
	}

	for (INT i=0;i<FontStack.Num();i++)
	{
		if ( FontName == FontStack(i)->KeyName )
		{
			*(UObject**) Result = FontStack(i);
			return;
		}
	}

	*(UObject**) Result = NULL;

	unguardexec;
}

void UGUIController::execGetStyle(FFrame& Stack, RESULT_DECL )
{
	guard(GUIController:execGetStyle);

	P_GET_STR(StyleName);
	P_GET_BYTE_REF(FontScale);
	P_FINISH;

	if (StyleName == TEXT("") || !StyleStack.Num())
		return;

	for (INT i=0;i<StyleStack.Num();i++)
	{
		if ( StyleName == StyleStack(i)->KeyName )
		{
			*(UObject**)Result = StyleStack(i);
			return;
		}

		else if (StyleName == StyleStack(i)->AlternateKeyName[0])
		{
			*(UObject**)Result = StyleStack(i);
			*FontScale = 0;
			return;
		}

		else if (StyleName == StyleStack(i)->AlternateKeyName[1])
		{
			*(UObject**)Result = StyleStack(i);
			*FontScale = 2;
			return;
		}
	}

	*(UObject**) Result = NULL;
	debugf(TEXT("Unknown style requested %s"), *StyleName);

	unguardexec;
}

// -- NOTE: The UGUIController always swallows input when it's active;
TCHAR* GetKeyStateName( BYTE State )
{
	switch(State)
	{
	case IST_Press: return TEXT("IST_Press");
	case IST_Hold:  return TEXT("IST_Hold");
	case IST_Release: return TEXT("IST_Release");
	}

	return TEXT("");
}

UBOOL UGUIController::NativeKeyEvent(BYTE& iKey, BYTE& State, FLOAT Delta )
{
	guard(UGUIController::NativeKeyEvent);

	if (Master==NULL || Master->Client==NULL || Master->Client->Viewports.Num()==0 || Master->Client->Viewports(0)==NULL)
		return false;

//	if ( State != IST_Axis )
//		debugf(TEXT("NativeKeyEvent %s %s"), Cast<UViewport>(ViewportOwner)->Input->GetKeyName( EInputKey(iKey) ), GetKeyStateName(State));

	if ( !GIsEditor && DELEGATE_IS_SET(OnNeedRawKeyPress) )
	{
		if (State==IST_Release)
			delegateOnNeedRawKeyPress(iKey);

		return true;
	}

	if ( !GIsEditor && iKey==IK_Enter && AltPressed)
	{
		ResetInput();
		return true;
	}

	// Make sure the mouse is within bounds
	if ( (!ActiveControl || !ActiveControl->bCaptureMouse) && (iKey == IK_LeftMouse || iKey == IK_RightMouse || (iKey >= IK_Mouse4 && iKey <= IK_MouseZ)) )
	{
		INT ptX, ptY;
		GetCursorPos( ptX, ptY );
		if ( ptX > ResX || ptY > ResY || ptX < 0 || ptY < 0 )
		{
			// Out of bounds
			ResetInput();
			return true;
		}
	}

	// Do we need to switch to the console?
	if (!GIsEditor && ViewportOwner!=NULL && ViewportOwner->Console!=NULL)
	{
		BYTE ConsoleHotKey = Cast<UConsole>(ViewportOwner->Console)->ConsoleHotKey;

		if (iKey==ConsoleHotKey && State==IST_Release)
		{

			if ( MouseOver )
			{
				MouseOver->delegateLeaveArea();
				MouseOver = NULL;
			}

			// Notify context menu
			if ( ContextMenu && !ContextMenu->Close() )
				return true;

			if (Cast<UConsole>(ViewportOwner->Console) != NULL)
				Cast<UConsole>(ViewportOwner->Console)->eventNativeConsoleOpen();

			return true;
		}
	
	}

	// Update our array of pressed keys & handle key repeats
	if ( State == IST_Press )
	{
		KeyDown[iKey] = 1;
		if ( iKey != RepeatKey )
		{
			RepeatKey = iKey;
			RepeatTime = ButtonRepeatDelay * 3;
			RepeatDelta = Delta;
		}

		// Support for screenshots of the menu - but hardcode the command, just to be safe ;)
		UViewport* Viewport = Cast<UViewport>(ViewportOwner);
		if ( Viewport && Viewport->Input )
		{
			if ( Viewport->Input->IsBoundTo(TEXT("SHOT"), iKey) )
			{
				Viewport->Exec(TEXT("SHOT"), *GLog);
				return true;
			}

			if ( Viewport->Input->IsBoundTo(TEXT("MUSICMENU"), iKey) )
			{
				Viewport->Exec(TEXT("MUSICMENU"), *GLog);
				return true;
			}
		}
	}
	else if ( State == IST_Release )
	{
		// We didn't receive the press
		if ( KeyDown[iKey] != 1 )
		{
//			debugf(TEXT("Ignoring errant release for key %s"), Cast<UViewport>(ViewportOwner)->Input->GetKeyName(EInputKey(iKey)));
			return true;
		}

		KeyDown[iKey] = 0;
		if ( iKey == RepeatKey )
		{
			RepeatKey = 0;
			RepeatTime = 0;
			RepeatDelta = 0;
		}
	}

	ShiftPressed = KeyDown[IK_Shift];
	AltPressed = KeyDown[IK_Alt];
	CtrlPressed = KeyDown[IK_Ctrl];

	// If we are in design mode, allow components to be moved around
	if ( bModAuthor && DesignKeyEvent(iKey, State, Delta) )
		return true;

	// The UGUIControl automatically tracks Mouse X/Y changes
	if ( iKey==IK_MouseX || iKey==IK_MouseY || bForceMouseCheck )
	{
		bForceMouseCheck = false;

		FLOAT dX=0.0f;
		FLOAT dY=0.0f;

		if (iKey==IK_MouseX)
		{
			dX = Delta;
			MouseX += (dX*MenuMouseSens);
		}
		else 
		{
			dY = Delta;
			MouseY -= (dY * MenuMouseSens);
		}

		// As long as we are not capturing the mouse, make sure the Active Control is up to date
		LookUnderCursor(dX,dY);
		if ( iKey == IK_MouseX || iKey == IK_MouseY )
			return true;
	}
	else if ( !FocusedControl || !FocusedControl->bFocusOnWatch )
		LookUnderCursor(0.f,0.f);

	if ( GIsEditor )
		return 0;

	if ( MouseOver && MouseOver->bTrackInput && MouseOver->bVisible && MouseOver->delegateLeaveArea() )
		MouseOver = NULL;

	if ( ContextMenu && ContextMenu->KeyEvent(iKey,State,Delta) )
		return true;

	if (iKey==IK_RightMouse && State==IST_Release && ActiveControl != NULL && DropSource == NULL )
	{
		guard(UGUIController::NativeKeyEvent::RightMouseClick);

		UGUIComponent* Who = ActiveControl;
		UGUIComponent* AC = ActiveControl;
		while (Who!=NULL)
		{
			if ( OBJ_DELEGATE_IS_SET(Who,OnRightClick) )
			{
				if ( !Who->delegateOnRightClick(Who) )
				{
					Who = Who->MenuOwner;
					continue;
				}
			}

			if (Who->ContextMenu!=NULL)
			{
				LastClickX=-1;
				LastClickY=-1;
				LastClickTime=0.0f;

				if (ContextMenu)
				{
					ContextMenu->Close();
					LookUnderCursor(0.f, 0.f);
				}

				// Never play click sound on Right Click
				BYTE TempSound = 0;
				if (AC->OnClickSound)
				{
					TempSound = AC->OnClickSound;
					AC->OnClickSound = 0;
				}

				if ( bIgnoreNextRelease )
					bIgnoreNextRelease = false;
				else MouseReleased();

				if (TempSound)
					AC->OnClickSound = TempSound;

				if ( OBJ_DELEGATE_IS_SET(Who->ContextMenu,OnOpen) )
				{
					if ( !Who->ContextMenu->delegateOnOpen(Who->ContextMenu) )
					{
						Who = Who->MenuOwner;
						continue;
					}
				}

				// Eliminate any mouse-over hints that are displaying
				if ( MouseOver )
				{
					MouseOver->delegateLeaveArea();
					MouseOver = NULL;
				}

				ContextMenu = Who->ContextMenu;
				ContextMenu->MenuOwner = Who;
				ContextMenu->Controller = this;
				ContextMenu->WinLeft = MouseX;
				ContextMenu->WinTop = MouseY;
				ContextMenu->Style = eventGetStyle(ContextMenu->StyleName, ContextMenu->FontScale);
				ContextMenu->SelectionStyle = eventGetStyle(ContextMenu->SelectionStyleName, ContextMenu->FontScale);
				ContextMenu->ItemIndex=-1;

				return true;
			}
			Who = Who->MenuOwner;
		}

		return true;

		unguard;
	}

	if (iKey==IK_LeftMouse)
	{
		guard(UGUIController::NativeKeyEvent::LeftMouseClick);
		if (ContextMenu)
		{
			ContextMenu->Close();
			LookUnderCursor(0.f,0.f);
		}

		if (State==IST_Press)			// Process Mouse Presses
		{
			MousePressed(0);
		}
		else if (State==IST_Hold)		// Process Mouse Repeat
		{
			if (!bIgnoreNextRelease)
				MousePressed(1);
		}
		else if (State==IST_Release)	// Process Mouse Release
		{
			if (!bIgnoreNextRelease)
			{
				MouseReleased();
			}
			bIgnoreNextRelease=false;
		}

		// @@Note: Add code to handle double-click here

		return true;
		unguard;
	}


	// Pass key events along to the pages.  Start with the active page, then
	// proceed upward in the stack

	if ( ActivePage )
	{
		if ( ActivePage->NativeKeyEvent(iKey, State, Delta) || ActivePage->bCaptureInput )
		{
			if (ContextMenu)
			{
				ContextMenu->Close();
				LookUnderCursor(0.f,0.f);
			}

			return true;
		}
	}

	// The Active Page didn't swallow it.. 		pass it along
	UGUIPage* page = NULL;
	for (INT i = MenuStack.Num() - 1; i >= 0; i--)
	{
		page = MenuStack(i);
		if ( page && page != ActivePage )
		{
			if ( page->NativeKeyEvent(iKey, State, Delta) )
			{
				if (ContextMenu)
				{
					ContextMenu->Close();
					LookUnderCursor(0.f,0.f);
				}

				return true;
			}

			if ( page->bCaptureInput )
				break;
		}
	}

	if (ContextMenu)
	{
		ContextMenu->Close();
		LookUnderCursor(0.f,0.f);
	}
	
	return true;

	unguard;
}
					   
UBOOL UGUIController::NativeKeyType(BYTE& iKey, TCHAR Unicode )
{

	guard(GUIController::NativeKeyType);

	RepeatKey=0;
	RepeatTime=0;
	RepeatDelta=0.0f;

	UGUIPage* page = ActivePage;
	if ( !page )
		return true;

	if ( page->NativeKeyType(iKey, Unicode) || page->bCaptureInput )
		return true;

	// The Active Page didn't swallow it.. pass it along
	for (INT i = MenuStack.Num() - 1; i>=0; i--)
	{
		page = MenuStack(i);
		if ( page && page != ActivePage && (page->NativeKeyType(iKey,Unicode) || page->bCaptureInput) )
			break;
	}

	return true;

	unguard;
}

// The Mouse Button was pressed

UBOOL UGUIController::MousePressed(UBOOL IsRepeat)
{
	guard(UGUIController::MousePressed);

	bIgnoreUntilPress = false;
	
	if ( !bCurMenuInitialized )
		return false;

	if ( ActiveControl && (ActiveControl->bDropSource || 
						  (ActiveControl->bCaptureMouse && ActiveControl->PageOwner == ActivePage)) )
	{
		ActiveControl->MousePressed(IsRepeat);
		return true;
	}

	UGUIPage* page = ActivePage;
	if ( page && (page->MousePressed(IsRepeat) || page->bCaptureInput) )
		return true;

	for (INT i = MenuStack.Num() - 1; i >= 0; i--)
	{
		page = MenuStack(i);
		if ( page && page != ActivePage )
		{
			if ( page->MousePressed(IsRepeat) )
				return true;

			if ( page->bCaptureInput )
				break;
		}
	}

	return false;

	unguard;

}

// The Mouse Button was released

UBOOL UGUIController::MouseReleased()
{
	guard(UGUIController::MouseReleased);

	if ( !bCurMenuInitialized || bIgnoreUntilPress )
		return false;

	if (DropSource )
	{
		UGUIComponent* DT = DropTarget, *DS = DropSource;
		if ( DropTarget)
		{

			// Find out if the target wants to accept the data
			// In DropStateChange(), DropTarget will call OnEndDrag() on the drop source with the appropriate result
			if (DT->delegateOnDragDrop(DT))
				DT->eventDropStateChange(DRP_Accept);
			else DT->eventDropStateChange(DRP_Reject);

			if ( DS != DT && DT->MouseReleased() && OBJ_DELEGATE_IS_SET(DT,OnMouseRelease) )
				DT->delegateOnMouseRelease(DT);

			// DropSource->MouseReleased() only returns false if it doesn't have a Controller
			// Otherwise, it calls OnMouseRelease() itself.
			if ( !DS->MouseReleased() && OBJ_DELEGATE_IS_SET(DS,OnMouseRelease) )
				DS->delegateOnMouseRelease(DS);

			DS->eventDropStateChange(DRP_None);
			return true;
		}

		// If we are here, it means we don't have a DropTarget.  Send the reject notification.
		if ( DropSource )
		{
			DS = DropSource;
			if ( OBJ_DELEGATE_IS_SET(DropSource, OnEndDrag) )
				DS->delegateOnEndDrag(NULL, 0);
 
			// Notify the DropSource of the mouse release (DropSource will change it's MenuState and call OnMouseRelease())
			if ( !DS->MouseReleased() && OBJ_DELEGATE_IS_SET(DS,OnMouseRelease) )
				DS->delegateOnMouseRelease(DropSource);

			DS->eventDropStateChange(DRP_None);
			return true;
		}
	}

	if (ActiveControl && ActiveControl->MenuState == MSAT_Pressed && ActiveControl->PageOwner == ActivePage )
	{
		UGUIComponent* C = ActiveControl;
		if ( !C->IsAnimating() && (C->MouseReleased()||C->bCaptureMouse) )
			return true;
	}

	UGUIPage* page = ActivePage;
	if ( page && (page->MouseReleased()||page->bCaptureInput) )
		return true;

	for ( INT i = MenuStack.Num() - 1; i >= 0; i-- )
	{
		page = MenuStack(i);
		if ( page && page != ActivePage )
		{
			if ( page->MouseReleased() || page->bCaptureInput )
				return true;
		}
	}

	return false;

	unguard;
}

void UGUIController::execSetMoveControl( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIController::execSetMoveControl);

	P_GET_OBJECT(UGUIComponent,C);
	P_FINISH;

	SetMoveControl(C);
	unguardexec;
}

void UGUIController::SetMoveControl( UGUIComponent* C )
{
	guard(UGUIController::SetMoveControl);

	MoveControl = C;
	if ( MoveControl )
		ShowProperties(MoveControl);

	unguard;
}


void UGUIController::CreatePropertyWindow()
{
	guard(UGUIController::CreatePropertyWindow);

	if ( DesignerMenu == TEXT("") || !Master || !Master->Client || !Master->Client->Viewports.Num() )
		return;

#ifdef WIN32
	UViewport* Port = Master->Client->Viewports(0);
	Port->Exec(TEXT("ENDFULLSCREEN"));

	if ( !Designer )
	{
		UClass* ManagerClass = LoadClass<UPropertyManagerBase>(NULL, *DesignerMenu, NULL, 0, NULL);
		if ( ManagerClass )
			Designer = (PTRINT)ConstructObject<UPropertyManagerBase>(ManagerClass);
	}

	if ( Designer )
	{
		((UPropertyManagerBase*)Designer)->SetWindow(Port->GetWindow());
		((UPropertyManagerBase*)Designer)->SetParent(this);
	}

#else
	debugf(TEXT("GUIDesigner is only supported on Windows!"));
#endif

	unguard;
}

void UGUIController::HidePropertyWindow()
{
	guard(UGUIController::HidePropertyWindow);

	if ( Designer )
	{
		((UPropertyManagerBase*)Designer)->SetCurrent(NULL);
		((UPropertyManagerBase*)Designer)->Show(0);
	}

	unguard;
}

void UGUIController::ResetDesigner()
{
	guard(UGUIController::ResetDesigner);
 	if ( Designer )
	{
		HidePropertyWindow();
		delete ((UPropertyManagerBase*)Designer);
	}

	Designer = NULL;
	unguard;
}

UBOOL UGUIController::DesignerVisible()
{
	guard(UGUIController::DesignerVisible);

	if ( !Designer )
		return 0;

	return ((UPropertyManagerBase*)Designer)->IsVisible();
	unguard;
}

UBOOL UGUIController::ShowProperties(UGUIComponent* Comp )
{
	guard(UGUIController::ShowProperties);

	if ( !Designer && DesignerMenu != TEXT("") )
		CreatePropertyWindow();

	if ( Designer )
	{
		if ( Comp )
			((UPropertyManagerBase*)Designer)->SetCurrent( (UObject**)&Comp );
		else ((UPropertyManagerBase*)Designer)->SetCurrent(NULL);
		RepeatKey = 0;
	}

	return Designer != NULL;

	unguard;
}


void UGUIController::Destroy()
{
	guard(UGUIController::Destroy);

	ResetDesigner();
	if ( Designer )
		delete ((UPropertyManagerBase*)Designer);

	Designer = NULL;

	Super::Destroy();

	unguard;
}

UGUIComponent* UGUIController::UnderCursor(FLOAT MouseX, FLOAT MouseY)
{
	guard(UGUIComponent::UnderCursor);

	UGUIComponent* Under = NULL;

	if ( !Under && ActivePage )
		Under = ActivePage->UnderCursor(MouseX,MouseY);

	if ( !Under )
	{
		for ( INT i = MenuStack.Num() - 1; i >= 0; i-- )
		{
			if ( MenuStack(i) != ActivePage )
			{
				Under = MenuStack(i)->UnderCursor(MouseX,MouseY);
				if ( Under )
					break;
			}
		}
	}

	return Under;

	unguard;
}

void UGUIController::PlayComponentSound(BYTE SoundNum)
{
	USound* s = NULL;

	switch(SoundNum)
	{
	case CS_Click:
		s = ClickSound;
		break;
	case CS_Edit:
		s = EditSound;
		break;
	case CS_Up:
		s = UpSound;
		break;
	case CS_Down:
		s = DownSound;
		break;
	case CS_Drag:
		s = DragSound;
		break;
	case CS_Fade:
		s = FadeSound;
		break;
	case CS_Hover:
		s = MouseOverSound;
		break;
	}

	if(!s)
		return;

	PlayInterfaceSound(s);
}

void UGUIController::execPlayInterfaceSound( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIController::execPlayInterfaceSound);

	P_GET_BYTE(SoundType);
	P_FINISH;

	PlayComponentSound(SoundType);
	unguardexec;
}

void UGUIController::PlayInterfaceSound(USound*  sound)
{
	guard(UGUIController::PlayInterfaceSound);

	if (sound==NULL || bQuietMenu)
		return;

	APlayerController* pc = ViewportOwner->Actor;
	if( pc && pc->GetLevel() && pc->GetLevel()->Engine && pc->GetLevel()->Engine->Audio )
		pc->GetLevel()->Engine->Audio->PlaySound( NULL, SLOT_Interface | 0x01, sound, FVector(0, 0, 0), 1.0, 4096.0, 1.0, SF_No3D, 0.f );

	unguard;
}

void UGUIController::LookUnderCursor(FLOAT dX, FLOAT dY)
{
	guard(UGUIController::LookUnderCursor);

	if ( ContextMenu )
		return;

	UGUIComponent* Under = UnderCursor(MouseX, MouseY);
	if (DropSource)
	{
		// If we are over a component, and it isn't the DropSource
		if (Under)
		{
			// If the current component is a valid drop target
			if (Under->bDropTarget)	// TODO may need to support checking menuowner's for drop status
			{

				// If the component is currently in the DRP_Target state
				if (DropTarget)
				{
					// If the current component isn't our drop target, release the old droptarget and make this one the new target
					if (DropTarget != Under)
						DropTarget->eventDropStateChange(DRP_None);
				}

				// This component is already the drop source...support moving drag-n-drop within single component
				// DropState remains DRP_Source, so don't call DropStateChange(DRP_Target)
				if ( DropSource == Under )
				{
					if ( DropTarget != Under )
						Under->delegateOnDragEnter(Under);

					DropTarget = Under;
				}

				else if ( DropTarget != Under )
					Under->eventDropStateChange(DRP_Target);
			}

			// We are mousing over a component that cannot be a target.  If we have one, release it
			else if (DropTarget)
				DropTarget->eventDropStateChange(DRP_None);
		}

		else if (DropTarget)	// we have a drop target, but we aren't over anything - release the drop target
			DropTarget->eventDropStateChange(DRP_None);

		if (Under && OBJ_DELEGATE_IS_SET(Under, OnDragOver))
			Under->delegateOnDragOver(Under);

		// If we are currently performing a drag-n-drop operation, stop here.
		return;
	}

	// Handle moving over new control
	if ( ActiveControl ) 
	{
		UGUIComponent* C = ActiveControl;
		if ( C->MenuState == MSAT_Pressed && (C->bCaptureMouse || C->bDropSource) )
		{
			C->MouseMove(dX, dY);
			return;
		}
		else
		{
			// Mousing over a new component
			if ( C != Under) 
			{
				if ( C->MenuState == MSAT_Watched )
					C->eventMenuStateChange(ActiveControl->LastMenuState);

				LastClickTime = 0.f;
				if ( C->delegateOnEndTooltip() )
					MouseOver = NULL;
			}
		}
	}
		
	if ( Under )
	{
		if ( Under->MenuState == MSAT_Blurry )
		{
			Under->eventMenuStateChange(MSAT_Watched);

			// Mouse over sound
			if( Under->bMouseOverSound && ActiveControl != Under )
				PlayComponentSound(CS_Hover);
		}

		if ( MouseOver == NULL && Under != ActiveControl && eventCanShowHints() )
			MouseOver = Under->delegateOnBeginTooltip();

		// Call MouseHover on the thing the mouse is on.
		Under->MouseHover();
		ActiveControl = Under;
	}

	unguard;
}

void UGUIController::ResolutionChanged( INT NewX, INT NewY )
{
	guard(UGUIController::ResolutionChanged);

	ResX = NewX;
	ResY = NewY;
	ViewportOwner->bShowWindowsMouse = bActive && ViewportOwner->bWindowsMouseAvailable;

	UGUIPage* page = NULL;
	for ( INT i = 0; i < MenuStack.Num(); i++ )
	{
		page = MenuStack(i);
		if ( page && !page->bPersistent )
			page->NotifyResolutionChange(NewX, NewY);
	}

	for ( INT i = 0; i < PersistentStack.Num(); i++ )
	{
		page = PersistentStack(i);
		if ( page )
			page->NotifyResolutionChange(NewX, NewY);
	}

	unguard;
}

UBOOL UGUIController::HasMouseMoved( FLOAT ErrorMargin )
{
	return Abs(MouseX - LastMouseX) > Abs(ErrorMargin) || Abs(MouseY - LastMouseY) > Abs(ErrorMargin);
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIFonts 
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIFont::execGetFont(FFrame& Stack, RESULT_DECL )
{
	guard(GUIFont::execGetFont);

	P_GET_INT(XRes);
	P_FINISH;
	
	INT Index;
	FLOAT ScaleFactor;

	// If it's a scaled font, always use the first font.

	if (bScaled)
	{
		// Handle fallbacks

		if ( XRes <= FallBackRes )
		{
			Index = 1;
			ScaleFactor = 1.0f;
		}
		else
		{
			Index = 0;
			ScaleFactor = float(XRes) / float(NormalXRes); 
		}
	}
	else
	{
		if ( (XRes<800) || (bFixedSize) )
			Index = 0;
		else if (XRes<1024)
			Index = 1;
		else if (XRes<1280)
			Index = 2;
		else if (XRes<1600)
			Index = 3;
		else
			Index = 4;

		ScaleFactor = 1.0f;
	}

	Index = Min<INT>(Index,FontArrayNames.Num()-1);

	if( Index < 0)
		*(UObject**) Result = NULL;

	if( Index >= FontArrayFonts.Num() )
		FontArrayFonts.AddZeroed(1+Index-FontArrayFonts.Num());

	if( FontArrayFonts(Index) == NULL )
	{
		FontArrayFonts(Index) = Cast<UFont>( StaticLoadObject( UFont::StaticClass(), NULL, *FontArrayNames(Index), NULL, LOAD_NoWarn, NULL ) );
		if( FontArrayFonts(Index) == NULL )
		{
			GWarn->Logf(TEXT("Warning: %s could not load font %s"), GetName(), *FontArrayNames(Index) );
			return;
		}
	}
	FontArrayFonts(Index)->ScaleFactor = 1.0f;
	*(UObject**) Result = FontArrayFonts(Index);
	FontArrayFonts(Index)->ScaleFactor=ScaleFactor;

	unguardexec;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIStyles
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIStyles::execTextSize(FFrame& Stack, RESULT_DECL )	// UScript stubs
{
	guard(UGUIStyles::execTextSize);

	P_GET_OBJECT(UCanvas, pCanvas);
	P_GET_BYTE(State);
	P_GET_STR(Text);
	P_GET_FLOAT_REF(XL);
	P_GET_FLOAT_REF(YL);
	P_GET_BYTE(FontScale);
	P_FINISH;
	
	TextSize(pCanvas,State,*Text,*XL,*YL, FontScale);

	unguardexec;
}


void UGUIStyles::execDraw(FFrame& Stack, RESULT_DECL )	// UScript stubs
{
	guard(UGUIStyles::execDraw);

	P_GET_OBJECT(UCanvas, pCanvas);
	P_GET_BYTE(State);
	P_GET_FLOAT(Left);
	P_GET_FLOAT(Top);
	P_GET_FLOAT(Width);
	P_GET_FLOAT(Height);
	P_FINISH;

	Draw(pCanvas,State,Left,Top,Width,Height);

	unguardexec;
}

void UGUIStyles::execDrawText(FFrame& Stack, RESULT_DECL ) // UScript Stubs
{
	guard(UGUIStyles::execDrawText);

	P_GET_OBJECT(UCanvas, pCanvas);
	P_GET_BYTE(State);
	P_GET_FLOAT(Left);
	P_GET_FLOAT(Top);
	P_GET_FLOAT(Width);
	P_GET_FLOAT(Height);
	P_GET_BYTE(Just);
	P_GET_STR(Text);
	P_GET_BYTE(FontScale);
	P_FINISH;

	DrawText(pCanvas,State,Left,Top,Width,Height,Just, *Text, FontScale);

	unguardexec;
}


// Draw outputs the Style object to the canvas using the current menu state

void UGUIStyles::Draw(UCanvas* Canvas, BYTE MenuState, FLOAT Left, FLOAT Top, FLOAT Width, FLOAT Height)
{

	guard(UGUIStyles::Draw);

	if ( Canvas == NULL || (MenuState<0) || (MenuState>5) ) 
		return;

	SaveCanvasState(Canvas);

#if UCONST_Counter == 0
	if ( DELEGATE_IS_SET(OnDraw) && delegateOnDraw(Canvas,MenuState,Left,Top,Width,Height) )
#else
	if ( delegateOnDraw(Canvas, MenuState, Left, Top, Width, Height) )
#endif
	{
		RestoreCanvasState(Canvas);
		return;
	}

	UMaterial* Image = Images[MenuState];	
	FLOAT mW, mH;

	if ( Image == NULL )
		return;

	if (ImgWidths[MenuState]>=0)
		mW = ImgWidths[MenuState];
	else
		mW =Image->MaterialUSize();

	if (ImgHeights[MenuState]>=0)
		mH = ImgHeights[MenuState];
	else
		mH = Image->MaterialVSize();
	
	Canvas->Style = RStyles[MenuState];
	Canvas->Color = ImgColors[MenuState];

	switch (ImgStyle[MenuState])
	{
		case ISTY_Normal:
			Canvas->DrawTile(Image,Left,Top,mW,mH,0,0,mW,mH,0,FColor(255,255,255,255),FPlane(0,0,0,0)); break;
		case ISTY_Tiled:
			Canvas->DrawTile(Image,Left,Top,Width,Height,0,0,Width,Height,0,FColor(255,255,255,255),FPlane(0,0,0,0)); break;
		case ISTY_Stretched: 
			Canvas->DrawTileStretched(Image, Left,Top, Width, Height); break;
		case ISTY_Bound:
			Canvas->DrawTileBound(Image, Left, Top, Width, Height); break;
		case ISTY_Scaled:
				Canvas->DrawTileScaleBound(Image, Left, Top, Width, Height);break;
		case ISTY_Justified:
			Canvas->DrawTileJustified(Image, Left, Top, Width, Height, IMGA_Center ); break;
		case ISTY_PartialScaled:
			Canvas->DrawTileStretchedOrScaled(Image, Left, Top, Width, Height, BorderOffsets[0], BorderOffsets[1]); break;
	}

	RestoreCanvasState(Canvas);

	unguardobj;
}

// Draw Text outputs text of the current style to the canvas using the current menu state
void UGUIStyles::DrawText(UCanvas* Canvas, BYTE MenuState, FLOAT Left, FLOAT Top, FLOAT Width, FLOAT Height, BYTE Just, const TCHAR* Text, BYTE FontScale)
{
	guard(UGUIStyles::DrawText);

	if ( Canvas == NULL || (MenuState<0) || (MenuState>5) || (Fonts[MenuState+(5*FontScale)]==NULL) ) 
		return;

	SaveCanvasState(Canvas);

//	FontScale=0;  ??

	// Snap location and size to integer amounts in screen space.
	Left = appRound(Left);
	Top = appRound(Top);
	Width = appRound(Width);
	Height = appRound(Height);

	if ( DELEGATE_IS_SET(OnDrawText) && delegateOnDrawText(Canvas,MenuState,Left,Top,Width,Height,Just,FString::Printf(TEXT("%s"), Text),FontScale) )
	{
		RestoreCanvasState(Canvas);
		return;
	}

	INT FontClipX = Canvas->SizeX;
	Canvas->Style	= RStyles[MenuState];
	Canvas->Color	= FontColors[MenuState];

	if (MenuState==MSAT_Disabled)
		Canvas->ColorModulate *= FPlane(0.5,0.5,0.5,1.0);

	Canvas->Font = Fonts[MenuState+(5*FontScale)]->eventGetFont(FontClipX);
	if (Canvas->Font!=NULL)
		Canvas->DrawTextJustified(Just, Left, Top, Left+Width, Top+Height, TEXT("%s"), Text);

	RestoreCanvasState(Canvas);
	unguardobj;
}

void UGUIStyles::TextSize(UCanvas* Canvas, BYTE MenuState, const TCHAR* Test, FLOAT& XL, FLOAT& YL, BYTE FontScale)
{
	guard(UGUIStyles::TextSize);

	if ( Canvas == NULL || (MenuState<0) || (MenuState>5) || (Fonts[MenuState+(5*FontScale)]==NULL) ) 
		return;

	INT FontClipX = Canvas->SizeX;
	if ( Fonts[MenuState + (5 * FontScale)] )
		Canvas->Font	= Fonts[MenuState+(5*FontScale)]->eventGetFont(FontClipX);
	
	if (Canvas->Font!=NULL)
	{
		if (*Test == '\0')
		{
			Canvas->ClippedStrLen( Canvas->Font, 1.0, 1.0, XL, YL, TEXT("W") );

			XL = 0;
		}
		else
			Canvas->ClippedStrLen( Canvas->Font,1.0,1.0,XL,YL, Test);
	}

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIComponent
// =======================================================================================================================================================
// =======================================================================================================================================================

// PreDraw - Updates the Bounding box depending on resolution
void UGUIComponent::PreDraw(UCanvas* Canvas)
{
	guard(GUIComponent::PreDraw);

	if ( INVALIDRENDER )
		return;

	UGUIController* C = Controller;
#if UCONST_Counter
	PreDrawCount++;
	if ( PreDrawCount > UCONST_Counter )
		debugf(TEXT("PreDraw called %i times: %s"), PreDrawCount, GetMenuPath());
#endif

	// Animate
	if ( MotionFrame.Num() )
	{
		// Grab the current frame
		FVector& Dest = MotionFrame(0);
		bTravelling = Dest.Z > 0.f;

		if ( bTravelling )	// If we're not there yet
		{
			if ( WinLeft != Dest.X )
			{
				FLOAT Result = WinLeft + ((Dest.X - WinLeft) * (C->RenderDelta/Dest.Z));

				// Result should always be between the current position and the destination position
				WinLeft = Clamp<FLOAT>( Result,
					Min<FLOAT>(WinLeft, Dest.X), 
					Max<FLOAT>(WinLeft, Dest.X) );
			}

			if (WinTop != Dest.Y)
			{
				FLOAT Result = WinTop + ((Dest.Y - WinTop) * (C->RenderDelta/Dest.Z));

				// Result should always be between the current position and the destination position
				WinTop = Clamp<FLOAT>( Result,
					Min(WinTop, Dest.Y),
					Max(WinTop, Dest.Y) );
			}

			// Update the counter
			Dest.Z -= C->RenderDelta;
		}

		else
		{
			WinLeft = Dest.X;
			WinTop = Dest.Y;

			// Send notification that we've arrived at this keyframe
			if ( DELEGATE_IS_SET(OnArrival) )
				delegateOnArrival(this,AT_Position);

			MotionFrame.Remove(0);

			// If there aren't anymore frames, send end anim notification
			if ( MotionFrame.Num() < 1 )
				eventEndAnimation(this,AT_Position);
		}

	}

	if ( SizeFrame.Num() )
	{
		// Grab the current frame
		FVector& Dest = SizeFrame(0);
		bSizing = Dest.Z > 0.f;

		if ( bSizing )
		{
			if ( WinWidth != Dest.X )
			{
				FLOAT Result = WinWidth + ((Dest.X - WinWidth) * (C->RenderDelta/Dest.Z));

				// Result should always be between the current position and the destination position
				WinWidth = Clamp<FLOAT>( Result,
					Min<FLOAT>( WinWidth, Dest.X ),
					Max<FLOAT>( WinWidth, Dest.X ) );
			}

			if ( WinHeight != Dest.Y )
			{
				FLOAT Result = WinHeight + ((Dest.Y - WinHeight) * (C->RenderDelta/Dest.Z));

				// Result should always be between the current position and the destination position
				WinHeight = Clamp<FLOAT>( Result,
					Min<FLOAT>( WinHeight, Dest.Y ),
					Max<FLOAT>( WinHeight, Dest.Y ) );
			}

			// Update the counter
			Dest.Z -= C->RenderDelta;
		}
		else
		{
			WinWidth = Dest.X;
			WinHeight = Dest.Y;

			// Send notification that we've arrived at this keyframe
			if ( DELEGATE_IS_SET(OnArrival) )
				delegateOnArrival(this, AT_Dimension);

			SizeFrame.Remove(0);

			// If there aren't anymore frames, send end anim notification
			if ( SizeFrame.Num() < 1 )
				eventEndAnimation(this,AT_Dimension);
		}

	}

	UpdateBounds();
#if UCONST_Counter == 0
	if( DELEGATE_IS_SET(OnPreDraw) && delegateOnPreDraw(Canvas) )
#else
	if ( delegateOnPreDraw(Canvas) )
#endif
		UpdateBounds();

	unguardobj;
}

// Draw should always be subclassed in the individual components, and they should ALWAYS call their super first to
// obtain their state.

void UGUIComponent::Draw(UCanvas* Canvas)
{
	guard(GUIComponent::Draw);

	if ( !bVisible || INVALIDRENDER )
		return;
#if UCONST_Counter
	DrawCount++;
	if ( DrawCount > UCONST_Counter )
		debugf(TEXT("Draw called %i times: %s"), DrawCount, GetMenuPath());
#endif
	Canvas->CurX = ActualLeft();
	Canvas->CurY = ActualTop();

	if (Style!=NULL)
	{
		Canvas->Color = Style->FontColors[MenuState];
		Canvas->Style = Style->RStyles[MenuState];

		if (Style->Fonts[MenuState+(5*FontScale)]!=NULL)
			Canvas->Font  = Style->Fonts[MenuState+(5*FontScale)]->eventGetFont(Controller->ResX);
	}

#if UCONST_Counter == 0
	if ( DELEGATE_IS_SET(OnRender) )
#endif
		delegateOnRender(Canvas);

	unguardobj;
}

void UGUIComponent::execGetMenuPath( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIComponent::execGetMenuPath);

	P_FINISH;

	*(FString*)Result = GetMenuPath();

	unguardexec;
}

TCHAR* UGUIComponent::GetMenuPath( TCHAR* Str ) const
{
	guard(UGUIComponent::GetMenuPath);

	TCHAR* Result = Str ? Str : appStaticString1024();
	*Result = 0;
	if( MenuOwner && !Cast<UGUIPage>(MenuOwner) )
	{
		MenuOwner->GetMenuPath( Result );
		appStrncat(Result, TEXT("->"), 1024);
	}
	appStrncat( Result, GetName(), 1024 );

	return Result;
	unguardobj;
}

void UGUIComponent::UpdateBounds()
{
	guard(GUIComponent::UpdateBounds);

	// Calculate the bounds
	Bounds[0] = ActualLeft();
	Bounds[1] = ActualTop();
	Bounds[2] = ActualLeft()+ActualWidth();
	Bounds[3] = ActualTop()+ActualHeight();

	if (Style!=NULL)
	{
		ClientBounds[0] = Bounds[0]+Style->BorderOffsets[0];
		ClientBounds[1] = Bounds[1]+Style->BorderOffsets[1];
		ClientBounds[2] = Bounds[2]-Style->BorderOffsets[2];
		ClientBounds[3] = Bounds[3]-Style->BorderOffsets[3];
	}
	else
	{
		ClientBounds[0] = Bounds[0];
		ClientBounds[1] = Bounds[1];
		ClientBounds[2] = Bounds[2];
		ClientBounds[3] = Bounds[3];
	}

	bPositioned = 1;
	unguardobj;
}

void UGUIComponent::execPlayerOwner( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIComponent::execPlayerOwner);

	P_FINISH;

	if ( !Controller || !Controller->ViewportOwner )
		return;

	*(UObject**)Result = Controller->ViewportOwner->Actor;
	unguardexec;
}

// PerformHitTest checks the current Mouse X/Y position to see if it's touching this
// component.  

UBOOL UGUIComponent::PerformHitTest(INT MouseX, INT MouseY)
{

	guard(GUIComponent::PerformHitTest);

	// Not accepting input, disabled, or animating, return false
	if ( !bVisible || !bAcceptsInput || MenuState == MSAT_Disabled || bAnimating )	// If never accepting input, leave this
		return false;

	if (DELEGATE_IS_SET(OnHitTest) )
		delegateOnHitTest(MouseX,MouseY);

	// Perform simple box collision
	return WithinBounds(MouseX, MouseY);

	unguardobj;

}

// Returns the actual width (including scaling) of a component
FLOAT UGUIComponent::ActualWidth( FLOAT Val, UBOOL bForce ) const
{
	guard(UGUIComponent::ActualWidth);

	if ( Val == INDEX_NONE )
		Val = WinWidth;

	if ( !Controller )
		return 0.0;

	if ( Val < 2.f && !(bNeverScale || bForce) )
	{
		if ( (bScaleToParent) && (MenuOwner!=NULL) && (ScalingType == SCALE_All || ScalingType == SCALE_X) )
			return MenuOwner->ActualWidth() * Val;
		else
			return Controller->ResX * Val;
	}
	else
		return Val;	

	unguardobj;
}

void UGUIComponent::execActualWidth(FFrame& Stack, RESULT_DECL )
{
	guard(GUIComponent::execActualWidth);
	P_GET_FLOAT_OPTX(Val,INDEX_NONE);
	P_GET_UBOOL_OPTX(bForce,0);
	P_FINISH;

	*(FLOAT*)Result = ActualWidth(Val,bForce);
	
	unguardexec;
}


// Returns the actual height (including scaling) of a component
FLOAT UGUIComponent::ActualHeight( FLOAT Val, UBOOL bForce ) const
{
	guard(UGUIComponent::ActualHeight);

	if ( Val == INDEX_NONE )
	{
		if (bStandardized)
			return StandardHeight * Controller->ResY;
		else
			Val = WinHeight;
	}

	if ( !Controller )
		return 0.0;

	if ( Val < 2.f && !(bNeverScale || bForce) )
	{
		if ( bScaleToParent && MenuOwner && (ScalingType == SCALE_All || ScalingType == SCALE_Y) ) 
			return MenuOwner->ActualHeight() * Val;
		else
			return Controller->ResY * Val;
	}
	else
		return Val;

	unguardobj;
}

void UGUIComponent::execActualHeight(FFrame& Stack, RESULT_DECL )
{
	guard(GUIComponent::execActualHeight);
	P_GET_FLOAT_OPTX(Val,INDEX_NONE);
	P_GET_UBOOL_OPTX(bForce,0);
	P_FINISH;

	*(FLOAT*)Result = ActualHeight(Val,bForce);
	
	unguardexec;
}


// Returns the actual left (including scaling) of a component
FLOAT UGUIComponent::ActualLeft( FLOAT Val, UBOOL bForce ) const
{
	guard(UGUIComponent::ActualLeft);

	if ( Val == INDEX_NONE )
		Val = WinLeft;

	if ( !Controller )
		return 0.0f;

	if (Val < 2.f && Val > -2.f && !(bNeverScale||bForce))
	{
		FLOAT WL = Abs(Val);
		if ( (bBoundToParent) && (MenuOwner!=NULL) && (BoundingType == SCALE_All || BoundingType == SCALE_X)  )
		{
			if ( Val < 0 )
				return MenuOwner->ActualLeft() - (MenuOwner->ActualWidth() * WL);
			else
				return MenuOwner->ActualLeft() + (MenuOwner->ActualWidth() * WL);
		}
		else
		{
			if ( Val < 0 )
				return (Controller->ResX * WL)*-1;
			else
				return Controller->ResX * WL;
		}
	}
	else
	{
		if ( (bBoundToParent) && (MenuOwner!=NULL) )
			return MenuOwner->ActualLeft() + Val;
		else
			return Val;	
	}

	unguardobj;
}

void UGUIComponent::execActualLeft(FFrame& Stack, RESULT_DECL )
{
	guard(GUIComponent::execActualLeft);
	P_GET_FLOAT_OPTX(Val,INDEX_NONE);
	P_GET_UBOOL_OPTX(bForce,0);
	P_FINISH;

	*(FLOAT*)Result = ActualLeft(Val,bForce);
	
	unguardexec;
}


// Returns the actual top (including scaling) of a component
FLOAT UGUIComponent::ActualTop( FLOAT Val, UBOOL bForce ) const
{
	guard(UGUIComponent::ActualTop);

	if ( Val == INDEX_NONE )
		Val = WinTop;

	if ( !Controller )
		return 0.0;

	if ( Val < 2.f && Val >- 2.f && !(bNeverScale||bForce))
	{
		FLOAT WT = Abs(Val);

		if ( (bBoundToParent) && (MenuOwner!=NULL) && (BoundingType == SCALE_All || BoundingType == SCALE_Y) )
		{
			FLOAT MOT = MenuOwner->ActualTop();
			FLOAT MOH = MenuOwner->ActualHeight();
			
			if ( Val < 0 )
				return MOT - ( MOH * WT);
			else
				return MOT + ( MOH * WT);
		}
		else
		{
			if ( Val < 0 )
				return (Controller->ResY * WT)*-1;
			else
				return Controller->ResY * WT;
		}
	}
	else
	{		
		if ( (bBoundToParent) && (MenuOwner!=NULL) )
			return MenuOwner->ActualTop() + Val;
		else
			return Val;
	}

	unguardobj;
}

void UGUIComponent::execActualTop(FFrame& Stack, RESULT_DECL )
{
	guard(GUIComponent::execActualTop);
	P_GET_FLOAT_OPTX(Val,INDEX_NONE);
	P_GET_UBOOL_OPTX(bForce,0);
	P_FINISH;

	*(FLOAT*)Result = ActualTop(Val,bForce);
	
	unguardexec;
}

FLOAT UGUIComponent::RelativeLeft( FLOAT Value, UBOOL bForce ) const
{
	guard(UGUIComponent::RelativeLeft);

	if ( Value == INDEX_NONE )
		Value = WinLeft;

	if ( !Controller )
		return Value;
	
	if ( Value >= 2.0f || bForce )
	{
		FLOAT MOL = 0.f, MOW = Controller->ResX;
		if ( MenuOwner && bBoundToParent && (BoundingType == SCALE_All || BoundingType == SCALE_X))
		{
			MOL = MenuOwner->ActualLeft();
			MOW = MenuOwner->ActualWidth();
		}

		Value -= MOL;
		Value /= MOW;
	}

	return Value;

	unguardobj;
}

void UGUIComponent::execRelativeLeft( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIComponent::execRelativeLeft);

	P_GET_FLOAT_OPTX(RealLeft,INDEX_NONE);
	P_GET_UBOOL_OPTX(bForce,0);
	P_FINISH;

	*(FLOAT*)Result = RelativeLeft(RealLeft,bForce);
	unguardexec;
}

FLOAT UGUIComponent::RelativeTop( FLOAT Value, UBOOL bForce ) const
{
	guard(UGUIComponent::RelativeTop);

	if ( Value == INDEX_NONE )
		Value = WinTop;

	if ( !Controller )
		return Value;

	if ( Value >= 2.0f || bForce )
	{
		FLOAT MOT = 0.f, MOH = Controller->ResY;
		if ( MenuOwner && bBoundToParent && (BoundingType == SCALE_All || BoundingType == SCALE_Y) )
		{
			MOT = MenuOwner->ActualTop();
			MOH = MenuOwner->ActualHeight();
		}

		Value -= MOT;
		Value /= MOH;
	}

	return Value;
	
	unguardobj;
}

void UGUIComponent::execRelativeTop( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIComponent::execRelativeTop);
	P_GET_FLOAT_OPTX(RealTop,INDEX_NONE);
	P_GET_UBOOL_OPTX(bForce,0);
	P_FINISH;

	*(FLOAT*)Result = RelativeTop(RealTop,bForce);
	unguardexec;
}

FLOAT UGUIComponent::RelativeWidth( FLOAT Value, UBOOL bForce ) const
{
	guard(UGUIComponent::RelativeWidth);
	
	if ( Value == INDEX_NONE )
		Value = WinWidth;

	if ( !Controller )
		return Value;
	
	if ( Value >= 2.0f || bForce )
	{
		FLOAT MOW = Controller->ResX;
		if ( bScaleToParent && MenuOwner && (ScalingType == SCALE_All || ScalingType == SCALE_X) )
			MOW = MenuOwner->ActualWidth();

		Value /= MOW;
	}

	return Value;

	unguardobj;
}


void UGUIComponent::execRelativeWidth( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIComponent::execRelativeWidth);
	P_GET_FLOAT_OPTX(RealWidth,INDEX_NONE);
	P_GET_UBOOL_OPTX(bForce,0);
	P_FINISH;

	*(FLOAT*)Result = RelativeWidth(RealWidth,bForce);

	unguardexec;
}

FLOAT UGUIComponent::RelativeHeight( FLOAT Value, UBOOL bForce ) const
{
	guard(UGUIComponent::RelativeHeight);

	if ( Value == INDEX_NONE )
		Value = WinHeight;

	if ( !Controller )
		return Value;
	
	if ( Value >= 2.0f || bForce)
	{
		FLOAT MOH = Controller->ResY;
		if ( bScaleToParent && MenuOwner && (ScalingType == SCALE_All || ScalingType == SCALE_Y))
			MOH = MenuOwner->ActualHeight();

		Value /= MOH;
	}

	return Value;
	unguardobj;
}

void UGUIComponent::execRelativeHeight( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIComponent::execRelativeHeight);
	P_GET_FLOAT_OPTX(RealHeight,INDEX_NONE);
	P_GET_UBOOL_OPTX(bForce,0);
	P_FINISH;

	*(FLOAT*)Result = RelativeHeight(RealHeight,bForce);
	unguardexec;
}

void UGUIComponent::AutoPosition(TArray<UGUIComponent*>& Components, FLOAT PosL, FLOAT PosT, FLOAT PosR, FLOAT PosB, INT Col, FLOAT ColSpace)
{
	guard(UGUIComponent::AutoPosition);

	PosT = Min<FLOAT>(PosT,PosB);
	PosL = Min<FLOAT>(PosL,PosR);

	PosB = Max<FLOAT>(PosT,PosB);
	PosR = Max<FLOAT>(PosL,PosR);

	check(PosB>PosT);
	check(Col>0);
	check(ColSpace>=0.f);

	INT CLen = Components.Num();
	check(CLen);

	FLOAT TotalHeight = PosB - PosT, TotalWidth = PosR - PosL, Spacing = ColSpace * TotalWidth;

	UGUIComponent* comp = NULL;
	FLOAT ColRollover = CLen / Col;
	FLOAT CompWidth = (TotalWidth - ((Col - 1) * Spacing)) / Col;
	FLOAT CompOffset = TotalHeight / (appCeil(ColRollover));	// Amount of space that should be between each components WinTop

	INT ColWrap = appFloor(ColRollover);	// Where we should start a new column
	FLOAT X = PosL, Y = PosT;

	for (INT i = 0; i < CLen; i++)
	{
		comp = Components(i);
		if (comp)
		{
			if ( i > 0 && i % ColWrap == 0.f )
			{
				X += (CompWidth + Spacing);
				Y = PosT;
			}
			comp->SetAdjustedDims(CompWidth, comp->WinHeight, X, Y);
			Y += CompOffset;
		}
	}

	unguardobj;
}

void UGUIComponent::AutoPositionOn( TArray<UGUIComponent*>& Components, UGUIComponent* Frame, FLOAT LPerc, FLOAT TPerc, FLOAT RPerc, FLOAT BPerc, INT Col, FLOAT ColSpace )
{
	guard(UGUIComponent::AutoPositionOn);

	INT CLen = Components.Num();
	if (CLen == 0)
	{
		debugf(TEXT("%s::AutoPosition() could not be executed: Components array has no members."), GetName());
		return;
	}

	if ( !Frame )
	{
		debugf(TEXT("%s::AutoPosition() could not be executed: Frame was None."), GetName());
		return;
	}

	Col = Clamp(Col, 1, CLen);

	FLOAT AL = Frame->ActualLeft(), AT = Frame->ActualTop(), AW = Frame->ActualWidth(), AH = Frame->ActualHeight();

	AutoPosition(Components,
		AL + (AW * LPerc),
		AT + (AH * TPerc),
		(AL + AW) - (RPerc * AW),
		(AT + AH) - (BPerc * AH),
		Col, ColSpace
		);

	unguardobj;
}

void UGUIComponent::execAutoPositionOn( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIComponent::execAutoPositionOn);

	P_GET_TARRAY(Components,UGUIComponent*);
	P_GET_OBJECT(UGUIComponent,Frame);
	P_GET_FLOAT(LeftPadPerc);
	P_GET_FLOAT(UpperPadPerc);
	P_GET_FLOAT(RightPadPerc);
	P_GET_FLOAT(LowerPadPerc);

	P_GET_INT_OPTX(NumCol,1);
	P_GET_FLOAT_OPTX(ColPadding,0.f);
	
	P_FINISH;

	AutoPositionOn(Components, Frame, LeftPadPerc, UpperPadPerc, RightPadPerc, LowerPadPerc, NumCol, ColPadding);

	unguardexec;
}

void UGUIComponent::execAutoPosition( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIComponent::execAutoPosition);

	P_GET_TARRAY(Components,UGUIComponent*);
	
	P_GET_FLOAT(LeftBound);
	P_GET_FLOAT(UpperBound);
	P_GET_FLOAT(RightBound);
	P_GET_FLOAT(LowerBound);
	
	P_GET_FLOAT(LeftPad);
	P_GET_FLOAT(UpperPad);
	P_GET_FLOAT(RightPad);
	P_GET_FLOAT(LowerPad);

	P_GET_INT_OPTX(NumCol,1);
	P_GET_FLOAT_OPTX(ColPadding,0.f);
	
	P_FINISH;

	FLOAT NL, NT, NW, NH, CompH, Offset;
	NL = NT = NW = NH = CompH = Offset = 0.f;
	INT CLen = Components.Num();

	if (CLen == 0)
	{
		debugf(TEXT("%s::AutoPosition() could not be executed: Components array has no members."), GetName());
		return;
	}

	NumCol = Clamp(NumCol, 1, CLen);

	NL = LeftBound + LeftPad;
	NT = UpperBound + UpperPad;
	NW = RightBound - RightPad - NL;
	NH = LowerBound - LowerPad - NT;

	Offset = NH / (CLen/NumCol);	// Amount of space that should be between each components WinTop

	for (INT i = 0; i < CLen; i++)
		if (Components(i) && Components(i)->ActualHeight() >Offset)
			Components(i)->WinHeight = Components(i)->RelativeHeight(Offset);

//	if (Offset < CompH)
//	{
//		debugf(TEXT("%s::AutoPosition() could not be executed: component %i height (%f) greater than component offset (%f).  Use less components, or increase the size of the area determined by bounds paramters."), GetName(), Largest, CompH, Offset);
//		return;
//	}
	AutoPosition(Components, NL, NT, NL + NW, NT + NH, NumCol, ColPadding);

	unguardexec;
}

void UGUIComponent::execUpdateOffset( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIComponent::execUpdateOffset);

	P_GET_FLOAT(PosX);
	P_GET_FLOAT(PosY);
	P_GET_FLOAT(PosW);
	P_GET_FLOAT(PosH);
	P_FINISH;

	UpdateOffset(PosX, PosY, PosW, PosH);

	unguardexec;
}

void UGUIComponent::UpdateOffset(FLOAT PosX, FLOAT PosY, FLOAT PosW, FLOAT PosH)
{
	guard(UGUIComponent::UpdateOffset);
	if ( !Controller )
		return;

	MouseOffset[0] = Controller->MouseX - PosX;
	MouseOffset[1] = Controller->MouseY - PosY;
	MouseOffset[2] = PosW - Controller->MouseX;
	MouseOffset[3] = PosH - Controller->MouseY;

	for (INT i = 0; i < 4; i++)
		if (MouseOffset[i] < 0)
			MouseOffset[i] = 0;

	unguardobj;
}

// - SaveCanvasState and RestoreCanvasState are called before and after the actual component is rendered to make sure the
//   canvas is kept in the proper state for any other action.

void UGUI::SaveCanvasState(UCanvas* Canvas)
{
	guardSlow(UGUI::SaveCanvasState);

	check(Canvas);

	SaveX	  = Canvas->CurX;
	SaveY	  = Canvas->CurY;
	SaveColor = Canvas->Color;
	SaveStyle = Canvas->Style;
	SaveFont  = Canvas->Font;
	SaveModulation = Canvas->ColorModulate;

	unguardobjSlow;
}
	
void UGUI::RestoreCanvasState(UCanvas* Canvas)
{
	guardSlow(UGUI::RestoreCanvasState);

	check(Canvas);

	Canvas->CurX  = SaveX;
	Canvas->CurY  = SaveY;
	Canvas->Color = SaveColor;
	Canvas->Style = SaveStyle;
	Canvas->Font  = SaveFont;
	Canvas->ColorModulate = SaveModulation;

	unguardobjSlow;
}

// It's useful for each component to be able to act upon mouse/presses and releases 

UBOOL UGUIComponent::MousePressed(UBOOL IsRepeat)	
{
	guard(UGUIComponent::MousePressed);

	if ( INVALIDMENU )
		return false;

	// Store a reference to the controller, in case it's wiped out in script
	UGUIController* C = Controller;

	// Attempt to SetFocus to this control
	if ( MenuState == MSAT_Disabled || !PerformHitTest(C->MouseX, Controller->MouseY) )
		return false;

	// Play sound _before_ executing action
	if (!IsRepeat)
		C->PlayComponentSound(OnClickSound);

	eventSetFocus(NULL);
	eventMenuStateChange(MSAT_Pressed);

	if ( DELEGATE_IS_SET(OnMousePressed) )
		delegateOnMousePressed(this,IsRepeat);

	if ( bDropSource )
		return true;

	// If we never focus we want this to act as a click
	if (bRepeatClick)
	{
		if (bNeverFocus)
			C->PlayComponentSound(OnClickSound);

		if ( DELEGATE_IS_SET(OnClick) )
			delegateOnClick(this);
	}

	return true;

	unguardobj;
}		

UBOOL UGUIComponent::MouseReleased()
{
	guard(UGUIComponent::MouseReleased);

	if ( INVALIDMENU )
		return false;

	// Store a temporary reference to controller in case script sets it NULL
	UGUIController* C = Controller;
	UBOOL WasPressed = 0;
	if ( MenuState == MSAT_Pressed && (bVisible||C->DropSource == this) )
	{
		WasPressed = 1;
		eventMenuStateChange(MSAT_Focused);
	}

	if ( bDropTarget && C->DropTarget == this )
		return true;

	// Haven't yet begun a drag operation - only multi-selecting at this point
	if ( bDropSource && eventIsMultiSelect() && !C->DropSource )
	{
		if (DELEGATE_IS_SET(OnMouseRelease))
			delegateOnMouseRelease(this);

		return true;
	}

	// If we're currently performing a drag-n-drop operation, capture any mouse release
	if ( C->DropSource )
	{
		// In case the drop source wasn't visible
		if ( C->DropSource == this && DELEGATE_IS_SET(OnMouseRelease) )
			delegateOnMouseRelease(this);

		return true;
	}

	// Always accept mouse release if we are capturing the mouse
	if ( ((MenuState == MSAT_Pressed || WasPressed) && (bRequireReleaseClick || bCaptureMouse)) || PerformHitTest(C->MouseX, C->MouseY) )
	{
		if (!bRepeatClick) 
		{
			// Check for Double Click
			if ( !PerformDoubleClick() )
			{
				if (DELEGATE_IS_SET(OnClick))
					delegateOnClick(this);
				
				C->LastClickX = C->MouseX;
				C->LastClickY = C->MouseY;
				C->LastClickTime = appSeconds();
			}
		}
	}

	else return false;

	if ( DELEGATE_IS_SET(OnMouseRelease) )
		delegateOnMouseRelease(this);

	return true;

	unguardobj;
}

UBOOL UGUIComponent::PerformDoubleClick()
{
	guard(UGUIComponent::PerformDoubleClick);

	if ( INVALIDMENU )
		return false;

	UGUIController* C = Controller;

	if ( DELEGATE_IS_SET(OnDblClick) && (appSeconds() < C->LastClickTime + C->DblClickWindow) && (C->LastClickX == (INT)C->MouseX && C->LastClickY == (INT)C->MouseY) )
	{
		UBOOL Result = delegateOnDblClick(this);
		C->LastClickX = -1;
		C->LastClickY = -1;
		C->LastClickTime = 0.f;
		return Result;
	}
	
	return false;

	unguardobj;
}
// By Default, when the mouse is moved, components need to be searched to see if they are affected.  If this function returns true, then
// This component has used the input so go no further.  It should always be assumed that 

UBOOL UGUIComponent::MouseMove(INT XDelta, INT YDelta)
{
	guard(GUIComponent::MouseMove);

	if ( INVALIDMENU )
		return false;

	UGUIController* C = Controller;
	
	if ( MenuState == MSAT_Pressed )
	{
		if ( C->HasMouseMoved() )
		{
			if ( C->DropSource )
			{
				if ( DELEGATE_IS_SET(OnDragOver) )
					delegateOnDragOver(this);
			}
			else if ( bDropSource )
				eventDropStateChange(DRP_Source);
		}
	}

	if ( MenuState==MSAT_Pressed && bCaptureMouse && DELEGATE_IS_SET(OnCapturedMouseMove) )
		return delegateOnCapturedMouseMove(XDelta, YDelta);

	return false;

	unguardobj;
}

// When moving over things that have not captured the mouse, we might still want to know (ie changing cursor shape)
UBOOL UGUIComponent::MouseHover()
{
	guard(GUIComponent::MouseHover);

	if ( INVALIDMENU )
		return false;

	UGUIController* C = Controller;
	if ( DELEGATE_IS_SET(OnHover) && delegateOnHover(this) )
		return true;

	if ( C->ActivePage && C->ActivePage->MouseHover() )
		return true;

	return false;

	unguardobj;
}

// Input events should be handled by the subclasses

UBOOL UGUIComponent::NativeKeyEvent(BYTE& iKey, BYTE& State, FLOAT Delta )
{
	guard(GUIComponent::NativeKeyEvent);

	if ( INVALIDMENU )
		return false;

	if ( !bCaptureTabs && iKey == IK_Tab && (State == IST_Press || State == IST_Hold) )
	{
		if ( Controller->CtrlPressed )
		{
			if ( Controller->ShiftPressed )
			{
				if ( eventPrevPage() )
					return true;
			}

			else if ( eventNextPage() )
				return true;
		}

		else
		{
			if ( Controller->ShiftPressed )
			{
				if ( eventPrevControl(NULL) )
					return true;
			}
			else if ( eventNextControl(NULL) )
				return true;
		}
	}
	else if ( DELEGATE_IS_SET(OnKeyEvent) )
        return delegateOnKeyEvent(iKey, State, Delta);

	return false;

	unguardobj;
}

UBOOL UGUIComponent::NativeKeyType(BYTE& iKey, TCHAR Unicode )
{
	guard(GUIComponent::NativeKeyType);

	if ( INVALIDMENU )
		return false;

	if ( DELEGATE_IS_SET(OnKeyType) )
        return delegateOnKeyType(iKey,FString::Printf(TEXT("%c"), Unicode));

	return false;

	unguardobj;
}

void UGUIComponent::CloneDims(UGUIComponent* From)
{
	guard(GUIComponent::CloneDims);

	if ( From )
		SetAdjustedDims(From->ActualWidth(), From->ActualHeight(), From->ActualLeft(), From->ActualTop());

	unguardobj;
}

void  UGUIComponent::SetDims(FLOAT Width, FLOAT Height, FLOAT Left, FLOAT Top)
{
	guard(GUIComponent::SetDims);

	WinWidth = Width;
	WinHeight = Height;
	WinLeft = Left;
	WinTop = Top;

	unguardobj;
}

// TODO Support negative relative position
void UGUIComponent::SetAdjustedDims( FLOAT Width, FLOAT Height, FLOAT Left, FLOAT Top )
{
	guard(UGUIComponent::SetAdjustedDims);

	if ( INVALIDMENU )
		return;

	if ( !bNeverScale )
	{
		Width = RelativeWidth(Width);
		Height = RelativeHeight(Height);
		Left = RelativeLeft(Left);
		Top = RelativeTop(Top);
	}

	SetDims(Width, Height, Left, Top);

	unguardobj;
}

UBOOL UGUIComponent::SpecialHit( UBOOL bForce )
{
	guard(UGUIComponent::SpecialHit);

	if ( INVALIDMENU )
		return false;

	UGUIController* C = Controller;

	if ( bForce || WithinBounds(C->MouseX, C->MouseY) )
	{
		if (C->MoveControl == this)
			C->MoveControl = NULL;
		else if (C->MoveControl == NULL || (!bForce && !C->MoveControl->WithinBounds(C->MouseX, C->MouseY)))
		{
			C->SetMoveControl(this);
			return true;
		}

	}

	return false;
	unguardobj;
}

UGUIComponent* UGUIComponent::UnderCursor(FLOAT MouseX, FLOAT MouseY)
{
	guard(UGUIComponent::UnderCursor);

	if ( bVisible && MenuState!=MSAT_Disabled && PerformHitTest(MouseX,MouseY) )
		return this;

	return NULL;
	unguardobj;
}

void UGUIComponent::NativeInvalidate(UGUIComponent* Who)
{
	guard(UGUIComponent::NativeInvalidate);

	if ( DELEGATE_IS_SET(OnInvalidate) )
		delegateOnInvalidate(Who);

	unguardobj;
}

void UGUIComponent::execSetTimer(FFrame& Stack, RESULT_DECL )
{
	guard(UGUIComponent::execSetTimer);

	// Verify that the control is really on the good page
	P_GET_FLOAT(Interval);
	P_GET_UBOOL_OPTX(bRepeat, false);
	P_FINISH;

	// If our owning menu is closing or opening, do not continue
	if ( !Controller )
	{
		debugf(TEXT("%s.SetTimer() may not be called while menu is being initialized or destroyed!"), GetMenuPath());
		return;
	}

	// Find Component's Page
	UGUIPage* Page = eventOwnerPage();
	if ( Page == NULL )
		return;

	if (TimerIndex < 0 && Interval > 0.0f)
		TimerIndex = Page->Timers.AddItem(this);

	TimerInterval = Interval;
	TimerCountdown = Interval;
	bTimerRepeat = false;
	if (TimerInterval > 0.0f)
		bTimerRepeat = bRepeat;

	unguardexec;
}

void UGUIComponent::execKillTimer(FFrame& Stack, RESULT_DECL )
{
	guard(UGUIComponent::execKillTimer);
	P_FINISH;

	TimerInterval=0.0;
	TimerCountdown=0.0;
	bTimerRepeat=false;

	unguardexec;
}

void UGUIComponent::execSpecialHit( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIComponent::execSpecialHit);

	P_GET_UBOOL_OPTX(bForce,0);
	P_FINISH;

	*(UBOOL*)Result = SpecialHit(bForce);
	unguardexec;
}

void UGUIComponent::NotifyResolutionChange( INT NewX, INT NewY )
{
	guard(UGUIComponent::NotifyResolutionChange);

	bPositioned = 0;
	eventResolutionChanged(NewX,NewY);

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIMultiComponent
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIMultiComponent::NotifyResolutionChange(INT NewX, INT NewY)
{
	guard(UGUIMultiComponent::NotifyResolutionChange);

	for ( INT i = 0; i < Controls.Num(); i++ )
	{
		if ( Controls(i) )
			Controls(i)->NotifyResolutionChange(NewX, NewY);
	}

	Super::NotifyResolutionChange(NewX, NewY);

	unguardobj;
}

void UGUIMultiComponent::execInitializeControls(FFrame& Stack, RESULT_DECL)
{
	guard(UGUIMultiComponent::execInitializeControls);
	P_FINISH;

	InitializeControls();

	unguardexec;
}

static bool bRemap = false;
static FLOAT Compare(UGUIComponent* A, UGUIComponent* B)
{
	guardSlow(UGUI::Compare);

	FLOAT result(0.f);
	if ( bRemap )
	{
		result = A->TabOrder - B->TabOrder;
		if ( !result )
			result = A->RenderWeight - B->RenderWeight;
	}
	else 
	{
		result = A->RenderWeight - B->RenderWeight;
		if ( !result )
			result = A->TabOrder - B->TabOrder;
	}

	return result;

	unguardSlow;
}

void UGUIMultiComponent::InitializeControls()
{
	guard(UGUIMultiComponent::InitializeControls);

	if ( Controls.Num() > 0 && !bAlwaysAutomate )		// Controls array already initialized
	{
		bOldStyleMenus=true;
		return;
	}

	bOldStyleMenus=false;
	Controls.Empty();

	UClass* Cls = GetClass();
	UGUIComponent* Ptr = NULL;

	for ( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Cls); It; ++It )
	{
		if ( It->PropertyFlags & CPF_Automated )
		{
			for ( INT i = 0; i < It->ArrayDim; i++ )
			{
				if ( It->IsA(UArrayProperty::StaticClass()) )
				{
					UProperty* Prop = Cast<UArrayProperty>(*It)->Inner;
					if ( Prop->IsA(UObjectProperty::StaticClass()) && Cast<UObjectProperty>(Prop)->PropertyClass->IsChildOf(UGUIComponent::StaticClass()) )
					{
						FArray* Array = (FArray*)((BYTE*)this + It->Offset + (i * It->ElementSize));
						if ( Array )
						{
							for ( INT j = 0; j < Array->Num(); j++ )
							{
								Ptr = (UGUIComponent*)( ((PTRINT*)Array->GetData())[j] );
								if ( Ptr )
									Controls.AddItem(Ptr);
							}
						}
					}
				}

				else if ( It->IsA(UObjectProperty::StaticClass()) && Cast<UObjectProperty>(*It)->PropertyClass->IsChildOf(UGUIComponent::StaticClass()) )
				{
					Ptr = *((UGUIComponent**)(((BYTE*)this) + It->Offset + (i * It->ElementSize)));
					if( Ptr )
						Controls.AddItem(Ptr);
				}
			}
		}
	}

	bRemap = false;
	if ( Controls.Num() > 1 )
		Sort( &Controls(0), Controls.Num() );

	unguardobj;
}

// RemapComponents - This sets the tab order for all the components on this page
void UGUIMultiComponent::execRemapComponents( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIMultiComponent::execRemapComponents);

	P_FINISH;
	RemapComponents();

	unguardexec;
}

void UGUIMultiComponent::RemapComponents()
{
	guard(UGUIMultiComponent::RemapComponents);

	Components.Empty();
	UGUIComponent* Comp = NULL;
	for ( INT i = 0; i < Controls.Num(); i++ )
	{
		Comp = Controls(i);
		if ( Comp && Comp->bTabStop )
			Components.AddItem(Comp);
	}

	bRemap = true;
	if ( Components.Num() > 1 )
		Sort( &Components(0), Components.Num() );

	unguard;
}

UBOOL UGUIMultiComponent::PerformHitTest(INT MouseX, INT MouseY)
{
	guard(GUIMultiComponent::PerformHitTest);

	if ( !bVisible || bAnimating )
		return false;

	if (FocusedControl && FocusedControl->PerformHitTest(MouseX, MouseY))
		return true;

	UGUIComponent* Comp = NULL;
	for ( INT i = Controls.Num() - 1; i >= 0; i-- )
	{
		Comp = Controls(i);
		if ( Comp && !IsFocusedOn(Comp) && Comp->PerformHitTest(MouseX,MouseY) )
			return true;
	}

	return Super::PerformHitTest(MouseX, MouseY);

	unguardobj;
}

void UGUIMultiComponent::PreDraw(UCanvas* Canvas)
{
	guard(GUIMultiComponent::PreDraw);

	if ( INVALIDRENDER )
		return;

	Super::PreDraw(Canvas);
	PreDrawControls(Canvas);
	unguardobj;
}

void UGUIMultiComponent::PreDrawControls( UCanvas* Canvas )
{
	guard(UGUIMultiComponent::PreDrawControls);

	if ( INVALIDRENDER )
		return;

	UGUIComponent* Comp = NULL;
	INT compnum = Controls.Num();
	for ( INT i = 0; i < compnum; i++ )
	{
		Comp = Controls(i);
		if ( Comp )
		{
			Comp->SaveCanvasState(Canvas);
			Comp->PreDraw(Canvas);
			Comp->RestoreCanvasState(Canvas);
		}
	}
	unguardobj;
}

void UGUIMultiComponent::Draw(UCanvas* Canvas)
{
	guard(GUIMultiComponent::Draw);

	if ( !bVisible )
		return;

	Super::Draw(Canvas);
	DrawControls(Canvas);

	unguardobj;
}

void UGUIMultiComponent::DrawControls( UCanvas* Canvas )
{
	guard(UGUIMultiComponent::DrawControls);

	if ( INVALIDRENDER )
		return;

	INT FocusedIndex=INDEX_NONE, compnum = Controls.Num();
	UGUIComponent* Comp = NULL;

	for ( INT i = 0; i < compnum; i++ )
	{
		Comp = Controls(i);
		if ( Comp )
		{
			if ( bDrawFocusedLast && IsFocusedOn(Comp) )
				FocusedIndex = i;
			else
			{
				Comp->SaveCanvasState(Canvas);
#if UCONST_Counter == 0
				if ( OBJ_DELEGATE_IS_SET(Comp, OnDraw) )
				{
					if (!Comp->delegateOnDraw(Canvas) )
						Comp->Draw(Canvas);
				}
				else
					Comp->Draw(Canvas);
#else
				if ( !Comp->delegateOnDraw(Canvas) )
					Comp->Draw(Canvas);
#endif

#if UCONST_Counter == 0
				if ( OBJ_DELEGATE_IS_SET(Comp, OnRendered) )
#endif
					Comp->delegateOnRendered(Canvas);

				Comp->RestoreCanvasState(Canvas);
			}
		}
	}

	if ( Controls.IsValidIndex(FocusedIndex) )
	{
		Comp = Controls(FocusedIndex);
		if ( Comp )
		{
			Comp->SaveCanvasState(Canvas);
#if UCONST_Counter == 0
			if ( OBJ_DELEGATE_IS_SET(Comp,OnDraw) )
			{
				if ( !Comp->delegateOnDraw(Canvas) )
					Comp->Draw(Canvas);
			}
			else 
				Comp->Draw(Canvas);

			if ( OBJ_DELEGATE_IS_SET(Comp, OnRendered) )
				Comp->delegateOnRendered(Canvas);
#else
			if ( !Comp->delegateOnDraw(Canvas) )
				Comp->Draw(Canvas);

			Comp->delegateOnRendered(Canvas);
#endif
			Comp->RestoreCanvasState(Canvas);
		}
	}
	unguardobj;
}

UBOOL UGUIMultiComponent::NativeKeyEvent(BYTE& iKey, BYTE& State, FLOAT Delta )
{
	guard(UGUIMultiComponent::NativeKeyEvent);

	if ( bAnimating )
		return false;

	if ( (FocusedControl!=NULL) && (FocusedControl->NativeKeyEvent(iKey, State, Delta)) )
		return true;

	if (MenuState==MSAT_Focused)
		return Super::NativeKeyEvent(iKey, State, Delta);

	return false;

	unguardobj;
}
				 
UBOOL UGUIMultiComponent::NativeKeyType(BYTE& iKey, TCHAR Unicode )
{

	guard(UGUIMultiComponent::NativeKeyType);

	if ( bAnimating )
		return false;

	if ( (FocusedControl!=NULL) && (FocusedControl->NativeKeyType(iKey, Unicode)) )
		return true;

	if (MenuState==MSAT_Focused)
		return Super::NativeKeyType(iKey, Unicode);

	return false;

	unguardobj;
}

UGUIComponent* UGUIMultiComponent::UnderCursor(FLOAT MouseX, FLOAT MouseY)
{

	guard(UGUIMultiComponent::UnderCursor);
	UGUIComponent* tComp(NULL), *Comp;

	if ( bAnimating )
		return NULL;

	if (FocusedControl!=NULL)
	{
		tComp = FocusedControl->UnderCursor(MouseX,MouseY);
		if (tComp)
			return tComp;
	}

	for (INT i=Controls.Num() - 1; i >= 0; i--)
	{
		Comp = Controls(i);
		if ( Comp && Comp != FocusedControl )
		{
			tComp = Comp->UnderCursor(MouseX, MouseY);
			if (tComp)
				return tComp;
		}
	}

	return Super::UnderCursor( MouseX, MouseY );

	unguardobj;
}

void UGUIMultiComponent::NativeInvalidate(UGUIComponent* Who)
{
	guard(UGUIMultiComponent::NativeInvalidate);

	Super::NativeInvalidate(Who);

	if (FocusedControl!=NULL)
		FocusedControl->NativeInvalidate(Who);

	unguardobj;
}

UBOOL UGUIMultiComponent::SpecialHit( UBOOL bForce )
{

	guard(UGUIMultiComponent::SpecialHit);

	if ( bAnimating )
		return false;

	if (Super::SpecialHit(bForce))
		return true;

	UGUIComponent* Comp = NULL;
	for (INT i = Controls.Num() - 1; i >= 0; i-- )
	{
		Comp = Controls(i);
		if ( Comp && Comp->SpecialHit(bForce) )
			return true;
	}

	return false;

	unguardobj;

}

UBOOL UGUIMultiComponent::MousePressed(UBOOL IsRepeat)
{
	guard(UGUIMultiComponent::MousePressed);

	if ( bAnimating )
		return false;

	if ( bCaptureMouse )
		return Super::MousePressed(IsRepeat);

	if ( FocusedControl!=NULL && FocusedControl->MousePressed(IsRepeat) )
		return true;

	UGUIComponent* Comp = NULL;
	for (INT i=Controls.Num()-1;i>=0;i--)
	{
		Comp = Controls(i);
		if ( Comp && !IsFocusedOn(Comp) && Comp->MousePressed(IsRepeat) )
			return true;
	}

	if ( bAcceptsInput )
		return Super::MousePressed(IsRepeat);

	return false;

	unguardobj;

}

UBOOL UGUIMultiComponent::MouseReleased()
{
	guard(UGUIMultiComponent::MouseReleased);

	if ( bAnimating )
		return false;

	if ( bCaptureMouse )
		return Super::MouseReleased();

	if ( FocusedControl && FocusedControl->MouseReleased() )
		return true;

	UGUIComponent* Comp;
	for (INT i=Controls.Num()-1;i>=0;i--)
	{
		Comp = Controls(i);
		if ( Comp && !IsFocusedOn(Comp) && Comp->MouseReleased() )
			return true;
	}


	if ( bAcceptsInput )
		return Super::MouseReleased();

	return false;

	unguardobj;
}

void UGUIMultiComponent::execFindComponentIndex( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIMultiComponent::execFindComponentIndex);

	P_GET_OBJECT(UGUIComponent,Who);
	P_FINISH;

	*(INT*)Result = Components.FindItemIndex(Who);

	unguardexec;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUI
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUI::execProfile( FFrame& Stack, RESULT_DECL )
{
	guard(UGUI::execProfile);
	
	P_GET_STR(ProfileName);
	P_FINISH;

	if ( !Controller )
		return;

	Controller->Profile(ProfileName);
	unguardexec;
}

// Stuff used in Getting user mod data form the UT2K4MOD.ini file located in each
// Mod's root directory.

void UGUI::execGetModList( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIController::execGetModList);
	P_GET_TARRAY_REF(ModDirs,   FString); 
	P_GET_TARRAY_REF(ModTitles, FString); 
	P_FINISH;

	FString SearchPath = FString::Printf(TEXT("..")) * FString(TEXT("*.*"));
	TArray<FString> DirList = GFileManager->FindFiles( *SearchPath, 0, 1 );		

	for (INT i=0;i<DirList.Num();i++)
	{
		FString Filename = FString::Printf( TEXT("..") ) * DirList(i) * FString( TEXT("ut2k4mod.ini") );
		if ( GFileManager->FileSize(*Filename)>0 )
		{
			FConfigFile ModFile;
			ModFile.Read(*Filename);

			FConfigSection* Sec = ModFile.Find( TEXT("MOD") );
			if( !Sec )
				break;

			FString* Title  = Sec->Find( TEXT("ModTitle") );
			 
			if( !Title )
				break;

			INT index=ModDirs->AddZeroed();
			(*ModDirs)(index) = DirList(i);
			index=ModTitles->AddZeroed();
			(*ModTitles)(index) = *Title;
		}
	}
	unguardexec;
}
void UGUI::execGetModValue( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIController::execGetModDesc);
	P_GET_STR(ModDir);
	P_GET_STR(ModKey);
	P_FINISH;


	FString Filename = FString::Printf( TEXT("..") ) * ModDir * FString( TEXT("ut2k4mod.ini") );
	if ( GFileManager->FileSize(*Filename)>0 )
	{
		FConfigFile ModFile;
		ModFile.Read(*Filename);

		FConfigSection* Sec = ModFile.Find( TEXT("MOD") );
		if( Sec )
		{
			FString* S = Sec->Find( *ModKey );
			if (S)
			{
				*(FString*)Result = *S;
				return;
			}
		}

		*(FString*)Result = TEXT("");
	}
	unguardexec;
}


void UGUI::execGetModLogo( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIController::execGetModLogo);
	P_GET_STR(ModDir);
	P_FINISH;


	FString Filename = FString::Printf( TEXT("..") ) * ModDir * FString( TEXT("ut2k4mod.ini") );
	if ( GFileManager->FileSize(*Filename)>0 )
	{
		FConfigFile ModFile;
		ModFile.Read(*Filename);

		FConfigSection* Sec = ModFile.Find( TEXT("MOD") );
		if( Sec )
		{
			FString* pModLogo = Sec->Find( TEXT("ModLogo") );
			if ( !pModLogo  )
				return;
			
			int Dot = pModLogo->InStr(TEXT("."));
			FString pModPkg = pModLogo->Left(Dot);
			FString MatFile = FString::Printf(TEXT("..")) * ModDir * TEXT("Textures") * (*pModPkg) + TEXT(".utx");

			if ( GFileManager->FileSize(*MatFile) > 0 )
			{
				UMaterial* Logo = Cast<UMaterial>(StaticLoadObject( UMaterial::StaticClass(), NULL, **pModLogo, *MatFile , LOAD_NoWarn, NULL ));
				*(UMaterial**)Result = Logo;
				return;
			}
		}
	}
	unguardexec;
}

void UGUIController::execCheckFirewall( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIController::execCheckFirewall);

	P_FINISH;

	UViewport* Viewport = Cast<UViewport>(ViewportOwner);
	if ( Viewport )
	{
		*(UBOOL*)Result = Viewport->FireWallHack(0);
	}
	unguard;
}

void UGUIController::execAuthroizeFirewall( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIController::execCheckFirewall);

	P_FINISH;

	UViewport* Viewport = Cast<UViewport>(ViewportOwner);
	if ( Viewport )
	{
		*(UBOOL*)Result = Viewport->FireWallHack(1);
	}
	unguard;
}