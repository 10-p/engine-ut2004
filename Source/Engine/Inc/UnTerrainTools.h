/*=============================================================================
	UnTerrain.h: Tools for manipulating terrain in the editor
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "UnForcePacking_begin.h"

enum ETerrainMirrorAxis
{
	MIRRORAXIS_NONE		= 0,
	MIRRORAXIS_X		= 1,
	MIRRORAXIS_Y		= 2,
	MIRRORAXIS_XY		= 3,
};

/*------------------------------------------------------------------------------
	UTerrainBrush
------------------------------------------------------------------------------*/
class ENGINE_API UTerrainBrush
{
public:
	FString Desc;			// Description for the editor to display
	FString Exec;			// The brush name for the exec command
	INT ID;					// ETerrainBrush
	UBOOL bForceSoftSel,	// Does this brush force a soft selection with each click?
		bAllowSoftSel,		// Does this brush use soft selection at all?
		bShowVertices,		// Does this brush show the selected vertices while editing?
		bUseInnerRadius,	// Does this brush utilize the inner radius?
		bShowMarkers,		// Show the circle markers?
		bShowRect,			// Show a rectangle?
		bShowRedVertex;		// Show the red marker, indicating the vertex being pointed it?
	FVector SelMin, SelMax;	// The starting/ending locations of the selection box

	// If bPerTool=1 in GTerrainTools, then these values will be set and used by UnrealEd
	INT InnerRadius,		// The radius of full influence
		OuterRadius,		// The radius of influence falloff
		Strength,			// The strength that brushes apply themselves to the layers/terrain
		Adjust;				// The base amount to adjust layers by when using paint tools.
	INT MirrorAxis;			// Which axis we should mirror editing effects across (MIRRORAXIS_)

	UTerrainBrush();
	virtual ~UTerrainBrush();

	virtual void Execute( UBOOL bAdditive );
	virtual void MouseMove( FLOAT InMouseX, FLOAT InMouseY );
	virtual void MouseButtonDown( UViewport* InViewport );
	virtual void MouseButtonUp( UViewport* InViewport );
	virtual FBox GetRect();
	UBOOL BeginPainting( UTexture** InAlphaTexture, ATerrainInfo** InTerrainInfo );
	void EndPainting( UTexture* InAlphaTexture, ATerrainInfo* InTerrainInfo );
};

/*------------------------------------------------------------------------------
	UTerrainBrushVertexEdit
------------------------------------------------------------------------------*/
class ENGINE_API UTerrainBrushVertexEdit : public UTerrainBrush
{
public:
	UTerrainBrushVertexEdit();
	virtual ~UTerrainBrushVertexEdit();
};

/*------------------------------------------------------------------------------
	UTerrainBrushPaint
------------------------------------------------------------------------------*/
class ENGINE_API UTerrainBrushPaint : public UTerrainBrush
{
public:
	UTerrainBrushPaint();
	virtual ~UTerrainBrushPaint();

	virtual void Execute( UBOOL bAdditive );
};

/*------------------------------------------------------------------------------
	UTerrainBrushColor
------------------------------------------------------------------------------*/
class ENGINE_API UTerrainBrushColor : public UTerrainBrush
{
public:
	UTerrainBrushColor();
	virtual ~UTerrainBrushColor();

	virtual void Execute( UBOOL bAdditive );
};

/*------------------------------------------------------------------------------
	UTerrainBrushSmooth
------------------------------------------------------------------------------*/
class ENGINE_API UTerrainBrushSmooth : public UTerrainBrush
{
public:
	UTerrainBrushSmooth();
	virtual ~UTerrainBrushSmooth();

	virtual void Execute( UBOOL bAdditive );

// Some versions of GNU C++ seem to need a little nudging here. --ryan.
#ifdef __GNUC__
    void *operator new(size_t x, TArray<UTerrainBrush>& t) { return ::operator new(x, t); }
#endif
};

/*------------------------------------------------------------------------------
	UTerrainBrushNoise
------------------------------------------------------------------------------*/
class ENGINE_API UTerrainBrushNoise : public UTerrainBrush
{
public:
	UTerrainBrushNoise();
	virtual ~UTerrainBrushNoise();

	virtual void Execute( UBOOL bAdditive );
};

/*------------------------------------------------------------------------------
	UTerrainBrushFlatten
------------------------------------------------------------------------------*/
class ENGINE_API UTerrainBrushFlatten : public UTerrainBrush
{
public:
	UTerrainBrushFlatten();
	virtual ~UTerrainBrushFlatten();

	virtual void Execute( UBOOL bAdditive );
};

/*------------------------------------------------------------------------------
	UTerrainBrushTexPan
------------------------------------------------------------------------------*/
class ENGINE_API UTerrainBrushTexPan : public UTerrainBrush
{
public:
	UTerrainBrushTexPan();
	virtual ~UTerrainBrushTexPan();

	virtual void MouseMove( FLOAT InMouseX, FLOAT InMouseY );
};

/*------------------------------------------------------------------------------
	UTerrainBrushTexRotate
------------------------------------------------------------------------------*/
class ENGINE_API UTerrainBrushTexRotate : public UTerrainBrush
{
public:
	UTerrainBrushTexRotate();
	virtual ~UTerrainBrushTexRotate();

	virtual void MouseMove( FLOAT InMouseX, FLOAT InMouseY );
};

/*------------------------------------------------------------------------------
	UTerrainBrushTexScale
------------------------------------------------------------------------------*/
class ENGINE_API UTerrainBrushTexScale : public UTerrainBrush
{
public:
	UTerrainBrushTexScale();
	virtual ~UTerrainBrushTexScale();

