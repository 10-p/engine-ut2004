/*=============================================================================
	UnRender.cpp: High-level rendering code.
	Copyright 2001-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"
#include "UnRenderPrivate.h"
extern int QueueScreenShot; // sjs label hack
//
//	FSceneNode::FSceneNode
//
FSceneNode::FSceneNode(UViewport* InViewport,FRenderTarget* InRenderTarget)
{
	Parent				= NULL;
	Viewport			= InViewport;
	RenderTarget		= InRenderTarget; 
	Recursion			= 0;
}

//
//	FSceneNode::FSceneNode
//
FSceneNode::FSceneNode(FSceneNode* InParent)
{
	Parent				= InParent;
	Viewport			= Parent->Viewport;
	RenderTarget		= Parent->RenderTarget;
	Recursion			= Parent->Recursion + 1;

	WorldToCamera		= InParent->WorldToCamera;
	CameraToScreen		= InParent->CameraToScreen;
	ScreenToCamera		= InParent->ScreenToCamera;
	CameraToWorld		= InParent->CameraToWorld;

	WorldToScreen		= InParent->WorldToScreen;
	ScreenToWorld		= InParent->ScreenToWorld;

	ViewOrigin			= Parent->ViewOrigin;
	CameraX				= InParent->CameraX;
	CameraY				= InParent->CameraY;

	Determinant			= WorldToScreen.Determinant();
}

//
//	FSceneNode::~FSceneNode
//
FSceneNode::~FSceneNode()
{
}

//
//	FSceneNode::Project
//
FPlane FSceneNode::Project(FVector V)
{
	FPlane	Result = WorldToScreen.TransformFPlane(FPlane(V,1));
	FLOAT	RHW = 1.0f / Result.W;

	return FPlane(Result.X * RHW,Result.Y * RHW,Result.Z * RHW,Result.W);
}

//
//	FSceneNode::Deproject
//
FVector FSceneNode::Deproject(FPlane P)
{
	return ScreenToWorld.TransformFPlane(FPlane(P.X * P.W,P.Y * P.W,P.Z * P.W,P.W));
}


//
//	FCoronaRender
//
FCoronaRender::FCoronaRender()
{
    Reset(); 
}

void FCoronaRender::Reset()
{
    Delta		= 0.f;
	LastTime	= 0;
	iFree		= 0;
	iVisible	= 0;
	appMemzero(CoronaLights, sizeof(CoronaLights));
	appMemzero(VisibleLights, sizeof(VisibleLights));
}

void FCoronaRender::CleanupDestroyed()
{
	for( INT i=0; i<MAX_CORONA_LIGHTS; i++ )
	{
		const AActor* Actor = CoronaLights[i].Actor;
		if(	Actor && Actor->bDeleteMe )
			CoronaLights[i].Actor = NULL;
	}	
}

void FCoronaRender::AddVisibleLight( AActor* Light )
{
	INT i;
	for( i=0; i<iVisible; i++ )
		if( VisibleLights[i] == Light )
			break;
	if( i == iVisible )
		VisibleLights[iVisible++] = Light;

	iVisible %= MAX_CORONA_LIGHTS;
}

UBOOL FCoronaRender::IsOccluded( FLevelSceneNode* SceneNode, AActor* Actor )
{
	guard(FCoronaRender::IsOccluded);

	UBOOL	Occluded,
			Traced = false;

	INT i;
	for( i=0; i<MAX_CORONA_LIGHTS; i++ )
		if( CoronaLights[i].Actor == Actor )
			break;

	if( i<MAX_CORONA_LIGHTS && ((LastTime - CoronaLights[i].LastTrace) < (appFrand()*0.33f + 0.33f)) )
	{
		Occluded	= CoronaLights[i].LastOccluded;
	}
	else
	{
		if(Actor->bDirectionalCorona)
		{
			FVector LookDir = (Actor->Location - SceneNode->ViewOrigin).SafeNormal();
			FVector CoronaDir = FVector(1,0,0).TransformVectorBy( GMath.UnitCoords * Actor->Rotation );
			if( (LookDir | CoronaDir) > 0.0f )
			{
				Occluded = true;
				if(i<MAX_CORONA_LIGHTS)
				{
					CoronaLights[i].LastOccluded = Occluded;
					CoronaLights[i].LastTrace = LastTime;
				}
				return Occluded;
			}

			FCheckResult Hit;
			Occluded = !Actor->GetLevel()->SingleLineCheck( 
				Hit, 
				Actor->Owner, 
				SceneNode->ViewOrigin, 
				Actor->Location, 
				TRACE_Corona | TRACE_World | TRACE_Pawns, 
				FVector(0,0,0)
				);

			if(Occluded && !SceneNode->Viewport->Actor->bBehindView && Hit.Actor == SceneNode->ViewActor)
				Occluded = false;
		}
		else
		{

			FCheckResult Hit;
			Occluded = !Actor->GetLevel()->SingleLineCheck( 
				Hit, 
				SceneNode->Viewport->Actor->bBehindView ? NULL : SceneNode->ViewActor, 
				Actor->Location, 
				SceneNode->ViewOrigin, 
				TRACE_Corona | TRACE_World | TRACE_StopAtFirstHit | TRACE_Pawns, 
				FVector(0,0,0)
				);

		}

		Traced = true;
	}
	
	if( i<MAX_CORONA_LIGHTS )
	{
		CoronaLights[i].LastOccluded = Occluded;
		if( Traced )
			CoronaLights[i].LastTrace = LastTime;
	}

	return Occluded;
	unguard;
}

void FCoronaRender::AddCoronaLight( FLevelSceneNode* SceneNode, AActor* Actor, FLOAT Delta )
{
	guard(FCoronaRender::AddCoronaLight);
	clock(GStats.DWORDStats(GEngineStats.STATS_Corona_CollisionCycles));
	
	if
	(	Actor
	&&	Actor->bCorona
	&&	Actor->Skins.Num()
	&&	Cast<UTexture>(Actor->Skins(0))
	&& !Actor->bDeleteMe
	&& !IsOccluded( SceneNode, Actor )
	)
	{		
		INT i;
		for( i=0; i<MAX_CORONA_LIGHTS; i++ )
			if( CoronaLights[i].Actor == Actor )
				break;
		if( i<MAX_CORONA_LIGHTS )
		{
			CoronaLights[i].Bright = Min(1.f,CoronaLights[i].Bright+2.f*Delta*CoronaLights[i].Actor->ScaleGlow ); 
		}
		else
		{
			while( iFree<MAX_CORONA_LIGHTS && CoronaLights[iFree].Actor )
				iFree++;
			if( iFree<MAX_CORONA_LIGHTS )
			{
				CoronaLights[iFree].Actor			= Actor;
				CoronaLights[iFree].iActor			= Actor->GetLevel()->GetActorIndex(Actor);
				CoronaLights[iFree].Bright			= Min(1.f,2.f*Delta*CoronaLights[iFree].Actor->ScaleGlow); 
				CoronaLights[iFree].LastOccluded	= 0;
				CoronaLights[iFree].LastTrace		= LastTime;
			}
		}
	}

	unclock(GStats.DWORDStats(GEngineStats.STATS_Corona_CollisionCycles));
	unguard;
}

void FCoronaRender::RenderCoronas( FLevelSceneNode* SceneNode, FRenderInterface* RI )
{
	guard(FCoronaRender::RenderCoronas);

	FSpriteVertexStream CoronaVertices;

	// Backward compatibility mojo.
	Delta	 = Min<FLOAT>( 1.0f, 3.0f * (SceneNode->Viewport->CurrentTime - LastTime) );
	LastTime = SceneNode->Viewport->CurrentTime;

	// Fade out existing coronas, and remove if they've faded out completely.
	for( INT i=0; i<MAX_CORONA_LIGHTS; i++ )
		if( CoronaLights[i].Actor && (CoronaLights[i].Bright -= Delta * CoronaLights[i].Actor->ScaleGlow ) < 0.f ) 
			CoronaLights[i].Actor = NULL;

	// Update coronas.
	for( INT i=0; i<MAX_CORONA_LIGHTS; i++ )
		AddCoronaLight( SceneNode, VisibleLights[i], Delta );

	// Reset arrays & indices.
	iFree = 0;

	// Coronas shouldn't be fogged.
	RI->SetDistanceFog( 0, 0, 0, 0 );

	// Set identity matrix.
	RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);

	// Render the coronas.
	INT Count = 0;
	for( INT i=0; i<MAX_CORONA_LIGHTS; i++ )
	{
		AActor* Actor = CoronaLights[i].Actor;
		ALight* Light = Cast<ALight>(Actor);

		if(		!Actor
			||	!((CoronaLights[i].iActor <= SceneNode->Viewport->Actor->GetLevel()->Actors.Num()) && 
				(SceneNode->Viewport->Actor->GetLevel()->Actors(CoronaLights[i].iActor)==Actor))
			||	Actor->bDeleteMe
			||	!Actor->Skins.Num()
		)
		{
			CoronaLights[i].Actor = NULL;
			continue;
		}
		
		PUSH_HIT(SceneNode->Viewport,HActor(Actor));

		FVector ProjBase     = SceneNode->Deproject(FPlane(0,0,0,1));	
		FVector ProjUp		 = SceneNode->Deproject(FPlane(0,-1,0,1)) - ProjBase;
		FVector ProjRight	 = SceneNode->Deproject(FPlane(1,0,0,1)) - ProjBase;
		FVector ProjFront	 = ProjRight ^ ProjUp;

		ProjUp.Normalize();
		ProjRight.Normalize();
		ProjFront.Normalize();

		BYTE    H		= Actor->LightHue;
		FVector Hue		= (H<86) ? FVector((85-H)/85.f,(H-0)/85.f,0) : (H<171) ? FVector(0,(170-H)/85.f,(H-85)/85.f) : FVector((H-170)/85.f,0,(255-H)/84.f);
		FLOAT	Alpha	= Actor->LightSaturation / 255.0f;
		FPlane  Color	= (Hue + Alpha * (FVector(1,1,1) - Hue)) * CoronaLights[i].Bright;
		FLOAT Distance	= FDist(SceneNode->ViewOrigin,Actor->Location);
		Distance		= Clamp<FLOAT>( Distance / 1000.f, 0.01f, 10.f );

		FVector Right, Up;

		if ( (Actor->DrawScale3D.X != Actor->DrawScale3D.Y) && (Actor->DrawScale3D.Z > 0) )
		{
			// Non square coronas.
			FVector X		= FVector(1,0,0).TransformVectorBy( GMath.UnitCoords * Actor->Rotation );
			FVector Y		= FVector(0,1,0).TransformVectorBy( GMath.UnitCoords * Actor->Rotation );
			FVector Z		= FVector(0,0,1).TransformVectorBy( GMath.UnitCoords * Actor->Rotation );
			FLOAT   Angle	= appSqrt((1 - Abs(ProjFront | X)) * (1 - Abs(ProjFront | Y)));
			Color			= FColor(Color * Angle);
			Right			= X;
			Up				= Y;
		}
		else
		{
			// Square Coronas.
			Right			= ProjRight;
			Up				= ProjUp;
			
			// Rotate corona dependend on distance.
			if( Light && (Light->CoronaRotation || Light->CoronaRotationOffset) )
			{
				FLOAT Angle;
				Angle	= Light->CoronaRotation * 16384.f * Distance + Light->CoronaRotationOffset;
				Right	= Right.RotateAngleAxis( Angle, ProjFront );
				Up		= Up.RotateAngleAxis( Angle, ProjFront );
			}
		}

		FLOAT	CoronaFudgeFactor	= 512.f,
				ScaleX				= Actor->DrawScale * Actor->DrawScale3D.X * Distance * CoronaFudgeFactor,
				ScaleY				= Actor->DrawScale * Actor->DrawScale3D.Y * Distance * CoronaFudgeFactor;

		if(Actor->bDirectionalCorona)
		{
			FVector LookDir = (Actor->Location - SceneNode->ViewOrigin).SafeNormal();
			FVector CoronaDir = FVector(1,0,0).TransformVectorBy( GMath.UnitCoords * Actor->Rotation );
			FLOAT DirScale = Clamp( -1.f * (LookDir | CoronaDir), 0.0f, 1.0f );
			ScaleX *= DirScale;
			ScaleY *= DirScale;
		}

		// Clamp size.
		if( Light )
		{
			ScaleX = Clamp( ScaleX, Light->MinCoronaSize, Light->MaxCoronaSize );
			ScaleY = Clamp( ScaleY, Light->MinCoronaSize, Light->MaxCoronaSize );
		}

		Right	*= ScaleX;
		Up		*= ScaleY;

		FColor RenderColor	= Color;
		RenderColor.A		= 255;
		RenderColor			= RenderColor.RenderColor();

		CoronaVertices.Vertices[0].Position	= Actor->Location - Right - Up;
		CoronaVertices.Vertices[0].U		= 0.f;
		CoronaVertices.Vertices[0].V		= 0.f;
		CoronaVertices.Vertices[0].Diffuse	= RenderColor;
			
		CoronaVertices.Vertices[1].Position	= Actor->Location + Right - Up;
		CoronaVertices.Vertices[1].U		= 1.f;
		CoronaVertices.Vertices[1].V		= 0.f;
		CoronaVertices.Vertices[1].Diffuse	= RenderColor;
			
		CoronaVertices.Vertices[2].Position	= Actor->Location + Right + Up;
		CoronaVertices.Vertices[2].U		= 1.f;
		CoronaVertices.Vertices[2].V		= 1.f;
		CoronaVertices.Vertices[2].Diffuse	= RenderColor;
			
		CoronaVertices.Vertices[3].Position	= Actor->Location - Right + Up;
		CoronaVertices.Vertices[3].U		= 0.f;
		CoronaVertices.Vertices[3].V		= 1.f;
		CoronaVertices.Vertices[3].Diffuse	= RenderColor;

		// Draw the sprite.
		INT BaseVertexIndex = RI->SetDynamicStream(VS_FixedFunction,&CoronaVertices);

		UBOOL AlphaTest = 0;
		UTexture* Texture = Cast<UTexture>(Actor->Skins(0));
		if( Texture && Texture->bAlphaTexture )
			AlphaTest = 1;

		DECLARE_STATIC_UOBJECT( UFinalBlend, FinalBlend, {} );
		FinalBlend->Material			= Actor->Skins(0);
		FinalBlend->TwoSided			= 1;
		FinalBlend->FrameBufferBlending = FB_Translucent;
		FinalBlend->ZWrite				= 0;
		FinalBlend->ZTest				= 0;
		FinalBlend->AlphaTest			= AlphaTest;
		FinalBlend->AlphaRef			= 0;

		RI->EnableLighting(0,1);
		RI->SetMaterial( Light && Light->UseOwnFinalBlend ? Actor->Skins(0) : FinalBlend );

		RI->SetIndexBuffer(NULL,0);
		RI->DrawPrimitive(PT_TriangleFan,BaseVertexIndex,2);

		POP_HIT( SceneNode->Viewport );

		Count++;
	}

	//!!SceneNode->Viewport->RenderStats.IntStat(TEXT("Coronas"),TEXT("rendered")) = Count;
	unguard;
}


//
//	DrawSprite
//
static void DrawSprite(AActor* Actor,FLevelSceneNode* SceneNode,FRenderInterface* RI)
{
	guard(DrawSprite);

	// Calculate the sprite right and down vectors.

	FVector	SpriteX = SceneNode->CameraX.SafeNormal() * Actor->DrawScale * Actor->DrawScale3D.X * Actor->Texture->MaterialUSize() / 2.0f,
			SpriteY = -SceneNode->CameraY.SafeNormal() * Actor->DrawScale * Actor->DrawScale3D.Y * Actor->Texture->MaterialUSize() / 2.0f;

	// Calculate the display color.

	FPlane	ColorPlane = FPlane(1.0f,1.0f,1.0f,1.0f);

	if(GIsEditor)
	{
		if(SceneNode->Viewport->Actor->ShowFlags & SHOW_SelectionHighlight && Actor->bSelected)
			ColorPlane = FPlane(0.5f,0.9f,0.5f,1.0f);
		else if(Actor->LightType != LT_None)
			ColorPlane = FGetHSV(Actor->LightHue,Actor->LightSaturation,255);
	}

	// Setup the sprite vertices.
	FSpriteVertexStream	SpriteVertices;
	FColor	Color = FColor(ColorPlane);
	FVector	SpriteBase = Actor->Location;

	UMaterial* Material = Actor->Texture;
	UTexture* Texture = Cast<UTexture>(Material);
	if( Texture )
	{
		DECLARE_STATIC_UOBJECT( UFinalBlend, FinalBlend, {} );
		FinalBlend->Material = Texture;
		FinalBlend->TwoSided = 1;

		if( Texture->bMasked || Actor->Style==STY_Masked )
		{
			FinalBlend->FrameBufferBlending = FB_Overwrite;
			FinalBlend->ZWrite				= 1;
			FinalBlend->ZTest				= 1;
			FinalBlend->AlphaTest			= 1;
			FinalBlend->AlphaRef			= 127;
		}
		else
		if( Texture->bAlphaTexture )
		{
			FinalBlend->FrameBufferBlending = FB_AlphaBlend;
			FinalBlend->ZWrite				= 0;
			FinalBlend->ZTest				= 1;
			FinalBlend->AlphaTest			= 1;
			FinalBlend->AlphaRef			= 0;
		}
		else
		if( Actor->Style==STY_Translucent )
		{
			FinalBlend->FrameBufferBlending = FB_Translucent;
			FinalBlend->ZWrite				= 0;
			FinalBlend->ZTest				= 1;
			FinalBlend->AlphaTest			= 0;
		}
		else
		if( Actor->Style==STY_Modulated )
		{
			FinalBlend->FrameBufferBlending = FB_Modulate;
			FinalBlend->ZWrite				= 0;
			FinalBlend->ZTest				= 1;
			FinalBlend->AlphaTest			= 0;
		}
		else
		if( Actor->Style==STY_Additive )
		{
			FinalBlend->FrameBufferBlending = FB_Brighten;
			FinalBlend->ZWrite				= 0;
			FinalBlend->ZTest				= 1;
			FinalBlend->AlphaTest			= 0;
		}
		else
		if( Actor->Style==STY_Subtractive ) 
		{
			FinalBlend->FrameBufferBlending = FB_Darken;
			FinalBlend->ZWrite				= 0;
			FinalBlend->ZTest				= 1;
			FinalBlend->AlphaTest			= 0;
		}

		Material = FinalBlend;
	}

	SpriteVertices.Vertices[0].Position = SpriteBase - SpriteX - SpriteY;
	SpriteVertices.Vertices[0].Diffuse = Color;
	SpriteVertices.Vertices[0].U = 0.0f;
	SpriteVertices.Vertices[0].V = 0.0f;

	SpriteVertices.Vertices[1].Position = SpriteBase + SpriteX - SpriteY;
	SpriteVertices.Vertices[1].Diffuse = Color;
	SpriteVertices.Vertices[1].U = 1.0f;
	SpriteVertices.Vertices[1].V = 0.0f;

	SpriteVertices.Vertices[2].Position = SpriteBase + SpriteX + SpriteY;
	SpriteVertices.Vertices[2].Diffuse = Color;
	SpriteVertices.Vertices[2].U = 1.0f;
	SpriteVertices.Vertices[2].V = 1.0f;

	SpriteVertices.Vertices[3].Position = SpriteBase - SpriteX + SpriteY;
	SpriteVertices.Vertices[3].Diffuse = Color;
	SpriteVertices.Vertices[3].U = 0.0f;
	SpriteVertices.Vertices[3].V = 1.0f;

	// Draw the sprite.

	INT BaseVertexIndex = RI->SetDynamicStream(VS_FixedFunction,&SpriteVertices);

	RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);
	//!!MAT
	//RI->SetBlending(PolyFlags | PF_TwoSided | Texture->PolyFlags);
	//RI->SetTexture(0,Texture->GetRenderInterface());
	RI->EnableLighting(0,1);
	RI->SetMaterial(Material);
	RI->SetIndexBuffer(NULL,0);

	RI->DrawPrimitive(PT_TriangleFan,BaseVertexIndex,2);

	unguard;
}

//
//	RequiresSorting
//

UBOOL RequiresSorting(AActor* Actor)
{
	guard(RequiresSorting);

	if(Actor->bDisableSorting)
		return 0;

	if(Actor->DrawType == DT_StaticMesh && Actor->StaticMesh)
	{
		for(INT MaterialIndex = 0;MaterialIndex < Actor->StaticMesh->Materials.Num();MaterialIndex++)
		{
			UMaterial*	Material = Actor->StaticMesh->GetSkin(Actor,MaterialIndex);

			if(Material && Material->RequiresSorting())
				return 1;
		}
	}
	else if(Actor->DrawType == DT_Mesh && Cast<ULodMesh>(Actor->Mesh))
	{
		ULodMesh*	LodMesh = Cast<ULodMesh>(Actor->Mesh);

		for(INT MaterialIndex = 0;MaterialIndex < LodMesh->Materials.Num();MaterialIndex++)
		{
			if(LodMesh->Materials(MaterialIndex) && LodMesh->Materials(MaterialIndex)->RequiresSorting())
				return 1;
		}
	}
	else if(Actor->DrawType == DT_Brush)
		return 1;
	else if(Actor->DrawType == DT_AntiPortal)
		return 1;
	else if(Actor->DrawType == DT_Particle)
		return 1;
	else if(Actor->DrawType == DT_Sprite)
		return 1;

	if(Actor->DrawType == DT_Mesh || Actor->DrawType == DT_FluidSurface)
	{
		for(INT SkinIndex = 0;SkinIndex < Actor->Skins.Num();SkinIndex++)
		{
			if(Actor->Skins(SkinIndex) && Actor->Skins(SkinIndex)->RequiresSorting())
				return 1;
		}
	}

	for(INT AttachmentIndex = 0;AttachmentIndex < Actor->Attached.Num();AttachmentIndex++)
		if(Actor->Attached(AttachmentIndex) && RequiresSorting(Actor->Attached(AttachmentIndex)))
			return 1;

	return 0;

	unguard;
}

//
//	FDynamicActor::FDynamicActor
//

FDynamicActor::FDynamicActor(AActor* InActor)
{
	Actor = InActor;
	VisibilityTag = 0;
	Revision = INDEX_NONE;
    PredictedBox.Init();
}

//
//	FDynamicActor::Update
//
	
void FDynamicActor::Update()
{
	guard(FDynamicActor::Update);

	if( Actor->DrawType == DT_Mesh && Actor->Mesh )
	{
		// Set up specific mesh-to-world matrix. 
		LocalToWorld = Actor->Mesh->MeshGetInstance(Actor)->MeshToWorld();		
	}
	else
	{
		LocalToWorld = Actor->LocalToWorld();
	}

	WorldToLocal = LocalToWorld.Inverse();
	Determinant = LocalToWorld.Determinant();

	if(Actor->DrawType == DT_Particle)
	{
        if ( Actor->IsA(AEmitter::StaticClass()) ) // sjs
        {
		    BoundingBox = Cast<AEmitter>(Actor)->BoundingBox;
        }
        else if ( Actor->IsA(AxEmitter::StaticClass()) )
        {
            BoundingBox = Cast<AxEmitter>(Actor)->mBounds;
        }
        else if ( Actor->IsA(AxProcMesh::StaticClass()) )
        {
            BoundingBox = Actor->GetPrimitive()->GetRenderBoundingBox(Actor).TransformBy(LocalToWorld);
        }
		else if( Actor->IsA(AxWeatherEffect::StaticClass()) )
		{
			BoundingBox = ((AxWeatherEffect*)Actor)->Box.ExpandBy( 65535.0f );
		}
		if ( GIsEditor )
		{
			BoundingBox += Actor->Location;
			BoundingBox = BoundingBox.ExpandBy( Abs(Actor->DrawScale) * Actor->DrawScale3D.GetAbsMax() );
		}
		BoundingSphere = FSphere(&BoundingBox.Min,2);
	}
	else if(Actor->GetPrimitive())
	{
		BoundingBox = Actor->GetPrimitive()->GetRenderBoundingBox(Actor).TransformBy(LocalToWorld);
		BoundingSphere = Actor->GetPrimitive()->GetRenderBoundingSphere(Actor).TransformBy(LocalToWorld);
	}
	else
	{
		BoundingBox = FBox(0);
		BoundingBox += Actor->Location;
	}

	AmbientColor = FColor(0,0,0);
	Translucent = RequiresSorting(Actor);

	Revision = Actor->RenderRevision;

	unguard;
}

//
//	FDynamicActor::Render
//
void FDynamicActor::Render(FLevelSceneNode* SceneNode,TList<FDynamicLight*>* Lights,TList<FProjectorRenderInfo*>* Projectors,FRenderInterface* RI)
{
	guard(FDynamicActor::Render);

	PUSH_HIT(SceneNode->Viewport,HActor(Actor));

	if(SceneNode->Viewport->bShowBounds)
	{
		// Render bounding box.

		RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);
		FLineBatcher(RI).DrawBox(BoundingBox,FColor(255,0,0));
        FLineBatcher(RI).DrawBox(PredictedBox,FColor(0,255,0)); 
	}

	Actor->LastRenderTime = Actor->Level->TimeSeconds;
	if(Actor->DrawType == DT_StaticMesh && Actor->StaticMesh)
	{
        // amb --- for debugging
        // Draw Collision Bounds
        if (SceneNode->Viewport->bShowCollisionBounds && (Actor->bCollideActors || Actor->bCollideWorld) ||
            GIsEditor && (SceneNode->Viewport->Actor->ShowFlags & SHOW_ActorRadii) && Actor->bSelected)
        {
            FLineBatcher LineBatcher(RI);
            RI->SetTransform(TT_LocalToWorld, FMatrix::Identity);
            
            // jij ---
            LineBatcher.DrawCylinder( RI, 
                                      Actor->Location, 
                                      FVector(1,0,0),
                                      FVector(0,1,0),
                                      FVector(0,0,1),
                                      Actor->IsA(AMover::StaticClass()) ? FColor(255,0,255) : FColor(0,255,255),
                                      Actor->CollisionRadius,
                                      Actor->CollisionHeight,
                                      16
                                    );
            // --- jij
        }
        // --- amb

		RenderStaticMesh(this,SceneNode,Lights,Projectors,RI);
	}
    // sjs ---
    else if(Actor->IsA( AxEmitter::StaticClass() ) )
    {
        ((AxEmitter*)Actor)->Render( SceneNode, RI );
        if(GIsEditor && Actor->Texture)
		    DrawSprite(Actor,SceneNode,RI);
    }
    else if(Actor->IsA( AxProcMesh::StaticClass() ) )
    {
        ((AxProcMesh*)Actor)->Render( SceneNode, Lights, RI );
        if(GIsEditor && Actor->Texture)
		    DrawSprite(Actor,SceneNode,RI);
    }
	else if(Actor->IsA( AxWeatherEffect::StaticClass() ) )
    {
        ((AxWeatherEffect*)Actor)->Render( SceneNode, RI );
        if(GIsEditor && Actor->Texture)
		    DrawSprite(Actor,SceneNode,RI);
    }
    // --- sjs
	else if(Actor->DrawType == DT_Mesh && Actor->Mesh)
	{
        // amb --- for debugging
        // Draw Collision Bounds
        if (SceneNode->Viewport->bShowCollisionBounds && (Actor->bCollideActors || Actor->bCollideWorld) ||
            GIsEditor && (SceneNode->Viewport->Actor->ShowFlags & SHOW_ActorRadii) && Actor->bSelected)
        {
            FLineBatcher LineBatcher(RI);
            RI->SetTransform(TT_LocalToWorld, FMatrix::Identity);
            
            // jij ---
            LineBatcher.DrawCylinder( RI, 
                                      Actor->Location, 
                                      FVector(1,0,0),
                                      FVector(0,1,0),
                                      FVector(0,0,1),
                                      Actor->IsA(AMover::StaticClass()) ? FColor(255,0,255) : FColor(0,255,255),
                                      Actor->CollisionRadius,
                                      Actor->CollisionHeight,
                                      16
                                    );
            // --- jij
        }
        // --- amb
		
		APawn *P = Actor->GetAPawn();
		if ( P && P->Texture && P->bDrawCorona )
		{
			FLOAT RealScale = Actor->DrawScale;
			BYTE RealStyle = Actor->Style;
			FVector RealScale3D = Actor->DrawScale3D;
			FLOAT Dist = FDist(SceneNode->ViewOrigin, Actor->Location);
			Actor->DrawScale3D = appSqrt(::Min(2500.f,Dist) * 0.0004f) * 0.02f * Actor->CollisionHeight * FVector(0.45f,1.f,0.f);
			Actor->DrawScale = 1.f;
			FVector RealLoc = Actor->Location;
			Actor->Location = Actor->Location + 0.8f * Actor->CollisionRadius * (Actor->Location - SceneNode->ViewOrigin)/Dist;
			Actor->Style=STY_Additive;
			Actor->DrawType = DT_Sprite;
			DrawSprite(Actor,SceneNode,RI);
			Actor->Location = RealLoc;
			Actor->DrawType = DT_Mesh;
			Actor->DrawScale = RealScale;
			Actor->Style = RealStyle;
			Actor->DrawScale3D = RealScale3D;
		}

		// Draw mesh.
		Actor->Mesh->MeshGetInstance(Actor)->Render(this,SceneNode,Lights,Projectors,RI);			

		// Draw Skeletal-mesh specific attachments. - universal recursive support.
		USkeletalMesh *SkelMesh = Cast<USkeletalMesh>(Actor->Mesh);
		if ( SkelMesh )
		{
		    for(INT i=0; i< Actor->Attached.Num();i++)
		    {
			    AActor* AttachedActor = Actor->Attached(i);
				if ( AttachedActor && !AttachedActor->bDeleteMe && AttachedActor->AttachmentBone != NAME_None )
				{
					// Temporarily set the bone to NAME_None to draw this attachment at its attached location/rotation.
					FName StoreBone = AttachedActor->AttachmentBone;
					AttachedActor->AttachmentBone = NAME_None;
					UBOOL bShouldDrawAttachment = SceneNode->FilterActor(AttachedActor); 
					AttachedActor->AttachmentBone = StoreBone;

			        if( bShouldDrawAttachment )
			            bShouldDrawAttachment = SceneNode->FilterAttachment(AttachedActor) 
												&& ((AttachedActor->CullDistance <= 0.f) || SceneNode->Viewport->Precaching
													|| (Square(AttachedActor->CullDistance) >= (SceneNode->ViewOrigin - Actor->Location).SizeSquared() 
														* Square(SceneNode->Viewport->Actor->FOVBias))); 

					if ( bShouldDrawAttachment && SkelMesh->SetAttachmentLocation(Actor, AttachedActor) )
					{
						AttachedActor->ClearRenderData(); 
				        AttachedActor->GetActorRenderData()->Render(SceneNode,Lights,Projectors,RI);
					}
                }
            }
        }
        else // jjs - base class attachment - for deca
        {
            for(INT i=0; i< Actor->Attached.Num();i++)
		    {
                UBOOL bAttachSuccess = 0;
			    AActor* AttachedActor = Actor->Attached(i);
                if ( AttachedActor->AttachmentBone != NAME_None )
				{
					FMatrix attachMat;

					if ( Actor->Mesh->MeshGetInstance(Actor)->GetAttachMatrix(AttachedActor->AttachmentBone, attachMat) )
					{
                        FCoords  rotCoords = attachMat.Coords() * (GMath.UnitCoords / AttachedActor->RelativeRotation);

                        // Handle location and rotation separately 
						if( AttachedActor->bCollideActors && AttachedActor->GetLevel()->Hash )  
							AttachedActor->GetLevel()->Hash->RemoveActor( AttachedActor );

                        AttachedActor->Location = attachMat.TransformFVector( AttachedActor->RelativeLocation );
                        AttachedActor->Rotation = rotCoords.Transpose().OrthoRotation();

						if( AttachedActor->bCollideActors && AttachedActor->GetLevel()->Hash )
							AttachedActor->GetLevel()->Hash->AddActor( AttachedActor );
						
						bAttachSuccess = true;
					}
                    // Temporarily set the bone to NAME_None to draw this attachment at its attached location/rotation.
			        FName StoreBone = AttachedActor->AttachmentBone;
			        AttachedActor->AttachmentBone = NAME_None;
			        if( bAttachSuccess && SceneNode->FilterAttachment(AttachedActor) ) // More dynamicActor setup ?
			        {
				        //debugf(TEXT("Rendering sprite actor: %s"),AttachedSprite.Actor->GetName() );
                        AttachedActor->GetActorRenderData()->Render(SceneNode,Lights,Projectors,RI);
			        }
			        else
			        {
				        //debugf(TEXT("Unable to render attached sprite actor: %s"), AttachedSprite.Actor->GetName() );
			        }
			        AttachedActor->AttachmentBone = StoreBone;
				}
            } // - jjs
        }
	}
	else if(Actor->DrawType == DT_Brush && Actor->IsA(ABrush::StaticClass()) && Actor->Brush)
		Actor->Brush->Render(this,SceneNode,RI);
	else if(Actor->DrawType == DT_Sprite && Actor->Texture && SceneNode->Viewport->Actor->RendMap!=REN_MatineePreview)
    {
		DrawSprite(Actor,SceneNode,RI);
        // Draw Collision Bounds
        if (SceneNode->Viewport->bShowCollisionBounds && (Actor->bCollideActors || Actor->bCollideWorld) ||
            GIsEditor && (SceneNode->Viewport->Actor->ShowFlags & SHOW_ActorRadii) && Actor->bSelected)
        {
            FLineBatcher LineBatcher(RI);
            RI->SetTransform(TT_LocalToWorld, FMatrix::Identity);
            
            LineBatcher.DrawCylinder( RI, 
                                      Actor->Location, 
                                      FVector(1,0,0),
                                      FVector(0,1,0),
                                      FVector(0,0,1),
                                      Actor->IsA(AMover::StaticClass()) ? FColor(255,0,255) : FColor(0,255,255),
                                      Actor->CollisionRadius,
                                      Actor->CollisionHeight,
                                      16
                                    );
        }
    }
	else if(Actor->DrawType == DT_Particle)
	{
		AEmitter* Emitter = Cast<AEmitter>(Actor);
		if ( Emitter )
		{
			Emitter->Render(this,SceneNode,Lights,RI);
			if(GIsEditor && Actor->Texture)
				DrawSprite(Actor,SceneNode,RI);
		}
		else 
			Actor->DrawType = DT_None;
	}
	else if(Actor->DrawType == DT_FluidSurface && Actor->IsA(AFluidSurfaceInfo::StaticClass()) )
		Cast<AFluidSurfaceInfo>(Actor)->Render(this, SceneNode, Lights, RI);

	// AntiPortal
	if( (Actor->DrawType == DT_AntiPortal) && Actor->AntiPortal 
		&& ((GIsEditor && !(SceneNode->Viewport->Actor->ShowFlags & SHOW_PlayerCtrl))
		|| (!GIsEditor && (SceneNode->Viewport->Actor->ShowFlags & SHOW_Volumes))))
	{
		FLineBatcher	LineBatcher(RI,0);
		FColor			LineColor = Actor->bSelected ? FColor(255,128,0) : FColor(128,64,0);

		RI->SetTransform(TT_LocalToWorld,LocalToWorld);

		for(INT FaceIndex = 0;FaceIndex < Actor->AntiPortal->Faces.Num();FaceIndex++)
		{
			FConvexVolumeFace*	Face = &Actor->AntiPortal->Faces(FaceIndex);

			for(INT VertexIndex = 0;VertexIndex < Face->Vertices.Num();VertexIndex++)
				LineBatcher.DrawLine(Face->Vertices(VertexIndex),Face->Vertices((VertexIndex + 1) % Face->Vertices.Num()),LineColor);
		}
	}

	if(GIsEditor )
		Actor->RenderEditorInfo(SceneNode,RI,this);

    // amb --- send a notification that this actor was rendered
    if( SceneNode->FilterAttachment(AActor::StaticClass()->GetDefaultActor()) ) // sjs - hack hack hack
        Actor->PostRender(SceneNode, RI);
    // --- amb

	POP_HIT(SceneNode->Viewport);

	unguard;
}

void AEmitter::RenderEditorInfo(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA)
{
	guard(AEmitter::RenderEditorInfo);

	Super::RenderEditorInfo(SceneNode,RI,FDA);
	if( Texture )
		DrawSprite(this,SceneNode,RI);
	unguard;
}

void AFluidSurfaceInfo::RenderEditorInfo(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA)
{
	guard(AFluidSurfaceInfo::RenderEditorInfo);

	Super::RenderEditorInfo(SceneNode,RI,FDA);
	if( Texture )
		DrawSprite(this,SceneNode,RI);
	unguard;
}

//
//	FLevelSceneNode::FLevelSceneNode
//
FLevelSceneNode::FLevelSceneNode(UViewport* InViewport,FRenderTarget* InRenderTarget) : FSceneNode(InViewport,InRenderTarget)
{
	Level = Viewport->Actor->XLevel;
	Model = Level->Model;
	ViewZone = INDEX_NONE;
	InvisibleZone = INDEX_NONE;
	ViewActor = NULL;

	StencilMask = 0;

	// If necessary, build model render data.
	if(!Model->Sections.Num())
		Model->BuildRenderData();

	if(!Level->BuiltRenderData)
		Level->BuildRenderData();
}

//
//	FLevelSceneNode::FLevelSceneNode
//
FLevelSceneNode::FLevelSceneNode(FLevelSceneNode* InParent,INT InViewZone,FMatrix LocalToParent) : FSceneNode(InParent)
{
	Level			= InParent->Level;
	Model			= InParent->Model;
	ViewZone		= InViewZone;
	InvisibleZone	= INDEX_NONE;
	ViewActor		= NULL;

	StencilMask		= Viewport->NextStencilMask;
	Viewport->NextStencilMask <<= 1;

	FMatrix	ParentToLocal = LocalToParent.Inverse();

	WorldToCamera	= LocalToParent * Parent->WorldToCamera;
	CameraToWorld	= Parent->CameraToWorld * ParentToLocal;

	WorldToScreen	= WorldToCamera * CameraToScreen;
	ScreenToWorld	= ScreenToCamera * CameraToWorld;

	Determinant		= WorldToScreen.Determinant();

	ViewOrigin		= ParentToLocal.TransformFVector(Parent->ViewOrigin);

	CameraX			= ScreenToWorld.TransformNormal(FVector(2.0f / Viewport->SizeX,0,0));
	CameraY			= ScreenToWorld.TransformNormal(FVector(0,2.0f / Viewport->SizeY,0));
}

//
//	FLevelSceneNode::~FLevelSceneNode
//
FLevelSceneNode::~FLevelSceneNode()
{
}

//
//	FLevelSceneNode::GetViewFrustum
//
FConvexVolume FLevelSceneNode::GetViewFrustum()
{
	FConvexVolume	ViewFrustum;
	FLOAT			Determinant = WorldToScreen.Determinant();

	if(Viewport->IsOrtho())
	{
		FVector	ViewSides[2][2][2];
		FLOAT	TempSigns[2] = { -1.0f, 1.0f };

		for(INT X = 0;X < 2;X++)
			for(INT Y = 0;Y < 2;Y++)
				for(INT Z = 0;Z < 2;Z++)
					ViewSides[X][Y][Z] = Deproject(FPlane(TempSigns[X],TempSigns[Y],TempSigns[Z],1.0f));

		if(Determinant < 0.0f)
		{
			ViewFrustum.BoundingPlanes[0] = FPlane(ViewSides[0][0][0],ViewSides[0][0][1],ViewSides[1][0][1]);
			ViewFrustum.BoundingPlanes[1] = FPlane(ViewSides[1][0][0],ViewSides[1][0][1],ViewSides[1][1][1]);
			ViewFrustum.BoundingPlanes[2] = FPlane(ViewSides[1][1][0],ViewSides[1][1][1],ViewSides[0][1][1]);
			ViewFrustum.BoundingPlanes[3] = FPlane(ViewSides[0][1][0],ViewSides[0][1][1],ViewSides[0][0][1]);
		}
		else
		{
			ViewFrustum.BoundingPlanes[0] = FPlane(ViewSides[0][0][0],ViewSides[1][0][1],ViewSides[0][0][1]);
			ViewFrustum.BoundingPlanes[1] = FPlane(ViewSides[1][0][0],ViewSides[1][1][1],ViewSides[1][0][1]);
			ViewFrustum.BoundingPlanes[2] = FPlane(ViewSides[1][1][0],ViewSides[0][1][1],ViewSides[1][1][1]);
			ViewFrustum.BoundingPlanes[3] = FPlane(ViewSides[0][1][0],ViewSides[0][0][1],ViewSides[0][1][1]);
		}

		ViewFrustum.NumPlanes = 4;
	}
	else
	{
		FVector	ViewSides[4];
		FLOAT	TempSigns[2] = { -1.0f, 1.0f };

		for(INT X = 0;X < 2;X++)
			for(INT Y = 0;Y < 2;Y++)
				ViewSides[X * 2 + Y] = Deproject(FPlane(TempSigns[X],TempSigns[Y],0.0f,NEAR_CLIPPING_PLANE));

		if(Determinant < 0.0f)
		{
			ViewFrustum.BoundingPlanes[0] = FPlane(ViewOrigin,ViewSides[1],ViewSides[0]);
			ViewFrustum.BoundingPlanes[1] = FPlane(ViewOrigin,ViewSides[0],ViewSides[2]);
			ViewFrustum.BoundingPlanes[2] = FPlane(ViewOrigin,ViewSides[2],ViewSides[3]);
			ViewFrustum.BoundingPlanes[3] = FPlane(ViewOrigin,ViewSides[3],ViewSides[1]);
		}
		else
		{
			ViewFrustum.BoundingPlanes[0] = FPlane(ViewOrigin,ViewSides[0],ViewSides[1]);
			ViewFrustum.BoundingPlanes[1] = FPlane(ViewOrigin,ViewSides[2],ViewSides[0]);
			ViewFrustum.BoundingPlanes[2] = FPlane(ViewOrigin,ViewSides[3],ViewSides[2]);
			ViewFrustum.BoundingPlanes[3] = FPlane(ViewOrigin,ViewSides[1],ViewSides[3]);
		}

		if(!Viewport->IsWire())
		{
			// Calculate the far clipping plane.

			AZoneInfo*	ViewZoneInfo = Level->GetZoneActor(ViewZone);
			FLOAT		FarClip = (ViewZoneInfo->bDistanceFog && (Viewport->Actor->ShowFlags & SHOW_DistanceFog)) ? Max(ViewZoneInfo->DistanceFogEnd,Viewport->Actor->CurrentDistanceFogEnd) : FAR_CLIPPING_PLANE;
			FVector		Z = (Deproject(FPlane(0,0,0,NEAR_CLIPPING_PLANE)) - ViewOrigin).SafeNormal();

			ViewFrustum.BoundingPlanes[4] = FPlane(ViewOrigin + Z * FarClip,Z);
			ViewFrustum.NumPlanes = 5;
		}
		else
			ViewFrustum.NumPlanes = 4;
	}

	return ViewFrustum;
}

#ifdef __PSX2_EE__
//
//	FLevelSceneNode::GetOverflowFrustum
//
FConvexVolume FLevelSceneNode::GetOverflowFrustum()
{
	FConvexVolume	ViewFrustum;
	FLOAT			Determinant = WorldToScreen.Determinant();

	if(Viewport->IsOrtho())
	{
		FVector	ViewSides[2][2][2];
		FLOAT	TempSigns[2] = { -1.0f, 1.0f };

		for(INT X = 0;X < 2;X++)
			for(INT Y = 0;Y < 2;Y++)
				for(INT Z = 0;Z < 2;Z++)
					ViewSides[X][Y][Z] = Deproject(FPlane(TempSigns[X],TempSigns[Y],TempSigns[Z],1.0f));

		if(Determinant < 0.0f)
		{
			ViewFrustum.BoundingPlanes[0] = FPlane(ViewSides[0][0][0],ViewSides[0][0][1],ViewSides[1][0][1]);
			ViewFrustum.BoundingPlanes[1] = FPlane(ViewSides[1][0][0],ViewSides[1][0][1],ViewSides[1][1][1]);
			ViewFrustum.BoundingPlanes[2] = FPlane(ViewSides[1][1][0],ViewSides[1][1][1],ViewSides[0][1][1]);
			ViewFrustum.BoundingPlanes[3] = FPlane(ViewSides[0][1][0],ViewSides[0][1][1],ViewSides[0][0][1]);
		}
		else
		{
			ViewFrustum.BoundingPlanes[0] = FPlane(ViewSides[0][0][0],ViewSides[1][0][1],ViewSides[0][0][1]);
			ViewFrustum.BoundingPlanes[1] = FPlane(ViewSides[1][0][0],ViewSides[1][1][1],ViewSides[1][0][1]);
			ViewFrustum.BoundingPlanes[2] = FPlane(ViewSides[1][1][0],ViewSides[0][1][1],ViewSides[1][1][1]);
			ViewFrustum.BoundingPlanes[3] = FPlane(ViewSides[0][1][0],ViewSides[0][0][1],ViewSides[0][1][1]);
		}

		ViewFrustum.NumPlanes = 4;
	}
	else
	{
		FVector	ViewSides[4];
		FLOAT	TempSigns[2] = { -1.4f, 1.4f };
//		FLOAT	TempSigns[2] = { -2.8f, 2.8f };

		for(INT X = 0;X < 2;X++)
			for(INT Y = 0;Y < 2;Y++)
				ViewSides[X * 2 + Y] = Deproject(FPlane(TempSigns[X],TempSigns[Y],0.0f,NEAR_CLIPPING_PLANE));

		if(Determinant < 0.0f)
		{
			ViewFrustum.BoundingPlanes[0] = FPlane(ViewOrigin,ViewSides[1],ViewSides[0]);
			ViewFrustum.BoundingPlanes[1] = FPlane(ViewOrigin,ViewSides[0],ViewSides[2]);
			ViewFrustum.BoundingPlanes[2] = FPlane(ViewOrigin,ViewSides[2],ViewSides[3]);
			ViewFrustum.BoundingPlanes[3] = FPlane(ViewOrigin,ViewSides[3],ViewSides[1]);
		}
		else
		{
			ViewFrustum.BoundingPlanes[0] = FPlane(ViewOrigin,ViewSides[0],ViewSides[1]);
			ViewFrustum.BoundingPlanes[1] = FPlane(ViewOrigin,ViewSides[2],ViewSides[0]);
			ViewFrustum.BoundingPlanes[2] = FPlane(ViewOrigin,ViewSides[3],ViewSides[2]);
			ViewFrustum.BoundingPlanes[3] = FPlane(ViewOrigin,ViewSides[1],ViewSides[3]);
		}

		ViewFrustum.NumPlanes = 4;
	}

	return ViewFrustum;
}
#endif // __PSX2_EE__
//
//	FLevelSceneNode::FilterActor
//
UBOOL FLevelSceneNode::FilterActor(AActor* Actor)
{
	if(!(Viewport->Actor->ShowFlags & SHOW_Actors) && !Actor->IsA(ABrush::StaticClass()))
		return 0;

	//WD: rearranged if() statement arguments
	if(!(Viewport->Actor->ShowFlags & SHOW_StaticMeshes) && Actor->StaticMesh)
			return 0;

	//WD: rearranged if() statement arguements
	if(!(Viewport->Actor->ShowFlags & SHOW_FluidSurfaces) && Actor->IsA(AFluidSurfaceInfo::StaticClass()))
			return 0;

	// Draw blocking volumes if SHOW_Collision or KDRAW_Collision is set.
	if( (Viewport->Actor->ShowFlags & SHOW_Collision) && Actor->IsA(ABlockingVolume::StaticClass()) )
		return 1;

	//WD: changed if(GIsEditor) to if(!GIsEditor), and swapped instruction blocks
	if( !GIsEditor )
	{
		if((Actor->bHighDetail && Actor->Level->DetailMode == DM_Low) || (Actor->bSuperHighDetail && Actor->Level->DetailMode != DM_SuperHigh))
			return 0;

		if( GIsPixomatic && (Actor->bHighDetail || Actor->bSuperHighDetail) )
			return 0;

		if(Actor->bCorona)
		{
			Actor->DrawType = DT_None;
			return 1;
		}
		else
		{
			if((Viewport->Actor->ShowFlags & SHOW_Volumes) && Actor->IsVolumeBrush())
				return 1;
			else if(Actor->bHidden)
				return 0;
			else if ( Actor->bOnlyDrawIfAttached && !Actor->Base )
				return 0;

			if ( Actor->IsOwnedBy(ViewActor) ? (Actor->bOwnerNoSee && !Viewport->Actor->bBehindView) : Actor->bOnlyOwnerSee )
				return 0;
		}
	}
	else
	{
		if( (Actor->IsVolumeBrush() || Cast<AAntiPortalActor>(Actor)) && !(Viewport->Actor->ShowFlags & SHOW_Volumes))
			return 0;

		if(Actor->IsMovingBrush() && !(Viewport->Actor->ShowFlags & SHOW_MovingBrushes))
			return 0;

		if( Actor->IsHiddenEd() )
			return 0;
	}

	// If attached to a base with a bone, an actor is only drawn along with its base.
	if( Actor->Base && (Actor->AttachmentBone != NAME_None) && Cast<USkeletalMesh>(Actor->Base->Mesh) ) 
		return 0;

	return 1;
}


//
//	FLevelSceneNode::FilterProjector
//

UBOOL FLevelSceneNode::FilterProjector(AProjector* Actor)
{
    if( !Actor ) 
        return 1;

	if(Viewport->IsWire())
		return 0;

	if ( Actor->IsOwnedBy(ViewActor) ? (Actor->bOwnerNoSee && !Viewport->Actor->bBehindView) : Actor->bOnlyOwnerSee )
		return 0;

    // sjs - distance cull projectors
    float effectiveDistSqr = (Actor->Location-ViewOrigin).SizeSquared() * Square(Viewport->Actor->FOVBias);
    if( Actor->CullDistance > 0.0f && effectiveDistSqr > Square(Actor->CullDistance) )
    {
        return 0;
    }

	if((Actor->bHighDetail && Actor->Level->DetailMode == DM_Low) || (Actor->bSuperHighDetail && Actor->Level->DetailMode != DM_SuperHigh))
		return 0;

	return 1;
}

//
//  FLevelSceneNode::Render
//

void FLevelSceneNode::Render(FRenderInterface* RI)
{
	guard(FLevelSceneNode::Render);

	DECLARE_STATIC_UOBJECT(
		UProxyBitmapMaterial,
		HackMaterial,
		{
			static FSolidColorTexture	BlackTexture(FColor(0,0,0));
			HackMaterial->SetTextureInterface(&BlackTexture);
		}
		);

	DECLARE_STATIC_UOBJECT(
		UFinalBlend,
		StencilFinalBlend,
		{
			StencilFinalBlend->Material = HackMaterial;
			StencilFinalBlend->FrameBufferBlending = FB_Invisible;
			StencilFinalBlend->ZWrite = 1;
			StencilFinalBlend->ZTest = 0;
		}
		);

	RI->PushState();

	if( StencilMask && (GIsEditor || Viewport->RenDev->UseStencil) )
	{
		RI->SetStencilOp(CF_NotEqual,StencilMask,StencilMask,SO_Keep,SO_Keep,SO_Increment,DEPTH_COMPLEXITY_MASK(Viewport));

		FCanvasUtil(&Viewport->RenderTarget,RI).DrawTile(
			0,0,
			Viewport->SizeX,Viewport->SizeY,
			0,0,
			0,0,
			1,
			StencilFinalBlend,
			FColor(0,0,0)
			);
	}

	// Calculate distance fog.
	UBOOL bVolumeFog = 0;

	if ( !Parent && Viewport->Actor )
	{
		APhysicsVolume *V = Viewport->Actor->Level->GetPhysicsVolume(Viewport->Actor->CalcViewLocation, NULL, false);
		if ( V && V->bDistanceFog )
		{
			UBOOL bShowFog = GEdShowFogInViewports && !Viewport->IsWire();
			bVolumeFog = true;
			RI->SetDistanceFog( bShowFog, V->DistanceFogStart, V->DistanceFogEnd, V->DistanceFogColor );
		}
	}

	if(!bVolumeFog)
	{
		AZoneInfo*			ViewZoneInfo	= Level->GetZoneActor(ViewZone);
		APlayerController*	Player			= Viewport->Actor;

		// Interpolate between fog zones.
		if( !Parent && Viewport->IsRealtime() )
		{
			// Handle zone changes.
			if( Player->LastZone != ViewZone )
			{
				// Use correct fog color if transitioning from a zone without fog.
				if( ViewZoneInfo->bDistanceFog && (Player->LastDistanceFogStart == FAR_CLIPPING_PLANE) )
					Player->LastDistanceFogColor = ViewZoneInfo->DistanceFogColor;

				// Don't transition out of zone 0
				if( Player->LastZone == 0 )
					Player->TimeSinceLastFogChange	= 1000000.f; //!! can't use FLT_MAX as value will be increased.
				else
					Player->TimeSinceLastFogChange	= 0.f;

				Player->LastZone = ViewZone;
			}

			FLOAT	Alpha = ViewZoneInfo->DistanceFogBlendTime ? Player->TimeSinceLastFogChange / ViewZoneInfo->DistanceFogBlendTime : 1.f;

			FLOAT	WantedDistanceFogStart,
					WantedDistanceFogEnd;
			FColor	WantedDistanceFogColor;

			// Handle case of transitioning from a fog zone to one without fog.
			if( ViewZoneInfo->bDistanceFog )
			{
				WantedDistanceFogStart	= ViewZoneInfo->DistanceFogStart;
				WantedDistanceFogEnd	= ViewZoneInfo->DistanceFogEnd;
				WantedDistanceFogColor	= ViewZoneInfo->DistanceFogColor;
			}
			else
			{
				WantedDistanceFogStart	= FAR_CLIPPING_PLANE;
				WantedDistanceFogEnd	= FAR_CLIPPING_PLANE;
				WantedDistanceFogColor	= Player->LastDistanceFogColor;
			}

			// Blend completed - also, don't transition into zone 0.
			if( Alpha > 1.f || ViewZone == 0 )
			{
				Player->LastDistanceFogStart	= WantedDistanceFogStart;
				Player->LastDistanceFogEnd		= WantedDistanceFogEnd;
				Player->LastDistanceFogColor	= WantedDistanceFogColor;
				Player->TimeSinceLastFogChange	= 1000000.f; //!! can't use FLT_MAX as value will be increased.
				Alpha = 1.f;
			}

			// Lerp between fog state.
			FLOAT	DistanceFogStart	= Lerp( Player->LastDistanceFogStart, WantedDistanceFogStart, Alpha ),
					DistanceFogEnd		= Lerp( Player->LastDistanceFogEnd  , WantedDistanceFogEnd  , Alpha );
			FColor	DistanceFogColor;	// Can't lerp directly as it will cast to DWORD and lerp there.
			
			DistanceFogColor.R = Lerp( Player->LastDistanceFogColor.R, WantedDistanceFogColor.R, Alpha );
			DistanceFogColor.G = Lerp( Player->LastDistanceFogColor.G, WantedDistanceFogColor.G, Alpha );
			DistanceFogColor.B = Lerp( Player->LastDistanceFogColor.B, WantedDistanceFogColor.B, Alpha );
			DistanceFogColor.A = 255;

			// Keep track of current max fog distance for culling.
			Player->CurrentDistanceFogEnd = DistanceFogEnd;

			UBOOL UseFog = ViewZoneInfo->bDistanceFog || Alpha < 1.f;
			RI->SetDistanceFog(UseFog && (Viewport->Actor->ShowFlags & SHOW_DistanceFog),DistanceFogStart,DistanceFogEnd,DistanceFogColor);
		}
		else
		{
			RI->SetDistanceFog(ViewZoneInfo->bDistanceFog && (Viewport->Actor->ShowFlags & SHOW_DistanceFog),ViewZoneInfo->DistanceFogStart,ViewZoneInfo->DistanceFogEnd,ViewZoneInfo->DistanceFogColor);
		}

	}

	RenderLevel(this,RI);

#ifdef WITH_KARMA
	UBOOL bDrawConstraints = false;
	if(KGData)
	{
		if( KGData->DebugDrawOpt & KDRAW_Contacts )
			bDrawConstraints = true;

		if( KGData->DebugDrawOpt & KDRAW_Limits )
			bDrawConstraints = true;

		if( KGData->DebugDrawOpt & KDRAW_Axis )
			bDrawConstraints = true;
	}

	if(!Parent && Level->KWorld && bDrawConstraints)
    {
		// go through world drawing constraints
		// done here instead of tick, so we get contacts etc. even if Karma is paused etc.
		KLevelDebugDrawConstraints(Level);
	}
#endif

	if( StencilMask && (GIsEditor || Viewport->RenDev->UseStencil) )
	{
		FCanvasUtil(&Viewport->RenderTarget,RI).DrawTile(
			0,0,
			Viewport->SizeX,Viewport->SizeY,
			0,0,
			0,0,
			1,
			StencilFinalBlend,
			FColor(0,0,0)
			);
	}

	// Render lines from elsewhere (collision debugging etc.)
	if(GTempLineBatcher)
	{
		RI->PushState();
		RI->SetTransform(TT_WorldToCamera,WorldToCamera);
		RI->SetTransform(TT_CameraToScreen,CameraToScreen);
		GTempLineBatcher->Render(RI);
		RI->PopState();
	}


	if ( Viewport->Actor->myHUD )
		Viewport->Actor->myHUD->eventWorldSpaceOverlays();

	// Render the graph
	if(GStatGraph)
	GStatGraph->Render(Viewport, RI);

	RI->PopState();

	unguard;
}

/*-----------------------------------------------------------------------------
	ufront AutoFOV (step 2.16).
-----------------------------------------------------------------------------*/

