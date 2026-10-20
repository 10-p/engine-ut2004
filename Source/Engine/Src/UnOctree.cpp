/*=============================================================================
	UnOctree.cpp: Octree implementation
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "EnginePrivate.h"
#include "UnOctreePrivate.h"
#if ((defined _XBOX) || (defined __GNUC__) || (defined __INTEL_COMPILER)) // sjs
#include <float.h>
#include <limits.h>
#endif

static DWORD GOctreeBytesUsed = 0;

// urgh
#define MY_FLTMAX (3.402823466e+38F)
#define MY_INTMAX (2147483647)

//////////////////////////////////////////// UTIL //////////////////////////////////////////////////
#define MAX_ACTORS_PER_NODE (3)		// Max number of actors held in a node before adding children.
#define MIN_NODE_SIZE		(100)   // Mininmum size a node can ever be.
#define ACTORBOX_EPSILON	(4.2f)  // Amount to expand actor boxes by on addition. 
									// Needed because line checks etc. 'pull back' their hit point a little.

#define TIME_OCTREE			(0)		// Do performance timing on octree
#define CHECK_FALSE_NEG		(0)		// Check for hits with primitive, but not with bounding box.

enum 
{
	CHILDXMAX = 0x004,
	CHILDYMAX = 0x002,
	CHILDZMAX = 0x001
};

static const FPlane RootNodeBox = FPlane(0, 0, 0, HALF_WORLD_MAX);

#define NODEXMAX(node) ((node)->X + (node)->W)
#define NODEXMIN(node) ((node)->X - (node)->W)
#define NODEYMAX(node) ((node)->Y + (node)->W)
#define NODEYMIN(node) ((node)->Y - (node)->W)
#define NODEZMAX(node) ((node)->Z + (node)->W)
#define NODEZMIN(node) ((node)->Z - (node)->W)

#define MY_BOX_SIDE_THRESHOLD	(0.1f)

#if 0
static TArray<FVector>	TmpStart;
static TArray<FVector>	TmpEnd;
static TArray<FBox>		TmpBox;
#endif

#if TIME_OCTREE
// Used to make timing a little easier!
class MyClock
{
private:
	DWORD* StatCount;
	FLOAT* StatTime;
	DWORD Time;

public:
	MyClock(DWORD* count, FLOAT* time)
	:	StatCount( count ),
		StatTime( time )
	{
		Time = 0;
		clock( Time );
	}
	~MyClock()
	{
		unclock( Time );
		(*StatTime) += Time * GSecondsPerCycle * 1000.0f;
		(*StatCount)++;
	}
};
#endif

//#define MY_FABS(x) (Abs(x))
#define MY_FABS(x) (fabsf(x))

// New, hopefully faster, 'slabs' box test.
static UBOOL LineBoxIntersect
(
	const FVector&	BoxCenter,
	const FVector&  BoxRadii,
	const FVector&	Start,
	const FVector&	Direction,
	const FVector&	OneOverDirection
)
{
	//const FVector* boxPlanes = &Box.Min;
	
	FLOAT tf, tb;
	FLOAT tnear = 0.f;
	FLOAT tfar = 1.f;
	
	FVector LocalStart = Start - BoxCenter;

	// X //
	// First - see if ray is parallel to slab.
	if(Direction.X != 0.f)
	{
		// If not, find the time it hits the front and back planes of slab.
		tf = - (LocalStart.X * OneOverDirection.X) - BoxRadii.X * MY_FABS(OneOverDirection.X);
		tb = - (LocalStart.X * OneOverDirection.X) + BoxRadii.X * MY_FABS(OneOverDirection.X);

		if(tf > tnear)
			tnear = tf;

		if(tb < tfar)
			tfar = tb;

		if(tfar < tnear)
			return 0;
	}
	else
	{
		// If it is parallel, early return if start is outiside slab.
		if(!(MY_FABS(LocalStart.X) <= BoxRadii.X))
			return 0;
	}

	// Y //
	if(Direction.Y != 0.f)
	{
		// If not, find the time it hits the front and back planes of slab.
		tf = - (LocalStart.Y * OneOverDirection.Y) - BoxRadii.Y * MY_FABS(OneOverDirection.Y);
		tb = - (LocalStart.Y * OneOverDirection.Y) + BoxRadii.Y * MY_FABS(OneOverDirection.Y);

		if(tf > tnear)
			tnear = tf;

		if(tb < tfar)
			tfar = tb;

		if(tfar < tnear)
			return 0;
	}
	else
	{
		if(!(Abs(LocalStart.Y) <= BoxRadii.Y))
			return 0;
	}

	// Z //
	if(Direction.Z != 0.f)
	{
		// If not, find the time it hits the front and back planes of slab.
		tf = - (LocalStart.Z * OneOverDirection.Z) - BoxRadii.Z * MY_FABS(OneOverDirection.Z);
		tb = - (LocalStart.Z * OneOverDirection.Z) + BoxRadii.Z * MY_FABS(OneOverDirection.Z);

		if(tf > tnear)
			tnear = tf;

		if(tb < tfar)
			tfar = tb;

		if(tfar < tnear)
			return 0;
	}
	else
	{
		if(!(MY_FABS(LocalStart.Z) <= BoxRadii.Z))
			return 0;
	}

	// we hit!
	return 1;
}

#define LINE_BOX LineBoxIntersect

static UBOOL BoxBoxIntersect(FBox &box1, FBox &box2)
{
	if( box1.Min.X > box2.Max.X || box2.Min.X > box1.Max.X )
		return false;
	if( box1.Min.Y > box2.Max.Y || box2.Min.Y > box1.Max.Y )
		return false;
	if( box1.Min.Z > box2.Max.Z || box2.Min.Z > box1.Max.Z )
		return false;
	return true;
}

static inline void MakeNodeBox(FBox* outBox, const FPlane& inNode)
{
	outBox->Min = inNode - FVector(inNode.W, inNode.W, inNode.W);
	outBox->Max = inNode + FVector(inNode.W, inNode.W, inNode.W);
	outBox->IsValid = 1;
}

static inline void CalcChildBox(FPlane* childNode, const FPlane* parentNode, const INT childIx)
{
	guardSlow(CalcChildBox);

	childNode->W = 0.5f * parentNode->W;
	childNode->X = parentNode->X + (((childIx & CHILDXMAX) >> 1)-1) * childNode->W;
	childNode->Y = parentNode->Y + (((childIx & CHILDYMAX)     )-1) * childNode->W;
	childNode->Z = parentNode->Z + (((childIx & CHILDZMAX) << 1)-1) * childNode->W;

	unguardSlow;
}

// Assumes that this box overlaps this node!
// Return 'true' if node is completely inside outerBox
static inline UBOOL NodeIsInside(const FPlane* node, const FBox* outerBox)
{
	guardSlow(NodeIsInside);

	if ( NODEXMIN(node) < outerBox->Min.X || NODEXMAX(node) > outerBox->Max.X )
		return 0;
	if ( NODEYMIN(node) < outerBox->Min.Y || NODEYMAX(node) > outerBox->Max.Y )
		return 0;
	if ( NODEZMIN(node) < outerBox->Min.Z || NODEZMAX(node) > outerBox->Max.Z )
		return 0;
	return 1;

	unguardSlow;
}

// If TRACE_SingleResult, only return 1 result (the first hit).
// This code has to ignore fake-backdrop hits during shadow casting though (can't do that in ShouldTrace).
FCheckResult* FindFirstResult(FCheckResult* Hits, DWORD TraceFlags)
{
	guard(FindChildren);

	FCheckResult* FirstResult = NULL;

	if(Hits)
	{
		FLOAT firstTime = MY_FLTMAX;
		for(FCheckResult* res = Hits; res!=NULL; res=res->GetNext())
		{
			if(res->Time < firstTime)
			{
				FirstResult = res;
				firstTime = res->Time;
			}
		}

		if(FirstResult)
			FirstResult->GetNext() = NULL;
	}

	return FirstResult;

	unguard;
}


// Create array of children node indices that this box overlaps.
static inline INT FindChildren(const FPlane* parentNode, const FBox* testBox, INT* childIXs)
{
	guardSlow(FindChildren);

	INT childCount = 0;

	if(testBox->Max.X > parentNode->X) // XMAX
	{ 
		if(testBox->Max.Y > parentNode->Y) // YMAX
		{
			if(testBox->Max.Z > parentNode->Z) // ZMAX
				childIXs[childCount++] = CHILDXMAX+CHILDYMAX+CHILDZMAX;
			if(testBox->Min.Z < parentNode->Z) // ZMIN
				childIXs[childCount++] = CHILDXMAX+CHILDYMAX          ;
		}

		if(testBox->Min.Y < parentNode->Y) // YMIN
		{
			if(testBox->Max.Z > parentNode->Z) // ZMAX
				childIXs[childCount++] = CHILDXMAX+          CHILDZMAX;
			if(testBox->Min.Z < parentNode->Z) // ZMIN
				childIXs[childCount++] = CHILDXMAX                    ;
		}
	}

	if(testBox->Min.X < parentNode->X) // XMIN
	{ 
		if(testBox->Max.Y > parentNode->Y) // YMAX
		{
			if(testBox->Max.Z > parentNode->Z) // ZMAX
				childIXs[childCount++] =           CHILDYMAX+CHILDZMAX;
			if(testBox->Min.Z < parentNode->Z) // ZMIN
				childIXs[childCount++] =           CHILDYMAX          ;	
		}

		if(testBox->Min.Y < parentNode->Y) // YMIN
		{
			if(testBox->Max.Z > parentNode->Z) // ZMAX
				childIXs[childCount++] =                     CHILDZMAX;
			if(testBox->Min.Z < parentNode->Z) // ZMIN
				childIXs[childCount++] = 0                            ;
		}
	}

	return childCount;

	unguardSlow;
}

// Returns which child node 'testBox' would fit into.
// Returns -1 if box overlaps any planes, and therefore wont fit into a child.
// Assumes testBox would fit into this (parent) node.

static inline INT FindChild(const FPlane* parentNode, const FBox* testBox)
{
	guardSlow(FindChild);

	INT result = 0;

	if(testBox->Min.X > parentNode->X)
		result |= CHILDXMAX;
	else if(testBox->Max.X > parentNode->X)
		return -1;

	if(testBox->Min.Y > parentNode->Y)
		result |= CHILDYMAX;
	else if(testBox->Max.Y > parentNode->Y)
		return -1;

	if(testBox->Min.Z > parentNode->Z)
		result |= CHILDZMAX;
	else if(testBox->Max.Z > parentNode->Z)
		return -1;

	return result;

	unguardSlow;
}

///////////////////////////////////////////// NODE /////////////////////////////////////////////////
FOctreeNode::FOctreeNode() 
: Actors( )
{
	guard(FOctreeNode::FOctreeNode);

	Children = NULL;
	// Bounding box is set up by FOctreeNode.StoreActor
	// (or FCollisionOctree constructor for root node).

	unguard;
}

FOctreeNode::~FOctreeNode()
{
	guard(FOctreeNode::~FOctreeNode);

	// We call RemoveActors on nodes in the Octree destructor, 
	// so we should never have actors in nodes when we destroy them.
	check(Actors.Num() == 0)

	if(Children)
	{
		delete[] Children;
		Children = NULL;
	}

	unguard;
}

// Remove all actors in this node from the octree
void FOctreeNode::RemoveAllActors(FCollisionOctree* o)
{
	guard(FOctreeNode::RemoveAllActors);

	    // All actors found at this octree node, remove from octree.
	    while(Actors.Num() != 0)
	    {
		    AActor* actor = Actors(0);

		    if(actor->OctreeNodes.Num() > 0)
			{
			    o->RemoveActor(actor);
			}
			else
			{
				Actors.RemoveItem(actor);
			    debugf(TEXT("Actor (%s) in Octree, but Actor->OctreeNodes empty."), actor->GetName());
			}
	    }

	// Then do the same for the children (if present).
	if(Children)
	{
		for(INT i=0; i<8; i++)
			Children[i].RemoveAllActors(o);
	}

	unguard;
}

// Error checking.
void FOctreeNode::CheckIsEmpty()
{
	guard(FOctreeNode::CheckIsEmpty);

	// Check that there are no actors left in this node.
	for(INT i=0; i<Actors.Num(); i++)
		debugf(TEXT("Octree Warning (~FOctreeNode): %s Still In Node."), Actors(i)->GetName());

	// Then check children, if present.
	if(Children)
	{
		for(INT i=0; i<8; i++)
			Children[i].CheckIsEmpty();
	}

	unguard;
}

// More error checking
void FOctreeNode::CheckActorNotReferenced(AActor* Actor)
{
	guard(FOctreeNode::CheckActorNotReferenced);

	for(INT i=0; i<Actors.Num(); i++)
		appErrorf(TEXT("Octree Error: %s Still In Octree."), Actors(i)->GetName());

	// Then check children, if present.
	if(Children)
	{
		for(INT i=0; i<8; i++)
			Children[i].CheckActorNotReferenced(Actor);
	}

	unguard;
}

// Filter node through tree, allowing Actor to only reside in one node.
// Assumes that Actor fits inside this node, but it might go lower.
void FOctreeNode::SingleNodeFilter(AActor *Actor, FCollisionOctree* o, const FPlane* nodeBox)
{
	guard(FOctreeNode::SingleNodeFilter);

	INT childIx = FindChild(nodeBox, &Actor->OctreeBox);

	if(!Children || childIx == -1)
		this->StoreActor(Actor, o, nodeBox);
	else
	{
		FPlane childBox;
		CalcChildBox(&childBox, nodeBox, childIx);
		this->Children[childIx].SingleNodeFilter(Actor, o, &childBox);
	}

	unguard;
}

// Filter node through tree, allowing actor to reside in multiple nodes.
// Assumes that Actor overlaps this node, but it might go lower.
void FOctreeNode::MultiNodeFilter(AActor *Actor, FCollisionOctree* o, const FPlane* nodeBox)
{
	guard(FOctreeNode::MultiNodeFilter);

	// If there are no children, or this actors bounding box completely contains this nodes
	// bounding box, store the actor at this node.
	if(!Children || NodeIsInside(nodeBox, &Actor->OctreeBox) )
		this->StoreActor(Actor, o, nodeBox);
	else
	{
		INT childIXs[8];
		INT numChildren = FindChildren(nodeBox, &Actor->OctreeBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			FPlane childBox;
			CalcChildBox(&childBox, nodeBox, childIXs[i]);
			this->Children[childIXs[i]].MultiNodeFilter(Actor, o, &childBox);
		}
	}

	unguard;
}


// Just for testing, this tells you which nodes the box should be in. 
void FOctreeNode::FilterTest(FBox* TestBox, UBOOL bMulti, TArray<FOctreeNode*> *Nodes, const FPlane* nodeBox)
{
	guard(FOctreeNode::FilterTest);

	if(bMulti) // Multi-Node Filter
	{
		if(!Children || NodeIsInside(nodeBox, TestBox) )
			Nodes->AddItem(this);
		else
		{
			for(INT i=0; i<8; i++)
			{
				FPlane childBox;
				CalcChildBox(&childBox, nodeBox, i);
				this->Children[i].FilterTest(TestBox, 1, Nodes, &childBox);
			}
		}
	}
	else // Single Node Filter
	{
		INT childIx = FindChild(nodeBox, TestBox);

		if(!Children || childIx == -1)
			Nodes->AddItem(this);
		else
		{
			INT childIXs[8];
			INT numChildren = FindChildren(nodeBox, TestBox, childIXs);
			for(INT i=0; i<numChildren; i++)
			{
				FPlane childBox;
				CalcChildBox(&childBox, nodeBox, childIXs[i]);
				this->Children[childIXs[i]].FilterTest(TestBox, 0, Nodes, &childBox);
			}
		}
	}

	unguard;
}


// We have decided to actually add an Actor at this node,
// so do whatever work needs doing.
void FOctreeNode::StoreActor(AActor *Actor, FCollisionOctree* o, const FPlane* nodeBox)
{
	guard(FOctreeNode::StoreActor);

	// If we are over the limit of actors in a node, and have not already split,
	// and are not already too small, add children, and re-check each actors held here against this node.
	// 
	// Note, because we don't only hold things in leaves, 
	// we can still have more than MAX_ACTORS_PER_NODE in a node.
	if(	Actors.Num() >= MAX_ACTORS_PER_NODE
		&& Children == NULL 
		&& 0.5f * nodeBox->W > MIN_NODE_SIZE)
	{
		// Allocate memory for children nodes.
		Children = new(TEXT("FOctreeNode"))FOctreeNode[8];
		GOctreeBytesUsed += 8 * sizeof(FOctreeNode);

		// Set up children. Calculate child bounding boxes
		//for(INT i=0; i<8; i++)
		//	CalcChildBox(&Children[i], this, i);

		// Now we need to remove each actor from this node and re-check it,
		// in case it needs to move down the Octree.
		TArray<AActor*> PendingActors = Actors;
		PendingActors.AddItem(Actor);

		Actors.Empty();

		for(INT i=0; i<PendingActors.Num(); i++)
		{
			// Remove this actors reference to this node.
			PendingActors(i)->OctreeNodes.RemoveItem((PTRINT)(this));

			// Then re-check it against this node, which will then check against children.
			if(PendingActors(i)->bWasSNFiltered)
				this->SingleNodeFilter(PendingActors(i), o, nodeBox);
			else
				this->MultiNodeFilter(PendingActors(i), o, nodeBox);
		}
	}
	// We are just going to add this actor here.
	else
	{
		// Add actor to this nodes list of actors,
		Actors.AddItem(Actor);

		// and add this node to the actors list of nodes.
		Actor->OctreeNodes.AddItem((PTRINT)(this));
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	Recursive ZERO EXTENT line checker
-----------------------------------------------------------------------------*/

