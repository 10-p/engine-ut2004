/*=============================================================================
	UnPath.cpp: Unreal pathnode placement

	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	These methods are members of the FPathBuilder class, which adds pathnodes to a level.  
	Paths are placed when the level is built.  FPathBuilder is not used during game play
 
   General comments:
   Path building
   The FPathBuilder does a tour of the level (or a part of it) using the "Scout" actor, starting from
   every actor in the level.  
   This guarantees that correct reachable paths are generated to all actors.  It starts by going to the
   wall to the right of the actor, and following that wall, keeping it to the left.  NOTE: my definition
   of left and right is based on normal Cartesian coordinates, and not screen coordinates (e.g. Y increases
   upward, not downward).  This wall following is accomplished by moving along the wall, constantly looking
   for a leftward passage.  If the FPathBuilder is stopped, he rotates clockwise until he can move forward 
   again.  While performing this tour, it keeps track of the set of previously placed pathnodes which are
   visible and reachable.  If a pathnode is removed from this set, then the FPathBuilder searches for an 
   acceptable waypoint.  If none is found, a new pathnode is added to provide a waypoint back to the no longer
   reachable pathnode.  The tour ends when a full circumlocution has been made, or if the FPathBuilder 
   encounters a previously placed path node going in the same direction.  Pathnodes that were not placed as
   the result of a left turn are marked, since they are due to some possibly unmapped obstruction.  In the
   final phase, these obstructions are inspected.  Then, the pathnode due to the obstruction is removed (since
   the obstruction is now mapped, and any needed waypoints have been built).

	Revision history:
		* Created by Steven Polge 3/97
=============================================================================*/

#include "EnginePrivate.h"
#include "UnPath.h"

/* buildPaths()
map out level and place NavigationPoints as needed
*/
int ENGINE_API FPathBuilder::buildPaths (ULevel *ownerLevel)
{
	guard(FPathBuilder::buildPaths);

	FLOAT BuildTime = -1 * appSeconds();

	Level = ownerLevel;

	definePaths( ownerLevel );

	int numpaths = 0;
	getScout();

	Scout->SetCollision(1, 1);
	Scout->bCollideWorld = 1;

	Scout->JumpZ = -1.f; //NO jumping
	Scout->GroundSpeed = TESTGROUNDSPEED; 

	// first pass define paths to pre-place inventory spots, etc.
	definePaths( ownerLevel );

	SetPathCollision(1);
	numpaths = numpaths + createPaths();
	SetPathCollision(0);
	Level->DestroyActor(Scout->Controller);
	Level->DestroyActor(Scout);

	definePaths( ownerLevel );

	BuildTime+=appSeconds();
	debugf(TEXT("Total paths build time %f seconds"),BuildTime);

	return numpaths;
	unguard;
}

/* removePaths()
remove NavigationPoints previously added by PathsBuild operation
*/
int ENGINE_API FPathBuilder::removePaths (ULevel *ownerLevel)
{
	guard(FPathBuilder::removePaths);
	Level = ownerLevel;
	int removed = 0;

	for (INT i=0; i<Level->Actors.Num(); i++)
	{
		ANavigationPoint *Nav = Cast<ANavigationPoint>(Level->Actors(i));
		if (Nav && Nav->bAutoBuilt )
		{
			removed++;
			Level->DestroyActor( Nav ); 
		}
	}
	Level->GetLevelInfo()->bPathsRebuilt = 0;
	return removed;
	unguard;
}

void ENGINE_API FPathBuilder::undefinePaths (ULevel *ownerLevel)
{
	guard(FPathBuilder::undefinePaths);
	Level = ownerLevel;

	//remove all reachspecs
	debugf(NAME_DevPath,TEXT("Remove old reachspecs"));

	// clear navigationpointlist
	Level->GetLevelInfo()->NavigationPointList = NULL;

	GWarn->BeginSlowTask( TEXT("Undefining Paths"), 1);

	//clear NavigationPoints
	for (INT i=0; i<Level->Actors.Num(); i++)
	{
		GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Undefining") );

		ANavigationPoint *Nav = Cast<ANavigationPoint>(Level->Actors(i)); 
		if ( Nav )
		{
			if ( !(Nav->GetClass()->ClassFlags & CLASS_Placeable) )
			{
				/* delete any nodes which aren't placeable, because they were automatically generated,
				  and will be automatically generated again. */
				Level->DestroyActor(Nav);
			}
			else
				Nav->ClearPaths();
		}
		else if ( Level->Actors(i) )
			Level->Actors(i)->ClearMarker();
	}

	Level->GetLevelInfo()->bPathsRebuilt = 0;
	GWarn->EndSlowTask();
	unguard;
}

void FPathBuilder::ReviewPaths(ULevel *ownerLevel)
{
	guard(FPathBuilder::ReviewPaths);

	debugf(TEXT("FPathBuilder reviewing paths"));
    GWarn->MapCheck_Clear();
	GWarn->BeginSlowTask( TEXT("Reviewing paths..."), 1 );

	Level = ownerLevel;

	if ( !Level || !Level->GetLevelInfo() || !Level->GetLevelInfo()->NavigationPointList )
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("No navigation point list. Paths define needed."));
	else
	{
		INT NumDone = 0;
		INT NumPaths = 0;
		INT NumStarts = 0;
		for ( ANavigationPoint* N=Level->GetLevelInfo()->NavigationPointList; N!=NULL; N=N->nextNavigationPoint )
		{
			if ( Cast<APlayerStart>(N) )
				NumStarts++;
			NumPaths++;
		}
		if ( NumStarts < 16 )
			GWarn->MapCheck_Add( MCTYPE_ERROR, Level->GetLevelInfo()->NavigationPointList, *FString::Printf(TEXT("Only %d PlayerStarts in this level"),NumStarts));

		getScout();
		SetPathCollision(1);
		for ( ANavigationPoint* N=Level->GetLevelInfo()->NavigationPointList; N!=NULL; N=N->nextNavigationPoint )
		{
			GWarn->StatusUpdatef( NumDone, NumPaths, TEXT("Reviewing Paths") );
			N->ReviewPath(Scout);
			NumDone++;
		}
		SetPathCollision(0);
		Level->DestroyActor(Scout->Controller);
		Level->DestroyActor(Scout);

		for (INT i=0; i<Level->Actors.Num(); i++)
		{
			GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Reviewing Movers") );
			AMover *M = Cast<AMover>(Level->Actors(i)); 
			if ( M && !M->bNoAIRelevance )
			{
				if ( !M->myMarker )
					GWarn->MapCheck_Add( MCTYPE_ERROR, M, TEXT("No navigation point associated with this mover!"));
				else
				{
					ALiftCenter *L = Cast<ALiftCenter>(M->myMarker);
					if ( L )
					{
						if ( L->LiftTag != M->Tag )
							GWarn->MapCheck_Add( MCTYPE_ERROR, L, TEXT("LiftTag does not match the lift this LiftCenter is based on"));
						if ( L->Base != M )
							GWarn->MapCheck_Add( MCTYPE_ERROR, L, TEXT("LiftCenter not based on its lift"));
					}
				}
			}
			APickup *P = Cast<APickup>(Level->Actors(i));
			if ( P && !(P->GetClass()->ClassFlags & CLASS_Placeable) )
				GWarn->MapCheck_Add( MCTYPE_WARNING, P, TEXT("This pickup should have a PickupBase"));
		}
	}
	GWarn->EndSlowTask();
    GWarn->MapCheck_ShowConditionally();
	unguard;
}

