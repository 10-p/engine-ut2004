#ifndef _MDTLIMIT_H
#define _MDTLIMIT_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.16.6.1 $

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
 * MdtLimit and MdtSingleLimit API functions.
 */

#include <MePrecision.h>
#include <MdtTypes.h>


#ifdef __cplusplus
extern "C"
{
#endif



/*
  Individual limit initialisation:
*/

/*
 * This implements hard and soft limits for the remaining degrees of
 * freedom of constraints.
 *
 * All angles are specified in radians.
 */

MEPUBLIC
void              MEAPI MdtSingleLimitReset(const MdtSingleLimitID limit);

/*
  Individual limit accessors:
*/

MEPUBLIC
MeReal            MEAPI MdtSingleLimitGetStop(
                            const MdtSingleLimitID sl);
MEPUBLIC
MeReal            MEAPI MdtSingleLimitGetStiffness(
                            const MdtSingleLimitID sl);
MEPUBLIC
MeReal            MEAPI MdtSingleLimitGetDamping(
                            const MdtSingleLimitID sl);
MEPUBLIC
MeReal            MEAPI MdtSingleLimitGetRestitution(
                            const MdtSingleLimitID sl);
/*
  Individual limit mutators:
*/

MEPUBLIC
void              MEAPI MdtSingleLimitSetStop(const MdtSingleLimitID sl,
                            const MeReal NewStop);
MEPUBLIC
void              MEAPI MdtSingleLimitSetStiffness(const MdtSingleLimitID sl,
                            const MeReal NewStiffness);
MEPUBLIC
void              MEAPI MdtSingleLimitSetDamping(const MdtSingleLimitID sl,
                            const MeReal NewDamping);
MEPUBLIC
void              MEAPI MdtSingleLimitSetRestitution(const MdtSingleLimitID sl,
                            const MeReal NewRestitution);

/*
  Joint limit functions:
*/

MEPUBLIC
void              MEAPI MdtLimitReset(const MdtLimitID limit);

MEPUBLIC
void              MEAPI MdtLimitResetState(const MdtLimitID  limit);

/*
  Joint limit accessors:
*/

MEPUBLIC
MeBool            MEAPI MdtLimitIsActive(const MdtLimitID l);
MEPUBLIC
MeBool            MEAPI MdtLimitPositionIsCalculated(const MdtLimitID l);
MEPUBLIC
MdtSingleLimitID  MEAPI MdtLimitGetLowerLimit(const MdtLimitID l);
MEPUBLIC
MdtSingleLimitID  MEAPI MdtLimitGetUpperLimit(const MdtLimitID l);
MEPUBLIC
MeReal            MEAPI MdtLimitGetPosition(const MdtLimitID l);
MEPUBLIC
MeReal            MEAPI MdtLimitGetOvershoot(const MdtLimitID l);
MEPUBLIC
MeReal            MEAPI MdtLimitGetVelocity(const MdtLimitID l);
MEPUBLIC
MeReal            MEAPI MdtLimitGetStiffnessThreshold(
                            const MdtLimitID l);
MEPUBLIC
MeBool            MEAPI MdtLimitIsMotorized(const MdtLimitID l);
MEPUBLIC
MeBool            MEAPI MdtLimitIsLocked(const MdtLimitID l);
MEPUBLIC
MeReal            MEAPI MdtLimitGetMotorDesiredVelocity(
                            const MdtLimitID l);
MEPUBLIC
MeReal            MEAPI MdtLimitGetMotorMaxForce(
                            const MdtLimitID l);

/*
  Joint limit mutators:
*/

MEPUBLIC
void              MEAPI MdtLimitSetLowerLimit(const MdtLimitID l,
                            const MdtSingleLimitID sl);
MEPUBLIC
void              MEAPI MdtLimitSetUpperLimit(const MdtLimitID l,
                            const MdtSingleLimitID sl);
MEPUBLIC
void              MEAPI MdtLimitSetPosition( MdtLimitID l,
                            const MeReal NewPosition );
MEPUBLIC
void              MEAPI MdtLimitActivateLimits(const MdtLimitID l,
                            const MeBool NewActivationState);
MEPUBLIC
void              MEAPI MdtLimitCalculatePosition(const MdtLimitID l,
                            const MeBool NewState);
MEPUBLIC
void              MEAPI MdtLimitSetStiffnessThreshold(const MdtLimitID l,
                            const MeReal NewStiffnessThreshold);
MEPUBLIC
void              MEAPI MdtLimitActivateMotor(const MdtLimitID l,
                            const MeBool NewActivationState);
MEPUBLIC
void              MEAPI MdtLimitSetLimitedForceMotor(const MdtLimitID l,
                            const MeReal desiredVelocity,
                            const MeReal forceLimit);
MEPUBLIC
void              MEAPI MdtLimitActivateLock(const MdtLimitID l,
                            const MeBool NewActivationState);
MEPUBLIC
void              MEAPI MdtLimitSetLock(const MdtLimitID l,
                            const MeReal lock_value,
                            const MeReal forceLimit);

#ifdef __cplusplus
}
#endif


#endif
