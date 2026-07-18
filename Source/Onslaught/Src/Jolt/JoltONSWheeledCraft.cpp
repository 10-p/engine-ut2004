/*=============================================================================
    JoltONSWheeledCraft.cpp

    Jolt-specific wheeled vehicle initialization.

    The UpdateVehicle, preKarmaStep, Tick, ProcessCarInput, etc. functions
    are compiled from the original ONSWheeledCraft.cpp (guard changed to
    #if defined(WITH_KARMA) || defined(WITH_PHYSICS)) — they use
    KAddForces/KGetRigidBodyState which work in both builds.

    This file provides only JoltInitWheeledVehicle for body setup.
=============================================================================*/

// Jolt header BEFORE OnslaughtPrivate.h to fix operator new/delete noexcept conflict
#include <Jolt/Jolt.h>

#include "OnslaughtPrivate.h"

#ifdef WITH_PHYSICS

#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>
#include <Jolt/Physics/Body/Body.h>

#include "JoltAdapter.h"
#include "JoltGlobals.h"

JPH_SUPPRESS_WARNINGS

/*-----------------------------------------------------------------------------
    JoltInitWheeledVehicle -- populate wheel positions from bones
-----------------------------------------------------------------------------*/

void JoltInitWheeledVehicle(AONSWheeledCraft* craft)
{
    guard(JoltInitWheeledVehicle);

    USkeletalMesh* smesh = Cast<USkeletalMesh>(craft->Mesh);
    if(smesh && craft->Wheels.Num() > 0)
    {
        TArray<FCoords> RefBases;
        RefBases.Add(smesh->RefSkeleton.Num());
        for(INT b = 0; b < smesh->RefSkeleton.Num(); b++)
        {
            FQuatToFCoordsFast(smesh->RefSkeleton(b).BonePos.Orientation,
                               smesh->RefSkeleton(b).BonePos.Position, RefBases(b));
            if(b > 0)
            {
                INT Parent = smesh->RefSkeleton(b).ParentIndex;
                RefBases(b) = RefBases(b).ApplyPivot(RefBases(Parent));
            }
        }

        FMatrix MeshRot = FRotationMatrix(smesh->RotOrigin);
        FVector XAxis(MeshRot.M[0][0], MeshRot.M[1][0], MeshRot.M[2][0]);
        FVector YAxis(MeshRot.M[0][1], MeshRot.M[1][1], MeshRot.M[2][1]);
        FVector ZAxis(MeshRot.M[0][2], MeshRot.M[1][2], MeshRot.M[2][2]);
        MeshRot.M[3][0] += -smesh->Origin | XAxis;
        MeshRot.M[3][1] += -smesh->Origin | YAxis;
        MeshRot.M[3][2] += -smesh->Origin | ZAxis;

        for(INT i = 0; i < craft->Wheels.Num(); i++)
        {
            USVehicleWheel* vw = craft->Wheels(i);
            if(!vw) continue;
            INT BoneIndex = smesh->MatchRefBone(vw->BoneName);
            if(BoneIndex != INDEX_NONE)
            {
                FMatrix BoneMatrix = RefBases(BoneIndex).Matrix() * MeshRot;
                FMatrix BoneMatrix2 = RefBases(BoneIndex).Matrix().Inverse() * MeshRot;
                BoneMatrix.M[3][0] = BoneMatrix2.M[3][0];
                BoneMatrix.M[3][1] = BoneMatrix2.M[3][1];
                BoneMatrix.M[3][2] = BoneMatrix2.M[3][2];
                vw->WheelPosition = smesh->Scale * BoneMatrix.TransformFVector(vw->BoneOffset);
            }
        }
    }

    printf("[JOLT-VEHICLE] Wheeled vehicle initialized: %s (force-based)\n",
        craft->GetName());

    unguard;
}

#endif // WITH_PHYSICS
