#ifndef _MDTKEA_H /* -*- mode: C; -*- */
#define _MDTKEA_H

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:01 $ - Revision: $Revision: 1.88.2.2 $

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
 *
 * Kea solver header file.
 *
 * For explanation of some notation see "Programming From
 * Specifications" by Carroll Morgan.
 */

#include <MePrecision.h>

#define MdtKeaBODYVER1           100

/**
 * MdtKeaMAXBODYCONSTRAINT
 *
 * The maximum number of bodies that can be constrained by each constraint
 */
#define MdtKeaMAXBODYCONSTRAINT 2

/**
 * MdtKeaBody Flag enum.
 *
 * Any bitwise combination of these options is valid for use in
 * 'MdtKeaBody.flags'.
 */
typedef enum
{
    /**
     * Use the 'fast spin' integrator for this body.
     */
    MdtKeaBodyFlagUseFastSpin = 0x01,

    /**
     * Indicates that this body has at least one constraint affecting it.
     */
    MdtKeaBodyFlagIsConstrained = 0x02,

    /**
     * indicates that this flag should use the full MoI tensor
     * in the mass matrix rather than a spherical approximation
     */
    MdtKeaBodyFlagIsNonSpherical = 0x04,

    /**
     * indicates that this flag should use add Coriolis Forces 
     * to the body
     */
    MdtKeaBodyFlagAddCoriolisForce = 0x08
}
MdtKeaBodyFlag;
/** 
 * Force,torque pair
 */
typedef struct
{
    MeReal force[3];
    int    pad0;

    MeReal torque[3];
    int    pad1;
} MdtKeaForce;

/** 
 * Velocity,angular velocity pair
 */
typedef struct
{
    MeReal velocity[3];
    int    pad0;

    MeReal angVelocity[3];
    int    pad1;
} MdtKeaVelocity;


/**
 * Pair of force,torque pairs.
 */
typedef struct 
{
    MdtKeaForce primary_body;
    MdtKeaForce secondary_body;
} MdtKeaForcePair;

/**
 * 4*6 Jacobian matrix block,
 * in transpose wallpaper format.
 */
typedef struct 
{
    MeVector4 col[6];
} MdtKeaJBlock;

/**
 * Pair of 4*6 Jacobian matrix blocks,
 * in transpose wallpaper format.
 */
typedef MdtKeaJBlock MdtKeaJBlockPair[2];

/**
 * Pair of body indices
 */
typedef int MdtKeaBodyIndexPair[2];

/**
 * Row of the bl2body table
 */
typedef int MdtKeaBl2BodyRow[8]; 

/**
 * Row of the bl2cbody table
 */
typedef int MdtKeaBl2CBodyRow[8]; 

/**
 * Kea Constraints structure.
 *
 * To find the total number of @a rows_to_allocate, find the maximum
 * number of rows the constraints can add, but allow up to an extra
 * three rows padding per partition.
*/
typedef struct
{
    /**
     * Number of partitions described by this structure.
     */
    int num_partitions;

    /**
     * Maximum number of partitions that can be added.
     */
    int max_partitions;

    /**
     * num_rows_partition[i] is number of rows in partition i
     * excluding padding rows
     *
     * The size should be one int for each partition.
     */
    int *num_rows_exc_padding_partition;

    /**
     * num_rows_partition[i] is number of rows in partition i.
     * including padding rows
     *
     * The size should be one int for each partition.
     */

    int *num_rows_inc_padding_partition;

    /**
     * num_constraints_partition[i] is number of constraints in
     * partition i.
     *
     * The size should be one int for each partition.
     */
    int *num_constraints_partition;

    /**
     *  Total number of constraint rows including padding
     *  after each constraint.
     */
    int num_rows_inc_padding;

    /**
     * Total number of constraint rows excluding padding
     * (But including padding at end of partitions)
     */
    int num_rows_exc_padding;

    /**
     *  Maximum number of constraint rows including padding
     *  after each constraint.
     */
    int max_rows_inc_padding;

    /**
     *  Maximum number of constraint rows excluding padding
     * (But including padding at end of partitions)
     */
    int max_rows_exc_padding;

    /**
     * Total number of constraints.
     *
     * (+ | 0<=i<num_partitions . num_constraints_partition[i])
     */
    int num_constraints;

    /**
     * Maximum number of constraints that can be added.
     */
    int max_constraints;

    /**
     * List of nonzero blocks of jacobian matrix.
     *
     * The size should be 12 MeReals for
     * each rows_to_allocate, initialised to 0.
     */
    MdtKeaJBlockPair *Jstore;

    /**
     * Constraint error vector.
     *
     * One MeReal for each rows_to_allocate initialised to 0.
     */
    MeReal *xi;

    /**
     * Vector in the constraint equation m{J*v=c}.
     *
     * One MeReal for each rows_to_allocate initialised to 0.
     */
    MeReal *c;

    /**
     * Low limit on Lagrange multiplier.
     *
     * One  MeReal for ebach rows_to_allocate initialised to 0.
     */
    MeReal *lo;

    /**
     * High limit on Lagrange multiplier.
     *
     * One MeReal for each rows_to_allocate initialised to 0.
     */
    MeReal *hi;

    /**
     * Calculated Lagrange multiplier READ ONLY.
     *
     * One MeReal for each rows_to_allocate initialised to 0.
     */
    MeReal *lambda;

    /**
     * Vector of forces applied to satisfy constraints READ ONLY.
     *
     * 16 MeReals for each constraint added initialised to 0.
     */
    MdtKeaForcePair *force;

    /**
     * First order constraint slipping vector.
     *
     * One MeReal for each rows_to_allocate initialised to 0.
     */
    MeReal *slipfactor;

    /**
     * Projection constants.
     *
     * One MeReal for each rows_to_allocate initialised to 0.
     */
    MeReal *xgamma;

    /**
     * Jsize[i] is the number of rows in constraint i.
     *
     * One int for each constraint added.
     */
    int *Jsize;

    /**
     * Offset of constraint i from the start of its partition.
     *
     * partition_start+Jofs[i] is the offset in the above vectors of the
     * first element corresponding to constraint i, where partition_start
     * is the offset of the first constraint in the partition which i
     * belongs to. One int for each constraint added
     */
    int *Jofs;

    /**
     * Jbody[i*2+0],Jbody[i*2+1], are the bodies
     * constrained by constraint i.
     *
     * 2 ints for each constraint added.
     */
    MdtKeaBodyIndexPair *Jbody;
}
MdtKeaConstraints;

