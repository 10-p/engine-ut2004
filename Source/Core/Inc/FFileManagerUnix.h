/*=============================================================================
	FFileManagerUnix.h: Unreal Unix based file manager.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Brandon Reinhart
		* Major changes by Daniel Vogel
=============================================================================*/

#ifndef _INCL_FFILEMANAGERUNIX_H_
#define _INCL_FFILEMANAGERUNIX_H_

#include <dirent.h>
#include <unistd.h>
#include <limits.h>
#include "FFileManagerGeneric.h"
// ufront 2.33: Windows' mkdir takes no mode — permissions are not a filesystem attribute there.
#ifdef _WIN32
#  define UT_MKDIR(p) mkdir(p)
#else
#  define UT_MKDIR(p) mkdir((p), S_IRUSR | S_IWUSR | S_IXUSR)
#endif

// ufront 2.33 ★ THE ut2004 WINDOWS STARTUP CRASH. MinGW's open() defaults to TEXT mode, which translates
// CRLF and stops at Ctrl-Z — so read() returns FEWER bytes than asked for, every archive read "fails",
// and the engine died reading its own ini before it could log a word. The fopen() calls in this file
// already say "rb"; these open() calls said nothing. O_BINARY is 0 on POSIX (no text/binary distinction),
// so OR-ing it in is a no-op for Linux and the web.
#ifndef O_BINARY
#  define O_BINARY 0
#endif

#ifndef _WIN32
#include <pwd.h>	// ufront 2.33: no passwd database on Windows (see the home-dir lookup below)
#endif
#include <stdlib.h>
#include <ctype.h>

#include "UnGnuG.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define UNREAL_USE_MMAP 0

#if UNREAL_USE_MMAP
#include <sys/mman.h>
#endif

/*-----------------------------------------------------------------------------
	File Manager.
-----------------------------------------------------------------------------*/

#if UNREAL_USE_MMAP
class FArchiveFileReaderMMAP : public FArchive
{
public:
	FArchiveFileReaderMMAP( int InFile, FOutputDevice* InError, INT InSize )
	:	File			( InFile )
	,	Error			( InError )
	,	Size			( InSize )
	,	MmapSize		( InSize )
	,	Pos				( 0 )
    ,   Buffer          ( NULL )
	{
		guard(FArchiveFileReaderMMAP::FArchiveFileReaderMMAP);
		ArIsLoading = ArIsPersistent = 1;

		// Align mmap() calls to page boundaries...
		int PageSize = getpagesize();
		if ((Size % PageSize) != 0)
			MmapSize = Size + (PageSize - (Size % PageSize));
		unguard;
	}

    bool DoMapping(void)
    {
		guard(FArchiveFileReaderMMAP::DoMapping);
        if ( ( Buffer == NULL ) && ( File >= 0 ) )
        {
            Buffer = (BYTE *) mmap(NULL, MmapSize, PROT_READ, MAP_SHARED | MAP_FILE, File, 0);
            if (Buffer == (void *) MAP_FAILED)
                Buffer = NULL;
            else
                close(File);  // don't need file handle anymore.
            //printf("mmap()ing %d bytes was a %s\n", (int) Size, Buffer != NULL ? "SUCCESS" : "FAILURE");
            File = -1;  // don't try to close handle either way.
        }

        return (Buffer != NULL);
		unguard;
    }

	~FArchiveFileReaderMMAP()
	{
		guard(FArchiveFileReaderMMAP::~FArchiveFileReaderMMAP);
		Close();
		unguard;
	}

	void Precache( INT HintCount )
	{
	}

