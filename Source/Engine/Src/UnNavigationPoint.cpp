/*=============================================================================
	UnNavigationPoint.cpp: 

  NavigationPoint and subclass functions

	Copyright 2001-2003 Epic Games, Inc. All Rights Reserved.

  	Revision history:
		* Created by Steven Polge 
=============================================================================*/

#include "EnginePrivate.h"
#include "UnPath.h"

void ANavigationPoint::SetVolumes(const TArray<class AVolume*>& Volumes)
{
	guard(ANavigationPoint::SetVolumes);

	Super::SetVolumes( Volumes );

	if ( PhysicsVolume )
		bMayCausePain = (PhysicsVolume->DamagePerSec != 0);

	unguard;
}

void ANavigationPoint::SetVolumes()
{
	guard(ANavigationPoint::SetVolumes);

	Super::SetVolumes();

	if ( PhysicsVolume )
		bMayCausePain = (PhysicsVolume->DamagePerSec != 0);

	unguard;
}

void ANavigationPoint::execSetBaseDistance( FFrame& Stack, RESULT_DECL )
{
	guard(ANavigationPoint::execSetBaseDistance);

	P_GET_INT(BaseNum);
	P_FINISH;

	SetBaseDistance(BaseNum, 0.f);
	unguard;
}

void ANavigationPoint::SetBaseDistance(INT BaseNum, FLOAT CurrentDist)
{
	guard(ANavigationPoint::SetBaseDistance);

	if ( BaseDist[BaseNum] <= CurrentDist )
		return;
    BaseDist[BaseNum] = CurrentDist;

	for ( INT i=0; i<PathList.Num(); i++ )
		if ( ((PathList(i)->reachFlags & R_PROSCRIBED) == 0) && (PathList(i)->Distance >= 0.f) )
			PathList(i)->End->SetBaseDistance(BaseNum, CurrentDist + PathList(i)->Distance); 

	unguard;
}

UBOOL ANavigationPoint::CanReach(ANavigationPoint *Dest, FLOAT Dist, UBOOL bUseFlag)
{
	guard(ANavigationPoint::CanReach)

//	if ( Dest->bMustBeReachable )
//		debugf(TEXT("%s CanReach %s visitedWeight %d dist %f"),GetName(),Dest->GetName(),visitedWeight,Dist);

	if ( (bUseFlag && bCanReach) || (this == Dest) )
	{
		bCanReach = true;
		return true;
	}
	if ( visitedWeight >= Dist )
		return false;

	visitedWeight = Dist;
	if ( Dist <= 1.f )
		return false;

	for ( INT i=0; i<PathList.Num(); i++ )
		if ( (PathList(i)->reachFlags & R_PROSCRIBED) == 0 )
		{
//			if ( Dest->bMustBeReachable )
//			debugf(TEXT("%s Path distance %d to %s"), GetName(), PathList(i)->Distance, PathList(i)->End->GetName());
			if( PathList(i)->Distance <= 1.f )
			{
				debugf(TEXT("%s negative or zero distance to %s!"), GetName(),PathList(i)->End->GetName());
				GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf(TEXT("negative or zero distance to %s!"), PathList(i)->End->GetName()));
			}
			else if ( PathList(i)->End->CanReach(Dest, Dist - PathList(i)->Distance, false) )
			{
				bCanReach = true;
				return true;
			}
		}

	return false;
	unguard;
}

UBOOL ARoadPathNode::ReviewPath(APawn* Scout)
{
	return true;
}

UBOOL AFlyingPathNode::ReviewPath(APawn* Scout)
{
	return true;
}

UBOOL ANavigationPoint::ReviewPath(APawn* Scout)
{
	guard(ANavigationPoint::ReviewPath)

	if ( bMustBeReachable )
	{
		for ( ANavigationPoint* M=Level->NavigationPointList; M!=NULL; M=M->nextNavigationPoint )
			M->bCanReach = false;

		// check that all other paths can reach me
		INT NumFailed = 0;
		for ( ANavigationPoint* N=Level->NavigationPointList; N!=NULL; N=N->nextNavigationPoint )
		{
			for ( ANavigationPoint* M=Level->NavigationPointList; M!=NULL; M=M->nextNavigationPoint )
				M->visitedWeight = 0.f;
			if ( !N->CanReach(this, 10000000.f, true) )
			{
				GWarn->MapCheck_Add( MCTYPE_ERROR, N, *FString::Printf(TEXT("Cannot reach %s from this node!"), GetName()));
				NumFailed++;
				if ( NumFailed > 8 )
					break;
			}
		}
	}

	// check that there aren't any really long paths from here
	for ( INT i=0; i<PathList.Num(); i++ )
		if ( ((PathList(i)->reachFlags & (R_PROSCRIBED | R_FORCED | R_JUMP)) == 0) && (PathList(i)->Distance > 1000.f) 
			&& (Abs(PathList(i)->Start->Location.Z - PathList(i)->End->Location.Z) < 500.f) )
		{
			// check if alternate route
			UBOOL bFoundRoute = false;
			for ( ANavigationPoint* M=Level->NavigationPointList; M!=NULL; M=M->nextNavigationPoint )
			{
				M->visitedWeight = 0.f;
				M->bCanReach = false;
			}

			for ( INT j=0; j<PathList.Num(); j++ )
				if ( (PathList(j)->Distance < 700.f)
					&& PathList(j)->End->CanReach(PathList(i)->End,::Max((PathList(i)->Distance - PathList(j)->Distance) * 1.25f,700.f), false) )
				{
					bFoundRoute = true;
					break;
				}
			if ( !bFoundRoute )
				GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("Path to %s is very long - add a pathnode in between."), PathList(i)->End->GetName()));
		}
	// do walks from this path, check that when lose this path, find another
	// FIXME implement

	return true;
	unguard;
}

UBOOL AJumpDest::ReviewPath(APawn* Scout)
{
	guard(AJumpDest::ReviewPath)

	// check there is a forced path to me
	if ( !bOptionalJumpDest )
	{
		UBOOL bHasForcedPath = false;
		for ( ANavigationPoint* N=Level->NavigationPointList; N!=NULL; N=N->nextNavigationPoint )
			if ( N != this )
				for ( INT i=0; i<N->PathList.Num(); i++ )
					if ( (N->PathList(i)->End == this) && ((N->PathList(i)->reachFlags & R_FORCED) != 0) )
					{
						bHasForcedPath = true;
						break;
					}
		if ( !bHasForcedPath )
			GWarn->MapCheck_Add( MCTYPE_WARNING, this, TEXT("JumpDest has no forced paths to it."));
	}
	Super::ReviewPath(Scout);
	return true;
	unguard;
}

UBOOL APathNode::ReviewPath(APawn* Scout)
{
	guard(APathNode::ReviewPath)

	// check if should be JumpDest
	for ( INT i=0; i<PathList.Num(); i++ )
		if ( (PathList(i)->reachFlags & R_PROSCRIBED) == 0 )
			PathList(i)->End->CheckSymmetry(this);

	Super::ReviewPath(Scout);
	return true;
	unguard;
}

