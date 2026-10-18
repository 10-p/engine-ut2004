/*=============================================================================
	PixoRenderInterface.cpp: Unreal Pixo support.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Michael Sartain from GL driver
		* Taken over by Daniel Vogel

=============================================================================*/

#include "PixoDrv.h"
#include <malloc.h>

//$ HACK
#define PT_QuadList     0x7fffffff

FPixoRenderInterface::FPixoSavedState *FPixoRenderInterface::GCurrentDrawPrimitiveState = NULL;

// PixoLighting.cpp function decls.
UBOOL PixoCommitLights(FPixoRenderInterface::FPixoSavedState *CurrentState);
void PixoLightUpdateStreamData();

#if !WIN32
#define MAKELONG(x, y) ((INT) ( ((INT) (x)) | ( ((INT) (y)) << 16) ))

// !!! FIXME: Clean this up.  --ryan.
static void *_aligned_malloc(size_t size, size_t alignment)
{
	size += alignment + sizeof (void *);
	void *rc = malloc(size);
	if (!rc)
		return(NULL);

	BYTE *retval = ((BYTE *) rc) + sizeof (void *);
	while (1)
	{
		if ( (((PTRINT) retval) % alignment) == 0 )
		{
			void **storagepoint = (void **) (retval - sizeof (void *));
			*storagepoint = rc;  // for free()ing later.
			break;
		}
		retval++;
	}

	return(retval);
}

static void _aligned_free(void *ptr)
{
	void **storagepoint = (void **) (((PTRINT) ptr) - sizeof (void *));
	free(*storagepoint);
}
#endif

//
//  FPixoRenderInterface::FPixoRenderInterface
//
FPixoRenderInterface::FPixoRenderInterface(UPixoRenderDevice* InRenDev)
{
	guard(FPixoRenderInterface::FPixoRenderInterface);

	RenDev          = InRenDev;
	Viewport        = NULL;
	PrecacheMode    = PRECACHE_All;

	SavedStates		= (FPixoSavedState*) _aligned_malloc( sizeof(FPixoSavedState) * (MAX_STATESTACKDEPTH + 1), 32 );
	appMemzero( SavedStates, sizeof(FPixoSavedState) * MAX_STATESTACKDEPTH );
	SavedStateIndex = 0;
	CurrentState    = &SavedStates[SavedStateIndex];

	unguard;
}

//
//  FPixoRenderInterface::~FPixoRenderInterface
//
FPixoRenderInterface::~FPixoRenderInterface()
{
	guard(FPixoRenderInterface::~FPixoRenderInterface);
	_aligned_free( SavedStates );
	unguard;
}

//
//  FPixoRenderInterface::PushState
//
void FPixoRenderInterface::PushState()
{
	guard(FPixoRenderInterface::PushState);

	// Push matrices and assorted state.
	check(SavedStateIndex + 1 < MAX_STATESTACKDEPTH);

	CurrentState = &SavedStates[SavedStateIndex+1];
	appMemcpy( CurrentState, &SavedStates[SavedStateIndex], sizeof(FPixoSavedState) );
	SavedStateIndex++;

	for(INT PassIndex = 0;PassIndex < CurrentState->NumMaterialPasses;PassIndex++)
		CurrentState->MaterialPasses[PassIndex]->NumRefs++;

	unguard;
}


//
//  FPixoRenderInterface::PopState
//
void FPixoRenderInterface::PopState()
{
	guard(FPixoRenderInterface::PopState);

	// Pop a saved state off the saved state stack.
	if( SavedStateIndex == 0 )
		appErrorf(TEXT("PopState stack underflow"));

	FPixoSavedState&    OldState    = SavedStates[SavedStateIndex--];
	CurrentState                    = &SavedStates[SavedStateIndex];

	// Set popped state.
	SetTransform( TT_LocalToWorld  , CurrentState->LocalToWorld   );
	SetTransform( TT_WorldToCamera , CurrentState->WorldToCamera  );
	SetTransform( TT_CameraToScreen, CurrentState->CameraToScreen );

	// Set changed state.
	PixoSetPolygonZBias(CurrentState->ZBias);

	SetCullMode( CurrentState->CullMode );

	if( !CurrentState->OtherFrameBuffer )
		PixoSetFrameBuffer( RenDev->PixoBuffer->Buffer, RenDev->PixoBuffer->BufferPitch );
	else
		PixoSetFrameBuffer( CurrentState->OtherFrameBuffer, CurrentState->OtherFrameBufferPitch );
	
	if( !CurrentState->OtherZBuffer )
		PixoSetZBuffer( RenDev->ZBuffer, (PixoHint & PIXOHINT_HUD) && RenDev->Zoom2X ? 2 * RenDev->ZBufferPitch : RenDev->ZBufferPitch, PixoGetZBufferType() );
	else
		PixoSetZBuffer( CurrentState->OtherZBuffer, CurrentState->OtherZBufferPitch, PixoGetZBufferType() );

	SetViewport( CurrentState->ViewportX, CurrentState->ViewportY,  CurrentState->ViewportWidth, CurrentState->ViewportHeight );

	SetDistanceFog( CurrentState->DistanceFogEnabled, CurrentState->DistanceFogStart, CurrentState->DistanceFogEnd,	CurrentState->DistanceFogColor );

	PixoSetAlphaRefValue( CurrentState->AlphaRef );
	PixoSetWStateACompare( CurrentState->AlphaTest ? PIXO_ACOMPARE_GT : PIXO_ACOMPARE_ALWAYS );

	PixoSetWStateZCompare( CurrentState->ZTest ? PIXO_ZCOMPARE_LE : PIXO_ZCOMPARE_ALWAYS );
	PixoSetWStateZWrite( CurrentState->ZWrite ?	PIXO_ZWRITE_ON : PIXO_ZWRITE_OFF );

	// Restore material state.
	for(INT PassIndex = 0;OldState.MaterialPasses[PassIndex];PassIndex++)
	{
		if(--OldState.MaterialPasses[PassIndex]->NumRefs == 0)
			MaterialStatePool.FreeState(OldState.MaterialPasses[PassIndex]);
		OldState.MaterialPasses[PassIndex] = NULL;
	}

	CurrentState->CurrentMaterialState  = NULL;
	CurrentState->LightsDirty           = 1;

	unguard;
}


//
//  FPixoRenderInterface::SetRenderTarget
//
UBOOL FPixoRenderInterface::SetRenderTarget(FRenderTarget* RenderTarget)
{
	guard(FPixoRenderInterface::SetRenderTarget);

	QWORD			CacheId		= RenderTarget->GetCacheId();
	FPixoTexture*	PixoTexture = (FPixoTexture*) RenDev->GetCachedResource(CacheId);

	if(!PixoTexture)
		PixoTexture = new(TEXT("FPixoTexture")) FPixoTexture(RenDev,CacheId);

	if(PixoTexture->CachedRevision != RenderTarget->GetRevision())
		PixoTexture->Cache(RenderTarget);

	if( PixoTexture->FrameBuffer && PixoTexture->ZBuffer )
	{
		PixoSetFrameBuffer( PixoTexture->FrameBuffer, PixoTexture->FrameBufferPitch );
		PixoSetZBuffer( PixoTexture->ZBuffer, PixoTexture->ZBufferPitch, PixoGetZBufferType() );
		
		CurrentState->OtherFrameBuffer		= PixoTexture->FrameBuffer;
		CurrentState->OtherZBuffer			= PixoTexture->ZBuffer;
		CurrentState->OtherFrameBufferPitch	= PixoTexture->FrameBufferPitch;
		CurrentState->OtherZBufferPitch		= PixoTexture->ZBufferPitch;

		SetViewport( 0, 0, PixoTexture->CachedWidth, PixoTexture->CachedHeight );

		return 1;
	}

	return 0;

	unguard;
}


//
//  FPixoRenderInterface::SetViewport
//
void FPixoRenderInterface::SetViewport(INT X,INT Y,INT Width,INT Height)
{
	guard(FPixoRenderInterface::SetViewport);

	if( RenDev->Zoom2X && !(PixoHint & PIXOHINT_HUD) && !CurrentState->OtherFrameBuffer )
	{
		PixoSetViewport(X/2, Y/2, Width/2, Height/2);
	}
	else
	{
		PixoSetViewport(X, Y, Width, Height);
	}

	CurrentState->ViewportX = X;
	CurrentState->ViewportY = Y;
	CurrentState->ViewportWidth	= Width;
	CurrentState->ViewportHeight = Height;

	unguard;
}


//
//  FPixoRenderInterface::Clear
//
void FPixoRenderInterface::Clear(UBOOL UseColor,FColor Color,UBOOL UseDepth,FLOAT Depth,UBOOL UseStencil,DWORD Stencil)
{
	guard(FPixoRenderInterface::Clear);

	PixoClearViewport(
		(UseColor ? PIXO_CLEARFRAMEBUFFER : 0) |
			(UseDepth ? (PIXO_CLEARZBUFFER | PIXO_CLEARSTENCIL) : 0),
		Color,
		Depth,
		Stencil & 0xff);

	unguard;
}


