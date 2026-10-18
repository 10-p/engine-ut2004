/*=============================================================================
	UnPhysic.cpp: Actor physics implementation

	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Steven Polge 3/97
=============================================================================*/

#include "EnginePrivate.h"

void AActor::execSetPhysics( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetPhysics);

	P_GET_BYTE(NewPhysics);
	P_FINISH;

	setPhysics(NewPhysics);
	unguardSlow;
}

void AActor::execAutonomousPhysics( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execAutonomousPhysics);

	P_GET_FLOAT(DeltaSeconds);
	P_FINISH;

	// round acceleration to be consistent with replicated acceleration
	Acceleration.X = 0.1f * int(10 * Acceleration.X);
	Acceleration.Y = 0.1f * int(10 * Acceleration.Y);
	Acceleration.Z = 0.1f * int(10 * Acceleration.Z);

	// Perform physics.
	if( Physics!=PHYS_None )
		performPhysics( DeltaSeconds );
	unguardSlow;
}

//======================================================================================
// smooth movement (no real physics)

void AActor::execMoveSmooth( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execMoveSmooth);

	P_GET_VECTOR(Delta);
	P_FINISH;

	bJustTeleported = 0;
	int didHit = moveSmooth(Delta);

	*(DWORD*)Result = didHit;
	unguardexecSlow;
}

UBOOL AActor::moveSmooth(FVector Delta)
{
	guard(AActor::moveSmooth);

	clock(GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles));
	FCheckResult Hit(1.f);
	UBOOL didHit = GetLevel()->MoveActor( this, Delta, Rotation, Hit );
	if (Hit.Time < 1.f)
	{
		FVector GravDir = FVector(0,0,-1);
		if (PhysicsVolume->Gravity.Z > 0)
			GravDir.Z = 1;
		FVector DesiredDir = Delta.SafeNormal();

		FLOAT UpDown = GravDir | DesiredDir;
		if ( (Abs(Hit.Normal.Z) < 0.2f) && (UpDown < 0.5f) && (UpDown > -0.2f) )
		{
			stepUp(GravDir, DesiredDir, Delta * (1.f - Hit.Time), Hit);
		}
		else
		{
			FVector Adjusted = (Delta - Hit.Normal * (Delta | Hit.Normal)) * (1.f - Hit.Time);
			if( (Delta | Adjusted) >= 0 )
			{
				FVector OldHitNormal = Hit.Normal;
				FVector DesiredDir = Delta.SafeNormal();
				GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
				if (Hit.Time < 1.f)
				{
					SmoothHitWall(Hit.Normal, Hit.Actor);
					TwoWallAdjust(DesiredDir, Adjusted, Hit.Normal, OldHitNormal, Hit.Time);
					GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
				}
			}
		}
	}
	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles));
	return didHit;
	unguard;
}

void AActor::SmoothHitWall(FVector HitNormal, AActor *HitActor)
{
	guardSlow(AActor::SmoothHitWall);

	eventHitWall(HitNormal, HitActor); 
	unguardSlow;
}

void APawn::SmoothHitWall(FVector HitNormal, AActor *HitActor)
{
	guardSlow(APawn::SmoothHitWall);

	if ( Controller )
	{
		FVector Dir = (Controller->Destination - Location).SafeNormal();
		if ( Physics == PHYS_Walking )
		{
			HitNormal.Z = 0;
			Dir.Z = 0;
		}
		if ( Controller->eventNotifyHitWall(HitNormal, HitActor) )
			return;
	}
	eventHitWall(HitNormal, HitActor); 
	unguardSlow;
}

//======================================================================================

void AActor::FindBase()
{
	guard(AActor::findBase);

	FCheckResult Hit(1.f);
	GetLevel()->SingleLineCheck( Hit, this, Location + FVector(0,0,-8), Location, TRACE_AllBlocking, GetCylinderExtent() );
	if (Base != Hit.Actor)
		SetBase(Hit.Actor,Hit.Normal);
	unguard;
}

void AActor::setPhysics(BYTE NewPhysics, AActor *NewFloor, FVector NewFloorV)
{
	guard(AActor::setPhysics);

	if (Physics == NewPhysics)
		return;
//	clock(GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles));

	if ( (Physics == PHYS_RootMotion) || (Physics == PHYS_CinMotion) )
	{
		if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
			((USkeletalMeshInstance*)Mesh->MeshGetInstance(this))->LockRootMotion(0);
	}

    
#ifdef WITH_KARMA
    BYTE OldPhysics = Physics;
#endif
    
	Physics = NewPhysics;

#ifdef WITH_KARMA
    // If the new physics isn't using Karma, but the old one was, shut it down.
    if(OldPhysics == PHYS_Karma)
    {
		AKConstraint* con = Cast<AKConstraint>(this);
        if(con)
            KTermConstraintKarma(con);
        else
            KTermActorDynamics(this);
    }
	else if(OldPhysics == PHYS_KarmaRagDoll)
		KTermActorKarma(this);

	// if we are turning on Karma, do it here.
	if(Physics == PHYS_Karma || Physics == PHYS_KarmaRagDoll)
    {
		KInitActorKarma(this);
    }
#endif

	if ( (Physics == PHYS_Walking) || (Physics == PHYS_None) || (Physics == PHYS_RootMotion) 
			|| (Physics == PHYS_Rotating) || (Physics == PHYS_Spider) )
	{	
		if ( NewFloor == NULL )
			FindBase();
		else if ( Base != NewFloor )
			SetBase(NewFloor,NewFloorV);

		if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
		{				
			if( Physics == PHYS_RootMotion )
			{
				//debugf(TEXT("LOCKING root motion for mesh %s actor %s "),Mesh->GetName(),this->GetName() ); 
				((USkeletalMeshInstance*)Mesh->MeshGetInstance(this))->LockRootMotion(1);
			}
		}
		if ( Physics == PHYS_Spider )
		{
			APawn *P = Cast<APawn>(this);
			if ( P )
				P->bUseCompressedPosition = false; // pawn must replicate Rotation.Roll if PHYS_Spider
		}
	}
	else if (Base != NULL)
		SetBase(NULL);

	if ( (Physics == PHYS_None) || (Physics == PHYS_Rotating) || (Physics == PHYS_CinMotion) )
	{
		Velocity = FVector(0,0,0);
		Acceleration = FVector(0,0,0);

		if( Physics == PHYS_CinMotion )
		{
			((USkeletalMeshInstance*)Mesh->MeshGetInstance(this))->LockRootMotion(2);			
			// Ensure any large off-root offset is taken care of immediately..
			physRootMotion(0.0f);
		}
	}
	
	if ( PhysicsVolume )
		PhysicsVolume->eventPhysicsChangedFor(this);

//	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles));
	unguard;
}

void AMover::performPhysics(FLOAT DeltaSeconds)
{
	guard(AMover::performPhysics);

	//FLOAT OldClock = GStats.DWORDStats(GEngineStats.STATS_Game_SpawningCycles);
	clock(GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles));
	FVector OldVelocity = Velocity;

	// change position
	switch (Physics)
	{
		case PHYS_Projectile: physProjectile(DeltaSeconds, 0); break;
		case PHYS_Falling: physFalling(DeltaSeconds, 0); break;
		case PHYS_Rotating: break;
		case PHYS_MovingBrush: physMovingBrush(DeltaSeconds); break;
        case PHYS_Trailer: physTrailer(DeltaSeconds); break;
#ifdef WITH_KARMA
        case PHYS_Karma: physKarma(DeltaSeconds); break;
        case PHYS_KarmaRagDoll: physKarmaRagDoll(DeltaSeconds); break;
#endif
	}
	if ( bOnlyDirtyReplication && (Physics != PHYS_Rotating) && (Physics != PHYS_MovingBrush) && (Physics != PHYS_None) )
		bNetDirty = true;

	// rotate
	if ( !RotationRate.IsZero() && !bInterpolating && ((Physics != PHYS_Rotating) || !bNoAIRelevance || (Level->TimeSeconds - LastRenderTime < 2.f)) ) 
		physicsRotation(DeltaSeconds);

	// allow touched actors to impact physics
	if ( PendingTouch )
	{
		PendingTouch->eventPostTouch(this);
		AActor *OldTouch = PendingTouch;
		PendingTouch = PendingTouch->PendingTouch;
		OldTouch->PendingTouch = NULL;
	}
	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles));
	unguard;
}

void AActor::performPhysics(FLOAT DeltaSeconds)
{
	guard(AActor::performPhysics);

	if ( (Physics == PHYS_Rotating) && !bInterpolating && (Level->TimeSeconds - LastRenderTime > 1.f) )
		return;
//	FLOAT OldClock = GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles);
	clock(GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles));

	FVector OldVelocity = Velocity;

	// change position
	switch (Physics)
	{
		case PHYS_Projectile: physProjectile(DeltaSeconds, 0); break;
		case PHYS_Falling: physFalling(DeltaSeconds, 0); break;
		case PHYS_Walking: physWalking(DeltaSeconds, 0); break;
		case PHYS_Flying: physFlying(DeltaSeconds, 0); break;
		case PHYS_Rotating: break;
		case PHYS_Trailer: physTrailer(DeltaSeconds); break;
		case PHYS_RootMotion: physRootMotion(DeltaSeconds); break;
		case PHYS_CinMotion: physRootMotion(DeltaSeconds);break;
#ifdef WITH_KARMA
        case PHYS_Karma: physKarma(DeltaSeconds); break;
        case PHYS_KarmaRagDoll: physKarmaRagDoll(DeltaSeconds); break;
#endif
	}

	// rotate
	if ( !RotationRate.IsZero() && !bInterpolating ) 
		physicsRotation(DeltaSeconds);

	// Orient in direction of velocity -- Dave@Psyonix 07/30/04
	if( bOrientToVelocity )
	{
		FCheckResult Hit(1.f);
		GetLevel()->MoveActor( this, FVector(0,0,0), Velocity.Rotation(), Hit );
	}

	// allow touched actors to impact physics
	if ( PendingTouch )
	{
		PendingTouch->eventPostTouch(this);
		AActor *OldTouch = PendingTouch;
		PendingTouch = PendingTouch->PendingTouch;
		OldTouch->PendingTouch = NULL;
	}
	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles));
//	debugf(TEXT("%s physics took %f"),GetName(), (GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles) - OldClock) * GSecondsPerCycle * 1000.0f);
	unguard;
}

void APawn::performPhysics(FLOAT DeltaSeconds)
{
	guard(APawn::performPhysics);

	if( Physics == PHYS_CinMotion )
	{
		// change position
		startNewPhysics(DeltaSeconds,0); // physRootMotion( DeltaSeconds ); //#SKEL
		return; 
		// No PAWN physics consequences for PHYS_CinMotion pawns. -Erik //#SKEL
	}
	
	if ( (Location.Z < ((Region.Zone->bSoftKillZ && (Physics != PHYS_Walking)) ? Region.Zone->KillZ - 2500.f : Region.Zone->KillZ)) && (Controller || (Region.Zone->KillZType != KILLZ_Suicide)) )
	{
		eventFellOutOfWorld(Region.Zone->KillZType);
		if ( bDeleteMe )
			return;
	}
	if ( bCollideWorld && (Region.ZoneNumber == 0) && !bIgnoreOutOfWorld )
	{
		// not in valid spot
		debugf( TEXT("%s fell out of the world!"), GetName());
		eventFellOutOfWorld(KILLZ_None);
		return;
	}
//	FLOAT OldClock = GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles);
	clock(GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles));

	FVector OldVelocity = Velocity;
	OldZ = Location.Z;	// used for eyeheight smoothing

	if ( Physics != PHYS_Walking )
	{
		// only crouch while walking
		if ( bIsCrouched )
			UnCrouch();
	}
	else if ( bWantsToCrouch && bCanCrouch ) 
	{
		// players crouch by setting bWantsToCrouch to true
		if ( !bIsCrouched )
			Crouch();
		else if ( bTryToUncrouch )
		{
			UncrouchTime -= DeltaSeconds;
			if ( UncrouchTime <= 0.f )
			{
				bWantsToCrouch = false;
				bTryToUncrouch = false;
			}
		}
	}

	// change position
	startNewPhysics(DeltaSeconds,0);
	bSimulateGravity = ( (Physics == PHYS_Falling) || (Physics == PHYS_Walking) );

	if ( bIsCrouched && ((Physics != PHYS_Walking) || !bWantsToCrouch) )
	{
		// uncrouch if bWantsToCrouch==false
		UnCrouch();
	}

	if( Controller )
	{
		Controller->MoveTimer -= DeltaSeconds;
		if ( !bInterpolating && Physics != PHYS_Karma && Physics != PHYS_KarmaRagDoll)
		{
			// rotate
			if ( bCrawler || (Rotation != DesiredRotation) || (RotationRate.Roll > 0) || IsHumanControlled() ) 
				physicsRotation(DeltaSeconds, OldVelocity);
		}
	}
	else
	{
		if( Health <= 0 && Physics == PHYS_Rotating && Rotation != DesiredRotation ) // sjs
		{
			//debugf(TEXT("Dead Pawn rotating!"));
			AActor::physicsRotation( DeltaSeconds );
		}
	}

	OldAcceleration = Acceleration;
	AvgPhysicsTime = 0.8f * AvgPhysicsTime + 0.2f * DeltaSeconds;

	if ( PendingTouch )
	{
		PendingTouch->eventPostTouch(this);
		if ( PendingTouch )
		{
			AActor *OldTouch = PendingTouch;
			PendingTouch = PendingTouch->PendingTouch;
			OldTouch->PendingTouch = NULL;
		}
	}
	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles));
//	debugf(TEXT("%s physics took %f"),GetName(), (GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles) - OldClock) * GSecondsPerCycle * 1000.0f);
	unguard;
}

void APawn::startNewPhysics(FLOAT deltaTime, INT Iterations)
{
	if ( (deltaTime < 0.0003f) || (Iterations > 7) )
		return;
	switch (Physics)
	{
		case PHYS_Walking: physWalking(deltaTime, Iterations); break;
		case PHYS_Falling: physFalling(deltaTime, Iterations); break;
		case PHYS_Flying: physFlying(deltaTime, Iterations); break;
		case PHYS_Swimming: physSwimming(deltaTime, Iterations); break;
		case PHYS_Spider: physSpider(deltaTime, Iterations); break;
		case PHYS_Ladder: physLadder(deltaTime, Iterations); break;
		case PHYS_RootMotion: physRootMotion(deltaTime); break;
		case PHYS_CinMotion: physRootMotion(deltaTime); break;
#ifdef WITH_KARMA
        case PHYS_Karma: physKarma(deltaTime); break;
        case PHYS_KarmaRagDoll: physKarmaRagDoll(deltaTime); break;
#endif
        case PHYS_Hovering: physHovering(deltaTime, Iterations); break; // sjs
	}
}

int AActor::fixedTurn(int current, int desired, int deltaRate)
{
	guard(AActor::fixedTurn);

	if (deltaRate == 0)
		return (current & 65535);

	int result = current & 65535;
	current = result;
	desired = desired & 65535;

	if (bFixedRotationDir)
	{
		if (bRotateToDesired)
		{
			if (deltaRate > 0)
			{
				if (current > desired)
					desired += 65536;
				result += Min(deltaRate, desired - current);
			}
			else 
			{
				if (current < desired)
					current += 65536;
				result += ::Max(deltaRate, desired - current);
			}
		}
		else
			result += deltaRate;
	}
	else if (bRotateToDesired)
	{
		if (current > desired)
		{
			if (current - desired < 32768)
				result -= Min((current - desired), Abs(deltaRate));
			else
				result += Min((desired + 65536 - current), Abs(deltaRate));
		}
		else
		{
			if (desired - current < 32768)
				result += Min((desired - current), Abs(deltaRate));
			else
				result -= Min((current + 65536 - desired), Abs(deltaRate));
		}
	}
	return (result & 65535);
	unguard;
}

