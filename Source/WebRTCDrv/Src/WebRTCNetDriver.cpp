/*=============================================================================
	WebRTCNetDriver.cpp: UWebRTCNetDriver, UE2 packets over the host's WebRTC
	data channels. Modelled on IpDrv's TcpNetDriver.cpp.
	Copyright 2026 UT2004 Web Port Project. All Rights Reserved.
=============================================================================*/

#include "Engine.h"
#include "UnNet.h"
#include "WebRTCDrv.h"

// Room lookup for a join that came from the server browser (WebRTCDiscovery.cpp).
extern FString WebRTCDiscovery_RoomForHost( const TCHAR* Host );

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// Engine pointer (set in SDLLaunch.cpp after InitEngine). Used to reach the
// current GameInfo for the listen-server status push.
extern UEngine* GEngineForExec;
#endif

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// The connection states (UE_NET_STATE_*) come from vendor/glue/ue_abi.h.

// Maximum packet size for recv polling.
#define WEBRTC_MAX_PACKET       576

// The client has exactly one transport slot: the glue hands every client connect the same conn id (0), so
// only one client driver can own the wire at a time. Tracked here so a second InitConnect can take the
// slot cleanly instead of sharing it. Server-side ids are per peer and unaffected.
static UWebRTCNetDriver* GClientDriver = NULL;

/*-----------------------------------------------------------------------------
	UWebRTCNetDriver.
-----------------------------------------------------------------------------*/

