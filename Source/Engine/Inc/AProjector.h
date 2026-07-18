/*=============================================================================
	AProjector.h: Class functions residing in the Projector class.
	Copyright 2000-2002 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	// Projector interface.
	virtual void Attach( FLOAT FadeInTime = 0.f );
	virtual void Detach( UBOOL Force );
	virtual void Abandon();
	virtual void CalcMatrix();
	virtual void UpdateParticleMaterial(class UParticleMaterial* ParticleMaterial, INT ProjectorIndex );
	

	// Actor interface.
	virtual UPrimitive* GetPrimitive();
	void PostEditChange()
	{
		Super::PostEditChange();
		Detach(1);
		Attach();
	}
	void PostEditLoad()
	{
		Super::PostEditLoad();
		SetZone( 0, 0 );
		Attach();
	}
	void PostEditMove()
	{
		Super::PostEditMove();
		if( OldLocation != Location )
		{
			Detach(1);
			Attach();
		}
	}
	void Destroy()
	{
		Detach(1);
		Super::Destroy();
	}
	void TickSpecial( FLOAT DeltaSeconds )
	{
		if( Physics==PHYS_Rotating )
			CalcMatrix();
	}
	void RenderWireframe(FRenderInterface* RI);
	void RenderEditorSelected(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA);

	UBOOL ShouldTrace(AActor *SourceActor, DWORD TraceFlags);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

