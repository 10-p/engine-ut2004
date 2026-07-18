/*=============================================================================
	UnTerrainTools.cpp: Tools for manipulating terrain in the editor
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "EnginePrivate.h"

class FTerrainTools;
extern ENGINE_API FTerrainTools		GTerrainTools;

/*------------------------------------------------------------------------------
	UTerrainBrush.

	Base class for all terrain painting/manipulating tools
------------------------------------------------------------------------------*/

UTerrainBrush::UTerrainBrush()
{
	Desc = TEXT("None");
	Exec = TEXT("NONE");
	ID = TB_None;
	bForceSoftSel = 1;
	bAllowSoftSel = 1;
	bShowVertices = 0;
	bUseInnerRadius = 1;
	bShowMarkers = 1;
	bShowRect = 0;
	bShowRedVertex = 1;
	InnerRadius = 256;
	OuterRadius = 1024;
	Strength = 100;
	Adjust = 32;
	MirrorAxis = MIRRORAXIS_NONE;
}

UTerrainBrush::~UTerrainBrush()
{
}

// Perform whatever action the brush wants to take on the current layer.
// This function assumes that the relevant vertices have already
// been selected.
void UTerrainBrush::Execute( UBOOL bAdditive )
{
}

// Allow the terrain brush to respond to mouse movement.
void UTerrainBrush::MouseMove( FLOAT InMouseX, FLOAT InMouseY )
{
}

// Called when the user first clicks the mouse on the terrain.
void UTerrainBrush::MouseButtonDown( UViewport* InViewport )
{
}

// Called when the user releases the mouse button after an editing operation.
void UTerrainBrush::MouseButtonUp( UViewport* InViewport )
{
}

// If a brush wants to display a selection rectangle or something similar, return the shape through this function.
FBox UTerrainBrush::GetRect()
{
	FBox Ret(0);
	return Ret;
}

// Gets everything ready for a brush to paint on a layer.
UBOOL UTerrainBrush::BeginPainting( UTexture** InAlphaTexture, ATerrainInfo** InTerrainInfo )
{
	guard(UTerrainBrush::BeginPainting);

	*InTerrainInfo = GTerrainTools.GetCurrentTerrainInfo();
	if( !(*InTerrainInfo) )	return 0;
	*InAlphaTexture = GTerrainTools.CurrentAlphaMap;
	if( !(*InAlphaTexture) )	return 0;

	if(!(*InAlphaTexture)->bParametric)
		(*InAlphaTexture)->Mips(0).DataArray.Load();

	return 1;

	unguard;
}

// Cleans up when layer painting is complete.
void UTerrainBrush::EndPainting( UTexture* InAlphaTexture, ATerrainInfo* InTerrainInfo )
{
	guard(UTerrainBrush::EndPainting);

	if(!InAlphaTexture->bParametric)
		InAlphaTexture->Mips(0).DataArray.Load();

	InTerrainInfo->UpdateFromSelectedVertices();
	InAlphaTexture->bRealtimeChanged = 1;

	unguard;
}

/*------------------------------------------------------------------------------
	UTerrainBrushVertexEdit

	Selects and drags vertices vertically.
------------------------------------------------------------------------------*/

UTerrainBrushVertexEdit::UTerrainBrushVertexEdit()
{
	Desc = TEXT("Vertex Editing");
	Exec = TEXT("VERTEXEDIT");
	ID = TB_VertexEdit;
	bShowVertices = 1;
	bForceSoftSel = 0;
}

UTerrainBrushVertexEdit::~UTerrainBrushVertexEdit()
{
}

/*------------------------------------------------------------------------------
	UTerrainBrushPaint

	Paints on a specified layer.
------------------------------------------------------------------------------*/

UTerrainBrushPaint::UTerrainBrushPaint()
{
	Desc = TEXT("Painting");
	Exec = TEXT("PAINT");
	ID = TB_Paint;
}

UTerrainBrushPaint::~UTerrainBrushPaint()
{
}