//
//  FPixoRenderInterface::PushHit
//
void FPixoRenderInterface::PushHit(const BYTE* Data,INT Count)
{
	guard(FPixoRenderInterface::PushHit);
	unguard;
}


//
//  FPixoRenderInterface::PopHit
//
void FPixoRenderInterface::PopHit(INT Count,UBOOL Force)
{
	guard(FPixoRenderInterface::PopHit);
	unguard;
}


//
//  FPixoRenderInterface::SetCullMode
//
void FPixoRenderInterface::SetCullMode(ECullMode CullMode)
{
	guard(FPixoRenderInterface::SetCullMode);

	CurrentState->CullMode = CullMode;
	switch( CullMode )
	{
	case CM_CW:
		PixoSetCullMode(PIXO_CULL_CW);
		break;
	case CM_CCW:
		PixoSetCullMode(PIXO_CULL_CCW);
		break;
	case CM_None:
		PixoSetCullMode(PIXO_CULL_NONE);
		break;
	}

	unguard;
}


//
//  FPixoRenderInterface::SetAmbientLight
//
void FPixoRenderInterface::SetAmbientLight(FColor Color)
{
	guard(FPixoRenderInterface::SetAmbientLight);

	CurrentState->AmbientLightColor = Color;
	CurrentState->LightsDirty       = 1;

	unguard;
}


//
//  FPixoRenderInterface::EnableLighting
//
void FPixoRenderInterface::EnableLighting(UBOOL UseDynamic, UBOOL UseStatic, UBOOL Modulate2X, FBaseTexture* LightmapTexture, UBOOL LightingOnly, FSphere LitSphere )
{
	guard(FPixoRenderInterface::EnableLighting);

	FPixoTexture* Lightmap = NULL;
	if( LightmapTexture )
		Lightmap = CacheTexture( LightmapTexture );

	CurrentState->Lightmap              = Lightmap;
	CurrentState->LightingOnly          = LightingOnly;

	CurrentState->UseDynamicLighting    = UseDynamic;
	CurrentState->UseStaticLighting     = UseStatic;

	CurrentState->LightingModulate2X    = Modulate2X;
	CurrentState->LitSphere             = LitSphere;

	CurrentState->LightsDirty           = 1;

	if(CurrentState->UseDynamicLighting)
	{
		SPEW_PIXO_WARNING(TEXT("Dynamic Lighting not supported"));
	}

	unguard;
}

//
//  FPixoRenderInterface::SetLight
//
void FPixoRenderInterface::SetLight(INT LightIndex,
	FDynamicLight *Light, FLOAT Scale)
{
	guard(FPixoRenderInterface::SetLight);

	if( LightIndex < 8 )
	{
		FPixoLightState *PixoLight = &CurrentState->Lights[LightIndex];

 		PixoLight->Type = PIXO_LIGHT_NONE;

		if(Light)
		{
			if(Light->Actor && Light->Actor->LightEffect == LE_Sunlight)
			{
				// Directional light
				PixoLight->Type			= PIXO_LIGHT_DIRECTIONAL;
				PixoLight->Direction	= -Light->Direction;

				FLOAT LightAlphaScale	= 2.25f * Light->Alpha * Scale;
				PixoLight->Diffuse.X	= Min( 1.f, Light->Color.X * LightAlphaScale );
				PixoLight->Diffuse.Y	= Min( 1.f, Light->Color.Y * LightAlphaScale );
				PixoLight->Diffuse.Z	= Min( 1.f, Light->Color.Z * LightAlphaScale );
			}
			else 
			if( Light->Actor && Light->Actor->LightEffect == LE_QuadraticNonIncidence || CurrentState->LitSphere.W == -1.0f )
			{
				// Point light
				PixoLight->Type         = PIXO_LIGHT_POINT_QUADRATIC_NON_INCIDENCE;
				PixoLight->Position     = Light->Position;
				PixoLight->Radius       = Light->Radius;

				float LightAlphaScale   = Light->Alpha * Scale;
				PixoLight->Diffuse.X	= Min( 1.f, Light->Color.X * LightAlphaScale );
				PixoLight->Diffuse.Y	= Min( 1.f, Light->Color.Y * LightAlphaScale );
				PixoLight->Diffuse.Z	= Min( 1.f, Light->Color.Z * LightAlphaScale );
			}
			else 
			if( Light->Actor &&	( Light->Actor->LightEffect == LE_StaticSpot ||	Light->Actor->LightEffect == LE_Spotlight) )
			{
				// Spot light
				SPEW_PIXO_WARNING(TEXT("Spot lights not supported."));
			}
			else
			{
				// Point light
				PixoLight->Type         = PIXO_LIGHT_POINT;
				PixoLight->Position     = Light->Position;
				PixoLight->Radius       = Light->Radius;

				float LightAlphaScale   = Light->Alpha * Scale;
				PixoLight->Diffuse.X    = Min( 1.f, Light->Color.X * LightAlphaScale );
				PixoLight->Diffuse.Y    = Min( 1.f, Light->Color.Y * LightAlphaScale );
				PixoLight->Diffuse.Z    = Min( 1.f, Light->Color.Z * LightAlphaScale );

				if(Light->Actor && (Light->Actor->LightEffect == LE_Negative))
				{
					PixoLight->Diffuse.X *= -1.0f;
					PixoLight->Diffuse.Y *= -1.0f;
					PixoLight->Diffuse.Z *= -1.0f;
				}
			}
		}

		if( PixoLight->Type != PIXO_LIGHT_NONE )
			CurrentState->LightsEnabled |= 1 << LightIndex;
		else
			CurrentState->LightsEnabled &= ~(1 << LightIndex);
	}

	CurrentState->LightsDirty = 1;

	unguard;
}


//
// FPixoRenderInterface::SetNPatchTesselation
//
void FPixoRenderInterface::SetNPatchTesselation( FLOAT Tesselation )
{
	guard(FPixoRenderInterface::SetNPatchTesselation);
	unguard;
}


//
//  FPixoRenderInterface::SetDistanceFog
//
void FPixoRenderInterface::SetDistanceFog(UBOOL Enable,
	FLOAT FogStart, FLOAT FogEnd, FColor Color)
{
	guard(FPixoRenderInterface::SetDistanceFog);

	if(!RenDev->FogEnabled)
		Enable = 0;

	if( Abs(FogStart - FogEnd) < 1.f )
		FogStart = Max( 0.f, FogEnd - 1.f );

	CurrentState->DistanceFogEnabled    = Enable;
	CurrentState->DistanceFogColor      = Color;
	CurrentState->DistanceFogStart      = FogStart;
	CurrentState->DistanceFogEnd        = FogEnd;

	if( Enable )
	{
		FPlane FogColor = Color.Plane();
		PixoSetFogColor(PixoMakeArgb04f(FogColor.W,  FogColor.X, FogColor.Y, FogColor.Z));
		PixoSetVertexDistanceFog( PIXO_VERTEXDISTANCEFOG_ON, FogStart, FogEnd, PIXO_VERTEXDISTANCEFOG_W_CAMERASPACE );
		PixoSetWStateFog( PIXO_FOG_ON );
	}
	else
		PixoSetWStateFog( PIXO_FOG_OFF );

	unguard;
}


//
//  FPixoRenderInterface::SetGlobalColor
//
void FPixoRenderInterface::SetGlobalColor(FColor Color)
{
	guard(FPixoRenderInterface::SetGlobalColor);

	CurrentState->MaterialPasses[CurrentState->NumMaterialPasses-1]->TFactorColor = Color;

	FPlane TFactorColor = Color.Plane();
	for( INT i=0; i<CurrentState->MaterialPasses[CurrentState->NumMaterialPasses-1]->StagesUsed; i++ )
	{
		PixoSetModulationFactor(
			TFactorColor.W,
			TFactorColor.X,
			TFactorColor.Y,
			TFactorColor.Z);
	}

	unguard;
}


//
//  FPixoRenderInterface::SetTransform
//
void FPixoRenderInterface::SetTransform(ETransformType Type, const FMatrix& Matrix)
{
	guard(FPixoRenderInterface::SetTransform);

	switch( Type )
	{
	case TT_LocalToWorld:
		CurrentState->LocalToWorld = Matrix;
		PixoSetWorldTransformMatrix((float *)&CurrentState->LocalToWorld);
		CurrentState->LocalToWorldDirty = 1;
		break;

	case TT_WorldToCamera:
		CurrentState->WorldToCamera = Matrix;
		PixoSetViewTransformMatrix((float *)&CurrentState->WorldToCamera);
		break;

	case TT_CameraToScreen:
		CurrentState->CameraToScreen = Matrix;
		PixoSetProjectionTransformMatrix((float *)&CurrentState->CameraToScreen);
		break;
	}

	// If we have any spot lights on clipped geometry we have to back project
	//  those guys into object space which uses the inverse of the composite
	//  matrix. So if any of these dudes change then lighting is now dirty.
	if( CurrentState->UseDynamicLighting )
		CurrentState->LightsDirty = 1;

	unguard;
}


