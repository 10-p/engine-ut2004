/*=============================================================================
	UnStaticMeshBuild.cpp: Static mesh building.
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

//
//	PointsEqual
//

inline UBOOL PointsEqual(FVector& V1,FVector& V2)
{
	if(Abs(V1.X - V2.X) > THRESH_POINTS_ARE_SAME * 4.0f)
		return 0;

	if(Abs(V1.Y - V2.Y) > THRESH_POINTS_ARE_SAME * 4.0f)
		return 0;

	if(Abs(V1.Z - V2.Z) > THRESH_POINTS_ARE_SAME * 4.0f)
		return 0;

	return 1;
}

//
//	NormalsEqual
//
 
inline UBOOL NormalsEqual(FVector& V1,FVector& V2)
{
	if(Abs(V1.X - V2.X) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 0;

	if(Abs(V1.Y - V2.Y) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 0;

	if(Abs(V1.Z - V2.Z) > THRESH_NORMALS_ARE_SAME * 4.0f)
		return 0;

	return 1;
}

//
//	FindVertexIndex
//

INT FindVertexIndex(UStaticMesh* StaticMesh,FVector Position,FVector Normal,FColor Color,FStaticMeshUV* UVs,INT NumUVs,UBOOL OnlyCollision)
{
	guard(FindVertexIndex);

	FLOAT	TolerableErrorU = 1.0f / 1024.0f,
			TolerableErrorV = 1.0f / 1024.0f;

	// Find any identical vertices already in the vertex buffer.

	INT	VertexBufferIndex = INDEX_NONE;

	for(INT VertexIndex = 0;VertexIndex < StaticMesh->VertexStream.Vertices.Num();VertexIndex++)
	{
		// Compare vertex position and normal.

		FStaticMeshVertex*	CompareVertex = &StaticMesh->VertexStream.Vertices(VertexIndex);

		if(!PointsEqual(CompareVertex->Position,Position))
			continue;

		// We only care about position matching for rebuilding collision.
		if( !OnlyCollision )
		{
			if(!NormalsEqual(CompareVertex->Normal,Normal))
				continue;

			// Compare vertex color.

			if(StaticMesh->ColorStream.Colors(VertexIndex) != Color)
				continue;

			// Compare vertex UVs.

			UBOOL	UVsMatch = 1;

			for(INT UVIndex = 0;UVIndex < NumUVs;UVIndex++)
			{
				FStaticMeshUV*	CompareUV = &StaticMesh->UVStreams(UVIndex).UVs(VertexIndex);

				if(Abs(CompareUV->U - UVs[UVIndex].U) > TolerableErrorU)
				{
					UVsMatch = 0;
					break;
				}

				if(Abs(CompareUV->V - UVs[UVIndex].V) > TolerableErrorV)
				{
					UVsMatch = 0;
					break;
				}
			}

			if(!UVsMatch)
				continue;
		}

		// The vertex matches!

		VertexBufferIndex = VertexIndex;
		break;
	}

	// If there is no identical vertex already in the vertex buffer...

	if(VertexBufferIndex == INDEX_NONE)
	{
		check( !OnlyCollision );

		// Add a new vertex to the vertex streams.

		FStaticMeshVertex	Vertex;

		Vertex.Position = Position;
		Vertex.Normal = Normal;

		VertexBufferIndex = StaticMesh->VertexStream.Vertices.AddItem(Vertex);

        // gam ---
		verify(StaticMesh->ColorStream.Colors.AddItem(Color) == VertexBufferIndex);
		verify(StaticMesh->AlphaStream.Colors.AddItem(FColor(255,255,255,Color.A)) == VertexBufferIndex);

		for(INT UVIndex = 0;UVIndex < NumUVs;UVIndex++)
			verify(StaticMesh->UVStreams(UVIndex).UVs.AddItem(UVs[UVIndex]) == VertexBufferIndex);

		for(INT UVIndex = NumUVs;UVIndex < StaticMesh->UVStreams.Num();UVIndex++)
			verify(StaticMesh->UVStreams(UVIndex).UVs.AddZeroed() == VertexBufferIndex);
		// --- gam
	}

	return VertexBufferIndex;

	unguard;
}

//
//	FStaticMeshEdge
//

struct FStaticMeshEdge
{
	INT	Vertices[2];
	INT	Triangles[2];
};

//
//	FindEdgeIndex
//

INT FindEdgeIndex(TArray<FStaticMeshEdge>& Edges,FStaticMeshEdge& Edge)
{
	guard(FindEdgeIndex);

	for(INT EdgeIndex = 0;EdgeIndex < Edges.Num();EdgeIndex++)
	{
		FStaticMeshEdge&	OtherEdge = Edges(EdgeIndex);

		if(OtherEdge.Vertices[0] == Edge.Vertices[1] && OtherEdge.Vertices[1] == Edge.Vertices[0])
		{
			OtherEdge.Triangles[1] = Edge.Triangles[0];

			return EdgeIndex;
		}
	}

	new(Edges) FStaticMeshEdge(Edge);

	return Edges.Num() - 1;

	unguard;
}

//
//	ClassifyTriangleVertices
//

ESplitType ClassifyTriangleVertices(FPlane Plane,FVector* Vertices)
{
	ESplitType	Classification = SP_Coplanar;

	for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
	{
		FLOAT	Dist = Plane.PlaneDot(Vertices[VertexIndex]);

		if(Dist < -0.0001f)
		{
			if(Classification == SP_Front)
				Classification = SP_Split;
			else if(Classification != SP_Split)
				Classification = SP_Back;
		}
		else if(Dist >= 0.0001f)
		{
			if(Classification == SP_Back)
				Classification = SP_Split;
			else if(Classification != SP_Split)
				Classification = SP_Front;
		}
	}

	return Classification;
}

//
//	UStaticMesh::Build
//

struct FanFace
{
	INT FaceIndex;
	INT MatchingIndex;
	UBOOL Filled;
};

void UStaticMesh::Build( UBOOL OnlyCollision, FVector* InNormals )
{
	guard(UStaticMesh::Build);

	if( !OnlyCollision )
		GWarn->BeginSlowTask(*FString::Printf(TEXT("(%s) Building"),GetPathName()),1);

	// Mark the parent package as dirty.

	UObject* Outer = GetOuter();
	while( Outer && Outer->GetOuter() )
		Outer = Outer->GetOuter();
	if( Outer && Cast<UPackage>(Outer) )
		Cast<UPackage>(Outer)->bDirty = 1;

	// Clear old data.
	if( !OnlyCollision )
	{
		Sections.Empty();

		VertexStream.Vertices.Empty();
		ColorStream.Colors.Empty();
		AlphaStream.Colors.Empty();
		UVStreams.Empty();

		IndexBuffer.Indices.Empty();
		WireframeIndexBuffer.Indices.Empty();
	}

	// Load the source data.

	if(!RawTriangles.Num())
		RawTriangles.Load();

	// Calculate triangle normals.

	TArray<FVector>	TriangleNormals(RawTriangles.Num());

	for(INT TriangleIndex = 0;TriangleIndex < RawTriangles.Num();TriangleIndex++)
	{
		FStaticMeshTriangle*	Triangle = &RawTriangles(TriangleIndex);

		TriangleNormals(TriangleIndex) = FPlane(
											Triangle->Vertices[2],
											Triangle->Vertices[1],
											Triangle->Vertices[0]
											);
	}


	TArray<FRawIndexBuffer> SectionIndices;

	if( !OnlyCollision )
	{
		// Initialize static mesh sections.	
		for(INT MaterialIndex = 0;MaterialIndex < Materials.Num();MaterialIndex++)
		{
			new(Sections) FStaticMeshSection();
			new(SectionIndices) FRawIndexBuffer();
		}

		// Create the necessary number of UV streams.
		for(INT TriangleIndex = 0;TriangleIndex < RawTriangles.Num();TriangleIndex++)
		{
			FStaticMeshTriangle*	Triangle = &RawTriangles(TriangleIndex);

			if( Triangle->NumUVs > 6 ) // sjs - clamp the upper uv limit (meshvert+color+6uvstreams==typical 8 streams available)
				Triangle->NumUVs = 6;

			while(UVStreams.Num() < Triangle->NumUVs)
			{
				FStaticMeshUVStream*	UVStream = new(UVStreams) FStaticMeshUVStream();
				UVStream->CoordinateIndex = UVStreams.Num() - 1;
			};
		}
	}

	TArray<FkDOPBuildCollisionTriangle> kDOPBuildTriangles;

	// Process each triangle.
    int numDegenerates = 0; // sjs
	for(INT TriangleIndex = 0;TriangleIndex < RawTriangles.Num();TriangleIndex++)
	{
		FStaticMeshTriangle*	Triangle = &RawTriangles(TriangleIndex);
		FStaticMeshSection*		Section = &Sections(Triangle->MaterialIndex);
		FStaticMeshMaterial*	Material = &Materials(Triangle->MaterialIndex);

        // sjs --- skip degenerates for collision and rendering, really screws up the BSP collision!
        // although the degenerates are tested below, the PointsEqual will merge really small tris
		// vogel: also detect case of all vertices in a line
        if( PointsEqual(Triangle->Vertices[0],Triangle->Vertices[1])
		 ||	PointsEqual(Triangle->Vertices[0],Triangle->Vertices[2])
		 ||	PointsEqual(Triangle->Vertices[1],Triangle->Vertices[2])
		 || TriangleNormals(TriangleIndex).IsZero()
		)
		{
            numDegenerates++;
			continue;
		}
        // --- sjs

		GWarn->StatusUpdatef(TriangleIndex,RawTriangles.Num(),TEXT("(%s) Indexing vertices..."),GetPathName());

		// Calculate smooth vertex normals.

		FVector	Normals[3];

		if( OnlyCollision )
		{
			// We don't care about the normals if we're only rebuilding collision.
			for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
				Normals[VertexIndex] = FVector(0,0,0);
		}
		else
		if( InNormals )
		{
			// Load vertex normals from the list		
			Normals[0] = InNormals[Triangle->SmoothingMask];
			Normals[1] = InNormals[reinterpret_cast<PTRINT>(Triangle->LegacyMaterial)];
			Normals[2] = InNormals[Triangle->LegacyPolyFlags];

			// Restore the values to the fields we stole; doesn't really matter, anyway, since
			// these fields are not used, but just for the sake of completeness...
			Triangle->SmoothingMask   = 1;
			Triangle->LegacyMaterial  = 0;
			Triangle->LegacyPolyFlags = 1;
		}
		else
		{
			for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
				Normals[VertexIndex] = FVector(0,0,0);

			// jij ---
			INT smoothingindex = 0;
			INT smoothingmask = Triangle->SmoothingMask;
			while(smoothingmask > 0)
			{
				smoothingindex++;
				smoothingmask >>= 1;
			}
			// --- jij
							
			//
			//  EDN:  Fully 3DS-Max compatible smoothing groups interpretation, for the vertex normal accumulation.
			//
			// Create a face fan lists for each vertex separately, in this face.						
			for( INT VertexIndex = 0; VertexIndex < 3; VertexIndex ++)
			{
				// Establish connectivity - determine which faces touch THIS vertex, and are potential contributors to its normal.
				TArray<FanFace> ContributingFaces;
				for( INT OtherTriangleIndex = 0; OtherTriangleIndex < RawTriangles.Num(); OtherTriangleIndex++)
				{
					FStaticMeshTriangle*	OtherTriangle = &RawTriangles(OtherTriangleIndex);
					for(INT OtherVertexIndex = 0;OtherVertexIndex < 3;OtherVertexIndex++)
					{
						if(PointsEqual( Triangle->Vertices[VertexIndex], OtherTriangle->Vertices[OtherVertexIndex]))
						{
							FanFace NewFace;
							NewFace.FaceIndex = OtherTriangleIndex;
							NewFace.Filled = ( OtherTriangleIndex == TriangleIndex ); // Starter face for smoothing flood fill.
							NewFace.MatchingIndex = OtherVertexIndex;
							ContributingFaces.AddItem ( NewFace );
							break; 
						}				
					}
				}			

				// "Flood fill" the fan around the vertex across friendly smoothing masks only.  
				// The vertex' own face is only face starting with .Filled == true.
				INT NewConnections = 1;
				while( NewConnections )
				{
					NewConnections = 0;
					for( INT OtherFaceIdx=0; OtherFaceIdx < ContributingFaces.Num(); OtherFaceIdx++ )
					{															
						if( ContributingFaces( OtherFaceIdx ).Filled )  // A face to continue our flood fill with ?
						{				
							FStaticMeshTriangle*	OtherTriangle = &RawTriangles( ContributingFaces(OtherFaceIdx).FaceIndex  );
							for( INT MoreFaceIdx = 0; MoreFaceIdx < ContributingFaces.Num(); MoreFaceIdx ++ )
							{								
								if( ! ContributingFaces( MoreFaceIdx ).Filled ) // An unconnected one ?
								{
									FStaticMeshTriangle* FreshTriangle = &RawTriangles( ContributingFaces( MoreFaceIdx).FaceIndex );								
									//  A least _two_ vertices in common with another qualifying face AND smooth across that ? 
									if(  FreshTriangle->SmoothingMask &  OtherTriangle->SmoothingMask )
									{																												
										INT CommonOthers = 0;
										if(  MoreFaceIdx == OtherFaceIdx ) // Same face..
										{
											CommonOthers = 3; 
										}
										else  // Always at least one in common.
										for( INT OtherVertexIndex = 0; OtherVertexIndex < 3 ; OtherVertexIndex ++ )
										{											
											for( INT OrigVertexIndex = 0; OrigVertexIndex < 3; OrigVertexIndex ++ )
											{
												if( PointsEqual ( FreshTriangle->Vertices[OrigVertexIndex],  OtherTriangle->Vertices[OtherVertexIndex]  )	)
												{
													CommonOthers++;
												}
											}										
										}
										if( CommonOthers > 1)
										{
											ContributingFaces( MoreFaceIdx).Filled = true;
											NewConnections++;
										}
									}
								}
							}
						}
					}
				} // While

				for( INT FaceIdx = 0; FaceIdx < ContributingFaces.Num(); FaceIdx++ )
				{				
					if( ContributingFaces( FaceIdx ).Filled )
					{				
						INT OtherTriangleIndex = ContributingFaces( FaceIdx).FaceIndex;
						// MaxSmoothingAngles check  ( only relevant for .LWO imported meshes.)
						// jij ---
						// The face does not contribute if it is outside the mesh's smoothing threshold for Triangle's smoothing group.
						if ((TriangleIndex != OtherTriangleIndex) && (((smoothingindex-1) >= 0) && ((smoothingindex-1) < MaxSmoothingAngles.Num())) &&
							(TriangleNormals(TriangleIndex) | TriangleNormals(OtherTriangleIndex)) < MaxSmoothingAngles((smoothingindex-1)))
							continue;
						// --- jij

						// Let this face contribute to the vertex' normal.
						Normals[VertexIndex] += TriangleNormals( ContributingFaces( FaceIdx).FaceIndex );
					}				
				}

			}// 3 vertices of face [TriangleIndex].
		}

		// Normalize the accumulated vertex normals.
		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			Normals[VertexIndex].Normalize();

		// Index the triangle's vertices.
		INT	VertexIndices[3];

		for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
			VertexIndices[VertexIndex] = FindVertexIndex(
											this,
											Triangle->Vertices[VertexIndex],
											Normals[VertexIndex],
											Triangle->Colors[VertexIndex],
											Triangle->UVs[VertexIndex],
											Triangle->NumUVs,
											OnlyCollision
											);

		// Reject degenerate triangles.

       	if(VertexIndices[0] == VertexIndices[1] || VertexIndices[1] == VertexIndices[2] || VertexIndices[0] == VertexIndices[2] )
			continue;

		// Put the indices in the section index buffer.
		if( !OnlyCollision )
		{		
			for(INT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
				SectionIndices(Triangle->MaterialIndex).Indices.AddItem(VertexIndices[VertexIndex]);

			Section->NumTriangles++;
			Section->NumPrimitives++;
		}

		if(Material->EnableCollision && TriangleNormals(TriangleIndex).Size() > 0.0f)
		{
			// Build a new kDOP collision triangle
			new (kDOPBuildTriangles) FkDOPBuildCollisionTriangle(VertexIndices[0],VertexIndices[1],VertexIndices[2],Triangle);
		}
	}

	if( !OnlyCollision )
	{
		if( numDegenerates )
			debugf(TEXT("%s StaticMesh had %i degenerates"), GetName(), numDegenerates ); // sjs test

		VertexStream.Revision++;
		ColorStream.Revision++;
		AlphaStream.Revision++;

		for(INT UVIndex = 0;UVIndex < UVStreams.Num();UVIndex++)
			UVStreams(UVIndex).Revision++;

		// Build a cache optimized triangle list for each section and copy it into the shared index buffer.
		for(INT SectionIndex = 0;SectionIndex < SectionIndices.Num();SectionIndex++)
		{
			FStaticMeshSection*	Section = &Sections(SectionIndex);

			GWarn->StatusUpdatef(SectionIndex,Sections.Num(),TEXT("(%s) Optimizing render data..."),GetPathName());

			if(SectionIndices(SectionIndex).Indices.Num())
			{
				if(InternalVersion == -1) // HACK: PS2 strip builder
				{
					Sections(SectionIndex).IsStrip = 1;
					Sections(SectionIndex).NumPrimitives = SectionIndices(SectionIndex).Stripify();
				}
				else
				{
					SectionIndices(SectionIndex).CacheOptimize();
					Sections(SectionIndex).NumPrimitives = SectionIndices(SectionIndex).Indices.Num() / 3;
				}

				Section->FirstIndex = IndexBuffer.Indices.Num();

				_WORD*	DestPtr = &IndexBuffer.Indices(IndexBuffer.Indices.Add(SectionIndices(SectionIndex).Indices.Num()));
				_WORD*	SrcPtr = &SectionIndices(SectionIndex).Indices(0);

				Section->MinVertexIndex = *SrcPtr;
				Section->MaxVertexIndex = *SrcPtr;

				for(INT Index = 0;Index < SectionIndices(SectionIndex).Indices.Num();Index++)
				{
					Section->MinVertexIndex = Min(*SrcPtr,Section->MinVertexIndex);
					Section->MaxVertexIndex = Max(*SrcPtr,Section->MaxVertexIndex);

					*DestPtr++ = *SrcPtr++;
				}
			}
		}

		IndexBuffer.Revision++;

		// Build a list of wireframe edges in the static mesh.

		TArray<FStaticMeshEdge>	Edges;

		for(INT TriangleIndex = 0;TriangleIndex < IndexBuffer.Indices.Num() / 3;TriangleIndex++)
		{
			_WORD*	TriangleIndices = &IndexBuffer.Indices(TriangleIndex * 3);

			for(INT EdgeIndex = 0;EdgeIndex < 3;EdgeIndex++)
			{
				FStaticMeshEdge	Edge;

				Edge.Vertices[0] = TriangleIndices[EdgeIndex];
				Edge.Vertices[1] = TriangleIndices[(EdgeIndex + 1) % 3];
				Edge.Triangles[0] = TriangleIndex;
				Edge.Triangles[1] = -1;

				FindEdgeIndex(Edges,Edge);
			}
		}

		for(INT EdgeIndex = 0;EdgeIndex < Edges.Num();EdgeIndex++)
		{
			FStaticMeshEdge&	Edge = Edges(EdgeIndex);

			WireframeIndexBuffer.Indices.AddItem(Edge.Vertices[0]);
			WireframeIndexBuffer.Indices.AddItem(Edge.Vertices[1]);
		}

		WireframeIndexBuffer.Revision++;

		// Calculate the bounding box.
		BoundingBox = FBox(0);

		for(INT VertexIndex = 0;VertexIndex < VertexStream.Vertices.Num();VertexIndex++)
			BoundingBox += VertexStream.Vertices(VertexIndex).Position;

		BoundingSphere = FSphere(&BoundingBox.Min,2);
	}

	kDOPTree.Build(VertexStream.Vertices,kDOPBuildTriangles);

	if( !GIsEditor )
		RawTriangles.Unload();

	if( !OnlyCollision )
		GWarn->EndSlowTask();

	unguard;
}