/* FindSlopeRotation()
return a rotation that will leave actor pointed in desired direction, and placed snugly against its floor
*/
FRotator AActor::FindSlopeRotation(FVector FloorNormal, FRotator NewRotation)
{
	guard(AActor::FindSlopeRotation);

	if ( (FloorNormal.Z < 0.95) && !FloorNormal.IsNearlyZero() ) 
	{
		FRotator TempRot = NewRotation;
		// allow yawing, but pitch and roll fixed based on wall
		TempRot.Pitch = 0;
		FVector YawDir = TempRot.Vector();
		FVector PitchDir = YawDir - FloorNormal * (YawDir | FloorNormal);
		TempRot = PitchDir.Rotation();
		NewRotation.Pitch = TempRot.Pitch;
		FVector RollDir = PitchDir ^ FloorNormal;
		TempRot = RollDir.Rotation();
		NewRotation.Roll = TempRot.Pitch;
	}
	else
		return FRotator(0,NewRotation.Yaw, 0);

	return NewRotation;
	unguard;
}

FRotator APawn::FindSlopeRotation(FVector FloorNormal, FRotator NewRotation)
{
	guard(APawn::FindSlopeRotation);

	if ( (Physics == PHYS_Spider) && Controller )
	{
		FCoords Coords( FVector(0,0,0), Controller->ViewY ^ Floor, Controller->ViewY, Floor );
		return Coords.OrthoRotation();
	}
	else
		return Super::FindSlopeRotation(FloorNormal,NewRotation);
	unguard;
}

void APawn::physicsRotation(FLOAT deltaTime, FVector OldVelocity)
{
	guard(APawn::physicsRotation);

	// Accumulate a desired new rotation.
	FRotator NewRotation = Rotation;	

	INT bPhysOnFloor = (Physics == PHYS_Walking) || (Physics == PHYS_RootMotion);
	bFixedRotationDir = 0;
	INT deltaYaw = 0;
	if ( IsHumanControlled() )
	{
		bRotateToDesired = Controller->bRotateToDesired;
		if ( bRotateToDesired )
		{
			// auto-rotate player to DesiredRotation (for auto-aiming)
			APlayerController *C = Controller->GetAPlayerController();//Cast<APlayerController>(Controller);
			deltaYaw = appRound(2 * C->EnemyTurnSpeed * deltaTime);
			DesiredRotation.Pitch = Controller->DesiredRotation.Pitch;
			DesiredRotation.Yaw = Controller->DesiredRotation.Yaw;
		}
	}
	else
	{
		bRotateToDesired = (Controller != NULL); //Pawns always have a "desired" rotation
		if ( bRotateToDesired && Controller->Enemy && (Controller->Focus == Controller->Enemy) )
		{
			if ( Controller->bEnemyAcquired && (Controller->Enemy->Visibility > 1) )
				deltaYaw = appRound(::Max(RotationRate.Yaw, Controller->RotationRate.Yaw) * deltaTime);
			else
				deltaYaw = appRound(Controller->AcquisitionYawRate * deltaTime);
			INT YawDiff = Abs((Rotation.Yaw & 65535) - DesiredRotation.Yaw);
			if ( (YawDiff < 2048) || (YawDiff > 63287) )
				Controller->bEnemyAcquired = true;
		}
		else
		{
			INT YawDiff = Abs((Rotation.Yaw & 65535) - DesiredRotation.Yaw);
			if ( YawDiff > 32768 )
				YawDiff -= 32768;
			deltaYaw = Clamp(2*YawDiff, RotationRate.Yaw, 2*RotationRate.Yaw);
			deltaYaw = appRound(deltaYaw * deltaTime);
		}
	}

	if ( (Physics == PHYS_Ladder) && OnLadder )
	{
		// must face ladder
		NewRotation = OnLadder->WallDir;
	}
	else if ( bRotateToDesired )
	{
		//YAW 
		if ( DesiredRotation.Yaw != NewRotation.Yaw )
			NewRotation.Yaw = fixedTurn(NewRotation.Yaw, DesiredRotation.Yaw, deltaYaw);

		// PITCH
		if ( !bRollToDesired && (bPhysOnFloor || (Physics == PHYS_Falling)) )
			DesiredRotation.Pitch = 0;
		if ( (!bCrawler || !bPhysOnFloor) && (DesiredRotation.Pitch != NewRotation.Pitch) )
			NewRotation.Pitch = fixedTurn(NewRotation.Pitch, DesiredRotation.Pitch, deltaYaw);

		if ( Controller->bRotateToDesired )
		{
			Controller->Rotation.Yaw = NewRotation.Yaw;
			Controller->Rotation.Pitch = fixedTurn(Controller->Rotation.Pitch, Controller->DesiredRotation.Pitch,deltaYaw);
		}
	}

	//ROLL
	if ( bRollToDesired )
	{
		if ( DesiredRotation.Roll != NewRotation.Roll )
			NewRotation.Roll = fixedTurn(NewRotation.Roll, DesiredRotation.Roll, deltaYaw);
	}
	else if ( bCrawler  )
	{
		if ( !bPhysOnFloor )
		{
			// Straighten out
			INT deltaYaw = (INT) (RotationRate.Yaw * deltaTime);
			NewRotation.Pitch = fixedTurn(NewRotation.Pitch, 0, deltaYaw);
			NewRotation.Roll = fixedTurn(NewRotation.Roll, 0, deltaYaw);
		}
		else
			NewRotation = FindSlopeRotation(Floor,NewRotation);
	}
	else if ( RotationRate.Roll > 0 ) 
	{
		NewRotation.Roll = NewRotation.Roll & 65535;
		if ( NewRotation.Roll < 32768 )
		{
			if ( NewRotation.Roll > RotationRate.Roll )
				NewRotation.Roll = RotationRate.Roll;
		}
		else if ( NewRotation.Roll < 65536 - RotationRate.Roll )
			NewRotation.Roll = 65536 - RotationRate.Roll;
		//pawns roll based on physics
		if ( (Physics == PHYS_Walking) && (Velocity.SizeSquared() < 40000.f) )
		{
			FLOAT SmoothRoll = Min(1.f, 8.f * deltaTime);
			if (NewRotation.Roll < 32768)
				NewRotation.Roll = (INT) (NewRotation.Roll * (1 - SmoothRoll));
			else
				NewRotation.Roll = (INT) (NewRotation.Roll + (65536 - NewRotation.Roll) * SmoothRoll);
		}
		else
		{
			FVector RealAcceleration = (Velocity - OldVelocity)/deltaTime;
			if (RealAcceleration.SizeSquared() > 10000.f) 
			{
				FLOAT MaxRoll = 28000.f;
				if ( Physics == PHYS_Walking )
					MaxRoll = 4096.f;
				NewRotation.Roll = 0;

				RealAcceleration = RealAcceleration.TransformVectorBy(GMath.UnitCoords/NewRotation); //y component will affect roll

				if (RealAcceleration.Y > 0) 
					NewRotation.Roll = Min(RotationRate.Roll, (int)(RealAcceleration.Y * MaxRoll/AccelRate)); 
				else
					NewRotation.Roll = ::Max(65536 - RotationRate.Roll, (int)(65536.f + RealAcceleration.Y * MaxRoll/AccelRate));

				//smoothly change rotation
				Rotation.Roll = Rotation.Roll & 65535;
				if (NewRotation.Roll > 32768)
				{
					if (Rotation.Roll < 32768)
						Rotation.Roll += 65536;
				}
				else if (Rotation.Roll > 32768)
					Rotation.Roll -= 65536;
	
				FLOAT SmoothRoll = Min(1.f, 5.f * deltaTime);
				NewRotation.Roll = (INT) (NewRotation.Roll * SmoothRoll + Rotation.Roll * (1 - SmoothRoll));
			}
			else
			{
				FLOAT SmoothRoll = Min(1.f, 8.f * deltaTime);
				if (NewRotation.Roll < 32768)
					NewRotation.Roll = (INT) (NewRotation.Roll * (1 - SmoothRoll));
				else
					NewRotation.Roll = (INT) (NewRotation.Roll + (65536 - NewRotation.Roll) * SmoothRoll);
			}
		}
	}
	else
		NewRotation.Roll = 0;

	// Set the new rotation.
	if( NewRotation != Rotation )
	{
		FCheckResult Hit(1.f);
		GetLevel()->MoveActor( this, FVector(0,0,0), NewRotation, Hit );
	}
	unguard;
}

void AActor::physicsRotation(FLOAT deltaTime)
{
	guard(AActor::physicsRotation);
	
	if ( (!bRotateToDesired && !bFixedRotationDir)
		|| (bRotateToDesired && (Rotation == DesiredRotation)) )
		return;

	// Accumulate a desired new rotation.
	FRotator NewRotation = Rotation;	
	FRotator deltaRotation = RotationRate * deltaTime;

	//YAW
	if ( (deltaRotation.Yaw != 0) && (!bRotateToDesired || (DesiredRotation.Yaw != NewRotation.Yaw)) )
		NewRotation.Yaw = fixedTurn(NewRotation.Yaw, DesiredRotation.Yaw, deltaRotation.Yaw);
	//PITCH
	if ( (deltaRotation.Pitch != 0) && (!bRotateToDesired || (DesiredRotation.Pitch != NewRotation.Pitch)) )
		NewRotation.Pitch = fixedTurn(NewRotation.Pitch, DesiredRotation.Pitch, deltaRotation.Pitch);
	//ROLL
	if ( (deltaRotation.Roll != 0) && (!bRotateToDesired || (DesiredRotation.Roll != NewRotation.Roll)) )
		NewRotation.Roll = fixedTurn(NewRotation.Roll, DesiredRotation.Roll, deltaRotation.Roll);	

	// Set the new rotation.
	if( NewRotation != Rotation )
	{
		FCheckResult Hit(1.f);
		GetLevel()->MoveActor( this, FVector(0,0,0), NewRotation, Hit );
	}

	if ( bRotateToDesired && (Rotation == DesiredRotation) )
		eventEndedRotation(); //tell thing rotation ended

	unguard;
}

/*====================================================================================
physWalking()
*/
// if AI controlled, check for fall by doing trace forward
// try to find reasonable walk along ledge

FVector APawn::CheckForLedges(FVector AccelDir, FVector Delta, FVector GravDir, int &bCheckedFall, int &bMustJump )
{
	guard(APawn::CheckForLedges)

	FCheckResult Hit(1.f);
	if ( !Base )
	{
		GetLevel()->SingleLineCheck(Hit, this, Location - FVector(0.f,0.f,4.f), Location, TRACE_AllBlocking|TRACE_StopAtFirstHit, GetCylinderExtent());
		if ( Hit.Time == 1.f )
		{
			bMustJump = true;
			return Delta;
		}
	}

	FVector ForwardCheck = AccelDir*CollisionRadius;

	// check if clear in front
	FVector Destn = Location + Delta + ForwardCheck;
	GetLevel()->SingleLineCheck(Hit, this, Destn, Location, TRACE_AllBlocking|TRACE_StopAtFirstHit);
	if (Hit.Time != 1.f)
		return Delta;

	// clear in front - see if there is footing at walk destination
	FLOAT DesiredDist = Delta.Size();
	// check down enough to catch either step or slope
	FLOAT TestDown = 4.f + CollisionHeight + ::Max( (float)UCONST_MAXSTEPHEIGHT, CollisionRadius + DesiredDist);
	// try a point trace
	GetLevel()->SingleLineCheck(Hit, this, Destn + TestDown * GravDir, Destn , TRACE_AllBlocking);
	// if point trace hit nothing, or hit a steep slope, or below a normal step down, do a trace with extent
	if ( !bAvoidLedges )
		Destn = Location + Delta;
	if ( (Hit.Time == 1.f) || (Hit.Normal.Z < UCONST_MINFLOORZ)
		|| ((Hit.Time * TestDown > CollisionHeight + 4.f + Min<FLOAT>(UCONST_MAXSTEPHEIGHT,appSqrt(1 - Hit.Normal.Z * Hit.Normal.Z) * (CollisionRadius + DesiredDist)/Hit.Normal.Z))) )
	{
		GetLevel()->SingleLineCheck(Hit, this, Destn, Location, TRACE_AllBlocking|TRACE_StopAtFirstHit, GetCylinderExtent());
		if ( Hit.Time != 1.f )
			return Delta;
		GetLevel()->SingleLineCheck(Hit, this, Destn + GravDir * (UCONST_MAXSTEPHEIGHT + 4.f), Destn , TRACE_AllBlocking|TRACE_StopAtFirstHit, GetCylinderExtent());
	}
	if ( (Hit.Time == 1.f) || (Hit.Normal.Z < UCONST_MINFLOORZ) )
	{ 
		// We have a ledge!
		if ( Controller && Controller->StopAtLedge() )
			return FVector(0.f,0.f,0.f);

		// check which direction ledge goes
		FVector DesiredDir = Destn - Location;
		DesiredDir = DesiredDir.SafeNormal();
		FVector SideDir(DesiredDir.Y, -1.f * DesiredDir.X, 0.f);
		
		// try left
		FVector LeftSide = Destn + DesiredDist * SideDir;
		GetLevel()->SingleLineCheck(Hit, this, LeftSide, Destn, TRACE_AllBlocking|TRACE_StopAtFirstHit, GetCylinderExtent());
		if ( Hit.Time == 1.f )
		{
			GetLevel()->SingleLineCheck(Hit, this, LeftSide + GravDir * (UCONST_MAXSTEPHEIGHT + 4.f), LeftSide , TRACE_AllBlocking|TRACE_StopAtFirstHit, GetCylinderExtent());
			if ( (Hit.Time < 1.f) && (Hit.Normal.Z >= UCONST_MINFLOORZ) )
			{
				// go left
				FVector NewDir = (LeftSide - Location).SafeNormal();
				return NewDir * DesiredDist;
			}
		}

		// try right
		FVector RightSide = Destn - DesiredDist * SideDir;
		GetLevel()->SingleLineCheck(Hit, this, RightSide, Destn, TRACE_AllBlocking|TRACE_StopAtFirstHit, GetCylinderExtent());
		if ( Hit.Time == 1.f )
		{
			GetLevel()->SingleLineCheck(Hit, this, RightSide + GravDir * (UCONST_MAXSTEPHEIGHT + 4.f), RightSide , TRACE_AllBlocking|TRACE_StopAtFirstHit, GetCylinderExtent());
			if ( (Hit.Time < 1.f) && (Hit.Normal.Z >= UCONST_MINFLOORZ) )
			{
				// go left
				FVector NewDir = (RightSide - Location).SafeNormal();
				return NewDir * DesiredDist;
			}
		}

		// no available direction, so try to jump
		if ( !bCheckedFall && Controller && Controller->IsProbing(NAME_MayFall) )
		{
			bCheckedFall = 1;
			Controller->eventMayFall();
			bMustJump = bCanJump;
		}
	}
	return Delta;
	unguard;
}