ENGINE_API FLOAT GAutoFovFrameBase = 90.0f;

// Parsed once on first use: config is authoritative, the command line overrides it. Re-reading per
// frame would be wasted work — the values only change across a launch.
static UBOOL  GAutoFovParsed  = 0;
static INT    GAutoFovMode    = AUTOFOV_Adaptive;
static FLOAT  GAutoFovRef     = 4.0f / 3.0f;
static FLOAT  GAutoFovWeapon  = 1.0f;
static INT    GAutoFovLastX    = -1;
static INT    GAutoFovLastY    = -1;
static FLOAT  GAutoFovLastBase = -1.0f;
static INT    GAutoFovUi       = 1;
static FLOAT  GAutoFovUiScale  = 1.0f;
static FLOAT  GAutoFovUiLastLog = -1.0f;

static void ufrontParseAutoFov()
{
	guard(ufrontParseAutoFov);
	GAutoFovParsed = 1;

	// [ufront.AutoFOV] is a plain config section, not a class — deliberately. The override has to be read
	// from UGameEngine::Draw (Engine), while the natural "display settings" home is the client class,
	// which lives in a driver package Engine cannot reach; and adding properties to a retail-Engine.u
	// class is not an option. A free section is readable anywhere, is spelled identically in all four
	// engines (so it stays DATA rather than per-engine code), and is one obvious block for a human to edit.
	INT   Mode = GAutoFovMode;
	FLOAT Ref  = GAutoFovRef;
	FLOAT Weap = GAutoFovWeapon;
	INT   Ui   = GAutoFovUi;
	FLOAT UiSc = GAutoFovUiScale;
	GConfig->GetInt  ( TEXT("ufront.AutoFOV"), TEXT("AutoFOV"),         Mode );
	GConfig->GetFloat( TEXT("ufront.AutoFOV"), TEXT("ReferenceAspect"), Ref  );
	GConfig->GetFloat( TEXT("ufront.AutoFOV"), TEXT("WeaponScale"),     Weap );
	GConfig->GetInt  ( TEXT("ufront.AutoFOV"), TEXT("AutoFOVUI"),       Ui   );
	GConfig->GetFloat( TEXT("ufront.AutoFOV"), TEXT("UIScale"),         UiSc );

	// Command line wins (lets the E2E drive both modes off ONE build, and makes a bad config recoverable).
	if( appStrfind( appCmdLine(), TEXT("NOAUTOFOV") ) )
		Mode = AUTOFOV_Off;
	const TCHAR* Str;
	if( (Str = appStrfind( appCmdLine(), TEXT("AUTOFOV=") )) != NULL )
		Mode = appAtoi( Str + 8 );
	if( (Str = appStrfind( appCmdLine(), TEXT("AUTOFOVREF=") )) != NULL )
		Ref = appAtof( Str + 11 );
	if( (Str = appStrfind( appCmdLine(), TEXT("AUTOFOVWEAPON=") )) != NULL )
		Weap = appAtof( Str + 14 );
	if( appStrfind( appCmdLine(), TEXT("NOAUTOFOVUI") ) )
		Ui = 0;
	if( (Str = appStrfind( appCmdLine(), TEXT("AUTOFOVUISCALE=") )) != NULL )
		UiSc = appAtof( Str + 15 );

	GAutoFovMode   = Clamp( Mode, (INT)AUTOFOV_Off, (INT)AUTOFOV_Adaptive );
	GAutoFovRef    = (Ref  > 0.01f) ? Ref : (4.0f / 3.0f);
	GAutoFovWeapon = Clamp( Weap, 0.0f, 4.0f );
	GAutoFovUi      = Clamp( Ui, 0, 1 );
	GAutoFovUiScale = Clamp( UiSc, 0.1f, 4.0f );

	debugf( NAME_Log, TEXT("[AutoFOV] mode=%i referenceAspect=%f weaponScale=%f ui=%i uiScale=%f"),
		GAutoFovMode, GAutoFovRef, GAutoFovWeapon, GAutoFovUi, GAutoFovUiScale );
	unguard;
}

