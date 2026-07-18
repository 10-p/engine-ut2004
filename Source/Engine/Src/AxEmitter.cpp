//=============================================================================
// AxEmitter - implementation
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#include "EnginePrivate.h"
#include "UnRenDev.h"
#include "xParticleMgr.h"
#include <math.h>

#include "xOctTree.h"
#define USEPOOL 0

#define UNSEEN_SUSPEND  1

// static mesh particle source - todo: fix these awful demo hacks, use mesh collision data for better performance
struct PclSourceTri
{
    FVector center;
    float   radiusSqr;
    float   wt;
};
TArray<PclSourceTri>    cachedTriData;
UStaticMesh*            cachedMesh = NULL;
TArray<int>             cachedTriIndex;
FVector                 cachedOrigin(-1,-1,-1);
float                   cachedRadius = 0.0f;
float                   cachedMaxArea = -99999.9f;
OctNode<INT>            cachedOctTree;


const int       MAX_BRANCHES = 10;
extern INT	    pcl_index[MAX_PARTICLES_PER];
void GetRandStaticMeshVertex( FVector& emitPos, AStaticMeshActor* sourceActor, FVector origin, float radius );
DWORD MeshPclInitCycles = 0;
const float ForceRadiusTweak = 100.0f;

static void UpdateSourceMesh(AxEmitter* pEmitter)
{
    if( pEmitter->mSpawningType == ST_StaticMesh && pEmitter->StaticMesh )
    {
        pEmitter->mBounds = pEmitter->StaticMesh->GetRenderBoundingBox(pEmitter); // should be in tick?
    }
    else if( pEmitter->SourceStaticMesh == NULL )
    {
        for( int i=0; i<pEmitter->XLevel->iFirstDynamicActor; i++ )
        {
            if( pEmitter->XLevel->Actors(i) && pEmitter->XLevel->Actors(i)->Tag == pEmitter->mSourceActor
                && pEmitter->XLevel->Actors(i)->IsA(AStaticMeshActor::StaticClass()))
            {
                pEmitter->SourceStaticMesh = (AStaticMeshActor*)pEmitter->XLevel->Actors(i);
                break;
            }
        }
        if( !GIsEditor && pEmitter->SourceStaticMesh == NULL ) // failed to find it, prevent trying every frame
            pEmitter->mSourceActor = NAME_None;
    }
}

UBOOL AxEmitter::IsForceAffected()
{
	guard(AxEmitter::IsForceAffected);
	if(bForceAffected == false)
		return false;

	ULevel* level = GetLevel();
	if(!level || level->Actors.Num() == 0 || level->Actors(0) == NULL)
		return false;

	ALevelInfo* lInfo = level->GetLevelInfo();

	if(lInfo && lInfo->PhysicsDetailLevel == PDL_High)
		return true;
	else
		return false;
	unguard;
}

void UpdateXEmitterCollision(AxEmitter* pEmitter)
{
	// Ensure collision is off if emitter is not force affected (regardless of bCollideActors flag)
    if( !pEmitter->IsForceAffected() )
    {
        if( pEmitter->bCollideActors )
        {
            pEmitter->SetCollision( false, false );
        }
        return;
    }

    if( !pEmitter->bCollideActors )
        pEmitter->SetCollision( true, false );

    FVector center, extents;
    pEmitter->mBounds.GetCenterAndExtents(center, extents);
	float NewEmitterRadius = appSqrt( Square(extents.X) + Square(extents.Y) ) + ForceRadiusTweak;
	float NewEmitterHeight = extents.Z + ForceRadiusTweak;
	// copied from vogel: If new radius or height are bigger than old one increase new one by 20% to decrease
	// frequency of updates.
	if ( (NewEmitterRadius > pEmitter->CollisionRadius) || (NewEmitterHeight > pEmitter->CollisionHeight) )
	{
		if ( NewEmitterRadius > pEmitter->CollisionRadius )
			NewEmitterRadius *= 1.2f;
		if ( NewEmitterHeight > pEmitter->CollisionHeight )
			NewEmitterHeight *= 1.2f;
        
		pEmitter->SetCollisionSize( NewEmitterRadius, NewEmitterHeight );
	}
}

DWORD CalcBlendFlags( BYTE Style, AActor* pActor )
{
    DWORD ExtraFlags = 0;
    // Handle DrawStyle overloading:
    switch( Style )
    {
        case STY_None:
            ExtraFlags = PF_Invisible;
        case STY_Normal:
        case STY_Particle:
            break;
        case STY_Masked:
            ExtraFlags = PF_Masked;
            break;
	    case STY_Translucent:
            ExtraFlags = PF_Translucent;
            break;
	    case STY_Modulated:
            ExtraFlags = PF_Modulated;
            break;
        case STY_Alpha:
            ExtraFlags = PF_AlphaTexture;
            break;
        case STY_Additive:
            ExtraFlags = PF_Additive;
            break;
        case STY_Subtractive:
            ExtraFlags = PF_Subtractive;
            break;
        case STY_AlphaZ:
            ExtraFlags = PF_AlphaTexture | PF_Occlude;
            break;
        default:
            check(false);
            break;
    }
    if ( pActor )
    {
	    if( pActor->bSelected )
            ExtraFlags |= PF_Selected;
    }
    return ExtraFlags;
}


// function helpers
#define Randf   qFRand
static inline float RandRangef( FLOAT Min, FLOAT Max )
{
	return Min + (Max - Min) * Randf();
}

