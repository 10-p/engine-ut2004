/*=============================================================================
	ONSWeapon.cpp: Support for attachable skeletal weapons
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Dave Hagewood @ Psyonix - 04/28/03
=============================================================================*/

#include "OnslaughtPrivate.h"
#include "UnNet.h"


static inline float	ClampAngle(FLOAT Angle)
{
	return (FLOAT)((int)Angle & 65535);
}

static inline float CWAngularDelta(FLOAT EndAngle, FLOAT StartAngle)
{
    // Returns the Clockwise delta from the StartAngle to the EndAngle

    return ClampAngle(EndAngle - StartAngle);
}

static inline float CCWAngularDelta(FLOAT EndAngle, FLOAT StartAngle)
{
    // Returns the Counter-Clockwise delta from the StartAngle to the EndAngle

    return -(ClampAngle(StartAngle - EndAngle));
}	

static inline float ShortestAngularDelta(float EndAngle, float StartAngle)
{
    // Returns the shortest delta that can be added to the StartAngle to get the EndAngle
    // Counter-Clockwise deltas will be negative!

    FLOAT DeltaCW, DeltaCCW;

    DeltaCW = CWAngularDelta(EndAngle, StartAngle);
    DeltaCCW = CCWAngularDelta(EndAngle, StartAngle);

    if (DeltaCW < 32768)
        return DeltaCW;
    else
        return DeltaCCW;
}

static inline FRotator SmoothRotate(FLOAT YawDelta, FLOAT PitchDelta, FRotator CurrentRotation, FLOAT RPS, FLOAT deltaSeconds)
{
    FLOAT AngularDistance;
    FRotator Aim;

    AngularDistance = ClampAngle(deltaSeconds * RPS * 65536);

	Aim.Yaw = CurrentRotation.Yaw + Clamp(YawDelta, -AngularDistance, AngularDistance);
	Aim.Pitch = CurrentRotation.Pitch + Clamp(PitchDelta, -AngularDistance, AngularDistance);
    Aim.Roll = 0;

    return Aim;
}

void AONSWeapon::execLimitPitch(FFrame& Stack, RESULT_DECL)
{
	guard(AONSWeapon::execLimitPitch);

	P_GET_INT(Pitch);
	P_GET_ROTATOR(ForwardRotation);
	P_GET_INT_OPTX(WeaponYaw, CurrentAim.Vector().TransformVectorBy(GMath.UnitCoords * Rotation).Rotation().Yaw);
	P_FINISH;

	*(INT*)Result = LimitPitch(Pitch, ForwardRotation, WeaponYaw);
	
	unguardexec;
}

INT AONSWeapon::LimitPitch(INT Pitch, FRotator ForwardRotation, INT WeaponYaw)
{
	FCoords Coords = GMath.UnitCoords / ForwardRotation;
	FQuat Forward = FCoordsQuaternion(Coords);
	FQuat Side = FCoordsQuaternion(GMath.UnitCoords / Coords.YAxis.Rotation());
	FQuat Result = SlerpQuat(Forward, Side, FLOAT((WeaponYaw - ForwardRotation.Yaw) & 65535) / 16384);
	INT PitchAdjust = FQuaternionCoords(Result).OrthoRotation().Pitch & 65535;

	INT AdjustedPitchUpLimit = (PitchUpLimit + PitchAdjust) & 65535;
	INT AdjustedPitchDownLimit = (PitchDownLimit + PitchAdjust) & 65535;

	Pitch = Pitch & 65535;
	if (AdjustedPitchDownLimit > AdjustedPitchUpLimit)
	{
		if (Pitch > AdjustedPitchUpLimit && Pitch < AdjustedPitchDownLimit)
		{
			if (Pitch - AdjustedPitchUpLimit < AdjustedPitchDownLimit - Pitch)
				Pitch = AdjustedPitchUpLimit;
			else
				Pitch = AdjustedPitchDownLimit;
		}
	}
	else
	{
		if (Pitch > AdjustedPitchUpLimit || Pitch < AdjustedPitchDownLimit)
		{
			FVector PitchDir = FRotator(Pitch,0,0).Vector();
			if ((FRotator(AdjustedPitchUpLimit,0,0).Vector() | PitchDir) > (FRotator(AdjustedPitchDownLimit,0,0).Vector() | PitchDir))
				Pitch = AdjustedPitchUpLimit;
			else
				Pitch = AdjustedPitchDownLimit;
		}
	}

	return Pitch;
}

