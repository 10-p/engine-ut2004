/*============================================================================
	Karma Farfield
    
    - Code for generating/handling pairs of McdModels as startand stop 
	  overlapping.
============================================================================*/

#include "EnginePrivate.h"

#ifdef WITH_KARMA
#include "McdBatch.h"

#define DISPLAY_HELLO	(0)
#define DISPLAY_GOODBYE	(0)

#define MODELGEOM_NOT_NULL(m) (McdGeometryGetTypeId(McdModelGetGeometry(m)) != kMcdGeometryTypeNull)

// This should give the same key regardless of model order.
KModelPairType KModelsToKey(McdModelID m1, McdModelID m2)
{
	guardSlow(KModelsToKey);

#if PLATFORM_32BITS
	check(sizeof(QWORD) == 2 * sizeof(McdModelID));
#elif PLATFORM_64BITS
	check(sizeof(QWORD) == sizeof (PTRINT));
#endif

	// Make sure m1 is the 'lower' pointer.
	if(m2 > m1)
	{
		McdModelID tmp = m2;
		m2 = m1;
		m1 = tmp;
	}

	KModelPairType Key;

#if PLATFORM_32BITS
    // Commented code below is an Aliasing Violation!
	//McdModelID* models = (McdModelID*)&Key;
	//models[0] = m1;
	//models[1] = m2;
    Key = (((QWORD) m1) << 32) | ((QWORD) m2);
#elif PLATFORM_64BITS
    // I'm a gambling man. I'll bet that the loworder bits are still unique
    //  enough for this key generation...yikes.  --ryan.
    Key = (QWORD) (((((PTRINT) m1) & 0xFFFFFFFF) << 32) | (((PTRINT) m2) & 0xFFFFFFFF));
#else
    #error Please define your platform.
#endif

	return Key;

	unguardSlow;
}

//////////////////////////////////// KUPDATECONTACTS UTILS //////////////////////////////////////////////////////////

// Get any collision data out of this actor. That might be none though!
static void GetModels(AActor* actor, McdModelID &model, USkeletalMeshInstance* &inst)
{
	guard(GetModels);

	inst = NULL;
	model = NULL;

	model = actor->getKModel();
	if(model)
		return;

	if(actor->Mesh && actor->Mesh->IsA(USkeletalMesh::StaticClass()))
	{
		USkeletalMesh* skelMesh = Cast<USkeletalMesh>(actor->Mesh);
		inst = Cast<USkeletalMeshInstance>(skelMesh->MeshGetInstance(actor));
	}
	if(inst && !inst->KSkelIsInitialised)
		inst = NULL;

	unguard;
}

static UBOOL CheckModelOverlap(McdModelID m1, McdModelID m2)
{
	guard(CheckModelOverlap);

	MeVector3 m1Min, m1Max;
	McdModelGetAABB(m1, m1Min, m1Max);

	MeVector3 m2Min, m2Max;
	McdModelGetAABB(m2, m2Min, m2Max);

	if(	m2Min[0] > m1Max[0] || m1Min[0] > m2Max[0] ||
		m2Min[1] > m1Max[1] || m1Min[1] > m2Max[1] ||
		m2Min[2] > m1Max[2] || m1Min[2] > m2Max[2] )
		return 0;
	else
		return 1;

	unguard;
}

static UBOOL ActorsShouldCollide(AActor* a1, AActor* a2)
{
	guard(ActorsShouldCollide);

	// Currently, dont let ragdolls collide with each other or regular karma stuff.
	if((a1->Physics == PHYS_KarmaRagDoll || a2->Physics == PHYS_KarmaRagDoll))
	{
		// the other thing is a Karma object.
		if(a1->Physics == PHYS_Karma || a2->Physics == PHYS_Karma)
			return 0;

		// its ragdoll-ragdoll (a1 & a2 are not the same)
		check(a1 != a2);
		if(a1->Physics == PHYS_KarmaRagDoll && a2->Physics == PHYS_KarmaRagDoll)
			return 0;
	}

	// Dont allow things with  different bKDoubleTickRate to collide. 
	// JTODO: Can we fix this? Set bKDoubleTickRate on the fly?
	// NB. Should always have KParams at this point.
	UKarmaParams* kparams1 = Cast<UKarmaParams>(a1->KParams);
	UKarmaParams* kparams2 = Cast<UKarmaParams>(a2->KParams);
	if(kparams1 && kparams2 && kparams1->bKDoubleTickRate != kparams1->bKDoubleTickRate)
	{
		debugf(TEXT("ActorsShouldCollide: Allowed collion between actors with different bKDoubleTickRate"));
		return 0;
	}

	ABlockingVolume* BV = NULL;
	AActor* Other = NULL;


	if( (BV = Cast<ABlockingVolume>(a1) ) != NULL )
		Other = a2;
	else if( (BV = Cast<ABlockingVolume>(a2) ) != NULL )
		Other = a1;

	if(BV && BV->bClassBlocker)
	{
		for(INT i=0; i<BV->BlockedClasses.Num(); i++)
		{
			if( Other->IsA( BV->BlockedClasses(i) ) )
				return 1;
		}

		return 0;
	}

	return 1;

	unguard;
}

