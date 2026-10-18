/*=============================================================================
	UnLevTic.cpp: Level timer tick function
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "UnPath.h"
#include "PhysicsStateLogger.h"
/*-----------------------------------------------------------------------------
	Helper classes.
-----------------------------------------------------------------------------*/

//
// Priority sortable list.
//
struct FActorPriority
{
	INT			    Priority;	// Update priority, higher = more important.
	AActor*			Actor;		// Actor.
	UActorChannel*	Channel;	// Actor channel.
	FActorPriority()
	{}
	FActorPriority( FVector& ViewPos, FVector& ViewDir, UNetConnection* InConnection, UActorChannel* InChannel, AActor* InActor, APlayerController *Viewer, UBOOL bLowBandwidth )
	{
		guard(FActorPriority::FActorPriority);

		Actor       = InActor;
		Channel     = InChannel;

		FLOAT Time  = Channel ? (InConnection->Driver->Time - Channel->LastUpdateTime) : InConnection->Driver->SpawnPrioritySeconds;
		if ( (Actor == Viewer) || (Actor == Viewer->Pawn) || (Actor->Instigator && (Actor->Instigator == InActor)) )
			Time *= 4.f; 
		else if ( !Actor->bHidden )
		{
			FVector Dir = Actor->Location - ViewPos;
			FLOAT DistSq = Dir.SizeSquared();
			if ( bLowBandwidth )
			{
				UBOOL bBehindView = 0;
				// prioritize projectiles and pawns in actual view more 
				if ( (ViewDir | Dir) < 0.f )
				{
					bBehindView = true;
					if ( DistSq > 4000000.f )
						Time *= 0.2f;
					else if ( DistSq > 250000.f )
						Time *= 0.5f;
				}
				if ( Actor->GetAPawn() )
				{
					if ( !bBehindView )
					{
						Dir = Dir.SafeNormal();
						if ( (ViewDir | Dir) > 0.7f )
							Time *= 2.5f;
					}
					else
					{
						APawn * P = Actor->GetAPawn();
						if ( P->DrivenVehicle && (P->DrivenVehicle->Controller == Viewer) )
							Time *= 2.5f;
					}
					if ( DistSq * Viewer->FOVBias * Viewer->FOVBias > 10000000.f )
						Time *= 0.5f;
				}
				else if ( Actor->GetAProjectile() )
				{
					if ( !bBehindView )
					{
						Dir = Dir.SafeNormal();
						if ( (ViewDir | Dir) > 0.7f )
							Time *= 2.5f;
					}
					if ( DistSq > 10000000.f )
						Time *= 0.2f;
				}
				else if ( DistSq > 25000000.f )
					Time *= 0.3f;
				else if ( Actor->Base && (Actor->Base == Viewer->Pawn) )
					Time *= 3.f;
			}
			else if ( (ViewDir | Dir) < 0.f )
			{
				if ( DistSq > 4000000.f )
					Time *= 0.3f;
				else if ( DistSq > 250000.f )
					Time *= 0.5f;
			}
		}
		Priority = appRound(65536.0f * Actor->NetPriority * Time);
		unguard;
	}
	friend INT Compare( const FActorPriority* A, const FActorPriority* B )
	{
		return B->Priority - A->Priority;
	}
};

/*-----------------------------------------------------------------------------
	Tick a single actor.
-----------------------------------------------------------------------------*/

UBOOL AActor::CheckOwnerUpdated()
{
	guardSlow(AActor::CheckOwnerUpdated);
	if( Owner && (INT)Owner->bTicked!=GetLevel()->Ticked && !Owner->bStatic && !Owner->bDeleteMe )
	{
		GetLevel()->NewlySpawned = new(GEngineMem)FActorLink(this,GetLevel()->NewlySpawned);
		return 0;
	}
	return 1;
	unguardSlow;
}

UBOOL APawn::CheckOwnerUpdated()
{
	guardSlow(APawn::CheckOwnerUpdated);
	if( Owner && (INT)Owner->bTicked!=GetLevel()->Ticked && !Owner->bStatic && !Owner->bDeleteMe )
	{
		GetLevel()->NewlySpawned = new(GEngineMem)FActorLink(this,GetLevel()->NewlySpawned);
		return 0;
	}
	// Handle controller-first updating.
	if( Controller && !Controller->bDeleteMe && (INT)Controller->bTicked!=GetLevel()->Ticked )
	{
		GetLevel()->NewlySpawned = new(GEngineMem)FActorLink(this,GetLevel()->NewlySpawned);
		return 0;
	}
	return 1;
	unguardSlow;
}

void ALineOfSightTrigger::TickAuthoritative( FLOAT DeltaSeconds )
{
	guardSlow(ALineOfSightTrigger::TickAuthoritative);

	AActor::TickAuthoritative(DeltaSeconds);

	if ( bEnabled && !bTriggered && SeenActor
		&& (SeenActor->LastRenderTime >= OldTickTime) )
	{
		// find player pawn, and check if close enough
		for ( AController *C=Level->ControllerList; C; C=C->nextController )
		{
			if ( C->LocalPlayerController() )
			{
			    APlayerController *PC = C->GetAPlayerController();
			    if ( (PC->GetViewTarget()->Location - Location).SizeSquared() < MaxViewDist*MaxViewDist )
			    {
				    // check if looking directly enough at target
				    FVector Dir = (SeenActor->Location - PC->ViewTarget->Location).SafeNormal();
				    if ( (PC->Rotation.Vector() | Dir) > RequiredViewDir )
				    {
					    FCheckResult Hit(1.f);
					    GetLevel()->SingleLineCheck( Hit, this, SeenActor->Location, PC->ViewTarget->Location, TRACE_World|TRACE_StopAtFirstHit );
					    if ( !Hit.Actor )
						    eventPlayerSeesMe(PC);
				    }
				    break;
			    }
		    }
	    }
    }
	OldTickTime = Level->TimeSeconds;

	unguardSlow;
}

void AWeapon::TickAuthoritative( FLOAT DeltaSeconds )
{
	guardSlow(AWeapon::TickAuthoritative);

    AActor::TickAuthoritative( DeltaSeconds );

    if( bDeleteMe )
        return;

    if ( Instigator && Instigator->Weapon == this && !Instigator->bDeleteMe && Instigator->Controller != NULL && !Instigator->Controller->bDeleteMe 
		&& !Instigator->bNoWeaponFiring )
    {
        eventWeaponTick(DeltaSeconds);
        // client & server: update mode timing
        for (INT mode=0; mode<UCONST_NUM_FIRE_MODES; mode++ )
        {
            if (FireMode[mode] != NULL)
            {
				FireMode[mode]->ModeTick( DeltaSeconds );

                if (!Instigator || !Instigator->Controller) // pawn was killed during mode tick (ie: shieldgun against the wall)
                    return;

                if (Role == ROLE_Authority)
                {
                    if (FireMode[mode]->bServerDelayStartFire)
                    {
                        if (FireMode[mode]->NextFireTime <= Level->TimeSeconds + FireMode[mode]->PreFireTime)
                            eventServerStartFire(mode);
                    }
                    else if (FireMode[mode]->bServerDelayStopFire)
                    {
                        FireMode[mode]->bServerDelayStopFire = false;
                        //debugf(TEXT("ServerDelayStopFire %f"), Level->TimeSeconds);
                        eventStopFire(mode);
                    }
                }
            }
        }

        // client side only: determine when firing starts and stops
		if ( Instigator->IsLocallyControlled() && !bEndOfRound )
		{
			if ( (ClientState == WS_None) || (ClientState == WS_Hidden) )
			{
				debugf(TEXT("%s ClientState was WRONG! (%d)"),GetName(),ClientState);
				ClientState = WS_ReadyToFire;
			}
			
			if ( ClientState == WS_ReadyToFire )
			{
				UBOOL bAltFire = Instigator->Controller->bAltFire;
				UBOOL bFire = Instigator->Controller->bFire;
				AWeapon *MyDefault = Cast<AWeapon>(GetClass()->GetDefaultActor());
				if ( MyDefault && MyDefault->ExchangeFireModes )
				{
					Exchange(bFire,bAltFire);
				}

				if (FireMode[0] != NULL)
				{
					if (FireMode[0]->bIsFiring && !bFire)
					{
						eventClientStopFire(0);
					}
					else if (!FireMode[0]->bIsFiring && bFire)
					{
						eventClientStartFire(0);
					}
				}

				if (FireMode[1] != NULL)
				{
					if (FireMode[1]->bIsFiring && !bAltFire)
					{
						eventClientStopFire(1);
					}
					else if (!FireMode[1]->bIsFiring && bAltFire)
					{                                       
						eventClientStartFire(1);
					}
				}
			}
			else if ( TimerRate<=0.f )
			{
				debugf(TEXT("%s no timer running with clientstate %d"),GetName(),ClientState);
				TimerRate = 0.3f;
			}
		}
    }

    unguardSlow;
}