ENGINE_API FLOAT ufrontEffectiveFov( FLOAT BaseFov, INT SizeX, INT SizeY, UBOOL bLog )
{
	guard(ufrontEffectiveFov);
	if( !GAutoFovParsed )
		ufrontParseAutoFov();

	if( GAutoFovMode == AUTOFOV_Off || SizeX <= 0 || SizeY <= 0 )
		return BaseFov;

	// Guard the trig: a degenerate or hostile FOV must not produce a broken projection.
	FLOAT Base = Clamp( BaseFov, 1.0f, 170.0f );
	FLOAT Aspect = (FLOAT)SizeX / (FLOAT)SizeY;
	FLOAT HorPlus = (FLOAT)( appAtan( appTan( Base * PI / 360.0 ) * ( Aspect / GAutoFovRef ) ) * 360.0 / PI );
	FLOAT Eff = ( GAutoFovMode == AUTOFOV_Adaptive ) ? Max( Base, HorPlus ) : HorPlus;
	Eff = Clamp( Eff, 1.0f, 170.0f );

	// Log whenever the INPUTS change — the viewport size or the base FOV. Both matter and neither can
	// be inferred from the other: the size is the resize event the E2E asserts on, and the base moves
	// when the game changes the player's FOV (an intro camera, a zoom). Two earlier cuts were wrong:
	// gating on "base == 90" emitted NOTHING on an engine whose stored FOV was not 90, and gating on
	// size alone latched whatever transient FOV the very first drawn frame happened to carry (an intro
	// camera at the 170 clamp), so the first line did not describe gameplay. Zoom still cannot spam it:
	// UE interpolates FovAngle over ~a dozen frames, so a zoom costs a dozen lines, once.
	if( bLog && ( SizeX != GAutoFovLastX || SizeY != GAutoFovLastY || Abs(Base - GAutoFovLastBase) > 0.5f ) )
	{
		GAutoFovLastX    = SizeX;
		GAutoFovLastY    = SizeY;
		GAutoFovLastBase = Base;
		debugf( NAME_Log, TEXT("[AutoFOV] viewport=%ix%i aspect=%f base=%f effective=%f"),
			SizeX, SizeY, Aspect, Base, Eff );
	}
	return Eff;
	unguard;
}

