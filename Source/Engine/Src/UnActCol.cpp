/*=============================================================================
	UnActCol.cpp: Actor list collision code.
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

Design goal:
	To be self-contained. This collision code maintains its own collision hash
	table and doesn't know about any far-away data structures like the level BSP.

Revision history:
	* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnOctreePrivate.h"

/*-----------------------------------------------------------------------------
	FCollisionHash.
-----------------------------------------------------------------------------*/

#define COMPILE_TESTER	(0)
#define COMPILE_NAIVE	(0)

//
// A collision hash table.
//
class ENGINE_API FCollisionHash : public FCollisionHashBase
{
public:
	// FCollisionHashBase interface.
	FCollisionHash();
	~FCollisionHash();
	void Tick();
	void AddActor( AActor *Actor );
	void RemoveActor( AActor *Actor );
	FCheckResult* ActorLineCheck( FMemStack& Mem, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags, DWORD ExtraNodeFlags, AActor *SourceActor );
	FCheckResult* ActorPointCheck( FMemStack& Mem, const FVector& Location, const FVector& Extent, DWORD TraceFlags, DWORD ExtraNodeFlags, UBOOL bSingleResult=0 );
	FCheckResult* ActorRadiusCheck( FMemStack& Mem, const FVector& Location, FLOAT Radius, DWORD ExtraNodeFlags );
	FCheckResult* ActorEncroachmentCheck( FMemStack& Mem, AActor* Actor, FVector Location, FRotator Rotation, DWORD TraceFlags, DWORD ExtraNodeFlags );
	FCheckResult* ActorOverlapCheck( FMemStack& Mem, AActor* Actor, FBox* Box, UBOOL bBlockKarmaOnly);
	void CheckActorNotReferenced( AActor* Actor );
	void CheckIsEmpty();
	void CheckActorLocations(ULevel* level);

	// Constants.
	enum { BULK_ALLOC  = 1024              };
	enum { NUM_BUCKETS = 16384             };
	enum { BASIS_BITS  = 10                };
	enum { GRAN_XY     = 256               };
	enum { GRAN_Z      = 256               };

	// Linked list item.
	struct FCollisionLink
	{
		// Varibles.
		AActor*          Actor;     // The actor.
		FCollisionLink*  Next;      // Next link belonging to this collision bucket.
		INT				 iLocation; // Based hash location.
	} *Hash[NUM_BUCKETS], *Available;
	TArray<FCollisionLink*> LinksToFree;

	FLOAT			ZE_SNF_PrimMillisec, 
					ZE_MNF_PrimMillisec, 
					NZE_SNF_PrimMillisec, 
					NZE_MNF_PrimMillisec;
	DWORD			ZE_SNF_PrimCount, 
					ZE_MNF_PrimCount, 
					NZE_SNF_PrimCount, 
					NZE_MNF_PrimCount;

	// Statics.
	static UBOOL Inited;
	static INT CollisionTag;
	static INT HashX[NUM_BUCKETS];
	static INT HashY[NUM_BUCKETS];
	static INT HashZ[NUM_BUCKETS];

	// Implementation.
	void GetActorExtent( AActor* Actor, INT& iX0, INT& iX1, INT& iY0, INT& iY1, INT& iZ0, INT& iZ1 );
	void GetHashIndices( FVector Location, INT& iX, INT& iY, INT& iZ )
	{
		iX = Clamp<INT>(appRound( (Location.X + HALF_WORLD_MAX) * (1.f/GRAN_XY) ), 0, NUM_BUCKETS-1 );
		iY = Clamp<INT>(appRound( (Location.Y + HALF_WORLD_MAX) * (1.f/GRAN_XY) ), 0, NUM_BUCKETS-1 );
		iZ = Clamp<INT>(appRound( (Location.Z + HALF_WORLD_MAX) * (1.f/GRAN_Z ) ), 0, NUM_BUCKETS-1 );
	}
	struct FCollisionLink*& GetHashLink( INT iX, INT iY, INT iZ, INT& iLocation )
	{
		iLocation = iX + (iY << BASIS_BITS) + (iZ << (BASIS_BITS*2));
		return Hash[ HashX[iX] ^ HashY[iY] ^ HashZ[iZ] ];
	}

private:
	FLOAT DistanceToHashPlane(INT X, FLOAT Dir, FLOAT Pos, INT Gran);
};

#if COMPILE_NAIVE
/* ----------------------------------------------------------------------------- */
/* ----------------------------------------------------------------------------- */
/* ------------------------------ Naive (slow) Hash -----------------------------*/
/* ------------------------ FOR DEBUGGING/TESTING ONLY! ------------------------ */
/* ----------------------------------------------------------------------------- */

class ENGINE_API FCollisionNaive : public FCollisionHashBase
{
public:
	INT					CollisionTag;
	TArray<AActor*>		Actors;

	FCollisionNaive() { }
	virtual ~FCollisionNaive() { }
	virtual void Tick() { }

	virtual void AddActor( AActor *Actor )
	{
		guard(FCollisionNaive::AddActor);
		Actors.AddItem(Actor);
		unguard;
	}

	virtual void RemoveActor( AActor *Actor )
	{
		guard(FCollisionNaive::RemoveActor);
		Actors.RemoveItem(Actor);
		unguard;
	}

	virtual FCheckResult* ActorLineCheck( FMemStack& Mem, FVector End, FVector Start, FVector Extent, DWORD TraceFlags, DWORD ExtraNodeFlags, AActor *SourceActor )
	{
		guard(FCollisionNaive::ActorLineCheck);

		FCheckResult* Result = NULL;

		for(INT i=0; i<Actors.Num(); i++)
		{
			AActor* testActor = Actors(i);

			FCheckResult Hit(0);
			UBOOL blockThisTrace = ( (Extent.IsZero() && testActor->bBlockZeroExtentTraces) || 
				(!Extent.IsZero() && testActor->bBlockNonZeroExtentTraces) );

			if( blockThisTrace
				&& testActor != SourceActor
				&& !SourceActor->IsOwnedBy(testActor) 
				&& testActor->ShouldTrace(SourceActor, TraceFlags)
				&& testActor->GetPrimitive()->LineCheck( Hit, testActor, End, Start, Extent, ExtraNodeFlags, TraceFlags )==0 )
			{
				FCheckResult* NewResult = new(Mem)FCheckResult(Hit);
				NewResult->GetNext() = Result;
				Result = NewResult;
				if ( TraceFlags & TRACE_StopAtFirstHit )
					break;
			}
		}

		if ( Result && (TraceFlags & TRACE_StopAtFirstHit) )
		{
			return Result;
		}
		else if ( Result && (TraceFlags & TRACE_SingleResult) )
		{
			return FindFirstResult(Result, TraceFlags);
		}

		return Result;

		unguard;
	}

