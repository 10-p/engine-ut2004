/*============================================================================
	Karma Script Interface
    
    - Karma UnrealScript native functions
============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UKMeshProps);
IMPLEMENT_CLASS(UKarmaParamsCollision);
IMPLEMENT_CLASS(UKarmaParams);
IMPLEMENT_CLASS(UKarmaParamsRBFull);
IMPLEMENT_CLASS(UKarmaParamsSkel);
IMPLEMENT_CLASS(AKActor);
IMPLEMENT_CLASS(AKTire);
IMPLEMENT_CLASS(AKVehicle);
IMPLEMENT_CLASS(AKRepulsor);

// // //

void AActor::execKGetSimParams( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKGetRBQuaternion);

	P_GET_STRUCT_REF(FKSimParams, SimParams);
	P_FINISH;

#ifdef WITH_KARMA
	if(!KGData)
		return;
	
	*SimParams = KGData->SimParams;
#endif

	unguard;
}

void AActor::execKSetSimParams( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKGetRBQuaternion);

	P_GET_STRUCT(FKSimParams, SimParams);
	P_FINISH;

#ifdef WITH_KARMA
	if(!KGData)
		return;

	KGData->SimParams = SimParams;
#endif

	unguard;
}

#ifdef WITH_KARMA

// // //

void AActor::preContactUpdate()
{
	guard(AActor::preContactUpdate);

	// Repulsors dont work on ragdolls - only actors using 1-body karma
	if(Physics != PHYS_Karma)
		return;

	if(!KParams)
	{
		debugf(TEXT("preContactUpdate on Actor with no KParams"));
		return;
	}

	UKarmaParams* kparams = Cast<UKarmaParams>(KParams);
	if(!kparams)
		return;

	McdModelID model = getKModel();
	check(model);

	MdtBodyID body = McdModelGetBody(model);
	if(!body)
		return; // If there is no body - we can't 'repulse' it!

	for(INT i=0; i<kparams->Repulsors.Num(); i++)
	{
		AKRepulsor* rep = kparams->Repulsors(i);
		if(rep)
		{
			rep->bRepulsorInContact = false;

			// Do a line check and possibly create a contact with this actor.
			FVector start, end;
			start = rep->Location;
			end = start + (rep->CheckDist * rep->CheckDir);

			// Do a zero-extent trace
			FCheckResult Hit(1.f);
			FCheckResult *MultiHit;

			if(rep->bEnableRepulsion)				
			{
				if (rep->bRepulseWater)
				{
					FMemMark Mark(GMem);
					
					MultiHit = GetLevel()->MultiLineCheck( GMem, end, start, FVector(0, 0, 0), Level, TRACE_Volumes | TRACE_World | TRACE_Pawns, this);
					
					rep->bRepulsorOnWater = false;
					
					FCheckResult* Check;
					for( Check = MultiHit; Check!=NULL; Check=Check->GetNext() )
					{
						AVolume* V = Cast<AVolume>(Check->Actor);
						if (V)
						{
							APhysicsVolume* PV = Cast<APhysicsVolume>(V);
							if (PV && PV->bWaterVolume)
							{
								Hit = *Check;
								rep->bRepulsorOnWater = true;
								break;
							}
							continue;
						}
						Hit = *Check;
						break;
					}
					Mark.Pop();
				}				
				if (!Hit.Actor)
					GetLevel()->SingleLineCheck( Hit, this, end, start, TRACE_World | TRACE_Pawns);
			}
					
			// See if we want repulsors on this actor to hit the other actor. If not - blank out hit result.
			if( Hit.Actor && !KRepulsorsShouldHit(Hit.Actor) )
			{
				Hit.Time = 1.0f;
				Hit.Actor = NULL;
			}

			FVector WorldVel(0,0,0);
			if( Hit.Actor && Hit.Actor->GetAMover() )
				WorldVel = Hit.Actor->Velocity * 1.1f;

			MeVector3 MeWorldVel;
			KU2MEPosition(MeWorldVel, WorldVel);

			if(KGData->DebugDrawOpt & KDRAW_Repulsors)
			{
				// DEBUG DRAWING
				FVector mid;
				mid = start + ( Hit.Time * rep->CheckDir * rep->CheckDist );

				GTempLineBatcher->AddLine(start, mid, FColor(0, 0, 255));
				GTempLineBatcher->AddLine(mid, end, FColor(255, 0, 0));
			}

			MdtContactGroupID cg = (MdtContactGroupID)rep->KContact;

			// Hit something that is flat enough to be considered 'ground' 
			if( Hit.Time < 1.f && Hit.Normal.Z > UCONST_MINFLOORZ )
			{
				// Create a contact group and configure first (if necessary)
				if(!cg)
				{
					cg = MdtContactGroupCreate( GetLevel()->KWorld );
					MdtContactGroupSetBodies(cg, body, 0);
					MdtContactGroupEnable(cg);

					rep->KContact = (PTRINT)cg;
				}

				check(cg); // Should always have a contact group by this point

				if(MdtContactGroupGetCount(cg) == 0)
					MdtContactGroupCreateContact(cg);

				check(MdtContactGroupGetCount(cg) == 1);

				MdtContactID contact = MdtContactGroupGetFirstContact(cg);

				// Configure contact
				MeVector3 pos;
				MdtContactParamsID params = MdtContactGetParams(contact);

				// Put contact at end of line.
				KU2MEPosition(pos, end); 
				MdtContactSetPosition(contact, pos[0], pos[1], pos[2]);

				// Normal points back along line.
				MdtContactSetNormal(contact, -1.f * rep->CheckDir.X, -1.f * rep->CheckDir.Y, -1.f * rep->CheckDir.Z);

				// Penetration is distance from hit point to end of line
				MdtContactSetPenetration(contact, K_U2MEScale * (rep->PenOffset + (1.f - Hit.Time) * rep->CheckDist * rep->PenScale));

				MdtContactSetWorldVelocity(contact, MeWorldVel[0], MeWorldVel[1], MeWorldVel[2]);				

				// PARAMS
				MdtContactParamsSetSoftness( params, MeFabs(rep->Softness) );
				MdtContactParamsSetType( params, MdtContactTypeFrictionZero ); 

				rep->bRepulsorInContact = true;

				// Contact group should be enabled
				if(!MdtContactGroupIsEnabled(cg))
					MdtContactGroupEnable(cg);
			}
			else
			{
				// Didn't hit anything - empty the contact group. Should only ever have at most one contact in!
				if(cg)
				{
					check(MdtContactGroupGetCount(cg) < 2);

					MdtContactID dynC = MdtContactGroupGetFirstContact(cg);
					if(dynC != MdtContactInvalidID)
					{
						MdtContactGroupDestroyContact(cg, dynC);
					}

					// Contact group should be disabled
					if(MdtContactGroupIsEnabled(cg))
						MdtContactGroupDisable(cg);
				}
			}

		}
	}

	unguard;
}

// // //
void AKTire::preContactUpdate()
{
	guard(AKTire::preContactUpdate);

	Super::preContactUpdate();

	GroundSlipVec = FVector(0, 0, 0);
	GroundSlipVel = 0.f;
	GroundMaterial = NULL;
	bTireOnGround = 0;

	unguard;
}

#endif

// // //
void AActor::execKGetRBQuaternion( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execKGetRBQuaternion);
	P_FINISH;

	FQuat Res(0, 0, 0, 1);

#ifdef WITH_KARMA
	McdModelID model = this->getKModel();
	if(!model)
	{
		*(FQuat*)Result = Res;
		return;
	}

	MdtBodyID body = McdModelGetBody(model);
	if(!body)
	{
		*(FQuat*)Result = Res;
		return;
	}

	MeVector4 q;
	MdtBodyGetQuaternion(body, q);
	Res.W = q[0];
	Res.X = q[1];
	Res.Y = q[2];
	Res.Z = q[3];
#endif

	*(FQuat*)Result = Res;

	unguardexecSlow;
}

void AActor::execKGetRigidBodyState( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKGetRigidBodyState);

	P_GET_STRUCT_REF(FKRigidBodyState, RBstate);
	P_FINISH;

#ifdef WITH_KARMA
	KGetRigidBodyState(RBstate);
#else
	appMemset( RBstate, 0, sizeof(FKRigidBodyState) );
	RBstate->Quaternion.W = 1.0f;
#endif

	unguard;
}

void AActor::execKDrawRigidBodyState( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKDrawRigidBodyState);

	P_GET_STRUCT(FKRigidBodyState, RBstate);
	P_GET_UBOOL(AltColour);
	P_FINISH;

#ifdef WITH_KARMA
	KDrawRigidBodyState(&RBstate, AltColour);
#endif

	unguard;
}

void AActor::execKRBVecToVector( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKRBVecToVector);

	P_GET_STRUCT(FKRBVec, RBvec);
	P_FINISH;

	FVector vec(RBvec.X, RBvec.Y, RBvec.Z);
	*(FVector*)Result = vec;

	unguard;
}

void AActor::execKRBVecFromVector( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKRBVecFromVector);

	P_GET_VECTOR(v);
	P_FINISH;

	FKRBVec RBvec;
	RBvec.X = v.X;
	RBvec.Y = v.Y;
	RBvec.Z = v.Z;
	*(FKRBVec*)Result = RBvec;

	unguard;
}

/*** MASS ***/

