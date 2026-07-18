/*=============================================================================
	UnReach.cpp: Reachspec creation and management

	These methods are members of the UReachSpec class, 

	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Steven Polge 3/97
=============================================================================*/
#include "EnginePrivate.h"
#include "UnPath.h"

void UReachSpec::Init()
{
	guard(UReachSpec::Init);
	// Init everything here.
	Start = End = NULL;
	Distance = CollisionRadius = CollisionHeight = 0;
	reachFlags = 0;
	bPruned = 0;
	bForced = 0;
	MaxLandingVelocity = 0;
	unguard;
};

/* Path Color
returns the color to use when displaying this path in the editor
 */
FPlane UReachSpec::PathColor()
{
	guard(UReachSpec::PathColor);


	if ( (reachFlags & R_SPECIAL) && End->IsA(ATeleporter::StaticClass()) )
		return FPlane(1.f,0.f,1.f, 0.f);	// purple path = special (lift or teleporter)

	if ( reachFlags & R_FORCED ) // yellow for forced paths
		return FPlane(1.f, 1.f, 0.f, 0.f);

	if ( reachFlags & R_PROSCRIBED ) // red is reserved for proscribed paths
		return FPlane(1.f, 0.f, 0.f, 0.f);

	if ( (reachFlags & R_FLY) && !(reachFlags & R_WALK) )
		return FPlane(1.f,0.5f,0.f,0.f);	// orange = flying

	if ( reachFlags & R_SPECIAL )
		return FPlane(1.f,0.f,1.f, 0.f);	// purple path = special (lift or teleporter)

	if ( reachFlags & R_LADDER )
		return FPlane(1.f,0.5f, 1.f,0.f);	// light purple = ladder

	if ( (CollisionRadius >= MAXVEHICLERADIUS) && (CollisionHeight >= MINCOMMONHEIGHT)
			&& !(reachFlags & R_FLY) )
		return FPlane(1.f,1.f,1.f,0.f);  // white path = super wide

	if ( CollisionRadius >= MAXCOMMONRADIUS && !(reachFlags & R_FLY) )
		return FPlane(0.5f,1.f,0.5f,0.f);  // light green path = very wide

	if ( (CollisionRadius >= COMMONRADIUS) && (CollisionHeight >= MINCOMMONHEIGHT)
			&& !(reachFlags & R_FLY) )
		return FPlane(0.f,1.f,0.f,0.f);  // green path = wide

	return FPlane(0.f,0.f,1.f,0.f); // blue path = narrow
	unguard;
}

int UReachSpec::BotOnlyPath()
{
	guard(UReachSpec::BotOnlyPath);

	return ( CollisionRadius < MINCOMMONRADIUS );
	
	unguard;
}
/* 
+ adds two reachspecs - returning the combined reachability requirements and total distance 
Note that Start and End are not set
*/
UReachSpec* UReachSpec::operator+ (const UReachSpec &Spec) const
{
	guard(UReachSpec::operator+);
	UReachSpec* Combined = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetOuter(),NAME_None,RF_Public);
	
	Combined->CollisionRadius = Min(CollisionRadius, Spec.CollisionRadius);
	Combined->CollisionHeight = Min(CollisionHeight, Spec.CollisionHeight);
	Combined->reachFlags = (reachFlags | Spec.reachFlags);
	Combined->Distance = Distance + Spec.Distance;
	Combined->MaxLandingVelocity = ::Max(MaxLandingVelocity, Spec.MaxLandingVelocity);

	return Combined; 
	unguard;
}
/* operator <=
Used for comparing reachspecs reach requirements
less than means that this has easier reach requirements (equal or easier in all categories,
does not compare distance, start, and end
*/
int UReachSpec::operator<= (const UReachSpec &Spec)
{
	guard(UReachSpec::operator<=);
	int result =  
		(CollisionRadius >= Spec.CollisionRadius) &&
		(CollisionHeight >= Spec.CollisionHeight) &&
		((reachFlags | Spec.reachFlags) == Spec.reachFlags) &&
		(MaxLandingVelocity <= ::Max(TESTSTANDARDFALLSPEED,Spec.MaxLandingVelocity));

	return result; 
	unguard;
}

