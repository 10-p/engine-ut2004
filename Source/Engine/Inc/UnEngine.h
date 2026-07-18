/*=============================================================================
	UnEngine.h: Unreal engine definition.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#ifndef _INCL_UNENGINE_H_
#define _INCL_UNENGINE_H_

#if SUPPORTS_PRAGMA_PACK
#pragma pack (push,4)
#endif

/*-----------------------------------------------------------------------------
	Unreal engine.
-----------------------------------------------------------------------------*/

class ENGINE_API UEngine : public USubsystem
{
	DECLARE_ABSTRACT_CLASS(UEngine,USubsystem,CLASS_Config|CLASS_Transient,Engine)

	// Variables.
	class UPrimitive*		Cylinder;
	class UClient*			Client;
	class UAudioSubsystem*	Audio;
	class URenderDevice*	GRenDev;

	// Stats.
	UBOOL					bShowFrameRate;
	UBOOL					bShowRenderStats;
	UBOOL					bShowHardwareStats;
	UBOOL					bShowGameStats;
	UBOOL					bShowNetStats;
	UBOOL					bShowAnimStats;		 // Show animation statistics.

	// ifdef WITH_LIPSinc not here since this needs to be in Engine.uc as well.
	UBOOL					bShowLIPSincStats;   // Show LIPSinc statistics.
	// endif

	UBOOL					bShowHistograph;
	UBOOL					bShowXboxMemStats;
	UBOOL					bShowMatineeStats;	// Show Matinee specific information
	UBOOL					bShowAudioStats;
	UBOOL					bShowLightStats;    // Show dynamic lighting statistics.

	INT					TickCycles GCC_PACK(4);
	INT					GameCycles, ClientCycles;
	INT					CacheSizeMegs;

	BITFIELD				UseSound:1 GCC_PACK(4);
	BITFIELD				UseStaticMeshBatching:1;
	BITFIELD				ServerReadsStdin:1;
	BITFIELD				bSlowRefChecking:1;
	FLOAT					CurrentTickRate GCC_PACK(4);
	INT						DetectedVideoMemory;

	// Color preferences.
	FColor
		C_WorldBox,
		C_GroundPlane,
		C_GroundHighlight,
		C_BrushWire,
		C_Pivot,
		C_Select,
		C_Current,
		C_AddWire,
		C_SubtractWire,
		C_GreyWire,
		C_BrushVertex,
		C_BrushSnap,
		C_Invalid,
		C_ActorWire,
		C_ActorHiWire,
		C_Black,
		C_White,
		C_Mask,
		C_SemiSolidWire,
		C_NonSolidWire,
		C_WireBackground,
		C_WireGridAxis,
		C_ActorArrow,
		C_ScaleBox,
		C_ScaleBoxHi,
		C_ZoneWire,
		C_Mover,
		C_OrthoBackground,
		C_StaticMesh,
		C_Volume,
		C_ConstraintLine,
		C_AnimMesh,
		C_TerrainWire;

	// Constructors.
	UEngine();
	void StaticConstructor();

	// UObject interface.
	void Serialize( FArchive& Ar );
	void Destroy();

	// UEngine interface.
	virtual void BreakCompatibility() {};
	virtual void Init();
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Out=*GLog );
	virtual void Flush( UBOOL AllowPrecache );
	virtual void UpdateGamma();
	virtual void RestoreGamma();
	virtual UBOOL Key( UViewport* Viewport, EInputKey Key, TCHAR Unicode );
	virtual UBOOL InputEvent( UViewport* Viewport, EInputKey iKey, EInputAction State, FLOAT Delta=0.f );
	virtual void Tick( FLOAT DeltaSeconds )=0;
	virtual void Draw( UViewport* Viewport, UBOOL Blit=1, BYTE* HitData=NULL, INT* HitSize=NULL )=0;
	virtual void MouseDelta( UViewport* Viewport, DWORD Buttons, FLOAT DX, FLOAT DY )=0;
	virtual void MousePosition( UViewport* Viewport, DWORD Buttons, FLOAT X, FLOAT Y )=0;
	virtual void MouseWheel( UViewport* Viewport, DWORD Buttons, INT Delta )=0;
	virtual void Click( UViewport* Viewport, DWORD Buttons, FLOAT X, FLOAT Y )=0;
	virtual void UnClick( UViewport* Viewport, DWORD Buttons, INT MouseX, INT MouseY )=0;
	virtual void SetClientTravel( UPlayer* Viewport, const TCHAR* NextURL, UBOOL bItems, ETravelType TravelType )=0;
	virtual INT ChallengeResponse( INT Challenge );
	virtual FLOAT GetMaxTickRate();
	virtual void SetProgress( const TCHAR* CmdStr, const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds = -1.f ); // gam
	void InitAudio();

	virtual void edSetClickLocation( FVector& InLocation ) {};
	virtual void edDrawAxisIndicator( class FSceneNode* SceneNode ) {};
	virtual int edcamMode( UViewport* Viewport ) {return 0;}
	virtual int edcamTerrainBrush() {return 0;}
	virtual int edcamMouseControl( UViewport* InViewport ) {return 0;}
	virtual void EdCallback( DWORD Code, UBOOL Send, DWORD lParam ) {};
	virtual INT AnotherDummyFunctionToBreakCompatibility( INT i );
	virtual INT ReallyBreakCompatibility( INT i );

