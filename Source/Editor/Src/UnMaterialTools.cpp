/*=============================================================================
	UnMaterialTools.cpp: Tools for the Materials editor
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Jack Porter
=============================================================================*/

#include "EditorPrivate.h"

/*------------------------------------------------------------------------------
	UMaterialLayout
------------------------------------------------------------------------------*/

void UMaterialLayout::MousePosition( DWORD Buttons, FLOAT X, FLOAT Y )
{
	MouseX = X;
	MouseY = Y;
/*
	if( Buttons & MOUSE_Left )
	{
		debugf(TEXT("Move+left: %f, %f"), X, Y );
		for( TMap<UMaterial*,FMaterialLayoutItem>::TIterator It(LayoutItems); It; ++It )
		{
			FMaterialLayoutItem& LayoutItem = It.Value();
			if( LayoutItem.Selected )
			{
				LayoutItem.X = MouseX-LayoutItem.SelectOffsetX;
				LayoutItem.Y = MouseY-LayoutItem.SelectOffsetY;
			}
		}
	}
*/}

void UMaterialLayout::AddItem( UMaterial* InItem, FLOAT X, FLOAT Y )
{
	guard(UMaterialLayout::AddItem);
	TArray<FMaterialInputInfo> Inputs;

	if( LayoutItems.Find(InItem) == NULL )
	{		
		FMaterialLayoutItem& LayoutItem = LayoutItems.Set( InItem, FMaterialLayoutItem(X,Y) );
		LayoutItem.Selected = 0;
		
		InItem->GetInputs( Inputs );
		
		for( INT i=0;i<Inputs.Num();i++ )
		{
			if( Inputs(i).MaterialItemValue )
				AddItem( Inputs(i).MaterialItemValue, X+48*i, Y+48 );
		}
	}

	unguard;
}

void UMaterialLayout::ClickItem( UMaterial* Item )
{
	guard(UMaterialLayout::ClickItem);

	FMaterialLayoutItem* LayoutItem = LayoutItems.Find(Item);
	check(LayoutItem);

//!!MAT
//	ObjPropWindow->Root.SetObjects( (UObject**)&Item, 1 );
	
	LayoutItem->Selected = !LayoutItem->Selected;
	LayoutItem->SelectOffsetX = MouseX - LayoutItem->X;
	LayoutItem->SelectOffsetY = MouseY - LayoutItem->Y;
	for( TMap<UMaterial*,FMaterialLayoutItem>::TIterator It(LayoutItems); It; ++It )
	{
		FMaterialLayoutItem* L = &It.Value();
		if( L != LayoutItem )
			L->Selected = 0;
	}
	unguard;
}


void UMaterialLayout::DrawMaterialEditor( UViewport* Viewport )
{
	guard(UMaterialLayout::DrawMaterialEditor);

	FString ErrorString(TEXT(""));
	UMaterial* ErrorMaterial = NULL;

	// Check material for errors
	Viewport->RI->SetMaterial( BaseMaterial, &ErrorString, &ErrorMaterial );
	if( **ErrorString )
	{
		Viewport->Canvas->Color = FColor(255,255,255);
		Viewport->Canvas->SetClip( 0, Viewport->SizeY-20, Viewport->SizeX, 20 );
		Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0, TEXT("%s"), *ErrorString );
	}

	for( TMap<UMaterial*,FMaterialLayoutItem>::TIterator It(LayoutItems); It; ++It )
	{
		UMaterial* Item = It.Key();
		FMaterialLayoutItem& LayoutItem = It.Value();

		UMaterial*			Material		= Cast<UMaterial>(Item);
		UBitmapMaterial*	BitmapMaterial	= Cast<UBitmapMaterial>(Item);
		UMaterial* Icon = NULL;
		
		if( BitmapMaterial )
			Icon = BitmapMaterial;
		else
		if( Material )
			Icon = Material->EditorIcon;

		if( Icon )
		{
			PUSH_HIT(Viewport,HMaterialEdit( this, Item ));			
			INT w = Min<INT>(32, Icon->MaterialUSize());
			INT h = Min<INT>(32, Icon->MaterialVSize());

			if( Item == ErrorMaterial )
			{
				Viewport->Canvas->DrawPattern( GEditor->BadHighlight, LayoutItem.X, LayoutItem.Y, w+10, h+10,
					1.0, 0.0, 0.0, 0.0f, FPlane(1.,1.,1.,1), FPlane(0,0,0,0) );
			}
			else
			if( LayoutItem.Selected )
			{
				Viewport->Canvas->DrawPattern( GEditor->BkgndHi, LayoutItem.X, LayoutItem.Y, w+10, h+10,
					1.0, 0.0, 0.0, 0.0f, FPlane(1.,1.,1.,1), FPlane(0,0,0,0) );
			}
			Viewport->Canvas->DrawIcon( Icon, LayoutItem.X+5, LayoutItem.Y+5, w, h, 0.0, FPlane(1.,1.,1.,1.), FPlane(0,0,0,0) );
			POP_HIT(Viewport);
		}

	}
	


	unguard;
}
IMPLEMENT_CLASS(UMaterialLayout);

/*------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------*/