//
//  FPixoRenderInterface::SetTexture
//
void FPixoRenderInterface::SetTexture( int Stage, FPixoTexture* Texture )
{
	guard(FPixoRenderInterface::SetBitmapMaterial);

	if( Texture )
	{
		PIXO_TEX_FORMAT TexFormat;

		if(Texture->Palette)
		{
			TexFormat = PIXO_TEX_FORMAT_PALETTIZED;

			PixoSetTexPaletteTableEntry(Stage, 0,
				(PIXO_ARGB0 *)Texture->Palette, 256 * 4);
		}
		else
		{
			TexFormat = PIXO_TEX_FORMAT_ARGB8888;
		}

		PixoBeginTextureSpecification(Stage, TexFormat,
			Texture->CachedWidth, Texture->CachedHeight, Texture->MaxMipLevel);

		for(INT MipLevel = 0; MipLevel < Texture->MaxMipLevel; MipLevel++)
		{
			check(Texture->Texels[MipLevel]);
			PixoSetMipImage(Stage, MipLevel, Texture->Texels[MipLevel]);
		}

		PixoEndTextureSpecification(Stage);
	}
	else
	{
		PixoSetTexture(Stage, PIXO_TEX_FORMAT_NONE, 0, 0, NULL);
	}

	unguard;
}

PIXO_RASTEROP GetPixoRasterOpFromGlBlendFactor(DWORD srcblend, DWORD dstblend)
{
	static SQWORD MMX_0 = 0;
#ifdef __GNUC__
	static SQWORD MMX_1 = (SQWORD)0x00ff00ff00ff00ffLL;
#else
	static SQWORD MMX_1 = (SQWORD)0x00ff00ff00ff00ff;
#endif

	static BYTE pixo_rasterop_src_x_dst_x_2[] =
	{
		// src * dst * 2
		0x0F, 0x6E, 0x34, 0x8F,                     // movd      mm6,[edi + ecx*4]  ; dest
		0x0F, 0x60, 0x35, 0x60, 0xCE, 0xE5, 0x01,   // punpcklbw mm6,[MMX_0]
		0x0F, 0xD5, 0xFE,                           // pmullw    mm7,mm6            ; src * dest
		0x0F, 0xDD, 0xFF,                           // paddusw   mm7,mm7            ; src * dest * 2
		0x0F, 0x71, 0xD7, 0x08,                     // psrlw     mm7,8
		0x0F, 0x67, 0xFF,                           // packuswb  mm7,mm7
		0x0F, 0x7E, 0x3C, 0x8F,                     // movd      [edi + ecx*4],mm7
	};
	static BYTE pixo_rasterop_invsrc_x_dst[] =
	{
		// inv_src * dst
		0x0F, 0x6E, 0x34, 0x8F,                     // movd      mm6,[edi + ecx*4]  ; dest
		0x0F, 0x60, 0x35, 0x60, 0xCE, 0xE5, 0x01,   // punpcklbw mm6,[MMX_0]
		0x0F, 0xEF, 0x3D, 0xff, 0xff, 0xff, 0xff,   // pxor      mm7,[MMX_1]
		0x0F, 0xD5, 0xFE,                           // pmullw    mm7,mm6            ; src * dest
		0x0F, 0x71, 0xD7, 0x08,                     // psrlw     mm7,8
		0x0F, 0x67, 0xFF,                           // packuswb  mm7,mm7
		0x0F, 0x7E, 0x3C, 0x8F,                     // movd      [edi + ecx*4],mm7
	};
#if 1
	//
	static BYTE pixo_rasterop_src_plus_one_minus_src_x_dst[] =
	{
		// src + (1-src)*dst == src - src*dst + dst
		0x0F, 0x6E, 0x34, 0x8F,                         // movd      mm6,[edi + ecx*4]  ; dst
		0x0F, 0x60, 0x35, 0xB8, 0xE2, 0xE5, 0x01,       // punpcklbw mm6,[MMX_0]
		0x0F, 0x7F, 0xF5,                               // movq      mm5,mm6
		0x0F, 0xD5, 0xF7,                               // pmullw    mm6,mm7            ; src * dst
		0x0F, 0x71, 0xD6, 0x08,                         // psrlw     mm6,8
		0x0F, 0xF9, 0xF5,                               // psubw     mm6,mm5            ; src*dst - dst
		0x0F, 0xF9, 0xFE,                               // psubw     mm7,mm6            ; src - src*dst + dst
		0x0F, 0x67, 0xFF,                               // packuswb  mm7,mm7
		0x0F, 0x7E, 0x3C, 0x8F,                         // movd      [edi + ecx*4],mm7
	};
#endif

	static bool assigned = false;
	if (!assigned)
	{
		assigned = true;
		*((SQWORD **) (((BYTE *) (pixo_rasterop_src_x_dst_x_2)) + 7)) = &MMX_0;
		*((SQWORD **) (((BYTE *) (pixo_rasterop_src_plus_one_minus_src_x_dst)) + 7)) = &MMX_0;
		*((SQWORD **) (((BYTE *) (pixo_rasterop_invsrc_x_dst)) + 7)) = &MMX_0;
		*((SQWORD **) (((BYTE *) (pixo_rasterop_invsrc_x_dst)) + 14)) = &MMX_1;
	}

	switch(MAKELONG(srcblend, dstblend))
	{
	// Code for blend modes not built into Pixo.
	case MAKELONG(GL_DST_COLOR, GL_SRC_COLOR):
		PixoSetUserRasteropCode(pixo_rasterop_src_x_dst_x_2,
			sizeof(pixo_rasterop_src_x_dst_x_2));
		return PIXO_RASTEROP_USER_CODE;

	case MAKELONG(GL_ZERO, GL_ONE_MINUS_SRC_COLOR):
		PixoSetUserRasteropCode(pixo_rasterop_invsrc_x_dst,
			sizeof(pixo_rasterop_invsrc_x_dst));
		return PIXO_RASTEROP_USER_CODE;

	case MAKELONG(GL_ONE, GL_ONE_MINUS_SRC_COLOR):

#if 1
		PixoSetUserRasteropCode(pixo_rasterop_src_plus_one_minus_src_x_dst,
			sizeof(pixo_rasterop_src_plus_one_minus_src_x_dst));
		return PIXO_RASTEROP_USER_CODE;
#else
		// Just use src + dst and lose the -src*dst term - it's a bit faster.
		// If you can tell the difference and this is bothering you then
		// uncomment out the rasterop_src_plus_one_minus_src_x_dst stuff above
		// and return PIXO_RASTEROP_USER_CODE.
		return PIXO_RASTEROP_ONExSRC_PLUS_ONExDST;
#endif
	// Convert gl blend factors to Pixo rasterops.
	case MAKELONG(GL_ONE, GL_ONE):
		return PIXO_RASTEROP_ONExSRC_PLUS_ONExDST;
	case MAKELONG(GL_ZERO, GL_ONE):
		return PIXO_RASTEROP_ZEROxSRC_PLUS_ONExDST;
	case MAKELONG(GL_ZERO, GL_SRC_COLOR):
		return PIXO_RASTEROP_ZEROxSRC_PLUS_SRCxDST;
	case MAKELONG(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA):
		return PIXO_RASTEROP_ALPHABLEND_ALPHAxSRC_PLUS_ONEMINUSALPHAxDST;
	case MAKELONG(GL_ONE, GL_ONE_MINUS_SRC_ALPHA):
		return PIXO_RASTEROP_ALPHABLEND_ONExSRC_PLUS_ONEMINUSALPHAxDST;
	case MAKELONG(GL_SRC_ALPHA, GL_ONE):
		return PIXO_RASTEROP_ALPHABLEND_ALPHAxSRC_PLUS_ONExDST;
	default:
		SPEW_PIXO_WARNING(TEXT("Blend mode not supported - needs to be added."));
	case MAKELONG(GL_ONE, GL_ZERO):
		return PIXO_RASTEROP_SOURCE_PASSTHROUGH;
	}
}