void UWeaponFire::ModeTick( FLOAT DeltaSeconds )
{
	guard(UWeaponFire::ModeTick);

	if ( !Weapon || Weapon->bDeleteMe || !Instigator || Instigator->bDeleteMe )
		return;

	// WeaponFire timer
	if ( TimerInterval != 0.f )
	{
		if ( NextTimerPop < Level->TimeSeconds )
		{
			eventTimer();
			if ( bTimerLoop )
				NextTimerPop = NextTimerPop + TimerInterval;
			else
				TimerInterval = 0.f;
		}
	}

	eventModeTick(DeltaSeconds);

	FLOAT CurrentTime = Weapon->Level->TimeSeconds;

    if ( (bIsFiring && !bFireOnRelease)
        || ((bInstantStop || !bIsFiring) && bFireOnRelease)
        || (HoldTime > MaxHoldTime && MaxHoldTime > 0.0f) )
    {
        if (CurrentTime > NextFireTime && (bInstantStop || !bNowWaiting) )
		{
           eventModeDoFire();

            if (bWaitForRelease)
                bNowWaiting = true;
		}
    }
    else if (bWaitForRelease && CurrentTime >= NextFireTime)
    {
        bNowWaiting = false;

        if (HoldTime == 0.0f)
            eventModeHoldFire();

        HoldTime += DeltaSeconds;
    }
	bInstantStop = false;
	unguard;
}

void AActor::TickAuthoritative( FLOAT DeltaSeconds )
{
	guardSlow(AActor::TickAuthoritative);

	// Tick the nonplayer.
	//clockSlow(GStats.DWORDStats(GEngineStats.STATS_Game_ScriptTickCycles));
	eventTick(DeltaSeconds);
	//unclockSlow(GStats.DWORDStats(GEngineStats.STATS_Game_ScriptTickCycles));

	// Update the actor's script state code.
	ProcessState( DeltaSeconds );

	UpdateTimers(DeltaSeconds );

	// Update LifeSpan.
	if( LifeSpan!=0.f )
	{
		LifeSpan -= DeltaSeconds;
        if( bAttenByLife ) // sjs - attenuate light by lifespan
        {
            LightBrightness = (BYTE)((float)LightBrightness*(LifeSpan / GetClass()->GetDefaultActor()->LifeSpan));
            if( LifeSpan < 1.0f )
                ScaleGlow = LifeSpan;
        }
		if( LifeSpan <= 0.0001f )
		{
			// Actor's LifeSpan expired.
			GetLevel()->DestroyActor( this );
			return;
		}
	}

	// Perform physics.
	if ( !bDeleteMe && (Physics!=PHYS_None) && (Role!=ROLE_AutonomousProxy) )
		performPhysics( DeltaSeconds );
	unguardSlow;
}

void AActor::TickSimulated( FLOAT DeltaSeconds )
{
	guardSlow(AActor::TickSimulated);

	TickAuthoritative(DeltaSeconds);
	unguardSlow;
}

void APawn::TickSimulated( FLOAT DeltaSeconds )
{
	guardSlow(APawn::TickSimulated);

	// Simulated Physics for pawns
	// simulate gravity
	if ( bHardAttach )
	{
		Acceleration = FVector(0.f,0.f,0.f);
		if ( (Physics == PHYS_Karma) || (Physics == PHYS_KarmaRagDoll) )
			setPhysics(PHYS_None);
		else
			Physics = PHYS_None;
	}
	else if ( (Physics != PHYS_Karma) && (Physics != PHYS_KarmaRagDoll) )
	{
		Acceleration = Velocity.SafeNormal();
		if ( PhysicsVolume->bWaterVolume )
			Physics = PHYS_Swimming;
		else if ( bCanClimbLadders && PhysicsVolume->IsA(ALadderVolume::StaticClass()) )
			Physics = PHYS_Ladder;
		else if ( bSimulateGravity )
			Physics = PHYS_Walking;	// set physics mode guess for use by animation
		else 
			Physics = PHYS_Flying;

		//simulated pawns just predict location, no script execution
		moveSmooth(Velocity * DeltaSeconds);

		// if simulated gravity, check if falling
		if ( bSimulateGravity && !bSimGravityDisabled )
		{
			FCheckResult Hit(1.f);
			if ( Velocity.Z == 0.f )
				GetLevel()->SingleLineCheck(Hit, this, Location - FVector(0.f,0.f,1.5f * CollisionHeight), Location, TRACE_AllBlocking, FVector(CollisionRadius,CollisionRadius,4.f));
			else if ( Velocity.Z < 0.f )
				GetLevel()->SingleLineCheck(Hit, this, Location - FVector(0.f,0.f,8.f), Location, TRACE_AllBlocking, GetCylinderExtent());

			if ( (Hit.Time == 1.f) || (Hit.Normal.Z < UCONST_MINFLOORZ) )
			{
				if ( Velocity.Z == 0.f )
					Velocity.Z = 0.15f * PhysicsVolume->Gravity.Z;
				Velocity += 0.5f * PhysicsVolume->Gravity * DeltaSeconds;
				Physics = PHYS_Falling; 
			}
			else
				Velocity.Z = 0.f;
		}
	}
	// Tick the nonplayer.
	//clockSlow(GStats.DWORDStats(GEngineStats.STATS_Game_ScriptTickCycles));
	eventTick(DeltaSeconds);
	//unclockSlow(GStats.DWORDStats(GEngineStats.STATS_Game_ScriptTickCycles));

	unguardSlow;
}

void AActor::TickSpecial( FLOAT DeltaSeconds )
{
}

void APawn::TickSpecial( FLOAT DeltaSeconds )
{
	guardSlow(APawn::TickSpecial);

	// assume dead if bTearOff
	if ( bTearOff && !bPlayedDeath )
		eventPlayDying(HitDamageType,TakeHitLocation);

	if ( !bInterpolating && bPhysicsAnimUpdate && Mesh )
		UpdateMovementAnimation(DeltaSeconds);

	// update weapon location (in case its playing sounds, etc.)
	// bAttachedMove = true (last switch in FarMoveActor) for weapon.
	if ( Weapon && !Weapon->bDeleteMe ) 
	{
		if ( !IsLocallyControlled() || !IsHumanControlled() 
			|| (Controller->GetViewTarget() != this) || ((APlayerController *)Controller)->bBehindView )
			GetLevel()->FarMoveActor( Weapon, Location, 0, 0, 1 ); 
	}
	// update eyeheight if someone is viewing through this pawn's eyes
	if ( bUpdateEyeheight || IsHumanControlled() )
		eventUpdateEyeHeight(DeltaSeconds);

	if( (Role==ROLE_Authority) && (BreathTime > 0.f) )
	{
		BreathTime -= DeltaSeconds;
		if (BreathTime < 0.001f)
		{
			BreathTime = 0.0f;
			eventBreathTimer();
		}
	}
	unguardSlow;
}

UBOOL AActor::PlayerControlled()
{
	return 0;
}

UBOOL APawn::PlayerControlled()
{
	return ( IsLocallyControlled() );
}

// sjs ---
void AActor::UpdateOverlay(FLOAT DeltaSeconds)
{
    if( OverlayMaterial )
    {
        if( ClientOverlayTimer != OverlayTimer )
        {
            ClientOverlayTimer = OverlayTimer;
            ClientOverlayCounter = OverlayTimer;
        }
        if( ClientOverlayCounter > 0.0f )
        {
            ClientOverlayCounter -= DeltaSeconds;
            if( ClientOverlayCounter <= 0.0f ) // count down
                ClientOverlayCounter = 0.0f;
        }
        else
        {
            ClientOverlayCounter += DeltaSeconds;
            if( ClientOverlayCounter >= 0.0f ) // count up
                ClientOverlayCounter = 0.0f;
        }
        if( ClientOverlayCounter == 0.0 )
		{
			if ( Role == ROLE_Authority )
				OverlayTimer = 0.f;
			ClientOverlayTimer = 0.f;
            OverlayMaterial = NULL;
		}
		else if ( (Role == ROLE_Authority) && (Abs(OverlayTimer - ClientOverlayCounter) > 1.f) )
		{
			ClientOverlayTimer = ClientOverlayCounter;
			OverlayTimer = ClientOverlayCounter;
		}
    }
}
// --- sjs

UBOOL AActor::Tick( FLOAT DeltaSeconds, ELevelTick TickType )
{
	guard(AActor::Tick);

	// Ignore actors in stasis
	if
	(	bStasis 
	&&	((Physics==PHYS_None) || (Physics == PHYS_Rotating))
	&&	((Level->NetMode == NM_DedicatedServer) || (Level->TimeSeconds - LastRenderTime > 5)) )
	{
		bTicked = GetLevel()->Ticked;
		return 1;
	}

	// Handle owner-first updating.
	if ( !CheckOwnerUpdated() )
		return 0;
	bTicked = GetLevel()->Ticked;

	// Non-player update.
	if( (TickType==LEVELTICK_ViewportsOnly) && !PlayerControlled() )
		return 1;

    UpdateOverlay(DeltaSeconds); //sjs

	// Update all animation, including multiple passes if necessary.
	// Meshes are allowed autonomous updating.	
	UpdateAnimation(DeltaSeconds);

	// This actor is tickable.
	if( RemoteRole == ROLE_AutonomousProxy ) 
	{
        APlayerController *PC = GetTopPlayerController();//Cast<APlayerController>(GetTopOwner());
		if ( (PC && PC->LocalPlayerController()) || Physics == PHYS_Karma )
			TickAuthoritative(DeltaSeconds);
		else
		{
			eventTick(DeltaSeconds);

			// Update the actor's script state code.
			ProcessState( DeltaSeconds );
			// Server handles timers for autonomous proxy.
			UpdateTimers( DeltaSeconds );
		}
	}
	else if ( Role>ROLE_SimulatedProxy )
		TickAuthoritative(DeltaSeconds);
	else if ( Role == ROLE_SimulatedProxy )
		TickSimulated(DeltaSeconds);
	else if ( !bDeleteMe && ((Physics == PHYS_Falling) || (Physics == PHYS_Rotating) || (Physics == PHYS_Projectile)) ) // dumbproxies simulate falling if client side physics set
		performPhysics( DeltaSeconds );

	if ( bDeleteMe )
		return 1;

	TickSpecial(DeltaSeconds);	// perform any tick functions unique to an actor subclass
	return 1;
	unguard;
}


