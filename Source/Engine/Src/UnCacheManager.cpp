/*=============================================================================
	UnCacheManager.cpp: Unreal caching system.
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Ron Prestenback
=============================================================================*/
#define DEBUGTEST

#include "EnginePrivate.h"
#include "UnNet.h"
#include "UnLinker.h"

#undef RECORDLOG
#undef DEBUGRECORDLOG

#if 1
#undef PROFILELOG
#define PROFILELOG(typ,sec)				debugf(NAME_Timer, TEXT("Cache hit for type '%s' took %i ms."), TEXT(#typ), (INT)((appSeconds() - sec) * 1000))
#else
#define PROFILELOG(typ,sec)
#endif

#define EPIC_CACHE TEXT("CacheRecords.ucl")
#define CHECKCLASS(var,cls) var && var->IsChildOf(cls::StaticClass()) && var != cls::StaticClass() && !((var->ClassFlags & CLASS_Abstract)||(var->ClassFlags&CLASS_NoCacheExport))
#define RECORDLOG(typ)					debugf(NAME_RecordCache, TEXT("Filling %sList From Cache"), TEXT(#typ))
#define DEBUGRECORDLOG(typ, name, var)	debugf(NAME_RecordCache, TEXT("Not adding %sRecord %s to cache because it has no '%s'"), TEXT(#typ), name, TEXT(#var))
#define RECORDINVALID(name)				debugf(NAME_RecordCache, TEXT("Removing cache entry for %s: Package wasn't found!"), name)
#define SAVELOG(file)					debugf(NAME_RecordCache, TEXT("Unable to update cache file '%s'!  Error while saving file."), file)

#define LOGEXPORT(typ,name)				GWarn->Logf(TEXT("%s exported successfully: %s"), TEXT(#typ), name)
#define VERIFY_BASE_LIST(base,list) ( base && base != this && !base->list )

IMPLEMENT_CLASS(UCacheManager);

static UCacheManager* Cache=NULL;

static bool bReverseSort = false;
static INT Compare( FString& A, FString& B )
{
	return appStrcmp( *A, *B ) * (bReverseSort ? -1 : 1);
}

static void StripBraces( FString& Text )
{
	guard(StripBraces);

	if ( Text.Left(1) == TEXT("(") )
		Text = Text.Mid(1);

	if ( Text.Right(1) == TEXT(")") )
		Text = Text.LeftChop(1);

	unguard;
}

UCacheManager* UCacheManager::GetCache()
{
	guard(UCacheManager::GetCache);

	if ( Cache == NULL )
	{
		Cache = new UCacheManager;
		Cache->AddToRoot();
	}

	return Cache;
	unguard;
}

// UnrealScript stubs
void UCacheManager::execInitCache( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execInitCache);

	P_FINISH;
	DOUBLE Time = appSeconds();

	GetCache()->InitCache();
	PROFILELOG(Full,Time);

	unguardexec;
}

void UCacheManager::execGetGameTypeList( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execGetGameTypeList);

	P_GET_TARRAY_REF( GameRecords, FGameRecord );
	P_FINISH;

	UCacheManager* C = GetCache();

	DOUBLE Time = appSeconds();
	GameRecords->Empty();
	if (!C->CacheGameTypes.Num())
	{
		TCHAR Key[5] = TEXT("Game");
		C->InitCache( Key );
	}
	else RECORDLOG(GameTypes);
	PROFILELOG(Game,Time);

	*GameRecords = C->CacheGameTypes;

	unguardexec;
}

void UCacheManager::execGetMutatorList( FFrame& Stack, RESULT_DECL )
{
    guard(UCacheManager::execGetMutatorList);

    P_GET_TARRAY_REF( MutatorRecords, FMutatorRecord );
    P_FINISH;

	UCacheManager* C = GetCache();

	DOUBLE Time = appSeconds();
	MutatorRecords->Empty();
	if (!C->CacheMutators.Num())
	{
		TCHAR Key[8] = TEXT("Mutator");
        C->InitCache( Key );
	}
	else RECORDLOG(Mutators);

	PROFILELOG(Mutator,Time);
	*MutatorRecords = C->CacheMutators;

    unguardexec;
}

void UCacheManager::execGetMapList( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execGetMapList);

	P_GET_TARRAY_REF( MapRecords, FMapRecord );
	P_GET_STR_OPTX( Acronym, TEXT("") );
	P_FINISH;

	UCacheManager* C = GetCache();
	DOUBLE Time = appSeconds();
	MapRecords->Empty();
	if (!C->CacheMaps.Num())
	{
		TCHAR Key[4] = TEXT("Map");
		C->InitCache( Key );
	}
	else RECORDLOG(Maps);

	if ( Acronym != TEXT("") )
	{
		for (INT i = 0; i < C->CacheMaps.Num(); i++)
		{
			if (C->CacheMaps(i).Acronym == Acronym)
			{
				INT j = MapRecords->AddZeroed();
				(*MapRecords)(j) = C->CacheMaps(i);
			}
		}
	}
	else *MapRecords = C->CacheMaps;
	PROFILELOG(Map,Time);

	unguardexec;
}

void UCacheManager::execGetWeaponList( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execGetWeaponList);

	P_GET_TARRAY_REF( WeaponRecords, FWeaponRecord );
	P_FINISH;

	UCacheManager* C = GetCache();

	DOUBLE Time = appSeconds();
	WeaponRecords->Empty();
	if (!C->CacheWeapons.Num())
	{
		TCHAR Key[7] = TEXT("Weapon");
		C->InitCache( Key );
	}
	else RECORDLOG(Weapons);

	*WeaponRecords = C->CacheWeapons;
	PROFILELOG(Weapon,Time);
	unguardexec;
}

void UCacheManager::execGetVehicleList( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execGetVehicleList);

	P_GET_TARRAY_REF( VehicleRecords, FVehicleRecord );
	P_FINISH;

	UCacheManager* C = GetCache();

	DOUBLE Time = appSeconds();
	VehicleRecords->Empty();

	if ( !C->CacheVehicles.Num() )
	{
		TCHAR Key[8] = TEXT("Vehicle");
		C->InitCache( Key );
	}
	else RECORDLOG(Vehicles);

	*VehicleRecords = C->CacheVehicles;
	PROFILELOG(Vehicle,Time);
	unguardexec;
}

void UCacheManager::execGetCrosshairList( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execGetCrosshairList);

	P_GET_TARRAY_REF( CrosshairRecords, FCrosshairRecord );
	P_FINISH;

	UCacheManager* C = GetCache();

	CrosshairRecords->Empty();

	DOUBLE Time = appSeconds();
	if (!C->CacheCrosshairs.Num())
	{
		TCHAR Key[10] = TEXT("Crosshair");
		C->InitCache( Key );
	}
	else RECORDLOG(Crosshair);

	*CrosshairRecords = C->CacheCrosshairs;
	PROFILELOG(Crosshair,Time);
	unguardexec;
}

void UCacheManager::execGetAnnouncerList( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execGetCrosshairList);

	P_GET_TARRAY_REF( AnnouncerRecords, FAnnouncerRecord );
	P_FINISH;

	UCacheManager* C = GetCache();

	AnnouncerRecords->Empty();

	DOUBLE Time = appSeconds();
	if ( !C->CacheAnnouncers.Num() )
	{
		TCHAR Key[10] = TEXT("Announcer");
		C->InitCache( Key );
	}
	else RECORDLOG(Announcer);

	*AnnouncerRecords = C->CacheAnnouncers;
	PROFILELOG(Crosshair,Time);

	unguardexec;
}

void UCacheManager::execGetGameRecord( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execGetGameRecord);

	P_GET_STR(Str);
	P_FINISH;

	GetCache()->GetGameRecord(*Str, *(FGameRecord*)Result);
	unguardexec;
}

void UCacheManager::execGetMapRecord( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execGetMapRecord);

	P_GET_STR( Str );
	P_FINISH;

	GetCache()->GetMapRecord(*Str,*(FMapRecord*)Result);
	unguardexec;
}

void UCacheManager::execGetMutatorRecord( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execGetMutatorRecord);

	P_GET_STR(Str);
	P_FINISH;

	GetCache()->GetMutatorRecord(*Str,*(FMutatorRecord*)Result);

	unguardexec;
}

void UCacheManager::execGetVehicleRecord( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execGetVehicleRecord);

	P_GET_STR(Str);
	P_FINISH;

	GetCache()->GetVehicleRecord(*Str,*(FVehicleRecord*)Result);
	unguardexec;
}

void UCacheManager::execGetWeaponRecord( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execGetWeaponRecord);

	P_GET_STR(Str);
	P_FINISH;

	GetCache()->GetWeaponRecord(*Str,*(FWeaponRecord*)Result);
	unguardexec;
}

void UCacheManager::execGetAnnouncerRecord( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execGetAnnouncerRecord);

	P_GET_STR(Str);
	P_FINISH;

	GetCache()->GetAnnouncerRecord(*Str,*(FAnnouncerRecord*)Result);
	unguardexec;
}

UBOOL UCacheManager::GetGameRecord( const TCHAR* ClassName, FGameRecord& Record )
{
	guard(UCacheManager::GetGameRecord);

	if (!CacheGameTypes.Num())
	{
		TCHAR Key[5] = TEXT("Game");
		InitCache( Key );
	}
	else RECORDLOG(GameType);

	for (INT i = 0; i < CacheGameTypes.Num(); i++)
	{
		if (CacheGameTypes(i).ClassName == ClassName)
		{
			Record = CacheGameTypes(i);
			return 1;
		}
	}

	appMemzero( &Record, sizeof(FGameRecord) );
	return 0;

	unguard;
}

UBOOL UCacheManager::GetMapRecord( const TCHAR* MapName, FMapRecord& Record )
{
	guard(UCacheManager::GetMapRecord);

	if (!CacheMaps.Num())
	{
		TCHAR Key[4] = TEXT("Map");
		InitCache( Key );
	}
	else RECORDLOG(Map);

	for (INT i = 0; i < CacheMaps.Num(); i++)
	{
		if (CacheMaps(i).MapName == MapName)
		{
			Record = CacheMaps(i);
			return 1;
		}
	}

	appMemzero( &Record, sizeof(FMapRecord) );
	return 0;

	unguard;
}

UBOOL UCacheManager::GetMutatorRecord( const TCHAR* ClassName, FMutatorRecord& Record )
{
	guard(UCacheManager::GetMutatorRecord);

	if ( !CacheMutators.Num() )
	{
		TCHAR Key[8] = TEXT("Mutator");
		InitCache(Key);
	}
	else RECORDLOG(Mutator);

	for ( INT i = 0; i < CacheMutators.Num(); i++ )
	{
		if ( CacheMutators(i).ClassName == ClassName )
		{
			Record = CacheMutators(i);
			return 1;
		}
	}

	appMemzero(&Record,sizeof(FMutatorRecord));
	return 0;

	unguard;
}

UBOOL UCacheManager::GetWeaponRecord( const TCHAR* ClassName, FWeaponRecord& Record )
{
	guard(UCacheManager::GetWeaponRecord);

	if ( !CacheWeapons.Num() )
	{
		TCHAR Key[8] = TEXT("Weapon");
		InitCache(Key);
	}
	else RECORDLOG(Weapon);

	for ( INT i = 0; i < CacheWeapons.Num(); i++ )
	{
		if ( CacheWeapons(i).ClassName == ClassName )
		{
			Record = CacheWeapons(i);
			return 1;
		}
	}

	appMemzero(&Record,sizeof(FWeaponRecord));
	return 0;
	unguard;
}

UBOOL UCacheManager::GetVehicleRecord( const TCHAR* ClassName, FVehicleRecord& Record )
{
	guard(UCacheManager::GetVehicleRecord);

	if ( !CacheVehicles.Num() )
	{
		TCHAR Key[9] = TEXT("Vehicle");
		InitCache(Key);
	}
	else RECORDLOG(Vehicle);

	for ( INT i = 0; i < CacheVehicles.Num(); i++ )
	{
		if ( CacheVehicles(i).ClassName == ClassName )
		{
			Record = CacheVehicles(i);
			return 1;
		}
	}

	appMemzero( &Record, sizeof(FVehicleRecord) );

	return 0;
	unguard;
}

UBOOL UCacheManager::GetAnnouncerRecord( const TCHAR* ClassName, FAnnouncerRecord& Record )
{
	guard(UCacheManager::GetAnnouncerRecord);

	if ( !CacheAnnouncers.Num() )
	{
		TCHAR Key[10] = TEXT("Announcer");
		InitCache(Key);
	}
	else RECORDLOG(Announcer);

	for ( INT i = 0; i < CacheAnnouncers.Num(); i++ )
	{
		if ( CacheAnnouncers(i).ClassName == ClassName )
		{
			Record = CacheAnnouncers(i);
			return 1;
		}
	}

	appMemzero( &Record, sizeof(FAnnouncerRecord) );

	return 0;
	unguard;
}

