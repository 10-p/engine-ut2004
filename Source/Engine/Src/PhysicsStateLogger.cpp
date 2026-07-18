/*=============================================================================
    PhysicsStateLogger.cpp

    Vehicle position logger. Auto-logs SVehicle actor positions to
    /tmp/vehicle-log.txt for comparison between Karma and Jolt builds.

    Writes every frame for first 10, then every 30 frames up to 3000.
    Logs: frame, actor name, class, position, velocity, driver status,
    throttle, steering, health.
=============================================================================*/

#include "EnginePrivate.h"
#include "PhysicsStateLogger.h"

/*-----------------------------------------------------------------------------
    Static state
-----------------------------------------------------------------------------*/

static FILE* GVehLogFile = NULL;
static INT GVehLogFrame = 0;
static UBOOL GVehLogOpened = 0;

/*-----------------------------------------------------------------------------
    PhysicsStateLogEnable / PhysicsStateLogIsEnabled
-----------------------------------------------------------------------------*/

void PhysicsStateLogEnable(UBOOL enable)
{
    // No-op, auto-enabled
}

UBOOL PhysicsStateLogIsEnabled()
{
    return 1;
}

/*-----------------------------------------------------------------------------
    PhysicsStateLogTick — auto-logs vehicle state
-----------------------------------------------------------------------------*/

void PhysicsStateLogTick(ULevel* level)
{
    guard(PhysicsStateLogTick);

    if(!level)
        return;

    // Open log file on first call
    if(!GVehLogOpened)
    {
        GVehLogFile = fopen("/tmp/vehicle-log.txt", "w");
        if(GVehLogFile)
        {
            setbuf(GVehLogFile, NULL);
            fprintf(GVehLogFile, "=== Vehicle State Log ===\n");
        }
        GVehLogOpened = 1;
    }

    if(!GVehLogFile)
        return;

    GVehLogFrame++;

    // Log on select frames: every frame for first 10, then every 30 frames, up to 3000
    UBOOL shouldLog = (GVehLogFrame <= 10) || (GVehLogFrame % 30 == 0);
    if(!shouldLog || GVehLogFrame > 3000)
        return;

    INT vehicleCount = 0;
    INT movingCount = 0;
    INT destroyedCount = 0;

    for(INT i = 0; i < level->Actors.Num(); i++)
    {
        AActor* actor = level->Actors(i);
        if(!actor || actor->bDeleteMe)
            continue;

        ASVehicle* veh = Cast<ASVehicle>(actor);
        if(!veh)
            continue;

        vehicleCount++;

        UBOOL hasDriver = (veh->Controller != NULL);
        FLOAT speed = veh->Velocity.Size();
        if(speed > 1.0f) movingCount++;

        fprintf(GVehLogFile,
            "F=%d %s cls=%s pos=(%.1f,%.1f,%.1f) vel=(%.1f,%.1f,%.1f) spd=%.1f drv=%d thr=%.2f str=%.2f hp=%d/%d phys=%d\n",
            GVehLogFrame, veh->GetName(),
            veh->GetClass()->GetName(),
            veh->Location.X, veh->Location.Y, veh->Location.Z,
            veh->Velocity.X, veh->Velocity.Y, veh->Velocity.Z,
            speed,
            (int)hasDriver,
            veh->Throttle, veh->Steering,
            (int)veh->Health, (int)veh->HealthMax,
            (int)veh->Physics);

        if(veh->Health <= 0) destroyedCount++;
    }

    // Summary line
    if(vehicleCount > 0)
    {
        fprintf(GVehLogFile, "F=%d SUMMARY: %d vehicles, %d moving, %d destroyed\n",
            GVehLogFrame, vehicleCount, movingCount, destroyedCount);
    }

    unguard;
}
