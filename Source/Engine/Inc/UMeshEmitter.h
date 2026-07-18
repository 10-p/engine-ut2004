/*=============================================================================
	UMeshEmitter.h.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	virtual void Initialize( INT InMaxParticles );
	virtual void PostEditChange();

	virtual INT  UpdateParticles( FLOAT DeltaTime );
	virtual INT  RenderParticles( FDynamicActor* DynActor, FLevelSceneNode* SceneNode, TList<class FDynamicLight*>* Lights, FRenderInterface* RI );

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

