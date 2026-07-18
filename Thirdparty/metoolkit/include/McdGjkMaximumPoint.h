#ifndef _MCDGJKMaximumPoint_H
#define _MCDGJKMaximumPoint_H
/* -*- mode: C++; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.2.2.1 $

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
#include <McdCTypes.h>
#include <McdGeometryTypes.h>
#include <McdGeometryInstance.h>


#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
  This gets the fatness of the geometry instance.  Presently only convex
  mesh have "fatness" however we need to change this someday.
*/
MeReal McdGjkFatness(McdGeometryInstanceID ins);

/****************************************************************************
  This computes the farthest point on a geometry instance in a given direction.
  Usually this just calls McdGeometryInstanceMaximumPoint, but I may want to
  add some optimizations, etc for ConvexMesh, Sphere, and Sphyl.
*/
void McdGjkMaximumPoint(McdGeometryInstanceID ins, 
                        const MeVector3 v, MeVector3 out);

/****************************************************************************
  This returns the farthest point in a given direction.
  The inDir is a direction in local reference frame.  
  outIndex is the index into the CnvPolyhedron vertices.
  hint is an index that might be a good place to start looking.
  The dot product of inDir with vertex[outIndex] is returned.
*/
MeReal
McdConvexMeshMaximumPointLocal(McdConvexMesh *conv,
                               const MeVector3 inDir, 
                               int hint,
                               MeReal minDist,
                               int *outIndex);

/****************************************************************************
  This returns the farthest point in a given direction.
  The inDir is a direction in WRF.  outPoint is a vertex of the ConvexMesh.
*/
MeReal MEAPI
McdConvexMeshMaximumPointNew(McdGeometryInstanceID ins,
                             const MeVector3 inDir, 
                             MeVector3 outPoint);


#ifdef __cplusplus
}
#endif

#endif