void APathNode::CheckSymmetry(ANavigationPoint* Other)
{
	guard(APathNode::CheckSymmetry)

	for ( INT i=0; i<PathList.Num(); i++ )
		if ( (PathList(i)->End == Other) && ((PathList(i)->reachFlags & R_PROSCRIBED) == 0) )
			return;

	// check for short enough alternate path to warrant no symmetry
	FLOAT Dist = (Location - Other->Location).Size();
	for ( ANavigationPoint* N=Level->NavigationPointList; N!=NULL; N=N->nextNavigationPoint )
	{
		N->bCanReach = false;
		N->visitedWeight = 0.f;
	}
	if ( CanReach(Other,Dist * 1.5f * PATHPRUNING,false) )
		return;

	GWarn->MapCheck_Add( MCTYPE_ERROR, Other, *FString::Printf(TEXT("Should be JumpDest for %s!"), GetName()));
	unguard;
}

UReachSpec* ANavigationPoint::GetReachSpecTo(ANavigationPoint *Nav)
{
	guard(ANavigationPoint::GetReachSpecTo)

	for (INT i=0; i<PathList.Num(); i++ )
		if ( PathList(i)->End == Nav ) 
			return PathList(i);
	return NULL;
	unguard;
}

AActor* AActor::AssociatedLevelGeometry()
{
	guard(AActor::AssociatedLevelGeometry);

	if ( bWorldGeometry )
		return this;

	return NULL;
	unguard;
}

AActor* ADoor::AssociatedLevelGeometry()
{
	guard(ADoor::AssociatedLevelGeometry);

	return MyDoor;
	unguard;
}

UBOOL AActor::HasAssociatedLevelGeometry(AActor *Other)
{
	guard(AActor::HasAssociatedLevelGeometry);

	return ( bWorldGeometry && (Other == this) );
	unguard;
}

UBOOL ADoor::HasAssociatedLevelGeometry(AActor *Other)
{
	guard(ADoor::HasAssociatedLevelGeometry);

	if ( !Other )
		return false;

	for ( AMover *Mine=MyDoor; Mine!=NULL; Mine=Mine->Follower )
		if ( Mine == Other )
			return true;

	return false;
	unguard;
}

/* if navigationpoint is moved, paths are invalid
*/
void ANavigationPoint::PostEditMove()
{
	guard(ANavigationPoint::PostEditMove);
	Level->bPathsRebuilt = 0;
	bPathsChanged = true;
	unguard;
}

/* if navigationpoint is spawned, paths are invalid
*/
void ANavigationPoint::Spawned()
{
	guard(ANavigationPoint::Spawned);
	Level->bPathsRebuilt = 0;
	bPathsChanged = true;
	unguard;
}

/* if navigationpoint is spawned, paths are invalid
NOTE THAT NavigationPoints should not be destroyed during gameplay!!!
*/
void ANavigationPoint::Destroy()
{
	guard(ANavigationPoint::Destroy);

	AActor::Destroy();

	if ( GIsEditor )
	{
		Level->bPathsRebuilt = 0;

		// clean up reachspecs referring to this NavigationPoint
		for ( INT i=0; i<PathList.Num(); i++ )
			if ( PathList(i) )
				PathList(i)->Start = NULL;

		for ( INT i=0; i<GetLevel()->Actors.Num(); i++ )
		{
			ANavigationPoint *Nav = Cast<ANavigationPoint>(GetLevel()->Actors(i));
			if ( Nav )
			{
				for ( INT j=0; j<Nav->PathList.Num(); j++ )
					if ( Nav->PathList(j) && Nav->PathList(j)->End == this )
					{
						Nav->PathList(j)->bPruned = true;
						Nav->PathList(j)->End = NULL;
					}
				Nav->CleanUpPruned();
			}
		}
	}

	unguard;
}

void ANavigationPoint::CleanUpPruned()
{
	guard(ANavigationPoint::CleanUpPruned);

	for ( INT i=PathList.Num()-1; i>=0; i-- )
		if ( PathList(i) && PathList(i)->bPruned )
			PathList.Remove(i);
	PathList.Shrink();
	unguard;
}

/* ProscribedPathTo()
returns 2 if no path is permited because L.D. proscribed it
returns 1 if no path from this to dest is permited for other reasons
*/
INT ANavigationPoint::ProscribedPathTo(ANavigationPoint *Nav)
{
	guard(ANavigationPoint::ProscribedPathTo);

	if ( (bOneWayPath && (((Nav->Location - Location) | Rotation.Vector()) <= 0))
		|| ((Location - Nav->Location).SizeSquared() > MAXPATHDISTSQ) )
		return 1;
					 
	// see if destination is in list of proscribed paths
	for (INT j=0; j<4; j++ )
		if ( Nav->IsIdentifiedAs(ProscribedPaths[j]) )
			return 2;

	return 0;
	unguard;
}

/* ProscribedPathTo()
returns 2 if no path is permited because L.D. proscribed it
returns 1 if no path from this to dest is permited for other reasons
*/
INT ARoadPathNode::ProscribedPathTo(ANavigationPoint *Nav)
{
	guard(ARoadPathNode::ProscribedPathTo);

	if ( (bOneWayPath && (((Nav->Location - Location) | Rotation.Vector()) <= 0)) )
		return 1;

	if ( (Location - Nav->Location).SizeSquared() > (Cast<ARoadPathNode>(Nav) ? (MaxRoadDist * MaxRoadDist) : MAXPATHDISTSQ) )
		return 1;
					 
	// see if destination is in list of proscribed paths
	for (INT j=0; j<4; j++ )
		if ( Nav->IsIdentifiedAs(ProscribedPaths[j]) )
			return 2;

	return 0;
	unguard;
}

INT AFlyingPathNode::ProscribedPathTo(ANavigationPoint *Nav)
{
	guard(AFlyingPathNode::ProscribedPathTo);

	if ( (bOneWayPath && (((Nav->Location - Location) | Rotation.Vector()) <= 0)) )
		return 1;
					 
	// see if destination is in list of proscribed paths
	for (INT j=0; j<4; j++ )
		if ( Nav->IsIdentifiedAs(ProscribedPaths[j]) )
			return 2;

	return 0;
	unguard;
}

UBOOL ANavigationPoint::IsIdentifiedAs(FName ActorName)
{
	guard(ANavigationPoint::IsIdentifiedAs);

	return ( ActorName == GetFName() );
	unguard;
}

UBOOL AInventorySpot::IsIdentifiedAs(FName ActorName)
{
	guard(AInventorySpot::IsIdentifiedAs);

	if ( ActorName == GetFName() )
		return true;
	if ( myPickupBase && (ActorName == myPickupBase->GetFName()) )
		return true;
	return ( markedItem && (ActorName == markedItem->GetFName()) ); 
	unguard;
}

UBOOL AAutoLadder::IsIdentifiedAs(FName ActorName)
{
	guard(AAutoLadder::IsIdentifiedAs);

	if ( ActorName == GetFName() )
		return true;
	return ( MyLadder && (ActorName == MyLadder->GetFName()) ); 
	unguard;
}

