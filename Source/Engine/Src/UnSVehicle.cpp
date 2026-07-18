#include "EnginePrivate.h"

IMPLEMENT_CLASS(ASVehicle);
IMPLEMENT_CLASS(USVehicleWheel);

void ASVehicle::execUpdateVehicle( FFrame& Stack, RESULT_DECL )
{
	guard(ASVehicle::execUpdateVehicle);
	
	P_GET_FLOAT(DeltaTime);
	P_FINISH;
#ifdef WITH_KARMA
	UpdateVehicle(DeltaTime);
#endif
	unguard;
}

// Handy function for plotting script variables onto screen graph.
void ASVehicle::execGraphData( FFrame& Stack, RESULT_DECL )
{
	guard(ASVehicle::execGraphData);

	P_GET_STR(DataName);
	P_GET_FLOAT(DataValue);
    P_FINISH;

	// Make graph line name by concatenating vehicle name with data name.
	FString lineName = FString::Printf(TEXT("%s_%s"), 
		this->GetName(), (TCHAR*)DataName.GetCharArray().GetData());

	GStatGraph->AddDataPoint(lineName, DataValue, 1);

	unguard;
}

#ifdef WITH_KARMA

// Remove any contacts due the line wheels
void KTermSVehicleDynamics(ASVehicle* v)
{
	guard(KTermSVehicleDynamics);

	// Destroy any contacts created using bLineWheel
	for(INT i=0; i<v->Wheels.Num(); i++)
	{
		USVehicleWheel* vw = v->Wheels(i);

		if(vw->KContact)
		{
			MdtContactGroupID cg = (MdtContactGroupID)vw->KContact;

			if( MdtContactGroupIsEnabled(cg) )
				MdtContactGroupDisable(cg);

			MdtContactGroupDestroy(cg);
			vw->KContact = NULL;
		}
	}

	unguard;
}

void ASVehicle::UpdateVehicle(FLOAT DeltaTime)
{
	guard(ASVehicle::UpdateVehicle);
	unguard;
}

// Tyre model
// Apply wheel/drive forces to car
void ASVehicle::preKarmaStep(FLOAT DeltaTime)
{
	guard(ASVehicle::preKarmaStep);

	Super::preKarmaStep(DeltaTime);

	// Do script vehicle model, updating wheel grips, speeds etc.
	eventUpdateVehicle(DeltaTime);

	// If this isn't a wheeled vehicle - do nothing
	if( Wheels.Num() == 0 )
		return;

	// Do wheeled vehicle stuff model.
	McdModelID model = getKModel();
	check(model);
	
	MdtBodyID body = McdModelGetBody(model);
	check(body);

	FMatrix l2w = LocalToWorld();

	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);

		// Find wheel center in world space.
		FVector WheelCenter = l2w.TransformFVector(vw->WheelPosition);

		// Apply drive force to wheel center.
		MeVector3 driveForce, meWheelCenter;
		driveForce[0] = vw->DriveForce * vw->WheelDir.X;
		driveForce[1] = vw->DriveForce * vw->WheelDir.Y;
		driveForce[2] = vw->DriveForce * vw->WheelDir.Z;

		KU2MEPosition(meWheelCenter, WheelCenter);

		// Add force in wheel direction at location of wheel geometry
		MdtBodyAddForceAtPosition(body, 
			driveForce[0], driveForce[1], driveForce[2], 
			meWheelCenter[0], meWheelCenter[1], meWheelCenter[2]);

		// Add chassis torque from wheel
		MdtBodyAddTorque(body, 
			vw->WheelAxle.X * vw->ChassisTorque,
			vw->WheelAxle.Y * vw->ChassisTorque,
			vw->WheelAxle.Z * vw->ChassisTorque);
	}

	// Finally

	unguard;
}

#define GROUND_VEL_THRESH	(0.001f)

