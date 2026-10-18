/*============================================================================
	MasterServerUplink.cpp - game server to master server uplink

	Revision history:
		* Created by Jack Porter
============================================================================*/

#include "UnIpDrv.h"
#include "UnTcpNetDriver.h"

/*-----------------------------------------------------------------------------
	FNATHeartbeatLink
-----------------------------------------------------------------------------*/

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

/*-----------------------------------------------------------------------------
	FServerQueryInterface
-----------------------------------------------------------------------------*/

class FServerQueryInterface : public FNATHeartbeatLink
{
protected:
	class FMasterServerUplinkLink* Uplink;
public:
	FServerQueryInterface( INT InPort, class FMasterServerUplinkLink* InUplink )
	:	FNATHeartbeatLink(HB_QueryInterface)
	,	Uplink(InUplink)
	{
		BindPort(InPort);
	}

	virtual void OnReceivedData( FIpAddr SrcAddr, BYTE* Data, INT Count );
	void SendPing( FArchive& Ar );
	void SendSmallPing( FArchive& Ar );
	void SendRules( FArchive& Ar );
	void SendPlayers( FArchive& Ar );
};

/*-----------------------------------------------------------------------------
	FServerQueryInterface
-----------------------------------------------------------------------------*/

class FServerLANInterface : public FServerQueryInterface
{
	FServerQueryInterface* ReplyInterface;
public:
	FServerLANInterface( INT Port, class FMasterServerUplinkLink* InUplink, FServerQueryInterface* InReplyInterface );
	virtual void OnReceivedData( FIpAddr SrcAddr, BYTE* Data, INT Count );
};


/*-----------------------------------------------------------------------------
	FMasterServerUplinkLink
-----------------------------------------------------------------------------*/

#define REFRESH_TIME 60.f

enum EMastserServerUplinkState
{
	MSUS_WaitingChallenge		= 0,
	MSUS_WaitingApproval		= 1,
	MSUS_WaitingForUDPResponse  = 2,
	MSUS_ChannelOpen			= 3,
};

class FMasterServerUplinkLink : public FTcpLink
{
	FServerQueryInterface*		QueryInterface;			// Native query interface / NAT heartbeat.
	FServerLANInterface*		LANInterface;			// LAN query interface
	FNATHeartbeatLink*			GameHeartbeat;			// NAT heartbeat for game protocol port
	FNATHeartbeatLink*			GSQueryHeartbeat;		// NAT heartbeat for gamespy query port.

	EMastserServerUplinkState	UplinkState;
	AMasterServerUplink*		Actor;
	TArray<FString>				OutstandingChallenges;	// CD Key challenge clients we're waiting for
	TArray<FString>				StatsBuffer;
	DOUBLE						LastRefreshTime;
	UBOOL						ConnectionFailed;
	UBOOL						ShouldTryReconnect;
	INT							HeartbeatPeriod;
	DOUBLE						LastHeartbeatTime;
	FIpAddr						RemoteHost;
	DWORD						QueryNatPort;
	DWORD						GameNatPort;
	DWORD						GamespyNatPort;

	FString						MasterServerName;
	INT							MasterServerPort;

	friend class FServerQueryInterface;
	friend class FServerLANInterface;
public:
	FMasterServerUplinkLink( AMasterServerUplink* InActor )
	:	FTcpLink()
	,	UplinkState(MSUS_WaitingChallenge)
	,	Actor(InActor)
	,	ConnectionFailed(0)
	,	ShouldTryReconnect(1)
	,	QueryInterface(NULL)
	,	LANInterface(NULL)
	,	GameHeartbeat(NULL)
	,	GSQueryHeartbeat(NULL)
	,	HeartbeatPeriod(0)
	,	QueryNatPort(0)
	,	GameNatPort(0)
	,	GamespyNatPort(0)
	,	MasterServerPort(0)
	{
		guard(FMasterServerUplinkLink::FMasterServerUplinkLink)

		SetLinkMode( TCPLINK_FArchive );
		LastRefreshTime = -REFRESH_TIME;
		LastHeartbeatTime = GCurrentTime;

		// Game protocol port NAT heartbeat.
		UTcpNetDriver* NetDriver = Cast<UTcpNetDriver>( Actor->XLevel->NetDriver );
		check(NetDriver);
		GameHeartbeat = new FNATHeartbeatLink( NetDriver->GetSocketData(), HB_GamePort );
		//GameHeartbeat->bConnectionBroken = false;
		// Gamespy query port NAT heartbeat.
		if( Actor->GamespyQueryLink )
			GSQueryHeartbeat = new FNATHeartbeatLink( Actor->GamespyQueryLink->GetSocketData(), HB_GamespyQueryPort );

		// Create the query interfaces
		QueryInterface = new FServerQueryInterface( NetDriver->GetSocketData().Port+1, this );
        if (Actor->LANServerPort >= 0)
		    LANInterface = new FServerLANInterface( Actor->LANServerPort, this, QueryInterface );

		unguard;
	}

