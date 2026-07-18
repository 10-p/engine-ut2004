/*=============================================================================
	FFileManagerXbox.h: Unreal X-Box based file manager.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "FFileManagerGeneric.h"
#include "../../../Thirdparty/zlib/zlib.h"

#include <sys/types.h>
#include <sys/stat.h>

#define COPY_BUFFER_SIZE		65536

/*-----------------------------------------------------------------------------
	File Manager.
-----------------------------------------------------------------------------*/

// File manager.
class FArchiveFileReaderCompressed : public FArchive
{
public:
	FArchiveFileReaderCompressed( HANDLE InHandle, FOutputDevice* InError, INT InSize )
	:	Handle			( InHandle )
	,	Error			( InError )
	,	Size			( InSize )
	,	Pos				( 0 )
	,	BufferBase		( 0 )
	,	BufferCount		( 0 )
	{
		guard(FArchiveFileReaderCompressed::FArchiveFileReaderCompressed);
		ArIsLoading = ArIsPersistent = 1;
		BufferSize = ARRAY_COUNT(Buffer);
		unguard;
	}
	~FArchiveFileReaderCompressed()
	{
		guard(FArchiveFileReaderCompressed::~FArchiveFileReaderCompressed);
		if( Handle )
			Close();
		unguard;
	}
	void Seek( INT InPos )
	{
		guard(FArchiveFileReaderCompressed::Seek);
		appErrorf(TEXT("Can't seek compressed file"));
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
		guardSlow(FArchiveFileReaderCompressed::Close);
		if( Handle )
			CloseHandle( Handle );
		Handle = NULL;
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		Pos += Length;
		INT Offset = 0;
		while( Length )
		{
			if( Length > BufferCount )
			{
				if( BufferCount )
				{
					appMemcpy( (BYTE*) V + Offset, Buffer+BufferBase, BufferCount );
					Length		-= BufferCount;
					Offset		+= BufferCount;
				}

				DWORD Count			= 0;
				DWORD Data[2];
				DWORD& Uncompressed	= Data[0];
				DWORD& Compressed	= Data[1];
				
				ReadFile( Handle, Data, 8, &Count, NULL );
				ReadFile( Handle, &Buffer[16384], Compressed, &Count, NULL );				

				if( uncompress( Buffer, &Uncompressed, &Buffer[16384], Compressed ) != Z_OK )
					appErrorf(TEXT("zlib: uncompress failed"));

				BufferCount = Uncompressed;
				BufferBase	= 0;
			}
			else
			{
				appMemcpy( (BYTE*) V + Offset, Buffer+BufferBase, Length );
				BufferCount -= Length;
				BufferBase  += Length;
				Length		= 0;
			}
		}
	}
protected:
	HANDLE			Handle;
	FOutputDevice*	Error;
	INT				Size;
	INT				Pos;
	INT				BufferBase;
	INT				BufferCount;
	INT				BufferSize;
	BYTE			Buffer[32768];
};



