#ifndef UNullRenderDevice_H
#define UNullRenderDevice_H

#include "EnginePrivate.h"

#include "UnForcePacking_begin.h"

// Package implementation.
class UNullRenderDevice;

//
//	FNullRenderInterface
//
class FNullRenderInterface : public FRenderInterface
{
public:

	// Variables.
	UNullRenderDevice*		RenDev;
	UViewport*				Viewport;

	// Constructor.
	FNullRenderInterface(UNullRenderDevice* InRenDev) {	RenDev = InRenDev; }

	// FRenderInterface interface.
	virtual void PushState() {}
	virtual void PopState() {}

	virtual UBOOL SetRenderTarget(FRenderTarget* RenderTarget) { return 1; }
	virtual void SetViewport(INT X,INT Y,INT Width,INT Height) {}
	virtual void Clear(UBOOL UseColor,FColor Color,UBOOL UseDepth,FLOAT Depth,UBOOL UseStencil,DWORD Stencil) {}

	virtual void PushHit(const BYTE* Data,INT Count) {}
	virtual void PopHit(INT Count,UBOOL Force) {}

	virtual void SetCullMode(ECullMode CullMode) {}

	virtual void SetAmbientLight(FColor Color) {}
	virtual void EnableLighting(UBOOL UseDynamic, UBOOL UseStatic, UBOOL Modulate2X, FBaseTexture* LightmapTexture, UBOOL LightingOnly, FSphere LitSphere) {}
    virtual void SetLight(INT LightIndex,FDynamicLight* Light,FLOAT Scale) {} // sjs

	virtual void SetNPatchTesselation( FLOAT Tesselation ) {}
	virtual void SetDistanceFog(UBOOL Enable,FLOAT FogStart,FLOAT FogEnd,FColor Color) {}
	virtual void SetGlobalColor(FColor Color) {}
	
	virtual void SetTransform(ETransformType Type,const FMatrix& Matrix) {}

	virtual void SetMaterial(UMaterial* Material, FString* ErrorString, UMaterial** ErrorMaterial, INT* NumPasses) {}
	virtual void SetZBias(INT ZBias) {}
	virtual void SetStencilOp(ECompareFunction Test,DWORD Ref,DWORD Mask,EStencilOp FailOp,EStencilOp ZFailOp,EStencilOp PassOp,DWORD WriteMask) {}
	virtual void SetPrecacheMode( EPrecacheMode PrecacheMode ) {}
	virtual void SetEmulationMode(EHardwareEmulationMode Mode) {}
	virtual INT SetVertexStreams(EVertexShader Shader,FVertexStream** Streams,INT NumStreams) { Streams = NULL; return 0; }
	virtual INT  SetDynamicStream(EVertexShader Shader,FVertexStream* Stream);
	virtual INT SetIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseVertexIndex) { return 0; }
	virtual INT  SetDynamicIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseVertexIndex) { return 0; }

	virtual void DrawPrimitive(EPrimitiveType PrimitiveType,INT FirstIndex,INT NumPrimitives,INT MinIndex,INT MaxIndex) {}

	// FNullRenderInterface interface.
	void Locked( UViewport* InViewport, BYTE* InHitData,INT* InHitSize) {}
	void Unlocked() {}

private:
};

class ENGINE_API UNullRenderDevice : public URenderDevice
{
	DECLARE_CLASS(UNullRenderDevice,URenderDevice,CLASS_Config,Engine);
public:
	// Our render interface.
	FNullRenderInterface RenderInterface;

	// Dummy dynamic buffer;
	TArray<BYTE> DynamicData;
	FRenderCaps RenderCaps;

	// Constructor/destructor.
	UNullRenderDevice() : RenderInterface(this) {}

	void StaticConstructor() {}

	// URenderDevice interface.
	virtual UBOOL Init() 
	{
		DynamicData.Empty();
		DynamicData.Add(65536);
		return 1; 
	}
	virtual void Exit(UViewport* Viewport){}
	virtual UBOOL SetRes(UViewport* Viewport,INT NewX,INT NewY,UBOOL Fullscreen,INT ColorBytes,UBOOL bSaveSize=true) 
	{ 
		verify(Viewport->ResizeViewport( BLIT_Direct3D, NewX, NewY ));
		return 1;
	}

	virtual void Flush(UViewport* Viewport) {}
	void FlushResource(QWORD CacheId) {}

	virtual void UpdateGamma(UViewport* Viewport) {}
	virtual void RestoreGamma() {}

	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar) { return 0; }

	virtual FRenderInterface* Lock(UViewport* Viewport,BYTE* HitData,INT* HitSize) { return &RenderInterface; }
	virtual void Unlock(FRenderInterface* RI) {}
	virtual void Present(UViewport* Viewport) {}

	virtual void ReadPixels(UViewport* Viewport,FColor* Pixels,UBOOL Flipped=0) {}

	virtual UBOOL SupportsTextureFormat( ETextureFormat ) { return 1; }

	virtual void SetEmulationMode(EHardwareEmulationMode Mode) {}

	virtual FRenderCaps* GetRenderCaps()
	{
		return &RenderCaps;
	}
};

#include "UnForcePacking_end.h"

#endif
