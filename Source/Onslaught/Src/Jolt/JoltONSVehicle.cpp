/*=============================================================================
    JoltONSVehicle.cpp

    Jolt Physics ONS vehicle dispatch.

    All vehicle Tick/ProcessCarInput/ChangeGear/PackState/PostNetReceive
    functions are now compiled from the original ONS*.cpp files with
    guards changed to #if defined(WITH_KARMA) || defined(WITH_PHYSICS).

    This file provides:
    1. The vehicle init dispatch function that routes vehicle subclasses
       to their Jolt-specific init functions
    2. Static registration of the dispatch function
=============================================================================*/

#include <Jolt/Jolt.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <Jolt/Physics/Body/BodyInterface.h>

#include "OnslaughtPrivate.h"

#ifdef WITH_PHYSICS

#include "JoltAdapter.h"
#include "JoltGlobals.h"

JPH_SUPPRESS_WARNINGS

/*-----------------------------------------------------------------------------
    External vehicle init functions from per-vehicle Jolt files
-----------------------------------------------------------------------------*/

extern void JoltInitWheeledVehicle(AONSWheeledCraft* craft);
extern void JoltInitHoverVehicle(AONSHoverCraft* craft);
extern void JoltInitChopperVehicle(AONSChopperCraft* craft);
extern void JoltInitPlaneVehicle(AONSPlaneCraft* craft);
extern void JoltInitTreadVehicle(AONSTreadCraft* craft);

/*-----------------------------------------------------------------------------
    Vehicle dispatch function
-----------------------------------------------------------------------------*/

static void JoltInitVehicleDispatch(ASVehicle* vehicle)
{
    AONSWheeledCraft* wheeled = Cast<AONSWheeledCraft>(vehicle);
    if(wheeled)
    {
        JoltInitWheeledVehicle(wheeled);
        return;
    }

    AONSHoverCraft* hover = Cast<AONSHoverCraft>(vehicle);
    if(hover)
    {
        JoltInitHoverVehicle(hover);
        return;
    }

    AONSChopperCraft* chopper = Cast<AONSChopperCraft>(vehicle);
    if(chopper)
    {
        JoltInitChopperVehicle(chopper);
        return;
    }

    AONSPlaneCraft* plane = Cast<AONSPlaneCraft>(vehicle);
    if(plane)
    {
        JoltInitPlaneVehicle(plane);
        return;
    }

    AONSTreadCraft* tread = Cast<AONSTreadCraft>(vehicle);
    if(tread)
    {
        JoltInitTreadVehicle(tread);
        return;
    }

    printf("[JOLT-VEHICLE] Unknown SVehicle subclass: %s (%s)\n",
        vehicle->GetName(), vehicle->GetClass()->GetName());
}

/*-----------------------------------------------------------------------------
    UpdateVehicle dispatch -- routes to correct subclass UpdateVehicle

    C++ vtable doesn't work because the vtable for ONS vehicle classes
    is emitted in Onslaught.cpp, which doesn't see the function body
    overrides. We bypass the vtable with this explicit dispatch.
-----------------------------------------------------------------------------*/

static void JoltUpdateVehicleDispatch(ASVehicle* vehicle, FLOAT DeltaTime)
{
    // DIAGNOSTIC: log dispatch
    {
        static INT sDispatch = 0;
        sDispatch++;
        if(sDispatch <= 10 || sDispatch % 5000 == 0)
        {
            FILE* lf = fopen("/tmp/jolt-physics.log", "a");
            if(lf) {
                UBOOL isWheeled = vehicle->IsA(AONSWheeledCraft::StaticClass());
                UBOOL isTread = vehicle->IsA(AONSTreadCraft::StaticClass());
                UBOOL isHover = vehicle->IsA(AONSHoverCraft::StaticClass());
                UBOOL isChopper = vehicle->IsA(AONSChopperCraft::StaticClass());
                UBOOL isPlane = vehicle->IsA(AONSPlaneCraft::StaticClass());
                fprintf(lf, "UV-DISP #%d W=%d T=%d H=%d C=%d P=%d ctrl=%d\n",
                    sDispatch, isWheeled, isTread, isHover, isChopper, isPlane,
                    vehicle->Controller ? 1 : 0);
                fclose(lf);
            }
        }
    }

    AONSWheeledCraft* wheeled = Cast<AONSWheeledCraft>(vehicle);
    if(wheeled)
    {
        wheeled->AONSWheeledCraft::UpdateVehicle(DeltaTime);
        return;
    }

    AONSHoverCraft* hover = Cast<AONSHoverCraft>(vehicle);
    if(hover)
    {
        hover->AONSHoverCraft::UpdateVehicle(DeltaTime);
        return;
    }

    AONSChopperCraft* chopper = Cast<AONSChopperCraft>(vehicle);
    if(chopper)
    {
        chopper->AONSChopperCraft::UpdateVehicle(DeltaTime);
        return;
    }

    AONSPlaneCraft* plane = Cast<AONSPlaneCraft>(vehicle);
    if(plane)
    {
        plane->AONSPlaneCraft::UpdateVehicle(DeltaTime);
        return;
    }

    AONSTreadCraft* tread = Cast<AONSTreadCraft>(vehicle);
    if(tread)
    {
        tread->AONSTreadCraft::UpdateVehicle(DeltaTime);
        return;
    }

    // Unknown subclass — call base (no-op)
    vehicle->UpdateVehicle(DeltaTime);
}

/*-----------------------------------------------------------------------------
    Static registration
-----------------------------------------------------------------------------*/

typedef void (*JoltUpdateVehicleFunc)(ASVehicle*, FLOAT);
extern ENGINE_API JoltUpdateVehicleFunc GJoltUpdateVehicleFunc;

struct JoltVehicleRegistrar
{
    JoltVehicleRegistrar()
    {
        GJoltInitVehicleFunc = &JoltInitVehicleDispatch;
        GJoltUpdateVehicleFunc = &JoltUpdateVehicleDispatch;
        fprintf(stderr, "[JOLT-REG] Vehicle dispatch registered: init=%p update=%p\n",
            (void*)GJoltInitVehicleFunc, (void*)GJoltUpdateVehicleFunc);
    }
};

static JoltVehicleRegistrar GJoltVehicleRegistrar;

#endif // WITH_PHYSICS