void AActor::execKSetMass( FFrame& Stack, RESULT_DECL )
{
    guard(AActor::execKSetMass);

    P_GET_FLOAT(mass);
    P_FINISH;
    
    if(!this->KParams)
        return;

	UKarmaParams* kparams = Cast<UKarmaParams>(this->KParams);
	if(!kparams)
		return;

    kparams->KMass = mass;

#ifdef WITH_KARMA
    kparams->PostEditChange(); // sync with Karma
#endif

    unguard;
}

void AActor::execKGetMass( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKGetMass);

	P_FINISH;

    if(!this->KParams)
        return;

	UKarmaParams* kparams = Cast<UKarmaParams>(this->KParams);
	if(!kparams)
		return;

    *(FLOAT*)Result = kparams->KMass;

    unguard;
}

/*** (Normalized) INERTIA TENSOR ***/

void AActor::execKGetInertiaTensor( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKGetInertiaTensor);

    P_GET_VECTOR_REF(it2);
    P_GET_VECTOR_REF(it1);
    P_FINISH;
    
    if(!this->KParams)
        return;


	// If this actor has the full (inertia/com-pos override) KParams, return that.
	UKarmaParamsRBFull* fullParams = Cast<UKarmaParamsRBFull>(this->KParams);
	if(fullParams)
	{
		it1->X = fullParams->KInertiaTensor[0];
		it1->Y = fullParams->KInertiaTensor[1];
		it1->Z = fullParams->KInertiaTensor[2];
		
		it2->X = fullParams->KInertiaTensor[3];
		it2->Y = fullParams->KInertiaTensor[4];
		it2->Z = fullParams->KInertiaTensor[5];
	}
	else // otherwise get inertia tensor params from static mesh
	{
		UStaticMesh* smesh = this->StaticMesh;
		if(smesh && smesh->KPhysicsProps)
		{
			it1->X = smesh->KPhysicsProps->InertiaTensor[0];
			it1->Y = smesh->KPhysicsProps->InertiaTensor[1];
			it1->Z = smesh->KPhysicsProps->InertiaTensor[2];
			
			it2->X = smesh->KPhysicsProps->InertiaTensor[3];
			it2->Y = smesh->KPhysicsProps->InertiaTensor[4];
			it2->Z = smesh->KPhysicsProps->InertiaTensor[5];
		}
	}

    unguard;
}

