/*=============================================================================
	UMasterCommandlet.cpp: Unreal command-line installer release builder.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
	* Optimized by Andrew Scheidecker.
=============================================================================*/

#include "EditorPrivate.h"
#include "FFileManagerArc.h"
#include "../../Setup/Inc/Setup.h"

/*-----------------------------------------------------------------------------
	Master distribution image generator.
-----------------------------------------------------------------------------*/

class FArchiveWriter : public FArchive
{
public:

	FArchive*	DestFile;
	DWORD		ArchiveCRC;
	INT			Size;

	FArchiveWriter(const TCHAR* Filename)
	{
		guard(FArchiveWriter::FArchiveWriter);

		ArchiveCRC = 0;
		Size = 0;

		ArIsSaving = ArIsPersistent = 1;

		DestFile = GFileManager->CreateFileWriter(Filename);

		if(!DestFile)
			appErrorf(TEXT("Couldn't create archive file %s"),Filename);

		unguard;
	}

	~FArchiveWriter()
	{
		if(DestFile)
			delete DestFile;
	}

	INT WriteFile(FArchive* SourceFile)
	{
		guard(FArchiveWriter::WriteFile);

		BYTE	Buffer[32768];
		INT		BytesLeft = SourceFile->TotalSize();

		while(BytesLeft)
		{
			INT	Length = Min(32768,BytesLeft);

			SourceFile->Serialize(Buffer,Length);
			Serialize(Buffer,Length);

			BytesLeft -= Length;
		};

		return Size - SourceFile->TotalSize();

		unguard;
	}

	INT WriteData(TArray<BYTE>& Data)
	{
		guard(FArchiveWriter::WriteData);

		Serialize(&Data(0),Data.Num());

		return Size - Data.Num();

		unguard;
	}

	INT TotalSize()
	{
		return Size;
	}

	void Serialize(void* Data,INT Length)
	{
		ArchiveCRC = appMemCrc(Data,Length,ArchiveCRC);
		DestFile->Serialize(Data,Length);
		Size += Length;
	}
};

class UMasterCommandlet : public UCommandlet
{
	DECLARE_CLASS(UMasterCommandlet,UCommandlet,CLASS_Transient,Editor);

	// Variables.
	FString GConfigFile, GProduct, GRefPath, GMasterPath, GSrcPath, GArchive;
	UBOOL GMultiCD, GAutoNextCD;
	INT GCurrentCDNum;
	QWORD GMaxCDSize, GCurrentCDSize;

	FArchiveWriter*	GArchiveData;
	FArchiveHeader GArc;

	// Archive management.
	void LocalCopyFile( INT& CDNum, const TCHAR* Dest, const TCHAR* Src, DWORD Flags, UBOOL Compressed, DWORD Align )
	{
		guard(UMasterCommandlet::LocalCopyFile);

		if( !appIsPureAnsi(Dest) )
			appErrorf( TEXT("Non-ansi filename: %s"), Dest );

		if( GArchive!=TEXT("") )
		{
			FArchive*	SourceFile = GFileManager->CreateFileReader(Src);

			if(!SourceFile)
				appErrorf(TEXT("Failed to load file %s"),Src);

			while( GArchiveData->Size&(Align-1) )
				{BYTE B=0; *GArchiveData << B;}
			new(GArc._Items_) FArchiveItem(Dest,GArchiveData->WriteFile(SourceFile),SourceFile->TotalSize(),Flags);

			delete SourceFile;
		}
		else
		{
			UBOOL MoveTemp = 0;
			FString TempFile = GSrcPath * TEXT("_Temp.tmp");
			FString FullDest;
			if( GMultiCD )
			{
				if( CDNum == 0 )
				{
					if( GAutoNextCD )
					{
						INT CurrentFileSize;
						if( Compressed )
						{
							if( !GFileManager->MakeDirectory( *GSrcPath, 1 ) )
								appErrorf( TEXT("Failed to create directory %s"), *GSrcPath );
							GWarn->Logf( TEXT("   Copying %s to %s"), Src, *TempFile );
							if( GFileManager->Copy( *TempFile, Src, 1, 1, 0, FILECOPY_Compress, NULL ) != COPY_OK )
								appErrorf( TEXT("Failed to copy %s to %s"), Src, *TempFile );
							TempFile = TempFile + COMPRESSED_EXTENSION;
							CurrentFileSize = GFileManager->FileSize( *TempFile );
							MoveTemp = 1;
						}
						else
							CurrentFileSize = GFileManager->FileSize(Src);

						// see if the current file fits on the current CD.
						if( GCurrentCDSize + CurrentFileSize > GMaxCDSize )
						{
							GCurrentCDSize = 0;
							GCurrentCDNum++;
						}
						GCurrentCDSize += CurrentFileSize;
						CDNum = GCurrentCDNum;
					}
					else
						CDNum = 1;
				}

				FullDest = GSrcPath * FString::Printf(TEXT("CD%d"),CDNum) * Dest;
				if( MoveTemp )
					FullDest = FullDest + COMPRESSED_EXTENSION;
			}
			else
				FullDest = GSrcPath * Dest;

			// Copy the file.
			FString DestDir = BasePath(*FullDest);
			FString DestFile = BaseFilename(*FullDest);
			if( GArchive==TEXT("") && !GFileManager->MakeDirectory( *DestDir, 1 ) )
				appErrorf( TEXT("Failed to create directory %s"), *DestDir );

			if( MoveTemp )
			{		
				GWarn->Logf( TEXT("   Compressed to %d%%"), 100 * GFileManager->FileSize(*TempFile) / GFileManager->FileSize(Src) );
				GWarn->Logf( TEXT("   Moving %s to %s"), *TempFile, *FullDest );
				if( !GFileManager->Move( *FullDest, *TempFile, 1, 1, 0 ) )
					appErrorf( TEXT("Failed to move %s to %s"), *TempFile, *FullDest );
			}
			else
			{
				GWarn->Logf( TEXT("   Copying %s to %s"), Src, *FullDest );
				if( GFileManager->Copy( *FullDest, Src, 1, 1, 0, Compressed ? FILECOPY_Compress : FILECOPY_Normal, NULL ) != COPY_OK )
					appErrorf( TEXT("Failed to copy %s to %s"), Src, *FullDest );
			}
		}
		unguard;
	}

