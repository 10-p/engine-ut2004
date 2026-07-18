#ifndef _MDTTYPES_H
#define _MDTTYPES_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/16 10:20:40 $ - Revision: $Revision: 1.141.2.6 $

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
    Declares all the C-specific data structures used by Mdt/MdtBcl.
*/

#include <MePrecision.h>
#include <MdtBcl.h>
#include <MePool.h>
#include <MeChunk.h>
#include <MeDict.h>
#include <MdtDefaults.h>

typedef struct MdtWorld          MdtWorld;
typedef struct MdtBody           MdtBody;

typedef struct MdtConstraintHeader  MdtConstraintHeader;
typedef struct MdtContact           MdtContact;
typedef struct MdtContactGroup      MdtContactGroup;
typedef struct MdtBSJoint           MdtBSJoint;
typedef struct MdtHinge             MdtHinge;
typedef struct MdtPrismatic         MdtPrismatic;
typedef struct MdtCarWheel       MdtCarWheel;
typedef struct MdtFixedPath      MdtFixedPath;
typedef struct MdtRPROJoint      MdtRPROJoint;
typedef struct MdtUniversal      MdtUniversal;
typedef struct MdtSkeletal       MdtSkeletal;
typedef struct MdtLinear1        MdtLinear1;
typedef struct MdtLinear2        MdtLinear2;
typedef struct MdtAngular3       MdtAngular3;
typedef struct MdtSpring6        MdtSpring6;
typedef struct MdtSpring         MdtSpring;
typedef struct MdtConeLimit      MdtConeLimit;
typedef struct MdtUserConstraint MdtUserConstraint;
typedef struct MdtBaseConstraint MdtBaseConstraint;

typedef struct MdtBclSingleLimit        MdtBclSingleLimit;
typedef struct MdtBclLimit              MdtBclLimit;
typedef struct MdtBclSolverParameters   MdtBclSolverParameters;
typedef struct MdtBclContactParams      MdtBclContactParams;

typedef struct MdtPartitionOutput   MdtPartitionOutput;
typedef struct MdtWorldParams       MdtWorldParams;
typedef struct MdtPartitionParams   MdtPartitionParams;
typedef struct MdtPartitionInfo     MdtPartitionInfo;
typedef struct MdtLODParams         MdtLODParams;

/** MdtWorld identifier */
typedef MdtWorld                 *MdtWorldID;
/** MdtBody identifier */
typedef MdtBody                  *MdtBodyID;
/** MdtContact identifier */
typedef MdtContact               *MdtContactID;
/** MdtContactGroup identifier */
typedef MdtContactGroup          *MdtContactGroupID;
/** MdtBSJoint ball and socket joint identifier */
typedef MdtBSJoint               *MdtBSJointID;
/** MdtHinge identifier */
typedef MdtHinge                 *MdtHingeID;
/** MdtPrismatic identifier */
typedef MdtPrismatic             *MdtPrismaticID;
/** MdtCarWheel identifier */
typedef MdtCarWheel              *MdtCarWheelID;
/** MdtFixedPath identifier */
typedef MdtFixedPath             *MdtFixedPathID;
/** MdtRPROJoint relative position relative orientation identifier */
typedef MdtRPROJoint             *MdtRPROJointID;
/** MdtUniversal identifier */
typedef MdtUniversal             *MdtUniversalID;
/** MdtSkeletal identifier */
typedef MdtSkeletal              *MdtSkeletalID;
/** MdtLinear1 identifier */
typedef MdtLinear1               *MdtLinear1ID;
/** MdtLinear2 identifier */
typedef MdtLinear2               *MdtLinear2ID;
/** MdtAngular3 identifier */
typedef MdtAngular3              *MdtAngular3ID;
/** MdtSpring6 identifier */
typedef MdtSpring6               *MdtSpring6ID;
/** MdtSpring identifier */
typedef MdtSpring                *MdtSpringID;
/** MdtConeLimit identifier */
typedef MdtConeLimit             *MdtConeLimitID;
/** MdtUserConstraint identifier */
typedef MdtUserConstraint        *MdtUserConstraintID;
/** MdtBclLimit identifier */
typedef MdtBclLimit              *MdtLimitID;
/** MdtBclSingleLimit identifier */
typedef MdtBclSingleLimit        *MdtSingleLimitID;
/** MdtBclContactParams contact parameters identifier */
typedef MdtBclContactParams      *MdtContactParamsID;

/**
    General MdtConstraint identifier.
    Use appropriate 'Qua' function to convert a specific type of constraint
    (eg. MdtContactID or MdtHingeID) to an MdtConstraintID.
*/
typedef MdtBaseConstraint        *MdtConstraintID;

/* 'Virtual Function' prototypes */
typedef void (MEAPI *MdtConstraintSetBodyFnPtr)(const MdtConstraintID c,
                         const MdtBodyID b1, const MdtBodyID b2);
typedef void (MEAPI *MdtConstraintSetAxisFnPtr)(const MdtConstraintID c,
                         const MeReal px, const MeReal py, const MeReal pz);


/* Callbacks */
typedef void (MEAPI *MdtPartitionEndCB)(MdtPartitionOutput* po, void* pcbdata);
typedef void (MEAPI *MdtConstraintIteratorCBPtr)(const MdtConstraintID c, void* ccbdata);
typedef void (MEAPI *MdtBodyCallbackCBPtr)(const MdtBodyID b);
typedef void (MEAPI *MdtContactGroupDestroyCallbackCBPtr)(const MdtContactGroupID c);
typedef void (MEAPI *MdtSimErrorCBPtr)(MdtKeaConstraints* kc, MdtKeaBody** kb, int nBodies, void* secbdata);

