/*============================================================================
	Karma Dynamics Stepping Functions
    
    - Modified copies/extensions of Mdt functions.
============================================================================*/

#include "EnginePrivate.h"

#ifdef WITH_KARMA

/* ********************* PARTITIONER ***************************** */

static inline UBOOL BodyGetDoubleRate(MdtBodyID body)
{
	guard(BodyGetDoubleRate);

	// All bodies should have associated actor with a UKarmaParams.
	AActor* actor = KBodyGetActor(body);
	check( actor->KParams );
	check( actor->KParams->IsA(UKarmaParams::StaticClass()) );
	UKarmaParams* kparams = (UKarmaParams*)(actor->KParams);
	return kparams->bKDoubleTickRate;

	unguard;
}

// Finds next un-ADDED body in list from supplied body that meet our needs.
static MeDictNode *FindNextUnadded(MeDict *dict, MeDictNode * node, UBOOL bDoubleRateActors)
{
    while (node)
    {
        MdtBody *b = (MdtBodyID)MeDictNodeGet(node);

		// Check this body is not already added, and bKDoubleTickRate is set as desired.
        if ( !(b->flags & MdtEntityAddedFlag) && BodyGetDoubleRate(b) == bDoubleRateActors )
        {
            MEASSERT(b->flags & MdtEntityEnabledFlag);
            return node;
        }
        node = MeDictNext(dict,node);
    }
    return 0;
}

// Divide the MdtWorld into seperate partitions that can be solved independently.
static void KBuildPartitions(MeDict *enabledBodyDict,
							  MdtPartitionOutput* po, const MdtPartitionEndCB pcb,
							  void* pcbdata, UBOOL bDoubleRateActors)
{
	guard(KBuildPartitions);

    /* Body to start next island exploration from. */
    MdtBody *rootBody;
    MeDictNode *rootNode;
    int i;

    MdtBaseConstraint *cp;
    MdtBody *bp;

    /*
       Arrays to keep track of all bodies and constraints added. This is
       necessary because the bodies and constraints may be removed during
       the end-of-partition callback.
    */
    MdtBody** addedBodies = (MdtBody**)MeMemoryALLOCA(po->maxBodies * sizeof(MdtBody*));
    int numAddedBodies = 0;

    MdtBaseConstraint** addedConstraints = (MdtBaseConstraint**)MeMemoryALLOCA(po->maxConstraints * sizeof(MdtBaseConstraint*));
    int numAddedConstraints = 0;

    po->nPartitions = 0;
    po->totalBodies = 0;
    po->totalConstraints = 0;

    po->overallInfo.contactCount = 0;
    po->overallInfo.jointCount = 0;
    po->overallInfo.rowCount = 0;

	// Find first node (body) that matches our criteria.
    rootNode = FindNextUnadded( enabledBodyDict, MeDictFirst(enabledBodyDict), bDoubleRateActors );

    /* LOOP OVER EACH PARTITION */
    while(rootNode != 0)
    {
        int exploreNext, addNext;

        rootBody = (MdtBodyID)MeDictNodeGet(rootNode);
        
        /* Reset number of bodies and constraints in the current partition. */
        po->bodiesSize[po->nPartitions] = 0;
        po->constraintsSize[po->nPartitions] = 0;

        MdtPartInfoReset(&po->info[po->nPartitions]);

        po->bodiesStart[po->nPartitions] = po->totalBodies;
        po->constraintsStart[po->nPartitions] = po->totalConstraints;

        /* Initialise to start of partition. */
        addNext = exploreNext = po->bodiesStart[po->nPartitions];

        /* Add nextRoot to partitionBodies array. */
        po->bodies[addNext++] = rootBody;
        addedBodies[numAddedBodies++] = rootBody;

        rootBody->arrayIdPartition = po->bodiesSize[po->nPartitions];
        rootBody->arrayIdWorld = po->bodiesStart[po->nPartitions] + po->bodiesSize[po->nPartitions];
        rootBody->partitionIndex = po->nPartitions;
        rootBody->flags |= MdtEntityAddedFlag;

        (po->totalBodies)++;
        (po->bodiesSize[po->nPartitions])++;


        /* LOOP OVER EACH PARTITION BODY */
        while (exploreNext < po->bodiesStart[po->nPartitions] + po->bodiesSize[po->nPartitions])
        {
            MeDict *dict;
            MeDictNode *node;

            MdtBody *currentBody = po->bodies[exploreNext++];
            dict = &currentBody->constraintDict;
            /* Loop over all constraints affecting this body. */

            /* LOOP OVER CURRENT BODY CONSTRAINTS */
            for(node = MeDictFirst(dict); node != 0; node = MeDictNext(dict,node))
            {
                cp = (MdtBaseConstraint *)MeDictNodeGet(node);

                /* Follow this constraint if it has not been ADDED yet. */
                if (!(cp->head.flags & MdtEntityAddedFlag))
                {
                    if (po->totalConstraints >= po->maxConstraints)
                        MeFatalError(1, "KBuildPartitions: Adding too many constraints.");

                    /* Add constraint pointer to partition constraints array
                       and mark as added. */
                    po->constraints[po->totalConstraints] = cp;
                    addedConstraints[numAddedConstraints++] = cp;
    
                    (po->totalConstraints)++;
                    (po->constraintsSize[po->nPartitions])++;

                    /* Keep track of other information about this partition. */
                    MdtPartInfoAddConstraint(&po->info[po->nPartitions], cp);

                    cp->head.flags |= MdtEntityAddedFlag;

                    /* Find bodies affected by this constraint */
                    for (i = 0; i < MdtKeaMAXBODYCONSTRAINT; i++)
                    {
                        bp = cp->head.mdtbody[i];

                        if ((bp) && (bp->keaBody.tag != MdtKeaBODYVER1))
                            MeFatalError(0,"KBuildPartitions: Constraint found with invalid or destroyed MdtBody.");

#if 1
						// Double check we are not adding bodies that have the wrong bKDoubleTickRate flag.
						if(bp && BodyGetDoubleRate(bp) != bDoubleRateActors)
                            MeFatalError(0,"KBuildPartitions: Constraint found between bodies using different bKDoubleTickRate.");
#endif

                        /* If a body exists AND we are not coming from it.. */
                        if ((bp) && (bp != currentBody) &&
                            /* ..AND it has not yet been added. */
                            !(bp->flags & MdtEntityAddedFlag))
                        {
                            if (po->totalBodies >= po->maxBodies)
                                MeFatalError(1, "KBuildPartitions: Adding too many constraints.");

                            /* Add body to partitionBodies array! */
                            po->bodies[addNext++] = bp;
                            addedBodies[numAddedBodies++] = bp;

                            bp->arrayIdPartition = po->bodiesSize[po->nPartitions];
                            bp->arrayIdWorld = po->bodiesStart[po->nPartitions] + po->bodiesSize[po->nPartitions];
                            bp->partitionIndex = po->nPartitions;
                            bp->flags |= MdtEntityAddedFlag;

                            (po->totalBodies)++;
                            (po->bodiesSize[po->nPartitions])++;
                        }
                    }
                }
            }

            /* If this body should be enabled, do so here. */
            if (!MdtBodyIsEnabled(currentBody))
            {
                MdtBodyEnable(currentBody);
                currentBody->flags |= MdtEntityEnabledByPartitioner;
            }
            else
                currentBody->flags &= ~MdtEntityEnabledByPartitioner;
        }

        /* FINISHED A PARTITION */

        /* Number of rows in a partition always rounded to nearest 4. */
        po->info[po->nPartitions].rowCount = MeMathCEIL4(po->info[po->nPartitions].rowCount);

        /* Keep track of overall stats (for all partitions). */
        po->overallInfo.rowCount += po->info[po->nPartitions].rowCount;        
        po->overallInfo.jointCount += po->info[po->nPartitions].jointCount;        
        po->overallInfo.contactCount += po->info[po->nPartitions].contactCount;

        /* Finally increase number of partitions! */
        if (po->nPartitions >= po->maxPartitions)
            MeFatalError(1, "KBuildPartitions: Adding too many partitions.");

        (po->nPartitions)++;

		// Find the next node (body) that matches our criteria.
        /* do this here in case the callback removes the node from the enabled list! */
        rootNode = FindNextUnadded(enabledBodyDict, rootNode, bDoubleRateActors);

        /* If we have a end-of-partition callback - call it. */
        if(pcb) pcb(po, pcbdata);
    }

    /* FINISHED ALL PARTITIONS */

    /* Mark constraints that were added as 'un-ADDED' again */
    for(i=0; i < numAddedConstraints; i++)
        addedConstraints[i]->head.flags &= ~MdtEntityAddedFlag;

    /* Mark bodies that were added as as 'un-ADDED' again */
    for(i=0; i < numAddedBodies; i++)
        addedBodies[i]->flags &= ~MdtEntityAddedFlag;

    MeMemoryFREEA(addedConstraints);
    MeMemoryFREEA(addedBodies);

	unguard;
}


