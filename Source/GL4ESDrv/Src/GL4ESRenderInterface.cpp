/*=============================================================================
	OpenGLRenderInterface.cpp: Unreal OpenGL support.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.
	
	Revision history:
	* Created by Daniel Vogel

=============================================================================*/

#include "GL4ESDrv.h"


//
//	FGL4ESRenderInterface::FGL4ESRenderInterface
//
FGL4ESRenderInterface::FGL4ESRenderInterface(UGL4ESRenderDevice* InRenDev)
{
	guard(FGL4ESRenderInterface::FGL4ESRenderInterface);
	
	RenDev			= InRenDev;
	Viewport		= NULL;
	PrecacheMode	= PRECACHE_All;

	LightsDirty		= 0;
	MatricesDirty	= 0;
	ArraysDirty		= 0;

	SavedStateIndex	= 0;
	CurrentState	= &SavedStates[SavedStateIndex];

	unguard;
}


//
//	FGL4ESRenderInterface::PushState
//
void FGL4ESRenderInterface::PushState()
{
	guard(FGL4ESRenderInterface::PushState);
	
	// Push matrices and assorted state.
	check(SavedStateIndex+1 < GL4ES_MAX_STATESTACKDEPTH);

	CurrentState = &SavedStates[SavedStateIndex+1];
	appMemcpy( CurrentState, &SavedStates[SavedStateIndex], sizeof(FGL4ESSavedState) );
	SavedStateIndex++;

	for(INT PassIndex = 0;PassIndex < CurrentState->NumMaterialPasses;PassIndex++)
		CurrentState->MaterialPasses[PassIndex]->NumRefs++;

	unguard;
}


//
//	FGL4ESRenderInterface::PopState
//
void FGL4ESRenderInterface::PopState()
{
	guard(FGL4ESRenderInterface::PopState);

	// Pop a saved state off the saved state stack.
	if( SavedStateIndex == 0 )
		appErrorf(TEXT("PopState stack underflow"));

	FGL4ESSavedState&	OldState	= SavedStates[SavedStateIndex--];
	CurrentState					= &SavedStates[SavedStateIndex];

	// Apply the restored state.
	if (RenDev->SUPPORTS_GL_EXT_framebuffer_object)
	{
		if( OldState.RenderTargetSurface != CurrentState->RenderTargetSurface ||
			OldState.DepthStencilSurface != CurrentState->DepthStencilSurface)
		{
			RenDev->glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, CurrentState->RenderTargetSurface);
            if (GHackNoDepthTestRenderTarget)
                RenDev->glEnable(GL_DEPTH_TEST);
            GHackNoDepthTestRenderTarget = 0;
		}
	}

	// Set popped state.
	SetTransform( TT_LocalToWorld  , CurrentState->LocalToWorld   );
	SetTransform( TT_WorldToCamera , CurrentState->WorldToCamera  );
	SetTransform( TT_CameraToScreen, CurrentState->CameraToScreen );

	// Set changed state.
	if( OldState.ZBias != CurrentState->ZBias )
	{
		if( RenDev->IsR100 && CurrentState->ZBias )
			RenDev->glPolygonOffset( -1 * CurrentState->ZBias, -255.f );
		else
			RenDev->glPolygonOffset( -1 * CurrentState->ZBias, -1 * CurrentState->ZBias );
	}

	if( OldState.CullMode != CurrentState->CullMode )
		SetCullMode( CurrentState->CullMode );

	if(
		OldState.ViewportX != CurrentState->ViewportX
	||  OldState.ViewportY != CurrentState->ViewportY
	||  OldState.ViewportWidth  != CurrentState->ViewportWidth
	||  OldState.ViewportHeight != CurrentState->ViewportHeight
	)
		SetViewport( CurrentState->ViewportX, CurrentState->ViewportY, CurrentState->ViewportWidth, CurrentState->ViewportHeight );

	if( 		
		OldState.DistanceFogColor	!= CurrentState->DistanceFogColor
	||	OldState.DistanceFogEnabled != CurrentState->DistanceFogEnabled
	||	OldState.DistanceFogStart	!= CurrentState->DistanceFogStart
	||	OldState.DistanceFogEnd		!= CurrentState->DistanceFogEnd
	)
		SetDistanceFog( CurrentState->DistanceFogEnabled, CurrentState->DistanceFogStart, CurrentState->DistanceFogEnd, CurrentState->DistanceFogColor );

	if( OldState.AlphaRef != CurrentState->AlphaRef )
		RenDev->glAlphaFunc( GL_GREATER, CurrentState->AlphaRef / 255.f );
	
	if( OldState.AlphaTest != CurrentState->AlphaTest )
	{
		if( CurrentState->AlphaTest )
			RenDev->glEnable( GL_ALPHA_TEST );
		else
			RenDev->glDisable( GL_ALPHA_TEST );
	}

	if( OldState.ZTest != CurrentState->ZTest )
		RenDev->glDepthFunc( CurrentState->ZTest ? GL_LEQUAL : GL_ALWAYS );

	if( OldState.ZWrite != CurrentState->ZWrite )
		RenDev->glDepthMask( CurrentState->ZWrite ? GL_TRUE : GL_FALSE );

	// Restore material state.
	for(INT PassIndex = 0;OldState.MaterialPasses[PassIndex];PassIndex++)
	{
		if(--OldState.MaterialPasses[PassIndex]->NumRefs == 0)
			MaterialStatePool.FreeState(OldState.MaterialPasses[PassIndex]);
		OldState.MaterialPasses[PassIndex] = NULL;
	}

	CurrentState->CurrentMaterialState	= NULL;
	LightsDirty							= 1;

	unguard;
}


//
//	FGL4ESRenderInterface::SetRenderTarget
//
UBOOL FGL4ESRenderInterface::SetRenderTarget(FRenderTarget* RenderTarget)
{
	guard(FGL4ESRenderInterface::SetRenderTarget);

	// If we don't have EXT_framebuffer_object, we'll "succeed" and just not render.
	if (!RenDev->SUPPORTS_GL_EXT_framebuffer_object)
	{
		CurrentState->OtherRenderTarget = 1;
		return 1;
	}

	QWORD			CacheId = RenderTarget->GetCacheId();
	FGL4ESTexture*	OpenGLTexture = (FGL4ESTexture*) RenDev->GetCachedResource(CacheId);

	if(!OpenGLTexture)
		OpenGLTexture = new(TEXT("FGL4ESTexture")) FGL4ESTexture(RenDev,CacheId);

	if(OpenGLTexture->CachedRevision != RenderTarget->GetRevision())
		OpenGLTexture->Cache(RenderTarget);

	if( !OpenGLTexture->RenderTargetSurface )
		return 0;

	if( !OpenGLTexture->DepthStencilSurface )
		return 0;

	if(   CurrentState->RenderTargetSurface != OpenGLTexture->RenderTargetSurface
	     || CurrentState->DepthStencilSurface != OpenGLTexture->DepthStencilSurface
	)
	{
		RenDev->glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, OpenGLTexture->RenderTargetSurface);
		CurrentState->OtherRenderTarget = 1;
		CurrentState->RenderTargetSurface = OpenGLTexture->RenderTargetSurface;
		CurrentState->DepthStencilSurface = OpenGLTexture->DepthStencilSurface;
		CurrentState->ViewportX = 0;
		CurrentState->ViewportY = 0;
		CurrentState->ViewportWidth = OpenGLTexture->CachedWidth;
		CurrentState->ViewportHeight = OpenGLTexture->CachedHeight;
        if (GHackNoDepthTestRenderTarget)
            RenDev->glDisable(GL_DEPTH_TEST);
		RenDev->glViewport( 0, 0, OpenGLTexture->CachedWidth, OpenGLTexture->CachedHeight );
		RenDev->glScissor( 0, 0, OpenGLTexture->CachedWidth, OpenGLTexture->CachedHeight );
	}

	return 1;

	unguard;
}


//
//	FGL4ESRenderInterface::SetViewport
//
void FGL4ESRenderInterface::SetViewport(INT X,INT Y,INT Width,INT Height)
{
	guard(FGL4ESRenderInterface::SetViewport);

	// if zero, we failed to create a render target...
	if ((CurrentState->OtherRenderTarget) && (CurrentState->RenderTargetSurface == 0))
		return;

	if( RenDev->LockedViewport )
	{
		RenDev->glViewport( X, RenDev->LockedViewport->SizeY - Y - Height, Width, Height );
		RenDev->glScissor( X, RenDev->LockedViewport->SizeY - Y - Height, Width, Height );
	}
	else
	{
		RenDev->glViewport( X, Y, Width, Height );
		RenDev->glScissor( X, Y, Width, Height );
	}

	CurrentState->ViewportX = X;
	CurrentState->ViewportY = Y;
	CurrentState->ViewportWidth	= Width;
	CurrentState->ViewportHeight = Height;

	unguard;
}


