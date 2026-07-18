/*=============================================================================
	Properties.cpp: property editing class implementation.
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Jack Porter
=============================================================================*/

#pragma warning( disable : 4201 )
#define STRICT
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#include "Engine.h"
#include "Window.h"

/*-----------------------------------------------------------------------------
	WPropertiesBase.
-----------------------------------------------------------------------------*/

FTreeItem* WPropertiesBase::GetListItem( INT i )
{
	guard(WPropertiesBase::GetListItem);
	FTreeItem* Result = (FTreeItem*)List.GetItemData(i);
	check(Result);
	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	FTreeItem
-----------------------------------------------------------------------------*/

HBRUSH FTreeItem::GetBackgroundBrush( UBOOL Selected )
{
	guard(FTreeItem::GetBackgroundBrush);
	return Selected ? hBrushCurrent : hBrushGrey197;//hBrushOffWhite;
	unguard;
}

COLORREF FTreeItem::GetTextColor( UBOOL Selected )
{
	guard(FTreeItem::GetTextColor);
	//return Selected ? RGB(255,255,255) : RGB(0,0,0);
	return RGB(0,0,0);
	unguard;
}

void FTreeItem::Serialize( FArchive& Ar )
{
	guard(FTreeItem::Serialize);
	//!!Super::Serialize( Ar );
	for( INT i=0; i<Children.Num(); i++ )
		Children(i)->Serialize( Ar );
	unguard;
}

INT FTreeItem::OnSetCursor()
{
	guard(FTreeItem::OnSetCursor);
	return 0;
	unguard;
}

void FTreeItem::EmptyChildren()
{
	guard(FTreeItem::EmptyChildren);
	for( INT i=0; i<Children.Num(); i++ )
		delete Children(i);
	Children.Empty();
	unguard;
}

FRect FTreeItem::GetRect()
{
	guard(FTreeItem::GetRect);
	return OwnerProperties->List.GetItemRect(OwnerProperties->List.FindItemChecked(this));
	unguard;
}

void FTreeItem::Redraw()
{
	guard(FTreeItem::Redraw);
	InvalidateRect( OwnerProperties->List, GetRect(), 0 );
	UpdateWindow( OwnerProperties->List );
	if( Parent!=OwnerProperties->GetRoot() )
		Parent->Redraw();
	unguard;
}

void FTreeItem::OnItemSetFocus()
{
	guard(FTreeItem::OnItemSetFocus);
	if( Parent && Parent!=OwnerProperties->GetRoot() )
		Parent->OnItemSetFocus();
	unguard;
}

void FTreeItem::OnItemKillFocus( UBOOL Abort )
{
	guard(FTreeItem::OnItemKillFocus);
	for( INT i=0; i<Buttons.Num(); i++ )
		delete Buttons(i);
	Buttons.Empty();
	ButtonWidth = 0;
	Redraw();
	if( Parent && Parent!=OwnerProperties->GetRoot() )
		Parent->OnItemKillFocus( Abort );
	unguard;
}

void FTreeItem::AddButton( const TCHAR* Text, FDelegate Action )
{
	guard(FTreeItem::AddButton);
	FRect Rect=GetRect();
	Rect.Max.X -= ButtonWidth;
	SIZE Size;
	HDC hDC = GetDC(*OwnerProperties);
	GetTextExtentPoint32X( hDC, Text, appStrlen(Text), &Size ); 
	ReleaseDC(*OwnerProperties,hDC);
	INT Width = Size.cx + 2;
	WCoolButton* Button = new WCoolButton(&OwnerProperties->List,0,Action);
	Buttons.AddItem( Button );
	Button->OpenWindow( 1, Rect.Max.X-Width, Rect.Min.Y, Width, Rect.Max.Y-Rect.Min.Y, Text );
	ButtonWidth += Width;
	unguard;
}

void FTreeItem::OnItemLeftMouseDown( FPoint P )
{
	guard(FTreeItem::OnItemLeftMouseDown);
	if( Expandable )
		ToggleExpansion();
	unguard;
}

void FTreeItem::OnItemRightMouseDown( FPoint P )
{
	guard(FTreeItem::OnItemRightMouseDown);
	unguard;
}

INT FTreeItem::GetIndent()
{
	guard(FTreeItem::GetIndent);
	INT Result=0;
	for( FTreeItem* Test=Parent; Test!=OwnerProperties->GetRoot(); Test=Test->Parent )
		Result++;
	return Result;
	unguard;
}

INT FTreeItem::GetUnitIndentPixels()
{
	return OwnerProperties->ShowTreeLines ? 12 : 8;
}

INT FTreeItem::GetIndentPixels( UBOOL Text )
{
	guard(FTreeItem::GetIndentPixels);
	return GetUnitIndentPixels()*GetIndent() + 
		(
			Text
			?
				OwnerProperties->ShowTreeLines 
				?
					20
				:
					16 
			:
				0
		);
	unguard;
}

FRect FTreeItem::GetExpanderRect()
{
	guard(FTreeItem::GetExpanderRect);
	return FRect( GetIndentPixels(0) + 4, 4, GetIndentPixels(0) + 13, 13 );
	unguard;
}

UBOOL FTreeItem::GetSelected()
{
	guard(FTreeItem::GetSelected);
	return Selected;
	unguard;
}

void FTreeItem::SetSelected( UBOOL InSelected )
{
	guard(FTreeItem::SetSelected);
	Selected = InSelected;
	unguard;
}

void FTreeItem::DrawTreeLines( HDC hDC, FRect Rect, UBOOL bTopLevelHeader )
{
	guard(FTreeItem::Draw);
	SetBkMode( hDC, TRANSPARENT );
	SetTextColor( hDC, RGB(0,0,0) );

	if( OwnerProperties->ShowTreeLines )
	{
		FTreeItem* Prev = this;
		for( INT i=GetIndent(); i>=0; i-- )
		{
			UBOOL bHeaderItem = (Prev->GetHeight() == 20);

			check(Prev->Parent);
			UBOOL FromAbove = (Prev->Parent->Children.Last()!=Prev || Prev->Parent->Children.Last()==this) && (i!=0 || Prev->Parent->Children(0)!=this);
			UBOOL ToBelow   = (Prev->Parent->Children.Last()!=Prev);
			FPoint P( Rect.Min.X + GetUnitIndentPixels()*i, Rect.Min.Y );
			if( !bHeaderItem )
			{
				if( FromAbove || ToBelow )
					FillRect( hDC, FRect(P+FPoint(8,FromAbove?0:8),P+FPoint(9,ToBelow?GetHeight():8)), hBrushDark );
				if( i==GetIndent() )
					FillRect( hDC, FRect(P+FPoint(8,8),P+FPoint(20,9)), hBrushDark );
			}
			Prev = Prev->Parent;
		}
	}
	if( Expandable )
	{
		FRect R = GetExpanderRect() + GetRect().Min;

		if( !bTopLevelHeader )
		{
			FillRect( hDC, R, hBrushDark );
			R = R + FRect(1,1,-1,-1);
			FillRect( hDC, R, GetBackgroundBrush(0) );
			R = R + FRect(1,1,-1,-1);
		}

		INT XMid = R.Min.X + (R.Width()/2), YMid = R.Min.Y + (R.Height()/2);
		FillRect( hDC, FRect( R.Min.X, YMid, R.Max.X, YMid+1 ), hBrushDark );
		if( !Expanded )
			FillRect( hDC, FRect( XMid, R.Min.Y, XMid+1, R.Max.Y), hBrushDark );
	}
	unguard;
}

void FTreeItem::Collapse()
{
	guard(FTreeItem::Collapse);
	OwnerProperties->SetItemFocus( 0 );
	INT Index = OwnerProperties->List.FindItemChecked( this );
	INT Count = OwnerProperties->List.GetCount();
	while( Index+1<Count )
	{
		FTreeItem* NextItem = OwnerProperties->GetListItem(Index+1);
		FTreeItem* Check;
		for( Check=NextItem->Parent; Check && Check!=this; Check=Check->Parent );
		if( !Check )
			break;
		NextItem->Expanded = 0;
		OwnerProperties->List.DeleteString( Index+1 );
		Count--;
	}
	Expanded=0;
	OwnerProperties->ResizeList();
	unguard;
}

void FTreeItem::Expand()
{
	guard(FTreeItem::Expand);
	if( Sorted )
	{
		Sort( &Children(0), Children.Num() );
	}
	if( this==OwnerProperties->GetRoot() )
	{
		for( INT i=0; i<Children.Num(); i++ )
			OwnerProperties->List.AddItem( Children(i) );
	}
	else
	{
		for( INT i=Children.Num()-1; i>=0; i-- )
			OwnerProperties->List.InsertItemAfter( this, Children(i) );
	}
	OwnerProperties->SetItemFocus( 0 );
	OwnerProperties->ResizeList();
	Expanded=1;
	unguard;
}

void FTreeItem::ToggleExpansion()
{
	guard(FTreeItem::ToggleExpansion);
	if( Expandable )
	{
		OwnerProperties->List.SetRedraw( 0 );
		if( Expanded )
			Collapse();
		else
			Expand();
		OwnerProperties->List.SetRedraw( 1 );
		UpdateWindow( OwnerProperties->List );
	}
	OwnerProperties->SetItemFocus( 1 );
	unguard;
}

void FTreeItem::OnItemDoubleClick()
{
	guard(FTreeItem::OnItemDoubleClick);
	//ToggleExpansion();
	unguard;
}

BYTE* FTreeItem::GetBase( BYTE* Base )
{
	return Parent ? Parent->GetContents(Base) : NULL;
}

BYTE* FTreeItem::GetContents( BYTE* Base )
{
	return GetBase(Base);
}

BYTE* FTreeItem::GetReadAddress( class FPropertyItem* Child, UBOOL RequireSingleSelection )
{
	guard(FTreeList::GetReadAddress);
	return Parent ? Parent->GetReadAddress(Child,RequireSingleSelection) : NULL;
	unguard;
}

void FTreeItem::NotifyChange()
{
	guard(FTreeList::NotifyChange);
	if( Parent )
		Parent->NotifyChange();
	unguard;
}

void FTreeItem::SetProperty( FPropertyItem* Child, const TCHAR* Value )
{
	guard(FTreeList::SetProperty);
	if( Parent )
		Parent->SetProperty( Child, Value );
	unguard;
}

void FTreeItem::GetStates( TArray<FName>& States )
{
	guard(FTreeList::GetStates);
	if( Parent ) Parent->GetStates( States );
	unguard;
}
UBOOL FTreeItem::AcceptFlags( DWORD InFlags )
{
	guard(FTreeList::AcceptFlags);
	return Parent ? Parent->AcceptFlags( InFlags ) : 0;
	unguard;
}

void FTreeItem::OnItemHelp()
{
}

void FTreeItem::SetFocusToItem()
{
}

void FTreeItem::SetValue( const TCHAR* Value )
{
}

void FTreeItem::SnoopChar( WWindow* Src, INT Char )
{
	guard(FTreeItem::SnoopChar);
	if( Char==13 && Expandable )
		ToggleExpansion();
	unguard;
}

void FTreeItem::SnoopKeyDown( WWindow* Src, INT Char )
{
	guard(FTreeItem::SnoopKeyDown);
	if( Char==VK_UP || Char==VK_DOWN )
		PostMessageX( OwnerProperties->List, WM_KEYDOWN, Char, 0 );
	if( Char==9 )
		PostMessageX( OwnerProperties->List, WM_KEYDOWN, (GetKeyState(16)&0x8000)?VK_UP:VK_DOWN, 0 );
	unguard;
}

UObject* FTreeItem::GetParentObject()
{
	guard(FTreeItem::GetParentObject);
	return Parent->GetParentObject();
	unguard;
}

/*-----------------------------------------------------------------------------
	FPropertyItem
-----------------------------------------------------------------------------*/

// QSort comparator.
static INT Compare( const class FTreeItem* T1, const class FTreeItem* T2 )
{
	return appStricmp( *T1->GetCaption(), *T2->GetCaption() );
}

void FPropertyItem::Serialize( FArchive& Ar )
{
	guard(FPropertyItem::Serialize);
	FTreeItem::Serialize( Ar );
	Ar << Property << Name;
	unguard;
}

QWORD FPropertyItem::GetId() const
{
	guard(FPropertyItem::GetId);
	return Name.GetIndex() + ((QWORD)1<<32);
	unguard;
}

FString FPropertyItem::GetCaption() const
{
	guard(FPropertyItem::GetCaption);
	return *Name;
	unguard;
}

INT FPropertyItem::OnSetCursor()
{
	guard(FPropertyItem::OnSetCursor);
	FPoint P = OwnerProperties->GetCursorPos() - GetRect().Min;
	if( Abs(P.X-OwnerProperties->GetDividerWidth())<=2 )
	{
		SetCursor(LoadCursorIdX(hInstanceWindow,IDC_SplitWE));
		return 1;
	}
	return 0;
	unguard;
}

void FPropertyItem::OnItemLeftMouseDown( FPoint P )
{
	guard(FPropertyItem::OnItemLeftMouseDown);
	P = OwnerProperties->GetCursorPos() - GetRect().Min;
	if( Abs(P.X-OwnerProperties->GetDividerWidth())<=2 )
	{
		OwnerProperties->BeginSplitterDrag();
		throw TEXT("NoRoute");
	}
	else FTreeItem::OnItemLeftMouseDown( P );
	unguard;
}

BYTE* FPropertyItem::GetBase( BYTE* Base )
{
	guard(FPropertyItem::GetBase);
	return Parent->GetContents(Base) + _Offset;
	unguard;
}

BYTE* FPropertyItem::GetContents( BYTE* Base )
{
	Base = GetBase(Base);
	UArrayProperty* ArrayProperty;
	if( (ArrayProperty=Cast<UArrayProperty>(Property))!=NULL )
		Base = (BYTE*)((FArray*)Base)->GetData();
	return Base;
}

void FPropertyItem::GetPropertyText( TCHAR* Str )
{
	guard(FPropertyItem::GetPropertyText);
	if( Cast<UClassProperty>(Property) && appStricmp(*Property->Category,TEXT("Drivers"))==0 )
	{
		// Class config.
		FString Path, Left, Right;
		GConfig->GetString( Property->GetOwnerClass()->GetPathName(), Property->GetName(), Path );
		if( Path.Split(TEXT("."),&Left,&Right) )
			appStrcpy( Str, Localize(*Right,TEXT("ClassCaption"),*Left) );
		else
			appStrcpy( Str, Localize("Language","Language",TEXT("Core"),*Path) );
	}
	else
	{
		// Regular property.
		Property->ExportText( 0, Str, GetReadAddress(this)-Property->Offset, GetReadAddress(this)-Property->Offset, PPF_Localized );
	}
	unguard;
}

void FPropertyItem::SetValue( const TCHAR* Value )
{
	guard(FPropertyItem::SetValue);
	SetProperty( this, Value );
	ReceiveFromControl();
	Redraw();
	unguard;
}


void FPropertyItem::Draw( HDC hDC )
{
	guard(FPropertyItem::Draw);
	FRect Rect = GetRect();
	TCHAR Str[4096];//!!

	// Draw background.
	FillRect( hDC, Rect, hBrushGrey160 ); 

	// Draw left background.
	FRect LeftRect=Rect;
	LeftRect.Max.X = OwnerProperties->GetDividerWidth();
	FillRect( hDC, LeftRect+FRect(0,1-GetSelected(),-1,0), GetBackgroundBrush(GetSelected()) );
	LeftRect.Min.X += GetIndentPixels(1);

	// Draw tree lines.
	DrawTreeLines( hDC, Rect, 0 );

	// Setup text.
	SetBkMode( hDC, TRANSPARENT );

	// Draw left text.
	if( ArrayIndex==-1 ) appStrcpy( Str, *Name );
	else appSprintf( Str, TEXT("[%i]"), ArrayIndex );
	SetTextColor( hDC, GetTextColor(GetSelected()) );
	DrawTextExX( hDC, Str, appStrlen(Str), LeftRect + FRect(0,1,0,0), DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );
	if( GetSelected() )	// cheap bold effect
		DrawTextExX( hDC, Str, appStrlen(Str), LeftRect + FRect(1,1,0,0), DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );

	// Draw right background.
	FRect RightRect = Rect;
	RightRect.Min.X = OwnerProperties->GetDividerWidth();
	FillRect( hDC, RightRect+FRect(0,1,0,0), GetBackgroundBrush(0) );

	// Draw right text.
	RightRect.Max.X -= ButtonWidth;
	BYTE* ReadValue = GetReadAddress( this );
	SetTextColor( hDC, GetTextColor(0) );
	if( (Property->ArrayDim!=1 && ArrayIndex==-1) || Cast<UArrayProperty>(Property) )
	{
		// Array expander.
		TCHAR* Str=TEXT("...");
		DrawTextExX( hDC, Str, appStrlen(Str), RightRect + FRect(4,0,0,1), DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );
	}
	// gam ---
	else if( Property->IsA( UStructProperty::StaticClass() ) && ((UStructProperty*) Property)->Struct->StructFlags & STRUCT_Long )
	{
		TCHAR* Str=TEXT("...");
		DrawTextExX( hDC, Str, appStrlen(Str), RightRect + FRect(4,0,0,1), DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );
	}
	// --- gam
	else if( ReadValue && Cast<UStructProperty>(Property) && Cast<UStructProperty>(Property)->Struct->GetFName()==NAME_Color )
	{
		// Color.
		FillRect( hDC, RightRect + FRect(4,4,-4,-4), hBrushBlack ); 
		DWORD D = (*(DWORD*)ReadValue); //!! 
		DWORD TempColor = ((D&0xff)<<16) + (D&0xff00) + ((D&0xff0000)>>16);
		HBRUSH hBrush = CreateSolidBrush(COLORREF(TempColor));
		FillRect( hDC, RightRect + FRect(5,5,-5,-5), hBrush ); 
		DeleteObject( hBrush );
	}
	else if( ReadValue )
	{
		// Text.
		*Str=0;
		GetPropertyText( Str );
		if(  Cast<UFloatProperty>(Property) || Cast<UIntProperty>(Property) || 
			(Cast<UByteProperty>(Property) && !Cast<UByteProperty>(Property)->Enum) )
		{
			if( !Equation.Len() )
				Equation = Str;
			appStrcpy( Str, *Equation );
			DrawTextExX( hDC, Str, appStrlen(Str), RightRect + FRect(4,1,0,0), DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );
		}
		else
			DrawTextExX( hDC, Str, appStrlen(Str), RightRect + FRect(4,1,0,0), DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );
	}

 	unguard;
}

INT FPropertyItem::GetHeight()
{
	guard(FPropertyItem::GetHeight);
	return 16;
	unguard;
}

void FPropertyItem::SetFocusToItem()
{
	guard(FPropertyItem::SetFocusToItem);
	if( EditControl )
		SetFocus( *EditControl );
	else if( TrackControl )
		SetFocus( *TrackControl );
	else if( ComboControl )
		SetFocus( *ComboControl );
	unguard;
}

void FPropertyItem::OnItemDoubleClick()
{
	guard(FPropertyItem::OnItemDoubleClick);
	Advance();
	FTreeItem::OnItemDoubleClick();
	unguard;
}

void FPropertyItem::OnItemSetFocus()
{
	guard(FPropertyItem::OnItemSetFocus);
	check(!EditControl);
	check(!TrackControl);
	FTreeItem::OnItemSetFocus();
	if
	(	(Property->ArrayDim==1 || ArrayIndex!=-1)
	&&	!(Property->PropertyFlags & CPF_EditConst) )
	{
		UObjectProperty* ObjProperty = Cast<UObjectProperty>(Property);
		if( Property->IsA(UByteProperty::StaticClass()) && !Cast<UByteProperty>(Property)->Enum )
		{
			// Slider.
			FRect Rect = GetRect();
			Rect.Min.X = 28+OwnerProperties->GetDividerWidth();
			Rect.Min.Y--;
			TrackControl = new WTrackBar( &OwnerProperties->List );
			TrackControl->ThumbTrackDelegate    = FDelegate(this,(TDelegate)&FPropertyItem::OnTrackBarThumbTrack);
			TrackControl->ThumbPositionDelegate = FDelegate(this,(TDelegate)&FPropertyItem::OnTrackBarThumbPosition);
			TrackControl->OpenWindow( 0 );
			TrackControl->SetTicFreq( 32 );
			TrackControl->SetRange( 0, 255 );
			TrackControl->MoveWindow( Rect, 0 );
		}
		if
		(	(Property->IsA(UBoolProperty::StaticClass()))
		||	(Property->IsA(UByteProperty::StaticClass()) && Cast<UByteProperty>(Property)->Enum)
		||	(Property->IsA(UNameProperty::StaticClass()) && Name==NAME_InitialState)
		||  (Cast<UClassProperty>(Property))
		||  (ObjProperty && !GIsEditor) )
		{
			// Combo box.
			FRect Rect = GetRect() + FRect(0,0,-1,-1);
			Rect.Min.X = OwnerProperties->GetDividerWidth();

			HolderControl = new WLabel( &OwnerProperties->List );
			HolderControl->Snoop = this;
			HolderControl->OpenWindow( 0 );
			FRect HolderRect = Rect.Right(20) + FRect(0,0,0,1);
			HolderControl->MoveWindow( HolderRect, 0 );

			Rect = Rect + FRect(-2,-6,-2,0);

			ComboControl = new WComboBox( HolderControl );
			ComboControl->Snoop = this;
			ComboControl->SelectionEndOkDelegate     = FDelegate(this,(TDelegate)&FPropertyItem::ComboSelectionEndOk);
			ComboControl->SelectionEndCancelDelegate = FDelegate(this,(TDelegate)&FPropertyItem::ComboSelectionEndCancel);
			ComboControl->OpenWindow( 0, ObjProperty || Cast<UClassProperty>(Property) ? 1 : 0 );
			ComboControl->MoveWindow( Rect-HolderRect.Min, 1 );

			if( Property->IsA(UBoolProperty::StaticClass()) )
			{
				ComboControl->AddString( GFalse );
				ComboControl->AddString( GTrue );
			}
			else if( Property->IsA(UByteProperty::StaticClass()) )
			{
				for( INT i=0; i<Cast<UByteProperty>(Property)->Enum->Names.Num(); i++ )
					ComboControl->AddString( *Cast<UByteProperty>(Property)->Enum->Names(i) );
			}
			else if( Property->IsA(UNameProperty::StaticClass()) && Name==NAME_InitialState )
			{
				TArray<FName> States;
				GetStates( States );
				ComboControl->AddString( *FName(NAME_None) );
				for( INT i=0; i<States.Num(); i++ )
					ComboControl->AddString( *States(i) );
			}
			else if( Cast<UClassProperty>(Property) )			
			{
				UClassProperty* ClassProp = CastChecked<UClassProperty>(Property);

				if( appStricmp(*Property->Category,TEXT("Drivers"))==0 ) 
				{
					TArray<FRegistryObjectInfo> Classes;
					UObject::GetRegistryObjects( Classes, UClass::StaticClass(), ClassProp->MetaClass, 0 );
					for( INT i=0; i<Classes.Num(); i++ )
					{
						FString Path=Classes(i).Object, Left, Right;
						if( Path.Split(TEXT("."),&Left,&Right) )
							ComboControl->AddString( Localize(*Right,TEXT("ClassCaption"),*Left) );
						else
							ComboControl->AddString( Localize("Language","Language",TEXT("Core"),*Path) );
					}
				}
				else
				{
					ComboControl->AddString( TEXT("None") );
					for( TObjectIterator<UClass> It; It; ++It )
						if( It->IsChildOf(ClassProp->MetaClass) && !(It->ClassFlags & CLASS_HideDropDown)  )
							ComboControl->AddString( It->GetName() );
				}
				goto SkipTheRest;
			}
			else if ( ObjProperty )
			{
				ComboControl->AddString( TEXT("None") );
				for( TObjectIterator<UObject> It; It; ++It )
					if( It->IsA(ObjProperty->PropertyClass) && !(It->GetClass()->ClassFlags & CLASS_HideDropDown)  )
						ComboControl->AddString( It->GetName() );

				if ( ObjProperty->PropertyClass->ClassFlags & CLASS_AutoInstancedProps )
				{
					AddButton( TEXT("Export"), FDelegate(this,(TDelegate)&FPropertyItem::OnBrowseButton) );
					AddButton( TEXT("Init"), FDelegate(this,(TDelegate)&FPropertyItem::OnUseCurrentButton) );
				}

				HolderRect.Min.X -= ButtonWidth;
				HolderRect.Max.X -= ButtonWidth;
				Rect.Max.X -= ButtonWidth;

				HolderControl->MoveWindow( HolderRect, 1 );
				ComboControl->MoveWindow(Rect-HolderRect.Min, 1);
				goto SkipTheRest;
			}
		}
		if( Property->IsA(UArrayProperty::StaticClass()) )
		{
			if( SingleSelect && !(Property->PropertyFlags&CPF_EditConstArray) )
			{
				AddButton( LocalizeGeneral("AddButton",TEXT("Window")), FDelegate(this,(TDelegate)&FPropertyItem::OnArrayAdd) );
				AddButton( LocalizeGeneral("EmptyButton",TEXT("Window")), FDelegate(this,(TDelegate)&FPropertyItem::OnArrayEmpty) );
			}
		}
		if( Cast<UArrayProperty>(Property->GetOuter()) )
		{
			if( SingleSelect && !(Cast<UArrayProperty>(Property->GetOuter())->PropertyFlags&CPF_EditConstArray) )
			{
				AddButton( LocalizeGeneral("InsertButton",TEXT("Window")), FDelegate(this,(TDelegate)&FPropertyItem::OnArrayInsert) );
				AddButton( LocalizeGeneral("DeleteButton",TEXT("Window")), FDelegate(this,(TDelegate)&FPropertyItem::OnArrayDelete) );
			}
		}
		if( Property->IsA(UStructProperty::StaticClass()) && appStricmp(Cast<UStructProperty>(Property)->Struct->GetName(),TEXT("Color"))==0 )
		{
			// Color.
			AddButton( LocalizeGeneral("BrowseButton",TEXT("Window")), FDelegate(this,(TDelegate)&FPropertyItem::OnChooseColorButton) );
			AddButton( LocalizeGeneral("PickButton",TEXT("Window")), FDelegate(this,(TDelegate)&FPropertyItem::OnPickColorButton) );

		}
		if( Property->IsA(UObjectProperty::StaticClass()) )
		{
			if( EdFindable )
			{
				AddButton( LocalizeGeneral("ClearButton", TEXT("Window")), FDelegate(this,(TDelegate)&FPropertyItem::OnClearButton) );
				AddButton( LocalizeGeneral("FindButton",TEXT("Window")), FDelegate(this,(TDelegate)&FPropertyItem::OnFindButton) );
				AddButton( LocalizeGeneral("UseButton",   TEXT("Window")), FDelegate(this,(TDelegate)&FPropertyItem::OnUseCurrentButton) );
			}
			else
			{
				AddButton( LocalizeGeneral("BrowseButton",TEXT("Window")), FDelegate(this,(TDelegate)&FPropertyItem::OnBrowseButton) );
				if( !EditInline || EditInlineUse )
					AddButton( LocalizeGeneral("UseButton",   TEXT("Window")), FDelegate(this,(TDelegate)&FPropertyItem::OnUseCurrentButton) );
				AddButton( LocalizeGeneral("ClearButton", TEXT("Window")), FDelegate(this,(TDelegate)&FPropertyItem::OnClearButton) );
			}
		}
		if
		(	(Property->IsA(UFloatProperty ::StaticClass()))
		||	(Property->IsA(UIntProperty   ::StaticClass()))
		||	(Property->IsA(UNameProperty  ::StaticClass()) && Name!=NAME_InitialState)
		||	(Property->IsA(UStrProperty   ::StaticClass()))
		||	(Property->IsA(UObjectProperty::StaticClass()))
		||	(Property->IsA(UByteProperty  ::StaticClass()) && Cast<UByteProperty>(Property)->Enum==NULL) )
		{
			// Edit control.
			FRect Rect = GetRect();
			Rect.Min.X = 1+OwnerProperties->GetDividerWidth();
			Rect.Min.Y--;
			if( Property->IsA(UByteProperty::StaticClass()) )
				Rect.Max.X = Rect.Min.X + 28;
			else
				Rect.Max.X -= ButtonWidth;
			EditControl = new WEdit( &OwnerProperties->List );
			EditControl->Snoop = this;
			EditControl->OpenWindow( 0, 0, 0 );
			EditControl->MoveWindow( Rect+FRect(0,1,0,1), 0 );
		}
		SkipTheRest:
		ReceiveFromControl();
		Redraw();
		if( EditControl )
			EditControl->Show(1);
		if( TrackControl )
			TrackControl->Show(1);
		if( ComboControl )
			ComboControl->Show(1);
		if( HolderControl )
			HolderControl->Show(1);
		SetFocusToItem();
	}
	unguard;
}

void FPropertyItem::OnItemKillFocus( UBOOL Abort )
{
	guard(FPropertyItem::OnKillFocus);
	if( !Abort )
		SendToControl();
	if( EditControl )
		delete EditControl;
	EditControl=NULL;
	if( TrackControl )
		delete TrackControl;
	TrackControl=NULL;
	if( ComboControl )
		delete ComboControl;
	ComboControl=NULL;
	if( HolderControl )
		delete HolderControl;
	HolderControl=NULL;
	FTreeItem::OnItemKillFocus( Abort );
	unguard;
}

void FPropertyItem::Collapse()
{
	guard(WPropertyItem::Collapse);
	FTreeItem::Collapse();
	EmptyChildren();
	unguard;
}

// FControlSnoop interface.
void FPropertyItem::SnoopChar( WWindow* Src, INT Char )
{
	guard(FPropertyItem::SnoopChar);
	if( Char==13 )
	{
		if( EditInline )
		{
			FTreeItem::SnoopKeyDown( Src, 9 );
			return;
		}			
		Advance();
	}
	else if( Char==27 )
		ReceiveFromControl();
	FTreeItem::SnoopChar( Src, Char );
	unguard;
}
void FPropertyItem::ComboSelectionEndCancel()
{
	guard(FPropertyItem::ComboSelectionEndCancel);
	ReceiveFromControl();
	unguard;
}

void FPropertyItem::ComboSelectionEndOk()
{
	guard(FPropertyItem::ComboSelectionEndOk);
	ComboChanged=1;
	SendToControl();
	ReceiveFromControl();
	Redraw();
	unguard;
}

void FPropertyItem::OnTrackBarThumbTrack()
{
	guard(FPropertyItem::OnTrackBarThumbTrack);
	if( TrackControl && EditControl )
	{
		TCHAR Tmp[256];
		appSprintf( Tmp, TEXT("%i"), TrackControl->GetPos() );
		EditControl->SetText( Tmp );
		EditControl->SetModify( 1 );
		UpdateWindow( *EditControl );
	}
	unguard;
}

void FPropertyItem::OnTrackBarThumbPosition()
{
	guard(FPropertyItem::OnTrackBarThumbPosition);
	OnTrackBarThumbTrack();
	SendToControl();
	if( EditControl )
	{
		SetFocus( *EditControl );
		EditControl->SetSelection( 0, EditControl->GetText().Len() );
		Redraw();
	}
	unguard;
}

void FPropertyItem::OnChooseColorButton()
{
	guard(FPropertyItem::OnChooseColorButton);
	union _FColor {
		struct{BYTE B,G,R,A;}; 
		DWORD DWColor;
	} ColorValue;
	DWORD* ReadValue = (DWORD*)GetReadAddress( this );
	ColorValue.DWColor = ReadValue ? *ReadValue : 0;

	CHOOSECOLORA cc;
	static COLORREF acrCustClr[16];
	appMemzero( &cc, sizeof(cc) );
	cc.lStructSize  = sizeof(cc);
	cc.hwndOwner    = OwnerProperties->List;
	cc.lpCustColors = (LPDWORD)acrCustClr;
	cc.rgbResult    = RGB(ColorValue.R, ColorValue.G, ColorValue.B);
	cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
	if( ChooseColorA(&cc)==TRUE )
	{
		TCHAR Str[256];
		appSprintf( Str, TEXT("(R=%i,G=%i,B=%i)"), GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult) );
		SetValue( Str );
		InvalidateRect( OwnerProperties->List, NULL, 0 );
		UpdateWindow( OwnerProperties->List );
	}
	Collapse();
	Redraw();
	unguard;
}

