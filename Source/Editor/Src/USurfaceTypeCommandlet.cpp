/*=============================================================================
//	USurfaceTypeCommandlet.cpp: Fills in any surface types it can infer.
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

#define CHECK_MATERIAL(m) { BYTE st = FindSurfaceType(m); if (st != EST_Default) return st; }

static BYTE FindSurfaceType( UMaterial* Material )
{
    if( !Material )
        return( EST_Default );

    if( Material->SurfaceType != EST_Default )
        return( Material->SurfaceType );

    CHECK_MATERIAL( Material->FallbackMaterial );
    CHECK_MATERIAL( Material->DefaultMaterial );

    UShader* Shader = Cast<UShader>( Material );
    
    if( Shader )
    {
        CHECK_MATERIAL( Shader->Diffuse );
        CHECK_MATERIAL( Shader->Opacity );
        CHECK_MATERIAL( Shader->Specular );
        CHECK_MATERIAL( Shader->SpecularityMask );
        CHECK_MATERIAL( Shader->SelfIllumination );
        CHECK_MATERIAL( Shader->SelfIlluminationMask );
        CHECK_MATERIAL( Shader->Detail );
    }

    UModifier* Modifier = Cast<UModifier>( Material );
    
    if( Modifier )
        CHECK_MATERIAL( Modifier->Material );

    UOpacityModifier* OpacityModifier = Cast<UOpacityModifier>( Material );
    
    if( OpacityModifier )
        CHECK_MATERIAL( OpacityModifier->Opacity );

    UCombiner* Combiner = Cast<UCombiner>( Material );
    
    if( Combiner )
    {
        CHECK_MATERIAL( Combiner->Material1 );
        CHECK_MATERIAL( Combiner->Material2 );
        CHECK_MATERIAL( Combiner->Mask );
    }

    UMaterialSwitch* MaterialSwitch = Cast<UMaterialSwitch>( Material );
    
    if( MaterialSwitch )
    {
        for( INT i = 0; i < MaterialSwitch->Materials.Num(); i++ )
        {
            CHECK_MATERIAL( MaterialSwitch->Materials(i) );
        }
    }

    return( EST_Default );
}

static bool DoMaterial( UMaterial* Material )
{
    Material->SurfaceType = FindSurfaceType( Material );
    
    if( Material->SurfaceType != EST_Default )
    {
        GWarn->Logf( TEXT("Updated %s"), Material->GetPathName() );
        GChanged++;
        return( true );
    }
    
    return( false );
}

static bool DoPackage( UPackage* Package )
{
    bool TouchedAny = false;
    
    for( TObjectIterator<UMaterial> It; It; ++It )
    {
        if( !It->IsIn( Package ) )
            continue;
            
        if( It->SurfaceType == EST_Default )
            TouchedAny |= DoMaterial( *It );
    }

    return( TouchedAny );
}

/*-----------------------------------------------------------------------------
	USurfaceTypeCommandlet.
-----------------------------------------------------------------------------*/

class USurfaceTypeCommandlet : public UCommandlet
{
	DECLARE_CLASS(USurfaceTypeCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(USurfaceTypeCommandlet::StaticConstructor);

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
		guard(USurfaceTypeCommandlet::Main);

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

                if( GFileManager->IsReadOnly( *FileName ) )
                    continue;

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
            GWarn->Log( NAME_Error, TEXT("Syntax: ucc SurfaceType <file[s]>") );
        else
            GWarn->Logf( TEXT("Updated %d materials"), GChanged );
        
		GIsRequestingExit=1;
		return 0;

		unguard;
	}
};
IMPLEMENT_CLASS(USurfaceTypeCommandlet)


#if __STATIC_LINK
void autoInitializeRegistrantsEditorSurfType(INT &Lookup)
{
	USurfaceTypeCommandlet::StaticClass();
}
#endif

