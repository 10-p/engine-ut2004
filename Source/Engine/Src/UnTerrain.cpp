/*=============================================================================
	UnTerrain.cpp: Unreal objects
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Jack Porter
=============================================================================*/

#include "EnginePrivate.h"
#include <math.h>

#include "UnRenderPrivate.h"

#define PROJECTOR_BBOX_TEST 0 // sjs
#define NO_PROJECTOR_CLIP 0 // sjs - merge_hack , I disabled this

#define MAXSTREAMVERTICES 65535
#define TERRAINSECTORSIZE (TerrainSectorSize)

#ifndef CONSOLE
extern ENGINE_API FTerrainTools		GTerrainTools;
extern ENGINE_API FRebuildTools		GRebuildTools;
#endif

// Only the modes we need to know about are duplicated here.
enum EEditorMode
{
	EM_TerrainEdit		= 28,	// Terrain editing.
};

// Terrain sector sorting.
static INT IntSortOrder = 1;
static inline INT Compare(INT& A,INT& B)
{
	return (A - B) * IntSortOrder;
}

/*------------------------------------------------------------------------------
    UTerrainSector Implementation.
------------------------------------------------------------------------------*/

UTerrainSector::UTerrainSector( ATerrainInfo* InInfo, INT InOffsetX, INT InOffsetY, INT InQuadsX, INT InQuadsY )
:	Info		( InInfo )
,	OffsetX		( InOffsetX )
,	OffsetY		( InOffsetY )
,	QuadsX		( InQuadsX )
,	QuadsY		( InQuadsY )
,	VertexStreamNum(-1)
{
	guard(UTerrainSector::UTerrainSector);
	unguard;
}

void UTerrainSector::Serialize(FArchive& Ar)
{
	guard(UTerrainSector::Serialize);
	Super::Serialize(Ar);

	if( Ar.Ver() < 94 )
	{
		// UTerrainSector used to be a UPrimitive
		FBox TempBox;
		FSphere TempSphere;
		Ar << TempBox << TempSphere;
	}

	Ar	<< Info;

	Ar 	<< QuadsX << QuadsY
		<< OffsetX << OffsetY;

	if( Ar.Ver() < 117 )
	{
		BoundingBox.Init();
		FVector TempBounds;
		for( INT i=0;i<8;i++ )
		{
			Ar << TempBounds;
			BoundingBox += TempBounds;
		}
	}
	else
		Ar << BoundingBox;

	Ar << LightInfos;

	if(Ar.Ver() < 89)
	{
			UVertexStreamCOLOR*	TempStream = NULL;
			Ar << TempStream;
	}

	if(Ar.Ver() < 117 )
	{
		FRawColorStream TempColorStream;
		Ar << TempColorStream;
		}

	if( Ar.IsLoading() )
		VertexStreamNum = -1;

	if( !(Ar.IsLoading() || Ar.IsSaving()) )
	{
		Ar	<< CompleteIndexBuffer 
			<< CompleteNumTriangles 
			<< CompleteNumIndices 
			<< CompleteMinIndex 
			<< CompleteMaxIndex 
			<< Projectors
			<< RenderPasses
			<< VertexStreamNum;
	}

	unguard;
}

void UTerrainSector::PostLoad()
{
	guard(UTerrainSector::PostLoad);
	Super::PostLoad();

#ifdef __PSX2_EE__
	// SL TODO: Don't call UpdateTriangles, if it's possible
	extern void PreCachePS2Terrain(UTerrainSector* Sector);
	PreCachePS2Terrain(this);
#endif //__PSX2_EE__

	unguard;	
}

void UTerrainSector::Destroy()
{
	guard(UTerrainSector::Destroy);
	Super::Destroy();
	for(INT ProjectorIndex = 0;ProjectorIndex < Projectors.Num();ProjectorIndex++)
	{
		Projectors(ProjectorIndex).Info->RenderInfo->RemoveReference();
		delete Projectors(ProjectorIndex).Info;
	}
	unguard;
}

//
// Terrain pass/layer triangluation and optimization
//
UBOOL UTerrainSector::IsSectorAll( INT Layer, BYTE AlphaValue )
{
	guard(UTerrainSector::IsSectorAll);

	UTexture* AlphaMap = Info->Layers[Layer].AlphaMap;
	FLOAT Ratio = (FLOAT)AlphaMap->USize / (FLOAT)Info->HeightmapX;

	INT MinX = appFloor(Ratio * OffsetX);
	INT MaxX = appCeil(Ratio * (OffsetX + QuadsX));
	INT MinY = appFloor(Ratio * OffsetY);
	INT MaxY = appCeil(Ratio * (OffsetY + QuadsY));

	for( INT x=MinX;x<=MaxX;x++ )
		for( INT y=MinY;y<=MaxY;y++ )
			if( Info->GetLayerAlpha( x, y, -2, AlphaMap ) != AlphaValue )
				return 0;

	return 1;
	unguard;
}

UBOOL UTerrainSector::IsTriangleAll( INT Layer, INT X, INT Y, INT Tri, INT Turned, BYTE AlphaValue )
{
    guard(UTerrainSector::IsTriangleAll);
    
    UTexture* AlphaMap = Info->Layers[Layer].AlphaMap;
    
    //!! Alphamaps must be square!
    check(AlphaMap);
    check(AlphaMap->USize == AlphaMap->VSize)
        
        // Special-case 1:1 alphamap:heightmap ratio for performance
        if( AlphaMap->USize == (FLOAT)Info->HeightmapX )
        {
            INT x = X+OffsetX;
            INT y = Y+OffsetY;
            
            if( Turned )
            {
                if( Tri )
                {
                    // 432
                    if( Info->GetLayerAlpha( x,   y+1, -2, AlphaMap ) != AlphaValue ||
                        Info->GetLayerAlpha( x+1, y+1, -2, AlphaMap ) != AlphaValue ||
                        Info->GetLayerAlpha( x+1, y,   -2, AlphaMap ) != AlphaValue )
                        return 0;
                }
                else
                {				
                    // 142
                    if( Info->GetLayerAlpha( x,   y,   -2, AlphaMap ) != AlphaValue ||
                        Info->GetLayerAlpha( x,   y+1, -2, AlphaMap ) != AlphaValue ||
                        Info->GetLayerAlpha( x+1, y,   -2, AlphaMap ) != AlphaValue )
                        return 0;
                }
            }
            else
            {
                if( Tri )
                {
                    // 132
                    if( Info->GetLayerAlpha( x,   y,   -2, AlphaMap ) != AlphaValue ||
                        Info->GetLayerAlpha( x+1, y+1, -2, AlphaMap ) != AlphaValue ||
                        Info->GetLayerAlpha( x+1, y,   -2, AlphaMap ) != AlphaValue )
                        return 0;
                }
                else
                {
                    // 143
                    if( Info->GetLayerAlpha( x,   y,   -2, AlphaMap ) != AlphaValue ||
                        Info->GetLayerAlpha( x,   y+1, -2, AlphaMap ) != AlphaValue ||
                        Info->GetLayerAlpha( x+1, y+1, -2, AlphaMap ) != AlphaValue )
                        return 0;
                }
            }
            
        }
        else
        {
            FLOAT Ratio = (FLOAT)AlphaMap->USize / (FLOAT)Info->HeightmapX;
            
            INT MinX = appFloor(Ratio * (X + OffsetX));
            INT MaxX = appCeil(Ratio * (X + OffsetX + 1));
            INT MinY = appFloor(Ratio * (Y + OffsetY));
            INT Range = MaxX - MinX;
            
            if( Turned )
            {
                if( Tri )
                {				
                    // 432
                    for( INT x=0; x<=Range; x++ )
                        for( INT y=Range;y>=Range-x;y-- )
                            if( Info->GetLayerAlpha( x+MinX, y+MinY, -2, AlphaMap ) != AlphaValue )
                                return 0;
                }	
                else
                {
                    // 142
                    for( INT x=0; x<=Range; x++ )
                        for( INT y=0;y<=Range-x;y++ )
                            if( Info->GetLayerAlpha( x+MinX, y+MinY, -2, AlphaMap ) != AlphaValue )
                                return 0;
                }
            }
            else
            {
                if( Tri )
                {
                    // 132
                    for( INT x=0; x<=Range; x++ )
                        for( INT y=0;y<=x;y++ )
                            if( Info->GetLayerAlpha( x+MinX, y+MinY, -2, AlphaMap ) != AlphaValue )
                                return 0;
                }
                else
                {
                    // 143
                    for( INT x=0; x<=Range; x++ )
                        for( INT y=Range;y>=x;y-- )
                            if( Info->GetLayerAlpha( x+MinX, y+MinY, -2, AlphaMap ) != AlphaValue )
                                return 0;
                }
            }
        }
        
        return 1;
        
        unguard;
}

UBOOL UTerrainSector::PassShouldRenderTriangle( INT Pass, INT X, INT Y, INT Tri, INT Turned )
{
    guard(UTerrainSector::PassShouldRenderTriangle);
    
    FTerrainSectorRenderPass &RenderPass = RenderPasses(Pass);
    FTerrainRenderCombination* RenderCombination = RenderPass.GetRenderCombination(Info);
    if( RenderCombination->RenderMethod == RM_AlphaMap )
    {
        // 1. Check if this triangle is completely transparent in all layers in this pass.
        UBOOL Transparent=1;
        for( INT l=0;l<RenderCombination->Layers.Num();l++ )
        {
            if( !IsTriangleAll( RenderCombination->Layers(l), X, Y, Tri, Turned, 0 ) )
            {
                Transparent = 0;
                break;
            }
        }
        
        if( Transparent )
            return 0;
        
        
        // 2. Check if the triangle in a layer in any pass above this layer occludes this triangle.
        for( INT p=Pass+1;p<RenderPasses.Num();p++ )
        {
            for( INT l=0;l<RenderPasses(p).GetRenderCombination(Info)->Layers.Num();l++ )
            {
				if( !Info->Layers[RenderPasses(p).GetRenderCombination(Info)->Layers(l)].Texture->IsTransparent() &&
                    IsTriangleAll( RenderPasses(p).GetRenderCombination(Info)->Layers(l), X, Y, Tri, Turned, 255 ) )
                    return 0;
            }
        }
        return 1;
    }
    else
    {
        // Triangles in the first pass must always be written to the framebuffer
        if( Pass == 0 )
            return 1;
        
        FColor Black = FColor(0,0,0,255);
        INT x = X+OffsetX;
        INT y = Y+OffsetY;
        
        for( INT l=0;l<RenderCombination->Layers.Num();l++ )
        {
            UTexture* WeightMap = Info->Layers[RenderCombination->Layers(l)].LayerWeightMap;
            
            // Triangles in subsequent passes can be omitted if they're solid black
            if( Info->GetTextureColor( x,   y,   WeightMap ) != Black ||
                Info->GetTextureColor( x+1, y,   WeightMap ) != Black ||
                Info->GetTextureColor( x+1, y+1, WeightMap ) != Black ||
                Info->GetTextureColor( x,   y+1, WeightMap ) != Black )
                return 1;
        }
        return 0;
    }
    
    unguard;
}

void UTerrainSector::GenerateTriangles( UBOOL CheckLayerBlending )
{
	guard(UTerrainSector::GenerateTriangles);

	if( !Info->TerrainMap )
		return;

	//
	// Create vertex stream if necessary.
	//
	if( VertexStreamNum == -1 )
	{
		INT count = (QuadsX+1) * (QuadsY+1);
		if( !GIsEditor )
		{
			for( INT s=0;s<Info->VertexStreams.Num();s++ )
			{
				if( Info->VertexStreams(s).Vertices.Num() + count < MAXSTREAMVERTICES )
				{
					VertexStreamNum = s;
					break;
				}
			}
		}

		if( VertexStreamNum == -1 )
		{
			new(Info->VertexStreams) FTerrainVertexStream;
			VertexStreamNum = Info->VertexStreams.Num() - 1;
		}

		VertexStreamOffset = Info->VertexStreams(VertexStreamNum).Vertices.Num();
		Info->VertexStreams(VertexStreamNum).Vertices.AddZeroed(count);
	}

	//
	// Update vertex positions
	//
	BoundingBox.Init();
	FTerrainVertex* Vertex = &Info->VertexStreams(VertexStreamNum).Vertices(VertexStreamOffset);
	for( INT y=0;y<=QuadsY;y++ )
	{
		for( INT x=0;x<=QuadsX;x++ )
		{
			INT g = GetGlobalVertex(x,y);
			BoundingBox += Info->Vertices( g );

			Vertex->Position = Info->Vertices( g );
			Vertex->Normal = Info->GetVertexNormal(x+OffsetX,y+OffsetY);
			Vertex->Color = Info->VertexColors( g );
			Vertex->U = ((FLOAT)(x+OffsetX) + 0.5f) / (FLOAT)Info->HeightmapX;
			Vertex->V = ((FLOAT)(y+OffsetY) + 0.5f) / (FLOAT)Info->HeightmapY;
			Vertex++;
		}
	}
	Info->VertexStreams(VertexStreamNum).Revision++;

	if( CheckLayerBlending )
	{
		//
		// Work out layer triangulation.
		//
		TArray<INT> SectorLayers;
		for( INT l=0; l<ARRAY_COUNT(Info->Layers) && Info->Layers[l].AlphaMap && Info->Layers[l].Texture; l++ )
		{
			// if the layer is completely transparent in this sector, skip it.
			if( IsSectorAll( l, 0 ) )
				continue;

			// see if this layer is completely occluded by another layer above it in this sector.
			for( INT o=l+1;o<ARRAY_COUNT(Info->Layers) && Info->Layers[o].AlphaMap && Info->Layers[o].Texture; o++)
				if( !Info->Layers[o].Texture->IsTransparent() && IsSectorAll( o, 255) )
					goto SkipLayer;

			SectorLayers.AddItem(l);
			SkipLayer:;
		}

		// Work out the number of passes required for this sector.
		INT MaxSimultaneousLayers = Info->GetLevel()->Engine->GRenDev ? Info->GetLevel()->Engine->GRenDev->GetRenderCaps()->MaxSimultaneousTerrainLayers : 1;

		UBOOL ForceAlphaMap =  GIsEditor || (Info->GetLevel()->Engine->GRenDev ? Info->GetLevel()->Engine->GRenDev->IsVoodoo3 : 0);
		//INT MaxSimultaneousLayers = GIsEditor ? 1 : Min<INT>( Info->DrawScale, 3 );
		//UBOOL ForceAlphaMap =  GIsEditor || Info->bActorShadows;

		RenderPasses.Empty();
		TArray<INT> PassLayers;
		for( INT i=0; i<SectorLayers.Num(); i++ )
		{
			// non-texture or alphatexture: render with alphamap
			UTexture* LayerTexture = Cast<UTexture>(Info->Layers[SectorLayers(i)].Texture);
			//!!vogel: OPENGL TODO
			if( ForceAlphaMap || !LayerTexture || LayerTexture->IsTransparent() || GIsOpenGL || GIsPixomatic )
			{
				PassLayers.AddItem( SectorLayers(i) );

				FTerrainSectorRenderPass* Pass = new(RenderPasses) FTerrainSectorRenderPass;
				Pass->RenderCombinationNum = Info->GetRenderCombinationNum(PassLayers, RM_AlphaMap);					
				PassLayers.Empty();
				// all subsequent layers must be rendered individually.
				ForceAlphaMap = 1;
				continue;
			}
		
			// Look for layers we can render simultaneously.
			while( i<SectorLayers.Num() )
			{
				// Fall out if we hit a layer which can't be rendered simultaneously.
				LayerTexture = Cast<UTexture>(Info->Layers[SectorLayers(i)].Texture);
				if( !LayerTexture || LayerTexture->IsTransparent() )
				{
					i--;
					break;
				}

				// Render this layer simultaneously.
				PassLayers.AddItem( SectorLayers(i) );
				if( PassLayers.Num()<MaxSimultaneousLayers )
					i++;
				else
					break;
			}

			// Render these layers simultaneously
			if( PassLayers.Num() )
			{
				FTerrainSectorRenderPass* Pass = new(RenderPasses) FTerrainSectorRenderPass;
				//!!powervr_aaron: Only pixel-shaders can use the combined weight maps
				if( PassLayers.Num() <= 2 || !Info->GetLevel()->Engine->GRenDev->GetRenderCaps()->PixelShaderVersion )
					Pass->RenderCombinationNum = Info->GetRenderCombinationNum(PassLayers, RM_WeightMap);		
				else
					Pass->RenderCombinationNum = Info->GetRenderCombinationNum(PassLayers, RM_CombinedWeightMap);		
				PassLayers.Empty();
			}
		}

	#if 0
		// Validate
		TArray<INT> CheckLayers;
		for( INT p=0;p<RenderPasses.Num();p++ )
		{
			FTerrainRenderCombination& RC = Info->RenderCombinations(RenderPasses(p).RenderCombinationNum);
			for( INT l=0;l<RC.Layers.Num();l++ )
			{
				// Check each layer is used only once.
				check( CheckLayers.FindItemIndex(RC.Layers(l)) == INDEX_NONE );
				CheckLayers.AddItem(RC.Layers(l));
			}
		}

		// Check each layer affecting the sector is rendered
		for( INT l=0;l<SectorLayers.Num();l++ )
			check( CheckLayers.FindItemIndex(SectorLayers(l)) != INDEX_NONE );

		// Check there are no extra layers
		check( CheckLayers.Num() == CheckLayers.Num() );
		// End validation
	#endif

		// triangulate
		for( INT pass=0; pass<RenderPasses.Num(); pass++ )
		{
			// If layer is visible, build index buffer for it.
			RenderPasses(pass).Indices.Empty();
			RenderPasses(pass).NumTriangles = 0;

			for( INT y=0;y<QuadsY;y++ )
			{
				for( INT x=0;x<QuadsX;x++ )
				{
					if( Info->GetQuadVisibilityBitmap( x+OffsetX, y+OffsetY ) )
					{
						INT V1 = GetLocalVertex( x, y ) + VertexStreamOffset;
						INT V2 = V1+1;
						INT V3 = GetLocalVertex( x+1, y+1 ) + VertexStreamOffset;
						INT V4 = V3-1;
	                    
						if( Info->GetEdgeTurnBitmap( x+OffsetX, y+OffsetY ) )
						{
							// triangle 1
							if( pass == 0 || PassShouldRenderTriangle( pass, x, y, 0, 1 ) )
							{
								RenderPasses(pass).Indices.AddItem(V1);
								RenderPasses(pass).Indices.AddItem(V4);
								RenderPasses(pass).Indices.AddItem(V2);
								RenderPasses(pass).NumTriangles++;
							}
							// triangle 2
							if( pass == 0 || PassShouldRenderTriangle( pass, x, y, 1, 1 ) )
							{
								RenderPasses(pass).Indices.AddItem(V4);
								RenderPasses(pass).Indices.AddItem(V3);
								RenderPasses(pass).Indices.AddItem(V2);
								RenderPasses(pass).NumTriangles++;
							}
						}
						else
						{
							// triangle 1
							if( pass == 0 || PassShouldRenderTriangle( pass, x, y, 0, 0 ) )
							{
								RenderPasses(pass).Indices.AddItem(V1);
								RenderPasses(pass).Indices.AddItem(V4);
								RenderPasses(pass).Indices.AddItem(V3);
								RenderPasses(pass).NumTriangles++;
							}
							// triangle 2
							if( pass == 0 || PassShouldRenderTriangle( pass, x, y, 1, 0 ) )
							{
								RenderPasses(pass).Indices.AddItem(V1);
								RenderPasses(pass).Indices.AddItem(V3);
								RenderPasses(pass).Indices.AddItem(V2);
								RenderPasses(pass).NumTriangles++;
							}
						}
					}
				}
			}

			RenderPasses(pass).NumIndices = RenderPasses(pass).Indices.Num();
			if( RenderPasses(pass).NumIndices )
			{
				// Work out the min and max indices
				RenderPasses(pass).MinIndex = MAXINT;
				RenderPasses(pass).MaxIndex = 0;
				for( INT j=0;j<RenderPasses(pass).NumIndices;j++ )
				{
					if( RenderPasses(pass).Indices(j) < RenderPasses(pass).MinIndex )
						RenderPasses(pass).MinIndex = RenderPasses(pass).Indices(j);
					if( RenderPasses(pass).Indices(j) > RenderPasses(pass).MaxIndex )
						RenderPasses(pass).MaxIndex = RenderPasses(pass).Indices(j);
				}
			}
			else
			{
				// Remove this pass if there are no triangles to render.
				RenderPasses.Remove(pass--);
			}
		}

		// Complete sector index buffer for rendering editor tools and wireframe.
		CompleteIndexBuffer.Indices.Empty();
		CompleteNumTriangles = 0;
		for( INT y=0;y<QuadsY;y++ )
		{
			for( INT x=0;x<QuadsX;x++ )
			{				
				if( Info->GetQuadVisibilityBitmap( x+OffsetX, y+OffsetY ) )
				{
					INT V1 = GetLocalVertex( x, y ) + VertexStreamOffset;
					INT V2 = V1+1;
					INT V3 = GetLocalVertex( x+1, y+1 ) + VertexStreamOffset;
					INT V4 = V3-1;
	    
					if( Info->GetEdgeTurnBitmap( x+OffsetX, y+OffsetY ) )
					{
						// triangle 1
						CompleteIndexBuffer.Indices.AddItem(V1);
						CompleteIndexBuffer.Indices.AddItem(V4);
						CompleteIndexBuffer.Indices.AddItem(V2);
						// triangle 2
						CompleteIndexBuffer.Indices.AddItem(V4);
						CompleteIndexBuffer.Indices.AddItem(V3);
						CompleteIndexBuffer.Indices.AddItem(V2);
						CompleteNumTriangles += 2;
					}
					else
					{
						// triangle 1
						CompleteIndexBuffer.Indices.AddItem(V1);
						CompleteIndexBuffer.Indices.AddItem(V4);
						CompleteIndexBuffer.Indices.AddItem(V3);
						// triangle 2
						CompleteIndexBuffer.Indices.AddItem(V1);
						CompleteIndexBuffer.Indices.AddItem(V3);
						CompleteIndexBuffer.Indices.AddItem(V2);
						CompleteNumTriangles += 2;
					}
				}
			}
		}

		CompleteNumIndices = CompleteIndexBuffer.Indices.Num();
		CompleteMinIndex = MAXINT;
		CompleteMaxIndex = 0;
		for( INT j=0;j<CompleteNumIndices;j++ )
		{
			if( CompleteIndexBuffer.Indices(j) < CompleteMinIndex )
				CompleteMinIndex = CompleteIndexBuffer.Indices(j);
			if( CompleteIndexBuffer.Indices(j) > CompleteMaxIndex )
				CompleteMaxIndex = CompleteIndexBuffer.Indices(j);
		}
		CompleteIndexBuffer.Revision++;
	}

	// Calculate the location/ center of the sector.
	FVector P1	= Info->Vertices(GetGlobalVertex(0,0));
	FVector P2	= Info->Vertices(GetGlobalVertex(0,QuadsY));
	FVector P3	= Info->Vertices(GetGlobalVertex(QuadsX,QuadsY));
	FVector P4	= Info->Vertices(GetGlobalVertex(QuadsX,0));

	Location	= P1 + (P2 - P1) / 2.f + (P4 - P1) / 2.f;
	Radius		= FDist( Location, P1 );

	// Update Decorations.
	INT SectorIndex;
	if ( Info->Sectors.FindItem( this, SectorIndex ) )
		Info->UpdateDecorations( SectorIndex );

	// Update static projectors
	guard(UpdateStaticProjectors);
	for( INT i=0;i<Projectors.Num();i++ )
	{
		FStaticProjectorVertex*	DestVertex = &Projectors(i).Info->Vertices(0);
		for(INT Y = Projectors(i).MinQuadY;Y <= Projectors(i).MaxQuadY + 1;Y++)
		{
			for(INT X = Projectors(i).MinQuadX;X <= Projectors(i).MaxQuadX + 1;X++)
			{
				DestVertex->WorldPosition = Info->Vertices(GetGlobalVertex(X,Y));
				DestVertex++;
			}
		}
	}
	unguard;

	unguard;
}

