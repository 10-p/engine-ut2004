/*=============================================================================
	APlayerController.h: A player pawn.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	//  Player Pawn interface.
	void SetPlayer( UPlayer* Player );

	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	void CheckHearSound(AActor* SoundMaker, INT Id, USound* Sound, FVector Parameters, FLOAT Radius, UBOOL Attenuate);
	UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );
	UBOOL IsNetRelevantFor( APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation );
	AActor* GetViewTarget();
	virtual UBOOL LocalPlayerController();
	virtual UBOOL WantsLedgeCheck();
	virtual UBOOL StopAtLedge();

    // amb ---
    virtual void PostRender(class FSceneNode* SceneNode);
    // --- amb

    virtual APlayerController* GetAPlayerController() { return this; } // sjs
	virtual void CleanupSubObjects();
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

