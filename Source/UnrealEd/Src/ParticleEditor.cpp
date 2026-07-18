//=============================================================================
//
// ParticleEditor.cpp	
// Copyright(c) 2002 Demiurge Studios. All Rights Reserved.
// 
//=============================================================================

#include "UnrealEd.h"

// Buttons for the editor
TBBUTTON tbPEButtons[] = 
{
	{ 0, IDMN_PE_Refresh, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0 }
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 1, IDMN_PE_NewEmitter, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 2, IDMN_PE_EmitterWizard, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 3, IDMN_PE_DeleteEmitter, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 4, IDMN_PE_SaveEmitter, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 5, IDMN_PE_LoadEmitter, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 6, IDMN_PE_DuplicateEmitter, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
	, { 0, 0, TBSTATE_ENABLED, TBSTYLE_SEP, 0L, 0}
	, { 7, IDMN_PE_ExportScript, TBSTATE_ENABLED, TBSTYLE_BUTTON, 0L, 0}
};

struct 
{
	TCHAR ToolTip[64];
	INT ID;
} ToolTips_PE[] = 
{
	TEXT("Refresh"), IDMN_PE_Refresh,
	TEXT("Add New Emitter"), IDMN_PE_NewEmitter,
	TEXT("New Emitter Wizard"), IDMN_PE_EmitterWizard,
	TEXT("Delete Emitter"), IDMN_PE_DeleteEmitter,
	TEXT("Save Emitter"), IDMN_PE_SaveEmitter,	
	TEXT("Load Emitter"), IDMN_PE_LoadEmitter,
	TEXT("Duplicate Emitter"), IDMN_PE_DuplicateEmitter,
	TEXT("Export to Script"), IDMN_PE_ExportScript,
	NULL, 0
};



WParticleEditor::WParticleEditor(FName InPersistentName, WWindow* InOwnerWindow) :
	WWindow(InPersistentName, InOwnerWindow), 
	EditTarget(NULL), CategoryList(NULL), Container(NULL), EmitterTabs(NULL), EditTargetWhenShown(NULL), LastTab(NULL),
	ComponentsOpened(FALSE), ComponentsReadyToShow(TRUE), ToolTipCtrl(NULL)
{
	UDNHelpTopic = 2000;

	UParticleEmitter* EditTarget = NULL;
	PSEComponents.AddItem( new WParticleGeneralComponent	(NULL, NULL, EditTarget, FString(TEXT("General")) ) );
	PSEComponents.AddItem( new WParticleTextureComponent	(NULL, NULL, EditTarget, FString(TEXT("Texture")) ) );
	PSEComponents.AddItem( new WParticleMeshTextureComponent	(NULL, NULL, EditTarget, FString(TEXT("Texture ")) ) ); //EXTRA SPACE " " on end
	PSEComponents.AddItem( new WParticleFadingColorComponent(NULL, NULL, EditTarget, FString(TEXT("Color/Fading")) ) );
	PSEComponents.AddItem( new WParticleRenderingComponent	(NULL, NULL, EditTarget, FString(TEXT("Rendering"))  ) );
	PSEComponents.AddItem( new WParticleTimeComponent		(NULL, NULL, EditTarget, FString(TEXT("Time"))  ) );
	PSEComponents.AddItem( new WParticleLocationComponent	(NULL, NULL, EditTarget, FString(TEXT("Location")) ) );
	PSEComponents.AddItem( new WParticleMovementComponent	(NULL, NULL, EditTarget, FString(TEXT("Movement")) ) );
	PSEComponents.AddItem( new WParticleBasicRotationComponent	(NULL, NULL, EditTarget, FString(TEXT("Rotation"))  ) );
	PSEComponents.AddItem( new WParticleSpriteRotationComponent	(NULL, NULL, EditTarget, FString(TEXT("Rotation "))  ) ); //SPRITE ROTATION HAS " " on end!!!!
	PSEComponents.AddItem( new WParticleMeshRotationComponent	(NULL, NULL, EditTarget, FString(TEXT("Rotation  "))  ) ); //MESH ROTATION HAS "  " on end!!!!
	PSEComponents.AddItem( new WParticleSizeComponent		(NULL, NULL, EditTarget, FString(TEXT("Size")) ) );
	PSEComponents.AddItem( new WParticleCollisionComponent	(NULL, NULL, EditTarget, FString(TEXT("Collision")) ) );
	PSEComponents.AddItem( new WParticleBeamComponent		(NULL, NULL, EditTarget, FString(TEXT("Beam")) ) );
	PSEComponents.AddItem( new WParticleBeamNoiseComponent	(NULL, NULL, EditTarget, FString(TEXT("Beam Noise")) ) );
	PSEComponents.AddItem( new WParticleBeamBranchingComponent(NULL, NULL, EditTarget, FString(TEXT("Beam Branching")) ) );
	PSEComponents.AddItem( new WParticleMeshComponent		(NULL, NULL, EditTarget, FString(TEXT("Mesh")) ) );
	PSEComponents.AddItem( new WParticleSparkComponent		(NULL, NULL, EditTarget, FString(TEXT("Spark")) ) );
}


