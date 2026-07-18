	INT AcceptNearbyPath(AActor *goal);
	void AdjustFromWall(FVector HitNormal, AActor* HitActor);
	void SetAdjustLocation(FVector NewLoc);
	virtual void AirSteering(float DeltaTime);
	DECLARE_FUNCTION(execPollWaitToSeeEnemy)

