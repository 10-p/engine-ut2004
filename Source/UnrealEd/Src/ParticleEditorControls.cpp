//=============================================================================
//
// ParticleEditorControls.cpp	
// Copyright(c) 2002 Demiurge Studios. All Rights Reserved.
// 
//=============================================================================

#include "UnrealEd.h"




//=============================================================================
// WButtonSlider

WButtonSlider::WButtonSlider( WWindow* InOwner, WEdit *InAssociatedEdit, INT InId, UBOOL InWholeNumbersOnly, FDelegate InClicked, WNDPROC InSuperProc ) :
	WButton(InOwner, InId, InClicked, InSuperProc),
	AssociatedEdit(InAssociatedEdit),
	WholeNumbersOnly(InWholeNumbersOnly),
	CurValue(0),
	IncAmount(1.0),
	Sliding(FALSE),
	NewClick(FALSE),
	HasMoved(FALSE),
	Location(FPoint(0,0))
{
}



void WButtonSlider::OnDestroy()
{
	DeleteObject( UpDownImage );
}



void WButtonSlider::OpenWindow()
{
	WButton::OpenWindow( TRUE, 0, 0, 0, 0, TEXT(""), TRUE );
	UpDownImage = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_UPDOWN), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS ); check(UpDownImage);
}



void WButtonSlider::OnMove(INT NewX, INT NewY)
{
	WButton::OnMove(NewX, NewY);
	SetBitmap(UpDownImage);
	ResizeWindow(12, 20, TRUE);
}



FLOAT WButtonSlider::GetCurrentValue()
{
	return CurValue;
}



void WButtonSlider::SetCurrentValue(FLOAT newValue)
{
	CurValue = newValue;

}



void WButtonSlider::OnLeftButtonDown()
{

	// If this is the first time we're running the editor
	INT trash = 0;
	if(!GConfig->GetInt(TEXT("ParticleEditor"), TEXT("Run"), trash, TEXT("UnrealEd.ini")))
	{
		GConfig->SetInt(TEXT("ParticleEditor"), TEXT("Run"), 1, TEXT("UnrealEd.ini"));
		appMsgf(0, TEXT("Click and DRAG these scroll buttons for an easy way to change the value!"));
	}
	else
	{

		Sliding = TRUE;
		NewClick = TRUE;

		CurValue = appAtof( *AssociatedEdit->GetText() );

		IncAmount = CurValue / 100;

		if(IncAmount < 0.0)
			IncAmount = -IncAmount;

		if(WholeNumbersOnly)
			IncAmount = (INT) IncAmount;

		if(IncAmount < 0.00001)
			IncAmount = 1.0;

		//click inc/dec -  click value is always 1 for whole number
		if(Location.Y < 10) //Clicked top half
			CurValue += WholeNumbersOnly ? 1 : IncAmount; 
		else
			CurValue -= WholeNumbersOnly ? 1 : IncAmount; 

		if(AssociatedEdit)
		{
			if(WholeNumbersOnly)
				AssociatedEdit->SetText(*FString::Printf(TEXT("%.0f"),CurValue) );
			else
				AssociatedEdit->SetText(*FString::Printf(TEXT("%.3f"),CurValue) );
		}
	}
}



void WButtonSlider::OnLeftButtonUp()
{
	Sliding = FALSE;
	HasMoved = FALSE;
}



void WButtonSlider::OnMouseMove(DWORD Flags, FPoint InLocation)
{
	Location = InLocation;
	if(Sliding)
	{
		if(NewClick)
		{
			LastLocation = Location;
			NewClick = FALSE;
		}
		//move mouse to change value
		else if(Location != LastLocation)
		{
			HasMoved = TRUE;

			CurValue += IncAmount * (LastLocation.Y - Location.Y);

			if(AssociatedEdit)
			{
				if(WholeNumbersOnly)
					AssociatedEdit->SetText(*FString::Printf(TEXT("%.0f"),CurValue) );
				else
					AssociatedEdit->SetText(*FString::Printf(TEXT("%.3f"),CurValue) );
			}

			LastLocation = Location;
		}
		//hold mouse on up or down arrow
		else if(!HasMoved)
		{
			if(Location.Y >= 0 && Location.Y < 10)
				CurValue += WholeNumbersOnly ? 1 : IncAmount; 
			else if (Location.Y > 10 && Location.Y < 20)
				CurValue -= WholeNumbersOnly ? 1 : IncAmount; 

			if(WholeNumbersOnly)
				AssociatedEdit->SetText(*FString::Printf(TEXT("%.0f"),CurValue) );
			else
				AssociatedEdit->SetText(*FString::Printf(TEXT("%.3f"),CurValue) );
		}
	}
}




