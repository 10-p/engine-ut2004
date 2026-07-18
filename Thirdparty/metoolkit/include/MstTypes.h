#ifndef _MSTTYPES_H
#define _MSTTYPES_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:41 $ - Revision: $Revision: 1.51.2.3 $

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
 * Mst C Type Definitions.
 */

#include <MeMemory.h>
#include <MePool.h>
#include <McdFrame.h>
#include <McdSpace.h>
#include <McdBatch.h>
#include <Mdt.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct  MstUniverse         MstUniverse;
typedef struct  MstUniverseSizes    MstUniverseSizes;
typedef struct  MstBridge           MstBridge;
typedef struct  MstMaterialPair     MstMaterialPair;

typedef         MstUniverse*        MstUniverseID;
typedef         MstBridge*          MstBridgeID;

typedef         unsigned int        MstMaterialID;

/**
 * Mst all contacts callback.
 * This is the prototype for the user-defined, optional pairwise-material
 * callback. The callback will be called once per pair of models. The
 * MdtContactID 'c' is the first in a linked list of dynamic contacts. If the
 * callback returns 0, _all_ contacts created for this pair will be removed.
 */
typedef MeBool
(MEAPI *MstPerPairCBPtr)(McdIntersectResult* result, MdtContactGroupID c);

/**
 * Mst per contact callback.
 * This is the prototype for the user-defined, optional pairwise-material
 * callback. The callback will be called once per contact between models.
 * If the callback returns 0, this will be removed.
 */
typedef MeBool
(MEAPI *MstPerContactCBPtr)(McdIntersectResult* result, McdContact* colC,
                         MdtContactID dynC);

/**
 * Mst 'intersect' model pair callback.
 * This is the prototype for the user-defined, optional per-intersection
 * callback, which is executed once for each successful intersection between
 * a pair models, before any dynamics contacts are generated, allowing the
 * user to (for example) perform further contact culling at this stage.
 */
typedef void
(MEAPI *MstIntersectCBPtr)(McdIntersectResult* result);

/**
 * Parameters for a pair of materials, containing dynamic contact
 * properties, and an optional user contact callback.
 */
struct MstMaterialPair
{
    /**
     * Dynamics contact parameters (friction, restitution etc.) used for a
     * particular pair of materials.
     */
    MdtBclContactParams cp;

    /**
     * Optionals user-defined callbacks executed when models of particular
     * materials come into contact.
     */

    MstPerContactCBPtr     contactCB;

    MstPerPairCBPtr        pairCB;

    MstIntersectCBPtr      intersectCB;
};

/**
 * Structure defining the maximum number of dynamic/collision bodies,
 * materials etc. in the 'universe'. This allows most memory allocation
 * to be done once at the start.
 */
struct MstUniverseSizes
{
    /** Maximum number of dynamic bodies allowed. */
    unsigned int dynamicBodiesMaxCount;

    /** Maximum number of dynamic constraints (joints & contacts) allowed. */
    unsigned int dynamicConstraintsMaxCount;

    /** Number of user collision geometry types allowed. */
    unsigned int collisionUserGeometryTypesMaxCount;

    /** Maximum number of collision models allowed. */
    unsigned int collisionModelsMaxCount;

    /** Maximum number of simultaneously overlapping models allowed. */
    unsigned int collisionPairsMaxCount;

    /** Maximum number of model-independent instances allowed. */
    unsigned int collisionGeometryInstancesMaxCount;

    /** Maximum number materials allowed. */
    unsigned int materialsMaxCount;

    /** approximate scale of the lengths we're dealing with */
    MeReal lengthScale;

    /** approximate scale of the masses we're dealing with */
    MeReal massScale;
};

MEPUBLIC
extern const MstUniverseSizes MstUniverseDefaultSizes;

/**
 * Information needed to move contact information from the Collision Toolkit
 * to the Dynamics Toolkit. This includes a material table, used for finding
 * dynamic contact properties (friction, restitution etc.) based on the two
 * materials involved.
 */
struct MstBridge
{
    /* Material Table */

    /** Number of MstMaterialPairs in table. */
    unsigned int            maxMaterials;

    /** Number of materials currently used from the table. */
    unsigned int            usedMaterials;

    /**
     * Array of MstMaterialPairs used for storing dynamics contact
     * paramters.
     */
    MstMaterialPair*        materialPairArray;


    /* Buffers */

    /**
     * McdModelPair container only used during ..Step for getting ModelPairs
     * out of the McdSpace farfield.
     */
    McdModelPairContainer*  pairs;

    McdBatchContext*        context;

    /**
     * Temporary store for collision detection contact information. Used
     * during ..Step.
     */
    McdContact*             contacts;

    /** Number of contacts in 'contacts' above. */
    unsigned int            contactsMaxCount;
};

/**
 * Convenient Simulation 'container', with an MdtWorld (for dynamics), McdSpace
 * (for collision farfield) and an MstBridge for move contact information
 * between them.
 */
struct MstUniverse
{
    /** Record of sizes specified when universe was created. */
    MstUniverseSizes    sizes;

    /** MathEngine Dynamics Toolkit world container. */
    MdtWorldID          world;

    /** MathEngine Collision Toolkit framework. */
    McdFrameworkID      frame;

    /** MathEngine Collision Toolkit farfield. */
    McdSpaceID          space;

    /** Collision -> Dynamics bridge data. */
    MstBridgeID           bridge;
};

#ifdef __cplusplus
}
#endif


#endif
