#ifndef _MCDMODELPAIR_H
#define _MCDMODELPAIR_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.30.6.1 $

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
  Objects for tracking pairs of McdModel constituent objects

  McdModelPair objects identify and keep track of a particular pair of
  McdModel objects over multiple time steps, and to hold a variety of data
  associated with that pair.

  It is used by the farfield module to
  identify pairs of models that are nearby to each other and
  potentially colliding.

  It is used as the first argument to all interactions, such as
  McdIntersect and McdSafeTime. To prepare a McdModelPair object
  for use by these functions, McdHello must first be called. The pair is
  then ready to be used over multiple time steps. When the pair is no
  longer needed, this must be indicated by calling McdGoodbye.

*/

#include <McdCTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------
 *  McdModelPair
 *----------------------------------------------------------------
 */


MEPUBLIC
McdModelPairID    MEAPI McdModelPairCreate( McdModelID, McdModelID );

MEPUBLIC
void              MEAPI McdModelPairDestroy( McdModelPairID );

MEPUBLIC
void              MEAPI McdModelPairGetModels( McdModelPairID,
                          McdModelID *, McdModelID* );

MEPUBLIC
void              MEAPI McdModelPairReset( McdModelPairID,
                       McdModelID, McdModelID );

MEPUBLIC
McdRequest *      MEAPI McdModelPairGetRequestPtr( const McdModelPairID );

MEPUBLIC
void              MEAPI McdModelPairSetRequestPtr( McdModelPairID,
                          McdRequest* );

MEPUBLIC
void *            MEAPI McdModelPairGetUserData( McdModelPairID );

MEPUBLIC
void              MEAPI McdModelPairSetUserData( McdModelPairID, void* );

#ifdef MCDCHECK
  /** @internal */
/* convenience function for debug messages output */
void              MEAPI McdModelPairGetGeometryNames( McdModelPairID,
                            const char** stringPtr1, const char** stringPtr2 );
#endif

#ifdef __cplusplus
} /* extern "C" */
#endif

#ifdef MCDCHECK

  /** @internal */
#define MCD_CHECK_MODEL_PAIR(p) {}

#else

  /** @internal */
#define MCD_CHECK_MODEL_PAIR(p)

#endif /* MCDCHECK */

#endif /* _MCDMODELPAIR_H */