void WParticleEditor::OnDestroy()
{

	WWindow::OnDestroy();

	if(Container)
	{
		delete Container;
		Container=NULL;
	}

	if(CategoryList)
	{
		delete CategoryList;
		CategoryList = NULL;
	}

	if(EmitterTabs)
	{
		for(INT i = 0; i < EmitterTabs->Pages.Num(); i++)
		{
			delete EmitterTabs->Pages(i);
		}

		delete EmitterTabs;
		EmitterTabs = NULL;
	}

	for(INT i = 0; i < PSEComponents.Num(); i++)
	{
		delete PSEComponents(i);
	}
	PSEComponents.Empty();

	if(ToolTipCtrl)
	{
		delete ToolTipCtrl;
		ToolTipCtrl = NULL;
	}
}



void WParticleEditor::InitialOpenComponents(WParticleEditorTab *ParentTab)
{
	GWarn->BeginSlowTask( TEXT("Initializing Particle System Editor"), 1);

	for(INT i = 0; i < PSEComponents.Num(); i++)
	{
		GWarn->StatusUpdatef( i, PSEComponents.Num(), TEXT("Initializing Particle System Editor") );
		PSEComponents(i)->SetParentTab(ParentTab);
		PSEComponents(i)->OpenWindow(0, 0);
	}
	ComponentsOpened = TRUE;

	GWarn->EndSlowTask();
}



WParticleEditorComponent* WParticleEditor::GetComponent(FString ComponentName)
{
	for(INT i = 0; i < PSEComponents.Num(); i++)
	{
		if(PSEComponents(i)->GetComponentName() == ComponentName)
		{
			return PSEComponents(i);
		}
	}
	return NULL;
}



void WParticleEditor::OnShowWindow( UBOOL bShow )
{
	if(bShow && EditTargetWhenShown)
	{
		if(EditTargetWhenShown != EditTarget)
		{
			SetEditTarget(EditTargetWhenShown);
		}
		EditTargetWhenShown = NULL;
	}
}


void WParticleEditor::NotifySelectionChange()
{
	AEmitter* NewEditTarget = NULL;
	for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; ++i )
	{
		AActor* Actor = GUnrealEd->Level->Actors(i);
		if( Actor && Actor->bSelected && Actor->IsA(AEmitter::StaticClass()))
		{
			NewEditTarget = (AEmitter*)Actor;
			break;
		}
	}

	// don't do this until we're Shown to avoid lag every click of a PS.
	if(bShow)
	{
		// if we didn't just select a particle system...don't de-select the old one.
		if(NewEditTarget && NewEditTarget != EditTarget)
		{
			SetEditTarget(NewEditTarget);
			EditTargetWhenShown = NULL;
		}
	}
	else
	{
		EditTargetWhenShown = NewEditTarget;
	}
}


INT WParticleEditor::OnSysCommand( INT Command )
{
	if( Command == SC_CLOSE )
	{
		Show(0);
		return 1;
	}

	return 0;

}

