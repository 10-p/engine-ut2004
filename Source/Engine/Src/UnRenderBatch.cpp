/*=============================================================================
	UnRenderBatch.cpp: Render batching code.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"
#include "UnRenderPrivate.h"

//
//	FCompositeVertexStream
//

class FCompositeVertexStream : public FVertexStream
{
public:

	FProjectorRenderBatch*	Batch;
	INT						Size,
							Stride;

	// Constructor.

	FCompositeVertexStream(FProjectorRenderBatch* InBatch,INT InSize)
	{
		Batch = InBatch;
		Size = InSize;

		if(Batch->BaseMaterial)
			Stride = sizeof(FProjectorBatchVertex) + sizeof(FLOAT) * 2;
		else
			Stride = sizeof(FProjectorBatchVertex);
	}

	// FRenderResource interface.

	virtual QWORD GetCacheId() { return 0; }
	virtual INT GetRevision() { return 1; }

	// FVertexStream interface.

	virtual INT GetSize() { return Size; }
	virtual INT GetStride() { return Stride; }
	virtual INT GetComponents(FVertexComponent* Components)
	{
		INT	NumComponents = 0;

		Components[NumComponents].Type = CT_Float3;
		Components[NumComponents].Function = FVF_Position;
		NumComponents++;

		Components[NumComponents].Type = CT_Color;
		Components[NumComponents].Function = FVF_Diffuse;
		NumComponents++;

		Components[NumComponents].Type = CT_Float3;
		Components[NumComponents].Function = FVF_TexCoord0;
		NumComponents++;

		if(Batch->BaseMaterial)
		{
			Components[NumComponents].Type = CT_Float2;
			Components[NumComponents].Function = FVF_TexCoord1;
			NumComponents++;
		}

		return NumComponents;
	}
	virtual void GetRawStreamData(void** Dest,INT FirstVertex) 
	{
		*Dest = NULL;
	}
	virtual void GetStreamData(void* Dest)
	{
		guard(FCompositeVertexStream::GetStreamData);

		FRenderBatchChunk<FProjectorRenderPrimitive>*	CurrentChunk = Batch->FirstChunk;
		INT												ChunkPrimitiveIndex = 0;
		BYTE*											DestVertex = (BYTE*)Dest;

		for(INT PrimitiveIndex = 0;PrimitiveIndex < Batch->NumPrimitives;PrimitiveIndex++)
		{
			FProjectorRenderPrimitive*	Primitive = CurrentChunk->Primitives[ChunkPrimitiveIndex];

			Primitive->GetVertices(DestVertex);
			DestVertex += Stride * Primitive->NumVertices;

			ChunkPrimitiveIndex++;

			if(ChunkPrimitiveIndex >= BATCH_CHUNK_SIZE)
			{
				CurrentChunk = CurrentChunk->NextChunk;
				ChunkPrimitiveIndex = 0;
			}
		}

		unguard;
	}
};

//
//	FCompositeIndexBuffer
//

class FCompositeIndexBuffer : public FIndexBuffer
{
public:

	FProjectorRenderBatch*	Batch;
	INT						Size;

	// Constructor.

	FCompositeIndexBuffer(FProjectorRenderBatch* InBatch,INT InSize)
	{
		Batch = InBatch;
		Size = InSize;
	}

	// FRenderResource interface.

	virtual QWORD GetCacheId() { return 0; }
	virtual INT GetRevision() { return 1; }

	// FIndexBuffer interface.

	virtual INT GetSize() { return Size; }
	virtual INT GetIndexSize() { return sizeof(_WORD); }
	virtual void GetContents(void* Data)
	{
		guard(FCompositeIndexBuffer::GetContents);

		FRenderBatchChunk<FProjectorRenderPrimitive>*	CurrentChunk = Batch->FirstChunk;
		INT												ChunkPrimitiveIndex = 0;
		_WORD*											DestIndex = (_WORD*) Data;
		_WORD											BaseVertexIndex = 0;

		for(INT PrimitiveIndex = 0;PrimitiveIndex < Batch->NumPrimitives;PrimitiveIndex++)
		{
			FProjectorRenderPrimitive*	Primitive = CurrentChunk->Primitives[ChunkPrimitiveIndex];

			Primitive->GetIndices(DestIndex,BaseVertexIndex);
			DestIndex += Primitive->NumIndices;
			BaseVertexIndex += Primitive->NumVertices;

			ChunkPrimitiveIndex++;

			if(ChunkPrimitiveIndex >= BATCH_CHUNK_SIZE)
			{
				CurrentChunk = CurrentChunk->NextChunk;
				ChunkPrimitiveIndex = 0;
			}
		}

		unguard;
	}
};

//
//	FProjectorRenderBatch::Render
//

void FProjectorRenderBatch::Render(FSceneNode* SceneNode,FRenderInterface* RI)
{
	guard(FProjectorRenderBatch::Render);

	DECLARE_STATIC_UOBJECT(
		UProjectorMaterial,
		ProjectorMaterial,
		{
			ProjectorMaterial->bStaticProjector = 1;
		}
		);

	ProjectorMaterial->Projected = ProjectedMaterial;
	ProjectorMaterial->BaseMaterial = BaseMaterial;
	ProjectorMaterial->BaseMaterialBlending = BaseMaterialBlending;
	ProjectorMaterial->FrameBufferBlending = FramebufferBlending;
	ProjectorMaterial->bStaticProjector = 1;
	ProjectorMaterial->bTwoSided = TwoSided;

	RI->PushState();

	RI->EnableLighting(0,1,0,NULL,0);
	RI->SetMaterial(ProjectorMaterial);

	RI->SetZBias(1);

	FCompositeVertexStream	VertexStream(this,VertexBufferSize);
	FCompositeIndexBuffer	IndexBuffer(this,IndexBufferSize);

	INT	BaseVertexIndex = RI->SetDynamicStream(VS_FixedFunction,&VertexStream),
		BaseIndex = RI->SetDynamicIndexBuffer(&IndexBuffer,BaseVertexIndex);

	RI->DrawPrimitive(
		PT_TriangleList,
		BaseIndex,
		IndexBufferSize / (3 * sizeof(_WORD)),
		0,
		VertexBufferSize / VertexStride - 1
		);

	STAT(GStats.DWORDStats(GEngineStats.STATS_Projector_Projectors) += NumPrimitives);
	STAT(GStats.DWORDStats(GEngineStats.STATS_Projector_Triangles) += IndexBufferSize / (3 * sizeof(_WORD)));

	RI->PopState();

	unguard;
}

//
//	FStaticMeshBatchVertexStream::FStaticMeshBatchVertexStream
//

FStaticMeshBatchVertexStream::FStaticMeshBatchVertexStream(UMaterial* Material)
{
	CacheId = MakeCacheID(CID_RenderVertices);
	Size = 0;

	// Count the number of required UVs.

	BYTE	RequiredUVStreams = Material->RequiredUVStreams();

	BYTE UVStreamMask = 0xff;
	for(INT UVIndex = 0;UVIndex < 8;UVIndex++,UVStreamMask <<= 1)
		if(!(RequiredUVStreams & UVStreamMask))
		{
			NumUVs = UVIndex;
			break;
		}

	// Calculate the stride.

	Normal = 1;//Material->RequiresNormal();
	Stride = (Normal ? sizeof(FStaticMeshBatchNormalVertex) : sizeof(FStaticMeshBatchVertex)) + sizeof(FLOAT) * 2 * NumUVs;
}

//
//	FStaticMeshBatchVertexStream::GetComponents
//

INT FStaticMeshBatchVertexStream::GetComponents(FVertexComponent* Components)
{
	INT	NumComponents = 0;

	Components[NumComponents].Type = CT_Float3;
	Components[NumComponents].Function = FVF_Position;
	NumComponents++;

	if(Normal)
	{
		Components[NumComponents].Type = CT_Float3;
		Components[NumComponents].Function = FVF_Normal;
		NumComponents++;
	}

	Components[NumComponents].Type = CT_Color;
	Components[NumComponents].Function = FVF_Diffuse;
	NumComponents++;

	for(INT UVIndex = 0;UVIndex < NumUVs;UVIndex++)
	{
		Components[NumComponents].Type = CT_Float2;
		Components[NumComponents].Function = (EFixedVertexFunction)(FVF_TexCoord0 + UVIndex);
		NumComponents++;
	}

	return NumComponents;
}

//
//	FStaticMeshBatchVertexStream::GetStreamData
//

void FStaticMeshBatchVertexStream::GetStreamData(void* Dest)
{
	guard(FStaticMeshBatchVertexStream::GetStreamData);

	FVector*	DestPosition = (FVector*)Dest;
	FVector*	DestNormal = Normal ? (DestPosition + 1) : DestPosition;
	FColor*		DestLighting = (FColor*)(DestNormal + 1);
	FLOAT*		DestUVs = (FLOAT*)(DestLighting + 1);

	for(INT ElementIndex = 0;ElementIndex < Batch->Elements.Num();ElementIndex++)
	{
		AActor*					Actor = Batch->Elements(ElementIndex).Actor;
		FDynamicActor*			DynamicActor = Actor->GetActorRenderData();
		UStaticMeshInstance*	StaticMeshInstance = NULL;
		UStaticMesh*			StaticMesh = Actor->StaticMesh;
		FStaticMeshSection&		Section = StaticMesh->Sections(Batch->Elements(ElementIndex).SectionIndex);
		UEngine*				Engine = Actor->XLevel->Engine;
		FColor					CustomVertexColor;
		UBOOL					UseCustomVertexColor = 0;

		UGameEngine*			GameEngine = Actor->Level->NetMode != NM_Standalone ? NULL : Cast<UGameEngine>(Engine);
		if( GameEngine && GameEngine->ColorNoCollisionMeshes && !Actor->bCollideActors /*|| (Actor->bCollideActors && !Actor->bBlockZeroExtentTraces)*/ )
		{
			UseCustomVertexColor	= 1;
			CustomVertexColor		= FColor(0,255,128);
		}
		else
		if( GameEngine && GameEngine->ColorSlowCollisionMeshes && Actor->bCollideActors && !Actor->bUseCylinderCollision && (Actor->bBlockNonZeroExtentTraces || Actor->bBlockActors) && StaticMesh && !StaticMesh->CollisionModel )
		{
			UseCustomVertexColor	= 1;
			CustomVertexColor		= FColor(255,0,255);
		}
		else if( GameEngine && GameEngine->ColorHighDetailMeshes && Actor->bHighDetail )
		{
			UseCustomVertexColor	= 1;
			CustomVertexColor		= FColor(255,128,0);
		}
		else if( GameEngine && GameEngine->ColorHighDetailMeshes && Actor->bSuperHighDetail )
		{
			UseCustomVertexColor	= 1;
			CustomVertexColor		= FColor(255,0,0);
		}
		else if( Actor->bUnlit )
		{
			UseCustomVertexColor	= 1;
			CustomVertexColor		= FColor(127,127,127);
		}

		if(Actor->StaticMeshInstance)
		{
			// Verify the actor's instance lighting matches the static mesh.

			UBOOL	Matches = 1;

			if(Actor->StaticMeshInstance->ColorStream.Colors.Num() != StaticMesh->VertexStream.Vertices.Num())
				Matches = 0;

			if(Matches)
			{
				for(INT LightIndex = 0;LightIndex < Actor->StaticMeshInstance->Lights.Num();LightIndex++)
					if(Actor->StaticMeshInstance->Lights(LightIndex).VisibilityBits.Num() != (StaticMesh->VertexStream.Vertices.Num() + 7) / 8)
					{
						Matches = 0;
						break;
					}
			}

			if(Matches)
				StaticMeshInstance = Actor->StaticMeshInstance;
		}

		if(StaticMeshInstance)
			CalculateStaticMeshLighting(StaticMesh,StaticMeshInstance,DynamicActor);

		INT	NumExistingUVs = Min(NumUVs,StaticMesh->UVStreams.Num());

		for(INT VertexIndex = Section.MinVertexIndex;VertexIndex <= Section.MaxVertexIndex;VertexIndex++)
		{
			FStaticMeshVertex*		SrcVertex = &StaticMesh->VertexStream.Vertices(VertexIndex);

			*DestPosition = DynamicActor->LocalToWorld.TransformFVector(SrcVertex->Position);

			if(Normal)
				*DestNormal = DynamicActor->LocalToWorld.TransformNormal(SrcVertex->Normal);

			if( UseCustomVertexColor )
				*DestLighting = CustomVertexColor;
			else if(StaticMeshInstance)
			{
				*DestLighting = StaticMeshInstance->ColorStream.Colors(VertexIndex);
				*DestLighting += DynamicActor->AmbientColor;
			}
			else
				*DestLighting = DynamicActor->AmbientColor;

			for(INT UVIndex = 0;UVIndex < NumExistingUVs;UVIndex++)
			{
				DestUVs[UVIndex * 2 + 0] = StaticMesh->UVStreams(UVIndex).UVs(VertexIndex).U;
				DestUVs[UVIndex * 2 + 1] = StaticMesh->UVStreams(UVIndex).UVs(VertexIndex).V;
			}

			for(INT UVIndex = NumExistingUVs;UVIndex < NumUVs;UVIndex++)
			{
				DestUVs[UVIndex * 2 + 0] = 0.0f;
				DestUVs[UVIndex * 2 + 1] = 0.0f;
			}

			DestPosition = (FVector*)((BYTE*) DestPosition + Stride);
			DestNormal = (FVector*)((BYTE*) DestNormal + Stride);
			DestLighting = (FColor*)((BYTE*) DestLighting + Stride);
			DestUVs = (FLOAT*)((BYTE*) DestUVs + Stride);
		}
	}

	unguard;
}

