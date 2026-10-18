/*=============================================================================
	UExporter.cpp: Exporter class implementation.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

// Core includes.
#include "CorePrivate.h"

/*----------------------------------------------------------------------------
	UExporter.
----------------------------------------------------------------------------*/

void UExporter::StaticConstructor()
{
	guard(UExporter::StaticConstructor);
	UArrayProperty* A = new(GetClass(),TEXT("Formats"),RF_Public)UArrayProperty(CPP_PROPERTY(Formats),TEXT(""),0);
	A->Inner = new(A,TEXT("StrProperty0"),RF_Public)UStrProperty;
	unguard;
}
UExporter::UExporter()
: Formats( E_NoInit )
{}
void UExporter::Serialize( FArchive& Ar )
{
	guard(UExporter::Serialize);
	Super::Serialize( Ar );
	Ar << SupportedClass << Formats;
	unguard;
}
IMPLEMENT_CLASS(UExporter);

/*----------------------------------------------------------------------------
	Object exporting.
----------------------------------------------------------------------------*/

//
// Find an exporter.
//
UExporter* UExporter::FindExporter( UObject* Object, const TCHAR* FileType )
{
	guard(UExporter::FindExporter);
	check(Object);

	// Do not walk the object table while a load is in progress. The TObjectIterator below visits every
	// object, and UClass::Serialize generates DefaultPropText for each class as it loads (in the game too),
	// whose ExportProperties calls ExportToOutputDevice for every CPF_ExportObject property: the walk would
	// run inside ULinkerLoad::Preload, while the package's other objects exist in the table unserialized,
	// and follow ->Class / SuperField through garbage (a fault on Windows, where the memory is not zero).
	// Nothing is lost: an exporter is a T3D/editor concern, and this build compiles no UExporter subclass.
	if( UObject::IsLoadInProgress() )
		return NULL;

	TMap<UClass*,UClass*> Exporters;

	for( TObjectIterator<UClass> It; It; ++It )
	{
		if( It->IsChildOf(UExporter::StaticClass()) )
		{
			UExporter* Default = (UExporter*)It->GetDefaultObject();
			if( Default->SupportedClass && Object->IsA(Default->SupportedClass) )
				for( INT i=0; i<Default->Formats.Num(); i++ )
					if
					(	appStricmp( *Default->Formats(i), FileType  )==0
					||	appStricmp( *Default->Formats(i), TEXT("*") )==0 )
						Exporters.Set( Default->SupportedClass, *It );
		}
	}

	UClass** E;
	for( UClass* TempClass=Object->GetClass(); TempClass; TempClass=(UClass*)TempClass->SuperField )
		if( (E = Exporters.Find( TempClass )) != NULL )
			return ConstructObject<UExporter>( *E );
		
	return NULL;
	unguard;
}

//
// Export an object to an archive.
//
void UExporter::ExportToArchive( UObject* Object, UExporter* InExporter, FArchive& Ar, const TCHAR* FileType )
{
	guard(UExporter::ExportToArchive);
	check(Object);
	UExporter* Exporter = InExporter;
	if( !Exporter )
	{
		Exporter = FindExporter( Object, FileType );
	}
	if( !Exporter )
	{
		GWarn->Logf( TEXT("No %s exporter found for %s"), FileType, Object, Object->GetFullName() );
		return;
	}
	check(Object->IsA(Exporter->SupportedClass));
	Exporter->ExportBinary( Object, FileType, Ar, GWarn );
	if( !InExporter )
		delete Exporter;
	unguard;
}

//
// Export an object to an output device.
//
void UExporter::ExportToOutputDevice( UObject* Object, UExporter* InExporter, FOutputDevice& Out, const TCHAR* FileType, INT Indent )
{
	guard(UExporter::ExportToOutputDevice);
	check(Object);
	UExporter* Exporter = InExporter;
	if( !Exporter )
	{
		Exporter = FindExporter( Object, FileType );
	}
	if( !Exporter )
	{
		GWarn->Logf( TEXT("No %s exporter found for %s"), FileType, Object->GetFullName() );
		return;
	}
	check(Object->IsA(Exporter->SupportedClass));
	INT SavedIndent = Exporter->TextIndent;
	Exporter->TextIndent = Indent;
	Exporter->ExportText( Object, FileType, Out, GWarn );
	Exporter->TextIndent = SavedIndent;
	if( !InExporter )
		delete Exporter;
	unguard;
}

