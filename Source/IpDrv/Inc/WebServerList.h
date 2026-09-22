/*=============================================================================
	WebServerList.h: where the in-game Internet browser gets its list in a BROWSER build.

	A web build has no sockets (under -sWASMFS every socket syscall is -ENOSYS), so on
	__EMSCRIPTEN__ AMasterServerClient cannot ask a master server. In Proxy networking mode the game
	proxy already holds that list (it polls the masters for everybody), so the client asks whoever
	registered GWebServerList instead, and answers the browser's own pings from the same rows.

	Same shape as WebHttp.h, for the same reasons: IpDrv imports NOTHING from JavaScript for this;
	the package that owns the transport (ProxyNetDrv) registers the table, so removing that package
	leaves it empty rather than the build broken.

	NO HOOK, or Active() false (P2P mode) = EXACTLY TODAY'S BEHAVIOUR: the WebRTC discovery path.
	Active() is a native check with no JavaScript behind it, so a query can decide at once; the list
	itself is only ever asked for from Poll(), never from inside a script call.
=============================================================================*/

#ifndef _INC_WEBSERVERLIST
#define _INC_WEBSERVERLIST

// What Poll() reports (the same values as WebHttp.h's, which the ABI shares).
enum EWebListStatus
{
	WEBLIST_Pending		=0,
	WEBLIST_Done		=2,	// Read() now hands over the whole list
	WEBLIST_Failed		=3,	// no proxy, or it could not be reached: the browser says "connection failed"
};

struct FWebServerListHooks
{
	// Is Proxy networking this build's mode? Native, instant, no JavaScript.
	UBOOL (*Active)();
	// Start (or reuse) a fetch of the proxy's list. A handle >= 0, or -1: no proxy to ask.
	INT  (*Open )();
	// Returns 0 and fills Status (an EWebListStatus) and Count (rows), or -1 for no such handle.
	INT  (*Poll )( INT Handle, INT* Status, INT* Count );
	// The list's size in bytes; copied only when it fits in MaxLen (a bigger return = call again).
	// The text: "#proxynet-list 1 ping=<ms>\n", then one row per line, TAB-separated: host, game port,
	// name, map, game type, players, max players, version, min version, password (0/1); -1 = unknown.
	INT  (*Read )( INT Handle, BYTE* Buffer, INT MaxLen );
	void (*Close)( INT Handle );
};

// NULL until somebody registers. Defined in MasterServerClient.cpp, on __EMSCRIPTEN__ only.
extern FWebServerListHooks* GWebServerList;

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
