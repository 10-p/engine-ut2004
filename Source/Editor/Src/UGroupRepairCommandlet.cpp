/*=============================================================================
//	UGroupRepairCommandlet.cpp: Finds any objects in the "(All)" Package and
//  moves them to the root of the package.
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

static bool DoPackage( UPackage* Package )
{
    FName AllName = TEXT("(All)");
    INT ObjectsMoved = 0;

	for( FObjectIterator ItA; ItA; ++ItA )
	{
        UObject *OuterObj = *ItA;

		if( !OuterObj->IsIn(Package) )
            continue;

		if( OuterObj->GetFName() == AllName )
		{
	        for( FObjectIterator ItB; ItB; ++ItB )
	        {
                UObject *Obj = *ItB;

		        if( Obj->GetOuter() != OuterObj )
                    continue;

                Obj->Rename( Obj->GetName(), Package );
                ObjectsMoved++;
	        }
        }
	}

    if( ObjectsMoved > 0 )
        GWarn->Logf (NAME_Log, TEXT("Moved %d objects..."), ObjectsMoved );

    return( ObjectsMoved > 0 );
}

/*-----------------------------------------------------------------------------
	UGroupRepairCommandlet.
-----------------------------------------------------------------------------*/

class UGroupRepairCommandlet : public UCommandlet
{
	DECLARE_CLASS(UGroupRepairCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UGroupRepairCommandlet::StaticConstructor);

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
		guard(UGroupRepairCommandlet::Main);

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

                GWarn->Logf (NAME_Log, TEXT("Loading %s..."), *PackageName );
                UPackage* Package = CastChecked<UPackage>( LoadPackage( NULL, *FileName, LOAD_NoWarn ) );

                if( DoPackage( Package ) )
                {
                    GWarn->Logf (NAME_Log, TEXT("Saving %s..."), *PackageName );
                	SavePackage( Package, NULL, RF_Standalone, *FileName, NULL );
                }

                UObject::CollectGarbage( RF_Native );

                Count++;
            }
        }

        if( !Count )
            GWarn->Log( NAME_Error, TEXT("Syntax: ucc GroupRepair <file[s]>") );

		GIsRequestingExit=1;
		return 0;

		unguard;
	}
};
IMPLEMENT_CLASS(UGroupRepairCommandlet)

