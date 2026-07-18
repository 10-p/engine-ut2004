/*=============================================================================
	ONSWheeledCraft.cpp: Support for wheel based vehicles
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Originally created by James Golding as SCar
	* Absorbed and mutated for Onslaught by Dave Hagewood @ Psyonix - 05/02/03
=============================================================================*/

#include "OnslaughtPrivate.h"

#if defined(WITH_KARMA) || defined(WITH_PHYSICS)

void AONSWheeledCraft::UpdateVehicle(FLOAT DeltaTime)
{
	guard(AONSWheeledCraft::UpdateVehicle);

	/////////// STEERING ///////////

	FLOAT maxSteerAngle = MaxSteerAngleCurve.Eval(Velocity.Size());
	FLOAT maxSteer = DeltaTime * SteerSpeed;
	FLOAT deltaSteer;
	
	// When using Jumping Vehicle mutator and holding down to jump, always steer straight.
	if(!bPushDown)
		deltaSteer = (-Steering * maxSteerAngle) - ActualSteering; // Amount we want to move (target - current)
	else
		deltaSteer = -ActualSteering; 

	deltaSteer = Clamp<FLOAT>(deltaSteer, -maxSteer, maxSteer);
	ActualSteering += deltaSteer;

	/////////// ENGINE ///////////

	// Calculate torque at output of engine. Combination of throttle, current RPM and engine braaking.
	FLOAT EngineTorque = OutputGas * TorqueCurve.Eval( EngineRPM );
	FLOAT EngineBraking = (1.0f - OutputGas) * (EngineBrakeRPMScale*EngineRPM * EngineBrakeRPMScale*EngineRPM * EngineBrakeFactor);

	EngineTorque -= EngineBraking;

	DebugInfo = FString::Printf(TEXT("OutputBrake: %f	EngineRPM: %f    EngineTorque: %f"), OutputBrake, EngineRPM, EngineTorque);

	// Total gear ratio between engine and differential (ie before being split between wheels).
	// A higher gear ratio and the torque at the wheels is reduced.
	FLOAT EngineWheelRatio = GearRatios[Gear] * TransRatio;

	// Reset engine RPM. We calculate this by adding the component of each wheel spinning.
	FLOAT NewTotalSpinVel=0.0f;
	EngineRPM = 0.0f;

	// Do model for each wheel.
	for(INT i=0; i<Wheels.Num(); i++)
	{
		USVehicleWheel* vw = Wheels(i);

		/////////// DRIVE ///////////

		// Heuristic to divide torque up so that the wheels that are spinning slower get more of it.
		// Sum of LSDFactor across all wheels should be 1.
		// JTODO: Do we need to handle the case of vehicles with different size wheels?
		FLOAT LSDSplit, EvenSplit, UseSplit;

		EvenSplit = 1/NumPoweredWheels;

		// If no wheels are spinning, just do an even split.
		if(TotalSpinVel > 0.1f)
			LSDSplit = (TotalSpinVel - vw->SpinVel)/((NumPoweredWheels-1) * TotalSpinVel);
		else
			LSDSplit = EvenSplit;

		UseSplit = ((1-LSDFactor) * EvenSplit) + (LSDFactor * LSDSplit);

		// Calculate Drive Torque : applied at wheels (ie after gearbox and differential)
		// This is an 'open differential' ie. equal torque to each wheel
		FLOAT DriveTorque = UseSplit * (EngineTorque / EngineWheelRatio);

		/////////// LONGITUDINAL ///////////

		// Calculate Grip Torque : longitudinal force against ground * distance of action (radius of tyre)
		// LongFrictionFunc is assumed to be reflected for negative Slip Ratio
		FLOAT GripTorque = FTScale * vw->WheelRadius * vw->TireLoad * WheelLongFrictionScale * vw->LongFrictionFunc.Eval( Abs(vw->SlipVel) );
		if(vw->SlipVel < 0.0f)
			GripTorque *= -1.0f;

		// GripTorque can't be more than the torque needed to invert slip ratio.
		FLOAT TransInertia = (EngineInertia / Abs(GearRatios[Gear] * TransRatio)) + vw->WheelInertia;
		//FLOAT SlipAngVel = vw->SlipVel/vw->WheelRadius;

		// Brake torque acts to stop wheels (ie against direction of motion)
		FLOAT BrakeTorque = 0.0f;

		if(vw->SpinVel > 0.0f)
			BrakeTorque = -OutputBrake * MaxBrakeTorque;
		else
			BrakeTorque = OutputBrake * MaxBrakeTorque;

		FLOAT LimitBrakeTorque = ( Abs(vw->SpinVel) * TransInertia ) / DeltaTime; // Size of torque needed to completely stop wheel spinning.
		BrakeTorque = Clamp(BrakeTorque, -LimitBrakeTorque, LimitBrakeTorque); // Never apply more than this!

		// Resultant torque at wheel : torque applied from engine + brakes + equal-and-opposite from tire-road interaction.
		FLOAT WheelTorque = DriveTorque + BrakeTorque - GripTorque;
	
		// Resultant linear force applied to car. (GripTorque applied at road)
		FLOAT VehicleForce = GripTorque / (FTScale * vw->WheelRadius);

		// If the wheel torque is opposing the direction of spin (ie braking) we use friction to apply it.
		if( OutputBrake > 0.0f ||  (DriveTorque + BrakeTorque) * vw->SpinVel < 0.0f)
		{
			vw->DriveForce = 0.0f;
			vw->LongFriction = Abs(VehicleForce) + (OutputBrake * MinBrakeFriction);
		}
		else
		{
			vw->DriveForce = VehicleForce;
			vw->LongFriction = 0.0f;
		}

		// Calculate torque applied back to chassis if wheel is on the ground
		if (vw->bWheelOnGround)
			vw->ChassisTorque = -1.0f * (DriveTorque + BrakeTorque) * ChassisTorqueScale;
		else
			vw->ChassisTorque = 0.0f;

		// Calculate new wheel speed. 
		// The lower the gear ratio, the harder it is to accelerate the engine.
		FLOAT TransAcc = WheelTorque / TransInertia;
		vw->SpinVel += TransAcc * DeltaTime;

		// Make sure the wheel can't spin in the wrong direction for the current gear.
		if(Gear == 0 && vw->SpinVel > 0.0f)
			vw->SpinVel = 0.0f;
		else if(Gear > 0 && vw->SpinVel < 0.0f)
			vw->SpinVel = 0.0f;

		// Accumulate wheel spin speeds to find engine RPM. 
		// The lower the gear ratio, the faster the engine spins for a given wheel speed.
		NewTotalSpinVel += vw->SpinVel;
		EngineRPM += vw->SpinVel / EngineWheelRatio;

		/////////// LATERAL ///////////

		vw->LatFriction = WheelLatFrictionScale * vw->TireLoad;
		vw->LatSlip = vw->LatSlipFunc.Eval(vw->SlipAngle);

		if(OutputHandbrake && vw->bHandbrakeWheel)
		{
			vw->LatFriction *= vw->HandbrakeFrictionFactor;
			vw->LatSlip *= vw->HandbrakeSlipFactor;
		}

		/////////// STEERING  ///////////

		// Pass on steering to wheels that want it.
		if(vw->SteerType == VST_Steered)
			vw->Steer = ActualSteering;
		else if(vw->SteerType == VST_Inverted)
			vw->Steer = -ActualSteering;
		else
			vw->Steer = 0.0f;
	}

	// EngineRPM is in radians per second, want in revolutions per minute
	EngineRPM /= NumPoweredWheels;
	EngineRPM /= 2.0f * (FLOAT)PI; // revs per sec
	EngineRPM *= 60;
	EngineRPM = Max( EngineRPM, 0.01f ); // ensure always positive!

	// Update total wheel spin vel
	TotalSpinVel = NewTotalSpinVel;

	// Turn (yaw) damping.
	FMatrix carTM = LocalToWorld();
	FVector worldUp(carTM.M[2][0], carTM.M[2][1], carTM.M[2][2]);
	FVector worldRight(carTM.M[1][0], carTM.M[1][1], carTM.M[1][2]);
	FVector worldForward(carTM.M[0][0], carTM.M[0][1], carTM.M[0][2]);

	FKRigidBodyState rbState;
	KGetRigidBodyState(&rbState);
	FVector AngVel(rbState.AngVel.X, rbState.AngVel.Y, rbState.AngVel.Z);
	FLOAT TurnAngVel = AngVel | worldUp;

	FLOAT DampingScale = 1.0f - MinAirControlDamping;

	if(bAllowAirControl && !bVehicleOnGround)
	{
		FLOAT TurnDampingMag = (1.0f - DampingScale*Abs(Steering)) * TurnDamping * TurnAngVel;
		KAddForces( FVector(0,0,0), -TurnDampingMag * worldUp );
	}
	else
	{
		FLOAT TurnDampingMag = (1.0f - Abs(Steering)) * TurnDamping * TurnAngVel;
		KAddForces( FVector(0,0,0), -TurnDampingMag * worldUp );
	}

	// If vehicle is in the air and we are allowing air control...
	if(!bVehicleOnGround)
	{
		FLOAT PitchAngVel = AngVel | worldRight;
		FLOAT RollAngVel = AngVel | worldForward;

		if(bAllowAirControl && !bPushDown)
		{		
			FVector AirControlTorque = (worldRight * OutputPitch * -AirPitchTorque);

			if(bIsWalking)
				AirControlTorque += (worldForward * Steering * -AirRollTorque);
			else
				AirControlTorque += (worldUp * Steering * -AirTurnTorque);

			KAddForces( FVector(0,0,0), AirControlTorque );

			// Damping forces
			FLOAT PitchDampingMag = (1.0f - DampingScale*Abs(OutputPitch)) * AirPitchDamping * PitchAngVel;
			FLOAT RollDampingMag = (1.0f - DampingScale*Abs(Steering)) * AirRollDamping * RollAngVel;

			KAddForces( FVector(0,0,0), (-PitchDampingMag * worldRight) + (-RollDampingMag * worldForward) );
		}
		else
		{
			FLOAT PitchDampingMag = AirPitchDamping * PitchAngVel;
			FLOAT RollDampingMag = AirRollDamping * RollAngVel;
			KAddForces( FVector(0,0,0), (-PitchDampingMag * worldRight) + (-RollDampingMag * worldForward) );
		}

	}

	unguard;
}

