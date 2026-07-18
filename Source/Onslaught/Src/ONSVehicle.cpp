/*=============================================================================
	ONSVehicle.cpp: Onslaught specific vehicle support
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Dave Hagewood @ Psyonix - 04/29/03
=============================================================================*/

#include "OnslaughtPrivate.h"

// Don't let repulsors hit non-vehicle pawns of the other team.
UBOOL AONSVehicle::KRepulsorsShouldHit(AActor* Actor)
{
	guard(AONSVehicle::KRepulsorsShouldHit);

	// Always hit non-pawns
	APawn* P = Cast<APawn>(Actor);
	if(!P)
		return true;

	// Do not hit raptors
	AONSChopperCraft* R = Cast<AONSChopperCraft>(P);
	if(R)
		return false;

	// Always hit other vehicles
	AVehicle* V = Cast<AVehicle>(P);	
	if(V)
		return true;

	// If we can't tell the team, we hit it.
	if(!P->PlayerReplicationInfo || !P->PlayerReplicationInfo->Team)
		return true;

	// If pawn is on same team hit it, otherwise don't.
	if(P->PlayerReplicationInfo->Team->TeamIndex == Team)
		return true;
	else
		return false;

	unguard;
}

void AONSVehicle::PostNetReceive()
{
	guard(AONSVehicle::PostNetReceive);

	Super::PostNetReceive();

	if (bDestroyAppearance)
	{
		eventDestroyAppearance();
		bDestroyAppearance = false;
	}

	if (ExplosionCount != OldExplosionCount)
	{
		eventClientVehicleExplosion(false);
		OldExplosionCount = ExplosionCount;
	}

	if (bDisintegrateVehicle)
	{
		eventClientVehicleExplosion(true);
		bDisintegrateVehicle = false;
	}

	unguard;
}

