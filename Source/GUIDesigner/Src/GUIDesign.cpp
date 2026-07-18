/*=============================================================================
	GUIDesign.cpp: GUI WYSIWYG designer
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Ron Prestenback
=============================================================================*/

#pragma warning( disable : 4201 )
#define STRICT
#pragma pack(push,8)
#include <windows.h>
#include <commctrl.h>
#include <shlobj.h>
#pragma pack(pop)

#include "XInterface.h"
#include "GUIDesign.h"

IMPLEMENT_PACKAGE(GUIDesigner);


// =======================================================================================================================================================
// =======================================================================================================================================================
// UPropertyManager
// =======================================================================================================================================================
// =======================================================================================================================================================
IMPLEMENT_CLASS(UPropertyManager);

/*
	void SetWindow( WObjectProperties* InWindow );

public:
	PTRINT Snoop, Hook, CurWindow;

	void SetParent( UGUIController* InParent );
	void SetCurrent( UObject** InCurrent );
	void SetWindow( void* InWindow );

	void Show(UBOOL bVisible);
	UBOOL IsVisible();

	FPropertyItem* GetSelectedItem();

	void* GetSnoop();
	void* GetHook();
	void* GetWindow();

*/

void UPropertyManager::SetWindow( WObjectProperties* InWindow )
{
	guard(UPropertyManager::SetWindow);

	CurWindow = InWindow;
	if ( InWindow ) 
	{
		InWindow->SetNotifyHook( GetNotifyHook() );
		InWindow->Snoop = GetControlSnoop();
	}

	unguard;
}


void UPropertyManager::SetParent( UGUIController* InParent )
{
	guard(UPropertyManager::SetParent);

	Parent = InParent;
	unguard;
}

void UPropertyManager::SetCurrent( UObject** InCurrent )
{
	guard(UPropertyManager::SetCurrent);

	WObjectProperties* Window = GetCurrentWindow();

	if ( Window && InCurrent )
	{
		Window->Root.SetObjects( InCurrent, 1 );
		if ( !Window->bShow )
			Window->WWindow::Show(1);
	}

	else if ( Window && Window->bShow )
		Window->WWindow::Show(0);

	unguard;
}

void UPropertyManager::SetWindow( void* InWindow )
{
	guard(UPropertyManager::SetWindow);

	if ( InWindow )
	{
		WObjectProperties* Window = GetCurrentWindow();
		Window->OpenWindow( (HWND)InWindow );
		Window->Show(1);
	}

	unguard;
}

void UPropertyManager::Show( UBOOL bIsVisible )
{
	guard(UPropertyManager::Show);

	GetCurrentWindow()->WWindow::Show(bIsVisible);
	unguard;
}

UBOOL UPropertyManager::IsVisible()
{
	guard(UPropertyManager::IsVisible);

	if ( !CurWindow )
		return 0;

	return GetCurrentWindow()->bShow;

	unguard;
}

FPropertyItem* UPropertyManager::GetSelectedItem()
{
	guard(UPropertyManager::GetSelectedItem);

	if ( !CurWindow )
		return NULL;

	WObjectProperties* Window = GetCurrentWindow();
	FTreeItem* Focused = Window->FocusItem;
	while ( Focused && !Focused->IsPropertyItem() )
		Focused = Focused->Parent;

	if ( Focused )
		return (FPropertyItem*)Focused;

	return NULL;
	unguard;
}

FPropertyManagerSnoop* UPropertyManager::GetControlSnoop()
{
	guard(UPropertyManager::GetSnoop);
	if ( !Snoop )
		Snoop = new FPropertyManagerSnoop(this);

	return Snoop;
	unguard;
}

FPropertyManagerHook* UPropertyManager::GetNotifyHook()
{
	guard(UPropertyManager::GetNotifyHook);

	if ( !Hook )
		Hook = new FPropertyManagerHook(this);


	return Hook;

	unguard;
}