template< class T > T static FloatLerp( T& A, T& B, FLOAT Alpha )
{
	float fa = (float)A;
	float fb = (float)B;
	return (T)(fa + Alpha * (fb-fa));
}

static inline void CalcRandomColor( FColor& outColor, FColor mColorRange[2], float atten )
{
	if ( mColorRange[RNG_LO] != mColorRange[RNG_HI] )
	{
		float t = qFRand() * atten;
		outColor.R = FloatLerp(mColorRange[0].R, mColorRange[1].R, t );
		outColor.G = FloatLerp(mColorRange[0].G, mColorRange[1].G, t );
		outColor.B = FloatLerp(mColorRange[0].B, mColorRange[1].B, t );
		outColor.A = FloatLerp(mColorRange[0].A, mColorRange[1].A, t );
	}
	else
		outColor = mColorRange[RNG_LO];
}

static inline FVector Randv()
{
	FVector v;
	float w, t, sq;
	
	v.Z = 2.0 * Randf() - 1.0;
	t = 2.0 * PI * Randf();
	sq = 1.0f - v.Z*v.Z;
	w = appSqrt( sq );
	v.X = w * appCos( t );
	v.Y = w * appSin( t );
	return v;
}

static inline FVector Randv2D()
{
	FVector Result;
	Result.X = 0.0f;
	do
	{
		Result.Y = appFrand()*2 - 1;
		Result.Z = appFrand()*2 - 1;
	} while( Result.SizeSquared() > 1.f );
	return Result.UnsafeNormal();
}

static inline float LerpInOut( float ratio, float x )
{
	if ( x < ratio )
	{
		float t = 1.0f / ratio;
		return x * t;
	}
	else
	{
		float t = 1.0f / (1.0f - ratio);
		return t * (1.0f - x);
	}
}

static inline INT appRandRange( INT Min, INT Max )
{
	INT Range = Max - Min;
	INT R = Range>0 ? qRand() % Range : 0;
	return R + Min;
}

static inline float SmoothStep( float x )
{
	return ( x * x * (3.0f - 2.0f * x) );
}

static inline float GetRandomRange( float rng[2] )
{
	return (rng[RNG_LO] + ((rng[RNG_HI] - rng[RNG_LO] ) * Randf())); 
}

static void ClearCachedMesh()
{
    if(!cachedMesh)
        return;

    cachedTriData.Empty();
    cachedMesh = NULL;
    cachedTriIndex.Empty();
    cachedOrigin = FVector(-1,-1,-1);
    cachedRadius = 0.0f;
    cachedMaxArea = -99999.9f;
    for ( int i=0; i<8; i++ )
	{
		delete cachedOctTree.pChildren[i];
        cachedOctTree.pChildren[i] = 0;
        cachedOctTree.contents.Empty();
	}
}

// singleton particle iterator instances
SpriteIterator	sSpriteIterator;
StreamIterator	sStreamIterator;
LineIterator    sLineIterator;
DiscIterator    sDiscIterator;
MeshIterator    sMeshIterator;
BeamIterator    sBeamIterator;
BranchIterator	sBranchIterator;

// pool manager - global singleton
ParticlePoolMgr gPclPool; 

void AxEmitter::PostEditChange()
{
	guard(AxEmitter::PostEditChange);
    PreCalc();
	Super::PostEditChange();
    UpdateXEmitterCollision(this);
    UpdateSourceMesh(this);
	unguard;
}

void AxEmitter::PostBeginPlay()
{
	guard(AxEmitter::PostLoad);

	if ( !Level->bStartup && Level->bBegunPlay )
		return;

	Reset();
	PreCalc();
	Initialize();

    if // FIXED rain splats after the fact
    (
        SourceStaticMesh &&
        SourceStaticMesh->StaticMesh &&
        bStasis == 1 && 
        bHidden == 1
		&& (Level->DetailMode != DM_Low)
		&& (Level->NetMode != NM_DedicatedServer)
    )
    {
        ClearCachedMesh();
        bStasis = 0;
        bHidden = 0;
    }
    
	Super::PostLoad();
	unguard;
}

void AxEmitter::Destroy()
{
	guard(AxEmitter::Destroy);

    if( SourceStaticMesh && cachedMesh == SourceStaticMesh->StaticMesh )// FIXED rain splats after the fact
    {
        ClearCachedMesh();
    }

	if ( mpParticles && mRegen == 0 )
	{
#if USEPOOL
		gPclPool.AddArray( ((PclArrayPtr)mpParticles) );
#else
        delete (PclArrayPtr)mpParticles;
#endif
		mpParticles = NULL;
	}
	if ( mpParticles != NULL )
	{
		delete ((PclArrayPtr)mpParticles);
		mpParticles = NULL;
	}
	Super::Destroy();
	unguard;
}

static void InitBranchPcl( ParticleT& p, FVector pos, short nextIdx, float life, float size, BYTE flag )
{
	p.life = life;
	p.invLifeSpan	= 1.0f / p.life;
	p.tileNum = 0;
	p.delay = 0.0f;
	p.rotInertia[0] = flag;
	p.pos = pos;
	p.size = size;
	p.mass = 0.0f;
	p.color = FColor(255,255,255,255);
	p.velocity = FVector(0,0,0);
	p.nextIndex = nextIdx;
}

