#ifndef _MDTBODY_H
#define _MDTBODY_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/29 15:38:56 $ - Revision: $Revision: 1.42.2.3 $

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
 * MdtBody API functions.
 */

#include <MePrecision.h>
#include <MdtTypes.h>

#ifdef __cplusplus
extern "C"
{
#endif


MEPUBLIC
MdtBodyID         MEAPI MdtBodyCreate(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtBodyReset(const MdtBodyID b);
MEPUBLIC
void              MEAPI MdtBodyDestroy(const MdtBodyID b);
MEPUBLIC
MdtBodyID         MEAPI MdtBodyCopy(const MdtBodyID b, const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtBodyEnable(const MdtBodyID b);
MEPUBLIC
void              MEAPI MdtBodyDisable(const MdtBodyID b);
MEPUBLIC
MeBool            MEAPI MdtBodyIsEnabled(const MdtBodyID b);
MEPUBLIC
void              MEAPI MdtBodyLog(const MdtBodyID b);
MEPUBLIC
void              MEAPI MdtBodyDisableConstraints(const MdtBodyID b);
MEPUBLIC
void              MEAPI MdtBodyDestroyConstraints(const MdtBodyID b);
MEPUBLIC
void              MEAPI MdtBodyDisableContacts(const MdtBodyID b);
MEPUBLIC
void              MEAPI MdtBodyDestroyContacts(const MdtBodyID b);

/* Body accessors */

MEPUBLIC
MdtWorldID        MEAPI MdtBodyGetWorld(const MdtBodyID b);
MEPUBLIC
int               MEAPI MdtBodyGetPartition(const MdtBodyID b);
MEPUBLIC
MeMatrix4Ptr      MEAPI MdtBodyGetTransformPtr(const MdtBodyID b);
MEPUBLIC
void              MEAPI MdtBodyGetTransform(const MdtBodyID b,MeMatrix4 m);
MEPUBLIC
void             *MEAPI MdtBodyGetUserData(const MdtBodyID b);
MEPUBLIC
MeReal            MEAPI MdtBodyGetMass(const MdtBodyID b);
MEPUBLIC
void              MEAPI MdtBodyGetInertiaTensor(const MdtBodyID b, MeMatrix3 i);
MEPUBLIC
void              MEAPI MdtBodyGetPosition(const MdtBodyID b, MeVector3 p);
MEPUBLIC
void              MEAPI MdtBodyGetQuaternion(const MdtBodyID b, MeVector4 q);
MEPUBLIC
void              MEAPI MdtBodyGetOrientation(const MdtBodyID b, MeMatrix3 R);
MEPUBLIC
void              MEAPI MdtBodyGetLinearVelocity(const MdtBodyID b,
                            MeVector3 v);
MEPUBLIC
MeReal           *MEAPI MdtBodyGetLinearVelocityPtr(const MdtBodyID b);
MEPUBLIC
void              MEAPI MdtBodyGetAngularVelocity(const MdtBodyID b,
                            MeVector3 v);
MEPUBLIC
MeReal           *MEAPI MdtBodyGetAngularVelocityPtr(const MdtBodyID b);
MEPUBLIC
void              MEAPI MdtBodyGetVelocityAtPoint(const MdtBodyID body,
                            MeVector3 p, MeVector3 v);
MEPUBLIC
void              MEAPI MdtBodyGetLinearAcceleration(const MdtBodyID b,
                            MeVector3 v);
MEPUBLIC
void              MEAPI MdtBodyGetAngularAcceleration(const MdtBodyID b,
                            MeVector3 v);
MEPUBLIC
void              MEAPI MdtBodyGetFastSpinAxis(const MdtBodyID b, MeVector3 v);
MEPUBLIC
MeReal            MEAPI MdtBodyGetLinearVelocityDamping(const MdtBodyID b);
MEPUBLIC
MeReal            MEAPI MdtBodyGetAngularVelocityDamping(const MdtBodyID b);
MEPUBLIC
void              MEAPI MdtBodyGetForce(const MdtBodyID b, MeVector3 v);
MEPUBLIC
void              MEAPI MdtBodyGetTorque(const MdtBodyID b, MeVector3 v);
MEPUBLIC
MeReal            MEAPI MdtBodyGetKineticEnergy(const MdtBodyID b);
MEPUBLIC
MeReal            MEAPI MdtBodyGetEnabledTime(const MdtBodyID b);
MEPUBLIC
void              MEAPI MdtBodyGetCenterOfMassRelativeTransform(
                            const MdtBodyID b, MeMatrix4 t);
MEPUBLIC
void              MEAPI MdtBodyGetCenterOfMassPosition(const MdtBodyID b,
                            MeVector3 pos);
MEPUBLIC
void              MEAPI MdtBodyGetCenterOfMassRelativePosition(const MdtBodyID b,
                            MeVector3 pos);
MEPUBLIC
MeMatrix4Ptr      MEAPI MdtBodyGetCenterOfMassTransformPtr(const MdtBodyID b);
MEPUBLIC
MeReal            MEAPI MdtBodyGetSafeTime(const MdtBodyID b);
MEPUBLIC
MeI32             MEAPI MdtBodyGetSortKey(const MdtBodyID b);

MEPUBLIC
MdtConstraintID   MEAPI MdtBodyGetFirstConstraint(MdtBodyID b);
MEPUBLIC
MdtConstraintID   MEAPI MdtBodyGetNextConstraint(MdtBodyID b,MdtConstraintID c);

/*
  Body mutators
*/

MEPUBLIC
void              MEAPI MdtBodySetUserData(const MdtBodyID b, void *d);
MEPUBLIC
void              MEAPI MdtBodySetTransform(const MdtBodyID b,
                            const MeMatrix4 tm);
MEPUBLIC
void              MEAPI MdtBodySetMass(const MdtBodyID b, const MeReal mass);
MEPUBLIC
void              MEAPI MdtBodySetInertiaTensor(const MdtBodyID b,
                            const MeMatrix3 i);
MEPUBLIC
void              MEAPI MdtBodySetSphericalInertiaTensor(const MdtBodyID b,
                            const MeReal i);
MEPUBLIC
void              MEAPI MdtBodySetPosition(const MdtBodyID b,
                            const MeReal x, const MeReal y, const MeReal z);
MEPUBLIC
void              MEAPI MdtBodySetOrientation(const MdtBodyID b,
                            const MeMatrix3 R);
MEPUBLIC
void              MEAPI MdtBodySetQuaternion(const MdtBodyID b,
                            const MeReal qw, const MeReal qx, const MeReal qy,
                            const MeReal qz);
MEPUBLIC
void              MEAPI MdtBodySetLinearVelocity(const MdtBodyID b,
                            const MeReal dx, const MeReal dy, const MeReal dz);
MEPUBLIC
void              MEAPI MdtBodySetAngularVelocity(const MdtBodyID b,
                            const MeReal wx, const MeReal wy, const MeReal wz);
MEPUBLIC
void              MEAPI MdtBodySetLinearVelocityDamping(const MdtBodyID b,
                                                        const MeReal d);
MEPUBLIC
void              MEAPI MdtBodySetAngularVelocityDamping(const MdtBodyID b,
                            const MeReal d);
MEPUBLIC
void              MEAPI MdtBodySetFastSpinAxis(const MdtBodyID b,
                            const MeReal x, const MeReal y, const MeReal z);
MEPUBLIC
void              MEAPI MdtBodySetNoFastSpinAxis(const MdtBodyID b);

MEPUBLIC
void              MEAPI MdtBodyEnableNonSphericalInertia(const MdtBodyID b);

MEPUBLIC
void              MEAPI MdtBodyDisableNonSphericalInertia(const MdtBodyID b);

MEPUBLIC
MeBool            MEAPI MdtBodyNonSphericalInertiaIsEnabled(const MdtBodyID b);

MEPUBLIC
void              MEAPI MdtBodyEnableCoriolisForce(const MdtBodyID b);

MEPUBLIC
void              MEAPI MdtBodyDisableCoriolisForce(const MdtBodyID b);

MEPUBLIC
MeBool            MEAPI MdtBodyCoriolisForceIsEnabled(const MdtBodyID b);

MEPUBLIC
void              MEAPI MdtBodySetCenterOfMassPosition(
                            const MdtBodyID b, const MeVector3 pos);
MEPUBLIC
void              MEAPI MdtBodySetCenterOfMassRelativePosition(
                            const MdtBodyID b, const MeVector3 pos);
MEPUBLIC
void              MEAPI MdtBodySetCenterOfMassRelativeTransform(
                            const MdtBodyID b, const MeMatrix4 t);
MEPUBLIC
void              MEAPI MdtBodySetSafeTime(const MdtBodyID b, const MeReal t);
MEPUBLIC
void              MEAPI MdtBodySetSortKey(const MdtBodyID b, const MeI32 key);
/*
  Iterators.
*/

MEPUBLIC
MdtBodyID         MEAPI MdtBodyGetFirst(const MdtWorldID w);
MEPUBLIC
MdtBodyID         MEAPI MdtBodyGetNext(const MdtBodyID b);


/*
  Body others.
*/
MEPUBLIC
void              MEAPI MdtBodyForAllConstraints(const MdtBodyID b,
                            MdtConstraintIteratorCBPtr cb, void* ccbdata);
MEPUBLIC
void              MEAPI MdtBodyResetForces(const MdtBodyID b);
MEPUBLIC
void              MEAPI MdtBodyResetImpulses(const MdtBodyID b);
MEPUBLIC
void              MEAPI MdtBodyAddForce(const MdtBodyID b,
                            const MeReal fx, const MeReal fy, const MeReal fz);
MEPUBLIC
void              MEAPI MdtBodyAddForceAtPosition(const MdtBodyID b,
                            const MeReal fx, const MeReal fy, const MeReal fz,
                            const MeReal px, const MeReal py, const MeReal pz);
MEPUBLIC
void              MEAPI MdtBodyAddTorque(const MdtBodyID b,
                            const MeReal tx, const MeReal ty, const MeReal tz);

MEPUBLIC
void              MEAPI MdtBodyAddImpulse(const MdtBodyID b,
                            const MeReal ix, const MeReal iy, const MeReal iz);
MEPUBLIC
void              MEAPI MdtBodyAddImpulseAtPosition(const MdtBodyID b,
                            const MeReal ix, const MeReal iy, const MeReal iz,
                            const MeReal px, const MeReal py, const MeReal pz);
MEPUBLIC
MeBool            MEAPI MdtBodyIsMovingTest(const MdtBodyID b,
                            const MdtPartitionParams* params);



#ifdef __cplusplus
}
#endif


#endif
