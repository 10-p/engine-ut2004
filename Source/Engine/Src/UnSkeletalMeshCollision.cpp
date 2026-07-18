/*=============================================================================
	UnSkeletalMeshCollision.cpp: Skeletal mesh collision code
	Copyright 2003 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"

UBOOL USkeletalMesh::LineCheck(
                FCheckResult &Result,
                AActor* Owner,
                const FVector& End,
                const FVector& Start,
                const FVector& Extent,
                DWORD ExtraNodeFlags,
				DWORD TraceFlags)
{
	guard(USkeletalMesh::LineCheck);

	if ( UseCylinderCollision(Owner) )
		return Super::LineCheck(Result, Owner, End, Start, Extent, ExtraNodeFlags, TraceFlags);

	if (CollisionStaticMesh && (ExtraNodeFlags & NF_UseCollisionStaticMesh))
	{
		FVector OldDrawScale3D = Owner->DrawScale3D;
		Owner->DrawScale3D *= Scale;
		UBOOL RetVal = CollisionStaticMesh->LineCheck(Result, Owner, End, Start, Extent, ExtraNodeFlags, TraceFlags);
		Owner->DrawScale3D = OldDrawScale3D;
		return RetVal;
	}

    USkeletalMeshInstance* inst = Cast<USkeletalMeshInstance>(this->MeshGetInstance(Owner));

	// If we have simple collision stuff, do the check against that
	if( inst && (BoneCollisionSpheres.Num() > 0 || BoneCollisionBoxes.Num() > 0) )
	{
		Result.Time = 1.0f;

		// Make sure bones are up to date.
		INT DummyVerts;
		inst->GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_BonesOnly);

		// // BOXES // //
		if( BoneCollisionBoxModels.Num() == BoneCollisionBoxes.Num() )
		{
			// Check line against each BSP model
			for(INT i=0; i<BoneCollisionBoxModels.Num() ; i++)
			{
				FSkelBoneBox* b = &BoneCollisionBoxes(i);

				if( Extent.IsZero() )
				{
					if( !b->bBlockZeroExtent )
						continue;
				}
				else
				{
					if( !b->bBlockNonZeroExtent )
						continue;
				}

				UModel* boneModel = BoneCollisionBoxModels(i);

				INT boneIx = inst->MatchRefBone( b->BoneName );
				check(boneIx != INDEX_NONE);
				FCoords boneC = inst->GetBoneCoords(boneIx);

				// Internally UModel::LineCheck calls LocalToWorld(), which uses Location, Rotation and PrePivot
				// So we trick it by changing them just before we call LineCheck for each bone.
				FVector boneLocation = boneC.Origin;
				FRotator boneRotation = boneC.OrthoRotation();
				FVector bonePrePivot = FVector(0, 0, 0);
				
				Exchange( boneLocation, Owner->Location );
				Exchange( boneRotation, Owner->Rotation );
				Exchange( bonePrePivot, Owner->PrePivot );

				FCheckResult Hit(0.0);
				UBOOL hitSomething = !boneModel->LineCheck(Hit, Owner, End, Start, Extent, ExtraNodeFlags, TraceFlags);

				Exchange( boneLocation, Owner->Location );
				Exchange( boneRotation, Owner->Rotation );
				Exchange( bonePrePivot, Owner->PrePivot );

				// If this is the closest hit yet, copy this result.
				if(hitSomething && Hit.Time < Result.Time)
					Result = Hit;
			}
		}
		else
			debugf(TEXT("BoneCollisionBoxModels/BoneCollisionBoxes Number Mismatch."));

		// // SPHERES // //
		// Check line against each sphere
		for(INT i=0; i<BoneCollisionSpheres.Num() ; i++)
		{
			//FSkelBoneSphere* s = &BoneCollisionSpheres(i);
			// JTODO: Write extent-line/sphere test. Anyone think of something better than a minkowski sum?
		}

#if 0
		if( !Extent.IsZero() )
		{
			FVector hitPos = Start + (Result.Time * (End - Start));
			GTempLineBatcher->AddStayingLine(Start, hitPos, FColor(0, 0, 255));
			GTempLineBatcher->AddStayingLine(hitPos, End, FColor(255, 0, 0));
		}
#endif
		
		// If we hit something during all of that, return 0.
		if(Result.Time < 1.0f)
			return 0;
		else
			return 1;
	}

#ifdef WITH_KARMA
	// If its not a zero extent check against a ragdoll, use the superclass (cylinder) line check.
    if(Extent != FVector(0, 0, 0) || (Owner->Physics != PHYS_KarmaRagDoll))
    {
        //debugf(TEXT("USkeletalMesh::LineCheck - can only test zero-extent."));
        return Super::LineCheck(Result, Owner, End, Start, Extent, ExtraNodeFlags, TraceFlags);
    }

	////////////////////////////
	// Zero-extent check against a ragdoll

    if(!inst)
        return 1;

    MeVector3 meStart, meEnd;
    KU2MEPosition(meStart, Start);
    KU2MEPosition(meEnd, End);

    McdLineSegIntersectResult overallRes;
    overallRes.distance = MEINFINITY;
    int hitBone = -1;

    int kbIdx;
    for(kbIdx=0; kbIdx < inst->KSkelModels.Num(); kbIdx++)
    {
        McdModelID model = inst->KSkelModels(kbIdx);
        if(model)
		{
			if(!McdModelGetSpace(model) && McdGeometryGetTypeId(McdModelGetGeometry(model)) != kMcdGeometryTypeNull)
				McdModelUpdate(model);

            McdLineSegIntersectResult localRes;
            McdLineSegIntersectFnPtr lifn;
            lifn = McdGeometryGetLineSegIntersectFnPtr(McdModelGetGeometry(model));

            if(lifn && (*lifn)(model, meStart, meEnd, &localRes))
            {
                /* If this is the nearest hit, use it. */
                if(hitBone == -1 || localRes.distance < overallRes.distance)
                {
                    overallRes = localRes;
                    hitBone = kbIdx;
                }
            }
        }
    }

    if(hitBone != -1) /* we hit something - construct result */
    {
        KME2UPosition(&Result.Location, overallRes.position);

        Result.Normal.X = overallRes.normal[0];
        Result.Normal.Y = overallRes.normal[1];
        Result.Normal.Z = overallRes.normal[2];

        Result.Primitive = this;

        MeVector3 line;
        MeVector3Subtract(line, meEnd, meStart);
        MeReal length = MeVector3Magnitude(line);
        Result.Time = overallRes.distance / length;

        Result.Item = hitBone;
        Result.Actor = Owner;

        inst->KLastTraceHit = hitBone;

        return 0;
    }
    else
	{
        inst->KLastTraceHit = -1;

        return 1;
	}
	////////////////////////////
#else
	return Super::LineCheck(Result, Owner, End, Start, Extent, ExtraNodeFlags, TraceFlags);
#endif // WITH_KARMA

	unguard;
}

UBOOL USkeletalMesh::PointCheck(
	FCheckResult&	Result,
	AActor*			Owner,
	const FVector&	Location,
	const FVector&	Extent,
	DWORD           ExtraNodeFlags)
{
	guard(USkeletalMesh::PointCheck);

	if ( Owner->bUseCylinderCollision )
		return Super::PointCheck(Result, Owner, Location, Extent, ExtraNodeFlags);

    USkeletalMeshInstance* inst = Cast<USkeletalMeshInstance>(this->MeshGetInstance(Owner));

	if( inst && (BoneCollisionSpheres.Num() > 0 || BoneCollisionBoxes.Num() > 0) )
	{
		Result.Time = 1.0f;

		// Make sure bones are up to date.
		INT DummyVerts;
		inst->GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_BonesOnly);

		// // BOXES // //
		if( BoneCollisionBoxModels.Num() == BoneCollisionBoxes.Num() )
		{
			// Check point against each BSP model
			for(INT i=0; i<BoneCollisionBoxModels.Num() ; i++)
			{
				FSkelBoneBox* b = &BoneCollisionBoxes(i);

				if( Extent.IsZero() )
				{
					if( !b->bBlockZeroExtent )
						continue;
				}
				else
				{
					if( !b->bBlockNonZeroExtent )
						continue;
				}

				UModel* boneModel = BoneCollisionBoxModels(i);

				INT boneIx = inst->MatchRefBone( b->BoneName );
				check(boneIx != INDEX_NONE);
				FCoords boneC = inst->GetBoneCoords(boneIx);

				// Internally UModel::LineCheck calls LocalToWorld(), which uses Location, Rotation and PrePivot
				// So we trick it by changing them just before we call LineCheck for each bone.
				FVector boneLocation = boneC.Origin;
				FRotator boneRotation = boneC.OrthoRotation();
				FVector bonePrePivot = FVector(0, 0, 0);
				
				Exchange( boneLocation, Owner->Location );
				Exchange( boneRotation, Owner->Rotation );
				Exchange( bonePrePivot, Owner->PrePivot );

				FCheckResult Hit(0.0);
				UBOOL result = !boneModel->PointCheck(Hit, Owner, Location, Extent, ExtraNodeFlags);

				Exchange( boneLocation, Owner->Location );
				Exchange( boneRotation, Owner->Rotation );
				Exchange( bonePrePivot, Owner->PrePivot );

				if(result)
				{
					Result = Hit;
					return 0;
				}
			}
		}
		else
			debugf(TEXT("BoneCollisionBoxModels/BoneCollisionBoxes Number Mismatch."));

		// // SPHERES // //
		// Check line against each sphere
		for(INT i=0; i<BoneCollisionSpheres.Num() ; i++)
		{
			//FSkelBoneSphere* s = &BoneCollisionSpheres(i);
			// JTODO: Write extent-line/sphere test. Anyone think of something better than a minkowski sum?
		}

		// Didn't hit anything
		return 1;
	}

	// If no collision primitives - do the normal thing
	return Super::PointCheck(Result, Owner, Location, Extent, ExtraNodeFlags);

	unguard;
}

