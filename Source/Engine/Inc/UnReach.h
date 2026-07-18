/*=============================================================================
	UnReach.h: AI reach specs.
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Steven Polge 3/97
=============================================================================*/

#include "UnForcePacking_begin.h"

// OBSOLETE - replaced by UReachSpec (see UReachSpec.h)
class ENGINE_API FReachSpec
{

public:
	INT distance; 
	AActor *Start;
	AActor *End;			// actor at endpoint of this path (next waypoint or goal)
	INT CollisionRadius; 
    INT CollisionHeight; 
	INT reachFlags;			// see EReachSpecFlags definition in UnPath.h
	INT MaxLandingVelocity;
	BYTE  bPruned;
	FLOAT LeftWidth, RightWidth, RegionHeight; // size in each direction of reachable region defined by path

	friend FArchive& operator<< (FArchive &Ar, FReachSpec &ReachSpec )
	{
		guard(FReachSpec<<);
		Ar << ReachSpec.distance << ReachSpec.Start << ReachSpec.End;
		Ar << ReachSpec.CollisionRadius << ReachSpec.CollisionHeight;
		Ar << ReachSpec.reachFlags << ReachSpec.bPruned;
		return Ar;
		unguard;
	};
};

#include "UnForcePacking_end.h"

