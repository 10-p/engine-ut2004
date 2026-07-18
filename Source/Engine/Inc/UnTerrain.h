/*=============================================================================
	UnTerrain.h: Unreal terrain objects.
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Jack Porter
=============================================================================*/

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

/*------------------------------------------------------------------------------
    Misc stuff.
------------------------------------------------------------------------------*/

struct FTerrainVertex
{
	FVector	Position,
			Normal;
	FColor	Color;
	FLOAT	U,
			V;

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FTerrainVertex& V)
	{
		return Ar
			<< V.Position
			<< V.Normal
			<< V.Color
			<< V.U
			<< V.V;
	}
};

class FTerrainVertexStream : public FVertexStream
{
public:

	TArray<FTerrainVertex>	Vertices;
	QWORD					CacheId;
	INT						Revision;

	// Constructor.

	FTerrainVertexStream()
	{
		CacheId = MakeCacheID(CID_RenderVertices);
		Revision = 0;
	}

	// GetCacheId

	virtual QWORD GetCacheId()
	{
		return CacheId;
	}

	// GetRevision

	virtual INT GetRevision()
	{
		return Revision;
	}

	// GetSize

	virtual INT GetSize()
	{
		return Vertices.Num() * sizeof(FTerrainVertex);
	}

	// GetStride

	virtual INT GetStride()
	{
		return sizeof(FTerrainVertex);
	}

	// GetComponents

	virtual INT GetComponents(FVertexComponent* OutComponents)
	{
		OutComponents[0].Type = CT_Float3;
		OutComponents[0].Function = FVF_Position;
		OutComponents[1].Type = CT_Float3;
		OutComponents[1].Function = FVF_Normal;
		OutComponents[2].Type = CT_Color;
		OutComponents[2].Function = FVF_Diffuse;
		OutComponents[3].Type = CT_Float2;
		OutComponents[3].Function = FVF_TexCoord0;
		return 4;
	}

	// GetStreamData

	virtual void GetStreamData(void* Dest)
	{
		appMemcpy(Dest,&Vertices(0),Vertices.Num() * sizeof(FTerrainVertex));
	}

	// GetRawStreamData

	virtual void GetRawStreamData(void ** Dest, INT FirstVertex )
	{
		*Dest = &Vertices(FirstVertex);
	}

	// Serializer.

	friend ENGINE_API FArchive& operator<<(FArchive& Ar,FTerrainVertexStream& VertexStream)
	{
		return Ar	<< VertexStream.Vertices
					<< VertexStream.Revision;
	}
};

struct FTerrainNormalPair
{
	FVector Normal1;
	FVector Normal2;

	friend FArchive& operator<<( FArchive& Ar, FTerrainNormalPair& N )
	{
		return Ar << N.Normal1 << N.Normal2;
	}
};

enum
{
	TEXMAPAXIS_XY	= 0,
	TEXMAPAXIS_XZ	= 1,
	TEXMAPAXIS_YZ	= 2,
	TEXMAPAXIS_MAX	= 3,
};


enum ESortOrder
{
	SORT_NoSort		= 0,
	SORT_BackToFront= 1,
	SORT_FrontToBack= 2,
};

struct FTerrainLayer
{
	UMaterial*		Texture;
	UTexture*		AlphaMap;
	FLOAT			UScale;
	FLOAT			VScale;
	FLOAT			UPan;
	FLOAT			VPan;
	BYTE			TextureMapAxis;
	FLOAT			TextureRotation;
	FRotator		LayerRotation;
	FMatrix			TextureMatrix;
	FLOAT			KFriction;
	FLOAT			KRestitution;
	UTexture*		LayerWeightMap;	// layer weight map (precomputed from alphamap)
};

struct FDecorationLayer
{
	UBOOL			ShowOnTerrain;
	UTexture*		ScaleMap;
	UTexture*		DensityMap;
	UTexture*		ColorMap;
	UStaticMesh*	StaticMesh;
	FRangeVector	ScaleMultiplier;
	FRange			FadeoutRadius;
	FRange			DensityMultiplier;
	INT				MaxPerQuad;
	INT				Seed;
	UBOOL			AlignToTerrain;
	BYTE			DrawOrder;
	UBOOL			ShowOnInvisibleTerrain;
	UBOOL			LitDirectional;
	UBOOL			DisregardTerrainLighting;
	UBOOL			RandomYaw;
	BYTE			DetailMode;
};