UBOOL ADoor::IsIdentifiedAs(FName ActorName)
{
	guard(ADoor::IsIdentifiedAs);

	if ( ActorName == GetFName() )
		return true;
	return ( MyDoor && (ActorName == MyDoor->GetFName()) ); 
	unguard;
}

UBOOL AAIMarker::IsIdentifiedAs(FName ActorName)
{
	guard(AAIMarker::IsIdentifiedAs);

	if ( ActorName == GetFName() )
		return true;
	return ( markedScript && (ActorName == markedScript->GetFName()) ); 
	unguard;
}

UBOOL AWarpZoneMarker::IsIdentifiedAs(FName ActorName)
{
	guard(AWarpZoneMarker::IsIdentifiedAs);

	if ( ActorName == GetFName() )
		return true;
	return ( markedWarpZone && (ActorName == markedWarpZone->GetFName()) ); 
	unguard;
}

INT ALadder::ProscribedPathTo(ANavigationPoint *Nav)
{
	guard(ALadder::ProscribedPathTo);

	ALadder *L = Cast<ALadder>(Nav);

	if ( L && (MyLadder == L->MyLadder) )
		return 1;

	return ANavigationPoint::ProscribedPathTo(Nav);
	unguard;
}

UBOOL ANavigationPoint::ShouldBeBased()
{
	return ( !PhysicsVolume->bWaterVolume && !bNotBased );
}

UBOOL AFlyingPathNode::ShouldBeBased()
{
	return false;
}

/* addReachSpecs()
Virtual function - adds reachspecs to path for every path reachable from it. 
*/
void ANavigationPoint::addReachSpecs(APawn * Scout, UBOOL bOnlyChanged)
{
	guard(ANavigationPoint::addReachspecs);

	bPathsChanged = bPathsChanged || !bOnlyChanged;
	UReachSpec *newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);

	// warn if no base
	if ( !Base && ShouldBeBased() && (GetClass()->ClassFlags & CLASS_Placeable) )
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("Navigation point not on valid base, or too close to steep slope"));

	// warn if bad base
	if ( Base && Base->bPathColliding )
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, *FString::Printf(TEXT("This type of NavigationPoint cannot be based on %s"), Base->GetName()) );

	// warn if imbedded in wall
	if ( Region.ZoneNumber == 0 )
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("Navigation point imbedded in level geometry"));

	for ( INT i=0; i<GetLevel()->Actors.Num(); i++)
	{
		ANavigationPoint *Nav = Cast<ANavigationPoint>(GetLevel()->Actors(i));
		if (Nav && !Nav->bDeleteMe && !Nav->bNoAutoConnect && !Nav->bSourceOnly && !Nav->bMakeSourceOnly
				&& (Nav != this) && (bPathsChanged || Nav->bPathsChanged) )
		{
			// check if paths are too close together
			if ( ((Nav->Location - Location).SizeSquared() < 2.f * HUMANRADIUS) && (Nav->GetClass()->ClassFlags & CLASS_Placeable) )
				GWarn->MapCheck_Add( MCTYPE_WARNING, this, TEXT("May be too close to other navigation points"));

			INT Proscribed = ProscribedPathTo(Nav);
			newSpec->Init();

			if ( Proscribed == 2 )
			{
				// no path allowed because LD marked it - mark it with a reachspec so LDs will know there is a proscribed path here
				newSpec->reachFlags = R_PROSCRIBED;
				newSpec->Start = this;
				newSpec->End = Nav;
				PathList.AddItem(newSpec);
				newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
			}
			else
			{
				INT bForced = 0;
				// check if forced path
				for (INT j=0; j<4; j++ )
				{
					if ( Nav->GetFName() == ForcedPaths[j] )
						bForced = 1;
				}

				if ( bForced )
				{
					// LD wants a path to Nav
					newSpec->Init();
					if ( Nav->bVehicleDestination )
					{
						newSpec->CollisionRadius = MAXVEHICLERADIUS;
						newSpec->CollisionHeight = MAXCOMMONHEIGHT;
					}
					else
					{
						newSpec->CollisionRadius = COMMONRADIUS;
						newSpec->CollisionHeight = COMMONHEIGHT;
					}
					newSpec->reachFlags = R_FORCED;
					newSpec->Start = this;
					newSpec->End = Nav;
					newSpec->Distance = (INT)((Location - Nav->Location).Size());
					newSpec->bForced = true;
					PathList.AddItem(newSpec);
					Nav->SetupForcedPath(Scout,newSpec);
					newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
				}
				else if ( !bForcedOnly && !Nav->bForcedOnly && !bDestinationOnly && !Proscribed && newSpec->defineFor(this, Nav, Scout) )
				{
					debugf(TEXT("***********added new spec from %s to %s"),GetName(),Nav->GetName());
					PathList.AddItem(newSpec);
					newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
				}
			}
		}
	}

	UReachSpec *tempSpec = NULL;
	// sort paths by path dist
	for ( INT i=0; i<PathList.Num(); i++ )
		for ( INT j=0; j<PathList.Num()-1; j++ )
			if ( PathList(j)->Distance > PathList(j+1)->Distance )
			{
				tempSpec = PathList(j+1);
				PathList(j+1) = PathList(j);
				PathList(j) = tempSpec;
			}


	unguard;
}