void UTerrainBrushPaint::Execute( UBOOL bAdditive )
{
	guard(UTerrainBrushPaint::Execute);

	if( !GTerrainTools.CurrentAlphaMap )
	{
		GTerrainTools.FindActorsToAlign();

		if( bAdditive )
			GTerrainTools.GetCurrentTerrainInfo()->MoveVertices( -GTerrainTools.GetAdjust ()* (GTerrainTools.GetStrength()/100.f) );
		else
			GTerrainTools.GetCurrentTerrainInfo()->MoveVertices( GTerrainTools.GetAdjust ()* (GTerrainTools.GetStrength()/100.f) );
		
		GTerrainTools.AdjustAlignedActors();
	}
	else
	{
		ATerrainInfo* TI;
		UTexture* AlphaTexture;
		if( !BeginPainting( &AlphaTexture, &TI ) ) return;

		INT CurrentAlpha;
		FLOAT AdjustAlpha;
		BYTE NewAlpha;
		FSelectedTerrainVertex* Vertex;

		for( INT x = 0 ; x < TI->SelectedVertices.Num() ; x++ )
		{
			Vertex = &TI->SelectedVertices(x);
			CurrentAlpha = TI->GetLayerAlpha( Vertex->X, Vertex->Y, 0, AlphaTexture );
			AdjustAlpha = GTerrainTools.GetAdjust ()* Vertex->Weight;

			if( bAdditive )
				NewAlpha = ( CurrentAlpha + AdjustAlpha < 255 ) ? (INT)(CurrentAlpha + AdjustAlpha) : 255;
			else
				NewAlpha = ( CurrentAlpha - AdjustAlpha > 0 ) ? (INT)(CurrentAlpha - AdjustAlpha) : 0;

			TI->SetLayerAlpha( Vertex->X, Vertex->Y, GTerrainTools.CurrentLayer-1, NewAlpha, GTerrainTools.CurrentAlphaMap );
		}

		EndPainting( AlphaTexture, TI );
	}

	unguard;
}

/*------------------------------------------------------------------------------
	UTerrainBrushColor

	Paints the current color into the RGB channels of the current layer.
------------------------------------------------------------------------------*/

UTerrainBrushColor::UTerrainBrushColor()
{
	Desc = TEXT("Color");
	Exec = TEXT("COLOR");
	ID = TB_Color;
	bUseInnerRadius = 0;
}

UTerrainBrushColor::~UTerrainBrushColor()
{
}

void UTerrainBrushColor::Execute( UBOOL bAdditive )
{
	guard(UTerrainBrushColor::Execute);

	ATerrainInfo* TI;
	UTexture* AlphaTexture;
	UBOOL bRet = BeginPainting( &AlphaTexture, &TI );

	if( !GTerrainTools.CurrentAlphaMap )
		AlphaTexture = TI->TerrainMap;
	else
		if( !bRet )
			return;

	// FIXME : should do some kind of nice blending and stuff here instead of just jamming the color in
	for( INT x = 0 ; x < TI->SelectedVertices.Num() ; x++ )
	{
		FSelectedTerrainVertex* Vertex = &TI->SelectedVertices(x);
		TI->SetTextureColor( Vertex->X, Vertex->Y, AlphaTexture, GTerrainTools.Color );
	}

	EndPainting( AlphaTexture, TI );

	unguard;
}

/*------------------------------------------------------------------------------
	UTerrainBrushSmooth

	Performs a 3x3 box filter the selected vertices.
------------------------------------------------------------------------------*/

UTerrainBrushSmooth::UTerrainBrushSmooth()
{
	Desc = TEXT("Smoothing");
	Exec = TEXT("SMOOTH");
	ID = TB_Smooth;
	bUseInnerRadius = 0;
}

UTerrainBrushSmooth::~UTerrainBrushSmooth()
{
}

