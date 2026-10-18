/*=============================================================================
	DemoRecDrv.cpp: Unreal demo recording network driver.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jack Porter.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "Core.h"
#define PACKETSIZE 512

/*-----------------------------------------------------------------------------
	UDemoRecConnection.
-----------------------------------------------------------------------------*/

void UDemoRecConnection::StaticConstructor()
{
	guard(UDemoRecConnection::StaticConstructor);
	unguard;
}
UDemoRecConnection::UDemoRecConnection( UNetDriver* InDriver, const FURL& InURL )
: UNetConnection( InDriver, InURL )
{
	guard(UDemoRecConnection::UDemoRecConnection);
	MaxPacket   = PACKETSIZE;
	InternalAck = 1;

	unguard;
}
UDemoRecDriver* UDemoRecConnection::GetDriver()
{
	return (UDemoRecDriver *)Driver;
}
FString UDemoRecConnection::LowLevelGetRemoteAddress()
{
	guard(UDemoRecConnection::LowLevelGetRemoteAddress);
	return TEXT("");
	unguard;
}
void UDemoRecConnection::LowLevelSend( void* Data, INT Count )
{
	guard(UDemoRecConnection::LowLevelSend);
	if( !GetDriver()->ServerConnection )
	{
		*GetDriver()->FileAr << GetDriver()->LastDeltaTime << GetDriver()->FrameNum << Count;
		GetDriver()->FileAr->Serialize( Data, Count );
		//!!if GetDriver()->GetFileAr()->IsError(), print error, cancel demo recording
	}
	unguard;
}
FString UDemoRecConnection::LowLevelDescribe()
{
	guard(UDemoRecConnection::Describe);
	return TEXT("Demo recording driver connection");
	unguard;
}
INT UDemoRecConnection::IsNetReady( UBOOL Saturate )
{
	return 1;
}
void UDemoRecConnection::FlushNet()
{
	// in playback, there is no data to send except
	// channel closing if an error occurs.
	if( !GetDriver()->ServerConnection )
		Super::FlushNet();
}
void UDemoRecConnection::HandleClientPlayer( APlayerController* PC )
{
	guard(UDemoRecConnection::HandleClientPlayer);
	Super::HandleClientPlayer(PC);
	PC->bDemoOwner = 1;
	unguard;
}
IMPLEMENT_CLASS(UDemoRecConnection);

/*-----------------------------------------------------------------------------
	UDemoRecDriver.
-----------------------------------------------------------------------------*/

