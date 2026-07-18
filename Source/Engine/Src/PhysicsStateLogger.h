/*=============================================================================
    PhysicsStateLogger.h

    Utility for logging physics actor state to CSV for comparison between
    Karma reference and Jolt Physics builds.

    This is engine-level code that reads actor state, not physics-engine-specific.
    Works identically with both Karma and Jolt backends.
=============================================================================*/

#ifndef PHYSICS_STATE_LOGGER_H
#define PHYSICS_STATE_LOGGER_H

class ULevel;

// Tick the physics state logger -- writes one CSV line per physics actor per frame
void PhysicsStateLogTick(ULevel* level);

// Enable or disable physics state logging
void PhysicsStateLogEnable(UBOOL enable);

// Query whether logging is currently enabled
UBOOL PhysicsStateLogIsEnabled();

#endif // PHYSICS_STATE_LOGGER_H