ENGINE_API FLOAT ufrontWeaponFov( FLOAT BaseFov, FLOAT EffectiveFov )
{
	guard(ufrontWeaponFov);
	if( !GAutoFovParsed )
		ufrontParseAutoFov();

	// 1.0 (default) = the overlay rides the corrected FOV, which is what preserves its reference-aspect
	// proportions; callers compare against EffectiveFov and can skip any extra work.
	if( GAutoFovMode == AUTOFOV_Off || Abs(GAutoFovWeapon - 1.0f) < 0.001f )
		return EffectiveFov;

	// Blend in TAN space, not degrees: tangent is the linear one on screen, so 0.5 lands visually halfway.
	FLOAT TanBase = (FLOAT)appTan( Clamp( BaseFov, 1.0f, 170.0f ) * PI / 360.0 );
	FLOAT TanEff  = (FLOAT)appTan( Clamp( EffectiveFov, 1.0f, 170.0f ) * PI / 360.0 );
	FLOAT TanOut  = TanBase + ( TanEff - TanBase ) * GAutoFovWeapon;
	if( TanOut < 0.001f )
		TanOut = 0.001f;
	return Clamp( (FLOAT)( appAtan( TanOut ) * 360.0 / PI ), 1.0f, 170.0f );
	unguard;
}


