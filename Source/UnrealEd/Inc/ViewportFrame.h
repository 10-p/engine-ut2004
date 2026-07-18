/*=============================================================================
	ViewportFrame : Simple window to hold a viewport into a level
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

class WVFToolBar : public WWindow
{
	DECLARE_WINDOWCLASS(WVFToolBar,WWindow,Window)

	TArray<WPictureButton> Buttons;
	HBITMAP hbm;
	BITMAP bm;
	FString Caption;
	UViewport* Viewport;
	HBRUSH brushBack;
	HPEN penLine;

	// Structors.
	WVFToolBar( FName InPersistentName, WWindow* InOwnerWindow );

	// WWindow interface.
	void OpenWindow();
	void OnDestroy();
	void OnCreate();
	void SetCaption( FString InCaption );
	void OnPaint();
	void AddButton( FString InToolTip, INT InID, 
		INT InClientLeft, INT InClientTop, INT InClientRight, INT InClientBottom,
		INT InBmpOffLeft, INT InBmpOffTop, INT InBmpOffRight, INT InBmpOffBottom,
		INT InBmpOnLeft, INT InBmpOnTop, INT InBmpOnRight, INT InBmpOnBottom );
	void OnSize( DWORD Flags, INT NewX, INT NewY );
	void SetViewport( UViewport* pViewport );
	void OnRightButtonUp();
	void UpdateButtons();
	void OnCommand( INT Command );
	void ButtonClicked( INT ID );
};


class WViewportFrame : public WWindow
{
	DECLARE_WINDOWCLASS(WViewportFrame,WWindow,Window)

	UViewport* Viewport;	// The viewport that this frame contains
	INT m_iIdx;				// Index into the global TArray of viewport frames (GViewports)
	FString Caption;
	HBITMAP bmpToolbar;
	WVFToolBar* VFToolbar;
	
	// Structors.
	WViewportFrame( FName InPersistentName, WWindow* InOwnerWindow );

	// WWindow interface.
	void OpenWindow();

	void AdjustToolbarButtons();
	void OnDestroy();
	void OnCreate();
	void OnPaint();
	void OnSize( DWORD Flags, INT NewX, INT NewY );
	void ComputePositionData();
	void OnKeyUp( WPARAM wParam, LPARAM lParam );
	void OnCommand( INT Command );
	void UpdateWindow( void );
	void SetViewport( UViewport* pViewport );
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

