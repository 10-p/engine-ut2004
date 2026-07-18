#ifndef _MECHUNK_H /* -*- mode: C; -*- */
#define _MECHUNK_H

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/18 14:25:11 $ - Revision: $Revision: 1.14.2.2 $

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
 *  Memory chunk implementation.
 *  This sits on top of MeMemoryAPI, and will call MeMemoryAPI.createAligned
 *  and MeMemoryAPI.destroyAligned as necessary, to provide temporary storage
 *  for an application.
 */

#include <MePrecision.h>


typedef struct MeChunk MeChunk;

/**
 *  Different ways in which temporary memory is handled by MeCHunk.
 *  @see MeChunkSetMode
 */
typedef enum 
{
    /**
     *  If this mode is set, the memory chunk is kept when MeChunkPutMem is 
     *  called. It will then be re-used/expanded as necessary when 
     *  MeChunkGetMem is next called.
     *  This was the old default behaviour for MeChunk auto-resizing.
     */
    kMeChunkModeKeepOnPut = 1,

    /**
     *  If this mode is set, the memory chunk is freed when MeChunkPutMem is
     *  called. It will be re-allocated when MeChunkGetMem is next called.
     */
    kMeChunkModeFreeOnPut = 2
} kMeChunkMode;

struct MeChunk
{
    /** Currently held memory chunk base address. */
    void                *memBase;
    
    /** Size of currently held memory chunk. */
    int                 memSize;
    
    /** Maximum size of chunk needed so far. */
    int                 maxUsed;
    
    /** Byte alignment of memory chunk. */
    int                 alignment;

    /** How this memory chunk is handled. */
    kMeChunkMode        mode;

    /** If the memory looked after by this MeChunk is currently 'in use'. */
    MeBool              isInUse;
};

#ifdef __cplusplus
extern "C"
{
#endif

MEPUBLIC
void    MEAPI MeChunkInit(MeChunk* chunk, int alignment);

MEPUBLIC
void    MEAPI MeChunkTerm(MeChunk* chunk);

MEPUBLIC
void    MEAPI MeChunkSetMode(MeChunk* chunk, kMeChunkMode mode);

MEPUBLIC
void*   MEAPI MeChunkGetMem(MeChunk* chunk, int size);

MEPUBLIC
void    MEAPI MeChunkPutMem(MeChunk* chunk, void* mem);

MEPUBLIC
int     MEAPI MeChunkGetMaxSize(MeChunk* chunk);

MEPUBLIC
int     MEAPI MeChunkGetCurrentSize(MeChunk* chunk);

MEPUBLIC
MeBool  MEAPI MeChunkIsInUse(MeChunk* chunk);

MEPUBLIC
int     MEAPI MeChunkGetAlignment(MeChunk* chunk);

#ifdef __cplusplus
}
#endif

#endif /* _MECHUNK_H */
