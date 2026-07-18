#ifndef _MDTUNIVERSAL_H
#define _MDTUNIVERSAL_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.19.2.1 $

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
 * MdtUniversal API functions.
 *
 * This joint is essentially a pair of hinges with their axes joined at an
 * angle (usually 90 degrees).
 */

#include <MePrecision.h>
#include <MdtTypes.h>

#ifdef __cplusplus
extern "C"
{
#endif


MEPUBLIC
MdtUniversalID    MEAPI MdtUniversalCreate(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtUniversalReset(const MdtUniversalID j);
MEPUBLIC
MdtConstraintID   MEAPI MdtUniversalQuaConstraint(const MdtUniversalID j);
MEPUBLIC
MdtUniversalID    MEAPI MdtConstraintDCastUniversal(const MdtConstraintID c);
#define                 MdtUniversalDestroy(j) \
                            MdtConstraintDestroy(MdtUniversalQuaConstraint(j))
#define                 MdtUniversalEnable(j) \
                            MdtConstraintEnable(MdtUniversalQuaConstraint(j))
#define                 MdtUniversalDisable(j) \
                            MdtConstraintDisable(MdtUniversalQuaConstraint(j))
#define                 MdtUniversalIsEnabled(j) \
                            MdtConstraintIsEnabled(MdtUniversalQuaConstraint(j))
#define                 MdtUniversalSetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtUniversalQuaConstraint(j),k)
#define                 MdtUniversalGetSortKey(j) \
                            MdtConstraintGetSortKey(MdtUniversalQuaConstraint(j))

/*
  Universal joint accessors.
*/


#define                 MdtUniversalGetPosition(j, p) \
                            MdtConstraintGetPosition(MdtUniversalQuaConstraint(j), p)
#define                 MdtUniversalGetAxes(j, bodyindex, p, o) \
                            MdtConstraintBodyGetAxes(MdtUniversalQuaConstraint(j), bodyindex, p, o)
#define                 MdtUniversalGetBody(j, bodyindex) \
                            MdtConstraintGetBody(MdtUniversalQuaConstraint(j), bodyindex)
#define                 MdtUniversalGetUserData(j) \
                            MdtConstraintGetUserData(MdtUniversalQuaConstraint(j))
#define                 MdtUniversalGetWorld(j) \
                            MdtConstraintGetWorld(MdtUniversalQuaConstraint(j))
#define                 MdtUniversalGetForce(j, bodyindex, f) \
                            MdtConstraintGetForce(MdtUniversalQuaConstraint(j), bodyindex, f)
#define                 MdtUniversalGetTorque(j, bodyindex, t) \
                            MdtConstraintGetTorque(MdtUniversalQuaConstraint(j), bodyindex, t)

/*
  Universal joint mutators.
*/

#define                 MdtUniversalSetPosition(j, x, y, z) \
                            MdtConstraintSetPosition(MdtUniversalQuaConstraint(j), x, y, z)
#define                 MdtUniversalSetAxes(j, px, py, pz, ox, oy, oz) \
                            MdtConstraintSetAxes(MdtUniversalQuaConstraint(j), px, py, pz, ox, oy, oz)
#define                 MdtUniversalSetBodies(j, b1, b2) \
                            MdtConstraintSetBodies(MdtUniversalQuaConstraint(j), b1, b2)
#define                 MdtUniversalSetUserData(j, d) \
                            MdtConstraintSetUserData(MdtUniversalQuaConstraint(j), d)


#ifdef __cplusplus
}
#endif


#endif