/**
 *  Options for displaying debug info using the MeDebugDraw functions.
 *  @see MdtWorldSetDebugDrawing
 */
typedef enum
{
    /** 
     *  Draw red lines indicating position/normal/penetration of contacts.
     *  Note - contacts are drawn _after_ any max matrix size operations.
     */
    MdtDebugDrawContacts = 0x001,

    /**
     *  Draw green lines indicating the force generated at each contact.
     */
    MdtDebugDrawContactForce  = 0x002
} MdtDebugDrawOptions;

/** 
 *  Flags on constraints and bodies. The first says the thing is enabled, 
 *  that is, should be simulated this timestep, although this is actually 
 *  irrelevant for constraints. The second says whether it's beed added by the 
 *  partitioner this timestep, it should therefore be cleared on all bodies 
 *  before the partitioner is run 
 */
typedef enum 
{
    MdtEntityEnabledFlag =              0x01,
    MdtEntityAddedFlag =                0x02,
    MdtEntityEnabledByPartitioner =     0x04
    /* next one must be 0x08 */
} MdtConstraintHeaderFlags;

/**
    MdtBcl Single Limit structure.
    
    Specifies behaviour of the constraint at a specific individual limit
    (either an upper or lower limit) of a motion which is otherwise
    unconstrained by the joint's hard constraints.
*/
struct MdtBclSingleLimit
{
    /**
     * Minimum (for lower limit) or maximum (for upper limit) linear or
     * angular separation of the attached bodies, projected onto the
     * relevant axis.
     *
     * For a soft limit, the stop is a boundary rather than an absolute
     * limit.
     */
    MeReal stop;

    /**
     * The spring constant (kp) used for restitution force when a limited
     * joint reaches one of its stops.
     *
     * This limit property must be zero or positive: the default value is
     * MEINFINITY.  If the stiffness and damping of an individual limit are
     * both zero, it is effectively deactivated.
     */
    MeReal stiffness;

    /**
     * The damping term (kd) for this limit.
     *
     * This must not be negative: the default value is zero.  This property
     * is used only if the limit hardness is less than or equal to the
     * damping threshold.  If the hardness and damping of an individual
     * limit are both zero, it is effectively deactivated.
     */
    MeReal damping;

    /**
     * The ratio of rebound velocity to impact velocity
     * when the joint reaches the low or high stop.
     *
     * This is used only if the limit hardness exceeds the damping
     * threshold.  Restitution must be in the range zero to one inclusive:
     * the default value is one.
     */
    MeReal restitution;
};

/**
    MdtBcl Limit structure.
 
    Specifies behaviour of the constraint for a motion which is otherwise
    unconstrained by the joint's hard constraints.  This motion may be
    constrained at either an upper limit or a lower limit, or both, or
    actuated (powered) by a force-limited motor between such limits or in the
    absence of limits.
*/
struct MdtBclLimit
{
    /**
     * Set if the corresponding degree of freedom of the joint is limited.
     *
     * Cleared (set 0) if the degree of freedom is not limited.
     */
    MeBool bLimited;

    /**
     * Set if the coordinate in the corresponding degree of freedom of the
     * joint is to be calculated.  Must always be set if the "limited"
     * flag is set.
     *
     * Cleared (set 0) if the coordinate is not calculated (or limited).
     */
    MeBool bCalculatePosition;

    /**
     * Set if the "position" attribute has been calculated at least once.
     *
     * Cleared (set 0) on creation.
     */
    int bPositionInitialised;

    /**
     * Non-zero if the joint has overshot a limit and is
     * relaxing back to that limit, zero if bouncing.
     */
    int bRelaxingToLimit;

    /**
     * The overshoot distance.
     *
     * This is set to zero between the limits; otherwise it is the amount by
     * which the joint has overshot its limit.  This is positive beyond the
     * upper limit, negative beyond the lower.
     */
    MeReal overshoot;

    /**
     * Relative position of the attached bodies, projected onto the
     * relevant axis.
     */
    MeReal position;


    /**
     *  user specified position lock: when set and is_locked is  TRUE, the
     *  constraint will maintain the joint coordinate specified in
     *  position_lock.
     *
     */
     MeReal position_lock;


    /**
     *  A switch to apply the lock or not.  This should always be false when
     *  bPowered is true and vice versa.
     */
    MeBool is_locked;

    /**
     * Relative position of the attached bodies in the previous time-step.
     */
    MeReal previous_position;

    /**
     * An offset used to transform the measured relative position
     * coordinate ("position") into the user's coordinate system.
     * This member variable (and not "position") is set by a call
     * to MdtLimitSetPosition.
     */
    MeReal offset;

    /**
     * Relative linear or angular velocity of the attached
     * bodies, projected onto the relevant axis.
     */
    MeReal velocity;

    /**
     * The limit hardness threshold.
     *
     * This value depends on the timestep and is recalculated in each
     * timestep in which a limit is exceeded.  When a limit hardness exceeds
     * this value, damping is ignored and only the restitution property is
     * used.  When the limit hardness is at or below this threshold,
     * restitution is ignored, and the hardness and damping terms are used
     * to simulate a damped spring.
     */
    MeReal damping_thresh;

    /**
     * The specific properties of the lower and upper limits.
     */
    MdtBclSingleLimit limit[2];

    /*
      Power.
    */