ENGINE_API FLOAT ufrontHudScaleFactor( INT SizeX, INT SizeY )
{
	guard(ufrontHudScaleFactor);
	if( !GAutoFovParsed )
		ufrontParseAutoFov();

	if( !GAutoFovUi || SizeX <= 0 || SizeY <= 0 )
		return 1.0f;

	FLOAT Aspect = (FLOAT)SizeX / (FLOAT)SizeY;

	// CLAMPED AT 1: only ever shrink. At or below the reference aspect this returns exactly 1.0, so a
	// narrow/tall window renders bit-for-bit as stock -- the stock sizing already shrinks with a taller
	// aspect (it follows ClipX), and preserving that is the point.
	FLOAT Factor = ( Aspect > GAutoFovRef ) ? ( GAutoFovRef / Aspect ) : 1.0f;
	Factor *= GAutoFovUiScale;
	Factor = Clamp( Factor, 0.05f, 4.0f );

	if( Abs(Factor - GAutoFovUiLastLog) > 0.001f )
	{
		GAutoFovUiLastLog = Factor;
		debugf( NAME_Log, TEXT("[AutoFOVUI] viewport=%ix%i aspect=%f hudScale x%f"), SizeX, SizeY, Aspect, Factor );
	}
	return Factor;
	unguard;
}

