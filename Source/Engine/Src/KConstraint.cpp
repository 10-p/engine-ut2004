/*============================================================================
	Karma Constraint Actor
============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(AKConstraint);
IMPLEMENT_CLASS(AKBSJoint);
IMPLEMENT_CLASS(AKHinge);
IMPLEMENT_CLASS(AKConeLimit);
IMPLEMENT_CLASS(AKCarWheelJoint);

#ifdef WITH_KARMA

///////////////// INIT AND TERM //////////////////////////

/*  If this is a constraint actor, find its bodies and craete it. 
    Will only do this if Physics == PHYS_Karma. 
	If attaching something to the world, it must be the second actor which is 'null'. */
void MEAPI KInitConstraintKarma(AKConstraint* con)
{
    guard(KInitConstraintKarma);

    if(!KGData || con->bDeleteMe) 
        return;

    KSetSecName(TEXT("KARMA: INIT CONSTRAINT"));

    ULevel* level = con->GetLevel();
	MdtWorldID world = level->KWorld;
    MdtBodyID b1 = 0, b2 = 0;
    McdModelID m1 = 0, m2 = 0;
    
    if(con == NULL || GIsEditor || con->Physics != PHYS_Karma || con->bDeleteMe)
        return;
    
	if(!con->KConstraintActor1 && con->KConstraintActor2)
	{
		debugf(TEXT("(Karma): KInitConstraintKarma: If joint is to world, second actor must be NULL."));
		con->Physics = PHYS_None;
		return;
	}

    // if this constraint has no actors set, ensure physics is PHYS_None
    if(!con->KConstraintActor1 && !con->KConstraintActor2)
    {
        con->Physics = PHYS_None;
        return;
    }

	if(con->KConstraintActor1->bDeleteMe || (con->KConstraintActor2 && con->KConstraintActor2->bDeleteMe))
	{
		debugf(TEXT("(Karma): KInitConstraintKarma: Creating constraint to destroyed actors."));
		con->Physics = PHYS_None;
		return;
	}

    RTN_WITH_ERR_IF(con->getKConstraint(), "(Karma): KInitConstraintKarma: Already initiaslised.");
    RTN_WITH_ERR_IF(!level->KWorld, "(Karma): KInitConstraintKarma: No MdtWorld.");

	UKarmaParams *kparams1 = 0, *kparams2 = 0;
	if(con->KConstraintActor1->KParams)
		kparams1 = Cast<UKarmaParams>(con->KConstraintActor1->KParams);
	if(con->KConstraintActor2 && con->KConstraintActor1->KParams)
		kparams2 = Cast<UKarmaParams>(con->KConstraintActor2->KParams);

	if(kparams1 && kparams2 && kparams1->bKDoubleTickRate != kparams2->bKDoubleTickRate)
	{
		debugf(TEXT("(Karma): KInitConstraintKarma: Creating constraint between actors with different bKDoubleTickRate."));
		con->Physics = PHYS_None;
		return;
	}

    MeVector3 mePos;
	KU2MEPosition(mePos, con->Location);

	// ensure connected actors Karma is init'ed first
	KInitActorKarma(con->KConstraintActor1); 
	KFindNearestActorBody(con->KConstraintActor1, mePos, con->KConstraintBone1, &b1, &m1);

	if(con->KConstraintActor2)
	{
		KInitActorKarma(con->KConstraintActor2);
		KFindNearestActorBody(con->KConstraintActor2, mePos, con->KConstraintBone2, &b2, &m2);
	}

	RTN_WITH_ERR_IF(b1 == 0, "(Karma): KInitConstraintKarma: KConstraint with no valid bodies.");
	RTN_WITH_ERR_IF(b1 == b2, "(Karma): KInitConstraintKarma: KConstraint with both bodies the same.");

	
	MdtConstraintID mdtCon = 0;

	guard(CreateConstraint);

	if(con->IsA(AKBSJoint::StaticClass()))
	{
		MdtBSJointID bs = MdtBSJointCreate(world);
		mdtCon = MdtBSJointQuaConstraint(bs);
	}
	else if(con->IsA(AKHinge::StaticClass()))
	{
		MdtHingeID h = MdtHingeCreate(world);
		mdtCon = MdtHingeQuaConstraint(h);
	}
	else if(con->IsA(AKCarWheelJoint::StaticClass()))
	{
		MdtCarWheelID cw = MdtCarWheelCreate(world);
		mdtCon = MdtCarWheelQuaConstraint(cw);
	}
	else if(con->IsA(AKConeLimit::StaticClass()))
	{
		MdtConeLimitID cl = MdtConeLimitCreate(world);
		mdtCon = MdtConeLimitQuaConstraint(cl);
	}

	RTN_WITH_ERR_IF(!mdtCon, "(Karma): KInitConstraintKarma: Could not create MdtConstraint.");

    MdtConstraintSetBodies(mdtCon, b1, b2);
	
    //  See if we have to re-calculate the rel pos/axis now.
	// 
	//  Usually done for constraints created inside UnrealEd.
	//  This is only for 829 backwards compatibility. Now constraint 
	//  relative axis/postiion calculate in PostEditMove within UnrealEd.
	// 
	//  We also do this for joints to things with PHYS_KarmaRagDoll, 
	//  because the bones dont actually exist until runtime.
    if(con->bKForceFrameUpdate == 1 || 
		(con->KConstraintActor1 && con->KConstraintActor1->Physics == PHYS_KarmaRagDoll) || 
		(con->KConstraintActor2 && con->KConstraintActor2->Physics == PHYS_KarmaRagDoll))
    {
        /* Calculate position/axis from constraint actor. */
        FCoords coords = FCoords(FVector(0, 0, 0));
        coords *= con->Rotation;
        
        /* World ref frame */
        MeVector3 wPos, wPri, wOrth;
        KU2MEPosition(wPos, con->Location);
        
        wPri[0] = coords.XAxis.X;
        wPri[1] = coords.YAxis.X;
        wPri[2] = coords.ZAxis.X;
        
        wOrth[0] = coords.XAxis.Y;
        wOrth[1] = coords.YAxis.Y;
        wOrth[2] = coords.ZAxis.Y;

        MeVector3 lPos, lPri, lOrth;
		
		if(con->bKForceFrameUpdate == 1 || (con->KConstraintActor1 && con->KConstraintActor1->Physics == PHYS_KarmaRagDoll))
		{
			MdtConvertPositionVector(0, wPos, b1, lPos);
			MdtConvertVector(0, wPri, b1, lPri);
			MdtConvertVector(0, wOrth, b1, lOrth);
			
			KME2UVecCopy(&con->KPos1, lPos);
			KME2UVecCopy(&con->KPriAxis1, lPri);
			KME2UVecCopy(&con->KSecAxis1, lOrth);
		}
        
		if(con->bKForceFrameUpdate == 1 || (con->KConstraintActor2 && con->KConstraintActor2->Physics == PHYS_KarmaRagDoll))
		{
			MdtConvertPositionVector(0, wPos, b2, lPos);
			MdtConvertVector(0, wPri, b2, lPri);
			MdtConvertVector(0, wOrth, b2, lOrth);
			
			KME2UVecCopy(&con->KPos2, lPos);
			KME2UVecCopy(&con->KPriAxis2, lPri);
			KME2UVecCopy(&con->KSecAxis2, lOrth);
		}
		
        con->bKForceFrameUpdate = false;
    }

    /*  Set position and axis of constraint relative to 
	both bodies reference frames. */

    /*** BODY 1 ***/
    MdtConstraintBodySetPositionRel(mdtCon, 0, 
		con->KPos1.X, con->KPos1.Y, con->KPos1.Z);
    MdtConstraintBodySetAxesRel(mdtCon, 0, 
        con->KPriAxis1.X, con->KPriAxis1.Y, con->KPriAxis1.Z, 
        con->KSecAxis1.X, con->KSecAxis1.Y, con->KSecAxis1.Z);
	
    /*** BODY 2 ***/
    MdtConstraintBodySetPositionRel(mdtCon, 1, 
		con->KPos2.X, con->KPos2.Y, con->KPos2.Z);
    MdtConstraintBodySetAxesRel(mdtCon, 1, 
        con->KPriAxis2.X, con->KPriAxis2.Y, con->KPriAxis2.Z, 
        con->KSecAxis2.X, con->KSecAxis2.Y, con->KSecAxis2.Z);

#if 0
    /* Constraint-specific configuration. */
	if(con->IsA(AKCarWheelJoint::StaticClass()))
    {
        MdtCarWheelID cw = MdtConstraintDCastCarWheel(mdtCon);
		
        /* Because car-wheel doesn't use the axis set above, we have to do them here. */

		// HACK - Because CarWheel stores position rel to chassis COM (NOT chassis origin)
		// have to compensate here. This WILL be fixed in Karma soon!!
		MeVector3 relPos;
		MeMatrix4 comRelTM;
        KU2MEVecCopy(relPos, con->KPos1);
		MdtBodyGetCenterOfMassRelativeTransform(b1, comRelTM);
		MeVector3Subtract(cw->pos1, relPos, comRelTM[3]);

        KU2MEVecCopy(cw->haxis1, con->KPriAxis1);
        KU2MEVecCopy(cw->haxis2, con->KPriAxis2);
        KU2MEVecCopy(cw->saxis1, con->KSecAxis2);
    }
#endif
	
	/* Set pointers between objects */
	con->KConstraintData = (PTRINT)mdtCon;
	MdtConstraintSetUserData(mdtCon, (void*)con);
	
	/* Sync parameters between Unreal structs and Karma. */
    con->KUpdateConstraintParams();

	/* Enable if desired. */
    MdtConstraintEnable(con->getKConstraint());
	
	/* Disable collision between joined models. */
	if(con->bKDisableCollision)
	{
		if(!con->KConstraintActor2)
			m2 = con->GetLevel()->KLevelModel;

		// If we have two different models, disable collision.
		if(	m1 && m2 && m1 != m2 )
			KDisablePairCollision(m1, m2, level);
	}
	
	// Update 'JoinedTag'.
	// Doesn't/Shouldn't matter which actor we call UpdateJoined on.
	guard(UpdateJoined);
	KUpdateJoined(con->KConstraintActor1, 1);
	unguard;

	unguard;
    
    KSetSecName(TEXT("KARMA: POST INIT CONSTRAINT"));

    unguard;
}

