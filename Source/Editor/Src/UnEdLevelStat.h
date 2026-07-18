/*=============================================================================
	UnEdLevelStat.h: Map stats
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Sam Zamani
=============================================================================*/

#ifndef UnEdLevelStat_H
#define UnEdLevelStat_H

//	Include Files / Forward References -----------------------------------------

//	Defines, Consts, and Macros ------------------------------------------------

//	Typedefs -------------------------------------------------------------------

//	Global Data ----------------------------------------------------------------

//	Global Class Definitions ---------------------------------------------------

class EDITOR_API FLevelStat
{
	// Class DEFINES -------------------------------------------------------

	typedef enum 
	{
		COUNT_STATICMESH=0,
		COUNT_STATICMESH_INSTANCES,
		COUNT_STATICMESH_VERTS,
		COUNT_STATICMESH_PRIMS,
		COUNT_STATICMESH_COLLISION_VERTS,
		COUNT_STATICMESH_COLLISION_PRIMS,
		COUNT_MESH,
		COUNT_MESH_INSTANCES,
		COUNT_MESH_VERTS,
		COUNT_MESH_PRIMS,
		COUNT_MESH_COLLISION_VERTS,
		COUNT_MESH_COLLISION_PRIMS,
		COUNT_BSP,
		COUNT_BSP_VERTS,
		COUNT_BSP_PRIMS,
		COUNT_BSP_SECTIONS,
		COUNT_BSP_NODES,
		COUNT_TERRAIN,
		COUNT_TERRAIN_VERTS,
		COUNT_TERRAIN_PRIMS,
		COUNT_TEX_ALL,
		COUNT_TEX_RGBA,
		COUNT_TEX_P8,
		COUNT_TEX_DXT1,
		COUNT_TEX_DXT3,
		COUNT_TEX_DXT5,
		COUNT_TEX_BUMP,
		COUNT_TEX_LIGHTMAP,
		COUNT_TEX_OTHER,
		COUNT_SPRITES,
		COUNT_ACTOR_SKINS,
		COUNT_TYPES_MAX,
	} CountTypes;

	typedef enum 
	{
		BYTES_GEOM_STATICMESH=0,
		BYTES_GEOM_STATICMESH_COLLISION,
		BYTES_GEOM_STATICMESH_INSTANCES,
		BYTES_GEOM_MESH,
		BYTES_GEOM_MESH_COLLISION,
		BYTES_GEOM_BSP,
		BYTES_GEOM_TERRAIN,
		BYTES_TEX_ALL,
		BYTES_TEX_RGBA,
		BYTES_TEX_P8,
		BYTES_TEX_DXT1,
		BYTES_TEX_DXT3,
		BYTES_TEX_DXT5,
		BYTES_TEX_BUMP,
		BYTES_TEX_LIGHTMAP,
		BYTES_TEX_OTHER,
		BYTE_TYPES_MAX
	} ByteTypes;


	// Class DATA ----------------------------------------------------------
public:	
protected:
private:
	TArray<UStaticMesh*>			StaticMeshes;
	TArray<UStaticMeshInstance*>	StaticMeshInstances;

	TArray<ULodMesh*>				Meshes;
	TArray<ULodMeshInstance*>		MeshInstances;

	TArray<UTexture*>				AllTextures;
	TArray<FLightMapTexture*>		LightMapTextures;

	DWORD							CountTotals[FLevelStat::COUNT_TYPES_MAX];
	DWORD							ByteTotals[FLevelStat::BYTE_TYPES_MAX];

	// Class METHODS -------------------------------------------------------
public:

	UBOOL Analyze( ULevel* InLevel );
	void PrintTextureSummary( FOutputDevice& Ar );
	void PrintTextureDetails( FOutputDevice& Ar );
	void PrintGeomSummary( FOutputDevice& Ar );
	
	DWORD GetCountTotal( INT type ) { return CountTotals[type];	}
	DWORD GetByteTotal( INT type ) { return ByteTotals[type];	}

	FLevelStat() 
	{
		Init();		
	}
	virtual ~FLevelStat() {}

protected:
private:
	void Init();
	UBOOL IsTextureUnique( UTexture* tex );
	void AddMaterialTextures( UMaterial* mat );
	void AddStaticMeshGeom( UStaticMesh* mesh );
	void AddMeshGeom( ULodMesh* mesh );
	void AddBspGeom( UModel* model );
	void AddTerrainGeom( ATerrainInfo* terrainInfo );
	static FString GetFormatDesc( ETextureFormat format );

};

//	Global Class Implementations -----------------------------------------------

//==============================================================================
#endif //UnEdLevelStat_H


