#ifndef _MEASELOAD_H
#define _MEASELOAD_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:06 $ - Revision: $Revision: 1.14.2.1 $

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
 * @file
 * 3DSMax .ASE File Loader Utility
 */

#include <MePrecision.h>
#include <MeStream.h>

typedef struct  MeASEUV             MeASEUV;
typedef struct  MeASEMaterial       MeASEMaterial;
typedef struct  MeASEFace           MeASEFace;
typedef struct  MeASEObject         MeASEObject;
typedef struct  MeASESubMaterial    MeASESubMaterial;
typedef struct  MeASEMaterialStore  MeASEMaterialStore;

typedef enum
{
    MeASEMaterialFlagNone = 0,
    MeASEMaterialFlagTexture,
    MeASEMaterialFlagColor
} MeASEMaterialFlags;

struct MeASEUV
{
    MeReal u;
    MeReal v;
};

struct MeASESubMaterial
{
    MeASEMaterialFlags  type;
    float               ambient[4];
    float               diffuse[4];
    float               specular[4];
    char                texFilename[256];
};

struct MeASEMaterial
{
    int                 numSubs;
    MeASESubMaterial    *subMaterials;
};

struct MeASEFace
{
    int             vertexId[3];
    MeVector3       normal;
    MeVector3       vNormal[3];
    MeASEUV         map[3];
    int             materialId;
    int             subMaterialId;
};

/* We just have one of these.. */
struct MeASEMaterialStore
{
    int                 numMaterials;
    MeASEMaterial       *materials;    
};

/* ..but possibly lots of these. */
struct MeASEObject
{
    MeBool              isLoaded;

    char                name[256];

    int                 numVerts;
    int                 numFaces;
    int                 numUvs;

    MeVector3           *verts;
    MeASEFace           *faces;
    MeASEUV             *uvs;

    MeASEMaterialStore  *matStore;

    MeASEObject         *nextObject;
};

#ifdef __cplusplus
extern "C" {
#endif

MEPUBLIC
MeASEObject* MEAPI  MeASEObjectLoad(char* filename,
                        MeReal xScale, MeReal yScale, MeReal zScale);
MEPUBLIC
MeASEObject* MEAPI  MeASEObjectLoadParts(char* filename, 
                        MeReal xScale, MeReal yScale, MeReal zScale,
                        MeBool asParts);

MEPUBLIC
MeASEObject* MEAPI  MeASEObjectLoadPartsFromStream(MeStream fp, 
                                         MeReal xScale, MeReal yScale, MeReal zScale,
                                         MeBool asParts);

MEPUBLIC
void         MEAPI  MeASEObjectDestroy(MeASEObject* object);

#ifdef __cplusplus
}
#endif

#endif
