/*=============================================================================
	UnGUI.cpp: See .UC for for info
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Joe Wilcox
	* Revised by Ron Prestenback / seperated into UnGUICore/UnGUI/GUILists
=============================================================================*/

#include "XInterface.h"

IMPLEMENT_CLASS(UGUIPage);
IMPLEMENT_CLASS(UGUIImage);
IMPLEMENT_CLASS(UGUILabel);
IMPLEMENT_CLASS(UGUIBorder);
IMPLEMENT_CLASS(UGUITitleBar);
IMPLEMENT_CLASS(UGUIPanel);
IMPLEMENT_CLASS(UGUITabControl);
IMPLEMENT_CLASS(UGUITabButton);
IMPLEMENT_CLASS(UGUITabPanel);
IMPLEMENT_CLASS(UGUIButton);
IMPLEMENT_CLASS(UGUIGFXButton);
IMPLEMENT_CLASS(UGUICheckBoxButton);
IMPLEMENT_CLASS(UGUISpinnerButton);
IMPLEMENT_CLASS(UGUIEditBox);
IMPLEMENT_CLASS(UGUINumericEdit);
IMPLEMENT_CLASS(UGUIFloatEdit);
IMPLEMENT_CLASS(UGUIScrollBarBase);
IMPLEMENT_CLASS(UGUIScrollButtonBase);
IMPLEMENT_CLASS(UGUIScrollZoneBase);
IMPLEMENT_CLASS(UGUIGripButtonBase);
IMPLEMENT_CLASS(UGUIComboBox);
IMPLEMENT_CLASS(UGUIMenuOption);
IMPLEMENT_CLASS(UGUISplitter);
IMPLEMENT_CLASS(UGUISlider);
IMPLEMENT_CLASS(UGUISectionBackground);
IMPLEMENT_CLASS(UCoolImage);
IMPLEMENT_CLASS(UGUIProgressBar);
IMPLEMENT_CLASS(UGUIContextMenu);
IMPLEMENT_CLASS(UGUIToolTip);
IMPLEMENT_CLASS(UStateButton);

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIPage
// =======================================================================================================================================================
// =======================================================================================================================================================
UBOOL UGUIPage::MouseHover()
{
	guard(UGUIPage::MouseHover);

	if (bAnimating)
		return true;	// Ignore when animating in or out

	if ( DELEGATE_IS_SET(OnHover) && delegateOnHover(this) )
		return true;

	return false;
	unguardobj;
}

UGUIComponent* UGUIPage::UnderCursor( FLOAT MouseX, FLOAT MouseY )
{
	guard(UGUIPage::UnderCursor);

	if ( bAnimating )
		return NULL;

	if ( bCaptureMouse )
		return this;

	UGUIComponent* Under = Super::UnderCursor(MouseX,MouseY);
	if ( !Under && bCaptureInput )
		Under = this;

	return Under;
	unguardobj;
}

void UGUIPage::Draw(UCanvas* Canvas)
{
	guard(GUIPage::Draw);

	if (!bVisible ||INVALIDRENDER )
		return;

	SaveCanvasState(Canvas);	// Save Canvas State

#if UCONST_Counter == 0
	if ( DELEGATE_IS_SET(OnDraw) )
#endif
		if ( delegateOnDraw(Canvas) )
		{
			RestoreCanvasState(Canvas);
			return;
		}

	// Draw the background
	if (Background!=NULL)
	{
		Canvas->Color = BackgroundColor;
		Canvas->Style = BackgroundRStyle;
		Canvas->DrawTileScaleBound( Background, ActualLeft(), ActualTop(), ActualWidth(), ActualHeight() );
	}

	Super::Draw(Canvas);

#if UCONST_Counter == 0
	if (DELEGATE_IS_SET(OnRendered))
#endif
		delegateOnRendered(Canvas);

	RestoreCanvasState(Canvas);		// Restore the Canvas state

	unguardobj;
}

UBOOL UGUIPage::NativeKeyEvent(BYTE& iKey, BYTE& State, FLOAT Delta )
{
	guard(UGUIPage::NativeKeyEvent);

	if (bAnimating) return false;	// Ignore when animating in or out

	if ( INVALIDMENU )
		return false;

	UGUIController* C = Controller;
	if ( Super::NativeKeyEvent(iKey, State, Delta) )
		return true;

	// Handle Close	-- Add support for aborting
	if ( State == IST_Press && iKey == IK_Escape && C->DropSource == NULL )
		C->eventCloseMenu(true);

	return true;

	unguardobj;
}

void UGUIPage::UpdateTimers(float DeltaTime)
{
	guard(UGUIPage::UpdateTimers);

	if ( INVALIDMENU )
		return;

	// We want to ignore any new timer created during this cycle
	INT TimerCnt = Timers.Num();
	UGUIComponent* Timer = NULL;
	for (INT i = 0; i < TimerCnt; i++)
	{
		Timer = Timers(i);
		if ( !Timer )
		{
			Timers.Remove(i--);
			TimerCnt--;
			continue;
		}

		Timer->TimerCountdown -= DeltaTime;
		Timer->TimerIndex = i;

		if (Timer->TimerCountdown <= 0.0 && Timer->TimerInterval > 0.0)
		{
			Timer->eventTimer();

			if (Timer->bTimerRepeat)
			{
				Timer->TimerCountdown += Timer->TimerInterval;
	
				// In case TimerInterval is too small
				if (Timer->TimerCountdown <= 0.0)
					Timer->TimerCountdown = Timer->TimerInterval;
			}
			else
			{
				Timer->TimerCountdown = 0.0;
				Timer->TimerInterval = 0.0;
			}
		}
		// Remove any defunct timer 
		if (Timer->TimerInterval <= 0.0)
		{
			Timer->TimerIndex = -1;
			Timers.Remove(i--);
			TimerCnt--;		// Make sure we dont trigger timers created in this timer cycle
		}
	}
	unguardobj;
}

UBOOL UGUIPage::MousePressed(UBOOL IsRepeat)
{
	guard(UGUIPage::MousePressed);

	if (bAnimating || INVALIDMENU ) return false;	// Ignore when animating in or out

	// Always assign a temp UGUIController pointer in case script sets Controller to NULL
	UGUIController* C = Controller;

	if ( !IsRepeat && C->ActiveControl == this && PerformHitTest(C->MouseX,C->MouseY) )
	{
		if ( C->FocusedControl )
			C->FocusedControl->NativeInvalidate(this);
	}

	if ( Super::MousePressed(IsRepeat) )
		return true;

	return bCaptureInput;

	unguardobj;

}

UBOOL UGUIPage::MouseReleased()
{
	guard(UGUIPage::MouseReleased);

	if (bAnimating || INVALIDMENU ) return false;	// Ignore when animating in or out

	// Always assign a temp UGUIController pointer in case script sets Controller to NULL
	UGUIController* C = Controller;

	if ( Super::MouseReleased() )
		return true;

	return bCaptureInput || PerformHitTest(C->MouseX,C->MouseY);

	unguardobj;
}


// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIPanel
// =======================================================================================================================================================
// =======================================================================================================================================================

UBOOL UGUIPanel::PerformHitTest(INT MouseX, INT MouseY)
{
	guard(GUIPanel::PerformHitTest);
	
	return Super::PerformHitTest(MouseX, MouseY);

	unguardobj;
}


void UGUIPanel::Draw(UCanvas* Canvas)
{
	guard(UGUPanel::Draw);

	if (!bVisible || INVALIDRENDER )
		return;

	if (MenuOwner==NULL)
	{
		debugf(TEXT("%s has no MenuOwner"),GetName() );
		return;
	}

	if (Background!=NULL)
		Canvas->DrawTileScaleBound( Background, ActualLeft(), ActualTop(), ActualWidth(), ActualHeight() );

	Super::Draw(Canvas);

	// Yech.. have to keep this for compatibility I guess

	if ( DELEGATE_IS_SET(OnPostDraw) )
		delegateOnPostDraw(Canvas);

	unguardobj;

}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUITabControl
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUITabControl::PreDraw(UCanvas* Canvas)
{
	guard(GUITabControl::PreDraw);

	if ( INVALIDRENDER )
		return;

	// Always assign a temp UGUIController pointer in case script sets Controller to NULL
	UGUIController* C = Controller;
	UGUIComponent::PreDraw(Canvas);

	INT StartIndex, RowX, RowY, ExtraPerButton, Count,  BottomRow, AHeight;
	FLOAT XL,YL;
	AHeight = StartIndex = RowX = RowY = ExtraPerButton = Count = XL = YL = BottomRow = 0;

	FLOAT ATabHeight, CompSize, TotalRowSize, RowWidth;
	ATabHeight = CompSize = TotalRowSize = RowWidth = 0.0f;
	
	const FLOAT AT = ActualTop(), AW = ActualWidth(), AL = ActualLeft(), AH = ActualHeight();

	ATabHeight = TabHeight;
	AHeight = AH;
	RowWidth = AW;

	if (ATabHeight<1)
		ATabHeight = C->ResY * ATabHeight;


	// Account for Style

	if (BackgroundStyle!=NULL)
	{
		RowWidth = RowWidth - BackgroundStyle->BorderOffsets[0] - BackgroundStyle->BorderOffsets[2];
		AHeight = AHeight - BackgroundStyle->BorderOffsets[1] - BackgroundStyle->BorderOffsets[3];
	}

	if (bDrawTabAbove)
	{
		if (BackgroundStyle!=NULL)
			RowY = AT + BackgroundStyle->BorderOffsets[1];
		else
			RowY = AT;
	}
	else
	{
		if (BackgroundStyle != NULL)
			RowY = AT + AHeight - BackgroundStyle->BorderOffsets[3] - ATabHeight;
		else
			RowY = AT + AHeight - ATabHeight;
	}

	UGUITabButton* Button = NULL;
	RowX = BackgroundStyle ? BackgroundStyle->BorderOffsets[0] + AL : AL;
	for (INT i=0;i<TabStack.Num();i++)
	{
		Button = TabStack(i);
		if ( Button )
		{
			Button->Style->TextSize(Canvas,Button->MenuState,*(Button->Caption),XL,YL,Button->FontScale);
			CompSize = 10 + XL + Button->Style->BorderOffsets[0] + Button->Style->BorderOffsets[2];
	        
			if ( ( TotalRowSize + CompSize > RowWidth ) )	// If we will overflow, or we are the lsat control
			{
				// Align and position this row.
				ExtraPerButton = bFillSpace ? appFloor( ((RowWidth - TotalRowSize) / Count ) - ((TabStack(StartIndex)->Style->BorderOffsets[0] + TabStack(StartIndex)->Style->BorderOffsets[2]) / Count) )  : 0;
				RowX = BackgroundStyle ? AL + BackgroundStyle->BorderOffsets[0] + 3 : AL + 3;

				while (Count>0)
				{
					TabStack(StartIndex)->Style->TextSize(Canvas,TabStack(StartIndex)->MenuState,*(TabStack(StartIndex)->Caption),XL,YL,TabStack(StartIndex)->FontScale);
					FLOAT ThisCompSize = 10 + XL + TabStack(StartIndex)->Style->BorderOffsets[0] + TabStack(StartIndex)->Style->BorderOffsets[2] + ExtraPerButton;

					TabStack(StartIndex)->SetAdjustedDims(ThisCompSize,ATabHeight,RowX,RowY);

					RowX+=ThisCompSize;
					Count--;
					StartIndex++;
					if (BackgroundStyle)
						BottomRow = bDrawTabAbove ? BackgroundStyle->BorderOffsets[0] + RowY + ATabHeight : RowY;
					else
                        BottomRow = bDrawTabAbove ? AT + RowY + ATabHeight : RowY;
				}
				
				if (bDrawTabAbove)
                    RowY += ATabHeight;//-3;
				else
					RowY -= ATabHeight;

				TotalRowSize=0;
			}
		}

		Count++;
		TotalRowSize+=CompSize;
	}

	if (Count > 0)
	{
		// Align and position last row.
		ExtraPerButton = bFillSpace 
			? appFloor( ((RowWidth - TotalRowSize) / Count) - ((TabStack(StartIndex)->Style->BorderOffsets[0] + TabStack(StartIndex)->Style->BorderOffsets[2]) / Count) ) : 0;
		
		RowX = BackgroundStyle
			? AL + BackgroundStyle->BorderOffsets[0] : AL;

		while (Count>0)
		{
			TabStack(StartIndex)->Style->TextSize(Canvas,TabStack(StartIndex)->MenuState,*(TabStack(StartIndex)->Caption),XL,YL,TabStack(StartIndex)->FontScale);
			FLOAT ThisCompSize = 10 + XL + TabStack(StartIndex)->Style->BorderOffsets[0] + TabStack(StartIndex)->Style->BorderOffsets[2] + ExtraPerButton;

			TabStack(StartIndex)->SetAdjustedDims(ThisCompSize,ATabHeight,RowX,RowY);

			RowX+=ThisCompSize;
			Count--;
			StartIndex++;

			if (BackgroundStyle)
				BottomRow = bDrawTabAbove
				? AT + BackgroundStyle->BorderOffsets[1] + RowY + ATabHeight : RowY;

			else BottomRow = bDrawTabAbove
				? AT + RowY + ATabHeight : RowY;
		}
		if (bDrawTabAbove)
            RowY += ATabHeight;//-3;
		else
			RowY -= ATabHeight;

		BottomRow = RowY;
	}

	if ( bDockPanels )
	{
		for ( INT i = 0; i < TabStack.Num(); i++ )
		{
			Button = TabStack(i);
			if ( Button )
			{
				Button->SaveCanvasState(Canvas);
				Button->PreDraw(Canvas);
				Button->RestoreCanvasState(Canvas);
				if ( Button->MyPanel )
				{                                         // Size panel if
					if ( !Button->MyPanel->bPositioned || // this is the panel's first PreDraw(), or
					      Button == ActiveTab ||          // this is the active tab, or
					      Button == PendingTab            // this is the pending tab
						  )        
					{
						FLOAT PLeft(0.f), PTop(0.f), PWidth(0.f), PHeight(0.f);
						PTop = bDrawTabAbove 
							? BottomRow : BackgroundStyle
							? AT + BackgroundStyle->BorderOffsets[1] : AT;

						PLeft = AL;
						PWidth = AW;
						if (MyFooter!=NULL)
						{
							FLOAT FooterAT = MyFooter->ActualTop();
							PHeight = bDrawTabAbove
								? FooterAT - BottomRow : FooterAT - BottomRow - AT;
						}
						else
						{
							// This code insures that the panel is drawn correctly regardless of whether the panel's dimensions fall
							// within this tabcontrol's, or outside of this tabcontrol's
							PHeight = bDrawTabAbove
								? (AH - (BottomRow - AT) > AH ? (AH - (BottomRow - AT)) : (MenuOwner->ActualHeight() - (BottomRow - MenuOwner->ActualTop()))) : BottomRow - AT;
						}

						Button->MyPanel->SetAdjustedDims( PWidth, PHeight, PLeft, PTop+1 );
					}

					Button->MyPanel->SaveCanvasState(Canvas);
					Button->MyPanel->PreDraw(Canvas);
					Button->MyPanel->RestoreCanvasState(Canvas);
				}
			}
		}
	}

	unguardobj;
}


