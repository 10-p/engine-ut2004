/*=============================================================================
	SurfacePropSheet : Property sheet for surface properties.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/


// --------------------------------------------------------------
//
// WSurfacePropPage
//
// Base class for all the pages on this sheet.
//
// --------------------------------------------------------------

class WSurfacePropPage : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WSurfacePropPage,WPropertyPage,Window)

	WSurfacePropPage ( WWindow* InOwnerWindow );
	virtual void Refresh();
};

// --------------------------------------------------------------
//
// WPageFlags
//
// --------------------------------------------------------------


class WPageFlags : public WSurfacePropPage
{
	DECLARE_WINDOWCLASS(WPageFlags,WSurfacePropPage,Window)

	WCheckBox*	InvisibleCheck;
	WCheckBox*	FakeBackdropCheck;
	WCheckBox*	TwoSidedCheck;
	WCheckBox*	SpecialLitCheck;
	WCheckBox*	UnlitCheck;
	WCheckBox*	PortalCheck;
	WCheckBox*	AntiPortalCheck;
	WCheckBox*	MirrorCheck;

	// Structors.
	WPageFlags ( WWindow* InOwnerWindow );

	virtual void OpenWindow( INT InDlgId, HMODULE InHMOD );

	void OnDestroy();
	virtual void Refresh();
	void OnButtonClicked();
	void GetDataFromSurfs();
	void SendDataToSurfs();
};


// --------------------------------------------------------------
//
// WPagePanRotScale
//
// --------------------------------------------------------------

class WPagePanRotScale : public WSurfacePropPage
{
	DECLARE_WINDOWCLASS(WPagePanRotScale,WSurfacePropPage,Window)

	WCheckBox *RelativeCheck;
	WGroupBox *PanBox, *AlignmentBox, *OptionsBox;
	WButton *PanU1Button, *PanU4Button, *PanU16Button, *PanU64Button,
		*PanV1Button, *PanV4Button, *PanV16Button, *PanV64Button,
		*Rot45Button, *Rot90Button, *FlipUButton, *FlipVButton,
		*ApplyButton, *Apply2Button;
	WComboBox *SimpleScaleCombo;
	WEdit *ScaleUEdit, *ScaleVEdit;

	WComboBox*	LightMapScaleCombo;

	// Structors.
	WPagePanRotScale ( WWindow* InOwnerWindow );

	virtual void OpenWindow( INT InDlgId, HMODULE InHMOD );
	void OnDestroy();
	virtual void Refresh();
	void OnLightMapScaleChanged();
	void PanU( INT InPan );
	void OnPanU1Clicked() { PanU(1); }
	void OnPanU4Clicked() { PanU(4); }
	void OnPanU16Clicked() { PanU(16); }
	void OnPanU64Clicked() { PanU(64); }
	void PanV( INT InPan );
	void OnPanV1Clicked() { PanV(1); }
	void OnPanV4Clicked() { PanV(4); }
	void OnPanV16Clicked() { PanV(16); }
	void OnPanV64Clicked() { PanV(64); }
	void Scale( FLOAT InScaleU, FLOAT InScaleV, UBOOL InRelative );
	void OnApplyClicked();
	void OnApply2Clicked();
	void OnFlipUClicked();
	void OnFlipVClicked();
	void OnRot45Clicked();
	void OnRot90Clicked();
};

// --------------------------------------------------------------
//
// WPageAlignment
//
// --------------------------------------------------------------

class WPageAlignment : public WSurfacePropPage
{
	DECLARE_WINDOWCLASS(WPageAlignment,WSurfacePropPage,Window)

	WObjectProperties* PropertyWindow;
	WGroupBox *AlignmentBox, *OptionsBox;
	WButton *AlignButton;
	WListBox *AlignList;
	UOptionsProxy* Proxy;

	// Structors.
	WPageAlignment ( WWindow* InOwnerWindow );

	virtual void OpenWindow( INT InDlgId, HMODULE InHMOD );
	void RefreshPropertyWindow();
	void OnDestroy();
	void FlushFields();
	virtual void Refresh();
	void AlignListSelectionChange();
	void OnAlignClick();
	void Align( ETexAlign InTexAlign );
};

// --------------------------------------------------------------
//
// WPageStats
//
// --------------------------------------------------------------

class WPageStats : public WSurfacePropPage
{
	DECLARE_WINDOWCLASS(WPageStats,WSurfacePropPage,Window)

	WGroupBox *LightingBox;
	WLabel *StaticLightsLabel, *MeshelsLabel, *MeshSizeLabel;

	// Structors.
	WPageStats ( WWindow* InOwnerWindow );

	virtual void OpenWindow( INT InDlgId, HMODULE InHMOD );
	void OnDestroy();
	virtual void Refresh();
};

// --------------------------------------------------------------
//
// WSurfacePropSheet
//
// --------------------------------------------------------------

class WSurfacePropSheet : public WWindow
{
	DECLARE_WINDOWCLASS(WSurfacePropSheet,WWindow,Window)

	WPropertySheet* PropSheet;
	WPageFlags* FlagsPage;
	WPagePanRotScale* PanRotScalePage;
	WPageAlignment* AlignmentPage;
	WPageStats* StatsPage;

	// Structors.
	WSurfacePropSheet( FName InPersistentName, WWindow* InOwnerWindow );

	// WSurfacePropSheet interface.
	void OpenWindow();
	void OnCreate();
	void OnDestroy();
	void OnSize( DWORD Flags, INT NewX, INT NewY );
	void PositionChildControls();
	INT OnSysCommand( INT Command );
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