// Given plane crossing points, find first sub-node that plane hits
static inline INT FindFirstNode(FLOAT T0X, FLOAT T0Y, FLOAT T0Z,
							    FLOAT TMX, FLOAT TMY, FLOAT TMZ)
{
	// First, figure out which plane ray hits first.
	INT FirstNode = 0;
	if(T0X > T0Y)
		if(T0X > T0Z)
		{ // T0X is max - Entry Plane is YZ
			if(TMY < T0X) FirstNode |= 2;
			if(TMZ < T0X) FirstNode |= 1;
		}
		else
		{ // T0Z is max - Entry Plane is XY
			if(TMX < T0Z) FirstNode |= 4;
			if(TMY < T0Z) FirstNode |= 2;
		}
	else
		if(T0Y > T0Z)
		{ // T0Y is max - Entry Plane is XZ
			if(TMX < T0Y) FirstNode |= 4;
			if(TMZ < T0Y) FirstNode |= 1;
		}
		else
		{ // T0Z is max - Entry Plane is XY
			if(TMX < T0Z) FirstNode |= 4;
			if(TMY < T0Z) FirstNode |= 2;
		}

	return FirstNode;
}

// Returns the INT whose corresponding FLOAT is smallest.
static inline INT GetNextNode(FLOAT X, INT nX, FLOAT Y, INT nY, FLOAT Z, INT nZ)
{
	if(X<Y)
		if(X<Z)
			return nX;
		else
			return nZ;
	else
		if(Y<Z)
			return nY;
		else
			return nZ;
}