UBOOL AONSWheeledCraft::Tick(FLOAT DeltaTime, enum ELevelTick TickType)
{
	guard(AONSWheeledCraft::Tick);

	UBOOL TickDid = Super::Tick(DeltaTime, TickType);
	if(TickDid == 0)
		return 0;

	// Update ForwardVel, CarMPH and bIsInverted on both server and client.
	FMatrix carTM = LocalToWorld();

	FVector worldForward(carTM.M[0][0], carTM.M[0][1], carTM.M[0][2]);
	FVector worldUp(carTM.M[2][0], carTM.M[2][1], carTM.M[2][2]);
  	
  	ForwardVel = Velocity | worldForward;
	CarMPH = Abs( (ForwardVel * 3600.0f) / 140800.0f ); // Convert from units per sec to miles per hour.

	bIsInverted = worldUp.Z < 0.2f;

	if ( Level->NetMode != NM_DedicatedServer )
	{
		// Update engine sound pitch
		FLOAT EnginePitch = 255.0 * ((EngineRPM+IdleRPM)/EngineRPMSoundRange);
		EnginePitch = Clamp<FLOAT>(EnginePitch, 0.0f, 255.0f);
		SoundPitch = (BYTE)EnginePitch;
	}

	// If on the server - we work out OutputGas, OutputBrake etc, and pack them to be sent to the client.
	if(Role == ROLE_Authority)
	{
		ProcessCarInput();
		PackState();
	}

	// If there is a SteerBone specified, rotate it around X based on current steering input.
	USkeletalMesh* smesh = Cast<USkeletalMesh>(Mesh);

	if(smesh && SteerBoneName != NAME_None)
	{
		USkeletalMeshInstance* inst = (USkeletalMeshInstance*)smesh->MeshGetInstance(this);

		if(inst)
		{
			FLOAT SteerBoneAngle = (ActualSteering/MaxSteerAngleCurve.Eval(Velocity.Size())) * SteerBoneMaxAngle * (65535.0f/360.0f);
			FRotator SteerRot;

			if(SteerBoneAxis == AXIS_X)
				SteerRot = FRotator(0, 0, SteerBoneAngle);
			else if(SteerBoneAxis == AXIS_Y)
				SteerRot = FRotator(SteerBoneAngle, 0, 0);
			else
				SteerRot = FRotator(0, SteerBoneAngle, 0);


			inst->SetBoneDirection(SteerBoneName, SteerRot, FVector(0, 0, 0), 1, 0);
		}
	}

	// Update any stunt variables.
	UKarmaParams* KP = Cast<UKarmaParams>(KParams);
	APlayerController* PC = Cast<APlayerController>(Controller);
	if(bDoStuntInfo && KP && PC)
	{
		FCoords OldCoords = GMath.UnitCoords / OldRotation;
		FCoords Coords = GMath.UnitCoords / Rotation;
		
		FVector ForwardsInOldPlane = Coords.XAxis - (Coords.XAxis | OldCoords.ZAxis) * OldCoords.ZAxis;
		ForwardsInOldPlane = ForwardsInOldPlane.SafeNormal();

		FLOAT DeltaHeading = appAcos( Clamp<FLOAT>( ForwardsInOldPlane | OldCoords.XAxis, -1.0, 1.0 ) );
		if( (ForwardsInOldPlane | OldCoords.YAxis) < 0.0f )
			DeltaHeading *= -1.0f;

		FLOAT DeltaPitch = appAsin( Clamp<FLOAT>(Coords.XAxis | OldCoords.ZAxis, -1.0f, 1.0f) );
		FLOAT DeltaRoll = appAsin( Clamp<FLOAT>(Coords.YAxis | OldCoords.ZAxis, -1.0f, 1.0f) );

		//debugf( TEXT("DR:%f DP:%f"), DeltaRoll, DeltaPitch );

		UBOOL bCurrentOnGround = (bVehicleOnGround || KP->bContactingLevel);
		DaredevilPoints = 0;

		if(bCurrentOnGround)
		{
			if(!bOldVehicleOnGround && LastOnGroundTime > 0)
			{
				// We just landed - see if we should display Daredevil 'message'
				InAirTime = Level->TimeSeconds - LastOnGroundTime;
				InAirDistance = (Location - LastOnGroundLocation).Size2D()*0.01875f; // Convert to meters

				DaredevilPoints += Max<INT>( appFloor( Abs(InAirSpin)/(0.5f*DaredevilThreshInAirSpin) ) - 1, 0 );
				DaredevilPoints += Max<INT>( appFloor( Abs(InAirPitch)/(0.5f*DaredevilThreshInAirPitch) ) - 1, 0 );
				DaredevilPoints += Max<INT>( appFloor( Abs(InAirRoll)/(0.5f*DaredevilThreshInAirRoll) ) - 1, 0 );
				DaredevilPoints += Max<INT>( appFloor( InAirTime/(0.5f*DaredevilThreshInAirTime) ) - 1, 0 );
				DaredevilPoints += Max<INT>( appFloor( InAirDistance/(0.5f*DaredevilThreshInAirDistance) ) - 1, 0 );

				DaredevilPoints *= 10;

				//debugf( TEXT("S:%f P:%f R:%f T:%f D:%f"), InAirSpin, InAirPitch, InAirRoll, InAirTime, InAirDistance);
				//debugf( TEXT("POINTS: %d"), DaredevilPoints);

				// A wheel must be touching the ground on landing to get a daredevil
				if( bVehicleOnGround && DaredevilPoints > 0 )
				{
					eventOnDaredevil();
				}
			}

			LastOnGroundLocation = Location;
			LastOnGroundTime = Level->TimeSeconds;
			InAirSpin = 0.0f;
			InAirPitch = 0.0f;
			InAirRoll = 0.0f;
		}
		else
		{
			InAirSpin += (180.f/PI) * DeltaHeading;
			InAirPitch += (180.f/PI) * DeltaPitch;
			InAirRoll += (180.f/PI) * DeltaRoll;
		}

		OldRotation = Rotation;
		bOldVehicleOnGround = bCurrentOnGround;
	}

	if(!PC)
	{
		bOldVehicleOnGround = true; // If no-one is in the vehicle, dont consider it for daredevil status.
	}

	if(bAllowChargingJump)
	{

		UBOOL bOldPushDown = bPushDown;

if (bVehicleOnGround)
			bPushDown = (OutputPitch < 0.f);

		if(bPushDown)
		{
			// PRESSING CROUCH

			// Accumulate jump/spin forces.
			JumpForce = Clamp(JumpForce + MaxJumpForce * (DeltaTime/JumpChargeTime), 0.f, MaxJumpForce);

			if( Abs(Steering) > 0.01f )
				JumpSpin = Clamp(JumpSpin + MaxJumpSpin * (DeltaTime/JumpChargeTime) * -Steering, -MaxJumpSpin, MaxJumpSpin);
			else
				JumpSpin = 0.0f;
		}
		else if(bOldPushDown)
		{
			// JUST STOPPED PRESSING CROUCH

			eventJumping();	//give script a chance to alter JumpForce or do effects

			// Do the jump.
			if (Role == ROLE_Authority && bVehicleOnGround)
			{
				FCoords Coords = GMath.UnitCoords / Rotation;
				KAddImpulse((0.0 * JumpForce * Coords.XAxis) + (1.0 * JumpForce * FVector(0,0,1)), FVector(0,0,0), NAME_None);
				KAddAngularImpulse(JumpSpin * Coords.ZAxis);
			}

			JumpForce = 0.f;
			JumpSpin = 0.f;
		}
	}

	return TickDid;

	unguard;
}

