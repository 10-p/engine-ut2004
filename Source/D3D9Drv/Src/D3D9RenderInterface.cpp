/*=============================================================================
	D3DRenderInterface.cpp: Unreal Direct3D rendering interface implementation.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "D3D9Drv.h"

//
//	FD3D9RenderInterface::FD3D9SavedState::FD3D9SavedState
//
FD3D9RenderInterface::FD3D9SavedState::FD3D9SavedState()
{
	guard(FD3D9RenderInterface::FD3D9SavedState::FD3D9SavedState);

	ViewportX			= 0;
	ViewportY			= 0;
	ViewportWidth		= 0;
	ViewportHeight		= 0;

	ZBias				= 0;

	StencilTest			= CF_Always;
	StencilFailOp		= SO_Keep;
	StencilZFailOp		= SO_Keep;
	StencilPassOp		= SO_Keep;
	StencilRef			= 0;
	StencilMask			= 0xffffffff;
	StencilWriteMask	= 0xffffffff;
	
	LocalToWorld		= FMatrix::Identity;
	WorldToCamera		= FMatrix::Identity;
	CameraToScreen		= FMatrix::Identity;

	VertexShader		= NULL;
	appMemzero(Streams,sizeof(Streams));
	appMemzero(StreamStrides,sizeof(StreamStrides));
	appMemzero(StreamOffsets,sizeof(StreamOffsets));

	NumStreams			= 0;
	
	IndexBuffer			= NULL;
	IndexBufferBase		= 0;

	CullMode			= D3DCULL_NONE;

	UseDetailTexturing  = 1;
	UseDynamicLighting	= 0;
	UseStaticLighting	= 1;
	LightingModulate2X	= 0;
	LightingOnly		= 0;
	Lightmap			= NULL;
	AmbientLightColor	= FColor(0,0,0,0);

	appMemzero(Lights,sizeof(Lights));
	appMemzero(LightEnabled,sizeof(LightEnabled));

	DistanceFogEnabled	= 0;
	DistanceFogStart	= 0.0f;
	DistanceFogEnd		= 0.0f;
	DistanceFogColor	= FColor(0,0,0,0);

	NPatchTesselation	= 1.0f;

	appMemzero(MaterialPasses,sizeof(MaterialPasses));
	NumMaterialPasses	= 0;
	CurrentMaterialState= NULL;

	unguard;
}

//
//	FD3D9RenderInterface::FD3D9RenderInterface
//
FD3D9RenderInterface::FD3D9RenderInterface(UD3D9RenderDevice* InRenDev)
{
	guard(FD3D9RenderInterface::FD3D9RenderInterface);

	RenDev		= InRenDev;
	Viewport	= NULL;

	PrecacheMode	= PRECACHE_All;

	HitCount	= 0;
	HitData		= NULL;
	HitSize		= NULL;

	SavedStateIndex	= 0;
	CurrentState	= &SavedStates[SavedStateIndex];

	unguard;
}

//
//	FD3D9RenderInterface::Locked
//
void FD3D9RenderInterface::Locked( UViewport* InViewport, BYTE* InHitData,INT* InHitSize)
{
	guard(FD3D9RenderInterface::Locked)

	Viewport = InViewport;
	HitCount = 0;
	HitData = InHitData;
	HitSize = InHitSize;

	// Determine the current render target and depth stencil surfaces.

	IDirect3DSurface9*	RenderTargetSurface = NULL;
	IDirect3DSurface9*	DepthStencilSurface = NULL;

	verify(!FAILED(RenDev->Direct3DDevice9->GetRenderTarget(0, &RenderTargetSurface)));
	verify(!FAILED(RenDev->Direct3DDevice9->GetDepthStencilSurface(&DepthStencilSurface)));

	// Setup the initial state

	CurrentState->ViewportX				= 0;
	CurrentState->ViewportY				= 0;
	CurrentState->ViewportWidth			= Viewport->SizeX;
	CurrentState->ViewportHeight		= Viewport->SizeY;
	CurrentState->DepthStencilSurface	= DepthStencilSurface;
	CurrentState->RenderTargetSurface	= RenderTargetSurface;

	// Save the render state.
	SavedStateIndex = 0;
	PushState();

	unguard;
}

//
//	FD3D9RenderInterface::Unlocked
//
void FD3D9RenderInterface::Unlocked()
{
	guard(FD3D9RenderInterface::Unlocked);

	// Restore the initial state.
	PopState();
	check(SavedStateIndex == 0);

	// Release the render target pointers.
	CurrentState->RenderTargetSurface->Release();
	CurrentState->RenderTargetSurface = NULL;

	CurrentState->DepthStencilSurface->Release();
	CurrentState->DepthStencilSurface = NULL;

	unguard;
}

//
//	FD3D9RenderInterface::PushState
//
void FD3D9RenderInterface::PushState()
{
	guard(F3DRenderInterface::PushState);

	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_PushStateCycles));
	check(SavedStateIndex+1 < MAX_STATESTACKDEPTH);

	CurrentState = &SavedStates[SavedStateIndex+1];
	appMemcpy( CurrentState, &SavedStates[SavedStateIndex], sizeof(FD3D9SavedState) );
	SavedStateIndex++;

	for(INT PassIndex = 0;PassIndex < CurrentState->NumMaterialPasses;PassIndex++)
		CurrentState->MaterialPasses[PassIndex]->NumRefs++;

	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_PushStateCycles));
	GStats.DWORDStats(RenDev->D3DStats.STATS_PushStateCalls)++;

	unguard;
}

//
//	FD3D9RenderInterface::PopState
//
void FD3D9RenderInterface::PopState()
{
	guard(F3DRenderInterface::PopState);

	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_PopStateCycles));

	// Pop a saved state off the saved state stack.
	if( SavedStateIndex == 0 )
		appErrorf(TEXT("PopState stack underflow"));
	
	FD3D9SavedState& OldState	= SavedStates[SavedStateIndex--];
	FD3D9SavedState& NewState	= SavedStates[SavedStateIndex];

	CurrentState = &SavedStates[SavedStateIndex];

	// Restore lighting.
	RenDev->DeferredState.SetRenderState( RS_LIGHTING, NewState.UseDynamicLighting ? TRUE : FALSE );
	RenDev->DeferredState.SetRenderState( RS_COLORVERTEX, NewState.UseStaticLighting ? TRUE : FALSE );

	// Apply the restored state.
	if( OldState.RenderTargetSurface != NewState.RenderTargetSurface ||
		OldState.DepthStencilSurface != NewState.DepthStencilSurface)
	{
		RenDev->Direct3DDevice9->SetRenderTarget(0, NewState.RenderTargetSurface);
		RenDev->Direct3DDevice9->SetDepthStencilSurface(NewState.DepthStencilSurface);

		// SetRenderTarget resets the viewport, so SetViewport needs to be called again.
		SetViewport(NewState.ViewportX,NewState.ViewportY,NewState.ViewportWidth,NewState.ViewportHeight);
	}
	else 
	if( OldState.ViewportX		!= NewState.ViewportX		||
		OldState.ViewportY		!= NewState.ViewportY		||
		OldState.ViewportWidth	!= NewState.ViewportWidth	||
		OldState.ViewportHeight	!= NewState.ViewportHeight)
	{
		SetViewport(NewState.ViewportX,NewState.ViewportY,NewState.ViewportWidth,NewState.ViewportHeight);
	}

	FLOAT RealZBias = -5.f * NewState.ZBias / FAR_CLIPPING_PLANE;
	RenDev->DeferredState.SetRenderState( RS_DEPTHBIAS			, *((DWORD*)&RealZBias) );
	RenDev->DeferredState.SetRenderState( RS_SLOPESCALEDEPTHBIAS, *((DWORD*)&RealZBias) );

	RenDev->DeferredState.SetRenderState( RS_STENCILFUNC, GetD3DCompFunc(NewState.StencilTest) );
	RenDev->DeferredState.SetRenderState( RS_STENCILREF, NewState.StencilRef & 0xff );
	RenDev->DeferredState.SetRenderState( RS_STENCILMASK, NewState.StencilMask & 0xff );
	RenDev->DeferredState.SetRenderState( RS_STENCILFAIL, GetD3DStencilOp(NewState.StencilFailOp) );
	RenDev->DeferredState.SetRenderState( RS_STENCILZFAIL, GetD3DStencilOp(NewState.StencilZFailOp) );
	RenDev->DeferredState.SetRenderState( RS_STENCILPASS, GetD3DStencilOp(NewState.StencilPassOp) );
	RenDev->DeferredState.SetRenderState( RS_STENCILWRITEMASK, NewState.StencilWriteMask & 0xff );
	RenDev->DeferredState.SetTransform( TS_WORLD, (D3DMATRIX*) &NewState.LocalToWorld);
	RenDev->DeferredState.SetTransform( TS_VIEW, (D3DMATRIX*) &NewState.WorldToCamera);
	RenDev->DeferredState.SetTransform( TS_PROJECTION, (D3DMATRIX*) &NewState.CameraToScreen);

	if( NewState.VertexShader ) 
	{
		RenDev->DeferredState.SetVertexShader(NewState.VertexShader->Shader);
		RenDev->DeferredState.SetDeclaration(NewState.VertexShader->Decl);
	}
	else
	{
		RenDev->DeferredState.SetVertexShader(NULL);
		RenDev->DeferredState.SetDeclaration(NULL);
	}

	CurrentState->VertexShader = NewState.VertexShader;

	for(INT Index = 0;Index < NewState.NumStreams;Index++)
		RenDev->DeferredState.SetStreamSource(Index,NewState.Streams[Index]->Direct3DVertexBuffer9,NewState.StreamOffsets[Index],NewState.StreamStrides[Index]);

	for(INT Index = NewState.NumStreams;Index < OldState.NumStreams;Index++)
		RenDev->DeferredState.SetStreamSource(Index,NULL,0, 0);

	CurrentState->NumStreams = NewState.NumStreams;

	if(NewState.IndexBuffer)
		RenDev->DeferredState.SetIndices(NewState.IndexBuffer->Direct3DIndexBuffer9);
	else
		RenDev->DeferredState.SetIndices(NULL);
	CurrentState->IndexBufferBase = NewState.IndexBufferBase;

	RenDev->DeferredState.SetRenderState( RS_CULLMODE, NewState.CullMode );
	RenDev->DeferredState.SetRenderState( RS_AMBIENT, NewState.AmbientLightColor );

	for(INT Index = 0;Index < 8;Index++)
	{
		RenDev->DeferredState.LightEnable(Index,NewState.LightEnabled[Index]);
		RenDev->DeferredState.SetLight(Index,&NewState.Lights[Index]);
	}

	RenDev->DeferredState.SetRenderState( RS_FOGENABLE, NewState.DistanceFogEnabled ? TRUE : FALSE );
	RenDev->DeferredState.SetRenderState( RS_FOGCOLOR, NewState.DistanceFogColor );
	RenDev->DeferredState.SetRenderState( RS_FOGSTART, *(DWORD*)(&NewState.DistanceFogStart) );
	RenDev->DeferredState.SetRenderState( RS_FOGEND, *(DWORD*)(&NewState.DistanceFogEnd) );

	// Restore material state.

	for(INT PassIndex = 0;OldState.MaterialPasses[PassIndex];PassIndex++)
	{
		if(--OldState.MaterialPasses[PassIndex]->NumRefs == 0)
			MaterialStatePool.FreeState(OldState.MaterialPasses[PassIndex]);

		OldState.MaterialPasses[PassIndex] = NULL;
	}

	CurrentState->CurrentMaterialState = NULL;

	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_PopStateCycles));
	GStats.DWORDStats(RenDev->D3DStats.STATS_PopStateCalls)++;

	unguard;
}

//
//	FD3D9RenderInterface::SetRenderTarget
//
UBOOL FD3D9RenderInterface::SetRenderTarget(FRenderTarget* RenderTarget)
{
	guard(FD3D9RenderInterface::SetRenderTarget);

	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_SetRenderTargetCycles));

	QWORD			CacheId = RenderTarget->GetCacheId();
	FD3D9Texture*	D3D9Texture = (FD3D9Texture*) RenDev->GetCachedResource(CacheId);

	if(!D3D9Texture)
		D3D9Texture = new(TEXT("FD3D9Texture")) FD3D9Texture(RenDev,CacheId);

	if(D3D9Texture->CachedRevision != RenderTarget->GetRevision())
	{
		if(D3D9Texture->Cache(RenderTarget))
			return 0;
	}

	if( D3D9Texture->RenderTargetSurface 
	&&  D3D9Texture->DepthStencilSurface
	&&  (   CurrentState->RenderTargetSurface != D3D9Texture->RenderTargetSurface 
	     || CurrentState->DepthStencilSurface != D3D9Texture->DepthStencilSurface
		)
	)
	{
		HRESULT	Result = RenDev->Direct3DDevice9->SetRenderTarget(0, D3D9Texture->RenderTargetSurface);

		if( FAILED(Result) )
			appErrorf(TEXT("SetRenderTarget failed(%s)."),*D3DError(Result));

		Result = RenDev->Direct3DDevice9->SetDepthStencilSurface(D3D9Texture->DepthStencilSurface);

		if( FAILED(Result) )
			appErrorf(TEXT("SetDepthStencilSurface failed(%s)."),*D3DError(Result));

		CurrentState->RenderTargetSurface = D3D9Texture->RenderTargetSurface;
		CurrentState->DepthStencilSurface = D3D9Texture->DepthStencilSurface;
		CurrentState->ViewportX = 0;
		CurrentState->ViewportY = 0;
		CurrentState->ViewportWidth = D3D9Texture->CachedWidth;
		CurrentState->ViewportHeight = D3D9Texture->CachedHeight;
	}

	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_SetRenderTargetCycles));
	GStats.DWORDStats(RenDev->D3DStats.STATS_SetRenderTargetCalls)++;

	return 1;

	unguard;
}

//
//  FD3D9RenderInterface::SetViewport
//
void FD3D9RenderInterface::SetViewport(INT X,INT Y,INT Width,INT Height)
{
	guard(FD3D9RenderInterface::SetViewport);

	D3DVIEWPORT9	Viewport9;

	Viewport9.X		= X;
	Viewport9.Y		= Y;
	Viewport9.Width = Width;
	Viewport9.Height= Height;
	Viewport9.MinZ	= 0.0f;
	Viewport9.MaxZ	= 1.0f;

	RenDev->Direct3DDevice9->SetViewport(&Viewport9);

	CurrentState->ViewportX = X;
	CurrentState->ViewportY = Y;
	CurrentState->ViewportWidth	= Width;
	CurrentState->ViewportHeight = Height;

	unguard;
}

//
// FD3D9RenderInterface::SetMaterialBlending
//
void FD3D9RenderInterface::SetMaterialBlending( FD3D9MaterialState* NewMaterialState, D3DCULL CullMode )
{
	guard(FD3D9RenderInterface::SetMaterialBlending);

	if(CurrentState->CurrentMaterialState == NewMaterialState)
		return;

	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_SetMaterialBlendingCycles));

	if(NewMaterialState->PixelShader != PS_None)
		RenDev->DeferredState.SetPixelShader( RenDev->GetPixelShader(NewMaterialState->PixelShader)->Shader );
	else
		RenDev->DeferredState.SetPixelShader( NULL );

	RenDev->DeferredState.SetRenderState( RS_ALPHABLENDENABLE, NewMaterialState->AlphaBlending ? TRUE : FALSE );
	RenDev->DeferredState.SetRenderState( RS_ALPHAREF, NewMaterialState->AlphaRef );
	RenDev->DeferredState.SetRenderState( RS_ALPHAFUNC, D3DCMP_GREATER );
	RenDev->DeferredState.SetRenderState( RS_ALPHATESTENABLE, NewMaterialState->AlphaTest ? TRUE : FALSE );

	switch(NewMaterialState->FillMode)
	{
	case FM_Wireframe:
	case FM_Solid:
		RenDev->DeferredState.SetRenderState( RS_FILLMODE, NewMaterialState->FillMode==FM_Wireframe ? D3DFILL_WIREFRAME : D3DFILL_SOLID );
		RenDev->DeferredState.SetTextureStageState( 0, TSS_COLOROP, D3DTOP_MODULATE );
		break;
	case FM_FlatShaded:
		RenDev->DeferredState.SetTextureStageState( 0, TSS_COLOROP, D3DTOP_SELECTARG2 );
		RenDev->DeferredState.SetRenderState( RS_FILLMODE, D3DFILL_SOLID );
		break;
	}

	RenDev->DeferredState.SetRenderState( RS_ZFUNC,NewMaterialState->ZTest ? D3DCMP_LESSEQUAL : D3DCMP_ALWAYS );
	RenDev->DeferredState.SetRenderState( RS_ZWRITEENABLE, NewMaterialState->ZWrite ? TRUE : FALSE );
	RenDev->DeferredState.SetRenderState( RS_CULLMODE,NewMaterialState->TwoSided ? D3DCULL_NONE : CullMode );

	// Source/destination blending
	RenDev->DeferredState.SetRenderState( RS_SRCBLEND, NewMaterialState->SrcBlend );
	RenDev->DeferredState.SetRenderState( RS_DESTBLEND, NewMaterialState->DestBlend );

	// Texture Factor
	RenDev->DeferredState.SetRenderState( RS_TEXTUREFACTOR, NewMaterialState->TFactorColor );

	// Texture stages and blending
	for( INT s=0;s<NewMaterialState->StagesUsed;s++ )
	{
		if( NewMaterialState->Stages[s].Texture )
		{
			if ( NewMaterialState->Stages[s].Texture->Direct3DCubeTexture9 )
				RenDev->DeferredState.SetTexture(s,NewMaterialState->Stages[s].Texture->Direct3DCubeTexture9);
			else
				RenDev->DeferredState.SetTexture(s,NewMaterialState->Stages[s].Texture->Direct3DTexture9);
		}
		else
			RenDev->DeferredState.SetTexture(s,NULL);

		// set texture clamping
		RenDev->DeferredState.SetSamplerState( s, SAMP_ADDRESSU, NewMaterialState->Stages[s].TextureAddressU );
		RenDev->DeferredState.SetSamplerState( s, SAMP_ADDRESSV, NewMaterialState->Stages[s].TextureAddressV );
		RenDev->DeferredState.SetSamplerState( s, SAMP_ADDRESSW, NewMaterialState->Stages[s].TextureAddressW );

		if( NewMaterialState->Stages[s].TextureMipLODBias < -100.f )
			NewMaterialState->Stages[s].TextureMipLODBias = RenDev->DefaultTexMipBias;
        RenDev->DeferredState.SetSamplerState( s, SAMP_MIPMAPLODBIAS, *(DWORD*)&NewMaterialState->Stages[s].TextureMipLODBias ); // sjs

		// alpha/color ops
		RenDev->DeferredState.SetTextureStageState( s, TSS_COLOROP,NewMaterialState->Stages[s].ColorOp );
		RenDev->DeferredState.SetTextureStageState( s, TSS_ALPHAOP,NewMaterialState->Stages[s].AlphaOp );

		// alpha/color args
		RenDev->DeferredState.SetTextureStageState( s, TSS_COLORARG0, NewMaterialState->Stages[s].ColorArg0 );
		RenDev->DeferredState.SetTextureStageState( s, TSS_COLORARG1, NewMaterialState->Stages[s].ColorArg1 );
		RenDev->DeferredState.SetTextureStageState( s, TSS_COLORARG2, NewMaterialState->Stages[s].ColorArg2 );
		RenDev->DeferredState.SetTextureStageState( s, TSS_ALPHAARG0, NewMaterialState->Stages[s].AlphaArg0 );
		RenDev->DeferredState.SetTextureStageState( s, TSS_ALPHAARG1, NewMaterialState->Stages[s].AlphaArg1 );
		RenDev->DeferredState.SetTextureStageState( s, TSS_ALPHAARG2, NewMaterialState->Stages[s].AlphaArg2 );
		RenDev->DeferredState.SetTextureStageState( s, TSS_RESULTARG, NewMaterialState->Stages[s].ResultArg );

		RenDev->DeferredState.SetTextureStageState( s, TSS_TEXCOORDINDEX, NewMaterialState->Stages[s].TexCoordIndex );
	
		if( NewMaterialState->Stages[s].TextureTransformsEnabled )
			RenDev->DeferredState.SetTextureStageState( s, TSS_TEXTURETRANSFORMFLAGS, NewMaterialState->Stages[s].TexCoordCount );
		else
			RenDev->DeferredState.SetTextureStageState( s, TSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );

		if( NewMaterialState->Stages[s].TextureTransformsEnabled )
			RenDev->DeferredState.SetTransform( (ED3DTransformState)(TS_TEXTURE0 + s), &NewMaterialState->Stages[s].TextureTransformMatrix );
	}

	for( INT s=NewMaterialState->StagesUsed; s<(INT)RenDev->DeviceCaps9.MaxTextureBlendStages; s++ )
	{
		NewMaterialState->Stages[s].ColorOp	= D3DTOP_DISABLE;
		NewMaterialState->Stages[s].AlphaOp	= D3DTOP_DISABLE;
		NewMaterialState->Stages[s].Texture	= NULL;

		RenDev->DeferredState.SetTextureStageState( s, TSS_COLOROP, D3DTOP_DISABLE );
		RenDev->DeferredState.SetTextureStageState( s, TSS_ALPHAOP, D3DTOP_DISABLE );
		RenDev->DeferredState.SetTexture( s, NULL );
	}

	CurrentState->CurrentMaterialState = NewMaterialState;

	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_SetMaterialBlendingCycles));
	GStats.DWORDStats(RenDev->D3DStats.STATS_SetMaterialBlendingCalls)++;

	unguard;
}



//
//	FD3D9RenderInterface::Clear
//
void FD3D9RenderInterface::Clear(UBOOL UseColor,FColor Color,UBOOL UseDepth,FLOAT Depth,UBOOL UseStencil,DWORD Stencil)
{
	guard(FD3D9RenderInterface::Clear);

	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_ClearCycles));
	RenDev->Direct3DDevice9->Clear(
		0,
		NULL,
		  (UseColor ? D3DCLEAR_TARGET : 0) 
		| (UseDepth ? D3DCLEAR_ZBUFFER : 0) 
		| (((RenDev->UseStencil || GIsEditor) & UseStencil) ? D3DCLEAR_STENCIL : 0),
		Color,
		Depth,
		Stencil & 0xff
		);

	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_ClearCycles));
	GStats.DWORDStats(RenDev->D3DStats.STATS_ClearCalls)++;

	unguard;
}

//
//	FD3D9RenderInterface::PushHit
//
void FD3D9RenderInterface::PushHit(const BYTE* Data,INT Count)
{
	guard(UD3D9RenderDevice::PushHit);

	UViewport*	LockedViewport = RenDev->LockedViewport;
	HRESULT		Result;

	check(LockedViewport->HitYL<=HIT_SIZE);
	check(LockedViewport->HitXL<=HIT_SIZE);

	// Get the current render target surface.
	IDirect3DSurface9*	RenderTarget;

	Result = RenDev->Direct3DDevice9->GetRenderTarget(0, &RenderTarget);

	if( FAILED(Result) )
	{
		debugf(TEXT("D3D Driver: GetRenderTarget failed (%s)"),*D3DError(Result));
		return;
	}

	// Lock the render target.
	D3DLOCKED_RECT	LockedRect;

	Result = RenderTarget->LockRect(&LockedRect,NULL,0);

	if( FAILED(Result) )
	{
		debugf(TEXT("D3D Driver: LockRect failed (%s)"),*D3DError(Result));
		return;
	}

	// Save the passed info on the working stack.
	INT	Index = HitStack.Add(Count);

	appMemcpy(&HitStack(Index),Data,Count);

	// Cleanup under cursor.
	switch( LockedViewport->ColorBytes )
	{
		case 2:
		{
			_WORD* src = (_WORD*) LockedRect.pBits;
			src = (_WORD*) ((BYTE*)src + LockedViewport->HitX * 2 + LockedViewport->HitY * LockedRect.Pitch);
			for( INT Y=0; Y<LockedViewport->HitYL; Y++, src=(_WORD*)((BYTE*)src + LockedRect.Pitch) )
			{
				for( INT X=0; X<LockedViewport->HitXL; X++ )
				{
					HitPixels[X][Y] = src[X];
					src[X] = IGNOREPIX;
				}
			}
			break;
		}
		case 3:
		{
			BYTE* src = (BYTE*) LockedRect.pBits;
			src = src + LockedViewport->HitX*3  + LockedViewport->HitY * LockedRect.Pitch;
			for( INT Y=0; Y<LockedViewport->HitYL; Y++, src+=LockedRect.Pitch )
			{
				for( INT X=0; X<LockedViewport->HitXL; X++ )
				{
					HitPixels[X][Y] = *((DWORD*)&src[X*3]);
					*((DWORD*)&src[X*3]) = IGNOREPIX;
				}
			}			
			break;
		}
		case 4:
		{
			DWORD* src = (DWORD*) LockedRect.pBits;
			src = (DWORD*)((BYTE*)src + LockedViewport->HitX * 4 + LockedViewport->HitY * LockedRect.Pitch);
			for( INT Y=0; Y<LockedViewport->HitYL; Y++, src=(DWORD*)((BYTE*)src + LockedRect.Pitch) )
			{
				for( INT X=0; X<LockedViewport->HitXL; X++ )
				{
					HitPixels[X][Y] = src[X];
					src[X] = IGNOREPIX;
				}
			}
			break;
		}
	
	}

	// Unlock the render target, and release our reference to it.
	RenderTarget->UnlockRect();
	RenderTarget->Release();

	unguard;
}

//
//	FD3D9RenderInterface::PopHit
//
void FD3D9RenderInterface::PopHit(INT Count,UBOOL Force)
{
	guard(FD3D9RenderInterface::PopHit);

	UViewport*	LockedViewport = RenDev->LockedViewport;
	HRESULT		Result;

	//debugf(TEXT("POPHIT stacknum   %i  Count %i "),HitStack.Num(),Count);
	check(Count <= HitStack.Num());
	UBOOL Hit=0;
	FColor HitColor = FColor(0,0,0);

	// Get the current render target surface.
	IDirect3DSurface9*	RenderTarget;

	Result = RenDev->Direct3DDevice9->GetRenderTarget(0, &RenderTarget);

	if( FAILED(Result) )
	{
		debugf(TEXT("D3D Driver: GetRenderTarget failed (%s)"),*D3DError(Result));
		return;
	}

	// Lock the render target.
	D3DLOCKED_RECT	LockedRect;

	Result = RenderTarget->LockRect(&LockedRect,NULL,0);

	if( FAILED(Result) )
	{
		debugf(TEXT("D3D Driver: LockRect failed (%s)"),*D3DError(Result));
		return;
	}

	// Check under cursor and restore.
	switch( LockedViewport->ColorBytes )
	{
		case 2:
		{
			_WORD* src = (_WORD*) LockedRect.pBits;
			src = (_WORD*) ((BYTE*)src + LockedViewport->HitX * 2 + LockedViewport->HitY * LockedRect.Pitch);
			for( INT Y=0; Y<LockedViewport->HitYL; Y++, src=(_WORD*)((BYTE*)src + LockedRect.Pitch) )
			{
				for( INT X=0; X<LockedViewport->HitXL; X++ )
				{
					if( src[X] != IGNOREPIX && !Hit)
					{
						HitColor = FColor( (src[X]>>11)<<3, ((src[X]>>6)&0x3f)<<2, (src[X]&0x1f)<<3 );
						Hit=1;
					}
					src[X] = (_WORD)HitPixels[X][Y];	
				
				}
			}
			break;
		}
		case 3:
		{
			BYTE* src = (BYTE*) LockedRect.pBits;
			src = src + LockedViewport->HitX*3  + LockedViewport->HitY * LockedRect.Pitch;
			for( INT Y=0; Y<LockedViewport->HitYL; Y++, src+=LockedRect.Pitch )
			{
				for( INT X=0; X<LockedViewport->HitXL; X++ )
				{
					if( *((DWORD*)&src[X*3]) != IGNOREPIX && !Hit )
					{
						HitColor = FColor( src[X*3]+2, src[X*3]+1, src[X*3] );
						Hit=1;
					}
					*((DWORD*)&src[X*3]) = HitPixels[X][Y];						
				}
			}			
			break;
		}
		case 4:
		{
			DWORD* src = (DWORD*) LockedRect.pBits;
			src = (DWORD*)((BYTE*)src + LockedViewport->HitX * 4 + LockedViewport->HitY * LockedRect.Pitch);
			for( INT Y=0; Y<LockedViewport->HitYL; Y++, src=(DWORD*)((BYTE*)src + LockedRect.Pitch) )
			{
				for( INT X=0; X<LockedViewport->HitXL; X++ )
				{						
					if ( src[X] != IGNOREPIX  && !Hit ) 
					{
						HitColor = FColor( (src[X]>>16)&0xff, (src[X]>>8)&0xff, (src[X])&0xff );
						Hit=1;
					}
					src[X] = HitPixels[X][Y];
				}
			}
			break;
		}		
	}

	// Unlock the render target, and release our reference to it.
	RenderTarget->UnlockRect();
	RenderTarget->Release();

	// Handle hit.
	if( Hit || Force )
	{
		((HHitProxy*)&HitStack(HitStack.Num()-Count))->HitColor = HitColor;
		if( HitStack.Num() <= *HitSize )
		{
			HitCount = HitStack.Num();
			appMemcpy( HitData, &HitStack(0), HitCount );
		}
		else HitCount = 0;
	}
	// Remove the passed info from the working stack.
	HitStack.Remove( HitStack.Num()-Count, Count );
	unguard;
}

//
//	FD3D9RenderInterface::SetCullMode
//
void FD3D9RenderInterface::SetCullMode(ECullMode CullMode)
{
	guard(FD3D9RenderInterface::SetCullMode);

	// Determine the actual cull mode to use.
	D3DCULL	NewMode = D3DCULL_NONE;

	if(CullMode == CM_CW)
		NewMode = D3DCULL_CW;
	else if(CullMode == CM_CCW)
		NewMode = D3DCULL_CCW;
	else if(CullMode == CM_None)
		NewMode = D3DCULL_NONE;

	// Set the new cull mode.
	RenDev->DeferredState.SetRenderState( RS_CULLMODE, NewMode );
	CurrentState->CullMode = NewMode;

	unguard;
}

//
//	FD3D9RenderInterface::SetAmbientLight
//
void FD3D9RenderInterface::SetAmbientLight(FColor Color)
{
	guard(FD3D9RenderInterface::SetAmbientLight);

	RenDev->DeferredState.SetRenderState( RS_AMBIENT, Color );
	CurrentState->AmbientLightColor = Color;

	unguard;
}

//
//	FD3D9RenderInterface::EnableLighting
//
void FD3D9RenderInterface::EnableLighting(UBOOL UseDynamic, UBOOL UseStatic, UBOOL Modulate2X, FBaseTexture* LightmapTexture, UBOOL LightingOnly, FSphere LitSphere )
{
	guard(FD3D9RenderInterface::EnableLighting);

	FD3D9Texture* Lightmap = NULL;
	if( LightmapTexture )
		Lightmap = CacheTexture( LightmapTexture );

	CurrentState->Lightmap		= Lightmap;		
	CurrentState->LightingOnly	= LightingOnly;

	RenDev->DeferredState.SetRenderState( RS_LIGHTING, UseDynamic );
	CurrentState->UseDynamicLighting = UseDynamic;

	RenDev->DeferredState.SetRenderState( RS_COLORVERTEX, UseStatic );
	CurrentState->UseStaticLighting	= UseStatic;

	CurrentState->LightingModulate2X = Modulate2X;

	CurrentState->LitSphere = LitSphere;

	unguard;
}

//
//	UnrealAttenuation
//
static FLOAT UnrealAttenuation(FLOAT Distance,FLOAT Radius)
{
	if(Distance <= Radius)
	{
		FLOAT	A = Distance / Radius,					// Unreal's lighting model.
				B = (2 * A * A * A - 3 * A * A + 1);

		return B / A * A * 2.0f;
	}
	else
		return 0.0f;
}

//
//	FD3D9RenderInterface::SetLight
//
void FD3D9RenderInterface::SetLight(INT LightIndex,FDynamicLight* Light,FLOAT Scale) // sjs
{
	guard(FD3D9RenderInterface::SetLight);

	static float MaxD3DRange = appSqrt(FLT_MAX); // sjs - xbox libs freak out about illegal light ranges

	if(LightIndex < 8)
	{
		if(Light)
		{
			D3DLIGHT9*	LightD3D = &CurrentState->Lights[LightIndex];

			appMemzero(LightD3D,sizeof(D3DLIGHT9));

			if(Light->Actor && Light->Actor->LightEffect == LE_Sunlight) // gam
			{
				LightD3D->Type			= D3DLIGHT_DIRECTIONAL;
				LightD3D->Direction.x	= Light->Direction.X;
				LightD3D->Direction.y	= Light->Direction.Y;
				LightD3D->Direction.z	= Light->Direction.Z;

				LightD3D->Diffuse.r = Light->Color.X * 1.75f * Light->Alpha * Scale; // sjs
				LightD3D->Diffuse.g = Light->Color.Y * 1.75f * Light->Alpha * Scale; // sjs
				LightD3D->Diffuse.b = Light->Color.Z * 1.75f * Light->Alpha * Scale; // sjs
			}
			else if(Light->Actor && Light->Actor->LightEffect == LE_QuadraticNonIncidence || CurrentState->LitSphere.W == -1.0f ) // sjs
			{
				LightD3D->Type			= D3DLIGHT_POINT;
				LightD3D->Position.x	= Light->Position.X;
				LightD3D->Position.y	= Light->Position.Y;
				LightD3D->Position.z	= Light->Position.Z;
				LightD3D->Attenuation0 = 0.0f;
	            LightD3D->Attenuation1 = 0.0f;
	            LightD3D->Attenuation2 = 8.0f / Square(Light->Radius);
				LightD3D->Range = Light->Radius;
				LightD3D->Diffuse.r = Light->Color.X * Light->Alpha * Scale;
				LightD3D->Diffuse.g = Light->Color.Y * Light->Alpha * Scale;
				LightD3D->Diffuse.b = Light->Color.Z * Light->Alpha * Scale;
				LightD3D->Diffuse.a = 1.0f;
			}
			else
			{				
				LightD3D->Position.x	= Light->Position.X;
				LightD3D->Position.y	= Light->Position.Y;
				LightD3D->Position.z	= Light->Position.Z;
												
				// Directional lights. 
				if(Light->Actor && ( Light->Actor->LightEffect == LE_StaticSpot || Light->Actor->LightEffect == LE_Spotlight) ) 
				{
					LightD3D->Type			= D3DLIGHT_SPOT;
					LightD3D->Direction.x	= Light->Direction.X;
					LightD3D->Direction.y	= Light->Direction.Y;
					LightD3D->Direction.z	= Light->Direction.Z;					
					// Outer cone light limit. Fudge factor 1.2f makes it match lightmaps better with the current falloff.
					LightD3D->Phi			= 1.15f * appAcos( Square( 1.f - (FLOAT)(Light->Actor->LightCone)/256.f ) ); 
					// Inner cone.
					LightD3D->Theta			= 0.05f * LightD3D->Phi ; // Pretty arbitrary..
					// Outer-to-inner-cone falloff
					// higher: slower at center, faster at edge; lower = faster falloff at center.
					LightD3D->Falloff		= 1.00f; //  1.0f is linear, and supposed to be fastest for most hardware. 2.0f gives better gradual falloff though.

					// TODO: Special attenuation and range setting for spotlights - to more closely match software-computed spot lights on BSP and terrain ?
					FLOAT		CenterDistance = Max(0.1f,(CurrentState->LitSphere - Light->Position).Size()),
					MaxDistance = Clamp(CenterDistance + CurrentState->LitSphere.W,Light->Radius * 0.05f,Light->Radius * 0.9f),
					MinDistance = Clamp(CenterDistance - CurrentState->LitSphere.W,Light->Radius * 0.1f,Light->Radius * 0.95f),
					MinAttenuation = 1.0f / UnrealAttenuation(MinDistance,Light->Radius),
					MaxAttenuation = 1.0f / UnrealAttenuation(MaxDistance,Light->Radius);

					//!!vogel: fix for D3D debug runtime on XBox
					if( Abs(MinAttenuation - MaxAttenuation) < SMALL_NUMBER )
					{
						LightD3D->Attenuation0 = MinAttenuation;
						LightD3D->Attenuation1 = 0.0f;
					}
					else
					{
						LightD3D->Attenuation0 = Max(0.01f,MinAttenuation - (MaxAttenuation - MinAttenuation) / (MaxDistance - MinDistance) * MinDistance);
						LightD3D->Attenuation1 = Max(0.0f,(MinAttenuation - LightD3D->Attenuation0) / MinDistance);
					}

					LightD3D->Attenuation2 = 0.0f;					
					LightD3D->Range = (256.0f - LightD3D->Attenuation0) / Max(0.01f,LightD3D->Attenuation1);

				}
				else // Point lights.
				{					
					LightD3D->Type			= D3DLIGHT_POINT;					
					// Attempt to approximate Unreal's light attenuation using the provided attenuation factors.
					// This is REALLY limited by D3D forcing positive attenuation factors. :(
					FLOAT		CenterDistance = Max(0.1f,(CurrentState->LitSphere - Light->Position).Size()),
						MaxDistance = Clamp(CenterDistance + CurrentState->LitSphere.W,Light->Radius * 0.05f,Light->Radius * 0.9f),
						MinDistance = Clamp(CenterDistance - CurrentState->LitSphere.W,Light->Radius * 0.1f,Light->Radius * 0.95f),
						MinAttenuation = 1.0f / UnrealAttenuation(MinDistance,Light->Radius),
						MaxAttenuation = 1.0f / UnrealAttenuation(MaxDistance,Light->Radius);

					// sjs - change Max clamping to avoid zero div in network games
					//!!vogel: fix for D3D debug runtime on XBox
					if( Abs(MinAttenuation - MaxAttenuation) < SMALL_NUMBER )
					{
						LightD3D->Attenuation0 = MinAttenuation;
						LightD3D->Attenuation1 = 0.0f;
					}
					else
					{
						LightD3D->Attenuation0 = Max(0.01f,MinAttenuation - (MaxAttenuation - MinAttenuation) / (MaxDistance - MinDistance) * MinDistance);
						LightD3D->Attenuation1 = Max(0.0f,(MinAttenuation - LightD3D->Attenuation0) / MinDistance);
					}

					LightD3D->Attenuation2 = 0.0f;					
					LightD3D->Range = (256.0f - LightD3D->Attenuation0) / Max(0.01f,LightD3D->Attenuation1);
				}

				LightD3D->Diffuse.r = Light->Color.X * Light->Alpha * Scale;
				LightD3D->Diffuse.g = Light->Color.Y * Light->Alpha * Scale;
				LightD3D->Diffuse.b = Light->Color.Z * Light->Alpha * Scale;

                if( Light->Actor && ( Light->Actor->LightEffect == LE_Negative ) ) // sjs, gam
                {
                    LightD3D->Diffuse.r *= -1.0f;
                    LightD3D->Diffuse.g *= -1.0f;
                    LightD3D->Diffuse.b *= -1.0f;
                }

				LightD3D->Diffuse.a = 1.0f;
			}

			LightD3D->Range = Clamp<FLOAT>( LightD3D->Range, 0.0f, MaxD3DRange ); // sjs - xbox freaks out illegal ranges

			LightD3D->Attenuation0 = Max( 0.f, LightD3D->Attenuation0 );
			LightD3D->Attenuation1 = Max( 0.f, LightD3D->Attenuation1 );
			LightD3D->Attenuation2 = Max( 0.f, LightD3D->Attenuation2 );

			RenDev->DeferredState.SetLight(LightIndex,LightD3D);
			RenDev->DeferredState.LightEnable(LightIndex,1);

			CurrentState->LightEnabled[LightIndex] = 1;
		}
		else
		{
			RenDev->DeferredState.LightEnable(LightIndex,0);
			CurrentState->LightEnabled[LightIndex] = 0;
		}
	}

	unguard;
}

//
//	FD3D9RenderInterface::SetGlobalColor
//
void FD3D9RenderInterface::SetGlobalColor(FColor Color)
{
	guard(FD3D9RenderInterface::SetGlobalColor);

	CurrentState->MaterialPasses[CurrentState->NumMaterialPasses-1]->TFactorColor = Color;
	RenDev->DeferredState.SetRenderState( RS_TEXTUREFACTOR, Color );

	unguard;
}


//
//	FD3D9RenderInterface::SetNPatchTesselation
//
void FD3D9RenderInterface::SetNPatchTesselation( FLOAT Tesselation )
{
	guard(FD3D9RenderInterface::SetNPatchTesselation);

	if( (Tesselation >= 1.f) && RenDev->UseNPatches )
		Tesselation = Max(1.f, Tesselation * RenDev->TesselationFactor );
	else
		Tesselation = 1.f;
	CurrentState->NPatchTesselation = Tesselation;
//	RenDev->DeferredState.SetRenderState( RS_PATCHSEGMENTS, *(DWORD*)(&Tesselation) );
// FIXME: npatch tesselation busted
	unguard;
}


//
//	FD3D9RenderInterface::SetDistanceFog
//
void FD3D9RenderInterface::SetDistanceFog(UBOOL Enable,FLOAT FogStart,FLOAT FogEnd,FColor FogColor)
{
	guard(FD3D9RenderInterface::SetDistanceFog);

	if(Enable)
	{
		RenDev->DeferredState.SetRenderState( RS_FOGENABLE, TRUE );
		CurrentState->DistanceFogEnabled = Enable;

		// Set the fog color.
		RenDev->DeferredState.SetRenderState( RS_FOGCOLOR, FogColor );
		CurrentState->DistanceFogColor = FogColor;

		// Workaround for R100 6166 drivers which don't handle fogstart == fogend correctly with vertex fog.
		if( Abs(FogStart - FogEnd) < 1.f )
			FogStart = Max( 0.f, FogEnd - 1.f );

		// Set fog parameters.
		CurrentState->DistanceFogStart	= FogStart;
		CurrentState->DistanceFogEnd	= FogEnd;

		if( !RenDev->UseVertexFog && !(RenDev->DeviceCaps9.RasterCaps & D3DPRASTERCAPS_WFOG) )
		{
			FogStart /= FAR_CLIPPING_PLANE;
			FogEnd /= FAR_CLIPPING_PLANE;
		}

		RenDev->DeferredState.SetRenderState( RS_FOGSTART, *(DWORD*)(&FogStart) );
		RenDev->DeferredState.SetRenderState( RS_FOGEND, *(DWORD*)(&FogEnd) );
	}
	else
	{
		RenDev->DeferredState.SetRenderState( RS_FOGENABLE, FALSE );
		CurrentState->DistanceFogEnabled = Enable;
	}

	unguard;
}

//
//	FD3D9RenderInterface::SetTransform
//
void FD3D9RenderInterface::SetTransform(ETransformType Type,const FMatrix& Matrix)
{
	guard(FD3D9RenderInterface::SetTransform);

	if(Type == TT_LocalToWorld)
	{
		RenDev->DeferredState.SetTransform(TS_WORLD,(D3DMATRIX*) &Matrix);
		CurrentState->LocalToWorld = Matrix;
	}
	else if(Type == TT_WorldToCamera)
	{
		CurrentState->WorldToCamera = Matrix;
		RenDev->DeferredState.SetTransform(TS_VIEW,(D3DMATRIX*) &Matrix);
	}
	else if(Type == TT_CameraToScreen)
	{
		CurrentState->CameraToScreen = Matrix;
		RenDev->DeferredState.SetTransform(TS_PROJECTION,(D3DMATRIX*) &Matrix);
		}

	unguard;
}

//
//	FD3D9RenderInterface::SetZBias
//
void FD3D9RenderInterface::SetZBias(INT ZBias)
{
	guard(FD3D9RenderInterface::SetZBias);

	if( CurrentState->ZBias != ZBias )
	{
		FLOAT RealZBias = -5.f * ZBias / FAR_CLIPPING_PLANE;
		RenDev->DeferredState.SetRenderState( RS_DEPTHBIAS			, *((DWORD*)&RealZBias) );
		RenDev->DeferredState.SetRenderState( RS_SLOPESCALEDEPTHBIAS, *((DWORD*)&RealZBias) );
	}
	CurrentState->ZBias = ZBias;

	unguard;
}

//
//	FD3D9RenderInterface::SetStencilOp
//
void FD3D9RenderInterface::SetStencilOp(ECompareFunction Test,DWORD Ref,DWORD Mask,EStencilOp FailOp,EStencilOp ZFailOp,EStencilOp PassOp,DWORD WriteMask)
{
	guard(FD3D9RenderInterface::SetStencilOp);

	RenDev->DeferredState.SetRenderState( RS_STENCILFUNC, GetD3DCompFunc(Test) );
	CurrentState->StencilTest = Test;

	RenDev->DeferredState.SetRenderState( RS_STENCILREF, Ref & 0xff );
	CurrentState->StencilRef = Ref;

	RenDev->DeferredState.SetRenderState( RS_STENCILMASK, Mask & 0xff );
	CurrentState->StencilMask = Mask;

	RenDev->DeferredState.SetRenderState( RS_STENCILFAIL, GetD3DStencilOp(FailOp) );
	CurrentState->StencilFailOp = FailOp;

	RenDev->DeferredState.SetRenderState( RS_STENCILZFAIL, GetD3DStencilOp(ZFailOp) );
	CurrentState->StencilZFailOp = ZFailOp;

	RenDev->DeferredState.SetRenderState( RS_STENCILPASS, GetD3DStencilOp(PassOp) );
	CurrentState->StencilPassOp = PassOp;

	RenDev->DeferredState.SetRenderState( RS_STENCILWRITEMASK, WriteMask & 0xff );
	CurrentState->StencilWriteMask = WriteMask;

	unguard;
}

//
//  FD3D9RenderInterface::SetPrecacheMode
//
void FD3D9RenderInterface::SetPrecacheMode( EPrecacheMode InPrecacheMode )
{
	PrecacheMode = InPrecacheMode;
}

/*----------------------------------------------------------------------------
	CacheTexture.
----------------------------------------------------------------------------*/