/* operator ==
Used for comparing reachspecs for choosing the best one
does not compare start and end
*/
int UReachSpec::operator== (const UReachSpec &Spec)
{
	guard(UReachSpec::operator ==);
	int result = (Distance == Spec.Distance) && 
		(CollisionRadius == Spec.CollisionRadius) &&
		(CollisionHeight == Spec.CollisionHeight) &&
		(reachFlags == Spec.reachFlags) &&
		((MaxLandingVelocity <= TESTSTANDARDFALLSPEED) == (Spec.MaxLandingVelocity <= TESTSTANDARDFALLSPEED));
	
	return result; 
	unguard;
}

void AScout::InitForPathing()
{
	guard(AScout::InitForPathing);

	Physics = PHYS_Walking;
	JumpZ = TESTJUMPZ; 
	bCanWalk = 1;
	bJumpCapable = 1;
	bCanJump = 1;
	bCanSwim = 1;
	bCanClimbLadders = 1;
	bCanFly = 0;
	GroundSpeed = TESTGROUNDSPEED;
	MaxFallSpeed = TESTMAXFALLSPEED;
	unguard;
}

/* defineFor()
initialize the reachspec for a  traversal from start actor to end actor.
Note - this must be a direct traversal (no routing).
Returns 1 if the definition was successful (there is such a reachspec), and zero
if no definition was possible
*/

int UReachSpec::defineFor(ANavigationPoint *begin, ANavigationPoint *dest, APawn *ScoutPawn)
{
	guard(UReachSpec::defineFor);

	Start = begin;
	End = dest;
	AScout *Scout = Cast<AScout>(ScoutPawn);
	Scout->InitForPathing();
	Start->PrePath();
	End->PrePath();
	debugf(TEXT("Define path from %s to %s"),begin->GetName(), dest->GetName());
	INT result = findBestReachable((AScout *)Scout);
	Start->PostPath();
	End->PostPath();
	return result;
	unguard;
}

/* findBestReachable()

  This is the function that determines what radii and heights of pawns are checked for setting up reachspecs

  Modify this function if you want a different set of parameters checked

  Note that reachable regions are also determined by this routine, so allow a wide max radius to be tested

  What is checked currently (in order):

	Crouched human (HUMANRADIUS, CROUCHEDHUMANHEIGHT)
	Standing human (HUMANRADIUS, HUMANHEIGHT)
	Small non-human
	Common non-human (COMMONRADIUS, COMMONHEIGHT)
	Large non-human (MAXCOMMONRADIUS, MAXCOMMONHEIGHT)

	Then, with the largest height that was successful, determine the reachable region

  TODO: it might be a good idea to look at what pawns are referenced by the level, and check for all their radii/height combos.
  This won't work for a UT type game where pawns are dynamically loaded

*/