//
//	FGL4ESRenderInterface::Clear
//
void FGL4ESRenderInterface::Clear(UBOOL UseColor,FColor Color,UBOOL UseDepth,FLOAT Depth,UBOOL UseStencil,DWORD Stencil)
{
	guard(FGL4ESRenderInterface::Clear);

	// if zero, we failed to create a render target...
	if ((CurrentState->OtherRenderTarget) && (CurrentState->RenderTargetSurface == 0))
		return;

	DWORD ClearFlags = 0;

	if( UseColor )
	{
		RenDev->glClearColor( Color.R / 255.f, Color.G / 255.f, Color.B / 255.f, Color.A / 255.f );
		ClearFlags |= GL_COLOR_BUFFER_BIT;
	}

	if( UseDepth )
	{
		RenDev->glClearDepth( Depth );
		ClearFlags |= GL_DEPTH_BUFFER_BIT;
	}

	if( UseStencil && (RenDev->UseStencil || GIsEditor) )
	{
		RenDev->glClearStencil( Stencil );
		ClearFlags |= GL_STENCIL_BUFFER_BIT;
	}

	RenDev->glClear( ClearFlags );

	unguard;
}


//
//	FGL4ESRenderInterface::PushHit
//
void FGL4ESRenderInterface::PushHit(const BYTE* Data,INT Count)
{
	guard(FGL4ESRenderInterface::PushHit);
	unguard;
}


//
//	FGL4ESRenderInterface::PopHit
//
void FGL4ESRenderInterface::PopHit(INT Count,UBOOL Force)
{
	guard(FGL4ESRenderInterface::PopHit);
	unguard;
}


//
//	FGL4ESRenderInterface::SetCullMode
//
void FGL4ESRenderInterface::SetCullMode(ECullMode CullMode)
{
	guard(FGL4ESRenderInterface::SetCullMode);

	CurrentState->CullMode = CullMode;
	switch( CullMode )
	{
	case CM_CW:
		RenDev->glEnable( GL_CULL_FACE );
		RenDev->glCullFace( GL_BACK );
		break;
	case CM_CCW:
		RenDev->glEnable( GL_CULL_FACE );
		RenDev->glCullFace( GL_FRONT );
		break;
	case CM_None:
		RenDev->glDisable( GL_CULL_FACE );
		break;
	}

	unguard;
}


//
//	FGL4ESRenderInterface::SetAmbientLight
//
void FGL4ESRenderInterface::SetAmbientLight(FColor Color)
{
	guard(FGL4ESRenderInterface::SetAmbientLight);
	CurrentState->AmbientLightColor = Color;
	LightsDirty						= 1;
	unguard;
}


//
//	FGL4ESRenderInterface::EnableLighting
//
void FGL4ESRenderInterface::EnableLighting(UBOOL UseDynamic, UBOOL UseStatic, UBOOL Modulate2X, FBaseTexture* LightmapTexture, UBOOL LightingOnly, FSphere LitSphere )
{
	guard(FGL4ESRenderInterface::EnableLighting);
	
	FGL4ESTexture* Lightmap = NULL;
	if( LightmapTexture )
		Lightmap = CacheTexture( LightmapTexture );

	CurrentState->Lightmap				= Lightmap;		
	CurrentState->LightingOnly			= LightingOnly;

	CurrentState->UseDynamicLighting	= UseDynamic;
	CurrentState->UseStaticLighting		= UseStatic;

	CurrentState->LightingModulate2X	= Modulate2X;
	CurrentState->LitSphere				= LitSphere;

	LightsDirty							= 1;

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
//	FGL4ESRenderInterface::SetLight
//
void FGL4ESRenderInterface::SetLight(INT LightIndex,FDynamicLight* Light,FLOAT Scale)
{
	guard(FGL4ESRenderInterface::SetLight);

	if( LightIndex < 8 )
	{
		if(Light)
		{
			CurrentState->Lights[LightIndex].Enabled		= 1;
			CurrentState->Lights[LightIndex].UseDirection	= 0;

			FPlane Diffuse = FPlane
			(
				Light->Color.X * Light->Alpha * Scale,
				Light->Color.Y * Light->Alpha * Scale,
				Light->Color.Z * Light->Alpha * Scale,
				1.0f
			);

			if(Light->Actor && Light->Actor->LightEffect == LE_Sunlight) // gam
			{
				//!!FUDGE FACTOR
				Diffuse.X *= 2.25f; Diffuse.Y *= 2.25f; Diffuse.Z *= 2.25f;

				CurrentState->Lights[LightIndex].Position = -Light->Direction;

				CurrentState->Lights[LightIndex].SpotCutoff				= 180.f;
				CurrentState->Lights[LightIndex].SpotExponent			= 0.f;

				CurrentState->Lights[LightIndex].ConstantAttenuation	= 1.0f;
				CurrentState->Lights[LightIndex].LinearAttenuation		= 0.0f;
				CurrentState->Lights[LightIndex].QuadraticAttenuation	= 0.0f;

				CurrentState->Lights[LightIndex].Diffuse				= Diffuse;
			}
			else if(Light->Actor && Light->Actor->LightEffect == LE_QuadraticNonIncidence || CurrentState->LitSphere.W == -1.0f ) // sjs
			{
				CurrentState->Lights[LightIndex].Position				= Light->Position;
				CurrentState->Lights[LightIndex].Position.W				= 1.0f;

				CurrentState->Lights[LightIndex].SpotCutoff				= 180.f;
				CurrentState->Lights[LightIndex].SpotExponent			= 0.f;

				CurrentState->Lights[LightIndex].ConstantAttenuation	= 0.0f;
				CurrentState->Lights[LightIndex].LinearAttenuation		= 0.0f;
				CurrentState->Lights[LightIndex].QuadraticAttenuation	= 8.0f / Square(Light->Radius);

				CurrentState->Lights[LightIndex].Diffuse				= Diffuse;
			}
			else
			{
				//!!FUDGE FACTOR
				Diffuse.X *= 1.25f; Diffuse.Y *= 1.25f; Diffuse.Z *= 1.25f;

				CurrentState->Lights[LightIndex].Position				= Light->Position;
				CurrentState->Lights[LightIndex].Position.W				= 1.0f;
		
				if(Light->Actor && ( Light->Actor->LightEffect == LE_StaticSpot || Light->Actor->LightEffect == LE_Spotlight) ) // gam
				{
					CurrentState->Lights[LightIndex].UseDirection		= 1;
					CurrentState->Lights[LightIndex].Direction			= -Light->Direction;
			
					CurrentState->Lights[LightIndex].SpotCutoff			= Light->Actor->LightCone / 256.0f * 90.f;
					CurrentState->Lights[LightIndex].SpotExponent		= 2.f;
				}
				else
				{
					CurrentState->Lights[LightIndex].SpotCutoff			= 180.f;
					CurrentState->Lights[LightIndex].SpotExponent		= 0.f;
				}

				// Attempt to approximate Unreal's light attenuation using the provided attenuation factors.
				// This is REALLY limited by OpenGL & D3D forcing positive attenuation factors. :(
				FLOAT		CenterDistance = Max(0.1f,(CurrentState->LitSphere - Light->Position).Size()),
							MaxDistance = Clamp(CenterDistance + CurrentState->LitSphere.W,Light->Radius * 0.05f,Light->Radius * 0.9f),
							MinDistance = Clamp(CenterDistance - CurrentState->LitSphere.W,Light->Radius * 0.1f,Light->Radius * 0.95f),
							MinAttenuation = 1.0f / UnrealAttenuation(MinDistance,Light->Radius),
							MaxAttenuation = 1.0f / UnrealAttenuation(MaxDistance,Light->Radius);

				if( Abs(MinAttenuation - MaxAttenuation) < SMALL_NUMBER )
				{
					CurrentState->Lights[LightIndex].ConstantAttenuation	= MinAttenuation;
					CurrentState->Lights[LightIndex].LinearAttenuation		= 0.0f;
				}
				else
				{
					FLOAT ConstantAttenuation = Max(0.01f,MinAttenuation - (MaxAttenuation - MinAttenuation) / (MaxDistance - MinDistance) * MinDistance);
					CurrentState->Lights[LightIndex].ConstantAttenuation	= ConstantAttenuation;
					CurrentState->Lights[LightIndex].LinearAttenuation		= Max(0.0f,(MinAttenuation - ConstantAttenuation) / MinDistance);
				}

				CurrentState->Lights[LightIndex].QuadraticAttenuation = 0.f;

                if( Light->Actor && ( Light->Actor->LightEffect == LE_Negative ) ) // sjs, gam
                {
                    Diffuse.X *= -1.0f;
                    Diffuse.Y *= -1.0f;
                    Diffuse.Z *= -1.0f;
                }

				CurrentState->Lights[LightIndex].Diffuse = Diffuse;
			}
		}
		else
			CurrentState->Lights[LightIndex].Enabled = 0;
	}

	LightsDirty = 1;

	unguard;
}
 

//
// FGL4ESRenderInterface::SetNPatchTesselation
//
void FGL4ESRenderInterface::SetNPatchTesselation( FLOAT Tesselation )
{
	guard(FGL4ESRenderInterface::SetNPatchTesselation);
	unguard;
}


//
//	FGL4ESRenderInterface::SetDistanceFog
//
void FGL4ESRenderInterface::SetDistanceFog(UBOOL Enable,FLOAT FogStart,FLOAT FogEnd,FColor Color)
{
	guard(FGL4ESRenderInterface::SetDistanceFog);

	// Workaround for certain ATI drivers that don't handle fogstart == fogend correctly.
	if( Abs(FogStart - FogEnd) < 1.f )
		FogStart = Max( 0.f, FogEnd - 1.f );

	if( Enable )
	{
		FPlane FogColor = Color.Plane();

		RenDev->glEnable( GL_FOG );
		RenDev->glFogf(	GL_FOG_START, FogStart );
		RenDev->glFogf( GL_FOG_END, FogEnd );
		RenDev->glFogfv( GL_FOG_COLOR, (GLfloat*) &FogColor );
	}
	else
		RenDev->glDisable( GL_FOG );

	CurrentState->DistanceFogEnabled	= Enable;
	CurrentState->DistanceFogColor		= Color;
	CurrentState->DistanceFogStart		= FogStart;
	CurrentState->DistanceFogEnd		= FogEnd;

	unguard;
}


//
//	FGL4ESRenderInterface::SetGlobalColor
//
void FGL4ESRenderInterface::SetGlobalColor(FColor Color)
{
	guard(FGL4ESRenderInterface::SetGlobalColor);

	CurrentState->MaterialPasses[CurrentState->NumMaterialPasses-1]->TFactorColor = Color;
	
	FPlane TFactorColor = Color.Plane();
	for( INT i=0; i<CurrentState->MaterialPasses[CurrentState->NumMaterialPasses-1]->StagesUsed; i++ )
	{
		RenDev->glActiveTextureARB( GL_TEXTURE0 + i );
		RenDev->glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, (GLfloat*) &TFactorColor );
	}

	unguard;
}


