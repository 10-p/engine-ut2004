/*=============================================================================
	Generic : A generic dialog box for accepting parameters for various tools.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

extern TMap<INT,UOptionsProxy*>* GOptionProxies;

class WDlgGeneric : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgGeneric,WDialog,UnrealEd)


	// Variables.
	WButton OKButton, CancelButton;

	WObjectProperties* PropertyWindow;
	WLabel CommentLabel;
	UOptionsProxy* Proxy;
	FString DlgCaption;

	// Constructor.
	WDlgGeneric( UObject* InContext, WWindow* InOwnerWindow, INT InProxyType, FString InDlgCaption, void* InData = NULL )
	:	WDialog			( TEXT("Generic"), IDDIALOG_GENERIC, InOwnerWindow )
	,	OKButton		( this, IDOK, FDelegate(this,(TDelegate)OnOK) )
	,	CancelButton	( this, IDCANCEL, FDelegate(this,(TDelegate)EndDialogFalse) )
	,	CommentLabel	( this, IDSC_COMMENT )
	{
		DlgCaption = InDlgCaption;
		PropertyWindow = new WObjectProperties( NAME_None, CPF_Edit, TEXT(""), this, 0 );
		PropertyWindow->ShowTreeLines = 0;
		Proxy = *GOptionProxies->Find( InProxyType );
		check(Proxy);
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgGeneric::OnInitDialog);
		WDialog::OnInitDialog();

		PropertyWindow->OpenChildWindow( IDSC_PROPS );
		PropertyWindow->Root.Sorted = 0;
		PropertyWindow->Root.SetObjects( (UObject**)&Proxy, 1 );

		SetText( *DlgCaption );

		unguard;
	}
	void OnDestroy()
	{
		guard(WDlgGeneric::OnDestroy);
		WDialog::OnDestroy();

		PropertyWindow->Root.SetObjects( NULL, 0 );
		delete PropertyWindow;

		unguard;
	}
	virtual INT DoModal( FString InComment )
	{
		guard(WDlgGeneric::DoModal);
		return WDialog::DoModal( hInstance );

		CommentLabel.SetText( *InComment );
		unguard;
	}
	void OnOK()
	{
		for( INT i=0; i<PropertyWindow->Root.Children.Num(); ++i )
			((FPropertyItem*)PropertyWindow->Root.Children(i))->SendToControl();

		PropertyWindow->Root.SetObjects( NULL, 0 );

		EndDialogTrue();
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

