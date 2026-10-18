/*=============================================================================
	IpDrv.cpp: Unreal TCP/IP driver.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
	* Additions by Brandon Reinhart.
=============================================================================*/

#include "UnIpDrv.h"

/*-----------------------------------------------------------------------------
	AUdpLink implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(AUdpLink);

#define MAXRECVDATASIZE 4096

//
// Constructor.
//
AUdpLink::AUdpLink()
{
	guard(AUdpLink::AUdpLink);
	unguard;
}

//
// PostScriptDestroyed.
//
void AUdpLink::PostScriptDestroyed()
{
	guard(AUdpLink::PostScriptDestroyed);
	if( GetSocket() != INVALID_SOCKET )
		closesocket(GetSocket());
	Super::PostScriptDestroyed();
	unguard;
}

//
// BindPort: Binds a free port or optional port specified in argument one.
//
void AUdpLink::execBindPort( FFrame& Stack, RESULT_DECL )
{
	guard(AUdpLink::execBindPort);
	P_GET_INT_OPTX(InPort,0);
	P_GET_UBOOL_OPTX(bUseNextAvailable,0);
	P_FINISH;
	if( GIpDrvInitialized )
	{
#ifdef __EMSCRIPTEN__
		// BSD sockets don't exist on the web. Return 0 (failure) so the calling script handles it.
		debugf( NAME_DevNet, TEXT("AUdpLink::BindPort: skipped on the web (no BSD sockets)") );
		*(INT*)Result = 0;
		return;
#endif
		if( GetSocket()==INVALID_SOCKET )
		{
			Socket = socket( AF_INET, SOCK_DGRAM, IPPROTO_UDP );
			SetSocketReuseAddr( GetSocket() );
			if( GetSocket() != INVALID_SOCKET )
			{
				UBOOL TrueBuffer=1;
				if( setsockopt( GetSocket(), SOL_SOCKET, SO_BROADCAST, (char*)&TrueBuffer, sizeof(TrueBuffer) )==0 )
				{
					sockaddr_in Addr;
					Addr.sin_family      = AF_INET;
					Addr.sin_addr        = getlocalbindaddr( Stack );
					Addr.sin_port        = htons(InPort);
					INT boundport = bindnextport( Socket, &Addr, bUseNextAvailable ? 20 : 1, 1 );
					if( boundport )
					{
						if( SetNonBlocking( Socket ) )
						{
							// Success.
							*(INT*)Result = boundport;
							Port = ntohs( Addr.sin_port );
							RecvBuf.Empty();
							return;
						}
						else Stack.Logf( TEXT("BindPort: ioctlsocket failed") );
					}
					else Stack.Logf( TEXT("BindPort: bind failed") );
				}
				else Stack.Logf( TEXT("BindPort: setsockopt failed") );
			}
			else Stack.Logf( TEXT("BindPort: socket failed") );
			closesocket(GetSocket());
			// Reset to INVALID_SOCKET, not 0: fd 0 is stdin, and PostScriptDestroyed's close-guard
			// tests `!= INVALID_SOCKET`, so a 0 sentinel would make it closesocket(0) later.
			GetSocket()=INVALID_SOCKET;
		}
		else Stack.Logf( TEXT("BindPort: already bound") );
	}
	else Stack.Logf( TEXT("BindPort: IpDrv is not initialized") );
	*(INT*)Result = 0;
	unguard;
}

//
// Send text in a UDP packet.
//
void AUdpLink::execSendText( FFrame& Stack, RESULT_DECL )
{
	guard(AUdpLink::execSendText);
	P_GET_STRUCT(FIpAddr,IpAddr);
	P_GET_STR(Str);
	P_FINISH;
	if( GetSocket() != INVALID_SOCKET )
	{
		if( LinkMode == MODE_Line )
		{
			switch (OutLineMode)
			{
				case LMODE_auto:
				case LMODE_DOS:		Str += TEXT("\r\n"); break;
				case LMODE_UNIX:	Str += TEXT("\n"); break;
				case LMODE_MAC:		Str += TEXT("\n\r"); break;
			}
		}
		sockaddr_in Addr;
		Addr.sin_family      = AF_INET;
		Addr.sin_port        = htons(IpAddr.Port);
		Addr.sin_addr.s_addr = htonl(IpAddr.Addr);
		INT SentBytes = sendto( Socket, (char*)appToAnsi(*Str), sizeof(ANSICHAR)*Str.Len(), MSG_NOSIGNAL, (sockaddr*)&Addr, sizeof(Addr) );
		if ( SentBytes == SOCKET_ERROR )
		{
            TCHAR* error = SocketError(WSAGetLastError());
            Stack.Logf( TEXT("SentText: sendto failed: %s"), error );
			*(DWORD*)Result = 0;
			return;
		}
		else
		{
			//debugf("Sent %i bytes.", SentBytes);
		}
	}
	*(DWORD*)Result = 1;
	unguard;
}

//
// Send binary data.
//
void AUdpLink::execSendBinary( FFrame& Stack, RESULT_DECL )
{
	guard(AUdpLink::execSendBinary);
	P_GET_STRUCT(FIpAddr,IpAddr);
	P_GET_INT(Count);
	P_GET_ARRAY_REF(BYTE,B);
	P_FINISH;
	if( GetSocket() != INVALID_SOCKET )
	{
		sockaddr_in Addr;
		Addr.sin_family      = AF_INET;
		Addr.sin_port        = htons(IpAddr.Port);
		Addr.sin_addr.s_addr = htonl(IpAddr.Addr);
		if( sendto( Socket, (char*)B, Count, MSG_NOSIGNAL, (sockaddr*)&Addr, sizeof(Addr) ) == SOCKET_ERROR )
		{
			Stack.Logf( TEXT("SendBinary: sendto failed") );
			*(DWORD*)Result = 1;
			return;
		}
	}
	*(DWORD*)Result = 0;
	unguard;
}

//
// Time passes...
//
UBOOL AUdpLink::Tick( FLOAT DeltaTime, enum ELevelTick TickType )
{
	guard(AUdpLink::Tick);
	UBOOL Result = Super::Tick( DeltaTime, TickType );
	if( GetSocket() != INVALID_SOCKET )
	{
		if( ReceiveMode == RMODE_Event )
		{
			BYTE Buffer[MAXRECVDATASIZE];
			sockaddr_in FromAddr;
			SOCKLEN FromSize = sizeof(FromAddr);
			INT Count = recvfrom( GetSocket(), (char*)Buffer, ARRAY_COUNT(Buffer)-1, MSG_NOSIGNAL, (sockaddr*)&FromAddr, &FromSize );
			if( Count!=SOCKET_ERROR )
			{
				FIpAddr Addr;
				Addr.Addr = ntohl( FromAddr.sin_addr.s_addr );
				Addr.Port = ntohs( FromAddr.sin_port );
				if( LinkMode == MODE_Text )
				{
					Buffer[Count]=0;
					eventReceivedText( Addr, appFromAnsi((ANSICHAR*)Buffer) );
				}
				else if ( LinkMode == MODE_Line )
				{
					Buffer[Count]=0;
					FString fstr, SplitStr;
					switch (InLineMode)
					{
						case LMODE_DOS:		SplitStr = TEXT("\r\n"); break;
						case LMODE_auto:
						case LMODE_UNIX:	SplitStr = TEXT("\n"); break;
						case LMODE_MAC:		SplitStr = TEXT("\n\r"); break;
					}
					RecvBuf = FString::Printf(TEXT("%s%s"), *RecvBuf, appFromAnsi((ANSICHAR*)Buffer));
					while (RecvBuf.Split(SplitStr, &fstr, &RecvBuf))
					{
						if (InLineMode == LMODE_auto)
						{
							if (fstr.Len() > 0 && fstr[fstr.Len()-1] == '\r') // DOS fix
								fstr = fstr.Left(fstr.Len()-1);
							if (RecvBuf.Len() > 0 && RecvBuf[0] == '\r') // MAC fix
								RecvBuf = RecvBuf.Mid(1);
						}
						eventReceivedLine( Addr, fstr );
					}
				}
				else if( LinkMode == MODE_Binary )
				{
					eventReceivedBinary( Addr, Count, (BYTE*)Buffer );
				}
			}
		}
		else if( ReceiveMode == RMODE_Manual )
		{
			fd_set SocketSet;
			TIMEVAL SelectTime = {0, 0};
			INT Error;

			FD_ZERO( &SocketSet );
			FD_SET( Socket, &SocketSet );
			Error = select( Socket + 1, &SocketSet, 0, 0, &SelectTime);
			if( Error==0 || Error==SOCKET_ERROR )
			{
				DataPending = 0;
			}
			else
			{
				DataPending = 1;
			}
		}
	}

	return Result;
	unguard;
}

//
// Read text.
//
void AUdpLink::execReadText( FFrame& Stack, RESULT_DECL )
{
	guard(AUdpLink::execReadText);
	P_GET_STRUCT_REF( FIpAddr, Addr );
	P_GET_STR_REF( Str );
	P_FINISH;
	*Str = TEXT("");
	if( GetSocket() != INVALID_SOCKET )
	{
		BYTE Buffer[MAXRECVDATASIZE];
		sockaddr_in FromAddr;
		SOCKLEN FromSize = sizeof(FromAddr);
		INT BytesReceived = recvfrom( (SOCKET)Socket, (char*)Buffer, sizeof(Buffer), MSG_NOSIGNAL, (sockaddr*)&FromAddr, &FromSize );
		if( BytesReceived != SOCKET_ERROR )
		{
			Addr->Addr = ntohl( FromAddr.sin_addr.s_addr );
			Addr->Port = ntohs( FromAddr.sin_port );
			*Str = appFromAnsi((ANSICHAR*)Buffer);
			*(DWORD*)Result = BytesReceived;
		}
		else
		{
			*(DWORD*) Result = 0;
			if ( WSAGetLastError() != WSAEWOULDBLOCK )
				debugf( NAME_Log, TEXT("ReadText: Error reading text.") );
			return;
		}
		return;
	}
	*(DWORD*)Result = 0;

	unguardexec;
}

//
// Read Binary.
//
void AUdpLink::execReadBinary( FFrame& Stack, RESULT_DECL )
{
	guard(AUdpLink::execReadBinary);
	P_GET_STRUCT_REF(FIpAddr, Addr);
	P_GET_INT(Count);
	P_GET_ARRAY_REF(BYTE,B);
	P_FINISH;
	if( GetSocket() != INVALID_SOCKET )
	{
		sockaddr_in FromAddr;
		SOCKLEN FromSize = sizeof(FromAddr);
		INT BytesReceived = recvfrom( (SOCKET) Socket, (char*)B, Min(Count,255), MSG_NOSIGNAL, (sockaddr*)&FromAddr, &FromSize );
		if( BytesReceived != SOCKET_ERROR )
		{
			Addr->Addr = ntohl( FromAddr.sin_addr.s_addr );
			Addr->Port = ntohs( FromAddr.sin_port );
			*(DWORD*) Result = BytesReceived;
		}
		else
		{
			*(DWORD*)Result = 0;
			if( WSAGetLastError() != WSAEWOULDBLOCK )
				debugf( NAME_Log, TEXT("ReadBinary: Error reading text.") );
			return;
		}
		return;
	}
	*(DWORD*)Result = 0;

	unguardexec;
}

//
// Return the UdpLink's socket.  For master server NAT socket opening purposes.
//
FSocketData AUdpLink::GetSocketData()
{
	FSocketData Result;
	Result.Socket = GetSocket();
	Result.UpdateFromSocket();
	return Result;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