void UCacheManager::execIs2003Content( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execIs2003Content);

	P_GET_STR(Item);
	P_FINISH;

	if ( DefaultContent.Num() )
	{
		if ( DefaultContent(0).Maps.FindItemIndex( Item ) != INDEX_NONE )
		{
			*(DWORD*)Result = 1;
			return;
		}

		if ( DefaultContent(0).Classes.FindItemIndex( Item ) != INDEX_NONE )
		{
			*(DWORD*)Result = 1;
			return;
		}
	}

	*(DWORD*)Result = 0;
	unguardexec;
}

void UCacheManager::execIsBPContent( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execIsBPContent);

	P_GET_STR(Item);
	P_FINISH;

	if ( DefaultContent.Num() > 1 )
	{
		if ( DefaultContent(1).Maps.FindItemIndex( Item ) != INDEX_NONE )
		{
			*(DWORD*)Result = 1;
			return;
		}

		if ( DefaultContent(1).Classes.FindItemIndex( Item ) != INDEX_NONE )
		{
			*(DWORD*)Result = 1;
			return;
		}
	}

	*(DWORD*)Result = 0;
	unguardexec;
}

void UCacheManager::execIs2004Content( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execIs2004Content);

	P_GET_STR(Item);
	P_FINISH;

	if ( DefaultContent.Num() > 2)
	{
		for ( INT i = 2; i < DefaultContent.Num(); i++ )
		{
			if ( DefaultContent(i).Maps.FindItemIndex(Item) != INDEX_NONE )
			{
				*(DWORD*)Result = 1;
				return;
			}

			if ( DefaultContent(i).Classes.FindItemIndex( Item ) != INDEX_NONE )
			{
				*(DWORD*)Result = 1;
				return;
			}
		}
	}

	*(DWORD*)Result = 0;

	unguardexec;
}

void UCacheManager::execIsDefaultContent( FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execIsDefaultContent);

	P_GET_STR(Item);
	P_FINISH;

	*(UBOOL*)Result = IsDefaultMap(*Item) || IsDefaultPackage(*Item);
	unguardexec;
}

void UCacheManager::execGetTeamSymbolList(FFrame& Stack, RESULT_DECL )
{
	guard(UCacheManager::execGetTeamSymbolList);

	P_GET_TARRAY_REF(SymbolNames, FString);
	P_GET_UBOOL_OPTX(bNoSinglePlayer, 0);
	P_FINISH;

	SymbolNames->Empty();

	TArray<FRegistryObjectInfo> RegList;	
	GetRegistryObjects( RegList, UTexture::StaticClass(), NULL, 0 );
	
	for(INT i=0; i<RegList.Num(); i++)
	{
		FRegistryObjectInfo* Info = &RegList(i);

		// Only pick up 'TeamSymbol's
		if( appStrncmp(*Info->Object, TEXT("TeamSymbols_"), 12) != 0 )
			continue;

		// Ignore 'single player' symbols if desired
		if( bNoSinglePlayer && appStrstr( *Info->Description, TEXT("SP") ) )
			continue;

		// Add team symbol name to array, making sure all UT2004 team symbols appear first
		if ( Info->Object.Left(Info->Object.InStr(TEXT("."))) == TEXT("TeamSymbols_UT2004") )
		{
			SymbolNames->InsertZeroed(0,1);
			(*SymbolNames)(0) = Info->Object;
		}
		else new(*SymbolNames) FString(Info->Object);
	}

	unguardexec;
}

void UCacheManager::InitCacheManager( DWORD Flags )
{
	guard(UCacheManager::InitCacheManager);

	if ( this == GetClass()->GetDefaultObject() )
	{
		GetCache()->InitCacheManager(Flags);
		return;
	}

	VERIFY_CLASS_SIZE(UCacheManager)

	FileManager = CreateFileManager( Flags );
	Tracker = CreateTracker();

	if (Tracker)
		Tracker->InitRegObjects();

	if ( FileManager )
		FileManager->Init();

	unguard;
}

void UCacheManager::InitCache( const TCHAR* CacheKey )
{
	guard(UCacheManager::InitCache);

	if ( this == GetClass()->GetDefaultObject() )
	{
		GetCache()->InitCache(CacheKey);
		return;
	}
 
// Check whether this cache has been initialized previously
	if ( Initialized( CacheKey ) )
		return;

// Create helpers
	DOUBLE Time = appSeconds();
	if ( !Tracker || !FileManager )
	{
		InitCacheManager( EXPORT_Append );
		debugf(NAME_RecordCache, TEXT("Initialization of Tracker & FileManager took %f ms"), (appSeconds() - Time)*1000);
	}

	check(Tracker);
	check(FileManager);
	
// Find all packages and filter them into our tracking lists
	if ( !Tracker->Initialized() )
	{
		ScanDirectories( Tracker );
		debugf(NAME_RecordCache, TEXT("ScanDirectories took %f ms"), (appSeconds() - Time) * 1000);
	}

// Parse each .ucl file and add the entries to the game's cache,  as each cache entry is added to cache
// it is culled from the list of packages that were found when the Tracker was initialized
// This is where I determine which packages are new, and which packages no longer exist

	Time = appSeconds();
	ParseCacheFiles( CacheKey );
	debugf(NAME_RecordCache, TEXT("Parsing took %f ms"), (appSeconds() - Time) * 1000);

// If any newly found cache entries were found while parsing, save those lines to .ucl files for fast retrieval

	Time = appSeconds();
	SaveNewPackages( Tracker, FileManager, CacheKey );
	debugf(NAME_RecordCache, TEXT("Saving took %f ms"), (appSeconds() - Time) * 1000);

// Now add any newly created cache entries to the game.
	IntegrateNewPackages( CacheKey );
	unguard;
}

UBOOL UCacheManager::Initialized( const TCHAR* CacheKey ) const
{
	guard(UCacheManager::Initialized);

	if ( ((void *) this) == ((void *) GetClass()->GetDefaultObject()) )
		return GetCache()->Initialized(CacheKey);

	if ( CacheKey )
	{
		if ( IsMapRecord( CacheKey ) )
			return CacheMaps.Num();

		if ( IsGameRecord( CacheKey ) )
			return CacheGameTypes.Num();

		if ( IsMutatorRecord( CacheKey ) )
			return CacheMutators.Num();

		if ( IsCrosshairRecord( CacheKey ) )
			return CacheCrosshairs.Num();

		if ( IsWeaponRecord(CacheKey) )
			return CacheWeapons.Num();

		if ( IsVehicleRecord(CacheKey) )
			return CacheVehicles.Num();

		if ( IsAnnouncerRecord(CacheKey) )
			return CacheAnnouncers.Num();
	}

	// If any groups were initialized individually, all remaining groups must also be initialized individually
	return CacheMaps.Num() || CacheGameTypes.Num() || CacheMutators.Num() || CacheCrosshairs.Num() || CacheWeapons.Num() || CacheVehicles.Num() || CacheAnnouncers.Num();
	unguard;
}

FCacheTracker* UCacheManager::CreateTracker() const
{
	guard(UCacheManager::CreateTracker);

	return new FCacheTracker;

	unguard;
}

FCacheFileManager* UCacheManager::CreateFileManager( DWORD Flags ) const
{
	guard(UCacheManager::CreateFileManager);

	return new FCacheFileManager(Flags);
	unguard;
}

// Called at level change to detect any new packages
// Returns true if cache state was modified
UBOOL UCacheManager::RefreshCache()
{
	guard(UCacheManager::RefreshCache);

	if ( this == GetClass()->GetDefaultObject() )
	{
		return GetCache()->RefreshCache();
	}

	// Haven't been initialized yet
	if ( Tracker == NULL || FileManager == NULL )
		return 0;

	// TODO
	// Ideally, I'd like to avoid reparsing the .ucl files if no packages have been added or removed
	// The existing code below resets the entire caching system

	// Clear all tracking info
	Tracker->Reset();
	FileManager->Reset();

	// Empty all records
	CacheMutators.Empty();
	CacheMaps.Empty();
	CacheWeapons.Empty();
	CacheVehicles.Empty();
	CacheGameTypes.Empty();
	CacheAnnouncers.Empty();
	CacheCrosshairs.Empty();

	return 1;

	unguard;
}

void UCacheManager::ScanDirectories( FCacheTracker* const InTracker )
{
	guard(UCacheManager::ScanDirectories);

	if ( this == GetClass()->GetDefaultObject() )
	{
		GetCache()->ScanDirectories(InTracker);
		return;
	}

	// Search all paths, looking for maps.  Add any that are found to the CacheRemaining array - we'll weed them out later
	for( INT DoCD=0; DoCD<1+(GCdPath[0]!=0); DoCD++ )
	{
		for( INT i = 0; i < GSys->Paths.Num(); i++ )
		{
			if( ContainsMaps(GSys->Paths(i)) )
			{
				TCHAR Tmp[256]=TEXT("");
				if( DoCD )
				{
					appStrcat( Tmp, GCdPath );
					appStrcat( Tmp, TEXT("System") PATH_SEPARATOR );
				}
				appStrcat( Tmp, *GSys->Paths(i) ); 
				TArray<FString>	TheseNames = GFileManager->FindFiles(Tmp,1,0);
	
				for( INT j = TheseNames.Num() - 1; j >= 0; j--)
				{
					if ( InTracker->IsValidMap(TheseNames(j)) )
					{
						InTracker->FoundUnregistered( TEXT("Map"), TheseNames(j) );
						InTracker->DetectedPackage( TheseNames(j) );
					}
				}
			}

			else if ( ContainsPackages(GSys->Paths(i)) )
			{
				TCHAR Tmp[256]=TEXT("");
				if( DoCD )
				{
					appStrcat( Tmp, GCdPath );
					appStrcat( Tmp, TEXT("System") PATH_SEPARATOR );
				}
				appStrcat( Tmp, *GSys->Paths(i) ); 
				TArray<FString>	TheseNames = GFileManager->FindFiles(Tmp,1,0);
	
				for( INT j = TheseNames.Num() - 1; j >= 0; j--)
					if ( InTracker->IsValidPackage(TheseNames(j)) )
						InTracker->DetectedPackage( TheseNames(j) );
			}
		}
	}

	unguard;
}

UBOOL UCacheManager::ParseCacheFiles( const TCHAR* CacheKey )
{
	guard(UCacheManager::ParseCacheFiles);

	if ( this == GetClass()->GetDefaultObject() )
		return GetCache()->ParseCacheFiles(CacheKey);

	// Find all .ucl files and load the entries into cache.
	TArray<FString> Lines;
	FileManager->GetCacheLines( Lines, CacheKey );

	// Ensures that items are always imported in the same order

	if ( Lines.Num() )
	{
		bReverseSort = true;
		Sort( &Lines(0), Lines.Num() );
		bReverseSort = false;
	}

	UBOOL Result;

	for ( INT i = Lines.Num() - 1; i >= 0; i-- )
    {
		INT Pos = Lines(i).InStr(TEXT("="));
		if ( Pos == INDEX_NONE )
			continue;

		FString Key = Lines(i).Left(Pos);
		FString Value = Lines(i).Mid(Pos+1);
		StripBraces(Value);

		Result = 0;

		if (      IsMapRecord(       *Key ) ) Result = ParseMap(       *Value );
		else if ( IsGameRecord(      *Key ) ) Result = ParseGame(      *Value );
		else if ( IsWeaponRecord(    *Key ) ) Result = ParseWeapon(    *Value );
		else if ( IsMutatorRecord(   *Key ) ) Result = ParseMutator(   *Value );
		else if ( IsCrosshairRecord( *Key ) ) Result = ParseCrosshair( *Value ); 
		else if ( IsVehicleRecord(   *Key ) ) Result = ParseVehicle(   *Value );
		else if ( IsAnnouncerRecord( *Key ) ) Result = ParseAnnouncer( *Value );

		if ( !Result && RemoveCacheEntry( *Lines(i) ) )
			Lines.Remove(i);
	}

	return Lines.Num();
	unguard;
}