#ifndef _XBOX
inline
#endif
INT UTerrainSector::GetGlobalVertex( INT x, INT y )
{
	return (x+OffsetX)+(y+OffsetY)*Info->HeightmapX;
}

// sjs ---
void ATerrainInfo::SmoothColors( void )
{
	TArray<FPlane> VertexClr;
	VertexClr.Add( HeightmapX*HeightmapY );

    TArray<FColor> RawVertexClr;
    RawVertexClr.Add( HeightmapX*HeightmapY );

	int i, x, y, a, b;

	GWarn->StatusUpdatef(0,3,TEXT("Smoothing Terrain colors..."));

    UBOOL Override = 0;

    if( VertexLightMap )
    {
        VertexLightMap->Mips(0).DataArray.Load();
        VertexLightMap->PostLoad();
        if( VertexLightMap->Format!=TEXF_RGBA8 )
        {
            GWarn->Logf(TEXT("VertexLightMap must be 32 bit color!"));
        }
        else if ( VertexLightMap->USize!=HeightmapX || VertexLightMap->VSize!=HeightmapY )
        {
            GWarn->Logf(TEXT("VertexLightMap must match heightfield dimesions!"));
        }
        else
        {
            Override = 1;
            appMemcpy( &RawVertexClr(0), &VertexLightMap->Mips(0).DataArray(0), HeightmapY * HeightmapX * sizeof(RawVertexClr(0)));
            for( int i=0; i<VertexClr.Num(); i++ )
            {
                VertexClr( i ).X = (float)RawVertexClr(i).R;
			    VertexClr( i ).Y = (float)RawVertexClr(i).G;
			    VertexClr( i ).Z = (float)RawVertexClr(i).B;
                VertexClr( i ).W = (float)RawVertexClr(i).A;
            }
        }
    }
    else
    {
	    // pull colors into a single large color buffer from sectors
	    for( i=0;i<Sectors.Num();i++ )
	    {
			FTerrainVertexStream& VertexStream = VertexStreams(Sectors(i)->VertexStreamNum);
		    for( y=0;y<=Sectors(i)->QuadsY;y++ )
		    {
			    for( x=0;x<=Sectors(i)->QuadsX;x++ )
			    {
				    INT GlobalVertex = Sectors(i)->GetGlobalVertex(x,y);
				    INT LocalVertex = Sectors(i)->GetLocalVertex(x,y);
					FColor clr = VertexStream.Vertices(LocalVertex+Sectors(i)->VertexStreamOffset).Color;
				    VertexClr( GlobalVertex ).X = (float)clr.R;
				    VertexClr( GlobalVertex ).Y = (float)clr.G;
				    VertexClr( GlobalVertex ).Z = (float)clr.B;
				    VertexClr( GlobalVertex ).W = (float)clr.A;
                    RawVertexClr( GlobalVertex ) = clr;
			    }
		    }
	    }
    }

    if( !VertexLightMap ) // export the raytraced result and then smooth it
    {
        UTexture* pExportTex;
        FName TextureName( *FString::Printf(TEXT("%sRayTrace"), GetName() ) );
		pExportTex = CastChecked<UTexture>(StaticConstructObject(UTexture::StaticClass(),GetOuter(),TextureName,RF_Public|RF_Standalone));
		pExportTex->Format = TEXF_RGBA8;
		pExportTex->Init( HeightmapX, HeightmapY );
		pExportTex->PostLoad();
		appMemcpy( &pExportTex->Mips(0).DataArray(0), &RawVertexClr(0), HeightmapX*HeightmapY*sizeof(RawVertexClr(0)) );

        GWarn->StatusUpdatef(1,3,TEXT("Smoothing Terrain colors..."));

	    // average the colors
	    float box[3][3] =	{	{ 1.0f, 2.0f, 1.0f },
							    { 2.0f, 4.0f, 2.0f },
							    { 1.0f, 2.0f, 1.0f } };
	    for( y=1; y<HeightmapY-1; y++ )
	    {
		    for( x=1; x<HeightmapX-1; x++ )
		    {
			    FPlane color = FPlane(0.0f,0.0f,0.0f,0.0f);
			    float wt = 0.0f;
			    for ( a=-1; a<2; a++ )
			    {
				    for ( b=-1; b<2; b++ )
				    {
					    int idx = GetGlobalVertex(x+a, y+b);
					    color += VertexClr( idx ) * box[a+1][b+1];
					    wt += box[a+1][b+1];
				    }
			    }
			    color /= wt;
			    VertexClr( GetGlobalVertex(x,y) ) = color;
		    }
	    }

	    GWarn->StatusUpdatef(2,3,TEXT("Smoothing Terrain colors..."));
    }

	// put colors back into all the Sector's color stream
	for( i=0;i<Sectors.Num();i++ )
	{
		FTerrainVertexStream& VertexStream = VertexStreams(Sectors(i)->VertexStreamNum);
		for( y=0;y<=Sectors(i)->QuadsY;y++ )
		{
			for( x=0;x<=Sectors(i)->QuadsX;x++ )
			{
				INT GlobalVertex = Sectors(i)->GetGlobalVertex(x,y);
				INT LocalVertex = Sectors(i)->GetLocalVertex(x,y);
				FPlane color = VertexClr( GlobalVertex );
				VertexStream.Vertices(LocalVertex+Sectors(i)->VertexStreamOffset).Color.R = Min((INT)color.X,255);
				VertexStream.Vertices(LocalVertex+Sectors(i)->VertexStreamOffset).Color.G = Min((INT)color.Y,255);
				VertexStream.Vertices(LocalVertex+Sectors(i)->VertexStreamOffset).Color.B = Min((INT)color.Z,255);
                // copy into global colors
			    VertexColors(GlobalVertex).R = Min((INT)color.X,255);
                VertexColors(GlobalVertex).G = Min((INT)color.Y,255);
                VertexColors(GlobalVertex).B = Min((INT)color.Z,255);
			}
		}
		VertexStream.Revision++;
	}
}

void ColorSubtract(FColor& dst, FColor& src); /// sjs test

static inline INT Compare(FTerrainSectorLightInfo& A, FTerrainSectorLightInfo& B) // sjs - test
{
    if( A.LightActor->LightEffect == LE_Negative && B.LightActor->LightEffect != LE_Negative )
        return 1;
    if( B.LightActor->LightEffect == LE_Negative && A.LightActor->LightEffect != LE_Negative )
        return -1;
    return 0;
}
// --- sjs

void UTerrainSector::StaticLight( UBOOL Force )
{
	if( !Force )
	{
		UBOOL TerrainLightingChanged = 0;
		for( INT light=0; light<LightInfos.Num();light++ )
		{
			if( LightInfos(light).LightActor->bDynamicLight || LightInfos(light).LightActor->bLightChanged )
			{
				TerrainLightingChanged = 1;
				break;
			}
		}

		if( !TerrainLightingChanged )
			return;
	}

	// Set ambient color
	AZoneInfo* ZoneActor = Info->Region.Zone;
	FPlane	AbmientColor = FGetHSV( ZoneActor->AmbientHue, ZoneActor->AmbientSaturation, ZoneActor->AmbientBrightness ) * ZoneActor->Level->Brightness * 0.5f;
	
	INT VertexStreamMax = (QuadsX+1)*(QuadsY+1)+VertexStreamOffset;
	FTerrainVertexStream& VertexStream = Info->VertexStreams(VertexStreamNum);

	for(INT VertexIndex = VertexStreamOffset;VertexIndex < VertexStreamMax;VertexIndex++)
		VertexStream.Vertices(VertexIndex).Color =
			FColor((BYTE) Min<INT>(255*AbmientColor.X,255),(BYTE) Min<INT>(255*AbmientColor.Y,255),(BYTE) Min<INT>(255*AbmientColor.Z,255), 255);

    Sort( &LightInfos(0), LightInfos.Num()); // sjs
	for( INT InfoIndex=0; InfoIndex<LightInfos.Num();InfoIndex++ )
	{
		TArray<BYTE>&	VisibilityBitmap = LightInfos(InfoIndex).VisibilityBitmap;
		INT				BitmapIndex = 0;
		BYTE			BitMask = 1;
		FDynamicLight*	DynamicLight = LightInfos(InfoIndex).LightActor->GetLightRenderData();

		for(INT VertexIndex = VertexStreamOffset;VertexIndex < VertexStreamMax;VertexIndex++)
		{
			if(VisibilityBitmap(BitmapIndex) & BitMask)
            {
                if( DynamicLight->Actor->LightEffect == LE_Negative ) // sjs
                {
                    FColor c = FColor(DynamicLight->Color * DynamicLight->SampleIntensity(VertexStream.Vertices(VertexIndex).Position, VertexStream.Vertices(VertexIndex).Normal));
                    ColorSubtract( VertexStream.Vertices(VertexIndex).Color, c );
                }
                else
                {
				    VertexStream.Vertices(VertexIndex).Color += FColor(DynamicLight->Color * DynamicLight->SampleIntensity(VertexStream.Vertices(VertexIndex).Position, VertexStream.Vertices(VertexIndex).Normal));
                }
            }

			BitMask <<= 1;

			if(!BitMask)
			{
				BitmapIndex++;
				BitMask = 1;
			}
		}
	}
	VertexStream.Revision++;

	for( INT y=0;y<=QuadsY;y++ )
		for( INT x=0;x<=QuadsX;x++ )
			Info->VertexColors(GetGlobalVertex(x,y)) = VertexStream.Vertices(GetLocalVertex(x,y)+VertexStreamOffset).Color;
}

static inline INT CheckVertexForProjector( const FVector& V, FPlane* FrustumPlanes )
{
	INT Result=0;
	if( FrustumPlanes[0].PlaneDot(V) <= 0 )
		Result |= 1;
	if( FrustumPlanes[1].PlaneDot(V) <= 0 )
		Result |= 2;
	if( FrustumPlanes[2].PlaneDot(V) <= 0 )
		Result |= 4;
	if( FrustumPlanes[3].PlaneDot(V) <= 0 )
		Result |= 8;
	if( FrustumPlanes[4].PlaneDot(V) <= 0 )
		Result |= 16;
	if( FrustumPlanes[5].PlaneDot(V) <= 0 )
		Result |= 32;
	return Result;
}

// Projectors.
void UTerrainSector::AttachProjector(AProjector* Projector,FProjectorRenderInfo* RenderInfo,INT MinQuadX,INT MinQuadY,INT MaxQuadX,INT MaxQuadY)
{
	guard(UTerrainSector::AttachProjector);

	Info->CheckComputeDataOnLoad();

	// Remove any old projectors.

	for(INT ProjectorIndex = 0;ProjectorIndex < Projectors.Num();ProjectorIndex++)
	{
		if(!Projectors(ProjectorIndex).Info->RenderInfo->Render(Projector->Level->TimeSeconds))
		{
			delete Projectors(ProjectorIndex).Info;
			Projectors.Remove(ProjectorIndex--);
		}
	}

	// Create the projector with the vertices for MinQuadX,MinQuadY -> MaxQuadX,MaxQuadY.

	FStaticProjectorInfo*	ProjectorInfo = new(TEXT("Terrain FStaticProjectorInfo")) FStaticProjectorInfo;
	FVector					WorldDirection = RenderInfo->Projector->Rotation.Vector();
	FLOAT					InvMaxTraceDistance = 1.0f / RenderInfo->Projector->MaxTraceDistance;

	for(INT Y = MinQuadY;Y <= MaxQuadY + 1;Y++)
	{
		for(INT X = MinQuadX;X <= MaxQuadX + 1;X++)
		{
			FStaticProjectorVertex*	DestVertex = new(ProjectorInfo->Vertices) FStaticProjectorVertex;
			
			DestVertex->WorldPosition = Info->Vertices(GetGlobalVertex(X,Y));

			FLOAT	DirectionalAttenuation = 1.0f;

			if(!(RenderInfo->ProjectorFlags & PRF_ProjectOnBackfaces))
				DirectionalAttenuation = Max(Info->GetVertexNormal(X+OffsetX,Y+OffsetY) | -WorldDirection,0.0f);

			FLOAT	DistanceAttenuation = 1.0f;

			if(RenderInfo->ProjectorFlags & PRF_Gradient)
				DistanceAttenuation = Clamp(1.0f - ((DestVertex->WorldPosition - RenderInfo->Projector->Location) | WorldDirection) * InvMaxTraceDistance,0.0f,1.0f);

			DestVertex->Attenuation = DirectionalAttenuation * DistanceAttenuation;
		}
	}

	// Work out which quads this projector intersects.

	for(INT Y = MinQuadY;Y <= MaxQuadY;Y++)
	{
		for(INT X = MinQuadX;X <= MaxQuadX;X++)
		{
			if( !Info->GetQuadVisibilityBitmap( X+OffsetX, Y+OffsetY ) )
				continue;

			// global vertex offsets
			INT gv1 = GetGlobalVertex(X,Y);
			INT gv2 = gv1+1;
			INT gv3 = GetGlobalVertex(X+1,Y+1);
			INT gv4 = gv3-1;

			const FVector& V1 = Info->Vertices(gv1);
			const FVector& V2 = Info->Vertices(gv2);
			const FVector& V3 = Info->Vertices(gv3);
			const FVector& V4 = Info->Vertices(gv4);

			INT O1 = CheckVertexForProjector( V1, Projector->FrustumPlanes );
			INT O2 = CheckVertexForProjector( V2, Projector->FrustumPlanes );
			INT O3 = CheckVertexForProjector( V3, Projector->FrustumPlanes );
			INT O4 = CheckVertexForProjector( V4, Projector->FrustumPlanes );
		
			UBOOL Turned = Info->GetEdgeTurnBitmap( X+OffsetX, Y+OffsetY );

			if( Turned )
			{
				// 142
				for( INT i=0;i<6;i++ )
					if( O1&(1<<i) && O4&(1<<i) && O2&(1<<i) )
						goto SkipTri1;
				ProjectorInfo->Indices.AddItem((X - MinQuadX + 0) + (Y - MinQuadY + 0) * (MaxQuadX - MinQuadX + 2));
				ProjectorInfo->Indices.AddItem((X - MinQuadX + 0) + (Y - MinQuadY + 1) * (MaxQuadX - MinQuadX + 2));
				ProjectorInfo->Indices.AddItem((X - MinQuadX + 1) + (Y - MinQuadY + 0) * (MaxQuadX - MinQuadX + 2));
			}
			else
			{
				// 143
				for( INT i=0;i<6;i++ )
					if( O1&(1<<i) && O4&(1<<i) && O3&(1<<i) )
						goto SkipTri1;
				ProjectorInfo->Indices.AddItem((X - MinQuadX + 0) + (Y - MinQuadY + 0) * (MaxQuadX - MinQuadX + 2));
				ProjectorInfo->Indices.AddItem((X - MinQuadX + 0) + (Y - MinQuadY + 1) * (MaxQuadX - MinQuadX + 2));
				ProjectorInfo->Indices.AddItem((X - MinQuadX + 1) + (Y - MinQuadY + 1) * (MaxQuadX - MinQuadX + 2));
			}
			
SkipTri1:;
			if( Turned )
			{
				// 432
				for( INT i=0;i<6;i++ )
					if( O4&(1<<i) && O3&(1<<i) && O2&(1<<i) )
						goto SkipTri2;
				ProjectorInfo->Indices.AddItem((X - MinQuadX + 0) + (Y - MinQuadY + 1) * (MaxQuadX - MinQuadX + 2));
				ProjectorInfo->Indices.AddItem((X - MinQuadX + 1) + (Y - MinQuadY + 1) * (MaxQuadX - MinQuadX + 2));
				ProjectorInfo->Indices.AddItem((X - MinQuadX + 1) + (Y - MinQuadY + 0) * (MaxQuadX - MinQuadX + 2));
			}
			else
			{
				// 132
				for( INT i=0;i<6;i++ )
					if( O1&(1<<i) && O3&(1<<i) && O2&(1<<i) )
						goto SkipTri2;
				ProjectorInfo->Indices.AddItem((X - MinQuadX + 0) + (Y - MinQuadY + 0) * (MaxQuadX - MinQuadX + 2));
				ProjectorInfo->Indices.AddItem((X - MinQuadX + 1) + (Y - MinQuadY + 1) * (MaxQuadX - MinQuadX + 2));
				ProjectorInfo->Indices.AddItem((X - MinQuadX + 1) + (Y - MinQuadY + 0) * (MaxQuadX - MinQuadX + 2));
			}
SkipTri2:;
		}
	}

	if(ProjectorInfo->Indices.Num())
	{
		ProjectorInfo->BaseMaterial = NULL;
		ProjectorInfo->TwoSided = 0;
		ProjectorInfo->RenderInfo = RenderInfo->AddReference();
		new(Projectors) FTerrainStaticProjectorInfo( ProjectorInfo, MinQuadX, MinQuadY, MaxQuadX, MaxQuadY );
	}
	else
		delete ProjectorInfo;

	unguard;
}

IMPLEMENT_CLASS(UTerrainSector);

/*------------------------------------------------------------------------------
    UTerrainPrimitive Implementation.
------------------------------------------------------------------------------*/

UBOOL UTerrainPrimitive::LineCheck(FCheckResult &Result,AActor* Owner,const FVector& End,const FVector& Start,const FVector& Extent,DWORD ExtraNodeFlags,DWORD TraceFlags)
{
	guard(UTerrainPrimitive::LineCheck);
	check(Info==Owner);
	return Info->LineCheck( Result, End, Start, Extent, TraceFlags, 0 );
	unguard;
}

