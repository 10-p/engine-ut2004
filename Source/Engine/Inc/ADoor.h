/*=============================================================================
	ADoor.h: Class functions residing in the ADoor class.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	void PostaddReachSpecs(APawn * Scout);
	void InitForPathFinding();
	void PrePath();
	void PostPath();
	void FindBase();
	AActor* AssociatedLevelGeometry();
	UBOOL HasAssociatedLevelGeometry(AActor *Other);
	UBOOL IsIdentifiedAs(FName ActorName);