FVector AONSWeapon::GetAimStart() const
{
	INT BoneIdx = ((USkeletalMeshInstance*)MeshInstance)->MatchRefBone( WeaponFireAttachmentBone );
	FCoords WeaponBoneCoords;
	if( BoneIdx >= 0)
		WeaponBoneCoords = ((USkeletalMeshInstance*)MeshInstance)->GetBoneCoords(BoneIdx);
	else
		WeaponBoneCoords = ((USkeletalMeshInstance*)MeshInstance)->GetBoneCoords(0);
	
	return WeaponBoneCoords.Origin;
}

//Set the location the weapon should aim towards. Returns true if the weapon will be able to point at that location,
//false if pitch limitations prevent it
//
UBOOL AONSWeapon::SetAim(FVector HitLocation, FRotator ForwardRotation)
{
	guard(AONSWeapon::SetAim);

	if (AimLockReleaseTime > Level->TimeSeconds)
    	return true;

	//If any of these checks fail, we can't do anything meaningful, so just bail out
	if (!Mesh)
	{
		debugf(NAME_Warning, TEXT("ONSWeapon %s doesn't have a mesh!"), GetName());
		return true;
	}
	USkeletalMesh* smesh = Cast<USkeletalMesh>(Mesh);
	if (!smesh)
	{
		debugf(NAME_Warning, TEXT("ONSWeapon %s doesn't have a skeletal mesh!"), GetName());
		return true;
	}
	USkeletalMeshInstance* inst = (USkeletalMeshInstance*)smesh->MeshGetInstance(this);
	if (!inst)
	{
		debugf(NAME_Warning, TEXT("ONSWeapon %s doesn't have a skeletal mesh instance!"), GetName());
		return true;
	}

	CurrentHitLocation = HitLocation;
	bNetDirty = true;

	FVector AimStart = GetAimStart();
	FVector AimVectorWorld = HitLocation - AimStart;
	FRotator AimRotatorWorld = AimVectorWorld.Rotation();
	INT NewPitch = LimitPitch(AimRotatorWorld.Pitch, ForwardRotation, AimRotatorWorld.Yaw);
	UBOOL bResult = ((AimRotatorWorld.Pitch & 65535) == NewPitch);
	if (!bAimable || !bInstantRotation)
		return bResult;

	if (bResult)
		AimVectorWorld.Normalize();
	else
	{
		AimRotatorWorld.Pitch = NewPitch;
		AimVectorWorld = AimRotatorWorld.Vector();
	}

	FVector AimVectorLocal = AimVectorWorld.TransformVectorBy(GMath.UnitCoords / Rotation);
	CurrentAim = AimVectorLocal.Rotation();

	if (YawBone == PitchBone)
	{
		FCoords AimCoords = GMath.UnitCoords * FRotator(0, CurrentAim.Yaw, 0) * FRotator(CurrentAim.Pitch, 0, 0);
		FRotator NewAim = AimCoords.OrthoRotation();
		inst->SetBoneRotation(YawBone, NewAim, 0, 1);
	}
	else
	{
   		inst->SetBoneRotation(YawBone, FRotator(0, -CurrentAim.Yaw, 0), 0, 1);
		inst->SetBoneRotation(PitchBone, FRotator(-CurrentAim.Pitch, 0, 0), 0, 1);
	}

	if (bResult)
	{
		if (!Instigator || !Instigator->IsHumanControlled())
			bCorrectAim = true;
		else
		{
			FCheckResult Hit(1.0f);
			bCorrectAim = GetLevel()->SingleLineCheck(Hit, Instigator, CurrentHitLocation, AimStart, TRACE_ProjTargets, FVector(0,0,0));
		}
	}
	else
		bCorrectAim = false;

	return bResult;

	unguard;
}

UBOOL AONSLinkableWeapon::SetAim(FVector HitLocation, FRotator ForwardRotation)
{
	guard(AONSLinkableWeapon::SetAim);

	if (ChildWeapon)
		ChildWeapon->SetAim(HitLocation, ForwardRotation);

	return Super::SetAim(HitLocation, ForwardRotation);

	unguard;
}

