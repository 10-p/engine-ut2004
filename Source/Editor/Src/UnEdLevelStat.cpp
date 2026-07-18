/*=============================================================================
UnEdLevelStat.cpp: 
Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
* Created by Sam Zamani
=============================================================================*/

//	Includes -------------------------------------------------------------------

#include "EditorPrivate.h"
#include "UnEdLevelStat.h"

//	Defines and Consts ---------------------------------------------------------

//	Macros and Typedefs --------------------------------------------------------

//	Global Data ----------------------------------------------------------------

//	Static Data ----------------------------------------------------------------

//	Local Prototypes -----------------------------------------------------------

//	Class Definitions ----------------------------------------------------------

//	Class Implementations ------------------------------------------------------

//------------------------------------------------------------------------------
void FLevelStat::Init()
{
	appMemzero( CountTotals, sizeof(CountTotals) );
	appMemzero( ByteTotals, sizeof(ByteTotals) );

	StaticMeshes.Empty();
	StaticMeshInstances.Empty();

	Meshes.Empty();
	MeshInstances.Empty();

	AllTextures.Empty();
	LightMapTextures.Empty();
}

//------------------------------------------------------------------------------
UBOOL FLevelStat::Analyze( ULevel* InLevel )
{
	Init();

	// -- Actors
	for( INT ActorIndex=0; ActorIndex < InLevel->Actors.Num(); ActorIndex++ )
	{
		AActor* Actor = Cast<AActor>( InLevel->Actors( ActorIndex ) );

		// -- skip deleted actors
		if( !Actor || Actor->bDeleteMe )
			continue;

		// -- Sprite Texture
		if( Actor->Texture )
		{
			AddMaterialTextures( Actor->Texture );
			CountTotals[COUNT_SPRITES]++;
		}

		// -- Static Mesh
		if( Cast<AStaticMeshActor>( Actor ) )
		{
			AStaticMeshActor* SM = Cast<AStaticMeshActor>( Actor );

			if( SM->StaticMesh )
				StaticMeshes.AddUniqueItem( SM->StaticMesh );

			for( INT s=0; s < SM->Skins.Num(); s++ )
			{
				if( SM->Skins(s) )
				{
					AddMaterialTextures( SM->Skins(s) );
				}				
			}

			CountTotals[COUNT_STATICMESH]++;
		}
		else
		{
			// -- Overridable Actor skins
			for( INT s=0; s < Actor->Skins.Num(); s++ )
			{
				if( Actor->Skins(s) )
				{
					AddMaterialTextures( Actor->Skins(s) );
					CountTotals[COUNT_ACTOR_SKINS];
				}
			}
		}

		// -- Static Mesh Instance
		if( Actor->StaticMeshInstance )
		{
			StaticMeshInstances.AddUniqueItem( Actor->StaticMeshInstance );
			FArchiveCountMem memCounter( Actor->StaticMeshInstance );
			ByteTotals[BYTES_GEOM_STATICMESH_INSTANCES] += memCounter.GetNum();
			CountTotals[COUNT_STATICMESH_INSTANCES]++;
		}

		// -- Skinned Mesh & Instances
		ULodMesh* LodMesh = Cast<ULodMesh>( Actor->Mesh );
		ULodMeshInstance* LodMeshInstance = Cast<ULodMeshInstance>( Actor->MeshInstance );
		if( LodMeshInstance && !LodMesh )
		{
			MeshInstances.AddUniqueItem( LodMeshInstance );
			LodMesh = Cast<ULodMesh>( LodMeshInstance->GetMesh() );
			CountTotals[COUNT_MESH_INSTANCES]++;
		}
		else if( LodMesh )
		{
			Meshes.AddUniqueItem( LodMesh );
			CountTotals[COUNT_MESH]++;
		}		

		// -- Other Textures
		if( Actor->OverlayMaterial )
		{
			AddMaterialTextures( Actor->OverlayMaterial );
		}

		// -- Terrain
		if( Cast<ATerrainInfo>( Actor ) )
		{
			ATerrainInfo* TerrainInfo = Cast<ATerrainInfo>( Actor );
			for( INT i=0 ; i < ARRAY_COUNT(TerrainInfo->Layers); i++ )
			{
				if( TerrainInfo->Layers[i].AlphaMap )
				{
					AddMaterialTextures( TerrainInfo->Layers[i].AlphaMap );					
				}
				if( TerrainInfo->Layers[i].Texture )
				{
					AddMaterialTextures( TerrainInfo->Layers[i].Texture );
				}
			}

			// -- add decoration layer static meshes to static mesh list
			for( INT d=0; d < TerrainInfo->DecoLayers.Num(); d++ )
			{
				if( TerrainInfo->DecoLayers(d).StaticMesh )
					StaticMeshes.AddUniqueItem( TerrainInfo->DecoLayers(d).StaticMesh );
			}

			AddMaterialTextures( TerrainInfo->TerrainMap );

			AddTerrainGeom( TerrainInfo );
		}

		// -- BSP Brush Materials
		if( Cast<ABrush>( Actor ) )
		{
			ABrush* Brush = Cast<ABrush>( Actor );
			if( Brush->Brush )
			{
				for( INT i=0; i < Brush->Brush->Polys->Element.Num(); i++ )
				{
					FPoly* Poly = &(Brush->Brush->Polys->Element(i));
					if( Poly->Material )
					{
						AddMaterialTextures( Poly->Material );
					}
				}				
			}			
		}

	} //for

	// -- BSP ROOT
	if( InLevel->Model )
	{
		for( INT i=0; i < InLevel->Model->Surfs.Num(); i++ )
		{
			AddMaterialTextures( InLevel->Model->Surfs(i).Material );
		}

		for( INT i=0; i < InLevel->Model->Sections.Num(); i++ )
		{
			AddMaterialTextures( InLevel->Model->Sections(i).Material );
		}

		for( INT i=0; i < InLevel->Model->LightMapTextures.Num(); i++ )
		{
			LightMapTextures.AddUniqueItem( &InLevel->Model->LightMapTextures(i) );
		}

		for( INT i=0; i < InLevel->Model->LightMaps.Num(); i++ )
		{
//			FLightMap* lightMap = &InLevel->Model->LightMaps(i);
		}

		AddBspGeom( InLevel->Model );
	}

	// -- Add Static Mesh textures to list
	for( INT sm=0; sm < StaticMeshes.Num(); sm++ )
	{		
		for( INT m=0; m < StaticMeshes(sm)->Materials.Num(); m++ )
		{
			if( StaticMeshes(sm)->Materials(m).Material )
			{
				AddMaterialTextures( StaticMeshes(sm)->Materials(m).Material );
			}				
		}				

		AddStaticMeshGeom( StaticMeshes(sm) );
	}

	// -- Add Skinned Mesh textures to list
	for( INT i=0; i < Meshes.Num(); i++ )
	{
		for( INT m=0; m < Meshes(i)->Materials.Num(); m++ )
		{
			AddMaterialTextures( Meshes(i)->Materials(m) );
		}

		AddMeshGeom( Meshes(i) );
	}

	// -- do byte counts for all textures
	for( INT t=0; t < AllTextures.Num(); t++ )
	{
		DWORD totalBytes=0;
		UTexture* tex = AllTextures(t);

		if( tex->GetNumMips() )
		{
			for( INT mi=0; mi < tex->GetNumMips(); mi++ )
			{
				totalBytes += GetBytesPerPixel( (ETextureFormat)tex->Format, tex->GetMip(mi)->USize * tex->GetMip(mi)->VSize );
			}
		}
		else
		{
			totalBytes += GetBytesPerPixel( (ETextureFormat)tex->Format, tex->USize * tex->VSize );
		}		

		switch( tex->Format )
		{
		case TEXF_RGBA8:
			ByteTotals[BYTES_TEX_RGBA] += totalBytes;
			CountTotals[COUNT_TEX_RGBA]++;
			break;
		case TEXF_P8:
			ByteTotals[BYTES_TEX_P8] += totalBytes;
			CountTotals[COUNT_TEX_P8]++;
			break;		
		case TEXF_DXT1:
			ByteTotals[BYTES_TEX_DXT1] += totalBytes;
			CountTotals[COUNT_TEX_DXT1]++;
			break;
		case TEXF_DXT3:
			ByteTotals[BYTES_TEX_DXT3] += totalBytes;
			CountTotals[COUNT_TEX_DXT3]++;
			break;
		case TEXF_DXT5:
			ByteTotals[BYTES_TEX_DXT5] += totalBytes;
			CountTotals[COUNT_TEX_DXT5]++;
			break;
			ByteTotals[BYTES_TEX_BUMP] += totalBytes;
			CountTotals[COUNT_TEX_BUMP]++;
			break;
		default:
			ByteTotals[BYTES_TEX_OTHER] += totalBytes;
			CountTotals[COUNT_TEX_OTHER]++;
		}
		
		ByteTotals[BYTES_TEX_ALL] += totalBytes;
		CountTotals[COUNT_TEX_ALL]++;
	}

	// -- do byte counts of all light maps
	for( INT t=0; t < LightMapTextures.Num(); t++ )
	{
		FLightMapTexture* lightmapTex = LightMapTextures(t);
		FStaticLightMapTexture* staticTex = &lightmapTex->StaticTexture;
		// mip 0
		ByteTotals[BYTES_TEX_LIGHTMAP] += GetBytesPerPixel( (ETextureFormat)staticTex->GetFormat(), staticTex->GetWidth() * staticTex->GetHeight() );
		// mip 1
		ByteTotals[BYTES_TEX_LIGHTMAP] += GetBytesPerPixel( (ETextureFormat)staticTex->GetFormat(), staticTex->GetWidth()/2.0f * staticTex->GetHeight()/2.0f );
		CountTotals[COUNT_TEX_LIGHTMAP]++;
	}

	ByteTotals[BYTES_TEX_ALL] += ByteTotals[BYTES_TEX_LIGHTMAP];

	return 1;
}

