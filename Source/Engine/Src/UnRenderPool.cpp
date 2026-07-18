/*=============================================================================
	UnRenderPool.cpp: Render resource pools.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"
#include "UnRenderPrivate.h"

//
//	FVertexPools
//

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,1)
#endif

struct FVertexPools
{
	FVertexPool*	FirstVertexPool GCC_PACK(1);

	FVertexPools()
	{
		FirstVertexPool = NULL;
	}

	~FVertexPools()
	{
		if(FirstVertexPool)
			delete FirstVertexPool;
	}
};

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

static FVertexPools	VertexPools;

//
//	FVertexPool::GetRevision
//

INT FVertexPool::GetRevision()
{
	check(Client);

	Revision = BaseClientRevision + Client->GetRevision();

	return Revision;
}

//
//	FVertexPool::SetClient
//

void FVertexPool::SetClient(FVertexStream* NewClient)
{
	guard(FVertexPool::SetClient);

	Client = NewClient;

	if(Client)
		BaseClientRevision = ++Revision;

	unguard;
}

//
//	GetVertexPool
//

FVertexPool* GetVertexPool(FVertexStream* Client)
{
	guard(GetVertexPool);

	// Find the vertex pool this client's currently attached to, or the smallest free vertex pool it will fit in.

	FVertexPool*	BestPool = NULL;
	INT				BestScore = MAXINT,
					Size = Client->GetSize();

	for(FVertexPool* Pool = VertexPools.FirstVertexPool;Pool;Pool = Pool->NextPool)
	{
		FVertexStream*	PoolClient = Pool->GetClient();
		INT				PoolSize = Pool->GetSize();

		if(PoolSize >= Size)
		{
			if(!PoolClient)
			{
				INT	Score = PoolSize - Size;

				if(!BestPool || Score < BestScore)
				{
					BestPool = Pool;
					BestScore = Score;
				}
			}
			else if(PoolClient == Client)
			{
				BestPool = Pool;
				break;
			}
		}
	}

	// If there isn't a free vertex pool that's large enough, create one.

	if(!BestPool)
		BestPool = VertexPools.FirstVertexPool = new(TEXT("VertexPool")) FVertexPool(Size,VertexPools.FirstVertexPool);

	return BestPool;

	unguard;
}

//
//	UpdateVertexPools
//

void UpdateVertexPools(UViewport* Viewport)
{
	guard(UpdateVertexPools);

	// Expire unused vertex pools.

	FVertexPool*	PrevPool = NULL;

	for(FVertexPool* Pool = VertexPools.FirstVertexPool;Pool;)
	{
		Pool->SetClient(NULL);
		Pool->LifeTimeFrames--;

		if(Pool->LifeTimeFrames <= 0)
		{
			if(PrevPool)
				PrevPool->NextPool = Pool->NextPool;
			else
				VertexPools.FirstVertexPool = Pool->NextPool;

			Pool->NextPool = NULL;
			Viewport->RenDev->FlushResource(Pool->GetCacheId());
			delete Pool;

			Pool = PrevPool;
		}

		PrevPool = Pool;
		Pool = Pool ? Pool->NextPool : VertexPools.FirstVertexPool;
	}

	unguard;
}

