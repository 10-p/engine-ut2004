#ifndef _MEPOOL_H
#define _MEPOOL_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:06 $ - Revision: $Revision: 1.17.6.2 $

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
 *  Memory pool implementation.
 */

#include <MePrecision.h>

#ifdef __cplusplus
extern "C"
{
#endif

enum MePoolType { MePoolNULL, MePoolFIXED, MePoolMALLOC };

/**
 * Simple fixed-size pool (used for bodies etc.)
 */
struct MePoolFixed
{
    /** Actual chunk of memory holding structs. */
    void                *structArray;

    /** Stack of pointers to free structs. */
    void                **freeStructStack;

    /** Index of next free pointer in stack. */
    int                 nextFreeStruct;

    /** Size of each struct in the pool. */
    int                 structSize;

    /** Total number of structs in pool. */
    int                 poolSize;

    /**
     * Indicates if the pool memory was allocated with
     * createAligned or create.
     */
    MeBool              createdAligned;
};

struct MePoolMalloc
{
    /** Number of allocated structs */
    int                 usedStructs;

    /** Size of each struct in the pool. */
    int                 structSize;

    /** Total number of structs in pool. */
    int                 poolSize;

    /**
     * Indicates requested alignment, if != 0.
     */
    int                 alignment;
};

struct MePool
{
    enum MePoolType     t;

    union
    {
        struct MePoolFixed      fixed;
        struct MePoolMalloc     malloc;
    }
                        u;
};

typedef struct MePool MePool;

struct MePoolAPI
{
    void            (MEAPI *init)(MePool* pool,
                            int poolSize, int structSize, int alignment);
    void            (MEAPI *destroy)(MePool* pool);
    void            (MEAPI *deset)(MePool* pool);
    void*           (MEAPI *getStruct)(MePool* pool);
    void            (MEAPI *putStruct)(MePool* pool, void* s);
    int             (MEAPI *getUsed)(MePool* pool);
    int             (MEAPI *getUnused)(MePool* pool);
};

extern struct MePoolAPI MePoolAPI;
extern struct MePoolAPI MePoolFixedAPI;
extern struct MePoolAPI MePoolMallocAPI;

#ifdef __cplusplus
}
#endif

#endif
