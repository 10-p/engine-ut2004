/*=============================================================================
	UnGame.h: Unreal game class.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

/*-----------------------------------------------------------------------------
	Unreal game engine.
-----------------------------------------------------------------------------*/

//
// The Unreal game engine.
//
class ENGINE_API UGameEngine : public UEngine
{
	DECLARE_CLASS(UGameEngine,UEngine,CLASS_Config|CLASS_Transient,Engine)

	// Variables.
	ULevel*			GLevel;
	ULevel*			GEntry;
	UPendingLevel*	GPendingLevel;
	FURL			LastURL;
	TArrayNoInit<FString> ServerActors;
	TArrayNoInit<FString> ServerPackages;

	TArray<UPackageCheckInfo*> PackageValidation GCC_PACK(4);	// The Database of allowed MD5s
	UObject* MD5Package;

	FString			PendingRecordMovie;		// If set, a record movie command is issued on level start.

	FStringNoInit MainMenuClass;
	FStringNoInit SinglePlayerMenuClass;
	FStringNoInit ConnectingMenuClass;
	FStringNoInit DisconnectMenuClass;
	FStringNoInit LoadingClass;

	BITFIELD        bCheatProtection:1 GCC_PACK(4);// Is cheat Protection Enabled
	BITFIELD		ColorHighDetailMeshes:1;
	BITFIELD		ColorSlowCollisionMeshes:1;
	BITFIELD		ColorNoCollisionMeshes:1;
	BITFIELD		ColorWorldTextures:1;
	BITFIELD		ColorPlayerAndWeaponTextures:1;
	BITFIELD		ColorInterfaceTextures:1;
	BITFIELD        VoIPAllowVAD:1;

	// Constructors.
	UGameEngine();

	// UObject interface.
	void Serialize( FArchive& Ar );
	void Destroy();

	// UEngine interface.
	void Init();
	void Tick( FLOAT DeltaSeconds );
	void Draw( UViewport* Viewport, UBOOL Blit=1, BYTE* HitData=NULL, INT* HitSize=NULL );
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	void MouseDelta( UViewport*, DWORD, FLOAT, FLOAT );
	void MousePosition( class UViewport*, DWORD, FLOAT, FLOAT );
	void MouseWheel( UViewport* Viewport, DWORD Buttons, INT Delta );
	void Click( UViewport*, DWORD, FLOAT, FLOAT );
	void UnClick( UViewport*, DWORD, INT, INT );
	void SetClientTravel( UPlayer* Viewport, const TCHAR* NextURL, UBOOL bItems, ETravelType TravelType );
	FLOAT GetMaxTickRate();
	INT ChallengeResponse( INT Challenge );
	void SetProgress(  const TCHAR* CmdStr, const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds = -1.f ); // gam

	// UGameEngine interface.
	virtual UBOOL Browse( FURL URL, const TMap<FString,FString>* TravelInfo, FString& Error );
	virtual ULevel* LoadMap( const FURL& URL, UPendingLevel* Pending, const TMap<FString,FString>* TravelInfo, FString& Error );
	virtual void SaveGame( INT Position );
	virtual void CancelPending();
	virtual void PaintProgress( AVignette* Vignette = NULL, FLOAT Progress = 0.0F ); // gam
	virtual void UpdateConnectingMessage();
	virtual void BuildServerMasterMap( UNetDriver* NetDriver, ULevel* InLevel );
	virtual void NotifyLevelChange();
	virtual UBOOL GUIActive( const UClient* InClient=NULL ) const;
	void FixUpLevel();

	FString InitNewNetConnection(TArray<FString> Packets, int PackageCount, INT Gozer);	// Obscured: ValidatePackages
	void	AdjustNetConnection(ULevel* Level);	// Obscured: AuthroizeClient.. Have the server authorize any packages with Code in them

	//@@Cheat Protection

	UBOOL CheckForRogues();					// Check to see if any rogue packages (not in Package Map) exist
	
	void ServerUpdateMD5();	// Update the MD5 Database with any MD5's from ServerPackages

	int PackageRevisionLevel();	// Figures out the top most package revision level
	void SaveMD5Database();		// Save the MD5 Database back out
	void AddMD5(const TCHAR* GUID, const TCHAR* MD5, int Revision);
	void DefaultMD5();	// Make sure the MD5's of everything in EditPackages is added

	FString InitSpecial(const TCHAR* Special);	// Performs a full MD5 on a given filename

};

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/


