/*=============================================================================
	UnActor.cpp: AActor implementation
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/ 

#include "EnginePrivate.h"
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	AActor object implementations.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(AActor);
IMPLEMENT_CLASS(ALight);
IMPLEMENT_CLASS(AClipMarker);
IMPLEMENT_CLASS(APolyMarker);
IMPLEMENT_CLASS(AWeapon);
IMPLEMENT_CLASS(UWeaponFire); 
IMPLEMENT_CLASS(ANote);
IMPLEMENT_CLASS(AVignette); 

IMPLEMENT_CLASS(AAmbientSound);

void AAmbientSound::PostBeginPlay()
{
	guard(AAmbientSound::PostBeginPlay);

	Super::PostBeginPlay();

	if ( Level->NetMode == NM_DedicatedServer )
	    return;
	
	for( INT i = 0; i < SoundEmitters.Num(); i++ )
	{
	    FSoundEmitter& SoundEmitter = SoundEmitters(i);
	    
	    if( !SoundEmitter.EmitSound )
	        continue;

	    if( SoundEmitter.EmitInterval <= 0.0f )
	        continue;

	    SoundEmitter.EmitTime = SoundEmitter.EmitInterval 
	        - SoundEmitter.EmitVariance + (2.0f * appFrand() * SoundEmitter.EmitVariance );
    }
	unguard;
}

UBOOL AAmbientSound::Tick( FLOAT DeltaTime, enum ELevelTick TickType )
{
	guard(AAmbientSound::Tick);
	
	// SoundEmitters are client-side only.
	if ( Level->NetMode == NM_DedicatedServer )
	{
		if ( bNotOnDedServer && (RemoteRole == ROLE_None) )
		{
				bNoDelete = false;
				GetLevel()->DestroyActor( this );
				return true;
		}
        return( Super::Tick( DeltaTime, TickType ) );
	}

	if ( bHiddenEd )
        return( Super::Tick( DeltaTime, TickType ) );
	
	for( INT i = 0; i < SoundEmitters.Num(); i++ )
	{
	    FSoundEmitter& SoundEmitter = SoundEmitters(i);
	    
	    if( !SoundEmitter.EmitSound )
	        continue;

	    if( SoundEmitter.EmitInterval <= 0.0f )
	        continue;

	    SoundEmitter.EmitTime -= DeltaTime;

	    if( SoundEmitter.EmitTime > 0.0f )
	        continue;

	    SoundEmitter.EmitTime = SoundEmitter.EmitInterval 
	        - SoundEmitter.EmitVariance + (2.0f * appFrand() * SoundEmitter.EmitVariance );

		UClient* Client = GetLevel()->Engine->Client;
		
		if( !Client )
		    continue;

        USound* Sound = SoundEmitter.EmitSound;

        FLOAT Volume = 4.f * SoundVolume * AmbientVolume;
        FLOAT Radius = SoundRadius;
		FLOAT Pitch = Clamp<FLOAT>(SoundPitch/64.f,0.5f,2.0f);
		    
    	INT Id = GetIndex()*16 + SLOT_None*2 + 1;

    	FVector Parameters = FVector(Volume, Radius, Pitch);
		
		if( !GIsEditor )
		{
		    for( INT vp=0; vp<Client->Viewports.Num(); vp++ )
		    {
			    AController* Hearer = Client->Viewports(vp)->Actor;
    			
			    if( Hearer && Hearer->GetLevel()==GetLevel() )
				    Hearer->CheckHearSound(this, Id, Sound, Parameters, Radius, 1);
		    }
	    }
	    else
	    {
		    for( INT vp=0; vp<Client->Viewports.Num(); vp++ )
		    {
			    AController* Hearer = Client->Viewports(vp)->Actor;
    			
	            if(	Hearer->LocalPlayerController()	&& GetLevel()->Engine->Audio )
		            GetLevel()->Engine->Audio->PlaySound( this, Id, Sound, Location, SoundVolume * AmbientVolume, Radius, Pitch, 0, 0.f );
		    }
	    }
	}

    return( Super::Tick( DeltaTime, TickType ) );
	
	unguard;
}

FLOAT AActor::GetAmbientVolume(FLOAT Attenuation)
{
	guardSlow(AActor::GetAmbientVolume);

	if( bFullVolume )
		return SoundVolume / 255.f / 2.f; // additionally divide by 2 as volume is now in range 0..2
	else
		return Attenuation * SoundVolume / 255.f / 2.f; // additionally divide by 2 as volume is now in range 0..2

	unguardSlow;
}

static void PreformatSpeechString(FString& TextOut, const FString& TextIn)
{
	guard(PreformatSpeechString);

	UTextToSpeechAlias* TTSAlias = Cast<UTextToSpeechAlias>( UTextToSpeechAlias::StaticClass()->GetDefaultObject() );

	// First check each word against replacement list.
	TArray<FString> Words;
	TextIn.ParseIntoArray( TEXT(" "), &Words );

	TextOut.Empty();

	for(INT i=0; i<Words.Num(); i++)
	{
		if ( TextOut.Len() ) TextOut += TEXT(" ");
		for(INT j=0; j<TTSAlias->Aliases.Num(); j++)
		{
			INT k;
			FSpeechReplacement& alias = TTSAlias->Aliases(j);
			for ( k = 0; k < alias.MatchWords.Num(); k++ )
				if ( Words(i) == alias.MatchWords(k) )
				{
					Words(i) = alias.ReplaceWord;
					break;
				}

			if ( k < alias.MatchWords.Num() )
				break;
		}

		TextOut += Words(i);
	}

	// Then we remove any undesirable characters. Iterate over each character in string.
	TArray<TCHAR>& OutChars = TextOut.GetCharArray();
	TArray<TCHAR>& RemChars = TTSAlias->RemoveCharacters.GetCharArray();

	INT k=0;
	while( k<TextOut.Len() )
	{
		// If this character is in the RemoveCharacters string, remove it.
		if( RemChars.FindItemIndex( OutChars(k) ) != INDEX_NONE )
			OutChars.Remove(k);
		else
			k++;
	}

	unguard;
}

void AActor::TextToSpeech( const FString& Text, FLOAT Volume )
{
	guard(AActor::TextToSpeech);

	FString StrippedText;
	PreformatSpeechString(StrippedText, Text);

	if( UTexture::__Client && UTexture::__Client->Viewports.Num() && UTexture::__Client->Viewports(0) )
		UTexture::__Client->Viewports(0)->TextToSpeech( StrippedText, Volume );

	unguard;
}

IMPLEMENT_CLASS(UTextToSpeechAlias);
IMPLEMENT_CLASS(ALevelInfo);
IMPLEMENT_CLASS(AGameInfo);
IMPLEMENT_CLASS(ACamera);
IMPLEMENT_CLASS(AZoneInfo);
IMPLEMENT_CLASS(ASkyZoneInfo);
IMPLEMENT_CLASS(UReachSpec);
IMPLEMENT_CLASS(APathNode);
IMPLEMENT_CLASS(ANavigationPoint);
IMPLEMENT_CLASS(ASmallNavigationPoint);
IMPLEMENT_CLASS(AScout);
IMPLEMENT_CLASS(AInterpolationPoint);
IMPLEMENT_CLASS(ADecoration);
IMPLEMENT_CLASS(AProjectile);
IMPLEMENT_CLASS(AWarpZoneInfo);
IMPLEMENT_CLASS(ATeleporter);
IMPLEMENT_CLASS(APlayerStart);
IMPLEMENT_CLASS(AKeypoint);
IMPLEMENT_CLASS(AInventory);
IMPLEMENT_CLASS(AInventorySpot);
IMPLEMENT_CLASS(AAIMarker);
IMPLEMENT_CLASS(ATriggers);
IMPLEMENT_CLASS(ATrigger);
IMPLEMENT_CLASS(AWarpZoneMarker);
IMPLEMENT_CLASS(AHUD);
IMPLEMENT_CLASS(ASavedMove);
IMPLEMENT_CLASS(ALiftCenter);
IMPLEMENT_CLASS(ALiftExit);
IMPLEMENT_CLASS(AInfo);
IMPLEMENT_CLASS(AReplicationInfo);
IMPLEMENT_CLASS(APlayerReplicationInfo);
IMPLEMENT_CLASS(ALinkedReplicationInfo);
IMPLEMENT_CLASS(AInternetInfo);
IMPLEMENT_CLASS(AGameReplicationInfo);
IMPLEMENT_CLASS(ULevelSummary);
IMPLEMENT_CLASS(ULogEntry);
IMPLEMENT_CLASS(AAmmo);
IMPLEMENT_CLASS(APickup);
IMPLEMENT_CLASS(APowerups);
IMPLEMENT_CLASS(AAmmunition);
IMPLEMENT_CLASS(AController);
IMPLEMENT_CLASS(AAIController);
IMPLEMENT_CLASS(APlayerController);
IMPLEMENT_CLASS(AVehicle);
IMPLEMENT_CLASS(AVehiclePart);
IMPLEMENT_CLASS(ASVehicleFactory);
IMPLEMENT_CLASS(ASVehicleTrigger);
IMPLEMENT_CLASS(ALadder);
IMPLEMENT_CLASS(ADoor);
IMPLEMENT_CLASS(ADamageType);
IMPLEMENT_CLASS(ABrush);
IMPLEMENT_CLASS(AAIScript);
IMPLEMENT_CLASS(ALineOfSightTrigger);
IMPLEMENT_CLASS(AVolume);
IMPLEMENT_CLASS(APhysicsVolume);
IMPLEMENT_CLASS(ADefaultPhysicsVolume);
IMPLEMENT_CLASS(ALadderVolume);
IMPLEMENT_CLASS(APotentialClimbWatcher);
IMPLEMENT_CLASS(ABlockingVolume);

IMPLEMENT_CLASS(AAutoLadder);
IMPLEMENT_CLASS(AAutoDoor);
IMPLEMENT_CLASS(ATeamInfo);
IMPLEMENT_CLASS(AInventoryAttachment);
IMPLEMENT_CLASS(AWeaponAttachment);
IMPLEMENT_CLASS(AAntiPortalActor);

IMPLEMENT_CLASS(AJumpPad);
IMPLEMENT_CLASS(AJumpDest);
IMPLEMENT_CLASS(UManifest); // gam
IMPLEMENT_CLASS(AAvoidMarker);
IMPLEMENT_CLASS(AMutator); //amb

IMPLEMENT_CLASS(UGameProfile);
IMPLEMENT_CLASS(AFlyingPathNode);
IMPLEMENT_CLASS(ARoadPathNode);
IMPLEMENT_CLASS(AHoverPathNode);
IMPLEMENT_CLASS(AAnnouncerVoice);

/*-----------------------------------------------------------------------------
	Replication.
-----------------------------------------------------------------------------*/

#if !FORCE_EXTREME_PACKING   // with this #define, BITFIELD is BYTE. --ryan.
static inline UBOOL NEQ(BITFIELD A,BITFIELD B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}
#endif

static inline UBOOL NEQ(BYTE A,BYTE B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}
static inline UBOOL NEQ(INT A,INT B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}
static inline UBOOL NEQ(FLOAT& A,FLOAT& B,UPackageMap* Map,UActorChannel* Channel) {return *(INT*)&A!=*(INT*)&B;}
static inline UBOOL NEQ(FVector& A,FVector& B,UPackageMap* Map,UActorChannel* Channel) {return ((INT*)&A)[0]!=((INT*)&B)[0] || ((INT*)&A)[1]!=((INT*)&B)[1] || ((INT*)&A)[2]!=((INT*)&B)[2];}
static inline UBOOL NEQ(FRotator& A,FRotator& B,UPackageMap* Map,UActorChannel* Channel) {return A.Pitch!=B.Pitch || A.Yaw!=B.Yaw || A.Roll!=B.Roll;}
static inline UBOOL NEQ(UObject* A,UObject* B,UPackageMap* Map,UActorChannel* Channel) {if( Map->CanSerializeObject(A) )return A!=B; Channel->bActorMustStayDirty = true; 
//debugf(TEXT("%s Must stay dirty because of %s"),Channel->Actor->GetName(),A->GetName());
return (B!=NULL);}
static inline UBOOL NEQ(FName& A,FName B,UPackageMap* Map,UActorChannel* Channel) {return *(INT*)&A!=*(INT*)&B;}
static inline UBOOL NEQ(FColor& A,FColor& B,UPackageMap* Map,UActorChannel* Channel) {return *(INT*)&A!=*(INT*)&B;}
static inline UBOOL NEQ(FPlane& A,FPlane& B,UPackageMap* Map,UActorChannel* Channel) {return
((INT*)&A)[0]!=((INT*)&B)[0] || ((INT*)&A)[1]!=((INT*)&B)[1] ||
((INT*)&A)[2]!=((INT*)&B)[2] || ((INT*)&A)[3]!=((INT*)&B)[3];}
static inline UBOOL NEQ(const FString& A,const FString& B,UPackageMap* Map,UActorChannel* Channel) {return A!=B;}