//
//	FStaticMeshBatchIndexBuffer::FStaticMeshBatchIndexBuffer
//

FStaticMeshBatchIndexBuffer::FStaticMeshBatchIndexBuffer()
{
	CacheId = MakeCacheID(CID_RenderVertices);
	Size = 0;
}

//
//	FStaticMeshBatchIndexBuffer::GetContents
//

void FStaticMeshBatchIndexBuffer::GetContents(void* Dest)
{
	guard(FStaticMeshBatchIndexBuffer::GetContents);

	_WORD*	DestIndex = (_WORD*) Dest;
	INT		BaseIndex = 0;

	for(INT ElementIndex = 0;ElementIndex < Batch->Elements.Num();ElementIndex++)
	{
		AActor*				Actor = Batch->Elements(ElementIndex).Actor;
		FDynamicActor*		DynamicActor = Actor->GetActorRenderData();
		UStaticMesh*		StaticMesh = Actor->StaticMesh;
		FStaticMeshSection*	Section = &StaticMesh->Sections(Batch->Elements(ElementIndex).SectionIndex);
		_WORD*				SrcIndex = &StaticMesh->IndexBuffer.Indices(Section->FirstIndex);

        if(DynamicActor->Determinant < 0.0f)
		{
			// The static mesh has a negative scale, reverse the vertex order.

			for(INT PrimitiveIndex = 0;PrimitiveIndex < Section->NumPrimitives;PrimitiveIndex++)
			{
				_WORD	TriangleIndices[3];
				
				TriangleIndices[0] = *SrcIndex++;
				TriangleIndices[1] = *SrcIndex++;
				TriangleIndices[2] = *SrcIndex++;

				*DestIndex++ = TriangleIndices[2] - Section->MinVertexIndex + BaseIndex;
				*DestIndex++ = TriangleIndices[1] - Section->MinVertexIndex + BaseIndex;
				*DestIndex++ = TriangleIndices[0] - Section->MinVertexIndex + BaseIndex;
			}
		}
		else
		{
			for(INT PrimitiveIndex = 0;PrimitiveIndex < Section->NumPrimitives;PrimitiveIndex++)
			{
				*DestIndex++ = *SrcIndex++ - Section->MinVertexIndex + BaseIndex;
				*DestIndex++ = *SrcIndex++ - Section->MinVertexIndex + BaseIndex;
				*DestIndex++ = *SrcIndex++ - Section->MinVertexIndex + BaseIndex;
			}
		}

		BaseIndex += Section->MaxVertexIndex - Section->MinVertexIndex + 1;
	}

	unguard;
}