void FOctreeNode::ActorZeroExtentLineCheck(FCollisionOctree* o, 
										   FLOAT T0X, FLOAT T0Y, FLOAT T0Z,
										   FLOAT T1X, FLOAT T1Y, FLOAT T1Z, const FPlane* nodeBox)
{
	guard(FOctreeNode::ActorZeroExtentLineCheck);

	// If node if before start of line (ie. exit times are negative)
	if(T1X < 0.0f || T1Y < 0.0f || T1Z < 0.0f)
		return;

	FLOAT MaxHitTime;

	// If we are only looking for the first hit, dont check this node if its beyond the
	// current first hit time.
	if((o->ChkTraceFlags & TRACE_SingleResult) && o->ChkFirstResult )
		MaxHitTime = o->ChkFirstResult->Time; // Check a little beyond current best hit.
	else
		MaxHitTime = 1.0f;

	// If node is beyond end of line (ie. any entry times are > MaxHitTime)
	if(T0X > MaxHitTime || T0Y > MaxHitTime || T0Z > MaxHitTime)
		return;

#if DRAW_LINE_TRACE
	Draw(FColor(255,0,0), 0);
#endif

	// If it does touch this box, first check line against each thing in the node.
	for(INT i=0; i<Actors.Num(); i++)
	{
		AActor* testActor = Actors(i);
		if(testActor->CollisionTag != o->CollisionTag)
		{
			// Check collision.
			testActor->CollisionTag = o->CollisionTag;

			if( testActor->bBlockZeroExtentTraces &&
				testActor != o->ChkActor &&
				// A trace without a source actor (FindSpot, static lighting) has no owner to skip.
				(!o->ChkActor || !o->ChkActor->IsOwnedBy(testActor)) &&
				testActor->ShouldTrace(o->ChkActor, o->ChkTraceFlags) )
			{
#if TIME_OCTREE			
				DWORD Time = 0;
				clock(Time);
#endif

				// Check line against actor's bounding box
				//FBox ActorBox = testActor->OctreeBox;
				UBOOL hitActorBox = LINE_BOX(testActor->OctreeBoxCenter, testActor->OctreeBoxRadii, o->ChkStart, o->ChkDir, o->ChkOneOverDir);

#if TIME_OCTREE
				unclock(Time);
				o->ZE_LineBox_Count++;
				o->ZE_LineBox_Millisec += Time * GSecondsPerCycle * 1000.0f;
#endif

#if !CHECK_FALSE_NEG
				if(!hitActorBox)
					continue;
#endif

#if TIME_OCTREE				
				Time = 0;
				clock(Time);
#endif

				FCheckResult Hit(0);
				UBOOL lineChkRes = testActor->GetPrimitive()->LineCheck(Hit, 
					testActor, 
					o->ChkEnd, 
					o->ChkStart, 
					o->ChkExtent, 
					o->ChkExtraNodeFlags, 
					o->ChkTraceFlags)==0;

#if TIME_OCTREE				
				unclock(Time);

				if(testActor->bWasSNFiltered)
				{
					o->ZE_SNF_PrimCount++;
					o->ZE_SNF_PrimMillisec += Time * GSecondsPerCycle * 1000.0f;
				}
				else
				{
					o->ZE_MNF_PrimCount++;
					o->ZE_MNF_PrimMillisec += Time * GSecondsPerCycle * 1000.0f;
				}
#endif

				if( lineChkRes )
				{
#if CHECK_FALSE_NEG
					if(!hitActorBox)
						debugf(TEXT("ZELC False Neg! : %s"), testActor->GetName());
#endif

					FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult(Hit);
					NewResult->GetNext() = o->ChkResult;
					o->ChkResult = NewResult;

#if 0
					// DEBUG CHECK - Hit time should never before any of the entry times for this node.
					if(T0X > NewResult->Time || T0Y > NewResult->Time || T0Z > NewResult->Time)
					{
						int sdf = 0;
					}
#endif
					// Keep track of smallest hit time.
					if(!o->ChkFirstResult || NewResult->Time < o->ChkFirstResult->Time)
						o->ChkFirstResult = NewResult;

					// If we only wanted one result - our job is done!
					if (o->ChkTraceFlags & TRACE_StopAtFirstHit)
						return;
				}
			}
		}
	}

	// If we have children - then traverse them, in the order the ray passes through them.
	if(Children)
	{
		// Find middle point of node.
		FLOAT TMX = 0.5f*(T0X+T1X);
		FLOAT TMY = 0.5f*(T0Y+T1Y);
		FLOAT TMZ = 0.5f*(T0Z+T1Z);

		// Fix for parallel-axis case.
		if(o->ParallelAxis)
		{
			if(o->ParallelAxis & 4)
				TMX = (o->RayOrigin.X < nodeBox->X) ? MY_FLTMAX : -MY_FLTMAX;
			if(o->ParallelAxis & 2)
				TMY = (o->RayOrigin.Y < nodeBox->Y) ? MY_FLTMAX : -MY_FLTMAX;
			if(o->ParallelAxis & 1)
				TMZ = (o->RayOrigin.Z < nodeBox->Z) ? MY_FLTMAX : -MY_FLTMAX;
		}

		INT currNode = FindFirstNode(T0X, T0Y, T0Z, TMX, TMY, TMZ);
		do 
		{
			FPlane childBox;
			CalcChildBox(&childBox, nodeBox, currNode ^ o->NodeTransform);

			switch(currNode) 
			{
			case 0:
				Children[0 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, T0X, T0Y, T0Z, TMX, TMY, TMZ, &childBox);
				currNode = GetNextNode(TMX, 4, TMY, 2, TMZ, 1);
				break;
			case 1:
				Children[1 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, T0X, T0Y, TMZ, TMX, TMY, T1Z, &childBox);
				currNode = GetNextNode(TMX, 5, TMY, 3, T1Z, 8);
				break;
			case 2:
				Children[2 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, T0X, TMY, T0Z, TMX, T1Y, TMZ, &childBox);
				currNode = GetNextNode(TMX, 6, T1Y, 8, TMZ, 3);
				break;
			case 3:
				Children[3 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, T0X, TMY, TMZ, TMX, T1Y, T1Z, &childBox);
				currNode = GetNextNode(TMX, 7, T1Y, 8, T1Z, 8);
				break;
			case 4:
				Children[4 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, TMX, T0Y, T0Z, T1X, TMY, TMZ, &childBox);
				currNode = GetNextNode(T1X, 8, TMY, 6, TMZ, 5);
				break;
			case 5:
				Children[5 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, TMX, T0Y, TMZ, T1X, TMY, T1Z, &childBox);
				currNode = GetNextNode(T1X, 8, TMY, 7, T1Z, 8);
				break;
			case 6:
				Children[6 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, TMX, TMY, T0Z, T1X, T1Y, TMZ, &childBox);
				currNode = GetNextNode(T1X, 8, T1Y, 8, TMZ, 7);
				break;
			case 7:
				Children[7 ^ o->NodeTransform].ActorZeroExtentLineCheck(o, TMX, TMY, TMZ, T1X, T1Y, T1Z, &childBox);
				currNode = 8;
				break;
			}
		} while(currNode < 8);
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	Recursive NON-ZERO EXTENT line checker
-----------------------------------------------------------------------------*/
// This assumes that the ray check overlaps this node.
void FOctreeNode::ActorNonZeroExtentLineCheck(FCollisionOctree* o, const FPlane* nodeBox)
{
	guard(FOctreeNode::ActorNonZeroExtentLineCheck);

	for(INT i=0; i<Actors.Num(); i++)
	{
		AActor* testActor = Actors(i);

		if(testActor->CollisionTag != o->CollisionTag)
		{
			testActor->CollisionTag = o->CollisionTag;

			// Check collision.
			if( testActor->bBlockNonZeroExtentTraces &&
				testActor != o->ChkActor &&
				(!o->ChkActor || !o->ChkActor->IsOwnedBy(testActor)) &&
				testActor->ShouldTrace(o->ChkActor, o->ChkTraceFlags) )
			{

#if TIME_OCTREE				
				DWORD Time = 0;			
				clock(Time);
#endif

				// Check line against actor's bounding box
				UBOOL hitActorBox = LINE_BOX(testActor->OctreeBoxCenter, 
					testActor->OctreeBoxRadii + o->ChkExtent, o->ChkStart, o->ChkDir, o->ChkOneOverDir);

#if TIME_OCTREE				
				unclock(Time);
				o->NZE_LineBox_Count++;
				o->NZE_LineBox_Millisec += Time * GSecondsPerCycle * 1000.0f;
#endif


#if !CHECK_FALSE_NEG
				if(!hitActorBox)
					continue;
#endif


#if TIME_OCTREE				
				Time = 0;
				clock(Time);
#endif

				FCheckResult TestHit(0);
				UBOOL lineChkRes = testActor->GetPrimitive()->LineCheck(TestHit, 
					testActor, 
					o->ChkEnd, 
					o->ChkStart, 
					o->ChkExtent, 
					o->ChkExtraNodeFlags, 
					o->ChkTraceFlags)==0;

#if TIME_OCTREE				
				unclock(Time);

				if(testActor->bWasSNFiltered)
				{
					o->NZE_SNF_PrimCount++;
					o->NZE_SNF_PrimMillisec += Time * GSecondsPerCycle * 1000.0f;
				}
				else
				{
					o->NZE_MNF_PrimCount++;
					o->NZE_MNF_PrimMillisec += Time * GSecondsPerCycle * 1000.0f;
				}
#endif

				if(lineChkRes)
				{
#if CHECK_FALSE_NEG
					if(!hitActorBox)
						debugf(TEXT("NZELC False Neg! : %s"), testActor->GetName());
#endif

					FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult(TestHit);
					NewResult->GetNext() = o->ChkResult;
					o->ChkResult = NewResult;

					// If we only wanted one result - our job is done!
					if (o->ChkTraceFlags & TRACE_StopAtFirstHit)
						return;
				}

			}

		}
	}

	// Now traverse children of this node if present.
	if(Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(nodeBox, &o->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			UBOOL hitsChild;

#if TIME_OCTREE
			DWORD Time = 0;
			clock(Time);
#endif
			// First - check extent line against child bounding box. 
			// We expand box it by the extent of the line.
			FPlane childBox;
			CalcChildBox(&childBox, nodeBox, childIXs[i]);

			hitsChild = LINE_BOX(childBox, 
				FVector(childBox.W + o->ChkExtent.X, childBox.W + o->ChkExtent.Y, childBox.W + o->ChkExtent.Z),
				o->ChkStart, o->ChkDir, o->ChkOneOverDir);

#if TIME_OCTREE
			unclock(Time);
			o->NZE_LineBox_Count++;
			o->NZE_LineBox_Millisec += Time * GSecondsPerCycle * 1000.0f;
#endif

			// If ray hits child node - go into it.
			if(hitsChild)
			{
				this->Children[childIXs[i]].ActorNonZeroExtentLineCheck(o, &childBox);

				// If that child resulted in a hit, and we only want one, return now.
				if ( o->ChkResult && (o->ChkTraceFlags & TRACE_StopAtFirstHit) )
					return;
			}
		}
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Recursive encroachment check
-----------------------------------------------------------------------------*/
void FOctreeNode::ActorEncroachmentCheck(FCollisionOctree* o, const FPlane* nodeBox)
{
	guard(FOctreeNode::ActorEncroachmentCheck);

	// o->ChkActor is the non-cylinder thing that is moving (mover, karma etc.).
	// Actors(i) is the thing (Pawn, Volume, Projector etc.) that its moving into.
	for(INT i=0; i<Actors.Num(); i++)
	{
		AActor* testActor = Actors(i);

		// Skip if we've already checked this actor, or we're joined to the encroacher,
		// or this is an encroacher and the other thing is the world (static mesh, terrain etc.)
		if(	testActor->CollisionTag != o->CollisionTag && 
			!testActor->IsJoinedTo(o->ChkActor) &&
			testActor->ShouldTrace(o->ChkActor,o->ChkTraceFlags) &&
			!(o->ChkActor->IsEncroacher() && testActor->bWorldGeometry) )
		{
			testActor->CollisionTag = o->CollisionTag;

#if TIME_OCTREE				
			DWORD Time = 0;			
			clock(Time);
#endif

			// Check bounding boxes against each other
			UBOOL hitActorBox = BoxBoxIntersect(testActor->OctreeBox, o->ChkBox);

#if TIME_OCTREE				
			unclock(Time);
			o->BoxBox_Count++;
			o->BoxBox_Millisec += Time * GSecondsPerCycle * 1000.0f;
#endif


#if !CHECK_FALSE_NEG
			if(!hitActorBox)
				continue;
#endif

			FCheckResult TestHit(1.f);
			if(o->ChkActor->IsOverlapping(testActor, &TestHit))
			{
#if CHECK_FALSE_NEG
				if(!hitActorBox)
					debugf(TEXT("ENC False Neg! : %s %s"), o->ChkActor->GetName(), testActor->GetName());
#endif

				TestHit.Actor     = testActor;
				TestHit.Primitive = NULL;

				FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult(TestHit);
				NewResult->GetNext() = o->ChkResult;
				o->ChkResult = NewResult;
			}
		}
	}

	// Now traverse children of this node if present.
	if(Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(nodeBox, &o->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			FPlane childBox;
			CalcChildBox(&childBox, nodeBox, childIXs[i]);
			this->Children[childIXs[i]].ActorEncroachmentCheck(o, &childBox);
			// JTODO: Should we check TRACE_StopAtFirstHit and bail out early for Encroach check?
		}
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	Recursive point (with extent) check
-----------------------------------------------------------------------------*/
void FOctreeNode::ActorPointCheck(FCollisionOctree* o, const FPlane* nodeBox)
{
	guard(FOctreeNode::ActorPointCheck);

	// First, see if this actors box overlaps tthe query point
	// If it doesn't - return straight away.
	//FBox TestBox = FBox(o->ChkStart - o->ChkExtent, o->ChkStart + o->ChkExtent);
	//if( !Box.Intersect(o->ChkBox) )
	//	return;

	for(INT i=0; i<Actors.Num(); i++)
	{
		AActor* testActor = Actors(i);

		// Skip if we've already checked this actor.
		if(	testActor->bBlockNonZeroExtentTraces &&
			testActor->CollisionTag != o->CollisionTag &&
			testActor->ShouldTrace(NULL, o->ChkTraceFlags) )
		{
			// Collision test.
			testActor->CollisionTag = o->CollisionTag;


#if TIME_OCTREE				
			DWORD Time = 0;			
			clock(Time);
#endif

			// Check actor box against query box.
			UBOOL hitActorBox = BoxBoxIntersect(testActor->OctreeBox, o->ChkBox);

#if TIME_OCTREE				
			unclock(Time);
			o->BoxBox_Count++;
			o->BoxBox_Millisec += Time * GSecondsPerCycle * 1000.0f;
#endif

#if !CHECK_FALSE_NEG
			if(!hitActorBox)
				continue;
#endif

			FCheckResult TestHit(1.f);
			if( testActor->GetPrimitive()->PointCheck( TestHit, testActor, o->ChkStart, o->ChkExtent, 0 )==0 )
			{
				check(TestHit.Actor == testActor);

#if CHECK_FALSE_NEG
				if(!hitActorBox)
					debugf(TEXT("PC False Neg! : %s %s"), testActor->GetName());
#endif

				FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult(TestHit);
				NewResult->GetNext() = o->ChkResult;
				o->ChkResult = NewResult;
				if (o->ChkTraceFlags & TRACE_StopAtFirstHit)
					return;
			}
		}
	}

	// Now traverse children of this node if present.
	if(Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(nodeBox, &o->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			FPlane childBox;
			CalcChildBox(&childBox, nodeBox, childIXs[i]);
			this->Children[childIXs[i]].ActorPointCheck(o, &childBox);
			// JTODO: Should we check TRACE_StopAtFirstHit and bail out early for Encroach check?
		}
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	Recursive radius check
-----------------------------------------------------------------------------*/
void FOctreeNode::ActorRadiusCheck(FCollisionOctree* o, const FPlane* nodeBox)
{
	guard(FOctreeNode::ActorRadiusCheck);

	// First, see if this actors box overlaps tthe query point
	// If it doesn't - return straight away.
	//FBox TestBox = FBox(o->ChkStart - o->ChkExtent, o->ChkStart + o->ChkExtent);
	//if( !Box.Intersect(o->ChkBox) )
	//	return;

	// Skip if we've already checked this actor.
	for(INT i=0; i<Actors.Num(); i++)
	{
		AActor* testActor = Actors(i);
		
		if(testActor->CollisionTag != o->CollisionTag)
		{
			testActor->CollisionTag = o->CollisionTag;

			FBox box = testActor->OctreeBox;
			FVector center = box.GetCenter();

			if( (center - o->ChkStart).SizeSquared() < o->ChkRadiusSqr )
			{
				FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult;
				NewResult->Actor = testActor;
				NewResult->GetNext() = o->ChkResult;
				o->ChkResult = NewResult;
			}
		}
	}

	// Now traverse children of this node if present.
	if(Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(nodeBox, &o->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			FPlane childBox;
			CalcChildBox(&childBox, nodeBox, childIXs[i]);
			this->Children[childIXs[i]].ActorRadiusCheck(o, &childBox);
			// JTODO: Should we check TRACE_StopAtFirstHit and bail out early for Encroach check?
		}
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	Recursive box overlap check
-----------------------------------------------------------------------------*/
void FOctreeNode::ActorOverlapCheck(FCollisionOctree* o, const FPlane* nodeBox)
{
	guard(FOctreeNode::ActorOverlapCheck);

	for(INT i=0; i<Actors.Num(); i++)
	{
		AActor* testActor = Actors(i);
		
		if(	testActor != o->ChkActor && 
			testActor->CollisionTag != o->CollisionTag)
		{
			testActor->CollisionTag = o->CollisionTag;

			// Dont bother if we are only looking for things with bBlockKarma == true
			if(o->ChkBlockKarmaOnly && !testActor->bBlockKarma)
				continue;

			UBOOL hitActorBox = BoxBoxIntersect(testActor->OctreeBox, o->ChkBox);

			if(hitActorBox)
			{
				FCheckResult* NewResult = new(*(o->ChkMem))FCheckResult();
				NewResult->Actor = testActor;
				NewResult->GetNext() = o->ChkResult;
				o->ChkResult = NewResult;
			}
		}
	}

	// Now traverse children of this node if present.
	if(Children)
	{
		INT childIXs[8];
		INT numChildren = FindChildren(nodeBox, &o->ChkBox, childIXs);
		for(INT i=0; i<numChildren; i++)
		{
			FPlane childBox;
			CalcChildBox(&childBox, nodeBox, childIXs[i]);
			this->Children[childIXs[i]].ActorOverlapCheck(o, &childBox);
		}
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	Debug drawing function
-----------------------------------------------------------------------------*/


void FOctreeNode::Draw(FColor DrawColor, UBOOL bAndChildren, const FPlane* nodeBox)
{
	guard(FOctreeNode::Draw);
	
	// Draw this node
	FBox b;
	MakeNodeBox(&b, *nodeBox);
	GTempLineBatcher->AddBox(b, DrawColor);

	// And draw children, if desired.
	if(Children && bAndChildren)
	{
		for(INT i=0; i<8; i++)
		{
			FPlane childBox;
			CalcChildBox(&childBox, nodeBox, i);
			this->Children[i].Draw(DrawColor, bAndChildren, &childBox);
		}
	}

	unguard;
}


void FOctreeNode::DrawFlaggedActors(FCollisionOctree* o, const FPlane* nodeBox)
{
	UBOOL drawn = 0;
	for(INT i=0; i<Actors.Num(); i++)
	{
		if(Actors(i)->bShowOctreeNodes)
		{
			if(!drawn)
			{
				this->Draw(FColor(255, 60, 50), 0, nodeBox);
				drawn = 1;
			}
			GTempLineBatcher->AddBox(Actors(i)->OctreeBox, FColor(255, 0, 255));
		}
	}

	if(Children)
	{
		for(INT i=0; i<8; i++)
		{
			FPlane childBox;
			CalcChildBox(&childBox, nodeBox, i);
			this->Children[i].DrawFlaggedActors(o, &childBox);
		}
	}
}

///////////////////////////////////////////// TREE ////////////////////////////////////////////////

//
// Constructor
//
FCollisionOctree::FCollisionOctree()
{
	guard(FCollisionOctree::FCollisionOctree);

	GOctreeBytesUsed = sizeof(FCollisionOctree);

	CollisionTag = MY_INTMAX/4;

	// Create root node (doesn't use block allocate)
	RootNode = new(TEXT("FCollisionOctree RootNode")) FOctreeNode;
	GOctreeBytesUsed += sizeof(FOctreeNode);

	//RootNode->Centre = FVector(0, 0, 0);
	//RootNode->Radius = HALF_WORLD_MAX;

	// Zero timers.
	ZE_SNF_PrimMillisec = 0;
	ZE_MNF_PrimMillisec = 0;
	NZE_SNF_PrimMillisec = 0;
	NZE_MNF_PrimMillisec = 0;
	BoxBox_Millisec = 0;
	ZE_LineBox_Millisec = 0;
	NZE_LineBox_Millisec = 0;

	ZE_SNF_PrimCount = 0;
	ZE_MNF_PrimCount = 0;
	NZE_SNF_PrimCount = 0;
	NZE_MNF_PrimCount = 0;
	BoxBox_Count = 0;
	ZE_LineBox_Count = 0;
	NZE_LineBox_Count = 0;

	Add_Millisec = 0;
	Remove_Millisec = 0;
	NZELineCheck_Millisec = 0;
	ZELineCheck_Millisec = 0;
	PointCheck_Millisec = 0;
	EncroachCheck_Millisec = 0;
	RadiusCheck_Millisec = 0;

	Add_Count = 0;
	Remove_Count = 0;
	NZELineCheck_Count = 0;
	ZELineCheck_Count = 0;
	PointCheck_Count = 0;
	EncroachCheck_Count = 0;
	RadiusCheck_Count = 0;


	unguard;
}

#define DISPLAY_STAT(name) {if(name##_Count > 0) debugf(TEXT("  ") TEXT(#name) TEXT(" : %f (%d) Average: %f"), name##_Millisec, name##_Count, name##_Millisec/(FLOAT)name##_Count);}

//
// Destructor
//
FCollisionOctree::~FCollisionOctree()
{
	guard(FCollisionOctree::~FCollisionOctree);

#if TIME_OCTREE
	debugf(TEXT(" -- Octree -- "));

	DISPLAY_STAT(Add);
	DISPLAY_STAT(Remove);

	DISPLAY_STAT(NZELineCheck);
	FLOAT nzeptt = NZE_SNF_PrimMillisec + NZE_MNF_PrimMillisec;
	debugf(TEXT("    PrimTest: %f\tLineBox: %f\tOther: %f"), nzeptt, NZE_LineBox_Millisec, NZELineCheck_Millisec - (nzeptt + NZE_LineBox_Millisec));
	debugf(TEXT("    "));

	DISPLAY_STAT(ZELineCheck);
	FLOAT zeptt = ZE_SNF_PrimMillisec + ZE_MNF_PrimMillisec;
	debugf(TEXT("    PrimTest: %f\tLineBox: %f\tOther: %f"), zeptt, ZE_LineBox_Millisec, ZELineCheck_Millisec - (zeptt + ZE_LineBox_Millisec));
	debugf(TEXT("    "));

	DISPLAY_STAT(EncroachCheck);
	DISPLAY_STAT(RadiusCheck);
	DISPLAY_STAT(NZE_LineBox);
	DISPLAY_STAT(ZE_LineBox);
	DISPLAY_STAT(BoxBox);

	debugf(TEXT(" Mem Used: %d bytes"), GOctreeBytesUsed);
	debugf(TEXT("    "));

	debugf(TEXT(" -- Octree PrimTests -- "));
	debugf(TEXT("  ZE: SNF: %f (%d) MNF: %f (%d)"), ZE_SNF_PrimMillisec, ZE_SNF_PrimCount, ZE_MNF_PrimMillisec, ZE_MNF_PrimCount);
	debugf(TEXT(" NZE: SNF: %f (%d) MNF: %f (%d)"), NZE_SNF_PrimMillisec, NZE_SNF_PrimCount, NZE_MNF_PrimMillisec, NZE_MNF_PrimCount);
#endif

	RootNode->RemoveAllActors(this);

	delete RootNode;

	unguard;
}

//
//	Tick
//
void FCollisionOctree::Tick()
{
	guard(FCollisionOctree::Tick);

	// Draw entire Octree.
	//RootNode->Draw(FColor(0,255,255), 1);

	// Draw actors with bShowOctreeNodes set to true
	//RootNode->DrawFlaggedActors(this);

#if 0
	for(INT i=0; i<TmpStart.Num(); i++)
	{
		GTempLineBatcher->AddLine(TmpStart(i), TmpEnd(i), FColor(0, 255, 0));
		GTempLineBatcher->AddBox(TmpBox(i), FColor(70, 255, 70));
	}
#endif

	unguard;
}

//
//	AddActor
//
void FCollisionOctree::AddActor(AActor* Actor)
{
	guard(FCollisionOctree::AddActor);
	check(Actor->bCollideActors);

#if TIME_OCTREE				
	MyClock Dummy(&Remove_Count, &Remove_Millisec);
#endif

	if( !Actor || Actor->bDeleteMe || Actor->bPendingDelete )
		return;

	//debugf(TEXT("Add: %p : %s"), Actor, Actor->GetName());

	// Do nothing if actors location is invalid
	if( !(Actor->Location == Actor->Location) )
	{
		debugf(TEXT("Octree Warning (AddActor): %s Has Invalid Location."), Actor->GetName());
		return;
	}

	// Just to be sure - if the actor is already in the octree, remove it and re-add it.
	if(Actor->OctreeNodes.Num() > 0)
	{
		if( !GIsEditor && (Actor->Level->NetMode != NM_Client) )
			debugf(TEXT("Octree Warning (AddActor): %s Already In Octree."), Actor->GetName());
		RemoveActor(Actor);
	}

	// Grab the actors bounding box just once and store it (this might be expensive)
	// You should never change an actors bounding box while it is still in the Octree.
	Actor->OctreeBox = Actor->GetPrimitive()->GetCollisionBoundingBox(Actor);
	Actor->OctreeBox = Actor->OctreeBox.ExpandBy(ACTORBOX_EPSILON);
	Actor->OctreeBox.GetCenterAndExtents(Actor->OctreeBoxCenter, Actor->OctreeBoxRadii);

	// Check if this actor is within world limits!
	if(	Actor->OctreeBox.Max.X < -HALF_WORLD_MAX || Actor->OctreeBox.Min.X > HALF_WORLD_MAX ||
		Actor->OctreeBox.Max.Y < -HALF_WORLD_MAX || Actor->OctreeBox.Min.Y > HALF_WORLD_MAX ||
		Actor->OctreeBox.Max.Z < -HALF_WORLD_MAX || Actor->OctreeBox.Min.Z > HALF_WORLD_MAX)
	{
		debugf(TEXT("Octree Warning (AddActor): %s Outside World."), Actor->GetName());
		return;
	}

	// If we have not yet started play, filter things into many leaves,
	// this is slower to add, but is more accurate.
	if(Actor->GetLevel() && (!Actor->GetLevel()->GetLevelInfo()->bBegunPlay || Actor->GetLevel()->GetLevelInfo()->bIsSaveGame) )
	{
		Actor->bWasSNFiltered = 0;
		RootNode->MultiNodeFilter(Actor, this, &RootNodeBox);
	}
	else
	{
		Actor->bWasSNFiltered = 1;
		RootNode->SingleNodeFilter(Actor, this, &RootNodeBox);
	}

	// For debugging.
	Actor->ColLocation = Actor->Location;
	unguard;
}

//
//	RemoveActor
//
void FCollisionOctree::RemoveActor(AActor* Actor)
{
	guard(FCollisionOctree::RemoveActor);
	check(Actor->bCollideActors || GIsEditor); // we'll let this go in the editor, even though it indicates something screwed up a bit

#if TIME_OCTREE				
	MyClock Dummy(&Add_Count, &Add_Millisec);
#endif

	if( !Actor )
		return;

#if 0
	if(!GIsEditor && Actor->OctreeNodes.Num() == 0)
	{
		debugf(TEXT("Octree Warning (RemoveActor): %s Not In Octree."), Actor->GetName() );
		return;
	}
#endif

#if 0
	if( !Actor->bDeleteMe )  
	{

		FBox CurrentBox = Actor->GetPrimitive()->GetCollisionBoundingBox(Actor);  
		CurrentBox = CurrentBox.ExpandBy(ACTORBOX_EPSILON); 

		if( (CurrentBox.Max - Actor->OctreeBox.Max).GetAbsMax() > 0.0001f || (CurrentBox.Min - Actor->OctreeBox.Min).GetAbsMax() > 0.0001f )  
		{
			debugf( TEXT("%x %s box changed while in Octree."), Actor, Actor->GetName() );
		}
	}
#endif

	// JTODO: This should check even if bDeleteMe is true! But the Hash doesn't, and sometimes
	// that is the case.
	if( !Actor->bDeleteMe && Actor->Location!=Actor->ColLocation )
	{
		// make sure ColLocation not undefined
		//check(Actor->ColLocation == Actor->ColLocation); 
		if( !(Actor->ColLocation == Actor->ColLocation) )
			debugf( TEXT("Octree Warning (RemoveActor): %s has invalid ColLocation"), Actor->GetName() );
		if ( Actor->Role == ROLE_Authority )
			debugf( TEXT("Octree Warning (RemoveActor): %s moved without proper hashing"), Actor->GetName() );
	}

	//debugf(TEXT("Remove: %p : %s"), Actor, Actor->GetName());

	guard(IterateNodes);
	// Work through this actors list of nodes, removing itself from each one.
	for(INT i=0; i<Actor->OctreeNodes.Num(); i++)
	{
		FOctreeNode* node = (FOctreeNode*)((PTRINT) Actor->OctreeNodes(i));
		check(node);
		node->Actors.RemoveItem(Actor);
	}
	unguard;

	// Then empty the list of nodes.
	Actor->OctreeNodes.Empty();

	unguard;
}

#define ToInfinity(f) ((f) > 0 ? MY_FLTMAX : -MY_FLTMAX)

//
//	ActorLineCheck
//
FCheckResult* FCollisionOctree::ActorLineCheck(FMemStack& Mem, 
											   const FVector& End, 
											   const FVector& Start, 
											   const FVector& Extent, 
											   DWORD TraceFlags, 
											   DWORD ExtraNodeFlags, 
											   AActor *SourceActor)
{
	guard(FCollisionOctree::ActorLineCheck);

#if TIME_OCTREE				
	DWORD* d = Extent.IsZero() ? &ZELineCheck_Count : &NZELineCheck_Count;
	FLOAT* f = Extent.IsZero() ? &ZELineCheck_Millisec : &NZELineCheck_Millisec;

	MyClock Dummy(d, f);
#endif

	// Fill in temporary data.
	CollisionTag++;
	//debugf(TEXT("LINE CHECK: CT: %d"), CollisionTag);

	ChkResult = NULL;

	ChkMem = &Mem;
	ChkEnd = End;
	ChkStart = Start;
	ChkExtent = Extent;
	ChkTraceFlags = TraceFlags;
	ChkExtraNodeFlags = ExtraNodeFlags;
	ChkActor = SourceActor;

	if (SourceActor && SourceActor->bUseCollisionStaticMesh)
		ChkExtraNodeFlags |= NF_UseCollisionStaticMesh;

	ChkDir = (End-Start);
	ChkOneOverDir = FVector(1.0f/ChkDir.X, 1.0f/ChkDir.Y, 1.0f/ChkDir.Z);

	ChkFirstResult = NULL;

	// This will recurse down, adding results to ChkResult as it finds them.
	// Taken from the Revelles/Urena/Lastra paper: http://wscg.zcu.cz/wscg2000/Papers_2000/X31.pdf
	if(Extent.IsZero())
	{		
		FVector RayDir = ChkDir;
		RayOrigin = ChkStart;
		NodeTransform = 0;
		ParallelAxis = 0;

		if(RayDir.X < 0.0f)
		{
			RayOrigin.X = -RayOrigin.X;
			RayDir.X = -RayDir.X;
			NodeTransform |= 4;
		}

		if(RayDir.Y < 0.0f)
		{
			RayOrigin.Y = -RayOrigin.Y;
			RayDir.Y = -RayDir.Y;
			NodeTransform |= 2;
		}

		if(RayDir.Z < 0.0f)
		{
			RayOrigin.Z = -RayOrigin.Z;
			RayDir.Z = -RayDir.Z;
			NodeTransform |= 1;
		}

		// T's should be between 0 and 1 for a hit on the tested ray.
		FVector T0, T1;


		// Check for parallel cases.
		// X //
		if(RayDir.X > 0.0f)
		{
			T0.X = (NODEXMIN(&RootNodeBox) - RayOrigin.X)/RayDir.X;
			T1.X = (NODEXMAX(&RootNodeBox)  - RayOrigin.X)/RayDir.X;
		}
		else
		{
			T0.X = ToInfinity(NODEXMIN(&RootNodeBox) - RayOrigin.X);
			T1.X = ToInfinity(NODEXMAX(&RootNodeBox) - RayOrigin.X);
			ParallelAxis |= 4;
		}

		// Y //
		if(RayDir.Y > 0.0f)
		{
			T0.Y = (NODEYMIN(&RootNodeBox) - RayOrigin.Y)/RayDir.Y;
			T1.Y = (NODEYMAX(&RootNodeBox) - RayOrigin.Y)/RayDir.Y;
		}
		else
		{
			T0.Y = ToInfinity(NODEYMIN(&RootNodeBox) - RayOrigin.Y);
			T1.Y = ToInfinity(NODEYMAX(&RootNodeBox) - RayOrigin.Y);
			ParallelAxis |= 2;
		}

		// Z //
		if(RayDir.Z > 0.0f)
		{
			T0.Z = (NODEZMIN(&RootNodeBox) - RayOrigin.Z)/RayDir.Z;
			T1.Z = (NODEZMAX(&RootNodeBox) - RayOrigin.Z)/RayDir.Z;
		}
		else
		{
			T0.Z = ToInfinity(NODEZMIN(&RootNodeBox) - RayOrigin.Z);
			T1.Z = ToInfinity(NODEZMAX(&RootNodeBox) - RayOrigin.Z);
			ParallelAxis |= 1;
		}

		// Only traverse if ray hits RootNode box.
		if(T0.GetMax() < T1.GetMax())
		{
			RootNode->ActorZeroExtentLineCheck(this, T0.X, T0.Y, T0.Z, T1.X, T1.Y, T1.Z, &RootNodeBox);
		}

		// Only return one (first) result if TRACE_SingleResult set.
		if(TraceFlags & TRACE_SingleResult)
		{
			ChkResult = ChkFirstResult;
			if(ChkResult)
				ChkResult->GetNext() = NULL;
		}
	}
	else
	{
		// Create box around fat ray check.
		ChkBox = FBox(0);
		ChkBox += Start;
		ChkBox += End;
		ChkBox.Min -= Extent;
		ChkBox.Max += Extent;

		// Then recurse through Octree
		RootNode->ActorNonZeroExtentLineCheck(this, &RootNodeBox);
	}

	// If TRACE_SingleResult, only return 1 result (the first hit).
	// This code has to ignore fake-backdrop hits during shadow casting though (can't do that in ShouldTrace)
	if(ChkResult && TraceFlags & TRACE_SingleResult)
	{
		return FindFirstResult(ChkResult, TraceFlags);
	}

	return ChkResult;
	
	unguard;
}

//
//	ActorPointCheck
//
FCheckResult* FCollisionOctree::ActorPointCheck(FMemStack& Mem, 
												const FVector& Location, 
												const FVector& Extent, 
												DWORD TraceFlags, 
												DWORD ExtraNodeFlags, 
												UBOOL bSingleResult)
{
	guard(FCollisionOctree::ActorPointCheck);

#if TIME_OCTREE				
	MyClock Dummy(&PointCheck_Count, &PointCheck_Millisec);
#endif

	// Fill in temporary data.
	CollisionTag++;
	ChkResult = NULL;

	ChkMem = &Mem;
	ChkStart = Location;
	ChkExtent = Extent;
	ChkTraceFlags = TraceFlags;
	ChkExtraNodeFlags = ExtraNodeFlags;
	if(bSingleResult)
		ChkTraceFlags |= TRACE_StopAtFirstHit;
	ChkBox = FBox(ChkStart - ChkExtent, ChkStart + ChkExtent);

	RootNode->ActorPointCheck(this, &RootNodeBox);

	return ChkResult;

	unguard;
}

//
//	ActorRadiusCheck
//
FCheckResult* FCollisionOctree::ActorRadiusCheck(FMemStack& Mem, 
												 const FVector& Location, 
												 FLOAT Radius, 
												 DWORD ExtraNodeFlags)
{
	guard(FCollisionOctree::ActorRadiusCheck);

#if TIME_OCTREE				
	MyClock Dummy(&RadiusCheck_Count, &RadiusCheck_Millisec);
#endif

	// Fill in temporary data.
	CollisionTag++;
	ChkResult = NULL;

	ChkMem = &Mem;
	ChkStart = Location;
	ChkRadiusSqr = Radius * Radius;
	ChkExtraNodeFlags = ExtraNodeFlags;
	ChkBox = FBox(Location - FVector(Radius, Radius, Radius), Location + FVector(Radius, Radius, Radius));

	RootNode->ActorRadiusCheck(this, &RootNodeBox);

	return ChkResult;

	unguard;
}

//
//	ActorEncroachmentCheck
//
FCheckResult* FCollisionOctree::ActorEncroachmentCheck(FMemStack& Mem, 
													   AActor* Actor, 
													   FVector Location, 
													   FRotator Rotation, 
													   DWORD TraceFlags, 
													   DWORD ExtraNodeFlags)
{
	guard(FCollisionOctree::ActorEncroachmentCheck);

#if TIME_OCTREE				
	MyClock Dummy(&EncroachCheck_Count, &EncroachCheck_Millisec);
#endif

	// Fill in temporary data.
	CollisionTag++;
	ChkResult = NULL;

	ChkMem = &Mem;
	ChkActor = Actor;
	ChkTraceFlags = TraceFlags;
	ChkExtraNodeFlags = ExtraNodeFlags;

	// Save actor's location and rotation.
	Exchange( Location, Actor->Location );
	Exchange( Rotation, Actor->Rotation );

	// Get bounding box at new location.
	ChkBox = Actor->GetPrimitive()->GetCollisionBoundingBox(Actor);

	if(ChkBox.IsValid)
		RootNode->ActorEncroachmentCheck(this, &RootNodeBox);

	// Restore actor's location and rotation.
	Exchange( Location, Actor->Location );
	Exchange( Rotation, Actor->Rotation );

	return ChkResult;

	unguard;
}

//
//	ActorOverlapCheck
//
FCheckResult* FCollisionOctree::ActorOverlapCheck(FMemStack& Mem, 
												  AActor* Actor, 
												  FBox* Box,
												  UBOOL bBlockKarmaOnly)
{
	guard(FCollisionOctree::ActorOverlapCheck);

	CollisionTag++;
	ChkResult = NULL;

	ChkBox = *Box;
	ChkActor = Actor;
	ChkMem = &Mem;
	ChkBlockKarmaOnly = bBlockKarmaOnly;

	if(ChkBox.IsValid)
		RootNode->ActorOverlapCheck(this, &RootNodeBox);

	return ChkResult;

	unguard;
}

//
//	CheckActorNotReferenced
//
void FCollisionOctree::CheckActorNotReferenced( AActor* Actor )
{
	guard(FCollisionOctree::CheckActorNotReferenced);
	#if DO_CHECK_SLOW
	if( GIsSoaking && !GIsEditor )
		RootNode->CheckActorNotReferenced(Actor);
	#endif
	unguard;	
}

//
//	CheckIsEmpty
//
void FCollisionOctree::CheckIsEmpty()
{
	guard(FCollisionOctree::CheckIsEmpty);
	RootNode->CheckIsEmpty();
	unguard;
}

// Comapares the two arrays to see if they contain the same nodes,
// but not necessarily in the same order. Returns true if they are the same.
static UBOOL CompareArray(TArray<FOctreeNode*> *Array1, TArray<PTRINT> *Array2)
{
	UBOOL isDifferent = 0;
	for(INT i=0; i<Array1->Num() && !isDifferent; i++)
	{
		UBOOL foundMatch = 0;
		FOctreeNode* n1 = (*Array1)(i);
		for(INT j=0; j<Array2->Num() && !foundMatch; j++)
		{
			if( n1 == (FOctreeNode*)(*Array2)(j) )
				foundMatch = 1;
		}
		if(!foundMatch)
			isDifferent = 1;
	}

	return !isDifferent;
}

//
//	CheckActorLocations
//  Debugging - see if all actors in this level are in the right nodes of the octree.
//
void FCollisionOctree::CheckActorLocations(ULevel *level)
{
	guard(FCollisionOctree::CheckActorLocations);

	for(INT i=0; i<level->Actors.Num(); i++)
	{
		AActor* actor = level->Actors(i);
		if(actor && actor->bCollideActors && !actor->bDeleteMe)
		{
			FBox testBox = actor->OctreeBox;
			TArray<FOctreeNode*> Nodes;
		
			if(actor->bWasSNFiltered)
				RootNode->FilterTest(&testBox, 0, &Nodes, &RootNodeBox);
			else
				RootNode->FilterTest(&testBox, 1, &Nodes, &RootNodeBox);

			if(!CompareArray(&Nodes, &actor->OctreeNodes))
			{
				debugf(TEXT("Octree Warning: Actor %s not in sync with Octree."), actor->GetName());
			}
		}
	}
	
	unguard;
}

