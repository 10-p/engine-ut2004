/*=============================================================================
	WebRTCNetDriver.cpp: WebRTC network driver implementation.
	Copyright 2026 UT2004 Web Port Project. All Rights Reserved.

Revision history:
	* Created for browser multiplayer networking (Phase 10).
	  Follows TcpNetDriver.cpp pattern, adapted for WebRTC data channels.
=============================================================================*/

#include "Engine.h"
#include "UnNet.h"
#include "WebRTCDrv.h"

// ufront 2.38 — room lookup for a join that came from the server browser (WebRTCDiscovery.cpp).
extern FString WebRTCDiscovery_RoomForHost( const TCHAR* Host );

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
// Engine pointer (set in SDLLaunch.cpp after InitEngine). Used to reach the
// current GameInfo for the listen-server status push, and for deferred connect.
extern UEngine* GWebRTCEngine;
#endif

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// WebRTC connection state constants (matching JS-side values).
#define WEBRTC_STATE_INVALID    0
#define WEBRTC_STATE_CONNECTING 1
#define WEBRTC_STATE_OPEN       2
#define WEBRTC_STATE_CLOSED     3

// Maximum packet size for recv polling.
#define WEBRTC_MAX_PACKET       576

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

UBOOL UWebRTCNetDriver::InitConnect( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error )
{
	guard(UWebRTCNetDriver::InitConnect);
	debugf( NAME_DevNet, TEXT("[WebRTC] InitConnect: URL='%s' Host='%s' Port=%d Map='%s'"), *ConnectURL.String(), *ConnectURL.Host, ConnectURL.Port, *ConnectURL.Map );

	if( !Super::InitConnect( InNotify, ConnectURL, Error ) )
	{
		debugf( NAME_DevNet, TEXT("[WebRTC] InitConnect: Super::InitConnect FAILED: %s"), *Error );
		return 0;
	}

	// ufront 2.36b — no ini names this class, so EVERY CPF_Config property arrives as zero unless we
	// substitute. Previously only the two timeouts below were patched here; RelevantTimeout=0 was what
	// made a guest's own PlayerController channel close within a tick of opening. See the normaliser.
	NormalizeConfigDefaults( TEXT("InitConnect") );

	// D-06: FURL.Host carries the room name.
	RoomName = ConnectURL.Host;
	bIsListenServer = 0;

	// ★ ufront 2.38 — …unless the join came from the SERVER BROWSER, in which case Host is the synthetic
	// address the discovery rows carry (10.199.0.N) rather than the room. It has to be an address: the
	// stock join builds `unreal://<IP>:<Port>` and FURL only sees a host in a dotted token, so a bare room
	// name was parsed as a MAP and the engine tried to load it from disk. See WebRTCDiscovery.cpp.
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
	webrtc_connect( TCHAR_TO_ANSI(*ServerPeerId) );

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

	// ufront 2.36b — see InitConnect; the listen side needs the same substitution (it is the side that
	// was closing the guest's channels).
	NormalizeConfigDefaults( TEXT("InitListen") );

	// D-07: Extract room name from travel URL options.
	RoomName = LocalURL.GetOption( TEXT("room="), TEXT("default") );

	// Listen server identity for browser discovery. ?ServerName= overrides; the
	// status push falls back to GameReplicationInfo.ServerName when this is empty.
	ServerName = LocalURL.GetOption( TEXT("ServerName="), TEXT("") );
	bIsListenServer = 1;

	// Start listening for WebRTC peer connections.
	debugf( NAME_DevNet, TEXT("[WebRTC] InitListen: Starting listen server on room '%s'"), *RoomName );
	webrtc_init_listen( TCHAR_TO_ANSI(*RoomName) );
	debugf( NAME_DevNet, TEXT("[WebRTC] InitListen: Listen server started, waiting for PeerJS peer open callback...") );

	return 1;
	unguard;
}

