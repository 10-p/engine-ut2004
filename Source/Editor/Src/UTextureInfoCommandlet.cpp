/*=============================================================================
//	UTextureInfoCommandlet.cpp: Dumps texture info so we can ressurect it.
//
//	Copyright 2001 Digital Extremes. All Rights Reserved.
//  Confidential.
=============================================================================*/

#include "EditorPrivate.h"

static INT Compare( FString& A, FString& B )
{
	return appStricmp( *A, *B );
}

static FString GetDirName( const FString &Path )
{
    INT chopPoint;

    chopPoint = Max (Path.InStr( TEXT("/"), 1 ) + 1, Path.InStr( TEXT("\\"), 1 ) + 1);

    if (chopPoint < 0)
        chopPoint = Path.InStr( TEXT("*"), 1 );

    if (chopPoint < 0)
        return (TEXT(""));

    return (Path.Left( chopPoint ) );
}

static void DoTexture( UTexture* Texture, UPackage* Package )
{
    /*
    if( Texture->Detail )
    {
        GWarn->Logf( TEXT("POKE Texture %s Detail %s"), Texture->GetPathName(), Texture->Detail->GetPathName() );
        GWarn->Logf( TEXT("POKE Texture %s DetailScale %f"), Texture->GetPathName(), Texture->DetailScale );
    }
    */
    
    if( Texture->Format == TEXF_DXT5 )
    {
        FString Line;
        
        Line = TEXT("TEXTURE IMPORT");
        Line += FString::Printf(TEXT(" PACKAGE=%s"), Package->GetName() );
        
        if( Texture->GetOuter() != Package )
            Line += FString::Printf(TEXT(" GROUP=%s"), Texture->GetOuter()->GetName() );
            
        Line += FString::Printf(TEXT(" FILE=%s.tga"), Texture->GetName() );

        Line += FString::Printf(TEXT(" MIPS=%d"), Texture->Mips.Num() > 1 );

        Line += FString::Printf(TEXT(" ALPHA=0") );

        Line += FString::Printf(TEXT(" DXT=1") );

        GWarn->Logf( TEXT("%s"), *Line );
    }
}

static void DoPackage( UPackage* Package )
{
    for( TObjectIterator<UTexture> It; It; ++It )
    {
        if( !It->IsIn( Package ) )
            continue;

        DoTexture( *It, Package );
    }
}

/*-----------------------------------------------------------------------------
	UTextureInfoCommandlet.
-----------------------------------------------------------------------------*/

class UTextureInfoCommandlet : public UCommandlet
{
	DECLARE_CLASS(UTextureInfoCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UTextureInfoCommandlet::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 1;
		ShowErrorCount  = 1;

		unguard;
	}

	INT Main( const TCHAR *Parms )
	{
		guard(UTextureInfoCommandlet::Main);

    	FString PackageWildcard;
        INT Count = 0;

		UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
        GEditor->InitEditor();
		GIsRequestingExit = 1; // Causes ctrl-c to immediately exit.

        while( ParseToken(Parms, PackageWildcard, 0) )
        {
            TArray<FString> FilesInPath;
            TArray <UObject*> Packages;
            FString PathPrefix;
            INT i;

            PathPrefix = GetDirName( PackageWildcard );

		    FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

            if( !FilesInPath.Num() )
            {
                GWarn->Logf( NAME_Error, TEXT("No packages found matching %s!"), *PackageWildcard );
                continue;
            }

            Sort( &FilesInPath(0), FilesInPath.Num() );

		    for( i = 0; i < FilesInPath.Num(); i++ )
            {
                FString PackageName = FilesInPath(i);
                FString FileName = PathPrefix + FilesInPath(i);
                FString IntName;

                UPackage* Package = CastChecked<UPackage>( LoadPackage( NULL, *FileName, LOAD_NoWarn ) );

                DoPackage( Package );

                UObject::CollectGarbage( RF_Native );

                Count++;
            }
        }

        if( !Count )
            GWarn->Log( NAME_Error, TEXT("Syntax: ucc TextureInfo <file[s]>") );
        
		GIsRequestingExit=1;
		return 0;

		unguard;
	}
};
IMPLEMENT_CLASS(UTextureInfoCommandlet)


#if __STATIC_LINK
void autoInitializeRegistrantsEditorTexInfo(INT &Lookup)
{
	UTextureInfoCommandlet::StaticClass();
}
#endif

