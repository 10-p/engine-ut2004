#ifndef _MEASSETFACTORYTYPES_H
#define _MEASSETFACTORYTYPES_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/05/17 14:09:41 $ - Revision: $Revision: 1.7.2.4.4.1 $

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
 * Types associated with MeAssetFactory.
 */

#include "MdtTypes.h"
#include "Mcd.h"
#include "MeAssetDBTypes.h"
#include "MeHash.h"
#include "Mst.h"

typedef struct McdGeomMan			McdGeomMan;
typedef struct MeAssetFactory		MeAssetFactory;
typedef struct MeAssetInstance		MeAssetInstance;
typedef struct MeAIGeomIt			MeAIGeomIt;
typedef struct MeAIModelIt			MeAIModelIt;
typedef struct MeAIJointIt			MeAIJointIt;

typedef void (MEAPI *GeometryPostCreateCB)(McdGeometryID geom, MeFGeometry *fg, void *userdata);
typedef McdModelID (MEAPI *ModelCreateFunc)(MeFAssetPart *part, McdGeometryID g, MdtWorldID world, MeMatrix4Ptr assetTM);
typedef void (MEAPI *ModelPostCreateCB)(McdModelID model, MeFAssetPart *part, void *userdata);
typedef MdtConstraintID (MEAPI *JointCreateFunc)(MeFJoint *joint, MdtWorldID world, McdModelID m1, McdModelID m2, MeMatrix4Ptr assetTM);
typedef void (MEAPI *JointPostCreateCB)(MdtConstraintID joint, MeFJoint *fj, void *userdata);

struct McdGeomMan
{
    McdFrameworkID  fwk;
    MeHash          *name2geom;  /* Name->McdGeometry lookup */
    MeHash          *geom2name;  /* McdGeometry->Name lookup */
	McdNullID		nullGeom;
};

struct MeAssetInstance
{
    MeAssetFactory      *af;
    MdtWorldID          world;
    McdSpaceID          space;
	MstBridgeID			bridge;
    MeFAsset            *asset;
    MeHash              *nameToGeometry;
    MeHash              *nameToModel;
    MeHash              *nameToJoint;
	MeBool				owner; /* if true, the geometry, models and constraints
								  will be destroyed in MeAssetInstanceDestroy. If
								  false the user must destroy them. */
    void                *userdata;
};

struct MeAssetFactory
{
    McdGeomMan          *gm;
    GeometryPostCreateCB geometryPostCreateCB;
	void				*geometryPostCreateCBUserdata;
    ModelCreateFunc     modelCreateFunc;
	ModelPostCreateCB	modelPostCreateCB;
	void			    *modelPostCreateCBUserdata;
    JointCreateFunc     jointCreateFunc;
	JointPostCreateCB	jointPostCreateCB;
	void			    *jointPostCreateCBUserdata;
};

struct MeAIGeomIt
{
	MeHashIterator hashIt;
};

struct MeAIModelIt
{
	MeHashIterator hashIt;
};

struct MeAIJointIt
{
	MeHashIterator hashIt;
};

#endif
