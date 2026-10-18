/*=============================================================================
	WebRTCDrv.h: the WebRTC network driver (web multiplayer).
	Copyright 2026 UT2004 Web Port Project. All Rights Reserved.
=============================================================================*/

#ifndef _WEBRTCDRV_H_
#define _WEBRTCDRV_H_

// Required for UNetDriver and UNetConnection base class definitions.
// Engine.h provides the UObject hierarchy but not the networking classes.
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	The wasm <-> JS boundary: the ue_net_* imports and the UE_NET_STATE_*
	values of vendor/glue/ue_abi.h, provided by the host's net glue through
	the baked forwarder (vendor/glue/net-shim.js). The package is built for
	the web only.
-----------------------------------------------------------------------------*/

#ifdef __EMSCRIPTEN__
#include "ue_abi.h"
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
	// Set when a newer client driver has taken the one client transport slot from us.
	UBOOL   bDetached;
	// Pipes the level told us to ignore (not refuse), re-offered on a later tick. See TickDispatch().
	TArray<INT> ParkedAccepts;

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
	// Substitute usable values for the CPF_Config properties an ini without a [WebRTCDrv.WebRTCNetDriver]
	// section (a retail one) leaves at zero, and log what was substituted.
	void NormalizeConfigDefaults( const TCHAR* Context );
};

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
