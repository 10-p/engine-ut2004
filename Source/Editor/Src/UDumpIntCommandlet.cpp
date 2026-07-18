/*=============================================================================
//	UDumpIntCommandlet.cpp: Imports/Merges/Exports INTs for specified packages.
//
//	Copyright 2001 Digital Extremes. All Rights Reserved.
//  Confidential.
=============================================================================*/

#include "EditorPrivate.h"
#include "../../Core/Inc/FConfigCacheIni.h"

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
	UDumpIntCommandlet.
-----------------------------------------------------------------------------*/

class UDumpIntCommandlet : public UCommandlet
{
	DECLARE_CLASS(UDumpIntCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UDumpIntCommandlet::StaticConstructor);

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
		guard(UDumpIntCommandlet::Main);

		GUglyHackFlags |= 64;
    	FString PackageWildcard;
        INT Count = 0;

		UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
        GEditor->InitEditor();
		GIsRequestingExit = 1; // Causes ctrl-c to immediately exit.

		TArray<FString> Files;
		UBOOL AutoCheckout(0);

        while( ParseToken(Parms, PackageWildcard, 0) )
        {
			if (PackageWildcard == TEXT("-auto"))
			{
				AutoCheckout = 1;
				continue;
			}

			if ( PackageWildcard.InStr(TEXT("-mod="))>=0 )
				continue;

            TArray<FString> FilesInPath;
            FString PathPrefix(GetDirName( PackageWildcard ));
			UBOOL bFound(0);

			if ( PathPrefix != TEXT("") )
			{
				FilesInPath = GFileManager->FindFiles( *PackageWildcard, 1, 0 );
				for ( INT i = 0; i < FilesInPath.Num(); i++ )
					new(Files) FString( PathPrefix + FilesInPath(i) );

				bFound = bFound || FilesInPath.Num() > 0;
			}
				
			else
			{
				for( INT DoCD=0; DoCD<1+(GCdPath[0]!=0); DoCD++ )
				{
					for( INT i=0; i<GSys->Paths.Num(); i++ )
					{
						TCHAR Test[256] = TEXT("");
						if (DoCD)
						{
							appStrcat( Test, GCdPath );
							appStrcat( Test, TEXT("System")PATH_SEPARATOR );
						}

						appStrcat( Test, *GSys->Paths(i) );
						if ( PackageWildcard != TEXT("*.*") )
						{
							*appStrstr( Test, TEXT("*.") ) = 0;

							PathPrefix = Test;
							appStrcat( Test, *PackageWildcard );
						}

						FilesInPath = GFileManager->FindFiles( Test, 1, 0 );
						bFound = bFound || FilesInPath.Num() > 0;

						for ( INT j = 0; j < FilesInPath.Num(); j++ )
							new(Files) FString(PathPrefix + FilesInPath(j));
					}
				}
			}

			if( !bFound )
			{
				GWarn->Logf( NAME_Error, TEXT("No packages found matching %s!"), *PackageWildcard );
				continue;
			}
		}

		if ( Files.Num() > 0 )
			Sort( &Files(0), Files.Num() );

		for( INT i = 0; i < Files.Num(); i++ )
        {
            FString IntName;

            GWarn->Logf (NAME_Log, TEXT("Loading %s..."), *Files(i) );
            UPackage* Package = Cast<UPackage>( LoadPackage( NULL, *Files(i), LOAD_NoWarn ) );
			if ( !Package )
			{
				GWarn->Logf(NAME_Error, TEXT("    Unable to load package '%s'"), *Files(i));
				continue;
			}

            IntGetNameFromPackageName ( *Files(i), IntName );

            IntExport( Package, *IntName, true, Files(i).Right(2) != TEXT(".u"), AutoCheckout );

            UObject::CollectGarbage( RF_Native );

            Count++;
        }

        if( !Count )
            GWarn->Log( NAME_Error, TEXT("Syntax: ucc dumpint <file.ext> [file.ext...]") );

