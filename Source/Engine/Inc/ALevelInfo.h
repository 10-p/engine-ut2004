/*=============================================================================
	ALevelInfo.h.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	// Constructors.
	ALevelInfo() {}

	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	void CheckForErrors();
	void PreNetReceive();
	void PostNetReceive();

	// Level functions
	void SetZone( UBOOL bTest, UBOOL bForceRefresh );
	void SetVolumes(const TArray<class AVolume*>& Volumes);
	void SetVolumes();
	APhysicsVolume* GetDefaultPhysicsVolume();
	APhysicsVolume* GetPhysicsVolume(FVector Loc, AActor *A, UBOOL bUseTouch);
	void InitDistanceFogLOD();
	void UpdateDistanceFogLOD( FLOAT LOD );
	APlayerController* GetLocalPlayerController() { return LocalPlayerController; };

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

