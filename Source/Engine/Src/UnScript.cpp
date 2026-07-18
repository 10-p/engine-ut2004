/*=============================================================================
	UnScript.cpp: UnrealScript engine support code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Description:
	UnrealScript execution and support code.

Revision history:
	* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnRender.h"
#include "UnNet.h"

#include "xForceFeedback.h" 

void AActor::execDebugClock( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execDebugClock);
	P_FINISH;
	clock(GStats.DWORDStats(GEngineStats.STATS_Game_ScriptDebugTime));
	unguard;
}

void AActor::execResetStaticFilterState(FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execResetStaticFilterState);

	P_FINISH;

	GetLevel()->ResetStaticFilterState(this);
	unguardexec;
}

void AActor::execAddToPackageMap(FFrame& Stack, RESULT_DECL)
{
	guard(AActor::execAddToPackageMap);

	P_GET_STR_OPTX(PackageName, TEXT(""));
	P_FINISH;

	if (Level->NetMode != NM_DedicatedServer && Level->NetMode != NM_ListenServer)
		return;
	if (!GetLevel() || !GetLevel()->NetDriver || !GetLevel()->NetDriver->MasterMap || !Level || !Level->Game || Level->Game->bScriptInitialized)
		return;

	if (PackageName == TEXT(""))
	{
		//add this actor's package to the package map
		if (GetClass()->GetLinker())
		{
			debugf(NAME_DevNet, TEXT("Added %s to package map"), GetClass()->GetOuter()->GetName());
			GetLevel()->NetDriver->MasterMap->AddLinker(GetClass()->GetLinker());
			GetLevel()->NetDriver->MasterMap->Compute();
		}
	}
	else
	{
		BeginLoad();
		ULinkerLoad* Linker = GetPackageLinker(NULL, *PackageName, LOAD_NoWarn, NULL, NULL);
		if (!Linker)
			debugf(NAME_Warning, TEXT("AddToPackageMap: Invalid package %s"), *PackageName);
		else
		{
			debugf(NAME_DevNet, TEXT("Added %s to package map"), *PackageName);
			GetLevel()->NetDriver->MasterMap->AddLinker(Linker);
			GetLevel()->NetDriver->MasterMap->Compute();
		}
		EndLoad();
	}

	unguardexec;
}

void AActor::execShouldBeHidden(FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execShouldBeHidden);

	P_FINISH;

	*(UBOOL*)Result = false;

	if( GetClass()->ClassFlags & CLASS_HideDropDown )
		*(UBOOL*)Result = true;
	unguardexec;
}
void AActor::execDebugUnclock( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execDebugUnclock);
	P_FINISH;
	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_ScriptDebugTime));
	unguard;
}

// 
// Generalize animation retrieval to work for both skeletal meshes (animation sits in Actor->SkelAnim->AnimSeqs) and
// classic meshes (Mesh->AnimSeqs) For backwards compatibility....
//


/*-----------------------------------------------------------------------------
	Tim's physics modes.
-----------------------------------------------------------------------------*/

/* unused - vogel
static FLOAT Splerp( FLOAT F )
{
	FLOAT S = Square(F);
	return (1.0f/16.0f)*S*S - (1.0f/2.0f)*S + 1;
}
*/
//
// Moving brush.
//
void AMover::physMovingBrush( FLOAT DeltaTime )
{
	guard(AMover::physMovingBrush);

	FVector OldLocation = Location;
    FRotator NewRot; // gam

	FLOAT TotalTime = DeltaTime;

	if( bInterpolating )
	{
		// If we are moving, turn off all attached anti-portals
		for (INT AP=0;AP<AntiPortals.Num();AP++)
		{
			if(AntiPortals(AP)->DrawType != DT_None)
				AntiPortals(AP)->SetDrawType(DT_None);
		}
	}

	while( bInterpolating && DeltaTime>0.0f )
	{
		// We are moving.
		FLOAT NewAlpha = PhysAlpha + DeltaTime * PhysRate;

		if( NewAlpha > 1.0f )
		{
			DeltaTime *= (NewAlpha - 1.0f) / (NewAlpha - PhysAlpha);
			NewAlpha   = 1.0f;
		}
		else DeltaTime = 0.0f;

		// Compute alpha.
		FLOAT RenderAlpha;
		if( MoverGlideType == MV_GlideByTime )
		{
			// Make alpha time-smooth and time-continuous.
			// f(0)=0, f(1)=1, f'(0)=f'(1)=0.
			RenderAlpha = 3.0f*NewAlpha*NewAlpha - 2.0f*NewAlpha*NewAlpha*NewAlpha;
		}
		else RenderAlpha = NewAlpha;

        if( !bUseShortestRotation )
            NewRot = OldRot + ((BaseRot + KeyRot[KeyNum]) - OldRot) * RenderAlpha;
        else if( NewAlpha == 1.0f )
            NewRot = KeyRot[KeyNum];
        else 
        {
            BaseRot.Pitch = BaseRot.Pitch & 65535;
            BaseRot.Yaw = BaseRot.Yaw & 65535;
            BaseRot.Roll = BaseRot.Roll & 65535;

            FRotator Delta = ((BaseRot + KeyRot[KeyNum]) - OldRot);

            if( Delta.Pitch > 32768 )
                Delta.Pitch -= 65535;
			if ( Abs(Delta.Pitch) < 10 )
				Delta.Pitch = 0;

            if( Delta.Yaw > 32768 )
                Delta.Yaw -= 65535;
			if ( Abs(Delta.Yaw) < 10 )
				Delta.Yaw = 0;

            if( Delta.Roll > 32768 )
                Delta.Roll -= 65535;
			if ( Abs(Delta.Roll) < 10 )
				Delta.Roll = 0;

            NewRot = OldRot + Delta * RenderAlpha;
        }

		// Move.
		FCheckResult Hit(1.0f);
		FVector MoveVect = FVector(0.f,0.f,0.f);
		MoveVect = OldPos + ((BasePos + KeyPos[KeyNum]) - OldPos) * RenderAlpha - Location;
		AMover *MoverBase = Cast<AMover>(Base);
		if ( MoverBase )
			MoveVect = FVector(0.f,0.f,0.f); // FIXME MoveVect += MoverBase->Location - MoverBase->BasePos;

		if( GetLevel()->MoveActor
		(
			this,
			MoveVect,
			NewRot, // gam
			Hit
		) )
		{
			// Successfully moved.
			PhysAlpha = NewAlpha;
			if( NewAlpha == 1.0f )
			{
				// Just finished moving.
				bInterpolating = 0;
				Velocity.Z = ::Max(Velocity.Z, PhysRate*(BasePos.Z + KeyPos[KeyNum].Z - OldPos.Z));
				eventKeyFrameReached();

				if ( KeyNum == 0 )
				{
					// If we have stopped at the intial frame, flag it
					for (INT AP=0;AP<AntiPortals.Num();AP++)
					{
						if(AntiPortals(AP)->DrawType != DT_AntiPortal)
							AntiPortals(AP)->SetDrawType(DT_AntiPortal);
					}
				}
			}
		}
	}
	Velocity = (Location - OldLocation)/TotalTime;
	unguard;
}

//
// Initialize execution.
//
void AActor::InitExecution()
{
	guard(AActor::InitExecution);

	UObject::InitExecution();

	checkSlow(GetStateFrame());
	checkSlow(GetStateFrame()->Object==this);
	checkSlow(GetLevel()!=NULL);
	checkSlow(GetLevel()->Actors(0)!=NULL);
	checkSlow(GetLevel()->Actors(0)==Level);
	checkSlow(Level!=NULL);

	unguardobj;
}

/*-----------------------------------------------------------------------------
	Natives.
-----------------------------------------------------------------------------*/

//////////////////////
// Console Commands //
//////////////////////

void AWeapon::execInitWeaponFires( FFrame& Stack, RESULT_DECL )
{
	guard(AWeapon::execInitWeaponFires);

	P_FINISH;

	for ( INT i=0; i<UCONST_NUM_FIRE_MODES; i++ )
	{
		if ( FireMode[i] )
			FireMode[i]->InitExecution();
	}
	unguard;
}

void AActor::execConsoleCommand( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execConsoleCommand);

	P_GET_STR(Command);
	P_GET_UBOOL_OPTX(bWriteToLog,0);
	P_FINISH;

	if ( Command.Len() > 1000 )
		Command = Command.Left(1000);

	FStringOutputDevice StrOut;

	if ( bWriteToLog )
		GetLevel()->Engine->Exec( *Command, *GLog );
	else GetLevel()->Engine->Exec( *Command, StrOut );

	*(FString*)Result = *StrOut;

	unguard;
}

//export in-game object's properties to the clipboard
void AActor::execCopyObjectToClipboard( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execCopyObjectToClipboard);

	P_GET_OBJECT(UObject, Obj)
	P_FINISH;

	if (!Obj)
		return;

	FStringOutputDevice Ar;
	AActor *Actor = Cast<AActor>(Obj);
	if (Actor)
	{
		//hack - temporarily blank properties we don't want exported (prevents editor crash on exit when this is pasted in)
		ALevelInfo *OldLevel = Actor->Level;
		APhysicsVolume *OldPV = Actor->PhysicsVolume;
		FPointRegion OldRegion = Actor->Region;
		UBOOL OldScriptInitialized = Actor->bScriptInitialized;
		Actor->Level = NULL;
		Actor->PhysicsVolume = NULL;
		Actor->Region.iLeaf = 0;
		Actor->Region.Zone = NULL;
		Actor->Region.ZoneNumber = 0;
		Actor->bScriptInitialized = false;
		Ar.Logf(TEXT("Begin Map\r\n"));
		Ar.Logf(TEXT("Begin Actor Class=%s Name=%s\r\n"), Obj->GetClass()->GetName(), Obj->GetName());
		ExportProperties(Ar, Obj->GetClass(), (BYTE*)Obj, 3, Obj->GetClass(), &Obj->GetClass()->Defaults(0));
		Ar.Logf(TEXT("End Actor\r\n"));
		Ar.Logf(TEXT("Begin Surface\r\n"));
		Ar.Logf(TEXT("End Surface\r\n"));
		Ar.Logf(TEXT("End Map\r\n"));
		Actor->Level = OldLevel;
		Actor->PhysicsVolume = OldPV;
		Actor->Region = OldRegion;
		Actor->bScriptInitialized = OldScriptInitialized;
	}
	else
	{
		Ar.Logf(TEXT("Begin Object Class=%s Name=%s\r\n"), Obj->GetClass()->GetName(), Obj->GetName());
		ExportProperties(Ar, Obj->GetClass(), (BYTE*)Obj, 3, Obj->GetClass(), &Obj->GetClass()->Defaults(0));
		Ar.Logf(TEXT("End Object\r\n"));
	}

	appClipboardCopy(*Ar);

	unguard;
}

void AActor::execTextToSpeech( FFrame& Stack, RESULT_DECL )
{
	guard(UObject::execTextToSpeech);

	P_GET_STR(Text);
	P_GET_FLOAT(Volume);
	P_FINISH;

	this->TextToSpeech(Text, Volume);

	unguard;
}


/////////////////////////////
// Log and error functions //
/////////////////////////////

void AActor::execError( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execError);

	P_GET_STR(S);
	P_FINISH;

	Stack.Log( *S );
	GetLevel()->DestroyActor( this );

	unguardexecSlow;
}

//////////////////////////
// Clientside functions //
//////////////////////////

void APlayerController::execClientTravel( FFrame& Stack, RESULT_DECL )
{
	guardSlow(APlayerController::execClientTravel);

	P_GET_STR(URL);
	P_GET_BYTE(TravelType);
	P_GET_UBOOL(bItems);
	P_FINISH;

	if( Player )
	{
		// Warn the client.
		eventPreClientTravel();

		// Do the travel.
		GetLevel()->Engine->SetClientTravel( Player, *URL, bItems, (ETravelType)TravelType );
	}

	unguardexecSlow;
}

void APlayerController::execGetPlayerNetworkAddress( FFrame& Stack, RESULT_DECL )
{
	guard(APlayerController::execGetPlayerNetworkAddress);
	P_FINISH;

	if( Player && Player->IsA(UNetConnection::StaticClass()) )
		*(FString*)Result = Cast<UNetConnection>(Player)->LowLevelGetRemoteAddress();
	else
		*(FString*)Result = TEXT("");
	unguard;
}

void APlayerController::execGetServerNetworkAddress( FFrame& Stack, RESULT_DECL )
{
	guard(APlayerController::execGetPlayerNetworkAddress);
	P_FINISH;

	if( GetLevel() && GetLevel()->NetDriver && GetLevel()->NetDriver->ServerConnection )
		*(FString*)Result = GetLevel()->NetDriver->ServerConnection->LowLevelGetRemoteAddress();
	else
		*(FString*)Result = TEXT("");
	unguard;
}

void APlayerController::execCopyToClipboard( FFrame& Stack, RESULT_DECL )
{
	guard(APlayerController::execCopyToClipboard);
	P_GET_STR(Text);
	P_FINISH;
	appClipboardCopy(*Text);
	unguard;
}

void APlayerController::execPasteFromClipboard( FFrame& Stack, RESULT_DECL )
{
	guard(APlayerController::execCopyToClipboard);
	P_FINISH;
	*(FString*)Result = appClipboardPaste();
	unguard;
}

void APlayerController::execGetURLProtocol( FFrame& Stack, RESULT_DECL )
{
	guard(APlayerController::execGetURLProtocol);
	P_FINISH;
	*(FString*)Result = FURL::DefaultProtocol;
	unguard;
}

