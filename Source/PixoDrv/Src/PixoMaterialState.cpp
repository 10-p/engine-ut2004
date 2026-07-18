/*=============================================================================
	PixoMaterialState.cpp: Unreal Pixo support.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Michael Sartain from GL driver
		* Taken over by Daniel Vogel

=============================================================================*/

#include "PixoDrv.h"

//
// FPixoModifierInfo::FPixoModifierInfo
//
FPixoModifierInfo::FPixoModifierInfo()
{
	ModifyTextureTransforms     = 0;
	ModifyFramebufferBlending   = 0;
	ModifyColor                 = 0;
	ModifyOpacity               = 0;

	Matrix                      = FMatrix::Identity;
	TexCoordSource              = TCS_Stream0;
	TexCoordCount               = TCN_2DCoords;
	TexCoordProjected           = 0;

	FrameBufferBlending         = FB_Overwrite;
	ZWrite                      = 1;
	ZTest                       = 1;
	AlphaTest                   = 0;
	TwoSided                    = 0;
	AlphaRef                    = 0;

	TFactorColor                = 0;
	AlphaBlend                  = 0;

	BestFallbackPoint           = NULL;

	Opacity                     = NULL;
	OpacityOverrideTexModifier  = 0;
}


//
// FPixoModifierInfo::SetDetailTextureScale - apply a texture scaling transform for a detail texture.
//
void FPixoModifierInfo::SetDetailTextureScale( FLOAT Scale )
{
	ModifyTextureTransforms = 1;
	Matrix *= FMatrix (
					FPlane(Scale,0,0,0),
					FPlane(0,Scale,0,0),
					FPlane(0,0,1,0),
					FPlane(0,0,0,1)
				);
}


//
// FPixoRenderInterface::FPixoSavedState::FPixoSavedState
//
FPixoRenderInterface::FPixoSavedState::FPixoSavedState()
{
	OtherFrameBuffer	= NULL;
	OtherZBuffer		= NULL;
	OtherFrameBufferPitch= 0;
	OtherZBufferPitch	= 0;

	ViewportX			= 0;
	ViewportY			= 0;
	ViewportWidth		= 0;
	ViewportHeight		= 0;

	ZBias				= 0;

	LocalToWorld		= FMatrix::Identity;
	WorldToCamera		= FMatrix::Identity;
	CameraToScreen		= FMatrix::Identity;

	VertexShader        = NULL;
	appMemzero( Streams         , sizeof(Streams)       );
	appMemzero( StreamStrides   , sizeof(StreamStrides) );

	NumStreams          = 0;

	IndexBuffer         = NULL;
	IndexBufferBase     = 0;

	CullMode            = CM_None;

	UseDetailTexturing  = 0;
	UseDynamicLighting  = 0;
	UseStaticLighting   = 1;
	LightingModulate2X  = 0;
	LightingOnly        = 0;
	Lightmap            = NULL;
	AmbientLightColor   = FColor(0,0,0,0);

	appMemzero( Lights, sizeof(Lights) );
	LightsDirty         = 0;

	ArraysDirty         = 0;

	HasDiffuse          = 1;

	DistanceFogEnabled  = 0;
	DistanceFogColor    = FColor(0,0,0,0);
	DistanceFogStart    = 0;
	DistanceFogEnd      = 0;

	ZWrite              = 1;
	ZTest               = 1;
	AlphaTest           = 0;
	AlphaRef            = 0;

	appMemzero( MaterialPasses, sizeof (MaterialPasses) );
	NumMaterialPasses   = 0;
	CurrentMaterialState= NULL;
}


//
// FPixoRenderInterface::CacheTexture
//
FPixoTexture* FPixoRenderInterface::CacheTexture(FBaseTexture* Texture)
{
	guard(FPixoRenderInterface::CacheTexture);

	if( !Texture )
		return 0;

	// Cache the texture
	QWORD           CacheId         = Texture->GetCacheId();
	FPixoTexture*   PixoTexture = (FPixoTexture*) RenDev->GetCachedResource(CacheId);

	if(!PixoTexture)
		PixoTexture = new(TEXT("FPixoTexture")) FPixoTexture(RenDev,CacheId);

	if( PixoTexture->CachedRevision != Texture->GetRevision() )
		PixoTexture->Cache(Texture);

	PixoTexture->LastFrameUsed = RenDev->FrameCounter;

	return PixoTexture;

	unguard;
}


//
// FPixoMaterialStateStage::FPixoMaterialStateStage
//
FPixoMaterialStateStage::FPixoMaterialStateStage()
{
	appMemcpy( &TextureTransformMatrix, &FMatrix::Identity, sizeof(FMatrix));

	TextureTransformsEnabled    = 0;
	TexGenMode                  = TCS_NoChange;
	TexGenProjected             = 0;
	UseTexGenMatrix             = 0;
	TexCoordIndex               = TCS_NoChange;
	TexCoordCount               = TCN_2DCoords;
	TextureAddressU             = GL_CLAMP_TO_EDGE;
	TextureAddressV             = GL_CLAMP_TO_EDGE;
	TextureAddressW             = GL_CLAMP_TO_EDGE;
	TextureMipLODBias           = DefaultMipBias; // sjs
	Texture                     = NULL;

	SourceRgb2   = GL_PREVIOUS;
	SourceRgb0   = GL_PREVIOUS;
	SourceRgb1   = GL_PREVIOUS;
	ColorOp     = GL_REPLACE;

	SourceAlpha2   = GL_PREVIOUS;
	SourceAlpha0   = GL_PREVIOUS;
	SourceAlpha1   = GL_PREVIOUS;
	AlphaOp     = GL_REPLACE;

	OperandRgb2   = GL_SRC_COLOR;
	OperandRgb0   = GL_SRC_COLOR;

	OperandAlpha2   = GL_SRC_ALPHA;
	OperandAlpha0   = GL_SRC_ALPHA;

	ColorScale  = 1.0f;
	AlphaScale  = 1.0f;
}

//
// FPixoMaterialState::FPixoMaterialState
//
FPixoMaterialState::FPixoMaterialState()
{
	//PixelShader           = PS_None;
	AlphaBlending       = 0;
	AlphaTest           = 0;
	AlphaRef            = 0;
	ZTest               = 1;
	ZWrite              = 1;
	TwoSided            = 0;
	FillMode            = FM_Solid;
	SrcBlend            = GL_ONE;
	DestBlend           = GL_ZERO;
	TFactorColor        = 0;
	StagesUsed          = 0;
	OverrideFogColor    = 0;
	PatchLighting		= 1;
}

/*----------------------------------------------------------------------------
	SetMaterialShader.
----------------------------------------------------------------------------*/

#define NEWPASS()                                                           \
		CurrentState->MaterialPasses[PassesUsed]->StagesUsed = StagesUsed;                  \
		PassesUsed++;                                                       \
		StagesUsed = 0;                                                     \
		CurrentState->MaterialPasses[PassesUsed] = MaterialStatePool.AllocateState(&DefaultPass); \
		CurrentState->MaterialPasses[PassesUsed]->ZWrite = 0;

//
//
//

inline void FPixoRenderInterface::HandleDetail( UBitmapMaterial* DetailBitmap, INT& PassesUsed, INT& StagesUsed, FPixoModifierInfo InModifierInfo, UBOOL SinglePassOnly )
{
	INT MaxTextureStages = RenDev->NumTextureUnits;
	if( StagesUsed >= MaxTextureStages )
	{
		// No room for detail texture in single pass, start a new render pass if possible
		if( (!InModifierInfo.ModifyFramebufferBlending || InModifierInfo.FrameBufferBlending == FB_Overwrite) && !SinglePassOnly )
		{
			NEWPASS();
			FPixoMaterialStateStage& Stage = CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed];
			SetShaderBitmap( Stage, DetailBitmap );
			Stage.SourceRgb0 = GL_TEXTURE;
			Stage.SourceAlpha0 = GL_PRIMARY_COLOR;
			Stage.ColorOp   = GL_REPLACE;
			Stage.AlphaOp   = GL_REPLACE;
			Stage.TextureMipLODBias = RenDev->DetailTexMipBias; // sjs
			ApplyTexModifier( Stage, &InModifierInfo );
			StagesUsed++;

			// modulate with framebuffer.
			CurrentState->MaterialPasses[PassesUsed]->SrcBlend          = GL_DST_COLOR;
			CurrentState->MaterialPasses[PassesUsed]->DestBlend         = GL_SRC_COLOR;
			CurrentState->MaterialPasses[PassesUsed]->AlphaBlending     = 1;
			CurrentState->MaterialPasses[PassesUsed]->ZWrite            = 0;
			CurrentState->MaterialPasses[PassesUsed]->ZTest             = 1; // sjs - need to ztest
			CurrentState->MaterialPasses[PassesUsed]->OverrideFogColor  = 1;
			CurrentState->MaterialPasses[PassesUsed]->OverriddenFogColor= FColor( 127, 127, 127, 0 );
			CurrentState->MaterialPasses[PassesUsed]->TwoSided          = InModifierInfo.TwoSided;
		}
	}
	else
	{
		// Add detail texture as a single pass
		FPixoMaterialStateStage& Stage = CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed];
		SetShaderBitmap( Stage, DetailBitmap );
		Stage.SourceRgb0 = GL_TEXTURE;
		Stage.SourceRgb1 = GL_PREVIOUS;
		Stage.SourceAlpha0 = GL_PREVIOUS;
		Stage.ColorOp   = GL_MODULATE;
		Stage.ColorScale= 2.0f;
		Stage.AlphaOp   = GL_REPLACE;
		Stage.TextureMipLODBias = RenDev->DetailTexMipBias; // sjs
		ApplyTexModifier( Stage, &InModifierInfo );
		StagesUsed++;
	}
}

//
// Handle lightmap in single pass
//
inline void FPixoRenderInterface::HandleLightmap_SP( FPixoMaterialStateStage& Stage, FPixoTexture* Lightmap )
{
	Stage.SourceAlpha0              = GL_PREVIOUS;
	Stage.SourceRgb0                = GL_TEXTURE;
	Stage.SourceRgb1                = GL_PREVIOUS;
	Stage.AlphaOp                   = GL_REPLACE;
	Stage.ColorOp                   = GL_MODULATE;
	Stage.ColorScale                = CurrentState->LightingModulate2X ? 2.0f : 1.0f;
	Stage.Texture                   = Lightmap;
	Stage.TextureAddressU           = GL_CLAMP_TO_EDGE;
	Stage.TextureAddressV           = GL_CLAMP_TO_EDGE;
	Stage.TexCoordIndex             = TCS_Stream1;
	Stage.TextureTransformsEnabled  = 0;
}

//
// Patch in the diffuse lighting into the first texture stage
//
inline void FPixoRenderInterface::HandleDiffuse_Patch( FPixoMaterialStateStage& Stage, UBOOL Modulate2X )
{
	Stage.SourceRgb1 = GL_PRIMARY_COLOR;
	Stage.SourceAlpha1 = GL_PRIMARY_COLOR;
	Stage.ColorOp   = GL_MODULATE;
	Stage.AlphaOp   = GL_MODULATE;
	Stage.ColorScale= CurrentState->LightingModulate2X ? 2.0f : 1.0f;
}

inline void FPixoRenderInterface::HandleDiffuse_SP( FPixoMaterialStateStage& Stage )
{
	Stage.SourceRgb0 = GL_PRIMARY_COLOR;
	Stage.SourceAlpha0 = GL_PRIMARY_COLOR;
	Stage.ColorScale = CurrentState->LightingModulate2X ? 2.0f : 1.0f;
	Stage.ColorOp	= GL_REPLACE;
	Stage.AlphaOp   = GL_REPLACE;
}

inline void FPixoRenderInterface::HandleDiffuse_Stage( FPixoMaterialStateStage& Stage, UBOOL Modulate2X )
{
	Stage.SourceRgb0 = GL_PRIMARY_COLOR;
	Stage.SourceRgb1 = GL_PREVIOUS;
	Stage.ColorOp   = GL_MODULATE;
	Stage.ColorScale= CurrentState->LightingModulate2X ? 2.0f : 1.0f;
	Stage.SourceAlpha0 = GL_PREVIOUS;
	Stage.AlphaOp   = GL_REPLACE;
}

//
// Blend using TFactorColor
//
inline void FPixoRenderInterface::HandleTFactor_SP( FPixoMaterialStateStage& Stage )
{
	Stage.SourceAlpha0 = GL_PREVIOUS;
	Stage.SourceAlpha1 = GL_CONSTANT;
	Stage.SourceRgb0 = GL_PREVIOUS;
	Stage.SourceRgb1 = GL_CONSTANT;
	Stage.AlphaOp   = GL_MODULATE;
	Stage.ColorOp   = GL_MODULATE;
	Stage.ColorScale= CurrentState->LightingModulate2X ? 2.0f : 1.0f;
	Stage.Texture   = NULL;
}

inline void FPixoRenderInterface::HandleTFactor_Patch( FPixoMaterialStateStage& Stage )
{
	Stage.SourceAlpha1 = GL_CONSTANT;
	Stage.SourceRgb1 = GL_CONSTANT;
	Stage.AlphaOp   = GL_MODULATE;
	Stage.ColorOp   = GL_MODULATE;
	Stage.ColorScale= CurrentState->LightingModulate2X ? 2.0f : 1.0f;
}

//
// Handle lightmap is it own pass
//
inline void FPixoRenderInterface::HandleLighting_MP( FPixoMaterialStateStage& Stage, FPixoTexture* Lightmap, UBOOL UseDiffuse )
{
	Stage.SourceAlpha0 = GL_PRIMARY_COLOR;
	Stage.SourceRgb0 = GL_TEXTURE;
	Stage.SourceRgb1 = GL_PRIMARY_COLOR;
	Stage.AlphaOp   = GL_REPLACE;

	if( !Lightmap )
	{
		Stage.SourceRgb0 = GL_PRIMARY_COLOR;
		Stage.ColorOp   = GL_REPLACE;
		Stage.ColorScale = CurrentState->LightingModulate2X ? 8.0f : 4.0f;
	}
	else
	if( !UseDiffuse )
	{
		Stage.ColorOp = GL_REPLACE;
	}
	else
	{
		Stage.ColorOp    = GL_MODULATE;
		Stage.ColorScale = CurrentState->LightingModulate2X ? 2.0f : 1.0f;
	}

	Stage.Texture           = Lightmap;
	Stage.TextureAddressU   = GL_REPEAT;
	Stage.TextureAddressV   = GL_REPEAT;

	Stage.TexCoordIndex     = TCS_Stream1;
	Stage.TextureTransformsEnabled = 0;
}

inline void FPixoRenderInterface::SetShaderBitmap( FPixoMaterialStateStage& Stage, UBitmapMaterial* BitmapMaterial )
{
	if( BitmapMaterial )
	{
		Stage.Texture = CacheTexture( BitmapMaterial->Get(Viewport->CurrentTime,Viewport)->GetRenderInterface() );
		{
			switch( BitmapMaterial->UClampMode )
			{
			case TC_Wrap:   Stage.TextureAddressU = GL_REPEAT; break;
			case TC_Clamp:  Stage.TextureAddressU = GL_CLAMP_TO_EDGE;  break;
			}
			switch( BitmapMaterial->VClampMode )
			{
			case TC_Wrap:   Stage.TextureAddressV = GL_REPEAT; break;
			case TC_Clamp:  Stage.TextureAddressV = GL_CLAMP_TO_EDGE;  break;
			}
		}
	}
	else
		Stage.Texture = NULL;
}

