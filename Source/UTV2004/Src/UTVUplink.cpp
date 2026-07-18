
#include "UTVUplink.h"
#include "ReplicatorEngine.h"
#include "UnTcpNetDriver.h"

//Is this really good that I need these two lines?
IMPLEMENT_CLASS(UTcpipConnection);
IMPLEMENT_CLASS(UTcpNetDriver);

#define REFRESH_TIME (60.0)
#define MAX_QUERY_PACKET_THRESHOLD (450)

///////////////////////////////////////////////

void UTVQueryInterface::OnReceivedData( FIpAddr SrcAddr, BYTE* Data, INT Count )
{
	guard(UTVQueryInterface::OnReceivedData);

	//debugf (TEXT ("Received %d bytes from %s"), Count, *SrcAddr.GetString(1));

	FArchiveUdpReader ArRecv( Data, Count );
	FArchiveUdpWriter ArSend( this, SrcAddr );
	BYTE Command;
	ArRecv << Command;
	if( !ArRecv.IsError() && ArRecv.AtEnd() )
	{
		switch( Command )
		{
		case QI_Ping:
			SendPing( ArSend );
			break;
		case QI_Rules:
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
			debugf (TEXT ("UTVQuery: Unhandled command %d"), Command);
			break;
		}
	}
	unguard;
}

void UTVQueryInterface::SendPing( FArchive& ArSend )
{
	BYTE Command = QI_Ping;
	ArSend << Command;
	FServerResponseLine PingResponse = UtvEngine->ServerState;
	PingResponse.PlayerInfo.Empty();
	PingResponse.ServerInfo.Empty();
	ArSend << PingResponse;
	ArSend.Flush();
}

void UTVQueryInterface::SendSmallPing( FArchive& ArSend )
{
	BYTE Command = QI_SmallPing;
	BYTE Data = 'P';
	ArSend << Command << Data;
	ArSend.Flush();
}

