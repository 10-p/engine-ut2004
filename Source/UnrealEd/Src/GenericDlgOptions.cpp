/*=============================================================================
	UnGenericDlgOptions.cpp: Option classes for generic dialogs
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "UnrealEd.h"

extern UStruct* GColorStruct;


/*------------------------------------------------------------------------------
	UOptionsProxy
------------------------------------------------------------------------------*/

// Constructors.
UOptionsProxy::UOptionsProxy()
{}

void UOptionsProxy::InitFields()
{
	guard(UOptionsProxy::InitFields);
	unguard;
}
void UOptionsProxy::Destroy()
{
	UObject::Destroy();
}
void UOptionsProxy::StaticConstructor()
{
	guard(UOptionsProxy::StaticConstructor);
	unguard;
}
IMPLEMENT_CLASS(UOptionsProxy);

/*------------------------------------------------------------------------------
	UOptionsBrushScale
------------------------------------------------------------------------------*/


UOptionsBrushScale::UOptionsBrushScale()
{
}

void UOptionsBrushScale::InitFields()
{
	guard(UOptionsBrushScale::InitFields);

	new(GetClass(),TEXT("Z"), RF_Public)UFloatProperty(CPP_PROPERTY(Z), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Y"), RF_Public)UFloatProperty(CPP_PROPERTY(Y), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("X"), RF_Public)UFloatProperty(CPP_PROPERTY(X), TEXT(""), CPF_Edit );

	unguard;
}

void UOptionsBrushScale::StaticConstructor()
{
	guard(UOptionsBrushScale::StaticConstructor);

	X = 1;
	Y = 1;
	Z = 1;

	unguard;
}

IMPLEMENT_CLASS(UOptionsBrushScale);

/*------------------------------------------------------------------------------
	UOptions2DShaper
------------------------------------------------------------------------------*/
UOptions2DShaper::UOptions2DShaper()
{}

void UOptions2DShaper::InitFields()
{
	guard(UOptions2DShaper::InitFields);

	new(GetClass(),TEXT("Axis"), RF_Public)UByteProperty(CPP_PROPERTY(Axis), TEXT(""), CPF_Edit, AxisEnum );

	unguard;
}

void UOptions2DShaper::StaticConstructor()
{
	guard(UOptions2DShaper::StaticConstructor);

	AxisEnum = new( GetClass(), TEXT("Axis") )UEnum( NULL );
	new(AxisEnum->Names)FName( TEXT("AXIS_X") );
	new(AxisEnum->Names)FName( TEXT("AXIS_Y") );
	new(AxisEnum->Names)FName( TEXT("AXIS_Z") );

	Axis = AXIS_Y;

	unguard;
}
IMPLEMENT_CLASS(UOptions2DShaper);

/*------------------------------------------------------------------------------
	UOptions2DShaperSheet
------------------------------------------------------------------------------*/

UOptions2DShaperSheet::UOptions2DShaperSheet()
{}

void UOptions2DShaperSheet::InitFields()
{
	guard(UOptions2DShaperSheet::InitFields);
	UOptions2DShaper::InitFields();
	unguard;
}

void UOptions2DShaperSheet::StaticConstructor()
{
	guard(UOptions2DShaperSheet::StaticConstructor);
	unguard;
}
IMPLEMENT_CLASS(UOptions2DShaperSheet);

/*------------------------------------------------------------------------------
	UOptions2DShaperExtrude
------------------------------------------------------------------------------*/
UOptions2DShaperExtrude::UOptions2DShaperExtrude()
{}

void UOptions2DShaperExtrude::InitFields()
{
	guard(UOptions2DShaperExtrude::InitFields);
	UOptions2DShaper::InitFields();
	new(GetClass(),TEXT("Depth"), RF_Public)UIntProperty(CPP_PROPERTY(Depth), TEXT(""), CPF_Edit );
	unguard;
}

void UOptions2DShaperExtrude::StaticConstructor()
{
	guard(UOptions2DShaperExtrude::StaticConstructor);
	Depth = 256;
	unguard;
}
IMPLEMENT_CLASS(UOptions2DShaperExtrude);

/*------------------------------------------------------------------------------
	UOptions2DShaperExtrudeToPoint
------------------------------------------------------------------------------*/
UOptions2DShaperExtrudeToPoint::UOptions2DShaperExtrudeToPoint()
{}

void UOptions2DShaperExtrudeToPoint::InitFields()
{
	guard(UOptions2DShaperExtrudeToPoint::InitFields);
	UOptions2DShaper::InitFields();
	new(GetClass(),TEXT("Depth"), RF_Public)UIntProperty(CPP_PROPERTY(Depth), TEXT(""), CPF_Edit );
	unguard;
}

void UOptions2DShaperExtrudeToPoint::StaticConstructor()
{
	guard(UOptions2DShaperExtrudeToPoint::StaticConstructor);
	Depth = 256;
	unguard;
}
IMPLEMENT_CLASS(UOptions2DShaperExtrudeToPoint);

/*------------------------------------------------------------------------------
	UOptions2DShaperExtrudeToBevel
------------------------------------------------------------------------------*/
UOptions2DShaperExtrudeToBevel::UOptions2DShaperExtrudeToBevel()
{}

void UOptions2DShaperExtrudeToBevel::InitFields()
{
	guard(UOptions2DShaperExtrudeToBevel::InitFields);
	UOptions2DShaper::InitFields();
	new(GetClass(),TEXT("Height"), RF_Public)UIntProperty(CPP_PROPERTY(Height), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("CapHeight"), RF_Public)UIntProperty(CPP_PROPERTY(CapHeight), TEXT(""), CPF_Edit );
	unguard;
}

void UOptions2DShaperExtrudeToBevel::StaticConstructor()
{
	guard(UOptions2DShaperExtrudeToBevel::StaticConstructor);
	Height = 128;
	CapHeight = 32;
	unguard;
}
IMPLEMENT_CLASS(UOptions2DShaperExtrudeToBevel);

/*------------------------------------------------------------------------------
	UOptions2DShaperRevolve
------------------------------------------------------------------------------*/
UOptions2DShaperRevolve::UOptions2DShaperRevolve()
{}

void UOptions2DShaperRevolve::InitFields()
{
	guard(UOptions2DShaperRevolve::InitFields);
	UOptions2DShaper::InitFields();
	new(GetClass(),TEXT("SidesPer360"), RF_Public)UIntProperty(CPP_PROPERTY(SidesPer360), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Sides"), RF_Public)UIntProperty(CPP_PROPERTY(Sides), TEXT(""), CPF_Edit );
	unguard;
}

void UOptions2DShaperRevolve::StaticConstructor()
{
	guard(UOptions2DShaperRevolve::StaticConstructor);
	SidesPer360 = 12;
	Sides = 3;
	unguard;
}
IMPLEMENT_CLASS(UOptions2DShaperRevolve);

/*------------------------------------------------------------------------------
	UOptions2DShaperBezierDetail
------------------------------------------------------------------------------*/

UOptions2DShaperBezierDetail::UOptions2DShaperBezierDetail()
{}

void UOptions2DShaperBezierDetail::InitFields()
{
	guard(UOptions2DShaperBezierDetail::InitFields);
	new(GetClass(),TEXT("DetailLevel"), RF_Public)UIntProperty(CPP_PROPERTY(DetailLevel), TEXT(""), CPF_Edit );
	unguard;
}

void UOptions2DShaperBezierDetail::StaticConstructor()
{
	guard(UOptions2DShaperBezierDetail::StaticConstructor);
	DetailLevel = 10;
	unguard;
}
IMPLEMENT_CLASS(UOptions2DShaperBezierDetail);

/*------------------------------------------------------------------------------
	UOptionsSurfBevel
------------------------------------------------------------------------------*/
UOptionsSurfBevel::UOptionsSurfBevel()
{}

void UOptionsSurfBevel::InitFields()
{
	guard(UOptionsSurfBevel::InitFields);
	new(GetClass(),TEXT("Depth"), RF_Public)UIntProperty(CPP_PROPERTY(Depth), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Bevel"), RF_Public)UIntProperty(CPP_PROPERTY(Bevel), TEXT(""), CPF_Edit );
	unguard;
}

void UOptionsSurfBevel::StaticConstructor()
{
	guard(UOptionsSurfBevel::StaticConstructor);
	Depth = 16;
	Bevel = 16;
	unguard;
}
IMPLEMENT_CLASS(UOptionsSurfBevel);

/*------------------------------------------------------------------------------
	UOptionsTexAlign
------------------------------------------------------------------------------*/
UOptionsTexAlign::UOptionsTexAlign()
{}

void UOptionsTexAlign::InitFields()
{
	guard(UOptionsTexAlign::InitFields);
	unguard;
}

void UOptionsTexAlign::StaticConstructor()
{
	guard(UOptionsTexAlign::StaticConstructor);

	TAxisEnum = new( GetClass(), TEXT("TAxis") )UEnum( NULL );
	new(TAxisEnum->Names)FName( TEXT("TAXIS_X") );
	new(TAxisEnum->Names)FName( TEXT("TAXIS_Y") );
	new(TAxisEnum->Names)FName( TEXT("TAXIS_Z") );
	new(TAxisEnum->Names)FName( TEXT("TAXIS_WALLS") );
	new(TAxisEnum->Names)FName( TEXT("TAXIS_AUTO") );

	TAxis = TAXIS_AUTO;

	unguard;
}

IMPLEMENT_CLASS(UOptionsTexAlign);

/*------------------------------------------------------------------------------
	UOptionsTexAlignPlanar
------------------------------------------------------------------------------*/

UOptionsTexAlignPlanar::UOptionsTexAlignPlanar()
{}

void UOptionsTexAlignPlanar::InitFields()
{
	guard(UOptionsTexAlignPlanar::InitFields);
	UOptionsTexAlign::InitFields();
	new(GetClass(),TEXT("TAxis"), RF_Public)UByteProperty(CPP_PROPERTY(TAxis), TEXT(""), CPF_Edit, TAxisEnum );
	unguard;
}

void UOptionsTexAlignPlanar::StaticConstructor()
{
	guard(UOptionsTexAlignPlanar::StaticConstructor);
	unguard;
}

IMPLEMENT_CLASS(UOptionsTexAlignPlanar);

/*------------------------------------------------------------------------------
	UOptionsTexAlignCylinder
------------------------------------------------------------------------------*/
UOptionsTexAlignCylinder::UOptionsTexAlignCylinder()
{}

void UOptionsTexAlignCylinder::InitFields()
{
	guard(UOptionsTexAlignCylinder::InitFields);
	UOptionsTexAlign::InitFields();
	new(GetClass(),TEXT("VTile"), RF_Public)UIntProperty(CPP_PROPERTY(VTile), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("UTile"), RF_Public)UIntProperty(CPP_PROPERTY(UTile), TEXT(""), CPF_Edit );
	unguard;
}

void UOptionsTexAlignCylinder::StaticConstructor()
{
	guard(UOptionsTexAlignCylinder::StaticConstructor);
	UTile = VTile = 1;
	unguard;
}
IMPLEMENT_CLASS(UOptionsTexAlignCylinder);

/*------------------------------------------------------------------------------
	UOptionsNewTerrain
------------------------------------------------------------------------------*/
UOptionsNewTerrain::UOptionsNewTerrain()
{}

void UOptionsNewTerrain::InitFields()
{
	guard(UOptionsNewTerrain::InitFields);
	new(GetClass(),TEXT("Height"), RF_Public)UIntProperty(CPP_PROPERTY(Height), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("YSize"), RF_Public)UIntProperty(CPP_PROPERTY(YSize), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("XSize"), RF_Public)UIntProperty(CPP_PROPERTY(XSize), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Name"), RF_Public)UStrProperty(CPP_PROPERTY(Name), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Group"), RF_Public)UStrProperty(CPP_PROPERTY(Group), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Package"), RF_Public)UStrProperty(CPP_PROPERTY(Package), TEXT(""), CPF_Edit );
	unguard;
}

void UOptionsNewTerrain::StaticConstructor()
{
	guard(UOptionsNewTerrain::StaticConstructor);

	XSize = 256;
	YSize = 256;
	Package = TEXT("MyLevel");
	Name = TEXT("MyTerrain");
	Height = 32768;

	unguard;
}
IMPLEMENT_CLASS(UOptionsNewTerrain);

/*------------------------------------------------------------------------------
	UOptionsNewTerrainLayer
------------------------------------------------------------------------------*/
UOptionsNewTerrainLayer::UOptionsNewTerrainLayer()
{}

void UOptionsNewTerrainLayer::InitFields()
{
	guard(UOptionsNewTerrainLayer::InitFields);
	new(GetClass(),TEXT("VScale"), RF_Public)UIntProperty(CPP_PROPERTY(VScale), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("UScale"), RF_Public)UIntProperty(CPP_PROPERTY(UScale), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("ColorFill"), RF_Public)UStructProperty(CPP_PROPERTY(ColorFill), TEXT(""), CPF_Edit, GColorStruct );
	new(GetClass(),TEXT("AlphaFill"), RF_Public)UStructProperty(CPP_PROPERTY(AlphaFill), TEXT(""), CPF_Edit, GColorStruct );
	new(GetClass(),TEXT("AlphaWidth"), RF_Public)UIntProperty(CPP_PROPERTY(AlphaWidth), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("AlphaHeight"), RF_Public)UIntProperty(CPP_PROPERTY(AlphaHeight), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Name"), RF_Public)UStrProperty(CPP_PROPERTY(Name), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Group"), RF_Public)UStrProperty(CPP_PROPERTY(Group), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Package"), RF_Public)UStrProperty(CPP_PROPERTY(Package), TEXT(""), CPF_Edit );
	unguard;
}

void UOptionsNewTerrainLayer::StaticConstructor()
{
	guard(UOptionsNewTerrainLayer::StaticConstructor);

	AlphaWidth = 256;
	AlphaHeight = 256;
	UScale = 1;
	VScale = 1;
	Package = TEXT("MyLevel");
	Name = TEXT("Layer");
	ColorFill = FColor(127,127,127,0);

	unguard;
}
IMPLEMENT_CLASS(UOptionsNewTerrainLayer);

/*------------------------------------------------------------------------------
	UOptionsMapScale
------------------------------------------------------------------------------*/
UOptionsMapScale::UOptionsMapScale()
{}

void UOptionsMapScale::InitFields()
{
	guard(UOptionsMapScale::InitFields);

	new(GetClass(),TEXT("Scale Collision?"),RF_Public)UBoolProperty (CPP_PROPERTY(bScaleCollision),TEXT(""),CPF_Edit );
	new(GetClass(),TEXT("Scale Locations?"),RF_Public)UBoolProperty (CPP_PROPERTY(bScaleLocations),TEXT(""),CPF_Edit );
	new(GetClass(),TEXT("Scale Sprites?"),RF_Public)UBoolProperty (CPP_PROPERTY(bScaleSprites),TEXT(""),CPF_Edit );
	new(GetClass(),TEXT("Adjust Lights?"),RF_Public)UBoolProperty (CPP_PROPERTY(bAdjustLights),TEXT(""),CPF_Edit );
	new(GetClass(),TEXT("Factor"), RF_Public)UFloatProperty(CPP_PROPERTY(Factor),TEXT(""),CPF_Edit );
	unguard;
}

void UOptionsMapScale::StaticConstructor()
{
	guard(UOptionsMapScale::StaticConstructor);

	Factor = 1.0f;
	bAdjustLights = 1;
	bScaleSprites = 0;
	bScaleLocations = 1;
	bScaleCollision = 1;

	unguard;
}
IMPLEMENT_CLASS(UOptionsMapScale);

/*------------------------------------------------------------------------------
	UOptionsMatNewCameraPath
------------------------------------------------------------------------------*/
UOptionsMatNewCameraPath::UOptionsMatNewCameraPath()
{}

void UOptionsMatNewCameraPath::InitFields()
{
	guard(UOptionsMatNewCameraPath::InitFields);

	new(GetClass(),TEXT("Path Name"), RF_Public)UStrProperty(CPP_PROPERTY(PathName), TEXT(""), CPF_Edit );
	unguard;
}

void UOptionsMatNewCameraPath::StaticConstructor()
{
	guard(UOptionsMatNewCameraPath::StaticConstructor);

	PathName = TEXT("New Path");

	unguard;
}
IMPLEMENT_CLASS(UOptionsMatNewCameraPath);

/*------------------------------------------------------------------------------
	UOptionsMatNewStaticMesh
------------------------------------------------------------------------------*/

UOptionsMatNewStaticMesh::UOptionsMatNewStaticMesh()
{}

void UOptionsMatNewStaticMesh::InitFields()
{
	guard(UOptionsMatNewStaticMesh::InitFields);

	new(GetClass(),TEXT("Name"), RF_Public)UStrProperty(CPP_PROPERTY(Name), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Group"), RF_Public)UStrProperty(CPP_PROPERTY(Group), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Package"), RF_Public)UStrProperty(CPP_PROPERTY(PkgName), TEXT(""), CPF_Edit );
	unguard;
}

void UOptionsMatNewStaticMesh::StaticConstructor()
{
	guard(UOptionsMatNewStaticMesh::StaticConstructor);

	Name = TEXT("MyStaticMesh");
	Group = TEXT("");
	PkgName = TEXT("MyLevel");

	unguard;
}
IMPLEMENT_CLASS(UOptionsMatNewStaticMesh);

/*------------------------------------------------------------------------------
	UOptionsDupTexture
------------------------------------------------------------------------------*/
UOptionsDupTexture::UOptionsDupTexture()
{}

void UOptionsDupTexture::InitFields()
{
	guard(UOptionsDupTexture::InitFields);
	new(GetClass(),TEXT("Name"), RF_Public)UStrProperty(CPP_PROPERTY(Name), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Group"), RF_Public)UStrProperty(CPP_PROPERTY(Group), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Package"), RF_Public)UStrProperty(CPP_PROPERTY(Package), TEXT(""), CPF_Edit );
	unguard;
}

void UOptionsDupTexture::StaticConstructor()
{
	guard(UOptionsDupTexture::StaticConstructor);

	Package = TEXT("MyLevel");
	Name = TEXT("NewTexture");

	unguard;
}
IMPLEMENT_CLASS(UOptionsDupTexture);

/*------------------------------------------------------------------------------
	UOptionsRotateActors
------------------------------------------------------------------------------*/
UOptionsRotateActors::UOptionsRotateActors()
{}

void UOptionsRotateActors::InitFields()
{
	guard(UOptionsRotateActors::InitFields);
	new(GetClass(),TEXT("Roll Max"), RF_Public)UIntProperty(CPP_PROPERTY(RollMax), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Roll Min"), RF_Public)UIntProperty(CPP_PROPERTY(RollMin), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Yaw Max"), RF_Public)UIntProperty(CPP_PROPERTY(YawMax), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Yaw Min"), RF_Public)UIntProperty(CPP_PROPERTY(YawMin), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Pitch Max"), RF_Public)UIntProperty(CPP_PROPERTY(PitchMax), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Pitch Min"), RF_Public)UIntProperty(CPP_PROPERTY(PitchMin), TEXT(""), CPF_Edit );
	unguard;
}

void UOptionsRotateActors::StaticConstructor()
{
	guard(UOptionsRotateActors::StaticConstructor);

	PitchMin = 0;
	PitchMax = 65535;
	YawMin = 0;
	YawMax = 65535;
	RollMin = 0;
	RollMax = 65535;

	unguard;
}
IMPLEMENT_CLASS(UOptionsRotateActors);

/*------------------------------------------------------------------------------
	UOptionsNewClassFromSel
------------------------------------------------------------------------------*/
UOptionsNewClassFromSel::UOptionsNewClassFromSel()
{}

void UOptionsNewClassFromSel::InitFields()
{
	guard(UOptionsNewClassFromSel::InitFields);
	new(GetClass(),TEXT("Name"), RF_Public)UStrProperty(CPP_PROPERTY(Name), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Package"), RF_Public)UStrProperty(CPP_PROPERTY(Package), TEXT(""), CPF_Edit );
	unguard;
}

void UOptionsNewClassFromSel::StaticConstructor()
{
	guard(UOptionsNewClassFromSel::StaticConstructor);

	Package = TEXT("MyLevel");
	Name = TEXT("MyClass");

	unguard;
}
IMPLEMENT_CLASS(UOptionsNewClassFromSel);

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