//------------------------------------------------------------------------------
UBOOL FLevelStat::IsTextureUnique( UTexture* tex )
{
	if( INDEX_NONE != AllTextures.FindItemIndex( tex ) )
		return 0;

	return 1;
}

//------------------------------------------------------------------------------
void FLevelStat::AddMaterialTextures( UMaterial* mat )
{
	// -- materials have to be traversed recursively to find their actual textures
	// -- hopefully there are no circular references!

	if( !mat )
		return;

	if( Cast<UCubemap>( mat ) )
	{
		UCubemap* cubemap = Cast<UCubemap>( mat );
		AllTextures.AddUniqueItem( cubemap->Faces[0] );
		AllTextures.AddUniqueItem( cubemap->Faces[1] );
		AllTextures.AddUniqueItem( cubemap->Faces[2] );
		AllTextures.AddUniqueItem( cubemap->Faces[3] );
		AllTextures.AddUniqueItem( cubemap->Faces[4] );
		AllTextures.AddUniqueItem( cubemap->Faces[5] );
	}
	else if( Cast<UTexture>( mat ) )
	{
		// found a real texture -- add it to list
		UTexture* tex = Cast<UTexture>( mat );
		AllTextures.AddUniqueItem( tex );
	}
	else if( Cast<UCombiner>( mat ) )
	{
		UCombiner* combiner = Cast<UCombiner>( mat );
		AddMaterialTextures( combiner->Material1 );
		AddMaterialTextures( combiner->Material2 );
		AddMaterialTextures( combiner->Mask );
	}
	else if( Cast<UShader>( mat ) )
	{
        UShader* shader = Cast<UShader>( mat );

		AddMaterialTextures( shader->Diffuse );
		AddMaterialTextures( shader->Opacity );
		AddMaterialTextures( shader->Specular );
		AddMaterialTextures( shader->SpecularityMask );
		AddMaterialTextures( shader->SelfIllumination );
		AddMaterialTextures( shader->SelfIlluminationMask );
		AddMaterialTextures( shader->Detail );
	}
	else if( Cast<UTerrainMaterial>( mat ) )
	{
		UTerrainMaterial* terrain = Cast<UTerrainMaterial>( mat );
		for( INT i=0; i < terrain->Layers.Num(); i++ )
		{
			AddMaterialTextures( terrain->Layers(i).AlphaWeight );
			AddMaterialTextures( terrain->Layers(i).Texture );
		}
	}
	else if( Cast<UParticleMaterial>( mat ) )
	{
		UParticleMaterial* particle = Cast<UParticleMaterial>( mat );
		AddMaterialTextures( particle->BitmapMaterial );
	}
	else if( Cast<UProjectorMaterial>( mat ) )
	{
		UProjectorMaterial* projector = Cast<UProjectorMaterial>( mat );
		AddMaterialTextures( projector->Gradient );
		AddMaterialTextures( projector->Projected );
		AddMaterialTextures( projector->BaseMaterial );
	}
	else if( Cast<UModifier>( mat ) )
	{
		if( Cast<UMaterialSwitch>( mat ) )
		{
			UMaterialSwitch* matswitch = Cast<UMaterialSwitch>( mat );
			for( INT i=0; i < matswitch->Materials.Num(); i++ )
			{
                AddMaterialTextures( matswitch->Materials(i) );
			}
		}
		else if( Cast<UMaterialSequence>( mat ) )
		{
			UMaterialSequence* sequence = Cast<UMaterialSequence>( mat );
			for( INT i=0; i < sequence->SequenceItems.Num(); i++ )
			{
				AddMaterialTextures( sequence->SequenceItems(i).Material );                
			}            
		}
		else if( Cast<UOpacityModifier>( mat ) )
		{
			UOpacityModifier* opacity = Cast<UOpacityModifier>( mat );
			AddMaterialTextures( opacity->Opacity );
		}
		else
		{
			UModifier* modifier = Cast<UModifier>( mat );
			AddMaterialTextures( modifier->Material );
		}
	}
}