// For generating model pairs betweeen different parts of the same skeletal asset.
// Will use the KSkelDisableTable to ignore certain pairs of models.
static void GenerateSelfOverlapPairs(USkeletalMeshInstance* inst, 
									 TArray<McdModelID>& PairM1, 
									 TArray<McdModelID>& PairM2 )
{
	guard(GenerateSelfOverlapPairs);

	for(INT i=inst->KPhysRootIndex; i <= inst->KPhysLastIndex-1; i++)
	{
		McdModelID model1 = inst->KSkelModels(i);
		if(model1 && MODELGEOM_NOT_NULL(model1))
		{
			for(INT j=i+1; j <= inst->KPhysLastIndex; j++)
			{
				McdModelID model2 = inst->KSkelModels(j);
				if(model2 && MODELGEOM_NOT_NULL(model2))
				{
					// If we dont already have this pair, and it isn't disabled, and the boxes overlap.
					// Note for intra-skeleton model pairs we use the skel instance KSkelDisableTable, 
					// not the level KDisableTable.
					KModelPairType Key = KModelsToKey(model1, model2);
					if( !inst->KSkelDisableTable.Find(Key) && CheckModelOverlap(model1, model2) )
					{
						PairM1.AddItem(model1);
						PairM2.AddItem(model2);
					}
				}
			}
		}
	}

	unguard;
}

// Given the two 'things' (either models of skeletal instances), generate all pairs of models
// whose bounding boxes currently overlap.
static void GenerateOverlapPairs(McdModelID model1, USkeletalMeshInstance* inst1, 
								 McdModelID model2, USkeletalMeshInstance* inst2,
								 TArray<McdModelID>& PairM1, TArray<McdModelID>& PairM2, 
								 UBOOL checkOverlap, TMap<KModelPairType, UBOOL>* DisableTable )
{
	guard(GenerateOverlapPairs);

	// If we have model-skel, make sure the model is the first one.
	if(inst1 && model2)
	{
		McdModelID tmpM = model1;
		USkeletalMeshInstance* tmpI = inst1;

		model1 = model2; inst1 = inst2;
		model2 = tmpM; inst2 = tmpI;
	}

	if(model1 && MODELGEOM_NOT_NULL(model1) && model2 && MODELGEOM_NOT_NULL(model2)) // MODEL-MODEL
	{
		// If we dont already have this pair, and this pair isn't disabled.
		// Dont do box check, because the ActorOverlapCheck has already done that.
		KModelPairType Key = KModelsToKey(model1, model2);
		if(!DisableTable->Find(Key))
		{
			PairM1.AddItem(model1);
			PairM2.AddItem(model2);
		}
	}
	else if(model1 && MODELGEOM_NOT_NULL(model1) && inst2) // MODEL-SKEL
	{
		for(INT j=inst2->KPhysRootIndex; j <= inst2->KPhysLastIndex; j++)
		{
			McdModelID skelModel2 = inst2->KSkelModels(j);
			if(skelModel2 && MODELGEOM_NOT_NULL(skelModel2))
			{
				// If we dont already have this pair, and this pair isn't disabled, and the boxes overlap (or we're not checking)
				KModelPairType Key = KModelsToKey(model1, skelModel2);
				if( !DisableTable->Find(Key) && (!checkOverlap || CheckModelOverlap(model1, skelModel2)) )
				{
					PairM1.AddItem(model1);
					PairM2.AddItem(skelModel2);
				}
			}
		}
	}
	else if(inst1 && inst2) // SKEL-SKEL
	{
		for(INT j=inst1->KPhysRootIndex; j <= inst1->KPhysLastIndex; j++)
		{
			McdModelID skelModel1 = inst1->KSkelModels(j);
			if(skelModel1 && MODELGEOM_NOT_NULL(skelModel1))
			{
				for(INT k=inst2->KPhysRootIndex; k <= inst2->KPhysLastIndex; k++)
				{
					McdModelID skelModel2 = inst2->KSkelModels(k);
					if(skelModel2 && MODELGEOM_NOT_NULL(skelModel2))
					{
						// If we dont already have this pair, and the boxes overlap (or we're not checking)
						KModelPairType Key = KModelsToKey(skelModel1, skelModel2);
						if( !DisableTable->Find(Key) && (!checkOverlap || CheckModelOverlap(skelModel1, skelModel2)) )
						{
							PairM1.AddItem(skelModel1);
							PairM2.AddItem(skelModel2);
						}
					}
				}
			}
		}
	}

	unguard;
}

static void UpdateModelPairs(TArray<McdModelID>& PairM1, TArray<McdModelID>& PairM2, 
							 TMap<KModelPairType, McdModelPairID>* OverlapPairs, TMap<KModelPairType, UBOOL>* ActivePairs,
							 McdModelPairContainer* pairContainer, ULevel* level)
{
	guard(UpdateModelPairs);

	for(INT i=0; i<PairM1.Num(); i++)
	{
		check(PairM1(i) && PairM2(i));

		// We dont want to remove (goodbye) this pair, so remove it from models goodbye-pending lists.
		// Note - one of these arrays will be empty! But its tricky to keep track of which one...
		KarmaModelUserData* d1 = (KarmaModelUserData*)McdModelGetUserData(PairM1(i));
		d1->GoodbyeModels.RemoveItem(PairM2(i));

		KarmaModelUserData* d2 = (KarmaModelUserData*)McdModelGetUserData(PairM2(i));
		d2->GoodbyeModels.RemoveItem(PairM1(i));


		// If this pair has already been found this frame, don't create it again.
		KModelPairType Key = KModelsToKey(PairM1(i), PairM2(i));
		if(ActivePairs->Find(Key))
			continue;

		// Look up this pair of models in the map of all model pairs (active or not)
		McdModelPairID pair = 0;
		McdModelPairID* pairPtr = OverlapPairs->Find(Key);

		// If there was no pair already, create a new model pair and hello it.
		if(!pairPtr)
		{
			pair = McdModelPairCreate(PairM1(i), PairM2(i));
			KHelloModelPair(pair, level);
		}
		else
		{
			pair = *pairPtr; // Otherwise, use existing pair.
		}

		// Want to generate new contacts for this pair.
		McdModelPairContainerInsertStayingPair(pairContainer, pair);

		// Make sure we dont generate this pair again!
		ActivePairs->Set(Key, 0);
	}

	unguard;
}