static inline UBOOL NEQ(const FAnimRep& A, const FAnimRep& B,UPackageMap* Map,UActorChannel* Channel)
{
	if ( (A.AnimSequence != B.AnimSequence)
		|| (A.AnimRate != B.AnimRate)
		|| (A.bAnimLoop != B.bAnimLoop) )
	{
		return 1;
	}

	return 0;
}

static inline UBOOL NEQ(const FHitFXData& A, const FHitFXData& B,UPackageMap* Map,UActorChannel* Channel)
{
	if ( A.Bone != B.Bone )
		return 1;

	return 0;
}

static inline UBOOL NEQ(const FCompressedPosition& A, const FCompressedPosition& B,UPackageMap* Map,UActorChannel* Channel)
{
		return 1; // only try to replicate in compressed form if already know location has changed
}


static inline UBOOL NEQ(const FMoverPosition& A, const FMoverPosition& B,UPackageMap* Map,UActorChannel* Channel)
{
		return (A.PositionNumber != B.PositionNumber); 
}

#define DOREP(c,v) \
	if( NEQ(v,((A##c*)Recent)->v,Map,Channel) ) \
	{ \
		static UProperty* sp##v = FindObjectChecked<UProperty>(A##c::StaticClass(),TEXT(#v)); \
		*Ptr++ = sp##v->RepIndex; \
	}

#define DOREPARRAY(c,v) \
	{static UProperty* sp##v = FindObjectChecked<UProperty>(A##c::StaticClass(),TEXT(#v)); \
	for( INT i=0; i<ARRAY_COUNT(v); i++ ) \
		if( NEQ(v[i],((A##c*)Recent)->v[i],Map,Channel) ) \
				*Ptr++ = sp##v->RepIndex+i;}

void AActor::NetDirty(UProperty* property) 
{ 
	guardSlow(AActor::Dirty);
	if ( property && (property->PropertyFlags & CPF_Net) )
	{
		// test and make sure actor not getting dirtied too often!
		bNetDirty = true;
	}
	unguardSlow;
}

INT* AActor::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(AActor::GetOptimizedRepList);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if ( bSkipActorPropertyReplication && !bNetInitial )
			return Ptr;
		if( Role==ROLE_Authority )
		{
			if ( bReplicateMovement )
			{
				UBOOL bKarmaActor = ( !bNetInitial && (Physics == PHYS_Karma) );
				if ( RemoteRole != ROLE_AutonomousProxy )
				{
					if ( !bKarmaActor )
					{
						UBOOL bAlreadyLoc = false;
						// If the actor was based and is no longer, send the location!
						if ( !bCompressedPosition && !Base && ((AActor*)Recent)->Base )
						{
							static UProperty* spLocation = FindObjectChecked<UProperty>(AActor::StaticClass(),TEXT("Location"));
							*Ptr++ = spLocation->RepIndex;
							bAlreadyLoc = true;
						}
	 
						DOREP(Actor,Base);
						DOREP(Actor,bOnlyDrawIfAttached);
						if( Base && !Base->bWorldGeometry )
						{
							DOREP(Actor,RelativeLocation);  
							DOREP(Actor,RelativeRotation);
							DOREP(Actor,AttachmentBone);
							if ( !bCompressedPosition && !((AActor*)Recent)->Base )
								DOREP(Actor,Location);
						}
						else if( !bCompressedPosition && ((RemoteRole < ROLE_SimulatedProxy) || bNetInitial || bUpdateSimulatedPosition) )
						{
							if ( !bNetInitial && !bAlreadyLoc )
								DOREP(Actor,Location);
							if( (DrawType==DT_Mesh || DrawType==DT_StaticMesh) && (!bNetInitial || !bNetInitialRotation) )
								DOREP(Actor,Rotation);
						}
						if( Physics==PHYS_Rotating )
						{
							DOREP(Actor,bFixedRotationDir);
							DOREP(Actor,bRotateToDesired);
							DOREP(Actor,RotationRate);
							DOREP(Actor,DesiredRotation);
						}
					}
					if ( RemoteRole == ROLE_SimulatedProxy )
					{
						if ( !bKarmaActor && !bCompressedPosition && (bNetInitial || bUpdateSimulatedPosition) )
							DOREP(Actor,Velocity);
						if( bNetInitial )
							DOREP(Actor,Physics);
				    }
				    else // dumb proxy
				    {
						DOREP(Actor,Physics);
						if ( Physics == PHYS_Falling )
							DOREP(Actor,Velocity);
					}
				}
				else if ( bNetInitial && !bNetInitialRotation )
				{
					if( DrawType==DT_Mesh || DrawType==DT_StaticMesh )
						DOREP(Actor,Rotation);
				}
			}
			if  ( !bNetOwner || !bClientAnim || bDemoRecording )
			{
				DOREP(Actor,AmbientSound);
				if( AmbientSound )
				{
					DOREP(Actor,SoundRadius);
					DOREP(Actor,SoundVolume);
					DOREP(Actor,SoundPitch);
					DOREP(Actor,SoundOcclusion);
				}
			}
			if ( bNetDirty ) 
			{
				DOREP(Actor,DrawScale);
				//DOREP(Actor,DrawScale3D); // Doesn't work in networking, because of vector rounding
				DOREP(Actor,DrawType);
				DOREP(Actor,bCollideActors);
				DOREP(Actor,bCollideWorld);
				DOREP(Actor,bOnlyOwnerSee);
				DOREP(Actor,Texture);
				DOREP(Actor,RepSkin);
				DOREP(Actor,bClientTrigger);
				DOREP(Actor,Style);
				if ( (DrawType == DT_Mesh) || (DrawType == DT_StaticMesh) )
					DOREP(Actor,PrePivot);
				if( bCollideActors || bCollideWorld )
				{
					DOREP(Actor,bProjTarget);
					DOREP(Actor,bBlockActors);
					DOREP(Actor,CollisionRadius);
					DOREP(Actor,CollisionHeight);
					DOREP(Actor,bIgnoreEncroachers);
				}
				if ( !bSkipActorPropertyReplication )
				{
					// skip these if bSkipActorPropertyReplication, because if they aren't relevant to the client, bNetInitial never gets cleared
					// which obviates bSkipActorPropertyReplication
 					if( bNetOwner )
					{
						DOREP(Actor,Owner);
						DOREP(Actor,Inventory);
					}
                    else if ( Physics == PHYS_Trailer )
                    {
                        DOREP(Actor,Owner);
                    }
					if( bReplicateInstigator && (!bNetTemporary || (Instigator && Map->CanSerializeObject(Instigator))) )
					{
						DOREP(Actor,Instigator);
					}
				}
				if( DrawType==DT_Mesh )
				{
					DOREP(Actor,AmbientGlow);
                    DOREP(Actor,ScaleGlow);
					DOREP(Actor,bUnlit);
					if ( !bNoRepMesh )
						DOREP(Actor,Mesh);
					if( bReplicateAnimations )
						DOREP(Actor,SimAnim);
				}
				else if( DrawType==DT_StaticMesh )
					DOREP(Actor,StaticMesh);
				DOREP(Actor,LightType);
				if( LightType!=LT_None )
				{
					DOREP(Actor,LightEffect);
					DOREP(Actor,LightBrightness);
					DOREP(Actor,LightHue);
					DOREP(Actor,LightSaturation);
					DOREP(Actor,LightRadius);
					DOREP(Actor,LightPeriod);
					DOREP(Actor,LightPhase);
					DOREP(Actor,bSpecialLit);
				}
                // sjs ---
                DOREP(Actor,OverlayMaterial);
                DOREP(Actor,OverlayTimer);
                // --- sjs
			}
			DOREP(Actor,bHidden);
			DOREP(Actor,Role);
			if( !(bClientDemoRecording && bTearOff) )
				DOREP(Actor,RemoteRole);
			DOREP(Actor,bNetOwner);
			DOREP(Actor,bTearOff);
 		}
	}
	return Ptr;
	unguard;
}

INT* APawn::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(APawn::GetOptimizedRepList);

	bCompressedPosition = false;
	if ( !bNetOwner || bDemoRecording )
	{
		if ( bUseCompressedPosition && (Base ? Base->bWorldGeometry : !((AActor*)Recent)->Base) )
		{
			if ( Velocity != ((AActor*)Recent)->Velocity )
			{
				LastLocTime = Level->TimeSeconds;
				bCompressedPosition = true;
				PawnPosition.Location = Location;
				PawnPosition.Rotation = Rotation;
				PawnPosition.Velocity = Velocity;
				DOREP(Pawn,PawnPosition);
				((APawn*)Recent)->Location = Location;
				((APawn*)Recent)->Rotation = Rotation;
				((APawn*)Recent)->Velocity = Velocity;
			}
			else if ( (Level->TimeSeconds - LastLocTime > 1.f) && (Physics == PHYS_Walking) && (((APawn*)Recent)->Location == Location) )
			{
				// force location update every half second
				LastLocTime = Level->TimeSeconds;
				bCompressedPosition = true;
				PawnPosition.Location = Location;
				PawnPosition.Rotation = Rotation;
				if ( PawnPosition.Velocity == FVector(0.f,0.f,-1.f) )
					PawnPosition.Velocity = FVector(0.f,0.f,-2.f);
				else
					PawnPosition.Velocity = FVector(0.f,0.f,-1.f);
				DOREP(Pawn,PawnPosition);
				((APawn*)Recent)->Location = Location;
				((APawn*)Recent)->Rotation = Rotation;
				((APawn*)Recent)->Velocity = PawnPosition.Velocity;
			}
		}
		if ( !bTearOff && !bNetOwner && (Level->TimeSeconds - Channel->LastFullUpdateTime < 0.09f) )
		{
 			DOREP(Pawn,PlayerReplicationInfo); 
			return Ptr;
		}
	}
	Channel->LastFullUpdateTime = Level->TimeSeconds;

	if ( bTearOff
		&& (StaticClass()->ClassFlags & CLASS_NativeReplication)
		&& (Role==ROLE_Authority) && bNetDirty )
	{
		DOREP(Pawn,TearOffMomentum);
		DOREPARRAY(Pawn,HitFx);
		DOREP(Pawn,HitFxTicker);
	}

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( Role==ROLE_Authority )
		{
			if ( !bNetOwner )
				DOREP(Pawn,ViewPitch); 
			if( bNetDirty )
			{
				DOREP(Pawn,PlayerReplicationInfo); 
				DOREP(Pawn,Health);
				if ( bNetInitial)
					DOREP(Pawn,HealthMax);
				if ( (bNetOwner || ((APawn*)Recent)->Controller) && (!Controller || Map->CanSerializeObject(Controller)) )
				{
					DOREP(Pawn,Controller);
				}
				if( bNetOwner )
				{
 					DOREP(Pawn,SelectedItem);
					DOREP(Pawn,GroundSpeed);
					DOREP(Pawn,WaterSpeed);
					DOREP(Pawn,AirSpeed);
					DOREP(Pawn,AccelRate);
					DOREP(Pawn,JumpZ);
					DOREP(Pawn,AirControl);
					DOREP(Pawn,bCanWallDodge);
					DOREP(Pawn,PitchUpLimit);
					DOREP(Pawn,PitchDownLimit);
				}
				else
				{
					DOREP(Pawn,bSteadyFiring);
 				}
				DOREP(Pawn,HitDamageType);
				DOREP(Pawn,TakeHitLocation);
				DOREP(Pawn,AnimAction);
				DOREP(Pawn,bSimulateGravity);
				DOREP(Pawn,bIsCrouched);
				DOREP(Pawn,bIsWalking);
				DOREP(Pawn,bIsTyping);
				DOREP(Pawn,DrivenVehicle);
				DOREP(Pawn,bSpecialHUD);
				DOREP(Pawn,HeadScale);

				// xPawn replicated properties - moved here to take advantage of native replication
				DOREP(Pawn,ShieldStrength);
				if ( !bTearOff )
				{
					DOREP(Pawn,TearOffMomentum);
					DOREPARRAY(Pawn,HitFx);
					DOREP(Pawn,HitFxTicker);
				}
			}
		}
    }
	return Ptr;
	unguard;
}

INT* AVehicle::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(AVehicle::GetOptimizedRepList);
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if ( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if ( Role == ROLE_Authority )
		{
			if ( bNetDirty )
			{
				DOREP(Vehicle,bDriving);
				DOREP(Vehicle,bTeamLocked);
				if ( bNetOwner || !Driver || !Driver->bHidden )
				{
					DOREP(Vehicle,Driver);
				}

				DOREP(Vehicle,Team);
				DOREP(Vehicle,bVehicleDestroyed);
				DOREP(Vehicle,WheelsScale);
			}

			if ( bNetInitial )
			{
				DOREP(Vehicle,bHUDTrackVehicle);
			}
		}
	}
	return Ptr;
	unguard;
}

INT* AController::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(AController::GetOptimizedRepList);
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( bNetDirty && (Role==ROLE_Authority) )
		{
			DOREP(Controller,PlayerReplicationInfo);
			DOREP(Controller,Pawn); 
			if ( bNetOwner )
			{
				DOREP(Controller,Adrenaline);
				DOREP(Controller,PawnClass);
				DOREP(Controller,bAdrenalineEnabled);
 			}
		}
		if( bDemoRecording )
		{
			DOREP(Actor,Rotation);
			DOREP(Controller,Pawn); 
			if( RemoteRole==ROLE_AutonomousProxy )
				DOREP(Actor,Location);
		}
	}
	return Ptr;
	unguard;
}

INT* APlayerController::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(APlayerController::GetOptimizedRepList);
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( bNetOwner && (Role==ROLE_Authority) )
		{
			if( bNetDirty )
			{
				DOREP(PlayerController,GameReplicationInfo);
				DOREP(PlayerController,VoiceReplicationInfo);
				DOREP(PlayerController,ChatManager);
			}
			if ( (GetViewTarget() != Pawn) && ViewTarget->GetAPawn() )
			{
				DOREP(PlayerController,TargetViewRotation);
				DOREP(PlayerController,TargetEyeHeight);
			}
		}
	}
	return Ptr;
	unguard;
}