struct FDecoInfo
{
	FVector		Location;
	FRotator	Rotation;
	FVector		Scale;
	FVector		TempScale;
	FColor		Color;
	INT			Distance;

	friend FArchive& operator<<( FArchive& Ar, FDecoInfo& DI )
	{
		return Ar << DI.Location << DI.Rotation << DI.Scale << DI.TempScale << DI.Color << DI.Distance;
	}
}; 

struct FDecoSectorInfo
{
	TArray<FDecoInfo>			DecoInfo;
	FVector						Location;
	FLOAT						Radius;

	friend FArchive& operator<<( FArchive& Ar, FDecoSectorInfo& DSI )
	{
		return Ar << DSI.DecoInfo << DSI.Location << DSI.Radius;
	}
};

struct FDecorationLayerData
{
	TArray<FDecoSectorInfo>		Sectors;

	friend FArchive& operator<<( FArchive& Ar, FDecorationLayerData& DLD )
	{
		return Ar << DLD.Sectors;
	}
};

enum ETerrainRenderMethod
{
	RM_WeightMap			= 0,
	RM_CombinedWeightMap	= 1,
	RM_AlphaMap				= 2,
};

//
// Combination of layers, etc.
//
struct FTerrainRenderCombination
{
	TArray<INT> Layers;
	ETerrainRenderMethod RenderMethod;
	UTexture* CombinedWeightMaps;

	friend FArchive& operator<<( FArchive& Ar, FTerrainRenderCombination& C )
	{
		return Ar << C.Layers << *(BYTE*)(&C.RenderMethod) << C.CombinedWeightMaps;
	}
};


//
// Sector info for terrain layers rendered in a single pass.
//
struct FTerrainSectorRenderPass
{
	ATerrainInfo* TerrainInfo;
	INT RenderCombinationNum;

	TArray<_WORD> Indices;
	INT NumTriangles;
	INT NumIndices;
	DWORD MinIndex;
	DWORD MaxIndex;

	FTerrainSectorRenderPass()
	:	RenderCombinationNum(-1)
	,	NumTriangles		(0)
	,	NumIndices			(0)
	,	MinIndex			(0)
	,	MaxIndex			(0)
	{}

	inline FTerrainRenderCombination* GetRenderCombination(ATerrainInfo* InTerrainInfo);

	friend FArchive& operator<<( FArchive& Ar, FTerrainSectorRenderPass& P )
	{
		return Ar	<< P.RenderCombinationNum
					<< P.Indices << P.NumTriangles << P.NumIndices
					<< P.MinIndex << P.MaxIndex;
	}
};

struct FTerrainSectorLightInfo
{
	AActor*			LightActor;
	TArray<BYTE>	VisibilityBitmap;

	FTerrainSectorLightInfo() {}

	FTerrainSectorLightInfo( AActor* InActor )
		: LightActor(InActor) {}

	friend FArchive& operator<<( FArchive& Ar, FTerrainSectorLightInfo& I )
	{
		return Ar << I.LightActor << I.VisibilityBitmap;
	}
};

struct FTerrainStaticProjectorInfo
{
	FStaticProjectorInfo* Info;
	INT MinQuadX;
	INT MinQuadY;
	INT MaxQuadX;
	INT MaxQuadY;

	FTerrainStaticProjectorInfo(FStaticProjectorInfo* InInfo, INT InMinQuadX, INT InMinQuadY, INT InMaxQuadX, INT InMaxQuadY )
	:	Info(InInfo)
	,	MinQuadX(InMinQuadX)
	,	MinQuadY(InMinQuadY)
	,	MaxQuadX(InMaxQuadX)
	,	MaxQuadY(InMaxQuadY)
	{}
	FTerrainStaticProjectorInfo()
	:	Info(NULL)
	{}

