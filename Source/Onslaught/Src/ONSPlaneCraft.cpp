/*=============================================================================
	ONSPlaneCraft.cpp: Support for airfoil based vehicles
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by James Golding as SCopter
	* Absorbed and mutated for Onslaught by Dave Hagewood @ Psyonix - 05/02/03
=============================================================================*/

#include "OnslaughtPrivate.h"

#ifdef WITH_KARMA

static inline float HeadingAngle(FVector dir)
{
	FLOAT angle = appAcos(dir.X);

	if(dir.Y < 0.0f)
		angle *= -1.0;

	return angle;
}

static inline float FindDeltaAngle(FLOAT a1, FLOAT a2)
{
	FLOAT delta = a2 - a1;

	if(delta > PI)
		delta = delta - (PI * 2.0f);
	else if(delta < -PI)
		delta = delta + (PI * 2.0f);

	return delta;
}

static inline float UnwindHeading(FLOAT a)
{
	while(a > PI)
		a -= ((FLOAT)PI * 2.0f);

	while(a < -PI)
		a += ((FLOAT)PI * 2.0f);

	return a;
}

// Calculate forces for thrust/turning etc. and apply them.
void AONSPlaneCraft::UpdateVehicle(FLOAT DeltaTime)
{
	guard(AONSPlaneCraft::UpdateVehicle);

	// Dont go adding forces if vehicle is asleep.
	if( !KIsAwake() )
		return;

	// Calc up (z), right(y) and forward (x) vectors
	FCoords Coords = GMath.UnitCoords / Rotation;
	FVector DirX = Coords.XAxis;
	FVector DirY = Coords.YAxis;
	FVector DirZ = Coords.ZAxis;

	// 'World plane' forward & right vectors ie. no z component.
	FVector Forward = DirX;
	Forward.Z = 0.0f;
	Forward.Normalize();

	FVector Right = DirY;
	Right.Z = 0.0f;
	Right.Normalize();

	FVector Up(0.0f, 0.0f, 1.0f);

	// Get body angular velocity (JTODO: Add AngularVelocity to Actor!)
	FKRigidBodyState rbState;
	KGetRigidBodyState(&rbState);
	//FVector AngVel(rbState.AngVel.X, rbState.AngVel.Y, rbState.AngVel.Z);
	//FLOAT TurnAngVel = AngVel | Up;
	//FLOAT RollAngVel = AngVel | DirX;
	//FLOAT PitchAngVel = AngVel | DirY;

	/*FLOAT ForwardVelMag = Velocity | Forward;*/
	//FLOAT RightVelMag = Velocity | Right;
	//FLOAT UpVelMag = Velocity | Up;

    if(!this->KParams)
        return;

	UKarmaParams* kparams = Cast<UKarmaParams>(this->KParams);
	if(!kparams)
		return;

//////////////////////////////////////////////////////////////////////////////////////////////

    FVector HitLocation, HitNormal;
    FVector UpAxis, RightAxis, ForwardAxis;
    FVector StabilizerTorqueAxis;
    FVector DesiredDir;
    //FLOAT GroundDist, DotAngle, PctBank;
    FRotator ViewOffset, DesiredRot;

	if (Controller)
	{
		INT ViewOffsetPitch = Controller->Rotation.Pitch & 65535;
		INT ViewOffsetYaw = Controller->Rotation.Yaw & 65535;
		//DebugInfo = FString::Printf(TEXT("Controller->Rotation.Pitch: %d"), Controller->Rotation.Pitch); 

		if (ViewOffsetPitch > 32768)
			ViewOffsetPitch -= 65535;

		if (ViewOffsetYaw > 32768)
			ViewOffsetYaw -= 65535;

		// Reset Controller Rotation
		Controller->Rotation = FRotator(0,0,0);

		FVector HoverForce = FVector(0,0,0);
		FVector Thrust = FVector(0,0,0);
		FVector Lift = FVector(0,0,0);
		FVector Drag = FVector(0,0,0);
		FVector	RelativeWindVelocity = FVector(0,0,0);
		FVector LateralVelocity = FVector(0,0,0);
		FLOAT AngleOfAttack = 0.0f;
		FLOAT RelativeWindFactor = 0.0f;

		// THRUST //
		if (Throttle > 0)
			CurrentThrust = Min(CurrentThrust + (ThrustAcceleration * DeltaTime), MaxThrust);
		else
			CurrentThrust = Max(CurrentThrust - (ThrustAcceleration * DeltaTime), 0.0f);

		Thrust = CurrentThrust * DirX;

		// RELATIVE WIND //
		LateralVelocity = (Velocity | DirY) * DirY;
		RelativeWindVelocity = -(Velocity - LateralVelocity); // Remove the Y component of the velocity and take the inverse
		//GTempLineBatcher->AddLine(Location, Location - RelativeWindVelocity, FColor( FPlane(0.0f,1.0f,0.0f,0.f)));
		//GTempLineBatcher->AddLine(Location, Location + DirX * 750.0, FColor( FPlane(0.0f,0.0f,1.0f,0.f)));
		RelativeWindFactor = AirFactor * RelativeWindVelocity.Size() * RelativeWindVelocity.Size();

		// ANGLE OF ATTACK //
		AngleOfAttack = appAcos(Clamp<FLOAT>((DirX | -RelativeWindVelocity) / RelativeWindVelocity.Size(), -1.0, 1.0));
		AngleOfAttack = (AngleOfAttack / 3.14159f) * 360.0f;
		if ((DirZ | -RelativeWindVelocity) > 0)
			AngleOfAttack *= -1;

		// LIFT //
		Lift = DirZ * LiftCoefficientCurve.Eval(AngleOfAttack) * RelativeWindFactor;
		//GTempLineBatcher->AddLine(Location, Location + Lift, FColor( FPlane(1.0f,0.0f,1.0f,0.f)));

		// DRAG //
		FVector NormalizedVelocity = Velocity;
		NormalizedVelocity.Normalize();
		Drag = DragCoefficientCurve.Eval(AngleOfAttack) * RelativeWindFactor * -NormalizedVelocity;
		//GTempLineBatcher->AddLine(Location, Location + Drag, FColor( FPlane(1.0f,0.0f,1.0f,0.f)));

		// ADD FORCES AND APPLY	
		FVector Force = Thrust + Lift + Drag - LateralVelocity * 0.2f;
		//DebugInfo = FString::Printf(TEXT("Angle of Attack: %f   Cl: %f   Lift: %f   Drag %f"), AngleOfAttack, LiftCoefficientCurve.Eval(AngleOfAttack), LiftCoefficientCurve.Eval(AngleOfAttack) * RelativeWindFactor, DragCoefficientCurve.Eval(AngleOfAttack) * RelativeWindFactor);

		////////////
		// TORQUE //
		////////////

		FVector CurPitchTorque = FVector(0,0,0);
		FVector PitchDampening = FVector(0,0,0);
		FVector CurBankTorque = FVector(0,0,0);
		FVector BankDampening = FVector(0,0,0);

		ViewOffset.Pitch = ViewOffsetPitch;
		ViewOffset.Yaw = -ViewOffsetYaw;
		ViewOffset.Roll = 0;

	//    	DesiredDir = vector(ViewOffset);
	//        DesiredDir.X *= -1;
	//        DesiredDir = DesiredDir >> Rotation;
	//        DesiredUpAxis = vect(0,0,1) >> rotator(DesiredDir);

		// Pitching Torque
		FLOAT PitchTorqueScale = fabs((FLOAT)ViewOffset.Pitch / 4096.0f);
		if (ViewOffset.Pitch > 0)
    		CurPitchTorque = PitchTorque * Min(RelativeWindVelocity.Size() / 3000.0f, 1.0f) * PitchTorqueScale * DirY;
		else
    		CurPitchTorque = PitchTorque * Min(RelativeWindVelocity.Size() / 3000.0f, 1.0f) * PitchTorqueScale * -DirY;

		//// Pitch Dampening
		//PitchDampening = -CurPitchTorque * AngleOfAttack / 90.0f;

		// Banking Torque
		FLOAT BankTorqueScale = fabs((FLOAT)ViewOffset.Yaw / 4096.0f);
		if (ViewOffset.Yaw > 0)
			CurBankTorque = BankTorque * Min(RelativeWindVelocity.Size() / 3000.0f, 1.0f) * BankTorqueScale * DirX;
		else
			CurBankTorque = BankTorque * Min(RelativeWindVelocity.Size() / 3000.0f, 1.0f) * BankTorqueScale * -DirX;

		//// Bank Dampening
		//BankDampening = -CurBankTorque * AngleOfAttack / 90.0f;

		// Banking Pitch Torque
		//FLOAT BankingPitchTorqueScale = 0.0;
		//if ((ViewOffset.Yaw >= 0) && ((Rotation.Roll & 65535) > 32768))
		//	 BankingPitchTorqueScale = Min(fabs(((Rotation.Roll & 65535) - 65536) / 16384.0), 1.0);
		//else if ((ViewOffset.Yaw <= 0) && ((Rotation.Roll & 65535) < 32768))
		//	BankingPitchTorqueScale = Min(fabs((Rotation.Roll & 65535) / 16384.0), 1.0);
		
		//CurBankingPitchTorque = PitchTorque * BankingPitchTorqueScale * -DirY; 
		//DebugInfo = FString::Printf(TEXT("BankingPitchTorqueScale: %f"), BankingPitchTorqueScale);
		//debugf(TEXT("ViewOffset.Pitch: %d    ViewOffset.Yaw: %d"), ViewOffset.Pitch, ViewOffset.Yaw);
		//debugf(TEXT("PitchTorqueScale: %f    BankTorqueScale: %f"), PitchTorqueScale, BankTorqueScale);

		//if ((Rotation.Roll & 65535) > 32768)
		//    PctBank = abs(((Rotation.Roll & 65535) - 65536) / 16384.0);
		//else
		//    PctBank = abs((Rotation.Roll & 65535) / 16384.0);

		//BankSpringTorque = -CurBankTorque * PctBank;
		//CurBankingPitchTorque = BankingPitchTorque * PctBank * DirY;

		// Stabilizing Torque
		//DesiredRot.Pitch = -1000;
		//DesiredRot.Yaw = Rotation.Yaw;
		//DesiredRot.Roll = 0;
		//DesiredDir = FVector(0,0,1) >> DesiredRot;

		//StabilizerTorqueAxis = Normal(UpAxis cross DesiredDir);
		//DotAngle = FClamp(UpAxis dot DesiredDir, -0.9999, 0.9999);
		//StabilizerTorqueScale = ACos(DotAngle)/3.1416;
		//CurStabilizerTorque += StabilizerTorque * StabilizerTorqueScale * StabilizerTorqueAxis;

		FVector Torque = CurPitchTorque + PitchDampening + CurBankTorque + BankDampening;

	//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

		// Apply force/torque to body.
		//debugf(TEXT("BankTorqueScale: %f   ViewOffset.Yaw: %f"), BankTorqueScale, ViewOffset.Yaw);
		KAddForces(Force, Torque);
	}

	unguard;
}

UBOOL AONSPlaneCraft::Tick(FLOAT DeltaTime, enum ELevelTick TickType)
{
	guard(AONSPlaneCraft::Tick);

	UBOOL TickDid = Super::Tick(DeltaTime, TickType);
	if(TickDid == 0)
		return 0;

    // If the server, process input and pack updated car info into struct.
    if(Role == ROLE_Authority)
	{
		if( Driver != NULL )
		{
			OutputThrust = Throttle;
			OutputStrafe = Steering;
			OutputRise = Rise;

			KWake(); // keep vehicle alive while driving
		}

		PackState();
	}

	return TickDid;

	unguard;
}

void AONSPlaneCraft::PackState()
{
	guard(AONSPlaneCraft::PackState);

	bIsAwake = KIsAwake();
	if( !bIsAwake )
		return;
	bHasBeenAwake = true;

	FKRigidBodyState RBState;
	KGetRigidBodyState(&RBState);

	PlaneState.ChassisPosition = RBState.Position;
	PlaneState.ChassisQuaternion = RBState.Quaternion;
	PlaneState.ChassisLinVel = RBState.LinVel;
	PlaneState.ChassisAngVel = RBState.AngVel;

	PlaneState.ServerThrust = OutputThrust;
	PlaneState.ServerStrafe = OutputStrafe;
	PlaneState.ServerRise = OutputRise;

	if (Controller)
	{
		if (IsHumanControlled())
		{			
			DriverViewPitch = Controller->Rotation.Pitch;
			DriverViewYaw = Controller->Rotation.Yaw;
		}
		else
		{
			FRotator ViewRot = (Controller->FocalPoint - Location).Rotation();
			DriverViewPitch = ViewRot.Pitch;
			DriverViewYaw = ViewRot.Yaw;
		}
	}
	else
	{
		DriverViewPitch = Rotation.Pitch;
		DriverViewYaw = Rotation.Yaw;
	}

	PlaneState.ServerViewPitch = DriverViewPitch;
	PlaneState.ServerViewYaw = DriverViewYaw;

	unguard;
}

// Deal with new infotmation about the arriving from the server
void AONSPlaneCraft::PostNetReceive()
{
	guard(AONSPlaneCraft::PostNetReceive);

	Super::PostNetReceive();

	// If we have received a new car state, deal with it here.
	if( OldPlaneState.ChassisPosition.X == PlaneState.ChassisPosition.X &&
		OldPlaneState.ChassisPosition.Y == PlaneState.ChassisPosition.Y &&
		OldPlaneState.ChassisPosition.Z == PlaneState.ChassisPosition.Z &&
		OldPlaneState.ChassisQuaternion.X == PlaneState.ChassisQuaternion.X &&
		OldPlaneState.ChassisQuaternion.Y == PlaneState.ChassisQuaternion.Y &&
		OldPlaneState.ChassisQuaternion.Z == PlaneState.ChassisQuaternion.Z &&
		OldPlaneState.ChassisQuaternion.W == PlaneState.ChassisQuaternion.W &&
		OldPlaneState.ChassisLinVel.X == PlaneState.ChassisLinVel.X &&
		OldPlaneState.ChassisLinVel.Y == PlaneState.ChassisLinVel.Y &&
		OldPlaneState.ChassisLinVel.Z == PlaneState.ChassisLinVel.Z &&
		OldPlaneState.ChassisAngVel.X == PlaneState.ChassisAngVel.X &&
		OldPlaneState.ChassisAngVel.Y == PlaneState.ChassisAngVel.Y &&
		OldPlaneState.ChassisAngVel.Z == PlaneState.ChassisAngVel.Z &&
		OldPlaneState.ServerThrust == PlaneState.ServerThrust &&
		OldPlaneState.ServerStrafe == PlaneState.ServerStrafe &&
		OldPlaneState.ServerRise == PlaneState.ServerRise &&		
		OldPlaneState.ServerViewPitch == PlaneState.ServerViewPitch &&
		OldPlaneState.ServerViewYaw == PlaneState.ServerViewYaw )
		return;

	ChassisState.Position = PlaneState.ChassisPosition;
	ChassisState.Quaternion = PlaneState.ChassisQuaternion;
	ChassisState.LinVel = PlaneState.ChassisLinVel;
	ChassisState.AngVel = PlaneState.ChassisAngVel;
	bNewPlaneState = true;

	// Set OldPlaneState to PlaneState
	OldPlaneState.ChassisPosition.X = PlaneState.ChassisPosition.X;
	OldPlaneState.ChassisPosition.Y = PlaneState.ChassisPosition.Y;
	OldPlaneState.ChassisPosition.Z = PlaneState.ChassisPosition.Z;
	OldPlaneState.ChassisQuaternion = PlaneState.ChassisQuaternion;
	OldPlaneState.ChassisLinVel.X = PlaneState.ChassisLinVel.X;
	OldPlaneState.ChassisLinVel.Y = PlaneState.ChassisLinVel.Y;
	OldPlaneState.ChassisLinVel.Z = PlaneState.ChassisLinVel.Z;
	OldPlaneState.ChassisAngVel.X = PlaneState.ChassisAngVel.X;
	OldPlaneState.ChassisAngVel.Y = PlaneState.ChassisAngVel.Y;
	OldPlaneState.ChassisAngVel.Z = PlaneState.ChassisAngVel.Z;
	OldPlaneState.ServerThrust = PlaneState.ServerThrust;
	OldPlaneState.ServerStrafe = PlaneState.ServerStrafe;
	OldPlaneState.ServerRise = PlaneState.ServerRise;	
	OldPlaneState.ServerViewPitch = PlaneState.ServerViewPitch;
	OldPlaneState.ServerViewYaw = PlaneState.ServerViewYaw;

	OutputThrust = PlaneState.ServerThrust;
	OutputStrafe = PlaneState.ServerStrafe;
	OutputRise = PlaneState.ServerRise;
	DriverViewPitch = PlaneState.ServerViewPitch;
	DriverViewYaw = PlaneState.ServerViewYaw;

	//KDrawRigidBodyState(PlaneState.ChassisState, false);

	unguard;
}

#endif // WITH_KARMA

IMPLEMENT_CLASS(AONSPlaneCraft);
