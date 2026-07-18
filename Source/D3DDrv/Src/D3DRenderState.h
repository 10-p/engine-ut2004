/*=============================================================================
	D3DRenderState.h: Unreal Direct3D deferred state header.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#ifndef HEADER_D3DRENDERSTATE
#define HEADER_D3DRENDERSTATE


enum ED3DRenderState
{
	RS_FILLMODE							= 0,
	RS_ZWRITEENABLE,
	RS_ALPHATESTENABLE,
	RS_SRCBLEND,
	RS_DESTBLEND,
	RS_CULLMODE,
	RS_ZFUNC,
	RS_ALPHAREF,
	RS_ALPHAFUNC,
	RS_ALPHABLENDENABLE,
	RS_FOGENABLE,
	RS_FOGCOLOR,
	RS_FOGSTART,
	RS_FOGEND,
	RS_ZBIAS,
	RS_STENCILENABLE,
	RS_STENCILFAIL,
	RS_STENCILZFAIL,
	RS_STENCILPASS,
	RS_STENCILFUNC,
	RS_STENCILREF,
	RS_STENCILMASK,
	RS_STENCILWRITEMASK,
	RS_TEXTUREFACTOR,
	RS_LIGHTING,
	RS_AMBIENT,
	RS_COLORVERTEX,
	RS_PATCHSEGMENTS,
	RS_MAX
};

enum ED3DTextureStateStage
{
	TSS_COLOROP							= 0,
	TSS_COLORARG1,
	TSS_COLORARG2,
	TSS_ALPHAOP,
	TSS_ALPHAARG1,
	TSS_ALPHAARG2,
	TSS_TEXCOORDINDEX,
	TSS_ADDRESSU,
	TSS_ADDRESSV,
	TSS_TEXTURETRANSFORMFLAGS,
	TSS_ADDRESSW,
	TSS_COLORARG0,
	TSS_ALPHAARG0,
	TSS_RESULTARG,
    TSS_MIPMAPLODBIAS, // sjs
	TSS_MAX
};


enum ED3DTransformState
{
    TS_TEXTURE0				= 0,	//!! vogel: this order is required.
    TS_TEXTURE1,
    TS_TEXTURE2,
    TS_TEXTURE3,
    TS_TEXTURE4,
    TS_TEXTURE5,
    TS_TEXTURE6,
    TS_TEXTURE7,
    TS_VIEW,
    TS_PROJECTION,
	TS_WORLD,						//!! macro in D3D
	TS_MAX
};

class FD3DDeferredState
{
public:
	void Init( UD3DRenderDevice* InRenDev );
	void Commit();

	void SetRenderState( ED3DRenderState State, DWORD Value );
	void SetTextureStageState( DWORD Stage, ED3DTextureStateStage State, DWORD Value );
	void SetVertexShader( DWORD Handle );
	void SetStreamSource( DWORD StreamNumber, IDirect3DVertexBuffer8* StreamData, DWORD Stride );
	void SetIndices( IDirect3DIndexBuffer8* pIndexData, DWORD BaseVertexIndex );
	void SetTexture( DWORD Stage, IDirect3DBaseTexture8* pTexture );
	void SetTransform( ED3DTransformState State, CONST D3DMATRIX* pMatrix );
	void SetLight( DWORD Index, CONST D3DLIGHT8* pLight );
	void LightEnable( DWORD LightIndex, BOOL bEnable );
	void DeleteVertexShader( DWORD Handle );
	void UnSetVertexBuffer( IDirect3DVertexBuffer8* StreamData );
	void SetPixelShader( DWORD Handle );
	void DeletePixelShader( DWORD Handle );
	
protected:
	struct FD3DInternalState
	{
		DWORD						RenderState[RS_MAX];
		DWORD						StageState[8][TSS_MAX];
		D3DMATRIX					Matrices[TS_MAX];
		DWORD						IsDirty_Matrices;

		DWORD						VertexShaderHandle;
		DWORD						PixelShaderHandle;
		
		struct FD3DDeferredVertexStream
		{
			IDirect3DVertexBuffer8*		StreamData;
			UINT						StreamStride;
		}							VertexStreams[16];

		IDirect3DIndexBuffer8*		IndexBufferData;
		UINT						IndexBufferBaseVertex;

		IDirect3DBaseTexture8*		Textures[8];
		D3DLIGHT8					Lights[8];
		BOOL						LightsEnabled[8];
	};

	UD3DRenderDevice*				RenDev;
	FD3DInternalState				WantedState;
	FD3DInternalState				HardwareState;
};

#endif