	friend FArchive& operator<<( FArchive& Ar, FTerrainStaticProjectorInfo& I )
	{
		return Ar << I.Info << I.MinQuadX << I.MinQuadY << I.MaxQuadX << I.MaxQuadY;
	}

};

/*------------------------------------------------------------------------------
    UTerrainSector.
------------------------------------------------------------------------------*/

class ENGINE_API UTerrainSector : public UObject
{
	DECLARE_CLASS(UTerrainSector,UObject,0,Engine);
	NO_DEFAULT_CONSTRUCTOR(UTerrainSector)

	TArray<FTerrainSectorRenderPass> RenderPasses;
	TArray<FTerrainSectorLightInfo> LightInfos;
	ATerrainInfo* Info;
	FBox BoundingBox;			// Bounding box
	INT QuadsX, QuadsY;			// Dimensions
	INT OffsetX, OffsetY;
	FVector Location;			// Center
	FLOAT	Radius;				// Radius
	TArray<FTerrainStaticProjectorInfo> Projectors;

	INT VertexStreamNum;
	_WORD VertexStreamOffset;

	FRawIndexBuffer CompleteIndexBuffer;
	INT CompleteNumTriangles;
	INT CompleteNumIndices;
	DWORD CompleteMinIndex;
	DWORD CompleteMaxIndex;

#ifdef __PSX2_EE__
	// PS2 specific data for this patch
	void* PS2Data;
#endif

	// Constructor.
	UTerrainSector( ATerrainInfo* InInfo, INT InOffsetX, INT InOffsetY, INT QuadsX, INT QuadsY );

	// UObject interface.
	virtual void Serialize(FArchive& Ar);
	virtual void PostLoad();
	virtual void Destroy();

	// UTerrainSector interface
	void GenerateTriangles( UBOOL CheckLayerBlending=1 );
	void StaticLight( UBOOL Force );
#ifdef _XBOX
	INT GetGlobalVertex( INT x, INT y );
#else
	inline INT GetGlobalVertex( INT x, INT y );
#endif
	INT GetLocalVertex( INT x, INT y ) { return x + y*(QuadsX+1); }
	inline UBOOL IsTriangleAll( INT Layer, INT X, INT Y, INT Tri, UBOOL Turned, BYTE AlphaValue );
	UBOOL IsSectorAll( INT Layer, BYTE AlphaValue );
	UBOOL PassShouldRenderTriangle( INT Pass, INT X, INT Y, INT Tri, UBOOL Turned );
		

	// Projectors
	void AttachProjector( AProjector* InProjector, FProjectorRenderInfo* InRenderInfo, INT MinQuadX, INT MinQuadY, INT MaxQuadX, INT MaxQuadY );
};

/*------------------------------------------------------------------------------
    UTerrainPrimitive.
------------------------------------------------------------------------------*/

class ENGINE_API UTerrainPrimitive : public UPrimitive
{
	DECLARE_CLASS(UTerrainPrimitive,UPrimitive,0,Engine);
	NO_DEFAULT_CONSTRUCTOR(UTerrainPrimitive)

	ATerrainInfo* Info;

	// Constructor
	UTerrainPrimitive( ATerrainInfo* InInfo )
		: Info( InInfo ) {}

	// UPrimitive interface
	UBOOL LineCheck(FCheckResult &Result,AActor* Owner,const FVector& End,const FVector& Start,const FVector& Extent,DWORD ExtraNodeFlags,DWORD TraceFlags);
	UBOOL PointCheck(FCheckResult& Result,AActor* Owner,const FVector& Point,const FVector& Extent,DWORD ExtraNodeFlags);
	FBox GetRenderBoundingBox( const AActor* Owner, UBOOL Exact );
	void Illuminate(AActor* Owner,UBOOL ChangedOnly);

	// UObject interface.
	virtual void Serialize(FArchive& Ar);
};

/*------------------------------------------------------------------------------
    ATerrainInfo.
------------------------------------------------------------------------------*/

struct FSelectedTerrainVertex
{
	INT X, Y;
	INT OldHeight;
	FLOAT Weight, Delta;