//
//	FGL4ESRenderInterface::SetTransform
//
void FGL4ESRenderInterface::SetTransform(ETransformType Type,const FMatrix& Matrix)
{
	guard(FGL4ESRenderInterface::SetTransform);

	switch( Type )
	{
	case TT_LocalToWorld:
		CurrentState->LocalToWorld	 = Matrix;
		MatricesDirty				|= GL4ES_MATRIX_Modelview;
		break;
	case TT_WorldToCamera:
		CurrentState->WorldToCamera	 = Matrix;
		MatricesDirty				|= GL4ES_MATRIX_Modelview;
		if( CurrentState->UseDynamicLighting )
			LightsDirty				 = 1;
		break;
	case TT_CameraToScreen:
		CurrentState->CameraToScreen = Matrix;
		MatricesDirty				|= GL4ES_MATRIX_Projection;
		break;
	}
	unguard;
}


//
//	FGL4ESRenderInterface::SetTexture
//
void FGL4ESRenderInterface::SetTexture( FGL4ESTexture* Texture )
{
	guard(FGL4ESRenderInterface::SetTexture);

	if( Texture )
	{
		if( Texture->IsCubemap )
			RenDev->glBindTexture( GL_TEXTURE_CUBE_MAP, Texture->TextureID );
		else
			RenDev->glBindTexture( GL_TEXTURE_2D, Texture->TextureID );
	}
	else
	{
		RenDev->glBindTexture( GL_TEXTURE_2D, RenDev->NullTextureID );
	}

	unguard;
}


//
//	FGL4ESRenderInterface::SetMaterialBlending
//

#define COMMIT_TEXTURE_ENV( A, B )								\
	if( RenDev->HardwareState[s].A != Stage.A )				\
	{															\
		RenDev->HardwareState[s].A = Stage.A;				\
		RenDev->glTexEnvi( GL_TEXTURE_ENV, B, Stage.A );	\
	}

