/*=============================================================================
	UnShaderProjector.cpp: Projected shadow rendering code.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UShadowBitmapMaterial);

//
//	UShadowBitmapMaterial::Destroy
//

void UShadowBitmapMaterial::Destroy()
{
	guard(UShadowBitmapMaterial::Destroy);

	FAuxRenderTarget*&	ShadowTexture = *(FAuxRenderTarget**)&TextureInterfaces[0];
	FAuxRenderTarget*&	TempRenderTarget = *(FAuxRenderTarget**)&TextureInterfaces[1];

	if(ShadowTexture)
	{
		delete ShadowTexture;
		ShadowTexture = NULL;
	}

	if(TempRenderTarget)
	{
		delete TempRenderTarget;
		TempRenderTarget = NULL;
	}

	Super::Destroy();

	unguard;
}

//
//	UShadowBitmapMaterial::GetRenderInterface
//

FBaseTexture* UShadowBitmapMaterial::GetRenderInterface()
{
	static FSolidColorTexture	InvalidShadowTexture(FColor(127,127,127));

	if(Invalid)
		return &InvalidShadowTexture;
	else
		return *(FBaseTexture**)&TextureInterfaces[0];
}

//
//	FShadowBitmapRenderInterface
//

class FShadowBitmapRenderInterface : public FRenderInterface
{
public:

	BYTE				Darkness;
	FRenderInterface*	RI;

	// Constructor/destructor.

	FShadowBitmapRenderInterface(FRenderInterface* InRI,BYTE InDarkness)
	{
		RI = InRI;
		Darkness = InDarkness;

		RI->PushState();
		RI->EnableLighting(0,0);
	}

	~FShadowBitmapRenderInterface()
	{
		RI->PopState();
	}

	// FRenderInterface interface.

	virtual void PushState()
	{
		RI->PushState();
	}

	virtual void PopState()
	{
		RI->PopState();
	}

	virtual UBOOL SetRenderTarget(FRenderTarget* RenderTarget)
	{
		return RI->SetRenderTarget(RenderTarget);
	}

	virtual void SetViewport(INT X,INT Y,INT Width,INT Height)
	{
		RI->SetViewport(X,Y,Width,Height);
	}

	virtual void Clear(UBOOL UseColor,FColor Color,UBOOL UseDepth,FLOAT Depth,UBOOL UseStencil,DWORD Stencil)
	{
		RI->Clear(UseColor,Color,UseDepth,Depth,UseStencil,Stencil);
	}

	virtual void PushHit(const BYTE* Data,INT Count)
	{
		RI->PushHit(Data,Count);
	}

	virtual void PopHit(INT Count,UBOOL Force)
	{
		RI->PopHit(Count,Force);
	}

	virtual void SetCullMode(ECullMode CullMode)
	{
	}

	virtual void SetAmbientLight(FColor Color)
	{
	}

	virtual void EnableLighting(UBOOL UseDynamic,UBOOL UseStatic,UBOOL Modulate2X,FBaseTexture* LightMap,UBOOL LightingOnly,FSphere LitSphere)
	{
	}

	virtual void SetLight(INT LightIndex,FDynamicLight* Light,FLOAT scale) // sjs
	{
	}

	virtual void SetNPatchTesselation( FLOAT Tesselation )
	{
	}

	virtual void SetDistanceFog(UBOOL Enable,FLOAT FogStart,FLOAT FogEnd,FColor Color)
	{
	}

	virtual void SetGlobalColor(FColor Color)
	{
	}

	virtual void SetTransform(ETransformType Type,const FMatrix& Matrix)
	{
		RI->SetTransform(Type,Matrix);
	}

	virtual void SetZBias(INT ZBias)
	{
		RI->SetZBias(ZBias);
	}

	virtual void SetStencilOp(ECompareFunction Test,DWORD Ref,DWORD Mask,EStencilOp FailOp,EStencilOp ZFailOp,EStencilOp PassOp,DWORD WriteMask)
	{
		RI->SetStencilOp(Test,Ref,Mask,FailOp,ZFailOp,PassOp,WriteMask);
	}

	virtual void SetPrecacheMode( EPrecacheMode PrecacheMode )
	{
		RI->SetPrecacheMode( PrecacheMode );
	}

	virtual void SetMaterial(UMaterial* Material,FString* ErrorString,UMaterial** ErrorMaterial,INT* NumPasses)
	{
		UShader*	Shader = Cast<UShader>(Material);

		DECLARE_STATIC_UOBJECT(
			UConstantColor,
			ShadowColor,
			{
			}
			);

		ShadowColor->Color = FColor(128,128,128,255 - Darkness);

		if(Shader)
		{
			DECLARE_STATIC_UOBJECT(
				UShader,
				ShadowShader,
				{
				}
				);

			ShadowShader->Diffuse = ShadowColor;
			ShadowShader->Opacity = Shader->Opacity;

			RI->SetMaterial(ShadowShader);
		}
		else
			RI->SetMaterial(ShadowColor);
	}

	virtual INT SetVertexStreams(EVertexShader Shader,FVertexStream** Streams,INT NumStreams)
	{
		return RI->SetVertexStreams(Shader,Streams,NumStreams);
	}

	virtual INT SetDynamicStream(EVertexShader Shader,FVertexStream* Stream)
	{
		return RI->SetDynamicStream(Shader,Stream);
	}

	virtual INT SetIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseIndex)
	{
		return RI->SetIndexBuffer(IndexBuffer,BaseIndex);
	}

	virtual INT SetDynamicIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseIndex)
	{
		return RI->SetDynamicIndexBuffer(IndexBuffer,BaseIndex);
	}

	virtual void DrawPrimitive(EPrimitiveType PrimitiveType,INT FirstIndex,INT NumPrimitives,INT MinIndex = INDEX_NONE,INT MaxIndex = INDEX_NONE)
	{
		RI->DrawPrimitive(PrimitiveType,FirstIndex,NumPrimitives,MinIndex,MaxIndex);
	}
};

//
//  FShadowSceneNode - sjs
//
class ENGINE_API FShadowSceneNode : public FActorSceneNode
{
public:

	// Constructor.
    FShadowSceneNode(UViewport* InViewport,FRenderTarget* InRenderTarget,AActor* InActor,AActor* CameraActor,FVector CameraLocation,FRotator CameraRotation,FLOAT CameraFOV) : FActorSceneNode(InViewport,InRenderTarget,InActor,CameraActor,CameraLocation,CameraRotation,CameraFOV)
    {
    }

    virtual UBOOL FilterAttachment(AActor* AttachedActor)
    {
        if( AttachedActor && !AttachedActor->bActorShadows )
            return 0;
        return FActorSceneNode::FilterAttachment(AttachedActor);
    }

	virtual FSceneNode* GetLodSceneNode()
	{
		if(Viewport->LodSceneNode)
			return Viewport->LodSceneNode;
		else
			return this;
	}
};

//
//	UShadowBitmapMaterial::Get
//

// ★★ ufront 2.53 — [ufront.Shadows]: the pawn-shadow fade, exposed so it can be tuned without a rebuild.
//
// Reported by the owner as "shadows fade out too close". Retail draws a shadow with
//     ShadowAtten = 1 - clamp( (dist / CullDistance)^2, 0, 1 )
// where CullDistance comes from ShadowProjector.uc's `CullDistance=1200.0` in Engine.u. Two things make
// that feel early, and BOTH are exposed here, because raising only the distance keeps the shape of the
// curve: the falloff is SQUARED, so a shadow is already 25% faded at half the cull distance and ~51% at
// 0.7 of it. FadeExponent 1.0 is linear; higher than 2.0 holds full strength longer and then drops fast.
//
// ⚠ A FREE CONFIG SECTION, NOT A CLASS — the same decision (and the same reasoning) as [ufront.AutoFOV]
// in UnRender.cpp: the value belongs to a retail Engine.u class that cannot gain properties, a free
// section is readable from anywhere, and it is one obvious block for a human to edit. It is NOT a
// renderer-device setting — this fade is computed in Engine, not in the GL driver — and not a per-player
// profile field, so it belongs in the system ini rather than User.ini.
//
// ⚠ DEFAULTS ARE THE RETAIL VALUES, so an absent or partial section is a NO-OP. Nothing changes until
// somebody sets something — and what the WEB BUILD sets is `CullDistance=4800.0` (4x the retail range,
// step 2.57) in Engine/Config/web/Default.ini. Only the RANGE ships changed; the falloff stays retail 2.0.
static UBOOL  GShadowCfgParsed  = 0;
static FLOAT  GShadowCullDist   = 1200.0f;	// ShadowProjector.uc default
static FLOAT  GShadowFadeExp    = 2.0f;		// retail's squared falloff
static UBOOL  GShadowDebug      = 0;		// -SHADOWDEBUG: log every shadow's distance and attenuation

static void ufrontParseShadowCfg()
{
	guard(ufrontParseShadowCfg);
	GShadowCfgParsed = 1;

	FLOAT Dist = GShadowCullDist;
	FLOAT Exp  = GShadowFadeExp;
	GConfig->GetFloat( TEXT("ufront.Shadows"), TEXT("CullDistance"), Dist );
	GConfig->GetFloat( TEXT("ufront.Shadows"), TEXT("FadeExponent"), Exp  );

	// ⚠ NO `SHADOWDIST=` / `SHADOWFADE=` SWITCHES (removed 2026-09-10, owner's call). They were added
	// alongside the config as "recoverable without editing an ini you cannot reach", and that reason no
	// longer holds: the ini IS reachable — the launcher's "Edit config & logs…" dialog opens the system
	// ini and the value persists in user data. A SETTING HAS ONE HOME; a duplicate on the command line is
	// permanent surface on a line that is already long, repeated every launch, and undiscoverable.
	//
	// SHADOWDEBUG stays: it is not a setting but a transient diagnostic (a per-shadow log for one run),
	// which is what a command line is actually for.
	if( appStrfind( appCmdLine(), TEXT("SHADOWDEBUG") ) )
		GShadowDebug = 1;

	// 0 or less would divide by zero / invert the curve; the ceilings are sanity, not policy.
	GShadowCullDist = Clamp( Dist, 1.0f, 65536.0f );
	GShadowFadeExp  = Clamp( Exp,  0.1f, 16.0f );

	debugf( NAME_Log, TEXT("[ufront.Shadows] cullDistance=%f fadeExponent=%f (retail: 1200.0 / 2.0)"),
		GShadowCullDist, GShadowFadeExp );
	unguard;
}

//
// Apply the configured shadow distance to the ShadowProjector CLASS DEFAULT. Called once from
// UGameEngine::Init, after Engine.u is loaded.
//
// ★★ WHY THIS IS NEEDED AND WHY THE FADE ALONE IS NOT ENOUGH — MEASURED, NOT ASSUMED. `CullDistance`
// is used TWICE for a shadow, and they are different mechanisms:
//   1. the FADE below, via the material's copy (InitShadow does `ShadowTexture.CullDistance = CullDistance`);
//   2. the ACTOR-level visibility cull, `CheckCullDistance` in UnRenderVisibility.cpp, which reads the
//      PROJECTOR ACTOR's own `CullDistance` and stops drawing it entirely.
// Raising only (1) gives a shadow that is still near-full-strength when (2) deletes it — a hard pop
// instead of a longer fade. Instrumented with -SHADOWDEBUG on two different maps: with the fade pushed
// to 6000 the attenuation at 1250-1499 was still 0.954, and the samples STOPPED at 1324 on DM-Antalus
// and 1312 on DM-Rankin. That ceiling is the retail 1200 plus the pawn's bounding-box radius, because
// CheckCullDistance measures to the box rather than the centre.
//
// Patching the class default fixes both at once — the actor cull and the material's own copy — so the
// two can never disagree, and a mod that overrides CullDistance on its own projector subclass still wins.
void ufrontApplyShadowCfg()
{
	guard(ufrontApplyShadowCfg);
	if( !GShadowCfgParsed )
		ufrontParseShadowCfg();

	// Retail value: leave the class exactly as shipped rather than rewriting it with itself.
	if( Abs(GShadowCullDist - 1200.0f) < 0.01f )
		return;

	UClass* C = FindObject<UClass>( ANY_PACKAGE, TEXT("ShadowProjector") );
	if( !C || !C->GetDefaultActor() )
	{
		debugf( NAME_Log, TEXT("[ufront.Shadows] ShadowProjector class not found; distance NOT applied") );
		return;
	}
	AActor* Def = C->GetDefaultActor();
	debugf( NAME_Log, TEXT("[ufront.Shadows] ShadowProjector.CullDistance %f -> %f (actor cull + fade)"),
		Def->CullDistance, GShadowCullDist );
	Def->CullDistance = GShadowCullDist;
	unguard;
}

UBitmapMaterial* UShadowBitmapMaterial::Get(FTime Time,UViewport* Viewport)
{
	guard(UShadowBitmapMaterial::Get);

	if( bBlobShadow || Invalid || !Viewport->RenDev->SupportsRenderToTextureRGBA8888 || GIsPixomatic )
		return BlobShadow;

	if(Dirty && ShadowActor) // gam
	{
	    FLOAT LastRenderTimeBackup = ShadowActor->LastRenderTime; // gam
	    
		FRenderInterface*	RI = Viewport->RI;
		FAuxRenderTarget*&	ShadowTexture = *(FAuxRenderTarget**)&TextureInterfaces[0];
		FAuxRenderTarget*&	TempRenderTarget = *(FAuxRenderTarget**)&TextureInterfaces[1];

		FVector		SavedLocation = Viewport->Actor->Location;
		FRotator	SavedRotation = Viewport->Actor->Rotation;
		INT			SavedSizeX = Viewport->SizeX,
					SavedSizeY = Viewport->SizeY;
		FLOAT		SavedScaleX = Viewport->ScaleX,
					SavedScaleY = Viewport->ScaleY;
		DWORD		SavedShowFlags = Viewport->Actor->ShowFlags;
		UBOOL		SavedCinematics = Viewport->bRenderCinematics;

        // sjs ---
        AActor*		CameraActor = Viewport->Actor;
	    FVector		CameraLocation;
	    FRotator	CameraRotation;
        FLOAT FovBias  = 1.0f;
        if( CameraActor )
        {
            CameraLocation = CameraActor->Location;
            CameraRotation = CameraActor->Rotation;
            FovBias = appTan( Viewport->Actor->FovAngle*(PI/360.f) );
            Viewport->Actor->eventPlayerCalcView(CameraActor,CameraLocation,CameraRotation);
        }
        // --- sjs

		USkeletalMeshInstance*	MeshInstance = (ShadowActor->DrawType == DT_Mesh && ShadowActor->Mesh) ? Cast<USkeletalMeshInstance>(ShadowActor->Mesh->MeshGetInstance(ShadowActor)) : NULL;

		FVector ShadowLocation = MeshInstance ? MeshInstance->GetBoneCoords(0).Origin : ShadowActor->Location + FVector(0,0,5),
				FrustumOrigin = ShadowLocation + LightDirection * LightDistance;

		Viewport->Actor->Location = FrustumOrigin;
		Viewport->Actor->Rotation = (-LightDirection).Rotation();
		//Viewport->SizeX = USize;
		//Viewport->SizeY = VSize;
		Viewport->ScaleX = Viewport->ScaleY = 1.0f;
		Viewport->bRenderCinematics = 0;
		Viewport->Actor->ShowFlags &= ~SHOW_Projectors;

		// Render the shadow into a temporary texture.

		RI->PushState();

		if(!TempRenderTarget)
			TempRenderTarget = new FAuxRenderTarget(USize,VSize,(ETextureFormat)Format);

		GHackNoDepthTestRenderTarget = 1;
		if(RI->SetRenderTarget(TempRenderTarget))
		{
			// sjs --- distance fade shadows
            float ShadowAtten = 1.0f;
            // ufront 2.53 — the DISTANCE arrives through this material's own CullDistance, which
            // InitShadow copies from the projector actor, whose class default ufrontApplyShadowCfg has
            // already set from [ufront.Shadows]. Only the CURVE is overridden here. Keeping the distance
            // on the instance means a mod that overrides CullDistance on its own projector still wins,
            // and the fade can never disagree with the actor-level cull that decides when it stops
            // being drawn at all.
            if( !GShadowCfgParsed )
                ufrontParseShadowCfg();
            if( CameraActor && CullDistance > 0.0f )
            {
                float effectiveDist = (ShadowLocation - CameraLocation).Size() * FovBias;
                float distRatio = effectiveDist / CullDistance;
                ShadowAtten = 1.0f-Clamp<FLOAT>(appPow(distRatio,GShadowFadeExp), 0.f, 1.f);
                // -SHADOWDEBUG: the numbers behind "shadows fade out too close", so the value can be
                // tuned against a reading instead of against an impression. ⚠ A shadow that has stopped
                // being DRAWN prints nothing at all — that absence is the signal that the ACTOR cull
                // dropped it, which is exactly how the 1200 ceiling was found.
                if( GShadowDebug )
                    debugf( NAME_Log, TEXT("[ufront.Shadows] %s dist=%.0f cull=%.0f ratio=%.3f atten=%.3f"),
                        ShadowActor->GetName(), effectiveDist, CullDistance, distRatio, ShadowAtten );
            }
            // --- sjs

			// Render the actor into the shadow texture (without worrying about lights and projectors).

			UBOOL	StoredAcceptProjectors	= ShadowActor->bAcceptsProjectors;
			INT		StoredRendMap			= Viewport->Actor->RendMap;
			Viewport->Actor->RendMap		= REN_PlainTex;
			ShadowActor->bAcceptsProjectors	= 0;

			FShadowBitmapRenderInterface	ShadowRI(RI,ShadowDarkness * ShadowAtten);
			FShadowSceneNode				SceneNode(Viewport,TempRenderTarget,ShadowActor,Viewport->Actor,Viewport->Actor->Location,Viewport->Actor->Rotation,LightFOV);

			RI->Clear(1,FColor(128,128,128,255),1,1.0f,1,~DEPTH_COMPLEXITY_MASK(Viewport));
			SceneNode.Render(&ShadowRI); // sjs

			Viewport->Actor->RendMap		= StoredRendMap;
			ShadowActor->bAcceptsProjectors	= StoredAcceptProjectors;

			RI->PopState();

			// Blur the shadow into the shadow texture.

			RI->PushState();

			if(!ShadowTexture)
				ShadowTexture = new FAuxRenderTarget(USize,VSize,(ETextureFormat)Format);

			if(RI->SetRenderTarget(ShadowTexture))
			{
				RI->Clear( 1, FColor(0,0,0) );

				FLOAT	Filter[3][3] = // amb - blurred is good
				{
					{ 1, 1, 1 },
					{ 1, 1, 1 },
					{ 1, 1, 1 }
				};
				FLOAT	FilterSum = 0.0f;

				for(INT Y = 0;Y < 3;Y++)
					for(INT X = 0;X < 3;X++)
						FilterSum += Filter[X][Y];

				DECLARE_STATIC_UOBJECT(
					UProxyBitmapMaterial,
					ShadowBitmapMaterial,
					{
					}
					);

				ShadowBitmapMaterial->SetTextureInterface(TempRenderTarget);

				DECLARE_STATIC_UOBJECT(
					UFinalBlend,
					BlurBlend,
					{
						BlurBlend->FrameBufferBlending = FB_Brighten;
						BlurBlend->Material = ShadowBitmapMaterial;
						BlurBlend->ZWrite = 0;
						BlurBlend->ZTest = 0;
					}
					);

				RI->EnableLighting(0,1);

				FCanvasUtil ShadowCanvas(ShadowTexture,RI);
				for(INT Y = 0;Y < 3;Y++)
				{
					for(INT X = 0;X < 3;X++)
					{
						ShadowCanvas.DrawTile(
							0,
							0,
							USize,
							VSize,
							-X * 1.5f + 1 * 1.5f,
							-Y * 1.5f + 1 * 1.5f,
							-X * 1.5f + USize + 1 * 1.5f,
							-Y * 1.5f + VSize + 1 * 1.5f,
							1.0f,
							BlurBlend,
							FColor(FPlane(1,1,1,Filter[X][Y] / FilterSum))
							);
					}
				}
				ShadowCanvas.Flush();

				RI->SetMaterial(NULL);

				Invalid = 0;
			}
			else
				Invalid = 1;
		}
		else
			Invalid = 1;

		RI->PopState();

		Viewport->Actor->Location = SavedLocation;
		Viewport->Actor->Rotation = SavedRotation;
		Viewport->SizeX = SavedSizeX;
		Viewport->SizeY = SavedSizeY;
		Viewport->ScaleX = SavedScaleX;
		Viewport->ScaleY = SavedScaleY;
		Viewport->bRenderCinematics = SavedCinematics;
		Viewport->Actor->ShowFlags = SavedShowFlags;

		Dirty = 0;

    	ShadowActor->LastRenderTime = LastRenderTimeBackup; // gam
	}

	if( Invalid )
		return BlobShadow;
	else
		return this;

	unguard;
}