void UTerrainBrushSmooth::Execute( UBOOL bAdditive )
{
	guard(UTerrainBrushSmooth::Execute);

	if( !GTerrainTools.CurrentLayer )
	{
		GTerrainTools.FindActorsToAlign();
		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();

		static TArray<FLOAT> SmoothedHeights;
		FLOAT Work, OrigHeight;

		for( INT x = 0 ; x < TI->SelectedVertices.Num() ; x++ )
		{
			OrigHeight = Work = TI->GetHeightmap(TI->SelectedVertices(x).X, TI->SelectedVertices(x).Y);

			Work += TI->GetHeightmap(TI->SelectedVertices(x).X-1, TI->SelectedVertices(x).Y-1);
			Work += TI->GetHeightmap(TI->SelectedVertices(x).X, TI->SelectedVertices(x).Y-1);
			Work += TI->GetHeightmap(TI->SelectedVertices(x).X+2, TI->SelectedVertices(x).Y-1);

			Work += TI->GetHeightmap(TI->SelectedVertices(x).X-1, TI->SelectedVertices(x).Y);
			Work += TI->GetHeightmap(TI->SelectedVertices(x).X+2, TI->SelectedVertices(x).Y);

			Work += TI->GetHeightmap(TI->SelectedVertices(x).X-1, TI->SelectedVertices(x).Y+1);
			Work += TI->GetHeightmap(TI->SelectedVertices(x).X, TI->SelectedVertices(x).Y+1);
			Work += TI->GetHeightmap(TI->SelectedVertices(x).X+2, TI->SelectedVertices(x).Y+1);

			Work /= 9.0f;
			Work = OrigHeight + ((Work - OrigHeight) * (GTerrainTools.GetStrength()/100.f));

			new( SmoothedHeights )FLOAT( Work );
		}

		for( INT x = 0 ; x < TI->SelectedVertices.Num() ; x++ )
			TI->SetHeightmap(TI->SelectedVertices(x).X, TI->SelectedVertices(x).Y, SmoothedHeights(x));

		SmoothedHeights.Empty();

		TI->UpdateFromSelectedVertices();
		GTerrainTools.AdjustAlignedActors();
	}
	else
	{
		ATerrainInfo* TI;
		UTexture* AlphaTexture;
		if(! BeginPainting( &AlphaTexture, &TI) ) return;

		static TArray<BYTE> SmoothedAlphas;

		FSelectedTerrainVertex* Vertex;
		FLOAT Work, OrigAlpha;

		for( INT x = 0 ; x < TI->SelectedVertices.Num() ; x++ )
		{
			Vertex = &TI->SelectedVertices(x);

			OrigAlpha = Work = TI->GetLayerAlpha( Vertex->X, Vertex->Y, 0, AlphaTexture );

			Work += TI->GetLayerAlpha( Vertex->X+1, Vertex->Y-1, 0, AlphaTexture );
			Work += TI->GetLayerAlpha( Vertex->X, Vertex->Y-1, 0, AlphaTexture );
			Work += TI->GetLayerAlpha( Vertex->X-1, Vertex->Y-1, 0, AlphaTexture );

			Work += TI->GetLayerAlpha( Vertex->X+1, Vertex->Y, 0, AlphaTexture );
			Work += TI->GetLayerAlpha( Vertex->X-1, Vertex->Y, 0, AlphaTexture );

			Work += TI->GetLayerAlpha( Vertex->X+1, Vertex->Y+1, 0, AlphaTexture );
			Work += TI->GetLayerAlpha( Vertex->X, Vertex->Y+1, 0, AlphaTexture );
			Work += TI->GetLayerAlpha( Vertex->X-1, Vertex->Y+1, 0, AlphaTexture );

			Work /= 9.0f;
			Work = OrigAlpha + ((Work - OrigAlpha) * (GTerrainTools.GetStrength()/100.f));

			Work = (INT)Work;
			Work = Min<INT>( Work, 255.f);
			Work = Max<INT>( Work, 0.f);
			new( SmoothedAlphas )BYTE( (BYTE)Work );
		}

		for( INT x = 0 ; x < TI->SelectedVertices.Num() ; x++ )
		{
			Vertex = &TI->SelectedVertices(x);
			TI->SetLayerAlpha( Vertex->X, Vertex->Y, GTerrainTools.CurrentLayer-1, SmoothedAlphas(x), GTerrainTools.CurrentAlphaMap );
		}

		SmoothedAlphas.Empty();

		EndPainting( AlphaTexture, TI );
	}

	unguard;
}