FD3D9Texture* FD3D9RenderInterface::CacheTexture( FBaseTexture* Texture )
{
	guard(FD3D9RenderInterface::CacheTexture);

	// Cache the texture
	QWORD			CacheId = Texture->GetCacheId();
	FD3D9Texture*	D3D9Texture = (FD3D9Texture*) RenDev->GetCachedResource(CacheId);

	if(!D3D9Texture)
		D3D9Texture = new(TEXT("FD3D9Texture")) FD3D9Texture(RenDev,CacheId);

	if(D3D9Texture->CachedRevision != Texture->GetRevision())
		D3D9Texture->Cache(Texture);

	D3D9Texture->LastFrameUsed = RenDev->FrameCounter;
	
	return D3D9Texture;
	unguard;
}

//
//	FD3D9RenderInterface::DescribeArg
//

FString FD3D9RenderInterface::DescribeArg(FD3D9MaterialState* MaterialState,INT StageIndex,DWORD Arg,UBOOL Alpha)
{
	if((Arg & D3DTA_SELECTMASK) == D3DTA_CURRENT)
		return FString::Printf(TEXT("(%s)"),*DescribeStage(MaterialState,StageIndex - 1,Alpha));
	else if((Arg & D3DTA_SELECTMASK) == D3DTA_TEXTURE)
		return FString::Printf(TEXT("texture(%u)"),StageIndex);
	else if((Arg & D3DTA_SELECTMASK) == D3DTA_DIFFUSE)
		return TEXT("diffuse");
	else if((Arg & D3DTA_SELECTMASK) == D3DTA_SPECULAR)
		return TEXT("specular");
	else if((Arg & D3DTA_SELECTMASK) == D3DTA_TFACTOR)
	{
		FColor	C(MaterialState->TFactorColor);
		return FString::Printf(TEXT("tfactor(%u,%u,%u,%u)"),C.R,C.G,C.B,C.A);
	}
	else
		return TEXT("?");
}

