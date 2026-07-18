//=============================================================================
//
// EmitterWizard.cpp	
// Copyright(c) 2002 Demiurge Studios. All Rights Reserved.
// 
//=============================================================================

#include "UnrealEd.h"
#include "EmitterWizard.h"

//=============================================================================
// WEmitterWizard

WEmitterWizard::WEmitterWizard(WWindow* InOwnerWindow, AEmitter* InEditTarget)
	: EditTarget(InEditTarget),
	  WDialog( TEXT("Emitter Wizard"), IDPS_EMITTER_WIZARD, InOwnerWindow ),
	  NextButton( this, ID_NEXT, FDelegate(this,(TDelegate)OnNext) ),
	  BackButton( this, ID_BACK, FDelegate(this,(TDelegate)OnBack) ),
	  CancelButton( this, ID_CANCEL, FDelegate(this,(TDelegate)OnCancel) ),
	  CurrentPage(NULL),
	  NewParticleEmitter(NULL)
{

}


void WEmitterWizard::OnCancel()
{
	if(NewParticleEmitter)
	{
		EditTarget->Emitters.RemoveItem(NewParticleEmitter);
		delete NewParticleEmitter;
		NewParticleEmitter = NULL;
	}
	EndDialogFalse();
}


void WEmitterWizard::OnDestroy()
{
	for(PageMap::TIterator It(Pages); It; ++It)
	{
		delete It.Value();
	}
	Pages.Empty();

	WDialog::OnDestroy();
}


void WEmitterWizard::OnInitDialog()
{
	WDialog::OnInitDialog();

	// Setup the pages array
	InitWizardPages();

	//Set window caption
	SetText( TEXT("Emitter Wizard") );

	UpdateButtonState();
}

void WEmitterWizard::OpenPage(FString PageName)
{
	//Set window caption
	SetText( *(FString(TEXT("Emitter Wizard - ")) + PageName) );

	WEmitterWizardPage** PagePtrPtr;
	WEmitterWizardPage* Page;

	PagePtrPtr = Pages.Find(PageName);
	check(PagePtrPtr);

	Page = *PagePtrPtr;

	if(!Page->hWnd)
	{
		FRect rect;
		::GetWindowRect(hWnd,rect);
		Page->MoveWindow(8,16,rect.Width() - 16,rect.Height() - 100, TRUE);
		Page->OpenWindow();
		InvalidateRect(hWnd, NULL, TRUE);
	}

	if(CurrentPage)
	{
		CurrentPage->Show(0);
	}
	Page->Show(1);

	CurrentPage = Page;
	CurrentPageName = PageName;
}


void WEmitterWizard::InitWizardPages()
{
	//Pages.Set(TEXT("Start"), new WStartWP(this));
	//Pages.Set(TEXT("New Emitter"), new WNewEmitterWP(this));
	Pages.Set(TEXT("Welcome"), new WNewEmitterWP(this));
	Pages.Set(TEXT("Start Shape"), new WShapeWP(this));
	Pages.Set(TEXT("Movement - Part1"), new WMovementWP(this));
	Pages.Set(TEXT("Texture"), new WTextureWP(this));
	Pages.Set(TEXT("Mesh"), new WMeshWP(this));
	Pages.Set(TEXT("Particle Density"), new WDenseWP(this));
	Pages.Set(TEXT("Rotation"), new WRotationWP(this));
	Pages.Set(TEXT("Time"), new WTimeWP(this));
	Pages.Set(TEXT("Movement - Part2"), new WAdditionalMovementWP(this));
	Pages.Set(TEXT("Beam Noise"), new WBeamNoiseWP(this));
	Pages.Set(TEXT("Finished"), new WEndWP(this));

	for(PageMap::TIterator It(Pages) ; It; ++It)
	{
		It.Value()->Show(0);
	}

	OpenPage(TEXT("Welcome"));
}



INT WEmitterWizard::DoModal()
{
	return WDialog::DoModal( hInstance );
}



void WEmitterWizard::UpdateButtonState()
{
	if(CurrentPageName == TEXT("Finished"))
	{
		NextButton.SetText(TEXT("Finish"));		
	}	
	else
	{
		NextButton.SetText(TEXT("Next >"));
	}

	if(CurrentPageName == TEXT("Welcome"))
	{
		EnableWindow(BackButton.hWnd, FALSE);
	}
	else
	{
		EnableWindow(BackButton.hWnd, TRUE);
	}

	//Delete this if everything works fine
	//FString Caption = TEXT("Emitter Wizard");
	//SetText(*Caption);
}



void WEmitterWizard::OnNext()
{
	// If Next is not "Finish"
	if(CurrentPageName != TEXT("Finished"))
	{
		new(History) FString(CurrentPageName);
		OpenPage(CurrentPage->NotifyLeave());
	}
	// if we are on the last page
	else
	{
		EndDialogTrue();
	}

	UpdateButtonState();
	InvalidateRect(hWnd, NULL, TRUE);
}



void WEmitterWizard::OnBack()
{
	OpenPage(History.Pop());
	UpdateButtonState();
	InvalidateRect(hWnd, NULL, TRUE);
}



void WEmitterWizard::OnCommand(INT Command)
{
	WDialog::OnCommand(Command);
}


//=============================================================================
// WEmitterWizardPage

WEmitterWizardPage::WEmitterWizardPage(WEmitterWizard* InOwnerWindow, INT InResID)
	: WPropertyPage(InOwnerWindow),
	  EditTarget(NULL),
	  ResID(InResID)
{
}



void WEmitterWizardPage::OpenWindow()
{
	WPropertyPage::OpenWindow(ResID, GetModuleHandleA("unrealed.exe"));
}



FString WEmitterWizardPage::NotifyLeave()
{
	return TEXT("NONE");
}



