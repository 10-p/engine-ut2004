/*=============================================================================
	D3DRenderState.h: Unreal Direct3D deferred state header.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#ifndef HEADER_D3D9RENDERSTATE
#define HEADER_D3D9RENDERSTATE


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
	RS_DEPTHBIAS,
	RS_SLOPESCALEDEPTHBIAS,
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
	TSS_TEXTURETRANSFORMFLAGS,
	TSS_COLORARG0,
	TSS_ALPHAARG0,
	TSS_RESULTARG,
	TSS_MAX
};

enum ED3DSamplerState
{
	SAMP_ADDRESSU							= 0,
	SAMP_ADDRESSV,
	SAMP_ADDRESSW,
    SAMP_MIPMAPLODBIAS,
	SAMP_MAX
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

class FD3D9DeferredState
{
public:
	void Init( UD3D9RenderDevice* InRenDev );
	void Commit();

	void SetRenderState( ED3DRenderState State, DWORD Value );
	void SetTextureStageState( DWORD Stage, ED3DTextureStateStage State, DWORD Value );
	void SetSamplerState( DWORD Stage, ED3DSamplerState State, DWORD Value );
	void SetVertexShader( IDirect3DVertexShader9* Shader );
	void SetDeclaration( IDirect3DVertexDeclaration9* Decl );
	void SetStreamSource( DWORD StreamNumber, IDirect3DVertexBuffer9* StreamData, DWORD Offset, DWORD Stride );
	void SetIndices( IDirect3DIndexBuffer9* pIndexData );
	void SetTexture( DWORD Stage, IDirect3DBaseTexture9* pTexture );
	void SetTransform( ED3DTransformState State, CONST D3DMATRIX* pMatrix );
	void SetLight( DWORD Index, CONST D3DLIGHT9* pLight );
	void LightEnable( DWORD LightIndex, BOOL bEnable );
	void DeleteVertexShader( IDirect3DVertexShader9* Shader );
	void UnSetVertexBuffer( IDirect3DVertexBuffer9* StreamData );
	void SetPixelShader( IDirect3DPixelShader9* Shader );
	void DeletePixelShader( IDirect3DPixelShader9* Shader );
	
protected:
	struct FD3D9InternalState
	{
		DWORD						RenderState[RS_MAX];
		DWORD						StageState[8][TSS_MAX];
		DWORD						SamplerState[8][SAMP_MAX];
		D3DMATRIX					Matrices[TS_MAX];
		DWORD						IsDirty_Matrices;

		IDirect3DVertexShader9*			VertexShader;
		IDirect3DPixelShader9*			PixelShader;
		IDirect3DVertexDeclaration9*    Decl;
		
		struct FD3D9DeferredVertexStream
		{
			IDirect3DVertexBuffer9*		StreamData;
			UINT						StreamOffset;
			UINT						StreamStride;
		}							VertexStreams[16];

		IDirect3DIndexBuffer9*		IndexBufferData;

		IDirect3DBaseTexture9*		Textures[8];
		D3DLIGHT9					Lights[8];
		BOOL						LightsEnabled[8];
	};

	UD3D9RenderDevice*				RenDev;
	FD3D9InternalState				WantedState;
	FD3D9InternalState				HardwareState;
};

#endif