// Initially, all pairs of this model are candidates for goodbyeing.
// Every time we find a pair, we remove it from the pending-goodbye list.
void static SetupModelGoodbyeList(McdModelID model, USkeletalMeshInstance* inst)
{
	check(model || inst);
	check(!(model && inst));

	if(model)
	{
		KarmaModelUserData* d = (KarmaModelUserData*)McdModelGetUserData(model);
		d->GoodbyeModels.Empty();
		d->GoodbyeModels = d->OverlapModels;		
	}
	else
	{
		for(INT i=inst->KPhysRootIndex; i <= inst->KPhysLastIndex; i++)
		{
			McdModelID skelModel = inst->KSkelModels(i);
			if(skelModel)
				SetupModelGoodbyeList(skelModel, 0);
		}
	}
}

// This is called as the last thing for a contact generating model.
// It removes any pairs that are no longer pertinant.
void static ProcessModelGoodbyeList(McdModelID model, USkeletalMeshInstance* inst, ULevel* level)
{
	check(model || inst);
	check(!(model && inst));

	if(model)
	{
		KarmaModelUserData* d = (KarmaModelUserData*)McdModelGetUserData(model);
		for(INT i=0; i<d->GoodbyeModels.Num(); i++)
		{
			KGoodbyePair(model, d->GoodbyeModels(i), level);
		}
	}
	else
	{
		for(INT i=inst->KPhysRootIndex; i <= inst->KPhysLastIndex; i++)
		{
			McdModelID skelModel = inst->KSkelModels(i);
			if(skelModel)
				ProcessModelGoodbyeList(skelModel, 0, level);
		}
	}
}

///////////////////////////////////////// KUPDATECONTACTS //////////////////////////////////////////////////////////////////