void FGL4ESRenderInterface::SetMaterialBlending( FGL4ESMaterialState* NewMaterialState )
{
	guard(FGL4ESRenderInterface::SetMaterialBlending);

	if(CurrentState->CurrentMaterialState == NewMaterialState)
		return;

	if(NewMaterialState->PixelShader != GL4ES_PS_None)
		RenDev->SetPixelShader( RenDev->GetPixelShader(NewMaterialState->PixelShader)->Handle );
	else
		RenDev->SetPixelShader( 0 );

	// Fillmode
	switch(NewMaterialState->FillMode)
	{
	case FM_Wireframe:
	case FM_Solid:
		RenDev->glPolygonMode( GL_FRONT_AND_BACK, FM_Wireframe ? GL_LINE : GL_FILL );
		//!!RenDev->DeferredState.SetTextureStageState( 0, TSS_COLOROP, D3DTOP_MODULATE );
		break;
	case FM_FlatShaded:
		check(0);
		//!!RenDev->DeferredState.SetTextureStageState( 0, TSS_COLOROP, D3DTOP_SELECTARG2 );
		//!!RenDev->DeferredState.SetRenderState( RS_FILLMODE, D3DFILL_SOLID );
		break;
	}

	// Alpha test.
	RenDev->glAlphaFunc( GL_GREATER, NewMaterialState->AlphaRef / 255.f );
	if( NewMaterialState->AlphaTest )
		RenDev->glEnable( GL_ALPHA_TEST );
	else
		RenDev->glDisable( GL_ALPHA_TEST );

	// Depth buffer.
	RenDev->glDepthFunc( NewMaterialState->ZTest ? GL_LEQUAL : GL_ALWAYS );
	RenDev->glDepthMask( NewMaterialState->ZWrite ? GL_TRUE : GL_FALSE );

	// For push/pop state.
	CurrentState->ZWrite	= NewMaterialState->ZWrite;
	CurrentState->ZTest		= NewMaterialState->ZTest;
	CurrentState->AlphaRef	= NewMaterialState->AlphaRef;
	CurrentState->AlphaTest	= NewMaterialState->AlphaTest;

	SetCullMode( NewMaterialState->TwoSided ? CM_None : CurrentState->CullMode );

	// Source/destination blending
	RenDev->glBlendFunc( NewMaterialState->SrcBlend, NewMaterialState->DestBlend );

	// Avoid redundant calls into driver.
	RenDev->glMatrixMode( GL_TEXTURE );
	FPlane TFactorColor	= NewMaterialState->TFactorColor.Plane();

	// Texture stages and blending
	for( INT s=0;s<NewMaterialState->StagesUsed;s++ )
	{
		FGL4ESMaterialStateStage& Stage = NewMaterialState->Stages[s];

		// Enable stage.	
		RenDev->glActiveTextureARB( GL_TEXTURE0 + s );
		RenDev->glEnable( GL_TEXTURE_2D );
		if( Stage.Texture && Stage.Texture->IsCubemap )
			RenDev->glEnable( GL_TEXTURE_CUBE_MAP );	
		else
			RenDev->glDisable( GL_TEXTURE_CUBE_MAP );	


		// Texture factor
		RenDev->glTexEnvfv( GL_TEXTURE_ENV, GL_TEXTURE_ENV_COLOR, (GLfloat*) &TFactorColor );

		SetTexture( Stage.Texture );
		
		// Set texture clamping.
		if( Stage.Texture && (Stage.Texture->IsCubemap || Stage.Texture->WasCubemap) )
		{
			RenDev->glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
			RenDev->glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
			RenDev->glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE );
		}
		else
		{
			RenDev->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, Stage.TextureAddressU );
			RenDev->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, Stage.TextureAddressV );
			RenDev->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_R, Stage.TextureAddressW );
		}

		// Part of OpenGL 1.4 but we only require 1.3.
		if( RenDev->SUPPORTS_GL_EXT_texture_lod_bias && RenDev->HardwareState[s].TextureMipLODBias != Stage.TextureMipLODBias )
		{
       		RenDev->glTexEnvf( GL_TEXTURE_FILTER_CONTROL_EXT, GL_TEXTURE_LOD_BIAS_EXT, Stage.TextureMipLODBias );
			RenDev->HardwareState[s].TextureMipLODBias = Stage.TextureMipLODBias;
		}

		// Combine4 blending.
		if( RenDev->SUPPORTS_GL_NV_texture_env_combine4 && !RenDev->SUPPORTS_GL_ATI_texture_env_combine3 )
		{
			if( Stage.UseNVCombine4 )
			{			
				if( !RenDev->HardwareState[s].UseNVCombine4 )
				{
					RenDev->glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE4_NV );
					RenDev->HardwareState[s].UseNVCombine4 = 1;
				}
			}
			else if( RenDev->HardwareState[s].UseNVCombine4 )
			{
				RenDev->glTexEnvi( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
				RenDev->HardwareState[s].UseNVCombine4 = 0;
			}
		}

		// Set blend stage state.
		COMMIT_TEXTURE_ENV( ColorOp		, GL_COMBINE_RGB	);
		COMMIT_TEXTURE_ENV( AlphaOp		, GL_COMBINE_ALPHA	);
		COMMIT_TEXTURE_ENV( ColorScale	, GL_RGB_SCALE		);
		COMMIT_TEXTURE_ENV( AlphaScale	, GL_ALPHA_SCALE	);
		COMMIT_TEXTURE_ENV( ColorArg1	, GL_SOURCE0_RGB	);
		COMMIT_TEXTURE_ENV( ColorArg2	, GL_SOURCE1_RGB	);
		COMMIT_TEXTURE_ENV( ColorArg0	, GL_SOURCE2_RGB	);
		COMMIT_TEXTURE_ENV( AlphaArg1	, GL_SOURCE0_ALPHA	);
		COMMIT_TEXTURE_ENV( AlphaArg2	, GL_SOURCE1_ALPHA	);
		COMMIT_TEXTURE_ENV( AlphaArg0	, GL_SOURCE2_ALPHA	);
		COMMIT_TEXTURE_ENV( ColorMod1	, GL_OPERAND0_RGB	);
		COMMIT_TEXTURE_ENV( ColorMod2	, GL_OPERAND1_RGB	);
		COMMIT_TEXTURE_ENV( ColorMod0	, GL_OPERAND2_RGB	);
		COMMIT_TEXTURE_ENV( AlphaMod1	, GL_OPERAND0_ALPHA	);
		COMMIT_TEXTURE_ENV( AlphaMod2	, GL_OPERAND1_ALPHA	);
		COMMIT_TEXTURE_ENV( AlphaMod0	, GL_OPERAND2_ALPHA	);

		// Figure out how many coords to automatically generate.
		INT TexCoordCount;
		switch( Stage.TexCoordCount )
		{
		case TCN_2DCoords:
			TexCoordCount = 2;
			break;
		case TCN_3DCoords:
			TexCoordCount = 3;
			break;
		case TCN_4DCoords:
			TexCoordCount = 4;
		default:
			TexCoordCount = 0;
		}

		// Handle texgen.
		FLOAT		TexFPlaneFloat[4];
		FPlane&		TexFPlane = *((FPlane*) &TexFPlaneFloat);
		UBOOL		Transpose = 0;

		switch( Stage.TexGenMode )
		{
		case TCS_WorldCoords:
			{
				for( INT i=0; i<TexCoordCount; i++ )
				{
					appMemzero( TexFPlaneFloat, sizeof(TexFPlaneFloat) );
					TexFPlaneFloat[i] = 1.0f;
					
					CurrentState->LocalToWorld.TransformFPlane( TexFPlane );

					RenDev->glTexGeni( GL_S + i, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
					RenDev->glTexGenfv( GL_S + i, GL_OBJECT_PLANE, (GLfloat*) &TexFPlane );
					RenDev->glEnable( GL_TEXTURE_GEN_S + i);
				}

				Stage.TextureTransformsEnabled = Stage.UseTexGenMatrix;

				break;
			}
		case TCS_WorldEnvMapCoords:
			{
				for( INT i=0; i<TexCoordCount; i++ )
				{
					RenDev->glTexGeni( GL_S + i, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP );
					RenDev->glEnable( GL_TEXTURE_GEN_S + i);
				}

				if( Stage.UseTexGenMatrix )
					Stage.TextureTransformMatrix = CurrentState->WorldToCamera * Stage.TextureTransformMatrix;
				else
					Stage.TextureTransformMatrix = CurrentState->WorldToCamera;

				Transpose						= 1;
				Stage.TextureTransformsEnabled	= 1;

				break;
			}
		case TCS_CameraCoords:
			{
				FMatrix LocalToCamera = CurrentState->LocalToWorld * CurrentState->WorldToCamera;
		
				for( INT i=0; i<TexCoordCount; i++ )
				{
					appMemzero( TexFPlaneFloat, sizeof(TexFPlaneFloat) );
					TexFPlaneFloat[i] = 1.0f;

					LocalToCamera.TransformFPlane( TexFPlane );
			
					RenDev->glTexGeni( GL_S + i, GL_TEXTURE_GEN_MODE, GL_OBJECT_LINEAR );
					RenDev->glTexGenfv( GL_S + i, GL_OBJECT_PLANE, (GLfloat*) &TexFPlane );	
					RenDev->glEnable( GL_TEXTURE_GEN_S + i);
				}

				Stage.TextureTransformsEnabled = Stage.UseTexGenMatrix;

				break;
			}
		case TCS_CameraEnvMapCoords:			
			{
				for( INT i=0; i<TexCoordCount; i++ )
				{
					RenDev->glTexGeni( GL_S + i, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP );
					RenDev->glEnable( GL_TEXTURE_GEN_S + i);
				}

				Stage.TextureTransformsEnabled = Stage.UseTexGenMatrix;

				break;
			}
		case TCS_ProjectorCoords:
			{
				for( INT i=0; i<TexCoordCount; i++ )
				{
					RenDev->glTexGeni( GL_S + i, GL_TEXTURE_GEN_MODE, GL_NORMAL_MAP );
					RenDev->glEnable( GL_TEXTURE_GEN_S + i);
				}

				Stage.TextureTransformsEnabled = Stage.UseTexGenMatrix;

				break;
			}
		case TCS_NoChange:
		default:
			TexCoordCount	= 0;
			break;
		}

		// Disable unwanted texgen.		
		for( INT i=TexCoordCount; i<4; i++ )
			RenDev->glDisable( GL_TEXTURE_GEN_S + i );
		
		// Convert D3D matrix to OpenGL and also handle 'PROJECTED'.
    	if( Stage.TexGenProjected || Stage.TextureTransformsEnabled )
		{
			if( !Stage.TextureTransformsEnabled )
			{
				Stage.TextureTransformMatrix	= FMatrix::Identity;
				Stage.TextureTransformsEnabled	= 1;
			}

			if( Stage.TexGenProjected )
			{
				Stage.TextureTransformMatrix.M[0][3] = Stage.TextureTransformMatrix.M[0][2]; 
				Stage.TextureTransformMatrix.M[1][3] = Stage.TextureTransformMatrix.M[1][2];
				Stage.TextureTransformMatrix.M[2][3] = Stage.TextureTransformMatrix.M[2][2];
				Stage.TextureTransformMatrix.M[3][3] = Stage.TextureTransformMatrix.M[3][2];
			}	
			else
			{
				Stage.TextureTransformMatrix.M[3][0] = Stage.TextureTransformMatrix.M[2][0]; 
				Stage.TextureTransformMatrix.M[3][1] = Stage.TextureTransformMatrix.M[2][1];
			}

			if( Transpose )
				Stage.TextureTransformMatrix = Stage.TextureTransformMatrix.Transpose();
		}

		// Set texture transform matrix.
		if( Stage.TextureTransformsEnabled )	
			RenDev->glLoadMatrixf( (GLfloat*) &Stage.TextureTransformMatrix );
		else
			RenDev->glLoadIdentity();
	}

	// Disable unused stages.
	for( INT s=NewMaterialState->StagesUsed; s<RenDev->NumTextureUnits; s++ )
	{
		RenDev->glActiveTextureARB( GL_TEXTURE0 + s );
		RenDev->glDisable( GL_TEXTURE_2D );
		RenDev->glDisable( GL_TEXTURE_CUBE_MAP );
	}

	CurrentState->CurrentMaterialState = NewMaterialState;

	unguard;
}


