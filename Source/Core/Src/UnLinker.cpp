/*=============================================================================
	UnLinker.cpp: Unreal object linker.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	Hash function.
-----------------------------------------------------------------------------*/

static inline INT HashNames( FName A, FName B, FName C )
{
	return A.GetIndex() + 7 * B.GetIndex() + 31*C.GetIndex();
}

/*-----------------------------------------------------------------------------
	FObjectExport.
-----------------------------------------------------------------------------*/

FObjectExport::FObjectExport()
:	ClassIndex		( 0															)
,	SuperIndex		( 0															)
,	PackageIndex	( 0															)
,	ObjectName		( FName(NAME_None)											)
,	ObjectFlags		( 0															)
,	SerialSize		( 0															)
,	SerialOffset	( 0															)
,	_Object			( NULL														)
,	_iHashNext		( INDEX_NONE												)
{}

FObjectExport::FObjectExport( UObject* InObject )
:	ClassIndex		( 0															)
,	SuperIndex		( 0															)
,	PackageIndex	( 0															)
,	ObjectName		( InObject ? InObject->GetFName() : FName(NAME_None)		)
,	ObjectFlags		( InObject ? (InObject->GetFlags() & RF_Load) : 0			)
,	SerialSize		( 0															)
,	SerialOffset	( 0															)
,	_Object			( InObject													)
,	_iHashNext		( INDEX_NONE												)
{}

FArchive& operator<<( FArchive& Ar, FObjectExport& E )
{
	guard(FObjectExport<<);

	Ar << AR_INDEX(E.ClassIndex);
	Ar << AR_INDEX(E.SuperIndex);
	Ar << E.PackageIndex;
	Ar << E.ObjectName;
	Ar << E.ObjectFlags;
	Ar << AR_INDEX(E.SerialSize);
	if( E.SerialSize )
		Ar << AR_INDEX(E.SerialOffset);

	return Ar;
	unguard;
}

/*-----------------------------------------------------------------------------
	FObjectImport.
-----------------------------------------------------------------------------*/

FObjectImport::FObjectImport()
:	SourceLinker	( NULL														)
{}

FObjectImport::FObjectImport( UObject* InObject )
:	ClassPackage	( InObject->GetClass()->GetOuter()->GetFName())
,	ClassName		( InObject->GetClass()->GetFName()		 )
,	PackageIndex	( 0                                      )
,	ObjectName		( InObject->GetFName()					 )
,	XObject			( InObject								 )
,	SourceLinker	( NULL									 )
,	SourceIndex		( INDEX_NONE							 )
{
	if( XObject )
		UObject::GImportCount++;
}

FArchive& operator<<( FArchive& Ar, FObjectImport& I )
{
	guard(FObjectImport<<);

	Ar << I.ClassPackage << I.ClassName;
	Ar << I.PackageIndex;
	Ar << I.ObjectName;
	if( Ar.IsLoading() )
	{
		I.SourceIndex = INDEX_NONE;
		I.XObject     = NULL;
	}
	return Ar;

	unguard;
}

/*----------------------------------------------------------------------------
	Items stored in Unrealfiles.
----------------------------------------------------------------------------*/

FGenerationInfo::FGenerationInfo( INT InExportCount, INT InNameCount )
: ExportCount(InExportCount), NameCount(InNameCount)
{}

FPackageFileSummary::FPackageFileSummary()
{
	appMemzero( this, sizeof(*this) );
}

FArchive& operator<<( FArchive& Ar, FPackageFileSummary& Sum )
{
	guard(FUnrealfileSummary<<);

	Ar << Sum.Tag;
	if( Sum.Tag != PACKAGE_FILE_TAG )	//!! Don't serialize any more if the tag doesn't match.
		return Ar;
	Ar << Sum.FileVersion;
	Ar << Sum.PackageFlags;
	Ar << Sum.NameCount     << Sum.NameOffset;
	Ar << Sum.ExportCount   << Sum.ExportOffset;
	Ar << Sum.ImportCount   << Sum.ImportOffset;
	if( Sum.GetFileVersion()>=68 )
	{
		INT GenerationCount = Sum.Generations.Num();
		Ar << Sum.Guid << GenerationCount;
		//!!67 had: return
		if( Ar.IsLoading() )
			Sum.Generations = TArray<FGenerationInfo>( GenerationCount );
		for( INT i=0; i<GenerationCount; i++ )
			Ar << Sum.Generations(i);
	}
	else //oldver
	{
		INT HeritageCount, HeritageOffset;
		Ar << HeritageCount << HeritageOffset;
		INT Saved = Ar.Tell();
		if( HeritageCount )
		{
			Ar.Seek( HeritageOffset );
			for( INT i=0; i<HeritageCount; i++ )
				Ar << Sum.Guid;
		}
		Ar.Seek( Saved );
		if( Ar.IsLoading() )
		{
			Sum.Generations.Empty( 1 );
			new(Sum.Generations)FGenerationInfo(Sum.ExportCount,Sum.NameCount);
		}
	}

	return Ar;
	unguard;
}

/*----------------------------------------------------------------------------
	ULinker.
----------------------------------------------------------------------------*/

