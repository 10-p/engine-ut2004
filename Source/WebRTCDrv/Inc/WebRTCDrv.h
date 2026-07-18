/*=============================================================================
	WebRTCDrv.h: WebRTC network driver for Emscripten/browser multiplayer.
	Copyright 2026 UT2004 Web Port Project. All Rights Reserved.

Revision history:
	* Created for browser multiplayer networking (Phase 10).
=============================================================================*/

#ifndef _WEBRTCDRV_H_
#define _WEBRTCDRV_H_

// Required for UNetDriver and UNetConnection base class definitions.
// Engine.h provides the UObject hierarchy but not the networking classes.
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	Extern "C" bridge functions (WASM <-> JS boundary).
	On Emscripten, these are implemented in webrtc-net.js.
	On native, inline stubs return 0 / do nothing.
-----------------------------------------------------------------------------*/

#ifdef __EMSCRIPTEN__
extern "C"
{
	void webrtc_init_listen(const char* roomName);
	void webrtc_connect(const char* peerId);
	int  webrtc_send(int connId, const void* data, int len);
	int  webrtc_recv(int connId, void* buffer, int maxLen);
	int  webrtc_accept_connection();
	void webrtc_close(int connId);
	void webrtc_destroy();
	int  webrtc_get_connection_state(int connId);
	// Returns 1 if there's a pending deferred connect, writes room name to buffer
	int  webrtc_check_pending_connect(char* buffer, int maxLen);
	// Deferred client connect — reads room from JS, executes "open room?connect".
	// Returns 1 if the connect was issued (engine ready), 0 if not ready (retry).
	int  webrtc_deferred_open();
	// Listen server: publish current status (name/map/players/max/gametype) for
	// browser discovery; the JS status responder hands this to browsing clients.
	void webrtc_set_listen_status(const char* name, const char* map, int players, int maxPlayers, const char* gametype);
}
#else
inline void webrtc_init_listen(const char* roomName) {}
inline void webrtc_connect(const char* peerId) {}
inline int  webrtc_send(int connId, const void* data, int len) { return 0; }
inline int  webrtc_recv(int connId, void* buffer, int maxLen) { return 0; }
inline int  webrtc_accept_connection() { return 0; }
inline void webrtc_close(int connId) {}
inline void webrtc_destroy() {}
inline int  webrtc_get_connection_state(int connId) { return 0; }
inline int  webrtc_check_pending_connect(char* buffer, int maxLen) { return 0; }
inline void webrtc_set_listen_status(const char* name, const char* map, int players, int maxPlayers, const char* gametype) {}
#endif

/*-----------------------------------------------------------------------------
	UWebRTCConnection.
-----------------------------------------------------------------------------*/

//
// WebRTC data channel connection.
// Maps a JS-side connection ID to a UNetConnection.
//
class UWebRTCConnection : public UNetConnection
{
	DECLARE_CLASS(UWebRTCConnection,UNetConnection,CLASS_Config|CLASS_Transient,WebRTCDrv)
	NO_DEFAULT_CONSTRUCTOR(UWebRTCConnection)

	// Variables.
	INT			WebRTCConnId;
	UBOOL		OpenedLocally;

	// Constructors.
	UWebRTCConnection( INT InWebRTCConnId, UNetDriver* InDriver, EConnectionState InState, UBOOL InOpenedLocally, const FURL& InURL );

	// UNetConnection interface.
	void LowLevelSend( void* Data, INT Count );
	FString LowLevelGetRemoteAddress();
	FString LowLevelDescribe();
};

/*-----------------------------------------------------------------------------
	UWebRTCNetDriver.
-----------------------------------------------------------------------------*/

//
// WebRTC network driver for browser multiplayer.
// Routes UE2 packets through JavaScript WebRTC data channels.
//
class UWebRTCNetDriver : public UNetDriver
{
	DECLARE_CLASS(UWebRTCNetDriver,UNetDriver,CLASS_Transient|CLASS_Config,WebRTCDrv)

	// Variables.
	FString RoomName;
	FString ServerName;       // Listen server: display name for browser discovery (?ServerName=).
	UBOOL   bIsListenServer;  // 1 on a listen server — pushes status each tick for discovery.

	// Constructor.
	void StaticConstructor();
	UWebRTCNetDriver()
	{}

	// UNetDriver interface.
	UBOOL InitConnect( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error );
	UBOOL InitListen( FNetworkNotify* InNotify, FURL& LocalURL, FString& Error );
	void TickDispatch( FLOAT DeltaTime );
	FString LowLevelGetNetworkNumber();
	void LowLevelDestroy();

	// UWebRTCNetDriver interface.
	UWebRTCConnection* GetServerConnection();
};

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
