/*=============================================================================
	BrowserPrefab : Browser window for prefabs
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

extern void Query( ULevel* Level, const TCHAR* Item, FString* pOutput );
extern FString GLastDir[eLASTDIR_MAX];
extern const DWORD GShowFlags;

#include <math.h>

// --------------------------------------------------------------
//
// NEW PREFAB Dialog
//
// --------------------------------------------------------------

class WDlgNewPrefab : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgNewPrefab,WDialog,UnrealEd)

	// Variables.
	WButton OkButton;
	WButton CancelButton;
	WEdit PackageEdit;
	WEdit GroupEdit;
	WEdit NameEdit;

	FString defPackage, defGroup;
	FString Package, Group, Name;

	// Constructor.
	WDlgNewPrefab( UObject* InContext, WBrowser* InOwnerWindow )
		:	WDialog			( TEXT("New Prefab"), IDDIALOG_NEW_PREFAB, InOwnerWindow )
	,	OkButton		( this, IDOK,			FDelegate(this,(TDelegate)OnOk) )
	,	CancelButton	( this, IDCANCEL,		FDelegate(this,(TDelegate)EndDialogFalse) )
	,	PackageEdit		( this, IDEC_PACKAGE )
	,	GroupEdit		( this, IDEC_GROUP )
	,	NameEdit		( this, IDEC_NAME )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgNewPrefab::OnInitDialog);
		WDialog::OnInitDialog();

		PackageEdit.SetText( *defPackage );
		GroupEdit.SetText( *defGroup );
		::SetFocus( NameEdit.hWnd );

		PackageEdit.SetText( *defPackage );
		GroupEdit.SetText( *defGroup );

		unguard;
	}
	virtual INT DoModal( FString _defPackage, FString _defGroup)
	{
		guard(WDlgNewPrefab::DoModal);

		defPackage = _defPackage;
		defGroup = _defGroup;

		return WDialog::DoModal( hInstance );
		unguard;
	}
	void OnOk()
	{
		guard(WDlgNewPrefab::OnOk);
		if( GetDataFromUser() )
			EndDialog(TRUE);
		unguard;
	}
	BOOL GetDataFromUser( void )
	{
		guard(WDlgNewPrefab::GetDataFromUser);
		Package = PackageEdit.GetText();
		Group = GroupEdit.GetText();
		Name = NameEdit.GetText();

		if( !Package.Len()
				|| !Name.Len() )
		{
			appMsgf( 0, TEXT("Invalid input.") );
			return FALSE;
		}

		return TRUE;
		unguard;
	}
};

// --------------------------------------------------------------
//
// IMPORT PREFAB Dialog
//
// --------------------------------------------------------------

class WDlgImportPrefab : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgImportPrefab,WDialog,UnrealEd)

	// Variables.
	WButton OkButton;
	WButton OkAllButton;
	WButton SkipButton;
	WButton CancelButton;
	WLabel FilenameStatic;
	WEdit PackageEdit;
	WEdit GroupEdit;
	WEdit NameEdit;

	FString defPackage, defGroup;
	TArray<FString>* paFilenames;

	FString Package, Group, Name;
	BOOL bOKToAll;
	INT iCurrentFilename;

	// Constructor.
	WDlgImportPrefab( UObject* InContext, WBrowser* InOwnerWindow )
	:	WDialog			( TEXT("Import Prefab"), IDDIALOG_IMPORT_PREFAB, InOwnerWindow )
	,	OkButton		( this, IDOK,			FDelegate(this,(TDelegate)OnOk) )
	,	OkAllButton		( this, IDPB_OKALL,		FDelegate(this,(TDelegate)OnOkAll) )
	,	SkipButton		( this, IDPB_SKIP,		FDelegate(this,(TDelegate)OnSkip) )
	,	CancelButton	( this, IDCANCEL,		FDelegate(this,(TDelegate)EndDialogFalse) )
	,	PackageEdit		( this, IDEC_PACKAGE )
	,	GroupEdit		( this, IDEC_GROUP )
	,	NameEdit		( this, IDEC_NAME )
	,	FilenameStatic	( this, IDSC_FILENAME )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgImportPrefab::OnInitDialog);
		WDialog::OnInitDialog();

		PackageEdit.SetText( *defPackage );
		GroupEdit.SetText( *defGroup );
		::SetFocus( NameEdit.hWnd );

		bOKToAll = FALSE;
		iCurrentFilename = -1;
		SetNextFilename();

		unguard;
	}
	virtual INT DoModal( FString _defPackage, FString _defGroup, TArray<FString>* _paFilenames)
	{
		guard(WDlgImportPrefab::DoModal);

		defPackage = _defPackage;
		defGroup = _defGroup;
		paFilenames = _paFilenames;

		return WDialog::DoModal( hInstance );
		unguard;
	}
	void OnOk()
	{
		guard(WDlgImportPrefab::OnOk);
		if( GetDataFromUser() )
		{
			ImportFile( (*paFilenames)(iCurrentFilename) );
			SetNextFilename();
		}
		unguard;
	}
	void OnOkAll()
	{
		guard(WDlgImportPrefab::OnOkAll);
		if( GetDataFromUser() )
		{
			ImportFile( (*paFilenames)(iCurrentFilename) );
			bOKToAll = TRUE;
			SetNextFilename();
		}
		unguard;
	}
	void OnSkip()
	{
		guard(WDlgImportPrefab::OnSkip);
		if( GetDataFromUser() )
			SetNextFilename();
		unguard;
	}
	void ImportPrefab( void )
	{
		guard(WDlgImportPrefab::ImportPrefab);
		unguard;
	}
	void RefreshName( void )
	{
		guard(WDlgImportPrefab::RefreshName);
		FilenameStatic.SetText( *(*paFilenames)(iCurrentFilename) );

		FString Name = GetFilenameOnly( (*paFilenames)(iCurrentFilename) );
		NameEdit.SetText( *Name );
		unguard;
	}
	BOOL GetDataFromUser( void )
	{
		guard(WDlgImportPrefab::GetDataFromUser);
		Package = PackageEdit.GetText();
		Group = GroupEdit.GetText();
		Name = NameEdit.GetText();

		if( !Package.Len()
				|| !Name.Len() )
		{
			appMsgf( 0, TEXT("Invalid input.") );
			return FALSE;
		}

		return TRUE;
		unguard;
	}
	void SetNextFilename( void )
	{
		guard(WDlgImportPrefab::SetNextFilename);
		iCurrentFilename++;
		if( iCurrentFilename == paFilenames->Num() ) {
			EndDialogTrue();
			return;
		}

		if( bOKToAll ) {
			RefreshName();
			GetDataFromUser();
			ImportFile( (*paFilenames)(iCurrentFilename) );
			SetNextFilename();
			return;
		};

		RefreshName();

		unguard;
	}
	void ImportFile( FString Filename )
	{
		guard(WDlgImportPrefab::ImportFile);
		TCHAR l_chCmd[512];

		if( Group.Len() )
			appSprintf( l_chCmd, TEXT("PREFAB IMPORT FILE=\"%s\" NAME=\"%s\" PACKAGE=\"%s\" GROUP=\"%s\""),
				*(*paFilenames)(iCurrentFilename), *Name, *Package, *Group );
		else
			appSprintf( l_chCmd, TEXT("PREFAB IMPORT FILE=\"%s\" NAME=\"%s\" PACKAGE=\"%s\""),
				*(*paFilenames)(iCurrentFilename), *Name, *Package );

		GUnrealEd->Exec( l_chCmd );
		unguard;
	}
};

// --------------------------------------------------------------
//
// WBrowserPrefab
//
// --------------------------------------------------------------

int CDECL PrefabNameCompare(const void *A, const void *B)
{
	return appStricmp( (*(UObject **)A)->GetName(), (*(UObject **)B)->GetName() );
}

#define ID_BT_TOOLBAR	29040
TBBUTTON tbBPButtons[] = {
	{ 0, IDMN_MB_DOCK, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 1, IDMN_PB_FileOpen, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 2, IDMN_PB_FileSave, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 3, IDMN_PB_CREATE_FROM_SELECTIONS, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 4, IDMN_PB_ADD_TO_LEVEL, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 5, IDMN_PB_PREV_GRP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 6, IDMN_PB_NEXT_GRP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 7, IDMN_PB_COMPILE, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
};
struct {
	TCHAR ToolTip[64];
	INT ID;
} ToolTips_BP[] = {
	TEXT("Toggle Dock Status"), IDMN_MB_DOCK,
	TEXT("Open Package"), IDMN_PB_FileOpen,
	TEXT("Save Package"), IDMN_PB_FileSave,
	TEXT("Previous Group"), IDMN_PB_PREV_GRP,
	TEXT("Next Group"), IDMN_PB_NEXT_GRP,
	TEXT("Insert Prefab into Level"), IDMN_PB_ADD_TO_LEVEL,
	TEXT("Create Prefab from Selected Actors"), IDMN_PB_CREATE_FROM_SELECTIONS,
	TEXT("Compile into Textured Preview"), IDMN_PB_COMPILE,
	NULL, 0
};

class WBrowserPrefab : public WBrowser
{
	DECLARE_WINDOWCLASS(WBrowserPrefab,WBrowser,Window)

	TMap<DWORD,FWindowAnchor> Anchors;

	FContainer *Container;
	WLabel* ViewportLabel;
	WComboBox *PackageCombo, *GroupCombo;
	WCheckBox *GroupAllCheck;
	WListBox *PrefabsList;
	UPrefab* RefPrefabs[MAX_RESULTS];
	HWND hWndToolBar;
	WToolTip *ToolTipCtrl;
	MRUList* mrulist;

	UViewport *Viewport;
	INT RendMap;

	// Structors.
	WBrowserPrefab( FName InPersistentName, WWindow* InOwnerWindow, HWND InEditorFrame )
	:	WBrowser( InPersistentName, InOwnerWindow, InEditorFrame )
	{
		Container = NULL;
		ViewportLabel = NULL;
		PackageCombo = GroupCombo = NULL;
		GroupAllCheck = NULL;
		PrefabsList = NULL;
		Viewport = NULL;
		MenuID = IDMENU_BrowserPrefab;
		BrowserID = eBROWSER_PREFAB;
		Description = TEXT("Prefabs");
		mrulist = NULL;
		RendMap = REN_Prefab;
	}

	// WBrowser interface.
	void OpenWindow( UBOOL bChild )
	{
		guard(WBrowserPrefab::OpenWindow);
		WBrowser::OpenWindow( bChild );
		SetCaption();
		unguard;
	}
	void OnCreate()
	{
		guard(WBrowserPrefab::OnCreate);
		WBrowser::OnCreate();

		SetMenu( hWnd, LoadMenuIdX(hInstance, IDMENU_BrowserPrefab) );

		Container = new FContainer();
		ViewportLabel = new WLabel( this, IDSC_VIEWPORT );
		ViewportLabel->OpenWindow( 1, 0 );
		PackageCombo = new WComboBox( this, IDCB_PACKAGE );
		PackageCombo->OpenWindow( 1, 1 );
		PackageCombo->SelectionChangeDelegate = FDelegate(this, (TDelegate)OnPackageComboSelChange);
		GroupCombo = new WComboBox( this, IDCB_GROUP );
		GroupCombo->OpenWindow( 1, 1 );
		GroupCombo->SelectionChangeDelegate = FDelegate(this, (TDelegate)OnGroupComboSelChange);
		GroupAllCheck = new WCheckBox( this, IDCK_GRP_ALL, FDelegate(this, (TDelegate)OnGroupAllClick) );
		GroupAllCheck->OpenWindow( 1, 0, 0, 1, 1, TEXT("All"), 1, 0, BS_PUSHLIKE );
		PrefabsList = new WListBox( this, IDLB_PREFABS );
		PrefabsList->OpenWindow( 1, 0, 0, 0, 0, WS_VSCROLL );
		PrefabsList->SelectionChangeDelegate = FDelegate(this, (TDelegate)OnPrefabSelChange);

		RefreshLevel();

		// Create the prefab browser viewport
		FName Name = TEXT("PrefabBrowser");
		Viewport = GUnrealEd->Client->NewViewport( Name );
		GPrefabLevel->SpawnViewActor( Viewport );
		Viewport->Actor->ShowFlags = GShowFlags;
		Viewport->Actor->RendMap   = RendMap;
		Viewport->Actor->XLevel = GPrefabLevel;
		Viewport->Actor->Location = FVector(500,0,300);
		Viewport->Group = NAME_None;
		Viewport->MiscRes = NULL;
		Viewport->Input->Init( Viewport );
		Viewport->OpenWindow( (DWORD)ViewportLabel->hWnd, 0, 320, 200, 0, 0 );

		RefreshViewport();

		hWndToolBar = CreateToolbarEx( 
			hWnd, WS_CHILD | WS_BORDER | WS_VISIBLE | CCS_ADJUSTABLE,
			IDB_BrowserPrefab_TOOLBAR,
			8,
			hInstance,
			IDB_BrowserPrefab_TOOLBAR,
			(LPCTBBUTTON)&tbBPButtons,
			12,
			16,16,
			16,16,
			sizeof(TBBUTTON));
		check(hWndToolBar);

		ToolTipCtrl = new WToolTip(this);
		ToolTipCtrl->OpenWindow();
		for( INT tooltip = 0 ; ToolTips_BP[tooltip].ID > 0 ; ++tooltip )
		{
			// Figure out the rectangle for the toolbar button.
			INT index = SendMessageX( hWndToolBar, TB_COMMANDTOINDEX, ToolTips_BP[tooltip].ID, 0 );
			RECT rect;
			SendMessageX( hWndToolBar, TB_GETITEMRECT, index, (LPARAM)&rect);

			ToolTipCtrl->AddTool( hWndToolBar, ToolTips_BP[tooltip].ToolTip, tooltip, &rect );
		}

		mrulist = new MRUList( *PersistentName );
		mrulist->ReadINI();
		if( GBrowserMaster->GetCurrent()==BrowserID )
			mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );

		INT Top = 0;
		Anchors.Set( (DWORD)hWndToolBar, FWindowAnchor( hWnd, hWndToolBar,					ANCHOR_TL, 0, 0,			ANCHOR_RIGHT|ANCHOR_HEIGHT, 0, STANDARD_TOOLBAR_HEIGHT ) );
		Top += STANDARD_TOOLBAR_HEIGHT+4;
		Anchors.Set( (DWORD)PackageCombo->hWnd, FWindowAnchor( hWnd, PackageCombo->hWnd,	ANCHOR_TL, 4, Top,			ANCHOR_RIGHT|ANCHOR_HEIGHT, -4, STANDARD_CTRL_HEIGHT ) );
		Top += STANDARD_CTRL_HEIGHT+2;
		Anchors.Set( (DWORD)GroupAllCheck->hWnd, FWindowAnchor( hWnd, GroupAllCheck->hWnd,	ANCHOR_TL, 4, Top,			ANCHOR_WIDTH|ANCHOR_HEIGHT, 64, STANDARD_CTRL_HEIGHT ) );
		Anchors.Set( (DWORD)GroupCombo->hWnd, FWindowAnchor( hWnd, GroupCombo->hWnd,		ANCHOR_TL, 4+64+2, Top,		ANCHOR_RIGHT|ANCHOR_HEIGHT, -4, STANDARD_CTRL_HEIGHT ) );
		Top += STANDARD_CTRL_HEIGHT+2;
		Anchors.Set( (DWORD)PrefabsList->hWnd, FWindowAnchor( hWnd, PrefabsList->hWnd,		ANCHOR_TL, 4, Top,			ANCHOR_WIDTH|ANCHOR_BOTTOM, 192, -4 ) );
		Anchors.Set( (DWORD)ViewportLabel->hWnd, FWindowAnchor( hWnd, ViewportLabel->hWnd,	ANCHOR_TL, 4+192+2, Top,	ANCHOR_BR, -4, -4 ) );
		Anchors.Set( (DWORD)Viewport->GetWindow(), FWindowAnchor( ViewportLabel->hWnd, (HWND)Viewport->GetWindow(),	ANCHOR_TL,0,0,					ANCHOR_BR,0,0 ) );

		Container->SetAnchors( &Anchors );

		PositionChildControls();
		RefreshPackages();
		RefreshGroups();
		RefreshPrefabsList();

		SetCaption();

		unguard;
	}
	void OnDestroy()
	{
		guard(WBrowserPrefab::OnDestroy);

		delete Viewport;

		delete Container;
		delete PackageCombo;
		delete GroupCombo;
		delete GroupAllCheck;
		delete PrefabsList;
		delete ViewportLabel;

		::DestroyWindow( hWndToolBar );
		delete ToolTipCtrl;

		mrulist->WriteINI();
		delete mrulist;

		//if( GPrefabLevel )
		//	GPrefabLevel->Destroy();
		//delete GPrefabLevel;
		GPrefabLevel = NULL;

		WBrowser::OnDestroy();
		unguard;
	}
	virtual void UpdateMenu()
	{
		guard(WBrowserPrefab::UpdateMenu);

		HMENU menu = GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd );

		CheckMenuItem( menu, IDMN_MB_DOCK, MF_BYCOMMAND | (IsDocked() ? MF_CHECKED : MF_UNCHECKED) );

		if( mrulist 
				&& GBrowserMaster->GetCurrent()==BrowserID )
			mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );

		unguard;
	}
	void Compile()
	{
		guard(WBrowserPrefab::Compile);

		if( GPrefabLevel )
		{
			// Compiling a prefab level just does a quick and dirty geometry rebuild.
			GUnrealEd->csgRebuild( GPrefabLevel );

			for(INT ActorIndex = 0;ActorIndex < GPrefabLevel->Actors.Num();ActorIndex++)
			{
				if(GPrefabLevel->Actors(ActorIndex))
					GPrefabLevel->Actors(ActorIndex)->ClearRenderData();
			}

			GUnrealEd->Flush(0);
			//GUnrealEd->shadowIlluminateBsp( GPrefabLevel, 0 );

			Viewport->Actor->RendMap = RendMap = REN_PrefabCompiled;
		}

		unguard;
	}
	void OnCommand( INT Command )
	{
		guard(WBrowserPrefab::OnCommand);
		switch( Command )
		{
			case IDMN_PB_COMPILE:
			{
				Compile();
			}
			break;

			case IDMN_PB_ADD_TO_LEVEL:
			{
				if( !GCurrentPrefab )
				{
					appMsgf(0, TEXT("Select a prefab first."));
					break;
				}

				if( !GUnrealEd->GetCurrentViewport() )
				{
					appMsgf(0, TEXT("No active viewport."));
					break;
				}

				GWarn->BeginSlowTask( TEXT("Inserting prefab"), 1 );

				const TCHAR* Ptr = *GCurrentPrefab->T3DText;
				ULevelFactory* Factory;
				Factory = ConstructObject<ULevelFactory>(ULevelFactory::StaticClass());
				Factory->FactoryCreateText( GUnrealEd->Level,ULevel::StaticClass(), GUnrealEd->Level->GetOuter(), GUnrealEd->Level->GetFName(), RF_Transactional, NULL, TEXT("paste"), Ptr, Ptr+GCurrentPrefab->T3DText.Len(), GWarn );
				GUnrealEd->NoteSelectionChange( GUnrealEd->Level );
				GBrowserMaster->RefreshAll();

				// Now that we've added the prefab into the world figure out the bounding box, and move it
				// so it's centered on the last click location.
				FBox bbox(1);
				for( INT i=0; i<GUnrealEd->Level->Actors.Num(); ++i )
					if( GUnrealEd->Level->Actors(i) && GUnrealEd->Level->Actors(i)->bSelected )
						bbox += (GUnrealEd->Level->Actors(i)->Location - GUnrealEd->Level->Actors(i)->PrePivot);

				FVector Diff = GUnrealEd->GetCurrentViewport()->Actor->Location - bbox.GetCenter();
				
				for( INT i=0; i<GUnrealEd->Level->Actors.Num(); ++i )
					if( GUnrealEd->Level->Actors(i) && GUnrealEd->Level->Actors(i)->bSelected )
						GUnrealEd->Level->Actors(i)->Location += Diff;

				GUnrealEd->RedrawLevel( GUnrealEd->Level );

				GWarn->EndSlowTask();
			}
			break;

			case IDMN_PB_CREATE_FROM_SELECTIONS:
			{
				FStringOutputDevice Ar;
				UExporter::ExportToOutputDevice( GUnrealEd->Level, NULL, Ar, TEXT("copy"), 0 );

				FString Package = PackageCombo->GetString( PackageCombo->GetCurrent() );
				FString Group = GroupCombo->GetString( GroupCombo->GetCurrent() );

				WDlgNewPrefab l_dlg( NULL, this );
				if( l_dlg.DoModal( Package, Group ) )
				{
					GUnrealEd->Exec( *FString::Printf( TEXT("PREFAB NEW CLASS=PREFAB NAME=\"%s\" GROUP=\"%s\" PACKAGE=\"%s\" T3DDATA=%d"),
						*l_dlg.Name, *l_dlg.Group, *l_dlg.Package, *Ar));

					RefreshPackages();
					PackageCombo->SetCurrent( PackageCombo->FindStringExact( *l_dlg.Package) );
					RefreshGroups();
					GroupCombo->SetCurrent( GroupCombo->FindStringExact( *l_dlg.Group) );
					RefreshPrefabsList();
					PrefabsList->SetCurrent( PrefabsList->FindStringExact( *l_dlg.Name) );

					RefreshViewport();
				}
			}
			break;

			case IDMN_PB_DELETE:
			{
				GUnrealEd->Trans->Reset( TEXT("Deleting Object") );

				if( !GCurrentPrefab )
				{
					appMsgf( 0, TEXT("Select a prefab first.") );
					break;
				}

				FString Name = GCurrentPrefab->GetName();
				FStringOutputDevice GetPropResult = FStringOutputDevice();
				TCHAR l_chCmd[256];

				appSprintf( l_chCmd, TEXT("DELETE CLASS=PREFAB OBJECT=\"%s\""), *Name);
				GUnrealEd->Get( TEXT("Obj"), l_chCmd, GetPropResult);

				if( !GetPropResult.Len() )
				{
					RefreshPackages();
					RefreshGroups();
					RefreshPrefabsList();
				}
				else
				{
					appMsgf( 0, TEXT("Can't delete prefab.\n\n%s"), *GetPropResult );
				}
			}
			break;

			case IDMN_PB_PREV_GRP:
			{
				INT Sel = GroupCombo->GetCurrent();
				Sel--;
				if( Sel < 0 ) Sel = GroupCombo->GetCount() - 1;
				GroupCombo->SetCurrent(Sel);
				RefreshPrefabsList();
			}
			break;

			case IDMN_PB_NEXT_GRP:
			{
				INT Sel = GroupCombo->GetCurrent();
				Sel++;
				if( Sel >= GroupCombo->GetCount() ) Sel = 0;
				GroupCombo->SetCurrent(Sel);
				RefreshPrefabsList();
			}
			break;

			case IDMN_PB_RENAME:
			{
				if( !GCurrentPrefab )
				{
					appMsgf( 0, TEXT("Select a prefab first.") );
					break;
				}

				WDlgRename dlg( NULL, this );
				FString Group, Package;
				if( !Cast<UPackage>(GCurrentPrefab->GetOuter()->GetOuter()) )
				{
					Group = TEXT("");
					Package = GCurrentPrefab->GetName();
				}
				else
				{			
					Group = GCurrentPrefab->GetOuter()->GetName();
					Package = GCurrentPrefab->GetOuter()->GetOuter()->GetName();
				}					
				if( dlg.DoModal( GCurrentPrefab->GetName(), Group, Package ) )
					GUnrealEd->Exec(*FString::Printf(TEXT("OBJ RENAME OLDNAME=\"%s\" OLDGROUP=\"%s\" OLDPACKAGE=\"%s\" NEWNAME=\"%s\" NEWGROUP=\"%s\" NEWPACKAGE=\"%s\""), *dlg.OldName, *dlg.OldGroup, *dlg.OldPackage, *dlg.NewName, *dlg.NewGroup, *dlg.NewPackage) );
				RefreshPackages();
				RefreshGroups();
				RefreshPrefabsList();
			}
			break;

			case IDMN_PB_FileOpen:
			{
				OPENFILENAMEA ofn;
				char File[8192] = "\0";

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "Prefab Packages (*.upx)\0*.upx\0All Files\0*.*\0\0";
				ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_UPX]) );
				ofn.lpstrDefExt = "upx";
				ofn.lpstrTitle = "Open Prefab Package";
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

					if( StringArray.Num() > 0 )
					{
						if( StringArray.Num() == 1 )
						{
							SavePkgName = *(StringArray(0));
							SavePkgName = SavePkgName.Right( SavePkgName.Len() - (SavePkgName.Left( SavePkgName.InStr(TEXT("\\"), 1)).Len() + 1 ));
						}
						else
							SavePkgName = *(StringArray(1));
						SavePkgName = SavePkgName.Left( SavePkgName.InStr(TEXT(".")) );
					}

					if( StringArray.Num() == 1 )
						GLastDir[eLASTDIR_UPX] = StringArray(0).Left( StringArray(0).InStr( TEXT("\\"), 1 ) );
					else
						GLastDir[eLASTDIR_UPX] = StringArray(0);

					GWarn->BeginSlowTask( TEXT(""), 1 );

					for( INT x = iStart ; x < StringArray.Num() ; ++x )
					{
						GWarn->StatusUpdatef( x, StringArray.Num(), TEXT("Loading %s"), *(StringArray(x)) );

						TCHAR l_chCmd[512];
						appSprintf( l_chCmd, TEXT("OBJ LOAD FILE=\"%s%s\""), *Prefix, *(StringArray(x)) );
						GUnrealEd->Exec( l_chCmd );

						mrulist->AddItem( *(StringArray(x)) );
						if( GBrowserMaster->GetCurrent()==BrowserID )
							mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );
					}

					GWarn->EndSlowTask();

					GBrowserMaster->RefreshAll();
					PackageCombo->SetCurrent( PackageCombo->FindStringExact( *SavePkgName ) );
					RefreshGroups();
					RefreshPrefabsList();

					StringArray.Empty();
				}

				GFileManager->SetDefaultDirectory(appBaseDir());
			}
			break;

			case IDMN_PB_FileSave:
			{
				OPENFILENAMEA ofn;
				char File[8192] = "\0";
				FString Package = PackageCombo->GetString( PackageCombo->GetCurrent() );

				::sprintf( File, "%s.upx", TCHAR_TO_ANSI( *Package ) );

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "Prefab Packages (*.upx)\0*.upx\0All Files\0*.*\0\0";
				ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_UPX]) );
				ofn.lpstrDefExt = "upx";
				ofn.lpstrTitle = "Save Prefab Package";
				ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

				if( GetSaveFileNameA(&ofn) )
				{
					TCHAR l_chCmd[512];

					appSprintf( l_chCmd, TEXT("OBJ SAVEPACKAGE PACKAGE=\"%s\" FILE=\"%s\""),
						*Package, appFromAnsi( File ) );
					if( GUnrealEd->Exec( l_chCmd ) )
					{
					FString S = appFromAnsi( File );
					mrulist->AddItem( S );
					if( GBrowserMaster->GetCurrent()==BrowserID )
						mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );
					GLastDir[eLASTDIR_UPX] = S.Left( S.InStr( TEXT("\\"), 1 ) );
				}
				}

				GFileManager->SetDefaultDirectory(appBaseDir());
			}
			break;

			case IDMN_MRU1:
			case IDMN_MRU2:
			case IDMN_MRU3:
			case IDMN_MRU4:
			case IDMN_MRU5:
			case IDMN_MRU6:
			case IDMN_MRU7:
			case IDMN_MRU8:
			{
				FString Filename = mrulist->Items[Command - IDMN_MRU1];
				if( GFileManager->FileSize( *Filename ) == -1 )
				{
					appMsgf( 0, TEXT("'%s' does not exist."), *Filename );
					mrulist->RemoveItem( Filename );
				}
				else
				{
					GUnrealEd->Exec( *FString::Printf(TEXT("OBJ LOAD FILE=\"%s\""), *Filename ));

					mrulist->MoveToTop( Command - IDMN_MRU1 );

					FString Package = Filename.Right( Filename.Len() - (Filename.InStr( TEXT("\\"), 1) + 1) );
					Package = Package.Left( Package.InStr( TEXT(".")) );

					GBrowserMaster->RefreshAll();
					RefreshPackages();
					PackageCombo->SetCurrent( PackageCombo->FindStringExact( *Package ) );
					RefreshGroups();
					RefreshPrefabsList();
				}

				mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );
			}
			break;

			case IDMN_PB_IMPORT_T3D:
			{
				OPENFILENAMEA ofn;
				char File[8192] = "\0";

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "T3D Files (*.t3d)\0*.t3d\0All Files\0*.*\0\0";
				ofn.lpstrDefExt = "t3d";
				ofn.lpstrTitle = "Import Prefabs";
				ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_T3D]) );
				ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

				// Display the Open dialog box. 
				//
				if( GetOpenFileNameA(&ofn) )
				{
					INT iNULLs = FormatFilenames( File );
					FString Package = PackageCombo->GetString( PackageCombo->GetCurrent() );
					FString Group = GroupCombo->GetString( GroupCombo->GetCurrent() );
	
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

					if( StringArray.Num() == 1 )
						GLastDir[eLASTDIR_T3D] = StringArray(0).Left( StringArray(0).InStr( TEXT("\\"), 1 ) );
					else
						GLastDir[eLASTDIR_T3D] = StringArray(0);

					TArray<FString> FilenamesArray;

					for( INT x = iStart ; x < StringArray.Num() ; ++x )
					{
						FString NewString;

						NewString = FString::Printf( TEXT("%s%s"), *Prefix, *(StringArray(x)) );
						new(FilenamesArray)FString( NewString );

						FString S = NewString;
					}

					WDlgImportPrefab l_dlg( NULL, this );
					l_dlg.DoModal( Package, Group, &FilenamesArray );

					// Flip to the package/group that was used for importing
					GBrowserMaster->RefreshAll();
					PackageCombo->SetCurrent( PackageCombo->FindStringExact( *l_dlg.Package) );
					RefreshGroups();
					GroupCombo->SetCurrent( GroupCombo->FindStringExact( *l_dlg.Group) );
					RefreshPrefabsList();

					StringArray.Empty();
					FilenamesArray.Empty();
				}

				GFileManager->SetDefaultDirectory(appBaseDir());
			}
			break;

			case IDMN_PB_EXPORT_T3D:
			{
				if( !GCurrentPrefab )
				{
					appMsgf( 0, TEXT("Select a prefab first.") );
					break;
				}

				OPENFILENAMEA ofn;
				char File[8192] = "\0";
				FString Name = GCurrentPrefab->GetName();

				::sprintf( File, "%s", TCHAR_TO_ANSI( *Name ) );

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "T3D Files (*.t3d)\0*.t3d\0All Files\0*.*\0\0";
				ofn.lpstrDefExt = "t3d";
				ofn.lpstrTitle = "Export Prefab";
				ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_T3D]) );
				ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

				// Display the Open dialog box. 
				//
				if( GetSaveFileNameA(&ofn) )
				{
					GUnrealEd->Exec( *FString::Printf( TEXT("OBJ EXPORT TYPE=PREFAB NAME=\"%s\" FILE=\"%s\""),
						GCurrentPrefab->GetPathName(), appFromAnsi( File )) );

					FString S = appFromAnsi( File );
					GLastDir[eLASTDIR_T3D] = S.Left( S.InStr( TEXT("\\"), 1 ) );
				}

				GFileManager->SetDefaultDirectory(appBaseDir());
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
		guard(WBrowserPrefab::OnSize);
		WBrowser::OnSize(Flags, NewX, NewY);
		PositionChildControls();
		InvalidateRect( hWnd, NULL, FALSE );
		UpdateMenu();
		unguard;
	}
	virtual void RefreshAll()
	{
		guard(WBrowserPrefab::RefreshAll);
		RefreshPackages();
		RefreshGroups();
		RefreshPrefabsList();
		RefreshViewport();
		if( GBrowserMaster->GetCurrent()==BrowserID )
			mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );
		unguard;
	}
	void RefreshPackages( void )
	{
		guard(WBrowserPrefab::RefreshPackages);

		INT Current = PackageCombo->GetCurrent();
		Current = (Current != CB_ERR) ? Current : 0;

		// PACKAGES
		//
		PackageCombo->Empty();

		FStringOutputDevice GetPropResult = FStringOutputDevice();
		GUnrealEd->Get( TEXT("OBJ"), TEXT("PACKAGES CLASS=Prefab"), GetPropResult );

		TArray<FString> StringArray;
		GetPropResult.ParseIntoArray( TEXT(","), &StringArray );

		for( INT x = 0 ; x < StringArray.Num() ; ++x )
			PackageCombo->AddString( *(StringArray(x)) );

		PackageCombo->SetCurrent( Current );

		StringArray.Empty();

		unguard;
	}
	void RefreshGroups( void )
	{
		guard(WBrowserPrefab::RefreshGroups);

		FString Package = PackageCombo->GetString( PackageCombo->GetCurrent() );
		INT Current = GroupCombo->GetCurrent();
		Current = (Current != CB_ERR) ? Current : 0;

		// GROUPS
		//
		GroupCombo->Empty();

		FStringOutputDevice GetPropResult = FStringOutputDevice();
		TCHAR l_ch[256];
		appSprintf( l_ch, TEXT("GROUPS CLASS=Prefab PACKAGE=\"%s\""), *Package );
		GUnrealEd->Get( TEXT("OBJ"), l_ch, GetPropResult );

		TArray<FString> StringArray;
		GetPropResult.ParseIntoArray( TEXT(","), &StringArray );

		for( INT x = 0 ; x < StringArray.Num() ; ++x )
		{
			GroupCombo->AddString( *(StringArray(x)) );
		}

		GroupCombo->SetCurrent(Current);

		StringArray.Empty();

		unguard;
	}
	void RefreshPrefabsList( void )
	{
		guard(WBrowserPrefab::RefreshPrefabsList);

		INT SaveSel = PrefabsList->GetCurrent();

		FString PackageName = PackageCombo->GetString( PackageCombo->GetCurrent() );
		FString GroupName = GroupCombo->GetString( GroupCombo->GetCurrent() );
		if( GroupAllCheck->IsChecked() )
			GroupName.Empty();

		PrefabsList->Empty();

		UPackage* Pkg = Cast<UPackage>(UObject::StaticFindObject( UPackage::StaticClass(), NULL, *PackageName ));

		INT idx = -1;
		if( Pkg )
		{
			if( GroupName.Len() )
				Pkg = FindObject<UPackage>( Pkg, *GroupName );

			for( TObjectIterator<UPrefab> It; It; ++It )
				if( It->IsIn(Pkg) )
				{
					PrefabsList->AddString( TEXT("X") );
					RefPrefabs[++idx] = *It;
				}
		}

		// Sort the reference prefabs so they match the listbox sorting
		appQsort( &RefPrefabs[0], PrefabsList->GetCount(), sizeof(UPrefab*), PrefabNameCompare );

		// Now that we've sort the list of prefab names, clear out the listbox and add our
		// list in.  Windows doesn't seem to sort the same way that we do which causes
		// the RefPrefabs list to not match the contents of the listbox.  The solution is
		// to handle the sorting ourselves.
		INT Num = PrefabsList->GetCount();
		PrefabsList->Empty();
		for( INT x = 0 ; x < Num ; ++x )
			PrefabsList->AddString( RefPrefabs[x]->GetName() );

		if( PrefabsList->SetCurrent( SaveSel ) == LB_ERR )
			PrefabsList->SetCurrent( 0 );

		OnPrefabSelChange();

		unguard;
	}
	void RefreshViewport( void )
	{
		guard(WBrowserPrefab::RefreshViewport);

		if( PrefabsList->GetCurrent() == LB_ERR )
			return;

		GUnrealEd->Exec( *FString::Printf( TEXT("CAMERA UPDATE FLAGS=%d REN=%d NAME=PrefabBrowser"),
			SHOW_StandardView | SHOW_Volumes | SHOW_ChildWindow | SHOW_Frame | SHOW_Actors | SHOW_Brush | SHOW_MovingBrushes | SHOW_StaticMeshes | SHOW_SelectionHighlight | SHOW_Projectors,
			RendMap ) );

		UpdateMenu();

		unguard;
	}
	void RefreshLevel( void )
	{
		guard(WBrowserPrefab::RefreshLevel);

		if( GPrefabLevel )
		{
			GPrefabLevel->Destroy();
			delete GPrefabLevel;
		}

		GPrefabLevel = new( UObject::GetTransientPackage(), TEXT("PrefabLevel") )ULevel( GUnrealEd, 0 );

		if( Viewport )
			Viewport->Actor->XLevel = GPrefabLevel;

		unguard;
	}

	// Moves the child windows around so that they best match the window size.
	//
	void PositionChildControls( void )
	{
		guard(WBrowserPrefab::PositionChildControls);
		if( Container ) Container->RefreshControls();
		unguard;
	}

	// Notification delegates for child controls.
	//
	void OnPackageComboSelChange()
	{
		guard(WBrowserPrefab::OnPackageComboSelChange);
		RefreshGroups();
		RefreshPrefabsList();
		RefreshViewport();
		unguard;
	}
	void OnGroupComboSelChange()
	{
		guard(WBrowserPrefab::OnGroupComboSelChange);
		RefreshPrefabsList();
		RefreshViewport();
		unguard;
	}
	void OnGroupAllClick()
	{
		guard(WBrowserPrefab::OnGroupAllClick);
		EnableWindow( GroupCombo->hWnd, !GroupAllCheck->IsChecked() );
		RefreshPrefabsList();
		RefreshViewport();
		unguard;
	}
	void OnPrefabSelChange()
	{
		guard(WBrowserPrefab::OnPrefabSelChange);

		if( !PrefabsList->GetCount() ) return;
		GCurrentPrefab = RefPrefabs[ PrefabsList->GetCurrent() ];

		//
		// Create a new level
		//

		RefreshLevel();

		//
		// Add the prefab into the new level
		//

		const TCHAR* Ptr = *GCurrentPrefab->T3DText;
		ULevelFactory* Factory;
		Factory = ConstructObject<ULevelFactory>(ULevelFactory::StaticClass());
		Factory->FactoryCreateText( GPrefabLevel,ULevel::StaticClass(), GPrefabLevel->GetOuter(), GPrefabLevel->GetFName(), RF_Transactional, NULL, TEXT("paste"), Ptr, Ptr+GCurrentPrefab->T3DText.Len(), GWarn );

		//Viewport->Actor->XLevel = GPrefabLevel;
		GCache.Flush();

		//
		// Center the prefab on the level origin.
		//

		// Now that we've added the prefab into the world figure out the bounding box, and move it
		// so it's centered on the last click location.
		FBox bbox(1);
		for( INT i=1; i<GPrefabLevel->Actors.Num(); ++i )
			if( GPrefabLevel->Actors(i) && GPrefabLevel->Actors(i)->bSelected )
				bbox += (GPrefabLevel->Actors(i)->Location - GPrefabLevel->Actors(i)->PrePivot);

		FVector Diff = bbox.GetCenter();
		
		for( INT i=0; i<GPrefabLevel->Actors.Num(); ++i )
			if( GPrefabLevel->Actors(i) && GPrefabLevel->Actors(i)->bSelected )
			{
				GPrefabLevel->Actors(i)->Location -= Diff;
				GPrefabLevel->Actors(i)->bSelected = 0;
			}

		RefreshViewport();
		GBrowserMaster->RefreshAllOthers(BrowserID);

		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

