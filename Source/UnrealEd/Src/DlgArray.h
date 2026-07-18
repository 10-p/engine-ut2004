/*=============================================================================
3DBuzz

Array : Duplicates a group of static meshes in the way specified in the
dialog box.

Revision history:
* Created by Joel VanEenwyk

=============================================================================*/

class WDlgArray : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgArray,WDialog,UnrealEd)

	// Variables.
	WButton OkButton;
	WButton CloseButton;
	WListBox ActorList;
	WEdit MX, MY, MZ;
	WEdit RX, RY, RZ;
	WEdit SX, SY, SZ;
	WEdit Duplicates;

	// Constructor.
	WDlgArray( UObject* InContext, WWindow* InOwnerWindow )
		:	WDialog			( TEXT("Search for Actors"), IDDIALOG_SEARCH, InOwnerWindow )
		,	CloseButton		( this, IDPB_CLOSE,		FDelegate(this,(TDelegate)OnClose) )
		,	OkButton		( this, IDPB_OK,		FDelegate(this,(TDelegate)OnOk) )
		,	MX		( this, IDEC_MX )
		,	MY		( this, IDEC_MY )
		,	MZ		( this, IDEC_MZ )
		,	RX		( this, IDEC_RX )
		,	RY		( this, IDEC_RY )
		,	RZ		( this, IDEC_RZ )
		,	SX		( this, IDEC_SX )
		,	SY		( this, IDEC_SY )
		,	SZ		( this, IDEC_SZ )
		,	Duplicates		( this, IDEC_DUPLICATES )
	{
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgArray::OnInitDialog);
		WDialog::OnInitDialog();
		unguard;
	}
	virtual void OnShowWindow( UBOOL bShow )
	{
		guard(WDlgArray::OnShowWindow);
		WWindow::OnShowWindow( bShow );
		MX.SetText(TEXT("0")); MY.SetText(TEXT("0")); MZ.SetText(TEXT("0"));
		RX.SetText(TEXT("0")); RY.SetText(TEXT("0")); RZ.SetText(TEXT("0"));
		SX.SetText(TEXT("1")); SY.SetText(TEXT("1")); SZ.SetText(TEXT("1"));
		Duplicates.SetText(TEXT("1"));
		unguard;
	}
	virtual void DoModeless( UBOOL bShow )
	{
		guard(WDlgArray::DoModeless);
		_Windows.AddItem( this );
		hWnd = CreateDialogParamA( hInstance, MAKEINTRESOURCEA(IDDIALOG_ARRAY), OwnerWindow?OwnerWindow->hWnd:NULL, (DLGPROC)StaticDlgProc, (LPARAM)this);
		if( !hWnd )
			appGetLastError();
		Show( bShow );
		unguard;
	}
	void DuplicateSelected( ULevel* Level )
	{
		guard(WDlgArray::DuplicateSelected);

		// Export the actors.
		FStringOutputDevice Ar;
		UExporter::ExportToOutputDevice( Level, NULL, Ar, TEXT("copy"), 0 );
		appClipboardCopy( *Ar );

		GEditor->SelectNone( GUnrealEd->Level, 1, 0 );

		// Get pasted text.
		FString PasteString = appClipboardPaste();
		const TCHAR* Paste = *PasteString;

		// Import the actors.
		Level->RememberActors();
		ULevelFactory* Factory = new ULevelFactory;
		Factory->FactoryCreateText( Level, ULevel::StaticClass(), Level->GetOuter(), Level->GetFName(), RF_Transactional, NULL, TEXT("paste"), Paste, Paste+appStrlen(Paste), GWarn );
		delete Factory;
		GCache.Flush();
		Level->ReconcileActors();

		unguard;
	}
	bool DuplicateActors()
	{
		guard(WDlgArray::DuplicateActors);

		GUnrealEd->Trans->Begin( TEXT("Duplicate Actors") );

		INT num = appAtoi( *(Duplicates.GetText()) );

		FVector moveOrg = FVector(appAtoi(*(MX.GetText())),appAtoi(*(MY.GetText())), appAtoi(*(MZ.GetText())));
		FRotator rotOrg = FRotator(appAtoi(*(RX.GetText()))*65535/360,appAtoi(*(RY.GetText()))*65535/360, appAtoi(*(RZ.GetText()))*65535/360);
		FVector scaleOrg = FVector(appAtof(*(SX.GetText())),appAtof(*(SY.GetText())), appAtof(*(SZ.GetText())));

		TArray<AActor*> SelectedActors;

		// get all currently selected objects
		for( INT i=0; i<GUnrealEd->Level->Actors.Num(); i++ )
			if( GUnrealEd->Level->Actors(i) && GUnrealEd->Level->Actors(i)->bSelected )
				SelectedActors.AddItem( GUnrealEd->Level->Actors(i) );

		GEditor->SelectNone( GUnrealEd->Level, 1, 0 );

		// duplicate the objects

		FVector move = moveOrg;
		FRotator rot = rotOrg;
		FVector scale = scaleOrg;

		for (INT dup = 0; dup < num; dup++)
		{
			GEditor->SelectNone( GUnrealEd->Level, 1, 0 );

			for( INT i = 0; i < SelectedActors.Num(); i++)
				SelectedActors(i)->bSelected = true;
			
			//GUnrealEd->Exec( TEXT("DUPLICATE") );
			DuplicateSelected( GUnrealEd->Level );

			GEdModeTools->MoveActors(NULL, GUnrealEd->Level, move, rot, false, 0, NULL);

			for( INT i=0; i<GUnrealEd->Level->Actors.Num(); i++ )
			{
				if( GUnrealEd->Level->Actors(i) && GUnrealEd->Level->Actors(i)->bSelected )
				{
					AActor *actor = GUnrealEd->Level->Actors(i);
					actor->DrawScale3D = scale;
				}
			}

			move += moveOrg;
			rot += rotOrg;
			scale *= scaleOrg;
		}

		GUnrealEd->Trans->End();

		GUnrealEd->RedrawLevel(GUnrealEd->Level);
		GUnrealEd->RedrawAllViewports(true);

		return true;

		unguard;
	}
	bool OnClose() // gam
	{
		guard(WDlgArray::OnClose);
		Show(0);
		return true; // gam
		unguard;
	}
	bool OnOk() // gam
	{
		guard(WDlgArray::OnOk);
		DuplicateActors();
		return true; // gam
		unguard;
	}
};

/*-----------------------------------------------------------------------------
The End.
-----------------------------------------------------------------------------*/