	// File diffing.
	struct FLink
	{
		INT Offset;
		FLink* Next;
		FLink( INT InOffset, FLink* InNext )
		: Offset( InOffset ), Next( InNext )
		{}
	};
	enum {ARRAY_SIZE=65536*64};
	enum {MIN_RUNLENGTH=10};
	INT ArrayCrc( const TArray<BYTE>& T, INT Offset )
	{
		return appMemCrc( &T(Offset), Min((INT)MIN_RUNLENGTH,T.Num()-Offset) ) & (ARRAY_SIZE-1);
	}
	void Decompress( TArray<BYTE>& New, TArray<BYTE>& Delta, TArray<BYTE> Old )
	{
		guard(UMasterCommandlet::Decompress);
		INT Magic=0, PrevSpot=0, OldSize=0, OldCRC=0, NewSize=0, NewCRC=0;
		FBufferReader Reader( Delta );
		Reader << Magic << OldSize << OldCRC << NewSize << NewCRC;
		check(Magic==0x92f92912);
		check(OldSize==Old.Num());
		check(OldCRC==(INT)appMemCrc(&Old(0),Old.Num()));
		while( !Reader.AtEnd() )
		{
			INT Index;
			Reader << AR_INDEX(Index);
			if( Index<0 )
			{
				INT Start = New.Add( -Index );
				Reader.Serialize( &New(Start), -Index );
			}
			else
			{
				INT CopyPos;
				Reader << AR_INDEX(CopyPos);
				CopyPos += PrevSpot;
				check(CopyPos>=0);
				check(CopyPos+Index<=Old.Num());
				INT Start = New.Add( Index );
				appMemcpy( &New(Start), &Old(CopyPos), Index );
				PrevSpot = CopyPos + Index;
			}
		}
		check(NewSize==New.Num());
		check(NewCRC==(INT)appMemCrc(&New(0),New.Num()));
		unguard;
	}
	UBOOL DeltaCode( const TCHAR* RefFilename, const TCHAR* MasterFilename, const TCHAR* SrcFilename )
	{
		guard(UMasterCommandlet::DeltaCode);

		// Load files, and delete the delta file.
		DOUBLE StartTime = appSeconds();
		FBufferArchive Data;
		TArray<BYTE> Old, New;
		FLink** Starts = new FLink*[ARRAY_SIZE];
		GWarn->Logf( TEXT("   Delta compressing %s to %s"), MasterFilename, *(GSrcPath*SrcFilename) );
		GWarn->Logf( TEXT("      Relative to %s"), RefFilename );
		if( !appLoadFileToArray(Old,RefFilename) )
			appErrorf( TEXT("Can't load ref file %s"), RefFilename );
		if( !appLoadFileToArray(New,MasterFilename) )
			appErrorf( TEXT("Can't load src file %s"), MasterFilename );

		// See if an exact match delta coded file exists.
		UBOOL Done=0;
		FString CachedDelta = BaseFilename(SrcFilename)+TEXT("_bak");
		if( appLoadFileToArray(Data,*CachedDelta) )
		{
			GWarn->Logf( TEXT("   Examining cached delta %s"), *(GSrcPath*SrcFilename) );
			TArray<BYTE> Test;
			Decompress( Test, Data, Old );
			if( Test.Num()==New.Num() )
			{
				INT i;
				for( i=0; i<Test.Num(); i++ )
					if( Test(i)!=New(i) )
						break;
				if( i==Test.Num() )
					Done=1;
			}
		}
		if( !Done )
		{
			// Save header.
			Data.Empty();
			INT	Magic=0x92f92912, OldSize=Old.Num(), OldCRC=appMemCrc(&Old(0),Old.Num()), NewSize=New.Num(), NewCRC=appMemCrc(&New(0),New.Num());
			Data << Magic << OldSize << OldCRC << NewSize << NewCRC;

			// Delta compress the files.
			GWarn->Logf( TEXT("Preprocessing...") );
			for( INT i=0; i<ARRAY_SIZE; i++ )
			{
				Starts[i] = NULL;
			}
			for( INT i=0; i<Old.Num(); i++ )
			{
				if( (i&1023)==0 )
					GWarn->Serialize( *FString::Printf( TEXT("Processed %i/%iK"), i/1024, Old.Num()/1024), NAME_Progress );
				INT Index = ArrayCrc(Old,i);
				Starts[Index] = new FLink(i,Starts[Index]);
			}
			GWarn->Logf( TEXT("\nCompressing...") );
			for( INT NewPos=0,LiteralStart=0,PrevSpot=0; NewPos<=New.Num(); )
			{
				INT BestPos=0, BestRunLength=0;
				for( FLink* Link=Starts[ArrayCrc(New,NewPos)]; Link; Link=Link->Next )
				{
					INT RunLength;
					for( RunLength=0; Link->Offset+RunLength<Old.Num() && NewPos+RunLength<New.Num() && Old(Link->Offset+RunLength)==New(NewPos+RunLength); RunLength++ );
					if( RunLength > BestRunLength )
					{
						BestRunLength = RunLength;
						BestPos = Link->Offset;
					}
				}
				if( (BestRunLength>=MIN_RUNLENGTH || NewPos==New.Num()) && LiteralStart<NewPos )
				{
					INT NegativeLiteralCount = LiteralStart - NewPos;
					Data << AR_INDEX(NegativeLiteralCount);
					Data.Serialize( &New(LiteralStart), -NegativeLiteralCount );
				}
				if( BestRunLength>=MIN_RUNLENGTH )
				{
					INT DeltaPos = BestPos - PrevSpot;
					Data << AR_INDEX(BestRunLength) << AR_INDEX(DeltaPos);
					NewPos += BestRunLength;
					PrevSpot = BestPos + BestRunLength;
					LiteralStart = NewPos;
				}
				else NewPos++;

				if( (NewPos&1023) == 0 )
					GWarn->Serialize( *FString::Printf( TEXT("Processed %i/%iK"), NewPos/1024, Old.Num()/1024), NAME_Progress );
			}
			GWarn->Logf( TEXT("      Result size %i (%5.3f%%) Time = %5.2f Min"), Data.Num(), 100.0*Data.Num()/New.Num(), (appSeconds()-StartTime)/60.0 );

			// Reconstruct the new file.
			TArray<BYTE> Test;
			Decompress( Test, Data, Old );
			if( Test.Num()!=New.Num() )
				appErrorf( TEXT("%i %i"), Test.Num(), New.Num() );
			for( INT i=0; i<Test.Num(); i++ )
				if( Test(i)!=New(i) )
					appErrorf( TEXT("%i %i %i"), i, Test(i), New(i) );
		}

		// Save delta to disk.
		if( GArchiveData )
			new(GArc._Items_)FArchiveItem(SrcFilename,GArchiveData->WriteData(Data),Data.Num(),0);
		else appSaveArrayToFile( Data, *(GSrcPath*SrcFilename) );
		appSaveArrayToFile( Data, *CachedDelta );

		// Cleanup.
		for( INT i=0; i<ARRAY_SIZE; i++ )
		{
			if( Starts[i]!=NULL )
			{
				delete Starts[i];
				Starts[i] = NULL;
			}
		}
		delete Starts;

		return 1;
		unguard;
	}

