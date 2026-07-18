/*=============================================================================
	UParticleEmitter.h.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	virtual	void PostLoad();
	virtual void Destroy();
	virtual void PostEditChange();

	virtual void Initialize( INT InMaxParticles );
	virtual void CleanUp();
	virtual void Reset();

	virtual void Scale( FLOAT ScaleFactor );
	
	virtual void HandleActorForce( AActor* Other, FLOAT DeltaTime );
	virtual FLOAT SpawnParticles( FLOAT OldLeftover, FLOAT Rate, FLOAT DeltaTime );
	virtual void SpawnParticle( INT Index, FLOAT SpawnTime, INT Flags = 0, INT SpawnFlags = 0, const FVector& LocalLocationOffset = FVector(0,0,0) );
	virtual INT UpdateParticles( FLOAT DeltaTime );
	virtual void UpdateParticle( FLOAT DeltaTime, INT Index ) {};
	virtual INT RenderParticles( FDynamicActor* DynActor, class FLevelSceneNode* SceneNode, TList<FDynamicLight*>* Lights, FRenderInterface* RI );

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

