/*=============================================================================
	UnCacheManager.h: Unreal caching system header.
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Ron Prestenback
=============================================================================*/
/*
#ifndef __CACHEMANAGER_H__
#define __CACHEMANAGER_H__
*/
#include "Engine.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#ifndef ENGINE_API
#define ENGINE_API DLL_IMPORT
#endif

#ifndef NAMES_ONLY
#define AUTOGENERATE_NAME(name) extern ENGINE_API FName ENGINE_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#endif


#ifndef NAMES_ONLY

#define META_CLASS_NUM (INT)(3)
#define WEAPON_META_INDEX (INT)(0)
#define GAMETYPE_META_INDEX (INT)(1)
#define MUTATOR_META_INDEX (INT)(2)

// Global methods for ScanDirectories() & GetCacheDirectories()
inline UBOOL ContainsMaps( const TCHAR* Test )		{ return appStrstr( Test, (FURL::DefaultMapExt == TEXT("") ? TEXT(".ut2") : *FURL::DefaultMapExt) ) != NULL; }
inline UBOOL ContainsPackages( const TCHAR* Test )  { return !appStrcmp(&Test[appStrlen(Test) - 3], TEXT("*.u")); }
// Overloads
inline UBOOL ContainsMaps( FString& Test )			{ return ContainsMaps(*Test);	  }
inline UBOOL ContainsPackages( FString& Test )		{ return ContainsPackages(*Test); }

inline void CreateCacheFileName( const FString& PackageName, FString& FileName )
{
	guard(CreateCacheFileName);

	INT Pos = PackageName.InStr(TEXT("."));
	if ( Pos != INDEX_NONE )
		FileName = PackageName.Left(Pos);
	else FileName = PackageName;

	if ( GSys )
	{
		FString CachePath = GSys->CacheRecordPath(0);
		Pos = CachePath.InStr(TEXT("*."));
		if ( Pos != INDEX_NONE )
			FileName += CachePath.Mid(Pos+1);
		else FileName += TEXT(".ucl");
	}
	else FileName += TEXT(".ucl");

	unguard;
}

// constant cast templates
template< class T > const T* ConstCast( const UObject* Src )
{
	return Src && Src->IsA(T::StaticClass()) ? (T*)Src : NULL;
}

template< class T, class U > const T* ConstCastChecked( const U* Src )
{
	if( !Src || !Src->IsA(T::StaticClass()) )
		appErrorf( TEXT("Cast of %s to %s failed"), Src ? Src->GetFullName() : TEXT("NULL"), T::StaticClass()->GetName() );
	return (T*)Src;
}


enum EExportFlag
{
	EXPORT_Append		= 0x01,
	EXPORT_Overwrite	= 0x02,
	EXPORT_NoCreate		= 0x04,
	EXPORT_Verbose      = 0x08,
};

struct ENGINE_API FGameRecord
{
    FString ClassName GCC_PACK(4);
    FString GameName;
    FString Description;
    FString TextName;
    FString GameAcronym;
    FString MapListClassName;
    FString MapPrefix;
    FString ScreenshotRef;
    FString HUDMenu;
    FString RulesMenu;
    BITFIELD bTeamGame:1 GCC_PACK(4);
    BYTE GameTypeGroup GCC_PACK(4);
    INT RecordIndex GCC_PACK(4);
};

struct ENGINE_API FMutatorRecord
{
    FString ClassName GCC_PACK(4);
    FString FriendlyName;
    FString Description;
    FString IconMaterialName;
    FString ConfigMenuClassName;
    FString GroupName;
    INT RecordIndex;
    BYTE bActivated;
};

struct ENGINE_API FMapRecord
{
    FString Acronym GCC_PACK(4);
    FString MapName;
    FString TextName;
    FString FriendlyName;
    FString Author;
    FString Description;
    INT PlayerCountMin;
    INT PlayerCountMax;
    FString ScreenshotRef;
    FString ExtraInfo;
    INT RecordIndex;
};

