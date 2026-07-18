/*============================================================================
	Karma Integration
    
    - PHYS_Karma & PHYS_KarmaRagDoll funcs
	- Other C++ Karma-related member functions.
============================================================================*/
#include "EnginePrivate.h"

#ifdef WITH_KARMA

void AActor::KFreezeRagdoll()
{
	guard(AActor::KFreezeRagdoll);

	if( !MeshInstance || !MeshInstance->IsA(USkeletalMeshInstance::StaticClass()) )
	{
		debugf(TEXT("(Karma:) KFreezeRagdoll: No skeletal mesh."));
		return;
	}

	USkeletalMeshInstance* inst = Cast<USkeletalMeshInstance>(this->MeshInstance);
	if(!inst->KSkelIsInitialised)
	{
		debugf(TEXT("(Karma:) KFreezeRagdoll: Ragdoll not initialised."));
		return;
	}

	// Turn off/destroy Karma for rag-doll
	KTermSkeletonKarma(inst);

	// We have to be careful here - because setting the 'frozen' flag changes
	// which bounding-box the rag-doll uses.

    if( Physics == PHYS_KarmaRagDoll ) // sjs
    {
        setPhysics(PHYS_Falling);
		Velocity = FVector(0, 0, 0);
    }

	ULevel* level = GetLevel();
	if(bCollideActors && level && level->Hash)
		level->Hash->RemoveActor(this);

	inst->KFrozen = 1;

	if(bCollideActors && level && level->Hash)
		level->Hash->AddActor(this);

	unguard;
}
#endif



/*****************************************************/

// Create a default KarmaParams for a newly created AKActor (as long as its not a constraint)
void AKActor::Spawned()
{
	guard(AKActor::Spawned);
    if( KParams == NULL && !this->IsA(AKConstraint::StaticClass()))
		KParams = ConstructObject<UKarmaParams>( UKarmaParams::StaticClass(), this->GetOuter() );
	unguard;
}

/*****************************************************/


#ifndef WITH_KARMA

void UKarmaParamsCollision::execCalcContactRegion( FFrame& Stack, RESULT_DECL )
{
	guard(UKarmaParamsCollision::execCalcContactRegion);

	P_FINISH;

	// no-op stub for builds without Karma support.  --ryan.

	unguard;
}

#else

// Returns actors model (or NULL if it has none, or is a constraint.
McdModelID AActor::getKModel() const
{
    if(!this->KParams)
        return 0;

    return ((McdModelID)this->KParams->KarmaData);
}

// Ensure this actor is awake (ie being simulated).
void AActor::KWake()
{
	guard(AActor::KWake);

	// If this is an actor with Karma dynamics, make sure they are enabled.
    McdModelID m = this->getKModel();
    if(m && McdModelGetBody(m))
	{
		MdtBodyEnable(McdModelGetBody(m));
		return;
	}

	// If this is a set-up rag-doll, enable the first model.
	if(this->Physics == PHYS_KarmaRagDoll && this->MeshInstance)
	{
		USkeletalMeshInstance *skelInst = Cast<USkeletalMeshInstance>(this->MeshInstance);
		if(skelInst && skelInst->KSkelIsInitialised)
		{
			m = skelInst->KSkelModels(0);
			if(m && McdModelGetBody(m))
			{
				MdtBodyEnable(McdModelGetBody(m));
				return;
			}
		}
	}

	unguard;
}

void AActor::KGetRigidBodyState(FKRigidBodyState* RBstate)
{
	guard(AActor::KGetRigidBodyState);

	// If no model or body, just return zero.
	McdModelID model = this->getKModel();
	if(!model)
	{
		appMemset( RBstate, 0, sizeof(FKRigidBodyState) );
		RBstate->Quaternion.W = 1.0f;
		return;
	}

	MdtBodyID body = McdModelGetBody(model);
	if(!body)
	{
		appMemset( RBstate, 0, sizeof(FKRigidBodyState) );
		RBstate->Quaternion.W = 1.0f;
		return;
	}

	MeVector3 tmp;
	MdtBodyGetPosition(body, tmp);
	RBstate->Position.X = tmp[0] * K_ME2UScale;
	RBstate->Position.Y = tmp[1] * K_ME2UScale;
	RBstate->Position.Z = tmp[2] * K_ME2UScale;

	MeVector4 q;
	MdtBodyGetQuaternion(body, q);
	RBstate->Quaternion.W = q[0];
	RBstate->Quaternion.X = q[1];
	RBstate->Quaternion.Y = q[2];
	RBstate->Quaternion.Z = q[3];

	MdtBodyGetLinearVelocity(body, tmp);
	RBstate->LinVel.X = tmp[0] * K_ME2UScale;
	RBstate->LinVel.Y = tmp[1] * K_ME2UScale;
	RBstate->LinVel.Z = tmp[2] * K_ME2UScale;

	MdtBodyGetAngularVelocity(body, tmp);
	RBstate->AngVel.X = tmp[0];
	RBstate->AngVel.Y = tmp[1];
	RBstate->AngVel.Z = tmp[2];

	unguard;
}

UBOOL AActor::KIsAwake()
{
	guard(AActor::KIsAwake);

    McdModelID model = this->getKModel();
	if(!model)
		return false;

	MdtBodyID body = McdModelGetBody(model);
	if(!body)
		return false;

	if(MdtBodyIsEnabled(body))
		return true;
	else
		return false;

	unguard;
}