		GIsRequestingExit=1;
		return 0;

		unguard;
	}
};
IMPLEMENT_CLASS(UDumpIntCommandlet)

/*-----------------------------------------------------------------------------
	UCompareIntCommandlet.
-----------------------------------------------------------------------------*/

class UCompareIntCommandlet : public UCommandlet
{
	DECLARE_CLASS(UCompareIntCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UCompareIntCommandlet::StaticConstructor);

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
		guard(UCompareIntCommandlet::Main);

		UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
        GEditor->InitEditor();
		GIsRequestingExit = 1; // Causes ctrl-c to immediately exit.

		FString Wildcard;

		if( !ParseToken(Parms, Wildcard, 0) )
			appErrorf(TEXT("Example: uuc compareint *.frt"));
		TArray<FString> ForeignFiles = GFileManager->FindFiles( *Wildcard, 1, 0 );
        if( !ForeignFiles.Num() )
            appErrorf( TEXT("No files matching %s found"), *Wildcard );

		for( INT i=0;i<ForeignFiles.Num();i++ )
		{
			UBOOL NewFile = 1;
			INT d = ForeignFiles(i).InStr(TEXT("."), 1);
			FString IntFile = ForeignFiles(i).Left(d) + TEXT(".int");

			FConfigFile* IntSections = ((FConfigCacheIni*)(GConfig))->Find( *IntFile, 0 );
			if( !IntSections )
				continue;
			FConfigFile* LocSections = ((FConfigCacheIni*)(GConfig))->Find( *ForeignFiles(i), 0 );
			check( LocSections );

			FString LastSection;
			for( TMap<FString,FConfigSection>::TIterator It(*IntSections); It; ++It )
			{
				for( TMultiMap<FString,FString>::TIterator It2(It.Value()); It2; ++It2 )
				{
					FString Section = It.Key();
					FString Key		= It2.Key();
					FString Value	= It2.Value();
					FConfigSection* LocSec;
					if( (LocSec=LocSections->Find(*Section))==NULL || LocSec->Find(*Key)==NULL )
					{
						if( NewFile )
						{
							GWarn->Logf(TEXT("------------------------------- %s -------------------------------"), *ForeignFiles(i) );
							NewFile = 0;
						}
						if( It.Key() != LastSection )
						{
							GWarn->Logf(TEXT("\n[%s]"), *Section );
							LastSection = Section;
						}
						GWarn->Logf(TEXT("%s=\"%s\""), *Key, *Value );
					}
				}                				
			}
			GConfig->UnloadFile( *ForeignFiles(i) );
			GConfig->UnloadFile( *IntFile );
		}

		return 0;

		unguard;
	}
};
IMPLEMENT_CLASS(UCompareIntCommandlet)



/*-----------------------------------------------------------------------------
	UMergeIntCommandlet.
-----------------------------------------------------------------------------*/

class UMergeIntCommandlet : public UCommandlet
{
	DECLARE_CLASS(UMergeIntCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UMergeIntCommandlet::StaticConstructor);

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
		guard(UMergeIntCommandlet::Main);

		UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
        GEditor->InitEditor();
		GIsRequestingExit = 1; // Causes ctrl-c to immediately exit.

		FString DiffFile, IntExt;

		if( !ParseToken(Parms, DiffFile, 0) )
			appErrorf(TEXT("You must specify a change file"));
		if( !ParseToken(Parms, IntExt, 0) )
			appErrorf(TEXT("You must specify an intfile extension"));
		FString DiffText;
		if( GFileManager->FileSize(*DiffFile) < 0 || !appLoadFileToString( DiffText, *DiffFile ) )
			appErrorf(TEXT("Could not open %s"),*DiffFile);