void AFlyingPathNode::InitForPathFinding()
{
	guard(AFlyingPathNode::InitForPathFinding);

	// calculate flightradius
	CollisionRadius = HALF_WORLD_MAX;
	CollisionHeight = HALF_WORLD_MAX;

	// look for floor
	FCheckResult Hit(1.f);
	GetLevel()->SingleLineCheck(Hit, this, Location - FVector(0.f,0.f,CollisionHeight), Location, TRACE_World);
	if ( Hit.Actor )
		CollisionHeight *= Hit.Time;
	GetLevel()->SingleLineCheck(Hit, this, Location + FVector(0.f,0.f,CollisionHeight), Location, TRACE_World);
	if ( Hit.Actor )
		CollisionHeight *= Hit.Time;
	FLOAT MaxHeight = CollisionHeight;

	CollisionRadius = CollisionHeight;
	GetLevel()->SingleLineCheck(Hit, this, Location - FVector(CollisionRadius,0.f,0.f), Location, TRACE_World);
	if ( Hit.Actor )
		CollisionRadius *= Hit.Time;
	GetLevel()->SingleLineCheck(Hit, this, Location + FVector(CollisionRadius,0.f,0.f), Location, TRACE_World);
	if ( Hit.Actor )
		CollisionRadius *= Hit.Time;

	GetLevel()->SingleLineCheck(Hit, this, Location - FVector(0.f,CollisionRadius,0.f), Location, TRACE_World);
	if ( Hit.Actor )
		CollisionRadius *= Hit.Time;
	GetLevel()->SingleLineCheck(Hit, this, Location + FVector(0.f,CollisionRadius,0.f), Location, TRACE_World);
	if ( Hit.Actor )
		CollisionRadius *= Hit.Time;

	// refine radius with non-zero extent trace (for static meshes)
	FVector Extent(CollisionRadius,CollisionRadius, CollisionRadius);
	FVector Unknown = 0.5f * Extent;
	while ( Unknown.X > 2.f )
	{
		if ( GetLevel()->EncroachingWorldGeometry( Hit,Location, Extent, 0, Level ) )
			Extent -= Unknown;
		else if ( Extent.X >= CollisionRadius )
			Unknown.X = 0.f;
		else
			Extent += Unknown;
		Unknown *= 0.5f;
	}
	Extent = Extent - Unknown - FVector(2.f,2.f,2.f);
	if ( Extent.X < 2.f )
	{
		CollisionRadius = 2.f;
		CollisionHeight = 2.f;
		return;
	}
	CollisionRadius = Extent.X;
	CollisionHeight = CollisionRadius;

	Extent = FVector(CollisionRadius,CollisionRadius,CollisionHeight+4.f);
	if ( !GetLevel()->EncroachingWorldGeometry( Hit,Location, Extent, 0, Level ) )
	{
		// try to increase height
		Extent.Z = MaxHeight;
		Unknown = 0.5f * Extent;
		Unknown.X = 0.f;
		Unknown.Y = 0.f;
		while ( Unknown.Z > 2.f )
		{
			if ( GetLevel()->EncroachingWorldGeometry( Hit,Location, Extent, 0, Level ) )
				Extent -= Unknown;
			else if ( Extent.Z >= MaxHeight )
				Unknown.Z = 0.f;
			else
				Extent += Unknown;
			Unknown *= 0.5f;
		}
		CollisionHeight = Extent.Z;
	}
	// try to increase radius
	Extent.Z = CollisionHeight;
	Extent.X = 4.f * CollisionRadius;
	Extent.Y = Extent.X;
	Unknown = 0.5f * Extent;
	Unknown.Z = 0.f;
	while ( Unknown.X > 2.f )
	{
		if ( GetLevel()->EncroachingWorldGeometry( Hit,Location, Extent, 0, Level ) )
			Extent -= Unknown;
		else if ( Extent.X >= 6.f * CollisionRadius )
			Unknown.X = 0.f;
		else
			Extent += Unknown;
		Unknown *= 0.5f;
	}
	CollisionRadius = Extent.X;
	unguard;
}