	friend FArchive& operator<<( FArchive& Ar, FSelectedTerrainVertex& V )
	{
		return Ar << V.X << V.Y << V.Weight << V.OldHeight << V.Delta;
	}
};

class ENGINE_API ATerrainInfo : public AInfo
{
    DECLARE_CLASS(ATerrainInfo,AInfo,0,Engine)

	// Editor Properties.
	INT							TerrainSectorSize;
	class UTexture*				TerrainMap;
    class UTexture*             VertexLightMap; // sjs
    FVector						TerrainScale;
	FTerrainLayer				Layers[32];
	TArray<FDecorationLayer>	DecoLayers;
	FLOAT						DecoLayerOffset;
    BITFIELD					Inverted:1		GCC_PACK(4);
    BITFIELD					bKCollisionHalfRes:1;

	// Variables.
	UBOOL						JustLoaded		GCC_PACK(4);
	TArray<FDecorationLayerData>DecoLayerData;
	TArray<UTerrainSector*>		Sectors;
	TArray<FVector>				Vertices;
	INT							HeightmapX;
	INT							HeightmapY;
	INT							SectorsX;
	INT							SectorsY;
	UTerrainPrimitive*			Primitive;
	
	TArray<FTerrainNormalPair>	FaceNormals;
	FCoords						ToWorld;
	FCoords						ToHeightmap;
	TArray<FSelectedTerrainVertex>		SelectedVertices;
	INT							ShowGrid;
	TArray<DWORD>				QuadVisibilityBitmap;
	TArray<DWORD>				EdgeTurnBitmap;
	TArray<UMaterial*>			QuadDomMaterialBitmap; // Dominant material of each quad.
	TArray<FTerrainRenderCombination> RenderCombinations;
	TArray<FTerrainVertexStream> VertexStreams;
	TArray<FColor>				VertexColors;
	TArray<FColor>				PaintedColor;		// editor only
	class UTexture*				CollapsedLayers;

	// Deprecated.
	class UTexture*				OldTerrainMap;
	TArray<_WORD>				OldHeightmap;


	// Constructors.
	ATerrainInfo(); 

	// UObject Interface
	void PostEditChange();
	virtual void Serialize(FArchive& Ar);
	void PostLoad();
	virtual void Destroy();

	// AActor Interface.
	UPrimitive* GetPrimitive() { return Sectors.Num() ? (Primitive ? Primitive : (Primitive = new(this) UTerrainPrimitive(this))): AActor::GetPrimitive(); }
	void CheckForErrors();

	// ATerrainInfo Interface.
	void SetupSectors();
	void Update( FLOAT Time, INT StartX=0, INT StartY=0, INT EndX=0, INT EndY=0, UBOOL UpdateLighting=0 );
	inline INT GetGlobalVertex( INT x, INT y ) { return x + y * HeightmapX; }
	void Render(
		FLevelSceneNode* SceneNode,
		FRenderInterface* RI,
		FVisibilityInterface* VI,
		FDynamicLight** DynamicLights,
		INT NumDynamicLights,
		FProjectorRenderInfo** DynamicProjectors,
		INT NumDynamicProjectors
	);

	// DecoLayers.
	void UpdateDecorations( INT SectorIndex );
	void RenderDecorations(FLevelSceneNode* SceneNode,FRenderInterface* RI,FVisibilityInterface* VI);	

	// Collision
	UBOOL LineCheck( FCheckResult &Result, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags, UBOOL CheckInvisibleQuads );
	UBOOL LineCheckWithQuad( INT X, INT Y, FCheckResult &Result, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags, UBOOL CheckInvisibleQuads );

	UBOOL PointCheck( FCheckResult &Result, const FVector& Point, const FVector& Extent, UBOOL CheckInvisibleQuads=0 );

	// Editor
	UBOOL GetClosestVertex( FVector& InLocation, FVector* InOutput, INT* InX, INT* InY );
	UBOOL SelectVertexX( INT InX, INT InY );
	UBOOL SelectVertex( FVector Location );
	void ConvertHeightmapFormat();
	void SoftSelect( FLOAT InnerRadius, FLOAT OuterRadius );
	void SoftDeselect();
	void SelectVerticesInBox( FBox& InRange );
	void MoveVertices( FLOAT Delta );
	void ResetMove();
	FBox GetSelectedVerticesBounds();
	void UpdateFromSelectedVertices();
    void SmoothColors( void ); // sjs

