//=============================================================================
// AxPickupBase - implementation
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================
#include "EnginePrivate.h"


IMPLEMENT_CLASS(AxPickUpBase);

#define HACKRAD 36 /* MERGE_HACK*/

INT AxPickUpBase::AddMyMarker(AActor *S)
{
	guard(AxPickUpBase::AddMyMarker);

	AScout* Scout = Cast<AScout>(S);
	if ( !Scout )
		return 0;

	if ( !Scout->findStart(Location) || (Abs(Scout->Location.Z - Location.Z) > Scout->CollisionHeight) )
		GetLevel()->FarMoveActor(Scout, Location + FVector(0,0,HACKRAD /* MERGE_HACK*/ - CollisionHeight), 1, 1);
	UClass *pathClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("InventorySpot") );	
    myMarker = Cast<AInventorySpot>(GetLevel()->SpawnActor(pathClass, NAME_None, Scout->Location));	
	if ( myMarker )
		myMarker->myPickupBase = this;
	return 1;
	unguard;
}

void AxPickUpBase::ClearMarker()
{
	guard(AxPickUpBase::ClearMarker);

	if ( myMarker )
		myMarker->myPickupBase = NULL;
	myMarker = NULL;
	unguard;
}