/*  If a constraint has physics of PHYS_Karma, it denotes the constraint is active.
    When physics becomes anything else, we should destroy the constraint using this function. */
void MEAPI KTermConstraintKarma(AKConstraint* con)
{
    guard(KTermConstraintKarma);

    if(!KGData) 
        return;

    MdtConstraintID mdtCon = con->getKConstraint();

    if(!mdtCon)
        return;

	// Make sure Physics is no longer PHYS_Karma
	if(con->Physics == PHYS_Karma)
		con->Physics = PHYS_None;
	else
		debugf(TEXT("(Karma:) KConstraint with Physics != Karma but an MdtConstraint."));

	AActor* a1 = con->KConstraintActor1;	

    MdtConstraintDisable(mdtCon);
    MdtConstraintDestroy(mdtCon);

    con->KConstraintData = NULL;

	// Update 'Joined' tags now we've removed this constraint.
	guard(UpdateJoined);
	if(a1 && !a1->bDeleteMe)
		KUpdateJoined(a1, 1);
	unguard;

    unguard;
}

/*** C++ CONSTRAINT ***/

///////////////// /////////////// //////////////////////////

// Updates steering and applied driving torque.
void AKCarWheelJoint::preKarmaStep(FLOAT DeltaTime)
{
    guard(AKCarWheelJoint::preKarmaStep);

	MdtConstraintID mdtC = this->getKConstraint();
    if(!mdtC)
        return;

    MdtCarWheelID mdtCW = MdtConstraintDCastCarWheel(mdtC);
    if(!mdtCW)
        return;

    MdtBodyID cBody = MdtCarWheelGetBody(mdtCW, 0);
    MdtBodyID tBody = MdtCarWheelGetBody(mdtCW, 1); /* Get the 'tyre' body */

    MeVector3 haxis;
    MdtCarWheelGetHingeAxis(mdtCW, haxis); // axis wheel is spinning about, in world ref frame.

	// If neither body is enabled, dont bother doing anything.
	if(!MdtBodyIsEnabled(cBody) && !MdtBodyIsEnabled(tBody))
		return;

	// So at least one body is enabled. Make sure they both are (prevents pedantic warnings).
	MdtBodyEnable(cBody);
	MdtBodyEnable(tBody);

    // Braking takes priority
    if(this->KBraking > 0.01)
    {
        MdtCarWheelSetHingeLimitedForceMotor(mdtCW, 0, KBraking);
    }
	else
	{
		// If not braking...
		// If there is a drive torque (+/-), apply it.
#if 0
		MdtCarWheelSetHingeLimitedForceMotor(mdtCW, 0, 0); // turn off motor
		MdtBodyAddTorque(tBody, KMotorTorque*haxis[0], KMotorTorque*haxis[1], KMotorTorque*haxis[2]);
		MdtBodyAddTorque(cBody, -KMotorTorque*haxis[0], -KMotorTorque*haxis[1], -KMotorTorque*haxis[2]);
#else
		if(KMotorTorque > 0.01)
			MdtCarWheelSetHingeLimitedForceMotor(mdtCW, K_U2Rad * KMaxSpeed, KMotorTorque);
		else if(KMotorTorque < -0.01)
			MdtCarWheelSetHingeLimitedForceMotor(mdtCW, -K_U2Rad * KMaxSpeed, -KMotorTorque);
		else
			MdtCarWheelSetHingeLimitedForceMotor(mdtCW, 0, 0); // turn off motor
#endif
	}

    // Update steering controller
    if(!bKSteeringLocked)
    {
		MeReal radPGap = KProportionalGap * K_U2Rad;
        MeReal radError = K_U2Rad * KSteerAngle - MdtCarWheelGetSteeringAngle(mdtCW);
        
        if(radError < -radPGap)
            MdtCarWheelSetSteeringLimitedForceMotor(mdtCW, K_U2Rad * KMaxSteerSpeed, KMaxSteerTorque);
        else if(radError > radPGap)
            MdtCarWheelSetSteeringLimitedForceMotor(mdtCW, -K_U2Rad * KMaxSteerSpeed, KMaxSteerTorque);
        else // we are in the proportional region
        {
            MeReal vel = (K_U2Rad * KMaxSteerSpeed / radPGap) * -radError;
            MdtCarWheelSetSteeringLimitedForceMotor(mdtCW, vel, KMaxSteerTorque);
        }
    }

    // Set 'fast spin' axis on body to be the hinge axis on the joint

    MdtBodySetFastSpinAxis(tBody, haxis[0], haxis[1], haxis[2]);

    // Get current position of wheel relative to suspension centre
    MeVector3 jPos, wPos, relPos, saxisDir;
    MdtCarWheelGetPosition(mdtCW, jPos);
    MdtBodyGetPosition(tBody, wPos);
    MdtConvertVector(cBody, mdtCW->head.ref1[0], 0, saxisDir);
    MeVector3Subtract(relPos, wPos, jPos);
	MeReal meWheelHeight = MeVector3Dot(relPos, saxisDir);
    KWheelHeight = K_ME2UScale * meWheelHeight;
	
#if 0
	// If wheel has moved beyond suspension range - reset 
	MeReal newHeight;
	if(meWheelHeight < 0.6 * this->KSuspLowLimit)
		newHeight = 0.6 * this->KSuspLowLimit;
	else if(meWheelHeight > 0.6 * this->KSuspHighLimit)
		newHeight = 0.6 * this->KSuspHighLimit;
	else
		return;

	debugf(TEXT("Adjust Wheel: %"), meWheelHeight);

	// So we needed to adjust wheel position
	MeVector3 newWheelPos;
	MeVector3Copy(newWheelPos, saxisDir);
	MeVector3Scale(newWheelPos, newHeight);
	MeVector3Add(newWheelPos, jPos, newWheelPos);

	MdtBodySetPosition(tBody, newWheelPos[0], newWheelPos[1], newWheelPos[2]);

	// kill any velocity in suspension direction
	MeVector3 newWheelVel, suspComp;
	MdtBodyGetLinearVelocity(tBody, newWheelVel);
	MeVector3Copy(suspComp, newWheelVel);
	MeReal suspCompMag = MeVector3Dot(newWheelVel, saxisDir);
	MeVector3Copy(suspComp, saxisDir);
	MeVector3Scale(suspComp, suspCompMag);
	MeVector3Subtract(newWheelVel, newWheelVel, suspComp);
#endif

    unguard;
}