WObjectProperties* UPropertyManager::GetCurrentWindow()
{
	guard(UPropertyManager::GetCurrentWindow);

	if ( !CurWindow )
		SetWindow( new WObjectProperties(TEXT("GUIDesigner"), CPF_Edit, TEXT(""), NULL, 1) );

	check(CurWindow);

	return CurWindow;
	unguard;
}

// FNotifyHook Interface
// Window was closed
void UPropertyManager::NotifyDestroy( void* Src )
{
	guard(UPropertyManager::NotifyDestroy);

	if ( CurWindow == NULL || Src == GetCurrentWindow() )
	{
		if ( Parent )
			Parent->Designer = NULL;

		CurWindow = NULL;
	}

	unguard;
}

void UPropertyManager::NotifyPreChange( void* Src )
{
	guard(UPropertyManager::NotifyPreChange);

	unguard;
}

void UPropertyManager::NotifyPostChange( void* Src )
{
	guard(UPropertyManager::NotifyPostChange);

	if ( CurWindow && Src == GetCurrentWindow() )
	{
		FPropertyItem* Item = GetSelectedItem();
		if ( Item )
		{
			TCHAR Str[4096]=TEXT("");
			Item->GetPropertyText(Str);
			debugf(TEXT("NotifyPostChange() Item '%s' Value '%s'"), Item->Property->GetName(), Str);
		}
	}

	unguard;
}

void UPropertyManager::NotifyExec( void* Src, const TCHAR* Cmd )
{
	guard(UPropertyManager::NotifyExec);

	debugf(TEXT("UPropertyManager::NotifyExec '%s'"), Cmd);
	if ( ParseCommand(&Cmd, TEXT("NEWOBJECT")) )
	{
		FTreeItem* Dest = (FTreeItem*)Src;
		UClass* Cls;
		ParseObject( Cmd, TEXT("CLASS="), Cls, ANY_PACKAGE );
		UObject* Outer;
		ParseObject( Cmd, TEXT("OUTER="), Outer, NULL );
		if( Cls && Outer )
		{
			UGUIComponent* NewObject = Cast<UGUIComponent>(StaticConstructObject(Cls, Outer, NAME_None, RF_Public|((Cls->ClassFlags&CLASS_Localized) ? RF_PerObjectLocalized : 0) ));
			if( NewObject )
				Dest->SetValue( NewObject->GetPathName() );
		}
	}

	else if ( ParseCommand(&Cmd,TEXT("USECURRENT")) )
	{
		FPropertyItem* Dest = GetSelectedItem();
		if ( Dest )
		{
			UObjectProperty* Prop = Cast<UObjectProperty>(Dest->Property);
			if ( Prop )
			{
				UGUIComponent* OwnerObj = Cast<UGUIComponent>(Dest->GetParentObject()), *Obj = *(UGUIComponent**)Dest->GetContents((BYTE*)OwnerObj);
				if ( Obj && !Obj->Controller )
					Obj->eventInitComponent(Parent,OwnerObj);
			}
		}
	}

	else if ( ParseCommand(&Cmd,TEXT("BROWSECLASS")) )
	{
		FPropertyItem* Dest = GetSelectedItem();
		if ( Dest )
		{
			UObjectProperty* Prop = Cast<UObjectProperty>(Dest->Property);
			if ( Prop )
			{
				UObject* OwnerObj = Dest->GetParentObject(), *Obj = *(UObject**)Dest->GetContents((BYTE*)OwnerObj);
				if ( Obj )
				{
					for ( FObjectIterator It; It; ++It )
						It->ClearFlags(RF_TagImp|RF_TagExp);

					UExporter::ExportToFile( Obj, NULL, TEXT("GUIDesigner.t3d"), 1 );
				}
			}
		}
	}

	unguard;
}


// FControlSnoop Interface
void UPropertyManager::SnoopChar( WWindow* Src, INT Char )
{
	guard(UPropertyManager::SnoopChar);

	unguard;
}

void UPropertyManager::SnoopKeyDown( WWindow* Src, INT Char )
{
	guard(UPropertyManager::SnoopKeyDown);

	unguard;
}

void UPropertyManager::SnoopLeftMouseDown( WWindow* Src, FPoint P )
{
	guard(UPropertyManager::SnoopLeftMouseDown);

	unguard;
}

