#ifndef UTVUPLINK
#define UTVUPLINK

#include "EpicInclude.h"
//#include "UnIpDrv.h"

//It would be nice if it was possible to use these as includes from ipdrv, but it's not
//Following is mostly copied from MasterServerUplink.cpp 

enum EUplinkState
{
	MSUS_WaitingChallenge		= 0,
	MSUS_WaitingApproval		= 1,
	MSUS_WaitingForUDPResponse  = 2,
	MSUS_ChannelOpen			= 3,
};

class FNATHeartbeatLink : public FUdpLink
{
	BYTE HeartbeatType;
public:
	// tors
	FNATHeartbeatLink(FSocketData InSocketData, BYTE InHeartbeatType)
	:	FUdpLink(InSocketData)
	,	HeartbeatType(InHeartbeatType)
	{}
	FNATHeartbeatLink(BYTE InHeartbeatType)
	:	HeartbeatType(InHeartbeatType)
	{}

	// FUdpLink interface.
	virtual void OnReceivedData( FIpAddr SrcAddr, BYTE* Data, INT Count )
	{}

	// FNATHeartbeatLink interface.
	FSocketData* GetSocketData()
	{
		return &SocketData;
	}
	virtual void SendHeartbeat( FIpAddr MasterServerAddr, INT Code )
	{
		FArchiveUdpWriter ArSend(this, MasterServerAddr);
		ArSend << HeartbeatType << Code;
		ArSend.Flush();
	}
};

//End of direct copy

class UTVQueryInterface : public FNATHeartbeatLink
{
public:
	UTVQueryInterface( INT InPort)
	:	FNATHeartbeatLink(HB_QueryInterface)
	{
		BindPort(InPort);
	}

	virtual void OnReceivedData( FIpAddr SrcAddr, BYTE* Data, INT Count );
	void SendPing( FArchive& Ar );
	void SendSmallPing( FArchive& Ar );
	void SendRules( FArchive& Ar );
	void SendPlayers( FArchive& Ar );
};

class UTVUplink : public FTcpLink
{
	EUplinkState	UplinkState;
	FIpAddr			RemoteHost;

	FString			MasterServerName;
	INT				MasterServerPort;
	
	INT				HeartbeatPeriod;
	
	FNATHeartbeatLink*			GameHeartbeat;			// NAT heartbeat for game protocol port
	UTVQueryInterface*		QueryInterface;			// Native query interface / NAT heartbeat.
	DOUBLE						LastRefreshTime;
public:
	UTVUplink (UBOOL connect);
	~UTVUplink();
	void TryConnect ();
	void OnResolved( FIpAddr a );
	void OnResolveFailed();
	void OnConnectionSucceeded();
	void OnConnectionFailed();
	void OnClosed();
	void OnDataReceived();
	void SendServerConfig();
	void CheckRefresh();
	void CheckRefresh(bool force);
	virtual UBOOL Poll( INT WaitTime );
};

#endif