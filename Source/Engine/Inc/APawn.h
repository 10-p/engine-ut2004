/*=============================================================================
	APawn.h: Class functions residing in the APawn class.
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// declare type for node evaluation functions
typedef FLOAT ( *NodeEvaluator ) (ANavigationPoint*, APawn*, FLOAT);

	// Constructors.
	APawn() {}

	// AActor interface.
	FLOAT GetNetPriority( FVector& ViewPos, FVector& ViewDir, AActor* Sent, FLOAT Time, FLOAT Lag );
	virtual UBOOL DelayScriptReplication(FLOAT LastFullUpdateTime);
	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	void NotifyAnimEnd( int Channel );

#ifdef WITH_LIPSINC
	void NotifyLIPSincAnimEnd();
#endif

	void NotifyBump(AActor *Other);
	UBOOL CheckOwnerUpdated();
	void TickSimulated( FLOAT DeltaSeconds );
	void TickSpecial( FLOAT DeltaSeconds );
	UBOOL PlayerControlled();
	void SetBase(AActor *NewBase, FVector NewFloor = FVector(0,0,1), int bNotifyActor=1);
	void CheckForErrors();
	UBOOL IsNetRelevantFor( APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation );
	UBOOL CacheNetRelevancy(UBOOL bIsRelevant, APlayerController* RealViewer, AActor* Viewer);
	UBOOL ShouldTrace(AActor *SourceActor, DWORD TraceFlags);
	void PreNetReceive();
	void PostNetReceive();
	void PostNetReceiveLocation();
	void RenderEditorSelected(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA);
	virtual FLOAT GetAmbientVolume(FLOAT Attenuation);
	virtual UBOOL ShouldUpdateRelativeLocation();

	// Level functions
	void SetZone( UBOOL bTest, UBOOL bForceRefresh );

	// Latent movement
	virtual void setMoveTimer(FVector MoveDir);
	FLOAT GetMaxSpeed();
	virtual UBOOL moveToward(const FVector &Dest, AActor *GoalActor);
	virtual void rotateToward(AActor *Focus, FVector FocalPoint);
	UBOOL PickWallAdjust(FVector WallHitNormal, AActor* HitActor);
	void StartNewSerpentine(FVector Dir,FVector Start);
	void ClearSerpentine();
	virtual UBOOL ReachedDesiredRotation();
	virtual UBOOL SharingVehicleWith(APawn *P);

	// reach tests
	UBOOL ReachedDestination(FVector Dir, AActor* GoalActor);
	virtual int pointReachable(FVector aPoint, int bKnowVisible=0);
	virtual int actorReachable(AActor *Other, UBOOL bKnowVisible=0, UBOOL bNoAnchorCheck=0);
	int Reachable(FVector aPoint, AActor* GoalActor);
	int walkReachable(FVector Dest, int reachFlags, AActor* GoalActor);
	int jumpReachable(FVector Dest, int reachFlags, AActor* GoalActor);
	int flyReachable(FVector Dest, int reachFlags, AActor* GoalActor);
	int swimReachable(FVector Dest, int reachFlags, AActor* GoalActor);
	int ladderReachable(FVector Dest, int reachFlags, AActor* GoalActor);
	
	// movement component tests (used by reach tests)
	ETestMoveResult jumpLanding(FVector testvel, int moveActor);
	ETestMoveResult walkMove(FVector Delta, FCheckResult& Hit, AActor* GoalActor, FLOAT threshold);
	ETestMoveResult flyMove(FVector Delta, AActor* GoalActor, FLOAT threshold);
	ETestMoveResult swimMove(FVector Delta, AActor* GoalActor, FLOAT threshold);
	ETestMoveResult FindBestJump(FVector Dest); 
	ETestMoveResult FindJumpUp(FVector Direction); 
	ETestMoveResult HitGoal(AActor *GoalActor); 
	FVector SuggestJumpVelocity(FVector Dest, FLOAT XYSpeed, FLOAT BaseZ);
	virtual UBOOL HurtByVolume(AActor *A);
	UBOOL CanCrouchWalk( const FVector& StartLocation, const FVector& EndLocation, AActor* HitActor );

	// Path finding
	UBOOL ValidAnchor(); 
	FLOAT findPathToward(AActor *goal, FVector GoalLocation, NodeEvaluator NodeEval, FLOAT BestWeight, UBOOL bWeightDetours);
	ANavigationPoint* breadthPathTo(NodeEvaluator NodeEval, ANavigationPoint *start, int moveFlags, FLOAT *Weight, UBOOL bWeightDetours);
	ANavigationPoint* CheckDetour(ANavigationPoint* BestDest, ANavigationPoint* Start, UBOOL bWeightDetours);
	int calcMoveFlags(); // FIXME: This used to be inline, but that didn't compile with static linking.
	void SetAnchor(ANavigationPoint *NewAnchor);

	// Pawn physics modes
	virtual void performPhysics(FLOAT DeltaSeconds);
	FVector CheckForLedges(FVector AccelDir, FVector Delta, FVector GravDir, int &bCheckedFall, int &bMustJump );
	void physWalking(FLOAT deltaTime, INT Iterations);
	void physFlying(FLOAT deltaTime, INT Iterations);
    void physHovering(FLOAT deltaTime, INT Iterations); // sjs
	void physSwimming(FLOAT deltaTime, INT Iterations);
	void physFalling(FLOAT deltaTime, INT Iterations);
	void physSpider(FLOAT deltaTime, INT Iterations);
	void physLadder(FLOAT deltaTime, INT Iterations);
	void startNewPhysics(FLOAT deltaTime, INT Iterations);
	void startSwimming(FVector OldLocation, FVector OldVelocity, FLOAT timeTick, FLOAT remainingTime, INT Iterations);
	virtual void physicsRotation(FLOAT deltaTime, FVector OldVelocity);
	void processLanded(FVector HitNormal, AActor *HitActor, FLOAT remainingTime, INT Iterations);
	void processHitWall(FVector HitNormal, AActor *HitActor);
	void Crouch(INT bTest=0);
	void UnCrouch(INT bTest=0);
	FRotator FindSlopeRotation(FVector FloorNormal, FRotator NewRotation);
	void SmoothHitWall(FVector HitNormal, AActor *HitActor);
	FVector NewFallVelocity(FVector OldVelocity, FVector OldAcceleration, FLOAT timeTick);
	void stepUp(FVector GravDir, FVector DesiredDir, FVector Delta, FCheckResult &Hit);

	// animation functions - temp FIXME - create animation objects with this functionality
	void UpdateMovementAnimation(FLOAT DeltaSeconds);
	void ZeroMovementAlpha(INT iStart, INT iEnd, FLOAT StepSize);

	// Controller interface
	UBOOL IsPlayer(); // FIXME: This used to be inline, but that didn't compile with static linking.
	UBOOL IsHumanControlled(); // FIXME: This used to be inline, but that didn't compile with static linking.
	UBOOL IsLocallyControlled(); // FIXME: This used to be inline, but that didn't compile with static linking.

    virtual void PostRender(FLevelSceneNode* SceneNode, FRenderInterface* RI);
    virtual APawn* GetAPawn() { return this; } // sjs

	virtual APawn* GetVehicleBase();

private:
	UBOOL Pick3DWallAdjust(FVector WallHitNormal, AActor* HitActor);
	FLOAT Swim(FVector Delta, FCheckResult &Hit); 
	FVector findWaterLine(FVector Start, FVector End);
	void SpiderstepUp(FVector DesiredDir, FVector Delta, FCheckResult &Hit);
	void calcVelocity(FVector AccelDir, FLOAT deltaTime, FLOAT maxSpeed, FLOAT friction, INT bFluid, INT bBrake, INT bBuoyant);
	int findNewFloor(FVector OldLocation, FLOAT deltaTime, FLOAT remainingTime, INT Iterations);
	int checkFloor(FVector Dir, FCheckResult &Hit);

    // jjs -
    int Get4WayDirection( void );
    void UpdateSwimming( void );
    void UpdateInAir( void );
    void UpdateOnGround( void );
    void PlayDoubleJump( void );
    void PlayIdle( void );
    void PlayRunning( void );
    void PlayLand( void );
    void SetTwistLook( int twist, int look );
    void UpdateTwistLook( float DeltaTime );
    // - jjs

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/