//
// ApplyFinalBlend
//
inline void FPixoRenderInterface::ApplyFinalBlend( FPixoModifierInfo* InModifierInfo )
{
	guard(FPixoRenderInterface::ApplyFinalBlend);

	FColor  FogColor = FColor(0,0,0,1);

	switch( InModifierInfo->FrameBufferBlending )
	{
	case FB_Overwrite :
		CurrentState->MaterialPasses[0]->SrcBlend           = GL_ONE;
		CurrentState->MaterialPasses[0]->DestBlend          = GL_ZERO;
		CurrentState->MaterialPasses[0]->AlphaBlending      = 0;
		break;
	case FB_AlphaBlend:
		CurrentState->MaterialPasses[0]->SrcBlend           = GL_SRC_ALPHA;
		CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE_MINUS_SRC_ALPHA;
		CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
		break;
	case FB_AlphaModulate_MightNotFogCorrectly:
		CurrentState->MaterialPasses[0]->SrcBlend           = GL_ONE;
		CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE_MINUS_SRC_ALPHA;
		CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
		CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
		CurrentState->MaterialPasses[0]->OverriddenFogColor= FColor( 0, 0, 0, 0 );
		break;
	case FB_Modulate:
		CurrentState->MaterialPasses[0]->SrcBlend           = GL_DST_COLOR;
		CurrentState->MaterialPasses[0]->DestBlend          = GL_SRC_COLOR;
		CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
		CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
		CurrentState->MaterialPasses[0]->OverriddenFogColor= FColor( 127, 127, 127, 0 );
		break;
	case FB_Translucent:
		CurrentState->MaterialPasses[0]->SrcBlend           = GL_ONE;
		CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE_MINUS_SRC_COLOR;   // MERGE ALERT
		CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
		CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
		CurrentState->MaterialPasses[0]->OverriddenFogColor= FColor( 0, 0, 0, 0 );
		break;
	case FB_Darken:
		CurrentState->MaterialPasses[0]->SrcBlend           = GL_ZERO;
		CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE_MINUS_SRC_COLOR;
		CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
		CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
		CurrentState->MaterialPasses[0]->OverriddenFogColor= FColor( 0, 0, 0, 0 );
		break;
	case FB_Brighten:
		CurrentState->MaterialPasses[0]->SrcBlend           = GL_SRC_ALPHA; // MERGE ALERT
		CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE;
		CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
		CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
		CurrentState->MaterialPasses[0]->OverriddenFogColor= FColor( 0, 0, 0, 0 );
		break;
	case FB_Invisible:
		CurrentState->MaterialPasses[0]->SrcBlend           = GL_ZERO;
		CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE;
		CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
		CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
		CurrentState->MaterialPasses[0]->OverriddenFogColor= FColor( 0, 0, 0, 0 );
		break;
	}

	CurrentState->MaterialPasses[0]->ZWrite     = InModifierInfo->ZWrite;
	CurrentState->MaterialPasses[0]->ZTest      = InModifierInfo->ZTest;
	CurrentState->MaterialPasses[0]->TwoSided   = InModifierInfo->TwoSided;
	CurrentState->MaterialPasses[0]->AlphaTest  = InModifierInfo->AlphaTest;
	CurrentState->MaterialPasses[0]->AlphaRef   = InModifierInfo->AlphaRef;

	unguard;
}

//
// ApplyTexModifier
//
inline void FPixoRenderInterface::ApplyTexModifier( FPixoMaterialStateStage& Stage, FPixoModifierInfo* ModifierInfo )
{
	guard(FPixoRenderInterface::ApplyTexModifier);

	if( ModifierInfo->ModifyTextureTransforms )
	{
		// Apply texture coordinate transforms
		Stage.TexCoordCount = ModifierInfo->TexCoordCount;
		if( ModifierInfo->TexCoordProjected )
			Stage.TexGenProjected = 1;

		switch( ModifierInfo->TexCoordSource )
		{
		case TCS_NoChange:
			Stage.TexCoordIndex = TCS_NoChange;
			Stage.TextureTransformsEnabled = 0;
			break;
		case TCS_Stream0:
		case TCS_Stream1:
		case TCS_Stream2:
		case TCS_Stream3:
		case TCS_Stream4:
		case TCS_Stream5:
		case TCS_Stream6:
		case TCS_Stream7:
			{
				Stage.TexCoordIndex = ModifierInfo->TexCoordSource;
				if( ModifierInfo->TexCoordProjected || ModifierInfo->Matrix != FMatrix::Identity )
				{
					Stage.TextureTransformMatrix    = ModifierInfo->Matrix;
					Stage.TextureTransformsEnabled  = 1;
				}
				else
					Stage.TextureTransformsEnabled  = 0;
			}
			break;
		case TCS_WorldCoords:
		case TCS_CameraCoords:
		case TCS_WorldEnvMapCoords:
		case TCS_CameraEnvMapCoords:
			{
				Stage.TexGenMode = ModifierInfo->TexCoordSource;
				if( ModifierInfo->Matrix != FMatrix::Identity )
				{
					Stage.UseTexGenMatrix           = 1;
					Stage.TextureTransformMatrix    = ModifierInfo->Matrix;
				}
			}
			break;
		default:
			appErrorf( TEXT("Unknown TexCoordSource %d"), ModifierInfo->TexCoordSource );
		}
	}

	unguard;
}

//
// Handle opacity bitmap
//
inline void FPixoRenderInterface::HandleOpacityBitmap( FPixoMaterialStateStage& Stage, UBitmapMaterial* Bitmap, UBOOL ModulateAlpha )
{
	SetShaderBitmap( Stage, Bitmap );
	Stage.SourceAlpha0 = GL_TEXTURE;
	Stage.SourceAlpha1 = GL_PREVIOUS;
	Stage.AlphaOp   = ModulateAlpha ? GL_MODULATE : GL_REPLACE;
	Stage.SourceRgb0 = GL_PREVIOUS;
	Stage.ColorOp   = GL_REPLACE;
}

//
// Handle vertex opacity
//
inline void FPixoRenderInterface::HandleVertexOpacity( FPixoMaterialStateStage& Stage, UVertexColor* VertexColor )
{
	Stage.SourceAlpha0 = GL_PRIMARY_COLOR;
	Stage.AlphaOp   = GL_REPLACE;
	Stage.SourceRgb0 = GL_PREVIOUS;
	Stage.ColorOp   = GL_REPLACE;
}

//
// Handle self-illumination in a single pass
//
inline void FPixoRenderInterface::HandleSelfIllumination_SP( FPixoMaterialStateStage& Stage, UBitmapMaterial* Bitmap )
{
	SetShaderBitmap( Stage, Bitmap );
	Stage.SourceAlpha0 = GL_PREVIOUS;
	Stage.AlphaOp   = GL_REPLACE;
	Stage.SourceRgb0 = GL_TEXTURE;
	Stage.SourceRgb1 = GL_PREVIOUS;
	Stage.SourceRgb2 = GL_CONSTANT;
	Stage.ColorOp   = GL_INTERPOLATE;
}

//
// Handle specular in a single pass
//
inline void FPixoRenderInterface::HandleSpecular_SP( FPixoMaterialStateStage& Stage, UBitmapMaterial* Bitmap, UBOOL UseSpecularity, UBOOL UseConstantSpecularity, UBOOL ModulateSpecular2X )
{
	SetShaderBitmap( Stage, Bitmap );

	Stage.SourceAlpha0 = GL_PREVIOUS;
	Stage.AlphaOp   = GL_REPLACE;

	if( !UseSpecularity )
	{
		Stage.SourceRgb0 = GL_PREVIOUS;
		Stage.SourceRgb1 = GL_TEXTURE;
		Stage.ColorOp   = ModulateSpecular2X ? GL_MODULATE : GL_ADD;
		Stage.ColorScale= ModulateSpecular2X ? 2.0f : 1.0f;
	}
	else if( UseConstantSpecularity )
	{
		Stage.SourceRgb0 = GL_CONSTANT;
		Stage.SourceRgb1 = GL_TEXTURE;
		Stage.SourceRgb2 = GL_PREVIOUS;
		if( ModulateSpecular2X )
		{
			Stage.ColorOp       = GL_MODULATE;
			Stage.ColorScale    = 2.0f;
		}
	}
	else
	{
		Stage.SourceRgb0 = GL_PREVIOUS;
		Stage.SourceRgb1 = GL_TEXTURE;
		if( ModulateSpecular2X )
		{
			Stage.ColorOp       = GL_MODULATE;
			Stage.ColorScale    = 2.0f;
		}
	}
}

#define SET_ERROR_STRING_MATERIAL(_ErrString, _ErrMaterial)                 \
	do {                                                                    \
		if(ErrorString)                                                     \
			*ErrorString = (_ErrString);                                    \
		if(ErrorMaterial)                                                   \
			*ErrorMaterial = (_ErrMaterial);                                \
	} while (0)

//
// Handle complex material
//
inline UBOOL FPixoRenderInterface::HandleCombinedMaterial( UMaterial* InMaterial, INT& PassesUsed, INT& StagesUsed, FPixoModifierInfo ModifierInfo, UBOOL InvertOutputAlpha, FString* ErrorString, UMaterial** ErrorMaterial )
{
	guard(FPixoRenderInterface::HandleCombinedMaterial);

	UVertexColor*       VertexColor;
	UConstantMaterial*  ConstantMaterial;
	UBitmapMaterial*    BitmapMaterial;
	UCombiner*          Combiner;

	INT MaxTextureStages = RenDev->NumTextureUnits;

	if( !InMaterial )
	{
		return 1;
	}
	else if( (VertexColor=CheckMaterial<UVertexColor,MT_VertexColor>(this, InMaterial, &ModifierInfo)) != NULL )
	{
//!!TODO: use texture_env_combine3/4
		if( StagesUsed >= MaxTextureStages )
		{
			SET_ERROR_STRING_MATERIAL(TEXT("No stages left for vertex color"), InMaterial);
			return 0;
		}
		if( StagesUsed
		&&  CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed-1].ColorOp == GL_REPLACE
		)
		{
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed-1].SourceRgb1= GL_PRIMARY_COLOR;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed-1].ColorOp  = GL_MODULATE;
		}
		else
		{
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0  = GL_PREVIOUS;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp    = GL_REPLACE;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0  = GL_PRIMARY_COLOR;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp    = GL_REPLACE;
			StagesUsed++;
		}
		return 1;
	}
	else if( (ConstantMaterial=CheckMaterial<UConstantMaterial,MT_ConstantMaterial>(this, InMaterial, &ModifierInfo)) != NULL )
	{
		FColor  Color = ConstantMaterial->GetColor(Viewport->Actor->Level->TimeSeconds);

		if( ((DWORD)(CurrentState->MaterialPasses[PassesUsed]->TFactorColor)) == 0 || CurrentState->MaterialPasses[PassesUsed]->TFactorColor == Color )
		{
			if( StagesUsed >= MaxTextureStages )
			{
				SET_ERROR_STRING_MATERIAL(TEXT("No stages left for constant color"), InMaterial);
				return 0;
			}
			CurrentState->MaterialPasses[PassesUsed]->TFactorColor = Color.DWColor();

			if( StagesUsed
			&&
			(   CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed-1].ColorOp == GL_REPLACE
			||  CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed-1].AlphaOp == GL_REPLACE
			)
			)
			{
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed-1].SourceAlpha1= GL_CONSTANT;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed-1].AlphaOp  = GL_MODULATE;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed-1].SourceRgb1= GL_CONSTANT;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed-1].ColorOp  = GL_MODULATE;
			}
			else
			{
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0  = GL_CONSTANT;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp    = GL_REPLACE;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0  = GL_CONSTANT;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp    = GL_REPLACE;
				StagesUsed++;
			}
			return 1;
		}
		else
		{
			SET_ERROR_STRING_MATERIAL(TEXT("Only one ConstantMaterial may be used per material."), InMaterial);
			return 0;
		}
	}
	else if( (BitmapMaterial=CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, InMaterial, &ModifierInfo)) != NULL )
	{
		// handle simple case
		if( StagesUsed >= MaxTextureStages )
		{
			SET_ERROR_STRING_MATERIAL(TEXT("No stages left for BitmapMaterial"), InMaterial);
			return 0;
		}
		SetShaderBitmap( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], BitmapMaterial );
		CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
		CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].OperandAlpha0 = InvertOutputAlpha ? GL_ONE_MINUS_SRC_ALPHA : GL_SRC_ALPHA;
		CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
		CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
		CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp   = GL_REPLACE;
		ApplyTexModifier( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], &ModifierInfo );
		StagesUsed++;
		return 1;
	}
	else if( (Combiner=CheckMaterial<UCombiner,MT_Combiner>(this, InMaterial, &ModifierInfo)) != NULL )
	{
		UBitmapMaterial* BitmapMaterial1 = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, Combiner->Material1);
		UBitmapMaterial* BitmapMaterial2 = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, Combiner->Material2);

		if( !Combiner->Material1 )
		{
			SET_ERROR_STRING_MATERIAL(TEXT("Combiner must specify Material1"), InMaterial);
			return 0;
		}
		else
		if( !BitmapMaterial1 && !BitmapMaterial2 )
		{
			SET_ERROR_STRING_MATERIAL(TEXT("Either Material1 or Material2 must be a simple bitmap material."), InMaterial);
			return 0;
		}


		// Swap Material1/2 if it makes our life easier.
		UMaterial* Material1;
		UMaterial* Material2;
		UBOOL Swapped;

		// Rearrange arguments to ensure simple bitmap is the second operand.
		if( !BitmapMaterial2 )
		{
			Material1 = Combiner->Material2;
			Material2 = Combiner->Material1;
			Swapped = 1;
		}
		else
		{
			if( Combiner->InvertMask && BitmapMaterial1 && Combiner->Mask==Combiner->Material2 )
			{
				// Swap the arguments if they're both bitmaps, and we're also using arg2 as an inverted mask
				Material1 = Combiner->Material2;
				Material2 = Combiner->Material1;
				Swapped = 1;
			}
			else
			{
				// No need to swap
				Material1 = Combiner->Material1;
				Material2 = Combiner->Material2;
				Swapped = 0;
			}
		}

		// Process the complex first operand.
		if( !HandleCombinedMaterial( Material1, PassesUsed, StagesUsed, ModifierInfo, Combiner->Mask==Material1 && Combiner->InvertMask, ErrorString, ErrorMaterial ) )
			return 0;

		UBOOL   UseTextureAlpha = 0,
				UseFactorAlpha = 0,
				UseDiffuseAlpha = 0;
		if( Combiner->Mask )
		{
			if( Combiner->Mask == Material2 )
			{
				if( Combiner->InvertMask )
				{
					if( StagesUsed >= MaxTextureStages )
					{
						SET_ERROR_STRING_MATERIAL(
							Swapped ? TEXT("No stages left for inverted Mask which is the same as Material1") : TEXT("No stages left for inverted Mask which is the same as Material2"),
							InMaterial);
						return 0;
					}

					// Unfortunately we need to load the texture again to support an inverted mask for argument 2, when argument1 is complex
					FPixoModifierInfo MaskModifier = ModifierInfo;
					SetShaderBitmap( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, Combiner->Mask, &MaskModifier) );
					ApplyTexModifier( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], &MaskModifier );
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].OperandAlpha0 = GL_ONE_MINUS_SRC_ALPHA;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp   = GL_REPLACE;
					StagesUsed++;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].OperandAlpha0 = InvertOutputAlpha ? GL_ONE_MINUS_SRC_ALPHA : GL_SRC_ALPHA;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
				}
				else
				{
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].OperandAlpha0 = InvertOutputAlpha ? GL_ONE_MINUS_SRC_ALPHA : GL_SRC_ALPHA;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
					UseTextureAlpha = 1;
				}
			}
			else if( Combiner->Mask != Material1 )
			{
				UBitmapMaterial*    MaskBitmap;
				UVertexColor*       VertexColor;
				UConstantMaterial*  ConstantMaterial;

				if( (MaskBitmap=CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, Combiner->Mask)) != NULL )
				{
					// Process alpha channel from Mask Bitmap.
					if( StagesUsed >= MaxTextureStages )
					{
						SET_ERROR_STRING_MATERIAL(TEXT("No stages left for Mask bitmap"), InMaterial);
						return 0;
					}

					FPixoModifierInfo MaskModifier = ModifierInfo;
					SetShaderBitmap( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, Combiner->Mask, &MaskModifier) );
					ApplyTexModifier( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], &MaskModifier );
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].OperandAlpha0 = Combiner->InvertMask ? GL_ONE_MINUS_SRC_ALPHA : GL_SRC_ALPHA;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp   = GL_REPLACE;
					StagesUsed++;

					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].OperandAlpha0 = InvertOutputAlpha ? GL_ONE_MINUS_SRC_ALPHA : GL_SRC_ALPHA;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
				}
				else if( (VertexColor=CheckMaterial<UVertexColor,MT_VertexColor>(this, Combiner->Mask)) != NULL )
				{
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0  = GL_PRIMARY_COLOR;
					CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp    = GL_REPLACE;
					UseDiffuseAlpha = 1;
				}
				else if( (ConstantMaterial=CheckMaterial<UConstantMaterial,MT_ConstantMaterial>(this, Combiner->Mask)) != NULL)
				{
					FColor  Color = ConstantMaterial->GetColor(Viewport->Actor->Level->TimeSeconds);

					if((DWORD)CurrentState->MaterialPasses[PassesUsed]->TFactorColor == 0 || CurrentState->MaterialPasses[PassesUsed]->TFactorColor == Color )
					{
						CurrentState->MaterialPasses[PassesUsed]->TFactorColor                  = Color.DWColor();
						CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0  = GL_CONSTANT;
						CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp    = GL_REPLACE;
						UseFactorAlpha = 1;
					}
					else
					{
						SET_ERROR_STRING_MATERIAL(TEXT("Only one ConstantMaterial may be used per material."), InMaterial);
						return 0;
					}
				}
				else
				{
					SET_ERROR_STRING_MATERIAL(
						TEXT("Combiner Mask must be a bitmap material, a vertex color, a constant color, Material1 or Material2."),
						InMaterial);
					return 0;
				}
			}
		}

		if( StagesUsed >= MaxTextureStages )
		{
			SET_ERROR_STRING_MATERIAL(
				Swapped ? TEXT("No stages left for Bitmap Material1") : TEXT("No stages left for Bitmap Material2"),
				InMaterial);
			return 0;
		}

		// Process Material2
		FPixoModifierInfo Material2Modifier = ModifierInfo;
		SetShaderBitmap( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, Material2, &Material2Modifier) );
		ApplyTexModifier( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], &Material2Modifier );

		switch( Combiner->CombineOperation )
		{
		case CO_Use_Color_From_Material1:
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0 = Swapped ? GL_TEXTURE : GL_PREVIOUS;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp   = GL_REPLACE;
			break;
		case CO_Use_Color_From_Material2:
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0 = Swapped ? GL_PREVIOUS : GL_TEXTURE;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp   = GL_REPLACE;
			break;
		case CO_Multiply:
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb1 = GL_PREVIOUS;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp   = GL_MODULATE;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorScale= Combiner->Modulate4X ? 4.0f : (Combiner->Modulate2X ? 2.0f : 1.0f);
			break;
		case CO_Add:
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb1 = GL_PREVIOUS;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp   = GL_ADD;
			break;
		case CO_Subtract:
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0 = Swapped ? GL_PREVIOUS : GL_TEXTURE;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb1 = Swapped ? GL_TEXTURE : GL_PREVIOUS;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp   = GL_SUBTRACT;
			break;
		case CO_AlphaBlend_With_Mask:
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0 = Swapped ? GL_PREVIOUS : GL_TEXTURE;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb1 = Swapped ? GL_TEXTURE : GL_PREVIOUS;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp   = GL_INTERPOLATE;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].OperandRgb2 = GL_SRC_ALPHA;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb2 = UseTextureAlpha ? GL_TEXTURE : UseFactorAlpha ? GL_CONSTANT : UseDiffuseAlpha ? GL_PRIMARY_COLOR : GL_PREVIOUS;

			break;
		case CO_Add_With_Mask_Modulation:
			//CurrentState->MaterialPasses[PassesUsed]->PatchLighting = 0;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp   = GL_REPLACE;
			break;
		}

		switch( Combiner->AlphaOperation )
		{
		case AO_Use_Mask:
			break;
		case AO_Multiply:
			if( Combiner->Mask && Combiner->Mask != Combiner->Material1 && Combiner->Mask != Combiner->Material2 )
			{
				SET_ERROR_STRING_MATERIAL(TEXT("Combiner Mask must be Material1, or Material2 or None when using Alpha Operation AO_Multiply."), InMaterial);
				return 0;
			}
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha1 = GL_PREVIOUS;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp   = GL_MODULATE;
			if( InvertOutputAlpha )
			{
				StagesUsed++;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp   = GL_REPLACE;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].OperandAlpha0 = GL_ONE_MINUS_SRC_ALPHA;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
			}
			break;
		case AO_Add:
			if( Combiner->Mask && Combiner->Mask != Combiner->Material1 && Combiner->Mask != Combiner->Material2 )
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("Combiner Mask must be Material1, or Material2 or None when using Alpha Operation AO_Add."),
					InMaterial);
				return 0;
			}
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha1 = GL_PREVIOUS;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp   = GL_ADD;
			if( InvertOutputAlpha )
			{
				StagesUsed++;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp   = GL_REPLACE;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].OperandAlpha0 = GL_ONE_MINUS_SRC_ALPHA;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
			}
			break;
		case AO_Use_Alpha_From_Material1:
			if( Combiner->Mask && Combiner->Mask != Combiner->Material1 && Combiner->Mask != Combiner->Material2 )
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("Combiner Mask must be Material1, or Material2 or None when using Alpha Operation AO_Use_Alpha_From_Material1."),
					InMaterial);
				return 0;
			}
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0 = Swapped ? GL_TEXTURE : GL_PREVIOUS;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
			if( InvertOutputAlpha )
			{
				StagesUsed++;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp   = GL_REPLACE;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].OperandAlpha0 = GL_ONE_MINUS_SRC_ALPHA;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
			}
			break;
		case AO_Use_Alpha_From_Material2:
			if( Combiner->Mask && Combiner->Mask != Combiner->Material1 && Combiner->Mask != Combiner->Material2 )
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("Combiner Mask must be Material1, or Material2 or None when using Alpha Operation AO_Use_Alpha_From_Material2."),
					InMaterial);
				return 0;
			}
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0 = Swapped ? GL_PREVIOUS : GL_TEXTURE;
			CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
			if( InvertOutputAlpha )
			{
				StagesUsed++;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp   = GL_REPLACE;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].OperandAlpha0 = GL_ONE_MINUS_SRC_ALPHA;
				CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
			}
			break;
		}

		StagesUsed++;
	}

	return 1;
	unguard;
}

