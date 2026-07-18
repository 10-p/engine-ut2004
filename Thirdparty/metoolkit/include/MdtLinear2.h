#ifndef _MDTLINEAR2_H
#define _MDTLINEAR2_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.18.6.1 $

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
 * MdtLinear2 API functions.
 *
 * The joint is constrained to lie on a line fixed by its initial position
 * and a direction vector fixed in the reference frame of the secondary
 * body, or fixed in the inertial frame if there is no secondary body.
 */

#include <MePrecision.h>
#include <MdtTypes.h>


#ifdef __cplusplus
extern "C"
{
#endif


MEPUBLIC
MdtLinear2ID      MEAPI MdtLinear2Create(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtLinear2Reset(const MdtLinear2ID j);
MEPUBLIC
MdtConstraintID   MEAPI MdtLinear2QuaConstraint(const MdtLinear2ID j);
MEPUBLIC
MdtLinear2ID      MEAPI MdtConstraintDCastLinear2(const MdtConstraintID c);

#define                 MdtLinear2Destroy(j) \
                            MdtConstraintDestroy(MdtLinear2QuaConstraint(j))
#define                 MdtLinear2Enable(j) \
                            MdtConstraintEnable(MdtLinear2QuaConstraint(j))
#define                 MdtLinear2Disable(j) \
                            MdtConstraintDisable(MdtLinear2QuaConstraint(j))
#define                 MdtLinear2IsEnabled(j) \
                            MdtConstraintIsEnabled(MdtLinear2QuaConstraint(j))
#define                 MdtLinear2SetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtLinear2QuaConstraint(j),k)
#define                 MdtLinear2GetSortKey(j) \
                            MdtConstraintGetSortKey(MdtLinear2QuaConstraint(j))

/*
  Linear2 joint accessors.
*/

MEPUBLIC
void              MEAPI MdtLinear2GetPosition(const MdtLinear2ID j,
                            MeVector3 position);
MEPUBLIC
void              MEAPI MdtLinear2GetDirection(const MdtLinear2ID j,
                            MeVector3 direction);

#define                 MdtLinear2GetBody(j, bodyindex) \
                            MdtConstraintGetBody(MdtLinear2QuaConstraint(j), bodyindex)
#define                 MdtLinear2GetUserData(j) \
                            MdtConstraintGetUserData(MdtLinear2QuaConstraint(j))
#define                 MdtLinear2GetWorld(j) \
                            MdtConstraintGetWorld(MdtLinear2QuaConstraint(j))
#define                 MdtLinear2GetForce(j, bodyindex, f) \
                            MdtConstraintGetForce(MdtLinear2QuaConstraint(j), bodyindex, f)
#define                 MdtLinear2GetTorque(j, bodyindex, t) \
                            MdtConstraintGetTorque(MdtLinear2QuaConstraint(j), bodyindex, t)
/*
  Linear2 joint mutators
*/

MEPUBLIC
void              MEAPI MdtLinear2SetPosition(const MdtLinear2ID j,
                            const MeReal x, const MeReal y, const MeReal z);
MEPUBLIC
void              MEAPI MdtLinear2SetDirection(const MdtLinear2ID j,
                            const MeReal x, const MeReal y, const MeReal z);

#define                 MdtLinear2SetBodies(j, b1, b2) \
                            MdtConstraintSetBodies(MdtLinear2QuaConstraint(j), b1, b2)
#define                 MdtLinear2SetUserData(j, d) \
                            MdtConstraintSetUserData(MdtLinear2QuaConstraint(j), d)


#ifdef __cplusplus
}
#endif


#endif