UBOOL UTerrainPrimitive::PointCheck(FCheckResult &Result,AActor* Owner,const FVector& Point,const FVector& Extent,DWORD ExtraNodeFlags)
{
	guard(UTerrainPrimitive::LineCheck);
	check(Info==Owner);
	return Info->PointCheck( Result, Point, Extent, 0 );
	unguard;
}

FBox UTerrainPrimitive::GetRenderBoundingBox( const AActor* Owner, UBOOL Exact )
{
	guard(UTerrainPrimitive::GetRenderBoundingBox);
	return FBox(Owner->Location,Owner->Location);
	unguard;
}

void UTerrainPrimitive::Illuminate(AActor* Owner,UBOOL ChangedOnly)
{
    guard(UTerrainPrimitive::Illuminate);
    
#ifndef CONSOLE // editor only here
    Info->CheckComputeDataOnLoad();
    
    // If we're only rebuilding visible actors and this terrains owner is hidden, leave.
    FRebuildOptions* RO = GRebuildTools.GetCurrent();
    if( (RO->Options&REBUILD_OnlyVisible) && Owner->IsHiddenEd() )
        return;
    
    GWarn->BeginSlowTask(TEXT("Raytracing Terrain"),1);
    
    TArray<AActor*>	Lights;
    
    for(INT ActorIndex = 0;ActorIndex < Owner->XLevel->Actors.Num();ActorIndex++)
    {
        AActor*	LightActor = Owner->XLevel->Actors(ActorIndex);
        if( LightActor && LightActor->LightType==LT_Steady && LightActor->bStatic && LightActor->bNoDelete && (LightActor->Region.ZoneNumber==Info->Region.ZoneNumber || Cast<ASkyZoneInfo>(LightActor->Region.Zone)) && LightActor->bSpecialLit==Info->bSpecialLit )
        {
            if(!ChangedOnly || LightActor->bLightChanged)
                Lights.AddItem(LightActor);
        }
    }
    
    // Get the zone number each terrrain vertex is in.
    TArray<UBOOL> VertexZones( Info->Vertices.Num() );
    for( INT i=0;i<Info->Vertices.Num();i++ )
        VertexZones(i) = Info->Level->XLevel->Model->PointRegion(Info->Level,Info->Vertices(i)).ZoneNumber == Info->Region.ZoneNumber;
    
    for( INT i=0;i<Info->Sectors.Num();i++ )
    {
        GWarn->StatusUpdatef(i,Info->Sectors.Num(),TEXT("Raytracing Terrain"));
        UTerrainSector* S = Info->Sectors(i);
        FSphere			BoundingSphere = FSphere(&S->BoundingBox.GetExtrema(0), 2);
        
        if(!ChangedOnly)
            S->LightInfos.Empty();
        
        // Setup LightInfos
        TArray<INT>	SectorLights;
        for( INT LightIndex=0; LightIndex<Lights.Num(); LightIndex++ )
        {
            AActor*	LightActor = Lights(LightIndex);
            
            if(LightActor->LightEffect == LE_Sunlight || ((LightActor->Location - BoundingSphere).SizeSquared() < Square(LightActor->WorldLightRadius() + BoundingSphere.W)))
            {
                FTerrainSectorLightInfo*	LightInfo = NULL;
                
				// Try to find the LightInfos the light already hits, if we're updating only changed lights.
                if(ChangedOnly)
                {
                    for(INT ExistingLightIndex = 0;ExistingLightIndex < S->LightInfos.Num();ExistingLightIndex++)
                        if(S->LightInfos(ExistingLightIndex).LightActor == LightActor)
                            LightInfo = &S->LightInfos(ExistingLightIndex);
                }

				// If this light didn't 
				if(!LightInfo)
                    LightInfo = new(S->LightInfos) FTerrainSectorLightInfo(LightActor);
                
                LightInfo->VisibilityBitmap.Empty();
                LightInfo->VisibilityBitmap.AddZeroed(((S->QuadsX+1)*(S->QuadsY+1) + 7) / 8);
                SectorLights.AddItem((INT)(LightInfo - &S->LightInfos(0)));
            }
        }
        
        if(SectorLights.Num())
        {
            INT		BitmapIndex = 0;
            BYTE	BitMask = 1;
            for( INT y=0; y<=S->QuadsY; y++ )
            {
                for( INT x=0; x<=S->QuadsX; x++ )
                {
                    // Check if any neighboring vertices are in the correct zone.  If none are,
                    // we don't need to raytrace this vertex.
                    UBOOL VertexZoneValid = 0;
                    for( INT yy=Max(0,y+S->OffsetY-1);yy<=Min(Info->HeightmapY-1,y+S->OffsetY+1);yy++ )
                    {
                        for( INT xx=Max(0,x+S->OffsetX-1);xx<=Min(Info->HeightmapX-1,x+S->OffsetX+1);xx++ )
                        {
                            if( VertexZones( Info->GetGlobalVertex( xx, yy ) ) )
                            {
                                VertexZoneValid = 1;
                                goto CheckedZones;
                            }
                        }
                    }
CheckedZones:;
         
         // Check if any quads used by this are visible.  If none are, we can avoid 
         if( VertexZoneValid &&
             (
             x+S->OffsetX == 0 ||
             y+S->OffsetY == 0 ||
             x+S->OffsetX == Info->HeightmapX-1 ||
             y+S->OffsetY == Info->HeightmapY-1 ||
             Info->GetQuadVisibilityBitmap(x+S->OffsetX, y+S->OffsetY) ||
             Info->GetQuadVisibilityBitmap(x+S->OffsetX-1, y+S->OffsetY) ||
             Info->GetQuadVisibilityBitmap(x+S->OffsetX, y+S->OffsetY-1) ||
             Info->GetQuadVisibilityBitmap(x+S->OffsetX-1, y+S->OffsetY-1)
             )
             )
         {	
             INT Index = S->GetLocalVertex(x,y) + S->VertexStreamOffset;
             FVector	SamplePoint = Info->VertexStreams(S->VertexStreamNum).Vertices(Index).Position,
                 SampleNormal = Info->VertexStreams(S->VertexStreamNum).Vertices(Index).Normal;
             
             SamplePoint += SampleNormal;
             for(INT LightIndex = 0;LightIndex < SectorLights.Num();LightIndex++)
             {
                 AActor* LightActor = S->LightInfos(SectorLights(LightIndex)).LightActor;
                 FLOAT WorldLightRadiusSquared = LightActor->WorldLightRadius();
                 WorldLightRadiusSquared *= WorldLightRadiusSquared;
                 FVector LightPoint = LightActor->LightEffect==LE_Sunlight ?
                     SamplePoint-WORLD_MAX*LightActor->Rotation.Vector() :
                 LightActor->Location;
                 
                 FCheckResult Result;
                 if(
                     (
                     LightActor->LightEffect==LE_Sunlight || 
                     (
                     (LightActor->Location-SamplePoint).SizeSquared() < WorldLightRadiusSquared && 
                     (SampleNormal|(LightActor->Location-SamplePoint)) > 0.0 
                     )
                     ) 
                     && (!Info->bStaticLighting || Info->Level->XLevel->SingleLineCheck(Result, NULL, LightPoint, SamplePoint, TRACE_ShadowCast|TRACE_Level|TRACE_Actors ) )
                     )
                     S->LightInfos(SectorLights(LightIndex)).VisibilityBitmap(BitmapIndex) |= BitMask;
             }
         }
         BitMask <<= 1;
         if(!BitMask)
         {
             BitmapIndex++;
             BitMask = 1;
         }
                }
            }
            
            for( INT LightIndex=0;LightIndex<S->LightInfos.Num();LightIndex++ )
            {
                for( INT j=0;j<S->LightInfos(LightIndex).VisibilityBitmap.Num(); j++ )
                    if( S->LightInfos(LightIndex).VisibilityBitmap(j) )
                        goto LightVisible;
                    
                    // Light is not visible to this sector, remove.
                    S->LightInfos(LightIndex).VisibilityBitmap.Empty();
                    S->LightInfos.Remove(LightIndex);
                    LightIndex--;
LightVisible:
                    ;
            }
		}
		S->StaticLight(1);
	}
   
    Info->SmoothColors(); // sjs
    GWarn->EndSlowTask();
#endif //!CONSOLE
    unguard;
}

void UTerrainPrimitive::Serialize(FArchive& Ar)
{
	guard(UTerrainPrimitive::Serialize);
	Super::Serialize(Ar);
	Ar << Info;
	unguard;
}

IMPLEMENT_CLASS(UTerrainPrimitive);

/*------------------------------------------------------------------------------
    ATerrainInfo Implementation.
------------------------------------------------------------------------------*/

void ATerrainInfo::PrecomputeLayerWeights()
{
	guard(ATerrainInfo::PrecomputeLayerWeights);

	INT i;
	for( i=0;i<ARRAY_COUNT(Layers);i++ )
	{
		if( !Layers[i].Texture || !Layers[i].AlphaMap )
			break;

		if(
			!Layers[i].LayerWeightMap || 
			 Layers[i].LayerWeightMap->USize != Layers[i].AlphaMap->USize ||
			 Layers[i].LayerWeightMap->VSize != Layers[i].AlphaMap->VSize ||
			 Layers[i].LayerWeightMap->Format != TEXF_RGBA8
		  )
		{
			Layers[i].LayerWeightMap = ConstructObject<UTexture>( UTexture::StaticClass(), Level, NAME_None, RF_Public );
			Layers[i].LayerWeightMap->Format = TEXF_RGBA8;
			Layers[i].LayerWeightMap->Init( Layers[i].AlphaMap->USize, Layers[i].AlphaMap->VSize );
			Layers[i].LayerWeightMap->PostLoad();
			Layers[i].LayerWeightMap->LODSet = LODSET_None;
		}
	}

	INT NumLayers = i;

	if( !NumLayers )
		return;

	Layers[0].LayerWeightMap->Clear(FColor(255,255,255,255));
	for( i=1;i<NumLayers;i++ )
	{
		Layers[i].LayerWeightMap->ArithOp( Layers[i].AlphaMap, TAO_AssignAlpha );
		for( INT j=0;j<i;j++ )
			Layers[j].LayerWeightMap->ArithOp( Layers[i].AlphaMap, TAO_MultiplyOneMinusAlpha );
	}
				

	for( i=0;i<NumLayers;i++ )
		Layers[i].LayerWeightMap->CreateMips( 1, 1 );

	unguard;
}

INT ATerrainInfo::GetRenderCombinationNum( TArray<INT>& Layers, ETerrainRenderMethod RenderMethod )
{
	guard(ATerrainInfo::GetRenderCombination);

	for( INT c=0;c<RenderCombinations.Num();c++ )
	{
		if( RenderCombinations(c).RenderMethod == RenderMethod &&
			RenderCombinations(c).Layers.Num() == Layers.Num() )
		{
			for( INT l=0;l<Layers.Num();l++ )
			{
				if( Layers(l) != RenderCombinations(c).Layers(l) )
					goto TryNextCombination;
			}
			return c;
		}
TryNextCombination:;		
	}

	FTerrainRenderCombination* NewCombination = new(RenderCombinations) FTerrainRenderCombination;
	NewCombination->CombinedWeightMaps = NULL;
	NewCombination->Layers.Add( Layers.Num() );
	appMemcpy( &NewCombination->Layers(0), &Layers(0), Layers.Num() * sizeof(INT) );
	NewCombination->RenderMethod = RenderMethod;

	return RenderCombinations.Num()-1;
		unguard;
	}

void ATerrainInfo::CombineLayerWeights()
{
	guard(ATerrainInfo::CombineLayerWeights);

	for( INT c=0;c<RenderCombinations.Num();c++ )
	{
		if( RenderCombinations(c).RenderMethod == RM_CombinedWeightMap )
		{
			RenderCombinations(c).CombinedWeightMaps = ConstructObject<UTexture>( UTexture::StaticClass(), Level, NAME_None, RF_Public );
			RenderCombinations(c).CombinedWeightMaps->Format = TEXF_RGBA8;
			RenderCombinations(c).CombinedWeightMaps->Init( Layers[RenderCombinations(c).Layers(0)].AlphaMap->USize, Layers[RenderCombinations(c).Layers(0)].AlphaMap->VSize ); //!!
			RenderCombinations(c).CombinedWeightMaps->PostLoad();
			RenderCombinations(c).CombinedWeightMaps->Clear(FColor(0,0,0,0));
			RenderCombinations(c).CombinedWeightMaps->LODSet = LODSET_None;

			switch( RenderCombinations(c).Layers.Num() )
			{
			case 5:
			case 4:
				RenderCombinations(c).CombinedWeightMaps->ArithOp( Layers[RenderCombinations(c).Layers(3)].LayerWeightMap, TAO_AssignLtoA );
			case 3:
				RenderCombinations(c).CombinedWeightMaps->ArithOp( Layers[RenderCombinations(c).Layers(2)].LayerWeightMap, TAO_AssignLtoB );
			case 2:
				RenderCombinations(c).CombinedWeightMaps->ArithOp( Layers[RenderCombinations(c).Layers(1)].LayerWeightMap, TAO_AssignLtoG );
			case 1:
				RenderCombinations(c).CombinedWeightMaps->ArithOp( Layers[RenderCombinations(c).Layers(0)].LayerWeightMap, TAO_AssignLtoR );
				break;
			default:
				appErrorf(TEXT("CombineLayerWeights: invalid number of passes %d"), RenderCombinations(c).Layers.Num()); 
			}
			RenderCombinations(c).CombinedWeightMaps->CreateMips( 1, 1 );
		}
	}

	unguard;
}

ATerrainInfo::ATerrainInfo()
{
	guard(ATerrainInfo::ATerrainInfo);
	CalcCoords();
	unguard;
}
void ATerrainInfo::PostEditChange()
{
	guard(ATerrainInfo::PostEditChange);
	Super::PostEditChange();
	Level->XLevel->UpdateTerrainArrays();
	DecoLayerData.Empty();
	RenderCombinations.Empty();
	SetupSectors();
	CalcCoords();
	Update(0.f);
	unguard;
}
void ATerrainInfo::Destroy()
{
	guard(ATerrainInfo::Destroy);
	if( Level && Level->XLevel && GIsEditor )
		Level->XLevel->UpdateTerrainArrays();
	Super::Destroy();
	unguard;
}
void ATerrainInfo::Serialize(FArchive& Ar)
{
	guard(ATerrainInfo::Serialize);
	Super::Serialize(Ar);

	Ar	<< Sectors
		<< Vertices
		<< SectorsX << SectorsY
		<< FaceNormals;

	if( Ar.Ver() < 83 )
	{
		TArray<FPlane> TempLighting;
		Ar << TempLighting;
	}

	Ar	<< ToWorld
		<< ToHeightmap;

	if( Ar.Ver() < 76 )
		Ar << OldHeightmap;

	Ar	<< HeightmapX 
		<< HeightmapY;

	// Decoration Layers - (these are serialized by UnrealScript now)
	if( Ar.Ver() > 74 && Ar.Ver() < 82 )
	{
		for( INT x = 0 ; x < 16 ; x++ )
		{
			INT Dummy;
			FStringNoInit DummyString;
			Ar << Dummy	<< Dummy << Dummy << Dummy << Dummy << Dummy << Dummy << DummyString;
		}
	}

	// Set OldTerrainMap on load
	if( Ar.IsLoading() )
		OldTerrainMap = TerrainMap;

	//!!OLDVER - this is now serialized by script (for T3D export)
	if(Ar.Ver() == 86 || Ar.Ver() == 87)
		Ar << QuadVisibilityBitmap;

	// New struct members need to be zeroed out.
	if( Ar.Ver() < 115 )
	{
		for( INT i=0; i<DecoLayers.Num(); i++ )
		{
			DecoLayers(i).LitDirectional			= 0;
			DecoLayers(i).DisregardTerrainLighting	= 0;
		}
	}

	if( Ar.Ver() < 119 )
	{
		for( INT i=0; i<DecoLayers.Num(); i++ )
			DecoLayers(i).RandomYaw = 0;		
	}

	if( Ar.Ver() < 127 )
	{
		for( INT i=0; i<DecoLayers.Num(); i++ )
			DecoLayers(i).DetailMode = DM_Low;
	}

	if( Ar.Ver() >= 117 )
		Ar << VertexColors;

	// Stuff not saved to disk
	if( !Ar.IsSaving() && !Ar.IsLoading() )
		Ar	<< SelectedVertices
			<< DecoLayerData
			<< ShowGrid
			<< OldTerrainMap
			<< Projectors
			<< Primitive
			<< VertexStreams
			<< PaintedColor
			<< RenderCombinations
			<< CollapsedLayers;

	unguard;
}

void ATerrainInfo::CalcCoords()
{
	ToWorld = FCoords(	FVector( -Location.X/TerrainScale.X, -Location.Y/TerrainScale.Y, -256.f*Location.Z/TerrainScale.Z),
						FVector( TerrainScale.X,0,0), FVector(0,TerrainScale.Y,0), FVector(0,0,TerrainScale.Z/256.f) );
	if( TerrainMap )
		ToWorld /= FVector( HeightmapX/2, HeightmapY/2, 32767 );

	ToHeightmap = ToWorld.Inverse();
}

#if CONSOLE ||  __GNUC__
BYTE ATerrainInfo::GetLayerAlpha( INT x, INT y, INT Layer, UTexture* InAlphaMap )
#else
inline BYTE ATerrainInfo::GetLayerAlpha( INT x, INT y, INT Layer, UTexture* InAlphaMap )
#endif
{
	guard(ATerrainInfo::GetLayerAlpha);
	UTexture* Texture;

	if( InAlphaMap ) 
		Texture = InAlphaMap;
	else
		Texture = (Layer < 0) ? TerrainMap : Layers[Layer].AlphaMap;

	if(!Texture)
		return 0;

	// -2 means don't scale
	if( Layer != -2 )
	{
		x = x * Texture->USize / HeightmapX;
		y = y * Texture->VSize / HeightmapY;
	}

	if( x < 0 || x >= Texture->USize ||
		y < 0 || y >= Texture->VSize )
		return 0;

	FStaticTexture StaticTexture( Texture );
	BYTE* Data = (BYTE*) StaticTexture.GetRawTextureData(0);

	BYTE ret = 0;
	if ( Data )
	{
		switch( Texture->Format )
		{
			case TEXF_L8:
				ret = Data[ x + y*Texture->USize ];
				break;
			case TEXF_P8:
				ret = Texture->Palette->Colors(Data[ x + y*Texture->USize ]).R;
				break;
			case TEXF_RGBA8:
				ret = ((FColor*)&Data[4*(x + y*Texture->USize)])->A;
				break;
		}
	}
	return ret;
	unguard;
}

#if CONSOLE || __GNUC__
FColor ATerrainInfo::GetTextureColor( INT x, INT y, UTexture* Texture )
#else
inline FColor ATerrainInfo::GetTextureColor( INT x, INT y, UTexture* Texture )
#endif
{
	x = x * Texture->USize / HeightmapX;
	y = y * Texture->VSize / HeightmapY;

	FColor Color(0);

	FStaticTexture StaticTexture( Texture );
	BYTE* Data = (BYTE*) StaticTexture.GetRawTextureData(0);

	if ( Data )
	{
		switch( Texture->Format )
		{
			case TEXF_RGBA8:
				Color = *((FColor*)&Data[4*(x + y*Texture->USize)]);
				break;
		}
	}
	return Color;
}

#if CONSOLE || __GNUC__
void ATerrainInfo::SetTextureColor( INT x, INT y, UTexture* Texture, FColor& Color )
#else
inline void ATerrainInfo::SetTextureColor( INT x, INT y, UTexture* Texture, FColor& Color )
#endif
{

	x = x * Texture->USize / HeightmapX;
	y = y * Texture->VSize / HeightmapY;

	// Retain the existing alpha value
	FColor SaveAlpha = GetTextureColor( x, y, Texture );
	Color.A = SaveAlpha.A;

	FStaticTexture StaticTexture( Texture );
	BYTE* Data = (BYTE*) StaticTexture.GetRawTextureData(0);

	if ( Data )
	{
		switch( Texture->Format )
		{
			case TEXF_RGBA8:
				*((FColor*)&Data[ 4*(x + y*Texture->USize)]) = Color;
				break;
		}
	}
}

#if CONSOLE || __GNUC__
void ATerrainInfo::SetLayerAlpha( FLOAT x, FLOAT y, INT Layer, BYTE Alpha, UTexture* InAlphaMap )
#else
inline void ATerrainInfo::SetLayerAlpha( FLOAT x, FLOAT y, INT Layer, BYTE Alpha, UTexture* InAlphaMap )
#endif
{
	UTexture* Texture;

	if( InAlphaMap ) 
		Texture = InAlphaMap;
	else
		Texture = (Layer == -1) ? TerrainMap : Layers[Layer].AlphaMap;

	INT X = x * Texture->USize / HeightmapX;
	INT Y = y * Texture->VSize / HeightmapY;

	FStaticTexture StaticTexture( Texture );
	BYTE* Data = (BYTE*) StaticTexture.GetRawTextureData(0);

	if ( Data )
	{
		switch( Texture->Format )
		{
			case TEXF_L8:
				Data[ X + Y*Texture->USize ] = Alpha;
				break;
			case TEXF_P8:
				Data[ X + Y*Texture->USize ] = Alpha;
				break;
			case TEXF_RGBA8:
				((FColor*)&Data[ 4*(X + Y*Texture->USize)])->A = Alpha;
				break;
		}
	}
}

