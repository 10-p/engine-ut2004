/*=============================================================================
	UnRenderEditorActor.cpp: Editor actor information rendering.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Steven Polge
=============================================================================*/

#include "EnginePrivate.h"
#include "UnRenderPrivate.h"

void ALadderVolume::RenderEditorInfo(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA)
{
	guard(ALadderVolume::RenderEditorInfo);

	Super::RenderEditorInfo(SceneNode,RI,FDA);
	UEngine* Engine = SceneNode->Viewport->GetOuterUClient()->Engine;
	FLineBatcher LineBatcher(RI);
	LineBatcher.DrawDirectionalArrow( FindCenter(), WallDir, Engine->C_ActorArrow, DrawScale );
	unguard;
}

void AProjector::RenderEditorSelected(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA)
{
	guard(AProjector::RenderEditorSelected);

	// display projector bounding box and projection frustum.
	RenderWireframe(RI);
	Super::RenderEditorSelected(SceneNode,RI,FDA);
	unguard;
}

void ACamera::RenderEditorInfo(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA)
{
	guard(ACamera::RenderEditorInfo);
	Super::RenderEditorInfo(SceneNode,RI,FDA);
	if( SceneNode->Viewport->IsOrtho() )
	{
		RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);
		FLineBatcher LineBatcher(RI);
		UEngine* Engine = SceneNode->Viewport->GetOuterUClient()->Engine;
		LineBatcher.DrawDirectionalArrow( Location, Rotation, Engine->C_ActorArrow, DrawScale );
	}
	unguard;
}

void AInterpolationPoint::RenderEditorSelected(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA)
{
	guard(AInterpolationPoint::RenderEditorSelected);

	// Camera View Cone
	FLineBatcher LineBatcher(RI);
	FCoords DirectionCoords = GMath.UnitCoords / Rotation;
	FVector Direction = DirectionCoords.XAxis;
	FVector UpVector = DirectionCoords.ZAxis;
	FVector LeftVector = -DirectionCoords.YAxis;

	FVector Verts[8];

	Verts[0] = Location + (Direction * 24) + (32 * (UpVector + LeftVector).SafeNormal());
	Verts[1] = Location + (Direction * 24) + (32 * (UpVector - LeftVector).SafeNormal());
	Verts[2] = Location + (Direction * 24) + (32 * (-UpVector - LeftVector).SafeNormal());
	Verts[3] = Location + (Direction * 24) + (32 * (-UpVector + LeftVector).SafeNormal());

	Verts[4] = Location + (Direction * 128) + (64 * (UpVector + LeftVector).SafeNormal());
	Verts[5] = Location + (Direction * 128) + (64 * (UpVector - LeftVector).SafeNormal());
	Verts[6] = Location + (Direction * 128) + (64 * (-UpVector - LeftVector).SafeNormal());
	Verts[7] = Location + (Direction * 128) + (64 * (-UpVector + LeftVector).SafeNormal());


	LineBatcher.DrawLine( Verts[0], Verts[1], FColor(255,255,255) );
	LineBatcher.DrawLine( Verts[1], Verts[2], FColor(255,255,255) );
	LineBatcher.DrawLine( Verts[2], Verts[3], FColor(255,255,255) );
	LineBatcher.DrawLine( Verts[3], Verts[0], FColor(255,255,255) );

	LineBatcher.DrawLine( Verts[4], Verts[5], FColor(255,255,255) );
	LineBatcher.DrawLine( Verts[5], Verts[6], FColor(255,255,255) );
	LineBatcher.DrawLine( Verts[6], Verts[7], FColor(255,255,255) );
	LineBatcher.DrawLine( Verts[7], Verts[4], FColor(255,255,255) );

	LineBatcher.DrawLine( Verts[0], Verts[4], FColor(255,255,255) );
	LineBatcher.DrawLine( Verts[1], Verts[5], FColor(255,255,255) );
	LineBatcher.DrawLine( Verts[2], Verts[6], FColor(255,255,255) );
	LineBatcher.DrawLine( Verts[3], Verts[7], FColor(255,255,255) );

	Super::RenderEditorSelected(SceneNode,RI,FDA);
	unguard;
}