void AActor::KDrawRigidBodyState(FKRigidBodyState* RBstate, UBOOL AltColour)
{
	guard(AActor::KDrawRigidBodyState);

	FVector pos(RBstate->Position.X, RBstate->Position.Y, RBstate->Position.Z);

	MeVector4 quat = {RBstate->Quaternion.W, RBstate->Quaternion.X, RBstate->Quaternion.Y, RBstate->Quaternion.Z};
	MeMatrix4 tm;
	MeQuaternionToTM(tm, quat);

	FVector x, y, z;
	KME2UPosition(&x, tm[0]);
	KME2UPosition(&y, tm[1]);
	KME2UPosition(&z, tm[2]);

	const float AxisLength = 1.8f;

	if(!AltColour)
	{
		GTempLineBatcher->AddLine(pos, pos + AxisLength * x, FColor(255, 0, 0));
		GTempLineBatcher->AddLine(pos, pos + AxisLength * y, FColor(0, 255, 0));
		GTempLineBatcher->AddLine(pos, pos + AxisLength * z, FColor(0, 0, 255));
	}
	else
	{
		GTempLineBatcher->AddLine(pos, pos + 0.8f * AxisLength * x, FColor(255, 128, 128));
		GTempLineBatcher->AddLine(pos, pos + 0.8f * AxisLength * y, FColor(128, 255, 128));
		GTempLineBatcher->AddLine(pos, pos + 0.8f * AxisLength * z, FColor(128, 128, 255));
	}

	unguard;
}


void AActor::KAddForces(FVector force, FVector torque)
{
	guard(AActor::KAddForces);

	McdModelID model = getKModel();
	if(!model)
		return;

	MdtBodyID body = McdModelGetBody(model);
	if(!body)
		return;

	MdtBodyAddForce(body, force.X, force.Y, force.Z);
	MdtBodyAddTorque(body, torque.X, torque.Y, torque.Z);

	unguard;
}
	
void AActor::KAddImpulse(FVector Impulse, FVector Position, FName BoneName)
{
	guard(AActor::KAddImpulse);

	MdtBodyID body = 0;
	UBOOL isSkel = 0;

	if(KGData && !KGData->bAutoEvolve)
		return;

	USkeletalMeshInstance* inst = 0;
	if(this->MeshInstance)
		inst = Cast<USkeletalMeshInstance>(this->MeshInstance);

	// If this is a basic, one-body Actor
	if(this->getKModel() && this->Physics == PHYS_Karma)
	{
		body = McdModelGetBody(this->getKModel());
	}
	// If this is a rag-doll, find the right bone.
	else if(this->Physics == PHYS_KarmaRagDoll && inst)
	{
		// If we have a specific name for the bone we want to set the velocity of,
		// find that bone.
		if(BoneName != NAME_None)
		{
			INT boneIx = inst->MatchRefBone(BoneName);

			McdModelID model = inst->KSkelModels(boneIx);
			if(model)
				body = McdModelGetBody(model);
		}
		else if (inst->KLastTraceHit != -1)// Otherwise, use bone from last ray check.
		{
			// If the hack isn't valid for some reason... bail out
			if(inst->KLastTraceHit < inst->KSkelModels.Num() && inst->KLastTraceHit >= 0 &&
				inst->KSkelModels(inst->KLastTraceHit) != 0)
			{
				body = McdModelGetBody(inst->KSkelModels(inst->KLastTraceHit));
				//debugf(TEXT("Hit Bone: %s Impulse: <%f %f %f>"), 
				//	*((USkeletalMesh*)inst->GetMesh())->RefSkeleton(inst->KLastTraceHit).Name,
				//	Impulse[0], Impulse[1], Impulse[2]);
			}
		}
#if 0
		else // If all else fails, kick the root (if present)
		{
			McdModelID model = inst->KSkelModels(0);
			if(model)
				body = McdModelGetBody(model);
		}
#endif

		inst->KLastTraceHit = -1;
		isSkel = 1;

		//debugf(TEXT("Impulse: %f %f %f"), Impulse.X, Impulse.Y, Impulse.Z);
	}

	// Scale both to ME sizes - for both mass AND velocity!
	MeVector3 kimpulse;
	KU2MEPosition(kimpulse, Impulse);
	MeVector3Scale(kimpulse, K_U2MEMassScale);


	// for skeleton shots - just apply impulse to centre-of-mass
	if(isSkel)
	{
		// If we hit an actual bone - apply full impulse to it.
		if(body)
		{
			MdtBodyAddImpulse(body, kimpulse[0], kimpulse[1], kimpulse[2]);
			MdtBodyEnable(body);
		}

#if 0
		// walk the heirarchy - applying impulses to other bones.
		MeVector3Scale(kimpulse, (MeReal)0.1);
		//kimpulse[2] += MeSqrt(kimpulse[0] * kimpulse[0] + kimpulse[1] * kimpulse[1]);

		for(INT i=0; i<inst->KSkelModels.Num(); i++)
		{
			McdModelID boneModel = inst->KSkelModels(i);
			if(!boneModel)
				continue;

			MdtBodyID boneBody = McdModelGetBody(boneModel);
			if(!boneBody || boneBody == body)
				continue;

			MdtBodyAddImpulse(boneBody, kimpulse[0], kimpulse[1], kimpulse[2]);

			MdtBodyEnable(boneBody);
		}
#endif
	}
	else
	{
		if(body)
		{
			if( Position.IsZero() )
			{
				MdtBodyAddImpulse(body, kimpulse[0], kimpulse[1], kimpulse[2]);
			}
			else
			{
				MeVector3 kpos;
				KU2MEPosition(kpos, Position);

				MdtBodyAddImpulseAtPosition(body, 
					kimpulse[0], kimpulse[1], kimpulse[2],
					kpos[0], kpos[1], kpos[2]);
			}

			MdtBodyEnable(body);
		}
	}

	unguard;
}

