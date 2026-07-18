#ifndef _MCDMODELPAIRMANAGER_H
#define _MCDMODELPAIRMANAGER_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.2.2.1 $

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

The McdModelPairManager object. The pair manager is responsible for maintaining the
life cycle of pairs. Pairs are deactivated and activated, and automatically
transferred between their hello, goodbye, staying, and inactive states. An
iterator is provided to iterate over the pairs which are not in the inactive
state.
*/

#include <McdCTypes.h>
#include <McdSpace.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct McdModelPairManagerLink
{
    struct McdModelPairManagerLink *phaseNext;
    struct McdModelPairManagerLink *phasePrev;
    int key;
    McdModelPairID pair;
    struct McdModelPairManagerLink *next;    
} McdModelPairManagerLink;

typedef struct McdModelPairManagerHash
{
    McdModelPairManagerLink **bucket;
    int size, bucketCount;
} McdModelPairManagerHash;

typedef struct
{
    McdModelPairManagerLink helloList,goodbyeList;
    McdModelPairManagerHash *hash;
    MePool *pairPool;
    MePool *linkPool;
    McdSpacePoolErrorFnPtr poolFullHandler;
} McdModelPairManager;



typedef McdModelPairManager* McdModelPairManagerID;

/**
Create a pair manager with capacity for @arg pairs
*/


MEPUBLIC 
McdModelPairManagerID  MEAPI McdModelPairManagerCreate(int size);

/**
Destroy a pair manager
*/

MEPUBLIC
void              MEAPI McdModelPairManagerDestroy(McdModelPairManager* manager);


/**
  Activate a pair of models. Returns zero if the pair was not active.
*/

MEPUBLIC
MeBool            MEAPI McdModelPairManagerActivate(McdModelPairManagerID manager, 
                                                    McdModelID model1, 
                                                    McdModelID model2);

/**
  Deactivate a pair of models. Returns zero if the pair was not active.
*/

MEPUBLIC
MeBool            MEAPI McdModelPairManagerDeactivate(McdModelPairManagerID manager, 
                                                      McdModelID model1, 
                                                      McdModelID model2);

/**
  Get the pair corresponding to a pair of models. Returns zero if the pair does not exist.
*/
MEPUBLIC
McdModelPairID    MEAPI McdModelPairManagerGetPair(McdModelPairManagerID manager, 
                                                   McdModelID m1, 
                                                   McdModelID m2);

/**
  Flush the pairs: all Hello pairs become staying pairs, and all goodbye pairs become inactive
*/
MEPUBLIC
void              MEAPI McdModelPairManagerFlush(McdModelPairManagerID manager);


/**
  Reset the iterator for getting the list of pairs
*/

MEPUBLIC          
void              MEAPI McdModelPairManagerResetIterator(McdModelPairManagerID manager,                                                   
                                                         McdSpacePairIterator *iterator);

/**
  Get the set of transition pairs, i.e. the Hello and Goodbye pairs. Returns non-zero if
  there are more pairs to get
*/
MEPUBLIC          
MeBool            MEAPI McdModelPairManagerGetTransitions(McdModelPairManagerID manager,                                                        
                                                          McdSpacePairIterator *iterator,
                                                          McdModelPairContainer *container);

/**
  Get the list of pairs which are Hello, Goodbye, or Staying. returns non-zero if there are
  more pairs to get,
*/
MEPUBLIC          
MeBool            MEAPI McdModelPairManagerGetPairs(McdModelPairManagerID manager,                                             
                                                    McdSpacePairIterator *iterator,
                                                    McdModelPairContainer *container);

/**
  Returns the number of pairs this pair manager is dealing with
*/
MEPUBLIC
int               MEAPI McdModelPairManagerGetSize(McdModelPairManagerID manager);

MEPUBLIC
void              MEAPI McdModelPairManagerSetPoolFullHandler(McdModelPairManagerID manager, 
                                                              McdSpacePoolErrorFnPtr handler);

MEPUBLIC
McdSpacePoolErrorFnPtr  
                  MEAPI McdModelPairManagerGetPoolFullHandler(McdModelPairManagerID manager);


#ifdef __cplusplus
}
#endif

#endif /* _MCDMODELPAIRMANAGER_H */