void UCacheManager::SaveNewPackages( FCacheTracker* InTracker, FCacheFileManager* InFileManager, const TCHAR* CacheKey, const FString* DestFile )
{
	guard(UCacheManager::SaveNewPackages);

#if !DEMOVERSION

	if ( this == GetClass()->GetDefaultObject() )
	{
		GetCache()->SaveNewPackages(InTracker,InFileManager,CacheKey,DestFile);
		return;
	}

	check(InTracker);
	check(InFileManager);

	// This is required for the exportcache commandlet
	// When the exportcache commandlet calls SaveNewPackages(), the CacheManager's Tracker && FileManager properties
	// are NULL
	// Several of the methods that will be called here require that the CacheManager has valid values for these properties,
	// however, so assign them here.
	// Since the CacheManager is an abstract class, Destroy() will never be called, thus FileManager and Tracker must be deleted
	// from CacheManager's destructor, rather than Destroy().
	// This is a problem when we're exporting stuff via the commandlet, though, since the commandlet will delete the FileManager and Tracker
	// it created
	FCacheTracker* OldTracker = Tracker;
	FCacheFileManager* OldFileManager = FileManager;

	Tracker = InTracker;
	FileManager = InFileManager;

	TArray<FString> NewMaps, NewItems, Result;

	// If we are loading a specific cache group
	if ( CacheKey )
	{
		if ( IsMapRecord(CacheKey) )
			Tracker->GetNewItems( TEXT("Map"), NewMaps, 1 );
		else Tracker->GetNewItems( CacheKey, NewItems );
	}
	else
	{
		// Otherwise, get all items
		Tracker->GetNewItems(TEXT("Map"), NewMaps, 1);
		Tracker->GetNewItems(NULL,NewItems);
	}

	Result.Empty();
	FString FileName;

	guard(NewMaps);

	// Save the newly found maps to the appropriate .ucl file.
	if (NewMaps.Num())
	{
		for ( INT i = NewMaps.Num() - 1; i >= 0; i-- )
		{
			if ( IsDefaultMap(*NewMaps(i)) && !DestFile )
				continue;

			CreateCacheFileName(DestFile ? *DestFile : NewMaps(i), FileName);
			ExportCacheMaps( NewMaps(i), *FileName, Result );
			NewMaps.Remove(i);
		}

		if ( NewMaps.Num() )	// Epic maps
			ExportCacheMaps( NewMaps, EPIC_CACHE, Result );
	}
	unguard;

	guard(NewItems);

	if ( NewItems.Num() )
	{
		for ( INT i = 0; i <NewItems.Num(); i++ )
		{
			if ( IsDefaultPackage(*NewItems(i)) && !DestFile)
				continue;

			CreateCacheFileName( DestFile ? *DestFile : NewItems(i), FileName );
			ExportCachePackages( NewItems(i), *FileName, Result );
			NewItems.Remove(i--);
		}

		if ( NewItems.Num() )	// Epic packages
			ExportCachePackages( NewItems, EPIC_CACHE, Result );
	}

	unguard;

	for ( INT i = 0; i < Result.Num(); i++ )
	{
		FString Left, Right;
		Result(i).Split( TEXT("="), &Left, &Right );
		StripBraces( Right );
		Tracker->PostNewCache( Left, Right );
	}

	FileManager->Close();
	FileManager = OldFileManager;
	Tracker = OldTracker;

#endif

	unguard;
}

void UCacheManager::IntegrateNewPackages( const TCHAR* CacheKey )
{
	guard(UCacheManager::IntegrateNewPackages);

#if !DEMOVERSION
	if ( this == GetClass()->GetDefaultObject() )
	{
		GetCache()->IntegrateNewPackages(CacheKey);
		return;
	}

	TArray<FString> Items;

	if ( IsMapRecord(CacheKey) )
	{
		Tracker->GetPostedCache( TEXT("Map"), Items );
		for ( INT i = 0; i < Items.Num(); i++ )
			ParseMap( *Items(i) );
	}

	if ( IsGameRecord(CacheKey) )
	{
		Items.Empty();
		Tracker->GetPostedCache( TEXT("Game"), Items );
		for ( INT i = 0; i < Items.Num(); i++ )
			ParseGame( *Items(i) );
	}

	if ( IsWeaponRecord(CacheKey) )
	{
		Items.Empty();
		Tracker->GetPostedCache( TEXT("Weapon"), Items );
		for ( INT i = 0; i < Items.Num(); i++ )
			ParseWeapon( *Items(i) );
	}

	if ( IsMutatorRecord(CacheKey) )
	{
		Items.Empty();
		Tracker->GetPostedCache( TEXT("Mutator"), Items );
		for ( INT i = 0; i < Items.Num(); i++ )
			ParseMutator( *Items(i) );
	}

	if ( IsCrosshairRecord(CacheKey) )
	{
		Items.Empty();
		Tracker->GetPostedCache( TEXT("Crosshair"), Items );
		for ( INT i = 0; i < Items.Num(); i++ )
			ParseCrosshair( *Items(i) );
	}

	if ( IsVehicleRecord(CacheKey) )
	{
		Items.Empty();
		Tracker->GetPostedCache( TEXT("Vehicle"), Items );
		for ( INT i = 0; i < Items.Num(); i++ )
			ParseVehicle( *Items(i) );
	}

	if ( IsAnnouncerRecord(CacheKey) )
	{
		Items.Empty();
		Tracker->GetPostedCache( TEXT("Announcer"), Items );
		for ( INT i = 0; i < Items.Num(); i++ )
			ParseAnnouncer( *Items(i) );
	}

#endif
	unguard;
}

UBOOL UCacheManager::ParseMap( const TCHAR* Value )
{
	guard( UCacheManager::ParseMap );

	FMapRecord Record;

    if (!Parse( Value, TEXT("MapName="), Record.MapName ))
		return 0;

	if ( Tracker && !Tracker->RegisterCacheItem(Record.MapName) )
		return 0;

	Parse( Value, TEXT("Acronym="), Record.Acronym );
	Parse( Value, TEXT("Author="), Record.Author );
	Parse( Value, TEXT("PlayerCountMin="), Record.PlayerCountMin );
	Parse( Value, TEXT("PlayerCountMax="), Record.PlayerCountMax );
	Parse( Value, TEXT("TextName="), Record.TextName );
	Parse( Value, TEXT("Screenshot="), Record.ScreenshotRef );
	Parse( Value, TEXT("ExtraInfo="), Record.ExtraInfo );

	if (Record.Acronym == TEXT(""))
	{
		DEBUGRECORDLOG(Map,*Record.MapName,Acronym);
		return 0;
	}

	if (Record.Acronym == TEXT("UT2") )
	{
		debugf(NAME_RecordCache, TEXT("Not adding %s to cache because it has an invalid acronym: '%s'"),*Record.MapName,*Record.Acronym);
		return 0;
	}

	FString tmp=TEXT("");

	// Try to load the Name and Description using localization first
	// If that fails, use the fallback text, if it exists - this is normally the value of the property as specified in defaultproperties
	if ( Parse( Value, TEXT("FriendlyName="), tmp ) )
	{
		TArray<FString> Arr;
		tmp.ParseIntoArray(TEXT("."), &Arr);
		if ( Arr.Num() > 2 )
			Record.FriendlyName = Localize( *(Arr(1)), *(Arr(2)), *(Arr(0)), UObject::GetLanguage(), 1);
	}

	if ( Parse(Value, TEXT("Description="), tmp) )
	{
		TArray<FString> Arr;
		tmp.ParseIntoArray( TEXT("."), &Arr);
		if ( Arr.Num() > 2 )
			Record.Description = Localize( *(Arr(1)), *(Arr(2)), *(Arr(0)), UObject::GetLanguage(), 1 );
	}

	// Use fallbacks if no localized versions exist
	if ( Record.FriendlyName.Len() == 0 )
		Parse(Value, TEXT("FallbackName="), Record.FriendlyName);

	if ( Record.Description.Len() == 0 )
		Parse(Value, TEXT("FallbackDesc="), Record.Description);

	INT i = CacheMaps.AddZeroed();
	Record.RecordIndex = i;
	CacheMaps(i) = Record;

	return 1;
	unguard;
}

UBOOL UCacheManager::ParseMutator( const TCHAR* Value )
{
	guard(UCacheManager::ParseMutator);

	FMutatorRecord Record;

	if ( !Parse(Value, TEXT("ClassName="), Record.ClassName) )
		return 0;

	if ( Tracker && !Tracker->RegisterCacheItem(Record.ClassName) )
		return 0;

	Parse(Value, TEXT("IconMaterialName="), Record.IconMaterialName );
	Parse(Value, TEXT("ConfigMenuClassName="), Record.ConfigMenuClassName);
	Parse(Value, TEXT("GroupName="), Record.GroupName);

	FString tmp=TEXT("");

	// Try to load the Name and Description using localization first
	// If that fails, use the fallback text, if it exists - this is normally the value of the property as specified in defaultproperties
	if ( Parse(Value, TEXT("FriendlyName="), tmp) )
	{
		TArray<FString> Arr;

		tmp.ParseIntoArray( TEXT("."), &Arr);
		if ( Arr.Num() > 2 )
			Record.FriendlyName = Localize( *(Arr(1)), *(Arr(2)), *(Arr(0)), UObject::GetLanguage(),1  );
	}

	if ( Parse(Value, TEXT("Description="), tmp) )
	{
		TArray<FString> Arr;
		tmp.ParseIntoArray( TEXT("."), &Arr);
		if ( Arr.Num() > 2 )
			Record.Description = Localize( *(Arr(1)), *(Arr(2)), *(Arr(0)), UObject::GetLanguage(),1  );
	}


	if ( Record.FriendlyName.Len() == 0 )
		Parse(Value, TEXT("FallbackName="), Record.FriendlyName);

	if ( Record.Description.Len() == 0 )
		Parse( Value, TEXT("FallbackDesc="), Record.Description);

	INT i = CacheMutators.AddZeroed();
	Record.RecordIndex = i;
	CacheMutators(i) = Record;
	return 1;
	unguard;
}

UBOOL UCacheManager::ParseCrosshair( const TCHAR* Value )
{
	guard(UCacheManager::ParseCrosshair);

	FCrosshairRecord Record;

	TCHAR tmp [NAME_SIZE];

	if ( !Parse( Value, TEXT("FriendlyName="), Record.FriendlyName ) )
		return 0;

	if (Record.FriendlyName.Len() == 0)
		return 0;

	if ( Parse(Value, TEXT("CrosshairTexture="), tmp, ARRAY_COUNT(tmp)) )
		Record.CrosshairTexture = Cast<UTexture>(UObject::StaticLoadObject( UTexture::StaticClass(), NULL, tmp, NULL, 0, NULL ));
	else return 0;

	if (!Record.CrosshairTexture)
	{
		RECORDINVALID(tmp);
		return 0;
	}

	INT i = CacheCrosshairs.AddZeroed();
	Record.RecordIndex = i;
	CacheCrosshairs(i) = Record;
	return 1;
	unguard;
}

UBOOL UCacheManager::ParseGame( const TCHAR* Value )
{
	guard(UCacheManager::ParseGame);

	FGameRecord Record;

	Parse(Value, TEXT("ClassName="),		Record.ClassName);
	if ( Tracker && !Tracker->RegisterCacheItem(Record.ClassName) )
		return 0;

	Parse(Value, TEXT("GameAcronym="),		Record.GameAcronym);
	Parse(Value, TEXT("MapListClassName="),	Record.MapListClassName);
	Parse(Value, TEXT("MapPrefix="),		Record.MapPrefix);
	Parse(Value, TEXT("TextName="),			Record.TextName);
	Parse(Value, TEXT("Screenshot="),		Record.ScreenshotRef);
	Parse(Value, TEXT("HUDMenu="),          Record.HUDMenu);
	Parse(Value, TEXT("RulesMenu="),        Record.RulesMenu);

	INT i;
	// Test whether this gametype is among the epic gametypes, and set the GameTypeGroup accordingly
	for (i = 0; i < 3; i++)
	{
		if (DefaultContent(i).Classes.FindItemIndex(Record.ClassName) != INDEX_NONE)
			break;
	}

	Record.GameTypeGroup = i;
	if (Parse(Value, TEXT("TeamGame="), i))
		Record.bTeamGame = i;

	// If this gametype doesn't have an acronym, do not allow it to appear in the menus.
	if (Record.GameAcronym == TEXT("") || Record.GameAcronym == TEXT("???"))
	{
		DEBUGRECORDLOG(GameType,*Record.ClassName,GameAcronym);
		return 0;
	}

	// If this gametype doesn't have a maplist type, do not allow it to appear in the menus.
	if (Record.MapListClassName == TEXT(""))
	{
		DEBUGRECORDLOG(GameType,*Record.ClassName,GameName);
		return 0;
	}

	// Try to load the Name and Description using localization first
	// If that fails, use the fallback text, if it exists - this is normally the value of the property as specified in defaultproperties
	FString tmp;
	if (Parse(Value, TEXT("GameName="), tmp))
	{
		TArray<FString> Arr;
		tmp.ParseIntoArray( TEXT("."), &Arr );
		if ( Arr.Num() > 2 )
			Record.GameName = Localize( *(Arr(1)), *(Arr(2)), *(Arr(0)), UObject::GetLanguage(), 1 );
	}

	if ( Parse(Value, TEXT("Description="), tmp ) )
	{
		TArray<FString> Arr;
		tmp.ParseIntoArray( TEXT("."), &Arr );
		if ( Arr.Num() > 2 )
			Record.Description = Localize( *(Arr(1)), *(Arr(2)), *(Arr(0)), UObject::GetLanguage(),1  );
	}

	if ( Record.GameName.Len() == 0 )
		Parse(Value, TEXT("FallbackName="), Record.GameName);

	if ( Record.Description.Len() == 0 )
		Parse(Value, TEXT("FallbackDesc="), Record.Description);

	i = CacheGameTypes.AddZeroed();
	Record.RecordIndex = i;
	CacheGameTypes(i) = Record;
	return 1;
	unguard;
}