void AActor::KAddAngularImpulse(FVector AngImpulse)
{
	guard(AActor::KAddAngularImpulse);

	if(KGData && !KGData->bAutoEvolve)
		return;

	if(this->getKModel() && this->Physics == PHYS_Karma)
	{
		MdtBodyID body = McdModelGetBody(this->getKModel());

		if(body)
		{
			body->impulseAngular[0] += AngImpulse.X;
			body->impulseAngular[1] += AngImpulse.Y;
			body->impulseAngular[2] += AngImpulse.Z;

			MdtBodyEnable(body);
		}
	}

	unguard;
}

// Update any MdtBody's mass properties using the UnrealScript version.
// Need to call this after changing mass, damping, inertia tensor, com-position, KScale(3D), stay-upright etc.
void UKarmaParams::PostEditChange()
{
	guard(UKarmaParams::PostEditChange);

	Super::PostEditChange();

	McdModelID model = (McdModelID)this->KarmaData;
	if(!model)
		return;

	MdtBodyID body = McdModelGetBody(model);
	if(!body)
		return;

	//AActor* actor = KBodyGetActor(body);


	// If we have a 'full' Karma Params including inertia tensor/com-offset, use that.
	UKarmaParamsRBFull* fullParams = Cast<UKarmaParamsRBFull>(this);
	if(fullParams)
	{
		// Mass props
		MeMatrix3 I;
		FVector totalScale = KScale3D * KScale;
		I[0][0] =			fullParams->KInertiaTensor[0] * KMass * totalScale.Y * totalScale.Z;
		I[0][1] = I[1][0] = fullParams->KInertiaTensor[1] * KMass * totalScale.X * totalScale.Y;
		I[0][2] = I[2][0] = fullParams->KInertiaTensor[2] * KMass * totalScale.Z * totalScale.X;
		I[1][1] =			fullParams->KInertiaTensor[3] * KMass * totalScale.X * totalScale.Z;
		I[1][2] = I[2][1] = fullParams->KInertiaTensor[4] * KMass * totalScale.Y * totalScale.Z;
		I[2][2] =			fullParams->KInertiaTensor[5] * KMass * totalScale.X * totalScale.Y;

		// Centre-of-mass position.
		// Can only do this if there are no constraints.
		MeVector3 o;
		FVector newCOM = fullParams->KCOMOffset * totalScale;
		KU2MEVecCopy(o, newCOM);
		MeDictNode *node = MeDictFirst(&body->constraintDict);
		if(!node)
			MdtBodySetCenterOfMassRelativePosition(body, o);

		// Inertia tensor
		KBodySetInertiaTensor(body, I);
	}
	else
	{
		//	JTODO: Mass properties came from static mesh, so we need to get hold of them again 
		//	to rescale them in case scale or mass has changed. But how!?
	}
	
	KBodySetMass(body, this->KMass);
	
	// Spherical?
	if(this->bKNonSphericalInertia)
		MdtBodyEnableNonSphericalInertia(body);
	else
		MdtBodyDisableNonSphericalInertia(body);

    // Damping
    MdtBodySetAngularVelocityDamping(body, this->KAngularDamping);
    MdtBodySetLinearVelocityDamping(body, this->KLinearDamping);

	// Stay upright stuff

	// We use an Angular3 constraint to keep this thing upright.
	if(this->bKStayUpright)
	{
		MdtAngular3ID ang3 = (MdtAngular3ID)this->KAng3;

		if(!ang3) // need to create it
		{
			ang3 = MdtAngular3Create(MdtBodyGetWorld(body));
			MdtAngular3SetBodies(ang3, body, 0);
			
			MdtConstraintID mdtCon = MdtAngular3QuaConstraint(ang3);

			MdtConstraintBodySetAxesRel(mdtCon, 0, 
				0, 0, 1, 
				1, 0, 0);
			
			MdtConstraintBodySetAxesRel(mdtCon, 1, 
				0, 0, 1, 
				1, 0, 0);

			MdtAngular3Enable(ang3);

			this->KAng3 = (PTRINT)ang3;
		}

		if(MdtAngular3RotationIsEnabled(ang3) && !this->bKAllowRotate)
			MdtAngular3EnableRotation(ang3, 0);
		else if(!MdtAngular3RotationIsEnabled(ang3) && this->bKAllowRotate)
			MdtAngular3EnableRotation(ang3, 1);

		MdtAngular3SetStiffness(ang3, this->StayUprightStiffness);
		MdtAngular3SetDamping(ang3, this->StayUprightDamping);
	}
	else
	{
		// need to destroy it
		if(this->KAng3)
		{		
			MdtAngular3ID ang3 = (MdtAngular3ID)this->KAng3;

			MdtAngular3Disable(ang3);
			MdtAngular3Destroy(ang3);
			this->KAng3 = 0;
		}
	}

    unguard;
}

// If we destroy the KarmaParams before the Actor, we need to remove all the KRepulsors, because the array will be zeroed.
void UKarmaParams::Destroy()
{
	guard(UKarmaParams::Destroy);

	// Remove any KRepulsor contacts on this actor.
	if(Repulsors.Num() > 0)
	{
		for(INT i=0; i<Repulsors.Num(); i++)
		{
			AKRepulsor* rep = Repulsors(i);
			if(rep->KContact)
			{
				MdtContactGroupID cg = (MdtContactGroupID)rep->KContact;

				if( MdtContactGroupIsEnabled(cg) )
					MdtContactGroupDisable(cg);

				MdtContactGroupDestroy(cg);
				rep->KContact = NULL;
			}
		}
	}

	Super::Destroy();

	unguard;
}