struct ENGINE_API FWeaponRecord
{
    FString ClassName GCC_PACK(4);
    FString PickupClassName;
    FString AttachmentClassName;
    FString Description;
    FString TextName;
    FString FriendlyName;
    INT RecordIndex;
};

struct ENGINE_API FVehicleRecord
{
    FString ClassName GCC_PACK(4);
    FString FriendlyName;
    FString Description;
    INT RecordIndex;
};

struct ENGINE_API FCrosshairRecord
{
    FString FriendlyName GCC_PACK(4);
    class UTexture* CrosshairTexture;
    INT RecordIndex;
};

struct ENGINE_API FAnnouncerRecord
{
    FString ClassName GCC_PACK(4);
    FString FriendlyName;
    FString PackageName;
    FString FallbackPackage;
	BITFIELD bEnglishOnly:1 GCC_PACK(4);
    INT RecordIndex GCC_PACK(4);
};

struct ENGINE_API FStandard
{
    TArrayNoInit<FString> Classes GCC_PACK(4);
    TArrayNoInit<FString> Maps;
};

//=============================================================================================================================
//  FCacheFileManager is responsible for moving caching information to and from ucl files.  When CacheFileManager is initialized,
//  it loads all .ucl files from disk, and prepares the information to be imported to the caching system.  When new information
//  is exported from objects, FCacheFileManager is responsible for storing that information out to .ucl file.
//=============================================================================================================================
class FCacheFileManager
{
protected:

	// An FCacheFile object is created for each .ucl file that is found in the cache directory.
	class FCacheFile : public TArray<FString>
	{
		      UBOOL              Dirty;							// Whether this object's state is different from the version on disk
		const FString            FileName;                      // The filename of this object's associated .ucl file.
		const INT				 PathsIndex;					// Index in to the GSys->CacheRecordPath array
		const DWORD              ExportFlags;                   // Determines whether this object can append or overwrite file on disk
		const FCacheFileManager* Manager;

		void LoadData();	// Read data from .ucl file
		void SaveData();	// Write data to .ucl file

		UBOOL CreateString(FString& String) const;

	public:
		FCacheFile( const TCHAR* Name = NULL, DWORD Flags = 0, const FCacheFileManager* InManager = NULL, INT PIndex=0 );
		virtual ~FCacheFile();

		// Interface
		const TCHAR* GetFileName() const;
		FString GetFullFileName() const; // Includes directory in filename
		UBOOL GetFullFileName( FString& FullName ) const;

		void GetLines( TArray<FString>& Lines, const TCHAR* KeyFilter = NULL, UBOOL AllowDuplicates = 0 );

		UBOOL AddItem( const TCHAR* Item );
		UBOOL AddItem( const FString& Item );
		UBOOL UpdateItem( const TCHAR* Item );
		UBOOL UpdateItem( const FString& Item );
		UBOOL RemoveItem( const TCHAR* Item );
		UBOOL RemoveItem( const FString& Item );

		UBOOL CanOverwrite() const	{ return ExportFlags & EXPORT_Overwrite;	}
		UBOOL CanAppend() const		{ return ExportFlags & EXPORT_Append;		}
		UBOOL CanCreate() const		{ return ExportFlags & EXPORT_NoCreate;		}

		void Close();
	};


	const DWORD ExportFlags;

	FCacheFile* FindCacheFile( const TCHAR* FileName );
	UBOOL       DeleteCacheFile( FCacheFile& File );

private:
	TMap<FString,FString> CacheListBuffer;
	TArray<FString>       CacheFileList;
	TArray<FCacheFile>    CacheFiles;

public:
	FCacheFileManager( DWORD Flags = 0 );
	~FCacheFileManager(void);

	void	Init();
	void    Reset();
	void    Close();

	UBOOL   Initialized() const;
	UBOOL	LoadCacheFiles();
	UBOOL   GetCacheDirectory( FString& Path, INT Index, FString* ExtensionWildcard = NULL ) const;
	UBOOL	GetCacheLines( TArray<FString>& Lines, const TCHAR* KeyFilter = NULL, UBOOL AllowDuplicates = 0 );