void FPropertyItem::OnArrayAdd()
{
	guard(FPropertyItem::OnArrayAdd);
	// Only works with single selection.
	BYTE* Addr = GetReadAddress( this, 1 );
	if( Addr )
	{
		UArrayProperty* Array = CastChecked<UArrayProperty>( Property );
		Collapse();
		((FArray*)Addr)->AddZeroed( Array->Inner->ElementSize, 1 );
		Expand();
		NotifyChange();

		// Activate the newly added item
		OwnerProperties->List.SetCurrent( OwnerProperties->List.FindItem(this) + ((FArray*)Addr)->Num() );
		if( Children.Num() && Children(Children.Num()-1)->Expandable )
			Children(Children.Num()-1)->Expand();
		OwnerProperties->SetItemFocus(1);
	}
	unguard;
}

void FPropertyItem::OnArrayEmpty()
{
	guard(FPropertyItem::OnArrayEmpty);
	// Only works with single selection.
	BYTE* Addr = GetReadAddress( this, 1 );
	if( Addr )
	{
		UArrayProperty* Array = CastChecked<UArrayProperty>( Property );
		Collapse();
		((FArray*)Addr)->Empty( Array->Inner->ElementSize );
		Expand();
		NotifyChange();
	}
	unguard;
}

void FPropertyItem::OnArrayInsert()
{
	guard(FPropertyItem::OnArrayInsert);
	FArray* Addr  = (FArray*)GetReadAddress((FPropertyItem*)Parent, 1);
	if( Addr )
	{
		Addr->Insert( ArrayIndex, 1, Property->GetSize() );
		appMemzero( (BYTE*)Addr->GetData() + ArrayIndex*Property->GetSize(), Property->GetSize() );
		FTreeItem* P = Parent;
		P->Collapse();
		P->Expand();
		P->NotifyChange();
	}
	unguard;
}

