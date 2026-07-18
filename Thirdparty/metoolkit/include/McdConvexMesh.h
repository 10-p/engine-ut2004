#ifndef _MCDCONVEXMESH_H
#define _MCDCONVEXMESH_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.36.2.2 $

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

/**
  @file
  The convex mesh geometry type
*/

#include <McdGeometry.h>
#include <McdGeometryTypes.h>
#include <McdCTypes.h>
#include <McdInteractionTable.h>

#ifdef __cplusplus
extern "C" {
#endif

/** McdConvexMesh is a concrete McdGeometry type. */

MCD_DECLARE_GEOMETRY_TYPE(McdConvexMesh);

/* creation */

MEPUBLIC
McdConvexMeshID   MEAPI McdConvexMeshCreate(McdFramework *frame,McdConvexHull *poly, MeReal fatness);
MEPUBLIC
McdConvexMeshID   MEAPI McdConvexMeshCreateHull(McdFramework *frame,
                            const MeVector3 *vertices,
                            int vertexCount, MeReal fatnessRadius );
MEPUBLIC
McdConvexMeshID   MEAPI McdConvexMeshCreateSphyl(McdFramework *frame, MeReal radius, MeReal height);
MEPUBLIC
void              MEAPI McdConvexMeshDestroy(McdConvexMeshID);

/**
    Create new convex polyhedron object from its vertices. Computes
    the convex hull polygons and edges.
*/

MEPUBLIC
void              MEAPI McdConvexMeshGetXYAABB( McdConvexMeshID, MeMatrix4 tm,
                                                MeReal bounds[4]);


/* accessors */

MEPUBLIC
int               MEAPI McdConvexMeshGetPolygonCount(McdConvexMeshID );
MEPUBLIC
int               MEAPI McdConvexMeshGetPolygonVertexCount(McdConvexMeshID,
                            int polyID);
MEPUBLIC
void              MEAPI McdConvexMeshGetPolygonVertex(McdConvexMeshID,
                            int polyID, int vertexID, MeVector3);
MEPUBLIC
const MeReal*     MEAPI McdConvexMeshGetPolygonVertexPtr(McdConvexMeshID,
                            int polyID, int vertexID);
MEPUBLIC
void              MEAPI McdConvexMeshGetPolygonNormal(McdConvexMeshID, int,
                            MeVector3);
MEPUBLIC
MeI16             MEAPI McdConvexMeshGetMassProperties( McdConvexMeshID mesh, MeMatrix4 relTM,
                            MeMatrix3 m, MeReal* volume);

MEPUBLIC
MeReal            MEAPI McdConvexMeshGetFatness(McdConvexMeshID mesh);

MEPUBLIC
const McdConvexHull * MEAPI McdConvexMeshGetPolyhedron(McdConvexMeshID);
MEPUBLIC
void              MEAPI McdConvexMeshSetPolyhedron(McdConvexMeshID, McdConvexHull *poly, MeReal fatness);

  /* interactions */

/*

MEPUBLIC
MeBool            MEAPI McdConvexMeshConvexMeshHello( McdModelPair* );
MEPUBLIC
MeBool            MEAPI McdConvexMeshConvexMeshIntersect( McdModelPair*, McdIntersectResult* );
MEPUBLIC
void              MEAPI McdConvexMeshConvexMeshGoodbye(McdModelPair*);
MEPUBLIC
MeBool            MEAPI McdBoxConvexMeshIntersect(McdModelPair *p,
                                                  McdIntersectResult *info);
*/
MEPUBLIC
MeBool            MEAPI McdConvexMeshRegisterBoxAndSphereFns(McdFramework *frame);
MEPUBLIC
MeBool            MEAPI McdConvexMeshPrimitivesRegisterInteractions(McdFramework *frame);

MCD_DECLARE_INTERSECT_INTERACTION(ConvexMesh, ConvexMesh);
MCD_DECLARE_INTERSECT_INTERACTION(ConvexMesh, Plane);
MCD_DECLARE_INTERSECT_INTERACTION(Sphyl, ConvexMesh);
MCD_DECLARE_INTERSECT_INTERACTION(Sphere, ConvexMesh);
MCD_DECLARE_INTERSECT_INTERACTION(Box, ConvexMesh);
MCD_DECLARE_INTERSECT_INTERACTION(Plane, ConvexMesh);
MCD_DECLARE_INTERSECT_INTERACTION(ConvexMesh, TriangleList);
MCD_DECLARE_INTERSECT_INTERACTION(Cylinder, ConvexMesh);
MCD_DECLARE_INTERSECT_INTERACTION(ConvexMesh, RGHeightField);

  /* "AsConvex" interactions for primitive-primitive */

/*
MEPUBLIC
MeBool            MEAPI McdBoxBoxAsConvexRegisterInteraction(McdFramework *frame);
MEPUBLIC
MeBool            MEAPI McdBoxSphereAsConvexRegisterInteraction(McdFramework *frame);
MEPUBLIC
MeBool            MEAPI McdCylinderCylinderAsConvexRegisterInteraction(McdFramework *frame);
MEPUBLIC
MeBool            MEAPI McdCylinderBoxAsConvexRegisterInteraction(McdFramework *frame);
*/

MCD_DECLARE_LINESEG_INTERACTION(ConvexMesh);

#ifdef __cplusplus
}
#endif

#endif /* _MCDCONVEXMESH_H */