void FPathBuilder::SetPathCollision(INT bEnabled)
{
	guard(FPathBuilder::SetPathCollision);

	if ( bEnabled )
	{
		//Level->SetActorCollision( 1 );
		for ( INT i=0; i<Level->Actors.Num(); i++)
		{
			AActor *Actor = Level->Actors(i); 
			// turn off collision - for non-static actors with bPathColliding false
			if ( Actor && !Actor->bDeleteMe )
			{
				if ( Actor->bBlockActors && Actor->bCollideActors && !Actor->bStatic && !Actor->bPathColliding )
				{
					Actor->bPathTemp = 1;
					Actor->SetCollision(0,Actor->bBlockActors);
					//debugf(TEXT("Collision turned OFF for %s"),Actor->GetName());
				}
				else 
				{
					//if ( Actor->bCollideActors && Actor->bBlockActors )
					//	debugf(TEXT("Collision left ON for %s"),Actor->GetName());
					Actor->bPathTemp = 0;
				}
			}
		}
	}
	else
	{
		for ( INT i=0; i<Level->Actors.Num(); i++)
		{
			AActor *Actor = Level->Actors(i); 
			if ( Actor && Actor->bPathTemp )
			{
				Actor->bPathTemp = 0;
				Actor->SetCollision(1,Actor->bBlockActors);
			}
		}
		//Level->SetActorCollision( 0 );
	}
	unguard;
}

