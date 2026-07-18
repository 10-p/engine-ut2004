#ifndef _MSTBRIDGE_H
#define _MSTBRIDGE_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:40 $ - Revision: $Revision: 1.15.2.1 $

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
 * MathEngine Collision <-> Dynamics Toolkit Bridge API.
 * This is used by MstUniverseStep to set the physical parameters of
 * dynamics contacts (such as friction, restitution etc.) based on the
 * materials of the two geometries in contats. It also holds an optional
 * callback for each MstMaterialID pair.
 */


#include <MstTypes.h>

#define MstBridgeGetDefaultMaterial() (0)


#ifdef __cplusplus
extern "C" {
#endif

MEPUBLIC
MstBridgeID           MEAPI MstBridgeCreate(McdFrameworkID frame,
                                            const unsigned int maxMaterials);

MEPUBLIC
void                  MEAPI MstBridgeDestroy(const MstBridgeID b);


MEPUBLIC
void                  MEAPI MstBridgeStepAll(const MstBridgeID bridge,
                                const MeReal stepSize);
MEPUBLIC
void                  MEAPI MstBridgeUpdateContacts(const MstBridgeID b,
                                                    const McdSpaceID s,
                                                    const MdtWorldID w);
MEPUBLIC
void                  MEAPI MstBridgeUpdateTransitions(const MstBridgeID b,
                                                       const McdSpaceID s,
                                                       const MdtWorldID w);

MEPUBLIC
MdtContactParamsID    MEAPI MstBridgeGetContactParams(const MstBridgeID b,
                                const MstMaterialID m1,
                                const MstMaterialID m2);
MEPUBLIC
MstPerPairCBPtr       MEAPI MstBridgeGetPerPairCB(const MstBridgeID b,
                                const MstMaterialID m1,
                                const MstMaterialID m2);
MEPUBLIC
MstPerContactCBPtr    MEAPI MstBridgeGetPerContactCB(const MstBridgeID b,
                                const MstMaterialID m1,
                                const MstMaterialID m2);
MEPUBLIC
MstMaterialID         MEAPI MstBridgeGetNewMaterial(const MstBridgeID b);
MEPUBLIC
MstIntersectCBPtr     MEAPI MstBridgeGetIntersectCB(const MstBridgeID b,
                                const MstMaterialID m1,
                                const MstMaterialID m2);

MEPUBLIC
void                  MEAPI MstBridgeSetPerPairCB(const MstBridgeID b,
                                const MstMaterialID m1,
                                const MstMaterialID m2,
                                const MstPerPairCBPtr cb);
MEPUBLIC
void                  MEAPI MstBridgeSetPerContactCB(const MstBridgeID b,
                                const MstMaterialID m1,
                                const MstMaterialID m2,
                                const MstPerContactCBPtr cb);
MEPUBLIC
void                  MEAPI MstBridgeSetIntersectCB(const MstBridgeID b,
                                const MstMaterialID m1,
                                const MstMaterialID m2,
                                const MstIntersectCBPtr cb);
MEPUBLIC
void                  MEAPI MstBridgeSetModelPairBufferSize(const MstBridgeID b,
                                const unsigned int s);
MEPUBLIC
void                  MEAPI MstBridgeSetContactBufferSize(const MstBridgeID b,
                                const unsigned int s);

#ifdef __cplusplus
}
#endif

#endif