	UBOOL	AddCacheEntry(    const TCHAR* DestFile, const TCHAR* NewKey, const TCHAR* NewLine      );
	UBOOL	AddCacheEntry(    const TCHAR* DestFile, const TCHAR* NewKey, TArray<FString>& NewLines );
	UBOOL	RemoveCacheEntry( const TCHAR* Item );
};

class FCacheTracker
{
	TMultiMap<FString,FString>	UnregisteredPackages;		// Packages found that do not exist in cache
	TMultiMap<FString,FString>  NewCache;					// Packages which were successfully exported after initial cache startup
															// These packages will be added to the cache in IntegrateNewPackages()
	TArray<FString>				DetectedPackages;			// Files that actually exist in directories
	UBOOL						Dirty;						// Need to flush this cache

	TArray<FRegistryObjectInfo> RegList[META_CLASS_NUM];	// List of packages registered in .int files
	UClass* MetaClasses[META_CLASS_NUM];					// The meta classes we care about
	TCHAR	CacheTag[META_CLASS_NUM][10];

public:
	FCacheTracker();
	~FCacheTracker();

	UBOOL   Initialized() const;
	void    Reset();
	void	GetNewItems( const TCHAR* KeyName, TArray<FString>& ItemList, UBOOL PurgeFromList = 0 );
	void	GetPostedCache( const TCHAR* KeyName, TArray<FString>& ItemList );
	
	// FoundUnregistered is called for all maps, as well as any package (mutator, weapon, gametype, etc.) information
	// received from the global caching system (.int files)

	// When .ucl files are parsed by CacheManager, it registers the item with CacheTracker.  If the item was flagged as
	// unregistered, it is then removed from the unregistered list.
	UBOOL   FoundUnregistered( const TCHAR*   Key, const TArray<FString>& Value );
	UBOOL   FoundUnregistered( const FString& Key, const TArray<FString>& Value );
	UBOOL	FoundUnregistered( const TCHAR*   Key, const TCHAR* Value );
	UBOOL	FoundUnregistered( const TCHAR*   Key, const FString& Value )
	{
		INT Pos = Value.InStr(TEXT("."));
		if ( Pos != INDEX_NONE )
			return FoundUnregistered( Key, *(Value.Left(Pos)) );
		else return FoundUnregistered( Key, *Value );
	}
	UBOOL   FoundUnregistered( const FString& Key, const FString& Value )
	{
		INT Pos = Value.InStr(TEXT("."));
		if ( Pos != INDEX_NONE )
			return FoundUnregistered( *Key, *(Value.Left(Pos)) );
		else return FoundUnregistered( *Key, *Value );
	}

	UBOOL	DetectedPackage( const TCHAR* PackageName );
	UBOOL   DetectedPackage( const FString& PackageName )
	{
		INT Pos = PackageName.InStr(TEXT("."));
		if ( Pos != INDEX_NONE )
			return DetectedPackage( *(PackageName.Left(Pos)) );
		else return DetectedPackage( *PackageName );
	}

	UBOOL	IsUnregistered( const TCHAR* Value ) const;
	UBOOL	WasDetected(    const TCHAR* PackageName ) const;
	UBOOL   WasDetected(    const FString& PackageName ) const						{ return WasDetected( *PackageName );	}

	// Called whenever a cacheline is parsed and added to cache
	UBOOL	RegisterCacheItem( const FString& MatchText );
	UBOOL	RegisterCacheItem( const TCHAR* MatchText )								{ return RegisterCacheItem( FString(MatchText) ); }

	UBOOL	PostNewCache( const TCHAR* Key, const TCHAR* Value );
	UBOOL	PostNewCache( const FString& Key, const FString& Value )				{ return PostNewCache( *Key, *Value ); }

	// Interfacing with registry objects
	void           InitRegObjects();
    const FString* GetDescription( const INT MetaIndex, const TCHAR* ClassName ) const;

	UBOOL   IsValidPackage( const TCHAR* Test )   const;
	UBOOL   IsValidPackage( const FString& Test ) const { return IsValidPackage( *Test ); }