//------------------------------------------------------------------------------
void FLevelStat::AddStaticMeshGeom( UStaticMesh* mesh )
{
	FArchiveCountMem memCounter( mesh );
	ByteTotals[BYTES_GEOM_STATICMESH] += memCounter.GetMax();

#if 0
	// -- byte counts of all streams and buffers
	ByteTotals[BYTES_GEOM_STATICMESH] += mesh->VertexStream.GetSize();
	ByteTotals[BYTES_GEOM_STATICMESH] += mesh->ColorStream.GetSize();
	ByteTotals[BYTES_GEOM_STATICMESH] += mesh->AlphaStream.GetSize();
	for( INT uv=0; uv < mesh->UVStreams.Num(); uv++ )
	{
		ByteTotals[BYTES_GEOM_STATICMESH] += mesh->UVStreams(uv).GetSize();
	}
	ByteTotals[BYTES_GEOM_STATICMESH] += mesh->IndexBuffer.GetSize();
	ByteTotals[BYTES_GEOM_STATICMESH] += mesh->WireframeIndexBuffer.GetSize();
	ByteTotals[BYTES_GEOM_STATICMESH] += mesh->Sections.Num() * sizeof(FStaticMeshSection);
#endif

	CountTotals[COUNT_STATICMESH_VERTS] += mesh->VertexStream.Vertices.Num();
	for( INT s=0; s < mesh->Sections.Num(); s++ )
	{
		CountTotals[COUNT_STATICMESH_PRIMS] += mesh->Sections(s).NumPrimitives;
	}	

#if 0
	// -- byte count for collision data -- NOTESZ: this is basically a copy of the static mesh tris/verts
	ByteTotals[BYTES_GEOM_STATICMESH_COLLISION] += mesh->CollisionTriangles.Num() * sizeof(FStaticMeshCollisionTriangle);
	ByteTotals[BYTES_GEOM_STATICMESH_COLLISION] += mesh->CollisionNodes.Num() * sizeof(FStaticMeshCollisionNode);
	CountTotals[COUNT_STATICMESH_COLLISION_VERTS] += mesh->CollisionTriangles.Num() * 3;
	CountTotals[COUNT_STATICMESH_COLLISION_PRIMS] += mesh->CollisionTriangles.Num();
#endif

	// -- UModel collision object
	if( mesh->CollisionModel )
	{
		// -- prim counts
		CountTotals[COUNT_STATICMESH_COLLISION_VERTS] += mesh->CollisionModel->Verts.Num();
		CountTotals[COUNT_STATICMESH_COLLISION_PRIMS] += mesh->CollisionModel->Surfs.Num();

#if 0
		// -- byte counts
		DWORD totalSize=0;
		totalSize += mesh->CollisionModel->Nodes.Num() * sizeof(FBspNode);
		totalSize += mesh->CollisionModel->Verts.Num() * sizeof(FVert);
		totalSize += mesh->CollisionModel->Vectors.Num() * sizeof(FVector);
		totalSize += mesh->CollisionModel->Points.Num() * sizeof(FVector);
		totalSize += mesh->CollisionModel->Surfs.Num() * sizeof(FBspSurf);
		totalSize += mesh->CollisionModel->Bounds.Num() * sizeof(FBox);
		totalSize += mesh->CollisionModel->LeafHulls.Num() * sizeof(INT);
		totalSize += mesh->CollisionModel->Leaves.Num() * sizeof(FLeaf);
		totalSize += mesh->CollisionModel->Sections.Num() * sizeof(FBspSection);
		ByteTotals[BYTES_GEOM_STATICMESH_COLLISION] += totalSize;
#endif
		FArchiveCountMem Counter( mesh->CollisionModel );
		ByteTotals[BYTES_GEOM_STATICMESH_COLLISION] += Counter.GetMax();
	}
}

