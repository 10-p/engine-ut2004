/*=============================================================================
	WebRTCConnection.cpp: WebRTC connection implementation.
	Copyright 2026 UT2004 Web Port Project. All Rights Reserved.

Revision history:
	* Created for browser multiplayer networking (Phase 10).
	  Follows UTcpipConnection pattern, adapted for WebRTC data channels.
=============================================================================*/

#include "Engine.h"
#include "UnNet.h"
#include "WebRTCDrv.h"

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// Max packet size matching WINSOCK_MAX_PACKET from TcpNetDriver.cpp.
#define WEBRTC_MAX_PACKET_SIZE  512

/*-----------------------------------------------------------------------------
	UWebRTCConnection.
-----------------------------------------------------------------------------*/

UWebRTCConnection::UWebRTCConnection( INT InWebRTCConnId, UNetDriver* InDriver, EConnectionState InState, UBOOL InOpenedLocally, const FURL& InURL )
:	UNetConnection	( InDriver, InURL )
,	WebRTCConnId	( InWebRTCConnId )
,	OpenedLocally	( InOpenedLocally )
{
	guard(UWebRTCConnection::UWebRTCConnection);

	// Init the connection.
	State           = InState;
	MaxPacket       = WEBRTC_MAX_PACKET_SIZE;
	PacketOverhead  = 0;  // No IP/UDP headers over WebRTC data channels.
	InitOut();

	debugf( NAME_DevNet, TEXT("[WebRTC] Connection created: connId=%d state=%s openedLocally=%d MaxPacket=%d"),
		WebRTCConnId,
		State==USOCK_Pending ? TEXT("Pending") : State==USOCK_Open ? TEXT("Open") : TEXT("Other"),
		(INT)OpenedLocally, MaxPacket );

	unguard;
}

void UWebRTCConnection::LowLevelSend( void* Data, INT Count )
{
	guard(UWebRTCConnection::LowLevelSend);

	// Match native TcpNetDriver semantics: UTcpipConnection::LowLevelSend sends
	// REGARDLESS of the UE2 connection State (it just calls sendto()). The client's
	// server connection legitimately stays USOCK_Pending through the ENTIRE download
	// + join handshake — UE2 only promotes it to USOCK_Open from inside
	// HandleClientPlayer (UnConn.cpp), which fires when the owning PlayerController
	// arrives and whose gate (UnChan.cpp:1072) REQUIRES State==USOCK_Pending.
	//
	// The previous gate `State!=USOCK_Open` forced TickDispatch to prematurely flip
	// the connection to USOCK_Open the moment the WebRTC pipe opened (so sends would
	// work during the Pending download phase). That premature promotion is exactly
	// what made the HandleClientPlayer gate fail post-JOIN, so the guest downloaded
	// the map but never spawned. Gate only on the transport pipe being usable;
	// webrtc_send() itself drops cleanly (returns 0) until the pipe is open, after
	// which UE2's reliability layer retransmits any reliable bunches — just like UDP.
	if( State == USOCK_Closed )
	{
		debugf( NAME_DevNet, TEXT("[WebRTC] LowLevelSend on connId=%d but state=Closed, dropping %d bytes"),
			WebRTCConnId, Count );
		return;
	}

	if( Count > MaxPacket )
	{
		debugf( NAME_DevNet, TEXT("[WebRTC] ERROR: LowLevelSend packet too large: %d bytes > MaxPacket=%d on connId=%d"), Count, MaxPacket, WebRTCConnId );
	}

	clock(Driver->SendCycles);
	webrtc_send( WebRTCConnId, Data, Count );
	unclock(Driver->SendCycles);
	unguard;
}

FString UWebRTCConnection::LowLevelGetRemoteAddress()
{
	guard(UWebRTCConnection::LowLevelGetRemoteAddress);
	return FString::Printf( TEXT("webrtc:%d"), WebRTCConnId );
	unguard;
}

FString UWebRTCConnection::LowLevelDescribe()
{
	guard(UWebRTCConnection::LowLevelDescribe);
	return FString::Printf
	(
		TEXT("%s webrtc:%d state: %s"),
		*URL.Host,
		WebRTCConnId,
			State==USOCK_Pending	?	TEXT("Pending")
		:	State==USOCK_Open		?	TEXT("Open")
		:	State==USOCK_Closed		?	TEXT("Closed")
		:								TEXT("Invalid")
	);
	unguard;
}

IMPLEMENT_CLASS(UWebRTCConnection);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
