/*=============================================================================
	AddSpecial : Add special brushes
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

class WDlgAddSpecial : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgAddSpecial,WDialog,UnrealEd)

	// Variables.
	WComboBox PrefabCombo;
	WButton OKButton;
	WButton CloseButton;

	// Constructor.
	WDlgAddSpecial( UObject* InContext, WWindow* InOwnerWindow )
	:	WDialog			( TEXT("Add Special"), IDDIALOG_ADD_SPECIAL, InOwnerWindow )
	,	PrefabCombo		( this, IDCB_PREFABS )
	,	OKButton		( this, IDOK, FDelegate(this,(TDelegate)OnOK) )
	,	CloseButton	( this, IDPB_CLOSE, FDelegate(this,(TDelegate)OnClose) )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgAddSpecial::OnInitDialog);
		WDialog::OnInitDialog();

		PrefabCombo.AddString(TEXT("Invisible Collision Hull"));
		PrefabCombo.AddString(TEXT("Zone Portal"));
		PrefabCombo.AddString(TEXT("Anti-Portal"));
		PrefabCombo.AddString(TEXT("Regular Brush"));
		PrefabCombo.AddString(TEXT("Semi Solid Brush"));
		PrefabCombo.AddString(TEXT("Non Solid Brush"));
		PrefabCombo.SelectionChangeDelegate = FDelegate(this, (TDelegate)OnComboPrefabsSelChange);

		PrefabCombo.SetCurrent( 3 );
		OnComboPrefabsSelChange();

		unguard;
	}
	virtual void DoModeless( UBOOL bShow )
	{
		guard(WDlgAddSpecial::DoModeless);
		_Windows.AddItem( this );
		hWnd = CreateDialogParamA( hInstance, MAKEINTRESOURCEA(IDDIALOG_ADD_SPECIAL), OwnerWindow?OwnerWindow->hWnd:NULL, (DLGPROC)StaticDlgProc, (LPARAM)this);
		if( !hWnd )
			appGetLastError();
		Show( bShow );
		unguard;
	}
	void OnOK()
	{
		guard(WDlgAddSpecial::OnCompileAll);

		INT Flags = 0;

		if( SendMessageX( ::GetDlgItem( hWnd, IDCK_ANTIPORTAL), BM_GETCHECK, 0, 0) == BST_CHECKED )		Flags |= PF_AntiPortal;
		if( SendMessageX( ::GetDlgItem( hWnd, IDCK_MIRROR), BM_GETCHECK, 0, 0) == BST_CHECKED )			Flags |= PF_Mirrored;
		if( SendMessageX( ::GetDlgItem( hWnd, IDCK_ZONE_PORTAL), BM_GETCHECK, 0, 0) == BST_CHECKED )	Flags |= PF_Portal;
		if( SendMessageX( ::GetDlgItem( hWnd, IDCK_INVIS), BM_GETCHECK, 0, 0) == BST_CHECKED )			Flags |= PF_Invisible;
		if( SendMessageX( ::GetDlgItem( hWnd, IDCK_TWO_SIDED), BM_GETCHECK, 0, 0) == BST_CHECKED )		Flags |= PF_TwoSided;
		if( SendMessageX( ::GetDlgItem( hWnd, IDRB_SEMI_SOLID), BM_GETCHECK, 0, 0) == BST_CHECKED )		Flags |= PF_Semisolid;
		if( SendMessageX( ::GetDlgItem( hWnd, IDRB_NON_SOLID), BM_GETCHECK, 0, 0) == BST_CHECKED )		Flags |= PF_NotSolid;

		GUnrealEd->Exec( *FString::Printf(TEXT("BRUSH ADD FLAGS=%d"), Flags));
		unguard;
	}
	bool OnClose() // gam
	{
		Show(0);
        return true; // gam
	}
	void OnComboPrefabsSelChange()
	{
		switch( PrefabCombo.GetCurrent() )
		{
			case 0:	// Invisible Collision Hull
				SendMessageX( ::GetDlgItem( hWnd, IDCK_ANTIPORTAL), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_MIRROR), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_ZONE_PORTAL), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_INVIS), BM_SETCHECK, BST_CHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_TWO_SIDED), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_SOLID), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_SEMI_SOLID), BM_SETCHECK, BST_CHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_NON_SOLID), BM_SETCHECK, BST_UNCHECKED, 0);
				break;

			case 1:	// Zone Portal
				SendMessageX( ::GetDlgItem( hWnd, IDCK_ANTIPORTAL), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_MIRROR), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_ZONE_PORTAL), BM_SETCHECK, BST_CHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_INVIS), BM_SETCHECK, BST_CHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_TWO_SIDED), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_SOLID), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_SEMI_SOLID), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_NON_SOLID), BM_SETCHECK, BST_CHECKED, 0);
				break;

			case 2:	// Anti Portal
				SendMessageX( ::GetDlgItem( hWnd, IDCK_ANTIPORTAL), BM_SETCHECK, BST_CHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_MIRROR), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_ZONE_PORTAL), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_INVIS), BM_SETCHECK, BST_CHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_TWO_SIDED), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_SOLID), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_SEMI_SOLID), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_NON_SOLID), BM_SETCHECK, BST_CHECKED, 0);
				break;

			case 3:	// Regular Brush
				SendMessageX( ::GetDlgItem( hWnd, IDCK_ANTIPORTAL), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_MIRROR), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_ZONE_PORTAL), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_INVIS), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_TWO_SIDED), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_SOLID), BM_SETCHECK, BST_CHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_SEMI_SOLID), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_NON_SOLID), BM_SETCHECK, BST_UNCHECKED, 0);
				break;

			case 4:	// Semisolid brush
				SendMessageX( ::GetDlgItem( hWnd, IDCK_ANTIPORTAL), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_MIRROR), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_ZONE_PORTAL), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_INVIS), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_TWO_SIDED), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_SOLID), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_SEMI_SOLID), BM_SETCHECK, BST_CHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_NON_SOLID), BM_SETCHECK, BST_UNCHECKED, 0);
				break;

			case 5:	// Nonsolid brush
				SendMessageX( ::GetDlgItem( hWnd, IDCK_ANTIPORTAL), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_MIRROR), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_ZONE_PORTAL), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_INVIS), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDCK_TWO_SIDED), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_SOLID), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_SEMI_SOLID), BM_SETCHECK, BST_UNCHECKED, 0);
				SendMessageX( ::GetDlgItem( hWnd, IDRB_NON_SOLID), BM_SETCHECK, BST_CHECKED, 0);
				break;
		}
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