void UTVQueryInterface::SendRules( FArchive& ArSend )
{
	BYTE Command = QI_Rules;
	TArray<FKeyValuePair>& ServerInfo = UtvEngine->ServerState.ServerInfo;
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

void UTVQueryInterface::SendPlayers( FArchive& ArSend )
{
	BYTE Command = QI_Players;
	TArray<FPlayerResponseLine>& PlayerInfo = UtvEngine->ServerState.PlayerInfo;

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

///////////////////////////////////////////////

UTVUplink::UTVUplink (UBOOL connect) 
{
	SetLinkMode( TCPLINK_FArchive );

	LastRefreshTime = -REFRESH_TIME;

	UTcpNetDriver* NetDriver = (UTcpNetDriver *)UtvEngine->ListenDriver;
	check(NetDriver);

	QueryInterface = new UTVQueryInterface (NetDriver->GetSocketData().Port + 1);

	//If true, try to connect to master instead of just replying on requests
	if (connect) {
		GameHeartbeat = new FNATHeartbeatLink( NetDriver->GetSocketData(), HB_GamePort );
		TryConnect();
	} else
		GameHeartbeat = 0;
}

UTVUplink::~UTVUplink()
{
	if (GameHeartbeat)
		delete GameHeartbeat;
	delete QueryInterface;
}
void UTVUplink::TryConnect ()
{
	UplinkState = MSUS_WaitingChallenge;
	MasterServerName = "ut2004master1.epicgames.com";
	MasterServerPort = 28902;
	Resolve (*MasterServerName);
}

void UTVUplink::OnResolved( FIpAddr a )
{
	a.Port = MasterServerPort;
	RemoteHost = a;
	debugf (TEXT ("Uplink: Resolved master server %s to: %s"), *MasterServerName, *a.GetString (0));
	Connect( a );
}

void UTVUplink::OnResolveFailed()
{
	debugf (TEXT ("Uplink: Failed to resolve master server %s."), *MasterServerName);
	//ConnectionFailed = 1;
}

void UTVUplink::OnConnectionSucceeded()
{
	GWarn->Logf(TEXT("Uplink: Connection to %s established."), *MasterServerName);
}

void UTVUplink::OnConnectionFailed()
{
	debugf (TEXT ("Uplink: Uplink failed to connect to master server %s."), *MasterServerName);
	//ConnectionFailed = 1;
}

void UTVUplink::OnClosed()
{
	//ConnectionFailed = 1;
}

void UTVUplink::SendServerConfig()
{
	UBOOL NAT = false; //Actor->ServerBehindNAT;
	UBOOL Gamespy = false; //GSQueryHeartbeat != NULL;
	*ArSend << NAT << Gamespy;
	ArSend->Flush();
}

void UTVUplink::OnDataReceived()
{
	guard(UTVUplink::OnDataReceived);
		
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
					INT MatchID = -1;
					BYTE Platform = GRunningOS;
					FString Language = UObject::GetLanguage();

					*ArRecv << Challenge;

					CDKeyHash = GetCDKeyHash();
					Response = GetCDKeyResponse( *Challenge );
					ClientType = TEXT("UT2K4SERVER");

					*ArSend << CDKeyHash << Response << ClientType << Version << Platform << Language << MatchID;
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

					if( Approval == TEXT("APPROVED") )
					{
						// Server was approved.  Send our configuration and wait for
						// command to send UDP heartbeats.
						SendServerConfig();
						UplinkState = MSUS_WaitingForUDPResponse;
						debugf (TEXT ("Uplink: Master server accepted us, sending configuration"));
					}
					else
					if( Approval == TEXT("UPGRADE") )
					{
						INT UpgradeVersion;
						*ArRecv << UpgradeVersion;
						debugf (TEXT ("Uplink: Rejected. Must upgrade to version %d."), UpgradeVersion);
						//ShouldTryReconnect = 0;
						//ConnectionFailed = 1;
					}
					else
					if( Approval == TEXT("MSLIST") )
					{
						debugf (TEXT ("Uplink: Receiving new masters, but we don't support this.. Aborting."));

						//ShouldTryReconnect = 1;
						//ConnectionFailed = 1;
					}
					else
					if( Approval == TEXT("DENIED") )
					{
						debugf (TEXT ("Uplink: Master server rejected authentication request."));
						debugf (TEXT ("Check your CD key."));
						if( GetCDKeyHash() == TEXT("d41d8cd98f00b204e9800998ecf8427e") )
							debugf (TEXT ("Your CD key appears to be blank.  Double-check it's installed in the registry correctly."));
						else
							debugf (TEXT ("Sent CD key hash: \"%s\""), *GetCDKeyHash() );
						//ShouldTryReconnect = 0;
						//ConnectionFailed = 1;
					}
					else
					{
						// master server busy, or i didn't understand its response
						//ShouldTryReconnect = 1;
						//ConnectionFailed = 1;
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
						//Don't keep these
						DWORD						QueryNatPort;
						DWORD						GameNatPort;
						DWORD						GamespyNatPort;

						*ArRecv << HeartbeatPeriod;
						*ArRecv << QueryNatPort << GameNatPort << GamespyNatPort;
						UplinkState = MSUS_ChannelOpen;
						debugf (TEXT ("Uplink: Master server connect completed! (%d, %d, %d)"), GameNatPort, QueryNatPort, GamespyNatPort);
						
						CheckRefresh (true);
					}
					else
					{
						BYTE HeartbeatType;
						INT HeartbeatCode;
						*ArRecv << HeartbeatType << HeartbeatCode;
						GWarn->Logf(TEXT("Uplink: Master server requests heartbeat %d with code %d"), HeartbeatType, HeartbeatCode);
						switch( HeartbeatType )
						{
						case HB_QueryInterface:
							QueryInterface->SendHeartbeat(RemoteHost, HeartbeatCode);
							break;
						case HB_GamePort:
							GameHeartbeat->SendHeartbeat(RemoteHost, HeartbeatCode);
							break;
						case HB_GamespyQueryPort:
							//if( GSQueryHeartbeat )
							//	GSQueryHeartbeat->SendHeartbeat(RemoteHost, HeartbeatCode);
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
							//debugf (TEXT ("Uplink: Challenging (%s) (%s)"), *Client, *ClientChallenge);
							//ChallengeClient( *Client, *ClientChallenge );
						}
						break;
					case MTS_MatchID:
						{
							INT MatchID;
							*ArRecv << MatchID;
							debugf (TEXT ("Uplink: Master server assigned our MatchID: %d"), MatchID);
						}
						break;
					default:
						{
							debugf (TEXT ("Uplink: Received unhandled command %i"), Command);
						}
						break;
					}
					unguard;
				}
				break;
		}
	}

	unguard;
}

UBOOL UTVUplink::Poll( INT WaitTime )
{
	guard(UTVUplink::Poll);

	UBOOL Result = FTcpLink::Poll( WaitTime );

	// answer queries
//	if( LANInterface )
//		LANInterface->Poll();
	if( QueryInterface )
		QueryInterface->Poll();

	CheckRefresh ();

/*
	if( ConnectionFailed )
	{
		Actor->eventConnectionFailed(ShouldTryReconnect);
		ConnectionFailed = 0;
	}
	else
	{
		// See if any CD key challenges have been answered.
		//CheckOutstandingChallenges();

		// See if we need to update the masterserver's copy of our server state.
		CheckRefresh();

		// See if we need to send any heartbeats;
		//CheckUDPHeartbeats();
	}
*/
	return Result;
	unguard;
}

void UTVUplink::CheckRefresh()
{
	CheckRefresh (false);
}

void UTVUplink::CheckRefresh(bool force)
{
	guard(UTVUplinkLink::CheckRefresh);

	if (( appSeconds() - LastRefreshTime > REFRESH_TIME ) || force)
	{
		LastRefreshTime = appSeconds();

		if( UplinkState==MSUS_ChannelOpen && (LinkState==LINK_Connected || LinkState==LINK_ClosePending) )
		{
			TArray<FString> Clients;

			// Send the clients and game state to the master server.
			BYTE Command = STM_GameState;
			*ArSend << Command << Clients << UtvEngine->ServerState;
			ArSend->Flush();
			//debugf (TEXT ("Uplink: Sending gamestate"));
		}
	}
	unguard;
}