	virtual FCheckResult* ActorPointCheck( FMemStack& Mem, const FVector& Location, const FVector& Extent, DWORD TraceFlags, DWORD ExtraNodeFlags, UBOOL bSingleResult=0 )
	{
		guard(FCollisionNaive::ActorPointCheck);

		FCheckResult* Result = NULL;

		for(INT i=0; i<Actors.Num(); i++)
		{
			AActor* testActor = Actors(i);

			if(	testActor->bBlockNonZeroExtentTraces
				&&	testActor->ShouldTrace(NULL, TraceFlags) )
			{
				FCheckResult TestHit(1.f);
				if( testActor->GetPrimitive()->PointCheck( TestHit, testActor, Location, Extent, 0 )==0 )
				{
					check(TestHit.Actor==testActor);
					FCheckResult* New = new(GMem)FCheckResult(TestHit);
					New->GetNext() = Result;
					Result = New;
					if ( bSingleResult )
						return Result;
				}
			}
		}

		return Result;
		unguard;
	}

	virtual FCheckResult* ActorRadiusCheck( FMemStack& Mem, const FVector& Location, FLOAT Radius, DWORD ExtraNodeFlags )
	{
		guard(FCollisionNaive::ActorRadiusCheck);

		FCheckResult* Result = NULL;
		FLOAT RadiusSq = Radius * Radius;

		for(INT i=0; i<Actors.Num(); i++)
		{
			AActor* testActor = Actors(i);

			FBox box = testActor->GetPrimitive()->GetCollisionBoundingBox( testActor );
			FVector center = box.GetCenter();
			if( (center - Location).SizeSquared() < RadiusSq )
			{
				FCheckResult* New = new(GMem)FCheckResult;
				New->Actor = testActor;
				New->GetNext() = Result;
				Result = New;
			}
		}

		return Result;
		unguard;
	}

	virtual FCheckResult* ActorEncroachmentCheck( FMemStack& Mem, AActor* Actor, FVector Location, FRotator Rotation, DWORD TraceFlags, DWORD ExtraNodeFlags )
	{
		guard(FCollisionNaive::ActorEncroachmentCheck);

		FCheckResult* Result = NULL;

		for(INT i=0; i<Actors.Num(); i++)
		{
			AActor* testActor = Actors(i);

			Exchange( Location, Actor->Location );
			Exchange( Rotation, Actor->Rotation );

			if(	!testActor->IsJoinedTo(Actor) &&
				testActor->ShouldTrace(Actor,TraceFlags) &&
				!(Actor->IsEncroacher() && testActor->bWorldGeometry) )
			{
				testActor->CollisionTag = CollisionTag;

				FCheckResult TestHit(1.f);
				if(Actor->IsOverlapping(testActor, &TestHit))
				{
					TestHit.Actor     = testActor;
					TestHit.Primitive = NULL;

					FCheckResult* New = new(GMem)FCheckResult(TestHit);
					New->GetNext() = Result;
					Result = New;
				}
			}

			Exchange( Location, Actor->Location );
			Exchange( Rotation, Actor->Rotation );
		}

		return Result;
		unguard;
	}

	virtual FCheckResult* ActorOverlapCheck( FMemStack& Mem, AActor* Actor, FBox* Box, UBOOL bBlockKarmaOnly)
	{
		return NULL;
	}


	virtual void CheckActorNotReferenced( AActor* Actor ) { }
	virtual void CheckIsEmpty() { }
	virtual void CheckActorLocations(ULevel* level) { }
};
#endif // COMPILE_NAIVE

#if COMPILE_TESTER

/* ----------------------------------------------------------------------------- */
/* ----------------------------------------------------------------------------- */
/* ------------------------------ Hash vs Octree comparer -----------------------*/
/* ----------------------------------------------------------------------------- */
/* ----------------------------------------------------------------------------- */

#define HASH_SHOW_DIFFERENCES	(1)
#define	REPEAT_FAILED_TESTS		(1)
#define TEST_WITH_NAIVE			(0)

#if HASH_SHOW_DIFFERENCES
static void OutputResult(FCheckResult* Result)
{
	guard(OutputResult);

	if(!Result)
	{
		debugf(TEXT("    EMPTY"));
		return;
	}

	while(Result)
	{
		if(Result->Actor)
			debugf(TEXT("    ACT: %s TIM: %f"), Result->Actor->GetName(), Result->Time);
		else
			debugf(TEXT("    ACT: NONE TIM: %f"), Result->Time);

		debugf(TEXT("    LOC: %f, %f, %f NOR: %f, %f, %f"), 
			Result->Location.X, Result->Location.Y, Result->Location.Z, 
			Result->Normal.X, Result->Normal.Y, Result->Normal.Z);

		Result = Result->GetNext();
	}

	unguard;
}

#define COMPARE_TOL (1.e-4f)

static inline UBOOL FloatIsEqual(FLOAT a, FLOAT b)
{
	return Abs(a-b) < COMPARE_TOL;
}

static inline UBOOL VectorIsEqual(FVector &a, FVector &b)
{
	return FloatIsEqual(a.X, b.X) && FloatIsEqual(a.Y, b.Y) && FloatIsEqual(a.Z, b.Z);
}

// Compare result, and print out if different.
// Returns TRUE if results are different.
// onlyActor indicates whether results should only compare 'Actor's, or all data.
static UBOOL CheckResults(FCheckResult* BenchResult, FCheckResult* OctreeResult, TCHAR* Name, UBOOL onlyActor, DWORD TraceFlags)
{
	guard(OutputResult);

	UBOOL isDifferent = 0;

	FCheckResult* thr = BenchResult;
	FCheckResult* tor = OctreeResult;

	INT numHR=0, numOR=0;

	// First - count results
	while(thr) { numHR++; thr = thr->GetNext(); }
	while(tor) { numOR++; tor = tor->GetNext(); }

	// If numbers of results are different...
	if(numHR != numOR)
		isDifferent = 1;

	// Then check each result in the hash results against each result in the octree results.
	thr = BenchResult;
	while( thr && !isDifferent && !(TraceFlags & TRACE_StopAtFirstHit) )
	{	
		UBOOL foundMatch = 0;
		tor = OctreeResult;
		while(tor && !foundMatch)
		{
			if(onlyActor)
			{
				if(thr->Actor == tor->Actor)
					foundMatch = 1;
			}
			//	If we are only looking for one result, then we might gets different 
			//	actors with the same time, but thats ok.
			else if(TraceFlags & TRACE_SingleResult)
			{
				if(FloatIsEqual(thr->Time, tor->Time))
					foundMatch = 1;
			}
			else
			{
				if(	thr->Actor		== tor->Actor &&
					thr->Primitive	== tor->Primitive && 
					VectorIsEqual(thr->Location, tor->Location) &&
					VectorIsEqual(thr->Normal, tor->Normal) &&
					FloatIsEqual(thr->Time, tor->Time) )
					foundMatch = 1;
			}

			tor = tor->GetNext();
		}

		if(!foundMatch)
			isDifferent = 1;

		thr = thr->GetNext();
	}

	if(isDifferent)
	{
		debugf(Name);
#if TEST_WITH_NAIVE
		debugf(TEXT("  NAIVE RESULT:"));
		OutputResult(BenchResult);
#else
		debugf(TEXT("  HASH RESULT:"));
		OutputResult(BenchResult);
#endif

		debugf(TEXT("  OCTREE RESULT:"));
		OutputResult(OctreeResult);
		return 1;
	}
	else
		return 0;

	unguard;
}
#endif

