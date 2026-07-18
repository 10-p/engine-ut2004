#ifndef _MDTCARWHEEL_H
#define _MDTCARWHEEL_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.18.8.2 $

   This software and its accompanying manuals have been developed
   by MathEngine PLC ("MathEngine") and the copyright and all other
   intellectual property rights in them belong to MathEngine. All
   rights conferred by law (including rights under international
   copyright conventions) are reserved to MathEngine. This software
   may also incorporate information which is confidential to
   MathEngine.

   Save to the extent permitted by law, or as otherwise expressly
   permitted by MathEngine, this software and the manuals must not
   be copied (in whole or in part), re-arranged, altered or adapted
   in any way without the prior written consent of the Company. In
   addition, the information contained in the software may not be
   disseminated without the prior written consent of MathEngine.

 */

/** @file
 * MdtCarWheel API functions.
 */

#include <MePrecision.h>
#include <MdtTypes.h>


#ifdef __cplusplus
extern "C"
{
#endif


MEPUBLIC
MdtCarWheelID     MEAPI MdtCarWheelCreate(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtCarWheelReset(const MdtCarWheelID j);
MEPUBLIC
MdtConstraintID   MEAPI MdtCarWheelQuaConstraint(const MdtCarWheelID j);
MEPUBLIC
MdtCarWheelID     MEAPI MdtConstraintDCastCarWheel(const MdtConstraintID c);

#define                 MdtCarWheelDestroy(j) \
                            MdtConstraintDestroy(MdtCarWheelQuaConstraint(j))
#define                 MdtCarWheelEnable(j) \
                            MdtConstraintEnable(MdtCarWheelQuaConstraint(j))
#define                 MdtCarWheelDisable(j) \
                            MdtConstraintDisable(MdtCarWheelQuaConstraint(j))
#define                 MdtCarWheelIsEnabled(j) \
                            MdtConstraintIsEnabled(MdtCarWheelQuaConstraint(j))
#define                 MdtCarWheelSetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtCarWheelQuaConstraint(j),k)
#define                 MdtCarWheelGetSortKey(j) \
                            MdtConstraintGetSortKey(MdtCarWheelQuaConstraint(j))
/*
  Car wheel joint accessors.
*/

#define           MdtCarWheelGetPosition(j, p) \
                        MdtConstraintGetPosition(MdtCarWheelQuaConstraint(j), p)
#define           MdtCarWheelGetSteeringAndHingeAxes(j, bodyindex, p, o) \
                        MdtConstraintBodyGetAxes(MdtCarWheelQuaConstraint(j), bodyindex, p, o)                            MeVector3 v);
MEPUBLIC
void              MEAPI MdtCarWheelGetSteeringAxis(const MdtCarWheelID j,
                            MeVector3 v);
MEPUBLIC
MeReal            MEAPI MdtCarWheelGetSteeringAngle(const MdtCarWheelID j);
MEPUBLIC
MeReal            MEAPI MdtCarWheelGetSteeringAngleRate(const MdtCarWheelID j);
MEPUBLIC
void              MEAPI MdtCarWheelGetHingeAxis(const MdtCarWheelID j,
                            MeVector3 v);
MEPUBLIC
MeReal            MEAPI MdtCarWheelGetHingeAngle(const MdtCarWheelID j);
MEPUBLIC
MeReal            MEAPI MdtCarWheelGetHingeAngleRate(const MdtCarWheelID j);

MEPUBLIC
MeReal            MEAPI MdtCarWheelGetSuspensionHeight(const MdtCarWheelID j);
MEPUBLIC
MeReal            MEAPI MdtCarWheelGetSuspensionRate(const MdtCarWheelID j);
MEPUBLIC
MeReal            MEAPI MdtCarWheelGetSteeringMotorDesiredVelocity(
                            const MdtCarWheelID j);
MEPUBLIC
MeReal            MEAPI MdtCarWheelGetSteeringMotorMaxForce(
                            const MdtCarWheelID j);
MEPUBLIC
MeBool            MEAPI MdtCarWheelIsSteeringLocked(const MdtCarWheelID j);
MEPUBLIC
MeReal            MEAPI MdtCarWheelGetHingeMotorDesiredVelocity(
                            const MdtCarWheelID j);
MEPUBLIC
MeReal            MEAPI MdtCarWheelGetHingeMotorMaxForce(
                            const MdtCarWheelID j);
MEPUBLIC
MeReal            MEAPI MdtCarWheelGetSuspensionHighLimit(
                            const MdtCarWheelID j);
MEPUBLIC
MeReal            MEAPI MdtCarWheelGetSuspensionLowLimit(
                            const MdtCarWheelID j);
MEPUBLIC
MeReal            MEAPI MdtCarWheelGetSuspensionLimitSoftness(
                            const MdtCarWheelID j);
MEPUBLIC
MeReal            MEAPI MdtCarWheelGetSuspensionReference(
                            const MdtCarWheelID j);
MEPUBLIC
MeReal            MEAPI MdtCarWheelGetSuspensionKp(const MdtCarWheelID j);
MEPUBLIC
MeReal            MEAPI MdtCarWheelGetSuspensionKd(const MdtCarWheelID j);

#define                 MdtCarWheelGetBody(j, bodyindex) \
                            MdtConstraintGetBody(MdtCarWheelQuaConstraint(j), bodyindex)
#define                 MdtCarWheelGetUserData(j) \
                            MdtConstraintGetUserData(MdtCarWheelQuaConstraint(j))
#define                 MdtCarWheelGetWorld(j) \
                            MdtConstraintGetWorld(MdtCarWheelQuaConstraint(j))
#define                 MdtCarWheelGetForce(j, bodyindex, f) \
                            MdtConstraintGetForce(MdtCarWheelQuaConstraint(j), bodyindex, f)
#define                 MdtCarWheelGetTorque(j, bodyindex, t) \
                            MdtConstraintGetTorque(MdtCarWheelQuaConstraint(j), bodyindex, t)
/*
  Car wheel joint mutators.
*/

#define           MdtCarWheelSetPosition(j, x, y, z) \
                        MdtConstraintSetPosition(MdtCarWheelQuaConstraint(j), x, y, z)
#define           MdtCarWheelSetSteeringAndHingeAxes(j, px, py, pz, ox, oy, oz) \
                        MdtConstraintSetAxes(MdtCarWheelQuaConstraint(j), px, py, pz, ox, oy, oz)
MEPUBLIC
void              MEAPI MdtCarWheelSetSteeringLimitedForceMotor(
                            const MdtCarWheelID j, const MeReal desiredVelocity,
                            const MeReal forceLimit);
MEPUBLIC
void              MEAPI MdtCarWheelSetSteeringLock(const MdtCarWheelID j,
                            const MeBool lock);

MEPUBLIC
void              MEAPI MdtCarWheelSetHingeLimitedForceMotor(const MdtCarWheelID j,
                            const MeReal desiredVelocity, const MeReal forceLimit);

MEPUBLIC
void              MEAPI MdtCarWheelSetSuspension(const MdtCarWheelID j,
                            const MeReal Kp, const MeReal Kd,
                            const MeReal limit_softness, const  MeReal lolimit,
                            const MeReal hilimit, const MeReal reference);

#define                 MdtCarWheelSetBodies(j, b1, b2) \
                            MdtConstraintSetBodies(MdtCarWheelQuaConstraint(j), b1, b2)
#define                 MdtCarWheelSetUserData(j, d) \
                            MdtConstraintSetUserData(MdtCarWheelQuaConstraint(j), d)



#ifdef __cplusplus
}
#endif


#endif
