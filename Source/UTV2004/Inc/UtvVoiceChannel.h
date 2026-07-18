#ifndef UTVVOICECHANNEL
#define UTVVOICECHANNEL

#include "UnIpDrv.h"
#include "UnNet.h"
#include "UnForcePacking_begin.h"

//
// A channel for exchanging voice properties.
//
class /*ENGINE_API*/ UtvVoiceChannel : public UVoiceChannel
{
	DECLARE_CLASS(UtvVoiceChannel,UVoiceChannel,CLASS_Transient,utv2004);
	
	bool ServerChannel;

	// Constructor.
	void StaticConstructor()
	{
		guard(UtvVoiceChannel::StaticConstructor);
		ChannelClasses[CHTYPE_Voice]        = GetClass();
		ChannelClasses[6]        = GetClass();
		GetDefault<UtvVoiceChannel>()->ChType = CHTYPE_Voice;
		unguard;
	}
	UtvVoiceChannel();
	void Init( UNetConnection* InConnection, INT InChIndex, UBOOL InOpenedLocally );

	void ReceivedBunch( FInBunch& Bunch );
	void DistributeVoicePacket(FVoiceInfo* VoiceInfo);
};

#include "UnForcePacking_end.h"

#endif