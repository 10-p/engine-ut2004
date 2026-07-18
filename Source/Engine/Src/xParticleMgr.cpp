//=============================================================================
// xParticleMgr.cpp Particle Iterator implementations
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================
#include "EnginePrivate.h"
#include "xParticleMgr.h"
#include "UnRender.h"

#include <math.h>
#include <stdlib.h>

float GParticleAtten = 1.0f;

// arrays for doing faster stuff - could be a static member
INT			pcl_index[MAX_PARTICLES_PER];
//static TArray<AxWind*> windActors; // cache of wind actors affecting the current emitter

extern void SpawnBranchParticles( AxEmitter* pEmitter, int* freePclIdx, int numFreePcl );
extern BranchIterator	sBranchIterator;

// static helpers
#define	Randf qFRand

// note the ptr references
static inline void SetupTextureSource( UTexture* pTex, BYTE*& pBitmap, FColor*& pColors )
{
	if ( !pTex || (pTex->Format != TEXF_P8) )
	{
    	if ( pTex && (pTex->Format != TEXF_P8) )
            debugf( NAME_Warning, TEXT("LifeMap %s not in P8 format!"), pTex->GetName() );
        
		pBitmap = NULL;
		pColors = NULL;
		return;
	}
    pTex->Mips(0).DataArray.Load();
	pBitmap = &pTex->Mips(0).DataArray(0);
	pColors = &pTex->Palette->Colors(0);
}

static inline void UnlockTextureSource( UTexture* pTex )
{
    // gam -- Well I guess it all Depends (Undergarments).
}