void AONSWeapon::PostNetReceive()
{
	guard(AONSWeapon::PostNetReceive);

	Super::PostNetReceive();

	if (bShowChargingBar && Owner)
	{
		AVehicle *V = Cast<AVehicle>(Owner);
		if (V)
			V->bShowChargingBar = bShowChargingBar;
	}

	if (OldFlashCount != FlashCount)
	{
        OldFlashCount = FlashCount;
        if (FlashCount)
        {
            eventFlashMuzzleFlash();
            if (AmbientEffectEmitter && !AmbientEffectEmitter->bDeleteMe)
			{
				AmbientEffectEmitter->eventSetEmitterStatus(true);
			}
        }
        else
        {
			if (AmbientEffectEmitter && !AmbientEffectEmitter->bDeleteMe)
			{
        		AmbientEffectEmitter->eventSetEmitterStatus(false);
			}
		}
    }
    if (OldHitCount != HitCount)
    {
        OldHitCount = HitCount;
   	    if (HitCount)
            eventClientSpawnHitEffects();
    }

	unguard;
}

UBOOL AONSWeapon::Tick(FLOAT DeltaTime, enum ELevelTick TickType)
{
	guard(AONSWeapon::Tick);

	if (!Super::Tick(DeltaTime, TickType))
		return 0;

	if (!bActive || (TickType == LEVELTICK_ViewportsOnly && !PlayerControlled()))
	{
		if ( AmbientSound == RotateSound )
			AmbientSound = NULL;
		return 1;
	}

	if (FireCountdown > 0)
	{
		FireCountdown -= DeltaTime;
		if (FireCountdown <= 0 && Level->NetMode != NM_DedicatedServer)
		{
			AVehicle* V = Cast<AVehicle>(Owner);
			if (V && V->IsLocallyControlled() && V->IsHumanControlled() && ((V->bWeaponisFiring && !bIsAltFire) || (V->bWeaponisAltFiring && bIsAltFire)))
				eventOwnerEffects();
		}
	}

	// Added to support freecamera in vehicle weapons
	if (Instigator && Instigator->Controller)
	{
		APlayerController* PC = Instigator->Controller->GetAPlayerController();
		if (PC && PC->bFreeCamera)
			bAimable = false;
		else
			bAimable = ((AONSWeapon *)(GetClass()->GetDefaultActor()))->bAimable;
	}

	if (bAimable && (!bInstantRotation || (Role < ROLE_Authority && !Owner)) && AimLockReleaseTime <= Level->TimeSeconds)
	{
		FRotator  Aim, NewAim, AimRotatorWorld;
		FLOAT	  YawDelta, PitchDelta, AimEndDelta, AimStartDelta, EndAimDelta;

		//If any of these checks fail, we can't do anything meaningful, so just bail out
		if (!Mesh)
		{
			debugf(NAME_Warning, TEXT("ONSWeapon %s doesn't have a mesh!"), GetName());
			return 1;
		}
		USkeletalMesh* smesh = Cast<USkeletalMesh>(Mesh);
		if (!smesh)
		{
			debugf(NAME_Warning, TEXT("ONSWeapon %s doesn't have a skeletal mesh!"), GetName());
			return 1;
		}
		USkeletalMeshInstance* inst = (USkeletalMeshInstance*)smesh->MeshGetInstance(this);
		if (!inst)
		{
			debugf(NAME_Warning, TEXT("ONSWeapon %s doesn't have a skeletal mesh instance!"), GetName());
			return 1;
		}

		FVector AimStart = GetAimStart();

		if (bForceCenterAim)
			Aim = FRotator(0,0,0);
		else
		{
			FVector AimVectorWorld = CurrentHitLocation - AimStart;
			AimVectorWorld.Normalize();
			AimRotatorWorld = AimVectorWorld.Rotation();
			FVector AimVectorLocal = AimVectorWorld.TransformVectorBy(GMath.UnitCoords / Rotation);
			Aim = AimVectorLocal.Rotation();
		}

		NewAim.Yaw = 0;
		NewAim.Pitch = 0;
		NewAim.Roll = 0;

		// Find the shortest rotation from gun to aim
		YawDelta = ShortestAngularDelta(Aim.Yaw, CurrentAim.Yaw);
		PitchDelta = ShortestAngularDelta(Aim.Pitch, CurrentAim.Pitch);

		// Smooth rotate Aim and call it NewAim
		NewAim = SmoothRotate(YawDelta, PitchDelta, CurrentAim, RotationsPerSecond, DeltaTime);

		if (RotateSound)
		{
			if (Abs(YawDelta) > RotateSoundThreshold)
				AmbientSound = RotateSound;
			else
				AmbientSound = GetClass()->GetDefaultActor()->AmbientSound;
		}

		if (!bForceCenterAim)
		{
			// Pitch limiting
			FRotator ForwardRotation;
			AONSWeaponPawn* WP = Cast<AONSWeaponPawn>(Instigator);
			if (WP)
				ForwardRotation = WP->VehicleBase ? WP->VehicleBase->Rotation : FRotator(0,0,0);
			else
				ForwardRotation = Instigator ? Instigator->Rotation : Rotation; //want Instigator's rotation, but if we don't have it, my rotation should be close enough
			NewAim.Pitch = LimitPitch(NewAim.Pitch, ForwardRotation, AimRotatorWorld.Yaw);

			UBOOL bHumanControlled = (Instigator && Instigator->IsHumanControlled());
			if (!bHumanControlled)
				bCorrectAim = ((NewAim.Yaw & 65535) == (Aim.Yaw & 65535) && ((NewAim.Pitch & 65535) == (Aim.Pitch & 65535) || NewAim.Pitch == LimitPitch(Aim.Pitch, ForwardRotation, AimRotatorWorld.Yaw)));
			else if ((NewAim.Yaw & 65535) == (Aim.Yaw & 65535) && (NewAim.Pitch & 65535) == (Aim.Pitch & 65535))
			{
				FCheckResult Hit(1.0f);
				bCorrectAim = GetLevel()->SingleLineCheck(Hit, Instigator, CurrentHitLocation, AimStart, TRACE_ProjTargets, FVector(0,0,0));
			}
			else
				bCorrectAim = false;
		}
		else
			bCorrectAim = false;


        ///////////////////////////////////
        // Weapon Part Constraint System //
        ///////////////////////////////////

        // Yaw Delta from Aim to EndConstraint
        AimEndDelta = CWAngularDelta(YawEndConstraint, NewAim.Yaw);

        if (AimEndDelta > YawConstraintDelta) // True if we are outside the constraints
        {
            // Yaw Delta from Aim to StartConstraint
            AimStartDelta = CWAngularDelta(YawStartConstraint, NewAim.Yaw);

            // Yaw Delta from EndConstraint to Aim
            EndAimDelta = CWAngularDelta(NewAim.Yaw, YawEndConstraint);

            if (AimStartDelta < EndAimDelta) // True if we are closer to StartConstraint
                NewAim.Yaw = YawStartConstraint;
            else
                NewAim.Yaw = YawEndConstraint;
        }

		CurrentAim.Pitch = ClampAngle(NewAim.Pitch);
		CurrentAim.Yaw = ClampAngle(NewAim.Yaw);
		CurrentAim.Roll = 0;

		if (YawBone == PitchBone)
		{
			FCoords AimCoords = GMath.UnitCoords * FRotator(0, NewAim.Yaw, 0) * FRotator(NewAim.Pitch, 0, 0);
			NewAim = AimCoords.OrthoRotation();
			inst->SetBoneRotation(YawBone, NewAim, 0, 1);
		}
		else
		{
			inst->SetBoneRotation(YawBone, FRotator(0, -NewAim.Yaw, 0), 0, 1);
			inst->SetBoneRotation(PitchBone, FRotator(-NewAim.Pitch, 0, 0), 0, 1);
		}
	}
	return 1;

	unguard;
}