void SpawnBranchParticles( AxEmitter* pEmitter, int* freePclIdx, int numFreePcl )
{
	ParticleT* p = &(*(PclArrayPtr)pEmitter->mpParticles)(0);

	FVector start = pEmitter->Location;
	FVector end = pEmitter->mSpawnVecA;
	FVector dir = end - start;
	FVector mid;
	float	len = dir.Size();
	dir *= 1.0f / len;

	if ( dir.Size() == 0.0f )
		return;

	float branchProb = pEmitter->mSpawnVecB.X;
	int branchNum = 0;
	int branchStart[MAX_BRANCHES];
	int usedPcl = 0;
	float life = pEmitter->mLifeRange[0];
	float size = pEmitter->mSizeRange[0];

	int minToSpawn = 2;

	if ( numFreePcl < minToSpawn )
		return;

    pEmitter->mNumActivePcl += numFreePcl;

	branchStart[0] = freePclIdx[0];
	branchNum = 1;

	InitBranchPcl( p[ freePclIdx[0] ], start, freePclIdx[1], life, size, 1 );
	InitBranchPcl( p[ freePclIdx[1] ], end, -1, life, size, 0 );

	usedPcl = 2;
	// use a subdivision method
	while( usedPcl < numFreePcl )
	{
		for ( int b=0; b<branchNum; b++ )
		{
			int curIdx = branchStart[b];
			int nextIdx = p[ curIdx ].nextIndex;
			while ( nextIdx > -1 )
			{
				// add a subdiv
				start = p[ curIdx ].pos;
				end = p[ nextIdx ].pos;
				mid = Lerp( start, end, RandRangef( 0.4f, 0.6f ) );
				mid += Randv() * pEmitter->mPosDev;

				InitBranchPcl( p[ freePclIdx[usedPcl] ], mid, nextIdx, life, size, 0 );
				// set current's next to new mid point
				p[ curIdx ].nextIndex = freePclIdx[usedPcl];
				usedPcl++;

				if ( usedPcl >= numFreePcl )
					break;

				// check branch prob
				if ( Randf() < branchProb && usedPcl < (numFreePcl-2) && branchNum < MAX_BRANCHES-1 )
				{
					InitBranchPcl( p[ freePclIdx[usedPcl] ], mid, freePclIdx[usedPcl+1], life, size, 1 );
					usedPcl++;

					mid += dir + (Randv() * pEmitter->mDirDev) * len * 0.10f;
					InitBranchPcl( p[ freePclIdx[usedPcl] ], mid, -1, life, size, 0 );
					usedPcl++;
				}

				if ( usedPcl >= numFreePcl )
					break;

				// do not use new mid point as 'next' otherwise we'll recurse into the subdivision!
				curIdx = nextIdx;
				nextIdx = p[ curIdx ].nextIndex;
			}
		}
	}
}

void AxEmitter::Initialize()
{
    guard(AxEmitter::Initialize);

    if ( blockOnNet && mSpawnVecA.SizeSquared()==0.0f )
    {
        mMaxParticles = -mMaxParticles;
        return;
    }

	if ( Level->NetMode == NM_DedicatedServer )
		return;

    if( mSpawningType == ST_StaticMesh )
	{
        UpdateSourceMesh(this);
	}

    mHeadIndex = INDEX_NONE;
	mMaxParticles = Min( mMaxParticles, ((PclArrayPtr)mpParticles)->Num() );
    ParticleT* p = &(*(PclArrayPtr)mpParticles)(0);

    if ( mParticleType == PT_Beam )
    {
        mWavePhaseA = Randf()*65536.0f;
        mWavePhaseB = Randf()*65536.0f;
        mSpinRange[1] = mRandOrient ? Randf()*65536.0f : 0.0f;
        mLifeRange[1] = 0.0f;
    }

	if ( mpIterator == (PTRINT)&sBranchIterator )
	{
        mBounds.IsValid = 0;
	    mBounds += Location;
		mHeadIndex = 0;
		for (int i=0; i<mStartParticles; i++, p++)
			pcl_index[i] = i;
		SpawnBranchParticles( this, pcl_index, mStartParticles );
        ParticleT* p = &(*(PclArrayPtr)mpParticles)(0);
        for (int i=0; i<mStartParticles; i++, p++)
            mBounds += p->pos;
	}
	else
	{
		for (int i=0; i<mStartParticles; i++, p++)
		{
			InitParticle( i );
			mBounds += p->pos;
		}
	}
    int remain = mMaxParticles - mStartParticles;
    if( remain > 0 )
        appMemzero( p, sizeof(ParticleT)*remain );

	mBounds = mBounds.ExpandBy( mSizeRange[RNG_HI] );
	mRegenTimer = 0.0f;
	mLastPos = Location;

	//jag -- When loading in game, reset bounding volume. This will grow to right size in game.
	if(!GIsEditor)
	{
		// If there is a level/hash, do the full 'set collision size'
		if( GetLevel() && GetLevel()->Hash )
	SetCollisionSize(0.1f, 0.1f);
		else // Otherwise, just change the numbers.
		{
			CollisionRadius = 0.1f;
			CollisionHeight = 0.1f;
		}
	}
	// jag

    UpdateXEmitterCollision(this);

    unguard;
}