/* Controller Tick
Controllers are never animated, and do not look for an owner to be ticked before them
Non-player controllers don't support being an autonomous proxy
*/
UBOOL AController::Tick( FLOAT DeltaSeconds, ELevelTick TickType )
{
	guard(AController::Tick);

	// Ignore controllers with no pawn, or pawn is in stasis
	if
	(	bStasis
	|| (TickType==LEVELTICK_ViewportsOnly) 
	|| (Pawn && Pawn->bStasis 
	&&	((Pawn->Physics==PHYS_None) || (Pawn->Physics == PHYS_Rotating))
	&&	(GetLevel()->TimeSeconds - Pawn->LastRenderTime > 5)
	&&	(Level->NetMode == NM_Standalone)) )
	{
		bTicked = GetLevel()->Ticked;
		return 1;
	}

	bTicked = GetLevel()->Ticked;

	if( Role>=ROLE_SimulatedProxy )
		TickAuthoritative(DeltaSeconds);
	
	// Update eyeheight and send visibility updates
	// with PVS, monsters look for other monsters, rather than sending msgs

	if( Role==ROLE_Authority && TickType==LEVELTICK_All )
	{
		if( SightCounter < 0.0f )
		{
			if( IsProbing(NAME_EnemyNotVisible) )
			{
				CheckEnemyVisible();
				SightCounter = 0.05f + 0.1f * appFrand();
			}
			else
				SightCounter += 0.15f + 0.1f * appFrand();
		}

		SightCounter = SightCounter - DeltaSeconds; 
		// for best performance, players show themselves to players and non-players (e.g. monsters),
		// and monsters show themselves to players
		// but monsters don't show themselves to each other
		// also

		if ( WarningProjectile && !WarningProjectile->bDeleteMe && (Level->TimeSeconds > WarningDelay) )
		{
			eventDelayedWarning();
			WarningProjectile = NULL;
		}

		if( Pawn && !Pawn->bHidden && !Pawn->bAmbientCreature )
			ShowSelf();
	}

	if ( Pawn )
	{
		// rotate pawn toward focus
		if ( Pawn->bRotateToDesired )
			Pawn->rotateToward(Focus, FocalPoint);

		// face same direction as pawn
		Rotation = Pawn->Rotation;
	}
	if ( MonitoredPawn )
	{
		if ( !Pawn || MonitoredPawn->bDeleteMe || !MonitoredPawn->Controller )
			eventMonitoredPawnAlert();
		else if ( !Pawn->SharingVehicleWith(MonitoredPawn) )
		{
			// quit if further than max dist, moving away fast, or has moved far enough
			if ( ((MonitoredPawn->Location - Pawn->Location).SizeSquared() > MonitorMaxDistSq)
				|| ((MonitoredPawn->Location - MonitorStartLoc).SizeSquared() > 0.25f * MonitorMaxDistSq) )
				eventMonitoredPawnAlert();
			else if ( (MonitoredPawn->Velocity.SizeSquared() > 0.6f * MonitoredPawn->GroundSpeed)
				&& ((MonitoredPawn->Velocity | (MonitorStartLoc - Pawn->Location)) > 0.f)
				&& ((MonitoredPawn->Location - Pawn->Location).SizeSquared() > 0.25f * MonitorMaxDistSq) )
				eventMonitoredPawnAlert();
		}
	}
	return 1;
	unguard;
}

/* 
PlayerControllers
Controllers are never animated, and do not look for an owner to be ticked before them
*/
UBOOL APlayerController::Tick( FLOAT DeltaSeconds, ELevelTick TickType )
{
	guard(APlayerController::Tick);

	bTicked = GetLevel()->Ticked;

	TimeSinceLastFogChange += DeltaSeconds;

	GetViewTarget();
	if( (RemoteRole == ROLE_AutonomousProxy) && !LocalPlayerController() ) 
	{
		// kick idlers
		if ( PlayerReplicationInfo )
		{
			if ( (Pawn && (!Level->bKickLiveIdlers || (Pawn->Physics != PHYS_Walking)) ) || !bShortConnectTimeOut || (PlayerReplicationInfo->bOnlySpectator && (ViewTarget != this)) || PlayerReplicationInfo->bOutOfLives 
			|| Level->Pauser || (Level->Game && (Level->Game->bWaitingToStartMatch || Level->Game->bGameEnded || (Level->Game->NumPlayers < 2))) || PlayerReplicationInfo->bAdmin )
			{
				LastActiveTime = Level->TimeSeconds;
			}
			else if ( (Level->Game->MaxIdleTime > 0) && (Level->TimeSeconds - LastActiveTime > Level->Game->MaxIdleTime - 10) )
			{
				if ( Level->TimeSeconds - LastActiveTime > Level->Game->MaxIdleTime )
				{
					Level->Game->eventKickIdler(this);
					LastActiveTime = Level->TimeSeconds - Level->Game->MaxIdleTime + 3.f;
				}
				else
					eventKickWarning();
			}
		}

		// force physics update for clients that aren't sending movement updates in a timely manner 
		// this prevents cheats associated with artificially induced ping spikes
		if ( Pawn && !Pawn->bDeleteMe && (Pawn->Physics!=PHYS_None) && (Pawn->Physics != PHYS_Karma) && (Level->TimeSeconds - ServerTimeStamp > ::Max(DeltaSeconds+0.06f,0.25f)) 
			&& (ServerTimeStamp != 0.f) )
		{
			// force position update
			if ( !Pawn->Velocity.IsZero() )
			{
				Pawn->performPhysics( Level->TimeSeconds - ServerTimeStamp );
			}
			ServerTimeStamp = Level->TimeSeconds;
			TimeMargin = 0.f;
			MaxTimeMargin = Level->MaxTimeMargin;
		}

		// update viewtarget replicated info
		if( ViewTarget != Pawn )
		{
            APawn* TargetPawn = ViewTarget ? ViewTarget->GetAPawn() : NULL; //Cast<APawn>(ViewTarget);
			if ( TargetPawn )
			{
				if ( TargetPawn->Controller && TargetPawn->Controller->GetAPlayerController() )
					TargetViewRotation = TargetPawn->Controller->Rotation;
				else
					TargetViewRotation = TargetPawn->Rotation;
				TargetEyeHeight = TargetPawn->BaseEyeHeight;
			}
		}

		// Update the actor's script state code.
		ProcessState( DeltaSeconds );
		// Server handles timers for autonomous proxy.
		UpdateTimers( DeltaSeconds );

		// send ClientAdjustment if necessary
		if ( PendingAdjustment.TimeStamp > 0.f )
			eventSendClientAdjustment();
	}
	else if( Role>=ROLE_SimulatedProxy )
	{
		// Player update.
		if( IsA(ACamera::StaticClass()) && !(ShowFlags & SHOW_PlayerCtrl) )
			return 1;

		// Process PlayerTick with input.
		if ( Player )
		{
			if ( !PlayerInput )
				eventInitInputSystem();
			if ( PlayerInput )
			{
				Player->ReadInput( DeltaSeconds );
				eventPlayerTick( DeltaSeconds );
				Player->ReadInput( -1.0f );
			}
		}

		// Update the actor's script state code.
		ProcessState( DeltaSeconds );

		UpdateTimers( DeltaSeconds );

		if ( bDeleteMe )
			return 1;

		// Perform physics.
		if( Physics!=PHYS_None && Role!=ROLE_AutonomousProxy )
			performPhysics( DeltaSeconds );
	}

	// During demo playback, setup view offsets for viewtarget
	if( GetLevel()->DemoRecDriver && GetLevel()->DemoRecDriver->ServerConnection )
	{
		if( Role == ROLE_Authority )
		{
			// update viewtarget replicated info
			if( !bBehindView )
			{
				APawn* TargetPawn = ViewTarget ? ViewTarget->GetAPawn() : NULL; //Cast<APawn>(ViewTarget);
				if ( TargetPawn )
				{
					TargetViewRotation = Rotation;
					TargetEyeHeight = TargetPawn->EyeHeight;
				}
			}
		}
	}

	// Update eyeheight and send visibility updates
	// with PVS, monsters look for other monsters, rather than sending msgs
	if( Role==ROLE_Authority && TickType==LEVELTICK_All )
	{
		if( SightCounter < 0.0f )
			SightCounter += 0.2f;

		//SightCounter = SightCounter - DeltaSeconds; // uncomment this line if PlayerControllers should receive SeePlayer() or SeeMonster() events
		if( Pawn && !Pawn->bHidden )
			ShowSelf();
	}

	return 1;
	unguard;
}

void AActor::UpdateTimers(FLOAT DeltaSeconds)
{
	guardSlow(AActor::UpdateTimers);

	// Update timers.
	if( TimerRate>0.0f && (TimerCounter+=DeltaSeconds)>=TimerRate )
	{
		// Normalize the timer count.
		INT TimerTicksPassed = 1;
		if( TimerRate > 0.0f )
		{
			TimerTicksPassed     = (int)(TimerCounter/TimerRate);
			TimerCounter -= TimerRate * TimerTicksPassed;
			if( TimerTicksPassed && !bTimerLoop )
			{
				// Only want a one-shot timer message.
				TimerTicksPassed = 1;
				TimerRate = 0.0f;
			}
		}

		// Call timer routine with count of timer events that have passed.
		eventTimer();
	}

	unguardSlow;
}