void WEmitterWizardPage::OnCommand(INT Command)
{
	// true if this command was used by the WEmitterWizardPage (and therefore doesn't need to be passed up
	BOOL FoundCommand = FALSE;

	INT ID;
	WCheckBox* Checked = NULL;
	// First, look through all the check boxes and see if there are any that go with this command.
	for(RadioButtonMap::TIterator It(RadioButtons); It; ++It)
	{
		// When one is found, 
		if(It.Value()->ControlId == Command)
		{
			ID = It.Key();
			Checked = It.Value();
			FoundCommand = TRUE;
			break;
		}
	}

	// if we found a checkbox
	if(Checked)
	{
		// Uncheck them all
		TArray<WCheckBox*> Boxes;
		RadioButtons.MultiFind(ID, Boxes);
		for(TArray<WCheckBox*>::TIterator It(Boxes); It; ++It)
		{
			(*It)->SetCheck(FALSE);
		}

		// check the clicked one
		Checked->SetCheck(TRUE);
	}
	
	if(!FoundCommand)
	{
		WPropertyPage::OnCommand(Command);
	}
}



void WEmitterWizardPage::AddRadioButtonHack(WCheckBox* CheckBox, INT ID)
{
	RadioButtons.Add(ID, CheckBox);

	// if this is the first one with the given ID
	TArray<WCheckBox*> Others;
	RadioButtons.MultiFind(ID, Others);
	if(Others.Num() == 1)
	{
		// Check it
		CheckBox->SetCheck(TRUE);
	}
	else
	{
		CheckBox->SetCheck(FALSE);
	}
}



//=============================================================================
// WStartWP

WStartWP::WStartWP(WEmitterWizard* InOwnerWindow)
:	WEmitterWizardPage(InOwnerWindow, IDPS_WIZ_START_)
{	
}



void WStartWP::OpenWindow()
{
	WEmitterWizardPage::OpenWindow();
	Finalize();
}



FString WStartWP::NotifyLeave()
{
	WEmitterWizardPage::NotifyLeave();

	return TEXT("New Emitter");
}




//=============================================================================
// WEndWP

WEndWP::WEndWP(WEmitterWizard* InOwnerWindow)
:	WEmitterWizardPage(InOwnerWindow, IDPS_WIZ_END_)
{	
}



void WEndWP::OpenWindow()
{
	WEmitterWizardPage::OpenWindow();
	Finalize();
}



FString WEndWP::NotifyLeave()
{
	WEmitterWizardPage::NotifyLeave();

	return TEXT("None");
}




//=============================================================================
// WNewEmitterWP


WNewEmitterWP::WNewEmitterWP(WEmitterWizard* InOwnerWindow)
:	WEmitterWizardPage(InOwnerWindow, IDPS_WIZ_NEW_EMITTER)
{	
}



void WNewEmitterWP::OnDestroy()
{
	delete EmitterName;

	delete Sprite;
	delete Spark;
	delete Mesh;
	delete Beam;

	DeleteObject( SpriteBitmap );
	DeleteObject( SparkBitmap );
	DeleteObject( MeshBitmap );
	DeleteObject( BeamBitmap );

	WEmitterWizardPage::OnDestroy();
}



void WNewEmitterWP::OpenWindow()
{
	WEmitterWizardPage::OpenWindow();
	
	EmitterName = new WEdit(this, IDC_Name);
	EmitterName->OpenWindow(1,0,0);	

	Sprite = new WCheckBox(this, IDC_CHECK1);
	Sprite->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	Spark = new WCheckBox(this, IDC_CHECK2);
	Spark->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	Mesh = new WCheckBox(this, IDC_CHECK3);
	Mesh->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	Beam = new WCheckBox(this, IDC_CHECK4);
	Beam->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	
	AddRadioButtonHack(Sprite, 1);
	AddRadioButtonHack(Spark, 1);
	AddRadioButtonHack(Mesh, 1);
	AddRadioButtonHack(Beam, 1);


	PlaceControl(EmitterName);
	PlaceControl(Sprite);
	PlaceControl(Spark);
	PlaceControl(Mesh);
	PlaceControl(Beam);
	Finalize();


	SpriteBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_SPRITE), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(SpriteBitmap);
	Sprite->SetBitmap( SpriteBitmap );

	SparkBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_SPARK), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(SparkBitmap);
	Spark->SetBitmap( SparkBitmap );

	MeshBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_MESH), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(MeshBitmap);
	Mesh->SetBitmap( MeshBitmap );

	BeamBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_BEAM), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(BeamBitmap);
	Beam->SetBitmap( BeamBitmap );
}