    /**
     * Zero for a non-powered joint coordinate, non-zero for a powered coordinate.
     */
    MeBool bPowered;

    /**
     * Powered joint, desired velocity.
     *
     * A lower limiting velocity may be achieved if the attached bodies are
     * subject to velocity or angular velocity damping.
     */
    MeReal desired_vel;

    /**
     * Powered joint, maximum force that should be
     * applied to reach the desired velocity.
     */
    MeReal fmax;
};

struct MdtPartitionInfo
{
    /* Number of actual constraint rows (upper bound) in this partition. */
    int     rowCount;

    /* Number of actual contacts (not contact groups) in this partition. */
    int     contactCount;

    /* Number of joints (ie. non-contact constraints) in this partition. */
    int     jointCount;
};

/**
    Output from the partitioner.
    Contains 'partitions' of bodies and constraints that can be simulated
    independently.
*/
struct MdtPartitionOutput
{
    /** Constraint pointers to be processed, grouped into partitions. */
    MdtBaseConstraint       **constraints;

    /** The index of the first constraint in each partition. */
    int                 *constraintsStart;

    /** The number of constraints in each partition. */
    int                 *constraintsSize;

    /** Total number of constraints. */
    int                 totalConstraints;

    /** Maximum number of constraints (ie size of 'constraints' array). */
    int                 maxConstraints;	



    /** Body pointers to be processed, grouped into partitions. */
    MdtBody             **bodies;

    /** The index of the first body in each partition. */
    int                 *bodiesStart;

    /** The number of bodies in each partition. */
    int                 *bodiesSize;

    /** Total number of bodies. */
    int                 totalBodies;

    /** Maximum number of bodies (ie size of 'bodies' array). */
    int                 maxBodies;


    /** Other information about each partition. */
    MdtPartitionInfo    *info;


    /** Number of partitions. */
    int                 nPartitions;

    /** Maximum number of partitions (ie size of 'Start and 'Size arrays) */
    int                 maxPartitions;

    /** Sum of all elements of 'info' array. */
    MdtPartitionInfo    overallInfo;
};

/** Global parameters for a world (gravity, epsilon etc.) */
struct MdtWorldParams
{
    /** Gravity vector. */
    MeVector4           gravity;

    /* Matrix size logging. Should this be somewhere else? */

    /** Used for (optional) logging of matrix sizes. */
    int* matrixSizeLog;

    /** Size of the matrix size log. */
    int matrixSizeLogSize;    

    /** simulation default length parameter (used to scale auto-disable and density) */

    MeReal lengthScale;

    /** simulation default mass parameter (used to scale default epsilon and density, and
        default mass and inertia)
    */
    MeReal massScale;

    /** simulation default density, used by MstAutoSetMassProperties. This is computed as
        massScale / (lengthScale)^3 */

    MeReal defaultDensity;

};

struct MdtLODParams
{
    /* Partition-LOD parameters */

    /** Friciton/frictionless ratio to try and have in group. */
    MeReal              frictionRatio;
    /** Bonus importance for groups with nothing in. */
    MeReal              zeroRowBonus;
    /** Bonus importance for groups to world. */
    MeReal              toWorldBonus;
    /** Decrease in importance with each row added. */
    MeReal              rowCountBias;
    /** Average contact penetration multiplier. */
    MeReal              penetrationBias;
    /** RMS velocity multiplier. */
    MeReal              normVelBias;
    /** Extra importance for constraints to things not just auto-enabled
        by the partitioner. */
    MeReal              nonAutoBonus;

};

/**
    Threshold values for consider a body as 'stopped'.
*/
struct MdtPartitionParams
{
    /** 
     *  Flag to indicate whether bodies should be 'disabled' (ie. no longer
     *  simulated) when they are judged to have come to rest using the 
     *  velocity/acceleration threshold values.
     */
    MeBool              autoDisable;

    /** Sum-squared velocity threshold. */
    MeReal              vel_thresh;
    /** Sum-squared rotational velocity threshold. */
    MeReal              velrot_thresh;
    /** Sum-squared acceleration threshold. */
    MeReal              acc_thresh;
    /** Sum-squared rotation acceleration threshold. */
    MeReal              accrot_thresh;

    /**
     * Minimum number of steps to keep an object alive for.
     *
     * This is needed to give an object enough time to gain
     * a minimum velocity after it has been awakened.
     */
    MeReal              alive_time_thresh;

    /* Matrix size-capping parameters. */

    /** Maximum matrix size allowed. */
    int                 maxMatrixSize;

    /** Parameters used for re-sizing partitions larger than maxMatrixSize. */
    MdtLODParams        lodParams;

    /* Debug info */

    /** Current debug drawing options. */
    MdtDebugDrawOptions debugOptions;
};

/**
 * World Struct contains simulation-wide parameters.
 *
 * vel_thresh, velrot_thresh, acc_thresh, accrot_thresh and alive_time_thresh
 * are used for determining when bodies are at rest. A body at rest is
 * not processed by the solver till reenabled by a force or collision
 * event. A body is disabled if it falls below all of the 4 threshhold
 * values.

 * force_thresh and torque_thresh are threshholds for waking up resting
 * bodies.  If the force on a body is more than the force that was
 * applied to it when it was disabled plus the force threshold, then the
 * body is awakened.
 */

struct MdtWorld
{
    /** Mdt body list. */
    MeDict              bodyDict;

    /** Mdt Constraint list for all enabled constraints. */
    MeDict              constraintDict;

    /** List of enabled bodies. */
    MeDict              enabledBodyDict;