void FPropertyItem::OnArrayDelete()
{
	guard(FPropertyItem::OnArrayDelete);
	FArray* Addr  = (FArray*)GetReadAddress((FPropertyItem*)Parent, 1);
	if( Addr )
	{
		// 'this' gets deleted by Collapse() so we have to remember these values
		FTreeItem* LocalParent		= Parent;
		UProperty* LocalProperty	= Property;
		INT LocalArrayIndex			= ArrayIndex;
		LocalParent->Collapse();			
		Addr->Remove( LocalArrayIndex, 1, LocalProperty->GetSize() );
		LocalParent->Expand();
		LocalParent->NotifyChange();
	}
	unguard;
}

void FPropertyItem::OnBrowseButton()
{
	guard(FPropertyItem::OnBrowseButton);
	UObjectProperty* ReferenceProperty = CastChecked<UObjectProperty>(Property);
	TCHAR Temp[256];
	appSprintf( Temp, TEXT("BROWSECLASS CLASS=%s"), ReferenceProperty->PropertyClass ? ReferenceProperty->PropertyClass->GetName() : TEXT("Texture") );
	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyExec( OwnerProperties, Temp );
	Redraw();
	unguard;
}

void FPropertyItem::OnUseCurrentButton()
{
	guard(FPropertyItem::OnUseCurrentButton);
	UObjectProperty* ReferenceProperty = CastChecked<UObjectProperty>(Property);
	TCHAR Temp[256];
	appSprintf( Temp, TEXT("USECURRENT CLASS=%s"), ReferenceProperty->PropertyClass ? ReferenceProperty->PropertyClass->GetName() : TEXT("Texture") );
	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyExec( OwnerProperties, Temp );
	Redraw();
	if( EditInline )
	{
		Collapse();
		Expand();
		if(Children.Num())
			Children(0)->Expand();
	}
	unguard;
}

