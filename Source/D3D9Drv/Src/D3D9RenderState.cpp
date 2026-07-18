/*=============================================================================
	D3DRenderState.cpp: Unreal Direct3D deferred state implementation.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#include "D3D9Drv.h"

#define COMMIT_RENDERSTATE( a )																		\
	if( WantedState.RenderState[a] != HardwareState.RenderState[a] )								\
	{																								\
		RenDev->Direct3DDevice9->SetRenderState( D3D##a, WantedState.RenderState[a] );				\
		StateChanges++;																				\
	}
#define SET_RENDERSTATE( a, b )																		\
	{																								\
		RenDev->Direct3DDevice9->SetRenderState( D3D##a, b	);										\
		HardwareState.RenderState[a] = b;															\
	}

#define COMMIT_STAGESTATE( s, a )																	\
	if( WantedState.StageState[s][a] != HardwareState.StageState[s][a] )							\
	{																								\
		RenDev->Direct3DDevice9->SetTextureStageState( s, D3D##a, WantedState.StageState[s][a] );	\
		StateChanges++;																				\
	}
#define SET_STAGESTATE( s, a, b )																	\
	{																								\
		RenDev->Direct3DDevice9->SetTextureStageState( s, D3D##a, b );								\
		HardwareState.StageState[s][a] = b;															\
	}

#define COMMIT_SAMPLERSTATE( s, a )																	\
	if( WantedState.SamplerState[s][a] != HardwareState.SamplerState[s][a] )						\
	{																								\
		RenDev->Direct3DDevice9->SetSamplerState( s, D3D##a, WantedState.SamplerState[s][a] );		\
		StateChanges++;																				\
	}
#define SET_SAMPLERSTATE( s, a, b )																	\
	{																								\
		RenDev->Direct3DDevice9->SetSamplerState( s, D3D##a, b );									\
		HardwareState.SamplerState[s][a] = b;														\
	}
				
#define COMMIT_TRANSFORM( t )																		\
	if (   (WantedState.IsDirty_Matrices & (1 << t) )												\
		/*&& (WantedState.StageState[t][TSS_TEXTURETRANSFORMFLAGS] != D3DTTFF_DISABLE)*/			\
		&& appMemcmp( &WantedState.Matrices[t], &HardwareState.Matrices[t], sizeof(D3DMATRIX)) )	\
	{																								\
		RenDev->Direct3DDevice9->SetTransform( D3D##t, &WantedState.Matrices[t] );					\
		TransformChanges++;																			\
	}
#define SET_TRANSFORM( t, m )																		\
	{																								\
		RenDev->Direct3DDevice9->SetTransform( D3D##t, (D3DMATRIX*) &m );							\
		appMemcpy( &HardwareState.Matrices[t], &m, sizeof( FMatrix) );								\
	}


void FD3D9DeferredState::Init( UD3D9RenderDevice*	InRenDev )
{
	guard(FD3D9DeferredState::Init);
	RenDev = InRenDev;
	
	FLOAT	Dummy0 = 0.f;

	SET_RENDERSTATE( RS_FILLMODE			, D3DFILL_SOLID		);
	SET_RENDERSTATE( RS_ZWRITEENABLE		, TRUE				);
	SET_RENDERSTATE( RS_ALPHATESTENABLE		, FALSE				);
	SET_RENDERSTATE( RS_SRCBLEND			, D3DBLEND_ONE		);
	SET_RENDERSTATE( RS_DESTBLEND			, D3DBLEND_ZERO		);
	SET_RENDERSTATE( RS_CULLMODE			, D3DCULL_NONE		);
	SET_RENDERSTATE( RS_ZFUNC				, D3DCMP_LESSEQUAL	);
	SET_RENDERSTATE( RS_ALPHAREF			, 0					);
	SET_RENDERSTATE( RS_ALPHAFUNC			, D3DCMP_GREATER	);
	SET_RENDERSTATE( RS_ALPHABLENDENABLE	, FALSE				);
	SET_RENDERSTATE( RS_FOGENABLE			, FALSE				);
	SET_RENDERSTATE( RS_FOGCOLOR			, 0					);
	SET_RENDERSTATE( RS_FOGSTART			, *((DWORD*)&Dummy0));
	SET_RENDERSTATE( RS_FOGEND				, *((DWORD*)&Dummy0));
	SET_RENDERSTATE( RS_DEPTHBIAS			, 0					);
	SET_RENDERSTATE( RS_SLOPESCALEDEPTHBIAS	, 0					);
	SET_RENDERSTATE( RS_STENCILENABLE		, (RenDev->UseStencil || GIsEditor) ? TRUE : FALSE	);
	SET_RENDERSTATE( RS_STENCILFAIL			, D3DSTENCILOP_KEEP );
	SET_RENDERSTATE( RS_STENCILZFAIL		, D3DSTENCILOP_KEEP );
	SET_RENDERSTATE( RS_STENCILPASS			, D3DSTENCILOP_KEEP );
	SET_RENDERSTATE( RS_STENCILFUNC			, D3DCMP_ALWAYS		);
	SET_RENDERSTATE( RS_STENCILREF			, 0					);
	SET_RENDERSTATE( RS_STENCILMASK			, 0xFF				);
	SET_RENDERSTATE( RS_STENCILWRITEMASK	, 0xFF				);
	SET_RENDERSTATE( RS_TEXTUREFACTOR		, 0					);
	SET_RENDERSTATE( RS_LIGHTING			, 0					);
	SET_RENDERSTATE( RS_AMBIENT				, 0					);
	SET_RENDERSTATE( RS_COLORVERTEX			, TRUE				);

	SET_TRANSFORM( TS_VIEW					, FMatrix::Identity );
    SET_TRANSFORM( TS_PROJECTION			, FMatrix::Identity );
    SET_TRANSFORM( TS_TEXTURE0				, FMatrix::Identity );
    SET_TRANSFORM( TS_TEXTURE1				, FMatrix::Identity );
    SET_TRANSFORM( TS_TEXTURE2				, FMatrix::Identity );
    SET_TRANSFORM( TS_TEXTURE3				, FMatrix::Identity );
#ifndef _XBOX
    SET_TRANSFORM( TS_TEXTURE4				, FMatrix::Identity );
    SET_TRANSFORM( TS_TEXTURE5				, FMatrix::Identity );
    SET_TRANSFORM( TS_TEXTURE6				, FMatrix::Identity );
    SET_TRANSFORM( TS_TEXTURE7				, FMatrix::Identity );
    SET_TRANSFORM( TS_WORLD					, FMatrix::Identity );
#endif

	for( DWORD StageIndex=0; StageIndex<RenDev->DeviceCaps9.MaxTextureBlendStages; StageIndex++ )
	{
		SET_STAGESTATE( StageIndex, TSS_COLOROP					, D3DTOP_DISABLE					);
		SET_STAGESTATE( StageIndex, TSS_COLORARG1				, D3DTA_TEXTURE						);
		SET_STAGESTATE( StageIndex, TSS_COLORARG2				, D3DTA_CURRENT						);
		SET_STAGESTATE( StageIndex, TSS_ALPHAOP					, D3DTOP_DISABLE					);
		SET_STAGESTATE( StageIndex, TSS_ALPHAARG1				, D3DTA_DIFFUSE						);
		SET_STAGESTATE( StageIndex, TSS_ALPHAARG2				, D3DTA_CURRENT						);
		SET_STAGESTATE( StageIndex, TSS_TEXCOORDINDEX			, D3DTSS_TCI_PASSTHRU				);
		SET_STAGESTATE( StageIndex, TSS_TEXTURETRANSFORMFLAGS	, D3DTTFF_DISABLE					);
		SET_STAGESTATE( StageIndex, TSS_COLORARG0				, D3DTA_CURRENT						);
		SET_STAGESTATE( StageIndex, TSS_ALPHAARG0				, D3DTA_CURRENT						);
		SET_STAGESTATE( StageIndex, TSS_RESULTARG				, D3DTA_CURRENT						);

		SET_SAMPLERSTATE( StageIndex, SAMP_ADDRESSU				, RenDev->CubemapTextureAddressing 	);
		SET_SAMPLERSTATE( StageIndex, SAMP_ADDRESSV				, RenDev->CubemapTextureAddressing 	);
		SET_SAMPLERSTATE( StageIndex, SAMP_ADDRESSW				, RenDev->CubemapTextureAddressing 	);
        SET_SAMPLERSTATE( StageIndex, SAMP_MIPMAPLODBIAS		, RenDev->DefaultTexMipBias			);

		RenDev->Direct3DDevice9->SetTexture( StageIndex, NULL );
		HardwareState.Textures[StageIndex] = NULL;
	}

	//RenDev->Direct3DDevice9->GetVertexShader( &HardwareState.VertexShader );
	HardwareState.VertexShader = NULL;
	HardwareState.PixelShader = NULL;
	HardwareState.Decl = NULL;
	RenDev->Direct3DDevice9->SetFVF(D3DFVF_XYZ);
	RenDev->Direct3DDevice9->SetVertexShader( HardwareState.VertexShader );

	for( INT StreamIndex = 0; StreamIndex < Min<INT>(16,RenDev->DeviceCaps9.MaxStreams); StreamIndex++ )
	{
		RenDev->Direct3DDevice9->SetStreamSource( 
			StreamIndex, 
			NULL,
			0,
			0
		);
		HardwareState.VertexStreams[StreamIndex].StreamData		= NULL;
		HardwareState.VertexStreams[StreamIndex].StreamStride	= 0;
		HardwareState.VertexStreams[StreamIndex].StreamOffset	= 0;
	}

	RenDev->Direct3DDevice9->SetIndices( NULL );
	HardwareState.IndexBufferData		= NULL;
	
	for( INT LightIndex = 0; LightIndex < 8; LightIndex++ )
	{
		appMemzero( &HardwareState.Lights[LightIndex], sizeof(HardwareState.Lights[LightIndex]) );
		appMemzero( &HardwareState.LightsEnabled[LightIndex], sizeof(HardwareState.LightsEnabled[LightIndex]) );
		//RenDev->Direct3DDevice9->GetLight( LightIndex, &HardwareState.Lights[LightIndex] );
		//RenDev->Direct3DDevice9->GetLightEnable( LightIndex, &HardwareState.LightsEnabled[LightIndex] );
	}
	
	appMemcpy( &WantedState, &HardwareState, sizeof(FD3D9InternalState) );
	unguard;
}

void FD3D9DeferredState::Commit()
{
	guard(FD3D9InternalState::Commit);
	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_StateChangeCycles));
	
	// Stats.
	INT TextureChanges		= 0,
		LightSetChanges		= 0,
		LightChanges		= 0,
		StateChanges		= 0,
		TransformChanges	= 0,
		StreamSourceChanges	= 0;

	// Kyro specific optimization.
	//!!vogel: TODO: benchmark when particles use alphatest
	if( RenDev->IsKyro && (WantedState.RenderState[ RS_ALPHAREF ] == 0) )
		WantedState.RenderState[ RS_ALPHATESTENABLE ] = 0;
	
	COMMIT_RENDERSTATE( RS_FILLMODE				);
	COMMIT_RENDERSTATE( RS_ZWRITEENABLE			);
	COMMIT_RENDERSTATE( RS_ALPHATESTENABLE		);
	COMMIT_RENDERSTATE( RS_SRCBLEND				);
	COMMIT_RENDERSTATE( RS_DESTBLEND			);
	COMMIT_RENDERSTATE( RS_CULLMODE				);
	COMMIT_RENDERSTATE( RS_ZFUNC				);
	COMMIT_RENDERSTATE( RS_ALPHAREF				);
	COMMIT_RENDERSTATE( RS_ALPHAFUNC			);
	COMMIT_RENDERSTATE( RS_ALPHABLENDENABLE		);
	COMMIT_RENDERSTATE( RS_FOGENABLE			);
	// Avoid unnecessary fog state changes - especially for Kyro cards.
	if( WantedState.RenderState[ RS_FOGENABLE ] )
	{
		COMMIT_RENDERSTATE( RS_FOGCOLOR			);
		COMMIT_RENDERSTATE( RS_FOGSTART			);
		COMMIT_RENDERSTATE( RS_FOGEND			);
	}
	else
	{
		// Forget we ever tried to set it.
		WantedState.RenderState[ RS_FOGCOLOR ] = HardwareState.RenderState[ RS_FOGCOLOR ];
		WantedState.RenderState[ RS_FOGSTART ] = HardwareState.RenderState[ RS_FOGSTART ];
		WantedState.RenderState[ RS_FOGEND   ] = HardwareState.RenderState[ RS_FOGEND   ];
	}
	COMMIT_RENDERSTATE( RS_DEPTHBIAS			);
	COMMIT_RENDERSTATE( RS_SLOPESCALEDEPTHBIAS	);
	if( RenDev->UseStencil || GIsEditor )
	{
		COMMIT_RENDERSTATE( RS_STENCILENABLE		);
		COMMIT_RENDERSTATE( RS_STENCILFAIL			);
		COMMIT_RENDERSTATE( RS_STENCILZFAIL			);
		COMMIT_RENDERSTATE( RS_STENCILPASS			);
		COMMIT_RENDERSTATE( RS_STENCILFUNC			);
		COMMIT_RENDERSTATE( RS_STENCILREF			);
		COMMIT_RENDERSTATE( RS_STENCILMASK			);
		COMMIT_RENDERSTATE( RS_STENCILWRITEMASK		);
	}
	COMMIT_RENDERSTATE( RS_TEXTUREFACTOR		);
	COMMIT_RENDERSTATE( RS_LIGHTING				);
	COMMIT_RENDERSTATE( RS_AMBIENT				);
	COMMIT_RENDERSTATE( RS_COLORVERTEX			);

	for( INT StageIndex=0; StageIndex<(INT)RenDev->DeviceCaps9.MaxTextureBlendStages; StageIndex++ )
	{
		COMMIT_STAGESTATE( StageIndex, TSS_COLOROP				);
		COMMIT_STAGESTATE( StageIndex, TSS_COLORARG1			);
		COMMIT_STAGESTATE( StageIndex, TSS_COLORARG2			);
		COMMIT_STAGESTATE( StageIndex, TSS_ALPHAOP				);
		COMMIT_STAGESTATE( StageIndex, TSS_ALPHAARG1			);
		COMMIT_STAGESTATE( StageIndex, TSS_ALPHAARG2			);
		COMMIT_STAGESTATE( StageIndex, TSS_TEXCOORDINDEX		);
		COMMIT_STAGESTATE( StageIndex, TSS_TEXTURETRANSFORMFLAGS);
		COMMIT_STAGESTATE( StageIndex, TSS_COLORARG0			);
		COMMIT_STAGESTATE( StageIndex, TSS_ALPHAARG0			);
		COMMIT_STAGESTATE( StageIndex, TSS_RESULTARG			);

		COMMIT_SAMPLERSTATE( StageIndex, SAMP_ADDRESSU			);
		COMMIT_SAMPLERSTATE( StageIndex, SAMP_ADDRESSV			);
		COMMIT_SAMPLERSTATE( StageIndex, SAMP_ADDRESSW			);
        COMMIT_SAMPLERSTATE( StageIndex, SAMP_MIPMAPLODBIAS		);
	}
	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_StateChangeCycles));

	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_TransformChangeCycles));
    COMMIT_TRANSFORM( TS_TEXTURE0				);
    COMMIT_TRANSFORM( TS_TEXTURE1				);
    COMMIT_TRANSFORM( TS_TEXTURE2				);
    COMMIT_TRANSFORM( TS_TEXTURE3				);