//
// Export this object to a file.  Child classes do not
// override this, but they do provide an Export() function
// to do the resoource-specific export work.  Returns 1
// if success, 0 if failed.
//
UBOOL UExporter::ExportToFile( UObject* Object, UExporter* InExporter, const TCHAR* Filename, UBOOL NoReplaceIdentical, UBOOL Prompt )
{
	guard(UExporter::ExportToFile);
	check(Object);
	UExporter* Exporter = InExporter;
	const TCHAR* FileType = appFExt(Filename);
	UBOOL Result = 0;
	if( !Exporter )
	{
		Exporter = FindExporter( Object, FileType );
	}
	if( !Exporter )
	{
		GWarn->Logf( TEXT("No %s exporter found for %s"), FileType, Object->GetFullName() );
		return 0;
	}
	if( Exporter->bText )
	{
		FStringOutputDevice Buffer;
		ExportToOutputDevice( Object, Exporter, Buffer, FileType, 0 );
		if( NoReplaceIdentical )
		{
			FString FileBytes;
			if
			(	appLoadFileToString(FileBytes,Filename)
			&&	appStrcmp(*Buffer,*FileBytes)==0 )
			{
				debugf( TEXT("Not replacing %s because identical"), Filename );
				Result = 1;
				goto Done;
			}
			if( Prompt )
			{
				if( !GWarn->YesNof( LocalizeQuery(TEXT("Overwrite"),TEXT("Core")), Filename ) )
				{
					Result = 1;
					goto Done;
				}
			}
		}

        if( GFileManager->IsReadOnly( Filename ) )
        {
			if( ParseParam(appCmdLine(), TEXT("auto")) ||
			  (!ParseParam(appCmdLine(), TEXT("silentbuild")) && GWarn->YesNof(LocalizeQuery(TEXT("CheckoutPerforce"),TEXT("Core")), Filename) ) )
			{
				INT Code;
				void* Handle = appCreateProc( TEXT("p4"), *FString::Printf(TEXT("edit %s"), Filename) );
				while( !appGetProcReturnCode( Handle, &Code ) )
					appSleep(1);
			}
			else
			{
				GWarn->Logf( NAME_Error, LocalizeError(TEXT("ExportOpen"),TEXT("Core")), Object->GetFullName(), Filename );
				Result = 0;
				goto Done;
			}
		}

		if( !appSaveStringToFile( Buffer, Filename ) )
		{
			GWarn->Logf( NAME_Error, LocalizeError(TEXT("ExportOpen"),TEXT("Core")), Object->GetFullName(), Filename );
			Result = 0;
			goto Done;
		}
		Result = 1;
	}
	else
	{
		FBufferArchive Buffer;
		ExportToArchive( Object, Exporter, Buffer, FileType );
		if( NoReplaceIdentical )
		{
			TArray<BYTE> FileBytes;
			if
			(	appLoadFileToArray(FileBytes,Filename)
			&&	FileBytes.Num()==Buffer.Num()
			&&	appMemcmp(&FileBytes(0),&Buffer(0),Buffer.Num())==0 )
			{
				debugf( TEXT("Not replacing %s because identical"), Filename );
				Result = 1;
				goto Done;
			}
			if( Prompt )
			{
				if( !GWarn->YesNof( LocalizeQuery(TEXT("Overwrite"),TEXT("Core")), Filename ) )
				{
					Result = 1;
					goto Done;
				}
			}
		}

        // gam ---
        if( GFileManager->IsReadOnly( Filename ) )
        {
		    GWarn->Logf( NAME_Error, TEXT("Could not write %s: a read-only file with that name already exists."), Filename );
		    Result = 0;
		    goto Done;
        }
        // --- gam

		if( !appSaveArrayToFile( Buffer, Filename ) )
		{
			GWarn->Logf( LocalizeError(TEXT("ExportOpen"),TEXT("Core")), Object->GetFullName(), Filename );
			goto Done;
		}
		Result = 1;
	}
Done:
	if( !InExporter )
		delete Exporter;
	return Result;
	unguard;
}


/*------------------------------------------------------------------------------
	UObjectExporterT3D implementation.
------------------------------------------------------------------------------*/

void UObjectExporterT3D::StaticConstructor()
{
	guard(UObjectExporterT3D::StaticConstructor);
	SupportedClass = UObject::StaticClass();
	bText = 1;
	new(Formats)FString(TEXT("T3D"));
	unguard;
}
UBOOL UObjectExporterT3D::ExportText( UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn )
{
	guard(UObjectExporterT3D::ExportText);
	Ar.Logf( TEXT("%sBegin Object Class=%s Name=%s\r\n"),appSpc(TextIndent), Object->GetClass()->GetName(), Object->GetName() );
	ExportProperties( Ar, Object->GetClass(), (BYTE*)Object, TextIndent+3,Object->GetClass(), &Object->GetClass()->Defaults(0) );
	Ar.Logf( TEXT("%sEnd Object\r\n"), appSpc(TextIndent) );
	return 1;
	unguard;
}
IMPLEMENT_CLASS(UObjectExporterT3D);


/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