/* ******************** AUTO-DISABLE ***************************** */

static MeBool MEAPI KBodyIsMovingTest(const MdtBodyID b, const MdtPartitionParams* params)
{
    MeReal ss_vel, ss_velrot, ss_acc, ss_accrot;

    ss_vel = MeVector3MagnitudeSqr(b->keaBody.vel);
    ss_acc = MeVector3MagnitudeSqr(b->keaBody.accel);
    ss_velrot = MeVector3MagnitudeSqr(b->keaBody.velrot);
    ss_accrot = MeVector3MagnitudeSqr(b->keaBody.accelrot);

    /*  Check if its moving... 
        NB: things can be turned off straight away if turned on 
        automatically by partitioner */
    if ((ss_vel > params->vel_thresh)
        || (ss_velrot > params->velrot_thresh)
        || (ss_acc > params->acc_thresh)
        || (ss_accrot > params->accrot_thresh)
        || (b->enabledTime < params->alive_time_thresh && 
            !(b->flags & MdtEntityEnabledByPartitioner)))
        return 1;
    else
        return 0;
}

static void MEAPI KAutoDisableLastPartition(MdtPartitionOutput* po, void* pcbdata)
{
	guard(KAutoDisableLastPartition);

    MeBool partitionIsMoving = 0; /* assume stopped until proven otherwise*/
    int b, p = po->nPartitions-1; /* partition we are checking (last one) */

    MdtPartitionParams* params = (MdtPartitionParams*)pcbdata;

    /* If we are 'auto-disabling' bodies, see if partition has come to rest. */
    if(params->autoDisable)
    {
        /*  Iterate over last partition bodies.
            As soon as we find a moving body, bail out.. */
        for(b = po->bodiesStart[p]; (b < (po->bodiesStart[p] + po->bodiesSize[p]) && !partitionIsMoving); b++)
        {
            if(KBodyIsMovingTest(po->bodies[b], params))
                partitionIsMoving = 1;
        }
    }
    else
        partitionIsMoving = 1;

    /* if partition has come to rest, disable bodies in it and remove
       partition from MdtPartitionOutput. */
    if(!partitionIsMoving)
    {
        for(b = po->bodiesStart[p]; b < (po->bodiesStart[p] + po->bodiesSize[p]); b++)
        {
            MdtBodyID body = po->bodies[b];
            MdtBodyDisable(body);
            MdtBodyResetForces(body);
            MdtBodyResetImpulses(body);
        }

        po->nPartitions--;
        po->totalBodies -= po->bodiesSize[p];
        po->totalConstraints -= po->constraintsSize[p];

        po->overallInfo.rowCount -= po->info[p].rowCount;        
        po->overallInfo.jointCount -= po->info[p].jointCount;        
        po->overallInfo.contactCount -= po->info[p].contactCount;
    }
    /*  if its still alive, see how big we think its going to be, and if 
        larger than MaxMatrixSize, take measures to 'cap' it. */
    else
    {
        if(po->info[p].rowCount > params->maxMatrixSize)
            MdtLODLastPartition(po, params);
    }

	unguard;
}

