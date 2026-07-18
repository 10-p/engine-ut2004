#ifndef UTVCHANNEL
#define UTVCHANNEL

#include "EpicInclude.h"
#include "UnForcePacking_begin.h"

//
// A channel for exchanging actor properties.
//
class /*ENGINE_API*/ UtvChannel : public UChannel
{
	DECLARE_CLASS(UtvChannel,UChannel,CLASS_Transient,utv2004);
	
	bool ServerChannel;

	// Constructor.
	void StaticConstructor()
	{
		guard(UtvChannel::StaticConstructor);
		ChannelClasses[CHTYPE_Actor]        = GetClass();
		ChannelClasses[7]        = GetClass();
		GetDefault<UtvChannel>()->ChType = CHTYPE_Actor;
		unguard;
	}
	UtvChannel();
	void Init( UNetConnection* InConnection, INT InChIndex, UBOOL InOpenedLocally );
	void Destroy();

	// UChannel interface.
	void SetClosingFlag();
	void ReceivedBunch( FInBunch& Bunch );
	void ReceivedNak( INT NakPacketId );
	void Close();
	void Tick();

	FString Describe();
public:
	void UtvClose(void);
};

#include "UnForcePacking_end.h"

#endif