    /** Memory pool used by Kea when solving systems. */
    MeChunk             keaPool;

    /** Max number of bodies allowed in this world. */
    int                 maxBodies;
    /** Number of bodies in existence (enabled and disabled). */
    int                 nBodies;
    /** Total count of enabled bodies in the world*/
    int                 nEnabledBodies;

    /** Memory pool to get bodies from. */
    MePool              bodyPool;

    /** Temporary memory for array of kea transforms. */
    MeChunk             keaTMChunk;

    /** Maximum number of constraints allowed in this world. */
    int                 maxConstraints;
    /** Total count of enabled constraints in the world. */
    int                 nEnabledConstraints;

    /** Memory pool to get constraints from. */
    MePool              constraintPool;
    /** Memory pool to get constraint list nodes. */
    MePool              nodePool;

    /** Temporary memory for kea constraints structure passed to Kea. */
    MeChunk             keaConstraintsChunk;

    /**
     *  Temporary data used for storing partitioned state of the world,
     *  and eventually passing it to Kea, via BCL.
     */
    MeChunk             partOutChunk;

    /** World simulation parameters (gravity etc.) */
    MdtWorldParams      params;

    /** Constant (ie. non-timestep scaled) gamma projection constant. */
    MeReal              constantGamma;

    /** 
     *  If using MdtWorldStepSafeTime, this is the minimum amount that a body 
     *  will be stepped by, regardless of what the safetime for a body is.
     */
    MeReal              minSafeTime;

    /**
     * Parameters used by the end-of-partition function.
     * Includes auto-disable thresholds and max-matrix size info.
     */
    MdtPartitionParams  partitionParams;

    /**
     * Callback called whenever a Body is Enabled.
     * This is used by the MstBridge to unfreeze collision etc.
     */
    MdtBodyCallbackCBPtr   bodyEnableCallback;

    /**
     * Callback called whenever a Body is Disabled.
     * This is used by the MstBridge to freeze collision etc.
     */
    MdtBodyCallbackCBPtr   bodyDisableCallback;

    /**
     * Callback called whenever a contact group is destroyed
     * This is used by the MstBridge to clean up references to the
     * contact group.
     */
    MdtContactGroupDestroyCallbackCBPtr   contactGroupDestroyCallback;

    /** Version string for the MathEngine toolkit. */
    const char *toolkitVersionString;

    /** Parameters used by Kea when solving a physical system. */
    MdtKeaParameters        keaParams;


    MdtSimErrorCBPtr        simErrorCallback;
    MeBool                  checkSim;
    void*                   simErrorUserData;
};


/**
 * MdtBody represents a physical body in a world simulation.
 * NB. Must be able to cast from an MdtBody to a MdtKeaBody.
 */
struct MdtBody
{
    /** Kea body data, MUST be first in structure. */
    MdtKeaBody           keaBody;

    /** Transform of the Center of Mass */
    MeMatrix4            comTM;

    /** Transform of the body (given by MdtBodyGetTransform etc.) */
    MeMatrix4            bodyTM;

    /**
     * Linear Impulse accumulator.
     * Converted to force and added to force accumulator at step time
     */
    MeVector4           impulseLinear;

    /**
     * Angular Impulse accumulator.
     * Converted to torque and added to torque accumulator at step time
     */

    MeVector4           impulseAngular;

    /* center of mass in body's reference frame */
    MeVector3            com;

    /** Update bodyTM using comToBodyTM. */
    MeBool               useCom;

    /**
     * Flag to indicate an impulse has been added to this body this
     * time-step.
     */
    MeBool              impulseAdded;

    /** The world the body is in. */
    MdtWorldID          world;

    /** Sort key for dictionaries */
    MeI32               sortKey;
    /** dict node in world dict of all bodies. */
    MeDictNode             worldNode;

    /** dict node in world dict of enabled bodies. */
    MeDictNode             worldEnabledNode;

    /** User data. This will not be changed from within the toolkit */
    void                *userData;

    /**
     * Integer indicating the partition to which this body belongs.
     * This is updated by re-partitioning the world.
     */
    int                 partitionIndex;

    /** Index to corresponding MdtKeaBody in whole world array passed into Kea. */
    int                 arrayIdWorld;

    /** Index to corresponding MdtKeaBody in just its partitions array passed into Kea. */
    int                 arrayIdPartition;

    /** Flag used in partitioner to indicate body has not yet been added. */
    MeI32              flags;

    /** Mass in kilogrammes. */
    MeReal              mass;

    /** Velocity damping: 0 for no damping. */
    MeReal              damping;
    /** Angular velocity damping: 0 for no damping. */
    MeReal              angularDamping;

    /** A body's constraints may include joints or contacts. */
    MeDict              constraintDict;

    /**
     * Number of steps that a body has been enabled; -1 if object is
     * disabled.
     */
    MeReal              enabledTime;

    /** associated collision model */
    void                *model;

    /** safe time to evolve body. */
    MeReal              safeTime;

    /* INTERNAL */
    int                 LODpartIndex;
};



/*
 * Constraint Library Contact Parameters.
 *
 * Definitions of the MdtBclContactParam struct used by MdtContact.
 * This includes friction, restitution, softness, slip, surface velocity etc.
 */


/**
    Contact friction type enum.
*/
typedef enum
{
    /** Frictionless contact. */
    MdtContactTypeFrictionZero,

    /** Friction only along primary direction. */
    MdtContactTypeFriction1D,

    /** Friction in both directions. */
    MdtContactTypeFriction2D,

    /** Invalid contact type. */
    MdtContactTypeUnknown = -1
}
MdtContactType;