FString WNewEmitterWP::NotifyLeave()
{
	WEmitterWizardPage::NotifyLeave();

	WEmitterWizard* Wizard = (WEmitterWizard*)OwnerWindow;
	UParticleEmitter* NewPE = NULL;

	if(Sprite->IsChecked())
	{
		// Make a new sprite Emitter
		NewPE = (UParticleEmitter*)UObject::StaticConstructObject(USpriteEmitter::StaticClass(), Wizard->EditTarget);
	}
	else if(Spark->IsChecked())
	{
		// Make a new spark Emitter
		NewPE = (UParticleEmitter*)UObject::StaticConstructObject(USparkEmitter::StaticClass(), Wizard->EditTarget);
		((USparkEmitter*)NewPE)->TimeBetweenSegmentsRange.Min = 0.2f;
		((USparkEmitter*)NewPE)->TimeBetweenSegmentsRange.Max = 0.2f;
	}
	else if(Mesh->IsChecked())
	{
		// Make a new mesh Emitter
		NewPE = (UParticleEmitter*)UObject::StaticConstructObject(UMeshEmitter::StaticClass(), Wizard->EditTarget);
	}
	else if(Beam->IsChecked())
	{
		// Make a new beam Emitter
		NewPE = (UParticleEmitter*)UObject::StaticConstructObject(UBeamEmitter::StaticClass(), Wizard->EditTarget);
		((UBeamEmitter*)NewPE)->DetermineEndPointBy = PTEP_Distance;
		((UBeamEmitter*)NewPE)->BeamDistanceRange.Min = 500;
		((UBeamEmitter*)NewPE)->BeamDistanceRange.Max = 500;
	}
	else
	{
		check(0); // Bad Emitter type. Missing a clause in the above code block
	}

	// if there was a particle emitter previous created by this wizard
	if(Wizard->NewParticleEmitter)
	{
		// delete it
		Wizard->EditTarget->Emitters.RemoveItem(Wizard->NewParticleEmitter);
		delete Wizard->NewParticleEmitter;
		Wizard->NewParticleEmitter = NULL;
	}

	// Add the new Emitter to the particle system
	Wizard->GetEditTarget()->Emitters.AddItem(NewPE);
	// set the Name of the new Emitter
	NewPE->Name = EmitterName->GetText();

	for(WEmitterWizard::PageMap::TIterator It(Wizard->Pages); It; ++It)
	{
		It.Value()->SetEditTarget(NewPE);
	}

	Wizard->NewParticleEmitter = NewPE;



	UClass* EmitterClass = EditTarget->GetClass();
	if( EmitterClass->IsChildOf(UMeshEmitter::StaticClass()) )
	{
		return TEXT("Mesh");
	}
	else
	{
		return TEXT("Texture");
	}
}



//=============================================================================
// WShapeWP


WShapeWP::WShapeWP(WEmitterWizard* InOwnerWindow)
:	WEmitterWizardPage(InOwnerWindow, IDPS_WIZ_SHAPE_)
{	
}



void WShapeWP::OnDestroy()
{
	delete(Point);
	delete(Box);
	delete(Sphere);
	delete(PlaneHoriz);
	delete(PlaneVert);

	DeleteObject(PointBitmap);
	DeleteObject(BoxBitmap);
	DeleteObject(SphereBitmap);
	DeleteObject(PlaneHorizBitmap);
	DeleteObject(PlaneVertBitmap);

	delete(Small);
	delete(Med);
	delete(Large);

	WEmitterWizardPage::OnDestroy();
}



void WShapeWP::OpenWindow()
{
	WEmitterWizardPage::OpenWindow();

	Point = new WCheckBox(this, IDC_CHECK1);
	Point->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	Box = new WCheckBox(this, IDC_CHECK2);
	Box->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	Sphere = new WCheckBox(this, IDC_CHECK3);
	Sphere->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	PlaneHoriz = new WCheckBox(this, IDC_CHECK4);
	PlaneHoriz->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	PlaneVert = new WCheckBox(this, IDC_CHECK5);
	PlaneVert->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));
	
	AddRadioButtonHack(Point, 1);
	AddRadioButtonHack(Box, 1);
	AddRadioButtonHack(Sphere, 1);
	AddRadioButtonHack(PlaneHoriz, 1);
	AddRadioButtonHack(PlaneVert, 1);


	Small = new WCheckBox(this, IDC_CHECK6);
	Small->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	Med = new WCheckBox(this, IDC_CHECK7);
	Med->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	Large = new WCheckBox(this, IDC_CHECK8);
	Large->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	AddRadioButtonHack(Small, 2);
	AddRadioButtonHack(Med, 2);
	AddRadioButtonHack(Large, 2);


	PlaceControl(Point);
	PlaceControl(Box);
	PlaceControl(Sphere);
	PlaceControl(PlaneHoriz);
	PlaceControl(PlaneVert);
	PlaceControl(Small);
	PlaceControl(Med);
	PlaceControl(Large);
	Finalize();


	PointBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_SHAPE_POINT), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(PointBitmap);
	Point->SetBitmap( PointBitmap );

	BoxBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_SHAPE_BOX), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(BoxBitmap);
	Box->SetBitmap( BoxBitmap );

	SphereBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_SHAPE_SPHERE), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(SphereBitmap);
	Sphere->SetBitmap( SphereBitmap );

	PlaneHorizBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_SHAPE_PLANE_HORIZ), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(PlaneHorizBitmap);
	PlaneHoriz->SetBitmap( PlaneHorizBitmap );

	PlaneVertBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_SHAPE_PLANE_VERT), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(PlaneVertBitmap);
	PlaneVert->SetBitmap( PlaneVertBitmap );

}



FString WShapeWP::NotifyLeave()
{
	WEmitterWizardPage::NotifyLeave();

	FLOAT size = 0;
	if(Small->IsChecked())
		size = 25.0;
	else if(Med->IsChecked())
		size = 75.0;
	else
		size = 400.0;

	UClass* EmitterClass = EditTarget->GetClass();
	if( EmitterClass->IsChildOf(UBeamEmitter::StaticClass()) )
	{
		size = size / 3.0;
	}

	if(Box->IsChecked())
	{
		EditTarget->StartLocationRange.X.Min = -size;		
		EditTarget->StartLocationRange.X.Max = size;		
		EditTarget->StartLocationRange.Y.Min = -size;		
		EditTarget->StartLocationRange.Y.Max = size;		
		EditTarget->StartLocationRange.Z.Min = -size;		
		EditTarget->StartLocationRange.Z.Max = size;		
	}
	else if(Sphere->IsChecked())
	{
		EditTarget->StartLocationShape = PTLS_Sphere;
		EditTarget->SphereRadiusRange.Min = 0;
		EditTarget->SphereRadiusRange.Max = size;
	}
	else if(PlaneHoriz->IsChecked())
	{
		EditTarget->StartLocationRange.X.Min = -size;		
		EditTarget->StartLocationRange.X.Max = size;		
		EditTarget->StartLocationRange.Y.Min = -size;		
		EditTarget->StartLocationRange.Y.Max = size;		
	}
	else if(PlaneVert->IsChecked())
	{
		EditTarget->StartLocationRange.Y.Min = -size;		
		EditTarget->StartLocationRange.Y.Max = size;		
		EditTarget->StartLocationRange.Z.Min = -size;		
		EditTarget->StartLocationRange.Z.Max = size;		
	}
	else
	{
		//Point - do nothing
	}


	return TEXT("Movement - Part1");
}