void FPropertyItem::OnPickColorButton()
{
	guard(FPropertyItem::OnPickColorButton);
	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyExec( OwnerProperties, TEXT("USECOLOR") );
	Collapse();
	Redraw();
	unguard;
}

void FPropertyItem::OnFindButton()
{
	guard(FPropertyItem::OnFindButton);
	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyExec( OwnerProperties, TEXT("FINDACTOR") );
	Collapse();
	Redraw();
	unguard;
}

void FPropertyItem::OnClearButton()
{
	guard(FPropertyItem::OnClearButton);
	SetValue( TEXT("None") );
	Redraw();
	if( EditInline )
	{
		Collapse();
		Expand();
	}
	unguard;
}

void FPropertyItem::Advance()
{
	guard(FPropertyItem::Advance);
	if( ComboControl && ComboControl->GetCurrent()>=0 )
	{
		ComboControl->SetCurrent( (ComboControl->GetCurrent()+1) % ComboControl->GetCount() );
		ComboChanged=1;
	}
	SendToControl();
	ReceiveFromControl();
	Redraw();
	unguard;
}

void FPropertyItem::SendToControl()
{
	guard(FPropertyItem::SendToControl);
	if( EditControl )
	{
		if( EditControl->GetModify() )
		{
			SetValue( *EditControl->GetText() );
			if( EditInline )
				Collapse();
		}
	}
	else if( ComboControl )
	{
		if( ComboChanged )
			SetValue( *ComboControl->GetString(ComboControl->GetCurrent()) );
		ComboChanged=0;
	}
	unguard;
}

void FPropertyItem::ReceiveFromControl()
{
	guard(FPropertyItem::ReceiveFromControl);
	ComboChanged=0;
	BYTE* ReadValue = GetReadAddress( this );
	if( EditControl )
	{
		TCHAR Str[4096]=TEXT("");
		if( ReadValue )
			GetPropertyText( Str );

		if(  Cast<UFloatProperty>(Property) || Cast<UIntProperty>(Property) || 
			(Cast<UByteProperty>(Property) && !Cast<UByteProperty>(Property)->Enum) )
		{
			EditControl->SetText( *Equation );
		}
		else
			EditControl->SetText( Str );

		EditControl->SetSelection( 0, appStrlen(Str) );
	}
	if( TrackControl )
	{
		if( ReadValue )
			TrackControl->SetPos( *(BYTE*)ReadValue );
	}
	if( ComboControl )
	{
		UBoolProperty* BoolProperty;
		if( (BoolProperty=Cast<UBoolProperty>(Property))!=NULL )
		{
			ComboControl->SetCurrent( ReadValue ? (*(BITFIELD*)ReadValue&BoolProperty->BitMask)!=0 : -1 );
		}
		else if( Property->IsA(UByteProperty::StaticClass()) )
		{
			ComboControl->SetCurrent( ReadValue ? *(BYTE*)ReadValue : -1 );
		}
		else if( Property->IsA(UNameProperty::StaticClass()) && Name==NAME_InitialState )
		{
			INT Index=ReadValue ? ComboControl->FindString( **(FName*)ReadValue ) : 0;
			ComboControl->SetCurrent( Index>=0 ? Index : 0 );
		}
		ComboChanged=0;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	FHeaderItem
-----------------------------------------------------------------------------*/

void FHeaderItem::Draw( HDC hDC )
{
	guard(FHeaderItem::Draw);
	FRect Rect = GetRect();

	UBOOL bTopLevelHeader = !GetIndent();

	// Draw background.
	FillRect( hDC, Rect, GetBackgroundBrush(0) ); 
	Rect = Rect + FRect(1+GetIndentPixels(0),1,-1,-1);
	FillRect( hDC, Rect, bTopLevelHeader ? hBrushBlack : hBrushGrey160 ); 
	Rect = Rect + FRect(1,1,-1,-1);
	FillRect( hDC, Rect, bTopLevelHeader ? hBrushGrey180 : hBrushGrey );
	Rect = Rect + FRect(1,1,-1,-1);

	DrawTreeLines( hDC, GetRect(), bTopLevelHeader );

	// Prep text.
	SetTextColor( hDC, GetTextColor(GetSelected()) );
	SetBkMode( hDC, TRANSPARENT );

	// Draw name.
	FString C = GetCaption();

	Rect = Rect + FRect(18,0,-18,0);
	if( !appStricmp( UObject::GetLanguage(), TEXT("kot") ) )
		TextOutW( hDC, Rect.Min.X, Rect.Min.Y,  const_cast<TCHAR*>(*C), C.Len() );
	else
		DrawTextExX( hDC, const_cast<TCHAR*>(*C), C.Len(), Rect, DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );
 	unguard;
}

// Converts an RGB value to HSL format
//
// RGB : X = Red,	Y = Green,		Z = Blue
// HSL : X = Hue,	Y = Saturation,	Z = Luminance
//
FVector FHeaderItem::ToHSL( FVector RGB )
{
	guard(FHeaderItem::ToHSL);
	FVector HSL(0,0,0);

	unsigned char minval = min(RGB.X, min(RGB.Y, RGB.Z));
	unsigned char maxval = max(RGB.X, max(RGB.Y, RGB.Z));
	float mdiff  = float(maxval) - float(minval);
	float msum   = float(maxval) + float(minval);

	HSL.Z = msum / 510.0f;

	if (maxval == minval) 
	{
		HSL.Y = 0.0f;
		HSL.X = 0.0f; 
	}   
	else 
	{ 
		float rnorm = (maxval - RGB.X  ) / mdiff;      
		float gnorm = (maxval - RGB.Y) / mdiff;
		float bnorm = (maxval - RGB.Z ) / mdiff;   

		HSL.Y = (HSL.Z <= 0.5f) ? (mdiff / msum) : (mdiff / (510.0f - msum));

		if (RGB.X   == maxval) HSL.X = 60.0f * (6.0f + bnorm - gnorm);
		if (RGB.Y == maxval) HSL.X = 60.0f * (2.0f + rnorm - bnorm);
		if (RGB.Z  == maxval) HSL.X = 60.0f * (4.0f + gnorm - rnorm);
		if (HSL.X > 360.0f) HSL.X = HSL.X - 360.0f;
	}

	HSL.X = 255.0f * (HSL.X / 360.0f);
	HSL.Y *= 255.0f;
	HSL.Z *= 255.0f;

	return HSL;
	unguard;
}

FVector FHeaderItem::FromHSV( BYTE H, BYTE S, BYTE V )
{
	FLOAT Brightness = V * 1.4f / 255.f;
	Brightness *= 0.7f/(0.01f + appSqrt(Brightness));
	Brightness  = Clamp(Brightness,0.f,1.f);
	FVector Hue = (H<86) ? FVector((85-H)/85.f,(H-0)/85.f,0) : (H<171) ? FVector(0,(170-H)/85.f,(H-85)/85.f) : FVector((H-170)/85.f,0,(255-H)/84.f);
	return 255*FVector( (Hue + S/255.f * (FVector(1,1,1) - Hue)) * Brightness );
}

void FHeaderItem::SetValue( const TCHAR* Value )
{
	guard(FHeaderItem::SetValue);
	if( GetCaption() == TEXT("LightColor") )
	{
		FString Temp = Value;
		INT i;
		INT R = (i=Temp.InStr(TEXT("R="))) != -1 ? appAtoi( *Temp.Mid(i+2) ) : 255;
		INT G = (i=Temp.InStr(TEXT("G="))) != -1 ? appAtoi( *Temp.Mid(i+2) ) : 255;
		INT B = (i=Temp.InStr(TEXT("B="))) != -1 ? appAtoi( *Temp.Mid(i+2) ) : 255;

		FVector HSL = ToHSL( FVector(R, G, B) );
		for( INT x = 0 ; x < Children.Num() ; x++ )
		{
			FTreeItem* Item = Children(x);

			if( Item->GetCaption() == TEXT("LightHue") )
				Item->SetValue( *FString::Printf( TEXT("%d"), (INT)HSL.X ) );
			if( Item->GetCaption() == TEXT("LightSaturation") )
				Item->SetValue( *FString::Printf( TEXT("%d"), (INT)HSL.Z ) );
			if( Item->GetCaption() == TEXT("LightBrightness") )
				Item->SetValue( *FString::Printf( TEXT("%d"), (INT)HSL.Y ) );

		}
		Redraw();
	}
	else
		FTreeItem::SetValue(Value);
	unguard;
}

void FHeaderItem::OnChooseHSLColorButton()
{
	guard(FHeaderItem::OnChooseHSLColorButton);
	Collapse();
	Expand();
	BYTE H=0, S=0, V=0;
	for( INT x=0 ; x<Children.Num(); x++ )
	{
		FTreeItem* Item = Children(x);
		TCHAR Str[255];
		if( Item->GetCaption() == TEXT("LightHue") )
		{
			((FPropertyItem*)Item)->GetPropertyText(Str);
			H = appAtoi(Str);
		}
		if( Item->GetCaption() == TEXT("LightSaturation") )
		{
			((FPropertyItem*)Item)->GetPropertyText(Str);
			S = appAtoi(Str);
		}
		if( Item->GetCaption() == TEXT("LightBrightness") )
		{
			((FPropertyItem*)Item)->GetPropertyText(Str);
			V = appAtoi(Str);
		}
	}

	// Get the current color in HSL format and convert it to RGB.
	FVector _RGB = FromHSV(H,S,V);

	// Open the color dialog.
	CHOOSECOLORA cc;
	static COLORREF acrCustClr[16];
	appMemzero( &cc, sizeof(cc) );
	cc.lStructSize  = sizeof(cc);
	cc.hwndOwner    = OwnerProperties->List;
	cc.lpCustColors = (LPDWORD)acrCustClr;
	cc.rgbResult    = RGB( _RGB.X, _RGB.Y, _RGB.Z );
	cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
	if( ChooseColorA(&cc)==TRUE )
	{
		FVector HSL = ToHSL( FVector( GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult) ));

		// Once the user chooses a color, loop through the child items of this header item, and set
		// the appropriate values.  We have to do this, since I don't see how to get access to the
		// actual actors themselves.  NOTE : You have to have this header item expanded before this will work.
		for( INT x=0; x<Children.Num(); x++ )
		{
			FTreeItem* Item = Children(x);

			if( Item->GetCaption() == TEXT("LightHue") )
				Item->SetValue( *FString::Printf( TEXT("%d"), (INT)HSL.X ) );
			if( Item->GetCaption() == TEXT("LightSaturation") )
				Item->SetValue( *FString::Printf( TEXT("%d"), (INT)HSL.Z ) );
			if( Item->GetCaption() == TEXT("LightBrightness") )
				Item->SetValue( *FString::Printf( TEXT("%d"), (INT)HSL.Y ) );
		}

		InvalidateRect( OwnerProperties->List, NULL, 0 );
		UpdateWindow( OwnerProperties->List );
	}
	Redraw();
	unguard;
}

void FHeaderItem::OnPickColorButton()
{
	guard(FHeaderItem::OnPickColorButton);
	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyExec( OwnerProperties, TEXT("USECOLOR") );
	Redraw();
	unguard;
}

void FHeaderItem::OnItemSetFocus()
{
	guard(FHeaderItem::OnItemSetFocus);
	if( GetCaption() == TEXT("LightColor") )
	{
		AddButton( LocalizeGeneral("ColorButton",TEXT("Window")), FDelegate(this,(TDelegate)&FHeaderItem::OnChooseHSLColorButton) );
		AddButton( LocalizeGeneral("PickButton",TEXT("Window")), FDelegate(this,(TDelegate)&FHeaderItem::OnPickColorButton) );
	}
	unguard;
}

void FHeaderItem::OnItemKillFocus( UBOOL Abort )
{
	guard(FHeaderItem::OnItemKillFocus);
	for( INT i=0; i<Buttons.Num(); i++ )
		delete Buttons(i);
	Buttons.Empty();
	ButtonWidth = 0;
	Redraw();
	unguard;
}

INT FHeaderItem::GetHeight()
{
	return 20;
}

/*-----------------------------------------------------------------------------
	FCategoryItem
-----------------------------------------------------------------------------*/

void FCategoryItem::Serialize( FArchive& Ar )
{
	guard(FCategoryItem::Serialize);
	FHeaderItem::Serialize( Ar );
	Ar << Category << BaseClass;
	unguard;
}

QWORD FCategoryItem::GetId() const
{
	guard(FCategoryItem::GetId);
	return Category.GetIndex() + ((QWORD)2<<32);
	unguard;
}

FString FCategoryItem::GetCaption() const
{
	guard(FCategoryItem::GetText);
	return *Category;
	unguard;
}

void FCategoryItem::Expand()
{
	guard(FCategoryItem::Expand);
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(BaseClass); It; ++It )
		if( It->Category==Category && AcceptFlags(It->PropertyFlags) )
			Children.AddItem( new(TEXT("FPropertyItem"))FPropertyItem( OwnerProperties, this, *It, It->GetFName(), It->Offset, -1, 666 ) );
	FTreeItem::Expand();
	unguard;
}

