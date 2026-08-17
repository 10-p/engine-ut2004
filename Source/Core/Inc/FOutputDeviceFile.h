/*=============================================================================
	FOutputDeviceFile.h: ANSI file output device.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#if defined(__ANDROID__)
// ufront (Android): route GLog straight to logcat (tag "UT2004"). See the echo block below.
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
                        // ufront 2.33: the base-dir prefix belongs to the WINDOWS FILE MANAGER, not to
                        // Windows-the-platform. This build is a hybrid — a Windows binary using
                        // FFileManagerUnix — and that manager resolves relative paths itself, including
                        // the ~/.ut2004 home-dir redirect (only when -usehomedir asks for it, since
                        // ufront 2.37). Prefixing an already
                        // absolute "Z:\path\System/" then produced the nested absurdity
                        //   ~/.ut2004/System/Z:/tmp/.../System/UT2004.log
                        // which is why the log looked missing. Behave like Linux: leave it empty and let
                        // the file manager decide.
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
				// ufront (dual-sink logging): ALSO echo every non-suppressed line to stdout, in ADDITION to
				// the log file above -- terminal on native, Module.print -> console.log on web. A single sink
				// (web console-only; native file-only, and its writer did not always open) made a fully-running
				// engine look "silent"/hung -- the ut2004 native misdiagnosis. Convert TCHAR->ANSI with explicit
				// buffers (appToAnsi) so this is correct regardless of TCHAR WIDTH: web is ANSI
				// (NO_UNICODE_OS_SUPPORT => TCHAR=char) but native is UNICODE (2-byte TCHAR), and a raw
				// printf("%ls") would misread the 2-byte string on native (Linux wchar_t is 4-byte) and print
				// nothing -- the exact reason native GLog was invisible. Same "Event: Data" text + 1024 cap as the
				// file sink above, so the two are byte-comparable (the e2e log-parity check asserts file==console).
				// fflush so a piped/redirected stdout (fully buffered) never loses the line. Mirrors ut99 (2.8) /
				// unreal1 (2.6e); NOT #ifdef'd (standard UE dev behaviour).
				if( Event != NAME_Title )
				{
					ANSICHAR EchoEvent[256], EchoData[1024];
					appToAnsi( FName::SafeString(Event), EchoEvent, ARRAY_COUNT(EchoEvent) );
					appToAnsi( Data, EchoData, ARRAY_COUNT(EchoData) );
#if defined(__ANDROID__)
					// Android: write GLog straight to logcat (tag "UT2004"), one clean entry per
					// line. This bypasses the stdout->pipe->logcat hop (SDLLaunch.cpp) which is
					// subject to stdio buffering + logd throttling of the boot burst, and avoids
					// double-logging (GLog no longer prints to stdout, so it doesn't also traverse
					// that pipe). Mirrors the web Module.print->console.log path. The pipe stays for
					// raw printf ([ENGINE-DBG]/[WebRTC]) + stderr/crash output (FOutputDeviceAnsiError).
					// Use appToAnsi's RETURN value: its ANSI-build (#else) branch returns the input
					// unchanged and does NOT fill EchoEvent/EchoData (those are only written in the
					// UNICODE build), so the buffers above are garbage on Android — the return is the
					// correct string on both builds. Data is the %s ARGUMENT so '%' in text is safe.
					__android_log_print( ANDROID_LOG_INFO, "UT2004", "%s: %s",
						appToAnsi( FName::SafeString(Event), EchoEvent, ARRAY_COUNT(EchoEvent) ),
						appToAnsi( Data, EchoData, ARRAY_COUNT(EchoData) ) );
#else
					printf( "%s: %s\n",
						appToAnsi( FName::SafeString(Event), EchoEvent, ARRAY_COUNT(EchoEvent) ),
						appToAnsi( Data, EchoData, ARRAY_COUNT(EchoData) ) );
					fflush( stdout );
#endif
					// Flush the file sink per line too, so the log FILE reflects content LIVE (this writer
					// buffers, so a mid-run reader — the e2e log-parity check, or a crash post-mortem — would
					// otherwise see an empty file until close). Keeps the two sinks in step.
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