void ACamera::RenderEditorSelected(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA)
{
	guard(ACamera::RenderEditorSelected);
	unguard;
}

void APawn::RenderEditorSelected(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA)
{
	guard(APawn::RenderEditorSelected);

	Super::RenderEditorSelected(SceneNode,RI,FDA);

	// If this actor is an event source, draw event lines connecting it to
	// all corresponding event sinks.
	if(	(AIScriptTag!=NAME_None) && !IsHiddenEd() )
	{
		FLineBatcher	LineBatcher(RI,0);
		for( INT iOther=0; iOther<GetLevel()->Actors.Num(); iOther++ )
		{
			AActor* OtherActor = GetLevel()->Actors( iOther );
			if( OtherActor && (OtherActor->Tag == AIScriptTag) )
			{
				LineBatcher.DrawLine( Location, OtherActor->Location, FColor(0,0,255) );
			}
		}
	}
	unguard;
}

void AJumpPad::RenderEditorSelected(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA)
{
	guard(AJumpPad::RenderEditorSelected);

	if ( (PathList.Num() == 0) || !PathList(0) || !PathList(0)->End )
		return;

	FLineBatcher	LineBatcher(RI,0);

	FVector Vel = JumpVelocity;
	FVector Loc = Location;
	FVector Dest = PathList(0)->End->Location;
	while ( (Vel.Z > 0.f) || (Loc.Z > Dest.Z) )
	{
		FVector OldLoc = Loc;
		Loc += 0.03f * Vel;
		Vel += 0.03f * PhysicsVolume->Gravity;
		LineBatcher.DrawLine( Loc, OldLoc, FColor(255,64,255) );
	}
	unguard;
}

