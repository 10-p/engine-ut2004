/*=============================================================================
	UnLevAct.cpp: Level actor functions
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	Level actor management.
-----------------------------------------------------------------------------*/

//
// Create a new actor. Returns the new actor, or NULL if failure.
//
AActor* ULevel::SpawnActor
(
	UClass*			Class,
	FName			InName,
	FVector			Location,
	FRotator		Rotation,
	AActor*			Template,
	UBOOL			bNoCollisionFail,
	UBOOL			bRemoteOwned,
	AActor*			Owner,
	APawn*			Instigator,
	UBOOL			bNoFail
)
{
	guard(ULevel::SpawnActor);

    if( GetFlags() & RF_Unreachable )
        return NULL;

	// Make sure this class is spawnable.
	if( !Class )
	{
		debugf( NAME_Warning, TEXT("SpawnActor failed because no class was specified") );
		return NULL;
	}
	if( Class->ClassFlags & CLASS_Abstract )
	{
		debugf( NAME_Warning, TEXT("SpawnActor failed because class %s is abstract"), Class->GetName() );
		return NULL;
	}
	else if( !Class->IsChildOf(AActor::StaticClass()) )
	{
		debugf( NAME_Warning, TEXT("SpawnActor failed because %s is not an actor class"), Class->GetName() );
		return NULL;
	}
	else if( !GIsEditor && (Class->GetDefaultActor()->bStatic || Class->GetDefaultActor()->bNoDelete) )
	{
		debugf( NAME_Warning, TEXT("SpawnActor failed because class %s has bStatic or bNoDelete"), Class->GetName() );
		if ( !bNoFail )
			return NULL;		
	}

	// don't spawn bHighDetail actors if not wanted
	if( !GIsEditor && Class->GetDefaultActor()->bHighDetail && !bNoFail )
	{
	    if( GetLevelInfo()->DetailMode == DM_Low || GetLevelInfo()->bDropDetail || (GetLevelInfo()->NetMode == NM_DedicatedServer) )
		{
			//debugf(TEXT("%s not spawned"),Class->GetName());
			return NULL;
		}
    }
#if 1
    // sjs - level's outer is not transient so we must do this
    // doing this is a huge benefit for long running names, as the name table grows > 40 megs after long multiplayer games.
    if( !GTransientNaming && InName==NAME_None)
        InName = NAME_Transient;
#endif

	// Use class's default actor as a template.
	if( !Template )
		Template = Class->GetDefaultActor();
	check(Template!=NULL);

	// Make sure actor will fit at desired location, and adjust location if necessary.
	if( (Template->bCollideWorld || (Template->bCollideWhenPlacing && (GetLevelInfo()->NetMode != NM_Client))) && !bNoCollisionFail )
		if( !FindSpot( Template->GetCylinderExtent(), Location ) )
			return NULL;

	// Add at end of list.
	INT iActor = Actors.Add();
    AActor* Actor = Actors(iActor) = (AActor*)StaticConstructObject( Class, GetOuter(), InName, 0, Template );

	Actor->SetFlags( RF_Transactional );

	// Set base actor properties.
	Actor->Tag		= Class->GetFName();
	Actor->Region	= FPointRegion( GetLevelInfo() );
	Actor->Level	= GetLevelInfo();
	Actor->bTicked  = !Ticked;
	Actor->XLevel	= this;

	// Set network role.
	check(Actor->Role==ROLE_Authority);
	if( bRemoteOwned )
		Exchange( Actor->Role, Actor->RemoteRole );

	// Remove the actor's brush, if it has one, because moving brushes are not duplicatable.
	if( Actor->Brush )
		Actor->Brush = NULL;

	// Set the actor's location and rotation.
	Actor->Location = Location;
	Actor->Rotation = Rotation;
	if( Actor->bCollideActors && Hash  )
		Hash->AddActor( Actor );

	// init actor's physics volume
	Actor->PhysicsVolume = 	GetLevelInfo()->PhysicsVolume; 

	// Set owner.
	Actor->SetOwner( Owner );

	// Set instigator
	Actor->Instigator = Instigator;

#ifdef WITH_KARMA
    // Initilise Karma physics for this actor (if there are any)
    KInitActorKarma(Actor);
#endif

	// Send messages.
	Actor->InitExecution();
	Actor->Spawned();
	Actor->eventPreBeginPlay();
	if( Actor->bDeleteMe && !bNoFail )
		return NULL;
	Actor->eventBeginPlay();
	if( Actor->bDeleteMe && !bNoFail )
		return NULL;

	// Set the actor's zone.
	Actor->SetZone( iActor==0, 1 );

	// Update the list of leaves this actor is in.
	Actor->ClearRenderData();

	// Check for encroachment.
	if( !bNoCollisionFail )
	{
		if( Actor->bCollideActors && Hash )
			Hash->RemoveActor( Actor );

		if( CheckEncroachment( Actor, Actor->Location, Actor->Rotation, 1 ) )
		{
			DestroyActor( Actor );
			return NULL;
		}
		if( Actor->bCollideActors && Hash )
			Hash->AddActor( Actor );
	}
	//if ( Actor->bCollideActors && !Actor->bBlockActors && !Actor->bUseCylinderCollision && (Actor->DrawType == DT_StaticMesh) )
	//	debugf(TEXT("%s shouldn't be using static mesh collision"),Actor->GetName());

	// Send PostBeginPlay.
	Actor->eventPostBeginPlay();
	if( Actor->bDeleteMe && !bNoFail )
		return NULL;

	Actor->PostBeginPlay();

	// Init scripting.
	Actor->eventSetInitialState();

	// Find Base
	if( !GIsEditor && !Actor->Base && Actor->bCollideWorld && Actor->bShouldBaseAtStartup 
		 && ((Actor->Physics == PHYS_None) || (Actor->Physics == PHYS_Rotating)) )
		Actor->FindBase();

	// Success: Return the actor.
	if( InTick )
		NewlySpawned = new(GEngineMem)FActorLink(Actor,NewlySpawned);

	// replicated actors will have postnetbeginplay() called in net code, after initial properties are received
	if ( !bRemoteOwned )
		Actor->eventPostNetBeginPlay();

	return Actor;
	unguardf(( TEXT("(%s)"), Class->GetName() ));
}

//
// Spawn a brush.
//
ABrush* ULevel::SpawnBrush()
{
	guard(ULevel::SpawnBrush);

	ABrush* Result = (ABrush*)SpawnActor( ABrush::StaticClass() );
	check(Result);

	return Result;
	unguard;
}

/* EditorDestroyActor()
version of DestroyActor() which should be called by the editor
*/
UBOOL ULevel::EditorDestroyActor( AActor* ThisActor )
{
	guard(ULevel::EditorDestroyActor);
	check(ThisActor);
	check(ThisActor->IsValid());

	if ( (ThisActor->bPathColliding && ThisActor->bBlockActors)
		|| ThisActor->IsA(ANavigationPoint::StaticClass()) 
		|| ThisActor->IsA(APickup::StaticClass()) )
		GetLevelInfo()->bPathsRebuilt = 0;

	return DestroyActor(ThisActor);
	unguard;
}

