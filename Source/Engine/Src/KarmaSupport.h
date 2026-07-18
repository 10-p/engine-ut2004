/*=============================================================================
	KarmaSupport.h

    Engine-internal Karma Integration Functions
=============================================================================*/
#ifndef KARMA_SUPPORT_H
#define KARMA_SUPPORT_H
#include "KDebugDraw.h"

const int K_collisionModelsMaxCount = 2500;

#define ME_GRAVSCALE			((MeReal)1)

/* Karma integration global data. */
typedef struct _KarmaGlobals
{
    /* Used for batching/displaying Karma debug lines */
    //KLineBatcher* DebugLines;

    /* Temporary storage for querying/colliding triangles. */
    KarmaTriListData TriListData;

    /* We need one McdFramework throughout lifetime to persist McdGeometry. */
    McdFrameworkID Framework;
    
    /* This holds all mefiles loaded */
    MeAssetDB* AssetDB;

    /*  This acts as a 'cache' for McdGeometrys. 
        Also use to keep track of total number of McdModels and McdGeometrys */
    McdGeomMan* GeomMan;

    /* Used by memory manager to mark where allocations occur. */
    TCHAR SectionName[256];

    /* We should shutdown GameKarma when possible. */
    MeBool bShutdownPending;

    MeReal TimeStep;

    /* Debug-draw flags. */
    int DebugDrawOpt; /* SEE EKDebugDrawOptions */

    MeBool bDoTick;
    MeBool bAutoEvolve;

	MeBool bUseSafeTime;
	MeBool bDisableTriCollision; // only for testing safe-time and such...
	MeBool bDebugContacts;	

	int ModelCount;
	int GeometryCount;

	// Temporary model pair container, for filtering pairs.
	McdModelPairContainer* filterPairs;

	// Temporary array of triangle collision indices.
	// Used when querying staticmeshes to find nearby triangles in KTriListQuery.
	TArray<INT> StaticMeshTris;

	// Simulation constants.
	FKSimParams	SimParams;
} KarmaGlobals;



// Useful in a few places...
#define RTN_WITH_ERR_IF(test, error) {if(test) {debugf(TEXT(error)); return;}}

extern ENGINE_API KarmaGlobals* KGData; // sjs

const MeReal K_ME2UMassScale = (MeReal)100;
const MeReal K_U2MEMassScale = (MeReal)0.01;

void    MEAPI KLineDraw(MeVector3 start, MeVector3 end, MeReal r, MeReal g, MeReal b);
void    MEAPI KSetSecName(TCHAR* name);


/* KTODO: Maybe make C++ methods. This way keeps them separate though. */
void    MEAPI KInitGameKarma();
void    ENGINE_API KTermGameKarma(); // sjs - exported this for deca temp...

void    MEAPI KInitLevelKarma(ULevel* level);
void    MEAPI KTermLevelKarma(ULevel* level);

void    MEAPI KTickLevelKarma(ULevel* level, FLOAT DeltaSeconds);

void    MEAPI KInitActorCollision(AActor* actor, UBOOL makeNull);
void    MEAPI KTermActorCollision(AActor* actor);

void    MEAPI KInitActorDynamics(AActor* actor);
void    MEAPI KTermActorDynamics(AActor* actor);

void    MEAPI KInitActorKarma(AActor* actor);
void    MEAPI KTermActorKarma(AActor* actor);

void    MEAPI KInitConstraintKarma(AKConstraint* con);
void    MEAPI KTermConstraintKarma(AKConstraint* con);

void    MEAPI KInitSkeletonKarma(USkeletalMeshInstance* inst);
void    MEAPI KTermSkeletonKarma(USkeletalMeshInstance* inst);

void	MEAPI KTermStaticMeshCollision(UStaticMesh* smesh);

void    MEAPI KModelDestroy(McdModelID model);
void    MEAPI KGeometryDestroy(McdGeometryID geom);

UBOOL   MEAPI KExecCommand(const TCHAR* Cmd, FOutputDevice* Ar);

void	MEAPI KCheckActor(AActor* actor);

AActor*	MEAPI KBodyGetActor(MdtBodyID body);
AActor*	MEAPI KModelGetActor(McdModelID model);

void	MEAPI KUpdateJoined(AActor* actor, UBOOL newTag);

void	MEAPI KSetActorCollision(AActor* actor, UBOOL newBlock);