void UGUITabControl::Draw(UCanvas* Canvas)
{
	guard(GUITabControl::Draw);

	if (!bVisible || INVALIDRENDER )
		return;

	UGUIController* C = Controller;

	UGUIComponent::Draw(Canvas);

	if (BackgroundStyle!=NULL)
	{
		if ( (MenuState==MSAT_Focused) && (FocusedControl==NULL) )
			BackgroundStyle->Draw(Canvas,MenuState, Bounds[0], Bounds[1], Bounds[2] - Bounds[0], Bounds[3] - Bounds[1]);
		else
			BackgroundStyle->Draw(Canvas,MSAT_Blurry, Bounds[0], Bounds[1], Bounds[2] - Bounds[0], Bounds[3] - Bounds[1]);
	}

	if (BackgroundImage!=NULL)
		Canvas->DrawTileScaleBound(BackgroundImage, Bounds[0], Bounds[1], Bounds[2] - Bounds[0], Bounds[3] - Bounds[1] );

	for (INT i=0;i<TabStack.Num();i++)
	{
		TabStack(i)->SaveCanvasState(Canvas);
#if UCONST_Counter == 0
		if (OBJ_DELEGATE_IS_SET(TabStack(i),OnDraw) )
		{
			if ( !TabStack(i)->delegateOnDraw(Canvas) )
				TabStack(i)->Draw(Canvas);
		}
		else
			TabStack(i)->Draw(Canvas);
#else
		if ( !TabStack(i)->delegateOnDraw(Canvas) )
			TabStack(i)->Draw(Canvas);
#endif

		TabStack(i)->RestoreCanvasState(Canvas);
	}

	FLOAT CurFade(0.f);
	if ( FadeInTime > 0.f && PendingTab && PendingTab->MyPanel )
	{
		FadeInTime -= C->RenderDelta;
		if ( FadeInTime > 0.f )
			CurFade = FadeInTime / PendingTab->MyPanel->FadeInTime;
		else
		{
			FadeInTime = 0.f;
			eventMakeTabActive(PendingTab);
		}
	}

	if ( ActiveTab )
	{
		if ( ActiveTab->MyPanel )
		{
			if ( CurFade > 0.f )
			{
				Canvas->ForcedAlpha = CurFade;
				Canvas->bForceAlpha = 1;
			}
#if UCONST_Counter == 0
			if ( OBJ_DELEGATE_IS_SET(ActiveTab->MyPanel,OnDraw) )
			{
				if (!ActiveTab->MyPanel->delegateOnDraw(Canvas) )
					ActiveTab->MyPanel->Draw(Canvas);
			}
			else
				ActiveTab->MyPanel->Draw(Canvas);
			if ( OBJ_DELEGATE_IS_SET(ActiveTab->MyPanel,OnRendered) )
#else
			if ( !ActiveTab->MyPanel->delegateOnDraw(Canvas) )
				ActiveTab->MyPanel->Draw(Canvas);
#endif
				ActiveTab->MyPanel->delegateOnRendered(Canvas);

			if ( CurFade > 0.f )
			{
				Canvas->ForcedAlpha = 1.f - CurFade;
				Canvas->bForceAlpha = 1;
			}

		}


		if ( PendingTab )
		{
			if ( PendingTab->MyPanel )
			{

#if UCONST_Counter == 0
				if ( OBJ_DELEGATE_IS_SET(PendingTab->MyPanel,OnDraw) )
				{
					if (!PendingTab->MyPanel->delegateOnDraw(Canvas) )
						PendingTab->MyPanel->Draw(Canvas);
				}
				else
					PendingTab->MyPanel->Draw(Canvas);

				if ( OBJ_DELEGATE_IS_SET(PendingTab->MyPanel,OnRendered) )
					PendingTab->MyPanel->delegateOnRendered(Canvas);
#else
				if ( !PendingTab->MyPanel->delegateOnDraw(Canvas) )
					PendingTab->MyPanel->Draw(Canvas);

				PendingTab->MyPanel->delegateOnRendered(Canvas);
#endif
			}

			Canvas->bForceAlpha=0;
			Canvas->ForcedAlpha=255;
		}

		ActiveTab->RestoreCanvasState(Canvas);	
	}

	unguardobj;
}
				 
UGUIComponent* UGUITabControl::UnderCursor(FLOAT MouseX, FLOAT MouseY)
{

	guard(UGUITabControl::UnderCursor);
	UGUIComponent* tComp(NULL);

	if ( ActiveTab )
	{
		tComp = ActiveTab->UnderCursor(MouseX,MouseY);
		if ( tComp )
			return tComp;
	}

	for ( INT i = TabStack.Num() - 1; i >= 0; i-- )
	{
		if ( TabStack(i) && TabStack(i) != ActiveTab )
		{
			tComp = TabStack(i)->UnderCursor(MouseX, MouseY);
			if ( tComp )
				return tComp;
		}
	}

	return NULL;

	unguardobj;
}

UBOOL UGUITabControl::SpecialHit( UBOOL bForce )
{
	guard(UGUITabControl::SpecialHit);

	if (UGUIComponent::SpecialHit(bForce))
		return true;

	if ( (ActiveTab!=NULL) && (ActiveTab->MyPanel!=NULL) )
		return ActiveTab->MyPanel->SpecialHit(bForce);

	return false;
	unguardobj;
}

UBOOL UGUITabControl::MousePressed(UBOOL IsRepeat)
{
	guard(UGUITabControl::MousePressed);

	if (ActiveTab && ActiveTab->MyPanel !=NULL)
	{
		if ( ActiveTab->MyPanel->MousePressed(IsRepeat) )
			return true;
	}

	for (INT i=0;i<TabStack.Num();i++)
	{
		if ( TabStack(i) != ActiveTab && TabStack(i)->MousePressed(IsRepeat) )
			return true;
	}

	if ( UGUIComponent::MousePressed(IsRepeat) )
		return true;

	return false;

	unguardobj;

}

UBOOL UGUITabControl::MouseReleased()
{
	guard(UGUITabControl::MouseReleased);

	if (ActiveTab && ActiveTab->MyPanel !=NULL)
	{
		if ( ActiveTab->MyPanel->MouseReleased() )
			return true;
	}

	for (INT i=0;i<TabStack.Num();i++)
	{
		if ( TabStack(i) != ActiveTab && TabStack(i)->MouseReleased() )
			return true;
	}

	if ( UGUIComponent::MouseReleased() )
		return true;

	return false;

	unguardobj;
}