/**
    Contact options enum.
 
    Contact options such as friction (slipperiness) and restitution
    (bounciness).  Any bitwise combination of these options is valid for use
    in 'MdtBclContact.direction'.
 
    @see MdtContact
    @see MdtBclContactParams
*/
typedef enum
{
    /**
     * Use primary direction vector.
     *
     * Use \a MdtBclContact.direction vector for primary direction
     * (otherwise use auto).
     */
    MdtBclContactOptionUseDirection = 0x00001,

    /**
     * Use restitution at the contact.
     *
     * The restitution factor is specified in \a
     * MdtBclContactParams.restitution.
     */
    MdtBclContactOptionBounce = 0x00002,

    /**
     * Use a simple soft contact model..
     *
     * The softness factor is specified in \a MdtBclContactParams.softness.
     * This factor corresponds to 1/(h.k) in an undamped spring model,
     * where h is the timestep and k is the spring stiffness factor.
     */
    MdtBclContactOptionSoft = 0x00004,

    /**
     * Use a simple adhesive contact model.
     *
     * The maximum adhesive force is specified in \a
     * MdtBclContactParams.max_adhesive_force.
     */
    MdtBclContactOptionAdhesive = 0x00008,

    /**
     * Use a first order slip in the primary direction.
     *
     * The slip velocity is specified in \a MdtBclContactParams.slip1.
     */
    MdtBclContactOptionSlip1 = 0x00010,

    /**
     * Use a first order slip in the secondary direction.
     *
     * The slip velocity is specified in \a MdtBclContactParams.slip2.
     */
    MdtBclContactOptionSlip2 = 0x00020,

    /**
     * Use a surface velocity in the primary direction (like a conveyor belt
     * motion).
     *
     * The slide velocity is specified in \a MdtBclContactParams.slide1.
     */
    MdtBclContactOptionSlide1 = 0x00040,

    /**
     * Use a surface velocity in the secondary direction.
     *
     * The secondary slide velocity is specified in \a
     * MdtBclContactParams.slide1.
     */
    MdtBclContactOptionSlide2 = 0x00080,

    /**
     * Use world velocity vector to set relative surface velocity.
     *
     * The world velocity at the contact is specified in 
     * \a MdtContact.worldVel.
     */
     MdtBclContactOptionUseWorldVelocity = 0x00100
}
MdtBclContactOption;

/**
 *  MdtBcl Friction model enum.
 *  Friction model to use along primary or secondary direction.
 */

typedef enum
{
    /** Box-type friction model. */
    MdtFrictionModelBox,

    /** Previous-frame normal-force based friction model */
    MdtFrictionModelNormalForce,

    /** Invalid friction model. */
    MdtFrictionModelUnknown = -1
}
MdtFrictionModel;

/**
    MdtBcl Contact constraint parameters struct.
 
    If contact type is 1D friction - motion along secondary direction is
    frictionless, and model2, slip2, slide2 and friction2 are ignored.
 
    @see MdtBclContactOption
*/
struct MdtBclContactParams
{
    /**
     * Contact type (zero, 1D or 2D friction).
     */
    MdtContactType type;

    /** @var model1
     * Friction model to use along primary direction.
     */
    /** @var model2
     * Friction model to use along secondary direction.
     */
    MdtFrictionModel model1;
    MdtFrictionModel model2;

    /**
     * Bitwise combination of MdtBclContactOption's.
     *
     * @see MdtBclContactOption.
     */
    int options;

    /** @var restitution
     * Restitution parameter.
     */
    /** @var velThreshold
     * Minimum velocity for restitution.
     */
    /** @var softness
     * Contact softness parameter (soft mode).
     */
    /** @var max_adhesive_force
     * Contact maximum adhesive force parameter (adhesive mode).
     */
    MeReal restitution;
    MeReal velThreshold;
    MeReal softness;
    MeReal max_adhesive_force;

    /** @var friction1
     * Max friction force in primary direction.
     */
    /** @var slip1
     * First order slip in primary direction.
     */
    /** @var friction2
     * Max friction force in secondary direction.
     */
    /** @var slip2
     * First order slip in primary direction.
     */
    MeReal friction1;
    MeReal frictioncoeff1;
    MeReal slip1;
    MeReal friction2;
    MeReal frictioncoeff2;
    MeReal slip2;

    /** @var slide1
     * Surface velocity in primary direction.
     */
    /** @var slide2
     * Surface velocity in secondary direction.
     */
    MeReal slide1;
    MeReal slide2;

    /**
     * Padding - not used.
     */
    int pad[2];
};

/**
    The enumeration of joint types supported by the MdtBcl module. 
    The first Constraint type MUST be defined as 0, and the others must follow
    in strict ascending sequence, with no gaps, otherwise there will be an
    access violation in MdtWorldStep. These enums will occupy the 'tag' field
    of the MdtConstraintHeader structure.
*/
typedef enum
{
    MdtBclBSJOINT = 0,
    MdtBclHINGE,
    MdtBclPRISMATIC,
    MdtBclCARWHEEL,
    MdtBclCONTACT,
    MdtBclCONTACTGROUP,
    MdtBclFIXEDPATH,
    MdtBclRPROJOINT,
    MdtBclUNIVERSAL,
    MdtBclSKELETAL,
    MdtBclLINEAR1,
    MdtBclLINEAR2,
    MdtBclANGULAR3,
    MdtBclSPRING6,
    MdtBclSPRING,
    MdtBclCONELIMIT,
    MdtBclUSER
}
MdtBclJointType;