	virtual void MouseMove( FLOAT InMouseX, FLOAT InMouseY );
};

/*------------------------------------------------------------------------------
	UTerrainBrushSelect
------------------------------------------------------------------------------*/
class ENGINE_API UTerrainBrushSelect : public UTerrainBrush
{
public:
	UTerrainBrushSelect();
	virtual ~UTerrainBrushSelect();

	virtual void Execute( UBOOL bAdditive );
	virtual void MouseMove( FLOAT InMouseX, FLOAT InMouseY );
	virtual void MouseButtonDown( UViewport* InViewport );
	virtual FBox GetRect();
};

/*------------------------------------------------------------------------------
	UTerrainBrushVisibility
------------------------------------------------------------------------------*/
class ENGINE_API UTerrainBrushVisibility : public UTerrainBrush
{
public:
	UTerrainBrushVisibility();
	virtual ~UTerrainBrushVisibility();

	virtual void Execute( UBOOL bAdditive );
	virtual FBox GetRect();
};

/*------------------------------------------------------------------------------
	UTerrainBrushEdgeTurn
------------------------------------------------------------------------------*/
class ENGINE_API UTerrainBrushEdgeTurn : public UTerrainBrush
{
public:
	UTerrainBrushEdgeTurn();
	virtual ~UTerrainBrushEdgeTurn();

	virtual void Execute( UBOOL bAdditive );
	virtual FBox GetRect();
};

/*------------------------------------------------------------------------------
	FTerrainTools
------------------------------------------------------------------------------*/

struct FAttachedActorOffset
{
	AActor* Actor;
	FLOAT Height;

	friend FArchive& operator<<( FArchive& Ar, FAttachedActorOffset& O )
	{
		return Ar << O.Actor << O.Height;
	}
};

class ENGINE_API FTerrainTools
{
public:
	UBOOL bPerTool;					// Use a global radius/strength/adjust value for all tools or on a per tool basis?

	UBOOL bAutoSoftSel,				// Automatically perform soft selection when clicking vertices?
		bLockSliders,				// Do the radii sliders move together in the terrain tool dialog?
		bFirstClick,				// Lets the brushes know that this is the first update since the mouse was clicked.
		bMoveActors,				// Moves actors when dragging vertices.
		bShowDecoAlpha,				// Show the selected deco alpha texture on the terrain?
		bIgnoreInvisibleQuads;		// Ignore invisible quads when doing traces for painting
		
	DWORD RefHeight;
	FVector PointingAt;				// The vertex the mouse is currently pointing at.

	UTexture *WhiteCircle, *EyeBall;

	INT EditorMode,					// Editor mode, relayed from the editor
		TerrainEditBrush;			// Current terrain editing brush, relayed from the editor
	FColor Color;					// The color used by the "color" brush

	TArray<UTerrainBrush> Brushes;	// A list of all available terrain brushes
	UTerrainBrush* CurrentBrush;	// The terrain brush currently in use

	INT LayerScrollPos, LayerScrollMax;			// Layer viewport scrollbar values
	INT DecoLayerScrollPos, DecoLayerScrollMax;	// Decoration layer viewport scrollbar values

	TArray<FAttachedActorOffset>	AttachedActorOffsets;

	// Constructor.
	FTerrainTools();
	virtual ~FTerrainTools();

	void Init();
	void SetCurrentBrush( INT InID );

	inline INT GetInnerRadius()	{	return (bPerTool ? CurrentBrush->InnerRadius : InnerRadius); }
	inline INT GetOuterRadius()	{	return (bPerTool ? CurrentBrush->OuterRadius : OuterRadius); }
	inline INT GetStrength()	{	return (bPerTool ? CurrentBrush->Strength : Strength); }
	inline INT GetAdjust()		{	return (bPerTool ? CurrentBrush->Adjust : Adjust); }
	inline INT GetMirrorAxis()	{	return (bPerTool ? CurrentBrush->MirrorAxis : MirrorAxis); }

	inline void SetInnerRadius( INT In )	{	bPerTool ? CurrentBrush->InnerRadius = In : InnerRadius = In; }
	inline void SetOuterRadius( INT In )	{	bPerTool ? CurrentBrush->OuterRadius = In : OuterRadius = In; }
	inline void SetStrength( INT In )		{	bPerTool ? CurrentBrush->Strength = In : Strength = In; }
	inline void SetAdjust( INT In )			{	bPerTool ? CurrentBrush->Adjust = In : Adjust = In; }
	inline void SetMirrorAxis( INT In )		{	bPerTool ? CurrentBrush->MirrorAxis = In : MirrorAxis = In; }

	ATerrainInfo* GetCurrentTerrainInfo() { return CurrentTerrain; }
	void SetCurrentTerrainInfo( ATerrainInfo* InTerrainInfo );
	FString GetExecFromBrushName( FString& InBrushName );
	void FindActorsToAlign();
	void AdjustAlignedActors();
	INT CurrentLayer;
	UTexture* CurrentAlphaMap;

private:
	ATerrainInfo* CurrentTerrain;	// The terrain that all editing is being performed on.

	INT InnerRadius,		// The radius of full influence
		OuterRadius,		// The radius of influence falloff
		Strength,			// The strength that brushes apply themselves to the layers/terrain
		Adjust;				// The base amount to adjust layers by when using paint tools.
	INT MirrorAxis;			// Which axis we should mirror editing effects across (MIRRORAXIS_)
};

#include "UnForcePacking_end.h"

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