#ifndef _XBOX
    COMMIT_TRANSFORM( TS_TEXTURE4				);
    COMMIT_TRANSFORM( TS_TEXTURE5				);
    COMMIT_TRANSFORM( TS_TEXTURE6				);
    COMMIT_TRANSFORM( TS_TEXTURE7				);
#endif
	COMMIT_TRANSFORM( TS_VIEW					);
    COMMIT_TRANSFORM( TS_PROJECTION				);
	COMMIT_TRANSFORM( TS_WORLD					);
	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_TransformChangeCycles));
	
	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_TextureChangeCycles));
	for( INT StageIndex=0; StageIndex<Min<INT>(8,RenDev->DeviceCaps9.MaxTextureBlendStages); StageIndex++ )
	{
		if( WantedState.Textures[StageIndex] != HardwareState.Textures[StageIndex] )
		{
			RenDev->Direct3DDevice9->SetTexture( StageIndex, WantedState.Textures[StageIndex] );
			TextureChanges++;
		}
	}
	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_TextureChangeCycles));

	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_LightChangeCycles));
	for( INT LightIndex = 0; LightIndex < Min<INT>(8,RenDev->DeviceCaps9.MaxActiveLights); LightIndex++ )
	{
		if(WantedState.LightsEnabled[LightIndex])
		{
			RenDev->Direct3DDevice9->SetLight( LightIndex, &WantedState.Lights[LightIndex] );
			LightChanges++;
		}

		if( WantedState.LightsEnabled[LightIndex] != HardwareState.LightsEnabled[LightIndex] )
		{
			RenDev->Direct3DDevice9->LightEnable( LightIndex, WantedState.LightsEnabled[LightIndex] );
			LightSetChanges++;
		}
	}
	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_LightChangeCycles));

	if( WantedState.VertexShader != HardwareState.VertexShader )
		RenDev->Direct3DDevice9->SetVertexShader( WantedState.VertexShader );

	if( WantedState.Decl != HardwareState.Decl )
	{
		if( WantedState.Decl )
			RenDev->Direct3DDevice9->SetVertexDeclaration(WantedState.Decl);
		else
			RenDev->Direct3DDevice9->SetFVF(D3DFVF_XYZ);
	}

	for( INT StreamIndex = 0; StreamIndex < Min<INT>(16,RenDev->DeviceCaps9.MaxStreams); StreamIndex++ )
	{
		if( WantedState.VertexStreams[StreamIndex].StreamData != HardwareState.VertexStreams[StreamIndex].StreamData ||
			WantedState.VertexStreams[StreamIndex].StreamStride != HardwareState.VertexStreams[StreamIndex].StreamStride ||
			WantedState.VertexStreams[StreamIndex].StreamOffset != HardwareState.VertexStreams[StreamIndex].StreamOffset
		)
		{
			RenDev->Direct3DDevice9->SetStreamSource( 
				StreamIndex, 
				WantedState.VertexStreams[StreamIndex].StreamData,
				WantedState.VertexStreams[StreamIndex].StreamOffset,
				WantedState.VertexStreams[StreamIndex].StreamStride
			);
			StreamSourceChanges++;
		}
	}

	if( WantedState.PixelShader != HardwareState.PixelShader )
		RenDev->Direct3DDevice9->SetPixelShader( WantedState.PixelShader );

	if( WantedState.IndexBufferData != HardwareState.IndexBufferData )
		RenDev->Direct3DDevice9->SetIndices( WantedState.IndexBufferData );

	appMemcpy( &HardwareState, &WantedState, sizeof(FD3D9InternalState) );
	WantedState.IsDirty_Matrices = 0;

	GStats.DWORDStats(RenDev->D3DStats.STATS_TextureChanges		) += TextureChanges;
	GStats.DWORDStats(RenDev->D3DStats.STATS_LightSetChanges	) += LightSetChanges;
	GStats.DWORDStats(RenDev->D3DStats.STATS_LightChanges		) += LightChanges;
	GStats.DWORDStats(RenDev->D3DStats.STATS_StateChanges		) += StateChanges;
	GStats.DWORDStats(RenDev->D3DStats.STATS_TransformChanges	) += TransformChanges;
	GStats.DWORDStats(RenDev->D3DStats.STATS_StreamSourceChanges) += StreamSourceChanges;
	unguard;
}