void FCategoryItem::Collapse()
{
	guard(FCategoryItem::Collapse);
	FTreeItem::Collapse();
	EmptyChildren();
	unguard;
}

/*-----------------------------------------------------------------------------
	FNewObjectItem
-----------------------------------------------------------------------------*/
FString LastUsedClass;

// Remember the last class new'd
void FNewObjectItem::OnItemSetFocus()
{
	guard(FNewObjectItem::OnItemSetFocus);
	FHeaderItem::OnItemSetFocus();

	// New button.
	AddButton( LocalizeGeneral("New",TEXT("Window")), FDelegate(this,(TDelegate)&FNewObjectItem::OnNew) );

	// Combo box.
	FRect Rect = GetRect() + FRect(0,0,-ButtonWidth-1,-1);
	Rect.Min.X = OwnerProperties->GetDividerWidth();

	HolderControl = new WLabel( &OwnerProperties->List );
	HolderControl->Snoop = this;
	HolderControl->OpenWindow( 0 );
	FRect HolderRect = Rect.Right(20) + FRect(0,0,0,1);
	HolderControl->MoveWindow( HolderRect, 0 );

	Rect = Rect + FRect(-2,-6,-2,0);

	ComboControl = new WComboBox( HolderControl );
	ComboControl->Snoop = this;
	ComboControl->SelectionEndOkDelegate     = FDelegate(this,(TDelegate)&FNewObjectItem::ComboSelectionEndOk);
	ComboControl->SelectionEndCancelDelegate = FDelegate(this,(TDelegate)&FNewObjectItem::ComboSelectionEndCancel);
	ComboControl->OpenWindow( 0, 1 );
	ComboControl->MoveWindow( Rect-HolderRect.Min, 1 );

	for( TObjectIterator<UClass> It; It; ++It )
		if( It->IsChildOf(Property->PropertyClass) && (It->ClassFlags&CLASS_EditInlineNew) && !(It->ClassFlags&CLASS_Abstract) )
			ComboControl->AddString( It->GetName() );	

	INT Index = ComboControl->FindString( *NewClass );
	ComboControl->SetCurrent( Index>=0 ? Index : 0 );

	ComboControl->Show(1);
	HolderControl->Show(1);
	SetFocus( *ComboControl );
	unguard;
}

void FNewObjectItem::OnItemKillFocus( UBOOL Abort )
{
	guard(FNewObjectItem::OnKillFocus);
	if( !Abort )
		NewClass = ComboControl->GetString(ComboControl->GetCurrent());
	if( ComboControl )
		delete ComboControl;
	ComboControl=NULL;
	if( HolderControl )
		delete HolderControl;
	HolderControl=NULL;
	FHeaderItem::OnItemKillFocus( Abort );
	unguard;
}

void FNewObjectItem::ComboSelectionEndCancel()
{
	guard(FNewObjectItem::ComboSelectionEndCancel);
	INT Index=ComboControl->FindString( *NewClass );
	ComboControl->SetCurrent( Index>=0 ? Index : 0 );
	unguard;
}

void FNewObjectItem::ComboSelectionEndOk()
{
	guard(FNewObjectItem::ComboSelectionEndOk);
	NewClass = ComboControl->GetString(ComboControl->GetCurrent());
	LastUsedClass = NewClass;
	Redraw();
	unguard;
}

void FNewObjectItem::OnNew()
{
	guard(FNewObjectItem::OnNew);
	UObject* OuterObject = GetParentObject()->GetOuter();
	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyExec( Parent, *FString::Printf(TEXT("NEWOBJECT CLASS=%s OUTER=%s"), *NewClass, OuterObject->GetPathName() ) );
	FTreeItem* P = Parent;
	P->Collapse();
	P->Expand();
	P->Children(0)->Expand();
	unguard;
}

void FNewObjectItem::Draw( HDC hDC )
{
	guard(FNewObjectItem::Draw);
	FRect Rect = GetRect();
	TCHAR Str[4096];//!!

	// Draw background.
	FillRect( hDC, Rect, hBrushGrey160 ); 

	// Draw left background.
	FRect LeftRect=Rect;
	LeftRect.Max.X = OwnerProperties->GetDividerWidth();
	FillRect( hDC, LeftRect+FRect(0,1-GetSelected(),-1,0), GetBackgroundBrush(GetSelected()) );
	LeftRect.Min.X += GetIndentPixels(1);

	// Draw tree lines.
	DrawTreeLines( hDC, Rect, 0 );

	// Setup text.
	SetBkMode( hDC, TRANSPARENT );

	// Draw left text.
	appStrcpy( Str, LocalizeGeneral("New",TEXT("Window")) );
	SetTextColor( hDC, GetTextColor(GetSelected()) );
	DrawTextExX( hDC, Str, appStrlen(Str), LeftRect + FRect(0,1,0,0), DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );

	// Draw right background.
	FRect RightRect = Rect;
	RightRect.Min.X = OwnerProperties->GetDividerWidth();
	FillRect( hDC, RightRect+FRect(0,1,0,0), GetBackgroundBrush(0) );

	// Draw right text.
	RightRect.Max.X -= ButtonWidth;
	SetTextColor( hDC, GetTextColor(0) );
	DrawTextExX( hDC, (TCHAR *)(*NewClass), NewClass.Len(), RightRect + FRect(4,1,0,0), DT_END_ELLIPSIS | DT_LEFT | DT_SINGLELINE | DT_VCENTER, NULL );

 	unguard;
}

FString FNewObjectItem::GetCaption() const
{ 
	guard(FNewObjectItem::GetCaption);
	return TEXT("");
	unguard;
}

QWORD FNewObjectItem::GetId() const
{
	guard(FNewObjectItem::GetId);
	return ((QWORD)this) + (((QWORD)4)<<32);
	unguard;
}

/*-----------------------------------------------------------------------------
	WProperties
-----------------------------------------------------------------------------*/

void WProperties::Serialize( FArchive& Ar )
{
	guard(WProperties::Serialize);
	WPropertiesBase::Serialize( Ar );
	GetRoot()->Serialize( Ar );
	unguard;
}

void WProperties::DoDestroy()
{
	guard(WProperties::DoDestroy);
	PropertiesWindows.RemoveItem( this );
	WWindow::DoDestroy();
	unguard;
}

INT WProperties::OnSetCursor()
{
	guard(WProperties::OnSetCursor);
	if( ::IsWindow(List.hWnd) && List.GetCount() )
	{
		FPoint P = GetCursorPos();
		INT Index = List.ItemFromPoint( P );
		return Index>0 ? GetListItem( Index )->OnSetCursor() : 0;
	}
	return 0;
	unguard;
}

void WProperties::OnDestroy()
{
	guard(WProperties::OnDestroy);
	WWindow::OnDestroy();
	_DeleteWindows.AddItem( this );
	unguard;
}

void WProperties::OpenChildWindow( INT InControlId )
{
	guard(WProperties::OpenChildWindow);
	HWND hWndParent = InControlId ? GetDlgItem(OwnerWindow->hWnd,InControlId) : OwnerWindow->hWnd;
	check(hWndParent);
	FRect R;
	::GetClientRect( hWndParent, R );
	PerformCreateWindowEx
	(
		0, NULL, WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		0, 0, R.Width(), R.Height(),
		hWndParent, NULL, hInstance
	);
	List.OpenWindow( 1, 0, 0, 1 );
	Show(1);
	unguard;
}