	UBOOL	IsValidMap(     const TCHAR* Test )   const;
	UBOOL	IsValidMap(     const FString& Test ) const { return IsValidMap( *Test ); }
};

// needed to move this to a header, since static linking needs
// the definition for AUTO_INITIALIZE_REGISTRANTS_ENGINE ...  --ryan.
class UExportCacheCommandlet : public UCommandlet
{
	DECLARE_CLASS(UExportCacheCommandlet,UCommandlet,CLASS_Transient,Engine)
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

class ENGINE_API UCacheManager : public UObject
{
	// Helper method for InitCache.
	void        ScanDirectories( FCacheTracker* const InTracker );		// Scan all directories, adding any found packages to the lists.
	void        IntegrateNewPackages( const TCHAR* CacheKey = NULL );	// Load newly detected packages to the cache


	// Parsing methods
	UBOOL		ParseCacheFiles( const TCHAR* CacheKey = NULL );

	// Cache Exporters - writes object information to .ucl file
	UBOOL CreateMapEntry( const ULevelSummary*       MapInfo,   FString& Output ) const;
	UBOOL CreatePackageEntry( UClass*                Cls,       FString& Output ) const;
	UBOOL CreatePackageEntry( const AMutator*        Mutator,   FString& Output ) const;
	UBOOL CreatePackageEntry( const AGameInfo*       Game,      FString& Output ) const;
	UBOOL CreatePackageEntry( const AWeapon*         Weapon,    FString& Output ) const;
	UBOOL CreatePackageEntry( const UCrosshairPack*  Crosshair, FString& Output ) const;
	UBOOL CreatePackageEntry( const AVehicle*        Vehicle,   FString& Output ) const;
	UBOOL CreatePackageEntry( const AAnnouncerVoice* Announcer, FString& Ouptut ) const;

protected:
	UBOOL		ParseMap(       const TCHAR* Value );
	UBOOL		ParseMutator(   const TCHAR* Value );
	UBOOL		ParseWeapon(    const TCHAR* Value );
	UBOOL       ParseVehicle(   const TCHAR* Value );
	UBOOL		ParseCrosshair( const TCHAR* Value );
	UBOOL		ParseGame(      const TCHAR* Value );
	UBOOL       ParseAnnouncer( const TCHAR* Value );

	DECLARE_CLASS( UCacheManager, UObject, CLASS_Transient, Engine )
    NO_DEFAULT_CONSTRUCTOR(UCacheManager)

/*  this is what would be generated by UCC if the class weren't noexport
	TArrayNoInit<FStandard> DefaultContent GCC_PACK(4);
    TArrayNoInit<FMutatorRecord> CacheMutators;
    TArrayNoInit<FMapRecord> CacheMaps;
    TArrayNoInit<FWeaponRecord> CacheWeapons;
    TArrayNoInit<FVehicleRecord> CacheVehicles;
    TArrayNoInit<FCrosshairRecord> CacheCrosshairs;
    TArrayNoInit<FGameRecord> CacheGameTypes;
    TArrayNoInit<FAnnouncerRecord> CacheAnnouncers;
    PTRINT FileManager;
    PTRINT Tracker;
*/
    TArrayNoInit<FStandard>  DefaultContent GCC_PACK(4);
	TArray<FMutatorRecord>   CacheMutators;
	TArray<FMapRecord>       CacheMaps;
	TArray<FWeaponRecord>    CacheWeapons;
	TArray<FVehicleRecord>   CacheVehicles;
	TArray<FCrosshairRecord> CacheCrosshairs;
	TArray<FGameRecord>      CacheGameTypes;
	TArray<FAnnouncerRecord> CacheAnnouncers;

	class FCacheFileManager* FileManager;
	class FCacheTracker*	 Tracker;