UBOOL UCacheManager::ParseWeapon( const TCHAR* Value )
{
	guard(UCacheManager::ParseWeapon);

	FWeaponRecord Record;
	
	Parse(Value, TEXT("ClassName="), Record.ClassName);
	if ( Tracker && !Tracker->RegisterCacheItem(Record.ClassName) )
		return 0;

	Parse(Value, TEXT("PickupClassName="), Record.PickupClassName );
	Parse(Value, TEXT("AttachmentClassName="), Record.AttachmentClassName);
	
	FString tmp;
	// Try to load the Name and Description using localization first
	// If that fails, use the fallback text, if it exists - this is normally the value of the property as specified in defaultproperties
	if ( Parse(Value, TEXT("FriendlyName="), tmp) )
	{
		TArray<FString> Arr;
		tmp.ParseIntoArray( TEXT("."), &Arr );
		if ( Arr.Num() > 2 )
			Record.FriendlyName = Localize( *(Arr(1)), *(Arr(2)), *(Arr(0)), UObject::GetLanguage(), 1 );
	}

	if ( Parse(Value, TEXT("Description="), tmp ) )
	{
		TArray<FString> Arr;
		tmp.ParseIntoArray( TEXT("."), &Arr );
		if ( Arr.Num() > 2 )
			Record.Description = Localize( *(Arr(1)), *(Arr(2)), *(Arr(0)), UObject::GetLanguage(), 1 );
	}

	if ( Record.FriendlyName.Len() == 0 )
		Parse(Value,TEXT("FallbackName="), Record.FriendlyName);

	if ( Record.Description.Len() == 0 )
		Parse(Value,TEXT("FallbackDesc="), Record.Description);

	INT i = CacheWeapons.AddZeroed();
	Record.RecordIndex = i;
	CacheWeapons(i) = Record;
	return 1;
	unguard;
}

UBOOL UCacheManager::ParseVehicle( const TCHAR* Value )
{
	guard(UCacheManager::ParseVehicle);

	FVehicleRecord Record;

	Parse(Value, TEXT("ClassName="), Record.ClassName);
	if ( Tracker && !Tracker->RegisterCacheItem(Record.ClassName) )
		return 0;

	FString tmp;
	// Try to load the Name and Description using localization first
	// If that fails, use the fallback text, if it exists - this is normally the value of the property as specified in defaultproperties
	if ( Parse(Value, TEXT("FriendlyName="), tmp) )
	{
		TArray<FString> Arr;
		tmp.ParseIntoArray( TEXT("."), &Arr );
		if ( Arr.Num() > 2 )
			Record.FriendlyName = Localize( *(Arr(1)), *(Arr(2)), *(Arr(0)), UObject::GetLanguage(), 1 );
	}

	if ( Parse(Value, TEXT("Description="), tmp ) )
	{
		TArray<FString> Arr;
		tmp.ParseIntoArray( TEXT("."), &Arr );
		if ( Arr.Num() > 2 )
			Record.Description = Localize( *(Arr(1)), *(Arr(2)), *(Arr(0)), UObject::GetLanguage(), 1 );
	}

	if ( Record.FriendlyName.Len() == 0 )
		Parse(Value, TEXT("FallbackName="), Record.FriendlyName);

	if ( Record.Description.Len() == 0 )
		Parse(Value,TEXT("FallbackDesc="), Record.Description);

	INT i = CacheVehicles.AddZeroed();
	Record.RecordIndex = i;
	CacheVehicles(i) = Record;
	return 1;

	unguard;
}

UBOOL UCacheManager::ParseAnnouncer( const TCHAR* Value )
{
	guard(UCacheManager::ParseAnnouncer);

	FAnnouncerRecord Record;

	Parse(Value, TEXT("ClassName="), Record.ClassName);
	if ( Tracker && !Tracker->RegisterCacheItem(Record.ClassName) )
		return 0;

	FString tmp;

	// Try to load the Name using localization first
	// If that fails, use the fallback text, if it exists - this is normally the value of the property as specified in defaultproperties
	if ( Parse(Value, TEXT("FriendlyName="), tmp) )
	{
		TArray<FString> Arr;
		tmp.ParseIntoArray( TEXT("."), &Arr );
		if ( Arr.Num() > 2 )
			Record.FriendlyName = Localize( *(Arr(1)), *(Arr(2)), *(Arr(0)), UObject::GetLanguage(), 1 );
	}

	if ( Record.FriendlyName.Len() == 0 )
        Parse(Value, TEXT("FallbackName="), Record.FriendlyName);

	Parse(Value, TEXT("PackageName="), Record.PackageName);
	Parse(Value, TEXT("FallbackPackage="), Record.FallbackPackage);

	INT i=1;
	Parse(Value, TEXT("EnglishOnly="), i);
	if ( i == 1 && appStricmp(UObject::GetLanguage(),TEXT("int")) )
		return 1;

	i = CacheAnnouncers.AddZeroed();
	Record.RecordIndex = i;
	CacheAnnouncers(i) = Record;
	return 1;

	unguard;
}

// Accessor for cached classes
//const void* UCacheManager::GetRecords( const UClass* CacheClass )
const void* UCacheManager::GetRecords( const TCHAR* CacheKey )
{
	guard(UCacheManager::GetRecords);

	if ( this == GetClass()->GetDefaultObject() )
		return GetCache()->GetRecords(CacheKey);

	InitCache( CacheKey );
	if ( IsGameRecord(CacheKey) )
		return &CacheGameTypes;

	if ( IsMutatorRecord( CacheKey ) )
		return &CacheMutators;

	if ( IsMapRecord( CacheKey ) )
		return &CacheMaps;

	if ( IsWeaponRecord( CacheKey ) )
		return &CacheWeapons;

	if ( IsCrosshairRecord( CacheKey ) )
		return &CacheCrosshairs;

	if ( IsVehicleRecord( CacheKey ) )
		return &CacheVehicles;

	if ( IsAnnouncerRecord( CacheKey ) )
		return &CacheAnnouncers;

	return NULL;

	unguard;
}

UBOOL UCacheManager::IsDefaultMap( const TCHAR* Test ) const
{
	guard(UCacheManager::IsDefaultMap);

	for ( INT i = 0; i < DefaultContent.Num(); i++)
		for ( INT j = 0; j < DefaultContent(i).Maps.Num(); j++)
			if ( DefaultContent(i).Maps(j) == Test )
				return 1;
	
	return 0;
	unguard;
}

UBOOL UCacheManager::IsDefaultPackage( const TCHAR* Test ) const
{
	guard(UCacheManager::IsDefaultPackage);

	INT Count = appStrlen(Test);
	for ( INT i = 0; i < DefaultContent.Num(); i++ )
	{
		for ( INT j = 0; j < DefaultContent(i).Classes.Num(); j++ )
		{
			if ( DefaultContent(i).Classes(j).Left(Count) == Test )
				return 1;
		}
	}

	return 0;
	unguard;
}

UBOOL UCacheManager::RemoveCacheEntry( const TCHAR* MatchText ) const
{
	guard(UCacheManager::RemoveCacheEntry);

	if ( ((void *) this) == ((void *) GetClass()->GetDefaultObject()) )
		return GetCache()->RemoveCacheEntry(MatchText);

	return FileManager->RemoveCacheEntry( MatchText );

	unguard;
}

void UCacheManager::ExportCacheMaps( const TArray<FString>& Maps, const TCHAR* DestFile, TArray<FString>& Output ) const
{
	guard(UCacheManager::ExportCacheMaps);

	for (INT i = 0; i < Maps.Num(); i++)
		ExportCacheMaps( Maps(i), DestFile, Output );

	unguard;
}

void UCacheManager::ExportCacheMaps( const TArray<FString>& Maps, const TCHAR* DestFile ) const
{
	guard(UCacheManager::ExportCacheMaps);

	for (INT i = 0; i < Maps.Num(); i++)
		ExportCacheMaps( Maps(i), DestFile );

	unguard;
}

void UCacheManager::ExportCacheMaps( const FString& NewMap, const TCHAR* DestFile ) const
{
	guard(UCacheManager::ExportCacheMaps);

	FString File = NewMap;
	if ( File.Right(4) == TEXT(".ut2") )
		File = File.LeftChop(4);

	if ( !Tracker->IsValidMap(File) )
		return;

	GWarn->Logf(NAME_Progress, TEXT("Exporting %s..."), *File );
	ULevelSummary* MapInfo = Cast<ULevelSummary>(StaticLoadObject( ULevelSummary::StaticClass(), NULL, *(File + TEXT(".LevelSummary")), NULL, LOAD_NoWarn, NULL ));
	if (!MapInfo)
	{
		GWarn->Logf(NAME_Warning, TEXT("Error loading %s !"), *NewMap);
		return;
	}

	FString Output;
	if ( CreateMapEntry( MapInfo, Output ) )
	{
		// Get the mapname
		INT Start, End;
		Start = Output.InStr(TEXT("=")) + 1;
		Start += Output.Mid(Start + 1).InStr(TEXT("=")) + 1;
		End = Output.InStr(TEXT(",")) - 1;
		if (Start != INDEX_NONE && End != INDEX_NONE)
			GWarn->Logf(TEXT("Exporting %s.....Successful!"), *Output.Mid(Start + 2, End - Start - 2));
	
		ResetLoaders( MapInfo->GetOuter(), 0, 0 );
		FileManager->AddCacheEntry( DestFile, NULL, *Output );
	}

	unguard;
}

void UCacheManager::ExportCacheMaps( const FString& NewMap, const TCHAR* DestFile, TArray<FString>& Result ) const
{
	guard(UCacheManager::ExportCacheMaps);

	FString File = NewMap;
	if ( File.Right(4) == TEXT(".ut2") )
		File = File.LeftChop(4);

	if ( !Tracker->IsValidMap(File) )
		return;

	GWarn->Logf(NAME_Progress, TEXT("Exporting %s..."), *File );

	for (INT pth=0;pth<GSys->Paths.Num();pth++)
	{
		if ( appStrstr(*GSys->Paths(pth), TEXT("*.ut2") ) )
		{
			TCHAR Tmp[256]=TEXT("");
			appStrcat( Tmp, *GSys->Paths(pth) );
			*appStrstr( Tmp, TEXT("*.ut2") )=0;
			appStrcat( Tmp, *File);
			appStrcat( Tmp, TEXT(".ut2") );
		
			FArchive* Loader = GFileManager->CreateFileReader( Tmp, 0, GError );
			FPackageFileSummary Summary;
			if (Loader)
			{
				*Loader << Summary;
				if( Summary.Tag != PACKAGE_FILE_TAG )
				{
					delete Loader;
					GWarn->Logf(NAME_Warning, TEXT("Package %s has a bad summary !"), *NewMap);
					return;
				}
				else
				{
					delete Loader;
					break;
				}
			}
/*
			else
			{
				delete Loader;
				GWarn->Logf(NAME_Warning, TEXT("Error loading %s !"), *NewMap);
				return;
			}
			delete Loader;
*/
		}				
	}
	ULevelSummary* MapInfo = Cast<ULevelSummary>(StaticLoadObject( ULevelSummary::StaticClass(), NULL, *(File + TEXT(".LevelSummary")), NULL, LOAD_NoWarn, NULL ));

	if (!MapInfo)
	{
		GWarn->Logf(NAME_Warning, TEXT("Error loading %s !"), *NewMap);
		return;
	}

	FString Output;
	if ( CreateMapEntry( MapInfo, Output ) )
	{
		// Get the mapname
		INT Start, End;
		Start = Output.InStr(TEXT("=")) + 1;
		Start += Output.Mid(Start + 1).InStr(TEXT("=")) + 1;
		End = Output.InStr(TEXT(",")) - 1;
		if (Start != INDEX_NONE && End != INDEX_NONE)
			GWarn->Logf(TEXT("Exporting %s.....Successful!"), *Output.Mid(Start + 2, End - Start - 2));

		// Make sure we are not removing the loader to the map that's loaded.

		UBOOL bIsLevel=false;
		for( TObjectIterator<UGameEngine> It; It; ++It )				
		{
			if ( (It->GLevel && It->GLevel->GetOuter() == MapInfo->GetOuter() ) ||
				 (It->GPendingLevel && It->GPendingLevel->GetOuter() == MapInfo->GetOuter() ) )
				bIsLevel = true;

		}

		if (!bIsLevel)
			ResetLoaders( MapInfo->GetOuter(), 0, 0 );

		FileManager->AddCacheEntry( DestFile, NULL, *Output );
		new(Result) FString(Output);

	}

	unguard;
}

