/*=============================================================================
    JoltONSChopperCraft.cpp

    Jolt Physics chopper vehicle initialization.
    UpdateVehicle is defined in ONSChopperCraft.cpp (shared between Karma
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
    JoltInitChopperVehicle
-----------------------------------------------------------------------------*/

void JoltInitChopperVehicle(AONSChopperCraft* craft)
{
    printf("[JOLT-VEHICLE] Chopper vehicle initialized: %s (force-based)\n", craft->GetName());
}

#endif // WITH_PHYSICS