// Inertia tensor of object, if it had a mass of 1:
// (0 1 2)
// (1 3 4)
// (2 4 5)
void AActor::execKSetInertiaTensor( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKSetInertiaTensor);

	P_GET_VECTOR(it1);
    P_GET_VECTOR(it2);
	P_FINISH;

    if(!this->KParams)
        return;

	// we can only do this on the fly if this actor uses a UKarmaParamsRBFull for its KParams.
	// KTODO: We could just spawn one here if its not?
	UKarmaParamsRBFull* fullParams = Cast<UKarmaParamsRBFull>(this->KParams);
	if(fullParams)
	{
		fullParams->KInertiaTensor[0] = it1.X;
		fullParams->KInertiaTensor[1] = it1.Y;
		fullParams->KInertiaTensor[2] = it1.Z;
		
		fullParams->KInertiaTensor[3] = it2.X;
		fullParams->KInertiaTensor[4] = it2.Y;
		fullParams->KInertiaTensor[5] = it2.Z;
		
#ifdef WITH_KARMA
		this->KParams->PostEditChange(); // sync with Karma
#endif
	}

    unguard;
}

/*** CENTRE OF MASS OFFSET ***/

void AActor::execKSetCOMOffset( FFrame& Stack, RESULT_DECL )
{
    guard(AActor::execKSetCOMOffset);
    
    P_GET_VECTOR(offset);
    P_FINISH;

    if(!this->KParams)
        return;

	// we can only do this on the fly if this actor uses a UKarmaParamsRBFull for its KParams.
	// KTODO: We could just spawn one here if its not?
	UKarmaParamsRBFull* fullParams = Cast<UKarmaParamsRBFull>(this->KParams);
	if(fullParams)
	{
		fullParams->KCOMOffset = offset;

#ifdef WITH_KARMA
		this->KParams->PostEditChange(); // sync with Karma
#endif
	}

    unguard;
}