UBOOL UCacheManager::CreateMapEntry( const ULevelSummary* MapInfo, FString& Output ) const
{
	guard(UCacheManager::CreateMapEntry);

	FString MapName = MapInfo->GetOuter()->GetName();
	const TCHAR* FallbackName=NULL;

	Output = FString::Printf(TEXT("Map=(MapName=\"%s\",Acronym=%s,PlayerCountMin=%i,PlayerCountMax=%i"), *MapName, *(MapName.Left( MapName.InStr(TEXT("-")) )), MapInfo->IdealPlayerCountMin, MapInfo->IdealPlayerCountMax);
	if ( MapInfo->Title != TEXT("") && MapInfo->Title != TEXT("Untitled") )
	{
		FallbackName = *MapInfo->Title;
		Output += FString::Printf(TEXT(",FriendlyName=%s.LevelSummary.Title"), *MapName);
	}

	if ( MapInfo->DecoTextName != TEXT("") )
		Output += FString::Printf(TEXT(",TextName=%s"), *MapInfo->DecoTextName);

	if ( MapInfo->Author != TEXT("") && MapInfo->Author != TEXT("Anonymous") )
		Output += FString::Printf(TEXT(",Author=\"%s\""), *MapInfo->Author);

	if ( MapInfo->Screenshot )
		Output += FString::Printf(TEXT(",ScreenShot=%s"), MapInfo->Screenshot->GetPathName());

	if ( MapInfo->ExtraInfo != TEXT("") )
		Output += FString::Printf(TEXT(",ExtraInfo=%s"), *MapInfo->ExtraInfo);

	if ( MapInfo->Description != TEXT("") )
		Output += FString::Printf(TEXT(",Description=%s.LevelSummary.Description,FallbackDesc=\"%s\""), *MapName, *MapInfo->Description);

	if ( !FallbackName )
		FallbackName = *MapName;

    Output += FString::Printf(TEXT(",FallbackName=\"%s\")"), FallbackName);
	return 1;

	unguard;
}

void UCacheManager::ExportCachePackages( const TArray<FString>& PackageFiles, const TCHAR* DestFile, TArray<FString>& Output ) const
{
	guard(UCacheManager::ExportCachePackages);

	for ( INT i = 0; i < PackageFiles.Num(); i++ )
		ExportCachePackages(PackageFiles(i), DestFile, Output );

	unguard;
}

void UCacheManager::ExportCachePackages( const TArray<FString>& PackageFiles, const TCHAR* DestFile ) const
{
	guard(UCacheManager::ExportCachePackages);

	for ( INT i = 0; i < PackageFiles.Num(); i++ )
		ExportCachePackages( PackageFiles(i), DestFile );


	unguard;
}

void UCacheManager::ExportCachePackages( const FString& NewPackageFile, const TCHAR* DestFile ) const
{
	guard(UCacheManager::ExportCachePackages);

	FString Output, NewFile;

	if ( NewPackageFile.Right(2) == TEXT(".u") )
		NewFile = NewPackageFile.LeftChop(2);
	else NewFile = NewPackageFile;

	GWarn->Logf(NAME_Progress, TEXT("Exporting %s..."), *NewFile);
	UObject* NewPackage = LoadPackage( NULL, *NewFile, LOAD_NoWarn );

	if (!NewPackage)
	{
		GWarn->Logf(NAME_Warning, TEXT("Error loading %s !!!"), *NewFile);
		return;
	}

	for (TObjectIterator<UClass> It; It; ++It)
	{
		Output = TEXT("");
		if ( It->IsIn( NewPackage ) )
		{
			if ( CreatePackageEntry( *It, Output ) )
			{
				TArray<FString> OutputArray;
				Output.ParseIntoArray( LINE_TERMINATOR, &OutputArray );
				if ( OutputArray.Num() )
				{
					for (INT i = 0; i < OutputArray.Num(); i++)
						FileManager->AddCacheEntry( DestFile, NULL, *OutputArray(i) );
				}
				else FileManager->AddCacheEntry( DestFile, NULL, *Output );
			}
		}
	}

	unguard;
}

void UCacheManager::ExportCachePackages( const FString& NewPackageFile, const TCHAR* DestFile, TArray<FString>& Result ) const
{
	guard(UCacheManager::ExportCachePackages);

	FString Output, NewFile;

	if ( NewPackageFile.Right(2) == TEXT(".u") )
		NewFile = NewPackageFile.LeftChop(2);
	else NewFile = NewPackageFile;

	GWarn->Logf(NAME_Progress, TEXT("Loading %s..."), *NewFile);
	UObject* NewPackage = LoadPackage( NULL, *NewFile, LOAD_NoWarn );

	if (!NewPackage)
	{
		if ( NewPackageFile != TEXT("") )
			GWarn->Logf(NAME_Warning, TEXT("Error loading %s !!!"), *NewFile);
		return;
	}

	for (TObjectIterator<UClass> It; It; ++It)
	{
		Output = TEXT("");
		if ( It->IsIn( NewPackage ) )
		{
//			if ( It->ClassFlags&CLASS_Cacheable )
			//{
				if ( CreatePackageEntry( *It, Output ) )
				{
					TArray<FString> OutputArray;
					Output.ParseIntoArray( LINE_TERMINATOR, &OutputArray );
					if ( OutputArray.Num() )
					{
						Result += OutputArray;
						for (INT i = 0; i < OutputArray.Num(); i++)
							FileManager->AddCacheEntry( DestFile, NULL, *OutputArray(i) );
					}
					else
					{
						FileManager->AddCacheEntry( DestFile, NULL, *Output );
						new(Result) FString(Output);
					}
				//}
			}
		}
	}

	unguard;
}

INT UCacheManager::CreatePackageEntry( UClass* Cls, FString& Output ) const
{
	guard(UCacheManager::CreatePackageEntry::UClass);

	if ( CHECKCLASS(Cls,AMutator) )
	{
		const AMutator* Obj = ConstCast<AMutator>(StaticConstructObject( Cls, UObject::GetTransientPackage(), NAME_None, 0));
		return CreatePackageEntry( Obj, Output );
	}

	if ( CHECKCLASS(Cls,AGameInfo) )
	{
		const AGameInfo* Obj = ConstCast<AGameInfo>(StaticConstructObject( Cls, UObject::GetTransientPackage(), NAME_None, 0));
		return CreatePackageEntry( Obj, Output );
	}
	if ( CHECKCLASS(Cls,AWeapon) )
	{
		const AWeapon* Obj = ConstCast<AWeapon>(StaticConstructObject( Cls, UObject::GetTransientPackage(), NAME_None, 0));
		return CreatePackageEntry( Obj, Output );
	}

	if ( CHECKCLASS(Cls,UCrosshairPack) )
	{
		const UCrosshairPack* Obj = ConstCast<UCrosshairPack>(StaticConstructObject( Cls, UObject::GetTransientPackage(), NAME_None, 0) );
		return CreatePackageEntry( Obj, Output );
	}

	if ( CHECKCLASS(Cls,AVehicle) )
	{
		const AVehicle* Obj = ConstCast<AVehicle>(StaticConstructObject( Cls, UObject::GetTransientPackage(), NAME_None, 0));
		return CreatePackageEntry( Obj, Output );
	}

	if ( CHECKCLASS(Cls,AAnnouncerVoice) )
	{
		const AAnnouncerVoice* Obj = ConstCast<AAnnouncerVoice>(StaticConstructObject(Cls, UObject::GetTransientPackage(), NAME_None, 0));
		return CreatePackageEntry( Obj, Output );
	}

	return 0;

	unguard;
}

UBOOL UCacheManager::CreatePackageEntry( const UCrosshairPack* Crosshair, FString& Output ) const
{
	guard(UCacheManager::CreatePackageEntry::UCrosshairPack);

	if ( !Crosshair )
		return 0;

	Output = TEXT("");
	for (INT i = 0; i < Crosshair->Crosshair.Num(); i++)
	{
		if ( Output.Len() )
			Output += LINE_TERMINATOR;

		Output += FString::Printf(TEXT("Crosshair=(FriendlyName=\"%s\",CrosshairTexture=%s,ClassName=%s)"), *Crosshair->Crosshair(i).FriendlyName, Crosshair->Crosshair(i).CrosshairTexture->GetPathName(), Crosshair->GetClass()->GetPathName());
		LOGEXPORT(Crosshair,*Crosshair->Crosshair(i).FriendlyName);
	}

	return 1;
	unguard;
}

UBOOL UCacheManager::CreatePackageEntry( const AWeapon* Weapon, FString& Output ) const
{
	guard(UCacheManager::CreatePackageEntry::AWeapon);

	if ( !Weapon )
		return 0;

	const TCHAR* Path = Weapon->GetClass()->GetPathName();
	Output = FString::Printf( TEXT("Weapon=(ClassName=%s"), Path);
	if ( Weapon->PickupClass )
		Output += FString::Printf(TEXT(",PickupClassName=%s"), Weapon->PickupClass->GetPathName());
	if ( Weapon->AttachmentClass )
		Output += FString::Printf(TEXT(",AttachmentClassName=%s"), Weapon->AttachmentClass->GetPathName());

	const TCHAR* FallbackName = NULL, *FallbackDesc = NULL;

	// Attempt to determine whether this weapon has a valid item name and description.
	if ( Weapon->ItemName != TEXT("") )
	{
		FallbackName = *Weapon->ItemName;
		Output += FString::Printf(TEXT(",FriendlyName=%s.ItemName"), Path);
	}

	if ( Weapon->Description != TEXT("") )
	{
		FallbackDesc = *Weapon->Description;
		Output += FString::Printf(TEXT(",Description=%s.Description"), Path);
	}

	const FString* Temp = Tracker->GetDescription( WEAPON_META_INDEX, Path );
	if ( Temp && *Temp != TEXT("") )
		FallbackDesc = *(*Temp);

	if ( !FallbackName )
		FallbackName = Weapon->GetClass()->GetName();

	if ( !FallbackDesc )
		FallbackDesc = FallbackName;

	Output += FString::Printf(TEXT(",FallbackName=\"%s\",FallbackDesc=\"%s\")"), FallbackName, FallbackDesc);
	LOGEXPORT(Weapon,Path);
	return 1;

	unguard;
}

UBOOL UCacheManager::CreatePackageEntry( const AGameInfo* Game, FString& Output ) const
{
	guard(UCacheManager::CreatePackageEntry::AGameInfo);

	if ( !Game )
		return 0;

	const TCHAR* Path = Game->GetClass()->GetPathName();
	Output = TEXT("Game=(");
	Output += FString::Printf(TEXT("ClassName=%s,GameName=%s.GameName,TeamGame=%i"), Path, Path, Game->bTeamGame);
	if ( Game->Acronym != TEXT("") && Game->Acronym != TEXT("???") )
		Output += FString::Printf(TEXT(",GameAcronym=%s"), *Game->Acronym);
	if ( Game->MapPrefix != TEXT("") )
		Output += FString::Printf(TEXT(",MapPrefix=%s"), *Game->MapPrefix);
	if ( Game->ScreenShotName != TEXT("") )
		Output += FString::Printf(TEXT(",Screenshot=%s"), *Game->ScreenShotName);
	if ( Game->MapListType != TEXT("") )
		Output += FString::Printf(TEXT(",MapListClassName=%s"), *Game->MapListType);
	if ( Game->HUDSettingsMenu != TEXT("") )
		Output += FString::Printf(TEXT(",HUDMenu=%s"), *Game->HUDSettingsMenu);
	if ( Game->RulesMenuType != TEXT("") )
		Output += FString::Printf(TEXT(",RulesMenu=%s"), *Game->RulesMenuType);

	Output += FString::Printf(TEXT(",Description=%s.Description"), Path);
	
	const TCHAR* FallbackDesc = NULL, *FallbackName = NULL;
	const FString* Temp = Tracker->GetDescription( GAMETYPE_META_INDEX, Path );
	FString gameName;
	if ( Temp && *Temp != TEXT("") )
	{
		INT Pos;
		Pos = Temp->InStr(TEXT("|"));
		if ( Pos != INDEX_NONE )
		{
			gameName = Temp->Mid(Pos+1);
			Pos = gameName.InStr(TEXT("|"));
			if ( Pos != INDEX_NONE )
			{
				gameName = gameName.Left(Pos);
				FallbackName = *gameName;
			}
		}
	}

	else if ( Game->GameName != TEXT("") )
		FallbackName = *Game->GameName;
	
	if ( Game->Description != TEXT("") )
		FallbackDesc = *Game->Description;

	if ( !FallbackName )
		FallbackName = Game->GetClass()->GetName();

	if ( !FallbackDesc )
		FallbackDesc = FallbackName;

	Output += FString::Printf(TEXT(",FallbackName=\"%s\",FallbackDesc=\"%s\")"), FallbackName, FallbackDesc);
	LOGEXPORT(Game,Path);
	return 1;
	unguard;
}