/*------------------------------------------------------------------------------
	UTerrainBrushNoise

	Adds random noise into the selected vertices.
------------------------------------------------------------------------------*/

UTerrainBrushNoise::UTerrainBrushNoise()
{
	Desc = TEXT("Noise");
	Exec = TEXT("NOISE");
	ID = TB_Noise;
	bUseInnerRadius = 0;
}

UTerrainBrushNoise::~UTerrainBrushNoise()
{
}

void UTerrainBrushNoise::Execute( UBOOL bAdditive )
{
	guard(UTerrainBrushNoise::Execute);

	if( !GTerrainTools.CurrentLayer )
	{
		GTerrainTools.FindActorsToAlign();
		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();

		for( INT x = 0 ; x < TI->SelectedVertices.Num() ; x++ )
		{
			FLOAT RandAdjust = (appRand()%(GTerrainTools.GetAdjust()*2))-GTerrainTools.GetAdjust();
			RandAdjust *= GTerrainTools.GetStrength()/100.f;

			TI->SetHeightmap(TI->SelectedVertices(x).X, TI->SelectedVertices(x).Y,
				TI->GetHeightmap(TI->SelectedVertices(x).X, TI->SelectedVertices(x).Y) + (INT)RandAdjust);
		}

		TI->UpdateFromSelectedVertices();
		GTerrainTools.AdjustAlignedActors();
	}
	else
	{
		ATerrainInfo* TI;
		UTexture* AlphaTexture;
		if(! BeginPainting( &AlphaTexture, &TI ) ) return;

		FSelectedTerrainVertex* Vertex;
		FLOAT RandAdjust;
		BYTE Work;

		for( INT x = 0 ; x < TI->SelectedVertices.Num() ; x++ )
		{
			Vertex = &TI->SelectedVertices(x);

			RandAdjust = (appRand()%(128*2))-128;	// Gives random number between -128/+128
			RandAdjust *= GTerrainTools.GetStrength()/100.f;

			Work = TI->GetLayerAlpha( Vertex->X, Vertex->Y, 0, AlphaTexture );

			if( Work + (INT)RandAdjust > 255 ) Work = 255;
			else if( Work + (INT)RandAdjust < 0 ) Work = 0;
			else Work += (INT)RandAdjust;

			TI->SetLayerAlpha( Vertex->X, Vertex->Y, GTerrainTools.CurrentLayer-1, Work, GTerrainTools.CurrentAlphaMap );
		}

		EndPainting( AlphaTexture, TI );
	}

	unguard;
}

/*------------------------------------------------------------------------------
	UTerrainBrushFlatten

	Flattens selected vertices to the height of the vertex which was
	initially selected,
------------------------------------------------------------------------------*/

UTerrainBrushFlatten::UTerrainBrushFlatten()
{
	Desc = TEXT("Flatten");
	Exec = TEXT("FLATTEN");
	ID = TB_Flatten;
	bUseInnerRadius = 0;
}

UTerrainBrushFlatten::~UTerrainBrushFlatten()
{
}

void UTerrainBrushFlatten::Execute( UBOOL bAdditive )
{
	guard(UTerrainBrushFlatten::Execute);

	if( !GTerrainTools.CurrentLayer )
	{
		GTerrainTools.FindActorsToAlign();
		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();

		for( INT x = 0 ; x < TI->SelectedVertices.Num() ; x++ )
			TI->SetHeightmap( TI->SelectedVertices(x).X, TI->SelectedVertices(x).Y, GTerrainTools.RefHeight );

		TI->UpdateFromSelectedVertices();
		GTerrainTools.AdjustAlignedActors();
	}

	unguard;
}