void UWebRTCNetDriver::StaticConstructor()
{
	guard(UWebRTCNetDriver::StaticConstructor);
	new(GetClass(),TEXT("ConnectionTimeout"),		RF_Public)UFloatProperty(CPP_PROPERTY(ConnectionTimeout      ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("InitialConnectTimeout"),	RF_Public)UFloatProperty(CPP_PROPERTY(InitialConnectTimeout  ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("MaxClientRate"),			RF_Public)UIntProperty  (CPP_PROPERTY(MaxClientRate           ), TEXT("Client"), CPF_Config );
	new(GetClass(),TEXT("NetServerMaxTickRate"),		RF_Public)UIntProperty  (CPP_PROPERTY(NetServerMaxTickRate    ), TEXT("Client"), CPF_Config );
	unguard;
}

// Read and drop every datagram queued on the client slot (glue conn 0).
static void DiscardQueuedClientData()
{
	BYTE Data[WEBRTC_MAX_PACKET];
	INT Discarded = 0;
	while( ue_net_recv( 0, Data, sizeof(Data) ) > 0 )
		Discarded++;
	if( Discarded )
		debugf( NAME_DevNet, TEXT("[WebRTC] InitConnect: discarded %i datagram(s) queued for the previous client connection"), Discarded );
}

UBOOL UWebRTCNetDriver::InitConnect( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error )
{
	guard(UWebRTCNetDriver::InitConnect);
	debugf( NAME_DevNet, TEXT("[WebRTC] InitConnect: URL='%s' Host='%s' Port=%d Map='%s'"), *ConnectURL.String(), *ConnectURL.Host, ConnectURL.Port, *ConnectURL.Map );

	if( !Super::InitConnect( InNotify, ConnectURL, Error ) )
	{
		debugf( NAME_DevNet, TEXT("[WebRTC] InitConnect: Super::InitConnect FAILED: %s"), *Error );
		return 0;
	}

	// Fill the CPF_Config properties an ini without this class's section leaves at zero (see the
	// normaliser); RelevantTimeout=0 would close a guest's own PlayerController channel within a tick.
	NormalizeConfigDefaults( TEXT("InitConnect") );

	// FURL.Host carries the room name.
	RoomName = ConnectURL.Host;
	bIsListenServer = 0;

	// ...unless the join came from the server browser, in which case Host is the synthetic address the
	// discovery rows carry (10.199.0.N) rather than the room. It has to be an address: the stock join
	// builds `unreal://<IP>:<Port>` and FURL only sees a host in a dotted token (a bare room name would
	// be parsed as a map). See WebRTCDiscovery.cpp.
	if( RoomName.Len() )
	{
		FString FromBrowser = WebRTCDiscovery_RoomForHost( *RoomName );
		if( FromBrowser.Len() )
		{
			debugf( NAME_DevNet, TEXT("[WebRTC] InitConnect: browser address %s -> room '%s'"), *RoomName, *FromBrowser );
			RoomName = FromBrowser;
		}
	}
	if( !RoomName.Len() )
	{
		debugf( NAME_DevNet, TEXT("[WebRTC] WARNING: Empty room name in ConnectURL, using 'default'") );
		RoomName = TEXT("default");
	}

	// Construct server peer ID from room name.
	FString ServerPeerId = FString::Printf( TEXT("ut2k4-%s-srv"), *RoomName );

	// Connect to the server peer via WebRTC.
	debugf( NAME_DevNet, TEXT("[WebRTC] InitConnect: Connecting to room '%s' (server peer '%s')"), *RoomName, *ServerPeerId );
	// Take the one client slot. A server map change calls InitConnect again while the old level is still
	// ticking and flushing; on UDP the two connections have different local ports and coexist, here they
	// would share glue conn 0, so the departing level's driver would write into the new session.
	bDetached = 0;
	if( GClientDriver && GClientDriver!=this && GClientDriver->GetServerConnection() )
	{
		debugf( NAME_DevNet, TEXT("[WebRTC] InitConnect: detaching the previous client connection (one transport slot)") );
		// Detached, not closed: marking it USOCK_Closed reads as "connection lost" and aborts the travel.
		((UWebRTCConnection*)GClientDriver->GetServerConnection())->WebRTCConnId = -1;
		// ...and its LowLevelDestroy must not call ue_net_destroy(): that transport is now this driver's.
		GClientDriver->bDetached = 1;
	}
	GClientDriver = this;
	ue_net_connect( TCHAR_TO_ANSI(*ServerPeerId) );
	// The new session's pipe opens later, so every datagram already queued on the client slot belongs to an
	// earlier connection; the pending connection must not parse it as its own.
	DiscardQueuedClientData();

	// Create server connection in pending state.
	ServerConnection = new UWebRTCConnection( 0, this, USOCK_Pending, 1, ConnectURL );
	debugf( NAME_DevNet, TEXT("[WebRTC] InitConnect: Created server connection connId=0, rate=%i, MaxPacket=%i"), ServerConnection->CurrentNetSpeed, ServerConnection->MaxPacket );

	// Create channel zero (control channel).
	GetServerConnection()->CreateChannel( CHTYPE_Control, 1, 0 );
	debugf( NAME_DevNet, TEXT("[WebRTC] InitConnect: Control channel created, waiting for WebRTC connection...") );

	return 1;
	unguard;
}

UBOOL UWebRTCNetDriver::InitListen( FNetworkNotify* InNotify, FURL& LocalURL, FString& Error )
{
	guard(UWebRTCNetDriver::InitListen);
	debugf( NAME_DevNet, TEXT("[WebRTC] InitListen: URL='%s' Map='%s'"), *LocalURL.String(), *LocalURL.Map );

	if( !Super::InitListen( InNotify, LocalURL, Error ) )
	{
		debugf( NAME_DevNet, TEXT("[WebRTC] InitListen: Super::InitListen FAILED: %s"), *Error );
		return 0;
	}

	// The listen side needs the same substitution (it is the side that closes the guest's channels).
	NormalizeConfigDefaults( TEXT("InitListen") );

	// The room name comes from the travel URL options.
	RoomName = LocalURL.GetOption( TEXT("room="), TEXT("default") );

	// Listen server identity for browser discovery. ?ServerName= overrides; the
	// status push falls back to GameReplicationInfo.ServerName when this is empty.
	ServerName = LocalURL.GetOption( TEXT("ServerName="), TEXT("") );
	bIsListenServer = 1;

	// When the URL does not name the server, ask our own config. A room holds several servers, and the
	// transport identifies one as `<room>_<slug of the server name>`, so the name is half of the address
	// other players dial. The stock in-game host menu sends a travel URL with neither `room=` nor
	// `ServerName=`. `[Engine.GameReplicationInfo] ServerName` is a `globalconfig`, so GConfig holds it
	// (from the shipped ini, the launcher's `-ini:` seed, or an in-game rename); GameReplicationInfo
	// itself does not exist yet here (the level is not up), which is why this reads the config rather
	// than the object the status push in Tick uses. The engine is the source of truth for the name.
	if( !ServerName.Len() )
		GConfig->GetString( TEXT("Engine.GameReplicationInfo"), TEXT("ServerName"), ServerName );

	// Start listening for WebRTC peer connections.
	debugf( NAME_DevNet, TEXT("[WebRTC] InitListen: Starting listen server on room '%s' name '%s'"), *RoomName, *ServerName );
	// `room/serverName` — the transport slugs the name and builds `<prefix>-<room>_<slug>-srv`. The
	// separator is a `/` because a server name may contain anything, including the `_` the peer id uses.
	FString ListenKey = ServerName.Len() ? (RoomName + TEXT("/") + ServerName) : RoomName;
	ue_net_init_listen( TCHAR_TO_ANSI(*ListenKey) );
	debugf( NAME_DevNet, TEXT("[WebRTC] InitListen: Listen server started, waiting for PeerJS peer open callback...") );

	return 1;
	unguard;
}

/*-----------------------------------------------------------------------------
	Config defaults.

	UNetDriver registers nine CPF_Config properties and its constructor assigns only two of them; the
	rest come from `[<Package>.<Class>]` in the ini. An ini without a `[WebRTCDrv.WebRTCNetDriver]`
	section (a retail one has `[IpDrv.TcpNetDriver]` only) would leave every unassigned one at zero,
	and the zeroes are wrong:

	  RelevantTimeout=0      => the keep-alive clause in ULevel::ServerTickClients
	                           (`Time - Channel->RelevantTime < RelevantTimeout`) can never be true, so
	                           a channel survives only on the tick its relevance is re-evaluated (once a
	                           second): a listen server closes the guest's own xPlayer channel within a
	                           tick of opening it, and bAlwaysRelevant actors (the GameReplicationInfo)
	                           the same way; the guest never joins.
	  SpawnPrioritySeconds=0 => newly spawned actors get no priority boost.
	  ServerTravelPause=0    => no pause before server travel.
	  KeepAliveTime=0        => no keep-alives on an idle connection.
	  MaxClientRate=0        => every client's rate clamps to zero.
	  NetServerMaxTickRate=0 => no server tick pacing.

	So the whole set is normalised from the values the game ships in `[IpDrv.TcpNetDriver]`, and
	each substitution is logged.
-----------------------------------------------------------------------------*/
void UWebRTCNetDriver::NormalizeConfigDefaults( const TCHAR* Context )
{
	guard(UWebRTCNetDriver::NormalizeConfigDefaults);

	FString Patched;
	#define UFRONT_NETCFG( Field, Default )										\
		if( Field <= 0 )														\
		{																		\
			Field = Default;													\
			Patched += FString::Printf( TEXT(" %s=%s"), TEXT(#Field), *FString::Printf(TEXT("%g"), (FLOAT)(Default)) );	\
		}

	// Web-specific: the WebRTC handshake plus Emscripten's synchronous asset loading dwarf a TCP connect,
	// and no game ticks run while a map streams.
	UFRONT_NETCFG( InitialConnectTimeout, 600.f )
	UFRONT_NETCFG( ConnectionTimeout,     300.f )
	// Stock UT2004 values, straight from the shipped [IpDrv.TcpNetDriver] section.
	UFRONT_NETCFG( KeepAliveTime,           0.2f )
	UFRONT_NETCFG( RelevantTimeout,         5.0f )
	UFRONT_NETCFG( SpawnPrioritySeconds,    1.0f )
	UFRONT_NETCFG( ServerTravelPause,       4.0f )
	UFRONT_NETCFG( MaxClientRate,          15000 )
	UFRONT_NETCFG( MaxInternetClientRate,  10000 )
	UFRONT_NETCFG( NetServerMaxTickRate,      20 )
	#undef UFRONT_NETCFG

	debugf( NAME_Log, TEXT("[WebRTC] %s: net config resolved — ConnectionTimeout=%.1f InitialConnectTimeout=%.1f KeepAliveTime=%.2f RelevantTimeout=%.1f SpawnPrioritySeconds=%.1f ServerTravelPause=%.1f MaxClientRate=%i MaxInternetClientRate=%i NetServerMaxTickRate=%i%s"),
		Context, ConnectionTimeout, InitialConnectTimeout, KeepAliveTime, RelevantTimeout,
		SpawnPrioritySeconds, ServerTravelPause, MaxClientRate, MaxInternetClientRate, NetServerMaxTickRate,
		Patched.Len() ? *(FString(TEXT("  [substituted:")) + Patched + TEXT(" ]")) : TEXT("  [all from ini]") );

	unguard;
}

/*-----------------------------------------------------------------------------
	Receive-traffic log accumulator.

	One summary per window per direction instead of a line per tick that carried traffic (the browser
	console keeps a bounded ring, and a flood truncates the diagnostics either side of it). Silence is
	reported too, once per quiet stretch: a host showing In=0 while it sends is the shape of a
	half-open pipe.
-----------------------------------------------------------------------------*/

#define UFRONT_NET_LOG_WINDOW 10.0	/* seconds */
#define UFRONT_NET_MAX_CONNS  64

struct FUfrontRecvWindow
{
	DOUBLE	Start;
	INT		Packets;
	INT		Bytes;
	INT		Ticks;		// ticks that carried at least one packet — shows burstiness
	UBOOL	SilenceReported;
	FUfrontRecvWindow()
	: Start(0.0), Packets(0), Bytes(0), Ticks(0), SilenceReported(0)
	{}
};

// Server side is keyed by connId (a listen server has one per guest); the client has a single
// server connection and uses its own window.
static FUfrontRecvWindow GUfrontRecvFromClient[UFRONT_NET_MAX_CONNS];
static FUfrontRecvWindow GUfrontRecvFromServer;

// Call every tick, with (0,0) when nothing arrived: that is what lets a window close on time and what makes a
// link going quiet detectable.
static void UfrontAccumRecv( FUfrontRecvWindow& W, const char* From, INT ConnId, INT Packets, INT Bytes )
{
	DOUBLE Now = appSeconds();
	if( W.Start == 0.0 )
		W.Start = Now;
	W.Packets += Packets;
	W.Bytes   += Bytes;
	if( Packets > 0 )
		W.Ticks++;

	DOUBLE Elapsed = Now - W.Start;
	if( Elapsed < UFRONT_NET_LOG_WINDOW )
		return;

	if( W.Packets > 0 )
	{
		if( GUfrontLog )
			printf( "[WebRTC] TickDispatch: Recv from %s connId=%d — %.1fs: %d packets, %d bytes (%.1f KB/s, %.0f pkt/s, over %d ticks)\n",
				From, ConnId, Elapsed, W.Packets, W.Bytes,
				W.Bytes / 1024.0 / Elapsed, W.Packets / Elapsed, W.Ticks );
		W.SilenceReported = 0;
	}
	else if( !W.SilenceReported )
	{
		printf( "[WebRTC] TickDispatch: Recv from %s connId=%d — %.1fs: NOTHING RECEIVED\n", From, ConnId, Elapsed );
		W.SilenceReported = 1;
	}
	W.Start   = Now;
	W.Packets = 0;
	W.Bytes   = 0;
	W.Ticks   = 0;
}

void UWebRTCNetDriver::TickDispatch( FLOAT DeltaTime )
{
	guard(UWebRTCNetDriver::TickDispatch);
	Super::TickDispatch( DeltaTime );

	// Accept new incoming connections (server-side).
	//
	// ACCEPTC_Ignore is not a refusal: ULevel::NotifyAcceptingConnection returns it for every incoming
	// connection while a server travel is pending (`NextURL` set). On UDP that drops the connect packet
	// and the client retries until the new level is listening. Our transport is a pipe, and closing it
	// makes the client's glue tear the peer down and reconnect, so the two sides would livelock. So the
	// pipe is parked and re-offered on a later tick, the transport untouched. Parked ids are re-offered
	// before new arrivals, so a client that has been waiting keeps its place.
	TArray<INT> Incoming = ParkedAccepts;
	ParkedAccepts.Empty();
	INT NewConnId;
	while( (NewConnId = ue_net_accept_connection()) > 0 )
		Incoming.AddItem( NewConnId );

	for( INT i=0; i<Incoming.Num(); i++ )
	{
		INT ConnId = Incoming(i);
		// The client may have given up (or the page closed) while we were ignoring it.
		if( ue_net_get_connection_state( ConnId ) == UE_NET_STATE_CLOSED )
			continue;
		debugf( NAME_DevNet, TEXT("[WebRTC] TickDispatch: New connection available connId=%d, checking accept policy..."), ConnId );
		EAcceptConnection AcceptResult = Notify->NotifyAcceptingConnection();
		if( AcceptResult == ACCEPTC_Accept )
		{
			UWebRTCConnection* NewConn = new UWebRTCConnection( ConnId, this, USOCK_Open, 0, FURL() );
			NewConn->URL.Host = FString::Printf( TEXT("webrtc:%d"), ConnId );
			Notify->NotifyAcceptedConnection( NewConn );
			ClientConnections.AddItem( NewConn );
			debugf( NAME_DevNet, TEXT("[WebRTC] TickDispatch: ACCEPTED connection %d (total clients: %d)"), ConnId, ClientConnections.Num() );
		}
		else if( AcceptResult == ACCEPTC_Ignore )
		{
			ParkedAccepts.AddItem( ConnId );
		}
		else
		{
			debugf( NAME_DevNet, TEXT("[WebRTC] TickDispatch: REFUSED connection %d (policy=%d)"), ConnId, (INT)AcceptResult );
			ue_net_close( ConnId );
		}
	}

	// Poll data on all client connections.
	BYTE Data[WEBRTC_MAX_PACKET];
	for( INT i=0; i<ClientConnections.Num(); i++ )
	{
		UWebRTCConnection* Conn = (UWebRTCConnection*)ClientConnections(i);
		if( Conn->State == USOCK_Open || Conn->State == USOCK_Pending )
		{
			INT Size;
			INT PacketCount = 0;
			INT TotalBytes = 0;
			while( (Size = ue_net_recv( Conn->WebRTCConnId, Data, sizeof(Data) )) > 0 )
			{
				PacketCount++;
				TotalBytes += Size;
				Conn->ReceivedRawPacket( Data, Size );
			}
			if( PacketCount > 0 )
			{
				debugf( NAME_DevNetTraffic, TEXT("[WebRTC] TickDispatch: Recv from client connId=%d: %d packets, %d bytes"), Conn->WebRTCConnId, PacketCount, TotalBytes );
			}
			// Windowed summary instead of a line per tick (see the accumulator above).
			UfrontAccumRecv(
				GUfrontRecvFromClient[ (Conn->WebRTCConnId >= 0 && Conn->WebRTCConnId < UFRONT_NET_MAX_CONNS) ? Conn->WebRTCConnId : 0 ],
				"client", Conn->WebRTCConnId, PacketCount, TotalBytes );

			// Check for connection closure
			INT JSState = ue_net_get_connection_state( Conn->WebRTCConnId );
			if( JSState == UE_NET_STATE_CLOSED && Conn->State != USOCK_Closed )
			{
				// printf, like its ACCEPTED/REJECTED siblings above: NAME_DevNet does not reach the
				// browser console on this target.
				printf("[WebRTC] TickDispatch: Client connId=%d connection CLOSED by remote\n", Conn->WebRTCConnId);
				Conn->State = USOCK_Closed;
			}
		}
	}

	// Poll data on server connection (client-side).
	// A detached connection (WebRTCConnId < 0) no longer owns the slot: asking the glue about a connId we
	// gave away would report CLOSED and the departing level would be called "connection lost".
	if( GetServerConnection() && GetServerConnection()->WebRTCConnId >= 0 )
	{
		// Detect a dead transport session, but do not promote the UE2 connection to
		// USOCK_Open here: as natively, the client's server connection stays
		// USOCK_Pending through the whole download + join handshake, and only
		// HandleClientPlayer (UnConn.cpp) promotes it when the owning PlayerController
		// arrives; its gate (UnChan.cpp) requires USOCK_Pending. The transport pipe's
		// state is ue_net_get_connection_state(); the UE2 State is the engine's.
		INT JSState = ue_net_get_connection_state( GetServerConnection()->WebRTCConnId );
		if( JSState == UE_NET_STATE_CLOSED && GetServerConnection()->State != USOCK_Closed )
		{
			// Pipe dead before joining, or the session layer gave up reconnecting
			// (grace expired) — propagate the disconnect; the engine's
			// connection-lost path takes over.
			debugf( NAME_DevNet, TEXT("[WebRTC] TickDispatch: Server connection transport CLOSED (connId=%d, state=%d) — disconnecting"),
				GetServerConnection()->WebRTCConnId, (INT)GetServerConnection()->State );
			GetServerConnection()->State = USOCK_Closed;
		}

		if( GetServerConnection()->State == USOCK_Open || GetServerConnection()->State == USOCK_Pending )
		{
			INT Size;
			INT PacketCount = 0;
			INT TotalBytes = 0;
			while( (Size = ue_net_recv( GetServerConnection()->WebRTCConnId, Data, sizeof(Data) )) > 0 )
			{
				PacketCount++;
				TotalBytes += Size;
				GetServerConnection()->ReceivedRawPacket( Data, Size );
			}
			if( PacketCount > 0 )
			{
				debugf( NAME_DevNetTraffic, TEXT("[WebRTC] TickDispatch: Recv from server connId=%d: %d packets, %d bytes"), GetServerConnection()->WebRTCConnId, PacketCount, TotalBytes );
			}
			// Windowed summary instead of a line per tick (see the accumulator above).
			UfrontAccumRecv( GUfrontRecvFromServer, "server", GetServerConnection()->WebRTCConnId, PacketCount, TotalBytes );
		}
	}

#ifdef __EMSCRIPTEN__
	// Listen server: publish current status (name/map/players/gametype) so the JS
	// status responder can answer browser-discovery pings. Throttled to ~1s. The
	// name comes from ?ServerName=, else GameReplicationInfo.ServerName, else a
	// default; map/players/gametype come from the live GameInfo.
	if( bIsListenServer )
	{
		static DOUBLE GLastStatusPush = 0.0;
		DOUBLE Now = appSeconds();
		if( Now - GLastStatusPush > 1.0 )
		{
			GLastStatusPush = Now;
			FString SName = ServerName, MapName = TEXT("?"), GType = TEXT("");
			INT Players = 0, MaxPl = 0;
			UGameEngine* GE = Cast<UGameEngine>( GEngineForExec );
			if( GE && GE->GLevel )
			{
				if( GE->GLevel->URL.Map.Len() )
					MapName = GE->GLevel->URL.Map;
				ALevelInfo* LI = GE->GLevel->GetLevelInfo();
				AGameInfo* GI = LI ? LI->Game : NULL;
				if( GI )
				{
					Players = GI->NumPlayers;
					MaxPl   = GI->MaxPlayers;
					GType   = GI->GameName;
					if( !SName.Len() && GI->GameReplicationInfo )
						SName = GI->GameReplicationInfo->ServerName;
				}
			}
			if( !SName.Len() )
				SName = TEXT("UT2004 Server");
			ue_net_set_listen_status( TCHAR_TO_ANSI(*SName), TCHAR_TO_ANSI(*MapName), Players, MaxPl, TCHAR_TO_ANSI(*GType) );
		}
	}

	// Yield to the worker's event loop while a connection is being established: the WebRTC handshake
	// (SDP offer/answer, ICE candidates) needs several event-loop turns, and a heavy frame would
	// otherwise starve them past the connection timeout.
	if( !ServerConnection && ClientConnections.Num() == 0 && RoomName.Len() )
	{
		// Listen server waiting for clients
		emscripten_sleep(100);
	}
	else if( GetServerConnection() &&
	         ue_net_get_connection_state( GetServerConnection()->WebRTCConnId ) != UE_NET_STATE_OPEN )
	{
		// Client waiting for the transport pipe. Gated on the pipe state, not the UE2 connection
		// State: the connection stays USOCK_Pending through the download + join (until
		// HandleClientPlayer), and sleeping 100 ms a tick then would throttle the download.
		emscripten_sleep(100);
	}
#endif

	unguard;
}

FString UWebRTCNetDriver::LowLevelGetNetworkNumber()
{
	guard(UWebRTCNetDriver::LowLevelGetNetworkNumber);
	return RoomName.Len() ? RoomName : TEXT("webrtc");
	unguard;
}

void UWebRTCNetDriver::LowLevelDestroy()
{
	guard(UWebRTCNetDriver::LowLevelDestroy);
	debugf( NAME_Exit, TEXT("[WebRTC] LowLevelDestroy: Shutting down, room='%s', clients=%d%s"), *RoomName, ClientConnections.Num(), bDetached ? TEXT(" (detached — transport left alone)") : TEXT("") );
	// The parked pipes are about to be closed wholesale by ue_net_destroy(); just drop our list.
	ParkedAccepts.Empty();
	if( GClientDriver==this )
		GClientDriver = NULL;
	// A departing driver must not destroy the arriving driver's transport. ue_net_destroy() is global, and
	// on a client the two drivers overlap: InitConnect for the pending level runs first and the old
	// level's driver is destroyed later, inside LoadMap. On the host the old driver dies before InitListen.
	if( !bDetached )
		ue_net_destroy();
	debugf( NAME_Exit, TEXT("[WebRTC] LowLevelDestroy: Complete") );
	unguard;
}

UWebRTCConnection* UWebRTCNetDriver::GetServerConnection()
{
	return (UWebRTCConnection*)ServerConnection;
}

IMPLEMENT_CLASS(UWebRTCNetDriver);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
