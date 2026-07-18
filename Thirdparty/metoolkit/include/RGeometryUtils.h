#ifndef _RGEOMETRYUTILS_H
#define _RGEOMETRYUTILS_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

    $Date: 2002/04/04 15:29:39 $ $Revision: 1.8.2.2 $

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

#include <MeViewer.h>
#include <MeASELoad.h>

#ifdef __cplusplus
extern "C" {
#endif

void MEAPI RConvertTriStripToTriList(RGraphic* rg, RObjectVertex* strips,
                                     int* stripSize, int* stripStart, int numStrips);

void MEAPI RSetVertex(RObjectVertex* vertex,
                      MeVector3 vert, MeVector3 norm,
                      MeReal u, MeReal v);


int RCalculateTorusVertexCount(int sides, int rings);
void RCalculateTorusGeometry(RGraphic* rg, AcmeReal outerRadius,
                             AcmeReal innerRadius, int sides, int rings);

int RCalculateFrustumVertexCount(int sides);
void RCalculateFrustumGeometry(RGraphic* rg, AcmeReal bottomRadius,
                               AcmeReal topRadius, AcmeReal bottom,
                               AcmeReal top, int sides);

int RCalculateConeVertexCount(int sides);
void RCalculateConeGeometry(RGraphic* rg, AcmeReal radius,
                               AcmeReal bottom, AcmeReal top, int sides);

int RCalculateSphereVertexCount(int sides, int rings);
void RCalculateSphereGeometry(RGraphic* rg, AcmeReal radius,
                              int sides, int rings);

int RCalculateBoxVertexCount();
void RCalculateBoxGeometry(RGraphic* rg, AcmeReal lx,
                           AcmeReal ly, AcmeReal lz);

int RCalculateASEVertexCount(MeASEObject* object, MeBool noMCD);
void RLoadASEGeometry(RGraphic* rg, MeASEObject* object, MeBool noMCD);

int RCalculateDomeVertexCount(int sides, int rings);
void RCalculateDomeGeometry(RGraphic* rg, AcmeReal radius,
                            int sides, int rings,
                            int tileU, int tileV);

int RCalculateSphylVertexCount(int sides, int rings);
void RCalculateSphylGeometry(RGraphic* rg, AcmeReal radius, AcmeReal height,
                              int sides, int rings);

#ifdef __cplusplus
}
#endif

#endif /* Sentry */
