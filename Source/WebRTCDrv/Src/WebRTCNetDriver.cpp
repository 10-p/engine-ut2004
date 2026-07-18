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

	// Set default timeouts — WebRTC handshake (PeerJS signaling + ICE) takes
	// longer than TCP connect. Without these defaults, the INI-loaded values
	// are 0 (no [WebRTCDrv.WebRTCNetDriver] section in Default.ini) and the
	// connection times out immediately on the first tick.
	//
	// ConnectionTimeout must be high (300s) because Emscripten map loading
	// downloads assets via synchronous XHR, which can take 30-120+ seconds.
	// During that time no game ticks run, so LastReceiveTime goes stale.
	// On first tick after map load, Time-LastReceiveTime would exceed a low
	// timeout and immediately disconnect. WebRTC/ICE handles actual connection
	// monitoring at a lower level (ICE state → disconnected/failed).
	if( InitialConnectTimeout <= 0.f )
		InitialConnectTimeout = 600.f;
	if( ConnectionTimeout <= 0.f )
		ConnectionTimeout = 300.f;

	// D-06: FURL.Host carries the room name.
	RoomName = ConnectURL.Host;
	bIsListenServer = 0;
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

	// Set default timeouts (no [WebRTCDrv.WebRTCNetDriver] INI section).
	// Must match InitConnect — 300s ConnectionTimeout for Emscripten map loading.
	if( InitialConnectTimeout <= 0.f )
		InitialConnectTimeout = 600.f;
	if( ConnectionTimeout <= 0.f )
		ConnectionTimeout = 300.f;

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

void UWebRTCNetDriver::TickDispatch( FLOAT DeltaTime )
{
	guard(UWebRTCNetDriver::TickDispatch);
	Super::TickDispatch( DeltaTime );

	// Accept new incoming connections (server-side).
	INT NewConnId;
	while( (NewConnId = webrtc_accept_connection()) > 0 )
	{
		printf("[WebRTC] TickDispatch: Accept loop got connId=%d\n", NewConnId);
		debugf( NAME_DevNet, TEXT("[WebRTC] TickDispatch: New connection available connId=%d, checking accept policy..."), NewConnId );
		EAcceptConnection AcceptResult = Notify->NotifyAcceptingConnection();
		printf("[WebRTC] TickDispatch: AcceptResult=%d (Accept=%d, Reject=%d, Ignore=%d)\n",
			(int)AcceptResult, (int)ACCEPTC_Accept, (int)ACCEPTC_Reject, (int)ACCEPTC_Ignore);
		if( AcceptResult == ACCEPTC_Accept )
		{
			UWebRTCConnection* NewConn = new UWebRTCConnection( NewConnId, this, USOCK_Open, 0, FURL() );
			NewConn->URL.Host = FString::Printf( TEXT("webrtc:%d"), NewConnId );
			Notify->NotifyAcceptedConnection( NewConn );
			ClientConnections.AddItem( NewConn );
			printf("[WebRTC] TickDispatch: ACCEPTED connection %d (total clients: %d)\n", NewConnId, ClientConnections.Num());
		}
		else
		{
			printf("[WebRTC] TickDispatch: REJECTED connection %d (policy=%d)\n", NewConnId, (INT)AcceptResult);
			webrtc_close( NewConnId );
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
				printf("[WebRTC] TickDispatch: Recv from client connId=%d: %d packets, %d bytes\n", Conn->WebRTCConnId, PacketCount, TotalBytes);
			}

			// Check for connection closure
			INT JSState = webrtc_get_connection_state( Conn->WebRTCConnId );
			if( JSState == WEBRTC_STATE_CLOSED && Conn->State != USOCK_Closed )
			{
				debugf( NAME_DevNet, TEXT("[WebRTC] TickDispatch: Client connId=%d connection CLOSED by remote"), Conn->WebRTCConnId );
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
				printf("[WebRTC] TickDispatch: Recv from server connId=%d: %d packets, %d bytes\n", GetServerConnection()->WebRTCConnId, PacketCount, TotalBytes);
			}
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
