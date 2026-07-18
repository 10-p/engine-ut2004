/*=============================================================================
//	UAnalyzeBuildCommandlet.cpp: Checks game content for problems and gathers
//                                 statistics.
//	Copyright 2001 Digital Extremes. All Rights Reserved.
//  Confidential.
//Revision history:
//	* Created by Glen Miner.
//  * Spiced up by Daniel Vogel.
=============================================================================*/

#include "EditorPrivate.h"

class UAnalyzeBuildCommandlet;

struct PackagedObject
{
    FString Name, Package;
};

INT Compare( FString& A, FString& B )
{
	return appStricmp( *A, *B );
}

INT Compare( PackagedObject& A, PackagedObject& B )
{
	return appStricmp( *A.Name, *B.Name );
}

typedef void (AnalyzePacakgeCallback)(UObject *);

bool PackagesMatch( FString A, FString B )
{
    A = A.Left( A.InStr( TEXT("."), 1 ));
    B = B.Left( B.InStr( TEXT("."), 1 ));

    return (appStricmp( *A, *B) == 0);
}

FString FormatSize( int TotalBytes )
{
    if( TotalBytes < 1000 )
    {
        if (TotalBytes != 1)
            return FString::Printf( TEXT("%d bytes"), TotalBytes );
        else
            return FString::Printf( TEXT("%d byte"), TotalBytes );
    }
    else if( TotalBytes < 1000000 )
    {
        INT b, k, o;

        b = TotalBytes;

        k = b / 1000;
        b = b % 1000;

        o = b;
       
        return FString::Printf( TEXT("%d,%03d bytes"), k, o );
    }
    else
    {
        INT b, m, k, o;

        b = TotalBytes;

        m = b / 1000000;
        b = b % 1000000;

        k = b / 1000;
        b = b % 1000;

        o = b;
       
        return FString::Printf( TEXT("%d,%03d,%03d bytes"), m, k, o );
    }
}

void CheckStaticMeshVersions( UObject *Package )
{
	INT Found = 0;
	INT Obsolete = 0;
    INT SectionCount = 0;
    INT ImportErrors = 0;

	for( FObjectIterator It; It; ++It )
	{
        UObject *Obj = *It;

		if( !Obj->IsIn(Package) )
            continue;

        if( !Obj->IsA(UStaticMesh::StaticClass()) )
			continue;

        UStaticMesh *StaticMesh = CastChecked<UStaticMesh>(Obj);

		Found++;

		if( StaticMesh->NeededRebuild() )
			Obsolete++;
        
        for(SectionCount = 0; SectionCount < StaticMesh->Sections.Num(); SectionCount++)
        {
            if ( StaticMesh->Sections(SectionCount).NumPrimitives == 0)
            {
                GWarn->Logf( NAME_Warning, TEXT("    %s needs to be re-imported."), StaticMesh->GetFullName() ); 
                ImportErrors++;
                break;
            }
        }
	}

    if( Obsolete )
    {
        GWarn->Logf( NAME_Warning, TEXT("    %d static mesh%s need%s to be rebuilt!"), Obsolete,
                (Obsolete == 1) ? TEXT("") : TEXT("es"),
                (Obsolete == 1) ? TEXT("s") : TEXT(""));
    }

    if( ImportErrors )
    {
        GWarn->Logf( NAME_Warning, TEXT("    %d static mesh%s need%s to be re-imported!"), ImportErrors,
                (ImportErrors == 1) ? TEXT("") : TEXT("es"),
                (ImportErrors == 1) ? TEXT("s") : TEXT(""));    
    }
}

void CheckForConflictingPackageNames()
{
    TArray<FString> Files;
    INT i;

	for( i=0; i < GSys->Paths.Num(); i++ )
	{
		FString PackageWildcard;
        TArray<FString> FilesInPath;
        INT j;

		PackageWildcard = FString(appBaseDir()) + GSys->Paths(i);

		FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

        if( !FilesInPath.Num() )
            continue;

		for( j = 0; j < FilesInPath.Num(); j++ )
            new(Files)FString(FilesInPath(j));
    }

    if( Files.Num() )
        Sort( &Files(0), Files.Num() );

    for( i = 0; i < Files.Num() - 1; i++)
    {
        if (PackagesMatch( Files(i), Files(i+1) ))
            GWarn->Logf (NAME_Warning, TEXT("%s conflicts with %s"), *Files(i), *Files(i+1));
    }

    GWarn->Logf (NAME_Log, TEXT("%d packages checked."), Files.Num());
}