//
// Destroy an actor.
// Returns 1 if destroyed, 0 if it couldn't be destroyed.
//
// What this routine does:
// * Remove the actor from the actor list.
// * Generally cleans up the engine's internal state.
//
// What this routine does not do, but is done in ULevel::Tick instead:
// * Removing references to this actor from all other actors.
// * Killing the actor resource.
//
// This routine is set up so that no problems occur even if the actor
// being destroyed inside its recursion stack.
//
UBOOL ULevel::DestroyActor( AActor* ThisActor, UBOOL bNetForce )
{
	guard(ULevel::DestroyActor);
	check(ThisActor);
	check(ThisActor->IsValid());
	//debugf( NAME_Log, "Destroy %s", ThisActor->GetClass()->GetName() );

	// In-game deletion rules.
	guardSlow(DestroyRules);
	if( !GIsEditor )
	{
		// Can't kill bStatic and bNoDelete actors during play.
		if( ThisActor->bStatic || ThisActor->bNoDelete )
			return 0;

		// If already on list to be deleted, pretend the call was successful.
		if( ThisActor->bDeleteMe )
			return 1;

		// Can't kill if wrong role.
		if( ThisActor->Role!=ROLE_Authority && !bNetForce && !ThisActor->bNetTemporary )
			return 0;

		// Don't destroy player actors.
		APlayerController* PC = ThisActor->GetAPlayerController();
		if ( PC )
		{
			UNetConnection* C = Cast<UNetConnection>(PC->Player);
			if( C )
			{	
				if( C->Channels[0] && C->State!=USOCK_Closed )
				{
					PC->bPendingDestroy = true;
					C->Channels[0]->Close();
				}
				return 0;
			}
		}
	}
	unguardSlow;

	// Get index.
	INT iActor = GetActorIndex( ThisActor );
	guardSlow(ModifyActor);
	Actors.ModifyItem( iActor );
	ThisActor->Modify();
	unguardSlow;

	ThisActor->bPendingDelete = true;

	// Send EndState notification.
	guardSlow(EndState);
	if( ThisActor->GetStateFrame() && ThisActor->GetStateFrame()->StateNode )
	{
		ThisActor->eventEndState();
		if( ThisActor->bDeleteMe )
			return 1;
	}
	unguardSlow;

	// Tell this actor it's about to be destroyed.
	guardSlow(ProcessDestroyed);
	ThisActor->eventDestroyed();
	ThisActor->PostScriptDestroyed();
	unguardSlow;

	// Remove from base.
	guardSlow(Debase);
	if( ThisActor->Base )
	{
		ThisActor->SetBase( NULL );
		if( ThisActor->bDeleteMe )
			return 1;
	}

	// Iterate in reverse order as SetBase might remove current item from
	// the Attached array.	
	for( INT AttachmentIndex=ThisActor->Attached.Num()-1; AttachmentIndex>=0; AttachmentIndex-- )
	{
		if( ThisActor->Attached(AttachmentIndex) )
		{
			ThisActor->Attached(AttachmentIndex)->SetBase( NULL );
		}
	}
	unguardSlow;

	if( ThisActor->bDeleteMe )
		return 1;

	// Clean up all touching actors.
	guard(CleanupStandardRefs);
	INT iTemp = 0;
	clock(GStats.DWORDStats(GEngineStats.STATS_Game_CleanupDestroyedCycles));
	for ( INT i=0; i<ThisActor->Touching.Num(); i++ )
		if ( ThisActor->Touching(i) && ThisActor->Touching(i)->Touching.FindItem(ThisActor, iTemp) )
		{
			ThisActor->EndTouch( ThisActor->Touching(i), 1 );
			i--;
			if( ThisActor->bDeleteMe )
			{
				unclock(GStats.DWORDStats(GEngineStats.STATS_Game_CleanupDestroyedCycles));
				return 1;
			}
		}
	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_CleanupDestroyedCycles));
	unguard;

	// If this actor has an owner, notify it that it has lost a child.
	guardSlow(Disown);
	if( ThisActor->Owner )
	{
		ThisActor->Owner->eventLostChild( ThisActor );
		if( ThisActor->bDeleteMe )
			return 1;
	}
	unguardSlow;

	// Notify net players that this guy has been destroyed.
	if( NetDriver )
		NetDriver->NotifyActorDestroyed( ThisActor );

	// If demo recording, notify the demo.
	if( DemoRecDriver && !DemoRecDriver->ServerConnection )
		DemoRecDriver->NotifyActorDestroyed( ThisActor );


	// Remove from world collision hash.
	guardSlow(Unhash);
	if( Hash )
	{
		if( ThisActor->bCollideActors )
			Hash->RemoveActor( ThisActor ); 
		//Hash->CheckActorNotReferenced( ThisActor );
	}
	unguardSlow;

	// Clear the actor's render data.

	ThisActor->ClearRenderData();

	// Remove the actor from the actor list.
	guardSlow(Unlist);
	check(Actors(iActor)==ThisActor);
	Actors(iActor) = NULL;
	ThisActor->bDeleteMe = 1;
	unguardSlow;

	// ---- A destroyed PlayerController must not stay installed in a viewport ----
	// UPlayer::Actor is a raw pointer the garbage collector cannot see (UPlayer::Serialize does not
	// serialize it and no property registers it), and destroyed actors are `delete`d in
	// FinishDestroyedActors below, not left for a GC pass. So when a PlayerController that owns a
	// viewport is destroyed, Viewport->Actor would dangle and the next UGameEngine::Draw would call
	// eventPlayerCalcView through freed memory (under wasm, a low-address read does not fault, so the
	// first symptom is a virtual call into a recycled block, possibly a live object of another class).
	{
		APlayerController* DyingPC = ThisActor->GetAPlayerController();
		if( DyingPC && DyingPC->Player && DyingPC->Player->Actor==DyingPC )
		{
			debugf( NAME_Warning,
				TEXT("DestroyActor: %s (0x%08x, class %s, role=%i, netforce=%i) was the INSTALLED viewport actor — cleared Player->Actor (would otherwise dangle)"),
				DyingPC->GetName(), (DWORD)(PTRINT)DyingPC, DyingPC->GetClass()->GetName(),
				(INT)DyingPC->Role, (INT)bNetForce );
			DyingPC->Player->Actor = NULL;
		}
		else if( DyingPC )
		{
			debugf( NAME_DevNet,
				TEXT("[NET-DIAG] DestroyActor: PlayerController %s (0x%08x, class %s, role=%i, netforce=%i, Player=%s) destroyed"),
				DyingPC->GetName(), (DWORD)(PTRINT)DyingPC, DyingPC->GetClass()->GetName(),
				(INT)DyingPC->Role, (INT)bNetForce,
				DyingPC->Player ? DyingPC->Player->GetName() : TEXT("none") );
		}
	}

	// Do object destroy.
	guardSlow(ShutupSound);
	if( Engine->Audio )
		Engine->Audio->NoteDestroy( ThisActor );
	ThisActor->ConditionalDestroy();
	unguardSlow;

	// Cleanup mesh instance -> moved to ULevel::CleanupDestroyed()

	// Cleanup.
	guardSlow(Cleanup);
	if( !GIsEditor )
	{
		// During play, just add to delete-list list and destroy when level is unlocked.
		ThisActor->Deleted = FirstDeleted;
		FirstDeleted       = ThisActor;
	}
	else
	{
		// Destroy them now.
		CleanupDestroyed( 1 );
	}
	unguardSlow;

	// Return success.
	return 1;
	unguardf(( TEXT("(%s)"), ThisActor->GetFullName() ));
}

//
// Compact the actor list.
//
void ULevel::CompactActors()
{
	guard(ULevel::CompactActors);
	INT c = iFirstDynamicActor;
	for( INT i=iFirstDynamicActor; i<Actors.Num(); i++ )
	{
		if( Actors(i) )
		{
			if( !Actors(i)->bDeleteMe )
			{
				if(c != i)
					Actors.ModifyItem(c);

				Actors(c++) = Actors(i);
			}
			else
				debugf( TEXT("Undeleted %s"), Actors(i)->GetFullName() );
		}
	}
	if( c != Actors.Num() )
		Actors.Remove( c, Actors.Num()-c );
	unguard;
}

//
// Cleanup destroyed actors.
// During gameplay, called in ULevel::Unlock.
// During editing, called after each actor is deleted.
//
extern FCoronaRender GCoronaRender; 
void ULevel::CleanupDestroyed( UBOOL bForce )
{
	guard(ULevel::CleanupDestroyed);

	// Pack actor list.
	if( !GIsEditor && !bForce )
		CompactActors();

	// If nothing deleted, exit.
	if( !FirstDeleted )
		return;

	// Don't do anything unless a bunch of actors are in line to be destroyed.
	guard(CheckDeleted);
	if( !bForce )
    {
	INT c=0;
	for( AActor* A=FirstDeleted; A; A=A->Deleted )
		c++;
	    if( c<128 )
		return;
    }
	unguard;

	// Remove all references to actors tagged for deletion.
	guard(CleanupRefs);
	for( INT iActor=0; iActor<Actors.Num(); iActor++ )
	{
		AActor* Actor = Actors(iActor);
		if( Actor )
		{
			// Would be nice to say if(!Actor->bStatic), but we can't count on it.
			checkSlow(!Actor->bDeleteMe);
			Actor->DoCleanup();
		}
	}
	unguard;

	// If editor, let garbage collector destroy objects.
	if( GIsEditor )
		return;

	guard(RemoveCoronas);
	GCoronaRender.CleanupDestroyed();
	unguard;

	guard(FinishDestroyedActors);
	UBOOL bRefChecking = Engine && Engine->bSlowRefChecking;
	if ( bRefChecking )
	{
		// if doing reference checking, cleanup subobjects early
		AActor* DeleteList = FirstDeleted;
		while ( DeleteList != NULL )
		{
			DeleteList->CleanupSubObjects();
			DeleteList = DeleteList->Deleted;
		}
	}
	while( FirstDeleted!=NULL )
	{
		// Physically destroy the actor-to-delete.
		check(FirstDeleted->bDeleteMe);
		AActor* ActorToKill = FirstDeleted;
		FirstDeleted        = FirstDeleted->Deleted;
		checkSlow(ActorToKill->bDeleteMe);

		// clean up subobjects
		ActorToKill->CleanupSubObjects();

		if ( bRefChecking)
		{
			// Verify that no objects have this actor as an outer. This is
			// a very slow safety net that should only be used outside of production
			CheckDanglingOuter(ActorToKill);
			CheckDanglingRefs(ActorToKill);
		}

		// Destroy the actor.
		delete ActorToKill;
	}
	unguard;

	unguard;
}

/*-----------------------------------------------------------------------------
	Player spawning.
-----------------------------------------------------------------------------*/

//
// Find an available camera actor in the level and return it, or spawn a new
// one if none are available.  Returns actor number or NULL if none are
// available.
//
void ULevel::SpawnViewActor( UViewport* Viewport )
{
	guard(ULevel::SpawnViewActor);
	check(Engine->Client);
	check(Viewport->Actor==NULL);

	// Find an existing camera actor.
	guard(FindExisting);
	for( INT iActor=0; iActor<Actors.Num(); iActor++ )
	{
		ACamera* P = Cast<ACamera>( Actors(iActor) );
		if ( P && !P->Player && (Viewport->GetFName()==P->Tag) ) 
		{
			Viewport->Actor = P;
			break;
		}
    }
	unguard;

	guard(SpawnNew);
    if( !Viewport->Actor )
	{
		// None found, spawn a new one and set default position.
		ACamera* NewCamera = (ACamera*)SpawnActor( ACamera::StaticClass(), NAME_None, FVector(-500,-300,+300), FRotator(0,0,0), NULL, 1 );
		Viewport->Actor = NewCamera;
		check(Viewport->Actor);
		Viewport->Actor->ViewTarget = NewCamera;
		NewCamera->Tag = Viewport->GetFName();
	}
	unguard;

	// Set the new actor's properties.
	guard(SetProperties);
	if ( !Viewport->Actor->PlayerInput )
	{
		Viewport->Actor->eventInitInputSystem();
		if ( !Viewport->Actor->PlayerInput )
			Viewport->Actor->PlayerInput = Cast<UPlayerInput>(StaticConstructObject(UPlayerInput::StaticClass(), Viewport->Actor ));
	}
	Viewport->Actor->SetFlags( RF_NotForClient | RF_NotForServer );
	Viewport->Actor->ClearFlags( RF_Transactional );
	Viewport->Actor->Player		= Viewport;
	Viewport->Actor->ShowFlags	= SHOW_Frame | SHOW_MovingBrushes | SHOW_Volumes | SHOW_Actors | SHOW_Brush | SHOW_StaticMeshes | SHOW_Terrain | SHOW_Backdrop | SHOW_SelectionHighlight | SHOW_Coronas | SHOW_Particles | SHOW_BSP | SHOW_FluidSurfaces | SHOW_Projectors;
	Viewport->Actor->RendMap    = REN_DynLight;
	unguard;

	// Set the zone.
	Viewport->Actor->SetZone( 0, 1 );

	unguard;
}

//
// Spawn an actor for gameplay.
//
struct FAcceptInfo
{
	AActor*			Actor;
	FString			Name;
	TArray<FString> Parms;
	FAcceptInfo( AActor* InActor, const TCHAR* InName )
	: Actor( InActor ), Name( InName ), Parms()
	{}
};
APlayerController* ULevel::SpawnPlayActor( UPlayer* Player, ENetRole RemoteRole, const FURL& URL, FString& Error )
{
	guard(ULevel::SpawnPlayActor);
	Error=TEXT("");

	// Get package map.
	UPackageMap*    PackageMap = NULL;
	UNetConnection* Conn       = Cast<UNetConnection>( Player );
	if( Conn )
		PackageMap = Conn->PackageMap;

	// Make the option string.
	TCHAR Options[1024]=TEXT("");
	for( INT i=0; i<URL.Op.Num(); i++ )
	{
		appStrcat( Options, TEXT("?") );
		appStrcat( Options, *URL.Op(i) );
	}

	// Tell UnrealScript to log in.
	INT SavedActorCount = Actors.Num();//oldver: Login should say whether to accept inventory.
	APlayerController* Actor = GetLevelInfo()->Game->eventLogin( *URL.Portal, Options, Error );
	if( !Actor )
	{
		debugf( NAME_Warning, TEXT("Login failed: %s"), *Error);
		return NULL;
	}

	UBOOL AcceptInventory = (SavedActorCount!=Actors.Num());//oldver: Hack, accepts inventory iff actor was spawned.

	// Possess the newly-spawned player.
	Actor->SetPlayer( Player );
	//debugf(TEXT("%s got player %s"),Actor->GetName(), Player->GetName());
	Actor->Role       = ROLE_Authority;
	Actor->RemoteRole = RemoteRole;
	Actor->ShowFlags  = SHOW_Backdrop | SHOW_Actors | SHOW_StaticMeshes | SHOW_Terrain | SHOW_DistanceFog | SHOW_Backdrop | SHOW_PlayerCtrl | SHOW_RealTime | SHOW_Coronas | SHOW_Particles | SHOW_BSP | SHOW_FluidSurfaces | SHOW_Projectors;
	Actor->RendMap	  = REN_DynLight;
	GetLevelInfo()->Game->eventPostLogin( Actor );

	if ( Actor->Pawn && !Actor->Level->Game->bIsSaveGame )
	{
		Actor->eventTravelPreAccept();
		Actor->Pawn->eventTravelPreAccept();

		// Any saved items?
		const TCHAR* Str = NULL;
		if( AcceptInventory )
		{
			const TCHAR* PlayerName = URL.GetOption( TEXT("NAME="), *FURL::DefaultName );
			if( PlayerName )
			{
				FString* FoundItems = TravelInfo.Find( PlayerName );
				if( FoundItems )
					Str = **FoundItems;
			}
			if( !Str && GetLevelInfo()->NetMode==NM_Standalone )
			{
				TMap<FString,FString>::TIterator It(TravelInfo);
				if( It )
					Str = *It.Value();
			}
		}

		// Handle inventory items.
		TCHAR ClassName[256], ActorName[256];
		TArray<FAcceptInfo> Accepted;
		while( Str && Parse(Str,TEXT("CLASS="),ClassName,ARRAY_COUNT(ClassName)) && Parse(Str,TEXT("NAME="),ActorName,ARRAY_COUNT(ActorName)) )
		{
			// Load class.
			debugf( TEXT("Incoming travelling actor of class %s"), ClassName );//!!xyzzy
			FAcceptInfo* Accept=NULL;
			AActor* Spawned=NULL;
			UClass* Class=StaticLoadClass( AActor::StaticClass(), NULL, ClassName, NULL, LOAD_NoWarn|LOAD_AllowDll, PackageMap );
			if( !Class )
			{
				debugf( NAME_Log, TEXT("SpawnPlayActor: Cannot accept travelling class '%s'"), ClassName );
			}
			else if( Class->IsChildOf(APawn::StaticClass()) )
			{
				Accept = new(Accepted)FAcceptInfo(Actor->Pawn,ActorName);
			}
			else if( (Spawned=SpawnActor( Class, NAME_None, Actor->Pawn->Location, Actor->Pawn->Rotation, NULL, 1,0,Actor->Pawn ))==NULL )
			{
				debugf( NAME_Log, TEXT("SpawnPlayActor: Failed to spawn travelling class '%s'"), ClassName );
			}
			else
			{
				debugf( NAME_Log, TEXT("SpawnPlayActor: Spawned travelling actor") );
				Accept = new(Accepted)FAcceptInfo(Spawned,ActorName);
			}

			// Save properties.
			TCHAR Buffer[256];
			ParseLine(&Str,Buffer,ARRAY_COUNT(Buffer),1);
			ParseLine(&Str,Buffer,ARRAY_COUNT(Buffer),1);
			while( ParseLine(&Str,Buffer,ARRAY_COUNT(Buffer),1) && appStrcmp(Buffer,TEXT("}"))!=0 )
				if( Accept )
					new(Accept->Parms)FString(Buffer);
		}

		// Import properties.
		for( INT i=0; i<Accepted.Num(); i++ )
		{
			// Parse all properties.
			for( INT j=0; j<Accepted(i).Parms.Num(); j++ )
			{
				const TCHAR* Ptr = *Accepted(i).Parms(j);
				while( *Ptr==' ' )
					Ptr++;
				TCHAR VarName[256], *VarEnd=VarName;
				while( appIsAlnum(*Ptr) || *Ptr=='_' )
					*VarEnd++ = *Ptr++;
				*VarEnd=0;
				INT Element=0;
				if( *Ptr=='[' )
				{
					Element=appAtoi(++Ptr);
					while( appIsDigit(*Ptr) )
						Ptr++;
					if( *Ptr++!=']' )
						continue;
				}
				if( *Ptr++!='=' )
					continue;
				for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Accepted(i).Actor->GetClass()); It; ++It )
				{
					if
					(	(It->PropertyFlags & CPF_Travel)
					&&	appStricmp( It->GetName(), VarName )==0 
					&&	Element<It->ArrayDim )
					{
						// Import the property.
						BYTE* Data = (BYTE*)Accepted(i).Actor + It->Offset + Element*It->ElementSize;
						UObjectProperty* Ref = Cast<UObjectProperty>( *It );
						if( Ref && Ref->PropertyClass->IsChildOf(AActor::StaticClass()) )
						{
							for( INT k=0; k<Accepted.Num(); k++ )
							{
								if( Accepted(k).Name==Ptr )
								{
									*(UObject**)Data = Accepted(k).Actor;
									break;
								}
							}
						}
						else It->ImportText( Ptr, Data, 0 );
					}
				}
			}
		}

		// Call travel-acceptance functions in reverse order to avoid inventory flipping.
		for( INT i=Accepted.Num()-1; i>=0; i-- )
			Accepted(i).Actor->eventTravelPreAccept();
		GetLevelInfo()->Game->eventAcceptInventory( Actor->Pawn );
		for( INT i=Accepted.Num()-1; i>=0; i-- )
			Accepted(i).Actor->eventTravelPostAccept();
		Actor->eventTravelPostAccept();
	}

	return Actor;
	unguard;
}

/*-----------------------------------------------------------------------------
	Level actor moving/placing.
-----------------------------------------------------------------------------*/

// FindSpot()
// Find a suitable nearby location to place a collision box.
// No suitable location will ever be found if Location is not a valid point inside the level 

// CheckSlice() used by FindSpot()
UBOOL ULevel::CheckSlice( FVector& Location, FVector Extent, INT& bKeepTrying )
{
	guardSlow(ULevel::CheckSlice);
	FCheckResult Hit(1.f);
	FVector SliceExtent = Extent;
	SliceExtent.Z = 1.f;
	bKeepTrying = 0;

	if( !EncroachingWorldGeometry( Hit,Location, SliceExtent, 0, GetLevelInfo() ) )
	{
		// trace down to find floor
		FVector Down = FVector(0.f,0.f,Extent.Z);
		SingleLineCheck( Hit, NULL, Location - 2.f*Down, Location, TRACE_World, SliceExtent );
		FVector FloorNormal = Hit.Normal;
		if ( !Hit.Actor || (Hit.Time > 0.5f) )
		{
			// assume ceiling was causing problem
			if ( !Hit.Actor )
				Location = Location - Down; 
			else
				Location = Location - (2.f*Hit.Time-1.f) * Down + FVector(0.f,0.f,1.f);

			if( !EncroachingWorldGeometry( Hit,Location, Extent, 0, GetLevelInfo() ) )
			{
				// push back up to ceiling, and return
				SingleLineCheck( Hit, NULL, Location + Down, Location, TRACE_World, Extent );
				if ( Hit.Actor )
					Location = Hit.Location;
				return true;
			}
			else
			{
				// push out from floor, try to fit
				FloorNormal.Z = 0.f;
				Location = Location + FloorNormal * Extent.X;
				return ( !EncroachingWorldGeometry( Hit,Location, Extent, 0, GetLevelInfo() ) );
			}
		}
		else
		{
			// assume Floor was causing problem
			Location = Location + (0.5f-Hit.Time) * 2.f*Down + FVector(0.f,0.f,1.f);
			if( !EncroachingWorldGeometry( Hit,Location, Extent, 0, GetLevelInfo() ) )
				return true;
			else
			{
				// push out from floor, try to fit
				FloorNormal.Z = 0.f;
				Location = Location + FloorNormal * Extent.X;
				return ( !EncroachingWorldGeometry( Hit,Location, Extent, 0, GetLevelInfo() ) );
			}
		}
	}
	bKeepTrying = 1;
	return false;
	unguardSlow;
}

UBOOL ULevel::FindSpot(	FVector  Extent, FVector& Location )
{
	guard(ULevel::FindSpot);

	FCheckResult Hit(1.f);

	// check if fits at desired location
	if( !EncroachingWorldGeometry( Hit,Location, Extent, 0, GetLevelInfo() ) )
		return true;
	if( Extent==FVector(0.f,0.f,0.f) )
		return false;

	FVector StartLoc = Location;

	// Check if slice fits
	INT bKeepTrying = 1;
	if ( CheckSlice(Location,Extent,bKeepTrying) )
		return true;
	else if ( !bKeepTrying )
		return false;

	// Try to fit half-slices
	Location = StartLoc;
	FVector SliceExtent = 0.5f * Extent;
	SliceExtent.Z = 1.f;
	INT NumFit = 0;
	for (INT i=-1;i<2;i+=2)
		for (INT j=-1;j<2;j+=2)
			if ( NumFit < 2 )
			{
				FVector SliceOffset = FVector(0.55f*Extent.X*i, 0.55f*Extent.Y*j, 0.f);
				if ( !EncroachingWorldGeometry(Hit,StartLoc+SliceOffset, SliceExtent, 0, GetLevelInfo()) )
				{
					NumFit++;
					if ( GIsEditor )
						Location += 1.1f * SliceOffset;
					else
						Location += SliceOffset;
				}
			}
	if ( NumFit == 0 )
		return false;

	// find full-sized slice to check
	if ( NumFit == 1 )
		Location = 2.f * Location - StartLoc;
	SingleLineCheck( Hit, NULL, Location, StartLoc, TRACE_World );
	if ( Hit.Actor )
		return false;

	if ( !EncroachingWorldGeometry(Hit,Location, Extent, 0, GetLevelInfo()) )
	{
		// adjust toward center
		SingleLineCheck( Hit, NULL, StartLoc + 0.2f * (StartLoc - Location), Location, TRACE_World, Extent );
		if ( Hit.Actor )
			Location = Hit.Location;
		return true;
	}
	return false;
	unguard;
}

//
// Try to place an actor that has moved a long way.  This is for
// moving actors through teleporters, adding them to levels, and
// starting them out in levels.  The results of this function is independent
// of the actor's current location and rotation.
//
// If the actor doesn't fit exactly in the location specified, tries
// to slightly move it out of walls and such.
//
// Returns 1 if the actor has been successfully moved, or 0 if it couldn't fit.
//
// Updates the actor's Zone and PhysicsVolume.
//
UBOOL ULevel::FarMoveActor( AActor* Actor, FVector DestLocation,  UBOOL test, UBOOL bNoCheck, UBOOL bAttachedMove )
{
	guard(ULevel::FarMoveActor);
	check(Actor!=NULL);
	if( (Actor->bStatic || !Actor->bMovable) && !GIsEditor )
		return 0;
	if ( test && (Actor->Location == DestLocation) )
		return 1;
	if( Actor->bCollideActors && Hash ) 
		Hash->RemoveActor( Actor );

    FVector prevLocation = Actor->Location;
	FVector newLocation = DestLocation;
	int result = 1;

	if (!bNoCheck && (Actor->bCollideWorld || (Actor->bCollideWhenPlacing && (GetLevelInfo()->NetMode != NM_Client))) ) 
		result = FindSpot( Actor->GetCylinderExtent(), newLocation );

	if (result && !test && !bNoCheck)
		result = !CheckEncroachment( Actor, newLocation, Actor->Rotation, 1);

    if( prevLocation != Actor->Location && !test ) // CheckEncroachment moved this actor (teleported), we're done
    {
        // todo: ensure the actor was placed back into the collision hash
        //debugf(TEXT("CheckEncroachment moved this actor, we're done!"));
        //farMoveStackCnt--;
        return result;
    }
	
	if( result )
	{
		//move based actors and remove base unles this farmove was done as a test
		if ( !test )
		{
			Actor->bJustTeleported = true;
			if ( !bAttachedMove )
				Actor->SetBase(NULL);
			for ( INT i=0; i<Actor->Attached.Num(); i++ )
				if ( Actor->Attached(i) && (Actor->Attached(i)->AttachmentBone == NAME_None) )
				{
					FarMoveActor(Actor->Attached(i),
						newLocation + Actor->Attached(i)->Location - Actor->Location,false,bNoCheck,true);
				}
		}

		// Bit nasty this, but just in case FarMoveActor has triggered an event that has
		// re-inserted the Actor into the Octree (eg. called FarMoveActor again), we make
		// sure the Actor is not in the Octree before changing its location.
		if(Actor->IsInOctree())
		{
			//debugf(TEXT("FarMoveActor: Actor (%s) still in Octree. Removing."), Actor->GetName());
			Hash->RemoveActor( Actor );
		}

		Actor->Location = newLocation;
	}

	if( Actor->bCollideActors && Hash ) 
		Hash->AddActor( Actor );

	// FIXME - setzone fast if actor attached to bone

	// Set the zone after moving, so that if a PhysicsVolumeChange or ActorEntered/ActorEntered message
	// tries to move the actor, the hashing will be correct.
	if( result )
		Actor->SetZone( test,0 );

	if ( !test )
	{	
		Actor->ClearRenderData();
	}

	return result;
	unguard;
}

