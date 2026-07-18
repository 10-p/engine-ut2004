/*=============================================================================
	UnModel.h: Unreal UModel definition.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

//
//	Definitions.
//

#define LIGHTMAP_MAX_WIDTH			256
#define LIGHTMAP_MAX_HEIGHT			256

#define LIGHTMAP_TEXTURE_WIDTH		512
#define LIGHTMAP_TEXTURE_HEIGHT		512

//
//	FBspVertex
//

struct ENGINE_API FBspVertex
{
	FVector	Position;
	FVector	Normal;
	FLOAT	U,
			V;
	FLOAT	U2,
			V2;

	// Serializer.

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FBspVertex& V)
	{
		Ar	<< V.Position
			<< V.U
			<< V.V
			<< V.U2
			<< V.V2;
		
		if(Ar.Ver() >= 109)
			Ar	<< V.Normal;

		return Ar;
	}
};

//
//	FBspVertexStream
//

class ENGINE_API FBspVertexStream : public FVertexStream
{
public:

	TArray<FBspVertex>			Vertices;
	QWORD						CacheId;
	INT							Revision;

	// Constructor.

	FBspVertexStream()
	{
		CacheId = MakeCacheID(CID_RenderVertices);
		Revision = 0;
	}

	// FVertexStream interface.

	virtual INT GetSize() { return Vertices.Num() * sizeof(FBspVertex); }
	virtual INT GetStride()	{ return sizeof(FBspVertex); }
	virtual INT GetComponents(FVertexComponent* OutComponents)
	{
		OutComponents[0].Type = CT_Float3;
		OutComponents[0].Function = FVF_Position;
		OutComponents[1].Type = CT_Float3;
		OutComponents[1].Function = FVF_Normal;
		OutComponents[2].Type = CT_Float2;
		OutComponents[2].Function = FVF_TexCoord0;
		OutComponents[3].Type = CT_Float2;
		OutComponents[3].Function = FVF_TexCoord1;

		return 4;
	}
	virtual void GetStreamData(void* Dest) { appMemcpy(Dest,&Vertices(0),Vertices.Num() * sizeof(FBspVertex)); }
	virtual void GetRawStreamData(void** Dest,INT FirstVertex) { *Dest = &Vertices(FirstVertex); }

	// FRenderResource interface.

	virtual QWORD GetCacheId() { return CacheId; }
	virtual INT GetRevision() {	return Revision; }

	// Serializer.

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FBspVertexStream& S)
	{
		return Ar	<< S.Vertices
					<< S.Revision;
	}
};

//
//	FLightBitmap
//

class FLightBitmap
{
public:

	AActor*			LightActor;
	TArray<BYTE>	Bits;
	INT				SizeX,
					SizeY,
					MinX,
					MinY,
					MaxX,
					MaxY,
					OffsetX,	// Offset into framebuffer when rendering light visibility: internal use only.
					OffsetY,
					Stride;

	// Constructors.

	FLightBitmap()
	{
	}

	FLightBitmap(AActor* InActor,INT InMinX,INT InMinY,INT InMaxX,INT InMaxY)
	{
		LightActor = InActor;
		MinX = InMinX;
		MinY = InMinY;
		MaxX = InMaxX;
		MaxY = InMaxY;
		SizeX = MaxX - MinX + 1;
		SizeY = MaxY - MinY + 1;
		Stride = appCeil(SizeX / 8.0f);

		Bits.AddZeroed(Stride * SizeY);
	}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FLightBitmap& B)
	{
		UObject*&	LightActor = (UObject*&) B.LightActor;

		Ar	<< LightActor
			<< B.Bits
			<< B.SizeX
			<< B.SizeY;

		if(Ar.Ver() >= 91)
			Ar << B.Stride;

		if(Ar.Ver() >= 107)
		{
			Ar	<< B.MinX
				<< B.MinY
				<< B.MaxX
				<< B.MaxY;
		}

		return Ar;
	}
};

//
//	FLightMap
//

class ENGINE_API FLightMap : public FTexture
{
public:

	ULevel*					Level;
	INT						iTexture,
							iSurf,
							iZone,
							OffsetX,
							OffsetY,
							SizeX,
							SizeY,
							Revision;
	FMatrix					WorldToLightMap;
	FVector					LightMapBase,
							LightMapX,
							LightMapY;
	TArray<FLightBitmap>	Bitmaps;
	TArray<AActor*>			DynamicLights;

	// Constructors.

	FLightMap() {}
	FLightMap(ULevel* InLevel,INT InSurfaceIndex,INT InZoneIndex);

	// FTexture interface.

	virtual void* GetRawTextureData(INT MipIndex) { return NULL; }
	virtual void UnloadRawTextureData( INT MipIndex ) {}
	virtual void GetTextureData(INT MipIndex,void* Dest,INT DestStride,ETextureFormat DestFormat,UBOOL ColoredMips);
	virtual UTexture* GetUTexture() { return NULL; }

	// FBaseTexture interface.

	virtual INT GetWidth() { return SizeX; }
	virtual INT GetHeight() { return SizeY; }
	virtual INT GetFirstMip() { return 0; }
	virtual INT GetNumMips() { return 1; }
	virtual ETextureFormat GetFormat() { return TEXF_RGBA8; }

	virtual ETexClampMode GetUClamp() { return TC_Clamp; }
	virtual ETexClampMode GetVClamp() { return TC_Clamp; }

	// FRenderResource interface.

	virtual QWORD GetCacheId() { return 0; }
	virtual INT GetRevision() { return Revision; }

	// Serializer.

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FLightMap& L)
	{
		Ar	<< AR_INDEX(L.iTexture)
			<< AR_INDEX(L.iSurf)
			<< AR_INDEX(L.iZone)
			<< AR_INDEX(L.OffsetX)
			<< AR_INDEX(L.OffsetY)
			<< AR_INDEX(L.SizeX)
			<< AR_INDEX(L.SizeY)
			<< L.WorldToLightMap
			<< L.LightMapBase
			<< L.LightMapX
			<< L.LightMapY;

		if(Ar.Ver() >= 107)
		{
			Ar	<< L.Bitmaps;

			if(Ar.Ver() < 108)
			{
				UBOOL	Dirty = 0;

				Ar	<< Dirty;
			}
		}

		if(Ar.Ver() >= 110)
		{
			Ar	<< *(UObject**)&L.Level
				<< L.Revision;
		}
		else if(Ar.IsLoading())
		{
			L.Level = NULL;
			L.Revision = 0;
		}

		return Ar;
	}
};

//
//	FStaticLightMapTexture
//

class ENGINE_API FStaticLightMapTexture : public FTexture
{
public:	

	TLazyArray<BYTE>	Data[2];	// Two mip maps.
	BYTE				Format;
	INT					Width,
						Height;

	QWORD				CacheId;
	INT					Revision;

	// Constructors.

	FStaticLightMapTexture()
	{
		CacheId = MakeCacheID(CID_RenderTexture);
		Revision = 0;
	}

	// FTexture interface.

	virtual void* GetRawTextureData(INT MipIndex);
	virtual void UnloadRawTextureData( INT MipIndex );
	virtual void GetTextureData(INT MipIndex,void* Dest,INT DestStride,ETextureFormat DestFormat,UBOOL ColoredMips);
	virtual UTexture* GetUTexture() { return NULL; }

	// FBaseTexture interface.

	virtual INT GetWidth() { return Width; }
	virtual INT GetHeight() { return Height; }
	virtual INT GetFirstMip();
	virtual INT GetNumMips() { return 2; }
	virtual ETextureFormat GetFormat() { return (ETextureFormat) Format; }
	virtual ETexClampMode GetUClamp() { return TC_Wrap; }
	virtual ETexClampMode GetVClamp() { return TC_Wrap; }

	// FRenderResource interface.

	virtual QWORD GetCacheId() { return CacheId; }
	virtual INT GetRevision() { return Revision; }

	// Serializer.

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FStaticLightMapTexture& T)
	{
		return Ar	<< T.Data[0]
					<< T.Data[1]
					<< T.Format
					<< T.Width
					<< T.Height
					<< T.Revision;
	}
};

//
//	FLightMapTexture
//

class ENGINE_API FLightMapTexture : public FCompositeTexture
{
public:

	ULevel*					Level;
	TArray<INT>				LightMaps;
	FStaticLightMapTexture	StaticTexture;

	QWORD					CacheId;
	INT						Revision;

	// Constructors.

	FLightMapTexture() {}
	FLightMapTexture(ULevel* InLevel);

	// FCompositeTexture interface.

	virtual INT GetNumChildren() { return LightMaps.Num(); }
	virtual FTexture* GetChild(INT ChildIndex,INT* OutChildX,INT* OutChildY);

	// FBaseTexture interface.

	virtual INT GetWidth() { return LIGHTMAP_TEXTURE_WIDTH; }
	virtual INT GetHeight() { return LIGHTMAP_TEXTURE_HEIGHT; }
	virtual INT GetFirstMip() { return 0; }
	virtual INT GetNumMips() { return 1; }
	virtual ETextureFormat GetFormat() { return TEXF_RGBA8; }
	virtual ETexClampMode GetUClamp() { return TC_Wrap; }
	virtual ETexClampMode GetVClamp() { return TC_Wrap; }

	// FRenderResource interface.

	virtual QWORD GetCacheId() { return CacheId; }
	virtual INT GetRevision() { return Revision; }

	// Serializer.

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FLightMapTexture& T)
	{
		Ar	<< *(UObject**)&T.Level
			<< T.LightMaps
			<< T.CacheId
			<< T.Revision;

		if(Ar.Ver() >= 116)
			Ar << T.StaticTexture;

		return Ar;
	}
};

//
//	FBspSection
//

class ENGINE_API FBspSection
{
public:

	FBspVertexStream	Vertices;
	INT					NumNodes;

	UMaterial*			Material;
	DWORD				PolyFlags;

	INT					iLightMapTexture;

	// Constructor.

	FBspSection()
	{
		NumNodes = 0;
		iLightMapTexture = INDEX_NONE;

		Material = NULL;
		PolyFlags = 0;
	}

	// Serializer.

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FBspSection& S)
	{
		Ar	<< S.Vertices
			<< S.Material
			<< S.NumNodes
			<< S.PolyFlags
			<< S.iLightMapTexture;

		if(Ar.Ver() < 107 && Ar.IsLoading())
			S.iLightMapTexture = INDEX_NONE;

		return Ar;
	}
};

/*-----------------------------------------------------------------------------
	UModel.
-----------------------------------------------------------------------------*/

