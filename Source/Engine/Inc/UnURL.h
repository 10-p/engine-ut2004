/*=============================================================================
	UnURL.h: Unreal URL class.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

/*-----------------------------------------------------------------------------
	FURL.
-----------------------------------------------------------------------------*/

//
// A uniform resource locator.
//
class ENGINE_API FURL
{
public:
	// URL components.
	FString Protocol;	// Protocol, i.e. "unreal" or "http".
	FString Host;		// Optional hostname, i.e. "204.157.115.40" or "unreal.epicgames.com", blank if local.
	INT		Port;       // Optional host port.
	FString Map;		// Map name, i.e. "SkyCity", default is "Index".
	TArray<FString> Op;	// Options.
	FString Portal;		// Portal to enter through, default is "".

	// Status.
	UBOOL	Valid;		// Whether parsed successfully.

	// Statics.
	static FString DefaultProtocol;
	static FString DefaultProtocolDescription;
	static FString DefaultName;
	static FString DefaultMap;
	static FString DefaultLocalMap;
	static FString DefaultNetBrowseMap;
	static FString DefaultHost;
	static FString DefaultPortal;
	static FString DefaultMapExt;
	static FString DefaultSaveExt;
	static INT DefaultPort;

	// Constructors.
	FURL( const TCHAR* Filename=NULL );
	FURL( FURL* Base, const TCHAR* TextURL, ETravelType Type );
	static void StaticInit();
	static void StaticExit();

	// Functions.
	UBOOL IsInternal() const;
	UBOOL IsLocalInternal() const;
	UBOOL HasOption( const TCHAR* Test ) const;
	const TCHAR* GetOption( const TCHAR* Match, const TCHAR* Default ) const;
	void LoadURLConfig( const TCHAR* Section, const TCHAR* Filename=NULL );
	void SaveURLConfig( const TCHAR* Section, const TCHAR* Key, const TCHAR* Filename=NULL ) const;
	void AddOption( const TCHAR* KeyValuePair );
	void RemoveOption( const TCHAR* Key, const TCHAR* Section = NULL, const TCHAR* Filename = NULL);
	FString String( UBOOL FullyQualified=0, UBOOL bHidePasswords=0 ) const;
	FString OptionString( UBOOL bStripCommon=0 ) const;
	ENGINE_API friend FArchive& operator<<( FArchive& Ar, FURL& U );

	// Operators.
	UBOOL operator==( const FURL& Other ) const;
};

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