void AKRepulsor::Destroy()
{
	guard(AKRepulsor::Destroy);

	if(KContact)
	{
		MdtContactGroupID cg = (MdtContactGroupID)KContact;

		if( MdtContactGroupIsEnabled(cg) )
			MdtContactGroupDisable(cg);

		MdtContactGroupDestroy(cg);
		KContact = NULL;
	}

	Super::Destroy();

	unguard;
}


/////////////////////////// PRE/POST STEP ///////////////////////////////////

// This is called just before this actor is simulated. 
// You CAN change damping and add forces.
// You CANNOT create or destroy anything.
void AActor::preKarmaStep(FLOAT DeltaTime)
{
	guard(AActor::preKarmaStep);
	check(Physics == PHYS_Karma || Physics == PHYS_KarmaRagDoll);

	if(Physics == PHYS_KarmaRagDoll)
	{
		this->preKarmaStep_skeletal(DeltaTime);
		return;
	}

    McdModelID model = this->getKModel();
    if(!model)
        return;
    
    MdtBodyID body = McdModelGetBody(model);
    if(!body)
        return;

	MeReal meMass = MdtBodyGetMass(body);
	FVector calcForce(0, 0, 0);

    MeVector3 gforce = {0, 0, 0};
	if(PhysicsVolume)
		KU2MEPosition(gforce, PhysicsVolume->Gravity); // scales gravity (m/s^2) for ME sizes

	// Get the parameters for this actors simulation.
	UKarmaParams* kparams = 0;
	if(this->KParams)
		kparams = Cast<UKarmaParams>(this->KParams);

	if(!kparams)
	{
        debugf(TEXT("(Karma:) preKarmaStep: Actor has no KParams."));
        return;
	}

	MeReal gravScale = ME_GRAVSCALE * kparams->KActorGravScale;
	if(Level)
		gravScale *= Level->KarmaGravScale;

	// Buoyancy calculations - basically reduces affect of gravity and increases damping.
	MeReal buoyEffect = 0.f;
	MeReal linDamp = kparams->KLinearDamping;
	MeReal angDamp = kparams->KAngularDamping;

	if(PhysicsVolume->bWaterVolume)
	{
		buoyEffect = PhysicsVolume->KBuoyancy * kparams->KBuoyancy;
		linDamp += PhysicsVolume->KExtraLinearDamping;
		angDamp += PhysicsVolume->KExtraAngularDamping;
	}

	calcForce.X = gravScale * meMass * gforce[0];
	calcForce.Y = gravScale * meMass * gforce[1];
	calcForce.Z = gravScale * meMass * (gforce[2] * (1-buoyEffect));

	// Set damping
	MdtBodySetLinearVelocityDamping(body, linDamp);
	MdtBodySetAngularVelocityDamping(body, angDamp);

	// Any user forces
	FVector userForce(0, 0, 0), userTorque(0, 0, 0);
	eventKApplyForce(userForce, userTorque);

	// Apply force and torque
    MdtBodyAddForce(body, 
		calcForce.X + userForce.X, 
		calcForce.Y + userForce.Y, 
		calcForce.Z + userForce.Z);

	MdtBodyAddTorque(body,
		userTorque.X,
		userTorque.Y,
		userTorque.Z);

	unguard;
}

// Used when doing safetime line checks to figure out if motion should be slowed for this object.
UBOOL KShouldStopKarma(AActor* actor)
{
	guard(KShouldStopKarma);

	check(actor);

	if( actor->IsA(ATerrainInfo::StaticClass()) || actor->IsA(ALevelInfo::StaticClass()) )
		return true;

	if(!actor->bBlockKarma)
		return false;

	// Do safetime against blocking volumes (but not if class specific blocker - it depends on the class type).
	ABlockingVolume* BV = Cast<ABlockingVolume>(actor);
	if(BV && !BV->bClassBlocker)
		return true;

	UPrimitive* prim = actor->GetPrimitive();
	if(!prim)
		return false;

	UStaticMesh* statMesh = NULL;
	USkeletalMesh* skelMesh = NULL;
	if( (statMesh = Cast<UStaticMesh>(prim)) != NULL )
	{
		if(!statMesh->UseSimpleKarmaCollision)
			return true; // Karma collision with graphics triangles

		if(statMesh->UseSimpleKarmaCollision && statMesh->KPhysicsProps)
			return true; // Karma collision with collision model.
	}
	else if( (skelMesh = Cast<USkeletalMesh>(prim)) != NULL )
	{
		if(skelMesh->KPhysicsProps)
			return true;
	}

	return false;

	unguard;
}

// Called just after the actor has finished being simulated.
void AActor::postKarmaStep()
{
	guard(AActor::postKarmaStep);

	UKarmaParams* kparams = Cast<UKarmaParams>(this->KParams);
	if(!kparams)
		return;

	if(bDeleteMe)
		return;

	if(Physics != PHYS_Karma && Physics != PHYS_KarmaRagDoll)
	{
		debugf(TEXT("(Karma:) postKarmaStep: Actors with non-Karma physics."));
		return;
	}

	if(Physics == PHYS_KarmaRagDoll)
	{
		this->postKarmaStep_skeletal();
		return;
	}

    McdModelID model = this->getKModel();
    if(!model)
        return;
    
    MdtBodyID body = McdModelGetBody(model);
    if(!body)
        return;

	ULevel* level = GetLevel();

	// Update Unreal position/rotation from Karma body.
	MeMatrix4 tm;        
	MdtBodyGetTransform(body, tm);

	FRotator rot;
	FVector newPos, moveBy;
	FCheckResult Hit(1.0f);
	KME2UTransform(&newPos, &rot, tm);
	moveBy = newPos - this->Location; 

	// Just to be sure..
	this->bCollideWorld = 0;

	// Keep bounding box up to date!
	if(McdGeometryGetTypeId(McdModelGetGeometry(model)) != kMcdGeometryTypeNull)
		McdModelUpdate(model);

	clock(GStats.DWORDStats(GEngineStats.STATS_Karma_Temp));

	// If we are doing check to destroy actor when it passes through the world, do it now.
	if( kparams->bDestroyOnWorldPenetrate && Role == ROLE_Authority && !moveBy.IsZero() )
	{
		FBox Box = GetPrimitive()->GetCollisionBoundingBox(this);
		FVector BoxCenter = Box.GetCenter();

		// First try a zero-extent line check

		FMemMark Mark(GMem);

		// First do a zero-extent trace to see if we have passed through something that should have blocked us.
		FCheckResult* FirstHit = level->MultiLineCheck(GMem, BoxCenter + moveBy, BoxCenter, FVector(0,0,0), Level, TRACE_World | TRACE_Volumes, this);
		UBOOL bHit = false;
		for( FCheckResult* Check = FirstHit; Check!=NULL; Check=Check->GetNext() )
		{
			if( Check->Actor->bShouldStopKarma )
			{
				bHit = true;
				break;
			}
		}

		// If the zero extent trace hit something that should have stopped us, do an extent trace.
		if(bHit)
		{
			UBOOL bHitWorld = false;
			FirstHit = level->MultiLineCheck(GMem, BoxCenter + moveBy, BoxCenter, FVector(1,1,1), Level, TRACE_World | TRACE_Volumes, this);
			for( FCheckResult* Check = FirstHit; Check!=NULL; Check=Check->GetNext() )
			{
				if( Check->Actor->bShouldStopKarma )
				{
					bHitWorld = true;
					break;
				}
			}

			// Ok - if we really passed through the world (probably) kill the actor.
			if (bHitWorld)
			{
				if (Hit.Actor)
					debugf( NAME_DevKarma, TEXT("postKarmaStep: Karma Actor (%s) passed through %s."), GetName(), Hit.Actor->GetName());
				else
					debugf( NAME_DevKarma, TEXT("postKarmaStep: Actor (%s) passed through world."), GetName() );

				eventFellOutOfWorld(KILLZ_None);
			}
		}

		Mark.Pop();

		if(this->bDeleteMe || this->getKModel() != model || McdModelGetBody(model) != body)
			return;
	}

	unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_Temp));


	// Actually move the actor. 
	// This could actually destroy the actor due to Touch etc. - so we check afterwards.
	//clock(GStats.DWORDStats(GEngineStats.STATS_Karma_Temp));
	level->MoveActor(this, moveBy, rot, Hit);
	//unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_Temp));

	if(this->bDeleteMe || this->getKModel() != model || McdModelGetBody(model) != body)
		return;

	if( bCollideActors && !IsInOctree() )
	{
		level->DestroyActor(this);
		return;
	}

	// Angular/Linear Velocity capping.
	MeReal meMaxSpeed = K_U2MEScale * kparams->KMaxSpeed;
	MeVector3 linVel, angVel;
	MdtBodyGetLinearVelocity(body, linVel);
	if(MeVector3MagnitudeSqr(linVel) > meMaxSpeed * meMaxSpeed)
	{
	    MeVector3Normalize(linVel);
	    MeVector3Scale(linVel, meMaxSpeed);
	    MdtBodySetLinearVelocity(body, linVel[0], linVel[1], linVel[2]);
	}

	MdtBodyGetAngularVelocity(body, angVel);
	if(MeVector3MagnitudeSqr(angVel) > kparams->KMaxAngularSpeed * kparams->KMaxAngularSpeed)
	{
		MeVector3Normalize(angVel);
		MeVector3Scale(angVel, kparams->KMaxAngularSpeed);
		MdtBodySetAngularVelocity(body, angVel[0], angVel[1], angVel[2]);
	}

	// Set the Velocity field of the Actor, in case its used by anything.
	KME2UPosition(&this->Velocity, linVel);

	// Update instantaneous acceleration.
	MeVector3 a;
	MdtBodyGetLinearAcceleration(body, a);
	KME2UPosition(&KParams->KAcceleration, a);
	
	unguard;
}