#if DEMOVERSION
	virtual INT DummyFunctionToBreakCompatibility( INT i );
#endif	
};

/*-----------------------------------------------------------------------------
	UServerCommandlet.
-----------------------------------------------------------------------------*/

class UServerCommandlet : public UCommandlet
{
	DECLARE_CLASS(UServerCommandlet,UCommandlet,CLASS_Transient,Engine);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};


/*-----------------------------------------------------------------------------
	UUModUnpackCommandlet.
-----------------------------------------------------------------------------*/

class UUModUnpackCommandlet : public UCommandlet
{
	DECLARE_CLASS(UUModUnpackCommandlet,UCommandlet,CLASS_Transient,Engine);
	void StaticConstructor();
	INT Main( const TCHAR* Parms );
};

/*-----------------------------------------------------------------------------
	MasterMD5Commandlet.
-----------------------------------------------------------------------------*/

class UMasterMD5Commandlet : public UCommandlet
{
	DECLARE_CLASS(UMasterMD5Commandlet,UCommandlet,CLASS_Transient,Engine);

	TArray<UPackageCheckInfo*> PackageValidation;
	UObject* OutputPackage;

	void StaticConstructor();
	INT ProcessDirectory(FString Directory, const TCHAR* Parms);
	INT DoQuickMD5(const TCHAR *Parms, FString& MD5, FString& Guid);

	INT AddPackagesToDatabase(UBOOL Fresh, const TCHAR* Parms);	// Add packages to the database
	INT ShowDatabase(const TCHAR* Parms);			// Show all entries in this database
	INT Revision(const TCHAR* Parms);				// Adjust revision numbers
	INT FullMD5(const TCHAR* Parms);				// Do a full MD5 on a set of files
	INT QuickMD5(const TCHAR* Parms);				// Do a Quick MD5 on a given file
	INT Jack(const TCHAR* Parms);					// Added command for Jack
	INT Web(const TCHAR* Parms);					// Output MD5s in Epic web format
	INT ShowGUID(const TCHAR* Parms);				// Show the GUID of a package
	INT LinksTo(const TCHAR* Parms);				// Shows what a package links to
	INT GlobalID(const TCHAR* Params);				// Given a CDKEY show the GlobalID
	INT Main( const TCHAR* Parms );
};

/*-----------------------------------------------------------------------------
	UGlobalTempObjects.
-----------------------------------------------------------------------------*/

class UGlobalTempObjects : public UObject
{
	DECLARE_CLASS(UGlobalTempObjects,UObject,CLASS_Transient,Engine);
	TArray<UObject**> GlobalObjectPtrs;

	// Constructor
	UGlobalTempObjects()
	{
		AddToRoot();
	}

	// UGlobalTempObjects interface
	void AddGlobalObject( UObject** InObjectPtr )
	{
		guard(UGlobalTempObjects::AddGlobalObject);
		GlobalObjectPtrs.AddItem(InObjectPtr);
		unguard;
	}
	
	// UObject interface
	void Serialize( FArchive& Ar )
	{
		guard(UGlobalTempObjects::Serialize);
		Super::Serialize(Ar);
		if( !(Ar.IsLoading() || Ar.IsSaving()) )
		{
			for( INT i=0;i<GlobalObjectPtrs.Num();i++ )
				*(GlobalObjectPtrs(i)) = NULL;
			GlobalObjectPtrs.Empty();
		}
		unguard;
	}
	void Destroy()
	{
		guard(UGlobalTempObjects::Destroy);
		Super::Destroy();
		for( INT i=0;i<GlobalObjectPtrs.Num();i++ )
			*(GlobalObjectPtrs(i)) = NULL;
		GlobalObjectPtrs.Empty();
		unguard;
	}
};

#if SUPPORTS_PRAGMA_PACK
#pragma pack (pop)
#endif

#endif   // include-once blocker.

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

