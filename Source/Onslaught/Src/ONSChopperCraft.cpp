/*=============================================================================
	ONSChopperCraft.cpp: Support for vehicles with dynamic height hovering capability
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by James Golding as SCopter
	* Absorbed and mutated for Onslaught by Dave Hagewood @ Psyonix - 04/24/03
=============================================================================*/

#include "OnslaughtPrivate.h"

#if defined(WITH_KARMA) || defined(WITH_PHYSICS)

static inline float HeadingAngle(FVector dir)
{
	FLOAT angle = appAcos( Clamp<FLOAT>(dir.X,-1.0f,1.0f) );

	if(dir.Y < 0.0f)
		angle *= -1.0f;

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
void AONSChopperCraft::UpdateVehicle(FLOAT DeltaTime)
{
	guard(AONSChopperCraft::UpdateVehicle);

	CopterMPH = 0.0f;

	// Dont go adding forces if vehicle is asleep.
	if( !KIsAwake() )
		return;

	if(!this->KParams)
		return;

	UKarmaParams* kparams = Cast<UKarmaParams>(this->KParams);
	if(!kparams)
		return;

	// Zero force/torque accumulation.
	FVector Up(0.0f, 0.0f, 1.0f);
	FVector Force(0.0f, 0.0f, 0.0f);
	FVector Torque(0.0f, 0.0f, 0.0f);

	if( bDriving && (Controller || (Location.Z < Level->StallZ)) )
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


		// Get body angular velocity
		FKRigidBodyState rbState;
		KGetRigidBodyState(&rbState);
		FVector AngVel(rbState.AngVel.X, rbState.AngVel.Y, rbState.AngVel.Z);
		FLOAT TurnAngVel = AngVel | Up;
		FLOAT RollAngVel = AngVel | DirX;
		FLOAT PitchAngVel = AngVel | DirY;

		FLOAT ForwardVelMag = Velocity | Forward;
		FLOAT RightVelMag = Velocity | Right;
		FLOAT UpVelMag = Velocity | Up;

		// Thrust
		Force += (OutputThrust * MaxThrustForce * Forward);

		Force -= ( (1.0f - Abs(OutputThrust)) * LongDamping * ForwardVelMag * Forward);

		// Strafe
		Force += (-OutputStrafe * MaxStrafeForce * Right);

		Force -= ( (1.0f - Abs(OutputStrafe)) * LatDamping * RightVelMag * Right);

		// Rise
		AccumulatedTime += DeltaTime;
		if (AccumulatedTime > RandForceInterval)
		{
			AccumulatedTime = 0.0f;
			RandForce.X = 2 * (appFrand() - 0.5) * MaxRandForce;
			RandForce.Y = 2 * (appFrand() - 0.5) * MaxRandForce;
			RandForce.Z = 2 * (appFrand() - 0.5) * MaxRandForce;
			RandTorque.X = 2 * (appFrand() - 0.5) * MaxRandForce;
			RandTorque.Y = 2 * (appFrand() - 0.5) * MaxRandForce;
			RandTorque.Z = 2 * (appFrand() - 0.5) * MaxRandForce;
		}


		if (OutputRise == 0) // If not pushing up or down, apply vertical damping and small perturbation force.
		{
			Force -= (UpDamping * UpVelMag * Up);
			Force += RandForce;
			Torque += RandTorque;
		}
		else
		{
			Force += (OutputRise * MaxRiseForce * Up);
		}

			
		//DebugInfo = FString::Printf(TEXT("HoverHeight Set To %f"), HoverHeight);

		FRotator LookRot = FRotator(DriverViewPitch, DriverViewYaw, 0);
		FVector LookDir = LookRot.Vector();

		//GTempLineBatcher->AddLine(Location, Location + 100 * LookDir, FColor(255, 0, 0));

		// We try to turn the helicopter to match the way the camera is facing.

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

		/////// OUTPUT ////////

		// Set current bike speed. Convert from units per sec to miles per hour.
		CopterMPH = Abs( (ForwardVelMag * 3600.0f) / 140800.0f );
	}
	else
	{
		Force -= (MaxRiseForce * Up); // Go down if no-one driving.
	}

	// Apply force/torque to body.
	KAddForces(Force, Torque);

	unguard;
}