// Updates controller (if present), and updates position.
void AKHinge::preKarmaStep(FLOAT DeltaTime)
{
	guard(AKHinge::preKarmaStep);

	MdtConstraintID mdtC = getKConstraint();
    if(!mdtC)
        return;

    MdtHingeID mdtH = MdtConstraintDCastHinge(mdtC);
    if(!mdtH)
        return;

	MdtLimitID lim = MdtHingeGetLimit(mdtH);
	MeReal des = (KUseAltDesired == 1) ? KAltDesiredAngle :KDesiredAngle;

	if(KHingeType == HT_Controlled)
	{
		MdtLimitController(lim, 
			K_U2Rad * des, 
			K_U2Rad * KProportionalGap, 
			K_U2Rad * KDesiredAngVel, 
			KMaxTorque);
	}

	KCurrentAngle = K_Rad2U * MdtLimitGetPosition(lim);

	unguard;
}

UBOOL AKConstraint::CheckOwnerUpdated()
{
	guardSlow(AKConstraint::CheckOwnerUpdated);
	if( Owner && (INT)Owner->bTicked!=GetLevel()->Ticked )
	{
		GetLevel()->NewlySpawned = new(GEngineMem)FActorLink(this,GetLevel()->NewlySpawned);
		return 0;
	}

	// Make sure both any KConstraintActors are ticked before the constraint between them.
	if( KConstraintActor1 && (INT)KConstraintActor1->bTicked!=GetLevel()->Ticked )
	{
		GetLevel()->NewlySpawned = new(GEngineMem)FActorLink(this,GetLevel()->NewlySpawned);
		return 0;
	}

	if( KConstraintActor2 && (INT)KConstraintActor2->bTicked!=GetLevel()->Ticked )
	{
		GetLevel()->NewlySpawned = new(GEngineMem)FActorLink(this,GetLevel()->NewlySpawned);
		return 0;
	}

	return 1;
	unguardSlow;
}

