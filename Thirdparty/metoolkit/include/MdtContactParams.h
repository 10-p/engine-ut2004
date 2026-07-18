#ifndef _MDTCONTACTPARAMS_H
#define _MDTCONTACTPARAMS_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.12.6.2 $

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
 * MdtContactParams API functions.
 */

#include <MePrecision.h>
#include <MdtTypes.h>


#ifdef __cplusplus
extern "C"
{
#endif


/*
  Contact Parameters functions
*/


MEPUBLIC
void              MEAPI MdtContactParamsReset(const MdtContactParamsID p);

/*
  Contact Parameters accessors
*/

MEPUBLIC
MdtContactType    MEAPI MdtContactParamsGetType(
                            const MdtContactParamsID p);
MEPUBLIC
MdtFrictionModel  MEAPI MdtContactParamsGetPrimaryFrictionModel(
                            const MdtContactParamsID p);
MEPUBLIC
MdtFrictionModel  MEAPI MdtContactParamsGetSecondaryFrictionModel(
                            const MdtContactParamsID p);
MEPUBLIC
MeReal            MEAPI MdtContactParamsGetRestitution(
                            const MdtContactParamsID p);
MEPUBLIC
MeReal            MEAPI MdtContactParamsGetRestitutionThreshold(
                            const MdtContactParamsID p);
MEPUBLIC
MeReal            MEAPI MdtContactParamsGetSoftness(
                            const MdtContactParamsID p);
MEPUBLIC
MeReal            MEAPI MdtContactParamsGetMaxAdhesiveForce(
                            const MdtContactParamsID p);
MEPUBLIC
MeReal            MEAPI MdtContactParamsGetPrimaryFriction(
                            const MdtContactParamsID p);
MEPUBLIC
MeReal            MEAPI MdtContactParamsGetPrimaryFrictionCoeffecient(
                            const MdtContactParamsID p);
MEPUBLIC
MeReal            MEAPI MdtContactParamsGetPrimarySlip(
                            const MdtContactParamsID p);
MEPUBLIC
MeReal            MEAPI MdtContactParamsGetPrimarySlide(
                            const MdtContactParamsID p);
MEPUBLIC
MeReal            MEAPI MdtContactParamsGetSecondaryFriction(
                            const MdtContactParamsID p);
MEPUBLIC
MeReal            MEAPI MdtContactParamsGetSecondaryFrictionCoeffecient(
                            const MdtContactParamsID p);
MEPUBLIC
MeReal            MEAPI MdtContactParamsGetSecondarySlip(
                            const MdtContactParamsID p);
MEPUBLIC
MeReal            MEAPI MdtContactParamsGetSecondarySlide(
                            const MdtContactParamsID p);


/*
  Contact Parameters mutators
*/

MEPUBLIC
void              MEAPI MdtContactParamsSetType(const MdtContactParamsID p,
                            MdtContactType t);
MEPUBLIC
void              MEAPI MdtContactParamsSetPrimaryFrictionModel(
                            const MdtContactParamsID p, MdtFrictionModel m);
MEPUBLIC
void              MEAPI MdtContactParamsSetSecondaryFrictionModel(
                            const MdtContactParamsID p, MdtFrictionModel m);
MEPUBLIC
void              MEAPI MdtContactParamsSetFrictionModel(
                            const MdtContactParamsID p, MdtFrictionModel m);
MEPUBLIC
void              MEAPI MdtContactParamsSetRestitution(
                            const MdtContactParamsID p, MeReal r);
MEPUBLIC
void              MEAPI MdtContactParamsSetRestitutionThreshold(
                            const MdtContactParamsID p, MeReal v);
MEPUBLIC
void              MEAPI MdtContactParamsSetSoftness(
                            const MdtContactParamsID p, MeReal s);
MEPUBLIC
void              MEAPI MdtContactParamsSetMaxAdhesiveForce(
                            const MdtContactParamsID p, MeReal s);
MEPUBLIC
void              MEAPI MdtContactParamsSetPrimaryFriction(
                            const MdtContactParamsID p, MeReal f);
MEPUBLIC
void              MEAPI MdtContactParamsSetPrimaryFrictionCoeffecient(
                            const MdtContactParamsID p, MeReal f);
MEPUBLIC
void              MEAPI MdtContactParamsSetPrimarySlip(
                            const MdtContactParamsID p, MeReal s);
MEPUBLIC
void              MEAPI MdtContactParamsSetPrimarySlide(
                            const MdtContactParamsID p, MeReal s);
MEPUBLIC
void              MEAPI MdtContactParamsSetSecondaryFriction(
                            const MdtContactParamsID p, MeReal f);
MEPUBLIC
void              MEAPI MdtContactParamsSetSecondaryFrictionCoeffecient(
                            const MdtContactParamsID p, MeReal f);
MEPUBLIC
void              MEAPI MdtContactParamsSetSecondarySlip(
                            const MdtContactParamsID p, MeReal s);
MEPUBLIC
void              MEAPI MdtContactParamsSetSecondarySlide(
                            const MdtContactParamsID p, MeReal s);
MEPUBLIC
void              MEAPI MdtContactParamsSetFriction(
                            const MdtContactParamsID p, MeReal f);
MEPUBLIC
void              MEAPI MdtContactParamsSetFrictionCoeffecient(
                            const MdtContactParamsID p, MeReal f);
MEPUBLIC
void              MEAPI MdtContactParamsSetSlip(
                            const MdtContactParamsID p, MeReal f);

#ifdef __cplusplus
}
#endif


#endif
