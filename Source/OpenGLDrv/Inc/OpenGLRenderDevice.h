/*=============================================================================
	OpenGLRenderDevice.h: Unreal OpenGL render device definition.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#ifndef HEADER_OPENGLRENDERDEVICE
#define HEADER_OPENGLRENDERDEVICE

#define AUTO_INITIALIZE_REGISTRANTS_OPENGLDRV UOpenGLRenderDevice::StaticClass();

const float DefaultMipBias = -0.5f; // sjs

#ifndef OPENGLDRV_API
#define OPENGLDRV_API DLL_IMPORT
#endif


//
//	UOpenGLRenderDevice
//
class OPENGLDRV_API UOpenGLRenderDevice : public URenderDevice
{
	DECLARE_CLASS(UOpenGLRenderDevice,URenderDevice,CLASS_Config,OpenGLDrv);
public:

	// Our render interface.
	FOpenGLRenderInterface			RenderInterface;

	// Resource management.
	FOpenGLResource*				ResourceList;
	FOpenGLResource*				ResourceHash[4096];

	FOpenGLVertexShader*			VertexShaders;
	FOpenGLPixelShader*				PixelShaders;

	FOpenGLVertexStream*			DynamicVertexStream;
	FOpenGLIndexBuffer*				DynamicIndexBuffer;

	// DE's additions.
	FQuadIndexBuffer				DE_QuadIndexBuffer;
	FOpenGLDynVertStream			DE_DynamicVertexStream;

	// Variables.
	UViewport*						LockedViewport;
	INT								FrameCounter;
	FOpenGLMaterialStateStage		HardwareState[8];
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
									UsePixelShaders;
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

#ifdef UT_OPENGLDRV_WGL
	HGLRC							hRC;
	HWND							hWnd;
	HDC								hDC;
#endif
	UBOOL							ValidContext,
									WasFullscreen;

	// Static variables.
	static INT						NumDevices;
	static INT						LockCount;
	static BYTE*					VARPointer;
	static INT						VARIndex;
	static INT						VARSize;

#ifdef UT_OPENGLDRV_WGL
	static HMODULE					hModuleGLMain;
	static HGLRC					hCurrentRC;
	static HMODULE					hModuleGLGDI;
#else
	static UBOOL					GLLoaded;
#endif

	// GL functions.
	#define GL_EXT(name) static UBOOL SUPPORTS##name;
	#define GL_PROC(ext,ret,func,parms) static ret (STDCALL *func)parms;
	#include "OpenGLFuncs.h"
	#undef GL_EXT
	#undef GL_PROC


	// Constructor/destructor.
	UOpenGLRenderDevice();

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
	FOpenGLResource* GetCachedResource(QWORD CacheId);

	// FlushResource - Ensures that the given resource isn't being cached.
	virtual void FlushResource( QWORD CacheId );

	// ResourceCached - Returns whether a resource is cached or not.
	UBOOL ResourceCached(QWORD CacheId);

	// GetVertexShader - Finds a vertex shader with the given type/declaration.  Creates a vertex shader if none is found.
	FOpenGLVertexShader* GetVertexShader(EVertexShader Type,FShaderDeclaration& Declaration);

	// GetPixelShader - Finds a Pixel shader with the given type.
	FOpenGLPixelShader* GetPixelShader(EPixelShader Type);

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