/*-----------------------------------------------------------------------------
	Network client tick.
-----------------------------------------------------------------------------*/

void ULevel::TickNetClient( FLOAT DeltaSeconds )
{
	guard(ULevel::TickNetClient);
	clock(GStats.DWORDStats(GEngineStats.STATS_Game_NetTickCycles));
	if( NetDriver->ServerConnection->State==USOCK_Open )
	{
		/* Don't replicate any properties from client to server
		for( TMap<AActor*,UActorChannel*>::TIterator ItC(NetDriver->ServerConnection->ActorChannels); ItC; ++ItC )
		{
			guard(UpdateLocalActors);
			UActorChannel* It = ItC.Value();
			APawn* PlayerPawn = It->GetActor() ? It->GetActor()->GetAPawn() : NULL; // Cast<APawn>(It->GetActor());
			if( PlayerPawn && PlayerPawn->Controller )
			{
				APlayerController* PC = PlayerPawn->Controller->GetAPlayerController();// Cast<APlayerController>(PlayerPawn->Controller);
				if ( PC && PC->Player )
					It->ReplicateActor();
			}
			unguard;
		}
		*/
	}
	else if( NetDriver->ServerConnection->State==USOCK_Closed )
	{
		// Server disconnected.

		// Determine if it's a map change or disconnect

		UGameEngine* GE = Cast<UGameEngine>(Engine);

		if (!(GE && GE->GPendingLevel))
		{
			if( GE->Client && GE->Client->Viewports.Num() && GE->Client->Viewports(0)->GUIController )			
			{
				if ( !GE->Client->Viewports(0)->GUIController->bActive )
				{
					GE->SetProgress( *FString::Printf(TEXT("menu:%s"),*GE->DisconnectMenuClass), LocalizeError(TEXT("ConnectLost"),TEXT("Engine")), TEXT("") );			
				}
				else
					GE->SetProgress( TEXT(""), LocalizeError(TEXT("ConnectLost"),TEXT("Engine")), TEXT("") );			
			}
		}

		check(Engine->Client->Viewports.Num());
		Engine->SetClientTravel( Engine->Client->Viewports(0), TEXT("?closed"), 0, TRAVEL_Absolute );
	}
	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_NetTickCycles));
	unguard;
}

/*-----------------------------------------------------------------------------
	Network server ticking individual client.
-----------------------------------------------------------------------------*/
UBOOL AActor::IsNetRelevantFor( APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation )
{
	guard(AActor::IsNetRelevantFor);
	if( bAlwaysRelevant || IsOwnedBy(Viewer) || IsOwnedBy(RealViewer) || this==Viewer || Viewer==Instigator || RealViewer->bAllActorsRelevant )
		return 1;
	else if( AmbientSound 
			&& ((Location-Viewer->Location).SizeSquared() < Square(GAudioMaxRadiusMultiplier*SoundRadius)) )
		return 1;
	else if ( (Owner && (Base == Owner) && !bOnlyOwnerSee)
			|| (Base && (AttachmentBone != NAME_None) && Cast<USkeletalMesh>(Base->Mesh)) )
		return Base->IsNetRelevantFor( RealViewer, Viewer, SrcLocation );
	else if( (bHidden || bOnlyOwnerSee) && !bBlockActors && !AmbientSound )
		return 0;
	else
	{
		// check distance fog
		FLOAT DistSq = (Location - SrcLocation).SizeSquared();
		if ( Region.Zone->bDistanceFog && (DistSq > Region.Zone->DistanceFogEnd * Region.Zone->DistanceFogEnd) )
			return 0;
		if ( GetLevel()->Model->FastLineCheck(Location,SrcLocation) )
		{
			if ( RealViewer->bWasSaturated && (CullDistance > 0.f) && (DistSq > 0.8f * CullDistance) )
			{
				// check against terrain
				AZoneInfo* Z = Viewer->Region.Zone;
				if( Z && Z->bTerrainZone ) 
				{
					FVector End = Location;
					End.Z += 2.f * CollisionHeight;
					FCheckResult Hit(1.f);
					for( INT t=0;t<Z->Terrains.Num();t++ )
					{
						if( Z->Terrains(t)->LineCheck( Hit, End, SrcLocation, FVector(0.f,0.f,0.f), 0, 0 )==0 )
						{
							FPointRegion HitRegion = GetLevel()->Model->PointRegion( GetLevel()->GetLevelInfo(), Hit.Location );
							if( HitRegion.Zone == Z )
							{
								return 0;
							}
						}
					}
				} 
			}
			/*
			// check against antiportal volumes
			FCheckResult Hit(1.f);
			for ( INT i=0; i<GetLevel()->AntiPortals.Num(); i++ )
				if ( GetLevel()->AntiPortals(i) && GetLevel()->AntiPortals(i)->GetPrimitive()->LineCheck( Hit, GetLevel()->AntiPortals(i), Location, SrcLocation, FVector(0.f,0.f,0.f), 0, TRACE_StopAtFirstHit )==0 )
					return 0;
			*/
			return 1;
		}
		return 0;
	}
	unguard;
}

UBOOL APlayerController::IsNetRelevantFor( APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation )
{
	guardSlow(APlayerController::IsNetRelevantFor);

	return ( this==RealViewer );
	unguardSlow;
}

UBOOL APawn::CacheNetRelevancy(UBOOL bIsRelevant, APlayerController* RealViewer, AActor* Viewer)
{
	guardSlow(APawn::CacheNetRelevancy);

	bCachedRelevant = bIsRelevant;
	NetRelevancyTime = Level->TimeSeconds;
	LastRealViewer = RealViewer;
	LastViewer = Viewer;
	return bIsRelevant;
	unguardSlow;
}

UBOOL APawn::IsNetRelevantFor( APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation )
{
	guardSlow(APawn::IsNetRelevantFor);

	if ( (NetRelevancyTime == Level->TimeSeconds) && (RealViewer == LastRealViewer) && (Viewer == LastViewer) )
		return bCachedRelevant;
	if( bAlwaysRelevant || IsOwnedBy(Viewer) || IsOwnedBy(RealViewer) || this==Viewer || Viewer==Instigator
		|| IsBasedOn(Viewer) || (Viewer && Viewer->IsBasedOn(this)) || RealViewer->bAllActorsRelevant )
		return CacheNetRelevancy(true,RealViewer,Viewer);
	if( AmbientSound 
			&& ((Location-Viewer->Location).SizeSquared() < Square(GAudioMaxRadiusMultiplier*SoundRadius)) )
		return CacheNetRelevancy(true,RealViewer,Viewer);
	else if( (bHidden || bOnlyOwnerSee) && !bBlockActors && !AmbientSound )
		return CacheNetRelevancy(false,RealViewer,Viewer);
	else if ( (Owner && (Base == Owner) && !bOnlyOwnerSee)
			|| (Base && (AttachmentBone != NAME_None) && Cast<USkeletalMesh>(Base->Mesh)) )
		return Base->IsNetRelevantFor( RealViewer, Viewer, SrcLocation );
	else
	{
		// check distance fog
		if ( Region.Zone->bDistanceFog && ((Location - SrcLocation).SizeSquared() > (CollisionRadius + Region.Zone->DistanceFogEnd) * (CollisionRadius + Region.Zone->DistanceFogEnd)) )
			return CacheNetRelevancy(false,RealViewer,Viewer);

		// check against BSP - check head and center
		//debugf(TEXT("Check relevance of %s"),*(PlayerReplicationInfo->PlayerName));
		if ( !GetLevel()->Model->FastLineCheck(Location + FVector(0.f,0.f,BaseEyeHeight),SrcLocation) 
			 && !GetLevel()->Model->FastLineCheck(Location,SrcLocation)  )
			return CacheNetRelevancy(false,RealViewer,Viewer);
		/*
		if ( CollisionRadius <= COMMONRADIUS )
			return 0;

		// if large collision volume, check edges
		if ( GetLevel()->Model->FastLineCheck(Location + FVector(0.f,CollisionRadius,0.f),SrcLocation) )
			return CacheNetRelevancy(true,RealViewer,Viewer);
		if ( GetLevel()->Model->FastLineCheck(Location - FVector(0.f,CollisionRadius,0.f),SrcLocation) )
			return CacheNetRelevancy(true,RealViewer,Viewer);
		if ( GetLevel()->Model->FastLineCheck(Location + FVector(CollisionRadius,0.f,0.f),SrcLocation) )
			return CacheNetRelevancy(true,RealViewer,Viewer);
		if ( GetLevel()->Model->FastLineCheck(Location - FVector(CollisionRadius,0.f,0.f),SrcLocation) )
			return CacheNetRelevancy(true,RealViewer,Viewer);
		return 0;
		*/
		if ( RealViewer->bWasSaturated )
		{
			// check against terrain
			// FIXME - Jack, it would be nice to make this faster (a linecheck that just returns whether there was a hit)
			AZoneInfo* Z = Viewer->Region.Zone;
			if( Z && Z->bTerrainZone ) 
			{
				FVector End = Location;
				End.Z += 1.5f * CollisionHeight;
				FCheckResult Hit(1.f);
				for( INT t=0;t<Z->Terrains.Num();t++ )
				{
					if( (Z->Terrains(t)->LineCheck( Hit, End, SrcLocation, FVector(0.f,0.f,0.f), 0, 0 )==0)
						&& (Z->Terrains(t)->LineCheck( Hit, Location, SrcLocation, FVector(0.f,0.f,0.f), 0, 0 )==0) )
					{
						FPointRegion HitRegion = GetLevel()->Model->PointRegion( GetLevel()->GetLevelInfo(), Hit.Location );
						if( HitRegion.Zone == Z )
						{
							return CacheNetRelevancy(false,RealViewer,Viewer);
						}
					}
				}
			} 
		}
		/*
		// check against antiportal volumes
		FCheckResult Hit(1.f);
		for ( INT i=0; i<GetLevel()->AntiPortals.Num(); i++ )
			if ( GetLevel()->AntiPortals(i) 
				&& GetLevel()->AntiPortals(i)->GetPrimitive()->LineCheck( Hit, GetLevel()->AntiPortals(i), Location + FVector(0.f,0.f,CollisionHeight), SrcLocation, FVector(0.f,0.f,0.f), 0, TRACE_StopAtFirstHit )==0 )
				return CacheNetRelevancy(false,RealViewer,Viewer);
		*/
		return CacheNetRelevancy(true,RealViewer,Viewer);
	}
	unguardSlow;
}

