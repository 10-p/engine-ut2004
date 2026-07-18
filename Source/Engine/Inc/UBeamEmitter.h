/*=============================================================================
	UBeamEmitter.h.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	virtual void Initialize( INT InMaxParticles );
	virtual void CleanUp();
	virtual void PostEditChange();
	virtual void UpdateActorHitList();

	virtual void Scale( FLOAT ScaleFactor );
	virtual void UpdateBeamLength( FLOAT DeltaTime );

	virtual void SpawnParticle( INT Index, FLOAT SpawnTime, INT Flags = 0, INT SpawnFlags = 0, const FVector& LocalLocationOffset = FVector(0,0,0) );
	virtual INT  UpdateParticles( FLOAT DeltaTime );
	virtual INT  RenderParticles( FDynamicActor* DynActor, FLevelSceneNode* SceneNode, TList<FDynamicLight*>* Lights, FRenderInterface* RI );
	
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

