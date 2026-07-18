/*=============================================================================
	TexProp : Properties of a texture
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/

#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

INT GLastViewportNum = -1;

/*-----------------------------------------------------------------------------
	WDlgTexProp.
-----------------------------------------------------------------------------*/

WDlgTexProp::WDlgTexProp( UObject* InContext, WWindow* InOwnerWindow, UMaterial* InTexture )
:	WDialog			( TEXT("Material Properties"), IDDIALOG_TEX_PROP, InOwnerWindow )
,	ClearButton		( this, IDPB_CLEAR, FDelegate(this,(TDelegate)OnClear) )
,	FillButton		( this, IDPB_FILL, FDelegate(this,(TDelegate)OnFill) )
,	CubeButton		( this, IDPB_CUBE, FDelegate(this,(TDelegate)OnCube) )
,	SphereButton	( this, IDPB_SPHERE, FDelegate(this,(TDelegate)OnSphere) )
,	ScrollBar		( this, IDSB_TREE )
,	ShowFallback	( this, IDCK_FALLBACK, FDelegate(this,(TDelegate)OnShowFallback) )
,	ShowBackdrop	( this, IDCK_BACKDROP, FDelegate(this,(TDelegate)OnShowBackdrop) )
,	ShowTopLevel	( this, IDCK_TOPLEVEL, FDelegate(this,(TDelegate)OnShowTopLevel) )
,	ErrorLabel		( this, IDC_MATERIALERROR )
,	ErrorButton		( this, IDC_ERRORBUTTON, FDelegate(this,(TDelegate)OnError) )
{
	OldUSize = OldVSize = 0;
	StaticMesh = NULL;
	Container = NULL;
	ViewportName = FString::Printf( TEXT("TextureProp%d"), GLastViewportNum );
	MaterialsViewportName = FString::Printf( TEXT("MaterialsViewport%d"), ++GLastViewportNum );
	Material = MaterialTreeCurrent = InTexture;
	Viewport = NULL;
	pProps = new WObjectProperties( NAME_None, CPF_Edit, TEXT(""), this, 1 );
	pProps->ShowTreeLines = 1;

	PlaneBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_DTP_PLANE), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS);	check(PlaneBitmap);
	CubeBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_DTP_CUBE), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS );	check(CubeBitmap);
	SphereBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_DTP_SPHERE), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS );	check(SphereBitmap);
	BackdropBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_DTP_BACKDROP), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS );	check(BackdropBitmap);
	FallbackBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_DTP_FALLBACK), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS );	check(FallbackBitmap);
	TopLevelBitmap = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_DTP_TOPLEVEL), IMAGE_BITMAP, 0, 0, LR_LOADMAP3DCOLORS );	check(TopLevelBitmap);
}

