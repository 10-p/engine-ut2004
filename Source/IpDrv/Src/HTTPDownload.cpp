/*=============================================================================
	HTTPDownload.cpp: Unreal HTTP File Download
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jack Porter
=============================================================================*/

#include "UnIpDrv.h"
#include "HTTPDownload.h"
#include "WebHttp.h"

/*-----------------------------------------------------------------------------
	UHTTPDownload implementation.
-----------------------------------------------------------------------------*/
UHTTPDownload::UHTTPDownload()
{
	guard(UHTTPDownload::UHTTPDownload);
	// Init.
	ServerSocket = INVALID_SOCKET;
	ResolveInfo = NULL;
	HTTPState = HTTP_Initialized;
#ifdef __EMSCRIPTEN__
	WebHandle = -1;
#endif

	FString Error;
	InitSockets( Error );

	unguard;
}
void UHTTPDownload::Serialize( FArchive& Ar )
{
	guard(UHTTPDownload::Serialize);
	Super::Serialize( Ar );
	Ar << ReceivedData << Headers << DownloadURL;
	unguard;
}
void UHTTPDownload::StaticConstructor()
{
	guard(UHTTPDownload::StaticConstructor);
	// Config.
	new(GetClass(),TEXT("ProxyServerHost"),		RF_Public)UStrProperty(CPP_PROPERTY(ProxyServerHost		), TEXT("Settings"), CPF_Config );
	new(GetClass(),TEXT("ProxyServerPort"),		RF_Public)UIntProperty(CPP_PROPERTY(ProxyServerPort		), TEXT("Settings"), CPF_Config );
	new(GetClass(),TEXT("RedirectToURL"),		RF_Public)UStrProperty(CPP_PROPERTY(DownloadParams		), TEXT("Settings"), CPF_Config );
	new(GetClass(),TEXT("UseCompression"),		RF_Public)UBoolProperty(CPP_PROPERTY(UseCompression		), TEXT("Settings"), CPF_Config );
	new(GetClass(),TEXT("MaxRedirection"),		RF_Public)UBoolProperty(CPP_PROPERTY(MaxRedirection		), TEXT("Settings"), CPF_Config );
	unguard;
}
static FString FormatFilename( const TCHAR* InFilename )
{
	FString Result;
	for( INT i=0;InFilename[i];i++ )
	{
		if( (InFilename[i]>='a'&&InFilename[i]<='z') || 
			(InFilename[i]>='A'&&InFilename[i]<='Z') || 
			(InFilename[i]>='0'&&InFilename[i]<='9') || 
			InFilename[i]=='/' || InFilename[i]=='?' || 
			InFilename[i]=='&' || InFilename[i]=='.' || 
			InFilename[i]=='=' || InFilename[i]=='-'
		  )
			Result = Result + FString::Printf(TEXT("%c"),InFilename[i]);
		else
			Result = Result + FString::Printf(TEXT("%%%02x"),InFilename[i]);
	}
	return Result;
}
void UHTTPDownload::ReceiveFile( UNetConnection* InConnection, INT InPackageIndex, const TCHAR *Params, UBOOL InCompression, INT InAttempt )
{
	guard(UHTTPDownload::ReceiveFile);
	UDownload::ReceiveFile( InConnection, InPackageIndex, Params, InCompression, InAttempt );

	if( !*Params )
		return;
	IsCompressed = InCompression;
	// mh -- HTTP 301/302 support
	CurRedirectLevel = 0;
	if (MaxRedirection < 1) MaxRedirection = 5; // default to 5
	// -- mh

	FPackageInfo& Info = Connection->PackageMap->List( PackageIndex );
	FURL Base(NULL, TEXT(""), TRAVEL_Absolute);
	Base.Port = 80;

	FString File = *Info.URL;

	INT i = File.InStr(TEXT("."), 1);
	FString Extension = i == -1 ? TEXT("") : File.Mid(i+1);
	if( IsCompressed )
		File = File + COMPRESSED_EXTENSION;

	UBOOL FoundSubst = 0;
	FString StrURL = Params;
	while( (i=StrURL.Locs().InStr(TEXT("%guid%")))!=-1 )
	{
		StrURL = StrURL.Left(i) + FString(Info.Guid.String()) + StrURL.Mid(i+6);
		FoundSubst = 1;
	}
	while( (i=StrURL.Locs().InStr(TEXT("%ext%")))!=-1 )
	{
		StrURL = StrURL.Left(i) + Extension + StrURL.Mid(i+5);
		FoundSubst = 1;
	}
	while( (i=StrURL.Locs().InStr(TEXT("%lcext%")))!=-1 )
	{
		StrURL = StrURL.Left(i) + Extension.Locs() + StrURL.Mid(i+7);
		FoundSubst = 1;
	}
	while( (i=StrURL.Locs().InStr(TEXT("%ucext%")))!=-1 )
	{
		StrURL = StrURL.Left(i) + Extension.Caps() + StrURL.Mid(i+7);
		FoundSubst = 1;
	}
	while( (i=StrURL.Locs().InStr(TEXT("%file%")))!=-1 )
	{
		StrURL = StrURL.Left(i) + File + StrURL.Mid(i+6);
		FoundSubst = 1;
	}
	while( (i=StrURL.Locs().InStr(TEXT("%lcfile%")))!=-1 )
	{
		StrURL = StrURL.Left(i) + File.Locs() + StrURL.Mid(i+8);
		FoundSubst = 1;
	}
	while( (i=StrURL.Locs().InStr(TEXT("%ucfile%")))!=-1 )
	{
		StrURL = StrURL.Left(i) + File.Caps() + StrURL.Mid(i+8);
		FoundSubst = 1;
	}
	// if we didn't subsitute anything, just take th filename on the end.
	if( !FoundSubst )
		StrURL = StrURL + File;

	DownloadURL = FURL(&Base, *StrURL, TRAVEL_Relative);
#ifdef __EMSCRIPTEN__
	// No sockets and no resolver in a browser: the registered web-HTTP hook, or an immediate error.
	WebReceiveFile();
	return;
#endif
	const TCHAR *h = (*(*ProxyServerHost)) ? *ProxyServerHost : *DownloadURL.Host;
	DWORD addr = inet_addr(appToAnsi(h));
	if( addr==INADDR_NONE )
	{
		ResolveInfo = new FResolveInfo(h);
		HTTPState = HTTP_Resolving;
	}
	else
	{
		ServerAddr.sin_addr	= *((in_addr*)&addr);
		debugf( TEXT("Decoded IP %s (%s)"), h, *IpString(ServerAddr.sin_addr) );
		HTTPState = HTTP_Resolved;	
	}
	// Show progress while connecting.
	ReceiveData( NULL, 0 );
	unguard;
}
void UHTTPDownload::Tick(void)
{
	guard(UHTTPDownload::Tick);
#ifdef __EMSCRIPTEN__
	WebTick();
	return;
#endif
	switch( HTTPState )
	{
	case HTTP_Resolving:
		check(ResolveInfo);
		// If destination address isn't resolved yet, send nowhere.
		if( !ResolveInfo->Resolved() )
		{
			// Host name still resolving.
			break;
		}
		else if( ResolveInfo->GetError() )
		{
			// Host name resolution just now failed.
			DownloadError( *FString::Printf( LocalizeError( TEXT("InvalidUrl"), TEXT("Engine") ), (*(*ProxyServerHost)) ? *ProxyServerHost : *DownloadURL.Host ) );
			HTTPState  = HTTP_Closed;
			delete ResolveInfo;
			ResolveInfo = NULL;
			return;
		}
		else
		{
			// Host name resolution just now succeeded.
			debugf( TEXT("Resolved %s (%s)"), ResolveInfo->GetHostName(), *IpString(ResolveInfo->GetAddr()) );
			ServerAddr.sin_addr	= ResolveInfo->GetAddr();
			delete ResolveInfo;
			ResolveInfo = NULL;
			HTTPState = HTTP_Resolved;
		}
		break;
	case HTTP_Resolved:
		{
			ServerAddr.sin_family	= AF_INET;
			ServerAddr.sin_port		= htons( (*(*ProxyServerHost)) ? ProxyServerPort : DownloadURL.Port );

			// Connect	
			ServerSocket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
			SetSocketReuseAddr( ServerSocket );
			if( ServerSocket == INVALID_SOCKET )
			{
				DownloadError( LocalizeError( TEXT("ConnectionFailed"), TEXT("Engine") ) );
				HTTPState  = HTTP_Closed;
				return;
			}

			// Don't do this; breaks on Linux 2.4+. --ryan.
			//SetSocketLinger( ServerSocket );

			LocalAddr.sin_family      = AF_INET;
			LocalAddr.sin_addr        = getlocalbindaddr( *GLog );
			if( !bindnextport( ServerSocket, &LocalAddr, 20, 1 ) )
			{
				debugf( NAME_DevNet, TEXT("HTTPDownload: bind() failed") );
				DownloadError( LocalizeError( TEXT("ConnectionFailed"), TEXT("Engine") ) );
				HTTPState  = HTTP_Closed;
				return;
			}
			SetNonBlocking( ServerSocket );
			INT rc = connect( ServerSocket, (SOCKADDR*) &ServerAddr, sizeof(ServerAddr) );
			if ( rc == SOCKET_ERROR )
			{
				INT err = WSAGetLastError();
				if ((err != WSAEINPROGRESS) && (err != WSAEWOULDBLOCK))
				{
					debugf( NAME_DevNet, TEXT("HTTPDownload: connect() failed") );
					DownloadError( LocalizeError( TEXT("ConnectionFailed"), TEXT("Engine") ) );
					HTTPState  = HTTP_Closed;
					return;
				}
			}
			ConnectStartTime = GCurrentTime;
			HTTPState  = HTTP_Connecting;
		}
		break;
	case HTTP_Connecting:
		INT err;
		{
			fd_set SocketSet;
			TIMEVAL SelectTime = {0, 0};
			FD_ZERO( &SocketSet );
			FD_SET( ServerSocket, &SocketSet );
			// Check for writability.  If the socket is writable, the
			// connection attempt succeeded.
			err = select( ServerSocket + 1, 0, &SocketSet, 0, &SelectTime);
		}
		if ( err == SOCKET_ERROR )
		{
			debugf( NAME_DevNet, TEXT("HTTPDownload: select() failed") );
			DownloadError( LocalizeError( TEXT("ConnectionFailed"), TEXT("Engine") ) );
			HTTPState  = HTTP_Closed;
			return;
		}
		else if ( err == 0 )
		{
			if( GCurrentTime - ConnectStartTime > 30 )
			{
				debugf( NAME_DevNet, TEXT("HTTPDownload: connection timed out") );
				DownloadError( LocalizeError( TEXT("ConnectionFailed"), TEXT("Engine") ) );
				HTTPState  = HTTP_Closed;
			}
			//!! Still connecting - timeout?
			return;
		}
		// Socket is writable, so we are connected.  Send HTTP request.
		{
			FString Request = FString::Printf
			(
				TEXT("GET %s HTTP/1.1\r\nHost: %s\r\nUser-Agent: Unreal\r\nConnection: close\r\n\r\n"),
				(*(*ProxyServerHost)) ? *FormatFilename(*DownloadURL.String()) : *(FString(TEXT("/"))+FormatFilename(*(DownloadURL.Map+DownloadURL.OptionString()))),
				(*(*ProxyServerHost)) ? *ProxyServerHost : *DownloadURL.Host
			);
			send( ServerSocket, appToAnsi(*Request), Request.Len(), 0 );
		}
		HTTPState = HTTP_ReceivingHeader;
		break;
	case HTTP_ReceivingHeader:
		{
			FetchData();
			for( INT i=0;i<ReceivedData.Num();i++ )
			{
				if( ReceivedData(i)==13 || ReceivedData(i)==10 )
				{				
					UBOOL EOH = i==0;
					ReceivedData(i) = 0;
					Headers(Headers.AddZeroed()) = appFromAnsi((char*)&ReceivedData(0));
					if( i<ReceivedData.Num()-1 && (ReceivedData(i+1)==13 || ReceivedData(i+1)==10) )
						i++;					
					ReceivedData.Remove(0, i+1);
					if( EOH ) // end of headers
					{
						UBOOL bNeedRedir = false;
						// received a redirect header, want "Location:" header and restart
						if( (Headers(0).Mid(Headers(0).InStr(TEXT(" ")), 5) == TEXT(" 301 ")
							|| Headers(0).Mid(Headers(0).InStr(TEXT(" ")), 5) == TEXT(" 302 "))
							&& (CurRedirectLevel < MaxRedirection))
						{
							bNeedRedir = true;
							CurRedirectLevel++;
						}
						// check we got a 200 OK.
						else if( Headers(0).Mid(Headers(0).InStr(TEXT(" ")), 5) != TEXT(" 200 "))
						{
							DownloadError( *FString::Printf( LocalizeError( TEXT("InvalidUrl"), TEXT("Engine") ), *DownloadURL.String() ) );
							HTTPState  = HTTP_Closed;
							return;
						}
						for( i=0; i<Headers.Num();i++ )
						{
							if( Headers(i).Caps().Left(16) == TEXT("CONTENT-LENGTH: ") )
							{								
								Info->DownloadSize = appAtoi( *Headers(i).Mid(16) );
								if( Info->DownloadSize <= 0 )
									Info->DownloadSize = Info->FileSize;
								if( !IsCompressed && Info->DownloadSize != Info->FileSize )
								{
									DownloadError( LocalizeError( TEXT("NetSize"), TEXT("Engine")) );
									HTTPState  = HTTP_Closed;
									return;
								}
							}
							// mh -- support for HTTP redirection
							else if( Headers(i).Caps().Left(10) == TEXT("LOCATION: ") && bNeedRedir)
							{
								// set the new location
								guard(TEXT("Download Redirection"));
								FURL Base(&DownloadURL, TEXT(""), TRAVEL_Relative);
								Base.Port = 80;
								DownloadURL = FURL(&Base, *(Headers(i).Mid(10)), TRAVEL_Relative);
								debugf(TEXT("Download redirection to: %s"), *DownloadURL.String());

								if( ServerSocket != INVALID_SOCKET )
								{
									closesocket( ServerSocket );
									ServerSocket = INVALID_SOCKET;
								}
								ReceivedData.Empty();
								Headers.Empty();
								const TCHAR *h = (*(*ProxyServerHost)) ? *ProxyServerHost : *DownloadURL.Host;
								DWORD addr = inet_addr(appToAnsi(h));
								if( addr==INADDR_NONE )
								{								
									ResolveInfo = new FResolveInfo(h);
									HTTPState = HTTP_Resolving;
								}
								else
								{
									ServerAddr.sin_addr	= *((in_addr*)&addr);
									debugf( TEXT("Decoded IP %s (%s)"), h, *IpString(ServerAddr.sin_addr) );
									HTTPState = HTTP_Resolved;	
								}
								unguard;
								return;
							}
							// -- mh
						}
						if (bNeedRedir) // didn't receive a redirection location, but needed one
						{
							DownloadError( *FString::Printf( LocalizeError( TEXT("InvalidUrl"), TEXT("Engine") ), *DownloadURL.String() ) );
							HTTPState  = HTTP_Closed;
							return;
						}
						HTTPState = HTTP_ReceivingData;
						break;
					}
					FetchData();
					i = -1;
				}
			}
		}
		break;
	case HTTP_ReceivingData:
		while( ReceivedData.Num() || FetchData() )
		{
			if( ReceivedData.Num() > 0 )
			{
				INT Count = Transfered + ReceivedData.Num() > Info->FileSize ? Info->FileSize - Transfered : ReceivedData.Num(); //!! ?? this doesn't look correct for compressed files.
				if( Count > 0 )
					ReceiveData( &ReceivedData(0), Count );
				else
					HTTPState = HTTP_Closed;
				ReceivedData.Empty();
			}
		}
		break;
	case HTTP_Closed:
		DownloadDone();
		return;
	}
	unguard;
}
UBOOL UHTTPDownload::FetchData()
{
	guard(UHTTPDownload::FetchData);
	BYTE Buf[1024];
	INT Bytes = recv( ServerSocket, (char *)Buf, 1024, 0 );
	if( Bytes == 0 )
	{
		// Close received.
		HTTPState = HTTP_Closed;
		return 0;
	}
	if( Bytes != SOCKET_ERROR )
	{
		appMemcpy( &ReceivedData(ReceivedData.Add(Bytes)), Buf, Bytes );
		return 1;
	}
	if( Bytes == SOCKET_ERROR && WSAGetLastError() == WSAEWOULDBLOCK )
		return 0; // No data

	debugf( NAME_DevNet, TEXT("HTTPDownload: recv() failed") );
	DownloadError( LocalizeError( TEXT("ConnectionFailed"), TEXT("Engine") ) );
	HTTPState  = HTTP_Closed;
	return 0;
	unguard;	
}
UBOOL UHTTPDownload::TrySkipFile()
{
	guard(UHTTPDownload::TrySkipFile);
	if( Super::TrySkipFile() )
	{
		HTTPState = HTTP_Closed;
		Connection->Logf( TEXT("SKIP GUID=%s"), Info->Guid.String() );
		return 1;
	}
	return 0;
	unguard;
}
void UHTTPDownload::Destroy()
{
	guard(UHTTPDownload::Destroy);
#ifdef __EMSCRIPTEN__
	WebClose();
#endif
	if( ServerSocket != INVALID_SOCKET )
	{
		closesocket( ServerSocket );
		ServerSocket = INVALID_SOCKET;
	}
	Super::Destroy();
	unguard;
}

