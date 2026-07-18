#ifndef _MDTCONTACTGROUP_H
#define _MDTCONTACTGROUP_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.10.6.1 $

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
MdtContactGroupID MEAPI MdtContactGroupCreate(const MdtWorldID w);
MEPUBLIC
void              MEAPI MdtContactGroupReset(const MdtContactGroupID c);
MEPUBLIC
MdtConstraintID   MEAPI MdtContactGroupQuaConstraint(const MdtContactGroupID c);
MEPUBLIC
MdtContactGroupID MEAPI MdtConstraintDCastContactGroup(const MdtConstraintID c);

#define                 MdtContactGroupIsEnabled(c) \
                            MdtConstraintIsEnabled(MdtContactGroupQuaConstraint(c))
#define                 MdtContactGroupDisable(c) \
                            MdtConstraintDisable(MdtContactGroupQuaConstraint(c))
#define                 MdtContactGroupEnable(c) \
                            MdtConstraintEnable(MdtContactGroupQuaConstraint(c))
#define                 MdtContactGroupSetSortKey(j,k) \
                            MdtConstraintSetSortKey(MdtContactGroupQuaConstraint(j),k)
#define                 MdtContactGroupGetSortKey(j) \
                            MdtConstraintGetSortKey(MdtContactGroupQuaConstraint(j))

#define                 MdtContactGroupGetBody(c, bodyindex) \
                            MdtConstraintGetBody(MdtContactGroupQuaConstraint(c), bodyindex)

MEPUBLIC
void              MEAPI MdtContactGroupDestroy(MdtContactGroupID g);

#define                 MdtContactGroupGetWorld(c) \
                            MdtConstraintGetWorld(MdtContactGroupQuaConstraint(c))
#define                 MdtContactGroupGetForce(c, bodyindex, f) \
                            MdtConstraintGetForce(MdtContactGroupQuaConstraint(c), bodyindex, f)
#define                 MdtContactGroupTorque(c, bodyindex, t) \
                            MdtConstraintGetTorque(MdtContactGroupQuaConstraint(c), bodyindex, t)


MEPUBLIC
MdtContactID      MEAPI MdtContactGroupGetFirstContact(MdtContactGroupID c);
MEPUBLIC
MdtContactID      MEAPI MdtContactGroupGetNextContact(MdtContactGroupID g, MdtContactID c);
MEPUBLIC
void              MEAPI MdtContactGroupAppendContact(MdtContactGroupID g, MdtContactID c);
MEPUBLIC
void              MEAPI MdtContactGroupRemoveContact(MdtContactGroupID g, MdtContactID c);
MEPUBLIC
MdtContactID      MEAPI MdtContactGroupCreateContact(MdtContactGroupID g);
MEPUBLIC
void              MEAPI MdtContactGroupDestroyContact(MdtContactGroupID g, MdtContactID c);
MEPUBLIC
MeBool            MEAPI MdtContactGroupIsSwapped(MdtContactGroupID g);
MEPUBLIC
int               MEAPI MdtContactGroupGetCount(MdtContactGroupID g);
MEPUBLIC
MeReal            MEAPI MdtContactGroupGetNormalForce(MdtContactGroupID g);
MEPUBLIC
void              MEAPI MdtContactGroupSetGenerator(MdtContactGroupID, void *generator);
MEPUBLIC
void *            MEAPI MdtContactGroupGetGenerator(MdtContactGroupID group);

#define                 MdtContactGetUserData(c) \
                            MdtConstraintGetUserData(MdtContactQuaConstraint(c))
#define                 MdtContactSetUserData(c, d) \
                            MdtConstraintSetUserData(MdtContactQuaConstraint(c), d)


#define                 MdtContactGroupSetBodies(c, b1, b2) \
                            MdtConstraintSetBodies(MdtContactGroupQuaConstraint(c), b1, b2)

#ifdef __cplusplus
}
#endif


#endif
