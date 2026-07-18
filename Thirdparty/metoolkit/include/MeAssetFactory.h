#ifndef _MEASSETFACTORY_H
#define _MEASSETFACTORY_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/05/17 14:09:41 $ - Revision: $Revision: 1.14.2.4.4.1 $

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
 * Asset factory API.
 */

#include "MeAssetFactoryTypes.h"
#include "MeAssetDB.h"
#include "Mcd.h"
#include "Mdt.h"

#ifdef __cplusplus
extern "C" {
#endif

/********************* Karma creation functions *******************/

MEPUBLIC
McdModelID           MEAPI McdModelCreateFromMeFAssetPart(MeFAssetPart *part, 
                          McdGeometryID g, MdtWorldID world, MeMatrix4Ptr assetTM);

MEPUBLIC
MdtConstraintID      MEAPI MdtConstraintCreateFromMeFJoint(MeFJoint *joint, 
                          MdtWorldID world, McdModelID m1, McdModelID m2, MeMatrix4Ptr assetTM);

/************************ McdGeometry Manager *********************/

MEPUBLIC
McdGeomMan*          MEAPI McdGMCreate(McdFrameworkID fwk);

MEPUBLIC
void                 MEAPI McdGMDestroy(McdGeomMan* gm);

MEPUBLIC 
int                  MEAPI McdGMGetGeomCount(McdGeomMan* gm);

MEPUBLIC            
McdFrameworkID       MEAPI McdGMGetFramework(McdGeomMan* gm);

MEPUBLIC
McdGeometryID        MEAPI McdGMCreateGeometry(McdGeomMan* gm, 
                            const MeFGeometry* fg, const char *assetName);

MEPUBLIC
McdNullID            MEAPI McdGMGetNullGeometry(McdGeomMan* gm);

MEPUBLIC
void                 MEAPI McdGMDestroyGeometry(McdGeomMan* gm, 
                               McdGeometryID geom);

/************************** MeAssetFactory ************************/

MEPUBLIC
MeAssetFactory      *MEAPI MeAssetFactoryCreate(McdFrameworkID fwk);

MEPUBLIC
void                 MEAPI MeAssetFactoryDestroy(MeAssetFactory *af);

MEPUBLIC
void				 MEAPI MeAssetFactorySetGeometryPostCreateCB(MeAssetFactory *af,
							GeometryPostCreateCB cb, void *userdata);

MEPUBLIC
void				 MEAPI MeAssetFactorySetModelCreateFunction(MeAssetFactory *af, 
								ModelCreateFunc func);

MEPUBLIC
void				 MEAPI MeAssetFactorySetModelPostCreateCB(MeAssetFactory *af, 
								ModelPostCreateCB cb, void *userdata);
MEPUBLIC
void				 MEAPI MeAssetFactorySetJointCreateFunction(MeAssetFactory *af, 
								JointCreateFunc func);

MEPUBLIC
void				 MEAPI MeAssetFactorySetJointPostCreateCB(MeAssetFactory *af,
								JointPostCreateCB cb, void *userdata);

/********************** MeAssetInstance ***************************/

MEPUBLIC
MeAssetInstance     *MEAPI MeAssetInstanceCreate(MeAssetFactory *ai, 
                            const MeFAsset *asset, MeMatrix4Ptr tm, MeBool owner,
                            MdtWorldID world, McdSpaceID space);
MEPUBLIC
void                 MEAPI MeAssetInstanceDestroy(MeAssetInstance *ai);

/* MeAssetInstance accessors */

MEPUBLIC
McdGeometryID		 MEAPI MeAssetInstanceGetGeometry(MeAssetInstance *ins, char *name);

MEPUBLIC
McdModelID           MEAPI MeAssetInstanceGetModel(MeAssetInstance *ins, char *name);

MEPUBLIC
MdtConstraintID      MEAPI MeAssetInstanceGetJoint(MeAssetInstance *ins, char *name);

MEPUBLIC
void				 MEAPI MeAssetInstanceInitGeometryIterator(MeAssetInstance *ins, MeAIGeomIt *it);

MEPUBLIC
McdGeometryID		 MEAPI MeAssetInstanceGetNextGeometry(MeAIGeomIt *it);

MEPUBLIC
void				 MEAPI MeAssetInstanceInitModelIterator(MeAssetInstance *ins, MeAIModelIt *it);

MEPUBLIC
McdModelID		     MEAPI MeAssetInstanceGetNextModel(MeAIModelIt *it);

MEPUBLIC
void				 MEAPI MeAssetInstanceInitJointIterator(MeAssetInstance *ins, MeAIJointIt *it);

MEPUBLIC
MdtConstraintID		 MEAPI MeAssetInstanceGetNextJoint(MeAIJointIt *it);

MEPUBLIC
void                 MEAPI MeAssetInstanceEnableDynamics(MeAssetInstance *ins);

MEPUBLIC
void                 MEAPI MeAssetInstanceDisableDynamics(MeAssetInstance *ins);

MEPUBLIC
void                *MEAPI MeAssetInstanceGetUserData(MeAssetInstance *ins);

MEPUBLIC
void                 MEAPI MeAssetInstanceSetUserData(MeAssetInstance *ins, void *userdata);

#ifdef __cplusplus
}
#endif

#endif
