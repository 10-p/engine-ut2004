#ifndef _MEIDPOOL_H
#define _MEIDPOOL_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:06 $ - Revision: $Revision: 1.5.2.1 $

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
 *  A bitfield-based unique, auto-resizing integer ID pool.
 */

#include <MeCall.h>
#include <MePrecision.h>

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct MeIDPool MeIDPool;

struct MeIDPool
{
    MeU32           *IDbitfield;
    unsigned int    block;
    unsigned int    maxBlocks;
    unsigned int    maxIDs;
    unsigned int    assignedIDs;
};

MEPUBLIC
MeIDPool       *MEAPI MeIDPoolCreate();
MEPUBLIC
void            MEAPI MeIDPoolCopy(MeIDPool *to, MeIDPool *from);
MEPUBLIC
MeBool          MEAPI MeIDPoolIsEmpty(MeIDPool *pool);
MEPUBLIC
int             MEAPI MeIDPoolRequestID(MeIDPool *pool);
MEPUBLIC
void            MEAPI MeIDPoolReturnID(MeIDPool *pool, int id);
MEPUBLIC
void            MEAPI MeIDPoolReset(MeIDPool *pool);
MEPUBLIC
void            MEAPI MeIDPoolDestroy(MeIDPool *pool);

#ifdef __cplusplus
}
#endif

#endif /* _MEIDPOOL_H */