void UKarmaParamsCollision::CalcContactRegion()
{
	guard(UKarmaParamsCollision::CalcContactRegion);

	if(!bContactingLevel)
		return;

	McdModelID model = (McdModelID)KarmaData;
	if(!model)
		return;

	MdtBodyID body = McdModelGetBody(model);
	if(!body)
		return;

	TArray<FVector> ContactPositions;
	FVector ContactAverageNormal(0,0,0);

	INT NumContacts = 0;
	ContactRegionNormalForce = 0.f;

	MeDict *dict = &body->constraintDict;
	MeDictNode *nextNode, *node = MeDictFirst(dict);
	while(node != 0)
	{
		MdtConstraintID c = (MdtConstraintID)MeDictNodeGet(node);
		nextNode = MeDictNext(dict, node);

		// If this constraint is a contact group to the world - lets look at it.
		MdtContactGroupID cg = MdtConstraintDCastContactGroup(c);
		if( cg && MdtContactGroupGetBody(cg, 1) == NULL )
		{
			MdtContactID contact = MdtContactGroupGetFirstContact(cg);
			while(contact)
			{
				FVector UContactPos, UContactNormal;
				MeVector3 ContactPos, ContactNormal, ContactForce;

				MdtContactGetPosition(contact, ContactPos);
				KME2UPosition(&UContactPos, ContactPos);
				ContactPositions.AddItem(UContactPos);

				MdtContactGetNormal(contact, ContactNormal);
				KME2UVecCopy(&UContactNormal, ContactNormal);
				ContactAverageNormal += UContactNormal;
		
				MdtContactGetForce(contact, 0, ContactForce);
				ContactRegionNormalForce += MeVector3Dot(ContactNormal, ContactForce);

				NumContacts++;

				contact = MdtContactGroupGetNextContact(cg, contact);
			}
		}

		node = nextNode;
	}


	// Divide results and calc radius

	if(NumContacts == 0)
	{
		ContactRegionCenter = FVector(0,0,0);
		ContactRegionNormal = FVector(0,0,0);
		ContactRegionRadius = 0.f;
	}
	else
	{
		// POSITION
		ContactRegionCenter = FVector(0,0,0);

		for(INT i=0; i<ContactPositions.Num(); i++)
			ContactRegionCenter += ContactPositions(i);

		ContactRegionCenter = ContactRegionCenter/((FLOAT)NumContacts);

		// NORMAL
		ContactRegionNormal = ContactAverageNormal/((FLOAT)NumContacts);

		// RADIUS
		ContactRegionRadius = 0.f;

		for(INT i=0; i<ContactPositions.Num(); i++)
		{
			FVector Diff = ContactPositions(i) - ContactRegionCenter; // Find vector from contact to region center.
			Diff -= (Diff | ContactRegionNormal) * ContactRegionNormal; // Project contact into plane of contact region normal.

			FLOAT Dist = Diff.Size();

			if(Dist > ContactRegionRadius)
				ContactRegionRadius = Dist;
		}
	}

	unguard;
}

void UKarmaParamsCollision::execCalcContactRegion( FFrame& Stack, RESULT_DECL )
{
	guard(UKarmaParamsCollision::execCalcContactRegion);

	P_FINISH;

	CalcContactRegion();

	unguard;
}

// When we get an update for the physics, we try to do it smoothly if it is less than .._THRESHOLD.
// We directly fix .._AMOUNT * error. The rest is fixed by altering the velocity to correct the actor over 1.0/.._RECIPFIXTIME seconds.
// So if .._AMOUNT is 1, we will always just move the actor directly to its correct position (as it the error was over .._THRESHOLD
// If .._AMOUNT is 0, we will correct just by changing the velocity.

#define KUPDATE_LINEAR_INTERP_THRESHOLD (20.0f)
#define KUPDATE_LINEAR_INTERP_AMOUNT (0.2f) // Karma scale
#define KUPDATE_LINEAR_INTERP_RECIPFIXTIME (1.0f)
#define KUPDATE_LINEAR_INTERP_VELOCITY_THRESHOLD_SQR (0.2f)

#define KUPADTE_ANGULAR_INTERP_THRESHOLD (0.2f * PI) // Radians
#define KUPDATE_ANGULAR_INTERP_SNAP_THRESHOLD (0.5f * PI) // Radians
#define KUPDATE_ANGULAR_INTERP_AMOUNT (0.0f)
#define KUPDATE_ANGULAR_INTERP_RECIPFIXTIME (1.0f)
#define KUPDATE_ANGULAR_ACCUM_ERROR_THRESHOLD 60.0f * KUPADTE_ANGULAR_INTERP_THRESHOLD // Corresponds to a second at 60 Hz.