UBOOL AVehicle::IsNetRelevantFor( APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation )
{
	guardSlow(AVehicle::IsNetRelevantFor);

	if ( bAlwaysRelevant )
		return true;
	if ( (NetRelevancyTime == Level->TimeSeconds) && (RealViewer == LastRealViewer) && (Viewer == LastViewer) )
		return bCachedRelevant;
	if( IsOwnedBy(Viewer) || IsOwnedBy(RealViewer) || this==Viewer || Viewer==Instigator
		|| IsBasedOn(Viewer) || (Viewer && Viewer->IsBasedOn(this)) || RealViewer->bAllActorsRelevant )
		return CacheNetRelevancy(true,RealViewer,Viewer);
	if( AmbientSound 
			&& ((Location-Viewer->Location).SizeSquared() < Square(GAudioMaxRadiusMultiplier*SoundRadius)) )
		return CacheNetRelevancy(true,RealViewer,Viewer);
	else if ( (Owner && (Base == Owner) && !bOnlyOwnerSee)
			|| (Base && (AttachmentBone != NAME_None) && Cast<USkeletalMesh>(Base->Mesh)) )
		return Base->IsNetRelevantFor( RealViewer, Viewer, SrcLocation );
	else if( (bHidden || bOnlyOwnerSee) && !bBlockActors && !AmbientSound )
		return CacheNetRelevancy(false,RealViewer,Viewer);
	else
	{
		FLOAT DistSq = (Location - SrcLocation).SizeSquared();

		// check distance fog
		if ( Region.Zone->bDistanceFog && (DistSq > (500.f + CollisionRadius + Region.Zone->DistanceFogEnd) * (500.f + CollisionRadius + Region.Zone->DistanceFogEnd)) )
			return CacheNetRelevancy(false,RealViewer,Viewer);

		if ( RealViewer->bWasSaturated && (CollisionRadius < 200.f) )
		{
			// check against terrain
			AZoneInfo* Z = Viewer->Region.Zone;
			if( Z && Z->bTerrainZone ) 
			{
				FVector End = Location;
				End.Z += 1.5f * CollisionHeight;
				FCheckResult Hit(1.f);
				for( INT t=0;t<Z->Terrains.Num();t++ )
				{
					if( (Z->Terrains(t)->LineCheck( Hit, End, SrcLocation, FVector(0.f,0.f,0.f), 0, 0 )==0)
						&& (Z->Terrains(t)->LineCheck( Hit, Location, SrcLocation, FVector(0.f,0.f,0.f), 0, 0 )==0) )
					{
						// check extreme edges
						FVector Right = (End - SrcLocation) ^ FVector(0.f,0.f,1.f);
						Right = Right.SafeNormal() * 1.25f * CollisionRadius;
						if ( (Z->Terrains(t)->LineCheck( Hit, End + Right, SrcLocation, FVector(0.f,0.f,0.f), 0, 0 )==0)
							&& (Z->Terrains(t)->LineCheck( Hit, End - Right, SrcLocation, FVector(0.f,0.f,0.f), 0, 0 )==0) )
						{
							FPointRegion HitRegion = GetLevel()->Model->PointRegion( GetLevel()->GetLevelInfo(), Hit.Location );
							if( HitRegion.Zone == Z )
							{
								return CacheNetRelevancy(false,RealViewer,Viewer);
							}
						}
					}
				} 
			}
		}

		return CacheNetRelevancy(true,RealViewer,Viewer);
	}
	unguardSlow;
}