//
//	FGL4ESRenderInterface::SetMaterial
//
void FGL4ESRenderInterface::SetMaterial(UMaterial* InMaterial, FString* ErrorString, UMaterial** ErrorMaterial, INT* NumPasses)
{
	guard(FGL4ESRenderInterface::SetMaterial);

	// if zero, we failed to create a render target...
	if ((CurrentState->OtherRenderTarget) && (CurrentState->RenderTargetSurface == 0))
		return;

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
		SetLightingOnlyMaterial();
	}
	else
	{
		UBOOL UseFallbacks = !(Viewport->Actor->ShowFlags & SHOW_NoFallbackMaterials);

		// Keep going until we have an renderable material, using fallbacks where necessary.
		for(;;)
		{
		    // Check material type, stripping off and processing final modifiers.
		    FGL4ESModifierInfo	ModifierInfo;
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
		    if( (NonModifier=Shader=GL4ESCheckMaterial<UShader,MT_Shader>(this, InMaterial, &ModifierInfo, UseFallbacks)) != NULL )
		    {
			    Result = SetShaderMaterial( Shader, ModifierInfo, ErrorString, ErrorMaterial );
		    }
		    else
		    if( (NonModifier=Combiner=GL4ESCheckMaterial<UCombiner,MT_Combiner>(this, InMaterial, &ModifierInfo, UseFallbacks))!=NULL )
		    {
			    Result = SetSimpleMaterial( Combiner, ModifierInfo, ErrorString, ErrorMaterial );
		    }
		    else
		    if( (NonModifier=ConstantMaterial=GL4ESCheckMaterial<UConstantMaterial,MT_ConstantMaterial>(this, InMaterial, &ModifierInfo, UseFallbacks))!=NULL )
		    {
			    Result = SetSimpleMaterial( ConstantMaterial, ModifierInfo, ErrorString, ErrorMaterial );
		    }
		    else
		    if( (NonModifier=BitmapMaterial=GL4ESCheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, InMaterial, &ModifierInfo, UseFallbacks))!=NULL )
		    {
			    Result = SetSimpleMaterial( BitmapMaterial, ModifierInfo, ErrorString, ErrorMaterial );
		    }
		    else
		    if( (NonModifier=TerrainMaterial=GL4ESCheckMaterial<UTerrainMaterial,MT_TerrainMaterial>(this, InMaterial, &ModifierInfo, UseFallbacks))!=NULL)
		    {
			    Result = SetTerrainMaterial( TerrainMaterial, ModifierInfo, ErrorString, ErrorMaterial );
		    }
		    else
			if( (NonModifier=ParticleMaterial=GL4ESCheckMaterial<UParticleMaterial,MT_ParticleMaterial>(this, InMaterial, &ModifierInfo, UseFallbacks))!=NULL)
		    {
			    Result = SetParticleMaterial( ParticleMaterial, ModifierInfo, ErrorString, ErrorMaterial );
		    }
			else
			if( (NonModifier=ProjectorMaterial=GL4ESCheckMaterial<UProjectorMaterial,MT_ProjectorMaterial>(this, InMaterial, &ModifierInfo, UseFallbacks))!=NULL)
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
				break;

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

				if( ErrorMaterial ) 
					*ErrorMaterial = NULL;
				if( ErrorString ) 
					*ErrorString = TEXT("");
		    }
		    else
		    {
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

	if( CurrentState->MaterialPasses[0]->StagesUsed > RenDev->NumTextureUnits )
	{
		debugf(TEXT("Internal error decoding %s"), InMaterial->GetPathName() );
		appErrorf(TEXT("Internal error decoding %s"), InMaterial->GetPathName() );
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

	CurrentState->CurrentMaterialState	= NULL;
	ArraysDirty							= 1;

	unguard;
}


//
//	FGL4ESRenderInterface::SetZBias
//
void FGL4ESRenderInterface::SetZBias(INT ZBias)
{
	guard(FGL4ESRenderInterface::SetZBias);
	if( RenDev->IsR100 && ZBias )
		RenDev->glPolygonOffset( -1 * ZBias, -255.f );
	else
		RenDev->glPolygonOffset( -1 * ZBias, -1 * ZBias );
	CurrentState->ZBias = ZBias;
	unguard;
}


//
//	FGL4ESRenderInterface::SetStencilOp
//
void FGL4ESRenderInterface::SetStencilOp(ECompareFunction Test,DWORD Ref,DWORD Mask,EStencilOp FailOp,EStencilOp ZFailOp,EStencilOp PassOp,DWORD WriteMask)
{
	guard(FGL4ESRenderInterface::SetStencilOp);
	unguard;
}


//
//	FGL4ESRenderInterface::SetPrecacheMode
//
void FGL4ESRenderInterface::SetPrecacheMode( EPrecacheMode InPrecacheMode )
{
	guard(FGL4ESRenderInterface::SetPrecacheMode);
	PrecacheMode = InPrecacheMode;
	unguard;
}
	

//
//	FGL4ESRenderInterface::SetVertexStreams
//
INT FGL4ESRenderInterface::SetVertexStreams(EVertexShader Shader,FVertexStream** Streams,INT NumStreams)
{
	guard(FGL4ESRenderInterface::SetVertexStreams);
	
	// Unset any additional old streams.
	for(INT StreamIndex = NumStreams;StreamIndex < CurrentState->NumStreams;StreamIndex++)
		CurrentState->Streams[StreamIndex] = NULL;

	// Build the shader declarations.
	FGL4ESShaderDeclaration	ShaderDeclaration;

	ShaderDeclaration.NumStreams = NumStreams;

	// Add the vertex stream components to the shader declaration.
	for(INT StreamIndex = 0;StreamIndex < NumStreams;StreamIndex++)
		ShaderDeclaration.Streams[StreamIndex] = FGL4ESStreamDeclaration(Streams[StreamIndex]);

	// Find or create an appropriate vertex shader.
	FGL4ESVertexShader* VertexShader = RenDev->GetVertexShader(Shader,ShaderDeclaration);

	// Set the vertex shader.
	CurrentState->VertexShader = VertexShader;

	INT Size		= 0;
	INT TotalSize	= 0;

	if( RenDev->SUPPORTS_GL_NV_vertex_array_range || RenDev->SUPPORTS_GL_APPLE_vertex_array_range )
	{
		for(INT StreamIndex = 0;StreamIndex < NumStreams;StreamIndex++)
			TotalSize += Streams[StreamIndex]->GetSize();
	}

	// Set the vertex streams.
	for(INT StreamIndex = 0;StreamIndex < NumStreams;StreamIndex++)
	{
		// Cache the vertex stream.
		QWORD					CacheId				= Streams[StreamIndex]->GetCacheId();
		FGL4ESVertexStream*	OpenGLVertexStream	= (FGL4ESVertexStream*) RenDev->GetCachedResource(CacheId);

		if( !OpenGLVertexStream )
		{
			if( RenDev->SUPPORTS_GL_ARB_vertex_buffer_object )
				OpenGLVertexStream = new(TEXT("FGL4ESVertexStreamARB_VBO")) FGL4ESVertexStreamARB_VBO(RenDev,CacheId,false);
			else
			if( RenDev->SUPPORTS_GL_ATI_vertex_array_object )
				OpenGLVertexStream = new(TEXT("FGL4ESVertexStreamATI_VAO")) FGL4ESVertexStreamATI_VAO(RenDev,CacheId,false);
			else 
			if( (RenDev->SUPPORTS_GL_NV_vertex_array_range || RenDev->SUPPORTS_GL_APPLE_vertex_array_range )
				&& !Streams[StreamIndex]->HintDynamic() 
				&& (RenDev->VARIndex + TotalSize) < RenDev->VARSize )
					OpenGLVertexStream = new(TEXT("FGL4ESVertexStreamNV_VAR")) FGL4ESVertexStreamNV_VAR(RenDev,CacheId,false);
			else
			if( RenDev->SUPPORTS_GL_APPLE_vertex_array_object 
				&& !Streams[StreamIndex]->HintDynamic() )
				OpenGLVertexStream = new(TEXT("FGL4ESVertexStreamAPPLE_VAO")) FGL4ESVertexStreamAPPLE_VAO(RenDev,CacheId,false);
			else
				OpenGLVertexStream = new(TEXT("FGL4ESVertexStreamARB_VA")) FGL4ESVertexStreamARB_VA(RenDev,CacheId,false);

		}

		if( OpenGLVertexStream->CachedRevision != Streams[StreamIndex]->GetRevision() )
		{
			Size += Streams[StreamIndex]->GetSize();
			OpenGLVertexStream->Cache(Streams[StreamIndex]);
		}

		OpenGLVertexStream->LastFrameUsed = RenDev->FrameCounter;

		// Set the vertex stream.
		INT	Stride = Streams[StreamIndex]->GetStride();

		CurrentState->Streams[StreamIndex]			= OpenGLVertexStream;
		CurrentState->StreamStrides[StreamIndex]	= Stride;
	}

	CurrentState->NumStreams	= NumStreams;
	ArraysDirty					= 1;

	return Size;

	unguard;
}


//
//	FGL4ESRenderInterface::SetDynamicStream
//
INT FGL4ESRenderInterface::SetDynamicStream(EVertexShader Shader,FVertexStream* Stream)
{
	guard(FGL4ESRenderInterface::SetDynamicStream);

	// If there isn't a dynamic vertex stream already, allocate one.
	if(!RenDev->DynamicVertexStream)
	{
		if( RenDev->SUPPORTS_GL_ARB_vertex_buffer_object )
			RenDev->DynamicVertexStream = new FGL4ESVertexStreamARB_VBO(RenDev,NULL,true);
		else
		if( RenDev->SUPPORTS_GL_ATI_vertex_array_object )
			RenDev->DynamicVertexStream = new FGL4ESVertexStreamATI_VAO(RenDev,NULL,true);
		else
			RenDev->DynamicVertexStream = new FGL4ESVertexStreamARB_VA(RenDev,NULL,true);
	}

	// Add the vertices in Stream to the dynamic vertex stream.
	INT	BaseVertexIndex = RenDev->DynamicVertexStream->AddVertices(Stream),
		Stride = Stream->GetStride();

	// Set the dynamic vertex stream.
	CurrentState->Streams[0]		= RenDev->DynamicVertexStream;
	CurrentState->StreamStrides[0]	= Stride;

	// Unset any additional old streams.
	for(INT StreamIndex = 1;StreamIndex < CurrentState->NumStreams;StreamIndex++)
		CurrentState->Streams[StreamIndex] = NULL;

	CurrentState->NumStreams = 1;

	// Find or create an appropriate vertex shader.
	FGL4ESShaderDeclaration	ShaderDeclaration;

	ShaderDeclaration.NumStreams = 1;
	ShaderDeclaration.Streams[0] = FGL4ESStreamDeclaration(Stream);

	// Find or create an appropriate vertex shader.
	FGL4ESVertexShader*	VertexShader = RenDev->GetVertexShader(Shader,ShaderDeclaration);

	CurrentState->VertexShader	= VertexShader;
	ArraysDirty					= 1;

	return BaseVertexIndex;

	unguard;
}


//
//	FGL4ESRenderInterface::SetIndexBuffer
//
INT FGL4ESRenderInterface::SetIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseVertexIndex)
{
	guard(FGL4ESRenderInterface::SetIndexBuffer);
	
	UBOOL RequiresCaching = 0;
	if( IndexBuffer )
	{
		// Cache the index buffer.
		QWORD				CacheId				= IndexBuffer->GetCacheId();
		FGL4ESIndexBuffer*	OpenGLIndexBuffer	= (FGL4ESIndexBuffer*) RenDev->GetCachedResource(CacheId);

		if( !OpenGLIndexBuffer )
		{
			if( RenDev->SUPPORTS_GL_ARB_vertex_buffer_object )
				OpenGLIndexBuffer = new FGL4ESIndexBufferARB_VBO(RenDev,CacheId,false);
			else
			if( RenDev->SUPPORTS_GL_ATI_element_array )
				OpenGLIndexBuffer = new FGL4ESIndexBufferATI(RenDev,CacheId,false);
			else
				OpenGLIndexBuffer = new FGL4ESIndexBufferARB(RenDev,CacheId,false);
		}

		if( OpenGLIndexBuffer->CachedRevision != IndexBuffer->GetRevision() )
		{
			OpenGLIndexBuffer->Cache(IndexBuffer);
			RequiresCaching |= 1;
		}

		OpenGLIndexBuffer->LastFrameUsed = RenDev->FrameCounter;

		// Set the index buffer.
		CurrentState->IndexBuffer			= OpenGLIndexBuffer;
		CurrentState->IndexBufferBase		= BaseVertexIndex;
	}
	else
	{
		// Clear the index buffer.
		if(CurrentState->IndexBuffer != NULL)
		{
			CurrentState->IndexBuffer		= NULL;
			CurrentState->IndexBufferBase	= 0;
		}
	}

	ArraysDirty = 1;

	return RequiresCaching ? IndexBuffer->GetSize() : 0;

	unguard;
}