//
//	FD3D9RenderInterface::DescribeStage
//

FString FD3D9RenderInterface::DescribeStage(FD3D9MaterialState* MaterialState,INT StageIndex,UBOOL Alpha)
{
	if(StageIndex < 0)
		return TEXT("diffuse");
	else
	{
		FD3D9MaterialStateStage*	Stage = &MaterialState->Stages[StageIndex];
		DWORD					Op = Alpha ? Stage->AlphaOp : Stage->ColorOp,
								Arg1 = Alpha ? Stage->AlphaArg1 : Stage->ColorArg1,
								Arg2 = Alpha ? Stage->AlphaArg2 : Stage->ColorArg2;

		switch(Op)
		{
		case D3DTOP_SELECTARG1:
			return DescribeArg(MaterialState,StageIndex,Arg1,Alpha);
		case D3DTOP_SELECTARG2:
			return DescribeArg(MaterialState,StageIndex,Arg2,Alpha);
		case D3DTOP_MODULATE:
			return FString::Printf(TEXT("%s * %s"),*DescribeArg(MaterialState,StageIndex,Arg1,Alpha),*DescribeArg(MaterialState,StageIndex,Arg2,Alpha));
		case D3DTOP_MODULATE2X:
			return FString::Printf(TEXT("%s * %s * 2"),*DescribeArg(MaterialState,StageIndex,Arg1,Alpha),*DescribeArg(MaterialState,StageIndex,Arg2,Alpha));
		case D3DTOP_MODULATE4X:
			return FString::Printf(TEXT("%s * %s * 4"),*DescribeArg(MaterialState,StageIndex,Arg1,Alpha),*DescribeArg(MaterialState,StageIndex,Arg2,Alpha));
		case D3DTOP_ADD:
			return FString::Printf(TEXT("%s + %s"),*DescribeArg(MaterialState,StageIndex,Arg1,Alpha),*DescribeArg(MaterialState,StageIndex,Arg2,Alpha));
		case D3DTOP_ADDSIGNED:
			return FString::Printf(TEXT("%s + %s - 0.5"),*DescribeArg(MaterialState,StageIndex,Arg1,Alpha),*DescribeArg(MaterialState,StageIndex,Arg2,Alpha));
		case D3DTOP_ADDSIGNED2X:
			return FString::Printf(TEXT("(%s + %s - 0.5) * 2"),*DescribeArg(MaterialState,StageIndex,Arg1,Alpha),*DescribeArg(MaterialState,StageIndex,Arg2,Alpha));
		case D3DTOP_SUBTRACT:
			return FString::Printf(TEXT("%s - %s"),*DescribeArg(MaterialState,StageIndex,Arg1,Alpha),*DescribeArg(MaterialState,StageIndex,Arg2,Alpha));
		case D3DTOP_MODULATEALPHA_ADDCOLOR:
			return FString::Printf(TEXT("%s.rgb + %s.a * %s.rgb"),*DescribeArg(MaterialState,StageIndex,Arg1,0),*DescribeArg(MaterialState,StageIndex,Arg1,1),*DescribeArg(MaterialState,StageIndex,Arg2,0));
		case D3DTOP_BLENDTEXTUREALPHA:
			return FString::Printf(TEXT("%s * texture(%u).a + %s * (1 - texture(%u).a)"),*DescribeArg(MaterialState,StageIndex,Arg1,Alpha),StageIndex,*DescribeArg(MaterialState,StageIndex,Arg2,Alpha),StageIndex);
		case D3DTOP_BLENDCURRENTALPHA:
		{
			FString A = *DescribeStage(MaterialState,StageIndex - 1,1);
			return FString::Printf(TEXT("%s * %s.a + %s * (1 - %s.a)"),*DescribeArg(MaterialState,StageIndex,Arg1,Alpha),*A,*DescribeArg(MaterialState,StageIndex,Arg2,Alpha),*A);
		}
		case D3DTOP_BLENDFACTORALPHA:
			return FString::Printf(TEXT("%s * tfactor.a(%u) + %s * (1 - tfactor.a(%u))"),*DescribeArg(MaterialState,StageIndex,Arg1,Alpha),FColor(MaterialState->TFactorColor).A,*DescribeArg(MaterialState,StageIndex,Arg2,Alpha),FColor(MaterialState->TFactorColor).A);
		default:
			return TEXT("");
		};
	}
}