// This is the main function which will update/add/remove contacts from the Karma simulation.
// It first figures out all hello(new), staying and goodbye (just finished) McdModel pairs.
// Then calls the nearfield contact-generation tests, and updates the MdtWorld accordingly.
// The bool indicates if we are generating contacts for fixed or variable rate actors.
void KUpdateContacts(TArray<AActor*> &actors, ULevel* level, UBOOL bDoubleRateActors)
{
	guard(KUpdateContacts);

	if(!level->Hash)
		return;

	//debugf( TEXT("-------------- BEGIN CONTACT GEN ---------------") );

	FMemMark Mark(GMem);

	// Keep a lookup of pairs we already have, to prevent adding them twice.
	TMap<KModelPairType, UBOOL> ActivePairs;

	// Empty the model pair container
	McdModelPairContainerReset(KGData->filterPairs);

	// I am assuming that I dont need to update the models. That should always be done before things are upt back 
	// into the Hash (including for each part of a skeletal thing).

	// We iterate over the actors in the world. We will only query for an actor if it is physical,
	// and if it has bBlockKarma turned on. This will automatically ignore world-world pairs etc,
	// and karma things with no collision.
	for(INT i=0; i<actors.Num(); i++)
	{
		AActor* actor = actors(i);
		check(actor);
		check(actor->KParams);

		// Only update contacts for things we are stepping.
		UKarmaParams* kparams = Cast<UKarmaParams>(actor->KParams);
		check(kparams);
		if(kparams->bKDoubleTickRate != bDoubleRateActors)
			continue;

		// Reset 'touching level' flag. Set again in KPerContactCB if we are.
		kparams->bContactingLevel = 0;

		// Do any other pre-contact gen stuff (reset flags set inside contact callback etc.)
		actor->preContactUpdate();

		//debugf( TEXT("ContactGen: %x %s"), actor, actor->GetName() );

		if(actor->Physics != PHYS_Karma && actor->Physics != PHYS_KarmaRagDoll)
		{
			debugf( TEXT("KUpdateContacts: Actor without Karma physics mode. Name: %x %s Physics: %d"), actor, actor->GetName(), (INT)actor->Physics );
			check(actor->Physics == PHYS_Karma || actor->Physics == PHYS_KarmaRagDoll);
		}

		check(!actor->bDeleteMe);
		//check(actor->Physics == PHYS_Karma || actor->Physics == PHYS_KarmaRagDoll);
		check(actor->bBlockKarma);
		check(actor->bCollideActors);

		USkeletalMeshInstance* inst1 = NULL;
		McdModelID model1 = NULL;

		// Get the collision info for this actor (model or skel instance)
		GetModels(actor, model1, inst1);
		if(!model1 && !inst1)
			continue;
		check(!(model1 && inst1)); // Check we dont have a model AND a skel instance!


		// This is the model which has moved, so we might want to goodbye some pairs its involved with.
		// We assume we want to goodbye all the pairs. Then, for each pair we find we remove it from the
		// 'goodbye pending' list. Then we goodbye any remaining pairs before moving on to the next
		// moving model.
		SetupModelGoodbyeList(model1, inst1);

		// Find the actors (with bBlockKarma) which overlap this actors bounding box.
		FCheckResult* first = NULL;
		if( actor->IsInOctree() )
		{
			FBox box = actor->OctreeBox;
			first = level->Hash->ActorOverlapCheck(GMem, actor, &box, 1);
		}

		FCheckResult* overlap;
		TArray<McdModelID> PairM1, PairM2;
		for( overlap = first; overlap!=NULL; overlap=overlap->GetNext() )
		{
			AActor* overActor = overlap->Actor;
			check(overActor);
			check(overActor->bBlockKarma);
			//check(overActor->KParams);
			check(!overActor->bDeleteMe);

			//debugf(TEXT("Consider: %x %s - %x %s"), actor, actor->GetName(), overActor, overActor->GetName() );

			// Check if this pair of actors should happen at all.
			if( !overActor->KParams || actor == overActor || !ActorsShouldCollide(actor, overActor) )
				continue;

			USkeletalMeshInstance* inst2 = NULL;
			McdModelID model2 = NULL;

			GetModels(overActor, model2, inst2);
			if(!model2 && !inst2)
				continue;
			check(!(model2 && inst2));

			// Now generate the resulting set of overlapping models, using AABB test to reject further.
			//debugf(TEXT("GeneratePairs: %x %s - %x %s"), actor, actor->GetName(), overActor, overActor->GetName() );
			GenerateOverlapPairs(model1, inst1, model2, inst2, PairM1, PairM2, 1, &level->KDisableTable);
			check(PairM1.Num() == PairM2.Num());
			
		} // FOR each overlapping actors

		// We also need to make sure there is a pair between this actor and the level model,
		// for BSP/Terrain collision via TriList. No need to bother with the overlap test.
		//debugf(TEXT("GeneratePairs (world): %s - WORLD"), actor->GetName() );
		GenerateOverlapPairs(model1, inst1, level->KLevelModel, 0, PairM1, PairM2, 0, &level->KDisableTable);

		// If this is a skeletal actor, we need to generate pairs of models between its parts.
		if(inst1)
			GenerateSelfOverlapPairs(inst1, PairM1, PairM2);

		check(PairM1.Num() == PairM2.Num());

		// Update model pair container from the set pairs we have found.
		UpdateModelPairs(PairM1, PairM2, &level->OverlapPairs, &ActivePairs, KGData->filterPairs, level);

		// Any pairs still left in the goodbye lists are dealt with now - they are no longer needed.
		ProcessModelGoodbyeList(model1, inst1, level);

	} // FOR each dynamics, bBlockKarma actor in world.

	Mark.Pop();

	// Only bother doing contact generation if we have some pairs!
	if(McdModelPairContainerGetHelloCount(KGData->filterPairs) + McdModelPairContainerGetStayingCount(KGData->filterPairs) > 0)
		KHandleCollisions(KGData->filterPairs, level);
	
	unguard;
}


/////////////////////////////////////// HELLO/GOODBYE //////////////////////////////////////////////////////

void KHelloModelPair(McdModelPairID pair, ULevel* level)
{
	guard(KHelloModelPair);

#if DISPLAY_HELLO
	AActor* a1 = KModelGetActor(pair->model1);
	AActor* a2 = KModelGetActor(pair->model2);
	debugf(TEXT("HELLO: %s (%x) - %s (%x)"), a1?a1->GetName():TEXT("None"), pair->model1, a2?a2->GetName():TEXT("None"), pair->model2);
#endif

	MeI32 key1 = McdModelGetSortKey(pair->model1);
	MeI32 key2 = McdModelGetSortKey(pair->model2);

	McdGeometryType g1 = McdGeometryGetTypeId(McdModelGetGeometry(pair->model1));
	McdGeometryType g2 = McdGeometryGetTypeId(McdModelGetGeometry(pair->model2));

	/* if the geometries are the same, rearrange them before the hello
	so that the one with the lower sort key is first. */

	if(g1==g2 && key2<key1)
	{
		McdModelID tmp = pair->model1;
		pair->model1 = pair->model2;
		pair->model2 = tmp;
	}

	McdHello(pair);

	// Add each model to the others list of overlapping models.
	KarmaModelUserData* d1 = (KarmaModelUserData*)McdModelGetUserData(pair->model1);
	check(d1->OverlapModels.FindItemIndex(pair->model2) == INDEX_NONE); // Check model is not already in array
	d1->OverlapModels.AddItem(pair->model2);

	KarmaModelUserData* d2 = (KarmaModelUserData*)McdModelGetUserData(pair->model2);
	check(d2->OverlapModels.FindItemIndex(pair->model1) == INDEX_NONE);
	d2->OverlapModels.AddItem(pair->model1);

	// Add this pair to the store of model pairs.
	KModelPairType Key = KModelsToKey(pair->model1, pair->model2);
	level->OverlapPairs.Set(Key, pair);

	unguard;
}