		const TCHAR* Ptr = *DiffText;
		TMap<FString,FString> FileMap;
		FString StrLine;
		FString CurrentFile;
		while(ParseLine(&Ptr,StrLine))
		{
			if( StrLine.Left(5) == TEXT("-----") )
			{
				CurrentFile = StrLine.Mid( StrLine.InStr(TEXT(" ")) + 1 );
				CurrentFile = CurrentFile.Left( CurrentFile.InStr(TEXT(".")) );
			}
			else
			{
				FString* ExistingStr = FileMap.Find( *CurrentFile );
				if( ExistingStr )
					FileMap.Set( *CurrentFile, *(*ExistingStr + TEXT("\r\n") + StrLine) );
				else
					FileMap.Set( *CurrentFile, *StrLine );
			}		
		}

		for( TMultiMap<FString,FString>::TIterator It(FileMap); It; ++It )
		{
			appSaveStringToFile( It.Value(), *(It.Key()+TEXT(".temp")) );

			FConfigFile* NewSections = ((FConfigCacheIni*)(GConfig))->Find( *(It.Key()+TEXT(".temp")), 0 );
			check( NewSections );
			
			for( TMap<FString,FConfigSection>::TIterator It2(*NewSections); It2; ++It2 )
			{
				for( TMultiMap<FString,FString>::TIterator It3(It2.Value()); It3; ++It3 )
				{
					FConfigFile* ExistingFile = ((FConfigCacheIni*)(GConfig))->Find( *(It.Key()+TEXT(".")+IntExt), 0 );
					ExistingFile->Quotes = 1;

					GWarn->Logf(TEXT("%s.%s: [%s] %s=\"%s\""), *It.Key(), *IntExt, *It2.Key(), *It3.Key(), *It3.Value() );
					GConfig->SetString( *It2.Key(), *It3.Key(), *It3.Value(), *(It.Key()+TEXT(".")+IntExt), 0 );
				}
			}
		}

		for( TMultiMap<FString,FString>::TIterator It(FileMap); It; ++It )
			GFileManager->Delete(*(It.Key()+TEXT(".temp")));

		return 0;

		unguard;
	}
};
IMPLEMENT_CLASS(UMergeIntCommandlet)

/*-----------------------------------------------------------------------------
	URearrangeIntCommandlet.
-----------------------------------------------------------------------------*/

class URearrangeIntCommandlet : public UCommandlet
{
	DECLARE_CLASS(URearrangeIntCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(URearrangeIntCommandlet::StaticConstructor);

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
		guard(URearrangeIntCommandlet::Main);

		UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
        GEditor->InitEditor();
		GIsRequestingExit = 1; // Causes ctrl-c to immediately exit.

		FString NewInt, OldInt;

		if( !ParseToken(Parms, NewInt, 0) )
			appErrorf(TEXT("Example: uuc RearrangeInt newint.frt oldint.frt"));

		if( !ParseToken(Parms, OldInt, 0) )
			appErrorf(TEXT("Example: uuc RearrangeInt newint.frt oldint.frt"));

		FConfigFile* IntSections = ((FConfigCacheIni*)(GConfig))->Find( *OldInt, 0 );
		check( IntSections );

		for( TMap<FString,FConfigSection>::TIterator It(*IntSections); It; ++It )
		{
			for( TMultiMap<FString,FString>::TIterator It2(It.Value()); It2; ++It2 )
			{
				FString Section = It.Key();
				FString Key		= It2.Key();
				FString Value	= It2.Value();

				FString TempStr;
				if( GConfig->GetString( *Section, *Key, TempStr, *NewInt ) )
					GConfig->SetString( *Section, *Key, *Value, *NewInt );
				else
				{
					// new subobject format
					INT i = Section.InStr(TEXT("."));
					if( i != -1 )
					{
						Key = Section.Mid(i+1) + TEXT(".") + Key;
						Section = Section.Left(i);
						if( GConfig->GetString( *Section, *Key, TempStr, *NewInt ) )
							GConfig->SetString( *Section, *Key, *Value, *NewInt );
					}
				}
			}
		}

		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(URearrangeIntCommandlet);