ULinker::ULinker( UObject* InRoot, const TCHAR* InFilename )
:	LinkerRoot( InRoot )
,	Summary()
,	Success( 123456 )
,	Filename( InFilename )
,	_ContextFlags( 0 )
{
	check(LinkerRoot);
	check(InFilename);

	// Set context flags.
	if( GIsEditor ) _ContextFlags |= RF_LoadForEdit;
	if( GIsClient ) _ContextFlags |= RF_LoadForClient;
	if( GIsServer ) _ContextFlags |= RF_LoadForServer;
}

void ULinker::Serialize( FArchive& Ar )
{
	guard(ULinker::Serialize);
	Super::Serialize( Ar );

	// Sizes.
	ImportMap	.CountBytes( Ar );
	ExportMap	.CountBytes( Ar );

	// Prevent garbage collecting of linker's names and package.
	Ar << NameMap << LinkerRoot;
	{for( INT i=0; i<ExportMap.Num(); i++ )
	{
		FObjectExport& E = ExportMap(i);
		Ar << E.ObjectName;
	}}
	{for( INT i=0; i<ImportMap.Num(); i++ )
	{
		FObjectImport& I = ImportMap(i);
		Ar << *(UObject**)&I.SourceLinker;
		Ar << I.ClassPackage << I.ClassName;
	}}
	unguard;
}

// ULinker interface.
FString ULinker::GetImportFullName( INT i )
{
	guard(ULinkerLoad::GetImportFullName);
	FString S;
	for( INT j=-i-1; j!=0; j=ImportMap(-j-1).PackageIndex )
	{
		if( j != -i-1 )
			S = US + TEXT(".") + S;
		S = FString(*ImportMap(-j-1).ObjectName) + S;
	}
	return FString(*ImportMap(i).ClassName) + TEXT(" ") + S ;
	unguard;
}

FString ULinker::GetExportFullName( INT i, const TCHAR* FakeRoot )
{
	guard(ULinkerLoad::GetExportFullName);
	FString S;
	for( INT j=i+1; j!=0; j=ExportMap(j-1).PackageIndex )
	{
		if( j != i+1 )
			S = US + TEXT(".") + S;
		S = FString(*ExportMap(j-1).ObjectName) + S;
	}
	INT ClassIndex = ExportMap(i).ClassIndex;
	FName ClassName = ClassIndex>0 ? ExportMap(ClassIndex-1).ObjectName : ClassIndex<0 ? ImportMap(-ClassIndex-1).ObjectName : FName(NAME_Class);
	return FString(*ClassName) + TEXT(" ") + (FakeRoot ? FakeRoot : LinkerRoot->GetPathName()) + TEXT(".") + S;
	unguard;
}

// QuickMD5 - returns the Quick MD5 hash for this package
// ======================================================
FString ULinker::QuickMD5()
{
	int i;
	FString MD5;

	for (i=0; i<16; i++)
		MD5 += FString::Printf(TEXT("%02x"), QuickMD5Digest[i]);	

	return MD5;
}

UBOOL ULinker::LinksToCode()
{
	return false;
}


/*----------------------------------------------------------------------------
	ULinkerLoad.
----------------------------------------------------------------------------*/