// Test function that creates both Octree and Hash and checks results.
class ENGINE_API FCollisionTester : public FCollisionHashBase
{
public:
	class FCollisionHashBase*	Bench;
	class FCollisionHashBase*		Octree;

	// 0 = Hash, 1 = Octree
	FLOAT AddMillisec[2];
	FLOAT RemoveMillisec[2];
	FLOAT NZELineCheckMillisec[2];
	FLOAT ZELineCheckMillisec[2];
	FLOAT PointCheckMillisec[2];
	FLOAT EncroachCheckMillisec[2];
	FLOAT RadiusCheckMillisec[2];

#if REPEAT_FAILED_TESTS
	// For repeating tests that failed.
	TArray<FVector>		ODEnd;
	TArray<FVector>		ODStart;
	TArray<FVector>		ODExtent;
	TArray<DWORD>		ODFlags;
	//AActor*				ODActor;

	TArray<FBox>		ODPoint1;
	TArray<FBox>		ODPoint2;
#endif

	FCollisionTester()
	{
		guard(FCollisionTester::FCollisionTester);
#if TEST_WITH_NAIVE
		Bench = new(TEXT("Collision Naive")) FCollisionNaive;
#else
		Bench = new(TEXT("Collision Hash")) FCollisionHash;
#endif

		Octree = new(TEXT("Collision Octree")) FCollisionOctree;
		//Octree = new(TEXT("Collision Octree")) FCollisionHash;

		AddMillisec[0] = AddMillisec[1] = 0;
		RemoveMillisec[0] =	RemoveMillisec[1] =	0;
		NZELineCheckMillisec[0] = NZELineCheckMillisec[1] =	0;
		ZELineCheckMillisec[0] = ZELineCheckMillisec[1] = 0;
		PointCheckMillisec[0] =	PointCheckMillisec[1] =	0;
		EncroachCheckMillisec[0] = EncroachCheckMillisec[1] = 0;
		RadiusCheckMillisec[0] = RadiusCheckMillisec[1] = 0;

		unguard;
	}

	virtual ~FCollisionTester()
	{
		guard(FCollisionTester::~FCollisionTester);
		delete Bench;
		delete Octree;

		debugf(TEXT(" -- COLLISION TESTER PROFILE OVERALL STATS -- "));
		debugf(TEXT("Add: B:%f O:%f"), AddMillisec[0], AddMillisec[1] );
		debugf(TEXT("Remove: B:%f O:%f"), RemoveMillisec[0], RemoveMillisec[1] );
		debugf(TEXT("NZELineCheck: B:%f O:%f"), NZELineCheckMillisec[0], NZELineCheckMillisec[1] );
		debugf(TEXT("ZELineCheck: B:%f O:%f"), ZELineCheckMillisec[0], ZELineCheckMillisec[1] );
		debugf(TEXT("PointCheck: B:%f O:%f"), PointCheckMillisec[0], PointCheckMillisec[1] );
		debugf(TEXT("EncroachCheck: B:%f O:%f"), EncroachCheckMillisec[0], EncroachCheckMillisec[1] );
		debugf(TEXT("RadiusCheck: B:%f O:%f"), RadiusCheckMillisec[0], RadiusCheckMillisec[1] );

		unguard;
	}

	virtual void Tick()
	{
		guard(FCollisionTester::Tick);

#if REPEAT_FAILED_TESTS
		// Repeat any tests that failed.
		FMemMark Mark(GMem);
		for(INT i=0; i<ODStart.Num(); i++)
		{
			//Octree->ActorLineCheck(GMem, ODEnd(i), ODStart(i), ODExtent(i), ODFlags(i), 0, ODActor);
			GTempLineBatcher->AddLine(ODStart(i), ODEnd(i), FColor(0, 0, 255));
			GTempLineBatcher->AddBox(FBox(ODStart(i)-ODExtent(i), ODStart(i)+ODExtent(i)), FColor(255,128,128));
			GTempLineBatcher->AddBox(FBox(ODEnd(i)-ODExtent(i), ODEnd(i)+ODExtent(i)), FColor(128,128,255));
			GTempLineBatcher->AddLine(ODStart(i), FVector(0, 0, 0), FColor(0, 0, 255));
		}

		for(INT i=0; i<ODPoint1.Num(); i++)
			GTempLineBatcher->AddBox(ODPoint1(i), FColor(50, 50, 255));

		for(INT i=0; i<ODPoint2.Num(); i++)
			GTempLineBatcher->AddBox(ODPoint2(i), FColor(255, 50, 50));

		Mark.Pop();
#endif

		Bench->Tick();
		Octree->Tick();

		unguard;
	}

	virtual void AddActor( AActor *Actor )
	{
		guard(FCollisionTester::AddActor);

		if(Actor->bStatic == 0)
		//if(1)
		{
			DWORD Time = 0;
			clock(Time);
			Bench->AddActor(Actor);
			unclock(Time);
			AddMillisec[0] += Time * GSecondsPerCycle * 1000.0f;

			Time = 0;
			clock(Time);
			Octree->AddActor(Actor);
			unclock(Time);
			AddMillisec[1] += Time * GSecondsPerCycle * 1000.0f;
		}
		else
		{
			Bench->AddActor(Actor);
			Octree->AddActor(Actor);
		}

		unguard;
	}

	virtual void RemoveActor( AActor *Actor )
	{
		guard(FCollisionTester::RemoveActor);

		if(Actor->bStatic == 0)
		//if(1)
		{
			DWORD Time = 0;
			clock(Time);
			Bench->RemoveActor(Actor);
			unclock(Time);
			RemoveMillisec[0] += Time * GSecondsPerCycle * 1000.0f;

			Time = 0;
			clock(Time);
			Octree->RemoveActor(Actor);
			unclock(Time);
			RemoveMillisec[1] += Time * GSecondsPerCycle * 1000.0f;
		}
		else
		{
			Bench->RemoveActor(Actor);
			Octree->RemoveActor(Actor);
		}

		unguard;
	}