//
// Model objects are used for brushes and for the level itself.
//
enum {MAX_NODES  = 65536};
enum {MAX_POINTS = 128000};
class ENGINE_API UModel : public UPrimitive
{
	DECLARE_CLASS(UModel,UPrimitive,0,Engine)

	// Arrays and subobjects.
	UPolys*						Polys;
	TTransArray<FBspNode>		Nodes;
	TTransArray<FVert>			Verts;
	TTransArray<FVector>		Vectors;
	TTransArray<FVector>		Points;
	TTransArray<FBspSurf>		Surfs;
	TArray<FBox>				Bounds;
	TArray<INT>					LeafHulls;
	TArray<FLeaf>				Leaves;
	TArray<AActor*>				Lights;

	TArray<FBspSection>			Sections;
	TArray<FLightMapTexture>	LightMapTextures;
	TArray<FLightMap>			LightMaps;

	TArray<INT>					DynamicLightMaps;

	// Other variables.
	UBOOL						RootOutside;
	UBOOL						Linked;
	INT							MoverLink;
	INT							NumSharedSides;
	INT							NumZones;
	FZoneProperties				Zones[FBspNode::MAX_ZONES];

	// Constructors.
	UModel()
	: RootOutside( 1 )
	, Surfs( this )
	, Vectors( this )
	, Points( this )
	, Verts( this )
	, Nodes( this )
	{
		EmptyModel( 1, 0 );
	}
	UModel( ABrush* Owner, UBOOL InRootOutside=1 );

