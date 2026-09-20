/*=============================================================================
	ProxyNetDriver.cpp: the client-only net driver behind "Proxy networking".

	Follows IpDrv/Src/TcpNetDriver.cpp function for function; where that file calls
	sendto()/recvfrom(), this one calls the proxynet ABI. What TcpNetDriver does that a
	CLIENT never needs is simply absent: no listen socket, no accept path, no ICMP
	port-unreachable handling, no per-IP connection limiter.
=============================================================================*/

#include "Engine.h"
#include "UnNet.h"
#include "ProxyNetDrv.h"
#include "WebHttp.h"

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// The same figures TcpNetDriver.cpp uses, because the far end IS a TcpNetDriver: the packet size the
// server will send, and the per-packet overhead the rate limiter must charge so NETSPEED means to us
// what it means to a native client.
#define PROXYNET_IP_HEADER_SIZE     (20)
#define PROXYNET_UDP_HEADER_SIZE    (PROXYNET_IP_HEADER_SIZE+8)
#define PROXYNET_SLIP_HEADER_SIZE   (PROXYNET_UDP_HEADER_SIZE+4)
#define PROXYNET_MAX_PACKET         (512)
#define PROXYNET_RECV_BUFFER        (576)

/*-----------------------------------------------------------------------------
	UProxyNetConnection.
-----------------------------------------------------------------------------*/

UProxyNetConnection::UProxyNetConnection( INT InConnId, UNetDriver* InDriver, EConnectionState InState, const FURL& InURL )
:	UNetConnection	( InDriver, InURL )
,	ConnId			( InConnId )
,	RemoteHost		( InURL.Host )
,	RemotePort		( InURL.Port )
{
	guard(UProxyNetConnection::UProxyNetConnection);

	// Init the connection — as UTcpipConnection does.
	State			= InState;
	MaxPacket		= PROXYNET_MAX_PACKET;
	PacketOverhead	= PROXYNET_SLIP_HEADER_SIZE;
	OpenedTime		= GCurrentTime;
	InitOut();

	// No address resolution here: the host string goes to the game proxy as typed, and the PROXY
	// resolves it (and refuses private address space) — a browser has no resolver to ask.
	unguard;
}

void UProxyNetConnection::CloseTransport()
{
	guard(UProxyNetConnection::CloseTransport);
	if( ConnId >= 0 )
	{
		ue_proxynet_close( ConnId );
		ConnId = -1;
	}
	unguard;
}

void UProxyNetConnection::Destroy()
{
	guard(UProxyNetConnection::Destroy);
	// ★ THE CONNECTION OWNS ITS TRANSPORT, like a socket. ue_proxynet_connect() hands out a NEW id per
	// call, so at a server map change the departing level's connection and the arriving one are two ids,
	// exactly as they are two UDP sockets natively — closing this one cannot touch the other.
	CloseTransport();
	Super::Destroy();
	unguard;
}

void UProxyNetConnection::LowLevelSend( void* Data, INT Count )
{
	guard(UProxyNetConnection::LowLevelSend);
	if( ConnId >= 0 && Count > 0 )
	{
		clock(Driver->SendCycles);
		// Non-blocking, and allowed to drop: before the proxy says `ready` there is nowhere to send, and
		// under back-pressure a datagram transport loses packets. The engine's own reliability layer
		// retransmits what matters — which is also what carries the first HELLO to a server still loading.
		ue_proxynet_send( ConnId, Data, Count );
		unclock(Driver->SendCycles);
	}
	unguard;
}

FString UProxyNetConnection::LowLevelGetRemoteAddress()
{
	guard(UProxyNetConnection::LowLevelGetRemoteAddress);
	return FString::Printf( TEXT("%s:%i"), *RemoteHost, RemotePort );
	unguard;
}

FString UProxyNetConnection::LowLevelDescribe()
{
	guard(UProxyNetConnection::LowLevelDescribe);
	return FString::Printf
	(
		TEXT("%s %s:%i (proxy conn %i) state: %s"),
		*URL.Host,
		*RemoteHost,
		RemotePort,
		ConnId,
			State==USOCK_Pending	?	TEXT("Pending")
		:	State==USOCK_Open		?	TEXT("Open")
		:	State==USOCK_Closed		?	TEXT("Closed")
		:								TEXT("Invalid")
	);
	unguard;
}

IMPLEMENT_CLASS(UProxyNetConnection);

/*-----------------------------------------------------------------------------
	UProxyNetDriver.
-----------------------------------------------------------------------------*/

