#ifndef _MCDFRAME_H
#define _MCDFRAME_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.79.2.1 $

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
  Collision framework
*/

#include <MeMemory.h>
#include <McdCTypes.h>

#include <stddef.h> /* for size_t */


#ifdef __cplusplus
extern "C" {
#endif


/*----------------------------------------------------------------
 * Framework
 *---------------------------------------------------------------
 */

  /** @internal */

MEPUBLIC
McdFrameworkID    MEAPI McdInit(int geoTypeMaxCount, int modelCount, int instanceCount, MeReal unitLength);
MEPUBLIC
void              MEAPI McdTerm(McdFrameworkID);
MEPUBLIC
void              MEAPI McdFrameworkRegisterTermAction(McdFrameworkID frame, McdTermAction );
MEPUBLIC

/* types */
MeI16             MEAPI McdFrameworkGetRegisteredTypeCount(const McdFrameworkID frame);
MEPUBLIC
MeBool            MEAPI McdFrameworkTypeIsRegistered(const McdFrameworkID frame, int typeId );
MEPUBLIC
MeBool            MEAPI McdFrameworkTypeIsValid(const McdFrameworkID frame, int id );
MEPUBLIC
const char*       MEAPI McdFrameworkGetTypeName(const McdFrameworkID frame, int typeId );
MEPUBLIC
void              MEAPI McdFrameworkShowTypes(const McdFrameworkID frame);
MEPUBLIC
void              MEAPI McdFrameworkRegisterGeometryType( McdFrameworkID frame,
                                                          McdGeometryType typeID,
                                                          char *typeName,
                                                          McdGeometryDestroyFnPtr,
                                                          McdGeometryGetAABBFnPtr,
                                                          McdGeometryGetBSphereFnPtr,
                                                          McdGeometryMaximumPointFnPtr,
                                                          McdGeometryGetMassPropertiesFnPtr,
                                                          McdGeometryDebugDrawFnPtr);

/* defaults */
MEPUBLIC
MeReal            MEAPI McdFrameworkGetDefaultContactTolerance(const McdFrameworkID frame );
MEPUBLIC
void              MEAPI McdFrameworkSetDefaultContactTolerance(McdFrameworkID frame, MeReal tol);
MEPUBLIC
McdRequest *      MEAPI McdFrameworkGetDefaultRequestPtr(const McdFrameworkID frame) ;
MEPUBLIC
void              MEAPI McdFrameworkSetDefaultRequestPtr(McdFrameworkID frame, McdRequest *r);


/* models and geometries */
MEPUBLIC
int               MEAPI McdFrameworkGetModelCount(McdFrameworkID f);

MEPUBLIC
int               MEAPI McdFrameworkGetGeometryCount(McdFrameworkID f);

MEPUBLIC
void              MEAPI McdFrameworkDestroyAllModelsAndGeometries(McdFrameworkID f);


/* binary interactions */
MEPUBLIC
void              MEAPI McdFrameworkSetInteractionsWarned(McdFramework *framework,
                                                                int geoType1,
                                                                int geoType2,
                                                                int count);


MEPUBLIC
int               MEAPI McdFrameworkGetInteractionsWarned(McdFramework *framework,
                                                                int geoType1,
                                                                int geoType2);


MEPUBLIC
void              MEAPI McdFrameworkSetInteractions(McdFramework *frame, 
                                                    int geoType1, 
                                                    int geoType2,
                                                    McdInteractions* interactions);


MEPUBLIC
McdInteractions*  MEAPI McdFrameworkGetInteractions(McdFramework *frame,
                                                           int geoType1,
                                                           int geoType2);


#ifdef MCDCHECK
MEPUBLIC
void              MEAPI McdFrameworkPrintInteractionTable(McdFramework *frame);
#endif

/* line segment */
MEPUBLIC
void              MEAPI McdFrameworkSetLineSegInteraction(McdFramework *frame, 
                                                          int geoType, 
                                                          McdLineSegIntersectFnPtr isectfn);

MEPUBLIC
McdLineSegIntersectFnPtr 
                  MEAPI McdFrameworkGetLineSegInteraction(McdFramework *frame,
                                                          int geoType);


#ifdef __cplusplus
}
#endif


#endif /* _MCDFRAME_H */