/* ******************** FIND MODEL SAFETIME ***************************** */

#define RECIP_ROOT_TWO (0.70710678118654752440084436210485f) // One over the square root of two. Takes you from sphere radius to box radius (pythag).

// Returns number between 0 and 1, indicating how far this object can be evolved before it is in danger
// of passing through something.
static MeReal FindModelSafetime(McdModelID model, MeMatrix4 startTM, MeMatrix4 endTM, AActor* actor)
{
	guard(FindModelSafetime);

	FLOAT ModelSafeTime = 1.0f;

	// Should only ever be dealing with aggregates here, but double check.
	McdAggregateID aggGeom = McdModelGetGeometry(model);
	if(McdGeometryGetTypeId(aggGeom) != kMcdGeometryTypeAggregate)
		return ModelSafeTime;

	KarmaModelUserData* ModelData = (KarmaModelUserData*)McdModelGetUserData(model);

	if(!ModelData)
		return ModelSafeTime;

	for(INT i=0; i<ModelData->SafeTimeSpheres.Num(); i++)
	{
		MeVector3 sphereCenter;
		KU2MEVecCopy( sphereCenter, ModelData->SafeTimeSpheres(i) );

		MeReal checkRadius = RECIP_ROOT_TWO * K_ME2UScale * ModelData->SafeTimeSpheres(i).W; // Convert from karma-scale sphere to unreal-scale box radius.
		FVector checkExtent = FVector(checkRadius, checkRadius, checkRadius);

		MeVector3 meStartPos, meEndPos;
		MeMatrix4TMTransform(meStartPos, startTM, sphereCenter); // Work out sphere center in world space at start of timestep.
		MeMatrix4TMTransform(meEndPos, endTM, sphereCenter); // Work out sphere center in world space at end of timestep.

		FVector startPos, endPos;
		KME2UPosition(&startPos, meStartPos);
		KME2UPosition(&endPos, meEndPos);

		// Do actual line check against level
		FMemMark Mark(GMem);

		FCheckResult* FirstHit = actor->GetLevel()->MultiLineCheck(GMem, endPos, startPos, checkExtent, actor->Level, TRACE_World | TRACE_Volumes, actor);

		// Debug
		if( KGData && (KGData->DebugDrawOpt & KDRAW_SafeTime) )
		{
			GTempLineBatcher->AddLine(startPos, endPos, FColor(0, 255, 255));
			GTempLineBatcher->AddBox(FBox(startPos-checkExtent, startPos+checkExtent), FColor(0, 255, 255));
			GTempLineBatcher->AddBox(FBox(endPos-checkExtent, endPos+checkExtent), FColor(0, 255, 255));
		}

		// Walk through list of results to see if there is one we need to take notice of.
		FCheckResult* Check;
		for( Check = FirstHit; Check!=NULL; Check=Check->GetNext() )
		{
			// If we hit something that should stop karma - reduce the safe-time for this model.
			if( Check->Time < ModelSafeTime && Check->Actor->bShouldStopKarma )
			{
				ModelSafeTime = Check->Time;
				break;
			}
		}

		Mark.Pop();
	}

	return ModelSafeTime;

	unguard;
}

/* ******************** CUSTOM STEP SAFETIME ***************************** */