UBOOL UCacheManager::CreatePackageEntry( const AMutator* Mutator, FString& Output ) const
{
	guard(UCacheManager::CreatePackageEntry::AMutator);

	if ( !Mutator )
		return 0;

	const TCHAR* Path = Mutator->GetClass()->GetPathName();
	Output = TEXT("Mutator=(");

	Output += FString::Printf(TEXT("ClassName=%s"), Path);
	if (Mutator->ConfigMenuClassName != TEXT(""))
		Output += FString::Printf(TEXT(",ConfigMenuClassName=%s"), *Mutator->ConfigMenuClassName);
	if ( Mutator->GroupName != TEXT("") )
        Output += FString::Printf(TEXT(",GroupName=%s"), *Mutator->GroupName);
	if ( Mutator->IconMaterialName != TEXT("") )
        Output += FString::Printf(TEXT(",IconMaterialName=%s"), *Mutator->IconMaterialName);

	Output += FString::Printf(TEXT(",FriendlyName=%s.FriendlyName,Description=%s.Description"),Path, Path);

	const TCHAR* FallbackDesc = NULL, *FallbackName = NULL;
	const FString* Temp = Tracker->GetDescription( MUTATOR_META_INDEX, Path );
	if ( Temp && *Temp != TEXT("") )
		FallbackDesc = *(*Temp);
	else if ( Mutator->Description != TEXT("") )
		FallbackDesc = *Mutator->Description;


	if ( Mutator->FriendlyName != TEXT("") )
	{
        Output += FString::Printf(TEXT(",FriendlyName=%s.FriendlyName"), Path );
		FallbackName = *Mutator->FriendlyName;
	}


	if ( !FallbackName )
		FallbackName = Mutator->GetClass()->GetName();

	if ( !FallbackDesc )
		FallbackDesc = FallbackName;

	Output += FString::Printf(TEXT(",FallbackName=\"%s\",FallbackDesc=\"%s\")"), FallbackName, FallbackDesc);
	LOGEXPORT(Mutator,Path);
	return 1;
	unguard;
}

UBOOL UCacheManager::CreatePackageEntry( const AVehicle* Vehicle, FString& Output ) const
{
	guard(UCacheManager::CreatePackageEntry::AVehicle);

	if ( !Vehicle )
		return 0;

	const TCHAR* Path = Vehicle->GetClass()->GetPathName();
	Output = FString::Printf( TEXT("Vehicle=(ClassName=%s"), Path);

	const TCHAR* FallbackName = NULL, *FallbackDesc = NULL;
	if ( Vehicle->VehicleNameString != TEXT("") )
	{
		Output += FString::Printf(TEXT(",FriendlyName=%s.VehicleNameString"), Path);
		FallbackName = *Vehicle->VehicleNameString;
	}

	if ( Vehicle->VehicleDescription != TEXT("") )
	{
		Output += FString::Printf(TEXT(",Description=%s.VehicleDescription"), Path);
		FallbackDesc = *Vehicle->VehicleDescription;
	}

	if ( !FallbackName )
		FallbackName = Path;

	if ( !FallbackDesc )
		FallbackDesc = FallbackName;

	Output += FString::Printf(TEXT(",FallbackName=\"%s\",FallbackDesc=\"%s\")"), FallbackName, FallbackDesc);
	LOGEXPORT(Vehicle,Path);

	return 1;

	unguard;
}

UBOOL UCacheManager::CreatePackageEntry( const AAnnouncerVoice* Announcer, FString& Output ) const
{
	guard(UCacheManager::CreatePackageEntry::AAnnouncerVoice);

	if ( !Announcer )
		return 0;

	const TCHAR* Path = Announcer->GetClass()->GetPathName();
	Output = FString::Printf( TEXT("Announcer=(ClassName=%s"), Path);

	Output += FString::Printf( TEXT(",PackageName=%s"), *Announcer->SoundPackage );
	if ( Announcer->FallbackSoundPackage != TEXT("") )
		Output += FString::Printf(TEXT(",FallbackPackage=%s"), *Announcer->FallbackSoundPackage);

	Output += FString::Printf(TEXT(",EnglishOnly=%i"), Announcer->bEnglishOnly);
	const TCHAR* FallbackName = NULL;
	if ( Announcer->AnnouncerName != TEXT("") )
	{
		Output += FString::Printf(TEXT(",FriendlyName=%s.AnnouncerName"), Path);
		FallbackName = *Announcer->AnnouncerName;
	}

	if ( !FallbackName )
		FallbackName = Path;

	Output += FString::Printf(TEXT(",FallbackName=\"%s\")"), FallbackName);
	LOGEXPORT(Announcer,Path);

	return 1;

	unguard;
}

void UCacheManager::Destroy()
{
	guard(UCacheManager::Destroy);	
	Close();

	if ( Cache == this )
		Cache = NULL;

	Super::Destroy();
	unguard;
}

void UCacheManager::Close()
{
	guard(UCacheManager::Close);

	CacheMaps.Empty();
	CacheMutators.Empty();
	CacheWeapons.Empty();
	CacheCrosshairs.Empty();
	CacheGameTypes.Empty();
	CacheVehicles.Empty();
	CacheAnnouncers.Empty();

	if ( Tracker )
	{
		delete Tracker;
		Tracker = NULL;
	}

	if ( FileManager )
	{
		delete FileManager;
		FileManager = NULL;
	}

	unguard;
}

//======================================================================================
//
//	FCacheTracker
//
//======================================================================================
FCacheTracker::FCacheTracker()
{
	guard(FCacheTracker::FCacheTracker);

	MetaClasses[0] = AWeapon::StaticClass();
	MetaClasses[1] = AGameInfo::StaticClass();
	MetaClasses[2] = AMutator::StaticClass();

	appStrcpy( CacheTag[0], TEXT("Weapon") );
	appStrcpy( CacheTag[1], TEXT("GameType") );
	appStrcpy( CacheTag[2], TEXT("Mutator") );

	unguard;
}

FCacheTracker::~FCacheTracker()
{
	guard(FCacheTracker::~FCacheTracker);

	for (INT i = 0; i < ARRAY_COUNT(MetaClasses); i++)
		MetaClasses[i] = NULL;

	unguard;
}

UBOOL FCacheTracker::Initialized() const
{
	guard(FCacheTracker::Initialized);

	return DetectedPackages.Num();

	unguard;
}

void FCacheTracker::InitRegObjects()
{
	guard(FCacheTracker::InitRegObjects);

	// Find all of the registry entries from .int files for weapons, gametypes, and mutators.  Add them to the CacheRemaining array.
	// We will check this array after we have loaded everything into cache to determine if we have any new files.
	for (INT i = 0; i < META_CLASS_NUM; i++)
	{
		TArray<FRegistryObjectInfo>& RegObj = RegList[i];
		RegObj.Empty();

		UObject::GetRegistryObjects( RegObj, UClass::StaticClass(), MetaClasses[i], 0 );
		for ( INT j = 0; j < RegObj.Num(); j++)
			FoundUnregistered( CacheTag[i], RegObj(j).Object );
	}


	unguard;
}

UBOOL FCacheTracker::IsValidPackage( const TCHAR* Test ) const
{
	guard(FCacheTracker::IsValidMap);

	return true;

	unguard;
}

UBOOL FCacheTracker::IsValidMap( const TCHAR* Test) const
{
	guard(FCacheTracker::IsValidMap);

	// Return false if:
	// 1. No extension in map filename
	// 2. Extension is UT2
	// 3. Not demo version

	return appStrstr(Test, TEXT("-")) != NULL && /*appStrnicmp(Test, TEXT("TUT"), 3) &&*/ appStrnicmp(Test, TEXT("UT2"), 3);

	unguard;
}

void FCacheTracker::GetNewItems( const TCHAR* KeyName, TArray<FString>& ItemList, UBOOL PurgeFromList )
{
	guard(FCacheTracker::GetNewItems);

	if ( !KeyName )
	{
		for ( TMapBase<FString,FString>::TIterator It(UnregisteredPackages); It; ++It )
		{
			if ( ItemList.FindItemIndex(It.Value()) == INDEX_NONE )
				new(ItemList) FString(It.Value());

			if (PurgeFromList)
				It.RemoveCurrent();
		}
	}
	else 
	{
		UnregisteredPackages.MultiFind( KeyName, ItemList );
		if ( PurgeFromList )
		{
			for ( INT i = 0; i < ItemList.Num(); i++ )
				UnregisteredPackages.RemovePair( KeyName, *ItemList(i) );
		}
	}

	unguard;
}

UBOOL FCacheTracker::PostNewCache( const TCHAR* Key, const TCHAR* Value )
{
	guard(FCacheTracker::PostNewCache);

	NewCache.Add( Key, Value );
	return 1;

	unguard;
}

void FCacheTracker::GetPostedCache( const TCHAR* KeyName, TArray<FString>& ItemList )
{
	guard(FCacheTracker::GetPostedCache);

	if ( !KeyName )
	{
		for ( TMapBase<FString,FString>::TIterator It(NewCache); It; ++It )
			new(ItemList) FString(It.Value());
	}

	else NewCache.MultiFind(KeyName, ItemList);
	unguard;
}

UBOOL FCacheTracker::FoundUnregistered( const TCHAR* Key, const TArray<FString>& Value )
{
	guard(FCacheTracker::FoundNewCache_T_TArrF);

	for ( INT i = 0; i < Value.Num(); i++ )
		if ( !FoundUnregistered(Key, Value(i)) )
			return 0;

	return 1;

	unguard;
}

UBOOL FCacheTracker::FoundUnregistered(const FString& Key, const TArray<FString>& Value )
{
	guard(FCacheTracker::FoundNewCache_F_TArrF);

	return FoundUnregistered(*Key,Value);
	unguard;
}

UBOOL FCacheTracker::FoundUnregistered( const TCHAR* Key, const TCHAR* Value )
{
	guard(FCacheTracker::FoundUnregistered);

	check(Key);
	check(Value);

	UnregisteredPackages.AddUnique( Key, Value );
	return 1;

	unguard;
}

UBOOL FCacheTracker::DetectedPackage( const TCHAR* PackageName )
{
	guard(FCacheTracker::DetectedPackage);

	check(PackageName);
	if ( WasDetected(PackageName) )
		return 0;

	new(DetectedPackages) FString(PackageName);
	return 1;

	unguard;
}

UBOOL FCacheTracker::WasDetected( const TCHAR* PackageName ) const
{
	guard(FCacheTracker::WasDetected);

	for ( INT i = 0; i < DetectedPackages.Num(); i++ )
		if ( DetectedPackages(i) == PackageName )
			return 1;

	return 0;

	unguard;
}

UBOOL FCacheTracker::IsUnregistered( const TCHAR* Value ) const
{
	guard(FCacheTracker::IsUnregistered);

	check(Value);
	for ( TMapBase<FString,FString>::TConstIterator It(UnregisteredPackages); It; ++It )
		if ( It.Value() == Value )
			return 1;

	return 0;
	unguard;
}

UBOOL FCacheTracker::RegisterCacheItem( const FString& MatchText )
{
	guard(FCacheTracker::RegisterCacheItem);

	UBOOL Result = 1;
	FString Temp(MatchText);

	// UnregisteredPackages only contains packages, so remove the class name if we have one
	INT Pos = Temp.InStr(TEXT("."));
	if ( Pos != INDEX_NONE )
		Temp = Temp.Left(Pos);

	// If this package wasn't in our known packages list, return false and remove this cached entry
	if ( !WasDetected( Temp ) )
		Result = 0;

	// If this package exists in our list of unregistered items, remove it and return true
	for ( TMapBase<FString,FString>::TIterator It(UnregisteredPackages); It; ++It )
	{
		if ( It.Value() == Temp )
		{
			It.RemoveCurrent();
			return Result;
		}
	}

	// Otherwise, it isn't a new package
	return Result;

	unguard;
}

const FString* FCacheTracker::GetDescription( const INT MetaIndex, const TCHAR* ClassName ) const
{
	guard(FCacheTracker::GetDescription);

	for (INT i = 0; i < RegList[MetaIndex].Num(); i++)
	{
		if ( RegList[MetaIndex](i).Object == ClassName )
			return &RegList[MetaIndex](i).Description;
	}

	return NULL;

	unguard;
}

void FCacheTracker::Reset()
{
	guard(FCacheTracker::Reset);

	UnregisteredPackages.Empty();
	DetectedPackages.Empty();
	NewCache.Empty();

	Dirty = 0;
	unguard;
}

//======================================================================================
//
//	FCacheFileManager
//
//======================================================================================
FCacheFileManager::FCacheFileManager( DWORD Flags )
: ExportFlags(Flags)
{
}

FCacheFileManager::~FCacheFileManager()
{
	guard(FCacheFileManager::~FCacheFileManager);

	CacheFiles.Empty();
	CacheFileList.Empty();

	unguard;
}

void FCacheFileManager::Init()
{
	guard(FCacheManager::Init);
	LoadCacheFiles();
	unguard;
}

void FCacheFileManager::Reset()
{
	guard(FCacheFileManager::Reset);

	Close();

	unguard;
}

UBOOL FCacheFileManager::Initialized() const
{
	guard(FCacheFileManager::Initialized);

	return CacheFiles.Num();

	unguard;
}

UBOOL FCacheFileManager::GetCacheLines( TArray<FString>& Lines, const TCHAR* KeyFilter, UBOOL AllowDuplicates )
{
	guard(FCacheFileManager::GetCacheLines);

	Lines.Empty();
	for (INT i = 0; i < CacheFiles.Num(); i++)
		CacheFiles(i).GetLines( Lines, KeyFilter, AllowDuplicates );

	return Lines.Num();
	unguard;
}

