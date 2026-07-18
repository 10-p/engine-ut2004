/*=============================================================================
	BrowserLIPSinc.h : Browser window for LIPSinc Animations
	
	  Revision history:
		* Created by Jamie Redmond
		* Modifications by John Briggs

    Work-in-progress TODO's:
		
		  
=============================================================================*/

#ifdef WITH_LIPSINC

#include "..\\..\\ImpersonatorLib\\include\\ImpersonatorLib.h"

struct LIPSincToolTipStrings
{
	TCHAR ToolTip[64];
	INT ID;
};

class WBrowserLIPSinc : public WBrowser
{
	DECLARE_WINDOWCLASS(WBrowserLIPSinc,WBrowser,Window)

	HWND hWndToolBar;

	FContainer Container;
	TMap<DWORD,FWindowAnchor> Anchors;

	WToolTip *ToolTipCtrl;

	// Combo boxes
	WComboBox *PackageCombo;
	WComboBox *MeshCombo;
	WComboBox *UnrealAnimPackageCombo;
	WComboBox *UnrealAnimCombo;

	// List boxes
	WListBox *LIPSincAnimList;

	WLabel *AnimListLabel;
	WLabel *UnrealAnimListLabel;
	WLabel* ViewportLabel;
	WLabel* RegisteredToLabel;
	
	WPropertySheet* PropSheet;
	WPropertyPage*  LIPSincControllerPage;
	WPropertyPage*  LIPSincAnimPage;
	WPropertyPage*  LIPSincPrefsPage;
	
	WObjectProperties* LIPSincControllerPropertyWindow;
	WObjectProperties* LIPSincAnimPropertyWindow;
	WObjectProperties* LIPSincPrefsPropertyWindow;

	ULIPSincControllerProps *LIPSincControllerProps;
	ULIPSincAnimProps		*LIPSincAnimProps;
	ULIPSincPrefsProps		*LIPSincPrefsProps;
	
	// Group Boxes
	WGroupBox* LIPSincAnimGroupBox;

	// Splitter Container
	WSplitterContainer* SplitterContainer;

	// Check Boxes
	WCheckBox* BlendWithCheck;
	
	// Scrub bar
	WTrackBar* ScrubBar;

	// Buttons
	WButton* PlayButton;
	WButton* StopButton;

	// Bitmaps
	HBITMAP	PlayBitmap;
	HBITMAP StopBitmap;
	HBITMAP PauseBitmap;

	UViewport* Viewport;
	
	// We need a level just like the Animation Browser so that notifies show up correctly
	ULevel* LIPSincBrowserLevel;
	AActor* MeshActor;
	USkeletalMesh* WorkMesh;

	UBOOL bPlayJustStarted;
	UBOOL bRefPose;
	UBOOL bWireframe;
	UBOOL bPrintBones;
	UBOOL bBackface;
	UBOOL bPlaying;
	UBOOL bPlayingLIPSinc;
	UBOOL bPausedLIPSinc;
	UBOOL bShowTargetStats;

	FLOAT FrameTime;
	FLOAT OldFrameTime;

	// Serialize function: needed to associate the custom objects with THIS window so they don't get GC'd at the wrong time; when a level is loaded etc.
	virtual void Serialize( FArchive& Ar );

	// Structors.
	WBrowserLIPSinc( FName InPersistentName, WWindow* InOwnerWindow, HWND InEditorFrame );

	// WBrowser Interface
	void OpenWindow( UBOOL bChild );
	virtual void UpdateMenu();
	void OnCreate();
	virtual void RefreshAll();
	void RefreshPackages( void );
	void RefreshMeshList();
	void RefreshLIPSincAnimList();
	void RefreshUnrealAnimList();
	void RefreshViewport();
	void InitViewActor();
	void CleanupLevel();
	void Draw( UViewport* Viewport );
	void PositionChildControls();
	void OnDestroy();
	void OnSize( DWORD Flags, INT NewX, INT NewY );
	virtual USkeletalMeshInstance* CurrentMeshInstance();
	virtual USkeletalMesh* CurrentSkelMesh();
	void OnCommand( INT Command );
	void OnPackageSelectionChange();
	void OnMeshSelectionChange();
	void OnUnrealAnimListSelectionChange();
	void OnUnrealAnimPackageSelectionChange();
	void OnLIPSincAnimListDoubleClick();
	void OnLIPSincAnimListSelectionChange();
	void OnUnrealAnimListDoubleClick();
	int RunAnalysis( FString WavFilename, FString AnimName, FString PkgName, UBOOL bText, FString Text = TEXT("") );
	void OnLIPSincAnimListRightClick();
	void BatchProcessLIPSinc();
	void OnBlendWithChange();
	
	void RefreshLIPSincControllerProperties();
	void SaveLIPSincControllerProperties();
	void RefreshLIPSincAnimProperties();
	void SaveLIPSincAnimProperties();
	void RefreshLIPSincPrefsProperties();
	void SaveLIPSincPrefsProperties();

	void OnPlayButton();
	void OnStopButton();
	void RefreshUnrealAnimPackageList();
	void OnUnrealAnimSelectionChange();
	void OnLIPSincGenerateAnimation();
	void OnLIPSincImportLBP();
	void OnLIPSincImportLTF( UBOOL bUpdateExisting = 1, FString InLTFFile = TEXT("") );
	void OnLIPSincQuickLoadLTF();
	void OnLIPSincRenameAnimation();
	void OnLIPSincImportExpression();
	void OnLIPSincDeleteExpression();
	void OnLIPSincExportExpressions();
	void StopLIPSincAnimation();

	void OnSliderMove();
	
	void OnPaint( );
};

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
