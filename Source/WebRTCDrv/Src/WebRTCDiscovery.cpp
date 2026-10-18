/*=============================================================================
	WebRTCDiscovery.cpp - WebRTC server discovery for the in-game server browser.

	Kept out of the stock IpDrv query path (MasterServerClient.cpp), which only
	keeps a thin hook that calls these and fires the stock delegates. A script-level
	driver is not possible (the shipped GUI2K4.u hardcodes Spawn(class'LANQueryClient')
	and the build has no ucc to compile a new class), so the host-glue calls and the
	FServerResponseLine building live here, in this package. FServerResponseLine is an
	Engine type, so this is a one-way IpDrv -> WebRTCDrv dependency resolved at link time.
=============================================================================*/

#include "Engine.h"
#include "UnNet.h"
#include "WebRTCDrv.h"

#ifdef __EMSCRIPTEN__

// WebRTC discovery imports (the host's net glue, resolved at the wasm link).
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


/*-----------------------------------------------------------------------------
	Room <-> synthetic address.

	A discovery row carries a dotted address, not the room name, in FServerResponseLine::IP: the
	stock join (Browser_ServersList.Connect()) builds `unreal://<IP>:<Port>` and hands it to
	ClientTravel, and FURL only recognises a host in a token that contains a '.' (UnURL.cpp's
	"Parse optional host name and port" branch). A bare room name would land in URL.Map and be
	loaded as a map file. So the browser gets 10.199.0.0/16, the low 16 bits indexing an
	append-only table, translated back in UWebRTCNetDriver::InitConnect. The table must be
	append-only: a player can click JOIN minutes after the row was drawn, and the address has to
	still mean the same room.
-----------------------------------------------------------------------------*/

#define WEBRTC_LAN_ADDR_BASE  0x0AC70000u
#define WEBRTC_LAN_ADDR_MASK  0xFFFF0000u

static TArray<FString> GDiscoveredRooms;

static INT WebRTCRoomIndex( const FString& Room )
{
	for( INT i=0; i<GDiscoveredRooms.Num(); i++ )
		if( appStricmp( *GDiscoveredRooms(i), *Room )==0 )
			return i;
	// Placement new, not AddItem: `TArray<T>` is a POD container (AddItem is `Add()` then assignment),
	// and `FArray::Add` neither zeroes nor constructs the slot, so assigning an FString into it would
	// Realloc whatever garbage Data the allocator left there. `TArray::operator=` itself uses placement new.
	new( GDiscoveredRooms )FString( Room );
	return GDiscoveredRooms.Num() - 1;
}

// The dotted address that stands for this room, stable for the session.
static FString WebRTCHostForRoom( const FString& Room )
{
	const DWORD A = WEBRTC_LAN_ADDR_BASE | (DWORD)(WebRTCRoomIndex(Room) & 0xFFFF);
	return FString::Printf( TEXT("%i.%i.%i.%i"), (A>>24)&0xFF, (A>>16)&0xFF, (A>>8)&0xFF, A&0xFF );
}

// Reverse, for InitConnect. Returns "" when the host is not one of ours (a real address, or a room
// name that arrived some other way — the ?connect= path still passes the room directly).
FString WebRTCDiscovery_RoomForHost( const TCHAR* Host )
{
	if( !Host )
		return FString(TEXT(""));
	INT Oct[4] = {0,0,0,0}, N = 0;
	const TCHAR* P = Host;
	while( N < 4 )
	{
		if( *P < '0' || *P > '9' )
			return FString(TEXT(""));
		INT V = 0;
		while( *P >= '0' && *P <= '9' )
			V = V*10 + (*P++ - '0');
		if( V > 255 )
			return FString(TEXT(""));
		Oct[N++] = V;
		if( N < 4 )
		{
			if( *P != '.' )
				return FString(TEXT(""));
			P++;
		}
	}
	if( *P != 0 )
		return FString(TEXT(""));
	const DWORD Addr = ((DWORD)Oct[0]<<24) | ((DWORD)Oct[1]<<16) | ((DWORD)Oct[2]<<8) | (DWORD)Oct[3];
	if( (Addr & WEBRTC_LAN_ADDR_MASK) != WEBRTC_LAN_ADDR_BASE )
		return FString(TEXT(""));
	const INT Index = (INT)(Addr & 0xFFFF);
	if( Index < 0 || Index >= GDiscoveredRooms.Num() )
		return FString(TEXT(""));
	return GDiscoveredRooms( Index );
}
// Kick a peer-list query + per-server status pings ("" = all rooms).
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
// JS status handshake). IP = the room's synthetic address (unique, so the stock browser's
// IP+Port dedupe keeps every row). Returns the count appended. The IpDrv hooks fire the delegates over these.
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
		Server.IP             = WebRTCHostForRoom( ANSI_TO_TCHAR(joinBuf) ); // a host, not the room (see above)
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