ULinkerLoad::ULinkerLoad( UObject* InParent, const TCHAR* InFilename, DWORD InLoadFlags )
:	ULinker( InParent, InFilename )
,	LoadFlags( InLoadFlags )
{
	guard(ULinkerLoad::ULinkerLoad);

	Loader = GFileManager->CreateFileReader( InFilename, 0, GError );
	if( !Loader )
#ifdef __EMSCRIPTEN__
	{
		// Fail soft: Asyncify can't unwind a throw out of this (it does async file I/O),
		// so leave Success != 1 and emit a clear message. GetPackageLinker drops the
		// half-built linker and returns NULL — the load fails with a message, not an abort.
		debugf( NAME_Warning, TEXT("[PACKAGE-LOAD] Could not open package file '%s'"), InFilename );
		return;
	}
#else
		appThrowf( LocalizeError(TEXT("OpenFailed"),TEXT("Core")) );
#endif

	// Error if linker already loaded.
	{for( INT i=0; i<GObjLoaders.Num(); i++ )
		if( GetLoader(i)->LinkerRoot == LinkerRoot )
#ifdef __EMSCRIPTEN__
		{ debugf( NAME_Warning, TEXT("[PACKAGE-LOAD] Package '%s' is already loaded"), LinkerRoot->GetName() ); return; }
#else
			appThrowf( LocalizeError(TEXT("LinkerExists"),TEXT("Core")), LinkerRoot->GetName() );
#endif
	}

	// Begin.
	GWarn->StatusUpdatef( 0, 0, LocalizeProgress(TEXT("Loading"),TEXT("Core")), *Filename );

	// Create MD5 Context

	FMD5Context PMD5Context;
	appMD5Init( &PMD5Context );

	// Set status info.
	guard(InitAr);
	ArVer       = PACKAGE_FILE_VERSION;
	ArLicenseeVer	= PACKAGE_FILE_VERSION_LICENSEE;
	ArIsLoading = ArIsPersistent = 1;
	ArForEdit   = GIsEditor;
	ArForClient = 1;
	ArForServer = 1;
	unguard;

	// Read summary from file.
	guard(LoadSummary);
	*this << Summary;

	// MD5sum the package summary...
	// !!! FIXME: Can we just hook this up to the serialization code?
	appMD5Update32( &PMD5Context, Summary.Tag );
	appMD5Update32( &PMD5Context, Summary.GetFileVersionComplete() );
	appMD5Update32( &PMD5Context, Summary.PackageFlags );
	appMD5Update32( &PMD5Context, Summary.NameCount );
	appMD5Update32( &PMD5Context, Summary.NameOffset );
	appMD5Update32( &PMD5Context, Summary.ExportCount );
	appMD5Update32( &PMD5Context, Summary.ExportOffset );
	appMD5Update32( &PMD5Context, Summary.ImportCount );
	appMD5Update32( &PMD5Context, Summary.ImportOffset );
	appMD5Update32( &PMD5Context, Summary.Guid.A );
	appMD5Update32( &PMD5Context, Summary.Guid.B );
	appMD5Update32( &PMD5Context, Summary.Guid.C );
	appMD5Update32( &PMD5Context, Summary.Guid.D );

	// MD5 the Generations table.
	for (int i=0;i<Summary.Generations.Num(); i++)
	{
		appMD5Update32(&PMD5Context, Summary.Generations(i).ExportCount);
		appMD5Update32(&PMD5Context, Summary.Generations(i).NameCount);
	}

	// Check tag.
	guard(CheckTag);
	if( Summary.Tag != PACKAGE_FILE_TAG )
	{
		GWarn->Logf( LocalizeError(TEXT("BinaryFormat"),TEXT("Core")), *Filename );
#ifdef __EMSCRIPTEN__
		debugf( NAME_Warning, TEXT("[PACKAGE-LOAD] '%s' is not a valid Unreal package (corrupt file or wrong format) — skipping"), *Filename );
		return;
#else
		throw( LocalizeError(TEXT("Aborted"),TEXT("Core")) );
#endif
	}
	unguard;

	ArVer = Summary.GetFileVersion();
	ArLicenseeVer = Summary.GetFileVersionLicensee();
	if( Cast<UPackage>(LinkerRoot) )
		Cast<UPackage>(LinkerRoot)->PackageFlags = Summary.PackageFlags;
	unguard;

	// Validate the summary.
	guard(CheckVersion);
	if( Summary.GetFileVersion() < PACKAGE_MIN_VERSION )
#ifdef __EMSCRIPTEN__
	{
		debugf( NAME_Warning, TEXT("[PACKAGE-LOAD] '%s' is an incompatible (older) package — file version %i, engine needs >= %i"), *Filename, Summary.GetFileVersion(), PACKAGE_MIN_VERSION );
		return;
	}
#else
		if( !GWarn->YesNof( LocalizeQuery(TEXT("OldVersion"),TEXT("Core")), *Filename ) )
			throw( LocalizeError(TEXT("Aborted"),TEXT("Core")) );
#endif
	unguard;

	// Slack everything according to summary.
	ImportMap   .Empty( Summary.ImportCount   );
	ExportMap   .Empty( Summary.ExportCount   );
	NameMap		.Empty( Summary.NameCount     );

	// Load and map names.
	guard(LoadNames);
	if( Summary.NameCount > 0 )
	{
		Seek( Summary.NameOffset );
		for( INT i=0; i<Summary.NameCount; i++ )
		{
			// Read the name entry from the file.
			FNameEntry NameEntry;
			*this << NameEntry;

			// Add it to the name table if it's needed in this context.				
			NameMap.AddItem( (NameEntry.Flags & _ContextFlags) ? FName( NameEntry.Name, FNAME_Add ) : FName(NAME_None) );
			appMD5Update32(&PMD5Context, NameEntry.Flags);

			// This is a mistake; the "sizeof" makes this "4" without ever calling appStrlen!!
			//appMD5Update(&PMD5Context, (BYTE*)&NameEntry.Name,sizeof(appStrlen(NameEntry.Name) * sizeof(TCHAR) ) );

			// Here's a byte-safe version that assumes we're sending four bytes. Remove this when
			//  fixing the above version.  --ryan.
            appMD5Update16(&PMD5Context, (_WORD) NameEntry.Name[0]);
            appMD5Update16(&PMD5Context, (_WORD) NameEntry.Name[1]);
		}
	}
	unguard;

	// Load import map.
	guard(LoadImportMap);

	if( Summary.ImportCount > 0 )
	{
		Seek( Summary.ImportOffset );
		for( INT i=0; i<Summary.ImportCount; i++ )
		{
			FObjectImport* Import = new(ImportMap)FObjectImport;
			*this << *Import;
		}
	}
	unguard;

	// Load export map.
	guard(LoadExportMap);
	if( Summary.ExportCount > 0 )
	{
		Seek( Summary.ExportOffset );
		for( INT i=0; i<Summary.ExportCount; i++ )
		{
			FObjectExport*	Export = new(ExportMap)FObjectExport;
			*this << *Export;
		}
	}
	unguard;

	// Finalize the QuickMD5

	appMD5Final( QuickMD5Digest, &PMD5Context );

	// Create export hash.
	//warning: Relies on import & export tables, so must be done here.
	{for( INT i=0; i<ARRAY_COUNT(ExportHash); i++ )
	{
		ExportHash[i] = INDEX_NONE;
	}}
	{for( INT i=0; i<ExportMap.Num(); i++ )
	{
		INT iHash = HashNames( ExportMap(i).ObjectName, GetExportClassName(i), GetExportClassPackage(i) ) & (ARRAY_COUNT(ExportHash)-1);
		ExportMap(i)._iHashNext = ExportHash[iHash];
		ExportHash[iHash] = i;
	}}

	// Add this linker to the object manager's linker array.
	GObjLoaders.AddItem( this );
	if( !(LoadFlags & LOAD_NoVerify) )
		Verify();

	// Success.
	Success = 1;

	unguard;
}