///////////////// /////////////// //////////////////////////


// Main constraint 'Tick' function. This syncs any graphics etc. to the constraint itself.
// Note - most stuff like controller is done in prePhysKarma - applied at each simulation step.
void AKConstraint::physKarma(FLOAT deltaTime)
{
	clock(GStats.DWORDStats(GEngineStats.STATS_Karma_physKarma));

	MdtConstraintID mdtCon = getKConstraint();
	if(mdtCon)
	{
		// Move actor to constraint position (no rotation)
		FVector newPos, moveBy;
		MeVector3 cPos;
	    FRotator rot;
		FCheckResult Hit(1.0f);

		MdtConstraintGetPosition(mdtCon, cPos);
		KME2UPosition(&newPos, cPos);
		moveBy = newPos - this->Location; 
		rot = FRotator(0, 0, 0); // KTODO: Should we calculate some orientation for the constraint? Are people going to add graphics to them?
		GetLevel()->MoveActor(this, moveBy, rot, Hit);

		if(IsProbing(ENGINE_KForceExceed))
		{
			MeVector3 cForce;
			MdtConstraintGetForce(mdtCon, 0, cForce);
			MeReal cForceMag = MeVector3Magnitude(cForce);

			if(cForceMag > KForceThreshold)
				eventKForceExceed(cForceMag);
		}

		// For debugging
		this->bHidden = !(KGData->DebugDrawOpt & KDRAW_Axis);
	}

	unclock(GStats.DWORDStats(GEngineStats.STATS_Karma_physKarma));
}

