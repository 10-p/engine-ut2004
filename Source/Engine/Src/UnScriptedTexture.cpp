/*=============================================================================
	UnScriptedTexture.cpp: Scripted Texture implementation.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UScriptedTexture);

//
//	FScriptedRenderTarget
//

class FScriptedRenderTarget : public FRenderTarget
{
public:

	UScriptedTexture*	Texture;
	QWORD				CacheId;

	// Constructor.

	FScriptedRenderTarget(UScriptedTexture* InTexture)
	{
		Texture = InTexture;
		CacheId = MakeCacheID(CID_RenderTexture);
	}

	// FRenderResource interface.

	virtual QWORD GetCacheId() { return CacheId; }
	virtual INT GetRevision() { return Texture->Revision; }

	// FBaseTexture interface.

	virtual INT GetWidth() { return Texture->USize; }
	virtual INT GetHeight() { return Texture->VSize; }
	virtual INT GetFirstMip() { return 0; }
	virtual INT GetNumMips() { return 1; }
	virtual ETextureFormat GetFormat() { return TEXF_RGBA8; }
	virtual ETexClampMode GetUClamp() { return TC_Wrap; }
	virtual ETexClampMode GetVClamp() { return TC_Wrap; }
};

//
//	UScriptedTexture::Get
//

UBitmapMaterial* UScriptedTexture::Get(FTime Time,UViewport* Viewport)
{
	guard(UScriptedTexture::Get);

	if(!RenderTarget)
		*(FRenderTarget**)&RenderTarget = new FScriptedRenderTarget(this);

	if(!Viewport->RenDev->ResourceCached((*(FScriptedRenderTarget**)&RenderTarget)->CacheId))
		Revision++;

	if(!Invalid && Revision != OldRevision && USize > 0 && VSize > 0)
	{
		RenderViewport = Viewport;

		OldRevision = Revision;

		Viewport->RI->PushState();

		GHackNoDepthTestRenderTarget = 1;
		if( Viewport->RI->SetRenderTarget(*(FRenderTarget**)&RenderTarget) )
		{
			Viewport->RI->Clear(1,FColor(0,0,0,0));

			if(Client)
				Client->eventRenderTexture(this);
			else
			{
				FCanvasUtil	CanvasUtil(*(FRenderTarget**)&RenderTarget,RenderViewport->RI);

				CanvasUtil.DrawTile(
					0,
					0,
					USize,
					VSize,
					0,
					0,
					DefaultMaterial->MaterialUSize(),
					DefaultMaterial->MaterialVSize(),
					0.0f,
					DefaultMaterial,
					FColor(255,255,255,128)
					);

				UFont*	Font = FindObjectChecked<UFont>(ANY_PACKAGE,TEXT("DefaultFont"));
				INT		Width = 0;

				Width = CanvasUtil.DrawString(
					0,
					0,
					TEXT("Detached client"),
					Font,
					FColor(0,0,0,0)
					);

				CanvasUtil.DrawString(
					USize * 0.5f - Width * 0.5f,
					VSize * 0.5f - 8,
					TEXT("Detached client"),
					Font,
					FColor(0,0,0,255)
					);
			}
		}
		else
			Invalid = 1;

		Viewport->RI->PopState();

		RenderViewport = NULL;
	}

	return this;

	unguard;
}

//
//	UScriptedTexture::GetRenderInterface
//

FBaseTexture* UScriptedTexture::GetRenderInterface()
{
	return *(FRenderTarget**)&RenderTarget;
}

//
//	UScriptedTexture::Destroy
//

void UScriptedTexture::Destroy()
{
	guard(UScriptedTexture::Destroy);

	Super::Destroy();

	if(RenderTarget)
		delete *(FRenderTarget**)&RenderTarget;

	unguard;
}

//
//	UScriptedTexture::PostEditChange
//

void UScriptedTexture::PostEditChange()
{
	Super::PostEditChange();

	USize = UClamp;
	VSize = VClamp;
	Revision++;
}

//
//	UScriptedTexture::execSetSize
//

void UScriptedTexture::execSetSize(FFrame& Stack,RESULT_DECL)
{
	guard(UScriptedTexture::execSetSize);

	P_GET_INT(Width);
	P_GET_INT(Height);
	P_FINISH;

	UClamp = USize = Width;
	UBits = appCeilLogTwo(Width);

	VClamp = VSize = Height;
	VBits = appCeilLogTwo(Height);

	Revision++;

	unguard;
}

//
//	UScriptedTexture::execDrawText
//

void UScriptedTexture::execDrawText(FFrame& Stack,RESULT_DECL)
{
	guard(UScriptedTexture::execDrawText);

	P_GET_INT(StartX);
	P_GET_INT(StartY);
	P_GET_STR(Text);
	P_GET_OBJECT(UFont,Font);
	P_GET_STRUCT(FColor,Color);
	P_FINISH;

	if( !Font )
	{
		debugf(NAME_ScriptWarning,TEXT("No font passed to ScriptedTexture.DrawText(%s)"),*Text);
		return;
	}

	if( !Invalid )
	{	
		FCanvasUtil(*(FRenderTarget**)&RenderTarget,RenderViewport->RI).DrawString(
			StartX,
			StartY,
			*Text,
			Font,
			Color,
			1
			);
	}

	unguardexec;
}

//
//	UScriptedTexture:execTextSize
//

void UScriptedTexture::execTextSize(FFrame& Stack,RESULT_DECL)
{
	guard(UScriptedTexture::execTextSize);

	P_GET_STR(Text);
	P_GET_OBJECT(UFont,Font);
	P_GET_INT_REF(SizeX);
	P_GET_INT_REF(SizeY);
	P_FINISH;

	if( !Font )
	{
		debugf(NAME_ScriptWarning,TEXT("No font passed to ScriptedTexture.TextSize(%s)"),*Text);
		return;
	}

	*SizeX = FCanvasUtil(*(FRenderTarget**)&RenderTarget,RenderViewport->RI).DrawString(
		0,
		0,
		*Text,
		Font,
		FColor(0,0,0,0),
		1
		);

	INT Ch = (TCHARU)Font->RemapChar('Q');
	*SizeY = Font->Characters(Ch).VSize;

//	*SizeY = Font->Characters(0).VSize;

	unguard;
}

//
//	UScriptedTexture::execDrawTile
//

void UScriptedTexture::execDrawTile(FFrame& Stack,RESULT_DECL)
{
	guard(UScriptedTexture::execDrawTile);

	P_GET_FLOAT(X);
	P_GET_FLOAT(Y);
	P_GET_FLOAT(XL);
	P_GET_FLOAT(YL);
	P_GET_FLOAT(U);
	P_GET_FLOAT(V);
	P_GET_FLOAT(UL);
	P_GET_FLOAT(VL);
	P_GET_OBJECT(UMaterial,Material);
	P_GET_STRUCT(FColor,Color);
	P_FINISH;

	if( !Invalid )
	{
        if (GIsOpenGL)
        {
            V = VL - V;
            VL = -VL;
        }

		FCanvasUtil(*(FRenderTarget**)&RenderTarget,RenderViewport->RI).DrawTile(
			X,
			Y,
			X + XL,
			Y + YL,
			U,
			V,
			U + UL,
			V + VL,
			0.0f,
			Material,
			Color
			);
	}

	unguardexec;
}

//
//  FScriptedPortalSceneNode - sjs
//

class ENGINE_API FScriptedPortalSceneNode : public FCameraSceneNode
{
public:

	// Constructor.
    FScriptedPortalSceneNode(UViewport* InViewport,FRenderTarget* InRenderTarget,AActor* CameraActor,FVector CameraLocation,FRotator CameraRotation,FLOAT CameraFOV) : FCameraSceneNode(InViewport,InRenderTarget,CameraActor,CameraLocation,CameraRotation,CameraFOV)
    {
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
//	UScriptedTexture::execDrawPortal
//

void UScriptedTexture::execDrawPortal(FFrame& Stack,RESULT_DECL)
{
	guard(UScriptedTexture::execDrawPortal);

	P_GET_INT(X);
	P_GET_INT(Y);
	P_GET_INT(Width);
	P_GET_INT(Height);
	P_GET_OBJECT(AActor,CamActor);
	P_GET_VECTOR(CamLocation);
	P_GET_ROTATOR(CamRotation);
	P_GET_INT_OPTX(FOV,90);
	P_GET_UBOOL_OPTX(ClearZ,1);
	P_FINISH;

	// Save state.

	unclock(GScriptCycles);

	if( !Invalid )
	{
		RenderViewport->RI->PushState();

		FLOAT	SavedFovAngle = RenderViewport->Actor->FovAngle;
		RenderViewport->Actor->FovAngle = FOV;

		// Clear the portal viewport.

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

		FCanvasUtil(*(FRenderTarget**)&RenderTarget,RenderViewport->RI).DrawTile(
			X,Y,
			X + Width,Y + Height,
			0,0,
			0,0,
			1,
			StencilFinalBlend,
			FColor(0,0,0)
			);

		RenderViewport->RI->SetViewport(X,Y,Width,Height);

		// Render the scene.

		FScriptedPortalSceneNode	SceneNode(RenderViewport,*(FRenderTarget**)&RenderTarget,CamActor,CamLocation,CamRotation,FOV);

		SceneNode.Render(RenderViewport->RI);

		// Restore state.

		RenderViewport->Actor->FovAngle = SavedFovAngle;

		RenderViewport->RI->PopState();
	}

	clock(GScriptCycles);

	unguardexec;
}