/*------------------------------------------------------------------------------
	UTerrainBrushTexPan

	Pans the texture on the selected layer.
------------------------------------------------------------------------------*/

UTerrainBrushTexPan::UTerrainBrushTexPan()
{
	Desc = TEXT("Tex Pan");
	Exec = TEXT("TEXPAN");
	ID = TB_TexturePan;
	bUseInnerRadius = 0;
	bShowMarkers = 0;
}

UTerrainBrushTexPan::~UTerrainBrushTexPan()
{
}

void UTerrainBrushTexPan::MouseMove( FLOAT InMouseX, FLOAT InMouseY )
{
	guard(UTerrainBrushTexPan::MouseMove);

	if( GTerrainTools.CurrentLayer )
	{
		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
		TI->Layers[ GTerrainTools.CurrentLayer-1 ].UPan += InMouseX;
		TI->Layers[ GTerrainTools.CurrentLayer-1 ].VPan += InMouseY;
		TI->CalcLayerTexCoords();
		for( INT i = 0 ; i < UTexture::__Client->Viewports.Num() ; i++ )
			if( UTexture::__Client->Viewports(i)->Current )
			{
				UTexture::__Client->Viewports(i)->Repaint( 1 );
				break;
			}
	}

	unguard;
}

/*------------------------------------------------------------------------------
	UTerrainBrushTexRotate

	Rotates the texture on the selected layer.
------------------------------------------------------------------------------*/

UTerrainBrushTexRotate::UTerrainBrushTexRotate()
{
	Desc = TEXT("Tex Rotate");
	Exec = TEXT("TEXROTATE");
	ID = TB_TextureRotate;
	bUseInnerRadius = 0;
	bShowMarkers = 0;
}

UTerrainBrushTexRotate::~UTerrainBrushTexRotate()
{
}

void UTerrainBrushTexRotate::MouseMove( FLOAT InMouseX, FLOAT InMouseY )
{
	guard(UTerrainBrushTexRotate::MouseMove);

	if( GTerrainTools.CurrentLayer )
	{
		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
		TI->Layers[ GTerrainTools.CurrentLayer-1 ].TextureRotation += InMouseY;
		if( TI->Layers[ GTerrainTools.CurrentLayer-1 ].TextureRotation > 65536 )
			TI->Layers[ GTerrainTools.CurrentLayer-1 ].TextureRotation -=  65536;
		if( TI->Layers[ GTerrainTools.CurrentLayer-1 ].TextureRotation < 0 )
			TI->Layers[ GTerrainTools.CurrentLayer-1 ].TextureRotation +=  65536;
		TI->CalcLayerTexCoords();
		for( INT i = 0 ; i < UTexture::__Client->Viewports.Num() ; i++ )
			if( UTexture::__Client->Viewports(i)->Current )
			{
				UTexture::__Client->Viewports(i)->Repaint( 1 );
				break;
			}
	}

	unguard;
}

/*------------------------------------------------------------------------------
	UTerrainBrushTexScale

	Scales the texture on the selected layer.
------------------------------------------------------------------------------*/

UTerrainBrushTexScale::UTerrainBrushTexScale()
{
	Desc = TEXT("Tex Scale");
	Exec = TEXT("TEXSCALE");
	ID = TB_TextureScale;
	bUseInnerRadius = 0;
	bShowMarkers = 0;
}

UTerrainBrushTexScale::~UTerrainBrushTexScale()
{
}

void UTerrainBrushTexScale::MouseMove( FLOAT InMouseX, FLOAT InMouseY )
{
	guard(UTerrainBrushTexScale::MouseMove);

	if( GTerrainTools.CurrentLayer )
	{
		ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
		TI->Layers[ GTerrainTools.CurrentLayer-1 ].UScale += InMouseX;
		TI->Layers[ GTerrainTools.CurrentLayer-1 ].VScale += InMouseY;
		TI->CalcLayerTexCoords();
		for( INT i = 0 ; i < UTexture::__Client->Viewports.Num() ; i++ )
			if( UTexture::__Client->Viewports(i)->Current )
			{
				UTexture::__Client->Viewports(i)->Repaint( 1 );
				break;
			}
	}

	unguard;
}

