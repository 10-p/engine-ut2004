#ifndef _MDTUTILITIES_H
#define _MDTUTILITIES_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.13.4.1 $

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
 * Utility API functions.
 */

#include <MePrecision.h>
#include <MdtTypes.h>
#include <MeCall.h>


#ifdef __cplusplus
extern "C"
{
#endif


/*
  Public utility functions.
*/

MEPUBLIC
void              MEAPI MdtConvertVector(const MdtBodyID from_body,
                            const MeVector3 f, const MdtBodyID to_body,
                            MeVector3 t);
MEPUBLIC
void              MEAPI MdtConvertPositionVector(const MdtBodyID from_body,
                            const MeVector3 f, const MdtBodyID to_body,
                            MeVector3 t);

MEPUBLIC
void              MEAPI MdtMakeInertiaTensorSphere(const MeReal mass,
                            const MeReal radius, MeMatrix3 i);
MEPUBLIC
void              MEAPI MdtMakeInertiaTensorBox(const MeReal mass,
                            const MeReal lx, const MeReal ly, const MeReal lz,
                            MeMatrix3 i);

MEPUBLIC
void              MEAPI MdtLimitController(const MdtLimitID limit,
                            const MeReal desiredPosition, const MeReal gap,
                            const MeReal maxSpeed, const MeReal maxForce);


#ifdef __cplusplus
}
#endif


#endif
