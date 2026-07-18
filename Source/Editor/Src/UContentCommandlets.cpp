/*=============================================================================
	UCContentCommandlets.cpp: Various commmandlets for content analysis
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel

=============================================================================*/

#include "EditorPrivate.h"
#include "UnLinker.h"

/*-----------------------------------------------------------------------------
	UCheckTexturesCommandlet
-----------------------------------------------------------------------------*/

class UCheckTextures : public UCommandlet
{
	DECLARE_CLASS(UCheckTextures,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UCheckTextures::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 0;
		ShowErrorCount  = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(UCheckTextures::Main);
		
		FString	PackageWildcard;

		UClass* TextureClass		= FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("Texture") );
		UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor						= ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
		GEditor->InitEditor();

		GLazyLoad = 0;

		QWORD BytesSaved = 0;

		while( ParseToken(Parms, PackageWildcard, 0) )
		{
			TArray<FString> FilesInPath;
			FString			PathPrefix;

			FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

			if( !FilesInPath.Num() )
				continue;

			for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
			{
				const FString &File = FilesInPath(FileIndex);
				UObject* Package;

				//GWarn->Logf(NAME_Log, TEXT("Loading %s"), *File);

				try
				{
					Package = UObject::LoadPackage( NULL, *File, 0 );
				}
				catch( ... )
				{
					Package = NULL;
				}

				if( !Package )
				{
					GWarn->Logf(NAME_Log, TEXT("    Error loading %s!"), *File);
					continue;
				}

				guard(LoadTextures);
				for( TObjectIterator<UObject> It; It; ++It )
				{
					if( It->IsA(TextureClass) && It->IsIn(Package) )
					{
						UTexture* Texture = CastChecked<UTexture>(*It);
						UBOOL	CorruptedTexture	= 0,
								NoAlphaChannel		= 1;
							
						DWORD	DXTBytesUsed		= 0;

						for( INT MipIndex=0; MipIndex<Texture->Mips.Num(); MipIndex++ )
						{
							FMipmap& Mipmap = Texture->Mips(MipIndex);
							if(!Texture->bParametric)
								Mipmap.DataArray.Load();
							
							if( &Mipmap.DataArray(0) == NULL )
							{
								CorruptedTexture = 1;
								GWarn->Logf(NAME_Log, TEXT("Texture [%s] miplevel %i is empty [out of %i]."),Texture->GetPathName(),MipIndex,Texture->Mips.Num()-1);
							}
							else
							{
								if( Texture->Format == TEXF_DXT3 )
								{
									INT		NumberOfBlocks	= Max(Mipmap.USize/4, 1) * Max(Mipmap.VSize/4, 1);								
									DWORD*	AlphaData		= (DWORD *) &Mipmap.DataArray(0);
									DWORD	Dummy			= 0xFFFFFFFF;

									DXTBytesUsed += NumberOfBlocks * 16;

									while( NumberOfBlocks-- && Dummy==0xFFFFFFFF )
									{	
										Dummy &= *AlphaData++;	// First two rows of alpha data
										Dummy &= *AlphaData;	// Last two rows of alpha data
										AlphaData += 3;			// Go to next translucent block (3*4 = 12 bytes away)
									}

									NoAlphaChannel &= (Dummy==0xFFFFFFFF);
								}
								else
								if( Texture->Format == TEXF_DXT5 )
								{
									INT		NumberOfBlocks	= Max(Mipmap.USize/4, 1) * Max(Mipmap.VSize/4, 1);								
									_WORD*	AlphaData		= (_WORD *) &Mipmap.DataArray(0);
									_WORD	Dummy			= 0xFFFF;

									DXTBytesUsed += NumberOfBlocks * 16;

									while( NumberOfBlocks-- && Dummy==0xFFFF )
									{	
										Dummy &= *AlphaData;	// Check two alpha values
										AlphaData += 8;			// Go to next translucent block (8*2 = 16 bytes away)
									}

									NoAlphaChannel &= (Dummy==0xFFFF);
								}
								else
									NoAlphaChannel = 0;
							}

							Mipmap.DataArray.Unload();
						}
						if( CorruptedTexture )
							GWarn->Logf(NAME_Log, TEXT("Texture [%s] needs to be re-imported"),Texture->GetPathName());
						if( NoAlphaChannel && (Texture->Format == TEXF_DXT5 || Texture->Format == TEXF_DXT3) )
						{
							GWarn->Logf(NAME_Log, TEXT("Texture [%s] could be DXT1 compressed, currently %s"),Texture->GetPathName(), Texture->Format == TEXF_DXT3 ? TEXT("DXT3") : TEXT("DXT5") );
							BytesSaved += DXTBytesUsed;
						}
					}
				}
				unguard;

				UObject::ResetLoaders( NULL, 0, 1 );
				UObject::CollectGarbage(RF_Native);
			}
		}
	