//
// FPixoRenderInterface::SetShaderMaterial
//
UBOOL FPixoRenderInterface::SetShaderMaterial( UShader* InShader, FPixoModifierInfo BaseModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial )
{
	guard(FPixoRenderInterface::SetShaderMaterial);

	INT MaxTextureStages = RenDev->NumTextureUnits;

	INT StagesUsed = 0;
	INT PassesUsed = 0;

	//
	// 1. Process diffuse, opacity and lightmap
	//
	UMaterial* InDiffuse = InShader->Diffuse;

	UBOOL Unlit             = 0;
	UBOOL NeedsLightmap     = CurrentState->Lightmap ? 1 : 0;
	UBOOL NeedsDiffuseLight = CurrentState->UseStaticLighting || CurrentState->UseDynamicLighting;

	// Check for an unlit material
	if( InShader->SelfIllumination && !InShader->SelfIlluminationMask )
	{
		InDiffuse           = InShader->SelfIllumination;
		Unlit               = 1;
		NeedsDiffuseLight   = 0;
		NeedsLightmap       = 0;
	}

	// Simplify shader for Pixo.
	if( RenDev->SimpleMaterials )
	{
		InShader->Specular				= NULL;
		InShader->SpecularityMask		= NULL;
	}

	// Check for an opacity override
	UMaterial*  InOpacity               = InShader->Opacity;
	UBOOL       OpacityUseBaseModifier  = 1;

	if( BaseModifierInfo.ModifyOpacity )
	{
		InOpacity                       = BaseModifierInfo.Opacity;
		OpacityUseBaseModifier          = !BaseModifierInfo.OpacityOverrideTexModifier;
	}

	// Simple case: no opacity!
	if( !InOpacity )
	{
		// Process diffuse channel
		if( !HandleCombinedMaterial( InDiffuse, PassesUsed, StagesUsed, BaseModifierInfo, 0, ErrorString, ErrorMaterial ) )
			return 0;

		if( StagesUsed == 1 )
		{
			// patch in diffuse to stage[0]
			if( NeedsDiffuseLight )
				HandleDiffuse_Patch( CurrentState->MaterialPasses[PassesUsed]->Stages[0] );

			// add a lightmap
			if( NeedsLightmap )
			{
				HandleLightmap_SP( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], CurrentState->Lightmap );
				StagesUsed++;
			}
		}
		else
		if( StagesUsed<MaxTextureStages && NeedsLightmap && !NeedsDiffuseLight )
		{
			HandleLightmap_SP( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], CurrentState->Lightmap );
			StagesUsed++;
		}
		else
		if( StagesUsed<MaxTextureStages && !NeedsLightmap && NeedsDiffuseLight )
		{
			if( StagesUsed )
			{
				HandleDiffuse_Stage( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], InShader->ModulateStaticLighting2X );
				StagesUsed++;
			}
		}
		else
		{
			// Perform lighting in a seperate pass
			if( NeedsDiffuseLight || NeedsLightmap )
			{
				NEWPASS();
				HandleLighting_MP( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], CurrentState->Lightmap, NeedsDiffuseLight );
				StagesUsed++;

				// Set lighting framebuffer blending
				if( NeedsDiffuseLight && !NeedsLightmap && !InShader->ModulateStaticLighting2X )
				{
					CurrentState->MaterialPasses[PassesUsed]->SrcBlend              = GL_DST_COLOR;
					CurrentState->MaterialPasses[PassesUsed]->DestBlend             = GL_ZERO;
					CurrentState->MaterialPasses[PassesUsed]->OverriddenFogColor	= FColor( 255, 255, 255, 0 );
					CurrentState->MaterialPasses[PassesUsed]->OverrideFogColor		= 1;
				}
				else
				{
					CurrentState->MaterialPasses[PassesUsed]->SrcBlend              = GL_DST_COLOR;
					CurrentState->MaterialPasses[PassesUsed]->DestBlend             = GL_SRC_COLOR;
					CurrentState->MaterialPasses[PassesUsed]->OverriddenFogColor    = FColor( 127, 127, 127, 0 );
					CurrentState->MaterialPasses[PassesUsed]->OverrideFogColor      = 1;
				}

				CurrentState->MaterialPasses[PassesUsed]->TwoSided                  = InShader->TwoSided;
				CurrentState->MaterialPasses[PassesUsed]->AlphaBlending             = 1;
			}
		}


		//
		// 2. Self-illumination
		//
		UBOOL SinglePassSelfIllum=0;
		if( !RenDev->SimpleMaterials && !Unlit && InShader->SelfIllumination )
		{
			FPixoModifierInfo SelfIlluminationModifierInfo      = BaseModifierInfo;
			FPixoModifierInfo SelfIlluminationMaskModifierInfo  = BaseModifierInfo;

			UBitmapMaterial* SelfIlluminationBitmap     = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, InShader->SelfIllumination, &SelfIlluminationModifierInfo );
			UBitmapMaterial* SelfIlluminationMaskBitmap = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, InShader->SelfIlluminationMask, &SelfIlluminationMaskModifierInfo );

			if( PassesUsed==0 && StagesUsed<MaxTextureStages-1 && SelfIlluminationBitmap && SelfIlluminationMaskBitmap )
			{
				// we have room to add the self-illumination in the single pass
				HandleOpacityBitmap( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], SelfIlluminationMaskBitmap );
				ApplyTexModifier( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed],&SelfIlluminationMaskModifierInfo );
				StagesUsed++;

				HandleSelfIllumination_SP( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], SelfIlluminationBitmap);
				ApplyTexModifier( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed],&SelfIlluminationModifierInfo );
				StagesUsed++;
				SinglePassSelfIllum=1;
			}
			else
			{
				// we need to do the self-illumination in its own pass
				NEWPASS();
				CurrentState->MaterialPasses[PassesUsed]->TwoSided = InShader->TwoSided;

				// Load SelfIllumination
				if( !HandleCombinedMaterial( InShader->SelfIllumination, PassesUsed, StagesUsed, BaseModifierInfo, 0, ErrorString, ErrorMaterial ) )
					return 0;

				if( InShader->SelfIlluminationMask == InShader->SelfIllumination )
				{
					// Self-illumination alpha channel is in SelfIllumination.  Just blend it to the framebuffer.
					CurrentState->MaterialPasses[PassesUsed]->SrcBlend          = GL_SRC_ALPHA;
					CurrentState->MaterialPasses[PassesUsed]->DestBlend         = GL_ONE_MINUS_SRC_ALPHA;
					CurrentState->MaterialPasses[PassesUsed]->AlphaBlending     = 1;
				}
				else
				if( SelfIlluminationMaskBitmap )
				{
					if( StagesUsed < MaxTextureStages )
					{
						HandleOpacityBitmap( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], SelfIlluminationMaskBitmap );
						ApplyTexModifier( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed],&SelfIlluminationMaskModifierInfo );
						StagesUsed++;
					}
					else
					{
						SET_ERROR_STRING_MATERIAL(
							FString::Printf(TEXT("No room for SelfIllimunationMask in multipass SelfIllumination (StagesUsed was %d)"), StagesUsed),
							InShader);
						return 0;
					}

					// Add SelfIllumination to the framebuffer
					CurrentState->MaterialPasses[PassesUsed]->SrcBlend          = GL_SRC_ALPHA;
					CurrentState->MaterialPasses[PassesUsed]->DestBlend         = GL_ONE_MINUS_SRC_ALPHA;
					CurrentState->MaterialPasses[PassesUsed]->AlphaBlending     = 1;
				}
				else
				{
					SET_ERROR_STRING_MATERIAL(
						TEXT("SelfIllimunationMask must be a simple bitmap"),
						InShader);
					return 0;
				}
			}
		}

		//
		// 3. Detail Texture
		//
		if( RenDev->DetailTextures && CurrentState->UseDetailTexturing )
		{
			UMaterial*  DetailMaterial  = NULL;
			FLOAT       DetailScale     = 8.f;
			if( InShader->Detail )
			{
				DetailMaterial  = InShader->Detail;
				DetailScale     = InShader->DetailScale;
			}
			else
			{
				UTexture* DiffuseTex = CheckMaterial<UTexture,MT_Texture>(this, InDiffuse);
				if( DiffuseTex )
				{
					DetailMaterial  = DiffuseTex->Detail;
					DetailScale     = DiffuseTex->DetailScale;
				}
			}

			if( DetailMaterial )
			{
				FPixoModifierInfo DetailModifierInfo = BaseModifierInfo;
				DetailModifierInfo.SetDetailTextureScale( DetailScale );
				UBitmapMaterial* DetailBitmap = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, DetailMaterial, &DetailModifierInfo);
				if( DetailBitmap )
					HandleDetail( DetailBitmap, PassesUsed, StagesUsed, DetailModifierInfo );
			}
		}

		//
		// 4. Process specular and mask
		//
		if( InShader->Specular )
		{
			FPixoModifierInfo SpecularModifierInfo      = BaseModifierInfo;
			FPixoModifierInfo SpecularityMaskModifierInfo = BaseModifierInfo;

			UBitmapMaterial*    SpecularBitmap              = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, InShader->Specular, &SpecularModifierInfo );
			UBitmapMaterial*    SpecularityMaskBitmap       = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, InShader->SpecularityMask, &SpecularityMaskModifierInfo );
			UConstantMaterial*  SpecularityMaskConstant     = CheckMaterial<UConstantMaterial,MT_ConstantMaterial>(this, InShader->SpecularityMask, &SpecularityMaskModifierInfo );

			if( PassesUsed==0 && StagesUsed<MaxTextureStages && SpecularBitmap && ((!InShader->SpecularityMask)||(InShader->SpecularityMask==InShader->Diffuse&&!SinglePassSelfIllum)||SpecularityMaskConstant) )
			{
				// We can fit single-pass specular
				if( SpecularityMaskConstant )
				{
					BYTE SpecularAlpha = SpecularityMaskConstant->GetColor(Viewport->Actor->Level->TimeSeconds).A;
					CurrentState->MaterialPasses[PassesUsed]->TFactorColor = FColor(SpecularAlpha,SpecularAlpha,SpecularAlpha,SpecularAlpha);
				}
				HandleSpecular_SP( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], SpecularBitmap, InShader->SpecularityMask != NULL, SpecularityMaskConstant != NULL, InShader->ModulateSpecular2X ); // sjs
				ApplyTexModifier( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], &SpecularModifierInfo );
				StagesUsed++;
			}
			else
			if( PassesUsed==0 && StagesUsed<MaxTextureStages-1 && SpecularBitmap && SpecularityMaskBitmap )
			{
				// We have room for the specular and mask in the same pass pass as diffuse
				HandleOpacityBitmap( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], SpecularityMaskBitmap );
				ApplyTexModifier( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], &SpecularityMaskModifierInfo );
				StagesUsed++;
				HandleSpecular_SP( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], SpecularBitmap, 1, 0, InShader->ModulateSpecular2X ); // sjs
				ApplyTexModifier( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], &SpecularModifierInfo );
				StagesUsed++;
			}
			else
			{
				// We need to perform specular in its own pass
				NEWPASS();
				CurrentState->MaterialPasses[PassesUsed]->TwoSided = InShader->TwoSided;

				// Load specular
				if( !HandleCombinedMaterial( InShader->Specular, PassesUsed, StagesUsed, BaseModifierInfo, 0, ErrorString, ErrorMaterial ) )
					return 0;

				if( !InShader->SpecularityMask )
				{
					// Add specular to the framebuffer
					CurrentState->MaterialPasses[PassesUsed]->SrcBlend              = GL_ONE;
					CurrentState->MaterialPasses[PassesUsed]->DestBlend             = GL_ONE;
					CurrentState->MaterialPasses[PassesUsed]->AlphaBlending         = 1;
					CurrentState->MaterialPasses[PassesUsed]->OverrideFogColor      = 1;
					CurrentState->MaterialPasses[PassesUsed]->OverriddenFogColor    = FColor( 0, 0, 0, 0 );
				}
				else
				if( SpecularityMaskConstant )
				{
					SET_ERROR_STRING_MATERIAL(
						TEXT("TODO: constant SpecularityMask with multipass specular"),
						InShader);
					return 0;
				}
				else
				if( SpecularityMaskBitmap )
				{
					if( StagesUsed < MaxTextureStages )
					{
						HandleOpacityBitmap( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], SpecularityMaskBitmap );
						ApplyTexModifier( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], &SpecularityMaskModifierInfo );
						StagesUsed++;
					}
					else
					{
						SET_ERROR_STRING_MATERIAL(
							FString::Printf(TEXT("No room for SpecularityMask in multipass specular (StagesUsed was %d)"), StagesUsed),
							InShader);
						return 0;
					}

					// Add specular to the framebuffer
					CurrentState->MaterialPasses[PassesUsed]->SrcBlend              = GL_SRC_ALPHA;
					CurrentState->MaterialPasses[PassesUsed]->DestBlend             = GL_ONE;
					CurrentState->MaterialPasses[PassesUsed]->AlphaBlending         = 1;
					CurrentState->MaterialPasses[PassesUsed]->OverrideFogColor      = 1;
					CurrentState->MaterialPasses[PassesUsed]->OverriddenFogColor    = FColor( 0, 0, 0, 0 );
				}
				else
				{
					SET_ERROR_STRING_MATERIAL(
						TEXT("SpecularityMask must be a simple bitmap"),
						InShader);
					return 0;
				}
			}
		}

	} // no opacity
	else
	{
		//
		// 1. Handle diffuse
		//
		if( !HandleCombinedMaterial( InDiffuse, PassesUsed, StagesUsed, BaseModifierInfo, 0, ErrorString, ErrorMaterial ) )
			return 0;

		// Handle the vertex color
		if( NeedsDiffuseLight )
		{
			if( StagesUsed==1 )
				HandleDiffuse_Patch( CurrentState->MaterialPasses[PassesUsed]->Stages[0] );
			else
			{
				if( StagesUsed < MaxTextureStages)
				{
					HandleDiffuse_Stage( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], InShader->ModulateStaticLighting2X );
					StagesUsed++;
				}
				else
				{
					SET_ERROR_STRING_MATERIAL(
						TEXT("No stages left for vertex lighting stage"),
						InShader);
					return 0;
				}
			}
		}

		// Handle the lightmap
		if( NeedsLightmap )
		{
			if( StagesUsed < MaxTextureStages )
			{
				HandleLightmap_SP( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], CurrentState->Lightmap );
				StagesUsed++;
			}
			else
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("No stages left for lightmap"),
					InShader);
				return 0;
			}
		}

		// Load the opacity if needed
		if( InOpacity != InDiffuse )
		{
			FPixoModifierInfo OpacityModifierInfo = OpacityUseBaseModifier ? BaseModifierInfo : FPixoModifierInfo();
			UBitmapMaterial*    OpacityBitmap;
			UConstantMaterial*  ConstantMaterial;
			UVertexColor*       VertexColor;
			if( (OpacityBitmap=CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, InOpacity, &OpacityModifierInfo )) != NULL )
			{
				if( StagesUsed < MaxTextureStages )
				{
					// The opacity is a simple texture:
					HandleOpacityBitmap( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], OpacityBitmap );
					ApplyTexModifier( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], &OpacityModifierInfo );
					StagesUsed++;
				}
				else
				{
					SET_ERROR_STRING_MATERIAL(
						TEXT("No stages left for opacity bitmap"),
						InShader);
					return 0;
				}
			}
			else if( (ConstantMaterial=CheckMaterial<UConstantMaterial,MT_ConstantMaterial>(this, InOpacity, &OpacityModifierInfo )) != NULL )
			{
				if( StagesUsed < MaxTextureStages )
				{
					FColor  Color = ConstantMaterial->GetColor(Viewport->Actor->Level->TimeSeconds);

					if((DWORD)(CurrentState->MaterialPasses[PassesUsed]->TFactorColor) == 0 || CurrentState->MaterialPasses[PassesUsed]->TFactorColor == Color )
					{
						CurrentState->MaterialPasses[PassesUsed]->TFactorColor = Color.DWColor();
						CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceAlpha0  = GL_CONSTANT;
						CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].AlphaOp    = GL_REPLACE;
						CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].SourceRgb0  = GL_PREVIOUS;
						CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed].ColorOp    = GL_REPLACE;
						StagesUsed++;
					}
					else
					{
						SET_ERROR_STRING_MATERIAL(
							TEXT("Only one ConstantMaterial may be used per material."),
							InShader);
						return 0;
					}
				}
				else
				{
					SET_ERROR_STRING_MATERIAL(
						TEXT("No stages left for constant opacity"),
						InShader);
					return 0;
				}
			}
			else if( (VertexColor=CheckMaterial<UVertexColor,MT_VertexColor>(this, InOpacity, &OpacityModifierInfo )) != NULL )
			{
				if( StagesUsed < MaxTextureStages )
				{
					HandleVertexOpacity( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], VertexColor );
					StagesUsed++;
				}
				else
				{
					SET_ERROR_STRING_MATERIAL(
						TEXT("No stages left for vertex opacity"),
						InShader);
					return 0;
				}
			}
			else
			{
				// don't handle this case yet!
				SET_ERROR_STRING_MATERIAL(
					TEXT("Cannot handle complex opacity which is not the same as diffuse material"),
					InOpacity);
				return 0;
			}
		}

		//
		// 2. Self-illumination
		//
		if( !RenDev->SimpleMaterials && !Unlit && InShader->SelfIllumination )
		{
			// we need to do the self-illumination in its own pass
			NEWPASS();
			CurrentState->MaterialPasses[PassesUsed]->TwoSided = InShader->TwoSided;

			FPixoModifierInfo OpacityModifierInfo = OpacityUseBaseModifier ? BaseModifierInfo : FPixoModifierInfo();
			UBitmapMaterial* OpacityBitmap = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, InOpacity, &OpacityModifierInfo );

			if( !OpacityBitmap )
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("Opacity must be a simple bitmap when using SelfIllumination"),
					InOpacity);
				return 0;
			}

			FPixoModifierInfo SelfIlluminationModifierInfo = BaseModifierInfo;
			FPixoModifierInfo SelfIlluminationMaskModifierInfo = BaseModifierInfo;

			UBitmapMaterial* SelfIlluminationMaskBitmap = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, InShader->SelfIlluminationMask, &SelfIlluminationMaskModifierInfo );

			// Load SelfIllumination
			if( !HandleCombinedMaterial( InShader->SelfIllumination, PassesUsed, StagesUsed, BaseModifierInfo, 0, ErrorString, ErrorMaterial ) )
				return 0;

			if( SelfIlluminationMaskBitmap )
			{
				if( StagesUsed < MaxTextureStages-1 )
				{
					HandleOpacityBitmap( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], SelfIlluminationMaskBitmap );
					ApplyTexModifier( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed],&SelfIlluminationMaskModifierInfo );
					StagesUsed++;

					HandleOpacityBitmap( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], OpacityBitmap, 1 );
					ApplyTexModifier( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], &OpacityModifierInfo );
					StagesUsed++;
				}
				else
				{
					SET_ERROR_STRING_MATERIAL(
						FString::Printf(TEXT("No room for SelfIllimunationMask in multipass SelfIllumination (StagesUsed was %d)"), StagesUsed),
						InShader);
					return 0;
				}

				// Add SelfIllumination to the framebuffer
				CurrentState->MaterialPasses[PassesUsed]->SrcBlend          = GL_SRC_ALPHA;
				CurrentState->MaterialPasses[PassesUsed]->DestBlend         = GL_ONE_MINUS_SRC_ALPHA;
				CurrentState->MaterialPasses[PassesUsed]->AlphaBlending     = 1;
			}
			else
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("SelfIllimunationMask must be a simple bitmap"),
					InShader);
				return 0;
			}
		}

		//
		// 3. Specular
		//
		if( InShader->Specular )
		{
			FPixoModifierInfo SpecularModifierInfo = BaseModifierInfo;
			FPixoModifierInfo SpecularityMaskModifierInfo = BaseModifierInfo;

			UBitmapMaterial* SpecularityMaskBitmap = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, InShader->SpecularityMask, &SpecularityMaskModifierInfo );
			UConstantMaterial* SpecularityMaskConstant = CheckMaterial<UConstantMaterial,MT_ConstantMaterial>(this, InShader->SpecularityMask, &SpecularityMaskModifierInfo );

			// We need to perform specular in its own pass
			NEWPASS();
			CurrentState->MaterialPasses[PassesUsed]->TwoSided = InShader->TwoSided;

			// Load specular
			if( !HandleCombinedMaterial( InShader->Specular, PassesUsed, StagesUsed, BaseModifierInfo, 0, ErrorString, ErrorMaterial ) )
				return 0;

			if( !InShader->SpecularityMask )
			{
				// No specularity mask, do nothing.
			}
			else
			if( SpecularityMaskConstant )
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("TODO: constant SpecularityMask with multipass specular"),
					InShader);
				return 0;
			}
			else
			if( SpecularityMaskBitmap )
			{
				if( StagesUsed < MaxTextureStages )
				{
					HandleOpacityBitmap( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], SpecularityMaskBitmap );
					ApplyTexModifier( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], &SpecularityMaskModifierInfo );
					StagesUsed++;
				}
				else
				{
					SET_ERROR_STRING_MATERIAL(
						FString::Printf(TEXT("No room for SpecularityMask in multipass specular (StagesUsed was %d)"), StagesUsed),
						InShader);
					return 0;
				}
			}
			else
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("SpecularityMask must be a simple bitmap"),
					InShader);
				return 0;
			}

			// Add specular to the framebuffer
			CurrentState->MaterialPasses[PassesUsed]->SrcBlend          = InShader->SpecularityMask ? GL_SRC_ALPHA : GL_ONE;
			CurrentState->MaterialPasses[PassesUsed]->SrcBlend          = InShader->ModulateSpecular2X ? GL_DST_COLOR : CurrentState->MaterialPasses[PassesUsed]->SrcBlend; // sjs
			CurrentState->MaterialPasses[PassesUsed]->DestBlend         = InShader->ModulateSpecular2X ? GL_SRC_COLOR : GL_ONE; // sjs
			CurrentState->MaterialPasses[PassesUsed]->AlphaBlending     = 1;
			CurrentState->MaterialPasses[PassesUsed]->OverrideFogColor      = 1;
			CurrentState->MaterialPasses[PassesUsed]->OverriddenFogColor    = FColor( 0, 0, 0, 0 );
		}
	}

	//
	// Framebuffer blending
	//
	if( BaseModifierInfo.ModifyFramebufferBlending )
	{
		ApplyFinalBlend( &BaseModifierInfo );
	}
	else
	{
		switch( InShader->OutputBlending )
		{
		case OB_Normal :
			if( InOpacity )
			{
				CurrentState->MaterialPasses[0]->SrcBlend       = GL_SRC_ALPHA;
				CurrentState->MaterialPasses[0]->DestBlend      = GL_ONE_MINUS_SRC_ALPHA;
				CurrentState->MaterialPasses[0]->AlphaBlending  = 1;
				CurrentState->MaterialPasses[0]->AlphaTest      = 1;
				CurrentState->MaterialPasses[0]->ZWrite         = 0;
			}
			else
			{
				CurrentState->MaterialPasses[0]->SrcBlend       = GL_ONE;
				CurrentState->MaterialPasses[0]->DestBlend      = GL_ZERO;
				CurrentState->MaterialPasses[0]->AlphaBlending  = 0;
				CurrentState->MaterialPasses[0]->AlphaTest      = 0;
				CurrentState->MaterialPasses[0]->ZWrite         = 1;
			}
			break;
		case OB_Masked:
			CurrentState->MaterialPasses[0]->SrcBlend           = GL_ONE;
			CurrentState->MaterialPasses[0]->DestBlend          = GL_ZERO;
			CurrentState->MaterialPasses[0]->AlphaBlending      = 0;
			CurrentState->MaterialPasses[0]->AlphaTest          = 1;
			CurrentState->MaterialPasses[0]->AlphaRef           = 127;
			CurrentState->MaterialPasses[0]->ZWrite             = 1;
			break;
		case OB_Modulate:
			CurrentState->MaterialPasses[0]->SrcBlend           = GL_DST_COLOR;
			CurrentState->MaterialPasses[0]->DestBlend          = GL_SRC_COLOR;
			CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
			CurrentState->MaterialPasses[0]->ZWrite             = 0;
			CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
			CurrentState->MaterialPasses[0]->OverriddenFogColor = FColor( 127, 127, 127, 0 );
			break;
		case OB_Translucent:
			CurrentState->MaterialPasses[0]->SrcBlend           = GL_ONE;
			CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE;
			CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
			CurrentState->MaterialPasses[0]->ZWrite             = 0;
			CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
			CurrentState->MaterialPasses[0]->OverriddenFogColor = FColor( 0, 0, 0, 0 );
			break;
		case OB_Brighten:
			CurrentState->MaterialPasses[0]->SrcBlend           = GL_ONE;
			CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE_MINUS_SRC_COLOR;
			CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
			CurrentState->MaterialPasses[0]->ZWrite             = 0;
			CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
			CurrentState->MaterialPasses[0]->OverriddenFogColor= FColor( 0, 0, 0, 0 );
			break;
		case OB_Darken:
			CurrentState->MaterialPasses[0]->SrcBlend           = GL_ZERO;
			CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE_MINUS_SRC_COLOR;
			CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
			CurrentState->MaterialPasses[0]->ZWrite         = 0;
			CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
			CurrentState->MaterialPasses[0]->OverriddenFogColor= FColor( 0, 0, 0, 0 );
			break;
		case OB_Invisible:
			CurrentState->MaterialPasses[0]->SrcBlend           = GL_ZERO;
			CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE;
			CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
			CurrentState->MaterialPasses[0]->ZWrite         = 0;
			break;
		}
		CurrentState->MaterialPasses[0]->TwoSided = InShader->TwoSided;
	}

	// Fillmode.
	CurrentState->MaterialPasses[0]->FillMode = InShader->Wireframe ? FM_Wireframe : FM_Solid;

	// Color modifier
	if( BaseModifierInfo.ModifyColor )
	{
		if( BaseModifierInfo.AlphaBlend                             &&
			(CurrentState->MaterialPasses[0]->SrcBlend  == GL_ONE)  &&
			(CurrentState->MaterialPasses[0]->DestBlend == GL_ZERO)
		)
		{
				CurrentState->MaterialPasses[0]->SrcBlend   = GL_SRC_ALPHA;
				CurrentState->MaterialPasses[0]->DestBlend  = GL_ONE_MINUS_SRC_ALPHA;
		}
		CurrentState->MaterialPasses[0]->TFactorColor        = BaseModifierInfo.TFactorColor;
		CurrentState->MaterialPasses[0]->AlphaBlending      |= BaseModifierInfo.AlphaBlend;
		CurrentState->MaterialPasses[0]->TwoSided           |= BaseModifierInfo.TwoSided;

		if( StagesUsed
		&&  CurrentState->MaterialPasses[0]->Stages[StagesUsed-1].AlphaOp == GL_REPLACE
		&&  CurrentState->MaterialPasses[0]->Stages[StagesUsed-1].ColorOp == GL_REPLACE
		)
		{
			HandleTFactor_Patch(CurrentState->MaterialPasses[0]->Stages[StagesUsed-1]);
		}
		else if( StagesUsed < MaxTextureStages )
		{
			HandleTFactor_SP(CurrentState->MaterialPasses[0]->Stages[StagesUsed++]);
		}
		else
		{
			SET_ERROR_STRING_MATERIAL(
				TEXT("No stages left for constant color modifier."),
				InShader);
			return 0;
		}
	}

	// Done.
	CurrentState->MaterialPasses[PassesUsed++]->StagesUsed  = StagesUsed;
	CurrentState->NumMaterialPasses         = PassesUsed;

	return 1;
	unguard;
}

