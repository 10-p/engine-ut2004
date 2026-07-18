/*=============================================================================
	UExportCacheCommandlet.cpp: Cache record exporter.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Ron Prestenback.
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	UExportCacheCommandlet
-----------------------------------------------------------------------------*/
void UExportCacheCommandlet::StaticConstructor()
{
	guard(UExportCacheCommandlet::StaticConstructor);
	LogToStdout     = 0;
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 1;
	ShowErrorCount  = 1;

	unguard;
}

INT UExportCacheCommandlet::Main( const TCHAR* Parms )
{
	guard(UExportCacheCommandlet::Main);

	GIsRequestingExit = 1;
	GUglyHackFlags |= 64;  // disable loading of config / localization files
	UBOOL Verify = 0;

	DWORD Flags = 0;
	TArray<FString> PackageFiles, MapFiles;
	FString Wildcard;
	FString DestFile;

	if ( appStrlen(Parms) == 0 )
	{
		GWarn->Logf(TEXT("Use \"ucc help\" for help"));
		return 1;
	}

	while( ParseToken(Parms, Wildcard, 0) )
	{
		if (Wildcard == TEXT("-a") || Wildcard == TEXT("-Append"))
		{
			Flags |= EXPORT_Append;
			continue;
		}

		if (Wildcard == TEXT("-y") || Wildcard == TEXT("-Yes"))
		{
			Flags |= EXPORT_Overwrite;
			continue;
		}

		if ( Wildcard == TEXT("-v") || Wildcard == TEXT("-Verbose"))
		{
			Flags |= EXPORT_Verbose;
			continue;
		}

		if ( Wildcard == TEXT("-nocreate") )
		{
			Flags |= EXPORT_NoCreate;
			continue;
		}

		if ( Wildcard == TEXT("-Verify") )
		{
			Verify = 1;
			continue;
		}

		if ( Wildcard.InStr(TEXT("-mod=")) >= 0 )
			continue;

		if (Wildcard.Right(4) == TEXT(".ucl"))
		{
			DestFile = Wildcard;
			continue;
		}

		if ( Wildcard.InStr(*FURL::DefaultMapExt) == INDEX_NONE && Wildcard.InStr(TEXT(".u")) == INDEX_NONE && Wildcard.InStr(TEXT(".*")) == INDEX_NONE )
		   appErrorf(TEXT("Only .u and .ut2 supported! Usage: ucc exportcache [-a][-y][-v] <file.ext> [file.ext file.ext ...]"));

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
				if ( Wildcard != TEXT("*.*") )
				{
					*appStrstr( Test, TEXT("*.") ) = 0;
					appStrcat( Test, *Wildcard );
				}

				if ( ContainsMaps(GSys->Paths(i)) )
					MapFiles += GFileManager->FindFiles( Test, 1, 0 );
				else if ( ContainsPackages(GSys->Paths(i)) )
					PackageFiles += GFileManager->FindFiles(Test, 1, 0);
			}
		}
	}

	if ( MapFiles.Num() == 0 && PackageFiles.Num() == 0 )
	{
		GWarn->Logf(TEXT("No valid packages found!"));
		return 1;
	}

	if (DestFile != "")
	{
		const TCHAR* CacheExt = appStrstr(*GSys->CacheRecordPath(0), TEXT("*."));
		if ( CacheExt )
			CacheExt++;

		INT pos = DestFile.InStr(".");
		if ( pos != INDEX_NONE )
			DestFile = DestFile.Left(pos);

		if ( CacheExt )
			DestFile += CacheExt;
		else DestFile += TEXT(".ucl");

		if (GFileManager->IsReadOnly(*DestFile))
		{
			if ( !(Flags & EXPORT_Overwrite) && !GWarn->YesNof( LocalizeQuery( TEXT("OverwriteReadOnly"), TEXT("Core")), *DestFile) )
				appErrorf(TEXT("Could not write %s: Read Only"), *DestFile);
		}
	}

	LoadClass<UCacheManager>(NULL,TEXT("Engine.CacheManager"), NULL, 0, NULL);
	UCacheManager* Cache = UCacheManager::GetCache();
	check(Cache);

	FCacheTracker* Tracker = Cache->CreateTracker();
	check(Tracker);

	FCacheFileManager* FileManager = Cache->CreateFileManager( Flags );
	check(FileManager);

	Tracker->FoundUnregistered( TEXT("Map"), MapFiles );
	Tracker->FoundUnregistered( TEXT("Other"), PackageFiles );

	FileManager->Init();
	Cache->SaveNewPackages( Tracker, FileManager, NULL, DestFile == TEXT("") ? NULL : &DestFile );

	if ( Verify )
	{
		TArray<FString> DefaultItems;

		for ( INT i = 0; i < Cache->DefaultContent.Num(); i++ )
		{
			DefaultItems += Cache->DefaultContent(i).Maps;
			DefaultItems += Cache->DefaultContent(i).Classes;
		}

		TArray<FString> Lines;

		INT pos = INDEX_NONE;
		if ( FileManager->GetCacheLines(Lines) )
		{
			for ( INT i = 0; i < Lines.Num(); i++ )
			{
				FString& Line = Lines(i);

				pos = Line.InStr(TEXT("="));
				check(pos != INDEX_NONE );

				Line = Line.Mid(pos+1);
				pos = Line.InStr(TEXT("="));
				check(pos != INDEX_NONE);

				Line = Line.Mid(pos+1);
				pos = Line.InStr(TEXT(","));
				check(pos != INDEX_NONE);

				Line = Line.Left(pos);

				if ( Line.Left(1) == TEXT("\"") )
					Line = Line.Mid(1).LeftChop(1);

				for ( INT j = DefaultItems.Num() - 1; j >= 0; j-- )
					if ( DefaultItems(j) == Line )
					{
						DefaultItems.Remove(j);
						break;
					}
			}


			if ( DefaultItems.Num() > 0 )
			{
				GWarn->Logf(TEXT("***********   Obsolete default items found   ***********"));
				for ( INT i = 0; i < DefaultItems.Num(); i++ )
					GWarn->Logf(TEXT("  %i) %s"), i, *DefaultItems(i));
			}
			else GWarn->Logf(TEXT("***********   No obsolete default items found   ***********"));

			DefaultItems.Empty();
			for ( INT i = 0; i < Cache->DefaultContent.Num(); i++ )
			{
				DefaultItems += Cache->DefaultContent(i).Maps;
				DefaultItems += Cache->DefaultContent(i).Classes;
			}


			for ( INT i = 0; i < DefaultItems.Num(); i++ )
			{
				for ( INT j = Lines.Num() - 1; j >= 0; j-- )
				{
					if ( DefaultItems(i) == Lines(j) )
					{
						Lines.Remove(j);
						break;
					}
				}
			}

			if ( Lines.Num() > 0 )
			{
				GWarn->Logf(TEXT("***********   Missing Items   ***********"));
				for ( INT i = 0; i < Lines.Num(); i++ )
					GWarn->Logf(TEXT("  %i) %s"), i, *Lines(i));
			}
		}
	}

	delete Tracker;
	delete FileManager;

	return 0;

	unguard;
}
IMPLEMENT_CLASS(UExportCacheCommandlet);