UBOOL USkeletalMesh::UseCylinderCollision( const AActor* Owner )
{
	guardSlow(USkeletalMesh::UseCylinderCollision);

	// Dont use cylinder in ragdoll
	if( Owner->Physics == PHYS_KarmaRagDoll )
		return false;

	if ( Owner->bUseCylinderCollision )
		return true;

	// Dont use cylinder if we have collision prims 
	// JTODO: flag for this instead?
	if( BoneCollisionSpheres.Num() > 0 || BoneCollisionBoxes.Num() > 0 )
		return false;

	return true;

	unguardSlow;
}


static FBox CalcPrimBox(USkeletalMeshInstance* inst )
{
	FBox outBox(0);

	AActor* Owner = inst->GetActor();
	check(Owner);

	USkeletalMesh* smesh = Cast<USkeletalMesh>(inst->GetMesh());
	check(smesh)

	// Make sure bones are up to date.
	INT DummyVerts;
	inst->GetFrame( Owner, NULL, NULL, 0, DummyVerts, GF_BonesOnly);

	for(INT i=0; i<smesh->BoneCollisionBoxModels.Num() ; i++)
	{
		FSkelBoneBox* b = &smesh->BoneCollisionBoxes(i);
		UModel* boneModel = smesh->BoneCollisionBoxModels(i);

		// Get bone local->world transform
		INT boneIx = inst->MatchRefBone( b->BoneName );
		check(boneIx != INDEX_NONE);
		FCoords boneC = inst->GetBoneCoords(boneIx).Inverse();
		//FCoords boxC = boneC * FCoords( b->Offset ); // Box local->world transform

		// Transform umodel bounding box to world
		FBox primBox = boneModel->BoundingBox.TransformBy( boneC );

		//GTempLineBatcher->AddBox(primBox, FColor(255,255,0));

		// Add to running total
	    outBox += primBox;
	}

	for(INT i=0; i<smesh->BoneCollisionSpheres.Num(); i++)
	{
		FSkelBoneSphere* s = &smesh->BoneCollisionSpheres(i);

		// Get bone local->world transform
		INT boneIx = inst->MatchRefBone( s->BoneName );
		check(boneIx != INDEX_NONE);
		FCoords boneC = inst->GetBoneCoords(boneIx).Inverse();
		FVector sphereC = s->Offset.TransformPointBy( boneC ); // Find world-space sphere center

		FVector rVec(s->Radius, s->Radius, s->Radius);
		FBox primBox(0);
		primBox += (sphereC + rVec);
		primBox += (sphereC - rVec);

		//GTempLineBatcher->AddBox(primBox, FColor(0,255,255));

	    outBox += primBox;
	}

	//GTempLineBatcher->AddBox(outBox, FColor(255,0,255));

	return outBox;

}

// Calculate the collision bounding box for this SkeletalMesh using Karma collision.
FBox USkeletalMesh::GetCollisionBoundingBox( const AActor* Owner ) const
{
	guard(USkeletalMesh::GetCollisionBoundingBox);

	if( !Owner || !Owner->MeshInstance )
		return Super::GetCollisionBoundingBox(Owner);

    USkeletalMeshInstance* inst = Cast<USkeletalMeshInstance>(Owner->MeshInstance);

	if(!inst)
		return Super::GetCollisionBoundingBox(Owner);

	if( BoneCollisionSpheres.Num() > 0 || BoneCollisionBoxes.Num() > 0 )
		return CalcPrimBox(inst);

#if defined(WITH_KARMA) || defined(WITH_PHYSICS)
	if(inst->KFrozen)
		return inst->KSkelBox;

	if(inst->KSkelModels.Num() == 0 || !inst->KSkelIsInitialised)
		return Super::GetCollisionBoundingBox(Owner);

	return inst->KSkelBox;
#else
	return Super::GetCollisionBoundingBox(Owner);
#endif
	unguard;
}