void AONSWeapon::PostRender(FLevelSceneNode* SceneNode, FRenderInterface* RI)
{
	if (bCallInstigatorPostRender && Instigator)
		Instigator->PostRender(SceneNode, RI);
}

APawn* AONSWeaponPawn::GetVehicleBase()
{
	return VehicleBase;
}

BUGGYINLINE UBOOL AONSWeaponPawn::IsJoinedTo( const AActor *Other) const
{
	guard(AONSWeaponPawn::IsJoinedTo);

	if (Other == VehicleBase)
		return 1;

	// A JointedTag of 0 means this has never been joined to anything,
	// so that Actors can't be jointed together!
	for( const AActor* Test=this; Test!=NULL; Test=Test->Base )
		if( Test == Other || (Test && Other && Test->JoinedTag == Other->JoinedTag && Test->JoinedTag != 0) )
			return 1;
	return 0;

	unguard;
}

UBOOL AONSWeaponPawn::Tick(FLOAT DeltaTime, enum ELevelTick TickType)
{
	guard(AONSWeaponPawn::Tick);

	if (!Super::Tick(DeltaTime, TickType))
		return 0;

	if (TickType == LEVELTICK_ViewportsOnly && !PlayerControlled())
		return 1;

	if (Gun)
	{
		if (Controller)
		{
			if (Gun->bAimable)
			{
				FVector CameraLocation;
				FRotator CameraRotation;
				INT x;
				APlayerController* PC = Controller->GetAPlayerController();

				if (!PC)
					Gun->SetAim(Controller->Focus ? Controller->Focus->Location : Controller->FocalPoint, VehicleBase ? VehicleBase->Rotation : FRotator(0,0,0));

				else if ( GetLevel()->DemoRecDriver && GetLevel()->DemoRecDriver->ServerConnection )
				{
					FVector AimPoint(1,0,0);
					AimPoint = Gun->GetAimStart() + (Gun->AimTraceRange * AimPoint.TransformVectorBy(GMath.UnitCoords / (PC->Rotation*-1)));
					Gun->SetAim(AimPoint, PC->Rotation);
				}
				else
				{
					AActor *CameraActor = Controller;
					CameraRotation = Controller->Rotation;

					PC->eventPlayerCalcView(CameraActor, CameraLocation, CameraRotation);

					if (bCustomAiming)
						Gun->SetAim(Gun->Location + Gun->AimTraceRange * CustomAim.Vector(), VehicleBase ? VehicleBase->Rotation : Rotation);
					else
					{
						//don't want these to block the trace
						if (Driver)
							Driver->bBlockZeroExtentTraces = false;
						if (VehicleBase)
						{
							VehicleBase->bBlockZeroExtentTraces = false;
							if (VehicleBase->Driver)
								VehicleBase->Driver->bBlockZeroExtentTraces = false;
							for (x = 0; x < VehicleBase->WeaponPawns.Num(); x++)
								if (VehicleBase->WeaponPawns(x))
								{
									VehicleBase->WeaponPawns(x)->bBlockZeroExtentTraces = false;
									if (VehicleBase->WeaponPawns(x)->Driver)
										VehicleBase->WeaponPawns(x)->Driver->bBlockZeroExtentTraces = false;
								}
						}
						for (x = 0; x < Gun->Projectiles.Num(); x++)
						{
							if (!Gun->Projectiles(x))
							{
								Gun->Projectiles.Remove(x);
								x--;
							}
							else
								Gun->Projectiles(x)->bBlockZeroExtentTraces = false;
						}

						INT Count = 0;
						FCheckResult Hit(1.0f);
						FVector CameraDir = CameraRotation.Vector();
						CameraLocation += (Location - CameraLocation).Size() * CameraDir;
						FVector HitLocation = CameraLocation;
						UBOOL bGoodAim;
						do
						{
							Count++;

							GetLevel()->SingleLineCheck(Hit, Hit.Actor ? Hit.Actor : this, CameraLocation + Gun->AimTraceRange * CameraDir, HitLocation, TRACE_ProjTargets, FVector(0,0,0));
							if (Hit.Actor)
							{
								HitLocation = Hit.Location;
								if (!Hit.Actor->bWorldGeometry && !Hit.Actor->bBlockActors && !Hit.Actor->bCanBeDamaged)
								{
									bGoodAim = false;
									continue;
								}
							}
							else
								HitLocation = CameraLocation + Gun->AimTraceRange * CameraDir;

							bGoodAim = Gun->SetAim(HitLocation, VehicleBase ? VehicleBase->Rotation : FRotator(0,0,0));
						} while (!bGoodAim && Hit.Actor && Count < 3);

						if (!bGoodAim && Hit.Actor && Count == 3)
							Gun->SetAim(CameraLocation + Gun->AimTraceRange * CameraDir, VehicleBase ? VehicleBase->Rotation : Rotation);

						if (Driver)
							Driver->bBlockZeroExtentTraces = true;
						if (VehicleBase)
						{
							VehicleBase->bBlockZeroExtentTraces = true;
							if (VehicleBase->Driver)
								VehicleBase->Driver->bBlockZeroExtentTraces = true;
							for (x = 0; x < VehicleBase->WeaponPawns.Num(); x++)
								if (VehicleBase->WeaponPawns(x))
								{
									VehicleBase->WeaponPawns(x)->bBlockZeroExtentTraces = true;
									if (VehicleBase->WeaponPawns(x)->Driver)
										VehicleBase->WeaponPawns(x)->Driver->bBlockZeroExtentTraces = true;
								}
						}
						for (x = 0; x < Gun->Projectiles.Num(); x++)
								Gun->Projectiles(x)->bBlockZeroExtentTraces = true;
					}
				}
			}

			if (Role == ROLE_Authority && Gun->FireCountdown <= 0)
			{
				if (bWeaponisFiring)
				{
					if (Gun->eventAttemptFire(Controller, false) && bHasFireImpulse)
						eventApplyFireImpulse(false);
				}						
				else if (bWeaponisAltFiring && bHasAltFire)
				{
					if (Gun->eventAttemptFire(Controller, true) && bHasAltFireImpulse)
						eventApplyFireImpulse(true);
				}
			}
		}
	}
	return 1;
	unguard;
}

