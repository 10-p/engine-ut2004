//=============================================================================
// xParticleMgr - Particle System prototypes
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================


#ifndef xParticleMgr_H
#define xParticleMgr_H

#include "AxEmitter.h"

#include "UnForcePacking_begin.h"

#define MAX_VERTS_PER_NODE	12		// ie: a frame morphed sprite would be 12 verts
#define MAX_SYSTEMS			150
#define MAX_PARTICLES_PER	400		// change this in iterators also
#define RNG_HI 1
#define RNG_LO 0
const float	INV_255		=   1.0f/255.0f;
const float PCL_DEAD	=	0.0f;

extern float GParticleAtten;

struct DynamicVertex
{
	FVector	Point;
	DWORD	Color;
	FLOAT	U,V;
    enum { VF_Flags = VF_Position | VF_Diffuse | VF_Tex1 };
};

struct RangeT
{
	float min;
	float max;
	float GetRandom( void );
};

struct ParticleT // 64 bytes
{
    float		life;			// time this particle has been alive
    float		delay;			// life delay
	float		size;			// size
    float		invLifeSpan;	// inverse life span
	FVector		pos;			// position
	FVector		velocity;		// m/sec
	FColor		color;			// color	
	float		mass;
    float       spin;
	FRotator	rot;			// rotation
	char		rotInertia[3];	// rotational inertia
	BYTE		tileNum;		// tile number in particle texture
    short       nextIndex;      // next index for chained particles
};

struct ForceData
{
    FVector forceOrigin;
    float forceRadiusSqr;
    float invForceRadius;
    FVector velocityNormal;
    float forceScale;
    float forceVelocity;
    int forceType;
};

extern bool CalcForceData( ForceData& forceData, AActor* forceActor );
extern void CalcForceAtPoint( ForceData* forces, int numForces, FVector& pt, FVector& accumForce );

//class UParticleMesh;
// particle iterators!
// this controls of the update and drawing behaviour of the emitters
class ParticleIterator
{
public:
	virtual void Update( AxEmitter* pEmitter );
	virtual void Draw( AxEmitter*pSystem, FSceneNode* pFrame );
	virtual int GetVertexCount( AxEmitter*pSystem, ParticleT* p );
};

// iterator child classes
class SpriteIterator : public ParticleIterator
{
public:
	virtual void Draw( AxEmitter*pSystem, FSceneNode* pFrame );
	virtual int GetVertexCount( AxEmitter*pSystem, ParticleT* p );
};

class StreamIterator : public ParticleIterator
{
public:
	virtual void Draw( AxEmitter*pSystem, FSceneNode* pFrame );
	virtual int GetVertexCount( AxEmitter*pSystem, ParticleT* p );
};

class LineIterator : public ParticleIterator
{
public:
	virtual void Draw( AxEmitter*pSystem, FSceneNode* pFrame );
	virtual int GetVertexCount( AxEmitter*pSystem, ParticleT* p );
};

class DiscIterator : public ParticleIterator
{
public:
	virtual void Draw( AxEmitter*pSystem, FSceneNode* pFrame );
	virtual int GetVertexCount( AxEmitter*pSystem, ParticleT* p );
};

class MeshIterator : public ParticleIterator
{
public:
	virtual void Draw( AxEmitter*pSystem, FSceneNode* pFrame );
	virtual int GetVertexCount( AxEmitter*pSystem, ParticleT* p );
};

class BranchIterator : public ParticleIterator
{
public:
	virtual void Draw( AxEmitter*pSystem, FSceneNode* pFrame );
	virtual int GetVertexCount( AxEmitter*pSystem, ParticleT* p );
};

class BeamIterator : public ParticleIterator
{
public:
	virtual void Update( AxEmitter* pEmitter );
	virtual void Draw( AxEmitter*pSystem, FSceneNode* pFrame );
	virtual int GetVertexCount( AxEmitter*pSystem, ParticleT* p );
};

// hacking around pointer support
typedef TArray<ParticleT>*	PclArrayPtr;
typedef ParticleIterator*	PclIteratorPtr;

// gross particle pools
class ParticlePoolT
{
public:
	ParticlePoolT( PclArrayPtr pArray, ParticlePoolT* pN )
	{
		pParticleArray = pArray;
		pNext = pN;
	}
	PclArrayPtr			pParticleArray;
	ParticlePoolT*		pNext;
};

class ParticlePoolMgr
{
public:
	ParticlePoolMgr()
	{
		pHead = NULL;
		numPool = 0;
		poolMem = 0;
		peakMem = 0;
	}
	~ParticlePoolMgr()
	{
		Shutdown();
	}
	void Shutdown( void )
	{
		ParticlePoolT	*pCur = pHead;
		ParticlePoolT	*pNext = NULL;
		while ( pCur )
		{
			pNext = pCur->pNext;
			delete pCur->pParticleArray;
			delete pCur;
			pCur = pNext;
		}
		pHead = NULL;
		numPool = 0;
		poolMem = 0;
		peakMem = 0;
	}
	void AddArray( PclArrayPtr pNew )
	{
		pHead = new ParticlePoolT( pNew, pHead );
		numPool++;
		poolMem += pNew->Num() * sizeof( ParticleT );
		if ( poolMem > peakMem )
		{
			peakMem = poolMem;
			// debugf(TEXT("ParticlePoolMgr count: (%d) peak mem: (%dKB) curEmitters: (%d)"), numPool, peakMem/1024, UParticleMesh::msNumActiveSys );
		}
	}
	PclArrayPtr GetArray( int requested )
	{
		ParticlePoolT*	pPrev = NULL;
		ParticlePoolT*	pCur = pHead;
		PclArrayPtr		pArray = NULL;
		int				numChecked = 0;
		int				largestFound = 0;
		while ( pCur )
		{
			largestFound = Max( largestFound, pCur->pParticleArray->Num() );
			numChecked++;
			// also could check ArrayMax
			if ( requested <= pCur->pParticleArray->Num() ) // todo: make this have a tolerance like num > request-% num < request+%
			{
				pArray = pCur->pParticleArray;
				//debugf(TEXT("ParticlePoolMgr getting particle array(%d) poolMem(%d KB)"), pArray->Num() );
				if ( pPrev ) // fix list chain
				{
					pPrev->pNext = pCur->pNext;
				}
				else // this is the head
				{
					pHead = pCur->pNext;
				}
				delete pCur;
				numPool--;
				poolMem -= pArray->Num() * sizeof( ParticleT );
				return pArray;
			}
			pPrev = pCur;
			pCur = pCur->pNext;
		}
		return NULL;
	}
	ParticlePoolT*	pHead;
	int				numPool;
	int				poolMem;
	int				peakMem;
};

extern ParticlePoolMgr gPclPool; // global singleton

#include "UnForcePacking_end.h"

#endif//xParticleMgr_H