void ULinkerLoad::Verify()
{
	guard(ULinkerLoad::Verify);
	if( !Verified )
	{
		if( Cast<UPackage>(LinkerRoot) )
			Cast<UPackage>(LinkerRoot)->PackageFlags &= ~PKG_BrokenLinks;
		try
		{
			// Validate all imports and map them to their remote linkers.
			guard(ValidateImports);
			for( INT i=0; i<Summary.ImportCount; i++ )
				VerifyImport( i );
			unguard;
		}
		// !! PSX2 gcc doesn't like catch( TCHAR* Error )
		#if UNICODE
		catch( TCHAR* Error )
		#else
		catch( char* Error )
		#endif
		{
			GObjLoaders.RemoveItem( this );
			throw( Error );
		}
	}
	Verified=1;
	unguard;
}

FName ULinkerLoad::GetExportClassPackage( INT i )
{
	guardSlow(ULinkerLoad::GetExportClassPackage);
	FObjectExport& Export = ExportMap( i );
	if( Export.ClassIndex < 0 )
	{
		FObjectImport& Import = ImportMap( -Export.ClassIndex-1 );
		checkSlow(Import.PackageIndex<0);
		return ImportMap( -Import.PackageIndex-1 ).ObjectName;
	}
	else if( Export.ClassIndex > 0 )
	{
		return LinkerRoot->GetFName();
	}
	else
	{
		return NAME_Core;
	}
	unguardSlow;
}

FName ULinkerLoad::GetExportClassName( INT i )
{
	guardSlow(GetExportClassName);
	FObjectExport& Export = ExportMap(i);
	if( Export.ClassIndex < 0 )
	{
		return ImportMap( -Export.ClassIndex-1 ).ObjectName;
	}
	else if( Export.ClassIndex > 0 )
	{
		return ExportMap( Export.ClassIndex-1 ).ObjectName;
	}
	else
	{
		return NAME_Class;
	}
	unguardSlow;
}

