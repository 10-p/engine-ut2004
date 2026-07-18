#ifndef _MDTSKELETAL_H
#define _MDTSKELETAL_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.2.2.1 $

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
 * MdtSkeletal API functions.
 *
 */

#include <MePrecision.h>
#include <MdtTypes.h>

#ifdef __cplusplus
extern "C"
{
#endif


MEPUBLIC
MdtSkeletalID           MEAPI MdtSkeletalCreate(const MdtWorldID w);
MEPUBLIC
void                    MEAPI MdtSkeletalReset(const MdtSkeletalID j);
MEPUBLIC
MdtConstraintID         MEAPI MdtSkeletalQuaConstraint(const MdtSkeletalID j);
MEPUBLIC
MdtSkeletalID           MEAPI MdtConstraintDCastSkeletal(const MdtConstraintID c);

#define                 MdtSkeletalDestroy(j) \
                            MdtConstraintDestroy(MdtSkeletalQuaConstraint(j))
#define                 MdtSkeletalEnable(j) \
                            MdtConstraintEnable(MdtSkeletalQuaConstraint(j))
#define                 MdtSkeletalDisable(j) \
                            MdtConstraintDisable(MdtSkeletalQuaConstraint(j))
#define                 MdtSkeletalIsEnabled(j) \
                            MdtConstraintIsEnabled(MdtSkeletalQuaConstraint(j))
#define                 MdtSkeletalSetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtSkeletalQuaConstraint(j),k)
#define                 MdtSkeletalGetSortKey(j) \
                            MdtConstraintGetSortKey(MdtSkeletalQuaConstraint(j))

/*
  Skeletal joint accessors.
*/

MEPUBLIC
MdtSkeletalConeOption   MEAPI MdtSkeletalGetConeOption(const MdtSkeletalID j);
MEPUBLIC
MeReal                  MEAPI MdtSkeletalGetConePrimaryLimitAngle(const MdtSkeletalID j);
MEPUBLIC
MeReal                  MEAPI MdtSkeletalGetConeSecondaryLimitAngle(const MdtSkeletalID j);
MEPUBLIC
MeReal                  MEAPI MdtSkeletalGetConeStiffness(const MdtSkeletalID j);
MEPUBLIC
MeReal                  MEAPI MdtSkeletalGetConeDamping(const MdtSkeletalID j);


MEPUBLIC
MdtSkeletalTwistOption  MEAPI MdtSkeletalGetTwistOption(const MdtSkeletalID j);
MEPUBLIC
MeReal                  MEAPI MdtSkeletalGetTwistLimitAngle(const MdtSkeletalID j);
MEPUBLIC
MeReal                  MEAPI MdtSkeletalGetTwistStiffness(const MdtSkeletalID j);
MEPUBLIC
MeReal                  MEAPI MdtSkeletalGetTwistDamping(const MdtSkeletalID j);


#define                 MdtSkeletalGetPosition(j, p) \
                            MdtConstraintGetPosition(MdtSkeletalQuaConstraint(j), p)
#define                 MdtSkeletalBodyGetAxis(j, i, a) \
                            MdtConstraintBodyGetAxis(MdtSkeletalQuaConstraint(j), i, a)
#define                 MdtSkeletalBodyGetAxes(j, i, p, o) \
                            MdtConstraintBodyGetAxes(MdtSkeletalQuaConstraint(j), i, p, o)

#define                 MdtSkeletalGetBody(j, bodyindex) \
                            MdtConstraintGetBody(MdtSkeletalQuaConstraint(j), bodyindex)
#define                 MdtSkeletalGetUserData(j) \
                            MdtConstraintGetUserData(MdtSkeletalQuaConstraint(j))
#define                 MdtSkeletalGetWorld(j) \
                            MdtConstraintGetWorld(MdtSkeletalQuaConstraint(j))
#define                 MdtSkeletalGetForce(j, bodyindex, f) \
                            MdtConstraintGetForce(MdtSkeletalQuaConstraint(j), bodyindex, f)
#define                 MdtSkeletalGetTorque(j, bodyindex, t) \
                            MdtConstraintGetTorque(MdtSkeletalQuaConstraint(j), bodyindex, t)

/*
  Skeletal joint mutators.
*/



MEPUBLIC
void                    MEAPI MdtSkeletalZeroTwist(const MdtSkeletalID j);

MEPUBLIC
void                    MEAPI MdtSkeletalSetConeOption(const MdtSkeletalID j, MdtSkeletalConeOption co);
MEPUBLIC
void                    MEAPI MdtSkeletalSetConePrimaryLimitAngle(const MdtSkeletalID j, const MeReal theta);
MEPUBLIC
void                    MEAPI MdtSkeletalSetConeSecondaryLimitAngle(const MdtSkeletalID j, const MeReal theta);
MEPUBLIC
void                    MEAPI MdtSkeletalSetConeStiffness(const MdtSkeletalID j, const MeReal kp);
MEPUBLIC
void                    MEAPI MdtSkeletalSetConeDamping(const MdtSkeletalID j, const MeReal kd);

MEPUBLIC
void                    MEAPI MdtSkeletalSetTwistOption(const MdtSkeletalID j, MdtSkeletalTwistOption to);
MEPUBLIC
void                    MEAPI MdtSkeletalSetTwistLimitAngle(const MdtSkeletalID j, const MeReal theta);
MEPUBLIC
void                    MEAPI MdtSkeletalSetTwistStiffness(const MdtSkeletalID j, const MeReal kp);
MEPUBLIC
void                    MEAPI MdtSkeletalSetTwistDamping(const MdtSkeletalID j, const MeReal kd);

#define                 MdtSkeletalSetPosition(j, x, y, z) \
                            MdtConstraintSetPosition(MdtSkeletalQuaConstraint(j), x, y, z)
#define                 MdtSkeletalSetAxes(j, px, py, pz, ox, oy, oz) \
                            MdtConstraintSetAxes(MdtSkeletalQuaConstraint(j), px, py, pz, ox, oy, oz)
#define                 MdtSkeletalBodySetAxes(j, i, px, py, pz, ox, oy, oz) \
                            MdtConstraintBodySetAxes(MdtSkeletalQuaConstraint(j), i, px, py, pz, ox, oy, oz)

#define                 MdtSkeletalSetBodies(j, b1, b2) \
                            MdtConstraintSetBodies(MdtSkeletalQuaConstraint(j), b1, b2)
#define                 MdtSkeletalSetUserData(j, d) \
                            MdtConstraintSetUserData(MdtSkeletalQuaConstraint(j), d)


#ifdef __cplusplus
}
#endif


#endif
