#ifndef _MCDSPACE_H
#define _MCDSPACE_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.45.2.2 $

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

    Manages the large-scale spatial properties of a region of 3D space
    populated by McdModel objects.
    Its principal role is to keep track of proximities between many McdModel objects.

    There are two types of operations on McdSpace objects: state-query and
    state-modification. State-query functions can only be used when the
    state is well-defined, i.e. not in the process of being modified.
    Once modifications to the state begin to be applied (signalled by a call to
    McdSpaceBeginChanges()) , the original state is
    no longer available for query. When the set of modifications have been
    completed, (indicated by McdSpaceEndChanges()) the new state is
    properly defined and ready to be queried again. The current mode is
    indicated by McdSpaceIsChanging().

    Pairwise proximities are reported using a retained-mode mechanism that
    maintains identity of a given pair over successive states:
    When pairs of McdModel objects are first detected to be in close
    proximity, a McdModelPair object is assigned to them.
    In subsequent steps, the same McdModelPair
    object will be re-used to refer to the same pair.
    When the pair is no longer in proximity, the McdModelPair object is
    recycled as needed for use as a reference to a differnt pair.

    The continuity of McdModelPair identity is exploited by many of the Mcd
    interaction algorithms, and also by response modules such as
    MathEngine's Dynamics Toolkit and Simulation Toolkit.

*/

#include <McdCTypes.h>
#include <McdCullingTable.h>

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------
 *  McdSpace
 *----------------------------------------------------------------
 */

extern int McdXAxis;    /**< X-axis to be or'ed with McdYAxis and McdZAxis */
extern int McdYAxis;    /**< Y-axis to be or'ed with McdXAxis and McdZAxis */
extern int McdZAxis;    /**< Z-axis to be or'ed with McdXAxis and McdYAxis */
extern int McdAllAxes;  /**< A constant equal to McdXAxis+McdYAxis+McdZAxis.*/

typedef void (MEAPI *McdSpaceUpdateAABBFnPtr)(McdModelID m, MeReal duration);

MEPUBLIC
McdSpaceID        MEAPI McdSpaceAxisSortCreate(McdFrameworkID fwk,
                                               int axes, 
                                               int objectCount, 
                                               int pairCount);

MEPUBLIC
void              MEAPI McdSpaceDestroy(McdSpaceID s);

MEPUBLIC
void              MEAPI McdSpaceBuild(McdSpaceID s);

MEPUBLIC
MeBool            MEAPI McdSpaceInsertModel(McdSpaceID s, McdModelID cm);

MEPUBLIC
MeBool            MEAPI McdSpaceInsertModelWithCulling(McdSpaceID s, 
                                                       McdModelID cm, 
                                                       McdCullingTable *table, 
                                                       int cullingIndex,                               
                                                       int cullingID);

MEPUBLIC
MeBool            MEAPI McdSpaceRemoveModel(McdModelID);

MEPUBLIC
void              MEAPI McdSpaceUpdateModel(McdModelID);

MEPUBLIC
void              MEAPI McdSpaceSetModelCullingParameters(McdSpaceID s, 
                                                          McdModelID cm, 
                                                          McdCullingTable *table, 
                                                          int cullingIndex,                               
                                                          int cullingID);

MEPUBLIC
MeBool            MEAPI McdSpaceFreezeModel(McdModelID);

MEPUBLIC
MeBool            MEAPI McdSpaceUnfreezeModel(McdModelID);

MEPUBLIC
MeBool            MEAPI McdSpaceModelIsFrozen(McdModelID);

MEPUBLIC
void              MEAPI McdSpaceUpdateAll(McdSpaceID s);

MEPUBLIC
void              MEAPI McdSpacePathUpdateAll(McdSpaceID s, MeReal duration);

MEPUBLIC
void              MEAPI McdSpaceBeginChanges(McdSpaceID space);

MEPUBLIC
void              MEAPI McdSpaceEndChanges(McdSpaceID space);

MEPUBLIC
int               MEAPI McdSpaceIsChanging(McdSpaceID space);

MEPUBLIC
void              MEAPI McdSpaceSetAABBFn(McdSpaceID s, McdSpaceUpdateAABBFnPtr updateAABBFn);

MEPUBLIC
int               MEAPI McdSpaceGetLineSegIntersections(McdSpaceID space,
                                  MeReal* inOrig, MeReal* inDest,
                                  McdLineSegIntersectResult *outList,
                                  int inMaxListSize);
MEPUBLIC
int               MEAPI McdSpaceGetLineSegFirstIntersection(McdSpaceID space,
                          MeReal* inOrig, MeReal* inDest,
                                  McdLineSegIntersectResult *outResult);

MEPUBLIC
void              MEAPI McdSpaceSetPoolFullHandler(McdSpaceID space, McdSpacePoolErrorFnPtr handler);

MEPUBLIC
McdSpacePoolErrorFnPtr  
                  MEAPI McdSpaceGetPoolFullHandler(McdSpaceID space);

/** Callback allowing the user to specify whether intersection
    with a linesegment with a specific model is enabled. Return 0 if disabled,
    non-zero otherwise. filterData is passed. */
typedef int (MEAPI *McdLineSegIntersectEnableCallback)(McdModelID, void*filterData);

MEPUBLIC
int               MEAPI McdSpaceGetLineSegFirstEnabledIntersection(
                                McdSpaceID space,
                                MeReal* inOrig, MeReal* inDest,
                                McdLineSegIntersectEnableCallback filterCB,
                                void * filterData,
                                McdLineSegIntersectResult *outResult);

MEPUBLIC
MeBool            MEAPI McdSpaceEnablePair(McdModelID cm1, McdModelID cm2);
MEPUBLIC
MeBool            MEAPI McdSpaceDisablePair(McdModelID cm1, McdModelID cm2);
MEPUBLIC
MeBool            MEAPI McdSpacePairIsEnabled(McdModelID cm1, McdModelID cm2);

MEPUBLIC
void              MEAPI McdSpaceSetUserData(McdSpaceID s, void *data);
MEPUBLIC
void *            MEAPI McdSpaceGetUserData(McdSpaceID s);

/** Structure holding iteration information for McdSpaceGetPairs */
typedef struct
{
    int count; /**< @internal */
    void *ptr; /**< @internal */

} McdSpacePairIterator;

/** @internal */
void              MEAPI McdSpacePairIteratorBegin(McdSpaceID,
                            McdSpacePairIterator*);

/** @internal */
int               MEAPI McdSpaceGetPairs(McdSpaceID, McdSpacePairIterator*,
                            McdModelPairContainer*);

int               MEAPI McdSpaceGetTransitions(McdSpaceID s, McdSpacePairIterator* iter,
                                McdModelPairContainer* a);

/** @internal */
/*
void               MEAPI McdSpaceGetPairs_list(McdSpaceID,
                           McdPairContainer*);
*/

/** Structure holding iteration information for McdSpaceGetModel */
typedef struct
{
  int it;
  int count;
} McdSpaceModelIterator;


MEPUBLIC
void              MEAPI McdSpaceModelIteratorBegin(McdSpaceID,
                            McdSpaceModelIterator *it);

MEPUBLIC
MeBool            MEAPI McdSpaceGetModel(McdSpaceID,
                            McdSpaceModelIterator *it, McdModelID*);


MEPUBLIC
int               MEAPI McdSpaceGetModelCount(McdSpaceID space);


#ifdef __cplusplus
}
#endif


#endif /* _MCDSPACE_H */