// Safely verify an import.
void ULinkerLoad::VerifyImport( INT i )
{
	guard(ULinkerLoad::VerifyImport);

	FObjectImport& Import = ImportMap(i);
	if
	(	(Import.SourceLinker && Import.SourceIndex != INDEX_NONE)
	||	Import.ClassPackage	== NAME_None
	||	Import.ClassName	== NAME_None
	||	Import.ObjectName	== NAME_None )
	{
		// Already verified, or not relevent in this context.
		return;
	}

	// Find or load this import's linker.
	INT Depth=0;
	UObject* Pkg=NULL;
	if( Import.PackageIndex == 0 )
	{
		check(Import.ClassName==NAME_Package);
		check(Import.ClassPackage==NAME_Core);
		UPackage* TmpPkg = CreatePackage( NULL, *Import.ObjectName );
		try
		{
			Import.SourceLinker = GetPackageLinker( TmpPkg, NULL, LOAD_Throw | (LoadFlags & LOAD_Propagate), NULL, NULL );
		}
		catch( const TCHAR* Error )//oldver
		{
			appThrowf( Error );
		}
	}
	else
	{
		check(Import.PackageIndex<0);
		VerifyImport( -Import.PackageIndex-1 );
		Import.SourceLinker = ImportMap(-Import.PackageIndex-1).SourceLinker;
		//check(Import.SourceLinker);
		if( Import.SourceLinker )
		{
			FObjectImport* Top;
			for
			(	Top = &Import
			;	Top->PackageIndex<0
			;	Top = &ImportMap(-Top->PackageIndex-1),Depth++ );
			Pkg = CreatePackage( NULL, *Top->ObjectName );
		}
	}

	// Find this import within its existing linker.
	UBOOL SafeReplace = 0;
	Rehack://oldver
	//new:
	INT iHash = HashNames( Import.ObjectName, Import.ClassName, Import.ClassPackage) & (ARRAY_COUNT(ExportHash)-1);
	if( Import.SourceLinker )
	{
		for( INT j=Import.SourceLinker->ExportHash[iHash]; j!=INDEX_NONE; j=Import.SourceLinker->ExportMap(j)._iHashNext )
		//old:
		//for( INT j=0; j<Import.SourceLinker->ExportMap.Num(); j++ )
		{
			FObjectExport& Source = Import.SourceLinker->ExportMap( j );
			if
			(	(Source.ObjectName	                          ==Import.ObjectName               )
			&&	(Import.SourceLinker->GetExportClassName   (j)==Import.ClassName                )
			&&  (Import.SourceLinker->GetExportClassPackage(j)==Import.ClassPackage) )
			{
				if( Import.PackageIndex<0 )
				{
					FObjectImport& ParentImport = ImportMap(-Import.PackageIndex-1);
					if( ParentImport.SourceLinker )
					{
						if( ParentImport.SourceIndex==INDEX_NONE )
						{
							if( Source.PackageIndex!=0 )
							{
								continue;
							}
						}
						else if( ParentImport.SourceIndex+1 != Source.PackageIndex )
						{
							if( Source.PackageIndex!=0 )
							{
								continue;
							}
						}
					}
				}
				if( !(Source.ObjectFlags & RF_Public) )
				{
					if( LoadFlags & LOAD_Forgiving )
					{
						if( Cast<UPackage>(LinkerRoot) )
							Cast<UPackage>(LinkerRoot)->PackageFlags |= PKG_BrokenLinks;
						debugf( TEXT("Broken import: %s %s (file %s)"), *Import.ClassName, *GetImportFullName(i), *Import.SourceLinker->Filename );
						return;
					}
#ifdef __EMSCRIPTEN__
					// Fail soft: the dependency package is present but the referenced object
					// is private/incompatible (an incompatible mod). Mark broken links +
					// message and continue, instead of a throw that Asyncify can't unwind.
					if( Cast<UPackage>(LinkerRoot) )
						Cast<UPackage>(LinkerRoot)->PackageFlags |= PKG_BrokenLinks;
					debugf( NAME_Warning, TEXT("[PACKAGE-LOAD] '%s' references a private/incompatible object: %s %s — skipping (broken link)"), *Import.SourceLinker->Filename, *Import.ClassName, *GetImportFullName(i) );
					return;
#else
					appThrowf( LocalizeError(TEXT("FailedImportPrivate"),TEXT("Core")), *Import.ClassName, *GetImportFullName(i) );
#endif
				}
				Import.SourceIndex = j;
				break;
			}
		}
	}
	if( appStricmp(*Import.ClassName,TEXT("Mesh"))==0 )//oldver
	{
		Import.ClassName=FName(TEXT("LodMesh"));
		goto Rehack;
	}

	// If not found in file, see if it's a public native transient class.
	if( Import.SourceIndex==INDEX_NONE && Pkg!=NULL )
	{
		UObject* ClassPackage = FindObject<UPackage>( NULL, *Import.ClassPackage );
		if( ClassPackage )
		{
			UClass* FindClass = FindObject<UClass>( ClassPackage, *Import.ClassName );
			if( FindClass )
			{
				UObject* FindObject = StaticFindObject( FindClass, Pkg, *Import.ObjectName );
				if
				(	(FindObject)
				&&	(FindObject->GetFlags() & RF_Public)
				&&	(FindObject->GetFlags() & RF_Native)
				&&	(FindObject->GetFlags() & RF_Transient) )
				{
					Import.XObject = FindObject;
					GImportCount++;
				}
				else
				{
					if( GIsEditor && !GIsUCC )
						EdLoadErrorf( FEdLoadError::TYPE_RESOURCE, *GetImportFullName(i) );
					debugf( NAME_Warning, TEXT("Missing %s %s"), FindClass->GetName(), *GetImportFullName(i) );
					SafeReplace = 1;
				}
			}
		}
		if( !Import.XObject && !SafeReplace )
		{
			debugf( TEXT("Failed import: %s %s (file %s)"), *Import.ClassName, *GetImportFullName(i), *Import.SourceLinker->Filename );
			if( LoadFlags & LOAD_Forgiving )
			{
				if( Cast<UPackage>(LinkerRoot) )
					Cast<UPackage>(LinkerRoot)->PackageFlags |= PKG_BrokenLinks;
				debugf( TEXT("Broken import: %s %s (file %s)"), *Import.ClassName, *GetImportFullName(i), *Import.SourceLinker->Filename );
				return;
			}
#ifdef __EMSCRIPTEN__
			// Fail soft: the dependency package is present but the referenced object is
			// missing (an incompatible mod). Mark broken links + message and continue,
			// instead of a throw that Asyncify can't unwind.
			if( Cast<UPackage>(LinkerRoot) )
				Cast<UPackage>(LinkerRoot)->PackageFlags |= PKG_BrokenLinks;
			debugf( NAME_Warning, TEXT("[PACKAGE-LOAD] '%s' is missing referenced content: %s %s — skipping (broken link; package may be an incompatible mod)"), *Import.SourceLinker->Filename, *Import.ClassName, *GetImportFullName(i) );
			return;
#else
			appThrowf( LocalizeError(TEXT("FailedImport"),TEXT("Core")), *Import.ClassName, *GetImportFullName(i) );
#endif
		}
	}
	unguard;
}

