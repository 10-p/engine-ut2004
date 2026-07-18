#ifndef _MSTMODELDYNAMICS_H
#define _MSTMODELDYNAMICS_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:41 $ - Revision: $Revision: 1.12.8.1 $

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
 * McdModel Dynamics API.
 *
 * When using the MathEngine Collision toolkit with the MathEngine Dynamics
 * Toolkit, McdModel may have an associated MdtBody. These functions are used
 * for setting and modifying a models associated dynamics information. You can
 * use McdModelGetBody, and then use the Mdt interface to, for example, set
 * the bodies of a constraint.
 */

#ifdef __cplusplus
extern "C" {
#endif


MEPUBLIC
MdtBodyID         MEAPI McdModelGetBody(const McdModelID m);
MEPUBLIC
void              MEAPI McdModelSetBody(const McdModelID m, const MdtBodyID b);

MEPUBLIC
void              MEAPI McdModelDynamicsReset(const McdModelID m);



/* Dynamic Shortcut Macros */

/**
 * Enable the collision model's dynamics body
 */

#define McdModelDynamicsEnable(m) \
    (MdtBodyEnable(McdModelGetBody(m)))

/**
 * Disable the collision model's dynamics body
 */

#define McdModelDynamicsDisable(m) \
    (MdtBodyDisable(McdModelGetBody(m)))

/**
 * Test whether the collision model's dynamics body is enabled
 */

#define McdModelDynamicsIsEnabled(m) \
    (MdtBodyIsEnabled(McdModelGetBody(m)))


/**
 * Set the transformation matrix of the collision model's dynamics body
 */


#define McdModelDynamicsSetTransform(m, t) \
    (MdtBodySetTransform(McdModelGetBody(m), (t)))

/**
 * Set the position of the collision model's dynamics body
 */

#define McdModelDynamicsSetPosition(m, x, y, z) \
    (MdtBodySetPosition(McdModelGetBody(m), (x), (y), (z)))

/**
 * Set the quaternion of the collision model's dynamics body
 */

#define McdModelDynamicsSetQuaternion(m, qw, qx, qy, qz) \
    (MdtBodySetQuaternion(McdModelGetBody(m), (qw), (qx), (qy), (qz)))

/**
 * Set the linear velocity of the collision model's dynamics body
 */

#define McdModelDynamicsSetLinearVelocity(m, vx, vy, vz) \
    (MdtBodySetLinearVelocity(McdModelGetBody(m), (vx), (vy), (vz)))

/**
 * Set the angular velocity of the collision model's dynamics body
 */

#define McdModelDynamicsSetAngularVelocity(m, vx, vy, vz) \
    (MdtBodySetAngularVelocity(McdModelGetBody(m), (vx), (vy), (vz)))

/**
 * Set the linear and angular velocity damping of the collision model's dynamics body
 */

#define McdModelDynamicsSetDamping(m, lvd, avd) \
    do { \
        MdtBodySetLinearVelocityDamping(McdModelGetBody(m), (lvd)); \
        MdtBodySetAngularVelocityDamping(McdModelGetBody(m), (avd)); \
    } while(0)



/**
 * Get the position of the collision model's dynamics body
 */

#define McdModelDynamicsGetPosition(m, p) \
    (MdtBodyGetPosition(McdModelGetBody(m), (p)))

/**
 * Get the quaternion of the collision model's dynamics body
 */

#define McdModelDynamicsGetQuaternion(m, q) \
    (MdtBodyGetQuaternion(McdModelGetBody(m), (q)))

/**
 * Get the linear velocity of the collision model's dynamics body
 */

#define McdModelDynamicsGetLinearVelocity(m, v) \
    (MdtBodyGetLinearVelocity(McdModelGetBody(m), (v)))

/**
 * Get the angular velocity of the collision model's dynamics body
 */
#define McdModelDynamicsGetAngularVelocity(m, v) \
    (MdtBodyGetAngularVelocity(McdModelGetBody(m), (v)))



/**
 * apply a force to the centre of mass of the collision model's dynamics body
 * during the coming time step. Several calls for the same time step result in the forces
 * being accumulated.
 */

#define McdModelDynamicsAddForce(m, fx, fy, fz) \
    (MdtBodyAddForce(McdModelGetBody(m), (fx), (fy), (fz)))

/**
 * get the currently accumulated force to be applied to the centre of mass of the
 * collision model's dynamics body during the next time step.
 */

#define McdModelDynamicsGetForce(m, f) \
    (MdtBodyGetForce(McdModelGetBody(m), (f)))



/**
 * apply a torque to the collision model's dynamics body
 * during the next time step. Several calls for the same time step result in the torques
 * being accumulated.
 */

#define McdModelDynamicsAddTorque(m, tx, ty, tz) \
    (MdtBodyAddTorque(McdModelGetBody(m), (tx), (ty), (tz)))

/**
 * get the torque to be applied to the collision model's dynamics body
 * during the next time step.
 */

#define McdModelDynamicsGetTorque(m, t) \
    (MdtBodyGetTorque(McdModelGetBody(m), (t)))


#ifdef __cplusplus
}
#endif

#endif