	virtual FCheckResult* ActorLineCheck( FMemStack& Mem, FVector End, FVector Start, FVector Extent, DWORD TraceFlags, DWORD ExtraNodeFlags, AActor *SourceActor )
	{
		guard(FCollisionTester::ActorLineCheck);

		FCheckResult *BenchResult, *OctreeResult;
		DWORD Time = 0;

		if(Extent == FVector(0, 0, 0))
		{
			// We pick the order randomly, to avoid the one that goes second getting the cache warmed for it.
			if((FLOAT)appRand() / (FLOAT)RAND_MAX > 0.5f)
			{
				Time = 0;
				clock(Time);
				BenchResult = Bench->ActorLineCheck(Mem, End, Start, Extent, TraceFlags, ExtraNodeFlags, SourceActor);
				unclock(Time);
				ZELineCheckMillisec[0] += Time * GSecondsPerCycle * 1000.0f;

				Time = 0;
				clock(Time);
				OctreeResult = Octree->ActorLineCheck(Mem, End, Start, Extent, TraceFlags, ExtraNodeFlags, SourceActor);
				unclock(Time);
				ZELineCheckMillisec[1] += Time * GSecondsPerCycle * 1000.0f;
			}
			else
			{
				Time = 0;
				clock(Time);
				OctreeResult = Octree->ActorLineCheck(Mem, End, Start, Extent, TraceFlags, ExtraNodeFlags, SourceActor);
				unclock(Time);
				ZELineCheckMillisec[1] += Time * GSecondsPerCycle * 1000.0f;

				Time = 0;
				clock(Time);
				BenchResult = Bench->ActorLineCheck(Mem, End, Start, Extent, TraceFlags, ExtraNodeFlags, SourceActor);
				unclock(Time);
				ZELineCheckMillisec[0] += Time * GSecondsPerCycle * 1000.0f;
			}
		}
		else
		{
			if((FLOAT)appRand() / (FLOAT)RAND_MAX > 0.5f)
			{
				Time = 0;
				clock(Time);
				BenchResult = Bench->ActorLineCheck(Mem, End, Start, Extent, TraceFlags, ExtraNodeFlags, SourceActor);
				unclock(Time);
				NZELineCheckMillisec[0] += Time * GSecondsPerCycle * 1000.0f;

				Time = 0;
				clock(Time);
				OctreeResult = Octree->ActorLineCheck(Mem, End, Start, Extent, TraceFlags, ExtraNodeFlags, SourceActor);
				unclock(Time);
				NZELineCheckMillisec[1] += Time * GSecondsPerCycle * 1000.0f;
			}
			else
			{
				Time = 0;
				clock(Time);
				OctreeResult = Octree->ActorLineCheck(Mem, End, Start, Extent, TraceFlags, ExtraNodeFlags, SourceActor);
				unclock(Time);
				NZELineCheckMillisec[1] += Time * GSecondsPerCycle * 1000.0f;

				Time = 0;
				clock(Time);
				BenchResult = Bench->ActorLineCheck(Mem, End, Start, Extent, TraceFlags, ExtraNodeFlags, SourceActor);
				unclock(Time);
				NZELineCheckMillisec[0] += Time * GSecondsPerCycle * 1000.0f;
			}
		}

		if(BenchResult) {int dfg = 0; dfg++;} // Crap to stop .NET moaning if HASH_SHOW_DIFFERENCES == 0

#if HASH_SHOW_DIFFERENCES
		TCHAR c[256];
		if(Extent == FVector(0,0,0))
		{
			if(SourceActor)
			{
				if(TraceFlags & TRACE_StopAtFirstHit)
					appSprintf(c, TEXT("ZE LINE (SAFH) - %s"), SourceActor->GetName());
				else if(TraceFlags & TRACE_SingleResult)
					appSprintf(c, TEXT("ZE LINE (SR) - %s"), SourceActor->GetName());
				else
					appSprintf(c, TEXT("ZE LINE - %s"), SourceActor->GetName());
			}
			else
				appSprintf(c, TEXT("ZE LINE - NO ACTOR"));
		}
		else
		{
			if(SourceActor)
			{
				if(TraceFlags & TRACE_StopAtFirstHit)
					appSprintf(c, TEXT("NZE LINE (SAFH) - %s"), SourceActor->GetName());
				else if(TraceFlags & TRACE_SingleResult)
					appSprintf(c, TEXT("NZE LINE (SR) - %s"), SourceActor->GetName());
				else
					appSprintf(c, TEXT("NZE LINE - %s"), SourceActor->GetName());
			}
			else
				appSprintf(c, TEXT("NZE LINE - NO ACTOR"));
		}

		if(CheckResults(BenchResult, OctreeResult, c, 0, TraceFlags))
		{
#if REPEAT_FAILED_TESTS
			ODStart.AddItem(Start);
			ODEnd.AddItem(End);
			ODFlags.AddItem(TraceFlags);
			ODExtent.AddItem(Extent);
			//ODActor = SourceActor->Owner;
#endif
		}
#endif
		return OctreeResult;
		unguard;
	}

	virtual FCheckResult* ActorPointCheck( FMemStack& Mem, FVector Location, FVector Extent, DWORD TraceFlags, DWORD ExtraNodeFlags, UBOOL bSingleResult=0 )
	{
		guard(FCollisionTester::ActorPointCheck);

		DWORD Time = 0;
		clock(Time);
		FCheckResult* BenchResult = Bench->ActorPointCheck(Mem, Location, Extent, TraceFlags, ExtraNodeFlags, bSingleResult);
		unclock(Time);
		PointCheckMillisec[0] += Time * GSecondsPerCycle * 1000.0f;
		
		Time = 0;
		clock(Time);
		FCheckResult* OctreeResult = Octree->ActorPointCheck(Mem, Location, Extent, TraceFlags, ExtraNodeFlags, bSingleResult);
		unclock(Time);
		PointCheckMillisec[1] += Time * GSecondsPerCycle * 1000.0f;

		if(BenchResult) {int dfg = 0; dfg++;}

#if HASH_SHOW_DIFFERENCES
		CheckResults(BenchResult, OctreeResult, TEXT("POINT"), 0, TraceFlags);
#endif
		return OctreeResult;
		unguard;
	}

	virtual FCheckResult* ActorRadiusCheck( FMemStack& Mem, FVector Location, FLOAT Radius, DWORD ExtraNodeFlags )
	{
		guard(FCollisionTester::ActorRadiusCheck);

		DWORD Time = 0;
		clock(Time);
		FCheckResult* BenchResult = Bench->ActorRadiusCheck(Mem, Location, Radius, ExtraNodeFlags);
		unclock(Time);
		RadiusCheckMillisec[0] += Time * GSecondsPerCycle * 1000.0f;
		
		Time = 0;
		clock(Time);
		FCheckResult* OctreeResult = Octree->ActorRadiusCheck(Mem, Location, Radius, ExtraNodeFlags);
		unclock(Time);
		RadiusCheckMillisec[1] += Time * GSecondsPerCycle * 1000.0f;

		if(BenchResult) {int dfg = 0; dfg++;}

#if HASH_SHOW_DIFFERENCES
		CheckResults(BenchResult, OctreeResult, TEXT("RADIUS"), 1, 0);
#endif
		return OctreeResult;
		unguard;
	}