/*------------------------------------------------------------------------------
	UTerrainBrushSelect

	Selects areas of the terrain for copy/paste, terrain generation, etc
------------------------------------------------------------------------------*/

UTerrainBrushSelect::UTerrainBrushSelect()
{
	Desc = TEXT("Select");
	Exec = TEXT("SELECT");
	ID = TB_Select;
	bUseInnerRadius = 0;
	bShowMarkers = 0;
	bShowVertices = 0;
	bShowRect = 1;
	bForceSoftSel = 0;
	bAllowSoftSel = 0;
	SelMin = SelMax = FVector(0,0,0);
}

UTerrainBrushSelect::~UTerrainBrushSelect()
{
}

void UTerrainBrushSelect::Execute( UBOOL bAdditive )
{
	guard(UTerrainBrushSelect::Execute);

	ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
	if( TI )
	{
		FBox Selection = GetRect();
		TI->SelectVerticesInBox( Selection );
	}

	unguard;
}

void UTerrainBrushSelect::MouseMove( FLOAT InMouseX, FLOAT InMouseY )
{
	guard(UTerrainBrushSelect::MouseMove);

	SelMax = GTerrainTools.PointingAt;

	unguard;
}

void UTerrainBrushSelect::MouseButtonDown( UViewport* InViewport )
{
	guard(UTerrainBrushSelect::MouseButtonDown);

	UTerrainBrush::MouseButtonDown( InViewport );

	if( InViewport->Input->KeyDown(IK_Ctrl) && InViewport->Input->KeyDown(IK_LeftMouse) )
		SelMin = SelMax = GTerrainTools.PointingAt;

	unguard;
}

FBox UTerrainBrushSelect::GetRect()
{
	guard(UTerrainBrushSelect::GetRect);

	FBox Ret(1);
	Ret += SelMin;
	Ret += SelMax;
	return Ret;

	unguard;
}

/*------------------------------------------------------------------------------
	UTerrainBrushVisibility

	Toggles the visibility of terrain quads
	(2 triangles equating to 1 pixel on the heightmap).
------------------------------------------------------------------------------*/

UTerrainBrushVisibility::UTerrainBrushVisibility()
{
	Desc = TEXT("Visibility");
	Exec = TEXT("VISIBILITY");
	ID = TB_Visibility;
	bUseInnerRadius = 0;
	bShowMarkers = 1;
	bShowVertices = 0;
	bShowRect = 1;
	bForceSoftSel = 0;
	bAllowSoftSel = 1;
	bShowRedVertex = 0;
}

UTerrainBrushVisibility::~UTerrainBrushVisibility()
{
}

void UTerrainBrushVisibility::Execute( UBOOL bAdditive )
{
	guard(UTerrainBrushVisibility::Execute);

	ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
	if( TI && TI->SelectedVertices.Num() )
	{
		for( INT i=0;i<TI->SelectedVertices.Num();i++ )
			TI->SetQuadVisibilityBitmap( TI->SelectedVertices(i).X, TI->SelectedVertices(i).Y, bAdditive );

		TI->UpdateFromSelectedVertices();
	}

	unguard;
}

FBox UTerrainBrushVisibility::GetRect()
{
	guard(UTerrainBrushVisibility::GetRect);

	FBox Ret(1);
	Ret += GTerrainTools.PointingAt;
	ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
	if( TI )
		Ret += GTerrainTools.PointingAt + FVector( TI->TerrainScale.X, TI->TerrainScale.Y, 0 );
	return Ret;

	unguard;
}

/*------------------------------------------------------------------------------
	UTerrainBrushEdgeTurn

	Turns the edges of terrain quad triangles
	(2 triangles equating to 1 pixel on the heightmap).
------------------------------------------------------------------------------*/

