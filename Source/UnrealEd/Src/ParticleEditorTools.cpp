//=============================================================================
//
// ParticleEditorTools.cpp	
// Copyright(c) 2002 Demiurge Studios. All Rights Reserved.
// 
//=============================================================================

#include "UnrealEd.h"


//=============================================================================
// WParticleEditorTool

WParticleEditorTool::WParticleEditorTool(WParticleEditorComponent* inOwnerComponent, FString inToolName, DWORD inToolFlags, INT InUDNHelpTopic) :
	WPropertyPage(inOwnerComponent),
	OwnerComponent(inOwnerComponent),
	EditTarget(NULL),
	ToolName(inToolName),
	ToolFlags(inToolFlags),
	CallUpdateTarget(TRUE),
	ToolEnabled(TRUE)
{
	UDNHelpTopic = InUDNHelpTopic;
}

WParticleEditorTool::WParticleEditorTool(WWindow *OwnerWindow, WParticleEditorComponent* inOwnerComponent, FString inToolName, DWORD inToolFlags, INT InUDNHelpTopic) :
	WPropertyPage(OwnerWindow),
	OwnerComponent(inOwnerComponent),
	EditTarget(NULL),
	ToolName(inToolName),
	ToolFlags(inToolFlags),
	CallUpdateTarget(TRUE),
	ToolEnabled(TRUE)
{
	UDNHelpTopic = InUDNHelpTopic;
}


	
void WParticleEditorTool::OnDestroy()
{
	check(1);
}



void WParticleEditorTool::SetTarget()
{
	EditTarget = OwnerComponent->GetEditTarget();
}



DWORD WParticleEditorTool::GetToolState()
{
	return 0;
}



void WParticleEditorTool::SetToolState(DWORD State)
{
}



FString WParticleEditorTool::GetToolName() const
{
	return ToolName;
}



void WParticleEditorTool::Enable(UBOOL Enabled)
{
	ToolEnabled = Enabled;
	for(INT i = 0; i < Controls.Num(); i++)
	{
		EnableWindow( Controls(i)->hWnd, Enabled );
	}
}



void WParticleEditorTool::UpdateTarget()
{
	if(CallUpdateTarget)
	{
		CallUpdateTarget = FALSE;

		UpdateTargetUtil();
		if((ToolFlags & PTOOL_RefreshOnChange) == PTOOL_RefreshOnChange)
		{
			EditTarget->PostEditChange();
		}

		if((ToolFlags & PTOOL_UpdateTools) == PTOOL_UpdateTools)
			this->OwnerComponent->GetParentTab()->RereadComponentValues();

		CallUpdateTarget = TRUE;
	}
}




//=============================================================================
// WRangeEdit 

WRangeEdit::WRangeEdit(WParticleEditorComponent* OwnerComponent, FString inVectorName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool( OwnerComponent, inVectorName, ToolFlags | PTOOL_ExtraIndent, InUDNHelpTopic), 
	RangeToEdit(NULL),
	UseAbsMin(FALSE),
	UseAbsMax(FALSE),
	AlreadyChangingText(FALSE)
{
}



WRangeEdit::WRangeEdit(WWindow *OwnerWindow, WParticleEditorComponent* OwnerComponent, FString inVectorName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool(OwnerWindow, OwnerComponent, inVectorName, ToolFlags | PTOOL_ExtraIndent, InUDNHelpTopic), 
	RangeToEdit(NULL),
	UseAbsMin(FALSE),
	UseAbsMax(FALSE),
	AlreadyChangingText(FALSE)
{
}



void WRangeEdit::OnDestroy()
{
	if(MinEdit)			{delete MinEdit;	MinEdit			= NULL;}
	if(MaxEdit)			{delete MaxEdit;	MaxEdit			= NULL;}
	if(MinSlide)		{delete MinSlide;	MinSlide		= NULL;}
	if(MaxSlide)		{delete MaxSlide;	MaxSlide		= NULL;}
	if(LockButton)	{delete LockButton;LockButton	= NULL;}

	DeleteObject( LockOpenBitmap );
	DeleteObject( LockSameBitmap );
	DeleteObject( LockMirrorBitmap );
}



void WRangeEdit::SetTarget(FRange* InRangeToEdit)
{
	WParticleEditorTool::SetTarget();
	RangeToEdit = InRangeToEdit;

	if(RangeToEdit->Min == 0 || RangeToEdit->Max == 0)
		SetLockState(LOCK_None);
	else if(RangeToEdit->Min == RangeToEdit->Max)
		SetLockState(LOCK_Same);
	else if(-RangeToEdit->Min == RangeToEdit->Max)
		SetLockState(LOCK_Mirror);
	else
		SetLockState(LOCK_None);
}



DWORD WRangeEdit::GetToolState()
{
	return (DWORD)LockState;
}



void WRangeEdit::SetToolState(DWORD State)
{
	SetLockState( (ELockState)State );
}



void WRangeEdit::Enable(UBOOL Enabled)
{
	WParticleEditorTool::Enable(Enabled);
	EnableWindow( MinEdit->hWnd, (LockState==LOCK_None) && Enabled );
}



FPoint WRangeEdit::GetSize() const
{
	return FPoint(300,25);
}



void WRangeEdit::SetAbsMin(FLOAT newMin)
{
	UseAbsMin = TRUE;
	AbsMinValue = newMin;
}



void WRangeEdit::SetAbsMax(FLOAT newMax)
{
	UseAbsMax = TRUE;
	AbsMaxValue = newMax;
}



void WRangeEdit::OpenWindow(HMODULE InHMOD )
{
	WPropertyPage::OpenWindow( IDPS_RANGE, InHMOD );

	MinEdit = new WEdit( this, IDPS_RANGE_MIN_X );
	MinEdit->OpenWindow( TRUE, 0, 0 );
	MaxEdit = new WEdit( this, IDPS_RANGE_MAX_X );
	MaxEdit->OpenWindow( TRUE, 0, 0 );

	MinSlide = new WButtonSlider( this, MinEdit, IDPS_RANGE_MIN_SLIDE_X );
	MinSlide->OpenWindow();
	MaxSlide = new WButtonSlider( this, MaxEdit, IDPS_RANGE_MAX_SLIDE_X );
	MaxSlide->OpenWindow();

	LockButton = new WButton( this, IDPS_RANGE_LOCK_X );
	LockButton->OpenWindow( TRUE, 0, 0, 0, 0, TEXT("") );

	LockOpenBitmap	= (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_UNLOCKED), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS ); check(LockOpenBitmap);
	LockSameBitmap	= (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_LOCK_SAME), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS ); check(LockSameBitmap);
	LockMirrorBitmap= (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_LOCK_MIRROR), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS ); check(LockMirrorBitmap);

	PlaceControl( MinEdit );
	PlaceControl( MaxEdit );
	PlaceControl( MinSlide );
	PlaceControl( MaxSlide );
	PlaceControl( LockButton );
	Finalize();


	MinEdit->ChangeDelegate = FDelegate(this, (TDelegate)OnTextChangeMin);
	MaxEdit->ChangeDelegate = FDelegate(this, (TDelegate)OnTextChangeMax);
	LockButton->ClickDelegate = FDelegate(this, (TDelegate)OnLockClick);
}




void WRangeEdit::OnLockClick()
{
	if(LockState == LOCK_None)
		SetLockState(LOCK_Same);
	else if(LockState == LOCK_Same)
	{
		if( (UseAbsMin && AbsMinValue >= 0.0) || (UseAbsMax && AbsMaxValue <= 0.0) )
			SetLockState(LOCK_None);
		else
			SetLockState(LOCK_Mirror);
	}
	else
		SetLockState(LOCK_None);
}



void WRangeEdit::SetLockState(ELockState newLockState)
{
	LockState = newLockState;
	if(hWnd)
	{
		switch(LockState)
		{
		case LOCK_None :
			LockButton->SetBitmap(LockOpenBitmap);
			break;
		case LOCK_Same :
			LockButton->SetBitmap(LockSameBitmap);
			break;
		case LOCK_Mirror :
			LockButton->SetBitmap(LockMirrorBitmap);
			break;
		}
		EnableWindow( MinEdit->hWnd, LockState == LOCK_None );
	}
}



void WRangeEdit::HandleLockText()
{
	if(LockState == LOCK_Same)
	{
		MinEdit->SetText( *MaxEdit->GetText() );
	}
	else if(LockState == LOCK_Mirror)
	{
		FLOAT maxVal = appAtof( *MaxEdit->GetText() );
		MinEdit->SetText(*FString::Printf(TEXT("%.3f"),-maxVal) );
	}

	//check for exceeding of abs Min and Max values
	if(UseAbsMin)
	{
		if(appAtof( *MinEdit->GetText() ) < AbsMinValue)
			MinEdit->SetText( *(FString::Printf(TEXT("%.3f"),AbsMinValue)) );
		if(appAtof( *MaxEdit->GetText() ) < AbsMinValue)
			MaxEdit->SetText( *(FString::Printf(TEXT("%.3f"),AbsMinValue)) );
	}
	if(UseAbsMax)
	{
		if(appAtof( *MinEdit->GetText() ) > AbsMaxValue)
			MinEdit->SetText( *(FString::Printf(TEXT("%.3f"),AbsMaxValue)) );
		if(appAtof( *MaxEdit->GetText() ) > AbsMaxValue)
			MaxEdit->SetText( *(FString::Printf(TEXT("%.3f"),AbsMaxValue)) );
	}
}



void WRangeEdit::OnTextChangeMin() { OnTextChange(TRUE); }
void WRangeEdit::OnTextChangeMax() { OnTextChange(FALSE); }
void WRangeEdit::OnTextChange(UBOOL minChange)
{
	//insure that this function doesn't loop forever
	if(!AlreadyChangingText && CallUpdateTarget)
	{
		AlreadyChangingText=TRUE;

		FLOAT minVal = appAtof( *MinEdit->GetText() );
		FLOAT maxVal = appAtof( *MaxEdit->GetText() );

		//check for 0 crossover on mirror lock
		if(LockState == LOCK_Mirror && maxVal < 0.0)
			MaxEdit->SetText( *(FString::Printf(TEXT("%.3f"),0.0)) );

		//check for Min Max cross over
		if(minChange && minVal > maxVal)
			MaxEdit->SetText( *MinEdit->GetText() );
		else if(!minChange && maxVal < minVal)
			MinEdit->SetText( *MaxEdit->GetText() );

		//based on the lockstate set the text boxes correctly taking abs Min and Max into account
		HandleLockText();

		UpdateTarget();
		AlreadyChangingText=FALSE;
	}
}



void WRangeEdit::GetValuesFromTarget()
{
	CallUpdateTarget = FALSE;
	MinEdit->SetText(*FString::Printf(TEXT("%.3f"),RangeToEdit->Min) );
	MaxEdit->SetText(*FString::Printf(TEXT("%.3f"),RangeToEdit->Max) );
	CallUpdateTarget = TRUE;
}


void WRangeEdit::UpdateTargetUtil()
{
	RangeToEdit->Min = appAtof( *MinEdit->GetText() );
	RangeToEdit->Max = appAtof( *MaxEdit->GetText() );
}




//=============================================================================
// WRangeVectorEdit 

