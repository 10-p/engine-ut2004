#ifndef _MSTUNIVERSE_H
#define _MSTUNIVERSE_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:41 $ - Revision: $Revision: 1.34.8.1 $

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
 * MstUniverse simulation 'container' API.
 */

#include <Mst.h>

#ifdef __cplusplus
extern "C" {
#endif

MEPUBLIC
MstUniverseID         MEAPI MstUniverseCreate(
                                const MstUniverseSizes * const sizes);
MEPUBLIC
void                  MEAPI MstUniverseDestroy(const MstUniverseID u);
MEPUBLIC
void                  MEAPI MstUniverseStep(const MstUniverseID u,
                                const MeReal step);

MEPUBLIC
MdtWorldID            MEAPI MstUniverseGetWorld(const MstUniverseID u);
MEPUBLIC
McdSpaceID            MEAPI MstUniverseGetSpace(const MstUniverseID u);
MEPUBLIC
MstBridgeID           MEAPI MstUniverseGetBridge(const MstUniverseID u);
MEPUBLIC
McdFrameworkID		  MEAPI MstUniverseGetFramework(const MstUniverseID u);


#ifdef __cplusplus
}
#endif

#endif
