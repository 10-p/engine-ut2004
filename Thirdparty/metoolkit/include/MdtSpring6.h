#ifndef _MDTSPRING_H
#define _MDTSPRING6_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.2.2.1 $

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
 *  MdtSpring6 API functions.
 *
 *  This constraint is a 6 degree-of-freedom spring. It allows you to connect
 *  a body to the world, or another body, and it will be springy both in
 *  relation to the position and orientation of the other body. You can 
 *  specify different stiffness and damping for each linear and angular axis,
 *  but must specify the reference frame of the constraint by placing both 
 *  bodies in their equilibrium orientation and calling MdtSpring6SetAxes.
 *  Eg. if linear stiffness is MEINFINITY, angular stiffess is zero, and all
 *  dampings are zero, this constraint will act just like a ball-and-socket.
 */

#include <MePrecision.h>
#include <MdtTypes.h>


#ifdef __cplusplus
extern "C"
{
#endif


MEPUBLIC
MdtSpring6ID     MEAPI MdtSpring6Create(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtSpring6Reset(const MdtSpring6ID j);
MEPUBLIC
MdtConstraintID   MEAPI MdtSpring6QuaConstraint(const MdtSpring6ID j);
MEPUBLIC
MdtSpring6ID     MEAPI MdtConstraintDCastSpring6(const MdtConstraintID c);

#define                 MdtSpring6Destroy(j) \
                            MdtConstraintDestroy(MdtSpring6QuaConstraint(j))
#define                 MdtSpring6Enable(j) \
                            MdtConstraintEnable(MdtSpring6QuaConstraint(j))
#define                 MdtSpring6Disable(j) \
                            MdtConstraintDisable(MdtSpring6QuaConstraint(j))
#define                 MdtSpring6IsEnabled(j) \
                            MdtConstraintIsEnabled(MdtSpring6QuaConstraint(j))
#define                 MdtSpring6SetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtSpring6QuaConstraint(j),k)
#define                 MdtSpring6GetSortKey(j) \
                            MdtConstraintGetSortKey(MdtSpring6QuaConstraint(j))

/*
  Spring6 joint accessors.
*/

MEPUBLIC
void              MEAPI MdtSpring6GetPosition(const MdtSpring6ID j, MeVector3 v);
MEPUBLIC
MeReal            MEAPI MdtSpring6GetLinearStiffness(const MdtSpring6ID j, int axisindex);
MEPUBLIC
MeReal            MEAPI MdtSpring6GetLinearDamping(const MdtSpring6ID j, int axisindex);
MEPUBLIC
MeReal            MEAPI MdtSpring6GetAngularStiffness(const MdtSpring6ID j, int axisindex);
MEPUBLIC
MeReal            MEAPI MdtSpring6GetAngularDamping(const MdtSpring6ID j, int axisindex);
MEPUBLIC
void              MEAPI MdtSpring6GetWorldLinearVelocity(const MdtSpring6ID j, MeVector3 v);
MEPUBLIC
void              MEAPI MdtSpring6GetWorldAngularVelocity(const MdtSpring6ID j, MeVector3 v);


#define                 MdtSpring6GetPosition(j, v) \
                            MdtConstraintGetPosition(MdtSpring6QuaConstraint(j), v)
#define                 MdtSpring6GetAxes(j, p, o) \
                            MdtConstraintGetAxes(MdtSpring6QuaConstraint(j), p, o)
#define                 MdtSpring6GetBody(j, bodyindex) \
                            MdtConstraintGetBody(MdtSpring6QuaConstraint(j), bodyindex)
#define                 MdtSpring6GetUserData(j) \
                            MdtConstraintGetUserData(MdtSpring6QuaConstraint(j))
#define                 MdtSpring6GetWorld(j) \
                            MdtConstraintGetWorld(MdtSpring6QuaConstraint(j))
#define                 MdtSpring6GetForce(j, bodyindex, f) \
                            MdtConstraintGetForce(MdtSpring6QuaConstraint(j), bodyindex, f)
#define                 MdtSpring6GetTorque(j, bodyindex, t) \
                            MdtConstraintGetTorque(MdtSpring6QuaConstraint(j), bodyindex, t)

/*
  Spring6 joint mutators
*/

MEPUBLIC
void              MEAPI MdtSpring6SetLinearStiffness(const MdtSpring6ID j, int axisindex,
                            MeReal s);
MEPUBLIC
void              MEAPI MdtSpring6SetLinearDamping(const MdtSpring6ID j, int axisindex,
                            MeReal d);
MEPUBLIC
void              MEAPI MdtSpring6SetAngularStiffness(const MdtSpring6ID j, int axisindex,
                            MeReal s);
MEPUBLIC
void              MEAPI MdtSpring6SetAngularDamping(const MdtSpring6ID j, int axisindex,
                            MeReal d);
MEPUBLIC
void              MEAPI MdtSpring6SetWorldLinearVelocity(const MdtSpring6ID j, 
                            MeReal vx, MeReal vy, MeReal vz);
MEPUBLIC
void              MEAPI MdtSpring6SetWorldAngularVelocity(const MdtSpring6ID j, 
                            MeReal vx, MeReal vy, MeReal vz);

#define                 MdtSpring6SetPosition(j, x, y, z) \
                            MdtConstraintSetPosition(MdtSpring6QuaConstraint(j), x, y, z)
#define                 MdtSpring6SetAxes(j, px, py, pz, ox, oy, oz) \
                            MdtConstraintSetAxes(MdtSpring6QuaConstraint(j), px, py, pz, ox, oy, oz)
#define                 MdtSpring6SetBodies(j, b1, b2) \
                            MdtConstraintSetBodies(MdtSpring6QuaConstraint(j), b1, b2)
#define                 MdtSpring6SetUserData(j, d) \
                            MdtConstraintSetUserData(MdtSpring6QuaConstraint(j), d)


#ifdef __cplusplus
}
#endif


#endif