template <class T> static void CheckForConflictingNames(TCHAR *WildcardFilter, T *, AnalyzePacakgeCallback *callback = NULL )
{
    TArray<FString> Files;
    TArray<FString> BaseNames;
    TArray<PackagedObject> Objects;
    INT i;

	for( i=0; i < GSys->Paths.Num(); i++ )
	{
		FString PackageWildcard;
        TArray<FString> FilesInPath;
        FString PathPrefix;
        INT j;

		PackageWildcard = FString(appBaseDir()) + GSys->Paths(i);

        if( PackageWildcard.Right (appStrlen(WildcardFilter)) != WildcardFilter)
            continue;

		FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

        if( !FilesInPath.Num() )
            continue;

        Sort( &FilesInPath(0), FilesInPath.Num() );

        check( PackageWildcard.InStr(TEXT("*")) >= 0 );
        PathPrefix = PackageWildcard.Left( PackageWildcard.InStr( TEXT("*"), 1 ));

		for( j = 0; j < FilesInPath.Num(); j++ )
        {
            new(Files)FString(PathPrefix + FilesInPath(j));
            new(BaseNames)FString(FilesInPath(j));
        }
    }

    if( !Files.Num() )
    {
        GWarn->Logf (NAME_Log, TEXT("No packages found."));
        return;
    }

    for( i = 0; i < Files.Num(); i++)
    {
        UObject* Package;

        GWarn->Logf (NAME_Log, TEXT("Loading %s"), *BaseNames(i));

        FString File = Files(i);

        Package = UObject::LoadPackage( NULL, *File, 0 );

        UObject::ResetLoaders( NULL, 0, 1 );

        if( !Package )
        {
            GWarn->Logf (NAME_Error, TEXT("Could not load %s"), *BaseNames(i));
            continue;
        }

		CheckStaticMeshVersions( Package );

        if( callback )
            (*callback)( Package );

		for( FObjectIterator It; It; ++It )
		{
            UObject *Obj = *It;
            INT j;

			if( !Obj->IsIn(Package) )
                continue;

            if( !Obj->IsA(T::StaticClass()) )
                continue;

            j = Objects.AddZeroed();

            Objects (j).Name = Obj->GetName();
            Objects (j).Package = BaseNames(i);
		}

        UObject::CollectGarbage(RF_Native);
    }

    if( !Objects.Num() )
    {
        GWarn->Logf (NAME_Log, TEXT("No objects found (%d packages checked)."), Files.Num());
        return;
    }
/*
    GWarn->Logf (NAME_Log, TEXT("Checking for conflicting names..."));

    if( Objects.Num() )
        Sort( &Objects(0), Objects.Num() );

    for( i = 0; i < Objects.Num() - 1; i++)
    {
        if (Objects(i).Name == Objects(i+1).Name)
        {
            GWarn->Logf (NAME_Warning, TEXT("Found %s in both %s and %s"), *Objects(i).Name, *Objects(i).Package, *Objects(i+1).Package);
            i++;
        }
    }
*/
    GWarn->Logf (NAME_Log, TEXT("%d objects checked (spanning %d packages)."), Objects.Num(), Files.Num());
}

void AnalyzeTextureContent( UObject* Package )
{
    INT TotalTextures = 0;
    INT P8Textures = 0;
    INT RGBA7Textures = 0;
    INT RGB16Textures = 0;
    INT RGB8Textures = 0;
    INT RGBA8Textures = 0;
    INT G8Textures = 0;
    INT G16Textures = 0;
    INT Dxt1Textures = 0;
    INT Dxt3Textures = 0;
    INT Dxt5Textures = 0;
    INT FractalTextures = 0;
    INT Shaders = 0;
	
    for( FObjectIterator It; It; ++It )
    {
        UObject *Obj = *It;

        if( !Obj->IsIn( Package ) )
            continue;

        if( Obj->IsA( UShader::StaticClass() ) )
            Shaders++;

		UTexture *t = Cast<UTexture>(Obj);

        if( !t )
            continue;

        TotalTextures++;

        if( t->GetClass() != UTexture::StaticClass() )
            FractalTextures++;
        else
        {
			FString LODSetString;
			switch( t->LODSet )
			{
			case LODSET_None:
				LODSetString = TEXT("LODSET_None");
				break;
			case LODSET_World:
				LODSetString = TEXT("LODSET_World");
				break;
			case LODSET_PlayerSkin:
				LODSetString = TEXT("LODSET_PlayerSkin");
				break;
			case LODSET_WeaponSkin:
				LODSetString = TEXT("LODSET_WeaponSkin");
				break;
			case LODSET_Terrain:
				LODSetString = TEXT("LODSET_Terrain");
				break;
			case LODSET_Interface:
				LODSetString = TEXT("LODSET_Interface");
				break;
			case LODSET_RenderMap:
				LODSetString = TEXT("LODSET_RenderMap");
				break;
			case LODSET_Lightmap:
				LODSetString = TEXT("LODSET_Lightmap");
				break;
			default:
				LODSetString = TEXT("Unknown LODSET");
				break;
			}

			DWORD Size = 0;
			for( INT i=0; i<t->Mips.Num(); i++ )
				Size += t->Mips(i).USize * t->Mips(i).VSize;

            bool NeedsPalette = false;
            
            switch( t->Format )
            {
                case TEXF_DXT1:
                {
                    Dxt1Textures++;
					Size /= 2;
                    break;
                }

                case TEXF_DXT3:
                {
                    Dxt3Textures++;
                    break;
                }

                case TEXF_DXT5:
                {
                    Dxt5Textures++;
                    break;
                }

            	case TEXF_P8:
                {
                    P8Textures++;
                    NeedsPalette = true;
                    break;
                }

            	case TEXF_RGBA7:
                {
					Size *= 4;
                    RGBA7Textures++;
                    break;
                }

            	case TEXF_RGB16:
                {
					Size *= 8;
                    RGB16Textures++;
                    break;
                }

            	case TEXF_RGB8:
                {
					Size *= 4;
                    RGB8Textures++;
                    break;
                }

            	case TEXF_RGBA8:
                {
					Size *= 4;
                    RGBA8Textures++;
                    break;
                }

            	case TEXF_G16:
                {
					Size *= 2;
                    G16Textures++;
                    break;
                }

	            default:
                    GWarn->Logf( NAME_Warning, TEXT("    %s is in an unsupported format!"), t->GetName() );
            }

			Size /= 1024;

			GWarn->Logf( NAME_Log, TEXT("%s %ix%i %i Mips %i KByte NormalLOD %i %s %s"), *t->GetFormatDesc(), t->USize, t->VSize, t->GetNumMips(), Size, t->NormalLOD, *LODSetString, t->GetPathName() );
        }
    }

    if( P8Textures )
    {
        GWarn->Logf( NAME_Log, TEXT("    %d P8 texture%s."), P8Textures, (P8Textures == 1) ? TEXT("") : TEXT("s") );
    }

    if( RGB8Textures )
    {
        GWarn->Logf( NAME_Log, TEXT("    %d RGB8 texture%s."), RGB8Textures, (RGB8Textures == 1) ? TEXT("") : TEXT("s") );
    }

    if( RGBA8Textures )
    {
        GWarn->Logf( NAME_Log, TEXT("    %d RGBA8 texture%s."), RGBA8Textures, (RGBA8Textures == 1) ? TEXT("") : TEXT("s") );
    }

    if( G8Textures )
    {
        GWarn->Logf( NAME_Log, TEXT("    %d G8 texture%s."), G8Textures, (G8Textures == 1) ? TEXT("") : TEXT("s") );
    }

    if( G16Textures )
    {
        GWarn->Logf( NAME_Log, TEXT("    %d G16 texture%s."), G16Textures, (G16Textures == 1) ? TEXT("") : TEXT("s") );
    }

    if( Dxt1Textures )
    {
        GWarn->Logf( NAME_Log, TEXT("    %d DXT1 texture%s."), Dxt1Textures, (Dxt1Textures == 1) ? TEXT("") : TEXT("s") );
    }

    if( Dxt3Textures )
    {
        GWarn->Logf( NAME_Log, TEXT("    %d DXT3 texture%s."), Dxt3Textures, (Dxt3Textures == 1) ? TEXT("") : TEXT("s") );
    }

    if( Dxt5Textures )
    {
        GWarn->Logf( NAME_Log, TEXT("    %d DXT5 texture%s."), Dxt5Textures, (Dxt5Textures == 1) ? TEXT("") : TEXT("s") );
    }

    if( FractalTextures )
    {
        GWarn->Logf( NAME_Log, TEXT("    %d fractal texture%s."), FractalTextures, (FractalTextures == 1) ? TEXT("") : TEXT("s") );
    }

    if( Shaders )
    {
        GWarn->Logf( NAME_Log, TEXT("    %d shader%s."), Shaders, (Shaders == 1) ? TEXT("") : TEXT("s") );
    }
}