	virtual FCheckResult* ActorEncroachmentCheck( FMemStack& Mem, AActor* Actor, FVector Location, FRotator Rotation, DWORD TraceFlags, DWORD ExtraNodeFlags )
	{
		guard(FCollisionTester::ActorEncroachmentCheck);

		DWORD Time = 0;
		clock(Time);
		FCheckResult* BenchResult = Bench->ActorEncroachmentCheck(Mem, Actor, Location, Rotation, TraceFlags, ExtraNodeFlags);
		unclock(Time);
		EncroachCheckMillisec[0] += Time * GSecondsPerCycle * 1000.0f;
		
		Time = 0;
		clock(Time);
		FCheckResult* OctreeResult = Octree->ActorEncroachmentCheck(Mem, Actor, Location, Rotation, TraceFlags, ExtraNodeFlags);
		unclock(Time);
		EncroachCheckMillisec[1] += Time * GSecondsPerCycle * 1000.0f;

		if(BenchResult) {int dfg = 0; dfg++;}

#if HASH_SHOW_DIFFERENCES
		TCHAR c[256];
		if(Actor)
			appSprintf(c, TEXT("ENCROACH - %s"), Actor->GetName());
		else
			appSprintf(c, TEXT("ENCROACH - NO ACTOR"));
		if(CheckResults(BenchResult, OctreeResult, c, 0, TraceFlags))
		{
#if REPEAT_FAILED_TESTS
			ODPoint1.AddItem(Actor->GetPrimitive()->GetCollisionBoundingBox(Actor));

			FCheckResult* cr = BenchResult;
			while(cr)
			{
				ODPoint2.AddItem(cr->Actor->GetPrimitive()->GetCollisionBoundingBox(cr->Actor));
				cr = cr->GetNext();
			}
#endif
		}
#endif
		return OctreeResult;
		unguard;
	}

	virtual FCheckResult* ActorOverlapCheck( FMemStack& Mem, AActor* Actor, FBox* Box, UBOOL bBlockKarmaOnly)
	{
		return NULL;
	}

	virtual void CheckActorNotReferenced( AActor* Actor )
	{
		guard(FCollisionTester::CheckActorNotReferenced);
		Bench->CheckActorNotReferenced(Actor);
		Octree->CheckActorNotReferenced(Actor);
		unguard;
	}

	virtual void CheckIsEmpty()
	{
		guard(FCollisionTester::CheckActorNotReferenced);
		Bench->CheckIsEmpty();
		Octree->CheckIsEmpty();
		unguard;
	}

	virtual void CheckActorLocations(ULevel* level)
	{
		guard(FCollisionTester::CheckActorLocations);
		Bench->CheckActorLocations(level);
		Octree->CheckActorLocations(level);
		unguard;
	}
};

#endif // COMPILE_TESTER

/* ----------------------------------------------------------------------------- */
/* ----------------------------------------------------------------------------- */
/* ----------------------------------------------------------------------------- */
/* ----------------------------------------------------------------------------- */
/* ----------------------------------------------------------------------------- */

ENGINE_API FCollisionHashBase* GNewCollisionHash()
{
	guard(GNewCollisionHash);

	// Octree causes problems in Editor because of Undeo corrupting each Actors
	// list of OctreeNodes. JTODO: Fix!
	//if(GIsEditor)
	//return new(TEXT("FCollisionHash"))FCollisionHash;
	//else
	//{
		return new(TEXT("FCollisionOctree"))FCollisionOctree;
		//return new(TEXT("FCollisionHash"))FCollisionHash;
		//return new(TEXT("FCollisionTester"))FCollisionTester;
	//}

	unguard;
}

/*-----------------------------------------------------------------------------
	FCollisionHash statics.
-----------------------------------------------------------------------------*/

#define TIME_HASH (0)

// FCollisionHash statics.
UBOOL FCollisionHash::Inited=0;
INT FCollisionHash::CollisionTag=0;
INT FCollisionHash::HashX[NUM_BUCKETS];
INT FCollisionHash::HashY[NUM_BUCKETS];
INT FCollisionHash::HashZ[NUM_BUCKETS];	

// Global statistics.
static INT GActorsAdded=0, GFragsAdded=0, GUsed=0, GChecks=0;

static DWORD GHashBytesUsed = 0;

/*-----------------------------------------------------------------------------
	FCollisionHash init/exit.
-----------------------------------------------------------------------------*/

//
// Initialize the actor collision information.
//
FCollisionHash::FCollisionHash()
: Available( NULL )
, LinksToFree(  )
{
	guard(FCollisionHash::FCollisionHash);

	// Initialize static collision basis tables if necessary.
	if( !Inited )
	{
		Inited = 1;
		for( INT i=0; i<NUM_BUCKETS; i++ )
		{
			HashX[i] = HashY[i] = HashZ[i] = i;
		}
		for( INT i=0; i<NUM_BUCKETS; i++ )
		{
			Exchange( HashX[i], HashX[appRand() % NUM_BUCKETS] );
			Exchange( HashY[i], HashY[appRand() % NUM_BUCKETS] );
			Exchange( HashZ[i], HashZ[appRand() % NUM_BUCKETS] );
		}
	}

	// Init hash table.
	for( INT i=0; i<NUM_BUCKETS; i++ )
		Hash[i] = NULL;

	ZE_SNF_PrimMillisec = 0;
	ZE_MNF_PrimMillisec = 0;
	NZE_SNF_PrimMillisec = 0;
	NZE_MNF_PrimMillisec = 0;
	ZE_SNF_PrimCount = 0;
	ZE_MNF_PrimCount = 0;
	NZE_SNF_PrimCount = 0;
	NZE_MNF_PrimCount = 0;

	GHashBytesUsed = sizeof(FCollisionHash) + (3 * NUM_BUCKETS * sizeof(INT));

	unguard;
}

//
// Shut down the actor collision information.
//
FCollisionHash::~FCollisionHash()
{
	guard(FCollisionHash::~FCollisionHash);

	// Free all collision links.
	for( INT i=0; i<LinksToFree.Num(); i++ )
		appFree( LinksToFree(i) );

#if TIME_HASH
	debugf(TEXT(" -- Hash PrimTest -- "));
	debugf(TEXT("  ZE: SNF: %f (%d) MNF: %f (%d)"), ZE_SNF_PrimMillisec, ZE_SNF_PrimCount, ZE_MNF_PrimMillisec, ZE_MNF_PrimCount);
	debugf(TEXT(" NZE: SNF: %f (%d) MNF: %f (%d)"), NZE_SNF_PrimMillisec, NZE_SNF_PrimCount, NZE_MNF_PrimMillisec, NZE_MNF_PrimCount);
	debugf(TEXT(" Mem Used: %d bytes"), GHashBytesUsed);
#endif
	unguard;
}

