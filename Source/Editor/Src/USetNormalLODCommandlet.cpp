/*=============================================================================
	USetNormalLODCommandlet.cpp: Automatically set NormalLOD on textures.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel

=============================================================================*/

#include "EditorPrivate.h"
//#include <stdlib.h>

/*-----------------------------------------------------------------------------
	USetNormalLODCommandlet
-----------------------------------------------------------------------------*/

class USetNormalLODCommandlet : public UCommandlet
{
	DECLARE_CLASS(USetNormalLODCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(USetNormalLODCommandlet::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 1;
		ShowErrorCount  = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(USetNormalLOD::Main);
		FString InPackageName,
				OutPackageName;

		if( !Parse(appCmdLine(),TEXT("in="), InPackageName) )
			appErrorf(TEXT("Source filename not specified."));
	
		if( !Parse(appCmdLine(),TEXT("out="), OutPackageName) )
			appErrorf(TEXT("Source filename not specified."));
	
		GLazyLoad = 0;

		GEngineStats.Init();

		UClass* TextureClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("Texture") );

		GWarn->Logf( TEXT("Package %s...\n"), *InPackageName );

		UObject* Package = NULL;
		Package = LoadPackage(NULL,*InPackageName,LOAD_NoWarn);
	
		if (Package != NULL)
		{
			for( TObjectIterator<UObject> It; It; ++It )
			{
				if( It->IsA(TextureClass) && It->IsIn(Package) )
				{
					UTexture* Texture	= (UTexture*) *It;

					if( !Texture || Texture->GetClass() != UTexture::StaticClass() )
						continue;

					if( !Texture->Mips.Num() )
						continue;

#if 1
					DWORD Size = Texture->Mips(0).USize * Texture->Mips(0).VSize;
#else
					DWORD Size = 0;
					for( INT i=0; i<Texture->Mips.Num(); i++ )
						Size += Texture->Mips(i).USize * Texture->Mips(i).VSize;
#endif

					switch( Texture->Format )
					{
					case TEXF_DXT1:
						Size /= 2;
						break;
					case TEXF_DXT3:
						break;
					case TEXF_DXT5:
						break;
					case TEXF_P8:
						break;
					case TEXF_RGBA7:
						Size *= 4;
						break;
					case TEXF_RGB16:
						Size *= 8;
						break;
					case TEXF_RGB8:
						Size *= 4;
						break;
					case TEXF_RGBA8:
						Size *= 4;
						break;
					case TEXF_G16:
						Size *= 2;
						break;
					default:
						break;
					}

					// Don't adjust LOD on terrain heightmaps.
					if( Texture->Format == TEXF_G16 )
						continue;

					Size /= 1024;

					if( Size >= 1023)
					{
						Texture->NormalLOD = Max( 2, Texture->NormalLOD );
					}
					else if( Size >= 63 )
					{
						Texture->NormalLOD = Max( 1, Texture->NormalLOD );
					}
				}
			}

			SavePackage( Package, NULL, RF_Standalone, *OutPackageName, NULL );

			INT InSize	= GFileManager->FileSize( *InPackageName ),
				OutSize	= GFileManager->FileSize( *OutPackageName );

			if( InSize != OutSize )
			{
				debugf(TEXT("In : %i %s "), InSize,  *InPackageName );
				debugf(TEXT("Out: %i %s "), OutSize, *InPackageName );
			}
		}

		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(USetNormalLODCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