//=============================================================================
// WMovementWP

WMovementWP::WMovementWP(WEmitterWizard* InOwnerWindow)
:	WEmitterWizardPage(InOwnerWindow, IDPS_WIZ_MOVE)
{	
}



void WMovementWP::OnDestroy()
{
	delete(Parallel);
	delete(Spread);
	delete(Sphere);

	delete(Up);
	delete(Horiz);
	delete(Down);

	delete(SpeedHeading);
	delete(Still);
	delete(Slow);
	delete(Medium);
	delete(Fast);
	delete(VeryFast);

	DeleteObject(ParallelBitmap);
	DeleteObject(SpreadBitmap);
	DeleteObject(SphereBitmap);

	DeleteObject(UpBitmap);
	DeleteObject(HorizBitmap);
	DeleteObject(DownBitmap);

	WEmitterWizardPage::OnDestroy();
}



void WMovementWP::OpenWindow()
{
	WEmitterWizardPage::OpenWindow();

	Parallel = new WCheckBox(this, IDC_CHECK1);
	Parallel->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	Spread = new WCheckBox(this, IDC_CHECK2);
	Spread->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	Sphere = new WCheckBox(this, IDC_CHECK3);
	Sphere->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	
	AddRadioButtonHack(Parallel, 1);
	AddRadioButtonHack(Spread, 1);
	AddRadioButtonHack(Sphere, 1);


	Up = new WCheckBox(this, IDC_CHECK4);
	Up->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	Horiz = new WCheckBox(this, IDC_CHECK5);
	Horiz->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	Down = new WCheckBox(this, IDC_CHECK6);
	Down->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	AddRadioButtonHack(Up, 2);
	AddRadioButtonHack(Horiz, 2);
	AddRadioButtonHack(Down, 2);


	SpeedHeading = new WCheckBox(this, IDC_CHECK12);
	SpeedHeading->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	Slow = new WCheckBox(this, IDC_CHECK8);
	Slow->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	Medium = new WCheckBox(this, IDC_CHECK9);
	Medium->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	Fast = new WCheckBox(this, IDC_CHECK10);
	Fast->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	VeryFast = new WCheckBox(this, IDC_CHECK11);
	VeryFast->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	AddRadioButtonHack(Slow, 3);
	AddRadioButtonHack(Medium, 3);
	AddRadioButtonHack(Fast, 3);
	AddRadioButtonHack(VeryFast, 3);


	Still = new WCheckBox(this, IDC_CHECK7);
	Still->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	PlaceControl(Parallel);
	PlaceControl(Spread);
	PlaceControl(Sphere);

	PlaceControl(Up);
	PlaceControl(Horiz);
	PlaceControl(Down);

	UClass* EmitterClass = EditTarget->GetClass();
	if( !EmitterClass->IsChildOf(UBeamEmitter::StaticClass()) )
	{
		PlaceControl(SpeedHeading);
		PlaceControl(Still);
		PlaceControl(Slow);
		PlaceControl(Medium);
		PlaceControl(Fast);
		PlaceControl(VeryFast);
	}

	Finalize();


	ParallelBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_MOVE_PARALLEL), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(ParallelBitmap);
	Parallel->SetBitmap( ParallelBitmap );

	SpreadBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_MOVE_SPREAD), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(SpreadBitmap);
	Spread->SetBitmap( SpreadBitmap );

	SphereBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_MOVE_SPHERE), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(SphereBitmap);
	Sphere->SetBitmap( SphereBitmap );


	UpBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_DIR_UP), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(UpBitmap);
	Up->SetBitmap( UpBitmap );

	HorizBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_DIR_HORIZ), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(HorizBitmap);
	Horiz->SetBitmap( HorizBitmap );

	DownBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_DIR_DOWN), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(DownBitmap);
	Down->SetBitmap( DownBitmap );

}