void APawn::physWalking(FLOAT deltaTime, INT Iterations)
{
	guard(APawn::physWalking);

	if ( !Controller )
	{
		Acceleration = FVector(0.f,0.f,0.f);
		Velocity = FVector(0.f,0.f,0.f);
		return;
	}
	
	//bound acceleration
	Velocity.Z = 0.f;
	Acceleration.Z = 0.f;
	FVector AccelDir = Acceleration.IsZero() ? Acceleration : Acceleration.SafeNormal();
	calcVelocity(AccelDir, deltaTime, GroundSpeed, PhysicsVolume->GroundFriction, 0, 1, 0);   
	
	FVector DesiredMove = Velocity;
	// Add effect of velocity zone
	// Rather than constant velocity, hacked to make sure that velocity being clamped when walking doesn't 
	// cause the zone velocity to have too much of an effect at fast frame rates
	if ( (PhysicsVolume->ZoneVelocity.SizeSquared() > 0.f)
		&& (IsHumanControlled() || (PhysicsVolume->ZoneVelocity.SizeSquared() > 10000.f)) )
		DesiredMove = DesiredMove + PhysicsVolume->ZoneVelocity * 25 * deltaTime;

	DesiredMove.Z = 0.f;

	//Perform the move
	FVector GravDir = (PhysicsVolume->Gravity.Z > 0.f) ? FVector(0.f,0.f,1.f) : FVector(0.f,0.f,-1.f);
	FVector Down = GravDir * (UCONST_MAXSTEPHEIGHT + 2.f);
	FCheckResult Hit(1.f);
	FVector OldLocation = Location;
	FVector OldFloor = Floor;
	AActor *OldBase = Base;
	bJustTeleported = 0;
	INT bCheckedFall = 0;
	INT bMustJump = 0;
	FLOAT remainingTime = deltaTime;

	while ( (remainingTime > 0.f) && (Iterations < 8) && Controller )
	{
		Iterations++;
		// subdivide moves to be no longer than 0.05 seconds
		FLOAT timeTick = (remainingTime > 0.05f) ? Min(0.05f, remainingTime * 0.5f) : remainingTime;
		remainingTime -= timeTick;
		FVector Delta = timeTick * DesiredMove;
		FVector subLoc = Location;
		FLOAT NewHitTime = 0.f;
		FVector NewFloor(0.f,0.f,0.f);
		AActor *NewBase = NULL;

		if ( Delta.IsNearlyZero() )
			remainingTime = 0.f;
		else
		{
			// if AI controlled or walking player, avoid falls
			if ( Controller && Controller->WantsLedgeCheck() ) 
			{
				FVector subMove = Delta;
				Delta = CheckForLedges(AccelDir, Delta, GravDir, bCheckedFall, bMustJump);
				if ( Controller->MoveTimer == -1.f )
					remainingTime = 0.f;
			}

			// try to move forward
			if ( (Floor.Z < 0.98f) && ((Floor | Delta) < 0.f) )
				Hit.Time = 0.f;
			else
				GetLevel()->MoveActor(this, Delta, Rotation, Hit);

			if (Hit.Time < 1.f) 
			{
				// hit a barrier, try to step up
				FLOAT DesiredDist = Delta.Size();
				FVector DesiredDir = Delta/DesiredDist;
				stepUp(GravDir, DesiredDir, Delta * (1.f - Hit.Time), Hit);
				if ( Physics == PHYS_Falling ) // pawn decided to jump up
				{
					FLOAT ActualDist = (Location - subLoc).Size2D();
					remainingTime += timeTick * (1 - Min(1.f,ActualDist/DesiredDist)); 
					eventFalling();
					if ( Physics == PHYS_Flying )
					{
						Velocity = FVector(0.f,0.f,AirSpeed);
						Acceleration = FVector(0.f,0.f,AccelRate);
					}
					startNewPhysics(remainingTime,Iterations);
					return;
				}
				// see if I already found a floor
				if ( Hit.Time < 1.f )
				{
					NewHitTime = Hit.Time;
					NewFloor = Hit.Normal;
					NewBase = Hit.Actor;
				}
			}
			if ( Physics == PHYS_Swimming ) //just entered water
			{
				startSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
				return;
			}
		}

		// check if imbedded in terrain
		GetLevel()->SingleLineCheck( Hit, this, Location - FVector(0.f,0.f,CollisionHeight), Location, TRACE_Level | TRACE_StopAtFirstHit );
		if ( Hit.Time < 1.f )
		{
			// raise player up
			GetLevel()->SingleLineCheck( Hit, this, Location - FVector(0.f,0.f,CollisionHeight), Location, TRACE_AllBlocking, FVector(CollisionRadius,CollisionRadius,2.f) );
			GetLevel()->MoveActor(this, FVector(0.f,0.f,2.f + CollisionHeight * (1.f - Hit.Time)),Rotation, Hit);
		}

		//drop to floor
		FLOAT FloorDist;

		if ( NewBase )
		{
			Hit.Actor = NewBase;
			Hit.Normal = NewFloor;
			Hit.Time = 0.1f;
			FloorDist = MAXFLOORDIST;
		}
		else
		{
			GetLevel()->SingleLineCheck( Hit, this, Location + Down, Location, TRACE_AllBlocking, GetCylinderExtent() );
			FloorDist = Hit.Time * (UCONST_MAXSTEPHEIGHT + 2.f);
		}

		Floor = Hit.Normal;

		if ( (Hit.Normal.Z < UCONST_MINFLOORZ) && !Delta.IsNearlyZero() && ((Delta | Hit.Normal) < 0) )
		{
			// slide down slope
			FVector Slide = FVector(0.f,0.f,UCONST_MAXSTEPHEIGHT) - Hit.Normal * (FVector(0.f,0.f,UCONST_MAXSTEPHEIGHT) | Hit.Normal);
			GetLevel()->MoveActor(this, -1 * Slide, Rotation, Hit); 
			if ( (Hit.Actor != Base) && (Physics == PHYS_Walking) )
				SetBase(Hit.Actor, Hit.Normal);
		}
		else if( Hit.Time< 1.f && (Hit.Actor!=Base || FloorDist>MAXFLOORDIST) )
		{
			// move down to correct position 
			GetLevel()->MoveActor(this, Down, Rotation, Hit);
			if ( (Hit.Actor != Base) && (Physics == PHYS_Walking) )
				SetBase(Hit.Actor, Hit.Normal);
		}
		else if ( FloorDist < MINFLOORDIST )
		{
			// move up to correct position (average of MAXFLOORDIST and MINFLOORDIST above floor)
			FVector realNorm = Hit.Normal;
			GetLevel()->MoveActor(this, FVector(0.f,0.f,0.5f*(MINFLOORDIST+MAXFLOORDIST) - FloorDist), Rotation, Hit);
			Hit.Time = 0.f;
			Hit.Normal = realNorm;
		}

		// check if just entered water
		if ( Physics == PHYS_Swimming ) 
		{
			startSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
			return;
		}
		
		if( !bMustJump && Hit.Time<1.f && Hit.Normal.Z>=UCONST_MINFLOORZ )  
		{
			if( (Hit.Normal.Z < 0.99f) && ((Hit.Normal.Z * PhysicsVolume->GroundFriction) < 3.3f) ) 
			{
				// slide down slope, depending on friction and gravity
				FVector Slide = (deltaTime * PhysicsVolume->Gravity/(2 * ::Max(0.5f, PhysicsVolume->GroundFriction))) * deltaTime;
				Delta = Slide - Hit.Normal * (Slide | Hit.Normal);
				if( (Delta | Slide) >= 0.f )
					GetLevel()->MoveActor(this, Delta, Rotation, Hit);
				if ( Physics == PHYS_Swimming ) //just entered water
				{
					startSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
					return;
				}
			}				
		}
		else
		{
			if ( !bMustJump && bCanJump && !bCheckedFall && Controller && Controller->IsProbing(NAME_MayFall) )
			{
				// give this pawn a chance to abort its fall
				bCheckedFall = 1;
				Controller->eventMayFall();
			}
			if ( !bJustTeleported && !bMustJump && (!bCanJump || (!bCanWalkOffLedges && (bIsWalking || bIsCrouched))) ) 
			{
				// this pawn shouldn't fall, so undo its move
				Velocity = FVector(0.f,0.f,0.f);
				Acceleration = FVector(0.f,0.f,0.f);
				GetLevel()->FarMoveActor(this,OldLocation,false,false);
				if ( OldBase && (OldBase->bStatic || OldBase->bWorldGeometry || !OldBase->bMovable) )
				{
					SetBase(OldBase,OldFloor);
				}
				if ( Controller )
					Controller->MoveTimer = -1.f;
				return;
			}
			else 
			{
				// falling
				FLOAT DesiredDist = Delta.Size();
				FLOAT ActualDist = (Location - subLoc).Size2D();
				if (DesiredDist == 0.f)
					remainingTime = 0.f;
				else
					remainingTime += timeTick * (1.f - Min(1.f,ActualDist/DesiredDist)); 
				Velocity.Z = 0.f;
				eventFalling();
				if (Physics == PHYS_Walking)
					setPhysics(PHYS_Falling); //default if script didn't change physics
				startNewPhysics(remainingTime,Iterations);
				return;
			}
		}
	}

	// make velocity reflect actual move
	if ( !bJustTeleported && !bNoVelocityUpdate )
		Velocity = (Location - OldLocation) / deltaTime;
	bNoVelocityUpdate = 0;
	Velocity.Z = 0.f;
 	unguard;
}

/* calcVelocity()
Calculates new velocity and acceleration for pawn for this tick
bounds acceleration and velocity, adds effects of friction and momentum
// bBrake only for walking?
*/
void APawn::calcVelocity(FVector AccelDir, FLOAT deltaTime, FLOAT maxSpeed, FLOAT friction, INT bFluid, INT bBrake, INT bBuoyant)
{
	guard(APawn::calcVelocity);

	UBOOL bWalkingPawn = bIsWalking || bIsCrouched;
	FVector OldVelocity = Velocity;

	if ( bBrake && Acceleration.IsZero() ) 
	{
		FVector OldVel = Velocity;
		FVector SumVel = FVector(0,0,0);

		FLOAT RemainingTime = deltaTime;
		// subdivide braking to get reasonably consistent results at lower frame rates
		// (important for packet loss situations w/ networking)
		while( RemainingTime > 0.03f )
		{
			Velocity = Velocity - (2 * Velocity) * 0.03f * friction; //don't drift to a stop, brake
			if ( (Velocity | OldVel) > 0.f )
				SumVel += 0.03f * Velocity/deltaTime;
			RemainingTime -= 0.03f;
		}
		Velocity = Velocity - (2 * Velocity) * RemainingTime * friction; //don't drift to a stop, brake
		if ( (Velocity | OldVel) > 0.f )
			SumVel += RemainingTime * Velocity/deltaTime;
		Velocity = SumVel;
		if ( ((OldVel | Velocity) < 0.f)
			|| (Velocity.SizeSquared() < 100) )//brake to a stop, not backwards
			Velocity = FVector(0,0,0);
	}
	else
	{
		FLOAT VelSize = Velocity.Size();
		if ( bWalkingPawn )
		{
			if ( bIsCrouched )
			{
				if (Acceleration.SizeSquared() > CrouchedPct * CrouchedPct * AccelRate * AccelRate)
					Acceleration = AccelDir * AccelRate * CrouchedPct;
			}
			else if (Acceleration.SizeSquared() > WalkingPct * WalkingPct * AccelRate * AccelRate)
				Acceleration = AccelDir * AccelRate * WalkingPct;
		}
		else if (Acceleration.SizeSquared() > AccelRate * AccelRate)
			Acceleration = AccelDir * AccelRate;
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * deltaTime * friction;  
	}

	Velocity = Velocity * (1 - bFluid * friction * deltaTime) + Acceleration * deltaTime;

	if ( !IsHumanControlled() )
		maxSpeed *= DesiredSpeed;

	if ( bBuoyant )
		Velocity = Velocity + PhysicsVolume->Gravity * deltaTime * (1.f - Buoyancy/Mass);

	if ( IsProbing(NAME_ModifyVelocity) )
		eventModifyVelocity(deltaTime,OldVelocity);

	if ( bWalkingPawn )
	{
		if ( bIsCrouched )
			maxSpeed *= CrouchedPct;
		else
			maxSpeed *= WalkingPct;
	}
	if (Velocity.SizeSquared() > maxSpeed * maxSpeed)
	{
		Velocity = Velocity.SafeNormal();
		Velocity *= maxSpeed;
	}
	unguard;
}

void APawn::stepUp(FVector GravDir, FVector DesiredDir, FVector Delta, FCheckResult &Hit)
{
	guard(APawn::stepUp);

	FVector Down = GravDir * (UCONST_MAXSTEPHEIGHT + 2.f);
	UBOOL bStepDown = true;

	if ( (Abs(Hit.Normal.Z) < MAXSTEPSIDEZ) || (Hit.Normal.Z >= UCONST_MINFLOORZ) )
	{
		// step up - treat as vertical wall 
		GetLevel()->MoveActor(this, -1 * Down, Rotation, Hit); 
		GetLevel()->MoveActor(this, Delta, Rotation, Hit);
	}
	else if ( Physics != PHYS_Walking )
	{
		 // slide up slope
		FLOAT Dist = Delta.Size();
		GetLevel()->MoveActor(this, Delta + FVector(0,0,Dist*Hit.Normal.Z), Rotation, Hit); 
		bStepDown = false;
	}

	if (Hit.Time < 1.f)
	{
		ADecoration *PushDecor = NULL;
		if ( IsHumanControlled() )
			PushDecor = Cast<ADecoration>(Hit.Actor);
		if ( PushDecor && PushDecor->bPushable && ((Hit.Normal | DesiredDir) < -0.9) )
		{
			bNoVelocityUpdate = true;
			Velocity *= Mass/(Mass + PushDecor->Mass);
			processHitWall(Hit.Normal, PushDecor);
			if ( Physics == PHYS_Falling )
				return;
		}
		else if ( (Abs(Hit.Normal.Z) < MAXSTEPSIDEZ) && (Hit.Time * Delta.SizeSquared() > 144.f) )
		{
			// try another step
			if ( bStepDown )
				GetLevel()->MoveActor(this, Down, Rotation, Hit);
			stepUp(GravDir, DesiredDir, Delta * (1 - Hit.Time), Hit);
			return;
		}

		// notify script that pawn ran into a wall
		processHitWall(Hit.Normal, Hit.Actor);
		if ( Physics == PHYS_Falling )
			return;

		//adjust and try again
		Hit.Normal.Z = 0;	// treat barrier as vertical;
		Hit.Normal = Hit.Normal.SafeNormal();
		FVector OriginalDelta = Delta;
		FVector OldHitNormal = Hit.Normal;
		Delta = (Delta - Hit.Normal * (Delta | Hit.Normal)) * (1.f - Hit.Time);
		if( (Delta | OriginalDelta) >= 0 )
		{
			GetLevel()->MoveActor(this, Delta, Rotation, Hit);
			if (Hit.Time < 1.f)
			{
				processHitWall(Hit.Normal, Hit.Actor);
				if ( Physics == PHYS_Falling )
					return;
				TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
				GetLevel()->MoveActor(this, Delta, Rotation, Hit);
			}
		}
	}
	if ( bStepDown )
		GetLevel()->MoveActor(this, Down, Rotation, Hit);

	unguardf(( TEXT("(controller %d)"), Controller!=NULL ));
}

/* AActor::stepUp() used by MoveSmooth() to move smoothly up steps

*/
void AActor::stepUp(FVector GravDir, FVector DesiredDir, FVector Delta, FCheckResult &Hit)
{
	guard(AActor::stepUp);

	FVector Down = GravDir * UCONST_MAXSTEPHEIGHT;

	if ( Abs(Hit.Normal.Z) < MAXSTEPSIDEZ )
	{
		// step up - treat as vertical wall 
		GetLevel()->MoveActor(this, -1 * Down, Rotation, Hit); 
		GetLevel()->MoveActor(this, Delta, Rotation, Hit);
	}
	else
	{
		 // slide up slope
		FLOAT Dist = Delta.Size();
		GetLevel()->MoveActor(this, Delta + FVector(0,0,Dist*Hit.Normal.Z), Rotation, Hit); 
	}

	if (Hit.Time < 1.f)
	{
		if ( (Abs(Hit.Normal.Z) < MAXSTEPSIDEZ) && (Hit.Time * Delta.SizeSquared() > 144.f) )
		{
			// try another step
			GetLevel()->MoveActor(this, Down, Rotation, Hit);
			stepUp(GravDir, DesiredDir, Delta * (1 - Hit.Time), Hit);
			return;
		}

		// notify script that actor ran into a wall
		processHitWall(Hit.Normal, Hit.Actor);
		if ( Physics == PHYS_Falling )
			return;

		//adjust and try again
		Hit.Normal.Z = 0;	// treat barrier as vertical;
		Hit.Normal = Hit.Normal.SafeNormal();
		FVector OriginalDelta = Delta;
		FVector OldHitNormal = Hit.Normal;
		Delta = (Delta - Hit.Normal * (Delta | Hit.Normal)) * (1.f - Hit.Time);
		if( (Delta | OriginalDelta) >= 0 )
		{
			GetLevel()->MoveActor(this, Delta, Rotation, Hit);
			if (Hit.Time < 1.f)
			{
				processHitWall(Hit.Normal, Hit.Actor);
				if ( Physics == PHYS_Falling )
					return;
				TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
				GetLevel()->MoveActor(this, Delta, Rotation, Hit);
			}
		}
	}
	GetLevel()->MoveActor(this, Down, Rotation, Hit);
	unguard;
}