void WParticleEditor::OnCommand( INT Command )
{
	switch(Command)
	{
	// new Emitter button
	case IDMN_PE_NewEmitter:
		{
			// create a new particle Emitter dlg
			if(!EditTarget)
			{
				appMsgf( 0, TEXT("Select a particle system first.") );
			}
			else
			{
				WParticleEditorNewEmitterDlg dlg( this, EditTarget);
				dlg.DoModal();
			}
		}
		break;
	// delete Emitter button
	case IDMN_PE_DeleteEmitter:
		{
			if(!EditTarget)
			{
				appMsgf( 0, TEXT("Select a particle system first.") );
			}
			else if(appMsgf(1, TEXT("Delete the currently selected emitter?")))
			{
				WParticleEditorTab* toBeRemoved = (WParticleEditorTab*)EmitterTabs->Pages((EmitterTabs->Tabs->GetCurrent()));
				// get the particle Emitter from the tab and remove it from the Emitter
				EditTarget->Emitters.RemoveItem(toBeRemoved->GetEditTarget());
				// refresh the Emitter list
				UpdateParticleEmitterTabs();
			}
		}
		break;
	case IDMN_PE_Refresh:
		{
			if(EditTarget)
			{
				for(INT i = 0; i < EditTarget->Emitters.Num(); i++)
				{
					EditTarget->Emitters(i)->PostEditChange();
				}
			}
			else
			{
				appMsgf( 0, TEXT("Select a particle system first.") );
			}
		}
		break;
	// Duplicate emitter
	case IDMN_PE_DuplicateEmitter:
		{
			if(EditTarget && EditTarget->Emitters.Num() > 0)
			{
				WParticleEditorTab* toDuplicateTab = (WParticleEditorTab*)EmitterTabs->Pages((EmitterTabs->Tabs->GetCurrent()));
				DuplicateEmitter(toDuplicateTab->GetEditTarget());
			}
			else
			{
				if(!EditTarget)
				{
					appMsgf( 0, TEXT("Select a particle system first.") );
				}
				else if(EditTarget->Emitters.Num() < 0)
				{
					appMsgf( 0, TEXT("Select a particle system with at least one emitter.") );
				}
			}
		}
		break;
	case IDMN_PE_SaveEmitter:
		{
			if(EditTarget && EditTarget->Emitters.Num() > 0)
			{
				WParticleEditorTab* toSaveTab = (WParticleEditorTab*)EmitterTabs->Pages((EmitterTabs->Tabs->GetCurrent()));
				SaveEmitter(toSaveTab->GetEditTarget());
			}
			else
			{
				if(!EditTarget)
				{
					appMsgf( 0, TEXT("Select a particle system first.") );
				}
				else if(EditTarget->Emitters.Num() < 0)
				{
					appMsgf( 0, TEXT("Select a particle system with at least one emitter.") );
				}
			}
		}
		break;
	case IDMN_PE_LoadEmitter:
		{
			if(EditTarget)
			{
				LoadEmitter();
			}
			else
			{
				appMsgf( 0, TEXT("Select a particle system first.") );
			}
		}
		break;
	case IDMN_PE_EmitterWizard:
		{
			if(EditTarget)
			{
				WEmitterWizard dlg(this, EditTarget);
				if(dlg.DoModal())
				{
					UpdateParticleEmitterTabs();
					SelectParticleEmitterTab(dlg.GetNewEmitter());
				}
			}
			else
			{
				appMsgf( 0, TEXT("Select a particle system first.") );
			}
		}
		break;
	case IDMN_PE_ExportScript:
		{
			if(EditTarget)
			{
				WParticleEditorExportDlg dlg( this, EditTarget);
				dlg.DoModal();
			}
			else
			{
				appMsgf( 0, TEXT("Select a particle system first.") );
			}
		}
		break;
	default:
		WWindow::OnCommand(Command);
	}
}



void WParticleEditor::SaveEmitter(UParticleEmitter* Emitter)
{

	// Mostly ripped from FileSaveAs in Main.cpp. No need to re-invent the wheel. :-)
	OPENFILENAMEA ofn;
	char File[8192], *pFilename;
	//TCHAR l_chCmd[512];

	pFilename = TCHAR_TO_ANSI( *Emitter->Name );
	strcpy( File, pFilename );

	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = File;
	ofn.nMaxFile = sizeof(char) * 8192;
	ofn.lpstrFilter = "Emitters (*.emt)\0*.emt\0All Files\0*.*\0\0";;
	ofn.lpstrInitialDir = "..\\";
	ofn.lpstrDefExt = "emt";
	ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT; 

	// Display the Open dialog box. 
	if( GetSaveFileNameA(&ofn) )
	{
		FStringOutputDevice Ar;
		UExporter::ExportToOutputDevice( Emitter, ConstructObject<UExporter>( UObjectExporterT3D::StaticClass() ), Ar, TEXT("copy"), 0 );
		
		// save the file
		appSaveStringToFile(*Ar, ANSI_TO_TCHAR(ofn.lpstrFile));		
	}
}