UBOOL AONSWeaponPawn::ReachedDesiredRotation()
{
	guardSlow(AONSWeaponPawn::ReachedDesiredRotation);
	
	if ( Gun && Gun->Mesh && (Gun->YawBone != NAME_None) )
		return Gun->bCorrectAim;

	return Super::ReachedDesiredRotation();
	unguardSlow;
}

UBOOL AONSWeaponPawn::SharingVehicleWith(APawn *P)
{
	guardSlow(AONSWeaponPawn::SharingVehicleWith);
	
	return ( VehicleBase &&
			((VehicleBase == P) || P->SharingVehicleWith(VehicleBase)) );
	unguardSlow;
}

// ------------------------------------------------------------------------------ //
// ONSRVWebProjectileLeader
// ------------------------------------------------------------------------------ //

static inline UBOOL NeighbourIsAttached(AONSRVWebProjectileLeader* Leader, INT ProjNumber)
{
	guard(NeighbourIsAttached);

	if( ProjNumber >= Leader->Projectiles.Num() )
		return false;

	if( ProjNumber > 0 && Leader->Projectiles(ProjNumber-1) && Leader->Projectiles(ProjNumber-1)->StuckActor )
		return true;

	if( ProjNumber < Leader->Projectiles.Num()-1 && Leader->Projectiles(ProjNumber+1) && Leader->Projectiles(ProjNumber+1)->StuckActor )
		return true;

	return false;

	unguard;
}

static inline UBOOL IsSuckableClass(AONSRVWebProjectileLeader* Leader, AActor* Actor)
{
	guard(IsSuckableClass);

	for(INT i=0; i<Leader->SuckTargetClasses.Num(); i++)
	{
		if( Actor->IsA( Leader->SuckTargetClasses(i) ) )
			return true;
	}

	return false;

	unguard;
}

void AONSRVWebProjectileLeader::ApplySpringForces(float DeltaSeconds)
{
	guard(AONSRVWebProjectileLeader::ApplySpringForces);

	// Work out how much gravity to apply to projectiles (function of fly time)
	FLOAT FlyTime = Level->TimeSeconds - FireTime;
	FVector ProjGravity = ProjGravityScale.Eval(FlyTime) * PhysicsVolume->Gravity;

	// Walk list and reset all accelerations.
	for(INT i=0; i<Projectiles.Num(); i++)
	{
		if( Projectiles(i) && !Projectiles(i)->bDeleteMe )
		{
			AONSRVWebProjectile* P = Projectiles(i);

			if ( (P->Velocity.Z < -450.f) && (P->LifeSpan < 8.f)  )
				P->LifeSpan = ::Min(P->LifeSpan, 0.01f);

			if( NeighbourIsAttached( this, i ) )
				P->Acceleration = ProjGravity - ((ProjVelDamping + ProjStuckNeighbourVelDamping) * P->Velocity);
			else
				P->Acceleration = ProjGravity - (ProjVelDamping * P->Velocity);

			UBOOL OldBeingSucked = P->bBeingSucked;
			P->bBeingSucked = false;

			// If suck force is enabled, do a radius check, and see if there are any suck targets 
			if(bEnableSuckTargetForce)
			{
				FLOAT ClosestSuckDistanceSqr = SuckTargetRange * SuckTargetRange;
				FVector ClosestSuckLocation;
				AActor* ClosestSuckActor = NULL;

				FMemMark Mark(GMem);
				// Add magnitude of suck target offset to our radius check (radius check only looks at actor origin - want to find all suck targets with range).
				FCheckResult* Link = GetLevel()->Hash->ActorRadiusCheck(GMem, P->Location, SuckTargetRange+SuckTargetOffset.Size(), 0);

				while(Link)
				{
					if( Link->Actor && !Link->Actor->bDeleteMe && IsSuckableClass(this, Link->Actor) )
					{
						AVehicle* SuckVehicle = Cast<AVehicle>(Link->Actor);
						check(SuckVehicle);

						if((bSuckFriendlyActor || SuckVehicle->Team != ProjTeam) && (!bOnlySuckToDriven || SuckVehicle->bDriving))
						{
							FMatrix L2W = SuckVehicle->LocalToWorld();
							FVector VehicleUp(L2W.M[2][0], L2W.M[2][1], L2W.M[2][2]);

							FVector ActorSuckLocation = L2W.TransformFVector(SuckTargetOffset);
							FVector Delta = P->Location - ActorSuckLocation;

							// If we can suck from below, or the projectile is above the suck location, consider it.
							if(!bNoSuckFromBelow || (Delta | VehicleUp) > 0.f)
							{
								FLOAT DistSqr = Delta.SizeSquared();
								if(DistSqr < ClosestSuckDistanceSqr)
								{
									ClosestSuckDistanceSqr = DistSqr;
									ClosestSuckLocation = ActorSuckLocation;
									ClosestSuckActor = SuckVehicle;
									P->bBeingSucked = true;
								}
							}

							// Check the mirrored suck location
							if(bSymmetricSuckTarget)
							{
								FVector MirrorSuckTargetOffset(SuckTargetOffset.X, -SuckTargetOffset.Y, SuckTargetOffset.Z);

								ActorSuckLocation = L2W.TransformFVector(MirrorSuckTargetOffset);
								Delta = P->Location - ActorSuckLocation;

								if(!bNoSuckFromBelow || (Delta | VehicleUp) > 0.f)
								{
									FLOAT DistSqr = Delta.SizeSquared();
									if(DistSqr < ClosestSuckDistanceSqr)
									{
										ClosestSuckDistanceSqr = DistSqr;
										ClosestSuckLocation = ActorSuckLocation;
										ClosestSuckActor = SuckVehicle;
										P->bBeingSucked = true;
									}
								}
							}

						}
					}
						
					Link = Link->GetNext();
				}

				Mark.Pop();

				if(P->bBeingSucked)
				{
					//GTempLineBatcher->AddLine(ClosestSuckLocation, P->Location, FColor(255,255,0));

					FVector SuckDir = (ClosestSuckLocation - P->Location).SafeNormal();
					P->Acceleration += (SuckTargetForce * SuckDir);

					// If we have just started being sucked - 
					if(!OldBeingSucked)
					{
						NetUpdateTime = Level->TimeSeconds - 1.f;
						//P->Velocity *= (1.f - SuckReduceVelFactor);
						P->Velocity = (SuckReduceVelFactor * ClosestSuckActor->Velocity) + ((1.f - SuckReduceVelFactor) * P->Velocity);
					}
				}
			}
		}

	}

	// Then calculate forces for each spring.
	for(INT i=0; i<Projectiles.Num() - 1; i++)
	{
		AONSRVWebProjectile* P1 = Projectiles(i);
		AONSRVWebProjectile* P2 = Projectiles(i+1);

		if(!P1 || P1->bDeleteMe || !P2 || P2->bDeleteMe || (P1->bBeingSucked && P2->bBeingSucked) )
			continue;

		// Find distance between projectiles
		FVector Delta = P2->Location - P1->Location;
		FLOAT DeltaMag = Delta.Size();

		// On the server 
		if(Role == ROLE_Authority && DeltaMag > SpringExplodeLength)
		{
			eventDetonateWeb();
			return;
		}

		FVector DeltaDir;
		if(DeltaMag > 0.01)
			DeltaDir = Delta / DeltaMag;
		else 
			DeltaDir = FVector(1,0,0);

		// Find 'stretch' of spring
		FLOAT Error = DeltaMag - SpringLength;

		// Find relative velocity along error vector.
		FVector RelVel = P2->Velocity - P1->Velocity;
		FLOAT RelVelMag = RelVel | DeltaDir;

		// Make force to push/pull particles apart.
		FLOAT ForceMag = (-SpringStiffness * Error) + (-SpringDamping * RelVelMag);
		ForceMag = Clamp<FLOAT>(ForceMag, -SpringMaxForce, SpringMaxForce);

		// Equal and opposite
		P1->Acceleration += -ForceMag * DeltaDir;
		P2->Acceleration += ForceMag * DeltaDir;	
	}




	unguard;
}

