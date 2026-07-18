#ifndef _MCDINTERACTIONS_H
#define _MCDINTERACTIONS_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.22.6.1 $

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

    Queries on the geometrical relationship between a pair of McdModel
    objects. The principal query is McdIntersect(), which computes contacts.

    All pairwise queries are performed using McdModelPair objects.  The
    interactions rely on a retained-mode mechanism: Before a McdModelPair
    object is used in any of the interaction functions, you must prepare it
    by calling McdHello(). After that point, the object can be used on any
    available interaction functions, over multiple time steps in the
    simulation. When the McdModelPair object is no longer needed ( for
    example the pair of models involved are no longer in close proximity ),
    McdGoodbye() must be called on it.

    The continuity of McdModelPair identity, and keeping track of
    hello/goodbye events are managed automatically when you obtain your
    McdModelPair objects using McdSpace.

    The actual underlying algorithms depend on the actual geometry types of
    each McdModel object in the pair.
    The retained-mode mechanism  is exploited by
    some of these algorithms, which may employ
    coherence-based techniques, or perform pairwise pre-processing
    operations when a given pair is first encountered.

    Both the geometry types and the
    interaction algorithms must be explicitly registered with the Mcd
    system at initialization. If, for a particular geometry type - geometry
    type combination, there is no algorithm registered or available, the
    interaction will simply be ignored.

*/

#include <McdCTypes.h>

#ifdef __cplusplus
extern "C" {
#endif


MEPUBLIC
MeBool             MEAPI McdNearby( McdModelPair* );

MEPUBLIC
void               MEAPI McdHello( McdModelPair* );
MEPUBLIC
void               MEAPI McdGoodbye( McdModelPair* );


MEPUBLIC
MeBool             MEAPI McdIntersect( McdModelPair*, McdIntersectResult* );

MEPUBLIC
MeBool             MEAPI McdIntersectAt( McdModelPair *p, McdIntersectResult *result, MeReal time );

MEPUBLIC
MeBool             MEAPI McdSafeTime( McdModelPair*, MeReal maxTime, McdSafeTimeResult* );

MEPUBLIC
void               MEAPI McdHelloEach( McdModelPairContainer* );

MEPUBLIC
void               MEAPI McdGoodbyeEach( McdModelPairContainer* );

MEPUBLIC
MeBool             MEAPI McdIntersectEach( McdModelPairContainer* pairs,
                                           McdModelPairContainerIterator* pairsIter,
                                           McdIntersectResult* resultArray, 
                                           int resultArraySize,
                                           int *resultCount,
                                           McdContact* contactArray, 
                                           int contactArraySize,
                                           int *contactCount );

/** Intersect a linesegment with a collision model */
MEPUBLIC
unsigned int       MEAPI McdLineSegIntersect( const McdModelID cm,
                                               MeReal* const inOrig, MeReal* const inDest,
                                               McdLineSegIntersectResult *outOverlap);

#ifdef __cplusplus
}
#endif


#endif /* _MCDINTERACTIONS_H */