	// UObject interface.
	void Serialize( FArchive& Ar );
	void PostLoad();
	void Destroy();
	virtual void Rename( const TCHAR* InName=NULL, UObject* NewOuter=NULL );

	// UPrimitive interface.
	UBOOL PointCheck
	(
		FCheckResult	&Result,
		AActor			*Owner,
		const FVector&	Location,
		const FVector&	Extent,
		DWORD           ExtraNodeFlags
	);
	UBOOL LineCheck
	(
		FCheckResult	&Result,
		AActor			*Owner,
		const FVector&	End,
		const FVector&	Start,
		const FVector&	Extent,
		DWORD           ExtraNodeFlags,
		DWORD			TraceFlags
	);
	FBox GetCollisionBoundingBox( const AActor *Owner ) const;
	virtual UBOOL UseCylinderCollision( const AActor* Owner );
	FBox GetRenderBoundingBox( const AActor* Owner );

	virtual FVector GetEncroachExtent(AActor* Owner);
	virtual FVector GetEncroachCenter(AActor* Owner);

	// UPrimitive illumination interface.
	virtual void Illuminate(AActor* Owner,UBOOL ChangedOnly);

	// UModel interface.
	void Modify( UBOOL DoTransArrays=0 );
	void BuildBound();
	void Transform( ABrush* Owner );
	void EmptyModel( INT EmptySurfInfo, INT EmptyPolys );
	void ShrinkModel();
	UBOOL PotentiallyVisible( INT iLeaf1, INT iLeaf2 );
	BYTE FastLineCheck( const FVector& End, const FVector& Start );

