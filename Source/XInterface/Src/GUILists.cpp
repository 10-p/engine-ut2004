/*=============================================================================
	GUILists.cpp: GUI List Components
	Copyright 2003, Epic Games, Inc. All Rights Reserved.

Revision history:
	* Moved from UnGUI.cpp by Ron Prestenback
	* BDB Added UMultiSelectList::DrawItem 
=============================================================================*/

#include "XInterface.h"

IMPLEMENT_CLASS(UGUIListBoxBase);
IMPLEMENT_CLASS(UGUIListBox);
IMPLEMENT_CLASS(UGUIScrollTextBox);
IMPLEMENT_CLASS(UGUIMultiColumnListBox);
IMPLEMENT_CLASS(UGUITreeListBox);
IMPLEMENT_CLASS(UGUIListBase);
IMPLEMENT_CLASS(UGUIVertList);
IMPLEMENT_CLASS(UGUIList);
IMPLEMENT_CLASS(UGUIScrollText);
IMPLEMENT_CLASS(UGUIMultiOptionList);
IMPLEMENT_CLASS(UGUITreeList);
IMPLEMENT_CLASS(UGUIMultiColumnList);
IMPLEMENT_CLASS(UGUIHorzList);
IMPLEMENT_CLASS(UGUICircularList);
IMPLEMENT_CLASS(UGUICharacterList);
IMPLEMENT_CLASS(UGUIMultiColumnListHeader);
IMPLEMENT_CLASS(UMultiSelectList);
IMPLEMENT_CLASS(UGUICircularImageList);
IMPLEMENT_CLASS(UGUIVertImageList);

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIListBoxBase
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIListBoxBase::PreDraw(UCanvas* Canvas)
{
	guard(UGUIListBoxBase::PreDraw);

	if ( INVALIDRENDER )
		return;

	UGUIComponent::PreDraw(Canvas);

	if ( !MyList || !MyScrollBar )
		return;

	UGUIListBase* List = CastChecked<UGUIListBase>(MyList);
	if (bVisible)
	{
		if (!bVisibleWhenEmpty)
		{
			if (List->bVisible != (List->ItemCount > 0) )
				List->eventSetVisibility(List->ItemCount > 0);
		}

		if ( bTravelling || bSizing )
			MyScrollBar->bVisible = 0;
		
		else if ( MyScrollBar->bVisible != (MyList->ItemsPerPage > 0 && MyList->ItemCount > MyList->ItemsPerPage))
			MyScrollBar->eventSetVisibility(MyList->ItemsPerPage > 0 && MyList->ItemCount > MyList->ItemsPerPage);
	}


	MyList->SetAdjustedDims( Max<FLOAT>(ActualWidth() - (MyScrollBar->bVisible ? MyScrollBar->ActualWidth() : 0), 2.f), ActualHeight(), ActualLeft(), ActualTop() );
	MyScrollBar->SetAdjustedDims( MyScrollBar->ActualWidth(), ActualHeight(), ActualLeft() + MyList->ActualWidth(), ActualTop() );

	PreDrawControls(Canvas);	
	unguardobj;
}