void AnalyzeTexturePackage( UObject* Package )
{
    AnalyzeTextureContent( Package );

	for( FObjectIterator It; It; ++It )
	{
        UObject *Obj = *It;

        if( !Obj->IsIn( Package ) )
            continue;

        if( !Obj->IsA(UMaterial::StaticClass()) && !Obj->IsA(UPalette::StaticClass()) && !Obj->IsA(UPackage::StaticClass()) )
            GWarn->Logf (NAME_Warning, TEXT("    %s is a %s and doesn't belong in this package!"), Obj->GetName(), Obj->GetClass()->GetName() );
    }
}

void AnalyzeSoundContent( UObject* Package )
{
    INT TotalSounds = 0;

	for( FObjectIterator It; It; ++It )
    {
        UObject *Obj = *It;

		USound *Sound = Cast<USound>(Obj);

        if( !Sound || !Sound->IsIn( Package ) )
            continue;

		if( !Sound->IsValid() )
			continue;

		Sound->GetData().Load();
		if( Sound->GetData().Num() > 0 )
		{
			FWaveModInfo WaveInfo;
			WaveInfo.ReadWaveInfo(Sound->GetData());

			INT Size = Sound->GetData().Num() / 1024;

			GWarn->Logf (NAME_Log, TEXT("%i %i KByte %s"), *WaveInfo.pSamplesPerSec, Size, Sound->GetPathName() );
		}
		else
			GWarn->Logf (NAME_Log, TEXT("Problem loading %s"), Sound->GetPathName() );

		Sound->GetData().Unload();

        TotalSounds++;
    }

    if( TotalSounds )
    {
        GWarn->Logf (NAME_Log, TEXT("    %d sound%s."), TotalSounds, (TotalSounds == 1) ? TEXT("") : TEXT("s") );
    }
}

void AnalyzeSoundPackage( UObject* Package )
{
    AnalyzeSoundContent( Package );

	for( FObjectIterator It; It; ++It )
	{
        UObject *Obj = *It;

        if( !Obj->IsIn( Package ) )
            continue;

        if( !Obj->IsA(USound::StaticClass()) && !Obj->IsA(UPackage::StaticClass()) )
            GWarn->Logf (NAME_Warning, TEXT("    %s is a %s and doesn't belong in this package!"), Obj->GetName(), Obj->GetClass()->GetName() );
    }
}

void AnalyzeStaticMeshContent( UObject* Package )
{
    INT TotalStaticMeshes = 0;

	for( FObjectIterator It; It; ++It )
    {
        UObject *Obj = *It;

		UStaticMesh *s = Cast<UStaticMesh>(Obj);

        if( !s || !s->IsIn( Package ) )
            continue;

        TotalStaticMeshes++;
    }

    if( TotalStaticMeshes )
    {
        GWarn->Logf (NAME_Log, TEXT("    %d static mesh%s."), TotalStaticMeshes, (TotalStaticMeshes == 1) ? TEXT("") : TEXT("es") );
    }
}