#define G16Heightmap( x, y ) (*(_WORD *)(&TerrainMap->Mips(0).DataArray((x+y*TerrainMap->USize)*2)))

_WORD ATerrainInfo::GetHeightmap( INT x, INT y )
{
	_WORD rc;

	switch( TerrainMap->Format )
	{
	case TEXF_G16:
		rc = *(_WORD *)(&TerrainMap->Mips(0).DataArray((x+y*TerrainMap->USize)*2));
		return INTEL_ORDER16(rc);

	case TEXF_P8:
		return (_WORD)256 * (_WORD)TerrainMap->Mips(0).DataArray(x+y*TerrainMap->USize); 
	}
	return 0;
}

void ATerrainInfo::SetHeightmap( INT x, INT y, _WORD w )
{
	switch( TerrainMap->Format )
	{
	case TEXF_G16:
		*(_WORD *)(&TerrainMap->Mips(0).DataArray((x+y*TerrainMap->USize)*2)) = w;
		break;
	}
}

void ATerrainInfo::PostLoad()
{
	guard(ATerrainInfo::PostLoad);
	Super::PostLoad();

	// OLDVER Check for moved TerrainInfo
	FVector CorrectLocation = FVector(HeightmapX/2, HeightmapY/2, 32767).TransformPointBy(ToWorld);
	if( (Location - CorrectLocation).SizeSquared() > 5 )
	{
		debugf(TEXT("Terrain origin is incorrect") );
		debugf(TEXT("Resetting terrain origin to (%f, %f, %f)"), CorrectLocation.X, CorrectLocation.Y, CorrectLocation.Z );
		Location = CorrectLocation;
	}

	// OLDVER: pre697 Move Heightmap array to a UTexture.
	if( OldHeightmap.Num() )
	{
		// Create a texture in the level
		FName TextureName( *FString::Printf(TEXT("%sHeightMap"), GetName() ) );
		TerrainMap = CastChecked<UTexture>(StaticConstructObject(UTexture::StaticClass(),Level->GetOuter(),TextureName,0));
		TerrainMap->Format = TEXF_G16;
		TerrainMap->Init( HeightmapX, HeightmapY );
		TerrainMap->PostLoad();
		appMemcpy( &TerrainMap->Mips(0).DataArray(0), &OldHeightmap(0), HeightmapX*HeightmapY*2 );
		OldHeightmap.Empty();
	}

	// OLDVER: make sure the bitmaps are of the appropriate size
	INT BitmapSize = HeightmapX*HeightmapY/32;
	if( QuadVisibilityBitmap.Num() != BitmapSize )
	{
		QuadVisibilityBitmap.Empty();
		QuadVisibilityBitmap.Add( BitmapSize );
		appMemset( &QuadVisibilityBitmap(0), 0xFF, BitmapSize * sizeof(DWORD) );
	}
	if( EdgeTurnBitmap.Num() != BitmapSize )
	{
		EdgeTurnBitmap.Empty();
		EdgeTurnBitmap.Add( BitmapSize );
		appMemset( &EdgeTurnBitmap(0), 0, BitmapSize * sizeof(DWORD) );
	}

	// remember that we've just loaded so we rebuild terrain data before we try to render.
	JustLoaded = 1;

	// Generate DomMaterialBitmap at loadtime
	// Dont do this in editor. Don't think we'd need it, and it would change as you paint.
	if(!GIsEditor)
	{
		if(QuadDomMaterialBitmap.Num() != 0)
		{
			debugf(TEXT("DomMaterialBitmap not empty."));
			QuadDomMaterialBitmap.Empty();
		}
		QuadDomMaterialBitmap.AddZeroed( HeightmapX * HeightmapY );

		int NumLayers=0;
		while(NumLayers < ARRAY_COUNT(Layers) && Layers[NumLayers].Texture && Layers[NumLayers].AlphaMap)
			NumLayers++;

		// For each quad - find dominant material.
		for(INT y=0; y<HeightmapY-1; y++)
		{
			for(INT x=0; x<HeightmapX-1; x++)
			{
				// To find dominant material, we need to build a 'temporary' weight-map.
				// (we might not have a stored one)
				BYTE W[4][32];
				W[0][0] = W[1][0] = W[2][0] = W[3][0] = 1;

				for( INT l=1; l<NumLayers; l++ )
				{
					// Work up layers. Each time, scaling weights of all layers below by 
					// this layers alpha.

					W[0][l] = GetLayerAlpha(x,   y,   l);
					W[1][l] = GetLayerAlpha(x+1, y,   l);
					W[2][l] = GetLayerAlpha(x,   y+1, l);
					W[3][l] = GetLayerAlpha(x+1, y+1, l);

					for(INT i=0; i<l; i++ )
					{
						W[0][i] = W[0][i] * (1 - W[0][l]);
						W[1][i] = W[1][i] * (1 - W[1][l]);
						W[2][i] = W[2][i] * (1 - W[2][l]);
						W[3][i] = W[3][i] * (1 - W[3][l]);
					}
				} 

				// Now we have weights for the corners of all layers at this location, 
				// find layer with max (dominant) overall weight.
				UMaterial* domMat = NULL;
				INT maxWeight = 0;			
				for( INT l=0; l<NumLayers; l++ )
				{
					INT quadWeight = W[0][l] + W[1][l] + W[2][l] + W[3][l];
					if(quadWeight > maxWeight)
					{
						domMat = Layers[l].Texture;
						maxWeight = quadWeight;
					}
				}

				QuadDomMaterialBitmap(x + (y * HeightmapX)) = domMat;
			}
		}
	}

	unguard;
}

FVector ATerrainInfo::GetVertexNormal( INT x, INT y )
{
	FVector N;
	INT v;

	v = GetGlobalVertex(x,y);
	N = FaceNormals(v).Normal1 + FaceNormals(v).Normal2;
	if( x > 0 )
		N += FaceNormals(v-1).Normal1 + FaceNormals(v-1).Normal2;
	if( y > 0 )
	{
		v = GetGlobalVertex(x,y-1);
		N += FaceNormals(v).Normal1 + FaceNormals(v).Normal2;
		if( x > 0 )
			N += FaceNormals(v-1).Normal1 + FaceNormals(v-1).Normal2;
	}

	return Inverted ? -N.SafeNormal() : N.SafeNormal();
}

/*------------------------------------------------------------------------------
	ATerrainInfo terrain updating.
------------------------------------------------------------------------------*/

void ATerrainInfo::CheckComputeDataOnLoad()
{
	guard(ATerrainInfo::CheckComputeDataOnLoad);
	if( JustLoaded )
	{
		// rebuild vertex buffers and triangles
		Update(0.f);
		JustLoaded = 0;
	}
	unguard;
}


void ATerrainInfo::SetupSectors()
{
	guard(ATerrainInfo::SetupSectors);

	// See if the terrainmap has changed size.
	if( TerrainMap )
	{
		UBOOL JustPasted = (HeightmapX == 0 && HeightmapY == 0);
		HeightmapX = TerrainMap->USize;
		HeightmapY = TerrainMap->VSize;
		SectorsX = HeightmapX / Clamp(TERRAINSECTORSIZE,2,64); // sjs - fixed zero div possible when tweaking terraininfos
		SectorsY = HeightmapY / Clamp(TERRAINSECTORSIZE,2,64); // sjs - fixed zero div possible when tweaking terraininfos

		if( JustPasted )
		{
			// Make sure T3D pasted bitmap arrays are expanded to the required size.
			INT BitmapSize = HeightmapX*HeightmapY/32;
			if( QuadVisibilityBitmap.Num() < BitmapSize )
			{
				INT OldNum = QuadVisibilityBitmap.Num();
				QuadVisibilityBitmap.Add( BitmapSize-OldNum );
				appMemset( &QuadVisibilityBitmap(OldNum), 0xFF, (BitmapSize-OldNum) * sizeof(DWORD) );
			}
			if( EdgeTurnBitmap.Num() < BitmapSize )
			{
				INT OldNum = EdgeTurnBitmap.Num();
				EdgeTurnBitmap.Add( BitmapSize-OldNum );
				appMemset( &EdgeTurnBitmap(OldNum), 0, (BitmapSize-OldNum) * sizeof(DWORD) );
			}
		}
	}
	else
	{
		HeightmapX = HeightmapY = 0;
		SectorsX = SectorsY = 0;
	}

	if( Sectors.Num() == SectorsX*SectorsY )
		return;

	// If the heightmap has changed size, reinitialize the bitmaps.
	INT BitmapSize = HeightmapX*HeightmapY/32;
	if( QuadVisibilityBitmap.Num() != BitmapSize )
	{
		QuadVisibilityBitmap.Empty();
		QuadVisibilityBitmap.Add( BitmapSize );
		appMemset( &QuadVisibilityBitmap(0), 0xFF, BitmapSize * sizeof(DWORD) );
	}

	if( EdgeTurnBitmap.Num() != BitmapSize )
	{
		EdgeTurnBitmap.Empty();
		EdgeTurnBitmap.Add( BitmapSize );
		appMemset( &EdgeTurnBitmap(0), 0, BitmapSize * sizeof(DWORD) );
	}

	Sectors.Empty();
	VertexStreams.Empty();
	RenderCombinations.Empty();

	for( INT y=0;y<SectorsY;y++ )
		for( INT x=0;x<SectorsX;x++ )
			Sectors.AddItem( new(this) UTerrainSector	(	this, 
															x*TERRAINSECTORSIZE, y*TERRAINSECTORSIZE,
															x<SectorsX-1?TERRAINSECTORSIZE:TERRAINSECTORSIZE-1,
															y<SectorsY-1?TERRAINSECTORSIZE:TERRAINSECTORSIZE-1
														) );
	unguard;
}

void ATerrainInfo::CalcLayerTexCoords()
{
	guard(ATerrainInfo::CalcLayerTexCoords);
	for( INT i=0;i<ARRAY_COUNT(Layers);i++ )
	{
		if( Layers[i].Texture && Layers[i].AlphaMap )
		{	
			//!!OLDVER Hack alpha in P8 textures
			if( Layers[i].AlphaMap->Format == TEXF_P8 )
				for( INT p=0;p<256;p++ )
					Layers[i].AlphaMap->Palette->Colors(p).A = Layers[i].AlphaMap->Palette->Colors(p).R;

		    // Generate texture coordinates
		    FCoords TexCoords = ( GMath.UnitCoords / FRotator( 0.f, Layers[i].TextureRotation, 0.f) );
		    TexCoords *= ToHeightmap.Transpose();
		    TexCoords.XAxis /= Layers[i].UScale;
		    TexCoords.YAxis /= Layers[i].VScale;
		    TexCoords.Origin += FVector( Layers[i].UPan*Layers[i].UScale, Layers[i].VPan*Layers[i].VScale, 0.f ).TransformVectorBy(ToWorld);
		    switch( Layers[i].TextureMapAxis )
		    {
		    case TEXMAPAXIS_XY:
			    break;
		    case TEXMAPAXIS_XZ:
			    Exchange( TexCoords.Origin.Y, TexCoords.Origin.Z );
			    Exchange( TexCoords.Origin.X, TexCoords.Origin.Z );
			    Exchange( TexCoords.XAxis.X, TexCoords.XAxis.Z );
			    Exchange( TexCoords.YAxis.X, TexCoords.YAxis.Y );
			    Exchange( TexCoords.ZAxis.Y, TexCoords.ZAxis.Z );
			    break;
		    case TEXMAPAXIS_YZ:
			    Exchange( TexCoords.Origin.X, TexCoords.Origin.Z );
			    Exchange( TexCoords.XAxis.X, TexCoords.XAxis.Z );
			    Exchange( TexCoords.ZAxis.X, TexCoords.ZAxis.Z );
			    break;
		    }
		    TexCoords = TexCoords * ( GMath.UnitCoords / Layers[i].LayerRotation );
		    Layers[i].TextureMatrix = TexCoords.Matrix();
	    }
	}
	unguard;
}

void ATerrainInfo::Update( FLOAT Time, INT StartX, INT StartY, INT EndX, INT EndY, UBOOL UpdateLighting )
{
	guard(ATerrainInfo::Update);

	if( EndX==0 )
		EndX = HeightmapX;
	if( EndY==0 )
		EndY = HeightmapY;

	if( !GIsEditor )
		PrecomputeLayerWeights();

	CalcLayerTexCoords();
	UpdateVertices(Time, StartX, StartY, EndX, EndY );
	UpdateTriangles( StartX, StartY, EndX, EndY, UpdateLighting );

	if( !GIsEditor )
		CombineLayerWeights();

	//!!powervr_aaron: terrain blending without using D3DTA_TEMP requires blend weight to be in Alpha (RGB is not used); D3DFMT_A8 would be better
	for( INT i=0;i<ARRAY_COUNT(Layers);i++ )
	{
		if( Layers[i].LayerWeightMap )
		{
			Layers[i].LayerWeightMap->ArithOp( Layers[i].LayerWeightMap, TAO_AssignLtoA );
			Layers[i].LayerWeightMap->CreateMips( 1, 1 );
		}
	}

#if 0
	debugf(TEXT("Vertex Streams: %d"), VertexStreams.Num());
	for( INT i=0;i<VertexStreams.Num();i++ )
		debugf(TEXT("  %d: %d vertices"), i, VertexStreams(i).Vertices.Num());
	debugf(TEXT("RenderCombinations: %d"), RenderCombinations.Num());
	for( INT i=0;i<RenderCombinations.Num();i++ )
	{
		FString Layers=TEXT("");
		for( INT l=0;l<RenderCombinations(i).Layers.Num();l++ )
			Layers += FString::Printf(TEXT(" %d"), RenderCombinations(i).Layers(l) );
		debugf(TEXT("  %d: layers:%s type:%s"), i, *Layers,
			RenderCombinations(i).RenderMethod == RM_AlphaMap ? "A" :
			RenderCombinations(i).RenderMethod == RM_WeightMap ? "W" :
			RenderCombinations(i).RenderMethod == RM_CombinedWeightMap ? "C" :
			"U" );
	}
#endif

    if ( VertexLightMap ) // sjs
        SmoothColors();

	unguard;	
}

void ATerrainInfo::UpdateVertices( FLOAT Time, INT StartX, INT StartY, INT EndX, INT EndY )
{
	guard(ATerrainInfo::UpdateVertices);

	if( !TerrainMap )
		return;

	if( Vertices.Num() < HeightmapX*HeightmapY )
		Vertices.Add( HeightmapX*HeightmapY - Vertices.Num() );
	if( FaceNormals.Num() < HeightmapX*HeightmapY )
		FaceNormals.Add( HeightmapX*HeightmapY - FaceNormals.Num() );
	if( VertexColors.Num() < HeightmapX*HeightmapY )
	{
		VertexColors.Empty();
		VertexColors.Add( HeightmapX*HeightmapY );
		for( INT i=0;i<VertexColors.Num();i++ )
			VertexColors(i) = FColor(255,255,255,255);
	}

	// Calculate face normals.
	TerrainMap->Mips(0).DataArray.Load();
	guard(Part2.5);
	for( INT y=StartY; y<EndY; y++ )
	{
		for( INT x=StartX; x<EndX; x++ )
		{
			Vertices(GetGlobalVertex(x, y)) = HeightmapToWorld( FVector(x, y, GetHeightmap(x, y)) );

			if( x>0 && y>0 )
			{
				if( GetEdgeTurnBitmap( x-1, y-1 ) )
				{
					// 124, 423
					FaceNormals(GetGlobalVertex(x-1, y-1)).Normal1 = FPlane( Vertices(GetGlobalVertex(x-1, y-1)), Vertices(GetGlobalVertex(x, y-1)), Vertices(GetGlobalVertex(x-1, y)) ).SafeNormal();
					FaceNormals(GetGlobalVertex(x-1, y-1)).Normal2 = FPlane( Vertices(GetGlobalVertex(x-1, y)), Vertices(GetGlobalVertex(x, y-1)), Vertices(GetGlobalVertex(x, y)) ).SafeNormal();
				}
				else
				{
					// 123, 134
					FaceNormals(GetGlobalVertex(x-1, y-1)).Normal1 = FPlane( Vertices(GetGlobalVertex(x-1, y-1)), Vertices(GetGlobalVertex(x, y-1)), Vertices(GetGlobalVertex(x, y)) ).SafeNormal();
					FaceNormals(GetGlobalVertex(x-1, y-1)).Normal2 = FPlane( Vertices(GetGlobalVertex(x-1, y-1)), Vertices(GetGlobalVertex(x, y)), Vertices(GetGlobalVertex(x-1, y)) ).SafeNormal();
				}
			}
		}
	}
	unguard;

	INT MaxLayer = 0;
	for( INT Layer=0;Layer<32&&Layers[Layer].Texture&&Layers[Layer].AlphaMap;Layer++ )
	{
		Layers[Layer].AlphaMap->Mips(0).DataArray.Load();
		MaxLayer++;
	}
	unguard;
}

void ATerrainInfo::UpdateTriangles( INT StartX, INT StartY, INT EndX, INT EndY, UBOOL UpdateLighting, UBOOL CheckLayerBlending )
{
	guard(ATerrainInfo::UpdateTriangles);

	//!! make this faster
	for( INT i=0;i<Sectors.Num();i++ )
	{
		if( 
			Sectors(i)->OffsetX > EndX ||
			Sectors(i)->OffsetY > EndY ||
			Sectors(i)->OffsetX+Sectors(i)->QuadsX < StartX ||
			Sectors(i)->OffsetY+Sectors(i)->QuadsY < StartY 
		  )
			continue;

		if( UpdateLighting )
			Sectors(i)->StaticLight(1);
		Sectors(i)->GenerateTriangles(CheckLayerBlending);
	}

	unguard;
}

/*------------------------------------------------------------------------------
    Terrain	Collision
------------------------------------------------------------------------------*/

//!!vogel: Sep-Feb XDK can't handle this with optimizations enabled
//!!erik: After reshuffle, .NET ( .. || (defined(_MSC_VER) && _MSC_VER >= 1300) ) at least appears happy.
#if defined(_XBOX) 
#pragma DISABLE_OPTIMIZATION
#endif

// GetTriNormal doesn't waste time calculating the FPlane.W, as done in FPlane's three-vector constructor.
#define GetTriNormal( A, B, C ) ((((B)-(A))^((C)-(A))).SafeNormal())