void AActor::execKGetCOMOffset( FFrame& Stack, RESULT_DECL )
{
    guard(AActor::execKGetCOMOffset);
    
    P_GET_VECTOR_REF(offset);
    P_FINISH;
    
    if(!this->KParams)
        return;

	// If this actor has the full (inertia/com-pos override) KParams, return that.
	UKarmaParamsRBFull* fullParams = Cast<UKarmaParamsRBFull>(this->KParams);
	if(fullParams)
	{
		*offset = fullParams->KCOMOffset;
	}
	else // otherwise get inertia tensor params from static mesh
	{
		UStaticMesh* smesh = this->StaticMesh;
		if(smesh && smesh->KPhysicsProps)
			*offset = smesh->KPhysicsProps->COMOffset;
	}
    
    unguard;
}

void AActor::execKGetCOMPosition( FFrame& Stack, RESULT_DECL )
{
    guard(AActor::execKGetCOMPosition);
    
    P_GET_VECTOR_REF(pos);
    P_FINISH;

#ifdef WITH_KARMA
	McdModelID model = this->getKModel();
    if(!model)
        return;

	MdtBodyID body = McdModelGetBody(model);
	if(!body)
		return;

	MeVector3 mePos;
	MdtBodyGetCenterOfMassPosition(body, mePos);
	KME2UPosition(pos, mePos);
#else
	pos->X = pos->Y = pos->Z = 0;
#endif

    unguard;
}


/*** DAMPING ***/

void AActor::execKSetDampingProps( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKSetDampingProps);

    P_GET_FLOAT(lindamp);
    P_GET_FLOAT(angdamp);
	P_FINISH;

    if(!this->KParams)
        return;

	UKarmaParams* kparams = Cast<UKarmaParams>(this->KParams);
	if(!kparams)
		return;

    kparams->KLinearDamping = lindamp;
    kparams->KAngularDamping = angdamp;

#ifdef WITH_KARMA
    kparams->PostEditChange(); // sync with Karma
#endif

    unguard;
}