// Load all objects; all errors here are fatal.
void ULinkerLoad::LoadAllObjects()
{
	guard(ULinkerLoad::LoadAllObjects);
	for( INT i=0; i<Summary.ExportCount; i++ )
		CreateExport( i );
	unguardobj;
}

// Find the index of a specified object.
//!!without regard to specific package
INT ULinkerLoad::FindExportIndex( FName ClassName, FName ClassPackage, FName ObjectName, INT PackageIndex )
{
	guard(ULinkerLoad::FindExportIndex);
Rehack://oldver
	INT iHash = HashNames( ObjectName, ClassName, ClassPackage ) & (ARRAY_COUNT(ExportHash)-1);
	for( INT i=ExportHash[iHash]; i!=INDEX_NONE; i=ExportMap(i)._iHashNext )
	{
		if
		(  (ExportMap(i).ObjectName  ==ObjectName                              )
		&& (ExportMap(i).PackageIndex==PackageIndex || PackageIndex==INDEX_NONE)
		&& (GetExportClassPackage(i) ==ClassPackage                            )
		&& (GetExportClassName   (i) ==ClassName                               ) )
		{
			return i;
		}
	}

	// If an object with the exact class wasn't found, look for objects with a subclass of the requested class.

	for(INT ExportIndex = 0;ExportIndex < ExportMap.Num();ExportIndex++)
	{
		FObjectExport&	Export = ExportMap(ExportIndex);

		if(Export.ObjectName == ObjectName && (PackageIndex == INDEX_NONE || Export.PackageIndex == PackageIndex))
		{
			UClass*	ExportClass = Cast<UClass>(IndexToObject(Export.ClassIndex));

			// See if this export's class inherits from the requested class.

			for(UClass* ParentClass = ExportClass;ParentClass;ParentClass = ParentClass->GetSuperClass())
			{
				if(ParentClass->GetFName() == ClassName)
					return ExportIndex;
			}
		}
	}

	if( appStricmp(*ClassName,TEXT("Mesh"))==0 )//oldver.
	{
		ClassName = FName(TEXT("LodMesh"));
		goto Rehack;
	}
	return INDEX_NONE;
	unguard;
}

// Create a single object.
UObject* ULinkerLoad::Create( UClass* ObjectClass, FName ObjectName, DWORD LoadFlags, UBOOL Checked )
{
	guard(ULinkerLoad::Create);
	//old:
	//for( INT i=0; i<ExportMap.Num(); i++ )
	//new:
	INT Index = FindExportIndex( ObjectClass->GetFName(), ObjectClass->GetOuter()->GetFName(), ObjectName, INDEX_NONE );
	if( Index!=INDEX_NONE )
		return (LoadFlags & LOAD_Verify) ? (UObject*)-1 : CreateExport(Index);
	if( Checked )
		appThrowf( LocalizeError(TEXT("FailedCreate"),TEXT("Core")), ObjectClass->GetName(), *ObjectName );
	return NULL;
	unguard;
}

void ULinkerLoad::Preload( UObject* Object )
{
	guard(ULinkerLoad::Preload);
	check(IsValid());
	check(Object);
	if( Object->GetFlags() & RF_Preloading )
	{
		// Warning for internal development.
		//debugf( "Object preload reentrancy: %s", Object->GetFullName() );
	}
	if( Object->GetLinker()==this )
	{
		// Preload the object if necessary.
		if( Object->GetFlags() & RF_NeedLoad )
		{
			// If this is a struct, preload its super.
			if(	Object->IsA(UStruct::StaticClass()) )
				if( ((UStruct*)Object)->SuperField )
					Preload( ((UStruct*)Object)->SuperField );

			// Load the local object now.
			guard(LoadObject);
			FObjectExport& Export = ExportMap( Object->_LinkerIndex );
			check(Export._Object==Object);
			INT SavedPos = Loader->Tell();
			Loader->Seek( Export.SerialOffset );
			Loader->Precache( Export.SerialSize );

			// Load the object.
			Object->ClearFlags ( RF_NeedLoad );
			Object->SetFlags   ( RF_Preloading );
			Object->Serialize  ( *this );
			Object->ClearFlags ( RF_Preloading );
//				debugf(TEXT("    %s: %i"), Object->GetFullName(), Export.SerialSize );

			// Make sure we serialized the right amount of stuff.
			if( Tell()-Export.SerialOffset != Export.SerialSize )
				appErrorf( LocalizeError(TEXT("SerialSize"),TEXT("Core")), Object->GetFullName(), Tell()-Export.SerialOffset, Export.SerialSize );
			Loader->Seek( SavedPos );
			unguardf(( TEXT("(%s %i==%i/%i %i %i)"), Object->GetFullName(), Loader->Tell(), Loader->Tell(), Loader->TotalSize(), ExportMap( Object->_LinkerIndex ).SerialOffset, ExportMap( Object->_LinkerIndex ).SerialSize ));
		}
	}
	else if( Object->GetLinker() )
	{
		// Send to the object's linker.
		Object->GetLinker()->Preload( Object );
	}
	unguard;
}

