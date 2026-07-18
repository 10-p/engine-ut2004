#ifndef _MCDGEOMETRYTYPES_H
#define _MCDGEOMETRYTYPES_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/09 12:01:25 $ - Revision: $Revision: 1.19.2.7 $

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
  All geometry types
*/

#include <McdGeometry.h>
#include <McdQHullTypes.h>

typedef McdGeometryID McdNullID;
typedef McdGeometryID McdSphereID;
typedef McdGeometryID McdBoxID;
typedef McdGeometryID McdPlaneID;
typedef McdGeometryID McdCylinderID;
typedef McdGeometryID McdSphylID;
typedef McdGeometryID McdTriangleListID;
typedef McdGeometryID McdConvexMeshID;
typedef McdGeometryID McdAggregateID;

enum
{
    kMcdGeometryTypeNull = 0,
    kMcdGeometryTypeSphere = 1,
    kMcdGeometryTypeBox = 2,
    kMcdGeometryTypePlane = 3,
    kMcdGeometryTypeCylinder = 4,
    kMcdGeometryTypeSphyl = 5,
    kMcdGeometryTypeTriangleList = 6,
    kMcdGeometryTypeConvexMesh = 7,
    kMcdGeometryTypeAggregate = 8,
    kMcdGeometryBuiltInTypes = 9
};


  /** @internal */

/** McdNull is a concrete McdGeometry type with no properties. Attempts to
call virtual functions on this geometry will fail.
@see McdGeometry.h */

typedef struct
{
  McdGeometry m_g;

  MeReal mR[3];
  MeReal mRadius;

} McdNull;


/** McdBox is a concrete McdGeometry type. All functions taking type
    McdGeometryID as argument can be called with a McdBoxID argument.
@see McdGeometry.h */


typedef struct
{
  McdGeometry m_g;

  MeReal mR[3];
  MeReal mRadius;

} McdBox;


typedef struct
{
  McdGeometry m_g;

  MeReal mR;
  /* half-height */
  MeReal mRz;
  MeReal mSphereRadius;

} McdCylinder;


  /** McdSphere is a concrete McdGeometry type. */

typedef struct
{
  McdGeometry m_g;

  MeReal mRadius;

} McdSphere;



typedef struct
{
  McdGeometry m_g;

} McdPlane;


typedef struct
{
    McdGeometry m_g;

    MeReal mRadius;
    MeReal mHalfHeight;
} McdSphyl;

/* 
Collision user triangle. If the triangle is not two-sided, the normal 
is assumed to be  facing outwards (i.e. penetration will be along the 
direction opposite to the normal). For an edge to be taken into account 
when computing the minimal penetration plane, which also defines the 
contact normal for any contacts with that triangle, the edge must be set 
to be "active". In the standard version of McdTriangleList, all edges are 
active, and the triangle is two-sided
*/

typedef enum
{
    kMcdTriangleUseSmallestPenetration = 1,
    kMcdTriangleTwoSided               = 2,
    kMcdTriangleUseEdge0               = 4,  /* use edge v1 - v0 */
    kMcdTriangleUseEdge1               = 8,  /* use edge v2 - v1 */
    kMcdTriangleUseEdge2               = 16, /* use edge v0 - v2 */
    kMcdTriangleUseEdges               = 28,
    kMcdTriangleStandard               = 31
} McdTriangleFlags;


typedef struct _McdUserTriangle
{
    MeVector3 *vertices[3]; /**< pointers to vertices */
    MeVector3 *normal;

    union {void *ptr; int tag;} triangleData;
    McdTriangleFlags flags;

} McdUserTriangle;


/** user function to get triangles given a model containing a
triangle list geometry. The triangle list can be either model
in the pair. */


typedef int (MEAPI * McdTriangleListFnPtr) (McdModelPair* modelTriListPair,
                                            McdUserTriangle *triangle,
                                            MeVector3 pos, 
                                            MeReal radius,
                                            int maxTriangles);


typedef struct McdTriangleList
{
    McdGeometry m_g;
    
    MeVector3 center;
    MeVector3 radius;
    int triangleMaxCount;        /**< max number of triangles in list */
    void *userData;              /**< user geometry representation, or other user data */
    /**< function to calculate triangle list. Returns the number of triangles to intersect */
    McdTriangleListFnPtr triangleListGenerator; 

    McdUserTriangle *list;       /**< pointer to list. Safe to modify in callback if e.g.
                                      you already have the list */
} McdTriangleList;

#define MCD_MAX_POLY_VERTICES 8

typedef struct _McdUserPolygon
{
    int vertexList[MCD_MAX_POLY_VERTICES+1]; /**< pointers to vertices */
    int vertexCount;
    union {void *ptr; int tag;} polyData;
    int counter;
    int clump;
} McdUserPolygon;


/** user function to get triangles given a model containing a
triangle list geometry. The triangle list can be either model
in the pair. */


typedef int (MEAPI * McdPolyMashFnPtr) (McdModelPair* pair,
                                        MeVector3 **vertices,
                                        int *vertexCount,
                                        McdUserPolygon **poly,
                                        int *polyCount);


typedef struct McdPolyMash
{
    McdGeometry m_g;
    MeVector3 centre;
    MeVector3 radius;
    McdPolyMashFnPtr generator; 
    void *userData;              /**< user geometry representation, or other user data */    
} McdPolyMash;



typedef struct
{

  McdGeometry m_g;
  McdConvexHull mHull;
  MeReal mFatness;
  MeReal mBoundingSphereRadius;
  MeVector3 mBoundingSphereCenter;
} McdConvexMesh;



typedef struct 
{
    MeMatrix4 mRelTM;
    McdGeometryID mGeometry;
} McdAggregateElement;


typedef struct
{
    McdGeometry m_g;
    McdAggregateElement *elementTable;
    int elementCount;
    int elementCountMax;
} McdAggregate;

#endif /* _MCDGEOMETRYTYPES_H */
