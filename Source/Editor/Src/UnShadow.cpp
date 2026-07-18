/*=============================================================================
	UnLight.cpp: Bsp light mesh illumination builder code
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EditorPrivate.h"
#include "UnRender.h"

extern ENGINE_API FRebuildTools GRebuildTools;

// gam ---

#define USE_XDXTEX 0 // sjs - switched off, perhaps causes crashing when UnrealEd is running!

// This is a bit of a hack -- the original CompressLightmaps code is in Engine
// and you can't get to GEditor->Exec from there. So, after a little rape and paste,
// there's a version here.

static void CompressLightmaps( UModel* Model )
{
	guard(CompressLightmaps);
#if !CONSOLE && !__LINUX__
	GWarn->BeginSlowTask(TEXT("Compressing lightmaps..."),1);
	for(INT TextureIndex = 0;TextureIndex < Model->LightMapTextures.Num();TextureIndex++)
	{
		FLightMapTexture*	Texture		= &Model->LightMapTextures(TextureIndex);
		
		if( Texture->StaticTexture.Revision != Texture->Revision )
		{
			if(GRebuildTools.GetCurrent()->LightmapFormat == TEXF_RGBA8)
			{
				Texture->StaticTexture.Data[0].Empty();
				Texture->StaticTexture.Data[1].Empty();
			}
			else
			{
				FColor*				TextureData = new FColor[LIGHTMAP_TEXTURE_WIDTH * LIGHTMAP_TEXTURE_HEIGHT];

				GWarn->StatusUpdatef(TextureIndex,Model->LightMapTextures.Num(),TEXT("Compressing lightmaps..."));

				// Calculate the lightmap.

				for(INT LightMapIndex = 0;LightMapIndex < Texture->LightMaps.Num();LightMapIndex++)
				{
					FLightMap*		LightMap	= &Model->LightMaps(Texture->LightMaps(LightMapIndex));
					FColor*			LightMapData= TextureData + LightMap->OffsetX + LightMap->OffsetY * LIGHTMAP_TEXTURE_WIDTH;

					LightMap->GetTextureData(0,LightMapData,LIGHTMAP_TEXTURE_WIDTH * 4,TEXF_RGBA8,0);
				}
				
				// Setup compression options.
				FRebuildOptions*	Options		= GRebuildTools.GetCurrent();

				// Save out lightmaps for debugging purposes.
				if( Options->SaveOutLightmaps )
					appCreateBitmap( TEXT("LM-"), LIGHTMAP_TEXTURE_WIDTH, LIGHTMAP_TEXTURE_HEIGHT, (DWORD*) TextureData );

				// ---

				INT DXT = (Options->LightmapFormat == TEXF_DXT3) ? 3 : 1;

				INT FileNum = appCreateBitmap( TEXT("TempLM-"), LIGHTMAP_TEXTURE_WIDTH, LIGHTMAP_TEXTURE_HEIGHT, (DWORD*) TextureData );
				TCHAR FileNameA[256];
				TCHAR FileNameB[256];

				check( FileNum >= 0 );

        		appSprintf( FileNameA, TEXT("TempLM-%05i.bmp"), FileNum );
        		appSprintf( FileNameB, TEXT("TempLM-%05i.dds"), FileNum );

				GEditor->Exec( *FString::Printf( TEXT("TEXTURE IMPORT FILE=\"%s\" NAME=\"TempLM\" PACKAGE=\"MyLevel\" MIPS=1 ALPHA=0 DXT=%d"), FileNameA, DXT ) );
				UTexture* NewTexture = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("TempLM") ));

				if( !NewTexture )
					continue;

				for( INT MipIndex = 0; MipIndex < ARRAY_COUNT(Texture->StaticTexture.Data); MipIndex++ )
				{
					INT Size = NewTexture->Mips(MipIndex).DataArray.Num();

					Texture->StaticTexture.Data[MipIndex].Empty( Size );
					Texture->StaticTexture.Data[MipIndex].Add( Size );
					appMemcpy(&Texture->StaticTexture.Data[MipIndex](0),&NewTexture->Mips(MipIndex).DataArray(0),Size);
				}

				Texture->StaticTexture.Width	= LIGHTMAP_TEXTURE_WIDTH;
				Texture->StaticTexture.Height	= LIGHTMAP_TEXTURE_HEIGHT;
				Texture->StaticTexture.Format	= NewTexture->Format;

    			Texture->StaticTexture.Revision	= Texture->Revision;

				GFileManager->Delete( FileNameA );
				GFileManager->Delete( FileNameB );

				FStringOutputDevice GetPropResult = FStringOutputDevice();
				GEditor->Get( TEXT("Obj"), *FString::Printf( TEXT("DELETE CLASS=MATERIAL OBJECT=\"%s\""), NewTexture->GetName() ), GetPropResult);

				if( GetPropResult.Len() )
					appErrorf( TEXT("Can't delete texture.\n\n%s"), *GetPropResult );

				// ---
			}
		}
	}		
	GWarn->EndSlowTask();
#endif
	unguard;
}

// --- gam

//
//	UEditorEngine::shadowIlluminateBsp
//	Raytracing entry point.
//

void UEditorEngine::shadowIlluminateBsp(ULevel* Level,UBOOL SelectedOnly,UBOOL ChangedOnly)
{
	guard(UEditorEngine::shadowIlluminateBsp);

	DOUBLE	StartTime = appSeconds();
	UBOOL	RebuildVisible = GRebuildTools.GetCurrent()->Options & REBUILD_OnlyVisible;

	// Compute light visibility.

	TestVisibility(Level,Level->Model,0,0);

	// Prepare actors for raytracing.

	for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
	{
		AActor*	Actor = Level->Actors(ActorIndex);

		if(Actor)
		{
			Actor->PreRaytrace();
			Actor->ClearRenderData();
		}
	}

	// Illuminate the level.

	Level->Model->Illuminate(Level->GetLevelInfo(),ChangedOnly);

	// Illuminate actors.

	GWarn->BeginSlowTask(TEXT("Illuminating actors"),1);

	for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
	{
		AActor*	Actor = Level->Actors(ActorIndex);

		if(Actor && (!Actor->IsHiddenEd() || RebuildVisible))
		{
			// Put movers in their local raytrace location.

			AMover*	Mover = Cast<AMover>(Actor);

			if(Mover)
				Mover->SetBrushRaytraceKey();

			// Calculate lighting for the actor.

			if(Actor->GetPrimitive())
				Actor->GetPrimitive()->Illuminate(Actor,ChangedOnly);

			Actor->PostRaytrace();
		}
		
		GWarn->StatusUpdatef(ActorIndex,Level->Actors.Num(),TEXT("Illuminating actors"));
	}

	// Mark lights that changed since the last rebuild as unchanged.

	for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
	{
		AActor*	Actor = Level->Actors(ActorIndex);

		if(Actor)
			Actor->bLightChanged = 0;
	}

	// Compress the lightmaps.
	if( Level && Level->Model )
	{
        // gam ---
        if( USE_XDXTEX )
            CompressLightmaps( Level->Model );
        else
		    Level->Model->CompressLightmaps();
        // --- gam
	}
	
	GWarn->EndSlowTask();

	debugf(TEXT("Illumination: %f seconds"),appSeconds() - StartTime);

	unguard;
}
/*---------------------------------------------------------------------------------------
   The End
---------------------------------------------------------------------------------------*/