inline void AActor::physKarma_internal(FLOAT deltaTime)
{
	check(Physics == PHYS_Karma);

    FRotator rot;
    FVector newPos, moveBy;
    FCheckResult Hit(1.0f);

    MdtWorldID world = this->GetLevel()->KWorld;
    if(!world)
    {
        debugf(TEXT("(Karma:) AActor::physKarma: No Karma MdtWorld found."));
        return;
    }

    McdModelID model = this->getKModel();
    if(!model)
        return;
    
    MdtBodyID body = McdModelGetBody(model);
    if(!body)
        return;

	// Handle any updates to the rigid body state from script.
	// Note: Because actors are always ticked before constraints, we can be sure the constraint will
	// get the most up-to-date state.
	FKRigidBodyState newState;
	UBOOL doUpdate = eventKUpdateState(newState);
	if(doUpdate)
	{
#if 1
		// Check the quaternion...
		MeVector4 quat = {newState.Quaternion.W, newState.Quaternion.X, newState.Quaternion.Y, newState.Quaternion.Z};
		MeReal k = MeVector4MagnitudeSqr(quat);
		if(ME_IS_ZERO_TOL(k, ME_MEDIUM_EPSILON))
		{
			debugf(NAME_DevKarma, TEXT("Invalid zero quaternion set for body. (%s)"), this->GetName() );
			return;
		}
		/* Else if quaternion is not unit length. */
		else if(!ME_IS_ZERO_TOL((k - 1.f), ME_MEDIUM_EPSILON))
		{
			debugf(NAME_DevKarma, TEXT("Quaternion (%f %f %f %f) with non-unit magnitude detected. (%s)"), quat[0], quat[1], quat[2], quat[3], this->GetName() );
			return;
		}
#endif

		MdtBodyEnable(body); // If getting new state from the server - make sure we are awake...

		MeVector3 oldPos;
		MdtBodyGetPosition(body, oldPos);

		if (!bSmoothKarmaStateUpdates)
		{
 			MdtBodySetPosition(body, K_U2MEScale * newState.Position.X, K_U2MEScale * newState.Position.Y, K_U2MEScale * newState.Position.Z );
			MdtBodySetQuaternion(body, newState.Quaternion.W, newState.Quaternion.X, newState.Quaternion.Y, newState.Quaternion.Z);
			MdtBodySetLinearVelocity(body, K_U2MEScale * newState.LinVel.X, K_U2MEScale * newState.LinVel.Y, K_U2MEScale * newState.LinVel.Z);
			MdtBodySetAngularVelocity(body, newState.AngVel.X, newState.AngVel.Y, newState.AngVel.Z);
		}
		else
		{
			MeVector3 newPos;
			newPos[0] = K_U2MEScale * newState.Position.X;
			newPos[1] = K_U2MEScale * newState.Position.Y;
			newPos[2] = K_U2MEScale * newState.Position.Z;

			MeVector4 oldQuat;
			MdtBodyGetQuaternion( body, oldQuat );

			MeVector4 newQuat;
			newQuat[0] = newState.Quaternion.W;
			newQuat[1] = newState.Quaternion.X;
			newQuat[2] = newState.Quaternion.Y;
			newQuat[3] = newState.Quaternion.Z;

			/////// POSITION CORRECTION ///////
			// Find out how much of a correction we are making
			MeVector3 delta;
			MeVector3Subtract(delta, newPos, oldPos);
			MeReal deltaMagSqr = MeVector3MagnitudeSqr(delta);

			// If its a small correction, only make a partial correction, and calculate a velocity that would fix it over 'fixTime'.
			MeVector3 setPos, fixLinVel, bodyVel;

			MdtBodyGetLinearVelocity(body, bodyVel);

			if(deltaMagSqr < KUPDATE_LINEAR_INTERP_THRESHOLD)
			{
				if (MeVector3MagnitudeSqr(bodyVel) < KUPDATE_LINEAR_INTERP_VELOCITY_THRESHOLD_SQR)
				{
					setPos[0] = KUPDATE_LINEAR_INTERP_AMOUNT * newPos[0] + (1.0f - KUPDATE_LINEAR_INTERP_AMOUNT) * oldPos[0];
					setPos[1] = KUPDATE_LINEAR_INTERP_AMOUNT * newPos[1] + (1.0f - KUPDATE_LINEAR_INTERP_AMOUNT) * oldPos[1];
					setPos[2] = KUPDATE_LINEAR_INTERP_AMOUNT * newPos[2] + (1.0f - KUPDATE_LINEAR_INTERP_AMOUNT) * oldPos[2];
				}
				else
				{
					setPos[0] = oldPos[0];
					setPos[1] = oldPos[1];
					setPos[2] = oldPos[2];
				}

				fixLinVel[0] = (newPos[0] - setPos[0]) * KUPDATE_LINEAR_INTERP_RECIPFIXTIME;
				fixLinVel[1] = (newPos[1] - setPos[1]) * KUPDATE_LINEAR_INTERP_RECIPFIXTIME;
				fixLinVel[2] = (newPos[2] - setPos[2]) * KUPDATE_LINEAR_INTERP_RECIPFIXTIME;
			}
			else
			{
				MeVector3Copy(setPos, newPos);
				MeVectorSetZero(fixLinVel, 3);
			}

			MdtBodySetPosition( body, setPos[0], setPos[1], setPos[2] );

			MdtBodySetLinearVelocity(body, 
				K_U2MEScale * newState.LinVel.X + fixLinVel[0], 
				K_U2MEScale * newState.LinVel.Y + fixLinVel[1], 
				K_U2MEScale * newState.LinVel.Z + fixLinVel[2]);

			/////// ORIENTATION CORRECTION ///////

			MeVector4 DeltaQuat, InvOldQuat;
			MeVector4Copy( InvOldQuat, oldQuat );
			InvOldQuat[0] *= -1.f;	// inverts scalar of quat

			// Get quaternion that takes us from old to new
			MeQuaternionProduct( DeltaQuat, newQuat, InvOldQuat );

			if( DeltaQuat[0] < 0.f ) 
			{
				for( INT i = 0; i < 4; i++ ) 
					DeltaQuat[i] *= -1.f;
			}

			MeVector3 DeltaAxis;
			DeltaAxis[0] = DeltaQuat[1];
			DeltaAxis[1] = DeltaQuat[2];
			DeltaAxis[2] = DeltaQuat[3];

			FLOAT CosDeltaAng = Clamp(DeltaQuat[0], -1.0f, 1.0f);
			FLOAT SinDeltaAng = Clamp(MeVector3Magnitude(DeltaAxis), -1.0f, 1.0f);
			FLOAT DeltaAng = 2.0f * appAtan2(SinDeltaAng, CosDeltaAng);

			MeVector3 fixAngVel;
			MeVector4 adjustQuat;

			if (Abs(DeltaAng) < KUPDATE_ANGULAR_INTERP_SNAP_THRESHOLD)
			{
				if (Abs(DeltaAng) < KUPADTE_ANGULAR_INTERP_THRESHOLD)
				{
					AccumKarmaAngleError = 0.0f;
				}
				else
				{
					AccumKarmaAngleError += Abs(DeltaAng);
				}

				if (AccumKarmaAngleError >= KUPDATE_ANGULAR_ACCUM_ERROR_THRESHOLD)
				{
					MeVector4Copy( adjustQuat, newQuat );
					MeVectorSetZero( fixAngVel, 3 );
					AccumKarmaAngleError = 0.0f;
				}
				else
				{
					MeQuaternionSlerp( adjustQuat, oldQuat, newQuat, KUPDATE_ANGULAR_INTERP_AMOUNT );

					MeVector3Normalize( DeltaAxis );

					fixAngVel[0] = DeltaAxis[0] * DeltaAng * (1.f - KUPDATE_ANGULAR_INTERP_AMOUNT) * (KUPDATE_ANGULAR_INTERP_RECIPFIXTIME);
					fixAngVel[1] = DeltaAxis[1] * DeltaAng * (1.f - KUPDATE_ANGULAR_INTERP_AMOUNT) * (KUPDATE_ANGULAR_INTERP_RECIPFIXTIME);
					fixAngVel[2] = DeltaAxis[2] * DeltaAng * (1.f - KUPDATE_ANGULAR_INTERP_AMOUNT) * (KUPDATE_ANGULAR_INTERP_RECIPFIXTIME);
				}
			}
			else
			{
				AccumKarmaAngleError = 0.0f;

				MeVector4Copy( adjustQuat, newQuat );
				MeVectorSetZero( fixAngVel, 3 );
			}

			MdtBodySetQuaternion(body, 
				adjustQuat[0], 
				adjustQuat[1], 
				adjustQuat[2], 
				adjustQuat[3]);

			MdtBodySetAngularVelocity(body, 
				newState.AngVel.X + fixAngVel[0], 
				newState.AngVel.Y + fixAngVel[1], 
				newState.AngVel.Z + fixAngVel[2]);
		}
		
		//MeMatrix4Ptr NewBodyTM = MdtBodyGetTransformPtr(body);
		//if( !MeMatrix4IsTM( NewBodyTM, ME_MEDIUM_EPSILON) )
		//{
		//	debugf( TEXT("Bad TM: %s"), this->GetName() );
		//	debugf( TEXT("M0 %f %f %f %f"), NewBodyTM[0][0], NewBodyTM[0][1], NewBodyTM[0][2], NewBodyTM[0][3] );
		//	debugf( TEXT("M1 %f %f %f %f"), NewBodyTM[1][0], NewBodyTM[1][1], NewBodyTM[1][2], NewBodyTM[1][3] );
		//	debugf( TEXT("M2 %f %f %f %f"), NewBodyTM[2][0], NewBodyTM[2][1], NewBodyTM[2][2], NewBodyTM[2][3] );
		//	debugf( TEXT("M3 %f %f %f %f"), NewBodyTM[3][0], NewBodyTM[3][1], NewBodyTM[3][2], NewBodyTM[3][3] );
		//}
	}

	// If this body is not enabled - we dont need to do anything more.
	// That includes updating its graphics position.
	if(!MdtBodyIsEnabled(body) || !KGData->bDoTick)
	{
		this->Velocity = FVector(0, 0, 0);
		return;
	}
}

