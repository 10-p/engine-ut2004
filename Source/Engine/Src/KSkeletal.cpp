/*============================================================================
	Karma Farfield
    
    - Code for generating/handling pairs of McdModels as startand stop 
	  overlapping.
============================================================================*/

#include "EnginePrivate.h"

#ifdef WITH_KARMA

#define ME_ASSERT_INERTIASCALE ((MeReal)1)

#define PROFILE_INITSKEL (0)


static char *MEAPI CreateHashKey(const char *s)
{
    char *key = (char*)MeMemoryAPI.create(strlen(s) + 1);
    strcpy(key, s);
    return key;
}

/////////////////////////////////// ASSET INSTANCE ////////////////////////////////////////////////


/**
 * Create an Karma instance of a MeFAsset database structure.
 * The relevant contents of the MeFAsset structure will be
 * converted into McdGeometry, McdModels, MdtBody's and MdtConstraints.
 * 
 * If @param owner is true, these Karma objects will be destroyed
 * (@see MeAssetInstanceDestroy). If @param owner is false, the
 * user is responsible for destroying them.
 *
 * A transform can be passed in to position the whole asset in
 * world space.
 */
static MeAssetInstance* KAssetInstanceCreate(MeAssetFactory *af, 
                                             const MeFAsset *asset, 
                                             MeMatrix4Ptr tm, 
                                             MeBool owner, 
                                             MdtWorldID world,
											 TMap<KModelPairType, UBOOL>& DisableTable)
{
	guard(KAssetInstanceCreate);

    MeAssetInstance *ins = (MeAssetInstance*)MeMemoryAPI.create(sizeof(MeAssetInstance));
    
    const char *assetName = MeFAssetGetName(asset);
    char *key = 0;

    ins->af = af;
    ins->asset = (MeFAsset*)asset;
    ins->owner = owner;
    ins->world = world;
    ins->space = 0;
    ins->nameToGeometry = MeHashCreate(17);
    ins->nameToModel = MeHashCreate(17);
    ins->nameToJoint = MeHashCreate(17);
    
    MeHashSetKeyFreeFunc(ins->nameToGeometry, MeMemoryAPI.destroy);
    MeHashSetKeyFreeFunc(ins->nameToModel, MeMemoryAPI.destroy);
    MeHashSetKeyFreeFunc(ins->nameToJoint, MeMemoryAPI.destroy);

	KSetSecName(TEXT("KARMA: ASSET INSTANCE: GEOM"));
    {
        MeFGeometryIt it;
        MeFGeometry *geom;

        MeFAssetInitGeometryIterator(asset, &it);
		geom = MeFAssetGetGeometry(&it);
        while (geom)
        {
            McdGeometryID mcdGeom = McdGMCreateGeometry(ins->af->gm, geom, assetName);

			// BUG WORKAROUND
			// If we do not 'own' the geometry, we should not count our reference to it!
			if(owner)
				McdGeometryIncrementReferenceCount(mcdGeom);

            if (af->geometryPostCreateCB)
                af->geometryPostCreateCB(mcdGeom, geom, af->geometryPostCreateCBUserdata);

            key = CreateHashKey(MeFGeometryGetName(geom));
            MeHashInsert(key, mcdGeom, ins->nameToGeometry);

			geom = MeFAssetGetGeometry(&it);
        }
    }

	KSetSecName(TEXT("KARMA: ASSET INSTANCE: MODEL"));

    {
        int i;
        int count = MeFAssetGetPartCount(asset);
        MeFAssetPart **part = (MeFAssetPart**)MeMemoryALLOCA(sizeof(MeFAssetPart*) * count);
        MeFAssetGetPartsSortedByName(asset, part);
        
        for (i = 0; i < count; i++)
        {
            McdModelID model = 0;
            McdGeometryID geom;
            MeFModel *fmodel = MeFAssetPartGetModel(part[i]);
            MeFGeometry *fgeom = MeFModelGetGeometry(fmodel);

            if (MeFModelGetType(fmodel) != kMeFModelTypeDynamicsOnly && fgeom)
                geom = (McdGeometryID)MeHashLookup(MeFGeometryGetName(fgeom), ins->nameToGeometry);
            else
                geom = McdGMGetNullGeometry(af->gm);
        
            key = CreateHashKey(MeFAssetPartGetName(part[i]));

            if (af->modelCreateFunc)
                model = af->modelCreateFunc(part[i], geom, world, tm);

            if (af->modelPostCreateCB)
                af->modelPostCreateCB(model, part[i], af->modelPostCreateCBUserdata);

            MeHashInsert(key, model, ins->nameToModel);
        }

        /* disable collision pairs */
        for (i = 0; i < count; i++)
        {
            int j;
            for (j = i + 1; j < count; j++)
            {
                if (!MeFAssetPartIsCollisionEnabled(part[i], part[j]))
                {
                    McdModelID model1 = (McdModelID)MeHashLookup(MeFAssetPartGetName(part[i]), ins->nameToModel);
                    McdModelID model2 = (McdModelID)MeHashLookup(MeFAssetPartGetName(part[j]), ins->nameToModel);
                    
                    if (model1 && model2)
                    {
						// If both are 'real' geometry, we add this pair to the 'disable table' for this skeleton.
						if( McdGeometryGetTypeId(McdModelGetGeometry(model1)) != kMcdGeometryTypeNull &&
							McdGeometryGetTypeId(McdModelGetGeometry(model2)) != kMcdGeometryTypeNull)
						{
							KModelPairType Key = KModelsToKey(model1, model2);
							DisableTable.Set(Key, 0);
						}
                    }
#ifdef _MECHECK
                    else
                    {
                        ME_REPORT(MeWarning(3,"MeAssetInstanceCreate: Invalid collision pair."));
                    }
#endif
                }
            }
        }
    }

	KSetSecName(TEXT("KARMA: ASSET INSTANCE: JOINT"));
        
    {
        MeFJointIt it;
        MeFJoint *joint;
        MeFAssetInitJointIterator(asset, &it);
		joint = MeFAssetGetJoint(&it);

        while (joint)
        {
            MdtConstraintID constraint = 0;
            McdModelID model1 = 0, model2 = 0;
            MeFAssetPart *part1, *part2;
    
            part1 = MeFJointGetPart(joint, 0);
            part2 = MeFJointGetPart(joint, 1);
        
            if (part1)
                model1 = (McdModelID)MeHashLookup(MeFAssetPartGetName(part1), ins->nameToModel);

            if (part2)
                model2 = (McdModelID)MeHashLookup(MeFAssetPartGetName(part2), ins->nameToModel);

            if ( (model1 && McdModelGetBody(model1) ) || (model2 && McdModelGetBody(model2)) )
            {
                if (af->jointCreateFunc)
                    constraint = af->jointCreateFunc(joint, world, model1, model2, tm);

                if (constraint)
                    MdtConstraintEnable(constraint);
                
                if (af->jointPostCreateCBUserdata)
                    af->jointPostCreateCB(constraint, joint, af->jointPostCreateCBUserdata);

                key = CreateHashKey(MeFJointGetName(joint));
                MeHashInsert(key, constraint, ins->nameToJoint);
            }

			joint = MeFAssetGetJoint(&it);
        }
    }
    
	KSetSecName(TEXT("KARMA: ASSET INSTANCE: FINISH"));


    return ins;

	unguard;
}

// // //

McdModelID MEAPI KModelCreateFromMeFAssetPart(MeFAssetPart *part, McdGeometryID g, MdtWorldID world, MeMatrix4Ptr assetTM)
{
	guard(KModelCreateFromMeFAssetPart);

    MeFModel *fmodel = MeFAssetPartGetModel(part);
    MeFModelType type = MeFModelGetType(fmodel);
    McdModelID model = McdModelCreate(g);
    MeMatrix4Ptr modelTM;
    MdtBodyID body = 0;
    
    if (type == kMeFModelTypeGeometryOnly)
    {
        modelTM = (MeMatrix4Ptr)MeMemoryAPI.createAligned(sizeof(MeMatrix4), 16);
        McdModelSetTransformPtr(model, modelTM);
    }
    else
    {
        body = MdtBodyCreate(world);
        modelTM = MdtBodyGetTransformPtr(body);
    }

    if(assetTM)
    {
        MeMatrix4MultiplyMatrix(modelTM, 
            MeFAssetPartGetTransformPtr(part), assetTM);
    }
    else /* Just create part relative to origin. */
    {
        MeMatrix4Copy(modelTM, MeFAssetPartGetTransformPtr(part));
    }

    if (type == kMeFModelTypeDynamicsAndGeometry || type == kMeFModelTypeDynamicsOnly)
    {
        MdtBodyEnable(body);
		MdtBodySetTransform(body, modelTM);
        KBodySetMass(body, MeFModelGetMass(fmodel));
        {
            MeVector3 pos;
            MeFModelGetMassOffset(fmodel, pos);
            MdtBodySetCenterOfMassRelativePosition(body,pos);
        }
        MdtBodyEnableNonSphericalInertia(body);
        MdtBodyEnableCoriolisForce(body);

        {
            MeMatrix3 I;
            MeFModelGetInertiaTensor(fmodel, I);
			MeMatrix3Scale(I, ME_ASSERT_INERTIASCALE);
            KBodySetInertiaTensor(body, I);
        }

        MdtBodySetLinearVelocityDamping(body,
            MeFModelGetLinearVelocityDamping(fmodel));
        MdtBodySetAngularVelocityDamping(body,
            MeFModelGetAngularVelocityDamping(fmodel));

        if (MeFModelIsFastSpinAxisEnabled(fmodel))
        {
            MeVector3 axis;
            MeFModelGetFastSpinAxis(fmodel, axis);
            MdtBodySetFastSpinAxis(body, axis[0], axis[1], axis[2]);
        }
        McdModelSetBody(model, body);
    }

    return model;

	unguard;
}

///////////////////////////////////////////////////////////////////////////////////

FKBoneLifter::FKBoneLifter(DWORD InBoneIndex , FInterpCurve* InLiftVel, FLOAT InLateralFriction, FInterpCurve* InSoftness)
{
	BoneIndex = InBoneIndex;
	LateralFriction = InLateralFriction;

	// We have to zero these before copying into them. Yuck...
	appMemzero( &LiftVel, sizeof(LiftVel) ); 
	appMemzero( &Softness, sizeof(Softness) );

	LiftVel = *InLiftVel;
	Softness = *InSoftness;

	CurrentTime = 0.f;
	LiftContact = 0;
}	

///////////////////////////////////////////////////////////////////////////////////


/*  Take this skeletal mesh instance, try and find a Karma asset for it. Each bone needs a 
    model and a joint (except for root) for each bone. We create our own AssetParts, 
    because we need to set each model the current bone position, but use the existing 
    AssetJoints as they are specified in local coords. Model-bone and joint-bone association 
    is done on name (SkeletalMeshName.BoneName) expected in the AssetDB. A subset of the entire 
    heirarchy may be created as physics, but it must be complete (ie no gaps on route to root),
    and joints can only be created to the model of the parent bone. Also, joints are assumed
    to have child as body1 and parent as body2.
*/
void MEAPI KInitSkeletonKarma(USkeletalMeshInstance* inst)
{
	guard(KInitSkeletonKarma);

#if PROFILE_INITSKEL
	DWORD Timer = 0;
	clock(Timer);
#endif

	AActor* actor = inst->GetActor();
	ULevel* level = actor->GetLevel();

	// If no ULevel/KGData, or in editor, or it has already been initialised... do nothing
	if(!KGData || !level || GIsEditor)
		return;

	RTN_WITH_ERR_IF(actor->Physics != PHYS_KarmaRagDoll, 
		"(Karma): KInitSkeletonKarma: Physics mode not PHYS_KarmaRagDoll.");

	if(inst->KSkelIsInitialised)
		return;

	USkeletalMesh* skelmesh = Cast<USkeletalMesh>(inst->GetMesh());
	MdtWorldID world = level->KWorld;

	RTN_WITH_ERR_IF(!world, "(Karma): KInitSkeletonKarma: Framework, but no world.");
	RTN_WITH_ERR_IF(!actor->KParams || !actor->KParams->IsA(UKarmaParamsSkel::StaticClass()), 
		"(Karma): KInitSkeletonKarma: No KParams, or KParams is not a KarmaParamsSkel.");

	UKarmaParamsSkel* skelParams = Cast<UKarmaParamsSkel>(actor->KParams);

	/* Get skeletal mesh name and look up asset. */
	char cSkelName[512];

    /* appToAnsi is the engine's own TCHAR->ANSI conversion and is correct on every target
       whatever TCHAR is. wcstombs is not: UNICODE builds (native Linux) have a 2-byte UNICHAR
       TCHAR, and glibc's wchar_t is 4 bytes, so the skeleton name would be noise and every
       ragdoll would fail to initialise. */
	strncpy(cSkelName, appToAnsi(*skelParams->KSkeleton), 512);
	cSkelName[511] = 0;

	int jointCount = 0, modelCount = 0, boneIdx, numBones = skelmesh->RefSkeleton.Num();

	// Update skeleton to make sure bones are all in the right places.
	INT DummyVerts;
	inst->GetFrame( actor, NULL, NULL, 0, DummyVerts, GF_BonesOnly);

	//MeFAsset  *skelAss = MeAssetDBLookupAsset(KGData->AssetDB, cSkelName);
	MeFAsset  *skelAss = MeAssetDBLookupAssetByName(KGData->AssetDB, cSkelName);
	RTN_WITH_ERR_IF(!skelAss, "(Karma): KInitSkeletonKarma: Asset not found.");

	// Remove actor from collision hash before doing anything
	// If we change the collision box before we remove it, we leave fragments.
	guard(Disable Collision);
	actor->SetCollision(0, 0);
	unguard;

	// This isn't that effecient - we rarely have ALL the bones physical.
	inst->KSkelModels.AddZeroed(numBones);
	inst->KSkelJoints.AddZeroed(numBones);
	//inst->KSkelLimits.AddZeroed(numBones);

	//  Used to tell the animation system to leave this bone position
	//	completely up to the Director (not add to default pose).
	inst->NoRefPose.AddZeroed(numBones);
	inst->bForceRefpose = 1;

	//	Instance the skeleton asset.
	//	Models/joints are not 'owned' by Asset Instance, 
	//	so are not destroyed when the MeAssetInstance is.
	//	Feed in no transform, because we set each bones position from the graphics anyway.
	MeAssetInstance *skelAssIns;

#if PROFILE_INITSKEL
	unclock(Timer);
	FLOAT startTime = Timer * GSecondsPerCycle * 1000.0f;
	Timer = 0;
	clock(Timer);
#endif

	KSetSecName(TEXT("KARMA: ASSET INSTANCE"));
	guard(MeAssetInstanceCreate);
	skelAssIns = KAssetInstanceCreate(level->KAssetFactory, skelAss, 0, 0, world, inst->KSkelDisableTable);
	unguard;

#if PROFILE_INITSKEL
	unclock(Timer);
	FLOAT assetInstanceTime = Timer * GSecondsPerCycle * 1000.0f;
	Timer = 0;
	clock(Timer);
#endif

	KSetSecName(TEXT("KARMA: SKEL BONE HOOKUP"));
	guard(BoneHookup);

	// reset bounding box
	FBox TempSkelBox(0); 

	//	Now, for each graphics bone, see if there is a physics model/joint.
	//	If there is, set its position according to the graphics skeleton, and store it.
	UBOOL atRoot = true;
	for(boneIdx = 0; boneIdx<numBones; boneIdx++)
	{
		char cBoneName[512];
		// appToAnsi, as for cSkelName above.
		strncpy(cBoneName, appToAnsi(*skelmesh->RefSkeleton(boneIdx).Name), 512);
		cBoneName[511] = 0;

		strlwr(cBoneName); // Assume part names are always lower case.

		McdModelID boneModel = MeAssetInstanceGetModel(skelAssIns, cBoneName);
		MdtConstraintID boneJoint = MeAssetInstanceGetJoint(skelAssIns, cBoneName);

		// If there is no physics for this bone, move on.
		if(!boneModel)
			continue;

		modelCount++;

		// Allocate and set model UserData.
		KarmaModelUserData* data = new(KarmaModelUserData);
		data->actor = actor;
		McdModelSetUserData(boneModel, (void*)data);

		// When we hit the physics root, if its not the graphics root bone,
		// set all bones above to have zero transform.
		// Dont need to set this bones flag though - thats done at the bottom like normal bones.
		if(atRoot)
		{
			INT tmpBoneIdx = boneIdx;
			while(tmpBoneIdx != 0)
			{
				INT parentIdx = skelmesh->RefSkeleton(tmpBoneIdx).ParentIndex;
				inst->NoRefPose(parentIdx) = 1;
				tmpBoneIdx = parentIdx;
			}

			atRoot = false; // dealt with phys root - no others are.
		}

		MdtBodyID boneBody = McdModelGetBody(boneModel);

		// Get bone transform in world space.
		MeMatrix4 boneTM;
		FCoords BoneCoords = inst->GetBoneCoords(boneIdx);
		KU2METransform(boneTM, BoneCoords.Origin, BoneCoords.OrthoRotation());

		// Set physics body/model transform position to match graphics
		if(boneBody)
		{
			// Set physics body to location of graphics bone.
			MdtBodySetTransform(boneBody, boneTM);

			// For skeletons we are going to play safe and make sure we are using spherical inertia tensor.
			MdtBodyEnableNonSphericalInertia(boneBody);
			//MdtBodyDisableNonSphericalInertia(boneBody);
			MdtBodyDisableCoriolisForce(boneBody);

			MdtBodySetLinearVelocityDamping(boneBody, skelParams->KLinearDamping);
			MdtBodySetAngularVelocityDamping(boneBody, skelParams->KAngularDamping);

			// Start enabled if desired
			// Need to check if its in a space - might be null geometry.
			// We do the Space(Un)Freeze explicitly to make sure body and model are in sync
			if(skelParams->KStartEnabled)
			{
				MdtBodyEnable(boneBody);
			}
			else
			{
				MdtBodyDisable(boneBody);
			}
		}
		else
		{
			MeMatrix4Ptr boneModelTM = McdModelGetTransformPtr(boneModel);
			MeMatrix4Copy(boneModelTM, boneTM);
		}

		// Always enable joints!
		if(boneJoint)
		{
			if(!MdtConstraintIsEnabled(boneJoint))
				MdtConstraintEnable(boneJoint);

			// HACK! Karma bug workaround.
			// Hinges seem to assume they are at zero position for first tick. Annoying
			MdtHingeID hinge = MdtConstraintDCastHinge(boneJoint);
			if(hinge)
				hinge->limit.bPositionInitialised = 1;
			// END HACK

			jointCount++;

			// Save reference to bone and joint
			inst->KSkelJoints(boneIdx) = boneJoint;
			//MdtConstraintSetUserData(boneJoint, (void*)inst);
		}

		// Set model in instance's array of models.
		inst->KSkelModels(boneIdx) = boneModel;

		// We will use Directors to completely control this bone relative to parent.
		inst->NoRefPose(boneIdx) = 1;

		McdGeometryID geom = McdModelGetGeometry(boneModel);
		if(McdGeometryGetTypeId(geom) != kMcdGeometryTypeNull)
		{
			// Add to skel temporary bounding box
			MeVector3 mMin, mMax;
			FVector tmpVec;

			// Make sure model (AABB etc.) is up to date
			McdModelUpdate(boneModel);

			McdModelGetAABB(boneModel, mMin, mMax);
			KME2UPosition(&tmpVec, mMin);
			TempSkelBox += tmpVec;
			KME2UPosition(&tmpVec, mMax);
			TempSkelBox += tmpVec;
		}

		KCalcModelSafeTimeSpheres(boneModel);
	}

	inst->KSkelBox = TempSkelBox;

	unguard;

#if PROFILE_INITSKEL
	unclock(Timer);
	FLOAT hookupTime = Timer * GSecondsPerCycle * 1000.0f;
	Timer = 0;
	clock(Timer);
#endif

	if(modelCount != MeFAssetGetPartCount(skelAss))
		debugf(TEXT("(Karma): Not All Physics Parts Have Graphics Bones!"));

	if(jointCount != MeFAssetGetJointCount(skelAss))
		debugf(TEXT("(Karma): Not All Physics Joints Have Graphics Bones!"));

	// BUG WORKAROUND. 
	// AssetInstanceCreate seems to create geometry, even if its not used.
	// So we find any unsued ones, and clean them up straight away.
	TArray<McdGeometryID> UnusedGeom;
	MeAIGeomIt it;
	McdGeometryID geom;

	MeAssetInstanceInitGeometryIterator(skelAssIns, &it);
	geom = MeAssetInstanceGetNextGeometry(&it);
	while(geom)
	{
		if(McdGeometryGetReferenceCount(geom) == 0)
			UnusedGeom.AddItem(geom);

		geom = MeAssetInstanceGetNextGeometry(&it);
	}

	// Destroy instance struct (but not models/joints)
	MeAssetInstanceDestroy(skelAssIns);

	for(INT i=0; i<UnusedGeom.Num(); i++)
	{
		KGMDestroyGeometry(KGData->GeomMan, UnusedGeom(i));
	}

	//debugf(TEXT("(Karma): Skeleton (%s) sucessfully initialised (%d bones) from Karma Asset!"),	*(skelParams->KSkeleton), modelCount);

	// Store root physics bone index and and last (in array) physics bone index.
	inst->KPhysRootIndex = INDEX_NONE;
	inst->KPhysLastIndex = INDEX_NONE;
	for(INT i=0; i<inst->KSkelModels.Num(); i++)
	{
		if(inst->KSkelModels(i))
		{
			if(inst->KPhysRootIndex == INDEX_NONE)
				inst->KPhysRootIndex = i;

			inst->KPhysLastIndex = i;
		}
	}
	check(inst->KPhysRootIndex != INDEX_NONE && inst->KPhysLastIndex != INDEX_NONE);

	// Ensure no transform between actor and mesh origin during rag-doll.
	inst->bIgnoreMeshOffset = true;

	inst->KSkelIsInitialised = 1;

	// Initialise the 'last frame' velocity magnitude as infinity.
	// So if the very first frame's velocity is low, the KVelDropBelow event will be triggered.
	skelParams->KLastVel = MEINFINITY;

	// Now turn collision back on, with the new bounding box.
	guard(Reenable Collision);
	actor->SetCollision(1, 0);
	unguard;

	actor->bCollideWorld = 0;
	actor->bProjTarget = 1;

	KGData->ModelCount += modelCount;

	// Set collision correctly for this actor (will add to KActorContactGen list etc.)
	KSetActorCollision(actor, actor->bBlockKarma);

	// Add to array of active ragdolls.
	actor->XLevel->Ragdolls.AddItem(actor);

	// Give this rag-doll a triangle list to fill in each frame.
	KarmaTriListData* list;

	// If we have some left in our pool, re-use that..
	if(actor->XLevel->TriListPool.Num() > 0)
	{
		list = actor->XLevel->TriListPool(0);
		actor->XLevel->TriListPool.Remove(0);
	}
	else // Otherwise, allocate a new one now.
	{
		list = (KarmaTriListData*)appMalloc(sizeof(KarmaTriListData), TEXT("RAGDOLL TRILIST"));
	}

	check(list);
	KarmaTriListDataInit(list);
	skelParams->KTriList = (PTRINT)list;

	// DEFERRED VELOCITY AND IMPULSE ETC.

	// Set initial angular and linear velocity.
	KSetSkelVel(inst, skelParams->KStartLinVel, skelParams->KStartAngVel);

	// If we are applying a shot to a bone at startup, do the line check here.
	if(skelParams->KShotStrength > 0)
	{
		FCheckResult res(1);
		UBOOL noHit = inst->LineCheck(res, actor, skelParams->KShotEnd, skelParams->KShotStart, FVector(0, 0, 0), 0, 0);
		if(!noHit && inst->KLastTraceHit != -1)
		{
			check(inst->KLastTraceHit >= inst->KPhysRootIndex && inst->KLastTraceHit <= inst->KPhysLastIndex);

			McdModelID hitBoneModel = inst->KSkelModels(inst->KLastTraceHit);
			check(hitBoneModel);
			MdtBodyID hitBoneBody = McdModelGetBody(hitBoneModel);
			if(hitBoneBody)
			{
				// Scale both to ME sizes - for both mass AND velocity!

				FVector shotImpulse = (skelParams->KShotEnd - skelParams->KShotStart).SafeNormal();
				shotImpulse *= (K_U2MEMassScale * skelParams->KShotStrength);

				MeVector3 kimpulse;
				KU2MEPosition(kimpulse, shotImpulse);
				MdtBodyAddImpulse(hitBoneBody, kimpulse[0], kimpulse[1], kimpulse[2]);
				MdtBodyEnable(hitBoneBody);
			}
		}
	}

	inst->KRagdollAge = 0.f;

	if ( skelParams->bRubbery )
		KScaleJointLimits(inst, 5.f, 500000000.f);

	// If desired, schedule first convulsion (give it at least 1 second though)
	if(skelParams->bKDoConvulsions)
		inst->KNextConvulsionTime = inst->KRagdollAge + 1.0f + skelParams->KConvulseSpacing.GetRand();

#if PROFILE_INITSKEL
	unclock(Timer);
	FLOAT finishTime = Timer * GSecondsPerCycle * 1000.0f;

	debugf(TEXT("Start: %f Instance: %f Hookup: %f Finish: %f [TOTAL: %f]"), 
		startTime, assetInstanceTime, hookupTime, finishTime,
		startTime + assetInstanceTime + hookupTime + finishTime);
#endif

	KSetSecName(TEXT("KARMA: POST INIT SKEL"));

	unguard;
}