void WProperties::OpenWindow( HWND hWndParent )
{
	guard(WProperties::OpenWindow);
	PerformCreateWindowEx
	(
		WS_EX_TOOLWINDOW,
		*GetRoot()->GetCaption(),
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_MINIMIZEBOX,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		256+64+32,
		512,
		hWndParent ? hWndParent : OwnerWindow ? OwnerWindow->hWnd : NULL,
		NULL,
		hInstance
	);
	List.OpenWindow( 1, 1, 0, 1 );
	unguard;
}

void WProperties::OnActivate( UBOOL Active )
{
	guard(WProperties::OnActivate);
	if( Active==1 )
	{
		SetFocus( List );
		if( !FocusItem )
			SetItemFocus( 1 );
	}
	else
	{
		SetItemFocus( 0 );
	}
	unguard;
}

void WProperties::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	guard(WProperties::OnSize);
	WWindow::OnSize( Flags, NewX, NewY );
	if( List.hWnd )
	{
		SetItemFocus( 0 );
		InvalidateRect( List, NULL, FALSE );
		ResizeList();
		SetItemFocus( 1 );
	}
	unguard;
}

void WProperties::OnPaint()
{
	guard(WProperties::OnPaint);
	if( GetUpdateRect( *this, NULL, 0 ) )
	{
		PAINTSTRUCT PS;
		HDC   hDC      = BeginPaint( *this, &PS );
		FRect Rect     = GetClientRect();
		FRect ListRect = List.GetClientRect();
		Rect.Min.Y     = ListRect.Max.Y;
		FillRect( hDC, Rect, (HBRUSH)(COLOR_BTNFACE+1) ); 
		EndPaint( *this, &PS );
	}
	unguard;
}

void WProperties::OnListDoubleClick()
{
	if( FocusItem )
		FocusItem->OnItemDoubleClick();
}

void WProperties::OnListSelectionChange()
{
	SetItemFocus( 1 );
}

void WProperties::SnoopLeftMouseDown( WWindow* Src, FPoint P )
{
	guard(WProperties::SnoopLeftMouseDown);
	if( Src==&List )
	{
		INT Index = List.ItemFromPoint( P );
		if( Index>=0 )
		{
			List.SetCurrent( Index, 0 );
			FTreeItem* Item = GetListItem( Index );
			Item->OnItemLeftMouseDown( P-Item->GetRect().Min );
		}
	}

	if ( Snoop )
		Snoop->SnoopLeftMouseDown(Src,P);

	unguard;
}

void WProperties::SnoopRightMouseDown( WWindow* Src, FPoint P )
{
	guard(WProperties::SnoopRightMouseDown);
	if( Src==&List )
	{
		INT Index = List.ItemFromPoint( P );
		if( Index>=0 )
		{
			List.SetCurrent( Index, 0 );
			FTreeItem* Item = GetListItem( Index );
			Item->OnItemRightMouseDown( P-Item->GetRect().Min );
		}
	}

	if ( Snoop )
		Snoop->SnoopRightMouseDown(Src, P);

	unguard;
}

void WProperties::SnoopChar( WWindow* Src, INT Char )
{
	guard(WProperties::SnoopChar);
	if( FocusItem )
		FocusItem->SnoopChar( Src, Char );
	unguard;
}

void WProperties::SnoopKeyDown( WWindow* Src, INT Char )
{
	guard(WProperties::SnoopChar);
	if( Char==9 )
		PostMessageX( List, WM_KEYDOWN, (GetKeyState(16)&0x8000)?VK_UP:VK_DOWN, 0 );
	WPropertiesBase::SnoopKeyDown( Src, Char );
	unguard;
}

INT WProperties::GetDividerWidth()
{
	guard(WProperties::GetDividerWidth);
	return DividerWidth;
	unguard;
}

void WProperties::BeginSplitterDrag()
{
	guard(WProperties::BeginDrag);
	SetItemFocus( NULL );
	DragInterceptor            = new WDragInterceptor( this, FPoint(0,INDEX_NONE), GetClientRect(), FPoint(3,3) );	
	DragInterceptor->DragPos   = FPoint(GetDividerWidth(),GetCursorPos().Y);
	DragInterceptor->DragClamp = FRect(GetClientRect().Inner(FPoint(64,0)));
	DragInterceptor->OpenWindow();
	unguard;
}

void WProperties::OnFinishSplitterDrag( WDragInterceptor* Drag, UBOOL Success )
{
	guard(WProperties::OnFinishSplitterDrag);
	if( Success )
	{
		DividerWidth += Drag->DragPos.X - Drag->DragStart.X;
		if( PersistentName!=NAME_None )
			GConfig->SetInt( TEXT("WindowPositions"), *(FString(*PersistentName)+TEXT(".Split")), DividerWidth, TEXT("User.ini") ); //amb
		InvalidateRect( *this, NULL, 1 );
		InvalidateRect( List, NULL, 1 );
		UpdateWindow( *this );
	}
	DragInterceptor = NULL;
	unguard;
}

void WProperties::SetValue( const TCHAR* Value )
{
	guard(WProperties::SetValue);
	if( FocusItem )
		FocusItem->SetValue( Value );
	unguard;
}

void WProperties::SetItemFocus( UBOOL FocusCurrent )
{
	guard(WProperties::SetItemFocus);
	if( FocusItem )
		FocusItem->OnItemKillFocus( 0 );
	FocusItem = NULL;
	if( FocusCurrent && List.GetCount()>0 )
		FocusItem = GetListItem( List.GetCurrent() );
	if( FocusItem )
		FocusItem->OnItemSetFocus();
	unguard;
}

void WProperties::ResizeList()
{
	guard(WProperties::ResizeList);
	FRect ClientRect = GetClientRect();
	FRect R(0,0,0,4);//!!why?
	for( INT i=List.GetCount()-1; i>=0; i-- )
		R.Max.Y += List.GetItemHeight( i );
	AdjustWindowRect( R, GetWindowLongX(List,GWL_STYLE), 0 );
	List.MoveWindow( FRect(0,0,ClientRect.Width(),Min(ClientRect.Height(),R.Height())), 1 );
	unguard;
}

void WProperties::ForceRefresh()
{
	guard(WProperties::ForceRefresh);

	if( !bAllowForceRefresh ) return;

	// Disable editing.
	SetItemFocus( 0 );

	// Remember which items were expanded.
	if( List.GetCount() )
	{
		Remembered.Empty();
		SavedTop     = GetListItem(List.GetTop())->GetId();
		SavedCurrent = List.GetSelected(List.GetCurrent()) ? GetListItem(List.GetCurrent())->GetId() : 0;
		for( INT i=0; i<List.GetCount(); i++ )
		{
			FTreeItem* Item = GetListItem(i);
			if( Item->Expanded )
			{
				Remembered.AddItem( Item->GetId() );
			}
		}
	}

	// Empty it and add root items.
	List.Empty();
	GetRoot()->EmptyChildren();
	GetRoot()->Expanded=0;
	GetRoot()->Expand();

	// Restore expansion state of the items.
	INT CurrentIndex=-1, TopIndex=-1;
	for( INT i=0; i<List.GetCount(); i++ )
	{
		FTreeItem* Item = GetListItem(i);
		QWORD      Id   = Item->GetId();
		if( Item->Expandable && !Item->Expanded )
		{
			for( INT j=0; j<Remembered.Num(); j++ )
				if( Remembered(j)==Id )
				{
					Item->Expand();
					break;
				}
		}
		if( Id==SavedTop     ) TopIndex     = i;
		if( Id==SavedCurrent ) CurrentIndex = i;
	}

	// Adjust list size.
	ResizeList();

	// Set indices.
	if( TopIndex>=0 ) List.SetTop( TopIndex );
	if( CurrentIndex>=0 ) List.SetCurrent( CurrentIndex, 1 );

	unguard;
}

// gam ---
void WProperties::ExpandAll()
{
	for( INT i=0; i<List.GetCount(); i++ )
	{
		FTreeItem* Item = GetListItem(i);

		if( Item->Expandable && !Item->Expanded )
			Item->Expand();
	}
}
// --- gam

/*-----------------------------------------------------------------------------
	FPropertyItemBase
-----------------------------------------------------------------------------*/

void FPropertyItemBase::Serialize( FArchive& Ar )
{
	guard(FPropertyItemBase::Serialize);
	FHeaderItem::Serialize( Ar );
	Ar << BaseClass;
	unguard;
}

UBOOL FPropertyItemBase::AcceptFlags( DWORD InFlags )
{
	guard(FPropertyItemBase::AcceptFlags);
	return (InFlags&FlagMask)==FlagMask;
	unguard;
}

void FPropertyItemBase::GetStates( TArray<FName>& States )
{
	guard(FPropertyItemBase::GetStates);
	if( BaseClass )
		for( TFieldIterator<UState> StateIt(BaseClass); StateIt; ++StateIt )
			if( StateIt->StateFlags & STATE_Editable )
				States.AddUniqueItem( StateIt->GetFName() );
	unguard;
}

void FPropertyItemBase::Collapse()
{
	guard(FPropertyItemBase::Collapse);
	FTreeItem::Collapse();
	EmptyChildren();
	unguard;
}

FString FPropertyItemBase::GetCaption() const
{
	guard(FPropertyItemBase::GetCaption);
	return Caption;
	unguard;
}

QWORD FPropertyItemBase::GetId() const
{
	guard(FPropertyItemBase::GetId);
	return (QWORD)BaseClass + (QWORD)4;
	unguard;
}

/*-----------------------------------------------------------------------------
	FObjectsItem
-----------------------------------------------------------------------------*/

void FObjectsItem::Serialize( FArchive& Ar )
{
	guard(FObjectsItem::Serialize);
	FPropertyItemBase::Serialize( Ar );
	Ar << _Objects;
	unguard;
}

BYTE* FObjectsItem::GetBase( BYTE* Base )
{
	return Base;
}

BYTE* FObjectsItem::GetReadAddress( FPropertyItem* Child, UBOOL RequireSingleSelection )
{
	guard(FObjectsItem::GetReadAddress);
	if( !_Objects.Num() )
		return NULL;
	if( RequireSingleSelection && _Objects.Num() > 1 )
		return NULL;

	// If this item is the child of an array, return NULL if there is a different number
	// of items in the array in different objects, when multi-selecting.
	if( Cast<UArrayProperty>(Child->Property->GetOuter()) )
	{
		INT Num0 = ((FArray*)Child->Parent->GetBase((BYTE*)_Objects(0)))->Num();
		for( INT i=1; i<_Objects.Num(); i++ )
			if( Num0 != ((FArray*)Child->Parent->GetBase((BYTE*)_Objects(i)))->Num() )
				return NULL;
	}

	BYTE* Base0 = Child->GetBase((BYTE*)_Objects(0));

	// If the item is an array itself, return NULL if there are a different number of
	// items in the array in different objects, when multi-selecting.
	if( Cast<UArrayProperty>(Child->Property) )
	{
		INT Num0 = ((FArray*)Child->GetBase((BYTE*)_Objects(0)))->Num();
		for( INT i=1; i<_Objects.Num(); i++ )
			if( Num0 != ((FArray*)Child->GetBase((BYTE*)_Objects(i)))->Num() )
				return NULL;
	}
	else
	{
		for( INT i=1; i<_Objects.Num(); i++ )
			if( !Child->Property->Identical( Base0, Child->GetBase((BYTE*)_Objects(i)) ) )
				return NULL;
	}
	return Base0;
	unguard;
}