	// Process a group in advance.
	void UpdateGroup( FString MasterPath, const TCHAR* Group, TMultiMap<FString,FString>& Map )
	{
		guard(UpdateGroup);
		GWarn->Logf( TEXT("   Processing group %s"), Group );
		TMultiMap<FString,FString> AllFiles;
		{for( TMultiMap<FString,FString>::TIterator It(Map); It; ++It )
		{
			if( It.Key()==TEXT("File") || It.Key()==TEXT("Copy") )
			{
				// Expand wildcard.
				FFileInfo Info( It.Value() );
				check(Info.Src!=TEXT(""));
				FString Master = Info.Master!=TEXT("") ? Info.Master : Info.Src;
				FString Src    = Info.Src;
				if( Master.InStr(TEXT("*"))>=0 )
				{
					GWarn->Logf( TEXT("   Expanding wildcard %s"), *Master );
					TArray<FString> Files
					=	Info.MasterRecurse
					?	FindFilesRecursive( MasterPath * BasePath(Master), BaseFilename(Master) )
					:	GFileManager->FindFiles( *(MasterPath * BasePath(Master) * BaseFilename(Master)), 1, 0 );
					for( INT i=0; i<Files.Num(); i++ )
					{
						FFileInfo NewInfo(Info);
						NewInfo.Src           = BasePath(*Src) * Files(i);
						NewInfo.Master        = BasePath(Master) * Files(i);
						NewInfo.MasterRecurse = 0;
						FStringOutputDevice Out;
						NewInfo.Write( Out, 1 );
						AllFiles.Add( *It.Key(), *Out );
					}
				}
				else AllFiles.Add( *It.Key(), *It.Value() );
			}
		}}
		Map.Remove( TEXT("File") );
		Map.Remove( TEXT("Copy") );
		{for( TMultiMap<FString,FString>::TIterator It(AllFiles); It; ++It )
		{
			// Compose filenames.
			FFileInfo Info( *It.Value() );
			check(Info.Src!=TEXT(""));
			FString Master = MasterPath * (Info.Master!=TEXT("") ? Info.Master : Info.Src);

			// Update size.
			Info.Size = GFileManager->FileSize( *Master );
			if( GRefPath==TEXT("") )
				Info.Ref = TEXT("");
			if( Info.Ref!=TEXT("") )
				Info.RefSize = GFileManager->FileSize( *(GRefPath*Info.Ref) );
			if( Info.Size<0 )
				appErrorf( TEXT("Missing file %s"), *Master );
			FStringOutputDevice Str;
			Info.Write( Str, 1 );

			// Add to list.
			Map.Add( *It.Key(), *Str );
		}}
		unguard;
	}