// Use this to reset wheels - for if there are no contacts with ground etc.
void ASVehicle::preContactUpdate()
{
	guard(ASVehicle::preContactUpdate);

	Super::preContactUpdate();

	if(Physics != PHYS_Karma)
		return;

	// Get 'down' (-z) and right (+y) vector for vehicle.
	FMatrix l2w = LocalToWorld();

	FVector downVec(-l2w.M[2][0], -l2w.M[2][1], -l2w.M[2][2]);
	downVec.Normalize();

	FVector rightVec(l2w.M[1][0], l2w.M[1][1], l2w.M[1][2]);
	rightVec.Normalize();


	McdModelID model = getKModel();
	check(model);
	MeMatrix4Ptr modelTM = McdModelGetTransformPtr(model);
	McdAggregateID aggGeom = McdModelGetGeometry(model);
	check(aggGeom);
	MdtBodyID body = McdModelGetBody(model);
	check(body);

	bVehicleOnGround = false;

	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);

		vw->bWheelOnGround = false;
		vw->SuspensionPosition = 0.0f;
		vw->WheelDir = FVector(0.0f, 0.0f, 0.0f);
		vw->SlipVel = 0.0f;
		vw->TireLoad = 0.0f;
		vw->SlipVel = 0.0f;
		vw->SlipAngle = 0.0f;
		//vw->SpinVel = 0.0f;

		// Find wheel center in world space.
		FVector WheelCenter = l2w.TransformFVector(vw->WheelPosition);
		WheelCenter -= vw->SuspensionOffset * downVec;

		FVector CheckStart = WheelCenter - (downVec * vw->WheelRadius);
		FVector CheckEnd = WheelCenter + (downVec * vw->WheelRadius);
		FVector FudgeEnd = CheckEnd + (downVec * vw->WheelRadius); // Because of evil pull-back linecheck code, we over-extend the line by 50%, and just throw away results over 2/3 way along.

		// Do a zero-extent trace
		FCheckResult Hit(1.f);
		UBOOL bHitSomething = !GetLevel()->SingleLineCheck( Hit, this, FudgeEnd, CheckStart, TRACE_World | TRACE_Pawns);

		if(bHitSomething && Hit.Time > 0.6666f)
			bHitSomething = false;

		FVector WorldVel(0,0,0);
		if( bHitSomething && Hit.Actor && Hit.Actor->IsA(AMover::StaticClass()) )
			WorldVel = Hit.Actor->Velocity * 1.1f;
		
		MeVector3 MeWorldVel;
		KU2MEPosition(MeWorldVel, WorldVel);

		// DEBUG DRAWING
		if( KGData && (KGData->DebugDrawOpt & KDRAW_WheelPhys) )
		{
			FVector mid;
			mid = CheckStart + ( Hit.Time * 3.f * downVec * vw->WheelRadius );

			GTempLineBatcher->AddLine(CheckStart, mid, FColor(0, 0, 255));
			GTempLineBatcher->AddLine(mid, FudgeEnd, FColor(0, 255, 255));

			// Draw suspension high stop
			FVector suspLimit = CheckEnd - (vw->SuspensionTravel * downVec);
			GTempLineBatcher->AddLine(suspLimit, suspLimit + 10 * rightVec, FColor(255,0,255));
		}

		MdtContactGroupID cg = (MdtContactGroupID)vw->KContact;

		// Hit something...
		if(bHitSomething)
		{
			// Create a contact group and configure first (if necessary)
			if(!cg)
			{
				cg = MdtContactGroupCreate( GetLevel()->KWorld );
				MdtContactGroupSetBodies(cg, body, 0);
				MdtContactGroupEnable(cg);

				vw->KContact = (PTRINT)cg;
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
			KU2MEPosition(pos, CheckEnd); 
			MdtContactSetPosition(contact, pos[0], pos[1], pos[2]);

#if 1
			// Normal points back along line.
			MeVector3 cNormal = {-1.f * downVec.X, -1.f * downVec.Y, -1.f * downVec.Z};
#else
			// Normal is surface normal at point of contact.
			MeVector3 cNormal = {Hit.Normal.X, Hit.Normal.Y, Hit.Normal.Z};
#endif

			MdtContactSetNormal(contact, cNormal[0], cNormal[1], cNormal[2]);

			MdtContactSetWorldVelocity(contact, MeWorldVel[0], MeWorldVel[1], MeWorldVel[2]);

			///// PENETRATION /////

			// Distance to move suspension (ie basic distance to line check hit).
			vw->SuspensionPosition = (0.6666f - Hit.Time) * 3.f * vw->WheelRadius;

			// Contact penetration - scale to Karma.
			MeReal pen = (vw->SuspensionPosition * K_U2MEScale) - vw->PenOffset;

#if 0 // PENETRATION SCALE SUSPENSION
			// Scale depending on suspension travel ie. make it softer within travel range
			MeReal theta = (pen/(K_U2MEScale*vw->SuspensionTravel))*ME_PI - 0.5f*ME_PI;
			static MeReal sFac = 0.0f;
			MeReal pFac = sFac + (1-sFac)*0.5f*(MeSin(theta)+1.0f);
			pen *= pFac;

			MeReal ContactSoftness = vw->Softness;
#else // SOFTNESS SUSPENSION
			MeReal ContactSoftness = vw->Softness;

			// If the contact is beyond its 'soft' travel, make it 'hard'.
			if(pen > K_U2MEScale * vw->SuspensionTravel)
				ContactSoftness = 0.001f;
#endif

			// Final overall penetraion scaling.
			pen *= vw->PenScale; 

			MdtContactSetPenetration(contact, pen);

			///// DIRECTION /////
			MeVector3 haxis, latDir, rollDir;
			MeVector4 steerQuat;

			// find 'forward' direction direction
			MeQuaternionMake(steerQuat, modelTM[2], (vw->Steer/180.0f) * PI);
			MeQuaternionRotateVector3(haxis, steerQuat, modelTM[1]);

			// Store wheel vector for applying torque's around
			KME2UVecCopy(&vw->WheelAxle, haxis);

			// If wheel hinge and contact normal are parallel.
			MeReal dot = MeVector3Dot(haxis, cNormal);
			if( Abs(dot) > 0.999f ) 
			{
				MeVector3PlaneSpace(cNormal, rollDir, latDir);
			}
			else
			{
				// Cross contact normal and hinge axis to get roll direction.
				MeVector3Cross(rollDir, haxis, cNormal);
				MeVector3Normalize(rollDir);

				// Project hinge axis into plane of contact and normalize to get lateral direction.
				MeVector3Copy(latDir, haxis);
				MeVector3MultiplyAdd(latDir, -dot, cNormal);
				MeVector3Normalize(latDir);
			}

			if( KGData && (KGData->DebugDrawOpt & KDRAW_WheelPhys) )
			{
				// Draw little 'ref frame' at contact.
				FVector cp, d;
				FLOAT drawLen = 10.0f;
				KME2UPosition(&cp, pos);

				//KME2UVecCopy(&d, cNormal);
				//GTempLineBatcher->AddLine(cp, cp + drawLen * d, FColor(255, 0, 0));

				KME2UVecCopy(&d, rollDir);
				GTempLineBatcher->AddLine(cp, cp + drawLen * d, FColor(0, 255, 0));

				KME2UVecCopy(&d, latDir);
				GTempLineBatcher->AddLine(cp, cp + drawLen * d, FColor(0, 0, 255));
			}

			MdtContactSetDirection(contact, latDir[0], latDir[1], latDir[2]);

			// Store wheel 'direction' (for adding drive forces later)
			KME2UVecCopy(&vw->WheelDir, rollDir);

			///// PARAMS /////

			// Amount of force applied by contact in normal direction (from the last frame..)
			MeVector3 outForce;
			MdtContactGroupGetForce(cg, 0, outForce);
			vw->TireLoad = MeMAX( MeVector3Dot(outForce, cNormal), 0.0f); // ensure always positive!

			// Friction

			if(vw->LongFriction < 0.01f)
				MdtContactParamsSetType(params, MdtContactTypeFriction1D);
			else
				MdtContactParamsSetType(params, MdtContactTypeFriction2D);

			MdtContactParamsSetFrictionModel(params, MdtFrictionModelBox);

			MdtContactParamsSetPrimaryFriction(params, vw->LatFriction);
			MdtContactParamsSetSecondaryFriction(params, vw->LongFriction);

			// Slip
			MdtContactParamsSetPrimarySlip(params, vw->LatSlip);
			MdtContactParamsSetSecondarySlip(params, vw->LongSlip);

			// Softness (suspension)
			MdtContactParamsSetSoftness(params, ContactSoftness);

			// Others
			MdtContactParamsSetRestitution(params, vw->Restitution);
			MdtContactParamsSetMaxAdhesiveForce(params, vw->Adhesion);

			///// OUTPUT /////

			MeVector3 relVel, planeRelVel, relVelDir;
			MdtBodyGetVelocityAtPoint(body, pos, relVel);
			MeVector3Subtract(relVel, relVel, MeWorldVel);

			// Subtract component of rel velocity in direction of normal
			MeReal normVelMag = MeVector3Dot(relVel, cNormal);

			MeVector3Copy(planeRelVel, relVel);
			MeVector3MultiplyAdd(planeRelVel, -normVelMag, cNormal);

			MeReal WheelLongVel = vw->WheelRadius * vw->SpinVel;
			MeReal GroundLongVel = -K_ME2UScale * MeVector3Dot(rollDir, relVel);

			// Find relative velocity at contact between 'wheel' and ground
			vw->SlipVel = WheelLongVel + GroundLongVel;

			// Find angle between wheel direction and sliding direction (slip angle)
			// If wheel is not moving, assume slip angle of zero
			if( MeVector3MagnitudeSqr(planeRelVel) < 0.05f * 0.05f )
			{
				vw->SlipAngle = 0.0f;
			}
			else
			{
				MeVector3Copy(relVelDir, planeRelVel);
				MeVector3Normalize(relVelDir);

				FLOAT dirDot = MeVector3Dot(rollDir, relVelDir);
				dirDot = MeCLAMP(dirDot, 0.0f, 1.0f); // Only create slip angles up to 90 degrees

				vw->SlipAngle = ( 180.0f/PI ) * MeAcos( dirDot ); // Note this doesn't give a sign
			}

			if(i==0)
			{
				//GStatGraph->AddDataPoint( FString(TEXT("LongSlipVel")), vw->SlipVel, true );
				//GStatGraph->AddDataPoint( FString(TEXT("SlipAngle")), vw->SlipAngle, true );
				//GStatGraph->AddDataPoint( FString(TEXT("NormalForce")), vw->TireLoad, true );

				//debugf(TEXT("%f"), vw->SlipAngle);

				//GStatGraph->AddDataPoint( FString(TEXT("WheelLongVel")), WheelLongVel, true );
				//GStatGraph->AddDataPoint( FString(TEXT("GroundLongVel")), GroundLongVel, true );

				//if(vw->SlipRatio > 100.0f && GroundLongVel < 1.0f)
				//	int a=0;
			}

			vw->bWheelOnGround = true;
			bVehicleOnGround = true;

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

		// Add basic suspension offset regardless of whether wheel is on the gorund
		vw->SuspensionPosition += (vw->SuspensionOffset + (vw->PenOffset * K_ME2UScale));

	} // FOR EACH WHEEL

	unguard;
}

