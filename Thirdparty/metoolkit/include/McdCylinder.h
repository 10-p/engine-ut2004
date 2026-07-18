#ifndef _MCDCYLINDER_H
#define _MCDCYLINDER_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.22.6.2 $

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
  The cylinder primitive geometry type
*/

#include <McdGeometry.h>
#include <McdGeometryTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/** McdCylinder is a concrete McdGeometry type. */

MCD_DECLARE_GEOMETRY_TYPE( McdCylinder );

MEPUBLIC
void              MEAPI McdCylinderSetGeometricalParameters(McdCylinderID g,
                                                            MeReal inRadius, 
                                                            MeReal inHeight);

MEPUBLIC
McdCylinderID     MEAPI McdCylinderCreate(McdFramework *frame,  MeReal r, MeReal h );
MEPUBLIC
void              MEAPI McdCylinderSetRadius( McdCylinderID g, MeReal r );
MEPUBLIC
void              MEAPI McdCylinderSetHeight( McdCylinderID g, MeReal h );
MEPUBLIC
MeReal            MEAPI McdCylinderGetRadius( McdCylinderID g );
MEPUBLIC
MeReal            MEAPI McdCylinderGetHeight( McdCylinderID g );
MEPUBLIC
MeReal            MEAPI McdCylinderGetHalfHeight( McdCylinderID g );

  /** @internal */
void              MEAPI McdCylinderGetXYAABB( McdGeometry* g, MeMatrix4 tm,
                            MeReal bounds[4]) ;

  /** @internal */
void              MEAPI McdCylinderMaximumPointLocal( McdGeometry *g,
                            MeReal * const inDir, MeReal * const outPoint);

  /** @internal */


#ifdef __cplusplus
}
#endif

#endif /* _MCDCYLINDER_H */
