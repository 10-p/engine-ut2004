#ifndef _MCDPLANE_H
#define _MCDPLANE_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.15.6.1 $

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
  The plane primitive geometry type
*/

#include <McdGeometry.h>
#include <McdGeometryTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

  /** McdPlane is a concrete McdGeometry type. */

MCD_DECLARE_GEOMETRY_TYPE( McdPlane );

MEPUBLIC
McdPlaneID        MEAPI McdPlaneCreate(McdFramework *frame);
MEPUBLIC
void              MEAPI McdPlaneGetNormal( const McdGeometry *g,
                                           const MeMatrix4 tm,
                                           MeVector3 normal );
MEPUBLIC
MeReal            MEAPI McdPlaneGetDistanceToPoint( const McdGeometry *g,
                                                    const MeMatrix4 tm, 
                                                    const MeVector3 point);

#ifdef __cplusplus
}
#endif

#endif /* _MCDPLANE_H */