	// internal
	void CheckComputeDataOnLoad();
	INT GetRenderCombinationNum( TArray<INT>& Layers, ETerrainRenderMethod RenderMethod );
	void UpdateVertices( FLOAT Time, INT StartX, INT StartY, INT EndX, INT EndY );
	void UpdateTriangles( INT StartX, INT StartY, INT EndX, INT EndY, UBOOL UpdateLighting=0, UBOOL CheckLayerBlending=1 );
	void CalcCoords();
	void CalcLayerTexCoords();
	FVector GetVertexNormal( INT x, INT y ); 
	void PrecomputeLayerWeights();
	void CombineLayerWeights();



#if __STATIC_LINK || __LINUX__
	BYTE GetLayerAlpha( INT x, INT y, INT Layer, UTexture* InAlphaMap = NULL );
	void SetLayerAlpha( FLOAT x, FLOAT y, INT Layer, BYTE Alpha, UTexture* InAlphaMap = NULL );
	_WORD GetHeightmap( INT x, INT y );
	void SetHeightmap( INT x, INT y, _WORD w );
	FColor GetTextureColor( INT x, INT y, UTexture* InTextureMap );
	void SetTextureColor( INT x, INT y, UTexture* InTextureMap, FColor& Color );
#else
	inline BYTE GetLayerAlpha( INT x, INT y, INT Layer, UTexture* InAlphaMap = NULL );
	inline void SetLayerAlpha( FLOAT x, FLOAT y, INT Layer, BYTE Alpha, UTexture* InAlphaMap = NULL );
	inline _WORD GetHeightmap( INT x, INT y );
	inline void SetHeightmap( INT x, INT y, _WORD w );
	inline FColor GetTextureColor( INT x, INT y, UTexture* Texture );
	inline void SetTextureColor( INT x, INT y, UTexture* Texture, FColor& Color );
#endif
	UBOOL GetQuadVisibilityBitmap( INT x, INT y )
	{
		INT BitIndex = x + y * HeightmapX;
		return (QuadVisibilityBitmap(BitIndex>>5) & (1<<(BitIndex&0x1f))) ? 1 : 0;
	}
	void SetQuadVisibilityBitmap( INT x, INT y, UBOOL Visible )
	{
		INT BitIndex = x + y * HeightmapX;
		INT Index = BitIndex>>5;
		DWORD Bitmask = 1 << (BitIndex&0x1f);
		if( Visible )
			QuadVisibilityBitmap(Index) |= Bitmask;
		else
			QuadVisibilityBitmap(Index) &= ~Bitmask;
	}
	UBOOL GetEdgeTurnBitmap( INT x, INT y )
	{
		INT BitIndex = x + y * HeightmapX;
		return (EdgeTurnBitmap(BitIndex>>5) & (1<<(BitIndex&0x1f))) ? 1 : 0;
	}
	void SetEdgeTurnBitmap( INT x, INT y, UBOOL Turned )
	{
		INT BitIndex = x + y * HeightmapX;
		INT Index = BitIndex>>5;
		DWORD Bitmask = 1 << (BitIndex&0x1f);
		if( Turned )
			EdgeTurnBitmap(Index) |= Bitmask;
		else
			EdgeTurnBitmap(Index) &= ~Bitmask;
	}
	UMaterial* GetQuadDomMaterialBitmap( INT x, INT y )
	{
		if(QuadDomMaterialBitmap.Num() > 0)
			return (UMaterial*)QuadDomMaterialBitmap(x + (y * HeightmapX));
		else
			return NULL;
	}
	FVector HeightmapToWorld( FVector H )
	{
		return H.TransformPointBy( ToWorld );
	}
	FVector WorldToHeightmap( FVector W )
	{
		return W.TransformPointBy( ToHeightmap );
	}

	// Script functions
	DECLARE_FUNCTION(execPokeTerrain)
};

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

