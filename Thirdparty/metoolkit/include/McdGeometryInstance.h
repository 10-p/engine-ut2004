#ifndef _MCDGEOMETRYINSTANCE_H
#define _MCDGEOMETRYINSTANCE_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.10.2.1 $

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

    The geometry instance object utility functions

    Geometry Instances track instances of (possibly) shared geometries within Mcd

    You can query geometrical properties of an isolated McdGeometryInstance object
    using the McdGeometryInstance interface, which uses the McdGeometry
    protocol and takes into account its transformation matrix.

*/

#include <McdCTypes.h>

#ifdef __cplusplus
extern "C" {
#endif


MEPUBLIC
void              MEAPI McdGeometryInstanceReset(McdGeometryInstanceID ins);

MEPUBLIC
McdGeometryID     MEAPI McdGeometryInstanceGetGeometry(McdGeometryInstanceID ins);
MEPUBLIC
void              MEAPI McdGeometryInstanceSetGeometry(McdGeometryInstanceID ins, McdGeometryID g);

MEPUBLIC
void              MEAPI McdGeometryInstanceSetTransformPtr(const McdGeometryInstanceID ins,
                                                           const MeMatrix4Ptr geometryTM );
MEPUBLIC
MeMatrix4Ptr      MEAPI McdGeometryInstanceGetTransformPtr(McdGeometryInstanceID ins);


/** returns the bounding sphere radius of the geometry instance in world coordinates */
MEPUBLIC
void              MEAPI McdGeometryInstanceGetBSphere(McdGeometryInstanceID ins, 
                                                      MeVector3 center,                                                      
                                                      MeReal *radius);


/** updates the cached AABB. If finalTM is non-NULL, it uses the AABB of the volume swept by
   the geometry with its current and final positions.
   If tight=True, use a slower more accurate algorithm (for aggregate & convexmesh)
 */

MEPUBLIC
void              MEAPI McdGeometryInstanceUpdateAABB(McdGeometryInstanceID ins,
                                                      const MeMatrix4Ptr finalTM, 
                                                      MeBool tight);

/** returns the extremal point of this geometry instance in the given direction */

MEPUBLIC
void              MEAPI McdGeometryInstanceMaximumPoint(McdGeometryInstanceID ins,
                                                        MeReal * const inDir, 
                                                        MeReal * const outPoint);

/** get the geometry instance's axis-aligned bounding box. This is not computed
by this function, which simply returns the results cached by McdGeometryInstanceUpdateAABB
*/
MEPUBLIC
void              MEAPI McdGeometryInstanceGetAABB(McdGeometryInstanceID ins, 
                                                   MeVector3 minCorner,
                                                   MeVector3 maxCorner);

/** set the material associated with the geometry instance */
MEPUBLIC
void              MEAPI McdGeometryInstanceSetMaterial(McdGeometryInstanceID ins, 
                                                       int material);

/** get the material associated with the geometry instance */
MEPUBLIC
unsigned int      MEAPI McdGeometryInstanceGetMaterial(McdGeometryInstanceID ins);


/** get the n'th child of the geometry instance. If there is no such child, returns zero */
MEPUBLIC
McdGeometryInstanceID
                  MEAPI McdGeometryInstanceGetChild(McdGeometryInstanceID ins, int i);

/** determine whether two geometry instances overlap */
MEPUBLIC
MeBool            MEAPI McdGeometryInstanceOverlap(McdGeometryInstanceID ins1, McdGeometryInstanceID ins2);

MEPUBLIC
McdGeometryType   MEAPI McdGeometryInstanceGetGeometryType(McdGeometryInstanceID ins);

#ifdef __cplusplus
} 
#endif

#endif /* _MCDGEOMETRYINSTANCE_H */