/* addReachSpecs()
Virtual function - adds reachspecs to path for every path reachable from it. 
*/
void AFlyingPathNode::addReachSpecs(APawn * Scout, UBOOL bOnlyChanged)
{
	guard(AFlyingPathNode::addReachspecs);

	bPathsChanged = bPathsChanged || !bOnlyChanged;
	UReachSpec *newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);

	// warn if imbedded in wall
	if ( Region.ZoneNumber == 0 )
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("Navigation point imbedded in level geometry"));

	// add paths to paths that are within FlightRadius, or intersecting flightradius (intersection radius defines path radius, or dist from edge of radius)
	// Note that none flying nodes need to have a path added from them as well
	for ( INT i=0; i<GetLevel()->Actors.Num(); i++)
	{
		ANavigationPoint *Nav = Cast<ANavigationPoint>(GetLevel()->Actors(i));
		if (Nav && !Nav->bDeleteMe && (!Nav->bNoAutoConnect || Cast<AFlyingPathNode>(Nav)) && !Nav->bSourceOnly && !Nav->bMakeSourceOnly
				&& (Nav != this) && (bPathsChanged || Nav->bPathsChanged) )
		{
			// check if paths are too close together
			if ( ((Nav->Location - Location).SizeSquared() < 2.f * HUMANRADIUS) && (Nav->GetClass()->ClassFlags & CLASS_Placeable) )
				GWarn->MapCheck_Add( MCTYPE_WARNING, this, TEXT("May be too close to other navigation points"));

			INT Proscribed = ProscribedPathTo(Nav);
			newSpec->Init();

			if ( Proscribed == 2 )
			{
				// no path allowed because LD marked it - mark it with a reachspec so LDs will know there is a proscribed path here
				newSpec->reachFlags = R_PROSCRIBED;
				newSpec->Start = this;
				newSpec->End = Nav;
				PathList.AddItem(newSpec);
				newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
			}
			else
			{
				INT bForced = 0;
				// check if forced path
				for (INT j=0; j<4; j++ )
				{
					if ( Nav->GetFName() == ForcedPaths[j] )
						bForced = 1;
				}

				if ( bForced )
				{
					// LD wants a path to Nav
					newSpec->Init();
					if ( Nav->bVehicleDestination )
					{
						newSpec->CollisionRadius = MAXVEHICLERADIUS;
						newSpec->CollisionHeight = MAXCOMMONHEIGHT;
					}
					else
					{
						newSpec->CollisionRadius = COMMONRADIUS;
						newSpec->CollisionHeight = COMMONHEIGHT;
					}
					newSpec->reachFlags = R_FORCED;
					newSpec->Start = this;
					newSpec->End = Nav;
					newSpec->Distance = (INT)((Location - Nav->Location).Size());
					newSpec->bForced = true;
					PathList.AddItem(newSpec);
					Nav->SetupForcedPath(Scout,newSpec);
					newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
				}
				else if ( !bDestinationOnly && !Proscribed )
				{
					AFlyingPathNode *FlyNav = Cast<AFlyingPathNode>(Nav);
					FLOAT SpecRadius = -1.f;
					FLOAT SpecHeight = -1.f;
					FLOAT Dist2D = (Nav->Location - Location).Size2D();
					if ( Dist2D < Nav->CollisionRadius + CollisionRadius )
					{
						// check if visible
						FCheckResult Hit(1.f);
						GetLevel()->SingleLineCheck( Hit, this, Nav->Location, Location, TRACE_World|TRACE_StopAtFirstHit );
						if ( !Hit.Actor || (Hit.Actor == Nav) )
						{
							if ( FlyNav )
							{
								SpecHeight = CollisionHeight + FlyNav->CollisionHeight - Abs(FlyNav->Location.Z - Location.Z);
								if ( SpecHeight >= CROUCHEDHUMANHEIGHT )
								{
									// reachspec radius based on intersection of circles
									FLOAT R1 = CollisionRadius;
									FLOAT R2 = FlyNav->CollisionRadius;
									FLOAT Part = 0.5f*Dist2D + (R1*R1 - R2*R2)/(2.f*Dist2D);
									SpecRadius = ::Max<FLOAT>(SpecRadius,appSqrt(R1*R1 - Part*Part));
									debugf(TEXT("Radius from %s to %s is %f"),GetName(),FlyNav->GetName(),SpecRadius);
								}
							}
							else if ( Dist2D < CollisionRadius )
							{
								FCheckResult Hit(1.f);
								// if nav inside my cylinder, definitely add
								if ( Abs(Nav->Location.Z - Location.Z) < CollisionHeight )
								{
									SpecRadius = 0.75f * CollisionRadius;
									SpecHeight = 0.75f * CollisionHeight;
								}
								else if ( Location.Z > Nav->Location.Z )
								{
									// otherwise, try extent trace to cylinder
									FVector Intersect = Nav->Location; 
									Intersect.Z = Location.Z - CollisionHeight + 2.f;
									GetLevel()->SingleLineCheck( Hit, this, Intersect, Nav->Location, TRACE_World|TRACE_StopAtFirstHit, FVector(MAXVEHICLERADIUS,MAXVEHICLERADIUS,1.f) );
									if ( !Hit.Actor || (Hit.Actor == Nav) )
									{
										SpecHeight = MAXCOMMONHEIGHT;
										SpecRadius = MAXVEHICLERADIUS;
									}
									else
									{
										GetLevel()->SingleLineCheck( Hit, this, Intersect, Nav->Location, TRACE_World|TRACE_StopAtFirstHit, FVector(MAXCOMMONRADIUS,MAXCOMMONRADIUS,1.f) );
										if ( !Hit.Actor || (Hit.Actor == Nav) )
										{
											SpecHeight = MAXCOMMONHEIGHT;
											SpecRadius = MAXCOMMONRADIUS;
										}
										else
										{
											GetLevel()->SingleLineCheck( Hit, this, Intersect, Nav->Location, TRACE_World|TRACE_StopAtFirstHit, FVector(COMMONRADIUS,COMMONRADIUS,1.f) );
											if ( !Hit.Actor || (Hit.Actor == Nav) )
											{
												SpecHeight = COMMONHEIGHT;
												SpecRadius = COMMONRADIUS;
											}
											else
											{
												GetLevel()->SingleLineCheck( Hit, this, Intersect, Nav->Location, TRACE_World|TRACE_StopAtFirstHit, FVector(MINCOMMONRADIUS,MINCOMMONRADIUS,1.f) );
												if ( !Hit.Actor || (Hit.Actor == Nav) )
												{
													SpecHeight = MINCOMMONHEIGHT;
													SpecRadius = MINCOMMONRADIUS;
												}
												else
												{
													GetLevel()->SingleLineCheck( Hit, this, Intersect, Nav->Location, TRACE_World|TRACE_StopAtFirstHit, FVector(HUMANRADIUS,HUMANRADIUS,1.f) );
													if ( !Hit.Actor || (Hit.Actor == Nav) )
													{
														SpecHeight = HUMANHEIGHT;
														SpecRadius = HUMANRADIUS;
													}
													else
													{
														GetLevel()->SingleLineCheck( Hit, this, Intersect, Nav->Location, TRACE_World|TRACE_StopAtFirstHit, FVector(HUMANRADIUS,HUMANRADIUS,1.f) );
														if ( !Hit.Actor || (Hit.Actor == Nav) )
														{
															SpecHeight = CROUCHEDHUMANHEIGHT;
															SpecRadius = HUMANRADIUS;
														}
													}
												}
											}
										}
									}
								}
							}
						}
					}
					if ( SpecRadius > 0.f )
					{
						// we found a good connection
						FLOAT RealDist = (Location - Nav->Location).Size();
						newSpec->Init();
						newSpec->CollisionRadius = SpecRadius;
						newSpec->CollisionHeight = SpecHeight;
						INT NewFlags = R_FLY;
						if ( Nav->PhysicsVolume->bWaterVolume && PhysicsVolume->bWaterVolume )
							NewFlags = R_SWIM;
						else if ( Nav->PhysicsVolume->bWaterVolume || PhysicsVolume->bWaterVolume )
							NewFlags = R_SWIM + R_FLY;
						newSpec->reachFlags = NewFlags;
						newSpec->Start = this;
						newSpec->End = Nav;
						newSpec->Distance = (INT)(RealDist);
						PathList.AddItem(newSpec);
						newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);

						if ( !FlyNav && !Nav->bDestinationOnly )
						{
							newSpec->Init();
							newSpec->CollisionRadius = SpecRadius;
							newSpec->CollisionHeight = SpecHeight;
							newSpec->reachFlags = NewFlags;
							newSpec->Start = Nav;
							newSpec->End = this;
							newSpec->Distance = (INT)(RealDist);
							Nav->PathList.AddItem(newSpec);
							newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
						}
						debugf(TEXT("***********added new spec from %s to %s size %f %f"),GetName(),Nav->GetName(), SpecRadius, SpecHeight);
					}
				}
			}
		}
	}
	unguard;
}
void ALiftCenter::addReachSpecs(APawn * Scout, UBOOL bOnlyChanged)
{
	guard(ALiftCenter::addReachspecs);

	bPathsChanged = bPathsChanged || !bOnlyChanged;
	// find associated mover
	FindBase();
	MyLift = Cast<AMover>(Base);
	// Warn if there is no lift
	if ( !MyLift )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, TEXT("No Lift found for this LiftCenter"));

	if ( MyLift && (LiftTag != MyLift->Tag) )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, TEXT("LiftTag does not match the lift this LiftCenter is on"));
	// Warn if lift is bump open timed (the default initial state for AMover)
	if ( MyLift && (MyLift->InitialState == AMover::StaticClass()->GetDefaultActor()->InitialState) )
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("Mover associated with a LiftCenter must not be BumpOpenTimed"));

	UReachSpec *newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
	//debugf("Add Reachspecs for LiftCenter at (%f, %f, %f)", Location.X,Location.Y,Location.Z);
	INT NumExits = 0;

	for ( INT i=0; i<GetLevel()->Actors.Num(); i++ )
	{
		ALiftExit *Nav = Cast<ALiftExit>(GetLevel()->Actors(i)); 
		if ( Nav && (Nav->LiftTag == LiftTag) && (bPathsChanged || Nav->bPathsChanged) ) 
		{
			NumExits++;

			// add reachspec from LiftCenter to LiftExit
			Nav->MyLift = MyLift;
			newSpec->Init();
			newSpec->CollisionRadius = COMMONRADIUS;
			newSpec->CollisionHeight = COMMONHEIGHT;
			newSpec->reachFlags = R_SPECIAL;
			newSpec->Start = this;
			newSpec->End = Nav;
			newSpec->Distance = 500;
			PathList.AddItem(newSpec);
			newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);

			// add reachspec from LiftExit to LiftCenter
			newSpec->Init();
			newSpec->CollisionRadius = COMMONRADIUS;
			newSpec->CollisionHeight = COMMONHEIGHT;
			newSpec->reachFlags = R_SPECIAL;
			newSpec->Start = Nav;
			newSpec->End = this;
			newSpec->Distance = 500;
			Nav->PathList.AddItem(newSpec);
			newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);

			if ( !Nav->bLiftJumpExit )
			{
				// calculate the mover keyframe to associate with this liftexit
				// unless level designer has already specified it
				if ( Nav->SuggestedKeyFrame == 255 )
				{
					if ( MyLift )
					{
						INT Best = -1;
						FLOAT BestDist = 1000000.f;
						FCheckResult Hit(1.f);

						for ( INT j=0; j<MyLift->NumKeys; j++ )
						{
							FVector Pos = MyLift->Location + MyLift->KeyPos[j] + LiftOffset;
							if ( Abs(Pos.Z - Nav->Location.Z) < UCONST_MAXSTEPHEIGHT )
							{
								FLOAT Dist2D = (Pos - Nav->Location).Size2D();
								if ( Dist2D < BestDist )
								{
									GetLevel()->SingleLineCheck( Hit, Nav, Pos, Nav->Location, TRACE_World, FVector(CollisionRadius,CollisionRadius, 1.f) );
									if ( Hit.Time == 1.f )
									{
										Best = j;
										BestDist = Dist2D;
									}
								}
							}
						}
						if ( Best >= 0 )
							Nav->KeyFrame = Best;
						else
							GWarn->MapCheck_Add( MCTYPE_WARNING, Nav, TEXT("No suitable keyframe found for this LiftExit"));
					}
				}
				else
					Nav->KeyFrame = Nav->SuggestedKeyFrame;
			}
		}
	}
	
	// Warn if no lift exits
	if ( NumExits == 0 )
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("No LiftExits for this LiftCenter"));
	unguard;
}

void ALadder::addReachSpecs(APawn * Scout, UBOOL bOnlyChanged)
{
	guard(ALadder::addReachspecs);

	UReachSpec *newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);

	//debugf("Add Reachspecs for Ladder at (%f, %f, %f)", Location.X,Location.Y,Location.Z);
	bPathsChanged = bPathsChanged || !bOnlyChanged;

	// connect to all ladders in same LadderVolume
	if ( MyLadder )
	{
		for (INT i=0; i<GetLevel()->Actors.Num(); i++)
		{
			ALadder *Nav = Cast<ALadder>(GetLevel()->Actors(i));
			if ( Nav && (Nav != this) && (Nav->MyLadder == MyLadder) && (bPathsChanged || Nav->bPathsChanged) )
			{
				newSpec->Init();
				// add reachspec from this to other Ladder
				// FIXME - support single direction ladders (e.g. zipline)
				newSpec->CollisionRadius = COMMONRADIUS;
				newSpec->CollisionHeight = COMMONHEIGHT;
				newSpec->reachFlags = R_LADDER;
				newSpec->Start = this;
				newSpec->End = Nav;
				newSpec->Distance = (INT)(Location - Nav->Location).Size();
				PathList.AddItem(newSpec);
				newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
			}
		}
	}
	ANavigationPoint::addReachSpecs(Scout,bOnlyChanged);

	// Prune paths that require jumping
	for ( INT i=0; i<PathList.Num(); i++ )
		if ( PathList(i) && (PathList(i)->reachFlags & R_JUMP) 
			&& (PathList(i)->End->Location.Z < PathList(i)->Start->Location.Z - PathList(i)->Start->CollisionHeight) )
			PathList(i)->bPruned = true;

	unguard;
}

void ATeleporter::addReachSpecs(APawn * Scout, UBOOL bOnlyChanged)
{
	guard(ATeleporter::addReachspecs);

	UReachSpec *newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
	//debugf("Add Reachspecs for node at (%f, %f, %f)", Location.X,Location.Y,Location.Z);
	bPathsChanged = bPathsChanged || !bOnlyChanged;

	for (INT i=0; i<GetLevel()->Actors.Num(); i++)
	{
		ATeleporter *Nav = Cast<ATeleporter>(GetLevel()->Actors(i)); 
		if ( Nav && (Nav != this) && (Nav->Tag != NAME_None) && (URL==*Nav->Tag) && (bPathsChanged || Nav->bPathsChanged) )
		{
			newSpec->Init();
			newSpec->CollisionRadius = MAXCOMMONRADIUS;
			newSpec->CollisionHeight = MAXCOMMONHEIGHT;
			newSpec->reachFlags = R_SPECIAL | R_FORCED;
			newSpec->bForced = true;
			newSpec->Start = this;
			newSpec->End = Nav;
			newSpec->Distance = 100;
			PathList.AddItem(newSpec);
			newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
			break;
		}
	}

	ANavigationPoint::addReachSpecs(Scout, bOnlyChanged);
	unguard;
}

void APlayerStart::addReachSpecs(APawn * Scout, UBOOL bOnlyChanged)
{
	guard(APlayerStart::addReachspecs);
	ANavigationPoint::addReachSpecs(Scout, bOnlyChanged);

	// check that playerstart is useable
	Scout->SetCollisionSize( HUMANRADIUS, HUMANHEIGHT );
	if ( !GetLevel()->FarMoveActor(Scout,Location,1) )
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("PlayerStart is not useable"));

	unguard;
}

/* InitForPathFinding()
find door movers associated with ADoor actor, so the collision for these movers can be temporarily turned off
when check paths with ADoor as an end point
*/
void ADoor::InitForPathFinding()
{
	guard(ADoor::InitForPathFinding());

	// find associated mover
	if ( DoorTag != NAME_None )
	{
		MyDoor = NULL;
		for (INT i=0; i<GetLevel()->Actors.Num(); i++)
		{
			AMover * NewDoor = Cast<AMover>(GetLevel()->Actors(i));
			if ( NewDoor && ((NewDoor->Tag == DoorTag) 
					|| (MyDoor && (MyDoor->ReturnGroup != NAME_None) && (NewDoor->ReturnGroup == MyDoor->ReturnGroup))) )
			{
				if ( MyDoor && ((MyDoor->ReturnGroup == NAME_None) || (NewDoor->ReturnGroup != MyDoor->ReturnGroup)) )
				{
					GWarn->MapCheck_Add( MCTYPE_ERROR, MyDoor, TEXT("Door has same tag as another door"));
					GWarn->MapCheck_Add( MCTYPE_ERROR, NewDoor, TEXT("Door has same tag as another door"));
				}

				NewDoor->myMarker = this;
				if ( MyDoor )
				{
					NewDoor->Leader = MyDoor;
					NewDoor->Follower = MyDoor->Follower;
					MyDoor->Follower = NewDoor;
				}
				else
				{
					MyDoor = NewDoor;
					MyDoor->Leader = MyDoor;
					MyDoor->Follower = NULL;
				}
			}
		}
		if ( !MyDoor )
		{
			// Warn if there is no door
			GWarn->MapCheck_Add( MCTYPE_WARNING, this, TEXT("No Mover found for this Door"));
		}
	}
	unguard;
}

void ALadder::InitForPathFinding()
{
	guard(ALadder::InitForPathFinding());

	// find associated LadderVolume
	MyLadder = NULL;
	for( INT i=0; i<GetLevel()->Actors.Num(); i++ )
	{
		ALadderVolume *V = Cast<ALadderVolume>(GetLevel()->Actors(i));
		if ( V && (V->Encompasses(Location) || V->Encompasses(Location - FVector(0.f, 0.f, CollisionHeight))) )
		{
			MyLadder = V;
			break;
		}
	}
	if ( !MyLadder )
	{
		// Warn if there is no door
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, TEXT("Ladder is not in a LadderVolume"));
		return;
	}

	LadderList = MyLadder->LadderList;
	MyLadder->LadderList = this;
	unguard;
}


void ANavigationPoint::PrePath()
{
	guard(ANavigationPoint::PrePath);

	if ( bPathColliding && bBlockActors && bCollideActors )
		SetCollision(0, bBlockActors);
	unguard;
}

void ANavigationPoint::PostPath()
{
	guard(ANavigationPoint::PostPath);

	if ( bPathColliding && bBlockActors && GetClass()->GetDefaultActor()->bCollideActors )
		SetCollision(1, bBlockActors);
	unguard;
}

/* PrePath()
Turn off ADoor associated mover collision temporarily
*/
void ADoor::PrePath()
{
	guard(ADoor::PrePath);

	for ( AMover *D=MyDoor; D!=NULL; D=D->Follower )
		if ( D->bBlockActors && D->bCollideActors )
	{
			D->SetCollision(0, D->bBlockActors);
		bTempNoCollide = 1;
	}
	unguard;
}

/* PostPath()
Turn ADoor associated mover collision back on
*/
void ADoor::PostPath()
{
	guard(ADoor::PostPath);

	if ( bTempNoCollide )
		for ( AMover *D=MyDoor; D!=NULL; D=D->Follower )
			D->SetCollision(1, D->bBlockActors);

	unguard;
}

void ADoor::PostaddReachSpecs(APawn * Scout)
{
	guard(ADoor::addReachspecs);

	// add R_DOOR requirement to all reachspecs going through this node
	for (INT i=0; i<PathList.Num(); i++ )
		PathList(i)->reachFlags = PathList(i)->reachFlags | R_DOOR;

	for ( ANavigationPoint* Nav=Level->NavigationPointList; Nav!=NULL; Nav=Nav->nextNavigationPoint )
		for (INT i=0; i<Nav->PathList.Num(); i++ )
			if ( Nav->PathList(i)->End == this )
				Nav->PathList(i)->reachFlags = Nav->PathList(i)->reachFlags | R_DOOR;
	unguard;
}

void AWarpZoneMarker::addReachSpecs(APawn * Scout, UBOOL bOnlyChanged)
{
	guard(AWarpZoneMarker::addReachspecs);

	UReachSpec *newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
	//debugf("Add Reachspecs for node at (%f, %f, %f)", Location.X,Location.Y,Location.Z);

	for (INT i=0; i<GetLevel()->Actors.Num(); i++)
	{
		AWarpZoneMarker *Nav = Cast<AWarpZoneMarker>(GetLevel()->Actors(i)); 
		if ( Nav && (Nav != this) && (markedWarpZone->OtherSideURL==*Nav->markedWarpZone->ThisTag)
			 && (bPathsChanged || Nav->bPathsChanged) )
		{
			newSpec->Init();
			newSpec->CollisionRadius = MAXCOMMONRADIUS;
			newSpec->CollisionHeight = MAXCOMMONHEIGHT;
			newSpec->reachFlags = R_SPECIAL;
			newSpec->Start = this;
			newSpec->End = Nav;
			newSpec->Distance = 100;
			PathList.AddItem(newSpec);
			newSpec = ConstructObject<UReachSpec>(UReachSpec::StaticClass(),GetLevel()->GetOuter(),NAME_None,RF_Public);
			break;
		}
	}

	ANavigationPoint::addReachSpecs(Scout, bOnlyChanged);
	unguard;
}

void AJumpDest::SetupForcedPath(APawn* Scout, UReachSpec* Path)
{
	guard(AJumpDest::SetupForcedPath);
	
	if ( NumUpstreamPaths > 7 )
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("More than 8 paths to this JumpDest!"));
		return;
	}
	// calculate NeededJumpZ
	NeededJump[NumUpstreamPaths] = FVector(0.f,0.f,1.1f*TESTJUMPZ);
	Scout->JumpZ = 10000.f;
	Scout->GroundSpeed = TESTGROUNDSPEED;
	Scout->SetCollisionSize(HUMANRADIUS,HUMANHEIGHT);
	if ( GetLevel()->FarMoveActor(Scout, Path->Start->Location) )
	{
		CalculatedGravityZ[NumUpstreamPaths] = Scout->PhysicsVolume->Gravity.Z;
		NeededJump[NumUpstreamPaths] = Scout->SuggestJumpVelocity(Location,TESTGROUNDSPEED,0.f);
	}
	Scout->JumpZ = TESTJUMPZ;
	UpstreamPaths[NumUpstreamPaths] = Path;
	NumUpstreamPaths++;
	if ( Path->Start->Location.Z > Location.Z )
	{
		// set the path's MaxLandingVelocity
		Path->MaxLandingVelocity = -1 * PhysicsVolume->Gravity.Z * appSqrt(2.f * (Location.Z - Path->Start->Location.Z)/PhysicsVolume->Gravity.Z);
	}
	unguard;
}

void AJumpPad::addReachSpecs(APawn * Scout, UBOOL bOnlyChanged)
{
	guard(AJumpPad::addReachspecs);

	ANavigationPoint::addReachSpecs(Scout,bOnlyChanged);
	Scout->JumpZ = 10000.f;
	Scout->SetCollisionSize(HUMANRADIUS,HUMANHEIGHT);
	if ( (PathList.Num() > 0) && GetLevel()->FarMoveActor(Scout, Location) )
	{
		JumpTarget = PathList(0)->End;
		FVector XYDir = JumpTarget->Location - Location;
		FLOAT ZDiff = XYDir.Z;
		FLOAT Time = 2.5f * JumpZModifier * appSqrt(Abs(ZDiff/PhysicsVolume->Gravity.Z));
		if ( (Time == 0.f) || (Time != Time) )
		{
			Time = XYDir.Size()/800.f;
			if ( Time == 0.f )
				Time = 1.f;
		}
		JumpVelocity = XYDir/Time; 
		JumpVelocity.Z = ZDiff/Time - 0.5f * PhysicsVolume->Gravity.Z * Time;
	}
	else 
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("No forced destination for this jumppad!"));
		JumpVelocity = FVector(0.f,0.f,3.f*TESTJUMPZ);
	}
	Scout->JumpZ = TESTJUMPZ;
	unguard;
}

/* ClearForPathFinding()
clear transient path finding properties right before a navigation network search
*/
void ANavigationPoint::ClearForPathFinding()
{
	guard(ANavigationPoint::ClearForPathFinding);

	visitedWeight = 10000000;
	nextOrdered = NULL;
	prevOrdered = NULL;
	previousPath = NULL;
	bEndPoint = bTransientEndPoint;
	bTransientEndPoint = 0;
	bAlreadyVisited = 0;
	cost = ExtraCost + TransientCost + FearCost;
	TransientCost = 0;
	unguard;
}

/* ClearPaths()
remove all path information from a navigation point. (typically before generating a new version of this
information
*/
void ANavigationPoint::ClearPaths()
{
	guard(ANavigationPoint::ClearPaths);

	nextNavigationPoint = NULL;
	nextOrdered = NULL;
	prevOrdered = NULL;
	previousPath = NULL;
	PathList.Empty();
	unguard;
}

void AJumpDest::ClearPaths()
{
	guard(ANavigationPoint::ClearPaths);

	Super::ClearPaths();
	NumUpstreamPaths = 0;
	unguard;
}
void ALadder::ClearPaths()
{
	guard(ALadder::ClearPaths);

	Super::ClearPaths();

	if ( MyLadder )
		MyLadder->LadderList = NULL;
	LadderList = NULL;
	MyLadder = NULL;
	unguard;
}