void UPropertyManager::SnoopRightMouseDown( WWindow* Src, FPoint P )
{
	guard(UPropertyManager::SnoopRightMouseDown);
/*
	HMENU menu = LoadMenuIdX(hInstance, 107),
		submenu = GetSubMenu( menu, 0 );

	// Customize the menu options we need to.
	MENUITEMINFOA mif;
	char Buffer[255];

	mif.cbSize = sizeof(MENUITEMINFO);
	mif.fMask = MIIM_TYPE;
	mif.fType = MFT_STRING;

	FGetInfoRet gir = GetInfo( GUnrealEd->Level, GI_NUM_SELECTED | GI_CLASSNAME_SELECTED | GI_CLASS_SELECTED );

	sprintf( Buffer, "%s &Properties (%i Selected)", TCHAR_TO_ANSI( *gir.String ), gir.iValue );
	mif.dwTypeData = Buffer;
	SetMenuItemInfoA( submenu, IDMENU_ActorPopupProperties, FALSE, &mif );

	sprintf( Buffer, "&Select All %s", TCHAR_TO_ANSI( *gir.String ) );
	mif.dwTypeData = Buffer;
	SetMenuItemInfoA( submenu, IDMENU_ActorPopupSelectAllClass, FALSE, &mif );

	EnableMenuItem( submenu, IDMENU_ActorPopupEditScript, (gir.pClass == NULL) );
	EnableMenuItem( submenu, IDMENU_ActorPopupMakeCurrent, (gir.pClass == NULL) );

	if( Command == WM_EDC_RTCLICKACTORSTATICMESH)
	{
		if( GUnrealEd->CurrentClass )
		{
			sprintf( Buffer, "&Add %s Here", TCHAR_TO_ANSI( GUnrealEd->CurrentClass->GetName() ) );
			mif.dwTypeData = Buffer;
			SetMenuItemInfoA( submenu, ID_SurfPopupAddClass, FALSE, &mif );
		}
		else
			DeleteMenu( submenu, ID_SurfPopupAddClass, MF_BYCOMMAND );

		if( GUnrealEd->CurrentStaticMesh )
		{
			sprintf( Buffer, "&Add Static Mesh: '%s'", TCHAR_TO_ANSI( GUnrealEd->CurrentStaticMesh->GetPathName() ) );
			mif.dwTypeData = Buffer;
			SetMenuItemInfoA( submenu, ID_BackdropPopupAddStaticMeshHere, FALSE, &mif );
		}
		else
			DeleteMenu( submenu, ID_BackdropPopupAddStaticMeshHere, MF_BYCOMMAND );

		// Add/remove 'Add Karma Actor' menu item.
		if( GUnrealEd->CurrentStaticMesh  && GUnrealEd->CurrentStaticMesh->KPhysicsProps )
		{
			mif.dwTypeData = "Add Karma Actor";
			SetMenuItemInfoA( submenu, ID_BackdropPopupAddKActorHere, FALSE, &mif );
		}
		else
			DeleteMenu( submenu, ID_BackdropPopupAddKActorHere, MF_BYCOMMAND );					

		if( GCurrentPrefab )
		{
			sprintf( Buffer, "&Add Prefab: '%s'", TCHAR_TO_ANSI( GCurrentPrefab->GetPathName() ) );
			mif.dwTypeData = Buffer;
			SetMenuItemInfoA( submenu, ID_BackdropPopupAddPrefabHere, FALSE, &mif );
		}
		else
			DeleteMenu( submenu, ID_BackdropPopupAddPrefabHere, MF_BYCOMMAND );

		// Insert user defined items into the menu
		mif.fType = MFT_STRING;
		mif.fMask = MIIM_TYPE | MIIM_ID;
		for( INT x = 0 ; x < GActorPopupItems.Num() ; ++x )
		{
			sprintf( Buffer, "Add %s Here", TCHAR_TO_ANSI( *GActorPopupItems(x).Desc ) );
			mif.dwTypeData = Buffer;
			mif.wID = GActorPopupItems(x).ID;
			InsertMenuItemA( submenu, ID_SurfPopupAddLight, FALSE, &mif );
		}
	}
	else
	{
		DeleteMenu( submenu, ID_SurfPopupAddClass, MF_BYCOMMAND );
		DeleteMenu( submenu, ID_BackdropPopupAddStaticMeshHere, MF_BYCOMMAND );
		DeleteMenu( submenu, ID_BackdropPopupAddPrefabHere, MF_BYCOMMAND );
		DeleteMenu( submenu, ID_SurfPopupAddLight, MF_BYCOMMAND );
		DeleteMenu( submenu, ID_BackdropPopupAddKActorHere, MF_BYCOMMAND );					
	}

	TrackPopupMenu( submenu,
		TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
		P.X, P.Y, 0,
		CurrentWindow->hWnd, NULL);
	DestroyMenu( menu );

*/	debugf(TEXT("SnoopRightMouseDown X:%i Y:%i"), P.X, P.Y);

	unguard;
}