/*-----------------------------------------------------------------------------
	FCollisionHash tick - clean up collision info.
-----------------------------------------------------------------------------*/

//
// Cleanup the collision info.
//
void FCollisionHash::Tick()
{
	guard(FCollisionHash::Tick);

	// All we do here is stats.
	//debugf(NAME_Log,"Used=%i Added=%i Frags=%i Checks=%i",GUsed,GActorsAdded,GFragsAdded,GChecks);
	GActorsAdded = GFragsAdded = GChecks = 0;

	unguard;
}

/*-----------------------------------------------------------------------------
	FCollisionHash extent.
-----------------------------------------------------------------------------*/

//
// Compute the extent of an actor in hash coordinates.
//
void FCollisionHash::GetActorExtent
(
	AActor *Actor,
	INT &X0, INT &X1, INT &Y0, INT &Y1, INT &Z0, INT &Z1
)
{
	guard(FCollisionHash::GetActorExtent);

	// Get actor's bounding box.
	UPrimitive* prim = Actor->GetPrimitive();
	FBox Box = prim->GetCollisionBoundingBox(Actor);

	// Discretize to hash coordinates.
	GetHashIndices( Box.Min, X0, Y0, Z0 );
	GetHashIndices( Box.Max, X1, Y1, Z1 );

	unguard;
}

/*-----------------------------------------------------------------------------
	FCollisionHash adding/removing.
-----------------------------------------------------------------------------*/

//
// Add an actor to the collision info.
//
void FCollisionHash::AddActor( AActor *Actor )
{
	guard(FCollisionHash::AddActor);
	check(Actor->bCollideActors);
	if( Actor->bDeleteMe || Actor->bPendingDelete )
		return;
	CheckActorNotReferenced( Actor );
	GActorsAdded++;

	// Add actor in all the specified places.
	INT X0,Y0,Z0,X1,Y1,Z1;
	GetActorExtent( Actor, X0, X1, Y0, Y1, Z0, Z1 );
	for( INT X=X0; X<=X1; X++ )
	{
		for( INT Y=Y0; Y<=Y1; Y++ )
		{
			for( INT Z=Z0; Z<=Z1; Z++ )
			{
				INT iLocation;
				FCollisionLink*& Link = GetHashLink( X, Y, Z, iLocation );
				if( !Available )
				{
					// Allocate a lot of new links.
					Available = new(TEXT("FCollisionLink"))FCollisionLink[BULK_ALLOC];
					GHashBytesUsed += sizeof(FCollisionLink) * BULK_ALLOC;
					LinksToFree.AddItem( Available );
					for( INT i=0; i<BULK_ALLOC-1; i++ )
						Available[i].Next = &Available[i+1];
					Available[BULK_ALLOC-1].Next = NULL;
				}
				FCollisionLink* NewLink = Available;
				Available               = Available->Next;
				NewLink->Actor          = Actor;
				NewLink->Next           = Link;
				NewLink->iLocation      = iLocation;
				Link                    = NewLink;
				GUsed++;
				GFragsAdded++;
			}
		}
	}
	Actor->ColLocation = Actor->Location;
	unguard;
}

//
// Remove an actor from the collision info.
//
#define CHECK_REMOVE_LOOPS (1)

void FCollisionHash::RemoveActor( AActor* Actor )
{
	guard(FCollisionHash::RemoveActor);
	check(Actor->bCollideActors);
	if( Actor->bDeleteMe )
    {
		CheckActorNotReferenced( Actor );
		return;
    }
	if( Actor->Location!=Actor->ColLocation )
	{
		check(Actor->ColLocation == Actor->ColLocation); // make sure ColLocation not undefined
		if ( GIsEditor )
			debugf( TEXT("%s moved without proper hashing"), Actor->GetFullName() );
		else
			appErrorf( TEXT("%s moved without proper hashing"), Actor->GetFullName() );
	}

	// Remove actor.
	INT X0,Y0,Z0,X1,Y1,Z1;
	GetActorExtent( Actor, X0, X1, Y0, Y1, Z0, Z1 );
	for( INT X=X0; X<=X1; X++ )
	{
		for( INT Y=Y0; Y<=Y1; Y++ )
		{
			for( INT Z=Z0; Z<=Z1; Z++ )
			{
				INT iLocation;
				FCollisionLink** Link = &GetHashLink( X, Y, Z, iLocation );
#if CHECK_REMOVE_LOOPS
				int IterateCount = 0;
#endif
				while( *Link )
				{
#if CHECK_REMOVE_LOOPS
					if(GIsEditor)
					{
						IterateCount++;
						if(IterateCount > 10000)
						{
							appMsgf(0, TEXT("[FCollisionHash::RemoveActor] IterateCount Over 10000!") );
							continue;					
						}
					}
#endif
		
					if( (*Link)->Actor != Actor )
					{
						Link = &(*Link)->Next;
					}
					else
					{
						FCollisionLink* Scrap = *Link;
						*Link                 = (*Link)->Next;
						Scrap->Next           = Available;
						Available	          = Scrap;
						GUsed--;
					}
				}
			}
		}
	}
	CheckActorNotReferenced( Actor );
	unguard;
}

/*-----------------------------------------------------------------------------
	FCollisionHash collision checking.
-----------------------------------------------------------------------------*/

//
// Make a list of all actors which overlap with a cylinder at Location
// with the given collision size.
//
FCheckResult* FCollisionHash::ActorPointCheck
(
	FMemStack&		Mem,
	const FVector&	Location,
	const FVector&	Extent,
	DWORD			TraceFlags,
	DWORD			ExtraNodeFlags,
	UBOOL			bSingleResult
)
{
	guard(FCollisionHash::ActorPointCheck);
	FCheckResult* Result=NULL;

	// Get extent indices.
	INT X0,Y0,Z0,X1,Y1,Z1;
	GetHashIndices( Location - Extent, X0, Y0, Z0 );
	GetHashIndices( Location + Extent, X1, Y1, Z1 );
	CollisionTag++;

	// Check all actors in this neighborhood.
	for( INT X=X0; X<=X1; X++ ) for( INT Y=Y0; Y<=Y1; Y++ ) for( INT Z=Z0; Z<=Z1; Z++ )
	{
		INT iLocation;
		for( FCollisionLink* Link = GetHashLink( X, Y, Z, iLocation ); Link; Link=Link->Next )
		{
			// Skip if we've already checked this actor.
			if
			(	Link->Actor->bBlockNonZeroExtentTraces
			&&  Link->Actor->CollisionTag != CollisionTag
			&&	Link->iLocation           == iLocation 
			&&	Link->Actor->ShouldTrace(NULL,TraceFlags) )
			{
				// Collision test.
				Link->Actor->CollisionTag = CollisionTag;
				FCheckResult TestHit(1.f);
				if( Link->Actor->GetPrimitive()->PointCheck( TestHit, Link->Actor, Location, Extent, 0 )==0 )
				{
					check(TestHit.Actor==Link->Actor);
					FCheckResult* New = new(GMem)FCheckResult;
					*New = TestHit;
					New->GetNext() = Result;
					Result = New;
					if ( bSingleResult )
						return Result;
				}
			}
		}
	}
	return Result;
	unguard;
}