// File manager.
class FArchiveFileReader : public FArchive
{
public:
	FArchiveFileReader( HANDLE InHandle, FOutputDevice* InError, INT InSize, UBOOL InIsLinear )
	:	Handle			( InHandle )
	,	Error			( InError )
	,	Size			( InSize )
	,	Pos				( 0 )
	,	BufferBase		( 0 )
	,	BufferCount		( 0 )
	,	IsLinear		( InIsLinear )
	{
		guard(FArchiveFileReader::FArchiveFileReader);
		ArIsLoading = ArIsPersistent = 1;
		if( IsLinear )
		{
			BufferSize = 16384;
			Buffer = new BYTE[BufferSize];			
		}
		else
		{
			BufferSize = ARRAY_COUNT(RegularBuffer);
			Buffer = RegularBuffer;
		}

		unguard;
	}
	~FArchiveFileReader()
	{
		guard(FArchiveFileReader::~FArchiveFileReader);
		if( Handle )
			Close();
		if( BufferSize > 2048 )
			delete [] Buffer;
		Buffer = NULL;
		unguard;
	}
	void Precache( INT HintCount )
	{
		guardSlow(FArchiveFileReader::Precache);
		checkSlow(Pos==BufferBase+BufferCount);
		BufferBase	= Pos;
		BufferCount = Min( Min( HintCount, (INT)(BufferSize - (Pos&(BufferSize-1))) ), Size-Pos );
		INT Count=0;
		ReadFile( Handle, Buffer, BufferCount, (DWORD*)&Count, NULL );
		if( Count!=BufferCount )
		{
			ArIsError = 1;
			Error->Logf( TEXT("ReadFile failed: Count=%i BufferCount=%i Error=%s"), Count, BufferCount, appGetSystemErrorMessage() );
		}
		unguardSlow;
	}
	void Seek( INT InPos )
	{
		guard(FArchiveFileReader::Seek);
		check(InPos>=0);
		check(InPos<=Size);
		if( SetFilePointer( Handle, InPos, 0, FILE_BEGIN )==0xFFFFFFFF )
		{
			ArIsError = 1;
			Error->Logf( TEXT("SetFilePointer Failed %i/%i: %i %s"), InPos, Size, Pos, appGetSystemErrorMessage() );
		}
		Pos         = InPos;
		BufferBase  = Pos;
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
		if( Handle )
			CloseHandle( Handle );
		Handle = NULL;
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		guardSlow(FArchiveFileReader::Serialize);
		while( Length>0 )
		{
			INT Copy = Min( Length, BufferBase+BufferCount-Pos );
			if( Copy==0 )
			{
				if( Length >= BufferSize )
				{
					INT Count=0;
					ReadFile( Handle, V, Length, (DWORD*)&Count, NULL );
					if( Count!=Length )
					{
						ArIsError = 1;
						Error->Logf( TEXT("ReadFile failed: Count=%i Length=%i Error=%s"), Count, Length, appGetSystemErrorMessage() );
					}
					Pos += Length;
					BufferBase += Length;
					return;
				}
				Precache( MAXINT );
				Copy = Min( Length, BufferBase+BufferCount-Pos );
				if( Copy<=0 )
				{
					ArIsError = 1;
					Error->Logf( TEXT("ReadFile beyond EOF %i+%i/%i"), Pos, Length, Size );
				}
				if( ArIsError )
					return;
			}
			appMemcpy( V, Buffer+Pos-BufferBase, Copy );
			Pos       += Copy;
			Length    -= Copy;
			V          = (BYTE*)V + Copy;
		}
		unguardSlow;
	}
protected:
	HANDLE			Handle;
	FOutputDevice*	Error;
	INT				Size;
	INT				Pos;
	INT				BufferBase;
	INT				BufferCount;
	INT				BufferSize;
	BYTE*			Buffer;
	UBOOL			IsLinear;
	BYTE			RegularBuffer[2048];
};


class FArchiveFileWriter : public FArchive
{
public:
	FArchiveFileWriter( HANDLE InHandle, FOutputDevice* InError, INT InPos, UBOOL InIsLinear )
	:	Handle		( InHandle )
	,	Error		( InError )
	,	Pos			( InPos )
	,	BufferCount	( 0 )
	,	IsLinear	( InIsLinear )
	{
		ArIsSaving = ArIsPersistent = 1;
	}
	~FArchiveFileWriter()
	{
		guard(FArchiveFileWriter::~FArchiveFileWriter);
		if( Handle )
			Close();
		Handle = NULL;
		unguard;
	}
	void Seek( INT InPos )
	{
		Flush();
		if( SetFilePointer( Handle, InPos, 0, FILE_BEGIN )==0xFFFFFFFF )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("SeekFailed",TEXT("Core")) );
		}
		Pos = InPos;
	}
	INT Tell()
	{
		return Pos;
	}
	UBOOL Close()
	{
		guardSlow(FArchiveFileWriter::Close);
		Flush();
		if( Handle && !CloseHandle(Handle) )
		{
			ArIsError = 1;
			Error->Logf( LocalizeError("WriteFailed",TEXT("Core")) );
		}
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		Pos += Length;
		INT Copy;
		while( Length > (Copy=ARRAY_COUNT(Buffer)-BufferCount) )
		{
			appMemcpy( Buffer+BufferCount, V, Copy );
			BufferCount += Copy;
			Length      -= Copy;
			V            = (BYTE*)V + Copy;
			Flush();
		}
		if( Length )
		{
			appMemcpy( Buffer+BufferCount, V, Length );
			BufferCount += Length;
		}
	}
	void Flush()
	{
		if( BufferCount )
		{
			INT Result=0;
			
			if( IsLinear )
			{
				DWORD CompressedSize = 20000;
				if ( compress( &Compressed[8], &CompressedSize, Buffer, BufferCount ) != Z_OK )
					appErrorf(TEXT("zlib: compress failed"));

				*((DWORD*)&Compressed[0]) = BufferCount;
				*((DWORD*)&Compressed[4]) = CompressedSize;

				if( !WriteFile( Handle, Compressed, CompressedSize + 8, (DWORD*)&Result, NULL ) )
				{
					ArIsError = 1;
					Error->Logf( LocalizeError("WriteFailed",TEXT("Core")) );
				}
			}
			else if( !WriteFile( Handle, Buffer, BufferCount, (DWORD*)&Result, NULL ) )
			{
				ArIsError = 1;
				Error->Logf( LocalizeError("WriteFailed",TEXT("Core")) );
			}
		}
		BufferCount = 0;
	}