void AActor::execKGetDampingProps( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKGetDampingProps);

	P_GET_FLOAT_REF(lindamp);
    P_GET_FLOAT_REF(angdamp);
	P_FINISH;

    if(!this->KParams)
        return;

	UKarmaParams* kparams = Cast<UKarmaParams>(this->KParams);
	if(!kparams)
		return;

    *lindamp = kparams->KLinearDamping;
    *angdamp = kparams->KAngularDamping;

    unguard;
}

/*** RESTITUTION ***/

void AActor::execKSetRestitution( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKSetRestitution);

	P_GET_FLOAT(rest);
    P_FINISH;

    if(!this->KParams)
        return;

    this->KParams->KRestitution = rest;

    unguard;
}

void AActor::execKGetRestitution( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKGetRestitution);

	P_FINISH;

    if(!this->KParams)
        return;

    *(FLOAT*)Result = this->KParams->KRestitution;

    unguard;
}

/*** FRICTION ***/

void AActor::execKSetFriction( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKSetFriction);

	P_GET_FLOAT(friction);
    P_FINISH;

    if(!this->KParams)
        return;

    this->KParams->KFriction = friction;

    unguard;
}

void AActor::execKGetFriction( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKGetFriction);

	P_FINISH;

    if(!this->KParams)
        return;

    *(FLOAT*)Result = this->KParams->KFriction;

    unguard;
}

/*** IMPACT THRESHOLD ***/

void AActor::execKSetImpactThreshold( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKSetImpactThreshold);

	P_GET_FLOAT(thresh);
    P_FINISH;

    if(!this->KParams)
        return;

    this->KParams->KImpactThreshold = thresh;

    unguard;
}

void AActor::execKGetImpactThreshold( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKGetImpactThreshold);

	P_FINISH;

    if(!this->KParams)
        return;

    *(FLOAT*)Result = this->KParams->KImpactThreshold;

    unguard;
}

/*** ACTOR GRAV SCALE ***/
void AActor::execKSetActorGravScale( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKSetActorGravScale);

	P_GET_FLOAT(ActorGravScale);
    P_FINISH;

    if(!this->KParams)
        return;

	UKarmaParams* kparams = Cast<UKarmaParams>(this->KParams);
	if(!kparams)
		return;

    kparams->KActorGravScale = ActorGravScale;

    unguard;
}

void AActor::execKGetActorGravScale( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKGetActorGravScale);

	P_FINISH;

    if(!this->KParams)
        return;

	UKarmaParams* kparams = Cast<UKarmaParams>(this->KParams);
	if(!kparams)
		return;

    *(FLOAT*)Result = kparams->KActorGravScale;

    unguard;
}

/*********************************************/

// Change the state of bBlockKarma
void AActor::execKSetBlockKarma( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKSetBlockKarma);

	P_GET_UBOOL(newBlock);
	P_FINISH;

#ifdef WITH_KARMA
	// Do nothing if state is already right,or is a constraint.
	if(bBlockKarma == newBlock || this->IsA(AKConstraint::StaticClass()))
		return;

	// This will do the dirty work of changing the model geometry, 
	// or creating a new one if necessary.
	KSetActorCollision(this, newBlock);

	bBlockKarma = newBlock;

	bShouldStopKarma = KShouldStopKarma(this); // Cache this for speed.
#endif
	
	unguard;
}

// When Actors come to rest, they are automatically removed from simulation.
// This function can be called to 'force' things back into simulation.
void AActor::execKWake( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKWake);

	P_FINISH;

#ifdef WITH_KARMA
	KWake();
#endif

    unguard;
}

// Return if this actor's body is currently being simualted
// JTODO: Fix for ragdolls.
void AActor::execKIsAwake( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKIsAwake);

	P_FINISH;

#ifdef WITH_KARMA
	if( KIsAwake() )
		*(UBOOL*)Result = 1;
	else
		*(UBOOL*)Result = 0;
#else
	*(UBOOL*)Result = 0;
