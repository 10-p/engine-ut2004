/*=============================================================================
//	UTextureStripCommandlet.cpp: Fills in any surface types it can infer.
//
//	Copyright 2001 Digital Extremes. All Rights Reserved.
//  Confidential.
=============================================================================*/

#include "EditorPrivate.h"

static INT GChanged = 0;

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


static bool DoTexture( UTexture* Texture )
{
    INT NewMinLOD = Min( (INT)Texture->NormalLOD, (INT)Texture->MaxLOD );

    if( Texture->MinLOD == NewMinLOD || Texture->LODSet == LODSET_None )
        return( false );

    Texture->MinLOD = NewMinLOD;

    checkSlow( (INT)Texture->MinLOD <= (INT)Texture->MaxLOD );

    for( INT i = 0; i < Texture->MinLOD; i++ )
    {
        Texture->Mips(i).DataArray.Load();
        Texture->Mips(i).DataArray.Empty();
    }

    GWarn->Logf( TEXT("Updated %s"), Texture->GetPathName() );
    GChanged++;
    return( true );
}

static bool DoPackage( UPackage* Package )
{
    bool TouchedAny = false;
    
    for( TObjectIterator<UTexture> It; It; ++It )
    {
        if( !It->IsIn( Package ) )
            continue;

        TouchedAny |= DoTexture( *It );
    }

    return( TouchedAny );
}

/*-----------------------------------------------------------------------------
	UTextureStripCommandlet.
-----------------------------------------------------------------------------*/

class UTextureStripCommandlet : public UCommandlet
{
	DECLARE_CLASS(UTextureStripCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UTextureStripCommandlet::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 0;
		ShowErrorCount  = 1;

		unguard;
	}

	INT Main( const TCHAR *Parms )
	{
		guard(UTextureStripCommandlet::Main);

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
				FString FilePrefix = FilesInPath(i);
				INT j = FilePrefix.InStr(TEXT("."));
				if( j != -1 ) 
					FilePrefix = FilePrefix.Left(j);


                if( GFileManager->IsReadOnly( *FileName ) )
                    continue;

                GWarn->Logf (NAME_Log, TEXT("Loading %s..."), *PackageName );
				GFileManager->Copy( TEXT("___temp.utx"), *FileName );
				ULinkerLoad* Conform = NULL;
				BeginLoad();
				Conform = UObject::GetPackageLinker( CreatePackage(NULL,*(FilePrefix+TEXT("_OLD"))), TEXT("___temp.utx"), LOAD_NoFail, NULL, NULL );		
				EndLoad();

				UPackage* Package = CastChecked<UPackage>( LoadPackage( NULL, *FileName, LOAD_NoWarn ) );

				if( DoPackage( Package ) )
                {
                    GWarn->Logf (NAME_Log, TEXT("Saving %s..."), *PackageName );
                	SavePackage( Package, NULL, RF_Standalone, *FileName, NULL, Conform );
                }

                UObject::CollectGarbage( RF_Native );

				GFileManager->Delete(TEXT("___temp.utx"));

                Count++;
            }
        }

        if( !Count )
            GWarn->Log( NAME_Error, TEXT("Syntax: ucc TextureStrip <file[s]>") );
        else
            GWarn->Logf( TEXT("Updated %d textures."), GChanged );
        
		GIsRequestingExit=1;
		return 0;

		unguard;
	}
};
IMPLEMENT_CLASS(UTextureStripCommandlet)

#if __STATIC_LINK
void autoInitializeRegistrantsEditorTexStrip(INT &Lookup)
{
	UTextureStripCommandlet::StaticClass();
}
#endif