void AnalyzeStaticMeshPackage( UObject* Package )
{
    AnalyzeStaticMeshContent( Package );

	for( FObjectIterator It; It; ++It )
	{
        UObject *Obj = *It;

        if( !Obj->IsIn( Package ) )
            continue;

        if( !Obj->IsA(UStaticMesh::StaticClass()) && !Obj->IsA(UMaterial::StaticClass()) && !Obj->IsA(UPalette::StaticClass()) && !Obj->IsA(UPackage::StaticClass()) && !Obj->IsA( UKMeshProps::StaticClass() ) && !Obj->IsA( UPolys::StaticClass() ) && !Obj->IsA( UModel::StaticClass() ) )
            GWarn->Logf (NAME_Warning, TEXT("    %s is a %s and doesn't belong in this package!"), Obj->GetName(), Obj->GetClass()->GetName() );

		if( Obj->IsA(UStaticMesh::StaticClass()) )
		{
			((UStaticMesh*)Obj)->CheckForErrors();
		}
    }
}

void AnalyzeModelContent( UObject* Package )
{
    INT TotalModels = 0;
    INT TotalBytes = 0;

	for( FObjectIterator It; It; ++It )
    {
        UObject *Obj = *It;

        if( !Obj || !Obj->IsIn( Package ) )
            continue;

        if( Obj->IsA(UModel::StaticClass()) || Obj->IsA(UMesh::StaticClass()) )
        {
            TArray<BYTE> Buffer;
            FBufferWriter Archive( Buffer );

            Obj->Serialize ( Archive );

            TotalBytes += Buffer.Num();
            TotalModels++;
        }
    }

    if( TotalModels )
    {
        FString ByteString;
        ByteString = FormatSize( TotalBytes );
        GWarn->Logf (NAME_Log, TEXT("    %d model%s (%s)."), TotalModels, (TotalModels == 1) ? TEXT("") : TEXT("s"), *ByteString );
    }
}

void AnalyzeAnimationContent( UObject* Package )
{
    INT TotalAnimations = 0;
    INT TotalBytes = 0;

	for( FObjectIterator It; It; ++It )
    {
        UObject *Obj = *It;

        if( !Obj || !Obj->IsIn( Package ) )
            continue;

        if( Obj->IsA(UAnimation::StaticClass()) || Obj->IsA(UMeshAnimation::StaticClass()) )
        {
            TArray<BYTE> Buffer;
            FBufferWriter Archive( Buffer );

            Obj->Serialize ( Archive );

            TotalBytes += Buffer.Num();
            TotalAnimations++;
        } 

        if( Obj->IsA(UVertMesh::StaticClass()) )
        {
            UVertMesh* VertMesh;
            VertMesh = (UVertMesh*) Obj;

            if( VertMesh->AnimFrames == 1 )
            {
                GWarn->Logf (NAME_Warning, TEXT("    %s should be a static mesh not a vert mesh!"), VertMesh->GetName() );
            }
        }
    }

    if( TotalAnimations )
    {
        FString ByteString;

        ByteString = FormatSize( TotalBytes );
        GWarn->Logf (NAME_Log, TEXT("    %d animation%s (%s)."), TotalAnimations, (TotalAnimations == 1) ? TEXT("") : TEXT("s"), *ByteString );
    }
}

inline void AddReferencedMaterial( TArray<UMaterial*> &ReferencedMaterials, UMaterial *Material )
{
    if( !Material )
        return;

    ReferencedMaterials.AddUniqueItem( Material );
}

void AnalyzeTextureUsage( UObject* Package )
{
	TArray<UMaterial*> ReferencedMaterials;
    INT i;

	for( FObjectIterator It; It; ++It )
	{
        UObject *Obj = *It;

        if( !Obj->IsIn(Package) )
            continue;

        if( !Obj->IsA( AActor::StaticClass() ) )
            continue;

		AActor *Actor = Cast<AActor>(Obj);

		UModel *M = Actor->IsA(ALevelInfo::StaticClass()) ? Actor->GetLevel()->Model : Actor->Brush;

        if( M && M->IsIn(Package) )
        {
			for( TArray<FBspSurf>::TIterator ItS(M->Surfs); ItS; ++ItS )
                AddReferencedMaterial( ReferencedMaterials, ItS->Material );

			if( M->Polys && Actor->IsA(AMover::StaticClass()) )
			{
				for( TArray<FPoly>::TIterator ItP(M->Polys->Element); ItP; ++ItP )
                    AddReferencedMaterial( ReferencedMaterials, ItP->Material );
			}
        }
        else if( Actor->StaticMesh && Actor->StaticMesh->IsIn(Package) )
        {
            UStaticMesh* StaticMesh = Actor->StaticMesh;

            for( i = 0; i < StaticMesh->Materials.Num(); i++ )
                AddReferencedMaterial( ReferencedMaterials, StaticMesh->Materials(i).Material );
        }

        if( Actor->bHidden )
            continue;

		for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> ItP(Actor->GetClass()); ItP; ++ItP )
        {
            UProperty *Prop = *ItP;

            if( !Prop->IsA(UObjectProperty::StaticClass()) )
                continue;

            UObjectProperty* Ref = Cast<UObjectProperty>( Prop );

		    for( INT i = 0; i < Ref->ArrayDim; i++ )
            {
                UObject *RefObject = NULL;
                UTexture *Texture;

			    Ref->CopySingleValue( &RefObject, (BYTE*)Actor + Ref->Offset + (i * Prop->ElementSize) );

                if( !RefObject )
                    continue;

                if( RefObject->GetClass() != UTexture::StaticClass() )
                    continue;

                Texture = Cast<UTexture>(RefObject);

                AddReferencedMaterial( ReferencedMaterials, Texture );
            }
        }
	}

    if( ReferencedMaterials.Num() )
        GWarn->Logf (NAME_Log, TEXT("    %d material%s."), ReferencedMaterials.Num(), (ReferencedMaterials.Num() == 1) ? TEXT("") : TEXT("s") );
}