void AActor::processHitWall(FVector HitNormal, AActor *HitActor)
{
	guard(AActor::processHitWall);

	if ( !bBlockActors && HitActor->GetAPawn() && !HitActor->IsEncroacher() )
		return;
	eventHitWall(HitNormal, HitActor);
	unguard;
}

/* 
CanCrouchWalk()
Used by AI to determine if could continue moving forward by crouching
*/
UBOOL APawn::CanCrouchWalk( const FVector& StartLocation, const FVector& EndLocation, AActor* HitActor )
{
	guard(APawn::CanCrouchWalk);

	const FVector CrouchedOffset = FVector(0.0f,0.0f,CrouchHeight-CollisionHeight);

    if( !bCanCrouch )
        return false;

	DWORD TraceFlags = TRACE_World;
	if ( HitActor && !HitActor->bWorldGeometry )
		TraceFlags = TRACE_AllBlocking;

	// quick zero extent trace from start location
	FCheckResult Hit(1.0f);
	GetLevel()->SingleLineCheck( 
		Hit, 
		this,
		EndLocation + CrouchedOffset,
		StartLocation + CrouchedOffset, 
		TraceFlags | TRACE_StopAtFirstHit );

	if( !Hit.Actor )
	{
		// try slower extent trace
		GetLevel()->SingleLineCheck( 
			Hit, 
			this, 
			EndLocation + CrouchedOffset,
			StartLocation + CrouchedOffset,
			TraceFlags,
			FVector(CrouchRadius,CrouchRadius,CrouchHeight) );

			if( Hit.Time == 1.0f )
			{
				bWantsToCrouch = true;
				bTryToUncrouch = true;
				UncrouchTime = 0.5f;
				return true;
			}
	}
	return false;
	unguard;
}

void APawn::processHitWall(FVector HitNormal, AActor *HitActor)
{
	guard(APawn::processHitWall);

	if ( !HitActor )
		return;
		
	if ( HitActor->GetAPawn() )
	{
		if ( !Controller || (Physics == PHYS_Falling) || !HitActor->IsA(AVehicle::StaticClass()) )
			return;
		if ( Controller->eventNotifyHitWall(HitNormal, HitActor) )
			return;
		if ( Controller )
		{
			FVector Cross = (Controller->Destination - Location) ^ FVector(0.f,0.f,1.f);
			FVector Dir = 1.2f * HitActor->CollisionRadius * Cross.SafeNormal();
			if ( (Cross | (Controller->Destination - Location)) < 0.f )
				Dir *= -1.f;
			if ( appFrand() < 0.3f )
				Dir *= -2.f;
			Controller->SetAdjustLocation(Location+Dir);
		}
		return;		
	}
	if ( !bDirectHitWall && Controller )
	{
		if ( Acceleration.IsZero() )
			return;
		FVector Dir = (Controller->Destination - Location).SafeNormal();
		if ( Physics == PHYS_Walking )
		{
			HitNormal.Z = 0;
			Dir.Z = 0;
			HitNormal = HitNormal.SafeNormal();
			Dir = Dir.SafeNormal();
		}
		if ( Controller->MinHitWall < (Dir | HitNormal) )
		{
			if ( Controller->bNotifyFallingHitWall && (Physics == PHYS_Falling) )
			{
				FVector OldVel = Velocity;
				Controller->eventNotifyFallingHitWall(HitNormal, HitActor);
				if ( Velocity != OldVel )
					bJustTeleported = true;
			}
			return;
		}
		// give controller the opportunity to handle the hitwall event instead of the controlled pawn
		if ( Controller->eventNotifyHitWall(HitNormal, HitActor) )
			return;
		if ( Physics != PHYS_Falling )
		{
			UBOOL bTryCrouch = (Physics == PHYS_Walking) && !IsHumanControlled() && bCanCrouch && !bIsCrouched;
				// try moving crouched stepped up
			if ( bTryCrouch && CanCrouchWalk( Location, Location + CollisionRadius*Dir, HitActor) )
					return;
			FCheckResult Hit(1.f);
			GetLevel()->MoveActor(this,FVector(0,0,-1.f * UCONST_MAXSTEPHEIGHT), Rotation, Hit);
			if ( bTryCrouch && CanCrouchWalk( Location, Location + CollisionRadius*Dir, HitActor) )
						return;
			if ( Controller && HitActor->bWorldGeometry )
				Controller->AdjustFromWall(HitNormal, HitActor);
		}
		else if ( Controller->bNotifyFallingHitWall )
		{
			FVector OldVel = Velocity;
			Controller->eventNotifyFallingHitWall(HitNormal, HitActor);
			if ( Velocity != OldVel )
				bJustTeleported = true;
		}
	}
	eventHitWall(HitNormal, HitActor);
	unguard;
}

UBOOL AActor::ShrinkCollision(AActor *HitActor, const FVector &StartLocation)
{
	return false;
}

UBOOL AProjectile::ShrinkCollision(AActor *HitActor, const FVector &StartLocation)
{
	guardSlow(AProjectile::ShrinkCollision);

	if ( bSwitchToZeroCollision	&& ((CollisionHeight != 0.f) || (CollisionRadius != 0.f)) )
	{
		if ( HitActor->bBlockZeroExtentTraces )
		{
			if ( !HitActor->StaticMesh || !HitActor->StaticMesh->UseSimpleBoxCollision )
				return false;
			AStaticMeshActor *S = Cast<AStaticMeshActor>(HitActor);
			if ( S && !S->bExactProjectileCollision )
				return false;
		}
		FCheckResult Hit(1.f);
		FVector Dir = (StartLocation - Location).SafeNormal();

		GetLevel()->SingleLineCheck(Hit, this, StartLocation + 100.f*Dir, Location, TRACE_AllBlocking|TRACE_StopAtFirstHit);
		if ( Hit.Time == 1.f )
			GetLevel()->SingleLineCheck(Hit, this, Location, StartLocation + 100.f*Dir, TRACE_AllBlocking|TRACE_StopAtFirstHit);
		else
			GetLevel()->SingleLineCheck(Hit, this, Location, Hit.Location, TRACE_AllBlocking|TRACE_StopAtFirstHit);

		if ( Hit.Time == 1.f )
		{
			bSwitchToZeroCollision = false;
			SetCollisionSize(0.f,0.f);
			ZeroCollider = HitActor;
			return true;
		}
	}
	return false;
	unguardSlow;
}

void AActor::processLanded(FVector HitNormal, AActor *HitActor, FLOAT remainingTime, INT Iterations)
{
	guard(AActor::processLanded);

	if ( Location.Z < Region.Zone->KillZ )
		eventFellOutOfWorld(Region.Zone->KillZType);
	if ( bDeleteMe )
		return;
	if ( PhysicsVolume->bBounceVelocity && (PhysicsVolume->ZoneVelocity != FVector(0,0,0)) )
	{
		Velocity = PhysicsVolume->ZoneVelocity + FVector(0,0,80);
		return;
	}

	eventLanded(HitNormal);
	if (Physics == PHYS_Falling)
	{
		setPhysics(PHYS_None, HitActor, HitNormal);
		Velocity = FVector(0,0,0);
	}
	if ( bOrientOnSlope && (Physics == PHYS_None) )
	{
		// rotate properly onto slope
		FCheckResult Hit(1.f);
		FRotator NewRotation = FindSlopeRotation(HitNormal,Rotation);
		GetLevel()->MoveActor(this, FVector(0,0,0), NewRotation, Hit); 
	}
	unguard;
}

void ADecoration::processLanded(FVector HitNormal, AActor *HitActor, FLOAT remainingTime, INT Iterations)
{
	guard(ADecoration::processLanded);

	if ( Location.Z < Region.Zone->KillZ )
		eventFellOutOfWorld(Region.Zone->KillZType);
	if ( bDeleteMe )
		return;
	if ( PhysicsVolume->bBounceVelocity && (PhysicsVolume->ZoneVelocity != FVector(0,0,0)) )
	{
		Velocity = PhysicsVolume->ZoneVelocity + FVector(0,0,80);
		return;
	}
	if ( numLandings < 5 ) // make sure its on a valid landing
	{
		FCheckResult Hit(1.f);
		GetLevel()->SingleLineCheck(Hit, this, Location -  FVector(0,0,(CollisionHeight + CollisionRadius + 8)),
			Location - FVector(0,0,(0.8 * CollisionHeight)) , TRACE_ProjTargets|TRACE_StopAtFirstHit);  
		if ( !Hit.Actor )
		{
			FVector partExtent = 0.5 * GetCylinderExtent();
			partExtent.Z *= 2;
			int bQuad1 = GetLevel()->SingleLineCheck(Hit, this, Location + FVector(0.5 * CollisionRadius, 0.5 * CollisionRadius, -8),
				Location + FVector(0.5 * CollisionRadius, 0.5 * CollisionRadius, 0), TRACE_AllBlocking|TRACE_StopAtFirstHit, partExtent);
			int bQuad2 = GetLevel()->SingleLineCheck(Hit, this, Location + FVector(-0.5 * CollisionRadius, 0.5 * CollisionRadius, -8),
				Location + FVector(-0.5 * CollisionRadius, 0.5 * CollisionRadius, 0), TRACE_AllBlocking|TRACE_StopAtFirstHit, partExtent);
			int bQuad3 = GetLevel()->SingleLineCheck(Hit, this, Location + FVector(-0.5 * CollisionRadius, -0.5 * CollisionRadius, -8),
				Location + FVector(-0.5 * CollisionRadius, -0.5 * CollisionRadius, 0), TRACE_AllBlocking|TRACE_StopAtFirstHit, partExtent);
			int bQuad4 = GetLevel()->SingleLineCheck(Hit, this, Location + FVector(0.5 * CollisionRadius, -0.5 * CollisionRadius, -8),
				Location + FVector(0.5 * CollisionRadius, -0.5 * CollisionRadius, 0), TRACE_AllBlocking|TRACE_StopAtFirstHit, partExtent);
			
			if ( (bQuad1 + bQuad2 + bQuad3 + bQuad4 > 1) && !(bQuad1 + bQuad3 == 0) && !(bQuad2 + bQuad4 == 0) )
			{
				numLandings++;
				Velocity = 2 * Clamp( -1.f * Velocity.Z, 30.f, 30.f + CollisionRadius) * 
							FVector((FLOAT)(bQuad1 + bQuad4 - bQuad2 - bQuad3), (FLOAT)(bQuad1 + bQuad2 - bQuad3 - bQuad4) , 0.5);
				return;
			}
		}
		numLandings = 0;
	}
	else
		numLandings = 0;

	Super::processLanded(HitNormal,HitActor,remainingTime,Iterations);
	unguard;
}

void APawn::processLanded(FVector HitNormal, AActor *HitActor, FLOAT remainingTime, INT Iterations)
{
	guard(APawn::processLanded);

	//Check that it is a valid landing (not a BSP cut)
	FCheckResult Hit(1.f);
	GetLevel()->SingleLineCheck(Hit, this, Location -  FVector(0,0,0.2f * CollisionHeight + 8),
		Location, TRACE_Actors|TRACE_World|TRACE_StopAtFirstHit, 0.9f * GetCylinderExtent());  

	if ( Hit.Time == 1.f ) //Not a valid landing
	{
		FVector Adjusted = Location;
		if ( GetLevel()->FindSpot(1.1f * GetCylinderExtent(), Adjusted) && (Adjusted != Location) )
		{
			GetLevel()->FarMoveActor(this, Adjusted, 0, 0);
			Velocity.X += appFrand() * 60 - 30;
			Velocity.Y += appFrand() * 60 - 30; 
			return;
		}
	}
	Floor = HitNormal;
    FRotator NewRotation = FindSlopeRotation(HitNormal,Rotation);
    if ( Health <= 0 ) // sjs - moved here so this is available to eventLanded 
	{
        DesiredRotation = NewRotation;
	}
	if ( !Controller || !Controller->eventNotifyLanded(HitNormal) )
		eventLanded(HitNormal);
	if ( Physics == PHYS_Falling )
	{
		if ( Health > 0 )
			setPhysics(PHYS_Walking, HitActor, HitNormal);
		else
			setPhysics(PHYS_None, HitActor, HitNormal);
	}
	if ( Physics == PHYS_Walking )
		Acceleration = Acceleration.SafeNormal();
	startNewPhysics(remainingTime, Iterations);
	if ( Controller && Controller->bNotifyPostLanded )
		Controller->eventNotifyPostLanded();

	if ( !Controller && (Physics == PHYS_None) )
	{
		// rotate properly onto slope
		FCheckResult Hit(1.f);
		FRotator NewRotation = FindSlopeRotation(HitNormal,Rotation);
		if ( Health <= 0 ) // sjs
		{
            // for now, do nothing here! DesiredRotation = NewRotation;
		}
		else
		GetLevel()->MoveActor(this, FVector(0,0,0), NewRotation, Hit); 
	}

	unguard;
}

FVector APawn::NewFallVelocity(FVector OldVelocity, FVector OldAcceleration, FLOAT timeTick)
{
	guard(APawn::NewFallVelocity);

	FLOAT NetBuoyancy = 0.f;
	FLOAT NetFluidFriction = 0.f;
	GetNetBuoyancy(NetBuoyancy, NetFluidFriction);

	FVector NewVelocity = OldVelocity * (1 - NetFluidFriction * timeTick) 
			+ 0.5f * (OldAcceleration * (1.f - NetBuoyancy/Mass)) * timeTick; 
	return NewVelocity;
	unguard;
}

void AActor::TwoWallAdjust(FVector &DesiredDir, FVector &Delta, FVector &HitNormal, FVector &OldHitNormal, FLOAT HitTime)
{
	guard(AActor::TwoWallAdjust);

	if ((OldHitNormal | HitNormal) <= 0) //90 or less corner, so use cross product for dir
	{
		FVector NewDir = (HitNormal ^ OldHitNormal);
		NewDir = NewDir.SafeNormal();
		Delta = (Delta | NewDir) * (1.f - HitTime) * NewDir;
		if ((DesiredDir | Delta) < 0)
			Delta = -1 * Delta;
	}
	else //adjust to new wall
	{
		Delta = (Delta - HitNormal * (Delta | HitNormal)) * (1.f - HitTime); 
		if ((Delta | DesiredDir) <= 0)
			Delta = FVector(0,0,0);
	}
	unguard;
}

