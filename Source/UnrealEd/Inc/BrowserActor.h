/*=============================================================================
	BrowserActor : Browser window for actor classes
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

FString GetClassFlagsText( DWORD InClassFlags )
{
	FString Ret;

	char ch13 = '\x0d', ch10 = '\x0a';

	if( InClassFlags & CLASS_Abstract )
		Ret += FString::Printf( TEXT("%c%c\tabstract"), ch13, ch10 );
	if( InClassFlags & CLASS_NoExport )
		Ret += FString::Printf( TEXT("%c%c\tnoexport"), ch13, ch10 );
	if( InClassFlags & CLASS_Placeable )
		Ret += FString::Printf( TEXT("%c%c\tplaceable"), ch13, ch10 );
	if( InClassFlags & CLASS_PerObjectConfig )
		Ret += FString::Printf( TEXT("%c%c\tperobjectconfig"), ch13, ch10 );
	if( InClassFlags & CLASS_NativeReplication )
		Ret += FString::Printf( TEXT("%c%c\tnativereplication"), ch13, ch10 );
	if( InClassFlags & CLASS_EditInlineNew )
		Ret += FString::Printf( TEXT("%c%c\teditinlinenew"), ch13, ch10 );
	if( InClassFlags & CLASS_CollapseCategories )
		Ret += FString::Printf( TEXT("%c%c\tcollapsecategories"), ch13, ch10 );

	return Ret;
}

FString GetDefaultScriptText( FString InClass, FString InParentClass, DWORD InParentClassFlags )
{
	char ch13 = '\x0d', ch10 = '\x0a';

	FString DefScript = FString::Printf(
		TEXT("//=============================================================================%c%c// %s.%c%c//=============================================================================%c%cclass %s extends %s%s;%c%c"),
		ch13, ch10,
		*InClass, ch13, ch10,
		ch13, ch10,
		*InClass, *InParentClass, *GetClassFlagsText( InParentClassFlags ), ch13, ch10 );

	return DefScript;
}

static int CDECL ClassSortCompare( const void *elem1, const void *elem2 )
{
	return appStrcmp((*(UClass**)elem1)->GetName(),(*(UClass**)elem2)->GetName());
}

// --------------------------------------------------------------
//
// NEW CLASS Dialog
//
// --------------------------------------------------------------

class WDlgNewClass : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgNewClass,WDialog,UnrealEd)

	// Variables.
	WButton OkButton;
	WButton CancelButton;
	WLabel ParentLabel;
	WEdit PackageEdit;
	WEdit NameEdit;

	FString ParentClass, Package, Name;

	// Constructor.
	WDlgNewClass( UObject* InContext, WWindow* InOwnerWindow )
	:	WDialog			( TEXT("New Class"), IDDIALOG_NEW_CLASS, InOwnerWindow )
	,	OkButton		( this, IDOK,			FDelegate(this,(TDelegate)OnOk) )
	,	CancelButton	( this, IDCANCEL,		FDelegate(this,(TDelegate)OnCancel) )
	,	ParentLabel		( this, IDSC_PARENT )
	,	PackageEdit		( this, IDEC_PACKAGE )
	,	NameEdit		( this, IDEC_NAME )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgNewClass::OnInitDialog);
		WDialog::OnInitDialog();

		ParentLabel.SetText( *ParentClass );
		PackageEdit.SetText( TEXT("MyPackage") );
		NameEdit.SetText( *FString::Printf(TEXT("My%s"), *ParentClass) );
		::SetFocus( PackageEdit.hWnd );

		unguard;
	}
	virtual INT DoModal( FString InParentClass )
	{
		guard(WDlgNewClass::DoModal);

		ParentClass = InParentClass;

		return WDialog::DoModal( hInstance );
		unguard;
	}
	void OnOk()
	{
		guard(WDlgNewClass::OnOk);
		if( GetDataFromUser() )
		{
			// Create new class.

			GUnrealEd->Exec( *FString::Printf( TEXT("CLASS NEW NAME=\"%s\" PACKAGE=\"%s\" PARENT=\"%s\""),
				*Name, *Package, *ParentClass));
			GUnrealEd->Exec( *FString::Printf(TEXT("SETCURRENTCLASS CLASS=\"%s\""), *Name));

			// Create standard header for the new class.

			GUnrealEd->Set(TEXT("SCRIPT"), *Name, *GetDefaultScriptText( Name, ParentClass, CLASS_Placeable ) );

			EndDialog(TRUE);
		}
		unguard;
	}
	BOOL GetDataFromUser( void )
	{
		guard(WDlgNewClass::GetDataFromUser);
		Package = PackageEdit.GetText();
		Name = NameEdit.GetText();

		if( !Package.Len()
				|| !Name.Len() )
		{
			appMsgf( 0,TEXT("Invalid input.") );
			return FALSE;
		}
		else
			return TRUE;
		unguard;
	}
	void OnCancel()
	{
		guard(WDlgNewClass::OnCancel);
		EndDialog(FALSE);
		unguard;
	}
};

// --------------------------------------------------------------
//
// WBrowserActor
//
// --------------------------------------------------------------

#define ID_BA_TOOLBAR	29030
TBBUTTON tbBAButtons[] = {
	{ 0, IDMN_MB_DOCK, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 1, IDMN_AB_FileOpen, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 2, IDMN_AB_FileSave, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 3, IDMN_AB_NEW_CLASS, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 4, IDMN_AB_EDIT_SCRIPT, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 5, IDMN_AB_DEF_PROP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
};
struct {
	TCHAR ToolTip[64];
	INT ID;
} ToolTips_BA[] = {
	TEXT("Toggle Dock Status"), IDMN_MB_DOCK,
	TEXT("Open Package"), IDMN_AB_FileOpen,
	TEXT("Save Selected Packages"), IDMN_AB_FileSave,
	TEXT("New Script"), IDMN_AB_NEW_CLASS,
	TEXT("Edit Script"), IDMN_AB_EDIT_SCRIPT,
	TEXT("Edit Default Properties"), IDMN_AB_DEF_PROP,
	NULL, 0
};

class WBrowserActor : public WBrowser
{
	DECLARE_WINDOWCLASS(WBrowserActor,WBrowser,Window)

	TMap<DWORD,FWindowAnchor> Anchors;
	TMap<DWORD,FWindowAnchor> AnchorsPackages;

	FContainer *Container;
	WTreeView* TreeView;
	WComboBox* PackagesCombo;
	WCheckBox *ObjectsCheck, *PlaceableCheck;
	WCheckListBox* PackagesList;
	HTREEITEM htiRoot, htiLastSel;
	HWND hWndToolBar;
	WToolTip* ToolTipCtrl;

	UBOOL bShowPackages;

	// Structors.
	WBrowserActor( FName InPersistentName, WWindow* InOwnerWindow, HWND InEditorFrame )
	:	WBrowser( InPersistentName, InOwnerWindow, InEditorFrame )
	{
		Container = NULL;
		TreeView = NULL;
		ObjectsCheck = PlaceableCheck = NULL;
		PackagesCombo = NULL;
		htiRoot = htiLastSel = NULL;
		MenuID = IDMENU_BrowserActor;
		BrowserID = eBROWSER_ACTOR;
		Description = TEXT("Actor Classes");
	}

	// WBrowser interface.
	void OpenWindow( UBOOL bChild )
	{
		guard(WBrowserActor::OpenWindow);
		WBrowser::OpenWindow( bChild );
		SetCaption();
		unguard;
	}
	virtual void SetCaption( FString* Tail = NULL )
	{
		guard(WBrowserActor::SetCaption);

		FString Extra;
		if( GUnrealEd->CurrentClass )
		{
			Extra = GUnrealEd->CurrentClass->GetFullName();
			Extra = Extra.Right( Extra.Len() - 6 );	// remove "class" from the front of it
		}

		WBrowser::SetCaption( &Extra );
		unguard;
	}
	void OnCreate()
	{
		guard(WBrowserActor::OnCreate);
		WBrowser::OnCreate();

		SetMenu( hWnd, LoadMenuIdX(hInstance, IDMENU_BrowserActor) );
		
		Container = new FContainer();

		PackagesCombo = new WComboBox( this, IDCB_PACKAGE );
		PackagesCombo->OpenWindow( 0, 1 ); // !!this is invisible temporarily
		PackagesCombo->SelectionChangeDelegate = FDelegate(this, (TDelegate)OnPackagesComboSelChange);

		ObjectsCheck = new WCheckBox( this, IDCK_OBJECTS );
		ObjectsCheck->ClickDelegate = FDelegate(this, (TDelegate)OnObjectsClick);
		ObjectsCheck->OpenWindow( 1, 0, 0, 1, 1, TEXT("Use 'Actor' as Parent?") );
		SendMessageX( ObjectsCheck->hWnd, BM_SETCHECK, BST_CHECKED, 0 );

		PlaceableCheck = new WCheckBox( this, IDCK_OBJECTS );
		PlaceableCheck->ClickDelegate = FDelegate(this, (TDelegate)OnPlaceableClick);
		PlaceableCheck->OpenWindow( 1, 0, 0, 1, 1, TEXT("Placeable classes Only?") );
		SendMessageX( PlaceableCheck->hWnd, BM_SETCHECK, BST_CHECKED, 0 );

		TreeView = new WTreeView( this, IDTV_TREEVIEW );
		TreeView->OpenWindow( 1, 1, 0, 0, 1 );
		TreeView->SelChangedDelegate = FDelegate(this, (TDelegate)OnTreeViewSelChanged);
		TreeView->ItemExpandingDelegate = FDelegate(this, (TDelegate)OnTreeViewItemExpanding);
		TreeView->DblClkDelegate = FDelegate(this, (TDelegate)OnTreeViewDblClk);
		
		PackagesList = new WCheckListBox( this, IDLB_PACKAGES );
		PackagesList->OpenWindow( 1, 0, 0, 1 );

		if(!GConfig->GetInt( *PersistentName, TEXT("ShowPackages"), bShowPackages, TEXT("UnrealEd.ini") ))		bShowPackages = 1;
		UpdateMenu();

		hWndToolBar = CreateToolbarEx( 
			hWnd, WS_CHILD | WS_BORDER | WS_VISIBLE | CCS_ADJUSTABLE,
			IDB_BrowserActor_TOOLBAR,
			6,
			hInstance,
			IDB_BrowserActor_TOOLBAR,
			(LPCTBBUTTON)&tbBAButtons,
			8,
			16,16,
			16,16,
			sizeof(TBBUTTON));
		check(hWndToolBar);

		ToolTipCtrl = new WToolTip(this);
		ToolTipCtrl->OpenWindow();
		for( INT tooltip = 0 ; ToolTips_BA[tooltip].ID > 0 ; ++tooltip )
		{
			// Figure out the rectangle for the toolbar button.
			INT index = SendMessageX( hWndToolBar, TB_COMMANDTOINDEX, ToolTips_BA[tooltip].ID, 0 );
			RECT rect;
			SendMessageX( hWndToolBar, TB_GETITEMRECT, index, (LPARAM)&rect);

			ToolTipCtrl->AddTool( hWndToolBar, ToolTips_BA[tooltip].ToolTip, tooltip, &rect );
		}

		INT Top = 0;
		Anchors.Set( (DWORD)hWndToolBar, FWindowAnchor( hWnd, hWndToolBar,							ANCHOR_TL, 0, 0,						ANCHOR_RIGHT|ANCHOR_HEIGHT, 0, STANDARD_TOOLBAR_HEIGHT ) );
		Top += STANDARD_TOOLBAR_HEIGHT+4;
		Anchors.Set( (DWORD)ObjectsCheck->hWnd, FWindowAnchor( hWnd, ObjectsCheck->hWnd,			ANCHOR_TL, 4, Top,						ANCHOR_RIGHT|ANCHOR_HEIGHT, -4, STANDARD_CTRL_HEIGHT ) );
		Top += STANDARD_CTRL_HEIGHT+2;
		Anchors.Set( (DWORD)PlaceableCheck->hWnd, FWindowAnchor( hWnd, PlaceableCheck->hWnd,		ANCHOR_TL, 4, Top,						ANCHOR_RIGHT|ANCHOR_HEIGHT, -4, STANDARD_CTRL_HEIGHT ) );
		Top += STANDARD_CTRL_HEIGHT+4+2;
		Anchors.Set( (DWORD)TreeView->hWnd, FWindowAnchor( hWnd, TreeView->hWnd,					ANCHOR_TL, 4, Top,						ANCHOR_BR, -4, -4 ) );
		Anchors.Set( (DWORD)PackagesList->hWnd, FWindowAnchor( hWnd, PackagesList->hWnd,			ANCHOR_LEFT|ANCHOR_BOTTOM, 0, 0,		ANCHOR_WIDTH|ANCHOR_HEIGHT, 1, 1 ) );

		Top = 0;
		AnchorsPackages.Set( (DWORD)hWndToolBar, FWindowAnchor( hWnd, hWndToolBar,					ANCHOR_TL, 0, 0,						ANCHOR_RIGHT|ANCHOR_HEIGHT, 0, STANDARD_TOOLBAR_HEIGHT ) );
		Top += STANDARD_TOOLBAR_HEIGHT+4;
		AnchorsPackages.Set( (DWORD)ObjectsCheck->hWnd, FWindowAnchor( hWnd, ObjectsCheck->hWnd,	ANCHOR_TL, 4, Top,						ANCHOR_RIGHT|ANCHOR_HEIGHT, -4, STANDARD_CTRL_HEIGHT ) );
		Top += STANDARD_CTRL_HEIGHT+4+2;
		Anchors.Set( (DWORD)PlaceableCheck->hWnd, FWindowAnchor( hWnd, PlaceableCheck->hWnd,		ANCHOR_TL, 4, Top,						ANCHOR_RIGHT|ANCHOR_HEIGHT, -4, STANDARD_CTRL_HEIGHT ) );
		Top += STANDARD_CTRL_HEIGHT+4+2;
		AnchorsPackages.Set( (DWORD)TreeView->hWnd, FWindowAnchor( hWnd, TreeView->hWnd,			ANCHOR_TL, 4, Top,						ANCHOR_BR, -4, -4-192-2 ) );
		AnchorsPackages.Set( (DWORD)PackagesList->hWnd, FWindowAnchor( hWnd, PackagesList->hWnd,	ANCHOR_LEFT|ANCHOR_BOTTOM, 4, -4-192,	ANCHOR_RIGHT|ANCHOR_HEIGHT, -4, 192 ) );

		Container->SetAnchors( bShowPackages ? &AnchorsPackages : &Anchors );

		PositionChildControls();
		RefreshPackages();
		RefreshActorList();
		RefreshPackagesCombo();
		SendMessageX( TreeView->hWnd, TVM_EXPAND, TVE_EXPAND, (LPARAM)htiRoot );

		unguard;
	}
	virtual void UpdateMenu()
	{
		guard(WBrowserActor::UpdateMenu);

		HMENU menu = IsDocked() ? GetMenu( OwnerWindow->hWnd ) : GetMenu( hWnd );

		CheckMenuItem( menu, IDMN_AB_SHOWPACKAGES, MF_BYCOMMAND | (bShowPackages ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_MB_DOCK, MF_BYCOMMAND | (IsDocked() ? MF_CHECKED : MF_UNCHECKED) );

		unguard;
	}
	void RefreshPackagesCombo( void )
	{
		guard(WBrowserActor::RefreshPackagesCombo);

		INT Current = PackagesCombo->GetCurrent() == CB_ERR ? 0 : PackagesCombo->GetCurrent();

		FStringOutputDevice GetPropResult;
	    GUnrealEd->Get(TEXT("OBJ"), TEXT("PACKAGES CLASS=Class"), GetPropResult);

		TArray<FString> PkgArray;
		GetPropResult.ParseIntoArray( TEXT(","), &PkgArray );

		PackagesCombo->Empty();

		for( INT x = 0 ; x < PkgArray.Num() ; ++x )
			PackagesCombo->AddString( *(PkgArray(x)) );

		PackagesCombo->SetCurrent( Current );

		unguard;
	}
	void RefreshPackages(void)
	{
		guard(WBrowserActor::RefreshPackages);

		FStringOutputDevice GetPropResult = FStringOutputDevice();
	    GUnrealEd->Get(TEXT("OBJ"), TEXT("PACKAGES CLASS=Class"), GetPropResult);

		TArray<FString> PkgArray;
		GetPropResult.ParseIntoArray( TEXT(","), &PkgArray );

		PackagesList->Empty();

		for( INT x = 0 ; x < PkgArray.Num() ; ++x )
			PackagesList->AddString( *FString::Printf( TEXT("%s"), *PkgArray(x)));

		unguard;
	}
	void OnDestroy()
	{
		guard(WBrowserActor::OnDestroy);

		delete Container;
		delete TreeView;
		delete PackagesCombo;
		delete ObjectsCheck;
		delete PackagesList;

		::DestroyWindow( hWndToolBar );
		delete ToolTipCtrl;

		GConfig->SetInt( *PersistentName, TEXT("ShowPackages"), bShowPackages, TEXT("UnrealEd.ini") );

		WBrowser::OnDestroy();
		unguard;
	}

	void RemoveActorReferences( UStruct* Struct, BYTE* Base )
	{
		guard(RemoveActorReferences);
		for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Struct); It; ++It )
		{
			UObjectProperty* ObjectProperty;
			UArrayProperty* ArrayProperty;
			UStructProperty* StructProperty;

			if( (ObjectProperty=Cast<UObjectProperty>(*It)) != NULL )
			{
				if( Cast<AActor>(*(UObject**)(Base+It->Offset)) )
				{
					debugf(TEXT("Zeroing out actor %s"), (*(UObject**)(Base+It->Offset))->GetName() );
					*(UObject**)(Base+It->Offset) = NULL;						
				}
			}
			else
			if( (ArrayProperty=Cast<UArrayProperty>(*It)) != NULL )
			{
				FArray* Array = (FArray*)(Base+It->Offset);
				if( ArrayProperty->Inner->IsA(UObjectProperty::StaticClass()) )
				{
					for( INT i=0;i<Array->Num();i++ )
					{
						UObject** Obj = (UObject**)( (BYTE*)Array->GetData() + i * ArrayProperty->Inner->ElementSize );
						if( (*Obj)->IsA(AActor::StaticClass()) )
						{
							debugf(TEXT("Zeroing out array item actor %s"), (*Obj)->GetName() );
							*Obj = NULL;
						}
					}
				}
				else
				if( ArrayProperty->Inner->IsA(UStructProperty::StaticClass()) )
				{
					for( INT i=0;i<Array->Num();i++ )
						RemoveActorReferences(  Cast<UStructProperty>(ArrayProperty->Inner)->Struct, (BYTE*)Array->GetData() + i * ArrayProperty->Inner->ElementSize );
				}
			}
			else
			if( (StructProperty=Cast<UStructProperty>(*It)) != NULL )
			{
				RemoveActorReferences( StructProperty->Struct, Base + StructProperty->Offset );
			}
		}
		unguard;
	}
	void OnCommand( INT Command )
	{
		guard(WBrowserActor::OnCommand);
		switch( Command )
		{
			case WM_TREEVIEW_RIGHT_CLICK:
				{
					// Select the tree item underneath the mouse cursor.
					TVHITTESTINFO tvhti;
					POINT ptScreen;
					::GetCursorPos( &ptScreen );
					tvhti.pt = ptScreen;
					::ScreenToClient( TreeView->hWnd, &tvhti.pt );

					SendMessageX( TreeView->hWnd, TVM_HITTEST, 0, (LPARAM)&tvhti);

					if( tvhti.hItem )
						SendMessageX( TreeView->hWnd, TVM_SELECTITEM, TVGN_CARET, (LPARAM)(HTREEITEM)tvhti.hItem);

					// Show a context menu for the currently selected item.
					HMENU menu = LoadMenuIdX(hInstance, IDMENU_BrowserActor_Context),
						submenu = GetSubMenu( menu, 0 );
					TrackPopupMenu( submenu,
						TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
						ptScreen.x, ptScreen.y, 0,
						hWnd, NULL);
					DestroyMenu( menu );
				}
				break;

			case IDMN_AB_EXPORT_ALL:
				{
					if( ::MessageBox( hWnd, TEXT("This option will export all classes to text .uc files which can later be rebuilt. Do you want to do this?"), TEXT("Export classes to *.uc files"), MB_YESNO) == IDYES)
					{
						GUnrealEd->Exec( TEXT("CLASS SPEW ALL") );
					}
				}
				break;

			case IDMN_AB_EXPORT:
				{
					if( ::MessageBox( hWnd, TEXT("This option will export all modified classes to text .uc files which can later be rebuilt. Do you want to do this?"), TEXT("Export classes to *.uc files"), MB_YESNO) == IDYES)
					{
						GUnrealEd->Exec( TEXT("CLASS SPEW") );
					}
				}
				break;

			case IDMN_AB_SHOWPACKAGES:
				{
					bShowPackages = !bShowPackages;
					Container->SetAnchors( bShowPackages ? &AnchorsPackages : &Anchors );
					PositionChildControls();
					UpdateMenu();
				}
				break;

			case IDMN_AB_NEW_CLASS:
				{
					WDlgNewClass l_dlg( NULL, this );
					if( l_dlg.DoModal( GUnrealEd->CurrentClass ? GUnrealEd->CurrentClass->GetName() : TEXT("Actor") ) )
					{
						// Open an editing window.
						//
						GCodeFrame->AddClass( GUnrealEd->CurrentClass );
						RefreshActorList();
						RefreshPackages();
						RefreshPackagesCombo();
					}
				}
				break;

			case IDMN_AB_NEW_CLASS_FROM_SEL:
			{
				// Find the selected actor

				AActor* Actor = NULL;
				for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
				{
					Actor = Cast<AActor>(GUnrealEd->Level->Actors(i));
					if( Actor && Actor->bSelected )
						break;
				}

				if( !Actor )
				{
					appMsgf(0, TEXT("Select an actor to use as the base class first.") );
					return;
				}	

				// Get input from the user

				WDlgGeneric dlg( NULL, this, OPTIONS_NEWCLASSFROMSEL, TEXT("New Class From Selection") );
				if( dlg.DoModal( TEXT("Creates a new class derived from the selected actor.") ) )
				{
					UOptionsNewClassFromSel* Proxy = Cast<UOptionsNewClassFromSel>(dlg.Proxy);

					// Create the new class, based on the selected actor

					UPackage* Pkg = GUnrealEd->CreatePackage(NULL,*Proxy->Package);
					Pkg->bDirty = 1;
					UClass* Class = new( Pkg, *Proxy->Name, RF_Public|RF_Standalone )UClass( Actor->GetClass() );
					Class->ScriptText = new( Class->GetOuter(), Class->GetName(), RF_NotForClient|RF_NotForServer )UTextBuffer( *GetDefaultScriptText( Class->GetName(), Actor->GetClass()->GetName(), Actor->GetClass()->ClassFlags ) );

					// Set default properties

					FStringOutputDevice Ar;
					UObject::ExportProperties( Ar, Actor->GetClass(), (BYTE*)Actor, 0, Actor->GetClass()->GetSuperClass(), Actor->GetClass()->GetSuperClass() ? (BYTE*)Actor->GetClass()->GetSuperClass()->GetDefaultObject() : NULL );
					ImportProperties( Class, (BYTE*)Class->GetDefaultObject(), GUnrealEd->Level, *Ar, Actor->GetClass(), GWarn, 0 );

					// Remove actor references
					RemoveActorReferences( Class, (BYTE*)Class->GetDefaultObject() );
										

					// Compile changed scripts to get this one to show up correctly in the actor list

					GWarn->BeginSlowTask( TEXT("Compiling changed scripts"), 1 );
					GUnrealEd->Exec( TEXT("SCRIPT MAKE") );
					GWarn->EndSlowTask();

					// Refresh browser

					GUnrealEd->CurrentClass = Class;
					GCodeFrame->AddClass( GUnrealEd->CurrentClass );
					RefreshActorList();
					RefreshPackages();
					RefreshPackagesCombo();
				}
			}
			break;

			case IDMN_AB_DELETE:
				{
					if( GUnrealEd->CurrentClass )
					{
						FString CurName = GUnrealEd->CurrentClass->GetName();
						GUnrealEd->Exec( TEXT("SETCURRENTCLASS Class=Light") );

						FStringOutputDevice GetPropResult = FStringOutputDevice();
						GUnrealEd->Get( TEXT("OBJ"), *FString::Printf( TEXT("DELETE CLASS=CLASS OBJECT=\"%s\""), *CurName ), GetPropResult);

						if( !GetPropResult.Len() )
						{
							// Try to cleanly update the actor list.  If this fails, just reload it from scratch...
							if( !SendMessageX( TreeView->hWnd, TVM_DELETEITEM, 0, (LPARAM)htiLastSel ) )
								RefreshActorList();

							GCodeFrame->RemoveClass( CurName );
						}
						else
							appMsgf( 0, TEXT("Can't delete class") );
					}
				}
				break;

			case IDMN_AB_DEF_PROP:
				GUnrealEd->ShowClassProperties( GUnrealEd->CurrentClass );
				break;

			case IDMN_AB_FileOpen:
				{
					OPENFILENAMEA ofn;
					char File[8192] = "\0";

					ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
					ofn.lStructSize = sizeof(OPENFILENAMEA);
					ofn.hwndOwner = hWnd;
					ofn.lpstrFile = File;
					ofn.nMaxFile = sizeof(char) * 8192;
					ofn.lpstrFilter = "Class Packages (*.u)\0*.u\0All Files\0*.*\0\0";
					ofn.lpstrInitialDir = "..\\system";
					ofn.lpstrDefExt = "u";
					ofn.lpstrTitle = "Open Class Package";
					ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

					if( GetOpenFileNameA(&ofn) )
					{
						INT iNULLs = FormatFilenames( File );
		
						TArray<FString> StringArray;
						FString S = appFromAnsi( File );
						S.ParseIntoArray( TEXT("|"), &StringArray );

						INT iStart = 0;
						FString Prefix = TEXT("\0");

						if( iNULLs )
						{
							iStart = 1;
							Prefix = *(StringArray(0));
							Prefix += TEXT("\\");
						}

						for( INT x = iStart ; x < StringArray.Num() ; ++x )
							GUnrealEd->Exec( *FString::Printf( TEXT("CLASS LOAD FILE=\"%s%s\""), *Prefix, *(StringArray(x)) ) );

						GBrowserMaster->RefreshAll();
                        RefreshActorList(); // gam
						RefreshPackages();
						RefreshPackagesCombo();

					}

					GFileManager->SetDefaultDirectory(appBaseDir());
					RefreshPackages();
					RefreshPackagesCombo();
				}
				break;

			case IDMN_AB_FileSave:
				{
					FString Pkg;

					GWarn->BeginSlowTask( TEXT("Saving Packages"), 1);

					for( INT x = 0 ; x < PackagesList->GetCount() ; ++x )
					{
						if( (int)PackagesList->GetItemData(x) )
						{
							Pkg = *(PackagesList->GetString( x ));
							GUnrealEd->Exec( *FString::Printf(TEXT("OBJ SAVEPACKAGE PACKAGE=\"%s\" FILE=\"%s.u\""), *Pkg, *Pkg ));
						}
					}

					GWarn->EndSlowTask();
				}
				break;

			case IDMN_AB_EDIT_SCRIPT:
				{
					GCodeFrame->AddClass( GUnrealEd->CurrentClass );
				}
				break;

			default:
				WBrowser::OnCommand(Command);
				break;
		}
		unguard;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WBrowserActor::OnSize);
		WBrowser::OnSize(Flags, NewX, NewY);
		PositionChildControls();
		InvalidateRect( hWnd, NULL, FALSE );
		UpdateMenu();
		unguard;
	}
	void PositionChildControls( void )
	{
		guard(WBrowserActor::PositionChildControls);
		if( Container ) Container->RefreshControls();
		unguard;
	}
	virtual void RefreshAll()
	{
		guard(WBrowserActor::RefreshAll);
		// gam ---
		// I removed the call to RefreshActorList() because it was resetting
		// the actor browser far too often.
		// --- gam
		unguard;
	}
	void RefreshActorList( void )
	{
		guard(WBrowserActor::RefreshActorList);
		TreeView->Empty();

		if( ObjectsCheck->IsChecked() )
			htiRoot = TreeView->AddItem( TEXT(" Actor"), NULL, TRUE );
		else
			htiRoot = TreeView->AddItem( TEXT(" Object"), NULL, TRUE );

		unguard;
	}
	UBOOL HasChildren( const UClass* InParent, UBOOL InPlaceableOnly )
	{
		for( TObjectIterator<UClass> It ; It ; ++It )
		{
			if( It->GetSuperClass() == InParent )
				if( !InPlaceableOnly || (It->ClassFlags & CLASS_Placeable) )
					return 1;
				else
					if( HasChildren( *It, InPlaceableOnly ) )
						return 1;
		}

		return 0;
	}
	void AddChildren( const UClass* InParent, HTREEITEM InHTI )
	{
		guard(WBrowserActor::AddChildren);

		enum	{MAX_RESULTS=1024};
		int		NumResults = 0;
		UClass	*Results[MAX_RESULTS];

		FWaitCursor wc;

		check( InParent );

		UBOOL bPlaceableOnly = PlaceableCheck->IsChecked();

		for( TObjectIterator<UClass> It; It && NumResults<MAX_RESULTS; ++It )
			if( It->GetSuperClass()==InParent )
				Results[NumResults++] = *It;

		appQsort( Results, NumResults, sizeof(UClass*), ClassSortCompare );

		for( INT i = 0 ; i < NumResults ; i++ )
		{
			UBOOL bHasChildren = HasChildren( Results[i], bPlaceableOnly );

			if( bPlaceableOnly && !(Results[i]->ClassFlags & CLASS_Placeable) && !bHasChildren )
				continue;

			FString ClassName = FString::Printf( TEXT("%s%s"),
				( Results[i]->ClassFlags & CLASS_Placeable ) ? TEXT("*") : TEXT(" "),
				Results[i]->GetName() );

			TreeView->AddItem( *ClassName, InHTI, bHasChildren );
		}

		unguard;
	}
	void OnPackagesComboSelChange()
	{
		guard(WBrowserActor::OnPackagesComboSelChange);
		RefreshActorList();
		unguard;
	}
	void OnTreeViewSelChanged( void )
	{
		guard(WBrowserActor::OnTreeViewSelChanged);
		NMTREEVIEW* pnmtv = (LPNMTREEVIEW)TreeView->LastlParam;
		TCHAR chText[128] = TEXT("\0");
		TVITEM tvi;

		appMemzero( &tvi, sizeof(tvi));
		htiLastSel = tvi.hItem = pnmtv->itemNew.hItem;
		tvi.mask = TVIF_TEXT;
		tvi.pszText = chText;
		tvi.cchTextMax = sizeof(chText);

		if( SendMessageX( TreeView->hWnd, TVM_GETITEM, 0, (LPARAM)&tvi) )
		{
			FString Classname = tvi.pszText;
			Classname = Classname.Right( Classname.Len()-1 );
			GUnrealEd->Exec( *FString::Printf(TEXT("SETCURRENTCLASS CLASS=\"%s\""), *Classname ));
		}
		SetCaption();
		unguard;
	}
	void OnTreeViewItemExpanding( void )
	{
		guard(WBrowserActor::OnTreeViewItemExpanding);
		NMTREEVIEW* pnmtv = (LPNMTREEVIEW)TreeView->LastlParam;
		TCHAR chText[128] = TEXT("\0");

		TVITEM tvi;

		appMemzero( &tvi, sizeof(tvi));
		tvi.hItem = pnmtv->itemNew.hItem;
		tvi.mask = TVIF_TEXT;
		tvi.pszText = chText;
		tvi.cchTextMax = sizeof(chText);

		// If this item already has children loaded, leave.
		if( SendMessageX( TreeView->hWnd, TVM_GETNEXTITEM, TVGN_CHILD, (LPARAM)pnmtv->itemNew.hItem ) )
			return;

		//UPackage* Pkg = FindObject<UPackage>( ANY_PACKAGE, *Package );
		if( SendMessageX( TreeView->hWnd, TVM_GETITEM, 0, (LPARAM)&tvi) )
			AddChildren( FindObject<UClass>( ANY_PACKAGE, ++tvi.pszText ), pnmtv->itemNew.hItem );
		unguard;
	}
	void OnTreeViewDblClk( void )
	{
		guard(WBrowserActor::OnTreeViewDblClk);
		GCodeFrame->AddClass( GUnrealEd->CurrentClass );
		unguard;
	}
	void OnObjectsClick()
	{
		guard(WBrowserActor::OnObjectsClick);
		RefreshActorList();
		SendMessageX( TreeView->hWnd, TVM_EXPAND, TVE_EXPAND, (LPARAM)htiRoot );
		unguard;
	}
	void OnPlaceableClick()
	{
		guard(WBrowserActor::OnPlaceableClick);
		RefreshActorList();
		SendMessageX( TreeView->hWnd, TVM_EXPAND, TVE_EXPAND, (LPARAM)htiRoot );
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