/*----------------------------------------------------------------------------
	SetSimpleMaterial.
----------------------------------------------------------------------------*/

UBOOL FPixoRenderInterface::SetSimpleMaterial( UMaterial* InMaterial, FPixoModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial )
{
	guard(FPixoRenderInterface::SetSimpleMaterial);

	// Handle masked/alpha texture UTexture
	UTexture* Texture;
	UMaterial* DetailMaterial = NULL;
	FLOAT DetailScale = 8.f;
	if( !InModifierInfo.ModifyFramebufferBlending && (Texture=Cast<UTexture>(InMaterial))!=NULL )
	{
		if( Texture->bMasked )
		{
			InModifierInfo.ModifyFramebufferBlending    = 1;
			if( InModifierInfo.ModifyColor && InModifierInfo.AlphaBlend )
				InModifierInfo.FrameBufferBlending      = FB_AlphaBlend;
			else
				InModifierInfo.FrameBufferBlending      = FB_Overwrite;
			InModifierInfo.ZWrite       = 1;
			InModifierInfo.ZTest        = 1;
			InModifierInfo.AlphaTest    = 1;
			InModifierInfo.AlphaRef     = 127;
			if( !InModifierInfo.ModifyColor )
				InModifierInfo.TwoSided = Texture->bTwoSided;
		}
		else
		if( Texture->bAlphaTexture )
		{
			InModifierInfo.ModifyFramebufferBlending    = 1;
			InModifierInfo.FrameBufferBlending          = FB_AlphaBlend;
			InModifierInfo.ZWrite       = 1;
			InModifierInfo.ZTest        = 1;
			InModifierInfo.AlphaTest    = 1;
			InModifierInfo.AlphaRef     = 0;
			if( !InModifierInfo.ModifyColor )
				InModifierInfo.TwoSided = Texture->bTwoSided;
		}
		else
		{
			if( !InModifierInfo.ModifyColor )
				InModifierInfo.TwoSided = Texture->bTwoSided;
		}

		DetailMaterial = Texture->Detail;
		DetailScale = Texture->DetailScale;
	}

	INT StagesUsed = 0;
	INT PassesUsed = 0;

	INT MaxTextureStages = RenDev->NumTextureUnits;

	if( !HandleCombinedMaterial( InMaterial, PassesUsed, StagesUsed, InModifierInfo, 0, ErrorString, ErrorMaterial ) )
		return 0;

	if( CurrentState->UseStaticLighting || CurrentState->UseDynamicLighting )
	{
		if( CurrentState->MaterialPasses[PassesUsed]->PatchLighting )
		{		
			HandleDiffuse_Patch( CurrentState->MaterialPasses[PassesUsed]->Stages[0] );
		}
		else if( StagesUsed < MaxTextureStages )
		{
			HandleDiffuse_Stage( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed++] );
		}
		else
		{
			NEWPASS();
			HandleLighting_MP( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], NULL, 1 );
			StagesUsed++;

			CurrentState->MaterialPasses[PassesUsed]->SrcBlend              = GL_DST_COLOR;
			CurrentState->MaterialPasses[PassesUsed]->DestBlend             = GL_SRC_COLOR;
			CurrentState->MaterialPasses[PassesUsed]->OverriddenFogColor	= FColor( 127, 127, 127, 0 );
			CurrentState->MaterialPasses[PassesUsed]->OverrideFogColor		= 1;
			
			CurrentState->MaterialPasses[PassesUsed]->TwoSided				= InModifierInfo.TwoSided;
			CurrentState->MaterialPasses[PassesUsed]->AlphaBlending			= 1;
		}
	}

	if( CurrentState->Lightmap )
	{
		if( StagesUsed < MaxTextureStages )
			HandleLightmap_SP( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed++], CurrentState->Lightmap );
		else
		{
			NEWPASS();
			HandleLighting_MP( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed], CurrentState->Lightmap, CurrentState->UseStaticLighting );
			StagesUsed++;

			CurrentState->MaterialPasses[PassesUsed]->SrcBlend              = GL_DST_COLOR;
			CurrentState->MaterialPasses[PassesUsed]->DestBlend             = GL_SRC_COLOR;
			CurrentState->MaterialPasses[PassesUsed]->OverriddenFogColor    = FColor( 127, 127, 127, 0 );
			CurrentState->MaterialPasses[PassesUsed]->OverrideFogColor      = 1;

			CurrentState->MaterialPasses[PassesUsed]->AlphaBlending         = 1;
			CurrentState->MaterialPasses[PassesUsed]->TwoSided              = InModifierInfo.TwoSided;
		}
	}

	// Set framebuffer blending
	if( InModifierInfo.ModifyFramebufferBlending )
	{
		ApplyFinalBlend( &InModifierInfo );
	}

	// Color modifier
	if( InModifierInfo.ModifyColor )
	{
		if( StagesUsed < MaxTextureStages )
		{
			if( InModifierInfo.AlphaBlend                                           &&
				(CurrentState->MaterialPasses[PassesUsed]->SrcBlend  == GL_ONE) &&
				(CurrentState->MaterialPasses[PassesUsed]->DestBlend == GL_ZERO)
			)
			{
				CurrentState->MaterialPasses[PassesUsed]->SrcBlend  = GL_SRC_ALPHA;
				CurrentState->MaterialPasses[PassesUsed]->DestBlend = GL_ONE_MINUS_SRC_ALPHA;
			}
			if( StagesUsed
			&&  CurrentState->MaterialPasses[0]->Stages[StagesUsed-1].AlphaOp == GL_REPLACE
			&&  CurrentState->MaterialPasses[0]->Stages[StagesUsed-1].ColorOp == GL_REPLACE
			)
				HandleTFactor_Patch(CurrentState->MaterialPasses[0]->Stages[StagesUsed-1]);
			else
				HandleTFactor_SP(CurrentState->MaterialPasses[0]->Stages[StagesUsed++]);
			CurrentState->MaterialPasses[PassesUsed]->TFactorColor   = InModifierInfo.TFactorColor;
			CurrentState->MaterialPasses[PassesUsed]->AlphaBlending |= InModifierInfo.AlphaBlend;
			CurrentState->MaterialPasses[PassesUsed]->TwoSided      |= InModifierInfo.TwoSided;
		}
		else
		{
			if( StagesUsed
			&&  CurrentState->MaterialPasses[0]->Stages[StagesUsed-1].AlphaOp == GL_REPLACE
			&&  CurrentState->MaterialPasses[0]->Stages[StagesUsed-1].ColorOp == GL_REPLACE
			)
			{
				HandleTFactor_Patch(CurrentState->MaterialPasses[0]->Stages[StagesUsed-1]);
				CurrentState->MaterialPasses[PassesUsed]->TFactorColor   = InModifierInfo.TFactorColor;
				CurrentState->MaterialPasses[PassesUsed]->AlphaBlending |= InModifierInfo.AlphaBlend;
				CurrentState->MaterialPasses[PassesUsed]->TwoSided      |= InModifierInfo.TwoSided;
			}
			else
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("No stages left for constant color modifier."),
					InMaterial);
				return 0;
			}
		}
	}

	// Detail textures.
	if( RenDev->DetailTextures && CurrentState->UseDetailTexturing && DetailMaterial )
	{
		FPixoModifierInfo DetailModifierInfo = InModifierInfo;
		DetailModifierInfo.SetDetailTextureScale( DetailScale );
		UBitmapMaterial* DetailBitmap = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, DetailMaterial, &DetailModifierInfo);
		if( DetailBitmap )
			HandleDetail( DetailBitmap, PassesUsed, StagesUsed, DetailModifierInfo );
	}

	CurrentState->MaterialPasses[PassesUsed]->StagesUsed = StagesUsed;
	StagesUsed = 0;
	PassesUsed++;
	CurrentState->NumMaterialPasses = PassesUsed;

	return 1;
	unguard;
}

