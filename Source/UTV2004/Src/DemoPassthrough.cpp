#include "ReplicatorEngine.h"
#include "DemoPassthrough.h"

#define PACKETSIZE 512

UBOOL UDemoPassthroughDriver::InitConnect( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error )
{
	guard(UDemoPassthroughDriver::InitConnect);
	if( !UNetDriver::InitConnect( InNotify, ConnectURL, Error ) )
		return 0;
	if( !InitBase( 1, InNotify, ConnectURL, Error ) )
		return 0;

	// Playback, local machine is a client, and the demo stream acts "as if" it's the server.
	//ServerConnection = new UDemoPassthroughConnection( this, ConnectURL );
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

	return 1;
	unguard;
}


void UDemoPassthroughDriver::TickDispatch( FLOAT DeltaTime )
{
	guard(UDemoPassthroughDriver::TickDispatch);
	UNetDriver::TickDispatch( DeltaTime );

	if (pauseDemo)
		return;

	//UpdateDemoTime (&DeltaTime, 2.0f);
	realTime += DeltaTime;

	if( ServerConnection && (ServerConnection->State==USOCK_Pending || ServerConnection->State==USOCK_Open) )
	{	
		BYTE Data[PACKETSIZE + 8];
		// Read data from the demo file
		DWORD PacketBytes;
		INT PlayedThisTick = 0;

		for( ; ; )
		{
			if (demoTime > realTime) {
				break;
			}

			// At end of file?
			if( FileAr->AtEnd() || FileAr->IsError() )
			{
			AtEnd:
				ServerConnection->State = USOCK_Closed;
				DemoEnded = 1;
				return;
			}
	
			INT ServerFrameNum;
			FLOAT ServerDeltaTime;

			*FileAr << ServerDeltaTime;
			*FileAr << ServerFrameNum;

			//Check for speeding
			if (ServerFrameNum > lastFrame) {
				demoTime += ServerDeltaTime;
				lastFrame = ServerFrameNum;
			}
			else {
				//debugf(TEXT("Not adding delta %f"), ServerDeltaTime);
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

				//This could have been set now
				if (pauseDemo)
					break;
			}
		}

	}
	unguard;
}

UBOOL UDemoPassthroughDriver::IsDemoDriver()
{
	return (!cloakDemo);
}

void UDemoPassthroughDriver::SetCloak(UBOOL cloak)
{
	cloakDemo = cloak;
}

void UDemoPassthroughDriver::SetPaused(UBOOL pause)
{
	pauseDemo = pause;
	//debugf(TEXT("Setting pause to %d"), pause);
}

IMPLEMENT_CLASS(UDemoPassthroughDriver);