UTerrainBrushEdgeTurn::UTerrainBrushEdgeTurn()
{
	Desc = TEXT("Edge Turn");
	Exec = TEXT("EDGETURN");
	ID = TB_EdgeTurn;
	bUseInnerRadius = 0;
	bShowMarkers = 0;
	bShowVertices = 0;
	bShowRect = 1;
	bForceSoftSel = 0;
	bAllowSoftSel = 0;
	bShowRedVertex = 0;
}

UTerrainBrushEdgeTurn::~UTerrainBrushEdgeTurn()
{
}

void UTerrainBrushEdgeTurn::Execute( UBOOL bAdditive )
{
	guard(UTerrainBrushEdgeTurn::Execute);

	ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
	if( TI && TI->SelectedVertices.Num() )
	{
		for( INT i=0;i<TI->SelectedVertices.Num();i++ )
			TI->SetEdgeTurnBitmap( TI->SelectedVertices(i).X, TI->SelectedVertices(i).Y, bAdditive );

		TI->UpdateFromSelectedVertices();
	}

	unguard;
}

FBox UTerrainBrushEdgeTurn::GetRect()
{
	guard(UTerrainBrushEdgeTurn::GetRect);

	FBox Ret(1);
	Ret += GTerrainTools.PointingAt;
	ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
	if( TI )
		Ret += GTerrainTools.PointingAt + FVector( TI->TerrainScale.X, TI->TerrainScale.Y, 0 );
	return Ret;

	unguard;
}


/*------------------------------------------------------------------------------
	FTerrainTools.

	A helper class to store the state of the various terrain tools.
------------------------------------------------------------------------------*/

FTerrainTools::FTerrainTools()
{
}

FTerrainTools::~FTerrainTools()
{
	//Brushes.Empty();
}

void FTerrainTools::Init()
{
	guard(FTerrainTools::Init);

	InnerRadius = 256;
	OuterRadius = 1024;
	Strength = 100;
	Adjust = 32;
	MirrorAxis = MIRRORAXIS_NONE;
	bPerTool = 0;
	EditorMode = 0;
	TerrainEditBrush = 0;
	bAutoSoftSel = 1;
	bIgnoreInvisibleQuads = 0;
	bMoveActors = 1;
	bShowDecoAlpha = 0;
	bLockSliders = 0;
	bFirstClick = 0;
	WhiteCircle = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.S_WhiteCircle") ));
	check(WhiteCircle);
	EyeBall = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.S_Camera") ));
	check(EyeBall);
	CurrentTerrain = NULL;
	CurrentLayer = -1;
	CurrentAlphaMap = NULL;
	LayerScrollPos = 0;
	LayerScrollMax = 0;
	DecoLayerScrollPos = 0;
	DecoLayerScrollMax = 0;
	RefHeight = 0;
	PointingAt = FVector(0,0,0);
	Color = FColor(255,255,255);

	// Create the list of terrain brushes.
	Brushes.Empty();
	new( Brushes )UTerrainBrushVertexEdit();
	new( Brushes )UTerrainBrushSelect();
	new( Brushes )UTerrainBrushPaint();
	//new( Brushes )UTerrainBrushColor();	// Doesn't work in UT2003, so don't bother making it available.
	new( Brushes )UTerrainBrushSmooth();
	new( Brushes )UTerrainBrushNoise();
	new( Brushes )UTerrainBrushFlatten();
	new( Brushes )UTerrainBrushVisibility();
	new( Brushes )UTerrainBrushEdgeTurn();
	new( Brushes )UTerrainBrushTexPan();
	new( Brushes )UTerrainBrushTexRotate();
	new( Brushes )UTerrainBrushTexScale();

	SetCurrentBrush( TB_VertexEdit );

	unguard;
}

void FTerrainTools::SetCurrentBrush( INT InID )
{
	guard(FTerrainTools::SetCurrentBrush);

	if( CurrentTerrain )
		CurrentTerrain->SelectedVertices.Empty();

	for( INT x = 0 ; x < Brushes.Num() ; x++ )
		if( Brushes(x).ID == InID )
		{
			CurrentBrush = &Brushes(x);
			TerrainEditBrush = InID;
			return;
		}

	check(0);	// Invalid ID
	unguard;
}