//
//	FD3D9RenderInterface::SetMaterial.
//
void FD3D9RenderInterface::SetMaterial(UMaterial* InMaterial, FString* ErrorString, UMaterial** ErrorMaterial, INT* NumPasses)
{
	guard(FD3D9RenderInterface::SetMaterial);

	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_SetMaterialCycles));

	// Release old material state.

	for(INT PassIndex = 0;CurrentState->MaterialPasses[PassIndex];PassIndex++)
	{
		if(--CurrentState->MaterialPasses[PassIndex]->NumRefs == 0)
			MaterialStatePool.FreeState(CurrentState->MaterialPasses[PassIndex]);

		CurrentState->MaterialPasses[PassIndex] = NULL;
	}

	// Set default texture if Material is NULL
	if( !InMaterial || (PrecacheMode == PRECACHE_VertexBuffers) )
		InMaterial = Cast<UMaterial>(UMaterial::StaticClass()->GetDefaultObject())->DefaultMaterial;

	// Initialized default state.
	CurrentState->MaterialPasses[0] = MaterialStatePool.AllocateState(&DefaultPass);
	CurrentState->NumMaterialPasses = 1;

	// Check for circular material references
	if( GIsEditor )
	{
		static TArray<UMaterial*> History;
		History.Empty();
		if( !InMaterial->CheckCircularReferences(History) )
		{
			INT ErrorIndex = History.Num()-1;
			if( ErrorIndex >= 0 )
			{
				if( ErrorMaterial ) *ErrorMaterial = History(ErrorIndex);
				if( ErrorMaterial ) *ErrorString   = FString::Printf(TEXT("Circular material reference in %s"), History(ErrorIndex)->GetName() );
			}

			// Set null state
			return;
		}
	}

	if( CurrentState->LightingOnly )
	{
		// Initialized default state.
		//!!vogel: TODO: initializing all 8 stages is not necessarily needed.
		SetLightingOnlyMaterial();
	}
	else
	{
		UBOOL UseFallbacks = !(Viewport->Actor->ShowFlags & SHOW_NoFallbackMaterials);

		// Keep going until we have an renderable material, using fallbacks where necessary.
		for(;;)
		{
		    // Check material type, stripping off and processing final modifiers.
		    FD3D9ModifierInfo	ModifierInfo;
		    UShader*			Shader;
		    UBitmapMaterial*	BitmapMaterial;
		    UConstantMaterial*	ConstantMaterial;
		    UCombiner*			Combiner;
		    UParticleMaterial*	ParticleMaterial;
		    UTerrainMaterial*	TerrainMaterial;
			UProjectorMaterial*	ProjectorMaterial;
		    UMaterial*			NonModifier;
    
			// Notify material it's being set.
			InMaterial->CheckFallback()->PreSetMaterial( Viewport->Actor->Level->TimeSeconds );

		    UBOOL Result = 0;
		    if( (NonModifier=Shader=CheckMaterial<UShader,MT_Shader>(this, InMaterial, &ModifierInfo, UseFallbacks)) != NULL )
		    {
			    Result = SetShaderMaterial( Shader, ModifierInfo, ErrorString, ErrorMaterial );
		    }
		    else
		    if( (NonModifier=Combiner=CheckMaterial<UCombiner,MT_Combiner>(this, InMaterial, &ModifierInfo, UseFallbacks))!=NULL )
		    {
			    Result = SetSimpleMaterial( Combiner, ModifierInfo, ErrorString, ErrorMaterial );
		    }
		    else
		    if( (NonModifier=ConstantMaterial=CheckMaterial<UConstantMaterial,MT_ConstantMaterial>(this, InMaterial, &ModifierInfo, UseFallbacks))!=NULL )
		    {
			    Result = SetSimpleMaterial( ConstantMaterial, ModifierInfo, ErrorString, ErrorMaterial );
		    }
		    else
		    if( (NonModifier=BitmapMaterial=CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, InMaterial, &ModifierInfo, UseFallbacks))!=NULL )
		    {
			    Result = SetSimpleMaterial( BitmapMaterial, ModifierInfo, ErrorString, ErrorMaterial );
		    }
		    else
		    if( (NonModifier=TerrainMaterial=CheckMaterial<UTerrainMaterial,MT_TerrainMaterial>(this, InMaterial, &ModifierInfo, UseFallbacks))!=NULL)
		    {
			    Result = SetTerrainMaterial( TerrainMaterial, ModifierInfo, ErrorString, ErrorMaterial );
		    }
		    else
			if( (NonModifier=ParticleMaterial=CheckMaterial<UParticleMaterial,MT_ParticleMaterial>(this, InMaterial, &ModifierInfo, UseFallbacks))!=NULL)
		    {
			    Result = SetParticleMaterial( ParticleMaterial, ModifierInfo, ErrorString, ErrorMaterial );
		    }
			else
			if( (NonModifier=ProjectorMaterial=CheckMaterial<UProjectorMaterial,MT_ProjectorMaterial>(this, InMaterial, &ModifierInfo, UseFallbacks))!=NULL)
			{
				Result = SetProjectorMaterial( ProjectorMaterial, ModifierInfo, ErrorString, ErrorMaterial );
			}
		    else
			    break;
    
		    // Fall out if we're not interested in fallback materials for this viewport.
		    // eg Texture Browser.
		    if( !UseFallbacks  )
		    {
			    if( !Result )
			    {
				    // Clear any state we got part way through setting.
					for(INT PassIndex = 0;CurrentState->MaterialPasses[PassIndex];PassIndex++)
					{
						MaterialStatePool.FreeState(CurrentState->MaterialPasses[PassIndex]);
						CurrentState->MaterialPasses[PassIndex] = NULL;
					}
					CurrentState->MaterialPasses[0] = MaterialStatePool.AllocateState(&DefaultPass);
				    CurrentState->NumMaterialPasses = 1;
				}
			    break;
		    }
    
		    // Material looks renderable to the SetXxxMaterial code.
		    if( Result )
		    {
#if 1
//#ifdef _XBOX
				break;
#else
			    // fall out if we've previously validated this material.
			    if( InMaterial->GetValidated() )
				    break;
		        else
		        {
				    // Use ValidateDevice to check the material is really renderable.
				    for( INT pass=0;pass<CurrentState->NumMaterialPasses;pass++ )
				    {
						SetMaterialBlending( CurrentState->MaterialPasses[pass], CurrentState->CullMode );
					    RenDev->DeferredState.Commit();
    
					    DWORD	NumPasses;
					    HRESULT hr = RenDev->Direct3DDevice9->ValidateDevice( &NumPasses );
					    if( FAILED(hr) || NumPasses != 1 )
					    {
						    Result = 0;
						    break;
					    }
				    }
				    if( Result )
				    {
					    InMaterial->SetValidated(1);
					    break;
				    }
			    }
#endif
		    }


		    // Material is not renderable.  Find a fallback.
		    if( ModifierInfo.BestFallbackPoint )
		    {
			    // Try using a fallback somewhere in the modifier chain.
			    ModifierInfo.BestFallbackPoint->UseFallback = 1;
		    }
		    else
			if( NonModifier->HasFallback() )
			{
				// Try using the fallback 
				NonModifier->UseFallback = 1;

				if( ErrorMaterial ) *ErrorMaterial = NULL;
				if( ErrorString ) *ErrorString = TEXT("");
			}
		    else
		    {
				//!! TEMP
				debugf( TEXT("%s was not renderable and no fallbacks were available"), InMaterial->GetFullName() );

				// No fallbacks are available, use the default texture.
				InMaterial = Cast<UMaterial>(UMaterial::StaticClass()->GetDefaultObject())->DefaultMaterial;
			}

			// Clear any state we got part way through setting.
			for(INT PassIndex = 0;CurrentState->MaterialPasses[PassIndex];PassIndex++)
			{
				MaterialStatePool.FreeState(CurrentState->MaterialPasses[PassIndex]);
				CurrentState->MaterialPasses[PassIndex] = NULL;
			}
			CurrentState->MaterialPasses[0] = MaterialStatePool.AllocateState(&DefaultPass);
			CurrentState->NumMaterialPasses = 1;
		}
	}

#if 0	// Blending debugging info.
	debugf(TEXT("%s:"),InMaterial->GetFullName());
	for(INT PassIndex = 0;PassIndex < CurrentState->NumMaterialPasses;PassIndex++)
	{
		debugf(TEXT("	Pass %u:"),PassIndex);
		debugf(TEXT("		Color = %s"),*DescribeStage(CurrentState->MaterialPasses[PassIndex],CurrentState->MaterialPasses[PassIndex]->StagesUsed - 1,0));
		debugf(TEXT("		Alpha = %s"),*DescribeStage(CurrentState->MaterialPasses[PassIndex],CurrentState->MaterialPasses[PassIndex]->StagesUsed - 1,1));
	}
#endif

	if( NumPasses )
		*NumPasses = CurrentState->NumMaterialPasses;

	CurrentState->CurrentMaterialState = NULL;

	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_SetMaterialCycles));
	GStats.DWORDStats(RenDev->D3DStats.STATS_SetMaterialCalls)++;

	unguard;
}

//
//	FD3D9RenderInterface::SetVertexStreams
//
INT FD3D9RenderInterface::SetVertexStreams(EVertexShader Shader,FVertexStream** Streams,INT NumStreams)
{
	guard(FD3D9RenderInterface::SetVertexStreams);

	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_SetVertexStreamsCycles));

	// Unset any additional old streams.
	for(INT StreamIndex = NumStreams;StreamIndex < CurrentState->NumStreams;StreamIndex++)
	{
		if(CurrentState->Streams[StreamIndex] != NULL)
		{
			CurrentState->Streams[StreamIndex] = NULL;
			RenDev->DeferredState.SetStreamSource(StreamIndex,NULL,0, 0);
		}
	}

	// Build the shader declarations.
	FShaderDeclaration	ShaderDeclaration;

	ShaderDeclaration.NumStreams = NumStreams;

	// Add the vertex stream components to the shader declaration.
	for(INT StreamIndex = 0;StreamIndex < NumStreams;StreamIndex++)
		ShaderDeclaration.Streams[StreamIndex] = FStreamDeclaration(Streams[StreamIndex]);

	// Find or create an appropriate vertex shader.
	FD3D9VertexShader*	VertexShader = RenDev->GetVertexShader(Shader,ShaderDeclaration);

	// Set the vertex shader.
	CurrentState->VertexShader = VertexShader;
	RenDev->DeferredState.SetVertexShader(VertexShader->Shader);
	RenDev->DeferredState.SetDeclaration(VertexShader->Decl);

	INT Size = 0;

	// Set the vertex streams.
	for(INT StreamIndex = 0;StreamIndex < NumStreams;StreamIndex++)
	{
		// Cache the vertex stream.
		QWORD				CacheId			= Streams[StreamIndex]->GetCacheId();
		FD3D9VertexStream*	D3DVertexStream = (FD3D9VertexStream*) RenDev->GetCachedResource(CacheId);

		if(!D3DVertexStream)
			D3DVertexStream = new(TEXT("FD3D9VertexStream")) FD3D9VertexStream(RenDev,CacheId);

		if(D3DVertexStream->CachedRevision != Streams[StreamIndex]->GetRevision())
		{
			Size += Streams[StreamIndex]->GetSize();
			D3DVertexStream->Cache(Streams[StreamIndex]);
		}

		D3DVertexStream->LastFrameUsed = RenDev->FrameCounter;

		// Set the vertex stream.
		INT	Stride = Streams[StreamIndex]->GetStride();
		INT Offset = 0;//Streams[StreamIndex]->GetOffset();

		RenDev->DeferredState.SetStreamSource(StreamIndex,D3DVertexStream->Direct3DVertexBuffer9, Offset, Stride);
		CurrentState->Streams[StreamIndex]		 = D3DVertexStream;
		CurrentState->StreamStrides[StreamIndex] = Stride;
		CurrentState->StreamOffsets[StreamIndex] = Offset; 
	}

	CurrentState->NumStreams = NumStreams;

	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_SetVertexStreamsCycles));
	GStats.DWORDStats(RenDev->D3DStats.STATS_SetVertexStreamsCalls)++;

	return Size;

	unguard;
}

//
//	FD3D9RenderInterface::SetDynamicStream
//
INT FD3D9RenderInterface::SetDynamicStream(EVertexShader Shader,FVertexStream* Stream)
{
	guard(FD3D9RenderInterface::SetDynamicStream);

	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_SetDynamicStreamCycles));

	// If there isn't a dynamic vertex stream already, allocate one.
	if(!RenDev->DynamicVertexStream)
		RenDev->DynamicVertexStream = new FD3D9DynamicVertexStream(RenDev);

	// Add the vertices in Stream to the dynamic vertex stream.
	INT	BaseVertexIndex = RenDev->DynamicVertexStream->AddVertices(Stream),
		Stride = Stream->GetStride();
	INT Offset = 0;//Stream->GetOffset();

	// Set the dynamic vertex stream.
	RenDev->DeferredState.SetStreamSource(0,RenDev->DynamicVertexStream->Direct3DVertexBuffer9,Offset, Stride);
	CurrentState->Streams[0] = RenDev->DynamicVertexStream;
	CurrentState->StreamStrides[0] = Stride;
	CurrentState->StreamOffsets[0] = Offset;

	// Unset any additional old streams.
	for(INT StreamIndex = 1;StreamIndex < CurrentState->NumStreams;StreamIndex++)
	{
		CurrentState->Streams[StreamIndex] = NULL;
		RenDev->DeferredState.SetStreamSource(StreamIndex,NULL,0,0);
	}

	CurrentState->NumStreams = 1;

	// Find or create an appropriate vertex shader.
	FShaderDeclaration	ShaderDeclaration;

	ShaderDeclaration.NumStreams = 1;
	ShaderDeclaration.Streams[0] = FStreamDeclaration(Stream);

	// Find or create an appropriate vertex shader.
	FD3D9VertexShader*	VertexShader = RenDev->GetVertexShader(Shader,ShaderDeclaration);

	CurrentState->VertexShader = VertexShader;
	RenDev->DeferredState.SetVertexShader(VertexShader->Shader);
	RenDev->DeferredState.SetDeclaration(VertexShader->Decl);

	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_SetDynamicStreamCycles));
	GStats.DWORDStats(RenDev->D3DStats.STATS_SetDynamicStreamCalls)++;
	GStats.DWORDStats(RenDev->D3DStats.STATS_DynamicVertexBytes) += Stream->GetSize();

	return BaseVertexIndex;

	unguard;
}

