/*=============================================================================
	BrowserSound : Browser window for sound effects
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

#include <stdio.h>
extern FString GLastDir[eLASTDIR_MAX];

// --------------------------------------------------------------
//
// IMPORT SOUND Dialog
//
// --------------------------------------------------------------

class WDlgImportSound : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgImportSound,WDialog,UnrealEd)

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
	WDlgImportSound( UObject* InContext, WBrowser* InOwnerWindow )
	:	WDialog			( TEXT("Import Sound"), IDDIALOG_IMPORT_SOUND, InOwnerWindow )
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
		guard(WDlgImportSound::OnInitDialog);
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
		guard(WDlgImportSound::DoModal);

		defPackage = _defPackage;
		defGroup = _defGroup;
		paFilenames = _paFilenames;

		return WDialog::DoModal( hInstance );
		unguard;
	}
	void OnOk()
	{
		guard(WDlgImportSound::OnOk);
		if( GetDataFromUser() )
		{
			ImportFile( (*paFilenames)(iCurrentFilename) );
			SetNextFilename();
		}
		unguard;
	}
	void OnOkAll()
	{
		guard(WDlgImportSound::OnOkAll);
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
		guard(WDlgImportSound::OnSkip);
		if( GetDataFromUser() )
			SetNextFilename();
		unguard;
	}
	void RefreshName( void )
	{
		guard(WDlgImportSound::RefreshName);
		FilenameStatic.SetText( *(*paFilenames)(iCurrentFilename) );

		FString Name = GetFilenameOnly( (*paFilenames)(iCurrentFilename) );

        // gam --- strip spaces from the name
        INT i = Name.InStr( TEXT(" ") );

        while( i >= 0 )
        {
            FString Left, Right;

            Left = Name.Left (i);
            Right = Name.Right (Name.Len() - i - 1);

            Name = Left + Right;

            i = Name.InStr( TEXT(" ") );
        }
        // --- gam

		NameEdit.SetText( *Name );
		unguard;
	}
	BOOL GetDataFromUser( void )
	{
		guard(WDlgImportSound::GetDataFromUser);
		Package = PackageEdit.GetText();
		Group = GroupEdit.GetText();
		Name = NameEdit.GetText();

        // gam ---
        if( !Package.Len() )
		{
			appMsgf( 0, TEXT("You must specify a package.") );
			return FALSE;
		}

        if( appStrchr(*Package, ' ' ) != NULL )
		{
			appMsgf( 0, TEXT("Package can't have spaces.") );
			return FALSE;
		}

        if( appStrchr(*Group, ' ' ) != NULL )
		{
			appMsgf( 0, TEXT("Group can't have spaces.") );
			return FALSE;
		}

        if( !Name.Len() )
		{
			appMsgf( 0, TEXT("You must specify a name.") );
			return FALSE;
		}

        if( appStrchr(*Name, ' ' ) != NULL )
		{
			appMsgf( 0, TEXT("Name can't have spaces.") );
			return FALSE;
		}
        // --- gam

		return TRUE;
		unguard;
	}
	void SetNextFilename( void )
	{
		guard(WDlgImportSound::SetNextFilename);
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
		guard(WDlgImportSound::ImportFile);
		TCHAR l_chCmd[512];

		if( Group.Len() )
			appSprintf( l_chCmd, TEXT("AUDIO IMPORT FILE=\"%s\" NAME=\"%s\" PACKAGE=\"%s\" GROUP=\"%s\""),
				*(*paFilenames)(iCurrentFilename), *Name, *Package, *Group );
		else
			appSprintf( l_chCmd, TEXT("AUDIO IMPORT FILE=\"%s\" NAME=\"%s\" PACKAGE=\"%s\""),
				*(*paFilenames)(iCurrentFilename), *Name, *Package );
		GUnrealEd->Exec( l_chCmd );
		unguard;
	}
};

// gam ---

// --------------------------------------------------------------
//
// NEW SOUND Dialog
//
// --------------------------------------------------------------

class WDlgNewSound : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgNewSound,WDialog,UnrealEd)

	// Variables.
	WButton OkButton;
	WButton CancelButton;

	WEdit PackageEdit;
	WEdit GroupEdit;
	WEdit NameEdit;

	FString Package, Group, Name;

    FString TitleString;

	// Constructor.
	WDlgNewSound( UObject* InContext, WBrowser* InOwnerWindow )
	:	WDialog			( TEXT("New Sound"), IDDIALOG_NEW_SOUND, InOwnerWindow )
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
		guard(WDlgNewSound::OnInitDialog);
		WDialog::OnInitDialog();

		PackageEdit.SetText( *Package );
		GroupEdit.SetText( *Group );
		NameEdit.SetText( *Name );

        SetText( *TitleString );

		::SetFocus( NameEdit.hWnd );

		unguard;
	}
	void OnDestroy()
	{
		guard(WDlgNewSound::OnDestroy);
		WDialog::OnDestroy();
		unguard;
	}
	virtual INT DoModal( FString InTitleString, FString DefPackage, FString DefGroup )
	{
		guard(WDlgNewSound::DoModal);

        TitleString = InTitleString;

		Package = DefPackage;
		Group = DefGroup;
		Name = TEXT("");

		return WDialog::DoModal( hInstance );
		unguard;
	}
	void OnOk()
	{
		guard(WDlgNewSound::OnOk);
		Package = PackageEdit.GetText();
		Group = GroupEdit.GetText();
		Name = NameEdit.GetText();
        EndDialogTrue();
		unguard;
	}
};

// --- gam

// --------------------------------------------------------------
//
// WBROWSERSOUND
//
// --------------------------------------------------------------

#define ID_BS_TOOLBAR	29020
TBBUTTON tbBSButtons[] = {
	{ 0, IDMN_MB_DOCK, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 1, IDMN_SB_FileOpen, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 2, IDMN_SB_FileSave, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 3, IDMN_SB_PROPERTIES, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0} // gam
	, { 4, IDMN_SB_LOAD_ENTIRE_PACKAGE, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 5, IDMN_SB_PLAY, TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0}
	, { 6, IDMN_SB_LOOPING, TBSTATE_ENABLED, TBSTYLE_CHECK, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 7, IDMN_SB_STOP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
};
struct {
	TCHAR ToolTip[64];
	INT ID;
} ToolTips_BS[] = {
	TEXT("Toggle Dock Status"), IDMN_MB_DOCK,
	TEXT("Open Package"), IDMN_SB_FileOpen,
	TEXT("Save Package"), IDMN_SB_FileSave,
	TEXT("Properties"), IDMN_SB_PROPERTIES, // gam
	TEXT("Load Entire Package"), IDMN_SB_LOAD_ENTIRE_PACKAGE,
	TEXT("Play"), IDMN_SB_PLAY,
	TEXT("Looping?"), IDMN_SB_LOOPING,
	TEXT("Stop"), IDMN_SB_STOP,
	NULL, 0
};

class WBrowserSound : public WBrowser
{
	DECLARE_WINDOWCLASS(WBrowserSound,WBrowser,Window)

	TMap<DWORD,FWindowAnchor> Anchors;

	FContainer *Container;
	WComboBox *pComboPackage, *pComboGroup;
	WListBox *pListSounds;
	WCheckBox *pCheckGroupAll;
	HWND hWndToolBar;
	WToolTip *ToolTipCtrl;
	MRUList* mrulist;

	UBOOL bPlayButtonDown, bLooping;

	// Structors.
	WBrowserSound( FName InPersistentName, WWindow* InOwnerWindow, HWND InEditorFrame )
	:	WBrowser( InPersistentName, InOwnerWindow, InEditorFrame )
	{
		Container = NULL;
		pComboPackage = pComboGroup = NULL;
		pListSounds = NULL;
		pCheckGroupAll = NULL;
		MenuID = IDMENU_BrowserSound;
		BrowserID = eBROWSER_SOUND;
		Description = TEXT("Sounds");
 		bPlayButtonDown = bLooping = 0;
        // gam ---
		hWndToolBar = NULL;
		ToolTipCtrl = NULL;
		mrulist = NULL;
        // --- gam
	}

	// WBrowser interface.
	void OpenWindow( UBOOL bChild )
	{
		guard(WBrowserSound::OpenWindow);
		WBrowser::OpenWindow( bChild );
		SetCaption();
		unguard;
	}
	void OnCreate()
	{
		guard(WBrowserSound::OnCreate);
		WBrowser::OnCreate();

		SetMenu( hWnd, LoadMenuIdX(hInstance, IDMENU_BrowserSound) );

		Container = new FContainer();

		// PACKAGES
		//
		pComboPackage = new WComboBox( this, IDCB_PACKAGE );
		pComboPackage->OpenWindow( 1, 1 );
		pComboPackage->SelectionChangeDelegate = FDelegate(this, (TDelegate)OnComboPackageSelChange);

		// GROUP
		//
		pComboGroup = new WComboBox( this, IDCB_GROUP );
		pComboGroup->OpenWindow( 1, 1 );
		pComboGroup->SelectionChangeDelegate = FDelegate(this, (TDelegate)OnComboGroupSelChange);

		// SOUND LIST
		//
		pListSounds = new WListBox( this, IDLB_SOUNDS );
		pListSounds->OpenWindow( 1, 0, 0, 0, 1 );
		pListSounds->SelectionChangeDelegate = FDelegate(this, (TDelegate)OnListSoundsSelectionChange);
		pListSounds->DoubleClickDelegate = FDelegate(this, (TDelegate)OnListSoundsDblClick);
        // gam ---
		pListSounds->RightClickDelegate = FDelegate(this, (TDelegate)OnListSoundsRightClick);
        // --- gam

		// CHECK BOXES
		//
		pCheckGroupAll = new WCheckBox( this, IDCK_GRP_ALL, FDelegate(this, (TDelegate)OnGroupAllClick) );
		pCheckGroupAll->OpenWindow( 1, 0, 0, 1, 1, TEXT("All"), 1, 0, BS_PUSHLIKE );

		hWndToolBar = CreateToolbarEx( 
			hWnd, WS_CHILD | WS_BORDER | WS_VISIBLE | CCS_ADJUSTABLE,
			IDB_BrowserSound_TOOLBAR,
			8,
			hInstance,
			IDB_BrowserSound_TOOLBAR,
			(LPCTBBUTTON)&tbBSButtons,
			12,
			16,16,
			16,16,
			sizeof(TBBUTTON));
		check(hWndToolBar);

		ToolTipCtrl = new WToolTip(this);
		ToolTipCtrl->OpenWindow();
		for( INT tooltip = 0 ; ToolTips_BS[tooltip].ID > 0 ; ++tooltip )
		{
			// Figure out the rectangle for the toolbar button.
			INT index = SendMessageX( hWndToolBar, TB_COMMANDTOINDEX, ToolTips_BS[tooltip].ID, 0 );
			RECT rect;
			SendMessageX( hWndToolBar, TB_GETITEMRECT, index, (LPARAM)&rect);

			ToolTipCtrl->AddTool( hWndToolBar, ToolTips_BS[tooltip].ToolTip, tooltip, &rect );
		}

		mrulist = new MRUList( *PersistentName );
		mrulist->ReadINI();
		if( GBrowserMaster->GetCurrent()==BrowserID )
			mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );

		INT Top = 0;
		Anchors.Set( (DWORD)hWndToolBar,			FWindowAnchor( hWnd, hWndToolBar,			ANCHOR_TL, 0, 0,		ANCHOR_RIGHT|ANCHOR_HEIGHT, 0, STANDARD_TOOLBAR_HEIGHT ) );
		Top += STANDARD_TOOLBAR_HEIGHT+4;
		Anchors.Set( (DWORD)pComboPackage->hWnd,	FWindowAnchor( hWnd, pComboPackage->hWnd,	ANCHOR_TL, 4, Top,		ANCHOR_RIGHT|ANCHOR_HEIGHT, -4, STANDARD_CTRL_HEIGHT ) );
		Top += STANDARD_CTRL_HEIGHT+2; 
		Anchors.Set( (DWORD)pCheckGroupAll->hWnd,	FWindowAnchor( hWnd, pCheckGroupAll->hWnd,	ANCHOR_TL, 4, Top,		ANCHOR_WIDTH|ANCHOR_HEIGHT, 64, STANDARD_CTRL_HEIGHT ) );
		Anchors.Set( (DWORD)pComboGroup->hWnd,		FWindowAnchor( hWnd, pComboGroup->hWnd,		ANCHOR_TL, 4+64+2, Top,	ANCHOR_RIGHT|ANCHOR_HEIGHT, -4, STANDARD_CTRL_HEIGHT ) );
		Top += STANDARD_CTRL_HEIGHT+2; 
		Anchors.Set( (DWORD)pListSounds->hWnd,		FWindowAnchor( hWnd, pListSounds->hWnd,		ANCHOR_TL, 4, Top,		ANCHOR_BR, -4, -4 ) );

		Container->SetAnchors( &Anchors );

		RefreshAll();

		PositionChildControls();

		unguard;
	}
	void OnDestroy()
	{
		guard(WBrowserSound::OnDestroy);

        // gam ---
        HMENU hMenu = GetMenu( hWnd );
        if( hMenu )
        {
            SetMenu( hWnd, NULL );
            DestroyMenu( hMenu );
        }
        // --- gam

		mrulist->WriteINI();
		delete mrulist;

		delete Container;
		delete pComboPackage;
		delete pComboGroup;
		delete pListSounds;
		delete pCheckGroupAll;

		::DestroyWindow( hWndToolBar );
		delete ToolTipCtrl;

		WBrowser::OnDestroy();
		unguard;
	}
	virtual void UpdateMenu()
	{
		guard(WBrowserSound::UpdateMenu);

		// Update the menu ...

		HMENU menu = IsDocked() ? GetMenu( OwnerWindow->hWnd ) : GetMenu( hWnd );
        if( menu ) // gam
		{
			CheckMenuItem( menu, IDMN_MB_DOCK, MF_BYCOMMAND | (IsDocked() ? MF_CHECKED : MF_UNCHECKED) );
			CheckMenuItem( menu, IDMN_SB_PLAY, MF_BYCOMMAND | (bPlayButtonDown ? MF_CHECKED : MF_UNCHECKED) );
			CheckMenuItem( menu, IDMN_SB_LOOPING, MF_BYCOMMAND | (bLooping ? MF_CHECKED : MF_UNCHECKED) );
		}

		// Update the toolbar ...

		SendMessageX( hWndToolBar, TB_SETSTATE, IDMN_SB_PLAY, (bPlayButtonDown?TBSTATE_CHECKED:0) | TBSTATE_ENABLED );
		SendMessageX( hWndToolBar, TB_SETSTATE, IDMN_SB_LOOPING, (bLooping?TBSTATE_CHECKED:0) | TBSTATE_ENABLED );

		if( mrulist
				&& GBrowserMaster->GetCurrent()==BrowserID )
			mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );

		unguard;
	}

	void OnCommand( INT Command )
	{
		guard(WBrowserSound::OnCommand);
		switch( Command ) {

            // gam ---
			case IDMN_SB_PROPERTIES:
				{
					FString PackageName = pComboPackage->GetString( pComboPackage->GetCurrent() );
					FString GroupName = pComboGroup->GetString( pComboGroup->GetCurrent() );
					FString SoundName = GetSoundName();

				    UPackage* Package = FindObject<UPackage>( ANY_PACKAGE, *PackageName );

		            if( Package && GroupName.Len() )
				        Package = FindObject<UPackage>( Package, *GroupName );

					USound* Sound = NULL;
					if( pCheckGroupAll->IsChecked() )
						Sound = FindObject<USound>( ANY_PACKAGE, *SoundName );
					else
						Sound = FindObject<USound>( Package, *SoundName );

                    if( Sound )
        				GUnrealEd->ShowSoundProperties( Sound );
				}
				break;
            // --- gam

			case IDMN_SB_LOAD_ENTIRE_PACKAGE:
			{
				FString Package = pComboPackage->GetString( pComboPackage->GetCurrent() );
				if( Package != TEXT("MyLevel") )
				{
					GUnrealEd->LoadPackage( NULL, *Package, LOAD_NoWarn );
					RefreshAll();
				}
			}
			break;

			case IDMN_SB_RENAME:
				{
					FString Name = GetSoundName();
					FStringOutputDevice GetPropResult = FStringOutputDevice();
					TCHAR l_chCmd[256];

					WDlgRename dlg( NULL, this );
                    	
					FString PackageName = pComboPackage->GetString( pComboPackage->GetCurrent() );
					FString GroupName = pComboGroup->GetString( pComboGroup->GetCurrent() );
					if( dlg.DoModal(*Name,GroupName,PackageName) )
					{
						GUnrealEd->Exec(*FString::Printf(TEXT("OBJ RENAME OLDNAME=\"%s\" OLDGROUP=\"%s\" OLDPACKAGE=\"%s\" NEWNAME=\"%s\" NEWGROUP=\"%s\" NEWPACKAGE=\"%s\""), *dlg.OldName, *dlg.OldGroup, *dlg.OldPackage, *dlg.NewName, *dlg.NewGroup, *dlg.NewPackage) );

						for( TObjectIterator<USoundGroup> It; It; ++It )
							It->RefreshGroup(1);

						RefreshAll();
					}
				}
				break;

			case IDMN_SB_NEW_SOUND_GROUP:
				{
					FString PackageName = pComboPackage->GetString( pComboPackage->GetCurrent() );
					FString GroupName = pComboGroup->GetString( pComboGroup->GetCurrent() );
                    FString NameName;

					WDlgNewSound l_dlg( NULL, this );
					if( !l_dlg.DoModal( TEXT("New Sound Group"), PackageName, GroupName ) )
                        break;

                    PackageName = l_dlg.Package;
                    GroupName = l_dlg.Group;
                    NameName = l_dlg.Name;

			        UPackage* Pkg = UObject::CreatePackage(NULL,*PackageName);

			        if( GroupName.Len() > 0 )
				        Pkg = UObject::CreatePackage(Pkg,*GroupName);

                    USoundGroup *Sound = CastChecked<USoundGroup>(UObject::StaticConstructObject( USoundGroup::StaticClass(), Pkg, *NameName, RF_Public|RF_Standalone ) );
					
                    if( !Sound )
						appMsgf( 0, TEXT("Could not create sound!") );
                    else
                    {
						GBrowserMaster->RefreshAll();

                        // gam ---
                        for( TObjectIterator<USoundGroup> It; It; ++It )
                            It->RefreshGroup(1);

						pComboPackage->SetCurrent( pComboPackage->FindStringExact( *l_dlg.Package) );
						pComboGroup->SetCurrent( pComboGroup->FindStringExact( *l_dlg.Group) );
		                RefreshAll();
                        // --- gam
                    }
				}
				break;

			case IDMN_SB_NEW_PROCEDURAL_SOUND:
				{
					FString PackageName = pComboPackage->GetString( pComboPackage->GetCurrent() );
					FString GroupName = pComboGroup->GetString( pComboGroup->GetCurrent() );
                    FString NameName;

					WDlgNewSound l_dlg( NULL, this );
					if( !l_dlg.DoModal( TEXT("New Procedural Sound"), PackageName, GroupName ) )
                        break;

                    PackageName = l_dlg.Package;
                    GroupName = l_dlg.Group;
                    NameName = l_dlg.Name;

			        UPackage* Pkg = UObject::CreatePackage(NULL,*PackageName);

			        if( GroupName.Len() > 0 )
				        Pkg = UObject::CreatePackage(Pkg,*GroupName);

                    UProceduralSound *Sound = CastChecked<UProceduralSound>(UObject::StaticConstructObject( UProceduralSound::StaticClass(), Pkg, *NameName, RF_Public|RF_Standalone ) );
					
                    if( !Sound )
						appMsgf( 0, TEXT("Could not create sound!") );
                    else
                    {
						GBrowserMaster->RefreshAll();

                        // gam ---
                        for( TObjectIterator<USoundGroup> It; It; ++It )
                            It->RefreshGroup(1);

						pComboPackage->SetCurrent( pComboPackage->FindStringExact( *l_dlg.Package) );
						pComboGroup->SetCurrent( pComboGroup->FindStringExact( *l_dlg.Group) );
		                RefreshAll();
                        // --- gam
                    }
				}
				break;

            // --- gam

			case IDMN_SB_DELETE:
				{
					FString Name = GetSoundName();
					FStringOutputDevice GetPropResult = FStringOutputDevice();
					TCHAR l_chCmd[256];

					appSprintf( l_chCmd, TEXT("DELETE CLASS=SOUND OBJECT=\"%s\""), *Name);
				    GUnrealEd->Get( TEXT("Obj"), l_chCmd, GetPropResult);

                    for( TObjectIterator<USoundGroup> It; It; ++It )
                        It->RefreshGroup(1);

					if( !GetPropResult.Len() )
						RefreshSoundList();
					else
						appMsgf( 0, TEXT("Can't delete sound\n\n%s"), *GetPropResult ); // gam
				}
				break;

			case IDMN_SB_EXPORT_WAV:
				{
					OPENFILENAMEA ofn;
					char File[8192] = "\0";
					FString Name = GetSoundName();

					::sprintf( File, "%s", TCHAR_TO_ANSI( *Name ) );

					ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
					ofn.lStructSize = sizeof(OPENFILENAMEA);
					ofn.hwndOwner = hWnd;
					ofn.lpstrFile = File;
					ofn.nMaxFile = sizeof(char) * 8192;
					ofn.lpstrFilter = "WAV Files (*.wav)\0*.wav\0All Files\0*.*\0\0";
					ofn.lpstrDefExt = "wav";
					ofn.lpstrTitle = "Export Sound";
					ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_WAV]) );
					ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

					// Display the Open dialog box. 
					//
					if( GetSaveFileNameA(&ofn) )
					{
						TCHAR l_chCmd[512];
						FString Package = pComboPackage->GetString( pComboPackage->GetCurrent() );

						appSprintf( l_chCmd, TEXT("OBJ EXPORT TYPE=SOUND PACKAGE=\"%s\" NAME=\"%s\" FILE=\"%s\""),
							*Package, *Name, appFromAnsi( File ) );
						GUnrealEd->Exec( l_chCmd );

						FString S = appFromAnsi( File );
						GLastDir[eLASTDIR_WAV] = S.Left( S.InStr( TEXT("\\"), 1 ) );
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

					pComboPackage->SetCurrent( pComboPackage->FindStringExact( *Package ) );

					// gam ---
					for( TObjectIterator<USoundGroup> It; It; ++It )
						It->RefreshGroup(1);

					pComboPackage->SetCurrent( pComboPackage->FindStringExact( *Package ) );
					RefreshAll();
					// --- gam
				}

				mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );
			}
			break;

			case IDMN_SB_IMPORT_WAV:
				{
					OPENFILENAMEA ofn;
					char File[16384] = "\0";
					TCHAR TCFile[16384];

					ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
					ofn.lStructSize = sizeof(OPENFILENAMEA);
					ofn.hwndOwner = hWnd;
					ofn.lpstrFile = File;
					ofn.nMaxFile = sizeof(File);
					ofn.lpstrFilter = "WAV Files (*.wav)\0*.wav\0All Files\0*.*\0\0";
					ofn.lpstrDefExt = "wav";
					ofn.lpstrTitle = "Import Sounds";
					ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_WAV]) );
					ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

					// Display the Open dialog box. 
					//
					if( GetOpenFileNameA(&ofn) )
					{
						INT iNULLs = FormatFilenames( File );
						FString Package = pComboPackage->GetString( pComboPackage->GetCurrent() );
						FString Group = pComboGroup->GetString( pComboGroup->GetCurrent() );
		
						INT StrCount;
						for( StrCount=0; StrCount<16384-1 && File[StrCount]; StrCount++ )
							TCFile[StrCount] = FromAnsi( File[StrCount] );
						TCFile[StrCount] = 0;

						FString S(TCFile);
						TArray<FString> StringArray;
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
							GLastDir[eLASTDIR_WAV] = StringArray(0).Left( StringArray(0).InStr( TEXT("\\"), 1 ) );
						else
							GLastDir[eLASTDIR_WAV] = StringArray(0);

						TArray<FString> FilenamesArray;

						for( INT x = iStart ; x < StringArray.Num() ; ++x )
						{
							FString NewString;

							NewString = FString::Printf( TEXT("%s%s"), *Prefix, *(StringArray(x)) );
							new(FilenamesArray)FString( NewString );
						
							FString S = NewString;
						}

						WDlgImportSound l_dlg( NULL, this );
						l_dlg.DoModal( Package, Group, &FilenamesArray );

						GBrowserMaster->RefreshAll();

                        // gam ---
                        for( TObjectIterator<USoundGroup> It; It; ++It )
                            It->RefreshGroup(1);

						pComboPackage->SetCurrent( pComboPackage->FindStringExact( *l_dlg.Package) );
						pComboGroup->SetCurrent( pComboGroup->FindStringExact( *l_dlg.Group) );
						RefreshAll();
                        // --- gam
					}

					GFileManager->SetDefaultDirectory(appBaseDir());
				}
				break;

			case IDMN_SB_PLAY:
			{
				bPlayButtonDown = !bPlayButtonDown;

				if( bPlayButtonDown )
					OnPlay();
				else
					OnStop();
			}
			break;

			case IDMN_SB_LOOPING:
			{
				bLooping = !bLooping;
				UpdateMenu();
			}
			break;

			case IDMN_SB_STOP:
				OnStop();
				break;

			case IDMN_SB_FileSave:
				{
					OPENFILENAMEA ofn;
					char File[8192] = "\0";
					FString Package = pComboPackage->GetString( pComboPackage->GetCurrent() );

					::sprintf( File, "%s.uax", TCHAR_TO_ANSI( *Package ) );

					ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
					ofn.lStructSize = sizeof(OPENFILENAMEA);
					ofn.hwndOwner = hWnd;
					ofn.lpstrFile = File;
					ofn.nMaxFile = sizeof(char) * 8192;
					ofn.lpstrFilter = "Sound Packages (*.uax)\0*.uax\0All Files\0*.*\0\0";
					ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_UAX]) );
					ofn.lpstrDefExt = "uax";
					ofn.lpstrTitle = "Save Sound Package";
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
						GLastDir[eLASTDIR_UAX] = S.Left( S.InStr( TEXT("\\"), 1 ) );
					}
					}

					GFileManager->SetDefaultDirectory(appBaseDir());
				}
				break;

			case IDMN_SB_FileOpen:
				{
					OPENFILENAMEA ofn;
					char File[8192] = "\0";

					ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
					ofn.lStructSize = sizeof(OPENFILENAMEA);
					ofn.hwndOwner = hWnd;
					ofn.lpstrFile = File;
					ofn.nMaxFile = sizeof(char) * 8192;
					ofn.lpstrFilter = "Sound Packages (*.uax)\0*.uax\0All Files\0*.*\0\0";
					ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_UAX]) );
					ofn.lpstrDefExt = "uax";
					ofn.lpstrTitle = "Open Sound Package";
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
							GLastDir[eLASTDIR_UAX] = StringArray(0).Left( StringArray(0).InStr( TEXT("\\"), 1 ) );
						else
							GLastDir[eLASTDIR_UAX] = StringArray(0);

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

                        // gam ---
                        for( TObjectIterator<USoundGroup> It; It; ++It )
                            It->RefreshGroup(1);

						pComboPackage->SetCurrent( pComboPackage->FindStringExact( *SavePkgName ) );
						RefreshAll();

                        // --- gam
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
	virtual void RefreshAll()
	{
		guard(WBrowserSound::RefreshAll);
		RefreshPackages();
		RefreshGroups();
		RefreshSoundList();
		unguard;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WBrowserSound::OnSize);
		WBrowser::OnSize(Flags, NewX, NewY);
		PositionChildControls();
		InvalidateRect( hWnd, NULL, FALSE );
		UpdateMenu();
		unguard;
	}
	
	void RefreshPackages( void )
	{
		guard(WBrowserSound::RefreshPackages);

        // gam ---
		FString Package = pComboPackage->GetString( pComboPackage->GetCurrent() );
        // --- gam

		// PACKAGES
		//
		pComboPackage->Empty();

		FStringOutputDevice GetPropResult = FStringOutputDevice();
		GUnrealEd->Get( TEXT("OBJ"), TEXT("PACKAGES CLASS=Sound"), GetPropResult );

		TArray<FString> StringArray;
		GetPropResult.ParseIntoArray( TEXT(","), &StringArray );

		for( INT x = 0 ; x < StringArray.Num() ; ++x )
			pComboPackage->AddString( *(StringArray(x)) );

        // gam ---
        if( Package.Len() == 0 )
		pComboPackage->SetCurrent( 0 );
        else
        {
		    for( INT x = 0 ; x < pComboPackage->GetCount() ; ++x )
		    {
                if( pComboPackage->GetString(x) == Package )
        		    pComboPackage->SetCurrent( x );
		    }
        }
        // --- gam

		unguard;
	}
	void RefreshGroups( void )
	{
		guard(WBrowserSound::RefreshGroups);

		FString Package = pComboPackage->GetString( pComboPackage->GetCurrent() );

        // gam ---
		FString Group = pComboGroup->GetString( pComboGroup->GetCurrent() );
        // --- gam

		// GROUPS
		//
		pComboGroup->Empty();

		FStringOutputDevice GetPropResult = FStringOutputDevice();
		TCHAR l_ch[256];
		appSprintf( l_ch, TEXT("GROUPS CLASS=Sound PACKAGE=\"%s\""), *Package );
		GUnrealEd->Get( TEXT("OBJ"), l_ch, GetPropResult );

		TArray<FString> StringArray;
		GetPropResult.ParseIntoArray( TEXT(","), &StringArray );

		for( INT x = 0 ; x < StringArray.Num() ; ++x )
			pComboGroup->AddString( *(StringArray(x)) );

        // gam ---

        if( Group.Len() == 0 )
		pComboGroup->SetCurrent( 0 );
        else
        {
		    for( INT x = 0 ; x < pComboGroup->GetCount() ; ++x )
		    {
                if( pComboGroup->GetString(x) == Group )
        		    pComboGroup->SetCurrent( x );
		    }
        }
        // --- gam

		unguard;
	}
	void RefreshSoundList( void )
	{
		guard(WBrowserSound::RefreshSoundList);

		FString PackageName = pComboPackage->GetString( pComboPackage->GetCurrent() );
		FString Group = pComboGroup->GetString( pComboGroup->GetCurrent() );

		UPackage* Package = FindObject<UPackage>( ANY_PACKAGE, *PackageName );
		if( Package && Group.Len() )
			Package = FindObject<UPackage>( Package, *Group );

		// SOUNDS
		//
		pListSounds->Empty();

		FStringOutputDevice GetPropResult = FStringOutputDevice();
		TCHAR l_ch[256];

		if( pCheckGroupAll->IsChecked() )
			appSprintf( l_ch, TEXT("QUERY TYPE=Sound PACKAGE=\"%s\""), *PackageName );
		else
			appSprintf( l_ch, TEXT("QUERY TYPE=Sound PACKAGE=\"%s\" GROUP=\"%s\""), *PackageName, *Group );

		GUnrealEd->Get( TEXT("OBJ"), l_ch, GetPropResult );

		TArray<FString> StringArray;
		GetPropResult.ParseIntoArray( TEXT(" "), &StringArray );

		for( INT x = 0 ; x < StringArray.Num() ; ++x )
		{
			FString Desc = *(StringArray(x));

			UObject* Object = NULL;
			if( pCheckGroupAll->IsChecked() )
				Object = FindObject<UObject>( ANY_PACKAGE, *(StringArray(x)) );
			else
				Object = FindObject<UObject>( Package, *(StringArray(x)) );

			if( Cast<USoundGroup>( Object ) )
			{
				Desc = FString::Printf( TEXT("*%s"), *Desc );
			}
			else if( Cast<UProceduralSound>( Object ) )
			{
				Desc = FString::Printf( TEXT(">%s"), *Desc );
			}
			else
			{
				USound* Sound = Cast<USound>( Object );
				if( Sound )
				{
					Sound->GetData().Load();
					check(Sound->GetData().Num()>0);
					FWaveModInfo WaveInfo;
					WaveInfo.ReadWaveInfo(Sound->GetData());

					Desc = FString::Printf( TEXT("%s   [%d, %d, %d]"), *(StringArray(x)), *WaveInfo.pChannels, *WaveInfo.pBitsPerSample, *WaveInfo.pSamplesPerSec );
				}
			}

			pListSounds->AddString( *Desc );
		}

		pListSounds->SetCurrent( 0, 1 );

		unguard;
	}
	// Moves the child windows around so that they best match the window size.
	//
	void PositionChildControls( void )
	{
		guard(WBrowserSound::PositionChildControls);
		if( Container ) Container->RefreshControls();
		unguard;
	}

	// Notification delegates for child controls.
	//
	void OnComboPackageSelChange()
	{
		guard(WBrowserSound::OnComboPackageSelChange);
		RefreshGroups();
		RefreshSoundList();
		unguard;
	}
	void OnComboGroupSelChange()
	{
		guard(WBrowserSound::OnComboGroupSelChange);
		RefreshSoundList();
		unguard;
	}
	void OnListSoundsSelectionChange()
	{
		guard(WBrowserSound::OnListSoundsSelectionChange);
		if( bPlayButtonDown ) OnPlay();
		unguard;
	}
	void OnListSoundsDblClick()
	{
		guard(WBrowserSound::OnListSoundsDblClick);
		OnPlay();
		unguard;
	}
    // gam ---
    void OnListSoundsRightClick()
    {
		// Show a context menu for the currently selected item.
        HMENU root = LoadMenuIdX(hInstance, IDMENU_BrowserSound_Context);
		HMENU menu = GetSubMenu( root, 0 );
		POINT pt;
		::GetCursorPos( &pt );
		TrackPopupMenu( menu,
			TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
			pt.x, pt.y, 0,
			hWnd, NULL);
        DestroyMenu(root);
    }
    // --- gam
	void OnPlay()
	{
		guard(WBrowserSound::OnPlay);

		TCHAR l_chCmd[256];
		FString Name = GetCurrentPathName();
		appSprintf( l_chCmd, TEXT("AUDIO PLAY %s NAME=\"%s\""), (bLooping?TEXT("LOOPING"):TEXT("")), *Name );
		GUnrealEd->Exec( l_chCmd );
		unguard;
	}
	void OnStop()
	{
		guard(WBrowserSound::OnStop);
		GUnrealEd->Exec( TEXT("AUDIO PLAY NAME=None") );
		unguard;
	}
	void OnGroupAllClick()
	{
		guard(WBrowserSound::OnGroupAllClick);
		EnableWindow( pComboGroup->hWnd, !pCheckGroupAll->IsChecked() );
		RefreshSoundList();
		unguard;
	}
	virtual FString GetCurrentPathName()
	{
		guard(WBrowserSound::GetCurrentPathName);

		FString Package = pComboPackage->GetString( pComboPackage->GetCurrent() );
		FString Group = pComboGroup->GetString( pComboGroup->GetCurrent() );
		FString Name = GetSoundName();

		if( pCheckGroupAll->IsChecked() )
			return *Name;
		else
			if( Group.Len() )
				return *FString::Printf(TEXT("%s.%s.%s"), *Package, *Group, *Name );
			else
				return *FString::Printf(TEXT("%s.%s"), *Package, *Name );

		unguard;
	}
	FString GetSoundName()
	{
		FString Name = pListSounds->GetString( pListSounds->GetCurrent() );

		// Strip info from name

		if( Name[0] == '*' )
			Name = Name.Mid( 1, Name.Len()-1 );
		INT Pos = Name.InStr( TEXT("  ["), 0 );
		if( Pos > INDEX_NONE )
			Name = Name.Mid( 0, Pos-1 );

		return Name;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