void AONSWheeledCraft::preKarmaStep(FLOAT DeltaTime)
{
	guard(AONSWheeledCraft::preKarmaStep);

	Super::preKarmaStep(DeltaTime);

	if (bPushDown && bVehicleOnGround)
	{
		//push down the chassis a little
		FCoords Coords = GMath.UnitCoords / Rotation;
		FVector Z = Coords.ZAxis;
		KAddForces(-75.0 * Z, FVector(0,0,0));
	}

	unguard;
}

// Deal with new infotmation about the arriving from the server
void AONSWheeledCraft::PostNetReceive()
{
	guard(AONSWheeledCraft::PostNetReceive);

	Super::PostNetReceive();

	// If we have received a new car state, deal with it here.
	if( OldCarState.ChassisPosition == CarState.ChassisPosition &&
		OldCarState.ChassisQuaternion.X == CarState.ChassisQuaternion.X &&
		OldCarState.ChassisQuaternion.Y == CarState.ChassisQuaternion.Y &&
		OldCarState.ChassisQuaternion.Z == CarState.ChassisQuaternion.Z &&
		OldCarState.ChassisQuaternion.W == CarState.ChassisQuaternion.W &&
		OldCarState.ChassisLinVel == CarState.ChassisLinVel &&
		OldCarState.ChassisAngVel == CarState.ChassisAngVel &&
		OldCarState.ServerHandbrake == CarState.ServerHandbrake &&
		OldCarState.ServerBrake == CarState.ServerBrake &&
		OldCarState.ServerGas == CarState.ServerGas &&
		OldCarState.ServerGear == CarState.ServerGear &&
		OldCarState.ServerSteering == CarState.ServerSteering &&
		OldCarState.ServerViewPitch == CarState.ServerViewPitch &&
		OldCarState.ServerViewYaw == CarState.ServerViewYaw )
		return;

	ChassisState.Position.X = CarState.ChassisPosition.X;
	ChassisState.Position.Y = CarState.ChassisPosition.Y;
	ChassisState.Position.Z = CarState.ChassisPosition.Z;

	ChassisState.Quaternion = CarState.ChassisQuaternion;

	ChassisState.LinVel.X = 0.1f * CarState.ChassisLinVel.X;
	ChassisState.LinVel.Y = 0.1f * CarState.ChassisLinVel.Y;
	ChassisState.LinVel.Z = 0.1f * CarState.ChassisLinVel.Z;

	ChassisState.AngVel.X = 0.001f * CarState.ChassisAngVel.X;
	ChassisState.AngVel.Y = 0.001f * CarState.ChassisAngVel.Y;
	ChassisState.AngVel.Z = 0.001f * CarState.ChassisAngVel.Z;

	// Set OldCarState to CarState
	OldCarState.ChassisPosition = CarState.ChassisPosition;
	OldCarState.ChassisQuaternion = CarState.ChassisQuaternion;
	OldCarState.ChassisLinVel = CarState.ChassisLinVel;
	OldCarState.ChassisAngVel = CarState.ChassisAngVel;
	OldCarState.ServerHandbrake = CarState.ServerHandbrake;
	OldCarState.ServerBrake = CarState.ServerBrake;
	OldCarState.ServerGas = CarState.ServerGas;
	OldCarState.ServerGear = CarState.ServerGear;
	OldCarState.ServerSteering = CarState.ServerSteering;
	OldCarState.ServerViewPitch = CarState.ServerViewPitch;
	OldCarState.ServerViewYaw = CarState.ServerViewYaw;

	bNewCarState = true; // So KUpdateState will pass this in to update karma state.

	OutputPitch = RangeByteToFloat(CarState.ServerHandbrake);
	OutputHandbrake = (OutputPitch > 0.01f);
	OutputBrake = RangeByteToFloat(CarState.ServerBrake);
	OutputGas = RangeByteToFloat(CarState.ServerGas);
	Gear = CarState.ServerGear;
	Steering = RangeByteToFloat(CarState.ServerSteering);
	DriverViewPitch = CarState.ServerViewPitch;
	DriverViewYaw = CarState.ServerViewYaw;

	//KDrawRigidBodyState(&ChassisState, false);

	unguard;
}