FLOAT AONSWeapon::GetAmbientVolume(FLOAT Attenuation)
{
	guardSlow(AONSWeapon::GetAmbientVolume);

	if( bFullVolume )
		return SoundVolume / 255.f / 2.f; // additionally divide by 2 to reduce max volume of ambient sounds
	else
		return AmbientSoundScaling * SoundVolume / 255.f / 2.f; // additionally divide by 2 to reduce max volume of ambient sounds
	unguardSlow;
}

// This should get called before any Beam emitters are ticked and before any projectiles get moved by the physics.
void AONSRVWebProjectileLeader::UpdateBeams(float DeltaSeconds)
{
	guard(AONSRVWebProjectileLeader::UpdateBeams);

	for(INT i=0; i<Projectiles.Num() - 1; i++)
	{
		AONSRVWebProjectile* P1 = Projectiles(i);
		AONSRVWebProjectile* P2 = Projectiles(i+1);

		if( P1 && !P1->bDeleteMe && P2 && !P2->bDeleteMe && P1->ProjectileEffect && P1->ProjectileEffect->Emitters.Num() > BeamSubEmitterIndex)
		{
			FVector PredictedVel = P2->Velocity + (P2->Acceleration * DeltaSeconds);
			FVector PredictedPos = P2->Location + (PredictedVel * DeltaSeconds);

			UBeamEmitter* BE = Cast<UBeamEmitter>( P1->ProjectileEffect->Emitters(BeamSubEmitterIndex) );
			if(BE && BE->BeamEndPoints.Num() > 0)
			{
				BE->BeamEndPoints(0).Offset.X.Min = PredictedPos.X;
				BE->BeamEndPoints(0).Offset.X.Max = PredictedPos.X;
				BE->BeamEndPoints(0).Offset.Y.Min = PredictedPos.Y;
				BE->BeamEndPoints(0).Offset.Y.Max = PredictedPos.Y;
				BE->BeamEndPoints(0).Offset.Z.Min = PredictedPos.Z;
				BE->BeamEndPoints(0).Offset.Z.Max = PredictedPos.Z;

				FLOAT RandU = FRange(0.5f, 2.5f).GetRand();
				if( appFrand() > 0.5f )
					RandU *= -1.0f;

				BE->BeamTextureUScale = RandU;

				BE->Disabled = false;
			}
		}
	}

	unguard;
}