FString WMovementWP::NotifyLeave()
{
	WEmitterWizardPage::NotifyLeave();

	if(!Still->IsChecked())
	{

		FLOAT speed = 0;
		if(Slow->IsChecked())
			speed = 20;
		else if(Medium->IsChecked())
			speed = 150;
		else if(Fast->IsChecked())
			speed = 500;
		else if(VeryFast->IsChecked())
			speed = 1600;


		if(Parallel->IsChecked())
		{
			if(Up->IsChecked())
			{
				EditTarget->StartVelocityRange.Z.Min = speed;
				EditTarget->StartVelocityRange.Z.Max = speed;
			}
			else if(Horiz->IsChecked())
			{
				EditTarget->StartVelocityRange.X.Min = speed;
				EditTarget->StartVelocityRange.X.Max = speed;
			}
			else
			{
				EditTarget->StartVelocityRange.Z.Min = -speed;
				EditTarget->StartVelocityRange.Z.Max = -speed;
			}
		}
		else if(Spread->IsChecked())
		{
			if(Up->IsChecked())
			{
				EditTarget->StartVelocityRange.X.Min = -speed/2;
				EditTarget->StartVelocityRange.X.Max = speed/2;
				EditTarget->StartVelocityRange.Y.Min = -speed/2;
				EditTarget->StartVelocityRange.Y.Max = speed/2;
				EditTarget->StartVelocityRange.Z.Min = speed;
				EditTarget->StartVelocityRange.Z.Max = speed;
			}
			else if(Horiz->IsChecked())
			{
				EditTarget->StartVelocityRange.X.Min = speed;
				EditTarget->StartVelocityRange.X.Max = speed;
				EditTarget->StartVelocityRange.Y.Min = -speed/2;
				EditTarget->StartVelocityRange.Y.Max = speed/2;
				EditTarget->StartVelocityRange.Z.Min = -speed/2;
				EditTarget->StartVelocityRange.Z.Max = speed/2;
			}
			else
			{
				EditTarget->StartVelocityRange.X.Min = -speed/2;
				EditTarget->StartVelocityRange.X.Max = speed/2;
				EditTarget->StartVelocityRange.Y.Min = -speed/2;
				EditTarget->StartVelocityRange.Y.Max = speed/2;
				EditTarget->StartVelocityRange.Z.Min = -speed;
				EditTarget->StartVelocityRange.Z.Max = -speed;
			}
		}
		else
		{
			EditTarget->StartVelocityRange.X.Min = -speed;
			EditTarget->StartVelocityRange.X.Max = speed;
			EditTarget->StartVelocityRange.Y.Min = -speed;
			EditTarget->StartVelocityRange.Y.Max = speed;
			EditTarget->StartVelocityRange.Z.Min = -speed;
			EditTarget->StartVelocityRange.Z.Max = speed;
		}
	}


	UClass* EmitterClass = EditTarget->GetClass();
	if( EmitterClass->IsChildOf(UBeamEmitter::StaticClass()) )
	{
		return TEXT("Particle Density");
	}
	else
	{
		return TEXT("Movement - Part2");
	}

}




//=============================================================================
// WTextureWP


WTextureWP::WTextureWP(WEmitterWizard* InOwnerWindow)
:	WEmitterWizardPage(InOwnerWindow, IDPS_WIZ_TEXTURE)
{	
}



void WTextureWP::OnDestroy()
{
	delete(TextureEdit);
	delete(UseButton);

	delete(FadeInCheck);
	delete(FadeOutCheck);

	WEmitterWizardPage::OnDestroy();
}



void WTextureWP::OpenWindow()
{
	WEmitterWizardPage::OpenWindow();

	TextureEdit = new WEdit( this, IDPS_TEXTURE_VALUE );
	TextureEdit->OpenWindow( TRUE, 0, 0 );

	UseButton = new WButton(this, IDPS_TEXTURE_USE);
	UseButton->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	FadeInCheck = new WCheckBox(this, IDC_CHECK1);
	FadeInCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	FadeOutCheck = new WCheckBox(this, IDC_CHECK2);
	FadeOutCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));



	PlaceControl( TextureEdit );
	PlaceControl( UseButton );
	PlaceControl( FadeInCheck );
	PlaceControl( FadeOutCheck );

	Finalize();

	UseButton->ClickDelegate = FDelegate(this, (TDelegate)OnUseClick);

}



void WTextureWP::OnUseClick()
{
	TextureEdit->SetText(GUnrealEd->CurrentMaterial->GetPathName());
}



FString WTextureWP::NotifyLeave()
{
	WEmitterWizardPage::NotifyLeave();

	if(FadeInCheck->IsChecked())
	{
		EditTarget->FadeIn = TRUE;
		//Fade times will be set when lifetime is set
	}

	if(FadeOutCheck->IsChecked())
	{
		EditTarget->FadeOut = TRUE;
		//Fade times will be set when lifetime is set
	}

	EditTarget->Texture = (UTexture*) Cast<UMaterial>(UObject::StaticLoadObject( UMaterial::StaticClass(), NULL, *(TextureEdit->GetText()), NULL, LOAD_NoWarn | RF_Native, NULL ));

	return TEXT("Start Shape");
}




//=============================================================================
// WMeshWP


WMeshWP::WMeshWP(WEmitterWizard* InOwnerWindow)
:	WEmitterWizardPage(InOwnerWindow, IDPS_WIZ_MESH)
{	
}



void WMeshWP::OnDestroy()
{
	delete(MeshEdit);
	delete(UseButton);

	delete(FadeInCheck);
	delete(FadeOutCheck);

	WEmitterWizardPage::OnDestroy();
}



void WMeshWP::OpenWindow()
{
	WEmitterWizardPage::OpenWindow();

	MeshEdit = new WEdit( this, IDPS_TEXTURE_VALUE );
	MeshEdit->OpenWindow( TRUE, 0, 0 );

	UseButton = new WButton(this, IDPS_TEXTURE_USE);
	UseButton->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	FadeInCheck = new WCheckBox(this, IDC_CHECK1);
	FadeInCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	FadeOutCheck = new WCheckBox(this, IDC_CHECK2);
	FadeOutCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));



	PlaceControl( MeshEdit );
	PlaceControl( UseButton );
	PlaceControl( FadeInCheck );
	PlaceControl( FadeOutCheck );

	Finalize();

	UseButton->ClickDelegate = FDelegate(this, (TDelegate)OnUseClick);

}



void WMeshWP::OnUseClick()
{
	MeshEdit->SetText(GUnrealEd->CurrentStaticMesh->GetPathName());
}



FString WMeshWP::NotifyLeave()
{
	WEmitterWizardPage::NotifyLeave();

	(((UMeshEmitter*)EditTarget)->UseParticleColor) = TRUE;

	if(FadeInCheck->IsChecked())
	{
		EditTarget->FadeIn = TRUE;
		//Fade times will be set when lifetime is set
	}

	if(FadeOutCheck->IsChecked())
	{
		EditTarget->FadeOut = TRUE;
		//Fade times will be set when lifetime is set
	}

	(((UMeshEmitter*)EditTarget)->StaticMesh) = Cast<UStaticMesh>(UObject::StaticLoadObject( UStaticMesh::StaticClass(), NULL, *(MeshEdit->GetText()), NULL, LOAD_NoWarn | RF_Native, NULL ));

	return TEXT("Start Shape");
}