INT* AMover::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(AMover::GetOptimizedRepList);
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( Role==ROLE_Authority )
		{
			DOREP(Mover,SimOldPos);
			DOREP(Mover,SimOldRotPitch);
			DOREP(Mover,SimOldRotYaw);
			DOREP(Mover,SimOldRotRoll);
			DOREP(Mover,SimInterpolate);
			DOREP(Mover,RealPosition);
			DOREP(Mover,RealRotation);
			DOREP(Mover,StoppedPosition);
			DOREP(Actor,Velocity);
		}
	}
	return Ptr;
	unguard;
}

INT* APhysicsVolume::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(APhysicsVolume::GetOptimizedRepList);

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( (Role==ROLE_Authority) && bSkipActorPropertyReplication && !bNetInitial )
		{
			DOREP(Actor,Location);
			DOREP(Actor,Rotation);
			DOREP(Actor,Base);
			if( Base && !Base->bWorldGeometry )
			{
				DOREP(Actor,RelativeLocation);  
				DOREP(Actor,RelativeRotation);
				DOREP(Actor,AttachmentBone);
			}
		}
		if ( (Role==ROLE_Authority) && bNetDirty )
		{
			DOREP(PhysicsVolume,Gravity);
		}
	}
	return Ptr;
	unguard;
}

INT* ALevelInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(ALevelInfo::GetOptimizedRepList);
	// only replicate needed actor properties
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( (Role==ROLE_Authority) && bNetDirty )
		{
			DOREP(LevelInfo,Pauser);
			DOREP(LevelInfo,TimeDilation);
			DOREP(LevelInfo,DefaultGravity);
		}

		if( (Role==ROLE_Authority) && bNetInitial )
		{
			DOREP(LevelInfo,KarmaTimeScale);
			DOREP(LevelInfo,RagdollTimeScale);
			DOREP(LevelInfo,KarmaGravScale);
		}
	}
	return Ptr;
	unguard;
}

INT* AReplicationInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(AReplicationInfo::GetOptimizedRepList);
	if ( !bSkipActorPropertyReplication )
		Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	return Ptr;
	unguard;
}

INT* APlayerReplicationInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(APlayerReplicationInfo::GetOptimizedRepList);
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( (Role==ROLE_Authority) && bNetDirty )
		{
			DOREP(PlayerReplicationInfo,Score);
			DOREP(PlayerReplicationInfo,Deaths);

			if ( bNetOwner || !HasFlag || Map->CanSerializeObject(HasFlag) )
			{
				DOREP(PlayerReplicationInfo,HasFlag);
			}

			if ( !bNetOwner || bDemoRecording )
			{
				DOREP(PlayerReplicationInfo,Ping);
				DOREP(PlayerReplicationInfo,PacketLoss);
			}
 			DOREP(PlayerReplicationInfo,PlayerVolume);
			DOREP(PlayerReplicationInfo,PlayerZone);
			DOREP(PlayerReplicationInfo,PlayerName);
			DOREP(PlayerReplicationInfo,Team);
			DOREP(PlayerReplicationInfo,TeamID);
			DOREP(PlayerReplicationInfo,bAdmin);
			DOREP(PlayerReplicationInfo,bIsFemale);
			DOREP(PlayerReplicationInfo,bIsSpectator);
			DOREP(PlayerReplicationInfo,bOnlySpectator);
			DOREP(PlayerReplicationInfo,bWaitingPlayer);
			DOREP(PlayerReplicationInfo,bReadyToPlay);
			DOREP(PlayerReplicationInfo,bOutOfLives);

			// FIX for the CharacterName exploit

			if (Role == ROLE_Authority)
			{
				if (CharacterName != OldCharacterName)
				{
					if (CharacterName.Len() > 100 || CharacterName.Left(1) == TEXT(".") || CharacterName.Right(1) == TEXT(".") )
					{
						CharacterName = TEXT("");
					}
				}
				OldCharacterName = CharacterName;
			}

			DOREP(PlayerReplicationInfo,CharacterName);
			DOREP(PlayerReplicationInfo,ActiveChannel);
			DOREP(PlayerReplicationInfo,VoiceID);
			DOREP(PlayerReplicationInfo,VoiceMemberMask);
			DOREP(PlayerReplicationInfo,StartTime);
			if ( bNetInitial )
			{
				DOREP(PlayerReplicationInfo,PlayerID);

				if (Role == ROLE_Authority)
				{
					if (VoiceTypeName.Len() > 100 || VoiceTypeName.Left(1) == TEXT(".") || VoiceTypeName.Right(1) == TEXT(".") )
					{
						GWarn->Logf(TEXT("=============================================="));
						GWarn->Logf(TEXT("         Crash Attempt Avoided 2 "));
						GWarn->Logf(TEXT("=============================================="));
						VoiceTypeName = TEXT("");
					}
				}

				DOREP(PlayerReplicationInfo,VoiceTypeName);
				DOREP(PlayerReplicationInfo,bBot);
				DOREP(PlayerReplicationInfo,bNoTeam);
				DOREP(PlayerReplicationInfo,CustomReplicationInfo);
			}
		}
	}
	return Ptr;
	unguard;
}
INT* AGameReplicationInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(AGameReplicationInfo::GetOptimizedRepList);
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( (Role==ROLE_Authority) && bNetDirty )
		{
			DOREP(GameReplicationInfo,bStopCountDown);
			DOREP(GameReplicationInfo,bMatchHasBegun);
			DOREP(GameReplicationInfo,Winner);
			DOREP(GameReplicationInfo,FlagPos);
			DOREP(GameReplicationInfo,MatchID);
			DOREP(GameReplicationInfo,FlagTarget);
			DOREPARRAY(GameReplicationInfo,FlagState);
			DOREPARRAY(GameReplicationInfo,Teams);
			if ( bNetInitial )
			{
				DOREP(GameReplicationInfo,GameName);
				DOREP(GameReplicationInfo,GameClass);
				DOREP(GameReplicationInfo,bTeamGame);
				DOREP(GameReplicationInfo,bNoTeamSkins);
				DOREP(GameReplicationInfo,bForceTeamSkins);
				DOREP(GameReplicationInfo,bForceNoPlayerLights);
				DOREP(GameReplicationInfo,bAllowPlayerLights);
				DOREP(GameReplicationInfo,bFastWeaponSwitching);
				DOREP(GameReplicationInfo,bNoTeamChanges);
				DOREP(GameReplicationInfo,WeaponBerserk);
				DOREP(GameReplicationInfo,RemainingTime);
				DOREP(GameReplicationInfo,ElapsedTime);
				DOREP(GameReplicationInfo,ServerName);
				DOREP(GameReplicationInfo,ShortName);
				DOREP(GameReplicationInfo,AdminName);
				DOREP(GameReplicationInfo,AdminEmail);
				DOREP(GameReplicationInfo,ServerRegion);
				DOREP(GameReplicationInfo,MessageOfTheDay);
				DOREP(GameReplicationInfo,GoalScore);
				DOREP(GameReplicationInfo,TimeLimit);
				DOREP(GameReplicationInfo,MaxLives);
				DOREP(GameReplicationInfo,VoiceReplicationInfo);
				DOREP(GameReplicationInfo,MinNetPlayers);
                DOREPARRAY(GameReplicationInfo,TeamSymbols); // sjs
				DOREP(GameReplicationInfo,BotDifficulty);
			}
			else
				DOREP(GameReplicationInfo,RemainingMinute);
		}
	}
	return Ptr;
	unguard;
}
INT* ATeamInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(ATeamInfo::GetOptimizedRepList);
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( Role==ROLE_Authority )
		{
			if ( bNetDirty )
			{
				DOREP(TeamInfo,Score);
				DOREP(TeamInfo,HomeBase);
			}
			if ( bNetInitial )
			{
				DOREP(TeamInfo,TeamName);
				DOREP(TeamInfo,TeamIndex);
				DOREP(TeamInfo,TeamColor);
				DOREP(TeamInfo,AltTeamColor);
			}
		}
	}
	return Ptr;
	unguard;
}

INT* APickup::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(APickup::GetOptimizedRepList);

	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if ( bOnlyReplicateHidden )
		{
			DOREP(Actor,bHidden);
			if ( bNetInitial )
				DOREP(Actor,Rotation);
		}
		else
			Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	}
	return Ptr;

	unguard;
}

INT* AInventory::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(AInventory::GetOptimizedRepList);

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( bNetOwner && (Role==ROLE_Authority) && bNetDirty )
		{
			DOREP(Inventory,ThirdPersonActor);
			DOREP(Inventory,Charge);
		}
	}
	return Ptr;
	unguard;
}

INT* AWeapon::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(AWeapon::GetOptimizedRepList);

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
        if( bNetOwner && (Role==ROLE_Authority) )
		{
            DOREPARRAY(Weapon,Ammo);
            DOREPARRAY(Weapon,AmmoCharge);
		}
	}
	return Ptr;
	unguard;
}
INT* AInventoryAttachment::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(AInventoryAttachment::GetOptimizedRepList);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		return Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	}
	return Ptr;
	unguard;
}

INT* AWeaponAttachment::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(AWeaponAttachment::GetOptimizedRepList);

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if ( (Role == ROLE_Authority) && bNetDirty )
		{
			DOREP(WeaponAttachment,mHitLocation);
			DOREP(WeaponAttachment,SpawnHitCount);
			if ( !bNetOwner || bDemoRecording || bRepClientDemo )
			{
				DOREP(WeaponAttachment,FlashCount);
				DOREP(WeaponAttachment,FiringMode);  
				DOREP(WeaponAttachment,bAutoFire);
			}
		}
	}
	return Ptr;
	unguard;
}