static FMatrix RefMeshToWorld(USkeletalMesh* smesh)
{
	FMatrix NewMatrix = FRotationMatrix( smesh->RotOrigin );

	FVector XAxis( NewMatrix.M[0][0], NewMatrix.M[1][0], NewMatrix.M[2][0] );
	FVector YAxis( NewMatrix.M[0][1], NewMatrix.M[1][1], NewMatrix.M[2][1] );
	FVector ZAxis( NewMatrix.M[0][2], NewMatrix.M[1][2], NewMatrix.M[2][2] );

	NewMatrix.M[3][0] += - smesh->Origin | XAxis;
	NewMatrix.M[3][1] += - smesh->Origin | YAxis;
	NewMatrix.M[3][2] += - smesh->Origin | ZAxis;

	return NewMatrix;
}


void ASVehicle::PostBeginPlay()
{
	guard(ASVehicle::PostBeginPlay);

	Super::PostBeginPlay();

	// Set each wheel radius into USVehicleWheels before we start using them.
	// Should NOT call things like PreContactUpdate before we do this!.

	// SVehicles should have a skeletal mesh!
	USkeletalMesh* smesh = Cast<USkeletalMesh>(Mesh);
	if(!smesh)
	{
		debugf( TEXT("ASVehicle::PostBeginPlay : No Skeletal Mesh! (%s)"), this->GetName() );
		return;
	}

	// Warn if trying to do something silly like non-uniform scale a vehicle.
	FVector TotalScale = DrawScale * DrawScale3D * smesh->Scale;
	if( !TotalScale.IsUniform() )
		debugf( TEXT("ASVehicle::PostBeginPlay : Can only uniformly scale SVehicles. (%s)"), this->GetName() );

	// Calculate mesh-space bone transforms in the default pose.
	TArray<FCoords> RefBases;
	RefBases.Add( smesh->RefSkeleton.Num() );

	for( INT b=0; b<smesh->RefSkeleton.Num(); b++)
	{
		// Render the default pose.
		FQuatToFCoordsFast( smesh->RefSkeleton(b).BonePos.Orientation, smesh->RefSkeleton(b).BonePos.Position, RefBases(b));

		// Construct mesh-space skeletal hierarchy.
		if( b>0 )
		{
			INT Parent = smesh->RefSkeleton(b).ParentIndex;
			RefBases(b) = RefBases(b).ApplyPivot(RefBases(Parent));
		} 
	}

	// Make matrix from smesh Origin and RotOrigin
	FMatrix MeshToWorldMatrix = RefMeshToWorld(smesh);
	FVector Up(0.0f, 0.0f, 1.0f);

	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);

		INT BoneIndex = smesh->MatchRefBone(vw->BoneName);
		check(BoneIndex != INDEX_NONE); // JTODO: Make these friendly warnings instead!

		FMatrix BoneMatrix = RefBases(BoneIndex).Matrix() * MeshToWorldMatrix; // Bone -> Actor TM
		FMatrix BoneMatrix2 = RefBases(BoneIndex).Matrix().Inverse() * MeshToWorldMatrix;

		BoneMatrix.M[3][0] = BoneMatrix2.M[3][0];
		BoneMatrix.M[3][1] = BoneMatrix2.M[3][1];
		BoneMatrix.M[3][2] = BoneMatrix2.M[3][2];

		vw->WheelPosition = smesh->Scale * BoneMatrix.TransformFVector(vw->BoneOffset);

		// If we have a support bone specified, figure out how far the wheel is from the bone origin (joint)
		if(vw->SupportBoneName != NAME_None)
		{
			INT SuppBoneIndex = smesh->MatchRefBone(vw->SupportBoneName);
			check(SuppBoneIndex != INDEX_NONE);

			FMatrix SuppBoneMatrix = RefBases(SuppBoneIndex).Matrix() * MeshToWorldMatrix; // Bone -> Actor TM
			FMatrix SuppBoneMatrix2 = RefBases(SuppBoneIndex).Matrix().Inverse() * MeshToWorldMatrix;

			SuppBoneMatrix.M[3][0] = SuppBoneMatrix2.M[3][0];
			SuppBoneMatrix.M[3][1] = SuppBoneMatrix2.M[3][1];
			SuppBoneMatrix.M[3][2] = SuppBoneMatrix2.M[3][2];

			FVector PivotPosition = smesh->Scale * FVector(SuppBoneMatrix.M[3][0], SuppBoneMatrix.M[3][1], SuppBoneMatrix.M[3][2]);

			// Use enum to find axis about which bone will pivot.
			FVector PivotAxis;
			if(vw->SupportBoneAxis == AXIS_X)
				PivotAxis = FVector(SuppBoneMatrix.M[0][0], SuppBoneMatrix.M[0][1], SuppBoneMatrix.M[0][2]);
			else if(vw->SupportBoneAxis == AXIS_Y)
				PivotAxis = FVector(SuppBoneMatrix.M[1][0], SuppBoneMatrix.M[1][1], SuppBoneMatrix.M[1][2]);
			else
				PivotAxis = FVector(SuppBoneMatrix.M[2][0], SuppBoneMatrix.M[2][1], SuppBoneMatrix.M[2][2]);
			
			// Vector from Wheel position to pivot joint position.
			FVector WheelFromPivot = vw->WheelPosition - PivotPosition;
			
			FVector DeltaCrossUp = WheelFromPivot ^ Up; // Axis of rotation as wheel goes up and down.

			// Then find magnitude about pivot axis.
			vw->SupportPivotDistance = DeltaCrossUp | PivotAxis; 
		}
	}

	unguard;
}

