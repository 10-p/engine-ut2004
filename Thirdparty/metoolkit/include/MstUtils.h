#ifndef _MSTUTILS_H
#define _MSTUTILS_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/08 11:31:42 $ - Revision: $Revision: 1.27.8.2 $

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

/** @file
 * Mst stand-alone utility API.
 */

#include <Mst.h>

#ifdef __cplusplus
extern "C" {
#endif

MEPUBLIC
void          MEAPI MstAutoSetMassProperties(const MdtBodyID body,
                        const McdModelID model, const MeReal density);

MEPUBLIC
void		  MEAPI MstAutoSetInertialTensor(const McdModelID model);


MEPUBLIC
void          MEAPI MstHandleCollisions(McdModelPairContainer* pairs,
                        const McdSpaceID s,
                        const MdtWorldID w,
                        const MstBridgeID b);
MEPUBLIC
void          MEAPI MstHandleTransitions(McdModelPairContainer* pairs,
                        const McdSpaceID s,
                        const MdtWorldID w,
                        const MstBridgeID b);

MEPUBLIC
void          MEAPI MstSetWorldHandlers(const MdtWorldID world);

MEPUBLIC
McdModelID    MEAPI MstModelAndBodyCreate(const MstUniverseID u,
                              const McdGeometryID g, const MeReal density);
MEPUBLIC
void          MEAPI MstModelAndBodyDestroy(const McdModelID m);

MEPUBLIC
McdModelID    MEAPI MstFixedModelCreate(const MstUniverseID u,
                                        const McdGeometryID g,
                                        MeMatrix4Ptr transformation);

MEPUBLIC
void          MEAPI MstFixedModelDestroy(const McdModelID m);

#ifdef __cplusplus
}
#endif


#endif
