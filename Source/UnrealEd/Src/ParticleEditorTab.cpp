//=============================================================================
//
// ParticleEditorTab.cpp	
// Copyright(c) 2002 Demiurge Studios. All Rights Reserved.
// 
//=============================================================================

#include "UnrealEd.h"


//=============================================================================
// WParticleEditorTab 

WParticleEditorTab::WParticleEditorTab(WWindow* InOwnerWindow, WParticleEditor* InOwnerEditor, FString InTabName, UParticleEmitter* InEditTarget, AEmitter* InEmitter):
	WPropertyPage(InOwnerWindow),
	OwnerEditor(InOwnerEditor),
	EditTarget(InEditTarget),
	Container(NULL),
	scrollLoc(0),
	InitialVertOffset(10),
	spaceBetweenComponents(10),
	Emitter(InEmitter)
{
	Caption = InTabName;

}



void WParticleEditorTab::OnDestroy()
{
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

	if(scrollBar)
	{
		delete scrollBar;
		scrollBar = NULL;
	}


	//don't delete now that there are global componoents
	/*
	for(INT i = 0; i < currentComponents.Num(); i++)
		delete currentComponents(i);
	currentComponents.Empty();
	*/

}

void WParticleEditorTab::OnCreate()
{
	WPropertyPage::OnCreate();

	//open all the components if not opened
	if(!OwnerEditor->ComponentsOpened)
		OwnerEditor->InitialOpenComponents(this);

	// create and position the scroll-bar for the right-side.
	scrollBar = new WScrollBar( this, IDSB_SCROLLBAR );
	scrollBar->OpenWindow( 1, 0, 0, 0, 0, 1 );
	Anchors.Set((DWORD)scrollBar->hWnd, FWindowAnchor( hWnd, scrollBar->hWnd, ANCHOR_TOP | ANCHOR_RIGHT, -STANDARD_SB_WIDTH, 4, ANCHOR_BOTTOM | ANCHOR_WIDTH, STANDARD_SB_WIDTH, 0 ) );

	// create and position the left-side list of categories
	CategoryList = new WCheckListBox( this, IDLB_PARTICLE_EDITOR_CATS );
	CategoryList->OpenWindow( 1, 0, 1, 1 );
	Anchors.Set((DWORD)CategoryList->hWnd, FWindowAnchor(hWnd, CategoryList->hWnd, ANCHOR_TL, 0, 0, ANCHOR_BOTTOM | ANCHOR_WIDTH, 100, 0 ) );
	CategoryList->SelectionChangeDelegate = FDelegate(this, (TDelegate)OnComponentSelChange);

	CreateComponents();

	SetComponentAnchors();

	//finalize the placement of components given the anchors
	Container = new FContainer();
	Container->SetAnchors( &Anchors );
	PositionChildControls();

	Refresh();
}



void WParticleEditorTab::AddComponent(FString ComponentName, UBOOL Show)
{
	//opens the component with the given name (if it is not already open) and adds it to the list of components

	WParticleEditorComponent* NewComp = OwnerEditor->GetComponent( ComponentName );

	check(NewComp);
	
	currentComponents.AddItem(NewComp);
	
	//adds component Name to the end of the category List
	CategoryList->InsertItem(CategoryList->GetCount(), *NewComp->GetComponentName() );

	//sets the component to checked(Shown)
	CategoryList->SetItemData( CategoryList->FindStringExact( *NewComp->GetComponentName() ), Show );	
}



void WParticleEditorTab::SetComponentAnchors()
{
	INT Top = InitialVertOffset - scrollLoc;
	for(INT i = 0; i < currentComponents.Num(); i++)
	{
		if(currentComponents(i)->bShow)
		{
			INT height = currentComponents(i)->GetSize().Y;
			Anchors.Set((DWORD)currentComponents(i)->hWnd, FWindowAnchor( hWnd, currentComponents(i)->hWnd, ANCHOR_TL, 110, Top, ANCHOR_RIGHT | ANCHOR_HEIGHT, -20, height));
			//Anchors.Set((DWORD)currentComponents(i)->hWnd, FWindowAnchor( hWnd, currentComponents(i)->hWnd, ANCHOR_TL, 110, Top, ANCHOR_WIDTH | ANCHOR_HEIGHT, 500, height));
			Top+=height + spaceBetweenComponents;
		}
	}

	PositionChildControls();
}


void WParticleEditorTab::PositionChildControls()
{
	if( Container ) 
		Container->RefreshControls();
}


void WParticleEditorTab::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	WWindow::OnSize(Flags, NewX, NewY);
	Refresh();
}


void WParticleEditorTab::RefreshScrollBar()
{
	if( !scrollBar ) 
		return;

	// Set the scroll bar to have a valid range.
	//
	SCROLLINFO si;
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_DISABLENOSCROLL | SIF_RANGE | SIF_POS | SIF_PAGE;
	si.nPage = GetClientRect().Height(); 
	si.nMin = 0;
	si.nMax = componentListSize;
	si.nPos = scrollLoc;
	scrollLoc = SetScrollInfo( scrollBar->hWnd, SB_CTL, &si, TRUE );
}