	virtual ~FMasterServerUplinkLink()
	{
		guard(FMasterServerUplinkLink::~FMasterServerUplinkLink)
		delete LANInterface;
		delete QueryInterface;
		delete GameHeartbeat;
		delete GSQueryHeartbeat;
		unguard;
	}

	void TryConnect()
	{
		// restore state
		UplinkState = MSUS_WaitingChallenge;
		ConnectionFailed = 0;
		ShouldTryReconnect = 1;

		Actor->eventGetMasterServer( MasterServerName, MasterServerPort );
		Resolve( *MasterServerName );
	}

	void OnResolved( FIpAddr a )
	{
		a.Port = MasterServerPort;
		RemoteHost = a;
		GWarn->Logf(TEXT("MasterServerUplink: Resolved %s as %s."), *MasterServerName, *RemoteHost.GetString(0) );
		Connect( a );
	}

	void OnResolveFailed()
	{
		GWarn->Logf(TEXT("MasterServerUplink: Failed to resolve master server %s."), *MasterServerName );
		ConnectionFailed = 1;
		Actor->SaveConfig();
	}

	void OnConnectionSucceeded()
	{
		GWarn->Logf(TEXT("MasterServerUplink: Connection to %s established."), *MasterServerName);
	}

	void OnConnectionFailed()
	{
		GWarn->Logf(TEXT("MasterServerUplink: Uplink failed to connect to master server %s."), *MasterServerName);
		ConnectionFailed = 1;
		Actor->SaveConfig();
	}

	void OnClosed()
	{
		ConnectionFailed = 1;
	}

	void OnChannelOpened()
	{
		// Send our master MD5 revision
		SendMD5Revision();

		// Refresh MS's server state
		CheckRefresh();

		// Send any pending stats.
		if( StatsBuffer.Num() )
		{
			for( INT i=0;i<StatsBuffer.Num();i++ )
				SendStatLine( *StatsBuffer(i) );
			StatsBuffer.Empty();
		}
	}

	void SendMD5Revision()
	{
		guard(FMasterServerUplink::SendMD5Revision);
		BYTE Command = STM_MD5Version;
		
		INT MaxRevision = -1;
		UGameEngine* GE = Cast<UGameEngine>(Actor->XLevel->Engine);

		if (GE)
			MaxRevision = GE->PackageRevisionLevel();

		// Send our current maximum revision number to the master server.
		*ArSend << Command << MaxRevision;
		ArSend->Flush();
		unguard;
	}

