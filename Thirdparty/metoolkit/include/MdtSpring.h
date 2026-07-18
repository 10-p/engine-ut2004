#ifndef _MDTSPRING_H
#define _MDTSPRING_H
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
 * MdtSpring API functions.
 *
 * This joint attaches one body to another, or to the inertial
 * reference frame, at a given separation.  This separation
 * is governed by two limits which may both be "hard" (which
 * simulates a rod or strut joint) or both soft (simulating a
 * spring) or hard on one limit but soft on the other (e.g. an
 * elastic attachment which may be stretched but not compressed).
 * The default behaviour is spring-like, with two soft, damped
 * limits, both initialised at the initial separation of the
 * bodies.
 */

#include <MePrecision.h>
#include <MdtTypes.h>


#ifdef __cplusplus
extern "C"
{
#endif


MEPUBLIC
MdtSpringID       MEAPI MdtSpringCreate(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtSpringReset(const MdtSpringID j);
MEPUBLIC
MdtConstraintID   MEAPI MdtSpringQuaConstraint(const MdtSpringID j);
MEPUBLIC
MdtSpringID       MEAPI MdtConstraintDCastSpring(const MdtConstraintID c);

#define                 MdtSpringDestroy(j) \
                            MdtConstraintDestroy(MdtSpringQuaConstraint(j))
#define                 MdtSpringEnable(j) \
                            MdtConstraintEnable(MdtSpringQuaConstraint(j))
#define                 MdtSpringDisable(j) \
                            MdtConstraintDisable(MdtSpringQuaConstraint(j))
#define                 MdtSpringIsEnabled(j) \
                            MdtConstraintIsEnabled(MdtSpringQuaConstraint(j))
#define                 MdtSpringSetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtSpringQuaConstraint(j),k)
#define                 MdtSpringGetSortKey(j) \
                            MdtConstraintGetSortKey(MdtSpringQuaConstraint(j))

/*
  Spring joint accessors.
*/
MEPUBLIC
void              MEAPI MdtSpringGetPosition(const MdtSpringID j, MeVector3 v,
                                             const unsigned int bodyindex);
MEPUBLIC
MdtLimitID        MEAPI MdtSpringGetLimit(const MdtSpringID j);

#define                 MdtSpringGetBody(j, bodyindex) \
                            MdtConstraintGetBody(MdtSpringQuaConstraint(j), bodyindex)
#define                 MdtSpringGetUserData(j) \
                            MdtConstraintGetUserData(MdtSpringQuaConstraint(j))
#define                 MdtSpringGetWorld(j) \
                            MdtConstraintGetWorld(MdtSpringQuaConstraint(j))
#define                 MdtSpringGetForce(j, bodyindex, f) \
                            MdtConstraintGetForce(MdtSpringQuaConstraint(j), bodyindex, f)
#define                 MdtSpringGetTorque(j, bodyindex, t) \
                            MdtConstraintGetTorque(MdtSpringQuaConstraint(j), bodyindex, t)

/*
  Spring joint mutators
*/
MEPUBLIC
void              MEAPI MdtSpringSetPosition(const MdtSpringID j,
                            const unsigned int bodyindex,
                            const MeReal x, const MeReal y, const MeReal z );
MEPUBLIC
void              MEAPI MdtSpringSetLimit(const MdtSpringID j,
                            const MdtLimitID NewLimit);
/*
  Spring "ease-of-use" mutators, which set the given property in both the upper
  and lower limit.
*/
MEPUBLIC
void              MEAPI MdtSpringSetNaturalLength(const MdtSpringID j,
                                                  const MeReal NewNaturalLength);
MEPUBLIC
void              MEAPI MdtSpringSetStiffness(const MdtSpringID j,
                                                  const MeReal NewStiffness);
MEPUBLIC
void              MEAPI MdtSpringSetDamping(const MdtSpringID j,
                                                  const MeReal NewDamping);

#define                 MdtSpringSetBodies(j, b1, b2) \
                            MdtConstraintSetBodies(MdtSpringQuaConstraint(j), b1, b2)
#define                 MdtSpringSetUserData(j, d) \
                            MdtConstraintSetUserData(MdtSpringQuaConstraint(j), d)



#ifdef __cplusplus
}
#endif


#endif