void ALiftCenter::FindBase()
{
	guard(ALiftCenter::FindBase);

	if ( !GIsEditor )
		return;

	// find associated mover
	if ( LiftTag != NAME_None )
	{
		MyLift =  NULL;
		for (INT i=0; i<GetLevel()->Actors.Num(); i++)
		{
			if ( GetLevel()->Actors(i) && (GetLevel()->Actors(i)->Tag == LiftTag) )
			{
				AMover* FoundLift = Cast<AMover>(GetLevel()->Actors(i));
				if ( FoundLift )
				{
					if ( MyLift )
					{
						GWarn->MapCheck_Add( MCTYPE_ERROR, MyLift, TEXT("Lift has same tag as another lift"));
						GWarn->MapCheck_Add( MCTYPE_ERROR, FoundLift, TEXT("Lift has same tag as another lift"));
						return;
					}
					MyLift = FoundLift;
					MyLift->myMarker = this;
					SetBase(MyLift);
					LiftOffset = Location - MyLift->Location;
				}
			}
		}
	}
	unguard;
}

void ANavigationPoint::FindBase()
{
	guard(ANavigationPoint::FindBase);

	if ( !GIsEditor )
		return;

	SetZone(1,1);
	if (  ShouldBeBased() && (Region.ZoneNumber != 0) )
	{
		// not using find base, because don't want to fail if LD has navigationpoint slightly interpenetrating floor
		FCheckResult Hit(1.f);
		FVector CollisionSlice = FVector(HUMANRADIUS, HUMANRADIUS, 1.f);
		if ( CollisionRadius < HUMANRADIUS )
			CollisionSlice = FVector(CollisionRadius, CollisionRadius, 1.f);
		GetLevel()->SingleLineCheck( Hit, this, Location - FVector(0,0, 2.f * CollisionHeight), Location, TRACE_AllBlocking, CollisionSlice );
		if ( Hit.Actor )
		{
			if ( Hit.Normal.Z > UCONST_MINFLOORZ )
				GetLevel()->FarMoveActor(this, Hit.Location + FVector(0.f,0.f,CollisionHeight-1.f),0,1,0);
			else 
				Hit.Actor = NULL;
		}
		SetBase(Hit.Actor, Hit.Normal);
	}
	unguard;
}

void ADoor::FindBase()
{
	guard(ADoor::FindBase);
	if ( !GIsEditor )
		return;

	PrePath();
	ANavigationPoint::FindBase();
	PostPath();
	unguard;
}

/* Prune paths when an acceptable route using an intermediate path exists
*/
INT ANavigationPoint::PrunePaths()
{
	guard(ANavigationPoint::PrunePaths);

	INT pruned = 0;
	debugf(TEXT("Prune paths from %s"),GetName());

	for ( INT i=0; i<PathList.Num(); i++ )
	{
		for ( INT j=0; j<PathList.Num(); j++ )
			if ( (i!=j) && !PathList(j)->bPruned && (*PathList(j) <= *PathList(i)) 
				&& PathList(j)->End->FindAlternatePath(PathList(i), PathList(j)->Distance) )
			{
				PathList(i)->bPruned = true;
				//debugf(TEXT("Pruned path to %s because of path through %s"),PathList(i)->End->GetName(),PathList(j)->End->GetName());
				j = PathList.Num();
				pruned++;
			}
	}

	CleanUpPruned();

	return pruned;
	unguard;
}

UBOOL ANavigationPoint::FindAlternatePath(UReachSpec* StraightPath, INT AccumulatedDistance)
{
	guard(ANavigationPoint::FindAlternatePath);

	if ( bBlocked || bBlockable )
		return false;

	FVector StraightDir = StraightPath->End->Location - StraightPath->Start->Location;

	// check if the endpoint is directly reachable
	INT i;
	for ( i=0; i<PathList.Num(); i++ )
		if ( !PathList(i)->bPruned && (PathList(i)->End == StraightPath->End) 
				&& ((StraightDir | (StraightPath->End->Location - Location)) >= 0.f) )
		{
			if ( (AccumulatedDistance + PathList(i)->Distance + PathList(i)->End->ExtraCost < PATHPRUNING * StraightPath->Distance)
					&& (*PathList(i) <= *StraightPath) )
			{
				//debugf(TEXT("Direct path from %s to %s"),GetName(), PathList(i)->End->GetName());
				return true;
			}
			else 
				return false;
		}

	// now continue looking for path
	for ( INT i=0; i<PathList.Num(); i++ )
		if ( !PathList(i)->bPruned 
			&& (PathList(i)->Distance > 0.f)
			&& (AccumulatedDistance + PathList(i)->Distance < PATHPRUNING * StraightPath->Distance)
			&& (*PathList(i) <= *StraightPath)
			&& (PathList(i)->End != StraightPath->Start)
			&& ((StraightDir | (PathList(i)->End->Location - Location)) > 0.f)
			&& PathList(i)->End->FindAlternatePath(StraightPath, AccumulatedDistance + PathList(i)->Distance) )
		{
			//debugf(TEXT("Partial path from %s to %s"),GetName(), PathList(i)->End->GetName());
			return true;
		}

	return false;
	unguard;
}

UBOOL AFlyingPathNode::ReachedBy(APawn *P, FVector Loc) 
{ 
	guard(AFlyingPathNode::ReachedBy);

	if ( !P->bCanFly && !PhysicsVolume->bWaterVolume )
		return false;
	FVector Dir = Loc - Location;
	if ( Abs(Dir.Z) > CollisionHeight )
		return false;
	Dir.Z = 0.f;
	return ( Dir.SizeSquared() < CollisionRadius * CollisionRadius );
	unguard;
}

UBOOL AFlyingPathNode::NoReachDistance() 
{ 
	return !GIsEditor; 
}

UBOOL AFlyingPathNode::BigAnchor(APawn *P, FVector Loc)
{
	guard(AFlyingPathNode::BigAnchor);

	return ReachedBy(P,Loc);
	unguard;
}

UBOOL ALadder::ReachedBy(APawn *P, FVector Loc) 
{ 
	guard(ALadder::ReachedBy);

	// look at difference along ladder direction
	return ( P->OnLadder && (Abs((Loc - Location) | P->OnLadder->ClimbDir) < P->CollisionHeight) );
	unguard;
}

UBOOL ALadder::NoReachDistance() { return true; }

UBOOL ATriggers::ReachedBy(APawn *P, FVector Loc) 
{ 
	guard(ATriggers::ReachedBy);

	if ( bCollideActors && !GIsEditor )
		return true; // already checked touching in APawn::ReachedDestination()
	return false;
	unguard;
}

UBOOL ATeleporter::ReachedBy(APawn *P, FVector Loc) 
{ 
	guard(ATeleporter::ReachedBy);

	if ( bCollideActors && !GIsEditor )
		return true; // already checked touching in APawn::ReachedDestination()
	return false;
	unguard;
}

UBOOL ATeleporter::NoReachDistance() 
{ 
	guard(ATeleporter::NoReachDistance);

	return ( bCollideActors && !GIsEditor );
	unguard;
}

UBOOL AHoverPathNode::NotReachableBy(APawn *P)
{
	guard(AHoverPathNode::NotReachableBy);

	if ( P->bCanFly )
		return false;

	AVehicle *V = Cast<AVehicle>(P);
	return ( !V || !V->bCanHover );

	unguard;
}