//
// Make a list of all actors which are within a given radius.
//
FCheckResult* FCollisionHash::ActorRadiusCheck
(
	FMemStack&		Mem,
	const FVector&	Location,
	FLOAT			Radius,
	DWORD			ExtraNodeFlags
)
{
	guard(FCollisionHash::ActorVisRadiusCheck);
	FCheckResult* Result=NULL;

	// Get extent indices.
	INT X0,Y0,Z0,X1,Y1,Z1;
	GetHashIndices( Location - FVector(Radius,Radius,Radius), X0, Y0, Z0 );
	GetHashIndices( Location + FVector(Radius,Radius,Radius), X1, Y1, Z1 );
	CollisionTag++;
	FLOAT RadiusSq = Radius * Radius;

	// Check all actors in this neighborhood.
	for( INT X=X0; X<=X1; X++ ) for( INT Y=Y0; Y<=Y1; Y++ ) for( INT Z=Z0; Z<=Z1; Z++ )
	{
		INT iLocation;
		for( FCollisionLink* Link = GetHashLink( X, Y, Z, iLocation ); Link; Link=Link->Next )
		{
			// Skip if we've already checked this actor.
			if
			(	Link->Actor->CollisionTag != CollisionTag 
			&&	Link->iLocation           == iLocation )
			{
				// Collision test.
				Link->Actor->CollisionTag = CollisionTag;

                FBox box = Link->Actor->GetPrimitive()->GetCollisionBoundingBox( Link->Actor );
                FVector center = box.GetCenter();
				if( (center - Location).SizeSquared() < RadiusSq )
				{
					FCheckResult* New = new(GMem)FCheckResult;
					New->Actor = Link->Actor;
					New->GetNext() = Result;
					Result = New;
				}

			}
		}
	}
	return Result;
	unguard;
}

//
// Check for encroached actors.
//
FCheckResult* FCollisionHash::ActorEncroachmentCheck
(
	FMemStack&		Mem,
	AActor*			Actor,
	FVector			Location,
	FRotator		Rotation,
	DWORD			TraceFlags, 
	DWORD			ExtraNodeFlags
)
{
	guard(FCollisionHash::ActorEncroachmentCheck);
	check(Actor!=NULL);

	// Save actor's location and rotation.
	Exchange( Location, Actor->Location );
	Exchange( Rotation, Actor->Rotation );

	// Get extent indices.
	INT X0,Y0,Z0,X1,Y1,Z1;
	GetActorExtent( Actor, X0, X1, Y0, Y1, Z0, Z1 );
	FCheckResult *Result, **PrevLink = &Result;
	CollisionTag++;

	// Check all actors in this neighborhood.
	for( INT X=X0; X<=X1; X++ ) for( INT Y=Y0; Y<=Y1; Y++ ) for( INT Z=Z0; Z<=Z1; Z++ )
	{
		INT iLocation;
		for( FCollisionLink* Link = GetHashLink( X, Y, Z, iLocation ); Link; Link=Link->Next )
		{
			// Actor is the non-cylinder thing that is moving (mover, karma etc.).
			// Link->Actor is the thing (Pawn, Volume, Projector etc.) that its moving into.

			// Skip if we've already checked this actor, or we're joined to the encroacher,
			// or this is an encroacher and the other thing is the world (static mesh, terrain etc.)
			if
			(	Link->Actor->CollisionTag != CollisionTag && 
				Link->iLocation           == iLocation &&
				!Link->Actor->IsJoinedTo(Actor) &&
				Link->Actor->ShouldTrace(Actor,TraceFlags) &&
				!(Actor->IsEncroacher() && Link->Actor->bWorldGeometry) )
			{
				Link->Actor->CollisionTag = CollisionTag;
				
				FCheckResult TestHit(1.f);
				if(Actor->IsOverlapping(Link->Actor, &TestHit))
				{
					TestHit.Actor     = Link->Actor;
					TestHit.Primitive = NULL;
					*PrevLink         = new(GMem)FCheckResult;
					**PrevLink        = TestHit;
					PrevLink          = &(*PrevLink)->GetNext();
				}
			}
		}
	}

	// Restore actor's location and rotation.
	Exchange( Location, Actor->Location );
	Exchange( Rotation, Actor->Rotation );

	*PrevLink = NULL;
	return Result;
	unguard;
}

FCheckResult* FCollisionHash::ActorOverlapCheck( FMemStack& Mem, AActor* Actor, FBox* Box, UBOOL bBlockKarmaOnly)
{
	return NULL;
}

//
// Make a time-sorted list of all actors which overlap a cylinder moving 
// along a line from Start to End. If LevelInfo is specified, also checks for
// collision with the level itself and terminates collision when the trace
// hits solid space.
//
//
// * FIXME Get rid of ExtraNodeFlags