/*----------------------------------------------------------------------------
	SetParticleMaterial.
----------------------------------------------------------------------------*/

UBOOL FPixoRenderInterface::SetParticleMaterial( UParticleMaterial* ParticleMaterial, FPixoModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial )
{
	guard(FPixoRenderInterface::SetParticleMaterial);

	INT StagesUsed          = 0;
	INT PassesUsed          = 0;
	INT NumProjectors       = ParticleMaterial->AcceptsProjectors ? ParticleMaterial->NumProjectors : 0;
	UBOOL UseSpecialBlend   = 0;

	// Projectors on particles override blending between subdivisions for now.
	if( RenDev->NumTextureUnits == 2 )
		ParticleMaterial->BlendBetweenSubdivisions = 0;

	switch ( ParticleMaterial->ParticleBlending )
	{
	case PTDS_Regular :
		CurrentState->MaterialPasses[0]->SrcBlend           = GL_ONE;
		CurrentState->MaterialPasses[0]->DestBlend          = GL_ZERO;
		CurrentState->MaterialPasses[0]->AlphaBlending      = 0;
		break;
	case PTDS_AlphaBlend:
		CurrentState->MaterialPasses[0]->SrcBlend           = GL_SRC_ALPHA;
		CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE_MINUS_SRC_ALPHA;
		CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
		break;
	case PTDS_AlphaModulate_MightNotFogCorrectly:
		CurrentState->MaterialPasses[0]->SrcBlend           = GL_ONE;
		CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE_MINUS_SRC_ALPHA;
		CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
		CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
		CurrentState->MaterialPasses[0]->OverriddenFogColor = FColor( 0, 0, 0, 0 );
		break;
	case PTDS_Modulated:
		CurrentState->MaterialPasses[0]->SrcBlend           = GL_DST_COLOR;
		CurrentState->MaterialPasses[0]->DestBlend          = GL_SRC_COLOR;
		CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
		UseSpecialBlend                                     = 1;
		CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
		CurrentState->MaterialPasses[0]->OverriddenFogColor = FColor( 127, 127, 127, 0 );
		break;
	case PTDS_Translucent:
		CurrentState->MaterialPasses[0]->SrcBlend           = GL_ONE;
		CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE;
		CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
		CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
		CurrentState->MaterialPasses[0]->OverriddenFogColor = FColor( 0, 0, 0, 0 );
		break;
	case PTDS_Darken:
		CurrentState->MaterialPasses[0]->SrcBlend           = GL_ZERO;
		CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE_MINUS_SRC_COLOR;
		CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
		CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
		CurrentState->MaterialPasses[0]->OverriddenFogColor = FColor( 0, 0, 0, 0 );
		break;
	case PTDS_Brighten:
		CurrentState->MaterialPasses[0]->SrcBlend           = GL_ONE;
		CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE_MINUS_SRC_COLOR;
		CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
		CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
		CurrentState->MaterialPasses[0]->OverriddenFogColor = FColor( 0, 0, 0, 0 );
		break;
	}

	UBitmapMaterial* BitmapMaterial = ParticleMaterial->BitmapMaterial;
	FPixoTexture* Texture =  BitmapMaterial ? CacheTexture(BitmapMaterial->Get(Viewport->CurrentTime,Viewport)->GetRenderInterface()) : NULL;

	CurrentState->MaterialPasses[0]->Stages[0].Texture = Texture;

	if ( BitmapMaterial )
	{
		switch( BitmapMaterial->UClampMode )
		{
		case TC_Wrap:   CurrentState->MaterialPasses[0]->Stages[0].TextureAddressU = GL_REPEAT; break;
		case TC_Clamp:  CurrentState->MaterialPasses[0]->Stages[0].TextureAddressU = GL_CLAMP_TO_EDGE;  break;
		}
		switch( BitmapMaterial->VClampMode )
		{
		case TC_Wrap:   CurrentState->MaterialPasses[0]->Stages[0].TextureAddressV = GL_REPEAT; break;
		case TC_Clamp:  CurrentState->MaterialPasses[0]->Stages[0].TextureAddressV = GL_CLAMP_TO_EDGE;  break;
		}
	}

	if( !ParticleMaterial->BlendBetweenSubdivisions )
	{
		CurrentState->MaterialPasses[0]->Stages[0].SourceRgb0        = GL_TEXTURE;
		CurrentState->MaterialPasses[0]->Stages[0].SourceAlpha0        = GL_TEXTURE;
		if( ParticleMaterial->UseTFactor )
		{
			CurrentState->MaterialPasses[0]->Stages[0].SourceRgb1    = GL_CONSTANT;
			CurrentState->MaterialPasses[0]->Stages[0].SourceAlpha1    = GL_CONSTANT;
		}
		else
		{
			CurrentState->MaterialPasses[0]->Stages[0].SourceRgb1    = GL_PRIMARY_COLOR;
			CurrentState->MaterialPasses[0]->Stages[0].SourceAlpha1    = GL_PRIMARY_COLOR;
		}

		if( !UseSpecialBlend )
		{
			CurrentState->MaterialPasses[0]->Stages[0].ColorOp      = GL_MODULATE;
			CurrentState->MaterialPasses[0]->Stages[0].AlphaOp      = GL_MODULATE;
		}
		else
		{
			CurrentState->MaterialPasses[0]->Stages[0].ColorOp      = GL_INTERPOLATE;
			CurrentState->MaterialPasses[0]->Stages[0].AlphaOp      = GL_INTERPOLATE;

			CurrentState->MaterialPasses[0]->Stages[0].SourceRgb2    = GL_PRIMARY_COLOR;
			CurrentState->MaterialPasses[0]->Stages[0].OperandRgb2    = GL_SRC_ALPHA;
			CurrentState->MaterialPasses[0]->Stages[0].SourceAlpha2    = GL_PRIMARY_COLOR;
		}

		StagesUsed = 1;
	}
	else
	{
		// Color = C * ( (1-A) * T1 + A * T2 )
		// Alpha = (1-A) * T1 + A * T2
		CurrentState->MaterialPasses[0]->Stages[0].SourceRgb0    = GL_TEXTURE;
		CurrentState->MaterialPasses[0]->Stages[0].SourceAlpha0    = GL_TEXTURE;
		CurrentState->MaterialPasses[0]->Stages[0].ColorOp      = GL_REPLACE;
		CurrentState->MaterialPasses[0]->Stages[0].AlphaOp      = GL_REPLACE;

		CurrentState->MaterialPasses[0]->Stages[1].SourceRgb0    = GL_TEXTURE;
		CurrentState->MaterialPasses[0]->Stages[1].SourceRgb1    = GL_PREVIOUS;
		CurrentState->MaterialPasses[0]->Stages[1].SourceAlpha0    = GL_TEXTURE;
		CurrentState->MaterialPasses[0]->Stages[1].SourceAlpha1    = GL_PREVIOUS;
		CurrentState->MaterialPasses[0]->Stages[1].ColorOp      = GL_INTERPOLATE;
		CurrentState->MaterialPasses[0]->Stages[1].OperandRgb2    = GL_SRC_ALPHA;
		CurrentState->MaterialPasses[0]->Stages[1].SourceRgb2    = GL_PRIMARY_COLOR;
		CurrentState->MaterialPasses[0]->Stages[1].AlphaOp      = GL_INTERPOLATE;
		CurrentState->MaterialPasses[0]->Stages[1].OperandAlpha2    = GL_SRC_ALPHA;
		CurrentState->MaterialPasses[0]->Stages[1].SourceAlpha2    = GL_PRIMARY_COLOR;

		CurrentState->MaterialPasses[0]->Stages[2].SourceRgb0    = GL_PREVIOUS;
		CurrentState->MaterialPasses[0]->Stages[2].SourceRgb1    = GL_PRIMARY_COLOR;
		CurrentState->MaterialPasses[0]->Stages[2].ColorOp      = GL_MODULATE;
		CurrentState->MaterialPasses[0]->Stages[2].SourceAlpha0    = GL_PREVIOUS;
		CurrentState->MaterialPasses[0]->Stages[2].AlphaOp      = GL_REPLACE;

		CurrentState->MaterialPasses[0]->Stages[1].Texture          = Texture;
		CurrentState->MaterialPasses[0]->Stages[1].TexCoordIndex    = TCS_Stream1;

		if ( BitmapMaterial )
		{
			switch( BitmapMaterial->UClampMode )
			{
			case TC_Wrap:   CurrentState->MaterialPasses[0]->Stages[0].TextureAddressU = GL_REPEAT; break;
			case TC_Clamp:  CurrentState->MaterialPasses[0]->Stages[0].TextureAddressU = GL_CLAMP_TO_EDGE;  break;
			}
			switch( BitmapMaterial->VClampMode )
			{
			case TC_Wrap:   CurrentState->MaterialPasses[0]->Stages[0].TextureAddressV = GL_REPEAT; break;
			case TC_Clamp:  CurrentState->MaterialPasses[0]->Stages[0].TextureAddressV = GL_CLAMP_TO_EDGE;  break;
			}
		}

		StagesUsed = 3;
	}

	// Figure out how many projectors we can use.
	NumProjectors = Clamp<INT>( NumProjectors, 0, RenDev->NumTextureUnits - StagesUsed );

	// Projectors.
	for( INT ProjIndex=0; ProjIndex<NumProjectors; ProjIndex++ )
	{
		UBitmapMaterial* ProjMaterial = ParticleMaterial->Projectors[ProjIndex].BitmapMaterial;
		FPixoTexture*    ProjTexture  = ProjMaterial ? CacheTexture(ProjMaterial->Get(Viewport->CurrentTime,Viewport)->GetRenderInterface()) : NULL;

		if( ProjMaterial )
		{
			switch( ProjMaterial->UClampMode )
			{
			case TC_Wrap:   CurrentState->MaterialPasses[0]->Stages[1].TextureAddressU = GL_REPEAT;  break;
			case TC_Clamp:  CurrentState->MaterialPasses[0]->Stages[1].TextureAddressU = GL_CLAMP_TO_EDGE; break;
			}
			switch( ProjMaterial->VClampMode )
			{
			case TC_Wrap:   CurrentState->MaterialPasses[0]->Stages[1].TextureAddressV = GL_REPEAT;  break;
			case TC_Clamp:  CurrentState->MaterialPasses[0]->Stages[1].TextureAddressV = GL_CLAMP_TO_EDGE; break;
			}
		}

		CurrentState->MaterialPasses[0]->Stages[StagesUsed].SourceRgb0   = GL_TEXTURE;
		CurrentState->MaterialPasses[0]->Stages[StagesUsed].SourceRgb1   = GL_PREVIOUS;
		CurrentState->MaterialPasses[0]->Stages[StagesUsed].SourceAlpha0   = GL_PREVIOUS;
		CurrentState->MaterialPasses[0]->Stages[StagesUsed].AlphaOp     = GL_REPLACE;
		CurrentState->MaterialPasses[0]->Stages[StagesUsed].Texture     = ProjTexture;

		switch( ParticleMaterial->Projectors[ProjIndex].BlendMode )
		{
		case PB_AlphaBlend:
			CurrentState->MaterialPasses[0]->Stages[StagesUsed].ColorOp     = GL_INTERPOLATE;
			CurrentState->MaterialPasses[0]->Stages[StagesUsed].SourceRgb2   = GL_TEXTURE;
			CurrentState->MaterialPasses[0]->Stages[StagesUsed].OperandRgb2   = GL_SRC_ALPHA;
			break;
		case PB_Add:
			// There is not really a good way to handle this case.
			CurrentState->MaterialPasses[0]->Stages[StagesUsed].ColorOp     = GL_MODULATE;
			CurrentState->MaterialPasses[0]->Stages[StagesUsed].ColorScale  = 2.0f;
			break;
		case PB_None:
		case PB_Modulate:
		default:
			CurrentState->MaterialPasses[0]->Stages[StagesUsed].ColorOp = GL_MODULATE;
		}

		CurrentState->MaterialPasses[0]->Stages[StagesUsed].TexGenMode              = TCS_WorldCoords;
		CurrentState->MaterialPasses[0]->Stages[StagesUsed].UseTexGenMatrix         = 1;
		CurrentState->MaterialPasses[0]->Stages[StagesUsed].TextureTransformMatrix  = ParticleMaterial->Projectors[ProjIndex].Matrix;
		CurrentState->MaterialPasses[0]->Stages[StagesUsed].TexCoordCount           = TCN_3DCoords;

		if( ParticleMaterial->Projectors[ProjIndex].Projected )
			CurrentState->MaterialPasses[0]->Stages[StagesUsed].TexGenProjected = 1;

		StagesUsed++;
	}

	CurrentState->MaterialPasses[0]->ZWrite     = ParticleMaterial->ZWrite;
	CurrentState->MaterialPasses[0]->ZTest      = ParticleMaterial->ZTest;
	CurrentState->MaterialPasses[0]->TwoSided   = ParticleMaterial->RenderTwoSided;
	CurrentState->MaterialPasses[0]->AlphaTest  = ParticleMaterial->AlphaTest;
	CurrentState->MaterialPasses[0]->AlphaRef   = ParticleMaterial->AlphaRef;
	CurrentState->MaterialPasses[0]->FillMode   = ParticleMaterial->Wireframe ? FM_Wireframe : FM_Solid;


	CurrentState->MaterialPasses[PassesUsed]->StagesUsed    = StagesUsed;
	CurrentState->NumMaterialPasses                         = ++PassesUsed;

	return 1;
	unguard;
}

