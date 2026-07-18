/*=============================================================================
	ScaleLights : Allows for the scaling of light values
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

class WDlgScaleLights : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgScaleLights,WDialog,UnrealEd)

	// Variables.
	WCheckBox LiteralCheck;
	WButton OKButton, CloseButton;
	WEdit ValueEdit;

	// Constructor.
	WDlgScaleLights( UObject* InContext, WWindow* InOwnerWindow )
	:	WDialog			( TEXT("Scale Lights"), IDDIALOG_SCALE_LIGHTS, InOwnerWindow )
	,	OKButton		( this, IDOK,			FDelegate(this,(TDelegate)OnOK) )
	,	CloseButton		( this, IDPB_CLOSE,		FDelegate(this,(TDelegate)OnClose) )
	,	LiteralCheck	( this, IDRB_LITERAL )
	,	ValueEdit		( this, IDEC_VALUE )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgScaleLights::OnInitDialog);
		WDialog::OnInitDialog();
		LiteralCheck.SetCheck( BST_CHECKED );
		unguard;
	}
	virtual void DoModeless( UBOOL bShow )
	{
		guard(WDlgScaleLights::DoModeless);
		_Windows.AddItem( this );
		hWnd = CreateDialogParamA( hInstance, MAKEINTRESOURCEA(IDDIALOG_SCALE_LIGHTS), OwnerWindow?OwnerWindow->hWnd:NULL, (DLGPROC)StaticDlgProc, (LPARAM)this);
		if( !hWnd )
			appGetLastError();
		Show( bShow );
		unguard;
	}
	bool OnClose() // gam
	{
		guard(WDlgScaleLights::OnClose);
		Show(0);
        return true; // gam
		unguard;
	}
	void OnOK()
	{
		guard(WDlgScaleLights::OnTagEditChange);
		UBOOL bLiteral = LiteralCheck.IsChecked();
		INT Value = appAtoi( *(ValueEdit.GetText()) );

		// Loop through all selected actors and scale their light value by the specified amount.
		for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
		{
			AActor* pActor = GUnrealEd->Level->Actors(i);
			if( pActor && pActor->bSelected )
			{
				FLOAT fLightBrighness = pActor->LightBrightness;
				if( bLiteral )
					fLightBrighness += Value;
				else
					fLightBrighness += fLightBrighness * (Value / 100.0f);

				pActor->LightBrightness = fLightBrighness;

				pActor->PostEditChange();
			}
		}

		GUnrealEd->NoteSelectionChange( GUnrealEd->Level );
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

