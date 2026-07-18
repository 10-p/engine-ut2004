/*=============================================================================
	UnParticleSystem.h: Unreal Particle System.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,4)
#endif

enum EParticleFlags
{
	PTF_None			= 0,
	PTF_Active			= 1,
	PTF_NoTick			= 2,
	PTF_InitialSpawn	= 4
};

enum EParticleSpawnFlags
{
	PSF_None			= 0,
	PSF_NoGlobalOffset	= 1,
	PSF_NoOwnerLocation	= 2
};

struct FParticleBeamData
{
	FVector		Location;
	FLOAT		t;
};

struct FParticleBeamEndPoint
{
	FName			ActorTag;
	FRangeVector	Offset;
	FLOAT			Weight;
};


struct FParticleBeamScale
{
	FVector		FrequencyScale;
	FLOAT		RelativeLength;		// always in range [0..1]
};

struct FParticleSparkData
{
	FLOAT		TimeBeforeVisible;
	FLOAT		TimeBetweenSegments;
	FVector		StartLocation;
	FVector		StartVelocity;
};

struct FParticleTrailData
{
	FVector		Location;
	FColor		Color;
	FLOAT		Size;
	DOUBLE		Time;
};

struct FParticleTrailInfo
{
	INT			TrailIndex;
	INT			NumPoints;
	FVector		LastLocation;			// last point location (to compute new points)
	FVector		LastEmitterLocation;	// Last Emitter Location, to process points location interpolation
};

struct FParticleTimeScale
{
	FLOAT		RelativeTime;		// always in range [0..1]
	FLOAT		RelativeSize;
};

struct FParticleRevolutionScale
{
	FLOAT		RelativeTime;		// always in range [0..1]
	FVector		RelativeRevolution;
};

struct FParticleColorScale
{
	FLOAT		RelativeTime;		// always in range [0..1]
	FColor		Color;
};

struct FParticleVelocityScale
{
	FLOAT	RelativeTime;		// always in range [0..1]
	FVector	RelativeVelocity;
};

class USound;
struct FParticleSound
{
	USound*	Sound;
	FRange	Radius;
	FRange	Pitch;
	INT		Weight;
	FRange	Volume;
	FRange	Probability;
};

struct FParticle
{
	FVector		Location;
	FVector		OldLocation;
	FVector		Velocity;
	FVector		StartSize;
	FVector		SpinsPerSecond;
	FVector		StartSpin;
	FVector		RevolutionCenter;
	FVector		RevolutionsPerSecond;
	FVector		RevolutionsMultiplier;
	FVector		Size;
	FVector		StartLocation;
	FVector		ColorMultiplier;
	FVector		VelocityMultiplier;
	FVector		OldMeshLocation;
	FColor		Color;
	FLOAT		Time;
	FLOAT		MaxLifetime;
	FLOAT		Mass;
	INT			HitCount;
	INT			Flags;
	INT			Subdivision;
	INT			BoneIndex;
};

//
//	FParticleVertexStream
//
template<class VertexClass> class FParticleVertexStream : public FVertexStream
{
public:

	TArray<VertexClass>			Vertices;
	QWORD						CacheId;

	FParticleVertexStream()
	{
		CacheId = MakeCacheID(CID_RenderVertices);
	}

	virtual QWORD GetCacheId()
	{
		return CacheId;
	}

	virtual INT GetRevision()
	{
		return 1;
	}

	virtual INT GetSize()
	{
		return Vertices.Num() * sizeof(VertexClass);
	}

	virtual INT GetStride()
	{
		return sizeof(VertexClass);
	}

	virtual INT GetComponents(FVertexComponent* OutComponents)
	{
		return VertexClass::GetComponents(OutComponents);
	}

	virtual void GetStreamData(void* Dest)
	{
		appMemcpy(Dest,&Vertices(0),Vertices.Num() * sizeof(VertexClass));
	}
	virtual void GetRawStreamData(void ** Dest, INT FirstVertex )
	{
		*Dest = &Vertices(FirstVertex);
	}
};

#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