//
//	FCameraSceneNode::FCameraSceneNode
//
FCameraSceneNode::FCameraSceneNode(UViewport* InViewport,FRenderTarget* InRenderTarget,AActor* CameraActor,FVector CameraLocation, FRotator CameraRotation, FLOAT CameraFOV) : FLevelSceneNode(InViewport,InRenderTarget)
{
	guard(FCameraSceneNode::FCameraSceneNode);

	// Determine which zone the camera is in.
	FPointRegion	CameraZone = Model->PointRegion(CameraActor->Level,CameraLocation);

	ViewOrigin		= CameraLocation;
	ViewActor		= CameraActor;
	ViewZone		= (GIsEditor && Viewport->IsWire()) || Viewport->Precaching ? 0 : Model->PointRegion(ViewActor->Level,ViewOrigin).ZoneNumber;
	ViewRotation	= CameraRotation;
	ViewFOV			= CameraFOV;

	UpdateMatrices();

	unguard;
}

//
//	FCameraSceneNode::UpdateMatrices
//
void FCameraSceneNode::UpdateMatrices()
{
	// Initialize the view matrix.
	WorldToCamera = FTranslationMatrix(-ViewOrigin);

	if(!Viewport->IsOrtho())
		WorldToCamera = WorldToCamera * FInverseRotationMatrix(ViewRotation);

	if(Viewport->Actor->RendMap == REN_OrthXY)
		WorldToCamera = WorldToCamera * FMatrix(
									FPlane(Viewport->ScaleX,	0,					0,					0),
									FPlane(0,					-Viewport->ScaleY,	0,					0),
									FPlane(0,					0,					-1,					0),
									FPlane(0,					0,					-ViewOrigin.Z,		1));
	else if(Viewport->Actor->RendMap == REN_OrthXZ)
		WorldToCamera = WorldToCamera * FMatrix(
									FPlane(Viewport->ScaleX,	0,					0,					0),
									FPlane(0,					0,					-1,					0),
									FPlane(0,					Viewport->ScaleY,	0,					0),
									FPlane(0,					0,					-ViewOrigin.Y,		1));
	else if(Viewport->Actor->RendMap == REN_OrthYZ)
		WorldToCamera = WorldToCamera * FMatrix(
									FPlane(0,					0,					1,					0),
									FPlane(Viewport->ScaleX,	0,					0,					0),
									FPlane(0,					Viewport->ScaleY,	0,					0),
									FPlane(0,					0,					ViewOrigin.X,		1));
	else
		WorldToCamera = WorldToCamera * FMatrix(
									FPlane(0,					0,					1,	0),
									FPlane(Viewport->ScaleX,	0,					0,	0),
									FPlane(0,					Viewport->ScaleY,	0,	0),
									FPlane(0,					0,					0,	1));

	CameraToWorld = WorldToCamera.Inverse();

	// Initialize the projection matrix.
	if(Viewport->IsOrtho())
	{
		FLOAT	Zoom = Viewport->Actor->OrthoZoom / (Viewport->SizeX * 15.0f);

		CameraToScreen = FOrthoMatrix(Zoom * RenderTarget->GetWidth() / 2,Zoom * RenderTarget->GetHeight() / 2,0.5f / HALF_WORLD_MAX,HALF_WORLD_MAX);
	}
	else
	{
		FLOAT FOV = ViewFOV * PI / 360.0f;
		CameraToScreen = FPerspectiveMatrix(
								FOV,
								FOV,
								1.f,
								(FLOAT) RenderTarget->GetWidth() / RenderTarget->GetHeight(),
								NEAR_CLIPPING_PLANE,
								FAR_CLIPPING_PLANE
		);
	}

	ScreenToCamera = CameraToScreen.Inverse();

	WorldToScreen = WorldToCamera * CameraToScreen;
	ScreenToWorld = ScreenToCamera * CameraToWorld;

	Determinant = WorldToScreen.Determinant();

	CameraX = ScreenToWorld.TransformNormal(FVector(2.0f / Viewport->SizeX,0,0));
	CameraY = ScreenToWorld.TransformNormal(FVector(0,2.0f / Viewport->SizeY,0));

#ifdef __GCN__
	CameraToScreen  = FMatrix(FPlane(0.0f, ViewFOV, NEAR_CLIPPING_PLANE, FAR_CLIPPING_PLANE), FPlane(), FPlane(), FPlane());
#endif
}


//
//	FCameraSceneNode::~FCameraSceneNode
//
FCameraSceneNode::~FCameraSceneNode()
{
}

//
//	FCameraSceneNode::Render
//

void FCameraSceneNode::Render(FRenderInterface* RI)
{
	guard(FCameraSceneNode::Render);

	RI->PushState();

	if(Viewport->Actor->RendMap == REN_DepthComplexity)
		RI->SetStencilOp(CF_Always,0,0,SO_Keep,SO_Keep,SO_Increment,DEPTH_COMPLEXITY_MASK(Viewport));

	// Clear the screen.
	if(!GIsEditor && Viewport->IsWire() || !Viewport->Canvas->bRenderLevel) 
		RI->Clear();

	// Render the level.

	FLevelSceneNode::Render(RI);

	// Use the stencil buffer to render depth complexity.

	if(Viewport->Actor->RendMap == REN_DepthComplexity)
	{
		DECLARE_STATIC_UOBJECT(
			UProxyBitmapMaterial,
			HackMaterial,
			{
				static FSolidColorTexture	WhiteTexture(FColor(255,255,255));
				HackMaterial->SetTextureInterface(&WhiteTexture);
			}
			);

		DECLARE_STATIC_UOBJECT(
			UFinalBlend,
			DepthComplexityBlend,
			{
				DepthComplexityBlend->ZTest = 0;
				DepthComplexityBlend->Material = HackMaterial;
			}
			);

		FLOAT	LegendHeight = Viewport->SizeY / 16.0f;

		for(DWORD Stencil = 0;Stencil < 4;Stencil++)
		{
			RI->SetStencilOp(CF_Equal,(Stencil + 1) | ~DEPTH_COMPLEXITY_MASK(Viewport),DEPTH_COMPLEXITY_MASK(Viewport),SO_Keep,SO_Keep,SO_Keep,0);

			FCanvasUtil(&Viewport->RenderTarget,RI).DrawTile(
				0,0,
				Viewport->SizeX - 32,Viewport->SizeY,
				0,0,
				0,0,
				0,
				DepthComplexityBlend,
				FColor(
					Stencil * 64,
					255,
					0
					)
				);

			RI->SetStencilOp(CF_Always,0,0,SO_Keep,SO_Keep,SO_Keep,0);

			FCanvasUtil(&Viewport->RenderTarget,RI).DrawTile(
				Viewport->SizeX - 32,Stencil * LegendHeight,
				Viewport->SizeX,(Stencil + 1) * LegendHeight,
				0,0,
				0,0,
				0.0f,
				DepthComplexityBlend,
				FColor(
					Stencil * 64,
					255,
					0
					)
				);
		}

		for(DWORD Stencil = 4;Stencil < 16;Stencil++)
		{
			RI->SetStencilOp(CF_Equal,(Stencil + 1) | ~DEPTH_COMPLEXITY_MASK(Viewport),DEPTH_COMPLEXITY_MASK(Viewport),SO_Keep,SO_Keep,SO_Keep,0);

			FCanvasUtil(&Viewport->RenderTarget,RI).DrawTile(
				0,0,
				Viewport->SizeX - 32,Viewport->SizeY,
				0,0,
				0,0,
				0,
				DepthComplexityBlend,
				FColor(
					255,
					231 - (Stencil - 4) * 21,
					0
					)
				);

			RI->SetStencilOp(CF_Always,0,0,SO_Keep,SO_Keep,SO_Keep,0);

			FCanvasUtil(&Viewport->RenderTarget,RI).DrawTile(
				Viewport->SizeX - 32,Stencil * LegendHeight,
				Viewport->SizeX,(Stencil + 1) * LegendHeight,
				0,0,
				0,0,
				0.0f,
				DepthComplexityBlend,
				FColor(
					255,
					231 - (Stencil - 4) * 21,
					0
					)
				);
		}
	}

	UpdateVertexPools(Viewport);

	if ( Viewport->Canvas->pCanvasUtil ) 
        Viewport->Canvas->pCanvasUtil->Flush();

	RI->PopState();

	unguard;
}