WRangeVectorEdit::WRangeVectorEdit(WParticleEditorComponent* OwnerComponent, FString inVectorName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool( OwnerComponent, inVectorName, ToolFlags | PTOOL_ExtraIndent, InUDNHelpTopic), 
	VectorToEdit(NULL),
	AlreadyChangingText(FALSE),
	UseAbsMin(false),
	UseAbsMax(false),
	LockOpenBitmap(NULL),
	LockSameBitmap(NULL),
	LockMirrorBitmap(NULL)
{
}


WRangeVectorEdit::WRangeVectorEdit(WWindow *OwnerWindow, WParticleEditorComponent* OwnerComponent, FString inVectorName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool(OwnerWindow, OwnerComponent, inVectorName, ToolFlags | PTOOL_ExtraIndent, InUDNHelpTopic ), 
	VectorToEdit(NULL),
	AlreadyChangingText(FALSE),
	UseAbsMin(false),
	UseAbsMax(false),
	LockOpenBitmap(NULL),
	LockSameBitmap(NULL),
	LockMirrorBitmap(NULL)
{
}



void WRangeVectorEdit::OnDestroy()
{
	for(INT i = 0; i < 3; i++)
	{
		if(MinEdit[i])		{delete MinEdit[i];		MinEdit[i]		= NULL;}
		if(MaxEdit[i])		{delete MaxEdit[i];		MaxEdit[i]		= NULL;}
		if(MinSlide[i])		{delete MinSlide[i];	MinSlide[i]		= NULL;}
		if(MaxSlide[i])		{delete MaxSlide[i];	MaxSlide[i]		= NULL;}
		if(LinkCheck[i])	{delete LinkCheck[i];	LinkCheck[i]	= NULL;}
		if(LockButton[i])	{delete LockButton[i];	LockButton[i]	= NULL;}
	}
	if(LinkImageContainer) {delete LinkImageContainer; LinkImageContainer = NULL;}

	DeleteObject( LinkImage[0] );
	DeleteObject( LinkImage[1] );
	DeleteObject( LinkImage[2] );
	DeleteObject( LinkImage[3] );
	DeleteObject( LinkImage[4] );
	DeleteObject( LinkImage[5] );
	DeleteObject( LinkImage[6] );
	DeleteObject( LockOpenBitmap );
	DeleteObject( LockSameBitmap );
	DeleteObject( LockMirrorBitmap );
}



void WRangeVectorEdit::SetTarget(FRangeVector *InVectorToEdit)
{
	WParticleEditorTool::SetTarget();
	VectorToEdit = InVectorToEdit;

	if(hWnd)
	{

		if(VectorToEdit->X.Min == 0 || VectorToEdit->X.Max == 0)
			SetLockState(0, LOCK_None);
		else if(VectorToEdit->X.Min == VectorToEdit->X.Max)
			SetLockState(0, LOCK_Same);
		else if(-VectorToEdit->X.Min == VectorToEdit->X.Max)
			SetLockState(0, LOCK_Mirror);
		else
			SetLockState(0, LOCK_None);

		if(VectorToEdit->Y.Min == 0 || VectorToEdit->Y.Max == 0)
			SetLockState(1, LOCK_None);
		else if(VectorToEdit->Y.Min == VectorToEdit->Y.Max)
			SetLockState(1, LOCK_Same);
		else if(-VectorToEdit->Y.Min == VectorToEdit->Y.Max)
			SetLockState(1, LOCK_Mirror);
		else
			SetLockState(1, LOCK_None);

		if(VectorToEdit->Z.Min == 0 || VectorToEdit->Z.Max == 0)
			SetLockState(2, LOCK_None);
		else if(VectorToEdit->Z.Min == VectorToEdit->Z.Max)
			SetLockState(2, LOCK_Same);
		else if(-VectorToEdit->Z.Min == VectorToEdit->Z.Max)
			SetLockState(2, LOCK_Mirror);
		else
			SetLockState(2, LOCK_None);


		LinkCheck[0]->SetCheck(FALSE);	
		LinkCheck[1]->SetCheck(FALSE);	
		LinkCheck[2]->SetCheck(FALSE);	

		if(VectorToEdit->X.Min == VectorToEdit->Y.Min && VectorToEdit->X.Max == VectorToEdit->Y.Max &&
			!(VectorToEdit->X.Min == 0 && VectorToEdit->X.Max == 0))
		{
			LinkCheck[0]->SetCheck(TRUE);
			LinkCheck[1]->SetCheck(TRUE);
		}
		if(VectorToEdit->X.Min == VectorToEdit->Z.Min && VectorToEdit->X.Max == VectorToEdit->Z.Max &&
			!(VectorToEdit->X.Min == 0 && VectorToEdit->X.Max == 0))
		{
			LinkCheck[0]->SetCheck(TRUE);
			LinkCheck[2]->SetCheck(TRUE);
		}
		if(VectorToEdit->Y.Min == VectorToEdit->Z.Min && VectorToEdit->Y.Max == VectorToEdit->Z.Max &&
			!(VectorToEdit->Y.Min == 0 && VectorToEdit->Y.Max == 0))
		{
			LinkCheck[1]->SetCheck(TRUE);
			LinkCheck[2]->SetCheck(TRUE);
		}
	}
}



DWORD WRangeVectorEdit::GetToolState()
{
	DWORD state = 0;

	state |= LinkCheck[0]->IsChecked() << 0;
	state |= LinkCheck[1]->IsChecked() << 1;
	state |= LinkCheck[2]->IsChecked() << 2;

	state |=  LockState[0] << 4;
	state |=  LockState[1] << 8;
	state |=  LockState[2] << 12;

	return state;
}



void WRangeVectorEdit::SetToolState(DWORD State)
{
	LinkCheck[0]->SetCheck( State & 1 );
	LinkCheck[1]->SetCheck( (State >> 1) & 1 );
	LinkCheck[2]->SetCheck( (State >> 2) & 1 );

	SetLockState(0, (ELockState) ( (State >> 4) & 15 ) ); //( bin 1111 )
	SetLockState(1, (ELockState) ( (State >> 8) & 15 ) );
	SetLockState(2, (ELockState) ( (State >> 12) & 15 ) );
}



void WRangeVectorEdit::Enable(UBOOL Enabled)
{
	WParticleEditorTool::Enable(Enabled);
	if(Enabled)
		OnLinkClick();
}


FPoint WRangeVectorEdit::GetSize() const
{
	return FPoint(400, 68);
}



void WRangeVectorEdit::SetAbsMin(FLOAT newMin)
{
	UseAbsMin = TRUE;
	AbsMinValue = newMin;
}



void WRangeVectorEdit::SetAbsMax(FLOAT newMax)
{
	UseAbsMax = TRUE;
	AbsMaxValue = newMax;
}



void WRangeVectorEdit::OpenWindow(HMODULE InHMOD, INT InDlgId)
{
	check(OwnerWindow != NULL);

	WPropertyPage::OpenWindow( InDlgId, InHMOD );

	MinEdit[0] = new WEdit( this, IDPS_MIN_X );
	MinEdit[0]->OpenWindow( TRUE, 0, 0 );
	MaxEdit[0] = new WEdit( this, IDPS_MAX_X );
	MaxEdit[0]->OpenWindow( TRUE, 0, 0 );

	MinEdit[1] = new WEdit( this, IDPS_MIN_Y );
	MinEdit[1]->OpenWindow( TRUE, 0, 0 );
	MaxEdit[1] = new WEdit( this, IDPS_MAX_Y );
	MaxEdit[1]->OpenWindow( TRUE, 0, 0 );

	MinEdit[2] = new WEdit( this, IDPS_MIN_Z );
	MinEdit[2]->OpenWindow( TRUE, 0, 0 );
	MaxEdit[2] = new WEdit( this, IDPS_MAX_Z );
	MaxEdit[2]->OpenWindow( TRUE, 0, 0 );

	MinSlide[0] = new WButtonSlider( this, MinEdit[0], IDPS_MIN_SLIDE_X );
	MinSlide[0]->OpenWindow();
	MaxSlide[0] = new WButtonSlider( this, MaxEdit[0], IDPS_MAX_SLIDE_X );
	MaxSlide[0]->OpenWindow();

	MinSlide[1] = new WButtonSlider( this, MinEdit[1], IDPS_MIN_SLIDE_Y );
	MinSlide[1]->OpenWindow();
	MaxSlide[1] = new WButtonSlider( this, MaxEdit[1], IDPS_MAX_SLIDE_Y );
	MaxSlide[1]->OpenWindow();

	MinSlide[2] = new WButtonSlider( this, MinEdit[2], IDPS_MIN_SLIDE_Z );
	MinSlide[2]->OpenWindow();
	MaxSlide[2] = new WButtonSlider( this, MaxEdit[2], IDPS_MAX_SLIDE_Z );
	MaxSlide[2]->OpenWindow();


	LockButton[0] = new WButton( this, IDPS_MIRROR_X );
	LockButton[0]->OpenWindow( TRUE, 0, 0, 0, 0, TEXT("") );

	LockButton[1] = new WButton( this, IDPS_MIRROR_Y );
	LockButton[1]->OpenWindow( TRUE, 0, 0, 0, 0, TEXT("") );

	LockButton[2] = new WButton( this, IDPS_MIRROR_Z );
	LockButton[2]->OpenWindow( TRUE, 0, 0, 0, 0, TEXT("") );

	LockOpenBitmap	= (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_UNLOCKED), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS ); check(LockOpenBitmap);
	LockSameBitmap	= (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_LOCK_SAME), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS ); check(LockSameBitmap);
	LockMirrorBitmap= (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_LOCK_MIRROR), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS ); check(LockMirrorBitmap);


	LinkCheck[0] = new WCheckBox( this, IDPS_LOCK_X );
	LinkCheck[0]->OpenWindow( TRUE, 0, 0, 0, 0, TEXT("") );

	LinkCheck[1] = new WCheckBox( this, IDPS_LOCK_Y );
	LinkCheck[1]->OpenWindow( TRUE, 0, 0, 0, 0, TEXT("") );

	LinkCheck[2] = new WCheckBox( this, IDPS_LOCK_Z );
	LinkCheck[2]->OpenWindow( TRUE, 0, 0, 0, 0, TEXT("") );

	LinkImageContainer = new WButton( this, IDPS_VECT_LINK );
	LinkImageContainer->OpenWindow( TRUE, 0, 0, 39, 63, TEXT("") );

	LinkImage[0] = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_LINK_12), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS ); check(LinkImage[0]);
	LinkImage[1] = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_LINK_13), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS ); check(LinkImage[1]);
	LinkImage[2] = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_LINK_123), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS ); check(LinkImage[2]);
	LinkImage[3] = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_LINK_23), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS ); check(LinkImage[3]);
	LinkImage[4] = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_LINK_1), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS ); check(LinkImage[0]);
	LinkImage[5] = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_LINK_2), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS ); check(LinkImage[0]);
	LinkImage[6] = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_LINK_3), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS ); check(LinkImage[0]);


	INT i;
	for(i = 0; i < 3; i++)
	{
		PlaceControl( MinEdit[i] );
		PlaceControl( MaxEdit[i] );
		PlaceControl( MinSlide[i] );
		PlaceControl( MaxSlide[i] );
		PlaceControl( LinkCheck[i] );
		PlaceControl( LockButton[i] );
	}
	PlaceControl( LinkImageContainer );


	Finalize();


	AlreadyChangingText = true; //THIS IS A HACK! - this makes sure that now, when the text values are not loaded, that text is not changed
	OnLinkClick();	
	AlreadyChangingText = false; //THIS IS A HACK! - see above



	for(i = 0; i < 3; i++)
	{
		MinEdit[i]->ChangeDelegate = FDelegate(this, (TDelegate)OnTextChangeMin);
		MaxEdit[i]->ChangeDelegate = FDelegate(this, (TDelegate)OnTextChangeMax);
		LinkCheck[i]->ClickDelegate = FDelegate(this, (TDelegate)OnLinkClick);
	}
	LockButton[0]->ClickDelegate = FDelegate(this, (TDelegate)OnLockClickX);
	LockButton[1]->ClickDelegate = FDelegate(this, (TDelegate)OnLockClickY);
	LockButton[2]->ClickDelegate = FDelegate(this, (TDelegate)OnLockClickZ);
}



void WRangeVectorEdit::OnLockClickX() { OnLockClick(0); }
void WRangeVectorEdit::OnLockClickY() { OnLockClick(1); }
void WRangeVectorEdit::OnLockClickZ() { OnLockClick(2); }
void WRangeVectorEdit::OnLockClick(INT button)
{
	if(LockState[button] == LOCK_None)
		SetLockState(button, LOCK_Same);
	else if(LockState[button] == LOCK_Same)
	{  
		if( (UseAbsMin && AbsMinValue >= 0.0) || (UseAbsMax && AbsMaxValue <= 0.0) )
			SetLockState(button, LOCK_None);
		else
			SetLockState(button, LOCK_Mirror);
	}
	else
		SetLockState(button, LOCK_None);
}



void WRangeVectorEdit::SetLockState(INT button, ELockState newLockState)
{
	LockState[button] = newLockState;
	switch(LockState[button])
	{
	case LOCK_None :
		LockButton[button]->SetBitmap(LockOpenBitmap);
		break;
	case LOCK_Same :
		LockButton[button]->SetBitmap(LockSameBitmap);
		break;
	case LOCK_Mirror :
		LockButton[button]->SetBitmap(LockMirrorBitmap);
		break;
	}
	EnableWindow( MinEdit[button]->hWnd, (LockState[button] == LOCK_None) && ToolEnabled );
	EnableWindow( MinSlide[button]->hWnd, (LockState[button] == LOCK_None) && ToolEnabled );
}



void WRangeVectorEdit::HandleLockText(INT button)
{
	if(LockState[button] == LOCK_Same)
	{
		MinEdit[button]->SetText( *MaxEdit[button]->GetText() );
	}
	else if(LockState[button] == LOCK_Mirror)
	{
		FLOAT maxVal = appAtof( *MaxEdit[button]->GetText() );
		MinEdit[button]->SetText(*FString::Printf(TEXT("%.3f"),-maxVal) );
	}


	//check for exceeding of abs Min and Max values
	for(INT i = 0; i < 3; i++)
	{
		if(UseAbsMin)
		{
			if(appAtof( *MinEdit[i]->GetText() ) < AbsMinValue)
				MinEdit[i]->SetText( *(FString::Printf(TEXT("%.3f"),AbsMinValue)) );
			if(appAtof( *MaxEdit[i]->GetText() ) < AbsMinValue)
				MaxEdit[i]->SetText( *(FString::Printf(TEXT("%.3f"),AbsMinValue)) );
		}
		if(UseAbsMax)
		{
			if(appAtof( *MinEdit[i]->GetText() ) > AbsMaxValue)
				MinEdit[i]->SetText( *(FString::Printf(TEXT("%.3f"),AbsMaxValue)) );
			if(appAtof( *MaxEdit[i]->GetText() ) > AbsMaxValue)
				MaxEdit[i]->SetText( *(FString::Printf(TEXT("%.3f"),AbsMaxValue)) );
		}
	}

}



void WRangeVectorEdit::OnLinkClick()
{
	if(ToolEnabled)
	{
		INT leader = -1;
		for(INT i = 0; i < 3; i++)
		{
			if(leader >= 0 && LinkCheck[i]->IsChecked())
			{
				EnableWindow( MinEdit[i]->hWnd, FALSE );
				EnableWindow( MaxEdit[i]->hWnd, FALSE );
				EnableWindow( LockButton[i]->hWnd, FALSE );
			}
			else
			{
				EnableWindow( MinEdit[i]->hWnd, LockState[i]==LOCK_None );
				EnableWindow( MaxEdit[i]->hWnd, TRUE );
				EnableWindow( LockButton[i]->hWnd, TRUE );
				
				if(LinkCheck[i]->IsChecked())
					leader = i;
			}
		}
	}

	if(LinkCheck[0]->IsChecked() && LinkCheck[1]->IsChecked() && LinkCheck[2]->IsChecked())
		LinkImageContainer->SetBitmap(LinkImage[2]);
	else if(LinkCheck[0]->IsChecked() && LinkCheck[1]->IsChecked())
		LinkImageContainer->SetBitmap(LinkImage[0]);
	else if(LinkCheck[0]->IsChecked() && LinkCheck[2]->IsChecked())
		LinkImageContainer->SetBitmap(LinkImage[1]);
	else if(LinkCheck[1]->IsChecked() && LinkCheck[2]->IsChecked())
		LinkImageContainer->SetBitmap(LinkImage[3]);
	else if(LinkCheck[0]->IsChecked())
		LinkImageContainer->SetBitmap(LinkImage[4]);
	else if(LinkCheck[1]->IsChecked())
		LinkImageContainer->SetBitmap(LinkImage[5]);
	else if(LinkCheck[2]->IsChecked())
		LinkImageContainer->SetBitmap(LinkImage[6]);
	else
		LinkImageContainer->SetBitmap(NULL);

	OnTextChangeMin();
	OnTextChangeMax();
}



void WRangeVectorEdit::OnTextChangeMin() { OnTextChange(TRUE); }
void WRangeVectorEdit::OnTextChangeMax() { OnTextChange(FALSE); }
void WRangeVectorEdit::OnTextChange(UBOOL minChange)
{
	//insure that this function doesn't loop forever
	if(!AlreadyChangingText && CallUpdateTarget)
	{
		AlreadyChangingText=TRUE;
		
		INT leader = -1;
		for(INT i = 0; i < 3; i++)
		{
			if(leader >= 0 && LinkCheck[i]->IsChecked())
			{
				MinEdit[i]->SetText(*MinEdit[leader]->GetText() );
				MaxEdit[i]->SetText(*MaxEdit[leader]->GetText() );
			}
			else
			{
				//save values for convenience
				FLOAT minVal = appAtof( *MinEdit[i]->GetText() );
				FLOAT maxVal = appAtof( *MaxEdit[i]->GetText() );

				//check for 0 crossover on mirror lock
				if(LockState[i] == LOCK_Mirror && maxVal < 0.0)
					MaxEdit[i]->SetText( *(FString::Printf(TEXT("%.3f"),0.0)) );
				
				//check for Min Max cross over
				if(minChange && minVal > maxVal)
					MaxEdit[i]->SetText( *MinEdit[i]->GetText() );
				else if(!minChange && maxVal < minVal)
					MinEdit[i]->SetText( *MaxEdit[i]->GetText() );
				
				//based on the lockstate set the text boxes correctly taking abs Min and Max into account
				HandleLockText(i);

				//set leader if checked
				if(LinkCheck[i]->IsChecked())
					leader = i;
			}
		}
		UpdateTarget();
		
		AlreadyChangingText=FALSE;
	}
}