void AONSRVWebProjectileLeader::TryPreAllProjectileTick(FLOAT DeltaSeconds)
{
	guard(AONSRVWebProjectileLeader::TryPreAllProjectileTick);

	if (bDeleteMe)
		return;

	for (INT i=0; i<Projectiles.Num(); i++)
		if (Projectiles(i) && !Projectiles(i)->bDeleteMe && Projectiles(i)->LastTickTime == Level->TimeSeconds)
			return; // Some projectiles have already been ticked this frame...

	// If we get here - this is the first projectile in this web to be ticked.
	// We do all the spring forces now, before physics is done.
	ApplySpringForces(DeltaSeconds);
	UpdateBeams(DeltaSeconds);

	unguard;
}

void AONSRVWebProjectile::TickAuthoritative(FLOAT DeltaSeconds)
{
	guard(AONSRVWebProjectile::TickAuthoritative);

	if (Leader)
		Leader->TryPreAllProjectileTick(DeltaSeconds);

	LastTickTime = Level->TimeSeconds;

	Super::TickAuthoritative(DeltaSeconds);

	unguard;
}

IMPLEMENT_CLASS(AONSWeapon);
IMPLEMENT_CLASS(AONSLinkableWeapon);
IMPLEMENT_CLASS(AONSWeaponPawn);
IMPLEMENT_CLASS(AONSWeaponAmbientEmitter);
IMPLEMENT_CLASS(AONSRVWebProjectile);
IMPLEMENT_CLASS(AONSRVWebProjectileLeader);
