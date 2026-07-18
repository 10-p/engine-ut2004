#ifndef _MCDMODELPAIRCONTAINER_H
#define _MCDMODELPAIRCONTAINER_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.25.6.1 $

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

    A container for McdModelPair objects.

    Intended to indicate pairs of McdModel objects that are in close
    proximity to each other.

    Three distinct states are identified: "hello", "staying" and "goodbye".
    When a pair of models are first detected to be in close proximity, the
    pair is in the "hello" state. When the pair continues to be in close
    proximity over successive time steps, the pair is in the "staying"
    state, and when the pair is first detected to be no longer in close
    proximity, it is a "goodbye" pair, After this point, the pair is no longer
    valid.


    The McdSpace module will respect these semantics and manage the state
    transitions for you. McdSpaceGetPairs() will fill in a
    McdModelPairContainer appropriately.

    This container is a compact way of passing on the results of McdSpace
    to various response modules, such as MathEngine's Simulation Toolkit.

    Whether it is McdSpace or a module that you have written yourself that
    fills in a McdModelPairContainer, the categorization of the three
    states is essential for efficient use of the interaction functions such
    as McdIntersect or McdSafeTime:
    The "hello" and "goodbye" states match the McdHello, McdGoodbye
    calls that prepare a McdModelPair object for use.

    @see McdModelPair, McdSpaceGetPairs, McdHello, McdGoodbye, McdIntersect

 */

#include <McdCTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

MEPUBLIC
void              MEAPI McdModelPairContainerInit(
                            McdModelPairContainer*,
                            McdModelPair **array, int size );

MEPUBLIC
McdModelPairContainer* MEAPI McdModelPairContainerCreate( int size );
MEPUBLIC
void              MEAPI McdModelPairContainerDestroy(
                            McdModelPairContainer* a );

MEPUBLIC
void              MEAPI McdModelPairContainerReset( McdModelPairContainer* );
MEPUBLIC
int               MEAPI McdModelPairContainerGetGoodbyeCount(
                            McdModelPairContainer* );

MEPUBLIC
int               MEAPI McdModelPairContainerGetHelloCount(
                            McdModelPairContainer* );

MEPUBLIC
int               MEAPI McdModelPairContainerGetStayingCount(
                            McdModelPairContainer* );

MEPUBLIC
McdModelPair**    MEAPI McdModelPairContainerGetGoodbyeArray(
                            McdModelPairContainer*, int *count );
MEPUBLIC
McdModelPair**    MEAPI McdModelPairContainerGetHelloArray(
                            McdModelPairContainer*, int *count );
MEPUBLIC
McdModelPair**    MEAPI McdModelPairContainerGetStayingArray(
                            McdModelPairContainer*, int *count );

MeBool            MEAPI McdModelPairContainerInsertHelloPair(McdModelPairContainer *m, 
                                                             McdModelPairID p);
MeBool            MEAPI McdModelPairContainerInsertGoodbyePair(McdModelPairContainer *m, 
                                                               McdModelPairID p);
MeBool            MEAPI McdModelPairContainerInsertStayingPair(McdModelPairContainer *m, 
                                                               McdModelPairID p);
void              MEAPI McdModelPairContainerRemovePair(McdModelPairContainer *m, 
                                                        McdModelPairID *p);


#ifdef MCDCHECK
void              MEAPI McdModelPairContainerPrintStats(
                            McdModelPairContainer* );
#endif


MEPUBLIC
void              MEAPI McdModelPairContainerIteratorInit(
                  McdModelPairContainerIterator*,
                            McdModelPairContainer* );

#ifdef MCDCHECK
  /** @internal */
void              MEAPI McdPairContainerPrintStats( McdModelPairContainer* );
#endif

#ifdef __cplusplus
}
#endif

#endif /* _MCDMODELPAIRCONTAINER_H */
