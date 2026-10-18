/*=============================================================================
	FOutputDeviceFile.h: ANSI file output device.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#if defined(__ANDROID__)
// GLog goes straight to logcat (tag "UT2004"); see the echo block below.
#include <android/log.h>
#endif

//
// ANSI file output device.
//
class FOutputDeviceFile : public FOutputDevice
{
public:
	FOutputDeviceFile()
	: LogAr( NULL )
	, Opened( 0 )
	, Dead( 0 )
    , bLogEvent( 1 ) // gam
	{
		Filename[0]=0;
	}
	~FOutputDeviceFile()
	{
		if( LogAr )
		{
			Logf( NAME_Log, TEXT("Log file closed, %s"), appTimestamp() );
			delete LogAr;
			LogAr = NULL;
		}
	}
	void Flush()
	{
		if( LogAr )
			LogAr->Flush();
	}
	void Serialize( const TCHAR* Data, enum EName Event )
	{
		static UBOOL Entry=0;
		if( !GIsCriticalError || Entry )
		{
			if( !FName::SafeSuppressed(Event) )
			{
				if( !LogAr && !Dead )
				{
					// Make log filename.
					if( !Filename[0] )
					{
                        // The base-dir prefix belongs to the Windows file manager, not to the
                        // platform: this build is a Windows binary using FFileManagerUnix, which
                        // resolves relative paths itself (including the ~/.ut2004 redirect under
                        // -usehomedir), so prefixing an absolute "Z:\path\System/" would nest it.
                        // Leave it empty, as on Linux.
                        #if WIN32 && defined(_MSC_VER)
						appStrcpy( Filename, appBaseDir() );
                        #endif

						if
						(	!Parse(appCmdLine(), TEXT("LOG="), Filename+appStrlen(Filename), ARRAY_COUNT(Filename)-appStrlen(Filename) )
						&&	!Parse(appCmdLine(), TEXT("ABSLOG="), Filename, ARRAY_COUNT(Filename) ) )
						{
							// gam ---
							if( ParseParam( appCmdLine(), TEXT("SOAKING") ) )
							{
								TCHAR *InsertPoint;
								const INT MaxTries = 9999;
								INT i;
							    
								TCHAR ComputerName[256];
							    
								appStrcpy( ComputerName, appComputerName() );
							    
								for( i = 0; ComputerName[i] != '\0'; i++ )
									ComputerName[i] = appToUpper( ComputerName[i] );

								InsertPoint = Filename + appStrlen(Filename);

								for( i = 0; i < MaxTries; i++ )
								{
									appSprintf( InsertPoint, TEXT("%s_SOAK_%04d.log"), ComputerName, i );

									if( GFileManager->FileSize( Filename ) < 0 )
										break;
								}
		                        
								if( i >= MaxTries )
								{
									appStrcpy( InsertPoint, appPackage() );
									appStrcat( InsertPoint, TEXT(".log") );
								}
							}
							// --- gam
							else
							{
								FString pName = appPackage();
								FString ModPath;
								if ( Parse(appCmdLine(), TEXT("-MOD="),ModPath) )
								{
									if ( pName == TEXT("UT2004") )
										pName = FString::Printf(TEXT("..%s%s%ssystem%s%s"),PATH_SEPARATOR,*ModPath,PATH_SEPARATOR,PATH_SEPARATOR,*ModPath);
								}

								appStrcat( Filename, *pName );
								appStrcat( Filename, TEXT(".log") );
							}
						}
					}

					// Open log file.
					LogAr = GFileManager->CreateFileWriter( Filename, FILEWRITE_AllowRead|FILEWRITE_Unbuffered|(Opened?FILEWRITE_Append:0));
					if( LogAr )
					{
						Opened = 1;
#if UNICODE && !FORCE_ANSI_LOG
                        #if __UNIX__
                        if (GUnicodeOS) {
                        #endif
						_WORD UnicodeBOM = UNICODE_BOM;
						LogAr->Serialize( &UnicodeBOM, 2 );
                        #if __UNIX__
                        }
                        #endif
#endif
						Logf( NAME_Log, TEXT("Log file open, %s"), appTimestamp() );
					}
					else Dead = 1;
				}
				if( LogAr && Event!=NAME_Title )
				{
#if ((FORCE_ANSI_LOG && UNICODE) || (__UNIX__))
                    #if __UNIX__
                    if (!GUnicodeOS) {
                    #endif

					TCHAR Ch[1024];
					ANSICHAR ACh[1024];

                    // gam ---
                    if( bLogEvent )
					    appSprintf( Ch, TEXT("%s: %s%s"), FName::SafeString(Event), Data, LINE_TERMINATOR );
                    else
					    appSprintf( Ch, TEXT("%s%s"), Data, LINE_TERMINATOR );
                    // --- gam

					INT i;
					for( i=0; Ch[i]; i++ )
						ACh[i] = ToAnsi(Ch[i] );
					ACh[i] = 0;
					LogAr->Serialize( ACh, i );
                    //LogAr->Flush(); //amb

                    #if __UNIX__
                    } else {
					WriteRaw( FName::SafeString(Event) );
					WriteRaw( TEXT(": ") );
					WriteRaw( Data );
					WriteRaw( LINE_TERMINATOR );
                    }
                    #endif
#else
                    // gam ---
                    if( bLogEvent )
                    {
					    WriteRaw( FName::SafeString(Event) );
					    WriteRaw( TEXT(": ") );
                    }
                    // --- gam
					WriteRaw( Data );
					WriteRaw( LINE_TERMINATOR );
#endif
#ifdef _DEBUG
#if _MSC_VER
					TCHAR Temp[1024];

                    // gam ---
                    if( bLogEvent )
					    appSprintf( Temp, TEXT("%s: %s%s"), FName::SafeString(Event), Data, LINE_TERMINATOR );
                    else
    					appSprintf( Temp, TEXT("%s%s"), Data, LINE_TERMINATOR );
                    // --- gam

					OutputDebugString(Temp);
#endif
#endif
				}
				// Also echo every non-suppressed line to stdout, besides the log file above: the terminal
				// natively, Module.print -> console.log on the web. TCHAR->ANSI through appToAnsi with explicit
				// buffers, correct for either TCHAR width (the web is ANSI, native is UNICODE with a 2-byte
				// TCHAR that printf("%ls") would misread against Linux's 4-byte wchar_t). The same
				// "Event: Data" text and 1024 cap as the file sink, so the two compare byte for byte; fflush so
				// a piped stdout never loses the line.
				if( Event != NAME_Title )
				{
					ANSICHAR EchoEvent[256], EchoData[1024];
					appToAnsi( FName::SafeString(Event), EchoEvent, ARRAY_COUNT(EchoEvent) );
					appToAnsi( Data, EchoData, ARRAY_COUNT(EchoData) );
#if defined(__ANDROID__)
					// Android: GLog goes straight to logcat (tag "UT2004"), one entry per line,
					// instead of the stdout pipe (SDLLaunch.cpp), which is subject to stdio
					// buffering and logd throttling; the pipe stays for raw printf and stderr. Use
					// appToAnsi's return value: its ANSI-build branch returns the input unchanged
					// and does not fill EchoEvent/EchoData. Data is the %s argument, so '%' in the
					// text is safe.
					__android_log_print( ANDROID_LOG_INFO, "UT2004", "%s: %s",
						appToAnsi( FName::SafeString(Event), EchoEvent, ARRAY_COUNT(EchoEvent) ),
						appToAnsi( Data, EchoData, ARRAY_COUNT(EchoData) ) );
#else
					printf( "%s: %s\n",
						appToAnsi( FName::SafeString(Event), EchoEvent, ARRAY_COUNT(EchoEvent) ),
						appToAnsi( Data, EchoData, ARRAY_COUNT(EchoData) ) );
					fflush( stdout );
#endif
					// Flush the file sink per line too, so the log file is current for a reader mid-run
					// (or after a crash).
					if( LogAr )
						LogAr->Flush();
				}
				if( GLogHook )
					GLogHook->Serialize( Data, Event );
			}
		}
		else
		{
			Entry=1;
			try
			{
				// Ignore errors to prevent infinite-recursive exception reporting.
				Serialize( Data, Event );
			}
			catch( ... )
			{}
			Entry=0;
		}
	}
	FArchive* LogAr;
	TCHAR Filename[1024];
    UBOOL bLogEvent; // gam
private:
	UBOOL Opened, Dead;
	void WriteRaw( const TCHAR* C )
	{
		LogAr->Serialize( const_cast<TCHAR*>(C), appStrlen(C)*sizeof(TCHAR) );
        //LogAr->Flush(); //amb
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