//
//	FStaticMeshBatch::FStaticMeshBatch
//

FStaticMeshBatch::FStaticMeshBatch(UMaterial* InMaterial,UBOOL DisableSorting) : Vertices(InMaterial)
{
	guard(FStaticMeshBatch::FStaticMeshBatch);

	Material = InMaterial;
	Sorted = Material->RequiresSorting() && !DisableSorting;

	unguard;
}

//
//	FStaticMeshBatch::AddElement
//

INT FStaticMeshBatch::AddElement(AActor* Actor,INT SectionIndex)
{
	guard(FStaticMeshBatch::AddElement);

	FStaticMeshSection&	Section = Actor->StaticMesh->Sections(SectionIndex);
	FBatchElement		NewElement;

	check(!Section.IsStrip);

	if(Section.NumPrimitives == 0)
		return INDEX_NONE;

	NewElement.Actor = Actor;
	NewElement.SectionIndex = SectionIndex;
	NewElement.FirstIndex = Indices.Size / sizeof(_WORD);
	NewElement.NumPrimitives = Section.NumPrimitives;
	NewElement.MinVertexIndex = Vertices.Size / Vertices.Stride;
	NewElement.MaxVertexIndex = NewElement.MinVertexIndex + (Section.MaxVertexIndex - Section.MinVertexIndex);

	INT	Index = Elements.AddItem(NewElement);
	Indices.Size += Section.NumPrimitives * 3 * sizeof(_WORD);
	Vertices.Size += (Section.MaxVertexIndex - Section.MinVertexIndex + 1) * Vertices.Stride;

	return Index;

	unguard;
}