// THe Karma collision detection/contact generation/simulation for this frame should have been called already by 
// this point. This function moves the graphics to where the physics are, and applys user forces etc.
// It also updates the position of constraints, and updates any controlling code.
void AActor::physKarma(FLOAT deltaTime)
{
    guard(AActor::physKarma);
	clock(GStats.DWORDStats(GEngineStats.STATS_Karma_physKarma));
    physKarma_internal(deltaTime);
	unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_physKarma));
    unguard;
}


//////////// AKVEHICLE C++ ////////////////

void AKVehicle::PostNetReceive()
{
	guard(AKVehicle::PostNetReceive);

	Super::PostNetReceive();
	eventVehicleStateReceived();

	unguard;
}

void AKVehicle::PostEditChange()
{
	guard(AKVehicle::PostEditChange);
	Super::PostEditChange();

	// Tell script that a parameters has changed, in case it needs to KUpdateConstraintParams on any constraints.
	this->eventKVehicleUpdateParams();
    unguard;
}

void AKVehicle::setPhysics(BYTE NewPhysics, AActor *NewFloor, FVector NewFloorV)
{
	guard(AKVehicle::setPhysics);

	check(Physics == PHYS_Karma);

	if(NewPhysics != PHYS_Karma)
	{
		debugf(TEXT("%s->setPhysics(%d). KVehicle's can only have Physics == PHYS_Karma."), this->GetName(), NewPhysics);
		return;
	}

	unguard;
}

void AKVehicle::TickAuthoritative( FLOAT DeltaSeconds )
{
	guard(AKVehicle::TickAuthoritative);

	check(Physics == PHYS_Karma); // karma vehicles should always be in PHYS_Karma

	eventTick(DeltaSeconds);
	ProcessState( DeltaSeconds );
	UpdateTimers(DeltaSeconds );

	// Update LifeSpan.
	if( LifeSpan!=0.f )
	{
		LifeSpan -= DeltaSeconds;
		if( LifeSpan <= 0.0001f )
		{
			GetLevel()->DestroyActor( this );
			return;
		}
	}

	// Perform physics.
	if ( !bDeleteMe )
		performPhysics( DeltaSeconds );

	unguard;
}

void AKVehicle::TickSimulated( FLOAT DeltaSeconds )
{
	guard(AKVehicle::TickSimulated);
	TickAuthoritative(DeltaSeconds);
	unguard;
}

#endif // WITH_KARMA

// Handy function for plotting script variables onto screen graph.
void AKVehicle::execGraphData( FFrame& Stack, RESULT_DECL )
{
	guard(AKVehicle::execGraphData);

	P_GET_STR(DataName);
	P_GET_FLOAT(DataValue);
    P_FINISH;

	// Make graph line name by concatenating vehicle name with data name.
	FString lineName = FString::Printf(TEXT("%s_%s"), 
		this->GetName(), (TCHAR*)DataName.GetCharArray().GetData());

	GStatGraph->AddDataPoint(lineName, DataValue, 1);

	unguard;
}