// WDialog interface.
void WDlgTexProp::OnInitDialog()
{
	guard(WDlgTexProp::OnInitDialog);
	WDialog::OnInitDialog();

	GMaterialTools->Add( (DWORD)hWnd );

	Container = new FContainer();

	pProps->OpenChildWindow( IDSC_PROPS );
	pProps->Root.SetObjects( (UObject**)&Material, 1 );
	pProps->SetNotifyHook( this );

	Viewport = NULL;
	MaterialsViewport = NULL;

	SetCaption( Material );


	FillButton.SetBitmap( PlaneBitmap );
	CubeButton.SetBitmap( CubeBitmap );
	SphereButton.SetBitmap( SphereBitmap );
	ShowBackdrop.SetBitmap( BackdropBitmap );
	ShowFallback.SetBitmap( FallbackBitmap );
	ShowTopLevel.SetBitmap( TopLevelBitmap );

	LastError = TEXT("");
	ErrorMaterial = NULL;
	::ShowWindow( ErrorButton.hWnd, SW_HIDE );


	HWND wnd = GetDlgItem( hWnd, IDSC_MATERIALS );
	Anchors.Set( (DWORD)wnd,	FWindowAnchor( hWnd, wnd,	ANCHOR_TL, 300, 4,							ANCHOR_RIGHT|ANCHOR_HEIGHT, -4, 316 ) );
	wnd = GetDlgItem( hWnd, IDSC_MATERIALS_VIEWPORT );
	Anchors.Set( (DWORD)wnd,	FWindowAnchor( hWnd, wnd,	ANCHOR_TL, 300+8, STANDARD_CTRL_HEIGHT-2,	ANCHOR_RIGHT|ANCHOR_HEIGHT, -4-8-STANDARD_SB_WIDTH, 316-STANDARD_CTRL_HEIGHT ) );
	Anchors.Set( (DWORD)ScrollBar.hWnd,	FWindowAnchor( hWnd, ScrollBar.hWnd,	ANCHOR_TOP|ANCHOR_RIGHT, -4-8-STANDARD_SB_WIDTH, STANDARD_CTRL_HEIGHT-2,	ANCHOR_WIDTH|ANCHOR_HEIGHT, STANDARD_SB_WIDTH, 316-STANDARD_CTRL_HEIGHT ) );
	wnd = GetDlgItem( hWnd, IDSC_PROPSBORDER );
	Anchors.Set( (DWORD)wnd,	FWindowAnchor( hWnd, wnd,	ANCHOR_TL, 4, 316+6,						ANCHOR_BR, -4, -4-STANDARD_CTRL_HEIGHT ) );
	wnd = GetDlgItem( hWnd, IDSC_PROPS );
	Anchors.Set( (DWORD)wnd,	FWindowAnchor( hWnd, wnd,	ANCHOR_TL, 4+6, 316+6+STANDARD_CTRL_HEIGHT,	ANCHOR_BR, -4-10, -2*STANDARD_CTRL_HEIGHT+6 ) );
	Anchors.Set( (DWORD)pProps->hWnd,	FWindowAnchor( wnd, pProps->hWnd,	ANCHOR_TL, 0,0,		ANCHOR_BR, 0,0) );
	Anchors.Set( (DWORD)ErrorLabel.hWnd,FWindowAnchor( hWnd, ErrorLabel.hWnd, ANCHOR_BOTTOM|ANCHOR_LEFT, 55, -STANDARD_CTRL_HEIGHT+1, ANCHOR_BR, -4-10, 0 ) );
	Anchors.Set( (DWORD)ErrorButton.hWnd,FWindowAnchor( hWnd, ErrorButton.hWnd, ANCHOR_BOTTOM|ANCHOR_LEFT, 4, -STANDARD_CTRL_HEIGHT-2, ANCHOR_BOTTOM|ANCHOR_LEFT, 50, -2 ) );
	RefreshViewports();

	Container->SetAnchors( &Anchors );

	PositionChildControls();

	FTexPropWindowInfo* Info = GMaterialTools->GetInfo( (DWORD)hWnd );
	Info->TopLevelMaterial = Material;

	FillButton.SetCheck( BST_CHECKED );
	ShowFallback.SetCheck( BST_CHECKED );

	unguard;
}
void WDlgTexProp::SetCaption( UMaterial* InMaterial )
{
	FString Caption = FString::Printf( TEXT("%s Properties - %s (%dx%d)"),
		InMaterial->GetClass()->GetName(),
		InMaterial->GetPathName(),
		InMaterial->MaterialUSize(),
		InMaterial->MaterialVSize() );

	SetText( *Caption );
}
void WDlgTexProp::RefreshViewports()
{	
	//
	// Create the preview viewport
	//

	if( Viewport )	delete Viewport;

	Viewport = GUnrealEd->Client->NewViewport( *ViewportName );
	GUnrealEd->Level->SpawnViewActor( Viewport );
	Viewport->Input->Init( Viewport );
	check(Viewport->Actor);
	Viewport->Actor->ShowFlags = SHOW_StandardView | SHOW_ChildWindow | SHOW_RealTime | SHOW_StaticMeshes | SHOW_Actors | SHOW_Frame | SHOW_NoFallbackMaterials;
	Viewport->Actor->RendMap   = REN_TexView;
	Viewport->Group = NAME_None;
	Viewport->Actor->Misc2 = (INT)this;
	FTexPropWindowInfo* Info = GMaterialTools->GetInfo( (DWORD)hWnd );
	Info->Material = Material;
	RECT rc;
	::GetWindowRect( GetDlgItem( hWnd, IDSC_PREVIEW_VIEWPORT ), &rc );
	POINT pt;
	pt.x = rc.left;		pt.y = rc.top;
	::ScreenToClient( hWnd, &pt );

	FLOAT Scale;

	if( Material->MaterialUSize() > Material->MaterialVSize() )
		Scale = min( 256, Material->MaterialUSize() ) / (FLOAT)Material->MaterialUSize();
	else
		Scale = min( 256, Material->MaterialVSize() ) / (FLOAT)Material->MaterialVSize();

	Viewport->OpenWindow( (DWORD)hWnd, 0, Material->MaterialUSize()*Scale, Material->MaterialVSize()*Scale, pt.x, pt.y );

	//
	// Create the preview viewport
	//

	if( MaterialsViewport )
	{
		Anchors.Remove( (DWORD)MaterialsViewport->GetWindow() );
		delete MaterialsViewport;
	}

	MaterialsViewport = GUnrealEd->Client->NewViewport( *MaterialsViewportName );
	GUnrealEd->Level->SpawnViewActor( MaterialsViewport );
	MaterialsViewport->Input->Init( MaterialsViewport );
	check(MaterialsViewport->Actor);
	MaterialsViewport->Actor->ShowFlags = SHOW_StandardView | SHOW_ChildWindow | SHOW_RealTime | SHOW_NoFallbackMaterials;
	MaterialsViewport->Actor->RendMap   = REN_MaterialEditor;
	MaterialsViewport->Group = NAME_None;

	MaterialsViewport->MiscRes = Material;						// The root of the material tre
	MaterialsViewport->Actor->Misc1 = (INT)hWnd;				// Used for searches
	MaterialsViewport->Actor->Misc2 = (INT)MaterialTreeCurrent;	// Holds the currently selected material on the tree

	::GetWindowRect( GetDlgItem( hWnd, IDSC_MATERIALS ), &rc );
	pt.x = rc.left;		pt.y = rc.top;
	::ScreenToClient( hWnd, &pt );
	MaterialsViewport->OpenWindow( (DWORD)GetDlgItem( hWnd, IDSC_MATERIALS_VIEWPORT ), 0, rc.right-rc.left-6, rc.bottom-rc.top-19, pt.x+3, pt.y+16 );

	Anchors.Set( (DWORD)MaterialsViewport->GetWindow(),	FWindowAnchor( GetDlgItem( hWnd, IDSC_MATERIALS_VIEWPORT ), (HWND)MaterialsViewport->GetWindow(),	ANCHOR_TL, 0,0,		ANCHOR_BR, 0,0 ) );
	Container->SetAnchors( &Anchors );
	PositionChildControls();
	RefreshScrollBar();
}
void WDlgTexProp::OnDestroy()
{
	guard(WDlgTexProp::OnDestroy);
	WDialog::OnDestroy();

	delete pProps;

	delete Viewport;
	delete MaterialsViewport;

	delete Container;

	DeleteObject( PlaneBitmap );
	DeleteObject( CubeBitmap );
	DeleteObject( SphereBitmap );
	DeleteObject( BackdropBitmap );
	DeleteObject( FallbackBitmap );
	DeleteObject( TopLevelBitmap );

	PostMessageX( GBrowserTextureHwnd, WM_COMMAND, WM_DLGTEXPROP_CLOSING, (LPARAM)hWnd );

	unguard;
}
void WDlgTexProp::PositionChildControls()
{
	if( Container ) Container->RefreshControls();
	if( pProps )
	{
		InvalidateRect( pProps->hWnd, NULL, 1 );
		InvalidateRect( pProps->List, NULL, 1 );
	}
}
void WDlgTexProp::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	PositionChildControls();
	InvalidateRect( hWnd, NULL, 1 );
}
void WDlgTexProp::DoModeless( UBOOL bShow )
{
	guard(WDlgTexProp::DoModeless);
	_Windows.AddItem( this );
	hWnd = CreateDialogParamA( hInstance, MAKEINTRESOURCEA(IDDIALOG_TEX_PROP), OwnerWindow?OwnerWindow->hWnd:NULL, (DLGPROC)StaticDlgProc, (LPARAM)this);
	if( !hWnd )
		appGetLastError();
	Show( bShow );
	unguard;
}
void WDlgTexProp::OnClear()
{
	guard(WDlgTexProp::OnClear);
	FTexPropWindowInfo* Info = GMaterialTools->GetInfo( (DWORD)hWnd );
	if( Info && Info->Material )
	{
		UTexture* Texture = Cast<UTexture>( Info->Material );
		if( Texture )
			Texture->Clear( TCLEAR_Temporal );
	}
	unguard;
}
void WDlgTexProp::OnShowFallback()
{
	guard(WDlgTexProp::OnShowFallback);
	FTexPropWindowInfo* Info = GMaterialTools->GetInfo( (DWORD)hWnd );
	if( Info )
		Info->bShowFallback = ShowFallback.IsChecked();
	unguard;
}
void WDlgTexProp::OnShowBackdrop()
{
	guard(WDlgTexProp::OnShowBackdrop);
	FTexPropWindowInfo* Info = GMaterialTools->GetInfo( (DWORD)hWnd );
	if( Info )
		Info->bShowBackdrop = ShowBackdrop.IsChecked();
	unguard;
}
void WDlgTexProp::OnShowTopLevel()
{
	guard(WDlgTexProp::OnShowTopLevel);
	FTexPropWindowInfo* Info = GMaterialTools->GetInfo( (DWORD)hWnd );
	if( Info )
		Info->bForceTopLevel = ShowTopLevel.IsChecked();
	unguard;
}
void WDlgTexProp::OnFill()
{
	Viewport->Actor->Misc1 = MVT_TEXTURE;
	Viewport->Actor->RendMap = REN_TexView;
	FTexPropWindowInfo* Info = GMaterialTools->GetInfo( (DWORD)hWnd );
	Info->Material = MaterialTreeCurrent;
	Info->StaticMesh = StaticMesh;

	OldUSize = OldVSize = 0;
	SizeTexViewport();

	FillButton.SetCheck( BST_CHECKED );
	CubeButton.SetCheck( BST_UNCHECKED );
	SphereButton.SetCheck( BST_UNCHECKED );
}
void WDlgTexProp::OnCube()
{
	Viewport->Actor->Misc1 = MVT_CUBE;
	Viewport->Actor->RendMap = REN_TexView;
	StaticMesh = GUnrealEd->TexPropCube;
	FTexPropWindowInfo* Info = GMaterialTools->GetInfo( (DWORD)hWnd );
	Info->Material = MaterialTreeCurrent;
	Info->StaticMesh = StaticMesh;

	::SetWindowPos( (HWND)Viewport->GetWindow(), HWND_TOP, 0, 0, 256, 256, SWP_NOMOVE );

	FillButton.SetCheck( BST_UNCHECKED );
	CubeButton.SetCheck( BST_CHECKED );
	SphereButton.SetCheck( BST_UNCHECKED );
}
void WDlgTexProp::OnSphere()
{
	Viewport->Actor->Misc1 = MVT_SPHERE;
	Viewport->Actor->RendMap = REN_TexView;
	StaticMesh = GUnrealEd->TexPropSphere;
	FTexPropWindowInfo* Info = GMaterialTools->GetInfo( (DWORD)hWnd );
	Info->Material = MaterialTreeCurrent;
	Info->StaticMesh = StaticMesh;

	::SetWindowPos( (HWND)Viewport->GetWindow(), HWND_TOP, 0, 0, 256, 256, SWP_NOMOVE );

	FillButton.SetCheck( BST_UNCHECKED );
	CubeButton.SetCheck( BST_UNCHECKED );
	SphereButton.SetCheck( BST_CHECKED );
}
void WDlgTexProp::Serialize( FArchive& Ar )
{
	guard(WDlgTexProp::Serialize);
	WDialog::Serialize( Ar );
	Ar << Viewport << Material << MaterialTreeCurrent << StaticMesh;
	unguard;
}
void WDlgTexProp::MaterialTreeClick( UMaterial* InMaterial )
{
	MaterialTreeCurrent = InMaterial;
	MaterialsViewport->Actor->Misc2 = (INT)InMaterial;
	MaterialsViewport->Repaint( 1 );

	FTexPropWindowInfo* Info = GMaterialTools->GetInfo( (DWORD)hWnd );
	Info->Material = MaterialTreeCurrent;
	Info->StaticMesh = StaticMesh;
	pProps->Root.SetObjects( (UObject**)&InMaterial, 1 );

	SetCaption( InMaterial );
}
void WDlgTexProp::OnVScroll( WPARAM wParam, LPARAM lParam )
{
	if( (HWND)lParam == ScrollBar.hWnd )
	{
		FTexPropWindowInfo* Info = GMaterialTools->GetInfo( (DWORD)hWnd );

		switch(LOWORD(wParam)) {

			case SB_LINEUP:
				Info->TreeScrollPos -= 32;
				Info->TreeScrollPos = Max( Info->TreeScrollPos, 0 );
				break;

			case SB_LINEDOWN:
				Info->TreeScrollPos += 32;
				Info->TreeScrollPos = Min( Info->TreeScrollPos, Info->TreeScrollMax );
				break;

			case SB_PAGEUP:
				Info->TreeScrollPos -= 64;
				Info->TreeScrollPos = Max( Info->TreeScrollPos, 0 );
				break;

			case SB_PAGEDOWN:
				Info->TreeScrollPos += 64;
				Info->TreeScrollPos = Min( Info->TreeScrollPos, Info->TreeScrollMax );
				break;

			case SB_THUMBTRACK:
				Info->TreeScrollPos = (short int)HIWORD(wParam);
				break;
		}

		RefreshScrollBar();
		MaterialsViewport->Repaint( 1 );
	}
}
void WDlgTexProp::RefreshScrollBar()
{
	guard(WDlgTexProp::RefreshScrollBar);

	if( !ScrollBar ) return;

	FTexPropWindowInfo* Info = GMaterialTools->GetInfo( (DWORD)hWnd );

	// Set the scroll bar to have a valid range.
	SCROLLINFO si;
	si.cbSize = sizeof(SCROLLINFO);
	si.fMask = SIF_DISABLENOSCROLL | SIF_RANGE | SIF_POS | SIF_PAGE;
	si.nPage = MaterialsViewport->SizeY;
	si.nMin = 0;
	si.nMax = Info->TreeScrollMax+MaterialsViewport->SizeY;
	si.nPos = Info->TreeScrollPos;
	SetScrollInfo( ScrollBar.hWnd, SB_CTL, &si, TRUE );

	unguard;
}
void WDlgTexProp::OnError()
{
	guard(WDlgTexProp::OnError)
	MaterialTreeClick( ErrorMaterial );
	unguard;
}

void WDlgTexProp::SizeTexViewport()
{
	if( Material->MaterialUSize() != OldUSize ||
		Material->MaterialVSize() != OldVSize )
	{
		OldUSize = Material->MaterialUSize();
		OldVSize = Material->MaterialVSize();

		FLOAT Scale;
		if( Material->MaterialUSize() > Material->MaterialVSize() )
			Scale = min( 256, Material->MaterialUSize() ) / (FLOAT)Material->MaterialUSize();
		else
			Scale = min( 256, Material->MaterialVSize() ) / (FLOAT)Material->MaterialVSize();

		::SetWindowPos( (HWND)Viewport->GetWindow(), HWND_TOP, 0, 0, Material->MaterialUSize()*Scale, Material->MaterialVSize()*Scale, SWP_NOMOVE );
	}
}

// FNotifyHook notifications
void WDlgTexProp::NotifyDestroy( void* Src )
{
	GUnrealEd->NotifyDestroy(Src);
}
void WDlgTexProp::NotifyPreChange( void* Src )
{
	GUnrealEd->NotifyPreChange(Src);
}
void WDlgTexProp::NotifyExec( void* Src, const TCHAR* Cmd )
{
	GUnrealEd->NotifyExec(Src, Cmd);
}
void WDlgTexProp::NotifyPostChange( void* Src )
{
	GUnrealEd->NotifyPostChange(Src);
	SizeTexViewport();
}

