/*=============================================================================
	LoadErrors : Displays the results from a map load that had problems.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

=============================================================================*/

class WDlgLoadErrors : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgLoadErrors,WDialog,UnrealEd)

	// Variables.
	WButton OKButton;
	WListBox PackageList, ResourceList;

	// Constructor.
	WDlgLoadErrors( UObject* InContext, WWindow* InOwnerWindow )
	:	WDialog			( TEXT("Load Errors"), IDDIALOG_LOAD_ERRORS, InOwnerWindow )
	,	OKButton		( this, IDOK, FDelegate(this,(TDelegate)EndDialogTrue) )
	,	PackageList		( this, IDLB_PACKAGES )
	,	ResourceList	( this, IDLB_RESOURCES )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgLoadErrors::OnInitDialog);
		WDialog::OnInitDialog();

		unguard;
	}
	virtual void DoModeless( UBOOL bShow )
	{
		guard(WDlgLoadErrors::DoModeless);
		_Windows.AddItem( this );
		hWnd = CreateDialogParamA( hInstance, MAKEINTRESOURCEA(IDDIALOG_LOAD_ERRORS), OwnerWindow?OwnerWindow->hWnd:NULL, (DLGPROC)StaticDlgProc, (LPARAM)this);
		if( !hWnd )
			appGetLastError();
		Show( bShow );
		unguard;
	}
	bool OnClose() // gam
	{
		guard(WDlgLoadErrors::OnClose);
		Show(0);
        return true; // gam
		unguard;
	}
	void Refresh()
	{
		guard(WDlgLoadErrors::Refresh);

		PackageList.Empty();
		ResourceList.Empty();

		for( INT x = 0 ; x < GEdLoadErrors.Num() ; ++x )
		{
			if( GEdLoadErrors(x).Type == FEdLoadError::TYPE_FILE )
				PackageList.AddString( *GEdLoadErrors(x).Desc );
			else
				ResourceList.AddString( *GEdLoadErrors(x).Desc );
		}
	
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
