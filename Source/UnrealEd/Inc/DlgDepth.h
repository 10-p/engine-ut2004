/*=============================================================================
	DlgDepth : Accepts the depth value for a brush extrusion
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

class WDlgDepth : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgDepth,WDialog,UnrealEd)

	WButton OKButton, CancelButton;
	WEdit DepthEdit;

	// Variables.
	INT Depth;

	// Constructor.
	WDlgDepth( UObject* InContext, WWindow* InOwnerWindow )
	:	WDialog				( TEXT("Depth"), IDDIALOG_Depth, InOwnerWindow )
	,	CancelButton		( this, IDCANCEL, FDelegate(this,(TDelegate)EndDialogFalse) )
	,	OKButton			( this, IDOK, FDelegate(this,(TDelegate)OnOK) )
	,	DepthEdit			( this, IDEC_DEPTH )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgDepth::OnInitDialog);
		WDialog::OnInitDialog();
		DepthEdit.SetText( TEXT("256") );
		unguard;
	}
	virtual INT DoModal()
	{
		guard(WDlgNewClass::DoModal);
		return WDialog::DoModal( hInstance );
		unguard;
	}

	void OnOK()
	{
		guard(WDlgDepth::OnDestroy);
		Depth = appAtoi( *DepthEdit.GetText() );
		EndDialogTrue();
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

