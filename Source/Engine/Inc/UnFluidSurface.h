
#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

class AFluidSurfaceInfo;

class ENGINE_API UFluidSurfacePrimitive : public UPrimitive
{
	DECLARE_CLASS(UFluidSurfacePrimitive,UPrimitive,0,Engine);
	NO_DEFAULT_CONSTRUCTOR(UFluidSurfacePrimitive)

	AFluidSurfaceInfo*	FluidInfo;
	
	// UPrimitive interface.
	virtual UBOOL LineCheck
	(
		FCheckResult	&Result,
		AActor			*Owner,
		const FVector&	End,
		const FVector&	Start,
		const FVector&	Extent,
		DWORD           ExtraNodeFlags,
		DWORD			TraceFlags
	);

	virtual UBOOL PointCheck
	(
		FCheckResult	&Result,
		AActor			*Owner,
		const FVector&	Location,
		const FVector&	Extent,
		DWORD           ExtraNodeFlags
	);

	virtual FBox GetRenderBoundingBox( const AActor* Owner );
	virtual FSphere GetRenderBoundingSphere( const AActor* Owner );
	virtual FBox GetCollisionBoundingBox( const AActor* Owner ) const;
	virtual UBOOL UseCylinderCollision( const AActor* Owner );

	// UObject interface.
	virtual void Serialize(FArchive& Ar);
};

enum EFluidGridType
{
    FGT_Square              =0,
    FGT_Hexagonal           =1,
    FGT_MAX                 =2,
};

class ENGINE_API AFluidSurfaceInfo : public AInfo
{
public:
    DECLARE_CLASS(AFluidSurfaceInfo,AInfo,0,Engine)
	NO_DEFAULT_CONSTRUCTOR(AFluidSurfaceInfo)

	BYTE FluidGridType;
	FLOAT FluidGridSpacing;
    INT FluidXSize;
    INT FluidYSize;
	FLOAT FluidHeightScale;
    FLOAT FluidSpeed;
	FLOAT FluidTimeScale;
    FLOAT FluidDamping;
    FLOAT FluidNoiseFrequency;
    FRange FluidNoiseStrength;
    BITFIELD TestRipple:1 GCC_PACK(4);
    FLOAT TestRippleSpeed GCC_PACK(4);
    FLOAT TestRippleStrength;
	FLOAT TestRippleRadius;
    FLOAT UTiles;
	FLOAT UOffset;
    FLOAT VTiles;
	FLOAT VOffset;
	FLOAT AlphaCurveScale;
	FLOAT AlphaHeightScale;
	BYTE AlphaMax;
	FLOAT ShootStrength;
	FLOAT ShootRadius;
	FLOAT RippleVelocityFactor;
	FLOAT TouchStrength;
	class UClass* ShootEffect;
    BITFIELD OrientShootEffect:1	GCC_PACK(4);	
	class UClass* TouchEffect		GCC_PACK(4);
    BITFIELD OrientTouchEffect:1	GCC_PACK(4);
	TArray<DWORD> ClampBitmap		GCC_PACK(4);
	ATerrainInfo* ClampTerrain;
	BITFIELD bShowBoundingBox:1		GCC_PACK(4);
	BITFIELD bUseNoRenderZ:1;
	FLOAT NoRenderZ                 GCC_PACK(4);
	FLOAT WarmUpTime;
	FLOAT UpdateRate;
	FColor FluidColor;
    TArrayNoInit<FLOAT> Verts0;
    TArrayNoInit<FLOAT> Verts1;
	TArrayNoInit<BYTE>	VertAlpha;
    INT LatestVerts;
    FBox FluidBoundingBox;
	FVector FluidOrigin;
	FLOAT TimeRollover;
    FLOAT TestRippleAng;
	UFluidSurfacePrimitive* Primitive;
	TArrayNoInit<AFluidSurfaceOscillator*> Oscillators;
	BITFIELD bHasWarmedUp			GCC_PACK(4);

	DECLARE_FUNCTION(execPling);

	void Render(FDynamicActor* Actor,
		class FLevelSceneNode* SceneNode,
		TList<class FDynamicLight*>* Lights,
		FRenderInterface* RI);

	void RenderEditorInfo(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA);
	
	void Init();

	void FillVertexBuffer(void* Dest);
	void SimpleFillVertexBuffer(void* Dest);

	void FillIndexBuffer(void* Data);
	void SimpleFillIndexBuffer(void* Data);

	void RebuildClampedBitmap();

	void Pling(const FVector& Position, FLOAT Strength, FLOAT Radius);
	void PlingVertex(INT x, INT y, FLOAT Strength);

	inline UBOOL GetClampedBitmap( INT x, INT y )
	{
		INT BitIndex = x + y * FluidXSize;
		return (ClampBitmap(BitIndex>>5) & (1<<(BitIndex&0x1f))) ? 1 : 0;
	}
	
	inline void SetClampedBitmap( INT x, INT y, UBOOL Clamp )
	{
		INT BitIndex = x + (y * FluidXSize);
		INT Index = BitIndex>>5;
		DWORD Bitmask = 1 << (BitIndex&0x1f);
		if( Clamp )
			ClampBitmap(Index) |= Bitmask;
		else
			ClampBitmap(Index) &= ~Bitmask;
	}

	FVector GetVertexPosLocal(INT x, INT y);
	FVector GetVertexPosWorld(INT x, INT y);
	void GetNearestIndex(const FVector& pos, INT& xIndex, INT& yIndex);

	void UpdateSimulation(FLOAT DeltaTime);
	void UpdateOscillatorList();


	// Actor interface
	virtual UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );
	virtual void PostLoad();
	virtual void PostEditChange();
	virtual void PostEditMove();
	virtual void Spawned();
	virtual void Destroy();
	virtual UPrimitive* GetPrimitive();
	virtual UBOOL ShouldTrace(AActor *SourceActor, DWORD TraceFlags);
};

struct FFluidSurfaceVertex
{
	FVector	Position,
			Normal;
	FColor	Color;
	FLOAT	U,
			V;
};

class FFluidSurfaceVertexStream : public FVertexStream
{
public:

	AFluidSurfaceInfo*			Fluid;
	QWORD						CacheId;
	INT							Revision;

	FFluidSurfaceVertexStream(AFluidSurfaceInfo* InFluid)
	{
		Fluid = InFluid;
		CacheId = MakeCacheID(CID_RenderVertices);
		Revision = 0;
	}

	virtual QWORD GetCacheId()
	{
		return CacheId;
	}

	virtual INT GetRevision()
	{
		return Revision;
	}

	virtual INT GetStride()
	{
		return sizeof(FFluidSurfaceVertex);
	}

	virtual void GetRawStreamData(void ** Dest, INT FirstVertex )
	{
		*Dest = 0;
	}

	virtual INT GetSize();
	virtual void GetStreamData(void* Dest);
	virtual INT GetComponents(FVertexComponent* OutComponents);
};

class FFluidSurfaceIndexBuffer : public FIndexBuffer
{
public:
	AFluidSurfaceInfo*		Fluid;
	QWORD					CacheId;

	FFluidSurfaceIndexBuffer(AFluidSurfaceInfo* InFluid)
	{
		Fluid = InFluid;
		CacheId	= MakeCacheID(CID_RenderIndices);
	}

	virtual QWORD GetCacheId()
	{
		return CacheId;
	}

	virtual INT GetRevision()
	{
		return 1;
	}

	virtual INT GetIndexSize()
	{
		return sizeof(_WORD);
	}

	virtual INT GetSize();
	virtual void GetContents(void* Data);
};

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif
