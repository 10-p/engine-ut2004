/*=============================================================================
	AWeapon.h.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	// Constructors.
	AWeapon() {}

	virtual AActor* GetProjectorBase() { return Owner; }

	// AActor interface.
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	
    void TickAuthoritative( FLOAT DeltaSeconds );
	virtual void CleanupSubObjects();
	virtual void DoCleanup();

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

