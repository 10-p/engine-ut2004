/*=============================================================================
	D3DRenderDevice.h: Unreal Direct3D render device definition.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#ifndef HEADER_D3D9RENDERDEVICE
#define HEADER_D3D9RENDERDEVICE

#define VERTEXSHADER_ENVMAP 0

#define AUTO_INITIALIZE_REGISTRANTS_D3D9DRV UD3D9RenderDevice::StaticClass();

#include "XD3D9Helper.h" // sjs
//
//	UD3D9RenderDevice
//
class D3D9DRV_API UD3D9RenderDevice : public URenderDevice
{
	DECLARE_CLASS(UD3D9RenderDevice,URenderDevice,CLASS_Config,D3D9Drv);
public:

	xD3D9Helper			xHelper; // sjs

	// Resource management.
	FD3D9Resource*					ResourceList;
	FD3D9Resource*					ResourceHash[4096];

	FD3D9VertexShader*				VertexShaders;
	FD3D9PixelShader*				PixelShaders;

	FD3D9DynamicVertexStream*		DynamicVertexStream;
	FD3D9DynamicIndexBuffer*			DynamicIndexBuffer16;
	FD3D9DynamicIndexBuffer*			DynamicIndexBuffer32;

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
    D3DCAPS9						DeviceCaps9;
	D3DADAPTER_IDENTIFIER9			DeviceIdentifier;
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

	TArray<D3DADAPTER_IDENTIFIER9>	Adapters;
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
    IDirect3D9*						Direct3D9;
    IDirect3DDevice9*				Direct3DDevice9;
	D3DPRESENT_PARAMETERS			PresentParms;

	// Our render interface.
	FD3D9RenderInterface			RenderInterface;

	// Debugging data.
	TArray<BYTE>					StaticBuffer;

	// Deferred state.
	FD3D9DeferredState				DeferredState;

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
	UD3D9RenderDevice();

	void StaticConstructor();

	// GetCachedResource - Finds the cached copy of the given resource.  Returns NULL if failure.
	FD3D9Resource* GetCachedResource(QWORD CacheId);

	// FlushResource - Ensures that the given resource isn't being cached.
	void FlushResource(QWORD CacheId);

	// ResourceCached - Returns whether a resource is cached or not.
	UBOOL ResourceCached(QWORD CacheId);

	// GetVertexShader - Finds a vertex shader with the given type/declaration.  Creates a vertex shader if none is found.
	FD3D9VertexShader* GetVertexShader(EVertexShader Type,FShaderDeclaration& Declaration);

	// GetPixelShader - Finds a Pixel shader with the given type.
	FD3D9PixelShader* GetPixelShader(EPixelShader Type);

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