void APawn::physFalling(FLOAT deltaTime, INT Iterations)
{
	guard(APawn::physFalling);

	//bound acceleration, falling object has minimal ability to impact acceleration
	FLOAT BoundSpeed = 0; //Bound final 2d portion of velocity to this if non-zero

	if ( Controller && !ImpactVelocity.IsZero() )
		Controller->AirSteering(deltaTime);

	FVector RealAcceleration = Acceleration;
	FCheckResult Hit(1.f);

	// test for slope to avoid using air control to climb walls
	FLOAT TickAirControl = AirControl;
	Acceleration.Z = 0.f;
	if( TickAirControl > 0.05f )
	{
		FVector TestWalk = ( TickAirControl * AccelRate * Acceleration.SafeNormal() + Velocity ) * deltaTime;
		TestWalk.Z = 0;
		GetLevel()->SingleLineCheck( Hit, this, Location + TestWalk, Location, TRACE_World|TRACE_StopAtFirstHit, FVector( CollisionRadius, CollisionRadius, CollisionHeight ) );
		if( Hit.Actor )
			TickAirControl = 0.f;
	}

	// boost maxAccel to increase player's control when falling
	FLOAT maxAccel = AccelRate * TickAirControl;
	FVector Velocity2D = Velocity;
	Velocity2D.Z = 0;
	FLOAT speed2d = Velocity2D.Size2D(); 
	if ( (speed2d < 10.f) && (TickAirControl > 0.f) ) //allow initial burst
		maxAccel = maxAccel + (10 - speed2d)/deltaTime;
	else if ( speed2d >= GroundSpeed )
	{
		if ( TickAirControl <= 0.05f )
			maxAccel = 1.f;
		else 
			BoundSpeed = speed2d;
	}

	if ( Acceleration.SizeSquared() > maxAccel * maxAccel )
	{
		Acceleration = Acceleration.SafeNormal();
		Acceleration *= maxAccel;
	}
	if ( Controller && !ImpactVelocity.IsZero() )
	{
		FVector OldVel = ImpactVelocity;
		ImpactVelocity = NewFallVelocity(OldVel, Acceleration, deltaTime);
		if ( (ImpactVelocity | OldVel) < 0.f )
			ImpactVelocity = FVector(0.f,0.f,0.f);
	}

	FLOAT remainingTime = deltaTime;
	FLOAT timeTick = 0.1f;
	FVector OldLocation = Location;

	while ( (remainingTime > 0.f) && (Iterations < 8) )
	{
		Iterations++;
		if (remainingTime > 0.05f)
			timeTick = Min(0.05f, remainingTime * 0.5f);
		else timeTick = remainingTime;

		remainingTime -= timeTick;
		OldLocation = Location;
		bJustTeleported = 0;

		FVector OldVelocity = Velocity;
		Velocity = NewFallVelocity(OldVelocity,Acceleration + PhysicsVolume->Gravity + ConstantAcceleration,timeTick);
		if ( IsProbing(NAME_ModifyVelocity) )
			eventModifyVelocity(timeTick,OldVelocity);
		if ( Controller && Controller->bNotifyApex && (Velocity.Z <= 0.f) )
		{
			bJustTeleported = true;
			Controller->bNotifyApex = false;
			Controller->eventNotifyJumpApex();
			Controller->bJumpOverWall = false;
		}
		if ( BoundSpeed != 0 )
		{
			// using air control, so make sure not exceeding acceptable speed
			FVector Vel2D = Velocity;
			Vel2D.Z = 0;
			if ( Vel2D.SizeSquared() > BoundSpeed * BoundSpeed )
			{
				Vel2D = Vel2D.SafeNormal();
				Vel2D = Vel2D * BoundSpeed;
				Vel2D.Z = Velocity.Z;
				Velocity = Vel2D;
			}
		}
		FVector Adjusted = (Velocity + PhysicsVolume->ZoneVelocity) * timeTick;

		GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
		if ( bDeleteMe )
			return;
		else if ( Physics == PHYS_Swimming ) //just entered water
		{
			remainingTime = remainingTime + timeTick * (1.f - Hit.Time);
			startSwimming(OldLocation, OldVelocity, timeTick, remainingTime, Iterations);
			return;
		}
		else if ( Hit.Time < 1.f )
		{
			if (Hit.Normal.Z >= UCONST_MINFLOORZ)
			{
				remainingTime += timeTick * (1.f - Hit.Time);
				if (!bJustTeleported && (Hit.Time > 0.1f) && (Hit.Time * timeTick > 0.003f) )
					Velocity = (Location - OldLocation)/(timeTick * Hit.Time);
				processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
				return;
			}
			else
			{
				//check(!Hit.Normal.IsZero());
				if ( RealAcceleration.IsZero() && Cast<AAIController>(Controller) && !Controller->bNotifyFallingHitWall )
				{
					// try aircontrol push
					Acceleration = Velocity;
					Acceleration.Z = 0.f;
					Acceleration = Acceleration.SafeNormal();
					Acceleration *= AccelRate;
					RealAcceleration = Acceleration;
				}
				else
					processHitWall(Hit.Normal, Hit.Actor);
				FVector OldHitNormal = Hit.Normal;
				FVector Delta = (Adjusted - Hit.Normal * (Adjusted | Hit.Normal)) * (1.f - Hit.Time);

				if( (Delta | Adjusted) >= 0.f )
				{
					//if ( Delta.Z > 0 ) // friction slows sliding up slopes
					//	Delta *= 0.5f;	// FIXME should this be gone forever?
					GetLevel()->MoveActor(this, Delta, Rotation, Hit);
					if (Hit.Time < 1.f) //hit second wall
					{
						// FIXME REMOVE
						if ( Hit.Normal.IsZero() )
							debugf(TEXT("Zero normal from hit actor %s"),Hit.Actor->GetName());
						//check(!Hit.Normal.IsZero());
						if ( Hit.Normal.Z >= UCONST_MINFLOORZ )
						{
							remainingTime = 0.f;
							processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
							return;
						}
						processHitWall(Hit.Normal, Hit.Actor);
						FVector DesiredDir = Adjusted.SafeNormal();
						TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
						int bDitch = ( (OldHitNormal.Z > 0.f) && (Hit.Normal.Z > 0.f) && (Delta.Z == 0.f) && ((Hit.Normal | OldHitNormal) < 0.f) );
						GetLevel()->MoveActor(this, Delta, Rotation, Hit);
						if ( bDitch || (Hit.Normal.Z >= UCONST_MINFLOORZ) )
						{
							remainingTime = 0.f;
							processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
							return;
						}
					}
				}
				FLOAT OldVelZ = OldVelocity.Z;
				OldVelocity = (Location - OldLocation)/timeTick;
				OldVelocity.Z = OldVelZ;
			}
		}

		if ( !bJustTeleported )
		{
			// refine the velocity by figuring out the average actual velocity over the tick, and then the final velocity.
			// This particularly corrects for situations where level geometry affected the fall.
			Velocity = (Location - OldLocation)/timeTick - PhysicsVolume->ZoneVelocity; //actual average velocity
			if ( (Velocity.Z < OldVelocity.Z) || (OldVelocity.Z >= 0.f) )
				Velocity = 2 * Velocity - OldVelocity; //end velocity has 2* accel of avg
			if (!bIgnoreTerminalVelocity && Velocity.SizeSquared() > PhysicsVolume->TerminalVelocity * PhysicsVolume->TerminalVelocity)
			{
				Velocity = Velocity.SafeNormal();
				Velocity *= PhysicsVolume->TerminalVelocity;
			}
		}
	}

	if ( Controller && Controller->bInDodgeMove && (Velocity.Z < 0.f) && (Location.Z < Controller->DodgeLandZ + 10.f) )
	{
		Controller->bInDodgeMove = false;
		FVector Start = Location - FVector(0.f,0.f,CollisionHeight);
		FVector Dir = Velocity.SafeNormal();
		Dir = Dir * (2.f * UCONST_MAXSTEPHEIGHT + 20.f)/Abs(Dir.Z); 
		GetLevel()->SingleLineCheck( Hit, this, Start+Dir, Start, TRACE_World|TRACE_StopAtFirstHit );
		if ( Hit.Time == 1.f )
			GetLevel()->SingleLineCheck( Hit, this, Location+Dir, Location, TRACE_World|TRACE_StopAtFirstHit, GetCylinderExtent() );

		if ( Hit.Time == 1.f )
			Controller->eventMissedDodge();
	}

    if( Health <= 0 ) // sjs
    {
        AActor::physicsRotation( deltaTime );
        return;
    }

	Acceleration = RealAcceleration;
	unguard;
}

void AActor::physFalling(FLOAT deltaTime, INT Iterations)
{
	guard(AActor::physFalling);

	if ( Location.Z < (Region.Zone->bSoftKillZ ? Region.Zone->KillZ - 2500.f : Region.Zone->KillZ) )
	{
		eventFellOutOfWorld(Region.Zone->KillZType);
		return;
	}

	if ( (Region.ZoneNumber == 0) && !bIgnoreOutOfWorld )
	{
		// not in valid spot
		if ( (Role == ROLE_Authority)
			&& (IsA(APickup::StaticClass()) || IsA(ADecoration::StaticClass())) )
			debugf( TEXT("%s fell out of the world at %f %f %f!"), GetFullName(), Location.X, Location.Y, Location.Z );
		eventFellOutOfWorld(KILLZ_None);
		return;
	}

	//bound acceleration, falling object has minimal ability to impact acceleration
	FVector RealAcceleration = Acceleration;
	FCheckResult Hit(1.f);
	FLOAT remainingTime = deltaTime;
	FLOAT timeTick = 0.1f;
	int numBounces = 0;
	FVector OldLocation = Location;

	while ( (remainingTime > 0.f) && (Iterations < 8) )
	{
		Iterations++;
		if (remainingTime > 0.05f)
			timeTick = Min(0.05f, remainingTime * 0.5f);
		else timeTick = remainingTime;

		remainingTime -= timeTick;
		OldLocation = Location;
		bJustTeleported = 0;

		FVector OldVelocity = Velocity;
		FLOAT NetBuoyancy = 0.f;
		FLOAT NetFluidFriction = 0.f;
		GetNetBuoyancy(NetBuoyancy, NetFluidFriction);

		Velocity = OldVelocity * (1 - NetFluidFriction * timeTick) 
				+ 0.5f * (Acceleration + PhysicsVolume->Gravity * (1.f - NetBuoyancy/Mass)) * timeTick; 

		FVector Adjusted = (Velocity + PhysicsVolume->ZoneVelocity) * timeTick;

		GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
		if ( bDeleteMe )
			return;
		if ( (Hit.Time < 1.f) && ShrinkCollision(Hit.Actor, OldLocation) )
		{
			timeTick = timeTick * (1.f - Hit.Time);
			Adjusted = (Velocity + PhysicsVolume->ZoneVelocity) * timeTick;
			GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
		}
		if ( Hit.Time < 1.f )
		{
			ADecoration *D = Cast<ADecoration>(this);
			if ( D && Hit.Actor->GetAPawn() )
				D->numLandings = ::Max(0, D->numLandings - 1); 
			if (bBounce)
			{
				eventHitWall(Hit.Normal, Hit.Actor);
				if ( bDeleteMe || (Physics == PHYS_None) )
					return;
				else if ( numBounces < 2 )
					remainingTime += timeTick * (1.f - Hit.Time);
				numBounces++;
			}
			else
			{
				if (Hit.Normal.Z >= UCONST_MINFLOORZ)
				{
					remainingTime += timeTick * (1.f - Hit.Time);
					if (!bJustTeleported && (Hit.Time > 0.1f) && (Hit.Time * timeTick > 0.003f) )
						Velocity = (Location - OldLocation)/(timeTick * Hit.Time);
					processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
					return;
				}
				else
				{
					processHitWall(Hit.Normal, Hit.Actor);
					if ( bDeleteMe )
						return;
					FVector OldHitNormal = Hit.Normal;
					FVector Delta = (Adjusted - Hit.Normal * (Adjusted | Hit.Normal)) * (1.f - Hit.Time);
					if( (Delta | Adjusted) >= 0 )
					{
						if ( Delta.Z > 0 ) // friction slows sliding up slopes
							Delta *= 0.5;
						GetLevel()->MoveActor(this, Delta, Rotation, Hit);
						if (Hit.Time < 1.f) //hit second wall
						{
							if ( Hit.Normal.Z >= UCONST_MINFLOORZ )
							{
								remainingTime = 0.f;
								processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
								return;
							}
							else 
								processHitWall(Hit.Normal, Hit.Actor);
		
							if ( bDeleteMe )
								return;
							FVector DesiredDir = Adjusted.SafeNormal();
							TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
							int bDitch = ( (OldHitNormal.Z > 0) && (Hit.Normal.Z > 0) && (Delta.Z == 0) && ((Hit.Normal | OldHitNormal) < 0) );
							GetLevel()->MoveActor(this, Delta, Rotation, Hit);
							if ( bDitch || (Hit.Normal.Z >= UCONST_MINFLOORZ) )
							{
								remainingTime = 0.f;
								processLanded(Hit.Normal, Hit.Actor, remainingTime, Iterations);
								return;
							}
						}
					}
					FLOAT OldZ = OldVelocity.Z;
					OldVelocity = (Location - OldLocation)/timeTick;
					OldVelocity.Z = OldZ;
				}
			}
		}

		if (!bBounce && !bJustTeleported)
		{
			// refine the velocity by figuring out the average actual velocity over the tick, and then the final velocity.
			// This particularly corrects for situations where level geometry affected the fall.
			Velocity = (Location - OldLocation)/timeTick - PhysicsVolume->ZoneVelocity; //actual average velocity
			if ( (Velocity.Z < OldVelocity.Z) || (OldVelocity.Z >= 0) )
				Velocity = 2 * Velocity - OldVelocity; //end velocity has 2* accel of avg
			if (!bIgnoreTerminalVelocity && Velocity.SizeSquared() > PhysicsVolume->TerminalVelocity * PhysicsVolume->TerminalVelocity)
			{
				Velocity = Velocity.SafeNormal();
				Velocity *= PhysicsVolume->TerminalVelocity;
			}
		}
	}

	Acceleration = RealAcceleration;
	unguard;
}

void APawn::startSwimming(FVector OldLocation, FVector OldVelocity, FLOAT timeTick, FLOAT remainingTime, INT Iterations)
{
	guard(APawn::startSwimming);
	if (!bBounce && !bJustTeleported)
	{
		if ( timeTick > 0.f )
			Velocity = (Location - OldLocation)/timeTick; //actual average velocity
		Velocity = 2 * Velocity - OldVelocity; //end velocity has 2* accel of avg
		if (!bIgnoreTerminalVelocity && Velocity.SizeSquared() > PhysicsVolume->TerminalVelocity * PhysicsVolume->TerminalVelocity)
		{
			Velocity = Velocity.SafeNormal();
			Velocity *= PhysicsVolume->TerminalVelocity;
		}
	}
	FVector End = findWaterLine(Location, OldLocation);
	FLOAT waterTime = 0.f;
	if (End != Location)
	{	
		waterTime = timeTick * (End - Location).Size()/(Location - OldLocation).Size();
		remainingTime += waterTime;
		FCheckResult Hit(1.f);
		GetLevel()->MoveActor(this, End - Location, Rotation, Hit);
	}
	if ((Velocity.Z > -160.f) && (Velocity.Z < 0)) //allow for falling out of water
		Velocity.Z = -80.f - Velocity.Size2D() * 0.7; //smooth bobbing
	if ( (remainingTime > 0.01f) && (Iterations < 8) )
		physSwimming(remainingTime, Iterations);

	unguard;
}

