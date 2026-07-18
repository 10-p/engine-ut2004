/*=============================================================================
	UnStaticMeshCollision.cpp: Static mesh collision code.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

#define USECOLLISIONMODEL 1

//
//	UStaticMesh::UseCylinderCollision.
//
UBOOL UStaticMesh::UseCylinderCollision( const AActor* Owner )
{
	guardSlow(UStaticMesh::UseCylinderCollision);
	return Owner->bUseCylinderCollision;
	unguardSlow;
}

//
//	UStaticMesh::GetCollisionBoundingBox
//
FBox UStaticMesh::GetCollisionBoundingBox(const AActor* Owner) const
{
	FBox Result;

	if( Owner->bUseCylinderCollision )
		Result = UPrimitive::GetCollisionBoundingBox(Owner);
	else
	{
		Result = BoundingBox.TransformBy(Owner->LocalToWorld());

		if(CollisionModel)
			Result += CollisionModel->GetCollisionBoundingBox(Owner);
	}

#ifdef WITH_KARMA
	// If this actor is bBlockKarma, when ensure its collision bounding box also includes 
	// the Karma primitive bounding box. This also keep the bounding box up to date.
	if(Owner->bBlockKarma)
	{
		McdModelID model = Owner->getKModel();
		if(model)
		{
			MeVector3 min, max;
			FVector umin, umax;
			McdModelGetAABB(model, min, max);
			KME2UPosition(&umin, min);
			KME2UPosition(&umax, max);
			Result += FBox(umin, umax);
		}
	}
#endif

	return Result;
}

//
//	UStaticMesh::LineCheck
//
UBOOL UStaticMesh::LineCheck(FCheckResult& Result,AActor* Owner,const FVector& End,const FVector& Start,const FVector& Extent,DWORD ExtraNodeFlags,DWORD TraceFlags)
{
	guard(UStaticMesh::LineCheck);

	clock(GStats.DWORDStats(GEngineStats.STATS_StaticMesh_CollisionCycles));
	UBOOL	Hit = 0,
	ZeroExtent = (Extent == FVector(0,0,0));

	UBOOL IsShadowCast = TraceFlags & TRACE_ShadowCast;

	if(Owner->bUseCylinderCollision && !IsShadowCast)
		Hit = !UPrimitive::LineCheck(Result,Owner,End,Start,Extent,ExtraNodeFlags,TraceFlags);
#if USECOLLISIONMODEL	
	else if(CollisionModel && ((UseSimpleBoxCollision && !ZeroExtent) || (UseSimpleLineCollision && ZeroExtent)) && !IsShadowCast )
		Hit = !CollisionModel->LineCheck(Result,Owner,End,Start,Extent,ExtraNodeFlags,TraceFlags);
#endif
	else if(kDOPTree.Nodes.Num())
	{
		Result.Time = 1.0f;

		if(ZeroExtent)
		{
			FkDOPLineCollisionCheck kDOPCheck(&Result,Owner,this,Start,End);
			Hit = kDOPTree.LineCheck(kDOPCheck);
			if (Hit == 1)
			{
				// Transform the hit normal to world space if there was a hit
				// This is deferred until now because multiple triangles might get
				// hit in the search and I want to delay the expensive transformation
				// as late as possible
				Result.Normal = kDOPCheck.GetHitNormal();
			}
		}
		else
		{
			FkDOPBoxCollisionCheck kDOPCheck(&Result,Owner,this,Start,End,Extent);
			Hit = kDOPTree.BoxCheck(kDOPCheck);
			if( Hit == 1 )
			{
				// Transform the hit normal to world space if there was a hit
				// This is deferred until now because multiple triangles might get
				// hit in the search and I want to delay the expensive transformation
				// as late as possible
				Result.Normal = kDOPCheck.GetHitNormal();
			}
			//TEMP Hack
			else if (kDOPCheck.bUseHack == 1)
			{
				// Get the direction of the trace so we can back up
				const FVector& Dir = (End - Start).SafeNormal();
				// Create a new start back some units
				FVector NewStart = Start - (Dir * 5.f);
				// Run a new trace
				FkDOPBoxCollisionCheck kDOPCheck2(&Result,Owner,this,NewStart,End,Extent);
				Hit = kDOPTree.BoxCheck(kDOPCheck2);
				if(Hit)
				{
					Result.Normal = kDOPCheck2.GetHitNormal();
					Result.Actor = Owner;
					Result.Primitive = this;
					Result.Time = Clamp(Result.Time - Clamp(0.1f,0.1f / (End - NewStart).Size(),4.0f / (End - NewStart).Size()),0.0f,1.0f);
					Result.Location = NewStart + (End - NewStart) * Result.Time;
				}
				return !Hit;
			}
		}

		if(Hit)
		{
			Result.Actor = Owner;
			Result.Primitive = this;
			Result.Time = Clamp(Result.Time - Clamp(0.1f,0.1f / (End - Start).Size(),4.0f / (End - Start).Size()),0.0f,1.0f);
			Result.Location = Start + (End - Start) * Result.Time;
		}
	}

	unclock(GStats.DWORDStats(GEngineStats.STATS_StaticMesh_CollisionCycles));
	return !Hit;

	unguardf((TEXT("%s"), GetFullName()));
}

//
//	UStaticMesh::PointCheck
//
UBOOL UStaticMesh::PointCheck(FCheckResult& Result,AActor* Owner,const FVector& Location,const FVector& Extent,DWORD ExtraNodeFlags)
{
	guard(UStaticMesh::PointCheck);

	INT		StartCycles = appCycles();
	UBOOL	Hit = 0;

	if(Owner->bUseCylinderCollision)
		Hit = !UPrimitive::PointCheck(Result,Owner,Location,Extent,ExtraNodeFlags);
#if USECOLLISIONMODEL	
	else if(CollisionModel && UseSimpleBoxCollision)
		Hit = !CollisionModel->PointCheck(Result,Owner,Location,Extent,ExtraNodeFlags);
#endif
	else if(kDOPTree.Nodes.Num())
	{ 
		FkDOPPointCollisionCheck kDOPCheck(&Result,Owner,this,Location,Extent);
		Hit = kDOPTree.PointCheck(kDOPCheck);
		// Transform the hit normal to world space if there was a hit
		// This is deferred until now because multiple triangles might get
		// hit in the search and I want to delay the expensive transformation
		// as late as possible. Same thing holds true for the hit location
		if (Hit == 1)
		{
			Result.Normal = kDOPCheck.GetHitNormal();
			Result.Location = kDOPCheck.GetHitLocation();
		}

		if(Hit)
		{
			Result.Normal.Normalize();
			// Now calculate the location of the hit in world space
			Result.Actor = Owner;
			Result.Primitive = this;
		}
	}

	GStats.DWORDStats(GEngineStats.STATS_StaticMesh_CollisionCycles) += (appCycles() - StartCycles);

	return !Hit;

	unguard;
}