//------------------------------------------------------------------------------
void FLevelStat::AddMeshGeom( ULodMesh* mesh )
{
	if( Cast<USkeletalMesh>( mesh ) )
	{
		USkeletalMesh* sm = Cast<USkeletalMesh>( mesh );

		FArchiveCountMem memCounter( sm );
		ByteTotals[BYTES_GEOM_MESH] += memCounter.GetNum();

		sm->RawVerts.Load();
		sm->RawFaces.Load();
		
		CountTotals[COUNT_MESH_VERTS] += sm->RawVerts.Num();
		CountTotals[COUNT_MESH_PRIMS] += sm->RawFaces.Num();		
	}
	else if( Cast<UVertMesh>( mesh ) )
	{
		UVertMesh* vm = Cast<UVertMesh>( mesh );

		CountTotals[COUNT_MESH_VERTS] += vm->VertexStream.Vertices.Num();
		CountTotals[COUNT_MESH_PRIMS] += vm->Faces.Num();

		FArchiveCountMem memCounter( vm );
		ByteTotals[BYTES_GEOM_MESH] += memCounter.GetNum();
	}
	else
	{
		CountTotals[COUNT_MESH_VERTS] += mesh->Verts.Num();
		CountTotals[COUNT_MESH_PRIMS] += mesh->Faces.Num();

		FArchiveCountMem memCounter( mesh );
		ByteTotals[BYTES_GEOM_MESH] += memCounter.GetNum();
	}
}