void FPixoRenderInterface::SetPixoBlendModes(
	FPixoMaterialState* NewMaterialState, UBOOL StageUseTexture[2])
{
	float ColorScale = 1;
	float AlphaScale = 1;
	UBOOL HasTFactor = 0;
	UBOOL HasDiffuse = 0;
	FPlane TFactorColor = NewMaterialState->TFactorColor.Plane();

	StageUseTexture[0] = 0;
	StageUseTexture[1] = 0;

	PixoSetWStateDiffuse(PIXO_DIFFUSE_NONE);
	PixoSetWStateFixedModulation(PIXO_FIXED_MODULATION_1X);

	for(INT s = 0 ; s < NewMaterialState->StagesUsed; s++ )
	{
		FPixoMaterialStateStage& Stage = NewMaterialState->Stages[s];

		ColorScale *= Stage.ColorScale;
		AlphaScale *= Stage.AlphaScale;

		check(&Stage.AlphaOp == &Stage.ColorOp + 1);
		for(DWORD *Op = &Stage.ColorOp; Op <= &Stage.AlphaOp; Op++)
		{
			DWORD *OpSource = (Op == &Stage.ColorOp) ?
				&Stage.SourceRgb0 : &Stage.SourceAlpha0;

			switch(*Op)
			{
			case GL_INTERPOLATE:
				StageUseTexture[s] |= (OpSource[2] == GL_TEXTURE);
				HasTFactor |= (OpSource[2] == GL_CONSTANT);
				HasDiffuse |= (OpSource[2] == GL_PRIMARY_COLOR);
				if(s == 0)
					HasDiffuse |= (OpSource[2] == GL_PREVIOUS);

			case GL_ADD:
			case GL_SUBTRACT:
			case GL_MODULATE:
				StageUseTexture[s] |= (OpSource[1] == GL_TEXTURE);
				HasTFactor |= (OpSource[1] == GL_CONSTANT);
				HasDiffuse |= (OpSource[1] == GL_PRIMARY_COLOR);
				if(s == 0)
					HasDiffuse |= (OpSource[1] == GL_PREVIOUS);

			case GL_REPLACE:
				StageUseTexture[s] |= (OpSource[0] == GL_TEXTURE);
				HasTFactor |= (OpSource[0] == GL_CONSTANT);
				HasDiffuse |= (OpSource[0] == GL_PRIMARY_COLOR);
				if(s == 0)
					HasDiffuse |= (OpSource[0] == GL_PREVIOUS);
				break;

			default:
				check(0);
				break;
			}
		}
	}

	if(HasDiffuse)
		PixoSetWStateDiffuse(PIXO_DIFFUSE_GOURAUD);

	if((ColorScale != 1 || AlphaScale != 1))
	{
		ColorScale = Clamp<float>(ColorScale, 0, 8.0f);
		AlphaScale = Clamp<float>(AlphaScale, 0, 8.0f);

		if(!HasTFactor)
		{
			TFactorColor.X = ColorScale;
			TFactorColor.Y = ColorScale;
			TFactorColor.Z = ColorScale;
			TFactorColor.W = AlphaScale;
			HasTFactor = 1;
		}
		else
		{
			TFactorColor.X *= ColorScale;
			TFactorColor.Y *= ColorScale;
			TFactorColor.Z *= ColorScale;
			TFactorColor.W *= AlphaScale;
		}
	}

	if(HasTFactor)
	{
		PixoSetModulationFactor(
			TFactorColor.W,
			TFactorColor.X,
			TFactorColor.Y,
			TFactorColor.Z);
		PixoSetWStateFixedModulation(PIXO_FIXED_MODULATION_FACTOR);
	}

	NewMaterialState->StagesHasTFactor = HasTFactor;
	NewMaterialState->StagesHasDiffuse = HasDiffuse;
}

//
//  FPixoRenderInterface::SetMaterialBlending
//
void FPixoRenderInterface::SetMaterialBlending( FPixoMaterialState* NewMaterialState )
{
	guard(FD3DRenderInterface::SetMaterialBlending);

	if(CurrentState->CurrentMaterialState == NewMaterialState)
		return;

	CurrentState->CurrentMaterialState = NewMaterialState;

	PixoSetAlphaRefValue( NewMaterialState->AlphaRef );
	PixoSetWStateACompare( NewMaterialState->AlphaTest ? PIXO_ACOMPARE_GT : PIXO_ACOMPARE_ALWAYS );
	
	// Depth buffer.
	if( PixoHint & PIXOHINT_Skybox )
		PixoSetWStateZCompare( PIXO_ZCOMPARE_LE );
	else
		PixoSetWStateZCompare( NewMaterialState->ZTest ? PIXO_ZCOMPARE_LE : PIXO_ZCOMPARE_ALWAYS );

	PixoSetWStateZWrite( NewMaterialState->ZWrite ? PIXO_ZWRITE_ON : PIXO_ZWRITE_OFF );

	// For push/pop state.
	CurrentState->ZWrite    = NewMaterialState->ZWrite;
	CurrentState->ZTest     = NewMaterialState->ZTest;
	CurrentState->AlphaRef  = NewMaterialState->AlphaRef;
	CurrentState->AlphaTest = NewMaterialState->AlphaTest;

	SetCullMode(NewMaterialState->TwoSided ? CM_None : CurrentState->CullMode);

	// Source/destination blending
	PixoSetWStateRasterop( GetPixoRasterOpFromGlBlendFactor(NewMaterialState->SrcBlend, NewMaterialState->DestBlend) );

	// Clear all our texture parameters out.
	for(INT s = 0; s < 2; s++)
	{
		SetTexture(s, NULL);
		PixoSetTextureTransformMatrix(s, PIXO_TRANSFORM_MATRIX_IDENTITY);
		PixoSetTexGen(s, PIXO_TEXGEN_OFF);
		PixoSetWStateTextureProjected(s, PIXO_TEX_PROJECTED_OFF);
	}

	UBOOL StageUseTexture[2];
	SetPixoBlendModes(NewMaterialState, StageUseTexture);

	// Switch to flat shading if fillmode is FM_FlatShaded and gouraud is on.
	if((NewMaterialState->FillMode == FM_FlatShaded) && PixoGetWStateDiffuse())
		PixoSetWStateDiffuse(PIXO_DIFFUSE_FLAT);

	if(StageUseTexture[0] && StageUseTexture[1])
	{
		FPixoMaterialStateStage& Stage0 = NewMaterialState->Stages[0];
		FPixoMaterialStateStage& Stage1 = NewMaterialState->Stages[1];

		// If stage #1 is GL_REPLACE and TEXTURE and stage #2 is GL_REPLACE
		// and PREVIOUS then it is actually just using the one texture from
		// texunit 0 but the texcoords from texunit 1. And Pixo will modulate
		// those two buggers together though so don't use the stage0 texture
		// and copy tex0 over to tex1.
		// "Shader AntalusTextures.sky2.cloudsky2" is an example.
		if(Stage0.ColorOp == GL_REPLACE && Stage0.SourceRgb0 == GL_TEXTURE)
		{
			if(Stage1.ColorOp == GL_REPLACE && Stage1.SourceRgb0 == GL_PREVIOUS)
			{
				StageUseTexture[0] = 0;
				Stage1.Texture = Stage0.Texture;
			}
		}
	}

	if(StageUseTexture[1] && !StageUseTexture[0])
	{
		static PIXO_ARGB0 White = 0xffffffff;

		// Pixo won't allow you set tex1 without tex0 being set. So if we've
		// gotten that set tex0 to white.
		PixoSetTexture(0, PIXO_TEX_FORMAT_ARGB8888, 1, 1, &White);
	}

	// Texture stages and blending
	for(INT s = 0 ; s < NewMaterialState->StagesUsed; s++)
	{
		FPixoMaterialStateStage& Stage = NewMaterialState->Stages[s];

		check(s < sizeof(StageUseTexture) / sizeof(StageUseTexture[0]));
		if(!StageUseTexture[s])
			continue;

		SetTexture(s, Stage.Texture);

		// Pixo can only wrap with textures > 256
		if( Stage.Texture && (Stage.Texture->CachedWidth > 256 || Stage.Texture->CachedHeight > 256) )
			PixoSetWStateTextureBorder(s, PIXO_TEX_BORDER_WRAP);
		else
			PixoSetWStateTextureBorder( s, (Stage.TextureAddressU == GL_REPEAT || Stage.TextureAddressV == GL_REPEAT) ?	PIXO_TEX_BORDER_WRAP : PIXO_TEX_BORDER_CLAMP );

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
		UBOOL       Transpose = 0;
		UBOOL       DoTexGenMatrixConcat = 0;
		FMatrix     TexGenMatrixConcat;

		switch( Stage.TexGenMode )
		{
		case TCS_WorldCoords:
			{
				DoTexGenMatrixConcat = 1;
				memcpy(&TexGenMatrixConcat, PixoGetConcatenatedTransformMatrix(),
					sizeof(TexGenMatrixConcat));
				TexGenMatrixConcat = TexGenMatrixConcat.Inverse();
				PixoSetTexGen(s, PIXO_TEXGEN_UNPROJECTED_XYZW);

				Stage.TextureTransformsEnabled = Stage.UseTexGenMatrix;

				break;
			}
		case TCS_WorldEnvMapCoords:
			{
				SPEW_PIXO_WARNING(TEXT("TCS_WorldEnvMapCoords not supported"));
#ifdef NEVER
				for( INT i=0; i<TexCoordCount; i++ )
				{
					RenDev->glTexGeni( GL_S + i, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP );
					RenDev->glEnable( GL_TEXTURE_GEN_S + i);
				}
#endif /* NEVER */

				if( Stage.UseTexGenMatrix )
					Stage.TextureTransformMatrix = CurrentState->WorldToCamera * Stage.TextureTransformMatrix;
				else
					Stage.TextureTransformMatrix = CurrentState->WorldToCamera;

				Transpose                       = 1;
				Stage.TextureTransformsEnabled  = 1;
				break;
			}
		case TCS_CameraCoords:
			{
				SPEW_PIXO_WARNING(TEXT("TCS_CameraCoords not supported"));

#ifdef NEVER
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
#endif /* NEVER */

				Stage.TextureTransformsEnabled = Stage.UseTexGenMatrix;
				break;
			}
		case TCS_CameraEnvMapCoords:
			{
				SPEW_PIXO_WARNING(TEXT("TCS_CameraEnvMapCoords not supported"));
#ifdef NEVER
				for( INT i=0; i<TexCoordCount; i++ )
				{
					RenDev->glTexGeni( GL_S + i, GL_TEXTURE_GEN_MODE, GL_REFLECTION_MAP );
					RenDev->glEnable( GL_TEXTURE_GEN_S + i);
				}
#endif /* NEVER */

				Stage.TextureTransformsEnabled = Stage.UseTexGenMatrix;
				break;
			}
		case TCS_ProjectorCoords:
			{
				SPEW_PIXO_WARNING(TEXT("TCS_ProjectorCoords not supported"));
#ifdef NEVER
				for( INT i=0; i<TexCoordCount; i++ )
				{
					RenDev->glTexGeni( GL_S + i, GL_TEXTURE_GEN_MODE, GL_NORMAL_MAP );
					RenDev->glEnable( GL_TEXTURE_GEN_S + i);
				}
#endif /* NEVER */

				Stage.TextureTransformsEnabled = Stage.UseTexGenMatrix;
				break;
			}
		case TCS_NoChange:
		default:
			TexCoordCount   = 0;
			break;
		}

		// Convert D3D matrix to OpenGL and also handle 'PROJECTED'.
		if( Stage.TexGenProjected || Stage.TextureTransformsEnabled )
		{
			if( !Stage.TextureTransformsEnabled )
			{
				Stage.TextureTransformMatrix    = FMatrix::Identity;
				Stage.TextureTransformsEnabled  = 1;
			}

			if( Stage.TexGenProjected )
			{
				Stage.TextureTransformMatrix.M[0][3] = Stage.TextureTransformMatrix.M[0][2];
				Stage.TextureTransformMatrix.M[1][3] = Stage.TextureTransformMatrix.M[1][2];
				Stage.TextureTransformMatrix.M[2][3] = Stage.TextureTransformMatrix.M[2][2];
				Stage.TextureTransformMatrix.M[3][3] = Stage.TextureTransformMatrix.M[3][2];

				Stage.TextureTransformMatrix.M[0][2] = 0;
				Stage.TextureTransformMatrix.M[1][2] = 0;
				Stage.TextureTransformMatrix.M[2][2] = 1;
				Stage.TextureTransformMatrix.M[3][2] = 0;

				PixoSetWStateTextureProjected(s, PIXO_TEX_PROJECTED_ON);
			}
			else
			{
				Stage.TextureTransformMatrix.M[3][0] = Stage.TextureTransformMatrix.M[2][0];
				Stage.TextureTransformMatrix.M[3][1] = Stage.TextureTransformMatrix.M[2][1];

				Stage.TextureTransformMatrix.M[2][0] = 0;
				Stage.TextureTransformMatrix.M[2][1] = 0;
			}

			if( Transpose )
				Stage.TextureTransformMatrix = Stage.TextureTransformMatrix.Transpose();
		}

		// Set texture transform matrix.
		if( Stage.TextureTransformsEnabled )
		{
			if(DoTexGenMatrixConcat)
			{
				TexGenMatrixConcat *= Stage.TextureTransformMatrix;
				PixoSetTextureTransformMatrix(s, (float *)&TexGenMatrixConcat);
			}
			else
			{
				PixoSetTextureTransformMatrix(s, (float *)&Stage.TextureTransformMatrix);
			}
		}
	}

	unguard;
}

