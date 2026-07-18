/*=============================================================================
	UnCameraEffects.h: Camera effect definitions.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

//
//	UCameraEffect
//

class ENGINE_API UCameraEffect : public UObject
{
	DECLARE_ABSTRACT_CLASS(UCameraEffect,UObject,0,Engine);

public:

	FLOAT		Alpha;
    BITFIELD	FinalEffect : 1	GCC_PACK(4);

    INT cameraeffect_dummy GCC_PACK(4);

	// UCameraEffect interface.

	virtual void PreRender(UViewport* Viewport,FRenderInterface* RI) {}
	virtual void PostRender(UViewport* Viewport,FRenderInterface* RI) {}
};

//
//	UMotionBlur
//

class ENGINE_API UMotionBlur : public UCameraEffect
{
	DECLARE_CLASS(UMotionBlur,UCameraEffect,0,Engine);

public:

    BYTE				BlurAlpha;
	FAuxRenderTarget*	RenderTargets[2];
	FLOAT				LastFrameTime;

	// UObject interface.

	void Destroy();

	// UCameraEffect interface.

	virtual void PreRender(UViewport* Viewport,FRenderInterface* RI);
	virtual void PostRender(UViewport* Viewport,FRenderInterface* RI);
};

//
//	UCameraOverlay
//

class ENGINE_API UCameraOverlay : public UCameraEffect
{
	DECLARE_CLASS(UCameraOverlay,UCameraEffect,0,Engine);

public:

	FColor		OverlayColor;
	UMaterial*	OverlayMaterial;

	// UCameraEffect interface.

	virtual void PostRender(UViewport* Viewport,FRenderInterface* RI);
};

//
//	USubActionCameraEffect
//

class ENGINE_API USubActionCameraEffect : public UMatSubAction
{
	DECLARE_CLASS(USubActionCameraEffect,UMatSubAction,0,Engine);

public:

	UCameraEffect*	CameraEffect;
	FLOAT			StartAlpha,
					EndAlpha;
	BITFIELD		DisableAfterDuration : 1 GCC_PACK(4);

	// UMatSubAction interface.

	virtual UBOOL Update(FLOAT Percent,class ASceneManager* SceneManager);
	virtual FString GetStatString();
};

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif
