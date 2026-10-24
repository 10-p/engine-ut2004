/*=============================================================================
	IpDrvPrivate.h: Unreal TCP/IP driver.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#ifndef UNIPDRV_H
#define UNIPDRV_H

#if _MSC_VER
	#pragma warning( disable : 4201 )
#endif

// Socket API.
// Keyed on the platform, not the compiler: Windows is WinSock whichever compiler built the binary (MinGW
// ships no <sys/socket.h>).
#ifdef _WIN32
	#define __WINSOCK__ 1
	#define SOCKET_API TEXT("WinSock")
#else
	#define __BSD_SOCKETS__ 1
	#if ((MACOSX) || (defined __FreeBSD__))
		#define __BSD_SOCKETS_TRUE_BSD__ 1
	#endif
	#define SOCKET_API TEXT("Sockets")
#endif

// WinSock includes.
#if __WINSOCK__
#pragma pack(push,8)
#ifdef _XBOX
	#include <xtl.h>
#else
	#include <windows.h>
	#include <winsock.h>
	#include <conio.h>
	#include <ws2tcpip.h>	// socklen_t, rather than an #if at every call site
#endif
#pragma pack(pop)
#endif

// BSD socket includes.
#if __BSD_SOCKETS__
	#include <stdio.h>
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/socket.h>
	#include <netinet/in.h>
	#include <arpa/inet.h>
	#include <netdb.h>
	#include <sys/uio.h>
	#include <sys/ioctl.h>
	#include <sys/time.h>
	#include <errno.h>
	#include <pthread.h>
	#include <fcntl.h>
	#include <poll.h>

	// Handle glibc < 2.1.3
	#ifndef MSG_NOSIGNAL
	#define MSG_NOSIGNAL 0x4000
	#endif
#endif

/*----------------------------------------------------------------------------
	API.
----------------------------------------------------------------------------*/

#ifndef IPDRV_API
	#define IPDRV_API DLL_IMPORT
#endif

/*-----------------------------------------------------------------------------
	Includes..
-----------------------------------------------------------------------------*/

#include "Engine.h"
#include "UnNet.h"
#include "UnSocket.h"

/*-----------------------------------------------------------------------------
	Definitions.
-----------------------------------------------------------------------------*/

// Globals.
extern UBOOL GIpDrvInitialized;

/*-----------------------------------------------------------------------------
	More Includes.
-----------------------------------------------------------------------------*/

#include "InternetLink.h"

/*-----------------------------------------------------------------------------
	Host resolution thread.
-----------------------------------------------------------------------------*/

#if __UNIX__
void* ResolveThreadEntry( void* Arg );
#else
DWORD STDCALL ResolveThreadEntry( void* Arg );
#endif

//
// Class for creating a background thread to resolve a host.
//
class FResolveInfo
{
public:
	// Variables.
	in_addr		Addr;
	DWORD		ThreadId;
	ANSICHAR	HostName[256];
	TCHAR		Error[256];

	#if __UNIX__
	pthread_t	ResolveThread;
	#endif

	// Functions.
	FResolveInfo( const TCHAR* InHostName )
	{	
		debugf( TEXT("Resolving %s..."), InHostName );

		appMemcpy( HostName, appToAnsi(InHostName), appStrlen(InHostName) + 1 );
		*Error = 0;
		ThreadId = 1;

// The Win32 arm on every Windows compiler, matching the declaration above and the pthread_t member (which
// switch on __UNIX__): Windows gets the CreateThread resolver.
#ifdef _WIN32
		HANDLE hThread = CreateThread( NULL, 0, ResolveThreadEntry, this, 0, &ThreadId );
		check(hThread);
		CloseHandle( hThread );
#elif defined(__EMSCRIPTEN__)
		// The resolver thread is not started on the web: Emscripten's gethostbyname synthesizes a fake
		// address, so a resolve would "succeed" and the caller would open a socket that cannot exist.
		// Every socket syscall fails there; UHTTPDownload goes through GWebHttp and the net drivers
		// through the host's glue.
		(void)ResolveThread;
#else
		pthread_attr_t ThreadAttributes;
		pthread_attr_init( &ThreadAttributes );
		pthread_attr_setdetachstate( &ThreadAttributes, PTHREAD_CREATE_DETACHED );
		pthread_create( &ResolveThread, &ThreadAttributes, &ResolveThreadEntry, this );
#endif

	}
	UBOOL Resolved()
	{
		// ThreadId==0 (set by the thread just before it exits) is the completion signal. The POSIX
		// resolve thread is created PTHREAD_CREATE_DETACHED (the constructor above) and reaps itself:
		// pthread_join on a detached thread is invalid (bionic aborts the process on it).
		return ThreadId==0;
	}
	const TCHAR* GetError()
	{
		return *Error ? Error : NULL;
	}
	const in_addr GetAddr()
	{
		return Addr;
	}
	const TCHAR* GetHostName()
	{
		return appFromAnsi(HostName);
	}
};

/*-----------------------------------------------------------------------------
	Bind to next available port.
-----------------------------------------------------------------------------*/

