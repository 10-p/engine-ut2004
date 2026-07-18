/*=============================================================================
	UnConvexVolume.cpp
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UConvexVolume);

//
//	UConvexVolume::UConvexVolume
//

UConvexVolume::UConvexVolume()
{
}

//
//	UConvexVolume::Serialize
//

void UConvexVolume::Serialize(FArchive& Ar)
{
	guard(UConvexVolume::Serialize);

	Super::Serialize(Ar);

	Ar	<< Faces
		<< Edges
		<< BoundingBox;

	unguard;
}

//
//	UConvexVolume::GetCollisionBoundingBox
//

FBox UConvexVolume::GetCollisionBoundingBox(const AActor* Owner) const
{
	return BoundingBox.TransformBy(Owner->LocalToWorld());
}

//
//	FClippedLine
//

class FClippedLine
{
public:

	FVector	Start,
			End,
			Extent,
			HitNormal;
	FLOAT	T0,
			T1;
	UBOOL	Hit;

	// Constructor.

	FClippedLine(FVector InStart,FVector InEnd,FVector InExtent,FLOAT InT1)
	{
		Start = InStart;
		End = InEnd;
		Extent = InExtent;
		HitNormal = FVector(0,0,0);
		Hit = 0;
		T0 = -1.0f;
		T1 = InT1;
	}

	// ClipWithPlane

	friend UBOOL ClipWithPlane(const FPlane& Plane,FClippedLine& Clipee)
	{
		FLOAT	PushOut = FBoxPushOut(Plane,Clipee.Extent),
				StartDist = Plane.PlaneDot(Clipee.Start),
				EndDist = Plane.PlaneDot(Clipee.End);

		if((StartDist - EndDist) > 0.00001f)
		{
			FLOAT	Time = (PushOut - StartDist) / (EndDist - StartDist);

			if(Time > Clipee.T0)
			{
				Clipee.T0 = Time;
				Clipee.HitNormal = Plane;
				Clipee.Hit = 1;
			}
		}
		else if((StartDist - EndDist) < -0.00001f)
		{
			FLOAT	Time = (PushOut - StartDist) / (EndDist - StartDist);

			if(Time < Clipee.T1)
				Clipee.T1 = Time;
		}
		else if(StartDist > PushOut && EndDist > PushOut)
			return 0;

		return Clipee.T0 < Clipee.T1 && Clipee.T1 > 0.0f;
	}
};

//
//	UConvexVolume::LineCheck
//

UBOOL UConvexVolume::LineCheck(FCheckResult& Result,AActor* Owner,const FVector& End,const FVector& Start,const FVector& Extent,DWORD ExtraNodeFlags,DWORD TraceFlags)
{
	FClippedLine	ClippedLine(Start,End,FVector(0,0,0),1.0f);
	FDynamicActor*	DynamicActor = Owner->GetActorRenderData();
	FVector			LocalStart = DynamicActor->WorldToLocal.TransformFVector(Start),
					LocalEnd = DynamicActor->WorldToLocal.TransformFVector(End),
					LocalDirection = LocalEnd - LocalStart,
					LocalOneOverDirection(1.0f / LocalDirection.X,1.0f / LocalDirection.Y,1.0f / LocalDirection.Z);

	if(!FLineBoxIntersection(BoundingBox,LocalStart,LocalEnd,LocalDirection,LocalOneOverDirection))
		return 1;

	for(INT FaceIndex = 0;FaceIndex < Faces.Num();FaceIndex++)
	{
		FConvexVolumeFace&	Face = Faces(FaceIndex);

		if(!ClipWithPlane(Face.Plane.TransformBy(DynamicActor->LocalToWorld),ClippedLine))
			return 1;
	}

	if(ClippedLine.Hit)
	{
		Result.Actor = Owner;
		Result.Primitive = this;
		Result.Time = Clamp(ClippedLine.T0 - Clamp(0.1f,0.1f / (End - Start).Size(),1.0f / (End - Start).Size()),0.0f,1.0f);
		Result.Location = Start + (End - Start) * Result.Time;
		Result.Normal = DynamicActor->WorldToLocal.TransformNormal(ClippedLine.HitNormal).SafeNormal();
		return 0;
	}
	else
		return 1;
}