/* KConstraint (and below) use KarmaData for MdtConstriantID instead of McdModel */
McdModelID AKConstraint::getKModel() const
{
    return NULL;
}

MdtConstraintID AKConstraint::getKConstraint() const
{
    return ((MdtConstraintID)this->KConstraintData);
}

void AKConstraint::KUpdateConstraintParams() {}; // nothing to do for base class

void AKConstraint::PostEditChange()
{
	guard(AKConstraint::PostEditChange);

	if(GIsEditor)
		this->PostEditMove();

    this->KUpdateConstraintParams();

	unguard;
}

// When we move a constraint - we need to update the position/axis held in
// local space (ie. relative to each connected actor)
void AKConstraint::PostEditMove()
{
	guard(AKConstraint::PostEditMove);

	MeMatrix4 a1TM, a2TM;

	// Get actor(s) connected by this constraint.
	AActor *a1 = this->KConstraintActor1;
	AActor *a2 = this->KConstraintActor2;

	if(!a1)
	{
		if(!a2) // if no actor is set - do nothing
			return;
		else // if one is empty- it has to be actor2, so swap
		{
			AActor* tmp = a1; a1 = a2; a2 = tmp;
		}
	}

	// Get each actors transform
	KU2METransform(a1TM, a1->Location, a1->Rotation);

	if(a2)
		KU2METransform(a2TM, a2->Location, a2->Rotation);
	else
		MeMatrix4TMMakeIdentity(a2TM);

	// Calculate position/axis from constraint actor.
	FCoords coords = FCoords(FVector(0, 0, 0));
	coords *= this->Rotation;
	
	// World ref frame
	MeVector3 wPos, wPri, wOrth;
	KU2MEPosition(wPos, this->Location);
	
	wPri[0] = coords.XAxis.X;
	wPri[1] = coords.YAxis.X;
	wPri[2] = coords.ZAxis.X;
	
	wOrth[0] = coords.XAxis.Y;
	wOrth[1] = coords.YAxis.Y;
	wOrth[2] = coords.ZAxis.Y;
	
	MeVector3 lPos, lPri, lOrth;
	
	MeMatrix4TMInverseTransform(lPos, a1TM, wPos);
	MeMatrix4TMInverseRotate(lPri, a1TM, wPri);
	MeMatrix4TMInverseRotate(lOrth, a1TM, wOrth);

	KME2UVecCopy(&this->KPos1, lPos);
	KME2UVecCopy(&this->KPriAxis1, lPri);
	KME2UVecCopy(&this->KSecAxis1, lOrth);
	
	MeMatrix4TMInverseTransform(lPos, a2TM, wPos);
	MeMatrix4TMInverseRotate(lPri, a2TM, wPri);
	MeMatrix4TMInverseRotate(lOrth, a2TM, wOrth);
	
	KME2UVecCopy(&this->KPos2, lPos);
	KME2UVecCopy(&this->KPriAxis2, lPri);
	KME2UVecCopy(&this->KSecAxis2, lOrth);

	unguard;
}