void UGUIListBoxBase::Draw(UCanvas* Canvas)
{
	guard(UGUListBox::Draw);

	Super::Draw(Canvas);

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIVertList
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIVertList::PreDraw(UCanvas* Canvas)
{
	guard(UGUIVertList::PreDraw);
	if ( INVALIDRENDER )
		return;

	Super::PreDraw(Canvas);

	Canvas->Font = Style->Fonts[MenuState+(5*FontScale)]->eventGetFont(Canvas->SizeX);
	if (!Canvas->Font)
		return;

	FLOAT XL, YL;

	if ( DELEGATE_IS_SET(GetItemHeight) )
		YL = delegateGetItemHeight(Canvas);
	else
		Canvas->ClippedStrLen( Canvas->Font, 1, 1, XL, YL, TEXT("WQ,2") );

	ItemHeight = YL;
	ItemsPerPage = appFloor( (ClientBounds[3] - ClientBounds[1]) / ItemHeight );

	unguardobj;
}

void UGUIVertList::Draw(UCanvas* Canvas)
{
	guard(UGUIVertList::Draw);

	Super::Draw(Canvas);

	if ( !bVisible || (ItemCount == 0 && !bVisibleWhenEmpty) || INVALIDRENDER )
		return;

	// Always assign a temp UGUIController pointer in case script sets Controller to NULL
	UGUIController* C = Controller;

	Style->Draw( Canvas, MenuState, Bounds[0], Bounds[1], Bounds[2] - Bounds[0], Bounds[3] - Bounds[1]);
	INT X = ClientBounds[0], Y = ClientBounds[1], XL = ClientBounds[2] - ClientBounds[0];

	if ( bHotTrack && C->HasMouseMoved() && C->ActiveControl == this && C->ActivePage == PageOwner )
	{
		if ( PerformHitTest(C->MouseX, C->MouseY) )
		{
			INT OldIndex = Index;
			Index = Min( Top + appFloor( (C->MouseY - ClientBounds[1]) / ItemHeight), ItemCount - 1 );

			if (OldIndex != Index)
			{
				if ( bHotTrackSound )
					C->PlayComponentSound(CS_Hover);

				if ( DELEGATE_IS_SET(OnTrack) )
					delegateOnTrack(this, OldIndex);
			}
		}
		else Index = INDEX_NONE;
	}

	for (INT i = Top; i < Top + ItemsPerPage; i++)
	{
		if ( i >= ItemCount )
			break;

		// Check if this is the selected Index

		UBOOL bIsPending = SelectedItems.FindItemIndex(i) != INDEX_NONE;

		UBOOL bIsSelected = i == Index;
		UBOOL bIsDrop = i == DropIndex;
		bIsPending = ((bDropSource || bDropTarget) && (bIsPending || bIsDrop));

		if( DELEGATE_IS_SET(OnDrawItem) )
			delegateOnDrawItem( Canvas, i, X, Y, XL, ItemHeight, bIsSelected, bIsPending );
		else
		{
			if (bIsSelected || (bIsPending && !bIsDrop)) 
			{
				if (SelectedStyle!=NULL)
				{
					if (SelectedStyle->Images[MenuState]!=NULL)
						SelectedStyle->Draw(Canvas,MenuState,X,Y,XL,ItemHeight);
					else
						Canvas->DrawTile(C->DefaultPens[0], X, Y, XL,ItemHeight,0,0,32,32, 0.0f, SelectedStyle->FontBKColors[MenuState],FPlane(0,0,0,0));
				}
				else
				{
					// Display the selection

					if ( (MenuState==MSAT_Focused)  || (MenuState==MSAT_Pressed) )
					{
						if (SelectedImage==NULL)
							Canvas->DrawTile(C->DefaultPens[0], X, Y, XL,ItemHeight,0,0,32,32, 0.0f, SelectedBKColor,FPlane(0,0,0,0));
						else
						{
							Canvas->Color = SelectedBKColor;
							Canvas->DrawTileStretched(SelectedImage, X, Y, XL, ItemHeight);
						}
					}
				}
			}

			if (bIsPending && OutlineStyle)
			{
				if (OutlineStyle->Images[MenuState])
				{
					if ( bIsDrop )
						OutlineStyle->Draw( Canvas, MenuState, X + 1, Y + 1, XL - 2, ItemHeight - 2 );
					else
					{
						OutlineStyle->Draw(Canvas, MenuState, X, Y, XL, ItemHeight);
						if (DropState == DRP_Source)
							OutlineStyle->Draw(Canvas, MenuState, C->MouseX - MouseOffset[0], C->MouseY - MouseOffset[1] + Y - ClientBounds[1], MouseOffset[2] + MouseOffset[0], ItemHeight);
					}
				}
			}

			DrawItem( Canvas, i, X, Y, XL, ItemHeight, bIsSelected, bIsPending );
		}

		Y+=ItemHeight;
	}

	unguardobj;
}


// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIHorzList
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIHorzList::Draw(UCanvas* Canvas)
{
	guard(UGUIHorzList::Draw);

	Super::Draw(Canvas);

	if ( !bVisible || (ItemCount == 0 && !bVisibleWhenEmpty) || INVALIDRENDER )
		return;

	// Always assign a temp UGUIController pointer in case script sets Controller to NULL
	UGUIController* C = Controller;
	Style->Draw( Canvas, MenuState, Bounds[0], Bounds[1], Bounds[2] - Bounds[0], Bounds[3] - Bounds[1]);

	ItemsPerPage = appFloor( (ClientBounds[2] - ClientBounds[0]) / ItemWidth );

	INT X = ClientBounds[0], Y = ClientBounds[1], XL = ItemWidth, YL = ClientBounds[3] - ClientBounds[1];
	BYTE TempMenuState;

	FLOAT SelIndex = Index;

	if ( C->ActiveControl == this && bHotTrack && C->ActivePage == PageOwner && C->HasMouseMoved() )
	{
		if ( PerformHitTest(C->MouseX, C->MouseY) )
		{
			INT OldIndex = SelIndex;
			SelIndex = Min(Top + appFloor( (C->MouseX - ClientBounds[0]) / ItemWidth), ItemCount - 1);
			if (OldIndex != SelIndex )
			{
				if ( bHotTrackSound )
					C->PlayComponentSound(CS_Hover);
	
				if ( DELEGATE_IS_SET(OnTrack) )
					delegateOnTrack(this, OldIndex);
			}
		}

		else SelIndex = INDEX_NONE;
	}

	if (SelectedStyle!=NULL)
	{
		SelectedImage = SelectedStyle->Images[MenuState];
		SelectedBKColor = SelectedStyle->FontColors[MenuState];
	}

	for (INT i = Top; i < Top + ItemsPerPage;i++)
	{		
		TempMenuState = MenuState;

		if ( i >= ItemCount )
			break;

		// Check if this is the selected image
		UBOOL bIsPending = SelectedItems.FindItemIndex(i) != INDEX_NONE;
		UBOOL bIsSelected = i==SelIndex;
		UBOOL bIsDrop = i == DropIndex;
		bIsPending = ((bDropSource || bDropTarget) && (bIsPending || bIsDrop));


		if( DELEGATE_IS_SET(OnDrawItem) )
			delegateOnDrawItem( Canvas, i, X, Y, XL, YL, bIsSelected, bIsPending );
		else
		{
			if (bIsSelected) 
			{
				if (MenuState==MSAT_Focused) 
				{
					if (SelectedImage==NULL)
						Canvas->DrawTile(C->DefaultPens[0], X, Y, XL,  YL,0,0,32,32, 0.0f, SelectedBKColor,FPlane(0,0,0,0));
					else
						Canvas->DrawTileStretched(SelectedImage,X, Y, XL, YL);
				}

				MenuState=MSAT_Watched;
			}

			if (bIsPending && OutlineStyle)
			{
				if (OutlineStyle->Images[MenuState])
				{
					if ( bIsDrop )
						OutlineStyle->Draw(Canvas,MenuState, X + 1, Y + 1, XL - 2, YL - 2);
					else
					{
						OutlineStyle->Draw(Canvas, MenuState, X, Y, XL, YL);
						if (DropState == DRP_Source && !bIsDrop)
							OutlineStyle->Draw(Canvas, MenuState, C->MouseX - MouseOffset[0] + X - ClientBounds[0], C->MouseY - MouseOffset[1], ItemWidth, MouseOffset[3] - MouseOffset[1]);
					}
				}
			}

			DrawItem( Canvas, i, X, Y, XL, YL, bIsSelected, bIsPending);
		}
		MenuState=TempMenuState;
		X+=ItemWidth;
	}

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUICircularList
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUICircularList::Draw(UCanvas* Canvas)
{
	guard(UGUICircularList::Draw);

	Super::Draw(Canvas);

	if ( !bVisible || (ItemCount == 0 && !bVisibleWhenEmpty) || INVALIDRENDER )
		return;

	// Always assign a temp UGUIController pointer in case script sets Controller to NULL
	UGUIController* C = Controller;

	Style->Draw( Canvas, MenuState, Bounds[0], Bounds[1], Bounds[2] - Bounds[0], Bounds[3] - Bounds[1]);

	if (FixedItemsPerPage>0)
		ItemsPerPage = FixedItemsPerPage;
	else
		ItemsPerPage = appFloor( (ClientBounds[2] - ClientBounds[0]) / ItemWidth );

	if ( !bWrapItems && ItemsPerPage > ItemCount )
		ItemsPerPage = ItemCount;

	INT X = ClientBounds[0], Y = ClientBounds[1], XL = ItemWidth, YL = ClientBounds[3] - ClientBounds[1];
	INT Width = ClientBounds[2] - ClientBounds[0];

	if ( (bCenterInBounds) && (ItemsPerPage*ItemWidth<Width) )
		X += (Width - (ItemsPerPage*ItemWidth))/2;

	FLOAT xMod=0.0f;
	if (bFillBounds)
		xMod = (Width - (ItemsPerPage*ItemWidth)) / ItemsPerPage;

	BYTE TempMenuState;

	FLOAT SelIndex = Index;
	if ( C->ActiveControl == this && bHotTrack && C->ActivePage == PageOwner && C->HasMouseMoved() )
	{
		if ( PerformHitTest(C->MouseX, C->MouseY) )
		{
			INT OldIndex = SelIndex;
			SelIndex = (Top + appFloor( (C->MouseX - ClientBounds[0]) / ItemWidth)) % ItemCount;
			if (OldIndex != SelIndex )
			{
				if ( bHotTrackSound )
					C->PlayComponentSound(CS_Hover);

				if ( DELEGATE_IS_SET(OnTrack) )
					delegateOnTrack(this,OldIndex);
			}
		}

		else SelIndex = INDEX_NONE;
	}

	for (INT i=Top;i<Top+ItemsPerPage;i++)
	{
		if ( !bWrapItems && i >= ItemCount )
			break;

		TempMenuState = MenuState;
		INT DrawIndex = i % ItemCount;

		UBOOL bIsSelected = DrawIndex==SelIndex;
		UBOOL bIsPending = SelectedItems.FindItemIndex(DrawIndex) != INDEX_NONE;

		UBOOL bIsDrop = DrawIndex == DropIndex;
		bIsPending = ((bDropTarget || bDropSource) && (bIsPending || bIsDrop));

		if( DELEGATE_IS_SET(OnDrawItem) )
			delegateOnDrawItem( Canvas, DrawIndex, X, Y, XL, YL, bIsSelected, bIsPending );
		else
		{
			if (bIsSelected)
			{
				if (MenuState==MSAT_Focused) 
				{
					if (SelectedImage==NULL)
						Canvas->DrawTile(C->DefaultPens[0], X, Y, XL, YL,0,0,32,32, 0.0f, SelectedBKColor,FPlane(0,0,0,0));
					else
						Canvas->DrawTileStretched(SelectedImage,X, Y, XL, YL);
				}
				MenuState=MSAT_Watched;
			}

			DrawItem( Canvas, DrawIndex, X, ClientBounds[1], ItemWidth, ClientBounds[3]-ClientBounds[1], bIsSelected, bIsPending);
		}

		MenuState=TempMenuState;

		if (!bFillBounds)
			X+=ItemWidth;
		else
			X+=ItemWidth+xMod;
	}

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUICharacterList
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUICharacterList::PreDraw(UCanvas* Canvas)
{
	guard(UGUICharacterList::PreDraw);

	Super::PreDraw(Canvas);

	ItemHeight = Max( ActualHeight(), 2.f );
	ItemWidth = ItemHeight / 2;
	ItemsPerPage = appFloor( (ClientBounds[2] - ClientBounds[0]) / ItemWidth );

	if ( !bAnimating && MyScrollBar && MyScrollBar->bVisible != ItemsPerPage > 0 && ItemCount > ItemsPerPage )
	{
		MyScrollBar->eventSetVisibility(ItemsPerPage > 0 && ItemCount > ItemsPerPage);
		SetAdjustedDims( Max<FLOAT>(ActualWidth() - (MyScrollBar->bVisible ? MyScrollBar->ActualWidth() : 0), 2.f), ActualHeight(), ActualLeft(), ActualTop() );
		UpdateBounds();
	}

	unguardobj;
}

void UGUICharacterList::DrawItem(UCanvas* Canvas, INT Item, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, UBOOL bSelected, UBOOL bPending)
{
	guard(UGUICharacterList::DrawItem);

	if ( INVALIDRENDER )
		return;

	UMaterial* Mat = PlayerList(Item).Portrait;
	if (!Mat)
		Mat = DefaultPortrait;

	FColor Color;
	if (Item == Index)
	{
		Color = FColor(255,255,255,255);
		Canvas->DrawTile(Controller->DefaultPens[0], X + (XL/2) - (ItemWidth/2), Y + (YL/2) - (ItemHeight/2), ItemWidth, ItemHeight, 0, 0, 256, 512, 0, FColor(255,255,0,255), FPlane(0,0,0,0));
	}
	else
		Color = FColor(128,128,128,255);

	Canvas->DrawTile(Mat, X + (XL/2) - (ItemWidth/2)+1, Y + (YL/2)+1 - (ItemHeight/2), ItemWidth-2, ItemHeight-2, 0, 0, 256, 512, 0, Color, FPlane(0,0,0,0));
	if (bPending && OutlineStyle)
	{
		FLOAT NewX = X + (XL / 2) - (ItemWidth / 2);
		FLOAT NewY = Y + (YL / 2) - (ItemHeight / 2);
		if (OutlineStyle->Images[MenuState])
		{
			OutlineStyle->Draw(Canvas, 3, NewX + 1, NewY + 1, ItemWidth - 2, ItemHeight - 2);
			if (DropState == DRP_Source && DropIndex != Item)
				OutlineStyle->Draw(Canvas, MenuState, Controller->MouseX - MouseOffset[0] + NewX - ClientBounds[0], Controller->MouseY - MouseOffset[1] + NewY - ClientBounds[1], ItemWidth - 2, ItemHeight - 2);
		}
	}

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUICircularImageList
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUICircularImageList::PreDraw(UCanvas* Canvas)
{
	guard(UGUICircularImageList::PreDraw);

	Super::PreDraw(Canvas);

	ItemHeight = Max( ActualHeight(), 2.f );
	ItemWidth = ItemHeight;
	ItemsPerPage = appFloor( (ClientBounds[2] - ClientBounds[0]) / ItemWidth );

	if ( !bAnimating && MyScrollBar && MyScrollBar->bVisible != ItemsPerPage > 0 && ItemCount > ItemsPerPage )
	{
		MyScrollBar->eventSetVisibility(ItemsPerPage > 0 && ItemCount > ItemsPerPage);
		SetAdjustedDims( Max<FLOAT>(ActualWidth() - (MyScrollBar->bVisible ? MyScrollBar->ActualWidth() : 0), 2.f), ActualHeight(), ActualLeft(), ActualTop() );
		UpdateBounds();
	}

	unguardobj;
}

void UGUICircularImageList::DrawItem(UCanvas* Canvas, INT Item, FLOAT X, FLOAT Y, FLOAT W, FLOAT HT, UBOOL bSelected, UBOOL bPending)
{
	guard(UGUICircularImageList::DrawItem);

	if ( INVALIDRENDER )
		return;

	UMaterial* Mat = Cast<UMaterial>(Elements(Item).ExtraData);
	if (!Mat)
		Mat = Controller->DefaultPens[0];		// TODO Display fallback texture if no material

	FColor Color;
	if (bSelected)
	{
		Color = FColor(255,255,255,255);
		Canvas->Color = SelectedBKColor;
		Canvas->DrawTileScaleBound(Controller->DefaultPens[0], X + (W/2) - (ItemWidth/2), Y + (HT/2) - (ItemHeight/2), ItemWidth, ItemHeight);
	}
	else
		Color = FColor(128,128,128,255);

	FLOAT Ratio = Mat->MaterialVSize() / Mat->MaterialUSize();
	Canvas->DrawTile(Mat, X + (W/2) - ((ItemWidth * Ratio)/2)+1, Y + (HT/2)+1 - (ItemHeight/2), (ItemWidth*Ratio)-2, ItemHeight-2, 0, 0, Mat->MaterialUSize(), Mat->MaterialVSize(), 0, Color, FPlane(0,0,0,0));
	if (bPending && OutlineStyle)
	{
		FLOAT NewX = X + (W / 2) - (ItemWidth / 2);
		FLOAT NewY = Y + (HT / 2) - (ItemHeight / 2);
		if (OutlineStyle->Images[MenuState])
		{
			OutlineStyle->Draw(Canvas, 3, NewX + 1, NewY + 1, ItemWidth - 2, ItemHeight - 2);
			if (DropState == DRP_Source && DropIndex != Item)
				OutlineStyle->Draw(Canvas, MenuState, Controller->MouseX - MouseOffset[0] + NewX - ClientBounds[0], Controller->MouseY - MouseOffset[1] + NewY - ClientBounds[1], ItemWidth - 2, ItemHeight - 2);
		}
	}


	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIVertImageList
// =======================================================================================================================================================
// =======================================================================================================================================================
void UGUIVertImageList::PreDraw(UCanvas* Canvas)
{
	guard(UGUIVertImageList::PreDraw);

	if ( INVALIDRENDER )
		return;

	UGUIListBase::PreDraw(Canvas);

	// Make sure we have at least 1 visible row & column
	NoVisibleRows = Max(NoVisibleRows, 1);
	NoVisibleCols = Max(NoVisibleCols, 1);

	if (CellStyle==CELL_FixedSize)
	{
		ItemWidth = ItemHeight = 0.f;
		for (INT i=0;i<Elements.Num();i++)
		{
			if ( !Elements(i).Image )
				continue;

			FLOAT mU = Elements(i).Image->MaterialUSize() * ImageScale, mV = Elements(i).Image->MaterialVSize() * ImageScale;
			if ( mU > ItemWidth )
				ItemWidth = mU;
			if ( mV > ItemHeight )
				ItemHeight = mV;
		}

		NoVisibleCols = Max<FLOAT>((ClientBounds[2] - ClientBounds[0]) / ItemWidth, 1);
		NoVisibleRows = Max<FLOAT>((ClientBounds[3] - ClientBounds[1]) / ItemHeight, 1);

		if ( MyScrollBar )
			MyScrollBar->Step = NoVisibleCols;

		ItemsPerPage = NoVisibleRows * NoVisibleCols;
		if ( !bAnimating && MyScrollBar && MyScrollBar->bVisible != ItemsPerPage > 0 && ItemCount > ItemsPerPage )
		{
			MyScrollBar->eventSetVisibility(ItemsPerPage > 0 && ItemCount > ItemsPerPage);
			SetAdjustedDims( Max<FLOAT>(ActualWidth() - (MyScrollBar->bVisible ? MyScrollBar->ActualWidth() : 0), 2.f), ActualHeight(), ActualLeft(), ActualTop() );
			UpdateBounds();
		}
	}
	else if ( CellStyle == CELL_FixedCount  )
	{
		ItemsPerPage = NoVisibleRows * NoVisibleCols;
		if ( !bAnimating && MyScrollBar && MyScrollBar->bVisible != ItemsPerPage > 0 && ItemCount > ItemsPerPage )
		{
			MyScrollBar->eventSetVisibility(ItemsPerPage > 0 && ItemCount > ItemsPerPage);
			SetAdjustedDims( Max<FLOAT>(ActualWidth() - (MyScrollBar->bVisible ? MyScrollBar->ActualWidth() : 0), 2.f), ActualHeight(), ActualLeft(), ActualTop() );
			UpdateBounds();
		}

		// Must round down here, since a FLOAT isn't big enough to hold the result of this operation with sufficient accuracy
		// It is possible that ItemWidth * NoVisibleCols will be larger than the difference between ClientBounds[2] - ClientBounds[0]
		ItemWidth  = appFloor( Max<FLOAT>(0.f, (ClientBounds[2] - ClientBounds[0]) / NoVisibleCols) );
		ItemHeight = appFloor( Max<FLOAT>(0.f, (ClientBounds[3] - ClientBounds[1]) / NoVisibleRows) );
	}

	unguardobj;
}

void UGUIVertImageList::Draw(UCanvas* Canvas)
{
	guard(UGUIVertImageList::Draw);

	if ( !bVisible || (ItemCount == 0 && !bVisibleWhenEmpty) || INVALIDRENDER )
		return;

	check(ItemCount == Elements.Num());

	UGUIController* C = Controller;
	UGUIListBase::Draw(Canvas);

	Style->Draw( Canvas, MenuState, Bounds[0], Bounds[1], Bounds[2] - Bounds[0], Bounds[3] - Bounds[1]);

	FLOAT X = ClientBounds[0], Y = ClientBounds[1];
	if ( C->ActiveControl == this && bHotTrack && C->ActivePage == PageOwner && C->HasMouseMoved() )
	{
		if ( PerformHitTest(C->MouseX, C->MouseY) )
		{
			INT OldIndex = Index;
			Index = eventCalculateIndex(1);
			if (OldIndex != Index)
			{
				if ( bHotTrackSound )
					C->PlayComponentSound(CS_Hover);

				if ( DELEGATE_IS_SET(OnTrack) )
					delegateOnTrack(this, OldIndex);
			}
		}
		else Index = INDEX_NONE;
	}

	INT Col = 0;
	for ( INT i = Top; i < Top + ItemsPerPage; i++ )
	{
		if ( i >= ItemCount )
			return;

		if ( !Elements(i).Image )
			continue;

		UBOOL bIsSelected = i == Index, bIsDrop = i == DropIndex,
		bIsPending = ((bDropSource || bDropTarget) && (bIsDrop || SelectedItems.FindItemIndex(i) != INDEX_NONE));

		if ( DELEGATE_IS_SET(OnDrawItem) )
			delegateOnDrawItem(Canvas, i, X, Y, ItemWidth, ItemHeight, bIsSelected, bIsPending);
		else
		{
			if ( bIsSelected || (bIsPending && !bIsDrop) )
			{
				if ( SelectedStyle )
				{
					if ( SelectedStyle->Images[MenuState] )
						SelectedStyle->Draw(Canvas, MenuState, X, Y, ItemWidth, ItemHeight);
					else Canvas->DrawTile(C->DefaultPens[0], X, Y, ItemWidth, ItemHeight, 0,0,32,32,0.f, SelectedStyle->ImgColors[MenuState],FPlane(0,0,0,0));
				}
				else
				{
					if ( MenuState == MSAT_Focused || MenuState == MSAT_Pressed )
					{
						if ( !SelectedImage )
							Canvas->DrawTile(C->DefaultPens[0], X, Y, ItemWidth, ItemHeight, 0,0,32,32,0.f,SelectedBKColor,FPlane(0,0,0,0));
						else
						{
							Canvas->Color = SelectedBKColor;
							Canvas->DrawTileStretched(SelectedImage, X, Y, ItemWidth, ItemHeight);
						}
					}
				}
			}
			DrawItem( Canvas, i, X, Y, ItemWidth, ItemHeight, bIsSelected, bIsPending );

			X += ItemWidth;

			// start a new row
			if ( ++Col > NoVisibleCols || X + ItemWidth > ClientBounds[2] )
			{
				Col = 0;
				X = ClientBounds[0];
				Y += ItemHeight;
			}
		}
	}

	unguardobj;
}

void UGUIVertImageList::DrawItem( UCanvas* Canvas, INT Item, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, UBOOL bSelected, UBOOL bPending )
{
	guard(UGUIVertImageList::DrawItem);

	if ( INVALIDRENDER )
		return;

	FColor Color;
	if (bSelected)
	{
		Color = FColor(255,255,255,255);
		Canvas->Color = SelectedBKColor;
		Canvas->DrawTileScaleBound(Controller->DefaultPens[0], X + (XL/2) - (ItemWidth/2), Y + (YL/2) - (ItemHeight/2), ItemWidth, ItemHeight);
	}
	else
		Color = FColor(128,128,128,255);

	check(Item >= 0 && Item < ItemCount);
	UMaterial* Mat = Elements(Item).Image;
	if (!Mat)
		Mat = Controller->DefaultPens[0];		// TODO Display fallback texture if no material

	Canvas->DrawTile(Mat,X+HorzBorder,Y+VertBorder,ItemWidth-(HorzBorder*2),ItemHeight-(VertBorder*2),0,0,Mat->MaterialUSize(),Mat->MaterialVSize(),0,Color,FPlane(0.f,0.f,0.f,0.f));
	if (Elements(Item).Locked)
		Canvas->DrawTile(LockedMat,X+HorzBorder,Y+VertBorder,ItemWidth-(HorzBorder*2),ItemHeight-(VertBorder*2),0,0,Mat->MaterialUSize(),Mat->MaterialVSize(),0,FColor(128,128,128,32),FPlane(0.f,0.f,0.f,0.f));

	if (bPending && OutlineStyle)
	{
		FLOAT NewX = X + (XL / 2) - (ItemWidth / 2);
		FLOAT NewY = Y + (YL / 2) - (ItemHeight / 2);
		if (OutlineStyle->Images[MenuState])
		{
			OutlineStyle->Draw(Canvas, 3, NewX + 1, NewY + 1, ItemWidth - 2, ItemHeight - 2);
			if (DropState == DRP_Source && DropIndex != Item)
				OutlineStyle->Draw(Canvas, MenuState, Controller->MouseX - MouseOffset[0] + NewX - ClientBounds[0], Controller->MouseY - MouseOffset[1] + NewY - ClientBounds[1], ItemWidth - 2, ItemHeight - 2);
		}
	}

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIList
// =======================================================================================================================================================
// =======================================================================================================================================================
void UGUIList::DrawItem(UCanvas* Canvas, INT Item, FLOAT X, FLOAT Y, FLOAT WT, FLOAT HT, UBOOL bSelected, UBOOL bPending)
{
	guard(UGUIList::DrawItem);

	if ( INVALIDRENDER )
		return;
	
	if ( Elements(Item).bSection ) 
	{
		if ( Elements(Item).Item == TEXT("") )
			return;

		FLOAT XL,YL;
		SectionStyle->TextSize(Canvas,MenuState, *(Elements(Item).Item), XL, YL, FontScale);

		FLOAT mX = X + (WT/2);
		FLOAT mY = Y + (HT/2);

		SectionStyle->DrawText(Canvas, MenuState,mX-(XL/2),mY-(YL/2),XL,YL,TextAlign,*(Elements(Item).Item),FontScale);

		FLOAT BarW = ((WT - XL) /2) * 0.8;
		
		Canvas->DrawTile(Controller->DefaultPens[0],X + BarW*0.1, mY-2, BarW, 5, 0, 0, 8, 8, 0, SectionStyle->ImgColors[MenuState],FPlane(0,0,0,0));
		Canvas->DrawTile(Controller->DefaultPens[0],mX+(XL/2) + BarW*0.1, mY-2, BarW, 5, 0, 0, 8, 8, 0, SectionStyle->ImgColors[MenuState],FPlane(0,0,0,0));

		return;
	}

	if (bSelected)
		SelectedStyle->DrawText(Canvas, MenuState, X,Y,WT,HT, TextAlign, *(Elements(Item).Item), FontScale);
	else
		Style->DrawText(Canvas, MenuState, X,Y,WT,HT, TextAlign, *(Elements(Item).Item), FontScale);
	
	unguardobj;
}

// EEEUUUCCHHH!!!
static UGUIList* GSList;

static INT Compare( FGUIListElem& A, FGUIListElem& B )
{
	if(GSList)
		return GSList->delegateCompareItem(A, B);
	else
		return appStricmp( *(A.Item), *(B.Item) );
}

void UGUIList::execSortList( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIList::SortList);

	P_FINISH;

	// GSList being set indicates we should use the script delegate to compare elements.
	// If no delegate is set - we sort alphabetically.
	if(DELEGATE_IS_SET(CompareItem))
		GSList=this;
	else
		GSList=NULL;

	
	Sort( &Elements(0), Elements.Num() );

	unguardexec;
}


// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIMultiOptionList
// =======================================================================================================================================================
// =======================================================================================================================================================

INT UGUIMultiOptionList::CalculateIndex( UBOOL bRequireValidIndex )
{
	guard(UGUIComponent::CalculateIndex);

	INT NewIndex(INDEX_NONE), i(0);
	FLOAT X(ClientBounds[0]), Y(ClientBounds[1]);

	if ( WithinBounds(Controller->MouseX, Controller->MouseY) )
	{
		NewIndex = Top;
		if ( bVerticalLayout )
			i = 1;

		while ( NewIndex < ItemCount )
		{
			if ( !eventElementVisible(NewIndex) )
			{
				NewIndex++;
				continue;
			}

			if ( Controller->MouseX >= X && Controller->MouseX <= X + ItemWidth &&
			     Controller->MouseY >= Y && Controller->MouseY <= Y + ItemHeight )
			    break;

			if ( bVerticalLayout )
			{
				NewIndex += ItemsPerColumn;
				X += ItemWidth;

				if ( NewIndex >= ItemCount )
				{
					X = ClientBounds[0];
					Y += ItemHeight;
					NewIndex = Top + i++;
					if ( NewIndex >= Top + Min(ItemsPerPage, ItemCount) / NumColumns )
					{
						if ( bRequireValidIndex )
							NewIndex = -1;
						break;
					}
				}
			}

			else
			{
				X += ItemWidth;
				if ( ++i >= NumColumns )
				{
					i = 0;
					X = ClientBounds[0];
					Y += ItemHeight;
				}
			}

			NewIndex++;
		}
	}

	if ( NewIndex >= ItemCount && bRequireValidIndex )
		NewIndex = INDEX_NONE;

	return Min(NewIndex, ItemCount - 1);
	unguard;
}


UGUIComponent* UGUIMultiOptionList::GetFocused() const
{
	guard(UGUIComponent::GetFocused);

	if ( bHotTrack )
		return FocusInstead;

	return Index >= 0 && Index < Elements.Num() ? Elements(Index) : NULL;
	unguardobj;
}

UBOOL UGUIMultiOptionList::NativeKeyEvent(BYTE &iKey, BYTE &State, FLOAT Delta)
{
	guard(UGUIMultiOptionList::NativeKeyEvent);

	if ( INVALIDMENU )
		return false;

	if ( Controller->CtrlPressed )
	{
		// Allow MultiOptionList to receive a chance to process the input first.
		if (Super::NativeKeyEvent( iKey, State, Delta ))
			return true;

		return FocusInstead && FocusInstead->NativeKeyEvent(iKey, State, Delta);
	}

	else
	{
		if ( FocusInstead && FocusInstead->NativeKeyEvent(iKey, State, Delta) )
			return true;

		return Super::NativeKeyEvent(iKey,State,Delta);
	}

	unguardobj;
}

UBOOL UGUIMultiOptionList::NativeKeyType(BYTE &iKey, TCHAR Unicode)
{
	guard(UGUIMultiOptionList::NativeKeyType);

	if (Super::NativeKeyType(iKey,Unicode))
		return true;

	return FocusInstead && FocusInstead->NativeKeyType(iKey, Unicode);
	unguardobj;
}

void UGUIMultiOptionList::NativeInvalidate(UGUIComponent* Who)
{
	guard(UGUIMultiOptionList::NativeInvalidate);

	if (FocusInstead)
		FocusInstead->NativeInvalidate(Who);

	Super::NativeInvalidate(Who);
	unguardobj;
}

UBOOL UGUIMultiOptionList::SpecialHit(UBOOL bForce)
{
	guard(UGUIMultiOptionList::SpecialHit);

	if (Super::SpecialHit(bForce))
		return true;

	if (FocusInstead && FocusInstead->SpecialHit(bForce))
		return true;

	INT i = CalculateIndex();
	if ( Elements.IsValidIndex(i) && Elements(i) != FocusInstead && Elements(i)->SpecialHit(bForce) )
		return true;
	return false;

	unguardobj;
}

UGUIComponent* UGUIMultiOptionList::UnderCursor( FLOAT MouseX, FLOAT MouseY )
{
	guard(UGUIMultiOptionList::UnderCursor);

	UGUIComponent* Comp = GetFocused(), *Under = NULL;
	if ( Comp )
		Under = Comp->UnderCursor(MouseX, MouseY);

	if ( !Under )
	{
		INT UnderIndex = CalculateIndex(1);
		Comp = Elements.IsValidIndex(UnderIndex) ? Elements(UnderIndex) : NULL;
		Under = Comp ? Comp->UnderCursor(MouseX,MouseY) : NULL;
	}

	if ( Under )
		return Under;

	// Must return Super::UnderCursor() (rather than 'this'), or scrollbar can never become the controller's active control
	return Super::UnderCursor(MouseX,MouseY);

	unguardobj;
}

void UGUIMultiOptionList::PreDraw(UCanvas* Canvas)
{
	guard(UGUIMultiOptionList::PreDraw);

	if ( INVALIDRENDER )
		return;

	UGUIListBase::PreDraw(Canvas);

	ItemHeight = Canvas->ClipY * ItemScaling;
	ItemWidth = (ClientBounds[2] - ClientBounds[0]) / NumColumns;
	ItemsPerPage = appFloor((ClientBounds[3] - ClientBounds[1]) / ItemHeight) * NumColumns;
	ItemsPerColumn = appRound( FLOAT(ItemCount) / FLOAT(NumColumns));

	if ( !bAnimating && MyScrollBar && MyScrollBar->bVisible != ItemsPerPage > 0 && ItemCount > ItemsPerPage )
	{
		MyScrollBar->eventSetVisibility(ItemsPerPage > 0 && ItemCount > ItemsPerPage);
		SetAdjustedDims( Max<FLOAT>(ActualWidth() - (MyScrollBar->bVisible ? MyScrollBar->ActualWidth() : 0), 2.f), ActualHeight(), ActualLeft(), ActualTop() );
		UpdateBounds();
	}

	FLOAT Y = ClientBounds[1];
	FLOAT X = ClientBounds[0];

	INT col = 0;
	Top = Max(Top, 0);

	if ( bVerticalLayout )
	{
		INT i = Top;
		col = 1; // this is really the row
		while(i < ItemCount)
		{
			if (Elements(i) == NULL)
			{
				debugf(NAME_Warning, TEXT("Element %i of GUIMultiOptionList %s was NULL!"), i, GetName());
				continue;
			}
			Elements(i)->WinHeight = ItemHeight * (1.0 - ItemPadding);
			Elements(i)->WinWidth = ((Bounds[2] - Bounds[0]) / NumColumns) - Style->BorderOffsets[0] - Style->BorderOffsets[2];

			Elements(i)->WinLeft = X;
			Elements(i)->WinTop = Y + (ItemHeight * (ItemPadding / 2));

			Elements(i)->PreDraw(Canvas);

			i += ItemsPerColumn;
			X += (ItemWidth + Style->BorderOffsets[0]);

			if ( i >= ItemCount )
			{
				X = ClientBounds[0];
				Y += ItemHeight;
				i = Top + col++;
				if (  i >= Top + Min(ItemsPerPage , ItemCount) / NumColumns )
					break;
			}
		}
	}

	else
	{
		for ( INT i = Top; i < Top + ItemsPerPage; i++ )
		{
			if (i >= ItemCount)
				break;

			if (Elements(i) == NULL)
			{
				debugf(NAME_Warning, TEXT("Element %i of GUIMultiOptionList %s was NULL!"), i, GetName());
				continue;
			}

			if (col >= NumColumns)
			{
				col = 0;
				X = ClientBounds[0];
				Y += ItemHeight;
			}

			Elements(i)->WinHeight = ItemHeight * (1.0 - ItemPadding);
//			Elements(i)->WinWidth = ((Bounds[2] - Bounds[0]) / NumColumns) - Style->BorderOffsets[0] - Style->BorderOffsets[2];

			FLOAT MaxColWidth = ((Bounds[2] - Bounds[0]) / NumColumns) - Style->BorderOffsets[0] - Style->BorderOffsets[2];
			Elements(i)->WinWidth = MaxColWidth * ColumnWidth;

			Elements(i)->WinLeft = X;
			Elements(i)->WinTop = Y + (ItemHeight * (ItemPadding / 2));

			Elements(i)->PreDraw(Canvas);
			X += (ItemWidth + Style->BorderOffsets[0]);
			col++;
		}
	}

	unguardobj;
}

void UGUIMultiOptionList::Draw(UCanvas* Canvas)
{
	guard(UGUIMultiOptionList::Draw);

	if ( !bVisible || (ItemCount == 0 && !bVisibleWhenEmpty) || INVALIDRENDER )
		return;
	
	UGUIController* C = Controller;

	UGUIListBase::Draw(Canvas);
	Style->Draw( Canvas, MenuState, Bounds[0], Bounds[1], Bounds[2] - Bounds[0], Bounds[3] - Bounds[1]);

	Top = Max( Top, 0 );

	INT col = 0;
	
	if ( bHotTrack && C->ActivePage == PageOwner && C->HasMouseMoved() && PerformHitTest(C->MouseX,C->MouseY) )
	{
		INT i = CalculateIndex(1);
		if ( Elements.IsValidIndex(i) && eventCanFocusElement(Elements(i)) && (!C->ActiveControl || !(C->ActiveControl->MenuState == MSAT_Pressed && C->ActiveControl->bCaptureMouse)) )
		{
			INT OldIndex = Index;
			Index = i;
			if ( OldIndex != Index )
			{
				if ( bHotTrackSound )
					C->PlayComponentSound(CS_Hover);

				if ( DELEGATE_IS_SET(OnTrack) )
					delegateOnTrack(this,OldIndex);
			}
		}

		else Index = INDEX_NONE;
	}

	FLOAT Y = ClientBounds[1];
	FLOAT X = ClientBounds[0];

	UGUIStyles* FocusedStyle = NULL;
	UGUIMenuOption* FocusedElem = NULL;
	if ( bVerticalLayout )
	{
		INT i = Top;
		col = 1;
		while( i < ItemCount )
		{
			if ( !Elements(i) )
				continue;

			// Check if this is the selected Index
			UBOOL bIsSelected = FocusInstead == Elements(i);

			if ( DELEGATE_IS_SET(OnDrawItem) )
				delegateOnDrawItem(Canvas, i, X, Y, ItemWidth, ItemHeight, bIsSelected, 0);
			else
			{
				if ( bIsSelected )
				{
					FocusedElem = Elements(i);
					FocusedStyle = FocusedElem->MyLabel->Style;
				}
				
				DrawItem(Canvas, i, X, Y, ItemWidth, ItemHeight, bIsSelected, 0);
			}
			i += ItemsPerColumn;
			X += ItemWidth;

			if ( i >= ItemCount )
			{
				X = ClientBounds[0];
				Y += ItemHeight;
				i = Top + col++;
				if ( i >= Top + Min(ItemsPerPage,ItemCount) / NumColumns )
					break;
			}
		}
	}
	else
	{
		for ( INT i = Top; i < Top + ItemsPerPage; i++ )
		{
			if (i >= ItemCount)
				break;

			if ( !Elements(i) )
				continue;

			if ( col >= NumColumns)
			{
				col = 0;
				X = ClientBounds[0];
				Y += ItemHeight;
			}

			// Check if this is the selected Index
			UBOOL bIsSelected = Elements(i) == FocusInstead;

			if( DELEGATE_IS_SET(OnDrawItem) )
				delegateOnDrawItem( Canvas, i, X, Y, ItemWidth, ItemHeight, bIsSelected, 0 );

			else
			{
				if ( bIsSelected )
				{
					FocusedElem = Elements(i);
					FocusedStyle = FocusedElem->MyLabel->Style;
				}

				DrawItem(Canvas, i, X, Y, ItemWidth, ItemHeight, bIsSelected, 0);
			}

			X += ItemWidth;
			col++;
		}
	}

	// Must draw FocusedElem last so that combobox drop-down lists are always drawn on top of all other elements
	if ( FocusedElem )
	{
		FocusedElem->Draw(Canvas);
		if (FocusedStyle)
			FocusedElem->MyLabel->Style = FocusedStyle;
	}
	unguardobj;
}

void UGUIMultiOptionList::DrawItem( UCanvas* Canvas, INT Item, FLOAT X, FLOAT Y, FLOAT W, FLOAT HT, UBOOL bSelected, UBOOL bPending )
{
	guard(UGUIMultiOptionList::DrawItem);

	if ( INVALIDRENDER )
		return;

	UGUIMenuOption* Elem = Elements(Item);
	UGUIStyles* TempStyle = Elem->MyLabel->Style;

	if (bSelected && bDrawSelectionBorder) 
	{
		if ( SelectedStyle )
		{
			Elem->MyLabel->Style = SelectedStyle;
			if (SelectedStyle->Images[MenuState]!=NULL)
				SelectedStyle->Draw(Canvas, MenuState, X, Y, W, HT);
			else
				Canvas->DrawTile(Controller->DefaultPens[0], X, Y, W, HT, 0, 0, 32, 32, 0.0f, SelectedStyle->FontBKColors[MenuState],FPlane(0,0,0,0));
		}
		else
		{
			// Display the selection
			if (SelectedImage==NULL)
				Canvas->DrawTile(Controller->DefaultPens[0], X, Y, W, HT, 0, 0, 32, 32, 0.0f, SelectedBKColor,FPlane(0,0,0,0));
			else
			{
				Canvas->Color = SelectedBKColor;
				Canvas->DrawTileStretched(SelectedImage, X, Y, W, HT);
			}
		}
	}

	// If this is the FocusedElem, its Draw() will be called at the end of
	// UGUIMultiOptionList::Draw(), so only draw the non focused elements here
	if ( !FocusInstead || Elem != FocusInstead )
	{
		if (SectionStyle && Elem->Tag == -3 && Elem->Style)
		{
			FLOAT XL = 0, YL = 0;
			Elem->Style->TextSize( Canvas, Elem->MenuState, *(Elem->MyLabel->Caption), XL, YL, Elem->FontScale);
			Canvas->DrawTile(Controller->DefaultPens[0], X, Y + ((ItemHeight - YL) / 2), W, YL, 0,0,32,32, 0.0f, SectionStyle->FontBKColors[MenuState],FPlane(0,0,0,0));
		}

		Elem->Draw(Canvas);
		Elem->MyLabel->Style = TempStyle;
	}

	unguardobj;
}

UBOOL UGUIMultiOptionList::MousePressed(UBOOL IsRepeat)	
{
	guard(GUIMultiOptionList::MousePressed);

	if (FocusInstead && FocusInstead->MousePressed(IsRepeat))
		return true;

	else
	{
		Top = Max(Top,0);
		if ( bVerticalLayout )
		{
			INT i = Top, col = 1;
			while( i < ItemCount )
			{
				if ( !Elements(i) )
					continue;

				if ( Elements(i)->MousePressed(IsRepeat) )
					return true;

				i += ItemsPerColumn;
				if ( i >= ItemCount )
				{
					i = Top + col++;
					if ( i >= Top + Min(ItemsPerPage,ItemCount) / NumColumns )
						break;
				}
			}
		}
		else
		{
			for (INT i = Top; i < Top + ItemsPerPage; i++)
			{
				if (i >= ItemCount)
					break;

				if ( !Elements(i) )
					continue;

				if (Elements(i)->MousePressed(IsRepeat))
					return true;
			}
		}
	}

	return Super::MousePressed(IsRepeat);
	unguardobj;
}

UBOOL UGUIMultiOptionList::MouseReleased()
{
	guard(GUIMultiOptionList::MouseReleased);

	if (FocusInstead && FocusInstead->MouseReleased())
		return true;

	else
	{
		Top = Max(Top, 0);
		if ( bVerticalLayout )
		{
			INT i = Top, col = 1;
			while( i < ItemCount )
			{
				if ( !Elements(i) )
					continue;

				if ( Elements(i)->MouseReleased() )
					return true;

				i += ItemsPerColumn;
				if ( i >= ItemCount )
				{
					i = Top + col++;
					if ( i >= Top + Min(ItemsPerPage,ItemCount) / NumColumns )
						break;
				}
			}
		}
		else
		{
			for (INT i = Top; i < Top + ItemsPerPage; i++)
			{
				if (i >= ItemCount)
					break;

				if ( !Elements(i) )
					continue;

				if (Elements(i)->MouseReleased())
					return true;
			}
		}
	}

	return Super::MouseReleased();
	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIMultiColumnListBox
// =======================================================================================================================================================
// =======================================================================================================================================================

void UGUIMultiColumnListBox::Draw(UCanvas* Canvas)
{
	guard(UGUIMultiColumnListBox::Draw);

	if ( INVALIDRENDER )
		return;

	if (bVisible && bFullHeightStyle)
		Style->Draw(Canvas,MenuState, ActualLeft(),ActualTop(),ActualWidth(),ActualHeight());

	Super::Draw(Canvas);

	unguard;
}

void UGUIMultiColumnListBox::PreDraw(UCanvas* Canvas)
{

	guard(UGUIMultiColumnListBox::PreDraw);

	if ( INVALIDRENDER )
		return;

// Skip UGUIListBoxBase::PreDraw
	UGUIComponent::PreDraw(Canvas);

	UBOOL bSBVisible, bHeaderVisible;
	if ( !bVisible )
		return;

	if (MyScrollBar==NULL || Header==NULL || MyList==NULL)
	{
		debugf(TEXT("%s::PreDraw() has invalid components!"), GetName());
		if (MyScrollBar==NULL)
			debugf(TEXT("Scrollbar was NULL"));

		if (MyList==NULL)
			debugf(TEXT("List was NULL"));

		if (Header==NULL)
			debugf(TEXT("Header was NULL"));

		return;
	}

	FLOAT HeaderSize = Header->ActualHeight();
	DECLAREBOUNDS;

	MyScrollBar->WinWidth = HeaderSize / 1.5;
	bHeaderVisible = bDisplayHeader && AH > HeaderSize;
	if (bHeaderVisible != (Header->bVisible && bVisible) && !(bTravelling || bSizing))
        Header->eventSetVisibility(bHeaderVisible && bVisible);

	Header->SetAdjustedDims( AW, HeaderSize, AL, AT );
    bSBVisible = (Header->bVisible && (List->ItemsPerPage / FLOAT(List->ItemCount) < 1));

	if (bSBVisible != (bVisible && MyScrollBar->bVisible) && !(bTravelling || bSizing))
		MyScrollBar->eventSetVisibility(bSBVisible);

	MyList->SetAdjustedDims( AW - (MyScrollBar->bVisible ? HeaderSize : 0), AH - (Header->bVisible ? HeaderSize : 0.f), AL, AT + (Header->bVisible ? HeaderSize : 0.f));

	MyScrollBar->SetAdjustedDims( HeaderSize, AH - HeaderSize, AL + MyList->ActualWidth(), AT + HeaderSize );
	
	if (bVisible)
	{
		if ( List->bInit )
            List->eventInitializeColumns(Canvas);

		if (!List->bVisibleWhenEmpty && !(bTravelling || bSizing))
		{
			if (List->bVisible != (List->ItemCount > 0))
				List->eventSetVisibility(List->ItemCount > 0);
		}
	}
	PreDrawControls(Canvas);	
	unguardobj;
}
// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIMultiColumnListHeader
// =======================================================================================================================================================
// =======================================================================================================================================================
UBOOL UGUIMultiColumnListHeader::MousePressed(UBOOL IsRepeat)
{
	guard(UGUIMultiColumnListHeader::MousePressed);
	
	if ( !MyList || INVALIDMENU )
		return false;

	UGUIController* C = Controller;
	if ( !Super::MousePressed(IsRepeat) )
		return false;

	if( !IsRepeat )
	{
		ClickingCol = -1;
		SizingCol = -1;
		FLOAT x = ActualLeft(), x2 = 0.f;
		for( INT i=0;i<MyList->ColumnWidths.Num();i++ )
		{
			x += MyList->ColumnWidths(i);
			if( x >= C->MouseX-5.f && x <= (FLOAT)C->MouseX+5.f )
			{
				bCaptureMouse = 1;
				SizingCol = i;
			}
			else
			if( SizingCol >= 0 )
				return true;
		}


		x = ActualLeft() + 5.f, x2 = 0.f;
		for( INT i = 0; i < MyList->ColumnWidths.Num(); i++ )
		{
			x2 = x + (MyList->ColumnWidths(i) - 10.f);

			if( C->MouseX >= x && C->MouseX <= x2 && x2 - x > 5.f)
			{
				ClickingCol = i;

				if( MyList->SortColumn == i )
				{
					MyList->SortDescending = !MyList->SortDescending;

					if(MyList->SortDescending == 0)
						C->PlayComponentSound(CS_Up);
					else
						C->PlayComponentSound(CS_Down);
				}
				else if(MyList->SortColumn != -1)
				{
					MyList->SortColumn = i;
					MyList->SortDescending = 0;
					C->PlayComponentSound(CS_Up);
				}

				MyList->eventOnSortChanged();
				return true;
			}
			x = x2 + 10.f;
		}
	}
	return true;
	unguardobj;
}

UBOOL UGUIMultiColumnListHeader::MouseReleased()
{
	guard(UGUIMultiColumnListHeader::MouseReleased);

	if ( !MyList || INVALIDMENU )
		return false;

	Super::MouseReleased();

	if (SizingCol >= 0 && OBJ_DELEGATE_IS_SET(MyList, OnColumnSized))
		MyList->delegateOnColumnSized(SizingCol);

	ClickingCol = -1;
	SizingCol = -1;
	return true;
	unguardobj;
}

UBOOL UGUIMultiColumnListHeader::MouseMove(INT XDelta, INT YDelta)
{
	guard(UGUIMultiColumnListHeader::MouseMove);

	if ( !MyList || INVALIDMENU )
		return false;

	FLOAT CMouseX = Controller->MouseX;
	Super::MouseMove(XDelta,YDelta);

	if( MenuState==MSAT_Pressed && SizingCol >= 0 && SizingCol<MyList->ColumnWidths.Num() )
	{
		TArray<FLOAT> ColumnPos;
	
		FLOAT AW = MenuOwner->ActualWidth(), AL = MenuOwner->ActualLeft();
		FLOAT MinWidth = 0.1f * AW, x = AL;
		for (INT i = 0; i < MyList->ColumnWidths.Num(); i++ )
		{
			x += MyList->ColumnWidths(i);
			ColumnPos.AddItem(x);
		}

		FLOAT MinPos = MinWidth + (SizingCol > 0 ? ColumnPos(SizingCol - 1) : AL);
		FLOAT MaxPos = SizingCol == MyList->ColumnWidths.Num() - 1 
			? (AL + AW) - MinWidth : ColumnPos(SizingCol + 1) - MinWidth;
		ColumnPos(SizingCol) = Clamp<FLOAT>( CMouseX, MinPos, MaxPos );

		x = AL;
		for ( INT i = 0; i < ColumnPos.Num(); i++ )
		{
			MyList->ColumnWidths(i) = ColumnPos(i) - x;
			x = ColumnPos(i);
		}

		return true;

/*		FLOAT x = ActualLeft();
		for( INT i=0;i<SizingCol;i++ )
			x += MyList->ColumnWidths(i);
		MyList->ColumnWidths(SizingCol) = Clamp<FLOAT>( CMouseX - x, 0, ActualLeft() + ActualWidth() - x );
*/	}
	else
	{
		bCaptureMouse = 0;
		SizingCol = -1;
	}
	return false;
	unguardobj;
}

UBOOL UGUIMultiColumnListHeader::MouseHover()
{
	guard(UGUIMultiColumnListHeader::MouseHover);

	if (!MyList || INVALIDMENU )
		return false;

	FLOAT CMouseX = Controller->MouseX;
	if ( Super::MouseHover() )
		return true;

	// Update cursor if we are just waving the cursor over the thing. Dont care which one it is.
	INT OnColSize = -1;
	FLOAT x = ActualLeft();
	for( INT i=0; i<MyList->ColumnWidths.Num() && OnColSize == -1; i++ )
	{
		if( CMouseX > x + 5.f && CMouseX < x + MyList->ColumnWidths(i) - 5.f) // mouse hovering over column center area
		{
			if( i < MyList->ColumnHeadingHints.Num() && Hint != MyList->ColumnHeadingHints(i))
			{
				Hint = MyList->ColumnHeadingHints(i);  // change hint for column header
			}
		}

		x += MyList->ColumnWidths(i);
		if( x >= CMouseX - 5.f && x <= CMouseX + 5.f )
		{
			OnColSize = i;
		}
	}

	if(OnColSize != -1)
		MouseCursorIndex = 5;
	else
		MouseCursorIndex = 0;

	return false;

	unguardobj;
}


void UGUIMultiColumnListHeader::PreDraw(UCanvas* Canvas)
{
	guard(UGUIMultiColumnListHeader::PreDraw);

	if ( INVALIDRENDER )
		return;

	FLOAT XL, YL;
	Style->TextSize(Canvas, MenuState, TEXT("A"), XL, YL, FontScale);
	WinHeight = YL * 1.2;

	Super::PreDraw(Canvas);

	unguardobj;
}

void UGUIMultiColumnListHeader::Draw(UCanvas* Canvas)
{
	guard(UGUIMultiColumnListHeader::Draw);

	if ( !bVisible || !MyList || INVALIDRENDER )
		return;

	Super::Draw(Canvas);

	FLOAT HeaderSize,TopSize,BarSize;
	
	HeaderSize = Bounds[3] - Bounds[1];
	if (HeaderSize>16)
	{
		TopSize=16;
		BarSize=HeaderSize-16;
	}
	else
	{
		TopSize=HeaderSize;
		BarSize=0;
	}

	FLOAT x = Bounds[0];
    for( INT i=0;i<MyList->ColumnWidths.Num();i++ )
	{
		if ( i >= MyList->ColumnHeadings.Num() )
		{
			debugf(TEXT("%s: ColumnWidths/ColumnHeadings mismatch!!  ( %i/%i )"), MyList->GetMenuPath(), MyList->ColumnWidths.Num(), MyList->ColumnHeadings.Num());
			return;
		}

		FLOAT y = Bounds[1];
		FLOAT w = Min<FLOAT>( MyList->ColumnWidths(i), Bounds[2]-x );
		if( w > 0 )
		{
			BYTE DrawState;		
			if( i == MyList->SortColumn )
				DrawState = MSAT_Focused;
			else
			if( i == ClickingCol )
				DrawState = MSAT_Pressed;
			else
				DrawState = MSAT_Blurry;			

			Canvas->Color = Style->ImgColors[DrawState];
			Canvas->DrawTileStretched(Style->Images[DrawState],x,y,w,TopSize);
			y+=TopSize;
			if (BarSize>0)
			{
				Canvas->DrawTileStretched(BarStyle->Images[DrawState],x,y,w,BarSize);
			}

//			Style->Draw( Canvas, DrawState, x, Bounds[1], w, HeaderSize );
			Style->DrawText( Canvas, DrawState, x, ClientBounds[1], w, ClientBounds[3] - ClientBounds[1], 1, *MyList->ColumnHeadings(i), FontScale );
			x += w;
		}
    }
//	FLOAT x2 = ActualWidth()+ActualLeft();
//	if( x < x2 )
//		Style->Draw( Canvas, MSAT_Blurry, x, Bounds[1], x2 - x, Bounds[3] - Bounds[1] );
	unguardobj;
}
// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIMultiColumnList
// =======================================================================================================================================================
// =======================================================================================================================================================

FMultiColumnSortData::FMultiColumnSortData( INT InSortItem, const TCHAR* InString ) :
SortItem(InSortItem), SortString(InString)
{ }

static INT MLSortOrder=1;
static INT Compare( FMultiColumnSortData& A, FMultiColumnSortData& B )
{
	return MLSortOrder*appStricmp( *(A.SortString), *(B.SortString) );
}

void UGUIMultiColumnList::execGetListIndex( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIMultiColumnList::execGetListIndex);

	P_GET_INT(YourArrayIndex);
	P_FINISH;

	*(INT*)Result = FindSortIndex(YourArrayIndex);
	unguardexec;
}

INT UGUIMultiColumnList::FindSortIndex( INT SortItem ) const
{
	guard(UGUIMultiColumnList::FindSortIndex);

	for ( INT i = 0; i < SortData.Num(); i++ )
		if ( SortData(i).SortItem == SortItem )
			return i;

	return INDEX_NONE;

	unguard;
}

void UGUIMultiColumnList::execAddedItem(FFrame& Stack, RESULT_DECL )
{
    guard(UGUIMultiColumnList::execAddedItem);

	P_GET_INT_OPTX(YourArrayIndex,INDEX_NONE);
	P_FINISH;

	if ( YourArrayIndex == INDEX_NONE )
		YourArrayIndex = SortData.Num();

	FString SortString = delegateGetSortString(YourArrayIndex);
	if ( !SortString.IsNumeric() )
		SortString += appItoa(YourArrayIndex);

	new(SortData) FMultiColumnSortData(YourArrayIndex, *SortString);
	InvSortData.AddItem(InvSortData.Num());
	ItemCount++;

	unguardexec;
}

void UGUIMultiColumnList::execRemovedItem( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIMultiColumnList::execRemovedItem);

	P_GET_INT(YourArrayIndex);
	P_FINISH;

	INT i = FindSortIndex(YourArrayIndex);
	if ( SortData.IsValidIndex(i) )
	{
		SortData.Remove(i);
		InvSortData.Remove(i);
		ItemCount--;
		// fix up SortItems
		for( INT x=0; x < SortData.Num(); x++ )
			if( SortData(x).SortItem >= YourArrayIndex )
				SortData(x).SortItem--;

	}

	unguardexec;
}

void UGUIMultiColumnList::execUpdatedItem(FFrame& Stack, RESULT_DECL )
{
    guard(UGUIMultiColumnList::execUpdatedItem);
	P_GET_INT(YourArrayIndex);
	P_FINISH;

	INT i = FindSortIndex(YourArrayIndex);
	if ( SortData.IsValidIndex(i) )
	{
		SortData(i).SortString = delegateGetSortString(YourArrayIndex);
		if ( !SortData(i).SortString.IsNumeric() )
			SortData(i).SortString += appItoa(SortData(i).SortItem);
	}

	unguardexec;
}

void UGUIMultiColumnList::execChangeSortOrder(FFrame& Stack, RESULT_DECL )
{
    guard(UGUIMultiColumnList::execChangeSortOrder);
	P_FINISH;

	for( INT i=0;i<SortData.Num();i++ )
	{
		SortData(i).SortString = delegateGetSortString(SortData(i).SortItem);
		if ( !SortData(i).SortString.IsNumeric() )
			SortData(i).SortString += appItoa(SortData(i).SortItem);
	}

	unguardexec;
}

void UGUIMultiColumnList::execSortList(FFrame& Stack, RESULT_DECL )
{
    guard(UGUIMultiColumnList::execSortList);
	P_FINISH;

	if( SortColumn == -1 )
		return;

	if( SortDescending )
		MLSortOrder = -1;
	else
		MLSortOrder = 1;

	check(SortData.Num() == InvSortData.Num());
	TMap<INT,INT> TempSortData;

	for ( INT i = 0; i < SortData.Num(); i++ )
		TempSortData.Set( SortData(i).SortItem, i );

	Sort( &SortData(0), SortData.Num() );

	// update reverse map
	for ( INT i = 0; i < SortData.Num(); i++ )
	{
		INT j = TempSortData.FindRef(SortData(i).SortItem);
		InvSortData(j) = i;
	}

	NeedsSorting = 0;
	unguardexec;
}


// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUIScrollText
// =======================================================================================================================================================
// =======================================================================================================================================================


//
// Get a character's dimensions.
//
static inline void GetCharSize( UFont* Font, TCHAR InCh, INT& Width, INT& Height )
{
	guardSlow(GetCharSize);
	Width = 0;
	Height = 0;
	INT Ch    = (TCHAR)Font->RemapChar(InCh);
	if( Ch < Font->Characters.Num() )
	{
		FFontCharacter& Char = Font->Characters(Ch);
		Width = Char.USize;
		Height = Char.VSize;
	}
	unguardSlow;
}

void UGUIScrollText::PreDraw(UCanvas* Canvas)
{
	guard(UGUIScrollText::PreDraw);

	if ( !MyScrollBar || INVALIDRENDER || !Style->Fonts[MSAT_Blurry + (5*FontScale)])
		return;

	ItemCount = StringElements.Num();

	Super::PreDraw(Canvas);
	FLOAT cWidth = (Bounds[2] - Bounds[0]) - (Style->BorderOffsets[0] + Style->BorderOffsets[2]);

	bReceivedNewContent = bNewContent;

	// Re-wrap entire content (eg. we have changed resolution)
	if (bNewContent || cWidth != oldWidth)
	{
		UFont *Font = Style->Fonts[MSAT_Blurry + (5*FontScale)]->eventGetFont(Canvas->SizeX);
		// Carve big string into seperate strings of the right length
		StringElements.Empty();

		WrapURL(Content);
		Canvas->WrapStringToArray(*Content, &StringElements, cWidth, Font, **Separator);
		ItemCount = StringElements.Num();

		if (ItemCount == 0)
			VisibleLines = -1;
		else
			VisibleLines=0;
		VisibleChars=0;
		oldWidth = cWidth;
	}

	bNewContent = 0;

	// Note if we are viewing the bottom of the text.
	UBOOL bIsAtBottom = 0;
	if( (ItemCount - 1) < (Top + ItemsPerPage) )
		bIsAtBottom = 1;

	// Add on any text in the NewText string to the end of the scroll text.
	if( NewText != TEXT("") && ItemsPerPage > 0)
	{
		UFont *Font = Style->Fonts[MSAT_Blurry + (5*FontScale)]->eventGetFont(Canvas->SizeX);

		WrapURL(NewText);

		// Add new text to end of Content
		Content += (Separator + NewText);

		// Wrap just our new text, and add to array of strings.
		Canvas->WrapStringToArray(*NewText, &StringElements, cWidth, Font, **Separator);

		// If we have too much text, remove some.
		INT ExcessLines = StringElements.Num() - MaxHistory;
		if(MaxHistory > 0 && ExcessLines > 0)
		{
			INT RemoveChars = 0;
			for(INT i=0; i<ExcessLines; i++)
				RemoveChars += (StringElements(i).Len() + 1); // Add extra character in case there was a carriage return

			StringElements.Remove(0, ExcessLines);
			Content = Content.Mid(RemoveChars);
		}

		// Update text box length and viewing position
		ItemCount = StringElements.Num();

		// If we were at the bottom of the text, scroll down as it goes.
		if(bIsAtBottom)
		{
			Top = Max(ItemCount - ItemsPerPage + 1, 0);
			if (MyScrollBar && ItemCount>0)
				MyScrollBar->delegateAlignThumb();
		}

		// Empty 'NewText'
		NewText = TEXT("");
	}
	
	unguardobj;
}

void UGUIScrollText::Draw(UCanvas *Canvas)
{
	guard(UGUIScrollText::Draw);

	if ( !bVisible || (ItemCount == 0 && !bVisibleWhenEmpty) || INVALIDRENDER )
		return;

	if ( !bStopped || bReceivedNewContent )
	{
		// Find our best Top
		INT NewTop = Max(VisibleLines - ItemsPerPage + 1, 0);
		if (Top != NewTop)
		{
			Top = NewTop;
			if (MyScrollBar && StringElements.Num()>0)
				MyScrollBar->delegateAlignThumb();
		}

		INT     oldCnt = ItemCount;
		ItemCount = Clamp(VisibleLines+1, 0, StringElements.Num());
		Index = -1;

		Super::Draw(Canvas);

		ItemCount = oldCnt;
	}
	else
	{
		ItemCount = StringElements.Num();
		Index = -1;
		Super::Draw(Canvas);
	}


	unguardobj;
}

void UGUIScrollText::DrawItem(UCanvas* Canvas, INT Item, FLOAT X, FLOAT Y, FLOAT W, FLOAT HT, UBOOL bSelected, UBOOL bPending)
{
	guard(UGUIScrollText::DrawItem);

	if ( INVALIDRENDER )
		return;

	FLOAT XL, YL;
	check(Item >= Top && Item < StringElements.Num());

	if (!bStopped && Item == VisibleLines)
	{
		FString Elem(StringElements(Item).Left(VisibleChars));
		Style->DrawText(Canvas, MenuState, X,Y,W,HT, TextAlign, *Elem, FontScale);
		// Display a simple large caret
		Style->TextSize( Canvas, MenuState, *Elem, XL, YL, FontScale );
		switch(TextAlign)
		{
		case TXTA_Left:	X += XL;				break;
		case TXTA_Center: X += ((W + XL) / 2); 	break;
		case TXTA_Right: X += W;				break;
		}
		Canvas->Style=5;	// Alpha
		Canvas->DrawTile(Controller->DefaultPens[0], X, Y, YL/2,YL,0,0,1,1,0.0,FColor(255,255,255,Controller->FastCursorFade),FPlane(0,0,0,0));
	}
	else
		Style->DrawText(Canvas, MenuState, X,Y,W,HT, TextAlign, *(StringElements(Item)), FontScale);

	unguardobj;

}

// Find current word under cursor.
void UGUIScrollText::execGetWordUnderCursor( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIScrollText::execGetWordUnderCursor);

	P_FINISH;

	if ( Style == NULL || INVALIDMENU )
		return;

	FString& StringResult = *(FString*)Result;
	StringResult.Empty();

	if ( !WithinBounds(Controller->MouseX, Controller->MouseY) )
		return;

	if ( Style->Fonts[MSAT_Blurry + (5*FontScale)] == NULL )
		return;

	// First figure out the line
	INT ClickLineIx = eventCalculateIndex(1);

	// If line is outside range, do nothing.
	if ( !StringElements.IsValidIndex(ClickLineIx) )
		return;

	FString& ClickLine = StringElements(ClickLineIx);

	// Then find the word that we clicked on
	UFont *Font = Style->Fonts[MSAT_Blurry + (5*FontScale)]->eventGetFont( Controller->ResX );
	FLOAT currPos = ClientBounds[0]; // Starting from the left bound - work along until we pass our selection point.

	INT Width, Height;
	for ( INT i = 0; i < ClickLine.Len(); i++ )
	{
		TCHAR Cur = ClickLine[i];
		if ( Cur == 27 )
		{
			i += 3;
			continue;
		}

		else if ( Cur == 2 || Cur == 3 )
			continue;

		GetCharSize(Font, Cur, Width, Height);
		currPos += Width + Font->Kerning;

		if ( currPos > Controller->MouseX )
		{
			if ( Cur == ' ' )
				return;

			// Now crawl left and right to find ends of this word.
			INT startPos = i, endPos = i;
			while ( startPos > 0 && BYTE(ClickLine[startPos-1]) > 32 )
			{
				if ( startPos > 3 && ClickLine[startPos-4] == 0x1B )
					break;
				else
					startPos--;

			}

			while( endPos < ClickLine.Len()-1 && BYTE(ClickLine[endPos+1]) > 32 )
				endPos++;

			// Next strip any color codes
			while ( ClickLine[startPos] == 27 && startPos < endPos )
				startPos += 4;

			while ( endPos - 4 > startPos && ClickLine[endPos-3] == 27 )
				endPos -= 4;

			// STX indicates the beginning of a URL string - allow scanning of additonal lines if word wasn't completed
			if ( ClickLine[startPos] == 2 )
			{
				startPos++;

				// We found the URL ending...all is well
				if ( ClickLine[endPos] == 3 )
				{
					endPos--;
					StringResult = ClickLine.Mid(startPos, endPos - startPos + 1);
				}
				else
				{
					StringResult = ClickLine.Mid(startPos);

					// We'll need to grab the next line and append it to whatever we've got here
					if ( StringElements.IsValidIndex(ClickLineIx+1) )
					{
						FString& NextLine = StringElements(ClickLineIx+1);
						INT lastPos = NextLine.InStr(TEXT(""));
						if ( lastPos != INDEX_NONE )
						{
							// If we're in a URL, we're probably going to need to remove additional color codes from this line
							INT x=0;
							while ( NextLine[x] == 27 && x < lastPos )
								x += 4;

							StringResult += NextLine.Mid(x, lastPos - x);
						}

						// Otherwise we couldn't find it...oh well
					}
				}
			}
			else if ( ClickLine[endPos] == 3 )
			{
				endPos--;

				// We have a URL ending
				// We'll need to grab the previous line and prepend it to this string
				if ( StringElements.IsValidIndex(ClickLineIx-1) )
				{
					FString& PrevLine = StringElements(ClickLineIx-1);
					INT begPos = PrevLine.InStr(TEXT(""),1);
					if ( begPos != INDEX_NONE )
					{
						begPos++;

						// There should never be color codes after the STX char, but ya never know...
						while ( begPos < PrevLine.Len() && PrevLine[begPos] == 27 )
							begPos += 4;

						StringResult = PrevLine.Mid(begPos);
					}
				}

				StringResult += ClickLine.Mid(startPos,endPos-startPos+1);
			}

			else StringResult = ClickLine.Mid(startPos, endPos-startPos+1);
			return;
		}
	}


	unguardexec;
}

UBOOL UGUIScrollText::WrapURL( FString& Text ) const
{
	guard(UGUIScrollText::WrapURL);

	if ( Text.Len() == 0 )
		return 0;

	INT urlPos = Text.InStr(TEXT("http://"));
	if ( urlPos != INDEX_NONE )
	{
		FString Temp(Text);
		Text = Temp.Left(urlPos);
		Temp = Temp.Mid(urlPos);

		Text += TEXT(""); // STX (ANSI 002 - Start of Text)
		urlPos = Temp.InStr(TEXT(" "));
		INT colorPos = Temp.InStr(TEXT(""));
		if ( urlPos != INDEX_NONE )
		{
			if ( colorPos != INDEX_NONE && colorPos < urlPos )
				urlPos = colorPos;

			Text += Temp.Left(urlPos) + TEXT(""); // ETX (ANSI 003 - End of Text)
			Text += Temp.Mid(urlPos);
		}
		else
		{
			if ( colorPos != INDEX_NONE )
			{
				Text += Temp.Left(colorPos) + TEXT("");
				Text += Temp.Mid(colorPos);
			}
			else
				Text += Temp + TEXT(""); // ETX (ANSI 003 - End of Text)
		}

		return 1;
	}

	return 0;

	unguard;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUITreeListBox
// =======================================================================================================================================================
// =======================================================================================================================================================
void UGUITreeListBox::PreDraw(UCanvas* Canvas)
{
	guard(UGUITreeListBox::PreDraw);
	
	if ( !MyList || !MyScrollBar || INVALIDRENDER )
		return;

	UGUIComponent::PreDraw(Canvas);

	UGUITreeList* List = CastChecked<UGUITreeList>(MyList);
	List->UpdateVisibleCount();

	if (bVisible)
	{
		if (!bVisibleWhenEmpty)
		{
			if (List->bVisible != (List->ItemCount > 0) )
				List->eventSetVisibility(List->ItemCount > 0);
		}

		if ( bTravelling || bSizing )
			MyScrollBar->bVisible = 0;
		
		else if ( MyScrollBar->bVisible != (List->VisibleCount > 0 && List->VisibleCount > List->ItemsPerPage))
			MyScrollBar->eventSetVisibility(List->VisibleCount > 0 && List->VisibleCount > List->ItemsPerPage);
	}


	List->SetAdjustedDims( Max<FLOAT>(ActualWidth() - (MyScrollBar->bVisible ? MyScrollBar->ActualWidth() : 0), 2.f), ActualHeight(), ActualLeft(), ActualTop() );
	MyScrollBar->SetAdjustedDims( MyScrollBar->ActualWidth(), ActualHeight(), ActualLeft() + List->ActualWidth(), ActualTop() );

	PreDrawControls(Canvas);	
	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UGUITreeList
// =======================================================================================================================================================
// =======================================================================================================================================================

UBOOL FGUITreeNode::operator==( const FGUITreeNode& Other ) const
{
	guard(FGUITreeNode::operator==);

	return 
		Other.Caption       == Caption       &&
		Other.Value         == Value         &&
		Other.ParentCaption == ParentCaption &&
		Other.ExtraInfo     == ExtraInfo     &&
		Other.Level         == Level         &&
		Other.bEnabled      == bEnabled;

	unguard;
}


INT UGUITreeList::FindParentIndex( INT ChildIndex ) const
{
	guard(UGUITreeList::FindParentIndex);

	if ( !Elements.IsValidIndex(ChildIndex) )
		return INDEX_NONE;

	if ( Elements(ChildIndex).Level == 0 )
		return INDEX_NONE;

	INT Level = Elements(ChildIndex).Level - 1;
	while ( --ChildIndex >= 0 && Elements(ChildIndex).Level > Level );
	return ChildIndex;

	unguard;
}

static UGUITreeList* GTreeList, *STreeList;

static INT Compare( FGUITreeNode& A, FGUITreeNode& B )
{
// FIXME
	if(GTreeList)
		return GTreeList->delegateCompareItem(A, B);
	else
	{
		INT ChildA = STreeList->Elements.FindItemIndex(A);
		INT ChildB = STreeList->Elements.FindItemIndex(B);

		INT ParentA = STreeList->FindParentIndex(ChildA);
		INT ParentB = STreeList->FindParentIndex(ChildB);

		if ( A.Level == B.Level )
		{
			if ( ParentA != ParentB )
				return ParentA - ParentB;

			return appStricmp( *(A.Caption), *(B.Caption) );
		}

		else if ( A.Level > B.Level )
		{
			if (ParentA>=0)
				return Compare( STreeList->Elements(ParentA), B );
			else
				return -1;
		}

		else if ( B.Level > A.Level )
		{
			if (ParentB >=0)
				return Compare( STreeList->Elements(ParentB), A );
			else
				return -1;
		}

		return appStricmp( *(A.Caption), *(B.Caption) );

	}
}

void UGUITreeList::execSortList( FFrame& Stack, RESULT_DECL )
{
	guard(UGUIList::SortList);

	P_FINISH;
	return;

	// FIXME
	// GTreeList being set indicates we should use the script delegate to compare elements.
	// If no delegate is set - we sort alphabetically.
	if(DELEGATE_IS_SET(CompareItem))
		GTreeList=this;
	else
		GTreeList=NULL;

	STreeList = this;
	Sort( &Elements(0), Elements.Num() );
	STreeList = NULL;

	unguardexec;
}

void UGUITreeList::execUpdateVisibleCount( FFrame& Stack, RESULT_DECL )
{
	guard(UGUITreeList::execUpdateVisibleCount);

	P_FINISH;

	UpdateVisibleCount();
	unguardexec;
}

void UGUITreeList::UpdateVisibleCount()
{
	guard(UGUITreeList::UpdateVisibleCount);

	VisibleCount = 0;
	for ( INT i = 0; i < ItemCount; i++ )
	{
		if ( !Elements.IsValidIndex(i) )
			break;

		if ( Elements(i).bEnabled )
			VisibleCount++;
	}

	unguardobj;
}

void UGUITreeList::Draw(UCanvas* Canvas)
{
	guard(UGUITreeList::Draw);

	if ( !bVisible || (ItemCount == 0 && !bVisibleWhenEmpty) || INVALIDRENDER )
		return;

	// Always assign a temp UGUIController pointer in case script sets Controller to NULL
	UGUIController* C = Controller;


	// determine the sizes of the prefix width
	FLOAT dummy;
	Style->TextSize(Canvas, MenuState, TEXT("+"), PrefixWidth, dummy, FontScale);
	if ( SelectedStyle )
		SelectedStyle->TextSize(Canvas, MenuState, TEXT("+"), SelectedPrefixWidth, dummy, FontScale);
	else SelectedPrefixWidth = PrefixWidth;

	Style->Draw( Canvas, MenuState, Bounds[0], Bounds[1], Bounds[2] - Bounds[0], Bounds[3] - Bounds[1]);
	INT X = ClientBounds[0], Y = ClientBounds[1], XL = ClientBounds[2] - ClientBounds[0];

	if ( bHotTrack && C->HasMouseMoved() && C->ActiveControl == this && C->ActivePage == PageOwner )
	{
		if ( PerformHitTest(C->MouseX, C->MouseY) )
		{
			INT OldIndex = Index;
			Index = eventCalculateIndex(1);
			if ( Elements.IsValidIndex(Index) )
			{
				if (OldIndex != Index)
				{
					if ( bHotTrackSound )
						C->PlayComponentSound(CS_Hover);

					if ( DELEGATE_IS_SET(OnTrack) )
						delegateOnTrack(this, OldIndex);
				}
			}
			else Index = OldIndex;
		}
		else Index = INDEX_NONE;
	}


	for ( INT i = Top, cnt = 0; cnt < ItemsPerPage; i++ )
	{
		if ( !Elements.IsValidIndex(i) )
			break;

		if ( !Elements(i).bEnabled )
			continue;

		cnt++;
		UBOOL bIsPending = SelectedItems.FindItemIndex(i) != INDEX_NONE;
		UBOOL bIsSelected = i == Index;
		UBOOL bIsDrop = i == DropIndex;
		bIsPending = ((bDropSource || bDropTarget) && (bIsPending || bIsDrop));

		if ( DELEGATE_IS_SET(OnDrawItem) )
			delegateOnDrawItem( Canvas, i, X, Y, XL, ItemHeight, bIsSelected, bIsPending );
		else
		{
			if (bIsSelected || (bIsPending && !bIsDrop)) 
			{
				if (SelectedStyle!=NULL)
				{
					if (SelectedStyle->Images[MenuState]!=NULL)
						SelectedStyle->Draw(Canvas,MenuState,X,Y,XL,ItemHeight);
					else
						Canvas->DrawTile(C->DefaultPens[0], X, Y, XL,ItemHeight,0,0,32,32, 0.0f, SelectedStyle->FontBKColors[MenuState],FPlane(0,0,0,0));
				}
				else
				{
					// Display the selection

					if ( (MenuState==MSAT_Focused)  || (MenuState==MSAT_Pressed) )
					{
						if (SelectedImage==NULL)
							Canvas->DrawTile(C->DefaultPens[0], X, Y, XL,ItemHeight,0,0,32,32, 0.0f, SelectedBKColor,FPlane(0,0,0,0));
						else
						{
							Canvas->Color = SelectedBKColor;
							Canvas->DrawTileStretched(SelectedImage, X, Y, XL, ItemHeight);
						}
					}
				}
			}

			if (bIsPending && OutlineStyle)
			{
				if (OutlineStyle->Images[MenuState])
				{
					if ( bIsDrop )
						OutlineStyle->Draw( Canvas, MenuState, X + 1, Y + 1, XL - 2, ItemHeight - 2 );
					else
					{
						OutlineStyle->Draw(Canvas, MenuState, X, Y, XL, ItemHeight);
						if (DropState == DRP_Source)
							OutlineStyle->Draw(Canvas, MenuState, C->MouseX - MouseOffset[0], C->MouseY - MouseOffset[1] + Y - ClientBounds[1], MouseOffset[2] + MouseOffset[0], ItemHeight);
					}
				}
			}
			
			DrawItem( Canvas, i, X, Y, XL, ItemHeight, bIsSelected, bIsPending );
		}

		Y += ItemHeight;
	}

	unguardobj;
}

void UGUITreeList::DrawItem(UCanvas* Canvas, INT Item, FLOAT X, FLOAT Y, FLOAT W, FLOAT HT, UBOOL bSelected, UBOOL bPending)
{
	guard(UGUITreeList::DrawItem);

	if ( !bVisible || INVALIDRENDER )
		return;

	verify(Elements.IsValidIndex(Item));

	FGUITreeNode& Elem = Elements(Item);
	UGUIStyles* DrawStyle = NULL;
	FLOAT PWidth = 0.f;

	if ( bSelected )
	{
		DrawStyle = SelectedStyle;
		PWidth = SelectedPrefixWidth;
	}
	else
	{
		DrawStyle = Style;
		PWidth = PrefixWidth;
	}

	check(DrawStyle);

	FLOAT PrefixOffset(PWidth * Elem.Level), CaptionOffset(PWidth * (Elem.Level+1));

	TCHAR Prefix[2] = {0,0};
	if ( eventHasChildren(Item) )
	{
		if ( eventIsExpanded(Item) )
			Prefix[0] = '-';
		else Prefix[0] = '+';
	}

	DrawStyle->DrawText( Canvas, MenuState, X + PrefixOffset, Y, W - PrefixOffset, HT, TextAlign, Prefix, FontScale );
	DrawStyle->DrawText( Canvas, MenuState, X + CaptionOffset, Y, W - CaptionOffset, HT, TextAlign, *Elem.Caption, FontScale );

	unguardobj;
}

// =======================================================================================================================================================
// =======================================================================================================================================================
// UMultiSelectList
// =======================================================================================================================================================
// =======================================================================================================================================================
void UMultiSelectList::Draw(UCanvas* Canvas)
{
	guard(UMultiSelectList::Draw);

	if ( !bVisible || (ItemCount == 0 && !bVisibleWhenEmpty) || INVALIDRENDER )
		return;

	// Always assign a temp UGUIController pointer in case script sets Controller to NULL
	UGUIController* C = Controller;

	Style->Draw( Canvas, MenuState, Bounds[0], Bounds[1], Bounds[2] - Bounds[0], Bounds[3] - Bounds[1]);
	INT Y = ClientBounds[1];
	if ( bHotTrack && C->HasMouseMoved() && C->ActiveControl == this && C->ActivePage == PageOwner )
	{
		if ( PerformHitTest(C->MouseX, C->MouseY) )
		{
			INT OldIndex = Index;
			Index = Min( Top + appFloor( (C->MouseY - ClientBounds[1]) / ItemHeight), ItemCount - 1 );
			if (OldIndex != Index )
			{
				if ( bHotTrackSound )
					C->PlayComponentSound(CS_Hover);

				if ( DELEGATE_IS_SET(OnTrack) )
					delegateOnTrack(this,OldIndex);
			}
		}

		else Index = INDEX_NONE;
	}

	for (INT i=0;i<ItemsPerPage;i++)
	{
		if ( Top+i<ItemCount )	// Draw the text
		{
			// Check if this is the selected Index

			UBOOL bIsPending = SelectedItems.FindItemIndex(Top+i) != INDEX_NONE;

			//UBOOL bIsSelected = (Top + i) == Index;
			UBOOL bIsSelected = MElements(Top + i).bSelected;

			bIsPending = ((bDropSource || bDropTarget) && (bIsPending || ((Top + i) == DropIndex)));

			if( DELEGATE_IS_SET(OnDrawItem) )
			{
				delegateOnDrawItem( Canvas, Top+i, ClientBounds[0], Y, ClientBounds[2]-ClientBounds[0], ItemHeight, bIsSelected, bIsPending );
			}
			else
			{
				if (bIsSelected) 
				{
					if (SelectedStyle!=NULL)
					{
						if (SelectedStyle->Images[MenuState]!=NULL)
							SelectedStyle->Draw(Canvas,MenuState,ClientBounds[0],Y,ClientBounds[2] - ClientBounds[0],ItemHeight);
						else
							Canvas->DrawTile(C->DefaultPens[0], ClientBounds[0],Y, ClientBounds[2] - ClientBounds[0],ItemHeight,0,0,32,32, 0.0f, SelectedStyle->FontBKColors[MenuState],FPlane(0,0,0,0));
					}
					else
					{
						// Display the selection

						if ( (MenuState==MSAT_Focused)  || (MenuState==MSAT_Pressed) )
						{
							if (SelectedImage==NULL)
								Canvas->DrawTile(C->DefaultPens[0], ClientBounds[0],Y, ClientBounds[2] - ClientBounds[0],ItemHeight,0,0,32,32, 0.0f, SelectedBKColor,FPlane(0,0,0,0));
							else
							{
								Canvas->Color = SelectedBKColor;
								Canvas->DrawTileStretched(SelectedImage,ClientBounds[0],Y,ClientBounds[2] - ClientBounds[0],ItemHeight);
							}
						}
					}
				}

				if (bIsPending && OutlineStyle)
				{
					if (OutlineStyle->Images[MenuState])
					{
						OutlineStyle->Draw(Canvas, MenuState, ClientBounds[0], Y, ClientBounds[2] - ClientBounds[0], ItemHeight);
						if (DropState == DRP_Source)
							OutlineStyle->Draw(Canvas, MenuState, C->MouseX - MouseOffset[0], C->MouseY - MouseOffset[1] + Y - ClientBounds[1], MouseOffset[2] + MouseOffset[0], ItemHeight);
					}
				}

				DrawItem( Canvas, Top+i, ClientBounds[0], Y, ClientBounds[2]-ClientBounds[0], ItemHeight, bIsSelected, bIsPending);
			}
		}
		Y+=ItemHeight;
	}

	unguardobj;
}


void UMultiSelectList::DrawItem(UCanvas* Canvas, INT Item, FLOAT X, FLOAT Y, FLOAT WT, FLOAT HT, UBOOL bSelected, UBOOL bPending)
{
	guard(UMultiSelectList::DrawItem);

	if ( !bVisible || INVALIDRENDER )
		return;

	if (bSelected)
		SelectedStyle->DrawText(Canvas, MenuState, X,Y,WT,HT, TextAlign, *(MElements(Item).Item), FontScale);
	else
		Style->DrawText(Canvas, MenuState, X,Y,WT,HT, TextAlign, *(MElements(Item).Item), FontScale);
	
	unguardobj;
}