//
//	FPlayerSceneNode::FPlayerSceneNode
//

FPlayerSceneNode::FPlayerSceneNode(UViewport* InViewport,FRenderTarget* InRenderTarget,AActor* CameraActor,FVector CameraLocation,FRotator CameraRotation,FLOAT CameraFOV) : FCameraSceneNode(InViewport,InRenderTarget,CameraActor,CameraLocation,CameraRotation,CameraFOV)
{
}

//
//	FPlayerSceneNode::Render
//

void FPlayerSceneNode::Render(FRenderInterface* RI)
{
	guard(FPlayerSceneNode::Render);

	APlayerController*	Controller = Viewport->Actor;

	RI->PushState();

	if(Viewport->Actor->RendMap == REN_DepthComplexity)
		RI->SetStencilOp(CF_Always,0,0,SO_Keep,SO_Keep,SO_Increment,DEPTH_COMPLEXITY_MASK(Viewport));

	// Clear the screen.
	if(!GIsEditor && Viewport->IsWire() || !Viewport->Canvas->bRenderLevel) 
		RI->Clear();

	// Update the console.
	Viewport->Canvas->Update();

    if ( Viewport->Canvas->pCanvasUtil ) 
        Viewport->Canvas->pCanvasUtil->Flush();

    // Send all the Interaction PreRenders

	clock(GStats.DWORDStats(GEngineStats.STATS_Game_InteractionPostRender));
    
	if ( Viewport->InteractionMaster )
	    Viewport->InteractionMaster->MasterProcessPreRender(Viewport->Canvas);
	
	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_InteractionPostRender));

    if( Viewport->Canvas->bRenderLevel )
    {
		check( Viewport->Actor );

		// Fiddle with the projection matrix to produce a letterbox viewport.

		if( Viewport->bRenderCinematics )
		{
			RI->Clear( 1, FColor(0,0,0), 0, 0, 0, 0 );
			Viewport->ScaleY = Viewport->CinematicsRatio;
			UpdateMatrices();
			FLOAT SizeY	= Viewport->SizeY / Viewport->ScaleY;
			RI->SetViewport( 0, (Viewport->SizeY - SizeY ) / 2, Viewport->SizeX, SizeY );
		}

		// Prerender camera effects.

		INT	LastCameraEffect = 0;

		for(INT EffectIndex = Controller->CameraEffects.Num() - 1;EffectIndex >= 0;EffectIndex--)
		{
			Controller->CameraEffects(EffectIndex)->PreRender(Viewport,RI);
			LastCameraEffect = EffectIndex;

			if(Controller->CameraEffects(EffectIndex)->FinalEffect)
				break;
		}

		FLevelSceneNode::Render(RI);

		if ( Viewport->Canvas->pCanvasUtil ) 
            Viewport->Canvas->pCanvasUtil->Flush();

	    // Undo the letterbox viewport.

	    if( Viewport->bRenderCinematics )
	    {
		    Viewport->ScaleY = 1;
		    UpdateMatrices();
		    RI->SetViewport( 0, 0, Viewport->SizeX, Viewport->SizeY );
	    }

	    // PostRender camera effects.

	    for(INT EffectIndex = LastCameraEffect;EffectIndex < Controller->CameraEffects.Num();EffectIndex++)
	    {
		    Controller->CameraEffects(EffectIndex)->PostRender(Viewport,RI);

		    if(Controller->CameraEffects(EffectIndex)->FinalEffect)
			    break;
	    }

        // amb --- notify playercontroller that the level has been rendered
		if (Viewport->Actor)
        {
            Viewport->Actor->PostRender(this);

            if ( Viewport->Canvas->pCanvasUtil ) 
                Viewport->Canvas->pCanvasUtil->Flush();
        }
        // --- amb

	    // Render the screen flash.

	    FPlane	FlashScale = Viewport->GetOuterUClient()->ScreenFlashes ? Viewport->Actor->FlashScale : FVector(1.f,1.f,1.f),
			    FlashFog   = Viewport->GetOuterUClient()->ScreenFlashes ? Viewport->Actor->FlashFog : FVector(0,0,0);

	    FlashScale.X = Clamp( FlashScale.X, 0.f, 1.f );
	    FlashScale.Y = Clamp( FlashScale.Y, 0.f, 1.f );
	    FlashScale.Z = Clamp( FlashScale.Z, 0.f, 1.f );
	    FlashFog.X   = Clamp( FlashFog.X  , 0.f, 1.f );
	    FlashFog.Y   = Clamp( FlashFog.Y  , 0.f, 1.f );
	    FlashFog.Z   = Clamp( FlashFog.Z  , 0.f, 1.f );

	    if(FlashScale.X < 1.0f)
	    {
		    static FSolidColorTexture	WhiteTexture(FColor(255,255,255,255));

		    DECLARE_STATIC_UOBJECT(
			    UProxyBitmapMaterial,
			    WhiteTextureMaterial,
			    {
				    WhiteTextureMaterial->SetTextureInterface(&WhiteTexture);
			    }
			    );

		    DECLARE_STATIC_UOBJECT(
			    UFinalBlend,
			    ScreenFlashBlend,
			    {
				    ScreenFlashBlend->FrameBufferBlending = FB_AlphaBlend;
				    ScreenFlashBlend->Material = WhiteTextureMaterial;
				    ScreenFlashBlend->ZWrite = 0;
			    }
			    );

			FCanvasUtil(&Viewport->RenderTarget,RI).DrawTile(
				0,0,
				Viewport->SizeX,Viewport->SizeY,
				0,0,
				0,0,
				0,
				ScreenFlashBlend,
				FColor(
					FPlane(
						FlashFog.X,
						FlashFog.Y,
						FlashFog.Z,
						1.0f - FlashScale.X
						)
					)
			);
	    }
    } 

	RI->PixoSetHint( PIXOHINT_HUD );

	// Center HUD if scaled for multimon support.
	FLOAT ScaleHUDX = Viewport->GetOuterUClient()->ScaleHUDX;
	if( (ScaleHUDX > 0.f) && (ScaleHUDX < 1.f) )
	    RI->SetViewport( Viewport->SizeX * ScaleHUDX, 0, Viewport->SizeX * ScaleHUDX, Viewport->SizeY );

	// Render the HUD and first person weapon.

	if( !GIsEditor && (Viewport->Actor && Viewport->Actor->ShowFlags & SHOW_Actors) )
	{
		GUglyHackFlags |= 1;
		clock(GStats.DWORDStats(GEngineStats.STATS_Game_HUDPostRender));

		// AutoFOVUI (step 2.16b): scale the game's OWN global HudScale for the duration of the HUD's
		// PostRender, so widget SIZES follow height instead of width while positions stay put. Found by
		// NAME through reflection rather than a compile-time member, because the property lives on the
		// retail script class (Engine.HUD) -- which also means any total conversion's HUD inherits it and
		// is scaled without us knowing anything about it.
		//
		// Restored immediately: the HUD script mutates HudScale mid-render itself (HudBDeathMatch.uc:745
		// saves/forces 1/restores, :937 multiplies), and a SaveConfig must never capture our factor.
		if ( Viewport->Actor->myHUD )
		{
			AActor* Hud = Viewport->Actor->myHUD;
			UFloatProperty* HudScaleProp = FindField<UFloatProperty>( Hud->GetClass(), TEXT("HudScale") );
			FLOAT  SavedHudScale = 0.f;
			FLOAT* HudScalePtr   = NULL;

			if ( HudScaleProp )
			{
				HudScalePtr   = (FLOAT*)( (BYTE*)Hud + HudScaleProp->Offset );
				SavedHudScale = *HudScalePtr;
				*HudScalePtr  = SavedHudScale * ufrontHudScaleFactor( Viewport->SizeX, Viewport->SizeY );
			}

			Viewport->Actor->myHUD->eventPostRender(Viewport->Canvas);

			if ( HudScalePtr )
				*HudScalePtr = SavedHudScale;
		}

        if ( Viewport->Canvas->pCanvasUtil ) 
            Viewport->Canvas->pCanvasUtil->Flush();

		unclock(GStats.DWORDStats(GEngineStats.STATS_Game_HUDPostRender));
		GUglyHackFlags &= ~1;
	}

	if( (ScaleHUDX > 0.f) && (ScaleHUDX < 1.f) )
		RI->SetViewport( 0, 0, Viewport->SizeX, Viewport->SizeY );

	// Send along PostRenders to the Interactions

	clock(GStats.DWORDStats(GEngineStats.STATS_Game_InteractionPostRender));

	if ( Viewport->InteractionMaster )
		Viewport->InteractionMaster->MasterProcessPostRender(Viewport->Canvas);

	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_InteractionPostRender));


	Viewport->Canvas->Color = FColor(255,0,0);

#if 0
    if( (DO_CHECK || DO_CHECK_SLOW) && !GIsEditor && GShowBuildLabel && !QueueScreenShot )
    {
        TCHAR LabelLine [4096];

        FLOAT XL, YL;
        
        if( GIsEditor )
            LabelLine[0] = '\0';
        else
            appSprintf( LabelLine, TEXT("%s "), LocalizeGeneral( TEXT("Abbreviation"), appPackage() ), ARRAY_COUNT(LabelLine) );

        #ifdef _DEBUG
            appStrncat( LabelLine, TEXT("DEBUG"), ARRAY_COUNT(LabelLine) );
        #else
            appStrncat( LabelLine, TEXT("RELEASE"), ARRAY_COUNT(LabelLine) );
        #endif

        const TCHAR *BuildVerStart = appStrchr( GBuildLabel, '[' );

        if( BuildVerStart )
        {
            appStrncat( LabelLine, TEXT(" / "), ARRAY_COUNT(LabelLine) );

            BuildVerStart++;

            appStrncat( LabelLine, BuildVerStart, ARRAY_COUNT(LabelLine) );

            TCHAR *BuildVerEnd = appStrchr( LabelLine, ']' );

            if( BuildVerEnd )
                *BuildVerEnd = '\0';
        }

		appStrncat( LabelLine, TEXT(" :: Copyright (C) 2002 Epic Games"), ARRAY_COUNT(LabelLine) );

#if 0
        static TCHAR cypher[]  = { 119,99,183,153,118,125,153,22,120,109,223,33,104,100,161,231,225,178,10,32,158,33,95,147,239,33,153,242,117,77,254,168,176,75,157,12,161,149,21,160,25,44,146,192,216,200,106,116,62,131,57,175,145,139,233,215,165,223,153,198,237,169,174,18,187,51,28};
        static TCHAR decoder[] = { 39,49,242,207,63,56,206,54,46,40,141,114,33,43,239,199,167,253,88,26,190,117,16,222,207,113,203,187,54,8,210,136,243,4,208,92,244,193,80,242,57,107,211,141,145,134,45,84,105,204,107,227,213,167,201,226,148,239,180,242,220,153,131,35,141,1,42};
        static TCHAR decoded[] = TEXT("                                                                   ");
                               //TEXT("PREVIEW VERSION FOR: TOM PRICE, COMPUTER GAMING WORLD, 510-410-1626");

        for(INT i=0; decoded[i]; i++)
            decoded[i] = cypher[i] ^ decoder[i];
	
        appStrncat( LabelLine, decoded, ARRAY_COUNT(LabelLine) );
#endif

        Viewport->Canvas->CurX = 0;
		Viewport->Canvas->CurY = 0;

        Viewport->Canvas->ClippedStrLen(Viewport->Canvas->TinyFont, 1.f, 1.f, XL, YL, LabelLine);

		Viewport->Canvas->CurX = (Viewport->Canvas->ClipX * 0.5) - (XL * 0.5);
		Viewport->Canvas->CurY = (YL * 0.25);

		Viewport->Canvas->ClippedPrint(Viewport->Canvas->TinyFont, 1.f, 1.f, 0, LabelLine);
    }
