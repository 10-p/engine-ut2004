/*============================================================================
	MasterServerClient.cpp - master server client

	Revision history:
		* Created by Jack Porter
============================================================================*/

#include "UnIpDrv.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// WebRTC server discovery lives in WebRTCDrv (WebRTCDiscovery.cpp) — the JS-bridge glue +
// the FServerResponseLine building is "our stuff", kept out of the stock IpDrv query path.
// Resolved at link time like the webrtc-net.js bridges; FServerResponseLine is an Engine
// type, so this is a one-way IpDrv -> WebRTCDrv dependency (no module cycle).
extern void  WebRTCDiscovery_Begin( const char* room );
extern UBOOL WebRTCDiscovery_Ready();
extern INT   WebRTCDiscovery_Drain( TArray<FServerResponseLine>& Out );

// Active browser actor for WebRTC discovery callback firing
static AMasterServerClient* GActiveWebRTCBrowser = NULL;
static UBOOL GWebRTCDiscoveryPending = 0;
#endif

/*-----------------------------------------------------------------------------
	FServerQueryLink
-----------------------------------------------------------------------------*/

#define PINGTIMEOUT 2.0f

struct FOutstandingPingInfo
{
	INT ServerID;
	BYTE PingCause;
	BYTE PingType;
	FIpAddr QueryAddr;
	DOUBLE PingSendTime;
	FServerResponseLine CurrentState;
	UBOOL GotReply;
	INT PingCount;
};

