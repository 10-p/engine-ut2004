/*=============================================================================
	D3DRenderState.cpp: Unreal Direct3D deferred state implementation.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#include "D3DDrv.h"

#define COMMIT_RENDERSTATE( a )																		\
	if( WantedState.RenderState[a] != HardwareState.RenderState[a] )								\
	{																								\
		HardwareState.RenderState[a] = WantedState.RenderState[a];									\
		RenDev->Direct3DDevice8->SetRenderState( D3D##a, WantedState.RenderState[a] );				\
		StateChanges++;																				\
	}
#define SET_RENDERSTATE( a, b )																		\
	{																								\
		RenDev->Direct3DDevice8->SetRenderState( D3D##a, b	);										\
		HardwareState.RenderState[a] = b;															\
	}


#define COMMIT_STAGESTATE( s, a )																	\
	if( WantedState.StageState[s][a] != HardwareState.StageState[s][a] )							\
	{																								\
		HardwareState.StageState[s][a] = WantedState.StageState[s][a];								\
		RenDev->Direct3DDevice8->SetTextureStageState( s, D3D##a, WantedState.StageState[s][a] );	\
		StateChanges++;																				\
	}
#define SET_STAGESTATE( s, a, b )																	\
	{																								\
		RenDev->Direct3DDevice8->SetTextureStageState( s, D3D##a, b );								\
		HardwareState.StageState[s][a] = b;															\
	}
				
#define COMMIT_TRANSFORM( t )																		\
	if (   (WantedState.IsDirty_Matrices & (1 << t) )												\
		/*&& (WantedState.StageState[t][TSS_TEXTURETRANSFORMFLAGS] != D3DTTFF_DISABLE)	*/			\
		&& appMemcmp( &WantedState.Matrices[t], &HardwareState.Matrices[t], sizeof(D3DMATRIX)) )	\
	{																								\
		appMemcpy( &HardwareState.Matrices[t], &WantedState.Matrices[t], sizeof(D3DMATRIX));		\
		RenDev->Direct3DDevice8->SetTransform( D3D##t, &WantedState.Matrices[t] );					\
		TransformChanges++;																			\
	}
#define SET_TRANSFORM( t, m )																		\
	{																								\
		RenDev->Direct3DDevice8->SetTransform( D3D##t, (D3DMATRIX*) &m );							\
		appMemcpy( &HardwareState.Matrices[t], &m, sizeof( FMatrix) );								\
	}


void FD3DDeferredState::Init( UD3DRenderDevice*	InRenDev )
{
	guard(FD3DDeferredState::Init);
	RenDev = InRenDev;
	
	FLOAT	Dummy0 = 0.f,
			Dummy1 = 1.0f;

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
	SET_RENDERSTATE( RS_ZBIAS				, 0					);
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
	SET_RENDERSTATE( RS_PATCHSEGMENTS		, *((DWORD*)&Dummy1));


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

	for( DWORD StageIndex=0; StageIndex<RenDev->DeviceCaps8.MaxTextureBlendStages; StageIndex++ )
	{
		SET_STAGESTATE( StageIndex, TSS_COLOROP					, D3DTOP_DISABLE					);
		SET_STAGESTATE( StageIndex, TSS_COLORARG1				, D3DTA_TEXTURE						);
		SET_STAGESTATE( StageIndex, TSS_COLORARG2				, D3DTA_CURRENT						);
		SET_STAGESTATE( StageIndex, TSS_ALPHAOP					, D3DTOP_DISABLE					);
		SET_STAGESTATE( StageIndex, TSS_ALPHAARG1				, D3DTA_DIFFUSE						);
		SET_STAGESTATE( StageIndex, TSS_ALPHAARG2				, D3DTA_CURRENT						);
		SET_STAGESTATE( StageIndex, TSS_TEXCOORDINDEX			, D3DTSS_TCI_PASSTHRU				);
		SET_STAGESTATE( StageIndex, TSS_ADDRESSU				, RenDev->CubemapTextureAddressing	);
		SET_STAGESTATE( StageIndex, TSS_ADDRESSV				, RenDev->CubemapTextureAddressing	);
		SET_STAGESTATE( StageIndex, TSS_TEXTURETRANSFORMFLAGS	, D3DTTFF_DISABLE					);
		SET_STAGESTATE( StageIndex, TSS_ADDRESSW				, RenDev->CubemapTextureAddressing	);
        SET_STAGESTATE( StageIndex, TSS_MIPMAPLODBIAS		    , RenDev->DefaultTexMipBias			);
		SET_STAGESTATE( StageIndex, TSS_COLORARG0				, D3DTA_CURRENT						);
		SET_STAGESTATE( StageIndex, TSS_ALPHAARG0				, D3DTA_CURRENT						);
		SET_STAGESTATE( StageIndex, TSS_RESULTARG				, D3DTA_CURRENT						);

		RenDev->Direct3DDevice8->SetTexture( StageIndex, NULL );
		HardwareState.Textures[StageIndex] = NULL;
	}

	//RenDev->Direct3DDevice8->GetVertexShader( &HardwareState.VertexShaderHandle );
	HardwareState.VertexShaderHandle = D3DFVF_XYZ;
	HardwareState.PixelShaderHandle = 0;
	RenDev->Direct3DDevice8->SetVertexShader( HardwareState.VertexShaderHandle );

	for( INT StreamIndex = 0; StreamIndex < Min<INT>(16,RenDev->DeviceCaps8.MaxStreams); StreamIndex++ )
	{
		RenDev->Direct3DDevice8->SetStreamSource( 
			StreamIndex, 
			NULL,
			0
		);
		HardwareState.VertexStreams[StreamIndex].StreamData		= NULL;
		HardwareState.VertexStreams[StreamIndex].StreamStride	= 0;
	}

	RenDev->Direct3DDevice8->SetIndices( NULL, 0 );
	HardwareState.IndexBufferData		= NULL;
	HardwareState.IndexBufferBaseVertex	= 0;
	
	for( INT LightIndex = 0; LightIndex < 8; LightIndex++ )
	{
		appMemzero( &HardwareState.Lights[LightIndex], sizeof(HardwareState.Lights[LightIndex]) );
		appMemzero( &HardwareState.LightsEnabled[LightIndex], sizeof(HardwareState.LightsEnabled[LightIndex]) );
		//RenDev->Direct3DDevice8->GetLight( LightIndex, &HardwareState.Lights[LightIndex] );
		//RenDev->Direct3DDevice8->GetLightEnable( LightIndex, &HardwareState.LightsEnabled[LightIndex] );
	}
	
	appMemcpy( &WantedState, &HardwareState, sizeof(FD3DInternalState) );
	unguard;
}

void FD3DDeferredState::Commit()
{
	guard(FD3DInternalState::Commit);
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
	COMMIT_RENDERSTATE( RS_ZBIAS				);
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
	COMMIT_RENDERSTATE( RS_PATCHSEGMENTS		);

	for( INT StageIndex=0; StageIndex<(INT)RenDev->DeviceCaps8.MaxTextureBlendStages; StageIndex++ )
	{
		COMMIT_STAGESTATE( StageIndex, TSS_COLOROP				);
		COMMIT_STAGESTATE( StageIndex, TSS_COLORARG1			);
		COMMIT_STAGESTATE( StageIndex, TSS_COLORARG2			);
		COMMIT_STAGESTATE( StageIndex, TSS_ALPHAOP				);
		COMMIT_STAGESTATE( StageIndex, TSS_ALPHAARG1			);
		COMMIT_STAGESTATE( StageIndex, TSS_ALPHAARG2			);
		COMMIT_STAGESTATE( StageIndex, TSS_TEXCOORDINDEX		);
		COMMIT_STAGESTATE( StageIndex, TSS_ADDRESSU				);
		COMMIT_STAGESTATE( StageIndex, TSS_ADDRESSV				);
		COMMIT_STAGESTATE( StageIndex, TSS_TEXTURETRANSFORMFLAGS);
		COMMIT_STAGESTATE( StageIndex, TSS_ADDRESSW				);
		COMMIT_STAGESTATE( StageIndex, TSS_COLORARG0			);
		COMMIT_STAGESTATE( StageIndex, TSS_ALPHAARG0			);
		COMMIT_STAGESTATE( StageIndex, TSS_RESULTARG			);
        COMMIT_STAGESTATE( StageIndex, TSS_MIPMAPLODBIAS        ); // sjs
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
	for( INT StageIndex=0; StageIndex<Min<INT>(8,RenDev->DeviceCaps8.MaxTextureBlendStages); StageIndex++ )
	{
		if( WantedState.Textures[StageIndex] != HardwareState.Textures[StageIndex] )
		{
			HardwareState.Textures[StageIndex] = WantedState.Textures[StageIndex];
			RenDev->Direct3DDevice8->SetTexture( StageIndex, WantedState.Textures[StageIndex]);
			TextureChanges++;
		}
	}
	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_TextureChangeCycles));

	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_LightChangeCycles));
	for( INT LightIndex = 0; LightIndex < Min<INT>(8,RenDev->DeviceCaps8.MaxActiveLights); LightIndex++ )
	{
		if(WantedState.LightsEnabled[LightIndex])
		{
			appMemcpy( &HardwareState.Lights[LightIndex], &WantedState.Lights[LightIndex], sizeof(D3DLIGHT8) );
			RenDev->Direct3DDevice8->SetLight( LightIndex, &WantedState.Lights[LightIndex] );
			LightChanges++;
		}

		if( WantedState.LightsEnabled[LightIndex] != HardwareState.LightsEnabled[LightIndex] )
		{
			HardwareState.LightsEnabled[LightIndex] = WantedState.LightsEnabled[LightIndex];	
			RenDev->Direct3DDevice8->LightEnable( LightIndex, WantedState.LightsEnabled[LightIndex] );
			LightSetChanges++;
		}
	}
	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_LightChangeCycles));

	if( WantedState.VertexShaderHandle != HardwareState.VertexShaderHandle )
	{
		HardwareState.VertexShaderHandle = WantedState.VertexShaderHandle;
		RenDev->Direct3DDevice8->SetVertexShader( WantedState.VertexShaderHandle );
	}

	for( INT StreamIndex = 0; StreamIndex < Min<INT>(16,RenDev->DeviceCaps8.MaxStreams); StreamIndex++ )
	{
		if( WantedState.VertexStreams[StreamIndex].StreamData != HardwareState.VertexStreams[StreamIndex].StreamData ||
			WantedState.VertexStreams[StreamIndex].StreamStride != HardwareState.VertexStreams[StreamIndex].StreamStride
		)
		{
			HardwareState.VertexStreams[StreamIndex].StreamData	= WantedState.VertexStreams[StreamIndex].StreamData;
			HardwareState.VertexStreams[StreamIndex].StreamStride = WantedState.VertexStreams[StreamIndex].StreamStride;

			RenDev->Direct3DDevice8->SetStreamSource( 
				StreamIndex, 
				WantedState.VertexStreams[StreamIndex].StreamData,
				WantedState.VertexStreams[StreamIndex].StreamStride
			);
			StreamSourceChanges++;
		}
	}

	if( WantedState.PixelShaderHandle != HardwareState.PixelShaderHandle )
	{
		HardwareState.PixelShaderHandle = WantedState.PixelShaderHandle;
		RenDev->Direct3DDevice8->SetPixelShader( WantedState.PixelShaderHandle );
	}

	if( WantedState.IndexBufferData != HardwareState.IndexBufferData ||
		WantedState.IndexBufferBaseVertex != HardwareState.IndexBufferBaseVertex 
	)
	{
		HardwareState.IndexBufferData = WantedState.IndexBufferData;
		HardwareState.IndexBufferBaseVertex = WantedState.IndexBufferBaseVertex;
		RenDev->Direct3DDevice8->SetIndices( WantedState.IndexBufferData, WantedState.IndexBufferBaseVertex );
	}

	//appMemcpy( &HardwareState, &WantedState, sizeof(FD3DInternalState) );
	WantedState.IsDirty_Matrices = 0;

	GStats.DWORDStats(RenDev->D3DStats.STATS_TextureChanges		) += TextureChanges;
	GStats.DWORDStats(RenDev->D3DStats.STATS_LightSetChanges	) += LightSetChanges;
	GStats.DWORDStats(RenDev->D3DStats.STATS_LightChanges		) += LightChanges;
	GStats.DWORDStats(RenDev->D3DStats.STATS_StateChanges		) += StateChanges;
	GStats.DWORDStats(RenDev->D3DStats.STATS_TransformChanges	) += TransformChanges;
	GStats.DWORDStats(RenDev->D3DStats.STATS_StreamSourceChanges) += StreamSourceChanges;

	unguard;
}