	// Copy a group.
	void CopyGroup( FString MasterPath, const TCHAR* Group, TMultiMap<FString,FString>& Map )
	{
		guard(CopyGroup);
		GWarn->Logf( TEXT("   Copying group %s"), Group );

		INT CDNum = 0;
		if( GMultiCD )
		{
			FString* CDNumPtr = Map.Find( TEXT("CDNumber") );
			CDNum = CDNumPtr ? appAtoi( *(*CDNumPtr) ) : 1;
			if( CDNumPtr && GAutoNextCD )
			{
				// Use NoSplit to ensure group isn't split between CDs
				if( *CDNumPtr == TEXT("NoSplit") )
				{
	                //!! Warning, goes by uncompressed filesizes.
					QWORD GroupSize = 0;
					for( TMultiMap<FString,FString>::TIterator It(Map); It; ++It )
					{
						if( It.Key()==TEXT("File") || It.Key()==TEXT("Copy") )
						{
							FFileInfo Info( It.Value() );
							GroupSize += Info.Size;
						}
					}
					if( GroupSize + GCurrentCDSize > GMaxCDSize )
						CDNum = GCurrentCDNum + 1;
					else
						CDNum = GCurrentCDNum;
				}
				if( *CDNumPtr == TEXT("New") )
					CDNum = GCurrentCDNum + 1;
				if( CDNum < GCurrentCDNum )
					appErrorf( TEXT("   Group %s specifies previous CD number with AutoNextCD."), Group);
				if( CDNum != GCurrentCDNum )
					GCurrentCDSize = 0;

				// Start group on a new CD
				GCurrentCDNum = CDNum;
				// Use auto CD allocation
				CDNum = 0;
				Map.Set( TEXT("CDNumber"), appItoa(GCurrentCDNum) );
			}
			else
				Map.Set( TEXT("CDNumber"), appItoa(CDNum) );
		}

		for( TMultiMap<FString,FString>::TIterator It(Map); It; ++It )
		{
			if( It.Key()==TEXT("File") || It.Key()==TEXT("Copy") )
			{
				// Compose filenames.
				FFileInfo Info( It.Value() );
				check(Info.Src!=TEXT(""));
				FString Master = MasterPath * (Info.Master!=TEXT("") ? Info.Master : Info.Src);
				if( GRefPath==TEXT("") || Info.Ref==TEXT("") )
				{
					LocalCopyFile( Info.CDNum, *Info.Src, *Master, Info.Flags, Info.Compressed, 16 );
					if( Info.Compressed )
						Info.CompSize = GFileManager->FileSize( *(Info.Src + COMPRESSED_EXTENSION) );

					// Update info
					FStringOutputDevice Str;
					Info.Write( Str, 1 );
					It.Value() = *Str;
				}
				else
				{
					FString SrcDir = BasePath(*(GSrcPath * Info.Src));
					if( GArchive==TEXT("") && !GFileManager->MakeDirectory( *SrcDir, 1 ) )
						appErrorf( TEXT("Failed to create directory %s"), *SrcDir );

					FString Ref = GRefPath * Info.Ref;
					DeltaCode( *Ref, *Master, *Info.Src );
				}
			}
		}
		unguard;
	}