INT ULevel::ServerTickClients( FLOAT DeltaSeconds )
{
	guard(ULevel::ServerTickClients);

	if ( NetDriver->ClientConnections.Num() == 0 )
		return 0;
	INT Updated=0;

	FMemMark Mark(GMem);
	// initialize connections
	for( INT i=NetDriver->ClientConnections.Num()-1; i>=0; i-- )
	{
		UNetConnection* Connection = NetDriver->ClientConnections(i);
		check(Connection);
		check(Connection->State==USOCK_Pending || Connection->State==USOCK_Open || Connection->State==USOCK_Closed);

		// Handle not ready channels.
		if( Connection->Actor && Connection->IsNetReady(0) && Connection->State==USOCK_Open 
			&& (Connection->Driver->Time - Connection->LastReceiveTime < 1.5f) )
		{
			Connection->Viewer = Connection->Actor->GetViewTarget();
			Connection->OwnedConsiderList = new(GMem,Actors.Num()-iFirstNetRelevantActor+2)AActor*;
			Connection->OwnedConsiderListSize = 0;
		}
		else
			Connection->Viewer = NULL;
	}
			
	// make list of actors to consider
	AActor **ConsiderList = new(GMem,Actors.Num()-iFirstNetRelevantActor+2)AActor*;
	INT ConsiderListSize = 0;

	// Add LevelInfo to considerlist
	if( Actors(0) && (Actors(0)->RemoteRole!=ROLE_None) )
	{
		ConsiderList[0] = Actors(0);
		ConsiderListSize++;
	}
	FLOAT ServerTickTime = Engine->GetMaxTickRate();
	if ( ServerTickTime == 0.f )
		ServerTickTime = DeltaSeconds;
	else
		ServerTickTime = 1.f/ServerTickTime;
//	debugf(TEXT("START LIST"));
	for( INT i=iFirstNetRelevantActor; i<Actors.Num(); i++ )
	{
		AActor* Actor = Actors(i);
/*
		if ( Actor && (Actor->RemoteRole != ROLE_None) )
		{
			debugf(TEXT(" Consider Replicating %s"),Actor->GetName());
		}
*/
		if( Actor 
			&& (Actor->RemoteRole!=ROLE_None) 
			&& (TimeSeconds > Actor->NetUpdateTime) ) 
		{
			Actor->NetUpdateTime = TimeSeconds + appFrand() * ServerTickTime + 1.f/Actor->NetUpdateFrequency; // FIXME - cache 1/netupdatefreq
			if ( Actor->bAlwaysRelevant || !Actor->bOnlyRelevantToOwner ) 
			{
				ConsiderList[ConsiderListSize] = Actor;
				ConsiderListSize++;
			}
			else
			{
				AActor* ActorOwner = Actor->Owner;
				if ( !ActorOwner && (Actor->GetAPlayerController() || Actor->GetAPawn()) ) 
					ActorOwner = Actor;
				if ( ActorOwner )
				{
					for ( INT j=0; j<NetDriver->ClientConnections.Num(); j++ )
					{
						UNetConnection* Connection = NetDriver->ClientConnections(j);
						if ( Connection->Viewer && ((ActorOwner == Connection->Viewer) || (ActorOwner == Connection->Actor)) )
						{
							Connection->OwnedConsiderList[Connection->OwnedConsiderListSize] = Actor;
							Connection->OwnedConsiderListSize++;
						}
					}
				}
			}
		}
	}

	UBOOL bCPUSaturated = (DeltaSeconds > 1.2f/Engine->GetMaxTickRate());

	for( INT i=NetDriver->ClientConnections.Num()-1; i>=0; i-- )
	{
		UNetConnection* Connection = NetDriver->ClientConnections(i);

		if( Connection->Viewer )
		{
			AActor*      Viewer    = Connection->Viewer;
			APlayerController* InViewer  = Connection->Actor;
			InViewer->Level->ReplicationViewer = InViewer;
			InViewer->Level->ReplicationViewTarget = Viewer;

			// Get list of visible/relevant actors.
			FLOAT PruneActors = 0.f;
			clock(PruneActors);
			FMemMark Mark(GMem);
			NetTag++;
			Connection->TickCount++;

			// Set up to skip all sent temporary actors.
			guard(SkipSentTemporaries);
			for( INT i=0; i<Connection->SentTemporaries.Num(); i++ )
				Connection->SentTemporaries(i)->NetTag = NetTag;
			unguard;

			// Get viewer coordinates.
			FVector      Location  = Viewer->Location;
			FRotator     Rotation  = InViewer->Rotation;
			InViewer->eventPlayerCalcView( Viewer, Location, Rotation );

			// Compute ahead-vectors for prediction.
			FVector Ahead = FVector(0,0,0);
			if( Connection->TickCount & 1 )
			{
				FLOAT PredictSeconds = (Connection->TickCount&2) ? 0.4f : 0.9f;
				Ahead = PredictSeconds * Viewer->Velocity;
				if( Viewer->Base )
					Ahead += PredictSeconds * Viewer->Base->Velocity;
				FCheckResult Hit(1.0f);
				Hit.Location = Location + Ahead;
				Viewer->GetLevel()->Model->LineCheck(Hit,NULL,Hit.Location,Location,FVector(0,0,0),NF_NotVisBlocking,0);
				Location = Hit.Location;
			}

			// Make list of all actors to consider.
			INT              ConsiderCount  = 0;
			FActorPriority*  PriorityList   = new(GMem,Actors.Num()-iFirstNetRelevantActor+2)FActorPriority;
			FActorPriority** PriorityActors = new(GMem,Actors.Num()-iFirstNetRelevantActor+2)FActorPriority*;
			FVector          ViewPos        = Viewer->Location;
			FVector          ViewDir        = InViewer->Rotation.Vector();
			UBOOL bLowNetBandwidth = !bCPUSaturated && (Connection->CurrentNetSpeed < 6000) && ( Connection->CurrentNetSpeed/FLOAT(Viewer->Level->Game->NumPlayers + Viewer->Level->Game->NumBots)
									< (Viewer->Level->Game->bAllowVehicles ? 500.f : 300.f) );
			InViewer->bWasSaturated = InViewer->bWasSaturated && bLowNetBandwidth;

			guard(MakeConsiderList);
			for( INT i=0; i<ConsiderListSize; i++ )
			{
				AActor* Actor = ConsiderList[i];
				if( Actor->NetTag!=NetTag )
				{
					//debugf(TEXT("Consider %s alwaysrelevant %d frequency %f "),Actor->GetName(), Actor->bAlwaysRelevant, Actor->NetUpdateFrequency);
					UActorChannel* Channel = Connection->ActorChannels.FindRef(Actor);
					if ( Actor->bOnlyDirtyReplication 
						&& Channel
						&& !Channel->ActorDirty
						&& Channel->Recent.Num() 
						&& Channel->Dirty.Num() == 0 )
					{
						Channel->RelevantTime = NetDriver->Time;
					}
					else
					{
						Actor->NetTag                 = NetTag;
						PriorityList  [ConsiderCount] = FActorPriority( ViewPos, ViewDir, Connection, Channel, Actor, InViewer, bLowNetBandwidth );
						PriorityActors[ConsiderCount] = PriorityList + ConsiderCount;
						ConsiderCount++;
					}
				}
			}
			for( INT i=0; i<Connection->OwnedConsiderListSize; i++ )
			{
				AActor* Actor = Connection->OwnedConsiderList[i];
				//debugf(TEXT("Consider owned %s alwaysrelevant %d frequency %f  "),Actor->GetName(), Actor->bAlwaysRelevant,Actor->NetUpdateFrequency);
				if( Actor->NetTag!=NetTag )
				{
					UActorChannel* Channel = Connection->ActorChannels.FindRef(Actor);
					if ( Actor->bOnlyDirtyReplication 
						&& Channel
						&& !Channel->ActorDirty
						&& Channel->Recent.Num() 
						&& Channel->Dirty.Num() == 0 )
					{
						Channel->RelevantTime = NetDriver->Time;
					}
					else
					{
						Actor->NetTag                 = NetTag;
						PriorityList  [ConsiderCount] = FActorPriority( ViewPos, ViewDir, Connection, Channel, Actor, InViewer, bLowNetBandwidth );
						PriorityActors[ConsiderCount] = PriorityList + ConsiderCount;
						ConsiderCount++;
					}
				}
			}
			Connection->OwnedConsiderList = NULL;
			Connection->OwnedConsiderListSize = 0;
			Connection->LastRepTime = Connection->Driver->Time;
			unguard;

/*			INT i = 0;
			for( TMap<AActor*,UActorChannel*>::TIterator ItC(Connection->ActorChannels); ItC; ++ItC )
			{
				i++;
				UActorChannel* It = ItC.Value();
				if ( It && It->GetActor() )
					debugf(TEXT("Channel %d is %s"),i,It->GetActor()->GetName());
			}
*/
			FLOAT RelevantTime = 0.f;
			// Sort by priority.
			guard(SortConsiderList);
			Sort( PriorityActors, ConsiderCount );
			unguard;
			unclock(PruneActors);

			// Update all relevant actors in sorted order.
			clock(RelevantTime);
			guard(UpdateRelevant);
			INT j;
			UBOOL bNewSaturated = false;
			//debugf(TEXT("START"));
			for( j=0; j<ConsiderCount; j++ )
			{
				UActorChannel* Channel     = PriorityActors[j]->Channel;
				//debugf(TEXT(" Maybe Replicate %s"),PriorityActors[j]->Actor->GetName());
				if ( !Channel || Channel->Actor ) //make sure didn't just close this channel
				{
					AActor*        Actor       = PriorityActors[j]->Actor;
					UBOOL          CanSee      = 0;
					// only check visibility on already visible actors every 1.0 + 0.5R seconds
					// bTearOff actors should never be checked
					if ( !Actor->bTearOff && (!Channel || NetDriver->Time-Channel->RelevantTime>1.f) )
						CanSee = Actor->IsNetRelevantFor( InViewer, Viewer, Location );
					if( CanSee || (Channel && NetDriver->Time-Channel->RelevantTime<NetDriver->RelevantTimeout) )
					{
						// Find or create the channel for this actor.
						GStats.DWORDStats(GEngineStats.STATS_Net_NumPV)++;
						if( !Channel && Connection->PackageMap->ObjectToIndex(Actor->GetClass())!=INDEX_NONE )
						{
							// Create a new channel for this actor.
							Channel = (UActorChannel*)Connection->CreateChannel( CHTYPE_Actor, 1 );
							if( Channel )
								Channel->SetChannelActor( Actor );
						}
						if( Channel )
						{
							if ( !Connection->IsNetReady(0) ) // here also???
							{
								bNewSaturated = true;
								break;
							}
							if( CanSee )
								Channel->RelevantTime = NetDriver->Time + 0.5f * appFrand();
							if( Channel->IsNetReady(0) )
							{
								//debugf(TEXT("Replicate %s"),Actor->GetName());
								Channel->ReplicateActor();
								Updated++;
							}
							else
								Actor->NetUpdateTime = TimeSeconds - 1.f;
							if ( !Connection->IsNetReady(0) )
							{
								bNewSaturated = true;
								break;
							}
						}
					}
					else if( Channel )
					{
						// Devnet diagnostic: why this channel was closed. Every input to the decision is
						// printed (relevance really false vs. the RelevantTime/Time arithmetic look the same
						// from outside). Capped, so it cannot flood the console.
						{
							static INT UfrontRelClose = 0;
							if( UfrontRelClose++ < 40 )
								debugf( NAME_DevNetTraffic,
									TEXT("[UFRONT-DIAG] RELEVANCE CLOSE #%i: actor=%s class=%s cansee=%i alwaysRel=%i onlyOwner=%i ")
									TEXT("relTime=%.2f driverTime=%.2f age=%.2f timeout=%.2f | viewer=%s realViewer=%s viewTarget=%s ")
									TEXT("connState=%i sinceRecv=%.2f"),
									UfrontRelClose,
									Actor->GetName(), Actor->GetClass()->GetName(), (INT)CanSee,
									(INT)Actor->bAlwaysRelevant, (INT)Actor->bOnlyRelevantToOwner,
									Channel->RelevantTime, NetDriver->Time,
									NetDriver->Time - Channel->RelevantTime, NetDriver->RelevantTimeout,
									Viewer ? Viewer->GetName() : TEXT("none"),
									InViewer ? InViewer->GetName() : TEXT("none"),
									(InViewer && InViewer->ViewTarget) ? InViewer->ViewTarget->GetName() : TEXT("none"),
									(INT)Connection->State,
									NetDriver->Time - Connection->LastReceiveTime );
						}
						Channel->Close();
						// skeletal attachments aren't relevant anymore either
						if ( Cast<USkeletalMesh>(Actor->Mesh) )
						{
							for ( INT k=0; k<Actor->Attached.Num(); k++ )
								if ( Actor->Attached(k) && !Actor->Attached(k)->bAlwaysRelevant 
									&& (Actor->Attached(k)->AttachmentBone != NAME_None) )
								{
									UChannel *AttachedChannel = Connection->ActorChannels.FindRef(Actor->Attached(k));
									if ( AttachedChannel )
										AttachedChannel->Close();
								}
						}
					}
				}
			}
			InViewer->bWasSaturated = false;
			for ( INT k=j; k<ConsiderCount; k++ )
			{
				AActor* Actor = PriorityActors[k]->Actor;
				if( Actor->IsNetRelevantFor( InViewer, Viewer, Location ) )
					Actor->NetUpdateTime = TimeSeconds - 1.f;
			}
			InViewer->bWasSaturated = bNewSaturated;
			unguard;
			Mark.Pop();
			unclock(RelevantTime);
			//debugf(TEXT("Potential %04i ConsiderList %03i ConsiderCount %03i Prune=%01.4f Relevance=%01.4f"),Actors.Num()-iFirstNetRelevantActor, 
			//			ConsiderListSize, ConsiderCount, PruneActors * GSecondsPerCycle * 1000.f,RelevantTime * GSecondsPerCycle * 1000.f);
		}
	}
	Mark.Pop();

	// Windowed replication summary: one line per 10 s window (the browser console keeps a bounded ring),
	// the same window as the other net accumulators; the peak is kept because an average hides a burst.
	{
		static DOUBLE UfrontRepWinStart = 0.0;
		static INT    UfrontRepActors = 0, UfrontRepPasses = 0, UfrontRepPeak = 0;
		DOUBLE RepNow = appSeconds();
		if( UfrontRepWinStart == 0.0 )
			UfrontRepWinStart = RepNow;
		if( Updated > 0 )
		{
			UfrontRepActors += Updated;
			UfrontRepPasses++;
			UfrontRepPeak = ::Max( UfrontRepPeak, Updated );
		}
		DOUBLE RepElapsed = RepNow - UfrontRepWinStart;
		if( RepElapsed >= 10.0 )
		{
			if( UfrontRepPasses > 0 )
				// The prefix is kept verbatim: the E2E's REPLICATING milestone matches it (see UnConn.cpp).
				printf("[NET-TRAFFIC] Server: Replicated %d actors to %d clients  [%.0fs window: %d passes, avg %.1f/pass, peak %d]\n",
					UfrontRepActors, NetDriver->ClientConnections.Num(),
					RepElapsed, UfrontRepPasses, (FLOAT)UfrontRepActors/UfrontRepPasses, UfrontRepPeak);
			UfrontRepWinStart = RepNow;
			UfrontRepActors = UfrontRepPasses = UfrontRepPeak = 0;
		}
	}

	return Updated;
	unguard;
}

