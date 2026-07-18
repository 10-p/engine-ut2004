/*=============================================================================
	NewMaterial : Properties of a brush builder
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

class WDlgNewMaterial : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgNewMaterial,WDialog,UnrealEd)

	// Variables.
	WButton NewButton, CancelButton;
	WEdit PackageEdit;
	WEdit GroupEdit;
	WEdit NameEdit;
	WComboBox FactoryCombo;
	WObjectProperties* pProps;
	UMaterialFactory* Factory;
	FString Package, Group, Name;
	FString defPackage, defGroup;
	FDelegate OnNewMaterial;
	TMap<FString,UClass*> ClassMap;

	// Constructor.
	WDlgNewMaterial( UObject* InContext, WWindow* InOwnerWindow, FDelegate InOnNewMaterial )
	:	WDialog		( TEXT("New Material"), IDDIALOG_NEW_MATERIAL, InOwnerWindow )
	,	NewButton	( this, IDPB_NEWMATERIAL, FDelegate(this,(TDelegate)OnNew) )
	,	CancelButton	( this, IDCANCEL,		FDelegate(this,(TDelegate)EndDialogFalse) )
	,	PackageEdit		( this, IDEC_PACKAGE )
	,	GroupEdit		( this, IDEC_GROUP )
	,	NameEdit		( this, IDEC_NAME )
	,	FactoryCombo	( this, IDCB_FACTORY )
	,	Factory			( NULL )
	,	pProps			( NULL )
	,	OnNewMaterial	( InOnNewMaterial )
	{
		FactoryCombo.SelectionChangeDelegate = FDelegate( this, (TDelegate)(OnFactoryChange) );
	}

	// WDialog interface.
	void OnInitDialog()
	{
		guard(WDlgNewMaterial::OnInitDialog);
		WDialog::OnInitDialog();

		PackageEdit.SetText( *defPackage );
		GroupEdit.SetText( *defGroup );
		::SetFocus( NameEdit.hWnd );
	
		for( TObjectIterator<UClass> ItC; ItC; ++ItC )
			if( ItC->IsChildOf(UMaterialFactory::StaticClass()) && !(ItC->ClassFlags&CLASS_Abstract) )
			{
				FString S = Cast<UMaterialFactory>(ItC->GetDefaultObject())->Description;
				ClassMap.Set( *S, *ItC );
				FactoryCombo.AddString( S.Len() ? *S : ItC->GetName() );
			}
		FactoryCombo.SetCurrent(0);
		OnFactoryChange();
		
		unguard;
	}
	void SetupPropertyList()
	{
		guard(WDlgNewMaterial::SetupPropertyList);

		if( pProps )
			delete pProps;
		pProps = NULL;

		if( Factory )
		{
			// This is a massive hack.  I blame Warren because it's copied from his Brush Builder code.
			pProps = new WObjectProperties( NAME_None, CPF_Edit, TEXT(""), this, 1 );
			pProps->OpenChildWindow( IDSC_PROPS );
			pProps->SetNotifyHook( GUnrealEd );


			pProps->ShowTreeLines = 1;
			pProps->Root.Sorted = 0;
			pProps->Root._Objects.AddItem( Factory );
			for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Factory->GetClass()); It; ++It )
				if( It->Category==FName(Factory->GetClass()->GetName()) && pProps->Root.AcceptFlags( It->PropertyFlags ) )
					pProps->Root.Children.AddItem( new(TEXT("FPropertyItem"))FPropertyItem( pProps, &(pProps->Root), *It, It->GetFName(), It->Offset, -1, 666 ) );
			pProps->Root.Expand();
			pProps->ResizeList();
			pProps->bAllowForceRefresh = 0;
		}
		unguard;
	}
	void OnDestroy()
	{
		guard(WDlgNewMaterial::OnDestroy);
		WDialog::OnDestroy();

		if( Factory )	delete Factory;
		if( pProps )	delete pProps;
	
		unguard;
	}
	void DoModeless( FString InDefPackage, FString InDefGroup)
	{
		defPackage = InDefPackage;
		defGroup = InDefGroup;

		guard(WDlgNewMaterial::DoModeless);
		_Windows.AddItem( this );
		hWnd = CreateDialogParamA( hInstance, MAKEINTRESOURCEA(IDDIALOG_NEW_MATERIAL), OwnerWindow?OwnerWindow->hWnd:NULL, (DLGPROC)StaticDlgProc, (LPARAM)this);
		if( !hWnd )
			appGetLastError();
		Show(1);
		unguard;
	}
	BOOL GetDataFromUser()
	{
		guard(WDlgNewTexture::GetDataFromUser);
		Package = PackageEdit.GetText();
		Group = GroupEdit.GetText();
		Name = NameEdit.GetText();

		if( !Package.Len() || !Name.Len() )
		{
			appMsgf( 0, TEXT("Invalid input.") );
			return FALSE;
		}

		return TRUE;
		unguard;
	}
	void OnNew()
	{
		guard(WDlgNewMaterial::OnBuild);

		if( Factory && GetDataFromUser() )
		{
			UPackage* Pkg = GUnrealEd->CreatePackage(NULL,*Package);
			if( Group.Len() )
				Pkg = GUnrealEd->CreatePackage(Pkg,*Group);

			// Force all controls to save their values before trying to build the brush.
			for( INT i=0; i<pProps->Root.Children.Num(); ++i )
				((FPropertyItem*)pProps->Root.Children(i))->SendToControl();

			UBOOL GIsSavedScriptableSaved = 1;
			Exchange(GIsScriptable,GIsSavedScriptableSaved);
			UMaterial* NewMaterial = Factory->eventCreateMaterial( Pkg, Package, Group, Name );
			Exchange(GIsScriptable,GIsSavedScriptableSaved);

			if(!NewMaterial )
				appMsgf( 0, TEXT("Unable to create material.") );
			else
			{
				GUnrealEd->CurrentMaterial = NewMaterial;
				OnNewMaterial();
				EndDialog(TRUE);
			}
		}

		unguard;
	}

	void OnFactoryChange()
	{
		guard(WDlgNewMaterial::OnFactoryChange);

		if( Factory )
			delete Factory;
		Factory = NULL;

		UClass** UMaterialFactoryClassPtr = ClassMap.Find(FactoryCombo.GetString(FactoryCombo.GetCurrent()));
		if( UMaterialFactoryClassPtr )
		{
			UClass* UMaterialFactoryClass = *UMaterialFactoryClassPtr;
			if( UMaterialFactoryClass )
			{
				LoadObject<UClass>( UMaterialFactoryClass->GetOuter(), UMaterialFactoryClass->GetName(), NULL, LOAD_Quiet|LOAD_NoWarn, NULL );
				Factory = ConstructObject<UMaterialFactory>(UMaterialFactoryClass);
				SetupPropertyList();	
			}
		}

		unguard;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