void AxEmitter::Spawned()
{
	guard(AxEmitter::Spawned);

    Super::Spawned();

	if ( Level->NetMode == NM_DedicatedServer )
	{
		if ( LifeSpan == 0.f )
		{
			LifeSpan = Max(mLifeRange[0], mLifeRange[1]);
			LifeSpan += Max(mDelayRange[0], mDelayRange[1]);
			if ( LifeSpan == 0.f )
			{
				debugf(TEXT("%s force lifespan to 0.3 on dedicated server"),GetName());
				LifeSpan = 0.3f;
			}
		}
		return;
	}

    // optional event, made this conditional for experimentation
    if( bCallPreSpawn )
        eventPreSpawned();

	Reset();
    // apply attenuation
    guard(AxEmitter::Spawned::GettingParticleMem);
    GParticleAtten = Clamp( GParticleAtten, 0.01f, 1.0f );
    mMaxParticles = appRound( (float)mMaxParticles * GParticleAtten );
    mStartParticles = appRound( (float)mStartParticles * GParticleAtten );
	if ( mpParticles == 0 )
	{
#if USEPOOL
		mpParticles = (PTRINT)gPclPool.GetArray( mMaxParticles );
#endif
	}
    unguard;

	PreCalc();
	Initialize();

#if UNSEEN_SUSPEND
    if( mRegen == 0  ) // set this effect's lifespan to reflect particle maximums.
    {
        mTime = 0.0f; // accumulator for deferred ticking
        LifeSpan = Max(mLifeRange[0], mLifeRange[1]);
        LifeSpan += Max(mDelayRange[0], mDelayRange[1]);

        // try to estimate bbox
	    mBounds += Location;
	    mRenderableVerts = 0;
        float maxPclMove = mPosDev.Size();
        maxPclMove += Max(mSpeedRange[0], mSpeedRange[1]) * LifeSpan;
        maxPclMove += Max(mSizeRange[0], mSizeRange[1]) + (fabsf(mGrowthRate) * LifeSpan);
        mBounds = mBounds.ExpandBy(maxPclMove);
        ClearRenderData();
    }
#endif

	

    LastRenderTime = Level->TimeSeconds;
	unguard;
}

void AxEmitter::Reset( void )
{
    guard(AxEmitter::Reset);
	mNumActivePcl = 0;
	mNumUpdates = 0;
	mAtLeastOneFrame = 0;
	mRenderableVerts = 0;
	SystemHandle = INDEX_NONE;
    mHeadIndex = INDEX_NONE;
	if ( mpParticles != NULL )
	{
		delete ((PclArrayPtr)mpParticles);
		mpParticles = NULL;
	}
    unguard;
}

void AxEmitter::InitParticle( int index )
{
	guard(AxEmitter::InitParticle);
	ParticleT& rPcl = (*(PclArrayPtr)mpParticles)(index);

	FVector emitPos;
	FVector savedLoc(0.0f,0.0f,0.0f);
	
	if ( mPosRelative )
	{
		savedLoc = Location;
		Location = FVector(0.f,0.f,0.f);
	}
	
	emitPos = Location;

	if ( mSpawningType == ST_Sphere )
	{
	    FVector emitDir = mDir + (Randv() * mDirDev);
		rPcl.pos = emitPos + Randv() * mPosDev;
	    rPcl.velocity = emitDir * GetRandomRange(mSpeedRange);
	}
	else if ( mSpawningType == ST_AimedSphere )
	{
		FVector emitDir = Randv2D();
		emitDir = emitDir.TransformVectorBy(GMath.UnitCoords * Rotation);
        rPcl.pos = emitPos + emitDir * mPosDev;
        rPcl.velocity = emitDir * GetRandomRange(mSpeedRange);
	}
    else if ( mSpawningType == ST_Explode )
    {
        // mDir=Rotation, mDirDev, mSpeedRange, mPosDev
	    FVector emitDir = Randv();
        rPcl.pos = emitPos + emitDir * mPosDev;
        rPcl.velocity = emitDir * GetRandomRange(mSpeedRange);
    }
    else if ( mSpawningType == ST_ExplodeRing )
    {
	    FVector emitDir = Randv2D();
        //if ( mPosRelative )
          //  emitDir = emitDir.TransformVectorBy(GMath.UnitCoords * Rotation);
        rPcl.pos = emitPos + emitDir * mPosDev;
        rPcl.velocity = emitDir * GetRandomRange(mSpeedRange);
    }
	else if ( mSpawningType == ST_Line ) // todo: implement
	{
		if ( mPosRelative )
		{
			FVector toVec = mSpawnVecA - emitPos;
			toVec *= Randf();
			rPcl.pos = toVec + (Randv() * mPosDev);
		}
		else
		{
			FVector interp = emitPos + Randf() * ( mSpawnVecA - emitPos );
			rPcl.pos	   = interp + (Randv() * mPosDev);
		}
	}
    else if ( mSpawningType == ST_StaticMesh && SourceStaticMesh )
	{
        GetRandStaticMeshVertex( emitPos, SourceStaticMesh, Location, mPosDev.X );
        rPcl.pos = emitPos;
        rPcl.pos.Z += mPosDev.Z;
        FVector emitDir = mDir + (Randv() * mDirDev);
        rPcl.velocity = emitDir * GetRandomRange(mSpeedRange);
	}
    else if ( mSpawningType == ST_OwnerSkeleton && Owner && Owner->Mesh )
    {
        USkeletalMeshInstance* pInst = Cast<USkeletalMeshInstance>(Owner->MeshInstance);
        USkeletalMesh* skelMesh = pInst ? Cast<USkeletalMesh>(pInst->GetMesh()) : NULL;
        if( skelMesh && skelMesh->TagAliases.Num() > 0 && pInst && pInst->OurActor == Owner )
        {
            // Evaluate skeleton if not already present / updated #debug - use instance's last game GTicks
	        if(!pInst->SpaceBases.Num() || (pInst->LastGTicks < GTicks))
	        {
		        AActor* InstOwner = pInst->GetActor();		
		        if( InstOwner )
		        {
			        INT DummyVerts;
			        pInst->GetFrame( InstOwner, NULL, NULL, 0, DummyVerts, GF_BonesOnly); 
		        }
	        }
            FMatrix MeshToWorldMatrix = pInst->MeshToWorld();
            int tries = 0;
            while( tries < 10 )
            {
                INT BoneIdx = appRand() % pInst->SpaceBases.Num();
                if( pInst->SpaceBases.Num() && BoneIdx != INDEX_NONE && BoneIdx < pInst->SpaceBases.Num() )
	            {
	                bool excludeBone = false;
	                // exclude zero scale bones from this test and bones whose parent is zero scale
	                for( int i=BoneIdx; i!=0; i=skelMesh->RefSkeleton(i).ParentIndex )
                    {
                        for(INT s=0; s<pInst->Scalers.Num(); s++ )
		                {
			                if( pInst->Scalers(s).Bone == i && pInst->Scalers(s).ScaleUniform == 0.0f )
			                {
                                excludeBone = true;
                                break;
			                }
		                }   
		                if( excludeBone )
		                    break;
                    }
                    if( excludeBone )
                        continue;

                    FVector offset = Location - Owner->Location;
		            FVector OriginA = MeshToWorldMatrix.TransformFVector( pInst->SpaceBases(BoneIdx).Origin );
                    FVector OriginB = OriginA;
                    if( skelMesh->RefSkeleton(BoneIdx).ParentIndex )
                    {
                        OriginB = MeshToWorldMatrix.TransformFVector( pInst->SpaceBases(skelMesh->RefSkeleton(BoneIdx).ParentIndex).Origin );
                    }
                    FVector emitDir = mDir + (Randv() * mDirDev);
		            rPcl.pos = Lerp(OriginA, OriginB, appFrand()) + Randv() * mPosDev;
                    rPcl.pos += offset;
	                rPcl.velocity = emitDir * GetRandomRange(mSpeedRange);
                    break;
	            }
            }
        }
    }

    // inherit owner velocity
    if (Owner && mOwnerVelocityFactor > 0.0f)
    {
        rPcl.velocity += Owner->Velocity * mOwnerVelocityFactor;
    }

	rPcl.delay			= GetRandomRange( mDelayRange );
	rPcl.life			= GetRandomRange( mLifeRange );
	rPcl.invLifeSpan	= 1.0f / rPcl.life;
	rPcl.size			= GetRandomRange( mSizeRange );
	rPcl.mass			= GetRandomRange( mMassRange );

	if ( !mTileAnimation )
	{
		rPcl.tileNum = (BYTE)appRandRange(0, (int)mTotalTiles );
	}
	else
	{
		rPcl.tileNum = 0;
	}

	CalcRandomColor( rPcl.color, mColorRange, ScaleGlow );

	if ( mRandOrient )
	{
		rPcl.rot.Yaw = (short)appRandRange( 0.0f, 65535.0f );
		rPcl.rot.Pitch = (short)appRandRange( 0.0f, 65535.0f );
		rPcl.rot.Roll = (short)appRandRange( 0.0f, 65535.0f );
	}
    else if (mParticleType==PT_Mesh)
    {
        if (mPosRelative)
            rPcl.rot.Roll = Rotation.Roll;
        else
            rPcl.rot = Rotation;
    }
    else
    {
        rPcl.rot = FRotator(0,0,0);
    }

	if ( mSpinRange[RNG_LO] != 0 || mSpinRange[RNG_HI] != 0)
	{
		rPcl.spin = (INT)(GetRandomRange( mSpinRange ) * 65535.0f / 360.0f);
		rPcl.rotInertia[0] = (char)(GetRandomRange( mSpinRange ) / 360.0f * 127.0f);
		rPcl.rotInertia[1] = (char)(GetRandomRange( mSpinRange ) / 360.0f * 127.0f);
		rPcl.rotInertia[2] = (char)(GetRandomRange( mSpinRange ) / 360.0f * 127.0f);
	}
	else
    {
        rPcl.spin = 0;
		rPcl.rotInertia[0] = rPcl.rotInertia[1] = rPcl.rotInertia[2] = 0;
    }

	if ( mRandMeshes )
		rPcl.tileNum = qRand() % ARRAY_COUNT( mMeshNodes );

	if ( mPosRelative )
	{
		Location = savedLoc;
	}

	mNumActivePcl++;

    // current head of system now.
    rPcl.nextIndex = mHeadIndex;
    mHeadIndex = index;

	unguard;
}

void AxEmitter::PreCalc( void )
{
	guard(AxEmitter::PreCalc);

	if ( Level->NetMode == NM_DedicatedServer )
		return;

	// sanity checks
	if ( mStartParticles < 0 || mStartParticles > MAX_PARTICLES_PER )
		mStartParticles = 0;

	if ( mMaxParticles > MAX_PARTICLES_PER )
		mMaxParticles = MAX_SYSTEMS;

	if( mRegen==1 && mMaxParticles > mStartParticles ) // try to trim max particles
	{
		float maxLife = Max( mLifeRange[0], mLifeRange[1] );
		float maxRegen = Max( mRegenRange[0], mRegenRange[1] );

		if( maxRegen == 0.0f )
		{
			//debugf(TEXT("%s is set to regenerate, but without any regen rate!"), GetName() );
		}
		else
		{
			int maxRegenAmount = appRound(maxLife * maxRegen);
			if( maxRegenAmount && maxRegenAmount < mMaxParticles )
			{
				//debugf(TEXT("Particles: Truncated %d max to %d [maxLife: %f maxRegen: %f"), mMaxParticles, maxRegenAmount, maxLife, maxRegen );
				//mMaxParticles = maxRegenAmount + 1;
			}
		}
	}
	else if ( mRegen==0 )
	{
		mMaxParticles = mStartParticles;
	}

	if ( mMaxParticles < mStartParticles)
		mMaxParticles = mStartParticles;

	PclArrayPtr* ptr = (PclArrayPtr*)&mpParticles;
	// todo: using tarray right now for particle node mem - change this back to static pools
	if ( !mpParticles )
	{	
		(*ptr) = new TArray<ParticleT>;
		(*ptr)->Empty( mMaxParticles );
        (*ptr)->Add( mMaxParticles );
	}

	if ( (*ptr)->Num() != mMaxParticles )
	{
		(*ptr)->Empty( mMaxParticles );
        (*ptr)->Add( mMaxParticles );
	}
    /*
	else if ( GIsEditor && ((*ptr)->Num() > mMaxParticles && mRegen==0) )
	{
		(*ptr)->Empty(  mMaxParticles );
        (*ptr)->Add( mMaxParticles );
	}*/

	// working vars for this emitter
    if ( mPosRelative )
        mDir = FVector(1,0,0);        
    else
	    mDir = Rotation.Vector();

	mRegenBias		= 1.0f;
	mAtLeastOneFrame = 1;

	mbSpinningNodes = ( mSpinRange[RNG_LO] != mSpinRange[RNG_HI] );

	mTexU = 1.0f / (float)mNumTileColumns;
	mTexV = 1.0f / (float)mNumTileRows;

	// clamp ranges
	mMaxParticles = Clamp( mMaxParticles, 0, MAX_PARTICLES_PER );
	mStartParticles = Clamp( mStartParticles, 0, mMaxParticles );

	mTotalTiles = (float)( mNumTileColumns * mNumTileRows ) - 1.0f;
	mInvTileCols = 1.0f / mNumTileColumns;

	// set iterator class
	mpIterator = NULL;
	switch (mParticleType)
	{
	case PT_Sprite:
		*((ParticleIterator**)&mpIterator) = (ParticleIterator*)&sSpriteIterator;
		break;
	case PT_Stream:
		*((ParticleIterator**)&mpIterator) = (ParticleIterator*)&sStreamIterator;
		break;
	case PT_Line:
		*((ParticleIterator**)&mpIterator) = (ParticleIterator*)&sLineIterator;
		break;
	case PT_Disc:
		*((ParticleIterator**)&mpIterator) = (ParticleIterator*)&sDiscIterator;
		break;
	case PT_Mesh:
		*((ParticleIterator**)&mpIterator) = (ParticleIterator*)&sMeshIterator;
 		break;
	case PT_Branch:
		*((ParticleIterator**)&mpIterator) = (ParticleIterator*)&sBranchIterator;
 		break;
	case PT_Beam:
		*((ParticleIterator**)&mpIterator) = (ParticleIterator*)&sBeamIterator;
		break;
	default:
		*((ParticleIterator**)&mpIterator) = (ParticleIterator*)&sSpriteIterator;
		break;
    }

	unguard;
}