static UBOOL CheckSim(MdtWorldID w,  
                     MdtKeaBody** keabodyArray, int nBodies, 
                     MdtKeaConstraints* keaCon, TArray<AActor*>& DeathRow)
{
	guard(CheckSim);

    int i, j;
    MeBool isError = 0;
    
    /* Check each body. */
    for(i=0; i<nBodies && !isError; i++)
    {
        MdtKeaBody* b = keabodyArray[i];
        for(j=0; j<3; j++)
        {
            if( !MeRealIsValid(b->accel[j]) || 
				!MeRealIsValid(b->accelrot[j]) || 
				!MeRealIsValid(b->force[j]) || 
				!MeRealIsValid(b->torque[j]))
                isError = 1;
        }
    }
    
    /* Check each constraint. */
    for(i=0; i<keaCon->num_constraints && !isError; i++)
    {
        MdtKeaForcePair* fp = &(keaCon->force[i]);
        for(j=0; j<3; j++)
        {
            if(	!MeRealIsValid(fp->primary_body.force[j]) || 
				!MeRealIsValid(fp->primary_body.torque[j]) || 
				!MeRealIsValid(fp->secondary_body.force[j]) || 
				!MeRealIsValid(fp->secondary_body.torque[j]))
                isError = 1;
        }
    }
    
#if 0 // Simulated SimErrors
	{
		//check(nBodies > 0);
		//MdtBodyID body = (MdtBody*)keabodyArray[0];
		//AActor* actor = KBodyGetActor(body);

		if(appFrand() < 0.002f)
		//if(actor->GetLevel()->GetLevelInfo()->NetMode == NM_DedicatedServer && appFrand() < 0.002f)
			isError = 1;
	}
#endif

	// If partition has blown up (bad!), destroy any actors in this partition.
    if(isError)
	{
		// We will destroy this body, but in the mean time, set forces/velocities to zero.
		for(i=0; i<nBodies; i++)
		{
			MdtKeaBody* body = keabodyArray[i];

			MeVector3Set(body->force, 0, 0, 0);
			MeVector3Set(body->torque, 0, 0, 0);
			MeVector3Set(body->accel, 0, 0, 0);
			MeVector3Set(body->accelrot, 0, 0, 0);
			MeVector3Set(body->vel, 0, 0, 0);
			MeVector3Set(body->velrot, 0, 0, 0);
		}

		for(i=0; i<keaCon->num_constraints; i++)
		{
			keaCon->force[i].primary_body.force[0] = 0;
			keaCon->force[i].primary_body.force[1] = 0;
			keaCon->force[i].primary_body.force[2] = 0;

			keaCon->force[i].primary_body.torque[0] = 0;
			keaCon->force[i].primary_body.torque[1] = 0;
			keaCon->force[i].primary_body.torque[2] = 0;

			keaCon->force[i].secondary_body.force[0] = 0;
			keaCon->force[i].secondary_body.force[1] = 0;
			keaCon->force[i].secondary_body.force[2] = 0;

			keaCon->force[i].secondary_body.torque[0] = 0;
			keaCon->force[i].secondary_body.torque[1] = 0;
			keaCon->force[i].secondary_body.torque[2] = 0;
		}

		debugf(NAME_DevKarma, TEXT("(Karma:) Partition SimError!"));

		// Destroy actors if desired
		for(i=0; i<nBodies; i++)
		{
			MdtBodyID body = (MdtBody*)keabodyArray[i];
			AActor* actor = KBodyGetActor(body);

			check(actor->KParams); // Should always have a UKarmaParams
			UKarmaParams* kparams = Cast<UKarmaParams>(actor->KParams);
			if(kparams->bDestroyOnSimError)
			{
				if(DeathRow.FindItemIndex(actor) == INDEX_NONE)
					DeathRow.AddItem(actor);
			}

			debugf(NAME_DevKarma, TEXT("(Karma:) SimError Actor: %s (%d)"), actor->GetName(), kparams->bDestroyOnSimError );
		}

		return 1;
    }

	return 0;

	unguard;
}

// See if this partition contains a ragdoll.
static UBOOL PartitionContainsRagdoll(MdtBodyID* bodyArray, int nBodies)
{
	guard(PartitionContainsRagdoll);

	for(INT i=0; i<nBodies; i++)
	{
		MdtBodyID body = bodyArray[i];
		AActor* actor = KBodyGetActor(body);
		check(actor);
		if(actor->Physics == PHYS_KarmaRagDoll)
			return 1;
	}

	return 0;

	unguard;
}

// Call the 'prePhysKarma' function on each actor about to be evolved.
// Because in the skeletal case multiple bodies relate to just one actor, we use the KStepTag 
// to only it call it once on each actor.
static INT GKStepTag = 0;

/// PRE ///
static void CallPreBodyStep(MdtBodyID* bodyArray, int nBodies, FLOAT delta)
{
	guard(CallPreBodyStep);

	GKStepTag++;
	for(INT i=0; i<nBodies; i++)
	{
		MdtBodyID body = bodyArray[i];
		AActor* actor = KBodyGetActor(body);
		check(actor);
		if(actor->KStepTag != GKStepTag)
		{
			actor->KStepTag = GKStepTag;
			actor->preKarmaStep(delta);
		}
	}

	unguard;
}

static void CallPreConstraintStep(MdtConstraintID* conArray, int nConstraints, FLOAT delta)
{
	guard(CallPreConstraintStep);

	GKStepTag++;
	for(INT i=0; i<nConstraints; i++)
	{
		MdtConstraintID con = conArray[i];
		AKConstraint* conActor = (AKConstraint*)MdtConstraintGetUserData(con);
		if(conActor && conActor->KStepTag != GKStepTag)
		{
			conActor->KStepTag = GKStepTag;
			conActor->preKarmaStep(delta);
		}
	}

	unguard;
}

/// POST ///
// Add each actor being simulated to the PostKarmaStep call-list. We do this to avoid calling
// PostKarmaStep inside the loop. We dont add any actors that are on death-row pending deletion due to
// sim error.
static void AddPostBodyStep(TArray<AActor*> &postActors, MdtBodyID* bodyArray, int nBodies)
{
	guard(AddPostBodyStep);

	GKStepTag++;
	for(INT i=0; i<nBodies; i++)
	{
		MdtBodyID body = bodyArray[i];
		AActor* actor = KBodyGetActor(body);
		check(actor);
		// If we haven't already added this actor to the array, and its not pending destruction.
		if( actor->KStepTag != GKStepTag )
		{
			actor->KStepTag = GKStepTag;
#if 1
			// Check actor is not already in the array.
			check(postActors.FindItemIndex(actor) == INDEX_NONE);
#endif
				postActors.AddItem(actor);
		}
	}

	unguard;
}

// As above, but for constraints.
static void AddPostConstraintStep(TArray<AActor*> &postActors, MdtConstraintID* conArray, int nConstraints)
{
	guard(AddPostConstraintStep);

	GKStepTag++;
	for(INT i=0; i<nConstraints; i++)
	{
		MdtConstraintID con = conArray[i];
		AKConstraint* conActor = (AKConstraint*)MdtConstraintGetUserData(con);
		if( conActor && conActor->KStepTag != GKStepTag )
		{
			conActor->KStepTag = GKStepTag;
#if 1
			check(postActors.FindItemIndex(conActor) == INDEX_NONE);
#endif
				postActors.AddItem(conActor);
		}
	}

	unguard;
}