static inline void FetchTextureColor( FPlane& r, UTexture* pTex, float u, float v, BYTE*& pBitmap, FColor*& pColors )
{
	int SrcXIndex = Clamp( (int)(u * pTex->USize), 0, pTex->USize-1 );
	int SrcYIndex = Clamp( (int)(v * pTex->VSize), 0, pTex->VSize-1 );
	BYTE b = (BYTE)pBitmap[ SrcXIndex + SrcYIndex * pTex->USize ];
	r = (*(FColor*)(&pColors[ b ])).Plane();
	//Exchange( r.X, r.Z );
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

static inline float SmoothStep( float x )
{
	return ( x * x * (3.0f - 2.0f * x) );
}

static inline float GetRandomRange( float rng[2] )
{
	return (rng[RNG_LO] + ((rng[RNG_HI] - rng[RNG_LO] ) * Randf())); 
}

static inline float SmoothStepRange( float a, float b, float x )
{
	if ( x < a && a > 0.01f)
		return x / a;
	else if ( x > b && b < 0.99f)
		return 1.0f - (x - b) / (1.0f - b);
    else
        return 1.0f;
}

static inline float Pulse( float x )
{
	if ( x < .5f )
	{
		return 2.0f * ( x * x * (3.0f - 2.0f * x) );
	}
	else
	{
		return 2.0f * (1.0f - ( x * x * (3.0f - 2.0f * x) ));
	}
}

static inline DWORD MakeColor( FPlane& p )
{
	DWORD d;
	if( GIsOpenGL )
	{
#if __INTEL_BYTE_ORDER__
		d  = (int)(p.X * 255.0f);
		d |= (int)(p.Y * 255.0f) << 8;
		d |= (int)(p.Z * 255.0f) << 16;
		d |= (int)(p.W * 255.0f) << 24;
#else
		#if MACOSX
		if( GIsOpenGLRevComps )
		{
			d  = (int)(p.X * 255.0f);
			d |= (int)(p.Y * 255.0f) << 8;
			d |= (int)(p.Z * 255.0f) << 16;
			d |= (int)(p.W * 255.0f) << 24;
		}
		else
		#endif
		{
			d  = (int)(p.X * 255.0f) << 24;
			d |= (int)(p.Y * 255.0f) << 16;
			d |= (int)(p.Z * 255.0f) << 8;
			d |= (int)(p.W * 255.0f);
		}
#endif
	}
	else
	{
		d  = (int)(p.Z * 255.0f);
		d |= (int)(p.Y * 255.0f) << 8;
		d |= (int)(p.X * 255.0f) << 16;
		d |= (int)(p.W * 255.0f) << 24;
	}
	return d;
}

static inline FPlane ColorPlane( FColor& c )
{
	return FPlane( c.R*INV_255, c.G*INV_255, c.B*INV_255, c.A*INV_255 );
}

static inline void CalcParticleUV( ParticleT* p, FPlane& c, FPlane& c2, float& atten, 
								float& u, float& v, AxEmitter* pEmitter, int& blendAnim,
								float& nextRow, float& nextCol, float& texU, float& texV )
{
	blendAnim = 0;   
    if ( pEmitter->mTileAnimation )
    {
        float fTile = pEmitter->mTotalTiles * ( 1.0f - (p->life * p->invLifeSpan) );
        p->tileNum = (BYTE)fTile;
        int col = p->tileNum / pEmitter->mNumTileColumns;
        int row = p->tileNum - (pEmitter->mNumTileRows * col);
        u = (float)row * texU;
        v = (float)col * texV;
        if ( 1 )
        {
            float nextTile = appCeil( fTile );
            float nextBlend = nextTile - fTile;
            col = (int)nextTile / pEmitter->mNumTileColumns;
            row = (int)nextTile - (pEmitter->mNumTileRows * col);
            nextRow = col;
            nextCol = row;
            blendAnim = 1;
            c = p->color.Plane() * ( nextBlend ) * atten;
            c2 = p->color.Plane() * (1.0f - nextBlend) * atten;
        }
    }
    else
    {
        int col = p->tileNum / pEmitter->mNumTileColumns;
        int row = p->tileNum - (pEmitter->mNumTileRows * col);
        u = (float)row * texU;
        v = (float)col * texV;
    }
}

static inline void CalcAttenColor( ParticleT* p, AxEmitter* pEmitter, float& atten, FPlane& clr, FPlane& modClr )
{
	if ( !pEmitter->mAttenuate || pEmitter->mAttenFunc == ATF_None ) // temp
	{
		pEmitter->mAttenFunc = ATF_None;
	}
	switch (pEmitter->mAttenFunc )
	{
	case ATF_LerpInOut:
		atten = LerpInOut( pEmitter->mAttenKa, 1.0f - (p->life * p->invLifeSpan));
		break;
	case ATF_ExpInOut:
		atten = LerpInOut( pEmitter->mAttenKa, pEmitter->mAttenKb - (p->life * p->invLifeSpan * p->life * p->invLifeSpan));
		break;
	case ATF_SmoothStep:
		atten = SmoothStepRange( pEmitter->mAttenKa, pEmitter->mAttenKb, 1.0f - p->life * p->invLifeSpan );
		break;
	case ATF_Pulse:
		atten = Pulse( p->life * p->invLifeSpan );
		break;
	case ATF_Random:
		atten = pEmitter->mAttenKa + (Randf() * (pEmitter->mAttenKb - pEmitter->mAttenKa ));
		break;
	case ATF_None:
		atten = 1.0f;
		break;
	default:
		atten = 1.0f;
		break;
	}

	if ( pEmitter->Style == STY_Modulated )
	{
		clr.X = clr.Y = clr.Z = 1.0f;
		clr.W = atten;
	}
	else if ( pEmitter->Style == STY_Alpha )
	{
		clr.X *= modClr.X;
		clr.Y *= modClr.Y;
		clr.Z *= modClr.Z;
		clr.W = atten;
	}
	else
	{
		clr *= modClr * Clamp((atten * pEmitter->ScaleGlow), 0.0f, 1.0f); // FIXED - trans-trail rainbow
    }
}

static inline void QuadSprite( DynamicVertex*& pOutVerts, FVector& center, FVector& uaxis, FVector& vaxis, DWORD dwClr, float uv[2][2] )
{
	pOutVerts->U = uv[1][0];
	pOutVerts->V = uv[1][1];
	pOutVerts->Color = dwClr;
	pOutVerts->Point = center - vaxis;
	pOutVerts++;

	pOutVerts->U = uv[1][0];
	pOutVerts->V = uv[0][1];
	pOutVerts->Color = dwClr;
	pOutVerts->Point =center + uaxis;
	pOutVerts++;

	pOutVerts->U = uv[0][0];
	pOutVerts->V = uv[0][1];
	pOutVerts->Color = dwClr;
	pOutVerts->Point = center + vaxis;
	pOutVerts++;

	pOutVerts->U = uv[0][0];
	pOutVerts->V = uv[1][1];
	pOutVerts->Color = dwClr;
	pOutVerts->Point = center - uaxis;
	pOutVerts++;
}

static inline void QuadLine( DynamicVertex*& pOutVerts, FVector& a, FVector& b, FVector& axis, DWORD clr, float uv[2][2] )
{            
	pOutVerts->U = uv[1][0];
	pOutVerts->V = uv[1][1];
	pOutVerts->Color = clr;
	pOutVerts->Point = a - axis;
	pOutVerts++;

	pOutVerts->U = uv[1][0];
	pOutVerts->V = uv[0][1];
	pOutVerts->Color = clr;
	pOutVerts->Point = a + axis;
	pOutVerts++;

	pOutVerts->U = uv[0][0];
	pOutVerts->V = uv[0][1];
	pOutVerts->Color = clr;
	pOutVerts->Point = b + axis;
	pOutVerts++;

	pOutVerts->U = uv[0][0];
	pOutVerts->V = uv[1][1];
	pOutVerts->Color = clr;
	pOutVerts->Point = b - axis;
	pOutVerts++;
}

class FQuadStream : public FVertexStream
{
public:
    AxEmitter*  pEmitter;
    int         numVerts;
	QWORD		CacheId;
    int         Revision;
    BYTE*       LifeSrcBits;
    FColor*     LifePalette;
    float       curLife;
    float       maxLife;
    int         numQuads;
    FMatrix     WorldToCamera;
    // working vars
    float       texU, texV;
    FVector center;
    float atten;
    FVector uaxis, vaxis;
    float u, v;
    float size;
    FPlane clr;
    FPlane c2;
    int blend_anim;
    float nextRow;
    float nextCol;
    DWORD dwClr;
    ParticleT* p;
    DynamicVertex* pOutVerts;

    void Init( AxEmitter* inEmitter, int renderVerts, FMatrix& inWorldToCamera )
    {
        pEmitter = inEmitter;
        numVerts = renderVerts;
        if( pEmitter->mParticleType == PT_Sprite && pEmitter->mSpawnVecB.X > 0.0f )
        {
            numVerts += 4;
        }
        curLife = pEmitter->LifeSpan;
	    maxLife = pEmitter->GetClass()->GetDefaultActor()->LifeSpan;
        LifeSrcBits = NULL;
	    LifePalette = NULL;
        numQuads = 0;
	    SetupTextureSource( pEmitter->mLifeColorMap, LifeSrcBits, LifePalette );
        WorldToCamera = inWorldToCamera;
        texU = pEmitter->mTexU;
        texV = pEmitter->mTexV;
        pEmitter->ScaleGlow = Clamp(pEmitter->ScaleGlow, 0.0f, 1.0f);
        Revision++;
    }

	FQuadStream()
	{
		CacheId = MakeCacheID(CID_RenderVertices);
        pEmitter = NULL;
        LifeSrcBits = NULL;
        numVerts = 0;
        Revision = 1;    
        numQuads = 0;
        pOutVerts = NULL;
        p = NULL;
    }

	virtual QWORD GetCacheId()
	{
		return CacheId;
	}

	virtual INT GetRevision()
	{
		return Revision;
	}

	virtual INT GetSize()
	{
		return numVerts * sizeof(DynamicVertex);
	}

	virtual INT GetStride()
	{
		return sizeof(DynamicVertex);
	}

	virtual INT GetComponents(FVertexComponent* OutComponents)
	{
		OutComponents[0].Type = CT_Float3;
		OutComponents[0].Function = FVF_Position;
        OutComponents[1].Type = CT_Color;
		OutComponents[1].Function = FVF_Diffuse;
        OutComponents[2].Type = CT_Float2;
		OutComponents[2].Function = FVF_TexCoord0;
        return 3;
	}

    virtual void GetStreamData(void* Dest)
    {
        p = &(*(PclArrayPtr)pEmitter->mpParticles)(0);
	    pOutVerts = (DynamicVertex*)Dest;
        numQuads = 0;
        switch( pEmitter->mParticleType )
        {
        case PT_Sprite:
            GetSpriteData();
            break;
        case PT_Line:
            GetLineData();
            break;
        default:
            break;
        }
        Done();
    }

    void Done(void)
    {
        if( pEmitter->mLifeColorMap )
            UnlockTextureSource( pEmitter->mLifeColorMap );
    }

    void GetLineData(void)
    {
        size *= 0.5f;
        FVector a, b;
        float lineLength = pEmitter->mSpawnVecB.Z;
        for ( int n=0; n<pEmitter->mMaxParticles; n++, p++ )
	    {
		    if ( /* *(int*)&*/ p->life == 0.f )
			    continue;

            if( /* *(int*)&*/ p->delay != 0.f )
                continue;

		    size = pEmitter->DrawScale * p->size;
		    a = p->pos;
		    b = p->pos - p->velocity * lineLength;
		    if ( pEmitter->mPosRelative )
		    {
			    a += pEmitter->Location;
			    b += pEmitter->Location;
		    }

            // get lifemap color if exists
		    FPlane colorPlane(1,1,1,1);
		    if ( LifeSrcBits )
		    {
			    FetchTextureColor( colorPlane, pEmitter->mLifeColorMap, 0.5f, p->life * p->invLifeSpan,
								    LifeSrcBits, LifePalette );
		    }

		    // attenuate
		    atten = 1.0f;
		    clr = ColorPlane( p->color );
		    CalcAttenColor( p, pEmitter, atten, clr, colorPlane );

		    // tile-based tex coord gen w/blending
		    CalcParticleUV( p, clr, c2, atten, u, v, pEmitter, blend_anim, nextRow, nextCol, texU, texV );
            dwClr = MakeColor(clr);
            
            uaxis = FVector(size,0,0);
            vaxis = FVector(0,size,0);
            a = WorldToCamera.TransformFVector( a );
            b = WorldToCamera.TransformFVector( b );
            float lineuv[2][2] = { {0.05f, 0.01f} , {0.95f, 0.49f} };

            QuadLine( pOutVerts, a, b, uaxis, dwClr, lineuv );
            QuadLine( pOutVerts, a, b, vaxis, dwClr, lineuv );

            // draw head
            uaxis = FVector(pEmitter->mSpawnVecB.X,0,0);
            vaxis = FVector(0,pEmitter->mSpawnVecB.X,0);
            float uv[2][2] = { { 0.0f, 0.5f} , {1.0f, 1.0f} };
            QuadSprite( pOutVerts, a, uaxis, vaxis, dwClr, uv );
            numQuads += 3;
	    }
    }

	void GetSpriteData(void)
	{
        for ( int n=0; n<pEmitter->mMaxParticles; n++, p++ )
	    {
/*
		    if ( *(int*)&p->life == 0 )
			    continue;

            if( *(int*)&p->delay > 0 )
                continue;
*/
		    if ( /* *(int*)&*/ p->life == 0.f )
			    continue;

            if( /* *(int*)&*/ p->delay > 0.f )
                continue;

            center = p->pos;

		    if ( pEmitter->mPosRelative )
			    center = (GMath.UnitCoords / p->pos / pEmitter->Rotation).Origin + pEmitter->Location;                

		    center = WorldToCamera.TransformFVector(center);
            size = pEmitter->DrawScale * p->size;

            uaxis.X = size;
            uaxis.Y = 0.0f;
            vaxis.X = 0.0f;
            vaxis.Y = size;

            if ( p->rot.Roll != 0 )
		    {
                uaxis.X = GMath.CosTab(p->rot.Roll) * size;
                uaxis.Y = GMath.SinTab(p->rot.Roll) * size;
                vaxis.X = -uaxis.Y;
                vaxis.Y = uaxis.X;
            }

		    // get lifemap color if exists
		    FPlane colorPlane(1,1,1,1);
		    if ( LifeSrcBits )
		    {
			    FetchTextureColor( colorPlane, pEmitter->mLifeColorMap, curLife / maxLife, p->life * p->invLifeSpan,
								    LifeSrcBits, LifePalette );
		    }

		    // attenuate
		    atten = 1.0f;
		    clr = ColorPlane( p->color );
		    CalcAttenColor( p, pEmitter, atten, clr, colorPlane );

		    // tile-based tex coord gen w/blending
		    CalcParticleUV( p, clr, c2, atten, u, v, pEmitter, blend_anim, nextRow, nextCol, texU, texV );
            float uv[2][2] = { { u, v} , {u+texU, v+texV} };
            DWORD dwClr = MakeColor(clr);
            QuadSprite(pOutVerts,center,uaxis,vaxis,dwClr,uv);
			numQuads++;
	    }

        // draw sprite at emitter origin
        if( pEmitter->mSpawnVecB.X )
        {
            uaxis.X = pEmitter->mSpawnVecB.X;
            uaxis.Y = 0.0f;
		    vaxis.X = 0.0f;
            vaxis.Y = pEmitter->mSpawnVecB.X;
            float uv[2][2] = { { 0.0f, 0.0f} , {texU, texV} };
            DWORD dwClr = 0xffffffff;
            center = WorldToCamera.TransformFVector(pEmitter->Location);
            QuadSprite(pOutVerts,center,uaxis,vaxis,dwClr,uv);
            numQuads++;
        }
	}

	virtual void GetRawStreamData(void ** Dest, INT FirstVertex )
	{
		*Dest = NULL;
	}
};

FQuadStream quadStream;

bool CalcForceData( ForceData& forceData, AActor* forceActor )
{
    if( !forceActor || forceActor->ForceType == FT_None || forceActor->ForceRadius == 0.0f || forceActor->ForceScale == 0.0f )
        return false;
    if( forceActor->ForceType == FT_DragAlong && forceActor->Velocity.SizeSquared() < 10.0f )
        return false;

    forceData.forceOrigin = forceActor->Location;
    forceData.invForceRadius = 1.0f / forceActor->ForceRadius;
    forceData.forceRadiusSqr = Square(forceActor->ForceRadius);
    forceData.forceScale = forceActor->ForceScale;
    forceData.forceVelocity = Max(1.0f,forceActor->Velocity.Size());
    forceData.forceType = forceActor->ForceType;
    forceData.velocityNormal = forceActor->Velocity * (1.0f / forceData.forceVelocity);
    // modify scale by noise
    forceData.forceScale *= (1.0f - forceActor->ForceNoise) + ( qFRand() * forceActor->ForceNoise );
    // atten by lifespan
    if( forceActor->LifeSpan > 0.0f && forceActor->LifeSpan < 1.0f )
    {
        forceData.forceScale *= forceActor->LifeSpan;
    }
    return true;
}

void CalcForceAtPoint( ForceData* forces, int numForces, FVector& pt, FVector& accumForce )
{
    for ( int i=0; i<numForces; i++ )
    {
        FVector vToPoint = pt - forces[i].forceOrigin;
        float dist = vToPoint.SizeSquared();
        if ( dist > forces[i].forceRadiusSqr )
		    continue;
	    if ( dist > 0.01f )
	    {
		    dist = appSqrt( dist );
		    vToPoint *= 1.0f / dist; // normalize
	    }
	    else
	    {
		    dist = 0.01f;
		    vToPoint = VRand();
	    }
        float effective_force = forces[i].forceScale * (1.0f - (dist * forces[i].invForceRadius));
        if( forces[i].forceType == FT_DragAlong )
        {
            effective_force *= (forces[i].velocityNormal|vToPoint) > 0.0f ? 1.0f : -1.0f;
        }
        effective_force *= forces[i].forceVelocity;
		accumForce += vToPoint * effective_force;
    }
}

// !!! FIXME: rcg06022002 Need this header anymore on any platform?
#if (!defined __GNUC__)
#include <xmmintrin.h>
#endif

void ParticleIterator::Update( AxEmitter* pEmitter )
{
	guard(ParticleIterator::Update);

	// put the box into a sphere for sphere required tests
	pEmitter->mSphere = pEmitter->mBounds.Min + 0.5f * ( pEmitter->mBounds.Max - pEmitter->mBounds.Min );
	pEmitter->mSphere.W = (pEmitter->mSphere - pEmitter->mBounds.Min).Size();

	//ULevel* pLevel = pEmitter->GetLevel();
	float T = pEmitter->mT;
	float fT = Clamp( T, 0.0f, 0.05f ); // force clamp

	// determine gravity effect
	FVector grav(0.0,0.0,0.0f);
	if ( pEmitter->PhysicsVolume )
		grav = pEmitter->PhysicsVolume->Gravity * 0.35f * T; // gam

	// check for potential collision with the system's bounds
	UBOOL bPotentialCollision = 0;
	if ( pEmitter->mCollision )
	{
		FCheckResult Hit(1.0f);
		if( pEmitter->GetLevel()->SinglePointCheck( Hit, pEmitter->mSphere, FVector(1,1,1)*pEmitter->mSphere.W, 0, pEmitter->GetLevel()->GetLevelInfo(), 1 )==0 )
		{
			bPotentialCollision = 1;
			//debugf(TEXT("Potential Collision for: %s"), pEmitter->GetName() );
		}
	}

    // build list of forces affecting this emitter, precalc some shit
    ForceData forces[8];
    int numForces = 0;
    APhysicsVolume* volForce = NULL;
    FVector volVector;

    if ( pEmitter->IsForceAffected() )
    {
        if ( pEmitter->PhysicsVolume && pEmitter->PhysicsVolume->ForceType != FT_None && pEmitter->PhysicsVolume->ForceScale != 0.0f )
        {
            volForce = pEmitter->PhysicsVolume;
            volVector = volForce->Rotation.Vector();
        }
        for( int i=0; i<pEmitter->Touching.Num(); i++ )
        {
            if( CalcForceData(forces[numForces], pEmitter->Touching(i)) )
            {
                numForces++;
                if( numForces > ARRAY_COUNT(forces)-1 )
                    break;
            }
        }
    }

	// update existing particles
	ParticleT*	p			= &(*(PclArrayPtr)pEmitter->mpParticles)(0);
	FVector		vec_between;
	pEmitter->mBounds.IsValid = 0;
	pEmitter->mBounds += pEmitter->Location;
	pEmitter->mRenderableVerts = 0;
    int numfree = 0;
	float growth = pEmitter->mGrowthRate * T;
	float airResist = 1.0f - (T * pEmitter->mAirResistance);
	int vertexCount = ((PclIteratorPtr)pEmitter->mpIterator)->GetVertexCount( pEmitter, p );

	for ( int n=0; n<pEmitter->mMaxParticles; n++,p++ )
	{
		//if ( !(*(int*)&p->life) )
		if ( /* *(int*)&*/ p->life == 0.f )
		{
			pcl_index[numfree++] = n;
			continue;
		}

		//if ( (*(int*)&p->delay) )
        if( /* *(int*)&*/ p->delay > 0.f )
		{
			p->delay -= T;
			if ( p->delay <= 0.0f )
			{
				pEmitter->mRenderableVerts += vertexCount;
				p->delay = 0.0f;
			}
			continue;
		}

		// move the particle and reduce life
		p->size += growth;
		if ( T >= p->life ||  p->size <= 0.0f )
		{
			p->life = PCL_DEAD;
            p->nextIndex = INDEX_NONE;
            if ( pEmitter->mHeadIndex == n )
                pEmitter->mHeadIndex = p->nextIndex;
            pcl_index[numfree++] = n;
			pEmitter->mNumActivePcl--;
			continue;
		}

		p->life -= T;

		p->velocity.X += grav.X * p->mass;
		p->velocity.Y += grav.Y * p->mass;
		p->velocity.Z += grav.Z * p->mass;

        // Determine force effects using new Force properties in actor
		if ( pEmitter->IsForceAffected() && p->mass && (numForces||volForce) )
		{
            FVector accumForce = FVector( 0,0,0 );
            float factor = Abs(1.0f/(p->mass-1.0f));
			if ( volForce )
			{
                // Volume Force
				accumForce += volVector * volForce->ForceScale * (1.0f - volForce->ForceNoise + ( qFRand() * volForce->ForceNoise )); // scale down the strength because there is no distance atten
                float lifeT = 1.0f - (p->life * p->invLifeSpan);
                accumForce *= Square(lifeT);
			}
            CalcForceAtPoint(forces, numForces, p->pos, accumForce );
            // lerp to accumForce velocity
            accumForce *= factor;
            p->velocity = Lerp( p->velocity, accumForce, fT );
		}

		FVector lastPos = p->pos;
		p->pos += p->velocity * T;
		p->velocity *= airResist;
		
		if ( p->spin )
			p->rot.Roll += p->spin * T;
        
#if 0
		if ( bPotentialCollision )
		{
			FCheckResult Hit(1.0);
			if ( !pLevel->SingleLineCheck( Hit, pEmitter, p->pos, lastPos, TRACE_AllColliding, FVector(0,0,0) )) // sjs test FVector(1,1,1)*p->size ) ) (gam)
			{
				p->velocity = p->velocity.MirrorByVector( Hit.Normal );
				p->pos = Hit.Location + Hit.Normal * p->size;
				p->velocity *= pEmitter->mColElasticity;
                // amb ---
                if (pEmitter->mColMakeSound)
                {
                    pEmitter->eventCollisionSound();
                }
                // --- amb
			}
			if ( p->velocity.SizeSquared() < 0.25f )
				p->velocity = FVector(0,0,0);
		}
#endif
		if ( !pEmitter->mPosRelative )
		{
			pEmitter->mBounds += p->pos;
			pEmitter->mBounds += lastPos;
		}
		else
		{
			pEmitter->mBounds += pEmitter->Location + p->pos;
			pEmitter->mBounds += pEmitter->Location + lastPos;
		}

		pEmitter->mRenderableVerts += vertexCount;
	}

	if ( pEmitter->mMeshNodes[0] )
		pEmitter->mBounds += pEmitter->mMeshNodes[0]->BoundingBox;


	//GLog->Logf(TEXT("Num active: %i Total nodes free: %i"), pEmitter->numActive, pEmitter->msFreeNodeCount );
	float maxSize = pEmitter->mSizeRange[RNG_HI];
	if ( pEmitter->mGrowthRate > 0.0f )
		maxSize += pEmitter->mGrowthRate * pEmitter->mLifeRange[RNG_HI];
	pEmitter->mBounds = pEmitter->mBounds.ExpandBy( maxSize ); // drawscale?

    if ( pEmitter->mParticleType == PT_Stream )
    {
        p = &(*(PclArrayPtr)pEmitter->mpParticles)(0);
        for ( INT n=0; n<pEmitter->mMaxParticles; n++, p++ ) // gam
	    {
            // correct linkage - gross
            if ( p->life != PCL_DEAD && p->nextIndex != INDEX_NONE )
            {
                if ( (*(PclArrayPtr)pEmitter->mpParticles)(p->nextIndex).life == PCL_DEAD )
                {
                    p->nextIndex = INDEX_NONE;
                }
            }
        }
    }
	
	
	// SPAWNING!!!
	FVector		saved_pos(0.0,0.0,0.0);
	FVector		spawn_pos(0.0,0.0,0.0);
	float		pos_delta=0.0f;
	UBOOL		interp_spawns=0;
	FVector		v_norm(0.0,0.0,0.0);
	float		vlen = 0.0;
	if ( pEmitter->Location != pEmitter->mLastPos )
	{
		v_norm = pEmitter->Location - pEmitter->mLastPos;
		vlen = v_norm.Size();
		v_norm.Normalize();
	}
	if ( vlen >= pEmitter->mRegenDist && (pEmitter->mRegenPause==0) )
		pEmitter->mRegenTimer += T * GetRandomRange( pEmitter->mRegenRange ) * GParticleAtten * pEmitter->mRegenBias;
	
	//debugf(TEXT("pEmitter %x mRegenTimer %f"), pEmitter, pEmitter->mRegenTimer );

	if ( vlen > 0.0f )
		interp_spawns = 1;

	// determine deficit and reset the regen timer
	int num_to_spawn = 0;
	if ( (pEmitter->mRegenTimer > 1.0f) && (pEmitter->mRegen==1) && (pEmitter->mRegenPause==0) )
	{
		num_to_spawn = (int)pEmitter->mRegenTimer;
		pEmitter->mRegenTimer -= (float)num_to_spawn;
	}

	// add burst number to spawn count
	num_to_spawn += pEmitter->mStartParticles;
	pEmitter->mStartParticles = 0;

	// clamp within max particles
	if ( pEmitter->mNumActivePcl + num_to_spawn >= pEmitter->mMaxParticles )
        num_to_spawn = pEmitter->mMaxParticles - pEmitter->mNumActivePcl;

	if ( num_to_spawn > 0)
	{
        //debugf(TEXT("spawning %x %d"), pEmitter, num_to_spawn);

		// grab needed particles
		num_to_spawn = Min( numfree, num_to_spawn ); // num_to_spawn cannot be more than free ones found
		if ( interp_spawns )
		{
			saved_pos = pEmitter->Location;
			spawn_pos = pEmitter->mLastPos;
			pos_delta = vlen / (float)num_to_spawn;
		}

		if ( pEmitter->mpIterator == (PTRINT)&sBranchIterator ) // hackish
		{
			SpawnBranchParticles( pEmitter, pcl_index, num_to_spawn );
		}
		else
		{
			for (int i=0; i<num_to_spawn; i++)
			{
				// spawn the particles along the path the emitter took since last uppdate
				if ( interp_spawns )
				{
					spawn_pos += v_norm * pos_delta;
					pEmitter->Location = spawn_pos;
				}

				pEmitter->InitParticle( pcl_index[i] );

				if ( (*(PclArrayPtr)pEmitter->mpParticles)(pcl_index[i]).delay <= 0.0f )
					pEmitter->mRenderableVerts += vertexCount;
			}
		}

		if ( interp_spawns )
			pEmitter->Location = saved_pos;
	}
	pEmitter->mLastPos = pEmitter->Location; // remember last pos

	if ( (pEmitter->mNumActivePcl == 0) && (pEmitter->mRegen != 1) ) // if not regenerating flag the emitter that it should expire now
	{
		pEmitter->Expire = 1;
        pEmitter->mRenderableVerts = 0;
        pEmitter->mpIterator = 0;
		pEmitter = NULL;
	}

    if( pEmitter )
        pEmitter->ClearRenderData();

	unguard;
}

inline static void MakeSprite( DynamicVertex* pOutVerts, FVector& camSpaceCenter, float size, short roll, float u, float v, float texU, float texV, DWORD dwClr )
{    
    FVector verts[4];
    FVector uaxis(1,0,0);
    FVector vaxis(0,1,0);

	verts[0] = camSpaceCenter;
    if ( roll != 0 )
	{
        uaxis.X = GMath.CosTab(roll) * size;
        uaxis.Y = GMath.SinTab(roll) * size;
        vaxis.X = -uaxis.Y;
        vaxis.Y = uaxis.X;
	}
    else
    {
        uaxis.X = size;
        uaxis.Y = 0.0f;
		vaxis.X = 0.0f;
        vaxis.Y = size;
    }

    verts[1] = verts[0] + uaxis;
	verts[2] = verts[0] + vaxis;
	verts[3] = verts[0] - uaxis;
	verts[0] -= vaxis;

    pOutVerts->U = u + texU;
    pOutVerts->V = v + texV;
    pOutVerts->Color = dwClr;
    pOutVerts->Point = verts[0];
    pOutVerts++;

    pOutVerts->U = u + texU;
    pOutVerts->V = v;
    pOutVerts->Color = dwClr;
    pOutVerts->Point = verts[1];
    pOutVerts++;

    pOutVerts->U = u;
    pOutVerts->V = v;
    pOutVerts->Color = dwClr;
    pOutVerts->Point = verts[2];
    pOutVerts++;

    pOutVerts->U = u;
    pOutVerts->V = v + texV;
    pOutVerts->Color = dwClr;
    pOutVerts->Point = verts[3];
    pOutVerts++;
}

void ParticleIterator::Draw( AxEmitter* pEmitter, FSceneNode* pFrame )
{
	guard(ParticleIterator::Draw);
	unguard;
}

void LineIterator::Draw( AxEmitter* pEmitter, FSceneNode* pFrame )
{
	guard(LineIterator::Draw);
    quadStream.Init( pEmitter, pEmitter->mRenderableVerts, pFrame->WorldToCamera );
    int	firstVertex = pFrame->Viewport->RI->SetDynamicStream(VS_FixedFunction,&quadStream);
    pFrame->Viewport->RI->DrawQuads( firstVertex, quadStream.numQuads );

	unguard;
}

int ParticleIterator::GetVertexCount( AxEmitter* pEmitter, ParticleT* p )
{
	return 0;
}

int LineIterator::GetVertexCount( AxEmitter* pEmitter, ParticleT* p )
{
	return 12;
}

int SpriteIterator::GetVertexCount( AxEmitter* pEmitter, ParticleT* p )
{
	if ( pEmitter->mTileAnimation )
		return 8;
	return 4;
}

int StreamIterator::GetVertexCount( AxEmitter* pEmitter, ParticleT* p )
{
	if ( pEmitter->mTileAnimation )
		return 8;
	return 8;
}

int MeshIterator::GetVertexCount( AxEmitter* pEmitter, ParticleT* p )
{
	guard(MeshIterator::GetVertexCount);
	return 1; //amb
	unguard;
}

int DiscIterator::GetVertexCount( AxEmitter* pEmitter, ParticleT* p )
{
	guard(DiscIterator::GetVertexCount);
	if ( pEmitter->mTileAnimation )
		return 8;
	return 4;
	unguard;
}

void static DrawStaticMeshSection(FRenderInterface* RI, UStaticMesh* StaticMesh, AxEmitter* Owner, INT SectionIndex, UMaterial* Material, int tile)
{
	FStaticMeshSection&	Section = StaticMesh->Sections(SectionIndex);

	FVertexStream*	VertexStreams[9] = { &StaticMesh->VertexStream };
	INT				NumVertexStreams = 1;

	for(INT UVIndex = 0;UVIndex < StaticMesh->UVStreams.Num();UVIndex++)
		VertexStreams[NumVertexStreams++] = &StaticMesh->UVStreams(UVIndex);

	RI->SetVertexStreams(VS_FixedFunction,VertexStreams,NumVertexStreams);
	RI->SetIndexBuffer(&StaticMesh->IndexBuffer,0);

	if ( Owner->mTotalTiles > 0.0f )
    {
        int col, row;
        DECLARE_STATIC_UOBJECT( UTexScaler, T, {} );
        col = tile / Owner->mNumTileColumns;
        row = tile - (Owner->mNumTileRows * col);
        T->UOffset = Material->MaterialUSize() * (float)row / (float)Owner->mNumTileRows;
        T->VOffset = Material->MaterialVSize() * (float)col / (float)Owner->mNumTileColumns;
        T->UScale = 1.0f;
        T->VScale = 1.0f;
        T->Material = Material;
        RI->SetMaterial(T);
    }
    else
    {
        RI->SetMaterial(Material);
    }

	if(Section.NumPrimitives > 0)
	{
#ifdef __PSX2_EE__
		extern void PSX2Render_RenderStaticMesh(UStaticMesh* StaticMesh, UStaticMeshInstance* Instance, INT SectionIndex);
		PSX2Render_RenderStaticMesh(StaticMesh, (Section.PolyFlags & PF_Unlit) ? NULL : Owner->Actor->StaticMeshInstance, SectionIndex);
#else
		RI->DrawPrimitive(
			Section.IsStrip ? PT_TriangleStrip : PT_TriangleList,
			Section.FirstIndex,
			Section.NumPrimitives,
			Section.MinVertexIndex,
			Section.MaxVertexIndex
			);
#endif
	}
	//Owner->GetLevel()->Engine->Stats->StaticMesh_Sections++;
	//Owner->GetLevel()->Engine->Stats->StaticMesh_Triangles += Section.NumTriangles;
}


void MeshIterator::Draw( AxEmitter* pEmitter, FSceneNode* pFrame )
{
	pFrame->Viewport->RI->SetTransform(TT_WorldToCamera, pFrame->WorldToCamera);
	ParticleT* p = &(*(PclArrayPtr)pEmitter->mpParticles)(0);

    pFrame->Viewport->RI->EnableLighting(1);

	for ( int n=0; n<pEmitter->mMaxParticles; n++, p++ )
	{
		if ( p->life == PCL_DEAD || p->delay > 0.0f )
			continue;

		FVector center;
		if ( pEmitter->mPosRelative )
			center = (GMath.UnitCoords / p->pos / pEmitter->Rotation).Origin + pEmitter->Location;
        else
            center = p->pos;

        if (pEmitter->mPosRelative)
        {
            p->rot.Pitch = pEmitter->Rotation.Pitch;
            p->rot.Yaw = pEmitter->Rotation.Yaw;
        }
        float size = pEmitter->DrawScale * p->size;
		FScaleMatrix LToW( FVector(size,size,size) );
		FRotationMatrix TempRot( p->rot);
		FTranslationMatrix TempTrans( center );
		LToW *= TempRot;
		LToW *= TempTrans;

		// attenuate
		float atten = 1.0f;
		FPlane colorPlane(1,1,1,1);
		FPlane clr = ColorPlane( p->color );
		CalcAttenColor( p, pEmitter, atten, clr, colorPlane );
        FColor fclr(clr);

        pFrame->Viewport->RI->SetAmbientLight( fclr );

        int tile;
        if (pEmitter->mTileAnimation )
            tile = (int)( (pEmitter->mTotalTiles+0.999) * ( 1.0f - (p->life * p->invLifeSpan) ) );
        else
            tile = p->tileNum;

		if ( pEmitter->mMeshNodes[0] )
		{
			pFrame->Viewport->RI->SetTransform( TT_LocalToWorld, LToW );
            for (int section=0; section<pEmitter->mMeshNodes[0]->Sections.Num(); section++) //amb
			    DrawStaticMeshSection( pFrame->Viewport->RI, pEmitter->mMeshNodes[0],  pEmitter, section, pEmitter->Skins(0), tile );
		}

		// tile-based tex coord gen w/blending
		//CalcParticleUV( p, clr, c2, atten, u, v, pEmitter, blend_anim, nextRow, nextCol, texU, texV );
        //float uv[2][2] = { { u, v} , {u+texU, v+texV} };
        //DWORD dwClr = MakeColor(clr);
	}
}

void DiscIterator::Draw( AxEmitter* pEmitter, FSceneNode* pFrame )
{
	guard(DiscIterator::Draw);
	FVector		verts[4];
	FVector		tmp[4];
	FVector		DrawScale = /*pEmitter->ScaleVector*/ FVector(1,1,1) * pEmitter->DrawScale;

	FVector uaxis(1,0,0);
	FVector vaxis(0,1,0);
	FPlane	c(1,1,1,1), c2(1,1,1,1);

	ParticleT* p = &(*(PclArrayPtr)pEmitter->mpParticles)(0);
	DynamicVertex* pOutVerts = NULL;
	int num_required = pEmitter->mRenderableVerts;
	int bufferNum = pFrame->Viewport->RI->LockDynBuffer( (BYTE**)&pOutVerts, pEmitter->mRenderableVerts, sizeof(DynamicVertex), DynamicVertex::VF_Flags  ); // lock the vbuffer
	if ( !pOutVerts )
		return;

	float	texU = pEmitter->mTexU;
	float	texV = pEmitter->mTexV;
	float	u, v;
	float	atten;
	int		verts_used = 0;
	int		num_prims = 0; // primitive count
	int		blend_anim	= 0;
	float	nextRow	= 0.0f;
	float	nextCol	= 0.0f;
    float   size = 0.0f;
	FPlane	clr;
    FVector dir, center;

	// lock working textures
	BYTE*		LifeSrcBits = NULL;
	FColor*		LifePalette = NULL;
	SetupTextureSource( pEmitter->mLifeColorMap, LifeSrcBits, LifePalette );
	float curLife = pEmitter->LifeSpan;
	float maxLife = pEmitter->GetClass()->GetDefaultActor()->LifeSpan;


	for ( int n=0; n<pEmitter->mMaxParticles; n++, p++ )
	{
		if ( p->life == PCL_DEAD || p->delay > 0.0f )
			continue;

        size = pEmitter->DrawScale * p->size;

        if ( !pEmitter->mSpawnVecA.IsNearlyZero() )
            dir = pEmitter->mSpawnVecA.UnsafeNormal();
        else if ( pEmitter->mPosRelative || p->velocity.IsNearlyZero() )
            dir = pEmitter->Rotation.Vector();
        else
            dir = p->velocity.UnsafeNormal();

        if ( Abs(dir.Z) > 0.57 ) // One of the components in a unit vector has to be > 0.57f [sqrt(1/3)].
        {
            vaxis.X = GMath.SinTab( p->rot.Roll );
            vaxis.Y = GMath.CosTab( p->rot.Roll );
            vaxis.Z = 0;
        }
        else if ( Abs(dir.X) > 0.57 )
        {
            vaxis.X = 0;
            vaxis.Y = GMath.SinTab( p->rot.Roll );
            vaxis.Z = GMath.CosTab( p->rot.Roll );
        }
        else
        {
            vaxis.X = GMath.SinTab( p->rot.Roll );
            vaxis.Y = 0;
            vaxis.Z = GMath.CosTab( p->rot.Roll );
        }

        uaxis = FVector(dir ^ vaxis).UnsafeNormal();
        vaxis = (uaxis ^ dir);

        uaxis *= size;
        vaxis *= size;

		if ( pEmitter->mPosRelative )
			center = (GMath.UnitCoords / p->pos / pEmitter->Rotation).Origin + pEmitter->Location;
        else
            center = p->pos;

        verts[0] = center - vaxis;
        verts[1] = center + uaxis;
        verts[2] = center + vaxis;
        verts[3] = center - uaxis;

        verts[0] = pFrame->WorldToCamera.TransformFVector(verts[0]);
        verts[1] = pFrame->WorldToCamera.TransformFVector(verts[1]);
        verts[2] = pFrame->WorldToCamera.TransformFVector(verts[2]);
        verts[3] = pFrame->WorldToCamera.TransformFVector(verts[3]);


		// get lifemap color if exists
		FPlane colorPlane(1,1,1,1);
		if ( LifeSrcBits )
		{
			FetchTextureColor( colorPlane, pEmitter->mLifeColorMap, curLife / maxLife, p->life * p->invLifeSpan,
								LifeSrcBits, LifePalette );
		}

		// attenuate
		atten = 1.0f;
		clr = ColorPlane( p->color );
		CalcAttenColor( p, pEmitter, atten, clr, colorPlane );

		// tile-based tex coord gen w/blending
		CalcParticleUV( p, clr, c2, atten, u, v, pEmitter, blend_anim, nextRow, nextCol, texU, texV );

		for ( int passes=0; passes<blend_anim+1; passes++ )
		{
			DWORD dwClr = MakeColor(clr);
			pOutVerts->U = u + texU;
			pOutVerts->V = v + texV;
			pOutVerts->Color = dwClr;
			pOutVerts->Point = verts[0];
			pOutVerts++;

			pOutVerts->U = u + texU;
			pOutVerts->V = v;
			pOutVerts->Color = dwClr;
			pOutVerts->Point = verts[1];
			pOutVerts++;

			pOutVerts->U = u;
			pOutVerts->V = v;
			pOutVerts->Color = dwClr;
			pOutVerts->Point = verts[2];
			pOutVerts++;

			pOutVerts->U = u;
			pOutVerts->V = v + texV;
			pOutVerts->Color = dwClr;
			pOutVerts->Point = verts[3];
			pOutVerts++;

			// blend anim settings for second pass
			clr = c2;
			u = nextCol * texU;
			v = nextRow * texV;
			
			num_prims += 1;
			verts_used += 4;

			// check for vbuffer overflow
			if ( verts_used + 4 > bufferNum )
			{
				pFrame->Viewport->RI->DrawDynQuads( num_prims );
				num_prims = 0;
				if ( verts_used < pEmitter->mRenderableVerts )
				{
					bufferNum = pFrame->Viewport->RI->LockDynBuffer( (BYTE**)&pOutVerts, num_required - verts_used, sizeof(DynamicVertex), DynamicVertex::VF_Flags  );
					num_required -= verts_used;
					verts_used = 0;
					if ( !pOutVerts )
						return;
				}
			}
		}
	}
	if ( num_prims > 0 )
		pFrame->Viewport->RI->DrawDynQuads( num_prims );
	// obsolete! pM->numTris = num_prims;

	// unlock
	UnlockTextureSource( pEmitter->mLifeColorMap );
	unguard;
}

void SpriteIterator::Draw( AxEmitter* pEmitter, FSceneNode* pFrame )
{
	guard(SpriteIterator::Draw);

    quadStream.Init( pEmitter, pEmitter->mRenderableVerts, pFrame->WorldToCamera );
    int	firstVertex = pFrame->Viewport->RI->SetDynamicStream(VS_FixedFunction,&quadStream);

    // no projector case
    if( !pEmitter->Projectors.Num() )
    {
        pFrame->Viewport->RI->DrawQuads( firstVertex, quadStream.numQuads );
        return;
    }
    
    // otherwise, projector fun!
    FRenderInterface* RI = pFrame->Viewport->RI;
    if( pEmitter->Projectors.Num() )
	{
		for( INT i=0;i<pEmitter->Projectors.Num();i++ )
		{
			FProjectorRenderInfo* P = pEmitter->Projectors(i);
			if( !P->Render( pEmitter->Level->TimeSeconds ) )
			{
				pEmitter->Projectors.Remove(i--);
				continue;
			}

			// Setup blending.
			UFinalBlend*	FinalBlend = Cast<UFinalBlend>(P->Material);

			DECLARE_STATIC_UOBJECT(
				UTexMatrix,
				SpritePclProjectorMatrix,
				{
					SpritePclProjectorMatrix->TexCoordSource = TCS_WorldCoords;
					SpritePclProjectorMatrix->TexCoordCount = TCN_3DCoords;
				}
				);

            FMatrix matrix = pFrame->CameraToWorld * P->Matrix; // put the projector into camera space!

            SpritePclProjectorMatrix->Material = FinalBlend ? FinalBlend->Material : P->Material;
			SpritePclProjectorMatrix->Matrix = matrix;
			SpritePclProjectorMatrix->TexCoordProjected = (P->ProjectorFlags & PRF_Projected) ? 1 : 0;

            DECLARE_STATIC_UOBJECT( UCombiner, PclCombiner, {PclCombiner->CombineOperation = CO_Multiply;} );
            PclCombiner->Material1			= pEmitter->Skins(0);
		    PclCombiner->Material2			= SpritePclProjectorMatrix;
		    PclCombiner->Mask				= NULL;
		    PclCombiner->Modulate2X		    = 0;

			DECLARE_STATIC_UOBJECT(
				UFinalBlend,
				SpritePclProjectorBlend,
				{
					SpritePclProjectorBlend->Material = PclCombiner;
					SpritePclProjectorBlend->ZWrite = 0;
				}
			    );

            switch( pEmitter->Style )
	        {
	        case STY_Masked:
		        SpritePclProjectorBlend->AlphaTest = 1;
		        SpritePclProjectorBlend->AlphaRef = 127;
		        break;
	        case STY_Translucent:
		        SpritePclProjectorBlend->FrameBufferBlending = FB_Translucent;
		        SpritePclProjectorBlend->AlphaTest = 0;	
		        break;
	        case STY_Modulated:
		        SpritePclProjectorBlend->FrameBufferBlending = FB_Modulate;
		        SpritePclProjectorBlend->AlphaTest = 0;	
		        break;
            case STY_AlphaZ:
                SpritePclProjectorBlend->ZWrite = 1;
	        case STY_Alpha:
		        SpritePclProjectorBlend->FrameBufferBlending = FB_AlphaBlend;
		        SpritePclProjectorBlend->AlphaTest = 1;
		        SpritePclProjectorBlend->AlphaRef = 0;		
		        break;
	        case STY_Additive:
		        SpritePclProjectorBlend->FrameBufferBlending = FB_Brighten;
		        SpritePclProjectorBlend->AlphaTest = 1;
		        SpritePclProjectorBlend->AlphaRef = 0;	
		        break;
	        case STY_Subtractive:
		        SpritePclProjectorBlend->FrameBufferBlending = FB_Darken;
		        SpritePclProjectorBlend->AlphaTest = 0;
		        break;
	        }
			RI->SetMaterial(SpritePclProjectorBlend);
            pFrame->Viewport->RI->DrawQuads( firstVertex, quadStream.numQuads );
		}
    }
    
	unguard;
}

void StreamIterator::Draw( AxEmitter* pEmitter, FSceneNode* pFrame ) // particle ordering issues
{
	guard(StreamIterator::Draw);
	FVector		verts[4];
	FVector		tmp[4];
    FVector		tmpU[4];
	FVector		DrawScale = /*pEmitter->ScaleVector*/ FVector(1,1,1) * pEmitter->DrawScale;
	FCoords		emitterMatrix;

	emitterMatrix = GMath.UnitCoords * pEmitter->Location / pEmitter->Rotation;

	FVector		uaxis = FVector(1,0,0);// emitterMatrix.XAxis;
	FVector		vaxis = FVector(0,1,0);//emitterMatrix.YAxis; 

    //vaxis = pFrame->WorldToCamera.TransformNormal(vaxis);

	//ParticleT* pPrevPcl = NULL;
	ParticleT* p = &(*(PclArrayPtr)pEmitter->mpParticles)(0);

	float	ratio = -1.0f;
	float	tu = 1.0f;//pEmitter->mTime - (int)pEmitter->mTime;
	tu *= -ratio;
	float	tuinc = -ratio;
	float	size;
	int		i = 0; // polygon index
    int num_prims = 0;
    int verts_used = 0;
	
    DynamicVertex* pOutVerts = NULL;
	int num_required = pEmitter->mRenderableVerts + 4;
	int bufferNum = pFrame->Viewport->RI->LockDynBuffer( (BYTE**)&pOutVerts, num_required, sizeof(DynamicVertex), DynamicVertex::VF_Flags  );
	if ( !pOutVerts )
		return;

	BYTE*		LifeSrcBits = NULL;
	FColor*		LifePalette = NULL;
    SetupTextureSource( pEmitter->mLifeColorMap, LifeSrcBits, LifePalette );

	DWORD prev_color = 0x0;

    float emAtten = 1.0f;
    if( pEmitter->mRegen == 0 )
        emAtten = (float)pEmitter->mNumActivePcl / (float)pEmitter->mMaxParticles;
    //if( pEmitter->GetClass()->GetDefaultActor()->LifeSpan != 0.0f )
        //emAtten = pEmitter->LifeSpan / pEmitter->GetClass()->GetDefaultActor()->LifeSpan;
	
	int next = pEmitter->mHeadIndex;
    for ( ; ; )
    {
        if ( next == INDEX_NONE )
            break;
        
        p = &(*(PclArrayPtr)pEmitter->mpParticles)(next);
        next = p->nextIndex;
		if ( p->life == PCL_DEAD || p->delay > 0.0f )
			continue;
        
		size = p->size;// * (p->life * p->invLifeSpan);
		//size *= size;// * p->size * pEmitter->DrawScale;

		if ( i == 0 )
		{
			verts[1] = pFrame->WorldToCamera.TransformFVector(pEmitter->Location);// always start at the spawn point
			verts[2] = verts[1];

			if ( pEmitter->mPosRelative )
			    verts[0] = pFrame->WorldToCamera.TransformFVector(pEmitter->Location + p->pos);
		    else
			    verts[0] = pFrame->WorldToCamera.TransformFVector(p->pos);

			verts[3] = verts[0];

			//vaxis = pEmitter->mDir;//emitterDir;

			verts[1] += vaxis * size;
			verts[2] -= vaxis * size;

			verts[0] += vaxis * size;
			verts[3] -= vaxis * size;

			tmp[0] = verts[0];
			tmp[3] = verts[3];
		}
		else
		{
			//vaxis = p->dir;

			verts[1] = tmp[0];
			verts[2] = tmp[3];

            if ( pEmitter->mPosRelative )
			    verts[0] = pFrame->WorldToCamera.TransformFVector(pEmitter->Location + p->pos);
		    else
			    verts[0] = pFrame->WorldToCamera.TransformFVector( p->pos );

			verts[3] = verts[0];

			verts[0] += vaxis * size;
			verts[3] -= vaxis * size;

			tmp[0] = verts[0];
			tmp[3] = verts[3];
		}

		//tuinc = mpCurNodes[curidx].afuvA[0] * .025f;

		float tvA = 0.01f;
		float tvB = 0.49f;

        // attenuate
        FPlane colorPlane(1,1,1,1);
		if ( LifeSrcBits )
		{
	        float curLife = pEmitter->LifeSpan;
	        float maxLife = pEmitter->GetClass()->GetDefaultActor()->LifeSpan;
			FetchTextureColor( colorPlane, pEmitter->mLifeColorMap, curLife / maxLife, p->life * p->invLifeSpan,
								LifeSrcBits, LifePalette );
		}
		float atten = emAtten;
		FPlane clr = ColorPlane( p->color );
        if ( i == 0 )
        {
            clr *= emAtten;
        }
        else
            CalcAttenColor( p, pEmitter, atten, clr, colorPlane );	

        DWORD dwClr = MakeColor(clr);

        //if ( p->nextIndex == INDEX_NONE )
            //dwClr = 0x0;

		if ( i == 0 )
			prev_color = dwClr;

        pOutVerts[0].Point = verts[0];
		pOutVerts[0].U = tu + tuinc;	pOutVerts[0].V = tvA;
		pOutVerts[0].Color = dwClr;

        pOutVerts[1].Point = verts[1]; 
		pOutVerts[1].U = tu;			pOutVerts[1].V = tvA;
		pOutVerts[1].Color = prev_color;

        pOutVerts[2].Point = verts[2];
		pOutVerts[2].U = tu;			pOutVerts[2].V = tvB;
		pOutVerts[2].Color = prev_color;

        pOutVerts[3].Point = verts[3];
		pOutVerts[3].U = tu + tuinc;	pOutVerts[3].V = tvB;
	    pOutVerts[3].Color = dwClr;

		num_prims += 1;
		verts_used += 4;
        pOutVerts += 4;
        
        if ( i == 0 )
		{
			verts[1] = pFrame->WorldToCamera.TransformFVector(pEmitter->Location);// always start at the spawn point
			verts[2] = verts[1];

			if ( pEmitter->mPosRelative )
			    verts[0] = pFrame->WorldToCamera.TransformFVector(pEmitter->Location + p->pos);
		    else
			    verts[0] = pFrame->WorldToCamera.TransformFVector(p->pos);

			verts[3] = verts[0];

			//vaxis = pEmitter->mDir;//emitterDir;

			verts[1] += uaxis * size;
			verts[2] -= uaxis * size;

			verts[0] += uaxis * size;
			verts[3] -= uaxis * size;

			tmpU[0] = verts[0];
			tmpU[3] = verts[3];
		}
		else
		{
			//vaxis = p->dir;

			verts[1] = tmpU[0];
			verts[2] = tmpU[3];

            if ( pEmitter->mPosRelative )
			    verts[0] = pFrame->WorldToCamera.TransformFVector(pEmitter->Location + p->pos);
		    else
			    verts[0] = pFrame->WorldToCamera.TransformFVector( p->pos );

			verts[3] = verts[0];

			verts[0] += uaxis * size;
			verts[3] -= uaxis * size;

			tmpU[0] = verts[0];
			tmpU[3] = verts[3];
		}

        pOutVerts[0].Point = verts[0];
		pOutVerts[0].U = tu + tuinc;	pOutVerts[0].V = tvA;
		pOutVerts[0].Color = dwClr;

        pOutVerts[1].Point = verts[1]; 
		pOutVerts[1].U = tu;			pOutVerts[1].V = tvA;
		pOutVerts[1].Color = prev_color;

        pOutVerts[2].Point = verts[2];
		pOutVerts[2].U = tu;			pOutVerts[2].V = tvB;
		pOutVerts[2].Color = prev_color;

        pOutVerts[3].Point = verts[3];
		pOutVerts[3].U = tu + tuinc;	pOutVerts[3].V = tvB;
	    pOutVerts[3].Color = dwClr;

		num_prims += 1;
		verts_used += 4;
        pOutVerts += 4;


		// check for vbuffer overflow
		if ( verts_used + 8 > bufferNum )
		{
			pFrame->Viewport->RI->DrawDynQuads( num_prims );
			num_prims = 0;
			if ( verts_used < num_required )
			{
				bufferNum = pFrame->Viewport->RI->LockDynBuffer( (BYTE**)&pOutVerts, num_required - verts_used, sizeof(DynamicVertex), DynamicVertex::VF_Flags  );
				num_required -= verts_used;
				verts_used = 0;
				if ( !pOutVerts )
					return;
			}
		}

		prev_color = dwClr;

		tu += tuinc;
		i++;
	}

    FVector cent;
    cent = pFrame->WorldToCamera.TransformFVector(pEmitter->Location);
    DWORD clr = 0xffffffff;
    FPlane pl( emAtten,emAtten,emAtten,1.0f );
    clr = MakeColor( pl );
    
    MakeSprite( pOutVerts, cent, pEmitter->mSpawnVecB.X, 0, 0.0f, 0.5f, 1.0f, 0.5f, clr );
    num_prims += 1;
    verts_used += 4;
    pOutVerts += 4;

	if ( num_prims > 0 )
		pFrame->Viewport->RI->DrawDynQuads( num_prims );
	unguard;
}

int BranchIterator::GetVertexCount( AxEmitter* pEmitter, ParticleT* p )
{
	return 8;
}

void BranchIterator::Draw( AxEmitter* pEmitter, FSceneNode* pFrame ) // particle ordering issues
{
	guard(StreamIterator::Draw);
	FVector		verts[4];
	FVector		prevVecA[2];
    FVector		prevVecB[2];
	FVector		DrawScale = FVector(1,1,1) * pEmitter->DrawScale;
	FCoords		emitterMatrix;

	emitterMatrix = GMath.UnitCoords * pEmitter->Location * pEmitter->Rotation;

	FVector		uaxis = emitterMatrix.XAxis; //pFrame->Coords.XAxis;
	FVector		vaxis = emitterMatrix.YAxis; 

	//ParticleT* pPrevPcl = NULL;
	ParticleT* p = &(*(PclArrayPtr)pEmitter->mpParticles)(0);
	ParticleT* mainp = &(*(PclArrayPtr)pEmitter->mpParticles)(0);

	float	ratio = -0.1f;
	float	tu = 1.0f;//pEmitter->mTime - (int)pEmitter->mTime;
	tu *= -ratio;
	float	tuinc = -ratio;
	float	size;
    int num_prims = 0;
    int verts_used = 0;
    DWORD headColor = 0xffffffff;
	
    DynamicVertex* pOutVerts = NULL;
	int num_required = pEmitter->mRenderableVerts + 8;
	int bufferNum = pFrame->Viewport->RI->LockDynBuffer( (BYTE**)&pOutVerts, num_required, sizeof(DynamicVertex), DynamicVertex::VF_Flags  );
	if ( !pOutVerts )
		return;

	FPlane	prev_color(1.f,1.f,1.f,1.f);
	
	for ( int i=0; i<pEmitter->mMaxParticles; i++ )
	{
		if ( mainp->life == PCL_DEAD || mainp->rotInertia[0] != 1 ) // found the head of a branch
		{
			mainp++;
			continue;
		}

		int next = mainp->nextIndex;
		tu = 1.0f;
		int count = 0;
        size = p->size;

		prevVecA[0] = pFrame->WorldToCamera.TransformFVector(mainp->pos);// always start at the spawn point
		prevVecA[1] = prevVecA[0];
		prevVecA[0] += vaxis * size;
		prevVecA[1] -= vaxis * size;

        prevVecB[0] = pFrame->WorldToCamera.TransformFVector(mainp->pos);// always start at the spawn point
		prevVecB[1] = prevVecB[0];
		prevVecB[0] += uaxis * size;
		prevVecB[1] -= uaxis * size;

        FPlane c = p->color.Plane() * p->life * p->invLifeSpan;	
        headColor = MakeColor(c);
		for ( ; ; )
		{
			if ( next == INDEX_NONE )
				break;
    
			p = &(*(PclArrayPtr)pEmitter->mpParticles)(next);
			next = p->nextIndex;
			if ( p->life == PCL_DEAD || p->delay > 0.0f )
				continue;
    
			size = p->size;// * (p->life * p->invLifeSpan);
            //if ( next == INDEX_NONE )
            //    size = 0.0f;

			verts[1] = prevVecA[0];
			verts[2] = prevVecA[1];

			if ( pEmitter->mPosRelative )
				verts[0] = pFrame->WorldToCamera.TransformFVector(pEmitter->Location + p->pos);
			else
				verts[0] = pFrame->WorldToCamera.TransformFVector(p->pos);
			verts[3] = verts[0];
			verts[0] += vaxis * size;
			verts[3] -= vaxis * size;

			prevVecA[0] = verts[0];
			prevVecA[1] = verts[3];

			float tvA = 0.01f;
			float tvB = 0.499f;

			FPlane c = p->color.Plane() * p->life * p->invLifeSpan;	
			if ( count == 0 )
				prev_color = c;

			pOutVerts[0].Point = verts[0];
			pOutVerts[0].U = tu + tuinc;	pOutVerts[0].V = tvA;
			pOutVerts[0].Color = MakeColor(c);

			pOutVerts[1].Point = verts[1];
			pOutVerts[1].U = tu;			pOutVerts[1].V = tvA;
			pOutVerts[1].Color = MakeColor(prev_color);

			pOutVerts[2].Point = verts[2];
			pOutVerts[2].U = tu;			pOutVerts[2].V = tvB;
			pOutVerts[2].Color = MakeColor(prev_color);

			pOutVerts[3].Point = verts[3];
			pOutVerts[3].U = tu + tuinc;	pOutVerts[3].V = tvB;
			pOutVerts[3].Color = MakeColor(c);

			num_prims += 1;
			verts_used += 4;
			pOutVerts += 4;

            verts[1] = prevVecB[0];
			verts[2] = prevVecB[1];

			if ( pEmitter->mPosRelative )
				verts[0] = pFrame->WorldToCamera.TransformFVector(pEmitter->Location + p->pos);
			else
				verts[0] = pFrame->WorldToCamera.TransformFVector(p->pos);
			verts[3] = verts[0];
			verts[0] += uaxis * size;
			verts[3] -= uaxis * size;

			prevVecB[0] = verts[0];
			prevVecB[1] = verts[3];
            
			pOutVerts[0].Point = verts[0];
			pOutVerts[0].U = tu + tuinc;	pOutVerts[0].V = tvA;
			pOutVerts[0].Color = MakeColor(c);

			pOutVerts[1].Point = verts[1];
			pOutVerts[1].U = tu;			pOutVerts[1].V = tvA;
			pOutVerts[1].Color = MakeColor(prev_color);

			pOutVerts[2].Point = verts[2];
			pOutVerts[2].U = tu;			pOutVerts[2].V = tvB;
			pOutVerts[2].Color = MakeColor(prev_color);

			pOutVerts[3].Point = verts[3];
			pOutVerts[3].U = tu + tuinc;	pOutVerts[3].V = tvB;
			pOutVerts[3].Color = MakeColor(c);

			num_prims += 1;
			verts_used += 4;
			pOutVerts += 4;

			// check for vbuffer overflow
			if ( verts_used + 8 > bufferNum )
			{
				pFrame->Viewport->RI->DrawDynQuads( num_prims );
				num_prims = 0;
				if ( verts_used < pEmitter->mRenderableVerts )
				{
					bufferNum = pFrame->Viewport->RI->LockDynBuffer( (BYTE**)&pOutVerts, num_required - verts_used, sizeof(DynamicVertex), DynamicVertex::VF_Flags  );
					num_required -= verts_used;
					verts_used = 0;
					if ( !pOutVerts )
						return;
				}
			}
			prev_color = c;
			tu += tuinc;
			count++;
		}

        FVector cent;
        cent = pFrame->WorldToCamera.TransformFVector(pEmitter->Location);
        MakeSprite( pOutVerts, cent, pEmitter->mSpawnVecB.X, 0, 0.0f, 0.5f, 1.0f, 0.5f, headColor );
        num_prims += 1;
        verts_used += 4;
        pOutVerts += 4;
        cent = pFrame->WorldToCamera.TransformFVector(pEmitter->mSpawnVecA);
        MakeSprite( pOutVerts, cent, pEmitter->mSpawnVecB.Y, 0, 0.0f, 0.5f, 1.0f, 0.5f, headColor );
        num_prims += 1;
        verts_used += 4;
        pOutVerts += 4;
		mainp++;
	}
	
	if ( num_prims > 0 )
		pFrame->Viewport->RI->DrawDynQuads( num_prims );

	unguard;
}



int BeamIterator::GetVertexCount( AxEmitter* pEmitter, ParticleT* p )
{
	if ( pEmitter->mTileAnimation )
		return 8;
	return 4;
}

void BeamIterator::Update( AxEmitter* pEmitter )
{
	guard(BeamIterator::Update);

    pEmitter->mRenderableVerts = 1;

    if (pEmitter->mAttenuate)
    {
        // reset
        if (pEmitter->mStartParticles > 0)
        {
            pEmitter->mLifeRange[1] = pEmitter->mLifeRange[0];
            //pEmitter->mSizeRange[1] = pEmitter->mSizeRange[0];
            pEmitter->mStartParticles = 0;
        }

        if (pEmitter->mLifeRange[1] <= 0.0f) pEmitter->mRenderableVerts = 0;
    }

    // timed updates //
    if (pEmitter->mAttenuate)
    {
        pEmitter->mLifeRange[1] -= pEmitter->mT;
    }

    pEmitter->mSpinRange[1] += pEmitter->mT * pEmitter->mSpinRange[0];
    if (pEmitter->mSpinRange[1] >= 65536.0f) pEmitter->mSpinRange[1] -= 65536.0f;

    pEmitter->mWavePhaseA += pEmitter->mT * pEmitter->mWaveShift;
    if (pEmitter->mWavePhaseA >= 65536.0) pEmitter->mWavePhaseA -= 65536.0;

    pEmitter->mWavePhaseB -= pEmitter->mT * pEmitter->mWaveShift;
    if (pEmitter->mWavePhaseB < 0.0) pEmitter->mWavePhaseB += 65536.0;

    // udpate bound box //
    pEmitter->mBounds.IsValid = 0;
	pEmitter->mBounds += pEmitter->Location;
	pEmitter->mBounds += pEmitter->mSpawnVecA;

    unguard;
}

void BeamIterator::Draw( AxEmitter* pEmitter, FSceneNode* pFrame )
{
    /*
        start point - Location
        end point - mSpawnVecA
        beam diameter - mSizeRange[0], mSizeRange[1] (start, final)
        rotation rate - mSpinRange[0]
        current rotation - mSpinRange[1]
        beam planes - mMaxParticles
        sampling distance - mRegenDist
        attenuation - mAttenuate hardcoded linear lifetime falloff
        atten life time - mLifeRange[0]
        start fadeout distance - mAttenKa (distance from start of beam)
    */

	guard(BeamIterator::Draw);
    
    DynamicVertex* pOutVerts = NULL;
    int NumVerts, VertsNeeded;
    int NumPrims = 0;
    FVector XAxis, YAxis, ZAxis, V;
    float Length;
    FVector Normal;
    FVector Start, End, BeamVect;
    FVector SecStart, SecEnd, SecEnd2;
    float SectionLength;
    int Planes, Sections;
    FVector SVerts[8][2]; // MaxPlanes = 8
    FVector EVerts[8][2];
    float Width;
    float PlaneRot;
    float Spin, SpinRate;
    FPlane PlaneColor;
    DWORD dwColor, dwPrevColor = 0;
    int p, s;
    float f;
    float BeamPos = 0.0f;
    float StartFadeDist;
    ParticleT Particle;
    int x, y;
    float u, v, u2, v2;
    int TileNum = 0;
    float Atten; // 1 to 0 over life of beam
    FMatrix MeshLToW;

    Start         = pEmitter->Location;
    if (pEmitter->mPosRelative)
    {
        End = Start + (GMath.UnitCoords / pEmitter->mSpawnVecA / pEmitter->Rotation).Origin;
    }
    else
    {
        End = pEmitter->mSpawnVecA;
    }
    BeamVect      = End - Start;
    Length        = BeamVect.Size();
    SectionLength = pEmitter->mRegenDist;
    Planes        = pEmitter->mMaxParticles; 
    Sections      = (int)(Length/SectionLength) + 1;
    Spin          = pEmitter->mSpinRange[1];
    SpinRate      = pEmitter->mSpinRange[0];
    StartFadeDist = pEmitter->mAttenKa;
    Atten         = (pEmitter->mLifeRange[1] / pEmitter->mLifeRange[0]);
    Width         = pEmitter->mSizeRange[0];

    if (Start == End) return;

    if (Planes > 8) Planes = 8;

    XAxis = BeamVect;
    XAxis.Normalize();
    YAxis = XAxis.GetNonParallel();
    ZAxis = XAxis ^ YAxis;
    YAxis = XAxis ^ ZAxis;
    
    VertsNeeded = (Sections + 1) * Planes * 4;

    // width //
    if (pEmitter->mAttenuate)
    {
        Width = pEmitter->mSizeRange[0] + (pEmitter->mSizeRange[1] - pEmitter->mSizeRange[0]) * (1.0f-Atten)*(1.0f-Atten);
    }

    if (pEmitter->mMeshNodes[0])
    {
    	pFrame->Viewport->RI->SetTransform(TT_WorldToCamera, pFrame->WorldToCamera);
        pFrame->Viewport->RI->EnableLighting(1);

        FVector Scale = pEmitter->DrawScale3D * pEmitter->DrawScale;
        Scale.Y *= Width;
        Scale.Z *= Width;
        FCoords C(FVector(0,0,0), XAxis, YAxis, ZAxis);
        MeshLToW = FScaleMatrix( Scale ) * C.Inverse().Matrix();
    }
    else
    {
	    NumVerts = pFrame->Viewport->RI->LockDynBuffer( (BYTE**)&pOutVerts, VertsNeeded, sizeof(DynamicVertex), DynamicVertex::VF_Flags );
        if (NumVerts < VertsNeeded || pOutVerts==NULL)
            return; // fix me
    }

    SecStart = Start;

    // start points //
    for (p = 0; p < Planes; p++)
    {
        PlaneRot = (32768.0f * p / Planes) + Spin;
        V = (YAxis * GMath.SinTab(PlaneRot) + ZAxis * GMath.CosTab(PlaneRot)) * Width;
        SVerts[p][1] = SecStart + V;
        SVerts[p][0] = SecStart - V;

        SVerts[p][0] = pFrame->WorldToCamera.TransformFVector(SVerts[p][0]);
        SVerts[p][1] = pFrame->WorldToCamera.TransformFVector(SVerts[p][1]);
    }

    // start color //
    if (StartFadeDist > 0.0f)
    {
        PlaneColor = FPlane(0.0f, 0.0f, 0.0f, 1.0f);
    }
    else
    {
        PlaneColor = ColorPlane(pEmitter->mColorRange[0]) * pEmitter->ScaleGlow; //FPlane(1.0f, 1.0f, 1.0f, 1.0f);
        if (pEmitter->mAttenuate)
        {
            PlaneColor *= Atten*Atten;
        }
    }
    dwPrevColor = MakeColor(PlaneColor);

    // beam sections //
    for (s = 1; s <= Sections; s++)
    {
        BeamPos = SectionLength * s;
        SecEnd = Start + BeamVect * (BeamPos / Length);
        f = 1.0f;

        if (s == Sections)
        {
            // f is the scale of the last beam section since it might be shorter than the rest.
            // the beam texture must tile verticaly mmkay.
            f = 1.0f - (BeamPos - Length) / SectionLength;
            BeamPos = Length;
            SecEnd = End;
        }
        
        // bendyness
        if (!pEmitter->mPosRelative && s < Sections && pEmitter->mBendStrength > 0.0f)
        {
            float a = pEmitter->mBendStrength / (float)(s+pEmitter->mBendStrength);
            SecEnd2 = Start + pEmitter->Rotation.Vector() * BeamPos;
            SecEnd = SecEnd2*a + SecEnd*(1.0-a);
        }

        // wave effect
        if (pEmitter->mWaveAmplitude > 0.0f)
        {
            float SecAmp = pEmitter->mWaveAmplitude;
            float ClampDist = SectionLength*3.0f;
            if (BeamPos <= ClampDist)
                SecAmp *= (BeamPos / ClampDist);
            else if (pEmitter->mWaveLockEnd && BeamPos > Length - ClampDist)
                SecAmp *= ((Length - BeamPos) / ClampDist);

            float A = 65536.0f * (s+f) * pEmitter->mWaveFrequency + pEmitter->mWavePhaseA;
            float B = 65536.0f * (s+f) * pEmitter->mWaveFrequency * 1.231f + pEmitter->mWavePhaseB;
            SecEnd += YAxis * SecAmp * (GMath.SinTab(A) + 0.5f * GMath.SinTab(B*4.0f));
            SecEnd += ZAxis * (SecAmp * (GMath.SinTab(B) + 0.5f * GMath.SinTab(A*4.0f)));
        }

        // color //
        PlaneColor = ColorPlane(pEmitter->mColorRange[0]) * pEmitter->ScaleGlow; //FPlane(1.0f, 1.0f, 1.0f, 1.0f);
        if (pEmitter->mAttenuate)
        {
            PlaneColor *= Atten*Atten;
        }
        if (BeamPos < StartFadeDist && StartFadeDist > 0.0f)
        {
            PlaneColor *= (BeamPos / StartFadeDist);
        }
        dwColor = MakeColor(PlaneColor);

        // static mesh
        if (pEmitter->mMeshNodes[0])
        {
            FMatrix LToW;
            LToW = MeshLToW * FTranslationMatrix( SecStart );

            FColor fclr(PlaneColor);
            pFrame->Viewport->RI->SetAmbientLight( fclr );

            int tile = 0;
            if (pEmitter->mTileAnimation )
                tile = (int)( (pEmitter->mTotalTiles+0.999f) * Atten );

            pFrame->Viewport->RI->SetTransform( TT_LocalToWorld, LToW );
            for (int section = 0; section < pEmitter->mMeshNodes[0]->Sections.Num(); section++)
            {
	            DrawStaticMeshSection( pFrame->Viewport->RI, pEmitter->mMeshNodes[0], pEmitter, section, pEmitter->Skins(0), tile );
            }
        }
        // planes
        else
        {

        for (p = 0; p < Planes; p++)
        {
            // end points //
            PlaneRot = (32768.0f * p / Planes) + Spin;
            V = (YAxis * GMath.SinTab(PlaneRot) + ZAxis * GMath.CosTab(PlaneRot)) * Width;
            EVerts[p][0] = SecEnd - V;
            EVerts[p][1] = SecEnd + V;

            EVerts[p][0] = pFrame->WorldToCamera.TransformFVector(EVerts[p][0]);
            EVerts[p][1] = pFrame->WorldToCamera.TransformFVector(EVerts[p][1]);

            // calc UV - same for all sections for now //
            if (pEmitter->mAttenuate)
            {
                TileNum = (1.0f - Atten) * (pEmitter->mNumTileColumns * pEmitter->mNumTileRows);
            }
            x = TileNum % pEmitter->mNumTileColumns;
            y = TileNum / pEmitter->mNumTileColumns;
            u = (float)x / pEmitter->mNumTileColumns;
            v = (float)y / pEmitter->mNumTileRows;
            u2 = u + (1.0f / pEmitter->mNumTileColumns);
            v2 = v + (1.0f / pEmitter->mNumTileRows);

            // fill in verts //
            pOutVerts[0].Point = SVerts[p][0];
            pOutVerts[0].U = u2;
            pOutVerts[0].V = v;
            pOutVerts[0].Color = dwPrevColor;

            pOutVerts[1].Point = EVerts[p][0];
            pOutVerts[1].U = u2;
            pOutVerts[1].V = v2 * f;
            pOutVerts[1].Color = dwColor;

            pOutVerts[2].Point = EVerts[p][1];
            pOutVerts[2].U = u;
            pOutVerts[2].V = v2 * f;
            pOutVerts[2].Color = dwColor;

            pOutVerts[3].Point = SVerts[p][1];
            pOutVerts[3].U = u;
            pOutVerts[3].V = v;
            pOutVerts[3].Color = dwPrevColor;

            pOutVerts += 4;
            NumPrims++;
        }

        /*PlaneRot = Spin;
        Axis = (YAxis * GMath.SinTab(PlaneRot) + ZAxis * GMath.CosTab(PlaneRot)) * Width;
        U = 0;
        for (p = 0; p < Planes; p++)
        {
            NextPlaneRot = PlaneRot + (32768.0f / Planes);
            NextAxis = (YAxis * GMath.SinTab(NextPlaneRot) + ZAxis * GMath.CosTab(NextPlaneRot)) * Width;
            NextU = U + (1.0f / Planes);

            EVerts[p][0] = SecEnd + NextAxis;
            EVerts[p][1] = SecEnd + Axis;

            EVerts[p][0] = pFrame->WorldToCamera.TransformFVector(EVerts[p][0]);
            EVerts[p][1] = pFrame->WorldToCamera.TransformFVector(EVerts[p][1]);

            // fill in verts //
            pOutVerts[0].Point = SVerts[p][0];
            pOutVerts[0].U = U;
            pOutVerts[0].V = 0;
            pOutVerts[0].Color = dwPrevColor;

            pOutVerts[1].Point = EVerts[p][0];
            pOutVerts[1].U = U;
            pOutVerts[1].V = 1 * f;
            pOutVerts[1].Color = dwColor;

            pOutVerts[2].Point = EVerts[p][1];
            pOutVerts[2].U = NextU;
            pOutVerts[2].V = 1 * f;
            pOutVerts[2].Color = dwColor;

            pOutVerts[3].Point = SVerts[p][1];
            pOutVerts[3].U = NextU;
            pOutVerts[3].V = 0;
            pOutVerts[3].Color = dwPrevColor;

            PlaneRot = NextPlaneRot;
            Axis = NextAxis;
            U = NextU;
            pOutVerts += 4;
            NumPrims++;
        }*/

        appMemcpy(SVerts, EVerts, sizeof(SVerts));

        }
           
        dwPrevColor = dwColor;
        SecStart = SecEnd;
    }

    // draw vert buffer //
    if (pEmitter->mMeshNodes[0] == NULL)
        pFrame->Viewport->RI->DrawDynQuads( NumPrims );

    unguard;
}