UBOOL AxEmitter::Tick( FLOAT deltaTime, ELevelTick TickType )
{
	guard(AxEmitter::Tick);

	if ( Level->NetMode == NM_DedicatedServer )
	{
		if ( bNotOnDedServer && (RemoteRole == ROLE_None) )
		{
				bNoDelete = false;
				GetLevel()->DestroyActor( this );
				return true;
		}
        return( Super::Tick( deltaTime, TickType ) );
	}

	//debugf(TEXT("AxEmitter::Update"));
    if( mParticleType == PT_Beam ) // fixed shockbeam, but can't this be done elsewhere?
    {
        mBounds.IsValid = 0;
	    mBounds += Location;
	    mBounds += mSpawnVecA;
    }

#if UNSEEN_SUSPEND
    if( mRegen == 0 && LifeSpan == 0.0f ) // mRegen has been set to false during play!
    {
        LifeSpan = Max(mLifeRange[0], mLifeRange[1]);
        //debugf(TEXT("%s had LifeSpan imposed!"), GetName() );
    }
#endif

	if ( Expire==1 || bDeleteMe==1 || (blockOnNet && mSpawnVecA.SizeSquared()==0.0f) ) // just waiting for this to expire
    {
        if( Expire )
        {
            GetLevel()->DestroyActor( this );
            return 1;
        }
        else
		    return Super::Tick( deltaTime, TickType );
    }

    if ( mMaxParticles < 0 )
    {
        //debugf(TEXT("Delayed Init [%f,%f,%f]"), mSpawnVecA.X, mSpawnVecA.Y, mSpawnVecA.Z);
        mMaxParticles = -mMaxParticles;
        Initialize();
        blockOnNet = 0;
        return Super::Tick( deltaTime, TickType );
    }

    // update emitter stats
	if ( bStasis || (TickType == LEVELTICK_ViewportsOnly && !GIsEditor) )
		return 1;

    // don't tick constant-type particle effects - add bSuspendWhenNotVisible??
	if ( bSuspendWhenNotVisible && ((Level->TimeSeconds - LastRenderTime) > 0.5f && mNumUpdates > 10) )
    {
        if ( (mRegenOnTime[1] == 0.0f) && (mRegen == 1) && (mRegenRange[0] > 0.0) )
        {
            return Super::Tick( deltaTime, TickType );
        }
    }

#if UNSEEN_SUSPEND
    if( bSuspendWhenNotVisible && (mRegen == 0 && (LastRenderTime < Level->TimeSeconds)) )
    {
        // save last update time
	    mTime += deltaTime;
        // update bbox estimate
        if( mLastPos != Location )
        {
            mBounds += Location;
            mLastPos = Location;
        }
        return Super::Tick( deltaTime, TickType );
    }
#endif

	// calc delta time!
    mT = Clamp(deltaTime, 0.0f, 0.3f);
	if ( mNumUpdates == 0 && mAtLeastOneFrame )
	{
		mNumUpdates++;
	}
	else if ( Level->NetMode!=NM_DedicatedServer ) // this condition is safe if particles are simulated properly
	{
        if ( mPosRelative )
            mDir = FVector(1,0,0);        
        else
            mDir = Rotation.Vector();

        INT SetupStartTime = appCycles();
		if ( mpIterator )
		{
			((ParticleIterator*)mpIterator)->Update(this);
            UpdateXEmitterCollision(this);
		}
        GStats.DWORDStats( GEngineStats.STATS_Particle_SpriteSetupCycles ) += appCycles() - SetupStartTime;
		mNumUpdates++;
	}

	// regen timing
	if ( mRegenOnTime[1] > 0.0f )
	{
		mPauseTimer -= mT;
		if ( mPauseTimer < 0.0f )
		{
            if ( mRegenOffTime[1]==0.0f && mPauseTimer==1 ) // if off-time is zero, never switch on unless triggered
            {
                if( mRegenPause==1 )
                {
                    mPauseTimer = 0.0f; // currently off, don't unpause unless triggered
                }
                else if( mRegenPause==0 ) // set unpaused by trigger event
                {
                    mPauseTimer = GetRandomRange( mRegenOnTime );
                }
            }
			else if ( mRegenPause==0 ) // is emitting, so turn it off
			{
				mPauseTimer = GetRandomRange( mRegenOffTime );
				mRegenPause = 1;
			}
			else // switch it on!
			{
				mPauseTimer = GetRandomRange( mRegenOnTime );
				mRegenPause = 0;
			}
		}
	}

	return Super::Tick( deltaTime, TickType );
	unguard;
}

void AxEmitter::Render( FLevelSceneNode* Frame, FRenderInterface* RI )
{
    guard(AxEmitter::Render);
    
    if ( bDeleteMe || blockOnNet )
        return;

    if( !GIsEditor && SourceStaticMesh )
        GetLevel()->FarMoveActor( this, Frame->ViewOrigin);

    LastRenderTime = GetLevel()->TimeSeconds;
	ParticleIterator* pPclIterator = (ParticleIterator*)mpIterator;

#if UNSEEN_SUSPEND
    if( mRegen == 0 && mpIterator && mTime > 0.0f )
    {
        INT SetupStartTime = appCycles();
        mT = mTime;//Clamp(deltaTime, 0.0f, 0.3f);
        if ( mPosRelative )
            mDir = FVector(1,0,0);        
        else
            mDir = Rotation.Vector();
	    if ( mpIterator )
	    {
		    ((ParticleIterator*)mpIterator)->Update(this); // !! may cause pEmitter to become bDeleteMe
            UpdateXEmitterCollision(this);
	    }
        GStats.DWORDStats( GEngineStats.STATS_Particle_SpriteSetupCycles ) += appCycles() - SetupStartTime;
	    mNumUpdates++;
        mTime = 0.0f;
    }
#endif

	if ( !Frame || !Frame->Viewport || !Frame->Viewport->RI || Frame->Viewport->IsOrtho() ||
          mRenderableVerts == 0 || !Skins.Num() || Skins(0)==NULL || pPclIterator==NULL || mpIterator==NULL || Cast<UMaterial>(Skins(0))==NULL )
	{
		return;
    }

    if( mpParticles == 0 )
    {
        debugf(TEXT("%s tried to draw without particles!"), GetName());
        return;
    }

	INT	RenderStartTime = appCycles();

	DECLARE_STATIC_UOBJECT( UFinalBlend, FinalBlend, {} );

	FinalBlend->Material	= Skins(0);
	FinalBlend->TwoSided	= 1;
	FinalBlend->ZWrite		= 0;
	FinalBlend->ZTest		= 1;
	FinalBlend->AlphaRef	= 0;	
	FinalBlend->AlphaTest	= 0;
	FinalBlend->FrameBufferBlending	= FB_Overwrite;

	UTexture* Texture = Cast<UTexture>(Skins(0));
	if( Texture && Texture->bAlphaTexture )
		FinalBlend->AlphaTest = 1;

    switch( Style )
	{
	case STY_Masked:
		FinalBlend->AlphaTest	= 1;
		FinalBlend->AlphaRef	= 127;
		break;
	case STY_Translucent:
		FinalBlend->FrameBufferBlending = FB_Translucent;
		break;
	case STY_Modulated:
		FinalBlend->FrameBufferBlending = FB_Modulate;
		break;
    case STY_AlphaZ:
        FinalBlend->ZWrite		= 1;
	case STY_Alpha:
		FinalBlend->FrameBufferBlending = FB_AlphaBlend;
		FinalBlend->AlphaTest	= 1;
		break;
	case STY_Additive:
		FinalBlend->FrameBufferBlending = FB_Brighten;
		FinalBlend->AlphaTest	= 1;
		break;
	case STY_Subtractive:
		FinalBlend->FrameBufferBlending = FB_Darken;
		break;
	}

    RI->EnableLighting(0,1,NULL,0);

    RI->SetMaterial( FinalBlend );

    RI->SetTransform(TT_WorldToCamera,FMatrix::Identity);
    RI->SetTransform(TT_LocalToWorld, FMatrix::Identity);

    pPclIterator->Draw( this, Frame );  

    // Reset the camera transform.
    RI->SetTransform(TT_WorldToCamera, Frame->WorldToCamera);

    GStats.DWORDStats( GEngineStats.STATS_Particle_Particles ) += mNumActivePcl;
    GStats.DWORDStats( GEngineStats.STATS_Particle_RenderCycles ) += (appCycles() - RenderStartTime);

#if 0
    if( mSpawningType == ST_StaticMesh && Frame->Viewport->IsWire() )
    {
        RI->SetTransform(TT_LocalToWorld, SourceStaticMesh->LocalToWorld() ); 
        cachedOctTree.Draw( Frame );
    }
#endif
    
    unguard;
}

