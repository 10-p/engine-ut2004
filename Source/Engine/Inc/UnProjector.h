/*=============================================================================
	UnProjector.h: Projected textures & Decals
	Copyright 2000 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

#define DECALREMOVETIME 1.f

class AProjector;

enum EProjectorFlags
{
	PRF_Projected			= 0x01,
	PRF_ProjectOnUnlit		= 0x02,
	PRF_Gradient			= 0x04,
	PRF_ProjectOnAlpha		= 0x08,
	PRF_ProjectOnBackfaces	= 0x10
};

struct FProjectorRenderInfo
{
	AProjector*			Projector;			// May not be valid.
	INT					ReferenceCount;		// Number of surfaces, actors etc referencing this data structure
	DOUBLE				LastRenderTime;		// Time of last render
	DOUBLE				Expires;			// Time the projector can be removed, 0 = never
	DOUBLE				CreationTime;		// Time projector was created
	FLOAT				FadeInTime;			// Time it takes projector to fade in
	UMaterial*			Material;
	UTexture*			GradientTexture;

	BYTE				MaterialBlendingOp,
						FrameBufferBlendingOp;
	FMatrix				Matrix,
						InverseMatrix;
	FVector				BoundingBoxCenter,
						BoundingBoxExtent;
	FPlane				FrustumPlanes[6];
	FMatrix				GradientMatrix;
	DWORD				ProjectorFlags;

	FProjectorRenderInfo( AProjector* InProjector, DOUBLE InExpires = 0, FLOAT InFadeInTime = 0.f );
	~FProjectorRenderInfo()
	{
		check(ReferenceCount==0);
	}
	inline FProjectorRenderInfo* AddReference()
	{
		ReferenceCount++;
		return this;
	}
	void RemoveReference();
	inline UBOOL Render( DOUBLE Now )
	{
        if( 
			Expires!=0 
		&&  (		UTexture::__Client->Decals==0 
				||	UTexture::__Client->Projectors==0 
				||	UTexture::__Client->Engine->GRenDev->SupportsZBIAS==0
			) 
		)
        {
            RemoveReference();
			return 0;
        }

		// See if this decal has expired.  If so, remove it.
		if( Expires!=0 && (Expires<=Now || (LastRenderTime!=0.0f && (Now-LastRenderTime>DECALREMOVETIME))) ) // sjs
		{
			RemoveReference();
			return 0;
		}

		LastRenderTime = Now;
		return 1;
	}
	UMaterial* GetMaterial(class FSceneNode* SceneNode,UMaterial* BaseMaterial);
	
	ENGINE_API friend FArchive& operator<<( FArchive& Ar, FProjectorRenderInfo* Info )
	{
		return Ar << Info->Material << Info->GradientTexture;
	}
};

struct ENGINE_API FStaticProjectorVertex
{
	FVector	WorldPosition;
	FLOAT	Attenuation;
};

struct ENGINE_API FStaticProjectorUV
{
	FLOAT	U,
			V;
};

struct ENGINE_API FStaticProjectorInfo
{
	FProjectorRenderInfo*			RenderInfo;
	UMaterial*						BaseMaterial;
	TArray<FStaticProjectorVertex>	Vertices;
	TArray<FStaticProjectorUV>		BaseUVs;	// Only if RenderInfo->BaseMaterialBlendingOp != PB_None or base material has opacity.
	TArray<_WORD>					Indices;
	UBOOL							TwoSided;

	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FStaticProjectorInfo* ProjectorInfo)
	{
		return Ar << ProjectorInfo->RenderInfo << ProjectorInfo->BaseMaterial;
	}
};

class ENGINE_API UProjectorPrimitive : public UPrimitive
{
	DECLARE_CLASS(UProjectorPrimitive,UPrimitive,0,Engine);

	// UPrimitive interface
	virtual UBOOL UseCylinderCollision( const AActor* Owner );
	virtual FBox GetCollisionBoundingBox( const AActor* Owner ) const;
	virtual UBOOL PointCheck
	(
		FCheckResult	&Result,
		AActor			*Actor,
		const FVector&	Location,
		const FVector&	Extent,
		DWORD           ExtraNodeFlags
	);
	virtual UBOOL LineCheck
	(
		FCheckResult&	Result,
		AActor*			Owner,
		const FVector&	End,
		const FVector&	Start,
		const FVector&	Extent,
		DWORD           ExtraNodeFlags,
		DWORD			TraceFlags
	);
	virtual void Destroy();
	
	virtual FVector GetEncroachExtent(AActor* Owner) { return GetCollisionBoundingBox(Owner).GetExtent(); }
	virtual FVector GetEncroachCenter(AActor* Owner) { return GetCollisionBoundingBox(Owner).GetCenter(); }
};

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

