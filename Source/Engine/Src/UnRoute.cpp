/*=============================================================================
	UnRoute.cpp: Unreal AI routing code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* ...
=============================================================================*/

#include "EnginePrivate.h"
#include "UnPath.h"

ANavigationPoint* FSortedPathList::findStartAnchor(APawn *Searcher) 
{
	guard(FSortedPathList::findStartAnchor);

	// see which nodes are visible and reachable
	FCheckResult Hit(1.f);
	for ( INT i=0; i<numPoints; i++ )
	{
		Searcher->GetLevel()->SingleLineCheck( Hit, Searcher, Path[i]->Location, Searcher->Location, TRACE_World|TRACE_StopAtFirstHit );
		if ( Hit.Actor )
			Searcher->GetLevel()->SingleLineCheck( Hit, Searcher, Path[i]->Location + FVector(0.f,0.f, Path[i]->CollisionHeight), Searcher->Location + FVector(0.f,0.f, Searcher->CollisionHeight), TRACE_World|TRACE_StopAtFirstHit );
		if ( !Hit.Actor && Searcher->actorReachable(Path[i], 1, 0) )
			return Path[i];
	}
	return NULL;
	unguard;
}

ANavigationPoint* FSortedPathList::findEndAnchor(APawn *Searcher, AActor *GoalActor, FVector EndLocation, UBOOL bAnyVisible, UBOOL bOnlyCheckVisible ) 
{
	guard(FSortedPathList::findEndAnchor);

	if ( bOnlyCheckVisible && !bAnyVisible )
		return NULL;

	ANavigationPoint* NearestVisible = NULL;
	ULevel* MyLevel = Searcher->GetLevel();
	FVector RealLoc = Searcher->Location;

	// now see from which nodes EndLocation is visible and reachable
	FCheckResult Hit(1.f);
	for ( INT i=0; i<numPoints; i++ )
	{
		MyLevel->SingleLineCheck( Hit, Searcher, EndLocation, Path[i]->Location, TRACE_World|TRACE_StopAtFirstHit );
		if ( Hit.Actor )
		{
			if ( GoalActor )
				MyLevel->SingleLineCheck( Hit, Searcher, EndLocation + FVector(0.f,0.f,GoalActor->CollisionHeight), Path[i]->Location  + FVector(0.f,0.f, Path[i]->CollisionHeight), TRACE_World|TRACE_StopAtFirstHit );
			else
				MyLevel->SingleLineCheck( Hit, Searcher, EndLocation, Path[i]->Location + FVector(0.f,0.f, Path[i]->CollisionHeight), TRACE_World|TRACE_StopAtFirstHit );
		}
		if ( !Hit.Actor )
		{
			if ( bOnlyCheckVisible )
				return Path[i];
			FVector AdjustedDest = Path[i]->Location;
			AdjustedDest.Z = AdjustedDest.Z + Searcher->CollisionHeight - Path[i]->CollisionHeight;
			if ( MyLevel->FarMoveActor(Searcher,AdjustedDest,1,1) )
			{
				if ( GoalActor ? Searcher->actorReachable(GoalActor,1,0) : Searcher->pointReachable(EndLocation, 1) )
				{
					MyLevel->FarMoveActor(Searcher, RealLoc, 1, 1);
					return Path[i];
				}
				else if ( bAnyVisible && !NearestVisible )
					NearestVisible = Path[i];
			}
		}
	}

	if ( Searcher->Location != RealLoc )
		MyLevel->FarMoveActor(Searcher, RealLoc, 1, 1);

	return NearestVisible;
	unguard;
}

UBOOL APawn::ValidAnchor()
{
	guard(APawn::ValidAnchor);
	if ( Anchor && !Anchor->bBlocked 
		&& (bCanCrouch ? (Anchor->MaxPathSize.X >= CrouchRadius) && (Anchor->MaxPathSize.Z >= CrouchHeight)
						: (Anchor->MaxPathSize.X >= CollisionRadius) && (Anchor->MaxPathSize.Z >= CollisionHeight))
		&& ReachedDestination(Anchor->Location-Location,Anchor) )
	{
		LastValidAnchorTime = Level->TimeSeconds;
		LastAnchor = Anchor;
		return true;
	}
	return false;
	unguard;
}

