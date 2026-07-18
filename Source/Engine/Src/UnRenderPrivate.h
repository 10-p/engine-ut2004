/*=============================================================================
	UnRenderPrivate.h: Private rendering definitions.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#define UPDATE_CHANGED_LIGHTMAPS	GIsEditor
#define UPDATE_DYNAMIC_LIGHTMAPS	0

//
//	FSceneMemStack
//

class FSceneMemStack : public FMemStack
{
public:

	FSceneMemStack();
	~FSceneMemStack();
};

extern FSceneMemStack	GSceneMem;	// A memory stack used for temporary rendering allocations.

//
//	FRenderBatchChunk
//

enum { BATCH_CHUNK_SIZE = 32 };

template<class PrimitiveType> struct FRenderBatchChunk
{
	INT					BaseIndex;
	PrimitiveType*		Primitives[BATCH_CHUNK_SIZE];
	FRenderBatchChunk*	NextChunk;
};

//
//	FRenderBatch
//

template<class PrimitiveType> class FRenderBatch
{
public:

	FRenderBatchChunk<PrimitiveType>*	FirstChunk;
	FRenderBatchChunk<PrimitiveType>*	LastChunk;
	INT									NumPrimitives;

	// Constructor.
	FRenderBatch()
	{
		FirstChunk = LastChunk = NULL;
		NumPrimitives = 0;
	}

	// AddPrimitive

	void AddPrimitive(PrimitiveType* Primitive)
	{
		if(!LastChunk || (NumPrimitives - LastChunk->BaseIndex) == BATCH_CHUNK_SIZE)
		{
			FRenderBatchChunk<PrimitiveType>*	NewChunk = new(GSceneMem) FRenderBatchChunk<PrimitiveType>;

			NewChunk->NextChunk = NULL;

			if(LastChunk)
			{
				NewChunk->BaseIndex = LastChunk->BaseIndex + BATCH_CHUNK_SIZE;
				LastChunk->NextChunk = NewChunk;
				LastChunk = NewChunk;
			}
			else
			{
				NewChunk->BaseIndex = 0;
				FirstChunk = LastChunk = NewChunk;
			}
		}

		LastChunk->Primitives[NumPrimitives - LastChunk->BaseIndex] = Primitive;
		NumPrimitives++;
	}
};

//
//	FProjectorBatchVertex
//

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,1)
#endif

struct FProjectorBatchVertex
{
	FVector	WorldPosition GCC_PACK(1);
	FColor	Attenuation GCC_PACK(1);
	FVector	ProjectedPosition GCC_PACK(1);
};

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

//
//	FProjectorRenderPrimitive
//

class FProjectorRenderPrimitive
{
public:

	INT	NumIndices,
		NumVertices;

	virtual void GetIndices(_WORD* DestIndices,_WORD BaseVertexIndex) = 0;
	virtual void GetVertices(BYTE* DestVertices) = 0;
};

//
//	FStaticProjectorPrimitive
//

class FStaticProjectorPrimitive : public FProjectorRenderPrimitive
{
public:

	FStaticProjectorInfo*	ProjectorInfo;

	// Constructor.

	FStaticProjectorPrimitive(FStaticProjectorInfo* InProjectorInfo)
	{
		ProjectorInfo = InProjectorInfo;

		NumVertices = ProjectorInfo->Vertices.Num();
		NumIndices = ProjectorInfo->Indices.Num();
	}

	// GetIndices

	virtual void GetIndices(_WORD* DestIndices,_WORD BaseVertexIndex)
	{
		for(INT Index = 0;Index < ProjectorInfo->Indices.Num();Index++)
			*DestIndices++ = ProjectorInfo->Indices(Index) + BaseVertexIndex;
	}

	// GetVertices

	virtual void GetVertices(BYTE* DestVertices)
	{
		FLOAT LifeAttenuation;
		if( ProjectorInfo->RenderInfo->Expires == 0.0 )
			LifeAttenuation = 255.f;
		else
			LifeAttenuation = Clamp<FLOAT>((ProjectorInfo->RenderInfo->Expires - ProjectorInfo->RenderInfo->LastRenderTime) * 255.f, 0.f, 255.f);
		if( ProjectorInfo->RenderInfo->FadeInTime > 0.f )
			LifeAttenuation	*= Min<FLOAT>( GCurrentTime - ProjectorInfo->RenderInfo->CreationTime, ProjectorInfo->RenderInfo->FadeInTime ) / ProjectorInfo->RenderInfo->FadeInTime;

		for(INT VertexIndex = 0;VertexIndex < ProjectorInfo->Vertices.Num();VertexIndex++)
		{
			FProjectorBatchVertex*	DestVertex	= (FProjectorBatchVertex*)DestVertices;
			BYTE*					Dummy		= DestVertices + sizeof(FProjectorBatchVertex); // avoid memory stall

			FStaticProjectorVertex*	SrcVertex	= &ProjectorInfo->Vertices(VertexIndex);

			DestVertex->WorldPosition = SrcVertex->WorldPosition;
			
			FVector ProjectedPosition = ProjectorInfo->RenderInfo->Matrix.TransformFVector(SrcVertex->WorldPosition);
			if(!(ProjectorInfo->RenderInfo->ProjectorFlags & PRF_Projected))
				ProjectedPosition.Z = 1.0f;

			DestVertices = Dummy; // avoid memory stall

			INT Attenuation = appTrunc( Clamp(SrcVertex->Attenuation * LifeAttenuation, 0.f, 255.f) );
			DestVertex->Attenuation	= Attenuation | (Attenuation << 8) | (Attenuation << 16) | (Attenuation << 24); // was: FColor(Attenuation,Attenuation,Attenuation,Attenuation)

			DestVertex->ProjectedPosition = ProjectedPosition;

			if(ProjectorInfo->BaseMaterial)
			{
				FLOAT*	DestUV = (FLOAT*)DestVertices;
				DestUV[0] = ProjectorInfo->BaseUVs(VertexIndex).U;
				DestUV[1] = ProjectorInfo->BaseUVs(VertexIndex).V;
				DestVertices += sizeof(FLOAT) * 2;
			}
		}
	}
};

//
//	FProjectorRenderBatch
//

class FProjectorRenderBatch : public FRenderBatch<FProjectorRenderPrimitive>
{
public:

	UMaterial*			BaseMaterial;
	UMaterial*			ProjectedMaterial;
	EProjectorBlending	BaseMaterialBlending;
	EProjectorBlending	FramebufferBlending;
	UBOOL				TwoSided;

	INT					VertexStride,
						VertexBufferSize,
						IndexBufferSize;

	// Constructor.

	FProjectorRenderBatch(UMaterial* InBaseMaterial,UMaterial* InProjectedMaterial,EProjectorBlending InBaseMaterialBlending,EProjectorBlending InFramebufferBlending,UBOOL InTwoSided)
	{
		BaseMaterial = InBaseMaterial;
		ProjectedMaterial = InProjectedMaterial;
		BaseMaterialBlending = InBaseMaterialBlending;
		FramebufferBlending = InFramebufferBlending;
		TwoSided = InTwoSided;

		VertexBufferSize = 0;
		IndexBufferSize = 0;

		VertexStride = sizeof(FProjectorBatchVertex);
		if(BaseMaterial)
			VertexStride += sizeof(FLOAT) * 2;
	}

	// Render

	void Render(FSceneNode* SceneNode,FRenderInterface* RI);

	// AddPrimitive

	void AddPrimitive(FProjectorRenderPrimitive* Primitive)
	{
		FRenderBatch<FProjectorRenderPrimitive>::AddPrimitive(Primitive);

		VertexBufferSize += Primitive->NumVertices * VertexStride;
		IndexBufferSize += Primitive->NumIndices * sizeof(_WORD);
	}
};

//
//	FVertexPool
//

class FVertexPool : public FVertexStream
{
private:

	QWORD			CacheId;
	INT				Size,
					BaseClientRevision,
					Revision;
	FVertexStream*	Client;

public:

	FVertexPool*	NextPool;
	INT				LifeTimeFrames;

	// Constructor.

	FVertexPool(INT InSize,FVertexPool* InNext)
	{
		CacheId = MakeCacheID(CID_RenderVertices);
		Size = InSize;
		Revision = 0;
		Client = NULL;
		NextPool = InNext;
		LifeTimeFrames = 0;
	}

	// Destructor.

	~FVertexPool()
	{
		if(NextPool)
			delete NextPool;
	}

	// GetClient

	FVertexStream* GetClient() { return Client; }

	// SetClient

	void SetClient(FVertexStream* NewClient);

	// FRenderResource interface.

	virtual QWORD GetCacheId() { return CacheId; }
	virtual INT GetRevision();

	// FVertexStream interface.

	virtual INT GetStride() { check(Client); return Client->GetStride(); }
	virtual INT GetSize() { return Size; }
	virtual UBOOL HintDynamic() { return 1; }
	virtual UBOOL UseNPatches() { return 1; }

	virtual INT GetComponents(FVertexComponent* Components) { check(Client); return Client->GetComponents(Components); }

	virtual void GetStreamData(void* Dest) { check(Client); Client->GetStreamData(Dest); }
	virtual void GetRawStreamData(void ** Dest,INT FirstVertex) { check(Client); Client->GetRawStreamData(Dest,FirstVertex); }
};

//
//	GetVertexPool
//

extern FVertexPool* GetVertexPool(FVertexStream* Client);

//
//	UpdateVertexPools
//

extern void UpdateVertexPools(UViewport* Viewport);

//
//	FBspNodeList
//

class FBspNodeList
{
public:

	UModel*	Model;
	INT		SectionIndex,
			NumNodes,
			NumTriangles;
	INT*	Nodes;

	// Constructors.

	FBspNodeList(UModel* InModel,INT InSectionIndex)
	{
		Model = InModel;
		SectionIndex = InSectionIndex;

		NumNodes = 0;
		NumTriangles = 0;
		Nodes = New<INT>(GSceneMem,Model->Sections(SectionIndex).NumNodes);
	}

	FBspNodeList(UModel* InModel)
	{
		Model = InModel;
		SectionIndex = INDEX_NONE;

		NumNodes = 0;
		NumTriangles = 0;
		Nodes = NULL;
	}

	// AddNode

	void AddNode(INT NodeIndex);
};

//
//	FBspDrawList
//

class FBspDrawList : public FBspNodeList
{
public:

	TList<class FBspLightDrawList*>*		DynamicLights;
	TList<class FBspProjectorDrawList*>*	Projectors;

	// Constructors.

	FBspDrawList(UModel* InModel,INT InSectionIndex) : FBspNodeList(InModel,InSectionIndex)
	{
		DynamicLights = NULL;
		Projectors = NULL;
	}

	FBspDrawList(UModel* InModel) :
		FBspNodeList(InModel)
	{
		DynamicLights = NULL;
		Projectors = NULL;
	}

	// AddNode

	void AddNode(INT NodeIndex,FDynamicLight** InDynamicLights,INT NumDynamicLights,FProjectorRenderInfo** InDynamicProjectors,INT NumDynamicProjectors,FLevelSceneNode* SceneNode);

	// Render

	void Render(FLevelSceneNode* SceneNode,FRenderInterface* RI);
};

//
//	FBspLightDrawList
//

class FBspLightDrawList : public FBspNodeList
{
public:

	FDynamicLight*	Light;

	// Constructor.

	FBspLightDrawList(FBspDrawList* Parent,FDynamicLight* InLight) :
		FBspNodeList(Parent->Model,Parent->SectionIndex)
	{
		Light = InLight;
	}

	// Render

	void Render(FLevelSceneNode* SceneNode,FRenderInterface* RI);
};

//
//	FBspStencilDrawList
//

class FBspStencilDrawList : public FBspNodeList
{
public:

	DWORD	StencilMask;

	FBspStencilDrawList(UModel* InModel,INT InSectionIndex,DWORD InStencilMask) : FBspNodeList(InModel,InSectionIndex)
	{
		StencilMask = InStencilMask;
	}

	// Render

	void Render(FLevelSceneNode* SceneNode,FRenderInterface* RI);
};

//
//	GetRelevantLights
//

extern TList<FDynamicLight*>* GetRelevantLights(FSceneNode* SceneNode,FDynamicActor* DynamicActor,FSphere RealBoundingSphere,FDynamicLight** Consider,INT NumConsider);

//
//	RenderLevel
//

extern void RenderLevel(FLevelSceneNode* SceneNode,FRenderInterface* RI);

//
//	CalculateStaticMeshLighting
//

extern void CalculateStaticMeshLighting(UStaticMesh* StaticMesh,UStaticMeshInstance* StaticMeshInstance,FDynamicActor* Owner);

//
//	RenderStaticMesh
//

extern void RenderStaticMesh(FDynamicActor* Owner,FLevelSceneNode* SceneNode,TList<FDynamicLight*>* Lights,TList<FProjectorRenderInfo*>* Projectors,FRenderInterface* RI);

//
//	FStaticMeshBatchList
//

struct FStaticMeshBatchList
{
	ULevel*	Level;
	INT*	Batches;
	INT**	VisibleBatchElements;
	INT*	NumVisibleBatchElements;
	INT		NumBatches;

	// Render

	void Render(FSceneNode* SceneNode,FRenderInterface* RI);
};

//
//	RenderBatchedStaticMesh
//

extern TList<FProjectorRenderBatch*>*	GProjectorBatchList;

extern void RenderBatchedStaticMesh(FStaticMeshBatchList& BatchList,FDynamicActor* Owner,FLevelSceneNode* SceneNode,FRenderInterface* RI);
