/*=============================================================================
	WebRTCDiscovery.cpp - WebRTC server discovery for the in-game server browser.

	"Our stuff" — kept OUT of the stock IpDrv query path (MasterServerClient.cpp),
	which only keeps a thin hook that calls these and fires the stock delegates.
	A true ini-pluggable driver isn't possible (the frozen GUI2K4.u hardcodes
	Spawn(class'LANQueryClient'), and we can't compile a new script class without
	ucc), so this is the next-best separation: the JS-bridge glue + the
	FServerResponseLine building live here, in our own native package.

	FServerResponseLine is an Engine type, so this is a one-way IpDrv -> WebRTCDrv
	dependency resolved at link time (exactly like the webrtc-net.js bridges) — no
	module cycle. See [[webrtc-discovery-architecture]].
=============================================================================*/

#include "Engine.h"
#include "UnNet.h"
#include "WebRTCDrv.h"

#ifdef __EMSCRIPTEN__

// WebRTC discovery bridge (webrtc-net.js --js-library functions; WASM-link resolved).
extern "C"
{
	void webrtc_discover_servers(const char* room);
	int  webrtc_get_server_count();
	int  webrtc_is_discovery_done();
	int  webrtc_get_server_info(int index, char* name, int nameMax, char* joinKey, int joinKeyMax);
	int  webrtc_get_server_map(int index, char* buf, int maxLen);
	int  webrtc_get_server_gametype(int index, char* buf, int maxLen);
	int  webrtc_get_server_players(int index);
	int  webrtc_get_server_maxplayers(int index);
	int  webrtc_get_server_ping(int index);
}

// Kick a PeerJS peer-list query + per-server status pings ("" = all rooms).
void WebRTCDiscovery_Begin( const char* room )
{
	webrtc_discover_servers( room ? room : "" );
}

// Has the peer list + status handshakes settled?
UBOOL WebRTCDiscovery_Ready()
{
	return webrtc_is_discovery_done() != 0;
}

// Build one FServerResponseLine per discovered server (name/map/players/ping from the
// JS status handshake). IP = the room/join key (unique -> the stock browser dedupes by
// IP+Port). Returns the count appended. The IpDrv hooks fire the delegates over these.
INT WebRTCDiscovery_Drain( TArray<FServerResponseLine>& Out )
{
	const INT Count = webrtc_get_server_count();
	debugf( NAME_DevNet, TEXT("[WebRTC-Discovery] %d server(s) discovered"), Count );
	char nameBuf[256], joinBuf[256], mapBuf[128], gtBuf[128];
	for( INT i = 0; i < Count; i++ )
	{
		if( !webrtc_get_server_info( i, nameBuf, sizeof(nameBuf), joinBuf, sizeof(joinBuf) ) )
		{
			debugf( NAME_DevNet, TEXT("[WebRTC-Discovery] WARNING: failed to get info for server %d"), i );
			continue;
		}
		webrtc_get_server_map( i, mapBuf, sizeof(mapBuf) );
		webrtc_get_server_gametype( i, gtBuf, sizeof(gtBuf) );
		const INT Players = webrtc_get_server_players( i );
		const INT MaxPlrs = webrtc_get_server_maxplayers( i );
		const INT PingMs  = webrtc_get_server_ping( i );

		// Build the row IN PLACE in the array (AddZeroed = add + appMemzero the element):
		// no extra copy/destruct of an appMemzero'd struct-with-FStrings, so the copy
		// profile matches the original inline path the delegate expects.
		FServerResponseLine& Server = Out( Out.AddZeroed() );
		Server.ServerName     = ANSI_TO_TCHAR(nameBuf);
		Server.IP             = ANSI_TO_TCHAR(joinBuf);    // join key -> unreal://<room>
		Server.Port           = 7777;
		Server.QueryPort      = 7777;
		Server.MapName        = (mapBuf[0] != 0) ? FString(ANSI_TO_TCHAR(mapBuf)) : FString(TEXT("?"));
		Server.GameType       = ANSI_TO_TCHAR(gtBuf);
		Server.CurrentPlayers = Players;
		Server.MaxPlayers     = (MaxPlrs > 0) ? MaxPlrs : 16;
		Server.Ping           = (PingMs > 0) ? PingMs : 1;  // 0 -> UI bumps to 9999 "?"
		Server.Flags          = 0;
		Server.SkillLevel     = TEXT("0");
		debugf( NAME_DevNet, TEXT("[WebRTC-Discovery]   '%s' room=%s map=%s %d/%d %dms"),
			*Server.ServerName, *Server.IP, *Server.MapName, Server.CurrentPlayers, Server.MaxPlayers, Server.Ping );
	}
	return Count;
}

#endif // __EMSCRIPTEN__
