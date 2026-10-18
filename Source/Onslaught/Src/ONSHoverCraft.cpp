/*=============================================================================
	ONSHoverCraft.cpp: Support for ground hovering vehicles
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by James Golding as SCopter
	* Absorbed and mutated for Onslaught by Dave Hagewood @ Psyonix - 04/27/03
=============================================================================*/

#include "OnslaughtPrivate.h"

#ifdef WITH_KARMA

// Don't let repulsors hit non-vehicle pawns of the other team.
UBOOL AONSHoverCraft::KRepulsorsShouldHit(AActor* Actor)
{
	guard(AONSHoverCraft::KRepulsorsShouldHit);

	return Super::KRepulsorsShouldHit(Actor);

	unguard;
}


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
void AONSHoverCraft::UpdateVehicle(FLOAT DeltaTime)
{
	guard(AONSHoverCraft::UpdateVehicle);

	HoverMPH = 0.0f;

	// Dont go adding forces if vehicle is asleep.
	if( !KIsAwake() )
		return;

	if(!this->KParams)
		return;

	UKarmaParams* kparams = Cast<UKarmaParams>(this->KParams);
	if(!kparams)
		return;

	if(Controller)
	{
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
		FVector AngVel(rbState.AngVel.X, rbState.AngVel.Y, rbState.AngVel.Z);
		FLOAT TurnAngVel = AngVel | Up;
		FLOAT RollAngVel = AngVel | DirX;
		FLOAT PitchAngVel = AngVel | DirY;

		FLOAT ForwardVelMag = Velocity | Forward;
		FLOAT RightVelMag = Velocity | Right;

		// Zero force/torque accumulation.
		FVector Force(0.0f, 0.0f, 0.0f);
		FVector Torque(0.0f, 0.0f, 0.0f);

		// Thrust
		Force += (OutputThrust * MaxThrustForce * Forward);

		Force -= ( (1.0f - Abs(OutputThrust)) * LongDamping * ForwardVelMag * Forward);

		// Strafe
		Force += (-OutputStrafe * MaxStrafeForce * Right);

		Force -= ( (1.0f - Abs(OutputStrafe)) * LatDamping * RightVelMag * Right);

		//DebugInfo = FString::Printf(TEXT("HoverHeight Set To %f"), HoverHeight);

		FRotator LookRot = FRotator(DriverViewPitch, DriverViewYaw, 0);
		FVector LookDir = LookRot.Vector();

		// We try to turn the HoverCraft to match the way the camera is facing.

		//// YAW ////

		// Project Look dir into z-plane
		FVector PlaneLookDir = LookDir;
		PlaneLookDir.Z = 0.0f;
		PlaneLookDir.Normalize();

		FLOAT CurrentHeading = HeadingAngle(Forward);
		FLOAT DesiredHeading = HeadingAngle(PlaneLookDir);
		
		if (!bHeadingInitialized)
		{
			TargetHeading = CurrentHeading;
			bHeadingInitialized = true;
		}

		// Move 'target heading' towards 'desired heading' as fast as MaxYawRate allows.
		FLOAT DeltaTargetHeading = FindDeltaAngle(TargetHeading, DesiredHeading);
		FLOAT MaxDeltaHeading = DeltaTime * MaxYawRate;
		DeltaTargetHeading = Clamp<FLOAT>(DeltaTargetHeading, -MaxDeltaHeading, MaxDeltaHeading);
		TargetHeading = UnwindHeading(TargetHeading + DeltaTargetHeading);
		
		// Then put a 'spring' on the copter to target heading.
		FLOAT DeltaHeading = FindDeltaAngle(CurrentHeading, TargetHeading);
		FLOAT TurnTorqueMag = (DeltaHeading / PI) * TurnTorqueFactor;
		TurnTorqueMag = Clamp<FLOAT>( TurnTorqueMag, -TurnTorqueMax, TurnTorqueMax );
		Torque += ( TurnTorqueMag * Up );

		//GStatGraph->AddDataPoint(FString(TEXT("TurnTorqueMag")), TurnTorqueMag, true);

		//// ROLL ////

		// Add roll torque about local X vector as helicopter turns.

		FLOAT RollTorqueMag = ( (-DeltaHeading / PI) * RollTorqueTurnFactor ) + ( OutputStrafe * RollTorqueStrafeFactor );
		RollTorqueMag = Clamp<FLOAT>( RollTorqueMag, -RollTorqueMax, RollTorqueMax );
		Torque += ( RollTorqueMag * DirX );

		//// PITCH ////
		FLOAT PitchTorqueMag = ( OutputThrust * PitchTorqueFactor );
		PitchTorqueMag = Clamp<FLOAT>( PitchTorqueMag, -PitchTorqueMax, PitchTorqueMax );
		Torque += ( PitchTorqueMag * DirY );

		// Steer (yaw) damping
		Torque -= ( TurnAngVel * TurnDamping * Up );

		// Roll damping
		Torque -= ( RollAngVel * RollDamping * DirX );

		// Pitch damping
		Torque -= ( PitchAngVel * PitchDamping * DirY );

		// Apply force/torque to body.
		KAddForces(Force, Torque);

		/////// OUTPUT ////////

		// Set current bike speed. Convert from units per sec to miles per hour.
		HoverMPH = Abs( (ForwardVelMag * 3600.0f) / 140800.0f );
	}

	unguard;
}