//
//  FPixoRenderInterface::SetMaterial
//
void FPixoRenderInterface::SetMaterial(UMaterial *InMaterial,
	FString *ErrorString, UMaterial **ErrorMaterial, INT *NumPasses)
{
	guard(FPixoRenderInterface::SetMaterial);

#ifdef _DEBUG
	CurrentState->MaterialName = InMaterial ?
		InMaterial->GetFullName() : TEXT("unknown");
#else
	CurrentState->MaterialName = TEXT("");
#endif

	// Release old material state.
	for(INT PassIndex = 0;CurrentState->MaterialPasses[PassIndex];PassIndex++)
	{
		if(--CurrentState->MaterialPasses[PassIndex]->NumRefs == 0)
			MaterialStatePool.FreeState(CurrentState->MaterialPasses[PassIndex]);

		CurrentState->MaterialPasses[PassIndex] = NULL;
	}

	// Set default texture if Material is NULL
	if( !InMaterial )
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
				if( ErrorMaterial )
					*ErrorMaterial = History(ErrorIndex);
				if( ErrorMaterial )
					*ErrorString   = FString::Printf(TEXT("Circular material reference in %s"), History(ErrorIndex)->GetName() );
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
		UBOOL UseFallbacks		= !(Viewport->Actor->ShowFlags & SHOW_NoFallbackMaterials);
		UBOOL ShaderHack		= 0;

		// Keep going until we have an renderable material, using fallbacks where necessary.
		for(;;)
		{
			// Check material type, stripping off and processing final modifiers.
			FPixoModifierInfo   ModifierInfo;
			UShader*            Shader				= NULL;
			UBitmapMaterial*    BitmapMaterial		= NULL;
			UConstantMaterial*  ConstantMaterial	= NULL;
			UCombiner*          Combiner			= NULL;
			UParticleMaterial*  ParticleMaterial	= NULL;
			UTerrainMaterial*   TerrainMaterial		= NULL;
			UProjectorMaterial* ProjectorMaterial	= NULL;
			UMaterial*          NonModifier			= NULL;

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
				if( !RenDev->SimpleMaterials )
					break;

				// Use fallback if there is an available modifier fallback point 
				// or if the material has a fallback and is not the default fallback
				// for a shader (==diffuse).
				if( !(	( NonModifier->HasFallback() && !(Shader && !Shader->FallbackMaterial) ) 
						|| ModifierInfo.BestFallbackPoint
					) 
				)
					break;
			}

			// Material is not renderable.  Find a fallback.
			if( ModifierInfo.BestFallbackPoint )
			{
				// Try using a fallback somewhere in the modifier chain.
				ModifierInfo.BestFallbackPoint->UseFallback = 1;
			}
			else if( Shader && !ShaderHack && !Shader->UseFallback && !Shader->FallbackMaterial && Shader->Diffuse && Shader->TwoSided )
			{
				// Hack so material system doesn't forget about 'twosidedness'
				Shader->Opacity					= NULL;
				Shader->Specular				= NULL;
				Shader->SpecularityMask			= NULL;
				Shader->SelfIllumination		= NULL;
				Shader->SelfIlluminationMask	= NULL;

				ShaderHack						= 1;

				if( ErrorMaterial )
					*ErrorMaterial = NULL;
				if( ErrorString )
					*ErrorString = TEXT("");
			}
			else if( NonModifier->HasFallback() )
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

	if( NumPasses )
		*NumPasses = CurrentState->NumMaterialPasses;

	CurrentState->CurrentMaterialState	= NULL;
	CurrentState->ArraysDirty			= 1;
	
	unguard;
}


//
//  FPixoRenderInterface::SetZBias
//
void FPixoRenderInterface::SetZBias(INT ZBias)
{
	guard(FPixoRenderInterface::SetZBias);

	CurrentState->ZBias = ZBias;

	PixoSetPolygonZBias(ZBias);
	unguard;
}


//
//  FPixoRenderInterface::SetStencilOp
//
void FPixoRenderInterface::SetStencilOp(ECompareFunction Test,DWORD Ref,DWORD Mask,EStencilOp FailOp,EStencilOp ZFailOp,EStencilOp PassOp,DWORD WriteMask)
{
	guard(FPixoRenderInterface::SetStencilOp);
	unguard;
}


//
//  FPixoRenderInterface::SetPrecacheMode
//
void FPixoRenderInterface::SetPrecacheMode( EPrecacheMode InPrecacheMode )
{
	guard(FPixoRenderInterface::SetPrecacheMode);
	PrecacheMode = InPrecacheMode;
	unguard;
}