UObject* ULinkerLoad::CreateExport( INT Index )
{
	guard(ULinkerLoad::CreateExport);

	// Map the object into our table.
	FObjectExport& Export = ExportMap( Index );
	if( !Export._Object && (Export.ObjectFlags & _ContextFlags) )
	{
		check(Export.ObjectName!=NAME_None || !(Export.ObjectFlags&RF_Public));

		// Get the object's class.
		UClass* LoadClass = (UClass*)IndexToObject( Export.ClassIndex );
		if( !LoadClass )
			LoadClass = UClass::StaticClass();
		check(LoadClass);
		check(LoadClass->GetClass()==UClass::StaticClass());
		if( (LoadClass->GetFName()==NAME_Camera) || (LoadClass->GetFName()==NAME_PlayerInput) )//oldver
			return NULL;
		Preload( LoadClass );

		// Get the outer object. If that caused the object to load, return it.
		UObject* ThisParent = Export.PackageIndex ? IndexToObject(Export.PackageIndex) : LinkerRoot;
		if( Export._Object )
			return Export._Object;

		// Create the export object.
		Export._Object = StaticConstructObject
		(
			LoadClass,
			ThisParent,
			Export.ObjectName,
			(Export.ObjectFlags & RF_Load) | RF_NeedLoad | RF_NeedPostLoad
		);
		Export._Object->SetLinker( this, Index );
		GObjLoaded.AddItem( Export._Object );
		debugfSlow( NAME_DevLoad, TEXT("    Created export %s"), Export._Object->GetFullName() );

		// If it's a struct or class, set its parent.
		if( Export._Object->IsA(UStruct::StaticClass()) && Export.SuperIndex!=0 )
			((UStruct*)Export._Object)->SuperField = (UStruct*)IndexToObject( Export.SuperIndex );

		// If it's a class, bind it to C++.
		if( Export._Object->IsA( UClass::StaticClass() ) )
			((UClass*)Export._Object)->Bind();
	}
	return Export._Object;
	unguardf(( TEXT("(%s %i)"), *ExportMap(Index).ObjectName, Tell() ));
}

// Return the loaded object corresponding to an import index; any errors are fatal.
UObject* ULinkerLoad::CreateImport( INT Index )
{
	guard(ULinkerLoad::CreateImport);
	FObjectImport& Import = ImportMap( Index );
	if( !Import.XObject )
	{
		if(!Import.SourceLinker)
		{
			BeginLoad();
			VerifyImport(Index);
			EndLoad();
		}

		if(Import.SourceIndex != INDEX_NONE)
		{
			Import.XObject = Import.SourceLinker->CreateExport( Import.SourceIndex );
			GImportCount++;
			debugfSlow(NAME_DevLoad, TEXT("    Created import %s"), Import.XObject->GetFullName());
		}
	}
	return Import.XObject;
	unguard;
}

// Map an import/export index to an object; all errors here are fatal.
UObject* ULinkerLoad::IndexToObject( INT Index )
{
	guard(IndexToObject);
	if( Index > 0 )
	{
		if( !ExportMap.IsValidIndex( Index-1 ) )
			appErrorf( LocalizeError(TEXT("ExportIndex"),TEXT("Core")), Index-1, ExportMap.Num() );			
		return CreateExport( Index-1 );
	}
	else if( Index < 0 )
	{
		if( !ImportMap.IsValidIndex( -Index-1 ) )
			appErrorf( LocalizeError(TEXT("ImportIndex"),TEXT("Core")), -Index-1, ImportMap.Num() );
		return CreateImport( -Index-1 );
	}
	else return NULL;
	unguard;
}

// Detach an export from this linker.
void ULinkerLoad::DetachExport( INT i )
{
	guard(ULinkerLoad::DetachExport);
	FObjectExport& E = ExportMap( i );
	check(E._Object);
	if( !E._Object->IsValid() )
		appErrorf( TEXT("Linker object %s %s.%s is invalid"), *GetExportClassName(i), LinkerRoot->GetName(), *E.ObjectName );
	if( E._Object->GetLinker()!=this )
		appErrorf( TEXT("Linker object %s %s.%s mislinked"), *GetExportClassName(i), LinkerRoot->GetName(), *E.ObjectName );
	if( E._Object->_LinkerIndex!=i )
		appErrorf( TEXT("Linker object %s %s.%s misindexed"), *GetExportClassName(i), LinkerRoot->GetName(), *E.ObjectName );
	ExportMap(i)._Object->SetLinker( NULL, INDEX_NONE );
	unguard;
}

// UObject interface.
void ULinkerLoad::Serialize( FArchive& Ar )
{
	guard(ULinkerLoad::Serialize);
	Super::Serialize( Ar );
	LazyLoaders.CountBytes( Ar );
	unguard;
}

void ULinkerLoad::Destroy()
{
	guard(ULinkerLoad::Destroy);
	debugf( NAME_DevLoad, TEXT("%1.1fms Unloading: %s"), appSeconds() * 1000.0, LinkerRoot->GetFullName() );

	// Detach all lazy loaders.
	DetachAllLazyLoaders( 0 );

	// Detach all objects linked with this linker.
	for( INT i=0; i<ExportMap.Num(); i++ )
		if( ExportMap(i)._Object )
			DetachExport( i );

	// Remove from object manager, if it has been added.
	GObjLoaders.RemoveItem( this );
	if( Loader )
		delete Loader;
	Loader = NULL;

	Super::Destroy();
	unguardobj;
}