//
//	FGL4ESRenderInterface::SetDynamicIndexBuffer
//
INT FGL4ESRenderInterface::SetDynamicIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseVertexIndex)
{
	guard( FGL4ESRenderInterface::SetDynamicIndexBuffer);

	check( IndexBuffer->GetIndexSize() == sizeof(_WORD) );

	// If there isn't a dynamic index buffer already, allocate one.
	if( !RenDev->DynamicIndexBuffer )
	{
		if( RenDev->SUPPORTS_GL_ARB_vertex_buffer_object )
			RenDev->DynamicIndexBuffer = new FGL4ESIndexBufferARB_VBO(RenDev,NULL,true);
		else
		if( RenDev->SUPPORTS_GL_ATI_element_array )
			RenDev->DynamicIndexBuffer = new FGL4ESIndexBufferATI(RenDev,NULL,true);
		else
			RenDev->DynamicIndexBuffer = new FGL4ESIndexBufferARB(RenDev,NULL,true);
	}

	// Add the indices in the index buffer to the dynamic index buffer.
	INT	BaseIndex = RenDev->DynamicIndexBuffer->AddIndices(IndexBuffer);

	// Set the dynamic index buffer.
	CurrentState->IndexBuffer		= RenDev->DynamicIndexBuffer;
	CurrentState->IndexBufferBase	= BaseVertexIndex;
	ArraysDirty						= 1;

	return BaseIndex;

	unguard;
}


//
//	FGL4ESRenderInterface::CommitLights
//
void FGL4ESRenderInterface::CommitLights()
{
	guard(FGL4ESRenderInterface::CommitLights());

	if( CurrentState->UseDynamicLighting )
	{
		RenDev->glMatrixMode( GL_MODELVIEW );
		RenDev->glPushMatrix();
		RenDev->glLoadIdentity();

		for( INT LightIndex=0; LightIndex<8; LightIndex++ )
		{
			if( CurrentState->Lights[LightIndex].Enabled )
			{		
				FPlane Position	= CurrentState->Lights[LightIndex].Position;
				Position = CurrentState->WorldToCamera.TransformFPlane( Position );
				RenDev->glLightfv( GL_LIGHT0 + LightIndex, GL_POSITION, (GLfloat*) &Position );
		
				if( CurrentState->Lights[LightIndex].UseDirection )
				{
					FPlane Direction = CurrentState->Lights[LightIndex].Direction;
					Direction = CurrentState->WorldToCamera.TransformFPlane( Direction );
					RenDev->glLightfv( GL_LIGHT0 + LightIndex, GL_SPOT_DIRECTION, (GLfloat*) &Direction );
				}

				RenDev->glLightf( GL_LIGHT0 + LightIndex, GL_SPOT_CUTOFF  , CurrentState->Lights[LightIndex].SpotCutoff		);
				RenDev->glLightf( GL_LIGHT0 + LightIndex, GL_SPOT_EXPONENT, CurrentState->Lights[LightIndex].SpotExponent	);

				RenDev->glLightf( GL_LIGHT0 + LightIndex, GL_CONSTANT_ATTENUATION , CurrentState->Lights[LightIndex].ConstantAttenuation	);
				RenDev->glLightf( GL_LIGHT0 + LightIndex, GL_LINEAR_ATTENUATION	  , CurrentState->Lights[LightIndex].LinearAttenuation		);
				RenDev->glLightf( GL_LIGHT0 + LightIndex, GL_QUADRATIC_ATTENUATION, CurrentState->Lights[LightIndex].QuadraticAttenuation	);
				
				RenDev->glLightfv( GL_LIGHT0 + LightIndex, GL_DIFFUSE, (GLfloat*) &CurrentState->Lights[LightIndex].Diffuse );
			
				RenDev->glEnable( GL_LIGHT0 + LightIndex );
			}
			else
				RenDev->glDisable( GL_LIGHT0 + LightIndex );
		}

		FPlane AmbientColor = CurrentState->AmbientLightColor.Plane();
		RenDev->glLightModelfv( GL_LIGHT_MODEL_AMBIENT, (GLfloat*) &AmbientColor );
		RenDev->glEnable( GL_LIGHTING );
		RenDev->glPopMatrix();
	}
	else
		RenDev->glDisable( GL_LIGHTING );
	
	LightsDirty = 0;

	unguard;
}

