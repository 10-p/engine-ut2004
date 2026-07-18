//=============================================================================
// AxEmitter - AActor overloads
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================
	void PostEditChange();
	void PostBeginPlay();
	void Destroy();
	void Spawned();
	void InitParticle( int Index );
	void Initialize( void );
	UBOOL Tick( FLOAT deltaTime, ELevelTick TickType );
	void PreCalc();
	void Reset();
    void Render( FLevelSceneNode* SceneNode, FRenderInterface* RI );
	UBOOL IsForceAffected();