	void OnDataReceived()
	{
		guard(FMasterServerUplinkLink::OnDataReceived);
		while( DataAvailable() )
		{
			switch( UplinkState )
			{
			case MSUS_WaitingChallenge:
				{
					guard(MSUS_WaitingChallenge);
					FString Challenge;
					FString CDKeyHash, Response, ClientType;
					INT Version = ENGINE_VERSION;
					INT MatchID = Actor->GameStats ? Actor->MatchID : -1;
					BYTE Platform = GRunningOS;
					FString Language = UObject::GetLanguage();

					*ArRecv << Challenge;

					CDKeyHash = GetCDKeyHash();
					Response = GetCDKeyResponse( *Challenge );
					ClientType = TEXT("UT2K4SERVER");

					BYTE IUTB = Actor->bIgnoreUTANBans;

					*ArSend << CDKeyHash << Response << ClientType << Version << Platform << Language << MatchID << IUTB;
					ArSend->Flush();

					UplinkState = MSUS_WaitingApproval;
					unguard;
				}
				break;
			case MSUS_WaitingApproval:
				{
					guard(MSUS_WaitingApproval);
					FString Approval;
					*ArRecv << Approval;

					GWarn->Logf(TEXT("Approval %s"),*Approval);

					if( Approval == TEXT("APPROVED") )
					{
						// Server was approved.  Send our configuration and wait for
						// command to send UDP heartbeats.
						SendServerConfig();
						UplinkState = MSUS_WaitingForUDPResponse;
					}

					// Updating the master server List

					else if( Approval == TEXT("MSUPDATE") )
					{
						Actor->ActiveMasterServerList.Empty();
						Actor->LastMSIndex=-1;

						Actor->MasterServerList.Empty();
						INT ListCnt,Port;
						FString Addr;

						*ArRecv << ListCnt;

						GWarn->Logf(TEXT("Updating Master Server List %i"),ListCnt);

						for (int i=0;i<ListCnt;i++)
						{
							*ArRecv << Addr << Port;
							INT Index = Actor->MasterServerList.AddZeroed();
							Actor->MasterServerList(Index).Address = Addr;
							Actor->MasterServerList(Index).Port = Port;
							GWarn->Logf(TEXT("    Added: %s [%i]"),*Addr,Port);
						}
						Actor->SaveConfig();
						ShouldTryReconnect = 1;
						ConnectionFailed = 1;
					}
					else if( Approval == TEXT("UPGRADE") )
					{
						INT UpgradeVersion;
						*ArRecv << UpgradeVersion;
						GWarn->Logf(TEXT("MasterServerUplink: Rejected.  Must upgrade to version %d."), UpgradeVersion);
						ShouldTryReconnect = 0;
						ConnectionFailed = 1;
					}
					else if( Approval == TEXT("DENIED") )
					{
						GWarn->Logf(TEXT("MasterServerUplink: Master server rejected authentication request."));
						GWarn->Logf(TEXT("Check your CD key."));
						if( GetCDKeyHash() == TEXT("d41d8cd98f00b204e9800998ecf8427e") )
							GWarn->Logf(TEXT("Your CD key appears to be blank.  Double-check it's installed in the registry correctly."));
						else
							GWarn->Logf(TEXT("Sent CD key hash: \"%s\""), *GetCDKeyHash() );
						ShouldTryReconnect = 0;
						ConnectionFailed = 1;
					}
					else
					{
						// master server busy, or i didn't understand its response
						ShouldTryReconnect = 1;
						ConnectionFailed = 1;
					}
					unguard;
				}
				break;
			case MSUS_WaitingForUDPResponse:
				{
					guard(MSUS_WaitingForUDPResponse);
					BYTE Success;
					*ArRecv << Success;

					if( Success )
					{
						*ArRecv << HeartbeatPeriod;
						*ArRecv << QueryNatPort << GameNatPort << GamespyNatPort;
						UplinkState = MSUS_ChannelOpen;
						OnChannelOpened();
					}
					else
					{
						BYTE HeartbeatType;
						INT HeartbeatCode;
						*ArRecv << HeartbeatType << HeartbeatCode;
						GWarn->Logf(TEXT("Master server requests heartbeat %d with code %d"), HeartbeatType, HeartbeatCode);
						switch( HeartbeatType )
						{
						case HB_QueryInterface:
							QueryInterface->SendHeartbeat(RemoteHost, HeartbeatCode);
							break;
						case HB_GamePort:
							GameHeartbeat->SendHeartbeat(RemoteHost, HeartbeatCode);
							break;
						case HB_GamespyQueryPort:
							if( GSQueryHeartbeat )
								GSQueryHeartbeat->SendHeartbeat(RemoteHost, HeartbeatCode);
							break;
						}
					}
					unguard;
				}
				break;
			case MSUS_ChannelOpen:
				{
					guard(MSUS_ChannelOpen);
                    BYTE Command;
					*ArRecv << Command;
					switch( Command )
					{
					case MTS_ClientChallenge:
						{
							FString Client;
							FString ClientChallenge;
							*ArRecv << Client;
							*ArRecv << ClientChallenge;
							ChallengeClient( *Client, *ClientChallenge );
						}
						break;
					case MTS_ClientAuthFailed:
					case MTS_ClientDupKey:
						{
							FString Client;
							*ArRecv << Client;
							ClientAuthFailed( *Client, TEXT("RI_AuthenticationFailed"), TEXT("") );
						}
						break;
					case MTS_UTANBan:
						{
							FString Client;
							*ArRecv << Client;
							ClientAuthFailed( *Client, TEXT("RI_UTANBan"), TEXT("") );
						}
						break;
					case MTS_ClientMD5Fail:
						{
							FString Client;
							*ArRecv << Client;
							ClientAuthFailed(*Client, TEXT("RI_BadClient"),TEXT("") );
						}
					case MTS_ClientBanned:
						{
							FString Client, Banned;
							*ArRecv << Client << Banned;
							ClientAuthFailed(*Client, TEXT("RI_BannedClient"),*Banned );
						}
					case MTS_Shutdown:
						{
							Close();
							ConnectionFailed = 1;
						}
						break;
					case MTS_MatchID:
						{
							INT MatchID;
							*ArRecv << MatchID;
							GWarn->Logf(TEXT("Master server assigned our MatchID: %d"), MatchID);
							// save in the GRI.
							Actor->MatchID = MatchID;
							if( Actor->Level->Game && Actor->Level->Game->GameReplicationInfo )
								Actor->Level->Game->GameReplicationInfo->MatchID = MatchID;

							// If we were denied a match ID, don't try to send stats.
							if( !MatchID && Actor->GameStats )
							{
								Actor->GameStats->Uplink = NULL;
								Actor->GameStats = NULL;
							}

							LastRefreshTime = GCurrentTime - REFRESH_TIME;	// Force the initial update of the database.
						}
						break;
					case MTS_MD5Update:
						{
							TArray<FMD5UpdateData> UpdateData;
							*ArRecv << UpdateData;
							GWarn->Logf(TEXT("Received updated MD5 data from master server."));

							UGameEngine* GE = Cast<UGameEngine>(Actor->XLevel->Engine);
							if (GE)
							{
								for( INT i=0;i<UpdateData.Num();i++ )
								{
									GE->AddMD5(*UpdateData(i).Guid, *UpdateData(i).MD5, UpdateData(i).Revision);
								}

								GE->SaveMD5Database();
							}
						}
						break;
					case MTS_UpdateOption:
						{
							// Set INI entry
							FString Section, Key, Value;
							UBOOL Unique;
                            *ArRecv << Section << Key << Value << Unique;
							GConfig->SetString( *Section, *Key, *Value, NULL, Unique );
						}
						break;
					case MTS_CheckOption:
						{
							// Get INI entry
							FString Section, Key, Result;
							*ArRecv << Section << Key;
							GConfig->GetString( *Section, *Key, Result );
							BYTE Command = STM_CheckOptionReply;
							*ArSend << Command << Section << Key << Result;
							ArSend->Flush();
						}
						break;
					default:
						GWarn->Logf(TEXT("MSUS_ChannelOpen: received unknown command %d"), Command);
					}
					unguard;
				}
				break;
			}
		}
		unguard;
	}

