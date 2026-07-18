#ifndef _MDTBSJOINT_H
#define _MDTBSJOINT_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.22.2.1 $

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
 * MdtBSJoint API functions.
 */

#include <MePrecision.h>
#include <MdtTypes.h>


#ifdef __cplusplus
extern "C"
{
#endif


/*
  Ball and socket joint functions.
*/

MEPUBLIC
MdtBSJointID      MEAPI MdtBSJointCreate(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtBSJointReset(const MdtBSJointID j);
MEPUBLIC
MdtConstraintID   MEAPI MdtBSJointQuaConstraint(const MdtBSJointID j);
MEPUBLIC
MdtBSJointID      MEAPI MdtConstraintDCastBSJoint(const MdtConstraintID c);

#define                 MdtBSJointDestroy(j) \
                            MdtConstraintDestroy(MdtBSJointQuaConstraint(j))
#define                 MdtBSJointEnable(j) \
                            MdtConstraintEnable(MdtBSJointQuaConstraint(j))
#define                 MdtBSJointDisable(j) \
                            MdtConstraintDisable(MdtBSJointQuaConstraint(j))
#define                 MdtBSJointIsEnabled(j) \
                            MdtConstraintIsEnabled(MdtBSJointQuaConstraint(j))
#define                 MdtBSJointSetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtBSJointQuaConstraint(j),k)
#define                 MdtBSJointGetSortKey(j) \
                            MdtConstraintGetSortKey(MdtBSJointQuaConstraint(j))

/*
  Ball and socket joint accessors.
*/


#define                 MdtBSJointGetPosition(j, p) \
                            MdtConstraintGetPosition(MdtBSJointQuaConstraint(j), p)
#define                 MdtBSJointGetBody(j, bodyindex) \
                            MdtConstraintGetBody(MdtBSJointQuaConstraint(j), bodyindex)
#define                 MdtBSJointGetUserData(j) \
                            MdtConstraintGetUserData(MdtBSJointQuaConstraint(j))
#define                 MdtBSJointGetWorld(j) \
                            MdtConstraintGetWorld(MdtBSJointQuaConstraint(j))
#define                 MdtBSJointGetForce(j, bodyindex, f) \
                            MdtConstraintGetForce(MdtBSJointQuaConstraint(j), bodyindex, f)
#define                 MdtBSJointGetTorque(j, bodyindex, t) \
                            MdtConstraintGetTorque(MdtBSJointQuaConstraint(j), bodyindex, t)

/*
  Ball and socket joint mutators.
*/


#define                 MdtBSJointSetPosition(j, x, y, z) \
                            MdtConstraintSetPosition(MdtBSJointQuaConstraint(j), x, y, z)
#define                 MdtBSJointSetBodies(j, b1, b2) \
                            MdtConstraintSetBodies(MdtBSJointQuaConstraint(j), b1, b2)
#define                 MdtBSJointSetUserData(j, d) \
                            MdtConstraintSetUserData(MdtBSJointQuaConstraint(j), d)


#ifdef __cplusplus
}
#endif


#endif
