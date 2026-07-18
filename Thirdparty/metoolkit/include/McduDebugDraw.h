#ifndef _MCDUDEBUGDRAW_H
#define _MCDUDEBUGDRAW_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.4.4.1 $

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
    Debug draw functions
*/

#include <McdCTypes.h>
#include <McdGeometryTypes.h>


#ifdef __cplusplus
extern "C" {
#endif

typedef enum 
{
    kMcduDebugDrawAABB = 1,
    kMcduDebugDrawDetail = 2,
    kMcduDebugDrawEnabledOnly = 4
} McduDebugDrawFlags;

void MEAPI McduDebugDrawSphere(const McdSphereID sphere, const MeMatrix4 tm, const MeReal colour[3]);
void MEAPI McduDebugDrawBox(const McdBoxID box, const MeMatrix4 tm, const MeReal colour[3]);
void MEAPI McduDebugDrawCylinder(const McdCylinderID cyl, const MeMatrix4 tm, const MeReal colour[3]);
void MEAPI McduDebugDrawConvex(const McdConvexMeshID convex, const MeMatrix4 tm, const MeReal colour[3]);
void MEAPI McduDebugDrawAggregate(const McdAggregateID agg, const MeMatrix4 tm, const MeReal colour[3]);

void MEAPI McduDebugDrawAABB(const MeVector3 min, const MeVector3 max, const MeReal colour[3]);
void MEAPI McduDebugDrawGeometry(const McdGeometryID geometry, const MeMatrix4Ptr tm, const MeReal colour[3]);
void MEAPI McduDebugDrawModel(const McdModelID model, McduDebugDrawFlags flags, const MeReal colour[3]);
void MEAPI McduDebugDrawSpace(const McdSpaceID space, McduDebugDrawFlags flags, const MeReal colour[3]);

#ifdef __cplusplus
}
#endif


#endif /* _MCDUDEBUGDRAW_H */
