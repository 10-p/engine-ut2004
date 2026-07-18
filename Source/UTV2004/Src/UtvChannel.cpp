#include "UtvChannel.h"
#include "ReplicatorEngine.h"
#include "UTVPackageMap.h"
#include "BunchDelayer.h"

/*-----------------------------------------------------------------------------
	UtvChannel.
-----------------------------------------------------------------------------*/

//
// Initialize this actor channel.
//
UtvChannel::UtvChannel()
{}
void UtvChannel::Init( UNetConnection* InConnection, INT InChannelIndex, INT InOpenedLocally )
{
	guard(UtvChannel::UtvChannel);
	Super::Init( InConnection, InChannelIndex, InOpenedLocally );
	if(UtvEngine->ConnectDriver->ServerConnection==InConnection)
		ServerChannel=true;
	else
		ServerChannel=false;
	unguard;
}

//
// Set the closing flag.
//
void UtvChannel::SetClosingFlag()
{
	guard(UtvChannel::SetClosingFlag);
	UChannel::SetClosingFlag();
	unguard;
}

//
// Close it.
//
void UtvChannel::Close()
{
	guard(UtvChannel::Close);
	//UtvEngine->ChannelClosed(this);
	printf("Closing channel %i\n",ChIndex);
	UChannel::Close();
	unguard;
}

//
// Time passes...
//
void UtvChannel::Tick()
{
	guard(UtvChannel::Tick);
	UChannel::Tick();
	unguard;
}

//
// Actor channel destructor.
//
void UtvChannel::Destroy()
{
	guard(UtvChannel::Destroy);
	Super::Destroy();
	unguard;
}

//
// Negative acknowledgements.
//
void UtvChannel::ReceivedNak( INT NakPacketId )
{
/*	guard(UtvChannel::ReceivedNak);
	for( FOutBunch* Out=OutRec; Out; Out=Out->Next )
	{
		// Retransmit reliable bunches in the lost packet.
		if( Out->PacketId==NakPacketId && !Out->ReceivedAck )
		{
			check(Out->bReliable);
			debugfSlow( NAME_DevNetTraffic, TEXT("      Channel %i nak; resending %i..."), Out->ChIndex, Out->ChSequence );
			printf("Nak resend %i %i %i\n",ChIndex,Out->bOpen,Out->bClose);
			Connection->SendRawBunch( *Out, 0 );
		}
	}
	unguard;
/*/
	guard(UtvChannel::ReceivedNak);
	UChannel::ReceivedNak(NakPacketId);
	unguard;/**/
}


//
// Handle receiving a bunch of data on this actor channel.
//
void UtvChannel::ReceivedBunch( FInBunch& Bunch )
{
	guard(UtvChannel::ReceivedBunch);
	FInBunch* tempBunch=new FInBunch(Bunch);

	if(ServerChannel)
		UtvEngine->MsgTimeOut=0;

	//Handle things that goes over the communication actor channel
	if (ChIndex == UNetConnection::MAX_CHANNELS-1) {
		UtvEngine->ParseReplicationBunch (Connection, tempBunch);
		return;		//Don't need to try to send it anywhere
	}

	if(ServerChannel){
		Delayer->AddBunch(tempBunch);	
	} else {
		if(Connection==UtvEngine->PrimaryConnection)
			UtvEngine->SendBunch(UtvEngine->ConnectDriver->ServerConnection,tempBunch);
	}

	delete tempBunch;
//	UChannel::ReceivedBunch(Bunch);
	unguard;
}

//
// Describe the actor channel.
//
FString UtvChannel::Describe()
{
	guard(UtvChannel::Describe);
	return UChannel::Describe();
	unguard;
}

void UtvChannel::UtvClose(void)
{
	guard(UtvChannel::UtvClose);
	if(!Closing){
		debugf(TEXT("UtvClosing channel %i"),ChIndex);
		Close();
	}
	unguard;
}

IMPLEMENT_CLASS(UtvChannel);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/