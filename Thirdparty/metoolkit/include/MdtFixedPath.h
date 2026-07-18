#ifndef _MDTFIXEDPATH_H
#define _MDTFIXEDPATH_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.18.6.2 $

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
 * MdtFixedPath API functions.
 *
 * This joint constrains point in two bodys to coincide; like the ball
 * and socket except that the points can follow defined trajectories.
 * A full-constraint version is implemented in MdtRPROJoint.cpp.
 * Unlike most other constraints, the user is expected to set the joint
 * position in the constrained bodies' reference frames.
 * The velocity is also set in the bodies' reference frames.
 */

#include <MePrecision.h>
#include <MdtTypes.h>

#ifdef __cplusplus
extern "C"
{
#endif




MEPUBLIC
MdtFixedPathID    MEAPI MdtFixedPathCreate(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtFixedPathReset(const MdtFixedPathID j);
MEPUBLIC
MdtConstraintID   MEAPI MdtFixedPathQuaConstraint(const MdtFixedPathID j);
MEPUBLIC
MdtFixedPathID    MEAPI MdtConstraintDCastFixedPath(const MdtConstraintID c);

#define                 MdtFixedPathDestroy(j) \
                            MdtConstraintDestroy(MdtFixedPathQuaConstraint(j))
#define                 MdtFixedPathEnable(j) \
                            MdtConstraintEnable(MdtFixedPathQuaConstraint(j))
#define                 MdtFixedPathDisable(j) \
                            MdtConstraintDisable(MdtFixedPathQuaConstraint(j))
#define                 MdtFixedPathIsEnabled(j) \
                            MdtConstraintIsEnabled(MdtFixedPathQuaConstraint(j))
#define                 MdtFixedPathSetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtFixedPathQuaConstraint(j),k)
#define                 MdtFixedPathGetSortKey(j) \
                            MdtConstraintGetSortKey(MdtFixedPathQuaConstraint(j))

/*
  Fixed path joint accessors.
*/

MEPUBLIC
void              MEAPI MdtFixedPathGetPosition(const MdtFixedPathID j,
                            const unsigned int bodyindex, MeVector3 position);
MEPUBLIC
void              MEAPI MdtFixedPathGetVelocity(const MdtFixedPathID j,
                            const unsigned int bodyindex, MeVector3 velocity);

#define                 MdtFixedPathGetBody(j, bodyindex) \
                            MdtConstraintGetBody(MdtFixedPathQuaConstraint(j), bodyindex)
#define                 MdtFixedPathGetUserData(j) \
                            MdtConstraintGetUserData(MdtFixedPathQuaConstraint(j))
#define                 MdtFixedPathGetWorld(j) \
                            MdtConstraintGetWorld(MdtFixedPathQuaConstraint(j))
#define                 MdtFixedPathGetForce(j, bodyindex, f) \
                            MdtConstraintGetForce(MdtFixedPathQuaConstraint(j), bodyindex, f)
#define                 MdtFixedPathGetTorque(j, bodyindex, t) \
                            MdtConstraintGetTorque(MdtFixedPathQuaConstraint(j), bodyindex, t)
/*
  Fixed path joint mutators.
*/

MEPUBLIC
void              MEAPI MdtFixedPathSetPosition(const MdtFixedPathID j,const unsigned int bodyindex,
                            const MeReal x, const MeReal y, const MeReal z);
MEPUBLIC
void              MEAPI MdtFixedPathSetVelocity(const MdtFixedPathID j, const unsigned int bodyindex,
                            const MeReal dx, const MeReal dy, const MeReal dz);

#define                 MdtFixedPathSetBodies(j, b1, b2) \
                            MdtConstraintSetBodies(MdtFixedPathQuaConstraint(j), b1, b2)
#define                 MdtFixedPathSetUserData(j, d) \
                            MdtConstraintSetUserData(MdtFixedPathQuaConstraint(j), d)



#ifdef __cplusplus
}
#endif


#endif