// The 'timestepping' scheme Karma uses dictates the form of this integrator.
// Replacing it with a different one (eg Runge Kutta) would be a _very_ bad idea.
static void MEAPI KIntegrateSystem(MdtKeaBody *const		blist[],
								   MdtKeaTransformation *	tlist,
								   int						num_bodies,
								   MeReal					stepSize)
{
	guard(KIntegrateSystem);

    int i,j;
    
    // do 'pos = pos + h*vel' : compute quaternion derivatives and update
    // position
    for (i=0; i!=num_bodies; i++)
    {
        tlist[i].pos[0] += stepSize*blist[i]->vel[0];
        tlist[i].pos[1] += stepSize*blist[i]->vel[1];
        tlist[i].pos[2] += stepSize*blist[i]->vel[2];
        tlist[i].pos[3] = (MeReal)(1.0);
        
        // for this body, if fastSpin=0 then use the standard infitesimal
        // quaternion update. otherwise use a finite rotation on the fast spin
        // axis followed by an infitesimal update on the rotation axis orthogonal
        // to the fast spin axis. this is a dodgy hack!!
        MeReal myw[3];    // omega goes here
        myw[0] = blist[i]->velrot[0];
        myw[1] = blist[i]->velrot[1];
        myw[2] = blist[i]->velrot[2];
        
        if (blist[i]->flags & MdtKeaBodyFlagUseFastSpin) 
		{
            MeReal rot = MeVector3Dot (myw,blist[i]->fastSpinAxis);
            MeQuaternionFiniteRotation (blist[i]->qrot,blist[i]->fastSpinAxis,rot * stepSize);
            myw[0] -= blist[i]->fastSpinAxis[0] * rot;
            myw[1] -= blist[i]->fastSpinAxis[1] * rot;
            myw[2] -= blist[i]->fastSpinAxis[2] * rot;
        }
        
        MeReal dq[4];
        MeReal s=0.0f;
        dq[0] = (MeReal)(0.5)*( -blist[i]->qrot[1]*myw[0] -blist[i]->qrot[2]*myw[1] -blist[i]->qrot[3]*myw[2] );
        dq[1] = (MeReal)(0.5)*( +blist[i]->qrot[0]*myw[0] +blist[i]->qrot[3]*myw[1] -blist[i]->qrot[2]*myw[2] );
        dq[2] = (MeReal)(0.5)*( -blist[i]->qrot[3]*myw[0] +blist[i]->qrot[0]*myw[1] +blist[i]->qrot[1]*myw[2] );
        dq[3] = (MeReal)(0.5)*( +blist[i]->qrot[2]*myw[0] -blist[i]->qrot[1]*myw[1] +blist[i]->qrot[0]*myw[2] );
        
        blist[i]->qrot[0] += stepSize * dq[0];
        blist[i]->qrot[1] += stepSize * dq[1];
        blist[i]->qrot[2] += stepSize * dq[2];
        blist[i]->qrot[3] += stepSize * dq[3];
        
        // Normalise the quaternion
        for (j=0; j<4; j++) s += MeSqr(blist[i]->qrot[j]);
        s = MeRecipSqrt(s);
        
        // update the position auxiliary variables
        // (maintain coupling invariant that qrot and R represent the same rotation)
        tlist[i].R0[3] = 0;
        tlist[i].R1[3] = 0;
        tlist[i].R2[3] = 0;
        
        blist[i]->qrot[0] *= s;
        blist[i]->qrot[1] *= s;
        blist[i]->qrot[2] *= s;
        blist[i]->qrot[3] *= s;
        
        tlist[i].R0[0] =   blist[i]->qrot[0]*blist[i]->qrot[0] + blist[i]->qrot[1]*blist[i]->qrot[1] - blist[i]->qrot[2]*blist[i]->qrot[2] - blist[i]->qrot[3]*blist[i]->qrot[3];
        tlist[i].R1[0] =  2.0f*blist[i]->qrot[1]*blist[i]->qrot[2] - 2.0f*blist[i]->qrot[0]*blist[i]->qrot[3];
        tlist[i].R2[0] =  2.0f*blist[i]->qrot[0]*blist[i]->qrot[2] + 2.0f*blist[i]->qrot[1]*blist[i]->qrot[3];
        tlist[i].R0[1] =  2.0f*blist[i]->qrot[1]*blist[i]->qrot[2] + 2.0f*blist[i]->qrot[0]*blist[i]->qrot[3];
        tlist[i].R1[1] =   blist[i]->qrot[0]*blist[i]->qrot[0] -      blist[i]->qrot[1]*blist[i]->qrot[1] + blist[i]->qrot[2]*blist[i]->qrot[2] - blist[i]->qrot[3]*blist[i]->qrot[3];
        tlist[i].R2[1] = -2.0f*blist[i]->qrot[0]*blist[i]->qrot[1] + 2.0f*blist[i]->qrot[2]*blist[i]->qrot[3];
        tlist[i].R0[2] = -2.0f*blist[i]->qrot[0]*blist[i]->qrot[2] + 2.0f*blist[i]->qrot[1]*blist[i]->qrot[3];
        tlist[i].R1[2] =  2.0f*blist[i]->qrot[0]*blist[i]->qrot[1] + 2.0f*blist[i]->qrot[2]*blist[i]->qrot[3];
        tlist[i].R2[2] =   blist[i]->qrot[0]*blist[i]->qrot[0] -      blist[i]->qrot[1]*blist[i]->qrot[1] - blist[i]->qrot[2]*blist[i]->qrot[2] + blist[i]->qrot[3]*blist[i]->qrot[3];
        
    }

	unguard;
}