void AKConstraint::CheckForErrors()
{
	guard(AKConstraint::CheckForErrors);

	Super::CheckForErrors();

	if(!this->KConstraintActor1 && !this->KConstraintActor2)
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("KConstraint which does not point to any Actors."));
	}

	if((this->KConstraintActor1 && !this->KConstraintActor1->KParams) || 
		(this->KConstraintActor2 && !this->KConstraintActor2->KParams))
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("KConstraint references Actor with no KParams."));
	}

	unguard;
}

#endif // WITH_KARMA

/*** UC CONSTRAINT ***/
void AKConstraint::execKGetConstraintForce( FFrame& Stack, RESULT_DECL )
{
    guard(AKConstraint::execKGetConstraintForce);

    P_GET_VECTOR_REF(Force);
    P_FINISH;
#ifdef WITH_KARMA
    MdtConstraintID constraint = this->getKConstraint();
    if(!constraint)
        return;

    /* Scale by mass _and_ distance */
    MeVector3 mforce;
    MdtConstraintGetForce(constraint, 0, mforce);
    KME2UPosition(Force, mforce);
#else
	Force->X = Force->Y = Force->Z = 0;
#endif
    unguard;
}

void AKConstraint::execKGetConstraintTorque( FFrame& Stack, RESULT_DECL )
{
    guard(AKConstraint::execKGetConstraintTorque);

    P_GET_VECTOR_REF(Torque);
    P_FINISH;
#ifdef WITH_KARMA
    MdtConstraintID constraint = this->getKConstraint();
    if(!constraint)
        return;

    MeVector3 mtorque;
    MdtConstraintGetTorque(constraint, 0, mtorque);
    KME2UPosition(Torque, mtorque);
#else
	Torque->X = Torque->Y = Torque->Z = 0;
#endif
    unguard
}


