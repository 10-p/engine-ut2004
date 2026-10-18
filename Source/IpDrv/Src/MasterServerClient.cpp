/*============================================================================
	MasterServerClient.cpp - master server client

	Revision history:
		* Created by Jack Porter
============================================================================*/

#include "UnIpDrv.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>

// WebRTC server discovery lives in WebRTCDrv (WebRTCDiscovery.cpp): the host-glue calls and the
// FServerResponseLine building are kept out of the stock IpDrv query path. Resolved at link time;
// FServerResponseLine is an Engine type, so this is a one-way IpDrv -> WebRTCDrv dependency.
extern void  WebRTCDiscovery_Begin( const char* room );
extern UBOOL WebRTCDiscovery_Ready();
extern INT   WebRTCDiscovery_Drain( TArray<FServerResponseLine>& Out );

// Active browser actor for WebRTC discovery callback firing
static AMasterServerClient* GActiveWebRTCBrowser = NULL;
static UBOOL GWebRTCDiscoveryPending = 0;

// Proxy networking: the in-game Internet browser's list comes from the game proxy (WebServerList.h), and
// the browser's own pings are answered from the same rows. Every query type shares one fetch; StartQuery
// only records the request and Poll() asks, never a script call.
#include "WebServerList.h"
FWebServerListHooks* GWebServerList = NULL;

struct FProxyListRequest
{
	AMasterServerClient* Browser;
	BYTE QueryType;
};
static TArray<FProxyListRequest> GProxyRequests;	// waiting for the list
static INT    GProxyListHandle = -1;					// -1 until Poll() opens it
static DOUBLE GProxyNextPoll = 0.0;
static TArray<FServerResponseLine> GProxyRows;		// the last list: what the pings are answered from
static INT    GProxyPingMs = -1;					// the player -> proxy round trip; -1 = unknown
static UBOOL  GProxyPingsLogged = 0;				// one line per list: the first ping the list answered

static UBOOL ProxyListActive()
{
	return GWebServerList && GWebServerList->Active && GWebServerList->Active();
}
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
	// Pings answered from the game proxy's list, delivered after the proxy's round trip.
	struct FProxyPingReply
	{
		INT  ServerID;
		BYTE PingCause;
		DOUBLE Due;
		FServerResponseLine State;
	};
	TArray<FProxyPingReply> ProxyReplies;
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
		// the stock LAN browser lists them (it dedupes by IP+Port; IP = the room's synthetic address).
		// The row building lives in WebRTCDrv (WebRTCDiscovery_Drain).
		if( bWebRTCBroadcastPending && WebRTCDiscovery_Ready() )
		{
			bWebRTCBroadcastPending = 0;
			TArray<FServerResponseLine> Servers;
			WebRTCDiscovery_Drain( Servers );
			for( INT i = 0; i < Servers.Num(); i++ )
				Actor->delegateOnReceivedPingInfo( WebRTCServerID, WebRTCPingCause, Servers(i) );
		}

		// The proxy-answered pings that are due (PingServer queued them).
		for( INT r = 0; r < ProxyReplies.Num(); r++ )
		{
			if( ProxyReplies(r).Due > CurrentTime )
				continue;
			FProxyPingReply Reply = ProxyReplies(r);	// a copy: the delegate may ping again
			ProxyReplies.Remove(r--);
			Actor->delegateOnReceivedPingInfo( Reply.ServerID, Reply.PingCause, Reply.State );
		}
