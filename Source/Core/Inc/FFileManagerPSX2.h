/*=============================================================================
	FFileManagerPSX2.h: Unreal PSX2 based file manager.
	Copyright 1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include <sifdev.h>
#include <libcdvd.h>
#include "FFileManagerGeneric.h"

/*-----------------------------------------------------------------------------
	File Manager.
-----------------------------------------------------------------------------*/

// File manager.
struct FArchiveFileReaderHost : public FArchive
{
	BYTE			Buffer[2048];
	FOutputDevice*  Error;
	INT				File, Size, Pos, BufferBase, BufferCount;
	FArchiveFileReaderHost(INT InFile, FOutputDevice* InError)
	: File(InFile), Error(InError), Pos(0), BufferBase(0), BufferCount(0)
	{
		Size=sceLseek(File,0,SCE_SEEK_END);
		sceLseek(File,0,SCE_SEEK_SET);
		ArIsLoading = ArIsPersistent = 1;
	}
	~FArchiveFileReaderHost()
	{
		Close();
	}
	UBOOL Close()
	{
		if( File>=0 )
			sceClose(File);
		File=-1;
		return !ArIsError;
	}
	INT Tell()
	{
		return Pos;
	}
	INT TotalSize()
	{
		return Size;
	}
	void Seek(INT InPos)
	{
		Pos = InPos;
	}
	void Serialize(void* V, INT Length)
	{
		for( INT Start=Pos,End=Pos+Length,Copy; Pos<End; Pos+=Copy )
		{
			if( Pos<BufferBase || Pos>=BufferBase+BufferCount )
			{
				BufferBase     = Pos&~(ARRAY_COUNT(Buffer)-1);
				BufferCount    = Min<INT>(ARRAY_COUNT(Buffer),Size-BufferBase);
				verify(sceLseek(File,BufferBase,SCE_SEEK_SET)>=0);
				verify(sceRead(File,Buffer,BufferCount)>=0);
			}
			Copy = Min<INT>(End-Pos,BufferBase+BufferCount-Pos);
			appMemcpy( (BYTE*)V+Pos-Start, Buffer+Pos-BufferBase, Copy );
		}
	}
};
void DiskReady()
{
	while(sceCdDiskReady(0)!=SCECdComplete);
}
void RetryCD()
{
	debugf(TEXT("Retrying CD"));
	printf("Retrying CD");
	DiskReady();
}
sceCdRMode Mode={254,SCECdSpinNom,SCECdSecS2048,0};
struct FArchiveFileReaderCD : public FArchive
{
	BYTE*			Buffer;
	FOutputDevice*  Error;
	INT				Pos, BufferBase, BufferCount, BufferSize;
	sceCdlFILE		File;
	FArchiveFileReaderCD( const sceCdlFILE& InFile, FOutputDevice* InError, BYTE* InBuffer, INT InBufferSize )
	: File(InFile), Error(InError), Pos(0), BufferBase(0), BufferCount(0), Buffer(InBuffer), BufferSize(InBufferSize)
	{
		ArIsLoading = ArIsPersistent = 1;
	}
	~FArchiveFileReaderCD() {Close();}
	void Seek(INT InPos)    {Pos = InPos;}
	INT Tell()              {return Pos;}
	INT TotalSize()         {return File.size;}
	UBOOL Close()           {return !ArIsError;}
	void Serialize( void* V, INT Length )
	{
		for( INT Start=Pos,End=Pos+Length,Copy; Pos<End; Pos+=Copy )
		{
			if( Pos<BufferBase || Pos>=BufferBase+BufferCount )
			{
				BufferBase     = Pos&~(BufferSize-1);
				BufferCount    = Min<INT>(BufferSize,File.size-BufferBase);
				if( GUglyHackFlags&16 )
				{
					debugf(TEXT("Pausing CD music"));
					appPauseMusic();
				}
				DiskReady();
				while
				(	sceCdRead(File.lsn+BufferBase/2048,(BufferCount+2047)/2048,Buffer,&Mode)!=1
				||	sceCdSync(0)!=0
				||	sceCdGetError()!=SCECdErNO )
					RetryCD();
				if( GUglyHackFlags&16 )
				{
					debugf(TEXT("Unpausing CD music"));
					appResumeMusic();
				}
			}
			Copy = Min<INT>(End-Pos,BufferBase+BufferCount-Pos);
			appMemcpy( (BYTE*)V+Pos-Start, Buffer+Pos-BufferBase, Copy );
		}
	}
};
class FArchiveFileWriter : public FArchive
{
public:
	FArchiveFileWriter( INT InFile, FOutputDevice* InError )
	:	File		(InFile)
	,	Error		(InError)
	,	Pos			(0)
	,	BufferCount (0)
	{
		ArIsSaving = ArIsPersistent = 1;
	}
	~FArchiveFileWriter()
	{
		Close();
	}
	void Seek( INT InPos )
	{
		if( PSX2Host && Pos!=InPos )
		{
			FlushBuffer();
			sceLseek(File,InPos,SCE_SEEK_SET);
		}
		Pos = InPos;
	}
	INT Tell()
	{
		return Pos;
	}
	UBOOL Close()
	{
		if( File>=0 && PSX2Host )
		{
			FlushBuffer();
			sceClose(File);
		}
		File = -1;
		return 1;
	}
	void Serialize( void* V, INT Length )
	{
		while( Length>0 )
		{
			INT ToWrite=Min<INT>(Length,ARRAY_COUNT(Buffer)-BufferCount);
			appMemcpy(Buffer+BufferCount,V,ToWrite);
			BufferCount += ToWrite;
			if( BufferCount>=ARRAY_COUNT(Buffer) )
				FlushBuffer();
			Length = Length   - ToWrite;
			V      = (BYTE*)V + ToWrite;
		}
		Pos += Length;
	}
	void FlushBuffer()
	{
		if( BufferCount && PSX2Host )
			verify(sceWrite(File,Buffer,BufferCount)>=0);
		BufferCount=0;
	}
	BYTE			Buffer[8192];
	INT				File;
	FOutputDevice*	Error;
	INT				Pos, BufferCount;
};
class FFileManagerPSX2 : public FFileManagerGeneric
{
public:
	FArchive* CreateFileReader( const TCHAR* filename, DWORD flags, FOutputDevice* error )
	{
		TCHAR fullFilename[256];
		FArchive* Result=NULL;
		FindPSX2File(filename,fullFilename);
		if(PSX2Host)
		{
			INT file = sceOpen(fullFilename,SCE_RDONLY);
			if( file>=0 )
				Result=new(TEXT("PSX2FileReader"))FArchiveFileReaderHost(file,error);
		}
		else if(fullFilename[0])
		{
			sceCdlFILE file;
			DiskReady();
			if(sceCdSearchFile(&file,fullFilename+7))
			{
				UBOOL IsLinearFile = appStricmp(filename,TEXT("psx2lins.umd"))==0;
				INT   BufferSize   = IsLinearFile?65536:16384;
				BYTE* Buffer       = IsLinearFile?_memcache_start:new BYTE[BufferSize];
				debugf(TEXT("Opening CD file: %s (buffer=%iK)"),filename,BufferSize/1024);
				Result=new(TEXT("PSX2FileReader"))FArchiveFileReaderCD(file,error,Buffer,BufferSize);
			}
		}
		if( (!Result) && (flags&FILEREAD_NoFail) )
			appErrorf(TEXT("Failed to read file: %s (%s)"),filename,fullFilename);
		return Result;
	}
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		if( (Flags & FILEWRITE_NoReplaceExisting) && FileSize(Filename)>=0 )
			return NULL;
		TCHAR fullFilename[256];
		appStrcpy( fullFilename, "host:" );
		appStrcat( fullFilename, Filename );
		INT NewFlags = SCE_WRONLY | SCE_CREAT;
		if( Flags & FILEWRITE_Append )
			NewFlags |= SCE_APPEND;
		//if( Flags & FILEWRITE_Unbuffered )
		//	NewFlags |= SCE_NOBUF;
		INT File=-1;
		if( PSX2Host )
		{
			File = sceOpen(fullFilename,NewFlags);
			if( File<0 )
			{
				if( Flags & FILEWRITE_NoFail )
					appErrorf( TEXT("Failed to write: %s"), Filename );
				return NULL;
			}
		}
		return new(TEXT("PSX2FileWriter"))FArchiveFileWriter(File,Error);
	}
	UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
	{
		return true;
	}
    // gam ---
    UBOOL IsReadOnly( const TCHAR* Filename )
    {
		guard(FFileManagerPSX2::IsReadOnly);
		return 0;
		unguard;
    }
    // --- gam
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
		return 0;
	}
	FString GetDefaultDirectory()
	{
		FString blah;
		return blah;
	}
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar )
	{
		return 0;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

