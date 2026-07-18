/*=============================================================================
	BottomBar : Container window for things that get docked at the bottom
	            of the editor window.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

#include "BottomBarStandard.h"

extern WMdiFrame* GMdiFrame;

class WBottomBar : public WWindow
{
	DECLARE_WINDOWCLASS(WBottomBar,WWindow,Window)

	WDockingFrame* DockingFrame;	// The docking frame that this window is docked in

	WBottomBarStandard* BottomBarStandard;

	// Structors.
	WBottomBar( FName InPersistentName, WWindow* InOwnerWindow, WDockingFrame* InDockingFrame )
	:	WWindow( InPersistentName, InOwnerWindow )
	,	DockingFrame(InDockingFrame)
	{
	}
	void OpenWindow()
	{
		guard(WBottomBar::OpenWindow);
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
		unguard;
	}
	void OnCreate()
	{
		guard(WBottomBar::OnCreate);
		WWindow::OnCreate();

		BottomBarStandard = new WBottomBarStandard( TEXT("BottomBarStandard"), this );
		BottomBarStandard->OpenWindow();

		PositionChildControls();

		unguard;
	}
	void OnDestroy()
	{
		guard(WBottomBar::OnDestroy);
		WWindow::OnDestroy();

		::DestroyWindow( BottomBarStandard->hWnd );
		
		delete BottomBarStandard;

		unguard;
	}
	void Refresh()
	{
		PositionChildControls();
		if( GMdiFrame )
			GMdiFrame->RepositionClient();
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WBottomBar::OnSize);
		WWindow::OnSize(Flags, NewX, NewY);
		PositionChildControls();
		InvalidateRect( hWnd, NULL, FALSE );
		unguard;
	}
	void PositionChildControls( void )
	{
		guard(WBottomBar::PositionChildControls);

		RECT rc;
		::GetClientRect( hWnd, &rc );

		switch( GUnrealEd->Mode )
		{
			default:
			{
				DockingFrame->DockDepth = 36;
				PositionStandardWindow( rc );
			}
			break;
		}

		unguard;
	}
	void PositionStandardWindow( RECT& InRect )
	{
		BottomBarStandard->MoveWindow( FRect(InRect.left+1, InRect.bottom - 36, InRect.left + 1024, InRect.bottom-1), 1 ); // gam
	}
	void OnPaint()
	{
		guard(WBottomBar::OnPaint);
		PAINTSTRUCT PS;
		HDC hDC = BeginPaint( *this, &PS );

		FRect Rect = GetClientRect();
		FillRect( hDC, Rect, hBrushGrey );
		MyDrawEdge( hDC, Rect, 1 );

		EndPaint( *this, &PS );

		unguard;
	}
	INT OnSetCursor()
	{
		guard(WBottomBar::OnSetCursor);
		WWindow::OnSetCursor();
		SetCursor(LoadCursorIdX(NULL,IDC_ARROW));
		return 0;
		unguard;
	}
	void OnCommand( INT Command )
	{
		guard(WBottomBar::OnCommand);

		switch( Command )
		{
			case WM_REFRESH:
			{
				Refresh();
			}
			break;
		}

		unguard;
	}
	// Called when something has happened to the editor to invalidate the contents of the various tools in this window.
	// (file new, file load, etc)
	void Reset()
	{
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