#endif

		FUdpLink::Poll();
	}
	void PingServer( INT ServerID, BYTE PingCause, FIpAddr Addr, BYTE Command, FServerResponseLine& CurrentState )
	{
		guard(FServerQueryLink::PingServer);

#ifdef __EMSCRIPTEN__
		// LAN broadcast has no UDP in the browser: kick WebRTC discovery (the peer list + per-server
		// status ping); Poll() emits the results via OnReceivedPingInfo. The UDP send below is skipped.
		if( Addr.Addr == INADDR_BROADCAST )
		{
			WebRTCDiscovery_Begin( "" );  // all rooms; Poll() drains the results
			bWebRTCBroadcastPending = 1;
			WebRTCServerID  = ServerID;   // -1 for a broadcast request
			WebRTCPingCause = PingCause;  // PC_LANBroadcast
			return;
		}

		// Proxy networking: no UDP here either. A server from the game proxy's list is answered from that
		// list (a full row, as a real reply is: the browser replaces the whole row with it), after the
		// proxy's own round trip, so the ping shown is the player -> proxy leg. The stock browser pings only
		// when GUIController MaxSimultaneousPings > 0: at the web's netspeed (1000000, above every threshold)
		// CalculateMaxConnections() returns Min(MaxSimultaneousPings, 35), so DefUser.ini sets it to 35; a
		// User.ini without that key reads 0.
		if( ProxyListActive() && GProxyPingMs >= 0 )
		{
			for( INT r = 0; r < GProxyRows.Num(); r++ )
			{
				FIpAddr RowAddr( *GProxyRows(r).IP, GProxyRows(r).QueryPort );
				if( !(RowAddr == Addr) )
					continue;
				INT i = ProxyReplies.AddZeroed();
				ProxyReplies(i).ServerID  = ServerID;
				ProxyReplies(i).PingCause = PingCause;
				ProxyReplies(i).Due       = appSeconds() + GProxyPingMs / 1000.0;
				ProxyReplies(i).State     = GProxyRows(r);
				ProxyReplies(i).State.ServerID = ServerID;
				ProxyReplies(i).State.Ping     = Max( GProxyPingMs, 1 );
				if( !GProxyPingsLogged )
				{
					GProxyPingsLogged = 1;
					GLog->Logf( TEXT("ProxyNet: the in-game browser's pings are answered from the list (first: %s:%i)"), *GProxyRows(r).IP, GProxyRows(r).QueryPort );
				}
				return;
			}
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
#ifdef __EMSCRIPTEN__
		ProxyReplies.Empty();
#endif
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

#ifdef __EMSCRIPTEN__
/*-----------------------------------------------------------------------------
	The in-game Internet browser from the game proxy's list (WebServerList.h).
-----------------------------------------------------------------------------*/

// The list is UTF-8 (it is the page's own text), and TCHAR is 8 bits on the web (16 in a UNICODE build): each
// character is decoded, never copied byte by byte (ANSI_TO_TCHAR would turn a name's "»" into two characters).
// A code point this TCHAR cannot hold becomes '?', as the engine's own narrowing does; so does a broken sequence.
static FString ProxyFromUtf8( const ANSICHAR* In )
{
	const DWORD MaxChar = sizeof(TCHAR) == 1 ? 0xFF : 0xFFFF;
	TArray<TCHAR> Out;
	for( const BYTE* P = (const BYTE*)In; *P; )
	{
		DWORD C = *P++;
		INT More = 0;
		if( C >= 0x80 )
		{
			if( C >= 0xC0 && C < 0xE0 )			{ C &= 0x1F; More = 1; }
			else if( C >= 0xE0 && C < 0xF0 )	{ C &= 0x0F; More = 2; }
			else if( C >= 0xF0 && C < 0xF8 )	{ C &= 0x07; More = 3; }
			else								C = '?';	// a stray continuation byte, or no UTF-8 lead at all
		}
		for( ; More > 0; More-- )
		{
			if( (*P & 0xC0) != 0x80 )
			{
				C = '?';	// cut short: the next byte starts a character of its own
				break;
			}
			C = (C << 6) | (*P++ & 0x3F);
		}
		Out.AddItem( (TCHAR)(C == 0 || C > MaxChar ? '?' : C) );
	}
	Out.AddItem( 0 );
	return FString( &Out(0) );
}

// "#proxynet-list 1 ping=<ms>" then one row per line: host TAB port TAB name TAB map TAB gametype
// TAB players TAB maxplayers TAB version TAB minversion TAB password. Empty fields keep their column.
static void ProxyParseList( ANSICHAR* Text )
{
	GProxyRows.Empty();
	GProxyPingMs = -1;
	for( ANSICHAR* Line = Text; *Line; )
	{
		ANSICHAR* End = Line;
		while( *End && *End != '\n' )
			End++;
		UBOOL Last = (*End == 0);
		*End = 0;
		if( Line[0] == '#' )
		{
			const ANSICHAR* P = strstr( Line, "ping=" );
			if( P )
				GProxyPingMs = atoi( P + 5 );
		}
		else if( Line[0] )
		{
			ANSICHAR* F[10];
			INT N = 0;
			F[N++] = Line;
			for( ANSICHAR* C = Line; *C && N < 10; C++ )
				if( *C == '\t' )
				{
					*C = 0;
					F[N++] = C + 1;
				}
			if( N == 10 && F[0][0] && atoi(F[1]) > 0 )
			{
				INT i = GProxyRows.AddZeroed();
				FServerResponseLine& S = GProxyRows(i);
				S.ServerID       = i;
				S.IP             = ANSI_TO_TCHAR(F[0]);
				S.Port           = atoi( F[1] );
				S.QueryPort      = S.Port + 1;	// the proxy's rows carry no query port; +1 is the game's own rule
				S.ServerName     = F[2][0] ? ProxyFromUtf8(F[2]) : FString::Printf( TEXT("%s:%i"), *S.IP, S.Port );
				S.MapName        = ProxyFromUtf8(F[3]);
				S.GameType       = ProxyFromUtf8(F[4]);
				S.CurrentPlayers = Max( atoi(F[5]), 0 );
				S.MaxPlayers     = Max( atoi(F[6]), 0 );
				S.Ping           = 0;
				S.Flags          = atoi( F[9] ) ? 1 : 0;	// 1 = passworded (the browser's icon legend)
			}
		}
		if( Last )
			break;
		Line = End + 1;
	}
}

// The browser's filters that the list can honour: the game type (the rest need a real query).
static UBOOL ProxyRowWanted( AMasterServerClient* Browser, const FServerResponseLine& S )
{
	for( INT i = 0; i < Browser->Query.Num(); i++ )
	{
		const FQueryData& Q = Browser->Query(i);
		// A buddy query asks for the servers a person is on: the list cannot know that.
		if( Q.Key == TEXT("buddy") )
			return 0;
		if( Q.Key != TEXT("gametype") )
			continue;
		UBOOL Same = (S.GameType == Q.Value);
		if( (Q.QueryType == QT_Equals && !Same) || (Q.QueryType == QT_NotEquals && Same) )
			return 0;
	}
	return 1;
}

static void ProxyFinishQuery( AMasterServerClient* Browser, BYTE QueryType, UBOOL Ok )
{
	if( !Ok )
	{
		// No proxy chosen ("Start with proxying disabled"), or it could not be reached. For the news
		// query this still unlocks the Internet tab (MOTDVerified(false)), where it then says so.
		Browser->delegateOnQueryFinished( RI_ConnectionFailed, QueryType );
		return;
	}
	if( QueryType == CTM_GetMOTD )
	{
		// This answer is what unlocks the Internet tab.
		Browser->delegateOnReceivedMOTDData( MR_MOTD, FString::Printf(
			TEXT("Proxy networking (experimental): the Internet list comes from this site's game proxy -- %i servers. Their ping is to the proxy only."),
			GProxyRows.Num() ) );
		Browser->delegateOnQueryFinished( RI_Success, QueryType );
		return;
	}
	if( QueryType == CTM_Query )
	{
		Browser->ResultCount = 0;
		for( INT i = 0; i < GProxyRows.Num(); i++ )
			if( ProxyRowWanted( Browser, GProxyRows(i) ) )
				Browser->ResultCount++;
		for( INT i = 0; i < GProxyRows.Num(); i++ )
			if( ProxyRowWanted( Browser, GProxyRows(i) ) )
				Browser->delegateOnReceivedServer( GProxyRows(i) );
	}
	// An upgrade check, the mod news, the ownage list: nothing to say, and nothing wrong.
	Browser->delegateOnQueryFinished( RI_Success, QueryType );
}

// Called from the browsers' Poll(): open the list, wait for it, then answer every waiting request.
// At most four sync calls a second: a per-frame proxied import can overflow Emscripten's proxy queue.
static void ProxyPollList()
{
	if( !GProxyRequests.Num() || appSeconds() < GProxyNextPoll )
		return;
	GProxyNextPoll = appSeconds() + 0.25;
	UBOOL Ok = 0;
	if( GProxyListHandle < 0 )
	{
		GProxyListHandle = GWebServerList->Open();
		if( GProxyListHandle >= 0 )
			return;
	}
	else
	{
		INT Status = WEBLIST_Pending, Count = 0;
		if( GWebServerList->Poll( GProxyListHandle, &Status, &Count ) < 0 )
			Status = WEBLIST_Failed;
		if( Status == WEBLIST_Pending )
			return;
		if( Status == WEBLIST_Done )
		{
			INT Need = GWebServerList->Read( GProxyListHandle, NULL, 0 );
			if( Need > 0 )
			{
				TArray<BYTE> Blob;
				Blob.Add( Need + 1 );
				if( GWebServerList->Read( GProxyListHandle, &Blob(0), Need ) == Need )
				{
					Blob(Need) = 0;
					ProxyParseList( (ANSICHAR*)&Blob(0) );
					GProxyPingsLogged = 0;
					Ok = 1;
				}
			}
		}
		GWebServerList->Close( GProxyListHandle );
		GProxyListHandle = -1;
	}
	GLog->Logf( TEXT("ProxyNet: the in-game server browser's list from the game proxy: %s, %i servers"),
		Ok ? TEXT("ok") : TEXT("not available"), Ok ? GProxyRows.Num() : 0 );
	// A copy, cleared first: a delegate may start the next query.
	TArray<FProxyListRequest> Requests = GProxyRequests;
	GProxyRequests.Empty();
	for( INT i = 0; i < Requests.Num(); i++ )
		ProxyFinishQuery( Requests(i).Browser, Requests(i).QueryType, Ok );
}

static void ProxyForgetBrowser( AMasterServerClient* Browser )
{
	for( INT i = 0; i < GProxyRequests.Num(); i++ )
		if( GProxyRequests(i).Browser == Browser )
			GProxyRequests.Remove(i--);
}
#endif

UBOOL AMasterServerClient::Poll( INT WaitTime )
{
	guard(AMasterServerClient::Poll);
	Super::Poll( WaitTime );

#ifdef __EMSCRIPTEN__
	// The game proxy's list, for every request StartQuery recorded.
	if( ProxyListActive() )
		ProxyPollList();

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
#ifdef __EMSCRIPTEN__
	ProxyForgetBrowser( this );
#endif
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
	// Proxy networking: every query is answered from the game proxy's list. Only record the request
	// here; Poll() asks (never a sync import from inside a script call).
	if( ProxyListActive() )
	{
		ProxyForgetBrowser( this );
		INT i = GProxyRequests.AddZeroed();
		GProxyRequests(i).Browser   = this;
		GProxyRequests(i).QueryType = QueryType;
		ResultCount = 0;
		return;
	}

	// Use WebRTC discovery (the room's peer list) instead of connecting to the master server. The room
	// name is read from localStorage['ut2004_net_room'], "default" when it is absent, and sanitized to
	// alphanumerics and dashes.
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
	ProxyForgetBrowser( this );
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