void		  KSetSkelVel(USkeletalMeshInstance* inst, FVector Velocity, FVector AngVelocity);
void		  KScaleJointLimits(USkeletalMeshInstance* inst, FLOAT Scale, FLOAT Stiffness);

/* *** INTEGRATION INTERNAL *** */

// Farfield stuff
KModelPairType KModelsToKey(McdModelID m1, McdModelID m2);

void		  KUpdateContacts(TArray<AActor*> &actors, ULevel* level, UBOOL bDoubleRateActors);
void          KHandleCollisions(McdModelPairContainer* pairs, ULevel* level);

void		  KHelloModelPair(McdModelPairID pair, ULevel* level);
void		  KGoodbyeModelPair(McdModelPairID pair, ULevel* level);
void          KGoodbyePair(McdModelID model1, McdModelID model2, ULevel* level);
void		  KGoodbyeAffectedPairs(McdModelID model, ULevel* level);
void		  KGoodbyeActorAffectedPairs(AActor* actor);

void		  KEnablePairCollision(McdModelID m1, McdModelID m2, ULevel* level);
void		  KDisablePairCollision(McdModelID m1, McdModelID m2, ULevel* level);

void		  KActorContactGen(AActor* actor, UBOOL gen);

MeBool		  KBatchIntersectEach(McdBatchContext *context,
                      McdModelPairContainer *pairs,
                      McdIntersectResult* resultArray, 
                      int *resultCount,
                      int resultMaxCount,
                      McdContact* contactArray,
                      int *contactCount,
                      int contactMaxCount);

// Trilist stuff
void		  KUpdateRagdollTrilist(AActor* actor, UBOOL bDoubleRateActors);
void	MEAPI KTriListQuery(ULevel* level, FSphere* sphere, KarmaTriListData* triData);
int		MEAPI KTriListGenerator(McdModelPair* modelTriListPair,
								McdUserTriangle *triangle,
								MeVector3 pos, 
								MeReal radius,
								int maxTriangles);

// Asset DB
void    MEAPI KCreateAssetDB(MeAssetDB** db, McdGeomMan** gm);
McdModelID MEAPI KModelCreateFromMeFAssetPart(MeFAssetPart *part, McdGeometryID g, MdtWorldID world, MeMatrix4Ptr assetTM);

// Other
void    MEAPI KWorldStepSafeTime(MdtWorldID w, MeReal stepSize, ULevel* level, UBOOL bDoubleRateActors, UBOOL bFinalSubStep);

void		  KBodySetInertiaTensor(const MdtBodyID b, const MeMatrix3 i);
void		  KBodySetMass(const MdtBodyID b, const MeReal mass);
void		  KGMDestroyGeometry(McdGeomMan* gm, McdGeometryID geom);

void	MEAPI KLevelDebugDrawConstraints(const ULevel* Level);

void    MEAPI KFindNearestActorBody(AActor* actor, MeVector3 pos, FName boneName, MdtBodyID* ab, McdModelID* am);

void		  KBodyTermKConstraints(MdtBodyID body);

FName		  KBoneNameFromElementIndex(USkeletalMesh* smesh, INT ElementIndex);
UBOOL		  KShouldStopKarma(AActor* actor);
void		  KCalcModelSafeTimeSpheres(McdModelID model);

void		  KTermSVehicleDynamics(ASVehicle* v);

MeBool		  KBoxTriangleListIntersect( McdModelPair* p, McdIntersectResult *result );

// Memory Handlers
void*   MEAPI KMemCreate(size_t bytes);
void    MEAPI KMemDestroy(void *const block);
void*   MEAPI KMemResize(void *const block, size_t bytes);
void*   MEAPI KMemCreateZeroed(size_t bytes);

void	MEAPI KPoolMallocInit(MePool* pool, int poolSize, int structSize, int alignment);
void	MEAPI KPoolMallocDestroy(MePool* pool);
void	MEAPI KPoolMallocReset(MePool* pool);
void*	MEAPI KPoolMallocGetStruct(MePool* pool);
void	MEAPI KPoolMallocPutStruct(MePool* pool, void* s);
int		MEAPI KPoolMallocGetUsed(MePool* pool);
int		MEAPI KPoolMallocGetUnused (MePool* pool);

// Message Handlers
void    MEAPI KMessageShow(const int level,const char *const string);
void    MEAPI KDebugHandler(const int level, const char *const format,va_list ap);
void    MEAPI KErrorHandler(const int level, const char *const format,va_list ap);

#endif