TCHAR* GUIGetPropFlags( const UProperty* Prop )
{
	guard(GetFlags);

	if ( !Prop )
		return NULL;

	TCHAR* c = appStaticString1024();

	DWORD Flags = Prop->PropertyFlags;
	if ( Flags & CPF_Edit) addflag(CPF_Edit);
	if ( Flags & CPF_Const) addflag(CPF_Const);
	if ( Flags & CPF_Input) addflag(CPF_Input);
	if ( Flags & CPF_ExportObject) addflag(CPF_ExportObject);
	if ( Flags & CPF_OptionalParm) addflag(CPF_OptionalParm);
	if ( Flags & CPF_Net) addflag(CPF_Net);
	if ( Flags & CPF_EditConstArray) addflag(CPF_EditConstArray);
	if ( Flags & CPF_Parm) addflag(CPF_Parm);
	if ( Flags & CPF_OutParm) addflag(CPF_OutParm);
	if ( Flags & CPF_SkipParm) addflag(CPF_SkipParm);
	if ( Flags & CPF_ReturnParm) addflag(CPF_ReturnParm);
	if ( Flags & CPF_CoerceParm) addflag(CPF_CoerceParm);
	if ( Flags & CPF_Native) addflag(CPF_Native);
	if ( Flags & CPF_Transient) addflag(CPF_Transient);
	if ( Flags & CPF_Config) addflag(CPF_Config);
	if ( Flags & CPF_Localized) addflag(CPF_Localized);
	if ( Flags & CPF_Travel) addflag(CPF_Travel);
	if ( Flags & CPF_EditConst) addflag(CPF_EditConst);
	if ( Flags & CPF_GlobalConfig) addflag(CPF_GlobalConfig);
	if ( Flags & CPF_OnDemand) addflag(CPF_OnDemand);
	if ( Flags & CPF_New) addflag(CPF_New);
	if ( Flags & CPF_NeedCtorLink) addflag(CPF_NeedCtorLink);
	if ( Flags & CPF_NoExport) addflag(CPF_NoExport);
	if ( Flags & CPF_Button) addflag(CPF_Button);
	if ( Flags & CPF_CommentString) addflag(CPF_CommentString);
	if ( Flags & CPF_EditInline) addflag(CPF_EditInline);
	if ( Flags & CPF_EdFindable) addflag(CPF_EdFindable);
	if ( Flags & CPF_EditInlineUse) addflag(CPF_EditInlineUse);
	if ( Flags & CPF_Deprecated) addflag(CPF_Deprecated);
	if ( Flags & CPF_EditInlineNotify) addflag(CPF_EditInlineNotify);
	if ( Flags & CPF_Automated) addflag(CPF_Automated);
	if ( Flags & CPF_ParmFlags) addflag(CPF_ParmFlags);
	if ( Flags & CPF_PropagateFromStruct) addflag(CPF_PropagateFromStruct);
	if ( Flags & CPF_PropagateToArrayInner) addflag(CPF_PropagateToArrayInner);

	return c;

	unguard;
}