FCacheFileManager::FCacheFile* FCacheFileManager::FindCacheFile( const TCHAR* FileName )
{
	guard(FCacheFileManager::FindCacheFile);

	for (INT i = 0; i < CacheFiles.Num(); i++)
	{
		if ( !appStricmp(CacheFiles(i).GetFileName(), FileName) )
			return &CacheFiles(i);
	}

	return NULL;
	unguard;
}

UBOOL FCacheFileManager::LoadCacheFiles()
{
	guard(FCacheFileManager::LoadCacheFiles);

	CacheFiles.Empty();

	FString CacheDir, CacheWildcard;

/*
#if DEMOVERSION
	ExportFlags = EXPORT_NoCreate;
#endif
*/
	for (INT i=0;i<GSys->CacheRecordPath.Num(); i++)
	{
		if ( GetCacheDirectory( CacheDir, i, &CacheWildcard ) )
		{
			CacheFileList = GFileManager->FindFiles( *(CacheDir + CacheWildcard), 1, 0 );
			for ( INT j = 0; j < CacheFileList.Num(); j++ )
				new(CacheFiles) FCacheFile( *CacheFileList(j), ExportFlags, this, i );
		}
	}
	return CacheFiles.Num();
	unguard;
}

UBOOL FCacheFileManager::GetCacheDirectory( FString& Path, INT Index, FString* ExtensionWildcard ) const
{
	guard(FCacheFileManager::GetCacheDirectories);

	if ( Index >= GSys->CacheRecordPath.Num() )
	{
		Path = TEXT("../System/");
		if ( ExtensionWildcard )
			*ExtensionWildcard = TEXT("*.ucl");
		return 1;
	}

	INT Pos = GSys->CacheRecordPath(Index).InStr( TEXT("*.") );
	if ( Pos == INDEX_NONE )
		return 0;

	Path = GSys->CacheRecordPath(Index).Left( Pos );
	if ( ExtensionWildcard )
		*ExtensionWildcard = GSys->CacheRecordPath(Index).Mid( Pos );

	return 1;
	unguard;
}

UBOOL FCacheFileManager::AddCacheEntry( const TCHAR* DestFile, const TCHAR* NewKey, const TCHAR* NewValue )
{
	guard(FCacheFileManager::AddCacheEntry);

	FString Buffer;
	// First check if this file already exists
	FCacheFile* File = FindCacheFile( DestFile );

	// If no Key, just use Value
	if ( NewKey )
		Buffer = FString::Printf(TEXT("%s=(%s)"), NewKey, NewValue);
	else Buffer = NewValue;

	// If so, add this line to that file
	if ( File )
	{
		return File->AddItem( Buffer );
	}

	// If not, create a new file, and add this line
	else
	{
		File = new(CacheFiles) FCacheFile( DestFile, ExportFlags, this );
		if ( File )
			return File->AddItem( Buffer );
		else debugf(NAME_Warning, TEXT("Memory allocation failure while creating new cache file!"));
	}

	return 0;
	unguard;
}

UBOOL FCacheFileManager::RemoveCacheEntry( const TCHAR* Item )
{
	guard(FCacheFileManager::RemoveCacheEntry);

	for ( INT i = CacheFiles.Num() - 1; i >= 0; i-- )
	{
		if ( CacheFiles(i).RemoveItem(Item) )
		{
			// If this was that cache file's last entry, delete the file
			if ( !CacheFiles(i).Num() && DeleteCacheFile(CacheFiles(i)) )
				CacheFiles.Remove(i);

			return 1;
		}
	}

	return 0;

	unguard;
}

UBOOL FCacheFileManager::DeleteCacheFile( FCacheFile& File )
{
	guard(FCacheFileManager::DeleteCacheFile);

	return GFileManager->Delete( *File.GetFullFileName(), 0, File.CanOverwrite() );

	unguard;
}

void FCacheFileManager::Close()
{
	guard(FCacheFileManager::Close);

	for ( INT i = 0; i < CacheFiles.Num(); i++ )
		CacheFiles(i).Close();

	unguard;
}

//======================================================================================
//
//	FCacheFile
//
//======================================================================================

FCacheFileManager::FCacheFile::FCacheFile( const TCHAR* Name, DWORD Flags, const FCacheFileManager* InManager, INT PIndex  )
: FileName(Name), ExportFlags(Flags), Manager(InManager), PathsIndex(PIndex)
{
	guard(FCacheFile::FCacheFile);

	LoadData();

	unguard;
}

FCacheFileManager::FCacheFile::~FCacheFile()
{
	guard(FCacheFile::~FCacheFile);

    SaveData();
	Empty();

	unguard;
}

void FCacheFileManager::FCacheFile::Close()
{
	guard(FCacheFile::Close);

	SaveData();
	unguard;
}

const TCHAR* FCacheFileManager::FCacheFile::GetFileName() const
{
	guard(FCacheFile::GetFileName);

	return *FileName;
	unguard;
}

FString FCacheFileManager::FCacheFile::GetFullFileName() const
{
	guard(FCacheFile::GetFullFileName);

	FString FullName;
	if ( Manager && Manager->GetCacheDirectory( FullName, PathsIndex ) )
		FullName += FileName;

	return FullName;
	unguard;
}

UBOOL FCacheFileManager::FCacheFile::GetFullFileName( FString& FullName ) const
{
	guard(FCacheFile::GetFullFileName);

	if ( Manager && Manager->GetCacheDirectory(FullName, PathsIndex) )
	{
		FullName += FileName;
		return 1;
	}

	return 0;
	unguard;
}

void FCacheFileManager::FCacheFile::LoadData()
{
	guard(FCacheFile::LoadData);

	FString FullFileName, Buffer;
	GetFullFileName(FullFileName);

	if ( ExportFlags & EXPORT_Append )
	{
		if ( appLoadFileToString( Buffer, *FullFileName ) )
		{
			TCHAR* Stuff = const_cast<TCHAR*>(*Buffer), *Pos = NULL;
			while ( Stuff )
			{
				Pos = appStrchr(Stuff,'\r');
				if ( Pos )
					*Pos++ = 0;
				else
					Pos = appStrchr(Stuff,'\n');  // handle Unix endlines.
				if ( Pos && *Pos == '\n' )
					*Pos++ = 0;
				new(*this)FString(Stuff);
				if ( ExportFlags & EXPORT_Verbose )
					debugf(NAME_RecordCache, TEXT("Loaded %s: %s"), *FullFileName, Stuff);
				Stuff = Pos;
			}
		}
	}

	Dirty = 0;

	unguard;
}

void FCacheFileManager::FCacheFile::SaveData()
{
	guard(FCacheFile::SaveData);

	if ( !Dirty || Num() <= 0 )
		return;

	FString FullFileName;
	GetFullFileName(FullFileName);

	if ( GFileManager->FileSize(*FullFileName) > 0)
	{
		if ( GFileManager->IsReadOnly( *FullFileName ) && !(ExportFlags & EXPORT_Overwrite) )
		{
			debugf(NAME_RecordCache, TEXT("Error saving %s: Read-Only"), *FileName);
			return;
		}
	}
	else if ( ExportFlags & EXPORT_NoCreate )
	{
		debugf(NAME_RecordCache, TEXT("Error saving %s: File not found"), *FileName);
		return;
	}


	FString Output;

	bReverseSort=false;
	Sort( (FString*)Data, Num() );
	if ( CreateString( Output ) )
		appSaveStringToFile( Output, *FullFileName, GFileManager );

	Dirty = 0;
	unguard;
}

void FCacheFileManager::FCacheFile::GetLines( TArray<FString>& Lines, const TCHAR* KeyFilter, UBOOL AllowDuplicates )
{
	guard(FCacheFile::GetLines);

	for ( INT Index = 0; Index < ArrayNum; Index++ )
	{
		INT Pos = (*this)(Index).InStr(TEXT("="));
		FString Key;
		if ( Pos != INDEX_NONE )
			Key = (*this)(Index).Left(Pos);

		if ( KeyFilter && Key != KeyFilter )
			continue;

		if ( AllowDuplicates )
			new(Lines) FString( (*this)(Index) );
		else
		{
			INT i;
			for (i = 0; i < Lines.Num(); i++)
			{
				INT MaskPos = (*this)(Index).InStr(TEXT(","));
				if ( MaskPos == INDEX_NONE )
					continue;

				if ( !appStrnicmp( *Lines(i), *(*this)(Index), MaskPos+1 ) )
					break;
			}

			if ( i == Lines.Num() )
				new(Lines) FString( (*this)(Index) );
		}
	}

	unguard;
}

UBOOL FCacheFileManager::FCacheFile::AddItem( const TCHAR* Item )
{
	guard(FCacheFile::AddItemT);

	UBOOL Result = UpdateItem( Item );
	if ( !Result )
	{
		INT i = AddZeroed();
		(*this)(i) = Item;
		Dirty = 1;

		if ( ExportFlags & EXPORT_Verbose )
			debugf(NAME_RecordCache, TEXT("Added %s: %s"), *GetFullFileName(), Item);
	}

	return 1;
	unguard;
}

UBOOL FCacheFileManager::FCacheFile::AddItem(const FString& Item)
{
	guard(FCacheFile::AddItemF);

	if ( UpdateItem(*Item) )
	{
		Dirty = 1;
		return 0;
	}

	INT i = AddZeroed();
	(*this)(i) = Item;
	if ( ExportFlags & EXPORT_Verbose )
		debugf(NAME_RecordCache, TEXT("Added %s: %s"), *GetFullFileName(), *Item);

	Dirty = 1;
	return 1;

	unguard;
}

UBOOL FCacheFileManager::FCacheFile::UpdateItem( const TCHAR* Item )
{
	guard(FCacheFile::UpdateItem);

	for ( INT i = 0; i < ArrayNum; i++ )
	{
		INT Pos = (*this)(i).InStr(TEXT(","));
		if ( Pos != INDEX_NONE )
		{
			if ( !appStrnicmp( *(((*this)(i)).Left(Pos+1)), Item, Pos+1 ) )
			{
				(*this)(i) = Item;
				Dirty = 1;
				if ( ExportFlags & EXPORT_Verbose )
					debugf(NAME_RecordCache, TEXT("Updated %s: %s"), *GetFullFileName(), Item);
				return 1;
			}
		}
	}

	return 0;
	unguard;
}

UBOOL FCacheFileManager::FCacheFile::RemoveItem( const TCHAR* Item )
{
	guard(FCacheFile::RemoveItem);

	INT idx = FindItemIndex( Item );
	if ( idx != INDEX_NONE )
	{
		Remove(idx);
		Dirty = 1;
		if ( ExportFlags & EXPORT_Verbose )
			debugf(NAME_RecordCache, TEXT("Removed %s: %s"), *GetFullFileName(), Item);
		return 1;
	}

	return 0;
	unguard;
}

UBOOL FCacheFileManager::FCacheFile::CreateString( FString& String ) const
{
	guard(FCacheFile::CreateString);

	if ( ArrayNum == 0 )
		return 0;

	String = TEXT("");
	for ( INT i = 0; i < ArrayNum; i++ )
	{
		if ( String != TEXT("") )
			String += LINE_TERMINATOR;
		String += (*this)(i);
	}

	return 1;

	unguard;
}

//===================================================================================================================================
//
// Globals
//
//===================================================================================================================================

UBOOL CacheInt( FString* Result, const FString& MetaClass, const INT CurrentIndex )
{
	guard(CacheGetNextInt);

	UCacheManager* Base = UCacheManager::GetCache();
	if (MetaClass == TEXT("Engine.Crosshair"))
	{
		const TArray<FCrosshairRecord>* Records = (TArray<FCrosshairRecord>*)(Base->GetRecords(TEXT("Crosshair")));
		if ( Records == NULL || !Records->IsValidIndex(CurrentIndex) )
			return 0;

		*Result = (*Records)(CurrentIndex).CrosshairTexture->GetPathName();
		return 1;
	}

	if ( MetaClass == TEXT("Engine.GameInfo") )
	{
		const TArray<FGameRecord>* Records = (TArray<FGameRecord>*)(Base->GetRecords(TEXT("Game")));
		if ( Records == NULL || !Records->IsValidIndex(CurrentIndex) )
			return 0;

		*Result = (*Records)(CurrentIndex).ClassName;
		return 1;
	}

	else if (MetaClass == TEXT("Engine.Mutator"))
	{
		const TArray<FMutatorRecord>* Records = (TArray<FMutatorRecord>*)(Base->GetRecords(TEXT("Mutator")));
		if ( Records == NULL || !Records->IsValidIndex(CurrentIndex) )
			return 0;

		*Result = (*Records)(CurrentIndex).ClassName;
		return 1;
	}

	else if (MetaClass == TEXT("Engine.Weapon"))
	{
		const TArray<FWeaponRecord>* Records = (TArray<FWeaponRecord>*)(Base->GetRecords(TEXT("Weapon")));
		if ( Records == NULL || !Records->IsValidIndex(CurrentIndex) )
			return 0;

		*Result = (*Records)(CurrentIndex).ClassName;
		return 1;
	}

	else if ( MetaClass == TEXT("Engine.Vehicle") )
	{
		const TArray<FVehicleRecord>* Records = (TArray<FVehicleRecord>*)(Base->GetRecords(TEXT("Vehicle")));
		if ( Records == NULL || !Records->IsValidIndex(CurrentIndex) )
			return 0;

		*Result = (*Records)(CurrentIndex).ClassName;
		return 1;
	}

	else if ( MetaClass == TEXT("UnrealGame.AnnouncerVoice") )
	{
		const TArray<FAnnouncerRecord>* Records = (TArray<FAnnouncerRecord>*)(Base->GetRecords(TEXT("Announcer")));
		if ( !Records || !Records->IsValidIndex(CurrentIndex) )
			return 0;

		*Result = (*Records)(CurrentIndex).ClassName;
		return 1;
	}

	return 0;

	unguard;
}