void APawn::physFlying(FLOAT deltaTime, INT Iterations)
{
	guard(APawn::physFlying);

	FVector AccelDir;

	if ( bCollideWorld && (Region.ZoneNumber == 0) && !bIgnoreOutOfWorld )
	{
		// not in valid spot
		if ( !Controller || !Controller->bIsPlayer )
		{
			debugf( TEXT("%s flew out of the world!"), GetName());
			GetLevel()->DestroyActor( this );
		}
		return;
	}

	if ( Acceleration.IsZero() )
	{
		AccelDir = Acceleration;
		if ( (MinFlySpeed  > 0.f) && (Velocity.SizeSquared() < MinFlySpeed*MinFlySpeed) )
		{
			AccelDir = Rotation.Vector();
			Acceleration = AccelRate * AccelDir;
		}
	}
	else
		AccelDir = Acceleration.SafeNormal();

	calcVelocity(AccelDir, deltaTime, AirSpeed, 0.5f * PhysicsVolume->FluidFriction, 1, 0, 0);  
	if ( (MinFlySpeed  > 0.f) && (Velocity.SizeSquared() < MinFlySpeed*MinFlySpeed) )
		Velocity = Velocity.SafeNormal() * MinFlySpeed;

	Iterations++;
	FVector OldLocation = Location;
	bJustTeleported = 0;
	FVector ZoneVel;
	if ( IsHumanControlled() || (PhysicsVolume->ZoneVelocity.SizeSquared() > 90000) )
		ZoneVel = PhysicsVolume->ZoneVelocity;
	else
		ZoneVel = FVector(0,0,0);
	FVector Adjusted = (Velocity + ZoneVel) * deltaTime; 
	FCheckResult Hit(1.f);
	GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
	if (Hit.Time < 1.f) 
	{
		Floor = Hit.Normal;
		FVector GravDir = FVector(0,0,-1);
		if (PhysicsVolume->Gravity.Z > 0)
			GravDir.Z = 1;
		FVector DesiredDir = Adjusted.SafeNormal();
		FVector VelDir = Velocity.SafeNormal();
		FLOAT UpDown = GravDir | VelDir;
		if ( (Abs(Hit.Normal.Z) < 0.2f) && (UpDown < 0.5f) && (UpDown > -0.2f) )
		{
			FLOAT stepZ = Location.Z;
			stepUp(GravDir, DesiredDir, Adjusted * (1.f - Hit.Time), Hit);
			OldLocation.Z = Location.Z + (OldLocation.Z - stepZ);
		}
		else
		{
			processHitWall(Hit.Normal, Hit.Actor);
			//adjust and try again
			FVector OldHitNormal = Hit.Normal;
			FVector Delta = (Adjusted - Hit.Normal * (Adjusted | Hit.Normal)) * (1.f - Hit.Time);
			if( (Delta | Adjusted) >= 0 )
			{
				GetLevel()->MoveActor(this, Delta, Rotation, Hit);
				if (Hit.Time < 1.f) //hit second wall
				{
					processHitWall(Hit.Normal, Hit.Actor);
					TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
					GetLevel()->MoveActor(this, Delta, Rotation, Hit);
				}
			}
		}
	}
	else
		Floor = FVector(0.f,0.f,1.f);

	if ( !bJustTeleported && !bNoVelocityUpdate )
		Velocity = (Location - OldLocation) / deltaTime;
	bNoVelocityUpdate = 0;
	unguard;
}

/* Swimming uses gravity - but scaled by (mass - buoyancy)/mass
This is used only by pawns 
*/
FLOAT APawn::Swim(FVector Delta, FCheckResult &Hit)
{
	guard(APawn::Swim);
	FVector Start = Location;
	FLOAT airTime = 0.f;
	GetLevel()->MoveActor(this, Delta, Rotation, Hit);

	if ( !PhysicsVolume->bWaterVolume ) //then left water
	{
		FVector End = findWaterLine(Start, Location);
		if (End != Location)
		{
			airTime = (End - Location).Size()/Delta.Size();
			if ( ((Location - Start) | (End - Location)) > 0.f )
				airTime = 0.f;
			GetLevel()->MoveActor(this, End - Location, Rotation, Hit);
		}
	}
	return airTime;
	unguard;
}

//get as close to waterline as possible, staying on same side as currently
FVector APawn::findWaterLine(FVector InWater, FVector OutofWater)
{
	guard(APawn::findWaterLine);

	FCheckResult Hit(1.f);
	FMemMark Mark(GMem);
	FCheckResult* FirstHit = GetLevel()->MultiLineCheck
	(
		GMem,
		InWater,
		OutofWater,
		FVector(0,0,0),
		Level,
		TRACE_Volumes | TRACE_World,
		this
	);

	// Skip owned actors and return the one nearest actor.
	for( FCheckResult* Check = FirstHit; Check!=NULL; Check=Check->GetNext() )
	{
		if( !IsOwnedBy( Check->Actor ) )
		{
			if( Check->Actor->bWorldGeometry )
			{
				Mark.Pop();
				return OutofWater;		// never hit a water volume
			}
			else 
			{
				APhysicsVolume *W = Cast<APhysicsVolume>(Check->Actor);
				if ( W && W->bWaterVolume )
				{
					FVector Dir = InWater - OutofWater;
					Dir = Dir.SafeNormal();
					FVector Result = Check->Location;
					if ( W == PhysicsVolume )
						Result += 0.1f * Dir;
					else
						Result -= 0.1f * Dir;
					Mark.Pop();
					return Result;
				}
			}
		}
	}
	Mark.Pop();
	return OutofWater;
	unguard;
}

/*
GetNetBuoyancy()
determine how deep in water actor is standing:
0 = not in water,
1 = fully in water
*/
void AActor::GetNetBuoyancy(FLOAT &NetBuoyancy, FLOAT &NetFluidFriction)
{
	guard(AActor::GetNetBuoyancy);

	APhysicsVolume *WaterVolume = NULL;
	FLOAT depth = 0.f;

	if ( PhysicsVolume->bWaterVolume )
	{
		WaterVolume = PhysicsVolume;
		if ( (CollisionHeight == 0.f) || (Buoyancy == 0.f) )
			depth = 1.f;
		else
		{
			FCheckResult Hit(1.f);
			if ( PhysicsVolume->Brush )
				PhysicsVolume->Brush->LineCheck(Hit,PhysicsVolume,
										Location - FVector(0.f,0.f,CollisionHeight),
										Location + FVector(0.f,0.f,CollisionHeight),
										FVector(0.f,0.f,0.f),
										0, 0);
			if ( Hit.Time == 1.f )
				depth = 1.f;
			else
				depth = (1.f - Hit.Time);
		}
	}
	if ( WaterVolume )
	{
		NetBuoyancy = Buoyancy * depth;
		NetFluidFriction = WaterVolume->FluidFriction * depth;
	}
	else if ( PhysicsVolume->Gravity.Z != Level->GetDefaultPhysicsVolume()->Gravity.Z )// FIXME THIS IS A BUG LEFT IN TO KEEP FALLING BEHAVIOUR CONSISTENT WITH PREVIOUS VERSIONS
	{
		NetBuoyancy = 0.5f * Buoyancy;
		NetFluidFriction = 0.5f * PhysicsVolume->FluidFriction;
	}
	unguard;
}

/* 
Encompasses()
returns true if point is within the volume
*/
INT AVolume::Encompasses(FVector point)
{
	guard(AVolume::Encompasses);

	if ( !Brush )
		return 0;
	FCheckResult Hit(1.f);
//	debugf(TEXT("%s brush pointcheck %d at %f %f %f"),GetName(),!Brush->PointCheck(Hit,this,	point, FVector(0.f,0.f,0.f), 0), point.X, point.Y,point.Z);
	return !Brush->PointCheck(Hit,this,	point, FVector(0.f,0.f,0.f), 0); 
	unguard;
}

void APawn::physSwimming(FLOAT deltaTime, INT Iterations)
{
	guard(APawn::physSwimming);

	FLOAT NetBuoyancy = 0.f;
	FLOAT NetFluidFriction  = 0.f;
	GetNetBuoyancy(NetBuoyancy, NetFluidFriction);
	if ( (Velocity.Z > 100.f) && (Buoyancy != 0.f) )
	{
		//damp positive Z out of water
		Velocity.Z = Velocity.Z * NetBuoyancy/Buoyancy;
	}

	Iterations++;
	FVector OldLocation = Location;
	bJustTeleported = 0;
	FVector AccelDir;
	if ( Acceleration.IsZero() )
		AccelDir = Acceleration;
	else
		AccelDir = Acceleration.SafeNormal();
	calcVelocity(AccelDir, deltaTime, WaterSpeed, 0.5f * PhysicsVolume->FluidFriction, 1, 0, 1);  
	FLOAT velZ = Velocity.Z;
	FVector ZoneVel;
	if ( IsHumanControlled() || (PhysicsVolume->ZoneVelocity.SizeSquared() > 90000) )
	{
		// Add effect of velocity zone
		// Rather than constant velocity, hacked to make sure that velocity being clamped when swimming doesn't 
		// cause the zone velocity to have too much of an effect at fast frame rates

		ZoneVel = PhysicsVolume->ZoneVelocity * 25 * deltaTime;
	}
	else
		ZoneVel = FVector(0,0,0);
	FVector Adjusted = (Velocity + ZoneVel) * deltaTime; 
	FCheckResult Hit(1.f);
	FLOAT remainingTime = deltaTime * Swim(Adjusted, Hit);

	if (Hit.Time < 1.f)
	{
		Floor = Hit.Normal;
		FVector GravDir = FVector(0,0,-1);
		if (PhysicsVolume->Gravity.Z > 0)
			GravDir.Z = 1;
		FVector DesiredDir = Adjusted.SafeNormal();
		FVector VelDir = Velocity.SafeNormal();
		FLOAT UpDown = GravDir | VelDir;
		if ( (Abs(Hit.Normal.Z) < 0.2f) && (UpDown < 0.5f) && (UpDown > -0.2f) )
		{
			FLOAT stepZ = Location.Z;
			bWaterStepup = true;
			stepUp(GravDir, DesiredDir, Adjusted * (1.f - Hit.Time), Hit);
			bWaterStepup = false;
			OldLocation.Z = Location.Z + (OldLocation.Z - stepZ);
		}
		else
		{
			processHitWall(Hit.Normal, Hit.Actor);
			//adjust and try again
			FVector OldHitNormal = Hit.Normal;
			FVector Delta = (Adjusted - Hit.Normal * (Adjusted | Hit.Normal)) * (1.f - Hit.Time);
			if( (Delta | Adjusted) >= 0 )
			{
				remainingTime = remainingTime * (1.f - Hit.Time) * Swim(Delta, Hit);
				if(Hit.Time < 1.f) //hit second wall
				{
					processHitWall(Hit.Normal, Hit.Actor);
					TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
					remainingTime = remainingTime * (1.f - Hit.Time) * Swim(Delta, Hit);
				}
			}
		}
	}
	else 
		Floor = FVector(0.f,0.f,1.f);

	if (!bJustTeleported && (remainingTime < deltaTime))
	{
		int bWaterJump = !PhysicsVolume->bWaterVolume; 
		if (bWaterJump)
			velZ = Velocity.Z;
		if ( !bNoVelocityUpdate )
			Velocity = (Location - OldLocation) / (deltaTime - remainingTime);
		bNoVelocityUpdate = 0;
		if (bWaterJump)
			Velocity.Z = velZ;
	}

	if ( !PhysicsVolume->bWaterVolume )
	{
		if (Physics == PHYS_Swimming)
			setPhysics(PHYS_Falling); //in case script didn't change it (w/ zone change)
		if ((Velocity.Z < 160.f) && (Velocity.Z > 0)) //allow for falling out of water
			Velocity.Z = 40.f + Velocity.Size2D() * 0.4; //smooth bobbing
	}

	//may have left water - if so, script might have set new physics mode
	if ( Physics != PHYS_Swimming )
		startNewPhysics(remainingTime, Iterations);

	unguard;
}
/* PhysWalking for Actors is a fast mini version of walking physics
 * it doesn't emulate friction, a max speed, or anything
 * all it does is force the object to stick to the ground and climb up/down small steps
 */
void AActor::physWalking(FLOAT deltaTime, INT Iterations)
{
	guard(AActor::physWalking);

	//no z-axis movement while walking
	Acceleration.Z = 0;
	Velocity.Z = 0;

	//do the move
	Velocity += Acceleration * deltaTime;
	FVector Delta = Velocity * deltaTime;
	moveSmooth(Delta);

	//check floor
	FVector GravDir = (PhysicsVolume->Gravity.Z > 0.f) ? FVector(0.f,0.f,1.f) : FVector(0.f,0.f,-1.f);
	FVector Down = GravDir * (UCONST_MAXSTEPHEIGHT + 2.f);
	FCheckResult Hit(1.f);
	GetLevel()->SingleLineCheck(Hit, this, Location + Down, Location, TRACE_AllBlocking, GetCylinderExtent());
	FLOAT FloorDist = Hit.Time * (UCONST_MAXSTEPHEIGHT + 2.f);
	if ( (Hit.Normal.Z < UCONST_MINFLOORZ) && !Delta.IsNearlyZero() && ((Delta | Hit.Normal) < 0) )
	{
		// slide down slope
		FVector Slide = FVector(0.f,0.f,UCONST_MAXSTEPHEIGHT) - Hit.Normal * (FVector(0.f,0.f,UCONST_MAXSTEPHEIGHT) | Hit.Normal);
		GetLevel()->MoveActor(this, -1 * Slide, Rotation, Hit); 
		if ( (Hit.Actor != Base) && (Physics == PHYS_Walking) )
			SetBase(Hit.Actor, Hit.Normal);
	}
	else if( Hit.Time< 1.f && (Hit.Actor!=Base || FloorDist>MAXFLOORDIST) )
	{
		// move down to correct position 
		GetLevel()->MoveActor(this, Down, Rotation, Hit);
		if ( (Hit.Actor != Base) && (Physics == PHYS_Walking) )
			SetBase(Hit.Actor, Hit.Normal);
	}
	else if ( FloorDist < MINFLOORDIST )
	{
		// move up to correct position (average of MAXFLOORDIST and MINFLOORDIST above floor)
		GetLevel()->MoveActor(this, FVector(0.f,0.f,0.5f*(MINFLOORDIST+MAXFLOORDIST) - FloorDist), Rotation, Hit);
	}
	else if (Hit.Time >= 1.f || Hit.Normal.Z < UCONST_MINFLOORZ)
	{
		// fall
		eventFalling();
		if (Physics == PHYS_Walking)
			setPhysics(PHYS_Falling); //default if script didn't change physics
	}

	unguard;
}
/* Simple PhysFlying for Actor
 */
void AActor::physFlying(FLOAT deltaTime, INT Iterations)
{
	guard(AActor::physFlying);

	if ( Location.Z < (Region.Zone->bSoftKillZ ? Region.Zone->KillZ - 2500.f : Region.Zone->KillZ) )
	{
		eventFellOutOfWorld(Region.Zone->KillZType);
		return;
	}

	FLOAT remainingTime = deltaTime;
	INT numBounces = 0;

	if ( (Region.ZoneNumber == 0) && !bIgnoreOutOfWorld )
	{
		GetLevel()->DestroyActor( this );
		return;
	}

	bJustTeleported = 0;
	FCheckResult Hit(1.f);

	while ( remainingTime > 0.f )
	{
		Iterations++;
		if ( !Acceleration.IsZero() )
		{
			//debugf(TEXT("%s has acceleration!"),GetName());
			Velocity = Velocity	+ Acceleration * remainingTime;
		}

		FLOAT timeTick = remainingTime;
		remainingTime = 0.f;
		FVector Adjusted = Velocity * deltaTime;
		FVector StartLocation = Location;
		Hit.Time = 1.f;
		GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
		
		if( Hit.Time<1.f && !bDeleteMe && !bJustTeleported )
		{
			if ( ShrinkCollision(Hit.Actor, StartLocation) )
				remainingTime = timeTick * (1.f - Hit.Time);
			else
			{
				eventHitWall(Hit.Normal, Hit.Actor);
				if (bBounce)
				{
					if (numBounces < 2)
						remainingTime = timeTick * (1.f - Hit.Time);
					numBounces++;
					if (Physics == PHYS_Falling)
						physFalling(remainingTime, Iterations);
				}
			}
		}
	}
	unguard;
}
/* PhysProjectile is tailored for projectiles 
*/
void AActor::physProjectile(FLOAT deltaTime, INT Iterations)
{
	guard(AActor::physProjectile);

	if ( Location.Z < (Region.Zone->bSoftKillZ ? Region.Zone->KillZ - 2500.f : Region.Zone->KillZ) )
	{
		eventFellOutOfWorld(Region.Zone->KillZType);
		return;
	}

	FLOAT remainingTime = deltaTime;
	INT numBounces = 0;

	if ( (Region.ZoneNumber == 0) && !bIgnoreOutOfWorld )
	{
		GetLevel()->DestroyActor( this );
		return;
	}

	bJustTeleported = 0;
	FCheckResult Hit(1.f);
	if ( bCollideActors )
	{
		AProjectile *Proj = Cast<AProjectile>(this);
		if ( Proj && Proj->ZeroCollider )
		{
			UBOOL bStillTouching = false;
			for ( INT i=0; i<Touching.Num(); i++ )
				if ( Touching(i) == Proj->ZeroCollider )
				{
					bStillTouching = true;
					break;
				}
			if ( !bStillTouching )
			{
				Proj->ZeroCollider = NULL;
				SetCollisionSize(GetClass()->GetDefaultActor()->CollisionRadius,GetClass()->GetDefaultActor()->CollisionHeight);
			}

		}
	}

	while ( remainingTime > 0.f )
	{
		Iterations++;
		if ( !Acceleration.IsZero() )
		{
			//debugf(TEXT("%s has acceleration!"),GetName());
			Velocity = Velocity	+ Acceleration * remainingTime;
			BoundProjectileVelocity();
		}

		FLOAT timeTick = remainingTime;
		remainingTime = 0.f;
		FVector Adjusted = Velocity * deltaTime; 
		Hit.Time = 1.f;
		FVector StartLocation = Location;
		GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
		
		if( Hit.Time<1.f && !bDeleteMe && !bJustTeleported )
		{
			if ( ShrinkCollision(Hit.Actor, StartLocation) )
				remainingTime = timeTick * (1.f - Hit.Time);
			else
			{
				eventHitWall(Hit.Normal, Hit.Actor);
				if (bBounce)
				{
					if (numBounces < 2)
						remainingTime = timeTick * (1.f - Hit.Time);
					numBounces++;
					if (Physics == PHYS_Falling)
						physFalling(remainingTime, Iterations);
				}
			}
		}
	}
	unguard;
}