UBOOL UProxyNetDriver::InitConnect( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error )
{
	guard(UProxyNetDriver::InitConnect);
	if( !Super::InitConnect( InNotify, ConnectURL, Error ) )
		return 0;
	NormalizeConfigDefaults();

	if( !ConnectURL.Host.Len() )
	{
		Error = TEXT("Proxy networking needs a server address (host:port).");
		return 0;
	}
	RegisterWebHttp();

	// Connect to remote — through the game proxy. A NEW connection id every time.
	INT NewConnId = ue_proxynet_connect( TCHAR_TO_ANSI(*ConnectURL.Host), ConnectURL.Port );
	if( NewConnId < 0 )
	{
		// The module is absent, or this build has no game proxy configured. Fail here, in words, rather
		// than sit in "Connecting…" until InitialConnectTimeout.
		Error = TEXT("Proxy networking is not available in this build.");
		debugf( NAME_Log, TEXT("ProxyNetDrv: %s"), *Error );
		return 0;
	}

	// Create new connection.
	ServerConnection = new UProxyNetConnection( NewConnId, this, USOCK_Pending, ConnectURL );
	debugf( NAME_DevNet, TEXT("ProxyNetDrv: game client -> %s:%i (proxy conn %i), rate %i"),
		*ConnectURL.Host, ConnectURL.Port, NewConnId, ServerConnection->CurrentNetSpeed );

	// Create channel zero.
	GetServerConnection()->CreateChannel( CHTYPE_Control, 1, 0 );

	return 1;
	unguard;
}

UBOOL UProxyNetDriver::InitListen( FNetworkNotify* InNotify, FURL& LocalURL, FString& Error )
{
	guard(UProxyNetDriver::InitListen);
	// A browser cannot accept UDP, and the game proxy carries a client to ONE server — it is not a
	// rendezvous. Hosting is what P2P networking is for: refuse in words.
	Error = TEXT("Proxy networking cannot host a game. Switch to P2P networking to host.");
	debugf( NAME_Log, TEXT("ProxyNetDrv: %s"), *Error );
	return 0;
	unguard;
}

void UProxyNetDriver::TickDispatch( FLOAT DeltaTime )
{
	guard(UProxyNetDriver::TickDispatch);
	Super::TickDispatch( DeltaTime );

	UProxyNetConnection* Conn = GetServerConnection();
	if( !Conn || Conn->ConnId < 0 )
		return;

	// The transport's verdict. CONNECTING covers "reached the proxy" AND "queued by the proxy" (it paces
	// new connections to one game server), so it is simply waited out. CLOSED/INVALID is final: refused,
	// unreachable, or lost beyond the proxy's resume window.
	//
	// ★ Never promote the UE connection to USOCK_Open here. It stays USOCK_Pending through the whole
	// download + join handshake and only HandleClientPlayer opens it — exactly as under TcpNetDriver,
	// which knows nothing of connection state either. This block only ever CLOSES.
	INT TransportState = ue_proxynet_state( Conn->ConnId );
	if( (TransportState==PROXYNET_STATE_CLOSED || TransportState==PROXYNET_STATE_INVALID) && Conn->State!=USOCK_Closed )
	{
		debugf( NAME_DevNet, TEXT("ProxyNetDrv: transport closed (proxy conn %i) - disconnecting"), Conn->ConnId );
		Conn->State = USOCK_Closed;
		return;
	}

	// Process all incoming packets.
	if( Conn->State==USOCK_Open || Conn->State==USOCK_Pending )
	{
		BYTE Data[PROXYNET_RECV_BUFFER];
		for( ; ; )
		{
			clock(RecvCycles);
			INT Size = ue_proxynet_recv( Conn->ConnId, Data, sizeof(Data) );
			unclock(RecvCycles);
			if( Size <= 0 )
				break;
			Conn->ReceivedRawPacket( Data, Size );
			// ReceivedRawPacket can close the connection (a malformed packet, a server-side close).
			if( Conn->State==USOCK_Closed || Conn->ConnId < 0 )
				break;
		}
	}
	unguard;
}

FString UProxyNetDriver::LowLevelGetNetworkNumber()
{
	guard(UProxyNetDriver::LowLevelGetNetworkNumber);
	// TcpNetDriver answers with the local interface address. A browser has none to report, and the
	// address the game server sees belongs to the proxy.
	return TEXT("proxy");
	unguard;
}

void UProxyNetDriver::LowLevelDestroy()
{
	guard(UProxyNetDriver::LowLevelDestroy);
	// The connection closes its own transport in Destroy(); this covers a driver torn down first.
	if( GetServerConnection() )
		GetServerConnection()->CloseTransport();
	unguard;
}

UProxyNetConnection* UProxyNetDriver::GetServerConnection()
{
	return (UProxyNetConnection*)ServerConnection;
}