	virtual UBOOL Poll( INT WaitTime )
	{
		guard(FMasterServerUplinkLink::Poll);

		UBOOL Result = FTcpLink::Poll( WaitTime );

		// answer queries
		if( LANInterface )
			LANInterface->Poll();
		if( QueryInterface )
			QueryInterface->Poll();

		if( ConnectionFailed )
		{
			Actor->eventConnectionFailed(ShouldTryReconnect);
			ConnectionFailed = 0;
		}
		else
		{
			// See if any CD key challenges have been answered.
			CheckOutstandingChallenges();

			// See if we need to update the masterserver's copy of our server state.
			CheckRefresh();

			// See if we need to send any heartbeats;
			CheckUDPHeartbeats();
		}

		if ( GameHeartbeat->bConnectionBroken )
		{
			Actor->eventConnectionFailed(ShouldTryReconnect);
			ConnectionFailed = 0;
		}


		return Result;
		unguard;
	}

	void CheckOutstandingChallenges()
	{
		guard(FMasterServerUplinkLink::CheckOutstandingChallenges);
		if( OutstandingChallenges.Num() )
		{
			UNetDriver* NetDriver = Actor->XLevel->NetDriver;
			if( NetDriver )
			{
				for( INT conn=0;conn<NetDriver->ClientConnections.Num(); conn++ )
				{
//					if( NetDriver->ClientConnections(conn)->CDKeyResponse != TEXT("") )
//					{
						INT idx = OutstandingChallenges.FindItemIndex( NetDriver->ClientConnections(conn)->LowLevelGetRemoteAddress() );
						if( idx != INDEX_NONE )
						{
							// Send response to master server.

							UNetConnection* NetConn = NetDriver->ClientConnections(conn);

							BYTE Command = STM_ClientResponse;
							FString Client = OutstandingChallenges(idx);
							FString CDKeyHash = NetConn->CDKeyHash;
							FString Response = NetConn->CDKeyResponse;
							FString GlobalMD5;

							for (INT i=0; i<16; i++)
								GlobalMD5 += FString::Printf(TEXT("%02x"), NetConn->GMD5[i]);	

							FString PlayerName=TEXT("");
							if (NetConn->Actor && NetConn->Actor->PlayerReplicationInfo)
								PlayerName = NetConn->Actor->PlayerReplicationInfo->PlayerName; 

							*ArSend << Command << Client << CDKeyHash << Response << GlobalMD5 << PlayerName;
							ArSend->Flush();
							OutstandingChallenges.Remove(idx);
						}
//					}
				}
			}
		}
		unguard;
	}