/* Remove all dynamics and collision from this skeleton. */
void MEAPI KTermSkeletonKarma(USkeletalMeshInstance* inst)
{
    guard(KTermSkeletonKarma);
	check(inst);

    // Check if we have to do anything!
	guard(NotInitialised);
    if(!inst->KSkelIsInitialised)
	{
#if 1
		// TEMPORARY
		for(INT i=0; i<inst->KSkelModels.Num(); i++)
		{
			if(inst->KSkelModels(i) != 0)
				debugf(TEXT("(Karma:) KTermSkeletonKarma (NI): KSkelModels Not Empty!"));
		}

		for(INT i=0; i<inst->KSkelJoints.Num(); i++)
		{
			if(inst->KSkelJoints(i) != 0)
				debugf(TEXT("(Karma:) KTermSkeletonKarma (NI): KSkelJoints Not Empty!"));
		}
#endif

		return;
	}
	unguard;

	USkeletalMesh* smesh;
	AActor* actor;
	ULevel* level;

	guard(Start);

	if(!KGData) 
        return;

    KSetSecName(TEXT("KARMA: TERM SKEL"));

	smesh = Cast<USkeletalMesh>(inst->GetMesh());
	check(smesh);

	actor = inst->GetActor();
	check(actor);

    level = actor->GetLevel();
	check(level);

	unguard;


	guard(RemoveActor);
	// The actor might already have been removed from the octree, so don't try and remove it again!
	if(actor->bCollideActors && level && level->Hash && actor->IsInOctree())
		level->Hash->RemoveActor(actor);
	unguard;

	// Put trilist back into pool.
	guard(TriList);

	check(actor->KParams);
	UKarmaParamsSkel* skelParams = Cast<UKarmaParamsSkel>(actor->KParams);
	check(skelParams);

	if(skelParams->KTriList)
	{
		level->TriListPool.AddItem((KarmaTriListData*)skelParams->KTriList);
		skelParams->KTriList = 0;
	}
	else
	{
		debugf(TEXT("(Karma:) KTermSkeletonKarma: No Tri-list found for ragdoll."));
	}

	unguard;

	guard(Lifters);

	// Remove any bone forces (including lifting contacts)
	// Need to do this before destroying bone bodies.
	for(INT i=0; i<inst->KBoneLifters.Num(); i++)
	{
		MdtContactGroupID cg = inst->KBoneLifters(i).LiftContact;
		check(cg);
		MdtContactGroupDisable(cg);
		MdtContactGroupDestroy(cg);
	}
	inst->KBoneLifters.Empty();

	unguard;

	// copious checking...
	check(inst->KPhysRootIndex >= 0 && inst->KPhysRootIndex < inst->KSkelModels.Num());
	check(inst->KPhysLastIndex >= 0 && inst->KPhysLastIndex < inst->KSkelModels.Num());

	// First remove joints.
	// JTODO: Should be ok not doing this, because destroying a body destroys its constraints. But need to check.
	guard(Joints);
	for(INT i=inst->KPhysRootIndex; i<=inst->KPhysLastIndex; i++)
	{
        MdtConstraintID con = inst->KSkelJoints(i);
		if( con )
		{
			MdtConstraintDisable(con);
			MdtConstraintDestroy(con);
			inst->KSkelJoints(i) = 0;
		}
	}
	unguard;

	// Then remove collision and dynamics.
	guard(Models);
	for(INT i=inst->KPhysRootIndex; i<=inst->KPhysLastIndex; i++)
	{
        // If is a model, and we are a child of the desired bone, remove it.
		// Note, if we are removing all, we can skip the IsChildOf test!
		McdModelID model = inst->KSkelModels(i);
		if( model )
		{
			// Then remove physics and collision.
			MdtBodyID body;
			MeMatrix4Ptr tm = 0;

			KGoodbyeAffectedPairs(model, level);

			body = McdModelGetBody(model);
			if(body)
			{
				// Make sure there are no active AKConstraints attached to this body.
				KBodyTermKConstraints(body);

				MdtBodyDisable(body);
				MdtBodyDestroy(body);
			}
			else
			{
				tm = McdModelGetTransformPtr(model);
			}

			McdGeometryID geom = McdModelGetGeometry(model);

			KarmaModelUserData* data = (KarmaModelUserData*)McdModelGetUserData(model);
			check(data->actor == actor);
			check(data->OverlapModels.Num() == 0); // Check we have been properly removed from any pairs.
			delete data;

			McdModelDestroy(model);
			(KGData->ModelCount)--;

			if(tm)
				MeMemoryAPI.destroyAligned(tm);

			// Try and free the geometry. Will do nothing if its still in use.
			if(McdGeometryGetTypeId(geom) != kMcdGeometryTypeNull)
			{
				KGMDestroyGeometry(KGData->GeomMan, geom);
			}

			inst->KSkelModels(i) = 0;

			KTermGameKarma(); // 'Try' and shut down.
		}
	}
	unguard;

#if 1
	guard(CheckEmpty);
	// TEMPORARY
	for(INT i=0; i<inst->KSkelModels.Num(); i++)
	{
		if(inst->KSkelModels(i) != 0)
			debugf(TEXT("(Karma:) KTermSkeletonKarma (RA): KSkelModels Not Empty!"));
	}

	for(INT i=0; i<inst->KSkelJoints.Num(); i++)
	{
		if(inst->KSkelJoints(i) != 0)
			debugf(TEXT("(Karma:) KTermSkeletonKarma (RA): KSkelJoints Not Empty!"));
	}
	unguard;
#endif

	guard(Empty);
	// Probably dont need to bother with inst...
	inst->KSkelJoints.Empty();
	inst->KSkelModels.Empty();

	// Leave these as they are, so that if we are freezing the ragdoll, it will still render correctly.
	// The 'directors' will persist.

	//inst->NoRefPose.Empty();
	//inst->bIgnoreMeshOffset = 0;

	inst->KSkelIsInitialised = 0;
	unguard;


	guard(AddActor);
	if(actor->bCollideActors && level && level->Hash)
		level->Hash->AddActor(actor);
	unguard;

	KSetSecName(TEXT("KARMA: POST TERM SKEL"));

	guard(RemoveFromRagdollList);
	check(actor->XLevel->Ragdolls.Num() > 0);
	INT ragIx = actor->XLevel->Ragdolls.FindItemIndex(actor);
	check(ragIx != INDEX_NONE);
	actor->XLevel->Ragdolls.Remove(ragIx);
	unguard;
    
	guard(KActorContactGen);
	if(actor->bCollideActors)
		KActorContactGen(actor, 0);
	unguard;

    unguard;
}

// Take a skeleton, and apply an overall velocity to it.
void KSetSkelVel(USkeletalMeshInstance* inst, FVector Velocity, FVector AngVelocity)
{
	guard(KSetSkelVel);

	AActor* actor = inst->GetActor();

	if(actor->Physics != PHYS_KarmaRagDoll)
		return;

	//USkeletalMesh* skelmesh = Cast<USkeletalMesh>(inst->GetMesh());

	if(!inst->KSkelIsInitialised)
		return;

	MeVector3 kVel, kAngVel, kActorCentre;
	KU2MEPosition(kVel, Velocity);
	KU2MEPosition(kActorCentre, actor->Location);
	kAngVel[0] = K_U2Rad * AngVelocity.X;
	kAngVel[1] = K_U2Rad * AngVelocity.Y;
	kAngVel[2] = K_U2Rad * AngVelocity.Z;

	for(INT i=0; i<inst->KSkelModels.Num(); i++)
	{
		McdModelID model = inst->KSkelModels(i);
		if(model)
		{
			MdtBodyID body = McdModelGetBody(model);
			if(body)
			{
				MeVector3 boneLinVel, bposRel, bpos, rotVel;
				MeVector3Copy(boneLinVel, kVel);

				// Add component of linear velocity due to angular velocity.

				// First, find position of bone centre-of-mass relative to actor centre.
				MdtBodyGetCenterOfMassPosition(body, bpos);
				MeVector3Subtract(bposRel, bpos, kActorCentre);

				// Then, cross product to find velocity of body.
				MeVector3Cross(rotVel, kAngVel, bposRel);
				//debugf(TEXT("%s boneLinVel: %f %f %f"), *(skelmesh->RefSkeleton(i).Name), boneLinVel[0], boneLinVel[1], boneLinVel[2]);
				//debugf(TEXT("rotVel: %f %f %f"), rotVel[0], rotVel[1], rotVel[2]);
				MeVector3Add(boneLinVel, boneLinVel, rotVel);

				// Apply to body
				MdtBodySetLinearVelocity(body, boneLinVel[0], boneLinVel[1], boneLinVel[2]);
				MdtBodySetAngularVelocity(body, kAngVel[0], kAngVel[1], kAngVel[2]);
			}
		}
	}

	unguard;
}


// Take the ragdoll actor, and update its list of potentially-colliding triangles.
void KUpdateRagdollTrilist(AActor* actor, UBOOL bDoubleRateActors)
{
	guard(KUpdateRagdollTrilist);

	ULevel* level = actor->GetLevel();

	UKarmaParams* kp = 0;
	if(actor->KParams) 
		kp = Cast<UKarmaParams>(actor->KParams);

	// Should always have a KarmaData and allocated tri-list memory if rag-doll physics is on.
	// check(kp && kp->KTriList); 
	if(!kp)
	{
		debugf(TEXT("(Karma:) UpdateRagdollTrilist: No KarmaParams."));
		return;
	}

	// Only update tri-list for the actors we want.
	if(kp->bKDoubleTickRate != bDoubleRateActors)
		return;

	USkeletalMeshInstance* inst = NULL;
	if(actor->Physics == PHYS_KarmaRagDoll && actor->Mesh && actor->Mesh->IsA(USkeletalMesh::StaticClass()))
    {
        USkeletalMesh* skelMesh = Cast<USkeletalMesh>(actor->Mesh);
        inst = Cast<USkeletalMeshInstance>(skelMesh->MeshGetInstance(actor));
	}

	if(!level || !inst || !inst->KSkelIsInitialised || actor->bDeleteMe)
		return;
		
	if(!kp->KTriList)
	{
		debugf(TEXT("(Karma:) UpdateRagdollTrilist: No Tri-list found for ragdoll."));
		return;
	}

	KarmaTriListData* triData = (KarmaTriListData*)kp->KTriList;

	// If this actor is not generating Karma contatcs, dont bother generating list!
	if(!actor->bBlockKarma)
	{
		KarmaTriListDataInit(triData);
		return;
	}

	// Find bounding sphere around entire rag-doll.
	FBox bbox = actor->GetPrimitive()->GetCollisionBoundingBox(actor);
	
	if(	bbox.IsValid == 0 ||
		bbox.Max.X > HALF_WORLD_MAX || bbox.Min.X < -HALF_WORLD_MAX ||
		bbox.Max.Y > HALF_WORLD_MAX || bbox.Min.Y < -HALF_WORLD_MAX ||
		bbox.Max.Z > HALF_WORLD_MAX || bbox.Min.Z < -HALF_WORLD_MAX )
	{
		debugf(TEXT("(Karma:) Ragdoll Bounding Box invalid or outside world."));
		return;
	}

    FSphere sphere = FSphere(&bbox.Min,2);

	// Then we actually do the query to update triangles near this ragdoll.
	clock(GStats.DWORDStats(GEngineStats.STATS_Karma_KTriListQuery));
	KTriListQuery(level, &sphere, triData);
	unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_KTriListQuery));

	unguard;
}