void AActor::BoundProjectileVelocity()
{
	guard(AActor::BoundProjectileVelocity);

	if ( !Acceleration.IsZero() && (Velocity.SizeSquared() > Acceleration.SizeSquared()) )
	{
		Velocity = Velocity.SafeNormal();
		Velocity *= Acceleration.Size();
	}
	unguard;
}
void AProjectile::BoundProjectileVelocity()
{
	guard(AProjectile::BoundProjectileVelocity);

	if ( Velocity.SizeSquared() > MaxSpeed * MaxSpeed )
	{
		Velocity = Velocity.SafeNormal();
		Velocity *= MaxSpeed;
	}
	unguard;
}

/*
Move only in ClimbDir or -1 * ClimbDir, but also push into LookDir.
If leave ladder volume, then step pawn up onto ledge.
If hit ground, then change to walking
*/
void APawn::physLadder(FLOAT deltaTime, INT Iterations)
{
	guard(APawn::physLadder);

	Iterations++;
	FLOAT remainingTime = deltaTime;
	ALadderVolume *OldLadder = OnLadder;
	Velocity = FVector(0.f,0.f,0.f);

	if ( OnLadder && Controller && !Acceleration.IsZero() )
	{
		FCheckResult Hit(1.f);
		UBOOL bClimbUp = ((Acceleration | (OnLadder->ClimbDir + OnLadder->LookDir)) > 0.f);
		// First, push into ladder
		if ( !OnLadder->bNoPhysicalLadder && bClimbUp )
		{
			Velocity = OnLadder->LookDir * GroundSpeed;
			GetLevel()->MoveActor(this, OnLadder->LookDir * remainingTime * GroundSpeed, Rotation, Hit); 
			remainingTime = remainingTime * (1.f - Hit.Time);
			if ( !OnLadder )
			{
				if ( PhysicsVolume->bWaterVolume )
					setPhysics(PHYS_Swimming);
				else
					setPhysics(PHYS_Walking);
				startNewPhysics(remainingTime, Iterations);
				return;
			}
			if ( remainingTime == 0.f )
				return;
		}
		FVector AccelDir = Acceleration.SafeNormal();
		Velocity = FVector(0.f,0.f,0.f);

		// set up or down movement velocity
		if ( !OnLadder->bAllowLadderStrafing || (Abs(AccelDir | OnLadder->ClimbDir) > 0.1f) )
			Velocity = OnLadder->ClimbDir * LadderSpeed;
		if ( !bClimbUp )
			Velocity *= -1.f;

		// support moving sideways on ladder
		if ( OnLadder->bAllowLadderStrafing )
		{
			FVector LeftDir = OnLadder->LookDir ^ OnLadder->ClimbDir;
			LeftDir = LeftDir.SafeNormal();
			Velocity += (LeftDir | AccelDir) * LeftDir * LadderSpeed;
		}

		FVector MoveDir = Velocity * remainingTime;

		// move along ladder
		GetLevel()->MoveActor(this, MoveDir, Rotation, Hit);
		remainingTime = remainingTime * (1.f - Hit.Time);

		if ( !OnLadder )
		{
			//Moved out of ladder, try to step onto ledge
			if ( (MoveDir | PhysicsVolume->Gravity) > 0.f )
			{
				setPhysics(PHYS_Falling);
				return;
			}
			FVector Out = MoveDir.SafeNormal();
			Out *= 1.1f * CollisionHeight;
			GetLevel()->MoveActor(this, Out, Rotation, Hit);
			GetLevel()->MoveActor(this, 0.5f * OldLadder->LookDir * CollisionRadius, Rotation, Hit);
			GetLevel()->MoveActor(this, -1.f * (Out + MoveDir), Rotation, Hit);
			GetLevel()->MoveActor(this, (-0.5f * CollisionRadius + 3.f) * OldLadder->LookDir , Rotation, Hit);
			Velocity = FVector(0,0,0);
			if ( PhysicsVolume->bWaterVolume )
				setPhysics(PHYS_Swimming);
			else
				setPhysics(PHYS_Walking);
			startNewPhysics(remainingTime, Iterations);
			return;
		}	
		else if ( (Hit.Time < 1.f) && Hit.Actor->bWorldGeometry )
		{
			// hit ground
			FVector OldLocation = Location;
			MoveDir = OnLadder->LookDir * GroundSpeed * remainingTime;
			if ( !bClimbUp )
				MoveDir *= -1.f;

			// try to move along ground
			GetLevel()->MoveActor(this, MoveDir, Rotation, Hit);
			if ( Hit.Time < 1.f )
			{
				FVector GravDir = FVector(0,0,-1);
				if (PhysicsVolume->Gravity.Z > 0)
					GravDir.Z = 1;
				FVector DesiredDir = MoveDir.SafeNormal();
				stepUp(GravDir, DesiredDir, MoveDir, Hit);
				if ( OnLadder && (Physics != PHYS_Ladder) )
					setPhysics(PHYS_Ladder);
			}
			Velocity = (Location - OldLocation)/remainingTime;
		}
		else if ( !OnLadder->bNoPhysicalLadder && !bClimbUp )
		{
			FVector ClimbDir = OnLadder->ClimbDir;
			FVector PushDir = OnLadder->LookDir;
			GetLevel()->MoveActor(this, -1.f * ClimbDir * UCONST_MAXSTEPHEIGHT, Rotation, Hit); 
			FLOAT Dist = Hit.Time * UCONST_MAXSTEPHEIGHT;
			if ( Hit.Time == 1.f )
				GetLevel()->MoveActor(this, PushDir * deltaTime * GroundSpeed, Rotation, Hit); 
			GetLevel()->MoveActor(this, ClimbDir * Dist, Rotation, Hit); 
			if ( !OnLadder )
			{
				if ( PhysicsVolume->bWaterVolume )
					setPhysics(PHYS_Swimming);
				else
					setPhysics(PHYS_Walking);
			}
		}
	}
	
	if ( !Controller )
		setPhysics(PHYS_Falling);

	unguard;
}

/*
physSpider()

*/
#ifdef __GNUG__
int APawn::checkFloor(FVector Dir, FCheckResult &Hit)
#else
inline int APawn::checkFloor(FVector Dir, FCheckResult &Hit)
#endif
{
	GetLevel()->SingleLineCheck(Hit, 0, Location - UCONST_MAXSTEPHEIGHT * Dir, Location, TRACE_World, GetCylinderExtent());
	if (Hit.Time < 1.f)
	{
		SetBase(Hit.Actor, Hit.Normal);
		return 1;
	}
	return 0;
}

/* findNewFloor()
Helper function used by PHYS_Spider for determining what wall or floor to crawl on
*/
int APawn::findNewFloor(FVector OldLocation, FLOAT deltaTime, FLOAT remainingTime, int Iterations)
{
	guard(APawn::findNewFloor);

	//look for floor
	FCheckResult Hit(1.f);
	//debugf("Find new floor for %s", GetFullName());
	if ( checkFloor(FVector(0,0,1), Hit) )
		return 1;
	if ( checkFloor(FVector(0,1,0), Hit) )
		return 1;
	if ( checkFloor(FVector(0,-1,0), Hit) )
		return 1;
	if ( checkFloor(FVector(1,0,0), Hit) )
		return 1;
	if ( checkFloor(FVector(-1,0,0), Hit) )
		return 1;
	if ( checkFloor(FVector(0,0,-1), Hit) )
		return 1;

	// Fall
	eventFalling();
	if (Physics == PHYS_Spider)
		setPhysics(PHYS_Falling); //default if script didn't change physics
	if (Physics == PHYS_Falling)
	{
		FLOAT velZ = Velocity.Z;
		if (!bJustTeleported && (deltaTime > remainingTime))
			Velocity = (Location - OldLocation)/(deltaTime - remainingTime);
		Velocity.Z = velZ;
		if (remainingTime > 0.005f)
			physFalling(remainingTime, Iterations);
	}

	return 0;

	unguard;
}

void APawn::SpiderstepUp(FVector DesiredDir, FVector Delta, FCheckResult &Hit)
{
	guard(APawn::SpiderstepUp);

	FVector Down = -1.f * Floor * UCONST_MAXSTEPHEIGHT;

	if ( (Floor | Hit.Normal) < 0.1 )
	{
		// step up - treat as vertical wall 
		GetLevel()->MoveActor(this, -1 * Down, Rotation, Hit); 
		GetLevel()->MoveActor(this, Delta, Rotation, Hit);
	}
	else // walk up slope
	{
		Floor = Hit.Normal;
		Down = -1.f * Floor * UCONST_MAXSTEPHEIGHT;
		FLOAT Dist = Delta.Size();
		GetLevel()->MoveActor(this, Delta + FVector(0,0,Dist*Hit.Normal.Z), Rotation, Hit); 
	}

	if (Hit.Time < 1.f)
	{
		if ( ((Floor | Hit.Normal) < 0.1) && (Hit.Time * Delta.SizeSquared() > 144.f) )
		{
			// try another step
			GetLevel()->MoveActor(this, Down, Rotation, Hit);
			SpiderstepUp(DesiredDir, Delta * (1 - Hit.Time), Hit);
			return;
		}

		// Found a new floor
		FVector OldFloor = Floor;
		Floor = Hit.Normal;
		Down = -1.f * Floor * UCONST_MAXSTEPHEIGHT;

		//adjust and try again
		Hit.Normal.Z = 0;	// treat barrier as vertical;
		Hit.Normal = Hit.Normal.SafeNormal();
		FVector OriginalDelta = Delta;
		FVector OldHitNormal = Hit.Normal;

		FVector CrossY = Floor ^ OldFloor;
		CrossY.Normalize();
		FVector VecX = CrossY ^ OldFloor;
		VecX.Normalize();
		FLOAT X = VecX | Delta;
		FLOAT Y = CrossY | Delta;
		FLOAT Z = OldFloor | Delta;
		VecX = CrossY ^ Floor;
		Delta = X * VecX + Y * CrossY + Z * Floor;

		if( (Delta | OriginalDelta) >= 0 )
		{
			GetLevel()->MoveActor(this, Delta, Rotation, Hit);
			if (Hit.Time < 1.f)
			{
				processHitWall(Hit.Normal, Hit.Actor);
				if ( Physics == PHYS_Falling )
					return;
				TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
				GetLevel()->MoveActor(this, Delta, Rotation, Hit);
			}
		}
	}
	GetLevel()->MoveActor(this, Down, Rotation, Hit);
	unguard;
}

void APawn::physSpider(FLOAT deltaTime, INT Iterations)
{
	guard(APawn::physSpider);

	if ( !Controller )
		return;
	if ( Floor.IsNearlyZero() && !findNewFloor(Location, deltaTime, deltaTime, Iterations) )
		return;

	// calculate velocity
	FVector AccelDir;
	if ( Acceleration.IsZero() ) 
	{
		AccelDir = Acceleration;
		FVector OldVel = Velocity;
		Velocity = Velocity - (2 * Velocity) * deltaTime * PhysicsVolume->GroundFriction; //don't drift to a stop, brake
		if ((OldVel | Velocity) < 0.f) //brake to a stop, not backwards
			Velocity = Acceleration;
	}
	else
	{
		AccelDir = Acceleration.SafeNormal();
		FLOAT VelSize = Velocity.Size();
		if (Acceleration.SizeSquared() > AccelRate * AccelRate)
			Acceleration = AccelDir * AccelRate;
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * deltaTime * PhysicsVolume->GroundFriction;  
	}

	Velocity = Velocity + Acceleration * deltaTime;
	// only move along plane of floor
	Velocity = Velocity - Floor * (Floor | Velocity);

	FLOAT maxSpeed = GroundSpeed * DesiredSpeed;
	Iterations++;

	if (Velocity.SizeSquared() > maxSpeed * maxSpeed)
	{
		Velocity = Velocity.SafeNormal();
		Velocity *= maxSpeed;
	}
	FVector DesiredMove = Velocity;

	//-------------------------------------------------------------------------------------------
	//Perform the move
	FCheckResult Hit(1.f);
	FVector OldLocation = Location;
	bJustTeleported = 0;

	FLOAT remainingTime = deltaTime;
	FLOAT timeTick;
	FLOAT MaxStepHeightSq = UCONST_MAXSTEPHEIGHT * UCONST_MAXSTEPHEIGHT;
	while ( (remainingTime > 0.f) && (Iterations < 8) )
	{
		Iterations++;
		// subdivide moves to be no longer than 0.05 seconds for players, or no longer than the collision radius for non-players
		if ( (remainingTime > 0.05f) && (IsHumanControlled() ||
			(DesiredMove.SizeSquared() * remainingTime * remainingTime > Min(CollisionRadius * CollisionRadius, MaxStepHeightSq))) )
				timeTick = Min(0.05f, remainingTime * 0.5f);
		else timeTick = remainingTime;
		remainingTime -= timeTick;
		FVector Delta = timeTick * DesiredMove;
		FVector subLoc = Location;
		int bZeroMove = Delta.IsNearlyZero();

		if ( bZeroMove )
		{
			remainingTime = 0;
			// if not moving, quick check if still on valid floor
			FVector Foot = Location - CollisionHeight * Floor;
			GetLevel()->SingleLineCheck( Hit, this, Foot - 20 * Floor, Foot, TRACE_World );
			FLOAT FloorDist = Hit.Time * 20;
			bZeroMove = ((Base == Hit.Actor) && (FloorDist <= MAXFLOORDIST + CYLINDERREPULSION) && (FloorDist >= MINFLOORDIST + CYLINDERREPULSION));
		}
		else
		{
			// try to move forward
			GetLevel()->MoveActor(this, Delta, Rotation, Hit);
			if (Hit.Time < 1.f) 
			{
				// hit a barrier, try to step up
				FVector DesiredDir = Delta.SafeNormal();
				SpiderstepUp(DesiredDir, Delta * (1.f - Hit.Time), Hit);
			}

			if ( Physics == PHYS_Swimming ) //just entered water
			{
				startSwimming(OldLocation, Velocity, timeTick, remainingTime, Iterations);
				return;
			}
		}

		if ( !bZeroMove )
		{
			//drop to floor
			GetLevel()->SingleLineCheck( Hit, this, Location - Floor * (UCONST_MAXSTEPHEIGHT + 2.f), Location, TRACE_AllBlocking, GetCylinderExtent() );
			if ( Hit.Time == 1.f )
			{
				GetLevel()->MoveActor(this, -8.f * Floor, Rotation, Hit);
				// find new floor or fall
				if ( !findNewFloor(Location, deltaTime, deltaTime, Iterations) )
					return;
			}
			else
			{
				Floor = Hit.Normal;
				GetLevel()->MoveActor(this, -1.f * Floor * (UCONST_MAXSTEPHEIGHT + 2.f), Rotation, Hit);
				if ( Hit.Actor != Base )
					SetBase(Hit.Actor, Hit.Normal);
			}
		}
	}

	// make velocity reflect actual move
	if (!bJustTeleported)
		Velocity = (Location - OldLocation) / deltaTime;
 	unguard;
}