void FObjectsItem::NotifyChange()
{
	guard(FObjectsItem::NotifyChange)
	for( INT i=0; i<_Objects.Num(); i++ )
		_Objects(i)->PostEditChange();
	if( NotifyParent )
		Parent->NotifyChange();
	unguard;
}

void FObjectsItem::SetProperty( FPropertyItem* Child, const TCHAR* Value )
{
	guard(FObjectsItem::SetProperty);
	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyPreChange( OwnerProperties );
	for( INT i=0; i<_Objects.Num(); i++ )
	{
		if(  Cast<UFloatProperty>(Child->Property) || Cast<UIntProperty>(Child->Property) || 
			(Cast<UByteProperty>(Child->Property) && !Cast<UByteProperty>(Child->Property)->Enum) )
		{
			FLOAT Result;
			FString Wk = Child->Equation = Value;
			if( Eval( Wk, &Result ) )
				Child->Property->ImportText( *FString::Printf(TEXT("%f"),Result), Child->GetBase((BYTE*)_Objects(i)), PPF_Localized );
			else
			{
				Child->Equation = Value;
				Child->Property->ImportText( Value, Child->GetBase((BYTE*)_Objects(i)), PPF_Localized );
			}
		}
		else
			Child->Property->ImportText( Value, Child->GetBase((BYTE*)_Objects(i)), PPF_Localized );
		_Objects(i)->PostEditChange();
	}
	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyPostChange( OwnerProperties );
	if( NotifyParent )
		Parent->NotifyChange();
	unguard;
}

// Evaluate a numerical expression.
// Returns 1 if ok, 0 if error.
// Sets Result, or 0.0 if error.
//
// Operators and precedence: 1:+- 2:/% 3:* 4:^ 5:&|
// Unary: -
// Types: Numbers (0-9.), Hex ($0-$f)
// Grouping: ( )
//
UBOOL FObjectsItem::Eval( FString Str, FLOAT* pResult )
{
	guard(FObjectsItem::Eval);

	FLOAT Result;

	// Check for a matching number of brackets right up front.
	INT Brackets = 0;
	for( INT x = 0 ; x < Str.Len() ; x++ )
	{
		if( Str.Mid(x,1) == TEXT("(") )	Brackets++;
		if( Str.Mid(x,1) == TEXT(")") )	Brackets--;
	}
	if( Brackets != 0 )
	{
		debugf(TEXT("Expression Error : Mismatched brackets"));
		Result = 0;
	}
	else
		if( !SubEval( &Str, &Result, 0 ) )
		{
			debugf(TEXT("Expression Error : Error in expression"));
			Result = 0;
		}

	*pResult = Result;

	return 1;
	unguard;
}

UBOOL FObjectsItem::SubEval( FString* pStr, FLOAT* pResult, INT Prec )
{
	guard(FObjectsItem::SubEval);

	FString c;
	FLOAT V, W, N;

	V = W = N = 0.0f;

	c = GrabChar(pStr);

	if((c >= TEXT("0") && c <= TEXT("9")) || c == TEXT(".")) // Number
	{
		V = 0;
		while(c >= TEXT("0") && c <= TEXT("9"))
		{
			V = V * 10 + Val(c);
			c = GrabChar(pStr);
		}
		if(c == TEXT("."))
		{
			N = 0.1f;
			c = GrabChar(pStr);
			while(c >= TEXT("0") && c <= TEXT("9"))
			{
				V = V + N * Val(c);
				N = N / 10.0f;
				c = GrabChar(pStr);
			}
		}
	}
	else if( c == TEXT("(")) // Opening parenthesis
	{
		if( !SubEval(pStr, &V, 0) )
			return 0;
		c = GrabChar(pStr);
	}
	else if( c == TEXT("-") ) // Negation
	{
		if( !SubEval(pStr, &V, 1000) )
			return 0;
		V = -V;
		c = GrabChar(pStr);
	}
	else if( c == TEXT("+")) // Positive
	{
		if( !SubEval(pStr, &V, 1000) )
			return 0;
		c = GrabChar(pStr);
	}
	else if( c == TEXT("@") ) // Square root
	{
		if( !SubEval(pStr, &V, 1000) )
			return 0;
		if( V < 0 )
		{
			debugf(TEXT("Expression Error : Can't take square root of negative number"));
			return 0;
		}
		else
			V = appSqrt(V);
		c = GrabChar(pStr);
	}
	else // Error
	{
		debugf(TEXT("Expression Error : No value recognized"));
		return 0;
	}
PrecLoop:
	if( c == TEXT("") )
	{
		*pResult = V;
		return 1;
	}
	else if( c == TEXT(")") )
	{
		*pStr = TEXT(")") + *pStr;
		*pResult = V;
		return 1;
	}
	else if( c == TEXT("+") )
	{
		if( Prec > 1 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
			if( SubEval(pStr, &W, 2) )
			{
				V = V + W;
				c = GrabChar(pStr);
				goto PrecLoop;
			}
	}
	else if( c == TEXT("-") )
	{
		if( Prec > 1 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
			if( SubEval(pStr, &W, 2) )
			{
				V = V - W;
				c = GrabChar(pStr);
				goto PrecLoop;
			}
	}
	else if( c == TEXT("/") )
	{
		if( Prec > 2 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
			if( SubEval(pStr, &W, 3) )
			{
				if( W == 0 )
				{
					debugf(TEXT("Expression Error : Division by zero isn't allowed"));
					return 0;
				}
				else
				{
					V = V / W;
					c = GrabChar(pStr);
					goto PrecLoop;
				}
			}
	}
	else if( c == TEXT("%") )
	{
		if( Prec > 2 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else 
			if( SubEval(pStr, &W, 3) )
			{
				if( W == 0 )
				{
					debugf(TEXT("Expression Error : Modulo zero isn't allowed"));
					return 0;
				}
				else
				{
					V = (INT)V % (INT)W;
					c = GrabChar(pStr);
					goto PrecLoop;
				}
			}
	}
	else if( c == TEXT("*") )
	{
		if( Prec > 3 )
		{
			*pResult = V;
			*pStr = c + *pStr;
			return 1;
		}
		else
			if( SubEval(pStr, &W, 4) )
			{
				V = V * W;
				c = GrabChar(pStr);
				goto PrecLoop;
			}
	}
	else
	{
		debugf(TEXT("Expression Error : Unrecognized Operator"));
	}

	*pResult = V;
	return 1;
	unguard;
}

void FObjectsItem::Expand()
{
	guard(FObjectsItem::Expand);
	UBOOL OldSorted = Sorted;
	if( ByCategory )
	{
		if( BaseClass && (BaseClass->ClassFlags&CLASS_CollapseCategories) )
		{
			// Sort by category, but don't show the category headers.
			Sorted = 0;
			TArray<FName> Categories;
			for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(BaseClass); It; ++It )
				if( BaseClass->HideCategories.FindItemIndex(It->Category)==INDEX_NONE )
					Categories.AddUniqueItem( It->Category );

			for( INT i=0; i<Categories.Num(); i++ )
				for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(BaseClass); It; ++It )
					if( It->Category==Categories(i) && AcceptFlags(It->PropertyFlags) )
						Children.AddItem( new(TEXT("FPropertyItem"))FPropertyItem( OwnerProperties, this, *It, It->GetFName(), It->Offset, -1, 666 ) );
		}
		else
		{
			// Expand to show categories.
			TArray<FName> Categories;
			for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(BaseClass); It; ++It )
				if( AcceptFlags( It->PropertyFlags ) && BaseClass->HideCategories.FindItemIndex(It->Category)==INDEX_NONE )
					Categories.AddUniqueItem( It->Category );
			for( INT i=0; i<Categories.Num(); i++ )
				Children.AddItem( new(TEXT("FCategoryItem"))FCategoryItem(OwnerProperties,this,BaseClass,Categories(i),1) );
		}
	}
	else
	{
		// Expand to show individual items.
		for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(BaseClass); It; ++It )
			if( AcceptFlags(It->PropertyFlags)
			&&	BaseClass->HideCategories.FindItemIndex(It->Category)==INDEX_NONE
			&&	It->GetOwnerClass()!=UObject::StaticClass() //hack for ufactory display!!
			)
				Children.AddItem( new(TEXT("FPropertyItem"))FPropertyItem( OwnerProperties, this, *It, It->GetFName(), It->Offset, -1, 666 ) );
	}
	FTreeItem::Expand();
	Sorted = OldSorted;
	unguard;
}

FString FObjectsItem::GrabChar( FString* pStr )
{
	guard(FObjectsItem::GrabChar);

	FString GrabChar;
	if( pStr->Len() )
		do {
			GrabChar = pStr->Left(1);
			*pStr = pStr->Mid(1);
		} while( GrabChar == TEXT(" ") );
	else
		GrabChar = TEXT("");

	return GrabChar;
	unguard;
}

// Converts a string to it's numeric equivalent, ignoring whitespace.
// "123  45" - becomes 12,345
FLOAT FObjectsItem::Val( FString Value )
{
	FLOAT RetValue = 0;

	for( INT x = 0 ; x < Value.Len() ; x++ )
	{
		FString Char = Value.Mid(x, 1);

		if( Char >= TEXT("0") && Char <= TEXT("9") )
		{
			RetValue *= 10;
			RetValue += appAtoi( *Char );
		}
		else 
			if( Char != TEXT(" ") )
				break;
	}

	return RetValue;
}

FString FObjectsItem::GetCaption() const
{
	guard(FObjectsItem::GetCaption);

	if( Caption.Len() )
		return Caption;			
	else if( !BaseClass )
		return LocalizeGeneral("PropNone",TEXT("Window"));
	else if( _Objects.Num()==1 )
		return FString::Printf( LocalizeGeneral("PropSingle",TEXT("Window")), BaseClass->GetName() );
	else
		return FString::Printf( LocalizeGeneral("PropMulti",TEXT("Window")), BaseClass->GetName(), _Objects.Num() );

	unguard;
}

void FObjectsItem::SetObjects( UObject** InObjects, INT Count )
{
	guard(FObjectsItem::SetObjects);

	// Disable editing, to prevent crash due to edit-in-progress after empty objects list.
	OwnerProperties->SetItemFocus( 0 );

	// Add objects and find lowest common base class.
	_Objects.Empty();
	BaseClass = NULL;
	for( INT i=0; i<Count; i++ )
	{
		if( InObjects[i] )
		{
			check(InObjects[i]->GetClass());
			if( BaseClass==NULL )	
				BaseClass=InObjects[i]->GetClass();
			while( !InObjects[i]->GetClass()->IsChildOf(BaseClass) )
				BaseClass = BaseClass->GetSuperClass();
			_Objects.AddItem( InObjects[i] );
		}
	}

	// Automatically title the window.
	OwnerProperties->SetText( *GetCaption() );

	// Refresh all properties.
	if( Expanded || this==OwnerProperties->GetRoot() )
		OwnerProperties->ForceRefresh();

	unguard;
}

