#ifndef _MEASSETDBTYPES__H
#define _MEASSETDBTYPES__H

/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/18 12:29:57 $ - Revision: $Revision: 1.114.2.6 $

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
 * Types associated with MeAssetDB.
 */

#include <MePrecision.h>
#include <MeMisc.h>
#include <MeIDPool.h>
#include <MeHash.h>

typedef struct MyMesh               MyMesh;
typedef struct MyAABB               MyAABB;

typedef struct MeAssetDB            MeAssetDB;
typedef struct MeFPrimitive         MeFPrimitive;
typedef struct MeFGeometry          MeFGeometry;
typedef struct MeFModel             MeFModel;
typedef struct MeFJoint             MeFJoint;
typedef struct MeFAsset             MeFAsset;
typedef struct MeFAssetPart         MeFAssetPart;

/* iterators */
typedef struct MeFVertexIt          MeFVertexIt;
typedef struct MeFPrimitiveIt       MeFPrimitiveIt;
typedef struct MeFGeometryIt        MeFGeometryIt;
typedef struct MeFModelIt           MeFModelIt;
typedef struct MeFJointIt           MeFJointIt;
typedef struct MeFAssetPartIt       MeFAssetPartIt;
typedef struct MeFAssetIt           MeFAssetIt;

/* nodes */
typedef struct MeFPrimitiveNode     MeFPrimitiveNode;
typedef struct MeFGeometryNode      MeFGeometryNode;
typedef struct MeFModelNode         MeFModelNode;
typedef struct MeFJointNode         MeFJointNode;
typedef struct MeFAssetPartNode     MeFAssetPartNode;
typedef struct MeFAssetNode         MeFAssetNode;

typedef enum
{
    kMeFPrimitiveTypeUnknown = 0,
    kMeFPrimitiveTypeSphere,
    kMeFPrimitiveTypeBox,
    kMeFPrimitiveTypeCylinder,
    kMeFPrimitiveTypeSphyl,
    kMeFPrimitiveTypePlane,
    kMeFPrimitiveTypeConvex
} MeFPrimitiveType;

typedef enum
{
    kMeFJointTypeUnknown = 0,
    kMeFJointTypeCarwheel,
    kMeFJointTypeHinge,
    kMeFJointTypeBallAndSocket,
    kMeFJointTypeConeLimit,
    kMeFJointTypeUniversal,
    kMeFJointTypeRpro,
    kMeFJointTypePrismatic,
    kMeFJointTypeSkeletal,
    kMeFJointTypeAngular3,
    kMeFJointTypeSpring6
} MeFJointType;

typedef enum
{
    kMeFJointPropertyStop1,
    kMeFJointPropertyStop2,
    kMeFJointPropertyStop3,
    kMeFJointPropertyStiffness1,
    kMeFJointPropertyStiffness2,
    kMeFJointPropertyStiffness3,
    kMeFJointPropertyStiffness4,
    kMeFJointPropertyStiffness5,
    kMeFJointPropertyStiffness6,
    kMeFJointPropertyDamping1,
    kMeFJointPropertyDamping2,
    kMeFJointPropertyDamping3,
    kMeFJointPropertyDamping4,
    kMeFJointPropertyDamping5,
    kMeFJointPropertyDamping6,
    kMeFJointPropertyLimited1,
    kMeFJointPropertyMotorized1,
    kMeFJointPropertyStrength1,
    kMeFJointPropertyStrength2,
    kMeFJointPropertyStrength3,
    kMeFJointPropertyStrength4,
    kMeFJointPropertyStrength5,
    kMeFJointPropertyStrength6,
    kMeFJointPropertyDesiredVelocity1,
    kMeFJointPropertyDesiredVelocity2,
    kMeFJointPropertySpecialFloat1,
    kMeFJointPropertySpecialInt1,
    kMeFJointPropertySpecialInt2,
    kMeFJointPropertySpecialBool1
} MeFJointProperty;

typedef enum
{
    kMeFModelTypeDynamicsAndGeometry,
    kMeFModelTypeDynamicsOnly,
    kMeFModelTypeGeometryOnly
} MeFModelType;


struct MeFVertexIt
{
    int               currentVertex;
    MeFPrimitive      *prim;
};

struct MeFPrimitive
{
    char              *id;
    MeFGeometry       *geometry; /* pointer to parent */
    MeVector3         *vertices;
    int               nVertices;
    int               maxVertices;
    MeFPrimitiveType  type;
    MeVector3         dims; /* dims[0] is radius, dims[1] is height */
    MeMatrix4         tm;
};

