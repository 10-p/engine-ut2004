#ifndef _MDTCONTACT_H
#define _MDTCONTACT_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.21.2.1 $

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
 * MdtContact API functions.
 */

#include <MePrecision.h>
#include <MdtTypes.h>

#ifdef __cplusplus
extern "C"
{
#endif



/*
  Contact functions.
*/

MEPUBLIC
MdtContactID      MEAPI MdtContactCreate(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtContactReset(const MdtContactID c);
MEPUBLIC
MdtConstraintID   MEAPI MdtContactQuaConstraint(const MdtContactID c);
MEPUBLIC
MdtContactID      MEAPI MdtConstraintDCastContact(const MdtConstraintID c);

#define                 MdtContactDestroy(c) \
                            MdtConstraintDestroy(MdtContactQuaConstraint(c))
#define                 MdtContactEnable(c) \
                            MdtConstraintEnable(MdtContactQuaConstraint(c))
#define                 MdtContactDisable(c) \
                            MdtConstraintDisable(MdtContactQuaConstraint(c))
#define                 MdtContactIsEnabled(c) \
                            MdtConstraintIsEnabled(MdtContactQuaConstraint(c))
#define                 MdtContactSetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtContactQuaConstraint(j),k)
#define                 MdtContactGetSortKey(j) \
                            MdtConstraintGetSortKey(MdtContactQuaConstraint(j))

/*
  Contact accessors.
*/

MEPUBLIC
void              MEAPI MdtContactGetPosition(const MdtContactID c, MeVector3 v);
MEPUBLIC
void              MEAPI MdtContactGetNormal(const MdtContactID c, MeVector3 v);
MEPUBLIC
MeReal            MEAPI MdtContactGetPenetration(const MdtContactID c);
MEPUBLIC
void              MEAPI MdtContactGetDirection(const MdtContactID c, MeVector3 v);
MEPUBLIC
void              MEAPI MdtContactGetWorldVelocity(const MdtContactID c, MeVector3 v);
MEPUBLIC
MdtContactParamsID MEAPI MdtContactGetParams(const MdtContactID c);
MEPUBLIC
MdtContactID      MEAPI MdtContactGetNext(const MdtContactID c); /* DEPRECATED!! */
MEPUBLIC
MdtContactGroupID MEAPI MdtContactGetContactGroup(const MdtContactID c);
MEPUBLIC
void              MEAPI MdtContactGetRelativeVelocity(const MdtContactID c, MeVector3 v);        

/** Invalid ContactID indicating end of MdtContact linked list. */
extern const MdtContactID MdtContactInvalidID;

#define                 MdtContactGetBody(c, bodyindex) \
                            MdtConstraintGetBody(MdtContactQuaConstraint(c), bodyindex)
#define                 MdtContactGetUserData(c) \
                            MdtConstraintGetUserData(MdtContactQuaConstraint(c))
#define                 MdtContactGetWorld(c) \
                            MdtConstraintGetWorld(MdtContactQuaConstraint(c))
#define                 MdtContactGetForce(c, bodyindex, f) \
                            MdtConstraintGetForce(MdtContactQuaConstraint(c), bodyindex, f)
#define                 MdtContactGetTorque(c, bodyindex, t) \
                            MdtConstraintGetTorque(MdtContactQuaConstraint(c), bodyindex, t)

/*
  Contact mutators.
*/

MEPUBLIC
void              MEAPI MdtContactSetPosition(const MdtContactID c,
                            const MeReal x, const MeReal y, const MeReal z);
MEPUBLIC
void              MEAPI MdtContactSetNormal(const MdtContactID c,
                            const MeReal x, const MeReal y, const MeReal z);
MEPUBLIC
void              MEAPI MdtContactSetPenetration(const MdtContactID c, const MeReal p);
MEPUBLIC
void              MEAPI MdtContactSetDirection(const MdtContactID c,
                            const MeReal x, const MeReal y, const MeReal z);
MEPUBLIC
void              MEAPI MdtContactSetWorldVelocity(const MdtContactID c,
                            const MeReal x, const MeReal y, const MeReal z);
MEPUBLIC
void              MEAPI MdtContactSetParams(const MdtContactID c,
                            const MdtContactParamsID p);
MEPUBLIC
void              MEAPI MdtContactSetNext(const MdtContactID c, const MdtContactID nc); /* DEPRECATED!! */

#define                 MdtContactSetBodies(c, b1, b2) \
                            MdtConstraintSetBodies(MdtContactQuaConstraint(c), b1, b2)
#define                 MdtContactSetUserData(c, d) \
                            MdtConstraintSetUserData(MdtContactQuaConstraint(c), d)



#ifdef __cplusplus
}
#endif


#endif