/////////////////////////// PRE/POST STEP SKELETAL ///////////////////////////////////

// - Apply gravity and buoyancy forces to each bone
// - Set damping for each bone
void AActor::preKarmaStep_skeletal(FLOAT DeltaTime)
{
	guard(AActor::preKarmaStep_skeletal);

	USkeletalMesh* skelMesh = 0;
	if(this->Mesh)
		skelMesh = Cast<USkeletalMesh>(this->Mesh);
	RTN_WITH_ERR_IF(!skelMesh, "No skelMesh.");

	USkeletalMeshInstance* inst = (USkeletalMeshInstance*)skelMesh->MeshGetInstance(this);
	RTN_WITH_ERR_IF(!inst, "No USkeletalMeshInstance.");

	UKarmaParamsSkel* kparams = Cast<UKarmaParamsSkel>(this->KParams);
	RTN_WITH_ERR_IF(!kparams, "(Karma:) preKarmaStep_skeletal: No KarmaParamsSkel.")

    MeVector3 gforce = {0, 0, 0};
	if(PhysicsVolume)
		KU2MEPosition(gforce, PhysicsVolume->Gravity);

	MeReal gravScale = ME_GRAVSCALE * kparams->KActorGravScale;
	if(Level)
		gravScale *= Level->KarmaGravScale;

	guard(UpdateBones);

	// The first physics bone we hit is our 'physics root'.
	// This is the body we use to figure out where to put the actor each frame.
	for(INT boneIdx = inst->KPhysRootIndex; boneIdx <= inst->KPhysLastIndex; boneIdx++)
    {
        if(!inst->KSkelModels(boneIdx))
            continue;
		
        MdtBodyID body = McdModelGetBody(inst->KSkelModels(boneIdx));

		guard(UpdateBoneForces);

		MeReal meMass = MdtBodyGetMass(body);
		MeReal buoyEffect = 0.f;
		MeReal linDamp = kparams->KLinearDamping;
		MeReal angDamp = kparams->KAngularDamping;

		if(PhysicsVolume->bWaterVolume)
		{
			buoyEffect = kparams->KBuoyancy;
			linDamp += PhysicsVolume->KExtraLinearDamping;
			angDamp += PhysicsVolume->KExtraAngularDamping;
		}

		// Set damping
		MdtBodySetLinearVelocityDamping(body, linDamp);
		MdtBodySetAngularVelocityDamping(body, angDamp);

		// Add forces (gravity)
		MdtBodyAddForce(body, 
			meMass * gforce[0] * gravScale, 
			meMass * gforce[1] * gravScale,
			meMass * (gforce[2] * (1-buoyEffect)) * gravScale);

		unguard; // 'UpdateBoneForces'
       
    } // FOR EACH BONE.

	unguard; // 'UpdateBones'

	unguard;
}

// - Move actor to correspond with physics root
// - Set each graphics bones relative location/rotation
// - Cap velocity
void AActor::postKarmaStep_skeletal()
{
	guard(AActor::postKarmaStep_skeletal);

	UKarmaParams* kparams = Cast<UKarmaParams>(this->KParams);
	if(!kparams)
		return;

	FMatrix grap2PhysRootTM; // Transform from skeleton
	FBox TempSkelBox(0);  // reset bounding box
	FVector RecipTotalScale(1,1,1);

	USkeletalMesh* skelMesh = 0;
	if(this->Mesh)
		skelMesh = Cast<USkeletalMesh>(this->Mesh);
	RTN_WITH_ERR_IF(!skelMesh, "postKarmaStep_skeletal: No skelMesh.");

	USkeletalMeshInstance* inst = (USkeletalMeshInstance*)skelMesh->MeshGetInstance(this);
	RTN_WITH_ERR_IF(!inst, "postKarmaStep_skeletal: No USkeletalMeshInstance.");

	RecipTotalScale /= (skelMesh->Scale * this->DrawScale * this->DrawScale3D);

	MeReal meMaxSpeed = K_U2MEScale * kparams->KMaxSpeed;

	guard(UpdateBones);

	// The first physics bone we hit is our 'physics root'.
	// This is the body we use to figure out where to put the actor each frame.
	UBOOL atRoot = true;
	for(INT boneIdx = inst->KPhysRootIndex; boneIdx <= inst->KPhysLastIndex; boneIdx++)
    {
        if(!inst->KSkelModels(boneIdx))
            continue;
		
        MdtBodyID body, parentBody;
        MeMatrix4 bodyTM, parentTM, invParentTM, relTM;

		guard(GetTransforms);
        // Get child transform 
        body = McdModelGetBody(inst->KSkelModels(boneIdx));
		MdtBodyGetTransform(body, bodyTM);

        // Get parent transform. 
        if(!atRoot) // This not the root (ie. has a parent) 
		{
			int parentIdx = skelMesh->RefSkeleton(boneIdx).ParentIndex;

			if(!inst->KSkelModels(parentIdx))
			{
				debugf(TEXT("postKarmaStep_skeletal: Bone (%s) does not have physics parent (%s)."), 
					*(skelMesh->RefSkeleton(boneIdx).Name), *(skelMesh->RefSkeleton(parentIdx).Name) );

				continue;
			}

            parentBody = McdModelGetBody(inst->KSkelModels(parentIdx));
			MdtBodyGetTransform(parentBody, parentTM);
        }
        else
        {
            parentBody = 0;
            MeMatrix4TMMakeIdentity(parentTM);
        }
		unguard; // 'GetTransforms'
        
        // Calculate relative transform 
        FCoords relC;
        MeMatrix4Copy(invParentTM, parentTM);
        MeMatrix4TMInvert(invParentTM);
        MeMatrix4MultiplyMatrix(relTM, bodyTM, invParentTM);
        
        // Make FCoords of bone relative to parent (relative to world if physics root).
        KME2UCoords(&relC, relTM);

        guard(MoveBone);
        if(atRoot)
        {
			// We move the actor to follow the root of the skeleton.
			// We should have set all graphics bones above this to have zero transform,
			// and we are ignoring the Origin/RotOrigin in the mesh, so the actor should
			// be in the same place as the physics root bone
			// NOTE: this might actually DESTROY the skeleton - so we need to be careful afterwards.
            FCheckResult Hit(1.0f);
            FVector moveBy = relC.Origin - this->Location;
			this->GetLevel()->MoveActor(this, moveBy, FRotator(0, 0, 0), Hit);

			// Here we check if actor is still around. If not, do nothing more.
			if(!inst->OurActor || inst->OurActor->bDeleteMe || !inst->KSkelIsInitialised)
                return;

			// If we couldn't be inserted into the Octree (eg. out of world).
			if( this->bCollideActors && !this->IsInOctree() )
			{
				this->GetLevel()->DestroyActor(this);
				return;
			}

			// We dont rotate the actor during ragdoll - we just rotate the phys-root bone graphic.
            FRotator NewRot;
			if(boneIdx == 0)
				NewRot = relC.OrthoRotation();
			else
				NewRot = (GMath.UnitCoords * relC).OrthoRotation();

			FName BoneName = skelMesh->RefSkeleton(boneIdx).Name;

			inst->SetBoneLocation(BoneName, FVector(0, 0, 0), 1);
			inst->SetBoneRotation(BoneName, NewRot, 0, 1);

			// Update overall skeleon velocity (vel of physics root). Might be used by something...
			MeVector3 linVel;
			MdtBodyGetLinearVelocity(body, linVel);
			KME2UPosition(&this->Velocity, linVel);
        }
        else
        {
            // NOTE: Ragdoll WILL NOT WORK if DrawScale3D is non-uniform!!!!!!
			FVector relPos = relC.Origin * RecipTotalScale;

            FRotator NewRot = (GMath.UnitCoords * relC).OrthoRotation();
            FName BoneName = skelMesh->RefSkeleton(boneIdx).Name;

			inst->SetBoneLocation(BoneName, relPos , 1);
            inst->SetBoneRotation(BoneName, NewRot, 0, 1);	
        }
        unguard; // 'MoveBone'

		guard(UpdateAABB);
		McdModelID model = inst->KSkelModels(boneIdx);
		McdGeometryID geom = McdModelGetGeometry(model);
		if(McdGeometryGetTypeId(geom) != kMcdGeometryTypeNull)
		{
			// Add to skel temporary bounding box
			MeVector3 mMin, mMax;
			FVector tmpVec;

			// Update the McdModel (AABB etc.)
			McdModelUpdate(model);

			McdModelGetAABB(model, mMin, mMax);
			KME2UPosition(&tmpVec, mMin);
			TempSkelBox += tmpVec;
			KME2UPosition(&tmpVec, mMax);
			TempSkelBox += tmpVec;
		}
		unguard; // 'UpdateAABB'

		guard(CapVelocity);
		// Cap velocity
		MeVector3 meVel;
		MdtBodyGetLinearVelocity(body, meVel);
		if(MeVector3MagnitudeSqr(meVel) > meMaxSpeed * meMaxSpeed)
		{
			//debugf(TEXT("Capping Speed"));
			MeVector3Normalize(meVel);
			MeVector3Scale(meVel, meMaxSpeed);
			MdtBodySetLinearVelocity(body, meVel[0], meVel[1], meVel[2]);
		}
		unguard; // 'CapVelocity'

		// We've dealt with the root, so mark flag as false.
		if(atRoot)
			atRoot=false;
    } // FOR EACH BONE.

	unguard; // 'UpdateBones'

	guard(UpdateHash);
	// Now we remove the skeleton from the hash, update its AABB, and reinsert.
	if(bCollideActors && GetLevel() && GetLevel()->Hash)
	{
		GetLevel()->Hash->RemoveActor(this);
		inst->KSkelBox = TempSkelBox;
		//GTempLineBatcher->AddBox(inst->KSkelBox, FColor(255,128,255));
		GetLevel()->Hash->AddActor(this);
	}
	unguard; // 'UpdateHash'

	unguard;
}