/** Flag used to indicate the absence of an attached body */
#define MdtBclNO_BODY           -1

/**
    Kea parameters structure.
*/
struct MdtBclSolverParameters
{
    /**
     * Amount of time to evolve system by.
     */
    MeReal stepsize;

    /**
     * Numerical tolerance used by matrix solver.
     *
     * Default = 0.01.
     */
    MeReal epsilon;

    /**
     * Constraint relaxation rate.
     *
     * Default = 0.2.
     */
    MeReal gamma;
};

/**
    Mdt Constraint Header.
    Contains data common to all constraints.
*/


struct MdtConstraintHeader
{
    /** The world the constraint is in. */
    MdtWorldID              world;

    /** Sort key for this constraint. */
    MeI32                   sortKey;

    /** Pointer to node in world dict */
    MeDictNode                 worldNode;

    /**
     * Pointers to this constraints entries in its bodies
     * lists. This is kept so these references can be removed
     * when the constraint is Disabled.
     */
    MeDictNode                 bodyNode[MdtKeaMAXBODYCONSTRAINT];

    /** User data. This will not be changed from with the toolkit*/
    void                    *userData;

    /** The constrained bodies. */
    MdtBody                 *mdtbody[MdtKeaMAXBODYCONSTRAINT];


    /** Used by the partitioner to mark constraint as ADDED.*/
    MeI32                   flags;

    /** Function used to add this constraint to an MdtKeaConstraints. */
    MdtBclAddConstraintFn   bclFunction;

    /** Max number of rows this constraint can add to MdtKeaConstraints. */
    int                     maxRows;

    /* Output Applied Forces - READ ONLY! */

    /**
     * Force applied to each of the constraints bodies by this
     * constraint.
     */
    MeVector4               resultForce[2];

    /**
     * Torque applied to each of the constraints bodies by this constraint
     */
    MeVector4               resultTorque[2];

    /* 'V-table' */
    MdtConstraintSetBodyFnPtr  setBodyFunc;
    MdtConstraintSetAxisFnPtr  setAxisFunc;

    /** Constraint type tag enum. */
    int tag;

    /**
     * Body indices in the immBody array of constrained bodies.
     *
     * Unused bodies or constraints to the world should be set to
     * MdtBclNO_BODY.
     */
    int bodyindex[MdtKeaMAXBODYCONSTRAINT];

    /**
     * Relative transform between the first body ref. frame and the
     * constraints ref. frame.
     */
    MeMatrix4 ref1;

    /**
     * Relative transform between the second body (or world) ref. frame and
     * the constraints ref. frame.
     */
    MeMatrix4 ref2;

    /**
     *  Linear velocity of world at joint attachment position.
     *  Only used if joint is body-world (ignored if body-body)
     */
    MeVector3 worldLinVel;

    /**
     *  Angular velocity of world at joint attachment position.
     *  Only used if joint is body-world (ignored if body-body)
     */
    MeVector3 worldAngVel;

    /* INTERNAL */
    int     LODpartIndex;   /* Index of partition that this belongs to (or -1) */
    int     rowBudget;      /* Number of rows this constraint will be allowed to use. */
    MeReal  importance;     /* How important this constraint is. */
};


/**
 * A 'null' empty constraint, containing only data common to all
 * constraints.
 */
struct MdtBaseConstraint
{
    /** common to all constraints */
    MdtConstraintHeader head;
};

/** Contact constraint. */
struct MdtContact
{
    /** common to all constraints */
    MdtConstraintHeader head;

    /** Position of contact (world reference frame). */
    MeVector3           cpos;

    /** Unit normal at contact (world reference frame). */
    MeVector3           normal;

    /** Penetration depth of bodies at contact. */
    MeReal              penetration;

    /**
     * Principal friction direction.
     * World reference frame, unit length and perpendicular to normal.
     */
    MeVector3           direction;

    /** 
     *  Velocity of world at contact point.
     *  Only used if contact is body-world (ignored if body-body)
     */
    MeVector3           worldVel;

    /** Friction and restitution parameters. */
    MdtBclContactParams params;

    /**
     * Used by collision to create a list of contacts associated with a
     * pair of bodies.
     */
    MdtContactID        nextContact;

    /**
     * Used by collision to create a list of contacts associated with a
     * pair of bodies.
     */
    MdtContactID        prevContact;

    /** MdtContactGroup that this MdtContact is a member of (could be NULL). */
    MdtContactGroupID   contactGroup;

    /** 
     * 'Importance' rating used when trying to reduce the size of a problem.
     *  This is set when needed by the MdtContactImportanceCB.
     *  @see MdtWorldSetContactImportanceCB
     */
    MeReal              importance;

};


/** Contact group constraint. */
struct MdtContactGroup
{
    /** Common to all constraints */
    MdtConstraintHeader head;
    
    /** Number of contacts in group */
    int count;

    /** First contact in group */
    MdtContactID        first;

    /** Pointer to the "next" field of the last contact in the group */
    MdtContactID        last;

    MeBool              swapped;

    /** The total normal force applied by this contact group last timestep. */
    MeReal              normalForce;

    /** Used internally to track the generator of the contact group. */
    void *              generator;
};



/** Ball and Socket joint constraint */
struct MdtBSJoint
{
    /** common to all constraints */
    MdtConstraintHeader head;
};

/**
    Hinge constraint.

    A hinge constraint limits a body to rotate about one axis relative to
    its parent.
*/
struct MdtHinge
{
    /** common to all constraints */
    MdtConstraintHeader head;

