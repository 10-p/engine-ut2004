/*=============================================================================
	UnGameUtilities.cpp: APawn AI implementation

  This contains game utilities used by script for accessing files

	Copyright 2000 Epic MegaGames, Inc. This software is a trade secret.

	Revision history:
		* Created by Steven Polge 4/00
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "FConfigCacheIni.h"


void AActor::execGetNextInt( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execGetNextInt);

	P_GET_STR(ClassName);
	P_GET_INT(CurrentInt);
	P_FINISH;

	FString* StringResult = (FString*)Result;
	*StringResult = TEXT("");
	if (!CacheInt(StringResult,ClassName,CurrentInt))
	{
		UClass* TempClass = FindObjectChecked<UClass>( ANY_PACKAGE, *ClassName );
		TArray<FRegistryObjectInfo> List;
		GetRegistryObjects( List, UClass::StaticClass(), TempClass, 0 );
		*StringResult = (CurrentInt<List.Num()) ? List(CurrentInt).Object : FString(TEXT(""));
	}

	unguardexec;
}
// rjp --
void AActor::execGetAllInt( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execGetAllInt);

	P_GET_STR(MetaClass);
	P_GET_TARRAY_REF(Entries,FString);
	P_FINISH;

	// Possible memory stompage:
	// script calls native function that returns a dynamic array, but script doesn't assign the result to a variable
	// Always best to use out parameters for returning dynamic arrays
	Entries->Empty();
	if (!CacheInt( *Entries, MetaClass ))
	{
		UClass* TempClass = FindObjectChecked<UClass>( ANY_PACKAGE, *MetaClass );
		TArray<FRegistryObjectInfo> List;
		GetRegistryObjects( List, UClass::StaticClass(), TempClass, 0 );
		
		for (INT i = 0; i < List.Num(); i++)
			new(*Entries) FString(List(i).Object);
	}

	unguardexec;
}
// --rjp
void AActor::execGetNextIntDesc( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execGetNextIntDesc);

	P_GET_STR(ClassName);
	P_GET_INT(CurrentInt);
	P_GET_STR_REF(EntryName);
	P_GET_STR_REF(Description);
	P_FINISH;

	*EntryName = TEXT("");
	*Description = TEXT("");
	if (!CacheInt(EntryName,Description,ClassName,CurrentInt))
	{
		UClass* TempClass = FindObjectChecked<UClass>( ANY_PACKAGE, *ClassName );

		TArray<FRegistryObjectInfo> List;
		GetRegistryObjects( List, UClass::StaticClass(), TempClass, 0 );

		*EntryName = (CurrentInt<List.Num()) ? List(CurrentInt).Object : FString(TEXT(""));
		*Description = (CurrentInt<List.Num()) ? List(CurrentInt).Description : FString(TEXT(""));
	}

	unguardexec;
}
// rjp --
void AActor::execGetAllIntDesc( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execGetAllIntDesc);

	// Possible memory stompage:
	// script calls native function that returns a dynamic array, but script doesn't assign the result to a variable
	// Always best to use out parameters for returning dynamic arrays
	P_GET_STR(MetaClass);
	P_GET_TARRAY_REF(Entry,FString);
	P_GET_TARRAY_REF(Description,FString);
	P_FINISH;

	if ( !CacheInt(Entry, Description, MetaClass) )
	{
		UClass* TempClass = FindObjectChecked<UClass>( ANY_PACKAGE, *MetaClass );
		TArray<FRegistryObjectInfo> List;

		GetRegistryObjects( List, UClass::StaticClass(), TempClass, 0 );
		for (INT i = 0; i < List.Num(); i++)
		{
			new(*Entry) FString(List(i).Object);
			new(*Description) FString(List(i).Description);
		}
	}

	unguardexec;
}
// --rjp
void AActor::execGetCacheEntry( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execGetCacheEntry);
	P_GET_INT(Num);
	P_GET_STR_REF(GUID);
	P_GET_STR_REF(Filename);
	P_FINISH;

	*GUID = TEXT("");
	*Filename = TEXT("");
	TCHAR IniName[256];
	FConfigCacheIni CacheIni;
	appSprintf( IniName, TEXT("%s") PATH_SEPARATOR TEXT("cache.ini"), *GSys->CachePath );
	TMultiMap<FString,FString>* Sec = CacheIni.GetSectionPrivate( TEXT("Cache"), 0, 1, IniName );
	if( Sec )
	{
		INT i = 0;
		for( TMultiMap<FString,FString>::TIterator It(*Sec); It; ++It )
		{
			if( *(*It.Value()) )
			{
				if( i == Num )
				{
					*GUID = *It.Key();
					*Filename = *It.Value();
					*(UBOOL*)Result = 1;
					return;
				}
				i++;
			}
		}
	}
	*(UBOOL*)Result = 0;
	unguardexec;
}

void AActor::execMoveCacheEntry( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execMoveCacheEntry);
	P_GET_STR(GUID);
	P_GET_STR_OPTX(NewFilename,TEXT(""));
	P_FINISH;

	*(UBOOL*)Result = 0;
	TCHAR IniName[256], OrigFilename[256];
	FConfigCacheIni CacheIni;
	appSprintf( IniName, TEXT("%s") PATH_SEPARATOR TEXT("cache.ini"), *GSys->CachePath );
	if( !CacheIni.GetString( TEXT("Cache"), *GUID, OrigFilename, ARRAY_COUNT(OrigFilename), IniName ) )
		return;
	if( !*(*NewFilename) )
		NewFilename = OrigFilename;
	INT i;
	while( (i=NewFilename.InStr(TEXT("/"))) != -1 )
		NewFilename = NewFilename.Mid(i+1);
	while( (i=NewFilename.InStr(TEXT("\\"))) != -1 )
		NewFilename = NewFilename.Mid(i+1);
	while( (i=NewFilename.InStr(TEXT(":"))) != -1 )
		NewFilename = NewFilename.Mid(i+1);

	// get file extension
	FString FileExt = NewFilename;
	i = FileExt.InStr( TEXT(".") );
	if( i == -1 )
	{
		debugf(TEXT("MoveCacheEntry: No extension: %s"), *NewFilename);
		return;
	}
	while( i != -1 )
	{
		FileExt = FileExt.Mid( i + 1 );
		i = FileExt.InStr( TEXT(".") );
	}
	FileExt = FString(TEXT(".")) + FileExt;
	
	// get the destination path from a Paths array entry such as ../Maps/*.unr
	for( i=0; i<GSys->Paths.Num(); i++ )
	{
		if( GSys->Paths(i).Right( FileExt.Len() ) == FileExt )
		{
			const TCHAR* p;
			for( p = *GSys->Paths(i) + GSys->Paths(i).Len() - 1; p >= *GSys->Paths(i); p-- )
				if( *p=='/' || *p=='\\' || *p==':' )
					break;

			FString DestPath = GSys->Paths(i).Left( p - *GSys->Paths(i) + 1 );
			debugf( TEXT("MoveCacheEntry: %s -> %s"), *(GSys->CachePath + FString(PATH_SEPARATOR) + GUID + GSys->CacheExt), *(DestPath + NewFilename) );
			if( GFileManager->Move( *(DestPath + NewFilename), *(GSys->CachePath + FString(PATH_SEPARATOR) + GUID + GSys->CacheExt) ) )
			{
				CacheIni.SetString( TEXT("Cache"), *GUID, TEXT(""), IniName );
				*(UBOOL*)Result = 1;
			}
			return;
		}
	}
	unguardexec;
}

void AActor::execGetURLMap( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execGetURLMap);

	P_GET_UBOOL_OPTX(bIncludeOptions,0);
	P_FINISH;

	UGameEngine* GE = CastChecked<UGameEngine>(GetLevel()->Engine);
	FString& StringResult = *(FString*)Result;

	StringResult = GE->LastURL.Map;
	if ( bIncludeOptions )
		StringResult += GE->LastURL.OptionString(1);

	unguardexec;
}

void AActor::execGetNextSkin( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execGetNextSkin);

	P_GET_STR(Prefix);
	P_GET_STR(CurrentSkin);
	P_GET_INT(Dir);
	P_GET_STR_REF(SkinName);
	P_GET_STR_REF(SkinDesc);
	P_FINISH;

	TArray<FRegistryObjectInfo> List;
	GetRegistryObjects( List, UTexture::StaticClass(), NULL, 0 );

	INT UseSkin = -1;
	INT FirstSkin = -1;
	INT PrevSkin = -1;
	INT UseNext = 0;

	INT n = appStrlen( *Prefix );

	for( INT i=0; i<List.Num(); i++ )
	{
		// if it matches the prefix
		if( appStrnicmp(*List(i).Object, *Prefix, n) == 0 )
		{
			if( UseNext )
			{
				UseSkin = i;
				UseNext = 0;
				break;
			}

			if( FirstSkin == -1 )
				FirstSkin = i;

			if( appStricmp(*List(i).Object, *CurrentSkin) == 0 ) 
			{
				if ( Dir == -1 )
				{
					UseSkin = PrevSkin;
					break;
				}
				else if ( Dir == 0 )
				{
					UseSkin = i;
					break;
				}
				else
				{
					UseNext = 1;
				}
			}
			PrevSkin = i;
		}	
	}
	
	if( UseNext )
		UseSkin = FirstSkin;

	// if we wanted to use the previous skin and
	// it didn't exist, choose the last skin.
	if( UseSkin == -1 )
		UseSkin = PrevSkin;

	if( UseSkin >= 0 && UseSkin < List.Num() )
	{
		*SkinName = List(UseSkin).Object;
		*SkinDesc = List(UseSkin).Description;
	}
	else
	{
		*SkinName = TEXT("");
		*SkinDesc = TEXT("");
	}

	unguardexec;
}

void AActor::execGetMapName( FFrame& Stack, RESULT_DECL )
{
	guard(AActor::execGetMapName);

	P_GET_STR(Prefix);
	P_GET_STR(MapName);
	P_GET_INT(Dir);
	P_FINISH;

	const TArray<FMapRecord>& Maps = *(const TArray<FMapRecord>*)(Cast<UCacheManager>(UCacheManager::StaticClass()->GetDefaultObject())->GetRecords(TEXT("Map")));
	for ( INT i = 0; i < Maps.Num(); i++ )
		if ( Maps(i).MapName == MapName )
		{
			*(FString*)Result = Maps(i).MapName;
			return;
		}
	

	*(FString*)Result = TEXT("");
	TCHAR Wildcard[256];
	TArray<FString> MapNames;
	appSprintf( Wildcard, TEXT("*.%s"), *FURL::DefaultMapExt );
	for( INT DoCD=0; DoCD<1+(GCdPath[0]!=0); DoCD++ )
	{
		for( INT i=0; i<GSys->Paths.Num(); i++ )
		{
			if( appStrstr( *GSys->Paths(i), Wildcard ) )
			{
				TCHAR Tmp[256]=TEXT("");
				if( DoCD )
				{
					appStrcat( Tmp, GCdPath );
					appStrcat( Tmp, TEXT("System") PATH_SEPARATOR );
				}
				appStrcat( Tmp, *GSys->Paths(i) );
				*appStrstr( Tmp, Wildcard )=0;
				appStrcat( Tmp, *Prefix );
				appStrcat( Tmp, Wildcard );
				TArray<FString>	TheseNames = GFileManager->FindFiles(Tmp,1,0);
				for( INT i=0; i<TheseNames.Num(); i++ )
				{
					INT j;
					for( j=0; j<MapNames.Num(); j++ )
						if( appStricmp(*MapNames(j),*TheseNames(i))==0 )
							break;
					if( j==MapNames.Num() )
						new(MapNames)FString(TheseNames(i));
				}
			}
		}
	}
	FString Seperator = FString::Printf(TEXT("\\"));
	for( INT i=0; i<MapNames.Num(); i++ )
	{
		// Strip out any leading path crap (for umod meta packages).
		while( MapNames(i).InStr( Seperator ) >= 0 )
		{
			INT Pos = MapNames(i).InStr( Seperator ) + 1;
			MapNames(i) = MapNames(i).Right( MapNames(i).Len() - Pos );
		}
	}
	for( INT i=0; i<MapNames.Num(); i++ )
	{
		if( appStrcmp(*MapNames(i),*MapName)==0 )
		{
			INT Offset = i+Dir;
			if( Offset < 0 )
				Offset = MapNames.Num() - 1;
			else if( Offset >= MapNames.Num() )
				Offset = 0;
			*(FString*)Result = MapNames(Offset);
			return;
		}
	}
	if( MapNames.Num() > 0 )
		*(FString*)Result = MapNames(0);
	else
		*(FString*)Result = FString(TEXT(""));

	unguardexec;
}

void AGameInfo::execLoadMapList(FFrame& Stack, RESULT_DECL )
{
	guard(GUIController::execGetMapList);

	P_GET_STR(Prefix);
	P_GET_TARRAY_REF(Maps, FString);
	P_FINISH;

	Maps->Empty();

	const TArray<FMapRecord>* CacheMaps = (const TArray<FMapRecord>*)(Cast<UCacheManager>(UCacheManager::StaticClass()->GetDefaultObject())->GetRecords(TEXT("Map")));
	for ( INT i = 0; i < CacheMaps->Num(); i++ )
		if ( Prefix.Len() == 0 || (*CacheMaps)(i).Acronym == Prefix )
            new(*Maps) FString((*CacheMaps)(i).MapName);

	return;
	TCHAR Wildcard[256];
	appSprintf( Wildcard, TEXT("*.%s"), *FURL::DefaultMapExt );

	for( INT DoCD=0; DoCD<1+(GCdPath[0]!=0); DoCD++ )
	{
		for( INT i=0; i<GSys->Paths.Num(); i++ )
		{
			if( appStrstr( *GSys->Paths(i), Wildcard ) )
			{
				TCHAR Tmp[256]=TEXT("");
				if( DoCD )
				{
					appStrcat( Tmp, GCdPath );
					appStrcat( Tmp, TEXT("System") PATH_SEPARATOR );
				}
				appStrcat( Tmp, *GSys->Paths(i) ); 
				*appStrstr( Tmp, Wildcard )=0;
				appStrcat( Tmp, *Prefix );
				appStrcat( Tmp, Wildcard );
				TArray<FString>	TheseNames = GFileManager->FindFiles(Tmp,1,0);
				for( INT j=0; j<TheseNames.Num(); j++ )
				{
					INT k=Maps->AddZeroed();
					(*Maps)(k) = TheseNames(j).Left(TheseNames(j).Len()-4);
				}
			}
		}
	}
	unguardexec;
}