	void Seek( INT InPos )
	{
		guard(FArchiveFileReaderMMAP::Seek);
		check(InPos>=0);
		check(InPos<=Size);
		Pos         = InPos;
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
		guardSlow(FArchiveFileReaderMMAP::Close);
        if( Buffer != NULL )
        {
            munmap(Buffer, MmapSize);
            Buffer = NULL;
            //printf("munmap()ed %d bytes\n", (int) Size);
        }
		if( File >= 0 )
        {
			close( File );
		    File = -1;
        }
		return !ArIsError;
		unguardSlow;
	}
	void Serialize( void* V, INT Length )
	{
		guardSlow(FArchiveFileReaderMMAP::Serialize);

		if (Length > (this->Size - this->Pos))
			if( Error ) Error->Logf( TEXT("ReadFile beyond EOF %i+%i/%i"), Pos, Length, Size );
        
        memcpy(V, Buffer + Pos, Length);
        Pos += Length;
		unguardSlow;
	}
protected:
	int				File;
	FOutputDevice*	Error;
	INT				Size;
	INT				MmapSize;
	INT				Pos;
    BYTE			*Buffer;
};
#endif


class FArchiveFileReader : public FArchive
{
public:
	FArchiveFileReader( int InFile, FOutputDevice* InError, INT InSize )
	:	File			( InFile )
	,	Error			( InError )
	,	Size			( InSize )
	,	Pos				( 0 )
	,	BufferBase		( 0 )
	,	BufferCount		( 0 )
	{
		guard(FArchiveFileReader::FArchiveFileReader);
		ArIsLoading = ArIsPersistent = 1;
		unguard;
	}
	~FArchiveFileReader()
	{
		guard(FArchiveFileReader::~FArchiveFileReader);
		Close();
		unguard;
	}
	void Precache( INT HintCount )
	{
		guardSlow(FArchiveFileReader::Precache);
		checkSlow(Pos==BufferBase+BufferCount);
		BufferBase = Pos;
		BufferCount = Min( Min( HintCount, (INT)(ARRAY_COUNT(Buffer) - (Pos&(ARRAY_COUNT(Buffer)-1))) ), Size-Pos );
		DOUBLE	StartTime	= appSeconds();
		if( read( File, Buffer, BufferCount ) != BufferCount )
		{
			ArIsError = 1;
			if( Error ) Error->Logf( TEXT("read failed: BufferCount=%i errno=%d"), BufferCount, (int) errno );
			return;
		}
		FLOAT	DeltaTime	= (FLOAT) (appSeconds() - StartTime);
		if( DeltaTime > 100.f ) DeltaTime = 0.0f; //!!
		GFileManagerBytesRead+=BufferCount; GFileManagerLoadingTime+=DeltaTime;
		unguardSlow;
	}
	void Seek( INT InPos )
	{
		guard(FArchiveFileReader::Seek);
		check(InPos>=0);
		check(InPos<=Size);
		if( lseek(File,InPos,SEEK_SET) == -1 )
		{
			ArIsError = 1;
			if( Error ) Error->Logf( TEXT("seek Failed %i/%i: %i %i"), InPos, Size, Pos, errno );
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
		if( File >= 0 )
			close( File );
		File = -1;
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
				if( Length >= ARRAY_COUNT(Buffer) )
				{
					if( read( File, V, Length )!=Length )
					{
						ArIsError = 1;
						if( Error ) Error->Logf( TEXT("read failed: Length=%i errno=%i"), Length, (int) errno );
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
					if( Error ) Error->Logf( TEXT("ReadFile beyond EOF %i+%i/%i"), Pos, Length, Size );
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
	int				File;
	FOutputDevice*	Error;
	INT				Size;
	INT				Pos;
	INT				BufferBase;
	INT				BufferCount;
	BYTE			Buffer[1024];
};




class FArchiveFileWriter : public FArchive
{
public:
	FArchiveFileWriter( int InFile, FOutputDevice* InError )
	:	File		( InFile )
	,	Error		( InError )
	,	Pos		(0)
	,	BufferCount	(0)
	{
		ArIsSaving = ArIsPersistent = 1;
	}
	~FArchiveFileWriter()
	{
		guard(FArchiveFileWriter::~FArchiveFileWriter);
		Close();
		unguard;
	}
	void Seek( INT InPos )
	{
		Flush();
		if( lseek(File,InPos,SEEK_SET) == -1 )
		{
			ArIsError = 1;
			if( Error ) Error->Logf( LocalizeError("SeekFailed",TEXT("Core")) );
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
		if( ( File >= 0 ) && ( close( File ) == -1 ) )
		{
			ArIsError = 1;
			if( Error ) Error->Logf( LocalizeError("WriteFailed",TEXT("Core")) );
		}
		File = -1;
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
		if( BufferCount && write( File, Buffer, BufferCount )!=BufferCount )
		{
			ArIsError = 1;
			if( Error ) Error->Logf( LocalizeError("WriteFailed",TEXT("Core")) );
		}
		BufferCount=0;
	}
protected:
	int				File;
	FOutputDevice*	Error;
	INT				Pos;
	INT				BufferCount;
	BYTE			Buffer[4096];
};

class FFileManagerUnix : public FFileManagerGeneric
{
public:
    FFileManagerUnix(void)
    {
        BaseDir[0] = 0;
    }


    static bool isdir(const char *fname)
    {
        struct stat statbuf;
        if (stat(fname, &statbuf) != -1)
            return( (S_ISDIR(statbuf.st_mode)) ? true : false );
        return(false);
    }


    // Hack for Ogg file stream...  --ryan.
    FILE *fopenReadRespectHomeDir(const TCHAR *FakeFilename)
    {
        char ansipath[PATH_MAX];
        TCHAR tmp[PATH_MAX];
		FILE* File = NULL;

		if ( BaseDir[0] )   // Use home dir?
        {
    		appStrcpy( tmp, BaseDir );
            appStrcat( tmp, FakeFilename );
            unixToANSI( ansipath, tmp );
    		char *Filename = appUnixPath(ansipath);
            if (!isdir(Filename))
                File = fopen(Filename, "rb");
        }

        if ( File == NULL )
        {
            unixToANSI( ansipath, FakeFilename );
            char *Filename = appUnixPath(ansipath);
            if (!isdir(Filename))
                File = fopen(Filename, "rb");
        }

        return(File);
    }

	UBOOL GetFileLastModTime( const TCHAR* FakeFilename, DWORD &Hours, DWORD &Minutes, DWORD &Secs, DWORD &Day, DWORD &Month, DWORD &Year )
	{
        FILE* File = fopenReadRespectHomeDir(FakeFilename);
		if ( !File )
			return 0;

		struct stat statbuf;
        int rc = fstat(fileno(File), &statbuf);
		fclose(File);
        if (rc == -1)
            return 0;

        struct tm tm;
        // ufront 2.33: Windows spells the reentrant localtime `localtime_s`, with the arguments reversed
        // and an errno_t return (0 == success).
#ifdef _WIN32
        if (localtime_s(&tm, &statbuf.st_mtime) != 0)
#else
        if (localtime_r(&statbuf.st_mtime, &tm) == NULL)
#endif
            return 0;

        Hours = (DWORD) (tm.tm_hour);  // 0 - 23
        Minutes = (DWORD) (tm.tm_min);  // 0 - 59
        Secs = (DWORD) (tm.tm_sec);  // 0 - 59, may be 60 in rare occasions.
        Day = (DWORD) (tm.tm_mday);  // 1 - 31
        Month = (DWORD) (tm.tm_mon + 1);   // 1 - 12
        Year = (DWORD) (tm.tm_year + 1900); // actual year, not offset.

        if (Secs > 59) Secs = 59;  // Pesky leap seconds!

        return 1;
	}

	FArchive* CreateFileReader( const TCHAR* FakeFilename, DWORD Flags, FOutputDevice* Error )
	{
		guard(FFileManagerUnix::CreateFileReader);		

        if ( (FakeFilename == NULL) ||
             (appStrlen(FakeFilename) + appStrlen(BaseDir) >= PATH_MAX) )
            return NULL;

        char ansipath[PATH_MAX];
        TCHAR tmp[PATH_MAX];
		int File = -1;
        struct stat statbuf;

		if ( BaseDir[0] )   // Use home dir?
        {
    		appStrcpy( tmp, BaseDir );
            appStrcat( tmp, FakeFilename );
            unixToANSI( ansipath, tmp );
    		char* Filename = appUnixPath(ansipath);
            if (!isdir(Filename))
    		    File = open(Filename, O_RDONLY | O_BINARY, S_IRUSR | S_IWUSR);
        }

        if ( File < 0 )  // not in homedir? try game dir.
        {
            unixToANSI( ansipath, FakeFilename );
            char *Filename = appUnixPath(ansipath);
            if (!isdir(Filename))
		        File = open(Filename, O_RDONLY | O_BINARY, S_IRUSR | S_IWUSR);
        }

		if ( ( File < 0 ) || ( fstat ( File, &statbuf ) < 0 ) )
		{
			if( Flags & FILEREAD_NoFail )
				appErrorf(TEXT("Failed to read file: %s"),FakeFilename);
			return NULL;
		}

        #if UNREAL_USE_MMAP
        FArchiveFileReaderMMAP *ar = new(TEXT("UnixFileReaderMMAP"))FArchiveFileReaderMMAP(File,Error,statbuf.st_size);
        if (ar->DoMapping())
            return(ar);  // will try white-bread FArchiveFileReader if mmap() fails...

        delete ar;
        #endif

		return new(TEXT("UnixFileReader"))FArchiveFileReader(File,Error,statbuf.st_size);
		unguard;
	}
	FArchive* CreateFileWriter( const TCHAR* FakeFilename, DWORD Flags, FOutputDevice* Error )
	{	
		guard(FFileManagerUnix::CreateFileWriter);

        if ( (FakeFilename == NULL) ||
             (appStrlen(FakeFilename) + appStrlen(BaseDir) >= PATH_MAX) )
            return NULL;

        char ansipath[PATH_MAX];
        TCHAR tmp[PATH_MAX];

		appStrcpy( tmp, BaseDir );
        appStrcat( tmp, FakeFilename );
        unixToANSI( ansipath, tmp );
		char* Filename = appUnixPath(ansipath);

        if (BaseDir[0])  // might need to build dirs in $HOME...
        {
            char ansitmp[PATH_MAX];
            strcpy(ansitmp, Filename);
            char *ptr = strrchr(ansitmp, '/');
            if (ptr != NULL)
            {
                for (ptr = ansitmp; *ptr; ptr++)  // build each chunk.
                {
                    if (*ptr == '/')
                    {
                        *ptr = '\0';
                        // don't care if it fails, since fopen() will tell us.
                        UT_MKDIR(ansitmp);
                        *ptr = '/';
                    }
                }
            }
        }

	    if( Flags & FILEWRITE_EvenIfReadOnly )
			chmod(Filename, S_IREAD | S_IWRITE);
		if( (Flags & FILEWRITE_NoReplaceExisting) && (access(Filename, F_OK) != -1))
			return NULL;

        int Mode = O_WRONLY | O_CREAT | ((Flags & FILEWRITE_Append) ? O_APPEND : O_TRUNC);
		int File = open(Filename, Mode | O_BINARY, S_IRUSR | S_IWUSR);
		if( File < 0 )
		{
			if( Flags & FILEWRITE_NoFail )
				appErrorf( TEXT("Failed to write: %s"), &Filename );
			return NULL;
		}
		//if( Flags & FILEWRITE_Unbuffered )
		//	setvbuf( File, 0, _IONBF, 0 );
		return new(TEXT("UnixFileWriter"))FArchiveFileWriter(File,Error);

		unguard;
	}
	UBOOL Delete( const TCHAR* FakeFilename, UBOOL RequireExists=0, UBOOL EvenReadOnly=0 )
	{	       
		guard(FFileManagerUnix::Delete);

        if ( (FakeFilename == NULL) ||
             (appStrlen(FakeFilename) + appStrlen(BaseDir) >= PATH_MAX) )
            return 0;

        char ansipath[PATH_MAX];
        TCHAR tmp[PATH_MAX];

		appStrcpy( tmp, BaseDir );
        appStrcat( tmp, FakeFilename );
        unixToANSI( ansipath, tmp );
		char* Filename = appUnixPath(ansipath);
		if( EvenReadOnly )
			chmod(Filename, S_IREAD | S_IWRITE);
		return(unlink(Filename)==0 || (errno==ENOENT && !RequireExists));
		unguard;
	}
    // gam ---
    UBOOL IsReadOnly( const TCHAR* Filename )
    {
		guard(FFileManagerUnix::IsReadOnly);

        if ( (Filename == NULL) ||
             (appStrlen(Filename) >= PATH_MAX) )
            return 0;

        if( FileSize( Filename ) < 0 )
            return( 0 );

        char ansipath[PATH_MAX];
        unixToANSI(ansipath, Filename);
		return (access(appUnixPath(ansipath), W_OK) != 0);
		unguard;
    }
    // --- gam
	SQWORD GetGlobalTime( const TCHAR* Filename )
	{
		guard(FFileManagerUnix::GetGlobalTime);
		return 0;
		unguard;
	}
	UBOOL SetGlobalTime( const TCHAR* Filename )
	{
		guard(FFileManagerUnix::SetGlobalTime);
		return 0;	
		unguard;
	}
	UBOOL MakeDirectory( const TCHAR* FakePath, UBOOL Tree=0 )
	{
		guard(FFileManagerUnix::MakeDirectory);

        if ( (FakePath == NULL) ||
             (appStrlen(FakePath) + appStrlen(BaseDir) >= PATH_MAX) )
            return 0;

        char ansipath[PATH_MAX];
        TCHAR tmp[PATH_MAX];

		appStrcpy( tmp, BaseDir );
		TCHAR* Path = appStrcat( tmp, FakePath );
		if( Tree )
			return FFileManagerGeneric::MakeDirectory( Path, Tree );

        unixToANSI(ansipath, tmp);
		return UT_MKDIR(appUnixPath(ansipath))==0 || errno==EEXIST;
		unguard;
	}
	UBOOL DeleteDirectory( const TCHAR* FakePath, UBOOL RequireExists=0, UBOOL Tree=0 )
	{
		guard(FFileManagerUnix::DeleteDirectory);

        if ( (FakePath == NULL) ||
             (appStrlen(FakePath) + appStrlen(BaseDir) >= PATH_MAX) )
            return 0;

        char ansipath[PATH_MAX];
        TCHAR tmp[PATH_MAX];

		appStrcpy( tmp, BaseDir );	
		TCHAR* Path = appStrcat( tmp, FakePath );	
		if( Tree )
			return FFileManagerGeneric::DeleteDirectory( Path, RequireExists, Tree );

        unixToANSI(ansipath, tmp);

		return rmdir(appUnixPath(ansipath))==0 || (errno==ENOENT && !RequireExists);
		unguard;
	}
	TArray<FString> FindFiles( const TCHAR* FakeFilename, UBOOL Files, UBOOL Directories )
	{
		guard(FFileManagerUnix::FindFiles);
		TArray<FString> Result;

		if ( BaseDir[0] )
        {
    		TCHAR* tmp = appStaticString1024();
    		appStrcpy( tmp, BaseDir );
		    TCHAR* Filename = appStrcat( tmp, FakeFilename );
			SearchDirectory( Result, Filename );
        }
		SearchDirectory( Result, FakeFilename );

		return Result;
		unguard;
	}
	UBOOL SetDefaultDirectory( const TCHAR* Filename )
	{
		guard(FFileManagerUnix::SetDefaultDirectory);

        if ( (Filename == NULL) ||
             (appStrlen(Filename) >= PATH_MAX) )
            return 0;

        char ansipath[PATH_MAX];
        unixToANSI(ansipath, Filename);

		return chdir(appUnixPath(ansipath))==0;
		unguard;
	}
	FString GetDefaultDirectory()
	{
		guard(FFileManagerUnix::GetDefaultDirectory);
		{
			ANSICHAR Buffer[PATH_MAX]="";
			getcwd( Buffer, ARRAY_COUNT(Buffer) );
			return appFromAnsi( Buffer );
		}
		unguard;
	}
	// gam ---
	INT CompareFileTimes( const TCHAR* FileA, const TCHAR* FileB )
	{
		guard(FFileManagerUnix::CompareFileTimes);
	
		struct stat StatA, StatB;

		if( stat( appToAnsi( FileA ), &StatA ) != 0 )
		    return 0;

		if( stat( appToAnsi( FileB ), &StatB ) != 0 )
		    return 0;
		    
        return( INT(StatB.st_mtime - StatA.st_mtime) );

        unguard;
	}
	// --- gam
	// ufront 2.37: the home-dir redirect is now OPT-IN, and the default is what every one of our
	// launchers already asked for. It used to be the other way round -- user files (log, User.ini,
	// saves, the download cache) went to ~/.ut2004/System/ unless you remembered `-nohomedir` --
	// which meant EVERY caller carried the flag (web ENGINE_OPTS_DEFAULT, the Android bridge, the
	// native smoke harness, every doc'd command line) and the one that forgot silently wrote its
	// state somewhere nobody looked. `-nohomedir` is still ACCEPTED and is now a no-op, so no
	// existing command line, ini or script breaks; `-usehomedir` restores the old behaviour.
	void Init(UBOOL Startup)
	{
        if (UseHomeDir())
            appStrcpy(BaseDir, CalcHomeDir());
        else
            BaseDir[0] = 0;
	}

    // Opt in with -usehomedir. -nohomedir is the default and is kept only so old command lines
    // keep parsing.
    UBOOL UseHomeDir(void)
    {
        return ParseParam(appCmdLine(),TEXT("usehomedir"));
    }

    const TCHAR *GetUserDir(void)
    {
        return BaseDir[0] ? BaseDir : appBaseDir();
    }

    const TCHAR *CalcHomeDir(void)
    {
        if (!UseHomeDir())
            return(appBaseDir());

	    char *Home = getenv("HOME");
#if defined(_WIN32)
    	// ufront 2.33: Windows has no $HOME and no passwd database. USERPROFILE is its equivalent
    	// (C:\Users\<name>); fall back to the base dir, which is where we land by default anyway.
    	if ( Home == NULL )
    		Home = getenv("USERPROFILE");
    	if ( Home == NULL )
    		return(appBaseDir());
#elif defined(__ANDROID__)
    	// bionic has no getpwent/setpwent/endpwent (no passwd-db enumeration). $HOME is provided
    	// by the Android app shell (set in the JNI bridge, step 2.10b); fall back to cwd otherwise.
    	if ( Home == NULL )
    		Home = (char*)".";
#else
    	if ( Home == NULL )
    	{
    	    uid_t id = getuid();
    		struct passwd *pwd;

    		setpwent();
    		while( (pwd = getpwent()) != NULL )
    		{
    		    if( pwd->pw_uid == id )
    			{
    			    Home = pwd->pw_dir;
    				break;
    			}
    		}
    		endpwent();
    	}
#endif

    	TCHAR* tmp = appStaticString1024();
    	appStrcpy( tmp, appFromAnsi(Home) );

	// !!! FIXME: abstract game.
	#if MACOSX
		// Make sure these two parent folders exist...
		appStrcat( tmp, TEXT("/Library") );
		UT_MKDIR(appToAnsi(tmp));
		appStrcat( tmp, TEXT("/Application Support") );
		UT_MKDIR(appToAnsi(tmp));

		#if DEMOVERSION
			appStrcat( tmp, TEXT("/Unreal Tournament 2004 Demo") );
		#else
			appStrcat( tmp, TEXT("/Unreal Tournament 2004") );
		#endif
	#else
		#if DEMOVERSION
			appStrcat( tmp, TEXT("/.ut2004demo") );
		#else
			appStrcat( tmp, TEXT("/.ut2004") );
		#endif
	#endif

        UT_MKDIR(appToAnsi(tmp));
        appStrcat( tmp, TEXT("/System") );
        UT_MKDIR(appToAnsi(tmp));

        // If STILL not accessible don't use it.
        if ( access( appToAnsi(tmp), F_OK ) == -1 )
        {
            //debugf(TEXT("can't find homedir at [%s]."), homedir);
        }
        else
        {
            appStrcat( tmp, TEXT("/") );
        }

        return(tmp);
    }


protected:
	void SearchDirectory( TArray<FString> &Result, const TCHAR* Filename )
	{
		guard(FFileManagerUnix::SearchDirectory);
		DIR *Dirp;
		struct dirent* Direntp;
		char Path[PATH_MAX];
		char File[PATH_MAX];
		char *Filestart;
		char *Cur;
		UBOOL Match;

		// Initialize Path to Filename.
        unixToANSI( File, Filename );
		strcpy( Path, appUnixPath(File) );

        /* Do this (and case sensitivity check) in appUnixPath, above. --ryan.
		// Convert MS "\" to Unix "/".
		for( Cur = Path; *Cur != '\0'; Cur++ )
			if( *Cur == '\\' )
				*Cur = '/';
        */
	
		// Separate path and filename.
		Filestart = Path;
		for( Cur = Path; *Cur != '\0'; Cur++ )
        {
			if( *Cur == '/' )
				Filestart = Cur + 1;
        }

		// Store filename and remove it from Path.
		strcpy( File, Filestart );
		*Filestart = '\0';

		// Check for empty path.
		if (strlen( Path ) == 0)
			sprintf( Path, "./" );

		// Open directory, get first entry.
		Dirp = opendir( Path );
		if (Dirp == NULL)
		{
			return;
		}

		Direntp = readdir( Dirp );

		// Check each entry.
		while( Direntp != NULL )
		{
			Match = false;

			if( strcmp( File, "*" ) == 0 )
			{
				// Any filename.
				Match = true;
			}
			else if( strcmp( File, "*.*" ) == 0 )
			{
				// Any filename with a '.'.
				if( strchr( Direntp->d_name, '.' ) != NULL )
					Match = true;
			}
			else if( File[0] == '*' )
			{
				// "*.ext" filename.
				if( strstr( Direntp->d_name, (File + 1) ) != NULL )
					Match = true;
			}
			else if( File[strlen( File ) - 1] == '*' )
			{
				// "name.*" filename.
				if( strncmp( Direntp->d_name, File, strlen( File ) - 1 ) == 0 )
					Match = true;
			}
			else if( strstr( File, "*" ) != NULL )
			{
				// single str.*.str match.
				char* star = strstr( File, "*" );
				INT filelen = strlen( File );
				INT starlen = strlen( star );
				INT starpos = filelen - (starlen - 1);
				char prefix[256];
				strncpy( prefix, File, starpos );
				star++;
				if( strncmp( Direntp->d_name, prefix, starpos - 1 ) == 0 )
				{
					// part before * matches
					char* postfix = Direntp->d_name + (strlen(Direntp->d_name) - starlen) + 1;
					if ( strcmp( postfix, star ) == 0 )
						Match = true;
				}
			}
			else
			{
				// Literal filename.
				if( strcmp( Direntp->d_name, File ) == 0 )
					Match = true;
			}

			// Does this entry match the Filename?
			if( Match )
			{
				// Yes, add the file name to Result.
				new(Result)FString(Direntp->d_name);
			}

			// Get next entry.
			Direntp = readdir( Dirp );
		}

		// Close directory.
		closedir( Dirp );

		unguard;
	}


	TCHAR BaseDir[PATH_MAX];
};

#endif  // include-once blocker

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