UParticleEmitter* WParticleEditor::LoadEmitterFromFile(UClass* EmitterClass, FStringOutputDevice data)
{
	UParticleEmitter* ret = NULL;
	ret = ConstructObject<UParticleEmitter>( EmitterClass, EditTarget, NAME_None );
	ImportProperties( EmitterClass, (BYTE*)ret, GUnrealEd->Level, *data, EditTarget, GWarn, 0 );

	return ret;
}



void WParticleEditor::LoadEmitter()
{
	OPENFILENAMEA ofn;

	char File[8192] = "\0";

	ZeroMemory(&ofn, sizeof(OPENFILENAMEA));
	ofn.lStructSize = sizeof(OPENFILENAMEA);
	ofn.hwndOwner = hWnd;
	ofn.lpstrFile = File;
	ofn.nMaxFile = sizeof(char) * 8192;
	ofn.lpstrFilter = "Emitters (*.emt)\0*.emt\0All Files\0*.*\0\0";;
	ofn.lpstrInitialDir = "..\\"; 
	ofn.lpstrDefExt = "emt";
	ofn.Flags = OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_EXPLORER; 

	// Display the Open dialog box. 
	if( GetOpenFileNameA(&ofn) )
	{			

		FArchive* FileArchive = GFileManager->CreateFileReader(ANSI_TO_TCHAR(File));
		FStringOutputDevice Data;
		*FileArchive << Data;

		FString ClassName;
		UParticleEmitter* NewEmitter = NULL;
		if(Parse(*Data, TEXT("Object Class="), ClassName))
		{
			// Load the class of the object using the name in the T3D file
			UClass* EmitterClass = Cast<UClass>(UObject::StaticLoadObject(UClass::StaticClass(), NULL, *(FString(TEXT("Engine.")) + ClassName), NULL, LOAD_NoWarn, NULL));
			// use that class to load the emitter from the file. Appent "Engine." so we have the complete class name
			NewEmitter = LoadEmitterFromFile(EmitterClass, Data);
		}
				
		check(NewEmitter);
		// Add the new Emitter to the particle system
		EditTarget->Emitters.AddItem(NewEmitter);
		// set the Name of the new Emitter
		NewEmitter->Name = NewEmitter->GetName();
		// refresh the Emitter's tabs
		UpdateParticleEmitterTabs();
		SelectParticleEmitterTab(NewEmitter);

		delete FileArchive;

	}
}


void WParticleEditor::DuplicateEmitter(const UParticleEmitter* TemplateEmitter)
{
	UParticleEmitter* NewPE = (UParticleEmitter*)(UObject::StaticConstructObject( TemplateEmitter->GetClass(), EditTarget, NAME_None, 0, const_cast<UParticleEmitter*>(TemplateEmitter)));

	// Add the new Emitter to the particle system
	EditTarget->Emitters.AddItem(NewPE);
	// set the Name of the new Emitter
	NewPE->Name = NewPE->GetName();
	// refresh the Emitter's tabs
	UpdateParticleEmitterTabs();
	SelectParticleEmitterTab(NewPE);
}


void WParticleEditor::OpenWindow()
{

	MdiChild = 0;

	PerformCreateWindowEx
	(
		0,
		TEXT("Particle System Editor"),
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		50,
		50,
		600,
		600,
		OwnerWindow->hWnd,
		NULL,
		hInstance
	);
	
	NotifySelectionChange();
}



