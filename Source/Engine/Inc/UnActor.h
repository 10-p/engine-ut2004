/*=============================================================================
	UnActor.h: AActor class inlines.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
        * Aug 30, 1996: Mark added PLevel
		* Oct 19, 1996: Tim redesigned to eliminate redundency
=============================================================================*/

/*-----------------------------------------------------------------------------
	FActorLink.
-----------------------------------------------------------------------------*/

//
// Linked list of actors.
//
struct FActorLink
{
	// Variables.
	AActor*     Actor;
	FActorLink* Next;

	// Functions.
	FActorLink( AActor* InActor, FActorLink* InNext )
	: Actor(InActor), Next(InNext)
	{}
};

/*-----------------------------------------------------------------------------
	AActor inlines.
-----------------------------------------------------------------------------*/

//
// Brush checking.
//
inline UBOOL AActor::IsBrush()       const {return Brush!=NULL && ((AActor *)this)->IsABrush();}
inline UBOOL AActor::IsStaticBrush() const {return Brush!=NULL && ((AActor *)this)->IsABrush() && bStatic && !((AActor *)this)->IsAVolume(); }
inline UBOOL AActor::IsMovingBrush() const {return StaticMesh!=NULL && ((AActor *)this)->IsAMover();}
inline UBOOL AActor::IsVolumeBrush() const {return Brush!=NULL && ((AActor *)this)->IsAVolume();}
inline UBOOL AActor::IsEncroacher()  const {return bCollideActors && 
											(IsMovingBrush() || Physics==PHYS_KarmaRagDoll || (Physics==PHYS_Karma && !IsA(AKConstraint::StaticClass())));}

//
// See if this actor is owned by TestOwner.
//
inline UBOOL AActor::IsOwnedBy( const AActor* TestOwner ) const
{
	guardSlow(AActor::IsOwnedBy);
	for( const AActor* Arg=this; Arg; Arg=Arg->Owner )
	{
		if( Arg == TestOwner )
			return 1;
	}
	return 0;
	unguardSlow;
}

//
// Get the top-level owner of an actor.
//
inline AActor* AActor::GetTopOwner()
{
	AActor* Top;
	for( Top=this; Top->Owner; Top=Top->Owner );
	return Top;
}


//
// See if this actor is in the specified zone.
//
inline UBOOL AActor::IsInZone( const AZoneInfo* TestZone ) const
{
	return Region.Zone!=Level ? Region.Zone==TestZone : 1;
}

//
// If this actor is a player, return it as a Pawn.
// Otherwise return NULL.
//
inline APawn* AActor::GetPlayerPawn() const
{
	guardSlow(AActor::GetPlayerPawn);

	// Only descendents of the pawn class may be players.
	if( !((AActor *)this)->GetAPawn() )
		return NULL;

	// Players must have player objects.
    AController* Ctrl = ((APawn*)this)->Controller;
    APlayerController* C = Ctrl ? Ctrl->GetAPlayerController() : NULL;//Cast<APlayerController>(((APawn*)this)->Controller);
	if( !C || (C->Player == NULL) )
		return NULL;

	// This is a player.
	return (APawn*)this;

	unguardSlow;
}


//
// Return whether this actor looks like a player.
// Note - it could be a bot, so it may not have a viewport
//
inline UBOOL AActor::IsPlayer() const
{
	guardSlow(AActor::IsPlayer);

	// Only descendents of the pawn class may be players.
	if( !((AActor *)this)->GetAPawn() || !((APawn *)this)->Controller )
		return 0;

	return ((APawn*)this)->Controller->bIsPlayer;

	unguardSlow;
}

//
// Return whether this actor's movement is based on another actor.
//
inline UBOOL AActor::IsBasedOn( const AActor* Other ) const
{
	guard(AActor::IsBasedOn);
	for( const AActor* Test=this; Test!=NULL; Test=Test->Base )
		if( Test == Other )
			return 1;
	return 0;
	unguard;
}

//
// Return the level of an actor.
//
inline class ULevel* AActor::GetLevel() const
{
	return XLevel;
}

//
// Return the actor's view rotation.
//
inline FRotator AActor::GetViewRotation()
{
	guardSlow(AActor::GetViewRotation);
	return (((AActor *)this)->GetAPawn() && ((APawn*)this)->Controller) ? ((APawn*)this)->Controller->Rotation : Rotation;
	unguardSlow;
}

/*-----------------------------------------------------------------------------
	AActor audio.
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