// Same integrator as abov, but just works directly on a TM.
static void KIntegrateTM(const MeMatrix4 startTM, 
						 const MeVector4 startQ, 
						 const MeVector3 vel, 
						 const MeVector3 angVel, 
						 const MeReal stepSize, 
						 MeMatrix4 endTM)
{
	guard(KIntegrateTM);

	MeMatrix4TMMakeIdentity(endTM);

	// Linear update
	endTM[3][0] = startTM[3][0] + (stepSize * vel[0]);
	endTM[3][1] = startTM[3][1] + (stepSize * vel[1]);
	endTM[3][2] = startTM[3][2] + (stepSize * vel[2]);

	// Angular update

	// Calculate delta quatrnion
	MeVector4 dq, endQ;
	dq[0] = (MeReal)(0.5)*( -startQ[1]*angVel[0] -startQ[2]*angVel[1] -startQ[3]*angVel[2] );
	dq[1] = (MeReal)(0.5)*( +startQ[0]*angVel[0] +startQ[3]*angVel[1] -startQ[2]*angVel[2] );
	dq[2] = (MeReal)(0.5)*( -startQ[3]*angVel[0] +startQ[0]*angVel[1] +startQ[1]*angVel[2] );
	dq[3] = (MeReal)(0.5)*( +startQ[2]*angVel[0] -startQ[1]*angVel[1] +startQ[0]*angVel[2] );

	// Add to start quaternion
	endQ[0] = startQ[0] + (stepSize * dq[0]);
	endQ[1] = startQ[1] + (stepSize * dq[1]);
	endQ[2] = startQ[2] + (stepSize * dq[2]);
	endQ[3] = startQ[3] + (stepSize * dq[3]);

	// Normalize quaternion (project back onto unit hypersphere)
	MeReal s = MeSqr(endQ[0]) + MeSqr(endQ[1]) + MeSqr(endQ[2]) + MeSqr(endQ[3]);
	s = MeRecipSqrt(s);
	endQ[0] *= s;
	endQ[1] *= s;
	endQ[2] *= s;
	endQ[3] *= s;

	// Convert new quaternion to rotation matrix
	endTM[0][0] =       endQ[0]*endQ[0] +      endQ[1]*endQ[1] - endQ[2]*endQ[2] - endQ[3]*endQ[3];
	endTM[1][0] =  2.0f*endQ[1]*endQ[2] - 2.0f*endQ[0]*endQ[3];
	endTM[2][0] =  2.0f*endQ[0]*endQ[2] + 2.0f*endQ[1]*endQ[3];
	endTM[0][1] =  2.0f*endQ[1]*endQ[2] + 2.0f*endQ[0]*endQ[3];
	endTM[1][1] =       endQ[0]*endQ[0] -      endQ[1]*endQ[1] + endQ[2]*endQ[2] - endQ[3]*endQ[3];
	endTM[2][1] = -2.0f*endQ[0]*endQ[1] + 2.0f*endQ[2]*endQ[3];
	endTM[0][2] = -2.0f*endQ[0]*endQ[2] + 2.0f*endQ[1]*endQ[3];
	endTM[1][2] =  2.0f*endQ[0]*endQ[1] + 2.0f*endQ[2]*endQ[3];
	endTM[2][2] =       endQ[0]*endQ[0] -      endQ[1]*endQ[1] - endQ[2]*endQ[2] + endQ[3]*endQ[3];

	unguard;
}

void MEAPI KWorldStepSafeTime(MdtWorldID w, MeReal stepSize, ULevel* level, UBOOL bDoubleRateActors, UBOOL bFinalSubStep)
{
	guard(KWorldStepSafeTime);

    int partitionindex, constraintRows;
    MdtBody *b;
    MeDictNode *node;
    MeDict *dict;
    MdtPartitionOutput* po;
    TArray<AActor*> DeathRow, PostStepActors;

    po = MdtPartOutCreateFromChunk(&w->partOutChunk, w->nBodies, w->nEnabledConstraints);

    /* call the partitioner */
    KBuildPartitions(&w->enabledBodyDict, po, KAutoDisableLastPartition, &w->partitionParams, bDoubleRateActors);

	guard(ProcessPartitions);

    /* For _each_ partition pack, simulate and unpack. */
    for(partitionindex=0; partitionindex < po->nPartitions; partitionindex++)
    {
        int conCount, partStart, conStart, sizeRequired;
        MdtKeaBody **keabodyArray;
        MdtKeaTransformation *keatmArray;
        MdtKeaConstraints* keaCon;    
        MeReal partStepSize = stepSize;

        partStart = po->bodiesStart[partitionindex];
		conStart = po->constraintsStart[partitionindex];

		// Do extra timestep scaling if partition contains a ragdoll.
		if(PartitionContainsRagdoll(&po->bodies[partStart], po->bodiesSize[partitionindex]))
			partStepSize *= level->GetLevelInfo()->RagdollTimeScale;

		// Call PreKarmaStep on actors about to be simulated (gravity, controllers etc.)
		clock(GStats.DWORDStats(GEngineStats.STATS_Karma_PreKarmaStep));
		CallPreBodyStep(&po->bodies[partStart], po->bodiesSize[partitionindex], partStepSize);
		CallPreConstraintStep(&po->constraints[conStart], po->constraintsSize[partitionindex], partStepSize);
		unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_PreKarmaStep));

        /* Get temporary memory to hold array of body transforms for Kea. */
        keatmArray = (MdtKeaTransformation*)MeChunkGetMem(
            &w->keaTMChunk, 
            po->bodiesSize[partitionindex] * sizeof(MdtKeaTransformation));

        /* Create temporary MdtKeaConstraints structure for feeding Kea. */
        keaCon = MdtKeaConstraintsCreateFromChunk(
            &w->keaConstraintsChunk, 
            1, 
            po->info[partitionindex].contactCount + po->info[partitionindex].jointCount,
            po->info[partitionindex].rowCount);


        /* Pack one partition into one set of Kea input. */
		guard(MdtPackPartition);
        constraintRows = MdtPackPartition(po,
            partitionindex, partStepSize, &w->params,  &w->keaParams,
            keatmArray,
            keaCon);
		unguard;

        /*  Because we can cast from MdtBody* to MdtKeaBody*, this is how
            we obtain the array of MdtKeaBody* pointers. */
        keabodyArray = (MdtKeaBody**)&(po->bodies[partStart]);
        
        /* MAKE KEA PARAMETERS */
        
        /* Scale 'gamma' with timestep. If partStepSize is unchanged, gamma will be
           the user-defined value. */
        w->keaParams.gamma = w->constantGamma;
        w->keaParams.stepsize = partStepSize;
        
        /*  Use kea utility to figure out how much memory pool we need, then 
        grab it from the MeChunk. */
        sizeRequired = MdtKeaMemoryRequired(
            keaCon->num_rows_exc_padding_partition,
            keaCon->num_partitions,
            constraintRows,
            po->bodiesSize[partitionindex]);
        
        w->keaParams.memory_pool = MeChunkGetMem(&w->keaPool, sizeRequired);
        w->keaParams.memory_pool_size = sizeRequired;

        MdtFlushCache(0);