//
// FSortedStaticMeshBatchIndexBuffer
//

struct FTriangleSortInfo
{
	INT 	SortKey;
	_WORD	Indices[3];
};

static inline int Compare( const FTriangleSortInfo& A, const FTriangleSortInfo& B )
{
    return B.SortKey - A.SortKey;
}

class FSortedStaticMeshBatchIndexBuffer : public FIndexBuffer
{
public:

	FStaticMeshBatch*	Batch;
	INT*				VisibleElements;
	INT					NumVisibleElements;
	FVector				ViewOrigin;

	QWORD	CacheId;
	INT		Size;

	// Constructor.

	FSortedStaticMeshBatchIndexBuffer(FStaticMeshBatch* InBatch,INT* InVisibleElements,INT InNumVisibleElements,FVector InViewOrigin)
	{
		Batch = InBatch;
		VisibleElements = InVisibleElements;
		NumVisibleElements = InNumVisibleElements;
		ViewOrigin = InViewOrigin;

		// Calculate the size of the index buffer.

		Size = 0;

		for(INT ElementIndex = 0;ElementIndex < NumVisibleElements;ElementIndex++)
		{
			FStaticMeshBatch::FBatchElement&	Element = Batch->Elements(VisibleElements[ElementIndex]);

			Size += Element.NumPrimitives * 3 * sizeof(_WORD);
		}
	}