/*-----------------------------------------------------------------------------
	Network server tick.
-----------------------------------------------------------------------------*/

void UNetConnection::SetActorDirty(AActor* DirtyActor )
{
	guardSlow(UNetConnection::SetActorDirty);
	if( Actor && State==USOCK_Open )
	{
		UActorChannel* Channel = ActorChannels.FindRef(DirtyActor);
		if ( Channel )
			Channel->ActorDirty = true;
	}
	unguardSlow;
}

void ULevel::TickNetServer( FLOAT DeltaSeconds )
{
	guard(ULevel::TickNetServer);

	// Update all clients.
	INT Updated=0;

	// first, set which channels have dirty actors (need replication)
	AActor* Actor = Actors(0);
	if( Actor && Actor->bNetDirty )
	{
		for( INT j=NetDriver->ClientConnections.Num()-1; j>=0; j-- )
			NetDriver->ClientConnections(j)->SetActorDirty(Actor);
		Actor->bNetDirty = 0;
	}
	for( INT i=iFirstNetRelevantActor; i<Actors.Num(); i++ )
	{
		AActor* Actor = Actors(i);
		if( Actor && Actor->bNetDirty )
		{
			if ( Actor->RemoteRole != ROLE_None )
			{
				for( INT j=NetDriver->ClientConnections.Num()-1; j>=0; j-- )
					NetDriver->ClientConnections(j)->SetActorDirty(Actor);
			}
			Actor->bNetDirty = 0;
		}
	}

	clock(GStats.DWORDStats(GEngineStats.STATS_Game_NetTickCycles));
	Updated = ServerTickClients( DeltaSeconds );
	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_NetTickCycles));

	// Log message.
	if( (INT)(TimeSeconds-DeltaSeconds)!=(INT)(TimeSeconds) )
		debugf( NAME_Title, LocalizeProgress(TEXT("RunningNet"),TEXT("Engine")), *GetLevelInfo()->Title, *URL.Map, NetDriver->ClientConnections.Num() );

	// Stats.
	if( Updated )
	{
		for( INT i=0; i<NetDriver->ClientConnections.Num(); i++ )
		{
			UNetConnection* Connection = NetDriver->ClientConnections(i);
			if( Connection->Actor && Connection->State==USOCK_Open )
			{
				if( Connection->UserFlags&1 )
				{
					// Send stats.
					INT NumActors=0;
					for( INT i=0; i<Actors.Num(); i++ )
						NumActors += Actors(i)!=NULL;

					FString Stats = FString::Printf
					(
						TEXT("r=%i cli=%i act=%03.1f (%i) net=%03.1f pv/c=%i rep/c=%i rpc/c=%i"),
						appRound(Engine->GetMaxTickRate()),
						NetDriver->ClientConnections.Num(),
						GSecondsPerCycle*1000*GStats.DWORDStats(GEngineStats.STATS_Game_ActorTickCycles),
						NumActors,
						GSecondsPerCycle*1000*GStats.DWORDStats(GEngineStats.STATS_Game_NetTickCycles),
						GStats.DWORDStats(GEngineStats.STATS_Net_NumPV)   / NetDriver->ClientConnections.Num(),
						GStats.DWORDStats(GEngineStats.STATS_Net_NumReps) / NetDriver->ClientConnections.Num(),
						GStats.DWORDStats(GEngineStats.STATS_Net_NumRPC)  / NetDriver->ClientConnections.Num()
					);
					Connection->Actor->eventClientMessage( *Stats, NAME_None );
				}
				if( Connection->UserFlags&2 )
				{
					FString Stats = FString::Printf
					(
						TEXT("snd=%02.1f recv=%02.1f"),
						GSecondsPerCycle*1000*Connection->Driver->SendCycles,
						GSecondsPerCycle*1000*Connection->Driver->RecvCycles
					);
					Connection->Actor->eventClientMessage( *Stats, NAME_None );
				}
			}
		}
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Demo Recording tick.
-----------------------------------------------------------------------------*/

INT ULevel::TickDemoRecord( FLOAT DeltaSeconds )
{
	guard(ULevel::TickDemoRecord);

	// All replicatable actors are assumed to be relevant for demo recording.
	UNetConnection* Connection = DemoRecDriver->ClientConnections(0);
	for( INT i=iFirstNetRelevantActor; i<Actors.Num(); i++ )
	{
		AActor* Actor = Actors(i);
		UBOOL IsNetClient = (GetLevelInfo()->NetMode == NM_Client);
		if
		(	Actor
		&&	( (IsNetClient && Actor->bTearOff) || Actor->RemoteRole!=ROLE_None || (IsNetClient && Actor->Role!=ROLE_None && Actor->Role != ROLE_Authority))
		&&  (!Actor->bNetTemporary || Connection->SentTemporaries.FindItemIndex(Actor)==INDEX_NONE)
		&&  (Actor->bStatic || !Actor->GetClass()->GetDefaultActor()->bStatic))
		{
			// Create a new channel for this actor.
			UActorChannel* Channel = Connection->ActorChannels.FindRef( Actor );
			if( !Channel && Connection->PackageMap->ObjectToIndex(Actor->GetClass())!=INDEX_NONE )
			{
				// Check we haven't run out of actor channels.
				Channel = (UActorChannel*)Connection->CreateChannel( CHTYPE_Actor, 1 );
				check(Channel);
				Channel->SetChannelActor( Actor );
			}
			if( Channel )
			{
				// Send it out!
				check(!Channel->Closing);
				if( Channel->IsNetReady(0) )
				{
					Actor->bDemoRecording = 1;
					Actor->bNetDirty = 1;
					Actor->bClientDemoRecording = IsNetClient;
					UBOOL TornOff = 0;
					if(IsNetClient)
					{
						if( Actor->bTearOff && Actor->Role==ROLE_Authority && Actor->RemoteRole==ROLE_None )
						{
							TornOff = 1;
							Actor->RemoteRole = ROLE_SimulatedProxy;
						}
						else
						Exchange(Actor->RemoteRole, Actor->Role);
					}
					Channel->ReplicateActor();
					if(IsNetClient)
					{
						if( TornOff )
							Actor->RemoteRole = ROLE_None;
						else
						Exchange(Actor->RemoteRole, Actor->Role);
					}
					Actor->bDemoRecording = 0;
					Actor->bClientDemoRecording = 0;
				}
			}
		}
	}
	return 1;
	unguard;
}
INT ULevel::TickDemoPlayback( FLOAT DeltaSeconds )
{
	guard(ULevel::TickDemoPlayback);
	if
	(	GetLevelInfo()->LevelAction==LEVACT_Connecting 
	&&	DemoRecDriver->ServerConnection->State!=USOCK_Pending )
	{
		GetLevelInfo()->LevelAction = LEVACT_None;
		Engine->SetProgress( TEXT(""), TEXT(""), TEXT(""), 0.0f );
	} 
	if( DemoRecDriver->ServerConnection->State==USOCK_Closed && !DemoRecDriver->Loop )
	{
		// Demo stopped playing
		check(Engine->Client->Viewports.Num());
		Engine->SetClientTravel( Engine->Client->Viewports(0), TEXT("?entry"), 0, TRAVEL_Absolute );
	}
	return 1;
	unguard;
}

/*-----------------------------------------------------------------------------
	Main level timer tick handler.
-----------------------------------------------------------------------------*/
UBOOL ULevel::IsPaused()
{
	guardSlow(ULevel::IsPaused);
	
	return ( GetLevelInfo()->Pauser && (TimeSeconds >= GetLevelInfo()->PauseDelay) );
	unguardSlow;
}
//
// Update the level after a variable amount of time, DeltaSeconds, has passed.
// All child actors are ticked after their owners have been ticked.
//
void ULevel::Tick( ELevelTick TickType, FLOAT DeltaSeconds )
{
	guard(ULevel::Tick);

	FLOAT TotalTick = 0.f;
	clock(TotalTick);

	ALevelInfo* Info = GetLevelInfo();

	FMemMark Mark(GMem);
	FMemMark EngineMark(GEngineMem);
	GInitRunaway();
	InTick=1;

	UBOOL RecordDemoFrame = 0;
	guard(UpdateDemo);
	if( DemoRecDriver && !IsPaused() )
	{
		RecordDemoFrame = DemoRecDriver->UpdateDemoTime( &DeltaSeconds, Info->TimeDilation/1.1f ); // 1.1f == default TimeDilation
		DemoRecDriver->TickDispatch( DeltaSeconds );

		// Fetch demo playback packets from demo file.
		if( DemoRecDriver->ServerConnection )
			TickDemoPlayback( DeltaSeconds );
	}
	unguard;

	// Update the net code and fetch all incoming packets.
	FLOAT PreNetTime = 0.f;
	clock(PreNetTime);
	guard(UpdatePreNet);
	if( NetDriver )
	{
		NetDriver->TickDispatch( DeltaSeconds );
		if( NetDriver->ServerConnection )
			TickNetClient( DeltaSeconds );
	}
	unguard;
	unclock(PreNetTime);
	// Update collision.

	FLOAT HashTickTime = 0.f;
	clock(HashTickTime);
	guard(UpdateCollision);
	if( Hash )
		Hash->Tick();
	unguard;

	// Update time.
	guard(UpdateTime);
	if( !GUseFixedTimeStep ) 
		DeltaSeconds *= Info->TimeDilation;
	// Clamp time between 2000 fps and 2.5 fps.
	//if ( DeltaSeconds > 0.1f )
	//	debugf(TEXT("DeltaSeconds %f (unclamped)"),DeltaSeconds);
	DeltaSeconds = Clamp(DeltaSeconds,0.0005f,0.40f);

	if ( !IsPaused() )
		TimeSeconds += DeltaSeconds;
	Info->TimeSeconds = TimeSeconds;

#if 0
/* GERMAN VERSION */
	AGameInfo *DefaultGameActor = Cast<AGameInfo>((AGameInfo::StaticClass())->GetDefaultActor());
	if ( DefaultGameActor )
		DefaultGameActor->bAlternateMode = true;
	if ( Info->Game )
		Info->Game->bAlternateMode = true;
#endif

	UpdateTime(Info);
	if( Info->bPlayersOnly )
		TickType = LEVELTICK_ViewportsOnly;
	unguard;

	unclock(HashTickTime);
	// If caller wants time update only, or we are paused, skip the rest.
	clock(GStats.DWORDStats(GEngineStats.STATS_Game_ActorTickCycles));
	if
	(	(TickType!=LEVELTICK_TimeOnly)
	&&	!IsPaused()
	&&	(!NetDriver || !NetDriver->ServerConnection || NetDriver->ServerConnection->State==USOCK_Open) )
	{
		// Tick all actors, owners before owned.
		guard(TickAllActors);
		NewlySpawned = NULL;
		INT Updated  = 1;

#if defined(WITH_KARMA) || defined(WITH_PHYSICS)
        KTickLevelKarma(this, DeltaSeconds);
#endif
        PhysicsStateLogTick(this);

		for( INT iActor=iFirstDynamicActor; iActor<Actors.Num(); iActor++ )
		{
			if( Actors( iActor ) && !Actors(iActor)->bDeleteMe )
			{
				Updated += Actors( iActor )->Tick(DeltaSeconds,TickType);
			}
		}


		while( NewlySpawned && Updated )
		{
			FActorLink* Link = NewlySpawned;
			NewlySpawned     = NULL;
			Updated          = 0;
			for( Link; Link; Link=Link->Next )
			{
				if( Link->Actor->bTicked!=(DWORD)Ticked && !Link->Actor->bDeleteMe )
				{
					Updated += Link->Actor->Tick( DeltaSeconds, TickType );
				}
			}
		}
#ifdef WITH_KARMA
		if(KGData)
			KGData->bDoTick = 0;
#elif defined(WITH_PHYSICS)
		if(GJoltData)
			GJoltData->bDoTick = 0;
#endif
		unguard;
	}
	else if( IsPaused() )
	{
		// Absorb input if paused.
		guard(AbsorbedPaused);
		NewlySpawned = NULL;
		INT Updated  = 1;
		for( INT iActor=iFirstDynamicActor; iActor<Actors.Num(); iActor++ )
		{
			APlayerController* PC=Cast<APlayerController>(Actors(iActor));
			if( PC && Cast<UViewport>(PC->Player) )
			{
				PC->Player->ReadInput( DeltaSeconds );
				PC->PlayerInput->eventPlayerInput( DeltaSeconds );
				PC->LastPlayerCalcView = 0.f;
				for( TFieldIterator<UFloatProperty> It(PC->GetClass()); It; ++It )
					if( It->PropertyFlags & CPF_Input )
						*(FLOAT*)((BYTE*)PC + It->Offset) = 0.f;
				PC->bTicked = (DWORD)Ticked;
			}
			else if( Actors(iActor) )
			{
				if ( Actors(iActor)->bAlwaysTick && !Actors(iActor)->bDeleteMe )
					Actors(iActor)->Tick(DeltaSeconds,TickType);
				else
					Actors(iActor)->bTicked = (DWORD)Ticked;
			}
		}
		while( NewlySpawned && Updated )
		{
			FActorLink* Link = NewlySpawned;
			NewlySpawned     = NULL;
			Updated          = 0;
			for( Link; Link; Link=Link->Next )
				if( Link->Actor->bTicked!=(DWORD)Ticked && !Link->Actor->bDeleteMe )
				{
					if ( Link->Actor->bAlwaysTick )
						Updated += Link->Actor->Tick( DeltaSeconds, TickType );
					else
						Link->Actor->bTicked = (DWORD)Ticked;
				}
		}
		unguard;
	}
	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_ActorTickCycles));
