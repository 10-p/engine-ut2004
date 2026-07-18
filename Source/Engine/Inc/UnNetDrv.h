/*=============================================================================
	UnNetDrv.h: Unreal network driver base class.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#ifndef _UNNETDRV_H_
#define _UNNETDRV_H_

#include "UnForcePacking_begin.h"

/*-----------------------------------------------------------------------------
	UPackageMapLevel.
-----------------------------------------------------------------------------*/

class ENGINE_API UPackageMapLevel : public UPackageMap
{
	DECLARE_CLASS(UPackageMapLevel,UPackageMap,CLASS_Transient,Engine);
	UNetConnection* Connection;
	UBOOL CanSerializeObject( UObject* Obj );
	UBOOL SerializeObject( FArchive& Ar, UClass* Class, UObject*& Obj );
	UPackageMapLevel()
	{}
	UPackageMapLevel( UNetConnection* InConnection )
	: Connection( InConnection )
	{}
};

/*-----------------------------------------------------------------------------
	UNetDriver.
-----------------------------------------------------------------------------*/

#define VOICE_MAX_CHATTERS 64

//
// Base class of a network driver attached to an active or pending level.
//
class ENGINE_API UNetDriver : public USubsystem
{
	DECLARE_ABSTRACT_CLASS(UNetDriver,USubsystem,CLASS_Transient|CLASS_Config,Engine)

	// Variables.
	TArray<UNetConnection*>		ClientConnections;
	UNetConnection*				ServerConnection;
	FVoiceInfo*					VoiceInfos[VOICE_MAX_CHATTERS];
	BYTE						VoiceAckMap[VOICE_MAX_CHATTERS][VOICE_MAX_CHATTERS]; // [Sender][Recipient]
	FNetworkNotify*				Notify;
	UPackageMap*				MasterMap;
	DOUBLE						Time;
	FLOAT						ConnectionTimeout;
	FLOAT						InitialConnectTimeout;
	FLOAT						KeepAliveTime;
	FLOAT						RelevantTimeout;
	FLOAT						SpawnPrioritySeconds;
	FLOAT						ServerTravelPause;
	INT							MaxClientRate;
	INT							MaxInternetClientRate;
	INT							NetServerMaxTickRate;
	INT							LanServerMaxTickRate;
	UBOOL						AllowDownloads;
	UBOOL						ProfileStats;
	UProperty*					RoleProperty;
	UProperty*					RemoteRoleProperty;
	INT							SendCycles, RecvCycles;
	INT							MaxDownloadSize;
	TArray<FString>				DownloadManagers;
	UBOOL						DisableKSecFix;
	TArrayNoInit<FStringNoInit> ClientRedirectURLs;

	// Constructors.
	UNetDriver();
	void StaticConstructor();

	// UObject interface.
	void Destroy();
	void Serialize( FArchive& Ar );

	// UNetDriver interface.
	virtual void LowLevelDestroy()=0;
	virtual FString LowLevelGetNetworkNumber()=0;
	virtual void AssertValid();
	virtual UBOOL InitConnect( FNetworkNotify* InNotify, FURL& ConnectURL, FString& Error );
	virtual UBOOL InitListen( FNetworkNotify* InNotify, FURL& ListenURL, FString& Error );
	virtual void TickFlush();
	virtual void TickDispatch( FLOAT DeltaTime );
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	virtual void NotifyActorDestroyed( AActor* Actor );
    virtual UBOOL IsDemoDriver() { return 0; } // sjs
};

#include "UnForcePacking_end.h"

#endif
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

