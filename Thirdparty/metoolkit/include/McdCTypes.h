#ifndef _MCDCTYPES_H
#define _MCDCTYPES_H

/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.33.2.3 $

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
    "C" types for Mcd
*/

#include <MePrecision.h>
#include <MePool.h>

#ifdef __cplusplus
extern "C"
{
#endif

struct _McdGeometry;
typedef struct _McdGeometry McdGeometry;
typedef McdGeometry *McdGeometryID;

struct _McdModel;
typedef struct _McdModel McdModel;
typedef McdModel * McdModelID;

struct _McdGeometryInstance;
typedef struct _McdGeometryInstance * McdGeometryInstanceID;

struct _McdModelPair;
typedef struct _McdModelPair McdModelPair;
typedef McdModelPair * McdModelPairID;

struct _McdModelPairContainer;
typedef struct _McdModelPairContainer McdModelPairContainer;

struct _McdModelPairContainerIterator;
typedef struct _McdModelPairContainerIterator McdModelPairContainerIterator;

struct _McdContact;
typedef struct _McdContact McdContact;
typedef McdContact *McdContactID;

struct _McdLineSegIntersectResult;
typedef struct _McdLineSegIntersectResult McdLineSegIntersectResult;

struct _McdSafeTimeResult;
typedef struct _McdSafeTimeResult McdSafeTimeResult;

struct _McdIntersectResult;
typedef struct _McdIntersectResult McdIntersectResult;

struct _McdRequest;
typedef struct _McdRequest McdRequest;
typedef McdRequest *McdRequestID;

struct _McdInteractions;
typedef struct _McdInteractions McdInteractions;

struct _McdInteractionTable;
typedef struct _McdInteractionTable McdInteractionTable;

struct _McdFramework;
typedef struct _McdFramework McdFramework;
typedef McdFramework *McdFrameworkID;

typedef struct _McdSpace McdSpace;
typedef McdSpace *McdSpaceID;

typedef McdModelID McdCompositeModelID;

/**
 *  Options for displaying debug info using the MeDebugDraw functions.
 *  @see McdFrameworkSetDebugDrawing
 */
typedef enum
{
    /** 
     *  Draw AABBs of boxes (light grey for unfrozen models and 
        dark grey for frozen ones)
     */
    kMcdDebugDrawAABB = 0x001

} McdDebugDrawOptions;

/*---------------------------------------------------------------
 * McdFrame
 *---------------------------------------------------------------
 */



typedef
void (*McdTermAction)(McdFrameworkID frame);

/* Collision state struct */

typedef struct
{
  McdTermAction action;
  void *next;

} McdTermActionLink;


typedef int  (MEAPI *McdLineSegIntersectFnPtr)(const McdModelID cm,
                      MeReal* const inOrig, MeReal* const inDest,
                      McdLineSegIntersectResult *outOverlap);

typedef void (MEAPI *McdDebugDrawModelAABBFnPtr)(McdModelID cm);

  /* user-controlled setting of the Request field of p */
typedef void (MEAPI *McdHelloCallbackFnPtr)(McdModelPair*);

/* declarations and functions needed by plug-in macros */

typedef
void (MEAPI *McdGeometryDestroyFnPtr)(McdGeometryID);

typedef
void (MEAPI *McdGeometryGetAABBFnPtr)(McdGeometryInstanceID, MeMatrix4Ptr finalTM, MeBool tight);

typedef
void (MEAPI *McdGeometryGetBSphereFnPtr)(McdGeometryID,
                                         MeVector3 center, 
                                         MeReal *radius );

typedef
void (MEAPI *McdGeometryMaximumPointFnPtr)(McdGeometryInstanceID,
                                           MeReal * const inDir,
                                           MeReal * const outPoint);
typedef
MeI16 (MEAPI *McdGeometryGetMassPropertiesFnPtr)(McdGeometryID,
                       MeMatrix4,
                       MeMatrix3,
                       MeReal* );
typedef
char* (MEAPI *McdGeometryGetTypeNameFnPtr)(McdGeometryID) ;

typedef void (MEAPI *McdGeometryDebugDrawFnPtr)(const McdGeometryID g, 
                                                const MeMatrix4 tm, 
                                                const MeReal colour[3]);

typedef struct
{
    McdGeometryDestroyFnPtr destroy;
    McdGeometryGetAABBFnPtr getAABB;
    McdGeometryGetBSphereFnPtr getBSphere;
    McdGeometryMaximumPointFnPtr maximumPoint;
    McdGeometryGetMassPropertiesFnPtr getMassProperties;
    McdGeometryGetTypeNameFnPtr getTypeName;
    McdGeometryDebugDrawFnPtr debugDraw;
    McdLineSegIntersectFnPtr lineSegIntersect;
    const char *name;
    int registered;
} McdGeometryVTable;


/* global collision framework structure */
struct _McdFramework
{
    int geometryRegisteredCountMax;
    int geometryRegisteredCount;

    McdModelID firstModel;
    McdGeometryID firstGeometry;

    int modelCount;
    int geometryCount;

    McdGeometryVTable *geometryVTableTable;
    McdTermActionLink *termActions;
    McdInteractions *interactionTable;

    MePool cachePool;   /* for GJK coherence */

    McdHelloCallbackFnPtr mHelloCallbackFnPtr;

    MePool modelPool;
    MePool instancePool;
    MeReal mDefaultPadding;

    MeReal mScale;              /* approximate magnitude of the objects we're dealing with */

    McdRequest *request;        /* one we allocate */
    McdRequest *defaultRequest; /* one that gets used; usually pointer points to default */
    const char *toolkitVersionString;
};

/*---------------------------------------------------------------
 * McdGeometry
 *---------------------------------------------------------------
 */

 /**
  Base class for geometries. All geometries must include this struct as their first member 
  All concrete geometrical types share a common set of functionality defined by the McdGeometry
  interface.

The McdGeometry protocol is a set of interface functions that are
implemented individually by each concrete geometry type. All functions taking a
McdGeometryID tag as argument can be passed a tag representing any concrete geometry
type that has been registered with the system. 
The McdGeometry protocol consists of the following functions:

McdGeometryDestroy(), McdGeometryGetAABB(), McdGeometryGetBSphere(),
McdGeometryMaximumPoint(), McdGeometryGetMassProperties()
McdGeometryGetTypeName()

McdModel objects, the principle objects in the Mcd system, use a
McdGeometry object to define their local geometrical properties, and may share the same geometry with other McdModel objects.

*/

struct _McdGeometry
{
  /** @internal */

    /* ref ct is in top 24 bits, ID is in bottom 8 bits */
    MeU32 mRefCtAndID;
    McdGeometryID prev,next;
    McdFrameworkID frame;
};


/*---------------------------------------------------------------
 * McdGeometryInstance
 *---------------------------------------------------------------
 */

/** Every instance of a geometry in Mcd has a geometry instance structure. */

typedef struct _McdGeometryInstance
{
    /** @internal */
    McdGeometry *mGeometry;   /**< the geometry */
    
    /** @internal */
    MeMatrix4Ptr mTM;         /**< global transformation to world coordinates */
    
    /** @internal */
    MeVector3 min;            /**< minimum corner of AABB for this time step */
    /** @internal */
    MeVector3 max;            /**< maximum corner of AABB for this time step */
    
    
    /** @internal */
    int mMaterial;
    
    struct _McdGeometryInstance *prev;
    struct _McdGeometryInstance *next;
    struct _McdGeometryInstance *parent;
    struct _McdGeometryInstance *child; // first child; children are a linked list.
} McdGeometryInstance;


/*---------------------------------------------------------------
 * McdModel
 *---------------------------------------------------------------
 */

/** @internal */
typedef void (MEAPI *McdModelUpdateFnPtr)(McdModel *model);

/** per-model intersection callback */
typedef void (MEAPI *McdModelIntersectFnPtr)(McdModel *model, McdIntersectResult *result);

  /**
    An McdModel represents simulation objects
    whose geometrical shape is
    specified by an McdGeometry. An McdModel also holds the
    transformation matrix describing its position and orientiation in 3D
    space.
 */

typedef enum {
    kMcdModelOwnRelativeTransforms = 1,
    kMcdModelNoCollide = 2
} McdModelFlags;

struct _McdModel
{
    McdFramework *frame;
    
    /** @internal */
    McdModelID prev, next;
    /** @internal */
    McdSpaceID mSpace;        /**< McdSpace this model belongs to */
    
    /** @internal */
    MeReal *linearVelocity;   /**< linear velocity of model for this time step */
    /** @internal */
    MeReal *angularVelocity;  /**< angular velocity of model for this time step */
    
    /** @internal */
    MeReal mPadding;          /**< tolerance for contact generation. */
    
    /** @internal */
    MeI32 mSpaceID;           /**< its internal indentifier in that space */
    
    /** @internal */
    MeI16 sortKey; 
    /** @internal */
    void *mBody;
    
    /** @internal */
    int mRequestID;
    
    /** @internal */
    McdModelUpdateFnPtr mTransformUpdateFn;
    
    /** @internal */
    MeMatrix4Ptr mRelTM;
    /** @internal */
    MeMatrix4Ptr mRefTM;

    /** callback pointer for per model intersect */

    McdModelIntersectFnPtr mIntersectFn;

    /** various flags of type McdModelFlags */
    MeI32 flags;

    /** geometry instance for the top-level geometry*/

    McdGeometryInstance mInstance;

    /** @internal */
    void *mData;              /**< user data */
    
};


/*----------------------------------------------------------------
 *  McdModelPair
 *----------------------------------------------------------------
 */
typedef enum {
    kMcdFFStateInactive,     
    kMcdFFStateHello,
    kMcdFFStateStaying,
    kMcdFFStateGoodbye
} McdModelPairPhase;

struct _McdModelPair
{

  /** @internal */
  McdModelID model1;
  /** @internal */
  McdModelID model2;

  /** @internal */
  McdModelPairPhase phase;

  /** @internal */
  McdRequest *request;

  /** @internal */
  void *userData;

  /** @internal */
  void *m_cachedData;

  /** @internal */
  void *responseData;

};


/*----------------------------------------------------------------
 * McdModelPairContainer
 *---------------------------------------------------------------
 */

  /**
     An array of references to McdModelPair structures, in which three
     "nearby states" are distinguished:
     "hello", "staying" and "goodbye".

     The array is partioned into three index ranges, one for each of the
     distinct states:

     goodbye pairs:
     i = goodbyeFirst .. goodbyeEnd-1

     hello pairs:
     i = helloFirst .. helloEndStayingFirst - 1

     staying pairs:
     i = helloEndStayingFirst .. stayingEnd - 1

     This index scheme allows all "current pairs" ( hello +
     staying) to be iterated through in single loop using the index range
     i = helloFirst .. stayingEnd -1

   */

struct _McdModelPairContainer
{
  McdModelPair **array;  /**< array of McdModel pairs */
  int size;              /**< number of elements in @a array */

  /** index range for "goodbye" pairs is:
      for( i = goodbyeFirst ; i < goodbyeEnd; ++i )
  */

  int goodbyeFirst;

  /** index range for "goodbye" pairs is:
      for( i = goodbyeFirst ; i < goodbyeEnd; ++i )
  */

  int goodbyeEnd;

  /** index range for "hello" pairs is:
      for( i = helloFirstIndex; i < helloEndStayingFirst ; ++i )
  */

  int helloFirst;

  /**  End of hello pairs, first of staying pairs.
       This index scheme allows all "current pairs" ( hello +
       staying) to be iterated through in single loop using the index range
       i = helloFirst .. stayingEnd -1

  */

  int helloEndStayingFirst;

  /** index range for "staying" pairs is:
      for( i = helloEndStayingFirst; i < stayingEnd ; ++i )
  */

  int stayingEnd;

};

/*----------------------------------------------------------------
 * McdModelPairContainerIterator
 *---------------------------------------------------------------
 */

  /** An iterator for the contents of a McdModelPairContainer.
      Currently used only in McdIntersectEach().

      @see McdIntersectEach
  */

struct _McdModelPairContainerIterator
{
  /** @internal */
  McdModelPairContainer *container;
  /* contacts generated beginning with the last staying index,
     down until first hello index */
  /** @internal */
  int count;

};



/*----------------------------------------------------------------
 *  McdContact
 *----------------------------------------------------------------
 */

/** Geometric contact point information, typically between two McdModel
    objects. An array of these structs is associated with a given
    McdModelPair via the McdIntersectResult struct, and computed via
    McdIntersect();

    Some of the information in the McdContact struct depends on the order of the @arg
    model1 and @arg model2 fields of the associated McdModelPair.
    @see McdModelPair, McdIntersectResult, McdIntersect
*/


struct _McdContact
{
  /** position, in world coordinates, of the point of contact */
    MeVector3 position;
  /** surface normal at the contact position. Sign chosen so as to point
      from @arg model1 to @arg model2 in the associated McdModelPair. */
    MeVector3 normal;
  /** separation between objects. Negative if there is penetration */
    MeReal separation;
#ifdef MCD_CURVATURE
  /** local curvature of @arg model1's surface at the contact position */
    MeReal curvature1;
  /** local curvature of @arg model2's surface at the contact position */
    MeReal curvature2;
#endif
  /** dimensional characterisation of the contact. Possible values are  0,
      1 and 2, corresponding to  point, line and surface, respectively */
  short dims;
  /** auxiliary data. Used by some intersection functions. */
  union {void *ptr; int tag;} element1;
  /** auxiliary data. Used by some intersection functions. */
  union {void *ptr; int tag;} element2;
};


/*----------------------------------------------------------------
 * McdSafeTimeResult
 *---------------------------------------------------------------
 */

/** SafeTime query result. Might be merged with IntersectResult */
struct _McdSafeTimeResult 
{
  McdModelPair *pair; /**< pair involved in SafeTime query */
  MeReal time; /**< estimated time of contact given linear and angular
                    velocities */
  /* int touch;   **< if 1, indicates that collision has already occured.
            However if the velocities are small, the SafeTime
            functions exit without testing for collision, so
                    touch might might be 0, even if the objects are colliding.
            Use McdIntersect to determine colliding state.
        */

};

/*----------------------------------------------------------------
 * struct McdIntersectResult
 *---------------------------------------------------------------
 */

 /** Intersection query data. Output format for McdIntersect().
    @see McdIntersect
 */

struct _McdIntersectResult
{
    McdModelPair *pair;
    McdContact *contacts;           /**< array of contacts to be filled */
    int contactMaxCount;            /**< size of array */
    int contactCount;               /**< number of contacts returned in array */
    int touch;                      /**< 1 if objects are in contact, 0 otherwise */
    MeReal normal[3];               /**< average normal of contacts returned */
    void *data;                     /**< auxiliary data */
};


/*----------------------------------------------------------------
 * struct McdLineSegIntersectResult
 *---------------------------------------------------------------
 */

struct _McdLineSegIntersectResult 
{
  McdModelID model;           /**< Collision model intersecting with the line segment */
  MeVector3  position;        /**< Intersection point */
  MeVector3  normal;          /**< Model normal at intersection point */
  MeReal     distance;        /**< Distance from the first end point of line segment
                                   to the intersection point. */
};

/*----------------------------------------------------------------
 * McdRequest
 *---------------------------------------------------------------
 */

  /** A McdRequest structure provides parameters that are used to characterise
      how various interaction algorithms are carried out.
      Each McdModelPair object points to a McdRequest structure,
      and all interactions take a McdModelPair object as first argument.
  */

struct _McdRequest
{
  /** The maximum number of McdContact objects to be produced when using
      McdIntersect(). This number represents only a target: the algorithm may
      fail to acheive this target, or may ignore it altogether. See the
      individual documentation for each geotype-geotype intersect function.
      To set a limit on the number of contacts that is guaranteed to be
      respected, use the contactMaxCount field of McdIntersectResult
      instead.
  */

  int contactMaxCount;

  /** When computing the normal for a given McdContact object,
      many algorithms simply use the normal from the surface at the point
      of contact. There are two surfaces to choose from, the first model or
      the second model. This parameter tells the algorithm to prefer using
      the surface that corresponds to a face, if there is one.
   */

  int faceNormalsFirst;

};





/*----------------------------------------------------------------
 * struct McdInteractions
 *---------------------------------------------------------------
 */

typedef MeBool (MEAPI *McdHelloFn)(McdModelPair*);
typedef void   (MEAPI *McdGoodbyeFn)(McdModelPair*);
typedef int    (MEAPI *McdIntersectFn)(McdModelPair*, McdIntersectResult* );
typedef int    (MEAPI *McdSafeTimeFn)(McdModelPair*, MeReal maxTime, McdSafeTimeResult*);


struct _McdInteractions
{
  McdHelloFn helloFn;
  McdGoodbyeFn goodbyeFn;

  McdIntersectFn intersectFn;
  McdSafeTimeFn safetimeFn;

  MeBool swap;
  MeBool cull;
  MeBool warned;

};


/*----------------------------------------------------------------
 * McdDistanceResult
 *---------------------------------------------------------------
 */

/** Distance query data. Output structure for McdDistance().
    @see McdDistance
 */
typedef struct McdDistanceResult
{
    McdModelPair *pair;   /**< pair of models */
    MeReal distanceLB;    /**< lower bound on distance, same as distanceUB for exact distance */
    MeReal distanceUB;    /**< upper bound on distance */
    MeVector3 point1;    /**< point on model 1 realizing this distance, valid only for exact distance */
    MeVector3 point2;    /**< point on model 2 realizing this distance, valid only for exact distance */

  /** auxiliary data. Used by McdTriangleMesh distance function to return triangle index on closest triangle on model 1. */
  union {void *ptr; int tag;} element1;
  /** auxiliary data. Used by McdTriangleMesh distance function to return triangle index on closest triangle on model 2. */
  union {void *ptr; int tag;} element2;

} McdDistanceResult;


/** a callback allowing the user to deal with a "pool full" situation  - should really be in commonTypes*/

typedef
void (MEAPI *McdSpacePoolErrorFnPtr)(McdModelID model1, McdModelID model2);


typedef MeI16 McdGeometryType;

#ifdef __cplusplus
}
#endif

#endif
