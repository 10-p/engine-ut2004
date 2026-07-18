/*=============================================================================
	WDockingFrame : Windows that can dock at the sides of the main editor window
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

class WDockingFrame : public WWindow
{
	DECLARE_WINDOWCLASS(WDockingFrame,WWindow,UnrealEd)

	// Variables.
	INT DockDepth;
	WWindow* Child;

	// Functions.
	WDockingFrame( FName InPersistentName, WMdiFrame* InFrame, INT InDockDepth )
	:	WWindow			( InPersistentName, (WWindow*)InFrame )
	,   DockDepth       ( InDockDepth )
	,	Child			( NULL )
	{}
	void OpenWindow()
	{
		guard(WDockingFrame::OpenWindow);
		PerformCreateWindowEx
		(
			0,
			NULL,
			WS_CHILD,
			0, 0, 0, 0,
			OwnerWindow->hWnd,
			NULL,
			hInstance
		);
		Show(1);
		unguard;
	}
	void Dock( WWindow* InChild )
	{
		guard(WDockingFrame::Dock);
		Child = InChild;
		unguard;
	}
	void OnSize( DWORD Flags, INT InX, INT InY )
	{
		guard(WDockingFrame::OnSize);
		if( Child )
			Child->MoveWindow( GetClientRect(), TRUE );
		unguard;
	}
};