void ALevelInfo::execDetailChange( FFrame& Stack, RESULT_DECL )
{
	guard(ALevelInfo::execDetailChange);

	P_GET_BYTE(NewDetailMode);
	P_FINISH;

	XLevel->DetailChange((EDetailMode)NewDetailMode);

	unguard;
}

void ALevelInfo::execUpdateDistanceFogLOD( FFrame& Stack, RESULT_DECL )
{
	guard(ALevelInfo::execUpdateDistanceFogLOD);

	P_GET_FLOAT( DistanceFogLOD);
	P_FINISH;

	UpdateDistanceFogLOD( DistanceFogLOD );

	unguard;
}

void ALevelInfo::execForceLoadTexture( FFrame& Stack, RESULT_DECL )
{
	guard(ALevelInfo::execForceLoadTexture);
	P_GET_OBJECT( UTexture, Texture );
	P_FINISH;

	if( Texture && !Texture->bParametric )
	{
		for( INT i=0; i<Texture->Mips.Num(); i++ )
			Texture->Mips(i).DataArray.Load();
	}
	unguard;
}


void ALevelInfo::execGetLocalURL( FFrame& Stack, RESULT_DECL )
{
	guardSlow(ALevelInfo::execGetLocalURL);

	P_FINISH;

	*(FString*)Result = GetLevel()->URL.String();

	unguardexecSlow;
}

void ALevelInfo::execIsDemoBuild( FFrame& Stack, RESULT_DECL )
{
	guardSlow(ALevelInfo::execIsDemoBuild);
	P_FINISH;

#if DEMOVERSION
	*(UBOOL*)Result = 1;
#else
	*(UBOOL*)Result = 0;
#endif

	unguardexecSlow;
}

void ALevelInfo::execIsSoftwareRendering( FFrame& Stack, RESULT_DECL )
{
	guardSlow(ALevelInfo::execIsSoftwareRendering);
	P_FINISH;

	*(UBOOL*)Result = GIsPixomatic;
	unguardexecSlow;
}

void ALevelInfo::execGetAddressURL( FFrame& Stack, RESULT_DECL )
{
	guardSlow(ALevelInfo::execGetAddressURL);

	P_FINISH;

	*(FString*)Result = FString::Printf( TEXT("%s:%i"), *GetLevel()->URL.Host, GetLevel()->URL.Port );

	unguardexecSlow;
}

void ALevelInfo::execIsEntry( FFrame& Stack, RESULT_DECL )
{
	guard(ALevelInfo::execIsEntry);
	P_FINISH;

	*(UBOOL*)Result = 0;

	if(!XLevel || !XLevel->Engine)
		return;

	UGameEngine* GameEngine = Cast<UGameEngine>(XLevel->Engine);

	if( GameEngine &&  ( GameEngine->GLevel == GameEngine->GEntry ) )
		*(UBOOL*)Result = 1;

	unguardexec;
}

void ALevelInfo::execIsPendingConnection( FFrame& Stack, RESULT_DECL )
{
	guard(ALevelInfo::execIsPendingConnection);

	P_FINISH;

	UGameEngine* GameEngine = Cast<UGameEngine>(XLevel->Engine);
	if ( GameEngine && GameEngine->GPendingLevel )
        *(UBOOL*)Result = 1;
	else *(UBOOL*)Result = 0;

	unguardexec;
}

void ALevelInfo::execGetPhysicsVolume( FFrame& Stack, RESULT_DECL )
{
	guardSlow(APlayerController::execGetPhysicsVolume);

	P_GET_VECTOR(Loc);
	P_FINISH;

	*(APhysicsVolume**)Result = GetPhysicsVolume(Loc, NULL, false);
	unguardexecSlow;
}

///////////////////////////
// Client-side functions //
///////////////////////////

void APlayerController::execClientHearSound( FFrame& Stack, RESULT_DECL )
{
	guard(APlayerController::execClientHearSound);

	P_GET_OBJECT(AActor,Actor);
	P_GET_INT(Id);
	P_GET_OBJECT(USound,Sound);
	P_GET_VECTOR(SoundLocation);
	P_GET_VECTOR(Parameters);
	P_GET_UBOOL(Attenuate);
	P_FINISH;

	FLOAT Volume = 0.001f * Parameters.X * 0.5f; // additionally divide volume by 2 as incoming range is 0..2
	FLOAT Radius = Parameters.Y;
	FLOAT Pitch  = 0.001f * Parameters.Z;
	if(	LocalPlayerController()	&& GetLevel()->Engine->Audio && Sound )
	{
		if( Actor && Actor->bDeleteMe )
			Actor = NULL;

		// First person sound attenuation hack.
		INT Flags = 0;
		if ( !Attenuate && Actor )
		{
			UViewport* Viewport = NULL;
			if( GetLevel()->Engine->Audio )
				Viewport = GetLevel()->Engine->Audio->GetViewport();
			if( Viewport && Actor->IsOwnedBy( Viewport->Actor->GetViewTarget() ) )
				Flags |= SF_No3D;
		}
		GetLevel()->Engine->Audio->PlaySound( Actor, Id, Sound, SoundLocation, Volume, Radius ? Radius : Sound->GetRadius(), Pitch, Flags, 0.f ); // sjs
	}
	unguardexec;
}

////////////////////////////////
// Latent function initiators //
////////////////////////////////

void AActor::execSleep( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSleep);

	P_GET_FLOAT(Seconds);
	P_FINISH;

	GetStateFrame()->LatentAction = EPOLL_Sleep;
	LatentFloat  = Seconds;

	unguardexecSlow;
}

void AActor::execFinishAnim( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execFinishAnim);

	P_GET_INT_OPTX(Channel, 0);
	P_FINISH;

	LatentFloat = Channel;
	StartAnimPoll();
	unguardexecSlow;
}

void AActor::StartAnimPoll()
{
	// Part of ending the zero-channel main animation.
	if( Mesh )
	{				
		Mesh->MeshGetInstance(this);
		if( MeshInstance->IsAnimLooping(LatentFloat) ) 
			MeshInstance->AnimStopLooping(LatentFloat);

		// If animation is (re)playing, wait for it to finish.
		if( IsAnimating(LatentFloat) && !MeshInstance->IsAnimPastLastFrame(LatentFloat) )
			GetStateFrame()->LatentAction = EPOLL_FinishAnim;
	}	
}

void AController::StartAnimPoll()
{
	if ( !Pawn )
		return;

	if( Pawn->Mesh )
	{				
		Pawn->Mesh->MeshGetInstance(this);
		if( Pawn->MeshInstance->IsAnimLooping(LatentFloat) ) 
			Pawn->MeshInstance->AnimStopLooping(LatentFloat);

		// If animation is (re)playing, wait for it to finish.
		if( Pawn->IsAnimating(LatentFloat) && !Pawn->MeshInstance->IsAnimPastLastFrame(LatentFloat) )
			GetStateFrame()->LatentAction = EPOLL_FinishAnim;
	}	
}


UBOOL AActor::CheckAnimFinished(INT Channel)
{
	if ( !Mesh )
		return true;
	Mesh->MeshGetInstance(this);
	return ( !IsAnimating(Channel) || MeshInstance->IsAnimPastLastFrame(Channel) );
}

UBOOL AController::CheckAnimFinished(INT Channel)
{
	if ( !Pawn || !Pawn->Mesh )
		return true;
	Pawn->Mesh->MeshGetInstance(this);
	return ( !Pawn->IsAnimating(Channel) || Pawn->MeshInstance->IsAnimPastLastFrame(Channel) );
}

void AActor::execFinishInterpolation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execFinishInterpolation);

	P_FINISH;

	GetStateFrame()->LatentAction = EPOLL_FinishInterpolation;

	unguardexecSlow;
}

///////////////////////////
// Slow function pollers //
///////////////////////////

void AActor::execPollSleep( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPollSleep);

	FLOAT DeltaSeconds = *(FLOAT*)Result;
	if( (LatentFloat-=DeltaSeconds) < 0.5 * DeltaSeconds )
	{
		// Awaken.
		GetStateFrame()->LatentAction = 0;
	}
	unguardexecSlow;
}
IMPLEMENT_FUNCTION( AActor, EPOLL_Sleep, execPollSleep );

void AActor::execPollFinishAnim( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPollFinishAnim);

	if( CheckAnimFinished(LatentFloat) )
		GetStateFrame()->LatentAction = 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( AActor, EPOLL_FinishAnim, execPollFinishAnim );

void AActor::execPollFinishInterpolation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPollFinishInterpolation);

	if( !bInterpolating )
		GetStateFrame()->LatentAction = 0;

	unguardexecSlow;
}
IMPLEMENT_FUNCTION( AActor, EPOLL_FinishInterpolation, execPollFinishInterpolation );

/////////////////////////
// Animation functions //
/////////////////////////
void AActor::execGetMeshName( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::GetMeshName);
	P_FINISH;


	if(Mesh)
		*(FString*)Result = FString(Mesh->GetName());
	else
		*(FString*)Result = FString(TEXT(""));

	unguard;
}

UBOOL AActor::PlayAnim(INT Channel,FName SequenceName, FLOAT PlayAnimRate, FLOAT TweenTime, INT Loop)
{
	guard(AActor::PlayAnim);

	if( !Mesh )
	{
		if ( !bHidden )
		debugf(TEXT("%s playanim with no mesh!"),GetName());
		return 0;
	}
	Mesh->MeshGetInstance(this);
	return MeshInstance->PlayAnim( Channel, SequenceName, PlayAnimRate, TweenTime, Loop );
	unguard;
}

void AActor::execPlayAnim( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPlayAnim);

	P_GET_NAME(SequenceName);
	P_GET_FLOAT_OPTX(PlayAnimRate,1.f);
	P_GET_FLOAT_OPTX(TweenTime,0.f); // NOTE: universal default was "-1.0f" for a long time... - Erik
	P_GET_INT_OPTX(Channel, 0);
	P_FINISH;

	*(DWORD*)Result = PlayAnim(Channel,SequenceName, PlayAnimRate, TweenTime,0);
	
	unguardexecSlow;
}

void AActor::execPlayLIPSincAnim( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execPlayLIPSincAnim);

	P_GET_NAME(LIPSincAnimName);
	P_GET_FLOAT_OPTX(Volume,1.f);
	P_GET_FLOAT_OPTX(Radius,1.f);
	P_GET_FLOAT_OPTX(Pitch,1.f);
	P_FINISH;

#ifdef WITH_LIPSINC
	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{
		((USkeletalMeshInstance*)Mesh->MeshGetInstance(this))->PlayLIPSincAnim(LIPSincAnimName, Volume, Radius ? Radius : GAudioDefaultRadius, Pitch);
	}
	else
	{
		debugf(TEXT("%s is not a USkeletalMesh: PlayLIPSicAnim"),GetName());
	}
#endif	

	unguardexec;
}

void AActor::execIsPlayingLIPSincAnim( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execIsPlayingLIPSincAnim);

	P_FINISH;

#ifdef WITH_LIPSINC
	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{
		*(DWORD*)Result = ((USkeletalMeshInstance*)Mesh->MeshGetInstance(this))->IsPlayingLIPSincAnim();
	}
	else
	{
		debugf(TEXT("%s is not a USkeletalMesh: IsPlayingLIPSincAnim"),GetName());
	}
#endif

	unguardexec;
}

void AActor::execCurrentLIPSincAnim( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execCurrentLIPSincAnim);

	P_FINISH;

#ifdef WITH_LIPSINC
	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{
		*(FString*)Result = ((USkeletalMeshInstance*)Mesh->MeshGetInstance(this))->CurrentLIPSincAnim();
	}
	else
	{
		debugf(TEXT("%s is not a USkeletalMesh: CurrentLIPSincAnim"),GetName());
	}
#endif

	unguardexec;
}

void AActor::execHasLIPSincAnim( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execHasLIPSincAnim);

	P_GET_NAME(LIPSincAnimName);
	P_FINISH;

#ifdef WITH_LIPSINC
	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{
		*(DWORD*)Result = ((USkeletalMeshInstance*)Mesh->MeshGetInstance(this))->HasLIPSincAnim( LIPSincAnimName );
	}
	else
	{
		debugf(TEXT("%s is not a USkeletalMesh: HasLIPSincAnim"),GetName());
	}
#endif

	unguardexec;
}

void AActor::execStopLIPSincAnim( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execStopLIPSincAnim);

	P_FINISH;

#ifdef WITH_LIPSINC
	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{
		*(DWORD*)Result = ((USkeletalMeshInstance*)Mesh->MeshGetInstance(this))->StopLIPSincAnim();
	}
	else
	{
		debugf(TEXT("%s is not a USkeletalMesh: StopLIPSincAnim"),GetName());
	}
#endif

	unguardexec;
}

void AActor::execLoopAnim( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execLoopAnim);

	P_GET_NAME(SequenceName);
	P_GET_FLOAT_OPTX(PlayAnimRate,1.f);
	P_GET_FLOAT_OPTX(TweenTime,0.f);
	P_GET_INT_OPTX(Channel, 0);
	P_FINISH;

	*(DWORD*)Result = PlayAnim(Channel,SequenceName,PlayAnimRate,TweenTime,1);

	unguardexecSlow;
}

void AActor::execTweenAnim( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execTweenAnim);

	P_GET_NAME(SequenceName);
	P_GET_FLOAT(TweenTime);
	P_GET_INT_OPTX(Channel, 0);
	P_FINISH;

	*(DWORD*)Result = PlayAnim(Channel,SequenceName,0.f,TweenTime,0);

	unguardexecSlow;
}

