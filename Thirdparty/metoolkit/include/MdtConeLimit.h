#ifndef _MDTCONELIMIT_H
#define _MDTCONELIMIT_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.7.8.1 $

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
 * MdtConeLimit API functions.
 */

#include <MePrecision.h>
#include <MdtTypes.h>


#ifdef __cplusplus
extern "C"
{
#endif


/*
  Cone Limit joint functions.
*/

MEPUBLIC
MdtConeLimitID    MEAPI MdtConeLimitCreate(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtConeLimitReset(const MdtConeLimitID j);
MEPUBLIC
MdtConstraintID   MEAPI MdtConeLimitQuaConstraint(const MdtConeLimitID j);
MEPUBLIC
MdtConeLimitID    MEAPI MdtConstraintDCastConeLimit(const MdtConstraintID c);

#define           MdtConeLimitDestroy(j) \
                        MdtConstraintDestroy(MdtConeLimitQuaConstraint(j))
#define           MdtConeLimitEnable(j) \
                        MdtConstraintEnable(MdtConeLimitQuaConstraint(j))
#define           MdtConeLimitDisable(j) \
                        MdtConstraintDisable(MdtConeLimitQuaConstraint(j))
#define           MdtConeLimitIsEnabled(j) \
                        MdtConstraintIsEnabled(MdtConeLimitQuaConstraint(j))
#define           MdtConeLimitSetSortKey(j,k) \
                        MdtConstraintSetSortKey(MdtConeLimitQuaConstraint(j),k)
#define           MdtConeLimitGetSortKey(j) \
                        MdtConstraintGetSortKey(MdtConeLimitQuaConstraint(j))

/*
  Cone Limit accessors.
*/

#define           MdtConeLimitGetAxis(j, v) \
                        MdtConstraintGetAxis(MdtConeLimitQuaConstraint(j), (v))
#define           MdtConeLimitGetAxes(j, p, o) \
                        MdtConstraintGetAxes(MdtConeLimitQuaConstraint(j), (p), (o))
#define           MdtConeLimitBodyGetAxes(j, b, p, o) \
                        MdtConstraintBodyGetAxes(MdtConeLimitQuaConstraint(j), (b), (p), (o))

#define           MdtConeLimitGetBody(j, bodyindex) \
                        MdtConstraintGetBody(MdtConeLimitQuaConstraint(j), (bodyindex))
#define           MdtConeLimitGetUserData(j) \
                        MdtConstraintGetUserData(MdtConeLimitQuaConstraint(j))
#define           MdtConeLimitGetWorld(j) \
                        MdtConstraintGetWorld(MdtConeLimitQuaConstraint(j))
#define           MdtConeLimitGetForce(j, bodyindex, f) \
                        MdtConstraintGetForce(MdtConeLimitQuaConstraint(j), (bodyindex), (f))
#define           MdtConeLimitGetTorque(j, bodyindex, t) \
                        MdtConstraintGetTorque(MdtConeLimitQuaConstraint(j), (bodyindex), (t))

MEPUBLIC
MeReal    MEAPI MdtConeLimitGetConeHalfAngle(const MdtConeLimitID j);
MEPUBLIC
MeReal    MEAPI MdtConeLimitGetStiffness(const MdtConeLimitID j);
MEPUBLIC
MeReal    MEAPI MdtConeLimitGetDamping(const MdtConeLimitID j);

/*
  Cone Limit joint mutators.
*/

#define           MdtConeLimitSetBodies(j, b1, b2) \
                        MdtConstraintSetBodies(MdtConeLimitQuaConstraint(j), (b1), (b2))
#define           MdtConeLimitSetUserData(j, d) \
                        MdtConstraintSetUserData(MdtConeLimitQuaConstraint(j), (d))
#define           MdtConeLimitSetAxis(j, x, y, z) \
                        MdtConstraintSetAxis(MdtConeLimitQuaConstraint(j), (x), (y), (z))
#define           MdtConeLimitSetAxes(j, px, py, pz, ox, oy, oz) \
                        MdtConstraintSetAxes(MdtConeLimitQuaConstraint(j), (px), (py), (pz), (ox), (oy), (oz))
#define           MdtConeLimitBodySetAxes(j, b, px, py, pz, ox, oy, oz) \
                        MdtConstraintBodySetAxes(MdtConeLimitQuaConstraint(j), (b), (px), (py), (pz), (ox), (oy), (oz))
#define           MdtConeLimitBodySetAxesRel(j, b, px, py, pz, ox, oy, oz) \
                        MdtConstraintBodySetAxesRel(MdtConeLimitQuaConstraint(j), (b), (px), (py), (pz), (ox), (oy), (oz))

MEPUBLIC
void    MEAPI MdtConeLimitSetConeHalfAngle(const MdtConeLimitID j, const MeReal theta);
MEPUBLIC
void    MEAPI MdtConeLimitSetStiffness(const MdtConeLimitID j, const MeReal kp);
MEPUBLIC
void    MEAPI MdtConeLimitSetDamping(const MdtConeLimitID j, const MeReal kd);

#ifdef __cplusplus
}
#endif


#endif
