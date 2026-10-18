/*=============================================================================
    JoltONSTreadCraft.cpp

    Jolt Physics treaded vehicle initialization.
    UpdateVehicle is defined in ONSTreadCraft.cpp (shared between Karma
    and Jolt builds).

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
    JoltInitTreadVehicle
-----------------------------------------------------------------------------*/

void JoltInitTreadVehicle(AONSTreadCraft* craft)
{
    printf("[JOLT-VEHICLE] Tread vehicle initialized: %s (force-based)\n", craft->GetName());
}

#endif // WITH_PHYSICS