void KGoodbyeModelPair(McdModelPairID pair, ULevel* level)
{
	guard(KGoodbyeModelPair);

#if DISPLAY_GOODBYE
	AActor* a1 = KModelGetActor(pair->model1);
	AActor* a2 = KModelGetActor(pair->model2);
	debugf(TEXT("GOODBYE: %s (%x) - %s (%x)"), a1?a1->GetName():TEXT("None"), pair->model1, a2?a2->GetName():TEXT("None"), pair->model2);
#endif

#if 0
	if(bDoPairCheck)
	{
		for(INT i=KGData->filterPairs->helloFirst; i<KGData->filterPairs->stayingEnd; i++)
		{
			McdModelPair* mp = KGData->filterPairs->array[i];
			check(mp != pair);
		}
	}
#endif
	// Make sure pair is removed from set of overlapping pairs.
	KModelPairType Key = KModelsToKey(pair->model1, pair->model2);
	level->OverlapPairs.Remove(Key);

	// Clear up any dyanmic contacts going on.
	MdtContactGroupID g = (MdtContactGroupID)pair->responseData;
	if(g)
	{
		MdtContactGroupReset(g);
		MdtContactGroupDestroy(g);
		pair->responseData = 0;
	}

	// Do any Karma cleanup on model pair.
	McdGoodbye(pair);

	// Remove reference to each model in the others overlapping model array.
	KarmaModelUserData* d1 = (KarmaModelUserData*)McdModelGetUserData(pair->model1);
	check(d1->OverlapModels.FindItemIndex(pair->model2) != INDEX_NONE);
	d1->OverlapModels.RemoveItem(pair->model2);

	KarmaModelUserData* d2 = (KarmaModelUserData*)McdModelGetUserData(pair->model2);
	check(d2->OverlapModels.FindItemIndex(pair->model1) != INDEX_NONE);
	d2->OverlapModels.RemoveItem(pair->model1);

	// Finally, free the pair itself.
	McdModelPairDestroy(pair);

	unguard;
}


// Remove any pair between these two models.
// Also removes any reference to each other in each models list of overlapping models.
void KGoodbyePair(McdModelID model1, McdModelID model2, ULevel* level)
{
	guard(KGoodbyePair);

	KModelPairType Key = KModelsToKey(model1, model2);
	McdModelPairID* pair = level->OverlapPairs.Find(Key);

	// There is no pair between these models, do nothing
	if(!pair)
	{
#if 1
		// Check neither model thinks its overlapping the other.
		KarmaModelUserData* d1 = (KarmaModelUserData*)McdModelGetUserData(model1);
		check(d1->OverlapModels.FindItemIndex(model2) == INDEX_NONE);

		KarmaModelUserData* d2 = (KarmaModelUserData*)McdModelGetUserData(model2);		
		check(d2->OverlapModels.FindItemIndex(model1) == INDEX_NONE);
#endif
		return;
	}

	KGoodbyeModelPair(*pair, level);

	unguard;
}

// Call this when you are turning off collision or destroying a model. 
// It will find, goodbye and destroy any related McdModelPairs.
void KGoodbyeAffectedPairs(McdModelID model, ULevel* level)
{
	guard(KGoodbyeAffectedPairs);

	KarmaModelUserData* data = (KarmaModelUserData*)McdModelGetUserData(model);

	while( data->OverlapModels.Num() > 0)
	{
		McdModelID model2 = data->OverlapModels(0);
		KGoodbyePair(model, model2, level);
	}

	unguard;
}

// Util for feeding above. Gets any models out of the supplied actor and goodbye any affected pairs.
void KGoodbyeActorAffectedPairs(AActor* actor)
{
	McdModelID model;
	USkeletalMeshInstance* inst;

	GetModels(actor, model, inst);
	check( !(model && inst) );

	if(inst)
	{
		if(!inst->KSkelIsInitialised)
			return;

		for(INT i=0; i<inst->KSkelModels.Num(); i++)
		{
			McdModelID model = inst->KSkelModels(i);
			if(!model)
				continue;

			KGoodbyeAffectedPairs(model, actor->GetLevel() );
		}
	}
	else if(model)
		KGoodbyeAffectedPairs(model, actor->GetLevel() );
}

/////////////////////////////////////// PAIRWISE ENABLE/DISABLE /////////////////////////////////////////////////////////////

// Re-enable collision between these two models.
// (Collision is on by default)
void KEnablePairCollision(McdModelID model1, McdModelID model2, ULevel* level)
{
	guard(KEnablePairCollision);

	// Make key, and remove from the table. Will do nothing if its not there (ie. collision is already enabled).
	KModelPairType Key = KModelsToKey(model1, model2);
	level->KDisableTable.Remove(Key);

	unguard;
}

// Disable collision between these models.
// This also cleans up and modelpair/contact group already between these models.
void KDisablePairCollision(McdModelID m1, McdModelID m2, ULevel* level)
{
	guard(KEnablePairCollision);

	KModelPairType Key = KModelsToKey(m1, m2);

	// First see if we are already in the table. If so, do nothing.
	UBOOL* b = level->KDisableTable.Find(Key);
	if(b)
		return;

	// Add key to table.
	level->KDisableTable.Set(Key, 0);

	// We also want to remove any model pair between these two models.
	KGoodbyePair(m1, m2, level);

	unguard;
}

/* ******************** CUSTOM BRIDGE HANDLERS ***************************** */