void WRangeVectorEdit::GetValuesFromTarget()
{
	CallUpdateTarget = FALSE;
	MinEdit[0]->SetText(*FString::Printf(TEXT("%.3f"),VectorToEdit->X.Min) );
	MaxEdit[0]->SetText(*FString::Printf(TEXT("%.3f"),VectorToEdit->X.Max) );
	MinEdit[1]->SetText(*FString::Printf(TEXT("%.3f"),VectorToEdit->Y.Min) );
	MaxEdit[1]->SetText(*FString::Printf(TEXT("%.3f"),VectorToEdit->Y.Max) );
	MinEdit[2]->SetText(*FString::Printf(TEXT("%.3f"),VectorToEdit->Z.Min) );
	MaxEdit[2]->SetText(*FString::Printf(TEXT("%.3f"),VectorToEdit->Z.Max) );

	OnLinkClick();

	CallUpdateTarget = TRUE;
}


void WRangeVectorEdit::UpdateTargetUtil()
{
	if(VectorToEdit)
	{
		VectorToEdit->X.Min = appAtof( *MinEdit[0]->GetText() );
		VectorToEdit->Y.Min = appAtof( *MinEdit[1]->GetText() );
		VectorToEdit->Z.Min = appAtof( *MinEdit[2]->GetText() );
		VectorToEdit->X.Max = appAtof( *MaxEdit[0]->GetText() );
		VectorToEdit->Y.Max = appAtof( *MaxEdit[1]->GetText() );
		VectorToEdit->Z.Max = appAtof( *MaxEdit[2]->GetText() );
	}
}




//=============================================================================
// WRangeRotatorEdit 