void AActor::execIsAnimating( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execIsAnimating);

	P_GET_INT_OPTX(Channel, 0);
	P_FINISH;

	*(DWORD*)Result = IsAnimating(Channel);

	unguardexecSlow;
}

void AActor::execStopAnimating( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execStopAnimating);

	P_GET_UBOOL_OPTX(ClearAllButBase, 0);
	P_FINISH;

	// make all channels freeze in current pose
	if( Mesh )
	{
		Mesh->MeshGetInstance(this);
		
		USkeletalMeshInstance* SkelInstance = Cast<USkeletalMeshInstance>(MeshInstance);
		if(SkelInstance)
			SkelInstance->StopAnimating(ClearAllButBase);
		else
			MeshInstance->StopAnimating();
	}

	unguardexecSlow;
}

// jjs -
void AActor::execAnimStopLooping( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execAnimStopLooping);

	P_GET_INT_OPTX(Channel, 0);
	P_FINISH;

	if( Mesh )
	{
		Mesh->MeshGetInstance(this);
		MeshInstance->AnimStopLooping(Channel);
	}

	unguardexecSlow;
}
// - jjs

// GDC-hackish..
void AActor::execFreezeAnimAt( FFrame& Stack, RESULT_DECL )
{
	guardSlow( AActor::execFreezeAtAnimFrame);
	P_GET_FLOAT( Time );
	P_GET_INT_OPTX( Channel, 0 )
	P_FINISH;

	if( Mesh) 
	{
		Mesh->MeshGetInstance(this);
		MeshInstance->FreezeAnimAt( Time, Channel );
	}

	unguardexecSlow;
}

void AActor::execSetAnimFrame( FFrame& Stack, RESULT_DECL )
{
	guardSlow( AActor::execSetAnimFrame);
	P_GET_FLOAT( Frame );
	P_GET_INT_OPTX( Channel, 0 )
	P_GET_INT_OPTX( UnitFlag, 0 )
	P_FINISH;

	if( Mesh) 
	{
		Mesh->MeshGetInstance(this);
		MeshInstance->SetAnimFrame( Channel, Frame, UnitFlag );
	}

	unguardexecSlow;
}

// return true if than channel is animating, and animframe < 0
void AActor::execIsTweening( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execIsTweening);

	P_GET_INT(Channel);
	P_FINISH;

	*(DWORD*)Result = 0;

	if( Mesh )
	{
	Mesh->MeshGetInstance(this);
	*(DWORD*)Result = ( MeshInstance->IsAnimTweening( Channel ) ) ? 1 : 0;
	}
	
	unguardexecSlow;
}

void AActor::execHasAnim( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execHasAnim);

	P_GET_NAME(SequenceName);
	P_FINISH;

	// Check for a certain anim sequence.
	if( Mesh )
	{
		Mesh->MeshGetInstance(this);
		HMeshAnim HSeq = MeshInstance->GetAnimNamed( SequenceName );
		if( HSeq )
		{
			*(DWORD*)Result = 1;
		} else
			*(DWORD*)Result = 0;
	} else Stack.Logf( TEXT("HasAnim: No mesh") );
	unguardexecSlow;
}

void AActor::execLinkSkelAnim( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execLinkSkelAnim);
	P_GET_OBJECT(UMeshAnimation, NewAnim);
	P_GET_OBJECT_OPTX(USkeletalMesh, NewMesh, NULL);
	P_FINISH;	

	// Set main skel anim for this instance.
	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{
		if(! ((USkeletalMeshInstance*)Mesh->MeshGetInstance(this))->SetSkelAnim( NewAnim, NewMesh ) ) 
			debugf(TEXT("Trying to assign invalid animation object or into a bad channel for MeshInstance [%s]"), Mesh->MeshGetInstance(this)->GetName());
	}
	unguardexecSlow;
}

#if defined(WITH_KARMA) || defined(WITH_PHYSICS)
static UBOOL MeshIsCompatible(UMesh* Mesh, UMesh* newMesh)
{
	guard(MeshIsCompatible);

	// Check we have two skeletal meshes - otherwise deemed 'incompatible'
	if(!newMesh || !Mesh)
		return 0;

	USkeletalMesh* skelMesh = Cast<USkeletalMesh>(Mesh);
	USkeletalMesh* newSkelMesh = Cast<USkeletalMesh>(newMesh);
	if(!skelMesh || !newSkelMesh)
		return 0;

	// Must have same number of bones
	if( newSkelMesh->RefSkeleton.Num() != skelMesh->RefSkeleton.Num() )
		return 0;

	// Check heirarchy
	for(INT i=0; i<skelMesh->RefSkeleton.Num(); i++)
	{
		if( skelMesh->RefSkeleton(i).ParentIndex != newSkelMesh->RefSkeleton(i).ParentIndex )
			return 0;
	}

	return 1;

	unguard;
}
#endif

void AActor::execLinkMesh( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execLinkMesh);	
	P_GET_OBJECT(UMesh, NewMesh);
	P_GET_UBOOL_OPTX( bKeepAnim, 0);
	P_FINISH;	

	if ( Mesh != NewMesh )
	{
		if( NewMesh && NewMesh->IsA(USkeletalMesh::StaticClass()) )
		{
			// If we are currently in ragdoll, check the mesh has the same heirarchy before replacing it.
			if(Mesh)
			{
#if defined(WITH_KARMA) || defined(WITH_PHYSICS)
				USkeletalMeshInstance* inst = (USkeletalMeshInstance*)Mesh->MeshGetInstance(this);

				// If we are currently running ragdoll, skeletons have to exactly match
				if(Physics == PHYS_KarmaRagDoll && inst && inst->KSkelIsInitialised)
				{
					if( !MeshIsCompatible( Mesh, NewMesh ) )
						return;
				}
#endif

				//#Skel - Other subclasses need specific setmesh to update meshinstances properly.
				if( bKeepAnim )
					((USkeletalMeshInstance*)Mesh->MeshGetInstance(this))->SetMesh( NewMesh ); 
			}
		}
		Mesh = NewMesh;
		bNetDirty = true;

		// FIXME temp hack - make this a script event
		APawn *P = Cast<APawn>(this);
		if ( P && (P->HeadScale != 1.f) )
			P->eventSetHeadScale(P->HeadScale);
	}
	unguardexecSlow;
}

void AActor::execBoneRefresh( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execBoneRefresh);	
	P_FINISH;		
	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{
		((USkeletalMeshInstance*)Mesh->MeshGetInstance(this))->ForceBoneRefresh();
	}		
	else
	{
		debugf(TEXT("Actor %s has no valid skeletal mesh yet !!!"),this->GetName()); //#SKEL
	}
	unguardexecSlow;
}

void AActor::execGetBoneCoords( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execGetBoneCoords);
	P_GET_NAME(BoneName);
	P_FINISH;

    // gam ---
    if( IsPendingKill() )
    {
		*(FCoords*)Result = GMath.UnitCoords;
        return;
    }
    // --- gam

	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{				
		Mesh->MeshGetInstance(this);
		INT BoneIdx = ((USkeletalMeshInstance*)MeshInstance)->MatchRefBone( BoneName );
		if( BoneIdx >= 0)
		{
			*(FCoords*)Result = ((USkeletalMeshInstance*)MeshInstance)->GetBoneCoords(BoneIdx);
		}
		else
		{
			*(FCoords*)Result = ((USkeletalMeshInstance*)MeshInstance)->GetBoneCoords(0);
			//debugf(TEXT("Warning: Invalid bone index (%i) for GetBoneRotation."),BoneIdx);
			//*(FCoords*)Result = GMath.UnitCoords;
		}
	}	
    // sjs ---
    else
	{
		if( Mesh )
		{
			FMatrix attachMat;
			if ( Mesh->MeshGetInstance(this)->GetAttachMatrix(BoneName, attachMat) )
			{
				*(FCoords*)Result = attachMat.Coords();
			}
			else
			{
				*(FCoords*)Result = GMath.UnitCoords;
			}
		}
	}
    // --- sjs
	unguardexecSlow;
}

void AActor::execGetClosestBone( FFrame& Stack, RESULT_DECL )
{
    guardSlow(AActor::execGetBoneCoords);
    P_GET_VECTOR(loc);
    P_GET_VECTOR(ray);
    P_GET_FLOAT_REF(outboneDist);
    P_GET_NAME_OPTX(biasBone,NAME_None);
    P_GET_FLOAT_OPTX(biasDist,0.0f);
	P_FINISH;

    *(FName*)Result = NAME_None;

    UBOOL RayTest = 0;
    biasDist = Square(biasDist);
    FPlane RayPlane;
    if( ray.SizeSquared() > 0.0 )
    {
        RayPlane = FPlane(loc,ray);
        RayTest = 1;
    }

    if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{
        USkeletalMeshInstance* pInst = (USkeletalMeshInstance*)Mesh->MeshGetInstance(this);
        USkeletalMesh* pSkel = (USkeletalMesh*)Mesh;
        // Evaluate skeleton if not already present / updated #debug - use instance's last game GTicks
	    if(!pInst->SpaceBases.Num() || (pInst->LastGTicks < GTicks))
	    {
		    AActor* Owner = pInst->GetActor();		
		    if( Owner )
		    {
			    INT DummyVerts;
			    pInst->GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_BonesOnly); 
		    }
	    }
        FName bestName = NAME_None;
        float bestDist = 999999.9f;
        FMatrix MeshToWorldMatrix = pInst->MeshToWorld();

        for( INT t=0; t<pSkel->TagAliases.Num(); t++)
        {
            INT BoneIdx = pInst->MatchRefBone( pSkel->TagAliases(t) );
            if( pInst->SpaceBases.Num() && BoneIdx != INDEX_NONE && BoneIdx < pInst->SpaceBases.Num() )
	        {
	            bool excludeBone = false;
	            // exclude zero scale bones from this test and bones whose parent is zero scale
	            for( int i=BoneIdx; i!=0; i=pSkel->RefSkeleton(i).ParentIndex )
                {
                    for(INT s=0; s<pInst->Scalers.Num(); s++ )
		            {
			            if( pInst->Scalers(s).Bone == i && pInst->Scalers(s).ScaleUniform == 0.0f )
			            {
                            excludeBone = true;
                            break;
			            }
		            }   
		            if( excludeBone )
		                break;
                }
                if( excludeBone )
                    continue;

		        FVector Origin = MeshToWorldMatrix.TransformFVector( pInst->SpaceBases(BoneIdx).Origin );

                float dist = 0.0f;
                if (RayTest)
                {
                    // calc distance to line along ray
                    float planeDist = RayPlane.PlaneDot(Origin);
                    if( planeDist <= 0.0f )
                    {
                        dist = (Origin - loc).SizeSquared();
                    }
                    else
                    {
                        FVector t = loc + RayPlane * planeDist;
                        dist = (Origin - t).SizeSquared();
                    }
                }
                else
                {
		            dist = (Origin - loc).SizeSquared();
                }
                //debugf(TEXT("Bone: %s dist: %f"), *pSkel->TagAliases(t), appSqrt(dist));
                if( dist < bestDist )
                {
                    bestName = pSkel->TagAliases(t);
                    bestDist = dist;
                }
                // see if we have come near to bias bone
                if( biasBone != NAME_None && biasBone == pSkel->TagAliases(t) && dist <= biasDist )
                {
                    //debugf(TEXT("Using Bias Bone: %s dist: %f"), *pSkel->TagAliases(t), appSqrt(dist));
                    bestName = biasBone;
                    bestDist = dist;
                    *(FName*)Result = bestName;
                    return;
                }
	        }
        }
        *(FName*)Result = bestName;
    }
    else if( Mesh )
	{
        int numAttach = Mesh->MeshGetInstance(this)->GetAttachCount();
        if ( numAttach )
        {
            FName name;
            FMatrix matrix;
            FName bestName = NAME_None;
            float bestDist = 99999.9f;
            for( int i=0; i<numAttach; i++ )
            {
                if( !Mesh->MeshGetInstance(this)->GetAttachIdx(i, name, matrix ) )
                    continue;

                FVector offset( matrix.M[3][0], matrix.M[3][1], matrix.M[3][2] );
                float dist = (offset - loc).Size();
                if( dist < bestDist )
                {
                    bestDist = dist;
                    bestName = name;
                    *outboneDist = dist;
                }
            }
            *(FName*)Result = bestName;
        }
    }    
    unguardexecSlow;
}
// --- sjs

void AActor::execGetBoneRotation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execGetBoneRotation);
	P_GET_NAME(BoneName);
	P_GET_INT_OPTX( Space, 0 ); // 0 local, 1 global, 2 relative reference pose (?) 
	P_FINISH;
	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{				
		Mesh->MeshGetInstance(this);
		INT BoneIdx = ((USkeletalMeshInstance*)MeshInstance)->MatchRefBone( BoneName );
		if( BoneIdx >= 0)
		{
			*(FRotator*)Result = ((USkeletalMeshInstance*)MeshInstance)->GetBoneRotation(BoneIdx,Space);
		}
		else
		{
			debugf(TEXT("Warning: Invalid bone index (%i) for GetBoneRotation."),BoneIdx);
			*(FRotator*)Result = FRotator(0,0,0);
		}
	}	
	unguardexecSlow;
}


void AActor::execGetRootLocation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execGetRootLocation);
	P_FINISH;
	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{				
		Mesh->MeshGetInstance(this);
		*(FVector*)Result = ((USkeletalMeshInstance*)MeshInstance)->GetRootLocation();
	}
	unguardexecSlow;
}