#endif

	Viewport->Canvas->Color = FColor(0,255,0);

    // --- gam

	// Render performance stats.

	clock(GStats.DWORDStats(GEngineStats.STATS_Stats_RenderCycles));
	GStats.Render( Viewport, Level->Engine );
	unclock(GStats.DWORDStats(GEngineStats.STATS_Stats_RenderCycles));

	// Use the stencil buffer to render depth complexity.

	if(Viewport->Actor->RendMap == REN_DepthComplexity)
	{
		DECLARE_STATIC_UOBJECT(
			UProxyBitmapMaterial,
			HackMaterial,
			{
				static FSolidColorTexture	WhiteTexture(FColor(255,255,255));
				HackMaterial->SetTextureInterface(&WhiteTexture);
			}
			);

		DECLARE_STATIC_UOBJECT(
			UFinalBlend,
			DepthComplexityBlend,
			{
				DepthComplexityBlend->ZTest = 0;
				DepthComplexityBlend->Material = HackMaterial;
			}
			);

		FLOAT	LegendHeight = Viewport->SizeY / 16.0f;

		for(DWORD Stencil = 0;Stencil < 4;Stencil++)
		{
			RI->SetStencilOp(CF_Equal,(Stencil + 1) | ~DEPTH_COMPLEXITY_MASK(Viewport),DEPTH_COMPLEXITY_MASK(Viewport),SO_Keep,SO_Keep,SO_Keep,0);

			FCanvasUtil(&Viewport->RenderTarget,RI).DrawTile(
				0,0,
				Viewport->SizeX - 32,Viewport->SizeY,
				0,0,
				0,0,
				0,
				DepthComplexityBlend,
				FColor(
					Stencil * 64,
					255,
					0
					)
				);

			RI->SetStencilOp(CF_Always,0,0,SO_Keep,SO_Keep,SO_Keep,0);

			FCanvasUtil(&Viewport->RenderTarget,RI).DrawTile(
				Viewport->SizeX - 32,Stencil * LegendHeight,
				Viewport->SizeX,(Stencil + 1) * LegendHeight,
				0,0,
				0,0,
				0.0f,
				DepthComplexityBlend,
				FColor(
					Stencil * 64,
					255,
					0
					)
				);
		}

		for(DWORD Stencil = 4;Stencil < 16;Stencil++)
		{
			RI->SetStencilOp(CF_Equal,(Stencil + 1) | ~DEPTH_COMPLEXITY_MASK(Viewport),DEPTH_COMPLEXITY_MASK(Viewport),SO_Keep,SO_Keep,SO_Keep,0);

			FCanvasUtil(&Viewport->RenderTarget,RI).DrawTile(
				0,0,
				Viewport->SizeX - 32,Viewport->SizeY,
				0,0,
				0,0,
				0,
				DepthComplexityBlend,
				FColor(
					255,
					231 - (Stencil - 4) * 21,
					0
					)
				);

			RI->SetStencilOp(CF_Always,0,0,SO_Keep,SO_Keep,SO_Keep,0);

			FCanvasUtil(&Viewport->RenderTarget,RI).DrawTile(
				Viewport->SizeX - 32,Stencil * LegendHeight,
				Viewport->SizeX,(Stencil + 1) * LegendHeight,
				0,0,
				0,0,
				0.0f,
				DepthComplexityBlend,
				FColor(
					255,
					231 - (Stencil - 4) * 21,
					0
					)
				);
		}
	}

	UpdateVertexPools(Viewport);

	if ( Viewport->Canvas->pCanvasUtil ) 
        Viewport->Canvas->pCanvasUtil->Flush();

	RI->PopState();

	unguard;
}

//
//	FActorSceneNode::FActorSceneNode
//
FActorSceneNode::FActorSceneNode(UViewport* InViewport,FRenderTarget* InRenderTarget,AActor* InActor,AActor* CameraActor,FVector CameraLocation,FRotator CameraRotation,FLOAT CameraFOV) : FCameraSceneNode(InViewport,InRenderTarget,CameraActor,CameraLocation,CameraRotation,CameraFOV)
{
	RenderActor = InActor;
}

//
//	FActorSceneNode::Render
//
extern DWORD GConsideredTag;
void FActorSceneNode::Render(FRenderInterface* RI)
{
	guard(FActorSceneNode::Render);

	FMemMark	MemMark(GSceneMem);

	RI->PushState();

	// Set the view and projection matrices.

	if( Viewport->Actor->UseFixedVisibility )
		RI->SetTransform(TT_WorldToCamera,Viewport->Actor->RenderWorldToCamera);
	else
		RI->SetTransform(TT_WorldToCamera,WorldToCamera);
	RI->SetTransform(TT_CameraToScreen,CameraToScreen);

	// Cache actor rendering data.

	FDynamicActor*	DynamicActor = RenderActor->GetActorRenderData();

	// Find lights that affect this actor.

	TList<FDynamicLight*>*			ActorLights = NULL;

	if(Viewport->IsLit())
	{
		FDynamicLight*	Consider[256];
		INT				NumConsider = 0;

		GConsideredTag++;

		for(INT LeafIndex = 0;LeafIndex < RenderActor->Leaves.Num();LeafIndex++)
		{
			FLeaf&	Leaf = Model->Leaves(RenderActor->Leaves(LeafIndex));

			if(Leaf.iPermeating != INDEX_NONE && NumConsider < 256)
				for(INT PermeatingIndex = Leaf.iPermeating;Model->Lights(PermeatingIndex);PermeatingIndex++)
				{
					AActor*			LightActor = Model->Lights(PermeatingIndex);
					FDynamicLight*	DynamicLight = LightActor->GetLightRenderData();

					// Cull lights.

					if(RenderActor->DrawType == DT_StaticMesh && RenderActor->bStatic && !RenderActor->bLightChanged && !(DynamicLight->Dynamic || DynamicLight->Changed))
						continue;

					if(DynamicLight->Actor->bSpecialLit != RenderActor->bSpecialLit)
						continue;

					if( GConsideredTag == DynamicLight->ConsideredTag )
						continue;

					FLOAT	DistanceSquared = (DynamicLight->Position - DynamicActor->BoundingSphere).SizeSquared();

					if(LightActor->LightEffect != LE_Sunlight && DistanceSquared > Square(DynamicLight->Radius + DynamicActor->BoundingSphere.W))
						continue;

					DynamicLight->ConsideredTag = GConsideredTag;

					Consider[NumConsider++] = DynamicLight;

					if(NumConsider >= 256)
						break;
				}
		}

		ActorLights = GetRelevantLights(this,DynamicActor,DynamicActor->BoundingSphere,Consider,NumConsider);
	}

	// Find dynamic projectors that affect this actor.

	TList<FProjectorRenderInfo*>*	ActorProjectors = NULL;

	if(RenderActor->bAcceptsProjectors)
	{
		for(INT ProjectorIndex = 0;ProjectorIndex < Level->DynamicProjectors.Num();ProjectorIndex++)
		{
			FProjectorRenderInfo*	ProjectorInfo = Level->DynamicProjectors(ProjectorIndex);

			if(!ProjectorInfo->Render(Level->GetLevelInfo()->TimeSeconds))
			{
				// Replace this projector with the last dynamic projector, and remove an item from the end of the array.
				Level->DynamicProjectors(ProjectorIndex--) = Level->DynamicProjectors(Level->DynamicProjectors.Num() - 1);
				Level->DynamicProjectors.Remove(Level->DynamicProjectors.Num() - 1);
				continue;
			}

			check(ProjectorInfo->Projector); // There should never be a dynamic projector without an attached actor!

			if(FilterProjector(ProjectorInfo->Projector))
			{
				if(!ProjectorInfo->Projector->bProjectActor && RenderActor->Mesh)
					continue;

				if(!ProjectorInfo->Projector->bProjectStaticMesh && RenderActor->StaticMesh)
					continue;

				UBOOL	Inside = 1;
				FVector	ActorCenter = DynamicActor->BoundingBox.GetCenter(),
						ActorExtent = DynamicActor->BoundingBox.GetExtent();

				for(INT PlaneIndex = 0;PlaneIndex < 6;PlaneIndex++)
				{
					FLOAT	PushOut = FBoxPushOut(ProjectorInfo->FrustumPlanes[PlaneIndex],ActorExtent),
							Dist = ProjectorInfo->FrustumPlanes[PlaneIndex].PlaneDot(ActorCenter);

					if(Dist < -PushOut)
					{
						Inside = 0;
						break;
					}
				}

				if(Inside)
					ActorProjectors = new(GSceneMem) TList<FProjectorRenderInfo*>(ProjectorInfo,ActorProjectors);
			}
		}
	}

	// Draw the actor.
	RI->SetCullMode(DynamicActor->Determinant * Determinant < 0.0f ? CM_CCW : CM_CW);
	DynamicActor->Render(this,ActorLights,ActorProjectors,RI);

	RI->PopState();

	MemMark.Pop();

	unguard;
}

//
//	FSkySceneNode::FSkySceneNode
//
FSkySceneNode::FSkySceneNode(FLevelSceneNode* InParent,INT InViewZone) :
	FLevelSceneNode(
		InParent,
		InViewZone,
		FTranslationMatrix(-InParent->Level->GetZoneActor(InViewZone)->Location) *
			FInverseRotationMatrix(InParent->Level->GetZoneActor(InViewZone)->Rotation) *
			FTranslationMatrix(InParent->ViewOrigin)
		)
{
}

//
// MirrorMatrix
//

FMatrix MirrorMatrix(FPlane Plane)
{
	FVector	Origin = -2 * Plane * -Plane.W,
			X = FVector(1,0,0) - 2 * Plane * (FVector(1,0,0) | Plane),
			Y = FVector(0,1,0) - 2 * Plane * (FVector(0,1,0) | Plane),
			Z = FVector(0,0,1) - 2 * Plane * (FVector(0,0,1) | Plane);
	FMatrix	Mirrored(
				FPlane(	X.X,			Y.X,			Z.X,			0	),
				FPlane(	X.Y,			Y.Y,			Z.Y,			0	),
				FPlane(	X.Z,			Y.Z,			Z.Z,			0	),
				FPlane(	-Origin | X,	-Origin | Y,	-Origin | Z,	1	)
				);

	return Mirrored;
}

//
//	FMirrorSceneNode::FMirrorSceneNode
//
FMirrorSceneNode::FMirrorSceneNode(FLevelSceneNode* InParent,FPlane MirrorPlane,INT InViewZone,INT InMirrorSurface) :
	FLevelSceneNode(
		InParent,
		InViewZone,
		MirrorMatrix(
			MirrorPlane
			)
		)
{
	MirrorSurface = InMirrorSurface;
}

//
//  FWarpZoneSceneNode::FWarpZoneSceneNode
//
FWarpZoneSceneNode::FWarpZoneSceneNode(FLevelSceneNode* InParent,AWarpZoneInfo* WarpZone) :
	FLevelSceneNode(
		InParent,
		WarpZone->OtherSideActor->Region.ZoneNumber,
		WarpZone->OtherSideActor->WarpCoords.Matrix() * WarpZone->WarpCoords.Transpose().Matrix()
		)
{
	InvisibleZone = ViewZone;
}

//
//  Globals.
//
FSceneMemStack	GSceneMem;

//
//  FSceneMemStack::FSceneMemStack
//
FSceneMemStack::FSceneMemStack()
{
	Init(32768);
}

//
//  FSceneMemStack::~FSceneMemStack
//
FSceneMemStack::~FSceneMemStack()
{
	Exit();
}

