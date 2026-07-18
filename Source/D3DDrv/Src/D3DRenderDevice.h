/*=============================================================================
	D3DRenderDevice.h: Unreal Direct3D render device definition.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#ifndef HEADER_D3DRENDERDEVICE
#define HEADER_D3DRENDERDEVICE

#define VERTEXSHADER_ENVMAP 0

#define AUTO_INITIALIZE_REGISTRANTS_D3DDRV UD3DRenderDevice::StaticClass();

#include "XD3DHelper.h" // sjs
//
//	UD3DRenderDevice
//
class D3DDRV_API UD3DRenderDevice : public URenderDevice
{
	DECLARE_CLASS(UD3DRenderDevice,URenderDevice,CLASS_Config,D3DDrv);
public:

	xD3DHelper			xHelper; // sjs

	// Resource management.
	FD3DResource*					ResourceList;
	FD3DResource*					ResourceHash[4096];

	FD3DVertexShader*				VertexShaders;
	FD3DPixelShader*				PixelShaders;

	FD3DDynamicVertexStream*		DynamicVertexStream;
	FD3DDynamicIndexBuffer*			DynamicIndexBuffer16;
	FD3DDynamicIndexBuffer*			DynamicIndexBuffer32;
	TArray<FBaseTexture*>			UnusedTextures;
	TMap<PTRINT,INT>				UnusedTexturesNameMap;
	TArray<FString>					UnusedTextureNames;

	// Configuration.
	UBOOL							UsePrecaching,
									UseTrilinear,
									UseMipmapping,
									UseVSync,
									UseHardwareTL,
									UseHardwareVS,
									UseCubemaps,
									UseMippedCubemaps,
									UseDXT1,
									UseDXT3,
									UseDXT5,
									UseTripleBuffering,
									ReduceMouseLag,
									IsKyro,
									IsGeForce,
									UseVertexFog,
									UseRangeFog,
									CheckForOverflow,
									UseNPatches,
									DecompressTextures,
									AvoidHitches,
									ReportUnusedTextures,
									OverrideDesktopRefreshRate,
									HasNVCubemapBug;
	INT								AdapterNumber,
									FirstColoredMip,
									MaxPixelShaderVersion,
									LevelOfAnisotropy;
    FLOAT                           DetailTexMipBias, // sjs
									DefaultTexMipBias,
									TesselationFactor;
	DWORD							DesiredRefreshRate;									

	// Direct3D device info.
    D3DCAPS8						DeviceCaps8;
	D3DADAPTER_IDENTIFIER8			DeviceIdentifier;
	_WORD							wProduct,
									wVersion,
									wSubVersion,
									wBuild;
	TArray<D3DDISPLAYMODE>			DisplayModes;
	D3DFORMAT						BackBufferFormat;
	D3DFORMAT						DepthBufferFormat;
	D3DTEXTUREADDRESS				CubemapTextureAddressing;

	INT								MaxResWidth,
									MaxResHeight,
									PixelShaderVersion;	// 11 == 1.1, 14 == 1.4

	DWORD							InitialTextureMemory;

	TArray<D3DADAPTER_IDENTIFIER8>	Adapters;
	INT								BestAdapter;

	// Direct3D device state.
	UBOOL							ForceReset,
									CurrentFullscreen;
	UViewport*						LockedViewport;
	INT								CurrentColorBytes,
									FullScreenWidth,
									FullScreenHeight,
									FullScreenRefreshRate,
									FrameCounter,
									DesktopColorBits;

	// Direct3D interfaces.
    IDirect3D8*						Direct3D8;
    IDirect3DDevice8*				Direct3DDevice8;
	D3DPRESENT_PARAMETERS			PresentParms;

	// Editor specific stuff for windowed gamma.
	HDC								hDC;
	HWND							hWnd;

	// Our render interface.
	FD3DRenderInterface				RenderInterface;

	// Debugging data.
	TArray<BYTE>					StaticBuffer;

	// Deferred state.
	FD3DDeferredState				DeferredState;

	class FD3DStats
	{
	public:

		INT							STATS_FirstEntry,
									STATS_PushStateCalls,
									STATS_PushStateCycles,
									STATS_PopStateCalls,
									STATS_PopStateCycles,
									STATS_SetRenderTargetCalls,
									STATS_SetRenderTargetCycles,
									STATS_SetMaterialCalls,
									STATS_SetMaterialCycles,
									STATS_SetMaterialBlendingCalls,
									STATS_SetMaterialBlendingCycles,
									STATS_SetVertexStreamsCalls,
									STATS_SetVertexStreamsCycles,
									STATS_SetDynamicStreamCalls,
									STATS_SetDynamicStreamCycles,
									STATS_SetIndexBufferCalls,
									STATS_SetIndexBufferCycles,
									STATS_SetDynamicIndexBufferCalls,
									STATS_SetDynamicIndexBufferCycles,
									STATS_DrawPrimitiveCalls,
									STATS_DrawPrimitiveCycles,
									STATS_NumPrimitives,
									STATS_NumSVPVertices,
									STATS_LockCalls,
									STATS_LockCycles,
									STATS_UnlockCalls,
									STATS_UnlockCycles,
									STATS_PresentCalls,
									STATS_PresentCycles,
									STATS_DynamicVertexBytes,
									STATS_DynamicIndexBytes,
									STATS_NumTextures,
									STATS_TextureBytes,
									STATS_NumVertexStreams,
									STATS_VertexStreamBytes,
									STATS_NumIndexBuffers,
									STATS_IndexBufferBytes,
									STATS_StateChanges,
									STATS_StateChangeCycles,
									STATS_TextureChanges,
									STATS_TextureChangeCycles,
									STATS_TransformChanges,
									STATS_TransformChangeCycles,
									STATS_LightChanges,
									STATS_LightChangeCycles,
									STATS_LightSetChanges,
									STATS_LightSetCycles,
									STATS_DynamicVertexBufferLockCycles,
									STATS_DynamicVertexBufferLockCalls,
									STATS_DynamicVertexBufferDiscards,
									STATS_ClearCalls,
									STATS_ClearCycles,
									STATS_StreamSourceChanges,
									STATS_ResourceBytes,
									STATS_LastEntry;
		FD3DStats();
		void Init();
	} D3DStats;


	// Constructor/destructor.
	UD3DRenderDevice();

	void StaticConstructor();

	// GetCachedResource - Finds the cached copy of the given resource.  Returns NULL if failure.
	FD3DResource* GetCachedResource(QWORD CacheId);

	// FlushResource - Ensures that the given resource isn't being cached.
	void FlushResource(QWORD CacheId);

	// ResourceCached - Returns whether a resource is cached or not.
	UBOOL ResourceCached(QWORD CacheId);

	// GetVertexShader - Finds a vertex shader with the given type/declaration.  Creates a vertex shader if none is found.
	FD3DVertexShader* GetVertexShader(EVertexShader Type,FShaderDeclaration& Declaration);

	// GetPixelShader - Finds a Pixel shader with the given type.
	FD3DPixelShader* GetPixelShader(EPixelShader Type);

	// URenderDevice interface.
	virtual UBOOL Init();
	virtual void Exit(UViewport* Viewport);

	virtual UBOOL SetRes(UViewport* Viewport,INT NewX,INT NewY,UBOOL Fullscreen,INT ColorBytes=0,UBOOL bSaveSize=true);
	UBOOL UnSetRes( const TCHAR* Msg, HRESULT h, UBOOL Fatal = 0 );

	virtual void Flush(UViewport* Viewport);

	virtual void UpdateGamma(UViewport* Viewport);
	virtual void RestoreGamma();

	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);

	virtual FRenderInterface* Lock(UViewport* Viewport,BYTE* HitData,INT* HitSize);
	virtual void Unlock(FRenderInterface* RI);
	virtual void Present(UViewport* Viewport);

	virtual void ReadPixels(UViewport* Viewport,FColor* Pixels,UBOOL Flipped=0);

	virtual UBOOL SupportsTextureFormat( ETextureFormat );

	virtual void SetEmulationMode(EHardwareEmulationMode Mode);
	virtual FRenderCaps* GetRenderCaps();
};

#endif