///////////////////// PHYSKARMARAGDOLL ////////////////////////

void KScaleJointLimits(USkeletalMeshInstance* inst, FLOAT Scale, FLOAT Stiffness)
{
	guard(KScaleJointLimits);

	for(INT jointIdx = 0; jointIdx < inst->KSkelJoints.Num(); jointIdx++)
	{
		MdtConstraintID con = inst->KSkelJoints(jointIdx);

		if(!con)
			continue;

		MdtHingeID h = MdtConstraintDCastHinge(con);
		MdtSkeletalID s = MdtConstraintDCastSkeletal(con);

		if(h)
		{
			MdtLimitID l = MdtHingeGetLimit(h);
			MeReal pos = MdtSingleLimitGetStop(MdtLimitGetUpperLimit(l));
			MdtSingleLimitSetStop(MdtLimitGetUpperLimit(l), Scale * pos);
			MdtSingleLimitSetStiffness(MdtLimitGetUpperLimit(l), Stiffness);

			pos = MdtSingleLimitGetStop(MdtLimitGetLowerLimit(l));
			MdtSingleLimitSetStop(MdtLimitGetLowerLimit(l), Scale * pos);
			MdtSingleLimitSetStiffness(MdtLimitGetLowerLimit(l), Stiffness);
		}
		else if(s)
		{
			MeReal pos = MdtSkeletalGetConePrimaryLimitAngle(s);
			MdtSkeletalSetConePrimaryLimitAngle(s, pos * Scale);

			pos = MdtSkeletalGetConeSecondaryLimitAngle(s);
			MdtSkeletalSetConeSecondaryLimitAngle(s, pos * Scale);

			MdtSkeletalSetConeStiffness(s, Stiffness);
		}
	}

	unguard;
}


// - Joint-error safety code
// - 'VelDropBelow' event
// - Convulsions
// - Bone lifters