void ASVehicle::Destroy()
{
	guard(ASVehicle::Destroy);

	Super::Destroy();
	
	unguard;
}

void ASVehicle::PostNetReceive()
{
	guard(ASVehicle::PostNetReceive);

	Super::PostNetReceive();

	eventVehicleStateReceived();

	unguard;
}

void ASVehicle::PostNetReceiveLocation()
{
	guard(ASVehicle::PostNetReceiveLocation);

	AActor::PostNetReceiveLocation();

	unguard;
}

void ASVehicle::PostEditChange()
{
	guard(ASVehicle::PostEditChange);
	Super::PostEditChange();

	// Tell script that a parameters has changed, in case it needs to KUpdateConstraintParams on any constraints.
	this->eventSVehicleUpdateParams();
    unguard;
}

void ASVehicle::setPhysics(BYTE NewPhysics, AActor *NewFloor, FVector NewFloorV)
{
	guard(ASVehicle::setPhysics);

	check(Physics == PHYS_Karma || Physics == PHYS_None);

	if(NewPhysics == PHYS_Karma || NewPhysics == PHYS_None)
		Super::setPhysics(NewPhysics, NewFloor, NewFloorV);

	unguard;
}

void ASVehicle::TickAuthoritative( FLOAT DeltaSeconds )
{
	guard(ASVehicle::TickAuthoritative);

	check(Physics == PHYS_Karma || Physics == PHYS_None); // karma vehicles should always be in PHYS_Karma

	eventTick(DeltaSeconds);
	ProcessState( DeltaSeconds );
	UpdateTimers(DeltaSeconds );

	// Update LifeSpan.
	if( LifeSpan!=0.0f )
	{
		LifeSpan -= DeltaSeconds;
		if( LifeSpan <= 0.0001f )
		{
			GetLevel()->DestroyActor( this );
			return;
		}
	}

	// Perform physics.
	if ( !bDeleteMe && Physics != PHYS_None )
		performPhysics( DeltaSeconds );

	if( KIsAwake() )
		NetUpdateTime = Level->TimeSeconds - 1; // force quick net update

	unguard;
}