inline void AddReferencedSound (TArray<USound*> &ReferencedSounds, USound *Sound)
{
    if( Sound )
        ReferencedSounds.AddUniqueItem( Sound );
}

void AnalyzeSoundUsage( UObject* Package )
{
	TArray<USound*> ReferencedSounds;

	for( FObjectIterator It; It; ++It )
	{
        UObject *Obj = *It;

        if( !Obj->IsIn( Package ) )
            continue;

        if( !Obj->IsA( AActor::StaticClass() ) )
            continue;

		AActor *Actor = Cast<AActor>(Obj);
		
		for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> ItP(Actor->GetClass()); ItP; ++ItP )
        {
            UProperty *Prop = *ItP;

            if( !Prop->IsA(UObjectProperty::StaticClass()) )
                continue;

            UObjectProperty* Ref = Cast<UObjectProperty>( Prop );

		    for( INT i = 0; i < Ref->ArrayDim; i++ )
            {
                UObject *RefObject = NULL;
                USound *Sound;

			    Ref->CopySingleValue( &RefObject, (BYTE*)Actor + Ref->Offset + (i * Prop->ElementSize) );

                if( !RefObject )
                    continue;

                if( RefObject->GetClass() != USound::StaticClass() )
                    continue;

                Sound = Cast<USound>(RefObject);

                AddReferencedSound( ReferencedSounds, Sound );
            }
        }
	}

    if( ReferencedSounds.Num() )
        GWarn->Logf (NAME_Log, TEXT("    %d sound%s."), ReferencedSounds.Num(), (ReferencedSounds.Num() == 1) ? TEXT("") : TEXT("s") );
}

void AnalyzeScripts()
{
    INT i;
    INT PackagesChecked = 0;

	for( i=0; i < GSys->Paths.Num(); i++ )
	{
		FString PackageWildcard;
        TArray<FString> FilesInPath;
        FString PathPrefix;
        INT j;

		PackageWildcard = FString(appBaseDir()) + GSys->Paths(i);

        if( PackageWildcard.Right (appStrlen(TEXT ("*.u"))) != TEXT ("*.u"))
            continue;

		FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

        if( !FilesInPath.Num() )
            continue;

        Sort( &FilesInPath(0), FilesInPath.Num() );

        check( PackageWildcard.InStr(TEXT("*")) >= 0 );
        PathPrefix = PackageWildcard.Left( PackageWildcard.InStr( TEXT("*"), 1 ));

		for( j = 0; j < FilesInPath.Num(); j++ )
        {
            const FString &File = FilesInPath(j);
            UObject* Package;

            GWarn->Logf (NAME_Log, TEXT("Loading %s"), *File);

			Package = UObject::LoadPackage( NULL, *File, 0 );

            UObject::ResetLoaders( NULL, 0, 1 );

            if( !Package )
            {
                GWarn->Logf (NAME_Log, TEXT("    Error loading %s!"), *File);
                continue;
            }

			CheckStaticMeshVersions( Package );

            AnalyzeTextureContent( Package );
            AnalyzeSoundContent( Package );
            AnalyzeModelContent( Package );
            AnalyzeAnimationContent( Package );
            AnalyzeStaticMeshContent( Package );

            PackagesChecked++;

            UObject::CollectGarbage(RF_Native);
        }
    }

    GWarn->Logf (NAME_Log, TEXT("%d script packages checked."), PackagesChecked);
}