#ifdef __PSX2_EE__
        /* On PS2, the lambda vector and constraint forces vector must be accessed
        in uncached or uncached accelerated mode. */
        
        keaCon->force  = ((unsigned int)(keaCon->force)|0x30000000);
        keaCon->lambda = ((unsigned int)(keaCon->lambda)|0x30000000);
#endif                

        /* Run constraint solver to calculate forces required 
        to satisfy all constraints (in all partitions). */

		clock(GStats.DWORDStats(GEngineStats.STATS_Karma_ConstraintSolver));

		guard(MdtKeaAddConstraintForces);
        MdtKeaAddConstraintForces(*keaCon, 
            keabodyArray,
            keatmArray,
            po->bodiesSize[partitionindex], 
            w->keaParams);
		unguard;
                
		unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_ConstraintSolver));

		// If bad forces are generated, will just set them to zero so bodies wont move, then
		// they will be destroyed at the end. Calling PostKarmaStep should still be safe.
		UBOOL wasError = 0;
        if(w->checkSim)
		{
            wasError = CheckSim(w, keabodyArray, po->bodiesSize[partitionindex], keaCon, DeathRow);
		}
		
		///////// INTEGRATE USING SAFETIME /////////
		guard(Integrate);

		if(KGData->bUseSafeTime)
		{
	        MeReal safeTime = partStepSize;

			// Calculate safe-time for each body, and hence this partition.
			// We use the newly updated velocity of the body, and scale the timestep we use to update the position
		    MdtBody** blist = &(po->bodies[po->bodiesStart[partitionindex]]);

			for(INT bIx=0; bIx<po->bodiesSize[partitionindex]; bIx++)
			{
				MdtBodyID stbody = blist[bIx];

				// In our case we do velocity update first (outside integrator) using full timestep (thats what the solver uses)
				// Then we use this new velocity to do the line test and figure out if we are going to pass 
				// through the world on this timestep. If so, shorten the timestep for use in the position update.
				// We always do this to each body - because KIntegrateSystem doesn't!

				stbody->keaBody.vel[0] += partStepSize * stbody->keaBody.accel[0];
				stbody->keaBody.vel[1] += partStepSize * stbody->keaBody.accel[1];
				stbody->keaBody.vel[2] += partStepSize * stbody->keaBody.accel[2];
				
				stbody->keaBody.velrot[0] += partStepSize * stbody->keaBody.accelrot[0];
				stbody->keaBody.velrot[1] += partStepSize * stbody->keaBody.accelrot[1];
				stbody->keaBody.velrot[2] += partStepSize * stbody->keaBody.accelrot[2];

				// All dynamics bodies have a model and an actor.
				McdModelID stmodel = (McdModelID)stbody->model;
				check(stmodel);

				// Do no safe-time check if this model has a null collision geometry.
				if(McdGeometryGetTypeId(McdModelGetGeometry(stmodel)) == kMcdGeometryTypeNull)
					continue;

				AActor* stactor = KModelGetActor(stmodel);
				check(stactor);

				// If this actor does not have safe-time turned on
				// All actors with karma on have a UKarmaParams, not just a UKarmaParamsCollision
				UKarmaParams* kparams = (UKarmaParams*)stactor->KParams;
				check(kparams);

				if(!kparams->bDoSafetime)
					continue;

				/////////// DOING SAFETIME /////////////

				// Use the updated velocity/angular velocity to figure out where the COM will be at the end of the timestep.
				// startTM/endTM are the BODY not COM position (ie. should be same as model TM)
				MeMatrix4 startTM, endTM;

				MeMatrix4Copy(startTM, stbody->bodyTM);

				KIntegrateTM((MeMatrix4Ptr)&keatmArray[bIx], 
					stbody->keaBody.qrot, 
					stbody->keaBody.vel, 
					stbody->keaBody.velrot, 
					partStepSize, 
					endTM);

				// Apply com->body offset (body space)
				if(stbody->useCom)
				{
					MeVector3 tmp, bodyPos;
					MeVector3MultiplyScalar(tmp, stbody->com, -1);
					MeMatrix4TMTransform(bodyPos, endTM, tmp);
					MeMatrix4TMSetPositionVector(endTM, bodyPos);
				}

				
				MeReal hitTime;

				clock(GStats.DWORDStats(GEngineStats.STATS_Karma_FindModelSafetime));
				hitTime = FindModelSafetime(stmodel, startTM, endTM, stactor);	 // Returns between 0.0 and 1.0
				unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_FindModelSafetime));

				if (hitTime * partStepSize < safeTime)
				{
					stbody->keaBody.vel[0] /= 10.0f;
					stbody->keaBody.vel[1] /= 10.0f;
					stbody->keaBody.vel[2] /= 10.0f;
				}

				// If the time we hit something is less than our current safe time, make this our step size.
				safeTime = MeMIN(safeTime, hitTime * partStepSize);		
			}

			/* Ensure safetime is greater than world min safe time */
			safeTime = MeMAX(safeTime, w->minSafeTime);
			