INT* AAmmunition::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(AAmmunition::GetOptimizedRepList);
		
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( bNetOwner && (Role==ROLE_Authority) && bNetDirty )
			DOREP(Ammunition,AmmoAmount);
	}
	return Ptr;
	unguard;
}

INT* APowerups::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(APowerup::GetOptimizedRepList);

	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if( bNetOwner && (Role==ROLE_Authority) && bNetDirty )
		{
			DOREP(Powerups,bActivatable);
			DOREP(Powerups,bActive);
			DOREP(Powerups,NumCopies);
		}
	}
	return Ptr;
	unguard;
}

INT* AVoiceChatReplicationInfo::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	guard(AVoiceChatReplicationInfo::GetOptimizedRepList);
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);
	if( StaticClass()->ClassFlags & CLASS_NativeReplication )
	{
		if ( Role == ROLE_Authority )
		{
			if ( bNetInitial )
				DOREP(VoiceChatReplicationInfo,bEnableVoiceChat);

			if ( bEnableVoiceChat )
			{
				if ( bNetInitial )
				{
					DOREP(VoiceChatReplicationInfo,bPrivateChat);
					if ( bAllowLocalBroadcast )
						DOREP(VoiceChatReplicationInfo,BroadcastRadius);
				}

				if ( bNetDirty )
				{
					DOREP(VoiceChatReplicationInfo,PublicMask);
					DOREP(VoiceChatReplicationInfo,LocalMask);
				}

				if ( bNetDirty || bNetInitial )
					DOREP(VoiceChatReplicationInfo,DefaultChannel);
			}
		}
	}

	return Ptr;
	unguard;
}

/* ReplicateAnim()
store anim properties in replicated variable SimAnim
*/
void AActor::ReplicateAnim(INT channel, FName SequenceName, FLOAT Rate, FLOAT Frame, FLOAT TweenR, FLOAT Last, UBOOL bLoop)
{
	guard(AActor::ReplicateAnim);

	// only replicate animations on channel 0 
	if ( !bReplicateAnimations || (channel != 0) || (Level->NetMode == NM_Standalone) || (Level->NetMode == NM_Client) )
		return;

	FLOAT NewRate = (Last == 0.f) ? 124.f : 31 * (4.f + Clamp(Rate,-4.f,4.f));

	if ( (SimAnim.AnimSequence != SequenceName)
		|| (SimAnim.AnimRate != NewRate)
		|| (SimAnim.bAnimLoop != bLoop) )
		bNetDirty = true;

	SimAnim.AnimSequence = SequenceName;
	SimAnim.bAnimLoop = bLoop;
	SimAnim.AnimRate = NewRate;
	SimAnim.AnimFrame = 127 * (1.f + Clamp(Frame,-1.f,1.f));
	SimAnim.TweenRate = 4 * Clamp(TweenR,0.f,63.f);
	unguard;
}

/* PlayReplicatedAnim()
Play the animation represented by the current value of SimAnim
*/
void AActor::PlayReplicatedAnim()
{
	guard(AActor::PlayReplicatedAnim);

	if (SimAnim.AnimSequence != NAME_None)
	{
		Mesh->MeshGetInstance(this);
		MeshInstance->PlayAnim(	0,
								SimAnim.AnimSequence,
								(FLOAT(SimAnim.AnimRate) - 124.f)/31.f,
								4.f/SimAnim.TweenRate,
								SimAnim.bAnimLoop);
		MeshInstance->SetAnimFrame( 0, (FLOAT(SimAnim.AnimFrame) - 127.f)/127.f );
	}
	unguard;
}

UBOOL AActor::IsAnimating(int Channel) const
{
	if( DrawType == DT_Mesh && Mesh )
	{
		Mesh->MeshGetInstance(this);
		return MeshInstance->IsAnimating( Channel );
	}
	return false;
}

void AActor::UpdateAnimation(FLOAT DeltaSeconds)
{
	guardSlow(AActor::UpdateAnimation);

	if( Mesh )	
		Mesh->MeshGetInstance(this)->UpdateAnimation(DeltaSeconds);

	if ( bForceSkelUpdate )
	{
		USkeletalMesh *SkelMesh = Cast<USkeletalMesh>(Mesh);
		if ( SkelMesh && Attached.Num() )
		{
			UBOOL bEvaluateSkel = false;
			for ( INT i=0; i<Attached.Num(); i++ )
			{
				AActor *AttachedActor = Attached(i);
				if ( AttachedActor && !AttachedActor->bDeleteMe && (AttachedActor->AttachmentBone != NAME_None) )
				{
					bEvaluateSkel = true;
					break;
				}
			}
			if ( bEvaluateSkel )
			{
				USkeletalMeshInstance *SkelMeshInst = Cast<USkeletalMeshInstance>(SkelMesh->MeshGetInstance(this));
				if ( SkelMeshInst )
				{
					SkelMeshInst->ForceGetFrame();
					for ( INT i=0; i<Attached.Num(); i++ )
					{
						AActor *AttachedActor = Attached(i);
						if ( AttachedActor && !AttachedActor->bDeleteMe && (AttachedActor->AttachmentBone != NAME_None) )
							SkelMesh->SetAttachmentLocation(this,AttachedActor);
					}
				}
			}
		}
	}
	unguardSlow;
}

void AActor::NotifyAnimEnd( int Channel )
{
	eventAnimEnd(Channel);
}

#ifdef WITH_LIPSINC
void AActor::NotifyLIPSincAnimEnd()
{
	eventLIPSincAnimEnd();
}
#endif

FVector AActor::GetRootLocation()
{
	guardSlow(AActor::GetRootLocation);
	if( DrawType==DT_Mesh && Mesh && Mesh->IsA(USkeletalMesh::StaticClass()))
	{
		USkeletalMeshInstance*	MeshInstance = Cast<USkeletalMeshInstance>(Mesh->MeshGetInstance(this));
		return MeshInstance->GetBoneCoords(0).Origin;
	}
	return Location;
	unguardSlow;
}


/*-----------------------------------------------------------------------------
	AActor networking implementation.
-----------------------------------------------------------------------------*/

//
// Static variables for networking.
//
static FVector   SavedLocation;
static FVector   SavedRelativeLocation;
static FRotator  SavedRotation;
static FRotator  SavedRelativeRotation;
static AActor*   SavedBase;
static DWORD     SavedCollision;
static FLOAT	 SavedRadius;
static FLOAT     SavedHeight;
static FAnimRep  SavedSimAnim;
static FVector	 SavedSimInterpolate;
static FLOAT	 SavedDrawScale;
static FVector	 SavedDrawScale3D;
static BYTE		 SavedDrawType;
static FName	 SavedAnimAction;
static FLOAT	 SavedHeadScale;
static FCompressedPosition SavedPawnPosition;
static UMaterial*  SavedRepSkin;
static FLOAT	SavedGravity;
static AVehicle*		SavedDrivenVehicle;
static BYTE		SavedPhysics;
static BYTE		SavedClientTrigger;

//
// Skins.
//
UMaterial* AActor::GetSkin( INT Index )
{
	if( Index < Skins.Num() )
		return Skins(Index);
	return NULL;
}

//
// Net priority.
//
FLOAT AActor::GetNetPriority( FVector& ViewPos, FVector& ViewDir, AActor* Sent, FLOAT Time, FLOAT Lag )
{
	guardSlow(AActor::GetNetPriority);
	if ( bAlwaysRelevant )
		return NetPriority * Time * ::Min(NetUpdateFrequency * 0.1f, 1.f);
	else
		return NetPriority * Time;
	unguardSlow;
}

//
// Always called immediately before properties are received from the remote.
//
void AActor::PreNetReceive()
{
	guard(AActor::PreNetReceive);
	SavedLocation   = Location;
	SavedRotation   = Rotation;
	SavedRelativeLocation = RelativeLocation;
	SavedRelativeRotation = RelativeRotation;
	SavedBase       = Base;
	SavedCollision  = bCollideActors;
	SavedRadius		= CollisionRadius;
	SavedHeight     = CollisionHeight;
	SavedSimAnim    = SimAnim;
	SavedDrawScale	= DrawScale;
	SavedDrawScale3D= DrawScale3D;
	SavedDrawType = DrawType;
	SavedRepSkin = RepSkin;
	SavedPhysics = Physics;
	SavedClientTrigger = bClientTrigger;
	if( bCollideActors )
		GetLevel()->Hash->RemoveActor( this );
	unguard;
}

void APawn::PreNetReceive()
{
	guard(AActor::PreNetReceive);

	SavedPawnPosition = PawnPosition;
	SavedAnimAction = AnimAction;
	SavedHeadScale = HeadScale;
	SavedDrivenVehicle = DrivenVehicle;
	AActor::PreNetReceive();
	unguard;
}

void AMover::PreNetReceive()
{
	guard(AMover::PreNetReceive);

	SavedSimInterpolate = SimInterpolate;
	AActor::PreNetReceive();
	unguard;
}

void ALevelInfo::PreNetReceive()
{
	guard(ALevelInfo::PreNetReceive);

	SavedGravity = DefaultGravity;
	AActor::PreNetReceive();
	unguard;
}

void ALevelInfo::PostNetReceive()
{
	guard(ALevelInfo::PostNetReceive);

	if ( DefaultGravity != SavedGravity )
		GetDefaultPhysicsVolume()->Gravity.Z = DefaultGravity;

	AActor::PostNetReceive();
	unguard;
}
//
// Always called immediately after properties are received from the remote.
//
void AActor::PostNetReceive()
{
	guard(AActor::PostNetReceive);

	Exchange ( Location,        SavedLocation  );
	Exchange ( Rotation,        SavedRotation  );
	Exchange ( RelativeLocation,        SavedRelativeLocation  );
	Exchange ( RelativeRotation,        SavedRelativeRotation  );
	Exchange ( Base,            SavedBase      );
	ExchangeB( bCollideActors,  SavedCollision );
	Exchange ( CollisionRadius, SavedRadius    );
	Exchange ( CollisionHeight, SavedHeight    );
	Exchange ( DrawScale, SavedDrawScale       );
	Exchange ( DrawScale3D, SavedDrawScale3D   );
	Exchange ( DrawType, SavedDrawType       );
	Exchange ( RepSkin, SavedRepSkin	       );

	if( bCollideActors )
		GetLevel()->Hash->AddActor( this );
	if( Mesh && NEQ(SimAnim,SavedSimAnim, NULL, NULL) )
		PlayReplicatedAnim();
	if( CollisionRadius!=SavedRadius || CollisionHeight!=SavedHeight )
		SetCollisionSize( SavedRadius, SavedHeight );
	if( bCollideActors!=SavedCollision )
		SetCollision( SavedCollision, bBlockActors );
	if( Location!=SavedLocation )
		PostNetReceiveLocation();
	if( Rotation!=SavedRotation )
	{
		FCheckResult Hit;
		GetLevel()->MoveActor( this, FVector(0,0,0), SavedRotation, Hit, 0, 0, 0, 1 );
	}
	if ( DrawScale!=SavedDrawScale )
		SetDrawScale(SavedDrawScale);
	if ( DrawScale3D!=SavedDrawScale3D )
		SetDrawScale3D(SavedDrawScale3D);
	if ( DrawType != SavedDrawType )
		SetDrawType(EDrawType(SavedDrawType));

	if ( (Physics != SavedPhysics) 
		&& ((Physics == PHYS_Karma) || (SavedPhysics == PHYS_Karma) || (Physics == PHYS_KarmaRagDoll) || (SavedPhysics == PHYS_KarmaRagDoll)) ) 
	{
		Exchange ( Physics, SavedPhysics );
		setPhysics(SavedPhysics);
	}
	UBOOL bBaseChanged = ( Base!=SavedBase );
	if( bBaseChanged )
	{
  		// Base changed.
		if( SavedBase )
		{
			eventBump( SavedBase );
			SavedBase->eventBump( this );
		}
		UBOOL bRelativeLocationChanged = (SavedRelativeLocation != RelativeLocation);
		UBOOL bRelativeRotationChanged = (SavedRelativeRotation != RelativeRotation);
		SetBase( SavedBase );
		if ( !bRelativeLocationChanged )
			SavedRelativeLocation = RelativeLocation;
		if ( !bRelativeRotationChanged )
			SavedRelativeRotation = RelativeRotation;
	}

	if ( Base && !Base->bWorldGeometry )
	{
		if ( bAlwaysZeroBoneOffset && Base->Mesh && Base->Mesh->IsA(USkeletalMesh::StaticClass()) )
			SavedRelativeLocation = FVector(0.f,0.f,0.f);

		if ( AttachmentBone != NAME_None )
		{
			RelativeLocation = SavedRelativeLocation;
			RelativeRotation = SavedRelativeRotation;
		}
		else if ( !bHardAttach )
		{
			if ( bBaseChanged || (RelativeLocation != SavedRelativeLocation) )
			{
				GetLevel()->FarMoveActor( this, Base->Location + SavedRelativeLocation, 0, 1, 1 );
				RelativeLocation = SavedRelativeLocation;
			}
			if ( bBaseChanged || (RelativeRotation != SavedRelativeRotation) )
			{
				FCheckResult Hit;
				FCoords Coords = GMath.UnitCoords / Base->Rotation;
				FCoords PartCoords = GMath.UnitCoords / SavedRelativeRotation;
				PartCoords = PartCoords * Coords.Transpose();
				FRotator NewRotation = PartCoords.OrthoRotation();
				GetLevel()->MoveActor( this, FVector(0,0,0), NewRotation, Hit, 0, 0, 0, 1 );
			}
		}
	}
	bJustTeleported = 0;

	if ( SavedRepSkin != RepSkin )
	{
		if ( Skins.Num() == 0 )
		{
			if ( SavedRepSkin )
				Skins.AddItem(SavedRepSkin);
		}
		else
			Skins(0) = SavedRepSkin;
		RepSkin = SavedRepSkin;
	}

    if ( bNetNotify )
        eventPostNetReceive();

	if ( bClientTrigger != SavedClientTrigger )
		eventClientTrigger();

	unguard;
}