//------------------------------------------------------------------------------
void FLevelStat::AddBspGeom( UModel* model )
{
	/*
	{
		FArchiveCountMem memCounter( model );
		ByteTotals[BYTES_GEOM_BSP] += memCounter.GetNum();
	}
	*/

	// -- byte counts
	DWORD totalSize=0;
	totalSize += model->Nodes.Num() * sizeof(FBspNode);
	totalSize += model->Verts.Num() * sizeof(FVert);
	totalSize += model->Vectors.Num() * sizeof(FVector);
	totalSize += model->Points.Num() * sizeof(FVector);
	totalSize += model->Surfs.Num() * sizeof(FBspSurf);
	totalSize += model->Bounds.Num() * sizeof(FBox);
	totalSize += model->LeafHulls.Num() * sizeof(INT);
	totalSize += model->Leaves.Num() * sizeof(FLeaf);
	totalSize += model->Sections.Num() * sizeof(FBspSection);
	ByteTotals[BYTES_GEOM_BSP] += totalSize;

	// -- count totals
	CountTotals[COUNT_BSP_VERTS] += model->Verts.Num();
	CountTotals[COUNT_BSP_PRIMS] += model->Surfs.Num();
	CountTotals[COUNT_BSP_NODES] += model->Nodes.Num();
	CountTotals[COUNT_BSP_SECTIONS] += model->Sections.Num();
	CountTotals[COUNT_BSP]++;
}