	// Unrealscript declarations
    DECLARE_FUNCTION(execGetMapRecord);
    DECLARE_FUNCTION(execGetGameRecord);
	DECLARE_FUNCTION(execGetMutatorRecord);
	DECLARE_FUNCTION(execGetWeaponRecord);
	DECLARE_FUNCTION(execGetVehicleRecord);
	DECLARE_FUNCTION(execGetAnnouncerRecord);
    DECLARE_FUNCTION(execGetTeamSymbolList);
    DECLARE_FUNCTION(execGetAnnouncerList);
    DECLARE_FUNCTION(execGetMutatorList);
    DECLARE_FUNCTION(execGetCrosshairList);
    DECLARE_FUNCTION(execGetVehicleList);
    DECLARE_FUNCTION(execGetWeaponList);
    DECLARE_FUNCTION(execGetMapList);
    DECLARE_FUNCTION(execGetGameTypeList);
    DECLARE_FUNCTION(execIsDefaultContent);
    DECLARE_FUNCTION(execIsBPContent);
    DECLARE_FUNCTION(execIs2004Content);
    DECLARE_FUNCTION(execIs2003Content);
    DECLARE_FUNCTION(execInitCache);

	// Unrealscript counterparts
	UBOOL GetMapRecord( const TCHAR* MapName, FMapRecord& Record );
	UBOOL GetGameRecord( const TCHAR* ClassName, FGameRecord& Record );
	UBOOL GetMutatorRecord( const TCHAR* ClassName, FMutatorRecord& Record );
	UBOOL GetWeaponRecord( const TCHAR* ClassName, FWeaponRecord& Record );
	UBOOL GetVehicleRecord( const TCHAR* ClassName, FVehicleRecord& Record );
	UBOOL GetAnnouncerRecord( const TCHAR* ClassName, FAnnouncerRecord& Record );

	const void* GetRecords( const TCHAR* CacheKey );
	static UBOOL IsMapRecord(       const TCHAR* Key ) { return Key == NULL || !appStricmp( Key, TEXT("Map")       ); }
	static UBOOL IsGameRecord(      const TCHAR* Key ) { return Key == NULL || !appStricmp( Key, TEXT("Game")      ); }
	static UBOOL IsWeaponRecord(    const TCHAR* Key ) { return Key == NULL || !appStricmp( Key, TEXT("Weapon")    ); }
	static UBOOL IsMutatorRecord(   const TCHAR* Key ) { return Key == NULL || !appStricmp( Key, TEXT("Mutator")   ); }
	static UBOOL IsCrosshairRecord( const TCHAR* Key ) { return Key == NULL || !appStricmp( Key, TEXT("Crosshair") ); }
	static UBOOL IsVehicleRecord(   const TCHAR* Key ) { return Key == NULL || !appStricmp( Key, TEXT("Vehicle")   ); }
	static UBOOL IsAnnouncerRecord( const TCHAR* Key ) { return Key == NULL || !appStricmp( Key, TEXT("Announcer") ); }


	// Initialization

	static UCacheManager* GetCache();

	void InitCacheManager(   DWORD  Flags    = 0    );
	void InitCache(    const TCHAR* CacheKey = NULL );
	UBOOL Initialized( const TCHAR* CacheKey = NULL ) const;	    // returns whether this cache type has been initialized

	// Should be called at each mapchange
	UBOOL RefreshCache();                                           // returns true if cache state was modified

	FCacheTracker*     CreateTracker() const;						// Create CacheTracker
	FCacheFileManager* CreateFileManager(DWORD Flags = 0 ) const;	// Create CacheFileManager

 	void Destroy();

	// Accessor methods
	UBOOL IsDefaultMap(     const TCHAR* Test ) const;				// Whether this map is an Epic map
	UBOOL IsDefaultPackage( const TCHAR* Test ) const;				// Whether this package is an Epic package
	const FCacheTracker*     GetTracker()       const	{ return Tracker; };
	const FCacheFileManager* GetFileManager()	        { return FileManager; }


   // Methods used to manipulate cache
	UBOOL RemoveCacheEntry( const TCHAR* MatchText ) const;
	void  SaveNewPackages( FCacheTracker* InTracker, FCacheFileManager* InFileManager, const TCHAR* CacheKey = NULL, const FString* DestFile = NULL );	// Export newly detected packages to the appropriate .ucl file

protected:

