/*=============================================================================
	MapCheck : Displays errors/warnings/etc for the map.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

=============================================================================*/

struct {
	char* Text;
	INT Width;
} GCols[] =
{
	"Actor", 80,
	"Message", 500,
	NULL, -1,
};

class WDlgMapCheck : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgMapCheck,WDialog,UnrealEd)

	TMap<DWORD,FWindowAnchor> Anchors;
	FContainer *Container;
	
	HIMAGELIST himl;

	// Variables.
	WButton RefreshButton, CloseButton;
	WListView ItemList;

	// Constructor.
	WDlgMapCheck( UObject* InContext, WWindow* InOwnerWindow )
	:	WDialog				( TEXT("Check Map"), IDDIALOG_MAP_CHECK, InOwnerWindow )
	,	RefreshButton		( this, IDPB_REFRESH, FDelegate(this,(TDelegate)OnRefresh) )
	,	CloseButton			( this, IDPB_CLOSE, FDelegate(this,(TDelegate)OnClose) )
	,	ItemList			( this, IDLC_ITEMS )
	{
		Container = NULL;
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgMapCheck::OnInitDialog);
		WDialog::OnInitDialog();

		Container = new FContainer();

		// Set up the list view
		LVCOLUMNA lvcol;
		lvcol.mask = LVCF_TEXT | LVCF_WIDTH;

		for( INT x = 0 ; GCols[x].Text ; ++x )
		{
			lvcol.pszText = GCols[x].Text;
			lvcol.cx = GCols[x].Width;

			SendMessageX( ItemList.hWnd, LVM_INSERTCOLUMNA, x, (LPARAM)(const LPLVCOLUMNA)&lvcol );
		}

		himl = ImageList_LoadImage( hInstance, MAKEINTRESOURCE(IDBM_MAP_ERRORS), 16, 0, CLR_DEFAULT, IMAGE_BITMAP, LR_LOADMAP3DCOLORS );
		check(himl);
		ListView_SetImageList( ItemList.hWnd, himl, LVSIL_SMALL );

		ItemList.DblClkDelegate = FDelegate(this, (TDelegate)OnItemListDblClk);

		Anchors.Set( (DWORD)ItemList.hWnd,			FWindowAnchor( hWnd, ItemList.hWnd,			ANCHOR_TL, 4, 4,																	ANCHOR_BR, -4, -STANDARD_BUTTON_HEIGHT-8 ) );
		Anchors.Set( (DWORD)RefreshButton.hWnd,		FWindowAnchor( hWnd, RefreshButton.hWnd,
			ANCHOR_RIGHT|ANCHOR_BOTTOM, (-STANDARD_BUTTON_WIDTH-4)*2, -STANDARD_BUTTON_HEIGHT-4,
			ANCHOR_WIDTH|ANCHOR_HEIGHT, STANDARD_BUTTON_WIDTH, STANDARD_BUTTON_HEIGHT ) );
		Anchors.Set( (DWORD)CloseButton.hWnd,		FWindowAnchor( hWnd, CloseButton.hWnd,		ANCHOR_RIGHT|ANCHOR_BOTTOM, -STANDARD_BUTTON_WIDTH-4, -STANDARD_BUTTON_HEIGHT-4,	ANCHOR_WIDTH|ANCHOR_HEIGHT, STANDARD_BUTTON_WIDTH, STANDARD_BUTTON_HEIGHT ) );

		Container->SetAnchors( &Anchors );

		unguard;
	}
	virtual void DoModeless( UBOOL bShow )
	{
		guard(WDlgMapCheck::DoModeless);
		_Windows.AddItem( this );
		hWnd = CreateDialogParamA( hInstance, MAKEINTRESOURCEA(IDDIALOG_MAP_ERRORS), OwnerWindow?OwnerWindow->hWnd:NULL, (DLGPROC)StaticDlgProc, (LPARAM)this);
		if( !hWnd )
			appGetLastError();
		Show( bShow );
		unguard;
	}
	void OnDestroy()
	{
		WDialog::OnDestroy();
		delete Container;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		if( Container ) Container->RefreshControls();
		WDialog::OnSize( Flags, NewX, NewY );
	}
	void OnCommand( INT Command )
	{
		guard(WDlgMapCheck::OnCommand);

		switch( Command )
		{
			case WM_MC_SHOW:
				Show(1);
				break;

			case WM_MC_HIDE:
				Show(0);
				break;

			case WM_MC_SHOW_COND:
				if( SendMessageX( ItemList.hWnd, LVM_GETITEMCOUNT, 0, 0 ) )
					Show(1);
				break;

			case WM_MC_CLEAR:
				ItemList.Empty();
				break;

			case WM_MC_ADD:
				{
					MAPCHECK* MC = (MAPCHECK*)LastlParam;

					// Add the message to the window.
					LVITEMA lvi;
					::ZeroMemory( &lvi, sizeof(lvi));
					lvi.mask = LVIF_TEXT | LVIF_IMAGE;
					lvi.pszText = (char*)appToAnsi(MC->Actor->GetName());
					lvi.iItem = 0;
					lvi.iImage = MC->Type;

					INT idx = SendMessageX( ItemList.hWnd, LVM_INSERTITEMA, 0, (LPARAM)(const LPLVITEM)&lvi ); 
					if( idx > -1 )
					{
						::ZeroMemory( &lvi, sizeof(lvi));
						lvi.mask = LVIF_TEXT;
						lvi.pszText = (char*)appToAnsi( *MC->Message );
						lvi.iItem = idx;
						lvi.iSubItem = 1;
						SendMessageX( ItemList.hWnd, LVM_SETITEMA, 0, (LPARAM)(const LPLVITEM)&lvi );
					}
				}
				break;

			default:
				WWindow::OnCommand(Command);
				break;
		}

		unguard;
	}
	void OnRefresh()
	{
		guard(WDlgMapCheck::OnRefresh());
		GUnrealEd->Exec( TEXT("MAP CHECK") );
		unguard;
	}
	bool OnClose() // gam
	{
		guard(WDlgMapCheck::OnClose());
		Show(0);
        return true; // gam
		unguard;
	}
	void OnItemListDblClk()
	{
		guard(WDlgMapCheck::OnItemListDblClk());

		INT idx = ItemList.GetCurrent();
		if( idx == -1 ) return;	// Couldn't get valid selection

		char ActorName[80] = "";
		LVITEMA lvi;
		::ZeroMemory( &lvi, sizeof(lvi) );
		lvi.mask = LVIF_TEXT;
		lvi.cchTextMax = 80;
		lvi.pszText = ActorName;
		lvi.iItem = idx;

		SendMessageX( ItemList.hWnd, LVM_GETITEMA, 0, (LPARAM)(LPLVITEM)&lvi );

		GUnrealEd->Exec(TEXT("ACTOR SELECT NONE"));
		GUnrealEd->Exec(*FString::Printf(TEXT("CAMERA ALIGN NAME=%s"), ANSI_TO_TCHAR( ActorName ) ) );

		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