void ENGINE_API FPathBuilder::definePaths(ULevel *ownerLevel)
{
	guard(FPathBuilder::definePaths);

	// remove old paths
	undefinePaths( ownerLevel );

	Level = ownerLevel;
	getScout();
	Level->GetLevelInfo()->NavigationPointList = NULL;
	Level->GetLevelInfo()->bHasPathNodes = false;
    GWarn->MapCheck_Clear();
	GWarn->BeginSlowTask( TEXT("Defining Paths"), 1);

	INT NumPaths = 0; //used for progress bar
	SetPathCollision(1);

	// Add NavigationPoint markers to any actors which want to be marked
	for (INT i=0; i<Level->Actors.Num(); i++)
	{
		GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Defining") );

		AActor *Actor = Level->Actors(i); 
		if ( Actor )
			NumPaths += Actor->AddMyMarker(Scout);
	}

	// stick navigation points on bases  (level or terrain)
	// at the same time, create the navigation point list
	INT NumDone = 0;
	FCheckResult Hit(1.f);
	for ( INT i=0; i<Level->Actors.Num(); i++)
	{
		GWarn->StatusUpdatef( NumDone, NumPaths, TEXT("Navigation Points on Bases") );

		ANavigationPoint *Nav = Cast<ANavigationPoint>(Level->Actors(i)); 
		if ( Nav && !Nav->bDeleteMe )
		{
			NumDone++;
			Nav->nextNavigationPoint = Level->GetLevelInfo()->NavigationPointList;
			Level->GetLevelInfo()->NavigationPointList = Nav;
			Nav->FindBase();
		}
	}

	for( ANavigationPoint *Nav=Level->GetLevelInfo()->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
		Nav->InitForPathFinding();
	
	//calculate and add reachspecs to pathnodes
	debugf(NAME_DevPath,TEXT("Add reachspecs"));
	NumDone = 0;
	for( ANavigationPoint *Nav=Level->GetLevelInfo()->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
	{
		GWarn->StatusUpdatef( NumDone, NumPaths, TEXT("Adding Reachspecs") );
		Nav->addReachSpecs(Scout);
		NumDone++;
		debugf( NAME_DevPath, TEXT("Added reachspecs to %s"),Nav->GetName() );
	}

	for( ANavigationPoint *Nav=Level->GetLevelInfo()->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
		Nav->PostaddReachSpecs(Scout);

	//prune excess reachspecs
	debugf(NAME_DevPath,TEXT("Prune reachspecs"));
	INT numPruned = 0;
	NumDone = 0;
	for( ANavigationPoint *Nav=Level->GetLevelInfo()->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
	{
		GWarn->StatusUpdatef( NumDone, NumPaths, TEXT("Pruning Reachspecs") );
		numPruned += Nav->PrunePaths();
		NumDone++;
	}
	debugf(NAME_DevPath,TEXT("Pruned %d reachspecs"), numPruned);

	// set up terrain navigation
	GWarn->StatusUpdatef( NumDone, NumPaths, TEXT("Terrain Navigation") );
	
	// turn off collision and reset temporarily changed actors
	SetPathCollision(0);

	// clear pathschanged flags
	for( ANavigationPoint *Nav=Level->GetLevelInfo()->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
		Nav->bPathsChanged = false;

	Level->DestroyActor(Scout->Controller);
	Level->DestroyActor(Scout);
	Level->GetLevelInfo()->bPathsRebuilt = 1;

 	for (INT i=0; i<Level->Actors.Num(); i++)
	{
		AActor *Actor = Level->Actors(i); 
		if ( Actor )
			Actor->CheckForErrors();
	}
	debugf(NAME_DevPath,TEXT("All done"));
    GWarn->MapCheck_ShowConditionally();
	unguard;


	GWarn->EndSlowTask();
}

void ENGINE_API FPathBuilder::defineChangedPaths (ULevel *ownerLevel)
{
	guard(FPathBuilder::defineChangedPaths);

	// remove old paths
	undefinePaths( ownerLevel );

	Level = ownerLevel;
	getScout();
	Level->GetLevelInfo()->NavigationPointList = NULL;
    GWarn->MapCheck_Clear();
	GWarn->BeginSlowTask( TEXT("Defining Paths"), 1);

	INT NumPaths = 0; //used for progress bar
	SetPathCollision(1);

	// find out how many paths there are
	for ( INT i=0; i<Level->Actors.Num(); i++ )
	{
		GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Defining") );

		ANavigationPoint *Nav = Cast<ANavigationPoint>(Level->Actors(i)); 
		if ( Nav )
			NumPaths++;
	}

	// Don't add any NavigationPoint markers

	// stick changed navigation points on bases  (level or terrain), and add new paths to the navigation point list
	INT NumDone = 0;
	Level->GetLevelInfo()->NavigationPointList = NULL;
	FCheckResult Hit(1.f);
	for ( INT i=0; i<Level->Actors.Num(); i++)
	{
		GWarn->StatusUpdatef( NumDone, NumPaths, TEXT("Navigation Points on Bases") );

		ANavigationPoint *Nav = Cast<ANavigationPoint>(Level->Actors(i)); 

		if ( Nav && !Nav->bDeleteMe )
		{
			NumDone++;
			Nav->nextNavigationPoint = Level->GetLevelInfo()->NavigationPointList;
			Level->GetLevelInfo()->NavigationPointList = Nav;
			Nav->FindBase();
		}
	}

	for( ANavigationPoint *Nav=Level->GetLevelInfo()->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
		Nav->InitForPathFinding();
	
	//calculate and add reachspecs to pathnodes
	debugf(NAME_DevPath,TEXT("Add reachspecs"));
	NumDone = 0;
	for( ANavigationPoint *Nav=Level->GetLevelInfo()->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
	{
		GWarn->StatusUpdatef( NumDone, NumPaths, TEXT("Adding Reachspecs") );
		Nav->addReachSpecs(Scout,true);
		debugf( NAME_DevPath, TEXT("Added reachspecs to %s"),Nav->GetName() );
		NumDone++;
	}

	for( ANavigationPoint *Nav=Level->GetLevelInfo()->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
		Nav->PostaddReachSpecs(Scout);

	//prune excess reachspecs
	debugf(NAME_DevPath,TEXT("Prune reachspecs"));
	INT numPruned = 0;
	NumDone = 0;
	for( ANavigationPoint *Nav=Level->GetLevelInfo()->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
	{
		GWarn->StatusUpdatef( NumDone, NumPaths, TEXT("Pruning Reachspecs") );
		numPruned += Nav->PrunePaths();
		NumDone++;
	}
	debugf(NAME_DevPath,TEXT("Pruned %d reachspecs"), numPruned);

	// set up terrain navigation
	GWarn->StatusUpdatef( NumDone, NumPaths, TEXT("Terrain Navigation") );
	
	// turn off collision and reset temporarily changed actors
	SetPathCollision(0);

	// clear pathschanged flags
	for( ANavigationPoint *Nav=Level->GetLevelInfo()->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
		Nav->bPathsChanged = false;

	Level->DestroyActor(Scout->Controller);
	Level->DestroyActor(Scout);
	debugf(NAME_DevPath,TEXT("All done"));
    GWarn->MapCheck_ShowConditionally();
	unguard;

	GWarn->EndSlowTask();
}

//------------------------------------------------------------------------------------------------
// certain actors add paths to mark their position
INT ANavigationPoint::AddMyMarker(AActor *S)
{
	guard(ANavigationPoint::AddMyMarker);

	return 1;
	unguard;
}

INT APathNode::AddMyMarker(AActor *S)
{
	guard(APathNode::AddMyMarker);

	Level->bHasPathNodes = true;
	return 1;
	unguard;
}

INT AWarpZoneInfo::AddMyMarker(AActor *S)
{
	guard(AWarpZoneInfo::AddMyMarker);

	AScout* Scout = Cast<AScout>(S);
	if ( !Scout )
		return 0;

	if ( !Scout->findStart(Location) || (Scout->Region.Zone != Region.Zone) )
	{
		Scout->SetCollisionSize(HUMANRADIUS, Scout->CollisionHeight);
		if ( !Scout->findStart(Location) || (Scout->Region.Zone != Region.Zone) )
			GetLevel()->FarMoveActor(Scout, Location, 1, 1);
		Scout->SetCollisionSize(MINCOMMONRADIUS, Scout->CollisionHeight);
	}
	UClass* pathClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("WarpZoneMarker") );
	AWarpZoneMarker *newMarker = Cast<AWarpZoneMarker>(GetLevel()->SpawnActor(pathClass, NAME_None, Scout->Location));
	newMarker->markedWarpZone = this;
	return 1;
	unguard;
}

INT APickup::AddMyMarker(AActor *S)
{
	guard(APickup::AddMyMarker);

	AScout* Scout = Cast<AScout>(S);
	if ( !Scout )
		return 0;

	if ( !Scout->findStart(Location) || (Abs(Scout->Location.Z - Location.Z) > Scout->CollisionHeight) )
		GetLevel()->FarMoveActor(Scout, Location + FVector(0,0,HUMANRADIUS - CollisionHeight), 1, 1);
	UClass *pathClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("InventorySpot") );
	myMarker = Cast<AInventorySpot>(GetLevel()->SpawnActor(pathClass, NAME_None, Scout->Location));
	myMarker->markedItem = this;
	return 1;
	unguard;
}

void APickup::ClearMarker()
{
	guard(APickup::ClearMarker);

	myMarker = NULL;
	unguard;
}

INT AMover::AddMyMarker(AActor *S)
{
	guard(AMover::AddMyMarker);

	if ( !bAutoDoor )
		return 0;
	AScout* Scout = Cast<AScout>(S);
	if ( !Scout )
		return 0;

	if ( !Scout->findStart(Location) || (Abs(Scout->Location.Z - Location.Z) > Scout->CollisionHeight) )
		GetLevel()->FarMoveActor(Scout, Location + FVector(0,0,HUMANRADIUS - CollisionHeight), 1, 1);
	UClass *pathClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("AutoDoor") );
	ADoor *DoorMarker = Cast<ADoor>(GetLevel()->SpawnActor(pathClass, NAME_None, Scout->Location));
	myMarker = DoorMarker;
	DoorMarker->MyDoor = this;
	DoorMarker->DoorTag = Tag;
	return 1;
	unguard;
}

void AMover::ClearMarker()
{
	guard(AMover::ClearMarker);

	myMarker = NULL;
	unguard;
}

INT AAIScript::AddMyMarker(AActor *S)
{
	guard(AAIScript::AddMyMarker);

	if ( !bNavigate )
		return 0;

	AScout* Scout = Cast<AScout>(S);
	if ( !Scout )
		return 0;

	if ( !Scout->findStart(Location) || (Abs(Scout->Location.Z - Location.Z) > Scout->CollisionHeight) )
		GetLevel()->FarMoveActor(Scout, Location + FVector(0,0,HUMANRADIUS - CollisionHeight), 1, 1);
	UClass *pathClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("AIMarker") );
	myMarker = Cast<AAIMarker>(GetLevel()->SpawnActor(pathClass, NAME_None, Scout->Location));
	myMarker->markedScript = this;
	return 1;
	unguard;
}


INT AVehicle::AddMyMarker(AActor *S)
{
	guard(AVehicle::AddMyMarker);
/*
	if ( !bStationary || bAutoTurret  )
		return 0;

	AScout* Scout = Cast<AScout>(S);
	if ( !Scout )
		return 0;

	UBOOL bRealBlockActors = bBlockActors;
	SetCollision(bCollideActors, false);
	if ( !Scout->findStart(Location) || (Abs(Scout->Location.Z - Location.Z) > Scout->CollisionHeight) )
		GetLevel()->FarMoveActor(Scout, Location + FVector(0,0,HUMANRADIUS - CollisionHeight), 1, 1);
	UClass *pathClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("AIMarker") );
	SetCollision(bCollideActors, bRealBlockActors);
	myMarker = Cast<AAIMarker>(GetLevel()->SpawnActor(pathClass, NAME_None, Scout->Location));
	myMarker->bDestinationOnly = true;
	return 1;
*/
	return 0;
	unguard;
}

INT ASVehicleFactory::AddMyMarker(AActor *S)
{
	guard(ASVehicleFactory::AddMyMarker);

	AScout* Scout = Cast<AScout>(S);
	if ( !Scout )
		return 0;

	UBOOL bRealBlockActors = bBlockActors;
	SetCollision(bCollideActors, false);
	if ( !Scout->findStart(Location) || (Abs(Scout->Location.Z - Location.Z) > Scout->CollisionHeight) )
		GetLevel()->FarMoveActor(Scout, Location + FVector(0,0,HUMANRADIUS - CollisionHeight), 1, 1);
	debugf(TEXT("Add marker for %s at %f %f %f vs. %f %f %f"),GetName(), Location.X, Location.Y, Location.Z, Scout->Location.X, Scout->Location.Y, Scout->Location.Z);
	UClass *pathClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("NavigationPoint") );
	ANavigationPoint *MyMarker = Cast<ANavigationPoint>(GetLevel()->SpawnActor(pathClass, NAME_None, Scout->Location));
	if ( MyMarker )
		MyMarker->ExtraCost = 2000.f;
	SetCollision(bCollideActors, bRealBlockActors);
	return 1;
	unguard;
}

void AAIScript::ClearMarker()
{
	guard(AAIScript::ClearMarker);

	myMarker = NULL;
	unguard;
}

FVector ALadderVolume::FindCenter()
{
	FVector Center(0.f,0.f,0.f);
	for(INT PolygonIndex = 0;PolygonIndex < Brush->Polys->Element.Num();PolygonIndex++)
	{
		FPoly&	Poly = Brush->Polys->Element(PolygonIndex);
		FVector NewPart(0.f,0.f,0.f);
		for(INT VertexIndex = 0;VertexIndex < Poly.NumVertices;VertexIndex++)
			NewPart += Poly.Vertex[VertexIndex];
		NewPart /= Poly.NumVertices;
		Center += NewPart;
	}
	Center /= Brush->Polys->Element.Num();
	return Center;
}

INT ALadderVolume::AddMyMarker(AActor *S)
{
	guard(ALadderVolume::AddMyMarker);
	
	if ( !bAutoPath || !Brush )
		return 0;

	FVector Center = FindCenter();
	Center = LocalToWorld().TransformFVector(Center);

	AScout* Scout = Cast<AScout>(S);
	if ( !Scout )
		return 0 ;

	UClass *pathClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("AutoLadder") );

	// find ladder bottom
	FCheckResult Hit(1.f);
	GetLevel()->SingleLineCheck(Hit, this, Center - 10000.f * ClimbDir, Center, TRACE_World);
	if ( Hit.Time == 1.f )
		return 0;
	FVector Position = Hit.Location + pathClass->GetDefaultActor()->CollisionHeight * ClimbDir;

	// place Ladder at bottom of volume
	GetLevel()->SpawnActor(pathClass, NAME_None, Position);

	// place Ladder at top of volume + 0.5 * CollisionHeight of Ladder
	Position = FindTop(Center + 500.f * ClimbDir); 
	GetLevel()->SpawnActor(pathClass, NAME_None, Position - 5.f * ClimbDir);
	return 2;
	unguard;
}

// find the edge of the brush in the ClimbDir direction
FVector ALadderVolume::FindTop(FVector V)
{
	guard(ALadderVolume::FindTop);

	if ( Encompasses(V) )
		return FindTop(V + 500.f * ClimbDir);

	// trace back to brush edge from this outside point
	FCheckResult Hit(1.f);
	GetPrimitive()->LineCheck( Hit, this, V - 10000.f * ClimbDir, V, FVector(0.f,0.f,0.f), 0, 0 );
	return Hit.Location;

	unguard;
}

//------------------------------------------------------------------------------------------------
//Private methods

/* createPaths()
build paths for a given pawn type (which Scout is emulating)
*/
int FPathBuilder::createPaths ()
{
	guard(FPathBuilder::createPaths);

	GWarn->BeginSlowTask( TEXT("Creating"), 1);

	// make sure all navigation points are cleared
	for ( INT i=0; i<Level->Actors.Num(); i++ ) 
	{
		GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Clearing existing paths") );

		ANavigationPoint *Nav = (ANavigationPoint *)(Level->Actors(i));
		if ( Nav && !Nav->bDeleteMe && Nav->IsA(ANavigationPoint::StaticClass()) )
		{
			Nav->visitedWeight = 1;
			Nav->bEndPoint = 0;
		}
	}

	// multi pass build paths from every playerstart or inventory position in level
	for ( INT i=0; i<Level->Actors.Num(); i++ ) 
	{
		GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Building Paths") );

		ANavigationPoint *Nav = (ANavigationPoint *)(Level->Actors(i));
		if ( Nav && (Nav->IsA(AInventorySpot::StaticClass()) || Nav->IsA(APlayerStart::StaticClass())) )
		{
			debugf(TEXT("----------------------Starting From %s"), Nav->GetName());
			// check if this inventory has already been visited
			if ( !Nav->bEndPoint )
				testPathsFrom(Nav->Location); 
			else
				debugf(TEXT("%s already visited!"),Nav->GetName());
		}
	}

	// merge paths which are within 128 units of each other and can be merged
	Scout->SetCollisionSize(COMMONRADIUS, MINCOMMONHEIGHT);
	FCheckResult Hit(1.f);
	for ( INT i=0; i<Level->Actors.Num(); i++ )
	{
		GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Merging Paths") );

		ANavigationPoint *node = (ANavigationPoint *)(Level->Actors(i)); 
		if (node && !node->bDeleteMe && node->bAutoBuilt && node->IsA(ANavigationPoint::StaticClass()) )
			for (INT j=0; j<Level->Actors.Num(); j++)
			{
				ANavigationPoint *node2 = (ANavigationPoint *)(Level->Actors(j)); 
				if ( ValidNode(node, node2) 
					&& ((node->Location - node2->Location).SizeSquared() < 16384)
					&& !node->ProscribedPathTo(node2) )
				{
					Level->SingleLineCheck( Hit, node, node->Location, node2->Location, TRACE_World );
					if ( !Hit.Actor )
					{
						debugf(TEXT("Found potential merge pair %s and %s"),node->GetName(), node2->GetName());
						// see if scout can walk without jumping node to node2
						if ( TestReach(node->Location, node2->Location) ) 
						{
							//check if same reachability
							// see if there is an intermediate path
							INT bFoundDiff = 0;
							INT bMustNotAvg = 0;
							FVector AvgPos = 0.5 * (node->Location + node2->Location);
							for (INT k=0; k<Level->Actors.Num(); k++)
							{
								ANavigationPoint *node3 = (ANavigationPoint *)(Level->Actors(k)); 
								if ( ValidNode(node,node3) 
										&& (node3 != node2) && ((node->Location - node3->Location).SizeSquared() < MAXPATHDISTSQ)
										&& ((node2->Location - node3->Location).SizeSquared() < MAXPATHDISTSQ) )
								{
									Scout->SetCollisionSize(COMMONRADIUS, MINCOMMONHEIGHT);
									Level->SingleLineCheck( Hit, node, node->Location, node3->Location, TRACE_World );
									if ( !Hit.Actor )
									{
										if ( TestReach(node->Location, node3->Location) ) 
											bFoundDiff = 1;
										else
										{
											Scout->SetCollisionSize(MINCOMMONRADIUS, MINCOMMONHEIGHT);
											if ( TestReach(node->Location, node3->Location) ) 
												bFoundDiff = 1;
										}
									}
									if ( bFoundDiff && node2->bAutoBuilt && !bMustNotAvg )
									{
										Level->SingleLineCheck( Hit, node, AvgPos, node3->Location, TRACE_World );
										if ( !Hit.Actor && TestReach(AvgPos, node3->Location) ) 
											bFoundDiff = 0;
										if ( bFoundDiff )
											bMustNotAvg = 1;
									}

									if ( bFoundDiff )
									{
										Level->SingleLineCheck( Hit, node, node2->Location, node3->Location, TRACE_World );
										if ( !Hit.Actor && TestReach(node2->Location, node3->Location) ) 
											bFoundDiff = 0;
									}
									if ( bFoundDiff )
										break;
								}
							}
							if ( !bFoundDiff )
							{
								INT bBreakOut = 0;
								ANavigationPoint * Keeper;
								if ( node2->bAutoBuilt )
								{
									Keeper = node;
									debugf(TEXT("remove %s"),node2->GetName());
									Level->DestroyActor( node2 ); 
								}
								else
								{
									Keeper = node2;
									debugf(TEXT("remove %s"),node->GetName());
									Level->DestroyActor( node ); 
									bBreakOut = 1;
								}
								if ( !bMustNotAvg )
								{
									Keeper->Location = AvgPos;
									debugf(TEXT("Move %s to %f %f"),Keeper->GetName(), AvgPos.X, AvgPos.Y);
								}
								if ( bBreakOut )
									break;
							}
							Scout->SetCollisionSize(COMMONRADIUS, MINCOMMONHEIGHT);
						}
					}
				}
			}
	}

	//Now add intermediate paths
	// if two nodes are walk reachable, then make sure they are less than MAXPATHDIST apart
	// or add an intermediate path
	for ( INT i=0; i<Level->Actors.Num(); i++ )
	{
		GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Creating intermediate paths") );
	
		ANavigationPoint *node = (ANavigationPoint *)(Level->Actors(i)); 
		if (node && !node->bDeleteMe && node->IsA(ANavigationPoint::StaticClass()) && !node->bNoAutoConnect ) // && (!node->Base || !node->Base->IsA(ATerrainInfo::StaticClass())) )
			for (INT j=0; j<Level->Actors.Num(); j++)
			{
				ANavigationPoint *node2 = (ANavigationPoint *)(Level->Actors(j)); 
				if ( ValidNode(node,node2) // && (!node2->Base || !node2->Base->IsA(ATerrainInfo::StaticClass()))
					&& !node->ProscribedPathTo(node2) )
			{										
					Level->SingleLineCheck( Hit, node, node->Location, node2->Location, TRACE_World );
					if ( !Hit.Actor )
					{
						debugf(TEXT("Found potential distant pair %s (%f, %f) and %s (%f, %f)"),node->GetName(), node->Location.X, node->Location.Y, node2->GetName(), node2->Location.X, node2->Location.Y);
						// see if scout can walk without jumping node to node2
						Level->FarMoveActor(Scout, node->Location);
						Scout->Physics = PHYS_Walking;
						if ( Scout->pointReachable(node2->Location) ) 
						{
							// see if there is an intermediate path
							INT bFoundPath = 0;
							FLOAT TotalDist = (node->Location - node2->Location).Size();
							FLOAT TotalDistSq = TotalDist * TotalDist;

							for (INT k=0; k<Level->Actors.Num(); k++)
							{
								ANavigationPoint *node3 = (ANavigationPoint *)(Level->Actors(k)); 
								if (	ValidNode(node,node3) && (node3 != node2) 
										&& ((node->Location - node3->Location).SizeSquared() < TotalDistSq)
										&& ((node2->Location - node3->Location).SizeSquared() < TotalDistSq)
										&& !node3->ProscribedPathTo(node2) )
								{
									Level->SingleLineCheck( Hit, node, node->Location, node3->Location, TRACE_World );
									if ( !Hit.Actor )
									{
										Level->SingleLineCheck( Hit, node, node2->Location, node3->Location, TRACE_World );
										if ( !Hit.Actor )
										{
											FLOAT Dist13 = (node->Location - node3->Location).Size();
											FLOAT Dist32 = (node3->Location - node2->Location).Size();
										
											debugf(TEXT("Try %s Total %f versus %f + %f"),node3->GetName(),TotalDist, Dist13, Dist32);
											if ( Dist13 + Dist32 < 1.3 * TotalDist )
											{
												Level->FarMoveActor(Scout, node->Location);
												Scout->Physics = PHYS_Walking;
												if ( Scout->pointReachable(node3->Location) ) 
												{
													Level->FarMoveActor(Scout, node3->Location);
													Scout->Physics = PHYS_Walking;
													if ( Scout->pointReachable(node2->Location) )
													{
														debugf(TEXT("Found %s as intermediate"),node3->GetName());
														bFoundPath = 1;
														break;
													}
												}
											}
										}
									}
								}
							}
							// if not add a path
							if ( !bFoundPath && Level->FarMoveActor(Scout, 0.5 * (node->Location + node2->Location)) )
								newPath(Scout->Location);
						}
					}
				}
			}
	}

	//***************************
	int numpaths = 0;
	for ( INT i=0; i<Level->Actors.Num(); i++ ) 
	{
		GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Cleaning up") );

		AActor *Actor = Level->Actors(i);
		if (Actor )
		{
			if ( Actor->IsA(APawn::StaticClass()) )
				Actor->SetCollision(1, 1); //turn Pawn collision back on
			else if ( Actor->IsA(ANavigationPoint::StaticClass()) 
					&& ((ANavigationPoint *)Actor)->bAutoBuilt )
				numpaths++;
		}
	}

	GWarn->EndSlowTask();

	return numpaths; 
	unguard;
}

//newPath() 
//- add new pathnode to level at position spot
ANavigationPoint* FPathBuilder::newPath(FVector spot)
{
	guard(FPathBuilder::newPath);
	
	if (Scout->CollisionHeight < MINCOMMONHEIGHT) 
		spot.Z = spot.Z + MINCOMMONHEIGHT - Scout->CollisionHeight;
	UClass *pathClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("PathNode") );
	APathNode *addedPath = (APathNode *)Level->SpawnActor( pathClass, NAME_None, spot,FRotator(0,0,0),NULL,1 );
	if ( !addedPath )
	{
		debugf(TEXT("Failed to add path!"));
		return NULL;
	}
	debugf(TEXT("Added new path %s at %f %f"),addedPath->GetName(), addedPath->Location.X, addedPath->Location.Y);
	addedPath->bAutoBuilt = 1;
	return addedPath;
	unguard;
};

/*getScout()
Find the scout actor in the level. If none exists, add one.
*/ 
void FPathBuilder::getScout()
{
	guard(FPathBuilder::getScout);
	Scout = NULL;
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor *Actor = Level->Actors(i); 
		Scout = Cast<AScout>(Actor);
		if ( Scout )
			break;
	}
	if( !Scout )
	{
		UClass *scoutClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("Scout") );
		Scout = Cast<AScout>(Level->SpawnActor( scoutClass ));
		Scout->Controller = (AController *)Level->SpawnActor(FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("AIController")));
	}
	Scout->SetCollision(1,1);
	Scout->bCollideWorld = 1;
	Scout->SetZone( 1,1 );
	Scout->PhysicsVolume = Scout->Level->GetDefaultPhysicsVolume();
	Scout->SetVolumes();

	return;
	unguard;
}

