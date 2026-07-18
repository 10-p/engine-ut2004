/*
 * Dictionary Abstract Data Type
 * Copyright (C) 1997 Kaz Kylheku <kaz@ashi.footprints.net>
 *
 * Free Software License:
 *
 * All rights are reserved by the author, with the following exceptions:
 * Permission is granted to freely reproduce and distribute this software,
 * possibly in exchange for a fee, provided that this copyright notice appears
 * intact. Permission is also granted to adapt this software to produce
 * derivative works, as long as the modified versions carry this copyright
 * notice and additional notices stating that the work has been modified.
 * This source code may be translated into executable form and incorporated
 * into proprietary software; there is no requirement for such software to
 * contain a copyright notice related to this source.
 *
 * $Id: MeDict.h,v 1.4.8.1 2002/03/08 15:14:22 jamesrf Exp $
 * $Name: t-stevet-RWSpre-030110 $
 */

/* 
 * NOTE: This work is derived by MathEngine from Kazlib 
 * Pretty much only function names have been modified from the original
 */

#ifndef DICT_H
#define DICT_H

#include <limits.h>
#ifdef KAZLIB_SIDEEFFECT_DEBUG
#include "sfx.h"
#endif


/** @file
 * Dictionary implementation.
 */

/*
 * Blurb for inclusion into C++ translation units
 */

#ifdef __cplusplus
extern "C" {
#endif

#define DICTCOUNT_T_MAX ULONG_MAX

/*
 * The dictionary is implemented as a red-black tree
 */

typedef enum { kMeDictColorRed, kMeDictColorBlack } MeDictColor;

typedef struct MeDictNode {
    #if defined(DICT_IMPLEMENTATION) || !defined(KAZLIB_OPAQUE_DEBUG)
    struct MeDictNode *left;
    struct MeDictNode *right;
    struct MeDictNode *parent;
    MeDictColor color;
    const void *key;
    void *data;
    #else
    int dummy;
    #endif
} MeDictNode;

typedef int (*MeDictCompareFn)(const void *, const void *);
typedef MeDictNode *(*MeDictAllocFn)(void *);
typedef void (*MeDictFreeFn)(MeDictNode *, void *);

typedef struct MeDict {
    #if defined(DICT_IMPLEMENTATION) || !defined(KAZLIB_OPAQUE_DEBUG)
    MeDictNode nilnode;
    unsigned long nodecount;
    unsigned long maxcount;
    MeDictCompareFn compare;
    MeDictAllocFn allocnode;
    MeDictFreeFn freenode;
    void *context;
    int dupes;
    #else
    int dummmy;
    #endif
} MeDict;

typedef void (*MeDictProcessFn)(MeDict *, MeDictNode *, void *);

typedef struct MeDictLoad {
    #if defined(DICT_IMPLEMENTATION) || !defined(KAZLIB_OPAQUE_DEBUG)
    MeDict *dictptr;
    MeDictNode nilnode;
    #else
    int dummmy;
    #endif
} MeDictLoad;

extern MeDict *MeDictCreate(unsigned long, MeDictCompareFn);
extern void MeDictSetAllocator(MeDict *, MeDictAllocFn, MeDictFreeFn, void *);
extern void MeDictDestroy(MeDict *);
extern void MeDictFreeNodes(MeDict *);
extern void MeDictFree(MeDict *);
extern MeDict *MeDictInit(MeDict *, unsigned long, MeDictCompareFn);
extern void MeDictInitLike(MeDict *, const MeDict *);
extern int MeDictVerify(MeDict *);
extern int MeDictSimilar(const MeDict *, const MeDict *);
extern MeDictNode *MeDictLookup(MeDict *, const void *);
extern MeDictNode *MeDictLowerBound(MeDict *, const void *);
extern MeDictNode *MeDictUpperBound(MeDict *, const void *);
extern void MeDictInsert(MeDict *, MeDictNode *, const void *);
extern MeDictNode *MeDictDelete(MeDict *, MeDictNode *);
extern int MeDictAllocInsert(MeDict *, const void *, void *);
extern void MeDictDeleteFree(MeDict *, MeDictNode *);
extern MeDictNode *MeDictFirst(MeDict *);
extern MeDictNode *MeDictLast(MeDict *);
extern MeDictNode *MeDictNext(MeDict *, MeDictNode *);
extern MeDictNode *MeDictPrev(MeDict *, MeDictNode *);
extern unsigned long MeDictCount(MeDict *);
extern int MeDictIsEmpty(MeDict *);
extern int MeDictIsFull(MeDict *);
extern int MeDictContains(MeDict *, MeDictNode *);
extern void MeDictAllowDupes(MeDict *);

extern int MeDictNodeIsInADict(MeDictNode *);
extern MeDictNode *MeDictNodeCreate(void *);
extern MeDictNode *MeDictNodeInit(MeDictNode *, void *);
extern void MeDictNodeDestroy(MeDictNode *);
extern void *MeDictNodeGet(MeDictNode *);
extern const void *MeDictNodeGetKey(MeDictNode *);
extern void MeDictNodePut(MeDictNode *, void *);

extern void MeDictProcess(MeDict *, void *, MeDictProcessFn);
extern void MeDictLoadBegin(MeDictLoad *, MeDict *);
extern void MeDictLoadNext(MeDictLoad *, MeDictNode *, const void *);
extern void MeDictLoadEnd(MeDictLoad *);
extern void MeDictMerge(MeDict *, MeDict *);

#if defined(DICT_IMPLEMENTATION) || !defined(KAZLIB_OPAQUE_DEBUG)
#ifdef KAZLIB_SIDEEFFECT_DEBUG
#define MeDictIsFull(D) (SFX_CHECK(D)->nodecount == (D)->maxcount)
#else
#define MeDictIsFull(D) ((D)->nodecount == (D)->maxcount)
#endif
#define MeDictCount(D) ((D)->nodecount)
#define MeDictIsEmpty(D) ((D)->nodecount == 0)
#define MeDictNodeGet(N) ((N)->data)
#define MeDictNodeGetKey(N) ((N)->key)
#define MeDictNodePut(N, X) ((N)->data = (X))
#endif

#ifdef __cplusplus
}
#endif

#endif