void WParticleEditor::OnCreate()
{
	WWindow::OnCreate();

	// Create the toolbar
	ToolBar = CreateToolbarEx( 
		hWnd, WS_CHILD | WS_BORDER | WS_VISIBLE | CCS_ADJUSTABLE,
		IDB_ParticleEditor_TOOLBAR,
		8,
		hInstance,
		IDB_ParticleEditor_TOOLBAR,
		(LPCTBBUTTON)&tbPEButtons,
		11,
		16,16,
		16,16,
		sizeof(TBBUTTON));

	/*
	// initialize the left-side list of categories
	CategoryList = new WCheckListBox( this, IDLB_PARTICLE_EDITOR_CATS );
	CategoryList->OpenWindow( 1, 0, 1, 1 );
	*/

	// construct the Emitter tabs container
	EmitterTabs = new WPropertySheet( this, IDPS_PE_EMITTERS );
	// open the window visible, resizeable and with nothing extra
	EmitterTabs->OpenWindow(1, 1, 0);

	// Set the positions of the various controls
	// Position toolbar
	INT Top = 0;
	Anchors.Set( (DWORD)ToolBar, FWindowAnchor(hWnd, ToolBar, ANCHOR_TL, 0, 0, ANCHOR_RIGHT|ANCHOR_HEIGHT, 0, STANDARD_TOOLBAR_HEIGHT));

	// Position the Emitter tabs
	Top += (STANDARD_TOOLBAR_HEIGHT);
	Anchors.Set((DWORD)EmitterTabs->hWnd, FWindowAnchor(hWnd, EmitterTabs->hWnd, ANCHOR_TL, 0, Top, ANCHOR_BOTTOM | ANCHOR_RIGHT, 0, 0) );

	Container = new FContainer();
	Container->SetAnchors( &Anchors );

	PositionChildControls();

	ToolTipCtrl = new WToolTip(this);
	ToolTipCtrl->OpenWindow();
	// Ripped from the Browsers...it's already copy/pasted all over the place so this isn't really a new terriblness, it just expands an existing one. :-/
	for( INT tooltip = 0 ; ToolTips_PE[tooltip].ID > 0 ; ++tooltip )
	{
		// Figure out the rectangle for the toolbar button.
		INT index = SendMessageX( ToolBar, TB_COMMANDTOINDEX, ToolTips_PE[tooltip].ID, 0 );
		RECT rect;
		SendMessageX( ToolBar, TB_GETITEMRECT, index, (LPARAM)&rect);

		ToolTipCtrl->AddTool( ToolBar, ToolTips_PE[tooltip].ToolTip, tooltip, &rect );
	}
}



void WParticleEditor::PositionChildControls()
{
	if( Container ) 
		Container->RefreshControls();
}


void WParticleEditor::UpdateParticleEmitterTabs()
{

	// clear out the curret set of tabs
	check(EmitterTabs);

	//set the last tab to NULL so don't go crazy switching when it doesn't matter
	LastTab = NULL;


	//UnparentAllComponents
	for(INT i = 0; i < PSEComponents.Num(); i++)
		PSEComponents(i)->SetParentTab(NULL);

	// delete the old tabs
	for(INT i = 0; i < EmitterTabs->Pages.Num(); i++)
	{
		delete EmitterTabs->Pages(i);
	}
	EmitterTabs->Empty();
	

	if(!EditTarget)
	{
		InvalidateRect(hWnd, NULL, TRUE);
		return;
	}

	// for each ParticleEmitter in the new Emitter
	ComponentsReadyToShow = FALSE;
	EmitterTabs->Show(0);

	//scion jg -- Don't crash iterating null emitters
	for (INT Index = 0; Index < EditTarget->Emitters.Num(); Index++)
	{
		if (EditTarget->Emitters(Index) != NULL)
		{
				WParticleEditorTab* newPage = NULL;
				// Get a pointer to the emitter for reuse
				UParticleEmitter* Emitter = EditTarget->Emitters(Index);

				UClass* EmitterClass = Emitter->GetClass();
			FString tabName;

			// This Name is AParticleEmitter::Name. If this system doesn't have name
				if(Emitter->Name == TEXT(""))
			{
				// Give it a default name
					Emitter->Name = Emitter->GetName();
			}
				tabName = Emitter->Name;

			if(EmitterClass->IsChildOf(UBeamEmitter::StaticClass()) )
			{
					newPage = new WParticleEditorBeamTab( EmitterTabs->Tabs, this, *tabName, Emitter, EditTarget );
			}
			else if(EmitterClass->IsChildOf(USpriteEmitter::StaticClass()) )
			{
					newPage = new WParticleEditorSpriteTab( EmitterTabs->Tabs, this, *tabName, Emitter, EditTarget );
			}
			else if(EmitterClass->IsChildOf(UMeshEmitter::StaticClass()) )
			{
					newPage = new WParticleEditorMeshTab( EmitterTabs->Tabs, this, *tabName, Emitter, EditTarget );
			}
			else if(EmitterClass->IsChildOf(USparkEmitter::StaticClass()) )
			{
					newPage = new WParticleEditorSparkTab( EmitterTabs->Tabs, this, *tabName, Emitter, EditTarget );
			}
			else
			{	
				debugf(TEXT("Unknown Emitter class %s"), EmitterClass->GetName());
				newPage = NULL;
			}	
			if ( newPage )
			{
				newPage->OpenWindow( 0, 0 );
				EmitterTabs->AddPage( newPage );
			}
		}
		else
		{
			// Skip this emitter
			EditTarget->Emitters.Remove(Index);
			Index--;
		}
	}
	EmitterTabs->Show(1);
	ComponentsReadyToShow = TRUE;

	// Open the first tab for viewing/editing
	if(EmitterTabs->Tabs->GetCount() > 0)
	{
		LastTab = (WParticleEditorTab*) (EmitterTabs->Pages(0));
		EmitterTabs->SetCurrent( 0 );
		EmitterTabs->RefreshPages();
		EmitterTabs->Pages(0)->Show(TRUE);
		EmitterTabs->Pages(0)->OnShowWindow(TRUE);
	}
}

