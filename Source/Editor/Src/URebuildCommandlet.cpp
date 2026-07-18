/*=============================================================================
//	URebuildCommandlet.cpp: Rebuilds content.
//  UNRs are rebuilt, others are just opened/translated and saved back out.
//  Skips read-only files.
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

/*-----------------------------------------------------------------------------
	URebuildCommandlet.
-----------------------------------------------------------------------------*/

class URebuildCommandlet : public UCommandlet
{
	DECLARE_CLASS(URebuildCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(URebuildCommandlet::StaticConstructor);

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
		guard(URebuildCommandlet::Main);

    	FString PackageWildcard;
        INT Count = 0;

		UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
        GEditor->InitEditor();
		GIsRequestingExit = 1; // Causes ctrl-c to immediately exit.

        while( ParseToken(Parms, PackageWildcard, 0) )
        {
    	    FString PackageName;
    	    FString FileName;
            TArray<FString> FilesInPath;
            FString PathPrefix;
            TArray <UObject*> Packages;
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
                PackageName = FilesInPath(i);
                FileName = PathPrefix + FilesInPath(i);

                if( GFileManager->IsReadOnly( *FileName ) )
                {
                    GWarn->Logf( NAME_Log, TEXT("Skipping %s (read-only)"), *PackageName );
                    continue;
                }

    			if( appStrstr( *PackageName, TEXT(".ut2") ) )
                {
                    GWarn->Logf( NAME_Log, TEXT("Skipping %s (currently unsupported)"), *PackageName );
                    /* MERGE_HACK
                    GWarn->Logf (NAME_Log, TEXT("Loading %s..."), *PackageName );
    		        GEditor->Exec( *FString::Printf(TEXT("MAP LOAD FILE=\"%s\""), *FileName ) );

                    GWarn->Logf (NAME_Log, TEXT("Rebuilding static meshes...") );
			        GEditor->Exec( TEXT("STATICMESH ALLREBUILD") );

                    GWarn->Logf (NAME_Log, TEXT("Rebuilding map...") );
			        GEditor->Exec( TEXT("MAP REBUILD") );

                    GWarn->Logf (NAME_Log, TEXT("Rebuilding BSP...") );
			        GEditor->Exec( TEXT("BSP REBUILD") );

                    GWarn->Logf (NAME_Log, TEXT("Rebuilding Lighting...") );
                    GEditor->Exec( TEXT("LIGHT APPLY") );

                    GWarn->Logf (NAME_Log, TEXT("Rebuilding Paths...") );
			        GEditor->Exec( TEXT("PATHS DEFINE") );

                    GWarn->Logf (NAME_Log, TEXT("Saving...") );
                    GEditor->Exec( *FString::Printf(TEXT("MAP SAVE FILE=\"%s\""), *FileName ) );
                    MERGE_HACK */
                }
                else
                {
                    GWarn->Logf (NAME_Log, TEXT("Loading %s..."), *PackageName );
                    UPackage* Package = CastChecked<UPackage>( LoadPackage( NULL, *FileName, LOAD_NoWarn ) );

                    GWarn->Logf (NAME_Log, TEXT("Saving...") );
					SavePackage( Package, NULL, RF_Standalone, *FileName, GError, NULL );
                }

                GWarn->Logf (NAME_Log, TEXT("Cleaning up...") );

                UObject::CollectGarbage( RF_Native );

                Count++;
            }
        }

        if( !Count )
            GWarn->Log( NAME_Error, TEXT("Syntax: ucc rebuild <file[s]>") );

		GIsRequestingExit=1;
		return 0;

		unguard;
	}
};
IMPLEMENT_CLASS(URebuildCommandlet)

