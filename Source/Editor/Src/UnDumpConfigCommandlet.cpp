/*===========================================================================================
	UnDumpConfigCommandlet.cpp: Dumps all configurable variables in the game to an ini file.
	Revision history:
		* Created by Ron Prestenback
=============================================================================================*/

#include "EditorPrivate.h"
#include "UnDumpConfigCommandlet.h"

static INT Compare( FString& A, FString& B )
{
	return appStrcmp(*A,*B);
}

void UDumpConfigCommandlet::StaticConstructor()
{
	guard(UDumpConfigCommandlet::StaticConstructor);

	LogToStdout     = 0;
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 0;
	ShowErrorCount  = 1;

	bInit = 0;
	bNoLoad = 0;

	unguard;
}

INT UDumpConfigCommandlet::Main( const TCHAR* Parms )
{
	guard(UDumpConfigCommandlet::Main);

	GIsRequestingExit = 1; // Causes Ctrl-C to immediately exit

	TArray<FString> Files;

	// Parse the command line
	FString Param;
	while ( ParseToken(Parms, Param, 0) )
	{
		if ( Param == TEXT("-init") )
			bInit = 1;
		else if ( Param == TEXT("-noload") )
			bNoLoad = 1;
	}

	// Delete any existing config dumps if we're doing the initial dump
	if ( bInit )
	{
		Files = GFileManager->FindFiles( TEXT("Dump*.ini"), 1, 0 );
		for ( INT i = 0; i < Files.Num(); i++ )
		{
			GConfig->UnloadFile(*Files(i));
			GFileManager->Delete(*Files(i),1,1);
		}
	}

	if ( bNoLoad )
		GUglyHackFlags |= 64;  // Disables loading of configuration and localization data

	// Find all .u packages

	Files = GFileManager->FindFiles( TEXT("*.u"), 1, 0 );
	if ( Files.Num() == 0 )
	{
		GWarn->Logf(TEXT("No valid packages found!"));
		return 1;
	}

	// Sort the files
	if ( Files.Num() )
		Sort( &Files(0), Files.Num() );

	// Load all packages

	for ( INT i = 0; i < Files.Num(); i++ )
	{
        GWarn->Logf (NAME_Log, TEXT("Loading %s..."), *Files(i) );

        UPackage* Package = Cast<UPackage>( LoadPackage( NULL, *Files(i), LOAD_NoWarn ) );
		if ( !Package )
		{
			GWarn->Logf(NAME_Error, TEXT("    Unable to load package '%s'"), *Files(i));
			continue;
		}

		DumpPackage(Package);
		CollectGarbage(RF_Native);
	}

	return 0;

	unguard;
}

void UDumpConfigCommandlet::DumpPackage(UPackage* Package)
{
	guard(UDumpConfigCommandlet::DumpPackage);

	check(Package);

	// Iterate through all classes, searching for classes that exist in this package
	for ( TObjectIterator<UClass> It; It; ++It )
		if ( (It->ClassFlags & CLASS_Config) && (It->IsIn(Package)) )
			DumpClass(*It);

	GConfig->Flush(0);

	unguard;
}

void UDumpConfigCommandlet::DumpClass( UClass* Cls )
{
	guard(UDumpConfigCommandlet::DumpClass);

	check(Cls);
	if ( !(Cls->ClassFlags & CLASS_Config) )
		return;

	if ( Cls->ClassFlags & CLASS_PerObjectConfig )
	{
		GWarn->Logf(NAME_Log, TEXT("Skipping %s because it is PerObjectConfig."), Cls->GetPathName());
		return;
	}

	TCHAR Filename[NAME_SIZE];
	TCHAR Section[NAME_SIZE];

	if ( Cls->ClassConfigName == TEXT("System") )
		appStrncpy(Filename,TEXT("Dump.ini"),NAME_SIZE-1);
	else if ( Cls->ClassConfigName == TEXT("User") )
		appStrncpy(Filename,TEXT("DumpUser.ini"),NAME_SIZE-1);
	else
		appSprintf(Filename,TEXT("Dump%s.ini"),*Cls->ClassConfigName);

	TCHAR TempKey[256], Value[256*STATICSTRINGLENGTH]=TEXT("");
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Cls); It; ++It )
	{
		if ( !(It->PropertyFlags & CPF_Config) && !(It->PropertyFlags & CPF_GlobalConfig) )
			continue;

		UClass* BaseClass = Cls;
		if (It->PropertyFlags & CPF_GlobalConfig)
			BaseClass = It->GetOwnerClass();

		appStrncpy(Section,BaseClass->GetPathName(),NAME_SIZE);

		const TCHAR*    Key     = It->GetName();
		UArrayProperty* Array   = Cast<UArrayProperty>( *It );
		if( Array )
		{
			TMultiMap<FString,FString>* Sec = GConfig->GetSectionPrivate( Section, bInit, 0, Filename );
			if ( Sec )
			{
				Sec->Remove( Key );
				FArray* Ptr  = (FArray*)(&Cls->Defaults(0) + It->Offset);
				INT     Size = Array->Inner->ElementSize;

				if ( Ptr->Num() == 0 )
					Ptr->AddZeroed( Size, 2 );

				for( INT i=0; i<Ptr->Num(); i++ )
				{
					BYTE* Dest = (BYTE*)Ptr->GetData() + i*Size;
					Array->Inner->ExportTextItem( Value, Dest, Dest, 0 );
					Sec->Add( Key, Value );
				}
			}
			else GWarn->Logf(TEXT("Skipping %s because it didn't already exist in %s"), Section, Filename);
		}
		else 
		{
			TMultiMap<FString,FString>* Sec = GConfig->GetSectionPrivate( Section, bInit, 0, Filename );
			if ( Sec )
			{
				for( INT Index=0; Index<It->ArrayDim; Index++ )
				{
					if( It->ArrayDim!=1 )
					{
						appSprintf( TempKey, TEXT("%s[%i]"), It->GetName(), Index );
						Key = TempKey;
					}
					It->ExportText( Index, Value, &Cls->Defaults(0), &Cls->Defaults(0), 0 );
					GConfig->SetString( Section, Key, Value, Filename );
				}
			}
			else GWarn->Logf(TEXT("Skipping %s because it didn't already exist in %s"), Section, Filename);
		}
	}

	unguard;
}

IMPLEMENT_CLASS(UDumpConfigCommandlet);