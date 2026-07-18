/*=============================================================================
	PixoRenderDevice.h: Unreal Pixo render device definition.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Michael Sartain from GL driver
		* Taken over by Daniel Vogel

=============================================================================*/

#ifndef HEADER_PIXORENDERDEVICE
#define HEADER_PIXORENDERDEVICE

const float DefaultMipBias = -0.5f; // sjs

#ifndef PIXODRV_API
#define PIXODRV_API DLL_IMPORT
#endif

//
//  UPixoRenderDevice
//
class PIXODRV_API UPixoRenderDevice : public URenderDevice
{
	DECLARE_CLASS(UPixoRenderDevice,URenderDevice,CLASS_Config,PixoDrv);
public:

    static int                      cpu_features;

	PIXO_BUF                        *PixoBuffer;

	// pointer to out z buffer possibly offset to avoid 64k aliasing
	void                            *ZBuffer;
	// pointer to virtualalloc'd z buffer block
	void                            *ZBufferBlock;
	int                             ZBufferPitch;

    int                             VertexCacheCount;
    PIXO_VERT_CACHE                 *PixoVertexCache;

	// Our render interface.
	FPixoRenderInterface            RenderInterface;

	// Resource management.
	FPixoResource*                  ResourceList;
	FPixoResource*                  ResourceHash[4096];

	FPixoVertexShader*              VertexShaders;

	FPixoVertexStream*              DynamicVertexStream;
	FPixoIndexBuffer*               DynamicIndexBuffer;

	// DE's additions.
	FDynVertStream                  DE_DynamicVertexStream;

	// Variables.
	UViewport*                      LockedViewport;
	INT                             FrameCounter;

	// Configuration.
	FLOAT                           DetailTexMipBias;
	DWORD                           DesiredRefreshRate,
									FilterQuality3D,
									FilterQualityHUD;
	UBOOL                           FogEnabled,
									Zoom2X,
									LimitTextureSize,
									SimpleMaterials,
									UseVisibilityQuery;

	// Status/ system information.
	FRenderCaps                     RenderCaps;
	TArray<FPlane>                  Modes;
	INT                             NumTextureUnits;

	UBOOL                           ValidContext,
									WasFullscreen;

    static INT                      STATS_NumVertsXformed;
    static INT                      STATS_NumTrianglesSubmitted;
    static INT                      STATS_NumTrianglesDrawn;

    static UBOOL                    ShowDepthComplexity;

	// Constructor/destructor.
	UPixoRenderDevice();

	void StaticConstructor();

	// URenderDevice interface.
	virtual UBOOL Init();
	virtual void Exit(UViewport* Viewport);

	virtual UBOOL SetRes(UViewport* Viewport,INT NewX,INT NewY,UBOOL Fullscreen,INT ColorBytes=0,UBOOL bSaveSize=true);
	virtual void UnSetRes();

	// GetCachedResource - Finds the cached copy of the given resource.  Returns NULL if failure.
	FPixoResource* GetCachedResource(QWORD CacheId);

	// FlushResource - Ensures that the given resource isn't being cached.
	virtual void FlushResource( QWORD CacheId );

	// ResourceCached - Returns whether a resource is cached or not.
	UBOOL ResourceCached(QWORD CacheId);

	// GetVertexShader - Finds a vertex shader with the given type/declaration.  Creates a vertex shader if none is found.
	FPixoVertexShader* GetVertexShader(EVertexShader Type,FShaderDeclaration& Declaration);

	virtual void Flush(UViewport* Viewport);

	virtual void UpdateGamma(UViewport* Viewport);
	virtual void RestoreGamma();

	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);

	virtual FRenderInterface* Lock(UViewport* Viewport,BYTE* HitData,INT* HitSize);
	virtual void Unlock(FRenderInterface* RI);
	virtual void Present(UViewport* Viewport);

	virtual void ReadPixels(UViewport* Viewport,FColor* Pixels,UBOOL Flipped=0);

	virtual void SetEmulationMode(EHardwareEmulationMode Mode){};
	virtual FRenderCaps* GetRenderCaps();

    UBOOL AllocAndSetPixoVertexCache(int VerticesCount);
    void SetUpPixoBlendModeToShowComplexity();

	UBOOL CreatePixoBuffers(HWND hwnd, 
		UINT BufferWidth, UINT BufferHeight,
		PIXO_ZBUFFER_TYPE ZBufferType, int flags);
	void DestroyPixoBuffers();
};

// Transformation routines
void PixoTransformVector(float *out_xyzw, float *mat, float *in_xyz1);
void PixoTransformNormal(float *out_xyzw, float *mat, float *in_xyz0);

#endif