void FTerrainTools::SetCurrentTerrainInfo( ATerrainInfo* InTerrainInfo )
{
	guard(FTerrainTools::SetCurrentTerrainInfo);
	if( CurrentTerrain == InTerrainInfo ) return;

	CurrentTerrain = InTerrainInfo;
	CurrentLayer = 0;
	CurrentAlphaMap = NULL;
	LayerScrollPos = 0;
	DecoLayerScrollPos = 0;
	unguard;
}

FString FTerrainTools::GetExecFromBrushName( FString& InBrushName )
{
	guard(FTerrainTools::GetExecFromBrushName);

	for( INT x = 0 ; x < Brushes.Num() ; x++ )
		if( Brushes(x).Desc == InBrushName )
			return Brushes(x).Exec;

	return TEXT("NONE");

	unguard;
}

void FTerrainTools::FindActorsToAlign()
{
	guard(FTerrainTools::FindActorsToAlign);

	AttachedActorOffsets.Empty();

	INT MinX=MAXINT, MinY=MAXINT, MaxX=0, MaxY=0;
	for( INT v=0;v<CurrentTerrain->SelectedVertices.Num();v++ )
	{
		INT x = CurrentTerrain->SelectedVertices(v).X;
		INT y = CurrentTerrain->SelectedVertices(v).Y;
		if( x < MinX )
			MinX = x;
		if( x > MaxX )
			MaxX = x;
		if( y < MinY )
			MinY = y;
		if( y > MaxY )
			MaxY = y;
	}
	MaxX++;
	MaxY++;
	MinX--;
	MinY--;

	for( INT i=0; i<CurrentTerrain->Level->XLevel->Actors.Num(); i++ )
	{
		AActor* Actor = Cast<AActor>(CurrentTerrain->Level->XLevel->Actors(i));

		if( Actor && Actor->bAutoAlignToTerrain )
		{
			FVector LocalLocation = CurrentTerrain->WorldToHeightmap(Actor->Location);
			if( LocalLocation.X >= MinX && LocalLocation.X <= MaxX &&
				LocalLocation.Y >= MinY && LocalLocation.Y <= MaxY )
			{
				FVector Direction = FVector(0,0,-1);
				FVector Origin = Actor->Location - Direction*HALF_WORLD_MAX;
				FCheckResult Hit(1.0f);
				if( CurrentTerrain->LineCheck( Hit, Origin + Direction*WORLD_MAX, Origin, FVector(0,0,0), 0, 1 )==0 )
				{
					INT newitem = AttachedActorOffsets.Add();
					AttachedActorOffsets(newitem).Actor = Actor;
					AttachedActorOffsets(newitem).Height = Actor->Location.Z - Hit.Location.Z;
				}
			}
		}
	}

	unguard;
}

void FTerrainTools::AdjustAlignedActors()
{
	guard(FTerrainTools::AdjustAlignedActors);

	for( INT i=0; i<AttachedActorOffsets.Num(); i++ )
	{
		AActor* Actor = AttachedActorOffsets(i).Actor;

		FVector Direction = FVector(0,0,-1);
		FVector Origin = Actor->Location - Direction*HALF_WORLD_MAX;
		FCheckResult Hit(1.0f);
		if( CurrentTerrain->LineCheck( Hit, Origin + Direction*WORLD_MAX, Origin, FVector(0,0,0), 0, 1 )==0 )
		{
			FVector NewLocation = Actor->Location;
			NewLocation.Z = Hit.Location.Z + AttachedActorOffsets(i).Height;
			
			Actor->Location = NewLocation;
			//Actor->Level->FarMoveActor( Actor, NewLocation );
			Actor->PostEditChange();
		}
	}
	AttachedActorOffsets.Empty();

	unguard;
}


/*------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------*/