void AActor::RenderEditorSelected(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA)
{
	guard(AActor::RenderEditorSelected);

	// Directional Arrow
	FLineBatcher LineBatcher(RI);
	UEngine* Engine = SceneNode->Viewport->GetOuterUClient()->Engine;
	if( bDirectional )
	{
		RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);
		LineBatcher.DrawDirectionalArrow( Location, Rotation, Engine->C_ActorArrow, DrawScale );
	}

	// Radii
	if( SceneNode->Viewport->Actor->ShowFlags&SHOW_ActorRadii )
	{
		RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);

		if( IsMovingBrush() )
		{
			LineBatcher.DrawBox( FDA->BoundingBox, Engine->C_ActorArrow );
		}
		else if( !IsBrush() )
		{
			if( SceneNode->Viewport->IsOrtho() )
			{
				if( bCollideActors )
				{
					// Collision radius
					if( SceneNode->Viewport->Actor->RendMap==REN_OrthXY )
					{
						FPlane XPlane(SceneNode->CameraToWorld.M[0][0], SceneNode->CameraToWorld.M[0][1], SceneNode->CameraToWorld.M[0][2], SceneNode->CameraToWorld.M[0][3]);
						FPlane YPlane(SceneNode->CameraToWorld.M[1][0], SceneNode->CameraToWorld.M[1][1], SceneNode->CameraToWorld.M[1][2], SceneNode->CameraToWorld.M[1][3]);
						LineBatcher.DrawCircle( Location, XPlane, YPlane, Engine->C_BrushWire, CollisionRadius, 16 );
					}

					// Collision height.
					FVector Ext(CollisionRadius,CollisionRadius,CollisionHeight);
					FVector Min(Location - Ext);
					FVector Max(Location + Ext);
					if( SceneNode->Viewport->Actor->RendMap!=REN_OrthXY )
						LineBatcher.DrawBox( FBox( Min, Max ), Engine->C_BrushWire );
				}
			}
			else
			{
				if( bCollideActors )
					LineBatcher.DrawCylinder( RI, Location, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), Engine->C_BrushWire, CollisionRadius, CollisionHeight, 16 );
			}
		}

		// Sound radius.
		if( AmbientSound )
		{
			FPlane XPlane(SceneNode->CameraToWorld.M[0][0], SceneNode->CameraToWorld.M[0][1], SceneNode->CameraToWorld.M[0][2], SceneNode->CameraToWorld.M[0][3]);
			FPlane YPlane(SceneNode->CameraToWorld.M[1][0], SceneNode->CameraToWorld.M[1][1], SceneNode->CameraToWorld.M[1][2], SceneNode->CameraToWorld.M[1][3]);
			LineBatcher.DrawCircle( Location, XPlane, YPlane, Engine->C_ActorArrow, SoundRadius, 16 );
		}

		// Light radius.
		if( LightType!=LT_None && (LightBrightness > 0.0f) && (LightRadius > 0.0f) )
		{
			FPlane XPlane(SceneNode->CameraToWorld.M[0][0], SceneNode->CameraToWorld.M[0][1], SceneNode->CameraToWorld.M[0][2], SceneNode->CameraToWorld.M[0][3]);
			FPlane YPlane(SceneNode->CameraToWorld.M[1][0], SceneNode->CameraToWorld.M[1][1], SceneNode->CameraToWorld.M[1][2], SceneNode->CameraToWorld.M[1][3]);
			LineBatcher.DrawCircle( Location, XPlane, YPlane, Engine->C_ActorArrow, WorldLightRadius(), 16 );
		}
	}

	// Event Lines
	if( SceneNode->Viewport->Actor->ShowFlags&SHOW_EventLines )
	{
		// If this actor is an event source, draw event lines connecting it to
		// all corresponding event sinks.
		if(	(Event!=NAME_None) && !IsHiddenEd() )
		{
			RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);

			for( INT iOther=0; iOther<GetLevel()->Actors.Num(); iOther++ )
			{
				AActor* OtherActor = GetLevel()->Actors( iOther );
				if( OtherActor && OtherActor->Tag == Event )
				{
					LineBatcher.DrawLine( Location, OtherActor->Location, Engine->C_ActorArrow );
				}
			}
		}
	}

	unguard;
}

#ifdef WITH_KARMA
void AKConstraint::RenderEditorSelected(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA)
{
	guard(AKConstraint::RenderEditorSelected);

	if( SceneNode->Viewport->Actor->ShowFlags&SHOW_EventLines )
	{
		UEngine* Engine = SceneNode->Viewport->GetOuterUClient()->Engine;
		FLineBatcher LineBatcher(RI);
		if(KConstraintActor1)
		{
			FVector endLocation = KConstraintActor1->Location;			

			// If we are connected to a named bone, draw the line to inidcate this.
			if(	KConstraintBone1 != NAME_None &&
				KConstraintActor1->MeshInstance && 
				KConstraintActor1->MeshInstance->IsA(USkeletalMeshInstance::StaticClass()) )
			{
				USkeletalMeshInstance* inst = Cast<USkeletalMeshInstance>(KConstraintActor1->MeshInstance);
				INT boneIx = inst->MatchRefBone(KConstraintBone1);
				
				if(boneIx != INDEX_NONE)
				{
					FCoords boneCoords = inst->GetBoneCoords(boneIx);
					endLocation = boneCoords.Origin;
				}
			}

			LineBatcher.DrawLine( Location, endLocation, Engine->C_ConstraintLine );
		}

		if(KConstraintActor2)
		{
			FVector endLocation = KConstraintActor2->Location;			

			// If we are connected to a named bone, draw the line to inidcate this.
			if(	KConstraintBone2 != NAME_None &&
				KConstraintActor2->MeshInstance && 
				KConstraintActor2->MeshInstance->IsA(USkeletalMeshInstance::StaticClass()) )
			{
				USkeletalMeshInstance* inst = Cast<USkeletalMeshInstance>(KConstraintActor2->MeshInstance);
				INT boneIx = inst->MatchRefBone(KConstraintBone2);

				if(boneIx != INDEX_NONE)
				{
					FCoords boneCoords = inst->GetBoneCoords(boneIx);
					endLocation = boneCoords.Origin;
				}
			}

			LineBatcher.DrawLine( Location, endLocation, Engine->C_ConstraintLine );
		}
	}

	Super::RenderEditorSelected(SceneNode,RI,FDA);
	unguard;
}
#endif