#ifdef __EMSCRIPTEN__
/*-----------------------------------------------------------------------------
	The browser build: no sockets, so the fetch goes through whoever registered
	the web-HTTP hook table (WebHttp.h). Nobody registered = fail at once, and the
	engine moves on to the next download manager the server named.
-----------------------------------------------------------------------------*/

FWebHttpHooks* GWebHttp = NULL;

#define WEBHTTP_READ_SIZE		(256*1024)	// one Read()
#define WEBHTTP_READS_PER_TICK	8			// so one tick hands the file writer at most 2 MB
#define WEBHTTP_STALL_SECONDS	30			// the native path's connect timeout, applied to any silence

void UHTTPDownload::WebReceiveFile()
{
	guard(UHTTPDownload::WebReceiveFile);

	// The URL the native path requests, put back together: FormatFilename escapes the path exactly as
	// the GET line does. The scheme is kept, so an https redirect works here although it never could
	// over the native plain socket.
	const UBOOL bSecure = DownloadURL.Protocol.Caps()==TEXT("HTTPS");
	FString Url = FString::Printf( TEXT("%s://%s"), bSecure ? TEXT("https") : TEXT("http"), *DownloadURL.Host );
	if( DownloadURL.Port!=80 && !(bSecure && DownloadURL.Port==443) )
		Url += FString::Printf( TEXT(":%i"), DownloadURL.Port );
	Url += FString(TEXT("/")) + FormatFilename( *(DownloadURL.Map+DownloadURL.OptionString()) );

	WebHandle = GWebHttp ? GWebHttp->Open( Connection, TCHAR_TO_ANSI(*Url) ) : -1;
	if( WebHandle < 0 )
	{
		// Nobody can fetch for this connection. Tick() reports it and the next manager takes over.
		debugf( NAME_DevNet, TEXT("HTTPDownload: no web fetch available for this connection") );
		DownloadError( LocalizeError( TEXT("ConnectionFailed"), TEXT("Engine") ) );
		HTTPState = HTTP_Closed;
		return;
	}
	debugf( TEXT("HTTPDownload: fetching %s"), *Url );
	ConnectStartTime = GCurrentTime;
	HTTPState = HTTP_ReceivingHeader;

	// Show progress while connecting.
	ReceiveData( NULL, 0 );
	unguard;
}
void UHTTPDownload::WebTick()
{
	guard(UHTTPDownload::WebTick);
	if( HTTPState==HTTP_Closed )
	{
		// As on the native path, nothing may follow DownloadDone(): it can start the next download,
		// which deletes this object.
		WebClose();
		DownloadDone();
		return;
	}
	if( WebHandle < 0 )
		return;

	INT Status=WEBHTTP_Failed, Received=0, Total=-1;
	if( !GWebHttp || GWebHttp->Poll( WebHandle, &Status, &Received, &Total )!=0 )
		Status = WEBHTTP_Failed;
	if( Status==WEBHTTP_Failed )
	{
		debugf( TEXT("HTTPDownload: fetch failed after %i bytes"), Transfered );
		DownloadError( *FString::Printf( LocalizeError( TEXT("InvalidUrl"), TEXT("Engine") ), *DownloadURL.String() ) );
		HTTPState = HTTP_Closed;
		return;
	}

	if( HTTPState==HTTP_ReceivingHeader && Status!=WEBHTTP_Pending )
	{
		// The response has begun: what the native path reads from Content-Length.
		if( Total > 0 )
		{
			Info->DownloadSize = Total;
			if( !IsCompressed && Info->DownloadSize != Info->FileSize )
			{
				DownloadError( LocalizeError( TEXT("NetSize"), TEXT("Engine")) );
				HTTPState = HTTP_Closed;
				return;
			}
		}
		HTTPState = HTTP_ReceivingData;
	}

	UBOOL bGotData = 0;
	if( HTTPState==HTTP_ReceivingData )
	{
		if( ReceivedData.Num() < WEBHTTP_READ_SIZE )
			ReceivedData.Add( WEBHTTP_READ_SIZE - ReceivedData.Num() );
		for( INT i=0; i<WEBHTTP_READS_PER_TICK; i++ )
		{
			INT Count = GWebHttp->Read( WebHandle, &ReceivedData(0), WEBHTTP_READ_SIZE );
			if( Count <= 0 )
				break;
			ReceiveData( &ReceivedData(0), Count );
			bGotData = 1;
			if( *Error )
			{
				// The cache file could not be written: more bytes will not help.
				HTTPState = HTTP_Closed;
				return;
			}
		}
	}

	if( Status==WEBHTTP_Done )
	{
		// Done is only reported once the whole body has been read.
		HTTPState = HTTP_Closed;
	}
	else if( bGotData )
	{
		ConnectStartTime = GCurrentTime;
	}
	else if( GCurrentTime - ConnectStartTime > WEBHTTP_STALL_SECONDS )
	{
		debugf( NAME_DevNet, TEXT("HTTPDownload: fetch stalled") );
		DownloadError( LocalizeError( TEXT("ConnectionFailed"), TEXT("Engine") ) );
		HTTPState = HTTP_Closed;
	}
	unguard;
}
void UHTTPDownload::WebClose()
{
	guard(UHTTPDownload::WebClose);
	if( WebHandle >= 0 )
	{
		if( GWebHttp )
			GWebHttp->Close( WebHandle );
		WebHandle = -1;
	}
	ReceivedData.Empty();
	unguard;
}
#endif

IMPLEMENT_CLASS(UHTTPDownload)
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

