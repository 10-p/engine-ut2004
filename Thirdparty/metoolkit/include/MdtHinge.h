#ifndef _MDTHINGE_H
#define _MDTHINGE_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.20.6.1 $

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
 * MdtHinge API functions.
 */

#include <MePrecision.h>
#include <MdtTypes.h>

#ifdef __cplusplus
extern "C"
{
#endif



MEPUBLIC
MdtHingeID        MEAPI MdtHingeCreate(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtHingeReset(const MdtHingeID j);
MEPUBLIC
MdtConstraintID   MEAPI MdtHingeQuaConstraint(const MdtHingeID j);
MEPUBLIC
MdtHingeID        MEAPI MdtConstraintDCastHinge(const MdtConstraintID c);

#define                 MdtHingeDestroy(j) \
                            MdtConstraintDestroy(MdtHingeQuaConstraint(j))
#define                 MdtHingeEnable(j) \
                            MdtConstraintEnable(MdtHingeQuaConstraint(j))
#define                 MdtHingeDisable(j) \
                            MdtConstraintDisable(MdtHingeQuaConstraint(j))
#define                 MdtHingeIsEnabled(j) \
                            MdtConstraintIsEnabled(MdtHingeQuaConstraint(j))
#define                 MdtHingeSetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtHingeQuaConstraint(j),k)
#define                 MdtHingeGetSortKey(j) \
                            MdtConstraintGetSortKey(MdtHingeQuaConstraint(j))
/*
  Hinge joint accessors.
*/

MEPUBLIC
MdtLimitID        MEAPI MdtHingeGetLimit(const MdtHingeID j);

#define                 MdtHingeGetPosition(j, v) \
                            MdtConstraintGetPosition(MdtHingeQuaConstraint(j), v)
#define                 MdtHingeGetAxis(j, v) \
                            MdtConstraintGetAxis(MdtHingeQuaConstraint(j), v)
#define                 MdtHingeGetBody(j, bodyindex) \
                            MdtConstraintGetBody(MdtHingeQuaConstraint(j), bodyindex)
#define                 MdtHingeGetUserData(j) \
                            MdtConstraintGetUserData(MdtHingeQuaConstraint(j))
#define                 MdtHingeGetWorld(j) \
                            MdtConstraintGetWorld(MdtHingeQuaConstraint(j))
#define                 MdtHingeGetForce(j, bodyindex, f) \
                            MdtConstraintGetForce(MdtHingeQuaConstraint(j), bodyindex, f)
#define                 MdtHingeGetTorque(j, bodyindex, t) \
                            MdtConstraintGetTorque(MdtHingeQuaConstraint(j), bodyindex, t)
/*
  Hinge joint mutators.
*/

MEPUBLIC
void              MEAPI MdtHingeSetLimit(const MdtHingeID j,
                            const MdtLimitID NewLimit);

#define                 MdtHingeSetPosition(j, x, y, z) \
                            MdtConstraintSetPosition(MdtHingeQuaConstraint(j), x, y, z)
#define                 MdtHingeSetAxis(j, x, y, z) \
                            MdtConstraintSetAxis(MdtHingeQuaConstraint(j), x, y, z)
#define                 MdtHingeSetBodies(j, b1, b2) \
                            MdtConstraintSetBodies(MdtHingeQuaConstraint(j), b1, b2)
#define                 MdtHingeSetUserData(j, d) \
                            MdtConstraintSetUserData(MdtHingeQuaConstraint(j), d)


#ifdef __cplusplus
}
#endif


#endif