	void ForceStateRefresh(INT When)
	{
		guard(FMasterServerUplinkLink::ForceStateRefresh);

		if (GCurrentTime < LastRefreshTime + REFRESH_TIME - When)
			LastRefreshTime = GCurrentTime + When - REFRESH_TIME;

		unguard;
	}


	void CheckRefresh()
	{
		guard(FMasterServerUplinkLink::CheckRefresh);

		if( GCurrentTime - LastRefreshTime > REFRESH_TIME )
		{

			LastRefreshTime = GCurrentTime;
			Actor->eventRefresh();

			if( UplinkState==MSUS_ChannelOpen && (LinkState==LINK_Connected || LinkState==LINK_ClosePending) )
			{
				TArray<FString> Clients;
				UNetDriver* NetDriver = Actor->XLevel->NetDriver;
				if( NetDriver )
					for( INT conn=0;conn<NetDriver->ClientConnections.Num(); conn++ )
						if( NetDriver->ClientConnections(conn)->Actor )
							Clients(Clients.AddZeroed()) = NetDriver->ClientConnections(conn)->LowLevelGetRemoteAddress();

				// Send the clients and game state to the master server.
				BYTE Command = STM_GameState;

				// Insure the proper mutator count

				check(Actor);
				check(Actor->Level);
				check(Actor->GameGroup);

				guard(CheckingServerList);

				for (AMutator* Mut = Actor->Level->Game->BaseMutator; Mut; Mut = Mut->NextMutator)
				{
					FString MutName = FString::Printf(TEXT("%s"),Mut->GetName());
					UBOOL bFound=false;
					for (INT i=0;i<Actor->ServerState.ServerInfo.Num();i++)
						if ( Actor->ServerState.ServerInfo(i).Key.Caps() == TEXT("MUTATOR") && 
							 Actor->ServerState.ServerInfo(i).Value.Caps() == MutName.Caps() )
							bFound = true;

					if (!bFound)
					{
						guard(AddingHiddenServer);
						
						FKeyValuePair* Data= new(Actor->ServerState.ServerInfo) FKeyValuePair();
						Data->Key = TEXT("mutator");
						Data->Value = MutName;
						unguard;
					}
				}

				unguard;

				*ArSend << Command << Clients << Actor->ServerState;
				ArSend->Flush();
			}
		}
		unguard;
	}