protected:
	UBOOL			IsLinear;
	HANDLE			Handle;
	FOutputDevice*	Error;
	INT				Pos;
	INT				BufferCount;
	BYTE			Buffer[16384];
	BYTE			Compressed[20000];
};


class FFileManagerXbox : public FFileManagerGeneric
{
public:

	TCHAR	CurrentDirectory[1024];

	FFileManagerXbox()
	{
		appStrcpy(CurrentDirectory,TEXT("D:\\"));
		// Init scratch area.
		CreateDirectory(TCHAR_TO_ANSI(TEXT("Z:\\Cache"				)),NULL);
		CreateDirectory(TCHAR_TO_ANSI(TEXT("Z:\\Help"				)),NULL);
		CreateDirectory(TCHAR_TO_ANSI(TEXT("Z:\\Maps"				)),NULL);
		CreateDirectory(TCHAR_TO_ANSI(TEXT("Z:\\Music"				)),NULL);
		CreateDirectory(TCHAR_TO_ANSI(TEXT("Z:\\Prefabs"			)),NULL);
		CreateDirectory(TCHAR_TO_ANSI(TEXT("Z:\\Prefabs\\General"	)),NULL);
		CreateDirectory(TCHAR_TO_ANSI(TEXT("Z:\\Sounds"				)),NULL);
		CreateDirectory(TCHAR_TO_ANSI(TEXT("Z:\\StaticMeshes"		)),NULL);
		CreateDirectory(TCHAR_TO_ANSI(TEXT("Z:\\Streams"			)),NULL);
		CreateDirectory(TCHAR_TO_ANSI(TEXT("Z:\\System"				)),NULL);
		CreateDirectory(TCHAR_TO_ANSI(TEXT("Z:\\Textures"			)),NULL);
		CreateDirectory(TCHAR_TO_ANSI(TEXT("Z:\\Animations"			)),NULL);
	}
#if USE_Z_DRIVE
	FString GetFilename(const TCHAR* InFilename, const UBOOL Enforce = false, const TCHAR* InDrive = TEXT("Z:\\"))
#else
	FString GetFilename(const TCHAR* InFilename, const UBOOL Enforce = false, const TCHAR* InDrive = TEXT("D:\\"))
#endif
	{
		FString	Filename = InFilename;
		if((*Filename)[1] == TEXT(':'))
		{
			if( !Enforce )
            {
                // amb ---
                if (!appStrncmp(CurrentDirectory, InFilename, 3))
                {
                    return Filename;
                }
                // --- amb
				return Filename;
            }
			else
				return FString(InDrive) + Filename.Mid(3);
		}
		else if((*Filename)[0] == TEXT('.') && !((*Filename)[1] == TEXT('.')))
		{
			appErrorf(TEXT("Illegal filename (%s)"), *Filename);
			return Filename;
		}
		else if((*Filename)[0] == TEXT('.') && (*Filename)[1] == TEXT('.'))
			return FString(InDrive) + Filename.Mid(3);
		else
			return FString(InDrive) + TEXT("System\\") + Filename;
	}