    MdtBclLimit limit;
};

/**
    Prismatic constraint.
 
    A prismatic constraint fixes the orientation of two bodies, and limits
    their position along a line.
*/
struct MdtPrismatic
{
    /** common to all constraints */
    MdtConstraintHeader head;

    /** Optional limits set to motion along the sliding axis. */
    MdtBclLimit limit;
};

/** 
    CarWheel constraint.
 
    A hinge that can be steered along a steering axis, is powered along
    the hinge and steering axes, and has suspension along the steering
    axis. Body 1 is the chassis and body 2 is the wheel. The connection
    point for the wheel body is its center of mass.
*/
struct MdtCarWheel
{
    /** common to all constraints */
    MdtConstraintHeader head;

    /**
        Proportional (spring) constant for suspension.
        Typical values are usually in the range 1-10.
     */
    MeReal skp;
    
    /**
        Derivative (damping) constant for suspension.
        Typical values are usually in the range 1-10.
     */
    MeReal skd;
    /** Steering desired angular velocity, in radians per second. */
    MeReal svel;
    /** Maximum steering motor torque (in Nm). */
    MeReal sfmax;
    /** Hinge desired angular velocity, in radians per second. */
    MeReal hvel;
    /** Maximum hinge motor torque (in Nm). */
    MeReal hfmax;
    /** 1 if steering axis locked at angle = 0. */
    MeBool slock;

    /** Suspension hi limit (in meters), relative to the attachment point. */
    MeReal shi;
    /** Suspension low limit (in meters), relative to the attachment point. */
    MeReal slo;
    /** Suspension reference point (in meters), relative to the attachment
      * point. This is the `resting' position of the suspension. */
    MeReal sref;
    /** Suspension limit softness. */
    MeReal slsoft;
};


/**
    Fixed Path constraint data structure.
 
    A fixed path constraint is used to kinematically control the movement
    of a dynamic body.  This means a dynamic body may be forced to
    animate (translation only for this constraint type) and the resulting
    forces are passed on to other constrained bodies.
*/
struct MdtFixedPath
{
    /** common to all constraints */
    MdtConstraintHeader head;

    /** Position of joint (1st body reference frame). */
    MeVector3 pos1;
    /** Position of joint (2nd body reference frame). */
    MeVector3 pos2;

    /*
      Kinematic data for fixed path:
    */

    /** Kinematic velocity of 1st body (1st body reference frame). */
    MeVector3 vel1;
    /** Kinematic velocity of 2nd body (2nd body reference frame). */
    MeVector3 vel2;
};


/** Relative Position Relative Orientation Joint. */
struct MdtRPROJoint
{
    /** common to all constraints */
    MdtConstraintHeader head;

    /*
      WARNING: This feature is experimental and may not have an API which
      is compatible with other joints.
     */

    /** Orientation of joint in 1st body reference frame */
    MeVector4 q1;
    /** Becomes true if q1 is set */
    MeBool use_q1;
    /** Position of joint (2nd body reference frame). */
    MeVector4 q2;
    /** Becomes true if q2 is set */
    MeBool use_q2;

    /** Desired relative position between reference frame attached at pos1
        and coordinate system attached at pos2 (this takes q1 and q2 into
        account)
    */
    MeVector4 p_rel;
    /** Relative linear velocity at the joint */
    MeVector3 v_rel;

    /** 
        Desired relative orientation between reference frame attached at pos1
        and coordinate system attached at pos2 (this takes q1 and q2 into
        account)
    */
    MeVector4 q_rel;
    /** Relative angular velocity at the joint */
    MeVector3 omega;

    /** Lagrange multiplier limits */
    MeReal angular_fmax[3];
    MeReal linear_fmax[3];

};



/** Universal Joint. */
struct MdtUniversal
{
    /** common to all constraints */
    MdtConstraintHeader head;
};

/** Possible options for configuring skeletal joint cone limit. */
typedef enum
{
    /** No cone limit. */
    MdtSkeletalConeOptionFree = 0,

    /** Limited to angular slot in direction of orthogonal axis. */
    MdtSkeletalConeOptionSlot = 1,

    /* Limited to cone around primary axis. */
    MdtSkeletalConeOptionCone = 2,

    /* Fixed to primary axis. */
    MdtSkeletalConeOptionFixed = 3
} MdtSkeletalConeOption;

/** Possible options for configuring skeletal twist limit. */
typedef enum
{
    /** No twist limit. */
    MdtSkeletalTwistOptionFree = 0,

    /** Twist limited around primary axis. */
    MdtSkeletalTwistOptionLimited = 1,

    /** Twist fixed around primary axis. */
    MdtSkeletalTwistOptionFixed = 2
} MdtSkeletalTwistOption;

/** Skeletal Joint. */
struct MdtSkeletal
{
    /** Common to all constraints */
    MdtConstraintHeader head;

    /** 
     *  In the case of MdtSkeletalTwistOptionLimited & 
     *  MdtSkeletalConeOptionCone, use just one row to do all limits.
     *  This will use the cone stiffness/damping for all limits.
     */
    MeBool combinedLimits;

    /** Cone limit configuration. */
    MdtSkeletalConeOption coneOption;

    /** Twist limit configuration. */
    MdtSkeletalTwistOption twistOption;

    /** Twist limit angle, around x axis (primary axis). */
    /** Cos of half twist limit angle. */
    MeReal cos_half_twist_limit_angle;

