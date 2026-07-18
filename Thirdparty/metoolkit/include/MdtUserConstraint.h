#ifndef _MDTUSERCONSTRAINT_H
#define _MDTUSERCONSTRAINT_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.13.6.1 $

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
 * User defined constraint type.
 */

#include <MePrecision.h>
#include <MdtTypes.h>

#ifdef __cplusplus
extern "C"
{
#endif


MEPUBLIC
MdtUserConstraintID MEAPI MdtUserConstraintCreate(const MdtWorldID w);
MEPUBLIC
MdtConstraintID     MEAPI MdtUserConstraintQuaConstraint(const MdtUserConstraintID j);
MEPUBLIC
MdtUserConstraintID MEAPI MdtConstraintDCastUserConstraint(const MdtConstraintID c);

#define                 MdtUserConstraintDestroy(j) \
                            MdtConstraintDestroy(MdtUserConstraintQuaConstraint(j))
#define                 MdtUserConstraintEnable(j) \
                            MdtConstraintEnable(MdtUserConstraintQuaConstraint(j))
#define                 MdtUserConstraintDisable(j) \
                            MdtConstraintDisable(MdtUserConstraintQuaConstraint(j))
#define                 MdtUserConstraintIsEnabled(j) \
                            MdtConstraintIsEnabled(MdtUserConstraintQuaConstraint(j))
#define                 MdtUserConstraintSetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtUserConstraintQuaConstraint(j),k)
#define                 MdtUserConstraintGetSortKey(j) \
                            MdtConstraintGetSortKey(MdtUserConstraintQuaConstraint(j))

/*
  Universal joint accessors.
*/

MEPUBLIC
MdtBclAddConstraintFn MEAPI MdtUserConstraintGetFunction(const MdtUserConstraintID j);
MEPUBLIC
void             *MEAPI MdtUserConstraintGetConstraintData(const MdtUserConstraintID j);

#define                 MdtUserConstraintGetBody(j, bodyindex) \
                            MdtConstraintGetBody(MdtUserConstraintQuaConstraint(j), bodyindex)
#define                 MdtUserConstraintGetUserData(j) \
                            MdtConstraintGetUserData(MdtUserConstraintQuaConstraint(j))
#define                 MdtUserConstraintGetWorld(j) \
                            MdtConstraintGetWorld(MdtUserConstraintQuaConstraint(j))
#define                 MdtUserConstraintGetForce(j, bodyindex, f) \
                            MdtConstraintGetForce(MdtUserConstraintQuaConstraint(j), bodyindex, f)
#define                 MdtUserConstraintGetTorque(j, bodyindex, t) \
                            MdtConstraintGetTorque(MdtUserConstraintQuaConstraint(j), bodyindex, t)

/*
  Universal joint mutators.
*/

MEPUBLIC
void              MEAPI MdtUserConstraintSetFunction(const MdtUserConstraintID j, MdtBclAddConstraintFn f);
MEPUBLIC
void              MEAPI MdtUserConstraintSetConstraintData(const MdtUserConstraintID j, void* d);

#define                 MdtUserConstraintSetBodies(j, b1, b2) \
                            MdtConstraintSetBodies(MdtUserConstraintQuaConstraint(j), b1, b2)
#define                 MdtUserConstraintSetUserData(j, d) \
                            MdtConstraintSetUserData(MdtUserConstraintQuaConstraint(j), d)

#ifdef __cplusplus
}
#endif

#endif