UBOOL AONSVehicle::Tick(FLOAT DeltaTime, enum ELevelTick TickType)
{
	guard(AONSVehicle::Tick);

	if (!Super::Tick(DeltaTime, TickType))
		return 0;

	if (bDeleteMe || (TickType == LEVELTICK_ViewportsOnly && !PlayerControlled()))
		return 1;

	for (INT i = 0; i < WeaponPawns.Num(); i++)
		if (WeaponPawns(i) && WeaponPawns(i)->Gun)
			GetLevel()->FarMoveActor(WeaponPawns(i), WeaponPawns(i)->Gun->Location, 0, 1, 1);

	if (Role == ROLE_Authority)
	{
		// Check we are upside down and touching the level every half a second.
		if (UpsideDownDamage > 0 || bEjectPassengersWhenFlipped)
		{
			UKarmaParams* KP = Cast<UKarmaParams>(KParams);

			if (KP && (KP->bContactingLevel || bVehicleOnGround))
			{
				if (eventNeedsFlip())
				{
					if (bEjectPassengersWhenFlipped && (Level->TimeSeconds - LastCheckUpsideDownTime) > 0.5f)
					{
						if (Driver)
							eventKDriverLeave(true);

						for (INT i=0; i < WeaponPawns.Num(); i++)
							if (WeaponPawns(i) && WeaponPawns(i)->Driver)
								WeaponPawns(i)->eventKDriverLeave(true);
						
						LastCheckUpsideDownTime = Level->TimeSeconds;
					}
					if (UpsideDownDamage > 0)
						eventTakeDamage(UpsideDownDamage * DeltaTime, this, FVector(0,0,0), FVector(0,0,0), ExplosionDamageType);
				}				
			}
		}

		//check for pawns in danger of being run over every half a second
		if (Controller && (Level->TimeSeconds - LastRunOverWarningTime) > 0.5f)
		{
			FLOAT SpeedSquared = Velocity.SizeSquared();
			if (SpeedSquared > MinRunOverSpeed * MinRunOverSpeed)
			{
				FVector VelNormal = Velocity.SafeNormal();
				FLOAT WarningDistSquared = SpeedSquared * 2.f;
				for (AController* C = Level->ControllerList; C; C = C->nextController)
					if ( C != Controller && C->Pawn && !C->Pawn->IsA(AVehicle::StaticClass())
						&& ( !PlayerReplicationInfo || !PlayerReplicationInfo->Team || !C->PlayerReplicationInfo
							|| PlayerReplicationInfo->Team != C->PlayerReplicationInfo->Team ) )
					{
						FVector Dir = C->Pawn->Location - Location;
						if (Dir.SizeSquared() < WarningDistSquared && (VelNormal | Dir.SafeNormal()) > MinRunOverWarningAim)
							C->eventReceiveWarning(this, appSqrt(SpeedSquared), VelNormal);
					}
				LastRunOverWarningTime = Level->TimeSeconds;
			}
		}

		if (ImpactTicksLeft > 0 && KParams)
		{
			FLOAT AccelMag = KParams->KAcceleration.Size();
			if (AccelMag > ImpactDamageThreshold)
			{
				eventTakeImpactDamage(AccelMag);
				if (bDeleteMe)
					return 1;
				ImpactTicksLeft = 0;
        	}
			else
				ImpactTicksLeft--;
		}

		for (INT x = 0; x < WeaponPawns.Num(); x++)
			if (WeaponPawns(x) && !WeaponPawns(x)->bHasOwnHealth && WeaponPawns(x)->Health != Health)
			{
				WeaponPawns(x)->Health = Health;
				WeaponPawns(x)->bNetDirty = true;
			}

		if (!bDisintegrateVehicle && Health <= DisintegrationHealth)
		{
			bDisintegrateVehicle = true;
			eventClientVehicleExplosion(true);
		}
	}

	if (Driver)
	{
		Driver->LastRenderTime = LastRenderTime; //So driver is considered worth ragdolling if he's killed
		if (Controller && ActiveWeapon < Weapons.Num() && Weapons(ActiveWeapon))
		{
			if (Weapons(ActiveWeapon)->bAimable)
			{
				FVector CameraLocation;
				FRotator CameraRotation;
				APlayerController* PC = Controller->GetAPlayerController();

				if (!PC)
				{
					if (Controller->Focus && !Controller->Focus->bCollideActors && !Controller->Focus->bProjTarget && Controller->Enemy && Cast<ANavigationPoint>(Controller->Focus))
						Weapons(ActiveWeapon)->SetAim(Controller->LastSeenPos, Rotation);
					else if ( bAltFocalPoint )
						Weapons(ActiveWeapon)->SetAim(AltFocalPoint, Rotation);
					else
						Weapons(ActiveWeapon)->SetAim(Controller->Focus ? Controller->Focus->Location : Controller->FocalPoint, Rotation);
				}
				else
				{
					AActor *CameraActor = Controller;
					CameraRotation = Controller->Rotation;
					PC->eventPlayerCalcView(CameraActor, CameraLocation, CameraRotation);

					if (bCustomAiming)
						Weapons(ActiveWeapon)->SetAim(Weapons(ActiveWeapon)->Location + Weapons(ActiveWeapon)->AimTraceRange * CustomAim.Vector(), Rotation);
					else
					{
						//don't want these to block the trace
						Driver->bBlockZeroExtentTraces = false;
						for (INT x = 0; x < WeaponPawns.Num(); x++)
							if (WeaponPawns(x))
							{
								WeaponPawns(x)->bBlockZeroExtentTraces = false;
								if (WeaponPawns(x)->Driver)
									WeaponPawns(x)->Driver->bBlockZeroExtentTraces = false;
							}
						for (INT x = 0; x < Weapons(ActiveWeapon)->Projectiles.Num(); x++)
						{
							if (!Weapons(ActiveWeapon)->Projectiles(x))
							{
								Weapons(ActiveWeapon)->Projectiles.Remove(x);
								x--;
							}
							else
								Weapons(ActiveWeapon)->Projectiles(x)->bBlockZeroExtentTraces = false;
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

							GetLevel()->SingleLineCheck(Hit, Hit.Actor ? Hit.Actor : this, CameraLocation + Weapons(ActiveWeapon)->AimTraceRange * CameraDir, HitLocation, TRACE_ProjTargets, FVector(0,0,0));
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
								HitLocation = CameraLocation + Weapons(ActiveWeapon)->AimTraceRange * CameraDir;

							bGoodAim = Weapons(ActiveWeapon)->SetAim(HitLocation, Rotation);

						} while (!bGoodAim && Hit.Actor && Count < 3);

						if (!bGoodAim && Hit.Actor && Count == 3)
							Weapons(ActiveWeapon)->SetAim(CameraLocation + Weapons(ActiveWeapon)->AimTraceRange * CameraDir, Rotation);

						Driver->bBlockZeroExtentTraces = true;
						for (INT x = 0; x < WeaponPawns.Num(); x++)
							if (WeaponPawns(x))
							{
								WeaponPawns(x)->bBlockZeroExtentTraces = true;
								if (WeaponPawns(x)->Driver)
									WeaponPawns(x)->Driver->bBlockZeroExtentTraces = true;
							}
						for (INT x = 0; x < Weapons(ActiveWeapon)->Projectiles.Num(); x++)
								Weapons(ActiveWeapon)->Projectiles(x)->bBlockZeroExtentTraces = true;
					}
				}
			}

			if (Role == ROLE_Authority && Weapons(ActiveWeapon)->FireCountdown <= 0)
			{
				if (bWeaponisFiring)
				{
					if ( Weapons(ActiveWeapon)->eventAttemptFire(Controller, false) )
						eventApplyFireImpulse(false);
				}
				else if (bWeaponisAltFiring && bHasAltFire)
				{
					if ( Weapons(ActiveWeapon)->eventAttemptFire(Controller, true) )
						eventApplyFireImpulse(true);
				}
			}
		}
	}
	else if (Role == ROLE_Authority && Level->TimeSeconds >= ResetTime && Level->TimeSeconds - DeltaTime < ResetTime)
	{
		eventCheckReset();
		if (bDeleteMe)
			return 1;
	}

	if (Level->NetMode != NM_DedicatedServer)
	{
		if (HeadlightProjector)
		{
			HeadlightProjector->Detach(0);
			if (bDriving)
				HeadlightProjector->Attach();
		}

		for (INT x = 0; x < HeadlightCorona.Num(); x++)
			if (HeadlightCorona(x))
				HeadlightCorona(x)->bCorona = bDriving;

		if (DamagedEffectClass)
		{
			INT DamagedHealthThresh = INT(DamagedEffectHealthSmokeFactor * HealthMax);
			if (Health <= DamagedHealthThresh && !DamagedEffect)
			{
				DamagedEffect = Cast<AONSDamagedEffect>(GetLevel()->SpawnActor( DamagedEffectClass, NAME_None,
														Location + DamagedEffectOffset.TransformVectorBy(GMath.UnitCoords * Rotation),
														Rotation, NULL, 0, 0, this, Instigator ));
				if (DamagedEffect)
				        DamagedEffect->SetBase(this, FVector(0,0,1));
			}
			else if (Health >= DamagedHealthThresh && DamagedEffect)
			{
				DamagedEffect->Kill();
				DamagedEffect = NULL;
			}
		}

		if (DamagedEffect)
		{
			UBOOL bFire = (Health < DamagedEffectHealthFireFactor * HealthMax);
			if (bHadFire != bFire || Abs(Velocity.Size() - LastVelocitySize) > 250)
			{
				DamagedEffect->eventUpdateDamagedEffect(bFire, Velocity.Size());
				bHadFire = bFire;
				LastVelocitySize = Velocity.Size();
			}
			if (bFire && eventIsVehicleEmpty())
			{
				DamagedEffectAccruedDamage += DamagedEffectFireDamagePerSec * DeltaTime;
				if (DamagedEffectAccruedDamage > 1.0)
				{
					eventTakeDamage(DamagedEffectAccruedDamage, this, FVector(0,0,0), FVector(0,0,0), ExplosionDamageType);
					DamagedEffectAccruedDamage = 0.0;
				}
			}
		}

		if (SparkEffect)
		{
			#ifdef WITH_KARMA
			if (KParams && KParams->bContactingLevel)
			{
				KParams->CalcContactRegion();

				FVector SparkPlaneVel = Velocity - (KParams->ContactRegionNormal * (Velocity | KParams->ContactRegionNormal));
				GetLevel()->FarMoveActor(SparkEffect, KParams->ContactRegionCenter + (SparkPlaneVel * DeltaTime * SparkAdvanceFactor));
				FCheckResult Hit(1.0f);
				GetLevel()->MoveActor(SparkEffect, FVector(0,0,0), KParams->ContactRegionNormal.Rotation(), Hit);
				SparkEffect->eventUpdateSparks(KParams->ContactRegionRadius, Velocity);
			}
			else
			#endif
			{
				if (SparkEffect->bSparksActive)
					SparkEffect->eventUpdateSparks(0, FVector(0,0,0));
			}
		}

		if( bEnableProximityViewShake && (bDriving || !bOnlyViewShakeIfDriven) && (Level->TimeSeconds - ViewShakeLastCheck) > 0.5f )
		{
			APlayerController* PC = Level->LocalPlayerController;
			if (PC && PC->Pawn && PC->Pawn->Physics == PHYS_Walking)
			{
				UBOOL bIsWeaponPawn = false;
				for (INT i = 0; i < WeaponPawns.Num(); i++)
					if (PC->Pawn == WeaponPawns(i))
					{
						bIsWeaponPawn = true;
						break;
					}

				if (!bIsWeaponPawn)
				{
					FLOAT ShakeMag = 1.f - ((Location - PC->Pawn->Location).SizeSquared() / (ViewShakeRadius * ViewShakeRadius));
					ShakeMag = Clamp<FLOAT>(ShakeMag, 0.f, 1.f);

					if (ShakeMag > 0.f)
					{
						// Use a time long enough to keep us shaking until the next update.
						PC->eventSetAmbientShake(Level->TimeSeconds + 0.7f, 0.5f, ShakeMag * ViewShakeOffsetMag, ViewShakeOffsetFreq, ShakeMag * ViewShakeRotMag, ViewShakeRotFreq);
					}
				}
			}

			ViewShakeLastCheck = Level->TimeSeconds;
		}
	}

	return 1;

	unguard;
}

void AONSVehicle::SetBase( AActor* NewBase, FVector NewFloor, int bNotifyActor )
{
	guard(AONSVehicle::SetBase);

	if (bCannotBeBased)
		return;

	Super::SetBase(NewBase,NewFloor,bNotifyActor);
	unguard;
}

IMPLEMENT_CLASS(AONSVehicle);
IMPLEMENT_CLASS(AONSDamagedEffect);
IMPLEMENT_CLASS(AONSHeadlightProjector);
IMPLEMENT_CLASS(AONSHeadlightCorona);
IMPLEMENT_CLASS(AONSImpactSparks);