	FArchive* CreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		guard(FFileManagerXbox::CreateFileReader);
		DWORD  Access    = GENERIC_READ;
		DWORD  WinFlags  = FILE_SHARE_READ;
		DWORD  Create    = OPEN_EXISTING;

		//if( appStricmp( Filename, TEXT("save.lin")) == 0)
		//	Copy( TEXT("Z:\\System\\save.lin"), TEXT("D:\\System\\save.lin"), 0, 0, 0, NULL );

		HANDLE Handle = CreateFile( TCHAR_TO_ANSI(*GetFilename(Filename)), Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL );
		if( Handle==INVALID_HANDLE_VALUE )
			Handle    = CreateFile( TCHAR_TO_ANSI(*GetFilename(Filename,1,TEXT("D:\\"))), Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL );
		if( Handle==INVALID_HANDLE_VALUE )
		{
			debugf( TEXT("Failed to read file: %s"), *GetFilename(Filename) );
			if( Flags & FILEREAD_NoFail )
				appErrorf( TEXT("Failed to read file: %s"), *GetFilename(Filename) );
			return NULL;
		}
		UBOOL IsLinearFile = appStrstr(Filename,TEXT(".lin")) != NULL;
		if( IsLinearFile )
		{
			return new(TEXT("CompressedFileReader"))FArchiveFileReaderCompressed(Handle,Error,GetFileSize(Handle,NULL));
		}
		else
			return new(TEXT("WindowsFileReader"))FArchiveFileReader(Handle,Error,GetFileSize(Handle,NULL),IsLinearFile);
		unguard;
	}
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		guard(FFileManagerXbox::CreateFileWriter);
		if( Flags & FILEWRITE_EvenIfReadOnly )
			SetFileAttributes(TCHAR_TO_ANSI(*GetFilename(Filename)), 0);
		DWORD  Access    = GENERIC_WRITE;
		DWORD  WinFlags  = (Flags & FILEWRITE_AllowRead) ? FILE_SHARE_READ : 0;
		DWORD  Create    = (Flags & FILEWRITE_Append) ? OPEN_ALWAYS : (Flags & FILEWRITE_NoReplaceExisting) ? CREATE_NEW : CREATE_ALWAYS;
		HANDLE Handle    = CreateFile( TCHAR_TO_ANSI(*GetFilename(Filename)), Access, WinFlags, NULL, Create, FILE_ATTRIBUTE_NORMAL, NULL );
		INT    Pos       = 0;
		if( Handle==INVALID_HANDLE_VALUE )
		{
			if( Flags & FILEWRITE_NoFail )
				appErrorf( TEXT("Failed to create file: %s"), *GetFilename(Filename) );
			return NULL;
		}
		if( Flags & FILEWRITE_Append )
			Pos = SetFilePointer( Handle, 0, 0, FILE_END );
		UBOOL IsLinearFile = appStrstr(Filename,TEXT(".lin")) != NULL;
		return new(TEXT("WindowsFileWriter"))FArchiveFileWriter(Handle,Error,Pos,IsLinearFile);
		unguard;
	}
	INT FileSize( const TCHAR* Filename )
	{
		guard(FFileManagerXbox::FileSize);
		HANDLE Handle = CreateFile( TCHAR_TO_ANSI(*GetFilename(Filename)), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
		if( Handle==INVALID_HANDLE_VALUE )
			Handle = CreateFile( TCHAR_TO_ANSI(*GetFilename(Filename,1,TEXT("D:\\"))), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
		if( Handle==INVALID_HANDLE_VALUE )
			return -1;
		DWORD Result = GetFileSize( Handle, NULL );
		CloseHandle( Handle );
		return Result;
		unguard;
	}
	DWORD Copy( const TCHAR* DestFile, const TCHAR* SrcFile, UBOOL ReplaceExisting, UBOOL EvenIfReadOnly, UBOOL Attributes, DWORD Compress, FCopyProgress* Progress )
	{
		guard(FFileManagerXbox::Copy);
		if( EvenIfReadOnly )
			SetFileAttributes(TCHAR_TO_ANSI(*GetFilename(DestFile,1)), 0);
		debugf(TEXT("Copying [%s] to [%s]"), SrcFile, DestFile);
		UBOOL Success = true;
		if( Progress )
			Progress->Poll( 0.0 );
		HANDLE HandleSrc = CreateFile( TCHAR_TO_ANSI(*GetFilename(SrcFile)), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL );
		if( HandleSrc==INVALID_HANDLE_VALUE )
			HandleSrc = CreateFile( TCHAR_TO_ANSI(*GetFilename(SrcFile,1,TEXT("D:\\"))), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL );
		if( HandleSrc==INVALID_HANDLE_VALUE )
			Success = false;
		HANDLE HandleDst = INVALID_HANDLE_VALUE;
		if( Success )
		{
			HandleDst= CreateFile( TCHAR_TO_ANSI(*GetFilename(DestFile,1)), GENERIC_WRITE, 0, NULL, ReplaceExisting ? CREATE_ALWAYS : CREATE_NEW, FILE_FLAG_SEQUENTIAL_SCAN, NULL );
			if( HandleDst==INVALID_HANDLE_VALUE )
				Success = false;
		}
		if( Success )
		{
			BYTE* Buffer = new BYTE[COPY_BUFFER_SIZE];
			DWORD BytesRead, BytesWritten;
			do
			{
				ReadFile( HandleSrc, Buffer, COPY_BUFFER_SIZE, &BytesRead, NULL );
				WriteFile( HandleDst, Buffer, BytesRead, &BytesWritten, 0 );
			} while( BytesRead );
			delete [] Buffer;
		}
		if( HandleSrc!=INVALID_HANDLE_VALUE )
			CloseHandle( HandleSrc );
		if( HandleDst!=INVALID_HANDLE_VALUE )
			CloseHandle( HandleDst );
		if( Progress )
			Progress->Poll( 1.0 );
		if( Success && !Attributes )
			SetFileAttributes(TCHAR_TO_ANSI(*GetFilename(DestFile,1)), 0);
		return Success;
		unguard;
	}
	UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
	{
		guard(FFileManagerXbox::Delete);
		if( EvenReadOnly )
			SetFileAttributesA(TCHAR_TO_ANSI(*GetFilename(Filename)),FILE_ATTRIBUTE_NORMAL);
		return DeleteFile(TCHAR_TO_ANSI(*GetFilename(Filename)))!=0 || (!RequireExists && GetLastError()==ERROR_FILE_NOT_FOUND);
		unguard;
	}
	UBOOL Move( const TCHAR* Dest, const TCHAR* Src, UBOOL Replace=1, UBOOL EvenIfReadOnly=0, UBOOL Attributes=0 )
	{
		guard(FFileManagerXbox::Move);
		//warning: MoveFileEx is broken on Windows 95 (Microsoft bug).
		Delete( Dest, 0, EvenIfReadOnly );
		INT Result = MoveFile(TCHAR_TO_ANSI(*GetFilename(Src)),TCHAR_TO_ANSI(*GetFilename(Dest)));
		if( !Result )
			Result = MoveFile(TCHAR_TO_ANSI(*GetFilename(Src,1,TEXT("D:\\"))),TCHAR_TO_ANSI(*GetFilename(Dest)));
		if( !Result )
			debugf( NAME_Warning, TEXT("Error moving file '%s' to '%s'"), *GetFilename(Src), *GetFilename(Dest) );
		return Result!=0;
		unguard;
	}
	SQWORD GetGlobalTime( const TCHAR* Filename )
	{
		return 0;
	}
	UBOOL SetGlobalTime( const TCHAR* Filename )
	{
		return 0;//!!
	}
	UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )
	{
		guard(FFileManagerXbox::MakeDirectory);
		if( Tree )
			return FFileManagerGeneric::MakeDirectory( Path, Tree );
		return CreateDirectory(TCHAR_TO_ANSI(*GetFilename(Path)),NULL)!=0 || GetLastError()==ERROR_ALREADY_EXISTS;
		unguard;
	}
	UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )
	{
		guard(FFileManagerXbox::DeleteDirectory);
		if( Tree )
			return FFileManagerGeneric::DeleteDirectory( Path, RequireExists, Tree );
		return RemoveDirectory(TCHAR_TO_ANSI(*GetFilename(Path)))!=0 || (!RequireExists && GetLastError()==ERROR_FILE_NOT_FOUND);
		unguard;
	}
	TArray<FString> FindFiles( const TCHAR* Filename, UBOOL Files, UBOOL Directories )
	{
		guard(FFileManagerXbox::FindFiles);
		TArray<FString> Result;
		HANDLE Handle=NULL;
		WIN32_FIND_DATAA Data;
		// Search in scratch area
		Handle=FindFirstFileA(TCHAR_TO_ANSI(*GetFilename(Filename)),&Data);
		if( Handle!=INVALID_HANDLE_VALUE )
		{
			do
			{
			    // gam ---
                TCHAR Name[MAX_PATH];

                winToUNICODE( Name, Data.cFileName, Min<INT>( ARRAY_COUNT(Name), winGetSizeUNICODE(Data.cFileName) ) );
			
				if
				(	appStricmp(Name,TEXT("."))
				&&	appStricmp(Name,TEXT(".."))
				&&	((Data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)?Directories:Files) )
					new(Result)FString(Name);
				// --- gam
			}
			while( FindNextFileA(Handle,&Data) );

			if( Handle )
			FindClose( Handle );
		}
#if 0 // sjs - todo WHY??? this returns duplicates.
		//!!vogel: TODO - Z vs D
		Handle=FindFirstFileA(TCHAR_TO_ANSI(*GetFilename(Filename,1,TEXT("D:\\"))),&Data);
		if( Handle!=INVALID_HANDLE_VALUE )
		{
			do
			{
			    // gam ---
                TCHAR Name[MAX_PATH];

                winToUNICODE( Name, Data.cFileName, Min<INT>( ARRAY_COUNT(Name), winGetSizeUNICODE(Data.cFileName) ) );
			
				if
				(	appStricmp(Name,TEXT("."))
				&&	appStricmp(Name,TEXT(".."))
				&&	((Data.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY)?Directories:Files) )
					new(Result)FString(Name);
				// --- gam
			}
			while( FindNextFileA(Handle,&Data) );

			if( Handle )
			FindClose( Handle );
		}
#endif
		return Result;
		unguard;
    }
	UBOOL SetDefaultDirectory( const TCHAR* Filename )
	{
		guard(FFileManagerXbox::SetDefaultDirectory);
		appStrcpy(CurrentDirectory,Filename);
		return 1;
		unguard;
	}
	FString GetDefaultDirectory()
	{
		guard(FFileManagerXbox::GetDefaultDirectory);
		return FString(CurrentDirectory);
		unguard;
	}
	// gam ---
	INT CompareFileTimes( const TCHAR* FileA, const TCHAR* FileB )
	{
		guard(FFileManagerXbox::CompareFileTimes);
	
		struct _stat StatA, StatB;

		if( _stat( appToAnsi( FileA ), &StatA ) != 0 )
		    return 0;

		if( _stat( appToAnsi( FileB ), &StatB ) != 0 )
		    return 0;
		    
        return( INT(StatB.st_mtime - StatA.st_mtime) );

        unguard;
	}
	// --- gam
    // sjs ---
    UBOOL IsReadOnly( const TCHAR* Filename )
    {
		guard(FFileManagerXbox::IsReadOnly);

        DWORD rc;

        if( FileSize( Filename ) < 0 )
            return( 0 );

		rc = TCHAR_CALL_OS(GetFileAttributes(appToAnsi(Filename)),GetFileAttributesA((LPCSTR)TCHAR_TO_ANSI(Filename)));

        if (rc != 0xFFFFFFFF)
            return ((rc & FILE_ATTRIBUTE_READONLY) != 0);
        else
        {
			debugf( NAME_Warning, TEXT("Error reading attributes for '%s'"), Filename );
            return (0);
        }

		unguard;
    }
    // --- sjs
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