	// FRenderResource interface.

	virtual QWORD GetCacheId() { return CacheId; }
	virtual INT GetRevision() { return 1; }

	// FIndexBuffer interface.

	virtual INT GetSize() { return Size; }
	virtual INT GetIndexSize() { return sizeof(_WORD); }
	virtual void GetContents(void* Dest)
	{
		guard(FSortedStaticMeshBatchIndexBuffer::GetContents);
		clock(GStats.DWORDStats(GEngineStats.STATS_StaticMesh_BatchedSortCycles));

		_WORD* DestIndex = (_WORD*)Dest;

		for(INT ElementIndex = 0;ElementIndex < NumVisibleElements;ElementIndex++)
		{
			FStaticMeshBatch::FBatchElement&	Element = Batch->Elements(VisibleElements[ElementIndex]);
			FDynamicActor*						DynamicActor = Element.Actor->GetActorRenderData();
			FStaticMeshSection*					Section = &Element.Actor->StaticMesh->Sections(Element.SectionIndex);
			_WORD								Offset = -Section->MinVertexIndex + Element.MinVertexIndex;

			if((DynamicActor->BoundingSphere - ViewOrigin).SizeSquared() < Square(8192.0f))
			{
				FMemMark			MemMark(GSceneMem);
				FVector				LocalViewOrigin = DynamicActor->WorldToLocal.TransformFVector(ViewOrigin);
				_WORD*				Indices = &Element.Actor->StaticMesh->IndexBuffer.Indices(Section->FirstIndex);
				FStaticMeshVertex*	Vertices = &Element.Actor->StaticMesh->VertexStream.Vertices(0);
				FTriangleSortInfo*	SortedTriangles = New<FTriangleSortInfo>(GSceneMem,Section->NumTriangles);

				for(INT TriangleIndex = 0;TriangleIndex < Section->NumTriangles;TriangleIndex++)
				{
					SortedTriangles[TriangleIndex].Indices[0] = Indices[TriangleIndex * 3 + 0] + Offset;
					SortedTriangles[TriangleIndex].Indices[1] = Indices[TriangleIndex * 3 + 1] + Offset;
					SortedTriangles[TriangleIndex].Indices[2] = Indices[TriangleIndex * 3 + 2] + Offset;

					FLOAT Distance =( 
										Vertices[Indices[TriangleIndex * 3 + 0]].Position + 
										Vertices[Indices[TriangleIndex * 3 + 1]].Position + 
										Vertices[Indices[TriangleIndex * 3 + 2]].Position -
										LocalViewOrigin * 3
									).SizeSquared();

					SortedTriangles[TriangleIndex].SortKey = appTrunc( Distance );
				}

				Sort( SortedTriangles, Section->NumTriangles );

				if(DynamicActor->Determinant < 0.0f)
				{
					for(INT TriangleIndex = 0;TriangleIndex < Section->NumTriangles;TriangleIndex++)
					{
						*DestIndex++ = SortedTriangles[TriangleIndex].Indices[2];
						*DestIndex++ = SortedTriangles[TriangleIndex].Indices[1];
						*DestIndex++ = SortedTriangles[TriangleIndex].Indices[0];
					}
				}
				else
				{
					for(INT TriangleIndex = 0;TriangleIndex < Section->NumTriangles;TriangleIndex++)
					{
						*DestIndex++ = SortedTriangles[TriangleIndex].Indices[0];
						*DestIndex++ = SortedTriangles[TriangleIndex].Indices[1];
						*DestIndex++ = SortedTriangles[TriangleIndex].Indices[2];
					}
				}

				MemMark.Pop();
			}
			else
			{
				_WORD*	SrcIndex = &Element.Actor->StaticMesh->IndexBuffer.Indices(Section->FirstIndex);

				if(DynamicActor->Determinant < 0.0f)
				{
					// The static mesh has a negative scale, reverse the vertex order.

					for(INT PrimitiveIndex = 0;PrimitiveIndex < Section->NumPrimitives;PrimitiveIndex++)
					{
						_WORD	TriangleIndices[3];
						
						TriangleIndices[0] = *SrcIndex++;
						TriangleIndices[1] = *SrcIndex++;
						TriangleIndices[2] = *SrcIndex++;

						*DestIndex++ = TriangleIndices[2] + Offset;
						*DestIndex++ = TriangleIndices[1] + Offset;
						*DestIndex++ = TriangleIndices[0] + Offset;
					}
				}
				else
				{
					for(INT PrimitiveIndex = 0;PrimitiveIndex < Section->NumPrimitives;PrimitiveIndex++)
					{
						*DestIndex++ = *SrcIndex++ + Offset;
						*DestIndex++ = *SrcIndex++ + Offset;
						*DestIndex++ = *SrcIndex++ + Offset;
					}
				}
			}
		}

		unclock(GStats.DWORDStats(GEngineStats.STATS_StaticMesh_BatchedSortCycles));
		unguard;
	}
};

