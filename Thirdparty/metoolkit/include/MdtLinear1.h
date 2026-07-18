#ifndef _MDTLINEAR1_H
#define _MDTLINEAR1_H
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
 * MdtLinear1 API functions.
 *
 * The primary body is constrained to lie in a plane fixed in the
 * reference frame of the second body (or the inertial frame).  The
 * plane normal is the vector from the secondary body to the joint
 * position in the reference frame of the secondary body.  If there is
 * no secondary body, the plane normal is the vector from the origin to
 * the joint position.
 */

#include <MePrecision.h>
#include <MdtTypes.h>

#ifdef __cplusplus
extern "C"
{
#endif


MEPUBLIC
MdtLinear1ID      MEAPI MdtLinear1Create(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtLinear1Reset(const MdtLinear1ID j);
MEPUBLIC
MdtConstraintID   MEAPI MdtLinear1QuaConstraint(const MdtLinear1ID j);
MEPUBLIC
MdtLinear1ID      MEAPI MdtConstraintDCastLinear1(const MdtConstraintID c);

#define                 MdtLinear1Destroy(j) \
                            MdtConstraintDestroy(MdtLinear1QuaConstraint(j))
#define                 MdtLinear1Enable(j) \
                            MdtConstraintEnable(MdtLinear1QuaConstraint(j))
#define                 MdtLinear1Disable(j) \
                            MdtConstraintDisable(MdtLinear1QuaConstraint(j))
#define                 MdtLinear1IsEnabled(j) \
                            MdtConstraintIsEnabled(MdtLinear1QuaConstraint(j))
#define                 MdtLinear1SetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtLinear1QuaConstraint(j),k)
#define                 MdtLinear1GetSortKey(j) \
                            MdtConstraintGetSortKey(MdtLinear1QuaConstraint(j))
/*
  Linear1 joint accessors.
*/

MEPUBLIC
void              MEAPI MdtLinear1GetPosition(const MdtLinear1ID j,
                            MeVector3 position);

#define                 MdtLinear1GetBody(j, bodyindex) \
                            MdtConstraintGetBody(MdtLinear1QuaConstraint(j), bodyindex)
#define                 MdtLinear1GetUserData(j) \
                            MdtConstraintGetUserData(MdtLinear1QuaConstraint(j))
#define                 MdtLinear1GetWorld(j) \
                            MdtConstraintGetWorld(MdtLinear1QuaConstraint(j))
#define                 MdtLinear1GetForce(j, bodyindex, f) \
                            MdtConstraintGetForce(MdtLinear1QuaConstraint(j), bodyindex, f)
#define                 MdtLinear1GetTorque(j, bodyindex, t) \
                            MdtConstraintGetTorque(MdtLinear1QuaConstraint(j), bodyindex, t)

/*
  Linear1 joint mutators.
*/

/*
  Note that the joint position w.r.t. body b2 defines the plane normal for
  this joint. The joint position is therefore not allowed to be coincident
  with b2 (or the origin of the inertial frame if there is no b2).
*/

MEPUBLIC
void              MEAPI MdtLinear1SetPosition(const MdtLinear1ID j,
                            const MeReal x, const MeReal y, const MeReal z);

#define                 MdtLinear1SetBodies(j, b1, b2) \
                            MdtConstraintSetBodies(MdtLinear1QuaConstraint(j), b1, b2)
#define                 MdtLinear1SetUserData(j, d) \
                            MdtConstraintSetUserData(MdtLinear1QuaConstraint(j), d)


#ifdef __cplusplus
}
#endif


#endif
