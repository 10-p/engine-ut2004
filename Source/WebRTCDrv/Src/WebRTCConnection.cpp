/*=============================================================================
	WebRTCConnection.cpp: UWebRTCConnection, one WebRTC data channel as a
	UNetConnection. Modelled on UTcpipConnection.
	Copyright 2026 UT2004 Web Port Project. All Rights Reserved.
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

	// As UTcpipConnection::LowLevelSend, send regardless of the UE2 connection State: the client's
	// server connection stays USOCK_Pending through the entire download + join handshake, and only
	// HandleClientPlayer (UnConn.cpp) promotes it to USOCK_Open, behind a gate (UnChan.cpp) that
	// requires USOCK_Pending. Gate only on the transport: webrtc_send() drops cleanly (returns 0)
	// until the pipe is open, and UE2's reliability layer retransmits reliable bunches, as over UDP.
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
	// WebRTCConnId < 0: this connection has been detached from the transport (a newer client driver took
	// the one client slot; UWebRTCNetDriver::InitConnect). Its traffic is dropped rather than posted into
	// another session.
	if( WebRTCConnId >= 0 )
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