//------------------------------------------------------------------------------
void FLevelStat::AddTerrainGeom( ATerrainInfo* terrainInfo )
{	
	// -- byte counts
	{
		FArchiveCountMem memCounter( terrainInfo );
		ByteTotals[BYTES_GEOM_TERRAIN] += memCounter.GetNum();
	}

	// -- count totals
	CountTotals[COUNT_TERRAIN_VERTS] += terrainInfo->Vertices.Num();
	CountTotals[COUNT_TERRAIN_PRIMS] += terrainInfo->Sectors.Num();
	CountTotals[COUNT_TERRAIN]++;
}

//------------------------------------------------------------------------------
void FLevelStat::PrintTextureSummary( FOutputDevice& Ar )
{
	Ar.Logf( TEXT("") );
	Ar.Logf( TEXT("          TEXTURE SUMMARY") );
	Ar.Logf( TEXT("-----------------------------------") );
	Ar.Logf( TEXT("%8s %10s %12s"),		TEXT("Format"), TEXT("Count"), TEXT("Bytes") );
	Ar.Logf( TEXT("%8s %10i %10.1f KB"), TEXT("RGBA "), CountTotals[COUNT_TEX_RGBA], ByteTotals[BYTES_TEX_RGBA]/1024.f );
	Ar.Logf( TEXT("%8s %10i %10.1f KB"), TEXT("P8   "), CountTotals[COUNT_TEX_P8], ByteTotals[BYTES_TEX_P8]/1024.f );
	Ar.Logf( TEXT("%8s %10i %10.1f KB"), TEXT("DXT1 "), CountTotals[COUNT_TEX_DXT1], ByteTotals[BYTES_TEX_DXT1]/1024.f );
	Ar.Logf( TEXT("%8s %10i %10.1f KB"), TEXT("DXT3 "), CountTotals[COUNT_TEX_DXT3], ByteTotals[BYTES_TEX_DXT3]/1024.f );
	Ar.Logf( TEXT("%8s %10i %10.1f KB"), TEXT("DXT5 "), CountTotals[COUNT_TEX_DXT5], ByteTotals[BYTES_TEX_DXT5]/1024.f );
	Ar.Logf( TEXT("%8s %10i %10.1f KB"), TEXT("BUMP "), CountTotals[COUNT_TEX_BUMP], ByteTotals[BYTES_TEX_BUMP]/1024.f );
	Ar.Logf( TEXT("%8s %10i %10.1f KB"), TEXT("OTHER"), CountTotals[COUNT_TEX_OTHER], ByteTotals[BYTES_TEX_OTHER]/1024.f );
	Ar.Logf( TEXT("%8s %10i %10.1f KB"), TEXT("LIGHTMAP"), CountTotals[COUNT_TEX_LIGHTMAP], ByteTotals[BYTES_TEX_LIGHTMAP]/1024.f );
	Ar.Logf( TEXT("%8s %10i %10.1f KB"), TEXT("TOTAL"), CountTotals[COUNT_TEX_ALL], ByteTotals[BYTES_TEX_ALL]/1024.f );
}