static void ConvertCollisionContact(MdtBclContactParams* params, McdContact* colC, MdtContactID dynC, int swap)
{
	guardSlow(ConvertCollisionContact);

    if(swap)
        MdtContactSetNormal(dynC, -colC->normal[0], -colC->normal[1], -colC->normal[2]);
    else
        MdtContactSetNormal(dynC, colC->normal[0], colC->normal[1], colC->normal[2]);

    MdtContactSetPosition(dynC, colC->position[0], colC->position[1], colC->position[2]);
    MdtContactSetPenetration(dynC,-(colC->separation));
    MdtContactSetParams(dynC, params);

	unguardSlow;
}

static MdtContactGroupID CreateContactGroup(MdtWorldID w, McdModelPairID pair)
{
	guard(CreateContactGroup);

    MeI32 key1 = McdModelGetSortKey(pair->model1);
    MeI32 key2 = McdModelGetSortKey(pair->model2);
    MdtBodyID body1 = McdModelGetBody(pair->model1);
    MdtBodyID body2 = McdModelGetBody(pair->model2);
    
    MdtContactGroupID g = 0;

    if(body1 || body2)
    {
        if(body1==body2)
        {
            MeFatalError(0, "Attempt to create contact group between two collision models with "
            "the same dynamics body. You should disable collisions between such models in the"
            "McdSpace.");
            return 0;
        }

        g = MdtContactGroupCreate(w);
        if (g == 0)
        {
            MeFatalError(0, "Constraint pool too small during Mcd->Mdt ContactGroup creation.");
            return 0;
        }
        
        /* If one of the contacts is with the world, make world the second body. */
        if(body1) 
        {
            MdtContactGroupSetBodies(g, body1, body2);
            MdtContactGroupSetSortKey(g, -((key1<<15)+key2));
            g->swapped = 0;
        }
        else
        {
            MdtContactGroupSetBodies(g, body2, body1);
            MdtContactGroupSetSortKey(g, -((key2<<15)+key1));
            g->swapped = 1;
        }
        MdtContactGroupSetGenerator(g,pair);
        pair->responseData = g;
    }
    return g;

	unguard;
}

MeBool KIntersect(McdModelPair *p, McdIntersectResult *result);

// This should be the same as McdHello, but does not call the geometry-specific hello function.
// This is a hack - the only geometry type that uses a hello function is convex. But the caching does not work
// for aggregates of convex, so we just never call it just do not use caching. The intersect and goodbye functions
// work fine with a NULL m_cachedData.
static void KSimpleHello(McdModelPair *p)
{
	guard(KSimpleHello);

	McdFramework *frame = p->model1->frame;

	McdInteractions *interactions =
		McdFrameworkGetInteractions(p->model1->frame,
		McdGeometryGetType(p->model1->mInstance.mGeometry),
		McdGeometryGetType(p->model2->mInstance.mGeometry));

	if (interactions->swap) 
	{
		McdModelID tmp = p->model1;
		p->model1 = p->model2;
		p->model2 = tmp;
	}

	p->request = McdFrameworkGetDefaultRequestPtr(frame);

#if 0
	if (frame->mHelloCallbackFnPtr)
		(*(frame->mHelloCallbackFnPtr))(p);
#else
	if (interactions->helloFn)
		(*interactions->helloFn)(p);
#endif

	return;

	unguard;
}

static INT KAggregateGenericIntersect(McdModelPairID p, McdIntersectResult *result)
{
	guard(KAggregateGenericIntersect);

	McdModelPair dummyPair;
	McdModel dummyModel;
	McdIntersectResult dummyResult;
	MeMatrix4 elementTM;
	MeVector3 avgNormal;
	int i,j;
	McdGeometryInstanceID element, instance2;

	McdGeometryInstanceID ins = McdModelGetGeometryInstance(p->model1);
	McdAggregate *g = (McdAggregate *)McdGeometryInstanceGetGeometry(ins);

	check( McdGeometryGetType(&g->m_g) == kMcdGeometryTypeAggregate );

	McdContact contactBuffer[400];
	int contactCount = 0;

	result->touch = 0;
	result->contactCount = 0;

	dummyModel = *p->model1;
	dummyPair.m_cachedData = 0;
	dummyPair.model1 = &dummyModel;
	dummyPair.model2 = p->model2;
	dummyPair.request = p->request;

	dummyResult.pair = &dummyPair;

	instance2 = McdModelGetGeometryInstance(p->model2);

	MeVector3Set(avgNormal,0,0,0);

	for(i=0, element = ins->child; i<g->elementCountMax; i++, element = element->next)
	{
		if (g->elementTable[i].mGeometry && McdGeometryInstanceOverlap(instance2,element))
		{
			dummyModel.mInstance = *element;
			if(!dummyModel.mInstance.mTM)
			{
				dummyModel.mInstance.mTM = elementTM;
				MeMatrix4MultiplyMatrix(elementTM, g->elementTable[i].mRelTM, ins->mTM);
			}
			dummyResult.touch = 0;
			dummyResult.contactCount = 0;
			dummyResult.contacts = contactBuffer + contactCount;
			dummyResult.contactMaxCount = 400 - contactCount;

			dummyPair.m_cachedData = 0;

			KSimpleHello(&dummyPair);

			clock(GStats.DWORDStats(GEngineStats.STATS_Karma_KIntersect));
			KIntersect(&dummyPair, &dummyResult);
			unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_KIntersect));

			McdGoodbye(&dummyPair);

			if(dummyResult.touch)
			{
				result->touch = 1;
				McdGeometryType gtype = McdModelGetGeometryType(&dummyModel);
				/*
				* should we fillin detail in the user data,
				* or is there already some there?
				*/
				MeBool fillin = gtype!=kMcdGeometryTypeTriangleList &&
					gtype!=kMcdGeometryTypeAggregate;

				if(dummyPair.model2==p->model2)
				{
					if(fillin)
					{
						for(j=0;j<dummyResult.contactCount;j++)
						{
							dummyResult.contacts[j].element1.ptr = element;
						}
					}
				}
				else
				{   // models were flipped by Hello
					MeVector3Scale(dummyResult.normal,-1);
					for(j=0;j<dummyResult.contactCount;j++)
					{
						MeVector3Scale(dummyResult.contacts[j].normal,-1);
						dummyResult.contacts[j].element2.ptr = dummyResult.contacts[j].element1.ptr;

						if(fillin)
							dummyResult.contacts[j].element1.ptr = element;
					}
				}
				MeVector3MultiplyAdd(avgNormal, (MeReal)dummyResult.contactCount, dummyResult.normal);
				contactCount += dummyResult.contactCount;
			}
		}
	}
	MeVector3Normalize(avgNormal);

	MeVector3Copy(result->normal,avgNormal);

	if(contactCount>result->contactCount)
		result->contactCount = contactCount;
	memcpy(result->contacts,contactBuffer,contactCount*sizeof(McdContact));

	return result->touch;

	unguard;
}