/* Sync any parameters from the Unreal constraint structure into the Karma structs. */
void AKConstraint::execKUpdateConstraintParams( FFrame& Stack, RESULT_DECL )
{
    guard(AKConstraint::execKGetConstraintTorque);

    P_FINISH;
#ifdef WITH_KARMA
    this->KUpdateConstraintParams();
#endif
	unguard;
}

//////////////////////////////////////////////////////////
////// CONSTRAINT SPECIFIC KUPDATECONSTRAINTPARAMS ///////
//////////////////////////////////////////////////////////

#ifdef WITH_KARMA
/*** CONE LIMIT ***/
void AKConeLimit::KUpdateConstraintParams()
{
	guard(AKConeLimit::execKUpdateParams);

	if(this->bDeleteMe)
		return;

    if(!this->KConstraintData)
        return;

	MdtConeLimitID cl = (MdtConstraintDCastConeLimit(this->getKConstraint()));
	if(!cl)
		return;

    MdtConeLimitSetConeHalfAngle(cl, K_U2Rad * this->KHalfAngle);
    MdtConeLimitSetStiffness(cl, this->KStiffness);
    MdtConeLimitSetDamping(cl, this->KDamping);

	unguard;
}

/*** HINGE ***/
void AKHinge::KUpdateConstraintParams()
{
	guard(AKConeLimit::execKUpdateParams);

    if(!this->KConstraintData)
        return;

	MdtHingeID h = (MdtConstraintDCastHinge(this->getKConstraint()));
	if(!h)
		return;

	MdtLimitID lim = MdtHingeGetLimit(h);
	
	MeReal des = (this->KUseAltDesired == 1) ? this->KAltDesiredAngle : this->KDesiredAngle;

	MdtSingleLimitSetStop(MdtLimitGetLowerLimit(lim), K_U2Rad * des);
	MdtSingleLimitSetStiffness(MdtLimitGetLowerLimit(lim), this->KStiffness);
	MdtSingleLimitSetDamping(MdtLimitGetLowerLimit(lim), this->KDamping);
	
	MdtSingleLimitSetStop(MdtLimitGetUpperLimit(lim), K_U2Rad * des);
	MdtSingleLimitSetStiffness(MdtLimitGetUpperLimit(lim), this->KStiffness);
	MdtSingleLimitSetDamping(MdtLimitGetUpperLimit(lim), this->KDamping);

    if(this->KHingeType == HT_Normal)
    {
        MdtLimitActivateLimits(lim, 0);
    }
    else if(this->KHingeType == HT_Springy)
    {
		MdtLimitActivateLimits(lim, 1);
    }
    else if(this->KHingeType == HT_Motor)
    {
        MdtLimitActivateLimits(lim, 0);
        MdtLimitSetLimitedForceMotor(lim, K_U2Rad * KDesiredAngVel, KMaxTorque);
    }
    else if(this->KHingeType == HT_Controlled)
    {
        MdtLimitActivateLimits(lim, 0);

		/* See the AActor::physKarma (in KPhysic.cpp) for where controller is updated each frame. */
    }

	unguard;
}

/*** CAR WHEEL ***/
void AKCarWheelJoint::KUpdateConstraintParams()
{
	guard(AKConeLimit::execKUpdateParams);

	if(!this->KConstraintData)
        return;

    MdtCarWheelID cw = (MdtConstraintDCastCarWheel(this->getKConstraint()));
    if(!cw)
        return;

    /* Update steering controller */
    if(this->bKSteeringLocked)
        MdtCarWheelSetSteeringLock(cw, 1);
    else
        MdtCarWheelSetSteeringLock(cw, 0);
    
    /* Set suspension parameters from script structures. */
    MdtCarWheelSetSuspension(cw, this->KSuspStiffness, this->KSuspDamping, 0, 
        this->KSuspLowLimit, this->KSuspHighLimit, this->KSuspRef);

	unguard;
}

#endif

