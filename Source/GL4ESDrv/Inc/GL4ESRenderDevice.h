/*=============================================================================
	OpenGLRenderDevice.h: Unreal OpenGL render device definition.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#ifndef HEADER_GL4ESRENDERDEVICE
#define HEADER_GL4ESRENDERDEVICE

#define AUTO_INITIALIZE_REGISTRANTS_GL4ESDRV UGL4ESRenderDevice::StaticClass();

const float GL4ESDefaultMipBias = -0.5f; // sjs

#ifndef GL4ESDRV_API
#define GL4ESDRV_API DLL_IMPORT
#endif


//
//	UGL4ESRenderDevice
//
class GL4ESDRV_API UGL4ESRenderDevice : public URenderDevice
{
	DECLARE_CLASS(UGL4ESRenderDevice,URenderDevice,CLASS_Config,GL4ESDrv);
public:

	// Our render interface.
	FGL4ESRenderInterface			RenderInterface;

	// Resource management.
	FGL4ESResource*				ResourceList;
	FGL4ESResource*				ResourceHash[4096];

	FGL4ESVertexShader*			VertexShaders;
	FGL4ESPixelShader*				PixelShaders;

	FGL4ESVertexStream*			DynamicVertexStream;
	FGL4ESIndexBuffer*				DynamicIndexBuffer;

	// DE's additions.
	FGL4ESQuadIndexBuffer				DE_QuadIndexBuffer;
	FGL4ESDynVertStream			DE_DynamicVertexStream;

	// Variables.
	UViewport*						LockedViewport;
	INT								FrameCounter;
	FGL4ESMaterialStateStage		HardwareState[8];
	UBOOL							IsVAR;
	TArray<BYTE>					ScratchBuffer;
	GLuint							NullTextureID;

	// Configuration.
	FLOAT                           DetailTexMipBias,
									LevelOfAnisotropy;
	UBOOL							UseTrilinear,
									ReduceMouseLag,
									ForceCompression,
									UseVBO,
									UseVSync,
									UseRenderTargets,
									UsePixelShaders,
									UseHardwareGamma;
	INT								MaxTextureUnits,
									AppleVA,
									MultisampleBuffers,
									MultisampleSamples,
									MultisampleHint;
	DWORD							DesiredRefreshRate;


	// Status/ system information.
	FRenderCaps						RenderCaps;
	TArray<FPlane>					Modes;
	INT								NumTextureUnits,
									ConfigVARSize;
	UBOOL							IsR100;

	UBOOL							ValidContext,
									WasFullscreen;

	// Static variables.
	static INT						NumDevices;
	static INT						LockCount;
	static BYTE*					VARPointer;
	static INT						VARIndex;
	static INT						VARSize;

	static UBOOL					GLLoaded;

	// GL functions.
	#define GL_EXT(name) static UBOOL SUPPORTS##name;
	#define GL_PROC(ext,ret,func,parms) static ret (STDCALL *func)parms;
	#include "GL4ESFuncs.h"
	#undef GL_EXT
	#undef GL_PROC


	// Constructor/destructor.
	UGL4ESRenderDevice();

	void StaticConstructor();

	// Helper functions.
	virtual UBOOL FindExt( const TCHAR* Name );
	virtual void FindProc( void*& ProcAddress, char* Name, char* SupportName, UBOOL& Supports, UBOOL AllowExt );
	virtual void FindProcs( UBOOL AllowExt );
	virtual void MakeCurrent();
	virtual void GLError( TCHAR* Tag );

	// URenderDevice interface.
	virtual UBOOL Init();
	virtual void Exit(UViewport* Viewport);

	virtual UBOOL SetRes(UViewport* Viewport,INT NewX,INT NewY,UBOOL Fullscreen,INT ColorBytes=0,UBOOL bSaveSize=true);
	virtual void UnSetRes(UBOOL RestoreDisplaySettings);

	// GetCachedResource - Finds the cached copy of the given resource.  Returns NULL if failure.
	FGL4ESResource* GetCachedResource(QWORD CacheId);

	// FlushResource - Ensures that the given resource isn't being cached.
	virtual void FlushResource( QWORD CacheId );

	// ResourceCached - Returns whether a resource is cached or not.
	UBOOL ResourceCached(QWORD CacheId);

	// GetVertexShader - Finds a vertex shader with the given type/declaration.  Creates a vertex shader if none is found.
	FGL4ESVertexShader* GetVertexShader(EVertexShader Type,FGL4ESShaderDeclaration& Declaration);

	// GetPixelShader - Finds a Pixel shader with the given type.
	FGL4ESPixelShader* GetPixelShader(EGL4ESPixelShader Type);

	void SetPixelShader( GLuint PixelShader );

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
};

#endif
