/*=============================================================================
	UnVehicle.cpp: Vehicle physics implementation

	Copyright 2000-2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Steven Polge 7/00
=============================================================================*/

#include "EnginePrivate.h"


static FLOAT	 SavedWheelsScale;

void AVehicle::setMoveTimer(FVector MoveDir)
{
	guard(AVehicle::setMoveTimer);

	if ( !Controller )
		return;

	Super::setMoveTimer(MoveDir);
	Controller->MoveTimer += 2.f;
	if ( (MoveDir | Rotation.Vector()) < 0.f )
		Controller->MoveTimer += 2.5f;
	unguard;
}

void AVehicle::PreNetReceive()
{
	guard(AVehicle::PreNetReceive);

	SavedWheelsScale = WheelsScale;

	Super::PreNetReceive();

	unguard;
}

void AVehicle::PostNetReceive()
{
	guard(AVehicle::PostNetReceive);

	Super::PostNetReceive();

	if( Team != OldTeam )
	{
		eventTeamChanged();
		OldTeam = Team;
	}

	if (bDriving != bOldDriving)
	{
		eventDrivingStatusChanged();
		bOldDriving = bDriving;
	}

	if( SavedWheelsScale != WheelsScale )
		eventSetWheelsScale(WheelsScale);

	unguard;
}

UBOOL AVehicle::IsStuck()
{
	guardSlow(AVehicle::IsStuck);

	if ( Level->TimeSeconds - StuckTime < 1.f )
		return true;
	if ( (Velocity.SizeSquared() > 100.f) || (Level->TimeSeconds - ThrottleTime < 1.f) )
	{
		StuckCount = 0;
		return false;
	}

	StuckCount++;
	StuckTime = Level->TimeSeconds;
	return true;
	unguardSlow;
}