FCheckResult* FCollisionHash::ActorLineCheck
(
	FMemStack&		Mem,
	const FVector&	End,
	const FVector&	Start,
	const FVector&	Size,
	DWORD			TraceFlags,
	DWORD			ExtraNodeFlags,
	AActor*			SourceActor
)
{
	guard(FCollisionHash::ActorLineCheck);
	FCheckResult* Result = NULL;
	CollisionTag++;

	if ( Size.IsZero() )
	{
		// raycast for zero extent traces
		INT X, Y, Z, EndX, EndY, EndZ;
		FVector Dir = End - Start;
		Dir = Dir.SafeNormal();
		GetHashIndices(Start,X,Y,Z);
		GetHashIndices(End,EndX,EndY,EndZ);
		INT bStillTracing = 1;
		while ( bStillTracing )
		{
			INT iLocation;
			for( FCollisionLink* Link = GetHashLink( X, Y, Z, iLocation ); Link; Link=Link->Next )
			{
				// Skip if we've already checked this actor.
				if
				(	Link->Actor->CollisionTag != CollisionTag
				&&	Link->iLocation           == iLocation )
				{
					// Check collision.
					FCheckResult Hit(0);
					Link->Actor->CollisionTag = CollisionTag;
					if( Link->Actor->bBlockZeroExtentTraces
						&& Link->Actor != SourceActor
						&& !SourceActor->IsOwnedBy(Link->Actor) 
						&& Link->Actor->ShouldTrace(SourceActor,TraceFlags) )
					{
#if TIME_HASH
						DWORD Time = 0;
						clock(Time);
#endif
						UBOOL lineChkRes = Link->Actor->GetPrimitive()->LineCheck( Hit, Link->Actor, End, Start, Size, ExtraNodeFlags, TraceFlags )==0;
#if TIME_HASH
						unclock(Time);

						if(Link->Actor->bWasSNFiltered)
						{
							ZE_SNF_PrimCount++;
							ZE_SNF_PrimMillisec += Time * GSecondsPerCycle * 1000.0f;
						}
						else
						{
							ZE_MNF_PrimCount++;
							ZE_MNF_PrimMillisec += Time * GSecondsPerCycle * 1000.0f;
						}

#endif
						if(lineChkRes)
						{
							FCheckResult* Link = new(Mem)FCheckResult(Hit);
							Link->GetNext() = Result;
							Result = Link;
							if ( TraceFlags & TRACE_StopAtFirstHit )
								break;
						}
					}
				}
			}
			if ( Result && (TraceFlags & TRACE_StopAtFirstHit) )
				return Result;
			else if ( Result && (TraceFlags & TRACE_SingleResult) )
			{
				INT Xt, Yt, Zt;
				GetHashIndices(Result->Location,Xt,Yt,Zt);
				if(Xt == X && Yt == Y && Zt == Z)
				{
					return FindFirstResult(Result, TraceFlags);
				}
			}
			bStillTracing = ( (X != EndX) || (Y != EndY) || (Z != EndZ) );

			if ( bStillTracing )
			{
				// find next hash box
				// figure out which plane is intersected first, and increment to next box in that direction
				FLOAT DistX = DistanceToHashPlane(X, Dir.X, Start.X, GRAN_XY);
				FLOAT DistY = DistanceToHashPlane(Y, Dir.Y, Start.Y, GRAN_XY);
				FLOAT DistZ = DistanceToHashPlane(Z, Dir.Z, Start.Z, GRAN_Z);

				INT I;

				if ( (DistX <= DistY) && (DistX <= DistZ) )
				{
					X = (Dir.X < 0) ? X-1 : X+1;
					I = X;
				}
				else if ( (DistY <= DistX) && (DistY <= DistZ) )
				{
					Y = (Dir.Y < 0) ? Y-1 : Y+1;
					I = Y;
				}
				else
				{
					Z = (Dir.Z < 0) ? Z-1 : Z+1;
					I = Z;
				}

				// check if beyond edge of world
				bStillTracing = ( (I < NUM_BUCKETS) && (I >= 0) );
			}
		}

	}
	else
	{
		// Get extent.
		INT X0,Y0,Z0,X1,Y1,Z1,X;
		FBox Box( FBox(0) + Start + End );
		GetHashIndices( Box.Min - Size, X0, Y0, Z0 );
		GetHashIndices( Box.Max + Size, X1, Y1, Z1 );
 
		// Check all potentially colliding actors in the hash.
		for( X=X0; X<=X1; X++ )
		{
			for( INT Y=Y0; Y<=Y1; Y++ )
			{
				for( INT Z=Z0; Z<=Z1; Z++ )
				{
					INT iLocation;
					for( FCollisionLink* Link = GetHashLink( X, Y, Z, iLocation ); Link; Link=Link->Next )
					{
						// Skip if we've already checked this actor.
						if
							(	Link->Actor->CollisionTag != CollisionTag
							&&	Link->iLocation           == iLocation )
						{
							// Check collision.
							FCheckResult Hit(0);
							Link->Actor->CollisionTag = CollisionTag;
							if( Link->Actor->bBlockNonZeroExtentTraces
								&& Link->Actor != SourceActor
								&& !SourceActor->IsOwnedBy(Link->Actor) 
								&& Link->Actor->ShouldTrace(SourceActor,TraceFlags) )
							{
								//debugf(TEXT("LC: %s - %s"), Link->Actor->GetName(), SourceActor->GetName());

#if TIME_HASH
								DWORD Time = 0;
								clock(Time);
#endif
								UBOOL lineChkRes = Link->Actor->GetPrimitive()->LineCheck( Hit, Link->Actor, End, Start, Size, ExtraNodeFlags, TraceFlags )==0;
#if TIME_HASH
								unclock(Time);

								if(Link->Actor->bWasSNFiltered)
								{
									NZE_SNF_PrimCount++;
									NZE_SNF_PrimMillisec += Time * GSecondsPerCycle * 1000.0f;
								}
								else
								{
									NZE_MNF_PrimCount++;
									NZE_MNF_PrimMillisec += Time * GSecondsPerCycle * 1000.0f;
								}

#endif

								if(lineChkRes)
								{
									FCheckResult* Link = new(Mem)FCheckResult(Hit);
									Link->GetNext() = Result;
									Result = Link;
									if ( TraceFlags & TRACE_StopAtFirstHit )
										return Result;
								}
							}
						}
					}
				}
			}
		}

		if(Result && TraceFlags & TRACE_SingleResult)
		{
			return FindFirstResult(Result, TraceFlags);
		}

	}
	return Result;
	unguard;
}

FLOAT FCollisionHash::DistanceToHashPlane(INT X, FLOAT Dir, FLOAT Pos, INT Gran)
{
	guard(FCollisionHash::DistanceToHashPlane);

	if ( Dir == 0.f )
		return 1000.f * (INT)GRAN_XY;	
	else
	{
		FLOAT Plane;
		if ( Dir > 0.f )
			Plane = (X + 0.5f) * Gran - HALF_WORLD_MAX;
		else
			Plane = (X - 0.5f) * Gran - HALF_WORLD_MAX;
		return (Plane - Pos)/Dir;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Checks.
-----------------------------------------------------------------------------*/

//
// Make sure the actor isn't referenced in the collision hash.
//
void FCollisionHash::CheckActorNotReferenced( AActor* Actor )
{
	guard(FCollisionHash::CheckActorNotReferenced);

	#if DO_CHECK_SLOW
	if( GIsSoaking && !GIsEditor )
		for( INT i=0; i<NUM_BUCKETS; i++ )
			for( FCollisionLink* Link=Hash[i]; Link; Link=Link->Next )
				if( Link->Actor == Actor )
					appErrorf( TEXT("%s has collision hash fragments"), Actor->GetFullName() );
	#endif

	unguardf(( TEXT("(%s)"), Actor->GetFullName() ));
}

void FCollisionHash::CheckIsEmpty()
{
	guard(FCollisionHash::CheckActorNotReferenced);
	// does nothing
	unguard;
}

void FCollisionHash::CheckActorLocations(ULevel *level)
{
	guard(FCollisionHash::CheckActorLocations);
	// does nothing
	unguard;
}


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

