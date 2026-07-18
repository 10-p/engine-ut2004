#ifndef _MCDTRIANGLELIST_H
#define _MCDTRIANGLELIST_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.25.4.2 $

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
  The user-specified triangle list geometry type
*/

#include <McdGeometry.h>
#include <McdGeometryTypes.h>

#ifdef __cplusplus
extern "C" {
#endif
    
/** McdTriangleList is a concrete McdGeometry type. */
    
MCD_DECLARE_GEOMETRY_TYPE( McdTriangleList );
    

/** Geometry given by user-specified triangle list */

MEPUBLIC
McdTriangleListID MEAPI McdTriangleListCreate(McdFramework *frame, 
                                              MeVector3 min,
                                              MeVector3 max,
                                              int triangleMaxCount,
                                              McdTriangleListFnPtr f);

MEPUBLIC
void              MEAPI McdTriangleListSetMaxTriangles(McdTriangleListID g, int max);

MEPUBLIC
int               MEAPI McdTriangleListGetMaxTriangles(McdTriangleListID g);

MEPUBLIC
void              MEAPI McdTriangleListSetBoundingBox(McdTriangleListID g,
                                                      MeVector3 min,
                                                      MeVector3 max);
MEPUBLIC
void              MEAPI McdTriangleListGetBoundingBox(McdTriangleListID g,
                                                      MeVector3 min,
                                                      MeVector3 max);
MEPUBLIC
void              MEAPI McdTriangleListSetGenerator(McdTriangleListID g,
                                                    McdTriangleListFnPtr f);
MEPUBLIC
McdTriangleListFnPtr MEAPI McdTriangleListGetGenerator(McdTriangleListID g);

MEPUBLIC
void*             MEAPI McdTriangleListGetUserData( McdTriangleListID g );
MEPUBLIC
void              MEAPI McdTriangleListSetUserData( McdTriangleListID g, void* );


#ifdef __cplusplus
}
#endif

#endif /* _MCDTRIANGLELIST_H */