	void Close();
	// Protected export methods
	void ExportCacheMaps(     const FString& NewPackage,           const TCHAR* DestFile                          ) const;
	void ExportCacheMaps(     const TArray<FString>& Packages,     const TCHAR* DestFile                          ) const;
	void ExportCacheMaps(     const FString& NewPackage,           const TCHAR* DestFile, TArray<FString>& Output ) const;
	void ExportCacheMaps(     const TArray<FString>& Packages,     const TCHAR* DestFile, TArray<FString>& Output ) const;

	void ExportCachePackages( const FString& NewPackage,           const TCHAR* DestFile                          ) const;
	void ExportCachePackages( const TArray<FString>& PackageFiles, const TCHAR* DestFile                          ) const;
	void ExportCachePackages( const FString& NewPackage,           const TCHAR* DestFile, TArray<FString>& Output ) const;
	void ExportCachePackages( const TArray<FString>& PackageFiles, const TCHAR* DestFile, TArray<FString>& Output ) const;
};

// GetNextInt/GetNextIntDescription interface
ENGINE_API UBOOL CacheInt( FString* Result, const FString& MetaClass, const INT CurrentIndex );
ENGINE_API UBOOL CacheInt( FString* OutClassName, FString* OutDesc,   const FString& MetaClass, const INT CurrentIndex );
ENGINE_API UBOOL CacheInt( TArray<FString>& Result, const FString& MetaClass );
ENGINE_API UBOOL CacheInt( TArray<FString>* OutClassName, TArray<FString>* OutDesc, const FString& MetaClass );

ENGINE_API UBOOL ExportCacheMap( const TCHAR* MapPackage );
ENGINE_API UBOOL ExportCacheMap( TArray<FString>& MapPackages );
ENGINE_API UBOOL ExportCachePackage( const TCHAR* Package );
ENGINE_API UBOOL ExportCachePackage( TArray<FString>& Packages );

#endif	// NAMES_ONLY

AUTOGENERATE_FUNCTION(UCacheManager,883,execGetAnnouncerRecord);
AUTOGENERATE_FUNCTION(UCacheManager,882,execGetVehicleRecord);
AUTOGENERATE_FUNCTION(UCacheManager,881,execGetWeaponRecord);
AUTOGENERATE_FUNCTION(UCacheManager,880,execGetMutatorRecord);
AUTOGENERATE_FUNCTION(UCacheManager,819,execGetMapRecord);
AUTOGENERATE_FUNCTION(UCacheManager,818,execGetGameRecord);
AUTOGENERATE_FUNCTION(UCacheManager,811,execGetTeamSymbolList);
AUTOGENERATE_FUNCTION(UCacheManager,810,execGetAnnouncerList);
AUTOGENERATE_FUNCTION(UCacheManager,809,execGetMutatorList);
AUTOGENERATE_FUNCTION(UCacheManager,808,execGetCrosshairList);
AUTOGENERATE_FUNCTION(UCacheManager,807,execGetVehicleList);
AUTOGENERATE_FUNCTION(UCacheManager,806,execGetWeaponList);
AUTOGENERATE_FUNCTION(UCacheManager,805,execGetMapList);
AUTOGENERATE_FUNCTION(UCacheManager,804,execGetGameTypeList);
AUTOGENERATE_FUNCTION(UCacheManager,830,execIsDefaultContent);
AUTOGENERATE_FUNCTION(UCacheManager,803,execIsBPContent);
AUTOGENERATE_FUNCTION(UCacheManager,802,execIs2004Content);
AUTOGENERATE_FUNCTION(UCacheManager,801,execIs2003Content);
AUTOGENERATE_FUNCTION(UCacheManager,800,execInitCache);

#ifdef VERIFY_CLASS_SIZES
VERIFY_CLASS_SIZE_NODIE(UCacheManager)
#endif

#ifndef NAMES_ONLY
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#endif

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

//#endif // __CACHEMANAGER_H__