bool CheckGameType( const TCHAR* MapName, const TCHAR* DefaultGameType )
{
	const TArray<FGameRecord>* Records = (TArray<FGameRecord>*)(Cast<UCacheManager>(UCacheManager::StaticClass()->GetDefaultObject())->GetRecords(TEXT("Game")));
	check(Records);

	INT PrefixLength = 0;
	bool MatchedAPrefix = false;
	for (INT i = 0; i < Records->Num(); i++)
	{
		const TCHAR* T = *(*Records)(i).MapPrefix;
		PrefixLength = appStrlen(T);
		if ( !appStrnicmp( MapName, T, PrefixLength ) && MapName[PrefixLength] == '-' )
		{
			MatchedAPrefix = true;
			break;
		}
	}

	for ( INT i = 0; i < Records->Num(); i++)
	{
		if (MatchedAPrefix)
		{
			if ((*Records)(i).ClassName == DefaultGameType)
			{
				PrefixLength = (*Records)(i).MapPrefix.Len();
				if ( !appStrnicmp( MapName, *((*Records)(i).MapPrefix), PrefixLength ) )
					return (true);
				else
					return (false);
			}
		}
		else
		{
			if ((*Records)(i).ClassName == DefaultGameType)
				return (true);
		}
	}
 
    if( MatchedAPrefix )
        return( false );

    return( appStricmp( DefaultGameType, TEXT("Engine.GameInfo") ) == 0 );

/*	TArray<FRegistryObjectInfo> List;
	UObject::GetRegistryObjects( List, UClass::StaticClass(), AGameInfo::StaticClass(), 0 );

    bool MatchedAPrefix = false;

    for( INT i = 0; i < List.Num(); i++ )
    {
	    UClass* GameClass = Cast<UClass>( UObject::StaticLoadObject( UClass::StaticClass(), NULL, *List(i).Object, NULL, 0, NULL ) );

        if( !GameClass )
            continue;

    	AGameInfo* GameInfo = CastChecked<AGameInfo>( GameClass->GetDefaultActor() );

        INT PrefixLength = appStrlen( *GameInfo->MapPrefix );

        if( ( appStrnicmp( MapName, *GameInfo->MapPrefix, PrefixLength ) == 0 ) && ( MapName[PrefixLength] == '-' ) )
        {
            MatchedAPrefix = true;
            break;
        }
    }

    for( INT i = 0; i < List.Num(); i++ )
    {
	    UClass* GameClass = Cast<UClass>( UObject::StaticLoadObject( UClass::StaticClass(), NULL, *List(i).Object, NULL, 0, NULL ) );

        if( !GameClass )
            continue;

    	AGameInfo* GameInfo = CastChecked<AGameInfo>( GameClass->GetDefaultActor() );

        if( MatchedAPrefix )
        {
            if( appStricmp( DefaultGameType, GameClass->GetPathName() ) == 0 )
            {
                INT PrefixLength = appStrlen( *GameInfo->MapPrefix );

                if( ( appStrnicmp( MapName, *GameInfo->MapPrefix, PrefixLength ) == 0 ) && ( MapName[PrefixLength] == '-' ) )
                    return( true );
                else
                    return( false );
            }
        }
        else
        {
            if( appStricmp( DefaultGameType, GameClass->GetPathName() ) == 0 )
                return( true );
        }
    }

    if( MatchedAPrefix )
        return( false );

    return( appStricmp( DefaultGameType, TEXT("Engine.GameInfo") ) == 0 );
*/
}

void CheckGameType( UObject* Package )
{
    INT LevelInfoCount;
    ALevelInfo *LevelInfo = NULL;

    LevelInfoCount = 0;

	for( FObjectIterator It; It; ++It )
	{
        UObject *Obj = *It;

        if( !Obj->IsIn( Package ) )
            continue;

        if( !Obj->IsA( ALevelInfo::StaticClass() ) )
            continue;

        LevelInfo = CastChecked<ALevelInfo>(Obj);
        LevelInfoCount++;
    }

    if (LevelInfoCount > 1)
        GWarn->Logf (NAME_Warning, TEXT("    %d extra LevelInfos found!"), LevelInfoCount - 1 );
    else if (LevelInfoCount == 0)
    {
        GWarn->Logf( NAME_Warning, TEXT("    No LevelInfo found!") );
        return;
    }

    check( LevelInfo );

    if( !CheckGameType( Package->GetName(), *LevelInfo->DefaultGameType ) )
    {
        if( LevelInfo->DefaultGameType.Len() )
            GWarn->Logf( NAME_Warning, TEXT("    %s is not a valid game-type for this map!"), *LevelInfo->DefaultGameType );
        else
            GWarn->Logf( NAME_Warning, TEXT("    No game-type set for this map!") );
    }
    
    if(( LevelInfo->IdealPlayerCountMin == 0 ) || ( LevelInfo->IdealPlayerCountMax == 0 ) || ( LevelInfo->IdealPlayerCountMin > LevelInfo->IdealPlayerCountMax ))
        GWarn->Logf( NAME_Warning, TEXT("    Invalid ideal player counts!") );
}

enum ECountRequired
{
    CR_None,
    CR_One,
    CR_Some
};

void CheckForClass ( UObject* Package, const TCHAR *ClassName, ECountRequired CountRequired )
{
    INT Count;

    Count = 0;

    UClass *Class = Cast<UClass>( UObject::StaticLoadObject( UClass::StaticClass(), NULL, ClassName, NULL, LOAD_NoWarn, NULL ) );

    if ( !Class )
    {
        GWarn->Logf( NAME_Error, TEXT("    Can't check for class %s!"), ClassName );
        return;
    }

	for( FObjectIterator It; It; ++It )
	{
        UObject *Obj = *It;

        if( !Obj->IsIn( Package ) )
            continue;

        if( !Obj->IsA( Class ) )
            continue;

        Count++;
    }

    switch (CountRequired)
    {
        case CR_None:
        {
            if( Count != 0)
                GWarn->Logf( NAME_Warning, TEXT("    %ss found!"), ClassName );
            break;
        }

        case CR_One:
        {
            if( Count == 0)
                GWarn->Logf( NAME_Warning, TEXT("    No %ss found!"), ClassName );
            if( Count > 1)
                GWarn->Logf( NAME_Warning, TEXT("    %d extra %ss found!"), Count - 1, ClassName );
            break;
        }

        case CR_Some:
        {
            if( Count == 0)
                GWarn->Logf( NAME_Warning, TEXT("    No %ss found!"), ClassName );
            break;
        }
    }
}

