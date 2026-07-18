/*=============================================================================
	TexProp : Properties of a texture
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

extern HWND GBrowserTextureHwnd;
extern INT GLastViewportNum;

class WDlgTexProp : public WDialog, public FNotifyHook
{
	DECLARE_WINDOWCLASS(WDlgTexProp,WDialog,UnrealEd)

	// Variables.
	TMap<DWORD,FWindowAnchor> Anchors;

	FContainer *Container;
	WButton ClearButton;
	WCheckBox ShowFallback, ShowBackdrop, ShowTopLevel, FillButton, CubeButton, SphereButton;
	WScrollBar ScrollBar;
	FString ViewportName, MaterialsViewportName;
	UViewport *Viewport, *MaterialsViewport;
	FString RenDevice;
	UMaterial *Material, *MaterialTreeCurrent;
	WObjectProperties* pProps;
	HBITMAP PlaneBitmap, CubeBitmap, SphereBitmap, FallbackBitmap, BackdropBitmap, TopLevelBitmap;
	WButton ErrorButton;
	WLabel ErrorLabel;
	FString LastError;
	UMaterial* ErrorMaterial;

	UStaticMesh* StaticMesh;
	INT OldUSize, OldVSize;

	// Constructor.
	WDlgTexProp( UObject* InContext, WWindow* InOwnerWindow, UMaterial* InTexture );

	// WDialog interface.
	void OnInitDialog();
	void SetCaption( UMaterial* InMaterial );
	void RefreshViewports();
	void OnDestroy();
	void PositionChildControls();
	void OnSize( DWORD Flags, INT NewX, INT NewY );
	virtual void DoModeless( UBOOL bShow );
	void OnClear();
	void OnShowFallback();
	void OnShowBackdrop();
	void OnShowTopLevel();
	void OnFill();
	void OnCube();
	void OnSphere();
	void OnError();
	virtual void Serialize( FArchive& Ar );
	void MaterialTreeClick( UMaterial* InMaterial );
	virtual void OnVScroll( WPARAM wParam, LPARAM lParam );
	void RefreshScrollBar();
	

	// FNotifyHook interface
	void NotifyDestroy( void* Src );
	void NotifyPreChange( void* Src );
	void NotifyExec( void* Src, const TCHAR* Cmd );
	void NotifyPostChange( void* Src );

	// WDlgTexProp interface
	void Render_TexView( UViewport* Viewport );
	void SizeTexViewport();
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

