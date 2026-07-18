#include "UtvVoiceChannel.h"
#include "ReplicatorEngine.h"
#include "UTVPackageMap.h"
#include "BunchDelayer.h"

/*-----------------------------------------------------------------------------
UtvVoiceChannel.
-----------------------------------------------------------------------------*/

//copied from unchann.cpp, keep updated
FArchive &operator<<( FArchive& Ar, FVoiceInfo& V )
{
	INT TempVoiceCodec = V.VoiceCodec;
	Ar << TempVoiceCodec << V.PlayerID << V.PacketSize << V.ServerOptions << V.VoiceIndex;
	V.VoiceCodec = (EVoiceCodec) TempVoiceCodec;
	return Ar;
}

//
// Initialize this actor channel.
//
UtvVoiceChannel::UtvVoiceChannel()
{}
void UtvVoiceChannel::Init( UNetConnection* InConnection, INT InChannelIndex, INT InOpenedLocally )
{
	guard(UtvVoiceChannel::UtvVoiceChannel);
	Super::Init( InConnection, InChannelIndex, InOpenedLocally );
	if(UtvEngine->ConnectDriver->ServerConnection==InConnection)
		ServerChannel=true;
	else
		ServerChannel=false;
	unguard;
}



//
// Handle receiving a bunch of data on this actor channel.
//
void UtvVoiceChannel::ReceivedBunch( FInBunch& Bunch )
{
	guard(UtvVoiceChannel::ReceivedBunch);

	for( ; ; )
	{
		BYTE PacketType;
		Bunch.Serialize( &PacketType, 1 );
		if( Bunch.IsError() )
			break;

		if( PacketType >= VOICE_MAX_CHATTERS ) 
		{
			// Status message (client & server).
			debugf(TEXT("Reveived voice status packet type %i on %i size %i"),PacketType,Connection,Bunch.GetNumBytes());

			FVoiceInfo VoiceInfo;
			Bunch << VoiceInfo;
			if( Bunch.IsError() ){
				debugf(TEXT("bunch is error on voice ack packet"));
				break;
			}

			ReceivedStatusPacket( &VoiceInfo, (EVoicePacketType) PacketType );

		}	else if(!Connection->Driver->ServerConnection){
			// Server.
//			debugf(TEXT("Reveived voice packet on %i"),Connection);

			FVoiceInfo* VoiceInfo = Connection->Driver->VoiceInfos[PacketType];
			check( VoiceInfo ); //!!VOIP TODO

			Bunch.Serialize( VoiceInfo->PacketData, VoiceInfo->PacketSize );
			if( Bunch.IsError() )
				break;

			if(UtvEngine->OpenConnections.Find(Connection)->vipConnection){
				if(Connection==UtvEngine->PrimaryConnection && UtvEngine->DelayPrimaryVoice){
					vDelayer.AddVoice(VoiceInfo);
				} else {
					DistributeVoicePacket(VoiceInfo);
				}
			}
		}
		else
		{
//		debugf(TEXT("Voice message on server channel??"));
			FVoiceInfo* VoiceInfo = Connection->Driver->VoiceInfos[PacketType];
			check( VoiceInfo ); //!!VOIP TODO

			Bunch.Serialize( VoiceInfo->PacketData, VoiceInfo->PacketSize );
			if( Bunch.IsError() )
				break;
			VoiceInfo->VoiceIndex=0;
			if(UtvEngine->PrimaryConnection && UtvEngine->PrimaryConnection->VoiceChannel){
				((UtvVoiceChannel*)UtvEngine->PrimaryConnection->VoiceChannel)->DistributeVoicePacket(VoiceInfo);
			} else if(UtvEngine->ListenDriver->ClientConnections(0) && UtvEngine->ListenDriver->ClientConnections(0)->VoiceChannel){
				((UtvVoiceChannel*)UtvEngine->ListenDriver->ClientConnections(0)->VoiceChannel)->DistributeVoicePacket(VoiceInfo);
			}

			// Client.
		}
	}
	//	delete tempBunch;
	//	UChannel::ReceivedBunch(Bunch);
	unguard;
}

void UtvVoiceChannel::DistributeVoicePacket(FVoiceInfo* VoiceInfo)
{
	// Iterate over clients.
	for( INT i=Connection->Driver->ClientConnections.Num()-1; i>=0; i-- )
	{
		UNetConnection* ClientConnection	= Connection->Driver->ClientConnections(i);
		UVoiceChannel*&	VoiceChannel		= ClientConnection->VoiceChannel;

		// Only send if the other player is in our ActiveRoom or if we are logged in as admin
		//				if( AllowVoiceTransmission(ClientConnection->Actor, *VoiceInfo) )
		//				{
		if( VoiceChannel && !VoiceChannel->Closing && VoiceInfo->VoiceIndex!=VoiceChannel->VoiceIndex)
		{
			//check( VoiceInfo->VoiceIndex == VoiceIndex ); //dont check since it might be from upstream server
			// Don't send data till connection has been established by the other end as packet size is variable.
			if( Connection->Driver->VoiceAckMap[VoiceInfo->VoiceIndex][VoiceChannel->VoiceIndex] == USOCK_Open ){
				VoiceChannel->SendVoicePacket( VoiceInfo );
				//							debugf(TEXT("Sending voice packet"));
			} else if( Connection->Driver->VoiceAckMap[VoiceInfo->VoiceIndex][VoiceChannel->VoiceIndex] == USOCK_Closed ){
				VoiceChannel->SendStatusPacket( VoiceInfo, VOICEPACKET_AddChatter );
				//							debugf(TEXT("Sending voice AddChatter packet"));
			}
		}
		//			}
	}
}

IMPLEMENT_CLASS(UtvVoiceChannel);

/*-----------------------------------------------------------------------------
The End.
-----------------------------------------------------------------------------*/