UObject* FObjectsItem::GetParentObject()
{
	guard(FObjectsItem::GetParentObject);
	return _Objects(0);
	unguard;
}


/*-----------------------------------------------------------------------------
	WObjectProperties
-----------------------------------------------------------------------------*/

FTreeItem* WObjectProperties::GetRoot()
{
	guard(WObjectProperties::GetRoot);
	return &Root;
	unguard;
}

void WObjectProperties::Show( UBOOL Show )
{
	WProperties::Show(Show);
	// Prevents the property windows from being hidden behind other windows.
	if( bShow )
		BringWindowToTop( hWnd );
}

/*-----------------------------------------------------------------------------
	FClassItem
-----------------------------------------------------------------------------*/

BYTE* FClassItem::GetBase( BYTE* Base )
{
	return Base;
}

// FTreeItem interface.
BYTE* FClassItem::GetReadAddress( class FPropertyItem* Child, UBOOL RequireSingleSelection )
{
	guard(FObjectsItem::GetReadAddress);
	return Child->GetBase(&BaseClass->Defaults(0));
	unguard;
}

void FClassItem::SetProperty( FPropertyItem* Child, const TCHAR* Value )
{
	guard(FObjectsItem::SetProperty);
	Child->Property->ImportText( Value, GetReadAddress(Child), PPF_Localized );
	BaseClass->SetFlags( RF_SourceModified );
	unguard;
}

void FClassItem::Expand()
{
	guard(FObjectsItem::Expand);
	TArray<FName> Categories;
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(BaseClass); It; ++It )
		if( AcceptFlags( It->PropertyFlags ) )
			Categories.AddUniqueItem( It->Category );
	for( INT i=0; i<Categories.Num(); i++ )
		Children.AddItem( new(TEXT("FCategoryItem"))FCategoryItem(OwnerProperties,this,BaseClass,Categories(i),1) );
	FTreeItem::Expand();
	unguard;
}

/*-----------------------------------------------------------------------------
	WClassProperties
-----------------------------------------------------------------------------*/

FTreeItem* WClassProperties::GetRoot()
{
	guard(WClassProperties::GetRoot);
	return &Root;
	unguard;
}

/*-----------------------------------------------------------------------------
	FObjectConfigItem
-----------------------------------------------------------------------------*/

BYTE* FObjectConfigItem::GetBase( BYTE* Base )
{
	return Base;
}

BYTE* FObjectConfigItem::GetReadAddress( FPropertyItem* Child, UBOOL RequireSingleSelection )
{
	guard(FObjectsItem::GetReadAddress);
	return Child->GetBase(&Class->Defaults(0));
	unguard;
}

void FObjectConfigItem::SetProperty( FPropertyItem* Child, const TCHAR* Value )
{
	guard(FObjectsItem::SetProperty);
	check(Class);
	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyPreChange( OwnerProperties );
	if( Cast<UClassProperty>(Child->Property) && appStricmp(*Child->Property->Category,TEXT("Drivers"))==0 )
	{
		// Save it.
		UClassProperty* ClassProp = CastChecked<UClassProperty>( Child->Property );
		TArray<FRegistryObjectInfo> Classes;
		UObject::GetRegistryObjects( Classes, UClass::StaticClass(), ClassProp->MetaClass, 0 );
		for( INT i=0; i<Classes.Num(); i++ )
		{
			TCHAR Path[4096], *Str;
			appStrcpy( Path, *Classes(i).Object );
			Str = appStrstr(Path,TEXT("."));
			const TCHAR* Text = Str ? (*Str++=0,Localize(Str,TEXT("ClassCaption"),Path)) : Localize("Language","Language",TEXT("Core"),Path);
				if( appStricmp( Text, Value )==0 )
					GConfig->SetString( Child->Property->GetOwnerClass()->GetPathName(), Child->Property->GetName(), *Classes(i).Object );
		}
	}
	else if( Cast<UArrayProperty>(Child->Property->GetOuter()) )
	{
		// Arrays.
		Child->Property->ImportText( Value, GetReadAddress(Child), PPF_Localized );
		Class->GetDefaultObject()->SaveConfig();
	}
	else
	{
		UStructProperty* ParentStructProperty = 0;
		PTRINT offset = 0;
		if( Child->Parent && Child->Parent->IsPropertyItem() )
		{
			ParentStructProperty = 
				Cast<UStructProperty>(((FPropertyItem*)
				(Child->Parent))->Property);                    
		}
		if( ParentStructProperty )
		{
			// member of struct
			BYTE* temp = 0;
			temp += ParentStructProperty->Offset;
			temp += Child->Property->Offset;
			offset = (PTRINT) temp;
		}
		else
		{
			// regular property
			offset = Child->Property->Offset;
		}

		//!! less limited now
		UObject::GlobalSetProperty( Value, Class, Child->Property, offset, Immediate );
	}

	if( OwnerProperties->NotifyHook )
		OwnerProperties->NotifyHook->NotifyPostChange( OwnerProperties );
	unguard;
}

void FObjectConfigItem::OnResetToDefaultsButton()
{
	guard(FObjectConfigItem::OnResetToDefaultsButton);
	LazyLoadClass();
	if( Class )
	{
		UObject::ResetConfig( Class );
		InvalidateRect( OwnerProperties->List, NULL, 1 );
		UpdateWindow( OwnerProperties->List );
	}
	Redraw();
	unguard;
}

void FObjectConfigItem::OnItemSetFocus()
{
	FPropertyItemBase::OnItemSetFocus();
	AddButton( LocalizeGeneral("DefaultsButton",TEXT("Window")), FDelegate(this,(TDelegate)&FObjectConfigItem::OnResetToDefaultsButton) );
}

void FObjectConfigItem::Expand()
{
	guard(FObjectsItem::Expand);
	LazyLoadClass();
	if( Class )
	{
		if( Children.Num()==0 )
		{
			Class->GetDefaultObject()->LoadConfig( 1 );//!!
			for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Class); It; ++It )
			{
				if
				(	(AcceptFlags(It->PropertyFlags))
				&&	(Class==It->GetOwnerClass() || !(It->PropertyFlags&CPF_GlobalConfig) )
				&&	(CategoryFilter==NAME_None || It->Category==CategoryFilter) )
					Children.AddItem( new(TEXT("FPropertyItem"))FPropertyItem( OwnerProperties, this, *It, It->GetFName(), It->Offset, -1, 666 ) );
			}
		}
		FTreeItem::Expand();
	}
	else
	{
		Expandable = 0;
		Redraw();
	}
	unguard;
}

void FObjectConfigItem::LazyLoadClass()
{
	guard(FObjectConfigItem::LazyLoadClass);
	if( !Class && !Failed )
	{
		Class = UObject::StaticLoadClass( UObject::StaticClass(), NULL, *ClassName, NULL, LOAD_NoWarn, NULL );
		if( !Class )
		{
			Failed = 1;
			Caption = FString::Printf( LocalizeError("FailedConfigLoad",TEXT("Window")), ClassName );
		}
	}
	unguard;
}

void FObjectConfigItem::Serialize( FArchive& Ar )
{
	guard(FObjectConfigItem::Serialize);
	FPropertyItemBase::Serialize( Ar );
	Ar << CategoryFilter << Class;
	unguard;
}

/*-----------------------------------------------------------------------------
	FConfigItem
-----------------------------------------------------------------------------*/

QWORD FConfigItem::GetId() const
{
	guard(FConfigItem::GetId);
	return ((QWORD)this) + (((QWORD)3)<<32);
	unguard;
}

FString FConfigItem::GetCaption() const
{
	guard(FConfigItem::GetText);
	return Prefs.Caption;
	unguard;
}

void FConfigItem::Expand()
{
	guard(FConfigItem::Expand);
	TArray<FPreferencesInfo> NewPrefs;
	UObject::GetPreferences( NewPrefs, *Prefs.Caption, 0 );
	for( INT i=0; i<NewPrefs.Num(); i++ )
	{
		INT j;
		for( j=0; j<Children.Num(); j++ )
		{
			if( appStricmp( *Children(j)->GetCaption(), *NewPrefs(i).Caption )==0 )
				break;
		}
		if( j==Children.Num() )
		{
			if( NewPrefs(i).Class!=TEXT("") )
				Children.AddItem( new(TEXT("FObjectConfigItem"))FObjectConfigItem( OwnerProperties, this, *NewPrefs(i).Caption, *NewPrefs(i).Class, NewPrefs(i).Immediate, NewPrefs(i).Category ) );
			else
				Children.AddItem( new(TEXT("FConfigItem"))FConfigItem( NewPrefs(i), OwnerProperties, this ) );
		}
	}
	FTreeItem::Expand();
	unguard;
}

void FConfigItem::Collapse()
{
	guard(FConfigItem::Collapse);
	FTreeItem::Collapse();
	EmptyChildren();
	unguard;
}

/*-----------------------------------------------------------------------------
	WConfigProperties
-----------------------------------------------------------------------------*/

FTreeItem* WConfigProperties::GetRoot()
{
	guard(WConfigProperties::GetRoot);
	return &Root;
	unguard;
}

/*-----------------------------------------------------------------------------
	FPropertyItem.
-----------------------------------------------------------------------------*/

void FPropertyItem::Expand()
{
	guard(FPropertyItem::Expand);
	UStructProperty* StructProperty;
	UArrayProperty* ArrayProperty;
	UObjectProperty* ObjectProperty;
	if( Property->ArrayDim>1 && ArrayIndex==-1 )
	{
		// Expand array.
		Sorted=0;
		for( INT i=0; i<Property->ArrayDim; i++ )
			Children.AddItem( new(TEXT("FPropertyItem"))FPropertyItem( OwnerProperties, this, Property, Name, i*Property->ElementSize, i, 666 ) );
	}
	else if( (ArrayProperty=Cast<UArrayProperty>(Property))!=NULL )
	{
		// Expand array.
		Sorted=0;
		FArray* Array = (FArray*)GetReadAddress(this);
		if( Array )
			for( INT i=0; i<Array->Num(); i++ )
				Children.AddItem( new(TEXT("FPropertyItem"))FPropertyItem( OwnerProperties, this, ArrayProperty->Inner, Name, i*ArrayProperty->Inner->ElementSize, i, 666 ) );
	}
	else if( (StructProperty=Cast<UStructProperty>(Property))!=NULL )
	{
		// Expand struct.
		for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(StructProperty->Struct); It; ++It )
			if( AcceptFlags( It->PropertyFlags ) )
				Children.AddItem( new(TEXT("FPropertyItem"))FPropertyItem( OwnerProperties, this, *It, It->GetFName(), It->Offset, -1, 666 ) );
	}
	else if((ObjectProperty=Cast<UObjectProperty>(Property))!=NULL )
	{
		BYTE* ReadValue = GetReadAddress( this );
		UObject* O = *(UObject**)ReadValue;
		if( O )
		{
			FObjectsItem* ObjectsItem = new(TEXT("FObjectsItem"))FObjectsItem( OwnerProperties, this, CPF_Edit, O->GetFullName(), 1, EditInlineNotify );
			ObjectsItem->SetObjects( &O, 1 );
			Children.AddItem( ObjectsItem );
		}
		else
			Children.AddItem( new(TEXT("FNewObjectItem"))FNewObjectItem( OwnerProperties, this, ObjectProperty ) );
	}
	FTreeItem::Expand();
	unguard;
}




/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