	// Recursively process all groups.
	void ProcessGroup( FString MasterPath, const TCHAR* File, const TCHAR* Group, void(UMasterCommandlet::*Process)( FString MasterPath, const TCHAR* Group, TMultiMap<FString,FString>& Map ) )
	{
		guard(ProcessGroup);
		TMultiMap<FString,FString>* Map = GConfig->GetSectionPrivate( Group, 0, 1, File );
		if( !Map )
			appErrorf( TEXT("Group '%s' not found in file '%s'"), Group, File );
		FString Str;
		if( GConfig->GetString( Group, TEXT("MasterPath"), Str, File ) )
			MasterPath = appFormat(Str,*GConfig->GetSectionPrivate(TEXT("Setup"),1,1,File));
		(this->*Process)( MasterPath, Group, *Map );
		for( TMultiMap<FString,FString>::TIterator It(*Map); It; ++It )
			if( It.Key()==TEXT("Group") )
				ProcessGroup( MasterPath, File, *It.Value(), Process );

		unguard;
	}

	// Static constructor.
	void StaticConstructor()
	{
		guard(UMasterCommandlet::StaticConstructor);
		LogToStdout = 0;
		IsClient    = 0;
		IsEditor    = 0;
		IsServer    = 0;
		LazyLoad    = 1;
		unguard;
	}

