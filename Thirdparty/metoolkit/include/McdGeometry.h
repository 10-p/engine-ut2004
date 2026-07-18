#ifndef _MCDGEOMETRY_H
#define _MCDGEOMETRY_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.35.2.1 $

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
    Common geometry functions
*/

#include <MePrecision.h>
#include <McdCTypes.h>
#include <McdFrame.h>

#ifdef __cplusplus
extern "C" {
#endif

/*----------------------------------------------------------------
 *  McdGeometry
 *----------------------------------------------------------------
 */


/** return the geometry Id for @a g.
   Each concrete geometry type has a distinct geometry id
   ( eg. McdSphereGetTypeId(), McdConvexMeshGetTypeId() ) Each McdGeometry
  object holds an id value in order to identify its actual concrete type.
  */
#define McdGeometryGetTypeId(g) ((int)((g)->mRefCtAndID&255))
#define McdGeometryGetType(g) ((int)((g)->mRefCtAndID&255))


/* polymorphic functions */

MEPUBLIC
void              MEAPI McdGeometryInit(McdGeometryID g, McdFramework *frame, MeI16 typeId );
MEPUBLIC
void              MEAPI McdGeometryDeinit(McdGeometryID g);
MEPUBLIC
MeBool            MEAPI McdGeometryIsValid(McdGeometryID g);
MEPUBLIC
const char*       MEAPI McdGeometryGetTypeName(McdGeometryID g);
MEPUBLIC
McdLineSegIntersectFnPtr
                  MEAPI McdGeometryGetLineSegIntersectFnPtr(McdGeometryID g);

MEPUBLIC
void              MEAPI McdGeometryDestroy(McdGeometryID);
MEPUBLIC
void              MEAPI McdGeometryGetAABB(McdGeometryID g, MeMatrix4 tm,
                            MeVector3 minCorner, MeVector3 maxCorner );
MEPUBLIC
void              MEAPI McdGeometryGetBSphere(McdGeometryID g,
                            MeVector3 center, MeReal *radius );
MEPUBLIC
void              MEAPI McdGeometryMaximumPoint(McdGeometryID, MeMatrix4,
                            MeReal * const inDir, MeReal * const outPoint);
MEPUBLIC
MeI16             MEAPI McdGeometryGetMassProperties( McdGeometryID g,
                            MeMatrix4 relativeTransform,
                            MeMatrix3 inertiaMatrix, MeReal *volume );
MEPUBLIC
McdFrameworkID    MEAPI McdGeometryGetFramework(McdGeometryID g);
void              MEAPI McdGeometryDebugDraw(const McdGeometryID geom, const MeMatrix4Ptr tm, const MeReal colour[3]);

MEPUBLIC
void              MEAPI McdGeometryIncrementReferenceCount( McdGeometryID g );
MEPUBLIC
void              MEAPI McdGeometryDecrementReferenceCount( McdGeometryID g );

MEPUBLIC
void              MEAPI McdGeometrySetReferenceCount( McdGeometryID g, int refCount );
MEPUBLIC
int               MEAPI McdGeometryGetReferenceCount( McdGeometryID g );


#ifdef MCDCHECK
#define MCDGEOMETRY_CHECK_ISVALID( g )\
McdGeometry_Check_IsValid( g );
#else
#define MCDGEOMETRY_CHECK_ISVALID( g )
#endif

/*
  Returns the number of geometries available in the entire collision system
#define McdGeometryGetImplementedTypeCount() (McdPrimitivesGetTypeCount()+5)
*/



/*----------------------------------------------------------------
 * Plug-in mechanism for user-defined concrete geometry types.
 *----------------------------------------------------------------
 *
 * The macros MCD_DECLARE_GEOMETRY_TYPE and MCD_DEFINE_GEOMETRY_TYPE
 * define and implement a common set of functionality
 * for all concrete geometry types T
 *
 *----------------------------------------------------------------

 * The following are declared and implemented:

 McdTypeId TGetTypeId();

 void TRegisterType();

 * The following are declared only.
 * type-specific behaviour is defined by
 * implementing these function declarations
 * for each concrete type:

   void TDestroy( McdGeometryID );
   void TGetAABB( McdGeometryID, MeMatrix4,
                  MeVector3 minCorner, MeVector3 maxCorner );
   void TGetBSphere( McdGeometryID, MeVector3 center, MeReal *radius );
   void TMaximumPoint( McdGeometryID, MeMatrix4,
                       MeReal * const inDir,
                       MeReal * const outPoint);
   void TGetMassProperties( McdGeometryID, MeMatrix4, MeMatrix3, MeReal* );


 *
 *----------------------------------------------------------------
*/

/* declare common set of "C" functionality shared by
   all concrete geometry types */

#define MCD_DECLARE_GEOMETRY_TYPE( T )                                     \
                                                                           \
MEPUBLIC                                                                   \
MeI16             MEAPI T##GetTypeId();                                    \
MEPUBLIC                                                                   \
void              MEAPI T##RegisterType(McdFramework *frame);              \
MEPUBLIC                                                                   \
void              MEAPI T##Destroy( McdGeometryID);                        \
MEPUBLIC                                                                   \
void              MEAPI T##UpdateAABB(McdGeometryInstanceID,               \
                                      MeMatrix4 finalTM, MeBool tight);    \
MEPUBLIC                                                                   \
void              MEAPI T##GetBSphere(McdGeometryID,                       \
                            MeVector3 center, MeReal *radius );            \
MEPUBLIC                                                                   \
void              MEAPI T##MaximumPoint( McdGeometryInstanceID,            \
                            MeReal * const inDir, MeReal * const outPoint);\
MEPUBLIC                                                                   \
MeI16             MEAPI T##GetMassProperties( McdGeometryID, MeMatrix4,    \
                            MeMatrix3, MeReal* );                          \
MEPUBLIC                                                                   \
char*             MEAPI T##GetTypeName( McdGeometryID );                   \
MEPUBLIC                                                                   \
void              MEAPI T##DebugDraw(const McdGeometryID,                  \
                                     const MeMatrix4 tm,                   \
                                     const MeReal colour[3])

/* implement common set of "C" functionality shared by
   all concrete geometry types */

#define MCD_IMPLEMENT_GEOMETRY_TYPE( T, T_STRING, T_ID )                   \
                                                                           \
MEPUBLIC                                                                   \
MeI16             MEAPI T##GetTypeId(){ return kMcdGeometryType##T_ID;}    \
                                                                           \
MEPUBLIC                                                                   \
void              MEAPI T##RegisterType(McdFramework *frame)               \
{                                                                          \
 McdFrameworkRegisterGeometryType(frame, kMcdGeometryType##T_ID, T_STRING, \
                         T##Destroy,                                       \
                         T##UpdateAABB, T##GetBSphere, T##MaximumPoint,    \
                         T##GetMassProperties, T##DebugDraw );             \
}                                                                          \

#ifdef __cplusplus
}
#endif

#endif /* _MCDGEOMETRY_H */
