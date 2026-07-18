#ifndef _MDTMAINLOOP_H
#define _MDTMAINLOOP_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.12.2.1 $

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

#include <MdtTypes.h>

#ifdef __cplusplus
extern "C"
{
#endif

MEPUBLIC
void            MEAPI MdtAutoDisableLastPartition(MdtPartitionOutput* po,
                        void* cbdata);

MEPUBLIC
unsigned int    MEAPI MdtPackAllPartitions(const MdtPartitionOutput* po,
                        const MeReal stepSize, MdtWorldParams* params,
                        MdtKeaParameters* keaParams,
                        MdtKeaTransformation* keaTMArray,
                        MdtKeaConstraints* constraints);

MEPUBLIC
unsigned int    MEAPI MdtPackPartition(const MdtPartitionOutput* po,
                        const unsigned int partitionindex,
                        const MeReal stepSize, MdtWorldParams* params,
                        MdtKeaParameters* keaParams,
                        MdtKeaTransformation* keaTMArray,
                        MdtKeaConstraints* constraints);

MEPUBLIC
void            MEAPI MdtUnpackBodies(MdtKeaTransformation* keaTMArray,
                        const unsigned int partitionindex, 
			            MdtPartitionOutput* po);

MEPUBLIC
unsigned int    MEAPI MdtUnpackForces(const MdtKeaForcePair *force,
                        const unsigned int partitionindex, 
                        MdtPartitionOutput* po);

MEPUBLIC
MeReal          MEAPI MdtPartitionGetSafeTime(MdtPartitionOutput* po, int i);



void            MEAPI MdtLODLastPartition(MdtPartitionOutput* po, 
                                    MdtPartitionParams* params);

MEPUBLIC
MdtKeaConstraints*  MEAPI MdtKeaConstraintsCreateFromChunk(MeChunk* chunk,
                            int maxPartitions,
                            int maxKeaConstraints,
                            int maxRows);

MEPUBLIC
void            MEAPI MdtDefaultSimErrorCallBack(MdtKeaConstraints* kc, 
                                                 MdtKeaBody** kb, 
                                                 int nBodies, 
                                                 void* secbdata);


#ifdef __cplusplus
}
#endif

#endif
