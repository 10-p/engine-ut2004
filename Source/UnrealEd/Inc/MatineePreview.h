/*=============================================================================
	MatineePreview : Window for previewing a Matinee path
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

INT GLastMatViewportNum = -1;

#define IDSB_TIME				19500
#define IDPB_RESET				19501
#define IDPB_SHOW_PATHS			19502
#define IDPB_VCR_FIRST			19503
#define IDPB_VCR_STOP			19505
#define IDPB_VCR_FORWARD		19506
#define IDPB_CINEMATICS			19508
#define IDPB_SHOW_ROTATIONS		19509
#define IDPB_MAT_REFRESH		19510

extern HWND GMatineeSheetHwnd;

struct {
	TCHAR ToolTip[64];
	INT ID;
} ToolTips_PreviewWindow[] = {
	TEXT("Jump to Start of Path"), IDPB_VCR_FIRST,
	TEXT("Stop"), IDPB_VCR_STOP,
	TEXT("Play Forwards"), IDPB_VCR_FORWARD,
	TEXT("Show Rotation Indicators"), IDPB_SHOW_ROTATIONS,
	TEXT("Show the Camera Path"), IDPB_SHOW_PATHS,
	TEXT("Toggle Cinematic View"), IDPB_CINEMATICS,
	TEXT("Refresh the Preview Data"), IDPB_MAT_REFRESH,
	TEXT("Reset the Viewport Camera"), IDPB_RESET,
	NULL, 0
};

class WMatineePreview : public WWindow
{
	DECLARE_WINDOWCLASS(WMatineePreview,WWindow,UnrealEd)

	FString ViewportName;
	UViewport *Viewport;
	ASceneManager* SM;		// The path being previewed

	WButton *FirstButton, *StopButton, *ForwardButton, *RefreshButton, *ResetButton;
	WCheckBox *ShowRotationsCheck, *ShowPathsCheck, *CinematicsCheck;
	WTimeScrollBar *TimeScrollBar;

	HBITMAP FirstBitmap, StopBitmap, ForwardBitmap, RefreshBitmap, ResetBitmap, ShowRotationsBitmap, ShowPathsBitmap, CinematicsBitmap;
	WToolTip* ToolTipCtrl;

	// Constructor.
	WMatineePreview( FName InPersistentName, WWindow* InOwnerWindow, ASceneManager* InSM )
	:	WWindow		( InPersistentName, InOwnerWindow )
	{
		ViewportName = FString::Printf( TEXT("MatineePreview%d"), ++GLastMatViewportNum );
		SM = InSM;
		Viewport = NULL;
		FirstButton = StopButton = ForwardButton = RefreshButton = ResetButton = ShowRotationsCheck = ShowPathsCheck = CinematicsCheck = NULL;
		TimeScrollBar = NULL;

		FirstBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_MAT_VCR_FIRST), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	check(FirstBitmap);
		StopBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_MAT_VCR_STOP), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	check(StopBitmap);
		ForwardBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_MAT_VCR_FORWARD), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	check(ForwardBitmap);
		RefreshBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_MAT_REFRESH), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	check(RefreshBitmap);
		ResetBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_MAT_RESET), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	check(ResetBitmap);
		ShowRotationsBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_MAT_SHOW_ROT), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	check(ShowRotationsBitmap);
		ShowPathsBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_MAT_SHOW_PATH), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	check(ShowPathsBitmap);
		CinematicsBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_MAT_TOGGLE_CINEMATIC_VIEW), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	check(CinematicsBitmap);
		
		UDNHelpTopic = 1000;
	}

	// WWindow interface.
	void OpenWindow()
	{
		guard(WMatineePreview::OpenWindow);
		MdiChild = 0;
		PerformCreateWindowEx
		(
			0,
			TEXT("MatineePreview"),
			WS_THICKFRAME | WS_CAPTION | WS_SYSMENU | WS_VISIBLE | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
			CW_USEDEFAULT, CW_USEDEFAULT,
			CW_USEDEFAULT, CW_USEDEFAULT,
			OwnerWindow ? OwnerWindow->hWnd : NULL,
			NULL,
			hInstance
		);

		TimeScrollBar = new WTimeScrollBar( this, IDSB_TIME );
		TimeScrollBar->OpenWindow( 0, 0, 1 );
		FirstButton = new WButton( this, IDPB_VCR_FIRST, FDelegate(this,(TDelegate)OnVCRFirst) );
		FirstButton->OpenWindow( 1, 0, 0, 24, 24, NULL, 0, BS_OWNERDRAW );
		StopButton = new WButton( this, IDPB_VCR_STOP, FDelegate(this,(TDelegate)OnVCRStop) );
		StopButton->OpenWindow( 1, 0, 0, 24, 24, NULL, 0, BS_OWNERDRAW );
		ForwardButton = new WButton( this, IDPB_VCR_FORWARD, FDelegate(this,(TDelegate)OnVCRForward) );
		ForwardButton->OpenWindow( 1, 0, 0, 24, 24, NULL, 0, BS_OWNERDRAW );
		RefreshButton = new WButton( this, IDPB_MAT_REFRESH, FDelegate(this,(TDelegate)OnRefreshClick) );
		RefreshButton->OpenWindow( 1, 0, 0, 24, 24, NULL, 0, BS_OWNERDRAW );
		ResetButton = new WButton( this, IDPB_RESET, FDelegate(this,(TDelegate)OnResetClick) );
		ResetButton->OpenWindow( 1, 0, 0, 24, 24, NULL, 0, BS_OWNERDRAW );
		ShowRotationsCheck = new WCheckBox( this, IDPB_SHOW_ROTATIONS );
		ShowRotationsCheck->OpenWindow( 1, 0, 0, 24, 24, NULL, 1, 1, BS_PUSHLIKE | BS_OWNERDRAW );
		ShowRotationsCheck->ClickDelegate = FDelegate(this,(TDelegate)OnShowRotations);
		ShowPathsCheck = new WCheckBox( this, IDPB_SHOW_PATHS );
		ShowPathsCheck->OpenWindow( 1, 0, 0, 24, 24, NULL, 1, 1, BS_PUSHLIKE | BS_OWNERDRAW );
		ShowPathsCheck->ClickDelegate = FDelegate(this,(TDelegate)OnShowPaths);
		CinematicsCheck = new WCheckBox( this, IDPB_CINEMATICS );
		CinematicsCheck->OpenWindow( 1, 0, 0, 24, 24, NULL, 1, 1, BS_PUSHLIKE | BS_OWNERDRAW );
		CinematicsCheck->ClickDelegate = FDelegate(this,(TDelegate)OnCinematics);

		TimeScrollBar->PosChangedDelegate = FDelegate(this, (TDelegate)OnTimePosChanged);

		FirstButton->SetBitmap( FirstBitmap );
		StopButton->SetBitmap( StopBitmap );
		ForwardButton->SetBitmap( ForwardBitmap );
		RefreshButton->SetBitmap( RefreshBitmap );
		ResetButton->SetBitmap( ResetBitmap );
		ShowRotationsCheck->SetBitmap( ShowRotationsBitmap );
		ShowPathsCheck->SetBitmap( ShowPathsBitmap );
		CinematicsCheck->SetBitmap( CinematicsBitmap );

		Viewport = GUnrealEd->Client->NewViewport( *ViewportName );
		GUnrealEd->Level->SpawnViewActor( Viewport );
		Viewport->Input->Init( Viewport );
		check(Viewport->Actor);
		Viewport->Actor->ShowFlags = SHOW_StandardView | SHOW_Volumes | SHOW_MovingBrushes | SHOW_ChildWindow | SHOW_Actors | SHOW_StaticMeshes | SHOW_Terrain | SHOW_DistanceFog | SHOW_MatRotations | SHOW_MatPaths | SHOW_Coronas | SHOW_Particles | SHOW_BSP | SHOW_FluidSurfaces | SHOW_Projectors,
		Viewport->Actor->RendMap   = REN_MatineePreview;
		Viewport->Group = NAME_None;
		Viewport->MiscRes = SM;
		Viewport->OpenWindow( (DWORD)hWnd, 0, 10, 10, 0, 0 );
		Viewport->Actor->bHiddenEd = 1;
		Viewport->Actor->bHiddenEdGroup = 1;
		Viewport->Actor->bHidden   = 1;

		SM->SetSceneStartTime();
		SM->Viewer = Viewport->Actor;

		SetText( *FString::Printf( TEXT("Matinee Preview - %s"), *(SM->Tag) ) );

		ToolTipCtrl = new WToolTip(this);
		ToolTipCtrl->OpenWindow();
		for( INT tooltip = 0 ; ToolTips_PreviewWindow[tooltip].ID > 0 ; ++tooltip )
			ToolTipCtrl->AddTool( GetDlgItem( hWnd, ToolTips_PreviewWindow[tooltip].ID ), ToolTips_PreviewWindow[tooltip].ToolTip, tooltip );

		PositionChildControls();

		UpdateButtons();

		SM->InitializePreviewActors();

		unguard;
	}
	void OnDestroy()
	{
		guard(WMatineePreview::OnDestroy);

		if( SM )
		{
			SM->CleanupPreviewActors(); 		
		}
	
		PostMessageX( GMatineeSheetHwnd, WM_COMMAND, WM_MAT_PREVIEW_CLOSING, (LPARAM)hWnd );

		WWindow::OnDestroy();

		delete TimeScrollBar;
		delete FirstButton;
		delete StopButton;
		delete ForwardButton;
		delete RefreshButton;
		delete ResetButton;
		delete ShowRotationsCheck;
		delete ShowPathsCheck;
		delete CinematicsCheck;

		DeleteObject( FirstBitmap );
		DeleteObject( StopBitmap );
		DeleteObject( ForwardBitmap );
		DeleteObject( RefreshBitmap );
		DeleteObject( ResetBitmap );
		DeleteObject( ShowRotationsBitmap );
		DeleteObject( ShowPathsBitmap );
		DeleteObject( CinematicsBitmap );

		delete Viewport;
		delete ToolTipCtrl;

		unguard;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WMatineePreview::OnSize);
		WWindow::OnSize(Flags, NewX, NewY);
		PositionChildControls();
		unguard;
	}
	void PositionChildControls()
	{
		guard(WMatineePreview::PositionChildControls);

		if( !Viewport ) return;

		FRect CR = GetClientRect();
		
		FDeferWindowPos dwp;

		INT Top = 0, Left = 0;
		dwp.MoveWindow( FirstButton->hWnd, Left, Top, 24, 24, 1 );	Left += 24;
		dwp.MoveWindow( StopButton->hWnd, Left, Top, 24, 24, 1 );	Left += 24;
		dwp.MoveWindow( ForwardButton->hWnd, Left, Top, 24, 24, 1 );	Left += 24;

		Left += 8;
		dwp.MoveWindow( RefreshButton->hWnd, Left, Top, 24, 24, 1 );	Left += 24;
		dwp.MoveWindow( ResetButton->hWnd, Left, Top, 24, 24, 1 );	Left += 24;

		Left += 8;
		dwp.MoveWindow( ShowRotationsCheck->hWnd, Left, Top, 24, 24, 1 );	Left += 24;
		dwp.MoveWindow( ShowPathsCheck->hWnd, Left, Top, 24, 24, 1 );	Left += 24;

		Left += 8;
		dwp.MoveWindow( CinematicsCheck->hWnd, Left, Top, 24, 24, 1 );	Left += 24;

		Left = 0;
		Top += 24;
		dwp.MoveWindow( TimeScrollBar->hWnd, Left, Top, CR.Width(), 17, 1 );

		Top += 17;

		dwp.MoveWindow( (HWND)Viewport->GetWindow(), Left, Top, CR.Width(), CR.Height() - Top, 1 );

		unguard;
	}
	void OnTimePosChanged()
	{
		guard(WMatineePreview::OnTimePosChanged);
		
		// If the user moves the scrubber, stop running the scene

		SM->bIsRunning = 0;
		SM->bIsSceneStarted = 0;

		// Update the window

		Update( TimeScrollBar->GetPct() );
		Viewport->Actor->Misc1 = TimeScrollBar->GetPct()*10000;
		InvalidateRect( TimeScrollBar->hWnd, NULL, 0 );

		unguard;
	}
	void OnPaint()
	{
		guard(WMatineePreview::OnPaint);
		PAINTSTRUCT PS;
		HDC hDC = BeginPaint( *this, &PS );

		FRect Rect = GetClientRect();
		FillRect( hDC, Rect, hBrushGrey );
		MyDrawEdge( hDC, Rect, 1 );

		EndPaint( *this, &PS );
		unguard;
	}
	void UpdateButtons()
	{
		ShowRotationsCheck->SetCheck( Viewport->Actor->ShowFlags & SHOW_MatRotations ? BST_CHECKED : BST_UNCHECKED );
		ShowPathsCheck->SetCheck( Viewport->Actor->ShowFlags & SHOW_MatPaths ? BST_CHECKED : BST_UNCHECKED );
		CinematicsCheck->SetCheck( Viewport->bRenderCinematics ? BST_CHECKED : BST_UNCHECKED );
	}
	// Updates the previews camera position to match the time slider percentage.
	void Update( FLOAT InPct )
	{
		guard(WMatineePreview::Updatee);

		SM->UpdateViewerFromPct( InPct );

		Viewport->Repaint(1);

		unguard;
	}
	void OnShowRotations()
	{
		guard(WMatineePreview::OnShowRotations);
		if( ShowRotationsCheck->IsChecked() )
			Viewport->Actor->ShowFlags |= SHOW_MatRotations;
		else
			Viewport->Actor->ShowFlags &= ~SHOW_MatRotations;
		UpdateButtons();
		Viewport->Repaint(1);
		unguard;
	}
	void OnShowPaths()
	{
		guard(WMatineePreview::OnShowPaths);
		if( ShowPathsCheck->IsChecked() )
			Viewport->Actor->ShowFlags |= SHOW_MatPaths;
		else
			Viewport->Actor->ShowFlags &= ~SHOW_MatPaths;
		UpdateButtons();
		Viewport->Repaint(1);
		unguard;
	}
	void OnCinematics()
	{
		guard(WMatineePreview::OnCinematics);
		Viewport->bRenderCinematics = CinematicsCheck->IsChecked();
		UpdateButtons();
		Viewport->Repaint(1);
		unguard;
	}
	void OnVCRFirst()
	{
		guard(WMatineePreview::OnVCRFirst);

		SM->SetCurrentTime( 0 );

		unguard;
	}
	void OnVCRStop()
	{
		guard(WMatineePreview::OnVCRStop);

		SM->bIsRunning = 0;
		SM->bIsSceneStarted = 0;

		Viewport->Actor->ShowFlags &= ~SHOW_PlayerCtrl;

		unguard;
	}
	void OnVCRForward()
	{
		guard(WMatineePreview::OnVCRForward);

		SM->PreparePath();
		SM->SceneStarted();
		Viewport->Current = 1;
		Viewport->Actor->ShowFlags |= SHOW_PlayerCtrl;
		SM->SetCurrentTime( SM->GetTotalSceneTime() * TimeScrollBar->GetPct() );

		unguard;
	}
	void OnRefreshClick()
	{
		guard(WMatineePreview::OnRefreshClick);

		SM->PreparePath();
		Viewport->Repaint(1);

		unguard;
	}
	// Reset the viewport camera to the default values (removes remnant flashfog, fov changes, etc)
	void OnResetClick()
	{
		guard(WMatineePreview::OnResetClick);

		if( SM->Affect == AFFECT_ViewportCamera )
		{
			APlayerController *PCSrc = Cast<APlayerController>(APlayerController::StaticClass()->GetDefaultActor()),
				*PCDst = Cast<APlayerController>( SM->Viewer );

			if( PCSrc && PCDst )
			{
				PCDst->FlashFog = PCSrc->FlashFog;
				PCDst->FlashScale = PCSrc->FlashScale;
				PCDst->FovAngle = PCSrc->FovAngle;
			}
		}

		SM->Level->TimeDilation = Cast<ALevelInfo>(ALevelInfo::StaticClass()->GetDefaultActor())->TimeDilation;
		SM->SceneSpeed = 1.f;
		
		Viewport->Repaint(1);

		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

