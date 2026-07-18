/*=============================================================================
	UnStaticMesh.cpp: Static mesh creation.
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EditorPrivate.h"

UBOOL GBuildStaticMeshCollision = 1;

//
//	Compare
//	Used to sort non-indexed triangles by texture/polyflags.
//

INT Compare(FStaticMeshTriangle& A,FStaticMeshTriangle& B)
{
	return (A.MaterialIndex - B.MaterialIndex);
}

//
//	CreateStaticMesh - Creates a static mesh object from raw triangle data.
//

UStaticMesh* CreateStaticMesh(TArray<FStaticMeshTriangle>& Triangles,TArray<FStaticMeshMaterial>& Materials,UObject* InOuter,FName InName)
{
	guard(CreateStaticMesh);

	// Create the UStaticMesh object.

	UStaticMesh*	StaticMesh = new(InOuter,InName,RF_Public|RF_Standalone) UStaticMesh;

	Sort(&Triangles(0),Triangles.Num());

	StaticMesh->RawTriangles.Add(Triangles.Num());
	appMemcpy(&StaticMesh->RawTriangles(0),&Triangles(0),Triangles.Num() * sizeof(FStaticMeshTriangle));

	StaticMesh->Materials.Add(Materials.Num());
	appMemcpy(&StaticMesh->Materials(0),&Materials(0),Materials.Num() * sizeof(FStaticMeshMaterial));

	StaticMesh->UseSimpleBoxCollision = 0; // sjs
	StaticMesh->Build();

	// Assign a valid content authentication key. 
	StaticMesh->AuthenticationKey = StaticMesh->CreateAuthenticationKey();

	return StaticMesh;

	unguard;
}

//
//	FindMaterialIndex
//

// !!! FIXME: also in Engine/Src/UnStaticMesh.cpp ...remove this if possible!
static INT FindMaterialIndex(TArray<FStaticMeshMaterial>& Materials,UMaterial* Material)
{
	INT MaterialIndex;

	for(MaterialIndex = 0;MaterialIndex < Materials.Num();MaterialIndex++)
		if(Materials(MaterialIndex).Material == Material)
			return MaterialIndex;

	MaterialIndex = Materials.Num();
	new(Materials) FStaticMeshMaterial(Material);

	return MaterialIndex;
}


//
//	GetBrushTriangles
//

void GetBrushTriangles(TArray<FStaticMeshTriangle>& Triangles,TArray<FStaticMeshMaterial>& Materials,ABrush* Brush,UModel* Model)
{
	guard(GetBrushTriangles);

	// Calculate the local to world transform for the source brush.

	FCoords	Coords = Brush ? Brush->ToWorld() : GMath.UnitCoords;
	UBOOL	ReverseVertices = 0;
	FVector	PostSub = Brush ? Brush->Location : FVector(0,0,0);

	// For each polygon in the model...

	for(INT PolygonIndex = 0;PolygonIndex < Model->Polys->Element.Num();PolygonIndex++)
	{
		FPoly&		Polygon = Model->Polys->Element(PolygonIndex);
		UMaterial*	Material = Polygon.Material ? Polygon.Material : ((UMaterial*)UMaterial::StaticClass()->GetDefaultObject())->DefaultMaterial;

		GWarn->StatusUpdatef(PolygonIndex,Model->Polys->Element.Num(),TEXT("Copy polygons"));

		// Find a material index for this polygon.

		INT	MaterialIndex = FindMaterialIndex(Materials,Material);

		// Cache the texture coordinate system for this polygon.

		FVector	TextureBase = Polygon.Base - (Brush ? Brush->PrePivot : FVector(0,0,0)),
				TextureX = Polygon.TextureU / ((FLOAT) Material->MaterialUSize()),
				TextureY = Polygon.TextureV / ((FLOAT) Material->MaterialVSize());

		// For each vertex after the first two vertices...

		for(INT VertexIndex = 2;VertexIndex < Polygon.NumVertices;VertexIndex++)
		{
			// Create a triangle for the vertex.

			FStaticMeshTriangle*	Triangle = new(Triangles) FStaticMeshTriangle;

			Triangle->MaterialIndex = MaterialIndex;
			Triangle->SmoothingMask = Polygon.SmoothingMask;
			Triangle->NumUVs = 1;

			Triangle->Vertices[ReverseVertices ? 0 : 2] = Polygon.Vertex[0].TransformPointBy(Coords) - PostSub;
			Triangle->UVs[ReverseVertices ? 0 : 2][0].U = (Triangle->Vertices[ReverseVertices ? 0 : 2] - TextureBase) | TextureX;
			Triangle->UVs[ReverseVertices ? 0 : 2][0].V = (Triangle->Vertices[ReverseVertices ? 0 : 2] - TextureBase) | TextureY;
			Triangle->Colors[0] = FColor(255,255,255,255);

			Triangle->Vertices[1] = Polygon.Vertex[VertexIndex - 1].TransformPointBy(Coords) - PostSub;
			Triangle->UVs[1][0].U = (Triangle->Vertices[1] - TextureBase) | TextureX;
			Triangle->UVs[1][0].V = (Triangle->Vertices[1] - TextureBase) | TextureY;
			Triangle->Colors[1] = FColor(255,255,255,255);

			Triangle->Vertices[ReverseVertices ? 2 : 0] = Polygon.Vertex[VertexIndex].TransformPointBy(Coords) - PostSub;
			Triangle->UVs[ReverseVertices ? 2 : 0][0].U = (Triangle->Vertices[ReverseVertices ? 2 : 0] - TextureBase) | TextureX;
			Triangle->UVs[ReverseVertices ? 2 : 0][0].V = (Triangle->Vertices[ReverseVertices ? 2 : 0] - TextureBase) | TextureY;
			Triangle->Colors[2] = FColor(255,255,255,255);
		}
	}

	// Merge vertices within a certain distance of each other.

	for(INT TriangleIndex = 0;TriangleIndex < Triangles.Num();TriangleIndex++)
	{
		FStaticMeshTriangle&	Triangle = Triangles(TriangleIndex);

		GWarn->StatusUpdatef(TriangleIndex,Triangles.Num(),TEXT("Merging proximate vertices"));

		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
		{
			FVector&	Vertex = Triangle.Vertices[VertexIndex];

			for(INT OtherTriangleIndex = 0;OtherTriangleIndex < Triangles.Num();OtherTriangleIndex++)
			{
				FStaticMeshTriangle&	OtherTriangle = Triangles(OtherTriangleIndex);

				for(INT OtherVertexIndex = 0;OtherVertexIndex < 3;OtherVertexIndex++)
				{
					if(FVerticesEqual(Vertex,OtherTriangle.Vertices[OtherVertexIndex]))
						Vertex = OtherTriangle.Vertices[OtherVertexIndex];
				}
			}
		}
	}

	unguard;
}

//
//	CreateStaticMeshFromBrush - Creates a static mesh from the triangles in a model.
//

UStaticMesh* CreateStaticMeshFromBrush(UObject* Outer,FName Name,ABrush* Brush,UModel* Model)
{
	guard(CreateStaticMeshFromBrush);

	GWarn->BeginSlowTask(TEXT("Creating static mesh..."),1);

	TArray<FStaticMeshTriangle>	Triangles;
	TArray<FStaticMeshMaterial>	Materials;

	GetBrushTriangles(Triangles,Materials,Brush,Model);

	UStaticMesh*	StaticMesh = CreateStaticMesh(Triangles,Materials,Outer,Name);

	GWarn->EndSlowTask();

	return StaticMesh;

	unguard;
}

//
//	CreateModelFromStaticMesh - Creates a model from the triangles in a static mesh.
//

void CreateModelFromStaticMesh(UModel* Model,AActor* StaticMeshActor)
{
	guard(CreateModelFromStaticMesh);

	UStaticMesh*	StaticMesh = StaticMeshActor->StaticMesh;
	FMatrix			LocalToWorld = StaticMeshActor->LocalToWorld();

	Model->Polys->Element.Empty();

#if RESTRICTEXPORT
	// Don't allow conversion if authentication restrictions apply.
	if( StaticMesh && !StaticMesh->ValidateAuthenticationKey() )
	{		
		return;
	}
#endif

	if(!StaticMesh->RawTriangles.Num())
		StaticMesh->RawTriangles.Load();

	if(StaticMesh->RawTriangles.Num())
	{
		for(INT TriangleIndex = 0;TriangleIndex < StaticMesh->RawTriangles.Num();TriangleIndex++)
		{
			FStaticMeshTriangle&	Triangle = StaticMesh->RawTriangles(TriangleIndex);
			FPoly*					Polygon = new(Model->Polys->Element) FPoly;

			Polygon->Init();
			Polygon->iLink = Polygon - &Model->Polys->Element(0);
			Polygon->Material = StaticMesh->Materials(Triangle.MaterialIndex).Material;
			Polygon->PolyFlags = 0;
			Polygon->SmoothingMask = Triangle.SmoothingMask;
			Polygon->NumVertices = 3;

			Polygon->Vertex[2] = LocalToWorld.TransformFVector(Triangle.Vertices[0]);
			Polygon->Vertex[1] = LocalToWorld.TransformFVector(Triangle.Vertices[1]);
			Polygon->Vertex[0] = LocalToWorld.TransformFVector(Triangle.Vertices[2]);

			Polygon->CalcNormal(1);
			Polygon->Finalize(0);

			FTexCoordsToVectors(Polygon->Vertex[2],FVector(Triangle.UVs[0][0].U * Polygon->Material->MaterialUSize(),Triangle.UVs[0][0].V * Polygon->Material->MaterialVSize(),1),
								Polygon->Vertex[1],FVector(Triangle.UVs[1][0].U * Polygon->Material->MaterialUSize(),Triangle.UVs[1][0].V * Polygon->Material->MaterialVSize(),1),
								Polygon->Vertex[0],FVector(Triangle.UVs[2][0].U * Polygon->Material->MaterialUSize(),Triangle.UVs[2][0].V * Polygon->Material->MaterialVSize(),1),
								&Polygon->Base,&Polygon->TextureU,&Polygon->TextureV);
		}
	}

	Model->Linked = 1;
	GEditor->bspValidateBrush(Model,0,1);
	Model->BuildBound();

	unguard;
}

//
//	GetActorTriangles - Creates a static mesh from the triangles in a mesh.
//

void GetActorTriangles(TArray<FStaticMeshTriangle>& Triangles,TArray<FStaticMeshMaterial>& Materials,AActor* Actor)
{
	guard(GetActorTriangles);

	ULodMesh*	Mesh = Cast<ULodMesh>(Actor->Mesh);

	// Store the actor's location.
	FVector		StoredLocation	= Actor->Location;
	FVector		StoredMeshScale	= Actor->DrawScale3D;
	FRotator	StoredRotation	= Actor->Rotation;
	FLOAT		StoredScale		= Actor->DrawScale;
	
	// Move the actor to the world origin.
	Actor->Location			= FVector(0.0f,0.0f,0.0f);
	Actor->DrawScale3D		= FVector(1.f,1.f,1.f);
	Actor->Rotation			= FRotator(0,0,0);
	Actor->DrawScale		= 1.0f;

	// Get the vertices for the actor's current animation frame.
	UMeshInstance*	MeshInstance = Mesh->MeshGetInstance(Actor);
	FMatrix			MeshToWorld = MeshInstance->MeshToWorld();
	UBOOL			FlipVerts = Mesh->IsA(UVertMesh::StaticClass());
	
	// Restore the actor's location.
	Actor->Location			= StoredLocation;
	Actor->Rotation			= StoredRotation;
	Actor->DrawScale	    = StoredScale;
	Actor->DrawScale3D		= StoredMeshScale;

#if RESTRICTEXPORT
	// Don't allow conversion if authentication restrictions apply.
	if( ! Mesh->ValidateAuthenticationKey() )
	{		
		return;
	}
#endif

	// Separate handling of skeletal meshes.
	if( Mesh->IsA(USkeletalMesh::StaticClass()) )
	{	
		TArray<FVector>	NewVerts;	
		USkeletalMesh*	SkelMesh = Cast<USkeletalMesh>(Actor->Mesh);

		// Use LOD model 0.
		if( SkelMesh->LODModels.Num() )
		{
			FStaticLODModel* Model = &SkelMesh->LODModels(0);
			INT NumWedges = Model->SmoothStreamWedges;
			NewVerts.AddZeroed( NumWedges );
			// GetMeshVerts for skeletal meshes returns vertices in order of vertex buffer (i.e wedges; textured vertices.)
			if( MeshInstance )
				((USkeletalMeshInstance*)MeshInstance)->GetMeshVerts( Actor, &NewVerts(0), sizeof(FVector), NumWedges );
			
			// Ensure lazy arrays are loaded.
			Model->Wedges.Load();
			Model->Faces.Load();
			for( INT FaceIndex = 0; FaceIndex < Model->Faces.Num(); FaceIndex++) 
			{			
				FMeshFace&	Face = Model->Faces(FaceIndex);
				// Create a FStaticMeshTriangle to represent this face.
				FStaticMeshTriangle*  Triangle = new( Triangles ) FStaticMeshTriangle;
				UMaterial*			  Material = SkelMesh->Materials( SkelMesh->MeshMaterials(Face.MeshMaterialIndex).MaterialIndex );

				Triangle->MaterialIndex = FindMaterialIndex( Materials, Material );
				Triangle->SmoothingMask = 1;
				Triangle->NumUVs = 1;

				// For each vertex in the triangle...
				for( INT VertexIndex = 0; VertexIndex < 3; VertexIndex++ )
				{
					// Copy the actual vertex out of the indexed vertex array.
					INT	iWedge = Face.iWedge[ FlipVerts ? 2 - VertexIndex : VertexIndex ];
					FVector NewVertex;
					if( iWedge < NumWedges )
						NewVertex = MeshToWorld.TransformFVector( NewVerts(iWedge));
					else
						NewVertex = FVector( 0.f,0.f,0.f );

						Triangle->Vertices[VertexIndex] = MeshToWorld.TransformFVector( NewVertex );

					// Calculate the vertex's texture coordinates.
					Triangle->UVs[VertexIndex][0].U = Model->Wedges(iWedge).TexUV.U;
					Triangle->UVs[VertexIndex][0].V = Model->Wedges(iWedge).TexUV.V;
					Triangle->Colors[VertexIndex] = FColor(255,255,255,255);
				}
			}		
		}		
	}
	else // Vertex-animated mesh.
	{
		TArray<FVector>	Vertices;	
		INT	NumVertices = Mesh->ModelVerts;
		Vertices.AddZeroed( Mesh->ModelVerts );
		if( MeshInstance )
			((ULodMeshInstance*)MeshInstance)->GetMeshVerts( Actor, &Vertices(0), sizeof(FVector), NumVertices );

		// For each face in mesh. 
		for( INT FaceIndex = 0; FaceIndex < Mesh->Faces.Num(); FaceIndex++ )
		{
			FMeshFace&	Face = Mesh->Faces(FaceIndex);

			// Create a FStaticMeshTriangle to represent this face.

			FStaticMeshTriangle*	Triangle = new(Triangles) FStaticMeshTriangle;
			UMaterial*				Material = Mesh->Materials(Mesh->MeshMaterials(Face.MeshMaterialIndex).MaterialIndex);

			Triangle->MaterialIndex = FindMaterialIndex(Materials,Material);
			Triangle->SmoothingMask = 1;
			Triangle->NumUVs = 1;

			// For each vertex in the triangle...

			for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			{
				// Copy the actual vertex out of the indexed vertex array.
				INT	iWedge = Face.iWedge[FlipVerts ? 2 - VertexIndex : VertexIndex],
					iVertex = Mesh->Wedges(iWedge).iVertex;

				Triangle->Vertices[VertexIndex] = MeshToWorld.TransformFVector(Vertices(iVertex));

				// Calculate the vertex's texture coordinates.
				Triangle->UVs[VertexIndex][0].U = Mesh->Wedges(iWedge).TexUV.U;
				Triangle->UVs[VertexIndex][0].V = Mesh->Wedges(iWedge).TexUV.V;
				Triangle->Colors[VertexIndex] = FColor(255,255,255,255);
			}
		}
	}
	unguard;
}

//
//	CreateStaticMeshFromActor
//

UStaticMesh* CreateStaticMeshFromActor(UObject* Outer,FName Name,AActor* Actor)
{
	guard(CreateStaticMeshFromActor);

	GWarn->BeginSlowTask(TEXT("Creating static mesh..."),1);

	TArray<FStaticMeshTriangle>	Triangles;
	TArray<FStaticMeshMaterial>	Materials;

	GetActorTriangles(Triangles,Materials,Actor);

	UStaticMesh*	StaticMesh = CreateStaticMesh(Triangles,Materials,Outer,Name); 

	GWarn->EndSlowTask();

	return StaticMesh;

	unguard;
}

//
//	UStaticMeshFactory::StaticConstructor
//

void UStaticMeshFactory::StaticConstructor()
{
	guard(UStaticMeshFactory::StaticConstructor);

	SupportedClass = UStaticMesh::StaticClass();
	new(Formats)FString(TEXT("t3d;Unreal model text"));
	new(Formats)FString(TEXT("ase;ASCII 3D Studio file"));
	bCreateNew = 0;
	bText = 1;

	new(GetClass(),TEXT("Pitch"),RF_Public) UIntProperty(CPP_PROPERTY(Pitch),TEXT("Import"),0);
	new(GetClass(),TEXT("Roll"),RF_Public) UIntProperty(CPP_PROPERTY(Roll),TEXT("Import"),0);
	new(GetClass(),TEXT("Yaw"),RF_Public) UIntProperty(CPP_PROPERTY(Yaw),TEXT("Import"),0);

	unguard;
}

//
//	UStaticMeshFactory::UStaticMeshFactory
//

UStaticMeshFactory::UStaticMeshFactory()
{
	guard(UStaticMeshFactory::UStaticMeshFactory);
	unguard;
}

//
//	TransformPolys
//

void TransformPolys(UPolys* Polys,FCoords& Coords)
{
	guard(TransformPolys);

	for(INT PolygonIndex = 0;PolygonIndex < Polys->Element.Num();PolygonIndex++)
	{
		FPoly&	Polygon = Polys->Element(PolygonIndex);

		for(INT VertexIndex = 0;VertexIndex < Polygon.NumVertices;VertexIndex++)
			Polygon.Vertex[VertexIndex] = Polygon.Vertex[VertexIndex].TransformPointBy(Coords);

		Polygon.Base = Polygon.Base.TransformPointBy(Coords);
		Polygon.TextureU = Polygon.TextureU.TransformVectorBy(Coords);
		Polygon.TextureV = Polygon.TextureV.TransformVectorBy(Coords);
	}

	unguard;
}

//
//	FASEMaterial
//

struct FASEMaterial
{
	FASEMaterial()
	{
		Width = Height = 256;
		UTiling = VTiling = 1;
		Material = NULL;
		bTwoSided = bUnlit = bAlphaTexture = bMasked = bTranslucent = 0;
	}
	FASEMaterial( TCHAR* InName, INT InWidth, INT InHeight, FLOAT InUTiling, FLOAT InVTiling, UMaterial* InMaterial, UBOOL InTwoSided, UBOOL InUnlit, UBOOL InAlphaTexture, UBOOL InMasked, UBOOL InTranslucent )
	{
		appStrcpy( Name, InName );
		Width = InWidth;
		Height = InHeight;
		UTiling = InUTiling;
		VTiling = InVTiling;
		Material = InMaterial;
		bTwoSided = InTwoSided;
		bUnlit = InUnlit;
		bAlphaTexture = InAlphaTexture;
		bMasked = InMasked;
		bTranslucent = InTranslucent;
	}

	TCHAR Name[128];
	INT Width, Height;
	FLOAT UTiling, VTiling;
	UMaterial* Material;
	UBOOL bTwoSided, bUnlit, bAlphaTexture, bMasked, bTranslucent;
};

//
//	FASEMaterialHeader
//

struct FASEMaterialHeader
{
	TArray<FASEMaterial> Materials;
};

//
//	FASEMappingChannel
//

struct FASEMappingChannel
{
	TArray<FVector>	TexCoord;
	TArray<INT>		FaceTexCoordIdx;
};

//
//	UStaticMeshFactory::FactoryCreateText
//

UObject* UStaticMeshFactory::FactoryCreateText
(
	ULevel*				InLevel,
	UClass*				Class,
	UObject*			InParent,
	FName				Name,
	DWORD				Flags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	guard(UStaticMeshFactory::FactoryCreateText);

	const TCHAR*	BufferTemp = Buffer;
	FCoords			Transform = GMath.UnitCoords * FRotator(Pitch,Yaw,Roll);

	if(appStricmp(Type,TEXT("ASE")) == 0)
	{
		UStaticMesh*	StaticMesh = new(InParent,Name,Flags|RF_Public) UStaticMesh;

		INT First=1;
		FString StrLine, ExtraLine;
		while( ParseLine( &Buffer, StrLine ) )
		{
			const TCHAR* Str = *StrLine;
			if( appStrstr(Str,TEXT("*3DSMAX_ASCIIEXPORT")) && First )
			{
				debugf( NAME_Log, TEXT("Reading 3D Studio ASE file") );

				TArray<FVector> Vertex;						// 1 FVector per entry
				TArray<FColor>	Colors;						// 1 FColor per entry
				TArray<INT> FaceIdx;						// 3 INT's for vertex indices per entry
				TArray<INT>	FaceColorIdx;					// 3 INT's for color indices per entry
				TArray<INT> FaceMaterialsIdx;				// 1 INT for material ID per face
				TArray<FASEMappingChannel> MappingChannels; // 1 per mapping channel
				TArray<FASEMaterialHeader> ASEMaterialHeaders;	// 1 per material (multiple sub-materials inside each one)
				TArray<DWORD>	SmoothingGroups;			// 1 DWORD per face.
				TArray<FVector>	CollisionVertices;
				TArray<INT>		CollisionFaceIdx;
				INT				BaseVertexIndex = 0,
								BaseCollisionVertexIndex = 0;
				
				INT		NumVertex = 0,
						NumFaces = 0,
						NumTVertex = 0,
						NumTFaces = 0,
						NumCVertex = 0,
						NumCFaces = 0,
						ASEMaterialRef = -1,
						CurrentMappingChannel = MappingChannels.AddZeroed();
				UBOOL	CollisionGeometry = 0,
						IgnoreMcdGeometry = 0;

				enum {
					GROUP_NONE			= 0,
					GROUP_MATERIAL		= 1,
					GROUP_GEOM			= 2,
				} Group;

				enum {
					SECTION_NONE		= 0,
					SECTION_MATERIAL	= 1,
					SECTION_MAP_DIFFUSE	= 2,
					SECTION_VERTS		= 3,
					SECTION_FACES		= 4,
					SECTION_TVERTS		= 5,
					SECTION_TFACES		= 6,
					SECTION_CVERTS		= 7,
					SECTION_CFACES		= 8,
				} Section;

				Group = GROUP_NONE;
				Section = SECTION_NONE;
				while( ParseLine( &Buffer, StrLine ) )
				{
					Str = *StrLine;

					if( Group == GROUP_NONE )
					{
						if( StrLine.InStr(TEXT("*MATERIAL_LIST")) != -1 )
							Group = GROUP_MATERIAL;
						else if( StrLine.InStr(TEXT("*GEOMOBJECT")) != -1 )
							Group = GROUP_GEOM;
					}
					else if ( Group == GROUP_MATERIAL )
					{
						static FLOAT UTiling = 1, VTiling = 1;
						static UMaterial* Material = NULL;
						static INT Height = 256, Width = 256;
						static UBOOL bTwoSided = 0, bUnlit = 0, bAlphaTexture = 0, bMasked = 0, bTranslucent = 0;

						// Determine the section and/or extract individual values
						if( StrLine == TEXT("}") )
							Group = GROUP_NONE;
						else if( StrLine.InStr(TEXT("*MATERIAL ")) != -1 )
							Section = SECTION_MATERIAL;
						else if( StrLine.InStr(TEXT("*MATERIAL_WIRE")) != -1 )
						{
							if( StrLine.InStr(TEXT("*MATERIAL_WIRESIZE")) == -1 )
								bTranslucent = 1;
						}
						else if( StrLine.InStr(TEXT("*MATERIAL_TWOSIDED")) != -1 )
						{
							bTwoSided = 1;
						}
						else if( StrLine.InStr(TEXT("*MATERIAL_SELFILLUM")) != -1 )
						{
							INT Pos = StrLine.InStr( TEXT("*") );
							FString NewStr = StrLine.Right( StrLine.Len() - Pos );
							FLOAT temp;
							appSSCANF( *NewStr, TEXT("*MATERIAL_SELFILLUM %f"), &temp );
							if( temp == 100.f || temp == 1.f )
								bUnlit = 1;
						}
						else if( StrLine.InStr(TEXT("*MATERIAL_TRANSPARENCY")) != -1 )
						{
							INT Pos = StrLine.InStr( TEXT("*") );
							FString NewStr = StrLine.Right( StrLine.Len() - Pos );
							FLOAT temp;
							appSSCANF( *NewStr, TEXT("*MATERIAL_TRANSPARENCY %f"), &temp );
							if( temp > 0.f )
								bAlphaTexture = 1;
						}
						else if( StrLine.InStr(TEXT("*MATERIAL_SHADING")) != -1 )
						{
							INT Pos = StrLine.InStr( TEXT("*") );
							FString NewStr = StrLine.Right( StrLine.Len() - Pos );
							TCHAR Buffer[20];
							appSSCANF( *NewStr, TEXT("*MATERIAL_SHADING %s"), Buffer );
							if( !appStrcmp( Buffer, TEXT("Constant") ) )
								bMasked = 1;
						}
						else if( StrLine.InStr(TEXT("*MAP_DIFFUSE")) != -1 )
						{
							Section = SECTION_MAP_DIFFUSE;
							Material = NULL;
							UTiling = VTiling = 1;
							Width = Height = 256;
						}
						else
						{
							if ( Section == SECTION_MATERIAL )
							{
								// We are entering a new material definition.  Allocate a new material header.
								new( ASEMaterialHeaders )FASEMaterialHeader();
								Section = SECTION_NONE;
							}
							else if ( Section == SECTION_MAP_DIFFUSE )
							{
								if( StrLine.InStr(TEXT("*BITMAP")) != -1 )
								{
									// Remove tabs from the front of this string.  The number of tabs differs
									// depending on how many materials are in the file.
									INT Pos = StrLine.InStr( TEXT("*") );
									FString NewStr = StrLine.Right( StrLine.Len() - Pos );

									NewStr = NewStr.Right( NewStr.Len() - NewStr.InStr(TEXT("\\"), -1 ) - 1 );	// Strip off path info
									NewStr = NewStr.Left( NewStr.Len() - 5 );									// Strip off '.bmp"' at the end

									// Find the texture
									Material = NULL;
									for( TObjectIterator<UMaterial> It; It ; ++It )
									{
										FString TexName = It->GetName();
										if( !appStrcmp( *TexName.Caps(), *NewStr.Caps() ) )
										{
											Material = *It;
											Width = Material->MaterialUSize();
											Height = Material->MaterialVSize();
											break;
										}
									}
								}
								else if( StrLine.InStr(TEXT("*UVW_U_TILING")) != -1 )
								{
									INT Pos = StrLine.InStr( TEXT("*") );
									FString NewStr = StrLine.Right( StrLine.Len() - Pos );
									appSSCANF( *NewStr, TEXT("*UVW_U_TILING %f"), &UTiling );
								}
								else if( StrLine.InStr(TEXT("*UVW_V_TILING")) != -1 )
								{
									INT Pos = StrLine.InStr( TEXT("*") );
									FString NewStr = StrLine.Right( StrLine.Len() - Pos );
									appSSCANF( *NewStr, TEXT("*UVW_V_TILING %f"), &VTiling );

									check(ASEMaterialHeaders.Num());
									new( ASEMaterialHeaders(ASEMaterialHeaders.Num()-1).Materials )FASEMaterial( (TCHAR*)*Name, Width, Height, UTiling, VTiling, Material, bTwoSided, bUnlit, bAlphaTexture, bMasked, bTranslucent );

									Section = SECTION_NONE;
									bTwoSided = bUnlit = bAlphaTexture = bMasked = bTranslucent = 0;
								}
							}
						}
					}
					else if ( Group == GROUP_GEOM )
					{
						// Determine the section and/or extract individual values
						if( StrLine == TEXT("}") )
						{
							CollisionGeometry = 0;
							IgnoreMcdGeometry = 0;
							Group = GROUP_NONE;

							BaseVertexIndex = Vertex.Num();
							BaseCollisionVertexIndex = CollisionVertices.Num();
						}
						// See if this is an MCD thing
						else if( StrLine.InStr(TEXT("*NODE_NAME")) != -1 )
						{
							TCHAR NodeName[512];                     
							appSSCANF( Str, TEXT("\t*NODE_NAME \"%s\""), &NodeName );
							if( appStrstr(NodeName, TEXT("MCDCX")) == NodeName )
							{	
								IgnoreMcdGeometry = 0;
								CollisionGeometry = 1;
							}
							else if( appStrstr(NodeName, TEXT("MCD")) == NodeName )
							{
								IgnoreMcdGeometry = 1;
								CollisionGeometry = 0;
							}
							else 
							{
								IgnoreMcdGeometry = 0;
								CollisionGeometry = 0;
							}
						}

						// Now do nothing if it's an MCD Geom
						if( !IgnoreMcdGeometry )
						{              
							if( StrLine.InStr(TEXT("*MESH_NUMVERTEX")) != -1 )
								appSSCANF( Str, TEXT("\t\t*MESH_NUMVERTEX %d"), &NumVertex );
							else if( StrLine.InStr(TEXT("*MESH_NUMFACES")) != -1 )
								appSSCANF( Str, TEXT("\t\t*MESH_NUMFACES %d"), &NumFaces );
							else if( StrLine.InStr(TEXT("*MESH_VERTEX_LIST")) != -1 )
								Section = SECTION_VERTS;
							else if( StrLine.InStr(TEXT("*MESH_FACE_LIST")) != -1 )
								Section = SECTION_FACES;
							else if( StrLine.InStr(TEXT("*MESH_NUMTVERTEX")) != -1 )
								appSSCANF( Str, TEXT("\t\t*MESH_NUMTVERTEX %d"), &NumTVertex );
							else if( StrLine.InStr(TEXT("*MESH_TVERTLIST")) != -1 )
								Section = SECTION_TVERTS;
							else if( StrLine.InStr(TEXT("*MESH_NUMTVFACES")) != -1 )
								appSSCANF( Str, TEXT("\t\t*MESH_NUMTVFACES %d"), &NumTFaces );
							else if( StrLine.InStr(TEXT("*MESH_NUMTVERTEX")) != -1 )
								appSSCANF( Str, TEXT("\t\t*MESH_NUMCVERTEX %d"), &NumCVertex );
							else if( StrLine.InStr(TEXT("*MESH_CVERTLIST")) != -1 )
								Section = SECTION_CVERTS;
							else if( StrLine.InStr(TEXT("*MESH_NUMCVFACES")) != -1 )
								appSSCANF( Str, TEXT("\t\t*MESH_NUMCVFACES %d"), &NumCFaces );
							else if(StrLine.InStr(TEXT("*MESH_CFACELIST")) != -1 )
								Section = SECTION_CFACES;
							else if( StrLine.InStr(TEXT("*MATERIAL_REF")) != -1 )
								appSSCANF( Str, TEXT("\t*MATERIAL_REF %d"), &ASEMaterialRef );
							else if( StrLine.InStr(TEXT("*MESH_TFACELIST")) != -1 )
								Section = SECTION_TFACES;
							else if( StrLine.InStr(TEXT("*MESH_MAPPINGCHANNEL")) != -1 )
							{
								INT	MappingChannel = -1;
								appSSCANF( Str, TEXT("\t\t*MESH_MAPPINGCHANNEL %d {"), &MappingChannel );

								if(!CollisionGeometry)
									CurrentMappingChannel = MappingChannels.AddZeroed();
							}
							else
							{
								// Extract data specific to sections
								if( Section == SECTION_VERTS )
								{
									if( StrLine.InStr(TEXT("\t\t}")) != -1 )
										Section = SECTION_NONE;
									else
									{
										int temp;
										FVector vtx;
										appSSCANF( Str, TEXT("\t\t\t*MESH_VERTEX    %d\t%f\t%f\t%f"),
											&temp, &vtx.X, &vtx.Y, &vtx.Z );

										if(CollisionGeometry)
											new(CollisionVertices)FVector(vtx);
										else
											new(Vertex)FVector(vtx);
									}
								}
								else if( Section == SECTION_FACES )
								{
									if( StrLine.InStr(TEXT("\t\t}")) != -1 )
										Section = SECTION_NONE;
									else
									{
										INT temp, idx1, idx2, idx3;
										appSSCANF( Str, TEXT("\t\t\t*MESH_FACE %d:    A: %d B: %d C: %d"),
											&temp, &idx1, &idx2, &idx3 );

										if(CollisionGeometry)
										{
											new(CollisionFaceIdx)INT(BaseCollisionVertexIndex + idx1);
											new(CollisionFaceIdx)INT(BaseCollisionVertexIndex + idx2);
											new(CollisionFaceIdx)INT(BaseCollisionVertexIndex + idx3);
										}
										else
										{
											new(FaceIdx)INT(BaseVertexIndex + idx1);
											new(FaceIdx)INT(BaseVertexIndex + idx2);
											new(FaceIdx)INT(BaseVertexIndex + idx3);

											FString SmoothTag(TEXT("*MESH_SMOOTHING"));
											INT SmGroupsLocation = StrLine.InStr( SmoothTag );											
											if( SmGroupsLocation != -1 )
											{
											FString	SmoothingString;
												DWORD	SmoothingMask = 0;

												// Cut off right part of StrLine to get smoothing group(s).
												SmoothingString = StrLine.Right( StrLine.Len() - SmGroupsLocation - SmoothTag.Len() );

												while(SmoothingString.Len())
												{
													// Process comma-separated multiple smoothing groups.
													INT	Length = SmoothingString.InStr(TEXT(","));
													INT SmoothingGroup = (Length != -1) ? appAtoi(*SmoothingString.Left(Length)) : appAtoi(*SmoothingString);

													if(SmoothingGroup <= 32)
														SmoothingMask |= (1 << (SmoothingGroup - 1));

													SmoothingString = (Length != -1) ? SmoothingString.Right(SmoothingString.Len() - Length - 1) : TEXT("");
												};

												SmoothingGroups.AddItem(SmoothingMask);
											}
											else
												SmoothingGroups.AddItem(0);

											// Sometimes "MESH_SMOOTHING" is a blank instead of a number, so we just grab the 
											// part of the string we need and parse out the material id.
											INT MaterialID;
											StrLine = StrLine.Right( StrLine.Len() - StrLine.InStr( TEXT("*MESH_MTLID"), -1 ) - 1 );
											appSSCANF( *StrLine , TEXT("MESH_MTLID %d"), &MaterialID );
											new(FaceMaterialsIdx)INT(MaterialID);
										}
									}
								}
								else if( Section == SECTION_TVERTS )
								{
									if( StrLine.InStr(TEXT("}")) != -1 )
										Section = SECTION_NONE;
									else
									{
										int temp;
										FVector vtx;
										appSSCANF( Str, TEXT("\t\t\t*MESH_TVERT %d\t%f\t%f"),
											&temp, &vtx.X, &vtx.Y );
										vtx.Z = 0;
										
										if(!CollisionGeometry)
											new(MappingChannels(CurrentMappingChannel).TexCoord)FVector(vtx);
									}
								}
								else if( Section == SECTION_TFACES )
								{
									if( StrLine.InStr(TEXT("}")) != -1 )
										Section = SECTION_NONE;
									else
									{
										int temp, idx1, idx2, idx3;
										appSSCANF( Str, TEXT("\t\t\t*MESH_TFACE %d\t%d\t%d\t%d"),
											&temp, &idx1, &idx2, &idx3 );

										if(!CollisionGeometry)
										{
											new(MappingChannels(CurrentMappingChannel).FaceTexCoordIdx)INT(idx1);
											new(MappingChannels(CurrentMappingChannel).FaceTexCoordIdx)INT(idx2);
											new(MappingChannels(CurrentMappingChannel).FaceTexCoordIdx)INT(idx3);
										}
									}
								}
								else if( Section == SECTION_CVERTS )
								{
									if( StrLine.InStr(TEXT("}")) != -1 )
										Section = SECTION_NONE;
									else
									{
										int	temp;
										FVector color;
										appSSCANF( Str, TEXT("\t\t\t*MESH_VERTCOL %d\t%f\t%f\t%f"),
											&temp, &color.X, &color.Y, &color.Z );

										if(!CollisionGeometry)
											new(Colors) FColor(Clamp(appFloor(color.X*255),0,255),Clamp(appFloor(color.Y*255),0,255),Clamp(appFloor(color.Z*255),0,255),Clamp(appFloor(color.X*255),0,255));
									}
								}
								else if( Section == SECTION_CFACES )
								{
									if( StrLine.InStr(TEXT("}")) != -1 )
										Section = SECTION_NONE;
									else
									{
										int	temp, idx1, idx2, idx3;
										appSSCANF( Str, TEXT("\t\t\t*MESH_CFACE %d\t%d\t%d\t%d"),
											&temp, &idx1, &idx2, &idx3 );

										if(!CollisionGeometry)
										{
											new(FaceColorIdx) INT(idx1);
											new(FaceColorIdx) INT(idx2);
											new(FaceColorIdx) INT(idx3);
										}
									}
								}
							}
						}
					}
				}

				// Create the polys from the gathered info.
				for(INT ChannelIndex = 0;ChannelIndex < MappingChannels.Num();ChannelIndex++)
				{
					if( FaceIdx.Num() != MappingChannels(ChannelIndex).FaceTexCoordIdx.Num() )
					{
						appMsgf( 0, TEXT("ASE Importer Error : Did you forget to include geometry AND materials in the export?\n\nFaces : %d, Texture Coordinates(channel %d) : %d"),
							FaceIdx.Num(), ChannelIndex, MappingChannels(ChannelIndex).FaceTexCoordIdx.Num());
						continue;
					}
				}

				// Import materials.

				if( ASEMaterialRef == -1 || !ASEMaterialHeaders(ASEMaterialRef).Materials.Num() )
				{
					appMsgf( 0, TEXT("ASE Importer Warning : This object has no material (current texture will be applied to object).") );

					new(StaticMesh->Materials) FStaticMeshMaterial(GEditor->CurrentMaterial);
				}
				else
				{
					for(INT MaterialIndex = 0;MaterialIndex < ASEMaterialHeaders(ASEMaterialRef).Materials.Num();MaterialIndex++)
						new(StaticMesh->Materials) FStaticMeshMaterial(ASEMaterialHeaders(ASEMaterialRef).Materials(MaterialIndex).Material);
				}

				if(!GBuildStaticMeshCollision)
					for(INT MaterialIndex = 0;MaterialIndex < StaticMesh->Materials.Num();MaterialIndex++)
						StaticMesh->Materials(MaterialIndex).EnableCollision = 0;

				// Create a BSP tree from the imported collision geometry.

				if(CollisionFaceIdx.Num())
				{
					StaticMesh->CollisionModel = new(StaticMesh->GetOuter()) UModel(NULL,1);

					for(INT x = 0;x < CollisionFaceIdx.Num();x += 3)
					{
						FPoly*	Poly = new(StaticMesh->CollisionModel->Polys->Element) FPoly();

						Poly->Init();

						Poly->Vertex[0] = CollisionVertices(CollisionFaceIdx(x + 2)) * FVector(-1,1,1);
						Poly->Vertex[1] = CollisionVertices(CollisionFaceIdx(x + 1)) * FVector(-1,1,1);
						Poly->Vertex[2] = CollisionVertices(CollisionFaceIdx(x + 0)) * FVector(-1,1,1);
						Poly->iLink = x / 3;
						Poly->NumVertices = 3;

						Poly->CalcNormal(1);
					}

					// Build bounding box.

					StaticMesh->CollisionModel->BuildBound();

					// Build BSP for the brush.

					GEditor->bspBuild(StaticMesh->CollisionModel,BSP_Good,15,70,1,0);
					GEditor->bspRefresh(StaticMesh->CollisionModel,1);
					GEditor->bspBuildBounds(StaticMesh->CollisionModel);

#ifdef WITH_KARMA
					// If we dont already have physics props, construct them here.
					if(!StaticMesh->KPhysicsProps)
						StaticMesh->KPhysicsProps = ConstructObject<UKMeshProps>(UKMeshProps::StaticClass(), StaticMesh);

					// Convert collision model into a collection of Karma convex hulls.
					// NB: This removes any convex hulls that were already part of the collision data.
					KModelToHulls(&StaticMesh->KPhysicsProps->AggGeom, StaticMesh->CollisionModel, FVector(0, 0, 0));

					// Then re-calc the mass props.
					KUpdateMassProps(StaticMesh->KPhysicsProps);
#endif
				}

				for( int x = 0 ; x < FaceIdx.Num() ; x += 3 )
				{
					FStaticMeshTriangle*	Triangle = new(StaticMesh->RawTriangles) FStaticMeshTriangle;
					
					Triangle->Vertices[0] = Vertex( FaceIdx(x) ) * FVector(-1,1,1);
					Triangle->Vertices[1] = Vertex( FaceIdx(x+1) ) * FVector(-1,1,1);
					Triangle->Vertices[2] = Vertex( FaceIdx(x+2) ) * FVector(-1,1,1);

					Triangle->SmoothingMask = SmoothingGroups(x / 3);

					if(FaceMaterialsIdx(x / 3) < StaticMesh->Materials.Num())
						Triangle->MaterialIndex = FaceMaterialsIdx(x / 3);
					else
						Triangle->MaterialIndex = 0;

					FASEMaterial	ASEMaterial;
					if( ASEMaterialRef != -1 )
						if( ASEMaterialHeaders(ASEMaterialRef).Materials.Num() )
							if( ASEMaterialHeaders(ASEMaterialRef).Materials.Num() == 1 )
								ASEMaterial = ASEMaterialHeaders(ASEMaterialRef).Materials(0);
							else
							{
								// Sometimes invalid material references appear in the ASE file.  We can't do anything about
								// it, so when that happens just use the first material.
								if( FaceMaterialsIdx(x/3) >= ASEMaterialHeaders(ASEMaterialRef).Materials.Num() )
									ASEMaterial = ASEMaterialHeaders(ASEMaterialRef).Materials(0);
								else
									ASEMaterial = ASEMaterialHeaders(ASEMaterialRef).Materials( FaceMaterialsIdx(x/3) );
							}

					for(INT ChannelIndex = 0;ChannelIndex < MappingChannels.Num() && ChannelIndex < 8;ChannelIndex++)
					{
						if(MappingChannels(ChannelIndex).FaceTexCoordIdx.Num() == FaceIdx.Num())
						{
							FVector	ST1 = MappingChannels(ChannelIndex).TexCoord(MappingChannels(ChannelIndex).FaceTexCoordIdx(x + 0)),
									ST2 = MappingChannels(ChannelIndex).TexCoord(MappingChannels(ChannelIndex).FaceTexCoordIdx(x + 1)),
									ST3 = MappingChannels(ChannelIndex).TexCoord(MappingChannels(ChannelIndex).FaceTexCoordIdx(x + 2));

							Triangle->UVs[0][ChannelIndex].U = ST1.X * ASEMaterial.UTiling;
							Triangle->UVs[0][ChannelIndex].V = (1.0f - ST1.Y) * ASEMaterial.VTiling;

							Triangle->UVs[1][ChannelIndex].U = ST2.X * ASEMaterial.UTiling;
							Triangle->UVs[1][ChannelIndex].V = (1.0f - ST2.Y) * ASEMaterial.VTiling;

							Triangle->UVs[2][ChannelIndex].U = ST3.X * ASEMaterial.UTiling;
							Triangle->UVs[2][ChannelIndex].V = (1.0f - ST3.Y) * ASEMaterial.VTiling;
						}
					}

					Triangle->LegacyPolyFlags = 0;

					Triangle->NumUVs = Min(MappingChannels.Num(),8);

					if(FaceColorIdx.Num() == FaceIdx.Num())
					{
						Triangle->Colors[0] = Colors(FaceColorIdx(x + 0));
						Triangle->Colors[1] = Colors(FaceColorIdx(x + 1));
						Triangle->Colors[2] = Colors(FaceColorIdx(x + 2));
					}
					else
					{
						Triangle->Colors[0] = FColor(255,255,255,255);
						Triangle->Colors[1] = FColor(255,255,255,255);
						Triangle->Colors[2] = FColor(255,255,255,255);
					}
				}

				StaticMesh->Build();

				debugf( NAME_Log, TEXT("Imported %i vertices, %i faces"), NumVertex, NumFaces );
			}
		}

		return StaticMesh;
	}
	else if(appStricmp(Type,TEXT("T3D")) == 0)
	{
		if(GetBEGIN(&BufferTemp,TEXT("PolyList")))
		{
			UModelFactory*	ModelFactory = new UModelFactory;
			UModel*			Model = (UModel*) ModelFactory->FactoryCreateText(InLevel,UModel::StaticClass(),InParent,NAME_None,0,NULL,TEXT("T3D"),Buffer,BufferEnd,Warn);

			delete ModelFactory;

			TransformPolys(Model->Polys,Transform);

			if(Model)
				return CreateStaticMeshFromBrush(InParent,Name,NULL,Model);
			else
				return NULL;
		}
		else
		{
			UStaticMesh*	StaticMesh = new(InParent,Name,Flags|RF_Public) UStaticMesh;

			FLOAT	Version = 0.0f;

			verify(Parse(Buffer,TEXT("Version="),Version));

			Parse(Buffer,TEXT("BoundingBox.Min.X="),StaticMesh->BoundingBox.Min.X);
			Parse(Buffer,TEXT("BoundingBox.Min.Y="),StaticMesh->BoundingBox.Min.Y);
			Parse(Buffer,TEXT("BoundingBox.Min.Z="),StaticMesh->BoundingBox.Min.Z);
			Parse(Buffer,TEXT("BoundingBox.Max.X="),StaticMesh->BoundingBox.Max.X);
			Parse(Buffer,TEXT("BoundingBox.Max.Y="),StaticMesh->BoundingBox.Max.Y);
			Parse(Buffer,TEXT("BoundingBox.Max.Z="),StaticMesh->BoundingBox.Max.Z);

			if(Version <= 1.1f)
			{
				const TCHAR*	StrPtr;
				FString			StrLine;

				while(ParseLine(&Buffer,StrLine))
				{
					StrPtr = *StrLine;

					if(GetEND(&StrPtr,TEXT("StaticMesh")))
						break;
					else if(GetBEGIN(&StrPtr,TEXT("Vertices")))
					{
						FString			VertexLine;
						const TCHAR*	VertexLinePtr;

						StaticMesh->LegacyVertexBuffer = new(StaticMesh) UVertexBuffer;

						while(ParseLine(&Buffer,VertexLine))
						{
							VertexLinePtr = *VertexLine;

							if(GetEND(&VertexLinePtr,TEXT("Vertices")) || !ParseCommand(&VertexLinePtr,TEXT("Vertex")))
								break;

							FUntransformedVertex*	NewVertex = new(StaticMesh->LegacyVertexBuffer->Vertices) FUntransformedVertex;

							NewVertex->Color = 0;

							Parse(VertexLinePtr,TEXT("Position.X="),NewVertex->Position.X);
							Parse(VertexLinePtr,TEXT("Position.Y="),NewVertex->Position.Y);
							Parse(VertexLinePtr,TEXT("Position.Z="),NewVertex->Position.Z);
							Parse(VertexLinePtr,TEXT("Normal.X="),NewVertex->Normal.X);
							Parse(VertexLinePtr,TEXT("Normal.Y="),NewVertex->Normal.Y);
							Parse(VertexLinePtr,TEXT("Normal.Z="),NewVertex->Normal.Z);
							Parse(VertexLinePtr,TEXT("U="),NewVertex->U);
							Parse(VertexLinePtr,TEXT("V="),NewVertex->V);
						};

						StaticMesh->LegacyVertexBuffer->Revision = 1;
					}
					else if(GetBEGIN(&StrPtr,TEXT("Indices")))
					{
						FString			IndexLine;
						const TCHAR*	IndexLinePtr;

						StaticMesh->LegacyIndexBuffer = new(StaticMesh) UIndexBuffer;

						while(ParseLine(&Buffer,IndexLine))
						{
							IndexLinePtr = *IndexLine;

							if(GetEND(&IndexLinePtr,TEXT("Indices")) || !ParseCommand(&IndexLinePtr,TEXT("Index")))
								break;

							_WORD*	NewIndex = new(StaticMesh->LegacyIndexBuffer->Indices) _WORD;

							Parse(IndexLinePtr,TEXT("Index="),*NewIndex);
						};

						StaticMesh->LegacyIndexBuffer->Revision = 1;
					}
					else if(GetBEGIN(&StrPtr,TEXT("Sections")))
					{
						FString			SectionLine;
						const TCHAR*	SectionLinePtr;

						while(ParseLine(&Buffer,SectionLine))
						{
							SectionLinePtr = *SectionLine;

							if(GetEND(&SectionLinePtr,TEXT("Sections")) || !ParseCommand(&SectionLinePtr,TEXT("Section")))
								break;

							FStaticMeshSection*	Section = new(StaticMesh->Sections) FStaticMeshSection;

							if(!ParseObject<UMaterial>(SectionLinePtr,TEXT("Texture="),Section->LegacyMaterial,ANY_PACKAGE))
								Section->LegacyMaterial = NULL;

							Parse(SectionLinePtr,TEXT("FirstIndex="),Section->LegacyFirstIndex);
							Parse(SectionLinePtr,TEXT("NumTriangles="),Section->NumTriangles);

							if(Version >= 1.1)
								Parse(SectionLinePtr,TEXT("IsStrip="),Section->IsStrip);
							else
								Section->IsStrip = 0;

							if(!Section->LegacyMaterial)
								Section->LegacyMaterial = GEditor->Level->GetLevelInfo()->DefaultTexture;
						};
					}
					else if(GetBEGIN(&StrPtr,TEXT("PolyList")) && Version < 1.1)
					{
						UPolysFactory*	PolysFactory = new UPolysFactory;

						PolysFactory->FactoryCreateText(InLevel,UPolys::StaticClass(),InParent,NAME_None,0,NULL,Type,Buffer,BufferEnd,Warn);

						delete PolysFactory;
					}
				};

				check(StaticMesh->LegacyVertexBuffer);
				check(StaticMesh->LegacyIndexBuffer);

				StaticMesh->InternalVersion = 0;
				StaticMesh->PostLoad();
			}
			else
			{
				while(1)
				{
					TCHAR			LineBuffer[1024];
					const TCHAR*	LineBufferPtr;

					ParseLine(&Buffer,LineBuffer,1024);
					LineBufferPtr = LineBuffer;

					if(GetBEGIN(&LineBufferPtr,TEXT("Triangle")))
					{
						FStaticMeshTriangle*	Triangle = new(StaticMesh->RawTriangles) FStaticMeshTriangle;

						while(1)
						{
							if(!ParseLine(&Buffer,LineBuffer,1024,1))
								break;

							LineBufferPtr = LineBuffer;

							if(GetEND(&LineBufferPtr,TEXT("Triangle")))
								break;
							else if(ParseCommand(&LineBufferPtr,TEXT("Texture")))
							{
								FString	TextureName = ParseToken(LineBufferPtr,0);

								Triangle->MaterialIndex = FindMaterialIndex(StaticMesh->Materials,Cast<UMaterial>(UObject::StaticLoadObject(UMaterial::StaticClass(),NULL,*TextureName,NULL,0,NULL)));
							}
							else if(ParseCommand(&LineBufferPtr,TEXT("PolyFlags")))
							{
								Triangle->LegacyPolyFlags = (DWORD) appAtoi(*ParseToken(LineBufferPtr,0));
							}
							else if(ParseCommand(&LineBufferPtr,TEXT("SmoothingMask")))
							{
								Triangle->SmoothingMask = (DWORD) appAtoi(*ParseToken(LineBufferPtr,0));
							}
							else if(ParseCommand(&LineBufferPtr,TEXT("Vertex")))
							{
								INT	VertexIndex = appAtoi(*ParseToken(LineBufferPtr,0));

								Triangle->Vertices[VertexIndex].X = appAtof(*ParseToken(LineBufferPtr,0));
								Triangle->Vertices[VertexIndex].Y = appAtof(*ParseToken(LineBufferPtr,0));
								Triangle->Vertices[VertexIndex].Z = appAtof(*ParseToken(LineBufferPtr,0));

								Triangle->UVs[VertexIndex][0].U = appAtof(*ParseToken(LineBufferPtr,0));
								Triangle->UVs[VertexIndex][0].V = appAtof(*ParseToken(LineBufferPtr,0));
							}
						};

						Triangle->NumUVs = 1;
					}
					else if(GetEND(&LineBufferPtr,TEXT("StaticMesh")))
						break;
				};

				GWarn->BeginSlowTask(TEXT("Rebuilding static mesh"),1);

				StaticMesh->Build();

				GWarn->EndSlowTask();
			}

			return StaticMesh;
		}
	}
	else
		return NULL;

	unguard;
}

IMPLEMENT_CLASS(UStaticMeshFactory);

//
//	UStaticMeshExporterT3D::StaticConstructor
//

void UStaticMeshExporterT3D::StaticConstructor()
{
	guard(UStaticMeshExporterT3D::StaticConstructor);

	SupportedClass = UStaticMesh::StaticClass();
	bText = 1;
	new(Formats)FString(TEXT("T3D"));

	unguard;
}

//
//	UStaticMeshExporterT3D::ExportText
//

UBOOL UStaticMeshExporterT3D::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	guard(UStaticMeshExporterT3D::ExportText);

	UStaticMesh*	StaticMesh = CastChecked<UStaticMesh>(Object);

#if RESTRICTEXPORT
	// Don't allow conversion if authentication restrictions apply.	
	if( StaticMesh && !StaticMesh->ValidateAuthenticationKey() )
	{		
		return 1;
	}
#endif


	Ar.Logf(TEXT("%sBegin StaticMesh Name=%s\r\n"),appSpc(TextIndent),Object->GetName());
	Ar.Logf(TEXT("%sVersion=%f BoundingBox.Min.X=%f BoundingBox.Min.Y=%f BoundingBox.Min.Z=%f BoundingBox.Max.X=%f BoundingBox.Max.Y=%f BoundingBox.Max.Z=%f\r\n"),appSpc(TextIndent + 4),2.0f,StaticMesh->BoundingBox.Min.X,StaticMesh->BoundingBox.Min.Y,StaticMesh->BoundingBox.Min.Z,StaticMesh->BoundingBox.Max.X,StaticMesh->BoundingBox.Max.Y,StaticMesh->BoundingBox.Max.Z);

	if(!StaticMesh->RawTriangles.Num())
		StaticMesh->RawTriangles.Load();

	for(INT TriangleIndex = 0;TriangleIndex < StaticMesh->RawTriangles.Num();TriangleIndex++)
	{
		FStaticMeshTriangle&	Triangle = StaticMesh->RawTriangles(TriangleIndex);

		Ar.Logf(TEXT("%sBegin Triangle\r\n"),appSpc(TextIndent + 4));

		Ar.Logf(TEXT("%sTexture %s\r\n"),appSpc(TextIndent + 8),StaticMesh->Materials(Triangle.MaterialIndex).Material->GetPathName());
		Ar.Logf(TEXT("%sSmoothingMask %i\r\n"),appSpc(TextIndent + 8),Triangle.SmoothingMask);

		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			Ar.Logf(TEXT("%sVertex %u %f %f %f %f %f\r\n"),appSpc(TextIndent + 8),
															VertexIndex,
															Triangle.Vertices[VertexIndex].X,
															Triangle.Vertices[VertexIndex].Y,
															Triangle.Vertices[VertexIndex].Z,
															Triangle.UVs[VertexIndex][0].U,
															Triangle.UVs[VertexIndex][0].V
															);

		Ar.Logf(TEXT("%sEnd Triangle\r\n"),appSpc(TextIndent + 4));
	}

	Ar.Logf(TEXT("%sEnd StaticMesh\r\n"),appSpc(TextIndent));

	return 1;
	unguard;
}
IMPLEMENT_CLASS(UStaticMeshExporterT3D);

//
// PLE import specific -  create a BSP tree from the imported collision geometry.
//
void BuildCollisionFromList( UStaticMesh* StaticMesh, const TArray< FVector > & collisionList )
{
	guard(BuildCollisionFromList);

	if( 0 < collisionList.Num() )
	{
		StaticMesh->CollisionModel = new( StaticMesh->GetOuter() ) UModel( NULL, 1 );

		for( INT x = 0;  x < collisionList.Num();  x += 3 )
		{
			FPoly *	Poly = new( StaticMesh->CollisionModel->Polys->Element ) FPoly();

			Poly->Init();

			Poly->Vertex[ 0 ] = collisionList( x + 0 ) * FVector(-1,1,1);
			Poly->Vertex[ 1 ] = collisionList( x + 1 ) * FVector(-1,1,1);
			Poly->Vertex[ 2 ] = collisionList( x + 2 ) * FVector(-1,1,1);
			Poly->iLink       = x / 3;
			Poly->NumVertices = 3;

			Poly->CalcNormal( 1 );
		}

		// Build bounding box.

		StaticMesh->CollisionModel->BuildBound();

		// Build BSP 
		
		GEditor->bspBuild      ( StaticMesh->CollisionModel, BSP_Good, 15, 70, 1, 0 );
		GEditor->bspRefresh    ( StaticMesh->CollisionModel, 1 );
		GEditor->bspBuildBounds( StaticMesh->CollisionModel );
	}
	unguard;
}

//
// PLE import specific - build with normals.
//
void UEditorEngine::CreateWithNormals( UStaticMesh* StaticMesh,
								TArray< FStaticMeshTriangle > & triList,
								TArray< FVector >             & collisionList,
							    TArray< FStaticMeshMaterial > & materialList,
								TArray< FVector >             & normalList )
{
	guard( UEditorEngine::CreateWithNormals );

	Sort( &triList(0), triList.Num() );

	StaticMesh->RawTriangles.Add( triList.Num() );
	appMemcpy( &StaticMesh->RawTriangles(0), &triList(0), 
				triList.Num() * sizeof( FStaticMeshTriangle ) );

	StaticMesh->Materials.Add( materialList.Num() );
	appMemcpy( &StaticMesh->Materials(0), &materialList(0), 
				materialList.Num() * sizeof( FStaticMeshMaterial ) );

	BuildCollisionFromList( StaticMesh, collisionList );

	StaticMesh->UseSimpleBoxCollision = 0; // sjs

	StaticMesh->Build( 0, &normalList(0) );

	// Assign a valid content authentication key. 
	StaticMesh->AuthenticationKey = StaticMesh->CreateAuthenticationKey();

	unguard;
}