//=============================================================================
// WColorScaleButton

WColorScaleButton::WColorScaleButton( WWindow* InOwner, WColorScaleEdit* inColorScaleEdit, INT inButtonIndex) :
	WColorButton(InOwner, 0),
	ColorScaleEdit(inColorScaleEdit),
	ButtonIndex(inButtonIndex),
	Selected(FALSE),
	alpha(255)
{
}



void WColorScaleButton::OpenWindow( UBOOL visible, INT left, INT top, INT width, INT height )
{
	WColorButton::OpenWindow( visible, 0 );
	::MoveWindow( hWnd, left, top, width, height, TRUE );
}



void WColorScaleButton::OnLeftButtonDown()
{
	ColorScaleEdit->ColorButtonDown(ButtonIndex);
}



void WColorScaleButton::OnLeftButtonUp()
{
	ColorScaleEdit->ColorButtonUp(ButtonIndex);
}



void WColorScaleButton::OnLeftButtonDoubleClick()
{
	ColorScaleEdit->ColorButtonDoubleClick(ButtonIndex);
}



void WColorScaleButton::OnRightButtonUp()
{
	ColorScaleEdit->ColorButtonRightClick(ButtonIndex);
}



void WColorScaleButton::OnMove(INT NewX, INT NewY)
{
	LastRelativeLocation = FPoint(NewX, NewY);
}



void WColorScaleButton::OnMouseMove(DWORD Flags, FPoint Location)
{
	/*
	RECT thisRect;
	::GetWindowRect( hWnd, &thisRect );
	RECT colorEditRect;
	::GetWindowRect( ColorScaleEdit->hWnd, &colorEditRect );
	FPoint relButtonLoc = FPoint( thisRect.left-colorEditRect.left, thisRect.top-colorEditRect.top );
	*/

	ColorScaleEdit->ColorButtonMove(LastRelativeLocation + Location);
}



FPoint WColorScaleButton::GetLocation()
{
	return LastRelativeLocation;
}



void WColorScaleButton::SetIndex(INT NewIndex)
{
	ButtonIndex = NewIndex;
}



void WColorScaleButton::SetSelected(UBOOL inSelected) 
{
	Selected = inSelected;	
}


void WColorScaleButton::SetColor( INT InR, INT InG, INT InB, INT InA )
{
	WColorButton::SetColor(InR, InG, InB);
	alpha = InA;
}


void WColorScaleButton::GetColor( INT& InR, INT& InG, INT& InB, INT& InA )
{
	WColorButton::GetColor(InR, InG, InB);
	InA = alpha;
}

	
void WColorScaleButton::OnDrawItem( DRAWITEMSTRUCT* Item )
{
	guard(WColorScaleButton::OnDrawItem);

	RECT rect = Item->rcItem;
	UBOOL bPressed = (Item->itemState&ODS_SELECTED)!=0;

	HBRUSH ColorBrush = CreateSolidBrush( RGB(R,G,B) ), OldBrush;
	FillRect( Item->hDC, &Item->rcItem, hBrushGrey );

	DrawEdge( Item->hDC, &rect, (Selected) ? EDGE_SUNKEN : EDGE_RAISED, BF_RECT );

	OldBrush = (HBRUSH)SelectObject( Item->hDC, ColorBrush );
	INT Adjust = (bPressed) ? 2 : 0;
	rect.left += 2 + Adjust;
	rect.top += 2 + Adjust;
	rect.right -= 2 + Adjust;
	rect.bottom -= 2 + Adjust;

	Rectangle( Item->hDC, rect.left, rect.top, rect.right, rect.bottom );

	// Clean up.
	//
	SelectObject( Item->hDC, OldBrush );
	DeleteObject( ColorBrush );

	unguard;
}