INT UReachSpec::findBestReachable(AScout * Scout)
{
	guard(UReachSpec::findBestReachable);

	// if MINCOMMONRADIUS AND HEIGHT ARE SMALLER THAN HUMANRADIUS AND crouched HUMANHEIGHT, TRY THEM FIRST
	FVector TestPawnSize[8];
	if ( (MINCOMMONRADIUS < HUMANRADIUS) && (MINCOMMONHEIGHT < CROUCHEDHUMANHEIGHT) )
	{
		TestPawnSize[0] = FVector(MINCOMMONRADIUS, MINCOMMONHEIGHT, 0.f);
		TestPawnSize[1] = FVector(HUMANRADIUS,CROUCHEDHUMANHEIGHT, 0.f);
		TestPawnSize[2] = FVector(HUMANRADIUS,HUMANHEIGHT, 0.f);
	}
	else
	{
		TestPawnSize[0] = FVector(HUMANRADIUS,CROUCHEDHUMANHEIGHT, 0.f);
		if ( (MINCOMMONRADIUS < HUMANRADIUS) && (MINCOMMONHEIGHT < HUMANHEIGHT) )
		{
			TestPawnSize[1] = FVector(MINCOMMONRADIUS, MINCOMMONHEIGHT, 0.f);
			TestPawnSize[2] = FVector(HUMANRADIUS,HUMANHEIGHT, 0.f);
		}
		else
		{
			TestPawnSize[1] = FVector(HUMANRADIUS,HUMANHEIGHT, 0.f);
			TestPawnSize[2] = FVector(MINCOMMONRADIUS, MINCOMMONHEIGHT, 0.f);
		}
	}

	TestPawnSize[3] = FVector(COMMONRADIUS, MINCOMMONHEIGHT, 0.f);
	TestPawnSize[4] = FVector(COMMONRADIUS, COMMONHEIGHT, 0.f);
	TestPawnSize[5] = FVector(MAXCOMMONRADIUS, COMMONHEIGHT, 0.f);
	TestPawnSize[6] = FVector(MAXCOMMONRADIUS, MAXCOMMONHEIGHT, 0.f);
	TestPawnSize[7] = FVector(MAXVEHICLERADIUS, MAXCOMMONHEIGHT, 0.f);

	// Crouched human (HUMANRADIUS, CROUCHEDHUMANHEIGHT)
	FLOAT bestRadius = TestPawnSize[0].X;
	FLOAT bestHeight = TestPawnSize[0].Y;
	Scout->SetCollisionSize( bestRadius, bestHeight );

	INT	success = PlaceScout( Scout );
	if ( success )
	{
		FCheckResult Hit(1.f);
		Scout->MaxLandingVelocity = 0.f;
		FVector ViewPoint = Start->Location;
		ViewPoint.Z += Start->CollisionHeight;
		Scout->GetLevel()->SingleLineCheck(Hit, Scout, End->Location, ViewPoint, TRACE_World);
		if ( Hit.Time != 1.f )
		{
			Scout->GetLevel()->SingleLineCheck(Hit, Scout, End->Location, Scout->Location, TRACE_World);
			if( Hit.Time!=1.f )
				return 0;
		}
		success = Scout->actorReachable(End,1,1); 
	}

	if ( !success )
		return 0; // No path at all

	MaxLandingVelocity = (int)(Scout->MaxLandingVelocity);
	reachFlags = success;

	// Find largest supported pawn size
	for ( INT i=1; i<8; i++ )
	{
		Scout->SetCollisionSize( TestPawnSize[i].X, TestPawnSize[i].Y );
		INT	success = PlaceScout( Scout );
		if ( success )
			success = Scout->actorReachable(End,1,1); 
		if ( success )
		{
			bestRadius = TestPawnSize[i].X;
			bestHeight = TestPawnSize[i].Y;
		}
		else
			break;
	}

	// init path based on results
	CollisionRadius = (INT) bestRadius;
	CollisionHeight = (INT) bestHeight;
	Distance = (int)(End->Location - Start->Location).Size(); 
	if ( reachFlags & R_SWIM )
		Distance = (INT) (Distance * SWIMCOSTMULTIPLIER);
	return 1; 
	unguard;
}

UBOOL UReachSpec::PlaceScout(AScout * Scout)
{
	guard(UReachSpec::PlaceScout);

	// try placing above and moving down
	FCheckResult Hit(1.f);
	UBOOL bSuccess = false;

	if ( Start->Base )
	{
	FVector Up(0.f,0.f, Scout->CollisionHeight - Start->CollisionHeight + ::Max(0.f, Scout->CollisionRadius - Start->CollisionRadius)); 
	if ( Scout->GetLevel()->FarMoveActor(Scout, Start->Location + Up) )
	{
			bSuccess = true;
		Scout->GetLevel()->MoveActor(Scout, -1.f * Up, Scout->Rotation, Hit);
	}
	}
	if ( !bSuccess && !Scout->GetLevel()->FarMoveActor(Scout, Start->Location) )
		return false;

	// if scout is walking, make sure it is on the ground
	if ( (Scout->Physics == PHYS_Walking) && !Scout->PhysicsVolume->bWaterVolume )
		Scout->GetLevel()->MoveActor(Scout, FVector(0,0,-1*Start->CollisionHeight), Scout->Rotation, Hit);
	return true;
	unguard;
}