void WParticleEditor::SelectParticleEmitterTab(UParticleEmitter* upe) {

	int ct = EmitterTabs->Tabs->GetCount();
	if ( ct <= 0 ) 
		return;

	for ( int i = 0; i < ct; i++ ) {
		if ( ((WParticleEditorTab*)EmitterTabs->Pages(i))->GetEditTarget() == upe )	{
			EmitterTabs->SetCurrent( i );
			EmitterTabs->RefreshPages();
			EmitterTabs->Pages(i)->Show(TRUE);
			EmitterTabs->Pages(i)->OnShowWindow(TRUE);
			return;
		}
	}
}

void WParticleEditor::SetEditTarget(AEmitter* InEmitter)
{
	EditTarget = InEmitter;

	UpdateParticleEmitterTabs();
}



void WParticleEditor::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	WWindow::OnSize(Flags, NewX, NewY);
	PositionChildControls();
}




//=============================================================================
// WParticleEditorNewEmitterDlg 

// Constructor.
WParticleEditorNewEmitterDlg::WParticleEditorNewEmitterDlg( WWindow* InOwnerWindow, AEmitter* InEditTarget)
:		WDialog			( TEXT("Add Emitter"), IDPS_ADD_EMITTER, InOwnerWindow )
	,	OkButton		( this, IDOK,			FDelegate(this,(TDelegate)OnOk) )
	,	CancelButton	( this, IDCANCEL,		FDelegate(this,(TDelegate)EndDialogFalse) )
	,	NameEdit		( this, IDC_Name )
	,	EmitterType		( this, IDPS_PICK_CLASS )
	,	EditTarget(InEditTarget)
{
}


// WDialog interface.
void WParticleEditorNewEmitterDlg::OnInitDialog()
{
	WDialog::OnInitDialog();
	EmitterType.AddString(TEXT("Beam Emitter"));
	EmitterType.AddString(TEXT("Mesh Emitter"));
	EmitterType.AddString(TEXT("Sprite Emitter"));
	EmitterType.AddString(TEXT("Spark Emitter"));
	EmitterType.SetCurrent(3);
}



INT WParticleEditorNewEmitterDlg::DoModal()
{
	return WDialog::DoModal( hInstance );
}



void WParticleEditorNewEmitterDlg::OnOk()
{


	UParticleEmitter* NewPE;

	if(EmitterType.GetString(EmitterType.GetCurrent()) == FString(TEXT("Beam Emitter")))
	{
		// Make a new bream Emitter
		NewPE = (UParticleEmitter*)UObject::StaticConstructObject(UBeamEmitter::StaticClass(), EditTarget);
	}
	else if(EmitterType.GetString(EmitterType.GetCurrent()) == FString(TEXT("Sprite Emitter")))
	{
		// Make a new sprite Emitter
		NewPE = (UParticleEmitter*)UObject::StaticConstructObject(USpriteEmitter::StaticClass(), EditTarget);
	}
	else if(EmitterType.GetString(EmitterType.GetCurrent()) == FString(TEXT("Mesh Emitter")))
	{
		// Make a new sprite Emitter
		NewPE = (UParticleEmitter*)UObject::StaticConstructObject(UMeshEmitter::StaticClass(), EditTarget);
	}
	else if(EmitterType.GetString(EmitterType.GetCurrent()) == FString(TEXT("Spark Emitter")))
	{
		// Make a new sprite Emitter
		NewPE = (UParticleEmitter*)UObject::StaticConstructObject(USparkEmitter::StaticClass(), EditTarget);
	}
	else
	{
		check(0); // Bad Emitter type. Missing a clause in the above code block
	}

	// Add the new Emitter to the particle system
	EditTarget->Emitters.AddItem(NewPE);
	// set the Name of the new Emitter
	NewPE->Name = NameEdit.GetText();
	// refresh the Emitter's tabs
	((WParticleEditor*)OwnerWindow)->UpdateParticleEmitterTabs();
	((WParticleEditor*)OwnerWindow)->SelectParticleEmitterTab(NewPE);

	// close the dialgue
	EndDialog(1);
}


