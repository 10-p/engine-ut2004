/*=============================================================================
	UnDemoPenLev.h: Unreal demo recording.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jack Porter.
=============================================================================*/

#include "UnForcePacking_begin.h"

/*-----------------------------------------------------------------------------
	UDemoPlayPendingLevel.
-----------------------------------------------------------------------------*/

//
// Class controlling a pending demo playback level.
//
class UDemoPlayPendingLevel : public UPendingLevel
{
	DECLARE_CLASS(UDemoPlayPendingLevel,UPendingLevel,CLASS_Transient,Engine)
	NO_DEFAULT_CONSTRUCTOR(UDemoPlayPendingLevel)

	// Constructors.
	UDemoPlayPendingLevel( UEngine* InEngine, const FURL& InURL );

	// FNetworkNotify interface.
	EAcceptConnection NotifyAcceptingConnection() { return ACCEPTC_Reject; }
	void NotifyAcceptedConnection( class UNetConnection* Connection ) {}
	UBOOL NotifyAcceptingChannel( class UChannel* Channel ) { return 1; }
	ULevel* NotifyGetLevel();
	void NotifyReceivedText( UNetConnection* Connection, const TCHAR* Text );
	UBOOL NotifySendingFile( UNetConnection* Connection, FGuid GUID ) {return 0;}

	// UPendingLevel interface.
	void Tick( FLOAT DeltaTime );
	UNetDriver* GetDriver();
	void SendJoin() { SentJoin = 1; }
	UBOOL TrySkipFile() { return 0; }
};

#include "UnForcePacking_end.h"

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/