//
// Bind to next available port.
//
inline int bindnextport( SOCKET s, struct sockaddr_in* addr, int portcount, int portinc )
{
	guard(bindnextport);

#ifdef __linux__
    // Long explanation follows:
    //
    // Using port 0 tells the OS to allocate an unused port. But, this will
    // allocate the same port as the socket we just closed on Linux's
    // TCP/IP stack. If you get a stray packet from the last socket, the game
    // gets VERY confused. This is most obvious by hitting a passworded server
    // with the wrong password. We connect, fail to gain access, user enters a
    // password and we closesocket() and immediately hit the server again with
    // a newly bound port...and gets some extra unexpected chatter that hangs
    // the client.
    //
    // The workaround is to pick our own random ports if port zero is
    // requested. If we can't find a valid port above 1024 (below 1024 is
    // reserved for the superuser) after a couple tries, we block for three
    // seconds and then continue as normal...the blocking sucks, but
    // tends to fix things, too. Oh well.
    //
    // The MacOS X and Win32 TCP/IP stacks do not exhibit this problem.
    //
    //    --ryan.

    if( addr->sin_port==0 )
    {
        // try 10 random ports...
        for (int i = 0; i < 10; i++)
        {
            short p = ((short) (31742.0f * rand() / (RAND_MAX + 1.0))) + 1024;
            addr->sin_port = htons(p);
            int rc = bindnextport(s, addr, portcount, portinc);
            if (rc != 0)
                return(rc);  // got one.
        }

        debugf( NAME_Warning, TEXT("Failed to manually bind random port. Letting OS do it instead.") );
        appSleep(3.0f);
        addr->sin_port = 0;
    }
#endif




	for( int i=0; i<portcount; i++ )
	{
		if( !bind( s, (sockaddr*)addr, sizeof(sockaddr_in) ) )
		{
			if (ntohs(addr->sin_port) != 0)
				return ntohs(addr->sin_port);
			else
			{
				// 0 means allocate a port for us, so find out what that port was
				struct sockaddr_in boundaddr;
				SOCKLEN size = sizeof(boundaddr);
				getsockname ( s, (sockaddr*)(&boundaddr), &size);
				return ntohs(boundaddr.sin_port);
			}
		}
		if( addr->sin_port==0 )
			break;
		addr->sin_port = htons( ntohs(addr->sin_port) + portinc );
	}
	return 0;
	unguard;
}

inline int getlocalhostaddr( FOutputDevice& Out, in_addr &HostAddr )
{
	guard(getlocalhostaddr);
	int CanBindAll = 0;
	IpSetInt( HostAddr, INADDR_ANY );
	TCHAR Home[256]=TEXT("");
#ifndef _XBOX
	TCHAR HostName[256]=TEXT("");
	ANSICHAR AnsiHostName[256]="";
	if( gethostname( AnsiHostName, 256 ) )
		Out.Logf( TEXT("%s: gethostname failed (%s)"), SOCKET_API, SocketError() );
	appStrcpy( HostName, appFromAnsi(AnsiHostName) );
#endif
	if( Parse(appCmdLine(),TEXT("MULTIHOME="),Home,ARRAY_COUNT(Home)) )
	{
		TCHAR *A, *B, *C, *D;
		A=Home;
		if
		(	(A=Home)!=NULL
		&&	(B=appStrchr(A,'.'))!=NULL
		&&	(C=appStrchr(B+1,'.'))!=NULL
		&&	(D=appStrchr(C+1,'.'))!=NULL )
		{
			IpSetBytes( HostAddr, appAtoi(A), appAtoi(B+1), appAtoi(C+1), \
				appAtoi(D+1) );
		}
		else Out.Logf( TEXT("Invalid multihome IP address %s"), Home );
	}
	else
	{
#ifndef _XBOX
		check(gethostbyname_mutex != NULL);
		gethostbyname_mutex->Lock();
		HOSTENT* HostEnt = gethostbyname( appToAnsi(HostName) );
		if( HostEnt==NULL )
		{
			Out.Logf( TEXT("gethostbyname failed (%s) ... can't get local IP."), SocketHostError() );
		}
		else if( HostEnt->h_addrtype!=PF_INET )
		{
			Out.Logf( TEXT("gethostbyname: non-Internet address (%s)"), \
				SocketHostError() );
		}
		else
#endif
		{
#ifdef _XBOX
			XNADDR xna;
			XNetGetTitleXnAddr( &xna );
			HostAddr = xna.ina;
#else
			HostAddr = *(in_addr*)( *HostEnt->h_addr_list );
#endif
			if( !ParseParam(appCmdLine(),TEXT("PRIMARYNET")) )
				CanBindAll = 1;
			static UBOOL First=0;
			if( !First )
			{
				First = 1;
#ifndef _XBOX
				debugf( NAME_Init, TEXT("%s: I am %s (%s)"), SOCKET_API, HostName, *IpString( HostAddr ) );
#else
				debugf( NAME_Init, TEXT("%s: XBox doesn't support gethostname() (%s)"), SOCKET_API, *IpString( HostAddr ) );
#endif
			}
		}
		gethostbyname_mutex->Unlock();
	}
	return CanBindAll;
	unguard;
}

//
// Get local IP to bind to
//
inline in_addr getlocalbindaddr( FOutputDevice& Out )
{
	guard(getlocalbindaddr);

	in_addr BindAddr;

	// If we can bind to all addresses, return 0.0.0.0
	if( getlocalhostaddr( Out, BindAddr ) )
		IpSetInt( BindAddr, INADDR_ANY );	
	return BindAddr;

	unguard;
}
/*-----------------------------------------------------------------------------
	Public includes.
-----------------------------------------------------------------------------*/

#include "IpDrvClasses.h"
#include "HTTPDownload.h"

#if __STATIC_LINK
#define NAMES_ONLY
#define NATIVE_DEFS_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "IpDrvClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVE_DEFS_ONLY
#undef NAMES_ONLY
#endif

#endif // UNIPDRV_H

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