	// Main.
	INT Main( const TCHAR* Parms )
	{
		guard(UMasterCommandlet::Main);
		INT i;

		// Delete all manifest files.
		TArray<FString> Manifests = GFileManager->FindFiles( MANIFEST_FILE TEXT(".*"), 1, 0 );
		for( i=0; i<Manifests.Num(); i++ )
			if( !GFileManager->Delete( *Manifests(i), 1, 1 ) )
				appErrorf( TEXT("Failed to delete manifest file: %s"), *Manifests(i) );

		// Get configuration file.
		GConfigFile = MANIFEST_FILE MANIFEST_EXT;
		FString ConfigBase;
		if( !ParseToken( Parms, ConfigBase, 0 ) )
			appErrorf( TEXT("Config (%s) filename not specified"), MANIFEST_EXT );
		if( ConfigBase.Right(4)!=MANIFEST_EXT )
			ConfigBase += MANIFEST_EXT;
		if( GFileManager->FileSize(*ConfigBase)<0 )
			appErrorf( TEXT("Can't find config file %s"), *ConfigBase );
		if( GFileManager->Copy( *GConfigFile, *ConfigBase, 1, 1, 0, NULL ) != COPY_OK )
			appErrorf( TEXT("Error copying config file %s to %s"), *ConfigBase, *GConfigFile );
		GWarn->Logf( TEXT("Using config: %s"), *ConfigBase );

		// Copy all localized manifest files.
		UBOOL GotIntManifest = 0;
		TArray<FString> List = GFileManager->FindFiles( *(ConfigBase.LeftChop(4) + TEXT(".*") ), 1, 0 );
		for( i=0; i<List.Num(); i++ )
		{
			INT Pos = List(i).InStr(TEXT("."),1);
			if( Pos>=0 )
			{
				FString Ext = List(i).Mid(Pos);
				if( Ext!=MANIFEST_EXT )
				{
					FString Str;
					GConfig->Detach(*List(i));
					if( List(i)!=GConfigFile && GConfig->GetString(TEXT("Setup"),TEXT("LocalProduct"),Str,*List(i)) )
					{
						FString Dest = FString(MANIFEST_FILE)+Ext;
						GWarn->Logf( TEXT("   Copying manifest %s to %s"), *List(i), *Dest );
						if( GFileManager->Copy( *Dest, *List(i), 0, 0, 0, NULL ) != COPY_OK )
							appErrorf( TEXT("Failed to copy manifest file: %s"), *Dest );
						if( Ext==TEXT(".int") )
							GotIntManifest = 1;
					}
				}
			}
		}
		if( !GotIntManifest )
			appErrorf( TEXT("Failed to create Manifest.int") );

		// Copy command line parameters to [Build] section.
		FString Temp;
		while( ParseToken(Parms,Temp,0) )
		{
			INT Pos = Temp.InStr(TEXT("="));
			if( Pos<0 )
				appErrorf( TEXT("Option '%' unrecognized"), *Temp );
			GConfig->SetString( TEXT("Setup"), *Temp.Left(Pos), *Temp.Mid(Pos+1), *GConfigFile );
		}

		// Set version if it's not defined.
		FString TempVersion;
		if( !GConfig->GetString( TEXT("Setup"), TEXT("Version"), TempVersion, *GConfigFile ) )
		{
			TempVersion = FString::Printf(TEXT("%d"), ENGINE_VERSION);
			GConfig->SetString( TEXT("Setup"), TEXT("Version"), *TempVersion, *GConfigFile );
		}

		// Init.
		GConfig->GetString( TEXT("Setup"), TEXT("Archive"), GArchive, *GConfigFile );
		GArchive = appFormat(GArchive,*GConfig->GetSectionPrivate(TEXT("Setup"),1,1,*GConfigFile));
		GConfig->GetString( TEXT("Setup"), TEXT("RefPath"), GRefPath, *GConfigFile );
		if( !GConfig->GetString( TEXT("Setup"), TEXT("MasterPath"), GMasterPath, *GConfigFile ) )
			appErrorf( TEXT("Missing MasterPath=") );
		if( !GConfig->GetString( TEXT("Setup"), TEXT("SrcPath"), GSrcPath, *GConfigFile ) )
			appErrorf( TEXT("Missing SrcPath=") );
		GSrcPath = appFormat(GSrcPath,*GConfig->GetSectionPrivate(TEXT("Setup"),1,1,*GConfigFile));
		if( !GConfig->GetBool( TEXT("Setup"), TEXT("MultiCD"), GMultiCD, *GConfigFile ) )
			GMultiCD = 0;
		if( !GConfig->GetBool( TEXT("Setup"), TEXT("AutoNextCD"), GAutoNextCD, *GConfigFile ) )
			GAutoNextCD = 0;
		INT m;
		if( GConfig->GetInt( TEXT("Setup"), TEXT("MaxCDSize"), m, *GConfigFile ) )
			GMaxCDSize = m;
		else
			GMaxCDSize = 671088640;
		
		// for AutoNextCD.
		GCurrentCDSize = 0;
		GCurrentCDNum = 1;

		if( GArchive==TEXT("") )
		{
			GArchiveData = NULL;

			// Make dest path.
			if( !GFileManager->DeleteDirectory( *GSrcPath, 0, 1 ) )
				appErrorf( TEXT("Failed to remove directory tree: %s"), *GSrcPath );
			if( !GFileManager->MakeDirectory( *GSrcPath, 1 ) )
				appErrorf( TEXT("Failed to create directory: %s"), *GSrcPath );
		}
		else
		{
			GArchiveData = new FArchiveWriter(*(GSrcPath * GArchive));

			if( GArchive.Right(4)==TEXT(".exe") )
			{
				// Write stub to self-extracting exe.
				GFileManager->Delete( *GArchive, 1, 0 );
				TArray<BYTE> Buffer;
				verify(appLoadFileToArray(Buffer,SFX_STUB));
				GArchiveData->WriteData(Buffer);
			}
		}

		// Process and copy the groups.
		ProcessGroup( GMasterPath, *GConfigFile, TEXT("Setup"), &UMasterCommandlet::UpdateGroup );
		GConfig->Flush( 0 );
		// Switch to the destination config file so we can update the cd number.
		if( GMultiCD )
		{
			FString DestDir = GSrcPath * TEXT("CD1\\System");
			if( !GFileManager->MakeDirectory( *DestDir, 1 ) )
				appErrorf( TEXT("Failed to create directory %s"), *DestDir );
			FString DestConfigFile = DestDir * GConfigFile;
			if( GFileManager->Copy( *DestConfigFile, *GConfigFile, 1, 1, 0, NULL ) != COPY_OK )
				appErrorf( TEXT("Error copying config file %s to %s"), *GConfigFile, *DestConfigFile );
			GConfigFile = DestConfigFile;
		}		
		ProcessGroup( GMasterPath, *GConfigFile, TEXT("Setup"), &UMasterCommandlet::CopyGroup );
		if( GMultiCD )
			GConfig->Flush( 0 );

		// Flush archive.
		if( GArchiveData )
		{
			GWarn->Logf( TEXT("   Saving archive: %s"), *(GSrcPath * GArchive) );
			GArc.TableOffset = GArchiveData->TotalSize();
			*GArchiveData << GArc._Items_;
			GArc.CRC      = GArchiveData->ArchiveCRC;
			GArc.FileSize = GArchiveData->TotalSize() + ARCHIVE_HEADER_SIZE;
			*GArchiveData << GArc;

			delete GArchiveData;
		}
		unguard;
		return 1;
	}
};
IMPLEMENT_CLASS(UMasterCommandlet)