//
//  FPixoRenderInterface::SetVertexStreams
//
INT FPixoRenderInterface::SetVertexStreams(EVertexShader Shader,FVertexStream** Streams,INT NumStreams)
{
	guard(FPixoRenderInterface::SetVertexStreams);

	// Unset any additional old streams.
	for(INT StreamIndex = NumStreams;StreamIndex < CurrentState->NumStreams;StreamIndex++)
		CurrentState->Streams[StreamIndex] = NULL;

	// Build the shader declarations.
	FShaderDeclaration  ShaderDeclaration;

	ShaderDeclaration.NumStreams = NumStreams;

	// Add the vertex stream components to the shader declaration.
	for(INT StreamIndex = 0;StreamIndex < NumStreams;StreamIndex++)
		ShaderDeclaration.Streams[StreamIndex] = FStreamDeclaration(Streams[StreamIndex]);

	// Find or create an appropriate vertex shader.
	FPixoVertexShader* VertexShader = RenDev->GetVertexShader(Shader,ShaderDeclaration);

	// Set the vertex shader.
	CurrentState->VertexShader = VertexShader;

	INT Size = 0;

	// Set the vertex streams.
	for(INT StreamIndex = 0;StreamIndex < NumStreams;StreamIndex++)
	{
		// Cache the vertex stream.
		QWORD               CacheId             = Streams[StreamIndex]->GetCacheId();
		FPixoVertexStream*  PixoVertexStream    = (FPixoVertexStream*) RenDev->GetCachedResource(CacheId);

		if( !PixoVertexStream )
		{
			PixoVertexStream = new(TEXT("FPixoVertexStream")) FPixoVertexStream(RenDev,CacheId,false);
		}

		if( PixoVertexStream->CachedRevision != Streams[StreamIndex]->GetRevision() )
		{
			Size += Streams[StreamIndex]->GetSize();
			PixoVertexStream->Cache(Streams[StreamIndex]);
		}

		PixoVertexStream->LastFrameUsed = RenDev->FrameCounter;

		// Set the vertex stream.
		INT Stride = Streams[StreamIndex]->GetStride();

		CurrentState->Streams[StreamIndex]          = PixoVertexStream;
		CurrentState->StreamStrides[StreamIndex]    = Stride;
	}

	CurrentState->NumStreams    = NumStreams;
	CurrentState->ArraysDirty   = 1;

	return Size;

	unguard;
}


//
//  FPixoRenderInterface::SetDynamicStream
//
INT FPixoRenderInterface::SetDynamicStream(EVertexShader Shader,FVertexStream* Stream)
{
	guard(FPixoRenderInterface::SetDynamicStream);

	// If there isn't a dynamic vertex stream already, allocate one.
	if(!RenDev->DynamicVertexStream)
	{
		//!!TODO
		RenDev->DynamicVertexStream = new FPixoVertexStream(RenDev,NULL,true);
	}

	// Add the vertices in Stream to the dynamic vertex stream.
	INT BaseVertexIndex = RenDev->DynamicVertexStream->AddVertices(Stream),
		Stride = Stream->GetStride();

	// Set the dynamic vertex stream.
	CurrentState->Streams[0]        = RenDev->DynamicVertexStream;
	CurrentState->StreamStrides[0]  = Stride;

	// Unset any additional old streams.
	for(INT StreamIndex = 1; StreamIndex < CurrentState->NumStreams; StreamIndex++)
		CurrentState->Streams[StreamIndex] = NULL;

	CurrentState->NumStreams = 1;

	// Find or create an appropriate vertex shader.
	FShaderDeclaration  ShaderDeclaration;

	ShaderDeclaration.NumStreams = 1;
	ShaderDeclaration.Streams[0] = FStreamDeclaration(Stream);

	// Find or create an appropriate vertex shader.
	FPixoVertexShader *VertexShader =
		RenDev->GetVertexShader(Shader, ShaderDeclaration);

	CurrentState->VertexShader  = VertexShader;
	CurrentState->ArraysDirty   = 1;

	return BaseVertexIndex;

	unguard;
}


//
//  FPixoRenderInterface::SetIndexBuffer
//
INT FPixoRenderInterface::SetIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseVertexIndex)
{
	guard(FPixoRenderInterface::SetIndexBuffer);

	UBOOL RequiresCaching = 0;
	if( IndexBuffer )
	{
		// Cache the index buffer.
		QWORD               CacheId             = IndexBuffer->GetCacheId();
		FPixoIndexBuffer*   PixoIndexBuffer = (FPixoIndexBuffer*) RenDev->GetCachedResource(CacheId);

		if( !PixoIndexBuffer )
		{
			PixoIndexBuffer = new FPixoIndexBuffer(RenDev,CacheId,false);
		}

		if( PixoIndexBuffer->CachedRevision != IndexBuffer->GetRevision() )
		{
			PixoIndexBuffer->Cache(IndexBuffer);
			RequiresCaching |= 1;
		}

		PixoIndexBuffer->LastFrameUsed = RenDev->FrameCounter;

		// Set the index buffer.
		CurrentState->IndexBuffer           = PixoIndexBuffer;
		CurrentState->IndexBufferBase       = BaseVertexIndex;
	}
	else
	{
		// Clear the index buffer.
		if(CurrentState->IndexBuffer != NULL)
		{
			CurrentState->IndexBuffer       = NULL;
			CurrentState->IndexBufferBase   = 0;
		}
	}

	CurrentState->ArraysDirty = 1;

	return RequiresCaching ? IndexBuffer->GetSize() : 0;

	unguard;
}


//
//  FPixoRenderInterface::SetDynamicIndexBuffer
//
INT FPixoRenderInterface::SetDynamicIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseVertexIndex)
{
	guard( FPixoRenderInterface::SetDynamicIndexBuffer);

	check( IndexBuffer->GetIndexSize() == sizeof(_WORD) );

	// If there isn't a dynamic index buffer already, allocate one.
	if( !RenDev->DynamicIndexBuffer )
	{
		RenDev->DynamicIndexBuffer = new FPixoIndexBuffer(RenDev,NULL,true);
	}

	// Add the indices in the index buffer to the dynamic index buffer.
	INT BaseIndex = RenDev->DynamicIndexBuffer->AddIndices(IndexBuffer);

	// Set the dynamic index buffer.
	CurrentState->IndexBuffer       = RenDev->DynamicIndexBuffer;
	CurrentState->IndexBufferBase   = BaseVertexIndex;
	CurrentState->ArraysDirty       = 1;

	return BaseIndex;

	unguard;
}

//
//  FPixoRenderInterface::CommitLights
//
UBOOL FPixoRenderInterface::CommitLights()
{
	guard(FPixoRenderInterface::CommitLights());

	return PixoCommitLights(CurrentState);

	unguard;
}

