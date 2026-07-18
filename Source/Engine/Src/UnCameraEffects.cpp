/*=============================================================================
	UnCameraEffects.cpp: Camera effect implemention
	Copyright 2001-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(USubActionCameraEffect);
IMPLEMENT_CLASS(UCameraEffect);
IMPLEMENT_CLASS(UMotionBlur);
IMPLEMENT_CLASS(UCameraOverlay);

//
//	USubActionCameraEffect::Update
//

UBOOL USubActionCameraEffect::Update(FLOAT Percent,ASceneManager* SceneManager)
{
	guard(USubActionCameraEffect::Update);

	if(!UMatSubAction::Update(Percent,SceneManager))
		return 0;

	if(IsRunning())
	{
        APlayerController*	Controller = SceneManager->Viewer ? SceneManager->Viewer->GetAPlayerController() : NULL;//Cast<APlayerController>(SceneManager->Viewer);

		if(Controller && CameraEffect)
		{
			if(PctDuration > 0.0f)
				CameraEffect->Alpha = Lerp(StartAlpha,EndAlpha,(Percent - PctStarting) / PctDuration);
			else
				CameraEffect->Alpha = EndAlpha;

			if(CameraEffect->Alpha <= 0.0f || (Status == SASTATUS_Ending && DisableAfterDuration))
				Controller->eventRemoveCameraEffect(CameraEffect);
			else
				Controller->eventAddCameraEffect(CameraEffect,1);
		}
	}

	return 1;

	unguard;
}

//
//	USubActionCameraEffect::GetStatString
//

FString USubActionCameraEffect::GetStatString()
{
	guard(USubActionCameraEffect:GetStatString);

	return TEXT("");

	unguard;
}

//
//	UMotionBlur::Destroy
//

void UMotionBlur::Destroy()
{
	guard(UMotionBlur::Destroy);

	Super::Destroy();

	for(INT TargetIndex = 0;TargetIndex < ARRAY_COUNT(RenderTargets);TargetIndex++)
		if(RenderTargets[TargetIndex])
		{
			delete RenderTargets[TargetIndex];
			RenderTargets[TargetIndex] = NULL;
		}

	unguard;
}

//
//	UMotionBlur::PreRender
//

void UMotionBlur::PreRender(UViewport* Viewport,FRenderInterface* RI)
{
	guard(UMotionBlur::PreRender);

	// If necessary, create new render targets.

	INT Width = Viewport->SizeX;
	INT Height = Viewport->SizeY;
	if (!Viewport->RenDev->SupportsRenderToTextureNPOT)  // must be power of two?
	{
		for (Width = 1; Width < Viewport->SizeX; Width <<= 1) {}
		for (Height = 1; Height < Viewport->SizeY; Height <<= 1) {}
	}

	for(INT TargetIndex = 0;TargetIndex < ARRAY_COUNT(RenderTargets);TargetIndex++)
	{
		UBOOL	Clear = 0;

		if(!RenderTargets[TargetIndex])
		{
			RenderTargets[TargetIndex] = new(TEXT("BlurRenderTarget")) FAuxRenderTarget(Width,Height,TEXF_RGBA8);
			Clear = 1;
		}
		else if(RenderTargets[TargetIndex]->Width != Width || RenderTargets[TargetIndex]->Height != Height)
		{
			RenderTargets[TargetIndex]->Width = Width;
			RenderTargets[TargetIndex]->Height = Height;
			RenderTargets[TargetIndex]->Revision++;
			Clear = 1;
		}

		if(Clear)
		{
			RI->PushState();

			RI->SetRenderTarget(RenderTargets[TargetIndex]);
			RI->Clear();

			RI->PopState();
		}
	}

	// Render the scene into the first render target.

	RI->PushState();

	RI->SetRenderTarget(RenderTargets[0]);
	RI->SetViewport(0,0,Viewport->SizeX,Viewport->SizeY);
	RI->Clear(1,FColor(0,0,0),1,1.0f,1,~DEPTH_COMPLEXITY_MASK(Viewport));

	unguard;
}

//
//	UMotionBlur::PostRender
//

void UMotionBlur::PostRender(UViewport* Viewport,FRenderInterface* RI)
{
	guard(UMotionBlur::PostRender);

	// Blend the new frame into the last frame, using BlurAlpha as the blending factor.

	DECLARE_STATIC_UOBJECT(
		UProxyBitmapMaterial,
		RenderTargetTexture,
		{
			RenderTargetTexture->SetTextureInterface(RenderTargets[0]);
		}
		);

	DECLARE_STATIC_UOBJECT(
		UVertexColor,
		VertexColor,
		{
		}
		);

	DECLARE_STATIC_UOBJECT(
		UShader,
		BlurShader,
		{
			BlurShader->Diffuse = RenderTargetTexture;
			BlurShader->Opacity = VertexColor;
		}
		);

	RI->SetRenderTarget(RenderTargets[1]);
	RI->SetViewport(0,0,Viewport->SizeX,Viewport->SizeY);

	FLOAT	FrameAlpha = Clamp(Alpha * (1.0f - (Viewport->Actor->Level->TimeSeconds - LastFrameTime) / 0.1f),0.0f,1.0f);

	FCanvasUtil(&Viewport->RenderTarget,RI).DrawTile(
		0,0,
		Viewport->SizeX, Viewport->SizeY,
		0,0,
		Viewport->SizeX, Viewport->SizeY,
		0.0f,
		BlurShader,
		FColor(255,255,255,BlurAlpha * FrameAlpha + 255 * (1.0f - FrameAlpha))
		);

	RI->PopState();

	// Copy the result to the frame buffer.

	DECLARE_STATIC_UOBJECT(
		UProxyBitmapMaterial,
		BltTexture,
		{
			BltTexture->SetTextureInterface(RenderTargets[1]);
		}
		);

	FCanvasUtil(&Viewport->RenderTarget,RI).DrawTile(
		0,0,
		Viewport->SizeX, Viewport->SizeY,
		0,0,
		Viewport->SizeX, Viewport->SizeY,
		1.0f,
		BltTexture,
		FColor(255,255,255,255)
		);

	LastFrameTime = Viewport->Actor->Level->TimeSeconds;

	unguard;
}

//
//	UCameraOverlay::PostRender
//

void UCameraOverlay::PostRender(UViewport* Viewport,FRenderInterface* RI)
{
	guard(UCameraOverlay::PostRender);

	if(OverlayMaterial)
	{
		// Render OverlayMaterial over the whole screen.

		FCanvasUtil(&Viewport->RenderTarget,RI).DrawTile(
			0,0,
			Viewport->SizeX,Viewport->SizeY,
			0,0,
			OverlayMaterial->MaterialUSize(),OverlayMaterial->MaterialVSize(),
			0.0f,
			OverlayMaterial,
			FColor(OverlayColor.R,OverlayColor.G,OverlayColor.B,OverlayColor.A * Alpha)
			);
	}

	unguard;
}

