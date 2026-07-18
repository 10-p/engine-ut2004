/*=============================================================================
	AActor.h.
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	// Constructors.
	AActor() {}
	void Destroy();

	// UObject interface.
	virtual INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	void ProcessEvent( UFunction* Function, void* Parms, void* Result=NULL );
	void ProcessState( FLOAT DeltaSeconds );
	UBOOL ProcessRemoteFunction( UFunction* Function, void* Parms, FFrame* Stack );
	void ProcessDemoRecFunction( UFunction* Function, void* Parms, FFrame* Stack );
	void Serialize( FArchive& Ar );
	void InitExecution();
	void PostEditChange();
	virtual void PreEditUndo();
	virtual void PostEditUndo();
	void PostLoad();
	void NetDirty(UProperty* property); 
    virtual void ScriptInit(class ALevelInfo* levelinfo); //amb
	virtual void ExecutingBadStateCode(FFrame& Stack);

	// AActor interface.
	class ULevel* GetLevel() const;
	class APawn* GetPlayerPawn() const;
	UBOOL IsPlayer() const;
	UBOOL IsOwnedBy( const AActor *TestOwner ) const;
	inline UBOOL IsBlockedBy( const AActor* Other ) const;
	UBOOL IsInZone( const AZoneInfo* Other ) const;
	UBOOL IsBasedOn( const AActor *Other ) const;
	virtual UBOOL IsJoinedTo( const AActor *Other) const;
	virtual UBOOL ShouldUpdateRelativeLocation();
	virtual void DoCleanup();

	// Editor specific
	UBOOL IsHiddenEd() { return (bHiddenEd || bHiddenEdGroup); }
	
	virtual FLOAT GetNetPriority( FVector& ViewPos, FVector& ViewDir, AActor* Sent, FLOAT Time, FLOAT Lag );
	virtual FLOAT WorldLightRadius() const {return 25.f * (LightRadius+1.0f);}
	virtual UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );
	virtual void PostEditMove() {}
	virtual void PostEditLoad() {}
	virtual void PreRaytrace() {}
	virtual void PostRaytrace() {}
	virtual void Spawned() {}
	virtual void PreNetReceive();
	virtual void PostNetReceive();
	virtual void PostNetReceiveLocation();
	virtual UMaterial* GetSkin( INT Index );
	virtual UBOOL ShouldTickInEntry() { return false; }

	// Rendering info.

	virtual FCoords ToLocal() const
	{
		return GMath.UnitCoords / Rotation / Location;
	}
	virtual FCoords ToWorld() const
	{
		return GMath.UnitCoords * Location * Rotation;
	}
	virtual FMatrix LocalToWorld() const
	{
#if 0
		FTranslationMatrix LToW( -PrePivot );
		FScaleMatrix TempScale( DrawScale3D * DrawScale );
		FRotationMatrix TempRot( Rotation );
		FTranslationMatrix TempTrans( Location );
		LToW *= TempScale;
		LToW *= TempRot;
		LToW *= TempTrans;
		return LToW;
#else
		FMatrix Result;

		FLOAT	SR	= GMath.SinTab(Rotation.Roll),
				SP	= GMath.SinTab(Rotation.Pitch),
				SY	= GMath.SinTab(Rotation.Yaw),
				CR	= GMath.CosTab(Rotation.Roll),
				CP	= GMath.CosTab(Rotation.Pitch),
				CY	= GMath.CosTab(Rotation.Yaw);
		
		FLOAT	LX	= Location.X,
				LY	= Location.Y,
				LZ	= Location.Z,
				PX	= PrePivot.X,
				PY	= PrePivot.Y,
				PZ	= PrePivot.Z;
				
		FLOAT	DX	= DrawScale3D.X * DrawScale,
				DY	= DrawScale3D.Y * DrawScale,
				DZ	= DrawScale3D.Z * DrawScale;
				
		Result.M[0][0] = CP * CY * DX;
		Result.M[0][1] = CP * DX * SY;
		Result.M[0][2] = DX * SP;
		Result.M[0][3] = 0.f;

		Result.M[1][0] = DY * ( CY * SP * SR - CR * SY );
		Result.M[1][1] = DY * ( CR * CY + SP * SR * SY );
		Result.M[1][2] = -CP * DY * SR;
		Result.M[1][3] = 0.f;

		Result.M[2][0] = -DZ * ( CR * CY * SP + SR * SY );
		Result.M[2][1] =  DZ * ( CY * SR - CR * SP * SY );
		Result.M[2][2] = CP * CR * DZ;
		Result.M[2][3] = 0.f;

		Result.M[3][0] = LX - CP * CY * DX * PX + CR * CY * DZ * PZ * SP - CY * DY * PY * SP * SR + CR * DY * PY * SY + DZ * PZ * SR * SY;
		Result.M[3][1] = LY - (CR * CY * DY * PY + CY * DZ * PZ * SR + CP * DX * PX * SY - CR * DZ * PZ * SP * SY + DY * PY * SP * SR * SY);
		Result.M[3][2] = LZ - (CP * CR * DZ * PZ + DX * PX * SP - CP * DY * PY * SR);
		Result.M[3][3] = 1.f;

		return Result;
#endif
	}
	virtual FMatrix WorldToLocal() const
	{
  		return	FTranslationMatrix(-Location) *
				FInverseRotationMatrix(Rotation) *
				FScaleMatrix(FVector( 1.f / DrawScale3D.X, 1.f / DrawScale3D.Y, 1.f / DrawScale3D.Z) / DrawScale) *
				FTranslationMatrix(PrePivot);
	}
	class FDynamicActor* GetActorRenderData();
	class FDynamicLight* GetLightRenderData();
	void ClearRenderData();
	void UpdateRenderData();

	FLOAT LifeFraction()
	{
		#ifdef _WIN32  // !!! FIXME: why do we get NaNs here? --ryan.
		return Clamp( 1.f - LifeSpan / GetClass()->GetDefaultActor()->LifeSpan, 0.f, 1.f );
		#else
		FLOAT ParentLifeSpan = GetClass()->GetDefaultActor()->LifeSpan;
		if (ParentLifeSpan == 0.f)
			return 1.f;
		return Clamp( 1.f - LifeSpan / ParentLifeSpan, 0.f, 1.f );
		#endif
	}
	FVector GetCylinderExtent() const {return FVector(CollisionRadius,CollisionRadius,CollisionHeight);}

	AActor* GetTopOwner();
	UBOOL IsPendingKill() {return bDeleteMe;}
	virtual void PostScriptDestroyed() {} // C++ notification that the script Destroyed() function has been called.
	virtual void CleanupSubObjects();		// called right before actor is permanently deleted

	// AActor collision functions.
	virtual UBOOL ShouldTrace(AActor *SourceActor, DWORD TraceFlags);
	virtual UPrimitive* GetPrimitive();
	UBOOL IsOverlapping( AActor *Other, FCheckResult* Hit=NULL );

	void ClearOctreeData();
	UBOOL IsInOctree() 
	{ 
		return OctreeNodes.Num() > 0; 
	}

	// AActor general functions.
	void BeginTouch(AActor *Other);
	void EndTouch(AActor *Other, UBOOL NoNotifySelf);
	void SetOwner( AActor *Owner );
	UBOOL IsBrush()       const;
	UBOOL IsStaticBrush() const;
	UBOOL IsMovingBrush() const;
	UBOOL IsVolumeBrush() const;
	UBOOL IsEncroacher() const;
	UBOOL IsAnimating(int Channel=0) const;
	
	virtual void NotifyBump(AActor *Other);
	void SetCollision( UBOOL NewCollideActors, UBOOL NewBlockActors);
	void SetCollisionSize( FLOAT NewRadius, FLOAT NewHeight );
	void SetDrawScale( FLOAT NewScale);
	void SetDrawScale3D( FVector NewScale3D);
	void SetStaticMesh( UStaticMesh* NewStaticMesh );
	void SetDrawType( EDrawType NewDrawType );
	virtual void SetBase(AActor *NewBase, FVector NewFloor = FVector(0,0,1), int bNotifyActor=1);
	FRotator GetViewRotation();
	virtual void NotifyAnimEnd( int Channel );

#ifdef WITH_LIPSINC
	virtual void NotifyLIPSincAnimEnd();
#endif

	virtual void UpdateAnimation(FLOAT DeltaSeconds);
	void ReplicateAnim(INT channel, FName SequenceName, FLOAT Rate, FLOAT Frame, FLOAT TweenR, FLOAT Last, UBOOL bLoop);
	void PlayReplicatedAnim();
	virtual void StartAnimPoll();
	virtual UBOOL CheckAnimFinished(int Channel);
	void UpdateTimers(FLOAT DeltaSeconds);
	virtual UBOOL CheckOwnerUpdated();
	virtual void TickAuthoritative( FLOAT DeltaSeconds );
	virtual void TickSimulated( FLOAT DeltaSeconds );
	virtual void TickSpecial( FLOAT DeltaSeconds );
    void UpdateOverlay(FLOAT DeltaSeconds);
	virtual UBOOL PlayerControlled();
	virtual UBOOL IsNetRelevantFor(APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation );
	virtual UBOOL DelayScriptReplication(FLOAT LastFullUpdateTime) { return false; }
	UBOOL AttachToBone( AActor* Attachment, FName BoneName );
	UBOOL DetachFromBone( AActor* Attachment );
	inline AActor* GetAmbientLightingActor() { return bUseLightingFromBase && Base ? Base->GetAmbientLightingActor() : this; }

	// AActor Rendering in editor
	virtual void RenderEditorInfo(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA);
	virtual void RenderEditorSelected(FLevelSceneNode* SceneNode,FRenderInterface* RI, FDynamicActor* FDA);

	// AActor audio.
	virtual FLOAT GetAmbientVolume(FLOAT Attenuation);
	void TextToSpeech( const FString& Text, FLOAT Volume );

	// Level functions
	virtual void SetZone( UBOOL bTest, UBOOL bForceRefresh );
	virtual void SetVolumes(const TArray<class AVolume*>& Volumes);
	virtual void SetVolumes();
	virtual void PostBeginPlay();

	// Physics functions.
	virtual void setPhysics(BYTE NewPhysics, AActor *NewFloor = NULL, FVector NewFloorV = FVector(0,0,1) );
	void FindBase();
	virtual void performPhysics(FLOAT DeltaSeconds);
	void physProjectile(FLOAT deltaTime, INT Iterations);
	virtual void BoundProjectileVelocity();
	virtual void processHitWall(FVector HitNormal, AActor *HitActor);
	virtual void processLanded(FVector HitNormal, AActor *HitActor, FLOAT remainingTime, INT Iterations);
	virtual void physFalling(FLOAT deltaTime, INT Iterations);
	virtual void physWalking(FLOAT deltaTime, INT Iterations);
	virtual void physFlying(FLOAT deltaTime, INT Iterations);
	void physicsRotation(FLOAT deltaTime);
	int fixedTurn(int current, int desired, int deltaRate); 
	void TwoWallAdjust(FVector &DesiredDir, FVector &Delta, FVector &HitNormal, FVector &OldHitNormal, FLOAT HitTime);
	void physTrailer(FLOAT DeltaTime);
	void physRootMotion(FLOAT DeltaTime);
	UBOOL moveSmooth(FVector Delta);
	virtual FRotator FindSlopeRotation(FVector FloorNormal, FRotator NewRotation);
	void UpdateRelativeRotation();
	void GetNetBuoyancy(FLOAT &NetBuoyancy, FLOAT &NetFluidFriction);
	virtual void SmoothHitWall(FVector HitNormal, AActor *HitActor);
	virtual void stepUp(FVector GravDir, FVector DesiredDir, FVector Delta, FCheckResult &Hit);
	virtual UBOOL ShrinkCollision(AActor *HitActor, const FVector &StartLocation);
    
#ifdef WITH_KARMA
    virtual McdModelID getKModel() const;
#endif

#if defined(WITH_KARMA) || defined(WITH_PHYSICS)
	void KWake();
	void KGetRigidBodyState(FKRigidBodyState* state);
	UBOOL KIsAwake();
	void KDrawRigidBodyState(FKRigidBodyState* state, UBOOL AltColour);
	void KAddForces(FVector force, FVector torque);
	void KAddImpulse(FVector impulse, FVector position, FName boneName=NAME_None);
	void KAddAngularImpulse(FVector angImpulse);

	virtual void physKarma(FLOAT DeltaTime);
	inline void physKarma_internal(FLOAT DeltaTime);

	void preKarmaStep_skeletal(FLOAT DeltaTime);
	void postKarmaStep_skeletal();

	virtual void preKarmaStep(FLOAT DeltaTime);
	virtual void postKarmaStep();

	void physKarmaRagDoll(FLOAT DeltaTime);
	inline void physKarmaRagDoll_internal(FLOAT DeltaTime);
	void KFreezeRagdoll();

	virtual void preContactUpdate();
	virtual UBOOL KRepulsorsShouldHit(AActor* Actor) { return true; }
#endif
    
	// AI functions.
	virtual UBOOL ReachedBy(APawn * P, FVector Loc) { return false; }
	virtual UBOOL NoReachDistance() { return false; }
	void CheckNoiseHearing(FLOAT Loudness);
	int TestCanSeeMe(class APlayerController *Viewer);
	virtual INT AddMyMarker(AActor *S) { return 0; };
	virtual void ClearMarker() {};
	virtual AActor* AssociatedLevelGeometry();
	virtual UBOOL HasAssociatedLevelGeometry(AActor *Other);
	virtual FVector GetDestination(AController *C);
	FVector SuggestFallVelocity(FVector Dest, FVector Start, FLOAT XYSpeed, FLOAT BaseZ, FLOAT JumpZ, FLOAT MaxXYSpeed);

	// Animation functions
	virtual UBOOL PlayAnim(INT Channel,FName SequenceName, FLOAT PlayAnimRate, FLOAT TweenTime, INT Loop);
	virtual FVector GetRootLocation();

	// Special editor behavior
	AActor* GetHitActor();
	virtual void CheckForErrors();

	// path creation
	virtual void PrePath() {};
	virtual void PostPath() {};

	// Projectors
	void AttachProjector( AProjector* Projector );
	void DetachProjector( AProjector* Projector );
	virtual AActor* GetProjectorBase();

 	virtual UBOOL IsABrush()  {return false;}
	virtual UBOOL IsAMover() {return false;}
	virtual UBOOL IsAVolume() {return false;}

    // amb ---
    virtual void PostRender(FLevelSceneNode* SceneNode, FRenderInterface* RI) {}
    // --- amb

    virtual APlayerController* GetAPlayerController() { return NULL; }
    virtual APawn* GetAPawn() { return NULL; } 
    virtual class AProjectile* GetAProjectile() { return NULL; }; 
    virtual class AMover* GetAMover() { return NULL; } 
    virtual APlayerController* GetTopPlayerController() 
    {
        AActor* TopActor = GetTopOwner();
        return TopActor ? TopActor->GetAPlayerController() : NULL;
    }

	// Natives.
	DECLARE_FUNCTION(execPollSleep)
	DECLARE_FUNCTION(execPollFinishAnim)
	DECLARE_FUNCTION(execPollFinishInterpolation)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

