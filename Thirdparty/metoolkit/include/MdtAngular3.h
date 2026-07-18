#ifndef _MDTANGULAR3_H
#define _MDTANGULAR3_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.17.8.1 $

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
 * MdtAngular3 API functions.
 *
 * This joint constrains one body to have a fixed orientation with
 * respect to another, and does not constrain either boies' position.
 * This constraint may, at the user's choice, become an Angular 2
 * joint by freeing rotation about a specified axis.
 * The default orientation that is maintained by this constraint
 * is that of the bodies when MdtAngular3SetBodies is called.
 */

#include <MePrecision.h>
#include <MdtTypes.h>


#ifdef __cplusplus
extern "C"
{
#endif


MEPUBLIC
MdtAngular3ID     MEAPI MdtAngular3Create(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtAngular3Reset(const MdtAngular3ID j);
MEPUBLIC
MdtConstraintID   MEAPI MdtAngular3QuaConstraint(const MdtAngular3ID j);
MEPUBLIC
MdtAngular3ID     MEAPI MdtConstraintDCastAngular3(const MdtConstraintID c);

#define                 MdtAngular3Destroy(j) \
                            MdtConstraintDestroy(MdtAngular3QuaConstraint(j))
#define                 MdtAngular3Enable(j) \
                            MdtConstraintEnable(MdtAngular3QuaConstraint(j))
#define                 MdtAngular3Disable(j) \
                            MdtConstraintDisable(MdtAngular3QuaConstraint(j))
#define                 MdtAngular3IsEnabled(j) \
                            MdtConstraintIsEnabled(MdtAngular3QuaConstraint(j))
#define                 MdtAngular3SetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtAngular3QuaConstraint(j),k)
#define                 MdtAngular3GetSortKey(j) \
                            MdtConstraintGetSortKey(MdtAngular3QuaConstraint(j))

/*
  Angular3 joint accessors.
*/

MEPUBLIC
MeBool            MEAPI MdtAngular3RotationIsEnabled( const MdtAngular3ID j );
MEPUBLIC
MeReal            MEAPI MdtAngular3GetStiffness( const MdtAngular3ID j );
MEPUBLIC
MeReal            MEAPI MdtAngular3GetDamping( const MdtAngular3ID j );

#define                 MdtAngular3GetAxis(j, axis) \
                            MdtConstraintGetAxis(MdtAngular3QuaConstraint(j), axis)
#define                 MdtAngular3GetBody(j, bodyindex) \
                            MdtConstraintGetBody(MdtAngular3QuaConstraint(j), bodyindex)
#define                 MdtAngular3GetUserData(j) \
                            MdtConstraintGetUserData(MdtAngular3QuaConstraint(j))
#define                 MdtAngular3GetWorld(j) \
                            MdtConstraintGetWorld(MdtAngular3QuaConstraint(j))
#define                 MdtAngular3GetForce(j, bodyindex, f) \
                            MdtConstraintGetForce(MdtAngular3QuaConstraint(j), bodyindex, f)
#define                 MdtAngular3GetTorque(j, bodyindex, t) \
                            MdtConstraintGetTorque(MdtAngular3QuaConstraint(j), bodyindex, t)

/*
  Angular3 joint mutators
*/

MEPUBLIC
void              MEAPI MdtAngular3EnableRotation( const MdtAngular3ID j,
                            const MeBool NewRotationState );
MEPUBLIC
void              MEAPI MdtAngular3SetStiffness( const MdtAngular3ID j, const MeReal s );
MEPUBLIC
void              MEAPI MdtAngular3SetDamping( const MdtAngular3ID j, const MeReal d );

#define                 MdtAngular3SetAxis(j, x, y, z) \
                            MdtConstraintSetAxis(MdtAngular3QuaConstraint(j), x, y, z)
#define                 MdtAngular3SetBodies(j, b1, b2) \
                            MdtConstraintSetBodies(MdtAngular3QuaConstraint(j), b1, b2)
#define                 MdtAngular3SetUserData(j, d) \
                            MdtConstraintSetUserData(MdtAngular3QuaConstraint(j), d)


#ifdef __cplusplus
}
#endif


#endif