    /** Cone limit angle1, towards y axis (orthogonal axis), ie around z axis. */
    /** Cos of half cone limit angle1. */
    MeReal cos_half_cone_limit_angle_1;

    /** Cone limit angle2, towards z axis, ie around y axis (orthogonal axis). */
    /** Cos of half cone limit angle2. */
    MeReal cos_half_cone_limit_angle_2;

    /* Twist limit stiffness. */
    MeReal twist_stiffness;

    /** Twist limit damping. */
    MeReal twist_damping;

    /** Cone limit stiffness. */
    MeReal cone_stiffness;
    
    /** Cone limit damping. */
    MeReal cone_damping;
};

/**
 *  Spring that constrains all 6 degrees of freedom.
 *  Allows seperate stiffness/damping for each DOF.
 */
struct MdtSpring6
{
    /** common to all constraints */
    MdtConstraintHeader head;

    /** Spring stiffness in each of the linear directions. */
    MeVector3 linearStiffness;

    /** Spring damping in each of the linear directions. */
    MeVector3 linearDamping;

    /** Spring stiffness in each of the angular directions. */
    MeVector3 angularStiffness;

    /** Spring damping in each of the angular directions. */
    MeVector3 angularDamping;

    /** If joint to the world, linear velocity of world at attachment. */
    MeVector3 worldLinearVel;

    /** If joint to the world, angular velocity of world at attachment. */
    MeVector3 worldAngularVel;
};

/**
    Linear1 constraint data structure.
 
    Constrains one body to lie in plane relative to the other, and does
    not constraint orientation.
*/
struct MdtLinear1
{
    /** common to all constraints */
    MdtConstraintHeader head;

    /** Position of joint (1st body reference frame). */
    MeVector3 pos1;
    /** Position of joint (2nd body reference frame). */
    MeVector3 pos2;

    /**
     * Dot product of the separation vector of the two bodies and the
     * normal vector (fixed in the body[1] reference frame): invariant
     * for this joint.
     */
    MeReal displacement;
};

/**
    Linear2 constraint data structure.
 
    Constrains one body to lie along a line relative to the other, and
    does not constrain orientation.
*/
struct MdtLinear2
{
    /** common to all constraints */
    MdtConstraintHeader head;

    /** Position of joint (1st body reference frame). */
    MeVector3 pos1;
    /** Position of joint (2nd body reference frame). */
    MeVector3 pos2;

    /**
     * Vector fixed in the body[1] reference frame (or the inertial
     * frame, if no second body) on which the joint position is
     * constrained to lie.
     */
    MeVector3 direction;
    /**
     * A vector fixed in 2nd body reference frame which is perpendicular
     * to the joint direction.
     */
    MeVector3 vec1;
    /**
     * A vector fixed in 2nd body reference frame which is perpendicular
     * to both the joint direction and vec1.
     */
    MeVector3 vec2;
};

/**
    Angular3 constraint data structure.
 
    Constrains one body to have a fixed orientation with respect to
    the other, and does not constrain either boies' position.  This
    constraint may, at the user's choice, become an Angular 2 joint
    by freeing rotation about a specified axis.
*/
struct MdtAngular3
{
    /** common to all constraints */
    MdtConstraintHeader head;

    /**
     * Flag which specifies whether rotation is allowed about the
     * vector[0] axis.  When set, this is effectively an
     * Angular2 constraint.
     */
    MeBool bEnableRotation;

    /**
     * Overall 'stiffness' of this constraint.
     * By default, constrained degrees of freedom are 'hard' ie.
     * Stiffness = INFINITY, but can be allowed some springy 'give'
     * using this parameter.
     */
    MeReal stiffness;

    /**
     * Overall spring 'damping' of this constraint.
     * By default, Damping = 0. Should be used in conjunction with
     * 'stiffness' above.
     */
    MeReal damping;
};


/**
    MdtBclSpring constraint data structure.
 
    This joint attaches one body to another, or to the inertial
    reference frame, at a given separation.  This separation
    is governed by two limits which may both be "hard" (which
    simulates a rod or strut joint) or both soft (simulating a
    spring) or hard on one limit but soft on the other (e.g. an
    elastic attachment which may be stretched but not compressed).
    The default behaviour is spring-like, with two soft, damped
    limits, both initialised at the initial separation of the
    bodies.
*/
struct MdtSpring
{
    /** common to all constraints */
    MdtConstraintHeader head;

    /** Spring attachment position to the first body, in 1st body's reference frame.  */
    MeVector3 pos1;
    /** Spring attachment position to the second body,
    in 2nd body's reference frame.  */
    MeVector3 pos2;

    /** Limits set to the displacement between the two bodies. */
    MdtBclLimit limit;
};

/** Cone Limit Constraint. */
struct MdtConeLimit
{
    /** common to all constraints */
    MdtConstraintHeader head;

    /** cone half-angle */
    MeReal cone_half_angle;

    /** cosine of cone half-angle */
    MeReal cos_cone_half_angle;

    /**
        Overall 'stiffness' of the limit constraint.
        By default, constrained degrees of freedom are 'hard' ie.
        Stiffness = INFINITY, but can be allowed some springy 'give'
        using this parameter.
    */
    MeReal stiffness;
    
    /**
        Overall spring 'damping' of the limit constraint.
        By default, Damping = 0. Should be used in conjunction with
        'stiffness' above.
    */
    MeReal damping;
};

/** User Defined Constraint Type. */
struct MdtUserConstraint
{
    /** common to all Mdt constraints */
    MdtConstraintHeader     head;

    /** User constraint data */
    void *userConstraint;
};


#endif