//
//  FPixoRenderInterface::CommitStreams
//
void FPixoRenderInterface::CommitStreams( INT FirstIndex )
{
	guard(FPixoRenderInterface::CommitStreams());

	UBOOL   NoIndexBuffer   = CurrentState->IndexBuffer == NULL;
	INT     IndexBufferBase = CurrentState->IndexBufferBase;

	UBOOL   HasNormals      = 0,
			HasDiffuse      = 0;

	UINT    IndexStreamDefinition = 0;
	// The definition handed to PixoSetStreamDefinition below gets one entry per component plus one per
	// stream: 16 streams x (MAX_VERTEX_COMPONENTS + 1) at most. Sized to that maximum and check()ed.
	UINT    PixoStreamDefinition[16 * (MAX_VERTEX_COMPONENTS + 1) + 1];
	static UINT PixoStreamDefinitionPeak = 0;

	if( NoIndexBuffer )
		IndexBufferBase += FirstIndex;

	for( INT StreamIndex = 0; StreamIndex < CurrentState->NumStreams; StreamIndex++ )
	{
		INT     VertexStride    = CurrentState->StreamStrides[StreamIndex];
		INT     VertexOffset    = VertexStride * IndexBufferBase;
		BYTE*   VertexBuffer    = (BYTE*) CurrentState->Streams[StreamIndex]->GetVertexData();

		PixoSetStream(StreamIndex, VertexBuffer + VertexOffset);
		PixoStreamDefinition[IndexStreamDefinition++] = PIXO_STREAM_SETSTREAM(StreamIndex);

		INT Offset = 0;
		for( INT ComponentIndex=0;
			ComponentIndex < CurrentState->VertexShader->Declaration.Streams[StreamIndex].NumComponents;
			ComponentIndex++ )
		{
			FVertexComponent    Component       =
				CurrentState->VertexShader->Declaration.Streams[StreamIndex].Components[ComponentIndex];
			INT                 ComponentSize   = 0;

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
			default:
				check(0);
				break;
			}

			switch( Component.Function )
			{
			case FVF_Position:
				if(Component.Type == CT_Float4)
					PixoStreamDefinition[IndexStreamDefinition++] = PIXO_STREAM_POSXYZW;
				else if(Component.Type == CT_Float3)
					PixoStreamDefinition[IndexStreamDefinition++] = PIXO_STREAM_POSXYZ;
				else if(Component.Type == CT_Float2)
					PixoStreamDefinition[IndexStreamDefinition++] = PIXO_STREAM_POSXY;
				else
					check(0);
				break;
			case FVF_Normal:
				PixoStreamDefinition[IndexStreamDefinition++] = CurrentState->UseDynamicLighting ?
					PIXO_STREAM_NORMAL : PIXO_STREAM_SKIP(ComponentSize / 4);
				HasNormals = CurrentState->UseDynamicLighting;
				break;
			case FVF_Specular:
				PixoStreamDefinition[IndexStreamDefinition++] = CurrentState->UseDynamicLighting ? PIXO_STREAM_DIFFUSE : PIXO_STREAM_SPECULAR;
				break;
			case FVF_Diffuse:
				PixoStreamDefinition[IndexStreamDefinition++] = CurrentState->UseStaticLighting ? (CurrentState->UseDynamicLighting ? PIXO_STREAM_SPECULAR : PIXO_STREAM_DIFFUSE) : PIXO_STREAM_SKIP(ComponentSize / 4);
				HasDiffuse = CurrentState->UseStaticLighting;
				break;
			case FVF_TexCoord0:
				if(Component.Type == CT_Float4)
					PixoStreamDefinition[IndexStreamDefinition++] = PIXO_STREAM_TEXSTRQ0;
				else if(Component.Type == CT_Float3)
					PixoStreamDefinition[IndexStreamDefinition++] = PIXO_STREAM_TEXSTR0;
				else if(Component.Type == CT_Float2)
					PixoStreamDefinition[IndexStreamDefinition++] = PIXO_STREAM_TEX0;
				else
					check(0);
				break;
			case FVF_TexCoord1:
				if(Component.Type == CT_Float4)
					PixoStreamDefinition[IndexStreamDefinition++] = PIXO_STREAM_TEXSTRQ1;
				else if(Component.Type == CT_Float3)
					PixoStreamDefinition[IndexStreamDefinition++] = PIXO_STREAM_TEXSTR1;
				else if(Component.Type == CT_Float2)
					PixoStreamDefinition[IndexStreamDefinition++] = PIXO_STREAM_TEX1;
				else
					check(0);
				break;
			default:
				PixoStreamDefinition[IndexStreamDefinition++] = PIXO_STREAM_SKIP(ComponentSize / 4);
				break;
			}

			Offset += ComponentSize;
		}
	}

	check(IndexStreamDefinition < ARRAY_COUNT(PixoStreamDefinition));
	if( IndexStreamDefinition > PixoStreamDefinitionPeak )
	{
		PixoStreamDefinitionPeak = IndexStreamDefinition;
		if( PixoStreamDefinitionPeak >= 32 )
			debugf( NAME_Log, TEXT("PixoDrv: stream definition needs %d entries (the original array held 32)"), PixoStreamDefinitionPeak + 1 );
	}
	PixoStreamDefinition[IndexStreamDefinition] = PIXO_STREAM_END;
	PixoSetStreamDefinition(PixoStreamDefinition);

	PixoSetNormals(HasNormals ? PIXO_NORMALS_ON : PIXO_NORMALS_OFF);

	CurrentState->HasDiffuse = HasDiffuse;

	CurrentState->ArraysDirty = 0;

	unguard;
}

//
//  FPixoRenderInterface::SetTexCoordIndices
//
void FPixoRenderInterface::SetTexCoordIndices(INT Pass)
{
	INT CurrentCoord = 0;
	for(INT StageIndex = 0; StageIndex < CurrentState->MaterialPasses[Pass]->StagesUsed; StageIndex++)
	{
		FPixoMaterialStateStage& Stage = CurrentState->MaterialPasses[Pass]->Stages[StageIndex];

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

		if(CoordIndex != -1)
		{
			PixoSetTexCoordIndex(StageIndex, CoordIndex);
		}

		check(StageIndex < RenDev->NumTextureUnits);
	}
}

//
//  GetPixoPrimitiveType
//
inline PIXO_PRIMITIVETYPES GetPixoPrimitiveType(INT *Count, EPrimitiveType PrimitiveType)
{
	if(PrimitiveType == (EPrimitiveType)PT_QuadList)
	{
		*Count   *= 4;
		return PIXO_QUADLIST;
	}

	switch( PrimitiveType )
	{
	case PT_TriangleList:
		*Count   *= 3;
		return PIXO_TRIANGLELIST;
		break;
	case PT_TriangleStrip:
		*Count   += 2;
		return PIXO_TRIANGLESTRIP;
		break;
	case PT_TriangleFan:
		*Count   += 2;
		return PIXO_TRIANGLEFAN;
		break;
	case PT_PointList:
		return PIXO_POINTLIST;
	case PT_LineList:
		*Count   *= 2;
		return PIXO_LINELIST;
	default:
		*Count   = 0;
		return PIXO_TRIANGLELIST;
	}
}

//
//  FPixoRenderInterface::DrawPrimitive
//
void FPixoRenderInterface::DrawPrimitive(EPrimitiveType PrimitiveType,
	INT FirstIndex, INT NumPrimitives, INT MinIndex, INT MaxIndex)
{
	guard(FPixoRenderInterface::DrawPrimitive);

	UBOOL   NoIndexBuffer   = CurrentState->IndexBuffer == NULL;
	_WORD*  IndexBuffer     = NoIndexBuffer ? NULL : (_WORD*) CurrentState->IndexBuffer->GetIndexData();
	INT     IndexCount      = NumPrimitives;

	PIXO_PRIMITIVETYPES PixoPrimitiveType = GetPixoPrimitiveType(&IndexCount, PrimitiveType);

	GCurrentDrawPrimitiveState = CurrentState;

	if(CurrentState->LocalToWorldDirty)
	{
		CurrentState->LocalToWorldInverse = CurrentState->LocalToWorld.Inverse();
		CurrentState->LocalToWorldDirty = 0;
	}

	UBOOL	DoLighting		= 0,
			UpdatedStreams	= 0;
	
	if( NoIndexBuffer || CurrentState->ArraysDirty || (CurrentState->NumMaterialPasses > 1) )
	{
		CommitStreams( FirstIndex );
		UpdatedStreams = 1;
	}

	// Needs to be after CommitStreams as we need to know whether there is a diffuse stream or not.
	if(CurrentState->LightsDirty)
		DoLighting = CommitLights();

	if( UpdatedStreams && DoLighting )
	{
		// The light callback needs to know where it can find
		// local object positions for this mesh.
		PixoLightUpdateStreamData();
	}

	if(!NoIndexBuffer)
	{
		// Make sure our transform vertex cache is large enough.
		if(!RenDev->AllocAndSetPixoVertexCache(MaxIndex + 1))
		{
			check(0);
			return;
		}

		// Transform the vertices for the PixoIndexedDrawPrimitiveStream
		//  below. This only needs to happen once for all the passes below.
		GStats.DWORDStats(RenDev->STATS_NumVertsXformed) +=
			PixoTransformVertices(0, MinIndex, MaxIndex + 1);
	}

	for( INT Pass = 0; Pass < CurrentState->NumMaterialPasses; Pass++ )
	{
		check( CurrentState->MaterialPasses[Pass]->StagesUsed < 3 );

		UBOOL RestoreFogColor = 0;

		SetTexCoordIndices( Pass );
		SetMaterialBlending( CurrentState->MaterialPasses[Pass] );

		PixoSpecular1up(0xffffffff);
		PixoDiffuse1up(DoLighting ? 0xff000000 : 0xffffffff);

		// Fog hacks needed for translucent objects.
		if( CurrentState->DistanceFogEnabled && CurrentState->CurrentMaterialState->OverrideFogColor )
		{
			FPlane FogColor = CurrentState->CurrentMaterialState->OverriddenFogColor.Plane();
			RestoreFogColor = 1;

			PixoSetFogColor(
				PixoMakeArgb04f(FogColor.W, FogColor.X, FogColor.Y, FogColor.Z));
		}

		if(UPixoRenderDevice::ShowDepthComplexity && !(PixoHint & PIXOHINT_HUD))
			RenDev->SetUpPixoBlendModeToShowComplexity();

		GStats.DWORDStats(RenDev->STATS_NumTrianglesSubmitted) += NumPrimitives;
		if( NoIndexBuffer )
		{
			PixoDrawPrimitiveStream( PixoPrimitiveType, IndexCount );
		}
		else
		{
			PixoIndexedDrawPrimitiveStream(
				PixoPrimitiveType, IndexBuffer + FirstIndex, IndexCount );
		}

		if( RestoreFogColor )
		{
			FPlane FogColor = CurrentState->DistanceFogColor.Plane();

			PixoSetFogColor(
				PixoMakeArgb04f(FogColor.W, FogColor.X, FogColor.Y, FogColor.Z));
		}
	}

	GCurrentDrawPrimitiveState = NULL;

	unguard;
}