UBOOL AVehicle::moveToward(const FVector &Dest, AActor *GoalActor )
{
	guard(AVehicle::moveToward);

	if ( !Controller )
		return false;

	UBOOL bFlyingDown = false;
	FVector AdjustedDest = Dest;

	if ( GoalActor && Controller->CurrentPath )
	{
		if ( Cast<AFlyingPathNode>(GoalActor) )
		{
			if ( !Cast<AFlyingPathNode>(Controller->CurrentPath->Start) )
			{
				// if not inside flying pathnode, just move straight up toward it if it's above me
				FVector Dir = GoalActor->Location - Location;
				if ( Dir.Z > GoalActor->CollisionHeight )
				{
					Dir.Z = 0.f;
					if ( Dir.SizeSquared() < GoalActor->CollisionRadius * GoalActor->CollisionRadius )
					{
						AdjustedDest = Location;
						AdjustedDest.Z = GoalActor->Location.Z;
					}
				}
			}
		}
		else if ( Cast<AFlyingPathNode>(Controller->CurrentPath->Start) )
		{
			// if inside flying pathnode, just move straight across toward it if it's below me
			FVector Dir = Controller->CurrentPath->Start->Location - Dest;
			if ( Dir.Z > Controller->CurrentPath->Start->CollisionHeight )
			{
				Dir.Z = 0.f;
				if ( (Dir.SizeSquared() < Controller->CurrentPath->Start->CollisionRadius * Controller->CurrentPath->Start->CollisionRadius)
					&& (Dir.SizeSquared() > ::Max(40000.f ,GoalActor->CollisionRadius * GoalActor->CollisionRadius)) )
				{
					AdjustedDest = Dest;
					if ( Location.Z < Controller->CurrentPath->Start->Location.Z )
						AdjustedDest.Z = Location.Z - 0.7f*CollisionHeight;
					if ( ReachedDestination(AdjustedDest - Location, GoalActor) )
						AdjustedDest = Dest;
				}
			}
			bFlyingDown = true;
		}
		// check if on next path already - FIXME - want this for regular pawns as well! - FIXME - also should work when no currentpath (moving to initial path on network)
		else if ( Controller->NextRoutePath )
		{
			FVector NextPathDir = Controller->NextRoutePath->End->Location - Controller->NextRoutePath->Start->Location;
			// see if location is between start and end
			if ( (((Location - Controller->NextRoutePath->Start->Location) | NextPathDir) > 0.f)
				&& (((Location - Controller->NextRoutePath->End->Location) | NextPathDir) < 0.f) )
			{
				// check distance to line
				NextPathDir = NextPathDir.SafeNormal();
				FVector Start = Controller->NextRoutePath->Start->Location;
				FVector ClosestPoint = Start + (NextPathDir | (Location - Start)) * NextPathDir;
				FVector LineDir = Location - ClosestPoint;
				if ( LineDir.SizeSquared() < Controller->NextRoutePath->CollisionRadius*Controller->NextRoutePath->CollisionRadius )
				{
					if ( Controller->Focus == Controller->MoveTarget )
						Controller->Focus = Controller->NextRoutePath->End;
					Controller->MoveTarget = Controller->NextRoutePath->End;
					GoalActor = Controller->MoveTarget;
					AdjustedDest = GoalActor->Location;
					Controller->CurrentPath = Controller->NextRoutePath;
					Controller->NextRoutePath = NULL;
					setMoveTimer(AdjustedDest - Location); 
				}
				else if ( (LineDir.Z > 0.f) && (LineDir.Z < 400.f) && (Level->TimeSeconds - AIMoveCheckTime > 0.2f) )
				{
					LineDir.Z = 0.f;
					if ( LineDir.SizeSquared() < Controller->NextRoutePath->CollisionRadius*Controller->NextRoutePath->CollisionRadius )
					{
						FCheckResult Hit(1.f);
						GetLevel()->SingleLineCheck(Hit, this, ClosestPoint, Location, TRACE_World, GetCylinderExtent());
						if ( !Hit.Actor )
						{
							if ( Controller->Focus == Controller->MoveTarget )
								Controller->Focus = Controller->NextRoutePath->End;
							Controller->MoveTarget = Controller->NextRoutePath->End;
							GoalActor = Controller->MoveTarget;
							AdjustedDest = GoalActor->Location;
							Controller->CurrentPath = Controller->NextRoutePath;
							Controller->NextRoutePath = NULL;
							setMoveTimer(AdjustedDest - Location); 
						}
					}
				}
			}
		}
	}

	if ( Physics != PHYS_Karma )
		return Super::moveToward( AdjustedDest, GoalActor );

	if ( (Throttle == 0.f) || (Velocity.SizeSquared() > 10000.f) )
		ThrottleTime = Level->TimeSeconds;

	VehicleMovingTime = Level->TimeSeconds;

	FVector Direction = AdjustedDest - Location;
	FLOAT ZDiff = Direction.Z;

	Direction.Z = 0.f;
	FLOAT Distance = Direction.Size();
	FCheckResult Hit(1.f);

	if ( ReachedDestination(AdjustedDest - Location, GoalActor) )
	{
		// FIXME - not if en route
        Throttle = 0.f;

		// if Pawn just reached a navigation point, set a new anchor
		ANavigationPoint *Nav = Cast<ANavigationPoint>(GoalActor);
		if ( Nav )
			SetAnchor(Nav);
		else if ( bScriptedRise )
		{
			APawn *P = Cast<APawn>(GoalActor);
			if ( P && (P == Controller->Enemy) )
				Rise = -1;
		}
		return true;
	}
	else if ( (Distance < CollisionRadius)
			&& (!GoalActor || ((ZDiff > CollisionHeight + 2.f * UCONST_MAXSTEPHEIGHT)
				&& !GetLevel()->SingleLineCheck(Hit, this, AdjustedDest, Location, TRACE_World))) )
	{
		// failed - below target
		return true;
	}
	else
	{
		if ( bCanFly )
		{
			if ( AdjustFlight(ZDiff, bFlyingDown,Distance,GoalActor) )
				return true;
		}
		else if ( !bScriptedRise )
			Rise = 0.f;	
		else if ( Level->TimeSeconds - AIMoveCheckTime > 0.2f )
		{
			AIMoveCheckTime = Level->TimeSeconds;

			// look for obstacles periodically, and rise over them
			if ( Rise == 0.f )
			{
				GetLevel()->SingleLineCheck(Hit, this, Location + 500.f * Direction/Distance, Location, TRACE_World|TRACE_Pawns);
				if ( Hit.Actor )
				{
					if ( Hit.Actor->bWorldGeometry && (Distance > 300.f) )
						Rise = 1.f;
					else if ( Hit.Actor->GetAPawn() )
					{
						// rise over vehicles and teammates, but not enemy foot soldiers
						if ( Cast<AVehicle>(Hit.Actor) )
							Rise = 1.f;
						else if ( PlayerReplicationInfo && PlayerReplicationInfo->Team && Hit.Actor->GetAPawn()->PlayerReplicationInfo
								&& (Hit.Actor->GetAPawn()->PlayerReplicationInfo->Team == PlayerReplicationInfo->Team) )
							Rise = 1.f;
					}
				}
			}
		}
		SteerVehicle(Direction);
	}

	if ( Controller->MoveTarget && Controller->MoveTarget->GetAPawn() )
	{
		if (Distance < CollisionRadius + Controller->MoveTarget->CollisionRadius + 0.8f * MeleeRange)
			return true;
		return false;
	}

	if ( IsStuck() )
	{
		Controller->MoveTimer = -1.f;
	}

	FLOAT speed = Velocity.Size(); 
	if ( speed != 0.f )
		Throttle *= ::Min(1.f, 2.f*Distance/speed);
	return false;
	unguard;
}

UBOOL AVehicle::AdjustFlight(FLOAT ZDiff, UBOOL bFlyingDown, FLOAT Distance, AActor* GoalActor)
{
	guard(AVehicle::AdjustFlight)

	if ( ZDiff > -0.7f * CollisionHeight )
	{
		Rise = 1.f;
		if ( bFlyingDown && (Distance < 800.f) )
		{
			ANavigationPoint *Nav = Cast<ANavigationPoint>(GoalActor);
			if ( Nav && Nav->bMustBeReachable )
				return true;
		}					
	}
	else if ( ZDiff < -1.f * CollisionHeight )
	{
		if ( (ZDiff < -6.f * CollisionHeight) || (Distance < ::Max(400.f,Abs(ZDiff))) )
			Rise = -1.f;
		if ( bFlyingDown && ((Velocity.Z < ::Min(-800.f, 0.5f*ZDiff)) || (Distance > ::Max(300.f,Abs(ZDiff)))) )
			Rise = 1.f;
	}
	else
	{
		if ( bFlyingDown && (ZDiff <  0.f) )
		{
			if ( Velocity.Z < ::Min(ZDiff, -200.f) ) 
				Rise = 1.f;
			else
				Rise = -1.f;
		}
		else if ( Velocity.Z < -100.f )
			Rise = 1.f;
		else if ( Velocity.Z > 800.f )
			Rise = -1.f;
		else
			Rise = 0.f;
	}
	return false;
	unguard;
}

void AVehicle::SteerVehicle(FVector Direction)
{
	guard(AVehicle::SteerVehicle);

	Direction.Z = 0.f;
	FVector DirCross = Direction ^ FVector(0.f,0.f,1.f);
	DirCross = DirCross.SafeNormal();
	if ( bFollowLookDir && (VehicleMovingTime == Level->TimeSeconds) )
	{
		// make vehicle correct if velocity is off course
		FVector VelDir = (Velocity | DirCross) * DirCross;
		if ( (VelDir.SizeSquared() > 160000.f) && (VelDir.SizeSquared() < Direction.SizeSquared()) )
		{	
			FLOAT Distance = Direction.Size();
			Direction = Direction - Distance * VelDir.SafeNormal(); 
		}
		else if ( VelDir.SizeSquared() > 10000.f )
			Direction = Direction - VelDir;	
	}
	FLOAT Distance = Direction.Size();
	if ( Distance != 0.f ) 
		Direction = Direction/Distance;
	FRotator FaceRot = Rotation;
	FaceRot.Pitch = 0;
	FVector Facing = FaceRot.Vector();
	FLOAT Dot = Facing | Direction;

	Throttle = 1.f;

	if ( bTurnInPlace )
	{
		FRotator ViewRot = Rotation;
		ViewRot.Pitch = 0;
		FLOAT ViewDot = Direction | ViewRot.Vector();
		if ( ViewDot < 0.9f )
		{
			if ( (ViewDot < -0.9f) && (Distance > 0.5f * CollisionRadius)  )
				Throttle = -1.f;
			else
				Throttle = 0.f;
		}
	}

	if  ( Dot > 0.98f )
	{
		if ( VehicleMovingTime < Level->TimeSeconds )
			Throttle = 0.f;
		Steering = 0.f;
		DesiredRotation.Yaw = Rotation.Yaw;
	}
	else if ( bFollowLookDir )
	{
		Throttle = Dot;
		FVector Cross = Facing ^ FVector(0.f,0.f,1.f);
		Cross = Cross.SafeNormal();
		Steering = Cross | Direction;
	}
	else if ( !bTurnInPlace && (Dot < -0.7f) && (Distance < 500.f) && (Distance > 1.5f) )
	{
		Throttle = -1.f;
		FVector Cross = Facing ^ FVector(0.f,0.f,1.f);
		if ( (Cross | Direction) < 0.f )
			Steering = 1.f;
		else
			Steering = -1.f;
	}
	else
	{
		FVector Cross = Facing ^ FVector(0.f,0.f,1.f);
		if ( (Cross | Direction) > 0.f )
			Steering = 1.f;
		else
			Steering = -1.f;
		if ( !bTurnInPlace && (Dot < ((OldSteering == -1.f) ? 0.3f : 0.f)) )
		{
			Steering *= -1.f;
			Throttle = -1.f;
		}
		if ( IsStuck() )
		{
			if ( bScriptedRise )
				Rise = 1.f;
			else
			{
				// check if stuck
				Steering *= -1.f;
				Throttle *= -1.f;
			}
		}
		if ( bHasHandbrake )
		{
			Direction.Z = 0.f;
			Direction = Direction.SafeNormal();
			FLOAT Dot = Facing | Direction;
			if ( (Dot < 0.9f) && (Dot > 0.f) && (Velocity.SizeSquared() > 240000.f) )
			{
				FVector Velocity2D = Velocity;
				Velocity2D.Z = 0.f;
				FVector VelDir = Velocity2D.SafeNormal();
				// check if sliding
				if ( (VelDir | Facing) < 0.96f )
				{
					if ( (VelDir | Facing) > 0.f )
					{
						// if facing forward, steer into it, or throttle down
						if ( (VelDir | Facing) < 0.9f )
						{
							Steering = 0.f;
						}
						else
							Throttle *= 0.1f;
					}
					// if sliding, no handbrake
					Rise = 0.f;
				}
				else
				{
					Rise = 1.f;
				}
				if ( Dot < 0.7f )
					Throttle = 0.f;
			}
			else
				Rise = 0.f;
		}
	}
	OldSteering = Throttle;
	unguard;
}

/* rotateToward()
rotate Actor toward a point.  Returns 1 if target rotation achieved.
(Set DesiredRotation, let physics do actual move)
*/
void AVehicle::rotateToward(AActor *Focus, FVector FocalPoint)
{
	guard(AVehicle::rotateToward);

	if ( !Controller )
		return;

	if ( (Throttle == 0.f) || (Velocity.SizeSquared() > 10000.f) )
		ThrottleTime = Level->TimeSeconds;

	if ( bFlyingKarma || (!bFollowLookDir && (Physics != PHYS_Karma) ))
	{
		Super::rotateToward(Focus,FocalPoint);
		return;
	}

	if ( Focus )
	{
		FocalPoint = Focus->Location;
		Controller->FocalPoint = FocalPoint;
	}

	FVector Direction = FocalPoint - Location - FVector(0,0,BaseEyeHeight);

	if ( bSeparateTurretFocus )
	{
		if ( Controller && Controller->MoveTarget )
			Direction = Controller->MoveTarget->Location - Location;
		else
			Direction = Rotation.Vector();
	}

	if ( bFollowLookDir )
	{
		Controller->Rotation = Direction.Rotation();
		DesiredRotation = Controller->Rotation;
		if ( VehicleMovingTime < Level->TimeSeconds )
			Throttle = bTurnInPlace ? 0.f : 1.f;
		return;
	}
	SteerVehicle(Direction.SafeNormal());
	if ( bTurnInPlace && (VehicleMovingTime < Level->TimeSeconds) )
	{
		Throttle = 0.f;
		if ( bFollowLookDir )
			Steering = 0.f;
		if ( bCanFly )
		{
		    if ( Velocity.Z < -100.f )
			    Rise = 1.f;
		    else if ( Velocity.Z > 800.f )
			    Rise = -1.f;
		    else
			    Rise = 0.f;
		}
	}
	unguard;
}

void AVehicle::performPhysics(FLOAT DeltaSeconds)
{
	guard(AVehicle::performPhysics);

	if (!bStalled && Location.Z > Level->StallZ)
	{
		bStalled = true;
		eventStalled();
	}
	else if (bStalled && Location.Z < Level->StallZ)
	{
		bStalled = false;
		eventUnStalled();
	}

	if (bStalled)
		Rise = -1;

	Super::performPhysics(DeltaSeconds);

	unguard;
}


void AVehicle::PostRender(FLevelSceneNode* SceneNode, FRenderInterface* RI)
{
	guard(AVehicle::PostRender);

    // Render team beacon
    if ( !SceneNode || !SceneNode->Viewport || !SceneNode->Viewport->Actor )
        return;

	if ( bScriptPostRender )
	{
		FVector camLoc = SceneNode->WorldToCamera.TransformFVector(Location + FVector(0.f,0.f,CollisionHeight));
		FPlane  screenLoc = SceneNode->Project(SceneNode->CameraToWorld.TransformFVector(camLoc));
		screenLoc.X = (SceneNode->Viewport->Canvas->ClipX * 0.5f * (screenLoc.X + 1.f));
		screenLoc.Y = (SceneNode->Viewport->Canvas->ClipY * 0.5f * (-screenLoc.Y + 1.f));
		eventPostRender2D(SceneNode->Viewport->Canvas,screenLoc.X, screenLoc.Y);
		return;
	}
    if ( bNoTeamBeacon || ((SceneNode->Viewport->Actor->ViewTarget == this) && (SceneNode->Viewport->Actor->Pawn == this)) )
        return;

    APlayerController* pc = SceneNode->Viewport->Actor;
	UBOOL bSpectating = pc->PlayerReplicationInfo ? (pc->PlayerReplicationInfo->bOnlySpectator && !pc->bHideSpectatorBeacons) : false;

    INT playerTeamIndex   = -1;

	// Draw 'No Entry' indicator.
	if ( !bSpectating && !SceneNode->Viewport->Actor->bHideVehicleNoEntryIndicator )
	{
		if (pc->PlayerReplicationInfo && pc->PlayerReplicationInfo->Team)
			playerTeamIndex = pc->PlayerReplicationInfo->Team->TeamIndex;

		if (playerTeamIndex < 0 || playerTeamIndex > 1)
			return;

		if ( bTeamLocked && (playerTeamIndex != Team) && !PlayerReplicationInfo )
		{
			FLOAT actorDist = SceneNode->WorldToScreen.TransformFVector(Location).Z;

			if ( (actorDist < 0.f) || (actorDist > 2.f * pc->TeamBeaconPlayerInfoMaxDist) || !NoEntryTexture )
				return;

			FCheckResult Hit(1.f);
			GetLevel()->SingleLineCheck( Hit, this, Location, SceneNode->ViewOrigin, TRACE_World|TRACE_StopAtFirstHit );
			if ( Hit.Actor )
			{
				GetLevel()->SingleLineCheck( Hit, this, Location + FVector(0.f,0.f,0.5f*CollisionHeight), SceneNode->ViewOrigin, TRACE_World|TRACE_StopAtFirstHit );
				if ( Hit.Actor )
					return;
			}
			GetLevel()->SingleLineCheck( Hit, pc->Pawn, Location, SceneNode->ViewOrigin, TRACE_Pawns );
			if ( Hit.Actor && (Hit.Actor != this) && !Hit.Actor->bWorldGeometry )
			{
				if ( Hit.Actor->Physics == PHYS_Karma )
					return;
				FVector Projected = (Hit.Actor->Location - Hit.Location);
				FVector ViewDir = (Hit.Actor->Location - SceneNode->ViewOrigin).SafeNormal();
				Projected = Projected - ViewDir * (ViewDir | Projected);
				if ( Abs(Projected.Z) < 0.8f * Hit.Actor->CollisionHeight )
					return;
				Projected.Z = 0.f;
				if ( Projected.SizeSquared() < 0.25f * Hit.Actor->CollisionRadius * Hit.Actor->CollisionRadius )
					return;
			}

			// draw locked symbol
			FVector camLoc = SceneNode->WorldToCamera.TransformFVector(Location);
			FPlane  screenLoc = SceneNode->Project(SceneNode->CameraToWorld.TransformFVector(camLoc));
			SceneNode->Viewport->Canvas->Style = STY_AlphaZ;
			FLOAT   xscale = ::Clamp( (2.f*pc->TeamBeaconPlayerInfoMaxDist - actorDist)/(2.f*pc->TeamBeaconPlayerInfoMaxDist), 0.55f, 1.f);
			xscale = xscale * xscale;
			screenLoc.X = (SceneNode->Viewport->Canvas->ClipX * 0.5f * (screenLoc.X + 1.f)) - 0.5*NoEntryTexture->USize*xscale;
			screenLoc.Y = (SceneNode->Viewport->Canvas->ClipY * 0.5f * (-screenLoc.Y + 1.f)) - 0.5*NoEntryTexture->VSize*xscale;

			SceneNode->Viewport->Canvas->DrawTile(
				NoEntryTexture,
				screenLoc.X, 
				screenLoc.Y, 
				NoEntryTexture->USize*xscale,
				NoEntryTexture->VSize*xscale,
				0.f, 
				0.f, 
				NoEntryTexture->USize, 
				NoEntryTexture->VSize,
				0.f,
				FPlane(1.f,0.f,0.f,1.f),
				FPlane(0.0f,0.0f,0.0f,0.0f));
			return;
		}
	}

    INT teamIndex = -1;

	if (PlayerReplicationInfo && PlayerReplicationInfo->Team)
		teamIndex = PlayerReplicationInfo->Team->TeamIndex;
	else
		teamIndex = Team;

	if ( teamIndex == -1 )
		return;
	if ( !bSpectating && (teamIndex != playerTeamIndex) )
		return;
    UTexture* teamBeacon = TeamBeaconTexture;
	UMaterial* teamBeaconBorder = TeamBeaconBorderMaterial;

    if ( !teamBeacon )
	{
		if ( !bDrawDriverInTP || (Driver && ((Location - SceneNode->ViewOrigin).SizeSquared() > Square(Driver->CullDistance))) )
			Super::PostRender(SceneNode,RI);
        return;
	}

    FLOAT actorDist = pc->FOVBias * SceneNode->WorldToScreen.TransformFVector(Location).Z;
	FLOAT ScaledDist = pc->TeamBeaconMaxDist * ::Clamp(0.04f * CollisionRadius,1.f,3.f);

	if ( (actorDist < 0.f) || (actorDist > ScaledDist) )
        return;

    if (!pc->LineOfSightTo(this))
        return;

    FPlane color = (teamIndex == 0) ? FPlane(1.f,0.25f,0.25f,1.f) : FPlane(0.25f,0.35f,1.f,1.f);
	if ( teamIndex > 1 )
		color = (teamIndex == 2) ? FPlane(0.f,1.f,0.25f,1.f) : FPlane(1.f,1.f,0.f,1.f);
    FVector camLoc = SceneNode->WorldToCamera.TransformFVector(Location+FVector(0.0f,0.0f,1.75f * CollisionHeight));
    FPlane  screenLoc = SceneNode->Project(SceneNode->CameraToWorld.TransformFVector(camLoc));

	SceneNode->Viewport->Canvas->Style = STY_AlphaZ;
	FLOAT   xscale = ::Clamp( (pc->TeamBeaconMaxDist - actorDist)/pc->TeamBeaconMaxDist, 0.7f, 1.f);
	xscale = xscale * xscale * 0.5f;
	if ( actorDist < 10.f*CollisionRadius)
		xscale *= 3.f * ::Max( 0.333f, (10.f*CollisionRadius - actorDist)/(10.f*CollisionRadius));
	else if ( actorDist > 1.5f * pc->TeamBeaconMaxDist )
		xscale *= ::Max( 0.5f, (ScaledDist - actorDist)/(ScaledDist - 1.5f * pc->TeamBeaconMaxDist));

	FLOAT   yscale = 0.25f * xscale;
	screenLoc.X = (SceneNode->Viewport->Canvas->ClipX * 0.5f * (screenLoc.X + 1.f)) - 0.5*teamBeacon->USize*xscale;
	screenLoc.Y = (SceneNode->Viewport->Canvas->ClipY * 0.5f * (-screenLoc.Y + 1.f)) - 0.5*teamBeacon->VSize*xscale;

	if ( !bHUDTrackVehicle )
	{
		if ( teamBeaconBorder )
		{
			SceneNode->Viewport->Canvas->DrawTile(
				teamBeaconBorder,
				screenLoc.X, 
				screenLoc.Y, 
				teamBeacon->USize*xscale,
				teamBeacon->VSize*yscale,
				0.f, 
				0.f, 
				teamBeacon->USize, 
				teamBeacon->VSize,
				0.f,
				FPlane(1.f,1.f,1.f,1.f),
				FPlane(0.0f,0.0f,0.0f,0.0f));
		}

		if ( teamBeacon )
		{
			FPlane HealthColor;
			if (Health / HealthMax > 0.5)
			{
				HealthColor = FPlane(::Clamp(1.f - (HealthMax - (HealthMax - Health) * 2)/HealthMax,0.f,1.f),
									1.f,
									0.f,
									1.f);
			}
			else
			{
				HealthColor = FPlane(1.f,
									::Clamp(2.f*Health/HealthMax,0.f,1.f),
									0.f,
									1.f);
			}
			SceneNode->Viewport->Canvas->DrawTile(
				teamBeacon,
				screenLoc.X, 
				screenLoc.Y, 
				teamBeacon->USize*xscale * Health/HealthMax,
				teamBeacon->VSize*yscale,
				0.f, 
				0.f, 
				teamBeacon->USize, 
				teamBeacon->VSize,
				0.f,
				HealthColor,
				FPlane(0.0f,0.0f,0.0f,0.0f));
		}
	}

	if ( PlayerReplicationInfo && (!bDrawDriverInTP  || (Driver && ((Location - SceneNode->ViewOrigin).SizeSquared() > Square(Driver->CullDistance)))) )
	{
		FLOAT xL, yL;
		FString info;

		UTexture* PCteamBeacon = pc->TeamBeaconTexture;
		if ( PCteamBeacon )
		{
			//FLOAT   xscale = Clamp(0.28f * (ScaledDist - actorDist)/ScaledDist,0.1f, 0.25f);
			if ( pc->myHUD && pc->myHUD->PortraitPRI && (pc->myHUD->PortraitPRI ==PlayerReplicationInfo) && pc->SpeakingBeaconTexture )
			{
				teamBeacon = pc->SpeakingBeaconTexture;
				xscale = 3.f * Clamp(0.28f * (ScaledDist - actorDist)/ScaledDist,0.1f, 0.25f);;
			}
			screenLoc.X -= teamBeacon->USize*xscale;
			screenLoc.Y -= 0.5*teamBeacon->VSize*xscale;
			SceneNode->Viewport->Canvas->Style = STY_AlphaZ;

			SceneNode->Viewport->Canvas->DrawTile(
				PCteamBeacon,
				screenLoc.X, 
				screenLoc.Y, 
				PCteamBeacon->USize*xscale,
				PCteamBeacon->VSize*xscale,
				0.f, 
				0.f, 
				PCteamBeacon->USize, 
				PCteamBeacon->VSize,
				0.f,
				color,
				FPlane(0.0f,0.0f,0.0f,0.0f));
		}
		if ( !GIsPixomatic && (actorDist < 2.f*pc->TeamBeaconPlayerInfoMaxDist) && (SceneNode->Viewport->Canvas->ClipX > 600) )
		{
			if ( PlayerReplicationInfo->bBot )
				info +=(PlayerReplicationInfo->eventGetNameCallSign());
			else
 				info +=(PlayerReplicationInfo->PlayerName);
			SceneNode->Viewport->Canvas->ClippedStrLen(
				SceneNode->Viewport->Canvas->SmallFont,
				1.f, 1.f, xL, yL, *info);

			INT index = pc->PlayerNameArray.AddZeroed();
			pc->PlayerNameArray(index).mInfo  = info;
			pc->PlayerNameArray(index).mColor = color;
			pc->PlayerNameArray(index).mXPos  = screenLoc.X;
			pc->PlayerNameArray(index).mYPos  = screenLoc.Y - yL;
		}
	}
	unguard;
}

