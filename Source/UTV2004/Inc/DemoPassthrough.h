#ifndef _UNNETPASSTHROUGHDRV_H_
#define _UNNETPASSTHROUGHDRV_H_

#include "EpicInclude.h"

#include "UnForcePacking_begin.h"

class UDemoPassthroughDriver : public UDemoRecDriver
{
	DECLARE_CLASS(UDemoPassthroughDriver,UDemoRecDriver,CLASS_Config|CLASS_Transient,utv2003)

	// UNetDriver interface.
	UBOOL InitConnect( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error );
	void TickDispatch( FLOAT DeltaTime );
    virtual UBOOL IsDemoDriver();

	UBOOL cloakDemo;			//When creating outbunches, we can't say we are a demo driver since they wont be packed then
	UBOOL pauseDemo;
	FLOAT realTime;
	FLOAT demoTime;
	INT lastFrame;
	void SetCloak(UBOOL cloak);
	void SetPaused(UBOOL pause);
};

#include "UnForcePacking_end.h"

#endif