void AActor::execGetRootRotation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execGetRootRotation);
	P_FINISH;
	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{				
		Mesh->MeshGetInstance(this);
		*(FRotator*)Result = ((USkeletalMeshInstance*)MeshInstance)->GetRootRotation();
	}
	unguardexecSlow;
}

void AActor::execGetRootLocationDelta( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execGetRootLocationDelta);
	P_FINISH;
	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{				
		Mesh->MeshGetInstance(this);
		*(FVector*)Result = ((USkeletalMeshInstance*)MeshInstance)->GetRootLocationDelta();
	}
	unguardexecSlow;
}

void AActor::execGetRootRotationDelta( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execGetRootRotationDelta);
	P_FINISH;
	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{				
		Mesh->MeshGetInstance(this);
		*(FRotator*)Result = ((USkeletalMeshInstance*)MeshInstance)->GetRootRotationDelta();
	}
	unguardexecSlow;
}


void AActor::execLockRootMotion( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execLockRootRotation);
	P_GET_INT_OPTX(Lock,1);	
	P_FINISH;

	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{				
		Mesh->MeshGetInstance(this);
		((USkeletalMeshInstance*)MeshInstance)->LockRootMotion(Lock);
	}
	unguardexecSlow;
}

void AActor::execAnimBlendParams( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execAnimBlendParams);
	P_GET_INT(Channel);
	P_GET_FLOAT_OPTX(BlendAlpha,0.0f); 
	P_GET_FLOAT_OPTX(InRate,0.0f);
	P_GET_FLOAT_OPTX(OutRate,0.0f);
	P_GET_NAME_OPTX(StartBoneName, NAME_None);	
	P_GET_UBOOL_OPTX(bGlobalPose, 0);
	P_FINISH;

	if( Mesh )//&& Mesh->IsA(USkeletalMesh::StaticClass()) ) // jjs - meshes of all races and colors deserve this privlage
	{				
		Mesh->MeshGetInstance(this);
		// Assign the blend parameters to the current AnimStage channel.
		MeshInstance->SetBlendParams( Channel, BlendAlpha, InRate, OutRate, StartBoneName, bGlobalPose ); // jjs
	}
	unguardexecSlow;
}

void AActor::execAnimBlendToAlpha( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execAnimBlendToAlpha);
	P_GET_INT(Channel);
	P_GET_FLOAT(TargetAlpha); 
	P_GET_FLOAT(TimeInterval);	
	P_FINISH;

	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{				
		Mesh->MeshGetInstance(this);
		// Assign the blend parameters to the current AnimStage channel.
		((USkeletalMeshInstance*)MeshInstance)->BlendToAlpha( Channel, TargetAlpha, TimeInterval);		
	}
	unguardexecSlow;
}

void AActor::execGetNotifyChannel( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execGetNotifyChannel);
	P_FINISH;

	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{				
		*(DWORD*)Result = (DWORD)((USkeletalMeshInstance*)Mesh->MeshGetInstance(this))->LastNotifyStage;
	}
	else
	*(DWORD*)Result = 0; //Default stage.

	unguardexecSlow;	
}

void AActor::execEnableChannelNotify( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execEnableChannelNotify);
	P_GET_INT(Channel);	
	P_GET_INT_OPTX(Switch,1);	
	P_FINISH;

	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass()) )
	{				
		((USkeletalMeshInstance*)Mesh->MeshGetInstance(this))->EnableChannelNotify(Channel,Switch);
	}
	unguardexecSlow;	
}

void AActor::execSetBoneScale( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetBoneScale);
	P_GET_INT(Slot);
	P_GET_FLOAT_OPTX(BoneScale, 1.0f);
	P_GET_NAME_OPTX(BoneName,NAME_None);	
	P_FINISH;
	// Set or clear bone scaler.
	if( Mesh ) 
	{
		Mesh->MeshGetInstance(this)->SetBoneScale( Slot, BoneScale, BoneName);
	}
	unguardexecSlow;
}

void AActor::execSetBoneLocation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetBoneLocation);
	//P_GET_INT(Slot);
	P_GET_NAME( BoneName );	
	P_GET_VECTOR_OPTX( BoneTrans, FVector(0.0f,0.0f,0.0f) );
	P_GET_FLOAT_OPTX( Alpha, 1.0f ); 
	P_FINISH;

	// Set or clear bone locator.
	if( Mesh ) 
	{
		Mesh->MeshGetInstance(this)->SetBoneLocation( BoneName, BoneTrans, Alpha );
	}
	unguardexecSlow;
}

void AActor::execSetBoneRotation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetBoneRotation);
	P_GET_NAME( BoneName );	
	P_GET_ROTATOR_OPTX( BoneTurn, FRotator(0,0,0) );
	P_GET_INT_OPTX( Space, 0 );   // 0 local, 1 global
	P_GET_FLOAT_OPTX( Alpha, 1.0f ); 
	P_FINISH;

	// Set or clear bone rotator.
	if( Mesh ) 
	{
		Mesh->MeshGetInstance(this)->SetBoneRotation( BoneName, BoneTurn, Space, Alpha );
	}
	unguardexecSlow;	
}


// native final function bool SetBoneDirection( name BoneName, rotator BoneTurn,  optional vector BoneTrans, optional float Alpha );

// 'Bend' bone direction (axis-angle style?) in world coordinates...
void AActor::execSetBoneDirection( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetBoneDirection);
	//P_GET_INT(Slot);
	P_GET_NAME( BoneName );	
	P_GET_ROTATOR( BoneTurn );
	P_GET_VECTOR_OPTX( BoneTrans, FVector(0.0f,0.0f,0.0f) );
	P_GET_FLOAT_OPTX( Alpha, 1.0f ); 
	P_GET_INT_OPTX( Space, 0 );   // 0 local, 1 global
	P_FINISH;

	// Set or clear bone direction controller.
	if( Mesh && Mesh->IsA(USkeletalMesh::StaticClass())) 
	{
		((USkeletalMeshInstance*)Mesh->MeshGetInstance(this))->SetBoneDirection( BoneName, BoneTurn, BoneTrans, Alpha, Space);
	}
	unguardexecSlow;	
}

// native final function GetAnimParams( int Channel, out name Sequence, out float AnimFrame, out float AnimRate );

void AActor::execGetAnimParams( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execGetAnimParams);
	P_GET_INT(Channel);
	P_GET_NAME_REF( OutSeqName );
	P_GET_FLOAT_REF( OutAnimFrame );
	P_GET_FLOAT_REF( OutAnimRate );
	P_FINISH;	

	*OutSeqName = NAME_None;
	*OutAnimFrame = 0.0f;
	*OutAnimRate = 0.0f;

	if( Mesh )		
	{	
		Mesh->MeshGetInstance(this);

		*OutSeqName = MeshInstance->GetActiveAnimSequence( Channel );
		*OutAnimFrame = MeshInstance->GetActiveAnimFrame( Channel );
		*OutAnimRate = MeshInstance->GetActiveAnimRate( Channel );
	}


	unguardexecSlow;	
}


// native final function bool AnimIsInGroup( int Channel, name GroupName ); 
// Determine if currently playing animation of channel (default 0) is member of given group 

void AActor::execAnimIsInGroup( FFrame& Stack, RESULT_DECL )
{
	guardSlow( AActor::execAnimIsInGroup );
	P_GET_INT( Channel );
	P_GET_NAME( GroupName )
	P_FINISH;	

	*(DWORD*)Result = 0;
	if( Mesh )
	{		
		Mesh->MeshGetInstance(this);
	FName ThisAnimSeq = NAME_None;
    
    if ( !MeshInstance ) //sjs
    {
        *(DWORD*)Result = 0;
        return;
    }

	ThisAnimSeq = MeshInstance->GetActiveAnimSequence( Channel );
	if( ThisAnimSeq !=NAME_None )
	{		
		HMeshAnim HSeq = MeshInstance->GetAnimNamed( ThisAnimSeq );
		if( HSeq )
		{
			*(DWORD*)Result = MeshInstance->AnimIsInGroup(HSeq, GroupName)? 1 : 0 ;
		}
	}
	}
	unguardexecSlow;	
}

///////////////
// Collision //
///////////////

void AActor::execSetCollision( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetCollision);

	P_GET_UBOOL_OPTX(NewCollideActors,bCollideActors);
	P_GET_UBOOL_OPTX(NewBlockActors,  bBlockActors  );
	P_GET_UBOOL_OPTX(NewBlockPlayers, bBlockPlayers );
	P_FINISH;

	SetCollision( NewCollideActors, NewBlockActors );

	unguardexecSlow;
}

void AActor::execOnlyAffectPawns( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execOnlyAffectPawns);

	P_GET_UBOOL(bNewAffectPawns);
	P_FINISH;

	if( bCollideActors && (bNewAffectPawns != bOnlyAffectPawns))
	{
		SetCollision(false, bBlockActors);
		bOnlyAffectPawns = bNewAffectPawns;
		SetCollision(true, bBlockActors);
	}

	unguardexecSlow;
}

void AActor::execSetCollisionSize( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetCollisionSize);

	P_GET_FLOAT(NewRadius);
	P_GET_FLOAT(NewHeight);
	P_FINISH;

	SetCollisionSize( NewRadius, NewHeight );

	// Return boolean success or failure.
	*(DWORD*)Result = 1;

	unguardexecSlow;
}

void AActor::execSetBase( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetFloor);

	P_GET_OBJECT(AActor,NewBase);
	P_GET_VECTOR_OPTX(NewFloor, FVector(0,0,1) );
	P_FINISH;

	SetBase( NewBase,NewFloor );

	unguardexecSlow;
}

void AActor::execIsJoinedTo( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execIsJoinedTo);

	P_GET_OBJECT(AActor,Other);
	P_FINISH;

	*(DWORD*)Result	= IsJoinedTo( Other );

	unguardexecSlow;
}

void AActor::execSetDrawScale( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetDrawScale);

	P_GET_FLOAT(NewScale);
	P_FINISH;

	SetDrawScale(NewScale);

	unguardexecSlow;
}

void AActor::execSetStaticMesh( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetStaticMesh);

	P_GET_OBJECT(UStaticMesh,NewStaticMesh);
	P_FINISH;

	SetStaticMesh(NewStaticMesh);

	unguardexecSlow;
}

void AActor::execSetDrawType( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetDrawType);

	P_GET_BYTE(NewDrawType);
	P_FINISH;

	SetDrawType(EDrawType(NewDrawType));

	unguardexecSlow;
}

void AActor::execSetDrawScale3D( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetCollisionSize3D);

	P_GET_VECTOR(NewScale3D);
	P_FINISH;

	SetDrawScale3D(NewScale3D);

	unguardexecSlow;
}

///////////
// Audio //
///////////
void AController::CheckHearSound(AActor* SoundMaker, INT Id, USound* Sound, FVector Parameters, FLOAT Radius, UBOOL Attenuate)
{
	guardSlow(AController::CheckHearSound);

	if ( Pawn && IsProbing(NAME_AIHearSound) && CanHearSound(Pawn->Location, SoundMaker, Radius) )
		eventAIHearSound( SoundMaker, Id, Sound, Pawn->Location, Parameters, Attenuate );

	unguardSlow;
}

// Can hear audio sound being played
UBOOL AController::CanHearSound(FVector HearSource, AActor* SoundMaker, FLOAT Radius)
{
	guardSlow(AController::CanHearSound);

	FVector SoundSourceLocation = SoundMaker->Location;
	if ( SoundMaker->IsA(AWeapon::StaticClass()) && SoundMaker->Instigator )
		SoundSourceLocation = SoundMaker->Instigator->Location;
	if( !GetLevel()->IsAudibleAt( SoundSourceLocation, HearSource, SoundMaker, (ESoundOcclusion) SoundMaker->SoundOcclusion ) )
	{
		FPointRegion Region1 = GetLevel()->Model->PointRegion( Level, HearSource );
		FPointRegion Region2 = GetLevel()->Model->PointRegion( Level, SoundSourceLocation );
		Radius *= GetLevel()->CalculateRadiusMultiplier( Region1.ZoneNumber, Region2.ZoneNumber );
	}

	if(	(HearSource - SoundSourceLocation).SizeSquared() < Square( Radius * GAudioMaxRadiusMultiplier * (Level->NetMode == NM_DedicatedServer ? 0.5f : 1.f)) )
		return true;
	else 
		return false;

	unguardSlow;
}

void APlayerController::CheckHearSound(AActor* SoundMaker, INT Id, USound* Sound, FVector Parameters, FLOAT Radius, UBOOL Attenuate )
{
	guardSlow(APlayerController::CheckHearSound);

	FVector SoundSourceLocation = SoundMaker->Location;
	if ( SoundMaker->IsA(AWeapon::StaticClass()) && SoundMaker->Instigator )
		SoundSourceLocation = SoundMaker->Instigator->Location;
	GetViewTarget();
	if ( bAcuteHearing )
	{
		Radius *= 10.f;
		Parameters.X = ::Max(Parameters.X, 1.5f);
	}
	FVector ParamModifier(1000.f,1.f,1000.f); // volumes are cut in half to allow max volume of 2.0 (hack)
	if ( CanHearSound(ViewTarget->Location, SoundMaker, Radius) || !Attenuate )
		eventClientHearSound( SoundMaker, Id, Sound, SoundSourceLocation, Parameters * ParamModifier, Attenuate );

	unguardSlow;
}

void AActor::execDemoPlaySound( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execDemoPlaySound);

	// Get parameters.
	P_GET_OBJECT(USound,Sound);
	P_GET_BYTE_OPTX(Slot,SLOT_Misc);
	P_GET_FLOAT_OPTX(Volume,TransientSoundVolume);
	P_GET_UBOOL_OPTX(bNoOverride, 0);
	P_GET_FLOAT_OPTX(Radius,TransientSoundRadius);
	P_GET_FLOAT_OPTX(Pitch,1.0f);
	P_GET_UBOOL_OPTX(Attenuate,1);
	P_FINISH;

	if( !Sound )
		return;

    if ( Radius == 0.0f ) // sjs
        Radius = Sound->GetRadius();

	if ( Instigator && ((Instigator == this) || (Instigator == Owner)) )
		Volume *= Instigator->SoundDampening;

	// Play the sound locally
	INT Id = GetIndex()*16 + Slot*2 + bNoOverride;
	FVector Parameters = FVector(Volume, Radius, Pitch);

	UClient* Client = GetLevel()->Engine->Client;
	if( Client )
	{
		for( INT i=0; i<Client->Viewports.Num(); i++ )
		{
			AController* Hearer = Client->Viewports(i)->Actor;
			if( Hearer && Hearer->GetLevel()==GetLevel() )
				Hearer->CheckHearSound(this, Id, Sound, Parameters, Radius ? Radius : Sound->GetRadius(), Attenuate ); // sjs
		}
	}
	unguardexecSlow;
}

void AActor::execAllowMusicPlayback( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execAllowMusicPlayback);
	P_GET_UBOOL(Allow);
	P_FINISH;

	if( GetLevel()->Engine->Audio )
		GetLevel()->Engine->Audio->AllowMusicPlayback( Allow );	

	unguardexecSlow;
}

void AActor::execPlayStream( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPlayStream);
	P_GET_STR(Song);
	P_GET_UBOOL_OPTX(UseMusicVolume,1);
	P_GET_FLOAT_OPTX(Volume,1);
	P_GET_FLOAT_OPTX(FadeInTime,0.f);
	P_GET_FLOAT_OPTX(SeekTime,0);
	P_FINISH;

	if( GetLevel()->Engine->Audio )
		*(INT*)Result = GetLevel()->Engine->Audio->PlayStream( Song, UseMusicVolume, Volume, FadeInTime, SeekTime );
	else
		*(INT*)Result = 0;

	unguardexecSlow;
}

void AActor::execSeekStream( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPlayStream);
	P_GET_INT(SongHandle);
	P_GET_FLOAT(Time);
	P_FINISH;

	if( GetLevel()->Engine->Audio )
		*(INT*)Result = GetLevel()->Engine->Audio->SeekStream( SongHandle, Time );
	else
		*(INT*)Result = 0;

	unguardexecSlow;
}

void AActor::execPlayMusic( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPlayMusic);
	P_GET_STR(Song);
	P_GET_FLOAT_OPTX(FadeInTime,0.f);
	P_FINISH;

	if( ParseParam( appCmdLine(), TEXT("nomusic") ) )
    {
    	*(INT*)Result = 0;
        return;
    }    

	if( GetLevel()->Engine->Audio )
		*(INT*)Result = GetLevel()->Engine->Audio->PlayMusic( Song, FadeInTime );
	else
		*(INT*)Result = 0;

	unguardexecSlow;
}

void AActor::execStopMusic( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execStopMusic);
	P_GET_INT( SongHandle );
	P_GET_FLOAT_OPTX( FadeOutTime, 0.f );
	P_FINISH;	

	if( GetLevel()->Engine->Audio )
		GetLevel()->Engine->Audio->StopStream( SongHandle, FadeOutTime );

	unguardexecSlow;
}

void AActor::execStopStream( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execStopStream);
	P_GET_INT( SongHandle );
	P_GET_FLOAT_OPTX( FadeOutTime,0.f );
	P_FINISH;	

	if( GetLevel()->Engine->Audio )
		GetLevel()->Engine->Audio->StopStream( SongHandle, FadeOutTime );

	unguardexecSlow;
}

void AActor::execStopAllMusic( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execStopAllMusic);
	P_GET_FLOAT_OPTX( FadeOutTime, 0.f );
	P_FINISH;	

	if( GetLevel()->Engine->Audio )
		GetLevel()->Engine->Audio->StopAllMusic( FadeOutTime );

	unguardexecSlow;
}

// rjp --
void AActor::execPauseStream( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPauseStream);

	P_GET_INT(SongHandle);
	P_FINISH;

	if ( GetLevel()->Engine->Audio )
		*(UBOOL*)Result = GetLevel()->Engine->Audio->PauseStream( SongHandle );
	else
		*(UBOOL*)Result = 0;

	unguardexecSlow;
}

void AActor::execAdjustVolume( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execAdjustVolume);
	
	P_GET_INT(Handle);
	P_GET_FLOAT(NewVolume);
	P_FINISH;

	if ( GetLevel()->Engine->Audio )
		*(UBOOL*)Result = GetLevel()->Engine->Audio->SetVolume( Handle, NewVolume );
	else
		*(UBOOL*)Result = 0;

	unguardexecSlow;
}

void AActor::execPlaySound( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPlaySound);

	// Get parameters.
	P_GET_OBJECT(USound,Sound);
	P_GET_BYTE_OPTX(Slot,SLOT_Misc);
	P_GET_FLOAT_OPTX(Volume,TransientSoundVolume);
	P_GET_UBOOL_OPTX(bNoOverride, 0);
	P_GET_FLOAT_OPTX(Radius,TransientSoundRadius);
	P_GET_FLOAT_OPTX(Pitch,1.0f);
	P_GET_UBOOL_OPTX(Attenuate,1);
	P_FINISH;

	if( !Sound )
		return;

    if ( Radius == 0.0f ) // sjs
        Radius = Sound->GetRadius() * 0.15f; //!!vogel: FUDGE HACK 

	if ( Instigator && ((Instigator == this) || (Instigator == Owner)) )
		Volume *= Instigator->SoundDampening;

	// Server-side demo needs a call to execDemoPlaySound for the DemoRecSpectator
	if(		GetLevel() && GetLevel()->DemoRecDriver
		&&	!GetLevel()->DemoRecDriver->ServerConnection
		&&	Level->NetMode != NM_Client )
		eventDemoPlaySound(Sound, Slot, Volume, bNoOverride, Radius, Pitch, Attenuate);

	INT Id = GetIndex()*16 + Slot*2 + bNoOverride;
	FVector Parameters = FVector(Volume, Radius, Pitch);

	// See if the function is simulated.
    UFunction* Caller = FlagCast<UFunction,CLASS_IsAUFunction>( Stack.Node );
	if( (Level->NetMode == NM_Client) || (Caller && (Caller->FunctionFlags & FUNC_Simulated)) )
	{
		// Called from a simulated function, so propagate locally only.
		UClient* Client = GetLevel()->Engine->Client;
		if( Client )
		{
			for( INT i=0; i<Client->Viewports.Num(); i++ )
			{
				AController* Hearer = Client->Viewports(i)->Actor;
				if( Hearer && Hearer->GetLevel()==GetLevel() )
					Hearer->CheckHearSound(this, Id, Sound, Parameters,Radius,Attenuate);
			}
		}
	}
	else
	{
		// Propagate to all player actors.
		for( AController* Hearer=Level->ControllerList; Hearer; Hearer=Hearer->nextController )
		{
			if( Hearer->bIsPlayer )
				Hearer->CheckHearSound(this, Id, Sound, Parameters, Radius ? Radius : Sound->GetRadius(), Attenuate ); // sjs
		}
	}
	unguardexecSlow;
}

void AActor::execPlayOwnedSound( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPlayOwnedSound);

	// Get parameters.
	P_GET_OBJECT(USound,Sound);
	P_GET_BYTE_OPTX(Slot,SLOT_Misc);
	P_GET_FLOAT_OPTX(Volume,TransientSoundVolume);
	P_GET_UBOOL_OPTX(bNoOverride, 0);
	P_GET_FLOAT_OPTX(Radius,TransientSoundRadius);
	P_GET_FLOAT_OPTX(Pitch,1.0f);
	P_GET_UBOOL_OPTX(Attenuate,0);
	P_FINISH;

	if( !Sound )
		return;

    if ( Radius == 0.0f ) // sjs
        Radius = Sound->GetRadius() * 0.15f; //!!vogel: FUDGE HACK

	if ( Instigator && ((Instigator == this) || (Instigator == Owner)) )
		Volume *= Instigator->SoundDampening;

	// if we're recording a demo, make a call to execDemoPlaySound()
	if( (GetLevel() && GetLevel()->DemoRecDriver && !GetLevel()->DemoRecDriver->ServerConnection) )
		eventDemoPlaySound(Sound, Slot, Volume, bNoOverride, Radius, Pitch, Attenuate);

	INT Id = GetIndex()*16 + Slot*2 + bNoOverride;
	FVector Parameters = FVector(Volume, Radius, Pitch);

	if( Level->NetMode == NM_Client )
	{
		UClient* Client = GetLevel()->Engine->Client;
		if( Client )
		{
			for( INT i=0; i<Client->Viewports.Num(); i++ )
			{
				APlayerController* Hearer = Client->Viewports(i)->Actor;
				if( Hearer && Hearer->GetLevel()==GetLevel() )
					Hearer->CheckHearSound(this, Id, Sound, Parameters, Radius ? Radius : Sound->GetRadius(), Attenuate ); // sjs
			}
		}
	}
	else
	{
		AActor *RemoteOwner = NULL;
		if( Level->NetMode != NM_Standalone )
		{
			APawn* NoisePawn = this->GetAPawn();
			if ( !NoisePawn && Owner && Owner->GetAPawn() )
				NoisePawn = Owner->GetAPawn();

			if ( NoisePawn && !NoisePawn->IsLocallyControlled() )
					RemoteOwner = NoisePawn;
		}

		for( AController* Hearer=Level->ControllerList; Hearer; Hearer=Hearer->nextController )
		{
			if( Hearer->bIsPlayer && ((Hearer->Pawn != RemoteOwner) || (Hearer->Pawn == NULL)) )
				Hearer->CheckHearSound(this, Id, Sound, Parameters, Radius ? Radius : Sound->GetRadius(), Attenuate ); // sjs
		}
	}
	unguardexecSlow;
}

void AActor::execGetSoundDuration( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execGetSoundDuration);

	// Get parameters.
	P_GET_OBJECT(USound,Sound);
	P_FINISH;

	if ( Sound )
		*(FLOAT*)Result = Sound->GetDuration();
	else
		*(FLOAT*)Result = 0.f;
	unguardexec;
}

//////////////
// Movement //
//////////////

void AActor::execMove( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execMove);

	P_GET_VECTOR(Delta);
	P_FINISH;

	FCheckResult Hit(1.0f);
	*(DWORD*)Result = GetLevel()->MoveActor( this, Delta, Rotation, Hit );

	unguardexecSlow;
}

void AActor::execSetLocation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetLocation);

	P_GET_VECTOR(NewLocation);
	P_FINISH;

	*(DWORD*)Result = GetLevel()->FarMoveActor( this, NewLocation );
	unguardexecSlow;
}

void AActor::execSetRelativeLocation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetLocation);

	P_GET_VECTOR(NewLocation);
	P_FINISH;

	if ( AttachmentBone != NAME_None )
	{
		RelativeLocation = NewLocation;
		*(DWORD*)Result = 1;
	}
	else if ( Base )
		{
			FCoords Coords = GMath.UnitCoords / Base->Rotation;
			NewLocation = Base->Location + NewLocation.X * Coords.XAxis + NewLocation.Y * Coords.YAxis + NewLocation.Z * Coords.ZAxis;
		*(DWORD*)Result = GetLevel()->FarMoveActor( this, NewLocation,false,false,true );
		if ( Base )
			RelativeLocation = Location - Base->Location;
	}

	unguardexecSlow;
}

void AActor::execSetRotation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetRotation);

	P_GET_ROTATOR(NewRotation);
	P_FINISH;

	FCheckResult Hit(1.0f);
	*(DWORD*)Result = GetLevel()->MoveActor( this, FVector(0,0,0), NewRotation, Hit );

	unguardexecSlow;
}

void AActor::execSetRelativeRotation( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetRelativeRotation);

	P_GET_ROTATOR(NewRotation);
	P_FINISH;

	if ( AttachmentBone != NAME_None )
	{
		RelativeRotation = NewRotation;
		*(DWORD*)Result = 1;
	}
	else
	{
		if ( Base )
		{
			if(bHardAttach && !bBlockActors)
			{
				FVector currentRelLoc;
				currentRelLoc.X = HardRelMatrix.M[3][0];
				currentRelLoc.Y = HardRelMatrix.M[3][1];
				currentRelLoc.Z = HardRelMatrix.M[3][2];
	
				// We make a new HardRelMatrix using the new rotation and the existing position.
				HardRelMatrix = FRotationMatrix(NewRotation) * FTranslationMatrix(currentRelLoc);

				// Work out what the new chile rotation is
				FMatrix BaseTM = FRotationMatrix(Base->Rotation) * FTranslationMatrix(Base->Location);
				FMatrix NewWorldTM = HardRelMatrix * BaseTM;
				NewRotation = (GMath.UnitCoords * NewWorldTM.Coords()).OrthoRotation();
			}
			else
			{
				FCoords Coords = GMath.UnitCoords / Base->Rotation;
				FCoords PartCoords = GMath.UnitCoords / NewRotation;
				PartCoords = PartCoords * Coords.Transpose();
				NewRotation = PartCoords.OrthoRotation();
			}
		}
		FCheckResult Hit(1.0f);
		*(DWORD*)Result = GetLevel()->MoveActor( this, FVector(0,0,0), NewRotation, Hit );
	}
	unguardexecSlow;
}