	void CheckUDPHeartbeats()
	{
		guard(FMasterServerUplinkLink::CheckUDPHeartbeats);

		if( UplinkState != MSUS_ChannelOpen )
			return;
		if( LinkState != LINK_Connected && LinkState != LINK_ClosePending )
			return;

		if( Actor->ServerBehindNAT && HeartbeatPeriod && (GCurrentTime - LastHeartbeatTime > HeartbeatPeriod) )
		{
			LastHeartbeatTime = GCurrentTime;

			// send UDP heartbeats.
			QueryInterface->SendHeartbeat(RemoteHost, 0);
			GameHeartbeat->SendHeartbeat(RemoteHost, 0);
			if( GSQueryHeartbeat )
				GSQueryHeartbeat->SendHeartbeat(RemoteHost, 0);
		}

		unguard;
	}

	UBOOL SendStatLine( const TCHAR* StatLine )
	{
		guard(SendStatLine);
		if( UplinkState == MSUS_ChannelOpen )
		{
			BYTE Command = STM_Stats;
			FString StatLineStr = StatLine;
			*ArSend << Command << StatLineStr;
			ArSend->Flush();
			//GWarn->Logf(TEXT("sent stats line >>%s<<"), StatLine);
		}
		else
		{
			//GWarn->Logf(TEXT("buffering stats line >>%s<<"), StatLine);
			StatsBuffer(StatsBuffer.AddZeroed()) = StatLine;
		}

		return 1;
		unguard;
	}

	UNetConnection* FindClient( const TCHAR* ClientIP )
	{
		guard(FMasterServerUplinkLink::FindClient);
		UNetDriver* NetDriver = Actor->XLevel->NetDriver;
        if( NetDriver )
		{
			for( INT i=0;i<NetDriver->ClientConnections.Num(); i++ )
				if( !appStrcmp( *NetDriver->ClientConnections(i)->LowLevelGetRemoteAddress(), ClientIP ) )
					return NetDriver->ClientConnections(i);
		}
		return NULL;
		unguard;
	}

	APlayerController* FindClientActor( const TCHAR* ClientIP )
	{
		guard(FMasterServerUplinkLink::FindClientActor);
		UNetDriver* NetDriver = Actor->XLevel->NetDriver;
        if( NetDriver )
		{
			for( INT i=0;i<NetDriver->ClientConnections.Num(); i++ )
				if( !appStrcmp( *NetDriver->ClientConnections(i)->LowLevelGetRemoteAddress(), ClientIP ) )
					return Cast<APlayerController>(NetDriver->ClientConnections(i)->Actor);
		}
		return NULL;
		unguard;
	}

	void ChallengeClient( const TCHAR* ClientIP, const TCHAR* Challenge )
	{
		guard(FMasterServerUplinkLink::ChallengeClient);
		UNetConnection* Conn = FindClient(ClientIP);
		if( Conn )
		{
			Conn->CDKeyResponse = TEXT("");
/*
			APlayerController *PC = Cast<APlayerController>(Conn->Actor);
			if ( PC )
				PC->eventClientValidate(Challenge);
*/
			OutstandingChallenges(OutstandingChallenges.AddZeroed()) = ClientIP;
		}
		unguard;
	}