UBOOL AONSHoverCraft::Tick(FLOAT DeltaTime, enum ELevelTick TickType)
{
	guard(AONSHoverCraft::Tick);

	UBOOL TickDid = Super::Tick(DeltaTime, TickType);
	if (TickDid == 0)
		return 0;

    // If the server, process input and pack updated car info into struct.
    if (Role == ROLE_Authority)
	{
		if (Driver)
		{
			OutputThrust = Throttle;
			OutputStrafe = Steering;

			KWake(); // keep vehicle alive while driving
		}

		PackState();
	}

	UKarmaParams* KP = Cast<UKarmaParams>(KParams);
	if (KP)
		for (INT i = 0; i < KP->Repulsors.Num(); i++)
			if (KP->Repulsors(i))
			{
				if (bDriving)
					KP->Repulsors(i)->bEnableRepulsion = true;
				else
					KP->Repulsors(i)->bEnableRepulsion = false;
			}

	return TickDid;

	unguard;
}

void AONSHoverCraft::PackState()
{
	guard(AONSHoverCraft::PackState);

	bIsAwake = KIsAwake();
	if( !bIsAwake )
		return;
	bHasBeenAwake = true;

	FKRigidBodyState RBState;
	KGetRigidBodyState(&RBState);

	HoverState.ChassisPosition.X = RBState.Position.X;
	HoverState.ChassisPosition.Y = RBState.Position.Y;
	HoverState.ChassisPosition.Z = RBState.Position.Z;

	HoverState.ChassisQuaternion = RBState.Quaternion;

	HoverState.ChassisLinVel.X = 10.f * RBState.LinVel.X;
	HoverState.ChassisLinVel.Y = 10.f * RBState.LinVel.Y;
	HoverState.ChassisLinVel.Z = 10.f * RBState.LinVel.Z;

	HoverState.ChassisAngVel.X = 1000.f * RBState.AngVel.X;
	HoverState.ChassisAngVel.Y = 1000.f * RBState.AngVel.Y;
	HoverState.ChassisAngVel.Z = 1000.f * RBState.AngVel.Z;

	HoverState.ServerThrust = FloatToRangeByte(OutputThrust);
	HoverState.ServerStrafe = FloatToRangeByte(OutputStrafe);

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

	HoverState.ServerViewPitch = DriverViewPitch;
	HoverState.ServerViewYaw = DriverViewYaw;

	unguard;
}

// Deal with new infotmation about the arriving from the server
void AONSHoverCraft::PostNetReceive()
{
	guard(AONSHoverCraft::PostNetReceive);

	Super::PostNetReceive();

    if (Driver != OldDriver)
    {
        eventSVehicleUpdateParams();
        OldDriver = Driver;
    }

	if( OldHoverState.ChassisPosition == HoverState.ChassisPosition &&
		OldHoverState.ChassisQuaternion.X == HoverState.ChassisQuaternion.X &&
		OldHoverState.ChassisQuaternion.Y == HoverState.ChassisQuaternion.Y &&
		OldHoverState.ChassisQuaternion.Z == HoverState.ChassisQuaternion.Z &&
		OldHoverState.ChassisQuaternion.W == HoverState.ChassisQuaternion.W &&
		OldHoverState.ChassisLinVel == HoverState.ChassisLinVel &&
		OldHoverState.ChassisAngVel == HoverState.ChassisAngVel &&
		OldHoverState.ServerThrust == HoverState.ServerThrust &&
		OldHoverState.ServerStrafe == HoverState.ServerStrafe &&
		OldHoverState.ServerViewPitch == HoverState.ServerViewPitch &&
		OldHoverState.ServerViewYaw == HoverState.ServerViewYaw )
		return;

	ChassisState.Position.X = HoverState.ChassisPosition.X;
	ChassisState.Position.Y = HoverState.ChassisPosition.Y;
	ChassisState.Position.Z = HoverState.ChassisPosition.Z;

	ChassisState.Quaternion = HoverState.ChassisQuaternion;

	ChassisState.LinVel.X = 0.1f * HoverState.ChassisLinVel.X;
	ChassisState.LinVel.Y = 0.1f * HoverState.ChassisLinVel.Y;
	ChassisState.LinVel.Z = 0.1f * HoverState.ChassisLinVel.Z;

	ChassisState.AngVel.X = 0.001f * HoverState.ChassisAngVel.X;
	ChassisState.AngVel.Y = 0.001f * HoverState.ChassisAngVel.Y;
	ChassisState.AngVel.Z = 0.001f * HoverState.ChassisAngVel.Z;

	bNewHoverState = true;

	// Set OldHoverState to HoverState
	OldHoverState.ChassisPosition = HoverState.ChassisPosition;
	OldHoverState.ChassisQuaternion = HoverState.ChassisQuaternion;
	OldHoverState.ChassisLinVel = HoverState.ChassisLinVel;
	OldHoverState.ChassisAngVel = HoverState.ChassisAngVel;
	OldHoverState.ServerThrust = HoverState.ServerThrust;
	OldHoverState.ServerStrafe = HoverState.ServerStrafe;
	OldHoverState.ServerViewPitch = HoverState.ServerViewPitch;
	OldHoverState.ServerViewYaw = HoverState.ServerViewYaw;

	OutputThrust = RangeByteToFloat(HoverState.ServerThrust);
	OutputStrafe = RangeByteToFloat(HoverState.ServerStrafe);
	DriverViewPitch = HoverState.ServerViewPitch;
	DriverViewYaw = HoverState.ServerViewYaw;

	//KDrawRigidBodyState(HoverState.ChassisState, false);

	unguard;
}

#endif // WITH_KARMA

IMPLEMENT_CLASS(AONSHoverCraft);
