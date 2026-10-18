/*=============================================================================
	ONSTreadCraft.cpp: Support for treaded vehicles
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Dave Hagewood @ Psyonix - 05/12/03
=============================================================================*/

#include "OnslaughtPrivate.h"

#ifdef WITH_KARMA

// Calculate forces for thrust/turning etc. and apply them.
void AONSTreadCraft::UpdateVehicle(FLOAT DeltaTime)
{
	guard(AONSTreadCraft::UpdateVehicle);

	BikeMPH = 0.0f;

	// Dont go adding forces if vehicle is asleep.
	if( !KIsAwake() )
		return;

	// Calc up (z), right(y) and forward (x) vectors
	FCoords Coords = GMath.UnitCoords / Rotation;
	FVector DirX = Coords.XAxis;
	FVector DirY = Coords.YAxis;
	FVector DirZ = Coords.ZAxis;

	// Get body angular velocity (JTODO: Add AngularVelocity to Actor!)
	FKRigidBodyState rbState;
	KGetRigidBodyState(&rbState);
	FVector angVel(rbState.AngVel.X, rbState.AngVel.Y, rbState.AngVel.Z);

	// Zero force/torque accumulation.
	FVector Force(0.0f, 0.0f, 0.0f);
	FVector Torque(0.0f, 0.0f, 0.0f);

	// Thrust
	Force += (OutputThrust * MaxThrust * DirX);

	// Pitching torque
	Torque += (PitchTorqueFactor * OutputThrust * DirY);

	// Pitch damping
	FLOAT VelMag = angVel | DirY;
	Torque += (-1.0f * VelMag * PitchDampFactor * DirY);

	// Forward damping
	FLOAT ForwardVelMag = Velocity | DirX;
	FLOAT UseForwardDamp = ForwardDampFactor;
	if(!Driver)
		UseForwardDamp += ParkingDampFactor;
	Force += (-1.0f * ForwardVelMag * UseForwardDamp * DirX);

	// Forward damping from turning
	Force += (-1.0f * Abs(OutputTurn) * ForwardVelMag * TurnDampFactor * DirX);

	// Invert steering when we are going backwards
	FLOAT UseTurn;
	if( OutputThrust < InvertSteeringThrottleThreshold )
		UseTurn = -1.0f * OutputTurn;
	else
		UseTurn = OutputTurn;

	// Lateral damping
	VelMag = Velocity | DirY;
	FLOAT UseLatDamp = LateralDampFactor;
	if(!Driver)
		UseLatDamp += ParkingDampFactor;
	Force += (-1.0f * VelMag * UseLatDamp * DirY);

	// Turn damping
	VelMag = angVel | DirZ;
	Torque += (-1.0f * SteerDampFactor * VelMag * DirZ);
		
	// Steer Torque
	Torque += (-1.0f * MaxSteerTorque * UseTurn * DirZ);

	// Banking Torque
	Torque += (BankTorqueFactor * UseTurn * DirX);	

	// Bank (roll) Damping
	VelMag = angVel | DirX;
	Torque += (-1.0f * VelMag * BankDampFactor * DirX);

	// Apply force/torque to body.
	KAddForces(Force, Torque);

	/////// OUTPUT ////////

	// Set current bike speed. Convert from units per sec to miles per hour.
	BikeMPH = Abs( (ForwardVelMag * 3600.0f) / 140800.0f );

	unguard;
}

UBOOL AONSTreadCraft::Tick(FLOAT DeltaTime, enum ELevelTick TickType)
{
	guard(AONSTreadCraft::Tick);

	UBOOL TickDid = Super::Tick(DeltaTime, TickType);
	if(TickDid == 0)
		return 0;

    // If the server, process input and pack updated car info into struct.
    if(Role == ROLE_Authority)
	{
		OutputThrust = Throttle;
		OutputTurn = Steering;

		if (OutputThrust != 0.0 || OutputTurn != 0.0)
			KWake();

		PackState();
	}

	return TickDid;

	unguard;
}