//=============================================================================
// WParticleEditorExportDlg 

// Constructor.
WParticleEditorExportDlg::WParticleEditorExportDlg( WWindow* InOwnerWindow, AEmitter* InEditTarget)
:		WDialog			( TEXT("Export Particle System to Script"), IDDIALOG_PE_EXPORT, InOwnerWindow )
	,	OkButton		( this, IDOK,			FDelegate(this,(TDelegate)OnOk) )
	,	CancelButton	( this, IDCANCEL,		FDelegate(this,(TDelegate)EndDialogFalse) )
	,	ClassEdit		( this, IDEC_CLASSNAME )
	,	PackageEdit		( this, IDEC_PACKAGENAME )
	,	AutoDestroyCheck( this, IDC_AUTODESTROY )
	,	EditTarget(InEditTarget)
{

}



// WDialog interface.
void WParticleEditorExportDlg::OnInitDialog()
{
	WDialog::OnInitDialog();
}



INT WParticleEditorExportDlg::DoModal()
{
	return WDialog::DoModal( hInstance );
}


#if 1//scion jg -- My export helper functions
	UBOOL StripLine(const TCHAR* szProp,FString& SourceStr);
	void StripLines(const TCHAR* szProp,FString& SourceStr);
	void StripOuter(const TCHAR* szOuter,FString& SourceStr);
#endif

void WParticleEditorExportDlg::OnOk()
{
	// Save the props we need to change to export
	UBOOL OldAutoDestroy = 	EditTarget->AutoDestroy;
	EditTarget->AutoDestroy = AutoDestroyCheck.IsChecked();
	UBOOL OldNoDelete = EditTarget->bNoDelete;
	EditTarget->bNoDelete = FALSE;

	//scion jg -- My reworked export code
	FStringOutputDevice Ar;

	// Get the copy/paste text
	UExporter::ExportToOutputDevice( EditTarget, ConstructObject<UExporter>( UObjectExporterT3D::StaticClass() ), Ar, TEXT("copy"), 0 );

	// Build the class declaration
	FString Output(TEXT("class "));
	Output += ClassEdit.GetText();
	Output += TEXT(" extends Emitter;\r\n");
	FString DefProps(Ar);
	// Remove all myLevel references
	StripOuter(TEXT("MyLevel."),DefProps);
	// Remove all emitter as outer references
	FString strOuter(EditTarget->GetName());
	strOuter += TEXT(".");
	StripOuter(*strOuter,DefProps);
	// Removes unwanted lines from the defaultproperties
	StripLine(TEXT(" PhysicsVolume"),DefProps);
	StripLine(TEXT(" Location"),DefProps);
	StripLine(TEXT(" Level"),DefProps);
	StripLine(TEXT(" Region"),DefProps);
	StripLine(TEXT(" bLightChanged"),DefProps);
	StripLine(TEXT(" bSelected"),DefProps);
	StripLine(TEXT(" SpawnedTime"),DefProps);
	StripLine(TEXT(" Tag"),DefProps);
	StripLines(TEXT("  Name="),DefProps);
	// Append the stripped string to the output
	Output += DefProps;
	FString strFileName = FString::Printf(
		TEXT("../%s/Classes/%s.uc"),*PackageEdit.GetText(),
		*ClassEdit.GetText());
	// Save the file to the package and class names
	appSaveStringToFile(Output,*strFileName);

	// return the PS to normal
	EditTarget->AutoDestroy = OldAutoDestroy;
	EditTarget->bNoDelete = OldNoDelete;

	// close the dialoge
	EndDialog(1);
}