typedef struct _MeCPUResources* MeCPUResources;

/**
 * Debug data logging
 */

typedef struct
{
    /** Set writeKeaInterData in order to write keas input to a file */
    MeBool writeKeaInputData;

    /** Name of the file to dump kea's input to */
    char * writeKeaInputDataFilename;

    /** Set readKeaInterData in order to read kea's input from a file, ignoring
        the input supplied as parameters to MdtKeaCalcConstraintForces */
    MeBool readKeaInputData;

    /** Name of the file to read kea's input from */
    char * readKeaInputDataFilename;

    /** Set writeKeaInterData in order to dump intermediate results of the kea 
        calculation */
    MeBool writeKeaInterData;

    /** Name of the file to dump the intermediate results to */
    char * writeKeaInterDataFilename;

    /** Set writeKeaOutputData in order to dump the output of the kea */ 
    MeBool writeKeaOutputData;

    /** Name of the file to dump kea's to */
    char * writeKeaOutputDataFilename;

    /** frame should be set to the number of the current graphics frame */
    int    frame;

    /** The number of the frame being investigated */
    int    badFrame;

    /** The partition that is being investigated */
    int    badPartition;
    
} MdtKeaDebugDataRequest;

/**
 * Kea Parameters structure.
 */
typedef struct
{
    /** Amount of time to evolve system by. */
    MeReal stepsize;

    /** Numerical tolerance used by matrix solver. Default = 0.01. */
    MeReal epsilon;

    /** Constraint relaxation rate. Default = 0.2. */
    MeReal gamma;

    /** Maximum allowed number of LCP iterations. Default = 10 */
    int max_iterations;

    /** Velocity below which LCP considers velocity to be zero.
        Default = 0.03. */
    MeReal velocityZeroTol;

    /**
     * Pointer to some memory that Kea can use.
     *
     * Kea doesn't malloc, only uses this area.
     */
    void *memory_pool;

    /** Size of this area in bytes. */
    unsigned int memory_pool_size;

    /** CPU resources */
    MeCPUResources cpu_resources;

    /** For mathengine internal use only */
    MdtKeaDebugDataRequest debug;
}
MdtKeaParameters;

