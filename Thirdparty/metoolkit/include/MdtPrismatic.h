#ifndef _MDTPRISMATIC_H
#define _MDTPRISMATIC_H
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
 *  MdtPrismatic API functions.
 */

#include <MePrecision.h>
#include <MdtTypes.h>

#ifdef __cplusplus
extern "C"
{
#endif


MEPUBLIC
MdtPrismaticID    MEAPI MdtPrismaticCreate(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtPrismaticReset(const MdtPrismaticID j);
MEPUBLIC
MdtConstraintID   MEAPI MdtPrismaticQuaConstraint(const MdtPrismaticID j);
MEPUBLIC
MdtPrismaticID    MEAPI MdtConstraintDCastPrismatic(const MdtConstraintID c);

#define                 MdtPrismaticDestroy(j) \
                            MdtConstraintDestroy(MdtPrismaticQuaConstraint(j))
#define                 MdtPrismaticEnable(j) \
                            MdtConstraintEnable(MdtPrismaticQuaConstraint(j))
#define                 MdtPrismaticDisable(j) \
                            MdtConstraintDisable(MdtPrismaticQuaConstraint(j))
#define                 MdtPrismaticIsEnabled(j) \
                            MdtConstraintIsEnabled(MdtPrismaticQuaConstraint(j))
#define                 MdtPrismaticSetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtPrismaticQuaConstraint(j),k)
#define                 MdtPrismaticGetSortKey(j) \
                            MdtConstraintGetSortKey(MdtPrismaticQuaConstraint(j))


/*
  Prismatic joint accessors.
*/

MEPUBLIC
MdtLimitID        MEAPI MdtPrismaticGetLimit(const MdtPrismaticID j);

#define                 MdtPrismaticGetAxis(j, v) \
                            MdtConstraintGetAxis(MdtPrismaticQuaConstraint(j), v)
#define                 MdtPrismaticGetBody(j, bodyindex) \
                            MdtConstraintGetBody(MdtPrismaticQuaConstraint(j), bodyindex)
#define                 MdtPrismaticGetUserData(j) \
                            MdtConstraintGetUserData(MdtPrismaticQuaConstraint(j))
#define                 MdtPrismaticGetWorld(j) \
                            MdtConstraintGetWorld(MdtPrismaticQuaConstraint(j))
#define                 MdtPrismaticGetForce(j, bodyindex, f) \
                            MdtConstraintGetForce(MdtPrismaticQuaConstraint(j), bodyindex, f)
#define                 MdtPrismaticGetTorque(j, bodyindex, t) \
                            MdtConstraintGetTorque(MdtPrismaticQuaConstraint(j), bodyindex, t)

/*
  Prismatic joint mutators.
*/

MEPUBLIC
void              MEAPI MdtPrismaticSetLimit(const MdtPrismaticID j,
                            const MdtLimitID NewLimit);

#define                 MdtPrismaticSetAxis(j, x, y, z) \
                            MdtConstraintSetAxis(MdtPrismaticQuaConstraint(j), x, y, z);
#define                 MdtPrismaticSetBodies(j, b1, b2) \
                            MdtConstraintSetBodies(MdtPrismaticQuaConstraint(j), b1, b2)
#define                 MdtPrismaticSetUserData(j, d) \
                            MdtConstraintSetUserData(MdtPrismaticQuaConstraint(j), d)


#ifdef __cplusplus
}
#endif


#endif
