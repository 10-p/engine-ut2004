#ifndef _MDTRPROJOINT_H
#define _MDTRPROJOINT_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.15.6.2 $

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
 * MdtRPROJoint API functions.
 *
 * This joint maintains a user defined orientation and position between two
 * rigid bodies.   The user specifies both the location and the orientation
 * of an attachment point on each rigid body and the orientation that should
 * be maintained between them.  Contrary to the other joints, the positions
 * that are set with the mutators are expected to be specified in the center
 * of mass frame of reference while the orientation quaternion for each
 * attachment is relative to the orientation of the rigid body.  Please
 * read more details in the user manual.
 *
 */

#include <MePrecision.h>
#include <MdtTypes.h>


#ifdef __cplusplus
extern "C"
{
#endif


MEPUBLIC
MdtRPROJointID    MEAPI MdtRPROJointCreate(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtRPROJointReset(const MdtRPROJointID j);
MEPUBLIC
MdtConstraintID   MEAPI MdtRPROJointQuaConstraint(const MdtRPROJointID j);
MEPUBLIC
MdtRPROJointID    MEAPI MdtConstraintDCastRPROJoint(const MdtConstraintID c);

#define                 MdtRPROJointDestroy(j) \
                            MdtConstraintDestroy(MdtRPROJointQuaConstraint(j))
#define                 MdtRPROJointEnable(j) \
                            MdtConstraintEnable(MdtRPROJointQuaConstraint(j))
#define                 MdtRPROJointDisable(j) \
                            MdtConstraintDisable(MdtRPROJointQuaConstraint(j))
#define                 MdtRPROJointIsEnabled(j) \
                            MdtConstraintIsEnabled(MdtRPROJointQuaConstraint(j))
#define                 MdtRPROSetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtRPROQuaConstraint(j),k)
#define                 MdtRPROGetSortKey(j) \
                            MdtConstraintGetSortKey(MdtRPROQuaConstraint(j))


/*
  Relative position, relative orientation joint accessors.
*/


MEPUBLIC
void              MEAPI MdtRPROJointGetAttachmentPosition(const MdtRPROJointID j,
                            const unsigned int bodyindex, MeVector3 position);

MEPUBLIC
void              MEAPI MdtRPROJointGetAttachmentQuaternion(const MdtRPROJointID j,
                            const unsigned int bodyindex, MeVector4 quaternion);

MEPUBLIC
void              MEAPI MdtRPROJointGetRelativeQuaternion(const MdtRPROJointID j, MeVector4 q);

MEPUBLIC
void              MEAPI MdtRPROJointGetRelativeAngularVelocity(const MdtRPROJointID j, MeVector3 w);

MEPUBLIC
void              MEAPI MdtRPROJointGetAngularStrength(const MdtRPROJointID j, MeVector3 v);

MEPUBLIC
void              MEAPI MdtRPROJointGetLinearStrength(const MdtRPROJointID j, MeVector3 v);

#define                 MdtRPROJointGetBody(j, bodyindex) \
                            MdtConstraintGetBody(MdtRPROJointQuaConstraint(j), bodyindex)
#define                 MdtRPROJointGetUserData(j) \
                            MdtConstraintGetUserData(MdtRPROJointQuaConstraint(j))
#define                 MdtRPROJointGetWorld(j) \
                            MdtConstraintGetWorld(MdtRPROJointQuaConstraint(j))
#define                 MdtRPROJointGetForce(j, bodyindex, f) \
                            MdtConstraintGetForce(MdtRPROJointQuaConstraint(j), bodyindex, f)
#define                 MdtRPROJointGetTorque(j, bodyindex, t) \
                            MdtConstraintGetTorque(MdtRPROJointQuaConstraint(j), bodyindex, t)



/*
  Relative position, relative orientation joint mutators.
*/


MEPUBLIC
void              MEAPI MdtRPROJointSetAttachmentPosition(const MdtRPROJointID j,
                            const MeReal x, const MeReal y, const MeReal z,
                            const unsigned int bodyindex);


MEPUBLIC
void              MEAPI MdtRPROJointSetAttachmentQuaternion(const MdtRPROJointID j,
                            const MeReal q0, const MeReal q1, const MeReal q2, const MeReal q3, const unsigned int bodyindex);

MEPUBLIC
void              MEAPI MdtRPROJointSetRelativeQuaternion(const MdtRPROJointID j, const MeVector4 q);

MEPUBLIC
void              MEAPI MdtRPROJointSetRelativeAngularVelocity(MdtRPROJointID j, MeVector3 w);


MEPUBLIC
void              MEAPI MdtRPROJointSetAngularStrength(const MdtRPROJointID j,
                                                        const MeReal sX, const MeReal sY, const MeReal sZ);
MEPUBLIC
void              MEAPI MdtRPROJointSetLinearStrength(const MdtRPROJointID j,
                                                        const MeReal sX, const MeReal sY, const MeReal sZ);

#define                 MdtRPROJointSetBodies(j, b1, b2) \
                            MdtConstraintSetBodies(MdtRPROJointQuaConstraint(j), b1, b2)
#define                 MdtRPROJointSetUserData(j, d) \
                            MdtConstraintSetUserData(MdtRPROJointQuaConstraint(j), d)


#ifdef __cplusplus
}
#endif

#endif
