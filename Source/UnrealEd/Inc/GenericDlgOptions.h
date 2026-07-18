/*=============================================================================
	UnGenericDlgOptions.h: Option classes for generic dialogs
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/*------------------------------------------------------------------------------
	UOptionsProxy
------------------------------------------------------------------------------*/
class UOptionsProxy : public UObject
{
	DECLARE_ABSTRACT_CLASS(UOptionsProxy,UObject,0,UnrealEd)

	// Constructors.
	UOptionsProxy();
	virtual void InitFields();
	virtual void Destroy();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptionsBrushScale
------------------------------------------------------------------------------*/
class UOptionsBrushScale : public UOptionsProxy
{
	DECLARE_CLASS(UOptionsBrushScale,UOptionsProxy,0,UnrealEd)

	FLOAT X, Y, Z;

	UOptionsBrushScale();

	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptions2DShaper
------------------------------------------------------------------------------*/
class UOptions2DShaper : public UOptionsProxy
{
	DECLARE_CLASS(UOptions2DShaper,UOptionsProxy,0,UnrealEd)

	UEnum* AxisEnum;

    BYTE Axis;

	UOptions2DShaper();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptions2DShaperSheet
------------------------------------------------------------------------------*/
class UOptions2DShaperSheet : public UOptions2DShaper
{
	DECLARE_CLASS(UOptions2DShaperSheet,UOptions2DShaper,0,UnrealEd)

	UOptions2DShaperSheet();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptions2DShaperExtrude
------------------------------------------------------------------------------*/
class UOptions2DShaperExtrude : public UOptions2DShaper
{
	DECLARE_CLASS(UOptions2DShaperExtrude,UOptions2DShaper,0,UnrealEd)

	INT Depth;

	UOptions2DShaperExtrude();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptions2DShaperExtrudeToPoint
------------------------------------------------------------------------------*/
class UOptions2DShaperExtrudeToPoint : public UOptions2DShaper
{
	DECLARE_CLASS(UOptions2DShaperExtrudeToPoint,UOptions2DShaper,0,UnrealEd)

	INT Depth;

	UOptions2DShaperExtrudeToPoint();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptions2DShaperExtrudeToBevel
------------------------------------------------------------------------------*/
class UOptions2DShaperExtrudeToBevel : public UOptions2DShaper
{
	DECLARE_CLASS(UOptions2DShaperExtrudeToBevel,UOptions2DShaper,0,UnrealEd)

	INT Height, CapHeight;

	UOptions2DShaperExtrudeToBevel();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptions2DShaperRevolve
------------------------------------------------------------------------------*/
class UOptions2DShaperRevolve : public UOptions2DShaper
{
	DECLARE_CLASS(UOptions2DShaperRevolve,UOptions2DShaper,0,UnrealEd)

	INT SidesPer360, Sides;

	UOptions2DShaperRevolve();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptions2DShaperBezierDetail
------------------------------------------------------------------------------*/
class UOptions2DShaperBezierDetail : public UOptionsProxy
{
	DECLARE_CLASS(UOptions2DShaperBezierDetail,UOptionsProxy,0,UnrealEd)

	INT DetailLevel;

	UOptions2DShaperBezierDetail();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptionsSurfBevel
------------------------------------------------------------------------------*/
class UOptionsSurfBevel : public UOptionsProxy
{
	DECLARE_CLASS(UOptionsSurfBevel,UOptionsProxy,0,UnrealEd)

	INT Depth, Bevel;

	UOptionsSurfBevel();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptionsTexAlign
------------------------------------------------------------------------------*/
class UOptionsTexAlign : public UOptionsProxy
{
	DECLARE_CLASS(UOptionsTexAlign,UOptionsProxy,0,UnrealEd)

	UEnum* TAxisEnum;

    BYTE TAxis;

	UOptionsTexAlign();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptionsTexAlignPlanar
------------------------------------------------------------------------------*/
class UOptionsTexAlignPlanar : public UOptionsTexAlign
{
	DECLARE_CLASS(UOptionsTexAlignPlanar,UOptionsTexAlign,0,UnrealEd)

	UOptionsTexAlignPlanar();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptionsTexAlignCylinder
------------------------------------------------------------------------------*/
class UOptionsTexAlignCylinder : public UOptionsTexAlign
{
	DECLARE_CLASS(UOptionsTexAlignCylinder,UOptionsTexAlign,0,UnrealEd)

	INT UTile, VTile;

	UOptionsTexAlignCylinder();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptionsNewTerrain
------------------------------------------------------------------------------*/
class UOptionsNewTerrain : public UOptionsProxy
{
	DECLARE_CLASS(UOptionsNewTerrain,UOptionsProxy,0,UnrealEd)

	FString Package, Group, Name;
	INT XSize, YSize;
	INT Height;

	UOptionsNewTerrain();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptionsNewTerrainLayer
------------------------------------------------------------------------------*/
class UOptionsNewTerrainLayer : public UOptionsProxy
{
	DECLARE_CLASS(UOptionsNewTerrainLayer,UOptionsProxy,0,UnrealEd)

	FString Package, Group, Name;
	INT AlphaWidth, AlphaHeight;
	FColor AlphaFill, ColorFill;
	INT UScale, VScale;

	UOptionsNewTerrainLayer();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptionsMapScale
------------------------------------------------------------------------------*/
class UOptionsMapScale : public UOptionsProxy
{
	DECLARE_CLASS(UOptionsMapScale,UOptionsProxy,0,UnrealEd)

	FLOAT Factor;
	BITFIELD bAdjustLights, bScaleSprites, bScaleLocations, bScaleCollision;

	UOptionsMapScale();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptionsMatNewCameraPath
------------------------------------------------------------------------------*/
class UOptionsMatNewCameraPath : public UOptionsProxy
{
	DECLARE_CLASS(UOptionsMatNewCameraPath,UOptionsProxy,0,UnrealEd)

	FString PathName;

	UOptionsMatNewCameraPath();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptionsMatNewStaticMesh
------------------------------------------------------------------------------*/
class UOptionsMatNewStaticMesh : public UOptionsProxy
{
	DECLARE_CLASS(UOptionsMatNewStaticMesh,UOptionsProxy,0,UnrealEd)

	FString PkgName;
	FString Group;
	FString Name;

	UOptionsMatNewStaticMesh();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptionsDupTexture
------------------------------------------------------------------------------*/
class UOptionsDupTexture : public UOptionsProxy
{
	DECLARE_CLASS(UOptionsDupTexture,UOptionsProxy,0,UnrealEd)

public:
	FString Package, Group, Name;

	UOptionsDupTexture();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptionsRotateActors
------------------------------------------------------------------------------*/
class UOptionsRotateActors : public UOptionsProxy
{
	DECLARE_CLASS(UOptionsRotateActors,UOptionsProxy,0,UnrealEd)

public:
	INT PitchMin, PitchMax, YawMin, YawMax, RollMin, RollMax;

	UOptionsRotateActors();
	virtual void InitFields();
	void StaticConstructor();
};


/*------------------------------------------------------------------------------
	UOptionsNewClassFromSel
------------------------------------------------------------------------------*/
class UOptionsNewClassFromSel : public UOptionsProxy
{
	DECLARE_CLASS(UOptionsNewClassFromSel,UOptionsProxy,0,UnrealEd)

public:
	FString Package, Name;

	UOptionsNewClassFromSel();
	virtual void InitFields();
	void StaticConstructor();
};


/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