/*-----------------------------------------------------------------------------
	UUpdateUModCommandlet - update a umod with a new file
-----------------------------------------------------------------------------*/
class UUpdateUModCommandlet : public UCommandlet
{
	DECLARE_CLASS(UUpdateUModCommandlet,UCommandlet,CLASS_Transient,Editor);

	INT Main( const TCHAR* Parms )
	{
		guard(UUpdateUModCommandlet::Main);
	
		FString UModFile, Command, File;
		if( !ParseToken( Parms, UModFile, 0 ) )
			appErrorf( TEXT("UMod archive file not specified") );

		GWarn->Logf( TEXT("Loading Archive: %s"), *UModFile );

		FBufferArchive DestArc;
		FArchiveHeader SrcHdr;
		FArchiveHeader DestHdr;

		// load source UMod to memory
		TArray<BYTE> SrcBuffer;
		verify(appLoadFileToArray( SrcBuffer, *UModFile ));
		FBufferReader SrcArc( SrcBuffer );
		SrcArc.Seek( SrcArc.TotalSize() - ARCHIVE_HEADER_SIZE );
		SrcArc << SrcHdr;
		SrcArc.Seek( SrcHdr.TableOffset );
		SrcArc << SrcHdr._Items_;
		
		if( 
			ParseToken( Parms, Command, 0 ) && 
			( Command.Caps()==TEXT("LIST") || Command.Caps()==TEXT("REPLACE") || 
			  Command.Caps()==TEXT("ADD") || Command.Caps()==TEXT("DELETE") ||
			  Command.Caps()==TEXT("EXTRACT") )
		  )
		{
			if( Command.Caps()==TEXT("LIST") )
			{
				// list contents of archive
				GWarn->Logf( TEXT("") );
				GWarn->Logf( TEXT("%-30s%10s%10s"), TEXT("Filename"), TEXT("Size"), TEXT("Offset") );
				GWarn->Logf( TEXT("%-30s%10s%10s"), TEXT("--------"), TEXT("----"), TEXT("------") );

				for( INT i=0; i<SrcHdr._Items_.Num(); i++ )
					GWarn->Logf( TEXT("%-30s%10d%10d"), *SrcHdr._Items_(i)._Filename_, SrcHdr._Items_(i).Size, SrcHdr._Items_(i).Offset );
			}
			else
			{
				if( !ParseToken( Parms, File, 0 ) )
					appErrorf( TEXT("Filename not specified for %s"), *Command.Caps() );

				if( Command.Caps()==TEXT("EXTRACT") )
				{
					for( INT i=0; i<SrcHdr._Items_.Num(); i++ )
					{
						if( SrcHdr._Items_(i)._Filename_.Caps() == File.Caps() || 
							SrcHdr._Items_(i)._Filename_.Caps().Right(File.Len()+1) == FString(TEXT("\\"))+File.Caps() )
						{
							GFileManager->Delete( *File, 1, 0 );
							FArchive* DestFile = GFileManager->CreateFileWriter( *File,FILEWRITE_EvenIfReadOnly );
							if( !DestFile )
								appErrorf( TEXT("Failed saving file: %s"), *File );				
							DestFile->Serialize( &SrcBuffer(SrcHdr._Items_(i).Offset), SrcHdr._Items_(i).Size );
							delete DestFile;
							GWarn->Logf( TEXT("Saved %s"), *File );
							return 1;
						}
					}
					appErrorf( TEXT("Could not locate file %s in archive"), *File );
				}
				else
				{
					FString UpdatePath = TEXT("");
					INT UpdateFlags=0;

					if( Command.Caps()==TEXT("DELETE") || Command.Caps()==TEXT("REPLACE") )
					{
						for( INT i=0; i<SrcHdr._Items_.Num(); i++ )
						{
							if( SrcHdr._Items_(i)._Filename_.Caps() == File.Caps() || 
								SrcHdr._Items_(i)._Filename_.Caps().Right(File.Len()+1) == FString(TEXT("\\"))+File.Caps() )
							{
								// save path and flags
								UpdatePath = SrcHdr._Items_(i)._Filename_;
								UpdateFlags = SrcHdr._Items_(i).Flags;
								SrcHdr._Items_.Remove(i);
								//GWarn->Logf( TEXT("UpdatePath is %s, file is %s"), *UpdatePath, *File );
								break;
							}			
						}
					}
					// create destination UMod
					if( UModFile.Right(4).Caps()==TEXT(".EXE") )
					{
						// Write stub to self-extracting exe.
						TArray<BYTE> SFXBuffer;
						verify(appLoadFileToArray(SFXBuffer,SFX_STUB));
						DestArc.Serialize( &SFXBuffer(0), SFXBuffer.Num() );
					}
					for( INT i=0; i<SrcHdr._Items_.Num(); i++ )
					{
						new(DestHdr._Items_)FArchiveItem(*SrcHdr._Items_(i)._Filename_,DestArc.Num(),SrcHdr._Items_(i).Size,SrcHdr._Items_(i).Flags);
						DestArc.Serialize( &SrcBuffer(SrcHdr._Items_(i).Offset), SrcHdr._Items_(i).Size );
					}
					if( Command.Caps()==TEXT("ADD") || Command.Caps()==TEXT("REPLACE") )
					{
						TArray<BYTE> Data;
						if( !appLoadFileToArray( Data, *File ) )
							appErrorf( TEXT("Failed to load file %s"), *File );

						if( UpdatePath == TEXT("") )
							UpdatePath = File;
						new(DestHdr._Items_)FArchiveItem(*UpdatePath,DestArc.Num(),Data.Num(),UpdateFlags);
						DestArc.Serialize( &Data(0), Data.Num() );
					}

					UModFile = UModFile+TEXT(".new");
					GWarn->Logf( TEXT("Saving archive: %s"), *UModFile );
					DestHdr.TableOffset = DestArc.Num();
					DestArc << DestHdr._Items_;
					DestHdr.CRC      = appMemCrc( &DestArc(0), DestArc.Num(), 0 );
					DestHdr.FileSize = DestArc.Tell() + ARCHIVE_HEADER_SIZE;
					DestArc << DestHdr;

					// save archive to disk
					GFileManager->Delete( *UModFile, 1, 0 );
					if( !appSaveArrayToFile( DestArc, *UModFile ) )
						appErrorf( TEXT("Failed saving archive: %s"), *UModFile );
				}
			}
		}
		else
			appErrorf( TEXT("You must specify one of LIST, EXTRACT, ADD, DELETE or REPLACE.") );

		unguard;
		return 1;
	}
};
IMPLEMENT_CLASS(UUpdateUModCommandlet);