UBOOL AONSChopperCraft::Tick(FLOAT DeltaTime, enum ELevelTick TickType)
{
	guard(AONSChopperCraft::Tick);

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

void AONSChopperCraft::PackState()
{
	guard(AONSChopperCraft::PackState);

	bIsAwake = KIsAwake();
	if( !bIsAwake )
		return;
	bHasBeenAwake = true;

	FKRigidBodyState RBState;
	KGetRigidBodyState(&RBState);

	CopState.ChassisPosition.X = RBState.Position.X;
	CopState.ChassisPosition.Y = RBState.Position.Y;
	CopState.ChassisPosition.Z = RBState.Position.Z;

	CopState.ChassisQuaternion = RBState.Quaternion;

	CopState.ChassisLinVel.X = 10.f * RBState.LinVel.X;
	CopState.ChassisLinVel.Y = 10.f * RBState.LinVel.Y;
	CopState.ChassisLinVel.Z = 10.f * RBState.LinVel.Z;

	CopState.ChassisAngVel.X = 1000.f * RBState.AngVel.X;
	CopState.ChassisAngVel.Y = 1000.f * RBState.AngVel.Y;
	CopState.ChassisAngVel.Z = 1000.f * RBState.AngVel.Z;

	CopState.ServerThrust = FloatToRangeByte(OutputThrust);
	CopState.ServerStrafe = FloatToRangeByte(OutputStrafe);
	CopState.ServerRise = FloatToRangeByte(OutputRise);
	
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

	CopState.ServerViewPitch = DriverViewPitch;
	CopState.ServerViewYaw = DriverViewYaw;

	bNetDirty = true;

	unguard;
}

// Deal with new infotmation about the arriving from the server
void AONSChopperCraft::PostNetReceive()
{
	guard(AONSChopperCraft::PostNetReceive);

	Super::PostNetReceive();

	// If we have received a new car state, deal with it here.
	if( OldCopState.ChassisPosition == CopState.ChassisPosition &&
		OldCopState.ChassisQuaternion.X == CopState.ChassisQuaternion.X &&
		OldCopState.ChassisQuaternion.Y == CopState.ChassisQuaternion.Y &&
		OldCopState.ChassisQuaternion.Z == CopState.ChassisQuaternion.Z &&
		OldCopState.ChassisQuaternion.W == CopState.ChassisQuaternion.W &&
		OldCopState.ChassisLinVel == CopState.ChassisLinVel &&
		OldCopState.ChassisAngVel == CopState.ChassisAngVel &&
		OldCopState.ServerThrust == CopState.ServerThrust &&
		OldCopState.ServerStrafe == CopState.ServerStrafe &&
		OldCopState.ServerRise == CopState.ServerRise &&
		OldCopState.ServerViewPitch == CopState.ServerViewPitch &&
		OldCopState.ServerViewYaw == CopState.ServerViewYaw )
		return;

	ChassisState.Position.X = CopState.ChassisPosition.X;
	ChassisState.Position.Y = CopState.ChassisPosition.Y;
	ChassisState.Position.Z = CopState.ChassisPosition.Z;

	ChassisState.Quaternion = CopState.ChassisQuaternion;

	ChassisState.LinVel.X = 0.1f * CopState.ChassisLinVel.X;
	ChassisState.LinVel.Y = 0.1f * CopState.ChassisLinVel.Y;
	ChassisState.LinVel.Z = 0.1f * CopState.ChassisLinVel.Z;

	ChassisState.AngVel.X = 0.001f * CopState.ChassisAngVel.X;
	ChassisState.AngVel.Y = 0.001f * CopState.ChassisAngVel.Y;
	ChassisState.AngVel.Z = 0.001f * CopState.ChassisAngVel.Z;

	// Set OldCopState to CopState
	OldCopState.ChassisPosition = CopState.ChassisPosition;
	OldCopState.ChassisQuaternion = CopState.ChassisQuaternion;
	OldCopState.ChassisLinVel = CopState.ChassisLinVel;
	OldCopState.ChassisAngVel = CopState.ChassisAngVel;
	OldCopState.ServerThrust = CopState.ServerThrust;
	OldCopState.ServerStrafe = CopState.ServerStrafe;
	OldCopState.ServerRise = CopState.ServerRise;
	OldCopState.ServerViewPitch = CopState.ServerViewPitch;
	OldCopState.ServerViewYaw = CopState.ServerViewYaw;

	bNewCopterState = true;

	OutputThrust = RangeByteToFloat(CopState.ServerThrust);
	OutputStrafe = RangeByteToFloat(CopState.ServerStrafe);
	OutputRise = RangeByteToFloat(CopState.ServerRise);
	DriverViewPitch = CopState.ServerViewPitch;
	DriverViewYaw = CopState.ServerViewYaw;

	//KDrawRigidBodyState(CopState.ChassisState, false);

	unguard;
}

#endif // WITH_KARMA

/* AdjustFlight()
ZDiff is Destination.Z - Location.Z
Distance is 2D distance
bFlyingDown=true means going from FlyingPathNode to NavigationPoint
*/
UBOOL AONSChopperCraft::AdjustFlight(FLOAT ZDiff, UBOOL bFlyingDown, FLOAT Distance, AActor* GoalActor)
{
	guard(AONSChopperCraft::AdjustFlight)

	if ( ZDiff > -0.7f * CollisionHeight )
	{
		Rise = 1.f;
		if ( bFlyingDown && (Distance < 800.f) )
		{
			ANavigationPoint *Nav = Cast<ANavigationPoint>(GoalActor);
			if ( Nav && Nav->bMustBeReachable )
			{
				return true;
			}
		}					
	}
	else if ( ZDiff < -1.f * CollisionHeight )
	{
		Rise = ::Clamp((ZDiff - Velocity.Z)/MaxRiseForce,-1.f,1.f);
		if ( bFlyingDown )
		{
			if ( GoalActor && (Distance < ::Min(2.f*GoalActor->CollisionRadius,ObjectiveGetOutDist)) && (ZDiff > -500.f) && (Level->TimeSeconds - LastJumpOutCheck > 1.f) )
			{
				LastJumpOutCheck = Level->TimeSeconds;
				eventJumpOutCheck();
				if ( !Controller )
					return true;
			}
			else if ( Distance > ::Max(400.f,Abs(ZDiff)) )
				Rise = ::Clamp((Distance - ::Max(400.f,Abs(ZDiff)))/MaxRiseForce,-1.f,1.f);
		}				
	}
	else
	{
		if ( bFlyingDown && (ZDiff <  0.f) )
		{
			Rise = ::Clamp((ZDiff - Velocity.Z)/MaxRiseForce,-1.f,1.f);
		}
		else
		{
			Rise = ::Clamp(-1.f*Velocity.Z/MaxRiseForce,-1.f,1.f);
		}
	}
	return false;
	unguard;
}

IMPLEMENT_CLASS(AONSChopperCraft);
