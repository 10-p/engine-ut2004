/*=============================================================================
	SurfacePropSheet : Property sheet for surface properties.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

#include "UnrealEd.h"

// --------------------------------------------------------------
//
// WSurfacePropPage
//
// Base class for all the pages on this sheet.
//
// --------------------------------------------------------------

WSurfacePropPage::WSurfacePropPage ( WWindow* InOwnerWindow )
:	WPropertyPage( InOwnerWindow )
{}

void WSurfacePropPage::Refresh()
{
	guard(WSurfacePropPage::Refresh);

	// Figure out a good caption for the sheet.
	FStringOutputDevice GetPropResult = FStringOutputDevice();
	GetPropResult.Empty();
	if( GUnrealEd )
		GUnrealEd->Get( TEXT("POLYS"), TEXT("NUMSELECTED"), GetPropResult );
	INT NumSelected = appAtoi(*GetPropResult);

	// Disable all controls if no surfaces are selected.
	HWND hwndChild = GetWindow( hWnd, GW_CHILD );
	while( hwndChild )
	{
		EnableWindow( hwndChild, NumSelected );
		hwndChild = GetWindow( hwndChild, GW_HWNDNEXT );
	}

	unguard;
}


// --------------------------------------------------------------
//
// WPageFlags
//
// --------------------------------------------------------------

#define dNUM_FLAGS	8
struct {
	INT Flag;		// Unreal's bit flag
	INT ID;			// Windows control ID
	INT Count;		// Temp var
} GPolyFlags[] = {
	PF_Invisible,			IDCK_INVISIBLE,			0,
	PF_FakeBackdrop,		IDCK_FAKEBACKDROP,		0,
	PF_TwoSided,			IDCK_2SIDED,			0,
	PF_SpecialLit,			IDCK_SPECIALLIT,		0,
	PF_Unlit,				IDCK_UNLIT,				0,
	PF_Portal,				IDCK_PORTAL,			0,
	PF_AntiPortal,			IDCK_ANTIPORTAL,		0,
	PF_Mirrored,			IDCK_MIRROR,			0
};

// Structors.
WPageFlags::WPageFlags ( WWindow* InOwnerWindow )
:	WSurfacePropPage( InOwnerWindow )
{
	InvisibleCheck = FakeBackdropCheck = TwoSidedCheck = SpecialLitCheck = UnlitCheck = AntiPortalCheck = PortalCheck = MirrorCheck = NULL;
}

void WPageFlags::OpenWindow( INT InDlgId, HMODULE InHMOD )
{
	guard(WPageFlags::OpenWindow);
	WSurfacePropPage::OpenWindow( InDlgId, InHMOD );

	// Create child controls and let the base class determine their proper positions.
	InvisibleCheck = new WCheckBox( this, IDCK_INVISIBLE );
	InvisibleCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	FakeBackdropCheck = new WCheckBox( this, IDCK_FAKEBACKDROP );
	FakeBackdropCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	TwoSidedCheck = new WCheckBox( this, IDCK_2SIDED );
	TwoSidedCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	SpecialLitCheck = new WCheckBox( this, IDCK_SPECIALLIT );
	SpecialLitCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	UnlitCheck = new WCheckBox( this, IDCK_UNLIT );
	UnlitCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	PortalCheck = new WCheckBox( this, IDCK_PORTAL );
	PortalCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	AntiPortalCheck = new WCheckBox( this, IDCK_ANTIPORTAL );
	AntiPortalCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	MirrorCheck = new WCheckBox( this, IDCK_MIRROR );
	MirrorCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );

	PlaceControl( InvisibleCheck );
	PlaceControl( FakeBackdropCheck );
	PlaceControl( TwoSidedCheck );
	PlaceControl( SpecialLitCheck );
	PlaceControl( UnlitCheck );
	PlaceControl( PortalCheck );
	PlaceControl( AntiPortalCheck );
	PlaceControl( MirrorCheck );

	Finalize();

	// Delegates.
	InvisibleCheck->ClickDelegate = FDelegate(this, (TDelegate)OnButtonClicked);
	FakeBackdropCheck->ClickDelegate = FDelegate(this, (TDelegate)OnButtonClicked);
	TwoSidedCheck->ClickDelegate = FDelegate(this, (TDelegate)OnButtonClicked);
	SpecialLitCheck->ClickDelegate = FDelegate(this, (TDelegate)OnButtonClicked);
	UnlitCheck->ClickDelegate = FDelegate(this, (TDelegate)OnButtonClicked);
	PortalCheck->ClickDelegate = FDelegate(this, (TDelegate)OnButtonClicked);
	AntiPortalCheck->ClickDelegate = FDelegate(this, (TDelegate) OnButtonClicked);
	MirrorCheck->ClickDelegate = FDelegate(this, (TDelegate)OnButtonClicked);

	unguard;
}
void WPageFlags::OnDestroy()
{
	guard(WPageFlags::OnDestroy);
	WSurfacePropPage::OnDestroy();

	delete InvisibleCheck;
	delete FakeBackdropCheck;
	delete TwoSidedCheck;
	delete SpecialLitCheck;
	delete UnlitCheck;
	delete PortalCheck;
	delete AntiPortalCheck;
	delete MirrorCheck;

	unguard;
}
void WPageFlags::Refresh()
{
	guard(WPageFlags::Refresh);
	WSurfacePropPage::Refresh();

	GetDataFromSurfs();

	// Figure out a good caption for the sheet.
	FStringOutputDevice GetPropResult = FStringOutputDevice();
	GetPropResult.Empty();
	if( GUnrealEd )
		GUnrealEd->Get( TEXT("POLYS"), TEXT("NUMSELECTED"), GetPropResult );
	INT NumSelected = appAtoi(*GetPropResult);

	// Change caption to show how many surfaces are selected.
	GetPropResult.Empty();
	if( GUnrealEd )
		GUnrealEd->Get( TEXT("POLYS"), TEXT("MATERIALNAME"), GetPropResult );
	FString Caption;
	if( NumSelected == 1 )
		Caption = FString::Printf(TEXT("%d Surface%s%s"), NumSelected, GetPropResult.Len() ? TEXT(" : ") : TEXT(""), *GetPropResult );
	else
		Caption = FString::Printf(TEXT("%d Surfaces%s%s"), NumSelected, GetPropResult.Len() ? TEXT(" : ") : TEXT(""), *GetPropResult );
	if( GetParent(GetParent(GetParent(hWnd))) )
		SendMessageA( GetParent(GetParent(GetParent(hWnd))), WM_SETTEXT, 0, (LPARAM)appToAnsi( *Caption ) );

	unguard;
}
void WPageFlags::OnButtonClicked()
{
	guard(WPageFlags::OnButtonClicked);

	HWND hwndButton = (HWND)LastlParam;

	if( SendMessageA( hwndButton, BM_GETCHECK, 0, 0 ) == BST_CHECKED )
		SendMessageA( hwndButton, BM_SETCHECK, BST_CHECKED, 0 );
	else
		SendMessageA( hwndButton, BM_SETCHECK, BST_UNCHECKED, 0 );

	SendDataToSurfs();

	unguard;
}
void WPageFlags::GetDataFromSurfs()
{
	guard(WPageFlags::GetDataFromSurfs);

	INT TotalSurfs = 0;

	// Init counts.
	//
	for( INT x = 0 ; x < dNUM_FLAGS ; ++x )
		GPolyFlags[x].Count = 0;

	// Check to see which flags are used on all selected surfaces.
	//
	for( INT i = 0 ; i < GUnrealEd->Level->Model->Surfs.Num() ; ++i )
	{
		FBspSurf *Poly = &GUnrealEd->Level->Model->Surfs(i);
		if( Poly->PolyFlags & PF_Selected )
		{
			for( INT x = 0 ; x < dNUM_FLAGS ; ++x )
				if( Poly->PolyFlags & GPolyFlags[x].Flag )
					GPolyFlags[x].Count++;
			TotalSurfs++;
		}
	}

	// Update checkboxes on dialog to match selections.
	//
	for( INT x = 0 ; x < dNUM_FLAGS ; ++x )
	{
		SendMessageA( GetDlgItem( hWnd, GPolyFlags[x].ID ), BM_SETCHECK, BST_UNCHECKED, 0 );

		if( TotalSurfs > 0
				&& GPolyFlags[x].Count > 0 )
		{
			if( GPolyFlags[x].Count == TotalSurfs )
				SendMessageA( GetDlgItem( hWnd, GPolyFlags[x].ID ), BM_SETCHECK, BST_CHECKED, 0 );
			else
				SendMessageA( GetDlgItem( hWnd, GPolyFlags[x].ID ), BM_SETCHECK, BST_INDETERMINATE, 0 );
		}
	}

	unguard;
}
void WPageFlags::SendDataToSurfs()
{    
	guard(WPageFlags::SendDataToSurfs);

	INT OnFlags, OffFlags;

	OnFlags = OffFlags = 0;

	for( INT x = 0 ; x < dNUM_FLAGS ; ++x )
	{
		if( SendMessageA( GetDlgItem( hWnd, GPolyFlags[x].ID ), BM_GETCHECK, 0, 0 ) == BST_CHECKED )
			OnFlags += GPolyFlags[x].Flag;
		if( SendMessageA( GetDlgItem( hWnd, GPolyFlags[x].ID ), BM_GETCHECK, 0, 0 ) == BST_UNCHECKED )
			OffFlags += GPolyFlags[x].Flag;
	}

	GUnrealEd->Exec( *FString::Printf(TEXT("POLY SET SETFLAGS=%d CLEARFLAGS=%d"), OnFlags, OffFlags));

	unguard;
}

// --------------------------------------------------------------
//
// WPagePanRotScale
//
// --------------------------------------------------------------

WPagePanRotScale::WPagePanRotScale ( WWindow* InOwnerWindow )
:	WSurfacePropPage( InOwnerWindow )
{
	RelativeCheck = NULL;
	PanBox = AlignmentBox = OptionsBox = NULL;
	PanU1Button = PanU4Button = PanU16Button = PanU64Button
		= PanV1Button = PanV4Button = PanV16Button = PanV64Button
		= Rot45Button = Rot90Button = FlipUButton = FlipVButton
		= ApplyButton = Apply2Button = NULL;
	SimpleScaleCombo = NULL;
	ScaleUEdit = ScaleVEdit = NULL;
	LightMapScaleCombo = NULL;
}

void WPagePanRotScale::OpenWindow( INT InDlgId, HMODULE InHMOD )
{
	guard(WPagePanRotScale::OpenWindow);
	WSurfacePropPage::OpenWindow( InDlgId, InHMOD );

	// Create child controls and let the base class determine their proper positions.
	RelativeCheck = new WCheckBox( this, IDCK_RELATIVE );
	RelativeCheck->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	PanBox = new WGroupBox( this, IDGP_PAN );
	PanBox->OpenWindow( 1, 0 );
	AlignmentBox = new WGroupBox( this, IDGP_ROTATION );
	AlignmentBox->OpenWindow( 1, 0 );
	OptionsBox = new WGroupBox( this, IDGP_SCALING );
	OptionsBox->OpenWindow( 1, 0 );
	PanU1Button = new WButton( this, IDPB_PAN_U_1 );
	PanU1Button->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	PanU4Button = new WButton( this, IDPB_PAN_U_4 );
	PanU4Button->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	PanU16Button = new WButton( this, IDPB_PAN_U_16 );
	PanU16Button->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	PanU64Button = new WButton( this, IDPB_PAN_U_64 );
	PanU64Button->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	PanV1Button = new WButton( this, IDPB_PAN_V_1 );
	PanV1Button->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	PanV4Button = new WButton( this, IDPB_PAN_V_4 );
	PanV4Button->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	PanV16Button = new WButton( this, IDPB_PAN_V_16 );
	PanV16Button->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	PanV64Button = new WButton( this, IDPB_PAN_V_64 );
	PanV64Button->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	Rot45Button = new WButton( this, IDPB_ROT_45 );
	Rot45Button->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	Rot90Button = new WButton( this, IDPB_ROT_90 );
	Rot90Button->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	FlipUButton = new WButton( this, IDPB_ROT_FLIP_U );
	FlipUButton->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	FlipVButton = new WButton( this, IDPB_ROT_FLIP_V );
	FlipVButton->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	ApplyButton = new WButton( this, IDPB_SCALE_APPLY );
	ApplyButton->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	Apply2Button = new WButton( this, IDPB_SCALE_APPLY2 );
	Apply2Button->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	SimpleScaleCombo = new WComboBox( this, IDCB_SIMPLE_SCALE );
	SimpleScaleCombo->OpenWindow( 1, 0, CBS_DROPDOWN );
	ScaleUEdit = new WEdit( this, IDEC_SCALE_U );
	ScaleUEdit->OpenWindow( 1, 0, 0 );
	ScaleVEdit = new WEdit( this, IDEC_SCALE_V );
	ScaleVEdit->OpenWindow( 1, 0, 0 );
	LightMapScaleCombo = new WComboBox( this, IDCB_LIGHTMAP_SCALE );
	LightMapScaleCombo->OpenWindow( 1, 0, CBS_DROPDOWNLIST );

	PlaceControl( RelativeCheck );
	PlaceControl( PanBox );
	PlaceControl( AlignmentBox );
	PlaceControl( OptionsBox );
	PlaceControl( PanU1Button );
	PlaceControl( PanU4Button );
	PlaceControl( PanU16Button );
	PlaceControl( PanU64Button );
	PlaceControl( PanV1Button );
	PlaceControl( PanV4Button );
	PlaceControl( PanV16Button );
	PlaceControl( PanV64Button );
	PlaceControl( Rot45Button );
	PlaceControl( Rot90Button );
	PlaceControl( FlipUButton );
	PlaceControl( FlipVButton );
	PlaceControl( ApplyButton );
	PlaceControl( Apply2Button );
	PlaceControl( SimpleScaleCombo );
	PlaceControl( ScaleUEdit );
	PlaceControl( ScaleVEdit );
	PlaceControl( LightMapScaleCombo );

	Finalize();

	// Delegates.
	PanU1Button->ClickDelegate = FDelegate(this, (TDelegate)OnPanU1Clicked);
	PanU4Button->ClickDelegate = FDelegate(this, (TDelegate)OnPanU4Clicked);
	PanU16Button->ClickDelegate = FDelegate(this, (TDelegate)OnPanU16Clicked);
	PanU64Button->ClickDelegate = FDelegate(this, (TDelegate)OnPanU64Clicked);
	PanV1Button->ClickDelegate = FDelegate(this, (TDelegate)OnPanV1Clicked);
	PanV4Button->ClickDelegate = FDelegate(this, (TDelegate)OnPanV4Clicked);
	PanV16Button->ClickDelegate = FDelegate(this, (TDelegate)OnPanV16Clicked);
	PanV64Button->ClickDelegate = FDelegate(this, (TDelegate)OnPanV64Clicked);
	ApplyButton->ClickDelegate = FDelegate(this, (TDelegate)OnApplyClicked);
	Apply2Button->ClickDelegate = FDelegate(this, (TDelegate)OnApply2Clicked);
	FlipUButton->ClickDelegate = FDelegate(this, (TDelegate)OnFlipUClicked);
	FlipVButton->ClickDelegate = FDelegate(this, (TDelegate)OnFlipVClicked);
	Rot45Button->ClickDelegate = FDelegate(this, (TDelegate)OnRot45Clicked);
	Rot90Button->ClickDelegate = FDelegate(this, (TDelegate)OnRot90Clicked);

	LightMapScaleCombo->SelectionEndOkDelegate = FDelegate(this, (TDelegate)OnLightMapScaleChanged);

	// Initialize controls.
	SimpleScaleCombo->AddString( TEXT("0.0625" ) );
	SimpleScaleCombo->AddString( TEXT("0.125" ) );
	SimpleScaleCombo->AddString( TEXT("0.25" ) );
	SimpleScaleCombo->AddString( TEXT("0.5" ) );
	SimpleScaleCombo->AddString( TEXT("1.0" ) );
	SimpleScaleCombo->AddString( TEXT("2.0" ) );
	SimpleScaleCombo->AddString( TEXT("4.0" ) );
	SimpleScaleCombo->AddString( TEXT("8.0" ) );
	SimpleScaleCombo->AddString( TEXT("16.0" ) );

	LightMapScaleCombo->AddString( TEXT("1.0") );
	LightMapScaleCombo->AddString( TEXT("2.0") );
	LightMapScaleCombo->AddString( TEXT("4.0") );
	LightMapScaleCombo->AddString( TEXT("8.0") );
	LightMapScaleCombo->AddString( TEXT("16.0") );
	LightMapScaleCombo->AddString( TEXT("32.0") );
	LightMapScaleCombo->AddString( TEXT("64.0") );
	LightMapScaleCombo->AddString( TEXT("128.0") );
	LightMapScaleCombo->AddString( TEXT("256.0") );		

	Refresh();

	unguard;
}
void WPagePanRotScale::OnDestroy()
{
	guard(WPagePanRotScale::OnDestroy);
	WSurfacePropPage::OnDestroy();

	delete RelativeCheck;
	delete PanBox;
	delete AlignmentBox;
	delete OptionsBox;
	delete PanU1Button;
	delete PanU4Button;
	delete PanU16Button;
	delete PanU64Button;
	delete PanV1Button;
	delete PanV4Button;
	delete PanV16Button;
	delete PanV64Button;
	delete Rot45Button;
	delete Rot90Button;
	delete FlipUButton;
	delete FlipVButton;
	delete ApplyButton;
	delete Apply2Button;
	delete SimpleScaleCombo;
	delete ScaleUEdit;
	delete ScaleVEdit;
	delete LightMapScaleCombo;

	unguard;
}

void WPagePanRotScale::Refresh()
{
	guard(WPagePanRotScale::Refresh);

	WSurfacePropPage::Refresh();

	// Find the lightmap scale on the selected surfaces.

	FLOAT	LightMapScale = 0.0f;
	UBOOL	ValidScale = 0;

	for(INT SurfaceIndex = 0;SurfaceIndex < GUnrealEd->Level->Model->Surfs.Num();SurfaceIndex++)
	{
		FBspSurf&	Surf =  GUnrealEd->Level->Model->Surfs(SurfaceIndex);

		if(Surf.PolyFlags & PF_Selected)
		{
			if(!ValidScale)
			{
				LightMapScale = Surf.LightMapScale;
				ValidScale = 1;
			}
			else if(LightMapScale != Surf.LightMapScale)
			{
				ValidScale = 0;
				break;
			}
		}
	}

	// Select the appropriate scale.

	INT		ScaleItem = INDEX_NONE;

	if(ValidScale)
	{
		FString	ScaleString = FString::Printf(TEXT("%.1f"),LightMapScale);

		ScaleItem = LightMapScaleCombo->FindString(*ScaleString);

		if(ScaleItem == INDEX_NONE)
		{
			ScaleItem = LightMapScaleCombo->GetCount();
			LightMapScaleCombo->AddString(*ScaleString);
		}
	}

	LightMapScaleCombo->SetCurrent(ScaleItem);

	unguard;
}

void WPagePanRotScale::OnLightMapScaleChanged()
{
	guard(WPagePanRotScale::OnLightMapScaleChanged);

	FLOAT	LightMapScale = appAtof(*LightMapScaleCombo->GetString(LightMapScaleCombo->GetCurrent()));

	for(INT SurfaceIndex = 0;SurfaceIndex < GUnrealEd->Level->Model->Surfs.Num();SurfaceIndex++)
	{
		FBspSurf&	Surf = GUnrealEd->Level->Model->Surfs(SurfaceIndex);

		if(Surf.PolyFlags & PF_Selected)
		{
			Surf.Actor->Brush->Polys->Element(Surf.iBrushPoly).LightMapScale = LightMapScale;
			Surf.LightMapScale = LightMapScale;
		}
	}

	Refresh();

	unguard;
}

void WPagePanRotScale::PanU( INT InPan )
{
	FLOAT Mod = GetAsyncKeyState(VK_SHIFT) & 0x8000 ? -1 : 1;
	GUnrealEd->Exec( *FString::Printf( TEXT("POLY TEXPAN U=%f"), InPan * Mod ) );
}

void WPagePanRotScale::PanV( INT InPan )
{
	FLOAT Mod = GetAsyncKeyState(VK_SHIFT) & 0x8000 ? -1 : 1;
	GUnrealEd->Exec( *FString::Printf( TEXT("POLY TEXPAN V=%f"), InPan * Mod ) );
}

void WPagePanRotScale::Scale( FLOAT InScaleU, FLOAT InScaleV, UBOOL InRelative )
{
	if( !InScaleU || !InScaleV ) { return; }

	InScaleU = 1.0f / InScaleU;
	InScaleV = 1.0f / InScaleV;

	GUnrealEd->Exec( *FString::Printf( TEXT("POLY TEXSCALE %s UU=%f VV=%f"), InRelative?TEXT("RELATIVE"):TEXT(""), InScaleU, InScaleV ) );
}
void WPagePanRotScale::OnApplyClicked()
{
	FLOAT ScaleU = appAtof( *ScaleUEdit->GetText() );
	FLOAT ScaleV = appAtof( *ScaleVEdit->GetText() );
	Scale( ScaleU, ScaleV, RelativeCheck->IsChecked() );
}
void WPagePanRotScale::OnApply2Clicked()
{
	FLOAT ScaleValue = appAtof( *SimpleScaleCombo->GetText() );
	Scale( ScaleValue, ScaleValue, RelativeCheck->IsChecked() );
}

void WPagePanRotScale::OnFlipUClicked()
{
	GUnrealEd->Exec( TEXT("POLY TEXMULT UU=-1 VV=1") );
}
void WPagePanRotScale::OnFlipVClicked()
{
	GUnrealEd->Exec( TEXT("POLY TEXMULT UU=1 VV=-1") );
}
void WPagePanRotScale::OnRot45Clicked()
{
	FLOAT Mod = GetAsyncKeyState(VK_SHIFT) & 0x8000 ? -1 : 1;
	FLOAT UU = 1.0f / appSqrt(2.f);
	FLOAT VV = 1.0f / appSqrt(2.f);
	FLOAT UV = (1.0f / appSqrt(2.f)) * Mod;
	FLOAT VU = -(1.0f / appSqrt(2.f)) * Mod;
	GUnrealEd->Exec( *FString::Printf( TEXT("POLY TEXMULT UU=%f VV=%f UV=%f VU=%f"), UU, VV, UV, VU ) );
}
void WPagePanRotScale::OnRot90Clicked()
{
	FLOAT Mod = GetAsyncKeyState(VK_SHIFT) & 0x8000 ? -1 : 1;
	FLOAT UU = 0;
	FLOAT VV = 0;
	FLOAT UV = 1 * Mod;
	FLOAT VU = -1 * Mod;
	GUnrealEd->Exec( *FString::Printf( TEXT("POLY TEXMULT UU=%f VV=%f UV=%f VU=%f"), UU, VV, UV, VU ) );
}

// --------------------------------------------------------------
//
// WPageAlignment
//
// --------------------------------------------------------------


// Structors.
WPageAlignment::WPageAlignment ( WWindow* InOwnerWindow )
:	WSurfacePropPage( InOwnerWindow )
{
	AlignmentBox = OptionsBox = NULL;
	AlignButton = NULL;
	AlignList = NULL;
}

void WPageAlignment::OpenWindow( INT InDlgId, HMODULE InHMOD )
{
	guard(WPageAlignment::OpenWindow);
	WSurfacePropPage::OpenWindow( InDlgId, InHMOD );

	// Create child controls and let the base class determine their proper positions.
	AlignmentBox = new WGroupBox( this, IDGP_ALIGN );
	AlignmentBox->OpenWindow( 1, 0 );
	OptionsBox = new WGroupBox( this, IDGP_OPTIONS );
	OptionsBox->OpenWindow( 1, 0 );
	AlignButton = new WButton( this, IDPB_ALIGN );
	AlignButton->OpenWindow( 1, 0, 0, 0, 0, TEXT("") );
	AlignList = new WListBox( this, IDLB_ALIGN );
	AlignList->OpenWindow( 1, 0, 0, 0 );

	PlaceControl( AlignmentBox );
	PlaceControl( OptionsBox );
	PlaceControl( AlignButton );
	PlaceControl( AlignList );

	Finalize();

	// Delegates.
	AlignButton->ClickDelegate = FDelegate(this, (TDelegate)OnAlignClick);
	AlignList->DoubleClickDelegate = FDelegate(this, (TDelegate)OnAlignClick);
	AlignList->SelectionChangeDelegate = FDelegate(this, (TDelegate)AlignListSelectionChange);

	// Initialize controls.
	for( INT x = 0 ; x < GTexAlignTools.Aligners.Num() ; ++x )
		AlignList->AddString( *GTexAlignTools.Aligners(x)->Desc );
	AlignList->SetCurrent( 0 );

	PropertyWindow = NULL;

	RefreshPropertyWindow();

	unguard;
}
void WPageAlignment::RefreshPropertyWindow()
{
	guard(WPageAlignment::RefreshPropertyWindow);

	if( PropertyWindow )
		::DestroyWindow( PropertyWindow->hWnd );
	delete PropertyWindow;
	PropertyWindow = NULL;

	Proxy = NULL;
	INT Sel = AlignList->GetCurrent();

	PropertyWindow = new WObjectProperties( NAME_None, CPF_Edit, TEXT(""), this, 0 );
	PropertyWindow->ShowTreeLines = 0;
	PropertyWindow->Root.Sorted = 0;
	PropertyWindow->OpenChildWindow( IDSC_PROPS );

	PropertyWindow->Root.SetObjects( (UObject**)&GTexAlignTools.Aligners(Sel), 1 );

	unguard;
}
void WPageAlignment::OnDestroy()
{
	guard(WPageAlignment::OnDestroy);
	WSurfacePropPage::OnDestroy();

	::DestroyWindow( AlignmentBox->hWnd );
	::DestroyWindow( OptionsBox->hWnd );
	::DestroyWindow( AlignButton->hWnd );
	::DestroyWindow( AlignList->hWnd );
	if( PropertyWindow )
		::DestroyWindow( PropertyWindow->hWnd );

	delete PropertyWindow;
	delete AlignmentBox;
	delete OptionsBox;
	delete AlignButton;
	delete AlignList;

	unguard;
}
void WPageAlignment::FlushFields()
{
	if( !PropertyWindow ) return;

	// Force all controls to save their values before trying to build the brush.
	for( INT i=0; i<PropertyWindow->Root.Children.Num(); ++i )
		((FPropertyItem*)PropertyWindow->Root.Children(i))->SendToControl();
}
void WPageAlignment::Refresh()
{
	guard(WPageAlignment::Refresh);
	WSurfacePropPage::Refresh();

	RefreshPropertyWindow();
	unguard;
}

void WPageAlignment::AlignListSelectionChange()
{
	RefreshPropertyWindow();
}
void WPageAlignment::OnAlignClick()
{
	INT Sel = AlignList->GetCurrent();
	GTexAlignTools.Aligners( Sel )->Align( TEXALIGN_None, GUnrealEd->Level->Model );
}

void WPageAlignment::Align( ETexAlign InTexAlign )
{
	GTexAlignTools.GetAligner( InTexAlign )->Align( InTexAlign, GUnrealEd->Level->Model );
}


// --------------------------------------------------------------
//
// WPageStats
//
// --------------------------------------------------------------

WPageStats::WPageStats ( WWindow* InOwnerWindow )
:	WSurfacePropPage( InOwnerWindow )
{
	LightingBox = NULL;
	StaticLightsLabel = MeshelsLabel = MeshSizeLabel = NULL;
}

void WPageStats::OpenWindow( INT InDlgId, HMODULE InHMOD )
{
	guard(WPageStats::OpenWindow);
	WSurfacePropPage::OpenWindow( InDlgId, InHMOD );

	// Create child controls and let the base class determine their proper positions.
	LightingBox = new WGroupBox( this, IDGP_LIGHTING );
	LightingBox->OpenWindow( 1, 0 );
	StaticLightsLabel = new WLabel( this, IDSC_STATIC_LIGHTS );
	StaticLightsLabel->OpenWindow( 1, 0 );
	MeshelsLabel = new WLabel( this, IDSC_MESHELS );
	MeshelsLabel->OpenWindow( 1, 0 );
	MeshSizeLabel = new WLabel( this, IDSC_MESH_SIZE );
	MeshSizeLabel->OpenWindow( 1, 0 );

	PlaceControl( LightingBox );
	PlaceControl( StaticLightsLabel );
	PlaceControl( MeshelsLabel );
	PlaceControl( MeshSizeLabel );

	Finalize();

	unguard;
}
void WPageStats::OnDestroy()
{
	guard(WPageStats::OnDestroy);
	WSurfacePropPage::OnDestroy();

	::DestroyWindow( LightingBox->hWnd );
	::DestroyWindow( StaticLightsLabel->hWnd );
	::DestroyWindow( MeshelsLabel->hWnd );
	::DestroyWindow( MeshSizeLabel->hWnd );

	delete LightingBox;
	delete StaticLightsLabel;
	delete MeshelsLabel;
	delete MeshSizeLabel;

	unguard;
}
void WPageStats::Refresh()
{
	guard(WPageStats::Refresh);
	WSurfacePropPage::Refresh();

	FStringOutputDevice GetPropResult = FStringOutputDevice();

	GetPropResult.Empty();	GUnrealEd->Get( TEXT("POLYS"), TEXT("STATICLIGHTS"), GetPropResult );
	StaticLightsLabel->SetText( *GetPropResult );
	GetPropResult.Empty();	GUnrealEd->Get( TEXT("POLYS"), TEXT("MESHELS"), GetPropResult );
	MeshelsLabel->SetText( *GetPropResult );
	GetPropResult.Empty();	GUnrealEd->Get( TEXT("POLYS"), TEXT("MESHSIZE"), GetPropResult );
	MeshSizeLabel->SetText( *GetPropResult );

	unguard;
}

// --------------------------------------------------------------
//
// WSurfacePropSheet
//
// --------------------------------------------------------------


// Structors.
WSurfacePropSheet::WSurfacePropSheet( FName InPersistentName, WWindow* InOwnerWindow )
:	WWindow( InPersistentName, InOwnerWindow )
{
}

// WSurfacePropSheet interface.
void WSurfacePropSheet::OpenWindow()
{
	guard(WSurfacePropSheet::OpenWindow);
	MdiChild = 0;
	PerformCreateWindowEx
	(
		NULL,
		TEXT("Surface Properties"),
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU,
		0, 0,
		0, 0,
		OwnerWindow ? OwnerWindow->hWnd : NULL,
		NULL,
		hInstance
	);

	unguard;
}
void WSurfacePropSheet::OnCreate()
{
	guard(WSurfacePropSheet::OnCreate);
	WWindow::OnCreate();

	// Create the sheet
	PropSheet = new WPropertySheet( this, IDPS_SURFACE_PROPS );
	PropSheet->OpenWindow( 1, 0, 0 );

	// Create the pages for the sheet
	FlagsPage = new WPageFlags( PropSheet->Tabs );
	FlagsPage->OpenWindow( IDPP_SP_FLAGS1, GUnrealEdModule);
	PropSheet->AddPage( FlagsPage );

	PanRotScalePage = new WPagePanRotScale( PropSheet->Tabs );
	PanRotScalePage->OpenWindow( IDPP_SP_PANROTSCALE, GUnrealEdModule);
	PropSheet->AddPage( PanRotScalePage );

	AlignmentPage = new WPageAlignment( PropSheet->Tabs );
	AlignmentPage->OpenWindow( IDPP_SP_ALIGNMENT, GUnrealEdModule);
	PropSheet->AddPage( AlignmentPage );

	StatsPage = new WPageStats( PropSheet->Tabs );
	StatsPage->OpenWindow( IDPP_SP_STATS, GUnrealEdModule);
	PropSheet->AddPage( StatsPage );

	PropSheet->SetCurrent( 0 );

	// Resize the property sheet to surround the pages properly.
	RECT rect;
	::GetClientRect( FlagsPage->hWnd, &rect );
	::SetWindowPos( hWnd, HWND_TOP, 0, 0, rect.right + 32, rect.bottom + 64, SWP_NOMOVE );

	PositionChildControls();

	unguard;
}
void WSurfacePropSheet::OnDestroy()
{
	guard(WSurfacePropSheet::OnDestroy);
	WWindow::OnDestroy();

	delete FlagsPage;
	delete PanRotScalePage;
	delete AlignmentPage;
	delete StatsPage;
	delete PropSheet;
	unguard;
}
void WSurfacePropSheet::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	guard(WSurfacePropSheet::OnSize);
	WWindow::OnSize(Flags, NewX, NewY);
	PositionChildControls();
	InvalidateRect( hWnd, NULL, FALSE );
	unguard;
}
void WSurfacePropSheet::PositionChildControls()
{
	guard(WSurfacePropSheet::PositionChildControls);
	if( !PropSheet || !::IsWindow( PropSheet->hWnd )
			)
		return;

	FRect CR = GetClientRect();
	::MoveWindow( PropSheet->hWnd, 0, 0, CR.Width(), CR.Height(), 1 );

	unguard;
}
INT WSurfacePropSheet::OnSysCommand( INT Command )
{
	guard(WSurfacePropSheet::OnSysCommand);
	if( Command == SC_CLOSE )
	{
		Show(0);
		return 1;
	}

	return 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