UBOOL ATerrainInfo::LineCheckWithQuad( INT X, INT Y, FCheckResult &Result, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags, UBOOL CheckInvisibleQuads )
{
	guard(ATerrainInfo::LineCheckWithQuad);
	
	if( bDeleteMe )
		return 0;

#if 1 // sjs - tracking down crashes
    INT BitIndex = X + Y * HeightmapX;
    if( !CheckInvisibleQuads )
    {
        check( QuadVisibilityBitmap.Num() > (BitIndex>>5) );
    }
    check( EdgeTurnBitmap.Num() > (BitIndex>>5) );
#endif

	if( !CheckInvisibleQuads && !GetQuadVisibilityBitmap( X, Y ) )
		return 0;

	UBOOL ret = 0;
	FLOAT Flip = Inverted ? -1.f : 1.f;
	UBOOL Turned = GetEdgeTurnBitmap( X, Y );

	INT v1 = GetGlobalVertex(X,Y);
	INT v2 = v1+1;
	INT v3 = GetGlobalVertex(X+1,Y+1);
	INT v4 = v3-1;
	
	FVector V1 = Vertices(v1); 
	FVector V2 = Vertices(v2); 
	FVector V3 = Vertices(v3); 
	FVector V4 = Vertices(v4);
	FLOAT ZExt = Flip * Extent.Z;
	V1.Z +=ZExt;
	V2.Z +=ZExt;
	V3.Z +=ZExt;
	V4.Z +=ZExt;

	FVector StartEnd = End - Start;

	// Normal: 123
	// Turned: 124
	FVector N = Turned ?  (Inverted ? GetTriNormal( V1, V4, V2 ) : GetTriNormal( V1, V2, V4 ))
						: (Inverted ? GetTriNormal( V1, V3, V2 ) : GetTriNormal( V1, V2, V3 ));
	if( (N|StartEnd) < -0.0001f )
	{
		FVector Int = FLinePlaneIntersection( End, Start, FPlane(V1, N) );
		if( Result.Actor == NULL ||	((Int-Start).SizeSquared() < (Result.Location-Start).SizeSquared()) )  
		{					
			FLOAT T = ((Int-Start|StartEnd)/(StartEnd|StartEnd));
			if( T >=0.f && T<=1.f )
			{	
				FVector& Vert3 = Turned ? V4 : V3;
				FVector NF = N*Flip;

				FPlane Plan1 = FPlane( V1, (V2-V1)^NF );
				FLOAT Dot1 = Plan1.PlaneDot(Int);				
				if( Dot1 <= Extent.X )
				{
					FPlane Plan2 =  FPlane( V2, (Vert3-V2)^NF );
					FLOAT Dot2 = Plan2.PlaneDot(Int);
					if( Dot2 <= Extent.X ) 
					{
						FPlane Plan3 = FPlane( Vert3, (V1-Vert3)^NF );
						FLOAT Dot3 = Plan3.PlaneDot(Int);
						if( Dot3 <= Extent.X ) 
						{
							FVector Dir = End - Start;
							Dir.Normalize();
							Result.Location = Int;
							Result.Normal	= N;
							FVector V       = End-Start;
							Result.Time     = ((Result.Location-Start)|V)/(V|V);
							Result.Time		= Clamp( Result.Time - 0.5f / V.Size(), 0.f, 1.f );
							Result.Location	= Start + V * Result.Time;

							if(TraceFlags & TRACE_Material)
								Result.Material = GetQuadDomMaterialBitmap( X, Y );
							else
								Result.Material = NULL;

							Result.Actor	= this;
							if ( Owner )
								Result.Normal = Result.Normal.TransformVectorBy(Owner->ToWorld());													
							ret = 1;				
						} 
					}
				}			
			}
		}
	}
	
	// Normal: 134
	// Turned: 423
	N = Turned ?  (Inverted ? GetTriNormal( V4, V3, V2 ) : GetTriNormal( V4, V2, V3 ))
				: (Inverted ? GetTriNormal( V1, V4, V3 ) : GetTriNormal( V1, V3, V4 ));
	if( (N|StartEnd) < -0.0001f )
	{	
		FVector Int = FLinePlaneIntersection( End, Start, FPlane(V3, N) );
		if( Result.Actor == NULL ||
			((Int-Start).SizeSquared() < (Result.Location-Start).SizeSquared()) )  
		{
			FLOAT T = ((Int-Start|StartEnd)/(StartEnd|StartEnd));
			if( T >=0.f && T<=1.f )
			{
				FVector& Vert1 = Turned ? V4 : V1;				
				FVector& Vert3 = Turned ? V2 : V3;
				FVector& Vert4 = Turned ? V3 : V4;
				FVector NF = N*Flip;
				FLOAT Dot1 = FPlane( Vert1, (Vert3-Vert1)^NF ).PlaneDot(Int);
				if( Dot1 <= Extent.X ) 
				{
					FLOAT Dot2 = FPlane( Vert3, (Vert4-Vert3)^NF ).PlaneDot(Int);
					if( Dot2 <= Extent.X ) 
					{
						FLOAT Dot3 = FPlane( Vert4, (Vert1-Vert4)^NF ).PlaneDot(Int);
						if( Dot3 <= Extent.X )
						{
							FVector Dir = End - Start;
							Dir.Normalize();
							Result.Location = Int;
							Result.Normal	= N;
							FVector V       = End-Start;
							Result.Time     = ((Result.Location-Start)|V)/(V|V);
							Result.Time		= Clamp( Result.Time - 0.5f / V.Size(), 0.f, 1.f );
							Result.Location	= Start + V * Result.Time;

							if(TraceFlags & TRACE_Material)
								Result.Material = GetQuadDomMaterialBitmap( X, Y );
							else
								Result.Material = NULL;

							Result.Actor	= this;
							if ( Owner )
								Result.Normal = Result.Normal.TransformVectorBy(Owner->ToWorld());
							ret = 1;
						}
					}
				} 
			}
		}
	}

	return ret;
	unguard;
}
#if defined(_XBOX) 
#pragma ENABLE_OPTIMIZATION
#endif


// quick sector rejection.
#define SECTORREJECTFAILED( X, Y ) (!SectorRejectMap || SectorRejectFailed( this, SectorRejectMap, InStart, InEnd, WorldDirection, WorldOneOverDirection, X, Y ))
FORCEINLINE UBOOL SectorRejectFailed( ATerrainInfo* Info, BYTE* Map, const FVector& Start, const FVector& End, const FVector& Direction, const FVector& InvDirection, INT X, INT Y )
{
	INT s = Y / Info->TerrainSectorSize *Info->SectorsX + X / Info->TerrainSectorSize;
	if( Map[s] == 0 )
	{
		if( FLineBoxIntersection( Info->Sectors(s)->BoundingBox, Start, End, Direction, InvDirection ) )
			Map[s] = 2;	// line hit the sector box, we have to check quads.
		else
			Map[s] = 1;	// line missed the sector box, we can reject the entire sector.
	}

	if( Map[s] == 1 )
		return 0;			// missed, we can reject.
	else
		return 1;			// hit, sector reject failed.
}

UBOOL ATerrainInfo::LineCheck( FCheckResult &Result, const FVector& InEnd, const FVector& InStart, const FVector& InExtent, DWORD TraceFlags, UBOOL CheckInvisibleQuads )
{
	guard(ATerrainInfo::LineCheck);
	if( !TerrainMap || Vertices.Num()==0 )
		return 1;
	
	clock(GStats.DWORDStats(GEngineStats.STATS_Terrain_CollisionCycles));

	FVector End = InEnd.TransformPointBy( ToHeightmap );
	FVector Start = InStart.TransformPointBy( ToHeightmap );
	FVector HeightmapExtent = InExtent.TransformVectorBy( ToHeightmap );
	FVector TraceDir = (End - Start).SafeNormal();
	FLOAT HeightmapExtentX = Abs(HeightmapExtent.X);

	FVector WorldDirection;
	FVector WorldOneOverDirection;

	FMemMark Mark(GMem);
	BYTE* SectorRejectMap;
	if( InExtent == FVector(0,0,0) )
	{
		SectorRejectMap = new( GMem, SectorsX*SectorsY ) BYTE; 
		appMemzero( SectorRejectMap, SectorsX*SectorsY );		
		WorldDirection = InEnd - InStart;
		WorldOneOverDirection = FVector( 1.f/WorldDirection.X, 1.f/WorldDirection.Y, 1.f/WorldDirection.Z );
	}
	else
		SectorRejectMap = NULL;

	Start -= TraceDir*HeightmapExtentX;
	End   += TraceDir*HeightmapExtentX;

	// According to a post to unprog by Jeff Wofford, the following instances of "HeightMapX-2" and "HeightMapY-2" should be
	// "HeightMapX-1" and "HeightMapY-1" instead.
	// This fix appears to be valid, but was not integrated into the UT2004 codebase to avoid changing collision behavior in
	// released levels.
	// Worth revisiting!
	// -AJS 4/14/2004

	UBOOL Hit = 0;

	if( Start.X < 0 && End.X < 0 )
		goto ReturnLineCheck;
	if( Start.Y < 0 && End.Y < 0 )
		goto ReturnLineCheck;
	if( Start.X > HeightmapX-2 && End.X > HeightmapX-2 )
		goto ReturnLineCheck;
	if( Start.Y > HeightmapY-2 && End.Y > HeightmapY-2 )
		goto ReturnLineCheck;

	{
	Result.Actor = NULL;

	INT X1 = Clamp<INT>( /*appFloor*/(Start.X), 0, HeightmapX-2 );
	INT X2 = Clamp<INT>( /*appFloor*/(End.X  ), 0, HeightmapX-2 );

	if( X1 == X2 )
	{
		INT Y1 = Clamp<INT>( /*appFloor*/(Start.Y), 0, HeightmapY-2 );
		INT Y2 = Clamp<INT>( /*appFloor*/(End.Y  ), 0, HeightmapY-2 );
		if( Y1 == Y2 )
		{
			Hit = SECTORREJECTFAILED(X1, Y1) && LineCheckWithQuad( X1, Y1, Result, InEnd, InStart, InExtent, TraceFlags, CheckInvisibleQuads );
			goto ReturnLineCheck;
		}

		INT YDir = Y1 > Y2 ? -1 : 1;
		// X = mY + c
		FLOAT m = (End.X - Start.X) / (End.Y - Start.Y);
		FLOAT c = End.X - m*End.Y;
		INT HitY=0;
		for( INT y=Y1; y*YDir<=Y2*YDir; y+=YDir )
		{
			FLOAT fx1 = m*y + c;
			FLOAT fx2 = m*(y+1) + c;
			if( fx1 > fx2 )
				Exchange( fx1, fx2 );
			fx1 -= HeightmapExtentX;
			fx2 += HeightmapExtentX;
			INT x1 = Clamp<INT>( /*appFloor*/(fx1), 0, HeightmapX-2 );
			INT x2 = Clamp<INT>( /*appFloor*/(fx2), 0, HeightmapX-2 );
			for( INT x=x1;x<=x2;x++ )
			{
				if( SECTORREJECTFAILED(x, y) && LineCheckWithQuad( x, y, Result, InEnd, InStart, InExtent, TraceFlags, CheckInvisibleQuads ) )
				{
					if( !Hit )
						HitY = y;
					Hit = 1;
				}
			}
			if( Hit && Abs(HitY - y) > HeightmapExtentX )
				break;
		}
	}
	else
	{
		INT XDir = X1 > X2 ? -1 : 1;
		// Y = mX + c
		FLOAT m = (End.Y - Start.Y) / (End.X - Start.X);
		FLOAT c = End.Y - m*End.X;
		INT HitX=0;
		for( INT x=X1; x*XDir<=X2*XDir; x+=XDir )
		{
			FLOAT fy1 = m*x + c;
			FLOAT fy2 = m*(x+1) + c;
			if( fy1 > fy2 )
				Exchange( fy1, fy2 );
			fy1 -= HeightmapExtentX;
			fy2 += HeightmapExtentX;
			INT y1 = Clamp<INT>( /*appFloor*/(fy1), 0, HeightmapY-2 );
			INT y2 = Clamp<INT>( /*appFloor*/(fy2), 0, HeightmapY-2 );
			for( INT y=y1;y<=y2;y++ )
			{
				if( SECTORREJECTFAILED(x, y) && LineCheckWithQuad( x, y, Result, InEnd, InStart, InExtent, TraceFlags, CheckInvisibleQuads ) )
				{
					if( !Hit )
						HitX = x;
					Hit = 1;
				}
			}
			if( Hit && Abs(HitX - x) > HeightmapExtentX )
				break;
		}
	}
	}

ReturnLineCheck:
	unclock(GStats.DWORDStats(GEngineStats.STATS_Terrain_CollisionCycles));
	Mark.Pop();
	return !Hit;
	unguard;
}

//
//	ATerrainInfo::PointCheck
//

UBOOL ATerrainInfo::PointCheck( FCheckResult &Result, const FVector& InPoint, const FVector& InExtent, UBOOL CheckInvisibleQuads )
{
	guard(ATerrainInfo::PointCheck);

	if(LineCheck(Result,InPoint - FVector(0,0,InExtent.Z),InPoint + FVector(0,0,InExtent.Z),FVector(InExtent.X,InExtent.Y,0),0,CheckInvisibleQuads)==0)
	{
		Result.Location.Z += InExtent.Z;
		return 0;
	}
	else
		return 1;

	unguard;
}

/*------------------------------------------------------------------------------
    Terrain	Editor stuff
------------------------------------------------------------------------------*/

// Finds the closest vertex on the terrain to "InLocation", and optionally puts the
// results in "InOutput", "InX" and "InY".  Returns FALSE if vertex cannot be found.
UBOOL ATerrainInfo::GetClosestVertex( FVector& InLocation, FVector* InOutput, INT* InX, INT* InY )
{
	FVector H = InLocation.TransformPointBy( ToHeightmap );
	INT X = appRound(H.X);
	INT Y = appRound(H.Y);

	if( InX )	*InX = X;
	if( InY )	*InY = Y;

	if( X<0 || Y<0 || X>=HeightmapX || Y>=HeightmapY )
		return 0;

	if( InOutput )
		*InOutput = Vertices( GetGlobalVertex( X, Y ) );

	return 1;
}

UBOOL ATerrainInfo::SelectVertexX( INT InX, INT InY )
{
#ifndef CONSOLE
	INT i;
	for( i=0;i<SelectedVertices.Num();i++ )
		if( SelectedVertices(i).X==InX && SelectedVertices(i).Y==InY )
		{
			SelectedVertices.Remove(i);
			return 0;
		}

	i = SelectedVertices.Add();
	SelectedVertices(i).X = InX;
	SelectedVertices(i).Y = InY;
	SelectedVertices(i).Weight = 1.0f * (GTerrainTools.GetStrength()/100.f);
	SelectedVertices(i).OldHeight = GetHeightmap(InX,InY);
	SelectedVertices(i).Delta = 0;
#endif
    return 1; // sjs moved below #endif
}

UBOOL ATerrainInfo::SelectVertex( FVector Location )
{
	guard(ATerrainInfo::SelectVertex);
#ifndef CONSOLE
	INT X, Y;
	if( !GetClosestVertex( Location, NULL, &X, &Y ) )
		return 0;

	// Capture information about the first vertex clicked.  Some brushes need this info.
	if( GTerrainTools.bFirstClick )
	{
		GTerrainTools.RefHeight = GetHeightmap(X,Y);
		GTerrainTools.bFirstClick = 0;
	}

	if( !SelectVertexX( X, Y ) ) return 0;

	// Select mirrored vertices

	switch( GTerrainTools.GetMirrorAxis() )
	{
		case MIRRORAXIS_X:
			if( !SelectVertexX( TerrainMap->USize - X, Y ) ) return 0;
			break;

		case MIRRORAXIS_Y:
			if( !SelectVertexX( X, TerrainMap->VSize - Y ) ) return 0;
			break;

		case MIRRORAXIS_XY:
			if( !SelectVertexX( TerrainMap->USize - X, Y ) ) return 0;
			if( !SelectVertexX( X, TerrainMap->VSize - Y ) ) return 0;
			if( !SelectVertexX( TerrainMap->USize - X, TerrainMap->VSize - Y ) ) return 0;
			break;
	}

#endif
	return 1;
	unguard;
}

void ATerrainInfo::ConvertHeightmapFormat()
{
	guard(ATerrainInfo::ConvertHeightmapFormat);

	if(TerrainMap->Format != TEXF_P8)	return;

	TArray<BYTE> OldData(TerrainMap->USize*TerrainMap->VSize);
	appMemcpy( &OldData(0), &TerrainMap->Mips(0).DataArray(0), TerrainMap->USize*TerrainMap->VSize);
	TerrainMap->Palette = NULL;
	TerrainMap->Format	= TEXF_G16;
	TerrainMap->Init( TerrainMap->USize, TerrainMap->VSize );
	TerrainMap->bRealtimeChanged = 1;

	for( INT i = 0 ; i < OldData.Num() ; i++ )
		*(_WORD *)(&TerrainMap->Mips(0).DataArray((i*2))) = 256*OldData(i);

	unguard;
}

// Soft select vertices on the terrain.  Verts within the InnerRadius get a full weight associated with them.
void ATerrainInfo::SoftSelect( FLOAT InnerRadius, FLOAT OuterRadius )
{
	guard(ATerrainInfo::SoftSelect);
#ifndef CONSOLE
	// Reset deltas
	for( INT i=0;i<SelectedVertices.Num();i++ )
	{
		SelectedVertices(i).Delta = 0.f;
		SelectedVertices(i).OldHeight = GetHeightmap(SelectedVertices(i).X,SelectedVertices(i).Y);
	}

	FVector R = (Rotation.Vector() * OuterRadius).TransformVectorBy( ToHeightmap );
	INT Delta = (INT)(R.Size2D() + 1);;

	INT Num = SelectedVertices.Num();

	for( INT i=0;i<Num;i++ )
	{
		FVector Vertex = Vertices( GetGlobalVertex(SelectedVertices(i).X,SelectedVertices(i).Y) );

		INT MaxDist=0;
		for( INT y=Max<INT>(0,SelectedVertices(i).Y-Delta);y<=Min<INT>(SelectedVertices(i).Y+Delta,HeightmapY-1); y++ )
			for( INT x=Max<INT>(0,SelectedVertices(i).X-Delta);x<=Min<INT>(SelectedVertices(i).X+Delta,HeightmapX-1); x++ )
			{
				FLOAT Dist = (Vertex-Vertices(GetGlobalVertex(x,y))).Size2D();
				if( Dist<OuterRadius && Dist>MaxDist )
					MaxDist = (INT) Dist;
			}

		for( INT y=Max<INT>(0,SelectedVertices(i).Y-Delta);y<=Min<INT>(SelectedVertices(i).Y+Delta,HeightmapY-1); y++ )
		{
			for( INT x=Max<INT>(0,SelectedVertices(i).X-Delta);x<=Min<INT>(SelectedVertices(i).X+Delta,HeightmapX-1); x++ )
			{
				INT GlobalVertex = GetGlobalVertex(x,y);
				FLOAT Dist = (Vertex-Vertices(GlobalVertex)).Size2D();

				FLOAT NewDist = Dist - InnerRadius;
				INT NewMaxDist = (INT)(MaxDist - InnerRadius);
				if(NewDist > NewMaxDist)
					continue;

				FLOAT Weight;
				if( Dist > InnerRadius )
					Weight = SelectedVertices(i).Weight * (0.5+(0.5f*appCos(PI*(NewDist/NewMaxDist))));
				else
					Weight = 1.0f;

				Weight *= (GTerrainTools.GetStrength()/100.f);
							
				if( Weight > 0 )
				{
					UBOOL bFound = 0;
					for( INT j=0;j<SelectedVertices.Num();j++ )
					{
						if( SelectedVertices(j).X==x && 
							SelectedVertices(j).Y==y )
						{
							bFound = 1;
							if( SelectedVertices(j).Weight < Weight )
								SelectedVertices(j).Weight = Weight;
						}
					}
					if( !bFound )
					{
						INT idx = SelectedVertices.Add();
						SelectedVertices(idx).X = x;
						SelectedVertices(idx).Y = y;
						SelectedVertices(idx).Weight = Weight;
						SelectedVertices(idx).OldHeight = GetHeightmap(x,y);
						SelectedVertices(idx).Delta = 0;
					}
				}
			}
		}
	}
#endif
	unguard;
}
void ATerrainInfo::SoftDeselect()
{
	guard(ATerrainInfo::SoftDeselect);
	for( INT i=0;i<SelectedVertices.Num();i++ )
	{
		if( SelectedVertices(i).Weight != 1.f )
		{
			SelectedVertices.Remove(i);
			--i;
		}
	}
	unguard;
}
// Selects all the vertices on the heightmap which fall inside the specified box.
void ATerrainInfo::SelectVerticesInBox( FBox& InRange )
{
	guard(ATerrainInfo::SelectVerticesInBox);

	FVector Start, End;

	Start = InRange.Min.TransformPointBy( ToHeightmap );
	End = InRange.Max.TransformPointBy( ToHeightmap );

	SelectedVertices.Empty();

	for( INT y = (INT)Start.Y ; y <= (INT)End.Y ; y++ )
	{
		for( INT x = (INT)Start.X ; x <= (INT)End.X ; x++ )
		{
			INT i = SelectedVertices.Add();
			SelectedVertices(i).X = x;
			SelectedVertices(i).Y = y;
			SelectedVertices(i).Weight = 1.0f;
			SelectedVertices(i).OldHeight = GetHeightmap(x,y);
			SelectedVertices(i).Delta = 0;
		}
	}

	unguard;
}

void ATerrainInfo::MoveVertices( FLOAT Delta )
{
	guard(ATerrainInfo::MoveVertices);
	if( !SelectedVertices.Num() )
		return;

	for( INT i=0;i<SelectedVertices.Num();i++ )
	{
		SelectedVertices(i).Delta += Delta;

		INT NewHeight = Clamp<INT>( SelectedVertices(i).OldHeight - 8.f * SelectedVertices(i).Delta * SelectedVertices(i).Weight, 0, 65535 );

		SetHeightmap( SelectedVertices(i).X, SelectedVertices(i).Y, NewHeight );
	}

	UpdateFromSelectedVertices();

	unguard;
}
void ATerrainInfo::ResetMove()
{
	guard(ATerrainInfo::ResetMove);
	if( !SelectedVertices.Num() )
		return;
	for( INT i=0;i<SelectedVertices.Num();i++ )
	{
		SelectedVertices(i).Delta = 0;
		SetHeightmap( SelectedVertices(i).X, SelectedVertices(i).Y, SelectedVertices(i).OldHeight );
	}

	UpdateFromSelectedVertices();
	unguard;
}
FBox ATerrainInfo::GetSelectedVerticesBounds()
{
	guard(ATerrainInfo::GetSelectedVerticesBounds);

	FBox BBox( FVector(HeightmapX, HeightmapY, 0), FVector(-HeightmapX, -HeightmapY, 0) );

	for( INT i = 0 ; i < SelectedVertices.Num() ; i++ )
		BBox += FVector( SelectedVertices(i).X, SelectedVertices(i).Y, 0 );

	return BBox;

	unguard;

}
// Updates the terrain to reflect changes to the selected vertices.
void ATerrainInfo::UpdateFromSelectedVertices()
{
	guard(ATerrainInfo::UpdateFromSelectedVertices);

	FBox BBox = GetSelectedVerticesBounds();

	BBox.Min.X--;
	BBox.Min.Y--;
	BBox.Max.X++;
	BBox.Max.Y++;

	// Make sure we're contained within the heightmap
	BBox.Min.X = Max<FLOAT>( BBox.Min.X, 0.f );
	BBox.Min.Y = Max<FLOAT>( BBox.Min.Y, 0.f );
	BBox.Max.X = Min<FLOAT>( BBox.Max.X, HeightmapX );
	BBox.Max.Y = Min<FLOAT>( BBox.Max.Y, HeightmapY );

	Update(0.f, BBox.Min.X, BBox.Min.Y, BBox.Max.X, BBox.Max.Y, 1 );

	unguard;
}

IMPLEMENT_CLASS(ATerrainInfo);

// Draws a shape conforming circle at a 3D location on the terrain.
static inline void DrawCircleOnTerrainX( FRenderInterface* RI, ATerrainInfo* InTerrainInfo, FVector InLocation, INT InRadius, FColor InColor )
{
	guard(DrawCircleOnTerrain);

	// Compute a decent number of sides for the radius markers, based on the radii.
	#define SIDES_MAX	32
	INT Sides = InRadius/24;
	Sides = Min( Sides, SIDES_MAX );	// Stay between 8 and SIDES_MAX
	Sides = Max( Sides,  8 );

	FVector Points[SIDES_MAX];
	FVector Origin = FVector( InRadius, 0, 0);
	FRotator StepRotation( 0, 65536/Sides, 0 );
	FCheckResult Check;
	INT Flip = InTerrainInfo->Inverted ? -1 : 1;

	for( INT x = 0 ; x < Sides ; x++ )
	{
		Points[x] = Origin.TransformVectorBy( GMath.UnitCoords * (StepRotation * x)) + InLocation;
		Points[x].Z = HALF_WORLD_MAX/2;
		
		if( InTerrainInfo->LineCheck( Check, Points[x] + (FVector(0,0,-Flip)*WORLD_MAX), Points[x] - (FVector(0,0,-Flip)*WORLD_MAX), FVector(0,0,0), 0, 1 )==0 )
			Points[x] = Check.Location;
		else
			Points[x] = x > 0 ? Points[x-1] : InLocation;
	}

	FLineBatcher	LineBatcher(RI,0);

	for( INT x = 0 ; x < Sides ; x++ )
		LineBatcher.DrawLine( Points[x], Points[(x+1)%Sides], InColor );

	LineBatcher.Flush();

	unguard;
}

#ifndef CONSOLE // sjs
static inline void DrawCircleOnTerrain( FRenderInterface* RI, ATerrainInfo* InTerrainInfo, FVector& InLocation, INT InRadius, FColor InColor )
{
	guard(DrawCircleOnTerrain);

	DrawCircleOnTerrainX( RI, InTerrainInfo, InLocation, InRadius, InColor );

	FVector Mirror;
	Mirror = InLocation.TransformPointBy( InTerrainInfo->ToHeightmap );
	Mirror.X = InTerrainInfo->TerrainMap->USize - Mirror.X;
	Mirror.Y = InTerrainInfo->TerrainMap->VSize - Mirror.Y;
	Mirror = Mirror.TransformPointBy( InTerrainInfo->ToWorld );

	switch( GTerrainTools.GetMirrorAxis() )
	{
		case MIRRORAXIS_X:
			DrawCircleOnTerrainX( RI, InTerrainInfo, FVector( Mirror.X, InLocation.Y, InLocation.Z), InRadius, InColor );
			break;

		case MIRRORAXIS_Y:
			DrawCircleOnTerrainX( RI, InTerrainInfo, FVector( InLocation.X, Mirror.Y, InLocation.Z), InRadius, InColor );
			break;

		case MIRRORAXIS_XY:
			DrawCircleOnTerrainX( RI, InTerrainInfo, FVector( Mirror.X, InLocation.Y, InLocation.Z), InRadius, InColor );
			DrawCircleOnTerrainX( RI, InTerrainInfo, FVector( InLocation.X, Mirror.Y, InLocation.Z), InRadius, InColor );
			DrawCircleOnTerrainX( RI, InTerrainInfo, FVector( Mirror.X, Mirror.Y, InLocation.Z), InRadius, InColor );
			break;
	}

	unguard;
}
#endif

#ifndef CONSOLE
static void DrawSegmentedLine( FRenderInterface* RI, ATerrainInfo* InTerrainInfo, FVector InStart, FVector InEnd, FColor InColor )
{
	guard(DrawSegmentedLine);

	FLineBatcher	LineBatcher(RI,0);
	FVector			Start,
					End,
					Step;

	Start = InStart.TransformPointBy( InTerrainInfo->ToHeightmap );
	End = InEnd.TransformPointBy( InTerrainInfo->ToHeightmap );

	FBox Rect(0);
	Rect += Start;
	Rect += End;

	// Assumes that the line will run along either the X or Y axis.  Diagonal lines aren't supported.
	FVector LineStart, LineEnd;
	if( Rect.Max.X - Rect.Min.X )
	{
		LineStart = InTerrainInfo->Vertices( InTerrainInfo->GetGlobalVertex(Rect.Min.X, Rect.Min.Y ) );
		for( INT x = (INT)Rect.Min.X+1 ; x <= (INT)Rect.Max.X ; x++ )
		{
			LineEnd = InTerrainInfo->Vertices( InTerrainInfo->GetGlobalVertex(x, Rect.Min.Y ) );
			LineBatcher.DrawLine(LineStart,LineEnd,InColor);

			LineStart = LineEnd;
		}
	}
	else
	{
		LineStart = InTerrainInfo->Vertices( InTerrainInfo->GetGlobalVertex(Rect.Min.X, Rect.Min.Y ) );
		for( INT y = (INT)Rect.Min.Y+1 ; y <= (INT)Rect.Max.Y ; y++ )
		{
			LineEnd = InTerrainInfo->Vertices( InTerrainInfo->GetGlobalVertex(Rect.Min.X, y ) );
			LineBatcher.DrawLine(LineStart,LineEnd,InColor);

			LineStart = LineEnd;
		}
	}
	LineBatcher.Flush();

	unguard;
}

// Draws a shape conforming rectangle on the terrain.
static void DrawRectOnTerrain( FRenderInterface* RI, ATerrainInfo* InTerrainInfo, FBox InRect, FColor InColor )
{
	guard(DrawRectOnTerrain);

	DrawSegmentedLine( RI, InTerrainInfo, FVector( InRect.Min.X, InRect.Min.Y, 0), FVector( InRect.Min.X, InRect.Max.Y, 0 ), InColor );
	DrawSegmentedLine( RI, InTerrainInfo, FVector( InRect.Min.X, InRect.Max.Y, 0), FVector( InRect.Max.X, InRect.Max.Y, 0 ), InColor );
	DrawSegmentedLine( RI, InTerrainInfo, FVector( InRect.Max.X, InRect.Max.Y, 0), FVector( InRect.Max.X, InRect.Min.Y, 0 ), InColor );
	DrawSegmentedLine( RI, InTerrainInfo, FVector( InRect.Max.X, InRect.Min.Y, 0), FVector( InRect.Min.X, InRect.Min.Y, 0 ), InColor );

	unguard;
}
#endif

// Draws a vertex marker
static inline void DrawVertex( FSceneNode* SceneNode, FRenderInterface* RI, FVector& InLocation, FColor InColor )
{
	guard(DrawVertex);
#ifndef CONSOLE
	// Draw the pin.

	FLineBatcher(RI).DrawLine(InLocation,InLocation - FVector(0,0,32),InColor);

	// Draw the pinhead.

	FCanvasUtil	CanvasUtil(&SceneNode->Viewport->RenderTarget,RI);
	UTexture*	Tex = GTerrainTools.WhiteCircle;
	FPlane		P = SceneNode->Project(InLocation);
	FVector		C = CanvasUtil.ScreenToCanvas.TransformFVector(P);

	CanvasUtil.DrawTile(
			C.X - (Tex->USize/2),
			C.Y - (Tex->VSize/2),
			C.X + (Tex->USize/2),
			C.Y + (Tex->VSize/2),
			0,0,
			Tex->UClamp,Tex->VClamp,
			C.Z,
			Tex,
			//!!MAT   PF_Masked,
			InColor
			);
#endif
	unguard;
}


/*------------------------------------------------------------------------------
    Terrain Rendering.
------------------------------------------------------------------------------*/

//
// UTerrainMaterial
//
UMaterial* UTerrainMaterial::CheckFallback()
{
	guard(UTerrainMaterial::CheckFallback);
	return this;
	unguard;
}
UBOOL UTerrainMaterial::HasFallback()
{
	guard(UTerrainMaterial::HasFallback);
	if( UseFallback )
		return 0;
	UShader* Shader = Layers.Num() ? Cast<UShader>(Layers(0).Texture) : NULL;
	return Shader ? Shader->HasFallback() : 0;
	unguard;
}

IMPLEMENT_CLASS(UTerrainMaterial);


FTerrainRenderCombination* FTerrainSectorRenderPass::GetRenderCombination(ATerrainInfo* TerrainInfo)
{
	return &TerrainInfo->RenderCombinations(RenderCombinationNum);
}

// 
// FTerrainRenderThing
//
// - There is one of these for each combination of simultaneous layers and vertex stream.
// - A number of these will be rendered in the same DrawPrimitive call.
//
struct FTerrainRenderThing
{
	INT ForceSortOrder;
	INT NumLayers;
	FTerrainSectorRenderPass* RenderPass;
	FTerrainVertexStream* VertexStream;
	FLOAT Distance;
    FDynamicLight*  DynLights[8]; // sjs

	void Set( INT InForceSortOrder, FTerrainSectorRenderPass* InRenderPass, FTerrainVertexStream* InVertexStream, INT InNumLayers, FLOAT InDistance, FDynamicLight* inDynLights[8] )
    {	
		ForceSortOrder	= InForceSortOrder;
		RenderPass		= InRenderPass;
		VertexStream	= InVertexStream;
		NumLayers		= InNumLayers;
		Distance		= InDistance;
        // sjs - copy dynamic light ptrs
        for( int i=0; i<8; i++ )
            DynLights[i] = inDynLights[i];
	}

    bool CheckMatchingLights( FTerrainRenderThing& other ) // sjs
    {
        // 
        for( int i=0; i<8; i++ )
        {
            if( DynLights[i] != other.DynLights[i] )
                return false;
        }
        return true;
    }
};

static inline INT Compare(FTerrainRenderThing& A,FTerrainRenderThing& B)
{
	// 1.  Sort by vertex stream
	if( A.VertexStream > B.VertexStream )
		return 1;
	else if( A.VertexStream < B.VertexStream )
		return -1;

	// 2.  Force order-dependent stuff
	if( A.ForceSortOrder != B.ForceSortOrder )
		return A.ForceSortOrder - B.ForceSortOrder;

	// 3.  Sort by number of simultaneous layers
	if( A.NumLayers != B.NumLayers )
		return -1 * (A.NumLayers - B.NumLayers);

	// 4. Sort by RenderCombination
	if( A.RenderPass->RenderCombinationNum > B.RenderPass->RenderCombinationNum )
		return 1;
		else
	if( A.RenderPass->RenderCombinationNum < B.RenderPass->RenderCombinationNum )
		return -1;

	// 5. Sort by distance (front to back)
	return A.Distance - B.Distance;
}

//
// Dynamic index buffer for terrain rendering
//
class FTerrainIndexBuffer : public FIndexBuffer
{
public:
	FTerrainRenderThing**			RenderThings;  // List of things we're going to render simultaneously.
	INT								RenderThingsNum;
	QWORD							CacheId;
	INT								MinIndex;
	INT								MaxIndex;
	INT								NumTriangles;
			
	FTerrainIndexBuffer( INT MaxThings )
    {
		// Allocate memory for the RenderThings array.
		RenderThings = new(GMem, MaxThings) (FTerrainRenderThing*);
	}

	void Init()
    {
		CacheId	= MakeCacheID(CID_RenderIndices);
		RenderThingsNum = 0;
		MinIndex = MAXINT;
		MaxIndex = 0;
		NumTriangles = 0;
	}

	//
	// AddThing - Add another thing to render with the same DrawPrimitive call.
	//
	void AddThing( FTerrainRenderThing* Thing )
					{
		RenderThings[RenderThingsNum++] = Thing;
		MinIndex = Min<INT>( MinIndex, Thing->RenderPass->MinIndex );
		MaxIndex = Max<INT>( MaxIndex, Thing->RenderPass->MaxIndex );
		NumTriangles += Thing->RenderPass->NumTriangles;
					}

	virtual QWORD GetCacheId()
	{
		return CacheId;
    }

	virtual INT GetRevision()
    {
		return 1;
    }

	virtual INT GetSize()
	{
		INT total=0;
		for( INT i=0;i<RenderThingsNum;i++ )
			total += RenderThings[i]->RenderPass->Indices.Num();
		return total * sizeof(_WORD);
	}

	virtual INT GetIndexSize()
    {
		return sizeof(_WORD);
    }

	virtual void GetContents(void* Data)
	{
		_WORD* Ptr = (_WORD*)Data;
		//
		// Copy the indices of the things we're going to render simultaneously.
		//
		for( INT i=0;i<RenderThingsNum;i++ )
		{
			INT count = RenderThings[i]->RenderPass->Indices.Num();
			appMemcpy( Ptr, &RenderThings[i]->RenderPass->Indices(0), count * sizeof(_WORD) );
			Ptr += count;
		}
	}
};

//
// FTerrainWireframeIndexBuffer - used to render individual terrain layer wireframes.
//
class FTerrainWireframeIndexBuffer : public FIndexBuffer
{
	FTerrainSectorRenderPass*	RenderPass;
	QWORD						CacheId;

public:
	FTerrainWireframeIndexBuffer( FTerrainSectorRenderPass* InRenderPass )
	{
		RenderPass = InRenderPass;
		CacheId	= MakeCacheID(CID_RenderIndices);
	}

	virtual QWORD GetCacheId()
	{
		return CacheId;
	}

	virtual INT GetRevision()
	{
		return 1;
	}

	virtual INT GetSize()
	{
		return RenderPass->NumIndices * sizeof(_WORD);
	}

	virtual INT GetIndexSize()
	{
		return sizeof(_WORD);
	}

	virtual void GetContents(void* Data)
	{
		appMemcpy( Data, &RenderPass->Indices(0), RenderPass->NumIndices * sizeof(_WORD) );
	}
};

//
//	FDynamicTerrainProjector
//
class FDynamicTerrainProjector : public FIndexBuffer
{
public:

	ATerrainInfo*				TerrainInfo;
	UTerrainSector*				Sector;
	FProjectorRenderInfo*		ProjectorInfo;
	FDynamicTerrainProjector*	Next;
	INT							MinQuadX,
								MinQuadY,
								MaxQuadX,
								MaxQuadY;

	// Only valid after GetContents.

	INT	ClippedNumTriangles,
		MinIndex,
		MaxIndex;

	// Constructor.

	FDynamicTerrainProjector(ATerrainInfo* InTerrainInfo, UTerrainSector* InSector,FProjectorRenderInfo* InInfo,INT InMinX,INT InMinY,INT InMaxX,INT InMaxY,FDynamicTerrainProjector* InNext)
	{
		TerrainInfo = InTerrainInfo;
		Sector = InSector;
		ProjectorInfo = InInfo;
		MinQuadX = InMinX;
		MinQuadY = InMinY;
		MaxQuadX = InMaxX;
		MaxQuadY = InMaxY;
		Next = InNext;
		ClippedNumTriangles = 0;
		MinIndex = MAXWORD;
		MaxIndex = 0;
	}

	// FRenderResource interface.
	virtual QWORD GetCacheId() { return 0; }
	virtual INT GetRevision() { return 1; }

	// FIndexBuffer interface.
	virtual INT GetSize() { return (MaxQuadX - MinQuadX + 1) * (MaxQuadY - MinQuadY + 1) * 2 * 3 * sizeof(_WORD); }
	virtual INT GetIndexSize() { return sizeof(_WORD); }
	virtual void GetContents(void* Data)
	{
        guard(FDynamicTerrainProjector::GetContents);
		_WORD*	DestIndex = (_WORD*)Data;

		for( INT y=MinQuadY;y<=MaxQuadY;y++ )
		{
			for( INT x=MinQuadX;x<=MaxQuadX;x++ )
			{
				// Don't render projectors on invisible terrain.
				if( !TerrainInfo->GetQuadVisibilityBitmap( Sector->OffsetX + x, Sector->OffsetY + y ) )
					continue;

				// index buffer vertex offsets
				INT v1 = Sector->GetLocalVertex(x,y) + Sector->VertexStreamOffset;
				INT v2 = v1+1;
				INT v3 = Sector->GetLocalVertex(x+1,y+1) + Sector->VertexStreamOffset;
				INT v4 = v3-1;

				// global vertex offsets
				INT gv1 = Sector->GetGlobalVertex(x,y);
				INT gv2 = gv1+1;
				INT gv3 = Sector->GetGlobalVertex(x+1,y+1);
				INT gv4 = gv3-1;
	
				INT O1, O2, O3, O4;
#if __HAS_SSE__
				if( GIsSSE )
				{
					O1 = O2 = O3 = O4 = 0;

					F32vec4		dummy0, dummy1,
								V_xxxx, V_yyyy, V_zzzz;
					
					dummy0 = _mm_loadl_pi( dummy0, (__m64*) &Sector->Info->Vertices(gv1).X);	// --- --- V0y V0x
					dummy0 = _mm_loadh_pi( dummy0, (__m64*) &Sector->Info->Vertices(gv2).X);	// V1y V1x V0y V0x
					dummy1 = _mm_loadl_pi( dummy1, (__m64*) &Sector->Info->Vertices(gv3).X);	// --- --- V2y V2x
					dummy1 = _mm_loadh_pi( dummy1, (__m64*) &Sector->Info->Vertices(gv4).X);	// V3y V3x V2y V2x
			
					V_xxxx = _mm_shuffle_ps( dummy0, dummy1, _MM_SHUFFLE(2, 0, 2, 0));			// V3x V2x V1x V0x
					V_yyyy = _mm_shuffle_ps( dummy0, dummy1, _MM_SHUFFLE(3, 1, 3, 1));			// V3y V2y V1y V0y

					dummy0 = _mm_loadl_pi( dummy0, (__m64*) &Sector->Info->Vertices(gv1).Y);	// --- --- V0z V0y
					dummy0 = _mm_loadh_pi( dummy0, (__m64*) &Sector->Info->Vertices(gv2).Y);	// V1z V1y V0z V0y
					dummy1 = _mm_loadl_pi( dummy1, (__m64*) &Sector->Info->Vertices(gv3).Y);	// --- --- V2z V2y
					dummy1 = _mm_loadh_pi( dummy1, (__m64*) &Sector->Info->Vertices(gv4).Y);	// V3z V3y V2z V2y
	
					V_zzzz = _mm_shuffle_ps( dummy0, dummy1, _MM_SHUFFLE(3, 1, 3, 1));			// V3z V2z V1z V0z
					
					register INT i;
					for( i=0; i<6; i++ )
					{
						F32vec4		F_xxxx, F_yyyy, F_zzzz, F_wwww;

						F_xxxx = _mm_load_ps1( &ProjectorInfo->FrustumPlanes[i].X );			// Fix Fix Fix Fix
						F_yyyy = _mm_load_ps1( &ProjectorInfo->FrustumPlanes[i].Y );			// Fiy Fiy Fiy Fiy
						F_zzzz = _mm_load_ps1( &ProjectorInfo->FrustumPlanes[i].Z );			// Fiz Fiz Fiz Fiz
						F_wwww = _mm_load_ps1( &ProjectorInfo->FrustumPlanes[i].W );			// Fiw Fiw Fiw Fiw

						// PlaneDots.
						F32vec4 Distance	=	_mm_sub_ps
												(
													_mm_add_ps
													( 
														_mm_add_ps
														( 
															_mm_mul_ps( F_xxxx, V_xxxx ), 
															_mm_mul_ps( F_yyyy, V_yyyy ) 
														)
														,
														_mm_mul_ps( F_zzzz, V_zzzz )
													)
													,
													F_wwww
												);

						INT Mask = move_mask( cmple( Distance, _mm_setzero_ps() ) );
						
						O1 |= ((Mask & 1) >> 0) << i;
						O2 |= ((Mask & 2) >> 1) << i;
						O3 |= ((Mask & 4) >> 2) << i;
						O4 |= ((Mask & 8) >> 3) << i;
					}
				}
				else
#endif
				{
					const FVector& V1 = Sector->Info->Vertices(gv1);
					const FVector& V2 = Sector->Info->Vertices(gv2);
					const FVector& V3 = Sector->Info->Vertices(gv3);
					const FVector& V4 = Sector->Info->Vertices(gv4);
							
					O1 = CheckVertexForProjector( V1, ProjectorInfo->FrustumPlanes );
					O2 = CheckVertexForProjector( V2, ProjectorInfo->FrustumPlanes );
					O3 = CheckVertexForProjector( V3, ProjectorInfo->FrustumPlanes );
					O4 = CheckVertexForProjector( V4, ProjectorInfo->FrustumPlanes );
				}

				UBOOL Turned = Sector->Info->GetEdgeTurnBitmap( x+Sector->OffsetX, y+Sector->OffsetY );

				if( Turned )
				{
					// 142
					if( O1 & O4 & O2 )
						goto SkipTri1;
	
					*DestIndex++ = v1;
					*DestIndex++ = v4;
					*DestIndex++ = v2;

					ClippedNumTriangles++;
					MinIndex = Min(MinIndex,v1);
					MaxIndex = Max(MaxIndex,v4);
				}
				else
				{
					// 143
					if( O1 & O4 & O3 )
						goto SkipTri1;
					
					*DestIndex++ = v1;
					*DestIndex++ = v4;
					*DestIndex++ = v3;

					ClippedNumTriangles++;
					MinIndex = Min(MinIndex,v1);
					MaxIndex = Max(MaxIndex,v3);
				}
				
	SkipTri1:;
				if( Turned )
				{
					// 432
					if( O4 & O3 & O2 )
						goto SkipTri2;

					*DestIndex++ = v4;
					*DestIndex++ = v3;
					*DestIndex++ = v2;

					ClippedNumTriangles++;
					MinIndex = Min(MinIndex,v2);
					MaxIndex = Max(MaxIndex,v3);
				}
				else
				{
					// 132
					if( O1 & O3 & O2 )
						goto SkipTri2;
					*DestIndex++ = v1;
					*DestIndex++ = v3;
					*DestIndex++ = v2;

					ClippedNumTriangles++;
					MinIndex = Min(MinIndex,v1);
					MaxIndex = Max(MaxIndex,v3);
				}
	SkipTri2:;
			}
		}
        unguard;
	}
};

//
// ATerrainInfo::Render - render terrain
//
void ATerrainInfo::Render(FLevelSceneNode* SceneNode,FRenderInterface* RI,FVisibilityInterface* VI, FDynamicLight** DynamicLights,INT NumDynamicLights,FProjectorRenderInfo** DynamicProjectors,INT NumDynamicProjectors)
{
	guard(ATerrainInfo::Render);
	
	if( !TerrainMap )
		return;

    TArray<AActor*> WorldDynLights;

				
	if( GIsEditor )
	{
		// Hide this terrain if the terraininfo actor is hidden.
		if( IsHiddenEd() )
			return;
	}
	
	CheckComputeDataOnLoad();
	
	RI->PushState();
	
	RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);
	
	PUSH_HIT(SceneNode->Viewport,HTerrain(this));
	
#ifdef __PSX2_EE__
	for( INT s=0;s<Sectors.Num();s++ )
				{
		UTerrainSector* Sector = Sectors(SectorIndices[s]);
		for( INT passindex=0;passindex<Sector->RenderPasses.Num();passindex++ )
		{
			extern void PSX2Render_RenderTerrainSector(void* PS2Data, INT Layer, INT QuadsX, INT QuadsY, INT OffsetX, INT OffsetY);
			PSX2Render_RenderTerrainSector(Sector->PS2Data, passindex, Sector->QuadsX + 1, Sector->QuadsY + 1, Sector->OffsetX, Sector->OffsetY);
			RI->SetIndexBuffer(NULL,0);
		}
				}
#else		
	
	if( SceneNode->Viewport->Actor->RendMap == REN_Wire )
	{		    
		for( INT s=0;s<Sectors.Num();s++ )
		{
			UTerrainSector* Sector = Sectors(s);
			if(VI && !VI->Visible(Sector->BoundingBox))
				continue;
			
			if( Sector->CompleteNumTriangles )
			{
			    FVertexStream*	Streams[1] = { &VertexStreams(Sector->VertexStreamNum) };
			    RI->SetVertexStreams(VS_FixedFunction,Streams,1);
			    DECLARE_STATIC_UOBJECT( UConstantColor, WireframeColor, {} );
			    DECLARE_STATIC_UOBJECT(
				    UShader, WireframeShader, 
				    {
					    WireframeShader->Diffuse = WireframeColor;
			    		WireframeShader->Wireframe = 1;
				    }
			    );
			    WireframeColor->Color = GetLevel()->Engine->C_TerrainWire;	
			    RI->EnableLighting(0,0);
			    RI->SetMaterial(WireframeShader);								
			    RI->SetIndexBuffer(&Sector->CompleteIndexBuffer,0);
			    RI->DrawPrimitive(PT_TriangleList,0,Sector->CompleteNumTriangles,Sector->CompleteMinIndex,Sector->CompleteMaxIndex);
		    }
	    }
	}
	else
	if( CollapsedLayers && SceneNode->Viewport->RenDev->LowQualityTerrain )
	{
		for( INT s=0;s<Sectors.Num();s++ )
		{
			UTerrainSector* Sector = Sectors(s);
			if(VI && !VI->Visible(Sector->BoundingBox))
				continue;
			
			if( Sector->CompleteNumTriangles )
			{
			    FVertexStream*	Streams[1] = { &VertexStreams(Sector->VertexStreamNum) };
			    RI->SetVertexStreams(VS_FixedFunction,Streams,1);
			    RI->EnableLighting(0,0);
			    RI->SetMaterial(CollapsedLayers);								
			    RI->SetIndexBuffer(&Sector->CompleteIndexBuffer,0);
			    RI->DrawPrimitive(PT_TriangleList,0,Sector->CompleteNumTriangles,Sector->CompleteMinIndex,Sector->CompleteMaxIndex);
		    }
	    }
	}
	else
	{
		// Get list of everything we need to render in its own pass
		FMemMark Mark(GMem);
		
		// Get a list of visible sectors.
		INT* VisibleSectors = new(GMem, Sectors.Num()) INT;
		INT VisibleSectorsCount = 0;
		INT RenderThingsCount = 0;
		for( INT s=0;s<Sectors.Num();s++ )
		{
			UTerrainSector* Sector = Sectors(s);
			if(VI && !VI->Visible(Sector->BoundingBox))
				continue;
			VisibleSectors[VisibleSectorsCount++] = s;
			GStats.DWORDStats( GEngineStats.STATS_Terrain_Sectors )++;
			if(GIsEditor)
				Sector->StaticLight(0);
			RenderThingsCount += Sector->RenderPasses.Num();
		}

		// Build a list of dynamic projectors hitting each sector.
		FDynamicTerrainProjector**	SectorProjectors = NewZeroed<FDynamicTerrainProjector*>(GMem,Sectors.Num());

		for(INT ProjectorIndex = 0;ProjectorIndex < NumDynamicProjectors;ProjectorIndex++)
		{
			FProjectorRenderInfo*	ProjectorInfo = DynamicProjectors[ProjectorIndex];

			check(ProjectorInfo->Projector);

			FBox	LocalBoundingBox = ProjectorInfo->Projector->Box.TransformBy(ToHeightmap);
			INT		MinX = appFloor(LocalBoundingBox.Min.X),
					MaxX = appFloor(LocalBoundingBox.Max.X),
					MinY = appFloor(LocalBoundingBox.Min.Y),
					MaxY = appFloor(LocalBoundingBox.Max.Y),
					MinSectorX = MinX / TerrainSectorSize,
					MaxSectorX = MaxX / TerrainSectorSize,
					MinSectorY = MinY / TerrainSectorSize,
					MaxSectorY = MaxY / TerrainSectorSize;

			for(INT Y = MinSectorY;Y <= MaxSectorY;Y++)
			{
				if(Y >= 0 && Y < SectorsY)
				{
					for(INT X = MinSectorX;X <= MaxSectorX;X++)
					{
						if(X >= 0 && X < SectorsX)
						{
							INT				SectorIndex = X + Y * SectorsX;
							UTerrainSector*	Sector = Sectors(SectorIndex);

							if(Sector->BoundingBox.Intersect(ProjectorInfo->Projector->Box))
								SectorProjectors[SectorIndex] = new(GMem) FDynamicTerrainProjector(
																			this,
																			Sector,
																			ProjectorInfo,
																			Clamp(MinX - X * TerrainSectorSize,0,Sector->QuadsX - 1),
																			Clamp(MinY - Y * TerrainSectorSize,0,Sector->QuadsY - 1),
																			Clamp(MaxX - X * TerrainSectorSize,0,Sector->QuadsX - 1),
																			Clamp(MaxY - Y * TerrainSectorSize,0,Sector->QuadsY - 1),
																			SectorProjectors[SectorIndex]
																			);
						}
					}
				}
			}
			
		}

		// Get a list of unique sector/layer combinations.
		FTerrainRenderThing* RenderThings = new(GMem, RenderThingsCount) FTerrainRenderThing;
		RenderThingsCount = 0;
		for( INT s=0;s<VisibleSectorsCount;s++ )
		{
			UTerrainSector* Sector = Sectors(VisibleSectors[s]);
			FLOAT Dist = FDist( SceneNode->ViewOrigin, Sector->Location );
            // sjs --- light list for sector
            FDynamicLight* ConsiderLights[8];
            appMemzero(ConsiderLights,sizeof(ConsiderLights));
            int numConsider = 0;
            for(INT LightIndex = 0;LightIndex < NumDynamicLights;LightIndex++)
            {
		        if(DynamicLights[LightIndex]->Dynamic || DynamicLights[LightIndex]->Changed)
                {
                    FBox BoundingBox = Sector->BoundingBox.ExpandBy(DynamicLights[LightIndex]->Radius);
                    if( FPointBoxIntersection(DynamicLights[LightIndex]->Position, BoundingBox) )
                    {
                        ConsiderLights[numConsider++] = DynamicLights[LightIndex];
                        if( numConsider > ARRAY_COUNT(ConsiderLights)-1 )
                            break;
                    }
                }
            }
            // --- sjs
			for( INT p=0;p<Sector->RenderPasses.Num();p++ )
				RenderThings[RenderThingsCount++].Set(	(p==0 || Sector->RenderPasses(p).GetRenderCombination(this)->RenderMethod==RM_AlphaMap) ? p : 1, 
				&Sector->RenderPasses(p), 
				&VertexStreams(Sector->VertexStreamNum), 
				Sector->RenderPasses(p).GetRenderCombination(this)->Layers.Num(), 
				Dist,
                ConsiderLights ); // sjs
		}
		
		
		// sort it to minimize state changes
		Sort( RenderThings, RenderThingsCount );
		
		FTerrainIndexBuffer IndexBuffer( RenderThingsCount );
		
		// Render
		for( INT t=0;t<RenderThingsCount;t++ )
		{
			// clear out the dynamic index buffer
			IndexBuffer.Init();
			IndexBuffer.AddThing( &RenderThings[t] );
			
			//!!powervr_aaron: DrawIndexedPrimitive gets smaller ranges, so D3D SW TnL transforms fewer vertices, if the following is skipped
			if( GetLevel()->Engine->GRenDev->GetRenderCaps()->HardwareTL )
			{
				// Check the following RenderThings and render them simultaneously
				// if they have the same textures and vertex stream.
				for( INT n=t+1;n<RenderThingsCount;n++ )
				{
					if( RenderThings[t].ForceSortOrder != RenderThings[n].ForceSortOrder ||
						RenderThings[t].VertexStream != RenderThings[n].VertexStream ||
						RenderThings[t].RenderPass->RenderCombinationNum != RenderThings[n].RenderPass->RenderCombinationNum ||
                        !RenderThings[t].CheckMatchingLights(RenderThings[n]) ) // sjs
						break;
					IndexBuffer.AddThing( &RenderThings[n] );
					t = n;
				}
			}
			
			FTerrainRenderCombination* RenderCombination = RenderThings[t].RenderPass->GetRenderCombination(this);
			
			FVertexStream*	Streams[1] = { RenderThings[t].VertexStream };
			RI->SetVertexStreams(VS_FixedFunction, Streams, 1);
			INT BaseIndex = RI->SetDynamicIndexBuffer(&IndexBuffer,0);
			
			DECLARE_STATIC_UOBJECT( UTerrainMaterial, TerrainMaterial, {} );
			TerrainMaterial->UseFallback = 0;
			
			// Copy the data in for the TerrainMaterial.
			TerrainMaterial->FirstPass = RenderThings[t].ForceSortOrder == 0 ? 1 : 0;
			TerrainMaterial->RenderMethod = RenderCombination->RenderMethod;
			
			// Make sure the material has the right number layers
			if( TerrainMaterial->Layers.Num() < RenderCombination->Layers.Num() )
				TerrainMaterial->Layers.Add( RenderCombination->Layers.Num() - TerrainMaterial->Layers.Num() );
			else
				if( TerrainMaterial->Layers.Num() > RenderCombination->Layers.Num() )
					TerrainMaterial->Layers.Remove( RenderCombination->Layers.Num(), TerrainMaterial->Layers.Num() - RenderCombination->Layers.Num() );
				
				// Copy the layers' textures to the UTerrainMaterial.
				for( INT layer=0;layer<RenderCombination->Layers.Num();layer++ )
				{
					TerrainMaterial->Layers(layer).Texture = Layers[RenderCombination->Layers(layer)].Texture;
					switch( RenderCombination->RenderMethod )
					{
					case RM_AlphaMap:
						TerrainMaterial->Layers(layer).AlphaWeight = Layers[RenderCombination->Layers(layer)].AlphaMap;
						break;
					case RM_WeightMap:
						TerrainMaterial->Layers(layer).AlphaWeight = Layers[RenderCombination->Layers(layer)].LayerWeightMap;
						break;
					case RM_CombinedWeightMap:
						// The combined weightmap is set only for the first layer.
						TerrainMaterial->Layers(layer).AlphaWeight = layer==0 ? RenderCombination->CombinedWeightMaps : NULL;
						break;
					}
					TerrainMaterial->Layers(layer).TextureMatrix = Layers[RenderCombination->Layers(layer)].TextureMatrix;
				}
				
				if( SceneNode->Viewport->Actor->RendMap==REN_LightingOnly )
				{
					RI->EnableLighting(!GIsPixomatic, 1, 1, NULL, 1, FSphere(FVector(0,0,0),-1.0f));
					RI->SetMaterial(TerrainMaterial);
				}
				else if(	SceneNode->Viewport->Actor->RendMap==REN_PlainTex )
				{
					RI->EnableLighting(0, 0);
					RI->SetMaterial(TerrainMaterial);
				}
				else
				{
					RI->EnableLighting(!GIsPixomatic, 1, 1, NULL, 0, FSphere(FVector(0,0,0),-1.0f)); // sjs
					RI->SetMaterial(TerrainMaterial);
				}

				if( !GIsPixomatic )
	           		for( int i=0; i<8; i++ )
               			RI->SetLight( i, RenderThings[t].DynLights[i] );
				
				RI->DrawPrimitive(PT_TriangleList,BaseIndex,IndexBuffer.NumTriangles,IndexBuffer.MinIndex,IndexBuffer.MaxIndex);			
				GStats.DWORDStats( GEngineStats.STATS_Terrain_Triangles ) += IndexBuffer.NumTriangles;
				GStats.DWORDStats( GEngineStats.STATS_Terrain_DrawPrimitives ) ++;
		}
		
		if(!SceneNode->Viewport->IsWire() && (SceneNode->Viewport->Actor->ShowFlags & SHOW_Projectors))
		{
			// Render projectors for all visible sectors
            guard(ATerrainInfo::Render::RenderProjectors); // sjs
			for( INT s=0;s<VisibleSectorsCount;s++ )
			{
				UTerrainSector* Sector = Sectors(VisibleSectors[s]);
				
				if( Sector->Projectors.Num() )
				{
					INT	ProjectorStartCycles = appCycles();
					RI->PushState();
					
					// Render projectors for this sector.
					for( INT i=0;i<Sector->Projectors.Num();i++ )
					{
						FStaticProjectorInfo*	ProjectorInfo = Sector->Projectors(i).Info;
						
						if( !ProjectorInfo->RenderInfo->Render( Level->TimeSeconds ) )
						{
							delete ProjectorInfo;
							Sector->Projectors.Remove(i--);
							continue;
						}

						// Find an existing batch for the projector.

						FProjectorRenderBatch*	ProjectorBatch = NULL;

						for(TList<FProjectorRenderBatch*>* ProjectorBatchList = GProjectorBatchList;ProjectorBatchList;ProjectorBatchList = ProjectorBatchList->Next)
						{
							if(ProjectorBatchList->Element->ProjectedMaterial != ProjectorInfo->RenderInfo->Material)
								continue;

							if(ProjectorBatchList->Element->FramebufferBlending != ProjectorInfo->RenderInfo->FrameBufferBlendingOp)
								continue;

							if(ProjectorBatchList->Element->BaseMaterialBlending != ProjectorInfo->RenderInfo->MaterialBlendingOp)
								continue;

							if(ProjectorBatchList->Element->BaseMaterial != ProjectorInfo->BaseMaterial)
								continue;

							if(ProjectorBatchList->Element->TwoSided != ProjectorInfo->TwoSided)
								continue;

							if(ProjectorBatchList->Element->VertexBufferSize >= 100000)
								continue;

							ProjectorBatch = ProjectorBatchList->Element;
						}

						// Create a new batch if necessary.

						if(!ProjectorBatch)
						{
							ProjectorBatch = new(GSceneMem) FProjectorRenderBatch(
																ProjectorInfo->BaseMaterial,
																ProjectorInfo->RenderInfo->Material,
																(EProjectorBlending)ProjectorInfo->RenderInfo->MaterialBlendingOp,
																(EProjectorBlending)ProjectorInfo->RenderInfo->FrameBufferBlendingOp,
																ProjectorInfo->TwoSided
																);
							GProjectorBatchList = new(GSceneMem) TList<FProjectorRenderBatch*>(ProjectorBatch,GProjectorBatchList);
						}

						// Add the projector to the batch.

						ProjectorBatch->AddPrimitive(new(GSceneMem) FStaticProjectorPrimitive(ProjectorInfo));
					}
					RI->PopState();
					GStats.DWORDStats( GEngineStats.STATS_Projector_RenderCycles ) += appCycles() - ProjectorStartCycles;
				}

				if(SectorProjectors[VisibleSectors[s]])
				{
					INT	ProjectorStartCycles = appCycles();
					RI->PushState();
					
					// Render projectors for this sector.
					for(FDynamicTerrainProjector* DynamicProjector = SectorProjectors[VisibleSectors[s]];DynamicProjector;DynamicProjector = DynamicProjector->Next)
					{					    
						// Setup blending.
						// Ask for static lighting so the projector is modulated by the terrain's own
						// baked vertex colour, which is what D3D does implicitly (its stage 0 reads
						// D3DTA_CURRENT, the interpolated vertex diffuse). GL feeds the colour array
						// only when UseStaticLighting is set; with 0 here the projector would render at
						// full brightness over unlit terrain. Scoped to this pass: the colour-array rule
						// stays as it is everywhere else.
						RI->EnableLighting(0,1);
						RI->SetMaterial(DynamicProjector->ProjectorInfo->GetMaterial(SceneNode,NULL));

						// Set the vertex stream / index buffer
						FVertexStream*	Streams[1] = { &VertexStreams(Sector->VertexStreamNum) };
						RI->SetVertexStreams(VS_FixedFunction, Streams, 1);

						// Render the projector.
						INT BaseIndex = RI->SetDynamicIndexBuffer( DynamicProjector, 0 );

                        // sjs, gam - ensure we don't call DrawPrimitive with 0 prims from clipping ---
					    if( DynamicProjector->ClippedNumTriangles == 0 )
					        continue;
					    // --- gam

						RI->DrawPrimitive(
							PT_TriangleList,
							BaseIndex,
							DynamicProjector->ClippedNumTriangles,
							DynamicProjector->MinIndex,
							DynamicProjector->MaxIndex
							);

						GStats.DWORDStats( GEngineStats.STATS_Projector_Projectors )++;
						GStats.DWORDStats( GEngineStats.STATS_Projector_Triangles ) += DynamicProjector->ClippedNumTriangles;
					}

					RI->PopState();
					GStats.DWORDStats( GEngineStats.STATS_Projector_RenderCycles ) += appCycles() - ProjectorStartCycles;
				}
			}
            unguard; // sjs
		}
		
		// Render wireframe lines for specific layers.
		if( GIsEditor && ShowGrid )
		{
			DECLARE_STATIC_UOBJECT( UConstantColor, WireframeColor, {} );
			DECLARE_STATIC_UOBJECT(
				UShader, WireframeShader, 
				{
					WireframeShader->Diffuse = WireframeColor;
					WireframeShader->Wireframe = 1;
				}
			);
			WireframeColor->Color = GetLevel()->Engine->C_TerrainWire;
			RI->EnableLighting(0,0);
			RI->SetMaterial(WireframeShader);

			for( INT s=0;s<VisibleSectorsCount;s++ )
			{
				UTerrainSector* Sector = Sectors(VisibleSectors[s]);
				FVertexStream*	Streams[1] = { &VertexStreams(Sector->VertexStreamNum) };
				RI->SetVertexStreams(VS_FixedFunction,Streams,1);

				for( INT p=0;p<Sector->RenderPasses.Num();p++ )
				{
					FTerrainSectorRenderPass& RenderPass = Sector->RenderPasses(p);
					UBOOL RenderGrid = 0;

					FTerrainRenderCombination* RenderCombination = RenderPass.GetRenderCombination(this);
					for( INT l=0;l<RenderCombination->Layers.Num();l++ )
					{
						if( ShowGrid&(1<<RenderCombination->Layers(l)) )
						{
							RenderGrid = 1;
							break;
						}
					}

					// Draw the grid for this renderpass
					if( RenderGrid )
					{
						FTerrainWireframeIndexBuffer WireframeIndexBuffer(&RenderPass);
						INT BaseIndex = RI->SetDynamicIndexBuffer(&WireframeIndexBuffer,0);
						RI->DrawPrimitive(PT_TriangleList,BaseIndex,RenderPass.NumTriangles,RenderPass.MinIndex,RenderPass.MaxIndex);					
					}
				}
			}
		}

		// Free the memory used to render terrain.
		Mark.Pop();
	}
#endif
	
	POP_HIT(SceneNode->Viewport);
	
	RI->PopState();
	
#ifdef CONSOLE
	RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);
#else // no need to have this code on PS2
	if( GIsEditor && GTerrainTools.EditorMode == EM_TerrainEdit )
	{
		FLOAT W;
		FVector V, VPA = FVector(0,0,0);
		FColor Color;
		
		//
		// Vertex being pointed at.
		//
		
		if( GTerrainTools.GetCurrentTerrainInfo() == this && GTerrainTools.CurrentBrush->bShowRedVertex )
		{
			Color = FColor(255,0,0);
			VPA = GTerrainTools.PointingAt + FVector(0,0,32);
			DrawVertex( SceneNode, RI, VPA, Color );
		}
		
		//
		// Selected vertices.
		//
		
		if( GTerrainTools.CurrentBrush->bShowVertices )
			for( INT i=0;i<SelectedVertices.Num();i++ )
			{
				W = SelectedVertices(i).Weight;
				Color = FColor(W*255,W*255,W*255);
				
				V = Vertices( GetGlobalVertex(SelectedVertices(i).X, SelectedVertices(i).Y ) );
				V += FVector(0,0,32);
				
				// If this vertex is selected AND it's the one being pointed at, tint it red.
				if( V.X == VPA.X && V.Y == VPA.Y )
					Color = FColor(255,W*128,W*128 );
				
				DrawVertex( SceneNode, RI, V, Color );
			}
			
			//
			// Circle markers.
			//
			
			if( GTerrainTools.GetCurrentTerrainInfo() == this )
			{
				if( GTerrainTools.CurrentBrush->bShowMarkers )
				{
					if( GTerrainTools.CurrentBrush->bUseInnerRadius )
						DrawCircleOnTerrain( RI, this, SceneNode->Viewport->TerrainPointAtLocation, GTerrainTools.GetInnerRadius(), FColor(255,255,0) );
					DrawCircleOnTerrain( RI, this, SceneNode->Viewport->TerrainPointAtLocation, GTerrainTools.GetOuterRadius(), FColor(160,160,0) );
				}
				
				//
				// Rectangle.
				//
				
				if( GTerrainTools.CurrentBrush->bShowRect )
					DrawRectOnTerrain( RI, this, GTerrainTools.CurrentBrush->GetRect(), FColor(160,160,0) );
			}
	}
#endif
	
	unguard;
}

void ATerrainInfo::UpdateDecorations( INT SectorIndex )
{
	guard(ATerrainInfo::UpdateDecorations);

	if ( !DecoLayerData.Num() )
	{
		guard(ATerrainInfo::UpdateDecorations::InitDecorations);
		DecoLayerData.AddZeroed( DecoLayers.Num() );
		for( INT LayerIndex=0; LayerIndex<DecoLayers.Num(); LayerIndex++ )
			DecoLayerData(LayerIndex).Sectors.AddZeroed( Sectors.Num() );
		unguard;
	}

	UTerrainSector* Sector = Sectors(SectorIndex);

	for (INT LayerIndex=0; LayerIndex<DecoLayers.Num(); LayerIndex++)
	{
		DecoLayerData(LayerIndex).Sectors(SectorIndex).DecoInfo.Empty();
		DecoLayerData(LayerIndex).Sectors(SectorIndex).Location = Sector->Location;
		DecoLayerData(LayerIndex).Sectors(SectorIndex).Radius	= Sector->Radius;

		FDecorationLayer& DecoLayer = DecoLayers(LayerIndex);
		INT Counter = 0;

		if ( !DecoLayer.StaticMesh || !DecoLayer.DensityMap )
			continue;

		UTexture* DensityMap = DecoLayer.DensityMap;
		UTexture* ScaleMap   = DecoLayer.ScaleMap;
		UTexture* ColorMap   = DecoLayer.ColorMap;

		// Initialize RNG per layer/ sector.
		appSRandInit(DecoLayer.Seed + SectorIndex);

		for (INT y=0; y<Sector->QuadsY; y++)
		{
			for (INT x=0; x<Sector->QuadsX; x++)
			{
				INT GlobalX = Sector->OffsetX + x;
				INT GlobalY = Sector->OffsetY + y;

				if( !DecoLayer.ShowOnInvisibleTerrain && !GetQuadVisibilityBitmap( GlobalX, GlobalY ) )
					continue;

				for (INT i=0; i<DecoLayer.MaxPerQuad; i++)
				{						
					FLOAT temp = GetLayerAlpha(GlobalX,GlobalY,0,DensityMap) / 255.f;			
					if (appSRand() < (temp * DecoLayer.DensityMultiplier.GetSRand()))
					{
						INT TempIndex = DecoLayerData(LayerIndex).Sectors(SectorIndex).DecoInfo.AddZeroed(1);
						FDecoInfo& DecoInfo = DecoLayerData(LayerIndex).Sectors(SectorIndex).DecoInfo( TempIndex );

						FLOAT RandX = appSRand();
						FLOAT RandY = appSRand();

						FVector Normal;
						FVector Location;
								
						// Calculate Z value.
						if ( RandX > RandY )	// 1,2,4
						{
							FVector DirX = Vertices(Sector->GetGlobalVertex( x, y )) - Vertices(Sector->GetGlobalVertex( x+1, y ));
							FVector DirY = Vertices(Sector->GetGlobalVertex( x+1, y+1 )) - Vertices(Sector->GetGlobalVertex( x+1, y ));
							Location	 = Vertices(Sector->GetGlobalVertex( x+1, y )) + DirX * (1-RandX) + DirY * RandY;
							Normal		 = (DirX ^ DirY).SafeNormal();
						}
						else					// 1,4,3
						{
							FVector DirX = Vertices(Sector->GetGlobalVertex( x, y )) - Vertices(Sector->GetGlobalVertex( x, y+1 ));
							FVector DirY = Vertices(Sector->GetGlobalVertex( x+1, y+1 )) - Vertices(Sector->GetGlobalVertex( x, y+1 ));
							Location	 = Vertices(Sector->GetGlobalVertex( x, y+1 )) + DirX * RandX + DirY * (1 - RandY);
							Normal		 = (DirX ^ DirY).SafeNormal();
						}

						if (Normal.Z < 0)
							Normal *= -1;

						if( Inverted )
							Normal *= -1;

						Normal = DecoLayer.AlignToTerrain ? Normal : Inverted ? FVector(0.f,0.f,-1.f) : FVector(0.f,0.f,1.f);

						FRotator Rotation = Normal.Rotation();
						// Objects are created along Z axis and not -X
						Rotation.Pitch -= 16384;
						if( DecoLayer.RandomYaw )
							Rotation.Yaw	= 65535 * appSRand();

						// Position/Rotation/Scale.
						DecoInfo.Location	= Location + Normal * DecoLayerOffset;
						DecoInfo.Rotation	= Rotation;
						DecoInfo.Scale		= DecoLayer.ScaleMultiplier.GetSRand();
						DecoInfo.Distance	= Counter++;

						// ScaleMap.
						if ( ScaleMap )
							DecoInfo.Scale *= (FVector)GetTextureColor( GlobalX, GlobalY, ScaleMap);

						// Terrain lighting vs constant color.
						if( DecoLayer.DisregardTerrainLighting )
							DecoInfo.Color = FColor( 127,127,127,255 );
						else
							DecoInfo.Color = VertexStreams(Sector->VertexStreamNum).Vertices( Sector->VertexStreamOffset + Sector->GetLocalVertex(x,y) ).Color;

						// ColorMap.
						if ( ColorMap )
						{
							FVector Color = GetTextureColor(GlobalX, GlobalY, ColorMap);
							DecoInfo.Color.R *= Color.X;
							DecoInfo.Color.G *= Color.Y;
							DecoInfo.Color.B *= Color.Z;
						}
					}
				}
			}
		}
	}
	unguard;
};

static INT DecoSortOrder = 1;
static inline INT Compare(FDecoInfo& A, FDecoInfo& B)
{
	return (A.Distance - B.Distance) * DecoSortOrder;
}

void ATerrainInfo::RenderDecorations(FLevelSceneNode* SceneNode,FRenderInterface* RI,FVisibilityInterface* VI)
{
	guard(ATerrainInfo::RenderDecorations);

	if( !TerrainMap || !DecoLayers.Num() )
		return;

	if( GIsEditor )
	{
		// Hide this terrain if the terraininfo actor is hidden.
		if( IsHiddenEd() )
			return;
	}

	CheckComputeDataOnLoad();

	INT		RenderedTriangles	= 0,
			RenderedDecorations	= 0;
	FLOAT	MaxMeshExtent		= 0.f;

	// Find maximum mesh extent.
	for( INT i=0; i<DecoLayers.Num(); i++ )
	{
		if ( DecoLayers(i).StaticMesh )
		{
			FVector Extent = DecoLayers(i).StaticMesh->BoundingBox.GetExtent();
			MaxMeshExtent = Max<FLOAT>( MaxMeshExtent, Extent.GetMax() * DecoLayers(i).ScaleMultiplier.GetMax().GetMax() );
		}
	}

	const  FVector& Origin = SceneNode->ViewOrigin;

	// Roughly sort sectors by distance. (decolayer 0 is dominant)
	INT* SectorIndices = new INT[Sectors.Num()];
	for( INT s=Sectors.Num()-1; s>=0; s-- )
		SectorIndices[s] = s + (((INT) FDist( Origin, DecoLayerData(0).Sectors(s).Location )) << 16);
	if( DecoLayers(0).DrawOrder != SORT_NoSort )
	{
		IntSortOrder = (DecoLayers(0).DrawOrder == SORT_FrontToBack) ? -1 : 1;
		Sort( SectorIndices, Sectors.Num() );
	}
	for( INT s=Sectors.Num()-1; s>=0; s-- )
		SectorIndices[s] &= 0xFFFF;
	
	// Iterate through all sectors.
	for( INT s=Sectors.Num()-1; s>=0; s-- )
	{
		UTerrainSector* Sector		= Sectors(SectorIndices[s]);
		FBox			BoundingBox = Sector->BoundingBox;
			
		// Extent by maximum mesh size.
		BoundingBox = BoundingBox.ExpandBy( MaxMeshExtent );

		// We need our own visibility determination to avoid popping.
		if( !VI->Visible(BoundingBox) )
			continue;
		
		// Iterate through all deco layers.
		for( INT LayerIndex=0; LayerIndex<DecoLayers.Num(); LayerIndex++ )
		{
			const FDecorationLayer& DecoLayer = DecoLayers(LayerIndex);
					
			// Don't render if it doesn't make sense.
			if ( !DecoLayer.StaticMesh || !DecoLayer.DensityMap || !DecoLayer.ShowOnTerrain )
				continue;

			// Don't render if the detail mode is higher than the current one.
			if( DecoLayer.DetailMode > Level->DetailMode )
				continue;

			// Early out if nothing to render.
			INT Amount = DecoLayerData(LayerIndex).Sectors(SectorIndices[s]).DecoInfo.Num();
			if ( !Amount )
				continue;

			// Bail out if sector too far away.
			const FVector&	SectorLocation	= DecoLayerData(LayerIndex).Sectors(SectorIndices[s]).Location;
			const FLOAT&	SectorRadius	= DecoLayerData(LayerIndex).Sectors(SectorIndices[s]).Radius;
			if ( FDistSquared( Origin, SectorLocation ) > Square(DecoLayer.FadeoutRadius.Max + MaxMeshExtent + SectorRadius) )
				continue;

			FLOAT RMaxMin = 1.f;
			if ( DecoLayer.FadeoutRadius.Max - DecoLayer.FadeoutRadius.Min )
				RMaxMin /= DecoLayer.FadeoutRadius.Max - DecoLayer.FadeoutRadius.Min;
	
			FDecoInfo* DecoInfo = &DecoLayerData(LayerIndex).Sectors(SectorIndices[s]).DecoInfo(0);
			
			INT Rendered = 0;
			for( INT DecoIndex=0; DecoIndex<Amount; DecoIndex++ )
			{
				FLOAT Distance			= FDist(Origin, DecoInfo->Location);
                DecoInfo->TempScale.Y	= Distance; // sjs
								
				// Don't render if too far away.
				if ( Distance > DecoLayer.FadeoutRadius.Max )
				{
					DecoInfo->TempScale.X = -1.f;
					DecoInfo++;
					continue;
				}

				FLOAT DistanceScale = 1.f;	
				if ( Distance > DecoLayer.FadeoutRadius.Min )
					DistanceScale = 1 - (Distance - DecoLayer.FadeoutRadius.Min) * RMaxMin;
				
				DecoInfo->Distance		= (DecoInfo->Distance & 0xFFFF) | (appTrunc(Distance) << 16);
				DecoInfo->TempScale.X	= DistanceScale;
				
				DecoInfo++;
				Rendered++;
			}

			RenderedDecorations += Rendered;

			// Bail out if nothing to render.
			if ( !Rendered )
				continue;

			// Set sunlights.
			if( DecoLayers(LayerIndex).LitDirectional )
			{
				INT HWLightIndex = 0;
				for( INT LightIndex=0; LightIndex<Sector->LightInfos.Num(); LightIndex++)
				{
					if( Sector->LightInfos(LightIndex).LightActor->LightEffect == LE_Sunlight )
					{		
						FDynamicLight SunLight( Sector->LightInfos(LightIndex).LightActor );
						SunLight.Color = FPlane( 0.572f, 0.572f, 0.572f, 1.f );
						RI->SetLight( HWLightIndex++, &SunLight );
					}
				}
				for( INT LightIndex=HWLightIndex; LightIndex<8; LightIndex++ )
					RI->SetLight( LightIndex, NULL );
			}

			RI->EnableLighting( (DecoLayers(LayerIndex).LitDirectional && !SceneNode->Viewport->IsWire()) ? 1 : 0, 0, 1, NULL, SceneNode->Viewport->Actor->RendMap == REN_LightingOnly );
			
			// Set vertex and index buffers.
			UStaticMesh*	StaticMesh			= DecoLayers(LayerIndex).StaticMesh;
			FVertexStream*	VertexStreams[9]	= { &StaticMesh->VertexStream };
			INT				NumVertexStreams	= 1;

			for(INT UVIndex = 0;UVIndex < StaticMesh->UVStreams.Num();UVIndex++)
				VertexStreams[NumVertexStreams++] = &StaticMesh->UVStreams(UVIndex);

			RI->SetVertexStreams(VS_FixedFunction,VertexStreams,NumVertexStreams);
			RI->SetIndexBuffer(&StaticMesh->IndexBuffer,0);

			// Sort by section to reduce amount of state changes.
			for (INT SectionIndex=0; SectionIndex<StaticMesh->Sections.Num(); SectionIndex++)
			{
				const FStaticMeshSection&	Section		= StaticMesh->Sections(SectionIndex);
				UMaterial*					Material	= StaticMesh->Materials(SectionIndex).Material ? StaticMesh->Materials(SectionIndex).Material : SceneNode->Viewport->Actor->Level->DefaultTexture->Get(SceneNode->Viewport->CurrentTime,SceneNode->Viewport);
			
				if( Section.NumPrimitives == 0 )
					continue;

				DECLARE_STATIC_UOBJECT( UColorModifier, ColorModifier, {} );
				if(SceneNode->Viewport->IsWire())
				{
					DECLARE_STATIC_UOBJECT( UShader, WireframeShader, { WireframeShader->Wireframe = 1; });
					ColorModifier->Material = WireframeShader;
				}
				else		
					ColorModifier->Material = Material;
				RI->SetMaterial( ColorModifier );

				RenderedTriangles += Rendered * Section.NumPrimitives;

				DecoInfo = &DecoLayerData(LayerIndex).Sectors(SectorIndices[s]).DecoInfo(0);

				if( DecoLayer.DrawOrder != SORT_NoSort )
				{
					DecoSortOrder = (DecoLayer.DrawOrder == SORT_FrontToBack) ? 1 : -1;
					Sort( DecoInfo, DecoLayerData(LayerIndex).Sectors(SectorIndices[s]).DecoInfo.Num() );
				}

				// Finally render them all.
				for(INT i=0; i<Amount; i++)
				{
					// Skip if not visible (too far away).
					if ( DecoInfo->TempScale.X < 0.f )
					{
						DecoInfo++;
						continue;
					}
					
					RI->SetTransform(TT_LocalToWorld,FScaleMatrix(DecoInfo->Scale) * FRotationMatrix(DecoInfo->Rotation) * FTranslationMatrix(DecoInfo->Location));
					
					FColor Color(DecoInfo->Color);
					Color.A *= DecoInfo->TempScale.X;
					RI->SetGlobalColor(Color);

#ifdef __PSX2_EE__
					extern void PSX2Render_RenderStaticMesh(UStaticMesh*, UStaticMeshInstance*, INT);
					PSX2Render_RenderStaticMesh(StaticMesh, NULL, i);
#else
					RI->DrawPrimitive(
						Section.IsStrip ? PT_TriangleStrip : PT_TriangleList,
						Section.FirstIndex,
						Section.NumPrimitives,
						Section.MinVertexIndex,
						Section.MaxVertexIndex
					);
#endif
					DecoInfo++;
				}
			}
		}
	}

	delete [] SectorIndices;

	GStats.DWORDStats( GEngineStats.STATS_DecoLayer_Triangles ) += RenderedTriangles;
	GStats.DWORDStats( GEngineStats.STATS_DecoLayer_Decorations ) += RenderedDecorations;

	unguard;
}

void ATerrainInfo::execPokeTerrain( FFrame& Stack, RESULT_DECL )
{
	guard(ATerrainInfo::execPokeTerrain);
	P_GET_STRUCT(FVector, WorldLocation);
	P_GET_INT(WorldRadius);
	P_GET_INT(WorldMaxDepth);
	P_FINISH;

	if( !TerrainMap || TerrainMap->Format!=TEXF_G16 )
		return;

	TerrainMap->Mips(0).DataArray.Load();

	FVector Center = WorldToHeightmap(WorldLocation);
	FLOAT Radius = (FLOAT)WorldRadius / (FLOAT)TerrainScale.X;
	FLOAT MaxDepth = - 256 * WorldMaxDepth / TerrainScale.Z;
   
	INT X1 = Max<INT>( appRound(Center.X-Radius), 0 );
	INT X2 = Min<INT>( appRound(Center.X+Radius), HeightmapX );
	INT Y1 = Max<INT>( appRound(Center.Y-Radius), 0 );
	INT Y2 = Min<INT>( appRound(Center.Y+Radius), HeightmapY );

	for( INT y=Y1;y<=Y2;y++ )
		for( INT x=X1;x<=X2;x++ )
            G16Heightmap(x,y) = Clamp<INT>( appRound(MaxDepth*appSqrt( Radius*Radius - ((Center.X-x)*(Center.X-x)+(Center.Y-y)*(Center.Y-y)) ) / Radius) + (INT)G16Heightmap(x,y), 0, 65535 );

	UpdateVertices( 0, X1, Y1, X2+1, Y2+1 );
	UpdateTriangles( X1, Y1, X2, Y2, 0, 0 );

	unguard;
}
IMPLEMENT_FUNCTION( ATerrainInfo, -1, execPokeTerrain );

/*------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------*/