//------------------------------------------------------------------------------
void FLevelStat::PrintTextureDetails( FOutputDevice& Ar )
{
	Ar.Log( TEXT("") );
	Ar.Logf( TEXT("          TEXTURE DETAILS") );
	Ar.Logf( TEXT("-----------------------------------") );

	Ar.Logf( TEXT("%10s %13s %6s %13s %30s"), TEXT("Format"), TEXT("Size"), TEXT("Mips"), TEXT("Bytes"), TEXT("Name") );

	for( INT t=0; t < AllTextures.Num(); t++ )
	{
		DWORD totalBytes=0;
		UTexture* tex = AllTextures(t);

		if( tex->GetNumMips() )
		{
			for( INT mi=0; mi < tex->GetNumMips(); mi++ )
			{
				totalBytes += GetBytesPerPixel( (ETextureFormat)tex->Format, tex->GetMip(mi)->USize * tex->GetMip(mi)->VSize );
			}
		}
		else
		{
			totalBytes += GetBytesPerPixel( (ETextureFormat)tex->Format, tex->USize * tex->VSize );
		}

		Ar.Logf( TEXT("%10s [%4i x%4i] %6i %13.1f KB %30s"), 
			*tex->GetFormatDesc(), tex->USize, tex->VSize, tex->GetNumMips(), totalBytes/1024.f, tex->GetName() );
	}

	for( INT t=0; t < LightMapTextures.Num(); t++ )
	{
		DWORD totalBytes=0;
		FLightMapTexture* lightmapTex = LightMapTextures(t);
		FStaticLightMapTexture* staticTex = &lightmapTex->StaticTexture;
		totalBytes += GetBytesPerPixel( (ETextureFormat)staticTex->GetFormat(), staticTex->GetWidth() * staticTex->GetHeight() );
		totalBytes += GetBytesPerPixel( (ETextureFormat)staticTex->GetFormat(), staticTex->GetWidth()/2.0f * staticTex->GetHeight()/2.0f );

		Ar.Logf( TEXT("%10s [%4i x%4i] %6i %13.1f KB %29s%i"), 
			*GetFormatDesc(staticTex->GetFormat()), staticTex->GetWidth(), staticTex->GetHeight(), staticTex->GetNumMips(), totalBytes/1024.f, TEXT("LIGHTMAP"), t );
	}

}

//------------------------------------------------------------------------------
void FLevelStat::PrintGeomSummary( FOutputDevice& Ar )
{
	Ar.Logf( TEXT("") );
	Ar.Logf( TEXT("          GEOMETRY SUMMARY") );
	Ar.Logf( TEXT("-----------------------------------") );
	
	Ar.Logf( TEXT("") );
	Ar.Logf( TEXT("[STATIC MESH]") );
	Ar.Logf( TEXT("%8s %10s %10s %12s"), TEXT("Count"), TEXT("Prims"), TEXT("Verts"), TEXT("Bytes") );
	Ar.Logf( TEXT("%8i %10i %10i %10.1f KB"),
		CountTotals[COUNT_STATICMESH], CountTotals[COUNT_STATICMESH_PRIMS], CountTotals[COUNT_STATICMESH_VERTS], 
		ByteTotals[BYTES_GEOM_STATICMESH]/1024.f );

	Ar.Logf( TEXT("") );
	Ar.Logf( TEXT("[STATIC MESH COLLISION]") );
	Ar.Logf( TEXT("%8s %10s %10s %12s"), TEXT("Count"), TEXT("Prims"), TEXT("Verts"), TEXT("Bytes") );
	Ar.Logf( TEXT("%8i %10i %10i %10.1f KB"),
		CountTotals[COUNT_STATICMESH], CountTotals[COUNT_STATICMESH_COLLISION_PRIMS], CountTotals[COUNT_STATICMESH_COLLISION_VERTS], 
		ByteTotals[BYTES_GEOM_STATICMESH_COLLISION]/1024.f );

	Ar.Logf( TEXT("") );
	Ar.Logf( TEXT("[SKIN MESH]") );
	Ar.Logf( TEXT("%8s %10s %10s %12s"), TEXT("Count"), TEXT("Prims"), TEXT("Verts"), TEXT("Bytes") );
	Ar.Logf( TEXT("%8i %10i %10i %10.1f KB"), 
		CountTotals[COUNT_MESH], CountTotals[COUNT_MESH_PRIMS], CountTotals[COUNT_MESH_VERTS], 
		ByteTotals[BYTES_GEOM_MESH]/1024.f );

/*
	Ar.Logf( TEXT("") );
	Ar.Logf( TEXT("[SKIN MESH COLLISION]") );
	Ar.Logf( TEXT("%8s %10s %10s %12s"), TEXT("Count"), TEXT("Prims"), TEXT("Verts"), TEXT("Bytes") );
	Ar.Logf( TEXT("%8i %10i %10i %10.1f KB"),
		CountTotals[COUNT_MESH], CountTotals[COUNT_MESH_COLLISION_PRIMS], CountTotals[COUNT_MESH_COLLISION_VERTS],
		ByteTotals[BYTES_GEOM_MESH_COLLISION]/1024.f );
*/

	Ar.Logf( TEXT("") );
	Ar.Logf( TEXT("[BSP]") );
	Ar.Logf( TEXT("%8s %10s %10s %12s   %10s %10s"), TEXT("Count"), TEXT("Prims"), TEXT("Verts"), TEXT("Bytes"), TEXT("Nodes"), TEXT("Sections") );
	Ar.Logf( TEXT("%8i %10i %10i %10.1f KB %10i %10i"), 
		CountTotals[COUNT_BSP], CountTotals[COUNT_BSP_PRIMS], CountTotals[COUNT_BSP_VERTS],
		ByteTotals[BYTES_GEOM_BSP]/1024.f,
		CountTotals[COUNT_BSP_NODES], CountTotals[COUNT_BSP_SECTIONS] );

	Ar.Logf( TEXT("") );
	Ar.Logf( TEXT("[TERRAIN]") );
	Ar.Logf( TEXT("%8s %10s %10s %12s"), TEXT("Count"), TEXT("Prims"), TEXT("Verts"), TEXT("Bytes") );
	Ar.Logf( TEXT("%8i %10i %10i %10.1f KB"),
		CountTotals[COUNT_TERRAIN], CountTotals[COUNT_TERRAIN_PRIMS], CountTotals[COUNT_TERRAIN_VERTS],
		ByteTotals[BYTES_GEOM_TERRAIN]/1024.f );
}

//------------------------------------------------------------------------------
FString FLevelStat::GetFormatDesc( ETextureFormat format )
{
	switch( format )
	{
	case TEXF_P8:		return TEXT("P8");
	case TEXF_RGBA7:	return TEXT("RGBA7");
	case TEXF_RGB16:	return TEXT("RGB16");
	case TEXF_DXT1:		return TEXT("DXT1");
	case TEXF_RGB8:		return TEXT("RGB8");
	case TEXF_RGBA8:	return TEXT("RGBA8");
	case TEXF_DXT3:		return TEXT("DXT3");
	case TEXF_DXT5:		return TEXT("DXT5");
	case TEXF_G16:		return TEXT("G16");
	case TEXF_RRRGGGBBB:return TEXT("RRRGGGBBB");
	case TEXF_L8:		return TEXT("L8");
		break;
	}

	return TEXT("?");
}

//==============================================================================