UBOOL CacheInt( FString* OutClassName, FString* OutDesc, const FString& MetaClass, const INT CurrentIndex )
{
	guard(CacheGetNextIntDesc);

	UCacheManager* Base = UCacheManager::GetCache();
	if (MetaClass == TEXT("Engine.Crosshair"))
	{
		const TArray<FCrosshairRecord>* Records = (TArray<FCrosshairRecord>*)(Base->GetRecords(TEXT("Crosshair")));
		if ( Records == NULL || !Records->IsValidIndex(CurrentIndex) )
			return 0;

		*OutClassName = (*Records)(CurrentIndex).CrosshairTexture->GetPathName();
		return 1;
	}

	if ( MetaClass == TEXT("Engine.GameInfo") )
	{
		const TArray<FGameRecord>* Records = (TArray<FGameRecord>*)(Base->GetRecords(TEXT("Game")));
		if ( Records == NULL || !Records->IsValidIndex(CurrentIndex) )
			return 0;

        *OutClassName = (*Records)(CurrentIndex).ClassName;
		*OutDesc = FString::Printf(TEXT("%s|%s||%s|%i"),
							*((*Records)(CurrentIndex).GameAcronym),
                            *((*Records)(CurrentIndex).GameName),
                            *((*Records)(CurrentIndex).MapListClassName),
                             ((*Records)(CurrentIndex).bTeamGame));
		return 1;
	}

	else if (MetaClass == TEXT("Engine.Mutator"))
	{
		const TArray<FMutatorRecord>* Records = (TArray<FMutatorRecord>*)(Base->GetRecords(TEXT("Mutator")));
		if ( Records == NULL || !Records->IsValidIndex(CurrentIndex) )
			return 0;

		*OutClassName = (*Records)(CurrentIndex).ClassName;
		*OutDesc = (*Records)(CurrentIndex).Description;
		return 1;
	}

	else if (MetaClass == TEXT("Engine.Weapon"))
	{
		const TArray<FWeaponRecord>* Records = (TArray<FWeaponRecord>*)(Base->GetRecords(TEXT("Weapon")));
		if ( Records == NULL || !Records->IsValidIndex(CurrentIndex) )
			return 0;

		*OutClassName = (*Records)(CurrentIndex).ClassName;
		*OutDesc = (*Records)(CurrentIndex).Description;
		return 1;
	}

	else if ( MetaClass == TEXT("Engine.Vehicle") )
	{
		const TArray<FVehicleRecord>* Records = (TArray<FVehicleRecord>*)(Base->GetRecords(TEXT("Vehicle")));
		if ( Records == NULL || !Records->IsValidIndex(CurrentIndex) )
			return 0;

		*OutClassName = (*Records)(CurrentIndex).ClassName;
		*OutDesc = (*Records)(CurrentIndex).Description;
		return 1;
	}

	else if ( MetaClass == TEXT("UnrealGame.AnnouncerVoice") )
	{
		const TArray<FAnnouncerRecord>* Records = (TArray<FAnnouncerRecord>*)(Base->GetRecords(TEXT("Announcer")));
		if ( !Records || !Records->IsValidIndex(CurrentIndex) )
			return 0;

		*OutClassName = (*Records)(CurrentIndex).ClassName;
		*OutDesc = (*Records)(CurrentIndex).FriendlyName;
		return 1;
	}

	return 0;

	unguard;
}

UBOOL CacheInt( TArray<FString>& Result, const FString& MetaClass )
{
	guard(CacheAllInt);

	UCacheManager* Base = UCacheManager::GetCache();
	if (MetaClass == TEXT("Engine.Crosshair"))
	{
		const TArray<FCrosshairRecord>* Records = (TArray<FCrosshairRecord>*)(Base->GetRecords(TEXT("Crosshair")));
		if ( Records == NULL )
			return 0;

		for ( INT i = 0; i < Records->Num(); i++ )
			new(Result) FString((*Records)(i).CrosshairTexture->GetPathName());

		return 1;
	}

	if ( MetaClass == TEXT("Engine.GameInfo") )
	{
		const TArray<FGameRecord>* Records = (TArray<FGameRecord>*)(Base->GetRecords(TEXT("Game")));
		if ( Records == NULL )
			return 0;

		for ( INT i = 0; i < Records->Num(); i++ )
			new(Result) FString((*Records)(i).ClassName);
		return 1;
	}

	else if (MetaClass == TEXT("Engine.Mutator"))
	{
		const TArray<FMutatorRecord>* Records = (TArray<FMutatorRecord>*)(Base->GetRecords(TEXT("Mutator")));
		if ( Records == NULL )
			return 0;

		for ( INT i = 0; i < Records->Num(); i++ )
			new(Result) FString((*Records)(i).ClassName);
		return 1;
	}

	else if (MetaClass == TEXT("Engine.Weapon"))
	{
		const TArray<FWeaponRecord>* Records = (TArray<FWeaponRecord>*)(Base->GetRecords(TEXT("Weapon")));
		if ( Records == NULL )
			return 0;

		for ( INT i = 0; i < Records->Num(); i++ )
			new(Result) FString((*Records)(i).ClassName);
		return 1;
	}

	else if ( MetaClass == TEXT("Engine.Vehicle") )
	{
		const TArray<FVehicleRecord>* Records = (TArray<FVehicleRecord>*)(Base->GetRecords(TEXT("Vehicle")));
		if ( !Records )
			return 0;

		for ( INT i = 0; i < Records->Num(); i++ )
			new(Result) FString((*Records)(i).ClassName);
		return 1;
	}

	else if ( MetaClass == TEXT("UnrealGame.AnnouncerVoice") )
	{
		const TArray<FAnnouncerRecord>* Records = (TArray<FAnnouncerRecord>*)(Base->GetRecords(TEXT("Announcer")));
		if ( !Records )
			return 0;

		for ( INT i = 0; i < Records->Num(); i++ )
			new(Result) FString((*Records)(i).ClassName);

		return 1;
	}

	return 0;

	unguard;
}

UBOOL CacheInt( TArray<FString>* OutClassName, TArray<FString>* OutDesc, const FString& MetaClass )
{
	guard(CacheAllIntDesc);

	UCacheManager* Base = UCacheManager::GetCache();
	if (MetaClass == TEXT("Engine.Crosshair"))
	{
		const TArray<FCrosshairRecord>* Records = (TArray<FCrosshairRecord>*)(Base->GetRecords(TEXT("Crosshair")));
		if ( Records == NULL  )
			return 0;

		for (INT i = 0; i < Records->Num(); i++)
			new(*OutClassName) FString((*Records)(i).CrosshairTexture->GetPathName());

		return 1;
	}

	if ( MetaClass == TEXT("Engine.GameInfo") )
	{
		const TArray<FGameRecord>* Records = (TArray<FGameRecord>*)(Base->GetRecords(TEXT("Game")));
		if ( Records == NULL )
			return 0;

		for ( INT i = 0; i < Records->Num(); i++ )
		{
			new(*OutClassName) FString( (*Records)(i).ClassName );
			INT idx = OutDesc->AddZeroed();
			(*OutDesc)(idx) = FString::Printf(TEXT("%s|%s||%s|%i"),
			*((*Records)(i).GameAcronym), *((*Records)(i).GameName), *((*Records)(i).MapListClassName), (*Records)(i).bTeamGame);
		}
		return 1;
	}

	else if (MetaClass == TEXT("Engine.Mutator"))
	{
		const TArray<FMutatorRecord>* Records = (TArray<FMutatorRecord>*)(Base->GetRecords(TEXT("Mutator")));
		if ( Records == NULL )
			return 0;

		for ( INT i = 0; i < Records->Num(); i++ )
		{
			new(*OutClassName) FString( (*Records)(i).ClassName );
			new(*OutDesc) FString( (*Records)(i).Description );
		}
		return 1;
	}

	else if (MetaClass == TEXT("Engine.Weapon"))
	{
		const TArray<FWeaponRecord>* Records = (TArray<FWeaponRecord>*)(Base->GetRecords(TEXT("Weapon")));
		if ( Records == NULL )
			return 0;

		for ( INT i = 0; i < Records->Num(); i++ )
		{
			new(*OutClassName) FString( (*Records)(i).ClassName );
			new(*OutDesc) FString( (*Records)(i).Description );
		}
		return 1;
	}

	else if ( MetaClass == TEXT("Engine.Vehicle") )
	{
		const TArray<FVehicleRecord>* Records = (TArray<FVehicleRecord>*)(Base->GetRecords(TEXT("Vehicle")));
		if ( !Records )
			return 0;

		for ( INT i = 0; i < Records->Num(); i++ )
		{
			new(*OutClassName) FString( (*Records)(i).ClassName );
			new(*OutDesc) FString( (*Records)(i).Description );
		}

		return 1;
	}

	else if ( MetaClass == TEXT("UnrealGame.AnnouncerVoice") )
	{
		const TArray<FAnnouncerRecord>* Records = (TArray<FAnnouncerRecord>*)(Base->GetRecords(TEXT("Announcer")));
		if ( !Records )
			return 0;

		for ( INT i = 0; i < Records->Num(); i++ )
		{
			new(*OutClassName) FString( (*Records)(i).ClassName );
			new(*OutDesc) FString( (*Records)(i).FriendlyName );
		}

		return 1;
	}

	return 0;

	unguard;
}

#define PREPARE_CACHE_EXPORT(flags) \
	LoadClass<UCacheManager>(NULL,TEXT("Engine.CacheManager"), NULL, 0, NULL); \
	UCacheManager* Cache = UCacheManager::GetCache(); \
	check(Cache); \
	FCacheTracker* Tracker = Cache->CreateTracker(); \
	check(Tracker); \
	FCacheFileManager* FileManager = Cache->CreateFileManager(flags); \
	check(FileManager);

UBOOL ExportCacheMap( const TCHAR* MapPackage )
{
	guard(ExportCacheMap);

	PREPARE_CACHE_EXPORT(EXPORT_Append);

	UBOOL Result = 0;
	if ( Tracker->FoundUnregistered(TEXT("Map"),MapPackage) )
	{
		FileManager->Init();
		Cache->SaveNewPackages( Tracker, FileManager, NULL, NULL );
		Result = 1;
	}

	delete FileManager;
	delete Tracker;

	return Result;

	unguard;
}

UBOOL ExportCacheMap( TArray<FString>& MapPackages )
{
	guard(ExportCacheMaps);
	PREPARE_CACHE_EXPORT(EXPORT_Append);

	UBOOL Result = 0;
	if ( Tracker->FoundUnregistered(TEXT("Map"),MapPackages) )
	{
		FileManager->Init();
		Cache->SaveNewPackages(Tracker,FileManager,NULL,NULL);
		Result = 1;
	}

	delete Tracker;
	delete FileManager;
	
	return Result;

	unguard;
}

UBOOL ExportCachePackage( const TCHAR* Package )
{
	guard(ExportCache);

	PREPARE_CACHE_EXPORT(EXPORT_Append);

	UBOOL Result = 0;

	if ( Tracker->FoundUnregistered(TEXT("Other"), Package) )
	{
		FileManager->Init();
		Cache->SaveNewPackages(Tracker,FileManager,NULL,NULL);

		Result = 1;
	}

	delete Tracker;
	delete FileManager;

	return Result;

	unguard;
}

UBOOL ExportCachePackage( TArray<FString>& Packages )
{
	guard(ExportCachePackages);

	PREPARE_CACHE_EXPORT(EXPORT_Append);

	UBOOL Result = 0;
	if ( Tracker->FoundUnregistered(TEXT("Other"),Packages) )
	{
		FileManager->Init();
		Cache->SaveNewPackages(Tracker,FileManager,NULL,NULL);
		Result = 1;
	}

	delete Tracker;
	delete FileManager;

	return Result;
	unguard;
}

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) ENGINE_API FName ENGINE_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "UnCacheManager.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY
