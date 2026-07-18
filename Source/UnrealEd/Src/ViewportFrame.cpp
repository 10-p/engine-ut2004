/*=============================================================================
	ViewportFrame : Simple window to hold a viewport into a level
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

#include "UnrealEd.h"
#include "..\..\core\inc\UnMsg.h"

extern WSurfacePropSheet* GSurfPropSheet;
extern WBuildPropSheet* GBuildSheet;

WVFToolBar::WVFToolBar( FName InPersistentName, WWindow* InOwnerWindow )
	:	WWindow( InPersistentName, InOwnerWindow )
{
	hbm = (HBITMAP)LoadImageA( hInstance, MAKEINTRESOURCEA(IDBM_VF_TOOLBAR), IMAGE_BITMAP, 0, 0, LR_DEFAULTCOLOR );	check(hbm);
	GetObjectA( hbm, sizeof(BITMAP), (LPSTR)&bm );
	Viewport = NULL;
	brushBack = CreateSolidBrush( RGB(128,128,128) );
	penLine = CreatePen( PS_SOLID, 1, RGB(80,80,80) );
}

// WWindow interface.
void WVFToolBar::OpenWindow()
{
	guard(WVFToolBar::OpenWindow);
	MdiChild = 0;

	PerformCreateWindowEx
	(
		0,
		NULL,
		WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		0,
		0,
		320,
		200,
		OwnerWindow ? OwnerWindow->hWnd : NULL,
		NULL,
		hInstance
	);
	SendMessageX( *this, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), MAKELPARAM(0,0) );
	unguard;
}
void WVFToolBar::OnDestroy()
{
	guard(WVFToolBar::OnDestroy);
	WWindow::OnDestroy();
	for( INT x = 0 ; x < Buttons.Num() ; ++x )
		DestroyWindow( Buttons(x).hWnd );
	Buttons.Empty();
	DeleteObject( hbm );
	DeleteObject( brushBack );
	DeleteObject( penLine );
	unguard;
}
void WVFToolBar::OnCreate()
{
	guard(WVFToolBar::OnCreate);
	WWindow::OnCreate();
	unguard;
}
void WVFToolBar::SetCaption( FString InCaption )
{
	guard(WVFToolBar::OnCreate);
	Caption = InCaption;
	InvalidateRect( hWnd, NULL, FALSE );
	unguard;
}
void WVFToolBar::OnPaint()
{
	guard(WVFToolBar::OnPaint);
	PAINTSTRUCT PS;
	HDC hDC = BeginPaint( *this, &PS );

	RECT rc;
	::GetClientRect( hWnd, &rc );

	FillRect( hDC, &rc, brushBack );

	rc.left += 2;
	rc.top += 1;
	if( GViewportStyle == VSTYLE_Fixed )
	{
		::SetBkMode( hDC, TRANSPARENT );
		::DrawTextA( hDC, TCHAR_TO_ANSI( *Caption ), ::strlen( TCHAR_TO_ANSI( *Caption ) ), &rc, DT_LEFT | DT_SINGLELINE );
	}
	rc.left -= 2;
	rc.top -= 1;

	HPEN OldPen = (HPEN)SelectObject( hDC, penLine );
	::MoveToEx( hDC, 0, rc.bottom - 4, NULL );
	::LineTo( hDC, rc.right, rc.bottom - 4 );
	SelectObject( hDC, OldPen );

	EndPaint( *this, &PS );
	unguard;
}
void WVFToolBar::AddButton( FString InToolTip, INT InID, 
	INT InClientLeft, INT InClientTop, INT InClientRight, INT InClientBottom,
	INT InBmpOffLeft, INT InBmpOffTop, INT InBmpOffRight, INT InBmpOffBottom,
	INT InBmpOnLeft, INT InBmpOnTop, INT InBmpOnRight, INT InBmpOnBottom )
{
	guard(WVFToolBar::AddButton);

	new(Buttons)WPictureButton( this );
	WPictureButton* ppb = &(Buttons(Buttons.Num() - 1));
	check(ppb);

	ppb->SetUp( InToolTip, InID, 
		InClientLeft, InClientTop, InClientRight, InClientBottom,
		hbm, InBmpOffLeft, InBmpOffTop, InBmpOffRight, InBmpOffBottom,
		hbm, InBmpOnLeft, InBmpOnTop, InBmpOnRight, InBmpOnBottom );
	ppb->OpenWindow();
	ppb->OnSize( SIZE_MAXSHOW, InClientRight, InClientBottom );

	unguard;
}
void WVFToolBar::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	guard(WVFToolBar::OnSize);
	WWindow::OnSize(Flags, NewX, NewY);

	FDeferWindowPos dwp;

	for( INT x = 0 ; x < Buttons.Num() ; ++x )
	{
		WPictureButton* ppb = &(Buttons(x));
		dwp.MoveWindow( ppb->hWnd, ppb->ClientPos.left, ppb->ClientPos.top, ppb->ClientPos.right, ppb->ClientPos.bottom, 1 );
	}

	unguard;
}
void WVFToolBar::SetViewport( UViewport* pViewport )
{
	guard(WVFToolBar::SetViewport);
	Viewport = pViewport;
	unguard;
}
void WVFToolBar::OnRightButtonUp()
{
	guard(WVFToolBar::OnRightButtonUp);

	HMENU menu = LoadMenuIdX(hInstance, IDMENU_VF_CONTEXT);
	HMENU submenu = GetSubMenu( menu, 0 );

	POINT pt;
	::GetCursorPos( &pt );

	// "Check" appropriate menu items based on current settings.
	CheckMenuItem( submenu, ID_MapOverhead, (Viewport->Actor->RendMap == REN_OrthXY) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_MapXZ, (Viewport->Actor->RendMap == REN_OrthXZ) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_MapYZ, (Viewport->Actor->RendMap == REN_OrthYZ) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_MapWire, (Viewport->Actor->RendMap == REN_Wire) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_MapPolys, (Viewport->Actor->RendMap == REN_Polys) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_MapPolyCuts, (Viewport->Actor->RendMap == REN_PolyCuts) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_MapPlainTex, (Viewport->Actor->RendMap == REN_PlainTex) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_MapLighting, (Viewport->Actor->RendMap == REN_LightingOnly) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_MapDynLight, (Viewport->Actor->RendMap == REN_DynLight) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_MapZones, (Viewport->Actor->RendMap == REN_Zones) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_DepthComplexity, (Viewport->Actor->RendMap == REN_DepthComplexity) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_LockToSelectedActor, (Viewport->bLockOnSelectedActors) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowLargeVertices, (Viewport->bShowLargeVertices) ? MF_CHECKED : MF_UNCHECKED );

	CheckMenuItem( submenu, ID_ShowBrush, (Viewport->Actor->ShowFlags&SHOW_Brush) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowStaticMeshes, (Viewport->Actor->ShowFlags&SHOW_StaticMeshes) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowBackdrop, (Viewport->Actor->ShowFlags&SHOW_Backdrop) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowCoords, (Viewport->Actor->ShowFlags&SHOW_Coords) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowMovingBrushes, (Viewport->Actor->ShowFlags&SHOW_MovingBrushes) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowVolumes, (Viewport->Actor->ShowFlags&SHOW_Volumes) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowPaths, (Viewport->Actor->ShowFlags&SHOW_Paths) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowEventLines, (Viewport->Actor->ShowFlags&SHOW_EventLines) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowSelectionHighlight, (Viewport->Actor->ShowFlags&SHOW_SelectionHighlight) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowTerrain, (Viewport->Actor->ShowFlags&SHOW_Terrain) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowDistanceFog, (Viewport->Actor->ShowFlags&SHOW_DistanceFog) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowMatRotations, (Viewport->Actor->ShowFlags&SHOW_MatRotations) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowMatPaths, (Viewport->Actor->ShowFlags&SHOW_MatPaths) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowKarmaPrimitives, (Viewport->Actor->ShowFlags&SHOW_KarmaPrimitives) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowFluidSurfaces, (Viewport->Actor->ShowFlags&SHOW_FluidSurfaces) ? MF_CHECKED : MF_UNCHECKED );
	CheckMenuItem( submenu, ID_ShowCollision, (Viewport->Actor->ShowFlags&SHOW_Collision) ? MF_CHECKED : MF_UNCHECKED );
	
	CheckMenuItem( submenu, ID_Color16Bit, ((Viewport->ColorBytes==2) ? MF_CHECKED : MF_UNCHECKED ) );
	CheckMenuItem( submenu, ID_Color32Bit, ((Viewport->ColorBytes==4) ? MF_CHECKED : MF_UNCHECKED ) );

	DWORD ShowFilter = Viewport->Actor->ShowFlags & (SHOW_Actors | SHOW_ActorInfo | SHOW_ActorIcons | SHOW_ActorRadii);
	if		(ShowFilter==(SHOW_Actors | SHOW_ActorIcons)) CheckMenuItem( submenu, ID_ActorsIcons, MF_CHECKED );
	else if (ShowFilter==(SHOW_Actors | SHOW_ActorRadii)) CheckMenuItem( submenu, ID_ActorsRadii, MF_CHECKED );
	else if (ShowFilter==(SHOW_Actors | SHOW_ActorInfo )) CheckMenuItem( submenu, ID_ActorInfo, MF_CHECKED );
	else CheckMenuItem( submenu, ID_ActorsHide, MF_CHECKED );

	TrackPopupMenu( submenu,
		TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
		pt.x, pt.y, 0,
		OwnerWindow->hWnd, NULL);

	DestroyMenu( menu );

	unguard;
}
// Sets the bOn variable in the various buttons
void WVFToolBar::UpdateButtons()
{
	guard(WVFToolBar::UpdateButtons);

	if( !Viewport ) return;

	for( INT x = 0 ; x < Buttons.Num() ; ++x )
	{
		switch( Buttons(x).ID )
		{
			case IDMN_VF_REALTIME_PREVIEW:
				Buttons(x).bOn = Viewport->Actor->ShowFlags & SHOW_PlayerCtrl;
				break;

			case ID_MapDynLight:
				Buttons(x).bOn = (Viewport->Actor->RendMap == REN_DynLight);
				break;

			case ID_MapPlainTex:
				Buttons(x).bOn = (Viewport->Actor->RendMap == REN_PlainTex);
				break;

			case ID_MapLighting:
				Buttons(x).bOn = (Viewport->Actor->RendMap == REN_LightingOnly);
				break;

			case ID_MapWire:
				Buttons(x).bOn = (Viewport->Actor->RendMap == REN_Wire);
				break;

			case ID_MapOverhead:
				Buttons(x).bOn = (Viewport->Actor->RendMap == REN_OrthXY);
				break;

			case ID_MapXZ:
				Buttons(x).bOn = (Viewport->Actor->RendMap == REN_OrthXZ);
				break;

			case ID_MapYZ:
				Buttons(x).bOn = (Viewport->Actor->RendMap == REN_OrthYZ);
				break;

			case ID_MapPolys:
				Buttons(x).bOn = (Viewport->Actor->RendMap == REN_Polys);
				break;

			case ID_MapPolyCuts:
				Buttons(x).bOn = (Viewport->Actor->RendMap == REN_PolyCuts);
				break;

			case ID_MapZones:
				Buttons(x).bOn = (Viewport->Actor->RendMap == REN_Zones);
				break;

			case ID_DepthComplexity:
				Buttons(x).bOn = (Viewport->Actor->RendMap == REN_DepthComplexity);
				break;

			case ID_LockToSelectedActor:
				Buttons(x).bOn = Viewport->bLockOnSelectedActors;
				break;

			case ID_ShowLargeVertices:
				Buttons(x).bOn = Viewport->bShowLargeVertices;
				break;
		}

		InvalidateRect( Buttons(x).hWnd, NULL, 1 );
	}

	unguard;
}
void WVFToolBar::OnCommand( INT Command )
{
	guard(WVFToolBar::OnCommand);
	switch( Command ) {

		case WM_PB_PUSH:
			ButtonClicked(LastlParam);
			SendMessageX( OwnerWindow->hWnd, WM_COMMAND, WM_VIEWPORT_UPDATEWINDOWFRAME, 0 );
			break;

		default:
			WWindow::OnCommand(Command);
			break;
	}
	unguard;
}
void WVFToolBar::ButtonClicked( INT ID )
{
	guard(WVFToolBar::ButtonClicked);

	switch( ID )
	{
		case IDMN_VF_REALTIME_PREVIEW:
			Viewport->Actor->ShowFlags ^= SHOW_PlayerCtrl;
			GUnrealEd->Exec( TEXT("AUDIO FINDVIEWPORT") );
			break;

		case ID_MapDynLight:
			Viewport->Actor->RendMap=REN_DynLight;
			Viewport->Repaint( 1 );
			break;

		case ID_MapLighting:
			Viewport->Actor->RendMap=REN_LightingOnly;
			Viewport->Repaint( 1 );
			break;

		case ID_MapPlainTex:
			Viewport->Actor->RendMap=REN_PlainTex;
			Viewport->Repaint( 1 );
			break;

		case ID_MapWire:
			Viewport->Actor->RendMap=REN_Wire;
			Viewport->Repaint( 1 );
			break;

		case ID_MapOverhead:
			Viewport->Actor->RendMap=REN_OrthXY;
			Viewport->Repaint( 1 );
			break;

		case ID_MapXZ:
			Viewport->Actor->RendMap=REN_OrthXZ;
			Viewport->Repaint( 1 );
			break;

		case ID_MapYZ:
			Viewport->Actor->RendMap=REN_OrthYZ;
			Viewport->Repaint( 1 );
			break;

		case ID_MapPolys:
			Viewport->Actor->RendMap=REN_Polys;
			Viewport->Repaint( 1 );
			break;

		case ID_MapPolyCuts:
			Viewport->Actor->RendMap=REN_PolyCuts;
			Viewport->Repaint( 1 );
			break;

		case ID_MapZones:
		{
			Viewport->Actor->RendMap=REN_Zones;
			Viewport->Repaint( 1 );
		}
		break;

		case ID_DepthComplexity:
			Viewport->Actor->RendMap=REN_DepthComplexity;
			Viewport->Repaint( 1 );
			break;

		case ID_LockToSelectedActor:
		{
			Viewport->bLockOnSelectedActors = !Viewport->bLockOnSelectedActors;
		}
		break;

		case ID_ShowLargeVertices:
		{
			Viewport->bShowLargeVertices = !Viewport->bShowLargeVertices;
			Viewport->Repaint( 1 );
		}
		break;
	}

	UpdateButtons();
	InvalidateRect( hWnd, NULL, FALSE );

	unguard;
}

struct
{
	INT ID;
	TCHAR ToolTip[64];
	INT Move;
} GVFButtons[] =
{
	IDMN_VF_REALTIME_PREVIEW, TEXT("Realtime Preview (P)"), 22,
	-1, TEXT(""), 11,
	ID_MapOverhead, TEXT("Top (Alt+7)"), 22,
	ID_MapXZ, TEXT("Front (Alt+8)"), 22,
	ID_MapYZ, TEXT("Side (Alt+9)"), 22,
	-1, TEXT(""), 11,
	ID_MapWire, TEXT("Perspective (Alt+1)"), 22,
	ID_MapPolys, TEXT("Texture Usage (Alt+3)"), 22,
	ID_MapPolyCuts, TEXT("BSP Cuts (Alt+4)"), 22,
	ID_MapPlainTex, TEXT("Textured (Alt+6)"), 22,
	ID_MapLighting, TEXT("Lighting Only (Alt+0)"), 22,
	ID_MapDynLight, TEXT("Dynamic Light (Alt+5)"), 22,
	ID_MapZones, TEXT("Zone/Portal (Alt+2)"), 22,
	ID_DepthComplexity, TEXT("Depth Complexity"), 22,
	-1, TEXT(""), 11,
	ID_LockToSelectedActor, TEXT("Lock to Selected Actor?"), 22,
	ID_ShowLargeVertices, TEXT("Show Large Vertices?"), 22,
	-2, TEXT(""), 0
};

//
// WViewportFrame
//

WViewportFrame::WViewportFrame( FName InPersistentName, WWindow* InOwnerWindow )
:	WWindow( InPersistentName, InOwnerWindow )
{
	Viewport = NULL;
	m_iIdx = INDEX_NONE;
	Caption = TEXT("");
	VFToolbar = NULL;
}

// WWindow interface.
void WViewportFrame::OpenWindow()
{
	guard(WViewportFrame::OpenWindow);
	MdiChild = 0;

	PerformCreateWindowEx
	(
		0,
		TEXT("Viewport"),
		(GViewportStyle == VSTYLE_Floating)
			? WS_OVERLAPPEDWINDOW | WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS
			: WS_CHILD | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		0,
		0,
		320,
		200,
		OwnerWindow ? OwnerWindow->hWnd : NULL,
		NULL,
		hInstance
	);

	VFToolbar = new WVFToolBar( TEXT("VFToolbar"), this );
	VFToolbar->OpenWindow();

	INT BmpPos = 0;
	for( INT x = 0 ; GVFButtons[x].ID != -2 ; ++x )
	{
		if( GVFButtons[x].ID != -1 )
		{
			VFToolbar->AddButton( GVFButtons[x].ToolTip, GVFButtons[x].ID,
				0, 0, 22, 20,
				(22*BmpPos), 0, 22, 20,
				(22*BmpPos), 20, 22, 20 );
			BmpPos++;
		}
	}
	AdjustToolbarButtons();

	VFToolbar->UpdateButtons();
	UpdateWindow();

	unguard;
}
// call this function when the viewportstyle changes to move buttons on the toolbar back and forth.
void WViewportFrame::AdjustToolbarButtons()
{
	guard(WViewportFrame::AdjustToolbarButtons);
	INT Button = 0, Pos = (GViewportStyle == VSTYLE_Floating ? 0 : 100 );
	WPictureButton* pButton;
	FDeferWindowPos dwp;
	for( INT x = 0 ; GVFButtons[x].ID != -2 ; ++x )
	{
		if( GVFButtons[x].ID != -1 )
		{
			pButton = &(VFToolbar->Buttons(Button));
			pButton->ClientPos.left = Pos;
			dwp.MoveWindow( pButton->hWnd, pButton->ClientPos.left, pButton->ClientPos.top, pButton->ClientPos.right, pButton->ClientPos.bottom, 1 );
			Button++;
		}
		Pos += GVFButtons[x].Move;
	}
	unguard;
}
void WViewportFrame::OnDestroy()
{
	guard(WViewportFrame::OnDestroy);
	DestroyWindow(VFToolbar->hWnd);
	delete VFToolbar;
	unguard;
}
void WViewportFrame::OnCreate()
{
	guard(WViewportFrame::OnCreate);
	WWindow::OnCreate();
	unguard;
}
void WViewportFrame::OnPaint()
{
	guard(WViewportFrame::OnPaint);

	UpdateWindow();

	PAINTSTRUCT PS;
	HDC hDC = BeginPaint( *this, &PS );
	FRect rectOutline = GetClientRect();
	HBRUSH brushOutline = CreateSolidBrush( RGB(192,192,192) );

	rectOutline.Min.X += 2;
	rectOutline.Min.Y += 2;
	rectOutline.Max.X -= 2;
	rectOutline.Max.Y -= 2;

	// The current viewport has a white border.
	if( Viewport == GUnrealEd->Client->GetLastCurrent() )
		FillRect( hDC, GetClientRect(), (HBRUSH)GetStockObject(WHITE_BRUSH) );
	else
		FillRect( hDC, GetClientRect(), (HBRUSH)GetStockObject(BLACK_BRUSH) );

	FillRect( hDC, rectOutline, brushOutline );

	EndPaint( *this, &PS );

	DeleteObject(brushOutline);
	unguard;
}
void WViewportFrame::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	guard(WViewportFrame::OnSize);
	WWindow::OnSize(Flags, NewX, NewY);

	FRect R = GetClientRect();
	FDeferWindowPos dwp;

	if( VFToolbar )
		dwp.MoveWindow( VFToolbar->hWnd, 3, 3, R.Max.X - 6, 24, 1 );

	RECT rcVFToolbar = { 0, 0, 0, 0 };
	if( VFToolbar )
		::GetClientRect( VFToolbar->hWnd, &rcVFToolbar );

	if( Viewport )
	{
		// Viewport should leave a border around the outside so we can show which viewport is active.
		// This border will be colored white if the viewport is active, black if not.

		R.Max.X -= 3;
		R.Max.Y -= 3;
		R.Min.X += 3;
		R.Min.Y += (rcVFToolbar.bottom - rcVFToolbar.top);
		dwp.MoveWindow( (HWND)Viewport->GetWindow(), R.Min.X, R.Min.Y, R.Width(), R.Height(), 1 );
		Viewport->Repaint( 1 );
	}

	InvalidateRect( hWnd, NULL, FALSE );
	unguard;
}
// Computes the viewport frames position data relative to the parent window
void WViewportFrame::ComputePositionData()
{
	guard(WViewportFrame::ComputePositionData);
	VIEWPORTCONFIG* pVC = &(GViewports(m_iIdx));

	// Get the size of the client area we are being put into.
	RECT rcParent;
	HWND hwndParent = GetParent(GetParent(hWnd));
	::GetClientRect(hwndParent, &rcParent);

	// Get the size of this window, in screen coordinates.
	RECT rcThis;
	POINT point;
	::GetWindowRect( hWnd, &rcThis );
	point.x = rcThis.left;	point.y = rcThis.top;		::ScreenToClient( hwndParent, &point );		rcThis.left = point.x;	rcThis.top = point.y;
	point.x = rcThis.right;	point.y = rcThis.bottom;	::ScreenToClient( hwndParent, &point );		rcThis.right = point.x;	rcThis.bottom = point.y;
	
	// Compute the sizing percentages for this window.
	//
	// Avoid divide by zero
	rcParent.right = max(rcParent.right, 1);
	rcParent.bottom = max(rcParent.bottom, 1);

	pVC->PctLeft = rcThis.left / (FLOAT)rcParent.right;
	pVC->PctTop = rcThis.top / (FLOAT)rcParent.bottom;
	pVC->PctRight = (rcThis.right / (FLOAT)rcParent.right) - pVC->PctLeft;
	pVC->PctBottom = (rcThis.bottom / (FLOAT)rcParent.bottom) - pVC->PctTop;

	// Clamp the percentages to be INT's ... this prevents the viewports from drifting
	// between sessions.
	pVC->PctLeft = appRound(pVC->PctLeft * 100.0f) / 100.0f;
	pVC->PctTop = appRound(pVC->PctTop * 100.0f) / 100.0f;
	pVC->PctRight = appRound(pVC->PctRight * 100.0f) / 100.0f;
	pVC->PctBottom = appRound(pVC->PctBottom * 100.0f) / 100.0f;

	pVC->Left = rcThis.left;
	pVC->Top = rcThis.top;
	pVC->Right = rcThis.right - pVC->Left;
	pVC->Bottom = rcThis.bottom - pVC->Top;

	unguard;
}
void WViewportFrame::OnKeyUp( WPARAM wParam, LPARAM lParam )
{
	guard(WViewportFrame::OnKeyUp);		
	// A hack to get the familiar hotkeys working again.  This should really go through
	// Proper Windows accelerators, but I can't get them to work.
	switch( wParam )
	{
		case VK_F4:
			GUnrealEd->ShowActorProperties();
			break;

		case VK_F5:
			GSurfPropSheet->Show( TRUE );
			break;

		case VK_F6:
			GUnrealEd->ShowLevelProperties();
			break;

		case VK_F7:
			GWarn->BeginSlowTask( TEXT("Compiling changed scripts"), 1 );
			GUnrealEd->Exec( TEXT("SCRIPT MAKE") );
			GWarn->EndSlowTask();
			break;

		case VK_F8:
			GBuildSheet->Show(1);
			break;

		case VK_DELETE:
			GUnrealEd->Exec( TEXT("DELETE") );
			break;
	}
	unguard;
}
void WViewportFrame::OnCommand( INT Command )
{
	guard(WViewportFrame::OnCommand);
	switch( Command ) {

		case WM_VIEWPORT_UPDATEWINDOWFRAME:
			UpdateWindow();
			break;

		case ID_MapDynLight:
			Viewport->Actor->RendMap=REN_DynLight;
			UpdateWindow();
			Viewport->Repaint( 1 );
			break;

		case ID_MapPlainTex:
			Viewport->Actor->RendMap=REN_PlainTex;
			UpdateWindow();
			Viewport->Repaint( 1 );
			break;

		case ID_MapLighting:
			Viewport->Actor->RendMap=REN_LightingOnly;
			UpdateWindow();
			Viewport->Repaint( 1 );
			break;

		case ID_MapWire:
			Viewport->Actor->RendMap=REN_Wire;
			UpdateWindow();
			Viewport->Repaint( 1 );
			break;

		case ID_MapOverhead:
			Viewport->Actor->RendMap=REN_OrthXY;
			UpdateWindow();
			Viewport->Repaint( 1 );
			break;

		case ID_MapXZ:
			Viewport->Actor->RendMap=REN_OrthXZ;
			UpdateWindow();
			Viewport->Repaint( 1 );
			break;

		case ID_MapYZ:
			Viewport->Actor->RendMap=REN_OrthYZ;
			UpdateWindow();
			Viewport->Repaint( 1 );
			break;

		case ID_MapPolys:
			Viewport->Actor->RendMap=REN_Polys;
			UpdateWindow();
			Viewport->Repaint( 1 );
			break;

		case ID_MapPolyCuts:
			Viewport->Actor->RendMap=REN_PolyCuts;
			UpdateWindow();
			Viewport->Repaint( 1 );
			break;

		case ID_MapZones:
			Viewport->Actor->RendMap=REN_Zones;
			UpdateWindow();
			Viewport->Repaint( 1 );
			break;

		case ID_DepthComplexity:
			Viewport->Actor->RendMap=REN_DepthComplexity;
			UpdateWindow();
			Viewport->Repaint( 1 );
			break;

		case ID_LockToSelectedActor:
			Viewport->bLockOnSelectedActors = !Viewport->bLockOnSelectedActors;
			UpdateWindow();
			Viewport->Repaint( 1 );
			break;

		case ID_ShowLargeVertices:
			Viewport->bShowLargeVertices = !Viewport->bShowLargeVertices;
			UpdateWindow();
			Viewport->Repaint( 1 );
			break;

		case ID_Color16Bit:
			Viewport->RenDev->SetRes( Viewport, Viewport->SizeX, Viewport->SizeY, 0 );
			Viewport->Repaint( 1 );
			break;

		case ID_Color32Bit:
			Viewport->RenDev->SetRes( Viewport, Viewport->SizeX, Viewport->SizeY, 0 );
			Viewport->Repaint( 1 );
			break;

		case ID_ShowBackdrop:
			Viewport->Actor->ShowFlags ^= SHOW_Backdrop;
			Viewport->Repaint( 1 );
			break;

		case ID_ShowSelectionHighlight:
			Viewport->Actor->ShowFlags ^= SHOW_SelectionHighlight;
			Viewport->Repaint( 1 );
			break;

		case ID_ActorsShow:
			Viewport->Actor->ShowFlags &= ~(SHOW_Actors | SHOW_ActorIcons | SHOW_ActorRadii | SHOW_ActorInfo);
			Viewport->Actor->ShowFlags |= SHOW_Actors; 
			Viewport->Repaint( 1 );
			break;

		case ID_ActorInfo:
			Viewport->Actor->ShowFlags &= ~(SHOW_Actors | SHOW_ActorIcons | SHOW_ActorRadii | SHOW_ActorInfo);
			Viewport->Actor->ShowFlags |= SHOW_Actors | SHOW_ActorInfo; 
			Viewport->Repaint( 1 );
			break;

		case ID_ActorsIcons:
			Viewport->Actor->ShowFlags &= ~(SHOW_Actors | SHOW_ActorIcons | SHOW_ActorRadii | SHOW_ActorInfo); 
			Viewport->Actor->ShowFlags |= SHOW_Actors | SHOW_ActorIcons;
			Viewport->Repaint( 1 );
			break;

		case ID_ActorsRadii:
			Viewport->Actor->ShowFlags &= ~(SHOW_Actors | SHOW_ActorIcons | SHOW_ActorRadii | SHOW_ActorInfo); 
			Viewport->Actor->ShowFlags |= SHOW_Actors | SHOW_ActorRadii;
			Viewport->Repaint( 1 );
			break;

		case ID_ActorsHide:
			Viewport->Actor->ShowFlags &= ~(SHOW_Actors | SHOW_ActorIcons | SHOW_ActorRadii | SHOW_ActorInfo); 
			Viewport->Repaint( 1 );
			break;

		case ID_ShowPaths:
			Viewport->Actor->ShowFlags ^= SHOW_Paths;
			Viewport->Repaint( 1 );
			break;

		case ID_ShowEventLines:
			Viewport->Actor->ShowFlags ^= SHOW_EventLines;
			Viewport->Repaint( 1 );
			break;

		case ID_ShowCoords:
			Viewport->Actor->ShowFlags ^= SHOW_Coords;
			Viewport->Repaint( 1 );
			break;

		case ID_ShowBrush:
			Viewport->Actor->ShowFlags ^= SHOW_Brush;
			Viewport->Repaint( 1 );
			break;

		case ID_ShowStaticMeshes:
			Viewport->Actor->ShowFlags ^= SHOW_StaticMeshes;
			Viewport->Repaint( 1 );
			break;

		case ID_ShowMovingBrushes:
			Viewport->Actor->ShowFlags ^= SHOW_MovingBrushes;
			Viewport->Repaint( 1 );
			break;

		case ID_ShowVolumes:
			Viewport->Actor->ShowFlags ^= SHOW_Volumes;
			Viewport->Repaint( 1 );
			break;

		case ID_ShowTerrain:
			Viewport->Actor->ShowFlags ^= SHOW_Terrain;
			Viewport->Repaint( 1 );
			break;

		case ID_ShowDistanceFog:
			Viewport->Actor->ShowFlags ^= SHOW_DistanceFog;
			Viewport->Repaint( 1 );
			break;

		case ID_ShowMatRotations:
			Viewport->Actor->ShowFlags ^= SHOW_MatRotations;
			Viewport->Repaint( 1 );
			break;

		case ID_ShowMatPaths:
			Viewport->Actor->ShowFlags ^= SHOW_MatPaths;
			Viewport->Repaint( 1 );
			break;

		case ID_ShowKarmaPrimitives:
			Viewport->Actor->ShowFlags ^= SHOW_KarmaPrimitives;
			Viewport->Repaint( 1 );
			break;

		case ID_ShowFluidSurfaces:
			Viewport->Actor->ShowFlags ^= SHOW_FluidSurfaces;
			Viewport->Repaint( 1 );
			break;

		case ID_ShowCollision:
			Viewport->Actor->ShowFlags ^= SHOW_Collision;
			Viewport->Repaint( 1 );
			break;

		default:
			WWindow::OnCommand(Command);
			break;
	}
	unguard;
}
void WViewportFrame::UpdateWindow( void )
{
	guard(WViewportFrame::UpdateWindow);
	if( !Viewport || !Viewport->Actor ) 
	{
		Caption = TEXT("Viewport Frame");
		return;
	}

	switch( Viewport->Actor->RendMap )
	{
		case REN_Wire:
			Caption = TEXT("Wireframe");
			break;

		case REN_Zones:
			Caption = TEXT("Zone/Portal");
			break;

		case REN_Polys:
			Caption = TEXT("Texture Use");
			break;

		case REN_PolyCuts:
			Caption = TEXT("BSP Cuts");
			break;

		case REN_DynLight:
			Caption = TEXT("Dynamic Light");
			break;

		case REN_PlainTex:
			Caption = TEXT("Textured");
			break;

		case REN_LightingOnly:
			Caption = TEXT("Lighting Only");
			break;

		case REN_OrthXY:
			Caption = TEXT("Top");
			break;

		case REN_OrthXZ:
			Caption = TEXT("Front");
			break;

		case REN_OrthYZ:
			Caption = TEXT("Side");
			break;

		case REN_StaticMeshBrowser:
			Caption = TEXT("Static Mesh Browser");
			break;

		case REN_MeshView:
			Caption = TEXT("Mesh Viewer");
			break;

		case REN_Animation:
			Caption = TEXT("Animation Viewer");
			break;

#ifdef WITH_LIPSINC
		case REN_LIPSinc:
			Caption = TEXT("Impersonator Browser");
			break;
#endif

		case REN_MaterialEditor:
			Caption = TEXT("Material Editor");
			break;

		case REN_Prefab:
		case REN_PrefabCompiled:
			Caption = TEXT("Prefab Browser");
			break;

		default:
			Caption = TEXT("Unknown");
			break;
	}

	SetText(*Caption);
	VFToolbar->SetCaption( Caption );
	VFToolbar->UpdateButtons();

	unguard;
}
void WViewportFrame::SetViewport( UViewport* pViewport )
{
	guard(WViewportFrame::SetViewport);
	Viewport = pViewport;

	FRect R = GetClientRect();
	Viewport->OpenWindow( (DWORD)hWnd, 0, R.Width(), R.Height(), R.Min.X, R.Min.Y );

	// Forces things to set themselves up properly when the viewport is first assigned.
	OnSize( SIZE_MAXSHOW, R.Max.X, R.Max.Y );
	VFToolbar->SetViewport(pViewport);

	UpdateWindow();
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