//
//	FGL4ESRenderInterface::CommitStreams
//
void FGL4ESRenderInterface::CommitStreams( INT FirstIndex, INT Pass )
{
	guard(FGL4ESRenderInterface::CommitStreams());

	UBOOL	NoIndexBuffer	= CurrentState->IndexBuffer == NULL;
	INT		IndexBufferBase = CurrentState->IndexBufferBase;

	struct
	{
		INT		NumCoords;
		INT		Stride;
		INT		VertexOffset;
		void*	VertexBuffer;
	} TexCoordInfo[8];

	INT		NumTexCoords	= 0;

	UBOOL	HasNormals		= 0,
			HasDiffuse		= 0,
			IsVAR			= 1;

	if( NoIndexBuffer )
		IndexBufferBase += FirstIndex;

	if( RenDev->SUPPORTS_GL_APPLE_vertex_array_object )
	{
		GLuint VARId = CurrentState->Streams[0]->IsVAR();
		if( VARId && (CurrentState->NumStreams == 1) )
			RenDev->glBindVertexArrayAPPLE( VARId );
		else
			RenDev->glBindVertexArrayAPPLE( 0 );
	}

	for( INT StreamIndex=0; StreamIndex<CurrentState->NumStreams; StreamIndex++ )
	{
		IsVAR = IsVAR && CurrentState->Streams[StreamIndex]->IsVAR();

		INT		VertexStride	= CurrentState->StreamStrides[StreamIndex];
		INT		VertexOffset	= VertexStride * IndexBufferBase;
		BYTE*	VertexBuffer	= (BYTE*) CurrentState->Streams[StreamIndex]->GetVertexData();
		GLuint	BufferATI		= (GLuint) ((PTRINT) CurrentState->Streams[StreamIndex]->GetVertexData());

		if( RenDev->SUPPORTS_GL_ARB_vertex_buffer_object )
		{
			RenDev->glBindBufferARB( GL_ARRAY_BUFFER_ARB, BufferATI );
			VertexBuffer = NULL;
		}

		#if MACOSX
		if( RenDev->SUPPORTS_GL_ATI_array_rev_comps_in_4_bytes )
			RenDev->glVertexArrayParameteriAPPLE( GL_ARRAY_REV_COMPS_IN_4_BYTES_ATI, GL_TRUE );
		#endif

		INT Offset = 0;
		for( INT ComponentIndex=0; ComponentIndex<CurrentState->VertexShader->Declaration.Streams[StreamIndex].NumComponents; ComponentIndex++ )
		{
			FVertexComponent	Component		= CurrentState->VertexShader->Declaration.Streams[StreamIndex].Components[ComponentIndex];
			INT					ComponentSize	= 0;

			switch( Component.Type )
			{
			case CT_Float4:
				ComponentSize = 4*4;
				break;
			case CT_Float3:
				ComponentSize = 3*4;
				break;
			case CT_Float2:
				ComponentSize = 2*4;
				break;
			case CT_Float1:
			case CT_Color:
				ComponentSize = 1*4;
				break;
			}

			switch( Component.Function )
			{
			case FVF_Position:
				check( Offset == 0 );
				if( RenDev->SUPPORTS_GL_ATI_vertex_array_object )
					RenDev->glArrayObjectATI( GL_VERTEX_ARRAY, 3, GL_FLOAT, VertexStride, BufferATI, VertexOffset + Offset);
				else
					RenDev->glVertexPointer( 3, GL_FLOAT, VertexStride, VertexBuffer + VertexOffset + Offset );
				break;
			case FVF_Normal:
				if( RenDev->SUPPORTS_GL_ATI_vertex_array_object )
					RenDev->glArrayObjectATI( GL_NORMAL_ARRAY, 3, GL_FLOAT, VertexStride, BufferATI, VertexOffset + Offset);
				else					
					RenDev->glNormalPointer( GL_FLOAT, VertexStride, VertexBuffer + VertexOffset + Offset );
				HasNormals = 1;
				break;
			case FVF_Diffuse:
				if( RenDev->SUPPORTS_GL_ATI_vertex_array_object )
					RenDev->glArrayObjectATI( GL_COLOR_ARRAY, 4, GL_UNSIGNED_BYTE, VertexStride, BufferATI, VertexOffset + Offset );
				else
					RenDev->glColorPointer( 4, GL_UNSIGNED_BYTE, VertexStride, VertexBuffer + VertexOffset + Offset );
				HasDiffuse = 1;
				break;
			case FVF_TexCoord0:
			case FVF_TexCoord1:
			case FVF_TexCoord2:
			case FVF_TexCoord3:
			case FVF_TexCoord4:
			case FVF_TexCoord5:
			case FVF_TexCoord6:
			case FVF_TexCoord7:
				TexCoordInfo[NumTexCoords].NumCoords	= ComponentSize / 4;
				TexCoordInfo[NumTexCoords].VertexBuffer	= VertexBuffer;
				TexCoordInfo[NumTexCoords].VertexOffset = VertexOffset + Offset;
				TexCoordInfo[NumTexCoords].Stride		= VertexStride;
				NumTexCoords++;
			}

			Offset += ComponentSize;
		}
	}

	if( HasNormals )
		RenDev->glEnableClientState( GL_NORMAL_ARRAY );
	else
		RenDev->glDisableClientState( GL_NORMAL_ARRAY );

	if( HasDiffuse && CurrentState->UseStaticLighting )
		RenDev->glEnableClientState( GL_COLOR_ARRAY );
	else
		RenDev->glDisableClientState( GL_COLOR_ARRAY );
		
	if( !HasDiffuse )
		RenDev->glColor4f( 1.0f, 1.0f, 1.0f, 1.0f );

	if( HasDiffuse && !CurrentState->UseStaticLighting )
		RenDev->glColor4f( 0.0f, 0.0f, 0.0f, 0.0f );

	CurrentState->HasDiffuse = HasDiffuse;

	INT	CurrentCoord = 0;
	for( INT StageIndex=0; StageIndex<CurrentState->MaterialPasses[Pass]->StagesUsed; StageIndex++ )
	{
		FGL4ESMaterialStateStage& Stage = CurrentState->MaterialPasses[Pass]->Stages[StageIndex];

		INT CoordIndex = 0;
		switch( Stage.TexCoordIndex )
		{
		case TCS_NoChange:
			CoordIndex = CurrentCoord;
		case TCS_Stream0:
			CoordIndex = 0; break;
		case TCS_Stream1:
			CoordIndex = 1; break;
		case TCS_Stream2:
			CoordIndex = 2; break;
		case TCS_Stream3:
			CoordIndex = 3; break;
		case TCS_Stream4:
			CoordIndex = 4; break;
		case TCS_Stream5:
			CoordIndex = 5; break;
		case TCS_Stream6:
			CoordIndex = 6; break;
		case TCS_Stream7:
			CoordIndex = 7; break;
		default:
			CoordIndex = -1;
		}
		
		check(StageIndex < RenDev->NumTextureUnits);
	
		RenDev->glClientActiveTextureARB( GL_TEXTURE0 + StageIndex );

		if( NumTexCoords && (CoordIndex >= 0) && (CoordIndex < NumTexCoords) )
		{
			if( RenDev->SUPPORTS_GL_ATI_vertex_array_object )
			{
				RenDev->glArrayObjectATI( 
					GL_TEXTURE_COORD_ARRAY, 
					TexCoordInfo[CoordIndex].NumCoords, 
					GL_FLOAT,
					TexCoordInfo[CoordIndex].Stride, 
					(GLuint) ((PTRINT) TexCoordInfo[CoordIndex].VertexBuffer),
					TexCoordInfo[CoordIndex].VertexOffset 
				);
			}
			else
			{
				RenDev->glTexCoordPointer( 
					TexCoordInfo[CoordIndex].NumCoords, 
					GL_FLOAT,
					TexCoordInfo[CoordIndex].Stride,
					(BYTE*) TexCoordInfo[CoordIndex].VertexBuffer + TexCoordInfo[CoordIndex].VertexOffset
				);
			}
			CurrentCoord++;

			RenDev->glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		}
		else
			RenDev->glDisableClientState( GL_TEXTURE_COORD_ARRAY );

	}
	for( INT StageIndex=CurrentState->MaterialPasses[Pass]->StagesUsed; StageIndex<RenDev->NumTextureUnits; StageIndex++ )
	{
		RenDev->glClientActiveTextureARB( GL_TEXTURE0 + StageIndex );
		RenDev->glDisableClientState( GL_TEXTURE_COORD_ARRAY );
	}

	if( RenDev->SUPPORTS_GL_NV_vertex_array_range || RenDev->SUPPORTS_GL_APPLE_vertex_array_range || RenDev->SUPPORTS_GL_APPLE_vertex_array_object )
	{
		if( !CurrentState->NumStreams )
			IsVAR = 0;

		if( RenDev->SUPPORTS_GL_APPLE_vertex_array_object && (CurrentState->NumStreams > 1) )
			IsVAR = 0;

		if( RenDev->SUPPORTS_GL_APPLE_vertex_array_object || (IsVAR != RenDev->IsVAR) )
		{
			RenDev->IsVAR = IsVAR;
#if MACOSX
#define GL_VERTEX_ARRAY_RANGE GL_VERTEX_ARRAY_RANGE_APPLE
#else
#define GL_VERTEX_ARRAY_RANGE GL_VERTEX_ARRAY_RANGE_WITHOUT_FLUSH_NV
#endif
			if( RenDev->IsVAR )
				RenDev->glEnableClientState( GL_VERTEX_ARRAY_RANGE );
			else
				RenDev->glDisableClientState( GL_VERTEX_ARRAY_RANGE );
		}
	}

	ArraysDirty = 0;

	unguard;
}

