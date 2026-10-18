/*=============================================================================
    JoltONSHoverCraft.cpp

    Jolt Physics hover vehicle initialization.
    UpdateVehicle and KRepulsorsShouldHit are defined in ONSHoverCraft.cpp
    (shared between Karma and Jolt builds).

    In Onslaught/Src/Jolt/.
=============================================================================*/

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include "OnslaughtPrivate.h"

#ifdef WITH_PHYSICS

#include "JoltAdapter.h"

JPH_SUPPRESS_WARNINGS

/*-----------------------------------------------------------------------------
    JoltInitHoverVehicle -- no-op (body already created by KInitActorDynamics)
    Hover vehicles use the standard dynamic body, no VehicleConstraint needed.
-----------------------------------------------------------------------------*/

void JoltInitHoverVehicle(AONSHoverCraft* craft)
{
    printf("[JOLT-VEHICLE] Hover vehicle initialized: %s (force-based)\n", craft->GetName());
}

#endif // WITH_PHYSICS
