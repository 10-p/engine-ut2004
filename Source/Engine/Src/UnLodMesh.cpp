/*=============================================================================
	UnLodMesh.cpp: Unreal LOD mesh pseudo-abstract base.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Erik de Neve		 
	    
=============================================================================*/ 

#include "EnginePrivate.h"
#include "UnRender.h"

/*-----------------------------------------------------------------------------
	ULodMesh/Instance object implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(ULodMeshInstance);
IMPLEMENT_CLASS(ULodMesh);


void ULodMesh::Serialize( FArchive& Ar )
{
	guard(ULodMesh::Serialize);

	// Serialize parent's variables
	Super::Serialize(Ar);

	// Any re-saving of mesh: stamp with current version number.
	if( Ar.IsSaving() )
		InternalVersion = LODMESH_VERSION;

	Ar << InternalVersion;

	// Serialize the additional LodMesh variables.
	Ar << ModelVerts;
	Ar << Verts;

	if( InternalVersion < 2 ) 
	{
		// Dummy legacy data.
		TArray<FMeshTri> Tris;
		Ar << Tris;
	}

	Ar << Materials;
	Ar << Scale << Origin << RotOrigin;
	
	if( InternalVersion < 2 ) 
	{
		// Dummy legacy data.
		TArray<_WORD> CollapsePointThus;
		Ar << CollapsePointThus;
	}

	Ar << FaceLevel;
	Ar << Faces;
	Ar << CollapseWedgeThus;
	Ar << Wedges;
	Ar << MeshMaterials;
	Ar << MeshScaleMax;
	Ar << LODHysteresis << LODStrength << LODMinVerts << LODMorph << LODZDisplace;

	if( InternalVersion >= 3)
	{
		Ar << bImpostorPresent; 
		Ar << ImpostorProps;
	}	

	if( InternalVersion >= 4)
	{
		Ar << SkinTesselationFactor; 
	}	

	if( !Ar.IsPersistent() )
	{		
	}

	unguardobj;
}


void ULodMeshInstance::Serialize( FArchive& Ar )
{
	guard(ULodMeshInstance::Serialize);
	// Serialize parent
	Super::Serialize(Ar);

	// Serialize - when not loading or saving:
	if( !Ar.IsPersistent() )
	{
	}

	unguardobj;
}



/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/