UBOOL UGUITabControl::IsFocusedOn( const UGUIComponent* Comp ) const
{
	guard(UGUITabControl::IsFocusedOn);

	const UGUITabButton* TabComp = ConstCast<UGUITabButton>(Comp);
	if ( TabComp )
	{
		if ( Comp == ActiveTab )
			return true;

		if ( Controller && Comp == Controller->ActiveControl )
			return true;
	}
	return Super::IsFocusedOn(Comp);
	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUITabButton
// =======================================================================================================================================================
// =======================================================================================================================================================

UBOOL UGUITabButton::MousePressed(UBOOL IsRepeat)	
{
	guard(GUITabButton::MousePressed);

	// Attempt to SetFocus to this control
	if ( !Super::MousePressed(IsRepeat) )
		return false;

	return true;

	unguardobj;
}		

UBOOL UGUITabButton::MouseReleased()
{
	guard(GUITabButton::MouseReleased);

	if ( !Super::MouseReleased() )
		return false;

	return true;
	unguardobj;
}

void UGUITabButton::Draw(UCanvas* Canvas)
{
	guard(UGUITabButton::Draw);

	if ( INVALIDRENDER )
		return;

	BYTE CurrentState = MenuState;

	if(bForceFlash)
		MenuState = MSAT_Watched;

	else if ( MenuOwner->GetFocused() == MyPanel )
		MenuState = MSAT_Focused;

	else if ( Cast<UGUITabControl>(MenuOwner)->ActiveTab == this )
		MenuState = MSAT_Focused;

	Super::Draw(Canvas);
// JOE - This is handled in GUIComponet::Draw Now
//	if (DELEGATE_IS_SET(OnRender))
//		delegateOnRender(Canvas);

	MenuState = CurrentState;

	unguardobj;
}

UGUIComponent* UGUITabButton::UnderCursor(FLOAT MouseX, FLOAT MouseY)
{
	guard(UGUITabButton::UnderCursor);

	if ( Super::UnderCursor(MouseX,MouseY) )
		return this;

	if ( bActive && MyPanel )
	{
		UGUIComponent* tComp = MyPanel->UnderCursor(MouseX,MouseY);
		if ( tComp )
			return tComp;
	}

	return NULL;
	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUILabel
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUILabel::Draw(UCanvas* Canvas)
{
	guard(GUILabel::Draw);

	if (!bVisible || INVALIDRENDER )
		return;

	// Pass it along;

	Super::Draw(Canvas);

	// Grab the font

	if (!Style)
	{
	    UGUIFont* MyFont = Controller->eventGetMenuFont(TextFont);
	    if (MyFont)
	    {
		    UFont* CFont = MyFont->eventGetFont(Canvas->SizeX);
		    if (CFont==NULL)	
			    return;
    
		    Canvas->Font = CFont;
    
	    }
	    else
		    return;
	}

	if (!bTransparent)  // Draw the Background if required
	{
		if (Style==NULL)
			Canvas->DrawTile(Controller->DefaultPens[0], ActualLeft(), ActualTop(), ActualWidth(), ActualHeight(),0,0,32,32, 0.0f, BackColor,FPlane(0,0,0,0));
		else
			Style->Draw(Canvas, MenuState, ActualLeft(), ActualTop(), ActualWidth(), ActualHeight());
	}

	if (MenuState==MSAT_Disabled)
		Canvas->ColorModulate *= FPlane(0.5,0.5,0.5,1.0);

	UBOOL bRenderShadow = ((ShadowOffsetX != 0) || (ShadowOffsetY != 0));
	UBOOL bRenderHilight = ((HilightOffsetX != 0) || (HilightOffsetY != 0));

	if (!bMultiLine)
	{
		if (Style==NULL)
		{	
			Canvas->Style=MSTY_Alpha;
			if (bRenderShadow)
			{
				Canvas->Color=ShadowColor;
				Canvas->DrawTextJustified(TextAlign, 
					appRound(ActualLeft()+ShadowOffsetX), 
					appRound(ActualTop()+ShadowOffsetY), 
					appRound(ActualLeft()+ActualWidth()+ShadowOffsetX), 
					appRound(ActualTop()+ActualHeight()+ShadowOffsetY), 
					TEXT("%s"),
					*Caption, FontScale);
			}

			if (bRenderHilight)
			{
				Canvas->Color=HilightColor;
				Canvas->DrawTextJustified(TextAlign, 
					appRound(ActualLeft()-HilightOffsetX), 
					appRound(ActualTop()-HilightOffsetY), 
					appRound(ActualLeft()+ActualWidth()-HilightOffsetX), 
					appRound(ActualTop()+ActualHeight()-HilightOffsetY), 
					TEXT("%s"),
					*Caption, FontScale);
			}

			if (MenuState==MSAT_Focused)
				Canvas->Color=FocusedTextColor;
			else
				Canvas->Color=TextColor;

			Canvas->Style=TextStyle;
			
			Canvas->DrawTextJustified(TextAlign, 
				appRound(ActualLeft()), 
				appRound(ActualTop()), 
				appRound(ActualLeft()+ActualWidth()), 
				appRound(ActualTop()+ActualHeight()), 
				TEXT("%s"),
				*Caption, FontScale);
		}
		else {
			Canvas->Style = MSTY_Alpha;
			if (bRenderShadow)
			{
				Canvas->Color=ShadowColor;
				Canvas->DrawTextJustified(TextAlign, 
					appRound(ActualLeft()+ShadowOffsetX), 
					appRound(ActualTop()+ShadowOffsetY), 
					appRound(ActualLeft()+ActualWidth()+ShadowOffsetX), 
					appRound(ActualTop()+ActualHeight()+ShadowOffsetY), 
					TEXT("%s"),
					*Caption, FontScale);
			}
			if (bRenderHilight)
			{
				Canvas->Color=HilightColor;
				Canvas->DrawTextJustified(TextAlign, 
					appRound(ActualLeft()-HilightOffsetX), 
					appRound(ActualTop()-HilightOffsetY), 
					appRound(ActualLeft()+ActualWidth()-HilightOffsetX), 
					appRound(ActualTop()+ActualHeight()-HilightOffsetY), 
					TEXT("%s"),
					*Caption, FontScale);
			}
			Style->DrawText(Canvas, MenuState, ActualLeft(), ActualTop(), ActualWidth(), ActualHeight(), TextAlign, *Caption, FontScale);
		}
	}
	else	// Here we handle MultiLine Labels
	{
	TArray<FString> Lines;
	FLOAT XL, YL;
	FLOAT Left = 0.f, Top = 0.f, Width = 0.f, Bottom = 0.f;

		// TODO: Use Bounds or ClientBounds ???
		Left = ClientBounds[0];
		Width = ClientBounds[2] - ClientBounds[0];
		Bottom = ClientBounds[3];
			
		// TODO: Make Split Char configurable in GUILabel
		// First get the Height of the font
		Canvas->ClippedStrLen(NULL, 1.0, 1.0, XL, YL, TEXT("W"));
		Canvas->WrapStringToArray(*Caption, &Lines, Width, NULL, '|');
		if (Style==NULL)
		{

			FLOAT TextHeight = 0.f, Middle = 0.f;
			switch (VertAlign)
			{
			case TXTA_Left:
				Top = ClientBounds[1];
				break;
			case TXTA_Center:
				TextHeight = YL * Lines.Num();
				Middle = ClientBounds[1] + ((Bottom - ClientBounds[1]) / 2);
				Top = Middle - (TextHeight / 2.f);
				break;

			case TXTA_Right:
				TextHeight = YL * Lines.Num();
				Top = Bottom - TextHeight;
				break;
			}

			for (INT i=0; i<Lines.Num(); i++)
			{
				Canvas->Style = MSTY_Alpha;
				if (bRenderShadow)
				{
					Canvas->Color=ShadowColor;
					Canvas->DrawTextJustified(TextAlign, 
						appRound(Left + ShadowOffsetX), 
						appRound(Top + ShadowOffsetY), 
						appRound(Left + Width + ShadowOffsetX), 
						appRound(Top + YL + ShadowOffsetY), 
						TEXT("%s"),
						*Lines(i), FontScale);
				}

				if (bRenderHilight)
				{
					Canvas->Color=HilightColor;
					Canvas->DrawTextJustified(TextAlign, 
						appRound(Left - HilightOffsetX), 
						appRound(Top - HilightOffsetY), 
						appRound(Left + Width - HilightOffsetX), 
						appRound(Top + YL - HilightOffsetY), 
						TEXT("%s"),
						*Lines(i), FontScale);
				}
				Canvas->Color=TextColor;
				Canvas->Style=TextStyle;

				Canvas->DrawTextJustified(TextAlign, 
					appRound(Left), 
					appRound(Top), 
					appRound(Left + Width), 
					appRound(Top + YL), 
					TEXT("%s"),
					*Lines(i), FontScale);

				Top += YL;
				if ((Top+YL) > Bottom)
					break;
			}
		}
		else
		{
			FLOAT Middle = 0.f, TextHeight = 0.f;
			switch (VertAlign)
			{
			case TXTA_Left:
				Top = ClientBounds[1];
				break;
			case TXTA_Center:
				TextHeight = YL * Lines.Num();
				Middle = ClientBounds[1] + ((ClientBounds[3] - ClientBounds[1]) / 2);
				Top = Middle - (TextHeight / 2.f);
				break;

			case TXTA_Right:
				TextHeight = YL * Lines.Num();
				Top = ClientBounds[3] - TextHeight;
				break;
			}

			for (INT i=0; i<Lines.Num(); i++)
			{
				FColor tmpColor = Canvas->Color;
				Canvas->Style = MSTY_Alpha;
				if (bRenderShadow)
				{
					Canvas->Color=ShadowColor;
					Canvas->DrawTextJustified(TextAlign, 
						appRound(Left + ShadowOffsetX), 
						appRound(Top + ShadowOffsetY), 
						appRound(Left + Width + ShadowOffsetX), 
						appRound(Top + YL + ShadowOffsetY), 
						TEXT("%s"),
						*Lines(i), FontScale);
				}

				if (bRenderHilight)
				{
					Canvas->Color=HilightColor;
					Canvas->DrawTextJustified(TextAlign, 
						appRound(Left - HilightOffsetX), 
						appRound(Top - HilightOffsetY), 
						appRound(Left + Width - HilightOffsetX), 
						appRound(Top + YL - HilightOffsetY), 
						TEXT("%s"),
						*Lines(i), FontScale);
				}

				Style->DrawText(Canvas, MenuState, Left, Top, Width, YL, TextAlign, *Lines(i), FontScale);
				Top += YL;
				if ((Top+YL) > Bottom)
					break;
			}
		}
	}

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIBorder
// =======================================================================================================================================================
// =======================================================================================================================================================
void UGUIBorder::Draw( UCanvas* Canvas )
{
	guard(UGUIBorder::Draw);

	if ( INVALIDRENDER )
		return;

	UGUIComponent::Draw(Canvas);
	Style->Draw(Canvas, MenuState, Bounds[0], Bounds[1], Bounds[2] - Bounds[0], Bounds[3] - Bounds[1]);
	DrawControls(Canvas);

	// Draw the caption
	FLOAT Left = ClientBounds[0];
	FLOAT Top = ClientBounds[1];
	FLOAT Width = ClientBounds[2] - ClientBounds[0];
	FLOAT Height = ClientBounds[3] - ClientBounds[1];

	if ( TextIndent > 0 )
	{
		if ( Justification == TXTA_Left )
			Left += TextIndent;
		else if ( Justification == TXTA_Right )
			Left -= TextIndent;
	}

	Style->DrawText(Canvas, MenuState, Left, Top, Width, Height, Justification, *Caption, FontScale );

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUITitleBar
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUITitleBar::PreDraw(UCanvas* Canvas)
{
	guard(GUITitleBar::PreDraw);

	// Calculate the Height

	if ( INVALIDRENDER )
		return;

	if (bUseTextHeight)
	{
		FLOAT XL,YL;
		Style->TextSize(Canvas, 0, TEXT("QWz,1"),XL,YL,FontScale);
		WinHeight = RelativeHeight( YL + Style->BorderOffsets[1] + Style->BorderOffsets[3], 1 );
	}

	UGUIComponent::PreDraw(Canvas);
	if (DockedTabs!=NULL)
	{
		// Make sure to adjust the winheight to actual pixels before setting bNeverScale
		if (bDockTop)
			DockAlign = PGA_Top;

		DECLAREBOUNDS;
		FLOAT TT(DockedTabs->ActualTop()), TL(DockedTabs->ActualLeft()), TW(DockedTabs->ActualWidth()), TH(DockedTabs->ActualHeight());

		switch (DockAlign)
		{
			case PGA_Right:
			case PGA_Left:

				if (DockAlign==PGA_Left)
					TL = AL;
				else
					TL = AL + AW - TW;

				TT = AT + AH;
				break;

			case PGA_Top:	
				TT = AT + AH;
				break;

			case PGA_Client:
				TL = AL;
				TW = AW;
				TT = AT + AH;
				break;

			case PGA_None:
				break;

			default:
				TL = AL + Style->BorderOffsets[0];
				TW = AW - Style->BorderOffsets[0] - Style->BorderOffsets[2];
				TT = AT + AH - 2;
				break;
		}

		DockedTabs->SetAdjustedDims( TW, TH, TL, TT );
	}

	PreDrawControls(Canvas);
	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIFooter
// =======================================================================================================================================================
// =======================================================================================================================================================
#if 0
void UGUIFooter::Draw(UCanvas* Canvas)
{
	guard(UGUIFooter::Draw);

	if ( INVALIDRENDER )
		return;

	UGUIComponent::Draw(Canvas);
	Style->Draw(Canvas, 0, ActualLeft(), ActualTop(), ActualWidth(), ActualHeight() );
	DrawControls(Canvas);

	unguard;
}
#endif
// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIMenuOption
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIMenuOption::PreDraw(UCanvas* Canvas)
{
	guard(GUIMenuOption::PreDraw);

	if ( (MyLabel==NULL) || (MyComponent==NULL) || INVALIDRENDER )
		return;

	FLOAT AL, AT, AH, AW;

	UGUIComponent::PreDraw(Canvas);

	AL = ActualLeft();
	AT = ActualTop();
	AH = ActualHeight();
	AW = ActualWidth();

	if (bVerticalLayout)
	{
		// This is a quickly hacked in vertical split MenuOption
		// Dehacked -- rjp
		if (CaptionWidth <= 1.f)
			MyLabel->WinHeight = Max<FLOAT>(AH * CaptionWidth,0.f);
		else
			MyLabel->WinHeight = CaptionWidth;

		MyLabel->WinLeft = AL;
		MyLabel->WinWidth = AW;

		if (bFlipped)
			MyComponent->WinTop = AT;
		else
			MyLabel->WinTop = AT;

		// Figure out MyComponent new Height
		if (!bSquare)
		{
			MyComponent->WinWidth = Max<FLOAT>(MyLabel->WinWidth,0.f);
			if (ComponentWidth == -1.f)
				MyComponent->WinHeight = Max<FLOAT>(AH - MyLabel->ActualHeight(),0.f);
			else if ( ComponentWidth > 0 )
				MyComponent->WinHeight = Max<FLOAT>(AH * ComponentWidth,0.f);
			else ComponentWidth = 0;
		}
		else
		{
			if (ComponentWidth == -1.f)
				MyComponent->WinHeight = Max<FLOAT>(AH - MyLabel->ActualHeight(),0.f);
			else MyComponent->WinHeight = Max<FLOAT>(ComponentWidth >= 0 && ComponentWidth <= 1 ? AH * ComponentWidth : ComponentWidth,0.f);
			MyComponent->WinWidth = MyComponent->WinHeight;
		}

		if (bFlipped)
			MyLabel->WinTop = Max<FLOAT>(AT + MyComponent->WinHeight,0.f);
		else
            MyComponent->WinTop = Max<FLOAT>(AT + MyLabel->WinHeight,0.f);

		MyComponent->WinLeft = MyLabel->WinLeft;
	}
	else
	{
		if ( CaptionWidth <= 1.f )
			MyLabel->WinWidth = Max<FLOAT>(AW * CaptionWidth,0.f);
		else
			MyLabel->WinWidth = CaptionWidth;

		if (bAutoSizeCaption)
		{
			FLOAT XL, YL;
			XL = YL = 0;
			if ( MyLabel->Style )
				MyLabel->Style->TextSize( Canvas, MenuState, *MyLabel->Caption, XL, YL, MyLabel->FontScale );
			else Canvas->ClippedStrLen(Canvas->Font, (INT)MyLabel->FontScale / 2, (INT)MyLabel->FontScale / 2, XL, YL, *MyLabel->Caption);

			if (XL > MyLabel->WinWidth && XL + XL * 0.05 < ActualWidth())
				MyLabel->WinWidth = Max<FLOAT>(XL + XL * 0.05, 0.f);
		}

		MyLabel->WinHeight = AH;
		MyLabel->WinTop = AT;

		MyComponent->WinHeight = AH;
	
		if (!bSquare)
		{
			if (ComponentWidth == -1.f)
				MyComponent->WinWidth = Max<FLOAT>(AW - MyLabel->ActualWidth(), 0.f);
			else if (ComponentWidth > 0)
				MyComponent->WinWidth = Max<FLOAT>(AW * ComponentWidth, 0.f);
			else MyComponent->WinWidth = 0.f;
		}
		else	
			MyComponent->WinWidth = Max<FLOAT>(MyComponent->WinHeight,0.f);
	
		MyComponent->WinTop = AT;


		// Figure out the Lefts.
	
		if (bFlipped)
		{
			MyLabel->WinLeft = Max<FLOAT>(AL + MyComponent->WinWidth,0.f);
	
			if (ComponentJustification==TXTA_Left)
				MyComponent->WinLeft = AL;
			
			else 
				MyComponent->WinLeft = MyLabel->WinLeft - MyComponent->WinWidth;
	
		}
		else
		{
			MyLabel->WinLeft = AL;
	
			if (ComponentJustification==TXTA_Left)
				MyComponent->WinLeft = AL + MyLabel->WinWidth;
			
			else 
				MyComponent->WinLeft = AL + AW - MyComponent->WinWidth;
		}	
	}

	PreDrawControls(Canvas);
	unguardobj;
}

UBOOL UGUIMenuOption::MousePressed( UBOOL IsRepeat )
{
	guard(UGUIMenuOption::MousePressed);

	if ( Super::MousePressed(IsRepeat) )
		return true;

	if ( MenuOwner && Cast<UGUIMultiComponent>(MenuOwner) )
		return UGUIComponent::MousePressed( IsRepeat );

	return false;
	unguardobj;
}

UBOOL UGUIMenuOption::MouseReleased()
{
	guard(UGUIMenuOption::MouseReleased);

	if ( Super::MouseReleased() )
		return true;

	if ( MenuOwner && Cast<UGUIMultiComponent>(MenuOwner) )
		return UGUIComponent::MouseReleased();

	return false;

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIButton
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIButton::PreDraw(UCanvas* Canvas)
{
	guard(UGUIButton::PreDraw);

	if ( INVALIDRENDER )
		return;

	if ( Style && Style->Fonts[MenuState + (5 * FontScale)] )
	{
		FLOAT XL(0.f), YL(0.f);
		Canvas->ClippedStrLen(Style->Fonts[MenuState+(5*FontScale)]->eventGetFont(Canvas->ClipX), 1, 1, XL, YL, SizingCaption != TEXT("") ? *SizingCaption : *Caption);

		if ( bAutoSize && (XL > ActualWidth() || bAutoShrink))
			SetAdjustedDims( XL + (XL * AutoSizePadding.HorzPerc),  bUseCaptionHeight || AutoSizePadding.VertPerc > 0.f ? YL + (YL * AutoSizePadding.VertPerc) : ActualHeight(), ActualLeft(), ActualTop() );
		else if ( bUseCaptionHeight )
			WinHeight = RelativeHeight(YL);
	}

	Super::PreDraw(Canvas);
	unguardobj;
}

void UGUIButton::Draw(UCanvas* Canvas)
{
	guard(GUIButton::Draw);

	if ( !bVisible || INVALIDRENDER )
		return;

	Super::Draw(Canvas);

//	Joe - This is handled in the MultiComp that owns this button
//	if (delegateOnDraw(Canvas))
//		return;

	// Draw the button using it's style object
	if ( Style )
		Style->Draw(Canvas, MenuState, Bounds[0], Bounds[1], Bounds[2] - Bounds[0], Bounds[3] - Bounds[1]);

	if ( Caption == TEXT("") )
		return;

	if (CaptionEffectStyle!=NULL)
	{

		// Draw a drop shadow fading out to the right

		FLOAT OldAlpha = CaptionEffectStyle->FontColors[MenuState].A;
		for (INT xx=1;xx<4;xx++)
			for (INT yy=1;yy<4;yy++)
			{
				CaptionEffectStyle->DrawText(Canvas, MenuState, Bounds[0]+xx,Bounds[1]+yy,Bounds[2] - Bounds[0],Bounds[3] - Bounds[1], CaptionAlign, *Caption, FontScale );
				CaptionEffectStyle->FontColors[MenuState].A *= 0.75;
			}

		CaptionEffectStyle->FontColors[MenuState].A = OldAlpha;
	}

	if ( Style && Style->Fonts[MenuState + (5 * FontScale)] )
	{
		if ( !bAutoSize && bWrapCaption )
		{
			TArray<FString> Lines;
			FLOAT XL(0.f),YL(0.f), Y(ClientBounds[1]);

			UFont* Font = Style->Fonts[MenuState+(5*FontScale)]->eventGetFont(Controller->ResX);

			Canvas->ClippedStrLen(Font, 1, 1, XL, YL, *Caption);
			Canvas->WrapStringToArray( *Caption, &Lines, ClientBounds[2] - ClientBounds[0], Font );
			FLOAT TotalYL = YL * Lines.Num(), ClientHeight = ClientBounds[3] - ClientBounds[1];
			if ( TotalYL < ClientHeight )
				Y += ClientHeight / 2 - TotalYL / 2 ;

			for ( INT i = 0; i < Lines.Num(); i++ )
			{
				Style->DrawText(Canvas, MenuState, ClientBounds[0], Y, ClientBounds[2] - ClientBounds[0], YL, CaptionAlign, *Lines(i), FontScale);
				Y += YL;
			}
		}

		else Style->DrawText(Canvas, MenuState, ClientBounds[0], ClientBounds[1], ClientBounds[2] - ClientBounds[0], ClientBounds[3] - ClientBounds[1], CaptionAlign, *Caption, FontScale );
	}

	unguardobj;
}
// =======================================================================================================================================================
// =======================================================================================================================================================
// UStateButton
// =======================================================================================================================================================
// =======================================================================================================================================================


void UStateButton::Draw(UCanvas* Canvas)
{
	guard(StateButton::Draw);

	if ( !bVisible || INVALIDRENDER )
		return;

	// Skip the last
	
	UGUIComponent::Draw(Canvas);

	UMaterial* Graphic = Images[MenuState];

	if (Graphic==NULL)
		return;

	// Figure out the X pixels = Y % of screen so we can scale the border

	FLOAT mW = Graphic->MaterialUSize();
	FLOAT mH = Graphic->MaterialVSize();

	FLOAT cW, cH, CX, CY;
	cW = ActualWidth();
	cH = ActualHeight();
	CX = ActualLeft();
	CY = ActualTop();

	if (MenuState!=MSAT_Focused)
		Canvas->Color = FColor(255,255,255,255);
	else
		Canvas->Color = FColor(255,255,0,255);

	if (ImageStyle==ISTY_Stretched)
		Canvas->DrawTileStretchedOrScaled(Graphic, CX, CY, cW, cH, INDEX_NONE, INDEX_NONE );			

	else if (ImageStyle==ISTY_Scaled )
		Canvas->DrawTileScaleBound(Graphic, CX, CY, cW, cH ) ;			

	else if (ImageStyle==ISTY_Bound)
		Canvas->DrawTileBound(Graphic, CX, CY, cW, cH ) ;			

	else
		Canvas->DrawTile(Graphic, CX, CY, mW, mH, 0, 0, mW, mH, 0, Canvas->Color, FPlane(0,0,0,0));
	

	unguardobj;
}


// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIGFXButton
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIGFXButton::Draw(UCanvas* Canvas)
{
	guard(GUIGFXButton::Draw);

	if (!bVisible || INVALIDRENDER )
		return;

	// Draw the button

	Super::Draw(Canvas);

	if ( (Graphic==NULL) || ((bCheckBox) && (!bChecked)) )
		return;

	// Figure out the X pixels = Y % of screen so we can scale the border

	FLOAT mW = Graphic->MaterialUSize();
	FLOAT mH = Graphic->MaterialVSize();

	if (bCheckBox)
	{
		if (MenuState==MSAT_Disabled)
			Canvas->Color = FColor(200,200,200,180);
		else
			Canvas->Color = FColor(255,255,255,255);

		Canvas->Style = 5;
	}
	FLOAT cW, cH, CX, CY;
	if (bClientBound)
	{
		cW = ClientBounds[2] - ClientBounds[0];
		cH = ClientBounds[3] - ClientBounds[1];
		CX = ClientBounds[0];
		CY = ClientBounds[1];
	}
	else
	{
		cW = ActualWidth();
		cH = ActualHeight();
		CX = ActualLeft();
		CY = ActualTop();
	}
	FLOAT dW = Min<FLOAT>(cW, mW);
	FLOAT dH = Min<FLOAT>(cH, mH);

	// MC: Clip to ClientBounds
	if (Position==ICP_Center)
		Canvas->DrawTile(Graphic, CX + ((cW - dW)/2), CY + ((cH - dH)/2), dW, dH, (mW-dW)/2, (mH-dH)/2, dW, dH, 0, Canvas->Color, FPlane(0,0,0,0));

	else if (Position==ICP_Stretched)
		Canvas->DrawTileStretchedOrScaled(Graphic, CX, CY, cW, cH, INDEX_NONE, INDEX_NONE );			

	else if (Position==ICP_Scaled)
		Canvas->DrawTileScaleBound(Graphic, CX, CY, cW, cH ) ;			

	else if (Position==ICP_Bound)
		Canvas->DrawTileBound(Graphic, CX, CY, cW, cH ) ;			

	else
		Canvas->DrawTile(Graphic, CX, CY, dW, dH, 0, 0, mW, mH, 0, Canvas->Color, FPlane(0,0,0,0));

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUICheckBoxButton
// =======================================================================================================================================================
// =======================================================================================================================================================
void UGUICheckBoxButton::Draw( UCanvas* Canvas )
{
	guard(UGUICheckBoxButton::Draw);

	if ( !bVisible || INVALIDRENDER)
		return;

	UGUIButton::Draw(Canvas);

	FLOAT mW = CheckedOverlay[MenuState]->MaterialUSize();
	FLOAT mH = CheckedOverlay[MenuState]->MaterialVSize();
	INT Index;

	if (bAllOverlay)
	{
		if ( !CheckedOverlay[MenuState] || !CheckedOverlay[MenuState+5] )
			return;
		
		Index = bChecked?MenuState:MenuState+5;

	}
	else
		Index = MenuState;

	if ( !bAllOverlay && (!bChecked || !CheckedOverlay[Index] ) )
		return;

	// Figure out the X pixels = Y % of screen so we can scale the border

	Canvas->Style = 5;

	FLOAT cW, cH, CX, CY;
	if (bClientBound)
	{
		cW = ClientBounds[2] - ClientBounds[0];
		cH = ClientBounds[3] - ClientBounds[1];
		CX = ClientBounds[0];
		CY = ClientBounds[1];
	}
	else
	{
		cW = ActualWidth();
		cH = ActualHeight();
		CX = ActualLeft();
		CY = ActualTop();

		// Now this is a big fricken hack

		if (bAllOverlay)
		{
			cW-=8;
			cH-=8;
			CX+=4;
			CY+=4;
		}

	}
	FLOAT dW = Min<FLOAT>(cW, mW);
	FLOAT dH = Min<FLOAT>(cH, mH);

	Canvas->Color=FPlane(255,255,255,255);

	// MC: Clip to ClientBounds
	if (Position==ICP_Center)
		Canvas->DrawTile(CheckedOverlay[Index], CX + ((cW - dW)/2), CY + ((cH - dH)/2), dW, dH, (mW-dW)/2, (mH-dH)/2, dW, dH, 0, Canvas->Color, FPlane(0,0,0,0));

	else if (Position==ICP_Stretched)
		Canvas->DrawTileStretchedOrScaled(CheckedOverlay[Index], CX, CY, cW, cH, INDEX_NONE, INDEX_NONE ) ;			

	else if (Position==ICP_Scaled)
		Canvas->DrawTileScaleBound(CheckedOverlay[Index], CX, CY, cW, cH ) ;			

	else if (Position==ICP_Bound)
		Canvas->DrawTileBound(CheckedOverlay[Index], CX, CY, cW, cH ) ;			

	else
		Canvas->DrawTile(CheckedOverlay[Index], CX, CY, dW, dH, 0, 0, dW, dH, 0, Canvas->Color, FPlane(0,0,0,0));

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIEditBox
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIEditBox::Draw(UCanvas* Canvas)
{
	guard(GUIEditBox::Draw);

	if ( !bVisible || INVALIDRENDER )
		return;

	Super::Draw(Canvas);

	FString Storage,FinalDraw;
	Storage = TextStr;

	if ( (bMaskText) && (Storage.Len()>0) )
	{
		for (INT MaskIndex=0;MaskIndex<Storage.Len();MaskIndex++)
			Storage[MaskIndex] = *TEXT("#");
	}

	INT OldClipX = Canvas->ClipX;
	INT OldClipY = Canvas->ClipY;

	Canvas->Color = Style->FontColors[MenuState];

	INT BoxWidth=ClientBounds[2]-ClientBounds[0];
	FLOAT XL,YL;
	
	if ( (Storage.Len() != LastLength) || (CaretPos!=LastCaret) )
	{
		// Recaculate FirstVis

		if (CaretPos<=FirstVis)
			FirstVis = Max(0,CaretPos-1);
		else
		{
			FinalDraw = Storage.Mid(FirstVis,CaretPos-FirstVis);
			Style->TextSize(Canvas,MenuState,*FinalDraw,XL,YL,FontScale);

			while ( (XL>BoxWidth) && (FirstVis<Storage.Len()) )
			{
				FirstVis++;
				FinalDraw = Storage.Mid(FirstVis,CaretPos-FirstVis);
				Style->TextSize(Canvas,MenuState,*FinalDraw,XL,YL,FontScale);
			}
		}

	}
	LastLength = Storage.Len();

	if (bReadOnly)
		FirstVis = 0;

	FinalDraw = Storage.Mid(FirstVis,Storage.Len()-FirstVis);

	// Display Cursor/select-all block. Behind text.

	if (!bReadOnly && bHasFocus)
	{

		if ( (FirstVis==CaretPos) || (FinalDraw.Len()==0) )
		{
			Canvas->ClippedStrLen( Canvas->Font, 1, 1, XL, YL, TEXT("W") );
			XL = 0;
			bAllSelected=false;
		}
		else
		{
			FString TmpString = FinalDraw.Mid(0,CaretPos-FirstVis);
			Canvas->ClippedStrLen( Canvas->Font, 1, 1, XL, YL, *TmpString );
		}

		FLOAT CursorY = ActualTop() + (ActualHeight()/2) - (YL/2);

		Canvas->Style=5;	// Alpha

		if(bAllSelected)
			Canvas->DrawTile(Controller->DefaultPens[0],ClientBounds[0],CursorY,XL,YL,0,0,3,1,0.0,FColor(255,255,255,0.5f * Controller->CursorFade),FPlane(0,0,0,0));
		else
			Canvas->DrawTile(Controller->DefaultPens[0],ClientBounds[0]+XL,CursorY,3,YL,0,0,3,1,0.0,FColor(255,255,255,Controller->CursorFade),FPlane(0,0,0,0));
	}


	Canvas->CurX   = ClientBounds[0];
	Canvas->CurY   = Bounds[1];
	Canvas->ClipX  = ClientBounds[2];
	Canvas->ClipY  = Bounds[3];

	Style->DrawText(Canvas, MenuState, ClientBounds[0], Bounds[1], ClientBounds[2]-ClientBounds[0], Bounds[3] - Bounds[1], 0, *FinalDraw, FontScale);

	Canvas->ClipX = OldClipX;
	Canvas->ClipY = OldClipY;

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUINumericEdit
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUINumericEdit::PreDraw(UCanvas* Canvas)
{
	guard(GUINumericEdit::PreDraw);

	if ( INVALIDRENDER || MySpinner == NULL || MyEditBox == NULL )
		return;

	UGUIComponent::PreDraw(Canvas);

	FLOAT AL = ActualLeft(), AT = ActualTop(), AH = ActualHeight(), AW = ActualWidth();

	// Setup the actual widths/heights of all of the controls.

	MySpinner->WinWidth = AH;
	MySpinner->WinHeight = AH;
	
	if (bLeftJustified)
	{
		MySpinner->WinLeft = AL;
		MySpinner->WinTop  = AT;
		
		MyEditBox->WinWidth = AW - AH;
		MyEditBox->WinHeight = AH;
		MyEditBox->WinLeft = AL + AH;
		MyEditBox->WinTop = AT;
	}
	else
	{

		MyEditBox->WinWidth = AW - AH;
		MyEditBox->WinHeight = AH;
		MyEditBox->WinLeft = AL;
		MyEditBox->WinTop = AT;

		MySpinner->WinLeft = AL+MyEditBox->WinWidth;
		MySpinner->WinTop  = AT;
	}

	PreDrawControls(Canvas);
	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIFloatEdit
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIFloatEdit::PreDraw(UCanvas* Canvas)
{
	guard(GUIFloatEdit::PreDraw);

	if ( INVALIDRENDER || MySpinner == NULL || MyEditBox == NULL )
		return;

	UGUIComponent::PreDraw(Canvas);

	FLOAT AL = ActualLeft(), AT = ActualTop(), AH = ActualHeight(), AW = ActualWidth();

	// Setup the actual widths/heights of all of the controls.

	MySpinner->WinWidth = AH;
	MySpinner->WinHeight = AH;
	
	if (bLeftJustified)
	{
		MySpinner->WinLeft = AL;
		MySpinner->WinTop  = AT;
		
		MyEditBox->WinWidth = AW - AH;
		MyEditBox->WinHeight = AH;
		MyEditBox->WinLeft = AL + AH;
		MyEditBox->WinTop = AT;
	}
	else
	{

		MyEditBox->WinWidth = AW - AH;
		MyEditBox->WinHeight = AH;
		MyEditBox->WinLeft = AL;
		MyEditBox->WinTop = AT;

		MySpinner->WinLeft = AL+MyEditBox->WinWidth;
		MySpinner->WinTop  = AT;
	}

	PreDrawControls(Canvas);
	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIComboBox
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIComboBox::PreDraw(UCanvas* Canvas)
{

	guard(UGUIComboBox::PreDraw);
	
	if ( Edit == NULL || MyShowListBtn == NULL || MyListBox == NULL || INVALIDRENDER )
		return;

	// Do not call Super::PreDraw()
	UGUIComponent::PreDraw(Canvas);

	FLOAT AWidth  = ActualWidth();
	FLOAT AHeight = ActualHeight();
	Edit->SetDims( AWidth - AHeight, ActualHeight(), ActualLeft(), ActualTop() );
	MyShowListBtn->SetDims( AHeight, ActualHeight(), ActualLeft() + Edit->ActualWidth(), ActualTop() );

	if (!bVisible)
		return;

	if (List)
	{
		// Figure out how many items fit in the list
		int ListItems = List->Elements.Num();
		if (ListItems > MaxVisibleItems)
			ListItems = MaxVisibleItems;
		// We need a default value
		if (ListItems < 0)
			ListItems = 8;

		if (ListItems > 0 && List->Style)
		{
		int Wh = List->ItemHeight * ListItems + List->Style->BorderOffsets[1] + List->Style->BorderOffsets[3];
		int top = ActualTop() + ActualHeight();

			if ((top + Wh) > Canvas->ClipY)
				top = ActualTop() - Wh - 1;


			MyListBox->SetDims( ActualWidth(), Wh , ActualLeft(), top );
			MyListBox->MyScrollBar->WinWidth = MyShowListBtn->ActualWidth();
		}
	}

	PreDrawControls(Canvas);	

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUISplitter
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUISplitter::PreDraw(UCanvas* Canvas)
{
	guard(UGUISplitter::PreDraw);
	
	if ( INVALIDRENDER )
		return;

	UGUIComponent::PreDraw(Canvas);

	SplitterUpdatePositions();
	PreDrawControls(Canvas);	

	unguardobj;
}

void UGUISplitter::Draw(UCanvas* Canvas)
{
	guard(UGUISplitter::Draw);

	if ( !bVisible || INVALIDRENDER )
		return;

	// Draw the splitters subcomponents.  Have to turn off style.
	UGUIStyles* TempStyle = Style;
	Style = NULL;
	bRequiresStyle = 0;

	Super::Draw(Canvas);
	Style = TempStyle;
	bRequiresStyle = 1;

	// Draw the splitter using it's style object
	if(bDrawSplitter)
	{
		DECLAREBOUNDS;
		switch(SplitOrientation)
		{
		case SPLIT_Vertical:
			{
				FLOAT Y = AT + (SplitPosition * AH - SplitAreaSize / 2);
				Style->Draw(Canvas, MenuState, AL, Y, AW, SplitAreaSize);
			}
			break;
		case SPLIT_Horizontal:
			{
				FLOAT X = AL + (SplitPosition * AW - SplitAreaSize / 2);
				Style->Draw(Canvas, MenuState, X, AT, SplitAreaSize, AH );
			}
			break;
		}
	}

	unguardobj;
}

void UGUISplitter::execSplitterUpdatePositions(FFrame& Stack, RESULT_DECL )
{
	guard(UGUISplitter::execSplitterUpdatePositions);

	P_FINISH;
	SplitterUpdatePositions();

	unguardexec;
}

void UGUISplitter::SplitterUpdatePositions()
{
	guard(UGUISplitter::SplitterUpdatePositions);

	if ( !bVisible || INVALIDMENU )
		return;

	if( Controls.Num() < 2 || !Panels[0] || !Panels[1] )
		return;

	DECLAREBOUNDS;
	FLOAT Area = bDrawSplitter ? SplitAreaSize : 0.f;

	switch(SplitOrientation)
	{
	case SPLIT_Vertical:
		{
			FLOAT HT = (SplitPosition * AH - Area / 2);
			Panels[0]->SetAdjustedDims( AW, HT, AL, AT );
			Panels[1]->SetAdjustedDims( AW, AH - HT - Area, AL, AT + HT + Area);
		}
		break;
	case SPLIT_Horizontal:
		{
			FLOAT W = SplitPosition * AW - Area / 2;
			Panels[0]->SetAdjustedDims( W, AH, AL, AT );
			Panels[1]->SetAdjustedDims( AW - W - Area, AH, AL + W + Area, AT );
		}
		break;
	}
	unguardobj;
}

UBOOL UGUISplitter::MouseHover()
{
	guard(UGUISplitter::MouseHover);

	if ( INVALIDMENU )
		return false;

	if ( Super::MouseHover() )
		return true;

	// Change the mouse cursor as we wave it over the splitter

	if(bFixedSplitter)
		MouseCursorIndex = 0;
	else
	{
		FLOAT X1, X2, Y1, Y2;
		INT UseCursor;
		
		check(SplitOrientation == SPLIT_Vertical || SplitOrientation == SPLIT_Horizontal);

		if(SplitOrientation == SPLIT_Vertical)
		{
			X1 = ActualLeft();
			X2 = X1 + ActualWidth();
			Y1 = ActualTop() + SplitPosition*(ActualHeight()-SplitAreaSize);
			Y2 = Y1 + SplitAreaSize;
			UseCursor = 3;
		}
		else
		{
			X1 = ActualLeft() + SplitPosition*(ActualWidth()-SplitAreaSize);
			X2 = X1 + SplitAreaSize;
			Y1 = ActualTop();
			Y2 = Y1 + ActualHeight();
			UseCursor = 5;
		}

		// If we are over the splitter itself
		if(Controller->MouseX >= X1 && Controller->MouseX <= X2 && Controller->MouseY >= Y1 && Controller->MouseY <= Y2 )
			MouseCursorIndex = UseCursor;
		else
			MouseCursorIndex = 0;
	}

	return false;

	unguardobj;
}

UBOOL UGUISplitter::MousePressed(UBOOL IsRepeat)
{
	guard(UGUISplitter::MousePressed);

	if ( INVALIDMENU )
		return false;

	// Always assign a temp UGUIController pointer in case script sets Controller to NULL
	UGUIController* C = Controller;

	if (!PerformHitTest(C->MouseX, C->MouseY))
		return false;

	FLOAT X1, X2, Y1, Y2;
	switch(SplitOrientation)
	{
	case SPLIT_Vertical:
		X1 = ActualLeft();
		X2 = X1 + ActualWidth();
		Y1 = ActualTop() + SplitPosition*(ActualHeight()-SplitAreaSize);
		Y2 = Y1 + SplitAreaSize;
		break;
	case SPLIT_Horizontal:
		X1 = ActualLeft() + SplitPosition*(ActualWidth()-SplitAreaSize);
		X2 = X1 + SplitAreaSize;
		Y1 = ActualTop();
		Y2 = Y1 + ActualHeight();
		break;
	default:
		return true;		// for warning
	}

	if( !IsRepeat && !bFixedSplitter && C->MouseX >= X1 && C->MouseX <= X2 && C->MouseY >= Y1 && C->MouseY <= Y2 )
	{
		eventSetFocus(NULL);
		eventMenuStateChange(MSAT_Pressed);

		if ( DELEGATE_IS_SET(OnMousePressed) )
			delegateOnMousePressed(this,IsRepeat);

		C->PlayComponentSound(OnClickSound);
		bCaptureMouse = 1;
		return true;
	}

	else if (bCaptureMouse)
		return true;

	return Super::MousePressed(IsRepeat);
	unguardobj;
}

UBOOL UGUISplitter::MouseReleased()
{
	guard(UGUISplitter::MouseReleased);

	if (bCaptureMouse)
	{
		bCaptureMouse = 0;
		if ( DELEGATE_IS_SET(OnReleaseSplitter) )
			delegateOnReleaseSplitter(this, SplitPosition);

		return UGUIComponent::MouseReleased();
	}

	return Super::MouseReleased();

	unguardobj;
}


UBOOL UGUISplitter::MouseMove(INT XDelta, INT YDelta)
{
	guard(UGUISplitter::MouseMove);
	Super::MouseMove(XDelta,YDelta);

	if ( INVALIDMENU )
		return false;

	// Always assign a temp UGUIController pointer in case script sets Controller to NULL
	UGUIController* C = Controller;

	if( MenuState==MSAT_Pressed )
	{
		switch(SplitOrientation)
		{
		case SPLIT_Vertical:
			SplitPosition = Clamp<FLOAT>( ((FLOAT)C->MouseY - ActualTop()) / ActualHeight(), 0.f, 1.f );
			break;
		case SPLIT_Horizontal:
			SplitPosition = Clamp<FLOAT>( ((FLOAT)C->MouseX - ActualLeft()) / ActualWidth(), 0.f, 1.f );
			break;
		}
		
		// Clamp it

		if (MaxPercentage>0)
			SplitPosition = Clamp<FLOAT>( SplitPosition, (1-MaxPercentage), MaxPercentage);

		SplitterUpdatePositions();
		UpdateBounds();
		return true;
	}
	else
		bCaptureMouse = 0;

	return false;
	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUISlider
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUISlider::Draw(UCanvas* Canvas)
{

	guard(UGUISlider::Draw);

	if ( !bVisible || INVALIDRENDER || MinValue == MaxValue )
		return;

	Super::Draw(Canvas);
/*
	if ( FillImage && !bShowMarker )
		MarkerWidth = Canvas->SizeX * (8.f / 640.f);
	else MarkerWidth = Canvas->SizeX * (16.0f/640.0f);
*/
	FLOAT AL = ActualLeft();
	FLOAT AH = ActualHeight();
	FLOAT AW = ActualWidth();
	FLOAT AT = ActualTop();

	if ( BarStyle )
		BarStyle->Draw( Canvas, MenuState, AL, AT + (AH/4), AW, AH/2);

	MarkerWidth = Style->ImgWidths[0] * (Canvas->ClipX / 1024);	// Scale the marker
	FLOAT MarkerLeft = eventGetMarkerPosition();
	FLOAT MarkerHeight = Style->ImgHeights[0] * (Canvas->ClipY / 768);

	FLOAT FT = AT + (AH*0.25f);
	FLOAT FH = AH * 0.5f;

	if (FillImage!=NULL)	// Display the fill image
	 	Canvas->DrawTileStretched(FillImage, AL, FT, MarkerLeft - AL, FH );

	FLOAT MarkerTop = FT + (FH*0.5) - (MarkerHeight * 0.5);
	if (bShowMarker)
		Canvas->DrawTile(Style->Images[MenuState],   MarkerLeft,   MarkerTop,   MarkerWidth, MarkerHeight,0,0,Style->ImgWidths[MenuState],Style->ImgHeights[MenuState], 0.0f, Style->ImgColors[MenuState],FPlane(0,0,0,0));
	
	if (bShowCaption)
	{
		if (CaptionStyle==NULL)
			return;

		FLOAT XL,YL;
		AT = MarkerTop + MarkerHeight;
		BYTE Just = 1;

		CaptionStyle->TextSize(Canvas,MenuState, TEXT("WQ,"),XL,YL,FontScale);
		AH = YL;

		if ( IsAnimating() )
			return;

		if ( DELEGATE_IS_SET(OnPreDrawCaption) && delegateOnPreDrawCaption(AL, AT, AW, AH, Just) )
			return;

		CaptionStyle->DrawText(Canvas, MenuState, AL, AT, AW, AH, Just, *delegateOnDrawCaption(), FontScale);
	}
	unguardobj;

}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIScrollZoneBase
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIScrollZoneBase::Draw(UCanvas* Canvas)
{
	guard(UGUIScrollZoneBase::Draw);

	if ( !bVisible || INVALIDRENDER )
		return;

	Super::Draw(Canvas);

	Style->Draw(Canvas, MenuState, ActualLeft(), ActualTop(), ActualWidth(), ActualHeight());

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIScrollBarBase
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIScrollBarBase::PreDraw(UCanvas* Canvas)
{
	guard(UGUIScrollBarBase::PreDraw);

	UGUIComponent::PreDraw(Canvas);

	if ( !bVisible || INVALIDRENDER )
		return;
	
	// Position the two tick buttons

	FLOAT AW = ActualWidth(), AL = ActualLeft(), AT = ActualTop(), AH = ActualHeight(),
		ZoneSize = 0.f, ZeroPos = 0.f;

	if ( Orientation == ORIENT_Horizontal )
	{
		MyScrollZone->SetDims( AW - (2 * AH), AH, AL + AH, AT );
		MyDecreaseButton->SetDims( AH, AH, AL, AT );
		MyIncreaseButton->SetDims( AH, AH, (AL + AW) - AH, AT);

		ZoneSize = MyScrollZone->ActualWidth();
		ZeroPos = MyScrollZone->ActualLeft();
	}

	else
	{
		MyScrollZone->SetDims( AW, AH - (2 * AW), AL, AT + AW );
		MyDecreaseButton->SetDims( AW, AW, AL, AT );
		MyIncreaseButton->SetDims( AW, AW, AL, (AT + AH) - AW );

		ZoneSize = MyScrollZone->ActualHeight();
		ZeroPos = MyScrollZone->ActualTop();
	}
	// Calculate the grip

	if ( MyGripButton->MenuState != MSAT_Pressed )
	{
		if (MyList) GripSize = ZoneSize * ( (FLOAT)MyList->ItemsPerPage / (FLOAT)MyList->ItemCount );
		else GripSize = ZoneSize * ( (FLOAT)ItemsPerPage / (FLOAT)ItemCount );
	}

	if ( GripSize < MinGripPixels )
		GripSize = MinGripPixels;

	ZoneSize -= GripSize;

	FLOAT NewGripPos = ZeroPos + ZoneSize * GripPos;
	if ( Orientation == ORIENT_Horizontal )
        MyGripButton->SetDims( GripSize, AH, NewGripPos, AT);
	else MyGripButton->SetDims( AW, GripSize, AL, NewGripPos );

	PreDrawControls(Canvas);	
	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUISectionBackground
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUISectionBackground::AutoPosition(TArray<UGUIComponent*>& Components, FLOAT PosL, FLOAT PosT, FLOAT PosR, FLOAT PosB, INT Col, FLOAT ColSpace)
{
	guard(UGUISectionBackground::AutoPosition);

	PosT = Min<FLOAT>(PosT,PosB);
	PosL = Min<FLOAT>(PosL,PosR);

	PosB = Max<FLOAT>(PosT,PosB);
	PosR = Max<FLOAT>(PosL,PosR);

	if (PosB<=PosT)
		return;

	check(PosB>PosT);
	check(Col>0);
	check(ColSpace>=0.f);

	INT CLen = Components.Num();
	check(CLen);

	FLOAT TotalHeight = PosB - PosT, TotalWidth = PosR - PosL, Spacing = ColSpace * TotalWidth;

	UGUIComponent* comp = NULL;

	FLOAT MaxCompPerCol = CLen / Col;

	if (MaxPerColumn>0 && MaxCompPerCol>MaxPerColumn)
		MaxCompPerCol = MaxPerColumn;

	FLOAT CompWidth = (TotalWidth - ((Col - 1) * Spacing)) / Col;
	FLOAT CompOffset = TotalHeight / (appCeil(MaxCompPerCol));	// Amount of space that should be between each components WinTop

	INT ColWrap = appFloor(MaxCompPerCol);	// Where we should start a new column
	FLOAT X = PosL, Y = PosT;

	FLOAT CompHeight = bFillClient
		? (TotalHeight - ((Controller->ResY * 0.008) * (ColWrap - 1))) / ColWrap
		: 0.f;

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

			comp->SetAdjustedDims(CompWidth, CompHeight > 0.f ? CompHeight : comp->WinHeight, X, Y);
			Y += CompOffset;


		}

	}

	unguardobj;
}

void UGUISectionBackground::PreDraw(UCanvas* Canvas)
{
	guard(UGUISectionBackground::PreDraw);

	if ( !CaptionStyle || INVALIDRENDER )
		return;

	Super::PreDraw(Canvas);

	unguardobj;
}

void UGUISectionBackground::Draw(UCanvas* Canvas)
{
	guard(UGUISectionBackground::Draw);

	Super::Draw(Canvas);

	if ( !bVisible || !CaptionStyle || INVALIDRENDER )
		return;

	// This is a big hack.. look away.

//	Canvas->Color = FColor(255,255,255,155);
	Canvas->Color = FColor(255,255,255,255);
	Canvas->DrawTileStretched( HeaderBase,ActualLeft(),ActualTop(),ActualWidth(),ActualHeight() );

	FLOAT XL,YL;
	CaptionStyle->TextSize(Canvas, MSAT_Blurry,*Caption, XL, YL, FontScale);

	if (bAltCaption)
	{
		FLOAT X1 = ActualLeft() + AltCaptionOffset[0];
		FLOAT Y1 = ActualTop() + AltCaptionOffset[1];
		FLOAT X2 = ActualLeft() + ActualWidth() - AltCaptionOffset[2];
		FLOAT Y2 = ActualTop() + AltCaptionOffset[3];

		FLOAT L = X1 + ( (X2-X1) /2 ) - (XL/2);
		FLOAT T = Y1 + ( (Y2-Y1) /2 ) - (YL/2);

		CaptionStyle->DrawText(Canvas,MSAT_Blurry, L,T,XL,YL,AltCaptionAlign, *Caption, FontScale );
	}
	else
	{
		FLOAT MinX(Bounds[0] + ImageOffset[0]);
		FLOAT X2 = Bounds[2] - ImageOffset[2] * 2;
		BYTE Just = TXTA_Right;
		if ( X2 - (XL + ImageOffset[2]) > MinX )
			Just = TXTA_Center;

		FLOAT X1 = Max<FLOAT>(MinX, X2 - XL);
		FLOAT W =  X2 - X1;
		FLOAT T = ActualTop()+4;
		FLOAT H = 22;

//		FLOAT L = Max<FLOAT>(MinX, X1 - 6);
		if ( X1 - 6 > MinX )
            Canvas->DrawTileStretched(HeaderTop, X1 - 7, T, W + 11, H);
		else Canvas->DrawTileStretched( HeaderTop, Max<FLOAT>(X1 - 4, MinX), T, Min<FLOAT>(W + 8, X2 - X1), H );

		CaptionStyle->DrawText(Canvas,MSAT_Blurry,X1,T,W,H,Just,*Caption,FontScale);
		if ( X1 > MinX )
		{
			Canvas->Color = FColor(255,255,255,255);
			X2 = X1 - 8;
			X1 = ActualLeft() + 24;
			T += 7;
			H = 8;
			Canvas->DrawTileStretched(HeaderBar,X1,T,(X2-X1),H);
		}
	}
	

/*




	FLOAT XL,YL,X(ActualLeft()),Y(ActualTop());
	CaptionStyle->TextSize(Canvas,MSAT_Blurry,*Caption, XL, YL, 1.0);
	Canvas->Color = FColor(255,255,255,255);
	Canvas->DrawTileStretched(HeaderTop,X,Y,ActualWidth(),TopSize);
	Y+=TopSize;

	if (BarSize>0)
	{
		Canvas->DrawTileStretched(HeaderBar,X,Y,ActualWidth(),BarSize);
		Y+=BarSize;
	}

	FLOAT BaseSize = ActualHeight()- TopSize - BarSize;

	Canvas->DrawTileStretched(HeaderBase,X,Y,ActualWidth(),BaseSize);

	X = ActualLeft()+ActualWidth() - 8 - XL;
	Y = ActualTop()+ 3 ;
	CaptionStyle->DrawText(Canvas,MSAT_Blurry,X,Y,XL,YL,TXTA_Left,*Caption,1.0);

	X = ActualLeft()+15;
	Y = ActualTop() + ( YL / 2)+2;
	XL = ActualWidth() - XL - 28;
	Canvas->DrawTile(Controller->DefaultPens[0],X,Y,XL,2,0,0,16,16,0,CaptionStyle->FontColors[0],FPlane(0,0,0,0));
*/
	unguard;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUISpinnerButton
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUISpinnerButton::Draw(UCanvas* Canvas)
{
	guard(UGUISpinnerButton::Draw);

	if ( INVALIDRENDER )
		return;

	FLOAT H = ActualHeight() /2;
	FLOAT Y1 = ActualTop();
	FLOAT Y2 = ActualTop() + H;


	if (MenuState==MSAT_Watched || MenuState==MSAT_Pressed)
	{
		UBOOL OverPlus = Controller->MouseY <= ActualTop() + (ActualHeight()/2);
		
		if (OverPlus)
		{
			Canvas->DrawTile(Style->Images[MenuState],  ActualLeft(),Y1,ActualWidth(),H,0,0, 32,16,0,Style->ImgColors[MenuState],  FPlane(0,0,0,0));
			Canvas->DrawTile(Style->Images[MSAT_Blurry],ActualLeft(),Y2,ActualWidth(),H,0,16,32,16,0,Style->ImgColors[MSAT_Blurry],FPlane(0,0,0,0));
		}
		else
		{
			Canvas->DrawTile(Style->Images[MSAT_Blurry],ActualLeft(),Y1,ActualWidth(),H,0,0, 32,16,0,Style->ImgColors[MSAT_Blurry],FPlane(0,0,0,0));
			Canvas->DrawTile(Style->Images[MenuState],  ActualLeft(),Y2,ActualWidth(),H,0,16,32,16,0,Style->ImgColors[MenuState],  FPlane(0,0,0,0));
		}
	}
	else
	{
		Canvas->DrawTile(Style->Images[MenuState],ActualLeft(),Y1,ActualWidth(),H,0,0, 32,16,0,Style->ImgColors[MSAT_Blurry],FPlane(0,0,0,0));
		Canvas->DrawTile(Style->Images[MenuState],ActualLeft(),Y2,ActualWidth(),H,0,16,32,16,0,Style->ImgColors[MSAT_Blurry],FPlane(0,0,0,0));
	}

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIImage
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIImage::UpdateBounds()
{
	guard(UGUIImage::UpdateBounds);

	FLOAT AW = ActualWidth(), AH = ActualHeight(), AL = ActualLeft(), AT = ActualTop();

	Bounds[0] = AL;
	Bounds[1] = AT;
	Bounds[2] = AL + AW;
	Bounds[3] = AT + AH;

	ClientBounds[0] = Bounds[0] + (AW * BorderOffsets[0]);
	ClientBounds[1] = Bounds[1] + (AH * BorderOffsets[1]);
	ClientBounds[2] = Bounds[2] - (AW * BorderOffsets[2]);
	ClientBounds[3] = Bounds[3] - (AH * BorderOffsets[3]);

	bPositioned = 1;

	unguardobj;
}

void UGUIImage::Draw(UCanvas* Canvas)
{
	guard(GUIImage::Draw);

	if ( !bVisible || INVALIDRENDER )
		return;

	// Pass it along;

	Super::Draw(Canvas);
	FLOAT AL = ActualLeft(), AT = ActualTop(), AW = ActualWidth(), AH = ActualHeight();

	if ( !Image )
		return;

	if ( DropShadow && (DropShadowX != 0 || DropShadowY != 0) )		// Draw a Drop Shadow
	{
		Canvas->Style=MSTY_Alpha;
		Canvas->Color=FPlane(255,255,255,255);
		Canvas->DrawTileStretched(DropShadow, AL + DropShadowX, AT + DropShadowY, AW, AH ); 
	}

	// Draw the background starting in the upper left of the image

	Canvas->Style = ImageRenderStyle;
	Canvas->Color = ImageColor;

	INT mU = Image->MaterialUSize(), mV = Image->MaterialVSize();

	switch (ImageStyle)
	{
		case ISTY_Normal:
		{
			FLOAT X,Y, XL, YL;

			if ( X1 >=0 )
				X = X1;
			else
				X = 0;

			if ( Y1 >= 0 )
				Y = Y1;
			else
				Y = 0;

			if ( X2 < 0 )
				XL = Min<FLOAT>(AW, mU);
			else
				XL = X2 - X1;

			if ( Y2 < 0 )
				YL = Min<FLOAT>(AH, mV);
			else
				YL = Y2 - Y1;

			switch ( ImageAlign )
			{
			case IMGA_Center:
				AL -= (XL / 2);
				AT -= (YL / 2);
				break;
			case IMGA_BottomRight:
				AL = (AL + AW) - XL;
				AT = (AT + AH) - YL;
				break;
			}

			Canvas->DrawTile(Image, AL, AT, XL, YL,X,Y, XL, YL, 0, Canvas->Color, FPlane(0,0,0,0));
			break;
		}
		case ISTY_Stretched:
		case ISTY_PartialScaled:
		{
			if ( X1 >= 0 || X2 >= 0 || Y1 >= 0 || Y2 >= 0 )
			{
				FLOAT X, Y, XL, YL;

				if ( X1 >=0 )
					X = X1;
				else
					X = 0;

				if ( Y1 >= 0 )
					Y = Y1;
				else
					Y = 0;

				if ( X2 < 0 )
					XL = Min<FLOAT>(AW, mU);
				else
					XL = X2 - X1;

				if ( Y2 < 0 )
					YL = Min<FLOAT>(AH, mV);
				else
					YL = Y2 - Y1;

				switch ( ImageAlign )
				{
				case IMGA_Center:
					AL = AL - (XL / 2);
					AT = AT - (YL / 2);
					break;
				case IMGA_BottomRight:
					AL = (AL + AW) - XL;
					AT = (AT + AH) - YL;
					break;
				}

				// TODO Support for PartialScaled for sub-images
				Canvas->DrawTile(Image, AL, AT, AW, AH, X, Y, XL, YL, 0, Canvas->Color, FPlane(0,0,0,0));
				break;
			}

			if ( ImageStyle == ISTY_PartialScaled )
				Canvas->DrawTileStretchedOrScaled( Image, AL, AT, AW, AH, X3, Y3 );
			else Canvas->DrawTileStretched( Image, AL, AT, AW, AH );
			break;
		}
		case ISTY_Scaled:
		{
			if ( X1 < 0 && X2 < 0 && Y1 < 0 && Y2 < 0 )
			{
				if ( ImageAlign == IMGA_Center )
				{
					AL -= (AW / 2);
					AT -= (AH / 2);
				}
				Canvas->DrawTileScaleBound(Image, AL, AT, AW, AH );
			}

			else
			{
				FLOAT X,Y, XL, YL;

				if ( X1 >= 0 )
					X = X1;
				else
					X = 0;

				if ( Y1 >= 0 )
					Y = Y1;
				else
					Y = 0;

				if ( X2 < 0 )
					XL = Min<FLOAT>(AW, mU);
				else
					XL = X2 - X1;

				if ( Y2 < 0 )
					YL = Min<FLOAT>(AH, mV);
				else
					YL = Y2 - Y1;

				if ( ImageAlign == IMGA_Center )
				{
					AL -= (AW / 2);
					AT -= (AH / 2);
				}
				Canvas->DrawTile(Image, AL, AT, AW, AH, X, Y, XL, YL, 0, Canvas->Color, FPlane(0,0,0,0));		
			}
			break;
		}
		case ISTY_Bound:
			Canvas->DrawTileBound(Image, AL, AT, AW, AH );
			break;

		case ISTY_Justified:
			Canvas->DrawTileJustified(Image, AL, AT, AW, AH, ImageAlign );
			break;

		case ISTY_Tiled:
			Canvas->DrawTile(Image,AL,AT,AW,AH,0,0,AW,AH,0,Canvas->Color,FPlane(0,0,0,0)); 
			break;
	}

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UCoolImage
// =======================================================================================================================================================
// =======================================================================================================================================================

void UCoolImage::Draw(UCanvas* Canvas)
{
	guard(UCoolImage::Draw);

	if ( !bVisible || INVALIDRENDER )
		return;

	INT i;

	FLOAT mU = Image->MaterialUSize();
	FLOAT mV = Image->MaterialVSize();

	for (i=0;i<Anims.Num();i++)
	{
		FAnimInfo& Anim = Anims(i);
		FLOAT x = Anim.cX;
		FLOAT y = Anim.cY;

		if (Anim.Alpha>0)
		{
			Anim.cX+=Anim.TravelTime;
			Anim.cY-=Anim.TravelTime;
		}

		if (Anim.ResetDelay>0)
		{
			Anim.ResetDelay -= Controller->RenderDelta;
		}			
		else
		{

			Anim.ResetDelay=0;

			Anim.Alpha += (Anim.TargetAlpha - Anim.Alpha) * (Controller->RenderDelta / Anim.FadeTime);
			Anim.FadeTime -= Controller->RenderDelta;

			if (Anim.FadeTime<=0)
			{
				Anim.Alpha = Anim.TargetAlpha;
				if (!Anim.Alpha)
					eventResetItem(i);
				else
				{
					Anim.TargetAlpha = 0;
					Anim.FadeTime = MinFadeTime * (appFrand() * (MaxFadeTime - MinFadeTime));
					Anim.ResetDelay = MinResetDelay * (appFrand() * (MaxResetDelay - MinResetDelay));
				}
			}

		}
		Canvas->DrawTile(Image,x,y,mU * Anim.Scale ,mV * Anim.Scale, 0,0, mU,mV,0,FColor(255,255,255,Anim.Alpha),FPlane(0,0,0,0));

	}
	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIProgressBar
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIProgressBar::Draw(UCanvas* Canvas)
{
	guard(UGUIProgressBar::Draw);

	if ( !bVisible || INVALIDRENDER )
		return;

	Super::Draw(Canvas);
	float Left = ActualLeft(), Width = ActualWidth();
	float Top = ActualTop(), Height = ActualHeight();
	
	if (CaptionWidth > 0.0 && Width > 0)
	{
	float W = CaptionWidth;

		if (W < 1.0)
			W *= Width;

		if (W > Width)
			W = Width;

		// Draw the label
		if ( Style )
			Style->DrawText( Canvas, MenuState, Left, Top, W, Height, CaptionAlign, *Caption, FontScale );
		else
		{
				// Select the right font in the Canvas
			UGUIFont *Fnt = Controller->eventGetMenuFont(FontName);
			if (Fnt)
			{
				Canvas->Font = Fnt->eventGetFont(Canvas->SizeX);
			}

			Canvas->DrawTextJustified(CaptionAlign, Left, Top, Left + W, Top + Height, TEXT("%s"), *Caption);
		}
		Left += W;
		Width -= W;
	}

	if ( (bShowHigh || bShowValue) && ValueRightWidth > 0.0 && Width > 0.0)
	{
	float W = ValueRightWidth;
	FString str;

		if (W < 1.0)
			W *= Width;

		if (W > Width)
			W = Width;

		// TODO implemente NumDecimals
		if (bShowValue && bShowHigh)
			str = FString::Printf(TEXT("%.0f/%.0f"), Value, High);
		else if (bShowValue)
			str = FString::Printf(TEXT("%.0f"), Value);
		else
			str = FString::Printf(TEXT("%.0f"), High);

		if ( Style )
			Style->DrawText( Canvas, MenuState, Left, Top, W, Height, ValueRightAlign, *str, FontScale );
		
		else Canvas->DrawTextJustified(ValueRightAlign, Left + Width - W, Top, Left + Width, Top + Height, TEXT("%s"), *str);

		Width -= W;
	}
	
	if (Width > GraphicMargin)
	{
		Width -= GraphicMargin;
		Left += GraphicMargin / 2;
	}

	// Actually Draw the content
	Canvas->Color = FColor(255,255,255);
	if (Width > 0.0 && BarBack)
		Canvas->DrawTileStretched(BarBack, Left, Top, Width, Height);

	if (Width > 0.0 && BarTop && Value > Low)
	{
		Canvas->Style = STY_Normal;
		Canvas->Color = BarColor;
		switch (BarDirection)
		{
			case DRD_LeftToRight: Canvas->DrawTileStretched(BarTop, Left, Top, Width * Value/High, Height); break;
			case DRD_RightToLeft: Canvas->DrawTileStretched(BarTop, Left + Width * (1 - Value/High), Top, Width * Value/High, Height); break;
			case DRD_TopToBottom: Canvas->DrawTileStretched(BarTop, Left, Top, Width, Height * Value/High); break;
			case DRD_BottomToTop: Canvas->DrawTileStretched(BarTop, Left, Top + Height * (1 - Value/High), Width, Height * Value/High); break;
		}		
	}
	unguardobj;
}


// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIContextMenu
// =======================================================================================================================================================
// =======================================================================================================================================================

UBOOL UGUIContextMenu::Close()
{
	if ( !Controller )
		return true;

	UGUIController* C = Controller;
	if ( DELEGATE_IS_SET(OnClose) )
		if ( !delegateOnClose(this) )
			return false;

	C->ContextMenu = NULL;
	return true;
}

void UGUIContextMenu::PreDraw(UCanvas* Canvas)
{
	guard(GUIContextMenu::PreDraw);

	if ( INVALIDRENDER )
		return;

	INT i,Big;
	FLOAT XL,YL;
	UGUIController* C = Controller;

	if ( C->HasMouseMoved() )
		UpdateIndex(C->MouseX,C->MouseY);

#if UCONST_Counter == 0
	if ( DELEGATE_IS_SET(OnPreDraw) )
#endif
		if ( delegateOnPreDraw(Canvas) )
			return;
	
	if (ContextItems.Num() < 1)
	{
		RestoreCanvasState(Canvas);
		return;
	}

	WinHeight = 0.f;
	for (i=0,Big=-1,WinHeight=0;i<ContextItems.Num();i++)
	{
        Style->TextSize(Canvas, MSAT_Blurry, *ContextItems(i), XL,YL, FontScale);
		if (XL>Big)
			Big=XL;

		WinHeight += YL;
		ItemHeight=YL;
	}

	WinWidth   = Big + Style->BorderOffsets[0] + Style->BorderOffsets[2];
	WinHeight += (Style->BorderOffsets[1] + Style->BorderOffsets[3]);

	if (WinLeft + WinWidth > C->ResX)
		WinLeft = C->ResX - WinWidth - 5;

	if (WinTop + WinHeight > C->ResY)
		WinTop = C->ResY - WinHeight - 5;

	UpdateBounds();

	unguard;
}

// Draw should always be subclassed in the individual components, and they should ALWAYS call their super first to
// obtain their state.

void UGUIContextMenu::Draw(UCanvas* Canvas)
{
	guard(GUIContextMenu::Draw);

	// Draw the Background
	if (ContextItems.Num() < 1 || INVALIDRENDER )
		return;

	SaveCanvasState(Canvas);

	UGUIController* C = Controller;

	Canvas->ColorModulate *= FPlane (0.2f, 0.2f, 0.2f, 0.75f);
	Style->Draw(Canvas, MSAT_Blurry, WinLeft+3,WinTop+3,WinWidth,WinHeight);
	Canvas->ColorModulate = SaveModulation;
	Style->Draw(Canvas, MSAT_Blurry, WinLeft,WinTop,WinWidth,WinHeight);
#if UCONST_Counter == 0
	if ( DELEGATE_IS_SET(OnDraw) )
#endif
		if (delegateOnDraw(Canvas) )
		{
			RestoreCanvasState(Canvas);
			return;
		}
	
	// Draw the ContextItems

	INT X = ClientBounds[0];
	INT Y = ClientBounds[1];
	INT W = ClientBounds[2] - ClientBounds[0];
	FLOAT XL,YL;

	for (INT i=0;i<ContextItems.Num();i++)
	{
		Style->TextSize(Canvas, MSAT_Blurry, *ContextItems(i), XL,YL, FontScale);
		if ( ContextItems(i) != TEXT("-") )
		{
			// Highlight it
			if ( i == ItemIndex && SelectionStyle )
			{
				Canvas->DrawTile(C->DefaultPens[0],X,Y,W,YL,0,0,64,64,0,SelectionStyle->FontBKColors[MSAT_Focused],FPlane(0,0,0,0)); 
				SelectionStyle->DrawText(Canvas, MSAT_Focused, X, Y, XL, YL, 0, *ContextItems(i), FontScale);
			}
			else
				Style->DrawText(Canvas, MSAT_Blurry, X, Y, XL, YL, 0, *ContextItems(i), FontScale);
		}
		else
		{
			INT Mid = Y + (YL/2)-1;
			Canvas->DrawTile(C->DefaultPens[0],X,Mid,W,3,0,0,64,64,0,Style->FontColors[MSAT_Focused],FPlane(0,0,0,0)); 
		}

		Y+=YL;
	}

	RestoreCanvasState(Canvas);
	
	unguardobj;
}


void UGUIContextMenu::UpdateIndex(INT MouseX, INT MouseY)
{
	guard(UGUIContextMenu::UpdateIndex);

	if ( INVALIDMENU )
	{
		ItemIndex = INDEX_NONE;
		return;
	}

	if ( DELEGATE_IS_SET(OnContextHitTest) && !delegateOnContextHitTest(MouseX,MouseY))
	{
		ItemIndex = INDEX_NONE;
		return;
	}

	// Perform simple box collision
	if ( WithinBounds(MouseX,MouseY) )
		ItemIndex = (MouseY - ClientBounds[1]) / ItemHeight;
	else ItemIndex = INDEX_NONE;

	unguardobj;

}


UBOOL UGUIContextMenu::KeyEvent(BYTE& iKey, BYTE& State, FLOAT Delta)
{
	guard(UGUIContextMenu::KeyEvent);

	if ( INVALIDMENU )
		return false;

	if ( iKey == IK_LeftMouse )
	{
		if ( State == IST_Release )
		{
			if ( DELEGATE_IS_SET(OnSelect) && ContextItems.IsValidIndex(ItemIndex) )
			{
				delegateOnSelect(this, ItemIndex);
				if ( ContextItems(ItemIndex) != TEXT("-") )
					Close();

				return true;
			}

			Close();
		}

		return true;
	}

	if ( iKey == IK_Enter )
	{
		if ( State == IST_Release )
		{
			if ( DELEGATE_IS_SET(OnSelect) && ItemIndex >= 0 && ItemIndex < ContextItems.Num() )
			{
                delegateOnSelect(this,ItemIndex);
				if ( ContextItems(ItemIndex) != TEXT("-") )
					Close();

				return true;
			}

			Close();
		}

		return true;
	}

	if ( iKey==IK_Up || iKey==IK_NumPad8 )
	{
		if ( State == IST_Release )
		{
			if ( ItemIndex > 0 )
				ItemIndex--;
			while ( ItemIndex > 0 && ItemIndex < ContextItems.Num() && ContextItems(ItemIndex) == TEXT("-") )
				ItemIndex--;
		}

		return true;
	}

	if ( iKey == IK_Down || iKey == IK_NumPad2 )
	{
		if ( State == IST_Release )
		{
			if ( ItemIndex < ContextItems.Num() - 1 )
				ItemIndex++;

			while ( ItemIndex >= 0 && ItemIndex < ContextItems.Num() - 1 && ContextItems(ItemIndex) == TEXT("-") )
				ItemIndex++;
		}

		return true;
	}

	if ( iKey == IK_Escape && State == IST_Release )
		Close();

	// Always swallow input when context menu is visible
	return true;

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIToolTip
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIToolTip::PreDraw( UCanvas* Canvas )
{
	guard(UGUIToolTip::Draw);

	if ( !bVisible || INVALIDRENDER )
		return;

	if ( DELEGATE_IS_SET(OnPreDraw) && delegateOnPreDraw(Canvas) )
		return;

	if ( Text.Len() == 0 || Style->Fonts[5 + (MenuOwner->MenuState*FontScale)] == NULL )
		return;

	if ( Lines.Num() == 0 )
	{
		if ( bMultiLine )
			Canvas->WrapStringToArray(*Text, &Lines, (Canvas->SizeX * MaxWidth) - (Style->BorderOffsets[0] + Style->BorderOffsets[2]), Style->Fonts[5+(MenuOwner->MenuState*FontScale)]->eventGetFont(Canvas->SizeX));
		else new(Lines) FString(Text);
	}

	if ( bResetPosition || bTrackMouse )
		eventUpdatePosition(Canvas);

	UpdateBounds();

	unguardobj;
}

void UGUIToolTip::Draw( UCanvas* Canvas )
{
	guard(UGUIToolTip::Draw);

	if ( !bVisible ||INVALIDRENDER || bResetPosition )
		return;

	if ( DELEGATE_IS_SET(OnDraw) && delegateOnDraw(Canvas) )
		return;

	if ( Text.Len() == 0 || Style->Fonts[5 + (MenuOwner->MenuState*FontScale)] == NULL )
		return;

	// Draw the shadow
	FPlane OldCM(Canvas->ColorModulate);
	Canvas->ColorModulate *= FPlane(0.2f, 0.2f, 0.2f, 0.3f);
	Style->Draw( Canvas, MenuOwner->MenuState, WinLeft + 3, WinTop + 3, WinWidth, WinHeight );
	Canvas->ColorModulate = OldCM;

	// Draw the Background
	Style->Draw( Canvas, MenuOwner->MenuState, WinLeft, WinTop, WinWidth, WinHeight );

	// Draw the text
	FLOAT X(WinLeft + Style->BorderOffsets[0]), Y(WinTop + Style->BorderOffsets[1]), XL(0.f), YL(0.f);
	for ( INT i = 0; i < Lines.Num(); i++ )
	{
		const TCHAR* Text = *Lines(i);
		Style->TextSize( Canvas, MenuOwner->MenuState, Text, XL, YL, FontScale );
		Style->DrawText( Canvas, MenuOwner->MenuState, X, Y, WinWidth - (Style->BorderOffsets[0] + Style->BorderOffsets[2]), YL, 0, Text, FontScale );

		Y += YL;
		if ( Y > WinTop + (WinHeight - Style->BorderOffsets[3]) )
			break;
	}

	unguardobj;
}

void UGUIToolTip::SetTip( FString& NewTip )
{
	guard(UGUIToolTip::SetTip);

	if ( Text != NewTip )
	{
		Text = NewTip;
		Lines.Empty();
	}
	if ( bVisible )
		bResetPosition = 1;

	unguardobj;
}

void UGUIToolTip::execSetTip( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIToolTip::execSetTip);

	P_GET_STR(NewTip);
	P_FINISH;

	SetTip(NewTip);
	unguardexec;
}