#endif

    unguard;
}

// Add an instantaneous impulse to the Karma physics off this Actor.
// This is useful for shooting stuff.
void AActor::execKAddImpulse( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKAddImpulse);

	P_GET_VECTOR(Impulse);
    P_GET_VECTOR(Position);
	P_GET_NAME_OPTX(BoneName, NAME_None);

	P_FINISH;

#ifdef WITH_KARMA
	KAddImpulse(Impulse, Position, BoneName);
#endif
    unguard;
}

void AActor::execKAddAngularImpulse( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKAddAngularImpulse);

	P_GET_VECTOR(AngImpulse);

	P_FINISH;

#ifdef WITH_KARMA
	KAddAngularImpulse(AngImpulse);
#endif
	unguard;
}

// Set the velocity (in world ref frame) of all bones in the ragdoll to Velocity.
// AddToCurrent indicates whether to add the linear part to the current linear velocity of ragdoll.
void AActor::execKSetSkelVel( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKSetSkelVel);

	P_GET_VECTOR(Velocity);
	P_GET_VECTOR_OPTX(AngVelocity, FVector(0, 0, 0));
	P_GET_UBOOL_OPTX(AddToCurrent, 0);
	P_FINISH;

#ifdef WITH_KARMA
	RTN_WITH_ERR_IF(!MeshInstance || !MeshInstance->IsA(USkeletalMeshInstance::StaticClass()), 
		"(Karma:) execKSetSkelVel: No skeletal mesh.");

	USkeletalMeshInstance* inst = Cast<USkeletalMeshInstance>(MeshInstance);

	KSetSkelVel(inst, Velocity, AngVelocity);
	
#endif

	unguard;
}

// Apply a constant force to a particular bone, until turned off.
// RampTime indicates the time taken for force to reach full strength.
void AActor::execKAddBoneLifter( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKAddBoneLifter);

	P_GET_NAME(BoneName);
	P_GET_STRUCT(FInterpCurveInit, LiftVel);
	P_GET_FLOAT(LateralFriction);
	P_GET_STRUCT(FInterpCurveInit, Softness);
	P_FINISH;

#ifdef WITH_KARMA
	RTN_WITH_ERR_IF(!MeshInstance || !MeshInstance->IsA(USkeletalMeshInstance::StaticClass()), 
		"(Karma:) execKAddBoneLifter: No skeletal mesh.");

	USkeletalMeshInstance* inst = Cast<USkeletalMeshInstance>(this->MeshInstance);
	RTN_WITH_ERR_IF(!inst->KSkelIsInitialised, "(Karma:) execKAddBoneLifter: Ragdoll not initialised.");

	INT boneIx = inst->MatchRefBone(BoneName);
	RTN_WITH_ERR_IF(boneIx == INDEX_NONE, "(Karma:) execKAddBoneLifter: Bone not found.");

	McdModelID model = inst->KSkelModels(boneIx);
	RTN_WITH_ERR_IF(!model, "(Karma:) execKAddBoneLifter: Bone has no Karma model.");

	MdtBodyID body = McdModelGetBody(model);
	RTN_WITH_ERR_IF(!body, "(Karma:) execKAddBoneLifter: Bone has no dynamics.");

	// Ok - so the bone is fine, add a new contact-powered bone lifter.
	FKBoneLifter* lifter = new(inst->KBoneLifters)FKBoneLifter(boneIx, &LiftVel, LateralFriction, &Softness);

	// Create contact group for lifting.
	ULevel* level = GetLevel();
	MdtContactGroupID cg = MdtContactGroupCreate(level->KWorld);
	MdtContactGroupSetBodies(cg, body, 0);
	MdtContactGroupEnable(cg);
	lifter->LiftContact = cg;
#endif

	unguard;
}

// Remove all constant forces from a particular bone.
void AActor::execKRemoveLifterFromBone( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKRemoveLifterFromBone);

	P_GET_NAME(BoneName);
	P_FINISH;