/*----------------------------------------------------------------------------
	SetLightingOnlyMaterial.
----------------------------------------------------------------------------*/

void FPixoRenderInterface::SetLightingOnlyMaterial()
{
	guard(FPixoRenderInterface::SetLightingOnlyMaterial);
	INT StagesUsed = 0;
	INT PassesUsed = 0;

	HandleDiffuse_SP( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed++] );

	if( CurrentState->Lightmap )
		HandleLightmap_SP( CurrentState->MaterialPasses[PassesUsed]->Stages[StagesUsed++], CurrentState->Lightmap );

	CurrentState->MaterialPasses[PassesUsed]->StagesUsed = StagesUsed;
	StagesUsed = 0;
	PassesUsed++;
	CurrentState->NumMaterialPasses = PassesUsed;

	unguard;
}

/*----------------------------------------------------------------------------
	SetTerrainMaterial.
----------------------------------------------------------------------------*/

UBOOL FPixoRenderInterface::SetTerrainMaterial( UTerrainMaterial* TerrainMaterial, FPixoModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial )
{
	guard(FPixoRenderInterface::SetTerrainMaterial);

	if( TerrainMaterial->RenderMethod == RM_AlphaMap )
	{
		if( TerrainMaterial->UseFallback )
		{
			// Previous shader we tried wasn't renderable.  Try the Shader's FallbackMaterial.
			TerrainMaterial->Layers(0).Texture->UseFallback = 1;
			TerrainMaterial->Layers(0).Texture = TerrainMaterial->Layers(0).Texture->CheckFallback();
			TerrainMaterial->UseFallback = 0;
		}

		UBitmapMaterial* BitmapMaterial;
		FPixoModifierInfo TexModifier;
		TexModifier.ModifyTextureTransforms = 1;
		TexModifier.Matrix                  = TerrainMaterial->Layers(0).TextureMatrix;
		TexModifier.TexCoordSource          = TCS_WorldCoords;
		TexModifier.TexCoordCount           = TCN_3DCoords;
		if( (BitmapMaterial = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, TerrainMaterial->Layers(0).Texture, &TexModifier )) != NULL )
		{
			// This is a regular bitmap material.
			FPixoMaterialStateStage* Stage = &CurrentState->MaterialPasses[0]->Stages[0];

			// Alpha-blend with the framebuffer, using AlphaWeight as an alphamap
			if( TerrainMaterial->FirstPass )
			{
				CurrentState->MaterialPasses[0]->SrcBlend           = GL_ONE;
				CurrentState->MaterialPasses[0]->DestBlend          = GL_ZERO;
				CurrentState->MaterialPasses[0]->AlphaBlending      = 0;
				CurrentState->MaterialPasses[0]->AlphaTest          = 0;
			}
			else
			{
				CurrentState->MaterialPasses[0]->SrcBlend           = GL_SRC_ALPHA;
				CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE_MINUS_SRC_ALPHA;
				CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
				CurrentState->MaterialPasses[0]->AlphaTest          = 1;
			}

			CurrentState->MaterialPasses[0]->OverrideFogColor   = 0;
			CurrentState->MaterialPasses[0]->ZWrite             = 1;
			CurrentState->MaterialPasses[0]->ZTest              = 1;
			CurrentState->MaterialPasses[0]->TwoSided           = 0;
			CurrentState->MaterialPasses[0]->AlphaRef           = 0;

			// load the texture for the layer
			SetShaderBitmap( *Stage, BitmapMaterial );
			Stage->SourceRgb0 = GL_TEXTURE;
			Stage->SourceRgb1 = GL_PRIMARY_COLOR;
			Stage->SourceAlpha0 = GL_TEXTURE;
			Stage->ColorOp   = (CurrentState->UseStaticLighting || CurrentState->UseDynamicLighting) ? GL_MODULATE : GL_REPLACE;
			Stage->ColorScale= (CurrentState->UseStaticLighting || CurrentState->UseDynamicLighting) ? 2.0f : 1.0f;
			Stage->AlphaOp   = GL_REPLACE;
			ApplyTexModifier( *Stage, &TexModifier );
			Stage++;

			// use the alphamap's alpha for the layer
			SetShaderBitmap( *Stage, TerrainMaterial->Layers(0).AlphaWeight );
			Stage->SourceRgb0 = GL_PREVIOUS;
			Stage->ColorOp   = GL_REPLACE;
			Stage->SourceAlpha0 = GL_TEXTURE;
			Stage->AlphaOp   = BitmapMaterial->IsTransparent() ? GL_MODULATE : GL_REPLACE ;
			FPixoModifierInfo AlphaModifier;
			AlphaModifier.ModifyTextureTransforms = 1;
			AlphaModifier.TexCoordSource = TCS_Stream0;
			ApplyTexModifier( *Stage, &AlphaModifier );
			Stage++;

			CurrentState->MaterialPasses[0]->StagesUsed = Stage - &CurrentState->MaterialPasses[0]->Stages[0];
		}
		else
		{
			// This is a shader.
			DECLARE_STATIC_UOBJECT( UTexMatrix, LayerTexCoordModifier, {} );
			LayerTexCoordModifier->Material = TerrainMaterial->Layers(0).Texture;
			LayerTexCoordModifier->Matrix = TerrainMaterial->Layers(0).TextureMatrix;
			LayerTexCoordModifier->TexCoordSource = TCS_WorldCoords;

			DECLARE_STATIC_UOBJECT( UTexMatrix, AlphaTexCoordModifier, {} );
			AlphaTexCoordModifier->Material = TerrainMaterial->Layers(0).AlphaWeight;
			AlphaTexCoordModifier->Matrix = FMatrix::Identity;
			AlphaTexCoordModifier->TexCoordSource = TCS_Stream0;

			DECLARE_STATIC_UOBJECT( UOpacityModifier, OpacityModifier, { OpacityModifier->bOverrideTexModifier = 1; } );
			OpacityModifier->Material = LayerTexCoordModifier;
			OpacityModifier->Opacity = AlphaTexCoordModifier;

			UMaterial* InMaterial;
			if( TerrainMaterial->FirstPass )
				InMaterial = LayerTexCoordModifier;
			else
				InMaterial = OpacityModifier;

			FPixoModifierInfo ShaderModifierInfo;
			UShader* Shader;
			if( (Shader = CheckMaterial<UShader,MT_Shader>(this, InMaterial, &ShaderModifierInfo )) != NULL )
			{
				if( !SetShaderMaterial( Shader, ShaderModifierInfo, ErrorString, ErrorMaterial ) )
					return 0;
			}
			else
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("Terrain layers must be Shaders or a BitmapMaterials."),
					TerrainMaterial->Layers(0).Texture);
				return 0;
			}
		}
	}
	else
	{
		FPixoMaterialStateStage* Stage = &CurrentState->MaterialPasses[0]->Stages[0];

		// Add to the framebuffer, using AlphaWeight as a weightmap

		// Set framebuffer blending.
		CurrentState->MaterialPasses[0]->SrcBlend               = GL_ONE;
		if( TerrainMaterial->FirstPass )
		{
			CurrentState->MaterialPasses[0]->DestBlend          = GL_ZERO;
			CurrentState->MaterialPasses[0]->OverrideFogColor   = 0;
			CurrentState->MaterialPasses[0]->ZWrite             = 1;
			CurrentState->MaterialPasses[0]->AlphaBlending      = 0;
		}
		else
		{
			CurrentState->MaterialPasses[0]->DestBlend          = GL_ONE;
			CurrentState->MaterialPasses[0]->OverrideFogColor   = 1;
			CurrentState->MaterialPasses[0]->OverriddenFogColor = FColor( 0, 0, 0, 0 );
			CurrentState->MaterialPasses[0]->ZWrite             = 0;
			CurrentState->MaterialPasses[0]->AlphaBlending      = 1;
		}
		CurrentState->MaterialPasses[0]->AlphaTest      = 0;
		CurrentState->MaterialPasses[0]->ZTest          = 1;
		CurrentState->MaterialPasses[0]->TwoSided       = 0;
		CurrentState->MaterialPasses[0]->AlphaRef       = 0;

		// We're rendering with a combined weightmap and a pixelshader
		if( TerrainMaterial->RenderMethod == RM_CombinedWeightMap )
		{
			FPixoModifierInfo TexModifier, WeightMapModifier;

			SetShaderBitmap( *Stage, TerrainMaterial->Layers(0).AlphaWeight );
			WeightMapModifier.ModifyTextureTransforms = 1;
			WeightMapModifier.TexCoordSource = TCS_Stream0;
			ApplyTexModifier( *Stage, &WeightMapModifier );
			Stage++;

			TexModifier.ModifyTextureTransforms = 1;
			TexModifier.Matrix = TerrainMaterial->Layers(0).TextureMatrix;
			TexModifier.TexCoordSource = TCS_WorldCoords;
			SetShaderBitmap( *Stage, CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, TerrainMaterial->Layers(0).Texture, &TexModifier ) );
			ApplyTexModifier( *Stage, &TexModifier );
			Stage++;

			if( TerrainMaterial->Layers.Num() > 1 )
			{
				TexModifier = FPixoModifierInfo();
				TexModifier.ModifyTextureTransforms = 1;
				TexModifier.Matrix = TerrainMaterial->Layers(1).TextureMatrix;
				TexModifier.TexCoordSource = TCS_WorldCoords;
				SetShaderBitmap( *Stage, CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, TerrainMaterial->Layers(1).Texture, &TexModifier ) );
				ApplyTexModifier( *Stage, &TexModifier );
				Stage++;

				if( TerrainMaterial->Layers.Num() > 2 )
				{
					TexModifier = FPixoModifierInfo();
					TexModifier.ModifyTextureTransforms = 1;
					TexModifier.Matrix = TerrainMaterial->Layers(2).TextureMatrix;
					TexModifier.TexCoordSource = TCS_WorldCoords;
					SetShaderBitmap( *Stage, CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, TerrainMaterial->Layers(2).Texture, &TexModifier ) );
					ApplyTexModifier( *Stage, &TexModifier );
					Stage++;

					if( TerrainMaterial->Layers.Num() > 3 )
					{
						TexModifier = FPixoModifierInfo();
						TexModifier.ModifyTextureTransforms = 1;
						TexModifier.Matrix = TerrainMaterial->Layers(3).TextureMatrix;
						TexModifier.TexCoordSource = TCS_WorldCoords;
						SetShaderBitmap( *Stage, CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this, TerrainMaterial->Layers(3).Texture, &TexModifier ) );
						ApplyTexModifier( *Stage, &TexModifier );
						Stage++;
					}
				}
			}

