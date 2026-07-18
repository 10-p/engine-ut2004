/*=============================================================================
	TerrainEditSheet : Property sheet for terrain editing
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

#include <stdio.h>

#define IDMN_LAYER_BASE			19500
#define IDMN_DECOLAYER_BASE		19600

// --------------------------------------------------------------
//
// WPageToolsTerrains
//
// --------------------------------------------------------------

struct {
	TCHAR ToolTip[128];
	INT ID;
} ToolTips_PageToolsTerrains[] = {
	NULL, 0
};

class WPageToolsTerrains : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WPageToolsTerrains,WPropertyPage,Window)

	WToolTip* ToolTipCtrl;
	WButton *NewTerrainButton;
	WListBox *TerrainsList;
	UViewport *Viewport;
	HBITMAP NewBitmap;

	// Structors.
	WPageToolsTerrains ( WWindow* InOwnerWindow )
	:	WPropertyPage( InOwnerWindow )
	{
		NewTerrainButton = NULL;
		TerrainsList = NULL;
		Viewport = NULL;
		NewBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_NEW), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS );	check(NewBitmap);
	}

	virtual void OpenWindow( INT InDlgId, HMODULE InHMOD )
	{
		guard(WPageToolsTerrains::OpenWindow);
		WPropertyPage::OpenWindow( InDlgId, InHMOD );

		// Create child controls and let the base class determine their proper positions.
		TerrainsList = new WListBox( this, IDLB_TERRAINS );
		TerrainsList->OpenWindow( 1, 0, 0, 0, 1 );
		NewTerrainButton = new WButton( this, IDPB_TERRAIN_NEW, FDelegate(this,(TDelegate)OnNewTerrain) );
		NewTerrainButton->OpenWindow( 1, 0, 0, 10, 10, TEXT("N") );

		PlaceControl( TerrainsList );
		PlaceControl( NewTerrainButton );

		Finalize();

		TerrainsList->SelectionChangeDelegate = FDelegate(this,(TDelegate)OnTerrainsSelectionChange);
		TerrainsList->DoubleClickDelegate = FDelegate(this, (TDelegate)OnTerrainsDblClick);

		NewTerrainButton->SetBitmap( NewBitmap );

		// Viewport
		FName Name = TEXT("TerrainHeightmap");
		Viewport = GEditor->Client->NewViewport( Name );
		GEditor->Level->SpawnViewActor( Viewport );
		Viewport->Actor->ShowFlags = SHOW_StandardView | SHOW_ChildWindow;
		Viewport->Actor->RendMap   = REN_TerrainHeightmap;
		Viewport->Actor->Misc1 = 0;
		Viewport->Actor->Misc2 = 0;
		Viewport->Group = NAME_None;
		Viewport->MiscRes = NULL;
		Viewport->Input->Init( Viewport );

		RECT rc;
		::GetWindowRect( GetDlgItem( hWnd, IDSC_VIEWPORT_LAYERS ), &rc );
		::ScreenToClient( hWnd, (POINT*)&rc.left );
		::ScreenToClient( hWnd, (POINT*)&rc.right );
		Viewport->OpenWindow( (DWORD)hWnd, 0, (rc.right - rc.left), (rc.bottom - rc.top), rc.left, rc.top );

		ToolTipCtrl = new WToolTip(this);
		ToolTipCtrl->OpenWindow();
		for( INT tooltip = 0 ; ToolTips_PageToolsTerrains[tooltip].ID > 0 ; ++tooltip )
			ToolTipCtrl->AddTool( GetDlgItem( hWnd, ToolTips_PageToolsTerrains[tooltip].ID ), ToolTips_PageToolsTerrains[tooltip].ToolTip, tooltip );

		unguard;
	}
	void OnDestroy()
	{
		guard(WPageToolsTerrains::OnDestroy);
		WPropertyPage::OnDestroy();

		::DestroyWindow( TerrainsList->hWnd );
		::DestroyWindow( NewTerrainButton->hWnd );

		delete TerrainsList;
		delete NewTerrainButton;

		delete ToolTipCtrl;
		delete Viewport;

		DeleteObject( NewBitmap );

		unguard;
	}
	virtual void Refresh()
	{
		guard(WPageToolsTerrains::Refresh);
		WPropertyPage::Refresh();

		RefreshTerrains();
		RefreshViewport();

		unguard;
	}
	void RefreshTerrains()
	{
		guard(WPageToolsTerrains::RefreshTerrains);

		INT Pos = TerrainsList->GetCurrent();

		TerrainsList->Empty();

		for( INT i = 0 ; i < GEditor->Level->Actors.Num() ; ++i )
		{
			ATerrainInfo* TI = Cast<ATerrainInfo>(GEditor->Level->Actors(i));
			if( TI )
				TerrainsList->AddString( TI->GetName() );
		}

		if( TerrainsList->SetCurrent( Pos ) == LB_ERR )
			TerrainsList->SetCurrent(0);
		OnTerrainsSelectionChange();

		unguard;
	}
	void RefreshViewport()
	{
		guard(WPageToolsTerrains::RefreshViewport);
		Viewport->Repaint( 1 );
		unguard;
	}
	void OnTerrainsSelectionChange()
	{
		guard(WPageToolsTerrains::OnTerrainsSelectionChange);

		INT Pos = TerrainsList->GetCurrent();
		check(Pos != LB_ERR );

		// Figure out which terraininfo the selection represents, then tell the terrain
		// tools about it.
		INT Count = -1;
		ATerrainInfo* NewCurrentTI = NULL;
		for( INT i = 0 ; i < GEditor->Level->Actors.Num() ; ++i )
		{
			AActor* Actor = GEditor->Level->Actors(i);

			ATerrainInfo* TI = Cast<ATerrainInfo>(GEditor->Level->Actors(i));
			if( TI )
			{
				Count++;
				if( Count == Pos )
				{
					NewCurrentTI = TI;
					break;
				}
			}
		}

		if( Count != -1 )
			check(NewCurrentTI);	// The listbox is out of sync with the world, which should never happen
		GTerrainTools.SetCurrentTerrainInfo( NewCurrentTI );

		RefreshViewport();

		unguard;
	}
	void OnTerrainsDblClick()
	{
		GEditor->Exec(TEXT("SELECT NONE"));
		if( GTerrainTools.GetCurrentTerrainInfo() )
		{
			GEditor->SelectActor( GEditor->Level, GTerrainTools.GetCurrentTerrainInfo(), 1, 0 );
			GUnrealEd->ShowActorProperties();
		}
	}
	void OnNewTerrain()
	{
		guard(WPageToolsTerrains::OnNewTerrain);

		WDlgGeneric dlg( NULL, NULL, OPTIONS_NEWTERRAIN, TEXT("New Terrain") );
		if( dlg.DoModal( TEXT("") ) )
		{
			UOptionsNewTerrain* Proxy = Cast<UOptionsNewTerrain>(dlg.Proxy);
			if( !Proxy->Package.Len()
				|| !Proxy->Name.Len()
				|| !Proxy->XSize
				|| !Proxy->YSize )
			{
				appMsgf(0, TEXT("Invalid input.  Cannot create terrain."));
			}
			else
			{
				// Spawn a new TerrainInfo actor at the camera position

				UViewport* Viewport = GEditor->GetCurrentViewport();
				if( !Viewport )
				{
					appMsgf( 0, TEXT("No current viewport.") );
					return;
				}

				ATerrainInfo* TI = Cast<ATerrainInfo>( GEditor->AddActor( GEditor->Level, ATerrainInfo::StaticClass(), Viewport->Actor->Location ) );

				// Create the package/group

				UPackage* Pkg = GEditor->CreatePackage(NULL,*Proxy->Package);
				if( Proxy->Group.Len() )
					Pkg = GEditor->CreatePackage(Pkg,*Proxy->Group);

				// Create the heightmap texture and assign it to the new TerrainInfo

				if( !GEditor->StaticFindObject( UTexture::StaticClass(), Pkg, *Proxy->Name ) )
				{
					UTexture* Result = (UTexture*)GEditor->StaticConstructObject( UTexture::StaticClass(), Pkg, *Proxy->Name, RF_Public|RF_Standalone );
					Result->Format = TEXF_G16;
					Result->Init( Proxy->XSize, Proxy->YSize );
					Result->PostLoad();
					Result->Clear( TCLEAR_Temporal | TCLEAR_Bitmap );

					// Fill the new texture with all white in the RGB channels, and the fill color the user specified in the alpha channel.

					for( INT x = 0 ; x < Result->USize ; ++x )
						for( INT y = 0 ; y < Result->VSize ; ++y )
							*((_WORD*)&Result->Mips(0).DataArray( 2*(x + y*Result->USize))) = (_WORD)Proxy->Height;

					TI->TerrainMap = Result;

					// Look for a ZoneInfo in the same zone as this terraininfo.  If we find one, turn on the terrain flag.

					for( INT i = 0 ; i < GEditor->Level->Actors.Num() ; ++i )
					{
						AZoneInfo* ZI = Cast<AZoneInfo>(GEditor->Level->Actors(i));
						if( ZI && TI->IsInZone(ZI) )
							ZI->bTerrainZone = 1;
					}
				}
				else
					appMsgf(0, TEXT("This texture already exists."));

				// Update everything

				TI->PostEditChange();
				Refresh();
				GEditor->RedrawAllViewports(0);
			}
		}
		unguard;
	}
};

struct {
	TCHAR ToolTip[128];
	INT ID;
} ToolTips_PageToolsLayers[] = {
	NULL, 0
};

class WPageToolsLayers : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WPageToolsLayers,WPropertyPage,Window)

	WButton *NewButton, *DeleteButton, *DuplicateButton, *MoveUpButton, *MoveDownButton, *ShowGridButton;
	WScrollBar *LayerScrollBar;
	WToolTip *ToolTipCtrl;
	HBITMAP MoveUpBitmap, MoveDownBitmap, ShowGridBitmap;
	UViewport *Viewport;

	// Structors.
	WPageToolsLayers ( WWindow* InOwnerWindow )
	:	WPropertyPage( InOwnerWindow )
	{
		NewButton = DeleteButton = DuplicateButton = MoveUpButton = MoveDownButton = ShowGridButton = NULL;
		LayerScrollBar = NULL;
		Viewport = NULL;

		MoveUpBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_MOVE_UP), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS );	check(MoveUpBitmap);
		MoveDownBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_MOVE_DOWN), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS );	check(MoveDownBitmap);
		ShowGridBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_GRID), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS );	check(ShowGridBitmap);
	}

	virtual void OpenWindow( INT InDlgId, HMODULE InHMOD )
	{
		guard(WPageToolsLayers::OpenWindow);
		WPropertyPage::OpenWindow( InDlgId, InHMOD );

		// Create child controls and let the base class determine their proper positions.
		NewButton = new WButton( this, IDPB_NEW, FDelegate(this,(TDelegate)OnNew) );
		NewButton->OpenWindow( 1, 0, 0, 10, 10, TEXT("N") );
		DeleteButton = new WButton( this, IDPB_DELETE, FDelegate(this,(TDelegate)OnDelete) );
		DeleteButton->OpenWindow( 1, 0, 0, 10, 10, TEXT("X") );
		DuplicateButton = new WButton( this, IDPB_DUPLICATE, FDelegate(this,(TDelegate)OnDuplicate) );
		DuplicateButton->OpenWindow( 1, 0, 0, 10, 10, TEXT("D") );
		MoveUpButton = new WButton( this, IDPB_MOVE_UP, FDelegate(this,(TDelegate)OnMoveUp) );
		MoveUpButton->OpenWindow( 1, 0, 0, 10, 10, TEXT("+") );
		MoveDownButton = new WButton( this, IDPB_MOVE_DOWN, FDelegate(this,(TDelegate)OnMoveDown) );
		MoveDownButton->OpenWindow( 1, 0, 0, 10, 10, TEXT("-") );
		LayerScrollBar = new WScrollBar( this, IDSB_LAYERS );
		LayerScrollBar->OpenWindow( 1, 0, 0, 10, 10 );
		ShowGridButton = new WButton( this, IDPB_SHOW_GRID, FDelegate(this,(TDelegate)OnShowGrid) );
		ShowGridButton->OpenWindow( 1, 0, 0, 10, 10, TEXT("-") );

		PlaceControl( NewButton );
		PlaceControl( DeleteButton );
		PlaceControl( DuplicateButton );
		PlaceControl( MoveUpButton );
		PlaceControl( MoveDownButton );
		PlaceControl( ShowGridButton );
		PlaceControl( LayerScrollBar );

		Finalize();

		MoveUpButton->SetBitmap( MoveUpBitmap );
		MoveDownButton->SetBitmap( MoveDownBitmap );
		ShowGridButton->SetBitmap( ShowGridBitmap );

		// Viewport
		FName Name = TEXT("TerrainLayers");
		Viewport = GEditor->Client->NewViewport( Name );
		GEditor->Level->SpawnViewActor( Viewport );
		Viewport->Actor->ShowFlags = SHOW_StandardView | SHOW_ChildWindow;
		Viewport->Actor->RendMap   = REN_TerrainLayers;
		Viewport->Actor->Misc1 = 0;
		Viewport->Actor->Misc2 = 0;
		Viewport->Group = NAME_None;
		Viewport->MiscRes = NULL;
		Viewport->Input->Init( Viewport );

		RECT rc;
		::GetWindowRect( GetDlgItem( hWnd, IDSC_VIEWPORT_LAYERS ), &rc );
		::ScreenToClient( hWnd, (POINT*)&rc.left );
		::ScreenToClient( hWnd, (POINT*)&rc.right );
		Viewport->OpenWindow( (DWORD)hWnd, 0, (rc.right - rc.left), (rc.bottom - rc.top), rc.left, rc.top );

		ToolTipCtrl = new WToolTip(this);
		ToolTipCtrl->OpenWindow();
		for( INT tooltip = 0 ; ToolTips_PageToolsLayers[tooltip].ID > 0 ; ++tooltip )
			ToolTipCtrl->AddTool( GetDlgItem( hWnd, ToolTips_PageToolsLayers[tooltip].ID ), ToolTips_PageToolsLayers[tooltip].ToolTip, tooltip );

		unguard;
	}
	void OnDestroy()
	{
		guard(WPageToolsLayers::OnDestroy);
		WPropertyPage::OnDestroy();

		::DestroyWindow( NewButton->hWnd );
		::DestroyWindow( DeleteButton->hWnd );
		::DestroyWindow( DuplicateButton->hWnd );
		::DestroyWindow( MoveUpButton->hWnd );
		::DestroyWindow( MoveDownButton->hWnd );
		::DestroyWindow( ShowGridButton->hWnd );
		::DestroyWindow( LayerScrollBar->hWnd );

		delete NewButton;
		delete DeleteButton;
		delete DuplicateButton;
		delete MoveUpButton;
		delete MoveDownButton;
		delete ShowGridButton;
		delete LayerScrollBar;

		delete ToolTipCtrl;
		delete Viewport;

		DeleteObject( MoveUpBitmap );
		DeleteObject( MoveDownBitmap );
		DeleteObject( ShowGridBitmap );

		unguard;
	}
	virtual void Refresh()
	{
		guard(WPageToolsLayers::Refresh);
		WPropertyPage::Refresh();

		RefreshViewport();
		RefreshScrollBar();

		unguard;
	}
	void RefreshViewport()
	{
		guard(WPageToolsLayers::RefreshViewport);
		Viewport->Repaint( 1 );
		unguard;
	}
	void RefreshScrollBar()
	{
		guard(WPageToolsLayers::RefreshScrollBar);

		if( !LayerScrollBar ) return;

		// Set the scroll bar to have a valid range.
		SCROLLINFO si;
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_DISABLENOSCROLL | SIF_RANGE | SIF_POS | SIF_PAGE;
		si.nPage = Viewport->SizeY;
		si.nMin = 0;
		si.nMax = GTerrainTools.LayerScrollMax+Viewport->SizeY;
		si.nPos = GTerrainTools.LayerScrollPos;
		SetScrollInfo( LayerScrollBar->hWnd, SB_CTL, &si, TRUE );

		unguard;
	}
	void ShiftLayersUp( ATerrainInfo* InTerrainInfo, INT InStartLayer )
	{
		guard(WPageToolsLayers::ShiftLayersUp);
		for( INT x = InStartLayer-1 ; x < ARRAY_COUNT( InTerrainInfo->Layers )-1 ; ++x )
			InTerrainInfo->Layers[ x ] = InTerrainInfo->Layers[ x+1 ];
		unguard;
	}
	void ShiftLayersDown( ATerrainInfo* InTerrainInfo, INT InStartLayer )
	{
		guard(WPageToolsLayers::ShiftLayersDown);
		for( INT x = ARRAY_COUNT( InTerrainInfo->Layers )-2 ; x >= InStartLayer-1 ; --x )
			InTerrainInfo->Layers[ x+1 ] = InTerrainInfo->Layers[ x ];
		unguard;
	}
	void OnDuplicate()
	{
		guard(WPageToolsLayers::OnDuplicate);

		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
		if( TI && GTerrainTools.CurrentLayer > 0 )
		{
			// Shift all layers down - which will automatically duplicate the selected item
			ShiftLayersDown( TI, GTerrainTools.CurrentLayer );

			// Update everything
			TI->Update(0.f);
			RefreshViewport();
			GEditor->RedrawLevel( GEditor->Level );
		}

		unguard;
	}
	void OnMoveUp()
	{
		guard(WPageToolsLayers::OnMoveUp);

		// Swap the current layer with the one above up.
		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
		if( TI && GTerrainTools.CurrentLayer > 1 )
		{
			Exchange( TI->Layers[GTerrainTools.CurrentLayer-1], TI->Layers[GTerrainTools.CurrentLayer-2] );
			TI->Update(0.f);
			GTerrainTools.CurrentLayer--;
			RefreshViewport();
			GEditor->RedrawLevel( GEditor->Level );
		}

		unguard;
	}
	void OnMoveDown()
	{
		guard(WPageToolsLayers::OnMoveDown);

		// Swap the current layer with the one above up.
		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
		if( TI && GTerrainTools.CurrentLayer > 0 )
		{
			Exchange( TI->Layers[GTerrainTools.CurrentLayer-1], TI->Layers[GTerrainTools.CurrentLayer] );
			TI->Update(0.f);
			GTerrainTools.CurrentLayer++;
			RefreshViewport();
			GEditor->RedrawLevel( GEditor->Level );
		}

		unguard;
	}
	void OnShowGrid()
	{
		guard(WPageToolsLayers::OnShowGrid);

		RECT rect;
		::GetWindowRect( ShowGridButton->hWnd, &rect );

		// Grab a fresh menu
		HMENU menu = LoadMenuIdX(hInstance, IDMENU_ShowGrid),
			submenu = GetSubMenu( menu, 0 );

		// Add an item for each layer and check it appropriately
		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
		if( TI )
			for( INT x = 0 ; x < ARRAY_COUNT( TI->Layers ) ; ++x )
			{
				FTerrainLayer* Layer = &(TI->Layers[x]);
				if( Layer->AlphaMap )
				{
					MENUITEMINFOA mif;
					char Buffer[255];
					mif.cbSize = sizeof(MENUITEMINFO);
					mif.fMask = MIIM_TYPE | MIIM_ID;
					mif.fType = MFT_STRING;
					sprintf( Buffer, "%d - %s", x, TCHAR_TO_ANSI( Layer->AlphaMap->GetName() ) );
					mif.dwTypeData = Buffer;
					mif.wID = IDMN_LAYER_BASE + x;
					InsertMenuItemA( submenu, 999, TRUE, &mif );

					INT Power = appPow( 2, x );
					CheckMenuItem( submenu, mif.wID, MF_BYCOMMAND | ( (TI->ShowGrid & Power) ? MF_CHECKED : MF_UNCHECKED) );
				}
			}

		// Show the menu
		TrackPopupMenu( submenu,
			TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
			rect.left, rect.bottom, 0,
			hWnd, NULL);
		DestroyMenu( menu );

		unguard;
	}
	void OnCommand( INT Command )
	{
		guard(WPageToolsLayers::OnCommand);
		switch( Command )
		{
			case IDMN_SG_SHOW_ALL:
			{
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( TI )
					TI->ShowGrid = MAXINT;
				GEditor->RedrawLevel( GEditor->Level );
			}
			break;

			case IDMN_SG_HIDE_ALL:
			{
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( TI )
					TI->ShowGrid = 0;
				GEditor->RedrawLevel( GEditor->Level );
			}
			break;

			case IDMN_LAYER_BASE+0:
			case IDMN_LAYER_BASE+1:
			case IDMN_LAYER_BASE+2:
			case IDMN_LAYER_BASE+3:
			case IDMN_LAYER_BASE+4:
			case IDMN_LAYER_BASE+5:
			case IDMN_LAYER_BASE+6:
			case IDMN_LAYER_BASE+7:
			case IDMN_LAYER_BASE+8:
			case IDMN_LAYER_BASE+9:
			case IDMN_LAYER_BASE+10:
			case IDMN_LAYER_BASE+11:
			case IDMN_LAYER_BASE+12:
			case IDMN_LAYER_BASE+13:
			case IDMN_LAYER_BASE+14:
			case IDMN_LAYER_BASE+15:
			case IDMN_LAYER_BASE+16:
			case IDMN_LAYER_BASE+17:
			case IDMN_LAYER_BASE+18:
			case IDMN_LAYER_BASE+19:
			case IDMN_LAYER_BASE+20:
			case IDMN_LAYER_BASE+21:
			case IDMN_LAYER_BASE+22:
			case IDMN_LAYER_BASE+23:
			case IDMN_LAYER_BASE+24:
			case IDMN_LAYER_BASE+25:
			case IDMN_LAYER_BASE+26:
			case IDMN_LAYER_BASE+27:
			case IDMN_LAYER_BASE+28:
			case IDMN_LAYER_BASE+29:
			case IDMN_LAYER_BASE+30:
			case IDMN_LAYER_BASE+31:
			{
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( TI )
					TI->ShowGrid ^= (INT)appPow( 2, Command - IDMN_LAYER_BASE );
				GEditor->RedrawLevel( GEditor->Level );
			}
			break;

			default:
				WWindow::OnCommand(Command);
				break;
		}
		unguard;
	}
	virtual void OnVScroll( WPARAM wParam, LPARAM lParam )
	{
		if( (HWND)lParam == LayerScrollBar->hWnd )
		{
			switch(LOWORD(wParam)) {

				case SB_LINEUP:
					GTerrainTools.LayerScrollPos -= 32;
					GTerrainTools.LayerScrollPos = Max( GTerrainTools.LayerScrollPos, 0 );
					break;

				case SB_LINEDOWN:
					GTerrainTools.LayerScrollPos += 32;
					GTerrainTools.LayerScrollPos = Min( GTerrainTools.LayerScrollPos, GTerrainTools.LayerScrollMax );
					break;

				case SB_PAGEUP:
					GTerrainTools.LayerScrollPos -= 64;
					GTerrainTools.LayerScrollPos = Max( GTerrainTools.LayerScrollPos, 0 );
					break;

				case SB_PAGEDOWN:
					GTerrainTools.LayerScrollPos += 64;
					GTerrainTools.LayerScrollPos = Min( GTerrainTools.LayerScrollPos, GTerrainTools.LayerScrollMax );
					break;

				case SB_THUMBTRACK:
					GTerrainTools.LayerScrollPos = (short int)HIWORD(wParam);
					break;
			}

			RefreshScrollBar();
			RefreshViewport();
		}
	}
	void OnNew()
	{
		guard(WPageToolsLayers::OnNew);

		if( !GEditor->CurrentMaterial )
		{
			appMsgf( 0, TEXT("You must select a texture first.") );
			return;
		}

		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
		if( TI && GTerrainTools.CurrentLayer > 0 )
		{
			WDlgGeneric dlg( NULL, NULL, OPTIONS_NEWTERRAINLAYER, TEXT("New Layer") );
			if( dlg.DoModal( TEXT("") ) )
			{
				UOptionsNewTerrainLayer* Proxy = Cast<UOptionsNewTerrainLayer>(dlg.Proxy);
				if( !Proxy->Package.Len()
					|| !Proxy->Name.Len()
					|| !Proxy->UScale
					|| !Proxy->VScale
					|| !Proxy->AlphaWidth
					|| !Proxy->AlphaHeight
					|| Proxy->AlphaWidth&(Proxy->AlphaWidth-1)		// checks for power of two
					|| Proxy->AlphaHeight&(Proxy->AlphaHeight-1) )
				{
					appMsgf(0, TEXT("Invalid input.  Cannot create terrain layer."));
				}
				else
				{
					// Set up the layer based on the input parameters
					FTerrainLayer NewLayer;
					appMemzero( &NewLayer, sizeof(NewLayer) );
					NewLayer.Texture = NULL;
					NewLayer.UScale = Proxy->UScale;
					NewLayer.VScale = Proxy->VScale;
					NewLayer.UPan = 0;
					NewLayer.VPan = 0;
					NewLayer.TextureRotation = 0;
					NewLayer.TextureMapAxis = TEXMAPAXIS_XY;

					// Create the package/group
					UPackage* Pkg = GEditor->CreatePackage(NULL,*Proxy->Package);
					if( Proxy->Group.Len() )
						Pkg = GEditor->CreatePackage(Pkg,*Proxy->Group);

					// Create the texture
					if( !GEditor->StaticFindObject( UTexture::StaticClass(), Pkg, *Proxy->Name ) )
					{
						// Make room for the new layer
						ShiftLayersDown( TI, GTerrainTools.CurrentLayer );

						// Create new alphamap texture.
						UTexture* Result = (UTexture*)GEditor->StaticConstructObject( UTexture::StaticClass(), Pkg, *Proxy->Name, RF_Public|RF_Standalone );
						Result->Format = TEXF_RGBA8;
						Result->Init( Proxy->AlphaWidth, Proxy->AlphaHeight );
						Result->PostLoad();
						Result->Clear( TCLEAR_Temporal | TCLEAR_Bitmap );

						// Fill the new texture with all white in the RGB channels, and the fill color the user specified in the alpha channel.
						// (The fill color is assumed to be a shade of gray, so we only use the red component - G and B should be the same)
						for( INT x = 0 ; x < Result->USize ; ++x )
							for( INT y = 0 ; y < Result->VSize ; ++y )
								*((FColor*)&Result->Mips(0).DataArray( 4*(x + y*Result->USize))) = FColor(Proxy->ColorFill.R,Proxy->ColorFill.G,Proxy->ColorFill.B,Proxy->AlphaFill.R);

						NewLayer.AlphaMap = Result;
						NewLayer.Texture = (UTexture*)GEditor->CurrentMaterial;

						// Assign the new layer to the new slot.
						TI->Layers[GTerrainTools.CurrentLayer-1] = NewLayer;

						// Update everything
						TI->Update(0.f);
						GTerrainTools.CurrentLayer++;
						RefreshViewport();
						GEditor->RedrawLevel( GEditor->Level );

						GBrowserMaster->RefreshAll();
					}
					else
						appMsgf(0, TEXT("This texture already exists."));
				}
			}
		}
		unguard;
	}
	void OnDelete()
	{
		guard(WPageToolsLayers::OnDelete);

		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
		if( TI && GTerrainTools.CurrentLayer > 0 )
		{
			ShiftLayersUp( TI, GTerrainTools.CurrentLayer );

			TI->Update(0.f);
			RefreshViewport();
			GEditor->RedrawLevel( GEditor->Level );
		}

		unguard;
	}
};

struct {
	TCHAR ToolTip[128];
	INT ID;
} ToolTips_PageToolsDecoLayers[] = {
	NULL, 0
};

class WPageToolsDecoLayers : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WPageToolsDecoLayers,WPropertyPage,Window)

	WButton *NewButton, *DeleteButton, *DuplicateButton, *ShowOnTerrainButton;
	WCheckBox *ShowAlphaLayerCheck;
	HBITMAP ShowOnTerrainBitmap;
	WScrollBar *LayerScrollBar;
	WToolTip *ToolTipCtrl;
	UViewport *Viewport;

	// Structors.
	WPageToolsDecoLayers ( WWindow* InOwnerWindow )
	:	WPropertyPage( InOwnerWindow )
	{
		NewButton = DeleteButton = DuplicateButton = ShowOnTerrainButton = ShowAlphaLayerCheck = NULL;
		ShowAlphaLayerCheck = NULL;
		LayerScrollBar = NULL;
		Viewport = NULL;

		ShowOnTerrainBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_SHOW_ON_TERRAIN), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS );	check(ShowOnTerrainBitmap);
	}

	virtual void OpenWindow( INT InDlgId, HMODULE InHMOD )
	{
		guard(WPageToolsDecoLayers::OpenWindow);
		WPropertyPage::OpenWindow( InDlgId, InHMOD );

		// Create child controls and let the base class determine their proper positions.
		NewButton = new WButton( this, IDPB_NEW, FDelegate(this,(TDelegate)OnNew) );
		NewButton->OpenWindow( 1, 0, 0, 10, 10, TEXT("N") );
		DeleteButton = new WButton( this, IDPB_DELETE, FDelegate(this,(TDelegate)OnDelete) );
		DeleteButton->OpenWindow( 1, 0, 0, 10, 10, TEXT("X") );
		DuplicateButton = new WButton( this, IDPB_DUPLICATE, FDelegate(this,(TDelegate)OnDuplicate) );
		DuplicateButton->OpenWindow( 1, 0, 0, 10, 10, TEXT("D") );
		LayerScrollBar = new WScrollBar( this, IDSB_LAYERS );
		LayerScrollBar->OpenWindow( 1, 0, 0, 10, 10 );
		ShowOnTerrainButton = new WButton( this, IDPB_SHOW_ON_TERRAIN, FDelegate(this,(TDelegate)OnShowOnTerrain) );
		ShowOnTerrainButton->OpenWindow( 1, 0, 0, 10, 10, TEXT("-") );
		ShowAlphaLayerCheck = new WCheckBox( this, IDCK_DISPLAY_ALPHA_LAYER );
		ShowAlphaLayerCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );

		PlaceControl( NewButton );
		PlaceControl( DeleteButton );
		PlaceControl( DuplicateButton );
		PlaceControl( LayerScrollBar );
		PlaceControl( ShowOnTerrainButton );
		PlaceControl( ShowAlphaLayerCheck );

		Finalize();

		ShowAlphaLayerCheck->ClickDelegate = FDelegate(this, (TDelegate)OnShowAlphaLayerClick);

		ShowOnTerrainButton->SetBitmap( ShowOnTerrainBitmap );

		// Viewport
		FName Name = TEXT("TerrainDecoLayers");
		Viewport = GEditor->Client->NewViewport( Name );
		GEditor->Level->SpawnViewActor( Viewport );
		Viewport->Actor->ShowFlags = SHOW_StandardView | SHOW_ChildWindow;
		Viewport->Actor->RendMap   = REN_TerrainDecoLayers;
		Viewport->Actor->Misc1 = 0;
		Viewport->Actor->Misc2 = 0;
		Viewport->Group = NAME_None;
		Viewport->MiscRes = NULL;
		Viewport->Input->Init( Viewport );

		RECT rc;
		::GetWindowRect( GetDlgItem( hWnd, IDSC_VIEWPORT_LAYERS ), &rc );
		::ScreenToClient( hWnd, (POINT*)&rc.left );
		::ScreenToClient( hWnd, (POINT*)&rc.right );
		Viewport->OpenWindow( (DWORD)hWnd, 0, (rc.right - rc.left), (rc.bottom - rc.top), rc.left, rc.top );

		ToolTipCtrl = new WToolTip(this);
		ToolTipCtrl->OpenWindow();
		for( INT tooltip = 0 ; ToolTips_PageToolsDecoLayers[tooltip].ID > 0 ; ++tooltip )
			ToolTipCtrl->AddTool( GetDlgItem( hWnd, ToolTips_PageToolsDecoLayers[tooltip].ID ), ToolTips_PageToolsDecoLayers[tooltip].ToolTip, tooltip );

		unguard;
	}
	void OnShowAlphaLayerClick()
	{
		guard(WPageToolsDecoLayers::OnShowAlphaLayerClick);
		GTerrainTools.bShowDecoAlpha = ShowAlphaLayerCheck->IsChecked();
		GEditor->RedrawLevel( GEditor->Level );
		unguard;
	}
	void OnDestroy()
	{
		guard(WPageToolsDecoLayers::OnDestroy);
		WPropertyPage::OnDestroy();

		::DestroyWindow( NewButton->hWnd );
		::DestroyWindow( DeleteButton->hWnd );
		::DestroyWindow( DuplicateButton->hWnd );
		::DestroyWindow( LayerScrollBar->hWnd );
		::DestroyWindow( ShowOnTerrainButton->hWnd );
		::DestroyWindow( ShowAlphaLayerCheck->hWnd );

		delete NewButton;
		delete DeleteButton;
		delete DuplicateButton;
		delete LayerScrollBar;
		delete ShowOnTerrainButton;
		delete ShowAlphaLayerCheck;

		delete ToolTipCtrl;
		delete Viewport;

		DeleteObject( ShowOnTerrainBitmap );

		unguard;
	}
	virtual void Refresh()
	{
		guard(WPageToolsDecoLayers::Refresh);
		WPropertyPage::Refresh();

		RefreshViewport();
		RefreshScrollBar();

		unguard;
	}
	void RefreshViewport()
	{
		guard(WPageToolsDecoLayers::RefreshViewport);
		Viewport->Repaint( 1 );
		unguard;
	}
	void RefreshScrollBar()
	{
		guard(WPageToolsDecoLayers::RefreshScrollBar);

		if( !LayerScrollBar ) return;

		// Set the scroll bar to have a valid range.
		SCROLLINFO si;
		si.cbSize = sizeof(SCROLLINFO);
		si.fMask = SIF_DISABLENOSCROLL | SIF_RANGE | SIF_POS | SIF_PAGE;
		si.nPage = Viewport->SizeY;
		si.nMin = 0;
		si.nMax = GTerrainTools.LayerScrollMax+Viewport->SizeY;
		si.nPos = GTerrainTools.LayerScrollPos;
		SetScrollInfo( LayerScrollBar->hWnd, SB_CTL, &si, TRUE );

		unguard;
	}
	virtual void OnVScroll( WPARAM wParam, LPARAM lParam )
	{
		if( (HWND)lParam == LayerScrollBar->hWnd )
		{
			switch(LOWORD(wParam)) {

				case SB_LINEUP:
					GTerrainTools.DecoLayerScrollPos -= 32;
					GTerrainTools.DecoLayerScrollPos = Max( GTerrainTools.DecoLayerScrollPos, 0 );
					break;

				case SB_LINEDOWN:
					GTerrainTools.DecoLayerScrollPos += 32;
					GTerrainTools.DecoLayerScrollPos = Min( GTerrainTools.DecoLayerScrollPos, GTerrainTools.DecoLayerScrollMax );
					break;

				case SB_PAGEUP:
					GTerrainTools.DecoLayerScrollPos -= 64;
					GTerrainTools.DecoLayerScrollPos = Max( GTerrainTools.DecoLayerScrollPos, 0 );
					break;

				case SB_PAGEDOWN:
					GTerrainTools.DecoLayerScrollPos += 64;
					GTerrainTools.DecoLayerScrollPos = Min( GTerrainTools.DecoLayerScrollPos, GTerrainTools.DecoLayerScrollMax );
					break;

				case SB_THUMBTRACK:
					GTerrainTools.DecoLayerScrollPos = (short int)HIWORD(wParam);
					break;
			}

			RefreshScrollBar();
			RefreshViewport();
		}
	}
	void OnShowOnTerrain()
	{
		guard(WPageToolsDecoLayers::OnShowOnTerrain);

		RECT rect;
		::GetWindowRect( ShowOnTerrainButton->hWnd, &rect );

		// Grab a fresh menu
		HMENU menu = LoadMenuIdX(hInstance, IDMENU_ShowOnTerrain),
			submenu = GetSubMenu( menu, 0 );

		// Add an item for each layer and check it appropriately
		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
		if( TI )
			for( INT x = 0 ; x < TI->DecoLayers.Num() ; ++x )
			{
				FDecorationLayer* Layer = &(TI->DecoLayers(x));

				MENUITEMINFOA mif;
				char Buffer[255];
				mif.cbSize = sizeof(MENUITEMINFO);
				mif.fMask = MIIM_TYPE | MIIM_ID;
				mif.fType = MFT_STRING;
				if( Layer->StaticMesh )
					sprintf( Buffer, "%d - %s", x, TCHAR_TO_ANSI( Layer->StaticMesh->GetName() ) );
				else
					sprintf( Buffer, "%d - No Static Mesh", x );
				mif.dwTypeData = Buffer;
				mif.wID = IDMN_DECOLAYER_BASE + x;
				InsertMenuItemA( submenu, 999, TRUE, &mif );

				CheckMenuItem( submenu, mif.wID, MF_BYCOMMAND | ( Layer->ShowOnTerrain ? MF_CHECKED : MF_UNCHECKED ) );
			}

		// Show the menu
		TrackPopupMenu( submenu,
			TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
			rect.left, rect.bottom, 0,
			hWnd, NULL);
		DestroyMenu( menu );

		unguard;
	}
	void OnCommand( INT Command )
	{
		guard(WPageToolsDecoLayers::OnCommand);
		switch( Command )
		{
			case IDMN_SOT_SHOW_ALL:
			{
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( TI )
					for( INT x = 0 ; x < TI->DecoLayers.Num() ; ++x )
						TI->DecoLayers(x).ShowOnTerrain = 1;
				GEditor->RedrawLevel( GEditor->Level );
			}
			break;

			case IDMN_SOT_HIDE_ALL:
			{
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( TI )
					for( INT x = 0 ; x < TI->DecoLayers.Num() ; ++x )
						TI->DecoLayers(x).ShowOnTerrain = 0;
				GEditor->RedrawLevel( GEditor->Level );
			}
			break;

			case IDMN_DECOLAYER_BASE+0:
			case IDMN_DECOLAYER_BASE+1:
			case IDMN_DECOLAYER_BASE+2:
			case IDMN_DECOLAYER_BASE+3:
			case IDMN_DECOLAYER_BASE+4:
			case IDMN_DECOLAYER_BASE+5:
			case IDMN_DECOLAYER_BASE+6:
			case IDMN_DECOLAYER_BASE+7:
			case IDMN_DECOLAYER_BASE+8:
			case IDMN_DECOLAYER_BASE+9:
			case IDMN_DECOLAYER_BASE+10:
			case IDMN_DECOLAYER_BASE+11:
			case IDMN_DECOLAYER_BASE+12:
			case IDMN_DECOLAYER_BASE+13:
			case IDMN_DECOLAYER_BASE+14:
			case IDMN_DECOLAYER_BASE+15:
			case IDMN_DECOLAYER_BASE+16:
			case IDMN_DECOLAYER_BASE+17:
			case IDMN_DECOLAYER_BASE+18:
			case IDMN_DECOLAYER_BASE+19:
			case IDMN_DECOLAYER_BASE+20:
			case IDMN_DECOLAYER_BASE+21:
			case IDMN_DECOLAYER_BASE+22:
			case IDMN_DECOLAYER_BASE+23:
			case IDMN_DECOLAYER_BASE+24:
			case IDMN_DECOLAYER_BASE+25:
			case IDMN_DECOLAYER_BASE+26:
			case IDMN_DECOLAYER_BASE+27:
			case IDMN_DECOLAYER_BASE+28:
			case IDMN_DECOLAYER_BASE+29:
			case IDMN_DECOLAYER_BASE+30:
			case IDMN_DECOLAYER_BASE+31:
			{
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( TI )
					TI->DecoLayers(Command-IDMN_DECOLAYER_BASE).ShowOnTerrain ^= 1;
				GEditor->RedrawLevel( GEditor->Level );
			}
			break;

			default:
				WWindow::OnCommand(Command);
				break;
		}
		unguard;
	}
	void OnNew()
	{
		guard(WPageToolsDecoLayers::OnNew);

		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
		if( TI )
		{
			INT Idx = GTerrainTools.CurrentLayer >= 32 ? GTerrainTools.CurrentLayer-32 : TI->DecoLayers.Num();

			TI->DecoLayers.InsertZeroed( Idx );
			FDecorationLayer* Layer = &(TI->DecoLayers( Idx ));

			// Set up some reasonable defaults for other fields.
			Layer->ShowOnTerrain = 1;
			Layer->AlignToTerrain = 0;
			Layer->DensityMultiplier = .1f;
			Layer->MaxPerQuad = 1;
			Layer->ScaleMultiplier = FVector(1,1,1);

			Layer->StaticMesh = NULL;
			Layer->DensityMap = NULL;
			Layer->ScaleMap = NULL;
			Layer->ColorMap = NULL;

			TI->PostEditChange();

			GEditor->RedrawLevel( GEditor->Level );
			Viewport->Repaint(1);
		}
		unguard;
	}
	void OnDelete()
	{
		guard(WPageToolsDecoLayers::OnDelete);
		
		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
		if( TI && GTerrainTools.CurrentLayer >= 32 )
		{
			TI->DecoLayers.Remove( GTerrainTools.CurrentLayer-32, 1 );
			RefreshViewport();
			GEditor->RedrawLevel( GEditor->Level );
		}

		unguard;
	}
	void OnDuplicate()
	{
		guard(WPageToolsDecoLayers::OnDuplicate);

		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
		if( TI && GTerrainTools.CurrentLayer >= 32 )
		{
			TI->DecoLayers.InsertZeroed( GTerrainTools.CurrentLayer-32+1 );
			TI->DecoLayers(GTerrainTools.CurrentLayer-32+1) = TI->DecoLayers(GTerrainTools.CurrentLayer-32);
			GTerrainTools.CurrentLayer++;

			RefreshViewport();
			GEditor->RedrawLevel( GEditor->Level );
		}

		unguard;
	}
};

class WTerrainToolsSheet : public WWindow
{
	DECLARE_WINDOWCLASS(WTerrainToolsSheet,WWindow,Window)

	WPropertySheet* ToolsSheet;
	WPageToolsTerrains* ToolsTerrainsPage;
	WPageToolsLayers* ToolsLayersPage;
	WPageToolsDecoLayers* ToolsDecoLayersPage;

	// Structors.
	WTerrainToolsSheet( FName InPersistentName, WWindow* InOwnerWindow )
	:	WWindow( InPersistentName, InOwnerWindow )
	{
	}

	// WTerrainToolsSheet interface.
	void OpenWindow()
	{
		guard(WTerrainToolsSheet::OpenWindow);
		MdiChild = 0;
		PerformCreateWindowEx
		(
			NULL,
			TEXT("SubTools"),
			WS_VISIBLE | WS_CHILD,
			0, 0,
			0, 0,
			OwnerWindow ? OwnerWindow->hWnd : NULL,
			NULL,
			hInstance
		);
 
		unguard;
	}
	void OnCreate()
	{
		guard(WTerrainToolsSheet::OnCreate);
		WWindow::OnCreate();

		// Create the sheet
		ToolsSheet = new WPropertySheet( this, IDPS_TERRAIN_TOOLS_EDIT );
		ToolsSheet->OpenWindow( 1, 0 );

		// Create the pages for the sheet
		ToolsTerrainsPage = new WPageToolsTerrains( ToolsSheet->Tabs );
		ToolsTerrainsPage->OpenWindow( IDPP_TE_TOOLS_TERRAINS, GUnrealEdModule);
		ToolsSheet->AddPage( ToolsTerrainsPage );

		ToolsLayersPage = new WPageToolsLayers( ToolsSheet->Tabs );
		ToolsLayersPage->OpenWindow( IDPP_TE_TOOLS_LAYERS, GUnrealEdModule);
		ToolsSheet->AddPage( ToolsLayersPage );

		ToolsDecoLayersPage = new WPageToolsDecoLayers( ToolsSheet->Tabs );
		ToolsDecoLayersPage->OpenWindow( IDPP_TE_TOOLS_DECORATIONS, GUnrealEdModule);
		ToolsSheet->AddPage( ToolsDecoLayersPage );

		ToolsSheet->SetCurrent( 0 );

		// Resize the property sheet to surround the pages properly.
		RECT rect;
		::GetClientRect( ToolsTerrainsPage->hWnd, &rect );
		::SetWindowPos( hWnd, HWND_TOP, 0, 0, rect.right, rect.bottom, SWP_NOMOVE );

		PositionChildControls();

		unguard;
	}
	void OnDestroy()
	{
		guard(WTerrainToolsSheet::OnDestroy);
		WWindow::OnDestroy();

		delete ToolsTerrainsPage;
		delete ToolsLayersPage;
		delete ToolsDecoLayersPage;
		delete ToolsSheet;
		unguard;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WTerrainToolsSheet::OnSize);
		WWindow::OnSize(Flags, NewX, NewY);
		PositionChildControls();
		InvalidateRect( hWnd, NULL, FALSE );
		unguard;
	}
	void PositionChildControls()
	{
		guard(WTerrainToolsSheet::PositionChildControls);
		if( !ToolsSheet || !::IsWindow( ToolsSheet->hWnd )
				)
			return;

		FRect CR = GetClientRect();
		::MoveWindow( ToolsSheet->hWnd, 0, 0, CR.Width(), CR.Height(), 1 );

		unguard;
	}
};

// --------------------------------------------------------------
//
// WPageTools
//
// --------------------------------------------------------------

struct {
	TCHAR ToolTip[64];
	INT ID;
} ToolTips_PageTools[] = {
	TEXT("Adjust the Inner Radius"), IDSL_INNER_RADIUS,
	TEXT("Adjust the Outer Radius"), IDSL_OUTER_RADIUS,
	TEXT("Adjust the Strength of the Tool"), IDSL_STRENGTH,
	TEXT("The Base Adjustment Value"), IDEC_ADJUST,
	TEXT("Mirror brush effects across which axis?"), IDCB_MIRROR,
	TEXT("Automatically Soft Select when Clicking Vertices?"), IDCK_AUTO_SOFT_SEL,
	TEXT("Use These Settings on a Per Tool Basis?"), IDCK_PER_TOOL,
	TEXT("Perform a Soft Select Now"), IDPB_SOFT_SEL,
	TEXT("Undo the Last Vertex Movement"), IDPB_RESETMOVE,
	TEXT("Insert New Layer"), IDPB_NEW,
	TEXT("Delete Layer"), IDPB_DELETE,
	TEXT("Duplicate Layer"), IDPB_DUPLICATE,
	TEXT("Move Layer Up"), IDPB_MOVE_UP,
	TEXT("Move Layer Down"), IDPB_MOVE_DOWN,
	TEXT("Show Grid Options"), IDPB_SHOW_GRID,
	TEXT("Lock the Radii Sliders Movements Together"), IDCK_LOCK_SLIDERS,
	TEXT("Insert New Terrain"), IDPB_TERRAIN_NEW,
	TEXT("Delete Selected Terrain"), IDPB_TERRAIN_DELETE,
	TEXT("Ignore invisible quads when doing traces?"), IDCK_IGNORE_INVISIBLE_QUADS,
	NULL, 0
};

class WPageTools : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WPageTools,WPropertyPage,Window)

	WGroupBox *TypesBox, *OptionsBox, *SoftSelBox, *AdjustBox;
	WListBox *BrushesList;
	WEdit *InnerRadiusEdit, *OuterRadiusEdit, *StrengthEdit, *AdjustEdit;
	WComboBox *MirrorCombo;
	WTrackBar *InnerRadiusBar, *OuterRadiusBar, *StrengthBar;
	WCheckBox *AutoSoftSelCheck, *LockSlidersCheck, *PerToolCheck, *IgnoreInvisQuadsCheck;
	WButton *SelectButton, *UndoButton;
	WColorButton *ColorButton;
	WLabel *SubtoolsLabel;
	HBITMAP LockSlidersBitmap;

	WTerrainToolsSheet* ToolsSheet;

	WToolTip* ToolTipCtrl;

	INT SliderDiff;

	// Structors.
	WPageTools ( WWindow* InOwnerWindow )
	:	WPropertyPage( InOwnerWindow )
	{
		TypesBox = OptionsBox = SoftSelBox = AdjustBox = NULL;
		BrushesList = NULL;
		InnerRadiusEdit = OuterRadiusEdit = StrengthEdit = AdjustEdit = NULL;
		MirrorCombo = NULL;
		InnerRadiusBar = OuterRadiusBar = StrengthBar = NULL;
		AutoSoftSelCheck = LockSlidersCheck = PerToolCheck = IgnoreInvisQuadsCheck = NULL;
		SelectButton = UndoButton = NULL;
		ColorButton = NULL;
		SubtoolsLabel = NULL;
		ToolsSheet = NULL;

		LockSlidersBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_LOCK), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS );	check(LockSlidersBitmap);
	}

	virtual void OpenWindow( INT InDlgId, HMODULE InHMOD )
	{
		guard(WPageTools::OpenWindow);
		WPropertyPage::OpenWindow( InDlgId, InHMOD );

		// Create child controls and let the base class determine their proper positions.
		TypesBox = new WGroupBox( this, IDGP_TYPES );
		TypesBox->OpenWindow( 1, 0 );
		OptionsBox = new WGroupBox( this, IDGP_OPTIONS );
		OptionsBox->OpenWindow( 1, 0 );
		SoftSelBox = new WGroupBox( this, IDGP_SOFT_SEL );
		SoftSelBox->OpenWindow( 1, 0 );
		AdjustBox = new WGroupBox( this, IDGP_ADJUST );
		AdjustBox->OpenWindow( 1, 0 );
		BrushesList = new WListBox( this, IDLB_TYPES );
		BrushesList->OpenWindow( 1, 0, 0, 0 );
		InnerRadiusEdit = new WEdit( this, IDEC_INNER_RADIUS );
		InnerRadiusEdit->OpenWindow( 1, 0, 0 );
		OuterRadiusEdit = new WEdit( this, IDEC_OUTER_RADIUS );
		OuterRadiusEdit->OpenWindow( 1, 0, 0 );
		StrengthEdit = new WEdit( this, IDEC_STRENGTH );
		StrengthEdit->OpenWindow( 1, 0, 0 );
		AdjustEdit = new WEdit( this, IDEC_ADJUST );
		AdjustEdit->OpenWindow( 1, 0, 0 );
		MirrorCombo = new WComboBox( this, IDCB_MIRROR );
		MirrorCombo->OpenWindow( 1, 0 );
		InnerRadiusBar = new WTrackBar( this, IDSL_INNER_RADIUS );
		InnerRadiusBar->OpenWindow( 1, 0 );
		OuterRadiusBar = new WTrackBar( this, IDSL_OUTER_RADIUS );
		OuterRadiusBar->OpenWindow( 1, 0 );
		StrengthBar = new WTrackBar( this, IDSL_STRENGTH );
		StrengthBar->OpenWindow( 1, 0 );
		AutoSoftSelCheck = new WCheckBox( this, IDCK_AUTO_SOFT_SEL );
		AutoSoftSelCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
		PerToolCheck = new WCheckBox( this, IDCK_PER_TOOL );
		PerToolCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
		LockSlidersCheck = new WCheckBox( this, IDCK_LOCK_SLIDERS );
		LockSlidersCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
		SelectButton = new WButton( this, IDPB_SOFT_SEL );
		SelectButton->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
		UndoButton = new WButton( this, IDPB_RESETMOVE );
		UndoButton->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
		ColorButton = new WColorButton( this, IDPB_COLOR );
		ColorButton->OpenWindow( 1, 0 );
		ColorButton->SetColor( GTerrainTools.Color.R, GTerrainTools.Color.G, GTerrainTools.Color.B );
		SubtoolsLabel = new WLabel( this, IDSC_TE_SUBTOOLS );
		SubtoolsLabel->OpenWindow( 0 );
		IgnoreInvisQuadsCheck = new WCheckBox( this, IDCK_IGNORE_INVISIBLE_QUADS );
		IgnoreInvisQuadsCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );

		PlaceControl( TypesBox );
		PlaceControl( OptionsBox );
		PlaceControl( SoftSelBox );
		PlaceControl( AdjustBox );
		PlaceControl( BrushesList );
		PlaceControl( InnerRadiusEdit );
		PlaceControl( OuterRadiusEdit );
		PlaceControl( StrengthEdit );
		PlaceControl( AdjustEdit );
		PlaceControl( MirrorCombo );
		PlaceControl( InnerRadiusBar );
		PlaceControl( OuterRadiusBar );
		PlaceControl( StrengthBar );
		PlaceControl( AutoSoftSelCheck );
		PlaceControl( PerToolCheck );
		PlaceControl( LockSlidersCheck );
		PlaceControl( SelectButton );
		PlaceControl( UndoButton );
		PlaceControl( ColorButton );
		PlaceControl( SubtoolsLabel );
		PlaceControl( IgnoreInvisQuadsCheck );

		Finalize();

		// Sub tools sheet
		ToolsSheet = new WTerrainToolsSheet( TEXT("SubTools"), this );
		ToolsSheet->OpenWindow();

		RECT rc, rcC;
		::GetWindowRect( GetDlgItem( hWnd, IDSC_TE_SUBTOOLS ), &rc );
		rcC = rc;
		::ScreenToClient( hWnd, (POINT*)&rcC.left );
		::MoveWindow( ToolsSheet->hWnd, rcC.left, rcC.top, rc.right - rc.left, rc.bottom - rc.top, 1 );

		InnerRadiusBar->SetRange( 1, 4000);
		OuterRadiusBar->SetRange( 1, 4000);
		StrengthBar->SetRange( 1, 100);
		AutoSoftSelCheck->SetCheck( GTerrainTools.bAutoSoftSel ? BST_CHECKED : BST_UNCHECKED );
		LockSlidersCheck->SetCheck( GTerrainTools.bLockSliders ? BST_CHECKED : BST_UNCHECKED );
		PerToolCheck->SetCheck( GTerrainTools.bPerTool ? BST_CHECKED : BST_UNCHECKED );
		IgnoreInvisQuadsCheck->SetCheck( GTerrainTools.bIgnoreInvisibleQuads ? BST_CHECKED : BST_UNCHECKED );

		RefreshToolValues();

		// Delegates.
		SelectButton->ClickDelegate = FDelegate(this, (TDelegate)OnSelectClick);
		UndoButton->ClickDelegate = FDelegate(this, (TDelegate)OnResetMoveClick);
		InnerRadiusBar->ThumbTrackDelegate = FDelegate(this, (TDelegate)InnerRadiusPosChange);
		InnerRadiusBar->ThumbPositionDelegate = FDelegate(this, (TDelegate)InnerRadiusPosChange);
		OuterRadiusBar->ThumbTrackDelegate = FDelegate(this, (TDelegate)OuterRadiusPosChange);
		OuterRadiusBar->ThumbPositionDelegate = FDelegate(this, (TDelegate)OuterRadiusPosChange);
		StrengthBar->ThumbTrackDelegate = FDelegate(this, (TDelegate)StrengthPosChange);
		StrengthBar->ThumbPositionDelegate = FDelegate(this, (TDelegate)StrengthPosChange);
		AutoSoftSelCheck->ClickDelegate = FDelegate(this, (TDelegate)OnAutoSoftSelClick);
		PerToolCheck->ClickDelegate = FDelegate(this, (TDelegate)OnPerToolClick);
		LockSlidersCheck->ClickDelegate = FDelegate(this, (TDelegate)OnLockSlidersClick);
		BrushesList->SelectionChangeDelegate = FDelegate(this, (TDelegate)OnTypesSelChange);
		AdjustEdit->ChangeDelegate = FDelegate(this, (TDelegate)OnAdjustChange);
		MirrorCombo->SelectionChangeDelegate = FDelegate(this, (TDelegate)OnMirrorSelChange);
		ColorButton->ClickDelegate = FDelegate(this, (TDelegate)OnColorClicked);
		IgnoreInvisQuadsCheck->ClickDelegate = FDelegate(this, (TDelegate)OnIgnoreInvisibleQuadsClick);

		LockSlidersCheck->SetBitmap( LockSlidersBitmap );

		MirrorCombo->AddString( TEXT("None") );
		MirrorCombo->AddString( TEXT("X") );
		MirrorCombo->AddString( TEXT("Y") );
		MirrorCombo->AddString( TEXT("XY") );
		MirrorCombo->SetCurrent( 0 );

		for( INT x = 0 ; x < GTerrainTools.Brushes.Num() ; ++x )
			BrushesList->AddString( *GTerrainTools.Brushes(x).Desc );
		BrushesList->SetCurrent(0);

		ToolTipCtrl = new WToolTip(this);
		ToolTipCtrl->OpenWindow();
		for( INT tooltip = 0 ; ToolTips_PageTools[tooltip].ID > 0 ; ++tooltip )
			ToolTipCtrl->AddTool( GetDlgItem( hWnd, ToolTips_PageTools[tooltip].ID ), ToolTips_PageTools[tooltip].ToolTip, tooltip );

		unguard;
	}
	void OnColorClicked()
	{
		INT R, G, B;
		ColorButton->GetColor( R, G, B );

		CHOOSECOLORA cc;
		static COLORREF acrCustClr[16];
		appMemzero( &cc, sizeof(cc) );
		cc.lStructSize  = sizeof(cc);
		cc.hwndOwner    = ColorButton->hWnd;
		cc.lpCustColors = (LPDWORD)acrCustClr;
		cc.rgbResult    = RGB(R, G, B);
		cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
		if( ChooseColorA(&cc)==TRUE )
			ColorButton->SetColor( GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult) );

		ColorButton->GetColor( R, G, B );
		GTerrainTools.Color = FColor( R, G, B );
	}
	void OnAdjustChange()
	{
		GTerrainTools.SetAdjust( appAtoi( *AdjustEdit->GetText() ) );
	}
	void OnMirrorSelChange()
	{
		GTerrainTools.SetMirrorAxis( MirrorCombo->GetCurrent() );
	}
	void InnerRadiusPosChange()
	{
		GTerrainTools.SetInnerRadius( InnerRadiusBar->GetPos() );

		if( GTerrainTools.GetInnerRadius() > GTerrainTools.GetOuterRadius() )
			GTerrainTools.SetOuterRadius( GTerrainTools.GetInnerRadius() );

		RefreshToolValues();
	}
	void OuterRadiusPosChange()
	{
		GTerrainTools.SetOuterRadius( OuterRadiusBar->GetPos() );

		if( GTerrainTools.bLockSliders )
		{
			InnerRadiusBar->SetPos( GTerrainTools.GetOuterRadius() - SliderDiff );
			InnerRadiusPosChange();
		}
		else
			if( GTerrainTools.GetOuterRadius() < GTerrainTools.GetInnerRadius() )
				GTerrainTools.SetInnerRadius( GTerrainTools.GetOuterRadius() );

		RefreshToolValues();
	}
	void StrengthPosChange()
	{
		GTerrainTools.SetStrength( StrengthBar->GetPos() );
		RefreshToolValues();
	}
	void OnAutoSoftSelClick()
	{
		GTerrainTools.bAutoSoftSel = AutoSoftSelCheck->IsChecked();
	}
	void OnIgnoreInvisibleQuadsClick()
	{
		GTerrainTools.bIgnoreInvisibleQuads = IgnoreInvisQuadsCheck->IsChecked();
	}
	void OnPerToolClick()
	{
		GTerrainTools.bPerTool = PerToolCheck->IsChecked();

		RefreshToolValues();
	}
	void OnLockSlidersClick()
	{
		GTerrainTools.bLockSliders = LockSlidersCheck->IsChecked();

		EnableWindow( InnerRadiusBar->hWnd, !LockSlidersCheck->IsChecked() );
		EnableWindow( InnerRadiusEdit->hWnd, !LockSlidersCheck->IsChecked() );

		SliderDiff = GTerrainTools.GetOuterRadius() - GTerrainTools.GetInnerRadius();
	}
	void OnSelectClick()
	{
		GEditor->Exec( TEXT("TERRAIN SOFTSELECT") );
	}
	void OnResetMoveClick()
	{
		GEditor->Exec( TEXT("TERRAIN RESETMOVE") );
		GEditor->RedrawLevel( GEditor->Level );
	}
	void OnTypesSelChange()
	{
		FString BrushName = BrushesList->GetString( BrushesList->GetCurrent() );
		GEditor->Exec( *FString::Printf(TEXT("TERRAIN BRUSH %s"), *GTerrainTools.GetExecFromBrushName( BrushName ) ));

		RefreshToolValues();
	}
	void OnDestroy()
	{
		guard(WPageTools::OnDestroy);
		WPropertyPage::OnDestroy();

		::DestroyWindow( TypesBox->hWnd );
		::DestroyWindow( OptionsBox->hWnd );
		::DestroyWindow( SoftSelBox->hWnd );
		::DestroyWindow( AdjustBox->hWnd );
		::DestroyWindow( BrushesList->hWnd );
		::DestroyWindow( InnerRadiusEdit->hWnd );
		::DestroyWindow( OuterRadiusEdit->hWnd );
		::DestroyWindow( StrengthEdit->hWnd );
		::DestroyWindow( AdjustEdit->hWnd );
		::DestroyWindow( InnerRadiusBar->hWnd );
		::DestroyWindow( OuterRadiusBar->hWnd );
		::DestroyWindow( StrengthBar->hWnd );
		::DestroyWindow( AutoSoftSelCheck->hWnd );
		::DestroyWindow( PerToolCheck->hWnd );
		::DestroyWindow( LockSlidersCheck->hWnd );
		::DestroyWindow( SelectButton->hWnd );
		::DestroyWindow( UndoButton->hWnd );
		::DestroyWindow( ColorButton->hWnd );
		::DestroyWindow( SubtoolsLabel->hWnd );
		::DestroyWindow( ToolsSheet->hWnd );
		::DestroyWindow( IgnoreInvisQuadsCheck->hWnd );

		delete TypesBox;
		delete OptionsBox;
		delete SoftSelBox;
		delete AdjustBox;
		delete BrushesList;
		delete InnerRadiusEdit;
		delete OuterRadiusEdit;
		delete StrengthEdit;
		delete AdjustEdit;
		delete InnerRadiusBar;
		delete OuterRadiusBar;
		delete StrengthBar;
		delete AutoSoftSelCheck;
		delete PerToolCheck;
		delete LockSlidersCheck;
		delete SelectButton;
		delete UndoButton;
		delete ColorButton;
		delete SubtoolsLabel;
		delete ToolsSheet;
		delete IgnoreInvisQuadsCheck;

		DeleteObject( LockSlidersBitmap );

		delete ToolTipCtrl;

		unguard;
	}
	void RefreshToolValues()
	{
		guard(WPageTools::RefreshToolValues);

		if( !::IsWindow( InnerRadiusEdit->hWnd ) )
			return;

		INT InnerRadius = GTerrainTools.GetInnerRadius();
		INT OuterRadius = GTerrainTools.GetOuterRadius();
		INT Strength = GTerrainTools.GetStrength();
		INT Adjust = GTerrainTools.GetAdjust();

		InnerRadiusEdit->SetText( *FString::Printf(TEXT("%d"),InnerRadius) );
		OuterRadiusEdit->SetText( *FString::Printf(TEXT("%d"),OuterRadius) );
		StrengthEdit->SetText( *FString::Printf(TEXT("%d"),Strength) );
		AdjustEdit->SetText( *FString::Printf(TEXT("%d"),Adjust) );
		MirrorCombo->SetCurrent( GTerrainTools.GetMirrorAxis() );

		InnerRadiusBar->SetPos( InnerRadius );
		OuterRadiusBar->SetPos( OuterRadius );
		StrengthBar->SetPos( Strength );

		unguard;
	}
	virtual void Refresh()
	{
		guard(WPageTools::Refresh);
		WPropertyPage::Refresh();
		RefreshToolValues();
		ToolsSheet->ToolsSheet->RefreshPages();
		unguard;
	}
};

// --------------------------------------------------------------
//
// WPageMisc
//
// --------------------------------------------------------------

struct {
	TCHAR ToolTip[128];
	INT ID;
} ToolTips_PageMisc[] = {
	TEXT("Show a Wireframe Grid on the Terrain?"), IDCK_SHOW_GRID,
	TEXT("Move actors relative to nearby vertices when editing the heightmap?"), IDCK_MOVE_ACTORS,
	NULL, 0
};

class WPageMisc : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WPageMisc,WPropertyPage,Window)

	WGroupBox *TerrainGenerationGroup;
	WCheckBox *MoveActorsCheck, *UseEntireHeightmapCheck;
	WButton *BuildButton;
	WEdit *StepsEdit, *StrengthEdit;

	WToolTip* ToolTipCtrl;
	UBOOL bUseEntireHeightmap;

	// Structors.
	WPageMisc ( WWindow* InOwnerWindow )
	:	WPropertyPage( InOwnerWindow )
	{
		TerrainGenerationGroup = NULL;
		MoveActorsCheck = UseEntireHeightmapCheck = NULL;
		BuildButton = NULL;
		StepsEdit = StrengthEdit = NULL;
		bUseEntireHeightmap = 0;
	}

	virtual void OpenWindow( INT InDlgId, HMODULE InHMOD )
	{
		guard(WPageMisc::OpenWindow);
		WPropertyPage::OpenWindow( InDlgId, InHMOD );

		// Create child controls and let the base class determine their proper positions.
		TerrainGenerationGroup = new WGroupBox( this, IDGP_TERRAIN_GENERATION );
		TerrainGenerationGroup->OpenWindow( 1, 0 );
		MoveActorsCheck = new WCheckBox( this, IDCK_MOVE_ACTORS );
		MoveActorsCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
		UseEntireHeightmapCheck = new WCheckBox( this, IDCK_USE_ENTIRE_HEIGHTMAP );
		UseEntireHeightmapCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
		BuildButton = new WButton( this, IDPB_BUILD );
		BuildButton->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
		StepsEdit = new WEdit( this, IDEC_STEPS );
		StepsEdit->OpenWindow( 1, 0, 0 );
		StrengthEdit = new WEdit( this, IDEC_STRENGTH );
		StrengthEdit->OpenWindow( 1, 0, 0 );

		PlaceControl( TerrainGenerationGroup );
		PlaceControl( MoveActorsCheck );
		PlaceControl( UseEntireHeightmapCheck );
		PlaceControl( BuildButton );
		PlaceControl( StepsEdit );
		PlaceControl( StrengthEdit );

		Finalize();

		MoveActorsCheck->SetCheck( GTerrainTools.bMoveActors );
		UseEntireHeightmapCheck->SetCheck( bUseEntireHeightmap );

		// Delegates.
		MoveActorsCheck->ClickDelegate = FDelegate(this, (TDelegate)OnMoveActorsClick);
		UseEntireHeightmapCheck->ClickDelegate = FDelegate(this, (TDelegate)OnUseEntireHeightmapClick);
		BuildButton->ClickDelegate = FDelegate(this, (TDelegate)OnBuildClick);

		StepsEdit->SetText(TEXT("5"));
		StrengthEdit->SetText(TEXT("250"));

		ToolTipCtrl = new WToolTip(this);
		ToolTipCtrl->OpenWindow();
		for( INT tooltip = 0 ; ToolTips_PageMisc[tooltip].ID > 0 ; ++tooltip )
			ToolTipCtrl->AddTool( GetDlgItem( hWnd, ToolTips_PageMisc[tooltip].ID ), ToolTips_PageMisc[tooltip].ToolTip, tooltip );

		unguard;
	}
	void OnMoveActorsClick()
	{
		GTerrainTools.bMoveActors = MoveActorsCheck->IsChecked();
	}
	void OnUseEntireHeightmapClick()
	{
		bUseEntireHeightmap = UseEntireHeightmapCheck->IsChecked();
	}
	void OnDestroy()
	{
		guard(WPageMisc::OnDestroy);
		WPropertyPage::OnDestroy();

		::DestroyWindow( TerrainGenerationGroup->hWnd );
		::DestroyWindow( MoveActorsCheck->hWnd );
		::DestroyWindow( UseEntireHeightmapCheck->hWnd );
		::DestroyWindow( BuildButton->hWnd );
		::DestroyWindow( StepsEdit->hWnd );
		::DestroyWindow( StrengthEdit->hWnd );

		delete TerrainGenerationGroup;
		delete MoveActorsCheck;
		delete UseEntireHeightmapCheck;
		delete BuildButton;
		delete StepsEdit;
		delete StrengthEdit;

		delete ToolTipCtrl;

		unguard;
	}

	void OnBuildClick()
	{
		guard(WPageMisc::OnBuildClick);

		//
		// Find the current terrain and verify the brush type
		//

		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
		if( !TI ) return;

		if( GTerrainTools.CurrentBrush->ID != TB_Select )
		{
			appMsgf(0, TEXT("Only available when using the 'select' tool."));
			return;
		}

		//
		// Validate user input
		//

		INT Steps = appAtoi( *StepsEdit->GetText() );
		FLOAT Strength = appAtof( *StrengthEdit->GetText() );

		if( !Steps || !Strength )
		{
			appMsgf(0, TEXT("Invalid input."));
			return;
		}

		INT HeightmapWidth = TI->TerrainMap->USize, HeightmapHeight = TI->TerrainMap->VSize;
		FBox Selection = GTerrainTools.CurrentBrush->GetRect();
		FVector Start = Selection.Min.TransformPointBy( TI->ToHeightmap ),
			End = Selection.Max.TransformPointBy( TI->ToHeightmap );

		if( bUseEntireHeightmap )
		{
			Start.X = Start.Y = 0;
			End.X = HeightmapWidth;
			End.Y = HeightmapHeight;
		}

		INT XSize = End.X - Start.X;
		INT YSize = End.Y - Start.Y;

		FLOAT UFactor = 1.f / HeightmapWidth, VFactor = 1.f / HeightmapHeight;

		UFactor *= Steps;
		VFactor *= Steps;

		FPerlinNoise PN;
		PN.init();

		for( INT x = Start.X ; x < End.X ; ++x )
		{
			for( INT y = Start.Y ; y < End.Y ; ++y )
			{
				FLOAT Values[2] = { x * UFactor, y * VFactor };
				FLOAT noise = PN.noise2( Values ) * (bUseEntireHeightmap ? 100 : 10);

				FLOAT Height = TI->GetHeightmap(x, y);

				if( bUseEntireHeightmap )
					Height = 32768 + (noise * Strength);
				else
					Height = Height + (noise * Strength);

				TI->SetHeightmap(x, y, Height);
			}
		}

		//
		// Update the terrain and clean up
		//

		if( UseEntireHeightmapCheck->IsChecked() )
			TI->Update(0.f);
		else
			TI->UpdateFromSelectedVertices();

		GEditor->RedrawLevel( GEditor->Level );

		unguard;
	}
};

// --------------------------------------------------------------
//
// WTerrainEditSheet
//
// --------------------------------------------------------------

class WTerrainEditSheet : public WWindow
{
	DECLARE_WINDOWCLASS(WTerrainEditSheet,WWindow,Window)

	WPropertySheet* PropSheet;
	WPageTools* ToolsPage;
	WPageMisc* MiscPage;

	// Structors.
	WTerrainEditSheet( FName InPersistentName, WWindow* InOwnerWindow )
	:	WWindow( InPersistentName, InOwnerWindow )
	{
		PropSheet = NULL;
	}

	// WTerrainEditSheet interface.
	void OpenWindow()
	{
		guard(WTerrainEditSheet::OpenWindow);
		MdiChild = 0;
		PerformCreateWindowEx
		(
			NULL,
			TEXT("Terrain Editing"),
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
			0, 0,
			0, 0,
			OwnerWindow ? OwnerWindow->hWnd : NULL,
			NULL,
			hInstance
		);
 
		unguard;
	}
	void OnCreate()
	{
		guard(WTerrainEditSheet::OnCreate);
		WWindow::OnCreate();

		// Create the sheet
		PropSheet = new WPropertySheet( this, IDPS_TERRAIN_EDIT );
		PropSheet->OpenWindow( 1, 0, 0 );

		// Create the pages for the sheet
		ToolsPage = new WPageTools( PropSheet->Tabs );
		ToolsPage->OpenWindow( IDPP_TE_TOOLS, GUnrealEdModule);
		PropSheet->AddPage( ToolsPage );

		MiscPage = new WPageMisc( PropSheet->Tabs );
		MiscPage->OpenWindow( IDPP_TE_MISC, GUnrealEdModule);
		PropSheet->AddPage( MiscPage );

		PropSheet->SetCurrent( 0 );

		// Resize the property sheet to surround the pages properly.
		RECT rect;
		::GetClientRect( ToolsPage->hWnd, &rect );
		::SetWindowPos( hWnd, HWND_TOP, 0, 0, rect.right + 32, rect.bottom + 64, SWP_NOMOVE );

		PositionChildControls();

		// Remove the "X" button.
		LONG Style = GetWindowLongA( hWnd, GWL_STYLE );
		Style &= ~WS_SYSMENU;
		SetWindowLongA( hWnd, GWL_STYLE, Style );

		unguard;
	}
	void OnDestroy()
	{
		guard(WTerrainEditSheet::OnDestroy);
		WWindow::OnDestroy();

		delete ToolsPage;
		delete MiscPage;
		delete PropSheet;
		unguard;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WTerrainEditSheet::OnSize);
		WWindow::OnSize(Flags, NewX, NewY);
		PositionChildControls();
		InvalidateRect( hWnd, NULL, FALSE );
		unguard;
	}
	void PositionChildControls()
	{
		guard(WTerrainEditSheet::PositionChildControls);

		if( !PropSheet
				|| !::IsWindow( PropSheet->hWnd ) )
			return;

		FRect CR = GetClientRect();
		::MoveWindow( PropSheet->hWnd, 0, 0, CR.Width(), CR.Height(), 1 );

		unguard;
	}
	INT OnSetCursor()
	{
		guard(WTerrainEditSheet::OnSetCursor);
		WWindow::OnSetCursor();
		SetCursor(LoadCursorIdX(NULL,IDC_ARROW));
		return 0;
		unguard;
	}
	void OnCommand( INT Command )
	{
		guard(WTerrainEditSheet::OnCommand);
		switch( Command )
		{
			case IDMN_TDL_TOGGLE_SHOW_ON_TERRAIN:
			{
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( TI && GTerrainTools.CurrentLayer >= 32 )
					TI->DecoLayers(GTerrainTools.CurrentLayer-32).ShowOnTerrain ^= 1;
				GEditor->RedrawLevel( GEditor->Level );
			}
			break;

			case IDMN_TDL_SET_DENSITY_FROM_CURRENT:
			{
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( TI && GTerrainTools.CurrentLayer >= 32 && GEditor->CurrentMaterial && GEditor->CurrentMaterial->IsA(UTexture::StaticClass()) )
				{
					TI->DecoLayers(GTerrainTools.CurrentLayer-32).DensityMap = Cast<UTexture>(GEditor->CurrentMaterial);
					GEditor->RedrawAllViewports(0);
				}
			}
			break;

			case IDMN_TDL_SET_COLOR_FROM_CURRENT:
			{
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( TI && GTerrainTools.CurrentLayer >= 32 && GEditor->CurrentMaterial && GEditor->CurrentMaterial->IsA(UTexture::StaticClass()) )
				{
					TI->DecoLayers(GTerrainTools.CurrentLayer-32).ColorMap = Cast<UTexture>(GEditor->CurrentMaterial);
					GEditor->RedrawAllViewports(0);
				}
			}
			break;

			case IDMN_TDL_SET_SCALE_FROM_CURRENT:
			{
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( TI && GTerrainTools.CurrentLayer >= 32 && GEditor->CurrentMaterial && GEditor->CurrentMaterial->IsA(UTexture::StaticClass()) )
				{
					TI->DecoLayers(GTerrainTools.CurrentLayer-32).ScaleMap = Cast<UTexture>(GEditor->CurrentMaterial);
					GEditor->RedrawAllViewports(0);
				}
			}
			break;

			case IDMN_TDL_SET_STATICMESH_FROM_CURRENT:
			{
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( TI && GTerrainTools.CurrentLayer >= 32 && GEditor->CurrentStaticMesh )
				{
					TI->DecoLayers(GTerrainTools.CurrentLayer-32).StaticMesh = GEditor->CurrentStaticMesh;
					GEditor->RedrawAllViewports(0);
				}
			}
			break;

			case IDMN_TL_TOGGLE_GRID:
			{
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( TI && GTerrainTools.CurrentLayer > -1 )
					TI->ShowGrid ^= (INT)appPow( 2, GTerrainTools.CurrentLayer-1 );
				GEditor->RedrawLevel( GEditor->Level );
			}
			break;

			case IDMN_TL_CONVERT_TO_16BIT:
			{
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( TI )
				{
					TI->ConvertHeightmapFormat();
					GEditor->RedrawAllViewports(0);
				}
			}
			break;

			case IDMN_TL_SET_TEXTURE_FROM_CURRENT:
			{
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( TI && GTerrainTools.CurrentLayer && GEditor->CurrentMaterial )
				{
					TI->Layers[GTerrainTools.CurrentLayer-1].Texture = GEditor->CurrentMaterial;

					TI->Update(0.f);
					GEditor->RedrawAllViewports(0);
				}
			}
			break;

			case IDMN_TL_SET_ALPHA_FROM_CURRENT:
			{
				ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
				if( TI && GTerrainTools.CurrentLayer && GEditor->CurrentMaterial && GEditor->CurrentMaterial->IsA(UTexture::StaticClass()) )
				{
					if( Cast<UTexture>(GEditor->CurrentMaterial)->Format != TEXF_RGBA8 )
						appMsgf(0, TEXT("Can't use texture '%s' as an alpha layer ... must be in RGBA8 format."), GEditor->CurrentMaterial->GetName() );
					else
					{
						TI->Layers[GTerrainTools.CurrentLayer-1].AlphaMap = Cast<UTexture>(GEditor->CurrentMaterial);

						TI->Update(0.f);
						GEditor->RedrawAllViewports(0);
					}
				}
			}
			break;

			default:
				WWindow::OnCommand(Command);
				break;
		}
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