	// BoxLeaves - Returns the leaves a bounding box touches.
	TArray<INT> BoxLeaves(FBox Box);

	// ClearRenderData - Clears the render data.
	void ClearRenderData(URenderDevice* RenDev);

	// BuildRenderData - Builds the data used to render the model.
	void BuildRenderData();

	// Render - Renders the UModel as a wireframe brush.
	void Render(class FDynamicActor* Owner,class FLevelSceneNode* SceneNode,class FRenderInterface* RI);

	// CompressLightmaps
	void CompressLightmaps();
	
	// UModel transactions.
	void ModifySelectedSurfs( UBOOL UpdateMaster );
	void ModifyAllSurfs( UBOOL UpdateMaster );
	void ModifySurf( INT Index, UBOOL UpdateMaster );

	// UModel collision functions.
	typedef void (*PLANE_FILTER_CALLBACK )(UModel *Model, INT iNode, int Param);
	typedef void (*SPHERE_FILTER_CALLBACK)(UModel *Model, INT iNode, int IsBack, int Outside, int Param);
	FPointRegion PointRegion( AZoneInfo* Zone, FVector Location ) const;
	FLOAT FindNearestVertex
	(
		const FVector	&SourcePoint,
		FVector			&DestPoint,
		FLOAT			MinRadius,
		INT				&pVertex
	) const;
	void PrecomputeSphereFilter
	(
		const FPlane	&Sphere
	);
	INT ConvexVolumeMultiCheck( FBox& Box, FPlane* Planes, INT NumPlanes, FVector Normal, TArray<INT>& Result );
	void AttachProjector( INT iNode, FProjectorRenderInfo* RenderInfo, FPlane* FrustumPlanes );
};

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/