//
//	FD3D9RenderInterface::SetIndexBuffer
//
INT FD3D9RenderInterface::SetIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseVertexIndex)
{
	guard(FD3D9RenderInterface::SetIndexBuffer);

	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_SetIndexBufferCycles));

	UBOOL RequiresCaching = 0;
	if(IndexBuffer)
	{
		// Cache the index buffer.
		QWORD				CacheId = IndexBuffer->GetCacheId();
		FD3D9IndexBuffer*	D3DIndexBuffer = (FD3D9IndexBuffer*) RenDev->GetCachedResource(CacheId);

		if(!D3DIndexBuffer)
			D3DIndexBuffer = new FD3D9IndexBuffer(RenDev,CacheId);

		if(D3DIndexBuffer->CachedRevision != IndexBuffer->GetRevision())
		{
			D3DIndexBuffer->Cache(IndexBuffer);
			RequiresCaching |= 1;
		}

        checkSlow(IndexBuffer->GetSize()==D3DIndexBuffer->CachedSize); // sjs

		D3DIndexBuffer->LastFrameUsed = RenDev->FrameCounter;

		// Set the index buffer.
		CurrentState->IndexBuffer = D3DIndexBuffer;
		CurrentState->IndexBufferBase = BaseVertexIndex;
		RenDev->DeferredState.SetIndices(D3DIndexBuffer->Direct3DIndexBuffer9);
	}
	else
	{
		// Clear the index buffer.
		if(CurrentState->IndexBuffer != NULL)
		{
			CurrentState->IndexBuffer = NULL;
			CurrentState->IndexBufferBase = 0;
			RenDev->DeferredState.SetIndices(NULL);
		}
	}

	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_SetIndexBufferCycles));
	GStats.DWORDStats(RenDev->D3DStats.STATS_SetIndexBufferCalls)++;

	return RequiresCaching ? IndexBuffer->GetSize() : 0;

	unguard;
}

//
//	FD3D9RenderInterface::SetDynamicIndexBuffer
//
INT FD3D9RenderInterface::SetDynamicIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseVertexIndex)
{
	guard(FD3D9RenderInterface::SetDynamicIndexBuffer);

	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_SetDynamicIndexBufferCycles));

	// If there isn't a dynamic index buffer already, allocate one.
	FD3D9DynamicIndexBuffer* DynamicIndexBuffer;
	if( IndexBuffer->GetIndexSize() == sizeof(DWORD) )
	{
		if(!RenDev->DynamicIndexBuffer32)
			RenDev->DynamicIndexBuffer32 = new FD3D9DynamicIndexBuffer(RenDev,sizeof(DWORD));
		DynamicIndexBuffer = RenDev->DynamicIndexBuffer32;
	}
	else
	{
		if(!RenDev->DynamicIndexBuffer16)
			RenDev->DynamicIndexBuffer16 = new FD3D9DynamicIndexBuffer(RenDev,sizeof(_WORD));
		DynamicIndexBuffer = RenDev->DynamicIndexBuffer16;
	}

	// Add the indices in the index buffer to the dynamic index buffer.
	INT	BaseIndex = DynamicIndexBuffer->AddIndices(IndexBuffer);

	// Set the dynamic index buffer.
	CurrentState->IndexBuffer = DynamicIndexBuffer;
	CurrentState->IndexBufferBase = BaseVertexIndex;
	RenDev->DeferredState.SetIndices(DynamicIndexBuffer->Direct3DIndexBuffer9);

	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_SetDynamicIndexBufferCycles));
	GStats.DWORDStats(RenDev->D3DStats.STATS_SetDynamicIndexBufferCalls)++;
	GStats.DWORDStats(RenDev->D3DStats.STATS_DynamicIndexBytes) += IndexBuffer->GetSize();

	return BaseIndex;

	unguard;
}

//
//	FD3D9RenderInterface::DrawPrimitive
//
void FD3D9RenderInterface::DrawPrimitive(EPrimitiveType PrimitiveType,INT FirstIndex,INT NumPrimitives,INT MinIndex,INT MaxIndex)
{
	guard(FD3D9RenderInterface::DrawPrimitive);

	for( INT pass=0;pass<CurrentState->NumMaterialPasses;pass++ )
	{
		SetMaterialBlending( CurrentState->MaterialPasses[pass], CurrentState->CullMode );

		// Fog hacks needed for translucent objects.
		UBOOL RestoreFogColor = 0;
		if( CurrentState->DistanceFogEnabled && CurrentState->CurrentMaterialState->OverrideFogColor )
		{
			RestoreFogColor = 1;
			RenDev->DeferredState.SetRenderState( RS_FOGCOLOR, (DWORD)CurrentState->CurrentMaterialState->OverriddenFogColor );
		}

		RenDev->DeferredState.Commit();

#define VALIDATE_DEVICE 0
#if VALIDATE_DEVICE
		DWORD	NumPasses;
		HRESULT hr = RenDev->Direct3DDevice9->ValidateDevice( &NumPasses );
		switch( hr )
		{
		case D3DERR_CONFLICTINGTEXTUREFILTER :
			debugf(TEXT("D3DERR_CONFLICTINGTEXTUREFILTER"));
			break;
		case D3DERR_CONFLICTINGTEXTUREPALETTE :
			debugf(TEXT("D3DERR_CONFLICTINGTEXTUREPALETTE"));
			break;
		case D3DERR_TOOMANYOPERATIONS :
			debugf(TEXT("D3DERR_TOOMANYOPERATIONS"));
			break;
		case D3DERR_UNSUPPORTEDALPHAARG :
			debugf(TEXT("D3DERR_UNSUPPORTEDALPHAARG"));
			break;
		case D3DERR_UNSUPPORTEDALPHAOPERATION :
			debugf(TEXT("D3DERR_UNSUPPORTEDALPHAOPERATION"));
			break;
		case D3DERR_UNSUPPORTEDCOLORARG :
			debugf(TEXT("D3DERR_UNSUPPORTEDCOLORARG"));
			break;
		case D3DERR_UNSUPPORTEDCOLOROPERATION :  
			debugf(TEXT("D3DERR_UNSUPPORTEDCOLOROPERATION"));
			break;
		case D3DERR_UNSUPPORTEDFACTORVALUE : 
			debugf(TEXT("D3DERR_UNSUPPORTEDFACTORVALUE"));
			break;
		case D3DERR_UNSUPPORTEDTEXTUREFILTER :
			debugf(TEXT("D3DERR_UNSUPPORTEDTEXTUREFILTER"));
			break;
		case D3DERR_WRONGTEXTUREFORMAT :
			debugf(TEXT("D3DERR_WRONGTEXTUREFORMAT"));
			break;
		default:;
		}
		if( FAILED(hr) )
			debugf(TEXT("INSERT BREAKPOINT HERE"));
#endif

		if( (PrimitiveType != PT_TriangleList) && (PrimitiveType != PT_LineList) )
			check( RenDev->DeviceCaps9.MaxPrimitiveCount > (UINT)NumPrimitives );

		GStats.DWORDStats(RenDev->D3DStats.STATS_NumPrimitives)	+= NumPrimitives;

		try
		{	
			clock(GStats.DWORDStats(RenDev->D3DStats.STATS_DrawPrimitiveCycles));
			if(PrimitiveType == PT_TriangleList)
			{
				while( NumPrimitives )
				{
					UINT RenderedPrimitives = Min<UINT>( RenDev->DeviceCaps9.MaxPrimitiveCount, NumPrimitives );
					if(CurrentState->IndexBuffer)
					{
						RenDev->Direct3DDevice9->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,CurrentState->IndexBufferBase, MinIndex,MaxIndex - MinIndex + 1,FirstIndex,RenderedPrimitives);
						GStats.DWORDStats(RenDev->D3DStats.STATS_NumSVPVertices) += MaxIndex - MinIndex + 1;
					}
					else
					{
						RenDev->Direct3DDevice9->DrawPrimitive(D3DPT_TRIANGLELIST,FirstIndex,RenderedPrimitives);
						GStats.DWORDStats(RenDev->D3DStats.STATS_NumSVPVertices) += RenderedPrimitives * 3;
					}
					NumPrimitives -= RenderedPrimitives;
					FirstIndex += RenderedPrimitives * 3;
				}
			}
			// sjs ---
#ifdef _XBOX
			else if(PrimitiveType == PT_QuadList)
			{
				RenDev->Direct3DDevice9->DrawPrimitive( D3DPT_QUADLIST, FirstIndex, NumPrimitives );
				GStats.DWORDStats(RenDev->D3DStats.STATS_NumSVPVertices) += 4 * NumPrimitives;
			}
#endif
			// --- sjs
			else if(PrimitiveType == PT_TriangleStrip)
			{
				if(CurrentState->IndexBuffer)
				{
					RenDev->Direct3DDevice9->DrawIndexedPrimitive(D3DPT_TRIANGLESTRIP,CurrentState->IndexBufferBase,MinIndex,MaxIndex - MinIndex + 1,FirstIndex,NumPrimitives);
					GStats.DWORDStats(RenDev->D3DStats.STATS_NumSVPVertices) += MaxIndex - MinIndex + 1;
				}
				else
				{
					RenDev->Direct3DDevice9->DrawPrimitive(D3DPT_TRIANGLESTRIP,FirstIndex,NumPrimitives);
					GStats.DWORDStats(RenDev->D3DStats.STATS_NumSVPVertices) += NumPrimitives + 2;
				}
			}
			else if(PrimitiveType == PT_TriangleFan)
			{
				if(CurrentState->IndexBuffer)
				{
					RenDev->Direct3DDevice9->DrawIndexedPrimitive(D3DPT_TRIANGLEFAN,CurrentState->IndexBufferBase,MinIndex,MaxIndex - MinIndex + 1,FirstIndex,NumPrimitives);
					GStats.DWORDStats(RenDev->D3DStats.STATS_NumSVPVertices) += MaxIndex - MinIndex + 1;
				}
				else
				{
					RenDev->Direct3DDevice9->DrawPrimitive(D3DPT_TRIANGLEFAN,FirstIndex,NumPrimitives);
					GStats.DWORDStats(RenDev->D3DStats.STATS_NumSVPVertices) += NumPrimitives + 2;
				}
			}
			else if(PrimitiveType == PT_PointList)
			{
				if(CurrentState->IndexBuffer)
				{
					RenDev->Direct3DDevice9->DrawIndexedPrimitive(D3DPT_POINTLIST,CurrentState->IndexBufferBase,MinIndex,MaxIndex - MinIndex + 1,FirstIndex,NumPrimitives);
					GStats.DWORDStats(RenDev->D3DStats.STATS_NumSVPVertices) += MaxIndex - MinIndex + 1;
				}
				else
				{
					RenDev->Direct3DDevice9->DrawPrimitive(D3DPT_POINTLIST,FirstIndex,NumPrimitives);
					GStats.DWORDStats(RenDev->D3DStats.STATS_NumSVPVertices) += NumPrimitives;
				}
			}
			else if(PrimitiveType == PT_LineList)
			{
				while( NumPrimitives )
				{
					UINT RenderedPrimitives = Min<UINT>( RenDev->DeviceCaps9.MaxPrimitiveCount, NumPrimitives );
					if(CurrentState->IndexBuffer)
					{		
						RenDev->Direct3DDevice9->DrawIndexedPrimitive(D3DPT_LINELIST,CurrentState->IndexBufferBase,MinIndex,MaxIndex - MinIndex + 1,FirstIndex,RenderedPrimitives);
						GStats.DWORDStats(RenDev->D3DStats.STATS_NumSVPVertices) += MaxIndex - MinIndex + 1;
					}
					else
					{
						RenDev->Direct3DDevice9->DrawPrimitive(D3DPT_LINELIST,FirstIndex,RenderedPrimitives);
						GStats.DWORDStats(RenDev->D3DStats.STATS_NumSVPVertices) += 2 * RenderedPrimitives;
					}
					NumPrimitives -= RenderedPrimitives;
					FirstIndex += 2 * RenderedPrimitives;
				}
			}
			unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_DrawPrimitiveCycles));
		}
		catch( ... )
		{
			static UBOOL LogOnce = 0;
			if( LogOnce == 0 )
			{
				LogOnce = 1;
				debugf(TEXT("This could be a mode switch related crash... silently ignoring it is probably less harmful than crashing!"));
			}
		}

		// Restore fog color set above.
		if( RestoreFogColor )
			RenDev->DeferredState.SetRenderState( RS_FOGCOLOR, CurrentState->DistanceFogColor );
	}

	GStats.DWORDStats(RenDev->D3DStats.STATS_DrawPrimitiveCalls)++;

	unguard;
}


//
//	FD3D9RenderInterface::GetD3DStencilOp
//
D3DSTENCILOP FD3D9RenderInterface::GetD3DStencilOp( EStencilOp StencilOp )
{
	switch( StencilOp )
	{
	case SO_Keep:
		return D3DSTENCILOP_KEEP;
	case SO_Zero:
		return D3DSTENCILOP_ZERO;
	case SO_Replace:
		return D3DSTENCILOP_REPLACE;
	case SO_IncrementSat:
		return D3DSTENCILOP_INCRSAT;
	case SO_DecrementSat:
		return D3DSTENCILOP_DECRSAT;
	case SO_Invert:
		return D3DSTENCILOP_INVERT;
	case SO_Increment:
		return D3DSTENCILOP_INCR;
	case SO_Decrement:
		return D3DSTENCILOP_DECR; 
	default:
		appErrorf(TEXT("invalid stencil op"));
		return D3DSTENCILOP_KEEP;
	}
}

//
//	FD3D9RenderInterface::GetD3DCompFunc
//
D3DCMPFUNC FD3D9RenderInterface::GetD3DCompFunc( ECompareFunction CompFunc )
{
	switch( CompFunc )
	{
	case CF_Never:
		return D3DCMP_NEVER;
	case CF_Less:
		return D3DCMP_LESS;
	case CF_Equal:
		return D3DCMP_EQUAL;
	case CF_LessEqual:
		return D3DCMP_LESSEQUAL;
	case CF_Greater:
		return D3DCMP_GREATER;
	case CF_NotEqual:
		return D3DCMP_NOTEQUAL;
	case CF_GreaterEqual:
		return D3DCMP_GREATEREQUAL;
	case CF_Always:
		return D3DCMP_ALWAYS;
	default:
		appErrorf(TEXT("invalid compare function"));
		return D3DCMP_NEVER;
	}
}





