void CheckForMapErrors( UObject *Package )
{
	for( FObjectIterator It; It; ++It )
	{
        UObject *Obj = *It;

        if( !Obj->IsIn( Package ) )
            continue;

		if( Obj->IsA(UStaticMesh::StaticClass()) )
		    ((UStaticMesh*)Obj)->CheckForErrors();

        AActor *Actor = Cast<AActor>(Obj);

        if( !Actor )
            continue;

        Actor->CheckForErrors();

        switch( Actor->DrawType )
        {
            case DT_StaticMesh:
            {
                if( !Actor->StaticMesh )
                    GWarn->Logf( NAME_Warning, TEXT("    %s has no StaticMesh!"), Actor->GetName() );
                else if( Actor->Style == STY_Masked )
                    GWarn->Logf( NAME_Warning, TEXT("    %s is using a draw style of STY_Masked!"), Actor->GetName() );
                break;
            }
            case DT_Mesh:
            {
                if( !Actor->Mesh )
                    GWarn->Logf( NAME_Warning, TEXT("    %s has no Mesh!"), Actor->GetName() );
                break;
            }
            case DT_Sprite:
            {
                if( !Actor->Texture )
                    GWarn->Logf( NAME_Warning, TEXT("    %s has no Texture!"), Actor->GetName() );
                break;
            }
        }
    }
}

void AnalyzeMap( const TCHAR* MapName )
{
    FString IntName;
    UObject* Package;

    GWarn->Logf (NAME_Log, TEXT("Loading %s"), MapName);

	Package = UObject::LoadPackage( NULL, MapName, 0 );

    UObject::ResetLoaders( NULL, 0, 1 );

    if( !Package )
    {
        GWarn->Logf (NAME_Error, TEXT("Could not load %s"), MapName);
        return;
    }

	CheckStaticMeshVersions( Package );

    CheckForClass ( Package, TEXT("Engine.PlayerStart"), CR_Some );
    CheckForClass ( Package, TEXT("XGame.AttractCamera"), CR_Some );
    CheckForClass ( Package, TEXT("Engine.LevelSummary"), CR_One );

    CheckGameType( Package );

    CheckForMapErrors( Package );

    if( GEditor->ShowIntWarnings )
    {
        IntGetNameFromPackageName( MapName, IntName );

        if( GFileManager->FileSize (*IntName) < 0 )
            GWarn->Logf (NAME_Warning, TEXT("    %s does not have a matching INT file!"), MapName);
        else if( !IntMatchesPackage ( Package, *IntName ) )
            GWarn->Logf (NAME_Warning, TEXT("    %s is out of synch with its INT file!"), MapName);
    }
    
    AnalyzeTextureContent( Package );
    AnalyzeTextureUsage( Package );
    AnalyzeSoundUsage( Package );
    
    INT UntaggedZones = 0;
    
    for( TObjectIterator<AZoneInfo> It; It; ++It )
    {
        AZoneInfo* ZI = *It;
        
        if( ZI->GetClass()->IsChildOf( ALevelInfo::StaticClass() ) )
            continue;
            
        AZoneInfo* Default = (AZoneInfo* )(ZI->GetClass()->GetDefaultObject());

        if( !ZI->LocationName.Len() || ( ZI->LocationName == Default->LocationName ) )
            UntaggedZones++;
    }
    
    if( UntaggedZones )
        GWarn->Logf (NAME_Warning, TEXT("    %d zone%s with unspecified LocationNames found!"), UntaggedZones, UntaggedZones == 1 ? TEXT("") : TEXT("s"));
    
    INT UntaggedVolumes = 0;
    
    for( TObjectIterator<AVolume> It; It; ++It )
    {
        AVolume* V = *It;
        
        if( appStrPrefix( V->GetName(), TEXT("Default") ) == 0 )
            continue;

        AVolume* Default = (AVolume* )(V->GetClass()->GetDefaultObject());

        if( !V->LocationName.Len() || ( V->LocationName == Default->LocationName ) )
            UntaggedVolumes++;
    }
    
    if( UntaggedVolumes )
        GWarn->Logf (NAME_Warning, TEXT("    %d volume%s with unspecified LocationNames found!"), UntaggedVolumes, UntaggedVolumes == 1 ? TEXT("") : TEXT("s"));
}

void AnalyzeMaps()
{
    INT i;
    INT MapsChecked = 0;

	for( i=0; i < GSys->Paths.Num(); i++ )
	{
		FString PackageWildcard;
        TArray<FString> FilesInPath;
        FString PathPrefix;
        INT j;

		PackageWildcard = FString(appBaseDir()) + GSys->Paths(i);

        if( PackageWildcard.Right (appStrlen(TEXT ("*.ut2"))) != TEXT ("*.ut2"))
            continue;

		FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

        if( !FilesInPath.Num() )
            continue;

        Sort( &FilesInPath(0), FilesInPath.Num() );

        check( PackageWildcard.InStr(TEXT("*")) >= 0 );
        PathPrefix = PackageWildcard.Left( PackageWildcard.InStr( TEXT("*"), 1 ));

		for( j = 0; j < FilesInPath.Num(); j++ )
        {
            const FString &File = FilesInPath(j);

            if( appStrfind( *File, TEXT("Auto") ) == *File )
                continue;

            AnalyzeMap( *File );
            MapsChecked++;

            UObject::CollectGarbage(RF_Native);
        }

    }

    GWarn->Logf (NAME_Log, TEXT("%d maps checked."), MapsChecked);
}