// 
// Explicitly attach an object to / detach from an actor, thereby setting it as the base.
//
void AActor::execAttachToBone( FFrame& Stack, RESULT_DECL )
{
	//native final function bool AttachToBone( actor Attachment, name BoneName );
	guardSlow(AActor::execAttachToActorBone);	
	P_GET_OBJECT(AActor,Attachment);
	P_GET_NAME(BoneName);
	P_FINISH;
	
	if ( Attachment && (BoneName == NAME_None) )
	{
		Attachment->AttachmentBone = NAME_None;
		Attachment->SetBase( this, FVector(0.f,0.f,1.f) );
		*(DWORD*)Result = 1;
	}
	else
		*(DWORD*)Result = AttachToBone( Attachment, BoneName );

	unguardexecSlow;
}

void AActor::execDetachFromBone( FFrame& Stack, RESULT_DECL )
{
	//native final function bool DetachFromBone( actor Attachment );
	guardSlow(AActor::execDetachFromBone);
	P_GET_OBJECT(AActor,Attachment);
	P_FINISH;

	*(DWORD*)Result = DetachFromBone( Attachment );

	unguardexecSlow;
}



///////////////
// Relations //
///////////////

void AActor::execSetOwner( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetOwner);

	P_GET_ACTOR(NewOwner);
	P_FINISH;

	SetOwner( NewOwner );

	unguardexecSlow;
}

//////////////////
// Line tracing //
//////////////////

void AActor::execTrace( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execTrace);

	P_GET_VECTOR_REF(HitLocation);
	P_GET_VECTOR_REF(HitNormal);
	P_GET_VECTOR(TraceEnd);
	P_GET_VECTOR_OPTX(TraceStart,Location);
	P_GET_UBOOL_OPTX(bTraceActors,bCollideActors);
	P_GET_VECTOR_OPTX(TraceExtent,FVector(0,0,0));
	P_GET_OBJECT_REF(UMaterial, Material);
	P_FINISH;

	// Trace the line.
	FCheckResult Hit(1.0f);
	DWORD TraceFlags;
	if( bTraceActors )
		TraceFlags = TRACE_ProjTargets;
	else
		TraceFlags = TRACE_World;

	if( Material )
		TraceFlags |= TRACE_Material;

	AActor *TraceActor = this;
	AController *C = Cast<AController>(this);
	if ( C && C->Pawn )
		TraceActor = C->Pawn;
	GetLevel()->SingleLineCheck( Hit, TraceActor, TraceEnd, TraceStart, TraceFlags, TraceExtent );

	*(AActor**)Result = Hit.Actor;
	*HitLocation      = Hit.Location;
	*HitNormal        = Hit.Normal;
	if( Material )
		*Material = Hit.Material;
	unguardexecSlow;
}

void AActor::execFastTrace( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execTrace);

	P_GET_VECTOR(TraceEnd);
	P_GET_VECTOR_OPTX(TraceStart,Location);
	P_FINISH;

	// Trace the line.
	FCheckResult Hit(1.f);
	GetLevel()->SingleLineCheck( Hit, this, TraceEnd, TraceStart, TRACE_World|TRACE_StopAtFirstHit );

	*(DWORD*)Result = !Hit.Actor;

	unguardexecSlow;
}

void AActor::execTraceThisActor( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execTraceThisActor);

	P_GET_VECTOR_REF(HitLocation);
	P_GET_VECTOR_REF(HitNormal);
	P_GET_VECTOR(TraceEnd);
	P_GET_VECTOR(TraceStart);
	P_GET_VECTOR_OPTX(TraceExtent,FVector(0,0,0));
	P_FINISH;

	UPrimitive* Prim = GetPrimitive();
	if(Prim)
	{
		FCheckResult Hit(1.f);
		UBOOL NoHit = Prim->LineCheck(Hit, this, TraceEnd, TraceStart, TraceExtent, 0, 0);

		*(DWORD*)Result = NoHit;
		*HitLocation = Hit.Location;
		*HitNormal = Hit.Normal;
	}
	else
	{
		*(DWORD*)Result = 1;
	}

	unguard;
}

///////////////////////
// Spawn and Destroy //
///////////////////////

void AActor::execSpawn( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSpawn);

	P_GET_OBJECT(UClass,SpawnClass);
	P_GET_OBJECT_OPTX(AActor,SpawnOwner,NULL); 
	P_GET_NAME_OPTX(SpawnName,NAME_None);
	P_GET_VECTOR_OPTX(SpawnLocation,Location);
	P_GET_ROTATOR_OPTX(SpawnRotation,Rotation);
	P_FINISH;

	// SCRIPTTIME FLOAT OldClock = GStats.DWORDStats(GEngineStats.STATS_Game_SpawningCycles);
	clock(GStats.DWORDStats(GEngineStats.STATS_Game_SpawningCycles));
	// Spawn and return actor.
	AActor* Spawned = SpawnClass ? GetLevel()->SpawnActor
	(
		SpawnClass,
		NAME_None,
		SpawnLocation,
		SpawnRotation,
		NULL,
		0,
		0,
		SpawnOwner,
		Instigator
	) : NULL;
	if( Spawned && (SpawnName != NAME_None) )
		Spawned->Tag = SpawnName;
	*(AActor**)Result = Spawned;
	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_SpawningCycles));
	/* SCRIPTTIME
	if ( Spawned )
		debugf(TEXT("%s Spawned %s took %f"),GetName(),Spawned->GetName(), (GStats.DWORDStats(GEngineStats.STATS_Game_SpawningCycles) - OldClock) * GSecondsPerCycle * 1000.0f);
	else
		debugf(TEXT("%s Failed spawn took %f"),GetName(),(GStats.DWORDStats(GEngineStats.STATS_Game_SpawningCycles) - OldClock) * GSecondsPerCycle * 1000.0f);
	*/
	unguardexecSlow;
}

void AActor::execDestroy( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execDestroy);

	P_FINISH;
	
	*(DWORD*)Result = GetLevel()->DestroyActor( this );

	unguardexecSlow;
}

////////////
// Timing //
////////////

void AActor::execSetTimer( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSetTimer);

	P_GET_FLOAT(NewTimerRate);
	P_GET_UBOOL(bLoop);
	P_FINISH;

	TimerCounter = 0.0f;
	TimerRate    = NewTimerRate;
	bTimerLoop   = bLoop;

	unguardexecSlow;
}

////////////////
// Warp zones //
////////////////

void AWarpZoneInfo::execWarp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AWarpZoneInfo::execWarp);

	P_GET_VECTOR_REF(WarpLocation);
	P_GET_VECTOR_REF(WarpVelocity);
	P_GET_ROTATOR_REF(WarpRotation);
	P_FINISH;

	// Perform warping.
	*WarpLocation = (*WarpLocation).TransformPointBy ( WarpCoords.Transpose() );
	*WarpVelocity = (*WarpVelocity).TransformVectorBy( WarpCoords.Transpose() );
	*WarpRotation = (GMath.UnitCoords / *WarpRotation * WarpCoords.Transpose()).OrthoRotation();

	unguardexecSlow;
}

void AWarpZoneInfo::execUnWarp( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AWarpZoneInfo::execUnWarp);

	P_GET_VECTOR_REF(WarpLocation);
	P_GET_VECTOR_REF(WarpVelocity);
	P_GET_ROTATOR_REF(WarpRotation);
	P_FINISH;

	// Perform unwarping.
	*WarpLocation = (*WarpLocation).TransformPointBy ( WarpCoords );
	*WarpVelocity = (*WarpVelocity).TransformVectorBy( WarpCoords );
	*WarpRotation = (GMath.UnitCoords / *WarpRotation * WarpCoords).OrthoRotation();

	unguardexecSlow;
}

/*-----------------------------------------------------------------------------
	Native iterator functions.
-----------------------------------------------------------------------------*/

void AActor::execAllActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execAllActors);

	// Get the parms.
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_NAME_OPTX(TagName,NAME_None);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if(	TestActor && 
                !TestActor->IsPendingKill() && //amb
                TestActor->IsA(BaseClass) && 
                (TagName==NAME_None || TestActor->Tag==TagName) )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}

void AActor::execDynamicActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execDynamicActors);

	// Get the parms.
	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_NAME_OPTX(TagName,NAME_None);
	P_FINISH;
	
	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor = GetLevel()->iFirstDynamicActor;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if(	TestActor && 
                !TestActor->IsPendingKill() && //amb
                TestActor->IsA(BaseClass) && 
                (TagName==NAME_None || TestActor->Tag==TagName) )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}

void AActor::execChildActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execChildActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if(	TestActor && 
                !TestActor->IsPendingKill() && //amb
                TestActor->IsA(BaseClass) && 
                TestActor->IsOwnedBy( this ) )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}

void AActor::execBasedActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execBasedActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iBased=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		for( iBased; iBased<Attached.Num() && *OutActor==NULL; iBased++ )
		{
			AActor* TestActor = Attached(iBased);
			if(	TestActor &&
                !TestActor->IsPendingKill() && //amb
                TestActor->IsA(BaseClass))
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}

void AActor::execTouchingActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execTouchingActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iTouching=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		for( iTouching; iTouching<Touching.Num() && *OutActor==NULL; iTouching++ )
		{
			AActor* TestActor = Touching(iTouching);
			if(	TestActor &&
                !TestActor->IsPendingKill() && //amb
                TestActor->IsA(BaseClass))
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}

void AActor::execTraceActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execTraceActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_VECTOR_REF(HitLocation);
	P_GET_VECTOR_REF(HitNormal);
	P_GET_VECTOR(End);
	P_GET_VECTOR_OPTX(Start,Location);
	P_GET_VECTOR_OPTX(TraceExtent,FVector(0,0,0));
	P_FINISH;

	FMemMark Mark(GMem);
	BaseClass         = BaseClass ? BaseClass : AActor::StaticClass();
	FCheckResult* Hit = GetLevel()->MultiLineCheck( GMem, End, Start, TraceExtent, Level, TRACE_AllColliding, this );

	PRE_ITERATOR;
		if( Hit )
		{
			if ( Hit->Actor && 
                 !Hit->Actor->IsPendingKill() && //amb
                 Hit->Actor->IsA(BaseClass))
			{
				*OutActor    = Hit->Actor;
				*HitLocation = Hit->Location;
				*HitNormal   = Hit->Normal;
			}
			Hit = Hit->GetNext();
		}
		else
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			*OutActor = NULL;
			break;
		}
	POST_ITERATOR;
	Mark.Pop();

	unguardexecSlow;
}

void AActor::execRadiusActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execRadiusActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_FLOAT(Radius);
	P_GET_VECTOR_OPTX(TraceLocation,Location);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if
			(	TestActor
            &&  !TestActor->IsPendingKill() //amb
			&&	TestActor->IsA(BaseClass) 
			&&	(TestActor->Location - TraceLocation).SizeSquared() < Square(Radius + TestActor->CollisionRadius) )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}

void AActor::execVisibleActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execVisibleActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_FLOAT_OPTX(Radius,0.0f);
	P_GET_VECTOR_OPTX(TraceLocation,Location);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor=0;
	FCheckResult Hit(1.f);

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if
			(	TestActor
			&& !TestActor->bHidden
            &&  !TestActor->IsPendingKill() //amb
			&&	TestActor->IsA(BaseClass)
			&&	(Radius==0.0f || (TestActor->Location-TraceLocation).SizeSquared() < Square(Radius)) )
			{
				GetLevel()->SingleLineCheck( Hit, this, TestActor->Location, TraceLocation, TRACE_World/*|TRACE_StopAtFirstHit*/ ); // sjs -stop at first hit sucks?
				if ( !Hit.Actor || (Hit.Actor == TestActor) )
					*OutActor = TestActor;
			}
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}

void AActor::execVisibleCollidingActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execVisibleCollidingActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_FLOAT(Radius);
	P_GET_VECTOR_OPTX(TraceLocation,Location);
	P_GET_UBOOL_OPTX(bIgnoreHidden, 0); 
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	FMemMark Mark(GMem);
	FCheckResult* Link=GetLevel()->Hash->ActorRadiusCheck( GMem, TraceLocation, Radius, 0 );
	
	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		FCheckResult Hit(1.f);
		if ( Link )
		{
			while ( Link )
			{
				if( !Link->Actor
                    ||  Link->Actor->IsPendingKill() //amb
					||	!Link->Actor->IsA(BaseClass) 
					||  (bIgnoreHidden && Link->Actor->bHidden) )
				{
					Link=Link->GetNext();
				}
				else
				{
					GetLevel()->SingleLineCheck( Hit, this, Link->Actor->Location, TraceLocation, TRACE_World );
					if ( Hit.Actor && (Hit.Actor != Link->Actor) )
						Link=Link->GetNext();
					else
						break;
				}
			}

			if ( Link )
			{
				*OutActor = Link->Actor;
				Link=Link->GetNext();
			}
		}
		if ( *OutActor == NULL ) 
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	Mark.Pop();
	unguardexecSlow;
}

