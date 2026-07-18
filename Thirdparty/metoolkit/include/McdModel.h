#ifndef _MCDMODEL_H
#define _MCDMODEL_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.57.2.3 $

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

    The collision model object and utility functions

    An McdModel is a collision model whose geometrical shape is
    specified by a McdGeometry object. An McdModel also holds the
    transformation matrix describing its position and orientiation in 3D
    space.

    Collision models are the principal object type in the Mcd system. Typically 
    there is a one-to-one correspondence between a collision model and 
    a 3D graphics model that is rendered on the screen. Usually there is one
    collision model per dynamics model,

    You can query geometrical properties of an isolated McdModel object
    using the McdModel interface, which uses the McdGeometryInstance protocol 
    and takes into account its transformation matrix. A model may contain a
    tree of geometry instances, in the case that it is an aggregate.

    You can query the geometrical relationship between a pair of McdModel
    objects in close proximity, by creating a McdModelPair object and using
    any of the pairwise queries declared in McdInteractions.h.

    You can insert McdModel objects into McdSpace, which will keep track of
    which pairs are in close proximity.

*/

#include <McdCTypes.h>

#ifdef __cplusplus
extern "C" {
#endif



MEPUBLIC
McdModelID        MEAPI McdModelCreate(McdGeometryID g );
MEPUBLIC
void              MEAPI McdModelDestroy( McdModelID cm );
MEPUBLIC
void              MEAPI McdModelReset(McdModelID cm);

MEPUBLIC
McdSpaceID        MEAPI McdModelGetSpace(McdModelID cm);
MEPUBLIC
McdGeometryID     MEAPI McdModelGetGeometry( McdModelID g );
MEPUBLIC
void              MEAPI McdModelSetGeometry( McdModelID cm, McdGeometryID g );

MEPUBLIC
McdGeometryInstanceID
                  MEAPI McdModelGetGeometryInstance(McdModelID cm);
MEPUBLIC
McdGeometryType   MEAPI McdModelGetGeometryType(McdModelID cm);

MEPUBLIC
void              MEAPI McdModelSetTransformPtr( const McdModelID cm,
                         const MeMatrix4Ptr geometryTM );
MEPUBLIC
MeMatrix4Ptr      MEAPI McdModelGetTransformPtr( McdModelID cm );


MEPUBLIC
void              MEAPI McdModelSetRelativeTransform(McdModelID cm,
                                                     MeMatrix4 relTM);

MEPUBLIC
MeMatrix4Ptr      MEAPI McdModelGetRelativeTransform(McdModelID cm);

MEPUBLIC
void              MEAPI McdModelSetRelativeTransformPtrs(McdModelID cm,
                                                         MeMatrix4 relTM,
                                                         MeMatrix4 refTM,
                                                         MeMatrix4 compoundTM,
                                                         MeBool own);


MEPUBLIC
void              MEAPI McdModelGetRelativeTransformPtrs(McdModelID cm,
                                                         MeMatrix4Ptr *relTM,
                                                         MeMatrix4Ptr *refTM);

MEPUBLIC
void              MEAPI McdModelUpdate( McdModelID cm );
MEPUBLIC
void              MEAPI McdModelUpdatePath( McdModelID cm, MeReal motionDuration );
MEPUBLIC
void              MEAPI McdModelUpdatePathCompatible( McdModelID cm, MeReal dummy );

MEPUBLIC
void              MEAPI McdModelSetUpdateCallback( McdModelID cm, McdModelUpdateFnPtr f );
MEPUBLIC
McdModelUpdateFnPtr
                  MEAPI McdModelGetUpdateCallback( McdModelID cm );
MEPUBLIC
void              MEAPI McdModelCompoundTransforms( McdModelID cm );

MEPUBLIC
void              MEAPI McdModelGetAABB( McdModelID cm, MeVector3 minCorner,
                            MeVector3 maxCorner );


MEPUBLIC
void              MEAPI McdModelSetLinearVelocityPtr( McdModelID cm, MeReal *);
MEPUBLIC
MeReal*           MEAPI McdModelGetLinearVelocityPtr( McdModelID cm );
MEPUBLIC
void              MEAPI McdModelSetAngularVelocityPtr(McdModelID cm, MeReal *);
MEPUBLIC
MeReal*           MEAPI McdModelGetAngularVelocityPtr(McdModelID cm );

MEPUBLIC
void              MEAPI McdModelGetBSphere( McdModelID cm, MeVector3 center,
                            MeReal *radius );


MEPUBLIC
void              MEAPI McdModelSetContactTolerance(McdModelID cm, MeReal tol);
MEPUBLIC
MeReal            MEAPI McdModelGetContactTolerance(McdModelID cm );

MEPUBLIC
void              MEAPI McdModelSetUserData( McdModelID cm, void *data );
MEPUBLIC
void*             MEAPI McdModelGetUserData( McdModelID cm );

  /** @internal */
void*             MEAPI McdModelGetBodyData( McdModelID cm );

  /** @internal */
void              MEAPI McdModelSetBodyData( McdModelID cm, void * body );

  /** Set the models material ID. Used in conjunction with Mst. */
MEPUBLIC
void              MEAPI McdModelSetMaterial( McdModelID cm, int material );

  /** Read the models material id. Used in conjunction with Mst. */
MEPUBLIC
unsigned int      MEAPI McdModelGetMaterial( McdModelID cm );


  /** Set the models request ID. Used in conjunction with McduRequestTable. */
MEPUBLIC
void              MEAPI McdModelSetRequestID( McdModelID cm, int requestId );

  /** Read the models request ID. Used in conjunction with McduRequestTable. */
MEPUBLIC
int               MEAPI McdModelGetRequestID( McdModelID cm );
  /** Get the line segment intersect function associated with the model's geometry */

  /** Read the model's sort key. Used in conjunction with McduRequestTable. */
MEPUBLIC
MeI16             MEAPI McdModelGetSortKey(McdModelID cm);

  /** Set the model's sort key. Should be between 0 and (2^15)-1 */
MEPUBLIC
void              MEAPI McdModelSetSortKey(McdModelID cm, MeI16 key);


MEPUBLIC 
void              MEAPI McdModelSetIntersectCallback(McdModelID cm, McdModelIntersectFnPtr fn);

MEPUBLIC           
McdModelIntersectFnPtr 
                  MEAPI McdModelGetIntersectCallback(McdModelID cm);

#define McdModelGetDefaultRequestID() (0)

#ifdef __cplusplus
}
#endif

#endif /* _MCDMODEL_H */