void FD3DDeferredState::SetRenderState( ED3DRenderState State, DWORD Value )
{
	WantedState.RenderState[State] = Value;
}
void FD3DDeferredState::SetTextureStageState( DWORD Stage, ED3DTextureStateStage State, DWORD Value )
{
	WantedState.StageState[Stage][State]  = Value;
}
void FD3DDeferredState::SetVertexShader( DWORD Handle )
{
	WantedState.VertexShaderHandle = Handle;
}
void FD3DDeferredState::SetPixelShader( DWORD Handle )
{
	WantedState.PixelShaderHandle = Handle;
}
void FD3DDeferredState::SetStreamSource( DWORD StreamNumber, IDirect3DVertexBuffer8* StreamData, DWORD StreamStride )
{
	WantedState.VertexStreams[StreamNumber].StreamData   = StreamData;
	WantedState.VertexStreams[StreamNumber].StreamStride = StreamStride;
}
void FD3DDeferredState::SetIndices( IDirect3DIndexBuffer8* IndexData, DWORD BaseVertexIndex )
{
	WantedState.IndexBufferData	= IndexData;
	WantedState.IndexBufferBaseVertex = BaseVertexIndex;
}
void FD3DDeferredState::SetTexture( DWORD Stage, IDirect3DBaseTexture8* Texture )
{
	WantedState.Textures[Stage]	= Texture;
}
void FD3DDeferredState::SetTransform( ED3DTransformState State, CONST D3DMATRIX* Matrix )
{
	appMemcpy( &WantedState.Matrices[State], Matrix, sizeof( D3DMATRIX ) );
	WantedState.IsDirty_Matrices |= (1 << State);
}
void FD3DDeferredState::SetLight( DWORD Index, CONST D3DLIGHT8* Light )
{
	appMemcpy( &WantedState.Lights[Index], Light, sizeof(D3DLIGHT8) );
}
void FD3DDeferredState::LightEnable( DWORD LightIndex, BOOL bEnable )
{
	WantedState.LightsEnabled[LightIndex] = bEnable;
}
void FD3DDeferredState::DeleteVertexShader( DWORD Handle )
{	
	if( HardwareState.VertexShaderHandle == Handle )
	{
		HardwareState.VertexShaderHandle = D3DFVF_XYZ;
		RenDev->Direct3DDevice8->SetVertexShader( D3DFVF_XYZ );
	}
	HRESULT	Result = RenDev->Direct3DDevice8->DeleteVertexShader(Handle);
	if( FAILED(Result) )
		appErrorf(TEXT("DeleteVertexShader failed(%s)."),*D3DError(Result));
}
void FD3DDeferredState::DeletePixelShader( DWORD Handle )
{	
	if( HardwareState.PixelShaderHandle == Handle )
	{
		HardwareState.PixelShaderHandle = 0;
		RenDev->Direct3DDevice8->SetPixelShader( 0 );
	}
	HRESULT	Result = RenDev->Direct3DDevice8->DeletePixelShader(Handle);
	if( FAILED(Result) )
		appErrorf(TEXT("DeletePixelShader failed(%s)."),*D3DError(Result));
}
void FD3DDeferredState::UnSetVertexBuffer( IDirect3DVertexBuffer8* StreamData )
{
	for( INT StreamIndex = 0; StreamIndex < Min<INT>(16,RenDev->DeviceCaps8.MaxStreams); StreamIndex++ )
	{
		if( HardwareState.VertexStreams[StreamIndex].StreamData == StreamData )
		{
			HardwareState.VertexStreams[StreamIndex].StreamData   = NULL;
			HardwareState.VertexStreams[StreamIndex].StreamStride = 0;
			RenDev->Direct3DDevice8->SetStreamSource( StreamIndex, NULL, 0 );

		}
		if( WantedState.VertexStreams[StreamIndex].StreamData == StreamData )
		{
			WantedState.VertexStreams[StreamIndex].StreamData   = NULL;
			WantedState.VertexStreams[StreamIndex].StreamStride = 0;
		}
	}
}