#ifdef WITH_KARMA
	RTN_WITH_ERR_IF(!MeshInstance || !MeshInstance->IsA(USkeletalMeshInstance::StaticClass()), 
		"(Karma:) execKRemoveLifterFromBone: No skeletal mesh.");

	USkeletalMeshInstance* inst = Cast<USkeletalMeshInstance>(this->MeshInstance);

	INT boneIx = inst->MatchRefBone(BoneName);
	RTN_WITH_ERR_IF(boneIx == INDEX_NONE, "(Karma:) execKRemoveLifterFromBone: Bone not found.");

	// Go through list, removing any forces that apply to this bone.
	INT i=0;
	while(i < inst->KBoneLifters.Num())
	{
		FKBoneLifter* f = &inst->KBoneLifters(i);
		if(f->BoneIndex == boneIx)
		{
			MdtContactGroupID cg = inst->KBoneLifters(i).LiftContact;
			check(cg);
			MdtContactGroupDisable(cg);
			MdtContactGroupDestroy(cg);

			inst->KBoneLifters.Remove(i);
		}
		else
			i++;
	}
#endif

	unguard;
}

// Remove all constant forces from all bones.
void AActor::execKRemoveAllBoneLifters( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKRemoveAllBoneLifters);

	P_FINISH;

#ifdef WITH_KARMA
	RTN_WITH_ERR_IF(!MeshInstance || !MeshInstance->IsA(USkeletalMeshInstance::StaticClass()), 
		"(Karma:) execKRemoveAllBoneLifters: No skeletal mesh.");

	USkeletalMeshInstance* inst = Cast<USkeletalMeshInstance>(this->MeshInstance);

	for(INT i=0; i<inst->KBoneLifters.Num(); i++)
	{
		MdtContactGroupID cg = inst->KBoneLifters(i).LiftContact;
		check(cg);
		MdtContactGroupDisable(cg);
		MdtContactGroupDestroy(cg);
	}
	inst->KBoneLifters.Empty(); // This calls MdtContactGroupDestroy from the destructor.
#endif

	unguard;
}

// Get entire mass of skeleton physics
void AActor::execKGetSkelMass( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKGetSkelMass);

	P_FINISH;

	*(FLOAT*)Result = 0.f;

#ifdef WITH_KARMA
	RTN_WITH_ERR_IF(!MeshInstance || !MeshInstance->IsA(USkeletalMeshInstance::StaticClass()), 
		"(Karma:) execKGetSkelMass: No skeletal mesh.");

	USkeletalMeshInstance* inst = Cast<USkeletalMeshInstance>(this->MeshInstance);
	RTN_WITH_ERR_IF(!inst->KSkelIsInitialised, 
		"(Karma:) execKGetSkelMass: Ragdoll not initialised.");

	FLOAT totalMass = 0.f;
	for(INT i=0; i<inst->KSkelModels.Num(); i++)
	{
		McdModelID model = inst->KSkelModels(i);
		if(model)
		{
			MdtBodyID body = McdModelGetBody(model);
			if(body)
			{
				totalMass += MdtBodyGetMass(body);
			}
		}
	}

	RTN_WITH_ERR_IF(totalMass == 0.f, 
		"(Karma:) execKGetSkelMass: Total mass of zero.");

	*(FLOAT*)Result = totalMass;
#endif

	unguard;
}

// Turn off physics for this ragdoll (doesn't change Physics from PHYS_KarmaRagdoll though), 
// but keep it in its current pose.
void AActor::execKFreezeRagdoll( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKFreezeRagdoll);

	P_FINISH;

#ifdef WITH_KARMA
	this->KFreezeRagdoll();
#endif

	unguard;
}

// Find if there is a free ragdoll
void AActor::execKIsRagdollAvailable( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKIsRagdollAvailable);

	P_FINISH;

#ifdef WITH_KARMA
	if(XLevel && Level && (XLevel->Ragdolls.Num() < Level->MaxRagdolls) )
		*(DWORD*)Result = 1;
	else
		*(DWORD*)Result = 0;