	void ClientAuthFailed( const TCHAR* ClientIP, const TCHAR* FailCode, const TCHAR* Why )
	{
		guard(FMasterServerUplinkLink::ClientAuthFailed);

		OutstandingChallenges.RemoveItem(FString(ClientIP));

		UNetConnection* Conn = FindClient(ClientIP);
		if( Conn )
		{
				GWarn->Logf(TEXT("Client %s failed auth. Reason= [%s]\"%s\"... disconnecting"), ClientIP,FailCode, Why );
			if (Conn && Conn->Actor)
			{
				if ( !appStrcmp(FailCode,TEXT("RI_UTANBan")) && Conn->NegotiatedVer < 3352 )
					Conn->Actor->eventClientNetworkMessage(TEXT("RI_BannedClient"),TEXT("you contact abuse@epicgames.com!"));
				else
					Conn->Actor->eventClientNetworkMessage(FString(FailCode),FString(Why));
			}

			// Disconnect the player.
			delete Conn;
		}
		else
		{
			// Couldn't find the client to disconnect.
			GWarn->Logf(TEXT("Disconnect of client %s failed"), ClientIP );

			// Tell the master server about it.
			BYTE Command = STM_ClientDisconnectFailed;
			FString StrClient = ClientIP;
			*ArSend << Command << StrClient;
			ArSend->Flush();
		}
		unguard;
	}

	void SendServerConfig()
	{
		UBOOL NAT = Actor->ServerBehindNAT;
		UBOOL Gamespy = GSQueryHeartbeat != NULL;
		*ArSend << NAT << Gamespy;
		ArSend->Flush();
	}

// Some versions of GNU C++ seem to need a little nudging here. --ryan.
#ifdef __GNUC__
    void *operator new(size_t x) { return ::operator new(x); }
#endif
};

/*-----------------------------------------------------------------------------
	FServerQueryInterface implementaion
-----------------------------------------------------------------------------*/

#define MAX_QUERY_PACKET_THRESHOLD 450

void FServerQueryInterface::OnReceivedData( FIpAddr SrcAddr, BYTE* Data, INT Count )
{
	guard(FServerQueryInterface::OnReceivedData);

//	GWarn->Logf(TEXT("Received %d bytes from %s"), Count, *SrcAddr.GetString(1) );

	FArchiveUdpReader ArRecv( Data, Count );
	FArchiveUdpWriter ArSend( this, SrcAddr );
	BYTE Command;
	ArRecv << Command;
	if( !ArRecv.IsError() && ArRecv.AtEnd() )
	{
		switch( Command )
		{
		case QI_Ping:
	//		GWarn->Logf(TEXT("Sending ping response to %s"), *SrcAddr.GetString(1) );
			SendPing( ArSend );
			break;
		case QI_Rules:
	//		GWarn->Logf(TEXT("Sending rules response to %s"), *SrcAddr.GetString(1) );
			SendRules( ArSend );
			break;
		case QI_Players:
			SendPlayers( ArSend );
			break;
		case QI_RulesAndPlayers:
			SendRules( ArSend );
			SendPlayers( ArSend );
			break;
		case QI_SmallPing:
			SendSmallPing( ArSend );
			break;
		default:
			GWarn->Logf(TEXT("Unknown ping request command: %d"), Command);
			break;
		}
	}
	unguard;
}

void FServerQueryInterface::SendSmallPing( FArchive& ArSend )
{
	BYTE Command = QI_SmallPing;
	BYTE Data = 'P';
	ArSend << Command << Data;
	ArSend.Flush();
}

void FServerQueryInterface::SendPing( FArchive& ArSend )
{
	BYTE Command = QI_Ping;
	ArSend << Command;
	FServerResponseLine PingResponse = Uplink->Actor->ServerState;
	PingResponse.PlayerInfo.Empty();
	PingResponse.ServerInfo.Empty();
	ArSend << PingResponse;
	ArSend.Flush();
}

void FServerQueryInterface::SendRules( FArchive& ArSend )
{
	BYTE Command = QI_Rules;

	TArray<FKeyValuePair>& ServerInfo = Uplink->Actor->ServerState.ServerInfo;
	ArSend << Command;
	for( INT i=0;i<ServerInfo.Num();i++ )
	{
		if( ArSend.Tell() > MAX_QUERY_PACKET_THRESHOLD )
		{
			ArSend.Flush();
			ArSend << Command;
		}
		ArSend << ServerInfo(i);
	}
	ArSend.Flush();
}