WRangeRotatorEdit::WRangeRotatorEdit(WParticleEditorComponent* OwnerComponent, FString inVectorName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WRangeVectorEdit(OwnerComponent, inVectorName, ToolFlags | PTOOL_ExtraIndent, InUDNHelpTopic)
{
}

void WRangeRotatorEdit::OpenWindow(HMODULE InHMOD)
{
	WRangeVectorEdit::OpenWindow( InHMOD, IDPS_RANGE_ROTATOR );
}



	

//=============================================================================
// WRangeColorEdit 

WRangeColorEdit::WRangeColorEdit(WParticleEditorComponent* OwnerComponent, FString inVectorName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WRangeVectorEdit(OwnerComponent, inVectorName, ToolFlags | PTOOL_ExtraIndent, InUDNHelpTopic)
{
}

void WRangeColorEdit::OpenWindow(HMODULE InHMOD)
{
	WRangeVectorEdit::OpenWindow( InHMOD, IDPS_RANGE_COLOR );
}



	
//=============================================================================
// WColorScaleEdit 

WColorScaleEdit::WColorScaleEdit(WParticleEditorComponent* OwnerComponent, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool( OwnerComponent, FString(TEXT("Color Scale")), ToolFlags, InUDNHelpTopic), 
	size(FPoint(300, 60)),
	buttonWidth(15),
	buttonHeight(49),
	curButton(-1),
	lastClicked(-1),
	Enabled(TRUE)
{
}



void WColorScaleEdit::OnDestroy()
{
	for(INT i = 0; i < ColorButtons.Num(); i++)
		delete(ColorButtons(i));
	ColorButtons.Empty();

	for(INT i = 0; i < DeleteQueue.Num(); i++)
		delete(DeleteQueue(i));
	DeleteQueue.Empty();

	if(Box) {delete(Box); Box=NULL;}
	if(disableLabel) {delete(disableLabel); disableLabel=NULL;}

	for(INT i = 0;  i < ColorLabels.Num(); i++)
	{
		if(ColorLabels(i)){delete ColorLabels(i);	ColorLabels(i)	= NULL;}
		if(ColorEdits(i))	{delete ColorEdits(i);	ColorEdits(i)		= NULL;}
		if(ColorSlides(i)){delete ColorSlides(i);	ColorSlides(i)	= NULL;}
	}

	if(RelTimeLabel) {delete(RelTimeLabel); RelTimeLabel=NULL;}
	if(RelTimeEdit) {delete(RelTimeEdit); RelTimeEdit=NULL;}
	if(RelTimeSlide) {delete(RelTimeSlide); RelTimeSlide=NULL;}
}



FPoint WColorScaleEdit::GetSize() const
{
	return size + FPoint(100, 30);
}



void WColorScaleEdit::Enable(UBOOL inEnabled)
{
	Enabled = inEnabled;

	WParticleEditorTool::Enable(Enabled);
	EnableWindow( Box->hWnd, Enabled );

	for(INT i = 0; i < ColorButtons.Num(); i++)
		ColorButtons(i)->Show(Enabled);

	disableLabel->Show(!Enabled);

	if(lastClicked < 0)
	{
		for(INT i = 0; i < ColorLabels.Num(); i++)
		{
			EnableWindow( ColorLabels(i)->hWnd, FALSE );
			EnableWindow( ColorEdits(i)->hWnd, FALSE );
			EnableWindow( ColorSlides(i)->hWnd, FALSE );
		}

		EnableWindow( RelTimeLabel->hWnd, FALSE );
		EnableWindow( RelTimeEdit->hWnd, FALSE );
		EnableWindow( RelTimeSlide->hWnd, FALSE );
	}
}



void WColorScaleEdit::OpenWindow(HMODULE InHMOD )
{
	WPropertyPage::OpenWindow( 0, InHMOD );

	disableLabel = new WLabel(this, 0);
	disableLabel->OpenWindow(1, 0);
	::MoveWindow(disableLabel->hWnd, 2, 8, size.X - 20, 15, TRUE );
	disableLabel->SetText( TEXT(" - disabled -") );
	disableLabel->Show(FALSE);

	//create a Box to contain the buttons
	Box = new WGroupBox(this);
	Box->OpenWindow(TRUE);
	::MoveWindow(Box->hWnd, 0, 0, size.X, size.Y, TRUE );



	//create fields to edit colors
	TArray<FString> FieldNames;
	new(FieldNames)FString(TEXT("R"));
	new(FieldNames)FString(TEXT("G"));
	new(FieldNames)FString(TEXT("B"));
	new(FieldNames)FString(TEXT("A"));
	INT yStart = size.Y + 5;
	INT curX = 0;
	for(INT i = 0;  i < FieldNames.Num(); i++)
	{

		ColorLabels.AddItem( new WLabel(this, 0) );
		ColorLabels(i)->OpenWindow(1, 0);
		::MoveWindow(ColorLabels(i)->hWnd, curX, yStart + 3, 10, 26, TRUE );
		ColorLabels(i)->SetText( *FieldNames(i) );
		curX += 10;

		ColorEdits.AddItem( new WEdit(this, 0) );
		ColorEdits(i)->OpenWindow( TRUE, 0, 0 );
		::MoveWindow(ColorEdits(i)->hWnd, curX, yStart, 30, 23, TRUE );
		curX += 30;

		ColorSlides.AddItem( new WButtonSlider( this, ColorEdits(i), 0, TRUE ) );
		ColorSlides(i)->OpenWindow();
		::MoveWindow(ColorSlides(i)->hWnd, curX, yStart, 14, 26, TRUE );
		curX += 14 + 5;

		EnableWindow( ColorLabels(i)->hWnd, FALSE );
		EnableWindow( ColorEdits(i)->hWnd, FALSE );
		EnableWindow( ColorSlides(i)->hWnd, FALSE );
	}

	for(INT i = 0;  i < FieldNames.Num(); i++)
	{
		ColorEdits(i)->ChangeDelegate = FDelegate(this, (TDelegate)OnColorTextChange);
	}

	FieldNames.Empty();


	curX += 5;
	RelTimeLabel = new WLabel(this, 0);
	RelTimeLabel->OpenWindow(1, 0);
	::MoveWindow(RelTimeLabel->hWnd, curX, yStart + 3, 65, 26, TRUE );
	RelTimeLabel->SetText( TEXT("RelativeTime") );
	curX += 65;

	RelTimeEdit = new WEdit(this, 0);
	RelTimeEdit->OpenWindow( TRUE, 0, 0 );
	::MoveWindow(RelTimeEdit->hWnd, curX, yStart, 50, 23, TRUE );
	curX += 50;

	RelTimeSlide = new WButtonSlider( this, RelTimeEdit, 0, FALSE );
	RelTimeSlide->OpenWindow();
	::MoveWindow(RelTimeSlide->hWnd, curX, yStart, 14, 26, TRUE );

	EnableWindow( RelTimeLabel->hWnd, FALSE );
	EnableWindow( RelTimeEdit->hWnd, FALSE );
	EnableWindow( RelTimeSlide->hWnd, FALSE );

	RelTimeEdit->ChangeDelegate = FDelegate(this, (TDelegate)OnRelTimeTextChange);
}


void WColorScaleEdit::OnColorTextChange()
{
	if(lastClicked < 0 || !Enabled)
		return;

	INT colors[4];
	for(INT i = 0; i < 4; i++)
	{
		INT value = appAtoi( *(ColorEdits(i)->GetText()) );
		if(value < 0)
		{
			value = 0;
			ColorEdits(i)->SetText( TEXT("0") );
		}
		else if(value > 255)
		{
			value = 255;
			ColorEdits(i)->SetText( TEXT("255") );
		}

		colors[i] = value;
	}

	EditTarget->ColorScale(lastClicked).Color = FColor(colors[0], colors[1], colors[2], colors[3]);
	ColorButtons(lastClicked)->SetColor(colors[0], colors[1], colors[2], colors[3]);
}



void WColorScaleEdit::OnRelTimeTextChange()
{
	if(lastClicked < 0 || !Enabled)
		return;

	FLOAT value = appAtof( *(RelTimeEdit->GetText()) );
	if(value < 0.0)
	{
		value = 0.0;
		RelTimeEdit->SetText( TEXT("0.000") );
	}
	else if(value > 1.0)
	{
		value = 1.0;
		RelTimeEdit->SetText( TEXT("1.000") );
	}

	EditTarget->ColorScale(lastClicked).RelativeTime = value;
	MoveButton(lastClicked, value, FALSE);
}



void WColorScaleEdit::SetSelectedButton(INT ButtonIndex)
{
	if(ButtonIndex < 0 || ButtonIndex > ColorButtons.Num())
		return;

	//UBOOL refreshAfter = ButtonIndex != lastClicked;

	//make sure OnColorTextChange doesn't do anything
	lastClicked = -1;

	ColorEdits(0)->SetText(*FString::Printf(TEXT("%d"),EditTarget->ColorScale(ButtonIndex).Color.R) );
	ColorEdits(1)->SetText(*FString::Printf(TEXT("%d"),EditTarget->ColorScale(ButtonIndex).Color.G) );
	ColorEdits(2)->SetText(*FString::Printf(TEXT("%d"),EditTarget->ColorScale(ButtonIndex).Color.B) );
	ColorEdits(3)->SetText(*FString::Printf(TEXT("%d"),EditTarget->ColorScale(ButtonIndex).Color.A) );

	RelTimeEdit->SetText(*FString::Printf(TEXT("%.3f"),EditTarget->ColorScale(ButtonIndex).RelativeTime) );

	for(INT i = 0; i < ColorLabels.Num(); i++)
	{
		EnableWindow( ColorLabels(i)->hWnd, TRUE );
		EnableWindow( ColorEdits(i)->hWnd, TRUE );
		EnableWindow( ColorSlides(i)->hWnd, TRUE );
	}

	EnableWindow( RelTimeLabel->hWnd, TRUE );
	EnableWindow( RelTimeEdit->hWnd, TRUE );
	EnableWindow( RelTimeSlide->hWnd, TRUE );

	lastClicked = ButtonIndex;

	for(INT i = 0; i < ColorButtons.Num(); i++)
	{
		ColorButtons(i)->SetSelected(i == lastClicked);
	}
}



void WColorScaleEdit::ColorButtonDown(INT ButtonIndex)
{
	if(!Enabled)
		return;

	//set the button to be selected so the edits change that color and it looks selected
	SetSelectedButton(ButtonIndex);

	//don't let move the first button on the far left if there is not a button under it
	if( (ButtonIndex == 0) && (GetRelativeTimeFromPoint(ColorButtons(1)->GetLocation()) != 0.0) )
		return;

	//don't let move the last button on the far right if there is not a button under it
	if( (ButtonIndex == ColorButtons.Num()-1) && (GetRelativeTimeFromPoint(ColorButtons(ColorButtons.Num()-2)->GetLocation()) != 1.0) )
		return;

	//set this button to be dragged
	curButton = ButtonIndex;
}



void WColorScaleEdit::ColorButtonUp(INT ButtonIndex)
{
	if(!Enabled)
		return;

	//set tso no button is being dragged.
	curButton = -1;
}



void WColorScaleEdit::ColorButtonDoubleClick(INT ButtonIndex)
{
	PopUpColorPickerForButton(ButtonIndex);
}



void WColorScaleEdit::PopUpColorPickerForButton(INT ButtonIndex)
{
	if(!Enabled)
		return;

	INT R, G, B, A;
	ColorButtons(ButtonIndex)->GetColor(R, G, B, A);


	CHOOSECOLORA cc;
	static COLORREF acrCustClr[16];
	appMemzero( &cc, sizeof(cc) );
	cc.lStructSize  = sizeof(cc);
	cc.hwndOwner    = ColorButtons(ButtonIndex)->hWnd;
	cc.lpCustColors = (LPDWORD)acrCustClr;
	cc.rgbResult    = RGB(R, G, B);
	cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
	if( ChooseColorA(&cc)==TRUE )
	{
		ColorButtons(ButtonIndex)->SetColor(GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult), A);
		UpdateTarget();
	}

	SetSelectedButton(ButtonIndex);
}



void WColorScaleEdit::OnMouseMove(DWORD flags, FPoint Location)
{
	mouseLoc = Location;
}



void WColorScaleEdit::OnLeftButtonDoubleClick()
{
	if(!Enabled)
		return;

	INT index = InsertButton(GetRelativeTimeFromPoint(mouseLoc), FColor(128,128,128), TRUE);

	PopUpColorPickerForButton(index);
}



void WColorScaleEdit::ColorButtonRightClick(INT ButtonIndex) 
{
	DeleteButton(ButtonIndex);
}



void WColorScaleEdit::DeleteButton(INT ButtonIndex)
{
	if(!Enabled)
		return;

	//don't let delete the first button on the far left
	if(ButtonIndex == 0)
		return;

	//don't let delete the last button on the far right
	if(ButtonIndex == ColorButtons.Num() - 1)
		return;

	UBOOL deleteActive = ButtonIndex == lastClicked;

	ColorButtons(ButtonIndex)->DoDestroy();

	DeleteQueue.AddItem(ColorButtons(ButtonIndex));
	ColorButtons.Remove(ButtonIndex);
	EditTarget->ColorScale.Remove(ButtonIndex);

	for(INT i = ButtonIndex; i < ColorButtons.Num(); i++)
		ColorButtons(i)->SetIndex(i);

	UpdateTarget();

	//make sure can't still change  after deleted
	if(deleteActive)
		GetValuesFromTarget();
}



INT WColorScaleEdit::InsertButton(FLOAT relativeTime, FColor color, UBOOL insertInTargetToo)
{
	//insert button in UI
	INT index = ColorButtons.Num();
	ColorButtons.AddItem(new WColorScaleButton(this, this, index));
	ColorButtons(index)->OpenWindow(TRUE, 0, 0, buttonWidth, buttonHeight );
	ColorButtons(index)->Show(TRUE);
	ColorButtons(index)->SetColor(color.R, color.G, color.B, color.A);

	//insert colorscale in Emitter if insertInTargetToo
	if(insertInTargetToo)
	{
		EditTarget->ColorScale.AddZeroed(1);
		EditTarget->ColorScale(index).Color = FColor(color.R, color.G, color.B, color.A);
		EditTarget->ColorScale(index).RelativeTime = relativeTime;
	}

	//MoveButton and account for the fact that the index may change
	WColorScaleButton *tmp = ColorButtons(index);
	MoveButton(index, relativeTime);
	index = ColorButtons.FindItemIndex(tmp);
	
	return index;
}


void WColorScaleEdit::ColorButtonMove(FPoint Location)
{
	if(TRUE)
	{
		if(curButton >= 0)
		{
			MoveButton(curButton, GetRelativeTimeFromPoint(Location));
		}
	}
}



void WColorScaleEdit::MoveButton(INT ButtonIndex, FLOAT relativeTime, UBOOL updateText)
{
	if(relativeTime > 1.0)
		relativeTime = 1.0;

	if(relativeTime < 0.0)
		relativeTime = 0.0;

	FLOAT xLocf = (relativeTime * (FLOAT)(size.X - buttonWidth - 5)); 
	INT xLoc = xLocf + 2; 
	::MoveWindow(ColorButtons(ButtonIndex)->hWnd, xLoc, 8, buttonWidth, buttonHeight, TRUE);

	if(updateText)
	{
		INT temp = lastClicked;
		lastClicked = -1;
		RelTimeEdit->SetText(*FString::Printf(TEXT("%.3f"), relativeTime) );
		lastClicked = temp;
	}

	UpdateTarget();
}



FLOAT WColorScaleEdit::GetRelativeTimeFromPoint(FPoint Location)
{
	FLOAT relativeTime = (FLOAT)(Location.X - 2) / (FLOAT)(size.X - buttonWidth - 5); 

	if(relativeTime > 1.0)
		relativeTime = 1.0;

	if(relativeTime < 0.0)
		relativeTime = 0.0;

	return relativeTime;
}



void WColorScaleEdit::GetValuesFromTarget()
{
	CallUpdateTarget = FALSE;

	while(ColorButtons.Num() > 0)
	{
		ColorButtons(0)->DoDestroy();
		DeleteQueue.AddItem(ColorButtons(0));
		ColorButtons.Remove(0);
	}

	UBOOL HasFrontBar = FALSE;
	UBOOL HasBackBar = FALSE;
	for(INT i = 0; i < EditTarget->ColorScale.Num(); i++)
	{
		FParticleColorScale cs = EditTarget->ColorScale(i);

		if(cs.RelativeTime == 0)
			HasFrontBar = TRUE;

		if(cs.RelativeTime == 1.0)
			HasBackBar = TRUE;

		InsertButton(cs.RelativeTime, cs.Color, FALSE);
	}
	if(!HasFrontBar)
		InsertButton(0.0, FColor(255,255,255,255), TRUE);
	if(!HasBackBar)
		InsertButton(1.0, FColor(255,255,255,255), TRUE);

	//take care of no selected button
	lastClicked = -1;

	for(INT i = 0; i < ColorLabels.Num(); i++)
	{
		EnableWindow( ColorLabels(i)->hWnd, FALSE );
		EnableWindow( ColorEdits(i)->hWnd, FALSE );
		EnableWindow( ColorSlides(i)->hWnd, FALSE );
	}
	EnableWindow( RelTimeLabel->hWnd, FALSE );
	EnableWindow( RelTimeEdit->hWnd, FALSE );
	EnableWindow( RelTimeSlide->hWnd, FALSE );

	Enable(this->ToolEnabled);	

	CallUpdateTarget = TRUE;
}



void WColorScaleEdit::UpdateTargetUtil()
{
	//assume only one crossover occurs per move
	//go backwards so add works better
	UBOOL reorderHappened = FALSE;
	for(INT i = ColorButtons.Num()-2; i >= 0; i--)
	{
		FPoint p1 = ColorButtons(i)->GetLocation();
		FPoint p2 = ColorButtons(i+1)->GetLocation();

		if(p1.X > p2.X)
		{
			ColorButtons(i)->SetIndex(i+1);
			ColorButtons(i+1)->SetIndex(i);

			if(curButton == i)
				curButton++;
			else if(curButton == i+1)
				curButton--;

			if(lastClicked == i)
				lastClicked++;
			else if(lastClicked == i+1)
				lastClicked--;

			WColorScaleButton* tmp	= ColorButtons(i);
			ColorButtons(i)			= ColorButtons(i+1);
			ColorButtons(i+1)		= tmp;

			reorderHappened = TRUE;
		}
	}

	if(reorderHappened || curButton < 0)
	{
		for(INT i = 0; i < ColorButtons.Num(); i++)
		{		
			INT R, G, B, A;
			ColorButtons(i)->GetColor(R, G, B, A);
			EditTarget->ColorScale(i).Color = FColor(R, G, B, A);
			EditTarget->ColorScale(i).RelativeTime = GetRelativeTimeFromPoint(ColorButtons(i)->GetLocation());
		}
	}
	else
	{
		INT R, G, B, A;
		ColorButtons(curButton)->GetColor(R, G, B, A);
		EditTarget->ColorScale(curButton).Color = FColor(R, G, B, A);
		EditTarget->ColorScale(curButton).RelativeTime = GetRelativeTimeFromPoint(ColorButtons(curButton)->GetLocation());		
	}
}



void WColorScaleEdit::OnPaint()
{
	guard(WParticleEditorTab::OnPaint);
	PAINTSTRUCT PS;
	HDC hDC = BeginPaint( *this, &PS );
	FillRect( hDC, GetClientRect(), (HBRUSH)(COLOR_BTNFACE+1) );
	EndPaint( *this, &PS );
	unguard;
}




//=============================================================================
// WEnumEdit

WEnumEdit::WEnumEdit(class WParticleEditorComponent* OwnerComponent, FString InEnumName, TArray<FString> InEnumNameList, 
					 TArray<INT> InEnumIDList, DWORD ToolFlags, INT InUDNHelpTopic, UBOOL InEmitterPicker) :
	WParticleEditorTool( OwnerComponent, InEnumName, ToolFlags, InUDNHelpTopic),
	EnumNameList(InEnumNameList),
	EnumIDList(InEnumIDList),
	EnumToEdit(NULL),
	EmitterPicker(InEmitterPicker)
{
	
}

//=============================================================================
// WEnumEdit

WEnumEdit::WEnumEdit(WWindow *OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString InEnumName, TArray<FString> InEnumNameList, 
					 TArray<INT> InEnumIDList, DWORD ToolFlags, INT InUDNHelpTopic, UBOOL InEmitterPicker) :
	WParticleEditorTool( OwnerWindow, OwnerComponent, InEnumName, ToolFlags, InUDNHelpTopic),
	EnumNameList(InEnumNameList),
	EnumIDList(InEnumIDList),
	EnumToEdit(NULL),
	EmitterPicker(InEmitterPicker)
{

}



void WEnumEdit::SetTarget(INT* InEnumToEdit)
{
	WParticleEditorTool::SetTarget();
	EnumToEdit = InEnumToEdit;

	if(EmitterPicker)
	{
		ValEdit->Empty();
		EnumIDList.Empty();		
		EnumNameList.Empty();		

		// This control is a list of all the emitters in the system
		EnumIDList.AddItem(-1);
		new(EnumNameList)FString(TEXT("None"));

		// iterate over the other emitters and make entries for them
		for(INT i = 0; i < this->OwnerComponent->GetParentTab()->GetEmitter()->Emitters.Num(); i++)
		{
			EnumIDList.AddItem(i);
			new(EnumNameList)FString(this->OwnerComponent->GetParentTab()->GetEmitter()->Emitters(i)->Name);
		}

		for(INT i = 0; i < EnumNameList.Num(); i++)
		{
			ValEdit->AddString(*(EnumNameList(i)));
		}
		ValEdit->SetCurrent(0);
	}

}



FPoint WEnumEdit::GetSize() const
{
	return FPoint(300,23);
}



void WEnumEdit::OpenWindow(HMODULE InHMOD )
{
	WPropertyPage::OpenWindow( IDPS_ENUM, InHMOD );

	ValEdit = new WComboBox( this, IDPS_ENUM_VALUE );
	ValEdit->OpenWindow( TRUE, FALSE );
	
	for(INT i = 0; i < EnumNameList.Num(); i++)
	{
		ValEdit->AddString(*(EnumNameList(i)));
	}
	ValEdit->SetCurrent(0);

	PlaceControl( ValEdit );
	Finalize();
	ValEdit->SelectionChangeDelegate = FDelegate(this, (TDelegate)UpdateTarget);
}



void WEnumEdit::GetValuesFromTarget()
{
	// find the Location in the two TArrays for the current value of the enum
	INT index = EnumIDList.FindItemIndex(*EnumToEdit);

	// verify that it exists
	check(index != INDEX_NONE);

	// Find the index of that string in the combo Box
	INT boxIndex = ValEdit->FindString(*(EnumNameList(index)));

	// Set the combobox to that index
	ValEdit->SetCurrent(boxIndex);

	UpdateTarget();
}



void WEnumEdit::UpdateTargetUtil()
{
	//make sure enable nest correctly - this is important so Tools have the right values before functions are called on them in the begining
	for(INT i = 0; i < DisableInfo.Num(); i++)
	{
		DisableInfo(i)->Tool->GetValuesFromTarget();
	}

	// enable the previous disabled Tools
	for(INT i = 0; i < DisableInfo.Num(); i++)
	{
		INT index;
		check(EnumIDList.FindItem(*EnumToEdit, index));
		if(DisableInfo(i)->id == index)
		{
			DisableInfo(i)->Tool->Enable(1);
			//this->OwnerComponent->ShowTool(DisableInfo(i)->Tool, TRUE); //expand Tool if not Expanded - does not work
		}
	}

	//make sure enable nest correctly - this is only important to do for enable edits and enum edits - but I can't test type info
	for(INT i = 0; i < DisableInfo.Num(); i++)
	{
		if(DisableInfo(i)->id == *EnumToEdit)
		{
			DisableInfo(i)->Tool->GetValuesFromTarget();
			DisableInfo(i)->Tool->UpdateTarget();
		}
	}

	// Find the string value of the combo Box
	FString textValue = ValEdit->GetText();

	// find the index in the arrays that matches with tha string
	INT index = EnumNameList.FindItemIndex(textValue);

	// use the index to lookup and set the value of the enum
	*EnumToEdit = EnumIDList(index);

	// Now disable Tools if there are any specified
	for(INT i = 0; i < DisableInfo.Num(); i++)
	{
		if(DisableInfo(i)->id == index)
		{
			DisableInfo(i)->Tool->Enable(0);
		}
	}


	
}



void WEnumEdit::OnDestroy()
{
	if(ValEdit)
	{
		delete ValEdit;
		ValEdit = NULL;
	}

	for(INT i = 0; i < DisableInfo.Num(); i++)
	{
		delete DisableInfo(i);
	}
	DisableInfo.Empty();
}

void WEnumEdit::AddDisableTool(WParticleEditorTool* toolToDisable, INT enumID)
{
	DisableStruct* info = new DisableStruct;

	info->Tool = toolToDisable;
	info->id = enumID;
	DisableInfo.AddItem(info);
}


//=============================================================================
// WBoolEdit

WBoolEdit::WBoolEdit(WParticleEditorComponent* OwnerComponent, FString InBoolName, FString InNameOfBoolToEdit, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool( OwnerComponent, InBoolName, ToolFlags, InUDNHelpTopic), 
	NameOfBoolToEdit(InNameOfBoolToEdit)
{

}


FPoint WBoolEdit::GetSize() const
{
	return FPoint(170,15);
}


void WBoolEdit::OpenWindow(HMODULE InHMOD )
{
	WPropertyPage::OpenWindow( IDPS_BOOL, InHMOD );

	ValEdit = new WCheckBox( this, IDPS_BOOL_VALUE );
	ValEdit->OpenWindow( TRUE, 0, 0, 0, 0, TEXT("") );

	PlaceControl( ValEdit );
	Finalize();

	ValEdit->SetText(*GetToolName());

	ValEdit->ClickDelegate = FDelegate(this, (TDelegate)UpdateTarget);
}


void WBoolEdit::GetValuesFromTarget()
{
	CallUpdateTarget = FALSE;

	// Get the property where the UBOOL is stored
	UBoolProperty* Property = FindField<UBoolProperty>( EditTarget->GetClass(), *NameOfBoolToEdit );

	// Voodoo-magic:
	TCHAR boolText[10];
	Property->ExportText( 0, boolText, (BYTE*)EditTarget, (BYTE*)EditTarget, PPF_Localized );
	
	if(FString(boolText) == TEXT("True"))
	{
		ValEdit->SetCheck(TRUE);
	}
	else
	{
		ValEdit->SetCheck(FALSE);
	}

	CallUpdateTarget = TRUE;
}


void WBoolEdit::UpdateTargetUtil()
{
	UBoolProperty* Property = FindField<UBoolProperty>( EditTarget->GetClass(), *NameOfBoolToEdit );

	FString boolText;

	if(ValEdit->IsChecked())
	{
		boolText = TEXT("True");
	}
	else
	{
		boolText = TEXT("False");
	}

	Property->ImportText( *boolText, (BYTE*)EditTarget + Property->Offset, PPF_Localized );
}


void WBoolEdit::OnDestroy()
{
	if(ValEdit)
	{
		delete ValEdit;
		ValEdit = NULL;
	}
}




//=============================================================================
// WEnableEdit

WEnableEdit::WEnableEdit(WParticleEditorComponent* OwnerComponent, FString InBoolName, FString InNameOfBoolToEdit, UBOOL inInvertEnable, DWORD ToolFlags, INT InUDNHelpTopic) :
	WBoolEdit(OwnerComponent, InBoolName, InNameOfBoolToEdit, ToolFlags, InUDNHelpTopic),
	InvertEnable(inInvertEnable)
{
}


void WEnableEdit::AddTool(WParticleEditorTool* NewTool)
{
	ToolsToEnable.AddItem(NewTool);
}


void WEnableEdit::GetValuesFromTarget()
{
	WBoolEdit::GetValuesFromTarget();
	UpdateTarget();
}


void WEnableEdit::UpdateTargetUtil()
{
	WBoolEdit::UpdateTargetUtil();

	UBOOL enable = ValEdit->IsChecked() ^ InvertEnable; //XOR

	//make sure enable nest correctly - this is important so Tools have the right values before functions are called on them in the begining
	for(INT i = 0; i < ToolsToEnable.Num(); i++)
	{
		ToolsToEnable(i)->GetValuesFromTarget();
	}

	for(INT i = 0; i < ToolsToEnable.Num(); i++)
		ToolsToEnable(i)->Enable(enable);

	//make sure enable nest correctly - this is only important to do for enable edits and enum edits - but I can't test type info
	for(INT i = 0; i < ToolsToEnable.Num(); i++)
	{
		if(enable)
		{
			ToolsToEnable(i)->UpdateTarget();
		}
	}
}


//=============================================================================
// WNameEdit

WNameEdit::WNameEdit(WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool( OwnerComponent, InStringName, ToolFlags, InUDNHelpTopic), 
	NameToEdit(NULL)
{
		
}


WNameEdit::WNameEdit(WWindow *OwnerWindow, WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool(OwnerWindow, OwnerComponent, InStringName, ToolFlags, InUDNHelpTopic), 
	NameToEdit(NULL)
{
		
}


void WNameEdit::SetTarget(FName *InNameToEdit)
{
	WParticleEditorTool::SetTarget();
	NameToEdit = InNameToEdit;
}


FPoint WNameEdit::GetSize() const
{
	return FPoint(300, 23);
}


void WNameEdit::OpenWindow(HMODULE InHMOD )
{
	WPropertyPage::OpenWindow( IDPS_STRING, InHMOD );

	ValEdit = new WEdit( this, IDPS_STRING_VALUE );
	ValEdit->OpenWindow( TRUE, 0, 0 );

	PlaceControl( ValEdit );

	Finalize();

	ValEdit->ChangeDelegate = FDelegate(this, (TDelegate)UpdateTarget);
}


void WNameEdit::GetValuesFromTarget()
{
	CallUpdateTarget = FALSE;
	ValEdit->SetText( *(*NameToEdit) );
	CallUpdateTarget = TRUE;
}


void WNameEdit::UpdateTargetUtil()
{
	*NameToEdit = FName(*ValEdit->GetText());
}


void WNameEdit::OnDestroy()
{
	if(ValEdit)
	{
		delete ValEdit;
		ValEdit = NULL;
	}
}


//=============================================================================
// WStringEdit

WStringEdit::WStringEdit(WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool( OwnerComponent, InStringName, ToolFlags, InUDNHelpTopic), 
	StringToEdit(NULL)
{
		
}

WStringEdit::WStringEdit(WWindow *OwnerWindow, WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool(OwnerWindow, OwnerComponent, InStringName, ToolFlags, InUDNHelpTopic), 
	StringToEdit(NULL)
{
		
}


void WStringEdit::OnDestroy()
{
	if(ValEdit)
	{
		delete ValEdit;
		ValEdit = NULL;
	}
}



void WStringEdit::SetTarget(FString *InStringToEdit)
{
	WParticleEditorTool::SetTarget();
	StringToEdit = InStringToEdit;
}



FPoint WStringEdit::GetSize() const
{
	return FPoint(300, 23);
}


void WStringEdit::OpenWindow(HMODULE InHMOD )
{
	WPropertyPage::OpenWindow( IDPS_STRING, InHMOD );

	ValEdit = new WEdit( this, IDPS_STRING_VALUE );
	ValEdit->OpenWindow( TRUE, 0, 0 );

	PlaceControl( ValEdit );

	Finalize();

	ValEdit->ChangeDelegate = FDelegate(this, (TDelegate)UpdateTarget);
}


void WStringEdit::GetValuesFromTarget()
{
	CallUpdateTarget = FALSE;
	ValEdit->SetText( *(*StringToEdit) );
	CallUpdateTarget = TRUE;
}


void WStringEdit::UpdateTargetUtil()
{
	*StringToEdit = ValEdit->GetText();
}




//=============================================================================
// WPercentEdit

WPercentEdit::WPercentEdit(WParticleEditorComponent* OwnerComponent, FString inToolName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool( OwnerComponent, inToolName, ToolFlags, InUDNHelpTopic), 
	FloatToEdit(NULL),
	DontCallDelegates(FALSE)
{
		
}



WPercentEdit::WPercentEdit(WWindow *OwnerWindow, WParticleEditorComponent* OwnerComponent, FString inToolName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool( OwnerWindow, OwnerComponent, inToolName, ToolFlags, InUDNHelpTopic), 
	FloatToEdit(NULL),
	DontCallDelegates(FALSE)
{
		
}



void WPercentEdit::OnDestroy()
{
	if(PercentEdit)		{ delete PercentEdit;	PercentEdit = NULL; }
	if(PercentTrack)	{ delete PercentTrack;	PercentTrack = NULL; }
}


void WPercentEdit::SetTarget(FLOAT *InFloatToEdit)
{
	WParticleEditorTool::SetTarget();
	FloatToEdit = InFloatToEdit;
}


FPoint WPercentEdit::GetSize() const
{
	return FPoint(300, 25);
}



void WPercentEdit::OpenWindow(HMODULE InHMOD )
{
	WPropertyPage::OpenWindow( IDPS_PERCENT, InHMOD );

	PercentEdit = new WEdit( this, IDPS_PERCENT_EDIT );
	PercentEdit->OpenWindow( TRUE, 0, 0 );

	PercentTrack = new WTrackBar( this, IDPS_PERCENT_TRACK );
	PercentTrack->OpenWindow( 1, 0 );


	PlaceControl( PercentEdit );
	PlaceControl( PercentTrack );
	Finalize();

	PercentTrack->SetRange(0, 100);

	PercentEdit->ChangeDelegate = FDelegate(this, (TDelegate)OnEditChange);
	PercentTrack->ThumbTrackDelegate	= FDelegate(this, (TDelegate)OnTrackChange);
	PercentTrack->ThumbPositionDelegate	= FDelegate(this, (TDelegate)OnTrackChange);
}



void WPercentEdit::OnEditChange()
{
	//This avoids loops
	if(DontCallDelegates)
		return;

	INT value = appAtoi(*PercentEdit->GetText());

	if(value < 0)
		PercentEdit->SetText( TEXT("0") );
	else if(value > 100)
		PercentEdit->SetText( TEXT("100") );

	DontCallDelegates = TRUE;
	PercentTrack->SetPos( value );
	DontCallDelegates = FALSE;

	UpdateTarget();
}



void WPercentEdit::OnTrackChange()
{
	//This avoids loops
	if(DontCallDelegates)
		return;

	DontCallDelegates = TRUE;
	PercentEdit->SetText( *FString::Printf(TEXT("%d"),PercentTrack->GetPos()) );
	DontCallDelegates = FALSE;

	UpdateTarget();
}



void WPercentEdit::GetValuesFromTarget()
{
	CallUpdateTarget = FALSE;

	INT value = (INT)(100 * (*FloatToEdit));
	PercentEdit->SetText( *FString::Printf(TEXT("%d"), value) );
	PercentTrack->SetPos( value );

	CallUpdateTarget = TRUE;
}



void WPercentEdit::UpdateTargetUtil()
{
	INT value = appAtoi(*PercentEdit->GetText());
	*FloatToEdit = (FLOAT)value / 100.0f;
}




//=============================================================================
// WTexturePickEdit

WTexturePickEdit::WTexturePickEdit(class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool( OwnerComponent, InStringName, ToolFlags, InUDNHelpTopic), 
	TextureToEdit(NULL),
	ValEdit(NULL),
	UseButton(NULL)
{
	
}

WTexturePickEdit::WTexturePickEdit(WWindow* inOwnerWindow, class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool(inOwnerWindow, OwnerComponent, InStringName, ToolFlags, InUDNHelpTopic), 
	TextureToEdit(NULL),
	ValEdit(NULL),
	UseButton(NULL)
{
	
}



void WTexturePickEdit::OnDestroy()
{
	if(ValEdit)
	{
		delete ValEdit;
		ValEdit = NULL;
	}

	if(UseButton)
	{
		delete UseButton;
		UseButton = NULL;
	}
}



void WTexturePickEdit::SetTarget(UMaterial** InTextureToEdit)
{
	WParticleEditorTool::SetTarget();
	TextureToEdit = InTextureToEdit;
}



FPoint WTexturePickEdit::GetSize() const
{
	return FPoint(300, 23);
}


void WTexturePickEdit::OpenWindow(HMODULE InHMOD )
{
	WPropertyPage::OpenWindow( IDPS_TEXTURE, InHMOD );

	ValEdit = new WEdit( this, IDPS_TEXTURE_VALUE );
	ValEdit->OpenWindow( TRUE, 0, 0 );

	UseButton = new WButton(this, IDPS_TEXTURE_USE);
	UseButton->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	PlaceControl( ValEdit );
	PlaceControl( UseButton );

	Finalize();

	ValEdit->ChangeDelegate = FDelegate(this, (TDelegate)UpdateTarget);
	UseButton->ClickDelegate = FDelegate(this, (TDelegate)OnUseClick);
}


void WTexturePickEdit::GetValuesFromTarget()
{
	CallUpdateTarget = FALSE;
	ValEdit->SetText( (*TextureToEdit)->GetPathName());
	CallUpdateTarget = TRUE;
}


void WTexturePickEdit::OnUseClick()
{
	ValEdit->SetText(GUnrealEd->CurrentMaterial->GetPathName());
	UpdateTargetUtil();
}


void WTexturePickEdit::UpdateTargetUtil()
{
	(*TextureToEdit) = Cast<UMaterial>(UObject::StaticLoadObject( UMaterial::StaticClass(), NULL, *(ValEdit->GetText()), NULL, LOAD_NoWarn | RF_Native, NULL ));
}



//=============================================================================
// WSoundPickEdit

WSoundPickEdit::WSoundPickEdit(class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool( OwnerComponent, InStringName, ToolFlags, InUDNHelpTopic), 
	SoundToEdit(NULL),
	ValEdit(NULL),
	UseButton(NULL)
{
	
}

WSoundPickEdit::WSoundPickEdit(WWindow* inOwnerWindow, class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool(inOwnerWindow, OwnerComponent, InStringName, ToolFlags, InUDNHelpTopic), 
	SoundToEdit(NULL),
	ValEdit(NULL),
	UseButton(NULL)
{
	
}



void WSoundPickEdit::OnDestroy()
{
	if(ValEdit)
	{
		delete ValEdit;
		ValEdit = NULL;
	}

	if(UseButton)
	{
		delete UseButton;
		UseButton = NULL;
	}
}


void WSoundPickEdit::SetTarget(USound** InSoundToEdit)
{
	WParticleEditorTool::SetTarget();
	SoundToEdit = InSoundToEdit;
}


FPoint WSoundPickEdit::GetSize() const
{
	return FPoint(300, 23);
}


void WSoundPickEdit::OpenWindow(HMODULE InHMOD )
{
	WPropertyPage::OpenWindow( IDPS_TEXTURE, InHMOD );

	ValEdit = new WEdit( this, IDPS_TEXTURE_VALUE );
	ValEdit->OpenWindow( TRUE, 0, 0 );

	UseButton = new WButton(this, IDPS_TEXTURE_USE);
	UseButton->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	PlaceControl( ValEdit );
	PlaceControl( UseButton );

	Finalize();

	ValEdit->ChangeDelegate = FDelegate(this, (TDelegate)UpdateTarget);
	UseButton->ClickDelegate = FDelegate(this, (TDelegate)OnUseClick);
}


void WSoundPickEdit::GetValuesFromTarget()
{
	CallUpdateTarget = FALSE;
	ValEdit->SetText( (*SoundToEdit)->GetPathName());
	CallUpdateTarget = TRUE;
}


void WSoundPickEdit::OnUseClick()
{
	WBrowser* browserSound = *(GBrowserMaster->Browsers[eBROWSER_SOUND]);
	ValEdit->SetText(*(browserSound->GetCurrentPathName()));
	UpdateTargetUtil();
}


void WSoundPickEdit::UpdateTargetUtil()
{
	(*SoundToEdit) = Cast<USound>(UObject::StaticLoadObject( USound::StaticClass(), NULL, *(ValEdit->GetText()), NULL, LOAD_NoWarn | RF_Native, NULL ));
}



//=============================================================================
// WMeshPickEdit

WMeshPickEdit::WMeshPickEdit(class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool( OwnerComponent, InStringName, ToolFlags, InUDNHelpTopic), 
	MeshToEdit(NULL),
	ValEdit(NULL),
	UseButton(NULL)
{
	
}


void WMeshPickEdit::OnDestroy()
{
	if(ValEdit)
	{
		delete ValEdit;
		ValEdit = NULL;
	}

	if(UseButton)
	{
		delete UseButton;
		UseButton = NULL;
	}
}

void WMeshPickEdit::SetTarget(UStaticMesh** InMeshToEdit)
{
	WParticleEditorTool::SetTarget();
	MeshToEdit = InMeshToEdit;
}



FPoint WMeshPickEdit::GetSize() const
{
	return FPoint(300, 23);
}


void WMeshPickEdit::OpenWindow(HMODULE InHMOD )
{
	WPropertyPage::OpenWindow( IDPS_TEXTURE, InHMOD );

	ValEdit = new WEdit( this, IDPS_TEXTURE_VALUE );
	ValEdit->OpenWindow( TRUE, 0, 0 );

	UseButton = new WButton(this, IDPS_TEXTURE_USE);
	UseButton->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	PlaceControl( ValEdit );
	PlaceControl( UseButton );

	Finalize();

	ValEdit->ChangeDelegate = FDelegate(this, (TDelegate)UpdateTarget);
	UseButton->ClickDelegate = FDelegate(this, (TDelegate)OnUseClick);
}


void WMeshPickEdit::GetValuesFromTarget()
{
	CallUpdateTarget = FALSE;
	ValEdit->SetText( (*MeshToEdit)->GetPathName());
	CallUpdateTarget = TRUE;
}


void WMeshPickEdit::OnUseClick()
{
	ValEdit->SetText(GUnrealEd->CurrentStaticMesh->GetPathName());
	UpdateTargetUtil();
}


void WMeshPickEdit::UpdateTargetUtil()
{
	(*MeshToEdit) = Cast<UStaticMesh>(UObject::StaticLoadObject( UStaticMesh::StaticClass(), NULL, *(ValEdit->GetText()), NULL, LOAD_NoWarn | RF_Native, NULL ));
}



//=============================================================================
// WXYZVectorEdit 

WXYZVectorEdit::WXYZVectorEdit(WParticleEditorComponent* OwnerComponent, FString inVectorName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WVectorEdit<FLOAT>(OwnerComponent, inVectorName, FALSE, 15, ToolFlags | PTOOL_ExtraIndent, InUDNHelpTopic)
{
	new(FieldNames)FString(TEXT("X"));
	new(FieldNames)FString(TEXT("Y"));
	new(FieldNames)FString(TEXT("Z"));

	data.AddItem( NULL );
	data.AddItem( NULL );
	data.AddItem( NULL );
}

WXYZVectorEdit::WXYZVectorEdit(WWindow *OwnerWindow, WParticleEditorComponent* OwnerComponent, FString inVectorName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WVectorEdit<FLOAT>(OwnerWindow, OwnerComponent, inVectorName, FALSE, 15, ToolFlags | PTOOL_ExtraIndent, InUDNHelpTopic)
{
	new(FieldNames)FString(TEXT("X"));
	new(FieldNames)FString(TEXT("Y"));
	new(FieldNames)FString(TEXT("Z"));

	data.AddItem( NULL );
	data.AddItem( NULL );
	data.AddItem( NULL );
}

void WXYZVectorEdit::SetTarget(FVector *VectorToEdit)
{
	WParticleEditorTool::SetTarget();
	data(0) = &(VectorToEdit->X);
	data(1) = &(VectorToEdit->Y);
	data(2) = &(VectorToEdit->Z);
}

FLOAT WXYZVectorEdit::GetDataFromString(FString DataString)
{
	return appAtof( *DataString );
}

FString WXYZVectorEdit::GetStringFromData(FLOAT DataItem)
{
	return FString::Printf(TEXT("%.3f"),DataItem);
}
	



//=============================================================================
// WColorEdit 


WColorEdit::WColorEdit(WParticleEditorComponent* OwnerComponent, FString inColorName, FColor *inColorToEdit, DWORD ToolFlags, INT InUDNHelpTopic) :
	WVectorEdit<BYTE>(OwnerComponent, inColorName, TRUE, 15, ToolFlags | PTOOL_ExtraIndent, InUDNHelpTopic),
	ColorToEdit(inColorToEdit),
	PickButton(NULL)
{
	Init();
}


WColorEdit::WColorEdit(WWindow* OwnerWindow, WParticleEditorComponent* OwnerComponent, FString inColorName, FColor *inColorToEdit, DWORD ToolFlags, INT InUDNHelpTopic) :
	WVectorEdit<BYTE>(OwnerWindow, OwnerComponent, inColorName, TRUE, 15, ToolFlags, InUDNHelpTopic),
	ColorToEdit(inColorToEdit)
{
	Init();
}


void WColorEdit::OnDestroy()
{
	delete PickButton;
	DeleteObject( ColorPickBitmap );

	WVectorEdit<BYTE>::OnDestroy();
}

void WColorEdit::OpenWindow( HMODULE InHMOD )
{
	WVectorEdit<BYTE>::OpenWindow(InHMOD);

	// create the pick button
	PickButton = new WColorButton( this, 0, FDelegate(this, (TDelegate)PickClicked));
	PickButton->OpenWindow(TRUE, 0);
	PickButton->MoveWindow(365, 1, 20, 20, TRUE);
	PickButton->SetColor(ColorToEdit->R, ColorToEdit->G, ColorToEdit->B);
}



void WColorEdit::PickClicked()
{
	// ripped from somehwere else in unrealed...

	INT R, G, B;
	R = ColorToEdit->R;
	G = ColorToEdit->G;
	B = ColorToEdit->B;

	CHOOSECOLORA cc;
	static COLORREF acrCustClr[16];
	appMemzero( &cc, sizeof(cc) );
	cc.lStructSize  = sizeof(cc);
	cc.hwndOwner    = PickButton->hWnd;
	cc.lpCustColors = (LPDWORD)acrCustClr;
	cc.rgbResult    = RGB(R, G, B);
	cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
	if( ChooseColorA(&cc)==TRUE )
	{
		debugf(TEXT("TRUE, something..."));
		//ColorButton->SetColor( GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult) );
	}

	ColorToEdit->R = GetRValue(cc.rgbResult);
	ColorToEdit->G = GetGValue(cc.rgbResult);
	ColorToEdit->B = GetBValue(cc.rgbResult);


	GetValuesFromTarget();
	UpdateTargetUtil();
}

void WColorEdit::UpdateTargetUtil()
{
	WVectorEdit<BYTE>::UpdateTargetUtil();

	if(PickButton)
	{
		PickButton->SetColor(ColorToEdit->R, ColorToEdit->G, ColorToEdit->B);
	}
}


void WColorEdit::Init()
{

	ColorPickBitmap	= (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_COLOR_PICK2), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS ); 
	check(ColorPickBitmap);

	//NOTE fix needed : Min and max don't work because when the edit field is cast to a byte it is never out a range
	SetMin(0);
	SetMax(255);

	new(FieldNames)FString(TEXT("R"));
	new(FieldNames)FString(TEXT("G"));
	new(FieldNames)FString(TEXT("B"));
	new(FieldNames)FString(TEXT("A"));

	data.AddItem( &(ColorToEdit->R) );
	data.AddItem( &(ColorToEdit->G) );
	data.AddItem( &(ColorToEdit->B) );
	data.AddItem( &(ColorToEdit->A) );
}

BYTE WColorEdit::GetDataFromString(FString DataString)
{
	return appAtoi( *DataString );
}

FString WColorEdit::GetStringFromData(BYTE DataItem)
{
	return FString::Printf(TEXT("%d"),DataItem);
}



//=============================================================================
// WColorMultEdit 
WColorMultEdit::WColorMultEdit(WParticleEditorComponent* OwnerComponent, FString inColorName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WVectorEdit<FLOAT>(OwnerComponent, inColorName, FALSE, 15, ToolFlags | PTOOL_ExtraIndent, InUDNHelpTopic)
{
	SetMin(0.0);

	new(FieldNames)FString(TEXT("R"));
	new(FieldNames)FString(TEXT("G"));
	new(FieldNames)FString(TEXT("B"));
	new(FieldNames)FString(TEXT("A"));

	data.AddItem( NULL );
	data.AddItem( NULL );
	data.AddItem( NULL );
	data.AddItem( NULL );
}

void WColorMultEdit::SetTarget(FPlane *ColorToEdit)
{
	WParticleEditorTool::SetTarget();
	data(0) = &(ColorToEdit->X);
	data(1) = &(ColorToEdit->Y);
	data(2) = &(ColorToEdit->Z);
	data(3) = &(ColorToEdit->W);
}

FLOAT WColorMultEdit::GetDataFromString(FString DataString)
{
	return appAtof( *DataString );
}

FString WColorMultEdit::GetStringFromData(FLOAT DataItem)
{
	return FString::Printf(TEXT("%.3f"),DataItem);
}

	


//=============================================================================
// WPlaneEdit 
WPlaneEdit::WPlaneEdit(WParticleEditorComponent* OwnerComponent, FString inPlaneName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WVectorEdit<FLOAT>(OwnerComponent, inPlaneName, FALSE, 15, ToolFlags | PTOOL_ExtraIndent, InUDNHelpTopic)
{
	new(FieldNames)FString(TEXT("X"));
	new(FieldNames)FString(TEXT("Y"));
	new(FieldNames)FString(TEXT("Z"));
	new(FieldNames)FString(TEXT("W"));

	data.AddItem( NULL );
	data.AddItem( NULL );
	data.AddItem( NULL );
	data.AddItem( NULL );
}

WPlaneEdit::WPlaneEdit(WWindow* OwnerWindow, WParticleEditorComponent* OwnerComponent, FString inPlaneName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WVectorEdit<FLOAT>(OwnerWindow, OwnerComponent, inPlaneName, FALSE, 15, ToolFlags | PTOOL_ExtraIndent, InUDNHelpTopic)
{
	new(FieldNames)FString(TEXT("X"));
	new(FieldNames)FString(TEXT("Y"));
	new(FieldNames)FString(TEXT("Z"));
	new(FieldNames)FString(TEXT("W"));

	data.AddItem( NULL );
	data.AddItem( NULL );
	data.AddItem( NULL );
	data.AddItem( NULL );
}


void WPlaneEdit::SetTarget(FPlane *PlaneToEdit)
{
	WParticleEditorTool::SetTarget();
	data(0) = &(PlaneToEdit->X);
	data(1) = &(PlaneToEdit->Y);
	data(2) = &(PlaneToEdit->Z);
	data(3) = &(PlaneToEdit->W);
}


FLOAT WPlaneEdit::GetDataFromString(FString DataString)
{
	return appAtof( *DataString );
}

FString WPlaneEdit::GetStringFromData(FLOAT DataItem)
{
	return FString::Printf(TEXT("%.3f"),DataItem);
}

	


//=============================================================================
// WRotatorEdit 
WRotatorEdit::WRotatorEdit(WParticleEditorComponent* OwnerComponent, FString RotatorName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WVectorEdit<INT>(OwnerComponent, RotatorName, FALSE, 30, ToolFlags | PTOOL_ExtraIndent, InUDNHelpTopic)
{
	SetMin(-360);
	SetMax(360);

	new(FieldNames)FString(TEXT("Yaw"));
	new(FieldNames)FString(TEXT("Pitch"));
	new(FieldNames)FString(TEXT("Roll"));

	data.AddItem( NULL );
	data.AddItem( NULL );
	data.AddItem( NULL );
}


void WRotatorEdit::SetTarget(FRotator *RotatorToEdit)
{
	WParticleEditorTool::SetTarget();
	data(0) = &(RotatorToEdit->Yaw);
	data(1) = &(RotatorToEdit->Pitch);
	data(2) = &(RotatorToEdit->Roll);
}


INT WRotatorEdit::FinalTransform(INT ShownVal)
{
	INT tempVal = ShownVal;

	if(tempVal < 0)
		tempVal = 360 + tempVal;

	return (INT) ((FLOAT)tempVal / 360.0 * 65535.0);
}


INT WRotatorEdit::PreTransform(INT BasicValue)
{
	return (INT) ((FLOAT)BasicValue / 65535.0 * 360.0);  //This is flawed because it can change the existing value
}


INT WRotatorEdit::GetDataFromString(FString DataString)
{
	return appAtoi( *DataString );
}

FString WRotatorEdit::GetStringFromData(INT DataItem)
{
	return FString::Printf(TEXT("%d.000"),DataItem);
}

	


//=============================================================================
// WFloatEdit 
WFloatEdit::WFloatEdit(class WParticleEditorComponent* OwnerComponent, FString FloatName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WVectorEdit<FLOAT>(OwnerComponent, FloatName, FALSE, 0, ToolFlags, InUDNHelpTopic)
{
	new(FieldNames)FString(TEXT(""));
	data.AddItem( NULL );
}

WFloatEdit::WFloatEdit(WWindow* OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString FloatName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WVectorEdit<FLOAT>(OwnerWindow, OwnerComponent, FloatName, FALSE, 0, ToolFlags, InUDNHelpTopic)
{
	new(FieldNames)FString(TEXT(""));
	data.AddItem( NULL );
}

void WFloatEdit::SetTarget(FLOAT* FloatToEdit)
{
	WParticleEditorTool::SetTarget();
	data(0) = FloatToEdit;
}

FPoint WFloatEdit::GetSize() const
{
	return FPoint(150, 23);
}

FLOAT WFloatEdit::GetDataFromString(FString DataString)
{
	return appAtof( *DataString );
}

FString WFloatEdit::GetStringFromData(FLOAT DataItem)
{
	return FString::Printf(TEXT("%.3f"),DataItem);
}

	


//=============================================================================
// WIntEdit 
WIntEdit::WIntEdit(class WParticleEditorComponent* OwnerComponent, FString IntName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WVectorEdit<INT>(OwnerComponent, IntName, TRUE, 0, ToolFlags, InUDNHelpTopic)
{
	new(FieldNames)FString(TEXT(""));
	data.AddItem( NULL );
}

WIntEdit::WIntEdit(WWindow *OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString IntName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WVectorEdit<INT>(OwnerWindow, OwnerComponent, IntName, TRUE, 0, ToolFlags, InUDNHelpTopic)
{
	new(FieldNames)FString(TEXT(""));
	data.AddItem( NULL );
}

void WIntEdit::SetTarget(INT* IntToEdit)
{
	WParticleEditorTool::SetTarget();
	data(0) = IntToEdit;
}

FPoint WIntEdit::GetSize() const
{
	return FPoint(150, 23);
}

INT WIntEdit::GetDataFromString(FString DataString)
{
	return appAtoi( *DataString );
}

FString WIntEdit::GetStringFromData(INT DataItem)
{
	return FString::Printf(TEXT("%d"),DataItem);
}

	


//=============================================================================
// WNoNegNumberEdit 
WNoNegNumberEdit::WNoNegNumberEdit(class WParticleEditorComponent* OwnerComponent, FString NumberName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WIntEdit(OwnerComponent, NumberName, ToolFlags, InUDNHelpTopic)
{
	SetMin(0);
}

	


//=============================================================================
// WFadingEdit 

WFadingEdit::WFadingEdit(WParticleEditorComponent* OwnerComponent, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool( OwnerComponent, FString::Printf(TEXT("Fading")), ToolFlags | PTOOL_ExtraIndent, InUDNHelpTopic), 
	LockSliders(FALSE),
	SliderDiff(0)
{
}



void WFadingEdit::OnDestroy()
{
	if(FadeInBar)	{delete FadeInBar;	FadeInBar	= NULL;}
	if(fadeOutBar)	{delete fadeOutBar;	fadeOutBar	= NULL;}
	if(FadeInEdit)	{delete FadeInEdit;	FadeInEdit	= NULL;}
	if(FadeOutEdit)	{delete FadeOutEdit;FadeOutEdit	= NULL;}
	if(LockSlidersCheck) {delete LockSlidersCheck;LockSlidersCheck = NULL;}
	DeleteObject( LockSlidersBitmap );
}



FPoint WFadingEdit::GetSize() const
{
	return FPoint(370, 42);
}



void WFadingEdit::OpenWindow(HMODULE InHMOD )
{
	WPropertyPage::OpenWindow( IDPS_FADING, InHMOD );

	FadeInBar = new WTrackBar( this, IDPS_FADEIN_SLIDE );
	FadeInBar->OpenWindow( 1, 0 );
	fadeOutBar = new WTrackBar( this, IDPS_FADEOUT_SLIDE );
	fadeOutBar->OpenWindow( 1, 0 );

	FadeInEdit = new WEdit( this, IDPS_FADEIN_EDIT );
	FadeInEdit->OpenWindow( TRUE, 0, 0 );
	FadeOutEdit = new WEdit( this, IDPS_FADEOUT_EDIT );
	FadeOutEdit->OpenWindow( TRUE, 0, 0 );

	LockSlidersCheck = new WCheckBox( this, IDCK_FADE_LOCK_SLIDERS );
	LockSlidersCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );

	PlaceControl( FadeInBar );
	PlaceControl( fadeOutBar );
	PlaceControl( FadeInEdit );
	PlaceControl( FadeOutEdit );
	PlaceControl( LockSlidersCheck );
	Finalize();

	FadeInBar->SetRange( 0, 100);
	fadeOutBar->SetRange( 0, 100);

	LockSlidersBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_LOCK), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );	check(LockSlidersBitmap);
	LockSlidersCheck->SetBitmap( LockSlidersBitmap );
	LockSlidersCheck->SetCheck( LockSliders ? BST_CHECKED : BST_UNCHECKED );

	FadeInBar->ThumbTrackDelegate		= FDelegate(this, (TDelegate)FadeInChange);
	FadeInBar->ThumbPositionDelegate	= FDelegate(this, (TDelegate)FadeInChange);
	fadeOutBar->ThumbTrackDelegate		= FDelegate(this, (TDelegate)FadeOutChange);
	fadeOutBar->ThumbPositionDelegate	= FDelegate(this, (TDelegate)FadeOutChange);
	LockSlidersCheck->ClickDelegate = FDelegate(this, (TDelegate)LockSlidersClick);
}



void WFadingEdit::FadeInChange()
{
	INT inPos = FadeInBar->GetPos();
	INT outPos = fadeOutBar->GetPos();	

	if(LockSliders)
	{
		fadeOutBar->SetPos(inPos + SliderDiff);
	}
	else if(inPos > outPos)
	{
		fadeOutBar->SetPos(inPos);
	}

	UpdateTarget();
}



void WFadingEdit::FadeOutChange()
{
	INT inPos = FadeInBar->GetPos();
	INT outPos = fadeOutBar->GetPos();	

	if(LockSliders)
	{
		FadeInBar->SetPos(outPos - SliderDiff);
	}
	else if(outPos < inPos)
	{
		FadeInBar->SetPos(outPos);
	}

	UpdateTarget();
}


void WFadingEdit::LockSlidersClick()
{
	LockSliders = LockSlidersCheck->IsChecked();
	SliderDiff = fadeOutBar->GetPos() - FadeInBar->GetPos();	
}



void WFadingEdit::GetValuesFromTarget()
{
	CallUpdateTarget = FALSE;
	FadeInBar->SetPos((INT) (EditTarget->FadeInEndTime / EditTarget->LifetimeRange.Max * 100.0));
	fadeOutBar->SetPos((INT) (EditTarget->FadeOutStartTime / EditTarget->LifetimeRange.Max * 100.0));

	//old way write percent
	//FadeInEdit->SetText( *FString::Printf(TEXT("%d%%"),FadeInBar->GetPos()) );
	//FadeOutEdit->SetText( *FString::Printf(TEXT("%d%%"),fadeOutBar->GetPos()) );

	FadeInEdit->SetText( *FString::Printf(TEXT("%.2fs"),EditTarget->FadeInEndTime) );
	FadeOutEdit->SetText( *FString::Printf(TEXT("%.2fs"),EditTarget->FadeOutStartTime) );
	CallUpdateTarget = TRUE;
}



void WFadingEdit::UpdateTargetUtil()
{
	INT inPos = FadeInBar->GetPos();
	INT outPos = fadeOutBar->GetPos();
	FLOAT inTime = FLOAT(inPos) / 100.0 * EditTarget->LifetimeRange.Max;
	FLOAT outTime = FLOAT(outPos) / 100.0 * EditTarget->LifetimeRange.Max;
	
	FadeInEdit->SetText( *FString::Printf(TEXT("%.2fs"),inTime) );
	FadeOutEdit->SetText( *FString::Printf(TEXT("%.2fs%"),outTime) );

	EditTarget->FadeInEndTime = inTime;
	EditTarget->FadeOutStartTime = outTime;

	EditTarget->FadeIn = (inPos > 0);
	EditTarget->FadeOut = (outPos < 100);	
}




//=============================================================================
// WScaleEmitterEdit 

// Constructor.
WScaleEmitterEdit::WScaleEmitterEdit( class WParticleEditorComponent* OwnerComponent, DWORD ToolFlags) :
	WParticleEditorTool( OwnerComponent, FString::Printf(TEXT("Scale Size")), ToolFlags | PTOOL_ExtraIndent, 2000 ) 

{
}



void WScaleEmitterEdit::OnDestroy()
{
	if(ApplyButton)	{ delete(ApplyButton);	ApplyButton = NULL; }
	if(ScaleEdit)	{ delete(ScaleEdit);	ScaleEdit = NULL; }
	if(ScaleSlide)	{ delete(ScaleSlide);	ScaleSlide = NULL; }
}



FPoint WScaleEmitterEdit::GetSize() const
{
	return FPoint(300, 25);
}



void WScaleEmitterEdit::OpenWindow(HMODULE InHMOD )
{
	WPropertyPage::OpenWindow( IDPS_SCALE, InHMOD );

	ApplyButton = new WButton( this, IDPS_SCALE_APPLY );
	ApplyButton->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	ScaleEdit = new WEdit( this, IDPS_SCALE_EDIT );
	ScaleEdit->OpenWindow( TRUE, 0, 0 );

	ScaleSlide = new WButtonSlider(this, ScaleEdit, IDPS_SCALE_SLIDE );
	ScaleSlide->OpenWindow();

	PlaceControl(ApplyButton);
	PlaceControl(ScaleEdit);
	PlaceControl(ScaleSlide);
	Finalize();

	ApplyButton->ClickDelegate = FDelegate(this, (TDelegate)OnApply);	
	ScaleEdit->ChangeDelegate = FDelegate(this, (TDelegate)UpdateTarget);

}



void WScaleEmitterEdit::OnApply()
{
	FLOAT scaleVal = appAtof( *ScaleEdit->GetText() );
	if(scaleVal <= 0.0f)
		scaleVal = 0.001f;

	EditTarget->StartSizeRange.X.Min = EditTarget->StartSizeRange.X.Min	* scaleVal;
	EditTarget->StartSizeRange.Y.Min = EditTarget->StartSizeRange.Y.Min	* scaleVal;
	EditTarget->StartSizeRange.Z.Min = EditTarget->StartSizeRange.Z.Min	* scaleVal;
	EditTarget->StartSizeRange.X.Max = EditTarget->StartSizeRange.X.Max	* scaleVal;
	EditTarget->StartSizeRange.Y.Max = EditTarget->StartSizeRange.Y.Max	* scaleVal;
	EditTarget->StartSizeRange.Z.Max = EditTarget->StartSizeRange.Z.Max	* scaleVal;

	EditTarget->StartLocationRange.X.Min = EditTarget->StartLocationRange.X.Min * scaleVal;
	EditTarget->StartLocationRange.Y.Min = EditTarget->StartLocationRange.Y.Min * scaleVal;
	EditTarget->StartLocationRange.Z.Min = EditTarget->StartLocationRange.Z.Min * scaleVal;
	EditTarget->StartLocationRange.X.Max = EditTarget->StartLocationRange.X.Max * scaleVal;
	EditTarget->StartLocationRange.Y.Max = EditTarget->StartLocationRange.Y.Max * scaleVal;
	EditTarget->StartLocationRange.Z.Max = EditTarget->StartLocationRange.Z.Max * scaleVal;

	EditTarget->StartVelocityRange.X.Min = EditTarget->StartVelocityRange.X.Min * scaleVal;
	EditTarget->StartVelocityRange.Y.Min = EditTarget->StartVelocityRange.Y.Min * scaleVal;
	EditTarget->StartVelocityRange.Z.Min = EditTarget->StartVelocityRange.Z.Min * scaleVal;
	EditTarget->StartVelocityRange.X.Max = EditTarget->StartVelocityRange.X.Max * scaleVal;
	EditTarget->StartVelocityRange.Y.Max = EditTarget->StartVelocityRange.Y.Max * scaleVal;
	EditTarget->StartVelocityRange.Z.Max = EditTarget->StartVelocityRange.Z.Max * scaleVal;

	EditTarget->Acceleration.X = EditTarget->Acceleration.X * scaleVal;
	EditTarget->Acceleration.Y = EditTarget->Acceleration.Y * scaleVal;
	EditTarget->Acceleration.Z = EditTarget->Acceleration.Z * scaleVal;

	UClass* EmitterClass = EditTarget->GetClass();
	if( EmitterClass->IsChildOf(UBeamEmitter::StaticClass()) )
	{
		UBeamEmitter *beamTarget = (UBeamEmitter*) EditTarget;

		beamTarget->LowFrequencyNoiseRange.X.Min = beamTarget->LowFrequencyNoiseRange.X.Min * scaleVal;
		beamTarget->LowFrequencyNoiseRange.Y.Min = beamTarget->LowFrequencyNoiseRange.Y.Min * scaleVal;
		beamTarget->LowFrequencyNoiseRange.Z.Min = beamTarget->LowFrequencyNoiseRange.Z.Min * scaleVal;
		beamTarget->LowFrequencyNoiseRange.X.Max = beamTarget->LowFrequencyNoiseRange.X.Max * scaleVal;
		beamTarget->LowFrequencyNoiseRange.Y.Max = beamTarget->LowFrequencyNoiseRange.Y.Max * scaleVal;
		beamTarget->LowFrequencyNoiseRange.Z.Max = beamTarget->LowFrequencyNoiseRange.Z.Max * scaleVal;
		
		beamTarget->HighFrequencyNoiseRange.X.Min = beamTarget->HighFrequencyNoiseRange.X.Min * scaleVal;
		beamTarget->HighFrequencyNoiseRange.Y.Min = beamTarget->HighFrequencyNoiseRange.Y.Min * scaleVal;
		beamTarget->HighFrequencyNoiseRange.Z.Min = beamTarget->HighFrequencyNoiseRange.Z.Min * scaleVal;
		beamTarget->HighFrequencyNoiseRange.X.Max = beamTarget->HighFrequencyNoiseRange.X.Max * scaleVal;
		beamTarget->HighFrequencyNoiseRange.Y.Max = beamTarget->HighFrequencyNoiseRange.Y.Max * scaleVal;
		beamTarget->HighFrequencyNoiseRange.Z.Max = beamTarget->HighFrequencyNoiseRange.Z.Max * scaleVal;
	}

	EditTarget->PostEditChange();
	OwnerComponent->GetParentTab()->RereadComponentValues(TRUE);
}



void WScaleEmitterEdit::GetValuesFromTarget()
{
	ScaleEdit->SetText( TEXT("1.00") );
}



void WScaleEmitterEdit::UpdateTargetUtil()
{
	if( appAtof(*ScaleEdit->GetText()) < 0.0)
	{
		ScaleEdit->SetText( TEXT("0.0") );
	}
}




//=============================================================================
// WSpeedScaleEdit 

// Constructor.
WSpeedScaleEdit::WSpeedScaleEdit( class WParticleEditorComponent* OwnerComponent, DWORD ToolFlags) :
	WParticleEditorTool( OwnerComponent, FString::Printf(TEXT("Scale Speed")), ToolFlags | PTOOL_ExtraIndent, 2000 ) 

{
}



void WSpeedScaleEdit::OnDestroy()
{
	if(ApplyButton)	{ delete(ApplyButton);	ApplyButton = NULL; }
	if(ScaleEdit)	{ delete(ScaleEdit);	ScaleEdit = NULL; }
	if(ScaleSlide)	{ delete(ScaleSlide);	ScaleSlide = NULL; }
}



FPoint WSpeedScaleEdit::GetSize() const
{
	return FPoint(300, 25);
}



void WSpeedScaleEdit::OpenWindow(HMODULE InHMOD )
{
	WPropertyPage::OpenWindow( IDPS_SCALE, InHMOD );

	ApplyButton = new WButton( this, IDPS_SCALE_APPLY );
	ApplyButton->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	ScaleEdit = new WEdit( this, IDPS_SCALE_EDIT );
	ScaleEdit->OpenWindow( TRUE, 0, 0 );

	ScaleSlide = new WButtonSlider(this, ScaleEdit, IDPS_SCALE_SLIDE );
	ScaleSlide->OpenWindow();

	PlaceControl(ApplyButton);
	PlaceControl(ScaleEdit);
	PlaceControl(ScaleSlide);
	Finalize();

	ApplyButton->ClickDelegate = FDelegate(this, (TDelegate)OnApply);	
	ScaleEdit->ChangeDelegate = FDelegate(this, (TDelegate)UpdateTarget);

}



void WSpeedScaleEdit::OnApply()
{
	FLOAT scaleVal = appAtof( *ScaleEdit->GetText() );
	if(scaleVal <= 0.0f)
		scaleVal = 0.001f;

	EditTarget->LifetimeRange.Min /= scaleVal;
	EditTarget->LifetimeRange.Max /= scaleVal;

	EditTarget->InitialParticlesPerSecond *= scaleVal;

	EditTarget->Acceleration.X *= scaleVal * scaleVal;
	EditTarget->Acceleration.Y *= scaleVal * scaleVal;
	EditTarget->Acceleration.Z *= scaleVal * scaleVal;

	EditTarget->FadeInEndTime /= scaleVal;
	EditTarget->FadeOutStartTime /= scaleVal;

	EditTarget->SpinsPerSecondRange.X.Min *= scaleVal;
	EditTarget->SpinsPerSecondRange.Y.Min *= scaleVal;
	EditTarget->SpinsPerSecondRange.Z.Min *= scaleVal;
	EditTarget->SpinsPerSecondRange.X.Max *= scaleVal;
	EditTarget->SpinsPerSecondRange.Y.Max *= scaleVal;
	EditTarget->SpinsPerSecondRange.Z.Max *= scaleVal;

	EditTarget->StartVelocityRange.X.Min *= scaleVal;
	EditTarget->StartVelocityRange.Y.Min *= scaleVal;
	EditTarget->StartVelocityRange.Z.Min *= scaleVal;
	EditTarget->StartVelocityRange.X.Max *= scaleVal;
	EditTarget->StartVelocityRange.Y.Max *= scaleVal;
	EditTarget->StartVelocityRange.Z.Max *= scaleVal;

	EditTarget->MinSquaredVelocity *= scaleVal;

	EditTarget->MaxAbsVelocity.X *= scaleVal;
	EditTarget->MaxAbsVelocity.X *= scaleVal;
	EditTarget->MaxAbsVelocity.Z *= scaleVal;

	EditTarget->VelocityLossRange.X.Min *= scaleVal;
	EditTarget->VelocityLossRange.Y.Min *= scaleVal;
	EditTarget->VelocityLossRange.Z.Min *= scaleVal;
	EditTarget->VelocityLossRange.X.Max *= scaleVal;
	EditTarget->VelocityLossRange.Y.Max *= scaleVal;
	EditTarget->VelocityLossRange.Z.Max *= scaleVal;

	UClass* EmitterClass = EditTarget->GetClass();
	if( EmitterClass->IsChildOf(USparkEmitter::StaticClass()) )
	{
		USparkEmitter *sparkTarget = (USparkEmitter*) EditTarget;

		sparkTarget->TimeBetweenSegmentsRange.Min /= scaleVal;
		sparkTarget->TimeBetweenSegmentsRange.Max /= scaleVal;
	}

	EditTarget->PostEditChange();
	OwnerComponent->GetParentTab()->RereadComponentValues(TRUE);
}



void WSpeedScaleEdit::GetValuesFromTarget()
{
	ScaleEdit->SetText( TEXT("1.00") );
}



void WSpeedScaleEdit::UpdateTargetUtil()
{
	if( appAtof(*ScaleEdit->GetText()) < 0.0)
	{
		ScaleEdit->SetText( TEXT("0.0") );
	}
}
		



//=============================================================================
// WParticleStructTool 

WParticleStructTool::WParticleStructTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, INT inLabelWidth, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool( OwnerComponent, ToolName, ToolFlags, InUDNHelpTopic),
	LabelWidth(inLabelWidth),
	SpaceBetweenTools(2)
{
}

	

WParticleStructTool::WParticleStructTool(WWindow *OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString ToolName, INT inLabelWidth, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleEditorTool(OwnerWindow, OwnerComponent, ToolName, ToolFlags, InUDNHelpTopic),
	LabelWidth(inLabelWidth),
	SpaceBetweenTools(2)
{
}



void WParticleStructTool::OnDestroy()
{
	for(INT i = 0; i < Tools.Num(); i++)
		delete( Tools(i) );
	Tools.Empty();
	
	for(INT i = 0; i < Labels.Num(); i++)
		delete( Labels(i) );
	Labels.Empty();
	
	FieldNames.Empty();
}



FPoint WParticleStructTool::GetSize() const
{
	FPoint Ret = FPoint(0,0);
	FPoint toolSize;
	for(INT i = 0; i < Tools.Num(); i++)
	{
		toolSize = Tools(i)->GetSize() + FPoint(LabelWidth, 0);
		if(toolSize.X > Ret.X)
			Ret.X = toolSize.X;
		Ret.Y += toolSize.Y + SpaceBetweenTools;
	}
	return Ret;
}



void WParticleStructTool::GetValuesFromTarget()
{
	for(INT i = 0; i < Tools.Num(); i++)
	{
		Tools(i)->GetValuesFromTarget();
	}
}



void WParticleStructTool::Enable(UBOOL Enabled)
{
	for(INT i = 0; i < Tools.Num(); i++)
	{
		Tools(i)->Enable(Enabled);
	}
	for(INT i = 0; i < Tools.Num(); i++)
	{
		EnableWindow( Labels(i)->hWnd, Enabled );
	}
}



void WParticleStructTool::AddStructTool(WParticleEditorTool *NewTool, FString FieldName)
{
	Tools.AddItem(NewTool);
	new(FieldNames)FString(FieldName);
}



void WParticleStructTool::OpenWindow( HMODULE InHMOD )
{
	WPropertyPage::OpenWindow( 0, InHMOD );

	INT curY = 0;
	FPoint toolSize;
	for(INT i = 0; i < Tools.Num(); i++)
	{
		Labels.AddItem( new WLabel(this, 0) );
		Labels(i)->OpenWindow(1, 0);
		::MoveWindow(Labels(i)->hWnd, 2, curY, LabelWidth, 15, TRUE );
		Labels(i)->SetText( *FieldNames(i) );

		Tools(i)->OpenWindow( InHMOD );
		toolSize = Tools(i)->GetSize();
		::MoveWindow(Tools(i)->hWnd, LabelWidth + 3, curY, toolSize.X, toolSize.Y, TRUE);

		curY += toolSize.Y  + SpaceBetweenTools;
	}
}


void WParticleStructTool::UpdateTargetUtil()
{
	for(INT i = 0; i < Tools.Num(); i++)
	{
		Tools(i)->UpdateTarget();
	}
}



//=============================================================================
// WParticleTextureArrayTool

WParticleTextureArrayTool::WParticleTextureArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleArrayTool<UMaterial*>(OwnerComponent, ToolName, ToolFlags, InUDNHelpTopic)
{
}



WParticleEditorTool* WParticleTextureArrayTool::CreateTool(UMaterial** DataToEdit)
{
	WTexturePickEdit *tmp = new WTexturePickEdit(this, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic );
	tmp->SetTarget(DataToEdit);
	return tmp;
}



//=============================================================================
// WParticleFloatArrayTool

WParticleFloatArrayTool::WParticleFloatArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleArrayTool<FLOAT>(OwnerComponent, ToolName, ToolFlags, InUDNHelpTopic)
{
}



WParticleEditorTool* WParticleFloatArrayTool::CreateTool(FLOAT* DataToEdit)
{
	WFloatEdit *tmp = new WFloatEdit(this, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic );
	tmp->SetTarget( DataToEdit );
	return tmp;
}



//=============================================================================
// WParticlePlaneArrayTool

WParticlePlaneArrayTool::WParticlePlaneArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleArrayTool<FPlane>(OwnerComponent, ToolName, ToolFlags, InUDNHelpTopic)
{
}



WParticleEditorTool* WParticlePlaneArrayTool::CreateTool(FPlane* DataToEdit)
{
	WPlaneEdit *tmp = new WPlaneEdit(this, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic );
	tmp->SetTarget( DataToEdit );
	return tmp;
}




//=============================================================================
// WParticleSizeScaleArrayTool

WParticleSizeScaleArrayTool::WParticleSizeScaleArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleArrayTool<FParticleTimeScale>(OwnerComponent, ToolName, ToolFlags, InUDNHelpTopic)
{
}



WParticleEditorTool* WParticleSizeScaleArrayTool::CreateTool(FParticleTimeScale* DataToEdit)
{
	FLOAT *relTime = &( DataToEdit->RelativeTime );
	FLOAT *relSize = &( DataToEdit->RelativeSize );


	WParticleStructTool *sizeStruct = new WParticleStructTool(this, OwnerComponent, GetToolName(), 75, PTOOL_None, UDNHelpTopic );

	WPercentEdit *relativeTime = new WPercentEdit(sizeStruct, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic );
	WFloatEdit *relativeSize = new WFloatEdit(sizeStruct, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic );
	relativeTime->SetTarget( relTime );
	relativeSize->SetTarget( relSize );
	sizeStruct->AddStructTool(relativeTime, FString(TEXT("Relative Time")) );
	sizeStruct->AddStructTool(relativeSize, FString(TEXT("Relative Size")));

	return sizeStruct;
}




//=============================================================================
// WParticleColorScaleArrayTool

//WParticleColorScaleArrayTool::WParticleColorScaleArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic) :
//	WParticleArrayTool<FParticleColorScale>(OwnerComponent, ToolName, ToolFlags, InUDNHelpTopic)
//{
//}
//
//
//
//WParticleEditorTool* WParticleColorScaleArrayTool::CreateTool(FParticleColorScale* DataToEdit)
//{
//	FLOAT *relTime = &( DataToEdit->RelativeTime );
//	FColor *color = &( DataToEdit->Color );
//
//	WParticleStructTool *colorStruct = new WParticleStructTool(this, OwnerComponent, GetToolName(), 100, PTOOL_None, UDNHelpTopic );
//	WPercentEdit *relativeTime = new WPercentEdit(colorStruct, OwnerComponent, GetToolName(), relTime, PTOOL_None, UDNHelpTopic );
//	WColorEdit *colorEdit = new WColorEdit(colorStruct, OwnerComponent, GetToolName(), color, PTOOL_None, UDNHelpTopic );
//	colorStruct->AddStructTool(relativeTime, FString(TEXT("Relative Time")) );
//	colorStruct->AddStructTool(colorEdit, FString(TEXT("Color")) );
//
//	return colorStruct;
//}




//=============================================================================
// WParticleBeamEndPointArrayTool

WParticleBeamEndPointArrayTool::WParticleBeamEndPointArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleArrayTool<FParticleBeamEndPoint>(OwnerComponent, ToolName, ToolFlags, InUDNHelpTopic)
{
}



WParticleEditorTool* WParticleBeamEndPointArrayTool::CreateTool(FParticleBeamEndPoint* DataToEdit)
{
	FName *actorTag			= &( DataToEdit->ActorTag );
	FRangeVector *offset	= &( DataToEdit->Offset );
	FLOAT *weight			= &( DataToEdit->Weight );


	WParticleStructTool *beamEndStruct = new WParticleStructTool(this, OwnerComponent, GetToolName(), 40, PTOOL_None, UDNHelpTopic );

	WNameEdit *actorTagEdit = new WNameEdit(beamEndStruct, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic);
	WRangeVectorEdit *offsetEdit = new WRangeVectorEdit(beamEndStruct, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic);
	WFloatEdit *weightEdit = new WFloatEdit(beamEndStruct, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic);
	weightEdit->SetMin(0.0);

	actorTagEdit->SetTarget(actorTag);
	offsetEdit->SetTarget(offset);
	weightEdit->SetTarget(weight);
	
	beamEndStruct->AddStructTool(actorTagEdit, FString(TEXT("Actor Tag")) );
	beamEndStruct->AddStructTool(offsetEdit, FString(TEXT("Offset")) );
	beamEndStruct->AddStructTool(weightEdit, FString(TEXT("Weight")) );

	return beamEndStruct;
}




//=============================================================================
// WParticleBeamScaleArrayTool

WParticleBeamScaleArrayTool::WParticleBeamScaleArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleArrayTool<FParticleBeamScale>(OwnerComponent, ToolName, ToolFlags, InUDNHelpTopic)
{
}



WParticleEditorTool* WParticleBeamScaleArrayTool::CreateTool(FParticleBeamScale* DataToEdit)
{
	FVector *frequencyScale	= &( DataToEdit->FrequencyScale );
	FLOAT *relativeLength	= &( DataToEdit->RelativeLength );

	WParticleStructTool *beamScaleStruct = new WParticleStructTool(this, OwnerComponent, GetToolName(), 80, PTOOL_None, UDNHelpTopic );
	WXYZVectorEdit *frequencyScaleEdit = new WXYZVectorEdit(beamScaleStruct, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic);
	WPercentEdit *relativeLengthEdit = new WPercentEdit(beamScaleStruct, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic );

	frequencyScaleEdit->SetTarget( frequencyScale );
	relativeLengthEdit->SetTarget( relativeLength );

	beamScaleStruct->AddStructTool(frequencyScaleEdit, FString(TEXT("Scale")) );
	beamScaleStruct->AddStructTool(relativeLengthEdit, FString(TEXT("Relative Length")) );

	return beamScaleStruct;
}


//=============================================================================
// WParticleSoundArrayTool

WParticleSoundArrayTool::WParticleSoundArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic) :
	WParticleArrayTool<FParticleSound>(OwnerComponent, ToolName, ToolFlags, InUDNHelpTopic)
{
}



WParticleEditorTool* WParticleSoundArrayTool::CreateTool(FParticleSound* DataToEdit)
{
	USound	**sound		= &( DataToEdit->Sound );
	FRange	*radius		= &( DataToEdit->Radius );
	FRange	*pitch		= &( DataToEdit->Pitch );
	INT		*weight		= &( DataToEdit->Weight );
	FRange	*volume		= &( DataToEdit->Volume );
	FRange	*probability= &( DataToEdit->Probability );

	WParticleStructTool *soundStruct = new WParticleStructTool(this, OwnerComponent, GetToolName(), 55, PTOOL_None, UDNHelpTopic );
	WSoundPickEdit *soundEdit = new WSoundPickEdit(soundStruct, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic);
	WIntEdit *weightEdit = new WIntEdit(soundStruct, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic);
	WRangeEdit *radiusEdit = new WRangeEdit(soundStruct, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic);
	WRangeEdit *pitchEdit = new WRangeEdit(soundStruct, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic);
	WRangeEdit *volumeEdit = new WRangeEdit(soundStruct, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic);
	volumeEdit->SetAbsMin(0.0);
	volumeEdit->SetAbsMax(1.0);
	WRangeEdit *probabilityEdit = new WRangeEdit(soundStruct, OwnerComponent, GetToolName(), PTOOL_None, UDNHelpTopic);
	probabilityEdit->SetAbsMin(0.0);
	probabilityEdit->SetAbsMax(1.0);

	soundEdit->SetTarget(sound);
	weightEdit->SetTarget(weight);
	radiusEdit->SetTarget(radius);
	pitchEdit->SetTarget(pitch);
	volumeEdit->SetTarget(volume);
	probabilityEdit->SetTarget(probability);
	

	soundStruct->AddStructTool(soundEdit, FString(TEXT("Sound")) );
	soundStruct->AddStructTool(weightEdit, FString(TEXT("Weight")) );
	soundStruct->AddStructTool(radiusEdit, FString(TEXT("Radius")) );
	soundStruct->AddStructTool(pitchEdit, FString(TEXT("Pitch")) );
	soundStruct->AddStructTool(volumeEdit, FString(TEXT("Volume")) );
	soundStruct->AddStructTool(probabilityEdit, FString(TEXT("Probability")) );

	return soundStruct;

}

//=============================================================================
// WNameTool


WNameTool::WNameTool(class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic)
	: WStringEdit(OwnerComponent, InStringName, ToolFlags, InUDNHelpTopic)
{

}



//Constructor
WNameTool::WNameTool(WWindow *OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic)
	:WStringEdit(OwnerWindow, OwnerComponent, InStringName, ToolFlags, InUDNHelpTopic)
{


}

void WNameTool::UpdateTargetUtil()
{
	
	WStringEdit::UpdateTargetUtil();
	TCITEM tci;

	WTabControl* PSETabControl = OwnerComponent->GetParentTab()->GetOwnerEditor()->EmitterTabs->Tabs; // Yowzah!
	INT TabIndex = OwnerComponent->GetParentTab()->GetEmitter()->Emitters.FindItemIndex(OwnerComponent->GetParentTab()->GetEditTarget());

	TCHAR* Text = new TCHAR[StringToEdit->Len() + 1];
	appStrcpy( Text, **StringToEdit );
	tci.pszText = Text;
	tci.mask = LVIF_TEXT;
	SendMessageX(*PSETabControl, TCM_SETITEM, TabIndex, (LPARAM)(const LPTCITEM)&tci );

	::InvalidateRect( OwnerComponent->GetParentTab()->OwnerWindow->hWnd, NULL, TRUE); 

	delete [] Text;
}