class FServerQueryLink : public FUdpLink
{
	AServerQueryClient* Actor;
	TArray<FOutstandingPingInfo> OutstandingPings;
#ifdef __EMSCRIPTEN__
	// LAN discovery via WebRTC (no UDP broadcast in the browser). Set when a
	// broadcast ping is requested; drained in Poll() once discovery settles.
	UBOOL bWebRTCBroadcastPending;
	INT   WebRTCServerID;
	BYTE  WebRTCPingCause;
#endif
public:
	FServerQueryLink( AServerQueryClient* InActor, INT InPort )
	:	FUdpLink()
	,	Actor(InActor)
	{
		guard(FServerQueryLink::FServerQueryLink);
#ifdef __EMSCRIPTEN__
		bWebRTCBroadcastPending = 0;
		WebRTCServerID  = -1;
		WebRTCPingCause = 0;
#endif
		BindPort(InPort);
		unguard;
	}
	void Poll()
	{
		// GCurrentTime is sometimes slightly behind appSeconds() at this point, so using GCurrentTime here 
		// runs the risk of invalidating some (not many) server's ping results
		DOUBLE CurrentTime = appSeconds();

		// check for ping timeouts.
		for( INT i=0;i<OutstandingPings.Num();i++ )
		{
			if( CurrentTime > OutstandingPings(i).PingSendTime + PINGTIMEOUT )
			{
				if( !OutstandingPings(i).GotReply && OutstandingPings(i).QueryAddr.Addr != INADDR_BROADCAST )
				{
					if( OutstandingPings(i).PingCause==PC_AutoPing && OutstandingPings(i).PingCount<2 )
					{
						// re-ping
						OutstandingPings(i).PingCount++;
						OutstandingPings(i).PingSendTime = CurrentTime;
						BYTE Command = OutstandingPings(i).PingType;
                   		FArchiveUdpWriter ArSend( this, OutstandingPings(i).QueryAddr );
						ArSend << Command;
						ArSend.Flush();
					}
					else
					{
						Actor->delegateOnPingTimeout( OutstandingPings(i).ServerID, OutstandingPings(i).PingCause );
						if ( i < OutstandingPings.Num() )
							OutstandingPings.Remove(i);
						i--;
					}
				}
				else
				{
					OutstandingPings.Remove(i);
					i--;
				}
			}
		}

#ifdef __EMSCRIPTEN__
		// WebRTC LAN discovery: once the peer-list + status pings settle, emit each
		// server via OnReceivedPingInfo on the ServerID<0 (broadcast-response) path so
		// the stock LAN browser lists them (it dedupes by IP+Port; IP = room/join key).
		// The row building lives in WebRTCDrv (WebRTCDiscovery_Drain).
		if( bWebRTCBroadcastPending && WebRTCDiscovery_Ready() )
		{
			bWebRTCBroadcastPending = 0;
			TArray<FServerResponseLine> Servers;
			WebRTCDiscovery_Drain( Servers );
			for( INT i = 0; i < Servers.Num(); i++ )
				Actor->delegateOnReceivedPingInfo( WebRTCServerID, WebRTCPingCause, Servers(i) );
		}
#endif

		FUdpLink::Poll();
	}
	void PingServer( INT ServerID, BYTE PingCause, FIpAddr Addr, BYTE Command, FServerResponseLine& CurrentState )
	{
		guard(FServerQueryLink::PingServer);

#ifdef __EMSCRIPTEN__
		// LAN broadcast has no UDP in the browser. Kick WebRTC discovery (PeerJS
		// peer list + per-server status ping); Poll() emits the results via
		// OnReceivedPingInfo. Skip the (dead) UDP send below.
		if( Addr.Addr == INADDR_BROADCAST )
		{
			WebRTCDiscovery_Begin( "" );  // all rooms; Poll() drains the results
			bWebRTCBroadcastPending = 1;
			WebRTCServerID  = ServerID;   // -1 for a broadcast request
			WebRTCPingCause = PingCause;  // PC_LANBroadcast
			return;
		}
#endif

		// find existing ping.
		INT i;
		for( i=0;i<OutstandingPings.Num();i++ )
			if( Addr == OutstandingPings(i).QueryAddr )
				break;
		if( i == OutstandingPings.Num() )
			i = OutstandingPings.AddZeroed();

		// Same issue with GCurrentTime here....GCurrentTime is sometimes slightly behind what appSeconds() would return,
		// so using GCurrentTime runs the risk of invalidating some ping results
		OutstandingPings(i).PingCount = 0;
		OutstandingPings(i).PingSendTime = appSeconds();
		OutstandingPings(i).QueryAddr = Addr;
		OutstandingPings(i).CurrentState = CurrentState;
		OutstandingPings(i).ServerID = ServerID;
		OutstandingPings(i).PingCause = PingCause;
		OutstandingPings(i).PingType  = Command;

		FArchiveUdpWriter ArSend( this, Addr );
		ArSend << Command;
		ArSend.Flush();

		// HACK: also ping the server when we request rules.
		if( Command == 3 )
		{
			Command = 0;
			ArSend << Command;
			ArSend.Flush();
		}
		unguard;
	}
	void CancelPings()
	{
		guard(FServerQueryLink::CancelPings);
		OutstandingPings.Empty();
		unguard;
	}
	void OnReceivedData( FIpAddr SrcAddr, BYTE* Data, INT Count )
	{
		guard(FServerQueryLink::OnReceivedData);
		FArchiveUdpReader ArRecv( Data, Count );

		// find server
		INT i;
		INT bc = -1;
		for( i=0;i<OutstandingPings.Num();i++ )
		{
			if( SrcAddr == OutstandingPings(i).QueryAddr )
				break;
			if( OutstandingPings(i).QueryAddr.Addr == INADDR_BROADCAST )
				bc = i;
		}

		// Same issue with GCurrentTime here....GCurrentTime is sometimes slightly behind what appSeconds() would return,
		// but here, using GCurrentTime can result in server's appearing in the list with zero ping
		FServerResponseLine* CurrentState=NULL;
		INT Ping;
		if( i < OutstandingPings.Num() )
		{
			Ping = appRound( (appSeconds() - OutstandingPings(i).PingSendTime) * 1000 );
			CurrentState = &OutstandingPings(i).CurrentState;
			OutstandingPings(i).GotReply = 1;
		}
		else
		if( bc >= 0 )
		{
			Ping = appRound( (appSeconds() - OutstandingPings(bc).PingSendTime) * 1000 );
		}
		else
			Ping = 9999;

		BYTE Command;
		ArRecv << Command;

		if( ArRecv.IsError() )
		{
			GWarn->Logf(TEXT("Received bad UDP ping data from %s (%i bytes)"), *SrcAddr.GetString(1), Count);
		}
		else
		{
			switch( Command )
			{
			case QI_Ping:
				{
					FServerResponseLine NewState;
					ArRecv << NewState;
						NewState.Ping = Ping;
					NewState.IP = SrcAddr.GetString(0);
					NewState.QueryPort = SrcAddr.Port;

					if( CurrentState )
					{
						// HACK don't update the data if you clicked on the server while we're pinging!
						if( OutstandingPings(i).PingType!=QI_Ping )
						{
							if( OutstandingPings.Num() > 5 )
							{
								NewState.CurrentPlayers = CurrentState->CurrentPlayers;
								NewState.MaxPlayers = CurrentState->MaxPlayers;
								NewState.MapName = CurrentState->MapName;
	                            NewState.Ping = CurrentState->Ping;
							}
							else
							if( CurrentState->Ping < 9999 )
	                            NewState.Ping = CurrentState->Ping;
						}

						// HACK keep master server's map capitalization
						if( NewState.MapName == CurrentState->MapName )
							NewState.MapName = CurrentState->MapName;

						NewState.Flags      = CurrentState->Flags;
						NewState.SkillLevel = CurrentState->SkillLevel;
						NewState.PlayerInfo = CurrentState->PlayerInfo;
						NewState.ServerInfo = CurrentState->ServerInfo;
						Actor->delegateOnReceivedPingInfo(OutstandingPings(i).ServerID, OutstandingPings(i).PingCause, NewState );
					}
					else
						Actor->delegateOnReceivedPingInfo( -1, PC_Unknown, NewState );
				}
				break;
			case QI_Rules:
				if( CurrentState )
				{
					TArray<FKeyValuePair> NewInfo;
					while( !ArRecv.AtEnd() && !ArRecv.IsError() )
					{
						INT i = NewInfo.AddZeroed();
						ArRecv << NewInfo(i);
					}

					UBOOL HadIP = 0;
					for(INT ci=0;ci<CurrentState->ServerInfo.Num();ci++ )
					{
						for( INT ni=0;ni<NewInfo.Num();ni++ )
						{
							if( CurrentState->ServerInfo(ci).Key==NewInfo(ni).Key && CurrentState->ServerInfo(ci).Value==NewInfo(ni).Value )
							{
								NewInfo.Remove(ni);
								ni--;
							}
						}

						if( CurrentState->ServerInfo(ci).Key == TEXT("IP") )
							HadIP = 1;
					}

					for( INT ni=0;ni<NewInfo.Num();ni++ )
					{
						INT ci=CurrentState->ServerInfo.AddZeroed();
						CurrentState->ServerInfo(ci).Key = NewInfo(ni).Key;
						CurrentState->ServerInfo(ci).Value = NewInfo(ni).Value;
					}

					// Add IP:Port to rules
					if( !HadIP )
					{
						INT ci=CurrentState->ServerInfo.AddZeroed();
						CurrentState->ServerInfo(ci).Key = TEXT("IP");
						CurrentState->ServerInfo(ci).Value = FString::Printf(TEXT("%s:%d"), *CurrentState->IP, CurrentState->Port);
					}

					Actor->delegateOnReceivedPingInfo(OutstandingPings(i).ServerID, OutstandingPings(i).PingCause, *CurrentState );
				}
				else
					GWarn->Logf(TEXT("Couldn't find outstanding request for rules from %s"), *SrcAddr.GetString(1) );
				break;
			case QI_Players:
				if( CurrentState )
				{
					TArray<FPlayerResponseLine> NewInfo;
					while( !ArRecv.AtEnd() && !ArRecv.IsError() )
					{
						INT i = NewInfo.AddZeroed();
						ArRecv << NewInfo(i);
					}

					for( INT np=0;np<NewInfo.Num();np++ )
					{
						for(INT cp=0;cp<CurrentState->PlayerInfo.Num();cp++ )
						{
							if( CurrentState->PlayerInfo(cp).PlayerNum==NewInfo(np).PlayerNum )
							{
								CurrentState->PlayerInfo.Remove(cp);
								cp--;
							}
						}
					}

					for( INT np=0;np<NewInfo.Num();np++ )
					{
						INT cp=CurrentState->PlayerInfo.AddZeroed();
						CurrentState->PlayerInfo(cp) = NewInfo(np);
					}

					Actor->delegateOnReceivedPingInfo(OutstandingPings(i).ServerID, OutstandingPings(i).PingCause, *CurrentState );
				}
				else
					GWarn->Logf(TEXT("Couldn't find outstanding request for players from %s"), *SrcAddr.GetString(1) );
				break;
			}
		}
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	AServerQueryClient
-----------------------------------------------------------------------------*/

void AServerQueryClient::Init()
{
	if( !LinkPtr )
    {
        if ((!bLANQuery) || (LANPort >= 0))
		    LinkPtr = (PTRINT)(new FServerQueryLink(this, bLANQuery ? LANPort : 0));
    }
}

void AServerQueryClient::execPingServer( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(ServerID);
	P_GET_BYTE(PingCause)
	P_GET_STR(IP);
	P_GET_INT(Port)
	P_GET_BYTE(Command);
	P_GET_STRUCT(FServerResponseLine,CurrentState);
	P_FINISH;

	Init();

	FIpAddr Addr( *IP, Port );
	if( IP == TEXT("BROADCAST") )
		Addr.Addr = INADDR_BROADCAST;

	((FServerQueryLink*)LinkPtr)->PingServer( ServerID, PingCause, Addr, Command, CurrentState );
}

void AServerQueryClient::execNetworkError( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	
	Init();
	*(DWORD*)Result = ((FServerQueryLink*)LinkPtr)->NetworkError() ? 1 : 0;
}

void AServerQueryClient::execCancelPings( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;
	Init();
	((FServerQueryLink*)LinkPtr)->CancelPings();
}

UBOOL AServerQueryClient::Poll( INT WaitTime )
{
	guard(AServerQueryClient::Poll);

	if( LinkPtr )
		((FServerQueryLink*)LinkPtr)->Poll();
	return 0;
	unguard;
}

void AServerQueryClient::Destroy()
{
	if( LinkPtr )
		delete ((FServerQueryLink*)LinkPtr);
	LinkPtr = 0;
	Super::Destroy();
}

void AServerQueryClient::PostScriptDestroyed()
{
	Super::PostScriptDestroyed();
	if( LinkPtr )
		delete ((FServerQueryLink*)LinkPtr);
	LinkPtr = 0;
}

IMPLEMENT_CLASS(AServerQueryClient);

/*-----------------------------------------------------------------------------
	FMasterServerClientLink
-----------------------------------------------------------------------------*/

#define DOWNLOADLISTFILE TEXT("AutoPatch.txt")

enum EMasterServerClientState
{
	MSCS_WaitingChallenge		= 0,
	MSCS_WaitingApproval		= 1,
	MSCS_SentQuery				= 2,
	MSCS_WaitQueryData			= 3,
	MSCS_WaitingReview			= 4,
};

class FMasterServerClientLink : public FTcpLink
{
	EMasterServerClientState ClientState;
	AMasterServerClient* Actor;
	INT ResultCount, ReceiveCount;
	BYTE ResultsCompressed;
	BYTE QueryType;

	FString						MasterServerName;
	INT							MasterServerPort;
public:
	FMasterServerClientLink( AMasterServerClient* InActor )
	:	FTcpLink()
	,	ClientState(MSCS_WaitingChallenge)
	,	Actor(InActor)
	,	ResultCount(-1)
	,	ReceiveCount(0)
	{
		SetLinkMode( TCPLINK_FArchive );
	}

	void StartQuery( BYTE InQueryType )
	{
		QueryType = InQueryType;
		Actor->eventGetMasterServer( MasterServerName, MasterServerPort );
		Resolve( *MasterServerName );
	}

	void OnResolved( FIpAddr a )
	{
		a.Port = MasterServerPort;
		GWarn->Logf(TEXT("Resolved %s -> %s"), *MasterServerName, *a.GetString(0) );
		Connect( a );
     }

	void OnConnectionSucceeded()
	{
		GWarn->Logf(TEXT("Connection established."));
	}

	void OnConnectionFailed()
	{

		if (!Actor)
			return;

		// Kind of hacky to put this here, but it saves having to setup a communication system
		// for the restart.

		GWarn->Logf(TEXT("Master Server Connection Failed!"));

		if (Actor->ActiveMasterServerList.Num()>0 && Actor->LastMSIndex < Actor->ActiveMasterServerList.Num() )
			Actor->ActiveMasterServerList.Remove(Actor->LastMSIndex,1);

		if (Actor->ActiveMasterServerList.Num()== 0)
			Actor->delegateOnQueryFinished(RI_ConnectionFailed,0);
		else
			StartQuery(QueryType);
	}

	void OnResolveFailed()
	{
		GWarn->Logf(TEXT("Failed to resolve '%s'"), *MasterServerName);
		Actor->delegateOnQueryFinished(RI_ConnectionFailed,0);
	}

	void OnClosed()
	{
		GWarn->Logf(TEXT("Connection closed!! %i %i"), ResultCount, ReceiveCount);
		if( ResultCount < 0 || ReceiveCount != ResultCount )
			Actor->delegateOnQueryFinished(RI_ConnectionTimeout,0);
	}

	void SaveDownloadSites( TArray<FString>& DownloadSites )
	{
		FString s;
		for( INT i=0;i<DownloadSites.Num();i++ )
			s = s + DownloadSites(i) + TEXT("\n");

		// Save AutoPatch.txt
		GFileManager->Delete( DOWNLOADLISTFILE, 0, 1 );
		appSaveStringToFile( s, DOWNLOADLISTFILE );
	}

	void OnDataReceived()
	{
		while( DataAvailable() )
		{
			switch( ClientState )
			{
			case MSCS_WaitingChallenge:
				{
					FString Challenge;
					*ArRecv << Challenge;
					FString CDKeyHash, Response, ClientType;
					CDKeyHash = GetCDKeyHash();
					Response = GetCDKeyResponse( *Challenge ) ;
					ClientType = TEXT("UT2K4CLIENT");
					INT Version = ENGINE_VERSION;
					BYTE Platform = GRunningOS;
					FString Language = UObject::GetLanguage();
					*ArSend << CDKeyHash << Response << ClientType << Version << Platform << Language;
					
					INT		CPUSpeed	= appRound(0.00000001 / GSecondsPerCycle), // in 100 MHz
							GPUDeviceID	= GGPUDeviceID,
							GPUVendorID	= GGPUVendorID;
					BYTE	CPUType		= GRunningCPU;
					*ArSend << GPUDeviceID << GPUVendorID << CPUSpeed << CPUType;

					ArSend->Flush();
					ClientState = MSCS_WaitingReview;
				}
				break;
			case MSCS_WaitingReview:
				{
					FString Results;
					*ArRecv << Results;

					if ( Results != TEXT("APPROVED") )
					{
							if ( Results == TEXT("UNKNOWN_CDKEY") )	
							{
								Actor->delegateOnQueryFinished(RI_AuthenticationFailed,0);
								Close();
							}
							else if( Results == TEXT("MODIFIED_CLIENT") )
							{
								Actor->delegateOnQueryFinished(RI_BadClient,0);
								Close();
							}
							else if( Results == TEXT("DEV_CLIENT") )
							{
								Actor->delegateOnQueryFinished(RI_DevClient,0);
								Close();
							}
							else if( Results == TEXT("UTAN_BAN") )
							{
								Actor->delegateOnQueryFinished(RI_UTANBan,0);
								Close();
							}
							else if( Results == TEXT("BANNED_CLIENT" ) )
							{
								*ArRecv << Actor->OptionalResult;
								Actor->delegateOnQueryFinished(RI_BannedClient,0);
								Close();
							}
							else if( Results == TEXT("NEED_UPGRADE") )
							{
								INT UpgradeVersion;
								TArray<FString> DownloadSites;
								FString MOTD;
								*ArRecv << UpgradeVersion << DownloadSites << MOTD;
								SaveDownloadSites(DownloadSites);

								AMasterServerClient* MSClient = Actor;

								// Delegates give unrealscript access to protected members - might invalidate Actor pointer
								if (MSClient)
								{
									MSClient->delegateOnReceivedMOTDData( MR_MandatoryUpgrade, FString::Printf(TEXT("%d"), UpgradeVersion) );
									MSClient->delegateOnReceivedMOTDData( MR_MOTD, MOTD );
									MSClient->delegateOnQueryFinished(RI_MustUpgrade, UpgradeVersion);
									Close();
								}
							}
							else if( Results == TEXT("MSUPDATE") )
							{
								Actor->ActiveMasterServerList.Empty();
								Actor->LastMSIndex=-1;


								Actor->MasterServerList.Empty();
								INT ListCnt;
								FString Addr;
								INT Port;

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

								ClientState= MSCS_WaitingChallenge;
								StartQuery(QueryType);
								return;
							}


						else
						{
							// master server busy, or i didn't understand its response
							Actor->delegateOnQueryFinished(RI_ConnectionFailed,0);
							Close();
						}
					}
					else
					{

						*ArRecv << Actor->ModRevLevel;

						FString GlobalMD5;
						for (INT i=0; i<16; i++)
							GlobalMD5 += FString::Printf(TEXT("%02x"), GMD5[i]);	

						*ArSend << GlobalMD5;
						ArSend->Flush();

						ClientState = MSCS_WaitingApproval;
					}
				}
				break;
			case MSCS_WaitingApproval:
				{
					FString Results;
					*ArRecv << Results;
					if( Results == TEXT("VERIFIED") )
					{
						*ArSend << QueryType;
						switch( QueryType )
						{
						case CTM_Query:
							*ArSend << Actor->Query;
							break;
						case CTM_GetMOTD:
							break;
						case CTM_GetModMOTD:
							*ArSend << Actor->OwnageLevel;
							break;
						}
						ArSend->Flush();
						ClientState = MSCS_SentQuery;
					}
					else
					{
						Actor->delegateOnQueryFinished(RI_BadClient,0);
						Close();
					}
				}
				break;
			case MSCS_SentQuery:
				{
					switch( QueryType )
					{
					case CTM_Query:
						{
							*ArRecv << ResultCount << ResultsCompressed;
							Actor->ResultCount = ResultCount;
							if( ResultCount > 0 )
								ClientState = MSCS_WaitQueryData;
							else
							{
								Actor->delegateOnQueryFinished(RI_Success,QueryType);
								Close();
							}
						}
						break;
					case CTM_GetMOTD:
						{
							INT UpgradeVersion = 0;
							FString MOTD;
							*ArRecv << MOTD << UpgradeVersion;
							Actor->delegateOnReceivedMOTDData( MR_MOTD, MOTD );
							if( UpgradeVersion )
							{
								TArray<FString> DownloadSites;
								*ArRecv << DownloadSites;
								SaveDownloadSites(DownloadSites);
								Actor->delegateOnReceivedMOTDData( MR_OptionalUpgrade, FString::Printf(TEXT("%d"), UpgradeVersion) );
							}
							Actor->delegateOnQueryFinished(RI_Success,QueryType);
							Close();
						}
						break;
					case CTM_GetModMOTD:
						{	
							BYTE PacketType;
							*ArRecv << PacketType;

							if ( PacketType == 0 )		// Mod MOTD
							{
								FString ModMOTD;
								*ArRecv << ModMOTD;
								Actor->delegateOnReceivedModMOTDData(ModMOTD);
							}

							else if (PacketType == 1)	// Ownage Entry
							{

								INT		RevisionLevel, OwnagePacketType;
								*ArRecv << OwnagePacketType << RevisionLevel;

								switch (OwnagePacketType)
								{
								case 0:			//	Header
									{
										FString MapName;
										*ArRecv << MapName;
								
										GWarn->Logf(TEXT("Received Ownage Map Header: %i [%s]"),RevisionLevel,*MapName);
										Actor->delegateOnReceivedOwnageItem(RevisionLevel,MapName,TEXT(""),TEXT(""));
									}
									break;

								case 1:			// Description
									{
										FString MapDesc;
										*ArRecv << MapDesc;
										GWarn->Logf(TEXT("Received Ownage Map Body: %i [%s]"),RevisionLevel,*MapDesc);
										Actor->delegateOnReceivedOwnageItem(RevisionLevel,TEXT(""),MapDesc,TEXT(""));
									}
									break;

								case 2:			// URL
									{
										FString MapURL;
										*ArRecv << MapURL;
										GWarn->Logf(TEXT("Received Ownage Map URL: %i [%s]"),RevisionLevel,*MapURL);
										Actor->delegateOnReceivedOwnageItem(RevisionLevel,TEXT(""),TEXT(""),MapURL);
									}
									break;
								}
							}
							else	// End of Packet Sends
							{
								Actor->delegateOnQueryFinished(RI_Success,QueryType);
								Close();
							}
					}
					break;
				}
				break;
			case MSCS_WaitQueryData:
				{
					if( ReceiveCount < ResultCount )
					{
						FServerResponseLine Server;
						appMemzero( &Server, sizeof(FServerResponseLine) );
						ReceiveCount++;
						if( ResultsCompressed )
						{
                            // decode compressed results.
							DWORD IP;
							_WORD Port;
							_WORD QueryPort;
							BYTE CurrentPlayers;
							BYTE MaxPlayers;
							FString SkillLevel;
							INT ServerFlags=0;

							*ArRecv << IP << Port << QueryPort;
							if( ArRecv->IsError() )
							{
								Close();
								return;
							}

							// argh...this is in 'host order', which is little
							//  endian on intel and needs special swapping
							//  for PowerPC, etc.  --ryan.
							IP = INTEL_ORDER32(IP);

							*ArRecv << Server.ServerName;
							if( Server.ServerName.Len() == 25 )
								Server.ServerName = Server.ServerName + TEXT("....");

							if( ArRecv->IsError() )
							{
								Close();
								return;
							}
							*ArRecv << Server.MapName;

							if( ArRecv->IsError() )
							{
								Close();
								return;
							}
							*ArRecv << Server.GameType
									<< CurrentPlayers
									<< MaxPlayers
									<< ServerFlags
									<< SkillLevel;

							INT j = Server.MapName.InStr(TEXT("-"));
							UBOOL HasPrefix = (j==2 || j==3);
							if( Server.GameType == TEXT("0") )
							{
								Server.GameType = TEXT("xDeathMatch");
								if( !HasPrefix )
									Server.MapName = FString(TEXT("DM-")) + Server.MapName;
							}
							else
							if( Server.GameType == TEXT("1") )
							{
								Server.GameType = TEXT("xCTFGame");
								if( !HasPrefix )
									Server.MapName = FString(TEXT("CTF-")) + Server.MapName;
							}
							else
							if( Server.GameType == TEXT("2") )
							{
								Server.GameType = TEXT("xBombingRun");
								if( !HasPrefix )
									Server.MapName = FString(TEXT("BR-")) + Server.MapName;
							}
							else
							if( Server.GameType == TEXT("3") )
							{
								Server.GameType = TEXT("xTeamGame");
								if( !HasPrefix )
									Server.MapName = FString(TEXT("DM-")) + Server.MapName;
							}
							else
							if( Server.GameType == TEXT("4") )
							{
								Server.GameType = TEXT("xDoubleDom");
								if( !HasPrefix )
									Server.MapName = FString(TEXT("DOM-")) + Server.MapName;
							}

							FIpAddr Addr;
							Addr.Addr = ntohl(IP);
							Server.IP = Addr.GetString(0);
							Server.Port = Port;
							Server.QueryPort = QueryPort;
							Server.CurrentPlayers = CurrentPlayers;
							Server.MaxPlayers = MaxPlayers;
							Server.Flags = ServerFlags;
							Server.SkillLevel = SkillLevel;

						}
						else
							*ArRecv << Server;

						Actor->delegateOnReceivedServer(Server);
					}

					if( ReceiveCount == ResultCount )
					{
						Actor->delegateOnQueryFinished(RI_Success,QueryType);
						Close();
					}
					break;
				}
			}
			}
		}
	}
};

/*-----------------------------------------------------------------------------
	AMasterServerLink
-----------------------------------------------------------------------------*/
void AMasterServerLink::execPoll( FFrame& Stack, RESULT_DECL )
{
	guard(AMasterServerLink::execPoll);
	P_GET_INT(WaitTime);
	P_FINISH;
	*(DWORD*)Result = Poll(WaitTime);
	unguard;
}

IMPLEMENT_CLASS(AMasterServerLink);

/*-----------------------------------------------------------------------------
	AMasterServerClient
-----------------------------------------------------------------------------*/
void AMasterServerClient::Init()
{
	Super::Init();

	// Make a new link to the master server
	if( MSLinkPtr )
		delete ((FMasterServerClientLink*)MSLinkPtr);
    MSLinkPtr = (PTRINT)(new FMasterServerClientLink(this));
}

UBOOL AMasterServerClient::Poll( INT WaitTime )
{
	guard(AMasterServerClient::Poll);
	Super::Poll( WaitTime );

#ifdef __EMSCRIPTEN__
	// WebRTC discovery results -> the Internet/master browser tab. Row building lives
	// in WebRTCDrv (WebRTCDiscovery_Drain); we just fire the stock delegates.
	if( GWebRTCDiscoveryPending && GActiveWebRTCBrowser == this && WebRTCDiscovery_Ready() )
	{
		TArray<FServerResponseLine> Servers;
		WebRTCDiscovery_Drain( Servers );
		for( INT i = 0; i < Servers.Num(); i++ )
			delegateOnReceivedServer( Servers(i) );
		debugf( NAME_DevNet, TEXT("[WebRTC-Discovery] OnQueryFinished(RI_Success), dispatched %d servers"), Servers.Num() );
		delegateOnQueryFinished(RI_Success, 0);
		GWebRTCDiscoveryPending = 0;
		GActiveWebRTCBrowser = NULL;
	}
#else
	if( MSLinkPtr )
		return ((FMasterServerClientLink*)MSLinkPtr)->Poll(WaitTime);
#endif

	return 0;
	unguard;
}

void AMasterServerClient::Destroy()
{
	if( MSLinkPtr )
		delete ((FMasterServerClientLink*)MSLinkPtr);
	MSLinkPtr = 0;
	Super::Destroy();
}

void AMasterServerClient::PostScriptDestroyed()
{
	Super::PostScriptDestroyed();
	if( MSLinkPtr )
		delete ((FMasterServerClientLink*)MSLinkPtr);
	MSLinkPtr = 0;
}

void AMasterServerClient::execStartQuery( FFrame& Stack, RESULT_DECL )
{
	P_GET_BYTE(QueryType)
	P_FINISH;

#ifdef __EMSCRIPTEN__
	// On Emscripten, use WebRTC discovery via PeerJS peer list instead of
	// connecting to the Epic master server. The room name is read from
	// localStorage (set by config-form.js) or defaults to "default".
	// Room name is sanitized to alphanumeric + dash only (T-10-13).
	char roomBuf[128];
	EM_ASM({
		var room = '';
		if (typeof localStorage !== 'undefined') {
			room = localStorage.getItem('ut2004_net_room') || '';
		}
		if (!room) room = 'default';
		// Sanitize: alphanumeric and dash only (T-10-13)
		room = room.replace(/[^a-zA-Z0-9\-]/g, '');
		if (!room) room = 'default';
		stringToUTF8(room, $0, $1);
	}, roomBuf, sizeof(roomBuf));

	ResultCount = 0;
	GActiveWebRTCBrowser = this;
	GWebRTCDiscoveryPending = 1;
	debugf( NAME_DevNet, TEXT("[WebRTC-Discovery] StartQuery: room='%s', querying PeerJS for servers..."), ANSI_TO_TCHAR(roomBuf) );
	WebRTCDiscovery_Begin(roomBuf);
#else
	Init();
    ResultCount = 0; // gam
	((FMasterServerClientLink*)MSLinkPtr)->StartQuery(QueryType);
#endif
}

void AMasterServerClient::execStop( FFrame& Stack, RESULT_DECL )
{
	P_FINISH;

#ifdef __EMSCRIPTEN__
	GWebRTCDiscoveryPending = 0;
	GActiveWebRTCBrowser = NULL;
#else
	if( MSLinkPtr )
		delete ((FMasterServerClientLink*)MSLinkPtr);
	MSLinkPtr = 0;
#endif
}

void AMasterServerClient::execLaunchAutoUpdate( FFrame& Stack, RESULT_DECL )
{
	guard(AMasterServerClient::execLaunchAutoUpdate);
	P_FINISH;

	// Verify that we have a download location before closing this process
	if ( GFileManager->FileSize( DOWNLOADLISTFILE ) <= 0 )
	{
		debugf(NAME_Error, TEXT("Failed to launch auto-updater! No valid download locations could be found!"));
		return;
	}

#if (defined WIN32)
    appLaunchURL( TEXT("Setup.exe"), TEXT("autopatch") );
#elif (defined __linux__)
    GUnixSpawnOnExit = TEXT("../updater/update");
#endif

	appRequestExit(0);
	unguard;
}

IMPLEMENT_CLASS(AMasterServerClient);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/


