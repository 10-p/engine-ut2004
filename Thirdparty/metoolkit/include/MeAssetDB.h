#ifndef __MEASSETDB__H
#define __MEASSETDB__H

/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/17 11:20:40 $ - Revision: $Revision: 1.119.2.11 $

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
 * Asset database API.
 */

#include <MeAssetDBTypes.h>

#ifdef __cplusplus
extern "C" {
#endif

/* asset database creation, destruction and copying */

MEPUBLIC
MeAssetDB      *MEAPI MeAssetDBCreate();
MEPUBLIC
void            MEAPI MeAssetDBDestroy(MeAssetDB *const db);
MEPUBLIC
void            MEAPI MeAssetDBDeleteContents(MeAssetDB *const db);
MEPUBLIC
MeAssetDB      *MEAPI MeAssetDBCreateCopy(const MeAssetDB *const db);
MEPUBLIC
void            MEAPI MeAssetDBInsertCopy(MeAssetDB *const to, const MeAssetDB *const from);

/* asset database accessors */

MEPUBLIC
MeBool          MEAPI MeAssetDBIsEmpty(const MeAssetDB *const db);
MEPUBLIC
int             MEAPI MeAssetDBGetAssetCount(const MeAssetDB *const db);
MEPUBLIC
void            MEAPI MeAssetDBInitAssetIterator(const MeAssetDB *const db, MeFAssetIt *const it);
MEPUBLIC
MeFAsset       *MEAPI MeAssetDBGetAsset(MeFAssetIt *const it);
MEPUBLIC
MeFAsset       *MEAPI MeAssetDBLookupAsset(const MeAssetDB *const db, int id);
MEPUBLIC
MeFAsset       *MEAPI MeAssetDBLookupAssetByName(const MeAssetDB *const db, const char *const name);

/* asset database mutators */

MEPUBLIC
void            MEAPI MeAssetDBInsertAsset(MeAssetDB *const db, MeFAsset *const asset);
MEPUBLIC
void            MEAPI MeAssetDBRemoveAsset(MeFAsset *const asset);

/* asset creation and destruction */

MEPUBLIC
MeFAsset       *MEAPI MeFAssetCreate(const char *const name, int id);
MEPUBLIC
MeFAsset       *MEAPI MeFAssetCreateCopy(const MeFAsset *const asset, MeBool recurse);
MEPUBLIC
void            MEAPI MeFAssetDestroy(MeFAsset *const asset);
MEPUBLIC
void            MEAPI MeFAssetCombine(MeFAsset *const dest, MeFAsset *const source);

/* asset accessors */

MEPUBLIC
char           *MEAPI MeFAssetGetName(const MeFAsset *const asset);
MEPUBLIC
int             MEAPI MeFAssetGetID(const MeFAsset *const asset);
MEPUBLIC
char           *MEAPI MeFAssetGetReferencePart(const MeFAsset *const asset);
MEPUBLIC
char           *MEAPI MeFAssetGetGraphicHint(const MeFAsset *const asset);
MEPUBLIC
MeReal          MEAPI MeFAssetGetGraphicScale(const MeFAsset *const asset);
MEPUBLIC
int             MEAPI MeFAssetGetGeometryCount(const MeFAsset *const asset);
MEPUBLIC
int             MEAPI MeFAssetGetModelCount(const MeFAsset *const asset);
MEPUBLIC
int             MEAPI MeFAssetGetJointCount(const MeFAsset *const asset);
MEPUBLIC
int             MEAPI MeFAssetGetPartCount(const MeFAsset *const asset);
MEPUBLIC
void            MEAPI MeFAssetInitGeometryIterator(const MeFAsset *const asset, MeFGeometryIt *const it);
MEPUBLIC
MeFGeometry    *MEAPI MeFAssetGetGeometry(MeFGeometryIt *const it);
MEPUBLIC
void            MEAPI MeFAssetInitModelIterator(const MeFAsset *const asset, MeFModelIt *const it);
MEPUBLIC
MeFModel       *MEAPI MeFAssetGetModel(MeFModelIt *const it);
MEPUBLIC
void            MEAPI MeFAssetInitJointIterator(const MeFAsset *const asset, MeFJointIt *const it);
MEPUBLIC
MeFJoint       *MEAPI MeFAssetGetJoint(MeFJointIt *const it);
MEPUBLIC
void            MEAPI MeFAssetInitPartIterator(const MeFAsset *const asset, MeFAssetPartIt *const it);
MEPUBLIC
MeFAssetPart   *MEAPI MeFAssetGetPart(MeFAssetPartIt *const it);
MEPUBLIC
MeFGeometry    *MEAPI MeFAssetLookupGeometry(const MeFAsset *const asset, char *const name);
MEPUBLIC
MeFModel       *MEAPI MeFAssetLookupModel(const MeFAsset *const asset, char *const name);
MEPUBLIC
MeFJoint       *MEAPI MeFAssetLookupJoint(const MeFAsset *const asset, char *const name);
MEPUBLIC
MeFAssetPart   *MEAPI MeFAssetLookupPart(const MeFAsset *const asset, char *const name);
MEPUBLIC
void            MEAPI MeFAssetGetGeometrySortedByName(const MeFAsset *const asset, MeFGeometry **geomArray);
MEPUBLIC
void            MEAPI MeFAssetGetModelsSortedByName(const MeFAsset *const asset, MeFModel **modelArray);
MEPUBLIC
void            MEAPI MeFAssetGetPartsSortedByName(const MeFAsset *const asset, MeFAssetPart **partArray);
MEPUBLIC
void            MEAPI MeFAssetGetJointsSortedByName(const MeFAsset *const asset, MeFJoint **jointArray);
MEPUBLIC
char           *MEAPI MeFAssetMakeGeometryNameUnique(const MeFAsset *const asset, const MeFAsset *const asset2, 
                                                     char *name, char *buffer, int bufLength);
MEPUBLIC
char           *MEAPI MeFAssetMakeModelNameUnique(const MeFAsset *const asset, const MeFAsset *const asset2,
                                                  char *name, char *buffer, int bufLength);
MEPUBLIC
char           *MEAPI MeFAssetMakePartNameUnique(const MeFAsset *const asset, const MeFAsset *const asset2,
                                                 char *name, char *buffer, int bufLength);
MEPUBLIC
char           *MEAPI MeFAssetMakeJointNameUnique(const MeFAsset *const asset, const MeFAsset *const asset2,
                                                  char *name, char *buffer, int bufLength);
MEPUBLIC
MeBool          MEAPI MeFAssetIsEmpty(const MeFAsset *const asset);
MEPUBLIC
MeReal          MEAPI MeFAssetGetMassScale(const MeFAsset *const asset);
MEPUBLIC
MeReal          MEAPI MeFAssetGetLengthScale(const MeFAsset *const asset);


/* asset mutators */

MEPUBLIC
void            MEAPI MeFAssetRename(MeFAsset *const asset, const char *const name);
MEPUBLIC
void            MEAPI MeFAssetSetID(MeFAsset *const asset, int id);
MEPUBLIC
void            MEAPI MeFAssetSetReferencePart(MeFAsset *const asset, const char *const part);
MEPUBLIC
void            MEAPI MeFAssetSetGraphicHint(MeFAsset *const asset, const char *const hint);
MEPUBLIC
void            MEAPI MeFAssetSetGraphicScale(MeFAsset *const asset, MeReal scale);
MEPUBLIC        
void            MEAPI MeFAssetInsertGeometry(MeFAsset *const asset, MeFGeometry *const geometry);
MEPUBLIC        
void            MEAPI MeFAssetRemoveGeometry(MeFGeometry *const geometry);
MEPUBLIC        
void            MEAPI MeFAssetInsertModel(MeFAsset *const asset, MeFModel *const model);
MEPUBLIC        
void            MEAPI MeFAssetRemoveModel(MeFModel *const model);
MEPUBLIC        
void            MEAPI MeFAssetInsertJoint(MeFAsset *const asset, MeFJoint *const joint);
MEPUBLIC        
void            MEAPI MeFAssetRemoveJoint(MeFJoint *const joint);
MEPUBLIC        
void            MEAPI MeFAssetInsertPart(MeFAsset *const asset, MeFAssetPart *const part);
MEPUBLIC        
void            MEAPI MeFAssetRemovePart(MeFAssetPart *const part);
MEPUBLIC
void            MEAPI MeFAssetRemoveAllGeometry(MeFAsset *const asset);
MEPUBLIC
void            MEAPI MeFAssetRemoveAllModels(MeFAsset *const asset);
MEPUBLIC
void            MEAPI MeFAssetRemoveAllParts(MeFAsset *const asset);
MEPUBLIC
void            MEAPI MeFAssetScale(MeFAsset *const asset, MeReal scale);
MEPUBLIC
void            MEAPI MeFAssetResolveGeometryReferences(MeFAsset *const asset, const char *oldName, const char *newName);
MEPUBLIC
void            MEAPI MeFAssetResolveModelReferences(MeFAsset *const asset, const char *oldName, const char *newName);
MEPUBLIC
void            MEAPI MeFAssetResolvePartReferences(MeFAsset *const asset, const char *oldName, const char *newName);
MEPUBLIC
void            MEAPI MeFAssetSetMassScale(MeFAsset *const asset, MeReal scale);
MEPUBLIC
void            MEAPI MeFAssetSetLengthScale(MeFAsset *const asset, MeReal scale);

/* geometry creation and destruction */

MEPUBLIC
MeFGeometry    *MEAPI MeFGeometryCreate(const char *const name);
MEPUBLIC
MeFGeometry    *MEAPI MeFGeometryCreateCopy(const MeFGeometry *const geometry, MeBool recurse);
MEPUBLIC
MeFGeometry    *MEAPI MeFGeometryCreateFromASE(const char *const name, char *const file, MeReal xScale, MeReal yScale, MeReal zScale);
MEPUBLIC
void            MEAPI MeFGeometryDestroy(MeFGeometry *const geometry);

/* geometry accessors */

MEPUBLIC
char *          MEAPI MeFGeometryGetName(const MeFGeometry *const geometry);
MEPUBLIC
char           *MEAPI MeFGeometryGetGraphicHint(const MeFGeometry *const geometry);
MEPUBLIC
MeReal          MEAPI MeFGeometryGetGraphicScale(const MeFGeometry *const geometry);
MEPUBLIC
void            MEAPI MeFGeometryGetGraphicOffset(const MeFGeometry *const geometry, MeVector3 v);
MEPUBLIC
void            MEAPI MeFGeometryInitPrimitiveIterator(const MeFGeometry *const geometry, MeFPrimitiveIt *const it);
MEPUBLIC
MeFPrimitive   *MEAPI MeFGeometryGetPrimitive(MeFPrimitiveIt *const it);
MEPUBLIC
MeFPrimitive   *MEAPI MeFGeometryLookupPrimitive(const MeFGeometry *const eometry, char *const name);
MEPUBLIC
int             MEAPI MeFGeometryGetPrimitiveCount(const MeFGeometry *const geometry);

/* geometry mutators */

MEPUBLIC
void            MEAPI MeFGeometryRename(MeFGeometry *const geometry, const char *const name);
MEPUBLIC
void            MEAPI MeFGeometrySetGraphicHint(MeFGeometry *const geometry, const char *const hint);
MEPUBLIC
void            MEAPI MeFGeometrySetGraphicScale(MeFGeometry *const geometry, MeReal scale);
MEPUBLIC
void            MEAPI MeFGeometrySetGraphicOffset(MeFGeometry *const geometry, MeReal dx, MeReal dy, MeReal dz);
MEPUBLIC        
void            MEAPI MeFGeometryInsertPrimitive(MeFGeometry *const geometry, MeFPrimitive *const p);
MEPUBLIC
void            MEAPI MeFGeometryRemovePrimitive(MeFPrimitive *const p);
MEPUBLIC
void            MEAPI MeFGeometryScale(MeFGeometry *const geometry, MeReal scale);
MEPUBLIC
void            MEAPI MeFGeometryScaleNoGraphic(MeFGeometry *const geometry, MeReal scale);

/* geometry primitive creation and destruction */

MEPUBLIC
MeFPrimitive   *MEAPI MeFPrimitiveCreate(const char *const name, MeFPrimitiveType type);
MEPUBLIC
MeFPrimitive   *MEAPI MeFSphereCreateFromMesh(const char *const name, const MyMesh*const  mesh, const MeMatrix4Ptr relTM);
MEPUBLIC
MeFPrimitive   *MEAPI MeFBoxCreateFromMesh(const char *const name, const MyMesh*const  mesh, const MeMatrix4Ptr relTM);
MEPUBLIC
MeFPrimitive   *MEAPI MeFCylinderCreateFromMesh(const char *const name, const MyMesh*const  mesh, const MeMatrix4Ptr relTM);
MEPUBLIC
MeFPrimitive   *MEAPI MeFConvexCreateFromMesh(const char *const name, const MyMesh*const  mesh, const MeMatrix4Ptr relTM);
MEPUBLIC
MeFPrimitive   *MEAPI MeFPrimitiveCreateCopy(const MeFPrimitive *const p, MeBool recurse);
MEPUBLIC
void            MEAPI MeFPrimitiveDestroy(MeFPrimitive *const p);

/* geometry primitive accessors */

MEPUBLIC
MeFPrimitiveType MEAPI MeFPrimitiveGetType(const MeFPrimitive *const p);
MEPUBLIC
char *          MEAPI MeFPrimitiveGetName(const MeFPrimitive *const p);
MEPUBLIC
MeReal          MEAPI MeFPrimitiveGetRadius(const MeFPrimitive *const p);
MEPUBLIC
void            MEAPI MeFPrimitiveGetDimensions(const MeFPrimitive *const p, MeVector3 dims);
MEPUBLIC
MeReal          MEAPI MeFPrimitiveGetHeight(const MeFPrimitive *const p);
MEPUBLIC
int             MEAPI MeFPrimitiveGetVertexCount(const MeFPrimitive *const p);
MEPUBLIC
MeVector3      *MEAPI MeFPrimitiveGetVertexArray(MeFPrimitive *const p);
MEPUBLIC
void            MEAPI MeFPrimitiveInitVertexIterator(MeFPrimitive *const p, MeFVertexIt *const it);
MEPUBLIC
MeReal         *MEAPI MeFPrimitiveGetVertex(MeFVertexIt *const it);
MEPUBLIC
MeMatrix4Ptr    MEAPI MeFPrimitiveGetTransformPtr(MeFPrimitive *const p);

/* geometry primitive mutators */

MEPUBLIC
void            MEAPI MeFPrimitiveRename(MeFPrimitive *const p, const char *const name);
MEPUBLIC
void            MEAPI MeFPrimitiveSetRadius(MeFPrimitive *const sphere, MeReal r);
MEPUBLIC
void            MEAPI MeFPrimitiveSetDimensions(MeFPrimitive *const box, MeReal dx, MeReal dy, MeReal dz);
MEPUBLIC
void            MEAPI MeFPrimitiveSetHeight(MeFPrimitive *const p, MeReal height);
MEPUBLIC
void            MEAPI MeFPrimitiveAddVertex(MeFPrimitive *const p, MeVector3 vertex);
MEPUBLIC
void            MEAPI MeFPrimitiveSetVertexArray(MeFPrimitive *const p, MeVector3 *vertices, int nVertices);
MEPUBLIC
void            MEAPI MeFPrimitiveSetTransform(MeFPrimitive *const p, const MeMatrix4Ptr tm);
MEPUBLIC
void            MEAPI MeFPrimitiveScale(MeFPrimitive *const p, MeReal scale);

/* model creation and destruction */

MEPUBLIC
MeFModel       *MEAPI MeFModelCreate(const char *const name, MeFModelType type);
MEPUBLIC
MeFModel       *MEAPI MeFModelCreateCopy(const MeFModel *const model, MeBool recurse);
MEPUBLIC
void            MEAPI MeFModelCopy(MeFModel *dst, const MeFModel *const src, MeBool setGeometry);
MEPUBLIC
void            MEAPI MeFModelDestroy(MeFModel *const model);

/* model accessors */

MEPUBLIC
char           *MEAPI MeFModelGetName(const MeFModel *const model);
MEPUBLIC
MeFModelType    MEAPI MeFModelGetType(const MeFModel *const model);
MEPUBLIC
char           *MEAPI MeFModelGetGeometryName(const MeFModel *const model);
MEPUBLIC
MeFGeometry    *MEAPI MeFModelGetGeometry(const MeFModel *const model);
MEPUBLIC
void            MEAPI MeFModelGetMassOffset(const MeFModel *const model, MeVector3 v);
MEPUBLIC
MeReal          MEAPI MeFModelGetMass(const MeFModel *const model);
MEPUBLIC
MeReal          MEAPI MeFModelGetDensity(const MeFModel *const model);
MEPUBLIC
void            MEAPI MeFModelGetInertiaTensor(const MeFModel *const model, const MeMatrix3Ptr I);
MEPUBLIC
MeReal          MEAPI MeFModelGetLinearVelocityDamping(const MeFModel *const model);
MEPUBLIC
MeReal          MEAPI MeFModelGetAngularVelocityDamping(const MeFModel *const model);
MEPUBLIC
void            MEAPI MeFModelGetFastSpinAxis(const MeFModel *const model, MeVector3 axis);
MEPUBLIC
MeBool          MEAPI MeFModelIsFastSpinAxisEnabled(const MeFModel *const model);

/* model mutators */

MEPUBLIC
void            MEAPI MeFModelRename(MeFModel *const model, const char *const name);
MEPUBLIC
void            MEAPI MeFModelSetType(MeFModel *const model, MeFModelType type);
MEPUBLIC
void            MEAPI MeFModelSetGeometry(MeFModel *const model, const MeFGeometry *const fg);
MEPUBLIC
void            MEAPI MeFModelSetGeometryByName(MeFModel *const model, const char *const name);
MEPUBLIC
void            MEAPI MeFModelSetMassOffset(MeFModel *const model, MeVector3 v);
MEPUBLIC
void            MEAPI MeFModelSetMass(MeFModel *const model, MeReal mass);
MEPUBLIC
void            MEAPI MeFModelSetDensity(MeFModel *const model, MeReal density);
MEPUBLIC
void            MEAPI MeFModelSetInertiaTensor(MeFModel *const model, const MeMatrix3Ptr I);
MEPUBLIC
void            MEAPI MeFModelSetLinearVelocityDamping(MeFModel *const model, MeReal d);
MEPUBLIC
void            MEAPI MeFModelSetAngularVelocityDamping(MeFModel *const model, MeReal d);
MEPUBLIC
void            MEAPI MeFModelSetFastSpinAxis(MeFModel *const model, MeReal x, MeReal y, MeReal z);
MEPUBLIC
void            MEAPI MeFModelEnableFastSpinAxis(MeFModel *const model, MeBool b);
MEPUBLIC
void            MEAPI MeFModelScale(MeFModel *const model, MeReal scale);

/* MeFAssetPart creation and destruction */

MEPUBLIC
MeFAssetPart   *MEAPI MeFAssetPartCreate(const char *const name, MeFModel *const model, const MeMatrix4Ptr relTM);
MEPUBLIC
MeFAssetPart   *MEAPI MeFAssetPartCreateCopy(const MeFAssetPart *const part, MeBool recurse);
MEPUBLIC
void            MEAPI MeFAssetPartDestroy(MeFAssetPart *const part);

/* MeFAssetPart accessors */

MEPUBLIC
char           *MEAPI MeFAssetPartGetName(const MeFAssetPart *const part);
MEPUBLIC
char           *MEAPI MeFAssetPartGetModelName(const MeFAssetPart *const part);
MEPUBLIC
MeFModel       *MEAPI MeFAssetPartGetModel(const MeFAssetPart *const part);
MEPUBLIC
MeFGeometry    *MEAPI MeFAssetPartGetGeometry(const MeFAssetPart *const part);
MEPUBLIC
MeMatrix4Ptr    MEAPI MeFAssetPartGetTransformPtr(MeFAssetPart *const part);
MEPUBLIC
void            MEAPI MeFAssetPartGetPosition(const MeFAssetPart *const part, MeVector3 pos);
MEPUBLIC
char           *MEAPI MeFAssetPartGetGraphicHint(const MeFAssetPart *const part);
MEPUBLIC
MeReal          MEAPI MeFAssetPartGetGraphicScale(const MeFAssetPart *const part);
MEPUBLIC
void            MEAPI MeFAssetPartGetGraphicOffset(const MeFAssetPart *const part, MeVector3 v);
MEPUBLIC
char           *MEAPI MeFAssetPartGetParentPartName(const MeFAssetPart *const part);
MEPUBLIC
MeFAssetPart   *MEAPI MeFAssetPartGetParentPart(const MeFAssetPart *const part);
MEPUBLIC
MeBool          MEAPI MeFAssetPartIsCollisionEnabled(const MeFAssetPart *const p1, MeFAssetPart *const p2);
MEPUBLIC
int             MEAPI MeFAssetPartGetDisabledCollisionIndex(const MeFAssetPart *const part);

/* MeFAssetPart mutators */

MEPUBLIC
void            MEAPI MeFAssetPartRename(MeFAssetPart *const part, const char *const name);
MEPUBLIC
void            MEAPI MeFAssetPartSetGraphicHint(MeFAssetPart *const part, const char *const hint);
MEPUBLIC
void            MEAPI MeFAssetPartSetGraphicScale(MeFAssetPart *const part, MeReal scale);
MEPUBLIC
void            MEAPI MeFAssetPartSetGraphicOffset(MeFAssetPart *const part, MeReal dx, MeReal dy, MeReal dz);
MEPUBLIC
void            MEAPI MeFAssetPartScale(MeFAssetPart *const part, MeReal scale);
MEPUBLIC
void            MEAPI MeFAssetPartSetTransform(MeFAssetPart *const part, const MeMatrix4Ptr tm);
MEPUBLIC
void            MEAPI MeFAssetPartSetPosition(MeFAssetPart *const part, MeReal x, MeReal y, MeReal z);
MEPUBLIC
void            MEAPI MeFAssetPartSetParentPart(MeFAssetPart *const part, const MeFAssetPart *const parent);
MEPUBLIC
void            MEAPI MeFAssetPartSetParentPartByName(MeFAssetPart *const part, const char *const name);
MEPUBLIC
void            MEAPI MeFAssetPartSetModel(MeFAssetPart *const part, const MeFModel *const model);
MEPUBLIC
void            MEAPI MeFAssetPartSetModelByName(MeFAssetPart *const part, const char *const name);
MEPUBLIC
void            MEAPI MeFAssetPartEnableCollision(const MeFAssetPart *const p1, const MeFAssetPart *const p2, MeBool enable);
MEPUBLIC
void            MEAPI MeFAssetPartEnableAllCollisions(const MeFAssetPart *const part);

/* joint creation and destruction */

MEPUBLIC
MeFJoint       *MEAPI MeFJointCreate(const char *const name, MeFJointType type);
MEPUBLIC
MeFJoint       *MEAPI MeFJointCreateCopy(const MeFJoint *const joint, MeBool recurse);
MEPUBLIC
void            MEAPI MeFJointCopy(MeFJoint *const dst, const MeFJoint *const src, MeBool copyAxes);
MEPUBLIC
void            MEAPI MeFJointDestroy(MeFJoint *const joint);

/* generic joint accessors */

MEPUBLIC
char           *MEAPI MeFJointGetName(const MeFJoint *const joint);
MEPUBLIC
MeFJointType    MEAPI MeFJointGetType(const MeFJoint *const joint);
MEPUBLIC
char           *MEAPI MeFJointGetPartName(const MeFJoint *const joint, int index);
MEPUBLIC
MeFAssetPart   *MEAPI MeFJointGetPart(const MeFJoint *const joint, int index);
MEPUBLIC
void            MEAPI MeFJointGetPosition(const MeFJoint *const joint, int index, MeVector3 pos);
MEPUBLIC
void            MEAPI MeFJointGetPrimaryAxis(const MeFJoint *const joint, int index, MeVector3 axis);
MEPUBLIC
void            MEAPI MeFJointGetOrthogonalAxis(const MeFJoint *const joint, int index, MeVector3 axis);

/* generic joint mutators */

MEPUBLIC
void            MEAPI MeFJointSetGenericDefaults(MeFJoint *const j);
MEPUBLIC
void            MEAPI MeFJointSetPerTypeDefaults(MeFJoint *const j);
MEPUBLIC
void            MEAPI MeFJointSetType(MeFJoint *const fj, MeFJointType type);
MEPUBLIC
void            MEAPI MeFJointRename(MeFJoint *const joint, const char *const name);
MEPUBLIC
void            MEAPI MeFJointSetPosition(MeFJoint *const joint, int index, MeReal x, MeReal y, MeReal z);
MEPUBLIC
void            MEAPI MeFJointSetPrimaryAxis(MeFJoint *const joint, int index, MeReal x, MeReal y, MeReal z);
MEPUBLIC
void            MEAPI MeFJointSetOrthogonalAxis(MeFJoint *const joint, int index, MeReal x, MeReal y, MeReal z);
MEPUBLIC
void            MEAPI MeFJointSetPart(MeFJoint *const j, const MeFAssetPart *const part, int index);
MEPUBLIC
void            MEAPI MeFJointSetPartByName(MeFJoint *const j, const char *const part, int index);
MEPUBLIC
void            MEAPI MeFJointScale(MeFJoint *const joint, MeReal scale);

/* joint-specific properties */

MEPUBLIC
MeBool          MEAPI MeFJointGetProperty1i(const MeFJoint *const joint, MeFJointProperty p, int *i);
MEPUBLIC
MeBool          MEAPI MeFJointGetProperty1ui(const MeFJoint *const joint, MeFJointProperty p, unsigned int *u);
MEPUBLIC
MeBool          MEAPI MeFJointGetProperty1b(const MeFJoint *const joint, MeFJointProperty p, MeBool *b);
MEPUBLIC
MeBool          MEAPI MeFJointGetProperty1f(const MeFJoint *const joint, MeFJointProperty p, MeReal *x);

MEPUBLIC
void            MEAPI MeFJointSetProperty1i(MeFJoint *const joint, MeFJointProperty p, int i);
MEPUBLIC
void            MEAPI MeFJointSetProperty1ui(MeFJoint *const joint, MeFJointProperty p, unsigned int i);
MEPUBLIC
void            MEAPI MeFJointSetProperty1b(MeFJoint *const joint, MeFJointProperty p, MeBool b);
MEPUBLIC
void            MEAPI MeFJointSetProperty1f(MeFJoint *const joint, MeFJointProperty p, MeReal x);

#ifdef __cplusplus
}
#endif


#endif