#if 0
			if(safeTime < partStepSize)
			{
				KGData->bAutoEvolve = 0;
				debugf(TEXT("Timestep Reduced From %f to %f."), partStepSize, safeTime);
			}
#endif
			
			// Run integrator to update body positions.
			// We use the entire
			KIntegrateSystem(keabodyArray, 
				keatmArray,
				po->bodiesSize[partitionindex], 
				safeTime);
		}
		///////// INTEGRATE WITHOUT USING SAFE-TIME /////////
		else
		{
			MdtKeaIntegrateSystem(keabodyArray, 
				keatmArray, 
				po->bodiesSize[partitionindex], 
				w->keaParams);
		}

		unguard; // INTEGRATE

        /* We are done with Kea's workspace now - put back into MeChunk. */        
        MeChunkPutMem(&(w->keaPool), w->keaParams.memory_pool);        

        /* Copy transforms from Kea output back to MdtBody structs. */
        MdtUnpackBodies(keatmArray, partitionindex, po);
        
        /* 
            Copy resulting forces from output to Mdt constraint structs.
            There is a subtlety here. Because one MdtContactGroup can generate
            multiple MdtKea consraints, we have to count how many constraints 
            we have read forces from out of the MdtKeaConstraints.
        */
        
        conCount = MdtUnpackForces(keaCon->force, partitionindex, po);
		if(conCount != keaCon->num_constraints)
		{
			debugf(TEXT("(Karma): conCount (%d) != keaCon->num_constraints (%d)"), 
				conCount, keaCon->num_constraints);
		}
        check(conCount == keaCon->num_constraints);
        
		// Defer calling postKarmaStep until outside the loop. Don't add this partitions actors if there
		// was a sim error. We should NEVER have an actor in both PostStepActors and DeathRow.
		if(!wasError)
		{
			AddPostBodyStep(PostStepActors, &po->bodies[partStart], po->bodiesSize[partitionindex]);
			AddPostConstraintStep(PostStepActors, &po->constraints[conStart], po->constraintsSize[partitionindex]);
		}

        /* Finished with KeaConstraints - return to MeChunk. */
        MeChunkPutMem(&w->keaConstraintsChunk, keaCon);
        
        /* Finished with keatmArray, so we  return it to its Chunk. */
        MeChunkPutMem(&w->keaTMChunk, keatmArray);
    } // FOR PARTITIONINDEX < NPARTITIONS

	unguard; // PROCESS PARTITIONS

    /* We are done with the output of the partitioner, return to MeChunk. */
    MeChunkPutMem(&w->partOutChunk, po);

    /* Finally, reset forces on enabled bodies. */
    dict = &w->enabledBodyDict;
    for(node = MeDictFirst(dict); node != 0; node = MeDictNext(dict, node))
    {
        b = (MdtBodyID)MeDictNodeGet(node);
        check(b->flags & MdtEntityEnabledFlag); /* Check its enabled */
        MdtBodyResetForces(b);

        /* Reset safe-time to inifinity (no upper limit for next frame) */
        b->safeTime = MEINFINITY; 
    }

#if 1
	// We should NEVER have an actor in both PostStepActors and DeathRow. That would mean an actor was in
	// two different partitions. This is some temporary code to check this invariant.
	for(INT i=0; i<PostStepActors.Num(); i++)
	{
		AActor* psActor = PostStepActors(i);
		for(INT j=0; j<DeathRow.Num(); j++)
		{
			check(psActor != DeathRow(j));
		}
	}
#endif

	guard(PostStepAndDestroy);

	// Call any PostKarmaSteps.
	clock(GStats.DWORDStats(GEngineStats.STATS_Karma_PostKarmaStep));
	for(INT i=0; i<PostStepActors.Num(); i++)
	{
		if(PostStepActors(i) && !PostStepActors(i)->bDeleteMe)
		{
			PostStepActors(i)->postKarmaStep();
			
			if (bFinalSubStep)
			{
				AActor* Actor = PostStepActors(i);
				FMatrix BaseTM = FRotationMatrix(Actor->Rotation) * FTranslationMatrix(Actor->Location);
				for (INT i = 0; i < Actor->Attached.Num(); i++)
				{
					AActor* Other = Actor->Attached(i);
					if (Other && Other->bDetailAttachment && !Other->bCollideActors)
					{
						FMatrix NewWorldTM = Other->HardRelMatrix * BaseTM;
						FRotator NewWorldRot = (GMath.UnitCoords * NewWorldTM.Coords()).OrthoRotation();
						FVector NewWorldPos(NewWorldTM.M[3][0], NewWorldTM.M[3][1], NewWorldTM.M[3][2]);
						level->FarMoveActor(Other, NewWorldPos, false, true, true);
						Other->Rotation = NewWorldRot;
					}
				}
			}
		}
	}
	unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_PostKarmaStep));

	// Kill any actors who had sim-errors during the simulation
	for(INT i=0; i<DeathRow.Num(); i++)
	{
		if(DeathRow(i) && !DeathRow(i)->bDeleteMe)
			level->DestroyActor(DeathRow(i));
	}

	unguard;

	unguard;
}

#endif // WITH_KARMA