struct MeFPrimitiveNode
{
    MeFPrimitiveNode  *prev;
    MeFPrimitive      *current;
    MeFPrimitiveNode  *next;
};

struct MeFPrimitiveIt
{
    MeFPrimitiveNode *node;
};

struct MeFGeometry
{
    char              *id;
    MeFAsset          *asset; /* pointer to parent */
    MeFPrimitiveNode  *nilPrimitive;
    char              *graphicHint;
    MeReal            graphicScale;
    MeVector3         graphicOffset;
    int               primCount;
};

struct MeFGeometryNode
{
    MeFGeometryNode   *prev;
    MeFGeometry       *current;
    MeFGeometryNode   *next;
};

struct MeFGeometryIt
{
    MeFGeometryNode *node;
};

struct MeFModel
{
    char          *id;
    MeFModelType  type;
    MeFAsset      *asset; /* pointer to parent */
    char          *geometry;
    MeVector3     mass_offset;
    MeReal        mass;
    MeReal        density;
    MeReal        inertia[6];
    MeReal        linearDamp;
    MeReal        angularDamp;
    MeVector3     fastSpin;
    MeBool        useFastSpin;
};

struct MeFModelNode
{
    MeFModelNode  *prev;
    MeFModel      *current;
    MeFModelNode  *next;
};

struct MeFModelIt
{
    MeFModelNode  *node;
};

struct MeFJoint
{
    char            *id;
    char            *part[2];
    MeFAsset        *asset; /* pointer to parent */
    MeFJointType    type;
    MeVector3       pos[2];
    MeVector3       pax[2];
    MeVector3       oax[2];

    /* joint-specific data */
    MeReal          stop[3];   /* half angles */
    MeReal          stiff[6]; /* hi_stiff, lo_stiff, susp_soft */
    MeReal          damp[6]; 
    MeBool          bLimited[1];
    MeBool          bMotorized[1];
    MeReal          strength[6]; /* rpro params, maxforces */
    MeReal          desVel[2];
    MeReal          special_f[1]; /* special, non-standard float params (susp_ref) */
    int             special_i[2];  /* special, non-standard signed int params (twistType, coneType) */
    MeBool          special_b[1]; /* special, not-standard bool params (combined, steering lock) */
};

struct MeFJointNode
{
    MeFJointNode  *prev;
    MeFJoint      *current;
    MeFJointNode  *next;
};

struct MeFJointIt
{
    MeFJointNode  *node;
};

struct MeFAssetPart
{
    char          *id;
    MeFAsset      *asset; /* pointer to parent */
    char          *model;
    char          *graphicHint;
    char          *parent;
    MeMatrix4     tm;
    MeReal        graphicScale;
    MeVector3     graphicOffset;
    int           index; /* index into disabled collisions array */
};

struct MeFAssetPartNode
{
    MeFAssetPartNode  *prev;
    MeFAssetPart      *current;
    MeFAssetPartNode  *next;
};

struct MeFAssetPartIt
{
    MeFAssetPartNode  *node;
};

/**
 * Asset data structure.
 */
struct MeFAsset
{
    char              *name;                /** Name of the asset. */
    MeAssetDB         *db;                  /* pointer to parent, in this case the asset database */
    MeFGeometryNode   *nilGeometry;         /** Geometries */
    MeFModelNode      *nilModel;            /** Models */
    MeFAssetPartNode  *nilPart;             /** Parts */
    MeFJointNode      *nilJoint;            /** Joints */
    MeHash            *nameToGeometry;      
    MeHash            *nameToModel;
    MeHash            *nameToPart;
    MeHash            *nameToJoint;
    char              *graphicHint;
    char              *refPart;
    MeU32             *disabledColArray;
    MeIDPool          *disabledColIndexPool;   /* used in disabled collisions array */
    int               id;
    MeReal            graphicScale;
    int               geomCount;
    int               modelCount;
    int               jointCount;
    int               partCount;
    int               maxParts;
    MeReal            massScale;
    MeReal            lengthScale;
};

struct MeFAssetNode
{
    MeFAssetNode  *prev;
    MeFAsset      *current;
    MeFAssetNode  *next;
};

struct MeFAssetIt
{
    MeFAssetNode  *node;
};

struct MeAssetDB
{
    MeFAssetNode    *nilAsset;
    unsigned int    assetCount;
};

struct MyMesh
{
    int         (*faceVertex)[3];
    MeVector3   *verts;
    int         numFaces;
    int         numVerts;
};

struct MyAABB
{
    MeVector3 min;
    MeVector3 max;
    MeVector3 size;
    MeVector3 centre;
};

#endif