void AONSWheeledCraft::ChangeGear(UBOOL bReverse)
{
	guard(AONSWheeledCraft::Tick);

	// If we want reverse, but aren't there already, and the engine is idling, change to it.
	if(bReverse && Gear != 0)// && EngineRPM < 100)
	{
		Gear = 0;
		return;
	}

	// If we want forwards, but are in reverse, and the engine is idling, change to it.
	if(!bReverse && Gear == 0)// && EngineRPM < 100)
	{
		Gear = 1;
		return;
	}

	// No gear changes in reverse!
	if(Gear == 0)
		return;

	// Forwards...
	if( EngineRPM > ChangeUpPoint && Gear < NumForwardGears )
	{
		Gear++;
	}
	else if( EngineRPM < ChangeDownPoint && Gear > 1 )
	{
		Gear--;
	}	

	unguard;
}

// INPUT: Velocity, bIsInverted, StopThreshold, HandbrakeThresh, Throttle, Driver, ActualSteering, bIsDriving, OutputHandbrake, EngineRPM
// OUTPUT: OutputHandbrake, OutputBrake, bIsDriving, OutputGas, Gear
void AONSWheeledCraft::ProcessCarInput()
{
	guard(AONSWheeledCraft::ProcessCarInput);

	// 'ForwardVel' isn't very helpful if we are inverted, so we just pretend its positive.
	if(bIsInverted)
		ForwardVel = 2.0f * StopThreshold;

	//Log("F:"$ForwardVel$"IsI:"$bIsInverted);

	UBOOL bReverse = false;

	if( Driver == NULL )
	{
		OutputBrake = 1.0f;
		OutputGas = 0.0f;
		ChangeGear(false);
	}
	else
	{
		if(Throttle > 0.01f) // pressing forwards
		{
			if(ForwardVel < -StopThreshold) // going backwards - so brake first
			{
				//debugf(TEXT("F - Brake"));
				bReverse = true;
				OutputBrake = Abs(Throttle);
				bIsDriving = false;
			}
			else // stopped or going forwards, so drive
			{
				//debugf(TEXT("F - Drive"));
				OutputBrake = 0.0f;
				bIsDriving = true;
			}
		}
		else if(Throttle < -0.01f) // pressing backwards
		{
			// We have to release the brakes and then press reverse again to go into reverse
			// Also, we can only go into reverse once the engine has slowed down.
			if(ForwardVel < StopThreshold && bIsDriving == false)
			{
				//debugf(TEXT("B - Drive"));
				bReverse = true;
				OutputBrake = 0.0f;
			}
			else // otherwise, we are going forwards, or still holding brake, so just brake
			{
				//debugf(TEXT("B - Brake"));
				if ( (ForwardVel >= StopThreshold) || IsHumanControlled() )
					OutputBrake = Abs(Throttle);

				bIsDriving = false;
			}
		}
		else // not pressing either
		{
			// If stationary, stick brakes on
			if(Abs(ForwardVel) < StopThreshold)
			{
				//debugf(TEXT("B - Brake"));
				OutputBrake = 1.0;
				bIsDriving = false;
			}
			else if(ForwardVel < -StopThreshold) // otherwise, coast (but keep it in reverse if we're going backwards!)
			{
				//debugf(TEXT("Coast Backwards"));
				bReverse = true;
				OutputBrake = 0.0;
				bIsDriving = false;
			}
			else
			{
				//debugf(TEXT("Coast"));
				OutputBrake = 0.0;
				bIsDriving = false;
			}
		}

		OutputPitch = Rise;
		OutputHandbrake = Rise > 0.0f;

		if (OutputHandbrake)
			OutputBrake = Abs(Throttle);

		// If there is any brake, dont throttle.
		if(OutputBrake > 0.0f)
			OutputGas = 0.0f;
		else
			OutputGas = Abs(Throttle);

		ChangeGear(bReverse);

		KWake();
	}

	unguard;
}

