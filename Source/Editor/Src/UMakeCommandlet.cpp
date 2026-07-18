/*=============================================================================
	UMakeCommandlet.cpp: UnrealEd script recompiler.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	UMakeCommandlet.
-----------------------------------------------------------------------------*/

class UMakeCommandlet : public UCommandlet
{
	DECLARE_CLASS(UMakeCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(StaticConstructor::StaticConstructor);

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
		guard(UMakeCommandlet::Main);

		#if !WIN32
			GWarn->Log( TEXT("'ucc make' is broken on Unix/Mac right now.") );
			GIsRequestingExit=1;
			return 0;
		#endif

		// Create the editor class.
		UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
		GEditor->InitEditor();
		GIsRequestingExit = 1; // Causes ctrl-c to immediately exit.
        GTransientNaming = 1; // sjs
		GUglyHackFlags  |= 64; // rjp

		FString ClassesPath;

		// Load classes for editing.
		UClassFactoryUC* ClassFactory = new UClassFactoryUC;
		TArray<FString> CompiledPackages;

		for( INT i=0; i<GEditor->EditPackages.Num(); i++ )
		{
			// Try to load class.
			const TCHAR* Pkg = *GEditor->EditPackages( i );
			FString Filename = FString(Pkg) + TEXT(".u");
			GWarn->Log( NAME_Heading, FString::Printf(TEXT("%s - %s"),Pkg,ParseParam(appCmdLine(), TEXT("DEBUG"))? TEXT("Debug") : TEXT("Release"))); //DEBUGGER
			if( !LoadPackage( NULL, *Filename, LOAD_NoWarn ) )
			{
				// Create package.
				GWarn->Log( TEXT("Analyzing...") );
				UPackage* PkgObject = CreatePackage( NULL, Pkg );

				// Try reading from package's .ini file.
				PkgObject->PackageFlags &= ~(PKG_AllowDownload|PKG_ClientOptional|PKG_ServerSideOnly);
				FString IniName = FString(TEXT("..")) * Pkg * TEXT("Classes") * Pkg + TEXT(".upkg");
				UBOOL B=0;
				if( GConfig->GetBool(TEXT("Flags"), TEXT("AllowDownload"), B, *IniName) && B )
					PkgObject->PackageFlags |= PKG_AllowDownload;
				if( GConfig->GetBool(TEXT("Flags"), TEXT("ClientOptional"), B, *IniName) && B )
					PkgObject->PackageFlags |= PKG_ClientOptional;
				if( GConfig->GetBool(TEXT("Flags"), TEXT("ServerSideOnly"), B, *IniName) && B )
					PkgObject->PackageFlags |= PKG_ServerSideOnly;
				if( GConfig->GetBool(TEXT("Flags"), TEXT("Official"), B, *IniName) && B )
					PkgObject->PackageFlags |= PKG_Official;
 
				// Rebuild the class from its directory.
				ClassesPath = FString(TEXT("..")) * Pkg * TEXT("Classes");
				FString Spec = ClassesPath * TEXT("*.uc");
				TArray<FString> Files = GFileManager->FindFiles( *Spec, 1, 0 );
				UBOOL ModPackage=false;
				if ( Files.Num() == 0 )
				{
					ClassesPath = FString(GModPath) * Pkg * TEXT("Classes");
					Spec =  ClassesPath * TEXT("*.uc");
					Files = GFileManager->FindFiles( *Spec, 1, 0 );
					if( Files.Num() == 0 )
						appErrorf( TEXT("Can't find files matching %s"), *Spec );

					ModPackage=true;
				}
				for( INT i=0; i<Files.Num(); i++ )
				{
					// Import class.
					FString Filename  = ClassesPath * Files(i);
					FString ClassName = Files(i).LeftChop(3);
					ImportObject<UClass>( GEditor->Level, PkgObject, *ClassName, RF_Public|RF_Standalone, *Filename, NULL, ClassFactory );
				}

				// Verify that all script declared superclasses exist.
				for( TObjectIterator<UClass> ItC; ItC; ++ItC )
					if( ItC->ScriptText && ItC->GetSuperClass() )
						if( !ItC->GetSuperClass()->ScriptText )
							appErrorf( TEXT("Superclass %s of class %s not found"), ItC->GetSuperClass()->GetName(), ItC->GetName() );

				// Bootstrap-recompile changed scripts.
				GEditor->Bootstrapping = 1;
				UBOOL Success = GEditor->MakeScripts( NULL, GWarn, 0, 1, 1 );
				GEditor->Bootstrapping = 0;

                if( !Success )
                {
                    GWarn->Logf ( TEXT("Compile aborted due to errors.") );
                    break;
                }

				// Tag native classes in this package for export.
				INT ClassCount=0;
				for( INT i=0; i<FName::GetMaxNames(); i++ )
					if( FName::GetEntry(i) )
						FName::GetEntry(i)->Flags &= ~RF_TagExp;
				for( TObjectIterator<UClass> It; It; ++It )
					It->ClearFlags( RF_TagImp | RF_TagExp );
				for( TObjectIterator<UClass> It=TObjectIterator<UClass>(); It; ++It )
					if( It->GetOuter()==PkgObject && It->ScriptText && (It->GetFlags()&RF_Native) && !(It->ClassFlags&CLASS_NoExport) )
						ClassCount++, It->SetFlags( RF_TagExp );

				// Export the C++ header.
				if( ClassCount && !ParseParam(appCmdLine(),TEXT("NOH")))
				{
					if (ModPackage)
					{
						Filename = FString(GModPath) * Pkg * TEXT("Inc") * Pkg + TEXT("Classes.h");
					}
					else
					{
						Filename = FString(TEXT("..")) * Pkg * TEXT("Inc") * Pkg + TEXT("Classes.h");
					}

//					Filename = FString(TEXT("..")) * Pkg * TEXT("Inc") * Pkg + TEXT("Classes.h");

					debugf( TEXT("Autogenerating C++ header: %s"), *Filename );
					if( !UExporter::ExportToFile( UObject::StaticClass(), NULL, *Filename, 1, 1 ) )
						break; // gam
				}

				// Save package.
				ULinkerLoad* Conform = NULL;
				
				if( !ParseParam(appCmdLine(),TEXT("NOCONFORM")) )
				{
					BeginLoad();
					Conform = UObject::GetPackageLinker( CreatePackage(NULL,*(US+Pkg+TEXT("_OLD"))), *(FString(TEXT("..")) * TEXT("GUIRes") * Pkg + TEXT(".u")), LOAD_Quiet|LOAD_NoWarn|LOAD_NoVerify, NULL, NULL );
					EndLoad();
					if( Conform )
						debugf( TEXT("Conforming: %s"), Pkg );
				}

				FStringOutputDevice PackageName;
				if ( ModPackage )
					PackageName.Logf(TEXT("%s%s%s%s"), GModPath, PATH_SEPARATOR, TEXT("System"), PATH_SEPARATOR);

				PackageName.Logf(TEXT("%s.u"), Pkg);
				if ( SavePackage(PkgObject,NULL,RF_Standalone,*PackageName,GError,Conform) )
					new(CompiledPackages) FString(PackageName);
			}
		}

		// Attempt to export .ucl information for any packages which were compiled
		// TODO -
		// Currently, if compiling multiple packages, and the compilation failed, no packages will be
		// exported to .ucl.  When the compile finally succeeds, only the packages that were compiled during the final
		// run will be exported....I think I could fix this by adding try {} catch () around the above block of code
		if ( CompiledPackages.Num() )
			ExportCachePackage(CompiledPackages);

		GIsRequestingExit=1;
		return 0;

		unguard;
	}
};
IMPLEMENT_CLASS(UMakeCommandlet)

#if __STATIC_LINK
void autoInitializeRegistrantsEditorMake(INT &Lookup)
{
	UMakeCommandlet::StaticClass();
}
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

