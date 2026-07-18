#ifndef _MCDPlaneIntersect_H
#define _MCDPlaneIntersect_H
/* -*- mode: C++; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.1.2.1 $

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


void McdBoxGetSlice(McdGeometryInstanceID ins,
                    const MeVector3 normal,
                    MeReal dist, 
                    int maxVert,
                    int *numVert,
                    MeVector3 *outVert);

void McdConvexMeshPlaneCut(McdConvexMesh *conv,
                           const MeVector3 norm,
                           MeReal dp, 
                           int flags,
                           const MeMatrix4 tm,
                           int maxVert,
                           int *numVert,
                           MeVector3 *outVert);

void McdConvexMeshGetSlice(McdGeometryInstanceID ins,
                           const MeVector3 normal,
                           MeReal dist, 
                           int maxVert,
                           int *numVert,
                           MeVector3 *outVert);

void McdCylinderGetSlice(McdGeometryInstanceID ins,
                         const MeVector3 normal,
                         MeReal dist, 
                         int maxVert,
                         int *numVert,
                         MeVector3 *outVert);

void McdGeometryInstanceGetSlice(McdGeometryInstanceID ins,
                                 const MeVector3 normal,
                                 MeReal dist, 
                                 int maxVert,
                                 int *numVert,
                                 MeVector3 *outVert);

int McdPlaneIntersectTest(McdModelID modplane, McdModelID mod, 
                          McdIntersectResult *result);


/****************************************************************************
  This is a collision detection function of any geometry vs. plane.
  This uses McdGeometryInstanceGetSlice and McdGjkMaximumPoint
  to generate contacts.
*/
int MEAPI
McdPlaneIntersect(McdModelPair* p, McdIntersectResult *result);



#ifdef __cplusplus
}
#endif

#endif

