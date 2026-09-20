/*=============================================================================
	ProxyNetDrv.h: "Proxy networking" — a client-only net driver that reaches an ordinary
	NATIVE game server through a WSS<->UDP game proxy.

	Modelled on IpDrv's UTcpNetDriver / UTcpipConnection, deliberately: it takes exactly what
	the TcpIp driver takes — `open host:port`, the same URL forms, an `ip:port` remote address,
	and an ini section ([ProxyNetDrv.ProxyNetDriver]) carrying the same keys as
	[IpDrv.TcpNetDriver]. The only thing swapped is the socket: sendto/recvfrom become the
	`ue_proxynet_*` imports of the OPTIONAL proxynet ABI module (vendor/glue/ue_proxynet_abi.h).

	DETACHED BY DECISION (ufrontend docs/12 ADR-16). This package shares nothing with WebRTCDrv
	and edits none of its files. It is selected like any other driver —
	    [Engine.Engine] NetworkDevice=ProxyNetDrv.ProxyNetDriver
	— and the launcher only rewrites NetworkDevice when it is the stock IpDrv.TcpNetDriver, so
	naming this one simply works. -DUT_WITH_PROXYNET=OFF removes it entirely.

	CLIENT ONLY. A browser cannot accept UDP; hosting is what P2P networking is for.
=============================================================================*/

#ifndef _INC_PROXYNETDRV
#define _INC_PROXYNETDRV

#include "UnNet.h"

#ifdef __EMSCRIPTEN__
#include "ue_proxynet_abi.h"
#else
// A native build has real sockets and never links this package; these keep a stray include compiling.
#define PROXYNET_STATE_INVALID    0
#define PROXYNET_STATE_CONNECTING 1
#define PROXYNET_STATE_OPEN       2
#define PROXYNET_STATE_CLOSED     3
inline int  ue_proxynet_connect( const char*, int ) { return -1; }
inline int  ue_proxynet_state( int ) { return PROXYNET_STATE_CLOSED; }
inline int  ue_proxynet_send( int, const void*, int ) { return 0; }
inline int  ue_proxynet_recv( int, void*, int ) { return 0; }
inline void ue_proxynet_close( int ) {}
#endif

/*-----------------------------------------------------------------------------
	UProxyNetConnection.
-----------------------------------------------------------------------------*/

//
// One connection to a native server, by the glue's connection id.
//
class UProxyNetConnection : public UNetConnection
{
	DECLARE_CLASS(UProxyNetConnection,UNetConnection,CLASS_Config|CLASS_Transient,ProxyNetDrv)
	NO_DEFAULT_CONSTRUCTOR(UProxyNetConnection)

	// Variables.
	INT		ConnId;			// from ue_proxynet_connect(); -1 once closed
	FString	RemoteHost;		// as the player typed it — the PROXY resolves names, not the engine
	INT		RemotePort;
	DOUBLE	OpenedTime;

	// Constructors and destructors.
	UProxyNetConnection( INT InConnId, UNetDriver* InDriver, EConnectionState InState, const FURL& InURL );
	void Destroy();

	// UNetConnection interface.
	void LowLevelSend( void* Data, INT Count );
	FString LowLevelGetRemoteAddress();
	FString LowLevelDescribe();

	// Give the glue connection back (idempotent).
	void CloseTransport();
};

/*-----------------------------------------------------------------------------
	UProxyNetDriver.
-----------------------------------------------------------------------------*/

class UProxyNetDriver : public UNetDriver
{
	DECLARE_CLASS(UProxyNetDriver,UNetDriver,CLASS_Transient|CLASS_Config,ProxyNetDrv)

	// Constructor.
	UProxyNetDriver()
	{}

	// UNetDriver interface.
	UBOOL InitConnect( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error );
	UBOOL InitListen( FNetworkNotify* InNotify, FURL& LocalURL, FString& Error );
	void TickDispatch( FLOAT DeltaTime );
	FString LowLevelGetNetworkNumber();
	void LowLevelDestroy();

	// UProxyNetDriver interface.
	UProxyNetConnection* GetServerConnection();
	// A player whose saved ini predates this driver has no [ProxyNetDrv.ProxyNetDriver] section, so every
	// CPF_Config property arrives as zero — and a zero ConnectionTimeout closes the connection on its first
	// tick. Substitute the stock TcpNetDriver values for whatever is missing, and say so in the log.
	void NormalizeConfigDefaults();
};

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