void AActor::PostNetReceiveLocation()
{
	guardSlow(AActor::PostNetReceiveLocation);

	GetLevel()->FarMoveActor( this, SavedLocation, 0, 1, 1 );
	unguardSlow;
}

void APawn::PostNetReceive()
{
	guardSlow(APawn::PostNetReceive);

	if ( (SavedPawnPosition.Location != PawnPosition.Location)
		|| (SavedPawnPosition.Rotation != PawnPosition.Rotation)
		|| (SavedPawnPosition.Velocity != PawnPosition.Velocity) )
	{
		Location = PawnPosition.Location;
		Rotation = PawnPosition.Rotation;
		Velocity = PawnPosition.Velocity;
	}
	if ( DrivenVehicle != SavedDrivenVehicle )
	{
		if ( DrivenVehicle )
		{
			Exchange ( Location,        SavedLocation  );
			GetLevel()->FarMoveActor( this, SavedLocation, 0, 1, 1 );
			Exchange ( Base, SavedBase );
			eventStartDriving(DrivenVehicle);
			SavedBase = Base;
			SavedLocation = Location;
			SavedRotation = Rotation;
			SavedRelativeLocation = RelativeLocation;
			SavedRelativeRotation = RelativeRotation;
		}
		else
		{
			FVector NewLoc = Location;
			Location = ColLocation;
			SavedLocation = ColLocation;
			eventStopDriving(SavedDrivenVehicle);
			if ( Location == ColLocation )
				Location = NewLoc;
		}
	}
	AActor::PostNetReceive();
		
	if ( SavedAnimAction != AnimAction && AnimAction != NAME_None )
		eventSetAnimAction(AnimAction);
	if ( SavedHeadScale != HeadScale )
		eventSetHeadScale(HeadScale);
	unguardSlow;
}

void APawn::PostNetReceiveLocation()
{
	guardSlow(APawn::PostNetReceiveLocation);

	if ( Physics == PHYS_Karma )
	{
		AActor::PostNetReceiveLocation();
	}
	else if( Role == ROLE_SimulatedProxy )
	{
		FCheckResult Hit(1.f);
		if ( GetLevel()->EncroachingWorldGeometry(Hit,SavedLocation,GetCylinderExtent(),0,Level) )
		{
			if ( CollisionRadius == GetClass()->GetDefaultActor()->CollisionRadius )
				SetCollisionSize(CollisionRadius - 1.f, CollisionHeight - 1.f);
			bSimGravityDisabled = true;
		}
		else if ( Velocity.IsZero() )
			bSimGravityDisabled = true;
		else 
		{
			SavedLocation.Z += 2.f;
			bSimGravityDisabled = false;
		}
		FVector OldLocation = Location;
		GetLevel()->FarMoveActor( this, SavedLocation, 0, 1, 1 );
		if ( !bSimGravityDisabled )
		{
			// smooth out movement of other players to account for frame rate induced jitter
			// look at whether location is a reasonable approximation already 
			// if so only partially correct
			FVector Dir = OldLocation - Location;
			FLOAT StartError = Dir.Size();
			if ( StartError > 4.f )
			{
				StartError = ::Min(0.5f * StartError,CollisionRadius);
				Dir.Normalize();
				moveSmooth(StartError * Dir);
			}
		}
	}
	else
		AActor::PostNetReceiveLocation();

	unguardSlow;
}

void AMover::PostNetReceive()
{
	guard(AMover::PostNetReceive);

	AActor::PostNetReceive();
	if( SavedSimInterpolate != SimInterpolate )
	{
		OldPos = SimOldPos;
		OldRot.Yaw = SimOldRotYaw;
		OldRot.Pitch = SimOldRotPitch;
		OldRot.Roll = SimOldRotRoll;
		PhysAlpha = SimInterpolate.X * 0.01f;
		PhysRate = SimInterpolate.Y * 0.01f;
		INT keynums = (INT) SimInterpolate.Z;
		KeyNum = keynums & 255;
		PrevKeyNum = keynums >> 8;
		setPhysics(PHYS_MovingBrush);
		bInterpolating = true;
	}

	if ( StoppedPosition != ClientStop )
	{
		ClientStop = StoppedPosition;
		if ( ClientStop == 0 )
		{
			setPhysics(PHYS_MovingBrush);
		}
		else
		{
			setPhysics(PHYS_None);
		}
	}
	unguard;
}

void APlayerReplicationInfo::PostNetReceive()
{
	guard(APlayerReplicationInfo::PostNetReceive);

	AActor::PostNetReceive();
	if ( Level->NetMode == NM_Client )
	{
		if( GetLevel()->NetDriver &&
			GetLevel()->Engine->Client->Viewports(0)->Actor->PlayerReplicationInfo == this )
			Ping -= (INT) ((GetLevel()->NetDriver->ServerConnection->AverageFrameTime * 1000) / 2);
		if( Ping < 0 )
			Ping = 0;

		if ( PreviousName != PlayerName )
		{
			OldName = PreviousName;
			PreviousName = PlayerName;
			eventClientNameChange();
		}
		if ( CharacterName != OldCharacterName )
		{
			OldCharacterName = CharacterName;
			eventUpdateCharacter();
		}
	}
	unguard;
}

void AWeaponAttachment::PostNetReceive()
{
	guard(AWeaponAttachment::PostNetReceive);


	// If FlashCount changes, or bAutoFire changes state,
	// Trigger the ThirdPersonEffects

	AActor::PostNetReceive();

	if ( Instigator && (Instigator == Base) && !Instigator->bDeleteMe 
		&& (!Instigator->IsLocallyControlled() || (Instigator->Controller && Instigator->Controller->bDemoOwner)) )
	{
		if (Instigator->FlashCount != FlashCount) 
		{
			Instigator->FlashCount = FlashCount;
			eventThirdPersonEffects();
		}
		
		if (Instigator->bAutoFire != bAutoFire)
		{
			Instigator->bAutoFire = bAutoFire;
			eventThirdPersonEffects();
		}
	}

	unguard;
}

/*
FIXME - demo mode disabled
void APlayerController::PostNetReceive()
{
	guard(APlayerController::PostNetReceive);

	AActor::PostNetReceive();

	if( GetLevel()->DemoRecDriver && GetLevel()->DemoRecDriver->ServerConnection )
	{
		Rotation.Pitch = DemoViewPitch; 
		Rotation.Yaw = DemoViewYaw;
	} 
	unguard;
} 
*/
/*-----------------------------------------------------------------------------
	APlayerPawn implementation.
-----------------------------------------------------------------------------*/

//
// Set the player.
//
void APlayerController::SetPlayer( UPlayer* InPlayer )
{
	guard(APlayerController::SetPlayer);
	check(InPlayer!=NULL);

	// Detach old player.
	if( InPlayer->Actor )
		InPlayer->Actor->Player = NULL;

	// Set the viewport.
	check(Player == NULL);
	Player = InPlayer;
	InPlayer->Actor = this;
	InPlayer->OldActor = this;

	UNetDriver* Driver = GetLevel()->NetDriver;
	if( (ClientCap>1800) && Driver && Driver->ServerConnection )
		Player->CurrentNetSpeed = Driver->ServerConnection->CurrentNetSpeed = Clamp( ClientCap, 1800, Driver->MaxClientRate );

	// initialize the input system only if local player
	if ( Cast<UViewport>(InPlayer) )
		eventInitInputSystem();

	// Voice chat.
//	if( Level->NetMode == NM_Client && GetLevel()->Engine->Audio )
//	{
//		GetLevel()->Engine->Audio->EnterVoiceChat();
//	}

	// Debug message.
	//debugf( NAME_Log, TEXT("Possessed PlayerPawn: %s"), GetFullName() );

	unguard;
}

/*-----------------------------------------------------------------------------
	AZoneInfo.
-----------------------------------------------------------------------------*/

void AZoneInfo::PostEditChange()
{
	guard(AZoneInfo::PostEditChange);
	Super::PostEditChange();
	AmbientVector = FGetHSV(AmbientHue,AmbientSaturation,AmbientBrightness);
	if( GIsEditor )
	{
		XLevel->Engine->Flush(0);

		for(INT ActorIndex = 0;ActorIndex < XLevel->Actors.Num();ActorIndex++)
			if(XLevel->Actors(ActorIndex))
				XLevel->Actors(ActorIndex)->ClearRenderData();
	}
	else
		RealDistanceFogEnd = DistanceFogEnd;
	unguard;
}

/*-----------------------------------------------------------------------------
	AActor.
-----------------------------------------------------------------------------*/

void AActor::Destroy()
{
	guard(AActor::Destroy);

	INT i;
	while( (i=Projectors.Num()) > 0)
	{
		Projectors(i-1)->RemoveReference();
		Projectors.Remove(i-1);		
	}
		
	for(INT ProjectorIndex = 0;ProjectorIndex < StaticMeshProjectors.Num();ProjectorIndex++)
	{
		StaticMeshProjectors(ProjectorIndex)->RenderInfo->RemoveReference();
		delete StaticMeshProjectors(ProjectorIndex);
	}

	StaticMeshProjectors.Empty();

	if(LightRenderData)
	{
		delete LightRenderData;
		LightRenderData = NULL;
	}

	if(ActorRenderData)
	{
		delete ActorRenderData;
		ActorRenderData = NULL;
	}

#ifdef WITH_KARMA
    KTermActorKarma(this);
#endif
#ifdef WITH_KARMA
	if(XLevel)
		check(XLevel->KContactGenActors.FindItemIndex(this) == INDEX_NONE);
#endif
#ifdef WITH_KARMA
	// If this is not a garbage-collection destroy, manually destroy the KParams.
	if( KParams && !GIsGarbageCollecting )
	{
		delete KParams;
		KParams = NULL;
	}
#endif

    if( this->IsInOctree() )
	{
		if(XLevel && XLevel->Hash)
			XLevel->Hash->RemoveActor(this);
		else
			debugf( TEXT("Destroying Actor (%s) that is in Octree - but can't find Octree."), this->GetName() );
	}
    
	UObject::Destroy();

	unguard;
}