/*-----------------------------------------------------------------------------
	Redirect downloads.

	A native server names an HTTP redirect (`DLMGR CLASS=IpDrv.HTTPDownload`). In a browser
	IpDrv's UHTTPDownload cannot open a socket, so it asks the table in IpDrv/Inc/WebHttp.h —
	and this package, which owns the `ue_proxynet_http_*` imports, is what fills it. The game
	proxy does the fetch for a LIVE session and pays for it from that session's download
	allowance; any refusal is simply FAILED and the engine falls back to the in-band download.
-----------------------------------------------------------------------------*/

// The hook's status values ARE the ABI's: they cross unconverted.
static_assert( WEBHTTP_Pending==PROXYNET_HTTP_PENDING && WEBHTTP_Receiving==PROXYNET_HTTP_RECEIVING
	&& WEBHTTP_Done==PROXYNET_HTTP_DONE && WEBHTTP_Failed==PROXYNET_HTTP_FAILED, "WebHttp.h and ue_proxynet_abi.h disagree" );

static INT ProxyNetHttpOpen( UNetConnection* Connection, const ANSICHAR* Url )
{
	// Only for a download that belongs to one of OUR connections: its session is what the proxy
	// charges. Anything else (a P2P connection in the same build) gets -1 and falls back at once.
	UProxyNetConnection* Conn = Cast<UProxyNetConnection>( Connection );
	if( !Conn || Conn->ConnId < 0 )
		return -1;
	return ue_proxynet_http_open( Conn->ConnId, Url );
}
static INT ProxyNetHttpPoll( INT Handle, INT* Status, INT* Received, INT* Total )
{
	return ue_proxynet_http_poll( Handle, Status, Received, Total );
}
static INT ProxyNetHttpRead( INT Handle, BYTE* Buffer, INT MaxLen )
{
	return ue_proxynet_http_read( Handle, Buffer, MaxLen );
}
static void ProxyNetHttpClose( INT Handle )
{
	ue_proxynet_http_close( Handle );
}
static FWebHttpHooks GProxyNetHttpHooks = { ProxyNetHttpOpen, ProxyNetHttpPoll, ProxyNetHttpRead, ProxyNetHttpClose };

void UProxyNetDriver::RegisterWebHttp()
{
	// Registered on the first Proxy connection and left in place: the functions are static, and Open()
	// turns away every connection that is not ours.
	GWebHttp = &GProxyNetHttpHooks;
}

void UProxyNetDriver::NormalizeConfigDefaults()
{
	guard(UProxyNetDriver::NormalizeConfigDefaults);

	// A missing SECTION is recognisable by its timeout being zero — no real ini sets that. The two
	// download switches are a bool and a size, which cannot say "unset" on their own, so they follow it:
	// without them a joined server could never send a custom map.
	const UBOOL bSectionMissing = ConnectionTimeout <= 0.f;
	if( bSectionMissing )
	{
		AllowDownloads  = 1;
		MaxDownloadSize = 268435456;
	}

	FString Patched;
	#define PROXYNET_CFG( Field, Default )										\
		if( Field <= 0 )														\
		{																		\
			Field = Default;													\
			Patched += FString::Printf( TEXT(" %s"), TEXT(#Field) );			\
		}

	// The shipped [IpDrv.TcpNetDriver] values, except the two timeouts: no game ticks run while a web
	// client streams a map it has never touched, so it needs more patience than a native one. These are
	// the CLIENT's timeouts; the SERVER's own (30 s / 200 s on a stock 3374) are not ours to set.
	PROXYNET_CFG( ConnectionTimeout,        60.f )
	PROXYNET_CFG( InitialConnectTimeout,   200.f )
	PROXYNET_CFG( KeepAliveTime,            0.2f )
	PROXYNET_CFG( RelevantTimeout,          5.0f )
	PROXYNET_CFG( SpawnPrioritySeconds,     1.0f )
	PROXYNET_CFG( ServerTravelPause,        4.0f )
	PROXYNET_CFG( MaxClientRate,          1000000 )
	PROXYNET_CFG( MaxInternetClientRate,  1000000 )
	PROXYNET_CFG( NetServerMaxTickRate,        20 )
	PROXYNET_CFG( LanServerMaxTickRate,        35 )
	#undef PROXYNET_CFG

	if( Patched.Len() )
		debugf( NAME_Log, TEXT("ProxyNetDrv: no ini value for:%s - using the stock defaults (add a [ProxyNetDrv.ProxyNetDriver] section to set them)"), *Patched );
	unguard;
}

IMPLEMENT_CLASS(UProxyNetDriver);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