void AActor::RenderEditorInfo(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA)
{
	guard(AActor::RenderEditorInfo);

	if ( bSelected )
		RenderEditorSelected(SceneNode,RI,FDA);

	if( SceneNode->Viewport->Actor->ShowFlags&SHOW_ActorInfo && DrawType==DT_Sprite )
	{
		// Information
		FCanvasUtil	CanvasUtil(&SceneNode->Viewport->RenderTarget,SceneNode->Viewport->RI);
		FPlane P = SceneNode->Project(
			Location
			+ FVector( 
				(Texture->MaterialUSize() * DrawScale * DrawScale3D.X) / 2,
				-(Texture->MaterialUSize() * DrawScale * DrawScale3D.Y) / 2,
				(Texture->MaterialUSize() * DrawScale * DrawScale3D.Z) / 2 )
		);
		FVector C = CanvasUtil.ScreenToCanvas.TransformFVector(P);

		INT CX, CY;
		SceneNode->Viewport->Canvas->WrappedStrLenf(SceneNode->Viewport->Canvas->SmallFont,CX,CY,TEXT("M"));

		SceneNode->Viewport->Canvas->Color = bSelected ? FColor(128,230,128) : FColor(255,255,255);

		SceneNode->Viewport->Canvas->SetClip(C.X,C.Y,SceneNode->Viewport->SizeX-C.X,SceneNode->Viewport->SizeY-C.Y);
		SceneNode->Viewport->Canvas->WrappedPrintf(SceneNode->Viewport->Canvas->SmallFont,0,GetClass()->GetName());
		SceneNode->Viewport->Canvas->SetClip(C.X,C.Y + CY,SceneNode->Viewport->SizeX-C.X,SceneNode->Viewport->SizeY-C.Y);
		SceneNode->Viewport->Canvas->WrappedPrintf(SceneNode->Viewport->Canvas->SmallFont,0,*FString::Printf(TEXT("E:%s"), *Event) );
		SceneNode->Viewport->Canvas->SetClip(C.X,C.Y + CY + CY,SceneNode->Viewport->SizeX-C.X,SceneNode->Viewport->SizeY-C.Y);
		SceneNode->Viewport->Canvas->WrappedPrintf(SceneNode->Viewport->Canvas->SmallFont,0,*FString::Printf(TEXT("T:%s"), *Tag) );
	}

	if( SceneNode->Viewport->Actor->ShowFlags&SHOW_ActorIcons )
	{
		// Icon View
		static UTexture* SActor = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.S_Actor") ));
		FCanvasUtil	CanvasUtil(&SceneNode->Viewport->RenderTarget,SceneNode->Viewport->RI);
		FPlane P = SceneNode->Project(Location);
		FVector C = CanvasUtil.ScreenToCanvas.TransformFVector(P);

		CanvasUtil.DrawTile(
			C.X - (SActor->USize/2),
			C.Y - (SActor->VSize/2),
			C.X + (SActor->USize/2),
			C.Y + (SActor->VSize/2),
			0,0,
			SActor->UClamp,SActor->VClamp,
			C.Z,
			//!!MAT
			SActor,/*PF_NoZTest|PF_Masked,*/bSelected ? FColor(128,230,128) : FColor(255,255,255) );
	}
	unguard;
}