void AActor::PostLoad()
{
	guard(AActor::PostLoad);
	Super::PostLoad();

	if( GetClass()->ClassFlags & CLASS_Localized )
		LoadLocalized();
	if( Brush )
		Brush->SetFlags( RF_Transactional );
	if( Brush && Brush->Polys )
		Brush->Polys->SetFlags( RF_Transactional );

#ifdef __PSX2_EE__
	if (StaticMesh && StaticMeshInstance)
	{
		extern void PreCachePS2StaticMeshLighting(UStaticMesh*, UStaticMeshInstance*);
		PreCachePS2StaticMeshLighting(StaticMesh, StaticMeshInstance);
		// you always have 1 which is the root or base collision primitive
	}
#endif

	// check for empty Attached entries
	for ( INT i=0; i<Attached.Num(); i++ )
		if ( (Attached(i) == NULL) || (Attached(i)->Base != this) || Attached(i)->bDeleteMe )
		{
			Attached.Remove(i);
			i--;
		}

	// check for empty Touching entries
	for ( INT i=0; i<Touching.Num(); i++ )
		if ( (Touching(i) == NULL) || Touching(i)->bDeleteMe )
		{
			Touching.Remove(i);
			i--;
		}

	unguard;
}

// amb ---
void AActor::ScriptInit(class ALevelInfo* levelinfo)
{
    Level  = levelinfo;
    XLevel = Level->XLevel;
}
// --- amb

void AActor::ProcessEvent( UFunction* Function, void* Parms, void* Result )
{
	guardSlow(AActor::ProcessEvent);
	if( Level->bBegunPlay && !GIsGarbageCollecting )
		Super::ProcessEvent( Function, Parms, Result );
	unguardSlow;
}

void AActor::PostEditChange()
{
	guard(AActor::PostEditChange);
	Super::PostEditChange();

	for ( INT i=0; i<Attached.Num(); i++ )
		if ( Attached(i) == NULL )
		{
			Attached.Remove(i);
			i--;
		}
	if( GIsEditor )
		bLightChanged = 1;

	ClearRenderData();
	unguard;
}


// Called before we have done undo things.
void AActor::PreEditUndo()
{
	guard(AActor::PreEditUndo);

	// Before we do undo things to the actor, we remove it from the octree.

	// Don't bother doing anything if we dont have collision on.
	if(bCollideActors)
	{
		ULevel* level = GetLevel();
		if( !level || !level->Hash )
			return;

		level->Hash->RemoveActor(this);
	}

	unguard;
}

// Called after we have done undo things.
void AActor::PostEditUndo()
{
	guard(AActor::PostEditUndo);

	// After we have done undo things to the actor, we put it back in to the octree.
	// The undo stuff may have messed with the actors octree info, but we know for sure
	// it isn't in there because we removed it in PreEditUndo, so we reset it here.
	ClearOctreeData();

	if(bCollideActors)
	{
		ULevel* level = GetLevel();
		if( !level || !level->Hash )
			return;

		level->Hash->AddActor(this);
	}

	unguard;
}

// WARNING: This does NOT remove the Actor from the Octree. Almost always you should use Hash->RemoveActor.
// This is just to re-zero the data if you are sure the actor is not in the octree.
void AActor::ClearOctreeData()
{
	guard(AActor::ClearOctreeData);

	OctreeNodes.Empty();

	unguard;
}


//
// Set the actor's collision properties.
//
void AActor::SetCollision
(
	UBOOL NewCollideActors,
	UBOOL NewBlockActors
)
{
	guard(AActor::SetCollision);

	UBOOL OldCollideActors = bCollideActors;

	// Remove this actor from the hash.
	if( bCollideActors && GetLevel()->Hash )
		GetLevel()->Hash->RemoveActor( this );

	// Untouch everything if we're turning collision off.
	if( bCollideActors && !NewCollideActors )
	{
		for( int i=0; i<Touching.Num(); )
		{
			if( Touching(i) )
				Touching(i)->EndTouch( this, 0 );
			else
				i++;
		}
	}

	// Set properties.
	bCollideActors = NewCollideActors;
	bBlockActors   = NewBlockActors;

	// Add this actor to the hash.
	if( bCollideActors && GetLevel()->Hash )
		GetLevel()->Hash->AddActor( this );

	// Touch.
	if( NewCollideActors && !OldCollideActors )
	{
		FMemMark Mark(GMem);
		FCheckResult* FirstHit = GetLevel()->Hash ? GetLevel()->Hash->ActorEncroachmentCheck( GMem, this, Location, Rotation, TRACE_AllColliding, 0 ) : NULL;	
		for( FCheckResult* Test = FirstHit; Test; Test=Test->GetNext() )
			if(	Test->Actor!=this &&
				!Test->Actor->IsJoinedTo(this) &&
				Test->Actor != Level )
			{ 
				if( !IsBlockedBy(Test->Actor) ) 
					BeginTouch( Test->Actor );
			}						
		Mark.Pop();
	}

	bNetDirty = true; //for network replication

#ifdef WITH_KARMA
	if(bBlockKarma && !GIsEditor) // Never generate contacts in the editor
	{
		// If turning on bCollideActors for an actor with bBlockKarma true and in a Karma physics mode,
		// add it to the KActorContactGen list.
		if(bCollideActors && !OldCollideActors && (Physics == PHYS_KarmaRagDoll || Physics == PHYS_Karma) )
			KActorContactGen(this, 1);
		// If turning off bCollideActors, ensure it is no longer in the contact gen list, and remove any existing pairs.
		else if(!bCollideActors && OldCollideActors)
		{
			KActorContactGen(this, 0);
			KGoodbyeActorAffectedPairs(this);
		}
	}
#endif

	unguard;
}

//
// Set collision size.
//
void AActor::SetCollisionSize( FLOAT NewRadius, FLOAT NewHeight )
{
	guard(AActor::SetCollisionSize);

	// Untouch this actor.
	if( bCollideActors && GetLevel() && GetLevel()->Hash )
		GetLevel()->Hash->RemoveActor( this );

	// Set properties.
	CollisionRadius = NewRadius;
	CollisionHeight = NewHeight;

	// Touch this actor.
	if( bCollideActors && GetLevel() && GetLevel()->Hash )
		GetLevel()->Hash->AddActor( this );

	/* FIXME - commented out for UT2003, until I have time to get it in w/ no issues
	// check that touching array is correct.
	if ( !bTest )
		GetLevel()->CheckEncroachment(this,Location,Rotation,true);
	*/
	bNetDirty = true;	// for network replication
	unguard;
}

void AActor::SetDrawScale( FLOAT NewScale )
{
	guard(AActor::SetDrawScale);

	if( bCollideActors && GetLevel()->Hash )
		GetLevel()->Hash->RemoveActor( this );

	DrawScale = NewScale;

	if( bCollideActors && GetLevel()->Hash )
		GetLevel()->Hash->AddActor( this );
	bNetDirty = true;	// for network replication
	ClearRenderData();
	unguard;
}

void AActor::SetStaticMesh( UStaticMesh* NewStaticMesh)
{
	guard(AActor::SetStaticMesh);

	if ( StaticMesh == NewStaticMesh )
		return;
	if( bCollideActors && GetLevel()->Hash )
		GetLevel()->Hash->RemoveActor( this );

	StaticMesh = NewStaticMesh;
	GetLevel()->ResetStaticFilterState( this );

	if( bCollideActors && GetLevel()->Hash )
		GetLevel()->Hash->AddActor( this );

	bNetDirty = true;
	ClearRenderData();

	unguard;
}

void AActor::SetDrawType( EDrawType NewDrawType  )
{
	guard(AActor::SetDrawScale);

	if( bCollideActors && GetLevel()->Hash )
		GetLevel()->Hash->RemoveActor( this );

	DrawType = NewDrawType;

	if( bCollideActors && GetLevel()->Hash )
		GetLevel()->Hash->AddActor( this );
	bNetDirty = true;	// for network replication
	ClearRenderData();
	unguard;
}

void AActor::SetDrawScale3D( FVector NewScale3D )
{
	guard(AActor::SetDrawScale3D);

	if( bCollideActors && GetLevel()->Hash )
		GetLevel()->Hash->RemoveActor( this );

	DrawScale3D = NewScale3D;

	if( bCollideActors && GetLevel()->Hash )
		GetLevel()->Hash->AddActor( this );
	bNetDirty = true;	// for network replication
	ClearRenderData();
	unguard;
}

/* Update relative rotation - called by ULevel::MoveActor()
 don't update RelativeRotation if attached to a bone -
 if attached to a bone, only update RelativeRotation directly
*/
void AActor::UpdateRelativeRotation()
{
	guard(AActor::UpdateRelativeRotation);

	if ( !Base || Base->bWorldGeometry || (AttachmentBone != NAME_None) )
		return;

	// update RelativeRotation which is the rotation relative to the base's rotation
	FCoords BaseCoords = GMath.UnitCoords * Base->Rotation;
	FCoords Coords = GMath.UnitCoords / Rotation;

	FCoords RelativeCoords = Coords/BaseCoords.OrthoRotation();
	RelativeRotation = RelativeCoords.OrthoRotation();

	unguard;
}



//
// Return whether this actor overlaps another.
// Called normally from MoveActor, to see if we should 'untouch' things.
// Normally - the only things that can overlap an actor are:
// Volumes, Projectors and FluidSurfaces
// However, we also use this test during ActorEncroachmentCheck, so we support
// Encroachers (ie. Movers and Karma actors) overlapping actors.
//
UBOOL AActor::IsOverlapping( AActor* Other, FCheckResult* Hit )
{
	guardSlow(AActor::IsOverlapping);
	checkSlow(Other!=NULL);

	if ( (this->IsBrush() && Other->IsBrush()) || (Other == Level) )
	{
		// We cannot detect whether these actors are overlapping so we say they aren't.
		return 0;
	}

	// Things dont overlap themselves
	if(this == Other)
		return 0;

	// Things that do encroaching (movers, karma actors etc.) can't encroach each other!
	if(this->IsEncroacher() && Other->IsEncroacher())
		return 0;

	// Things that are joined together dont overlap.
	if( this->IsJoinedTo(Other) || Other->IsJoinedTo(this) )
		return 0;

	// If one only affects pawns, and the other isn't a pawn, dont check.
	if(this->bOnlyAffectPawns && !Other->GetAPawn() )
		return 0;

	if(Other->bOnlyAffectPawns && !this->GetAPawn() )
		return 0;

	// If one thing doesn't block non-zero traces, they can't overlap
	if(!(this->bBlockNonZeroExtentTraces || Other->bBlockNonZeroExtentTraces) && !(this->IsA(AProjector::StaticClass()) || Other->IsA(AProjector::StaticClass())))
		return 0;

	// Don't let xEmitters and Movers overlap
	if(this->IsEncroacher() && Other->IsA(AxEmitter::StaticClass()))
		return 0;

	if(Other->IsEncroacher() && this->IsA(AxEmitter::StaticClass()))
		return 0;

	AActor* PrimitiveActor = this;
	AActor* BoxActor = Other;
	
	// Projectors should always use their primitive - fastest option.
	if( this->IsA(AProjector::StaticClass()) )
	{
		PrimitiveActor = this;
		BoxActor = Other;	
	}
	else
	if( Other->IsA(AProjector::StaticClass()) )
	{
		BoxActor = this;
		PrimitiveActor = Other;	
	}
	// If one is a volume or fluid surface- make sure you us its primitive.
	else if( this->IsVolumeBrush() || this->IsA(AFluidSurfaceInfo::StaticClass()) )
	{
		PrimitiveActor = this;
		BoxActor = Other;	
	}
	else 
	if( Other->IsVolumeBrush() || Other->IsA(AFluidSurfaceInfo::StaticClass()) )
	{
		BoxActor = this;
		PrimitiveActor = Other;	
	}
	// If one is an encroacher, use its primitive.
	// This is quite expensive, but should only happen for pushing pawns around,
	// where you need the accuracy.
	else 
	if(this->IsEncroacher())
	{
		PrimitiveActor = this;
		BoxActor = Other;	
	}
	else
	if(Other->IsEncroacher())
	{
		BoxActor = this;
		PrimitiveActor = Other;	
	}
	else
	{
	// See if cylinder actors are overlapping.
	return
			( (Square(Location.Z - Other->Location.Z) < Square(CollisionHeight + Other->CollisionHeight))
			&&	(Square(Location.X - Other->Location.X) + Square(Location.Y - Other->Location.Y)
				< Square(CollisionRadius + Other->CollisionRadius)) );
	}

	// PointCheck primitive with box
	FCheckResult TestHit;
	if(Hit==NULL)
		Hit = &TestHit;

	if( GetLevel()->bShowSlowOverlap )
	{
		// DEBUGGING! Print point check if it takes too long.
		DWORD Time=0;
		clock(Time);

		UBOOL pCheck = PrimitiveActor->GetPrimitive()->PointCheck( *Hit, PrimitiveActor, 
			BoxActor->GetPrimitive()->GetEncroachCenter(BoxActor), 
			BoxActor->GetPrimitive()->GetEncroachExtent(BoxActor), 0 )==0;

		unclock(Time);
		FLOAT mSec = Time * GSecondsPerCycle * 1000.f;
		if(mSec > GetLevel()->SlowOverlapThreshTime)
			debugf(TEXT("IOL: Testing: P:%s - B:%s Time: %f"), PrimitiveActor->GetName(), BoxActor->GetName(), mSec );

		return pCheck;
	}
	else
	{
		return PrimitiveActor->GetPrimitive()->PointCheck( *Hit, PrimitiveActor, 
			BoxActor->GetPrimitive()->GetEncroachCenter(BoxActor), 
			BoxActor->GetPrimitive()->GetEncroachExtent(BoxActor), 0 )==0;
	}

	unguardSlow;
}