void GetRandStaticMeshVertex( FVector& emitPos, AStaticMeshActor* sourceActor, FVector origin, float radius )
{
    if ( !sourceActor->StaticMesh )
        return;

    if ( radius == 0.0f )
        radius = 600.0f;

    float radiusSqr = radius * radius;

    // cache bboxes
    if ( cachedMesh != sourceActor->StaticMesh || cachedTriData.Num() == 0)
    {
        cachedMesh = sourceActor->StaticMesh;
        cachedMesh->RawTriangles.Load();
        cachedTriIndex.Empty();
        cachedTriData.Empty();
        cachedTriData.AddZeroed( cachedMesh->RawTriangles.Num() );
        cachedMaxArea = -99999.9f;
        int i;
        cachedOctTree.bounds = sourceActor->StaticMesh->BoundingBox;
        for( i=0; i<cachedMesh->RawTriangles.Num(); i++ )
        {
            FBox bbox(1);
            bbox += cachedMesh->RawTriangles(i).Vertices[0];
            bbox += cachedMesh->RawTriangles(i).Vertices[1];
            bbox += cachedMesh->RawTriangles(i).Vertices[2];
            float area = ((cachedMesh->RawTriangles(i).Vertices[0] - cachedMesh->RawTriangles(i).Vertices[1]) ^
                    (cachedMesh->RawTriangles(i).Vertices[2] - cachedMesh->RawTriangles(i).Vertices[1])).Size()*0.5f;

            cachedTriData(i).center = bbox.GetCenter();
            cachedTriData(i).radiusSqr = (cachedTriData(i).center - bbox.Max).SizeSquared();
            cachedTriData(i).wt = area;
            if ( area > cachedMaxArea )
                cachedMaxArea = area;

            cachedOctTree.AddItem( i, bbox );
        }
        for( i=0; i<cachedTriData.Num(); i++ )
        {
            cachedTriData(i).wt /= cachedMaxArea;
        }
        cachedMaxArea = 1.0f / cachedMaxArea;
    }

    
	clock(MeshPclInitCycles);
    // check for tri bounds that overlap radius, build list
    int prevCnt = cachedTriIndex.Num();
    if ( cachedOrigin != origin || cachedRadius != radius )
    {
        cachedOrigin = origin;
        cachedRadius = radius;
        cachedTriIndex.Empty(cachedTriIndex.Num());
        origin -= sourceActor->Location;

        FVector extent(radius,radius,radius);
        TArray<OctNode<INT>*> nodeList;
        cachedOctTree.ExtentMark( origin, extent, nodeList );

        for( int i=0; i<nodeList.Num(); i++ )
        {
            OctNode<INT>* pNode = nodeList(i);
            for( int j=0; j<pNode->contents.Num(); j++ )
            {
                int triIdx = pNode->contents(j);
                if( (cachedTriData(triIdx).center - origin).SizeSquared() < radiusSqr+cachedTriData(triIdx).radiusSqr)
                    cachedTriIndex.AddItem(pNode->contents(j));
            }
        }
    }
    if ( prevCnt == 0 )
        return;

    if ( cachedTriIndex.Num()==0 )
        cachedTriIndex.Add( prevCnt );

    float f[3];
	f[0] = 1.f - qFRand();
	f[1] = (1.f - f[0]) * qFRand();
	f[2] = (1.f - f[0] - f[1]);
    
    int rndIndex = qRand() % cachedTriIndex.Num();
    for( int i=0; i<10; i++ )
    {
        if( cachedTriData(cachedTriIndex(rndIndex)).wt < qFRand() ) // small weight(area) tris are less likely to have puffs
        {
            rndIndex = qRand() % cachedTriIndex.Num();
            continue;
        }
        break;
    }
    
    FVector rndPos =    (cachedMesh->RawTriangles(cachedTriIndex(rndIndex)).Vertices[0] * f[0])
                        + (cachedMesh->RawTriangles(cachedTriIndex(rndIndex)).Vertices[1] * f[1]) 
                        + (cachedMesh->RawTriangles(cachedTriIndex(rndIndex)).Vertices[2] * f[2]);

    emitPos = sourceActor->Location + rndPos;

    unclock(MeshPclInitCycles);
}

IMPLEMENT_CLASS(AxEmitter);

