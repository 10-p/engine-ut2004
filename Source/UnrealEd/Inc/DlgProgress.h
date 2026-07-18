/*=============================================================================
	Progress : Progress indicator
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:
	- cancel button needs to work

=============================================================================*/

class WDlgProgress : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgProgress,WDialog,UnrealEd)

	// Variables.

	// Constructor.
	WDlgProgress( UObject* InContext, WWindow* InOwnerWindow )
	:	WDialog				( TEXT("Progress"), IDDIALOG_PROGRESS, InOwnerWindow )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgProgress::OnInitDialog);
		WDialog::OnInitDialog();
		unguard;
	}
	virtual void DoModeless( UBOOL bShow )
	{
		guard(WDlgProgress::DoModeless);
		_Windows.AddItem( this );
		hWnd = CreateDialogParamA( hInstance, MAKEINTRESOURCEA(IDDIALOG_PROGRESS), OwnerWindow?OwnerWindow->hWnd:NULL, (DLGPROC)StaticDlgProc, (LPARAM)this);
		if( !hWnd )
			appGetLastError();
		Show( bShow );
		unguard;
	}
	void OnCancel()
	{
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