//
// Get the actor's primitive.
//
UPrimitive* AActor::GetPrimitive()
{
	guardSlow(AActor::GetPrimitive);

	if		( Mesh				)	return Mesh;
    else if ( StaticMesh        )   return (UPrimitive*) StaticMesh;
    else if	( AntiPortal		)	return AntiPortal;
	else if	( GetLevel()->Engine)	return GetLevel()->Engine->Cylinder;
	else							return NULL;
	unguardSlow;
}

UPrimitive* ABrush::GetPrimitive()
{
	guardSlow(ABrush::GetPrimitive);
	if		( Brush				)	return Brush;
	else if	( GetLevel()->Engine)	return GetLevel()->Engine->Cylinder;
	else							return NULL;
	unguardSlow;
}

/*-----------------------------------------------------------------------------
	Actor touch minions.
-----------------------------------------------------------------------------*/

static UBOOL TouchTo( AActor* Actor, AActor* Other )
{
	guard(TouchTo);
	check(Actor);
	check(Other);
	check(Actor!=Other);

	for ( INT j=0; j<Actor->Touching.Num(); j++ )
	  if ( Actor->Touching(j) == Other )
		return 1;
 
	// Make Actor touch TouchActor.
	Actor->Touching.AddItem(Other);
	Actor->eventTouch( Other );

	// See if first actor did something that caused an UnTouch.
	INT i = 0;
	return ( Actor->Touching.FindItem(Other,i) );

	unguard;
}

//
// Note that TouchActor has begun touching Actor.
//
// This routine is reflexive.
//
// Handles the case of the first-notified actor changing its touch status.
//
void AActor::BeginTouch( AActor* Other )
{
	guard(AActor::BeginTouch);

	// Perform reflective touch.
	if( TouchTo( this, Other ) )
		TouchTo( Other, this );

	unguard;
}

//
// Note that TouchActor is no longer touching Actor.
//
// If NoNotifyActor is specified, Actor is not notified but
// TouchActor is (this happens during actor destruction).
//
void AActor::EndTouch( AActor* Other, UBOOL bNoNotifySelf )
{
	guard(AActor::EndTouch);
	check(Other!=this);

	// Notify Actor.
	INT i=0;
	if ( !bNoNotifySelf && Touching.FindItem(Other,i) )
	{
		eventUnTouch( Other );
	}
	Touching.RemoveItem(Other);

	if ( Other->Touching.FindItem(this,i) )
	{
		Other->eventUnTouch( this );
		Other->Touching.RemoveItem(this);
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	AActor member functions.
-----------------------------------------------------------------------------*/

//
// Destroy the actor.
//
void AActor::Serialize( FArchive& Ar )
{
	guard(AActor::Serialize);
	Super::Serialize( Ar );
	if(!Ar.IsLoading() && !Ar.IsSaving())
		Ar	<< Projectors
			<< StaticMeshProjectors;
	unguard;
}

FDynamicActor* AActor::GetActorRenderData()
{
	if(!ActorRenderData)
		ActorRenderData = new FDynamicActor(this);

	if(ActorRenderData->Revision != RenderRevision)
		UpdateRenderData();

	return ActorRenderData;
}

FDynamicLight* AActor::GetLightRenderData()
{
	if(!LightRenderData)
		LightRenderData = new FDynamicLight(this);

	if(LightRenderData->Revision != RenderRevision)
		LightRenderData->Update();

	return LightRenderData;
}

void AActor::ClearRenderData()
{
	RenderRevision++;

	// Remove antiportals from the old zones they touch.

	if(!GIsEditor && AntiPortal)
	{
		for(INT LeafIndex = 0;LeafIndex < Leaves.Num();LeafIndex++)
		{
			FLeaf&	Leaf = XLevel->Model->Leaves(Leaves(LeafIndex));

			XLevel->ZoneRenderInfo(Leaf.iZone).AntiPortals.RemoveItem(this);
		}
	}
}

static bool ContainsBox( const FBox& a, const FBox& b )
{
    if( !a.IsValid )
        return false;
	if (!FPointBoxIntersection(b.Min,a))
		return false;
	if (!FPointBoxIntersection(b.Max,a))
		return false;
	return true;
}

float const PredictedBoxTweak = 128.0f;
void AActor::UpdateRenderData()
{
	guard(AActor::UpdateRenderData);
 
	clock(GStats.DWORDStats(GEngineStats.STATS_Game_UpdateRenderData));	

    if( bDeleteMe ) // touch events dispatched from moveactor can cause this
        return;

	check(ActorRenderData);
	ActorRenderData->Update();

    if( !GIsEditor && ContainsBox( ActorRenderData->PredictedBox, ActorRenderData->BoundingBox ) ) 
    {
        FLOAT	AmbientGlow;       
	    if(GetAmbientLightingActor()->AmbientGlow == 255)
		    AmbientGlow = 0.25f + 0.2f * appSin(8.0f * Level->TimeSeconds);
	    else
		    AmbientGlow = GetAmbientLightingActor()->AmbientGlow / 255.0;
        ActorRenderData->AmbientColor = FColor(ActorRenderData->AmbientVector + FVector(AmbientGlow,AmbientGlow,AmbientGlow));

		// Add antiportals to the zones they touch.

		if(!GIsEditor && DrawType == DT_AntiPortal && AntiPortal)
		{
			for(INT LeafIndex = 0;LeafIndex < Leaves.Num();LeafIndex++)
			{
				FLeaf&	Leaf = XLevel->Model->Leaves(Leaves(LeafIndex));

				XLevel->ZoneRenderInfo(Leaf.iZone).AntiPortals.AddUniqueItem(this);
			}
		}

		unclock(GStats.DWORDStats(GEngineStats.STATS_Game_UpdateRenderData));
        return;
    }

    // todo: add some bbox prediction for emitters
    if ( Physics == PHYS_Rotating )
    {
        FVector center, extents;
        ActorRenderData->BoundingBox.GetCenterAndExtents(center, extents);
        ActorRenderData->PredictedBox.IsValid = 1;
        ActorRenderData->PredictedBox.Min = ActorRenderData->PredictedBox.Max = center;
        ActorRenderData->PredictedBox = ActorRenderData->PredictedBox.ExpandBy(extents.Size()*1.2f);
    }
    else if ( Physics == PHYS_None )
    {
        ActorRenderData->PredictedBox = ActorRenderData->BoundingBox;
    }
    else
    {        
        ActorRenderData->PredictedBox = ActorRenderData->BoundingBox;
        //ActorRenderData->PredictedBox += (ActorRenderData->BoundingBox.Min + Velocity * 1.5f);
        //ActorRenderData->PredictedBox += (ActorRenderData->BoundingBox.Max + Velocity * 1.5f);
        ActorRenderData->PredictedBox = ActorRenderData->PredictedBox.ExpandBy(PredictedBoxTweak);
    }

    Leaves.Empty(Leaves.Num());

#if 0
    if( DrawType == DT_Particle )
    {
        // expand by %5
        FVector center, extents;
        ActorRenderData->PredictedBox.GetCenterAndExtents(center, extents);
        float radius = extents.Size();
        radius *= 0.05f;
        ActorRenderData->PredictedBox = ActorRenderData->PredictedBox.ExpandBy(radius);
    }
#endif

	(TArray<INT>&)Leaves = XLevel->Model->BoxLeaves(ActorRenderData->PredictedBox);

	if(ForcedVisibilityZoneTag != NAME_None)
	{
		for(INT LeafIndex = 0;LeafIndex < Leaves.Num();LeafIndex++)
		{
			FLeaf&		Leaf = XLevel->Model->Leaves(Leaves(LeafIndex));
			AZoneInfo*	ZoneInfo = XLevel->GetZoneActor(Leaf.iZone);

			if(ZoneInfo->Tag != ForcedVisibilityZoneTag)
				Leaves.Remove(LeafIndex--);
		}
	}

	// Calculate the ambient lighting.

	FVector	AmbientVector(0,0,0);

	for(INT LeafIndex = 0;LeafIndex < Leaves.Num();LeafIndex++)
	{
		FLeaf&		Leaf = XLevel->Model->Leaves(Leaves(LeafIndex));
		AZoneInfo*	ZoneInfo = XLevel->GetZoneActor(Leaf.iZone);
		FVector&	ZoneAmbientVector = ZoneInfo->AmbientVector;

		AmbientVector.X = Max(AmbientVector.X,ZoneAmbientVector.X);
		AmbientVector.Y = Max(AmbientVector.Y,ZoneAmbientVector.Y);
		AmbientVector.Z = Max(AmbientVector.Z,ZoneAmbientVector.Z);
	}

    ActorRenderData->AmbientVector = AmbientVector;

	FLOAT	AmbientGlow;

	if(GetAmbientLightingActor()->AmbientGlow == 255)
		AmbientGlow = 0.25f + 0.2f * appSin(8.0f * Level->TimeSeconds);
	else
		AmbientGlow = GetAmbientLightingActor()->AmbientGlow / 255.0;

	ActorRenderData->AmbientColor = FColor(ActorRenderData->AmbientVector + FVector(AmbientGlow,AmbientGlow,AmbientGlow));

	// Add antiportals to the zones they touch.
	if(!GIsEditor && DrawType == DT_AntiPortal && AntiPortal)
	{
		for(INT LeafIndex = 0;LeafIndex < Leaves.Num();LeafIndex++)
		{
			FLeaf&	Leaf = XLevel->Model->Leaves(Leaves(LeafIndex));

			XLevel->ZoneRenderInfo(Leaf.iZone).AntiPortals.AddUniqueItem(this);
		}
	}

	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_UpdateRenderData));

	unguardf((TEXT("%s"),GetFullName()));
}

/*-----------------------------------------------------------------------------
	Relations.
-----------------------------------------------------------------------------*/

//
// Change the actor's owner.
//
void AActor::SetOwner( AActor *NewOwner )
{
	guard(AActor::SetOwner);

	// Sets this actor's parent to the specified actor.
	if( Owner != NULL )
		Owner->eventLostChild( this );

	Owner = NewOwner;

	if( Owner != NULL )
		Owner->eventGainedChild( this );
	bNetDirty = true;
	unguard;
}

UBOOL AActor::ShouldUpdateRelativeLocation()
{
	return !bHardAttach || (Role == ROLE_Authority);
}

UBOOL APawn::ShouldUpdateRelativeLocation()
{
	return !bHardAttach || (Role == ROLE_Authority) || DrivenVehicle;
}