class UChecksumPackageCommandlet : public UCommandlet
{
	DECLARE_CLASS(UChecksumPackageCommandlet,UCommandlet,CLASS_Transient,Editor);

	INT Main( const TCHAR* Parms )
	{
		guard(UChecksumPackageCommandlet::Main);
	
		FString FileName;
		if( !ParseToken( Parms, FileName, 0 ) )
			appErrorf( TEXT("Package filename not specified") );

		INT Space = FileName.InStr(TEXT(" "));
		FileName = FileName.Right( FileName.Len() - (Space+1) );
		INT FileSize = GFileManager->FileSize( *FileName );

		// Checksum the .u files.
		FString CheckString = FileName.Caps() + FString::Printf( TEXT("%i"), FileSize );
		if (FileSize != -1)
		{
			FMD5Context PContext;
			appMD5Init( &PContext );
			appMD5Update( &PContext, (BYTE*) *CheckString, CheckString.Len() * sizeof(TCHAR) );
			BYTE Digest[16];
			appMD5Final( Digest, &PContext );
			FString Checksum;
			for (INT j=0; j<16; j++)
				Checksum += FString::Printf(TEXT("%02x"), Digest[j]);
			GWarn->Logf( TEXT("%s checksum: %s"), *FileName, *Checksum );
		}
		unguard;
		return 1;
	}
};
IMPLEMENT_CLASS(UChecksumPackageCommandlet);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

