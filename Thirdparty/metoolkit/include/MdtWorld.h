#ifndef _MDTWORLD_H
#define _MDTWORLD_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.55.2.2 $

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
 * MdtWorld API functions.
 */

#include <MePrecision.h>
#include <MdtTypes.h>

#ifdef __cplusplus
extern "C"
{
#endif


/*
  World functions.
*/

MEPUBLIC
MdtWorldID        MEAPI MdtWorldCreate(const unsigned int maxBodies,
                                       const unsigned int maxConstraints, 
                                       const MeReal lengthScale,
                                       const MeReal massScale);
MEPUBLIC
void              MEAPI MdtWorldReset(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtWorldReCreate(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtWorldDestroy(const MdtWorldID w);

MEPUBLIC
void              MEAPI MdtWorldStep(const MdtWorldID w,
                           const MeReal stepSize);
MEPUBLIC
void              MEAPI MdtWorldStepSafeTime(const MdtWorldID w, 
                           const MeReal stepSize);


/*
  World accessors
*/

MEPUBLIC
int               MEAPI MdtWorldGetTotalBodies(const MdtWorldID w);
MEPUBLIC
int               MEAPI MdtWorldGetEnabledBodies(const MdtWorldID w);
MEPUBLIC
int               MEAPI MdtWorldGetTotalConstraints(const MdtWorldID w);
MEPUBLIC
int               MEAPI MdtWorldGetEnabledConstraints(const MdtWorldID w);
MEPUBLIC
int               MEAPI MdtWorldGetMaxBodies(const MdtWorldID w);
MEPUBLIC
int               MEAPI MdtWorldGetMaxConstraints(const MdtWorldID w);
MEPUBLIC
MeReal            MEAPI MdtWorldGetEpsilon(const MdtWorldID w);
MEPUBLIC
MeReal            MEAPI MdtWorldGetGamma(const MdtWorldID w);
MEPUBLIC
MeReal            MEAPI MdtWorldGetMinSafeTime(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtWorldGetGravity(const MdtWorldID w, MeVector3 g);
MEPUBLIC
MeBool            MEAPI MdtWorldGetAutoDisable(const MdtWorldID w);
MEPUBLIC
MeReal            MEAPI MdtWorldGetAutoDisableVelocityThreshold(
                            const MdtWorldID w);
MEPUBLIC
MeReal            MEAPI MdtWorldGetAutoDisableAngularVelocityThreshold(
                            const MdtWorldID w);
MEPUBLIC
MeReal            MEAPI MdtWorldGetAutoDisableAccelerationThreshold(
                            const MdtWorldID w);
MEPUBLIC
MeReal            MEAPI MdtWorldGetAutoDisableAngularAccelerationThreshold(
                            const MdtWorldID w);
MEPUBLIC
MeReal            MEAPI MdtWorldGetAutoDisableAliveTime(const MdtWorldID w);
MEPUBLIC
int               MEAPI MdtWorldGetMaxMatrixSize(const MdtWorldID w);
MEPUBLIC          
MeChunk*          MEAPI MdtWorldGetKeaPoolChunk(const MdtWorldID w);

MEPUBLIC
MeChunk*          MEAPI MdtWorldGetPartitionOutputChunk(const MdtWorldID w);

MEPUBLIC
MeChunk*          MEAPI MdtWorldGetKeaTMChunk(const MdtWorldID w);

MEPUBLIC
MeChunk*          MEAPI MdtWorldGetKeaConstraintsChunk(const MdtWorldID w);

MEPUBLIC
int               MEAPI MdtWorldGetMaxMemoryPoolUsed(const MdtWorldID w);

MEPUBLIC
int               MEAPI MdtWorldGetMaxLCPIterations(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtWorldGetLODParams(const MdtWorldID w,
                            MdtLODParams* const lodParams);
MEPUBLIC          
MeBool            MEAPI MdtWorldGetCheckSim(const MdtWorldID w);
MEPUBLIC
MdtSimErrorCBPtr  MEAPI MdtWorldGetSimErrorCB(const MdtWorldID w);

MEPUBLIC          
MeReal            MEAPI MdtWorldGetDefaultDensity(const MdtWorldID w);


/*
  World mutators
*/

MEPUBLIC
void              MEAPI MdtWorldSetEpsilon(const MdtWorldID w, const MeReal e);
MEPUBLIC
void              MEAPI MdtWorldSetGamma(const MdtWorldID w, const MeReal g);
MEPUBLIC
void              MEAPI MdtWorldSetGravity(const MdtWorldID w,
                            const MeReal gx, const MeReal gy, const MeReal gz);
MEPUBLIC
void              MEAPI MdtWorldSetGammaWithRefTimeStep(const MdtWorldID w,
                            const MeReal aGamma, const MeReal aRefStep, 
                            const MeReal aTimeStep);
MEPUBLIC
void              MEAPI MdtWorldSetMinSafeTime(const MdtWorldID w, 
                            const MeReal t);
MEPUBLIC
void              MEAPI MdtWorldResetForces(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtWorldSetAutoDisable(const MdtWorldID w,
                            const MeBool d);
MEPUBLIC
void              MEAPI MdtWorldSetAutoDisableVelocityThreshold(
                            const MdtWorldID w, const MeReal vt);
MEPUBLIC
void              MEAPI MdtWorldSetAutoDisableAngularVelocityThreshold(
                            const MdtWorldID w, const MeReal avt);
MEPUBLIC
void              MEAPI MdtWorldSetAutoDisableAccelerationThreshold(
                            const MdtWorldID w, const MeReal at);
MEPUBLIC
void              MEAPI MdtWorldSetAutoDisableAngularAccelerationThreshold(
                            const MdtWorldID w,const MeReal aat);
MEPUBLIC
void              MEAPI MdtWorldSetAutoDisableAliveTime(
                            const MdtWorldID w, const MeReal aw);
MEPUBLIC
void              MEAPI MdtWorldSetMaxMatrixSize(const MdtWorldID w,
                            const int size);
MEPUBLIC
void              MEAPI MdtWorldSetDebugDrawing(const MdtWorldID w,
                            const MdtDebugDrawOptions drawOptions);
MEPUBLIC
void              MEAPI MdtWorldSetKeaDebugRequest(const MdtWorldID w,
                            const MdtKeaDebugDataRequest debugDataRequest);
MEPUBLIC
void              MEAPI MdtWorldSetMatrixSizeLog(const MdtWorldID w,
                            int *const sizeLog, const int logSize);
MEPUBLIC
void              MEAPI MdtWorldSetMaxLCPIterations(const MdtWorldID w,
                            const int mi);
MEPUBLIC
void              MEAPI MdtWorldSetLODParams(const MdtWorldID w,
                            const MdtLODParams* const lodParams);
MEPUBLIC          
void              MEAPI MdtWorldSetSimErrorCB(const MdtWorldID w,
                            MdtSimErrorCBPtr cb, void* secbdata);
MEPUBLIC          
void              MEAPI MdtWorldSetCheckSim(const MdtWorldID w,
                            const MeBool c);

/*
   World others.
*/
MEPUBLIC
void              MEAPI MdtWorldForAllConstraints(const MdtWorldID w,
                            MdtConstraintIteratorCBPtr cb, void* ccbdata);



#ifdef __cplusplus
}
#endif


#endif