//!!vogel: TODO
#if 0
			switch( TerrainMaterial->Layers.Num() )
			{
			case 3:
				CurrentState->MaterialPasses[0]->PixelShader = PS_Terrain3Layer;
				break;
			case 4:
				CurrentState->MaterialPasses[0]->PixelShader = PS_Terrain4Layer;
				break;
			default:
				appErrorf(TEXT("too many layers at once"));
			}
#endif
		}
		else
		{
			check( 0 );
		}
	}
	CurrentState->NumMaterialPasses = 1;

	return 1;
	unguard;
}

/*----------------------------------------------------------------------------
	SetProjectorMaterial.
----------------------------------------------------------------------------*/

//
//  FProjectorCubeMapFace
//

class FProjectorCubeMapFace : public FTexture
{
public:

	FVector Base,
			FaceX,
			FaceY;
	INT     Size;

	// Constructor.

	FProjectorCubeMapFace(FVector InBase,FVector InFaceX,FVector InFaceY,INT InSize)
	{
		Base = InBase;
		FaceX = InFaceX;
		FaceY = InFaceY;
		Size = InSize;
	}

	// FRenderResource interface.

	virtual QWORD GetCacheId() { return 0; }
	virtual INT GetRevision() { return 1; }

	// FBaseTexture interface.

	virtual INT GetWidth() { return Size; }
	virtual INT GetHeight() { return Size; }
	virtual INT GetFirstMip() { return 0; }
	virtual INT GetNumMips() { return 2; }
	virtual ETextureFormat GetFormat() { return TEXF_RGBA8; }

	virtual ETexClampMode GetUClamp() { return TC_Clamp; }
	virtual ETexClampMode GetVClamp() { return TC_Clamp; }

	// FTexture interface

	virtual void* GetRawTextureData(INT MipIndex) { return NULL; }
	virtual void UnloadRawTextureData(INT MipIndex) {}

	void GetTextureData(INT MipIndex,void* Dest,INT DestStride,ETextureFormat DestFormat,UBOOL ColoredMips)
	{
		guard(FSpecularCubeMapFace::GetTextureData);

		check(DestFormat == TEXF_RGBA8);

		INT     MipSize = Size >> MipIndex;
		FVector DiffX = FaceX / (MipSize - 1),
				DiffY = FaceY / (MipSize - 1);

		for(INT Y = 0;Y < MipSize;Y++)
		{
			FColor* DestPtr = (FColor*) ((BYTE*)Dest + Y * DestStride);
			FVector Vector = Base + DiffY * (Y + 0.5f) + DiffX * 0.5f;

			for(INT X = 0;X < MipSize;X++)
			{
				FVector Normal = Vector.SafeNormal();

				*DestPtr++ = FColor(
								Clamp(appFloor(Normal.Z * 255.0f),0,255),
								Clamp(appFloor(Normal.Z * 255.0f),0,255),
								Clamp(appFloor(Normal.Z * 255.0f),0,255),
								Clamp(appFloor(Normal.Z * 255.0f),0,255)
								);

				Vector += DiffX;
			}
		}

		unguard;
	}

	virtual UTexture* GetUTexture() { return NULL; }
};

//
//  FProjectorCubeMap
//

class FProjectorCubeMap : public FCubemap
{
public:

	FProjectorCubeMapFace   PositiveX,
							NegativeX,
							PositiveY,
							NegativeY,
							PositiveZ,
							NegativeZ;

	INT     Size;
	QWORD   CacheId;
	INT     Revision;

	// Constructor.

	FProjectorCubeMap(INT InSize) :
		PositiveX(FVector(+1,+1,+1),FVector(+0,+0,-2),FVector(+0,-2,+0),InSize),
		NegativeX(FVector(-1,+1,-1),FVector(+0,+0,+2),FVector(+0,-2,+0),InSize),
		PositiveY(FVector(-1,+1,-1),FVector(+2,+0,+0),FVector(+0,+0,+2),InSize),
		NegativeY(FVector(-1,-1,+1),FVector(+2,+0,+0),FVector(+0,+0,-2),InSize),
		PositiveZ(FVector(-1,+1,+1),FVector(+2,+0,+0),FVector(+0,-2,+0),InSize),
		NegativeZ(FVector(+1,+1,-1),FVector(-2,+0,+0),FVector(+0,-2,+0),InSize)
	{
		Size = InSize;
		CacheId = MakeCacheID(CID_RenderTexture);
		Revision = 1;
	}

	// FRenderResource interface.

	virtual QWORD GetCacheId() { return CacheId; }
	virtual INT GetRevision() { return Revision; }

	// FBaseTexture interface.

	virtual INT GetWidth() { return Size; }
	virtual INT GetHeight() { return Size; }
	virtual INT GetFirstMip() { return 0; }
	virtual INT GetNumMips() { return 2; }
	virtual ETextureFormat GetFormat() { return TEXF_RGBA8; }

	virtual ETexClampMode GetUClamp() { return TC_Clamp; }
	virtual ETexClampMode GetVClamp() { return TC_Clamp; }

	// FCubeMap interface.

	virtual FTexture* GetFace(INT FaceIndex)
	{
		switch(FaceIndex)
		{
		case 0:
			return &PositiveX;
		case 1:
			return &NegativeX;
		case 2:
			return &PositiveY;
		case 3:
			return &NegativeY;
		case 4:
			return &PositiveZ;
		case 5:
			return &NegativeZ;
		default:
			return NULL;
		};
	}
};