void WParticleEditorTab::RereadComponentValues(UBOOL ForceUpdateAll)
{
	for(INT i = 0; i < currentComponents.Num(); i++)
	{
		currentComponents(i)->RereadToolValues(ForceUpdateAll);
	}
}


void WParticleEditorTab::OnComponentSelChange()
{
	INT Location = 0;

	// for each component
	INT i;
	for(i = 0; i < CategoryList->GetCount(); i++)
	{
		// up to the selection
		if(CategoryList->GetSelected(i))
		{
			break;
		}
		// add the height
		if(currentComponents(i)->bShow)
		{
			Location += currentComponents(i)->GetSize().Y + spaceBetweenComponents;		
		}
	}

	scrollLoc = Location;
	Refresh();
}



void WParticleEditorTab::Refresh(UBOOL JustScroll)
{
	componentListSize = InitialVertOffset;

	for(INT i = 0 ; i < CategoryList->GetCount() ; ++i)
	{
		if(!JustScroll)
		{
			currentComponents(i)->Show((UBOOL)CategoryList->GetItemData(i));
		}
		if((UBOOL)CategoryList->GetItemData(i))
		{
			componentListSize += currentComponents(i)->GetSize().Y + spaceBetweenComponents;
		}
	}

	RefreshScrollBar();

	//this call makes it so the scroll bar works
	SetComponentAnchors();

	// if we're just scrolling
	if(!JustScroll)
	{
		InvalidateRect( hWnd, NULL, FALSE );	
	}
}



void WParticleEditorTab::OnVScroll( WPARAM wParam, LPARAM lParam )
{
	if( (HWND)lParam == scrollBar->hWnd )
	{
		switch(LOWORD(wParam))
		{
			case SB_LINEUP:
				scrollLoc -= 20;
				scrollLoc = Max( scrollLoc, 0 );
				break;

			case SB_LINEDOWN:
				scrollLoc += 20;
				scrollLoc = Min( scrollLoc, componentListSize);
				break;

			case SB_PAGEUP:
				scrollLoc -= GetClientRect().Height() - 50;
				scrollLoc = Max( scrollLoc, 0 );
				break;

			case SB_PAGEDOWN:
				scrollLoc += GetClientRect().Height() - 50;
				scrollLoc = Min( scrollLoc, componentListSize );
				break;

			case SB_THUMBTRACK:
				scrollLoc = (short)HIWORD(wParam);
				break;
		}

		Refresh(TRUE);
	}
}


void WParticleEditorTab::OnShowWindow(UBOOL bShow)
{
	WPropertyPage::OnShowWindow(bShow);

	
	if(bShow && OwnerEditor->ComponentsReadyToShow)
	{
		if(OwnerEditor->LastTab && OwnerEditor->LastTab != this)
		{
			OwnerEditor->LastTab->SaveStates();
		}

		OwnerEditor->LastTab = this;			

		for(INT i = 0; i < currentComponents.Num(); i++)
		{
			currentComponents(i)->SetParentTab(this);

			//SLOW
			currentComponents(i)->LinkToolsToEmitter(EditTarget);
			//END SLOW

			if(ExpandState.Num() == currentComponents.Num())  //If values have been filled in
				currentComponents(i)->SetExpandState( ExpandState(i) );

			if(ToolState.Num() == currentComponents.Num())  //If values have been filled in
				currentComponents(i)->SetToolState( ToolState(i) );

			currentComponents(i)->SetToolAnchors();
		}

		//LITTE SLOW
		RereadComponentValues(TRUE);
		//END LITTLE SLOW

		Refresh();
	}
}

void WParticleEditorTab::SaveStates()
{
	ExpandState.Empty();
	ExpandState.AddZeroed( currentComponents.Num() );
	
	ToolState.Empty();
	ToolState.AddZeroed( currentComponents.Num() );
	
	for(INT i = 0; i < currentComponents.Num(); i++)
	{
		ExpandState(i) = currentComponents(i)->GetExpandState();
		ToolState(i) = currentComponents(i)->GetToolState();
	}	
}
		


void WParticleEditorTab::OnCommand( INT Command )
{

	switch( Command )
	{
		case ID_SHAPE_RECENTER: //event for clicking check box
		{
			Refresh();
		}
		break;
	}
}



void WParticleEditorTab::OnPaint()
{
	guard(WParticleEditorTab::OnPaint);
	PAINTSTRUCT PS;
	HDC hDC = BeginPaint( *this, &PS );
	FillRect( hDC, GetClientRect(), (HBRUSH)(COLOR_BTNFACE+1) );
	EndPaint( *this, &PS );
	unguard;
}


void WParticleEditorTab::CreateComponents()
{
}




