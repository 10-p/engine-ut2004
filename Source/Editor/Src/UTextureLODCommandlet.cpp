/*=============================================================================
//	UTextureLODCommandlet.cpp: Batch adjusts NormalLOD and LODSet on Textures.
//	Copyright 2001 Digital Extremes. All Rights Reserved. Confidential.
=============================================================================*/

#include "EditorPrivate.h"


/*-----------------------------------------------------------------------------
	UTextureLODCommandlet.
-----------------------------------------------------------------------------*/

class UTextureLODCommandlet : public UCommandlet
{
	DECLARE_CLASS(UTextureLODCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UTextureLODCommandlet::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 0;
		ShowErrorCount  = 1;

		unguard;
	}

	INT Main( const TCHAR *args )
	{
		guard(UTextureLODCommandlet::Main);

		UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
		GEditor->InitEditor();

		GIsRequestingExit = 1; // Causes ctrl-c to immediately exit.

        FString PackageName;
        FString GroupName;
        INT Size;
        INT NormalLOD;
        BYTE LODSet;

        bool MatchSize = false;
        bool AutomaticNormalLOD = true;
        bool SetLODSet = false;

        UPackage* Package;
        UPackage* Group;

        if( !Parse( args, TEXT("Package="), PackageName ) )
        {
            GWarn->Log (NAME_Error, TEXT("No package specified."));
            return 0;
        }

        Parse( args, TEXT("Group="), GroupName );

        if( Parse( args, TEXT("Size="), Size ) )
            MatchSize = true;

        if( Parse( args, TEXT("NormalLOD="), NormalLOD ) )
            AutomaticNormalLOD = false;

        if( Parse( args, TEXT("LODSet="), LODSet ) )
            SetLODSet = true;

		Package = CastChecked<UPackage>( LoadPackage( NULL, *PackageName, 0 ) );

        ResetLoaders( NULL, 0, 1 );

        if( !Package )
        {
            GWarn->Logf (NAME_Error, TEXT("Could not load %s"), *PackageName);
            return 0;
        }

        if( GroupName.Len() == 0 )
            Group = Package;
        else
        {
			Group = FindObject<UPackage>( Package, *GroupName );
            
            if( !Group )
            {
                GWarn->Logf (NAME_Error, TEXT("Could not find group %s"), *GroupName);
                return 0;
            }
        }

        for( TObjectIterator<UTexture> It; It; ++It )
        {
            UTexture* Texture = CastChecked<UTexture>(*It);
            bool Modified = false;

		    if( !Texture->IsIn(Group) )
			    continue;

            if( MatchSize && (Texture->USize != Size) )
                continue;

            if( !AutomaticNormalLOD )
            {
                if( Texture->NormalLOD != NormalLOD )
                {
                    Texture->NormalLOD = NormalLOD;
                    Modified = true;
                }
            }
            else
            {
                if( Texture->USize == 1024 )
                {
                    if( Texture->NormalLOD != 1 )
                    {
                        Texture->NormalLOD = 1;
                        Modified = true;
                    }
                }
                else
                {
                    if( Texture->NormalLOD != 0 )
                    {
                        Texture->NormalLOD = 0;
                        Modified = true;
                    }
                }
            }

            if( SetLODSet )
            {
                if( Texture->LODSet != LODSet )
                {
                    Texture->LODSet = LODSet;
                    Modified = true;
                }
            }

            if( Modified )                
                GWarn->Logf (NAME_Log, TEXT("Updating %s"), Texture->GetName());
               
        }

		SavePackage( Package, NULL, RF_Standalone, *PackageName, NULL );

		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UTextureLODCommandlet)

#if __STATIC_LINK
void autoInitializeRegistrantsEditorTexLOD(INT &Lookup)
{
	UTextureLODCommandlet::StaticClass();
}
#endif

