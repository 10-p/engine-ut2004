	void BoundProjectileVelocity();
	virtual UBOOL ShrinkCollision(AActor *HitActor, const FVector &StartLocation);
    virtual class AProjectile* GetAProjectile() { return this; }
	virtual void PostRender(FLevelSceneNode* SceneNode, FRenderInterface* RI);	// jw - Added for hud effects