/*
	if ( (GStats.DWORDStats(GEngineStats.STATS_Game_ActorTickCycles) * GSecondsPerCycle * 1000.0f >= 30.f)
		|| (GScriptCycles * GSecondsPerCycle * 1000.0f > 10.f) )
 		debugf(TEXT("********** ActorTick Cycles %f at %f pathtime %f script time %f"), 
					GStats.DWORDStats(GEngineStats.STATS_Game_ActorTickCycles) * GSecondsPerCycle * 1000.0f, 
					TimeSeconds,
					GStats.DWORDStats(GEngineStats.STATS_Game_FindPathCycles) * GSecondsPerCycle * 1000.0f,
					GScriptCycles * GSecondsPerCycle * 1000.0f
					);
*/
 	if ( Engine->Client )
	{
		if ( Engine->Client->InteractionMaster )
			Engine->Client->InteractionMaster->MasterProcessTick(DeltaSeconds);
		if ( DemoRecDriver && DemoRecDriver->bNoRender && Engine->Client->Viewports(0) && Engine->Client->Viewports(0)->Canvas )
			Engine->Client->Viewports(0)->Canvas->bRenderLevel = 0;
	}

	// Update net server and flush networking.
	FLOAT ServerTime = 0.f;
	clock(ServerTime);
	guard(UpdateNetServer);
	if( NetDriver )
	{
		if( !NetDriver->ServerConnection )
			TickNetServer( DeltaSeconds );
		NetDriver->TickFlush();
	}
	unguard;
	unclock(ServerTime);
	// Demo Recording.
	guard(UpdatePostDemoRec);
	if( DemoRecDriver )
	{
		if( !DemoRecDriver->ServerConnection && RecordDemoFrame )
			TickDemoRecord( DeltaSeconds );
		DemoRecDriver->TickFlush();
	}
	unguard;

	// Finish up.
	Ticked = !Ticked;
	InTick = 0;
	Mark.Pop();
	EngineMark.Pop();

	clock(GStats.DWORDStats(GEngineStats.STATS_Game_CleanupDestroyedCycles));
#ifndef _XBOX
	UGameEngine* GameEngine = Cast<UGameEngine>(Engine);
	if( GameEngine && ( !GameEngine->GLevel || ( GameEngine->GLevel == GameEngine->GEntry ) ) )
    	CleanupDestroyed( 1 );
	else
    	CleanupDestroyed( 0 );
#endif
	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_CleanupDestroyedCycles));
	unclock(TotalTick);
	if ( false && NetDriver && (TotalTick * GSecondsPerCycle * 1000.f > 100.f) )
		debugf(TEXT("TotalTick %01.4f PreNetTime %01.4f ReceiveCycles %01.4f ActorTick %01.4f ServerTime %01.4f Collision %01.4f Physics %01.4f Clean up destroyed %01.4f VoiceReceivedBunch %01.4f ControlReceivedBunch %01.4f ReceivedBunch %01.4f"),
					TotalTick * GSecondsPerCycle * 1000.f,
					PreNetTime * GSecondsPerCycle * 1000.f,
					GSecondsPerCycle*1000.f*NetDriver->RecvCycles,
					GStats.DWORDStats(GEngineStats.STATS_Game_ActorTickCycles) * GSecondsPerCycle * 1000.f,
					ServerTime * GSecondsPerCycle * 1000.f,
					(GStats.DWORDStats(GEngineStats.STATS_Game_MLCheckCycles) +
					GStats.DWORDStats(GEngineStats.STATS_Game_MPCheckCycles)) * GSecondsPerCycle * 1000.f,
					GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles) * GSecondsPerCycle * 1000.f,
					GStats.DWORDStats(GEngineStats.STATS_Game_CleanupDestroyedCycles) * GSecondsPerCycle * 1000.f,
					GStats.DWORDStats(GEngineStats.STATS_Net_VoiceTime)  * GSecondsPerCycle * 1000.f,
					GStats.DWORDStats(GEngineStats.STATS_Net_ControlTime) * GSecondsPerCycle * 1000.f,
					GStats.DWORDStats(GEngineStats.STATS_Game_UnusedCycles) * GSecondsPerCycle * 1000.f);
	unguardf(( TEXT("(NetMode=%i)"), GetLevelInfo()->NetMode ));
}



/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