/**
 * Kea Body structure.
 *
 * After a time step, the contents of 'force' and 'torque' will be the
 * total applied forces plus any forces and torques due to constraints.
 * These MUST BE RE-ZEROED before accumulating values for the next time
 * step.
*/
typedef struct
{
    /** Data stucture tag. Should be MdtKeaBODYVER1. */
    int tag;                    /* 0x00*/
    /** Total length of data structure. */
    int len;

    /** 1/Mass of the body. */
    MeReal invmass;

    /**
     * Bitwise combination of MdtKeaBodyFlag's.
     * @see MdtKeaBodyFlag.
     */
    int flags;

    /** Applied (to centre of mass) force. */
    MeVector4 force;            /* 0x10 */

    /** Applied torque. */
    MeVector4 torque;           /* 0x20 */

    /* ToDo: This should become an MeMatrix3. */
    /** Inverse of 3x3 inertia tensor - row 0. */
    MeVector4 invI0;            /* 0x30 */
    /** Inverse of 3x3 inertia tensor - row 1.  */
    MeVector4 invI1;            /* 0x40 */
    /** Inverse of 3x3 inertia tensor - row 2. */
    MeVector4 invI2;            /* 0x50 */

    /* ToDo: This should become an MeMatrix3. */
    /** The 3x3 inertia tensor - row 0.  */
    MeVector4 I0;               /* 0x60 */
    /** The 3x3 inertia tensor - row 1. */
    MeVector4 I1;               /* 0x70 */
    /** The 3x3 inertia tensor - row 2. */
    MeVector4 I2;               /* 0x80 */

    /** Linear velocity of body. */
    MeVector4 vel;              /* 0x90 */
    /** Angular velocity of body. */
    MeVector4 velrot;           /* 0xa0 */
    /** Body orientation quaternion. */
    MeVector4 qrot;             /* 0xb0 */

    /** Linearl Acceleration of body (READ ONLY). */
    MeVector4 accel;            /* 0xc0 */
    /** Angular acceleration of body (READ ONLY). */
    MeVector4 accelrot;         /* 0xd0 */

    /**
     * Axis of assumed fast rotation (unit length).
     * Only used if MdtKeaBodyFlagUseFastSpin is set in 'MdtKeaBody.flags'.
     */
    MeVector4 fastSpinAxis;     /* 0xe0 */
}
MdtKeaBody;

/**
 * Sparse representation of the inverse of a 6*6 mass matrix.
 *
 * Can't use MeVector3 in this definition because we want to
 * explicitly pad so we can put invmass in the padding.
 *
 * It is absolutely vital that the sizeof this structure is an integer
 * multiple of the sizeof an MeReal, since the optimised solvers depend on
 * this.
 */
typedef struct
{
    /** Inverse of 3x3 inertia tensor - row 0. */
    MeReal invI0[3];
    /** Inverse of mass                        */
    MeReal invmass;

    /** Inverse of 3x3 inertia tensor - row 1. */
    MeReal invI1[3];
    MeReal pad1;

    /** Inverse of 3x3 inertia tensor - row 2. */
    MeReal invI2[3];
    MeReal pad2;
} MdtKeaInverseMassMatrix;

/**
 * Kea 4x4 Transformation Matrix.
 */
typedef struct
{
    /** Rotation matrix row 0 (last element 0). */
    MeVector4 R0;
    /** Rotation matrix row 1 (last element 0). */
    MeVector4 R1;
    /** Rotation matrix row 2 (last element 0). */
    MeVector4 R2;
    /** Position vector (last element 1). */
    MeVector4 pos;
} MdtKeaTransformation
#ifdef PS2
__attribute__((aligned(16)))
#endif
;



#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Utility function for detecting the CPU Resources
 *
 */
MEPUBLIC
MeCPUResources MEAPI MdtKeaQueryCPUResources(void);

/**
 * Utility function for calculating the size of the workspace needed by
 * Kea.
 *
 * This memory is passed into Kea in the MdtKeaParameters structure.
 */
MEPUBLIC
int MEAPI MdtKeaMemoryRequired(const int num_rows_exc_padding[],
                               int num_partitions,
                               int max_rows,
                               int max_bodies);

/**
 * Kea solver function.
 *
 * Takes an array of bodies and a MdtKeaConstraints struct (which
 * defines the constraints as defined above), as well as an array
 * indicating which constraints are to be solved together as one
 * partition. It solves each partition and evolves the system.
 *
 * @param constraints A MdtKeaConstraints struct containing constraint
 * rows generated by MdtBcl functions.
 * @param blist Array of MdtKeaBody structs to be simulated.
 * @param tlist Array of MdtKeaTransformation structs that correspond to
 * blist.
 * @param num_bodies Number of bodies/transformations in blist/tlist
 * @param parameters A MdtKeaParameters structure containing stepsize,
 * memory pool etc.
 */


MEPUBLIC
void MEAPI MdtKeaAddConstraintForces(MdtKeaConstraints       constraints, 
                                     MdtKeaBody *            blist[],
                                     const MdtKeaTransformation    
                                                             tlist[],
                                     int                     num_bodies,
                                     MdtKeaParameters        parameters);


MEPUBLIC
void MEAPI MdtKeaIntegrateSystem(MdtKeaBody           *const blist[],
                                 MdtKeaTransformation tlist[],
                                 int                  num_bodies, 
                                 MdtKeaParameters     parameters);

/*
 * Platform dependent functions for flushing and invalidating dcache lines
 */

MEPUBLIC
void MdtFlushCache(int);
MEPUBLIC
void MdtSyncDCache(void *, void *);
MEPUBLIC
void MdtInvalidDCache(void *, void *);

#ifdef __cplusplus
}
#endif

#endif