// FArchive interface.
void ULinkerLoad::AttachLazyLoader( FLazyLoader* LazyLoader )
{
	guard(ULinkerLoad::AttachLazyLoader);
	checkSlow(LazyLoader->SavedAr==NULL);
	checkSlow(LazyLoaders.FindItemIndex(LazyLoader)==INDEX_NONE);

	LazyLoaders.AddItem( LazyLoader );
	LazyLoader->SavedAr  = this;
	LazyLoader->SavedPos = Tell();

	unguard;
}

void ULinkerLoad::DetachLazyLoader( FLazyLoader* LazyLoader )
{
	guard(ULinkerLoad::DetachLazyLoader);
	checkSlow(LazyLoader->SavedAr==this);

	INT RemovedCount = LazyLoaders.RemoveItem(LazyLoader);
	if( RemovedCount!=1 )
		appErrorf( TEXT("Detachment inconsistency: %i (%s)"), RemovedCount, *Filename );
	LazyLoader->SavedAr = NULL;
	LazyLoader->SavedPos = 0;

	unguard;
}

void ULinkerLoad::DetachAllLazyLoaders( UBOOL Load )
{
	guard(ULinkerLoad::DetachAllLazyLoaders);
	for( INT i=0; i<LazyLoaders.Num(); i++ )
	{
		FLazyLoader* LazyLoader = LazyLoaders( i );
		if( Load )
			LazyLoader->Load();
		LazyLoader->SavedAr  = NULL;
		LazyLoader->SavedPos = 0;
	}
	LazyLoaders.Empty();
	unguard;
}

// FArchive interface.
void ULinkerLoad::Seek( INT InPos )
{
	guard(ULinkerLoad::Seek);
	Loader->Seek( InPos );
	unguard;
}

INT ULinkerLoad::Tell()
{
	guard(ULinkerLoad::Tell);
	return Loader->Tell();
	unguard;
}

INT ULinkerLoad::TotalSize()
{
	guard(ULinkerLoad::TotalSize);
	return Loader->TotalSize();
	unguard;
}

void ULinkerLoad::Serialize( void* V, INT Length )
{
	guard(ULinkerLoad::Serialize);
	Loader->Serialize( V, Length );
	unguard;
}

UBOOL ULinkerLoad::LinksToCode()
{
//	return ContainsCode();

	if ( !ExportMap.Num() )
		return false;

	for (INT i=0;i<ExportMap.Num();i++)
	{
		INT Index = ExportMap(i).ClassIndex;
		
		if (ExportMap(i).ClassIndex < 0)
		{
			Index = -Index-1;
			if (ImportMap(Index).ObjectName == NAME_Function)
				return true;
		}
		else
		{
			if ( ExportMap(Index).ObjectName == NAME_Function)
				return true;
		}
	}
	return false;
}


/*----------------------------------------------------------------------------
	ULinkerSave.
----------------------------------------------------------------------------*/

ULinkerSave::ULinkerSave( UObject* InParent, const TCHAR* InFilename )
:	ULinker( InParent, InFilename )
,	Saver( NULL )
{
	// Create file saver.
	Saver = GFileManager->CreateFileWriter( InFilename, 0, GThrow );
	if( !Saver )
		appThrowf( LocalizeError(TEXT("OpenFailed"),TEXT("Core")) );

	// Set main summary info.
	Summary.Tag           = PACKAGE_FILE_TAG;
	Summary.SetFileVersions( PACKAGE_FILE_VERSION, PACKAGE_FILE_VERSION_LICENSEE );
	Summary.PackageFlags  = Cast<UPackage>(LinkerRoot) ? Cast<UPackage>(LinkerRoot)->PackageFlags : 0;

	// Set status info.
	ArIsSaving     = 1;
	ArIsPersistent = 1;
	ArForEdit      = GIsEditor;
	ArForClient    = 1;
	ArForServer    = 1;

	// Allocate indices.
	ObjectIndices.AddZeroed( UObject::GObjObjects.Num() );
	NameIndices  .AddZeroed( FName::GetMaxNames() );

	// Success.
	Success=1;
}

void ULinkerSave::Destroy()
{
	guard(ULinkerSave::Destroy);
	if( Saver )
		delete Saver;
	Saver = NULL;
	Super::Destroy();
	unguard;
}

// FArchive interface.
INT ULinkerSave::MapName( FName* Name )
{
	guardSlow(ULinkerSave::MapName);
	return NameIndices(Name->GetIndex());
	unguardobjSlow;
}

INT ULinkerSave::MapObject( UObject* Object )
{
	guardSlow(ULinkerSave::MapObject);
	return Object ? ObjectIndices(Object->GetIndex()) : 0;
	unguardobjSlow;
}

void ULinkerSave::Seek( INT InPos )
{
	Saver->Seek( InPos );
}

INT ULinkerSave::Tell()
{
	return Saver->Tell();
}

void ULinkerSave::Serialize( void* V, INT Length )
{
	Saver->Serialize( V, Length );
}

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