//=============================================================================
// WDenseWP

WDenseWP::WDenseWP(WEmitterWizard* InOwnerWindow)
:	WEmitterWizardPage(InOwnerWindow, IDPS_WIZ_DENSE)
{	
}



void WDenseWP::OnDestroy()
{
	delete(SparseCheck);
	delete(NormalCheck);
	delete(DenseCheck);

	delete(SmallCheck);
	delete(MediumCheck);
	delete(LargeCheck);

	DeleteObject(SparseBitmap);
	DeleteObject(NormalBitmap);
	DeleteObject(DenseBitmap);

	DeleteObject(SmallBitmap);
	DeleteObject(MediumBitmap);
	DeleteObject(LargeBitmap);

	WEmitterWizardPage::OnDestroy();
}



void WDenseWP::OpenWindow()
{
	WEmitterWizardPage::OpenWindow();

	SparseCheck = new WCheckBox(this, IDC_CHECK1);
	SparseCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	NormalCheck = new WCheckBox(this, IDC_CHECK2);
	NormalCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	DenseCheck = new WCheckBox(this, IDC_CHECK3);
	DenseCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	
	AddRadioButtonHack(SparseCheck, 1);
	AddRadioButtonHack(NormalCheck, 1);
	AddRadioButtonHack(DenseCheck, 1);


	SmallCheck = new WCheckBox(this, IDC_CHECK4);
	SmallCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	MediumCheck = new WCheckBox(this, IDC_CHECK5);
	MediumCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	LargeCheck = new WCheckBox(this, IDC_CHECK6);
	LargeCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	AddRadioButtonHack(SmallCheck, 2);
	AddRadioButtonHack(MediumCheck, 2);
	AddRadioButtonHack(LargeCheck, 2);



	PlaceControl(SparseCheck);
	PlaceControl(NormalCheck);
	PlaceControl(DenseCheck);
	PlaceControl(SmallCheck);
	PlaceControl(MediumCheck);
	PlaceControl(LargeCheck);
	Finalize();


	SparseBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_DENSE_SPARSE), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(SparseBitmap);
	SparseCheck->SetBitmap( SparseBitmap );

	NormalBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_DENSE_NORMAL), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(NormalBitmap);
	NormalCheck->SetBitmap( NormalBitmap );

	DenseBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_DENSE_DENSE), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(DenseBitmap);
	DenseCheck->SetBitmap( DenseBitmap );


	SmallBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_PSIZE_SMALL), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(SmallBitmap);
	SmallCheck->SetBitmap( SmallBitmap );

	MediumBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_PSIZE_MEDIUM), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(MediumBitmap);
	MediumCheck->SetBitmap( MediumBitmap );

	LargeBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_PSIZE_LARGE), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(LargeBitmap);
	LargeCheck->SetBitmap( LargeBitmap );

}



FString WDenseWP::NotifyLeave()
{
	WEmitterWizardPage::NotifyLeave();


	if(SparseCheck->IsChecked())
		EditTarget->MaxParticles = 8;
	else if(NormalCheck->IsChecked())
		EditTarget->MaxParticles = 20;
	else if(DenseCheck->IsChecked())
		EditTarget->MaxParticles = 50;


	if(SmallCheck->IsChecked())
	{
		EditTarget->StartSizeRange.X.Min /= 10;		
		EditTarget->StartSizeRange.X.Max /= 10;		
		EditTarget->StartSizeRange.Y.Min /= 10;		
		EditTarget->StartSizeRange.Y.Max /= 10;		
		EditTarget->StartSizeRange.Z.Min /= 10;		
		EditTarget->StartSizeRange.Z.Max /= 10;		
	}
	else if(MediumCheck->IsChecked())
	{
		EditTarget->StartSizeRange.X.Min /= 2;		
		EditTarget->StartSizeRange.X.Max /= 2;		
		EditTarget->StartSizeRange.Y.Min /= 2;		
		EditTarget->StartSizeRange.Y.Max /= 2;		
		EditTarget->StartSizeRange.Z.Min /= 2;		
		EditTarget->StartSizeRange.Z.Max /= 2;		
	}
	else if(LargeCheck->IsChecked())
	{
		//keep size 100		
	}


	return TEXT("Time");
}





//=============================================================================
// WRotationWP

WRotationWP::WRotationWP(WEmitterWizard* InOwnerWindow)
:	WEmitterWizardPage(InOwnerWindow, IDPS_WIZ_ROTATE)
{	
}



void WRotationWP::OnDestroy()
{
	delete(StartSpinCheck);

	delete(NoSpinCheck);
	delete(SlowCheck);
	delete(FastCheck);

	WEmitterWizardPage::OnDestroy();
}



void WRotationWP::OpenWindow()
{
	WEmitterWizardPage::OpenWindow();

	StartSpinCheck = new WCheckBox(this, IDC_CHECK1);
	StartSpinCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));


	NoSpinCheck = new WCheckBox(this, IDC_CHECK2);
	NoSpinCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	SlowCheck = new WCheckBox(this, IDC_CHECK3);
	SlowCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	FastCheck = new WCheckBox(this, IDC_CHECK4);
	FastCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	AddRadioButtonHack(NoSpinCheck, 1);
	AddRadioButtonHack(SlowCheck, 1);
	AddRadioButtonHack(FastCheck, 1);


	PlaceControl(StartSpinCheck);
	PlaceControl(NoSpinCheck);
	PlaceControl(SlowCheck);
	PlaceControl(FastCheck);
	Finalize();
}