//
//	FStaticMeshBatchList::Render
//

void FStaticMeshBatchList::Render(FSceneNode* SceneNode,FRenderInterface* RI)
{
	guard(FStaticMeshBatchList::Render);

	clock(GStats.DWORDStats(GEngineStats.STATS_StaticMesh_BatchedRenderCycles));
	clock(GStats.DWORDStats(GEngineStats.STATS_StaticMesh_RenderCycles));

	RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);
	RI->SetCullMode(CM_CW);

	// Render solid batches.
	UBOOL ReportDynamicUploads = !SceneNode->Viewport->Precaching && SceneNode->Viewport->GetOuterUClient()->ReportDynamicUploads;

	for(INT BatchIndex = 0;BatchIndex < NumBatches;BatchIndex++)
	{
		FStaticMeshBatch&	Batch = Level->StaticMeshBatches(Batches[BatchIndex]);

		if(!Batch.Sorted)
		{
			RI->EnableLighting(0,SceneNode->Viewport->IsLit(),1,NULL,SceneNode->Viewport->Actor->RendMap == REN_LightingOnly);
			RI->SetMaterial(Batch.Material);

			FVertexStream*	VertexStreams[1] = { &Batch.Vertices };

			INT UploadedSize = RI->SetVertexStreams(VS_FixedFunction,VertexStreams,1);
			if( UploadedSize && ReportDynamicUploads )
				debugf(TEXT("Uploading Unsorted Batch: Index=[%u] Size=[%u]"),BatchIndex,UploadedSize);
			RI->SetIndexBuffer(&Batch.Indices,0);

			for(INT ElementIndex = 0;ElementIndex < NumVisibleBatchElements[Batches[BatchIndex]];ElementIndex++)
			{
				FStaticMeshBatch::FBatchElement&	Element = Batch.Elements(VisibleBatchElements[Batches[BatchIndex]][ElementIndex]);

				RI->DrawPrimitive(PT_TriangleList,Element.FirstIndex,Element.NumPrimitives,Element.MinVertexIndex,Element.MaxVertexIndex);
				GStats.DWORDStats(GEngineStats.STATS_StaticMesh_BatchedUnsortedSections)++;
				GStats.DWORDStats(GEngineStats.STATS_StaticMesh_BatchedUnsortedTriangles) += Element.NumPrimitives;
				GStats.DWORDStats(GEngineStats.STATS_StaticMesh_Triangles) += Element.NumPrimitives;
			}

			GStats.DWORDStats(GEngineStats.STATS_StaticMesh_Batches)++;
		}
	}

	// Render sorted batches.

	for(INT BatchIndex = 0;BatchIndex < NumBatches;BatchIndex++)
	{
		FStaticMeshBatch&	Batch = Level->StaticMeshBatches(Batches[BatchIndex]);

		if(Batch.Sorted)
		{
			RI->EnableLighting(0,SceneNode->Viewport->IsLit(),1,NULL,SceneNode->Viewport->Actor->RendMap == REN_LightingOnly);
			RI->SetMaterial(Batch.Material);

			FVertexStream*						VertexStreams[1] = { &Batch.Vertices };
			FSortedStaticMeshBatchIndexBuffer	IndexBuffer(&Batch,VisibleBatchElements[Batches[BatchIndex]],NumVisibleBatchElements[Batches[BatchIndex]],SceneNode->ViewOrigin);

			INT	UploadedSize = RI->SetVertexStreams(VS_FixedFunction,VertexStreams,1);
			
			if(UploadedSize && ReportDynamicUploads)
				debugf(TEXT("Uploading Sorted Batch: Index=[%u] Size=[%u]"),BatchIndex,UploadedSize);

			INT	BaseIndex = RI->SetDynamicIndexBuffer(&IndexBuffer,0);

			for(INT ElementIndex = 0;ElementIndex < NumVisibleBatchElements[Batches[BatchIndex]];ElementIndex++)
			{
				FStaticMeshBatch::FBatchElement&	Element = Batch.Elements(VisibleBatchElements[Batches[BatchIndex]][ElementIndex]);

				RI->DrawPrimitive(PT_TriangleList,BaseIndex,Element.NumPrimitives,Element.MinVertexIndex,Element.MaxVertexIndex);
				BaseIndex += Element.NumPrimitives * 3;

				GStats.DWORDStats(GEngineStats.STATS_StaticMesh_BatchedSortedSections)++;
				GStats.DWORDStats(GEngineStats.STATS_StaticMesh_BatchedSortedTriangles) += Element.NumPrimitives;
				GStats.DWORDStats(GEngineStats.STATS_StaticMesh_Triangles) += Element.NumPrimitives;
			}

			GStats.DWORDStats(GEngineStats.STATS_StaticMesh_Batches)++;
		}
	}

	// Reset the visible elements.

	NumBatches = 0;
	appMemzero(VisibleBatchElements,Level->StaticMeshBatches.Num() * sizeof(INT*));

	unclock(GStats.DWORDStats(GEngineStats.STATS_StaticMesh_RenderCycles));
	unclock(GStats.DWORDStats(GEngineStats.STATS_StaticMesh_BatchedRenderCycles));

	unguard;
}

