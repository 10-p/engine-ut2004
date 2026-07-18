/*=============================================================================
	UnCanvas.h: Unreal canvas definition.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"


class FCanvasUtil; // sjs

/*
	UCanvas
	A high-level rendering interface used to render objects on the HUD.
*/

class ENGINE_API UCanvas : public UObject
{
	DECLARE_CLASS(UCanvas,UObject,CLASS_Transient,Engine);
	NO_DEFAULT_CONSTRUCTOR(UCanvas);
public:

	// Variables.
	UFont*			Font;
    FLOAT           FontScaleX, FontScaleY; // gam
	FLOAT			SpaceX, SpaceY;
	FLOAT			OrgX, OrgY;
	FLOAT			ClipX, ClipY;
	FLOAT			CurX, CurY;
	FLOAT			Z;
	BYTE			Style;
	FLOAT			CurYL;
	FColor			Color;
	BITFIELD		bCenter:1 GCC_PACK(4);
	BITFIELD		bNoSmooth:1;
	INT				SizeX, SizeY;
    FPlane          ColorModulate;  // sjs - modulate all colors by this before rendering
	BITFIELD		bForceAlpha;
	FLOAT			ForcedAlpha;
    BITFIELD        bRenderLevel:1 GCC_PACK(4);   // gam - Will render the level if enabled.
	UFont			*TinyFont, *SmallFont, *MedFont;
	FStringNoInit	TinyFontName, SmallFontName, MedFontName;
	UViewport*		Viewport;
    FCanvasUtil*    pCanvasUtil; // sjs - shared canvas util for batching successive canvas rendering funcs

	// UCanvas interface.
	virtual void Init( UViewport* InViewport );
	virtual void Update();
	virtual void DrawTile( UMaterial* Material, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, FLOAT Z, FPlane Color, FPlane Fog );
	virtual void DrawIcon( UMaterial* Material, FLOAT ScreenX, FLOAT ScreenY, FLOAT XSize, FLOAT YSize, FLOAT Z, FPlane Color, FPlane Fog );
	virtual void DrawPattern( UMaterial* Material, FLOAT X, FLOAT Y, FLOAT XL, FLOAT YL, FLOAT Scale, FLOAT OrgX, FLOAT OrgY, FLOAT Z, FPlane Color, FPlane Fog );
	virtual void VARARGS WrappedStrLenf( UFont* Font, INT& XL, INT& YL, const TCHAR* Fmt, ... );
	virtual void VARARGS WrappedPrintf( UFont* Font, UBOOL Center, const TCHAR* Fmt, ... );

    // gam ---
	virtual void VARARGS WrappedStrLenf( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, INT& XL, INT& YL, const TCHAR* Fmt, ... );
	virtual void VARARGS WrappedPrintf( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, UBOOL Center, const TCHAR* Fmt, ... );

	virtual void WrapStringToArray( const TCHAR* Text, TArray<FString> *OutArray, float Width, UFont *Font = NULL, const TCHAR EOL='\n' );
	virtual void ClippedStrLen( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, FLOAT& XL, FLOAT& YL, const TCHAR* Text );
	virtual void ClippedPrint( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, UBOOL Center, const TCHAR* Text );

	void DrawActor( AActor* Actor, UBOOL WireFrame, UBOOL ClearZ, FLOAT DisplayFOV );
    void DrawScreenActor( AActor* Actor, UBOOL WireFrame, UBOOL ClearZ, FLOAT DisplayFOV );
    void SetScreenLight( INT Index, const FVector& Position, FColor Color, FLOAT Radius );

	void virtual DrawTileStretched(UMaterial* Mat, FLOAT Left, FLOAT Top, FLOAT AWidth, FLOAT AHeight);
	void virtual DrawTileScaled(UMaterial* Mat, FLOAT Left, FLOAT Top, FLOAT NewXScale, FLOAT NewYScale);
	void virtual DrawTileBound(UMaterial* Mat, FLOAT Left, FLOAT Top, FLOAT Width, FLOAT Height);
	void virtual DrawTileJustified(UMaterial* Mat, FLOAT Left, FLOAT Top, FLOAT Width, FLOAT Height, BYTE Justification);
	void virtual DrawTileScaleBound(UMaterial* Mat, FLOAT Left, FLOAT Top, FLOAT Width, FLOAT Height);
	void virtual DrawTileStretchedOrScaled( UMaterial* Mat, FLOAT Left, FLOAT Top, FLOAT AWidth, FLOAT AHeight, FLOAT UL, FLOAT VL);
	void virtual VARARGS DrawTextJustified(BYTE Justification, FLOAT x1, FLOAT y1, FLOAT x2, FLOAT y2, const TCHAR* Fmt, ... );

    // --- gam

	virtual void SetClip( INT X, INT Y, INT XL, INT YL );

	// Natives.
	DECLARE_FUNCTION(execStrLen)
	DECLARE_FUNCTION(execDrawText)
	DECLARE_FUNCTION(execDrawTile)
	DECLARE_FUNCTION(execDrawActor)
	DECLARE_FUNCTION(execDrawTileClipped)
	DECLARE_FUNCTION(execDrawTextClipped)
	DECLARE_FUNCTION(execTextSize)
	DECLARE_FUNCTION(execDrawPortal)
    DECLARE_FUNCTION(execWorldToScreen) // sjs
    DECLARE_FUNCTION(execGetCameraLocation)
	DECLARE_FUNCTION(execWrapStringToArray) // mc

    // gam ---
	DECLARE_FUNCTION(execSetScreenLight);
	DECLARE_FUNCTION(execSetScreenProjector);
	DECLARE_FUNCTION(execDrawScreenActor);
	DECLARE_FUNCTION(execClear);
	DECLARE_FUNCTION(execWrapText);
	DECLARE_FUNCTION(execDraw3DLine);
    DECLARE_FUNCTION(execDrawTexturedLine)
    // --- gam

	// jmw

	DECLARE_FUNCTION(execDrawTileStretched);
	DECLARE_FUNCTION(execDrawTileJustified);
	DECLARE_FUNCTION(execDrawTileScaled);
	DECLARE_FUNCTION(execDrawTextJustified);
	DECLARE_FUNCTION(execDrawActorClipped);

	// rjp
	DECLARE_FUNCTION(execDrawTilePartialStretched);

    void eventReset()
    {
        ProcessEvent(FindFunctionChecked(TEXT("Reset")),NULL);
    }

	void Destroy();

private:
	// Internal functions.
	void WrappedPrint( ERenderStyle Style, INT& XL, INT& YL, UFont* Font, FLOAT ScaleX, FLOAT ScaleY, UBOOL Center, const TCHAR* Text ); // gam
};

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