UBOOL AVehicle::Tick(FLOAT DeltaTime, enum ELevelTick TickType)
{
	guard(AVehicle::Tick);

	if (!Super::Tick(DeltaTime, TickType))
		return 0;

	if (bDeleteMe || (TickType == LEVELTICK_ViewportsOnly && !PlayerControlled()))
		return 1;

    if ( (Role == ROLE_Authority) && PhysicsVolume->bWaterVolume && PhysicsVolume->bDamagesVehicles && (WaterDamage > 0.f) )
		eventTakeWaterDamage(DeltaTime);

	APlayerController *PC = Controller ? Controller->GetAPlayerController() : NULL;
	if ( Driver )
	{
		if ( bEnemyLockedOn && PC && (Role == ROLE_Authority) 
			&& (Level->TimeSeconds > LastLockWarningTime + LockWarningInterval) )
		{
			eventLockOnWarning();
		}

		if ( (Level->NetMode != NM_DedicatedServer) && !bDropDetail && bDrawDriverInTP 
			&& bAdjustDriversHead && (Driver->HeadBone != NAME_None) )
		{
			FRotator Look(0,0,0);
			Look.Yaw = (DriverViewYaw - Driver->Rotation.Yaw) & 65535;
			if ( Look.Yaw > 12000 )
			{
				if ( Look.Yaw < 32768 )
				{
					if ( Look.Yaw > 20768 )
						Look.Yaw = Look.Yaw - 32768;
					else if ( Look.Yaw > 16768 )
						Look.Yaw = -12000 + 6 * (20768 - Look.Yaw);
					else
						Look.Yaw = 12000;
				}
				else if ( Look.Yaw < 53535 )
				{
					if ( Look.Yaw < 44768 )
						Look.Yaw = Look.Yaw + 32768;
					else if ( Look.Yaw < 48768 )
						Look.Yaw = 12000 + 6 * (44768 - Look.Yaw);
					else
						Look.Yaw = 53535;
				}
			}
			Look.Yaw *= -1;
			if( Driver->Mesh && Driver->Mesh->IsA(USkeletalMesh::StaticClass())) 
			{
				((USkeletalMeshInstance*)Driver->Mesh->MeshGetInstance(Driver))->SetBoneDirection(Driver->HeadBone, Look, FVector(0.f,0.f,0.f), 1.f, 0);
			}
		}
	}

	if(	PC
		&&	PC->bEnableGUIForceFeedback
		&&	PC->bForceFeedbackSupported
		&&	PC->LocalPlayerController()
		&&	(CenterSpringRangePitch > 0)
		&&	(CenterSpringRangeRoll > 0) )
	{
		eventUpdateTiltForceFeedback();
	}

	return 1;
	unguard;
}