FString WRotationWP::NotifyLeave()
{
	WEmitterWizardPage::NotifyLeave();

	if(StartSpinCheck->IsChecked())
	{		
		EditTarget->SpinParticles = TRUE;
		EditTarget->StartSpinRange.X.Min = 0.0f;
		EditTarget->StartSpinRange.Y.Min = 0.0f;
		EditTarget->StartSpinRange.Z.Min = 0.0f;
		EditTarget->StartSpinRange.X.Max = 1.0f;
		EditTarget->StartSpinRange.Y.Max = 1.0f;
		EditTarget->StartSpinRange.Z.Max = 1.0f;
	}

	if(SlowCheck->IsChecked())
	{
		EditTarget->SpinParticles = TRUE;
		EditTarget->SpinsPerSecondRange.X.Min = .05f;
		EditTarget->SpinsPerSecondRange.Y.Min = .05f;
		EditTarget->SpinsPerSecondRange.Z.Min = .05f;
		EditTarget->SpinsPerSecondRange.X.Max = .1f;
		EditTarget->SpinsPerSecondRange.Y.Max = .1f;
		EditTarget->SpinsPerSecondRange.Z.Max = .1f;
	}
	else if(FastCheck->IsChecked())
	{
		EditTarget->SpinParticles = TRUE;
		EditTarget->SpinsPerSecondRange.X.Min = .5f;
		EditTarget->SpinsPerSecondRange.Y.Min = .5f;
		EditTarget->SpinsPerSecondRange.Z.Min = .5f;
		EditTarget->SpinsPerSecondRange.X.Max = 1.0f;
		EditTarget->SpinsPerSecondRange.Y.Max = 1.0f;
		EditTarget->SpinsPerSecondRange.Z.Max = 1.0f;
	}

	return TEXT("Particle Density");

}




//=============================================================================
// WTimeWP

WTimeWP::WTimeWP(WEmitterWizard* InOwnerWindow)
:	WEmitterWizardPage(InOwnerWindow, IDPS_WIZ_TIME)
{	
}



void WTimeWP::OnDestroy()
{
	WEmitterWizardPage::OnDestroy();

	delete(LifetimeEdit);
	delete(LifetimeSlide);

	delete(RespawnDeadCheck);

	delete(SteadyCheck);
	delete(BurstCheck);
}



void WTimeWP::OpenWindow()
{
	WEmitterWizardPage::OpenWindow();

	LifetimeEdit = new WEdit( this, IDC_EDIT1 );
	LifetimeEdit->OpenWindow( TRUE, 0, 0 );

	LifetimeSlide = new WButtonSlider( this, LifetimeEdit, IDC_BUTTON1 );
	LifetimeSlide->OpenWindow();


	RespawnDeadCheck = new WCheckBox(this, IDC_CHECK1);
	RespawnDeadCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));


	SteadyCheck = new WCheckBox(this, IDC_CHECK2);
	SteadyCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	BurstCheck = new WCheckBox(this, IDC_CHECK3);
	BurstCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	AddRadioButtonHack(SteadyCheck, 1);
	AddRadioButtonHack(BurstCheck, 1);


	PlaceControl(LifetimeEdit);
	PlaceControl(LifetimeSlide);
	PlaceControl(RespawnDeadCheck);
	PlaceControl(SteadyCheck);
	PlaceControl(BurstCheck);
	Finalize();

	LifetimeEdit->SetText(TEXT("4.0"));
	RespawnDeadCheck->SetCheck(1);
}



FString WTimeWP::NotifyLeave()
{
	WEmitterWizardPage::NotifyLeave();

	EditTarget->LifetimeRange.Min = appAtof( *LifetimeEdit->GetText() );
	EditTarget->LifetimeRange.Max = appAtof( *LifetimeEdit->GetText() );

	if(EditTarget->FadeIn)
		EditTarget->FadeInEndTime = appAtof( *LifetimeEdit->GetText() ) / 2;

	if(EditTarget->FadeOut)
		EditTarget->FadeOutStartTime = appAtof( *LifetimeEdit->GetText() ) / 2;

	EditTarget->RespawnDeadParticles = RespawnDeadCheck->IsChecked();

	if( BurstCheck->IsChecked() )
	{
		EditTarget->AutomaticInitialSpawning = FALSE;
		EditTarget->InitialParticlesPerSecond = 5000;
	}

	UClass* EmitterClass = EditTarget->GetClass();
	if( EmitterClass->IsChildOf(UBeamEmitter::StaticClass()) )
	{
		return TEXT("Beam Noise");
	}
	else
	{
		return TEXT("Finished");
	}
}






//=============================================================================
// WAdditionalMovementWP

WAdditionalMovementWP::WAdditionalMovementWP(WEmitterWizard* InOwnerWindow)
:	WEmitterWizardPage(InOwnerWindow, IDPS_WIZ_ADDMOVE)
{	
}



void WAdditionalMovementWP::OnDestroy()
{
	WEmitterWizardPage::OnDestroy();

	delete(CollideCheck);

	delete(NoGravityCheck);
	delete(WeakCheck);
	delete(NormalCheck);
	delete(StrongCheck);
}



void WAdditionalMovementWP::OpenWindow()
{
	WEmitterWizardPage::OpenWindow();

	UClass* EmitterClass = EditTarget->GetClass();

	CollideCheck = new WCheckBox(this, IDC_CHECK1);
	CollideCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	NoGravityCheck = new WCheckBox(this, IDC_CHECK2);
	NoGravityCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	WeakCheck = new WCheckBox(this, IDC_CHECK3);
	WeakCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	NormalCheck = new WCheckBox(this, IDC_CHECK4);
	NormalCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	StrongCheck = new WCheckBox(this, IDC_CHECK5);
	StrongCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	AddRadioButtonHack(NoGravityCheck, 1);
	AddRadioButtonHack(WeakCheck, 1);
	AddRadioButtonHack(NormalCheck, 1);
	AddRadioButtonHack(StrongCheck, 1);


	if( !EmitterClass->IsChildOf(USparkEmitter::StaticClass()) )
		PlaceControl(CollideCheck);
	PlaceControl(NoGravityCheck);
	PlaceControl(WeakCheck);
	PlaceControl(NormalCheck);
	PlaceControl(StrongCheck);
	Finalize();
}