UDemoRecDriver::UDemoRecDriver()
{}
UBOOL UDemoRecDriver::InitBase( UBOOL Connect, FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error )
{
	guard(UDemoRecDriver::Init);

	DemoFilename   = ConnectURL.Map;
	Time           = 0;
	FrameNum       = 0;
	DemoEnded      = 0;

	// Zero the frame-time distribution here as well as at the start of timing: the reporting block is
	// guarded on FrameTimeSamples, and a demo that ends before precaching finishes never reaches the
	// timing-start branch. UObject memory is zeroed on construction, but a looped demo reuses the
	// driver, and without this the second run would report the first run's tail.
	FrameTimeSamples    = 0;
	WorstFrameMs        = 0.0;
	LastTimedFrameStart = 0.0;
	for( INT i=0; i<DEMO_FT_BUCKETS; i++ )
		FrameTimeBuckets[i] = 0;

	return 1;
	unguard;
}
UBOOL UDemoRecDriver::InitConnect( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error )
{
	guard(UDemoRecDriver::InitConnect);
	if( !Super::InitConnect( InNotify, ConnectURL, Error ) )
		return 0;
	if( !InitBase( 1, InNotify, ConnectURL, Error ) )
		return 0;

	// Playback, local machine is a client, and the demo stream acts "as if" it's the server.
	ServerConnection = new UDemoRecConnection( this, ConnectURL );
	ServerConnection->CurrentNetSpeed = 1000000;
	ServerConnection->State        = USOCK_Pending;
	FileAr                         = GFileManager->CreateFileReader( *DemoFilename );
	if( !FileAr )
	{
		Error = FString::Printf( TEXT("Couldn't open demo file %s for reading"), *DemoFilename );//!!localize!!
		return 0;
	}

	// Skip the new Demo Info Header

	FileAr->Seek(17);
	INT HeaderCheck;
	*FileAr << HeaderCheck;

	if (HeaderCheck != 1038)
	{
		delete FileAr;
		FileAr = NULL;
		Error = FString::Printf( TEXT("Incompatible Demo file (probably an earlier version)"), *DemoFilename );//!!localize!!
		return 0;
	}

	// Skip the headers 

	FString Str;
	INT    I;

	*FileAr << Str;
	*FileAr << Str;
	*FileAr << I;
	*FileAr << I;
	*FileAr << I;
	*FileAr << Str;
	*FileAr << Str;

	FString FileName;
	FGuid	GUID;
	INT		PkgCount, Gen;

	*FileAr << PkgCount;

	for (INT i=0;i<PkgCount;i++)
	{
		*FileAr << FileName;
		*FileAr << GUID;
		*FileAr << Gen;
	}

	LoopURL = ConnectURL;
	NoFrameCap          = ConnectURL.HasOption(TEXT("timedemo"));
	Loop				= ConnectURL.HasOption(TEXT("loop"));

	LastFrameTime = appSeconds();

	return 1;
	unguard;
}
UBOOL UDemoRecDriver::InitListen( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error )
{
	guard(UDemoRecDriver::InitListen);
	if( !Super::InitListen( InNotify, ConnectURL, Error ) )
		return 0;
	if( !InitBase( 0, InNotify, ConnectURL, Error ) )
		return 0;

	class ALevelInfo* LevelInfo = GetLevel()->GetLevelInfo();
	if ( !LevelInfo )
	{
		Error = TEXT("No LevelInfo!!");
		return 0;
	}

	// Recording, local machine is server, demo stream acts "as if" it's a client.
	UDemoRecConnection* Connection = new UDemoRecConnection( this, ConnectURL );
	Connection->CurrentNetSpeed   = 1000000;
	Connection->State             = USOCK_Open;
	Connection->InitOut();

	FileAr = GFileManager->CreateFileWriter( *DemoFilename );
	ClientConnections.AddItem( Connection );

	if( !FileAr )
	{
		Error = FString::Printf( TEXT("Couldn't open demo file %s for writing"), *DemoFilename );//localize!!
		return 0;
	}

	// Setup

	UGameEngine* GameEngine = CastChecked<UGameEngine>( GetLevel()->Engine );
	INT ClientDemo = INT(GetLevel()->GetLevelInfo()->NetMode == NM_Client);

	// Build package map.

	if( ClientDemo)
		MasterMap->CopyLinkers( GetLevel()->NetDriver->ServerConnection->PackageMap );
	else
	{
		GameEngine->BuildServerMasterMap( this, GetLevel() );
		UBOOL bNeedsReCompute=false;
		for (int pkg=0;pkg<MasterMap->List.Num();pkg++)
		{
			if ( MasterMap->List(pkg).URL.Right(4) == TEXT(".utx") )
			{
				GWarn->Logf(TEXT("Fixing up %s from %i to 1"),*MasterMap->List(pkg).URL,MasterMap->List(pkg).RemoteGeneration);
				MasterMap->List(pkg).RemoteGeneration = 1;
				bNeedsReCompute=true;
			}

			if (bNeedsReCompute)
				MasterMap->Compute();
		}
	}


	// Output the Demo Header information

	BYTE VersionText[17];  // UT2004 Demo File<ctrl-z> for nice /type support :)
	VersionText[0]=0x55;   
	VersionText[1]=0x54;
	VersionText[2]=0x32;
	VersionText[3]=0x30;
	VersionText[4]=0x30;
	VersionText[5]=0x34;
	VersionText[6]=0x20;
	VersionText[7]=0x44;
	VersionText[8]=0x45;
	VersionText[9]=0x4D;
	VersionText[10]=0x4F;
	VersionText[11]=0x20;
	VersionText[12]=0x46;
	VersionText[13]=0x49;
	VersionText[14]=0x4C;
	VersionText[15]=0x45;
	VersionText[16]=0x1A;
	
	FileAr->Serialize(VersionText,17);

	// Output a quick check, basically the sum of the characters above

	INT i,j=0;
	for (i=0;i<17;i++)
		j+=VersionText[i];

	*FileAr << j;

	// Output the actual demo header.  This holds all of the important information regarding the demo

	FString T;
	*FileAr << GetLevel()->URL.Map;
	if ( LevelInfo->Game )
		T = FString::Printf(TEXT("%s"),LevelInfo->Game->GetClass()->GetPathName());
	else if ( LevelInfo->GRI )
		T = FString::Printf(TEXT("%s"), *LevelInfo->GRI->GameClass );

	*FileAr << T; 

	*FileAr << LevelInfo->GRI->GoalScore;
	*FileAr << LevelInfo->GRI->TimeLimit;
	*FileAr << ClientDemo;
	
	T=FString::Printf( TEXT("%s"), 
		LevelInfo->NetMode == NM_DedicatedServer
		? (LevelInfo->GRI->ShortName != TEXT("") 
			? *LevelInfo->GRI->ShortName 
			: *LevelInfo->GRI->ServerName) 
		: GetLevel()->URL.GetOption(TEXT("Name="),TEXT("")));
	*FileAr << T;

	T=FString::Printf(TEXT("%s"),appTimestamp());
	*FileAr << T;

	INT PkgCount = MasterMap->List.Num();
	*FileAr << PkgCount;

	FString PackageName;
	for (int pkg=0;pkg<MasterMap->List.Num();pkg++)
	{
		PackageName = FString::Printf(TEXT("%s"),MasterMap->List(pkg).Parent->GetName());
		*FileAr << PackageName;
		*FileAr << MasterMap->List(pkg).Guid;
		*FileAr << MasterMap->List(pkg).RemoteGeneration;
	}

	// Create the control channel.
	Connection->CreateChannel( CHTYPE_Control, 1, 0 );

	// Welcome the player to the level.
	FString WelcomeExtra;

	if( LevelInfo->NetMode == NM_Client )
		WelcomeExtra = TEXT("NETCLIENTDEMO");
	else
	if( LevelInfo->NetMode == NM_Standalone )
		WelcomeExtra = TEXT("CLIENTDEMO");
	else
		WelcomeExtra = TEXT("SERVERDEMO");

	WelcomeExtra = WelcomeExtra + FString::Printf(TEXT(" TICKRATE=%d"), LevelInfo->NetMode == NM_DedicatedServer ? appRound(GameEngine->GetMaxTickRate()) : appRound(NetServerMaxTickRate) );

	GetLevel()->WelcomePlayer( Connection, (TCHAR*)(*WelcomeExtra)  );

	// Spawn the demo recording spectator.
	if( !ClientDemo )
		SpawnDemoRecSpectator(Connection);
	else
		GetLevel()->NetDriver->ServerConnection->Actor->eventStartClientDemoRec();

	return 1;
	unguard;
}
void UDemoRecDriver::StaticConstructor()
{
	guard(UDemoRecDriver::StaticConstructor);
	new(GetClass(),TEXT("DemoSpectatorClass"), RF_Public)UStrProperty(CPP_PROPERTY(DemoSpectatorClass), TEXT("Client"), CPF_Config);
	unguard;
}
void UDemoRecDriver::LowLevelDestroy()
{
	guard(UDemoRecDriver::LowLevelDestroy);

	debugf( TEXT("Closing down demo driver.") );

	// Shut down file.
	guard(CloseFile);
	if( FileAr )
	{	
		delete FileAr;
		FileAr = NULL;
	}
	unguard;

	unguard;
}
UBOOL UDemoRecDriver::UpdateDemoTime( FLOAT* DeltaTime, FLOAT TimeDilation )
{
	guard(UDemoRecDriver::UpdateDemoTime);

	UBOOL Result = 0;
	bNoRender = false;

	if( ServerConnection )
	{
		// Ensure LastFrameTime is inside a valid range, so we don't lock up if things get very out of sync.
		LastFrameTime = Clamp<DOUBLE>( LastFrameTime, appSeconds() - 1.0, appSeconds() );

		// Playback
		FrameNum++;
		if( Notify->NotifyGetLevel() && Notify->NotifyGetLevel()->FinishedPrecaching && InitialFrameStart == 0 )
		{
			PlaybackStartTime = appSeconds();
			InitialFrameStart = FrameNum;
			// Start the distribution at the same instant the mean starts, so the two describe exactly
			// the same set of frames and cannot disagree about what was measured.
			LastTimedFrameStart = PlaybackStartTime;
			FrameTimeSamples = 0;
			WorstFrameMs = 0.0;
			for( INT i=0; i<DEMO_FT_BUCKETS; i++ )
				FrameTimeBuckets[i] = 0;
		}
		else if( NoFrameCap && InitialFrameStart != 0 )
		{
			// One sample per frame of the timed run. Buckets are chosen as the frame rates a player
			// would actually name, not round millisecond numbers, so the histogram reads directly as
			// "how much of the run was above 60 / below 30 / below 15".
			const DOUBLE Now = appSeconds();
			const DOUBLE Ms  = (Now - LastTimedFrameStart) * 1000.0;
			LastTimedFrameStart = Now;
			if( Ms > WorstFrameMs )
				WorstFrameMs = Ms;
			INT B;
			if     ( Ms <  8.333 ) B = 0;   // >120 fps
			else if( Ms < 16.667 ) B = 1;   // 60-120
			else if( Ms < 33.333 ) B = 2;   // 30-60
			else if( Ms < 50.0   ) B = 3;   // 20-30
			else if( Ms < 66.667 ) B = 4;   // 15-20
			else if( Ms < 100.0  ) B = 5;   // 10-15
			else if( Ms < 200.0  ) B = 6;   // 5-10
			else                   B = 7;   // <5 fps
			FrameTimeBuckets[B]++;
			FrameTimeSamples++;
		}

		if( ServerConnection->State==USOCK_Open ) 
		{
			if( !FileAr->AtEnd() && !FileAr->IsError() )
			{
				// peek at next delta time.
				FLOAT NewDeltaTime;
				INT NewFrameNum;

				*FileAr << NewDeltaTime << NewFrameNum;
				FileAr->Seek(FileAr->Tell() - sizeof(NewDeltaTime) - sizeof(NewFrameNum));

				// If the real delta time is too small, sleep for the appropriate amount.
				if( !NoFrameCap )
				{
					if ( !GUseFixedTimeStep && (appSeconds() > LastFrameTime+(DOUBLE)NewDeltaTime/(1.1*TimeDilation)) )
						bNoRender = true;
					else
						while(appSeconds() < LastFrameTime+(DOUBLE)NewDeltaTime/(1.1*TimeDilation));
				}
				// Lie to the game about the amount of time which has passed.
				*DeltaTime = NewDeltaTime;
			}
		}
		LastDeltaTime = *DeltaTime;
	 	LastFrameTime = appSeconds();
	}
	else
	{
		// Recording
		BYTE NetMode = Notify->NotifyGetLevel()->GetLevelInfo()->NetMode;

		// Accumulate the current DeltaTime for the real frames this demo frame will represent.
		DemoRecMultiFrameDeltaTime += *DeltaTime;

		// Cap client demo recording rate (but not framerate).
		if( NetMode==NM_DedicatedServer || ( (appSeconds()-LastClientRecordTime) >= (DOUBLE)(1.f/NetServerMaxTickRate) ) )
		{
			// record another frame.
			FrameNum++;
			LastClientRecordTime = appSeconds();
			LastDeltaTime = DemoRecMultiFrameDeltaTime;
			DemoRecMultiFrameDeltaTime = 0.f;
			Result = 1;

			// Save the new delta-time and frame number, with no data, in case there is nothing to replicate.
			INT Count = 0;
			*FileAr << LastDeltaTime << FrameNum << Count;
		}
	}

	return Result;

	unguard;
}
void UDemoRecDriver::TickDispatch( FLOAT DeltaTime )
{
	guard(UDemoRecDriver::TickDispatch);
	Super::TickDispatch( DeltaTime );

	if( ServerConnection && (ServerConnection->State==USOCK_Pending || ServerConnection->State==USOCK_Open) )
	{	
		BYTE Data[PACKETSIZE + 8];
		// Read data from the demo file
		DWORD PacketBytes;
		INT PlayedThisTick = 0;
		for( ; ; )
		{
			// At end of file?
			if( FileAr->AtEnd() || FileAr->IsError() )
			{
			AtEnd:
				ServerConnection->State = USOCK_Closed;
				DemoEnded = 1;

				FLOAT Seconds = appSeconds()-PlaybackStartTime;
				if( NoFrameCap )
				{
					FString Result = FString::Printf(TEXT("Demo %s ended: %d frames in %lf seconds (%.3f fps)"), *DemoFilename, FrameNum-InitialFrameStart, Seconds, (FrameNum-InitialFrameStart)/Seconds );
					debugf(TEXT("%s"),*Result);
					ServerConnection->Actor->eventClientMessage( *Result, NAME_None );//!!localize!!

					// The distribution, not just the mean: the average above cannot distinguish a steady
					// 30 fps from an alternating 120/15. One machine-greppable line, in frame-rate bands
					// rather than milliseconds; percentages are of the timed frames only.
					if( FrameTimeSamples > 0 )
					{
						static const TCHAR* BandNames[DEMO_FT_BUCKETS] =
						{
							TEXT(">120"), TEXT("60-120"), TEXT("30-60"), TEXT("20-30"),
							TEXT("15-20"), TEXT("10-15"), TEXT("5-10"),  TEXT("<5")
						};
						FString Dist;
						for( INT i=0; i<DEMO_FT_BUCKETS; i++ )
							Dist += FString::Printf( TEXT("%s:%d(%.1f%%) "), BandNames[i], FrameTimeBuckets[i],
								100.f * FrameTimeBuckets[i] / FrameTimeSamples );
						// "below 30 fps" is the number a player feels, so it is stated outright.
						const INT Below30 = FrameTimeBuckets[3]+FrameTimeBuckets[4]+FrameTimeBuckets[5]+FrameTimeBuckets[6]+FrameTimeBuckets[7];
						debugf( TEXT("Demo %s distribution (%d frames): %s| worst %.1f ms | below 30fps: %.1f%%"),
							*DemoFilename, FrameTimeSamples, *Dist, WorstFrameMs, 100.f * Below30 / FrameTimeSamples );
					}

					if( ParseParam(appCmdLine(),TEXT("EXITAFTERDEMO")) )
					{
						// Output average framerate.
						FString OutputString	= TEXT("");
						INT		LastRand		= appRand();
						appLoadFileToString( OutputString, TEXT("..\\Benchmark\\benchmark.log") );
						OutputString += FString::Printf(TEXT("%f fps         rand[%i]\r\n"), (FrameNum-InitialFrameStart)/Seconds, LastRand );
						appSaveStringToFile( OutputString, TEXT("..\\Benchmark\\benchmark.log") );
						
						// Get time & date.
						INT Year, Month, DayOfWeek, Day, Hour, Minutes, Sec, MSec;
						appSystemTime( Year, Month, DayOfWeek, Day, Hour, Minutes, Sec, MSec );
						FString DateTime = FString::Printf(TEXT("%i-%02i-%02i-%02i-%02i-%02i"),Year,Month,Day,Hour,Minutes,Sec);

						// Output average framerate to dedicated file.
						OutputString  = FString::Printf(TEXT("%s\r\n%s\r\n%s\r\n%s\r\n\r\n%s\r\n\r\n"),GBuildLabel,GMachineOS,GMachineCPU,GMachineVideo,appCmdLine());
						OutputString += FString::Printf(TEXT("%f fps         rand[%i]\r\n"), (FrameNum-InitialFrameStart)/Seconds, LastRand );
						appSaveStringToFile( OutputString, *FString::Printf(TEXT("..\\Benchmark\\Results\\avgfps-%s.log"), *DateTime ) );
	
						// Output average for benchmark launcher.
						if( ParseParam(appCmdLine(),TEXT("UPT") ) )
						{
							OutputString = FString::Printf(TEXT("%f"), (FrameNum-InitialFrameStart)/Seconds );
							appSaveStringToFile( OutputString, TEXT("dummy.ben") );
						}

						// Copy the log.
						GLog->Flush();
						GFileManager->Copy( *FString::Printf(TEXT("..\\Benchmark\\Logs\\ut2004-%s.log"),*DateTime), TEXT("ut2004.log") );

						// Exit.
						GIsRequestingExit = 1;
						//appRequestExit(0);
					}
				}
				else
					ServerConnection->Actor->eventClientMessage( *FString::Printf(TEXT("Demo %s ended: %d frames in %f seconds"), *DemoFilename, FrameNum-InitialFrameStart, Seconds ), NAME_None );//!!localize!!

				if( Loop )
					GetLevel()->Exec( *(FString(TEXT("DEMOPLAY "))+(*LoopURL.String())), *GLog );
				return;
			}
	
			INT ServerFrameNum;
			FLOAT ServerDeltaTime;

			*FileAr << ServerDeltaTime;
			*FileAr << ServerFrameNum;
			if( ServerFrameNum > FrameNum )
			{
				FileAr->Seek(FileAr->Tell() - sizeof(ServerFrameNum) - sizeof(ServerDeltaTime));
				break;
			}
			*FileAr << PacketBytes;

			if( PacketBytes )
			{
				// Read data from file.
				FileAr->Serialize( Data, PacketBytes );
				if( FileAr->IsError() )
				{
					debugf( NAME_DevNet, TEXT("Failed to read demo file packet") );
					goto AtEnd;
				}

				// Update stats.
				PlayedThisTick++;

				// Process incoming packet.
				ServerConnection->ReceivedRawPacket( Data, PacketBytes );
			}

			// Only play one packet per tick on demo playback, until we're 
			// fully connected.  This is like the handshake for net play.
			if(ServerConnection->State == USOCK_Pending)
			{
				FrameNum = ServerFrameNum;
				break;
			}
		}
	}
	unguard;
}
FString UDemoRecDriver::LowLevelGetNetworkNumber()
{
	guard(UDemoRecDriver::LowLevelGetNetworkNumber);
	return TEXT("");
	unguard;
}
INT UDemoRecDriver::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UDemoRecDriver::Exec);
	if( DemoEnded )
		return 0;
	if( ParseCommand(&Cmd,TEXT("DEMOREC")) || ParseCommand(&Cmd,TEXT("DEMOPLAY")) )
	{
		if( ServerConnection )
			Ar.Logf( TEXT("Demo playback currently active: %s"), *DemoFilename );//!!localize!!
		else
			Ar.Logf( TEXT("Demo recording currently active: %s"), *DemoFilename );//!!localize!!
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("STOPDEMO")) )
	{
		Loop = 0;
		Ar.Logf( TEXT("Demo %s stopped at frame %d"), *DemoFilename, FrameNum );//!!localize!!
		if( !ServerConnection )
		{
			GetLevel()->DemoRecDriver=NULL;
			delete this;
		}
		else
			ServerConnection->State = USOCK_Closed;
		return 1;
	}
	else return 0;
	unguard;
}
ULevel* UDemoRecDriver::GetLevel()
{
	guard(UDemoRecDriver::GetLevel);
	check(Notify->NotifyGetLevel());
	return Notify->NotifyGetLevel();
	unguard;

}
void UDemoRecDriver::SpawnDemoRecSpectator( UNetConnection* Connection )
{
	guard(UDemoRecDriver::SpawnDemoRecSpectator);
	UClass* C = StaticLoadClass( AActor::StaticClass(), NULL, *DemoSpectatorClass, NULL, LOAD_NoFail, NULL );
	APlayerController* Controller = CastChecked<APlayerController>(GetLevel()->SpawnActor( C ));

    guard(FindPlayerStart);
	for( INT i=0; i<GetLevel()->Actors.Num(); i++ )
	{
		if( GetLevel()->Actors(i) && GetLevel()->Actors(i)->IsA(APlayerStart::StaticClass()) )
		{
			Controller->Location = GetLevel()->Actors(i)->Location;
			Controller->Rotation = GetLevel()->Actors(i)->Rotation;
			break;
		}
	}
	unguard;

	unguard;
}
IMPLEMENT_CLASS(UDemoRecDriver);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

