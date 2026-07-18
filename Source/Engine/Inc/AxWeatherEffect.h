//=============================================================================
// AxWeatherEffect - AActor overloads
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================
	void PostEditChange();
    void PostEditLoad();
	void PostLoad();
	void Destroy();
	void Spawned();
    void SetZone( UBOOL bTest, UBOOL bForceRefresh );
	void InitParticle( int Index );
	void Initialize( void );
    void PreCalc( void );
    void CacheBlockers( void );
	UBOOL Tick( FLOAT deltaTime, ELevelTick TickType );
    void Render( FLevelSceneNode* SceneNode, FRenderInterface* RI );
    void UpdateViewer( FLevelSceneNode* SceneNode );
    void InitParticle( FWeatherPcl& pcl );