#else
	*(DWORD*)Result = 0;
#endif

	unguard;
}

// Try and free up a ragdoll.
// If there is not a ragdoll 'slot' available, it will take the oldest ragdoll and 
// freeze it, freeing up a slot. 
void AActor::execKMakeRagdollAvailable( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKMakeRagdollAvailable);

	P_FINISH;

#ifdef WITH_KARMA
	if(!Level || !XLevel)
		return;

	// We can never make a ragdoll available if the max is zero.
	if(Level->MaxRagdolls == 0)
		return;

	// If there is already one available, we don't need to do anything.
	if(XLevel->Ragdolls.Num() < Level->MaxRagdolls)
		return;

	check(XLevel->Ragdolls.Num() > 0);

	// Freeze the oldest, non-'important' ragdoll. Array is in age order (oldest first)
	UBOOL doneFrozen = 0;
	for(INT i=0; i<XLevel->Ragdolls.Num() && !doneFrozen; i++)
	{
		AActor* ragdoll = XLevel->Ragdolls(i);
		check(ragdoll->KParams);
		UKarmaParamsSkel* skelParams = Cast<UKarmaParamsSkel>(ragdoll->KParams);
		check(skelParams);
		if(!skelParams->bKImportantRagdoll)
		{
			ragdoll->KFreezeRagdoll();
			doneFrozen = 1;
		}
	}

	// If we still dont have a free ragdoll - warn
	if(XLevel->Ragdolls.Num() >= Level->MaxRagdolls)
		debugf(TEXT("execKMakeRagdollAvailable: No Ragdoll Available."));
#endif

	unguard;
}

// Disable Karma contact generation between this actor, and another actor.
void AActor::execKDisableCollision( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKDisableCollision);

	P_GET_ACTOR(Other);
	P_FINISH;

	if(!Other)
		return;

#ifdef WITH_KARMA
	ULevel* level = this->GetLevel();

	if(!level)
		return;

	McdModelID m1 = this->getKModel();
	McdModelID m2 = Other->getKModel();

	if(m1 && m2 && m1 != m2)
		KDisablePairCollision(m1, m2, level);
#endif

	unguard;
}

// Enable Karma contact generation between this actor, and another actor.
// By default, collision detection is on between all Karma actors.
void AActor::execKEnableCollision( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKEnableCollision);

	P_GET_ACTOR(Other);
	P_FINISH;

	if(!Other)
		return;

#ifdef WITH_KARMA
	ULevel* level = this->GetLevel();

	if(!level)
		return;

	McdModelID m1 = this->getKModel();
	McdModelID m2 = Other->getKModel();

	if(m1 && m2 && m1 != m2)
		KEnablePairCollision(m1, m2, level);
#endif

	unguard;
}

// 
void AActor::execKSetStayUpright( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKSetStayUpright);

	P_GET_UBOOL(stayUpright);
	P_GET_UBOOL(allowRotate);
	P_FINISH;

    if(!this->KParams)
        return;

	UKarmaParams* kparams = Cast<UKarmaParams>(this->KParams);
	if(!kparams)
		return;

    kparams->bKStayUpright = stayUpright;
    kparams->bKAllowRotate = allowRotate;
	
#ifdef WITH_KARMA
    kparams->PostEditChange(); // sync with Karma
#endif

	unguard;
}

//
void AActor::execKSetStayUprightParams( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execKSetStayUprightParams);

	P_GET_FLOAT(stiffness);
	P_GET_FLOAT(damping);
	P_FINISH;

    if(!this->KParams)
        return;

	UKarmaParams* kparams = Cast<UKarmaParams>(this->KParams);
	if(!kparams)
		return;

	kparams->StayUprightStiffness = stiffness;
	kparams->StayUprightDamping = damping;

#ifdef WITH_KARMA
    kparams->PostEditChange(); // sync with Karma
#endif

	unguard;
}