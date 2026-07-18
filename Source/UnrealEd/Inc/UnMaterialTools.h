/*=============================================================================
	UnMaterialTools.h: Material editor classes
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Jack Porter
=============================================================================*/

// Holds information for a specific texture property window
class UNREALED_API FTexPropWindowInfo
{
public:
	FTexPropWindowInfo()
	{
		TreeScrollPos = TreeScrollMax = 0;
		Material = TopLevelMaterial = NULL;
		StaticMesh = NULL;
		bShowFallback = 1;
		bShowBackdrop = 0;
		bForceTopLevel = 0;
	}
	~FTexPropWindowInfo()
	{}

	INT TreeScrollPos, TreeScrollMax;	// Variables for tracking the scrollbar position
	UMaterial *Material,				// The currently selected material in the tree
		*TopLevelMaterial;				// The material at the top of the tree;
	UStaticMesh* StaticMesh;			// The static mesh that we'll use for this window
	UBOOL bShowBackdrop,				// If TRUE, the staticmesh views will have the material used as a backdrop
		bShowFallback,					// If TRUE, the material will utilize it's fallback material
		bForceTopLevel;					// If TRUE, the top level in the tree will always be shown
};

class UNREALED_API FMaterialTools
{
public:
	// Constructor.
	FMaterialTools()
	{}
	virtual ~FMaterialTools()
	{}

	TMap<DWORD,FTexPropWindowInfo> Infos;		// There should be one of these for every open material property window (keyed by their HWND's)

	FTexPropWindowInfo* GetInfo( DWORD InHwnd )
	{
		return Infos.Find( InHwnd );
	}
	void Add( DWORD InHwnd )
	{
		Infos.Set( InHwnd, FTexPropWindowInfo() );
	}
};

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