void FServerQueryInterface::SendPlayers( FArchive& ArSend )
{
	BYTE Command = QI_Players;

	TArray<FPlayerResponseLine>& PlayerInfo = Uplink->Actor->ServerState.PlayerInfo;

	if( PlayerInfo.Num() )
	{
		ArSend << Command;
		for( INT i=0;i<PlayerInfo.Num();i++ )
		{
			if( ArSend.Tell() > MAX_QUERY_PACKET_THRESHOLD )
			{
				ArSend.Flush();
				ArSend << Command;
			}
			ArSend << PlayerInfo(i);
		}
		ArSend.Flush();
	}
}


/*-----------------------------------------------------------------------------
	FServerLANInterface implementaion
-----------------------------------------------------------------------------*/

FServerLANInterface::FServerLANInterface( INT InPort, class FMasterServerUplinkLink* InUplink, FServerQueryInterface* InReplyInterface )
:	FServerQueryInterface( InPort, InUplink )
,	ReplyInterface(InReplyInterface)
{
    if (Uplink->Actor->LANPort >= 0)
    {
	    // on statup, broadcast our info in case anyone is listening on the server.
    	Uplink->Actor->eventRefresh();
    	FIpAddr Addr;
    	Addr.Addr = INADDR_BROADCAST;
    	Addr.Port = Uplink->Actor->LANPort;
    	FArchiveUdpWriter ArSend( ReplyInterface, Addr );
    	SendPing( ArSend );
    }
}

void FServerLANInterface::OnReceivedData( FIpAddr SrcAddr, BYTE* Data, INT Count )
{
	guard(FServerLANInterface::OnReceivedData);
	Uplink->Actor->eventRefresh();
	ReplyInterface->OnReceivedData( SrcAddr, Data, Count );
	unguard;
}


/*-----------------------------------------------------------------------------
	AMasterServerUplink
-----------------------------------------------------------------------------*/

void AMasterServerUplink::execReconnect( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
#ifdef __EMSCRIPTEN__
	// No master-server uplink on the web: servers are found through WebRTC discovery or the game
	// proxy's list. The FMasterServerUplinkLink constructor casts NetDriver to UTcpNetDriver, which
	// the web's net drivers are not.
	debugf( NAME_DevNet, TEXT("[WebRTC] MasterServerUplink::Reconnect: skipped on Emscripten (using PeerJS discovery)") );
	return;
#endif
	if( !LinkPtr )
		LinkPtr = (PTRINT)(new FMasterServerUplinkLink(this));
	else
		((FMasterServerUplinkLink*)LinkPtr)->Close();

	if( DoUplink )
		((FMasterServerUplinkLink*)LinkPtr)->TryConnect();
	else
		GWarn->Logf(TEXT("MasterServerUplink: DoUplink is False, not connecting to Epic master server"));
}

void AMasterServerUplink::execForceGameStateRefresh( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(When);
	P_FINISH;

	if ( LinkPtr )
		((FMasterServerUplinkLink*)LinkPtr)->ForceStateRefresh(When);
}

void AMasterServerUplink::execLogStatLine( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(StatLine);
	P_FINISH;
	if( !LinkPtr )
		LinkPtr = (PTRINT)(new FMasterServerUplinkLink(this));
	*(UBOOL*)Result = ((FMasterServerUplinkLink*)LinkPtr)->SendStatLine( *StatLine );
}

UBOOL AMasterServerUplink::Poll( INT WaitTime )
{
	guard(AMasterServerClient::Poll);
	if( LinkPtr )
		return ((FMasterServerUplinkLink*)LinkPtr)->Poll(WaitTime);
	return 0;
	unguard;
}

void AMasterServerUplink::Destroy()
{
	if( LinkPtr )
		delete ((FMasterServerUplinkLink*)LinkPtr);
	LinkPtr = 0;
	Super::Destroy();
}

void AMasterServerUplink::PostScriptDestroyed()
{
	if( LinkPtr )
		delete ((FMasterServerUplinkLink*)LinkPtr);
	LinkPtr = 0;
	Super::PostScriptDestroyed();
}

IMPLEMENT_CLASS(AMasterServerUplink);
IMPLEMENT_CLASS(AMasterServerGameStats);

