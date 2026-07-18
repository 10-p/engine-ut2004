/*=============================================================================
	BrowserMesh : Browser window for meshes
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

extern void Query( ULevel* Level, const TCHAR* Item, FString* pOutput );

// --------------------------------------------------------------
//
// WBrowserMesh
//
// --------------------------------------------------------------

#define ID_MESH_TOOLBAR	29050
TBBUTTON tbMESHButtons[] = {
	{ 0, IDMN_MB_DOCK, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 1, IDPB_PLAY, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 2, IDPB_STOP, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
};
struct {
	TCHAR ToolTip[64];
	INT ID;
} ToolTips_MESH[] = {
	TEXT("Toggle Dock Status"), IDMN_MB_DOCK,
	TEXT("Play Animation"), IDPB_PLAY,
	TEXT("Stop Animation"), IDPB_STOP,
	NULL, 0
};

class WBrowserMesh : public WBrowser
{
	DECLARE_WINDOWCLASS(WBrowserMesh,WBrowser,Window)

	TMap<DWORD,FWindowAnchor> Anchors;

	FContainer *Container;
	WComboBox* MeshCombo;
	WListBox* AnimList;
	WTrackBar* AnimFrameBar;
	WLabel* ViewportLabel;
	HWND hWndToolBar;
	WToolTip *ToolTipCtrl;

	UViewport *Viewport;

	UBOOL bPlaying;
	UBOOL bForceFrame;
	FLOAT SelectFrame;	

	// Structors.
	WBrowserMesh( FName InPersistentName, WWindow* InOwnerWindow, HWND InEditorFrame )
	:	WBrowser( InPersistentName, InOwnerWindow, InEditorFrame )
	{
		Container = NULL;
		ViewportLabel = NULL;
		MeshCombo = NULL;
		AnimList = NULL;
		AnimFrameBar = NULL;
		Viewport = NULL;
		bPlaying = FALSE;
		bForceFrame = FALSE;
		MenuID = IDMENU_BrowserMesh;
		BrowserID = eBROWSER_MESH;
		Description = TEXT("Meshes");
		SelectFrame = 0.0f;
	}

	// WBrowser interface.
	void OpenWindow( UBOOL bChild )
	{
		guard(WBrowserMesh::OpenWindow);
		WBrowser::OpenWindow( bChild );
		SetCaption();
		Show(1);
		unguard;
	}
	void OnCreate()
	{
		guard(WBrowserMesh::OnCreate);
		WBrowser::OnCreate();

		SetMenu( hWnd, LoadMenuIdX(hInstance, IDMENU_BrowserMesh) );
		
		Container = new FContainer();

		ViewportLabel = new WLabel( this, IDSC_VIEWPORT );
		ViewportLabel->OpenWindow( 1, 0 );
		MeshCombo = new WComboBox( this, IDCB_MESH );
		MeshCombo->OpenWindow( 1, 1 );
		AnimList = new WListBox( this, IDLB_ANIMATIONS );
		AnimList->OpenWindow( 1, 0, 0, 0, 0, WS_VSCROLL );

		SendMessageX( AnimList->hWnd, LB_SETCOLUMNWIDTH, 96, 0 );
		MeshCombo->SelectionChangeDelegate = FDelegate(this,(TDelegate)OnMeshSelectionChange);
		AnimList->DoubleClickDelegate = FDelegate(this,(TDelegate)OnAnimDoubleClick);
		AnimList->SelectionChangeDelegate = FDelegate(this,(TDelegate)OnAnimSelectionChange);

		AnimFrameBar = new WTrackBar( this, IDTB_ANIMFRAMES );
		AnimFrameBar->OpenWindow( 1, 0 );
		AnimFrameBar->SetTicFreq( 1000 );
		AnimFrameBar->SetRange( 1, 10000 );
		AnimFrameBar->ThumbPositionDelegate = FDelegate(this, (TDelegate)OnSliderMove ); 
		AnimFrameBar->ThumbTrackDelegate = FDelegate(this, (TDelegate)OnSliderMove );    

		// Create the mesh viewport
		//
		Viewport = GUnrealEd->Client->NewViewport( TEXT("MeshViewer") );
		check(Viewport);
		GUnrealEd->Level->SpawnViewActor( Viewport );
		Viewport->Input->Init( Viewport );
		check(Viewport->Actor);
		Viewport->Actor->ShowFlags = SHOW_StandardView | SHOW_ChildWindow | SHOW_Frame;
		Viewport->Actor->RendMap   = REN_MeshView;
		Viewport->Actor->bHidden = false;
		Viewport->Group = NAME_None;
		Viewport->Actor->Misc1 = 0;
		Viewport->Actor->Misc2 = 0;
		Viewport->MiscRes = NULL;

		Viewport->OpenWindow( (DWORD)ViewportLabel->hWnd, 0, 256, 256, 500, 500 );

		hWndToolBar = CreateToolbarEx( 
			hWnd, WS_CHILD | WS_BORDER | WS_VISIBLE | CCS_ADJUSTABLE,
			IDB_BrowserMesh_TOOLBAR,
			3,
			hInstance,
			IDB_BrowserMesh_TOOLBAR,
			(LPCTBBUTTON)&tbMESHButtons,
			4,
			16,16,
			16,16,
			sizeof(TBBUTTON));
		check(hWndToolBar);

		ToolTipCtrl = new WToolTip(this);
		ToolTipCtrl->OpenWindow();
		for( INT tooltip = 0 ; ToolTips_MESH[tooltip].ID > 0 ; ++tooltip )
		{
			// Figure out the rectangle for the toolbar button.
			INT index = SendMessageX( hWndToolBar, TB_COMMANDTOINDEX, ToolTips_MESH[tooltip].ID, 0 );
			RECT rect;
			SendMessageX( hWndToolBar, TB_GETITEMRECT, index, (LPARAM)&rect);

			ToolTipCtrl->AddTool( hWndToolBar, ToolTips_MESH[tooltip].ToolTip, tooltip, &rect );
		}

		RefreshAll();
		SetCaption();
		OnMeshSelectionChange();

		INT Top = 0;
		Anchors.Set( (DWORD)hWndToolBar, FWindowAnchor( hWnd, hWndToolBar,					ANCHOR_TL, 0, 0,							ANCHOR_RIGHT|ANCHOR_HEIGHT, 0, STANDARD_TOOLBAR_HEIGHT ) );
		Top += STANDARD_TOOLBAR_HEIGHT+4;
		Anchors.Set( (DWORD)MeshCombo->hWnd, FWindowAnchor( hWnd, MeshCombo->hWnd,			ANCHOR_TL, 4, Top,							ANCHOR_RIGHT|ANCHOR_HEIGHT, -4, STANDARD_CTRL_HEIGHT ) );
		Top += STANDARD_CTRL_HEIGHT+2;
		Anchors.Set( (DWORD)AnimList->hWnd, FWindowAnchor( hWnd, AnimList->hWnd,			ANCHOR_TL, 4, Top,							ANCHOR_WIDTH|ANCHOR_BOTTOM, 192, -4 ) );
		Anchors.Set( (DWORD)ViewportLabel->hWnd, FWindowAnchor( hWnd, ViewportLabel->hWnd,	ANCHOR_TL, 4+192+2, Top,					ANCHOR_BR, -4, -4-32-2 ) );
		Anchors.Set( (DWORD)AnimFrameBar->hWnd, FWindowAnchor( hWnd, AnimFrameBar->hWnd,	ANCHOR_LEFT|ANCHOR_BOTTOM, 4+192+2, -32,	ANCHOR_RIGHT|ANCHOR_HEIGHT, -4, 32 ) );
		Anchors.Set( (DWORD)Viewport->GetWindow(), FWindowAnchor( ViewportLabel->hWnd, (HWND)Viewport->GetWindow(),	ANCHOR_TL,0,0,									ANCHOR_BR,0,0 ) );

		Container->SetAnchors( &Anchors );

		PositionChildControls();

		unguard;
	}
	void SetCaption( void )
	{
		guard(WBrowserMesh::SetCaption);

		FString Caption = TEXT("Mesh Browser");

		if( GetCurrentMeshName().Len() )
			Caption += FString::Printf( TEXT(" - %s"),
				GetCurrentMeshName() );

		SetText( *Caption );
		unguard;
	}
	virtual void RefreshAll()
	{
		guard(WBrowserMesh::RefreshAll);
		RefreshMeshList();
		RefreshAnimList();
		RefreshViewport();
		unguard;
	}
	void RefreshMeshList()
	{
		guard(WBrowserMesh::RefreshMeshList);

		FStringOutputDevice GetPropResult = FStringOutputDevice();
		GUnrealEd->Get( TEXT("OBJ"), TEXT("Query Type=VertMesh"), GetPropResult );

		MeshCombo->Empty();

		TArray<FString> StringArray;
		GetPropResult.ParseIntoArray( TEXT(" "), &StringArray );

		for( INT x = 0 ; x < StringArray.Num() ; ++x )
			MeshCombo->AddString( *(StringArray(x)) );

		MeshCombo->SetCurrent(0);

		StringArray.Empty();

		unguard;
	}
	FString GetCurrentMeshName()
	{
		guard(WBrowserMesh::GetCurrentMeshName);
		return MeshCombo->GetString( MeshCombo->GetCurrent() );
		unguard;
	}
	void RefreshAnimList()
	{
		guard(WBrowserMesh::RefreshAnimList);

		FString MeshName = GetCurrentMeshName();

		AnimList->Empty();

		FStringOutputDevice GetPropResult = FStringOutputDevice();
		GUnrealEd->Get( TEXT("MESH"), *FString::Printf(TEXT("NUMANIMSEQS NAME=%s"), *MeshName), GetPropResult );
		INT NumAnims = appAtoi( *GetPropResult );

		for( INT anim = 0 ; anim < NumAnims ; ++anim )
		{
			FStringOutputDevice GetPropResult = FStringOutputDevice();
			GUnrealEd->Get( TEXT("MESH"), *FString::Printf(TEXT("ANIMSEQ NAME=%s NUM=%d"), *MeshName, anim), GetPropResult );

			INT NumFrames = appAtoi( *(GetPropResult.Right(3)) );
			FString Name = GetPropResult.Left( GetPropResult.InStr(TEXT(" ")));

			AnimList->AddString( *FString::Printf(TEXT("%s [ %d ]"), *Name, NumFrames ));
		}

		AnimList->SetCurrent(0, 1);

		unguard;
	}
	void RefreshViewport()
	{
		guard(WBrowserMesh::RefreshViewport);

		FString MeshName = MeshCombo->GetString(MeshCombo->GetCurrent());

		FStringOutputDevice GetPropResult = FStringOutputDevice();
		GUnrealEd->Get( TEXT("MESH"), *FString::Printf(TEXT("ANIMSEQ NAME=\"%s\" NUM=%d"), *MeshName, AnimList->GetCurrent()), GetPropResult );

		GUnrealEd->Exec( *FString::Printf(TEXT("CAMERA UPDATE NAME=MeshViewer MESH=\"%s\" FLAGS=%d REN=%d MISC1=%d MISC2=%d"),
			*MeshName,
			bForceFrame ? SHOW_StandardView | SHOW_ChildWindow | SHOW_Frame :	
			bPlaying	? SHOW_StandardView | SHOW_ChildWindow | SHOW_Frame | SHOW_RealTime : SHOW_StandardView | SHOW_ChildWindow | SHOW_Frame,
			REN_MeshView,
			appAtoi(*(GetPropResult.Right(7).Left(3))),
			bForceFrame ? (INT)(SelectFrame) :0
			));

		unguard;
	}
	void OnDestroy()
	{
		guard(WBrowserMesh::OnDestroy);

		delete Viewport;

		delete Container;
		delete MeshCombo;
		delete AnimList;
		delete AnimFrameBar;
		delete ViewportLabel;

		::DestroyWindow( hWndToolBar );
		delete ToolTipCtrl;

		WBrowser::OnDestroy();
		unguard;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WBrowserMesh::OnSize);
		WBrowser::OnSize(Flags, NewX, NewY);
		PositionChildControls();
		InvalidateRect( hWnd, NULL, FALSE );
		unguard;
	}
	void PositionChildControls()
	{
		guard(WBrowserMesh::PositionChildControls);
		if( Container ) Container->RefreshControls();
		if( Viewport ) Viewport->Repaint( 1 );
		unguard;
	}
	void OnPlay()
	{
		guard(WBrowserMesh::OnPlay);
		bPlaying = 1;
		RefreshViewport();
		unguard;
	}
	void OnStop()
	{
		guard(WBrowserMesh::OnStop);
		bPlaying = 0;
		RefreshViewport();
		unguard;
	}
	void OnCommand( INT Command )
	{
		guard(WBrowserMesh::OnCommand);
		switch( Command ) {

			case IDPB_PLAY:
				OnPlay();
				break;

			case IDPB_STOP:
				OnStop();
				break;

			case IDMN_REFRESH:
				RefreshAll();
				break;

			default:
				WBrowser::OnCommand(Command);
				break;
		}
		unguard;
	}

	// Notification delegates for child controls.
	//
	void OnMeshSelectionChange()
	{
		guard(WBrowserMesh::OnMeshSelectionChange);

		GUnrealEd->CurrentMesh = Cast<UMesh>(UObject::StaticFindObject(UVertMesh::StaticClass(), ANY_PACKAGE,*(MeshCombo->GetString(MeshCombo->GetCurrent())) ));

		RefreshAnimList();
		RefreshViewport();
		SetCaption();
		unguard;
	}
	void OnAnimDoubleClick()
	{
		guard(WBrowserMesh::OnAnimDoubleClick);
		OnPlay();
		unguard;
	}
	void OnAnimSelectionChange()
	{
		guard(WBrowserMesh::OnAnimSelectionChange);
		RefreshViewport();

		AnimFrameBar->SetRange( 1, 10000 );
		//AnimFrameBar->SetTicFreq( 1000 );//#SKEL
		AnimFrameBar->SetPos( 0 );

		unguard;
	}
	void OnSliderMove()
	{		
		guard(WBrowserMesh::OnSliderMove);
		SelectFrame = AnimFrameBar->GetPos();
		//debugf(TEXT("Slider action: Pos: %f"),SelectFrame );	
		bPlaying = 0;
		bForceFrame = 1;
		RefreshViewport();
		bForceFrame = 0;
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