#include "McdContact.h"

MeBool KIntersect(McdModelPair *p, McdIntersectResult *result)
{
	guard(KIntersect);

	//check(p->model1)
	//check(p->model2);

	int type1 = McdModelGetGeometryType(p->model1);
	int type2 = McdModelGetGeometryType(p->model2);

	guard(Types);

	///// TEMP DEBUGGING ////
	if(type1 <= kMcdGeometryTypeNull || type1 >= kMcdGeometryBuiltInTypes || type2 <= kMcdGeometryTypeNull || type2 >= kMcdGeometryBuiltInTypes)
	{
		AActor* actor1 = KModelGetActor(p->model1);
		AActor* actor2 = KModelGetActor(p->model2);

		if(actor1)
			debugf( TEXT("MODEL1 %x Actor: %s Geometry: %d"), p->model1, actor1->GetName(), type1 );
		else
			debugf( TEXT("MODEL1 %x Actor: NONE Geometry: %d"), p->model1, type1 );

		if(actor2)
			debugf( TEXT("MODEL2 %x Actor: %s Geometry: %d"), p->model2, actor2->GetName(), type2 );
		else
			debugf( TEXT("MODEL2 %x Actor: NONE Geometry: %d"), p->model2, type2 );
	}
	//////////////////////////

	check(type1 > kMcdGeometryTypeNull && type1 < kMcdGeometryBuiltInTypes);
	check(type2 > kMcdGeometryTypeNull && type2 < kMcdGeometryBuiltInTypes);

	McdInteractions *interactions;
	McdContact *outContacts = NULL;
	McdFramework *frame = p->model1->frame;

	result->pair = p;

	interactions = McdFrameworkGetInteractions(frame,type1,type2);
	check(interactions);

	if(!interactions->intersectFn)
	{
		result->contactCount = result->touch = 0;
		return 0;
	}

	if(interactions->cull)
	{
        const size_t BlockSize = 512*sizeof(McdContact);
		outContacts = result->contacts;
		result->contacts = (McdContact *)MeMemoryALLOCA(BlockSize); // TODO: fix me!
		appMemzero(result->contacts, BlockSize);
	}


	guard(Intersect Function);

	if(type1 == kMcdGeometryTypeAggregate || type2 == kMcdGeometryTypeAggregate)
	{
		result->touch = KAggregateGenericIntersect(p, result);
	}
	else if(type1 == kMcdGeometryTypeBox && type2 == kMcdGeometryTypeTriangleList)
	{
		result->touch = KBoxTriangleListIntersect(p, result);
	}
	else
	{
		result->touch = (*interactions->intersectFn)(p, result);
	}

	unguard;

	guard(McdContactSimplify);

	if(interactions->cull)
	{
		result->contactCount = McdContactSimplify(result->normal,
			result->contacts, result->contactCount,
			outContacts, p->request->contactMaxCount,
			p->request->faceNormalsFirst, frame->mScale);
		result->contacts = outContacts;
	}

	unguard;


	return 1;  // intersect function found

	unguardf(( TEXT("type1: %d, type2: %d"), type1, type2 ));

	unguard;
}

static MeBool KIntersectEach(McdModelPairContainer* pairs, 
							 int *startPair,
							 McdIntersectResult *resultArray,
							 int *resultCount,
							 int resultMaxCount,
							 McdContact *contactArray,
							 int *contactCount, 
							 int contactMaxCount)
{
	guard(KIntersectEach);

	// Reset contact/result counters
	*contactCount = 0;
	*resultCount = 0;

	// Iterate while we still have pairs to test.
	while( (*startPair) < pairs->stayingEnd )
	{
		// If we are out of contacts or results, bail out without incrementing startPair.
		INT contactsLeft = contactMaxCount - *contactCount;
		if(contactsLeft < 400)
			return 0;

		INT resultsLeft = resultMaxCount - *resultCount;
		if(resultsLeft == 0)
			return 0;

		// Go ahead and process this pair.
		McdModelPair* mp = pairs->array[(*startPair)];
		(*startPair)++;

		// Fill in intersect result.
		McdIntersectResult* r = resultArray + *resultCount;
		(*resultCount)++;

		r->touch = 0;
		r->contactCount = 0;
		r->contacts = contactArray + *contactCount;
		r->contactMaxCount = contactsLeft;

		//clock(GStats.DWORDStats(GEngineStats.STATS_Karma_KIntersect));
		KIntersect(mp, r);
		//unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_KIntersect));

		(*contactCount) += r->contactCount;
	}

	return 1; // We tested all pairs we were given.

	unguard;
}