//
// Change the actor's base.
//
void AActor::SetBase( AActor* NewBase, FVector NewFloor, int bNotifyActor )
{
	guard(AActor::SetBase);
	//debugf(TEXT("SetBase %s -> %s"),GetName(),NewBase ? NewBase->GetName() : TEXT("NULL"));

	// Verify no recursion.
	for( AActor* Loop=NewBase; Loop!=NULL; Loop=Loop->Base )
		if ( Loop == this ) 
			return;

	if( NewBase != Base )
	{
		// Notify old base, unless it's the level or terrain (but not movers).
		if( Base && !Base->bWorldGeometry )
		{
			Base->Attached.RemoveItem(this);
			Base->eventDetach( this );
		}

		// Set base.
		Base = NewBase;
		if ( Base && !Base->bWorldGeometry && (AttachmentBone == NAME_None) && !bOnlyDrawIfAttached ) 
		{
			if ( ShouldUpdateRelativeLocation() )
			{
				RelativeLocation = Location - Base->Location;
				UpdateRelativeRotation();
			}

			// Calculate the transform of this actor relative to its base.
			if(bHardAttach)
			{
				FMatrix BaseInvTM = FTranslationMatrix(-Base->Location) * FInverseRotationMatrix(Base->Rotation);
				FMatrix ChildTM = FRotationMatrix(Rotation) * FTranslationMatrix(Location);

				HardRelMatrix =  ChildTM * BaseInvTM;
			}
		}

		// Notify new base, unless it's the level.
		if( Base && !Base->bWorldGeometry )
		{
			Base->Attached.AddItem(this);
			Base->eventAttach( this );
		}

		// Notify this actor of his new floor.
		if ( bNotifyActor )
			eventBaseChange();
	}
	if ( (!Base || Base->bWorldGeometry) && (Level->NetMode != NM_Client) )
		AttachmentBone = NAME_None;

	unguard;
}

//
//	Same as IsBasedOn - except that this function also returns True if Other is connected
//	to something that This is based on by a KConstraint.
//
UBOOL AActor::IsJoinedTo( const AActor *Other) const
{
	guard(AActor::IsJoinedTo);

	// A JointedTag of 0 means this has never been joined to anything,
	// so that Actors can't be jointed together!
	for( const AActor* Test=this; Test!=NULL; Test=Test->Base )
		if( Test == Other || (Test && Other && Test->JoinedTag == Other->JoinedTag && Test->JoinedTag != 0) )
			return 1;
	return 0;

	unguard;
}

//
// Determine if BlockingActor should block actors of the given class.
// This routine needs to be reflexive or else it will create funky
// results, i.e. A->IsBlockedBy(B) <-> B->IsBlockedBy(A).
//
inline UBOOL AActor::IsBlockedBy( const AActor* Other ) const
{
	guardSlow(AActor::IsBlockedBy);
	checkSlow(this!=NULL);
	checkSlow(Other!=NULL);

	if( Other->bWorldGeometry )
		return bCollideWorld;
	else if(IsEncroacher() && Other->bIgnoreEncroachers)
		return false;
	else if(Other->IsEncroacher() && bIgnoreEncroachers)
		return false;
	else if( Other->IsBrush() || Other->IsEncroacher() ) 
	{
		if ( Other->IsMovingBrush() )
		{
			if( ((AActor*)this)->GetAPlayerController() )
				return false;
		}
		else if ( bIgnoreVehicles && Other->IsA(AVehicle::StaticClass()) )
			return false;
		return bCollideWorld && Other->bBlockActors;
	}
	else if ( IsBrush() || IsEncroacher() ) 
	{
		if ( IsMovingBrush() )
		{
			if ( ((AActor*)Other)->GetAPlayerController() )
				return false;
		}
		else if ( Other->bIgnoreVehicles && IsA(AVehicle::StaticClass()) )
			return false;
		return Other->bCollideWorld && bBlockActors;
	}
	else if ( Other->bBlockProjectiles && IsA(AProjectile::StaticClass()) )
		return true;
	else if ( bBlockProjectiles && Other->IsA(AProjectile::StaticClass()) )
		return true;
	else
		return ( Other->bBlockActors && bBlockActors );

	unguardSlow;
}

UBOOL AActor::AttachToBone( AActor* Attachment, FName BoneName )
{
	guardSlow(AActor::AttachToBone);
	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{				
		INT BoneIndex = -1;

		// Verify that the bone exists in the aliases or bones list.
		BoneIndex = ((USkeletalMeshInstance*) Mesh->MeshGetInstance(this))->MatchRefBone( BoneName );

		if( BoneIndex > -1 && Attachment && Attachment->IsValid() )
		{
			// No checks for previous..
			Attachment->AttachmentBone = BoneName;
			// Attach to base
			Attachment->SetBase( this, FVector(0,0,1) );			
			//debugf(TEXT("AttachToBonel: attachment made to bone named [s] in actor %s's skeleton."),GetName()); 
			return 1;
		}
		else
		{
			if( BoneIndex == -1 )
				debugf(TEXT("AttachToBone: No bone named [%s] found in actor %s's skeleton."),*BoneName, GetName() );		
			else
				debugf(TEXT("AttachToBone: Tried to attach invalid actor to bone %s of actor %s ."),*BoneName, GetName() );
		}
	}
    else if( Mesh ) // sjs - non-USkeletalMesh attachment!
	{				
		INT BoneIndex = -1;

		// Verify that the bone exists in the aliases or bones list.
		BoneIndex = (Mesh->MeshGetInstance(this))->MatchRefBone( BoneName );
		if( /*BoneIndex > -1 &&*/ Attachment && Attachment->IsValid() ) // BoneIndex won't be valid for deca because BoneName is not a bone name but an attachment tag name, dig?
		{
			// No checks for previous..
			Attachment->AttachmentBone = BoneName;
			// Attach to base
			Attachment->SetBase( this, FVector(0,0,1) );			
			//debugf(TEXT("AttachToBonel: attachment made to bone named [s] in actor %s's skeleton."),GetName()); 
			return 1;
		}
		else
		{
			if( BoneIndex == -1 )
				debugf(TEXT("AttachToBone: No bone named [%s] found in actor %s's skeleton."),*BoneName, GetName() );		
			else
				debugf(TEXT("AttachToBone: Tried to attach invalid actor to bone %s of actor %s ."),*BoneName, GetName() );
		}
	}
	return 0;
	unguardSlow;
}

UBOOL AActor::DetachFromBone( AActor* Attachment )
{
	guardSlow(AActor::DetachFromBone);
	if( Mesh ) // sjs && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{				
		// Remove it..
		Attachment->SetBase( NULL, FVector(0,0,1) );
		Attachment->AttachmentBone = NAME_None;
		return 1; // Add more useful success/fail reporting ?
	}
	return 0;
	unguardSlow;
}

void APawn::SetBase( AActor* NewBase, FVector NewFloor, int bNotifyActor )
{
	guard(APawn::SetBase);

	Floor = NewFloor;
	Super::SetBase(NewBase,NewFloor,bNotifyActor);
	unguard;
}

/*-----------------------------------------------------------------------------
	Special editor support.
-----------------------------------------------------------------------------*/
AActor* AActor::GetHitActor()
{
	return this;
}

/*---------------------------------------------------------------------------------------
	Brush class implementation.
---------------------------------------------------------------------------------------*/

void ABrush::InitPosRotScale()
{
	guard(ABrush::InitPosRotScale);
	check(Brush);
	
	MainScale = GMath.UnitScale;
	PostScale = GMath.UnitScale;
	Location  = FVector(0,0,0);
	Rotation  = FRotator(0,0,0);
	PrePivot  = FVector(0,0,0);

	unguard;
}
void ABrush::PostLoad()
{
	guard(ABrush::PostLoad);
	Super::PostLoad();
	unguard;
}

/*---------------------------------------------------------------------------------------
	Tracing check implementation.
	ShouldTrace() returns true if actor should be checked for collision under the conditions 
	specified by traceflags
---------------------------------------------------------------------------------------*/

UBOOL AActor::ShouldTrace(AActor *SourceActor, DWORD TraceFlags)
{
	guard(AActor::ShouldTrace);

	// Skip actors without bShadowCast when raytracing for shadows.
	if( Level && this!=Level && (!this->bShadowCast) && (TraceFlags & TRACE_ShadowCast) )
		return false;

	// Check bOnlyAffectPawns flag. APawn has its own ShouldTrace, so this actor can't be one.
	if( SourceActor && SourceActor->bOnlyAffectPawns )
		return false;

	if( this->bOnlyAffectPawns && SourceActor && !SourceActor->GetAPawn() )
		return false;

	if(TraceFlags & TRACE_AcceptProjectors)
		return bAcceptsProjectors;

	if((TraceFlags & TRACE_Corona) && 
		((StaticMesh && DrawType == DT_StaticMesh) || (Mesh && DrawType == DT_Mesh)) )
		return true;

	if( TraceFlags & TRACE_ShadowCast )
		return bShadowCast;
	else if ( bWorldGeometry )
		return (TraceFlags & TRACE_LevelGeometry);
	else if( TraceFlags & TRACE_Others )
	{
		if( TraceFlags & TRACE_OnlyProjActor )
		{
			if( bProjTarget || bBlockActors )
				return true;
		}
		else if ( TraceFlags & TRACE_Blocking )
		{
			if ( SourceActor && SourceActor->IsBlockedBy(this) )
				return true;
		}
		else
			return true;
	}
		
	return false;
	unguard;
}

UBOOL AVolume::ShouldTrace(AActor *SourceActor, DWORD TraceFlags)
{
	guard(AVolume::ShouldTrace);

	// Skip actors without bShadowCast when raytracing for shadows.
	if( !this->bShadowCast && (TraceFlags & TRACE_ShadowCast) )
		return false;

	if( SourceActor && SourceActor->bOnlyAffectPawns )
		return false;

	if( this->bOnlyAffectPawns && SourceActor && !SourceActor->GetAPawn() )
		return false;

	if ( bWorldGeometry && (TraceFlags & TRACE_LevelGeometry) )
		return true;
	if ( TraceFlags & TRACE_Volumes )
	{
		if( TraceFlags & TRACE_OnlyProjActor )
		{
			if( bProjTarget || bBlockActors )
				return true;
			if ( SourceActor && SourceActor->bTraceWater )
			{
				APhysicsVolume *PV = Cast<APhysicsVolume>(this);
				if ( PV && PV->bWaterVolume )
					return true;
			}
		}
		else if ( TraceFlags & TRACE_Blocking )
		{
			if ( SourceActor && SourceActor->IsBlockedBy(this) )
				return true;
		}
		else
			return true;
	}
	return false;
	unguard;
}

UBOOL ABlockingVolume::ShouldTrace(AActor *SourceActor, DWORD TraceFlags) // sjs
{
	guard(ABlockingVolume::ShouldTrace);

    if ( !bClassBlocker )
    {
        return Super::ShouldTrace( SourceActor, TraceFlags );
    }
    else if( Super::ShouldTrace( SourceActor, TraceFlags ) && SourceActor )
    {
        for( int i=0; i<BlockedClasses.Num(); i++ )
        {
            if( BlockedClasses(i) && SourceActor->IsA(BlockedClasses(i)) )
                return true;
        }
    }
    
    return false;

	unguard;
}

void AActor::CleanupSubObjects()
{
	guardSlow(CleanupSubObjects);

	// Clean up mesh instance.
	if( MeshInstance )
	{
		// debugf( NAME_Warning,TEXT("Deleting meshinstance for %s"),GetName()); 
		delete MeshInstance;
		MeshInstance = NULL;
	}
	unguardSlow;
}

void AActor::DoCleanup()
{
	guardSlow(AActor::DoCleanup);

	GetClass()->CleanupDestroyed( (BYTE*)this );
	unguardSlow;
}

void AWeapon::DoCleanup()
{
	guardSlow(AWeapon::DoCleanup);

	for ( INT i=0; i<UCONST_NUM_FIRE_MODES; i++ )
	{
		if ( FireMode[i] )
			FireMode[i]->GetClass()->CleanupDestroyed( (BYTE*)FireMode[i] );
	}

	GetClass()->CleanupDestroyed( (BYTE*)this );
	unguardSlow;
}

// AProjectile::PostRender - Allow for neat after process effects

void AProjectile::PostRender(FLevelSceneNode* SceneNode, FRenderInterface* RI)
{
	guard(AProjectile::PostRender);

	if ( bScriptPostRender )
	{
		FVector camLoc = SceneNode->WorldToCamera.TransformFVector(Location + FVector(0.f,0.f,CollisionHeight));
		FPlane  screenLoc = SceneNode->Project(SceneNode->CameraToWorld.TransformFVector(camLoc));
		screenLoc.X = (SceneNode->Viewport->Canvas->ClipX * 0.5f * (screenLoc.X + 1.f));
		screenLoc.Y = (SceneNode->Viewport->Canvas->ClipY * 0.5f * (-screenLoc.Y + 1.f));
		eventPostRender2D(SceneNode->Viewport->Canvas,screenLoc.X, screenLoc.Y);
	}

	unguard;
}


/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/

