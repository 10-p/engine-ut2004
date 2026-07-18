/*=============================================================================
	FFileManagerAnsi.h: Unreal ANSI C based file manager.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "FFileManagerGeneric.h"
#include <dolphin/dvd.h>
/*-----------------------------------------------------------------------------
	File Manager.
-----------------------------------------------------------------------------*/

// File manager.
class FArchiveFileReaderGCN : public FArchive
{
public:
	FArchiveFileReaderGCN( DVDFileInfo& InFile, FOutputDevice* InError, INT InSize )
	:	File			( InFile )
	,	Error			( InError )
	,	Size			( InSize )
	,	Pos				( 0 )
	,	BufferBase		( 0 )
	,	BufferCount		( 0 )
	{
		guard(FArchiveFileReader::FArchiveFileReaderGCN);
		ArIsLoading = ArIsPersistent = 1;
		Opened = 1;
		
		Buffer = (BYTE*)OSAlloc(OSRoundUp32B(1024));
		BufferBase = -1;

		unguard;
	}
	~FArchiveFileReaderGCN()
	{
		guard(FArchiveFileReader::~FArchiveFileReaderGCN);
		OSFree(Buffer);
		Close();
		unguard;
	}
	void Precache( INT HintCount )
	{
	}
	void Seek( INT InPos )
	{
		guard(FArchiveFileReader::Seek);
		check(InPos>=0);
		check(InPos<=Size);
		
#if 0//ndef EMU
		debugf("Seeking to %d / %d", InPos & (~3), Size);
		// NOTE: For DVDSeekAsync, a value of 0 is an error, not success (silly nintendo)
		if (DVDSeek(&File, InPos & (~3)) != 0)
		{
			ArIsError = 1;
			Error->Logf( TEXT("seek Failed %i/%i: %i"), InPos, Size, Pos );
		}
#endif
		Pos         = InPos;
		BufferCount = 0;
		unguard;
	}
	INT Tell()
	{
		return Pos;
	}
	INT TotalSize()
	{
		return Size;
	}
	UBOOL Close()
	{
		guardSlow(FArchiveFileReader::Close);
		if (Opened)
			DVDClose( &File );
		Opened = false;
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		guardSlow(FArchiveFileReader::Serialize);
		if (Length < 1024)
		{
			if (BufferBase < 0 || Pos + Length >= BufferBase + 1024 || Pos < BufferBase)
			{
				INT ReadPos = Pos & (~3);
				INT ReadLength = 1024;
				if (Size - ReadPos < 1024) ReadLength = OSRoundUp32B(Size - ReadPos);
				DVDRead(&File, Buffer, ReadLength, ReadPos);
				
				BufferBase = ReadPos;
			}
			appMemcpy(V, Buffer + (Pos - BufferBase), Length);
		}
		else
		{
			unsigned char* TempBuffer = (unsigned char*)OSAlloc(OSRoundUp32B(Length + (Pos & 3)));
			DVDRead(&File, TempBuffer, OSRoundUp32B(Length + (Pos & 3)), Pos & (~3));
			appMemcpy(V, TempBuffer + (Pos & 3), Length);
			OSFree(TempBuffer);
		}
		Pos += Length;
		unguardSlow;
	}
protected:
	DVDFileInfo		File;
	FOutputDevice*	Error;
	INT				Size;
	INT				Pos;
	INT				BufferBase;
	INT				BufferCount;
	BYTE*			Buffer;
	UBOOL			Opened;
};

class FFileManagerGCN : public FFileManagerGeneric
{
public:
	FArchive* CreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		guard(FFileManagerAnsi::CreateFileReader);
		DVDFileInfo info;
		INT EntryNum = DVDConvertPathToEntrynum((char*)Filename);
		if( EntryNum == -1 )
		{
			if( Flags & FILEREAD_NoFail )
				appErrorf(TEXT("Failed to read file: %s"),Filename);
			return NULL;
		}
		DVDFastOpen(EntryNum, &info);
		return new(TEXT("AnsiFileReader"))FArchiveFileReaderGCN(info,Error,DVDGetLength(&info));
		unguard;
	}
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		return NULL;
	}
	UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
	{
		return true;
	}
	SQWORD GetGlobalTime( const TCHAR* Filename )
	{
		return 0;
	}
	UBOOL SetGlobalTime( const TCHAR* Filename )
	{
		return 0;
	}
	UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )
	{
		return 0;
	}
	UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )
	{
		return 0;
	}
	TArray<FString> FindFiles( const TCHAR* Filename, UBOOL Files, UBOOL Directories )
	{
		TArray<FString> Result;
		return Result;
	}
	UBOOL SetDefaultDirectory( const TCHAR* Filename )
	{
		DVDChangeDir((char*)Filename);
		return 0;
	}
	FString GetDefaultDirectory()
	{
		return FString();
	}
	void Init(UBOOL Startup)
	{
		if (Startup)
			DVDInit();
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

