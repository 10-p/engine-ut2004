/*=============================================================================
	UnPrim.h: Unreal UPrimitive definition.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#ifndef _UNPRIM_H
#define _UNPRIM_H

/*-----------------------------------------------------------------------------
	FCheckResult.
-----------------------------------------------------------------------------*/

#include "UnForcePacking_begin.h"

//
// Results of an actor check.
//
struct FIteratorActorList : public FIteratorList
{
	// Variables.
	AActor* Actor;

	// Functions.
	FIteratorActorList()
	{}
	FIteratorActorList( FIteratorActorList* InNext, AActor* InActor )
	:	FIteratorList	(InNext)
	,	Actor			(InActor)
	{}
	FIteratorActorList* GetNext()
	{ return (FIteratorActorList*) Next; }
};

//
// Results from a collision check.
//
struct FCheckResult : public FIteratorActorList
{
	// Variables.
	FVector		Location;   // Location of the hit in coordinate system of the returner.
	FVector		Normal;     // Normal vector in coordinate system of the returner. Zero=none.
	UPrimitive*	Primitive;  // Actor primitive which was hit, or NULL=none.
	FLOAT       Time;       // Time until hit, if line check.
	INT			Item;       // Primitive data item which was hit, INDEX_NONE=none.
	UMaterial*	Material;	// Material of the item which was hit.

	// Functions.
	FCheckResult()
	:	Material	(NULL)
	{}
	FCheckResult( FLOAT InTime, FCheckResult* InNext=NULL )
	:	FIteratorActorList( InNext, NULL )
	,	Location	(0,0,0)
	,	Normal		(0,0,0)
	,	Primitive	(NULL)
	,	Time		(InTime)
	,	Item		(INDEX_NONE)
	,	Material	(NULL)
	{}
	FCheckResult*& GetNext()
		{ return *(FCheckResult**)&Next; }
	friend QSORT_RETURN CDECL CompareHits( const FCheckResult* A, const FCheckResult* B )
		{ return A->Time<B->Time ? -1 : A->Time>B->Time ? 1 : 0; }
};
// Standalone declaration for CompareHits (GCC 13 requires it for non-ADL lookup).
QSORT_RETURN CDECL CompareHits( const FCheckResult* A, const FCheckResult* B );

/*-----------------------------------------------------------------------------
	UPrimitive.
-----------------------------------------------------------------------------*/

//
// UPrimitive, the base class of geometric entities capable of being
// rendered and collided with.
//
class ENGINE_API UPrimitive : public UObject
{
	DECLARE_CLASS(UPrimitive,UObject,0,Engine)

	// Variables.
	FBox BoundingBox;
	FSphere BoundingSphere;

	// Constructor.
	UPrimitive()
	: BoundingBox(0)
	, BoundingSphere(0)
	{}

	// UObject interface.
	void Serialize( FArchive& Ar );

	// UPrimitive collision interface.
	virtual UBOOL PointCheck
	(
		FCheckResult	&Result,
		AActor			*Owner,
		const FVector&	Location,
		const FVector&	Extent,
		DWORD           ExtraNodeFlags
	);
	virtual UBOOL LineCheck
	(
		FCheckResult	&Result,
		AActor			*Owner,
		const FVector&	End,
		const FVector&	Start,
		const FVector&	Extent,
		DWORD           ExtraNodeFlags,
		DWORD			TraceFlags
	);
	virtual FBox GetRenderBoundingBox( const AActor* Owner );
	virtual FSphere GetRenderBoundingSphere( const AActor* Owner );
	virtual FBox GetCollisionBoundingBox( const AActor* Owner ) const;
	virtual UBOOL UseCylinderCollision( const AActor* Owner );
	virtual void Illuminate(AActor* Owner,UBOOL ChangedOnly = 0) {}
	
	// This returns the centre/extent to use during an encroachment point-check.
	virtual FVector GetEncroachExtent(AActor* Owner);
	virtual FVector GetEncroachCenter(AActor* Owner);
};

#include "UnForcePacking_end.h"

#endif

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

