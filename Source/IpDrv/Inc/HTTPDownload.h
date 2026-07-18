/*=============================================================================
	HTTPDownload.h: Unreal HTTP File Download
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jack Porter
=============================================================================*/
#ifndef UHTTPDownload_H
#define UHTTPDownload_H

#if FORCE_EXTREME_PACKING
#include "UnForcePacking_begin.h"
#elif SUPPORTS_PRAGMA_PACK
#pragma pack (push,4)
#endif

enum EHTTPState
{
	HTTP_Initialized		=0,
	HTTP_Resolving			=1,
	HTTP_Resolved			=2,
	HTTP_Connecting			=3,
	HTTP_ReceivingHeader	=4,
	HTTP_ReceivingData		=5,
	HTTP_Closed				=6
};

class UHTTPDownload : public UDownload
{
	DECLARE_CLASS(UHTTPDownload,UDownload,CLASS_Transient|CLASS_Config,IpDrv);
    	
	// Config.
	FStringNoInit	ProxyServerHost;
	INT				ProxyServerPort;
	// mh -- HTTP 301/302 support
	INT				MaxRedirection;
	INT				CurRedirectLevel;
	// -- mh

	// Variables.
	BYTE			HTTPState;
	SOCKADDR_IN		LocalAddr GCC_PACK(4);
	SOCKADDR_IN		ServerAddr;
	SOCKET				ServerSocket;
	FResolveInfo*	ResolveInfo;
	FURL			DownloadURL;
	TArray<BYTE>	ReceivedData;
	TArray<FString>	Headers;
	DOUBLE			ConnectStartTime;

	// Constructors.
	void StaticConstructor();
	UHTTPDownload();

	// UObject interface.
	void Destroy();
	void Serialize( FArchive& Ar );

	// UDownload Interface.
	virtual void ReceiveFile( UNetConnection* InConnection, INT PackageIndex, const TCHAR *Params, UBOOL InCompression, INT InAttempt );
	UBOOL TrySkipFile();
	void Tick(void);

	// UHTTPDownload Interface.
	UBOOL FetchData();
};

#if FORCE_EXTREME_PACKING
#include "UnForcePacking_end.h"
#elif SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

#endif
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

