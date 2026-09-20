/*=============================================================================
	WebHttp.h: how UHTTPDownload fetches a file in a BROWSER build.

	A web build has no sockets (under -sWASMFS every socket syscall is -ENOSYS), so on
	__EMSCRIPTEN__ UHTTPDownload does not drive a TCP connection: it hands the URL to whoever
	registered GWebHttp and polls that for bytes.

	IpDrv imports NOTHING from JavaScript for this. The table is generic; the package that owns a
	transport able to fetch registers it (today: ProxyNetDrv, whose game proxy fetches redirect
	downloads for a live session). So IpDrv has no link dependency on anybody's shim, and removing
	that package leaves this table empty rather than the build broken.

	NO HOOK = THE DOWNLOAD FAILS AT ONCE, and the engine moves on to the next download manager the
	server named (Engine.ChannelDownload) — exactly what a web build did before this table existed,
	minus the doomed socket() call and, for a host NAME, the doomed resolver thread.
=============================================================================*/

#ifndef _INC_WEBHTTP
#define _INC_WEBHTTP

class UNetConnection;

// What Poll() reports.
enum EWebHttpStatus
{
	WEBHTTP_Pending		=0,	// no response yet
	WEBHTTP_Receiving	=1,	// headers in; Total is the body size, or -1 when the far end did not say
	WEBHTTP_Done		=2,	// the whole body has been READ
	WEBHTTP_Failed		=3,	// refused / unreachable / cut short — fall back to the next download manager
};

struct FWebHttpHooks
{
	// Start fetching Url for a download that belongs to Connection.
	// Returns a handle >= 0, or -1: not a connection of mine, or fetching is not available.
	INT  (*Open )( UNetConnection* Connection, const ANSICHAR* Url );
	// Returns 0 and fills the three values (an EWebHttpStatus, body bytes handed over, body size or -1),
	// or -1 when there is no such handle.
	INT  (*Poll )( INT Handle, INT* Status, INT* Received, INT* Total );
	// Copies up to MaxLen body bytes that have arrived; returns the count (0 = nothing waiting).
	INT  (*Read )( INT Handle, BYTE* Buffer, INT MaxLen );
	void (*Close)( INT Handle );
};

// NULL until somebody registers. Defined in HTTPDownload.cpp, on __EMSCRIPTEN__ only.
extern FWebHttpHooks* GWebHttp;

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
