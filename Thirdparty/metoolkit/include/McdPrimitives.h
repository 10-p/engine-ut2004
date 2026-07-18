#ifndef _MCDPRIMITIVES_H
#define _MCDPRIMITIVES_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.33.2.1 $

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
#include <McdCTypes.h>
#include <McdInteractionTable.h>
#include <McdBox.h>
#include <McdSphere.h>
#include <McdPlane.h>
#include <McdCylinder.h>
#include <McdTriangleList.h>
#include <McdSphyl.h>

/**
  @file
  Geometrical primitive types and interactions
*/

#ifdef __cplusplus
extern "C" {
#endif


/** Returns the number of primitive geometry types. 
*/

MEPUBLIC
unsigned int      MEAPI McdPrimitivesGetTypeCount();

  /** Register all the primitive geometry types with the Mcd system */

MEPUBLIC
void              MEAPI McdPrimitivesRegisterTypes(McdFramework *frame);

  /** Register all the available interactions between primitive geometry types.
   The geometry types must be registered before calling this function.
   Return value indicates success: resources may be allocated inside.
*/

MEPUBLIC
MeBool            MEAPI McdPrimitivesRegisterInteractions(McdFramework *frame);

  /** Register the sphere, box and plane geometry types */

MEPUBLIC
void              MEAPI McdSphereBoxPlaneRegisterTypes(McdFramework *frame);

  /** Register all available interactions between sphere, box and plane geometry types.
   The geometry types must be registered before calling this function. */

MEPUBLIC
void              MEAPI McdSphereBoxPlaneRegisterInteractions(McdFramework *frame);

  /* Individual primitive-primitive interaction registration */

MCD_DECLARE_SAFETIME_INTERACTION(Sphere,Sphere);
MCD_DECLARE_SAFETIME_INTERACTION(Sphere,Plane);
MCD_DECLARE_SAFETIME_INTERACTION(Box,Sphere);
MCD_DECLARE_SAFETIME_INTERACTION(Box,Box);
MCD_DECLARE_SAFETIME_INTERACTION(Box,Plane);
MCD_DECLARE_SAFETIME_INTERACTION(Cylinder,Sphere);
MCD_DECLARE_SAFETIME_INTERACTION(Cylinder,Cylinder);
MCD_DECLARE_SAFETIME_INTERACTION(Box,Cylinder);

MCD_DECLARE_INTERSECT_INTERACTION(Cylinder,Plane);
MCD_DECLARE_INTERSECT_INTERACTION(Sphere,TriangleList);
MCD_DECLARE_INTERSECT_INTERACTION(Box,TriangleList);
MCD_DECLARE_INTERSECT_INTERACTION(Cylinder,TriangleList);

MCD_DECLARE_INTERSECT_INTERACTION(Sphyl,Plane);
MCD_DECLARE_INTERSECT_INTERACTION(Sphyl,Sphere);
MCD_DECLARE_INTERSECT_INTERACTION(Sphyl,Sphyl);
MCD_DECLARE_INTERSECT_INTERACTION(Sphyl,Box);
MCD_DECLARE_INTERSECT_INTERACTION(Sphyl,Cylinder);
MCD_DECLARE_INTERSECT_INTERACTION(Sphyl,TriangleList);


/*****************************************************************************
General note for the following functions:                                    *
int MEAPI Ix*LineSegment(const McdModelID inBox,                             *
                           MeReal* const inOrig, MeReal* const inDest,       *
                           McdLineSegIntersectResult * info );               *
 - The function returns 1 if an intersection is found, otherwise returns 0   *
 - inOrig and inDest are in world's c.s.                                     *
 - info->isctPoint is in world's c.s. In case of box and sphere, it's the    *
   first intersection point from inOrig                                      *
 - info->normal is the normal vector from the input CxGeometry type object   *
   at the intersection point, also in world's c.s.                           *
 - info->distance is the REAL distance from inOrig to the intersection point *
   (not a parametrized distance).                                            *
 - When the inOrig point is inside of box or sphere, there is no             *
   info->normal is returned and info->distance = 0                           *
******************************************************************************/

MCD_DECLARE_LINESEG_INTERACTION(Box);
MCD_DECLARE_LINESEG_INTERACTION(Sphere);
MCD_DECLARE_LINESEG_INTERACTION(Cylinder);
MCD_DECLARE_LINESEG_INTERACTION(Plane);
MCD_DECLARE_LINESEG_INTERACTION(Sphyl);

#ifdef __cplusplus
}
#endif

#endif /* _MCDPRIMITIVES_H */
