/*=============================================================================
	UnOctree.h: Octree implementation header
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

class ENGINE_API FOctreeNode
{
public:
	TArray<AActor*>			Actors;		// Actors held at this node.
	class FOctreeNode*		Children;	// Child nodes. If NULL, this is a leaf. Otherwise, always size 8.

	FOctreeNode();
	~FOctreeNode();

	void ActorNonZeroExtentLineCheck(class FCollisionOctree* octree, const FPlane* nodeBox);
	void ActorZeroExtentLineCheck(class FCollisionOctree* octree, 
										   FLOAT T0X, FLOAT T0Y, FLOAT T0Z,
										   FLOAT T1X, FLOAT T1Y, FLOAT T1Z, const FPlane* nodeBox);
	void ActorEncroachmentCheck(FCollisionOctree* octree, const FPlane* nodeBox);
	void ActorPointCheck(FCollisionOctree* octree, const FPlane* nodeBox);
	void ActorRadiusCheck(FCollisionOctree* octree, const FPlane* nodeBox);
	void ActorOverlapCheck(FCollisionOctree* octree, const FPlane* nodeBox);

	void SingleNodeFilter(AActor* Actor, FCollisionOctree* o, const FPlane* nodeBox);
	void MultiNodeFilter(AActor* Actor, FCollisionOctree* o, const FPlane* nodeBox);
	void RemoveAllActors(FCollisionOctree* o);

	void Draw(FColor DrawColor, UBOOL bAndChildren, const FPlane* nodeBox);
	void DrawFlaggedActors(FCollisionOctree* octree, const FPlane* nodeBox);
	void CheckIsEmpty();
	void CheckActorNotReferenced(AActor* Actor);

	void FilterTest(FBox* TestBox, UBOOL bMulti, TArray<FOctreeNode*> *Nodes, const FPlane* nodeBox);

private:
	void StoreActor(AActor *Actor, FCollisionOctree* o, const FPlane* nodeBox);
};

class ENGINE_API FCollisionOctree : public FCollisionHashBase
{
public:
	// Root node - assumed to have size WORLD_MAX
	FOctreeNode*	RootNode;
	INT				CollisionTag;

	/// This is a bit nasty...
	// Temporary storage while recursing for line checks etc.
	FCheckResult*	ChkResult;
	FMemStack*		ChkMem;
	FVector			ChkEnd;
	FVector			ChkStart; // aka Location
	FRotator		ChkRotation;
	FVector			ChkDir;
	FVector			ChkOneOverDir;
	FVector			ChkExtent;
	DWORD			ChkTraceFlags;
	DWORD			ChkExtraNodeFlags;
	AActor*			ChkActor;
	FLOAT			ChkRadiusSqr;
	FBox			ChkBox;
	UBOOL		    ChkBlockKarmaOnly;
	/// 

	// Keeps track of shortest hit time so far.
	FCheckResult*	ChkFirstResult;

	FVector			RayOrigin;
	INT				ParallelAxis;
	INT				NodeTransform;

	FLOAT			ZE_SNF_PrimMillisec, 
					ZE_MNF_PrimMillisec, 
					NZE_SNF_PrimMillisec, 
					NZE_MNF_PrimMillisec,
					BoxBox_Millisec,
					ZE_LineBox_Millisec,
					NZE_LineBox_Millisec,
					Add_Millisec,
					Remove_Millisec,
					NZELineCheck_Millisec,
					ZELineCheck_Millisec,
					PointCheck_Millisec,
					EncroachCheck_Millisec,
					RadiusCheck_Millisec;

	DWORD			ZE_SNF_PrimCount, 
					ZE_MNF_PrimCount, 
					NZE_SNF_PrimCount, 
					NZE_MNF_PrimCount,
					BoxBox_Count,
					ZE_LineBox_Count,
					NZE_LineBox_Count,
					Add_Count,
					Remove_Count,
					NZELineCheck_Count,
					ZELineCheck_Count,
					PointCheck_Count,
					EncroachCheck_Count,
					RadiusCheck_Count;

	// FCollisionHashBase Interface
	FCollisionOctree();
	virtual ~FCollisionOctree();

	virtual void Tick();
	virtual void AddActor(AActor *Actor);
	virtual void RemoveActor(AActor *Actor);
	virtual FCheckResult* ActorLineCheck(FMemStack& Mem, 
		const FVector& End, 
		const FVector& Start, 
		const FVector& Extent, 
		DWORD TraceFlags, 
		DWORD ExtraNodeFlags, 
		AActor *SourceActor);
	virtual FCheckResult* ActorPointCheck(FMemStack& Mem, 
		const FVector& Location, 
		const FVector& Extent, 
		DWORD TraceFlags, 
		DWORD ExtraNodeFlags, 
		UBOOL bSingleResult=0);
	virtual FCheckResult* ActorRadiusCheck(FMemStack& Mem, 
		const FVector& Location, 
		FLOAT Radius, 
		DWORD ExtraNodeFlags);
	virtual FCheckResult* ActorEncroachmentCheck(FMemStack& Mem, 
		AActor* Actor, 
		FVector Location, 
		FRotator Rotation, 
		DWORD TraceFlags, 
		DWORD ExtraNodeFlags);
	virtual FCheckResult* ActorOverlapCheck( FMemStack& Mem, 
		AActor* Actor,
		FBox* Box, 
		UBOOL bBlockKarmaOnly);

	virtual void CheckActorNotReferenced(AActor* Actor);
	virtual void CheckIsEmpty();
	virtual void CheckActorLocations(ULevel *level);
};

FCheckResult* FindFirstResult(FCheckResult* Hits, DWORD TraceFlags);
