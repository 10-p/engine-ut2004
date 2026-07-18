/*=============================================================================
	UBatchExportCommandlet.cpp: Unreal file exporting commandlet.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	UBatchExportCommandlet.
-----------------------------------------------------------------------------*/

class UBatchExportCommandlet : public UCommandlet
{
	DECLARE_CLASS(UBatchExportCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UBatchExportCommandlet::StaticConstructor);

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
		guard(UBatchExportCommandlet::Main);
		FString Pkg, Cls, Ext, Path;

		GUglyHackFlags |= 64; // rjp

		if( !ParseToken(Parms,Pkg,0) )
			appErrorf(TEXT("Package file not specified"));
		if( !ParseToken(Parms,Cls,0) )
			appErrorf(TEXT("Exporter not specified"));
		if( !ParseToken(Parms,Ext,0) )
			appErrorf(TEXT("File extension not specified"));
		if( !ParseToken(Parms,Path,0) )
			Path=TEXT(".");
		if( Ext.Left(1)==TEXT(".") )
			Ext = Ext.Mid(1);
		UClass* Class = FindObjectChecked<UClass>( ANY_PACKAGE, *Cls );
		GWarn->Logf( TEXT("Loading package %s..."), *Pkg );
		UObject* Package = LoadPackage(NULL,*Pkg,LOAD_NoFail);
		if( !GFileManager->MakeDirectory( *Path, 1 ) )
			appErrorf( TEXT("Failed to make directory %s"), *Path );

		// CPF_NoExport is only used by the GUIDesigner, but will prevent default properties from being exported during batchexport
		// Clear the CPF_NoExport flag for all properties prior to batchexport
		if ( Class == UClass::StaticClass() )
		{
			for ( TObjectIterator<UProperty> PropIt; PropIt; ++PropIt )
				if ( appStrcmp(PropIt->GetName(),TEXT("Name")) )
					PropIt->PropertyFlags &= ~CPF_NoExport;
		}

		for( TObjectIterator<UObject> It; It; ++It )
		{
			if( It->IsA(Class) && It->IsIn(Package) )
			{
				if ( Class == UClass::StaticClass() && !CastChecked<UClass>(*It)->ScriptText )
					continue;

				FString PackageWithDot = Package->GetName();
				PackageWithDot = PackageWithDot.Caps() + TEXT(".");
				FString WithGroup = It->GetPathName();
				if( WithGroup.Left(PackageWithDot.Len()).Caps() == PackageWithDot )
					WithGroup = WithGroup.Mid(PackageWithDot.Len());
				else
					WithGroup = It->GetName();

				FString Filename = Path * WithGroup + TEXT(".") + Ext;
				if( UExporter::ExportToFile(*It, NULL, *Filename, 1, 0) )
					GWarn->Logf( TEXT("Exported %s to %s"), It->GetFullName(), *Filename );
				else
					appErrorf(TEXT("Can't export %s to file %s"),It->GetFullName(),*Filename);
			}
		}
		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UBatchExportCommandlet)


/*-----------------------------------------------------------------------------
	UBatchImportCommandlet.
-----------------------------------------------------------------------------*/

// example: ucc editor.batchimport ..\sounds\TauntPack.uax audio t\*.wav
class UBatchImportCommandlet : public UCommandlet
{
	DECLARE_CLASS(UBatchImportCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UBatchImportCommandlet::StaticConstructor);

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
		guard(UBatchImportCommandlet::Main);

		UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
        GEditor->InitEditor();
		GIsRequestingExit = 1; // Causes ctrl-c to immediately exit.

		FString PackageFile, Pkg, Type, Wildcard, Extra, E;
		if( !ParseToken(Parms,PackageFile,0) )
			appErrorf(TEXT("Package file not specified"));

		INT j=PackageFile.InStr( TEXT("."), 1 );
		if( j ==-1 )
			appErrorf(TEXT("Package should contain an extension"));
		Pkg = PackageFile.Left(j);
		j = Pkg.InStr(PATH_SEPARATOR, 1);
		if( j==-1 )
			appErrorf(TEXT("Package should contain a path reference"));
		Pkg = Pkg.Mid(j+1);

		if( !ParseToken(Parms,Type,0) )
			appErrorf(TEXT("Type not specified (audio, texture)"));
		if( !ParseToken(Parms,Wildcard,0) )
			appErrorf(TEXT("Wildcard not specified"));
		while( ParseToken(Parms,E,0) )
			Extra = Extra + TEXT(" ") + E;

		FString Path;
		j = Wildcard.InStr( PATH_SEPARATOR, 1 );
		if( j != -1 )
			Path = Wildcard.Left( j+1 );
        TArray<FString> Files = GFileManager->FindFiles( *Wildcard, 1, 0 );
		for( INT i=0;i<Files.Num();i++ )
		{
			FString Exec, Name, Group;

			j = Files(i).InStr( TEXT("."), 1 );
			if( j!=-1 )
				Name = Files(i).Left(j);
			else
				Name = Files(i); //!! weird, no extension
            
			j = Name.InStr( TEXT(".") );
			if( j!=-1 )
			{
				Group = Name.Left(j);
				Name = Name.Mid(j+1);
			}
	
			Exec = FString::Printf(TEXT("%s import package=\"%s\" file=\"%s%s\" name=\"%s\""), *Type, *Pkg, *Path, *Files(i), *Name);
			if( Group != TEXT("") )
				Exec = Exec + FString::Printf(TEXT(" group=\"%s\""), *Group);
			if( Extra != TEXT("") )
				Exec = Exec + TEXT(" ") + Extra; 
            
			GWarn->Logf(*Exec);
			GEditor->Exec( *Exec );
		}

		GEditor->Exec( *FString::Printf(TEXT("OBJ SAVEPACKAGE PACKAGE=\"%s\" FILE=\"%s\""), *Pkg, *PackageFile ) );

		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UBatchImportCommandlet)


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

