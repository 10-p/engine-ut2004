/*=============================================================================
	ATeleporter.h: Class functions residing in the ATeleporter class.
	Copyright 2000-2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	void addReachSpecs(APawn * Scout, UBOOL bOnlyChanged=false);
	virtual UBOOL ReachedBy(APawn * P, FVector Loc);
	virtual UBOOL NoReachDistance();

