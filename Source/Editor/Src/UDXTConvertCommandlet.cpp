/*=============================================================================
	UDXTConvertCommandlet.cpp: DXT3/5 -> DXT1/RGBA conversion (KYRO I/II).
	Copyright 2000 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel.
=============================================================================*/

#include "EditorPrivate.h"
#include <stdlib.h>

/*-----------------------------------------------------------------------------
	UDXTConvertCommandlet
-----------------------------------------------------------------------------*/

class UDXTConvertCommandlet : public UCommandlet
{
	DECLARE_CLASS(UDXTConvertCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UDXTConvertCommandlet::StaticConstructor);

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
		guard(UDXTConvertCommandlet::Main);
		FString WildCard;
		if( !ParseToken(Parms,WildCard,0) )
			appErrorf(TEXT("Package search mask not specified."));
		UClass* TexClass = FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("Texture") );
		TArray<FString> FilesFound = GFileManager->FindFiles( *WildCard, 1, 0 );

		GLazyLoad = 0;
		for (INT i=0; i<FilesFound.Num(); i++)
		{
			FString Pkg = FilesFound(i);
			GWarn->Logf( TEXT("Package %s..."), *Pkg );
			GWarn->Logf( TEXT("  Loading") );

			UObject* Package = NULL;

			Package = LoadPackage(NULL,*Pkg,LOAD_NoWarn);

			if (Package != NULL)
			{
				GWarn->Logf( TEXT("  Converting") );
				for( TObjectIterator<UObject> It; It; ++It )
				{
					if( It->IsA(TexClass) && It->IsIn(Package) )
					{
						UTexture* Texture	= (UTexture*) *It;
#if 1
						Texture->ConvertDXT();
#else
						TCHAR TextureName[1024];
						appStrcpy(TextureName, Texture->GetName());
						if ( !  (
								appStrstr(appStrupr(TextureName), TEXT("ALPHA"))	||
								appStrstr(appStrupr(TextureName), TEXT("TERRAIN"))  ||
								(Texture->Format != TEXF_RGBA8)
								))
						{
							UBOOL ConvertToDXT1 = true;
							
							INT Width	= Texture->Mips(0).USize;
							INT Height	= Texture->Mips(0).VSize;
							
							// Texture are always lazy loaded.
							Texture->Mips(0).DataArray.Load();
							check( &Texture->Mips(0).DataArray(0) )

							INT* Data = (INT*) &Texture->Mips(0).DataArray(0);

							INT Alpha = ETrueColor_A;
							for( INT Y=0; Y<Height; Y++ );
								for( INT X=0; X<Width; X++ );
									Alpha &= (*(Data++) & ETrueColor_A);

							if( Alpha != ETrueColor_A )
								ConvertToDXT1 = false;
							
							Texture->Compress( ConvertToDXT1 ? TEXF_DXT1 : TEXF_DXT5, 1 );
						}
#endif
					}
				}

				GWarn->Logf( TEXT("  Saving") );
				
				if (appStrstr(*Pkg, TEXT(".ut2")))
				{
					// Save the updated map
					GEditor->Exec( *FString::Printf(TEXT("MAP SAVE FILE=\"%s\""), *Pkg ) );
				}
				else
					SavePackage( Package, NULL, RF_Standalone, *Pkg, GError, NULL );
				UObject::CollectGarbage(RF_Native);
			}
		}

		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UDXTConvertCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

