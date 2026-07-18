#ifndef _MEHASH_H
#define _MEHASH_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/05/17 14:44:57 $ - Revision: $Revision: 1.20.2.2.4.1 $

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
 *  Generic hash table implementation.
 */

#include <MeCall.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef int  (MEAPI *MeHashFunc)(const void *key,int size);
typedef int  (MEAPI *MeHashCompareFunc)(const void *key1,const void *key2);
typedef void (MEAPI *MeHashFreeFunc)(void *const);

typedef struct MeHash           MeHash;
typedef struct MeHashBucket     MeHashBucket;
typedef struct MeHashIterator   MeHashIterator;

struct MeHashBucket
{
    void *key;
    void *datum;
    MeHashBucket *next;
};

struct MeHash
{
    int size;
    int population;
    MeHashFunc hash;
    MeHashCompareFunc compare;
    MeHashFreeFunc freeKey;
    MeHashFreeFunc freeDatum;
    MeHashBucket **table;
    unsigned long collisions;
};

struct MeHashIterator
{
    MeHash *table;
    MeHashBucket *bucket;
    int index;
};

MEPUBLIC
MeHash *          MEAPI MeHashCreate(int size);
MEPUBLIC
void              MEAPI MeHashDestroy(MeHash *table);
MEPUBLIC
void              MEAPI MeHashReset(MeHash *table);

MEPUBLIC
void *            MEAPI MeHashInsert(const void *key,const void *datum,MeHash *table);
MEPUBLIC
void *            MEAPI MeHashDelete(const void *key,MeHash *table);

MEPUBLIC
void *            MEAPI MeHashLookup(const void *key,const MeHash *table);

MEPUBLIC
void              MEAPI MeHashSetHashFunc(MeHash *table,MeHashFunc hash);
MEPUBLIC
void              MEAPI MeHashSetKeyCompareFunc(MeHash *table,
                                                MeHashCompareFunc compare);
MEPUBLIC
int               MEAPI MeHashString(const void *i, int size);
MEPUBLIC
int               MEAPI MeHashInt(const void *i, int size);
MEPUBLIC
int               MEAPI MeHashStringCompare(const void *k1, const void *k2);
MEPUBLIC
int               MEAPI MeHashIntCompare(const void *k1, const void *k2);

MEPUBLIC
void              MEAPI MeHashSetKeyFreeFunc(MeHash *table,MeHashFreeFunc keyFree);
MEPUBLIC
void              MEAPI MeHashSetDatumFreeFunc(MeHash *table,MeHashFreeFunc datumFree);

MEPUBLIC
int               MEAPI MeHashPopulation(const MeHash *table);

MEPUBLIC
MeHashIterator *  MEAPI MeHashInitIterator(MeHashIterator *i,
                            const MeHash *table);

MEPUBLIC
void *            MEAPI MeHashGetDatum(MeHashIterator *i);


#ifdef __cplusplus
}
#endif

#endif /* _MEHASH_H */