/* Find a starting spot for Scout to perform perambulation
*/
int AScout::findStart(FVector start)
{
	guard(AScout::findStart);
	
	if (GetLevel()->FarMoveActor(this, start)) //move Scout to starting point
	{
		//slide down to floor
		FCheckResult Hit(1.f);
		FVector Down = FVector(0,0,-100);
		Hit.Normal.Z = 0.f;
		INT iters = 0;
		while (Hit.Normal.Z < UCONST_MINFLOORZ)
		{
			GetLevel()->MoveActor(this, Down, Rotation, Hit, 1,1);
			if ( (Hit.Time < 1.f) && (Hit.Normal.Z < UCONST_MINFLOORZ) ) 
			{
				//adjust and try again
				FVector OldHitNormal = Hit.Normal;
				FVector Delta = (Down - Hit.Normal * (Down | Hit.Normal)) * (1.f - Hit.Time);
				if( (Delta | Down) >= 0 )
				{
					GetLevel()->MoveActor(this, Delta, Rotation, Hit, 1,1);
					if ((Hit.Time < 1.f) && (Hit.Normal.Z < UCONST_MINFLOORZ))
					{
						FVector downDir = Down.SafeNormal();
						TwoWallAdjust(downDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
						GetLevel()->MoveActor(this, Delta, Rotation, Hit, 1,1);
					}
				}
			}
			iters++;
			if (iters >= 10)
			{
				debugf(NAME_DevPath,TEXT("No valid start found"));
				return 0;
			}
		}
		//debugf(NAME_DevPath,TEXT("scout placed on valid floor"));
		return 1;
 	}

	//debugf(NAME_DevPath,TEXT("Scout didn't fit"));
	return 0;
	unguard;
}

void FPathBuilder::testPathsFrom(FVector start)
{
	guard(FPathBuilder::testPathsFrom);
	
	if ( (!Scout->findStart(start) 
		|| (Abs(Scout->Location.Z - start.Z) > Scout->CollisionHeight))
		&& !Scout->findStart(start + FVector(0,0,20)) )
		return;

	// do a walk from this start, following wall.
	// make sure there is always a valid path anchor
	testPathwithRadius(start, MAXCOMMONRADIUS);
	testPathwithRadius(start, COMMONRADIUS);
	testPathwithRadius(start, MINCOMMONRADIUS);

	unguard;
}

void FPathBuilder::testPathwithRadius(FVector start, FLOAT R)
{
	guard(FPathBuilder::testPathwithRadius);

	Scout->SetCollisionSize(R, MINCOMMONHEIGHT);

	FVector StartDir(1,0,0);
	Pass2From(start, StartDir, 1);
	Pass2From(start, StartDir, -1);

	StartDir = FVector(0,1,0);
	Pass2From(start, StartDir, 1);
	Pass2From(start, StartDir, -1);

	StartDir = FVector(-1,0,0);
	Pass2From(start, StartDir, 1);
	Pass2From(start, StartDir, -1);

	StartDir = FVector(0,-1,0);
	Pass2From(start, StartDir, 1);
	Pass2From(start, StartDir, -1);

	unguard;
}

void FPathBuilder::Pass2From(FVector start, FVector moveDirection, FLOAT TurnDir)
{
	guard(FPathBuilder::Pass2From);

	debugf(TEXT("WALK WITH COLLISION SIZE %f %f"),Scout->CollisionRadius, Scout->CollisionHeight);
	for (INT i=0; i<Level->Actors.Num(); i++) 
	{
		ANavigationPoint *Nav = Cast<ANavigationPoint>(Level->Actors(i));
		if ( Nav && !Nav->bDeleteMe )
		{
			Nav->visitedWeight = 1;
			Nav->bEndPoint = 0;
		}
	}

	// find a wall
	INT stillmoving = 1;
	FCheckResult Hit(1.f);
	while (stillmoving == 1)
		stillmoving = TestWalk(moveDirection * 16.f, Hit, MINMOVETHRESHOLD); 

	FVector BlockNormal = -1 * moveDirection;
	FindBlockingNormal(BlockNormal);
	moveDirection = FVector(BlockNormal.Y, -1 * BlockNormal.X, 0); 

	// find an anchor
	ANavigationPoint *Anchor = NULL;
	FLOAT BestDistSq = MAXPATHDISTSQ;
	for ( INT i=0; i<Level->Actors.Num(); i++ ) 
	{
		ANavigationPoint *Nav = Cast<ANavigationPoint>(Level->Actors(i));
		if ( Nav && !Nav->bDeleteMe && ((Nav->Location - Scout->Location).SizeSquared() < BestDistSq) )
		{
			FVector RealLoc = Scout->Location;
			if ( TestReach(Nav->Location,RealLoc) )
			{
				debugf(TEXT("----------------------Anchor %s"), Nav->GetName());
				Anchor = Nav;
				Anchor->Velocity = moveDirection;
				Anchor->bEndPoint = 1;
				BestDistSq = (Anchor->Location - Scout->Location).SizeSquared();
			}
			Level->FarMoveActor(Scout, RealLoc);
		}
	}

	// follow wall - if turn right, look for new anchor, 
	// if turn left, look for new anchor, or keep checking old
	// if I find two anchors which are marked in a row (bEndPoint == 1), I'm done
	INT LoopComplete = 0;
	INT NumSteps = 0;
	INT TotalSteps = 0;
	FVector TestLoc = Scout->Location;
	stillmoving = 1;
	INT RightTurns = 0;

	while ( (LoopComplete <2) && (NumSteps < 1000) )
	{
		if ( (TestLoc - Scout->Location).SizeSquared() > 40000 )
		{
			NumSteps = 0;
			TestLoc = Scout->Location;
		}
		else
			NumSteps++;
		TotalSteps++;
		if ( TotalSteps > 200 )
		{
			debugf(TEXT("Total steps out of bounds"));
			NumSteps = 2000;
		}
		FVector OldPos = Scout->Location;
		// try right turn
		INT tryturn = TestWalk(FVector(-1 * TurnDir * moveDirection.Y, TurnDir * moveDirection.X, 0) * 16, Hit, 12);
		if ( tryturn == 1 )
			stillmoving = 1;
		else
			stillmoving = TestWalk(moveDirection * 16.f, Hit, MINMOVETHRESHOLD); 
		//debugf(TEXT("Now at %f %f"),Scout->Location.X, Scout->Location.Y);

		// check good floor
		Level->SingleLineCheck(Hit, Scout, Scout->Location - FVector(0,0,(Scout->CollisionHeight + UCONST_MAXSTEPHEIGHT + 4.f)) , Scout->Location, TRACE_World, FVector(16,16,1));

		if( Anchor && Hit.Time<1.f )
		{
			FVector RealLoc = Scout->Location;
			if ( !TestReach(Anchor->Location,RealLoc) )
			{
				// find new anchor or place one
				//debugf(TEXT("ANCHOR NO LONGER VALID"));
				ANavigationPoint *OldAnchor = Anchor;
				ANavigationPoint *ClosestPath = NULL;
				FLOAT ClosestDistSq = 65536.f;
				BestDistSq = 65536.f;
				for (INT i=0; i<Level->Actors.Num(); i++) 
				{
					ANavigationPoint *Nav = (ANavigationPoint *)(Level->Actors(i));
					if ( Nav && !Nav->bDeleteMe && Nav->IsA(ANavigationPoint::StaticClass()) )
					{
						FLOAT NewDistSq = (Nav->Location - Scout->Location).SizeSquared();

						if ( NewDistSq < BestDistSq )
						{
							//debugf(TEXT("Try %s as anchor"),Nav->GetName());
							if ( TestReach(Nav->Location,RealLoc) )
							{
								//if ( OldAnchor )
								//	debugf(TEXT("Can reach it - try from %s"), OldAnchor->GetName());
								if ( NewDistSq < ClosestDistSq )
								{
									ClosestDistSq = NewDistSq;
									ClosestPath = Nav;
								}
								if ( TestReach(OldAnchor->Location, Nav->Location) )
								{
									//debugf(TEXT("Reached new anchor again"));
									if ( TestReach(OldPos,Nav->Location) )
									{
										debugf(TEXT("---------------------- New Anchor %s"), Nav->GetName());
										Anchor = Nav;
										if ( Anchor->bEndPoint && ((Anchor->Velocity | moveDirection) > 0.9) )
											LoopComplete++;
										else
											LoopComplete = 0;
										Anchor->Velocity = moveDirection;
										Anchor->bEndPoint = 1;
										
										Level->FarMoveActor(Scout, RealLoc);
										BestDistSq = (Anchor->Location - Scout->Location).SizeSquared();
									}
								}
							}
							Level->FarMoveActor(Scout, RealLoc);
						}
					}
				}

				if ( Anchor == OldAnchor )
				{
					INT bAdjusted = 0;
					debugf(TEXT("didn't find new anchor"));
					if ( ClosestDistSq < 2500.f )
					{
						INT bNew = 1;
						INT bAvg = 1;
						FVector AvgPos = 0.5 * (OldPos + ClosestPath->Location);
						//look to see if can reach same paths as old spot in new
						debugf(TEXT("Check closest path %s"),ClosestPath->GetName());
						Level->FarMoveActor(Scout, ClosestPath->Location);
						for (INT i=0; i<Level->Actors.Num(); i++) 
						{
							ANavigationPoint *Nav = (ANavigationPoint *)(Level->Actors(i));
							if ( ValidNode(ClosestPath, Nav)
								&& ((ClosestPath->Location - Nav->Location).SizeSquared() < MAXPATHDISTSQ) )
							{
								Scout->Physics = PHYS_Walking;
								if ( Scout->pointReachable(Nav->Location) )
								{
									if ( bNew )
									{
										bNew = TestReach(OldPos,Nav->Location);
										Level->FarMoveActor(Scout, ClosestPath->Location);
									}
									if ( bAvg )
									{
										bAvg = TestReach(AvgPos,Nav->Location);
										Level->FarMoveActor(Scout, ClosestPath->Location);
									}
									if ( !bNew && !bAvg )
										break;
								}
							}
						}

						if ( bAvg )
						{
							Anchor = ClosestPath;
							Anchor->Location = AvgPos;
							bAdjusted = 1;
						}
						else if ( bNew )
						{
							Anchor = ClosestPath;
							Anchor->Location = OldPos;
							bAdjusted = 1;
						}
						else if ( ClosestDistSq < 1000.f )
							bAdjusted = 1;
					}
					FCheckResult Hit(1.f);
					if ( !bAdjusted )
					{
						Anchor = newPath(OldPos);
						if ( !Anchor )
							return;

						if ( (OldAnchor->Location - Anchor->Location).SizeSquared() < 1000.f )
						{
							Level->DestroyActor( Anchor ); 
							Anchor = OldAnchor;
						}
						else if ( ClosestPath && ((ClosestPath->Location - Anchor->Location).SizeSquared() < 1000.f) )
						{
							Level->DestroyActor( Anchor ); 
							Anchor = ClosestPath;
						}
						else
						{
							for ( INT i=0; i<Level->Actors.Num(); i++ ) 
							{
								AActor *Actor = Level->Actors(i);
								if ( Actor && (Actor != Anchor) && !Actor->bDeleteMe )
								{
									ANavigationPoint *Nav = Cast<ANavigationPoint>(Actor);
									if ( Nav && ((Anchor->Location - Nav->Location).SizeSquared() < 1000.f) )
									{
										Level->SingleLineCheck( Hit, Anchor, Anchor->Location, Nav->Location, TRACE_World );
										if ( !Hit.Actor )
										{
											Level->DestroyActor( Anchor ); 
											Anchor = Nav;
											break;
										}
									}
								}
							}
						}
						Anchor->visitedWeight++;
						if ( Anchor->visitedWeight > 10 )
							NumSteps = 5000;
						Anchor->Velocity = moveDirection;
						Anchor->bEndPoint = 1;
						debugf(TEXT("---------------------- ADD Anchor %s at %f %f"), Anchor->GetName(), Anchor->Location.X, Anchor->Location.Y);
					}

					if ( Anchor != OldAnchor )
						TotalSteps = 0;
				}
			}
			Level->FarMoveActor(Scout, RealLoc);
		}
		if ( tryturn == 1 )
		{
			if ( !Anchor && (RightTurns == 0) )
				Anchor = newPath(OldPos);

			RightTurns++;
			if ( RightTurns > 100 )
				 NumSteps = 3000;
			if ( RightTurns < 3)
				moveDirection = FVector(-1 * TurnDir * moveDirection.Y, TurnDir * moveDirection.X, 0);
			//debugf(TEXT("turn right dir %f %f"), moveDirection.X, moveDirection.Y);
		}
		else if ( stillmoving != 1 )
		{
			RightTurns = 4;
			BlockNormal = -1 * moveDirection;
			FindBlockingNormal(BlockNormal);
			moveDirection = FVector(-1 * TurnDir * BlockNormal.Y, TurnDir * BlockNormal.X, 0); 
			//debugf(TEXT("turn left dir %f %f"), moveDirection.X, moveDirection.Y);
		}
		else RightTurns = 0;
	}
	debugf(TEXT("Num steps %d"),NumSteps);

	unguard;
}

INT FPathBuilder::ValidNode(ANavigationPoint *node, AActor *node2)
{
	guard(FPathBuilder::TestReach);
	return (node2 && (node2 != node) && !node2->bDeleteMe 
		&& node2->IsA(ANavigationPoint::StaticClass()) 
		&& !node2->IsA(ALiftCenter::StaticClass()) );
	unguard;
}

INT FPathBuilder::TestReach(FVector Start, FVector End)
{
	guard(FPathBuilder::TestReach);

	FVector OldLoc = Scout->Location;
	Level->FarMoveActor(Scout, Start);
	Scout->Physics = PHYS_Walking;
	INT bResult = Scout->pointReachable(End);
	Level->FarMoveActor(Scout, OldLoc,0,1);
	return bResult;
	unguard;
}

INT FPathBuilder::TestWalk(FVector WalkDir, FCheckResult Hit, FLOAT Threshold)
{
	guard(FPathBuilder::TestWalk);

	FVector OldLoc = Scout->Location;
	ETestMoveResult Result = Scout->walkMove(WalkDir, Hit, NULL, Threshold);

	if ( Result == TESTMOVE_Moved )
	{
		Level->SingleLineCheck(Hit, Scout, Scout->Location - FVector(0,0,(Scout->CollisionHeight + UCONST_MAXSTEPHEIGHT + 4.f)) , Scout->Location, TRACE_World, FVector(16,16,1));
		if( Hit.Time < 1.f )
			return Result;
		Level->FarMoveActor(Scout,OldLoc,0,1);
		return -1;
	}

	return Result;
	unguard;
}

void FPathBuilder::FindBlockingNormal(FVector &BlockNormal)
{
	guard(FPathBuilder::FindBlockingNormal);

	FCheckResult Hit(1.f);
	Level->SingleLineCheck(Hit, Scout, Scout->Location - BlockNormal * 16, Scout->Location, TRACE_World, Scout->GetCylinderExtent());
	if( Hit.Time < 1.f )
	{
		BlockNormal = Hit.Normal;
		return;
	}

	// find ledge
	FVector Destn = Scout->Location - BlockNormal * 16;
	FVector TestDown = FVector(0,0, -1 * UCONST_MAXSTEPHEIGHT);
	Level->SingleLineCheck(Hit, Scout, Destn + TestDown, Destn, TRACE_World, Scout->GetCylinderExtent());
	if( Hit.Time < 1.f )
	{
		//debugf(TEXT("Found landing when looking for ledge"));
		return;
	}
	Level->SingleLineCheck(Hit, Scout, Scout->Location + TestDown, Destn + TestDown, TRACE_World, Scout->GetCylinderExtent());

	if( Hit.Time < 1.f )
		BlockNormal = Hit.Normal;
	
	unguard;
}