void AActor::execCollidingActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execCollidingActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_GET_FLOAT(Radius);
	P_GET_VECTOR_OPTX(TraceLocation,Location);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	FMemMark Mark(GMem);
	FCheckResult* Link=GetLevel()->Hash->ActorRadiusCheck( GMem, TraceLocation, Radius, 0 );
	
	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		FCheckResult Hit(1.f);
		if ( Link )
		{
			while ( Link )
			{
				if( !Link->Actor
                    ||  Link->Actor->IsPendingKill() //amb
					||	!Link->Actor->IsA(BaseClass) )
				{
					Link=Link->GetNext();
				}
				else
					break;
			}

			if ( Link )
			{
				*OutActor = Link->Actor;
				Link=Link->GetNext();
			}
		}
		if ( *OutActor == NULL ) 
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	Mark.Pop();
	unguardexecSlow;
}

void AZoneInfo::execZoneActors( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AZoneInfo::execZoneActors);

	P_GET_OBJECT(UClass,BaseClass);
	P_GET_ACTOR_REF(OutActor);
	P_FINISH;

	BaseClass = BaseClass ? BaseClass : AActor::StaticClass();
	INT iActor=0;

	PRE_ITERATOR;
		// Fetch next actor in the iteration.
		*OutActor = NULL;
		while( iActor<GetLevel()->Actors.Num() && *OutActor==NULL )
		{
			AActor* TestActor = GetLevel()->Actors(iActor++);
			if
			(	TestActor
            &&  !TestActor->IsPendingKill() //amb
			&&	TestActor->IsA(BaseClass)
			&&	TestActor->IsInZone(this) )
				*OutActor = TestActor;
		}
		if( *OutActor == NULL )
		{
			Stack.Code = &Stack.Node->Script(wEndOffset + 1);
			break;
		}
	POST_ITERATOR;

	unguardexecSlow;
}

/*-----------------------------------------------------------------------------
	Script processing function.
-----------------------------------------------------------------------------*/

void AActor::ExecutingBadStateCode(FFrame& Stack) 
{
	guard(AActor::ExecutingBadStateCode);

	debugf( NAME_Critical, TEXT("Unknown code token %02X - attempt to recover by setting bBadStateCode for actors"), Stack.Code[-1] );
	debugf(TEXT("Problem is in %s possibly because of state change to %s during execution of subexpression"), GetFullName(), GetStateFrame()->StateNode->GetFullName());
	bBadStateCode = true;
	unguard;
}


//
// Execute the state code of the actor.
//
void AActor::ProcessState( FLOAT DeltaSeconds )
{
	if
	(	GetStateFrame()
	&&	GetStateFrame()->Code
	&&	(Role>=ROLE_Authority || (GetStateFrame()->StateNode->StateFlags & STATE_Simulated))
	&&	!IsPendingKill() )
	{
		UState* StartStateNode = GetStateFrame()->StateNode;
		UState* OldStateNode = GetStateFrame()->StateNode;
		guard(AActor::ProcessState);
		// SCRIPTTIME FLOAT StartTime = GScriptCycles;
		if( ++GScriptEntryTag==1 )
			clock(GScriptCycles);

		// If a latent action is in progress, update it.
		if( GetStateFrame()->LatentAction )
			(this->*GNatives[GetStateFrame()->LatentAction])( *GetStateFrame(), (BYTE*)&DeltaSeconds );

		// Execute code.
		INT NumStates=0;
		INT NumCode=0;
		while( !bDeleteMe && GetStateFrame()->Code && !GetStateFrame()->LatentAction && !bBadStateCode )
		{
			BYTE Buffer[MAX_CONST_SIZE];
			GetStateFrame()->Step( this, Buffer );
			if ( NumCode++ > 1000000 )
			{
					bBadStateCode = true;
					GetStateFrame()->Code = NULL;
					debugf(NAME_Critical, TEXT("%s Infinite loop in state code - possibly because of state change to %s during execution of subexpression"), GetFullName(), GetStateFrame()->StateNode->GetFullName());
			} 
			if ( !bBadStateCode )
			{
				if( GetStateFrame()->StateNode!=OldStateNode )
				{
					StartStateNode = OldStateNode;
					OldStateNode = GetStateFrame()->StateNode;
					if( ++NumStates > 4 )
					{
						//GetStateFrame().Logf( "Pause going from %s to %s", xx, yy );
						break;
					}
				}
			}
			else
			{
				// Actor can try to recover from state code stack getting screwed up by state change during state code execution
				eventRecoverFromBadStateCode();
				break;
			}
		}
		if( --GScriptEntryTag==0 )
		{
			unclock(GScriptCycles);
			/* SCRIPTTIME
			FLOAT PT = (GScriptCycles - StartTime) * GSecondsPerCycle * 1000.0f;
			if ( PT > 0.01f )
			{
				debugf(TEXT("%s processSTATE"),GetName());
				if ( PT > 0.1f )
					debugf(TEXT("***STATE processing time %f"),PT);
				else
					debugf(TEXT("   STATE processing time %f"),PT);
			}
			*/
		}
		unguardf(( TEXT("Object %s, Old State %s, New State %s"), GetFullName(), StartStateNode->GetFullName(), GetStateFrame()->StateNode->GetFullName() ));
	}
}

//
// Internal RPC calling.
//
static inline void InternalProcessRemoteFunction
(
	AActor*			Actor,
	UNetConnection*	Connection,
	UFunction*		Function,
	void*			Parms,
	FFrame*			Stack,
	UBOOL			IsServer
)
{
	guardSlow(InternalProcessRemoteFunction);
	
	GStats.DWORDStats(GEngineStats.STATS_Net_NumRPC)++;

	// Make sure this function exists for both parties.
	FClassNetCache* ClassCache = Connection->PackageMap->GetClassNetCache( Actor->GetClass() );
	if( !ClassCache )
		return;
	FFieldNetCache* FieldCache = ClassCache->GetFromField( Function );
	if( !FieldCache )
		return;

	// Get the actor channel.
	UActorChannel* Ch = Connection->ActorChannels.FindRef(Actor);
	if( !Ch )
	{
		if( IsServer )
			Ch = (UActorChannel *)Connection->CreateChannel( CHTYPE_Actor, 1 );
		if( !Ch )
			return;
		if( IsServer )
			Ch->SetChannelActor( Actor );
	}

	// Make sure initial channel-opening replication has taken place.
	if( Ch->OpenPacketId==INDEX_NONE )
	{
		if( !IsServer )
			return;
		Ch->ReplicateActor();
	}

	// Form the RPC preamble.
	FOutBunch Bunch( Ch, 0 );
	//debugf(TEXT("   Call %s"),Function->GetFullName());
	Bunch.WriteInt( FieldCache->FieldNetIndex, ClassCache->GetMaxIndex() );

	// Form the RPC parameters.
	if( Stack )
	{
		appMemzero( Parms, Function->ParmsSize );
        for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
			Stack->Step( Stack->Object, (BYTE*)Parms + It->Offset );
		checkSlow(*Stack->Code==EX_EndFunctionParms);
	}
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Function); It && (It->PropertyFlags & (CPF_Parm|CPF_ReturnParm))==CPF_Parm; ++It )
	{
		if( Connection->PackageMap->ObjectToIndex(*It)!=INDEX_NONE )
		{
			UBOOL Send = 1;
			if( !FlagCast<UBoolProperty,CLASS_IsAUBoolProperty>(*It) )//->IsA(UBoolProperty::StaticClass()) )
			{
				Send = !It->Matches(Parms,NULL,0);
				Bunch.WriteBit( Send );
			}
			if( Send )
				It->NetSerializeItem( Bunch, Connection->PackageMap, (BYTE*)Parms + It->Offset );
		}
	}

	// Reliability.
	//warning: RPC's might overflow, preventing reliable functions from getting thorough.
	if( Function->FunctionFlags & FUNC_NetReliable )
		Bunch.bReliable = 1;

	// Send the bunch.
	if( Bunch.IsError() )
		debugf( NAME_DevNetTraffic, TEXT("RPC bunch overflowed") );
	else
	if( Ch->Closing )
		debugf( NAME_DevNetTraffic, TEXT("RPC bunch on closing channel") );
	else
	{
//		debugf( NAME_DevNetTraffic, TEXT("      Sent RPC: %s::%s"), Actor->GetName(), Function->GetName() );
		Ch->SendBunch( &Bunch, 1 );
	}

	unguardSlow;
}

//
// Return whether a function should be executed remotely.
//
UBOOL AActor::ProcessRemoteFunction( UFunction* Function, void* Parms, FFrame* Stack )
{
	guard(AActor::ProcessRemoteFunction);

	// Quick reject.
	if( (Function->FunctionFlags & FUNC_Static) || bDeleteMe )
		return 0;
	UBOOL Absorb = (Role<=ROLE_SimulatedProxy) && !(Function->FunctionFlags & (FUNC_Simulated | FUNC_Native));
	if( GetLevel()->DemoRecDriver )
	{
		if( GetLevel()->DemoRecDriver->ServerConnection )
			return Absorb;
		ProcessDemoRecFunction( Function, Parms, Stack );
	}
	if( Level->NetMode==NM_Standalone )
		return 0;
	if( !(Function->FunctionFlags & FUNC_Net) )
		return Absorb;

	// Check if the actor can potentially call remote functions.
	// FIXME - cleanup finding top - or make function
    APlayerController* Top = GetTopPlayerController();//Cast<APlayerController>(GetTopOwner());
	UNetConnection* ClientConnection = NULL;
	if
	(	(Role==ROLE_Authority)
	&&	(Top==NULL || (ClientConnection=Cast<UNetConnection>(Top->Player))==NULL) )
		return Absorb;

	// See if UnrealScript replication condition is met.
	while( Function->GetSuperFunction() )
		Function = Function->GetSuperFunction();
	UBOOL Val=0;
	FFrame( this, Function->GetOwnerClass(), Function->RepOffset, NULL ).Step( this, &Val );
	if( !Val )
		return Absorb;

	// Get the connection.
	UBOOL           IsServer   = Level->NetMode==NM_DedicatedServer || Level->NetMode==NM_ListenServer;
	UNetConnection* Connection = IsServer ? ClientConnection : GetLevel()->NetDriver->ServerConnection;
	if ( !Connection )
		return 1;

	// If saturated and function is unimportant, skip it.
	if( !(Function->FunctionFlags & FUNC_NetReliable) && !Connection->IsNetReady(0) )
		return 1;

	// Send function data to remote.
	InternalProcessRemoteFunction( this, Connection, Function, Parms, Stack, IsServer );
	return 1;

	unguardf(( TEXT("(%s)"), Function->GetFullName() ));
}

// Replicate a function call to a demo recording file
void AActor::ProcessDemoRecFunction( UFunction* Function, void* Parms, FFrame* Stack )
{
	guard(AActor::ProcessDemoRecFunction);

	// Check if the function is replicatable
	if( (Function->FunctionFlags & (FUNC_Static|FUNC_Net))!=FUNC_Net || bNetTemporary )
		return;

	UBOOL IsNetClient = (Level->NetMode == NM_Client);

	// Check if actor was spawned locally in a client-side demo 
	if(IsNetClient && Role == ROLE_Authority)
		return;

	// See if UnrealScript replication condition is met.
	while( Function->GetSuperFunction() )
		Function = Function->GetSuperFunction();

	UBOOL Val=0;
	if(IsNetClient)
		Exchange(RemoteRole, Role);
	bDemoRecording = 1;
	bClientDemoRecording = IsNetClient;
	FFrame( this, Function->GetOwnerClass(), Function->RepOffset, NULL ).Step( this, &Val );
	bDemoRecording = 0;
	bClientDemoRecording = 0;
	if(IsNetClient)
		Exchange(RemoteRole, Role);
	bClientDemoNetFunc = 0;
	if( !Val )
		return;

	// Get the connection.
	if( !GetLevel()->DemoRecDriver->ClientConnections.Num() )
		return;
	UNetConnection* Connection = GetLevel()->DemoRecDriver->ClientConnections(0);
	if(!Connection )
		return;

	// Send function data to remote.
	BYTE* SavedCode = Stack ? Stack->Code : NULL;
	InternalProcessRemoteFunction( this, Connection, Function, Parms, Stack, 1 );
	if( Stack )
		Stack->Code = SavedCode;

	unguardf(( TEXT("(%s/%s)"), GetName(), Function->GetFullName() ));
}

/*-----------------------------------------------------------------------------
	GameInfo
-----------------------------------------------------------------------------*/

//
// Network
//
void AGameInfo::execGetNetworkNumber( FFrame& Stack, RESULT_DECL )
{
	guard(AGameInfo::execNetworkNumber);
	P_FINISH;

	*(FString*)Result = XLevel->NetDriver ? XLevel->NetDriver->LowLevelGetNetworkNumber() : FString(TEXT(""));

	unguardexec;
}

//
// Deathmessage parsing.
//
void AGameInfo::execParseKillMessage( FFrame& Stack, RESULT_DECL )
{
	guard(AGameInfo::execParseKillMessage);
	P_GET_STR(KillerName);
	P_GET_STR(VictimName);
	P_GET_STR(KillMessage);
	P_FINISH;

	FString Message, Temp;
	INT Offset;

	Temp = KillMessage;

	Offset = Temp.InStr(TEXT("%k"));
	if (Offset != -1)
	{
		Message = Temp.Left(Offset);
		Message += KillerName;
		Message += Temp.Right(Temp.Len() - Offset - 2);
		Temp = Message;
	}

	Offset = Temp.InStr(TEXT("%o"));
	if (Offset != -1)
	{
		Message = Temp.Left(Offset);
		Message += VictimName;
		Message += Temp.Right(Temp.Len() - Offset - 2);
	}

	*(FString*)Result = Message;

	unguardexec;
}