void ASVehicle::TickSimulated( FLOAT DeltaSeconds )
{
	guard(ASVehicle::TickSimulated);
	TickAuthoritative(DeltaSeconds);
	unguard;
}

// This is where we update the skeletal mesh based on the SVehicleWheel data (set in the KPerContactCallback)
void ASVehicle::physKarma(FLOAT DeltaTime)
{
	guard(ASVehicle::PhysKarma);

	Super::physKarma(DeltaTime);

	USkeletalMesh* smesh = Cast<USkeletalMesh>(Mesh);
	if(!smesh)
	{
		debugf( TEXT("ASVehicle::physKarma : No Skeletal Mesh! (%s)"), this->GetName() );
		return;
	}

	USkeletalMeshInstance* inst = (USkeletalMeshInstance*)smesh->MeshGetInstance(this);
	if(!inst)
	{
		debugf( TEXT("ASVehicle::physKarma : No Skeletal Mesh Instance! (%s)"), this->GetName() );
		return;
	}

	FMatrix l2w = LocalToWorld().RemoveScaling();

	UBOOL bWheelsMoving = false;

	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);

		vw->CurrentRotation += (vw->SpinVel * DeltaTime * 65535/(2*PI));

		FLOAT SteerRot = (vw->Steer/360.0) * 65535.0f;

		FRotator SteerRotator;
		if(vw->BoneSteerAxis == AXIS_X)
			SteerRotator = FRotator(0, 0, SteerRot);
		else if(vw->BoneSteerAxis == AXIS_Y)
			SteerRotator = FRotator(SteerRot, 0, 0);
		else if(vw->BoneSteerAxis == AXIS_Z)
			SteerRotator = FRotator(0, SteerRot, 0);

		FRotator RollRotator;
		if(vw->BoneRollAxis == AXIS_X)
			RollRotator = FRotator(0, 0, vw->CurrentRotation);
		else if(vw->BoneRollAxis == AXIS_Y)
			RollRotator = FRotator(-vw->CurrentRotation, 0, 0);
		else if(vw->BoneRollAxis == AXIS_Z)
			RollRotator = FRotator(0, -vw->CurrentRotation, 0);

		FCoords WheelCoords = GMath.UnitCoords * SteerRotator * RollRotator;
		FRotator WheelRot = WheelCoords.OrthoRotation();
		inst->SetBoneRotation(vw->BoneName, WheelRot, 0, 1);

		// We need to compensate for the actor scaling, so we move the wheel SuspOffset in world space.
		FLOAT WheelRenderDisplacement = Min(vw->SuspensionMaxRenderTravel, vw->SuspensionPosition);

		FVector MoveBone = l2w.TransformNormal( FVector(0, 0, WheelRenderDisplacement) );
		inst->SetBoneDirection(vw->BoneName, FRotator(0,0,0), MoveBone, 1, 0);

		// See if we have a support bone (and valid distance) as well.
		if( vw->SupportBoneName != NAME_None && Abs(vw->SupportPivotDistance) > 0.001f )
		{
			FLOAT Deflection = ( 65535.0f/(2.0f * (FLOAT)PI) ) * appAtan( WheelRenderDisplacement/vw->SupportPivotDistance );

			FRotator DefRot;
			if(vw->SupportBoneAxis == AXIS_X)
				DefRot = FRotator(0, 0, Deflection);
			else if(vw->SupportBoneAxis == AXIS_Y)
				DefRot = FRotator(Deflection, 0, 0);
			else
				DefRot = FRotator(0, -Deflection, 0);

			inst->SetBoneRotation( vw->SupportBoneName, DefRot, 0, 1 );
		}

		if (fabs(vw->SpinVel) < 0.01f)
			vw->SpinVel = 0.0f;
		else
			bWheelsMoving = true;
	}

	if (bWheelsMoving)
		KWake();

	unguard;
}

#endif

