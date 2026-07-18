#ifndef _MCDCONE_H
#define _MCDCONE_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.24.4.3 $

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
    The cone primitive geometry type
*/

#include <McdGeometry.h>
#include <McdGeometryTypes.h>
#include <McdInteractionTable.h>

#ifdef __cplusplus
extern "C" {
#endif

MCD_DECLARE_GEOMETRY_TYPE( McdCone );

MEPUBLIC
McdConeID         MEAPI McdConeCreate(McdFramework *frame,  MeReal length, MeReal lowerRadius);
MEPUBLIC
MeReal            MEAPI McdConeGetLength( McdConeID );
MEPUBLIC
MeReal            MEAPI McdConeGetLowerRadius( McdConeID );
MEPUBLIC
MeReal            MEAPI McdConeGetZOffset( McdConeID );
void              MEAPI McdConeSetGeometricalParameters(McdConeID g,
                                                        MeReal length,
                                                        MeReal lowerRadius,
                                                        MeReal upperRadius) ;
MeReal            MEAPI McdConeGetLowerHeight( McdConeID );
MeReal            MEAPI McdConeGetUpperHeight( McdConeID );

MCD_DECLARE_INTERSECT_INTERACTION(Cone,Plane);
MCD_DECLARE_INTERSECT_INTERACTION(Cone,Sphere);
MCD_DECLARE_INTERSECT_INTERACTION(Cone,Box);
MCD_DECLARE_INTERSECT_INTERACTION(Cone,Cone);
MCD_DECLARE_INTERSECT_INTERACTION(Cone,Cylinder);

MCD_DECLARE_LINESEG_INTERACTION(Cone);

MeBool MEAPI
McdConePrimitivesRegisterInteractions(McdFramework *frame);

#ifdef __cplusplus
}
#endif

#endif /* _MCDCONE_H */