		GWarn->Logf(NAME_Log, TEXT("Transcoding would save %i MByte"), BytesSaved / 1024 / 1024 / 2 );

		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UCheckTextures)

/*-----------------------------------------------------------------------------
	ULoadPackageCommandlet
-----------------------------------------------------------------------------*/

class ULoadPackage : public UCommandlet
{
	DECLARE_CLASS(ULoadPackage,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(ULoadPackage::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 0;
		ShowErrorCount  = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(ULoadPackage::Main);
		
		FString	PackageWildcard;

		UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor						= ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
		GEditor->InitEditor();

		GLazyLoad = 0;

		while( ParseToken(Parms, PackageWildcard, 0) )
		{
			TArray<FString> FilesInPath;
			FString			PathPrefix;

			FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

			if( !FilesInPath.Num() )
				continue;

			for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
			{
				const FString &File = FilesInPath(FileIndex);
				UObject* Package;

				GWarn->Logf(NAME_Log, TEXT("Loading %s"), *File);

				try
				{
					Package = UObject::LoadPackage( NULL, *File, 0 );
				}
				catch( ... )
				{
					Package = NULL;
				}

				if( !Package )
				{
					GWarn->Logf(NAME_Log, TEXT("Error loading %s!"), *File);
					continue;
				}

				UObject::ResetLoaders( NULL, 0, 1 );
				UObject::CollectGarbage(RF_Native);
			}
		}
	
		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(ULoadPackage)


/*-----------------------------------------------------------------------------
	UStaticMeshConvert
-----------------------------------------------------------------------------*/

class UStaticMeshConvert : public UCommandlet
{
	DECLARE_CLASS(UStaticMeshConvert,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UStaticMeshConvert::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 0;
		ShowErrorCount  = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(UStaticMeshConvert::Main);
		
		FString	PackageWildcard;

		UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor						= ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
		GEditor->InitEditor();

		GLazyLoad = 0;

		while( ParseToken(Parms, PackageWildcard, 0) )
		{
			TArray<FString> FilesInPath;
			FString			PathPrefix;

			FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

			if( !FilesInPath.Num() )
				continue;

			for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
			{
				const FString &File = FilesInPath(FileIndex);
				UObject* Package;

				GWarn->Logf(NAME_Log, TEXT("Loading %s"), *File);

				try
				{
					try
					{
						Package = UObject::LoadPackage( NULL, *File, LOAD_NoWarn );
					}
					catch( ... )
					{
						Package = NULL;
					}

					if( !Package )
					{
						GWarn->Logf(NAME_Log, TEXT("Error loading %s!"), *File);
						continue;
					}
#if 0
					UClass* StaticMeshClass		= FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("StaticMesh") );
					guard(LoadStaticMeshes);
					for( TObjectIterator<UObject> It; It; ++It )
					{
						if( It->IsA(StaticMeshClass) && It->IsIn(Package) )
						{
							UStaticMesh* StaticMesh = CastChecked<UStaticMesh>(*It);
							StaticMesh->Build( 1 );
						}
					}
					unguard;
					UObject::SavePackage( Package, NULL, RF_Standalone, *File, GWarn );
#else
					ULevel* Level = FindObject<ULevel>( Package, TEXT("MyLevel") );
					if( Level )
						UObject::SavePackage( Package, Level, 0, *File, GWarn );
					else
						GWarn->Logf(NAME_Log, TEXT("Error loading %s! (no ULevel)"), *File);
#endif

				}
				catch( ... )
				{
					GWarn->Logf(NAME_Log, TEXT("Error loading %s!"), *File);
				}

				UObject::ResetLoaders( NULL, 0, 1 );
				UObject::CollectGarbage(RF_Native);
			}
		}
	
		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UStaticMeshConvert)


/*-----------------------------------------------------------------------------
	UCheckStaticMeshesCommandlet
-----------------------------------------------------------------------------*/

class UCheckStaticMeshes : public UCommandlet
{
	DECLARE_CLASS(UCheckStaticMeshes,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UStaticMeshesTextures::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 0;
		ShowErrorCount  = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(UCheckStaticMeshes::Main);
		
		FString	PackageWildcard;

		UClass* StaticMeshClass		= FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("StaticMesh") );
		UClass* StaticMeshActorClass= FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("StaticMeshActor") );
		UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor						= ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
		GEditor->InitEditor();

		GLazyLoad = 0;

		while( ParseToken(Parms, PackageWildcard, 0) )
		{
			TArray<FString> FilesInPath;
			FString			PathPrefix;

			FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

			if( !FilesInPath.Num() )
				continue;

			for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
			{
				const FString &File = FilesInPath(FileIndex);
				UObject* Package;

				//GWarn->Logf(NAME_Log, TEXT("Loading %s"), *File);

				try
				{
					Package = UObject::LoadPackage( NULL, *File, LOAD_NoWarn );

					if( !Package )
					{
						GWarn->Logf(NAME_Log, TEXT("    Error loading %s!"), *File);
						UObject::ResetLoaders( NULL, 0, 1 );
						UObject::CollectGarbage(RF_Native);
						continue;
					}

					guard(LoadStaticMeshes);
					for( TObjectIterator<UObject> It; It; ++It )
					{
						if( It->IsA(StaticMeshClass) && It->IsIn(Package) )
						{
							UStaticMesh* StaticMesh = CastChecked<UStaticMesh>(*It);

							StaticMesh->RawTriangles.Load();
							if( StaticMesh->RawTriangles.Num() == 0 )
								GWarn->Logf(NAME_Log, TEXT("[%s] is corrupt and needs to be re-imported"),StaticMesh->GetPathName());
							StaticMesh->RawTriangles.Unload();
						}
					}
					unguard;

					guard(LoadStaticMeshActors);
					UBOOL RebuildLighting = 0;
					for( TObjectIterator<UObject> It; It; ++It )
					{
						if( It->IsA(StaticMeshActorClass) && It->IsIn(Package) )
						{
							AStaticMeshActor* StaticMeshActor = CastChecked<AStaticMeshActor>(*It);

							if( !StaticMeshActor->bHidden && StaticMeshActor->StaticMeshInstance && StaticMeshActor->StaticMesh )
							{
								UBOOL ReportMesh = 0;

								if( StaticMeshActor->StaticMeshInstance->ColorStream.Colors.Num() != StaticMeshActor->StaticMesh->VertexStream.Vertices.Num() )
									ReportMesh = 1;

								for( INT LightIndex=0; LightIndex<StaticMeshActor->StaticMeshInstance->Lights.Num(); LightIndex++ )
								{				
									if( StaticMeshActor->StaticMeshInstance->Lights(LightIndex).VisibilityBits.Num() != (StaticMeshActor->StaticMesh->VertexStream.Vertices.Num() + 7) / 8 )
										ReportMesh = 1;

									FStaticMeshLightInfo*	LightInfo	= &StaticMeshActor->StaticMeshInstance->Lights(LightIndex);
									FDynamicLight*			Light		= LightInfo->LightActor->GetLightRenderData();
									if((!Light || Light->Dynamic || Light->Changed) == LightInfo->Applied)
										ReportMesh = 1;
								}
							
								if( ReportMesh )
								{
									//GWarn->Logf(NAME_Log, TEXT("[%s] [%s]"),StaticMeshActor->GetPathName(), StaticMeshActor->StaticMesh->GetPathName());
									RebuildLighting = 1;
								}
							}
						}
					}
					if( RebuildLighting )
						GWarn->Logf(NAME_Log, TEXT("\"%s\" needs to have lighting rebuild."),Package->GetPathName());
					unguard;
				}
				catch( ... )
				{
					GWarn->Logf(NAME_Log, TEXT("    Error loading %s!"), *File);
				}

				UObject::ResetLoaders( NULL, 0, 1 );
				UObject::CollectGarbage(RF_Native);
			}
		}
	
		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UCheckStaticMeshes)

/*-----------------------------------------------------------------------------
	UCheckEmitters
-----------------------------------------------------------------------------*/

class UCheckEmitters : public UCommandlet
{
	DECLARE_CLASS(UCheckEmitters,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UCheckEmitters::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 0;
		ShowErrorCount  = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(UCheckEmitters::Main);
		
		FString	PackageWildcard;

		UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor						= ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
		GEditor->InitEditor();

		GLazyLoad = 0;

		while( ParseToken(Parms, PackageWildcard, 0) )
		{
			TArray<FString> FilesInPath;
			FString			PathPrefix;

			FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

			if( !FilesInPath.Num() )
				continue;

			for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
			{
				const FString &File = FilesInPath(FileIndex);
				UObject* Package;

				try
				{
					Package = UObject::LoadPackage( NULL, *File, 0 );
				}
				catch( ... )
				{
					Package = NULL;
				}

				UClass* NetworkEmitterClass	= FindObject<UClass>( ANY_PACKAGE, TEXT("NetworkEmitter") );

				if( Package && NetworkEmitterClass )
				{
					for( TObjectIterator<UObject> It; It; ++It )
					{
						if( It->IsA(NetworkEmitterClass) && It->IsIn(Package) )
						{
							if( It->GetName()[0] == 'E' )
								GWarn->Logf(NAME_Log,(TEXT("%s"),It->GetPathName()));
						}
					}
				}

				UObject::ResetLoaders( NULL, 0, 1 );
				UObject::CollectGarbage(RF_Native);
			}
		}
	
		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UCheckEmitters)


/*-----------------------------------------------------------------------------
	UListExports
-----------------------------------------------------------------------------*/
struct FExportInfo
{
	FName	Name;
	INT		Size;
};
static inline INT Compare( FExportInfo& A, FExportInfo& B )
{
	return B.Size - A.Size;
}

class UListExports : public UCommandlet
{
	DECLARE_CLASS(UListExports,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UListExports::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 0;
		ShowErrorCount  = 1;

		unguard;
	}

	INT Main( const TCHAR* Parms )
	{
		guard(UListExports::Main);
		
		FString	PackageWildcard;

		UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor						= ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
		GEditor->InitEditor();

		GLazyLoad = 0;

		while( ParseToken(Parms, PackageWildcard, 0) )
		{
			TArray<FString> FilesInPath;
			FString			PathPrefix;

			FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

			if( !FilesInPath.Num() )
				continue;

			for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
			{
				const FString &Filename = FilesInPath(FileIndex);

				ULinkerLoad* Linker = NULL;

				try
				{
					UObject::BeginLoad();
					Linker = UObject::GetPackageLinker( NULL, *Filename, LOAD_Throw, NULL, NULL );
					UObject::EndLoad();
				}
				catch( ... )
				{
					Linker = NULL;
				}

				if( Linker )
				{
					TArray<FExportInfo> Exports;

					for( INT i=0; i<Linker->ExportMap.Num(); i++ )
					{
						FObjectExport& Export = Linker->ExportMap(i);
						INT Index = Exports.Add(1);
						Exports(Index).Name = Export.ObjectName;
						Exports(Index).Size = Export.SerialSize;
					}
					
					if( Exports.Num() )
					{
						Sort( &Exports(0), Exports.Num() );
						for( INT i=0; i<Exports.Num(); i++ )
							GWarn->Logf(TEXT("%i %s"),Exports(i).Size,*Exports(i).Name);
					}
				}
	
				UObject::ResetLoaders( NULL, 0, 1 );
				UObject::CollectGarbage(RF_Native);
			}
		}
	
		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UListExports)


/*-----------------------------------------------------------------------------
	UMapKillZ
-----------------------------------------------------------------------------*/

class UMapKillZ : public UCommandlet
{
	DECLARE_CLASS(UMapKillZ,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UMapKillZ::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 0;
		ShowErrorCount  = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(UMapKillZ::Main);
		
		FString	PackageWildcard;

		UClass* ZoneInfoClass		= FindObjectChecked<UClass>( ANY_PACKAGE, TEXT("ZoneInfo") );
		UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor						= ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
		GEditor->InitEditor();

		GLazyLoad = 0;

		while( ParseToken(Parms, PackageWildcard, 0) )
		{
			TArray<FString> FilesInPath;
			FString			PathPrefix;

			FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

			if( !FilesInPath.Num() )
				continue;

			for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
			{
				const FString &File = FilesInPath(FileIndex);
				UObject* Package;

				GWarn->Logf(NAME_Log, TEXT("Loading %s"), *File);

				Package = UObject::LoadPackage( NULL, *File, 0 );

				if( !Package )
				{
					GWarn->Logf(NAME_Log, TEXT("    Error loading %s!"), *File);
					continue;
				}

				for( TObjectIterator<UObject> It; It; ++It )
				{
					if( It->IsA(ZoneInfoClass) && It->IsIn(Package) )
					{
						AZoneInfo* ZoneInfo = CastChecked<AZoneInfo>(*It);
						
						if( ZoneInfo->bTerrainZone && Abs( ZoneInfo->KillZ - (-10000.0f) ) < 1.f )
							GWarn->Logf(NAME_Log, TEXT("%s"), ZoneInfo->GetPathName() );
					}
				}

				UObject::ResetLoaders( NULL, 0, 1 );
				UObject::CollectGarbage(RF_Native);
			}
		}
	
		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UMapKillZ)


/*-----------------------------------------------------------------------------
	USoundConvert
-----------------------------------------------------------------------------*/

class USoundConvert : public UCommandlet
{
	DECLARE_CLASS(USoundConvert,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(USoundConvert::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 0;
		ShowErrorCount  = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(USoundConvert::Main);
		
		FString	PackageWildcard;

		UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor						= ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
		GEditor->InitEditor();

		GLazyLoad = 0;

		while( ParseToken(Parms, PackageWildcard, 0) )
		{
			TArray<FString> FilesInPath;
			FString			PathPrefix;

			FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

			if( !FilesInPath.Num() )
				continue;

			for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
			{
				const FString &File = FilesInPath(FileIndex);
				UObject* Package;

				GWarn->Logf(NAME_Log, TEXT("Loading %s"), *File);

				try
				{
					try
					{
						Package = UObject::LoadPackage( NULL, *File, LOAD_NoWarn );
					}
					catch( ... )
					{
						Package = NULL;
					}

					if( !Package )
					{
						GWarn->Logf(NAME_Log, TEXT("Error loading %s!"), *File);
						continue;
					}

					ULevel* Level = FindObject<ULevel>( Package, TEXT("MyLevel") );
					if( Level )
					{
						UBOOL RequiresConversion = 0;

						for( TObjectIterator<UObject> It; It; ++It )
						{
							AActor* Actor = Cast<AActor>(*It);
							if( Actor && Actor->IsIn(Package) )
							{
								if( Actor->bFullVolume || (Actor->SoundVolume > (255 * 0.6f)) ) 
									RequiresConversion = 1;
							}
						}

						if( RequiresConversion )
						{
							for( TObjectIterator<UObject> It; It; ++It )
							{
								AActor* Actor = Cast<AActor>(*It);
								if( Actor && Actor->IsIn(Package) )
								{
									Actor->bFullVolume = 0;
									Actor->SoundVolume *= 0.6;
								}
							}
							UObject::SavePackage( Package, Level, 0, *File, GWarn );
						}
					}
					else
						GWarn->Logf(NAME_Log, TEXT("Error loading %s! (no ULevel)"), *File);

				}
				catch( ... )
				{
					GWarn->Logf(NAME_Log, TEXT("Error loading %s!"), *File);
				}

				UObject::ResetLoaders( NULL, 0, 1 );
				UObject::CollectGarbage(RF_Native);
			}
		}
	
		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(USoundConvert)


/*-----------------------------------------------------------------------------
	UCompressTextures
-----------------------------------------------------------------------------*/

class UCompressTextures : public UCommandlet
{
	DECLARE_CLASS(UCompressTextures,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UCompressTextures::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 0;
		ShowErrorCount  = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(UCompressTextures::Main);
		
		FString	PackageWildcard;

		UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor						= ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
		GEditor->InitEditor();

		GLazyLoad = 0;

		while( ParseToken(Parms, PackageWildcard, 0) )
		{
			TArray<FString> FilesInPath;
			FString			PathPrefix;

			FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

			if( !FilesInPath.Num() )
				continue;

			for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
			{
				const FString &File = FilesInPath(FileIndex);
				UObject* Package;

				GWarn->Logf(NAME_Log, TEXT("Loading %s"), *File);

				try
				{
					try
					{
						Package = UObject::LoadPackage( NULL, *File, LOAD_NoWarn );
					}
					catch( ... )
					{
						Package = NULL;
					}

					if( !Package )
					{
						GWarn->Logf(NAME_Log, TEXT("Error loading %s!"), *File);
						continue;
					}

					for( TObjectIterator<UObject> It; It; ++It )
					{
						if( It->IsA(UTexture::StaticClass()) && It->IsIn(Package) )
						{
							UTexture* Texture = CastChecked<UTexture>(*It);
							Texture->Compress( TEXF_DXT5, 1, NULL );
						}
					}

#if 1
					UObject::SavePackage( Package, NULL, RF_Standalone, *File, GWarn );
#else
					ULevel* Level = FindObject<ULevel>( Package, TEXT("MyLevel") );
					if( Level )
						UObject::SavePackage( Package, Level, 0, *File, GWarn );
					else
						GWarn->Logf(NAME_Log, TEXT("Error loading %s! (no ULevel)"), *File);
#endif

				}
				catch( ... )
				{
					GWarn->Logf(NAME_Log, TEXT("Error loading %s!"), *File);
				}

				UObject::ResetLoaders( NULL, 0, 1 );
				UObject::CollectGarbage(RF_Native);
			}
		}
	
		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UCompressTextures)


/*-----------------------------------------------------------------------------
	UBatchingSummary
-----------------------------------------------------------------------------*/

class UBatchingSummary : public UCommandlet
{
	DECLARE_CLASS(UBatchingSummary,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UBatchingSummary::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 0;
		ShowErrorCount  = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(UBatchingSummary::Main);
		
		FString	PackageWildcard;

		UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor						= ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
		GEditor->InitEditor();

		GLazyLoad = 0;

		while( ParseToken(Parms, PackageWildcard, 0) )
		{
			TArray<FString> FilesInPath;
			FString			PathPrefix;

			FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

			if( !FilesInPath.Num() )
				continue;

			for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
			{
				const FString &File = FilesInPath(FileIndex);
				UObject* Package;

				try
				{
					Package = UObject::LoadPackage( NULL, *File, 0 );
				}
				catch( ... )
				{
					Package = NULL;
				}

				if( !Package )
				{
					GWarn->Logf(NAME_Log, TEXT("    Error loading %s!"), *File);
					continue;
				}

				ULevel* Level = FindObject<ULevel>( Package, TEXT("MyLevel") );
				if( !Level )
					continue;
		
				INT VertexBytes	= 0,
				IndexBytes	= 0;

				for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
				{
					AActor*	Actor = Level->Actors(ActorIndex);

					if(Actor && Actor->bStatic && !Actor->bHidden && Actor->DrawType == DT_StaticMesh && Actor->StaticMesh && !Actor->Projectors.Num() && !Actor->UV2Texture )
					{
						for(INT SectionIndex = 0;SectionIndex < Actor->StaticMesh->Sections.Num();SectionIndex++)
						{
							FStaticMeshSection&	Section				= Actor->StaticMesh->Sections(SectionIndex);
							UMaterial*			Material			= Actor->StaticMesh->GetSkin(Actor,SectionIndex);
							BYTE				RequiredUVStreams	= Material->RequiredUVStreams(),
												UVStreamMask		= 0xFF;
							INT					NumUVs				= 0,
												Stride;
							
							for(INT UVIndex = 0;UVIndex < 8;UVIndex++,UVStreamMask <<= 1)
							{
								if(!(RequiredUVStreams & UVStreamMask))
								{
									NumUVs = UVIndex;
									break;
								}
							}

							Stride		 = sizeof(FStaticMeshBatchNormalVertex) + sizeof(FLOAT) * 2 * NumUVs;
							VertexBytes += (Section.MaxVertexIndex - Section.MinVertexIndex + 1) * Stride;
							IndexBytes  += Section.NumPrimitives * 3 * sizeof(_WORD);
						}
					}
				}

				GWarn->Logf(NAME_Log, TEXT("%3.1f MByte used for batching by %s"), (VertexBytes + IndexBytes) / 1024.f / 1024.f, *File);

				UObject::ResetLoaders( NULL, 0, 0 );
				UObject::CollectGarbage(RF_Native);
			}
		}
	
		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UBatchingSummary)


/*-----------------------------------------------------------------------------
	UCheckSounds
-----------------------------------------------------------------------------*/

class UCheckSounds : public UCommandlet
{
	DECLARE_CLASS(UCheckSounds,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UCheckSounds::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 0;
		ShowErrorCount  = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(UCheckSounds::Main);
		
		FString	PackageWildcard;

		UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor						= ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
		GEditor->InitEditor();

		GLazyLoad = 0;

		while( ParseToken(Parms, PackageWildcard, 0) )
		{
			TArray<FString> FilesInPath;
			FString			PathPrefix;

			FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );

			if( !FilesInPath.Num() )
				continue;

			for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
			{
				const FString &File = FilesInPath(FileIndex);
				UObject* Package;

				try
				{
					Package = UObject::LoadPackage( NULL, *File, 0 );
				}
				catch( ... )
				{
					Package = NULL;
				}

				
				for( TObjectIterator<UObject> It; It; ++It )
				{
					if( It->IsA(USound::StaticClass()) && It->IsIn(Package) )
					{
						USound* Sound = CastChecked<USound>(*It);
						if( Sound->IsValid() )
						{
							Sound->GetData().Load();
							FWaveModInfo WaveInfo;
							
							if( Sound->GetData().Num() == 0 || !WaveInfo.ReadWaveInfo(Sound->GetData()) )
//								GWarn->Logf( TEXT("BADBAD %s"), Sound->GetPathName() );
//							else
								GWarn->Logf( TEXT("       %s"), Sound->GetPathName() );
							Sound->GetData().Unload();						
						}
					}
				}

				if( !Package )
				{
					GWarn->Logf(NAME_Log, TEXT("    Error loading %s!"), *File);
					continue;
				}

				UObject::ResetLoaders( NULL, 0, 0 );
				UObject::CollectGarbage(RF_Native);
			}
		}
	
		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UCheckSounds)

#if __STATIC_LINK
void autoInitializeRegistrantsEditorContentCommandlets(INT &Lookup)
{
	UCheckSounds::StaticClass();
	UBatchingSummary::StaticClass();
	UCompressTextures::StaticClass();
	USoundConvert::StaticClass();
	UMapKillZ::StaticClass();
	UListExports::StaticClass();
	UCheckEmitters::StaticClass();
	UCheckStaticMeshes::StaticClass();
	UStaticMeshConvert::StaticClass();
	ULoadPackage::StaticClass();
	UCheckTextures::StaticClass();
}
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

