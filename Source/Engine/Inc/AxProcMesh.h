//=============================================================================
// AxProcMesh - AActor overloads
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================
	
	// AActor Functions.
	virtual UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );
	virtual void Spawned();
	virtual void PostLoad();
	virtual void Destroy();
	virtual void PostEditChange();
	
	void Render(class FLevelSceneNode* SceneNode,TList<class FDynamicLight*>* Lights,FRenderInterface* RI);
	void CalcMeshData(void);

