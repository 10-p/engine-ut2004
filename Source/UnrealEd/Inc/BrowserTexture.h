/*=============================================================================
	BrowserTexture : Browser window for textures
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

extern void Query( ULevel* Level, const TCHAR* Item, FString* pOutput );
extern FString GLastDir[eLASTDIR_MAX];
extern WDlgTexReplace* GDlgTexReplace;

// --------------------------------------------------------------
//
// NEW TEXTURE Dialog
//
// --------------------------------------------------------------

class WDlgNewTexture : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgNewTexture,WDialog,UnrealEd)

	// Variables.
	WButton OkButton;
	WButton CancelButton;
	WEdit PackageEdit;
	WEdit GroupEdit;
	WEdit NameEdit;
	WComboBox ClassCombo;
	WComboBox WidthCombo;
	WComboBox HeightCombo;

	FString defPackage, defGroup;
	TArray<FString>* Filenames;

	FString Package, Group, Name;

	// Constructor.
	WDlgNewTexture( UObject* InContext, WBrowser* InOwnerWindow )
		:	WDialog			( TEXT("New Texture"), IDDIALOG_NEW_TEXTURE, InOwnerWindow )
	,	OkButton		( this, IDOK,			FDelegate(this,(TDelegate)OnOk) )
	,	CancelButton	( this, IDCANCEL,		FDelegate(this,(TDelegate)EndDialogFalse) )
	,	PackageEdit		( this, IDEC_PACKAGE )
	,	GroupEdit		( this, IDEC_GROUP )
	,	NameEdit		( this, IDEC_NAME )
	,	ClassCombo		( this, IDCB_CLASS )
	,	WidthCombo		( this, IDCB_WIDTH )
	,	HeightCombo		( this, IDCB_HEIGHT )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgNewTexture::OnInitDialog);
		WDialog::OnInitDialog();

		PackageEdit.SetText( *defPackage );
		GroupEdit.SetText( *defGroup );
		::SetFocus( NameEdit.hWnd );

		WidthCombo.AddString( TEXT("1") );
		WidthCombo.AddString( TEXT("2") );
		WidthCombo.AddString( TEXT("4") );
		WidthCombo.AddString( TEXT("8") );
		WidthCombo.AddString( TEXT("16") );
		WidthCombo.AddString( TEXT("32") );
		WidthCombo.AddString( TEXT("64") );
		WidthCombo.AddString( TEXT("128") );
		WidthCombo.AddString( TEXT("256") );
		WidthCombo.SetCurrent(8);

		HeightCombo.AddString( TEXT("1") );
		HeightCombo.AddString( TEXT("2") );
		HeightCombo.AddString( TEXT("4") );
		HeightCombo.AddString( TEXT("8") );
		HeightCombo.AddString( TEXT("16") );
		HeightCombo.AddString( TEXT("32") );
		HeightCombo.AddString( TEXT("64") );
		HeightCombo.AddString( TEXT("128") );
		HeightCombo.AddString( TEXT("256") );
		HeightCombo.SetCurrent(8);

		FString Classes;

		Query( GUnrealEd->Level, TEXT("GETCHILDREN CLASS=MATERIAL CONCRETE=1"), &Classes);

		TArray<FString> Array;
		Classes.ParseIntoArray( TEXT(","), &Array );

		for( INT x = 0 ; x < Array.Num() ; ++x )
			ClassCombo.AddString( *(Array(x)) );
		ClassCombo.SetCurrent(0);

		PackageEdit.SetText( *defPackage );
		GroupEdit.SetText( *defGroup );

		Array.Empty();

		unguard;
	}
	virtual INT DoModal( FString InDefPackage, FString InDefGroup)
	{
		guard(WDlgNewTexture::DoModal);

		defPackage = InDefPackage;
		defGroup = InDefGroup;

		return WDialog::DoModal( hInstance );
		unguard;
	}
	void OnOk()
	{
		guard(WDlgNewTexture::OnOk);
		if( GetDataFromUser() )
		{
			GUnrealEd->Exec( *FString::Printf( TEXT("TEXTURE NEW NAME=\"%s\" CLASS=\"%s\" GROUP=\"%s\" USIZE=%s VSIZE=%s PACKAGE=\"%s\""),
				*NameEdit.GetText(), *ClassCombo.GetString( ClassCombo.GetCurrent() ), *GroupEdit.GetText(),
				*WidthCombo.GetString( WidthCombo.GetCurrent() ), *HeightCombo.GetString( HeightCombo.GetCurrent() ),
				*PackageEdit.GetText() ));
			EndDialog(TRUE);
		}
		unguard;
	}
	BOOL GetDataFromUser( void )
	{
		guard(WDlgNewTexture::GetDataFromUser);
		Package = PackageEdit.GetText();
		Group = GroupEdit.GetText();
		Name = NameEdit.GetText();

        // gam -- 
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
};

// --------------------------------------------------------------
//
// IMPORT TEXTURE Dialog
//
// --------------------------------------------------------------

class WDlgImportTexture : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgImportTexture,WDialog,UnrealEd)

	// Variables.
	WButton OkButton;
	WButton OkAllButton;
	WButton SkipButton;
	WButton CancelButton;
	WLabel FilenameStatic;
	WEdit PackageEdit;
	WEdit GroupEdit;
	WEdit NameEdit;
    WEdit NormalLodEdit; // gam
	WCheckBox MaskedCheck;
	WCheckBox MipMapCheck;
	WCheckBox DetailHackCheck;
    WCheckBox AlphaCheck;   // sjs

	FString defPackage, defGroup;
	TArray<FString>* Filenames;

	FString Package, Group, Name;
	BOOL bOKToAll;
	INT iCurrentFilename;
    INT NormalLod; // gam

	// Constructor.
	WDlgImportTexture( UObject* InContext, WBrowser* InOwnerWindow )
	:	WDialog			( TEXT("Import Texture"), IDDIALOG_IMPORT_TEXTURE, InOwnerWindow )
	,	OkButton		( this, IDOK,			FDelegate(this,(TDelegate)OnOk) )
	,	OkAllButton		( this, IDPB_OKALL,		FDelegate(this,(TDelegate)OnOkAll) )
	,	SkipButton		( this, IDPB_SKIP,		FDelegate(this,(TDelegate)OnSkip) )
	,	CancelButton	( this, IDCANCEL,		FDelegate(this,(TDelegate)EndDialogFalse) )
	,	PackageEdit		( this, IDEC_PACKAGE )
	,	GroupEdit		( this, IDEC_GROUP )
	,	NameEdit		( this, IDEC_NAME )
    ,   NormalLodEdit   ( this, IDEC_NORMAL_LOD ) // gam
	,	FilenameStatic	( this, IDSC_FILENAME )
	,	MaskedCheck		( this, IDCK_MASKED )
	,	MipMapCheck		( this, IDCK_MIPMAP )
	,	DetailHackCheck	( this, IDCK_DETAIL_HACK )
    ,   AlphaCheck      ( this, IDCK_ALPHA ) // sjs
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgImportTexture::OnInitDialog);
		WDialog::OnInitDialog();

		PackageEdit.SetText( *defPackage );
		GroupEdit.SetText( *defGroup );
        NormalLodEdit.SetText( TEXT("0") ); // gam
		::SetFocus( NameEdit.hWnd );

		bOKToAll = FALSE;
		iCurrentFilename = -1;
		SetNextFilename();
		MipMapCheck.SetCheck( BST_CHECKED );

		unguard;
	}
	virtual INT DoModal( FString InDefPackage, FString InDefGroup, TArray<FString>* InFilenames)
	{
		guard(WDlgImportTexture::DoModal);

		defPackage = InDefPackage;
		defGroup = InDefGroup;
		Filenames = InFilenames;

		return WDialog::DoModal( hInstance );
		unguard;
	}
	void OnOk()
	{
		guard(WDlgImportTexture::OnOk);
		if( GetDataFromUser() )
		{
			ImportFile( (*Filenames)(iCurrentFilename) );
			SetNextFilename();
		}
		unguard;
	}
	void OnOkAll()
	{
		guard(WDlgImportTexture::OnOkAll);
		if( GetDataFromUser() )
		{
			ImportFile( (*Filenames)(iCurrentFilename) );
			bOKToAll = TRUE;
			SetNextFilename();
		}
		unguard;
	}
	void OnSkip()
	{
		guard(WDlgImportTexture::OnSkip);
		if( GetDataFromUser() )
			SetNextFilename();
		unguard;
	}
	void ImportTexture( void )
	{
		guard(WDlgImportTexture::ImportTexture);
		unguard;
	}
	void RefreshName( void )
	{
		guard(WDlgImportTexture::RefreshName);
		FilenameStatic.SetText( *(*Filenames)(iCurrentFilename) );

		FString Name = GetFilenameOnly( (*Filenames)(iCurrentFilename) );

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
		guard(WDlgImportTexture::GetDataFromUser);
		Package = PackageEdit.GetText();
		Group = GroupEdit.GetText();
		Name = NameEdit.GetText();
        NormalLod = appAtoi (*NormalLodEdit.GetText());

        // gam -- 
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
		guard(WDlgImportTexture::SetNextFilename);
		iCurrentFilename++;
		if( iCurrentFilename == Filenames->Num() ) {
			EndDialogTrue();
			return;
		}

		if( bOKToAll ) {
			RefreshName();
			GetDataFromUser();
			ImportFile( (*Filenames)(iCurrentFilename) );
			SetNextFilename();
			return;
		};

		RefreshName();

		unguard;
	}
	void ImportFile( FString Filename )
	{
		guard(WDlgImportTexture::ImportFile);
		TCHAR l_chCmd[512];

		if( Group.Len() )
			appSprintf( l_chCmd, TEXT("TEXTURE IMPORT FILE=\"%s\" NAME=\"%s\" PACKAGE=\"%s\" GROUP=\"%s\" MIPS=%d MASKED=%d Alpha=%d NormalLOD=%d"),
				*(*Filenames)(iCurrentFilename), *Name, *Package, *Group,
				MipMapCheck.IsChecked(), MaskedCheck.IsChecked(), AlphaCheck.IsChecked(), NormalLod ); // sjs, gam
		else
			appSprintf( l_chCmd, TEXT("TEXTURE IMPORT FILE=\"%s\" NAME=\"%s\" PACKAGE=\"%s\" MIPS=%d MASKED=%d Alpha=%d NormalLOD=%d"),
				*(*Filenames)(iCurrentFilename), *Name, *Package,
				MipMapCheck.IsChecked(), MaskedCheck.IsChecked(), AlphaCheck.IsChecked(), NormalLod ); // sjs, gam

		GUnrealEd->Exec( l_chCmd );

		FString TextureName = *FString::Printf( TEXT("%s%s%s.%s"),
			*Package,
			Group.Len() ? TEXT(".") : TEXT(""),
			Group.Len() ? *Group : TEXT(""),
			*Name );

		if( DetailHackCheck.IsChecked() )
			GUnrealEd->Exec( *FString::Printf(TEXT("TEXTURE DETAILHACK NAME=%s"), *TextureName ) );

#if 0 // merge_hack
		if( CompressionCombo.GetCurrent() > 0 )
			GUnrealEd->Exec( *FString::Printf(TEXT("TEXTURE COMPRESS NAME=%s FORMAT=%s"),
				*TextureName, *CompressionCombo.GetString( CompressionCombo.GetCurrent() ) ) );
#endif
		unguard;
	}
};

// --------------------------------------------------------------
//
// WPageMaterials
//
// --------------------------------------------------------------

struct {
	TCHAR ToolTip[64];
	INT ID;
} ToolTips_PageMaterials[] = {
	NULL, 0
};

class WPageMaterials : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WPageMaterials,WPropertyPage,Window)

	WPropertySheet* PropSheet;

	TMap<DWORD,FWindowAnchor> Anchors;

	FContainer *Container;
	WLabel *ViewportLabel;
	WScrollBar *ScrollBar;
	WComboBox *PackageCombo, *GroupCombo;
	WCheckBox *GroupAllCheck, *RealtimeCheck;

	UViewport* Viewport;
	INT iScroll;
	UBOOL ShowRealtime;

	WToolTip* ToolTipCtrl;

	// Structors.
	WPageMaterials ( WWindow* InOwnerWindow )
	:	WPropertyPage( InOwnerWindow )
	{
		Viewport = NULL;
		ViewportLabel = NULL;
		ScrollBar = NULL;
		PackageCombo = GroupCombo = NULL;
		GroupAllCheck = RealtimeCheck = NULL;
		Container = NULL;
		iScroll = 0;
		ShowRealtime = 0;
	}

	virtual void OpenWindow( INT InDlgId, HMODULE InHMOD )
	{
		guard(WPageMaterials::OpenWindow);
		WPropertyPage::OpenWindow( InDlgId, InHMOD );

		Container = new FContainer();

		ViewportLabel = new WLabel( this, IDSC_VIEWPORT );
		ViewportLabel->OpenWindow( 1 );
		ScrollBar = new WScrollBar( this, IDSB_SCROLLBAR );
		ScrollBar->OpenWindow( 1, 0, 0, 0, 0, 1 );
		PackageCombo = new WComboBox( this, IDCB_PACKAGE );
		PackageCombo->OpenWindow( 1, 1 );
		PackageCombo->SelectionChangeDelegate = FDelegate(this, (TDelegate)OnPackageComboSelChange);
		GroupCombo = new WComboBox( this, IDCB_GROUP );
		GroupCombo->OpenWindow( 1, 1 );
		GroupCombo->SelectionChangeDelegate = FDelegate(this, (TDelegate)OnGroupComboSelChange);
		GroupAllCheck = new WCheckBox( this, IDCK_GRP_ALL, FDelegate(this, (TDelegate)OnGroupAllClick) );
		GroupAllCheck->OpenWindow( 1, 0, 0, 1, 1, TEXT("All"), 1, 0, BS_PUSHLIKE );
		RealtimeCheck = new WCheckBox( this, IDCK_REALTIME, FDelegate(this, (TDelegate)OnRealtimeClick) );
		RealtimeCheck->OpenWindow( 1, 0, 0, 1, 1, TEXT("!"), 1, 0, BS_PUSHLIKE );

		PlaceControl( ViewportLabel );
		PlaceControl( ScrollBar );
		PlaceControl( PackageCombo );
		PlaceControl( GroupCombo );
		PlaceControl( GroupAllCheck );
		PlaceControl( RealtimeCheck );

		Finalize();

		// Viewport
		FName Name = TEXT("TextureBrowser");
		Viewport = GUnrealEd->Client->NewViewport( Name );
		GUnrealEd->Level->SpawnViewActor( Viewport );
		Viewport->Actor->ShowFlags = SHOW_StandardView | SHOW_ChildWindow | SHOW_NoFallbackMaterials;
		Viewport->Actor->RendMap   = REN_TexBrowser;
		Viewport->Actor->Misc2 = iScroll;
		Viewport->Group = NAME_None;
		Viewport->MiscRes = NULL;
		Viewport->Input->Init( Viewport );
		Viewport->OpenWindow( (DWORD)ViewportLabel->hWnd, 0, 320, 200, 0, 0 );

		INT Top = 0;
		Anchors.Set( (DWORD)PackageCombo->hWnd,		FWindowAnchor( hWnd, PackageCombo->hWnd,			ANCHOR_TL, 4, Top,						ANCHOR_RIGHT|ANCHOR_HEIGHT, -4, STANDARD_CTRL_HEIGHT ) );
		Top += STANDARD_CTRL_HEIGHT+2;
		Anchors.Set( (DWORD)RealtimeCheck->hWnd,	FWindowAnchor( hWnd, RealtimeCheck->hWnd,			ANCHOR_TL, 4, Top,						ANCHOR_WIDTH|ANCHOR_HEIGHT, 21, STANDARD_CTRL_HEIGHT ) );
		Anchors.Set( (DWORD)GroupAllCheck->hWnd,	FWindowAnchor( hWnd, GroupAllCheck->hWnd,			ANCHOR_TL, 4+21+2, Top,					ANCHOR_WIDTH|ANCHOR_HEIGHT, 64, STANDARD_CTRL_HEIGHT ) );
		Anchors.Set( (DWORD)GroupCombo->hWnd,		FWindowAnchor( hWnd, GroupCombo->hWnd,				ANCHOR_TL, 4+21+64+2, Top,				ANCHOR_RIGHT|ANCHOR_HEIGHT, -4, STANDARD_CTRL_HEIGHT ) );
		Top += STANDARD_CTRL_HEIGHT+2;
		Anchors.Set( (DWORD)ViewportLabel->hWnd,	FWindowAnchor( hWnd, ViewportLabel->hWnd,							ANCHOR_TL, 0, Top,	ANCHOR_BR, -STANDARD_SB_WIDTH, 0 ) );
		Anchors.Set( (DWORD)Viewport->GetWindow(),	FWindowAnchor( ViewportLabel->hWnd, (HWND)Viewport->GetWindow(),	ANCHOR_TL,0,0,		ANCHOR_BR,0,0 ) );
		Anchors.Set( (DWORD)ScrollBar->hWnd,		FWindowAnchor( hWnd, ScrollBar->hWnd,								ANCHOR_TOP | ANCHOR_RIGHT, -STANDARD_SB_WIDTH, Top,	ANCHOR_BOTTOM | ANCHOR_WIDTH, STANDARD_SB_WIDTH, 0 ) );


		Container->SetAnchors( &Anchors );

		ToolTipCtrl = new WToolTip(this);
		ToolTipCtrl->OpenWindow();
		for( INT tooltip = 0 ; ToolTips_PageMaterials[tooltip].ID > 0 ; ++tooltip )
			ToolTipCtrl->AddTool( GetDlgItem( hWnd, ToolTips_PageMaterials[tooltip].ID ), ToolTips_PageMaterials[tooltip].ToolTip, tooltip );

		PositionChildControls();

		unguard;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WPageMaterials::OnSize);
		WPropertyPage::OnSize(Flags, NewX, NewY);
		PositionChildControls();
		InvalidateRect( hWnd, NULL, FALSE );
		Refresh();
		unguard;
	}
	void OnDestroy()
	{
		guard(WPageMaterials::OnDestroy);
		WPropertyPage::OnDestroy();

		delete Container;

		delete PackageCombo;
		delete GroupCombo;
		delete GroupAllCheck;
		delete RealtimeCheck;
		delete ViewportLabel;
		delete ScrollBar;
		//delete Viewport;

		unguard;
	}
	void PositionChildControls( void )
	{
		guard(WPageMaterials::PositionChildControls);
		if( Container ) Container->RefreshControls();
		unguard;
	}
	virtual void Refresh()
	{
		guard(WPageMaterials::Refresh);
		WPropertyPage::Refresh();

		PositionChildControls();
		RefreshViewport();
		RefreshScrollBar();

		unguard;
	}
	void RefreshViewport()
	{
		guard(WPageMaterials::RefreshViewport);
		if( Viewport )	Viewport->Repaint( 1 );
		unguard;
	}
	void RefreshScrollBar()
	{
		guard(WPageMaterials::RefreshScrollBar);

		if( !ScrollBar ) return;

		// Set the scroll bar to have a valid range.
		//
		SCROLLINFO si;
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_DISABLENOSCROLL | SIF_RANGE | SIF_POS | SIF_PAGE;
		si.nPage = Viewport->SizeY;
		si.nMin = 0;
		si.nMax = GTBOptions->LastScroll+Viewport->SizeY;
		si.nPos = iScroll;
		iScroll = SetScrollInfo( ScrollBar->hWnd, SB_CTL, &si, TRUE );

		unguard;
	}
	virtual void OnVScroll( WPARAM wParam, LPARAM lParam )
	{
		if( (HWND)lParam == ScrollBar->hWnd )
			switch(LOWORD(wParam))
			{
				case SB_LINEUP:
					iScroll -= 64;
					iScroll = Max( iScroll, 0 );
					RefreshTextureList();
					break;

				case SB_LINEDOWN:
					iScroll += 64;
					iScroll = Min( iScroll, GTBOptions->LastScroll );
					RefreshTextureList();
					break;

				case SB_PAGEUP:
					iScroll -= 256;
					iScroll = Max( iScroll, 0 );
					RefreshTextureList();
					break;

				case SB_PAGEDOWN:
					iScroll += 256;
					iScroll = Min( iScroll, GTBOptions->LastScroll );
					RefreshTextureList();
					break;

				case SB_THUMBTRACK:
					iScroll = (short int)HIWORD(wParam);
					RefreshTextureList();
					break;
			}
	}
	void RefreshTextureList( void )
	{
		guard(WPageMaterials::RefreshTextureList);

		FString Package = PackageCombo->GetString( PackageCombo->GetCurrent() );
		FString Group = GroupCombo->GetString( GroupCombo->GetCurrent() );

		TCHAR l_chCmd[1024];

		if( GroupAllCheck->IsChecked() )
		{
			appSprintf( l_chCmd, TEXT("CAMERA UPDATE FLAGS=%d MISC2=%d REN=%d NAME=TextureBrowser PACKAGE=\"%s\" GROUP=\"%s\""),
				SHOW_StandardView | SHOW_ChildWindow | SHOW_NoFallbackMaterials | (ShowRealtime?SHOW_RealTime:0),
				iScroll,
				REN_TexBrowser,
				*Package,
				TEXT("(All)") );
		}
		else
		{
			appSprintf( l_chCmd, TEXT("CAMERA UPDATE FLAGS=%d MISC2=%d REN=%d NAME=TextureBrowser PACKAGE=\"%s\" GROUP=\"%s\""),
				SHOW_StandardView | SHOW_ChildWindow | SHOW_NoFallbackMaterials | (ShowRealtime?SHOW_RealTime:0),
				iScroll,
				REN_TexBrowser,
				*Package,
				*Group );
		}
		GUnrealEd->Exec( l_chCmd );

		RefreshScrollBar();

		unguard;
	}
	void OnPackageComboSelChange()
	{
		guard(WPageMaterials::OnPackageComboSelChange);
		RefreshGroups();
		iScroll = 0;
		RefreshTextureList();
		unguard;
	}
	void OnGroupComboSelChange()
	{
		guard(WPageMaterials::OnGroupComboSelChange);
		iScroll = 0;
		RefreshTextureList();
		unguard;
	}
	void OnGroupAllClick()
	{
		guard(WPageMaterials::OnGroupAllClick);
		EnableWindow( GroupCombo->hWnd, !GroupAllCheck->IsChecked() );
		RefreshTextureList();
		unguard;
	}
	void OnRealtimeClick()
	{
		guard(WPageMaterials::OnRealtimeClick);
		ShowRealtime = RealtimeCheck->IsChecked();
		Viewport->Actor->ShowFlags &= ~SHOW_RealTime;
		if( ShowRealtime )
			Viewport->Actor->ShowFlags |= SHOW_RealTime;
		unguard;
	}
	void RefreshPackages( void )
	{
		guard(WPageMaterials::RefreshPackages);

		INT Current = PackageCombo->GetCurrent();
		Current = (Current != CB_ERR) ? Current : 0;

		// PACKAGES
		//
		PackageCombo->Empty();

		FStringOutputDevice GetPropResult = FStringOutputDevice();
		GUnrealEd->Get( TEXT("OBJ"), TEXT("PACKAGES CLASS=Material"), GetPropResult );

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
		guard(WPageMaterials::RefreshGroups);

		FString Package = PackageCombo->GetString( PackageCombo->GetCurrent() );
		INT Current = GroupCombo->GetCurrent();
		Current = (Current != CB_ERR) ? Current : 0;

		// GROUPS
		//
		GroupCombo->Empty();

		FStringOutputDevice GetPropResult = FStringOutputDevice();
		TCHAR l_ch[256];
		appSprintf( l_ch, TEXT("GROUPS CLASS=Material PACKAGE=\"%s\""), *Package );
		GUnrealEd->Get( TEXT("OBJ"), l_ch, GetPropResult );

		TArray<FString> StringArray;
		GetPropResult.ParseIntoArray( TEXT(","), &StringArray );

		for( INT x = 0 ; x < StringArray.Num() ; ++x )
			GroupCombo->AddString( *(StringArray(x)) );

		GroupCombo->SetCurrent(Current);
		StringArray.Empty();

		unguard;
	}
};

// --------------------------------------------------------------
//
// WPageUsed
//
// --------------------------------------------------------------

struct {
	TCHAR ToolTip[64];
	INT ID;
} ToolTips_PageUsed[] = {
	NULL, 0
};

class WPageUsed : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WPageUsed,WPropertyPage,Window)

	WPropertySheet* PropSheet;

	TMap<DWORD,FWindowAnchor> Anchors;

	FContainer *Container;
	WLabel *ViewportLabel;
	WScrollBar *ScrollBar;

	UViewport* Viewport;
	INT iScroll;

	WToolTip* ToolTipCtrl;

	// Structors.
	WPageUsed ( WWindow* InOwnerWindow )
	:	WPropertyPage( InOwnerWindow )
	{
		Viewport = NULL;
		ViewportLabel = NULL;
		ScrollBar = NULL;
		Container = NULL;
		iScroll = 0;
	}

	virtual void OpenWindow( INT InDlgId, HMODULE InHMOD )
	{
		guard(WPageUsed::OpenWindow);
		WPropertyPage::OpenWindow( InDlgId, InHMOD );

		Container = new FContainer();

		ViewportLabel = new WLabel( this, IDSC_VIEWPORT );
		ViewportLabel->OpenWindow( 1 );
		ScrollBar = new WScrollBar( this, IDSB_SCROLLBAR );
		ScrollBar->OpenWindow( 1, 0, 0, 0, 0, 1 );

		PlaceControl( ViewportLabel );
		PlaceControl( ScrollBar );

		Finalize();

		// Viewport
		FName Name = TEXT("TextureBrowserUsed");
		Viewport = GUnrealEd->Client->NewViewport( Name );
		GUnrealEd->Level->SpawnViewActor( Viewport );
		Viewport->Actor->ShowFlags = SHOW_StandardView | SHOW_ChildWindow | SHOW_NoFallbackMaterials;
		Viewport->Actor->RendMap   = REN_TexBrowserUsed;
		Viewport->Actor->Misc2 = iScroll;
		Viewport->Group = NAME_None;
		Viewport->MiscRes = NULL;
		Viewport->Input->Init( Viewport );
		Viewport->OpenWindow( (DWORD)ViewportLabel->hWnd, 0, 320, 200, 0, 0 );

		INT Top = 0;
		Anchors.Set( (DWORD)ViewportLabel->hWnd,	FWindowAnchor( hWnd, ViewportLabel->hWnd,							ANCHOR_TL, 0, Top,	ANCHOR_BR, -STANDARD_SB_WIDTH, 0 ) );
		Anchors.Set( (DWORD)Viewport->GetWindow(),	FWindowAnchor( ViewportLabel->hWnd, (HWND)Viewport->GetWindow(),	ANCHOR_TL,0,0,		ANCHOR_BR,0,0 ) );
		Anchors.Set( (DWORD)ScrollBar->hWnd,		FWindowAnchor( hWnd, ScrollBar->hWnd,								ANCHOR_TOP | ANCHOR_RIGHT, -STANDARD_SB_WIDTH, Top,	ANCHOR_BOTTOM | ANCHOR_WIDTH, STANDARD_SB_WIDTH, 0 ) );

		Container->SetAnchors( &Anchors );

		ToolTipCtrl = new WToolTip(this);
		ToolTipCtrl->OpenWindow();
		for( INT tooltip = 0 ; ToolTips_PageUsed[tooltip].ID > 0 ; ++tooltip )
			ToolTipCtrl->AddTool( GetDlgItem( hWnd, ToolTips_PageUsed[tooltip].ID ), ToolTips_PageUsed[tooltip].ToolTip, tooltip );

		PositionChildControls();

		unguard;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WPageUsed::OnSize);
		WPropertyPage::OnSize(Flags, NewX, NewY);
		PositionChildControls();
		InvalidateRect( hWnd, NULL, FALSE );
		Refresh();
		unguard;
	}
	void OnDestroy()
	{
		guard(WPageUsed::OnDestroy);
		WPropertyPage::OnDestroy();

		delete Container;

		delete ViewportLabel;
		delete ScrollBar;
		//delete Viewport;

		unguard;
	}
	void PositionChildControls( void )
	{
		guard(WPageUsed::PositionChildControls);
		if( Container ) Container->RefreshControls();
		unguard;
	}
	virtual void Refresh()
	{
		guard(WPageUsed::Refresh);
		WPropertyPage::Refresh();

		PositionChildControls();
		RefreshViewport();
		RefreshScrollBar();

		unguard;
	}
	void RefreshViewport()
	{
		guard(WPageUsed::RefreshViewport);
		if( Viewport )
		{
			Viewport->Actor->Misc2 = iScroll;
			Viewport->Repaint( 1 );
		}
		unguard;
	}
	void RefreshScrollBar()
	{
		guard(WPageUsed::RefreshScrollBar);

		if( !ScrollBar ) return;

		// Set the scroll bar to have a valid range.
		//
		SCROLLINFO si;
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_DISABLENOSCROLL | SIF_RANGE | SIF_POS | SIF_PAGE;
		si.nPage = Viewport->SizeY;
		si.nMin = 0;
		si.nMax = GTBOptions->LastScrollUsed+Viewport->SizeY;
		si.nPos = iScroll;
		iScroll = SetScrollInfo( ScrollBar->hWnd, SB_CTL, &si, TRUE );

		Viewport->Actor->Misc2 = iScroll;
		unguard;
	}
	virtual void OnVScroll( WPARAM wParam, LPARAM lParam )
	{
		if( (HWND)lParam == ScrollBar->hWnd )
			switch(LOWORD(wParam))
			{
				case SB_LINEUP:
					iScroll -= 64;
					iScroll = Max( iScroll, 0 );
					RefreshScrollBar();
					RefreshViewport();
					break;

				case SB_LINEDOWN:
					iScroll += 64;
					iScroll = Min( iScroll, GTBOptions->LastScrollUsed );
					RefreshScrollBar();
					RefreshViewport();
					break;

				case SB_PAGEUP:
					iScroll -= 256;
					iScroll = Max( iScroll, 0 );
					RefreshScrollBar();
					RefreshViewport();
					break;

				case SB_PAGEDOWN:
					iScroll += 256;
					iScroll = Min( iScroll, GTBOptions->LastScrollUsed );
					RefreshScrollBar();
					RefreshViewport();
					break;

				case SB_THUMBTRACK:
					iScroll = (short int)HIWORD(wParam);
					RefreshScrollBar();
					RefreshViewport();
					break;
			}
	}
};

// --------------------------------------------------------------
//
// WPageMRU
//
// --------------------------------------------------------------

struct {
	TCHAR ToolTip[64];
	INT ID;
} ToolTips_PageMRU[] = {
	NULL, 0
};

class WPageMRU : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WPageMRU,WPropertyPage,Window)

	WPropertySheet* PropSheet;

	TMap<DWORD,FWindowAnchor> Anchors;

	FContainer *Container;
	WLabel *ViewportLabel;
	WScrollBar *ScrollBar;

	UViewport* Viewport;
	INT iScroll;

	WToolTip* ToolTipCtrl;

	// Structors.
	WPageMRU ( WWindow* InOwnerWindow )
	:	WPropertyPage( InOwnerWindow )
	{
		Viewport = NULL;
		ViewportLabel = NULL;
		ScrollBar = NULL;
		Container = NULL;
		iScroll = 0;
	}

	virtual void OpenWindow( INT InDlgId, HMODULE InHMOD )
	{
		guard(WPageMRU::OpenWindow);
		WPropertyPage::OpenWindow( InDlgId, InHMOD );

		Container = new FContainer();

		ViewportLabel = new WLabel( this, IDSC_VIEWPORT );
		ViewportLabel->OpenWindow( 1 );
		ScrollBar = new WScrollBar( this, IDSB_SCROLLBAR );
		ScrollBar->OpenWindow( 1, 0, 0, 0, 0, 1 );

		PlaceControl( ViewportLabel );
		PlaceControl( ScrollBar );

		Finalize();

		// Viewport
		FName Name = TEXT("TextureBrowserMRU");
		Viewport = GUnrealEd->Client->NewViewport( Name );
		GUnrealEd->Level->SpawnViewActor( Viewport );
		Viewport->Actor->ShowFlags = SHOW_StandardView | SHOW_ChildWindow | SHOW_NoFallbackMaterials;
		Viewport->Actor->RendMap   = REN_TexBrowserMRU;
		Viewport->Actor->Misc2 = iScroll;
		Viewport->Group = NAME_None;
		Viewport->MiscRes = NULL;
		Viewport->Input->Init( Viewport );
		Viewport->OpenWindow( (DWORD)ViewportLabel->hWnd, 0, 320, 200, 0, 0 );

		INT Top = 0;
		Anchors.Set( (DWORD)ViewportLabel->hWnd,	FWindowAnchor( hWnd, ViewportLabel->hWnd,							ANCHOR_TL, 0, Top,	ANCHOR_BR, -STANDARD_SB_WIDTH, 0 ) );
		Anchors.Set( (DWORD)Viewport->GetWindow(),	FWindowAnchor( ViewportLabel->hWnd, (HWND)Viewport->GetWindow(),	ANCHOR_TL,0,0,		ANCHOR_BR,0,0 ) );
		Anchors.Set( (DWORD)ScrollBar->hWnd,		FWindowAnchor( hWnd, ScrollBar->hWnd,								ANCHOR_TOP | ANCHOR_RIGHT, -STANDARD_SB_WIDTH, Top,	ANCHOR_BOTTOM | ANCHOR_WIDTH, STANDARD_SB_WIDTH, 0 ) );

		Container->SetAnchors( &Anchors );

		ToolTipCtrl = new WToolTip(this);
		ToolTipCtrl->OpenWindow();
		for( INT tooltip = 0 ; ToolTips_PageMRU[tooltip].ID > 0 ; ++tooltip )
			ToolTipCtrl->AddTool( GetDlgItem( hWnd, ToolTips_PageMRU[tooltip].ID ), ToolTips_PageMRU[tooltip].ToolTip, tooltip );

		PositionChildControls();

		unguard;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WPageMRU::OnSize);
		WPropertyPage::OnSize(Flags, NewX, NewY);
		PositionChildControls();
		InvalidateRect( hWnd, NULL, FALSE );
		Refresh();
		unguard;
	}
	void OnDestroy()
	{
		guard(WPageMRU::OnDestroy);
		WPropertyPage::OnDestroy();

		delete Container;

		delete ViewportLabel;
		delete ScrollBar;
		//delete Viewport;

		unguard;
	}
	void PositionChildControls( void )
	{
		guard(WPageMRU::PositionChildControls);
		if( Container ) Container->RefreshControls();
		unguard;
	}
	virtual void Refresh()
	{
		guard(WPageMRU::Refresh);
		WPropertyPage::Refresh();

		PositionChildControls();
		RefreshViewport();
		RefreshScrollBar();

		unguard;
	}
	void RefreshViewport()
	{
		guard(WPageMRU::RefreshViewport);
		if( Viewport )
		{
			Viewport->Actor->Misc2 = iScroll;
			Viewport->Repaint( 1 );
		}
		unguard;
	}
	void RefreshScrollBar()
	{
		guard(WPageMRU::RefreshScrollBar);

		if( !ScrollBar ) return;

		// Set the scroll bar to have a valid range.
		//
		SCROLLINFO si;
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_DISABLENOSCROLL | SIF_RANGE | SIF_POS | SIF_PAGE;
		si.nPage = Viewport->SizeY;
		si.nMin = 0;
		si.nMax = GTBOptions->LastScrollMRU+Viewport->SizeY;
		si.nPos = iScroll;
		iScroll = SetScrollInfo( ScrollBar->hWnd, SB_CTL, &si, TRUE );

		Viewport->Actor->Misc2 = iScroll;
		unguard;
	}
	virtual void OnVScroll( WPARAM wParam, LPARAM lParam )
	{
		if( (HWND)lParam == ScrollBar->hWnd )
			switch(LOWORD(wParam))
			{
				case SB_LINEUP:
					iScroll -= 64;
					iScroll = Max( iScroll, 0 );
					RefreshScrollBar();
					RefreshViewport();
					break;

				case SB_LINEDOWN:
					iScroll += 64;
					iScroll = Min( iScroll, GTBOptions->LastScrollMRU );
					RefreshScrollBar();
					RefreshViewport();
					break;

				case SB_PAGEUP:
					iScroll -= 256;
					iScroll = Max( iScroll, 0 );
					RefreshScrollBar();
					RefreshViewport();
					break;

				case SB_PAGEDOWN:
					iScroll += 256;
					iScroll = Min( iScroll, GTBOptions->LastScrollMRU );
					RefreshScrollBar();
					RefreshViewport();
					break;

				case SB_THUMBTRACK:
					iScroll = (short int)HIWORD(wParam);
					RefreshScrollBar();
					RefreshViewport();
					break;
			}
	}
};

// --------------------------------------------------------------
//
// WBrowserTexture
//
// --------------------------------------------------------------

#define ID_TB_TOOLBAR	29040
TBBUTTON tbBTButtons[] = {
	{ 0, IDMN_MB_DOCK, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 1, IDMN_TB_FileOpen, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 2, IDMN_TB_FileSave, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 3, IDMN_TB_PROPERTIES, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 4, IDMN_TB_LOAD_ENTIRE_PACKAGE, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 5, IDMN_TB_PREV_GRP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 6, IDMN_TB_NEXT_GRP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
};
struct {
	TCHAR ToolTip[64];
	INT ID;
} ToolTips_BT[] = {
	TEXT("Toggle Dock Status"), IDMN_MB_DOCK,
	TEXT("Open Package"), IDMN_TB_FileOpen,
	TEXT("Save Package"), IDMN_TB_FileSave,
	TEXT("Texture Properties"), IDMN_TB_PROPERTIES,
	TEXT("Previous Group"), IDMN_TB_PREV_GRP,
	TEXT("Next Group"), IDMN_TB_NEXT_GRP,
	TEXT("Load Entire Package"), IDMN_TB_LOAD_ENTIRE_PACKAGE,
	NULL, 0
};

class WBrowserTexture : public WBrowser
{
	DECLARE_WINDOWCLASS(WBrowserTexture,WBrowser,Window)

	TArray<WDlgTexProp*> PropWindows;
	TMap<DWORD,FWindowAnchor> Anchors;

	FContainer *Container;
	WLabel *FilterLabel;
	WEdit *FilterEdit;
	WLabel* ViewportLabel;
	HWND hWndToolBar;
	WToolTip *ToolTipCtrl;
	MRUList* mrulist;

	WPropertySheet* PropSheet;
	WPageMaterials* MaterialsPage;
	WPageUsed* UsedPage;
	WPageMRU* MRUPage;

	// Structors.
	WBrowserTexture( FName InPersistentName, WWindow* InOwnerWindow, HWND InEditorFrame )
	:	WBrowser( InPersistentName, InOwnerWindow, InEditorFrame )
	{
		Container = NULL;
		MaterialsPage = NULL;
		UsedPage = NULL;
		MRUPage = NULL;
		FilterLabel = NULL;
		FilterEdit = NULL;
		ViewportLabel = NULL;
		GTBOptions->TexViewSize = TVS_50_PCT;
		GTBOptions->TypeFilter = MTF_Textures | MTF_Shaders |  MTF_Modifiers | MTF_Combiners | MTF_FinalBlends;
		GTBOptions->IUFilter = IUF_Actors | IUF_Brushes | IUF_StaticMeshes | IUF_StaticMeshes;
		MenuID = IDMENU_BrowserTexture;
		BrowserID = eBROWSER_TEXTURE;
		Description = TEXT("Textures");
		mrulist = NULL;
	}

	// WBrowser interface.
	void OpenWindow( UBOOL bChild )
	{
		guard(WBrowserTexture::OpenWindow);
		WBrowser::OpenWindow( bChild );
		SetCaption();
		unguard;
	}
	void OnCreate()
	{
		guard(WBrowserTexture::OnCreate);
		WBrowser::OnCreate();

		SetMenu( hWnd, LoadMenuIdX(hInstance, IDMENU_BrowserTexture) );

		Container = new FContainer();

		FilterLabel = new WLabel( this, IDST_FILTER );
		FilterLabel->OpenWindow( 1, 0 );
		FilterLabel->SetText( TEXT("Filter : ") );
		FilterEdit = new WEdit( this, IDEC_FILTER );
		FilterEdit->OpenWindow( 1, 0, 0 );
		FilterEdit->SetText( TEXT("") );
		FilterEdit->ChangeDelegate = FDelegate(this, (TDelegate)OnFilterEditChange);
		ViewportLabel = new WLabel( this, IDSC_VIEWPORT );
		ViewportLabel->OpenWindow( 1, 0 );

		// Create the sheet
		PropSheet = new WPropertySheet( ViewportLabel, IDPS_MATERIAL_BROWSER );
		PropSheet->OpenWindow( 1, 1, 0 );

		// Create the pages for the sheet
		MaterialsPage = new WPageMaterials( PropSheet->Tabs );
		MaterialsPage->OpenWindow( IDPP_TB_MATERIALS, GUnrealEdModule);//GetModuleHandleA("unrealed.exe") );
		MaterialsPage->PropSheet = PropSheet;
		PropSheet->AddPage( MaterialsPage );

		UsedPage = new WPageUsed( PropSheet->Tabs );
		UsedPage->OpenWindow( IDPP_TB_USED, GUnrealEdModule);//GetModuleHandleA("unrealed.exe") );
		UsedPage->PropSheet = PropSheet;
		PropSheet->AddPage( UsedPage );

		MRUPage = new WPageMRU( PropSheet->Tabs );
		MRUPage->OpenWindow( IDPP_TB_MRU, GUnrealEdModule);//GetModuleHandleA("unrealed.exe") );
		MRUPage->PropSheet = PropSheet;
		PropSheet->AddPage( MRUPage );

		PropSheet->SetCurrent( 0 );

		if(!GConfig->GetInt( *PersistentName, TEXT("TexViewSize"), GTBOptions->TexViewSize, TEXT("UnrealEd.ini") ))	GTBOptions->TexViewSize = TVS_50_PCT;

		hWndToolBar = CreateToolbarEx( 
			hWnd, WS_CHILD | WS_BORDER | WS_VISIBLE | CCS_ADJUSTABLE,
			IDB_BrowserTexture_TOOLBAR,
			7,
			hInstance,
			IDB_BrowserTexture_TOOLBAR,
			(LPCTBBUTTON)&tbBTButtons,
			10,
			16,16,
			16,16,
			sizeof(TBBUTTON));
		check(hWndToolBar);

		ToolTipCtrl = new WToolTip(this);
		ToolTipCtrl->OpenWindow();
		for( INT tooltip = 0 ; ToolTips_BT[tooltip].ID > 0 ; ++tooltip )
		{
			// Figure out the rectangle for the toolbar button.
			INT index = SendMessageX( hWndToolBar, TB_COMMANDTOINDEX, ToolTips_BT[tooltip].ID, 0 );
			RECT rect;
			SendMessageX( hWndToolBar, TB_GETITEMRECT, index, (LPARAM)&rect);

			ToolTipCtrl->AddTool( hWndToolBar, ToolTips_BT[tooltip].ToolTip, tooltip, &rect );
		}

		mrulist = new MRUList( *PersistentName );
		mrulist->ReadINI();
		if( GBrowserMaster->GetCurrent()==BrowserID )
			mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );

		INT Top = 0;
		Anchors.Set( (DWORD)hWndToolBar,			FWindowAnchor( hWnd, hWndToolBar,					ANCHOR_TL, 0, 0,						ANCHOR_RIGHT|ANCHOR_HEIGHT, 0, STANDARD_TOOLBAR_HEIGHT ) );
		Top += STANDARD_TOOLBAR_HEIGHT+4;
		Anchors.Set( (DWORD)ViewportLabel->hWnd,	FWindowAnchor( hWnd, ViewportLabel->hWnd,			ANCHOR_LEFT|ANCHOR_TOP, 4, Top,			ANCHOR_BR, -4, -4-STANDARD_CTRL_HEIGHT-2 ) );
		Anchors.Set( (DWORD)FilterLabel->hWnd,		FWindowAnchor( hWnd, FilterLabel->hWnd,				ANCHOR_LEFT|ANCHOR_BOTTOM, 4, -4-STANDARD_CTRL_HEIGHT+2,	ANCHOR_WIDTH|ANCHOR_HEIGHT, 64, STANDARD_CTRL_HEIGHT ) );
		Anchors.Set( (DWORD)FilterEdit->hWnd,		FWindowAnchor( hWnd, FilterEdit->hWnd,				ANCHOR_LEFT|ANCHOR_BOTTOM, 4+64+2, -4-STANDARD_CTRL_HEIGHT,	ANCHOR_RIGHT|ANCHOR_HEIGHT, -4, STANDARD_CTRL_HEIGHT ) );
		Anchors.Set( (DWORD)PropSheet->hWnd,		FWindowAnchor( ViewportLabel->hWnd, PropSheet->hWnd,ANCHOR_TL, 0,0,		ANCHOR_BR, 0,0 ) );

		Container->SetAnchors( &Anchors );

		PositionChildControls();
		MaterialsPage->RefreshPackages();
		MaterialsPage->RefreshGroups();
		RefreshTextureList();

		SetCaption();

		unguard;
	}
	void OnDestroy()
	{
		guard(WBrowserTexture::OnDestroy);

		GConfig->SetInt( *PersistentName, TEXT("TexViewSize"), GTBOptions->TexViewSize, TEXT("UnrealEd.ini") );

		delete MaterialsPage;
		delete UsedPage;
		delete MRUPage;
		delete PropSheet;
		delete Container;
		delete FilterLabel;
		delete FilterEdit;
		delete ViewportLabel;

		::DestroyWindow( hWndToolBar );
		delete ToolTipCtrl;

		mrulist->WriteINI();
		delete mrulist;

		PropWindows.Empty();

		WBrowser::OnDestroy();
		unguard;
	}
	virtual void UpdateMenu()
	{
		guard(WBrowserTexture::UpdateMenu);

		HMENU menu = GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd );

		CheckMenuItem( menu, IDMN_VAR_200, MF_BYCOMMAND | ((GTBOptions->TexViewSize==TVS_200_PCT) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_VAR_100, MF_BYCOMMAND | ((GTBOptions->TexViewSize==TVS_100_PCT) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_VAR_50, MF_BYCOMMAND | ((GTBOptions->TexViewSize==TVS_50_PCT) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_VAR_25, MF_BYCOMMAND | ((GTBOptions->TexViewSize==TVS_25_PCT) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_FIXED_32, MF_BYCOMMAND | ((GTBOptions->TexViewSize==TVS_32_FIXED) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_FIXED_64, MF_BYCOMMAND | ((GTBOptions->TexViewSize==TVS_64_FIXED) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_FIXED_128, MF_BYCOMMAND | ((GTBOptions->TexViewSize==TVS_128_FIXED) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_FIXED_256, MF_BYCOMMAND | ((GTBOptions->TexViewSize==TVS_256_FIXED) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_FIXED_512, MF_BYCOMMAND | ((GTBOptions->TexViewSize==TVS_512_FIXED) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_MB_DOCK, MF_BYCOMMAND | (IsDocked() ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_FILTER_SHOW_TEXTURE, MF_BYCOMMAND | ((GTBOptions->TypeFilter&MTF_Textures) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_FILTER_SHOW_SHADERS, MF_BYCOMMAND | ((GTBOptions->TypeFilter&MTF_Shaders) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_FILTER_SHOW_MODIFIERS, MF_BYCOMMAND | ((GTBOptions->TypeFilter&MTF_Modifiers) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_FILTER_SHOW_COMBINERS, MF_BYCOMMAND | ((GTBOptions->TypeFilter&MTF_Combiners) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_FILTER_SHOW_FINAL_BLENDS, MF_BYCOMMAND | ((GTBOptions->TypeFilter&MTF_FinalBlends) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_IUFILTER_SHOW_ACTORS, MF_BYCOMMAND | ((GTBOptions->IUFilter&IUF_Actors) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_IUFILTER_SHOW_SPRITES, MF_BYCOMMAND | ((GTBOptions->IUFilter&IUF_Sprites) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_IUFILTER_SHOW_BRUSHES, MF_BYCOMMAND | ((GTBOptions->IUFilter&IUF_Brushes) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_IUFILTER_SHOW_STATICMESHES, MF_BYCOMMAND | ((GTBOptions->IUFilter&IUF_StaticMeshes) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_IUFILTER_SHOW_TERRAIN, MF_BYCOMMAND | ((GTBOptions->IUFilter&IUF_Terrain) ? MF_CHECKED : MF_UNCHECKED) );
		CheckMenuItem( menu, IDMN_IUFILTER_SHOW_EMITTERS, MF_BYCOMMAND | ((GTBOptions->IUFilter&IUF_Emitters) ? MF_CHECKED : MF_UNCHECKED) );

		if( mrulist 
				&& GBrowserMaster->GetCurrent()==BrowserID )
			mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );

		unguard;
	}
	void OnNewMaterial()
	{
		guard(WBrowserTexture::OnNewMaterial);

		FString PackageName, GroupName = TEXT("");
		UObject* Package = GUnrealEd->CurrentMaterial->GetOuter();
		UObject* Group = NULL;
		if( Package->GetOuter() )
		{
			Group = Package;
			GroupName = Group->GetName();
			Package = Group->GetOuter();
		}
		PackageName = Package->GetName();

		MaterialsPage->RefreshPackages();
		MaterialsPage->PackageCombo->SetCurrent( MaterialsPage->PackageCombo->FindStringExact( *PackageName) );
		MaterialsPage->RefreshGroups();
		MaterialsPage->GroupCombo->SetCurrent( MaterialsPage->GroupCombo->FindStringExact( *GroupName) );
		RefreshTextureList();

		// Call up the properties on this new material
		INT idx = PropWindows.AddItem( new WDlgTexProp(NULL, OwnerWindow, GUnrealEd->CurrentMaterial ) );
		WDlgTexProp* dtp = PropWindows( idx );
		dtp->DoModeless(1);
		unguard;
	}
	void OnCommand( INT Command )
	{
		guard(WBrowserTexture::OnCommand);
		switch( Command )
		{
			case IDMN_TB_DETAIL_HACK:
			{
				if( GUnrealEd->CurrentMaterial )
					if( ::MessageBox( hWnd, TEXT("This operation will replace the existing mipmaps for this texture with a set that progressively fades out to gray.  This operation cannot be undone.\n\nAre you sure you want to do this?"), TEXT("Detail Hack"), MB_YESNO) == IDYES)
						GUnrealEd->Exec( *FString::Printf( TEXT("TEXTURE DETAILHACK NAME=%s"), GUnrealEd->CurrentMaterial->GetPathName() ) );
			}
			break;

			case IDMN_TB_COMPRESS_DXT1:
			case IDMN_TB_COMPRESS_DXT3:
			case IDMN_TB_COMPRESS_DXT5:
			{
				UTexture* CurrentTexture = GUnrealEd->CurrentMaterial ? Cast<UTexture>(GUnrealEd->CurrentMaterial) : NULL;
				if( CurrentTexture )
				{
					FString Format;
					if( Command == IDMN_TB_COMPRESS_DXT1 ) Format = TEXT("DXT1");
					else if( Command == IDMN_TB_COMPRESS_DXT3 ) Format = TEXT("DXT3");
					else if( Command == IDMN_TB_COMPRESS_DXT5 ) Format = TEXT("DXT5");

					GUnrealEd->Exec( *FString::Printf( TEXT("TEXTURE COMPRESS NAME=%s FORMAT=%s"),
						CurrentTexture->GetPathName(), *Format ) );
				}
			}
			break;

			case IDMN_TB_NEW:
			{
				FString Package = MaterialsPage->PackageCombo->GetString( MaterialsPage->PackageCombo->GetCurrent() );
				FString Group = MaterialsPage->GroupCombo->GetString( MaterialsPage->GroupCombo->GetCurrent() );
				(new WDlgNewMaterial( NULL, this, FDelegate(this, (TDelegate)OnNewMaterial) ))->DoModeless( Package, Group );
			}
			break;

			case IDMN_TB_PROPERTIES:
			{
				if( GUnrealEd->CurrentMaterial )
				{
					INT idx = PropWindows.AddItem( new WDlgTexProp(NULL, OwnerWindow, GUnrealEd->CurrentMaterial ) );
					WDlgTexProp* dtp = PropWindows( idx );
					dtp->DoModeless(1);
				}
			}
			break;

			case IDMN_TB_LOAD_ENTIRE_PACKAGE:
			{
				FString Package = MaterialsPage->PackageCombo->GetString( MaterialsPage->PackageCombo->GetCurrent() );
				if( Package != TEXT("MyLevel") )
				{
					GUnrealEd->LoadPackage( NULL, *Package, LOAD_NoWarn );
					RefreshAll();
				}
			}
			break;

			case IDMN_TB_DUPLICATE:
			{
				if( !GUnrealEd->CurrentMaterial )
				{
					appMsgf( 0, TEXT("Select a texture first.") );
					break;
				}

				WDlgGeneric dlg( NULL, NULL, OPTIONS_DUPTEXTURE, TEXT("Duplicate Texture") );
				if( dlg.DoModal( TEXT("") ) )
				{
					UOptionsDupTexture* Proxy = Cast<UOptionsDupTexture>(dlg.Proxy);
					if( !Proxy->Package.Len()
						|| !Proxy->Name.Len() )
					{
						appMsgf(0, TEXT("Invalid input.  Cannot create terrain layer."));
					}

					GUnrealEd->Exec( *FString::Printf( TEXT("TEXTURE DUPLICATE PACKAGE=%s GROUP=%s NAME=%s"),
						*Proxy->Package, *Proxy->Group, *Proxy->Name ) );
				}
			}
			break;

			case IDMN_TB_DELETE:
			{
				if( !GUnrealEd->CurrentMaterial )
				{
					appMsgf( 0, TEXT("Select a texture first.") );
					break;
				}

				FString Name = GUnrealEd->CurrentMaterial->GetPathName();
				FStringOutputDevice GetPropResult = FStringOutputDevice();
				TCHAR l_chCmd[256];

				GWarn->BeginSlowTask( TEXT("Deleting..."), 1 );

				UMaterial* Save  = GUnrealEd->CurrentMaterial;
				GUnrealEd->CurrentMaterial = NULL;
				appSprintf( l_chCmd, TEXT("DELETE CLASS=MATERIAL OBJECT=\"%s\""), *Name);
				GUnrealEd->Get( TEXT("Obj"), l_chCmd, GetPropResult);

				if( !GetPropResult.Len() )
				{
					MaterialsPage->RefreshPackages();
					MaterialsPage->RefreshGroups();
					GTBOptions->DeleteMRU( Save );
					RefreshTextureList();
				}
				else
				{
					appMsgf( 0, TEXT("Can't delete texture.\n\n%s"), *GetPropResult );
				}

				GWarn->EndSlowTask();
			}
			break;

			case IDMN_TB_PREV_GRP:
			{
				INT Sel = MaterialsPage->GroupCombo->GetCurrent();
				Sel--;
				if( Sel < 0 ) Sel = MaterialsPage->GroupCombo->GetCount() - 1;
				MaterialsPage->GroupCombo->SetCurrent(Sel);
				RefreshTextureList();
			}
			break;

			case IDMN_TB_NEXT_GRP:
			{
				INT Sel = MaterialsPage->GroupCombo->GetCurrent();
				Sel++;
				if( Sel >= MaterialsPage->GroupCombo->GetCount() ) Sel = 0;
				MaterialsPage->GroupCombo->SetCurrent(Sel);
				RefreshTextureList();
			}
			break;

			case IDMN_TB_RENAME:
			{
				if( !GUnrealEd->CurrentMaterial )
				{
					appMsgf( 0, TEXT("Select a texture first.") );
					break;
				}

				WDlgRename dlg( NULL, this );
				FString Group, Package;
				if( !Cast<UPackage>(GUnrealEd->CurrentMaterial->GetOuter()->GetOuter()) )
				{
					Group = TEXT("");
					Package = GUnrealEd->CurrentMaterial->GetOuter()->GetName();
				}
				else
				{			
					Group = GUnrealEd->CurrentMaterial->GetOuter()->GetName();
					Package = GUnrealEd->CurrentMaterial->GetOuter()->GetOuter()->GetName();
				}					
				if( dlg.DoModal( GUnrealEd->CurrentMaterial->GetName(), Group, Package ) )
					GUnrealEd->Exec(*FString::Printf(TEXT("OBJ RENAME OLDNAME=\"%s\" OLDGROUP=\"%s\" OLDPACKAGE=\"%s\" NEWNAME=\"%s\" NEWGROUP=\"%s\" NEWPACKAGE=\"%s\""), *dlg.OldName, *dlg.OldGroup, *dlg.OldPackage, *dlg.NewName, *dlg.NewGroup, *dlg.NewPackage) );
				MaterialsPage->RefreshPackages();
				MaterialsPage->RefreshGroups();
				RefreshTextureList();
			}
			break;

			case IDMN_TB_REMOVE:
			{
				if( !GUnrealEd->CurrentMaterial )
				{
					appMsgf( 0, TEXT("Select a texture first.") );
					break;
				}

				debugf(TEXT("Removing texture %s from level"),GUnrealEd->CurrentMaterial->GetFullName());
				for( TArray<AActor*>::TIterator ItA(GUnrealEd->Level->Actors); ItA; ++ItA )
				{
					AActor* Actor = *ItA;
					if( Actor )
					{
						// remove from BSP surfaces
						UModel* M = Actor->IsA(ALevelInfo::StaticClass()) ? Actor->GetLevel()->Model : Actor->Brush;
						if( M )
						{
							for( TArray<FBspSurf>::TIterator ItS(M->Surfs); ItS; ++ItS )
								if( ItS->Material==GUnrealEd->CurrentMaterial )
									ItS->Material = Actor->Level->DefaultTexture;
							if( M->Polys )
								for( TArray<FPoly>::TIterator ItP(M->Polys->Element); ItP; ++ItP )
									if( ItP->Material==GUnrealEd->CurrentMaterial )
										ItP->Material = Actor->Level->DefaultTexture;
						}

						// remove from Skins array.
						for( INT i=0;i<Actor->Skins.Num();i++ )
							if( Actor->Skins(i) == GUnrealEd->CurrentMaterial )
								Actor->Skins(i) = Actor->Level->DefaultTexture;

						// remove from Texture
						if( Actor->Texture == GUnrealEd->CurrentMaterial )
							Actor->Texture = Actor->Level->DefaultTexture;
					}

				}
				GUnrealEd->RedrawLevel(NULL);
			}
			break;

			case IDMN_TB_CULL:
			{
				GUnrealEd->Exec( TEXT("TEXTURE CULL"));
				appMsgf(0, TEXT("Texture cull complete.  Check log file for detailed report."));
			}
			break;

			case IDMN_VAR_200:
				GTBOptions->TexViewSize = TVS_200_PCT;
				MaterialsPage->iScroll = UsedPage->iScroll = MRUPage->iScroll = 0;
				RefreshTextureList();
				break;

			case IDMN_VAR_100:
				GTBOptions->TexViewSize = TVS_100_PCT;
				MaterialsPage->iScroll = UsedPage->iScroll = MRUPage->iScroll = 0;
				RefreshTextureList();
				break;

			case IDMN_VAR_50:
				GTBOptions->TexViewSize = TVS_50_PCT;
				MaterialsPage->iScroll = UsedPage->iScroll = MRUPage->iScroll = 0;
				RefreshTextureList();
				break;

			case IDMN_VAR_25:
				GTBOptions->TexViewSize = TVS_25_PCT;
				MaterialsPage->iScroll = UsedPage->iScroll = MRUPage->iScroll = 0;
				RefreshTextureList();
				break;

			case IDMN_FIXED_32:
				GTBOptions->TexViewSize = TVS_32_FIXED;
				MaterialsPage->iScroll = UsedPage->iScroll = MRUPage->iScroll = 0;
				RefreshTextureList();
				break;

			case IDMN_FIXED_64:
				GTBOptions->TexViewSize = TVS_64_FIXED;
				MaterialsPage->iScroll = UsedPage->iScroll = MRUPage->iScroll = 0;
				RefreshTextureList();
				break;

			case IDMN_FIXED_128:
				GTBOptions->TexViewSize = TVS_128_FIXED;
				MaterialsPage->iScroll = UsedPage->iScroll = MRUPage->iScroll = 0;
				RefreshTextureList();
				break;

			case IDMN_FIXED_256:
				GTBOptions->TexViewSize = TVS_256_FIXED;
				MaterialsPage->iScroll = UsedPage->iScroll = MRUPage->iScroll = 0;
				RefreshTextureList();
				break;

			case IDMN_FIXED_512:
				GTBOptions->TexViewSize = TVS_512_FIXED;
				MaterialsPage->iScroll = UsedPage->iScroll = MRUPage->iScroll = 0;
				RefreshTextureList();
				break;

			// FILTERS
			case IDMN_FILTER_SHOW_TEXTURE:
				GTBOptions->TypeFilter ^= MTF_Textures;
				RefreshTextureList();
				break;

			case IDMN_FILTER_SHOW_SHADERS:
				GTBOptions->TypeFilter ^= MTF_Shaders;
				RefreshTextureList();
				break;

			case IDMN_FILTER_SHOW_MODIFIERS:
				GTBOptions->TypeFilter ^= MTF_Modifiers;
				RefreshTextureList();
				break;

			case IDMN_FILTER_SHOW_COMBINERS:
				GTBOptions->TypeFilter ^= MTF_Combiners;
				RefreshTextureList();
				break;

			case IDMN_FILTER_SHOW_FINAL_BLENDS:
				GTBOptions->TypeFilter ^= MTF_FinalBlends;
				RefreshTextureList();
				break;

			case IDMN_FILTER_SHOW_ALL:
				GTBOptions->TypeFilter = MTF_Textures | MTF_Shaders | MTF_Modifiers | MTF_Combiners | MTF_FinalBlends;
				RefreshTextureList();
				break;

			case IDMN_FILTER_SHOW_NONE:
				GTBOptions->TypeFilter &= ~(MTF_Textures | MTF_Shaders | MTF_Modifiers | MTF_Combiners | MTF_FinalBlends);
				RefreshTextureList();
				break;

			// "IN USE" filters
			case IDMN_IUFILTER_SHOW_ACTORS:
				GTBOptions->IUFilter ^= IUF_Actors;
				RefreshTextureList();
				RefreshScrollBar();
				break;

			case IDMN_IUFILTER_SHOW_SPRITES:
				GTBOptions->IUFilter ^= IUF_Sprites;
				RefreshTextureList();
				RefreshScrollBar();
				break;

			case IDMN_IUFILTER_SHOW_BRUSHES:
				GTBOptions->IUFilter ^= IUF_Brushes;
				RefreshTextureList();
				RefreshScrollBar();
				break;

			case IDMN_IUFILTER_SHOW_STATICMESHES:
				GTBOptions->IUFilter ^= IUF_StaticMeshes;
				RefreshTextureList();
				RefreshScrollBar();
				break;

			case IDMN_IUFILTER_SHOW_TERRAIN:
				GTBOptions->IUFilter ^= IUF_Terrain;
				RefreshTextureList();
				RefreshScrollBar();
				break;

			case IDMN_IUFILTER_SHOW_EMITTERS:
				GTBOptions->IUFilter ^= IUF_Emitters;
				RefreshTextureList();
				RefreshScrollBar();
				break;

			case IDMN_IUFILTER_SHOW_ALL:
				GTBOptions->IUFilter = IUF_Actors | IUF_Sprites | IUF_Brushes | IUF_StaticMeshes | IUF_Terrain | IUF_Emitters;
				RefreshTextureList();
				RefreshScrollBar();
				break;

			case IDMN_IUFILTER_SHOW_NONE:
				GTBOptions->IUFilter &= ~(IUF_Actors | IUF_Sprites | IUF_Brushes | IUF_StaticMeshes | IUF_Terrain | IUF_Emitters);
				RefreshTextureList();
				RefreshScrollBar();
				break;

			case IDMN_TB_FileOpen:
			{
				OPENFILENAMEA ofn;
				char File[8192] = "\0";

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "Texture Packages (*.utx)\0*.utx\0All Files\0*.*\0\0";
				ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_UTX]) );
				ofn.lpstrDefExt = "utx";
				ofn.lpstrTitle = "Open Texture Package";
				ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

				if( GetOpenFileNameA(&ofn) )
				{
					INT NumNULLs = FormatFilenames( File );
	
					TArray<FString> StringArray;
					FString S = appFromAnsi( File );
					S.ParseIntoArray( TEXT("|"), &StringArray );

					INT iStart = 0;
					FString Prefix = TEXT("\0");

					if( NumNULLs )
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
						GLastDir[eLASTDIR_UTX] = StringArray(0).Left( StringArray(0).InStr( TEXT("\\"), 1 ) );
					else
						GLastDir[eLASTDIR_UTX] = StringArray(0);

					GWarn->BeginSlowTask( TEXT(""), 1 );

					for( INT x = iStart ; x < StringArray.Num() ; ++x )
					{
						GWarn->StatusUpdatef( x, StringArray.Num(), TEXT("Loading %s"), *(StringArray(x)) );
						GUnrealEd->Exec( *FString::Printf(TEXT("OBJ LOAD FILE=\"%s%s\""), *Prefix, *(StringArray(x))) );

						mrulist->AddItem( *(StringArray(x)) );
						if( GBrowserMaster->GetCurrent()==BrowserID )
							mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );
					}

					GWarn->EndSlowTask();

					GBrowserMaster->RefreshAll();
					MaterialsPage->PackageCombo->SetCurrent( MaterialsPage->PackageCombo->FindStringExact( *SavePkgName ) );
					MaterialsPage->RefreshGroups();
					RefreshTextureList();

					StringArray.Empty();
				}

				GFileManager->SetDefaultDirectory(appBaseDir());
			}
			break;

			case IDMN_TB_FileSave:
			{
				OPENFILENAMEA ofn;
				char File[8192] = "\0";
				FString Package = MaterialsPage->PackageCombo->GetString( MaterialsPage->PackageCombo->GetCurrent() );

				::sprintf( File, "%s.utx", TCHAR_TO_ANSI( *Package ) );

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "Texture Packages (*.utx)\0*.utx\0All Files\0*.*\0\0";
				ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_UTX]) );
				ofn.lpstrDefExt = "utx";
				ofn.lpstrTitle = "Save Texture Package";
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
					GLastDir[eLASTDIR_UTX] = S.Left( S.InStr( TEXT("\\"), 1 ) );
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
					MaterialsPage->RefreshPackages();
					MaterialsPage->PackageCombo->SetCurrent( MaterialsPage->PackageCombo->FindStringExact( *Package ) );
					MaterialsPage->RefreshGroups();
					RefreshTextureList();
				}

				mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );
			}
			break;

			case IDMN_TB_IMPORT_PCX:
			{
				OPENFILENAMEA ofn;
				char File[8192] = "\0";

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.lpstrFilter = "Supported formats (*.bmp,*.pcx,*.tga,*.dds,*.upt)\0*.pcx;*.bmp;*.tga;*.upt;*.dds\0BMP Files (*.bmp)\0*.bmp\0PCX Files (*.pcx)\0*.pcx\0Targa Files (*.tga)\0*.tga\0UPT Files (*.upt)\0*.upt\0DXT Files (*.dds)\0*.dds\0All Files\0*.*\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrDefExt = "bmp";
				ofn.lpstrTitle = "Import Textures";
				ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_PCX]) );
				ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_ALLOWMULTISELECT | OFN_EXPLORER;

				// Display the Open dialog box. 
				//
				if( GetOpenFileNameA(&ofn) )
				{
					INT NumNULLs = FormatFilenames( File );
					FString Package = MaterialsPage->PackageCombo->GetString( MaterialsPage->PackageCombo->GetCurrent() );
					FString Group = MaterialsPage->GroupCombo->GetString( MaterialsPage->GroupCombo->GetCurrent() );
	
					TArray<FString> StringArray;
					FString S = appFromAnsi( File );
					S.ParseIntoArray( TEXT("|"), &StringArray );

					INT iStart = 0;
					FString Prefix = TEXT("\0");

					if( NumNULLs )
					{
						iStart = 1;
						Prefix = *(StringArray(0));
						Prefix += TEXT("\\");
					}

					if( StringArray.Num() == 1 )
						GLastDir[eLASTDIR_PCX] = StringArray(0).Left( StringArray(0).InStr( TEXT("\\"), 1 ) );
					else
						GLastDir[eLASTDIR_PCX] = StringArray(0);

					TArray<FString> FilenamesArray;

					for( INT x = iStart ; x < StringArray.Num() ; ++x )
					{
						FString NewString;

						NewString = FString::Printf( TEXT("%s%s"), *Prefix, *(StringArray(x)) );
						new(FilenamesArray)FString( NewString );

						FString S = NewString;
					}

					WDlgImportTexture l_dlg( NULL, this );
					if( l_dlg.DoModal( Package, Group, &FilenamesArray ) )
					{
					// Flip to the texture/group that was used for importing
					GBrowserMaster->RefreshAll();
						MaterialsPage->PackageCombo->SetCurrent( MaterialsPage->PackageCombo->FindStringExact( *l_dlg.Package) );
						MaterialsPage->RefreshGroups();
						MaterialsPage->GroupCombo->SetCurrent( MaterialsPage->GroupCombo->FindStringExact( *l_dlg.Group) );
					RefreshTextureList();
					}

					StringArray.Empty();
					FilenamesArray.Empty();
				}

				GFileManager->SetDefaultDirectory(appBaseDir());
			}
			break;

			case IDMN_TB_EXPORT_PCX:
			{
				UTexture *Texture = Cast<UTexture>(GUnrealEd->CurrentMaterial);

				if( !Texture )
				{
					appMsgf( 0, TEXT("Select a texture first.") );
					break;
				}

				OPENFILENAMEA ofn;
				char File[8192] = "\0";
				FString Name = Texture->GetName();

				::sprintf( File, "%s", TCHAR_TO_ANSI( *Name ) );

				ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
				ofn.lStructSize = sizeof(OPENFILENAMEA);
				ofn.hwndOwner = hWnd;
				ofn.lpstrFile = File;
				ofn.nMaxFile = sizeof(char) * 8192;
				ofn.nFilterIndex = 1;
				ofn.lpstrTitle = "Export Texture";
				ofn.lpstrInitialDir = appToAnsi( *(GLastDir[eLASTDIR_PCX]) );
				ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;

				//!! ugly
				switch( Texture->Format )
				{
				case TEXF_P8:
					ofn.lpstrFilter = "8-bit Palettized BMP (*.bmp)\0*.bmp\0""8-bit Palettized PCX (*.pcx)\0*.pcx\0\0";
					ofn.lpstrDefExt = "bmp";
					break;
				case TEXF_G16:
					ofn.lpstrFilter = "16-bit Grayscale BMP (*.bmp)\0*.bmp\0";
					ofn.lpstrDefExt = "bmp";
					break;
				case TEXF_RGBA8:
					ofn.lpstrFilter = "32-bit Targa (*.tga)\0*.tga\0""32-bit UPT (*.upt)\0*.upt\0""24-bit BMP (*.bmp)\0*.bmp\0""24-bit PCX (*.pcx)\0*.pcx\0\0";
					ofn.lpstrDefExt = "tga";
					break;
				// sjs ---
				case TEXF_DXT1:
				case TEXF_DXT3:
				case TEXF_DXT5:
					ofn.lpstrFilter = "Direct Draw Surface (*.dds)\0*.dds\0";
					ofn.lpstrDefExt = "dds";
					break;
				// --- sjs
				}

				// Display the Open dialog box. 
				//
				if( GetSaveFileNameA(&ofn) )
				{
					TCHAR l_chCmd[512];

					appSprintf( l_chCmd, TEXT("OBJ EXPORT TYPE=TEXTURE NAME=\"%s\" FILE=\"%s\""),
						*Name, appFromAnsi( File ) );
					GUnrealEd->Exec( l_chCmd );

					FString S = appFromAnsi( File );
					GLastDir[eLASTDIR_PCX] = S.Left( S.InStr( TEXT("\\"), 1 ) );
				}

				GFileManager->SetDefaultDirectory(appBaseDir());
			}
			break;

			case WM_DLGTEXPROP_CLOSING:
			{
				for( INT x = 0 ; x < PropWindows.Num() ; ++x )
					if( PropWindows(x)->hWnd == (HWND)LastlParam )
					{
						delete PropWindows(x);
						PropWindows.Remove(x);
						break;
					}
			}
			break;

			case IDMN_EDIT_TEX_REPLACE:
			{
				GDlgTexReplace->Show(1);
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
		guard(WBrowserTexture::OnSize);
		WBrowser::OnSize(Flags, NewX, NewY);
		PositionChildControls();
		InvalidateRect( hWnd, NULL, FALSE );
		RefreshScrollBar();
		UpdateMenu();
		unguard;
	}
	virtual void RefreshAll()
	{
		guard(WBrowserTexture::RefreshAll);
		MaterialsPage->RefreshPackages();
		MaterialsPage->RefreshGroups();
		RefreshTextureList();
		if( GBrowserMaster->GetCurrent()==BrowserID )
			mrulist->AddToMenu( hWnd, GetMenu( IsDocked() ? OwnerWindow->hWnd : hWnd ) );
		UpdateMenu();
		unguard;
	}
	void RefreshTextureList( void )
	{
		guard(WBrowserTexture::RefreshTextureList);

		if( MaterialsPage ) MaterialsPage->RefreshTextureList();
		if( UsedPage ) UsedPage->RefreshViewport();
		if( MRUPage ) MRUPage->RefreshViewport();

		UpdateMenu();

		unguard;
	}
	void RefreshScrollBar( void )
	{
		guard(WBrowserTexture::RefreshScrollBar);

		if( MaterialsPage )		MaterialsPage->RefreshScrollBar();
		if( UsedPage )			UsedPage->RefreshScrollBar();
		if( MRUPage )			MRUPage->RefreshScrollBar();

		unguard;
	}

	// Moves the child windows around so that they best match the window size.
	//
	void PositionChildControls( void )
	{
		guard(WBrowserTexture::PositionChildControls);
		if( Container ) Container->RefreshControls();
		unguard;
	}
	virtual void SetCaption( FString* Tail = NULL )
	{
		guard(WBrowserTexture::SetCaption);

		FString Extra;
		if( GUnrealEd->CurrentMaterial )
		{
			Extra = *FString::Printf( TEXT("%s %s"), GUnrealEd->CurrentMaterial->GetClass()->GetName(), GUnrealEd->CurrentMaterial->GetPathName() );
            // gam ---
			if( GUnrealEd->CurrentMaterial->MaterialUSize() && GUnrealEd->CurrentMaterial->MaterialVSize() )
				Extra += FString::Printf( TEXT(" (%dx%d)"), GUnrealEd->CurrentMaterial->MaterialUSize(), GUnrealEd->CurrentMaterial->MaterialVSize() );
            // --- gam
		}

		WBrowser::SetCaption( &Extra );
		unguard;
	}

	void OnFilterEditChange()
	{
		guard(WBrowserTexture::OnFilterEditChange);
		GTBOptions->NameFilter = FilterEdit->GetText();
		RefreshTextureList();
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