typedef FLOAT ( *NodeEvaluator ) (ANavigationPoint*, APawn*, FLOAT);

static FLOAT FindEndPoint( ANavigationPoint* CurrentNode, APawn* seeker, FLOAT bestWeight )
{
	if ( CurrentNode->bEndPoint )
	{
//		debugf(TEXT("Found endpoint %s"),CurrentNode->GetName());
		return 2.f;
	}
	else
		return 0.f;
}

FLOAT APawn::findPathToward(AActor *goal, FVector GoalLocation, NodeEvaluator NodeEval, FLOAT BestWeight, UBOOL bWeightDetours)
{
	guard(APawn::findPathToward);

	NextPathRadius = 0.f;
	if ( !Level->NavigationPointList || (FindAnchorFailedTime == Level->TimeSeconds) || !Controller )
		return 0.f;

	//if ( goal )
	//	debugf(TEXT("%s Findpathtoward %s"),GetName(), goal->GetName());
	//else debugf(TEXT("%s Findpathtoward point"),GetName());
	int bSpecifiedEnd = (NodeEval == NULL);
	UBOOL bOnlyCheckVisible = (Physics == PHYS_Karma);
	FVector RealLocation = Location;
	ANavigationPoint * EndAnchor = Cast<ANavigationPoint>(goal);
	FLOAT EndDist=0, StartDist=0;
	if ( goal )
		GoalLocation = goal->Location;

	// find EndAnchor (destination path on navigation network)
	if ( goal && !EndAnchor )
	{
		APawn* PawnGoal = goal->GetAPawn();
		if ( PawnGoal )
		{
			if ( PawnGoal->ValidAnchor() )
			{
				EndAnchor = PawnGoal->Anchor;
				EndDist = (EndAnchor->Location - GoalLocation).Size();
			}
			else
			{
				AAIController *AI = Cast<AAIController>(PawnGoal->Controller);
				if ( AI && (AI->GetStateFrame()->LatentAction == UCONST_LATENT_MOVETOWARD) )  
					EndAnchor = Cast<ANavigationPoint>(AI->MoveTarget);
			}
			if ( !EndAnchor && PawnGoal->LastAnchor && (Anchor != PawnGoal->LastAnchor) && (Level->TimeSeconds - PawnGoal->LastValidAnchorTime < 0.25f) 
				&& PawnGoal->Controller && PawnGoal->Controller->LineOfSightTo(PawnGoal->LastAnchor) )
			{
				EndAnchor = PawnGoal->LastAnchor;
				EndDist = (EndAnchor->Location - GoalLocation).Size();
			}

			if ( !EndAnchor && (PawnGoal->Physics == PHYS_Falling) )
			{
				if ( PawnGoal->LastAnchor && (Anchor != PawnGoal->LastAnchor) && (Level->TimeSeconds - PawnGoal->LastValidAnchorTime < 1.f) 
					 && PawnGoal->Controller && PawnGoal->Controller->LineOfSightTo(PawnGoal->LastAnchor) )
				{
					EndAnchor = PawnGoal->LastAnchor;
					EndDist = (EndAnchor->Location - GoalLocation).Size();
				}
				else
					bOnlyCheckVisible = true;
			}
			if ( !EndAnchor )
			{
				APlayerController *PC = Cast<APlayerController>(PawnGoal->Controller);
				if ( PC && (PawnGoal->Location == PC->FailedPathStart) )
					bOnlyCheckVisible = true;
			}
		}
		else
		{
			ADecoration *Dec = Cast<ADecoration>(goal); // game flags are decorations
			if ( Dec )
			{
				if ( Dec->LastAnchor && (Level->TimeSeconds - Dec->LastValidAnchorTime < 0.25f) )
				{
					EndAnchor = Dec->LastAnchor;
					EndDist = (EndAnchor->Location - GoalLocation).Size();
				}
				else if ( (Dec->Physics == PHYS_Falling) || (Dec->Physics == PHYS_Projectile) )
					bOnlyCheckVisible = true;
			}
			else if ( goal->Physics == PHYS_Falling )
				bOnlyCheckVisible = true;
		}
	}

	// check if my anchor is still valid
	if ( !ValidAnchor() )
		Anchor = NULL;
	
	if ( !Anchor || (!EndAnchor && bSpecifiedEnd) )
	{
		//find anchors from among nearby paths
		FCheckResult Hit(1.f);
		FSortedPathList StartPoints, DestPoints;
		FLOAT dist;
		for ( ANavigationPoint *Nav=Level->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
		{
			Nav->ClearForPathFinding();
			if ( Nav->bFlyingPreferred )
			{
				if ( !bCanFly )
					continue;
			}
			else if ( bCanFly )
				Nav->TransientCost += 4000.f;

			if ( !Nav->bBlocked )
			{
				if ( !Anchor )
				{
					if ( Nav->BigAnchor(this,Location) )
						Anchor = Nav;
					else 
					{
						dist = (Location - Nav->Location).SizeSquared();
						if ( (dist < MAXPATHDISTSQ) 
		                    && (bCanCrouch ? (Nav->MaxPathSize.X >= CrouchRadius) && (Nav->MaxPathSize.Z >= CrouchHeight)
						                    : (Nav->MaxPathSize.X >= CollisionRadius) && (Nav->MaxPathSize.Z >= CollisionHeight)) )
							StartPoints.addPath(Nav, appRound(dist));
					}
				}
				if ( !EndAnchor && bSpecifiedEnd )
				{
					if ( Nav->BigAnchor(this,GoalLocation) )
						EndAnchor = Nav;
					else
					{
						dist = (GoalLocation - Nav->Location).SizeSquared();
						if ( (dist < MAXPATHDISTSQ) 
		                    && (bCanCrouch ? (Nav->MaxPathSize.X >= CrouchRadius) && (Nav->MaxPathSize.Z >= CrouchHeight)
						                    : (Nav->MaxPathSize.X >= CollisionRadius) && (Nav->MaxPathSize.Z >= CollisionHeight)) )
							DestPoints.addPath(Nav, appRound(dist));
					}
				}
			}
		}

		//debugf(TEXT("Startpoints = %d, DestPoints = %d"), StartPoints.numPoints, DestPoints.numPoints);
		if ( !Anchor )
		{
			if ( StartPoints.numPoints > 0 )
				Anchor = StartPoints.findStartAnchor(this);
			if ( !Anchor )
			{
				// look for road
				FSortedPathList RoadPoints;
				for ( ANavigationPoint *Nav=Level->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
				{
					if ( !Nav->bBlocked )
					{
						ARoadPathNode *RNav = Cast<ARoadPathNode>(Nav);
						if ( RNav )
						{
							FLOAT dist = (Location - RNav->Location).SizeSquared();
							if  ( dist > 0.5f * MAXPATHDISTSQ )
								RoadPoints.addPath(RNav, appRound(dist));
						}
					}
				}
				if ( RoadPoints.numPoints > 0 )
				{
					for ( INT i=0; i<RoadPoints.numPoints; i++ )
						if ( actorReachable(RoadPoints.Path[i],0,1) )
						{
							Anchor = RoadPoints.Path[i];
							break;
						}
					if ( !Anchor )
					{
						// try to move toward a road
						ANavigationPoint *BestRoad = NULL;
						FVector ViewPoint = Location + FVector(0.f,0.f,CollisionHeight);
						for ( INT i=0; i<RoadPoints.numPoints; i++ )
						{
							GetLevel()->SingleLineCheck( Hit, this, RoadPoints.Path[i]->Location, ViewPoint, TRACE_World|TRACE_StopAtFirstHit );
							if ( Hit.Time == 1.f )
							{
								BestRoad = RoadPoints.Path[i];
								if ( Controller )
								{
									Controller->RouteGoal = EndAnchor;
									Controller->RouteCache[0] = BestRoad;
								}
								return 10.f;
							}
						}
					}
				}
			}

			if ( !Anchor )
			{
				FindAnchorFailedTime = Level->TimeSeconds;
				return 0.f;
			}
			LastValidAnchorTime = Level->TimeSeconds;
			LastAnchor = Anchor;
			StartDist = (Anchor->Location - Location).Size();
			if ( Abs(Anchor->Location.Z - Location.Z) < ::Max(CollisionHeight,Anchor->CollisionHeight) )
			{
				FLOAT StartDist2D = (Anchor->Location - Location).Size2D();
				if ( StartDist2D <= CollisionRadius + Anchor->CollisionRadius )
					StartDist = 0.f;
			}
		}
		if ( !EndAnchor && bSpecifiedEnd )
		{
			if ( DestPoints.numPoints > 0 )
				EndAnchor = DestPoints.findEndAnchor(this, goal, GoalLocation, (goal && Controller->AcceptNearbyPath(goal)), bOnlyCheckVisible );
			if ( !EndAnchor )
			{
				// look for road
				FSortedPathList RoadPoints;
				for ( ANavigationPoint *Nav=Level->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
				{
					if ( !Nav->bBlocked )
					{
						ARoadPathNode *RNav = Cast<ARoadPathNode>(Nav);
						if ( RNav )
						{
							FLOAT dist = (GoalLocation - RNav->Location).SizeSquared();
							if  ( dist > 0.5f * MAXPATHDISTSQ )
								RoadPoints.addPath(RNav, appRound(dist));
						}
					}
				}
				if ( RoadPoints.numPoints > 0 )
				{
					FVector RealLoc = Location;
					FVector AdjustedDest = GoalLocation;
					if ( goal )
						AdjustedDest.Z = AdjustedDest.Z + CollisionHeight - goal->CollisionHeight;
					if ( GetLevel()->FarMoveActor(this,AdjustedDest,1) )
					{
						for ( INT i=0; i<RoadPoints.numPoints; i++ )
							if ( actorReachable(RoadPoints.Path[i],0,1) )
							{
								EndAnchor = RoadPoints.Path[i];
								break;
							}
						GetLevel()->FarMoveActor(this, RealLoc, 1, 1);
					}
				}
			}
			if ( !EndAnchor )
			{
				ADecoration *Dec = Cast<ADecoration>(goal); // game flags are decorations
				if ( Dec )
					Dec->eventNotReachableBy(this);
				return 0.f;
			}
			if ( goal )
			{
				APawn* PawnGoal = goal->GetAPawn();
				if ( PawnGoal )
				{
					PawnGoal->LastValidAnchorTime = Level->TimeSeconds;
					PawnGoal->LastAnchor = EndAnchor;
				}
				else
				{
					ADecoration *Dec = Cast<ADecoration>(goal); // game flags are decorations
					if ( Dec )
					{
						Dec->LastValidAnchorTime = Level->TimeSeconds;
						Dec->LastAnchor = EndAnchor;
					}
				}
			}
			EndDist = (EndAnchor->Location - GoalLocation).Size();
		}
		if ( EndAnchor == Anchor )
		{
			// no way to get closer on the navigation network
			INT PassedAnchor = 0;

			if ( ReachedDestination(Anchor->Location - Location, goal) )
			{
				PassedAnchor = 1;
				if ( !goal )
					return 0.f;
			}
			else
			{
				// if on route (through or past anchor), keep going
				FVector GoalAnchor = GoalLocation - Anchor->Location;
				GoalAnchor = GoalAnchor.SafeNormal();
				FVector ThisAnchor = Location - Anchor->Location;
				ThisAnchor = ThisAnchor.SafeNormal();
				if ( (ThisAnchor | GoalAnchor) > 0.9 )
					PassedAnchor = 1;
			}

			if ( PassedAnchor )
				Controller->RouteCache[0] = goal;
			else
				Controller->RouteCache[0] = Anchor;
			return (GoalLocation - Location).Size();
		}
	}
	else
	{
		for ( ANavigationPoint *Nav=Level->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
		{
			if ( bCanFly && !Nav->bFlyingPreferred )
				Nav->TransientCost += 4000.f;

			Nav->ClearForPathFinding();
		}
	}
	//debugf(TEXT("Found anchors"));

	if ( EndAnchor )
	{
		EndAnchor->bEndPoint = 1;
		if ( Physics == PHYS_Karma )
		{
			UBOOL bEndPath = (Cast<ANavigationPoint>(goal) != NULL);
			UBOOL bSkip = false;

			// check if already close
			for ( INT i=0; i<EndAnchor->PathList.Num(); i++ )
			{
				if ( EndAnchor->PathList(i)->End == Anchor )
				{
					bSkip = true;
					break;
				}
			}

			if ( !bSkip )
			{
				// mark nearby nodes also
				FCheckResult Hit(1.f);
				for ( INT i=0; i<EndAnchor->PathList.Num(); i++ )
				{
					UReachSpec *UpStream = EndAnchor->PathList(i)->End->GetReachSpecTo(EndAnchor);
					if ( UpStream && ((UpStream->reachFlags & (R_PROSCRIBED | R_FLY)) == 0) )
					{
						Hit.Actor = NULL;
						if ( !bEndPath )
							GetLevel()->SingleLineCheck( Hit, this,  EndAnchor->PathList(i)->End->Location, GoalLocation, TRACE_World|TRACE_StopAtFirstHit );
						if ( !Hit.Actor )
							EndAnchor->PathList(i)->End->bEndPoint = 1;
					}
				}
			}
		}
	}

	GetLevel()->FarMoveActor(this, RealLocation, 1, 1);
	Anchor->visitedWeight = appRound(StartDist);
	if ( bSpecifiedEnd )
		NodeEval = &FindEndPoint;
	ANavigationPoint* BestDest = breadthPathTo(NodeEval,Anchor,calcMoveFlags(),&BestWeight, bWeightDetours);
	if ( BestDest )
	{
		Controller->SetRouteCache(BestDest,StartDist,EndDist);
		return BestWeight;
	}
	else if ( Level->Game && Level->Game->bLiberalVehiclePaths ) // FIXME - should allow this always - hack to limit to onslaught because not tested in UT Assault
	{
		//debugf(TEXT("Second Search"));
		AVehicle *V = Cast<AVehicle>(this);
		if ( V && V->Driver )
		{
			AController *OldController = V->Driver->Controller; // fixme - should always be NULL
			Controller->Pawn = V->Driver;
			V->Driver->Controller = Controller;
			V->Driver->Anchor = Anchor;
			BestDest = V->Driver->breadthPathTo(NodeEval,Anchor,V->Driver->calcMoveFlags(),&BestWeight, false);
			//debugf(TEXT("ON FOOT found %s"),BestDest->GetName());
			V->Driver->Controller = OldController;
			Controller->Pawn = V;
			if ( BestDest )
			{
				ANavigationPoint *N = Cast<ANavigationPoint>(Controller->RouteCache[0]);
				if ( Anchor == N )
				{
					for ( INT i=0; i<15; i++ )
						Controller->RouteCache[i] = Controller->RouteCache[i+1];
					Controller->RouteCache[15] = NULL;
					N = Cast<ANavigationPoint>(Controller->RouteCache[0]);
				}
				if ( N && (!bSuperSize || !N->bNoSuperSize) )
				{
					//debugf(TEXT("anchor %s route %s"),Anchor->GetName(), N->GetName());
					UReachSpec *spec = Anchor->GetReachSpecTo(N);
					if ( spec && spec->supports(CollisionRadius, CollisionHeight, calcMoveFlags(), MaxFallSpeed) )
					{
						//debugf(TEXT("IT WORKS"));
						Controller->SetRouteCache(BestDest,StartDist,EndDist);
						return BestWeight;
					}
				}
				// tell bot to get out, continue on foot
				Controller->SetRouteCache(BestDest,StartDist,EndDist);
				if ( !V->bKeyVehicle )
				{
					V->VehicleLostTime = Level->TimeSeconds + 12;
					if ( Controller->RouteCache[0] )
						Controller->DirectionHint = (Controller->RouteCache[0]->Location - V->Location).SafeNormal();
					if ( V->eventKDriverLeave(false) )
						return BestWeight;
				}
			}
		}
	}
	return 0.f;
	unguard;
}

/* addPath()
add a path to a sorted path list - sorted by distance
*/

void FSortedPathList::addPath(ANavigationPoint *node, INT dist)
{
	guard(FSortedPathList::addPath);
	int n=0; 
	if ( numPoints > 8 )
	{
		if ( dist > Dist[numPoints/2] )
		{
			n = numPoints/2;
			if ( (numPoints > 16) && (dist > Dist[n + numPoints/4]) )
				n += numPoints/4;
		}
		else if ( (numPoints > 16) && (dist > Dist[numPoints/4]) )
			n = numPoints/4;
	}

	while ((n < numPoints) && (dist > Dist[n]))
		n++;

	if (n < MAXSORTED)
	{
		if (n == numPoints)
		{
			Path[n] = node;
			Dist[n] = dist;
			numPoints++;
		}
		else
		{
			ANavigationPoint *nextPath = Path[n];
			INT nextDist = Dist[n];
			Path[n] = node;
			Dist[n] = dist;
			if (numPoints < MAXSORTED)
				numPoints++;
			n++;
			while (n < numPoints) 
			{
				ANavigationPoint *afterPath = Path[n];
				INT afterDist = Dist[n];
				Path[n] = nextPath;
				Dist[n] = nextDist;
				nextPath = afterPath;
				nextDist = afterDist;
				n++;
			}
		}
	}
	unguard;
}

//-------------------------------------------------------------------------------------------------
/* breadthPathTo()
Breadth First Search through navigation network
starting from path bot is on.

Return when NodeEval function returns 1
*/
ANavigationPoint* APawn::breadthPathTo(NodeEvaluator NodeEval, ANavigationPoint *start, int moveFlags, FLOAT *Weight, UBOOL bWeightDetours)
{
	guard(APawn::breadthPathTo);

	ANavigationPoint* currentnode = start;
	ANavigationPoint* nextnode = NULL;
	ANavigationPoint* LastAdd = currentnode;
	ANavigationPoint* BestDest = NULL;

	INT iRadius = appFloor(CollisionRadius);
	INT iHeight = appFloor(CollisionHeight);
	INT iMaxFallSpeed = appFloor(MaxFallSpeed);
	FLOAT CrouchMultiplier = CROUCHCOSTMULTIPLIER * 1.f/CrouchedPct;
	INT DefaultCollisionHeight = GetClass()->GetDefaultActor()->CollisionHeight;

	if ( bCanCrouch )
	{
		iHeight = appFloor(CrouchHeight);
		iRadius = appFloor(CrouchRadius);
	}
	INT n = 0;
	if ( Controller )
		Controller->eventSetupSpecialPathAbilities();

	while ( currentnode )
	{
		currentnode->bAlreadyVisited = true;
		//debugf(TEXT("Distance to %s is %d"), currentnode->GetName(), currentnode->visitedWeight);
		FLOAT thisWeight = (*NodeEval)(currentnode, this, *Weight);
		if ( thisWeight > *Weight )
		{
			*Weight = thisWeight;
			BestDest = currentnode;
		}
		if ( *Weight >= 1.f )
			return CheckDetour(BestDest, start, bWeightDetours);
		if ( n++ > 200 )
		{
			if ( *Weight > 0 )
				return CheckDetour(BestDest, start, bWeightDetours);
			else
				n = 150;
		}
		INT nextweight = 0;

		for ( INT i=0; i<currentnode->PathList.Num(); i++ )
		{
			ANavigationPoint* endnode = NULL;
			UReachSpec *spec = currentnode->PathList(i);
			//debugf(TEXT("check path from %s to %s with %d, %d"),spec->Start->GetName(), spec->End->GetName(), spec->CollisionRadius, spec->CollisionHeight);
			if ( spec && spec->End && !spec->End->bAlreadyVisited && spec->supports(iRadius, iHeight, moveFlags, iMaxFallSpeed) && (!bSuperSize || !spec->End->bNoSuperSize) )
			{
				endnode = spec->End;
				if ( !endnode->bBlocked && (!endnode->bMayCausePain || !HurtByVolume(endnode)) )
				{
					if ( spec->bForced && endnode->bSpecialForced )
						nextweight = spec->Distance + endnode->eventSpecialCost(this,spec);
					else if( spec->CollisionHeight >= DefaultCollisionHeight )
						nextweight = spec->Distance + endnode->cost;
					else
						nextweight = CrouchMultiplier * spec->Distance + endnode->cost;

					if ( nextweight <= 0 )
					{
						debugf(TEXT("WARNING - negative weight %d from %s to %s"), nextweight, currentnode->GetName(), endnode->GetName());
						nextweight = 1;
					}
					INT newVisit = nextweight + currentnode->visitedWeight; 
					//debugf(TEXT("Path from %s to %s costs %d total %d"), currentnode->GetName(), endnode->GetName(), nextweight, newVisit);
					if ( endnode->visitedWeight > newVisit )
					{
						// found a better path to endnode
						endnode->previousPath = currentnode;
						if ( endnode->prevOrdered ) //remove from old position
						{
							endnode->prevOrdered->nextOrdered = endnode->nextOrdered;
							if (endnode->nextOrdered)
								endnode->nextOrdered->prevOrdered = endnode->prevOrdered;
							if ( (LastAdd == endnode) || (LastAdd->visitedWeight > endnode->visitedWeight) )
								LastAdd = endnode->prevOrdered;
							endnode->prevOrdered = NULL;
							endnode->nextOrdered = NULL;
						}
						else check(!endnode->nextOrdered);
						endnode->visitedWeight = newVisit;

						// LastAdd is a good starting point for searching the list and inserting this node
						nextnode = LastAdd;
						if ( nextnode->visitedWeight <= newVisit )
						{
							while ( nextnode->nextOrdered && (nextnode->nextOrdered->visitedWeight < newVisit) )
								nextnode = nextnode->nextOrdered;
						}
						else
						{
							while ( nextnode->prevOrdered && (nextnode->visitedWeight > newVisit) )
								nextnode = nextnode->prevOrdered;
						}

						if (nextnode->nextOrdered != endnode)
						{
							if (nextnode->nextOrdered)
								nextnode->nextOrdered->prevOrdered = endnode;
							endnode->nextOrdered = nextnode->nextOrdered;
							nextnode->nextOrdered = endnode;
							endnode->prevOrdered = nextnode;
						}
						LastAdd = endnode;
					}
				}
			}
		}
		currentnode = currentnode->nextOrdered;
	}
	return CheckDetour(BestDest, start, bWeightDetours);
	unguard;
}

ANavigationPoint* APawn::CheckDetour(ANavigationPoint* BestDest, ANavigationPoint* Start, UBOOL bWeightDetours)
{
	guard(APawn::CheckDetour);

	if ( !bWeightDetours || !Start || !BestDest || (Start == BestDest) || !Anchor || IsA(AVehicle::StaticClass()) )
		return BestDest;

	ANavigationPoint* DetourDest = NULL;
	FLOAT DetourWeight = 0.f;

	// FIXME - mark list to ignore (if already in route)
	for ( INT i=0; i<Anchor->PathList.Num(); i++ )
	{
		UReachSpec *spec = Anchor->PathList(i);
		if ( spec->End->visitedWeight < 2.f * MAXPATHDIST )
		{
			UReachSpec *Return = spec->End->GetReachSpecTo(Anchor);
			if ( Return && !Return->bForced )
			{
				spec->End->LastDetourWeight = spec->End->eventDetourWeight(this,spec->End->visitedWeight);
				if ( spec->End->LastDetourWeight > DetourWeight )
					DetourDest = spec->End;
			}
		}
	}
	if ( !DetourDest )
		return BestDest;

	ANavigationPoint *FirstPath = BestDest;
	// check that detourdest doesn't occur in route
	for ( ANavigationPoint *Path=BestDest; Path!=NULL; Path=Path->previousPath )
	{
		FirstPath = Path;
		if ( Path == DetourDest )
			return BestDest;
	}

	// check that AI really wants to detour
	if ( !Controller )
		return BestDest;
	Controller->RouteGoal = BestDest;
	Controller->RouteDist = BestDest->visitedWeight;
	if ( !Controller->eventAllowDetourTo(DetourDest) )
		return BestDest;

	// add detourdest to start of route
	if ( FirstPath != Anchor )
	{
		FirstPath->previousPath = Anchor;
		Anchor->previousPath = DetourDest;
		DetourDest->previousPath = NULL;
	}
	else 
	{
		for ( ANavigationPoint *Path=BestDest; Path!=NULL; Path=Path->previousPath )
			if ( Path->previousPath == Anchor )
			{
				Path->previousPath = DetourDest;
				DetourDest->previousPath = Anchor;
				break;
			}
	}

	return BestDest;
	unguard;
}

/* SetRouteCache() puts the first 16 navigationpoints in the best route found in the 
Controller's RouteCache[].  
*/
void AController::SetRouteCache(ANavigationPoint *EndPath, FLOAT StartDist, FLOAT EndDist)
{
	guard(AController::SetRouteCache);

	RouteGoal = EndPath;
	if ( !EndPath )
		return;
	RouteDist = EndPath->visitedWeight + EndDist;

	// reverse order of linked node list
	EndPath->nextOrdered = NULL;
	INT i = 0;
	while ( EndPath->previousPath )
	{
		EndPath->previousPath->nextOrdered = EndPath;
		EndPath = EndPath->previousPath;
		i++;
		if ( i > 500 )
		{
			debugf(TEXT("prev %s"),EndPath->GetName());
			if ( i > 600 )
				check(0);
		}

	}
	// if the pawn is on the start node, then the first node in the path should be the next one

	if ( Pawn && (StartDist > 0.f) )
	{
		// if pawn not on the start node, check if second node on path is a better destination
		if ( EndPath->nextOrdered )
		{
			UBOOL bRoadSuccess = false;
			// if vehicle && already on path, then keep going
			if ( Cast<AVehicle>(Pawn) )
			{
				UReachSpec *R = EndPath->GetReachSpecTo(EndPath->nextOrdered);
				if ( R )
				{
					FVector CurrentPathDir = (R->End->Location - R->Start->Location).SafeNormal();
					FVector LineDir = Pawn->Location - (R->Start->Location + (CurrentPathDir | (Pawn->Location - R->Start->Location)) * CurrentPathDir);
					if ( LineDir.SizeSquared() < (R->CollisionRadius + Pawn->NavigationPointRange) * (R->CollisionRadius + Pawn->NavigationPointRange) )
					{
						bRoadSuccess = true;
						EndPath = EndPath->nextOrdered;
					}
				}
			}
			if ( !bRoadSuccess )
			{
				FLOAT TwoDist = (Pawn->Location - EndPath->nextOrdered->Location).Size();
				FLOAT PathDist = (EndPath->Location - EndPath->nextOrdered->Location).Size();
				FLOAT MaxDist = 0.75f * MAXPATHDIST;
				if ( EndPath->nextOrdered->IsA(AFlyingPathNode::StaticClass()) )
					MaxDist = ::Max(MaxDist,EndPath->nextOrdered->CollisionRadius);
				if ( (TwoDist < MaxDist) && (TwoDist < PathDist) 
					&& ((Level->NetMode != NM_Standalone) || (Level->TimeSeconds - Pawn->LastRenderTime < 5.f) || (StartDist > 250.f)) )
				{
					FCheckResult Hit(1.f);
					GetLevel()->SingleLineCheck( Hit, this, EndPath->nextOrdered->Location, Pawn->Location, TRACE_World|TRACE_StopAtFirstHit );
					if ( !Hit.Actor	&& Pawn->actorReachable(EndPath->nextOrdered, 1, 1) )
						EndPath = EndPath->nextOrdered;
				}
			}
		}

	}
	else if ( EndPath->nextOrdered )
		EndPath = EndPath->nextOrdered;

	// put first 16 nodes of path into the Controller's RouteCache
	for ( int i=0; i<16; i++ )
	{
		if ( EndPath )
		{
			RouteCache[i] = EndPath;
			EndPath = EndPath->nextOrdered;
		}
		else
			RouteCache[i] = NULL;
	}
	if ( Pawn && RouteCache[1] )
	{
		ANavigationPoint *FirstPath = Cast<ANavigationPoint>(RouteCache[0]);
		UReachSpec* NextSpec = NULL;
		if ( FirstPath )
		{
			ANavigationPoint *SecondPath = Cast<ANavigationPoint>(RouteCache[1]);
			if ( SecondPath )
				NextSpec = FirstPath->GetReachSpecTo(SecondPath);
		}
		if ( NextSpec )
			Pawn->NextPathRadius = NextSpec->CollisionRadius;
		else
			Pawn->NextPathRadius = 0.f;
	}
	unguard;
}