bool PackageExistsInWildcard( const FString &PackageName, const FString &Wildcard )
{
    TArray<FString> FilesInPath;
    INT j;

    if( Wildcard.Right (appStrlen(TEXT("*.int"))) == TEXT("*.int"))
        return( false );

	FilesInPath = GFileManager->FindFiles( *Wildcard, 1, 0 );

	for( j = 0; j < FilesInPath.Num(); j++ )
    {
        if( PackagesMatch( PackageName, FilesInPath(j) ) )
            return( true );
    }

    return( false );
}

bool PackageExists( const FString &PackageName )
{
    INT i;

    if( PackageName == TEXT("Startup.int") ) // hack city!
        return( true );

    if( PackageName == TEXT("UC.int") ) // hack city!
        return( true );

    if( PackageName == TEXT("XMaps.int") ) // hack city!
        return( true );

    if( PackageName == TEXT("XDemoMaps.int") ) // hack city!
        return( true );

	for( i = 0; i < GSys->Paths.Num(); i++ )
	{
        if( PackageExistsInWildcard( PackageName, FString(appBaseDir()) + GSys->Paths(i) ) )
            return( true );
    }

    if( PackageExistsInWildcard( PackageName, FString(appBaseDir()) + TEXT("../System/*.dll") ) )
        return( true );

    if( PackageExistsInWildcard( PackageName, FString(appBaseDir()) + TEXT("../System/*.exe") ) )
        return( true );

    if( PackageExistsInWildcard( PackageName, FString(appBaseDir()) + TEXT("../System/*.ini") ) )
        return( true );

    return( false );
}

bool IsStandAloneInt( const TCHAR* FileName )
{
    static const TCHAR* StandAlones[] =
    {
        TEXT("XPlayers.int"),
        NULL
    };

    for( INT i = 0; StandAlones[i] != NULL; i++ )
    {
        if( appStricmp( FileName, StandAlones[i] ) == 0 )
            return( true );
    }

    return( false );
}

void FindOrphanedInts()
{
    TArray<FString> IntFiles;
    FString IntWildcard;
    INT i;
    
    IntWildcard = FString(appBaseDir()) + TEXT("*.int");
    IntFiles = GFileManager->FindFiles( *IntWildcard, 1, 0 );

	for( i = 0; i < IntFiles.Num(); i++ )
    {
        if( IsStandAloneInt( *IntFiles(i) ) )
            continue;

        if( PackageExists( IntFiles(i) ) )
            continue;
            
        GWarn->Logf( NAME_Warning, TEXT("%s has no matching package!"), *IntFiles(i) );
    }

    GWarn->Logf( NAME_Log, TEXT("%d files checked."), IntFiles.Num() );
}

/*-----------------------------------------------------------------------------
	UAnalyzeBuildCommandlet.
-----------------------------------------------------------------------------*/

class UAnalyzeBuildCommandlet : public UCommandlet
{
	DECLARE_CLASS(UAnalyzeBuildCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UAnalyzeBuildCommandlet::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 1;
		ShowErrorCount  = 1;

		unguard;
	}

	INT Main( const TCHAR *args )
	{
		guard(UAnalyzeBuildCommandlet::Main);

		UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
		GEditor->InitEditor();

		GIsRequestingExit = 1; // Causes ctrl-c to immediately exit.

        const TCHAR *ArgStart;

        for( ArgStart = args; *ArgStart != '\0'; ArgStart++)
        {
            if( !appIsSpace( *ArgStart ) )
                break;
        }
        
        if( ( appStrlen( args ) > 0 ) && ( *ArgStart != '-' ) )
        {
            AnalyzeMap( ArgStart );
        }
        else
        {
            if( !ParseParam( args, TEXT("noNames") ) )
            {
                GWarn->Log (NAME_Heading, TEXT("Analyzing Package Names"));
                CheckForConflictingPackageNames();
            }

            if( !ParseParam( args, TEXT("noUTX") ) )
            {
                GWarn->Log (NAME_Heading, TEXT("Analyzing Texture Packages") );
                CheckForConflictingNames (TEXT("*.utx"), (UTexture *)(NULL), &AnalyzeTexturePackage);
            }

            if( !ParseParam( args, TEXT("noUAX") ) )
            {
                GWarn->Log (NAME_Heading, TEXT("Analyzing Sound Packages"));
                CheckForConflictingNames (TEXT("*.uax"), (USound *)(NULL), &AnalyzeSoundPackage);
            }

            if( !ParseParam( args, TEXT("noUSX") ) )
            {
                GWarn->Log (NAME_Heading, TEXT("Analyzing Static Mesh Packages"));
                CheckForConflictingNames (TEXT("*.usx"), (UStaticMesh *)(NULL), &AnalyzeStaticMeshPackage);
            }

            if( !ParseParam( args, TEXT("noUC") ) )
            {
                GWarn->Log (NAME_Heading, TEXT("Analyzing Script Packages"));
                AnalyzeScripts();
                CollectGarbage(RF_Native);
            }

            if( !ParseParam( args, TEXT("noUT2") ) )
            {
                GWarn->Log (NAME_Heading, TEXT("Analyzing Maps"));
                AnalyzeMaps();
            }

            if( !ParseParam( args, TEXT("noINT") ) )
            {
                GWarn->Log (NAME_Heading, TEXT("Checking For Orphaned Ints"));
                FindOrphanedInts();
            }
        }

		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UAnalyzeBuildCommandlet)