inline void AActor::physKarmaRagDoll_internal(FLOAT deltaTime)
{
	check(Physics == PHYS_KarmaRagDoll);

	// We should NOT have a normal actor model if this is a skeletal thing.
	McdModelID actorModel = this->getKModel();
	check(!actorModel);

	USkeletalMesh* skelMesh = 0;
	if(this->Mesh)
		skelMesh = Cast<USkeletalMesh>(this->Mesh);
	RTN_WITH_ERR_IF(!skelMesh, "postKarmaStep_skeletal: No skelMesh.");

	USkeletalMeshInstance* inst = (USkeletalMeshInstance*)skelMesh->MeshGetInstance(this);
	RTN_WITH_ERR_IF(!inst, "postKarmaStep_skeletal: No USkeletalMeshInstance.");

	if(inst->KFrozen)
        return;

	// If not already initialised, try and do so here.
	if(!inst->KSkelIsInitialised)
		KInitSkeletonKarma(inst);

	// If STILL not initialised, return.
	if(!inst->KSkelIsInitialised)
        return;
    
	UKarmaParamsSkel* kparams = Cast<UKarmaParamsSkel>(this->KParams);
	RTN_WITH_ERR_IF(!kparams, "(Karma:) physKarmaRagDoll_internal: No KarmaParamsSkel.")

	check(inst->KPhysRootIndex >= 0 && inst->KPhysRootIndex < inst->KSkelModels.Num());
	check(inst->KPhysLastIndex >= 0 && inst->KPhysLastIndex < inst->KSkelModels.Num());

	// Joint-error safety code.
	UBOOL atRoot = true;    
	for(INT boneIdx = inst->KPhysRootIndex; boneIdx <= inst->KPhysLastIndex; boneIdx++)
    {
        if(!inst->KSkelModels(boneIdx))
            continue;

		// If some flags are set - do debug drawing
		if(KGData->DebugDrawOpt != 0)
			KModelDraw(inst->KSkelModels(boneIdx), KGData->DebugDrawOpt, KLineDraw);

        if(!atRoot) // This not the root (ie. has a joint to a parent) 
        {
			// Check how far out the joint is. If its too much - kill the actor.
			// This assumes that the joint allows no translation.
			MdtConstraintID constraint = inst->KSkelJoints(boneIdx);

			// Calc constraint position in world space for both bodies.
			MeVector3 pos1world, pos2world, diff;
			MdtConstraintBodyGetPosition(constraint, 0, pos1world);
			MdtConstraintBodyGetPosition(constraint, 1, pos2world);
			MeVector3Subtract(diff, pos1world, pos2world);
			
			MeReal errorSqr = MeVector3MagnitudeSqr(diff);
			if(errorSqr > 1.0f*1.0f)
			{
				debugf(TEXT("(Karma:) Excessive Joint Error. Destroying RagDoll."));
				inst->OurActor->GetLevel()->DestroyActor(this);
                return;
			}
        }

		// We've dealt with the root, so mark flag as false.
		if(atRoot)
			atRoot=false;
    } // FOR EACH BONE.

	// 'VelDropBelow' event.
	guard(VelDropBelow);
	FLOAT velMag = this->Velocity.Size();

	// If we have just crossed the threshold, trigger event.
	if(velMag < kparams->KVelDropBelowThreshold && kparams->KLastVel > kparams->KVelDropBelowThreshold)
	{
		this->eventKVelDropBelow();
	}
	kparams->KLastVel = velMag;
	unguard;

	// Apply convulsions if desired
	guard(Convulse);

	inst->KRagdollAge += deltaTime; // update ragdoll age

	const FLOAT ConvulseStrength = 0.5f;
	const FLOAT ConvulseStiffness = 1000.f;

	// Relax limits if it is time to.
	if(inst->KRagdollAge > inst->KRelaxLimitTime && inst->KLimitsAreReduced)
	{
		KScaleJointLimits(inst, 1.f/ConvulseStrength, MEINFINITY);
		inst->KLimitsAreReduced = 0;

		// If still convulsing, Schedule next convulsion!
		if(kparams->bKDoConvulsions)
		{
			inst->KNextConvulsionTime = inst->KRagdollAge + kparams->KConvulseSpacing.GetRand();
			//debugf(TEXT("END CONVULSION. Next Convulsion: %f"), inst->KNextConvulsionTime);
		}
	}

	// If this ragdoll is convulsing, 
	// and we are not currently convulsed, 
	// and its time for the next convulsion.
	if(	kparams->bKDoConvulsions && 
		!inst->KLimitsAreReduced &&
		inst->KRagdollAge > inst->KNextConvulsionTime )
	{
		//debugf(TEXT("START CONVULSION."));

		// Call script event for effects etc.
		eventKSkelConvulse();

		// Make sure we have enabled the skeleton (iterate over model until we find one with a body).
		UBOOL haveEnabled = 0;
		for(INT modelIdx = inst->KPhysRootIndex; modelIdx <= inst->KPhysLastIndex && !haveEnabled; modelIdx++)
		{

			McdModelID model = inst->KSkelModels(modelIdx);
			if(!model)
				continue;

			MdtBodyID body = McdModelGetBody(model);
			if(!body)
				continue;

			MdtBodyEnable(body);
			haveEnabled = 1;
		}

		// Reduce joint limits to cause ragdoll to convulse
		KScaleJointLimits(inst, ConvulseStrength, ConvulseStiffness);
		inst->KRelaxLimitTime = inst->KRagdollAge + 0.1f; // Convulse for 0.1 seconds.
		inst->KLimitsAreReduced = 1;
	}
	unguard;

	// Apply constant bone forces.
	guard(BoneLifters);
	if(KGData->bDoTick)
	{
		for(INT i=0; i<inst->KBoneLifters.Num(); i++)
		{
			FKBoneLifter* l = &inst->KBoneLifters(i);

			McdModelID model = inst->KSkelModels(l->BoneIndex);
			if(!model)
				continue;

			MdtBodyID body = McdModelGetBody(model);
			if(!body)
				continue;

			// Scale force over time.
			l->CurrentTime += deltaTime;

			MeReal useVel = K_U2MEScale * l->LiftVel.Eval(l->CurrentTime); // vertical velocity, karma scale
			MeReal useSoft = l->Softness.Eval(l->CurrentTime);

			//debugf(TEXT("Time:%f Vel:%f Soft:%f"), l->CurrentTime, K_ME2UScale* useVel, useSoft);

			// Always keep ragdoll alive while a force is active.
			MdtBodyEnable(body);

			MdtContactGroupID cg = l->LiftContact;
			check(cg);

			if(MdtContactGroupGetCount(cg) == 0)
				MdtContactGroupCreateContact(cg);

			check(MdtContactGroupGetCount(cg) == 1);

			MdtContactID contact = MdtContactGroupGetFirstContact(cg);

			MeReal Epsilon = MdtWorldGetEpsilon(MdtContactGroupGetWorld(cg));

			MeVector3 comPos;
			MdtBodyGetCenterOfMassPosition(body, comPos);
			MdtContactSetPosition(contact, comPos[0], comPos[1], comPos[2]);
			MdtContactSetNormal(contact, 0, 0, 1); // vertical normal
			MdtContactSetPenetration(contact, 0.f);

			MdtContactParamsID params = MdtContactGetParams(contact);
			MdtContactParamsSetMaxAdhesiveForce(params, MEINFINITY);
			
			MdtContactParamsSetSoftness(params, MeMAX(Epsilon, useSoft));

			// Optional lateral friction. Implemented using slip to avoid LCP.
			if(l->LateralFriction < KINDA_SMALL_NUMBER)
			{
				MdtContactParamsSetType(params, MdtContactTypeFrictionZero);
			}
			else
			{
				MdtContactParamsSetType(params, MdtContactTypeFriction2D);
				MdtContactParamsSetFrictionModel(params, MdtFrictionModelBox);
				MdtContactParamsSetFrictionCoeffecient(params, MEINFINITY);
				MdtContactParamsSetFriction(params, MEINFINITY);

				// Make sure slip is no less than world epsilon though...
				MdtContactParamsSetSlip( params, MeMAX(Epsilon, (1.f/l->LateralFriction)-1.f) );
			}

			MeVector3 meWorldVel = {0, 0, useVel};
			MdtContactSetWorldVelocity(contact, meWorldVel[0], meWorldVel[1], meWorldVel[2]);
		}
	}
	unguard; // 'BoneLifters'
}


// As with AActor::physKarma, this assumes Karma has been updated, and will synchronise the position of
// skeletal bones to correspond with the simulation.
void AActor::physKarmaRagDoll(FLOAT deltaTime)
{
    guard(AActor::physKarmaRagDoll);
	clock(GStats.DWORDStats(GEngineStats.STATS_Karma_physKarma));
    physKarmaRagDoll_internal(deltaTime);
	unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_physKarma));
    unguard;
}

#endif

void AActor::execKScaleJointLimits( FFrame& Stack, RESULT_DECL )
{
	guard(execKScaleJointLimits);

	P_GET_FLOAT(Scale);
	P_GET_FLOAT(Stiffness);
	P_FINISH;

#ifdef WITH_KARMA
	USkeletalMeshInstance* inst = Cast<USkeletalMeshInstance>(MeshInstance);

	if(inst)
		KScaleJointLimits(inst, Scale, Stiffness);
#endif

	unguard;
}