//
// Tries to move the actor by a movement vector.  If no collision occurs, this function 
// just does a Location+=Move.
//
// Assumes that the actor's Location is valid and that the actor
// does fit in its current Location. Assumes that the level's 
// Dynamics member is locked, which will always be the case during
// a call to ULevel::Tick; if not locked, no actor-actor collision
// checking is performed.
//
// If bCollideWorld, checks collision with the world.
//
// For every actor-actor collision pair:
//
// If both have bCollideActors and bBlocksActors, performs collision
//    rebound, and dispatches Touch messages to touched-and-rebounded 
//    actors.  
//
// If both have bCollideActors but either one doesn't have bBlocksActors,
//    checks collision with other actors (but lets this actor 
//    interpenetrate), and dispatches Touch and UnTouch messages.
//
// Returns 1 if some movement occured, 0 if no movement occured.
//
// Updates actor's Zone and PhysicsVolume.
//
// If Test = 1 (default 0), do not send notifications.
//
UBOOL ULevel::MoveActor
(
	AActor*			Actor,
	FVector			Delta,
	FRotator		NewRotation,
	FCheckResult&	Hit,
	UBOOL			bTest,
	UBOOL			bIgnorePawns,
	UBOOL			bIgnoreBases,
	UBOOL			bNoFail
)
{
	guard(ULevel::MoveActor);
	check(Actor!=NULL);
	if( (Actor->bStatic || !Actor->bMovable) && !GIsEditor )
		return 0;

	UBOOL bRelevantAttachments = (Actor->Attached.Num() != 0);
	UBOOL bNoDelta = Delta.IsZero();

	if ( bRelevantAttachments
		&& (bNoDelta && Actor->GetPrimitive()->UseCylinderCollision(Actor)) || (!Actor->bCollideActors && !Actor->bCollideWorld)  )
	{
		bRelevantAttachments = false;
		for ( INT i=0; i<Actor->Attached.Num(); i++ )
			if ( Actor->Attached(i) && Actor->Attached(i)->AttachmentBone == NAME_None )
			{
				bRelevantAttachments = true;
				break;
			}
	}

	// Skip if no vector.
	if( bNoDelta )
	{
		if( NewRotation==Actor->Rotation )
		{
			return 1;
		}
		if( !bRelevantAttachments && Actor->GetPrimitive()->UseCylinderCollision(Actor) )
		{
			Actor->Rotation = NewRotation;
			if ( !bTest )
			{
				Actor->UpdateRelativeRotation();
				Actor->ClearRenderData();
			}
			return 1;
		}
	}
	if ( !bRelevantAttachments && !Actor->bCollideActors && !Actor->bCollideWorld )
	{
		Actor->Location += Delta;
		if ( !bTest )
		{
			Actor->ClearRenderData();
		}
		if ( NewRotation == Actor->Rotation )
			return 1;
		Actor->Rotation = NewRotation;
		if ( !bTest )
		{
			Actor->UpdateRelativeRotation();
		}
		return 1;
	}

	// Set up.

	GStats.DWORDStats(GEngineStats.STATS_Game_NumMoves)++;
	clock(GStats.DWORDStats(GEngineStats.STATS_Game_MoveCycles));
	FMemMark Mark(GMem);
	Hit = FCheckResult(1.f);
	FLOAT DeltaSize;
	FVector DeltaDir;
	if( Delta.IsNearlyZero() )
	{
		DeltaSize = 0;
		DeltaDir = Delta;
	}
	else
	{
		DeltaSize = Delta.Size();
		DeltaDir       = Delta/DeltaSize;
	}
	FLOAT TestAdjust	   = 2.f;
	FVector TestDelta      = Delta + TestAdjust * DeltaDir;
	INT     MaybeTouched   = 0;
	FCheckResult* FirstHit = NULL;

	UBOOL doEncroachTouch = 1;

	// Perform movement collision checking if needed for this actor.
	if((Actor->bCollideActors || Actor->bCollideWorld) && 
		!Actor->IsEncroacher() && 
		(DeltaSize != 0.f) )
	{
		doEncroachTouch = 0;

		// Check collision along the line.
		DWORD TraceFlags = 0;
		if ( Actor->bCollideActors )
		{
			if ( !bIgnorePawns )
				TraceFlags |= TRACE_Pawns;
			TraceFlags |= TRACE_Others | TRACE_Volumes;
		}
		if ( Actor->bCollideWorld )
			TraceFlags |= TRACE_World;
		FirstHit = MultiLineCheck
		(
			GMem,
			Actor->Location + TestDelta,
			Actor->Location,
			Actor->GetCylinderExtent(),
			Actor->bCollideWorld  ? GetLevelInfo() : NULL,
			TraceFlags,
			Actor
		);

		// Handle first blocking actor.
		if( Actor->bCollideWorld || Actor->bBlockActors )
		{
			for( FCheckResult* Test=FirstHit; Test; Test=Test->GetNext() )
			{
				if( (!bIgnoreBases || !Actor->IsBasedOn(Test->Actor)) && !Test->Actor->IsBasedOn(Actor) )
				{
					MaybeTouched = 1;
					if( Actor->IsBlockedBy(Test->Actor) )
					{
						Hit = *Test;
						break;
					}
				}
			}
		}
	}

	// Attenuate movement.
	FVector FinalDelta = Delta;
	if( Hit.Time < 1.f && !bNoFail )
	{
		// Fix up delta, given that TestDelta = Delta + TestAdjust.
		FLOAT FinalDeltaSize = (DeltaSize + TestAdjust) * Hit.Time;
		if ( FinalDeltaSize <= TestAdjust)
		{
			FinalDelta = FVector(0.f,0.f,0.f);
			Hit.Time = 0.f;
			if ( Actor->Rotation == NewRotation )
			{
				// move failed completely
		        unclock(GStats.DWORDStats(GEngineStats.STATS_Game_MoveCycles));
		        Mark.Pop();
		        return 0;
	        }
		}
		else 
		{
			FinalDelta = TestDelta * Hit.Time - TestAdjust * DeltaDir;
			Hit.Time   = (FinalDeltaSize - TestAdjust) / DeltaSize;
		}
	}

	// Abort if encroachment declined.
	if( !bTest && !bNoFail && Actor->IsEncroacher() && CheckEncroachment( Actor, Actor->Location + FinalDelta, NewRotation, doEncroachTouch ) )
	{
		unclock(GStats.DWORDStats(GEngineStats.STATS_Game_MoveCycles));
		Mark.Pop();
		return 0;
	}

	// Move the based actors (after encroachment checking).
	if( Actor->bCollideActors && Hash )
		Hash->RemoveActor( Actor );
	Actor->Location += FinalDelta;
	FRotator OldRotation = Actor->Rotation;
	Actor->Rotation  = NewRotation;

	if( (Actor->Attached.Num() > 0) && !bTest )
	{
		// Move base.
		FRotator ReducedRotation(0,0,0);
		FCoords Coords = GMath.UnitCoords / OldRotation;
		FCoords NewCoords = GMath.UnitCoords / NewRotation;
		if( OldRotation != Actor->Rotation )
			ReducedRotation = FRotator( ReduceAngle(Actor->Rotation.Pitch) - ReduceAngle(OldRotation.Pitch),
										ReduceAngle(Actor->Rotation.Yaw) - ReduceAngle(OldRotation.Yaw), 
										ReduceAngle(Actor->Rotation.Roll) - ReduceAngle(OldRotation.Roll) );

		// Calculate new transform matrix of base actor (ignoring scale).
		FMatrix BaseTM = FRotationMatrix(Actor->Rotation) * FTranslationMatrix(Actor->Location);

		for( INT i=0; i<Actor->Attached.Num(); i++ )
		{
			AActor* Other = Actor->Attached(i);
			if ( Other && !(Actor->Physics == PHYS_Karma && Other->bDetailAttachment && !Other->bCollideActors) )
			{
				FVector   RotMotion( 0, 0, 0 );
				FCheckResult OtherHit(1.f);

				if( Other->bHardAttach && !Other->bBlockActors ) 
				{
					FMatrix NewWorldTM = Other->HardRelMatrix * BaseTM;
					FVector NewWorldPos( NewWorldTM.M[3][0], NewWorldTM.M[3][1], NewWorldTM.M[3][2] );

#if 0
					// debug ref-frame drawing
					FVector xDir( NewWorldTM.M[0][0], NewWorldTM.M[0][1], NewWorldTM.M[0][2] );
					FVector yDir( NewWorldTM.M[1][0], NewWorldTM.M[1][1], NewWorldTM.M[1][2] );
					FVector zDir( NewWorldTM.M[2][0], NewWorldTM.M[2][1], NewWorldTM.M[2][2] );
					GTempLineBatcher->AddLine(NewWorldPos, NewWorldPos + (50 * xDir), FColor(255, 0, 0));
					GTempLineBatcher->AddLine(NewWorldPos, NewWorldPos + (50 * yDir), FColor(0, 255, 0));
					GTempLineBatcher->AddLine(NewWorldPos, NewWorldPos + (50 * zDir), FColor(0, 0, 255));
#endif

					FRotator NewWorldRot = (GMath.UnitCoords * NewWorldTM.Coords()).OrthoRotation();

					MoveActor( Other, NewWorldPos - Other->Location, NewWorldRot, OtherHit, 0, 0, 1 );
				}
				else if ( Other->AttachmentBone == NAME_None )
				{
					FRotator finalRotation = Other->Rotation + ReducedRotation;

					if( OldRotation != Actor->Rotation )
					{
						// update player view rotation
						APawn *P = Other->GetAPawn();
						FLOAT ControllerRoll = 0;
						if( P && P->Controller )
						{
							ControllerRoll = P->Controller->Rotation.Roll;
							P->Controller->Rotation += ReducedRotation;
						}

						// If its a pawn, and its not a crawler, remove roll.
						if( P && !P->bCrawler )
						{
							finalRotation.Roll = Other->Rotation.Roll;
							if( P->Controller )
								P->Controller->Rotation.Roll = ControllerRoll;
						}

						// Handle rotation-induced motion.
						RotMotion = NewCoords.XAxis * (Coords.XAxis | Other->RelativeLocation)
							        + NewCoords.YAxis * (Coords.YAxis | Other->RelativeLocation)
							        + NewCoords.ZAxis * (Coords.ZAxis | Other->RelativeLocation)
							        - Other->RelativeLocation;
					}

					// move attached actor
					MoveActor( Other, FinalDelta + RotMotion, finalRotation, OtherHit, 0, 0, 1 );
				}

				if ( !bNoFail && Other->IsBlockedBy(Actor) )
				{
					// check if encroached
					FCheckResult TestHit(1.f);
					UBOOL bStillEncroaching = Other->IsVolumeBrush() 
								? Other->GetPrimitive()->PointCheck( TestHit, Other, Actor->Location, Actor->GetCylinderExtent(), 0 )==0
								: Actor->GetPrimitive()->PointCheck( TestHit, Actor, Other->Location, Other->GetCylinderExtent(), 0 )==0;

					// if encroachment declined, move back to old location
					if ( bStillEncroaching && Actor->eventEncroachingOn(Other) )
					{
						Actor->Location -= FinalDelta;
						Actor->Rotation = OldRotation;
						if( Actor->bCollideActors && Hash )
							Hash->AddActor( Actor );
						for( INT j=0; j<Actor->Attached.Num(); j++ )
							if ( Actor->Attached(j) )
								MoveActor( Actor->Attached(j), -1.f * FinalDelta, Actor->Attached(j)->Rotation, OtherHit, 0, 0, 1 );
						Mark.Pop();
						unclock(GStats.DWORDStats(GEngineStats.STATS_Game_MoveCycles));
						return 0;
					}
				}
			}
		}
	}

	// Update the location.

	// update relative location of this actor
	if ( !bTest && !Actor->bOnlyDrawIfAttached )
	{
		if ( Actor->Base && (Actor->AttachmentBone == NAME_None) )
			Actor->RelativeLocation = Actor->Location - Actor->Base->Location;
		if ( OldRotation != Actor->Rotation )
			Actor->UpdateRelativeRotation();
	}
	if( Actor->bCollideActors && Hash )
		Hash->AddActor( Actor );

#ifdef WITH_KARMA
	// If this actor has a Karma Model, but has no dynamics body and it is not Static,
	// then update its transform from the Unreal actor.
	// This allows moving movers/players etc. to have Karma collision.
	
	McdModelID model = Actor->getKModel();
	if(model && !McdModelGetBody(model) && !Actor->bStatic)
	{
		MeMatrix4Ptr modelTM = McdModelGetTransformPtr(model);
		KU2METransform(modelTM, Actor->Location, Actor->Rotation);

		if(McdGeometryGetTypeId(McdModelGetGeometry(model)) != kMcdGeometryTypeNull)
			McdModelUpdate(model);
	}
#endif

	// Handle bump and touch notifications.
	if( !bTest )
	{
		// Notify first bumped actor unless it's the level or the actor's base.
		if( Hit.Actor && !Hit.Actor->bWorldGeometry && !Actor->IsBasedOn(Hit.Actor) )
		{
			// Notify both actors of the bump.
			Hit.Actor->NotifyBump(Actor);
			Actor->NotifyBump(Hit.Actor);
		}

		// Handle Touch notifications.
		if( MaybeTouched || !Actor->bBlockActors )
			for( FCheckResult* Test=FirstHit; Test && Test->Time<Hit.Time; Test=Test->GetNext() )
			{
				if
				(	(!bIgnoreBases || !Actor->IsJoinedTo(Test->Actor))
				&&	(!Actor->IsBlockedBy(Test->Actor)) 
				&&  Actor != Test->Actor)
					Actor->BeginTouch( Test->Actor );
			}

		// UnTouch notifications.
		for( int i=0; i<Actor->Touching.Num(); )
		{
			if( Actor->Touching(i) && !Actor->IsOverlapping(Actor->Touching(i)) )
				Actor->EndTouch( Actor->Touching(i), 0 );
			else
				i++;
		}
	}
	// Set actor zone.
	Actor->SetZone( bTest,0 );
	Mark.Pop();
	if ( !bTest )
	{
		Actor->ClearRenderData();
	}

	// Return whether we moved at all.
	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_MoveCycles));

	return Hit.Time>0.f;
	unguardf((TEXT("%s "),Actor->GetName()));
}

