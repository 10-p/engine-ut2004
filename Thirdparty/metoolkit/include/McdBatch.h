#ifndef _MCDBATCH_H
#define _MCDBATCH_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.5.2.1 $

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
  Geometric contact point information
*/

#include <McdCTypes.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
    kMcdBatchPairFlattened = 1,
    kMcdBatchPairTested = 2,
    kMcdBatchPairUnflattened = 3,

    kMcdBatchPairFrozen = 8,
    kMcdBatchPairAggregate = 16
} McdBatchPairState;

typedef struct McdBatchPairData McdBatchPairData;

typedef struct McdBatchGeometryData
{
    MeVector3Ptr min, max;
    MeReal eps;
    MeMatrix4Ptr tm;
    McdGeometryID geometry;
    MeU16 type;
} McdBatchGeometryData;

typedef struct McdBatchEntry
{
    /* input to collision function */

    McdBatchGeometryData geometryData1,geometryData2;
    
    /* output from collision function */

    MeVector3 normal;
    MeBool touch;
    int contactCount;
    McdContact *contacts;

    /* admin stuff */

    McdGeometryInstance *ins1, *ins2;
    int pool;
    int flags;
    struct McdBatchEntry *next;    /* next in this bucket */
    McdBatchPairData *pairData;
} McdBatchEntry;

typedef struct 
{
    McdContact *contacts;
    int contactCount;
    int contactMaxCount;
} McdBatchContactPool;


/* the overall context for flattening which keeps track of all the state */

struct McdBatchPairData
{
    McdModelPairID pair;
    McdBatchEntry *start;
    int entries;
    int done;
    int status;
};


typedef struct McdBatchContext
{
    int state; 

    McdBatchEntry **table;
    McdFramework *frame;
    int typeCount;

    MeMatrix4 *tmArray;
    MeMatrix4Ptr **tmTrack;
    int tmMaxCount;
    int nextTM;

    McdBatchEntry *entryArray;
    int maxEntryCount;
    int nextEntry;

    McdBatchPairData *pairData;
    int pairDataMaxCount;
    int nextPairData;

    McdBatchContactPool *pools;
    int nextPool;
    int poolMaxCount;

    /* flattening state in case we need to bail out */

    int nextFlattenPair;

    /* unflattening state in case we need to bail out */
    int nextSingleUnflattenPair;
    int nextAggregateUnflattenPair;

} McdBatchContext;

McdBatchContext *
MEAPI McdBatchContextCreate(McdFramework *frame);

void MEAPI 
McdBatchContextReset(McdBatchContext *context);

void MEAPI 
McdBatchContextDestroy(McdBatchContext *context);

MeBool MEAPI
McdBatchIntersectEach(McdBatchContext *context,
                      McdModelPairContainer *pairs,
                      McdIntersectResult* resultArray, 
                      int *resultCount,
                      int resultMaxCount,
                      McdContact* contactArray,
                      int *contactCount,
                      int contactMaxCount);

void MEAPI McdBatchContextRelease(McdBatchContext *context);

#ifdef __cplusplus
}
#endif

#endif /* _MCDCONTACT_H */