/*-----------------------------------------------------------------------------
	Config defaults (ufront 2.36b).

	★ THE SAME DISEASE AS 2.35's COMPILED CLIENT DEFAULTS. `UNetDriver` registers nine CPF_Config
	properties and its constructor assigns only two of them; the rest arrive from
	`[<Package>.<Class>]` in the ini. Our class is `WebRTCDrv.WebRTCNetDriver`, which no 2004-era ini
	has ever heard of — the shipped section is `[IpDrv.TcpNetDriver]` — so every unassigned one is a
	zero-initialised default, and the zeroes are not conservative, they are WRONG:

	  RelevantTimeout=0      ⇒ the keep-alive clause in ULevel::ServerTickClients
	                           (`Time - Channel->RelevantTime < RelevantTimeout`) can never be true, so
	                           a channel survives only on the tick its relevance is re-evaluated — and
	                           that check is itself gated to once a second. Measured: a listen server
	                           closed the guest's own xPlayer channel within a tick of opening it, over
	                           and over, so the guest received a PlayerController, lost it, received
	                           another — never joining, stuck on the loading screen forever. It closed
	                           bAlwaysRelevant actors (the GameReplicationInfo) the same way.
	  SpawnPrioritySeconds=0 ⇒ newly spawned actors get no priority boost.
	  ServerTravelPause=0    ⇒ no pause before server travel.
	  KeepAliveTime=0        ⇒ no keep-alives on an idle connection.
	  MaxClientRate=0        ⇒ every client's rate clamps to zero.
	  NetServerMaxTickRate=0 ⇒ no server tick pacing.

	The previous author hit exactly this and patched the two timeouts they were bitten by (the comment
	they left says the ini values "are 0"); the other seven stayed at zero and cost this step three
	rounds of debugging. So normalise the WHOLE set, from the values the game itself ships in
	`[IpDrv.TcpNetDriver]`, and SAY so — 2.35's other lesson was that the wrong values survived because
	nothing ever printed them.
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

	// Web-specific (kept): the WebRTC handshake plus Emscripten's synchronous asset loading dwarf a
	// TCP connect, and no game ticks run while a map streams.
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
	Receive-traffic log accumulator (ufront 2.36b).

	The per-tick "Recv from …: N packets, M bytes" line printed on EVERY tick that carried traffic —
	tens of lines a second. That is not merely noisy: the browser console keeps a bounded ring, so the
	flood TRUNCATES the diagnostics either side of it, which is exactly how a channel closing under the
	guest's own PlayerController stayed invisible for three rounds of debugging.

	So: accumulate, and emit one summary per window per direction. Silence is reported too — but only
	ONCE per quiet stretch, so a link that goes dead says so and then stops talking. "Nothing received"
	is a real diagnostic here: a host showing In=0 while it happily sends is the shape of a half-open
	pipe.
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

// Call EVERY tick, with (0,0) when nothing arrived — that is what lets a window close on time and
// what makes a link going quiet detectable.
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
	// ★ ACCEPTC_Ignore IS NOT A REFUSAL. ULevel::NotifyAcceptingConnection returns it for every incoming
	// connection while a server travel is pending (`NextURL` set) — i.e. for the whole travel window. On
	// UDP that means "drop this connect packet on the floor": nothing is torn down, the client keeps
	// retrying and gets in once the new level is listening.
	//
	// Our transport is a PIPE, and closing it is a loud, visible event: the client's glue sees the data
	// channel die, tears the peer down, reconnects, is ignored again, and the two sides livelock.
	// Measured on the sibling UE1 engine during a coop travel: 306 connect/close rounds inside a ~6s
	// pause, after which the guest never rejoined at all.
	//
	// So park the pipe and re-offer it on a later tick, leaving the transport untouched — the
	// pipe-oriented equivalent of dropping the packet. ⚠ Parked ids are re-offered BEFORE new arrivals,
	// or a client that has been waiting loses its place to a newer one.
	TArray<INT> Incoming = ParkedAccepts;
	ParkedAccepts.Empty();
	INT NewConnId;
	while( (NewConnId = webrtc_accept_connection()) > 0 )
		Incoming.AddItem( NewConnId );

	for( INT i=0; i<Incoming.Num(); i++ )
	{
		INT ConnId = Incoming(i);
		// The client may have given up (or the page closed) while we were ignoring it.
		if( webrtc_get_connection_state( ConnId ) == WEBRTC_STATE_CLOSED )
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
			webrtc_close( ConnId );
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
			while( (Size = webrtc_recv( Conn->WebRTCConnId, Data, sizeof(Data) )) > 0 )
			{
				PacketCount++;
				TotalBytes += Size;
				Conn->ReceivedRawPacket( Data, Size );
			}
			if( PacketCount > 0 )
			{
				debugf( NAME_DevNetTraffic, TEXT("[WebRTC] TickDispatch: Recv from client connId=%d: %d packets, %d bytes"), Conn->WebRTCConnId, PacketCount, TotalBytes );
			}
			// ufront 2.36b — windowed summary instead of a line per tick (see the accumulator above).
			UfrontAccumRecv(
				GUfrontRecvFromClient[ (Conn->WebRTCConnId >= 0 && Conn->WebRTCConnId < UFRONT_NET_MAX_CONNS) ? Conn->WebRTCConnId : 0 ],
				"client", Conn->WebRTCConnId, PacketCount, TotalBytes );

			// Check for connection closure
			INT JSState = webrtc_get_connection_state( Conn->WebRTCConnId );
			if( JSState == WEBRTC_STATE_CLOSED && Conn->State != USOCK_Closed )
			{
				// ⚠ printf, like its ACCEPTED/REJECTED siblings above, NOT debugf. NAME_DevNet does not
				// reach the browser console on this target, so the debugf this used to be was invisible:
				// a departed guest was reaped correctly and the only trace of it anywhere was the glue's
				// own watchdog line, which proves the TRANSPORT gave up rather than that the engine acted.
				// Measured 2.38 while writing the disconnect test — the milestone simply never fired.
				printf("[WebRTC] TickDispatch: Client connId=%d connection CLOSED by remote\n", Conn->WebRTCConnId);
				Conn->State = USOCK_Closed;
			}
		}
	}

	// Poll data on server connection (client-side).
	if( GetServerConnection() )
	{
		// Detect a dead transport session, but do NOT promote the UE2 connection to
		// USOCK_Open here. Native parity: the client's server connection stays
		// USOCK_Pending through the whole download + join handshake, and ONLY
		// HandleClientPlayer (UnConn.cpp) promotes it to USOCK_Open when the owning
		// PlayerController arrives. Its gate (UnChan.cpp:1072) REQUIRES USOCK_Pending,
		// so flipping to Open early here silently prevented the guest from ever
		// spawning (map downloaded, JOIN succeeded, but HandleClientPlayer never ran).
		// The transport pipe being open is tracked separately via
		// webrtc_get_connection_state(); UE2 State is left to the engine's join flow.
		INT JSState = webrtc_get_connection_state( GetServerConnection()->WebRTCConnId );
		if( JSState == WEBRTC_STATE_CLOSED && GetServerConnection()->State != USOCK_Closed )
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
			while( (Size = webrtc_recv( GetServerConnection()->WebRTCConnId, Data, sizeof(Data) )) > 0 )
			{
				PacketCount++;
				TotalBytes += Size;
				GetServerConnection()->ReceivedRawPacket( Data, Size );
			}
			if( PacketCount > 0 )
			{
				debugf( NAME_DevNetTraffic, TEXT("[WebRTC] TickDispatch: Recv from server connId=%d: %d packets, %d bytes"), GetServerConnection()->WebRTCConnId, PacketCount, TotalBytes );
			}
			// ufront 2.36b — windowed summary instead of a line per tick (see the accumulator above).
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
			UGameEngine* GE = Cast<UGameEngine>( GWebRTCEngine );
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
			webrtc_set_listen_status( TCHAR_TO_ANSI(*SName), TCHAR_TO_ANSI(*MapName), Players, MaxPl, TCHAR_TO_ANSI(*GType) );
		}
	}

	// Yield to the browser event loop so PeerJS can process signaling messages.
	// SwiftShader (and heavy rendering in general) blocks the main thread for
	// seconds per frame, starving the event loop. PeerJS needs multiple event
	// loop iterations to complete the WebRTC handshake (SDP offer/answer, ICE
	// candidate exchange). Without this yield, the handshake can't complete
	// before the connection timeout expires.
	// Only yield when waiting for a connection to be established.
	if( !ServerConnection && ClientConnections.Num() == 0 && RoomName.Len() )
	{
		// Listen server waiting for clients
		emscripten_sleep(100);
	}
	else if( GetServerConnection() &&
	         webrtc_get_connection_state( GetServerConnection()->WebRTCConnId ) != WEBRTC_STATE_OPEN )
	{
		// Client waiting for the transport PIPE to establish. Gate on the pipe state,
		// NOT the UE2 connection State: the connection now correctly stays
		// USOCK_Pending all the way through the download + join (until
		// HandleClientPlayer), and we must NOT sleep 100ms/tick during that phase —
		// that would throttle the asset download to ~10 ticks/sec. Once the pipe is
		// open, data flows at full speed.
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
	debugf( NAME_Exit, TEXT("[WebRTC] LowLevelDestroy: Shutting down, room='%s', clients=%d"), *RoomName, ClientConnections.Num() );
	// The parked pipes are about to be closed wholesale by webrtc_destroy(); just drop our list.
	ParkedAccepts.Empty();
	webrtc_destroy();
	debugf( NAME_Exit, TEXT("[WebRTC] LowLevelDestroy: Complete") );
	unguard;
}

UWebRTCConnection* UWebRTCNetDriver::GetServerConnection()
{
	return (UWebRTCConnection*)ServerConnection;
}

IMPLEMENT_CLASS(UWebRTCNetDriver);

#ifdef __EMSCRIPTEN__

// Declared in SDLLaunch.cpp — engine pointer for deferred connect.
extern UEngine* GWebRTCEngine;

// C-callable function for deferred client connect from JS.
// Called by shell.html setTimeout after the engine boots and game loop ticks.
// Reads the room name from WebRTCNet.pendingConnect (set by JS), then
// executes "open roomname?connect" via UEngine::Exec.
extern "C" int EMSCRIPTEN_KEEPALIVE webrtc_deferred_open()
{
	guard(webrtc_deferred_open);

	// Read the pending connect room from JS WITHOUT clearing it. Engine init on
	// Emscripten is partly asynchronous (package file opens yield via Asyncify),
	// so GWebRTCEngine may not be assigned yet when shell.html first polls. Keep
	// the room name around so the next poll can retry — only clear it once the
	// connect has actually been issued.
	char roomBuf[128] = {0};
	EM_ASM({
		var room = '';
		if (typeof WebRTCNet !== 'undefined' && WebRTCNet.pendingConnect) {
			room = WebRTCNet.pendingConnect;
		}
		stringToUTF8(room, $0, $1);
	}, roomBuf, sizeof(roomBuf));

	if( roomBuf[0] == 0 )
	{
		// No room published yet (WebRTCNet not ready, or already connected). Retry.
		return 0;
	}

	if( !GWebRTCEngine )
	{
		printf("[WebRTC] webrtc_deferred_open: engine not ready yet, will retry\n");
		return 0;
	}

	FString Room = ANSI_TO_TCHAR(roomBuf);
	FString Cmd = FString::Printf( TEXT("open %s?connect"), *Room );
	printf("[WebRTC] webrtc_deferred_open: calling Engine->Exec('%s')\n", TCHAR_TO_ANSI(*Cmd));
	GWebRTCEngine->Exec( *Cmd, *GLog );
	printf("[WebRTC] webrtc_deferred_open: Exec returned\n");

	// Connect issued — clear the pending room so JS stops polling.
	EM_ASM({
		if (typeof WebRTCNet !== 'undefined') WebRTCNet.pendingConnect = null;
	});
	return 1;

	unguard;
}
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