void AActor::NotifyBump(AActor *Other)
{
	eventBump(Other);
}

void APawn::NotifyBump(AActor *Other)
{
	guardSlow(APawn::NotifyBump);
	if ( !Controller || !Controller->eventNotifyBump(Other) )
		eventBump(Other);
	unguardSlow;
}
/*-----------------------------------------------------------------------------
	Encroachment.
-----------------------------------------------------------------------------*/

//
// Check whether Actor is encroaching other actors after a move, and return
// 0 to ok the move, or 1 to abort it.
//
UBOOL ULevel::CheckEncroachment
(
	AActor*		Actor,
	FVector		TestLocation,
	FRotator	TestRotation,
	UBOOL		bTouchNotify
)
{
	guard(ULevel::CheckEncroachment);
	check(Actor);

	// If this actor doesn't need encroachment checking, allow the move.
	if( !Actor->bCollideActors && 
		!Actor->bBlockActors && 
		!Actor->IsEncroacher() )
		return 0;

	// set up matrices for calculating movement caused by mover rotation
	FMatrix WorldToLocal, TestLocalToWorld;
	if ( Actor->IsEncroacher() )
	{
		WorldToLocal = Actor->WorldToLocal();
		FVector RealLocation = Actor->Location;
		FRotator RealRotation = Actor->Rotation;
		Actor->Location = TestLocation;
		Actor->Rotation = TestRotation;
		TestLocalToWorld = Actor->LocalToWorld();
		Actor->Location = RealLocation;
		Actor->Rotation = RealRotation;
	}

	// Query the mover about what he wants to do with the actors he is encroaching.
	FMemMark Mark(GMem);
	FCheckResult* FirstHit = Hash ? Hash->ActorEncroachmentCheck( GMem, Actor, TestLocation, TestRotation, TRACE_AllColliding, 0 ) : NULL;	
	for( FCheckResult* Test = FirstHit; Test!=NULL; Test=Test->GetNext() )
	{
		if
		(	Test->Actor!=Actor
		&&	Test->Actor!=GetLevelInfo()
		&&  !Test->Actor->IsJoinedTo(Actor)
		&&	Actor->IsBlockedBy( Test->Actor ) 
		&& !Test->Actor->IsA(AStaticMeshActor::StaticClass()) )
		{
			UBOOL bStillEncroaching = true;
			// Actors can be pushed by movers or karma stuff.
			if ( Actor->IsEncroacher() && !Test->Actor->IsEncroacher() )
			{
				// check if mover can safely push encroached actor
				// Move test actor away from mover
				FVector MoveDir = TestLocation - Actor->Location;
				FVector OldLoc = Test->Actor->Location;
				FVector Dest = Test->Actor->Location + MoveDir;
				if ( TestRotation != Actor->Rotation )
				{
					FVector TestLocalLoc = WorldToLocal.TransformFVector(Test->Actor->Location);
					// multiply X 1.5 to account for max bounding box center to colliding edge dist change
					MoveDir += 1.5f * (TestLocalToWorld.TransformFVector(TestLocalLoc) - Test->Actor->Location); 
				}
				Test->Actor->moveSmooth(MoveDir);

				// see if mover still encroaches test actor
				// Save actor's location and rotation.
				Exchange( TestLocation, Actor->Location );
				Exchange( TestRotation, Actor->Rotation );

				FCheckResult TestHit(1.f);
				bStillEncroaching = Test->Actor->IsVolumeBrush() 
							? Test->Actor->GetPrimitive()->PointCheck( TestHit, Test->Actor, Actor->Location, Actor->GetCylinderExtent(), 0 )==0
							: Actor->GetPrimitive()->PointCheck( TestHit, Actor, Test->Actor->Location, Test->Actor->GetCylinderExtent(), 0 )==0;

				// Restore actor's location and rotation.
				Exchange( TestLocation, Actor->Location );
				Exchange( TestRotation, Actor->Rotation );

				if ( !bStillEncroaching && (MoveDir.SizeSquared() > 4.f) ) //push test actor back toward brush
				{
					FVector realLoc = Actor->Location;
					Actor->Location = TestLocation;
					MoveActor( Test->Actor, -0.5f * MoveDir, Test->Actor->Rotation, TestHit );
					Actor->Location = realLoc;
				}
			}
			if ( bStillEncroaching && Actor->eventEncroachingOn(Test->Actor) )
			{
				Mark.Pop();
				return 1;
			}
			else 
				Actor->eventRanInto(Test->Actor);
		}
	}

	// If bTouchNotify, send Touch and UnTouch notifies.
	if( bTouchNotify )
	{
		// UnTouch notifications.
		for( int i=0; i<Actor->Touching.Num(); )
		{
			if( Actor->Touching(i) && !Actor->IsOverlapping(Actor->Touching(i)) )
				Actor->EndTouch( Actor->Touching(i), 0 );
			else
				i++;
		}
	}

	// Notify the encroached actors but not the level.
	for( FCheckResult* Test = FirstHit; Test; Test=Test->GetNext() )
		if
		(	Test->Actor!=Actor
		&&  !Test->Actor->IsJoinedTo(Actor)
		&&	Test->Actor!=GetLevelInfo() )
		{ 
			if( Actor->IsBlockedBy(Test->Actor) ) 
				Test->Actor->eventEncroachedBy(Actor);
			else if( bTouchNotify )
				Actor->BeginTouch( Test->Actor );
		}
							
	Mark.Pop();


	// Ok the move.
	return 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	SinglePointCheck.
-----------------------------------------------------------------------------*/

//
// Check for nearest hit.
// Return 1 if no hit, 0 if hit.
//
UBOOL ULevel::SinglePointCheck
(
	FCheckResult&	Hit,
	const FVector&	Location,
	const FVector&	Extent,
	DWORD			ExtraNodeFlags,
	ALevelInfo*		Level,
	UBOOL			bActors
)
{
	guard(ULevel::SinglePointCheck);
	FMemMark Mark(GMem);
	FCheckResult* Hits = MultiPointCheck( GMem, Location, Extent, ExtraNodeFlags, Level, bActors );
	if( !Hits )
	{
		Mark.Pop();
		return 1;
	}
	Hit = *Hits;
	for( Hits = Hits->GetNext(); Hits!=NULL; Hits = Hits->GetNext() )
		if( (Hits->Location-Location).SizeSquared() < (Hit.Location-Location).SizeSquared() )
			Hit = *Hits;
	Mark.Pop();
	return 0;
	unguard;
}

/* 
  EncroachingWorldGeometry
  return true if Extent encroaches on level, terrain, or bWorldGeometry actors
*/
UBOOL ULevel::EncroachingWorldGeometry
(
	FCheckResult&	Hit,
	FVector			Location,
	FVector			Extent,
	DWORD			ExtraNodeFlags,
	ALevelInfo*		Level
)
{
	guard(ULevel::EncroachingWorldGeometry);
	FMemMark Mark(GMem);
	FCheckResult* Hits = MultiPointCheck( GMem, Location, Extent, ExtraNodeFlags, Level, true, true, true );
	if ( !Hits )
	{
		Mark.Pop();
		return false;
	}
	Hit = *Hits;
	Mark.Pop();
	return true;
	unguard;
}

/*-----------------------------------------------------------------------------
	MultiPointCheck.
-----------------------------------------------------------------------------*/

FCheckResult* ULevel::MultiPointCheck( FMemStack& Mem, const FVector& Location, const FVector& Extent, DWORD ExtraNodeFlags, ALevelInfo* Level, UBOOL bActors, UBOOL bOnlyWorldGeometry, UBOOL bSingleResult )
{
	guard(ULevel::MultiPointCheck);
	FCheckResult* Result=NULL;

	if(this->bShowPointChecks && GTempLineBatcher)
	{
		// Draw box showing extent of point check.
		GTempLineBatcher->AddBox(FBox(Location-Extent, Location+Extent), FColor(0, 128, 255));
	}

	INT		StartCycles = appCycles();

	// Check with level.
	if( Level )
	{
		FCheckResult TestHit(1.f);
		if( Level->GetLevel()->Model->PointCheck( TestHit, NULL, Location, Extent, 0 )==0 )
		{
			// Hit.
			TestHit.GetNext() = Result;
			Result            = new(Mem)FCheckResult(TestHit);
			Result->Actor     = Level;
			if ( bSingleResult )
			{
				GStats.DWORDStats(GEngineStats.STATS_Game_MPCheckCycles) += (appCycles() - StartCycles);
				return Result;
		}
	}
	}

	// check with terrain.
	guard(CheckWithTerrain);
	for( INT z=0;z<64;z++ )
	{
		AZoneInfo* Z = GetZoneActor(z);
		if( Z && Z->bTerrainZone ) 
		{
			for( INT t=0;t<Z->Terrains.Num();t++ )
			{
				FCheckResult	TestHit(1.0f);

				if( Z->Terrains(t)->PointCheck( TestHit, Location, Extent )==0 )
				{
					FPointRegion HitRegion = Model->PointRegion( GetLevelInfo(), TestHit.Location );
					if( HitRegion.Zone == Z )
					{
						TestHit.GetNext() = Result;
						Result            = new(Mem)FCheckResult(TestHit);
						Result->Actor	  = Z->Terrains(t);
						if( bSingleResult )
						{
							GStats.DWORDStats(GEngineStats.STATS_Game_MPCheckCycles) += (appCycles() - StartCycles);
							return Result;
					}
				}
			}
		}
	}
	}
	unguard;

	// Check with actors.
	if( bActors && Hash )
		Result = Hash->ActorPointCheck( Mem, Location, Extent, 
							bOnlyWorldGeometry ? TRACE_World : TRACE_AllColliding,
							ExtraNodeFlags, bSingleResult );

	GStats.DWORDStats(GEngineStats.STATS_Game_MPCheckCycles) += (appCycles() - StartCycles);
	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	SingleLineCheck.
-----------------------------------------------------------------------------*/

//
// Trace a line and return the first hit actor (Actor->bWorldGeometry means hit the world geomtry).
//
UBOOL ULevel::SingleLineCheck
(
	FCheckResult&	Hit,
	AActor*			SourceActor,
	const FVector&	End,
	const FVector&	Start,
	DWORD           TraceFlags,
	FVector			Extent
)
{
	guard(ULevel::SingleLineCheck);

	// Get list of hit actors.
	FMemMark Mark(GMem);
    
 	TraceFlags = TraceFlags | TRACE_SingleResult; 
	FCheckResult* FirstHit = MultiLineCheck
	(
		GMem,
		End,
		Start,
		Extent,
		(TraceFlags&TRACE_Level) ? GetLevelInfo() : NULL,
		TraceFlags,
		SourceActor
	);

	FCheckResult* Check;
	for( Check = FirstHit; Check!=NULL; Check=Check->GetNext() )
	{
		// Skip skybox geometry when raytracing for shadows
		if( (Check->Actor==GetLevelInfo()) && (TraceFlags&TRACE_ShadowCast) && (Model->Surfs(Model->Nodes(Check->Item).iSurf).PolyFlags&PF_FakeBackdrop) )
			continue;

		break;
	}
	if( Check )
	{
		Hit = *Check;
	}
	else
	{
		Hit.Time = 1.f;
		Hit.Actor = NULL;
	}

	Mark.Pop();
	return Check==NULL;
	unguard;
}

/*-----------------------------------------------------------------------------
	MultiLineCheck.
-----------------------------------------------------------------------------*/

FCheckResult* ULevel::MultiLineCheck
(
	FMemStack&		Mem,
	const FVector&	OriginalEnd,
	const FVector&	Start,
	const FVector&	Extent,
	ALevelInfo*		LevelInfo,
	DWORD			TraceFlags,
	AActor*			SourceActor
)
{
	guard(ULevel::MultiLineCheck);
	INT NumHits=0;
	FCheckResult Hits[64];

    if( GetFlags() & RF_Unreachable ) // sjs
        return NULL;

	FVector End = OriginalEnd;

	// Draw line that we are checking, and box showing extent at end of line, if non-zero
	if(this->bShowLineChecks && Extent.IsZero() && GTempLineBatcher)
	{
		GTempLineBatcher->AddLine(Start, End, FColor(0, 255, 128));
		
	}
	else if(this->bShowExtentLineChecks && !Extent.IsZero() && GTempLineBatcher)
	{
		GTempLineBatcher->AddLine(Start, End, FColor(0, 255, 255));
		GTempLineBatcher->AddBox(FBox(End-Extent, End+Extent), FColor(0, 255, 255));
	}

	INT		StartCycles = appCycles();
	FLOAT	Dilation	= 1.f;

	// Check for collision with the level, and cull by the end point for speed.
	INT WorldNum = 0;
	guard(CheckWithLevel);
	if( (TraceFlags & TRACE_Level) && LevelInfo && LevelInfo->GetLevel()->Model->LineCheck( Hits[NumHits], NULL, End, Start, Extent, 0, TraceFlags )==0 )
	{
		Hits[NumHits].Actor = LevelInfo;
		FLOAT Dist = (Hits[NumHits].Location - Start).Size();
		Hits[NumHits].Time *= Dilation;
		Dilation = ::Min(1.f, Hits[NumHits].Time * (Dist + 5)/(Dist+0.0001f));
		End = Start + (OriginalEnd - Start) * Dilation;
		WorldNum = NumHits;
		NumHits++;
	}
	unguard;
	if ( NumHits && (TraceFlags & TRACE_StopAtFirstHit) )
		goto SortList;

	// check with terrain.
	guard(CheckWithTerrain);
	// Terrain self-shadowing is kinda slow and doesn't look that good.
	if ( (TraceFlags & TRACE_Level) )// sjs - terrain self-shadowing looks good if you smooth the colors! && !(TraceFlags&TRACE_ShadowCast) )
	{
		for( INT z=0;z<64;z++ )
		{
			AZoneInfo* Z = GetZoneActor(z);
			if( Z && Z->bTerrainZone ) 
			{
				for( INT t=0;t<Z->Terrains.Num();t++ )
				{
                    if( ((TraceFlags&TRACE_ShadowCast) && !Z->Terrains(t)->bShadowCast) )
                        continue;
					if( Z->Terrains(t)->LineCheck( Hits[NumHits], End, Start, Extent, TraceFlags, 0 )==0 )
					{
						FPointRegion HitRegion = Model->PointRegion( GetLevelInfo(), Hits[NumHits].Location );
						if( HitRegion.Zone == Z )
						{
							Hits[NumHits].Actor = Z->Terrains(t);
							Hits[NumHits].Time *= Dilation;
							FLOAT Dist = (Hits[NumHits].Location - Start).Size();
							Dilation = ::Min(1.f, Hits[NumHits].Time * (Dist + 20.f)/(Dist+0.0001f));
							End = Start + (OriginalEnd - Start) * Dilation;
							NumHits++;
						}
					}
				}
			}
		}
	}
	unguard;
	if ( NumHits && (TraceFlags & TRACE_StopAtFirstHit) )
		goto SortList;

	// Check with actors.
	guard(CheckWithActors);
	if( (TraceFlags & TRACE_Hash) && Hash )
	{
		for( FCheckResult* Link=Hash->ActorLineCheck( Mem, End, Start, Extent, TraceFlags, 0, SourceActor ); Link && NumHits<ARRAY_COUNT(Hits); Link=Link->GetNext() )
		{
			Link->Time *= Dilation;
			Hits[NumHits++] = *Link;
		}
	}
	unguard;

	// Sort the list.
SortList:
	FCheckResult* Result = NULL;
	if( NumHits )
	{
		appQsort( Hits, NumHits, sizeof(Hits[0]), (QSORT_COMPARE)CompareHits );
		Result = new(Mem,NumHits)FCheckResult;
		for( INT i=0; i<NumHits; i++ )
		{
			Result[i]      = Hits[i];
			Result[i].Next = (i+1<NumHits) ? &Result[i+1] : NULL;
		}
	}

	GStats.DWORDStats(GEngineStats.STATS_Game_MLCheckCycles) += (appCycles() - StartCycles);

	return Result;
	unguard;
}

/*-----------------------------------------------------------------------------
	ULevel zone functions.
-----------------------------------------------------------------------------*/

//
// Figure out which zone an actor is in, update the actor's iZone,
// and notify the actor of the zone change.  Skips the zone notification
// if the zone hasn't changed.
//

void AActor::SetZone( UBOOL bTest, UBOOL bForceRefresh )
{
	guard(AActor::SetZone);

	if( bDeleteMe )
		return;

	// If refreshing, init the actor's current zone.
	if( bForceRefresh )
	{
		// Init the actor's zone.
		Region = FPointRegion(Level);
	}

	// Find zone based on actor's location and see if it has changed.
	FPointRegion	NewRegion = GetLevel()->Model->PointRegion( Level, Location );

	if( NewRegion.Zone!=Region.Zone )
	{
		// Notify old zone info of actor leaving.
		if( !bTest )
		{
			Region.Zone->eventActorLeaving(this);
			eventZoneChange( NewRegion.Zone );
		}
		Region = NewRegion;
		if( !bTest )
			Region.Zone->eventActorEntered(this);
	}
	else Region = NewRegion;

	// update physics volume
	APhysicsVolume *NewVolume = Level->GetPhysicsVolume(Location,this,bCollideActors && !bTest && !bForceRefresh);
	if ( !bTest )
	{
		if ( NewVolume != PhysicsVolume )
		{
			if ( PhysicsVolume )
			{
				PhysicsVolume->eventActorLeavingVolume(this);
				eventPhysicsVolumeChange(NewVolume);
			}
			PhysicsVolume = NewVolume;
			PhysicsVolume->eventActorEnteredVolume(this);
		}
	}
	else
		PhysicsVolume = NewVolume;
	checkSlow(Region.Zone!=NULL);
	unguard;
}

void APhysicsVolume::SetZone( UBOOL bTest, UBOOL bForceRefresh )
{
	guard(APhysicsVolume::SetZone);

	if( bDeleteMe )
		return;

	// If refreshing, init the actor's current zone.
	if( bForceRefresh )
	{
		// Init the actor's zone.
		Region = FPointRegion(Level);
	}

	// Find zone based on actor's location and see if it has changed.
	FPointRegion	NewRegion = GetLevel()->Model->PointRegion( Level, Location );

	if( NewRegion.Zone!=Region.Zone )
	{
		// Notify old zone info of actor leaving.
		if( !bTest )
		{
			Region.Zone->eventActorLeaving(this);
			eventZoneChange( NewRegion.Zone );
		}
		Region = NewRegion;
		if( !bTest )
			Region.Zone->eventActorEntered(this);
	}
	else Region = NewRegion;

	PhysicsVolume = this;
	checkSlow(Region.Zone!=NULL);
	unguard;
}

void APawn::SetZone( UBOOL bTest, UBOOL bForceRefresh )
{
	guard(APawn::SetZone);

	if( bDeleteMe )
		return;

	// If refreshing, init the actor's current zone.
	if( bForceRefresh )
	{
		// Init the actor's zone.
		Region = FPointRegion(Level);
	}

	// Find zone based on actor's location and see if it has changed.
	FPointRegion NewRegion = GetLevel()->Model->PointRegion( Level, Location );

	if( NewRegion.Zone!=Region.Zone )
	{
		// Notify old zone info of player leaving.
		if( !bTest )
		{
			Region.Zone->eventActorLeaving(this);
			eventZoneChange( NewRegion.Zone );
		}
		Region = NewRegion;
		if( !bTest )
			Region.Zone->eventActorEntered(this);
	}
	else Region = NewRegion;

	// update physics volume
	APhysicsVolume *NewVolume = Level->GetPhysicsVolume(Location,this,bCollideActors && !bTest && !bForceRefresh);
	APhysicsVolume *NewHeadVolume = Level->GetPhysicsVolume(Location + FVector(0,0,BaseEyeHeight),this,bCollideActors && !bTest && !bForceRefresh);
	if ( NewVolume != PhysicsVolume )
	{
		if ( !bTest )
		{
			if ( PhysicsVolume )
			{
				PhysicsVolume->eventPawnLeavingVolume(this);
				eventPhysicsVolumeChange(NewVolume);
			}
			if ( Controller )
				Controller->eventNotifyPhysicsVolumeChange( NewVolume );
		}
		PhysicsVolume = NewVolume;
		if ( !bTest )
			PhysicsVolume->eventPawnEnteredVolume(this);
	}
	if ( NewHeadVolume != HeadVolume )
	{
		if ( !bTest && (!Controller || !Controller->eventNotifyHeadVolumeChange(NewHeadVolume)) )
			eventHeadVolumeChange(NewHeadVolume);
		HeadVolume = NewHeadVolume;
	}
	checkSlow(PhysicsVolume);
	checkSlow(Region.Zone!=NULL);
	unguard;
}

void ALevelInfo::SetZone( UBOOL bTest, UBOOL bForceRefresh )
{
	guard(ALevelInfo::SetZone);

	if( bDeleteMe )
		return;

	// handle levelinfo specially.
	Region = FPointRegion( Level );
	unguard;
}

void ALevelInfo::InitDistanceFogLOD()
{
	guard(ALevelInfo::InitDistanceFogLOD);
	for( INT i=0; i<XLevel->Actors.Num(); i++ )
	{
		AZoneInfo* Z = Cast<AZoneInfo>(XLevel->Actors(i));
		if( Z )
			Z->RealDistanceFogEnd = Z->DistanceFogEnd;
	}
	unguard;
}

void ALevelInfo::UpdateDistanceFogLOD( FLOAT DistanceFogLOD )
{
	guard(ALevelInfo::UpdateDistanceFogLOD);
	if( GIsEditor || !GIsClient || GIsUCC || (NetMode==NM_DedicatedServer) || (NetMode==NM_ListenServer) )
		return;

	DistanceFogLOD = Clamp<FLOAT>( DistanceFogLOD, 0.f, 1.f );

	for( INT i=0; i<XLevel->Actors.Num(); i++ )
	{
		AZoneInfo* Z = Cast<AZoneInfo>(XLevel->Actors(i));
		if( Z )
		{
			FLOAT RealDistanceFogEndMin = Z->DistanceFogEndMin > 0.f ? Z->DistanceFogEndMin : Z->RealDistanceFogEnd;
			Z->DistanceFogEnd = Lerp( RealDistanceFogEndMin, Z->RealDistanceFogEnd, DistanceFogLOD );
		}
	}
	unguard;
}

void AActor::SetVolumes(const TArray<AVolume*>& Volumes)
{
	guard(AActor::SetVolumes);
	for( INT i=0; i<Volumes.Num(); i++ )
	{
		AVolume*		V = Volumes(i);
		APhysicsVolume*	P = Cast<APhysicsVolume>(V);
		
		if( (bCollideActors && V->bCollideActors || P) && V->Encompasses(Location) )
		{
			if( bCollideActors && V->bCollideActors )
			{
				V->Touching.AddItem(this);
				Touching.AddItem(V);
			}
			if( P && (P->Priority > PhysicsVolume->Priority) )
			{
				PhysicsVolume = P;
			}
		}
	}
	unguard;
}

// init actor volumes
void AActor::SetVolumes()
{
	guard(AActor::SetVolumes);
	for( INT i=0; i<GetLevel()->Actors.Num(); i++ )
	{
		AVolume*		V = Cast<AVolume>(GetLevel()->Actors(i));
		APhysicsVolume*	P = Cast<APhysicsVolume>(V);
		
		if( V && (bCollideActors && V->bCollideActors || P) && V->Encompasses(Location) )
		{
			if( bCollideActors && V->bCollideActors )
			{
				V->Touching.AddItem(this);
				Touching.AddItem(V);
			}
			if( P && (P->Priority > PhysicsVolume->Priority) )
			{
				PhysicsVolume = P;
			}
		}
	}
	unguard;
}

// Allow actors to initialize themselves on the C++ side
void AActor::PostBeginPlay()
{
}

void AVolume::SetVolumes()
{
}

void AVolume::SetVolumes(const TArray<class AVolume*>& Volumes)
{
}

void ALevelInfo::SetVolumes()
{
}

void ALevelInfo::SetVolumes(const TArray<class AVolume*>& Volumes)
{
}

APhysicsVolume* ALevelInfo::GetDefaultPhysicsVolume()
{
	guard(ALevelInfo::GetDefaultPhysicsVolume);

	if ( !PhysicsVolume )
	{
		PhysicsVolume = Cast<APhysicsVolume>(GetLevel()->SpawnActor(ADefaultPhysicsVolume::StaticClass()));
		check(PhysicsVolume);
		PhysicsVolume->Priority = -1000000;
		PhysicsVolume->bNoDelete = true;
	}
	return PhysicsVolume;
	unguard;
}

APhysicsVolume* ALevelInfo::GetPhysicsVolume(FVector Loc, AActor* A, UBOOL bUseTouch)
{
	guard(ALevelInfo::GetPhysicsVolume);
	APhysicsVolume *NewVolume = Level->GetDefaultPhysicsVolume();

	if ( A )
	{
		if ( A->Base && (A->AttachmentBone != NAME_None) && Cast<USkeletalMesh>(A->Base->Mesh) )
			return A->Base->PhysicsVolume ? A->Base->PhysicsVolume : NewVolume;
		if ( bUseTouch )
		{
			for ( INT i=0; i<A->Touching.Num(); i++ )
			{
				APhysicsVolume *V = Cast<APhysicsVolume>(A->Touching(i));
				if ( V && (V->Priority > NewVolume->Priority) && V->Encompasses(Loc) )
					NewVolume = V;
			}
			return NewVolume;
		}
	}

	if ( GetLevel()->Hash )
	{
		FMemMark Mark(GMem);

		for( FCheckResult* Link=GetLevel()->Hash->ActorPointCheck( GMem, Loc, FVector(0.f,0.f,0.f), TRACE_Volumes, 0, 0); Link; Link=Link->GetNext() )
		{
			APhysicsVolume *V = Cast<APhysicsVolume>(Link->Actor);
			if ( V && (V->Priority > NewVolume->Priority) )
				NewVolume = V;
		}
		Mark.Pop();
	}
	return NewVolume;
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