// Put stuff from the server's version of the car into a struct to send to the client and sync it's version of the car.
void AONSWheeledCraft::PackState()
{
	guard(AONSWheeledCraft::PackState);

	// Don't bother packing anything if this body is not enabled.
	bIsAwake = KIsAwake();
	if( !bIsAwake )
		return;
	bHasBeenAwake = true;

	// Get current rigid-body state of car.
	FKRigidBodyState RBState;
	KGetRigidBodyState(&RBState);

	CarState.ChassisPosition.X = RBState.Position.X;
	CarState.ChassisPosition.Y = RBState.Position.Y;
	CarState.ChassisPosition.Z = RBState.Position.Z;

	CarState.ChassisQuaternion = RBState.Quaternion;

	CarState.ChassisLinVel.X = 10.f * RBState.LinVel.X;
	CarState.ChassisLinVel.Y = 10.f * RBState.LinVel.Y;
	CarState.ChassisLinVel.Z = 10.f * RBState.LinVel.Z;

	CarState.ChassisAngVel.X = 1000.f * RBState.AngVel.X;
	CarState.ChassisAngVel.Y = 1000.f * RBState.AngVel.Y;
	CarState.ChassisAngVel.Z = 1000.f * RBState.AngVel.Z;

	CarState.ServerHandbrake = FloatToRangeByte(OutputPitch);
	CarState.ServerBrake = FloatToRangeByte(OutputBrake);
	CarState.ServerGas = FloatToRangeByte(OutputGas);
	CarState.ServerGear = Gear;
	CarState.ServerSteering = FloatToRangeByte(Steering);

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

	CarState.ServerViewPitch = DriverViewPitch;
	CarState.ServerViewYaw = DriverViewYaw;

	unguard;
}

#endif // WITH_KARMA || WITH_PHYSICS

IMPLEMENT_CLASS(AONSWheeledCraft);