void WDlgTexProp::Render_TexView( UViewport* Viewport )
{
	guard(REN_TexView);

	Viewport->Actor->bHiddenEd = 1;
	Viewport->Actor->bHiddenEdGroup = 1;
	Viewport->Actor->bHidden = 1;

	FTexPropWindowInfo* Info = GMaterialTools->GetInfo( (INT)hWnd );

	if( Info )
	{
		UMaterial* Material = Info->Material;
		if( Info->bForceTopLevel )
			Material = Info->TopLevelMaterial;

		// Material error checking
		FString ErrStr;
		UMaterial* ErrMat=NULL;
		INT NumPasses = 0;
		Viewport->RI->SetMaterial( Material, &ErrStr, &ErrMat, &NumPasses );
		if( ErrMat )
		{
			ErrorMaterial = ErrMat;
			if( LastError != ErrStr )
			{
				::ShowWindow( ErrorButton.hWnd, SW_SHOW );
				ErrorLabel.SetText(*ErrStr);
				LastError = ErrStr;
			}
		}
		else
		{
			// Display the number of passes in the error label.
			FString PassesMessage = FString::Printf(TEXT("%d pass%s"), NumPasses, NumPasses==1?TEXT(""):TEXT("es") );
			ErrorMaterial = NULL;
			if( LastError != PassesMessage )
			{
				::ShowWindow( ErrorButton.hWnd, SW_HIDE );
				ErrorLabel.SetText(*PassesMessage);
				LastError = PassesMessage;
			}
		}		

		if( Info->bShowBackdrop )
		{
			Viewport->Canvas->DrawIcon( GUnrealEd->MaterialBackdrop, 0, 0, Viewport->SizeX, Viewport->SizeY, 0.0f, FPlane(1,1,1,1), FPlane(0,0,0,0) );
			Viewport->RI->Clear(0,FColor(0,0,0),1,1.0f);
		}

		Viewport->Actor->ShowFlags &= ~SHOW_NoFallbackMaterials;
		if( Info->bShowFallback )
			Viewport->Actor->ShowFlags |= SHOW_NoFallbackMaterials;

		switch( Viewport->Actor->Misc1 )
		{
			case MVT_TEXTURE:
			{
				if( Material )
				{
					SizeTexViewport();
					PUSH_HIT(Viewport,HTextureView(Material,Viewport->SizeX,Viewport->SizeY));
					Viewport->Canvas->DrawIcon( Material, 0, 0, Viewport->SizeX, Viewport->SizeY, 0.0f, FPlane(1,1,1,1), FPlane(0,0,0,0) );
					POP_HIT(Viewport);
				}
			}
			break;

			case MVT_CUBE:
			case MVT_SPHERE:
			{
				FCameraSceneNode	SceneNode(Viewport,&Viewport->RenderTarget,Viewport->Actor,FVector(0,0,0),FRotator(0,0,0),Viewport->Actor->FovAngle);

				SceneNode.Viewport->RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);
				SceneNode.Viewport->RI->SetTransform(TT_WorldToCamera,SceneNode.WorldToCamera);
				SceneNode.Viewport->RI->SetTransform(TT_CameraToScreen,SceneNode.CameraToScreen);

				Viewport->RI->SetCullMode(CM_CW);

				if( Info->StaticMesh )
				{
					Viewport->Actor->SetDrawType(DT_StaticMesh);
					Viewport->Actor->StaticMesh = Info->StaticMesh;
					Viewport->Actor->AmbientGlow = 128; // Rendering unlit meshes.

					Viewport->Actor->Skins.Empty();
					for( INT x = 0 ; x < Info->StaticMesh->Sections.Num() ; ++x )
						Viewport->Actor->Skins.AddItem( Material );

					FVector SaveLocation = Viewport->Actor->Location;

					Viewport->Actor->Location = FVector(320,0,0);

					Viewport->Actor->ClearRenderData();
					FDynamicActor* DynamicActor = Viewport->Actor->GetActorRenderData();

					DynamicActor->Render(&SceneNode,NULL,NULL,Viewport->RI);

					Viewport->Actor->Location = SaveLocation;
					Viewport->Actor->Skins.Empty();
					Viewport->Actor->StaticMesh = NULL;
				}
			}
			break;
		}
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

