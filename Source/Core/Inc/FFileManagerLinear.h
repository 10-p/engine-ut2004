/*=============================================================================
	FFileManagerLinear.h
	Copyright 1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

UBOOL DebugLinear=0 && DO_CHECK;
#define LINEAR_DATA  0x43
#define LINEAR_SEEK  0x67
#define LINEAR_SIZE  0xEC
#define LINEAR_CLOSE 0xB3

void HandleLinearURL(const TCHAR* S)
{
	// Update global command line temporarily.
	TCHAR Temp[4096];
	appStrcpy(Temp,S);
	//debugf(TEXT("$$$$$ Old Cmd Line [%s]"),GCmdLine);
	//debugf(TEXT("$$$$$ New Linear URL [%s]"),Temp);
	if( appStrchr(appCmdLine(),TEXT(' ')) )
		appStrcat(Temp,appStrchr(GCmdLine,TEXT(' ')));
	appStrcpy(GCmdLine,Temp);
	//debugf(TEXT("$$$$$ New Cmd Line [%s]"),GCmdLine);
}

struct FArchiveFileReaderSaveLinear : public FArchive
{
	FArchive& Ar;
	FArchive& Linear;
	UBOOL LinearFileStillOpen;
	FArchiveFileReaderSaveLinear(FArchive& InAr, FArchive& InLinear)
	: Ar(InAr), Linear(InLinear), LinearFileStillOpen(1)
	{
		ArIsLoading = ArIsPersistent = 1;
		if(DebugLinear) {BYTE B=LINEAR_SIZE; Linear<<B;}
		INT     T=Ar.TotalSize();
		FString S=GLinearURL;
		Linear<<T<<S;
		HandleLinearURL(*S);
	}
	UBOOL Close()
	{
		BYTE B=LINEAR_CLOSE; Linear<<B;
		LinearFileStillOpen = 0;
		return Ar.Close() && Linear.Close();
	}
	INT Tell()
	{
		return Ar.Tell();
	}
	INT TotalSize()
	{
		return Ar.TotalSize();
	}
	void Seek(INT InPos)
	{
		Ar.Seek(InPos);
		if(DebugLinear) {BYTE B=LINEAR_SEEK; Linear<<B<<InPos;}
	}
	void Serialize( void* V, INT Length )
	{
		check(LinearFileStillOpen );
		if(DebugLinear) {BYTE B=LINEAR_DATA; Linear<<B<<Length;}
		Ar.Serialize(V,Length);
		Linear.Serialize(V,Length);
	}
};


struct FArchiveFileReaderLoadLinear : public FArchive
{
	FArchive& Linear;
	INT FakeSize;
	INT FakePos;
	INT LinearFileStillOpen;
	static FArchiveFileReaderLoadLinear* Global;
	FArchiveFileReaderLoadLinear( FArchive& InLinear )
	: Linear(InLinear)
	{
		ArIsLoading = ArIsPersistent = 1;
		LinearFileStillOpen = 1;
		FakePos = 0;
		if(DebugLinear) {verify(Arctor<BYTE>(Linear)==LINEAR_SIZE);}
		Linear << FakeSize;
		HandleLinearURL(*Arctor<FString>(Linear));
		Global=this;
	}
	~FArchiveFileReaderLoadLinear()
	{
		Close();
	}
	UBOOL Close()
	{
		check(LinearFileStillOpen);
		verify(Arctor<BYTE>(Linear)==LINEAR_CLOSE);
		LinearFileStillOpen=0;
		return Linear.Close();
	}
	INT Tell()
	{
		return FakePos;
	}
	INT TotalSize()
	{
		return FakeSize;
	}
	void Seek(INT InPos)
	{
		if(DebugLinear)
		{
			verify(Arctor<BYTE>(Linear)==LINEAR_SEEK);
			//verify(Arctor<INT>(Linear)==InPos);
			INT q;if((q=Arctor<INT>(Linear))!=InPos)debugf(TEXT("%i %i"),q,InPos);
			verify(q==InPos);
		}
		FakePos=InPos;
	}
	void Serialize( void* V, INT Length )
	{
		if( !LinearFileStillOpen )
			check(LinearFileStillOpen);
		if(DebugLinear) {verify(Arctor<BYTE>(Linear)==LINEAR_DATA); verify(Arctor<INT>(Linear)==Length);}
		Linear.Serialize(V,Length);
		GLinearBytesSerialized += Length;
		FakePos+=Length;
		check(!Linear.IsError());
	}
};
FArchiveFileReaderLoadLinear* FArchiveFileReaderLoadLinear::Global;


class FFileManagerLinear : public FFileManagerGeneric
{
public:
	FFileManager* FM;
	TCHAR         LinearFile[4096];
	TCHAR         Src[4096];
	FFileManagerLinear(FFileManager* InFM,const TCHAR* InLinearFile)
	{
		FM=InFM;
		appStrcpy(LinearFile,InLinearFile);
		appStrcpy(Src,TEXT(""));
	}
	void Init(UBOOL Startup)
	{
		FM->Init(Startup);
	}
	FArchive* CreateFileReader( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		UBOOL IsLinearFile = appStricmp(Filename,LinearFile)==0;
//		debugf(TEXT("$linear1 [%s] %i %i"),Filename,IsLinearFile,GLinearLoad);
		if( IsLinearFile && GLinearLoad && *Src )
		{
			Filename=Src;
//			debugf(TEXT("$linear2 %s"),Src);
		}
		FArchive* Result=FM->CreateFileReader(Filename, Flags, Error);
//		debugf(TEXT("$linear3 [%s]: %i"),Filename,Result);
		if( Result && IsLinearFile )
		{
			if( GLinearSave )
			{
				FArchive* Linear = FM->CreateFileWriter(Src,FILEWRITE_NoFail,Error);
				Result           = new(TEXT("ReaderSaveLinear"))FArchiveFileReaderSaveLinear(*Result,*Linear);
				debugf(TEXT("Saving to linear image [%s] using [%s]"),Src,Filename);
			}
			else if( GLinearLoad )
			{
				Result = new(TEXT("ReaderLoadLinear"))FArchiveFileReaderLoadLinear(*Result);
				debugf(TEXT("Loading from linear image [%s]"),Filename);
			}
		}
		return Result;
	}
	FArchive* CreateFileWriter( const TCHAR* Filename, DWORD Flags, FOutputDevice* Error )
	{
		return FM->CreateFileWriter(Filename,Flags,Error);
	}
	UBOOL Delete( const TCHAR* Filename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
	{
		return FM->Delete(Filename,RequireExists,EvenReadOnly);
	}
    // gam ---
    UBOOL IsReadOnly( const TCHAR* Filename )
    {
		return FM->IsReadOnly(Filename);
    }
    // --- gam
	INT FileSize( const TCHAR* Filename )
	{
		return FM->FileSize(Filename);
	}
	SQWORD GetGlobalTime( const TCHAR* Filename )
	{
		return FM->GetGlobalTime(Filename);
	}
	UBOOL SetGlobalTime( const TCHAR* Filename )
	{
		return FM->SetGlobalTime(Filename);
	}
	UBOOL MakeDirectory( const TCHAR* Path, UBOOL Tree=0 )
	{
		return FM->MakeDirectory(Path,Tree);
	}
	UBOOL DeleteDirectory( const TCHAR* Path, UBOOL RequireExists=0, UBOOL Tree=0 )
	{
		return FM->DeleteDirectory(Path,RequireExists,Tree);
	}
	TArray<FString> FindFiles( const TCHAR* Filename, UBOOL Files, UBOOL Directories )
	{
		return FM->FindFiles(Filename,Files,Directories);
	}
	UBOOL SetDefaultDirectory( const TCHAR* Filename )
	{
		return FM->SetDefaultDirectory(Filename);
	}
	FString GetDefaultDirectory()
	{
		return FM->GetDefaultDirectory();
	}
	// gam ---
	INT CompareFileTimes( const TCHAR* FileA, const TCHAR* FileB )
	{
		return FM->CompareFileTimes( FileA, FileB );
	}
	// --- gam
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar )
	{
		return 0;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