//
//  FPixoRenderInterface::Locked
//
void FPixoRenderInterface::Locked( UViewport* InViewport, BYTE* InHitData,INT* InHitSize)
{
	guard(FPixoRenderInterface::Locked);

	Viewport = InViewport;

	CurrentState->ViewportX             = 0;
	CurrentState->ViewportY             = 0;
	CurrentState->ViewportWidth         = Viewport->SizeX;
	CurrentState->ViewportHeight        = Viewport->SizeY;
	CurrentState->OtherFrameBuffer		= NULL;
	CurrentState->OtherFrameBufferPitch	= 0;
	CurrentState->OtherZBuffer			= NULL;
	CurrentState->OtherZBufferPitch		= 0;

	// Store the render state.
	SavedStateIndex = 0;
	PushState();

	unguard;
}


//
//  FPixoRenderInterface::Unlocked
//
void FPixoRenderInterface::Unlocked()
{
	guard(FPixoRenderInterface::Unlocked);

	Viewport = NULL;
	PixoResetHint( PIXOHINT_All );

	// Restore initial state.
	PopState();
	check(SavedStateIndex == 0);

	unguard;
}

static PIXO_TEX_FILTER PixoFilterQuality[4] = 
{
	PIXO_TEX_FILTER_POINT,
	PIXO_TEX_FILTER_FAST_TWO_POINT,
	PIXO_TEX_FILTER_FAST_FOUR_POINT,
	PIXO_TEX_FILTER_BILINEAR
};

//
//	FPixoRenderInterface::PixoSetHint
//
void FPixoRenderInterface::PixoSetHint(DWORD InPixoHint)
{
	guard(FPixoRenderInterface::PixoSetHint);
	
	if( (InPixoHint & PIXOHINT_HUD) && !(PixoHint & PIXOHINT_HUD) )
	{
		DWORD FilterQualityHUD = Clamp<DWORD>( RenDev->FilterQualityHUD, 0, 3 );
		PixoSetWStateTextureFilter( 0, PixoFilterQuality[FilterQualityHUD] );
		PixoSetWStateTextureFilter( 1, PixoFilterQuality[FilterQualityHUD] );

		if( RenDev->Zoom2X )
		{
			// Code assumes HUD not being rendered to texture.
			check( CurrentState->OtherFrameBuffer == NULL );

			PixoBufferUnlock(RenDev->PixoBuffer);
			PixoBufferLock(RenDev->PixoBuffer, PIXO_BUF_LOCK_COMPOSITE);
			PixoSetFrameBuffer(RenDev->PixoBuffer->Buffer, RenDev->PixoBuffer->BufferPitch);
			PixoSetZBuffer( RenDev->ZBuffer, 2 * RenDev->ZBufferPitch, PixoGetZBufferType());
			
			PixoHint |= PIXOHINT_HUD; // SetViewport relies on hint already set.
			SetViewport( CurrentState->ViewportX, CurrentState->ViewportY, CurrentState->ViewportWidth, CurrentState->ViewportHeight );
		}
	}

	if( (InPixoHint & PIXOHINT_Skybox) && !(PixoHint & PIXOHINT_Skybox) )
	{
		PixoSetDepthRange( 0.95f, 1.f );
	}

	if( (InPixoHint & PIXOHINT_Bilinear) && !(PixoHint & PIXOHINT_Bilinear) )
	{
		PixoSetWStateTextureFilter( 0, PIXO_TEX_FILTER_BILINEAR );
		PixoSetWStateTextureFilter( 1, PIXO_TEX_FILTER_BILINEAR );
	}

	PixoHint |= InPixoHint;

	unguard;
}

//
//	FPixoRenderInterface::PixoSetHint
//
void FPixoRenderInterface::PixoResetHint(DWORD InPixoHint)
{
	guard(FPixoRenderInterface::PixoResetHint);
	
	if( (InPixoHint & PIXOHINT_HUD) && (PixoHint & PIXOHINT_HUD) )
	{
		DWORD FilterQuality3D  = Clamp<DWORD>( RenDev->FilterQuality3D , 0, 3 );
		PixoSetWStateTextureFilter( 0, PixoFilterQuality[FilterQuality3D] );
		PixoSetWStateTextureFilter( 1, PixoFilterQuality[FilterQuality3D] );
	}

	if( (InPixoHint & PIXOHINT_Skybox) && (PixoHint & PIXOHINT_Skybox) )
	{
		if( RenDev->SkyboxHack )
			PixoSetDepthRange( 0.0f, 0.95f );
		else
			PixoSetDepthRange( 0.0f, 1.0f );
	}

	if( (InPixoHint & PIXOHINT_Bilinear) && (PixoHint & PIXOHINT_Bilinear) )
	{
		if( PixoHint & PIXOHINT_HUD )
		{
			DWORD FilterQualityHUD = Clamp<DWORD>( RenDev->FilterQualityHUD, 0, 3 );
			PixoSetWStateTextureFilter( 0, PixoFilterQuality[FilterQualityHUD] );
			PixoSetWStateTextureFilter( 1, PixoFilterQuality[FilterQualityHUD] );
		}
		else
		{
			DWORD FilterQuality3D  = Clamp<DWORD>( RenDev->FilterQuality3D , 0, 3 );
			PixoSetWStateTextureFilter( 0, PixoFilterQuality[FilterQuality3D] );
			PixoSetWStateTextureFilter( 1, PixoFilterQuality[FilterQuality3D] );
		}
	}

	PixoHint &= ~InPixoHint;

	unguard;
}

//
//	FPixoRenderInterface::PixoCreateTexture
//
UTexture* FPixoRenderInterface::PixoCreateTexture( FRenderTarget* RenderTarget, UBOOL CreateMips )
{
	guard(FPixoRenderInterface::PixoCreateTexture);
	
	QWORD			CacheId		= RenderTarget->GetCacheId();
	FPixoTexture*	PixoTexture = (FPixoTexture*) RenDev->GetCachedResource(CacheId);
	
	if( !PixoTexture )
		return NULL;

	UTexture* Texture;
	Texture = CastChecked<UTexture>(RenDev->StaticConstructObject(UTexture::StaticClass(),RenDev->GetOuter(),NAME_None,RF_Public|RF_Standalone));
	Texture->Format = TEXF_RGBA8;
	Texture->LODSet = LODSET_None;
	Texture->Init( PixoTexture->CachedWidth, PixoTexture->CachedHeight );
	Texture->PostLoad();
	appMemcpy( &Texture->Mips(0).DataArray(0), PixoTexture->FrameBuffer, PixoTexture->CachedWidth * PixoTexture->CachedHeight * 4 );

	//appCreateBitmap( *FString::Printf(TEXT("Terrain-%i"),CacheId) , PixoTexture->CachedWidth, PixoTexture->CachedHeight, (DWORD*) PixoTexture->FrameBuffer );

	if( CreateMips )
		Texture->CreateMips( 1, 1 );

	return Texture;

	unguard;
}


//
//  DE's additions
//

//
//  FPixoRenderInterface::LockDynBuffer
//
INT FPixoRenderInterface::LockDynBuffer(BYTE** pOutBuffer, int numVerts, int stride, DWORD componentFlags)
{
	guard( FPixoRenderInterface::LockDynBuffer );
	if ( numVerts == 0 )
	{
		(*pOutBuffer) = NULL;
		return 0;
	}

	RenDev->DE_DynamicVertexStream.Init( pOutBuffer, numVerts, stride, componentFlags );
	return numVerts;
	unguard;
}

//
//  FPixoRenderInterface::UnlockDynBuffer
//
INT FPixoRenderInterface::UnlockDynBuffer( void )
{
	INT first = SetDynamicStream(VS_FixedFunction,&RenDev->DE_DynamicVertexStream);
	return first;
}

//
//  FPixoRenderInterface::DrawDynQuads
//
void FPixoRenderInterface::DrawDynQuads(INT NumPrimitives)
{
	guard(FPixoRenderInterface::DrawDynQuads);

	INT first = UnlockDynBuffer();

	if( !NumPrimitives )
		return;

	SetIndexBuffer( NULL, 0 );
	DrawPrimitive((EPrimitiveType)PT_QuadList,first,NumPrimitives,0,0);

	unguard;
}

//
//  FPixoRenderInterface::DrawQuads
//
void FPixoRenderInterface::DrawQuads(INT FirstVertex, INT NumPrimitives)
{
	guard(FPixoRenderInterface::DrawQuads);

	if( !NumPrimitives )
		return;

	SetIndexBuffer( NULL, 0 );
	DrawPrimitive((EPrimitiveType)PT_QuadList,FirstVertex,NumPrimitives,0,0);

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