FString WAdditionalMovementWP::NotifyLeave()
{
	WEmitterWizardPage::NotifyLeave();

	EditTarget->UseCollision = CollideCheck->IsChecked();
	
	if(!NoGravityCheck->IsChecked())
	{
		FLOAT strength = 0;
		if(WeakCheck->IsChecked())
		{
			strength = 100;
		}
		else if(NormalCheck->IsChecked())
		{
			strength = 1500;
		}
		else if(StrongCheck->IsChecked())
		{
			strength = 5000;
		}

		EditTarget->Acceleration.Z = -strength;
	}

	UClass* EmitterClass = EditTarget->GetClass();
	if( EmitterClass->IsChildOf(USpriteEmitter::StaticClass()) || EmitterClass->IsChildOf(UMeshEmitter::StaticClass()) )
	{
		return TEXT("Rotation");
	}
	else
	{
		return TEXT("Particle Density");
	}
}




//=============================================================================
// WBeamNoiseWP

WBeamNoiseWP::WBeamNoiseWP(WEmitterWizard* InOwnerWindow)
:	WEmitterWizardPage(InOwnerWindow, IDPS_WIZ_BEAMNOISE)
{	
}



void WBeamNoiseWP::OnDestroy()
{
	WEmitterWizardPage::OnDestroy();

	delete(NoNoiseCheck);
	delete(SmallCheck);
	delete(MediumCheck);
	delete(LargeCheck);

	delete(ShortCheck);
	delete(AverageCheck);
	delete(LongCheck);

	DeleteObject(NoNoiseBitmap);
	DeleteObject(SmallBitmap);
	DeleteObject(MediumBitmap);
	DeleteObject(LargeBitmap);
}



void WBeamNoiseWP::OpenWindow()
{
	WEmitterWizardPage::OpenWindow();

	NoNoiseCheck = new WCheckBox(this, IDC_CHECK1);
	NoNoiseCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	SmallCheck = new WCheckBox(this, IDC_CHECK2);
	SmallCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	MediumCheck = new WCheckBox(this, IDC_CHECK3);
	MediumCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	LargeCheck = new WCheckBox(this, IDC_CHECK4);
	LargeCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	AddRadioButtonHack(NoNoiseCheck, 1);
	AddRadioButtonHack(SmallCheck, 1);
	AddRadioButtonHack(MediumCheck, 1);
	AddRadioButtonHack(LargeCheck, 1);


	ShortCheck = new WCheckBox(this, IDC_CHECK5);
	ShortCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	AverageCheck = new WCheckBox(this, IDC_CHECK6);
	AverageCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	LongCheck = new WCheckBox(this, IDC_CHECK7);
	LongCheck->OpenWindow(TRUE, 0, 0, 0, 0, TEXT(""));

	AddRadioButtonHack(ShortCheck, 2);
	AddRadioButtonHack(AverageCheck, 2);
	AddRadioButtonHack(LongCheck, 2);


	PlaceControl(NoNoiseCheck);
	PlaceControl(SmallCheck);
	PlaceControl(MediumCheck);
	PlaceControl(LargeCheck);
	PlaceControl(ShortCheck);
	PlaceControl(AverageCheck);
	PlaceControl(LongCheck);
	Finalize();

	NoNoiseBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_BEAM_NONE), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(NoNoiseBitmap);
	NoNoiseCheck->SetBitmap( NoNoiseBitmap );

	SmallBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_BEAM_SMALL), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(SmallBitmap);
	SmallCheck->SetBitmap( SmallBitmap );

	MediumBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_BEAM_MED), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(MediumBitmap);
	MediumCheck->SetBitmap( MediumBitmap );


	LargeBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDPS_BEAM_LARGE), IMAGE_BITMAP, 0, 0,  LR_LOADMAP3DCOLORS );
	check(LargeBitmap);
	LargeCheck->SetBitmap( LargeBitmap );

}



FString WBeamNoiseWP::NotifyLeave()
{
	WEmitterWizardPage::NotifyLeave();

	UBeamEmitter *BeamEmitter = (UBeamEmitter*) EditTarget;

	if(!NoNoiseCheck->IsChecked())
	{

		
		FLOAT noise = 0;
		if(SmallCheck->IsChecked())
		{
			noise = 10;
		}
		else if(MediumCheck->IsChecked())
		{
			noise = 50;
		}
		else if(LargeCheck->IsChecked())
		{
			noise = 100;
		}
		
		BeamEmitter->HighFrequencyNoiseRange.X.Min = -noise;
		BeamEmitter->HighFrequencyNoiseRange.Y.Min = -noise;
		BeamEmitter->HighFrequencyNoiseRange.Z.Min = -noise;
		BeamEmitter->HighFrequencyNoiseRange.X.Max = noise;
		BeamEmitter->HighFrequencyNoiseRange.Y.Max = noise;
		BeamEmitter->HighFrequencyNoiseRange.Z.Max = noise;
	}

	FLOAT length = 0;
	if(ShortCheck->IsChecked())
	{
		length = 200;
	}
	else if(AverageCheck->IsChecked())
	{
		length = 400;
	}
	else if(LongCheck->IsChecked())
	{
		length = 800;
	}
	BeamEmitter->BeamDistanceRange.Min = length;
	BeamEmitter->BeamDistanceRange.Max = length;

	return TEXT("Finished");
}











