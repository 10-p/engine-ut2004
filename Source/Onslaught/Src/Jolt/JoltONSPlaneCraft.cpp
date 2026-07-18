/*=============================================================================
    JoltONSPlaneCraft.cpp

    Jolt Physics plane vehicle initialization.
    UpdateVehicle is defined in ONSPlaneCraft.cpp (shared between Karma
    and Jolt builds).

    Per D-07: in Onslaught/Src/Jolt/.
=============================================================================*/

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include "OnslaughtPrivate.h"

#ifdef WITH_PHYSICS

#include "JoltAdapter.h"

JPH_SUPPRESS_WARNINGS

/*-----------------------------------------------------------------------------
    JoltInitPlaneVehicle
-----------------------------------------------------------------------------*/

void JoltInitPlaneVehicle(AONSPlaneCraft* craft)
{
    printf("[JOLT-VEHICLE] Plane vehicle initialized: %s (force-based)\n", craft->GetName());
}

#endif // WITH_PHYSICS
