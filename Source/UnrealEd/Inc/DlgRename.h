/*=============================================================================
	DlgRename : Accepts input of a new name for something
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

class WDlgRename : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgRename,WDialog,UnrealEd)

	WButton OKButton, CancelButton;
	WEdit NameEdit, GroupEdit, PackageEdit;

	// Variables.
	FString OldName, NewName;
	FString OldGroup, NewGroup;
	FString OldPackage, NewPackage;

	// Constructor.
	WDlgRename( UObject* InContext, WWindow* InOwnerWindow )
	:	WDialog				( TEXT("Rename"), IDDIALOG_RENAME, InOwnerWindow )
	,	CancelButton		( this, IDCANCEL, FDelegate(this,(TDelegate)EndDialogFalse) )
	,	OKButton			( this, IDOK, FDelegate(this,(TDelegate)OnOK) )
	,	NameEdit			( this, IDEC_NAME )
	,	GroupEdit			( this, IDEC_NEWGROUP )
	,	PackageEdit			( this, IDEC_NEWPACKAGE )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgRename::OnInitDialog);
		WDialog::OnInitDialog();
		NameEdit.SetText( *OldName );
		GroupEdit.SetText( *OldGroup );
		PackageEdit.SetText( *OldPackage );

		unguard;
	}
	virtual INT DoModal( FString InOldName, FString InOldGroup, FString InOldPackage )
	{
		guard(WDlgNewClass::DoModal);

		NewName = OldName = InOldName;
		NewGroup = OldGroup = InOldGroup;
		NewPackage = OldPackage = InOldPackage;

		return WDialog::DoModal( hInstance );
		unguard;
	}

	void OnOK()
	{
		guard(WDlgRename::OnDestroy);
		NewName = NameEdit.GetText();
		NewGroup = GroupEdit.GetText();
		NewPackage = PackageEdit.GetText();
		EndDialogTrue();
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