//
//	FGL4ESRenderInterface::CommitMatrices
//
void FGL4ESRenderInterface::CommitMatrices()
{
	guard(FGL4ESRenderInterface::CommitMatrices);

	if( MatricesDirty & GL4ES_MATRIX_Modelview )
	{
		FMatrix ModelView = CurrentState->LocalToWorld * CurrentState->WorldToCamera;
		RenDev->glMatrixMode( GL_MODELVIEW );
		RenDev->glLoadMatrixf( (GLfloat*) &ModelView );
	}
	 
	if( MatricesDirty & GL4ES_MATRIX_Projection )
	{
		RenDev->glMatrixMode( GL_PROJECTION );
		RenDev->glLoadMatrixf( (GLfloat*) &CurrentState->CameraToScreen );
	}

	MatricesDirty = 0;

	unguard;
}

//
//	FGL4ESRenderInterface::DrawPrimitive
//
void FGL4ESRenderInterface::DrawPrimitive(EPrimitiveType PrimitiveType,INT FirstIndex,INT NumPrimitives,INT MinIndex,INT MaxIndex)
{
	guard(FGL4ESRenderInterface::DrawPrimitive);

	// if zero, we failed to create a render target...
	if ((CurrentState->OtherRenderTarget) && (CurrentState->RenderTargetSurface == 0))
		return;

	if( MatricesDirty )
		CommitMatrices();

	if( LightsDirty )
		CommitLights();

	for( INT Pass=0; Pass<CurrentState->NumMaterialPasses; Pass++ )
	{
		SetMaterialBlending( CurrentState->MaterialPasses[Pass] );

		// Fog hacks needed for translucent objects.
		UBOOL RestoreFogColor = 0;
		if( CurrentState->DistanceFogEnabled && CurrentState->CurrentMaterialState->OverrideFogColor )
		{
			FPlane FogColor = CurrentState->CurrentMaterialState->OverriddenFogColor.Plane();
			RestoreFogColor = 1;
			RenDev->glFogfv( GL_FOG_COLOR, (GLfloat*) &FogColor );
		}

		try
		{
			UBOOL	NoIndexBuffer	= CurrentState->IndexBuffer == NULL;
			_WORD*	IndexBuffer		= NoIndexBuffer ? NULL : (_WORD*) CurrentState->IndexBuffer->GetIndexData();
			GLuint	IndexBufferATI	= NoIndexBuffer ? 0 : (GLuint) ((PTRINT) CurrentState->IndexBuffer->GetIndexData());
			DWORD	Count			= NumPrimitives;
			GLenum	Type;

			if( RenDev->SUPPORTS_GL_ARB_vertex_buffer_object && !NoIndexBuffer )
			{
				RenDev->glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, IndexBufferATI );
				IndexBuffer = NULL;
			}

			if( NoIndexBuffer || ArraysDirty || (CurrentState->NumMaterialPasses > 1) )
				CommitStreams( FirstIndex, Pass );

			switch( PrimitiveType )
			{
			case PT_TriangleList:
				Type	= GL_TRIANGLES;
				Count	*= 3; 
				break;
			case PT_TriangleStrip:
				Type	= GL_TRIANGLE_STRIP;
				Count	+= 2;
				break;
			case PT_TriangleFan:
				Type	= GL_TRIANGLE_FAN;
				Count	+= 2;
				break;
			case PT_PointList:
				Type	= GL_POINTS;
				break;
			case PT_LineList:
				Type	= GL_LINES;
				Count	*= 2;
				break;
			default:
				Type	= GL_TRIANGLES;
				Count	= 0;
			}
	
			if( CurrentState->UseDynamicLighting )
			{
				if( CurrentState->HasDiffuse )
				{
					RenDev->glEnable( GL_COLOR_MATERIAL );
				}
				else
				{
					RenDev->glDisable( GL_COLOR_MATERIAL );
					FLOAT ColorZero[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
					RenDev->glMaterialfv( GL_FRONT_AND_BACK, GL_EMISSION, (GLfloat*) ColorZero );
				}
			}

			if( NoIndexBuffer )
			{
				if( RenDev->SUPPORTS_GL_ATI_element_array )
					RenDev->glDisableClientState( GL_ELEMENT_ARRAY_ATI );
				RenDev->glDrawArrays( Type, 0, Count );
			}
			else
			{
				INT	IndexOffset	= FirstIndex;
				if( RenDev->SUPPORTS_GL_ATI_element_array )
				{
					RenDev->glEnableClientState( GL_ELEMENT_ARRAY_ATI );
					RenDev->glArrayObjectATI( GL_ELEMENT_ARRAY_ATI, 1, GL_UNSIGNED_SHORT, 0, IndexBufferATI, IndexOffset * sizeof(_WORD) );
					RenDev->glDrawRangeElementArrayATI( Type, MinIndex, MaxIndex, Count );
				}
				else
					RenDev->glDrawRangeElements( Type, MinIndex, MaxIndex, Count, GL_UNSIGNED_SHORT, IndexBuffer + IndexOffset );
			}
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

		if( RestoreFogColor )
		{
			FPlane FogColor = CurrentState->DistanceFogColor.Plane();
			RenDev->glFogfv( GL_FOG_COLOR, (GLfloat*) &FogColor );
		}

	}

	unguard;
}


//
//	FGL4ESRenderInterface::Locked
//
void FGL4ESRenderInterface::Locked( UViewport* InViewport, BYTE* InHitData,INT* InHitSize)
{
	guard(FGL4ESRenderInterface::Locked);

	Viewport = InViewport;

	// Store the render state.
	SavedStateIndex = 0;
	PushState();

	CurrentState->ViewportX				= 0;
	CurrentState->ViewportY				= 0;
	CurrentState->ViewportWidth			= Viewport->SizeX;
	CurrentState->ViewportHeight		= Viewport->SizeY;

	unguard;
}


//
//	FGL4ESRenderInterface::Unlocked
//
void FGL4ESRenderInterface::Unlocked()
{
	guard(FGL4ESRenderInterface::Unlocked);

	Viewport = NULL;

	// Restore initial state.
	PopState();
	check(SavedStateIndex == 0);

	unguard;
}


//
//	DE's additions
//

//
//	FGL4ESRenderInterface::LockDynBuffer
//
INT FGL4ESRenderInterface::LockDynBuffer(BYTE** pOutBuffer, int numVerts, int stride, DWORD componentFlags)
{
    guard( FGL4ESRenderInterface::LockDynBuffer );
    if ( numVerts == 0 )
	{
		(*pOutBuffer) = NULL;
		return 0;
	}

    if( numVerts >= RenDev->DE_QuadIndexBuffer.MaxVertIndex ) // prevent quad index overflow
        numVerts = RenDev->DE_QuadIndexBuffer.MaxVertIndex-1;

    RenDev->DE_DynamicVertexStream.Init( pOutBuffer, numVerts, stride, componentFlags );
	return numVerts;
	unguard;
}

//
//	FGL4ESRenderInterface::UnlockDynBuffer
//
INT FGL4ESRenderInterface::UnlockDynBuffer( void )
{
	INT first = SetDynamicStream(VS_FixedFunction,&RenDev->DE_DynamicVertexStream);
	return first;
}

//
//	FGL4ESRenderInterface::DrawDynQuads
//
void FGL4ESRenderInterface::DrawDynQuads(INT NumPrimitives)
{
	guard(FGL4ESRenderInterface::DrawDynQuads);
	
	// if zero, we failed to create a render target...
	if ((CurrentState->OtherRenderTarget) && (CurrentState->RenderTargetSurface == 0))
		return;

	if (!NumPrimitives)
		return;

    INT first = UnlockDynBuffer();
    SetIndexBuffer( &RenDev->DE_QuadIndexBuffer, first );
	DrawPrimitive( PT_TriangleList, 0, NumPrimitives * 2, 0, NumPrimitives * 4 - 1 );
	
	unguard;
}

//
//	FGL4ESRenderInterface::DrawQuads
//
void FGL4ESRenderInterface::DrawQuads(INT FirstVertex, INT NumPrimitives)
{
	guard(FGL4ESRenderInterface::DrawQuads);

	// if zero, we failed to create a render target...
	if ((CurrentState->OtherRenderTarget) && (CurrentState->RenderTargetSurface == 0))
		return;

	if (!NumPrimitives)
		return;

	SetIndexBuffer( &RenDev->DE_QuadIndexBuffer, FirstVertex );
    DrawPrimitive( PT_TriangleList, 0, NumPrimitives * 2, 0, NumPrimitives * 4 - 1 );
	
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