void AONSTreadCraft::PackState()
{
	guard(AONSTreadCraft::PackState);

	bIsAwake = KIsAwake();
	if( !bIsAwake )
		return;
	bHasBeenAwake = true;

	FKRigidBodyState RBState;
	KGetRigidBodyState(&RBState);

	TreadState.ChassisPosition.X = RBState.Position.X;
	TreadState.ChassisPosition.Y = RBState.Position.Y;
	TreadState.ChassisPosition.Z = RBState.Position.Z;

	TreadState.ChassisQuaternion = RBState.Quaternion;

	TreadState.ChassisLinVel.X = 10.f * RBState.LinVel.X;
	TreadState.ChassisLinVel.Y = 10.f * RBState.LinVel.Y;
	TreadState.ChassisLinVel.Z = 10.f * RBState.LinVel.Z;

	TreadState.ChassisAngVel.X = 1000.f * RBState.AngVel.X;
	TreadState.ChassisAngVel.Y = 1000.f * RBState.AngVel.Y;
	TreadState.ChassisAngVel.Z = 1000.f * RBState.AngVel.Z;

	TreadState.ServerThrust = FloatToRangeByte(OutputThrust);
	TreadState.ServerTurn = FloatToRangeByte(OutputTurn);

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

	TreadState.ServerViewPitch = DriverViewPitch;
	TreadState.ServerViewYaw = DriverViewYaw;

	unguard;
}

// Deal with new infotmation about the arriving from the server
void AONSTreadCraft::PostNetReceive()
{
	guard(AONSTreadCraft::PostNetReceive);

	Super::PostNetReceive();

	if( OldTreadState.ChassisPosition == TreadState.ChassisPosition &&
		OldTreadState.ChassisQuaternion.X == TreadState.ChassisQuaternion.X &&
		OldTreadState.ChassisQuaternion.Y == TreadState.ChassisQuaternion.Y &&
		OldTreadState.ChassisQuaternion.Z == TreadState.ChassisQuaternion.Z &&
		OldTreadState.ChassisQuaternion.W == TreadState.ChassisQuaternion.W &&
		OldTreadState.ChassisLinVel == TreadState.ChassisLinVel &&
		OldTreadState.ChassisAngVel == TreadState.ChassisAngVel &&
		OldTreadState.ServerThrust == TreadState.ServerThrust &&
		OldTreadState.ServerTurn == TreadState.ServerTurn &&
		OldTreadState.ServerViewPitch == TreadState.ServerViewPitch &&
		OldTreadState.ServerViewYaw == TreadState.ServerViewYaw )
		return;

	ChassisState.Position.X = TreadState.ChassisPosition.X;
	ChassisState.Position.Y = TreadState.ChassisPosition.Y;
	ChassisState.Position.Z = TreadState.ChassisPosition.Z;

	ChassisState.Quaternion = TreadState.ChassisQuaternion;

	ChassisState.LinVel.X = 0.1f * TreadState.ChassisLinVel.X;
	ChassisState.LinVel.Y = 0.1f * TreadState.ChassisLinVel.Y;
	ChassisState.LinVel.Z = 0.1f * TreadState.ChassisLinVel.Z;

	ChassisState.AngVel.X = 0.001f * TreadState.ChassisAngVel.X;
	ChassisState.AngVel.Y = 0.001f * TreadState.ChassisAngVel.Y;
	ChassisState.AngVel.Z = 0.001f * TreadState.ChassisAngVel.Z;

	// Set OldTreadState to TreadState
	OldTreadState.ChassisPosition = TreadState.ChassisPosition;
	OldTreadState.ChassisQuaternion = TreadState.ChassisQuaternion;
	OldTreadState.ChassisLinVel = TreadState.ChassisLinVel;
	OldTreadState.ChassisAngVel = TreadState.ChassisAngVel;
	OldTreadState.ServerThrust = TreadState.ServerThrust;
	OldTreadState.ServerTurn = TreadState.ServerTurn;
	OldTreadState.ServerViewPitch = TreadState.ServerViewPitch;
	OldTreadState.ServerViewYaw = TreadState.ServerViewYaw;

	bNewTreadState = true;

	OutputThrust = RangeByteToFloat(TreadState.ServerThrust);
	OutputTurn = RangeByteToFloat(TreadState.ServerTurn);
	DriverViewPitch = TreadState.ServerViewPitch;
	DriverViewYaw = TreadState.ServerViewYaw;

	//KDrawRigidBodyState(TreadState.ChassisState, false);

	unguard;
}

#endif // WITH_KARMA

IMPLEMENT_CLASS(AONSTreadCraft);