void AGameInfo::execLoadSRGrammar( FFrame& Stack, RESULT_DECL )
{
	guard(AGameInfo::execLoadSRGrammar);
	P_GET_STR(Grammar);
	P_FINISH;
	
	if( GetLevel()->Engine->Client && GetLevel()->Engine->Client->Viewports.Num() )
		GetLevel()->Engine->Client->Viewports(0)->LoadSRGrammar( 0, Grammar );

	unguard;
}


// Color functions
#define P_GET_COLOR(var)            P_GET_STRUCT(FColor,var)

void AActor::execMultiply_ColorFloat( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execMultiply_ColorFloat);

	P_GET_COLOR(A);
	P_GET_FLOAT(B);
	P_FINISH;

	A.R = (BYTE) (A.R * B);
	A.G = (BYTE) (A.G * B);
	A.B = (BYTE) (A.B * B);
	*(FColor*)Result = A;

	unguardexecSlow;
}	

void AActor::execMultiply_FloatColor( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execMultiply_FloatColor);

	P_GET_FLOAT (A);
	P_GET_COLOR(B);
	P_FINISH;

	B.R = (BYTE) (B.R * A);
	B.G = (BYTE) (B.G * A);
	B.B = (BYTE) (B.B * A);
	*(FColor*)Result = B;

	unguardexecSlow;
}	

void AActor::execAdd_ColorColor( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execAdd_ColorColor);

	P_GET_COLOR(A);
	P_GET_COLOR(B);
	P_FINISH;

	A.R = A.R + B.R;
	A.G = A.G + B.G;
	A.B = A.B + B.B;
	*(FColor*)Result = A;

	unguardexecSlow;
}

void AActor::execSubtract_ColorColor( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execSubtract_ColorColor);

	P_GET_COLOR(A);
	P_GET_COLOR(B);
	P_FINISH;

	A.R = A.R - B.R;
	A.G = A.G - B.G;
	A.B = A.B - B.B;
	*(FColor*)Result = A;

	unguardexecSlow;
}

// gam ---
void AActor::execUpdateURL( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execUpdateURL);

	P_GET_STR(NewOption);
	P_GET_STR(NewValue);
	P_GET_UBOOL(bSaveAsDefault);
	P_FINISH;

	UGameEngine* GameEngine = CastChecked<UGameEngine>( GetLevel()->Engine );
	if ( NewValue == TEXT("") )
		GameEngine->LastURL.RemoveOption( *NewOption, TEXT("DefaultPlayer"), TEXT("User") );
	else
	GameEngine->LastURL.AddOption( *(NewOption + TEXT("=") + NewValue) );

	if( bSaveAsDefault )
		GameEngine->LastURL.SaveURLConfig( TEXT("DefaultPlayer"), *NewOption, TEXT("User") );
	unguard;
}
void AActor::execGetUrlOption( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execUpdateURL);

	P_GET_STR(Option);
	P_FINISH;

	UGameEngine* GameEngine = CastChecked<UGameEngine>( GetLevel()->Engine );
	*(FString*)Result = FString( GameEngine->LastURL.GetOption(*(Option + FString(TEXT("="))), TEXT("")) );

	unguard;
}
// --- gam

void AVolume::execEncompasses( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execEncompasses);

	P_GET_ACTOR(InActor);
	P_FINISH;

	*(DWORD*)Result = Encompasses(InActor->Location);
	unguardexecSlow;
}

void AHUD::execDraw3DLine( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AHUD::execDraw3DLine);

	P_GET_VECTOR(Start);
	P_GET_VECTOR(End);
	P_GET_COLOR(LineColor);
	P_FINISH;

	if ( PlayerOwner && PlayerOwner->Player )
	{
		UViewport *V = Cast<UViewport>(PlayerOwner->Player);
		if ( V )
		{
			FLineBatcher LineBatcher(V->RI);
			LineBatcher.DrawLine(Start,End,LineColor);
		}
	}
	unguardexecSlow;
}

void AHUD::execDrawCanvasLine( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AHUD::execDrawCanvasLine);

	P_GET_FLOAT(X1);
	P_GET_FLOAT(Y1);
	P_GET_FLOAT(X2);
	P_GET_FLOAT(Y2);
	P_GET_COLOR(LineColor);
	P_FINISH;

	if ( PlayerOwner && PlayerOwner->Player )
	{
		
		UViewport *V = Cast<UViewport>(PlayerOwner->Player);
		if ( V )
		{
			FCanvasUtil CanvasUtil(&V->RenderTarget, V->RI);
			CanvasUtil.DrawLine(X1, Y1, X2, Y2, LineColor);	
		}
	}
	unguardexecSlow;
}

void AHUD::execStaticDrawCanvasLine( FFrame& Stack, RESULT_DECL )
{
	guard(AHUD::StaticDrawCanvasLine);

	P_GET_OBJECT(UCanvas,C);
	P_GET_FLOAT(X1);
	P_GET_FLOAT(Y1);
	P_GET_FLOAT(X2);
	P_GET_FLOAT(Y2);
	P_GET_COLOR(LineColor);
	P_FINISH;

	if ( C )
	{
		if ( C->pCanvasUtil )
			C->pCanvasUtil->DrawLine(X1, Y1, X2, Y2, LineColor);
		else debugf(TEXT("StaticDrawCanvasLine: Canvas has invalid pCanvasUtil"));
	}
	else debugf(TEXT("StaticDrawCanvasLine: None value specified for Canvas parameter"));


	unguardexec;
}

/*---------------------------------------------------------------------------------------
	Projectors
---------------------------------------------------------------------------------------*/

void AActor::AttachProjector( AProjector* Projector )
{
	// Remove old projectors.

	for(INT ProjectorIndex = 0;ProjectorIndex < Projectors.Num();ProjectorIndex++)
		if(!Projectors(ProjectorIndex)->Render( Level->TimeSeconds ))
			Projectors.Remove(ProjectorIndex--);

	// Add the new projector.

	Projectors.AddUniqueItem( Projector->RenderInfo->AddReference() );
}

void AActor::DetachProjector( AProjector* Projector )
{
	Projector->RenderInfo->RemoveReference();
	Projectors.RemoveItem( Projector->RenderInfo );
}

//
// AActor::GetProjectorBase - who's projector list to use.
//
AActor* AActor::GetProjectorBase()
{
	if( bCollideActors || !bAcceptsProjectors )
		return this;

	if( Base )
		return Base->GetProjectorBase();

	return this;
}

//
//	AActor::execGetRenderBoundingSphere
//

void AActor::execGetRenderBoundingSphere(FFrame& Stack,RESULT_DECL)
{
	guard(AActor::execGetRenderBoundingSphere);

	P_FINISH;

	FMatrix	_LocalToWorld;

	if(DrawType == DT_Mesh && Mesh)
		_LocalToWorld = Mesh->MeshGetInstance(this)->MeshToWorld();
	else
		_LocalToWorld = LocalToWorld();

	*(FSphere*)Result = GetPrimitive()->GetRenderBoundingSphere(this).TransformBy(_LocalToWorld);

	unguardexec;
}

//
//	AActor::execDrawDebugLine
//

void AActor::execDrawDebugLine(FFrame& Stack,RESULT_DECL)
{
	guard(AActor::execDrawDebugLine);

	P_GET_VECTOR(LineStart);
	P_GET_VECTOR(LineEnd);
	P_GET_BYTE(R);
	P_GET_BYTE(G);
	P_GET_BYTE(B);
	P_FINISH;

	GTempLineBatcher->AddLine(LineStart, LineEnd, FColor(R, G, B));

	unguard;
}

// Dave@Psyonix
void AActor::execDrawStayingDebugLine(FFrame& Stack,RESULT_DECL)
{
	guard(AActor::execDrawStayingDebugLine);

	P_GET_VECTOR(LineStart);
	P_GET_VECTOR(LineEnd);
	P_GET_BYTE(R);
	P_GET_BYTE(G);
	P_GET_BYTE(B);
	P_FINISH;

	GTempLineBatcher->AddStayingLine(LineStart, LineEnd, FColor(R, G, B));

	unguard;
}

// Dave@Psyonix
void AActor::execClearStayingDebugLines(FFrame& Stack,RESULT_DECL)
{
	guard(AActor::execClearStayingDebugLines);

	P_FINISH;

	GTempLineBatcher->ClearStayingLines();

	unguard;
}

// Dave@Psyonix
void AActor::execDrawDebugCircle(FFrame& Stack,RESULT_DECL)
{
	guard(AActor::execDrawDebugCircle);

	P_GET_VECTOR(Base);
	P_GET_VECTOR(X);
	P_GET_VECTOR(Y);
	P_GET_FLOAT(Radius);
	P_GET_INT(NumSides);
	P_GET_BYTE(R);
	P_GET_BYTE(G);
	P_GET_BYTE(B);
	P_FINISH;

	GTempLineBatcher->AddCircle(Base, X, Y, FColor(R, G, B), Radius, NumSides);

	unguard;
}

// Dave@Psyonix
void AActor::execDrawDebugSphere(FFrame& Stack,RESULT_DECL)
{
	guard(AActor::execDrawDebugSphere);

	P_GET_VECTOR(Base);
	P_GET_FLOAT(Radius);
	P_GET_INT(NumDivisions);
	P_GET_BYTE(R);
	P_GET_BYTE(G);
	P_GET_BYTE(B);
	P_FINISH;

	GTempLineBatcher->AddSphere(Base, FColor(R, G, B), Radius, NumDivisions);

	unguard;
}

// jdf ---

void AActor::execPlayFeedbackEffect( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execPlayFeedbackEffect);

	P_GET_STR(EffectName);
	P_FINISH;

    PlayFeedbackEffect( TCHAR_TO_ANSI(*EffectName) );

    unguardSlow;
}

void AActor::execStopFeedbackEffect( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execStopFeedbackEffect);

    P_GET_STR_OPTX(EffectName, TEXT("@#$%!"));
	P_FINISH;

    if( EffectName.Len() )
        StopFeedbackEffect( EffectName!=TEXT("@#$%!") ? TCHAR_TO_ANSI(*EffectName) : NULL );

    unguardSlow;
}

void AActor::execForceFeedbackSupported( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execFeedbackEffectSupported);

	P_GET_UBOOL_OPTX(Enable, 1);
	P_FINISH;
	void* hWnd = NULL;

	if (Enable)
	{
		TObjectIterator<UEngine> EngineIt;
		if
		(	EngineIt
		&&	EngineIt->Client
		&&	EngineIt->Client->Viewports.Num() )
			hWnd = EngineIt->Client->Viewports(0)->GetWindow();

		InitForceFeedback(hInstance, hWnd);
	}
	else
		ExitForceFeedback();

    *(UBOOL*)Result = (UBOOL)GForceFeedbackAvailable;

    unguardSlow;
}

void AActor::execChangeSpringFeedbackEffect( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execChangeSpringFeedbackEffect);

	P_GET_STR(EffectName);
	P_GET_FLOAT_OPTX(CenterX, 0);
	P_GET_FLOAT_OPTX(CenterY, 0);
	P_FINISH;

	// Assumes fCenterX, fCenterY are normalized
	ChangeSpringFeedbackEffect( TCHAR_TO_ANSI(*EffectName), (INT)(CenterX * 10000), (INT)(CenterY * 10000) );

    unguardSlow;
}

void AActor::execChangeBaseParamsFeedbackEffect( FFrame& Stack, RESULT_DECL )
{
	guardSlow(AActor::execChangeSpringFeedbackEffect);

	P_GET_STR(EffectName);
	P_GET_FLOAT_OPTX(DirectionX, -2.0);
	P_GET_FLOAT_OPTX(DirectionY, -2.0);
	P_GET_FLOAT_OPTX(Gain, -2.0);
	P_FINISH;

	ChangeBaseParamsForceFeedback
	(	TCHAR_TO_ANSI(*EffectName)
	,	DirectionX * 10000
	,	DirectionY * 10000
	,	Gain * 10000
	);

    unguardSlow;
}
// --- jdf

/*---------------------------------------------------------------------------------------
	CD Key Validation: MasterServer -> GameServer -> Client -> GameServer -> MasterServer
---------------------------------------------------------------------------------------*/

//
// Request a CD Key challenge response from the client.
//
void APlayerController::execClientValidate(FFrame& Stack,RESULT_DECL)
{
	guard(APlayerController::execClientValidate);
	P_GET_STR( Challenge );
	P_FINISH;
	eventServerValidationResponse( GetCDKeyResponse(*Challenge) );
	unguard;
}

//
// CD Key Challenge response received from the client.
//
void APlayerController::execServerValidationResponse(FFrame& Stack,RESULT_DECL)
{
	guard(APlayerController::execServerValidationResponse);
	P_GET_STR( Response );
	P_FINISH;
	UNetConnection* Conn = Cast<UNetConnection>(Player);

	// Store it in the connection.  The uplink will pick this up.
	if( Conn )
		Conn->CDKeyResponse = Response;
	unguard;
}

void AActor::execClock(FFrame& Stack,RESULT_DECL)
{
	guard(AActor::execClock);
	P_GET_FLOAT_REF( Time );
	P_FINISH;
	clock(*Time);
	unguard;
}

void AActor::execUnClock(FFrame& Stack,RESULT_DECL)
{
	guard(AActor::execUnClock);
	P_GET_FLOAT_REF( Time );
	P_FINISH;
	unclock(*Time);
	*Time = *Time * GSecondsPerCycle * 1000.f;
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

