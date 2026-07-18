/*=============================================================================
	UnConvexVolume.h
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnForcePacking_begin.h"

//
//	FConvexVolumeEdge
//

class FConvexVolumeEdge
{
public:

	INT	Faces[2];		// Indices of the two faces sharing this edge.
	INT	Vertices[2];	// Indices of the first vertices of this edge in each face.

	// Constructor.

	FConvexVolumeEdge() {}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FConvexVolumeEdge& E)
	{
		Ar	<< E.Faces[0]
			<< E.Faces[1]
			<< E.Vertices[0]
			<< E.Vertices[1];

		return Ar;
	}
};

//
//	FConvexVolumeFace
//

class FConvexVolumeFace
{
public:

	FPlane			Plane;
	TArray<FVector>	Vertices;

	// Constructor.

	FConvexVolumeFace() {}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FConvexVolumeFace& F)
	{
		Ar	<< F.Plane
			<< F.Vertices;

		return Ar;
	}
};

//
//	UConvexVolume
//

class ENGINE_API UConvexVolume : public UPrimitive
{
	DECLARE_CLASS(UConvexVolume,UPrimitive,0,Engine);

public:

	TArray<FConvexVolumeFace>	Faces;
	TArray<FConvexVolumeEdge>	Edges;
	FBox						BoundingBox;

	// Constructor.

	UConvexVolume();

	// UObject interface.

	virtual void Serialize(FArchive& Ar);

	// UPrimitive interface.

	virtual FBox GetRenderBoundingBox(const AActor* Owner) { return BoundingBox; }
	virtual FBox GetCollisionBoundingBox(const AActor* Owner) const;
	virtual UBOOL LineCheck(FCheckResult& Result,AActor* Owner,const FVector& End,const FVector& Start,const FVector& Extent,DWORD ExtraNodeFlags,DWORD TraceFlags);
};

#include "UnForcePacking_end.h"