void FD3D9DeferredState::SetRenderState( ED3DRenderState State, DWORD Value )
{
	WantedState.RenderState[State] = Value;
}
void FD3D9DeferredState::SetTextureStageState( DWORD Stage, ED3DTextureStateStage State, DWORD Value )
{
	WantedState.StageState[Stage][State]  = Value;
}
void FD3D9DeferredState::SetSamplerState( DWORD Stage, ED3DSamplerState State, DWORD Value )
{
	WantedState.SamplerState[Stage][State]  = Value;
}
void FD3D9DeferredState::SetDeclaration( IDirect3DVertexDeclaration9* Decl )
{
	WantedState.Decl = Decl;
}
void FD3D9DeferredState::SetVertexShader( IDirect3DVertexShader9* Shader )
{
	WantedState.VertexShader = Shader;
}
void FD3D9DeferredState::SetPixelShader( IDirect3DPixelShader9* Shader )
{
	WantedState.PixelShader = Shader;
}
void FD3D9DeferredState::SetStreamSource( DWORD StreamNumber, IDirect3DVertexBuffer9* StreamData, DWORD Offset, DWORD StreamStride )
{
	WantedState.VertexStreams[StreamNumber].StreamData   = StreamData;
	WantedState.VertexStreams[StreamNumber].StreamOffset = Offset;
	WantedState.VertexStreams[StreamNumber].StreamStride = StreamStride;
}
void FD3D9DeferredState::SetIndices( IDirect3DIndexBuffer9* IndexData)
{
	WantedState.IndexBufferData	= IndexData;
}
void FD3D9DeferredState::SetTexture( DWORD Stage, IDirect3DBaseTexture9* Texture )
{
	WantedState.Textures[Stage]	= Texture;
}
void FD3D9DeferredState::SetTransform( ED3DTransformState State, CONST D3DMATRIX* Matrix )
{
	appMemcpy( &WantedState.Matrices[State], Matrix, sizeof( D3DMATRIX ) );
	WantedState.IsDirty_Matrices |= (1 << State);
}
void FD3D9DeferredState::SetLight( DWORD Index, CONST D3DLIGHT9* Light )
{
	appMemcpy( &WantedState.Lights[Index], Light, sizeof(D3DLIGHT9) );
}
void FD3D9DeferredState::LightEnable( DWORD LightIndex, BOOL bEnable )
{
	WantedState.LightsEnabled[LightIndex] = bEnable;
}
void FD3D9DeferredState::DeleteVertexShader( IDirect3DVertexShader9* Shader )
{	
	if( HardwareState.VertexShader == Shader )
	{
		HardwareState.VertexShader	= NULL;
		HardwareState.Decl			= NULL;
		RenDev->Direct3DDevice9->SetFVF( D3DFVF_XYZ );
		RenDev->Direct3DDevice9->SetVertexShader( HardwareState.VertexShader );
	}
	SAFE_RELEASE( Shader );
}
void FD3D9DeferredState::DeletePixelShader( IDirect3DPixelShader9* Shader )
{	
	if( HardwareState.PixelShader == Shader )
	{
		HardwareState.PixelShader = NULL;
		RenDev->Direct3DDevice9->SetPixelShader( HardwareState.PixelShader );
	}
	SAFE_RELEASE( Shader );
}
void FD3D9DeferredState::UnSetVertexBuffer( IDirect3DVertexBuffer9* StreamData )
{
	for( INT StreamIndex = 0; StreamIndex < Min<INT>(16,RenDev->DeviceCaps9.MaxStreams); StreamIndex++ )
	{
		if( HardwareState.VertexStreams[StreamIndex].StreamData == StreamData )
		{
			HardwareState.VertexStreams[StreamIndex].StreamData   = NULL;
			HardwareState.VertexStreams[StreamIndex].StreamStride = 0;
			HardwareState.VertexStreams[StreamIndex].StreamOffset = 0;
			RenDev->Direct3DDevice9->SetStreamSource( StreamIndex, NULL, 0, 0 );

		}
		if( WantedState.VertexStreams[StreamIndex].StreamData == StreamData )
		{
			WantedState.VertexStreams[StreamIndex].StreamData   = NULL;
			WantedState.VertexStreams[StreamIndex].StreamStride = 0;
			WantedState.VertexStreams[StreamIndex].StreamOffset = 0;
		}
	}
}