#define CONTACT_BUFFER_SIZE 1000
void KHandleCollisions(McdModelPairContainer* pairs, ULevel* level)
{
	guard(KHandleCollisions);

    int i, j;

	MdtWorldID w = level->KWorld;
	MstBridgeID b = level->KBridge;

    if(pairs->helloFirst == pairs->stayingEnd)
        return;

	// Max number of results out is the max number of pairs in.
    McdIntersectResult *results = (McdIntersectResult*)MeMemoryALLOCA(pairs->size*sizeof(McdIntersectResult));
	INT resultCount = 0;

    McdContact *contacts = (McdContact*)MeMemoryALLOCA(CONTACT_BUFFER_SIZE*sizeof(McdContact));
	INT contactCount = 0;

    McdBatchContextReset(b->context);
    UBOOL arrayFinished = false;
	INT startPair = pairs->helloFirst;
    while(!arrayFinished)
    {
        arrayFinished = KIntersectEach(pairs, &startPair, 
			results, &resultCount, pairs->size, 
            contacts, &contactCount, CONTACT_BUFFER_SIZE);

        /* hello and staying - generate contacts and pass to dynamics */
        for( i = 0; i<resultCount ; i++ )
        {
            MdtContactID dynC;
            McdContact *colC;
            MdtContactGroupID group;
            McdIntersectResult *result = results+i;
            McdModelPairID pair = result->pair;
            
            MdtContactParamsID params;
            MstPerContactCBPtr contactCB;
            MstPerPairCBPtr pairCB;
            
            McdModelID m1 = pair->model1,
                       m2 = pair->model2;
            
            MstMaterialID material1 = McdModelGetMaterial(m1),
                          material2 = McdModelGetMaterial(m2);


            if(result->touch)
	        {		
                MstIntersectCBPtr intersectCB = MstBridgeGetIntersectCB(b,
                    material1, material2);

                if(m1->mIntersectFn)
                    (*m1->mIntersectFn)(m1,result);

                if(m2->mIntersectFn)
                    (*m2->mIntersectFn)(m2,result);
                
                if(intersectCB)
                    (*intersectCB)(result);
            }
            
            /* For each contact in result: copy geo data into existing MdtContact.
            If new MdtContacts needed, get, point to params, then copy geo data. */
            colC = result->contacts;
            
            group = (MdtContactGroupID)pair->responseData;
            if(!group)
            {
                // no contact group, either because there was no space to create it
                // at some point, or because this is the first time these objects
                // have actually intersected.
                group = CreateContactGroup(w,pair);
            }

            if(group)
            {
				guard(CopyContacts);

                dynC = MdtContactGroupGetFirstContact(group);
                params = MstBridgeGetContactParams(b, material1, material2);
                contactCB = MstBridgeGetPerContactCB(b, material1, material2);
                
                /* First try to use contacts already in list */
                
                for( j = 0; j < result->contactCount; j++)
                {
                    if(dynC == MdtContactInvalidID)
                    {
                        dynC = MdtContactGroupCreateContact(group); /* auto-assigns bodies */
                    }
                    
                    if (!dynC || dynC == MdtContactInvalidID)
                    {
						debugf(TEXT("KHandleCollisions: Could not create contact"));
                    }
                    else 
                    {
                        ConvertCollisionContact(params, colC+j, dynC, MdtContactGroupIsSwapped(group));
                        
                        /* If there's no contact callback, or the contact callback says keep the contact,
                        advance to next contact. Otherwise we re-use it. */
                        
                        if(!contactCB || (*contactCB)(result, colC+j, dynC))
                            dynC = MdtContactGroupGetNextContact(group,dynC);
                    }
                }
                
                /* nuke all the remaining contacts in this contact group */
                
                while(dynC != MdtContactInvalidID)
                {
                    MdtContactID nextC = MdtContactGroupGetNextContact(group,dynC);
                    MdtContactGroupDestroyContact(group,dynC);
                    dynC = nextC;
                }
                
				guard(Callbacks);
                /* Call Per Pair Callback (if there is one) */
                pairCB = MstBridgeGetPerPairCB(b, material1, material2);
                
                if(pairCB && !((*pairCB)(result,group)))
                {
                    MdtContactID c = MdtContactGroupGetFirstContact(group);
                    while (c != MdtContactInvalidID)
                    {
                        MdtContactID nextC = MdtContactGroupGetNextContact(group,c);
                        MdtContactGroupDestroyContact(group,c);
                        c = nextC;
                    }
                }
                
                if(group->count > 0)
                { 
                    if(!MdtContactGroupIsEnabled(group))
                        MdtContactGroupEnable(group);
                }
                else
                {
                    if(MdtContactGroupIsEnabled(group))
                        MdtContactGroupDisable(group);
                }
				unguard; // CALLBACKS

				unguard; // COPY CONTACTS
            }                
        }
    }

	unguard;
}

#endif

