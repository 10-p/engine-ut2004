/*=============================================================================
	AMover.h: Class functions residing in the AMover class.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	// Constructors.
	AMover();

	// UObject interface.
	void PostLoad();
	void PostEditChange();

	// AActor interface.
	void Spawned();
	void PostEditMove();
	void PreRaytrace();
	void PostRaytrace();
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	void physMovingBrush(FLOAT DeltaTime);
	void performPhysics(FLOAT DeltaSeconds);
	void PreNetReceive();
	void PostNetReceive();
	UBOOL ShouldTrace(AActor *SourceActor, DWORD TraceFlags)
	{
		// inlined for speed on PS2
		return (TraceFlags & /*TRACE_Movers*/ 0x002 );
	}
	virtual INT AddMyMarker(AActor *S);
	virtual void ClearMarker();
	virtual UBOOL IsAMover() {return true;}
    virtual class AMover* GetAMover() { return this; } // sjs

	// ABrush interface.
	virtual void SetWorldRaytraceKey();
	virtual void SetBrushRaytraceKey();

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

