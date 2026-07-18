/*=============================================================================
	WMdiFrame : The base class for the frame window for the editor
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

class WMdiFrame : public WWindow
{
	DECLARE_WINDOWCLASS(WMdiFrame,WWindow,UnrealEd)

	// Variables.
	WMdiClient MdiClient;
	WDockingFrame LeftFrame, BottomFrame, TopFrame;

	// Functions.
	WMdiFrame( FName InPersistentName )
	:	WWindow		( InPersistentName )
	,	MdiClient	( this )
	,	BottomFrame	( TEXT("MdiFrameBottom"), this, 32 )
	,	LeftFrame	( TEXT("MdiFrameLeft"), this, 68 )
	,	TopFrame	( TEXT("MdiFrameTop"), this, 32 )
	{}
	void OnCreate()
	{
		guard(WMdiFrame::OnCreate);
		WWindow::OnCreate();

		// Create docking frames.
		BottomFrame.OpenWindow();
		LeftFrame.OpenWindow();
		TopFrame.OpenWindow();

		unguard;
	}
	virtual void RepositionClient()
	{
		guard(WMdiFrame::RepositionClient);

		// Reposition docking frames.
		FRect Client = GetClientRect();
		BottomFrame.MoveWindow( FRect(LeftFrame.DockDepth, Client.Max.Y-BottomFrame.DockDepth, Client.Max.X, Client.Max.Y), 1 );
		LeftFrame.MoveWindow( FRect(0, TopFrame.DockDepth, LeftFrame.DockDepth, Client.Max.Y), 1 );
		TopFrame.MoveWindow( FRect(0, 0, Client.Max.X, TopFrame.DockDepth), 1 );

		// Reposition MDI client window.
		MdiClient.MoveWindow( FRect(LeftFrame.DockDepth, TopFrame.DockDepth, Client.Max.X, Client.Max.Y-BottomFrame.DockDepth), 1 );

		unguard;
	}
	void OnSize( DWORD Flags, INT NewX, INT NewY )
	{
		guard(WMdiFrame::OnSize);
		RepositionClient();
		throw TEXT("NoRoute");
		unguard;
	}
	void OpenWindow()
	{
		guard(WMdiFrame::OpenWindow);
		TCHAR Title[256];
		appSprintf( Title, LocalizeGeneral(TEXT("FrameWindow"),TEXT("UnrealEd")), LocalizeGeneral(TEXT("Product"),TEXT("Core")) );
		PerformCreateWindowEx
		(
			WS_EX_APPWINDOW,
			Title,
			WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_SIZEBOX | WS_MAXIMIZEBOX | WS_MINIMIZEBOX,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			640,
			480,
			NULL,
			NULL,
			hInstance
		);
		//ShowWindow( *this, SW_SHOWMAXIMIZED );
		unguard;
	}
	void OnSetFocus()
	{
		guard(WMdiFrame::OnSetFocus);
		SetFocus( MdiClient );
		unguard;
	}
};