UBOOL FPixoRenderInterface::SetProjectorMaterial( UProjectorMaterial* ProjectorMaterial, FPixoModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial )
{
	guard(FPixoRenderInterface::SetProjectorMaterial);

	FPixoMaterialState* MaterialState = CurrentState->MaterialPasses[0];
	INT                     PassesUsed = 0,
							StagesUsed = 0,
							MaxTextureStages = RenDev->NumTextureUnits;

	// See if we have an alpha channel in the BaseMaterial.

	UShader*    BaseShader = Cast<UShader>(ProjectorMaterial->BaseMaterial);
	UTexture*   BaseTexture = Cast<UTexture>(ProjectorMaterial->BaseMaterial);
	UMaterial*  BaseDiffuse = ProjectorMaterial->BaseMaterial;
	UMaterial*  BaseOpacity = NULL;

	if(BaseTexture && (BaseTexture->bMasked || BaseTexture->bAlphaTexture))
		BaseOpacity = BaseTexture;
	else if(BaseShader)
	{
		BaseDiffuse = BaseShader->Diffuse;
		BaseOpacity = BaseShader->Opacity;
		MaterialState->TwoSided = BaseShader->TwoSided;
	}

	if(ProjectorMaterial->bStaticProjector)
	{
		// See if the projected texture is simple.

		FPixoModifierInfo   ProjectedModifier;
		UBitmapMaterial*    ProjectedBitmap = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this,ProjectorMaterial->Projected,&ProjectedModifier,0);

		if(ProjectedBitmap)
		{
			// Handle the base material.

			if(ProjectorMaterial->BaseMaterialBlending != PB_None && BaseDiffuse)
			{
				FPixoModifierInfo   BaseModifier        = InModifierInfo;
				BaseModifier.TexCoordSource             = TCS_Stream1;
				BaseModifier.ModifyTextureTransforms    = 1;
				if(!HandleCombinedMaterial(BaseDiffuse,PassesUsed,StagesUsed,BaseModifier,0,ErrorString,ErrorMaterial))
					return 0;
			}

			if( StagesUsed < MaxTextureStages )
			{
				SetShaderBitmap(MaterialState->Stages[StagesUsed],ProjectedBitmap);
				ProjectedModifier.ModifyTextureTransforms   = 1;
				ProjectedModifier.TexCoordCount             = TCN_3DCoords;
				ProjectedModifier.TexCoordProjected         = 1;
				ProjectedModifier.TexCoordSource            = TCS_Stream0;
				ApplyTexModifier(MaterialState->Stages[StagesUsed],&ProjectedModifier);

				if(ProjectorMaterial->bProjected)
					MaterialState->Stages[StagesUsed].TexGenProjected = 1;

				switch(ProjectorMaterial->BaseMaterialBlending)
				{
					case PB_AlphaBlend:
					{
						MaterialState->Stages[StagesUsed].ColorOp   = GL_INTERPOLATE;
						MaterialState->Stages[StagesUsed].SourceRgb2 = GL_TEXTURE;
						MaterialState->Stages[StagesUsed].OperandRgb2 = GL_SRC_ALPHA;
						MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
						MaterialState->Stages[StagesUsed].SourceRgb1 = GL_PREVIOUS;
						MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
						break;
					}
					case PB_Modulate:
					{
						MaterialState->Stages[StagesUsed].ColorOp   = GL_MODULATE;
						MaterialState->Stages[StagesUsed].ColorScale= 2.0f;
						MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
						MaterialState->Stages[StagesUsed].SourceRgb1 = GL_PREVIOUS;
						MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
						break;
					}
					case PB_None:
					default:
					{
						switch(ProjectorMaterial->FrameBufferBlending)
						{
							case PB_Add:
							{
								MaterialState->Stages[StagesUsed].ColorOp   = GL_MODULATE;
								MaterialState->Stages[StagesUsed].SourceRgb0 = GL_PRIMARY_COLOR;
								MaterialState->Stages[StagesUsed].OperandRgb0 = GL_SRC_ALPHA;
								MaterialState->Stages[StagesUsed].SourceRgb1 = GL_TEXTURE;
								MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
								MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
								break;
							}
							case PB_AlphaBlend:
							{
								MaterialState->Stages[StagesUsed].ColorOp   = GL_REPLACE;
								MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
								MaterialState->Stages[StagesUsed].AlphaOp   = GL_MODULATE;
								MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_PRIMARY_COLOR;
								MaterialState->Stages[StagesUsed].SourceAlpha1 = GL_TEXTURE;
								break;
							}
							case PB_Modulate:
							default:
							{
								MaterialState->Stages[StagesUsed].ColorOp   = GL_INTERPOLATE;
								MaterialState->Stages[StagesUsed].SourceRgb2 = GL_PRIMARY_COLOR;
								MaterialState->Stages[StagesUsed].OperandRgb2 = GL_SRC_ALPHA;
								MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
								MaterialState->Stages[StagesUsed].SourceRgb1 = GL_CONSTANT;
								MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
								MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
								MaterialState->TFactorColor = FColor(127,127,127,0);
								break;
							}
						};
						break;
					}
				};

				StagesUsed++;
			}
			else
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("No stages left for projected texture"),
					ProjectorMaterial);
				return 0;
			}
		}
		else
		{
			// Handle the projected texture.

			ProjectedModifier.ModifyTextureTransforms = 1;
			ProjectedModifier.TexCoordCount = TCN_3DCoords;
			ProjectedModifier.TexCoordProjected = 1;
			ProjectedModifier.TexCoordSource = TCS_Stream0;

			if(!HandleCombinedMaterial(ProjectorMaterial->Projected,PassesUsed,StagesUsed,ProjectedModifier,0,ErrorString,ErrorMaterial))
				return 0;

			if(ProjectorMaterial->BaseMaterialBlending != PB_None && BaseDiffuse)
			{
				// Handle the base material.

				FPixoModifierInfo   BaseModifier = InModifierInfo;
				UBitmapMaterial*    BaseBitmap = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this,BaseDiffuse,&BaseModifier,0);

				if(!BaseBitmap)
				{
					SET_ERROR_STRING_MATERIAL(
						TEXT("Either projected texture or base material must be simple."),
						ProjectorMaterial);
					return 0;
				}

				if(StagesUsed < MaxTextureStages)
				{
					SetShaderBitmap(MaterialState->Stages[StagesUsed],BaseBitmap);
					ApplyTexModifier(MaterialState->Stages[StagesUsed],&BaseModifier);
					MaterialState->Stages[StagesUsed].TexCoordIndex = 1;

					switch(ProjectorMaterial->BaseMaterialBlending)
					{
						case PB_AlphaBlend:
						{
							MaterialState->Stages[StagesUsed].ColorOp   = GL_INTERPOLATE;
							MaterialState->Stages[StagesUsed].SourceRgb2 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].OperandRgb2 = GL_SRC_ALPHA;
							MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].SourceRgb1 = GL_PREVIOUS;
							MaterialState->Stages[StagesUsed].AlphaOp   = GL_INTERPOLATE;
							MaterialState->Stages[StagesUsed].SourceAlpha2 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].OperandAlpha2 = GL_SRC_ALPHA;
							MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].SourceAlpha1 = GL_PREVIOUS;
							break;
						}
						case PB_Modulate:
						{
							MaterialState->Stages[StagesUsed].ColorOp   = GL_MODULATE;
							MaterialState->Stages[StagesUsed].ColorScale= 2.0f;
							MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].SourceRgb1 = GL_PREVIOUS;
							MaterialState->Stages[StagesUsed].AlphaOp   = GL_MODULATE;
							MaterialState->Stages[StagesUsed].AlphaScale= 2.0f;
							MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].SourceAlpha1 = GL_PREVIOUS;
							break;
						}
						case PB_None:
						default:
						{
							MaterialState->Stages[StagesUsed].ColorOp   = GL_REPLACE;
							MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
							MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
							break;
						}
					};

					StagesUsed++;
				}
				else
				{
					SET_ERROR_STRING_MATERIAL(
						TEXT("No stages left for base material"),
						BaseDiffuse);
					return 0;
				}
			}
		}

		if(BaseOpacity)
		{
			// Handle the base material opacity.

			if(StagesUsed < MaxTextureStages)
			{
				FPixoModifierInfo   BaseOpacityModifier = InModifierInfo;
				UBitmapMaterial*    BaseOpacityBitmap = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this,BaseOpacity);

				if(BaseOpacityBitmap)
				{
					SetShaderBitmap(MaterialState->Stages[StagesUsed],BaseOpacityBitmap);
					ApplyTexModifier(MaterialState->Stages[StagesUsed],&BaseOpacityModifier);
					MaterialState->Stages[StagesUsed].TexCoordIndex = 1;

					switch(ProjectorMaterial->FrameBufferBlending)
					{
						case PB_Add:
						{
							MaterialState->Stages[StagesUsed].ColorOp   = GL_MODULATE;
							MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].OperandRgb0 = GL_SRC_ALPHA;
							MaterialState->Stages[StagesUsed].SourceRgb1 = GL_PREVIOUS;
							MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
							MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
							break;
						}
						case PB_AlphaBlend:
						{
							MaterialState->Stages[StagesUsed].ColorOp   = GL_REPLACE;
							MaterialState->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
							MaterialState->Stages[StagesUsed].AlphaOp   = GL_MODULATE;
							MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].SourceAlpha1 = GL_PREVIOUS;
							break;
						}
						case PB_Modulate:
						default:
						{
							MaterialState->Stages[StagesUsed].ColorOp   = GL_INTERPOLATE;
							MaterialState->Stages[StagesUsed].SourceRgb2 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].OperandRgb2 = GL_SRC_ALPHA;
							MaterialState->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
							MaterialState->Stages[StagesUsed].SourceRgb1 = GL_CONSTANT;
							MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
							MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
							MaterialState->TFactorColor = FColor(127,127,127,0);
							break;
						}
					};

					StagesUsed++;
				}
				else
				{
					SET_ERROR_STRING_MATERIAL(
						TEXT("Base opacity isn't bitmap"),
						BaseOpacity);
					return 0;
				}
			}
			else
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("No stages left for base opacity"),
					ProjectorMaterial);
				return 0;
			}
		}

		if( (RenDev->NumTextureUnits > 2) && ((ProjectorMaterial->BaseMaterialBlending != PB_None || !ProjectedBitmap) && (ProjectorMaterial->bGradient || !ProjectorMaterial->bProjectOnBackfaces)) )
		{
			// Handle the vertex attenuation.

			if(StagesUsed < MaxTextureStages)
			{
				switch(ProjectorMaterial->FrameBufferBlending)
				{
					case PB_Add:
					{
						MaterialState->Stages[StagesUsed].ColorOp   = GL_MODULATE;
						MaterialState->Stages[StagesUsed].SourceRgb0 = GL_PRIMARY_COLOR;
						MaterialState->Stages[StagesUsed].OperandRgb0 = GL_SRC_ALPHA;
						MaterialState->Stages[StagesUsed].SourceRgb1 = GL_PREVIOUS;
						MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
						break;
					}
					case PB_AlphaBlend:
					{
						MaterialState->Stages[StagesUsed].ColorOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
						MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_PRIMARY_COLOR;
						MaterialState->Stages[StagesUsed].SourceAlpha1 = GL_PREVIOUS;
						break;
					}
					case PB_Modulate:
					default:
					{
						MaterialState->Stages[StagesUsed].ColorOp   = GL_INTERPOLATE;
						MaterialState->Stages[StagesUsed].SourceRgb2 = GL_PRIMARY_COLOR;
						MaterialState->Stages[StagesUsed].OperandRgb2 = GL_SRC_ALPHA;
						MaterialState->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
						MaterialState->Stages[StagesUsed].SourceRgb1 = GL_CONSTANT;
						MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
						MaterialState->TFactorColor = FColor(127,127,127,0);
						break;
					}
				};

				StagesUsed++;
			}
			else
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("No stages left for attenuation"),
					ProjectorMaterial);
				return 0;
			}
		}
	}
	else
	{
		// See if the projected texture is simple.
		FPixoModifierInfo   ProjectedModifier;
		UBitmapMaterial*    ProjectedBitmap = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this,ProjectorMaterial->Projected,&ProjectedModifier,0);

		if(ProjectedBitmap)
		{
			// Handle the base material.
			if(ProjectorMaterial->BaseMaterialBlending != PB_None && BaseDiffuse)
			{
				if(!HandleCombinedMaterial(BaseDiffuse,PassesUsed,StagesUsed,InModifierInfo,0,ErrorString,ErrorMaterial))
					return 0;
			}

			// Handle the projected texture.
			if(StagesUsed < MaxTextureStages)
			{
				SetShaderBitmap(MaterialState->Stages[StagesUsed],ProjectedBitmap);
				MaterialState->Stages[StagesUsed].TextureTransformMatrix    = ProjectorMaterial->Matrix * ProjectedModifier.Matrix;
				MaterialState->Stages[StagesUsed].TexCoordCount             = TCN_3DCoords;
				MaterialState->Stages[StagesUsed].TexGenMode                = TCS_WorldCoords;
				MaterialState->Stages[StagesUsed].UseTexGenMatrix           = 1;
				if(ProjectorMaterial->bProjected)
					MaterialState->Stages[StagesUsed].TexGenProjected       = 1;

				switch(ProjectorMaterial->BaseMaterialBlending)
				{
					case PB_AlphaBlend:
					{
						MaterialState->Stages[StagesUsed].ColorOp = GL_INTERPOLATE;
						MaterialState->Stages[StagesUsed].SourceRgb2 = GL_TEXTURE;
						MaterialState->Stages[StagesUsed].OperandRgb2 = GL_SRC_ALPHA;
						MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
						MaterialState->Stages[StagesUsed].SourceRgb1 = GL_PREVIOUS;
						MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
						break;
					}
					case PB_Modulate:
					{
						MaterialState->Stages[StagesUsed].ColorOp   = GL_MODULATE;
						MaterialState->Stages[StagesUsed].ColorScale= 2.0f;
						MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
						MaterialState->Stages[StagesUsed].SourceRgb1 = GL_PREVIOUS;
						MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
						break;
					}
					case PB_None:
					default:
					{
						MaterialState->Stages[StagesUsed].ColorOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
						MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
						break;
					}
				};

				StagesUsed++;
			}
			else
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("No stages left for projected texture"),
					ProjectorMaterial);
				return 0;
			}
		}
		else
		{
			// Handle the projected texture.
			ProjectedModifier.ModifyTextureTransforms   = 1;
			ProjectedModifier.TexCoordSource            = TCS_WorldCoords;
			ProjectedModifier.TexCoordCount             = TCN_3DCoords;
			ProjectedModifier.TexCoordProjected         = ProjectorMaterial->bProjected;
			ProjectedModifier.Matrix                    = ProjectorMaterial->Matrix;

			if(!HandleCombinedMaterial(ProjectorMaterial->Projected,PassesUsed,StagesUsed,ProjectedModifier,0,ErrorString,ErrorMaterial))
				return 0;

			if(ProjectorMaterial->BaseMaterialBlending != PB_None && BaseDiffuse)
			{
				// Handle the base material.

				FPixoModifierInfo   BaseModifier = InModifierInfo;
				UBitmapMaterial*    BaseBitmap = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this,BaseDiffuse,&BaseModifier,0);

				if(!BaseBitmap)
				{
					SET_ERROR_STRING_MATERIAL(
						TEXT("Either projected texture or base material must be simple."),
						ProjectorMaterial);
					return 0;
				}

				if(StagesUsed < MaxTextureStages)
				{
					SetShaderBitmap(MaterialState->Stages[StagesUsed],BaseBitmap);
					ApplyTexModifier(MaterialState->Stages[StagesUsed],&BaseModifier);

					switch(ProjectorMaterial->BaseMaterialBlending)
					{
						case PB_AlphaBlend:
						{
							MaterialState->Stages[StagesUsed].ColorOp   = GL_INTERPOLATE;
							MaterialState->Stages[StagesUsed].SourceRgb2 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].OperandRgb2 = GL_SRC_ALPHA;
							MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].SourceRgb1 = GL_PREVIOUS;
							MaterialState->Stages[StagesUsed].AlphaOp   = GL_INTERPOLATE;
							MaterialState->Stages[StagesUsed].SourceAlpha2 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].OperandAlpha2 = GL_SRC_ALPHA;
							MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].SourceAlpha1 = GL_PREVIOUS;
							break;
						}
						case PB_Modulate:
						{
							MaterialState->Stages[StagesUsed].ColorOp   = GL_MODULATE;
							MaterialState->Stages[StagesUsed].ColorScale= 2.0f;
							MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].SourceRgb1 = GL_PREVIOUS;
							MaterialState->Stages[StagesUsed].AlphaOp   = GL_MODULATE;
							MaterialState->Stages[StagesUsed].AlphaScale= 2.0f;
							MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].SourceAlpha1 = GL_PREVIOUS;
							break;
						}
						case PB_None:
						default:
						{
							MaterialState->Stages[StagesUsed].ColorOp   = GL_REPLACE;
							MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
							MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
							break;
						}
					};

					StagesUsed++;
				}
				else
				{
					SET_ERROR_STRING_MATERIAL(
						TEXT("No stages left for base material"),
						BaseDiffuse);
					return 0;
				}
			}
		}

		if(ProjectorMaterial->bGradient)
		{
			// Handle the gradient texture.
			if(StagesUsed < MaxTextureStages)
			{
				SetShaderBitmap(MaterialState->Stages[StagesUsed],ProjectorMaterial->Gradient);

				MaterialState->Stages[StagesUsed].TextureTransformMatrix    = ProjectedModifier.Matrix * ProjectedModifier.Matrix;
				MaterialState->Stages[StagesUsed].TexCoordCount             = TCN_3DCoords;
				MaterialState->Stages[StagesUsed].TexGenMode                = TCS_WorldCoords;
				MaterialState->Stages[StagesUsed].UseTexGenMatrix           = 1;

				switch(ProjectorMaterial->FrameBufferBlending)
				{
					case PB_Add:
					{
						MaterialState->Stages[StagesUsed].ColorOp   = GL_MODULATE;
						MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
						MaterialState->Stages[StagesUsed].OperandRgb0 = GL_SRC_ALPHA;
						MaterialState->Stages[StagesUsed].SourceRgb1 = GL_PREVIOUS;
						MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
						break;
					}
					case PB_AlphaBlend:
					{
						MaterialState->Stages[StagesUsed].ColorOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
						MaterialState->Stages[StagesUsed].AlphaOp   = GL_MODULATE;
						MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
						MaterialState->Stages[StagesUsed].SourceAlpha1 = GL_PREVIOUS;
						break;
					}
					case PB_Modulate:
					default:
					{
						MaterialState->Stages[StagesUsed].ColorOp   = GL_INTERPOLATE;
						MaterialState->Stages[StagesUsed].SourceRgb2 = GL_TEXTURE;
						MaterialState->Stages[StagesUsed].OperandRgb2 = GL_SRC_ALPHA;
						MaterialState->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
						MaterialState->Stages[StagesUsed].SourceRgb1 = GL_CONSTANT;
						MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
						MaterialState->TFactorColor = FColor(127,127,127,0);
						break;
					}
				};

				StagesUsed++;
			}
			else
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("No stages left for gradient texture"),
					ProjectorMaterial);
				return 0;
			}
		}

		if(ProjectorMaterial->bProjectOnAlpha && BaseOpacity)
		{
			// Handle the gradient texture.
			if(StagesUsed < MaxTextureStages)
			{
				FPixoModifierInfo   BaseOpacityModifier = InModifierInfo;
				UBitmapMaterial*    BaseOpacityBitmap = CheckMaterial<UBitmapMaterial,MT_BitmapMaterial>(this,BaseOpacity);

				if(BaseOpacityBitmap)
				{
					SetShaderBitmap(MaterialState->Stages[StagesUsed],BaseOpacityBitmap);
					ApplyTexModifier(MaterialState->Stages[StagesUsed],&BaseOpacityModifier);

					switch(ProjectorMaterial->FrameBufferBlending)
					{
						case PB_Add:
						{
							MaterialState->Stages[StagesUsed].ColorOp   = GL_MODULATE;
							MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].OperandRgb0 = GL_SRC_ALPHA;
							MaterialState->Stages[StagesUsed].SourceRgb1 = GL_PREVIOUS;
							MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
							MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
							break;
						}
						case PB_AlphaBlend:
						{
							MaterialState->Stages[StagesUsed].ColorOp   = GL_REPLACE;
							MaterialState->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
							MaterialState->Stages[StagesUsed].AlphaOp   = GL_MODULATE;
							MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].SourceAlpha1 = GL_PREVIOUS;
							break;
						}
						case PB_Modulate:
						default:
						{
							MaterialState->Stages[StagesUsed].ColorOp   = GL_INTERPOLATE;
							MaterialState->Stages[StagesUsed].SourceRgb2 = GL_TEXTURE;
							MaterialState->Stages[StagesUsed].OperandRgb2 = GL_SRC_ALPHA;
							MaterialState->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
							MaterialState->Stages[StagesUsed].SourceRgb1 = GL_CONSTANT;
							MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
							MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
							MaterialState->TFactorColor = FColor(127,127,127,0);
							break;
						}
					};

					StagesUsed++;
				}
				else
				{
					SET_ERROR_STRING_MATERIAL(
						TEXT("Base opacity isn't bitmap"),
						BaseOpacity);
					return 0;
				}
			}
			else
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("No stages left for base opacity"),
					ProjectorMaterial);
				return 0;
			}
		}

		if(!ProjectorMaterial->bProjectOnBackfaces)
		{
			static FProjectorCubeMap    ProjectorCubeMap(32);

			// Handle the lighting modulation.

			if(StagesUsed < MaxTextureStages)
			{
				// Calculate the camera to projected texture matrix.
				FMatrix CameraToTexture = CurrentState->WorldToCamera.Inverse() * ProjectorMaterial->Matrix;

				MaterialState->Stages[StagesUsed].Texture                   = CacheTexture(&ProjectorCubeMap);
				MaterialState->Stages[StagesUsed].UseTexGenMatrix           = 1;
				MaterialState->Stages[StagesUsed].TexGenMode                = TCS_ProjectorCoords;
				MaterialState->Stages[StagesUsed].TexCoordCount             = TCN_3DCoords;

				FCoords NormalCameraToTexture = ((FMatrix*)&CameraToTexture)->Coords();

				NormalCameraToTexture.XAxis.Normalize();
				NormalCameraToTexture.YAxis.Normalize();
				NormalCameraToTexture.ZAxis.Normalize();

				MaterialState->Stages[StagesUsed].TextureTransformMatrix.M[0][0] = NormalCameraToTexture.XAxis.X;
				MaterialState->Stages[StagesUsed].TextureTransformMatrix.M[1][0] = NormalCameraToTexture.XAxis.Y;
				MaterialState->Stages[StagesUsed].TextureTransformMatrix.M[2][0] = NormalCameraToTexture.XAxis.Z;
				MaterialState->Stages[StagesUsed].TextureTransformMatrix.M[3][0] = 0.0f;

				MaterialState->Stages[StagesUsed].TextureTransformMatrix.M[0][1] = NormalCameraToTexture.YAxis.X;
				MaterialState->Stages[StagesUsed].TextureTransformMatrix.M[1][1] = NormalCameraToTexture.YAxis.Y;
				MaterialState->Stages[StagesUsed].TextureTransformMatrix.M[2][1] = NormalCameraToTexture.YAxis.Z;
				MaterialState->Stages[StagesUsed].TextureTransformMatrix.M[3][1] = 0.0f;

				MaterialState->Stages[StagesUsed].TextureTransformMatrix.M[0][2] = -NormalCameraToTexture.ZAxis.X;
				MaterialState->Stages[StagesUsed].TextureTransformMatrix.M[1][2] = -NormalCameraToTexture.ZAxis.Y;
				MaterialState->Stages[StagesUsed].TextureTransformMatrix.M[2][2] = -NormalCameraToTexture.ZAxis.Z;
				MaterialState->Stages[StagesUsed].TextureTransformMatrix.M[3][2] = 0.0f;

				MaterialState->Stages[StagesUsed].TextureTransformMatrix.M[2][3] = 0.0f;
				MaterialState->Stages[StagesUsed].TextureTransformMatrix.M[3][3] = 1.0f;

				switch(ProjectorMaterial->FrameBufferBlending)
				{
					case PB_Add:
					{
						MaterialState->Stages[StagesUsed].ColorOp   = GL_MODULATE;
						MaterialState->Stages[StagesUsed].SourceRgb0 = GL_TEXTURE;
						MaterialState->Stages[StagesUsed].SourceRgb1 = GL_PREVIOUS;
						MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
						break;
					}
					case PB_AlphaBlend:
					{
						MaterialState->Stages[StagesUsed].ColorOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
						MaterialState->Stages[StagesUsed].AlphaOp   = GL_MODULATE;
						MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_TEXTURE;
						MaterialState->Stages[StagesUsed].SourceAlpha1 = GL_PREVIOUS;
						break;
					}
					case PB_Modulate:
					default:
					{
						MaterialState->Stages[StagesUsed].ColorOp   = GL_INTERPOLATE;
						MaterialState->Stages[StagesUsed].SourceRgb2 = GL_TEXTURE;
						MaterialState->Stages[StagesUsed].OperandRgb2 = GL_SRC_ALPHA;
						MaterialState->Stages[StagesUsed].SourceRgb0 = GL_PREVIOUS;
						MaterialState->Stages[StagesUsed].SourceRgb1 = GL_CONSTANT;
						MaterialState->Stages[StagesUsed].AlphaOp   = GL_REPLACE;
						MaterialState->Stages[StagesUsed].SourceAlpha0 = GL_PREVIOUS;
						MaterialState->TFactorColor = FColor(127,127,127,0);
						break;
					}
				};

				StagesUsed++;
			}
			else
			{
				SET_ERROR_STRING_MATERIAL(
					TEXT("No stages left for directional attenuation"),
					ProjectorMaterial);
				return 0;
			}
		}
	}

	// Handle frame buffer blending.

	MaterialState->TwoSided = ProjectorMaterial->bTwoSided;
	MaterialState->AlphaBlending = 1;
	MaterialState->ZWrite = 0;

	switch(ProjectorMaterial->FrameBufferBlending)
	{
		case PB_Add:
		{
			MaterialState->SrcBlend = GL_ONE;
			MaterialState->DestBlend = GL_ONE;
			MaterialState->OverrideFogColor = 1;
			MaterialState->OverriddenFogColor = FColor(0,0,0,0);
			break;
		}
		case PB_AlphaBlend:
		{
			MaterialState->SrcBlend = GL_SRC_ALPHA;
			MaterialState->DestBlend = GL_ONE_MINUS_SRC_ALPHA;
			break;
		}
		case PB_Modulate:
		default:
		{
			MaterialState->SrcBlend = GL_DST_COLOR;
			MaterialState->DestBlend = GL_SRC_COLOR;
			MaterialState->OverrideFogColor = 1;
			MaterialState->OverriddenFogColor = FColor(127,127,127,0);
			break;
		}
	};

	CurrentState->MaterialPasses[PassesUsed]->StagesUsed = StagesUsed;
	CurrentState->NumMaterialPasses = ++PassesUsed;

	if(PassesUsed == 1)
		return 1;
	else
		return 0;

	unguard;
}


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

