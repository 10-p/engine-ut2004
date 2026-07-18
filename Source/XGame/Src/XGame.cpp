//=============================================================================
// XGame - Native Interface Package
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#include "Engine.h"
#include "XGame.h"

IMPLEMENT_PACKAGE(XGame);

#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) XGAME_API FName XGAME_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "XGameClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

// sjs --- import natives
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "XGameClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVES_ONLY
#undef NAMES_ONLY
// --- sjs

IMPLEMENT_CLASS(UDecoText);
IMPLEMENT_CLASS(UxUtil);
IMPLEMENT_CLASS(UCachePlayers);
IMPLEMENT_CLASS(AAimedAttachment);
IMPLEMENT_CLASS(USpeciesType);

void AAimedAttachment::performPhysics(FLOAT DeltaSeconds)
{
	guard(AAimedAttachment::performPhysics);

    if ( !Instigator )
        return;

	clock(GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles));
    APawn* P = Instigator;
    FCheckResult Hit(1.f);
    FRotator R = P->Rotation;
    if (P->Physics != PHYS_Swimming && P->Physics != PHYS_Flying)
        R.Pitch = 256 * P->ViewPitch;
    FVector V = BaseOffset;
    if (R.Pitch > 32768 && DownwardBias > 0.0f)
    {
        V.Z -= (float)(65536-R.Pitch) * DownwardBias / 16384.0f;
    }
    GetLevel()->FarMoveActor(this, P->Location + R.Vector()*AimedOffset + V, 0, 1);
    GetLevel()->MoveActor(this, FVector(0.0f, 0.0f, 0.0f), R, Hit);
	unclock(GStats.DWORDStats(GEngineStats.STATS_Game_PhysicsCycles));

    unguard;
}

