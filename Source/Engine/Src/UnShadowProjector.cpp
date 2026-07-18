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
            if( CameraActor && CullDistance > 0.0f )
            {
                float effectiveDist = (ShadowLocation - CameraLocation).Size() * FovBias;
                float distRatio = effectiveDist / CullDistance;
                ShadowAtten = 1.0f-Clamp<FLOAT>(appPow(distRatio,2.0f), 0.f, 1.f);
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