void AActor::physTrailer(FLOAT deltaTime)
{
	guard(AActor::physTrailer);

	FRotator trailRot;

	if ( !Owner )
		return;

	// Don't perform PHYS_Trailer on attachments.
	if( Base ) 
		return;

	if ( DrawType == DT_Sprite )
	{
		if ( bTrailerPrePivot )
			GetLevel()->FarMoveActor(this, Owner->Location + PrePivot, 0, 1);
		else if (bTrailerSameRotation )
			GetLevel()->FarMoveActor(this, Owner->Location - Mass * Owner->Rotation.Vector(), 0, 1);
		else
			GetLevel()->FarMoveActor(this, Owner->Location, 0, 1);
		return;
	}
	GetLevel()->FarMoveActor(this, Owner->Location, 0, 1);
	FCheckResult Hit(1.f);

    if (bTrailerAllowRotation)
        return;

	if ( bTrailerSameRotation )
		trailRot = Owner->Rotation;
	else if ( Owner->Velocity.IsNearlyZero() )
		trailRot = FRotator(16384,0,0);
	else
		trailRot = (-1 * Owner->Velocity).Rotation();

	GetLevel()->MoveActor(this, FVector(0,0,0), trailRot, Hit);
	unguard;
}

void AActor::physRootMotion(FLOAT deltaTime)
{
	guard(AActor::physRootMotion);

	USkeletalMesh *SkM = Cast<USkeletalMesh>(Mesh);
	if ( !SkM )
	{
		Velocity = FVector(0,0,0);
		Acceleration = Velocity;
		return;
	}
	SkM->MeshGetInstance(this);
	USkeletalMeshInstance *SkMInstance = Cast<USkeletalMeshInstance>(MeshInstance);

	if ( !SkMInstance )
	{
		Velocity = FVector(0,0,0);
		Acceleration = Velocity;
		return;
	}
	FVector OldLocation = Location;
			
	if( Physics == PHYS_RootMotion ) // Root-motion driven physical motion.
	{
		INT OldbCollideWorld = bCollideWorld;
		bCollideWorld = 0;
		FVector LocationDelta = SkMInstance->GetRootLocationDelta();

		// Check against NaN #SKEL
		if( LocationDelta.X == LocationDelta.X && LocationDelta.Y == LocationDelta.Y && LocationDelta.Z == LocationDelta.Z )
			GetLevel()->FarMoveActor(this, Location + LocationDelta, false, true);
		bCollideWorld = OldbCollideWorld;
	
		if (!bJustTeleported)
		Velocity = (Location - OldLocation) / deltaTime;
	}
	else if (Physics == PHYS_CinMotion ) // Pure cinematics, oblivious to collision.
	{
		INT OldbCollideWorld = bCollideWorld;
		bCollideWorld = false; // Force non-collision for PHYS_CinMotion.
		bCollideActors = false;
		bBlockActors = false;
		
		FVector LocationDelta = SkMInstance->GetRootLocationDelta();
		//debugf(TEXT("#Cin. Actor     on: %f %f %f   GTicks %i Actor %s "), Location.X, Location.Y, Location.Z, GTicks, this->GetName() ); //#SKEL
		//debugf(TEXT("#Cin. GetRootLocationDelta: %f %f %f "),LocationDelta.X, LocationDelta.Y, LocationDelta.Z ); //#SKEL
		//debugf(TEXT("#Cin. MoveActor to: %f %f %f"), Location.X+LocationDelta.X, Location.Y+LocationDelta.Y, Location.Z+LocationDelta.Z );//#SKEL				 

		// Check against NaN #SKEL
		if( (LocationDelta.X == LocationDelta.X) && (LocationDelta.Y == LocationDelta.Y) && (LocationDelta.Z == LocationDelta.Z) )
			GetLevel()->FarMoveActor( this, (Location + LocationDelta), false, true, false); // , false, true ? //#SKEL			
		else
		{
			if( Mesh ) debugf(TEXT("Invalid GetRootLocationDelta vector for Actor %s Mesh %s"),this->GetName(), Mesh->GetName()); //#SKEL
		}

		bCollideWorld = OldbCollideWorld;

		Velocity = FVector(0,0,0); //#SKEL

		// No speed update for CinMotion ?
        // if (!bJustTeleported)
		// Velocity = (Location - OldLocation) / deltaTime;

	}	


	unguard;
}

// sjs ---
void OneWaySpring( FVector& rForce, float len, float clampRatio, FVector deltaV, float restLen, float& rhTerm )
{
	FVector deltaP(0,0,len);
	float	Dterm;
	float	Hterm;

	float inKs = 2.0f;
	float inKd = 0.04f;

	rForce = FVector(0,0,0);

	len = Abs(len);
	if ( len < 0.00001f )
		return;

	if ( len > ( restLen * clampRatio )) // hmm??
		len = restLen * clampRatio;

	Hterm = ( len - restLen ) * inKs;			// spring force
	Dterm = ((deltaV | deltaP) * inKd) / len;	// dampening
	
	rForce = deltaP.SafeNormal();
	rForce *= -(Hterm + Dterm);				// resultant spring force
	rForce *= 2.0f; // one-way extertion

	rhTerm = Hterm;
}
// sjs ---
// Hovering physics
// todo:	- cleanup	
//			- attenuate lift by surface normal
//			- fix rolling characteristics
//			- more property driven
void APawn::physHovering(FLOAT deltaTime, INT Iterations)
{
	guard(APawn::physHovering);

	FVector AccelDir;

	if ( bCollideWorld && (Region.ZoneNumber == 0) )
	{
		// not in valid spot
		if ( !Controller || !Controller->bIsPlayer )
		{
			debugf( TEXT("%s flew out of the world!"), GetName());
			GetLevel()->DestroyActor( this );
		}
		return;
	}

	if ( Acceleration.IsZero() )
		AccelDir = Acceleration;
	else
		AccelDir = Acceleration.SafeNormal();
	
	bIsWalking = 0;

#if 0
	calcVelocity(AccelDir, deltaTime, AirSpeed, PhysicsVolume->FluidFriction * 0.5f, 0, 0, 0);  
#else
	// calc velocity differently
	UBOOL bFluid = 1;

	float accelSize = Acceleration.Size();
	if ( accelSize > AccelRate )
		Acceleration = AccelDir * AccelRate;

	//Acceleration = AccelDir * ( AirSpeed / 2.0f );


	float friction = PhysicsVolume->FluidFriction * 0.5f;
	float effectiveFriction = ::Max((FLOAT)bFluid,friction); 
	float VelSize = Velocity.Size();

	if ( AccelDir.IsNearlyZero() )
	{
		Velocity = Velocity - (Velocity - AccelDir * VelSize) * deltaTime * effectiveFriction;
	}
	
	Velocity = (Velocity * (1 - bFluid * friction * deltaTime)) + (Acceleration * deltaTime);
	
	FVector OldVelocity = Velocity;
	if (!PhysicsVolume->bWaterVolume)
	{
		Velocity += PhysicsVolume->Gravity * deltaTime; //average velocity for tick
	}
	else
	{
		Velocity = OldVelocity * (1 - 2 * PhysicsVolume->FluidFriction * deltaTime) 
				+ 0.5f * (Acceleration + PhysicsVolume->Gravity * (1.f - Buoyancy/::Max(1.f,Mass))) * deltaTime; 
	}
	//Velocity = Velocity + PhysicsVolume->Gravity * deltaTime * (1.f - Buoyancy/Mass);

	// clamp the forward velocity but not falling or repulsive (spring) induced velocity
	FVector Vel2d = Velocity;
	Vel2d.Z = 0.0f;
	float Vel2dSize = Vel2d.Size();
	if ( Vel2d.SizeSquared() > AirSpeed * AirSpeed)
	{
		Vel2d = Vel2d.SafeNormal();
		Vel2d *= AirSpeed;
		Velocity.X = Vel2d.X;
		Velocity.Y = Vel2d.Y;
	}
	// end calc velocity
#endif
    FVector OldLocation = Location; // gam
	Iterations++;
	bJustTeleported = 0;
	FVector ZoneVel;
	if ( (Controller && Controller->GetAPlayerController()) || (PhysicsVolume->ZoneVelocity.SizeSquared() > 90000) )
		ZoneVel = PhysicsVolume->ZoneVelocity;
	else
		ZoneVel = FVector(0,0,0);

	// trace down to look for hover support
	FVector hoverVect(0,0,0);
	FCheckResult Hit(1.f);

	float variance = 0.1f;
	float noise = appFrand() * variance;
	static float last[] = { 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 };
	static int curLastPos = 0;
	curLastPos = (curLastPos+1) % ARRAY_COUNT(last);
	last[curLastPos] = noise;
	noise = 0.0f;
	for ( int n=0; n<ARRAY_COUNT(last); n++ )
	{
		noise += last[n];
	}
	noise /= ARRAY_COUNT(last);
	noise += 1.0f - variance;
	
	float idealHover = CollisionHeight * 1.8f * noise;
	float hoverMult = 2.0f;
	//float gravScale = 1.0f;
	float testExtend = idealHover * hoverMult - idealHover;
	FVector a = Location - FVector(0,0,(idealHover + testExtend));
	FVector b = Location;
	//FPlane red(1,0,0,1);

	GetLevel()->SingleLineCheck(Hit, this,
		a,
		b,
		TRACE_ProjTargets);

	//GetLevel()->Engine->DebugLine( a, b, red, 1 );

	if ( !Hit.Actor )
	{
		/*
		FVector partExtent = 0.5 * GetCylinderExtent();
		partExtent.Z *= 2;
		int bQuad1 = GetLevel()->SingleLineCheck(Hit, this, Location + FVector(0.5 * CollisionRadius, 0.5 * CollisionRadius, -8),
			Location + FVector(0.5 * CollisionRadius, 0.5 * CollisionRadius, 0), TRACE_AllColliding, partExtent);
		int bQuad2 = GetLevel()->SingleLineCheck(Hit, this, Location + FVector(-0.5 * CollisionRadius, 0.5 * CollisionRadius, -8),
			Location + FVector(-0.5 * CollisionRadius, 0.5 * CollisionRadius, 0), TRACE_AllColliding, partExtent);
		int bQuad3 = GetLevel()->SingleLineCheck(Hit, this, Location + FVector(-0.5 * CollisionRadius, -0.5 * CollisionRadius, -8),
			Location + FVector(-0.5 * CollisionRadius, -0.5 * CollisionRadius, 0), TRACE_AllColliding, partExtent);
		int bQuad4 = GetLevel()->SingleLineCheck(Hit, this, Location + FVector(0.5 * CollisionRadius, -0.5 * CollisionRadius, -8),
			Location + FVector(0.5 * CollisionRadius, -0.5 * CollisionRadius, 0), TRACE_AllColliding, partExtent);
		
		if ( (bQuad1 + bQuad2 + bQuad3 + bQuad4 > 1) && !(bQuad1 + bQuad3 == 0) && !(bQuad2 + bQuad4 == 0) )
		{
			//hoverVect = 2 * Clamp( -1.f * Velocity.Z, 30.f, 30.f + CollisionRadius) * 
			//			FVector((FLOAT)(bQuad1 + bQuad4 - bQuad2 - bQuad3), (FLOAT)(bQuad1 + bQuad2 - bQuad3 - bQuad4) , 0.5);

			// here we'll pitch/roll a bit based on the trace results.
		}
		else
		{
			float dist = Hit.Location.Z - Location.Z;
			float diff = idealHover / dist;
			hoverVect = FVector(0,0,-1) * diff;
		}
		*/
	}
	else
	{
		FRotator newRot = Rotation;
		newRot = FindSlopeRotation(Hit.Normal, newRot);

		float dist = Location.Z - Hit.Location.Z;
		float hTerm;
		OneWaySpring( hoverVect, dist, 20.0f, Velocity, idealHover, hTerm );

		// reduce repulsive force based on surface normal to prevent climbing
		// impossible slopes and such

		float d = Hit.Normal | FVector(0,0,1);
		if ( d > 0.0f )
		{
			d = d*d*d;
			if ( d < 0.2f )
				d = 0.0f;
		}
		else
			hoverVect.Z = 0.0f;

		if ( Weapon )
		{
			float size = hoverVect.Size();
			float velRatio = Vel2dSize / AirSpeed;
			float pitch = 32.0f + (size * 0.3f) + ( 48.0f * velRatio );
			BYTE p = Clamp( (BYTE)pitch, (BYTE)32, (BYTE)200 );
			Weapon->SoundPitch = p;
			//debugf(TEXT(" pitch = %ed"), p);
			//float NewVelSize = Velocity.Size();
			//float diff = NewVelSize / VelSize;
			//Weapon->Rotation.Pitch = int( diff * 8192.0f );
			//Weapon->Rotation = newRot;
		}

		hoverVect.Z *= d;
		hoverVect += Hit.Normal * deltaTime * 5.0f;			

		if ( hoverVect.Z < 0.0f ) // cancel pro-gravity forces
			hoverVect.Z = 0.0f;
		/*
		float dif = idealHover / dis;
		hoverVect = PhysicsVolume->Gravity * -0.5f * dif;
		//hoverVect = PhysicsVolume->Gravity;
		*/
	}

	Velocity += hoverVect;
	// done hover vect

	FVector Adjusted = (Velocity + ZoneVel) * deltaTime; 
	GetLevel()->MoveActor(this, Adjusted, Rotation, Hit);
	if (Hit.Time < 1.f) 
	{
		Floor = Hit.Normal;
		FVector GravDir = FVector(0,0,-1);
		if (PhysicsVolume->Gravity.Z > 0)
			GravDir.Z = 1;
		FVector DesiredDir = Adjusted.SafeNormal();
		FVector VelDir = Velocity.SafeNormal();
		FLOAT UpDown = GravDir | VelDir;
		if ( (Abs(Hit.Normal.Z) < 0.2f) && (UpDown < 0.5f) && (UpDown > -0.2f) && 0 )// don't do step up??
		{
			FLOAT stepZ = Location.Z;
			stepUp(GravDir, DesiredDir, Adjusted * (1.f - Hit.Time), Hit);
			OldLocation.Z = Location.Z + (OldLocation.Z - stepZ);
		}
		else
		{
			processHitWall(Hit.Normal, Hit.Actor);
			//adjust and try again
			FVector OldHitNormal = Hit.Normal;
			float t = 1.0f - Hit.Time;
			float elasticity = 1.0f; // 2.0f
			FVector Delta = (Adjusted - Hit.Normal * (elasticity*(Adjusted | Hit.Normal))) * (1.f - Hit.Time);
			if( (Delta | Adjusted) >= 0 )
			{
				GetLevel()->MoveActor(this, Delta, Rotation, Hit);
				if (Hit.Time < 1.f) //hit second wall
				{
					processHitWall(Hit.Normal, Hit.Actor);
					TwoWallAdjust(DesiredDir, Delta, Hit.Normal, OldHitNormal, Hit.Time);
					GetLevel()->MoveActor(this, Delta, Rotation, Hit);
				}
			}
			Velocity = Delta / t;
		}

		// reflect velocity from collision
	}
	else
		Floor = FVector(0.f,0.f,1.f);

	if (!bJustTeleported)
		Velocity = (Location - OldLocation) / deltaTime;

	unguard;
}
// --- sjs

