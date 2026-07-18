#ifndef _MCDQHULLTYPES_H
#define _MCDQHULLTYPES_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.7.2.2 $

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

#include <MeMath.h>

/****************************************************************************
  These are the structures used to store a convex hull.

  Separate functions are provided to measure/allocate/populate the hull
  so that you can use any allocation method (such as alloca).
  
  This data allows you to easily traverse the convex hull using a hill 
  climbing algorithm, either by walking adjacent faces, or walking 
  adjacent vertices.  Each face points to a list of edges which are sorted
  in anti-clockwise order around the face.  Also each vertex points to 
  a list of edges which are sorted ACW around the vertex.  The list of
  all edges stores every edge two times (one each direction).  Every edge
  indicates its two end points and the face on its right side.  If you want
  to look at every edge once only, then you can ignore the backward
  edges, for instance, fromVert < toVert.
  
  The edges are sorted in anti-clockwise order around each face.
  To access the edges sorted in anti-clockwise order around a vertex, you go
  through the "edgeIndex" array.  For example,

    //  every edge/face/vertex around a vertex in ACW order

    for (i=cnv.vertex[a].firstEdgeIndex; i<cnv.vertex[a+1].firstEdgeIndex; ++i)
    {
        cout << cnv.edge[cnv.edgeIndex[i]].rightFace;
        cout << cnv.edge[cnv.edgeIndex[i]].toVert;
    }

    //  every edge/face/vertex around a face in ACW order

    for (i=cnv.face[a].firstEdge; i<cnv.face[a+1].firstEdge; ++i)
    {
        cout << cnv.edge[i].rightFace;
        cout << cnv.edge[i].fromVert;
    }

  For convenience, the last face/vertex in each list is a sentinel 
  in which firstEdge=numEdges.
*/

typedef struct McdConvexHull 
{
    struct McdCnvVertex *vertex;  /**< all vertices, not in any order */
    struct McdCnvFace *face;      /**< all faces, not in any order */
    struct McdCnvEdge *edge;      /**< all edges, sorted by leftFace and ACW */
    int *edgeIndex;               /**< this is used to access edges via vertices */
    int numVertex;
    int numFace;
    int numEdge;                  /**< each edge is stored twice */
}
McdConvexHull;

typedef struct McdCnvVertex 
{
    MeVector3 position;    /**< position of vertex in LRF */
    int firstEdgeIndex;    /**< index into edgeIndex array */
}
McdCnvVertex;

typedef struct McdCnvFace
{
    MeVector3 normal;      /**< perpendicular to face, pointing outside */
    int firstEdge;         /**< index into the edge array */
}
McdCnvFace;

typedef struct McdCnvEdge
{
    MeReal invLength;  /**< 1 / length of edge */
    int fromVert;    /**< index of vertex of start of edge */
    int toVert;      /**< index of vertex of end of edge */
    int rightFace;   /**< index of the face on the right side of the edge */
    int leftFace;    /**< index of the face on the left side of the edge */
}
McdCnvEdge;


/****************************************************************************
  These are the functions.
*/

#ifdef __cplusplus
extern "C" {
#endif 

/****************************************************************************
  This takes some points and returns an McdConvexHull structure.
  The memory for the convex hull data is allocated with MeMemoryAPI.create.

  returns 1 on success, or 0 on error.
*/
MEPUBLIC
int McdComputeHull(McdConvexHull *cnv,
                   int numpoints, const MeVector3 *points);

/****************************************************************************
  This computes the convex hull and returns the number of vertices,
  edges, and faces.

  It is presumed that the caller will invoke this function to
  determine the memory required for the convex hull, then allocate the 
  memory, and then call McdGetHullData to get the actual data.

  returns 1 on success, or 0 on error.
*/
int McdComputeHullSizes(McdConvexHull *cnv,
                        int numpoints, const MeVector3 *points);

/****************************************************************************
  This allocates memory for the convex hull based on the sizes.
  One extra face and vertex is required for the sentinel.
*/
void McdAllocateHull(McdConvexHull *cnv);

/****************************************************************************
  This gets the convex hull data that was generated in the previous call
  to McdComputeHullSizes.

  The caller must allocate the arrays for faces, vertices, edges and 
  edge index.  NOTE!! the face and vertex array must have room for one
  extra for the end sentinel.

  returns 1 on success, 0 on failure.
*/
int McdGetHullData(McdConvexHull *cnv);

/****************************************************************************
  This deallocates the memory that is allocated by McdAllocateHull
  or by McdComputeHull.
*/
MEPUBLIC
void McdDeallocateHull(McdConvexHull *cnv);

/****************************************************************************
  This populates a triangle convex hull.  It must have 3 vert, 1 face, and
  3 edges.  The points p1,p2,p3 must be ACW.
*/
void McdGetTriangleHull(McdConvexHull *cnv, const MeVector3 p1, 
                        const MeVector3 p2, const MeVector3 p3);

/*
    =======================================================================
        ACCESSOR METHODS
    =======================================================================
  Functions are provided below for convenience to access common things
  that are not trivial to access directly in the data structures.
*/

/****************************************************************************
  Get the number of edges (or vertices or adjacent faces) around a given face.
*/
MEPUBLIC
int McdCnvFaceGetCount(const McdConvexHull *cnv, int face);

/****************************************************************************
  Get the i-th edge of a face.  The edges are guaranteed to form a directed 
  path (from-to) in ACW order around the face. 
  You can look at the edge.rightFace to get all adjacent faces.
*/
const McdCnvEdge *McdCnvFaceGetEdge(const McdConvexHull *cnv, int face, int i);

/****************************************************************************
  These three functions get the i-th vertex on the face.
*/
int McdCnvFaceGetVertexId(const McdConvexHull *cnv, int face, int i);
const McdCnvVertex *McdCnvFaceGetVertex(const McdConvexHull *cnv, int face, int i);
MEPUBLIC
const MeReal *McdCnvFaceGetVertexPosition(const McdConvexHull *cnv, int face, int i);

/****************************************************************************
  Get the number of edges (or vertices or faces) around a given vertex.
*/
int McdCnvVertexGetCount(const McdConvexHull *cnv, int vertex);

/****************************************************************************
  Get the id of the i-th edge around a vertex.
*/
int McdCnvVertexGetEdgeId(const McdConvexHull *cnv, int vertex, int i);

/****************************************************************************
  Get the i-th edge from a vertex.    The edges are guaranteed
  to be all "from" the specified vertex "to" another vertex.
  You can look at the edge.rightFace to get all adjacent faces.
*/
const McdCnvEdge *McdCnvVertexGetEdge(const McdConvexHull *cnv, int vertex, int i);

/****************************************************************************
  Get the id of the i-th vertex that is adjacent to a given vertex.
*/
int McdCnvVertexGetNeighbor(const McdConvexHull *cnv, int vertex, int i);



#ifdef __cplusplus
}
#endif 

#endif /* _MCDQHULLTYPES_H */