//=============================================================================
// WParticleEditorSpriteTab 

WParticleEditorSpriteTab::WParticleEditorSpriteTab(WWindow* ownerWindow, WParticleEditor* InOwnerEditor, FString InTabName, UParticleEmitter* InEditTarget, AEmitter* InEmitter):
	WParticleEditorTab(ownerWindow, InOwnerEditor, InTabName, InEditTarget, InEmitter)
{

}


void WParticleEditorSpriteTab::CreateComponents()
{
	WParticleEditorTab::CreateComponents();

	AddComponent( FString(TEXT("General")), TRUE );
	AddComponent( FString(TEXT("Texture")), TRUE );
	AddComponent( FString(TEXT("Color/Fading")), TRUE );
	AddComponent( FString(TEXT("Rendering")), FALSE );
	AddComponent( FString(TEXT("Time")), TRUE );
	AddComponent( FString(TEXT("Location")), TRUE );
	AddComponent( FString(TEXT("Movement")), TRUE );
	AddComponent( FString(TEXT("Rotation ")), FALSE );
	AddComponent( FString(TEXT("Size")), TRUE );
	AddComponent( FString(TEXT("Collision")), FALSE );
}




//=============================================================================
// WParticleEditorMeshTab 

WParticleEditorMeshTab::WParticleEditorMeshTab(WWindow* ownerWindow, WParticleEditor* InOwnerEditor, FString InTabName, UParticleEmitter* InEditTarget, AEmitter* InEmitter):
	WParticleEditorTab(ownerWindow, InOwnerEditor, InTabName, InEditTarget, InEmitter)
{

}


void WParticleEditorMeshTab::CreateComponents()
{
	WParticleEditorTab::CreateComponents();
	AddComponent( FString(TEXT("General")), TRUE );
	AddComponent( FString(TEXT("Texture ")), TRUE );
	AddComponent( FString(TEXT("Color/Fading")), TRUE );
	AddComponent( FString(TEXT("Rendering")), FALSE );
	AddComponent( FString(TEXT("Time")), TRUE );
	AddComponent( FString(TEXT("Location")), TRUE );
	AddComponent( FString(TEXT("Movement")), TRUE );
	AddComponent( FString(TEXT("Rotation  ")), FALSE );
	AddComponent( FString(TEXT("Size")), TRUE );
	AddComponent( FString(TEXT("Collision")), FALSE );
	AddComponent( FString(TEXT("Mesh")), TRUE );
}




//=============================================================================
// WParticleEditorBeamTab 

WParticleEditorBeamTab::WParticleEditorBeamTab(WWindow* ownerWindow, WParticleEditor* InOwnerEditor, FString InTabName, UParticleEmitter* InEditTarget, AEmitter* InEmitter):
	WParticleEditorTab(ownerWindow, InOwnerEditor, InTabName, InEditTarget, InEmitter)
{
}


void WParticleEditorBeamTab::CreateComponents()
{
	WParticleEditorTab::CreateComponents();

	AddComponent( FString(TEXT("General")), TRUE );
	AddComponent( FString(TEXT("Texture")), TRUE );
	AddComponent( FString(TEXT("Color/Fading")), TRUE );
	AddComponent( FString(TEXT("Rendering")), FALSE );
	AddComponent( FString(TEXT("Time")), TRUE );
	AddComponent( FString(TEXT("Location")), TRUE );
	AddComponent( FString(TEXT("Movement")), TRUE );
	AddComponent( FString(TEXT("Rotation")), FALSE );
	AddComponent( FString(TEXT("Size")), TRUE );
	AddComponent( FString(TEXT("Beam")), TRUE );
	AddComponent( FString(TEXT("Beam Noise")), TRUE );
	AddComponent( FString(TEXT("Beam Branching")), FALSE );
}




//=============================================================================
// WParticleEditorSparkTab 

WParticleEditorSparkTab::WParticleEditorSparkTab(WWindow* ownerWindow, WParticleEditor* InOwnerEditor, FString InTabName, UParticleEmitter* InEditTarget, AEmitter* InEmitter):
	WParticleEditorTab(ownerWindow, InOwnerEditor, InTabName, InEditTarget, InEmitter)
{

}


void WParticleEditorSparkTab::CreateComponents()
{
	WParticleEditorTab::CreateComponents();

	AddComponent( FString(TEXT("General")), TRUE );
	AddComponent( FString(TEXT("Texture")), TRUE );
	AddComponent( FString(TEXT("Color/Fading")), TRUE );
	AddComponent( FString(TEXT("Rendering")), FALSE );
	AddComponent( FString(TEXT("Time")), TRUE );
	AddComponent( FString(TEXT("Location")), TRUE );
	AddComponent( FString(TEXT("Movement")), TRUE );
	AddComponent( FString(TEXT("Rotation")), FALSE );
	AddComponent( FString(TEXT("Size")), FALSE );
	AddComponent( FString(TEXT("Spark")), TRUE );
}


