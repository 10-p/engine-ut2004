#ifndef _MDTCONSTRAINT_H
#define _MDTCONSTRAINT_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.36.2.1 $

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
 * MdtConstraint API functions.
 */

#include <MePrecision.h>
#include <MdtTypes.h>

#ifdef __cplusplus
extern "C"
{
#endif


/*
  Constraint functions are common to any type of constraint.
*/
MEPUBLIC
void              MEAPI MdtConstraintDestroy(const MdtConstraintID c);
MEPUBLIC
void              MEAPI MdtConstraintEnable(const MdtConstraintID c);
MEPUBLIC
void              MEAPI MdtConstraintDisable(const MdtConstraintID c);
MEPUBLIC
MeBool            MEAPI MdtConstraintIsEnabled(const MdtConstraintID c);

/*
  Constraint accessors.
*/

MEPUBLIC
MdtBodyID         MEAPI MdtConstraintGetBody(const MdtConstraintID c,
                            const unsigned int bodyindex);
MEPUBLIC
void             *MEAPI MdtConstraintGetUserData(const MdtConstraintID c);
MEPUBLIC
MdtWorldID        MEAPI MdtConstraintGetWorld(const MdtConstraintID c);
MEPUBLIC
void              MEAPI MdtConstraintGetForce(const MdtConstraintID c,
                            const unsigned int bodyindex, MeVector3 f);
MEPUBLIC
void              MEAPI MdtConstraintGetTorque(const MdtConstraintID c,
                            const unsigned int bodyindex, MeVector3 t);
MEPUBLIC
void              MEAPI MdtConstraintGetPosition(const MdtConstraintID c,
                            MeVector3 p);
MEPUBLIC
void              MEAPI MdtConstraintBodyGetPosition(const MdtConstraintID c,
                            const unsigned int bodyindex, MeVector3 p);
MEPUBLIC
void              MEAPI MdtConstraintBodyGetPositionRel(const MdtConstraintID c,
                            const unsigned int bodyindex, MeVector3 p);
MEPUBLIC
void              MEAPI MdtConstraintBodyGetAxis(const MdtConstraintID c,
                                                 const unsigned int bodyindex, 
                                                 MeVector3 a);

/* old name for above API call, deprecated; */
#define           MdtConstraintGetAxis(c, a) \
                  MdtConstraintBodyGetAxis((c), 1, (a))

MEPUBLIC
void              MEAPI MdtConstraintGetAxes(const MdtConstraintID c,
                            MeVector3 p,  MeVector3 o );
/* old name for above API call, deprecated; */
#define           MdtConstraintGetBothAxes(c, p, o) \
                  MdtConstraintGetAxes((c), (p), (o))
MEPUBLIC
void              MEAPI MdtConstraintBodyGetAxes(const MdtConstraintID c,
                                        const unsigned int bodyindex,
                                        MeVector3 p,  MeVector3 o );
/* old name for above API call, deprecated; */
#define           MdtConstraintBodyGetBothAxes(c, b, p, o) \
                  MdtConstraintBodyGetAxes((c), (b), (p), (o))
MEPUBLIC
void              MEAPI MdtConstraintBodyGetAxesRel(const MdtConstraintID c,
                                        const unsigned int bodyindex,
                                        MeVector3 p,  MeVector3 o );

MEPUBLIC
MeI32             MEAPI MdtConstraintGetSortKey(const MdtConstraintID c);


MEPUBLIC
int               MEAPI MdtConstraintGetRowCount(const MdtConstraintID c);

void              MEAPI MdtConstraintGetWorldAngularVelocity(
                            const MdtConstraintID c, MeVector3 av);
void              MEAPI MdtConstraintGetWorldLinearVelocity(
                            const MdtConstraintID c, MeVector3 lv);

/*
  Constraint mutators.
*/

MEPUBLIC
void              MEAPI MdtConstraintSetBodies(const MdtConstraintID c,
                            const MdtBodyID b1, const MdtBodyID b2);
MEPUBLIC
void              MEAPI MdtConstraintSetUserData(const MdtConstraintID c,
                            void *d);


MEPUBLIC
void              MEAPI MdtConstraintBodySetPosition(const MdtConstraintID c,
                            const unsigned int bodyindex,
                            const MeReal x, const MeReal y, const MeReal z);
MEPUBLIC
void              MEAPI MdtConstraintBodySetPositionRel(const MdtConstraintID c,
                            const unsigned int bodyindex,
                            const MeReal x, const MeReal y, const MeReal z);
MEPUBLIC
void              MEAPI MdtConstraintSetPosition(const MdtConstraintID c,
                            const MeReal x, const MeReal y, const MeReal z);

MEPUBLIC
void              MEAPI MdtConstraintSetAxis(const MdtConstraintID c,
                            const MeReal px, const MeReal py, const MeReal pz);

MEPUBLIC
void              MEAPI MdtConstraintSetAxes(const MdtConstraintID c,
                            const MeReal px, const MeReal py, const MeReal pz,
                            const MeReal ox, const MeReal oy, const MeReal oz);


/* old name for above API call, deprecated; */
#define           MdtConstraintSetBothAxis(c, px, py, pz, ox, oy, oz) \
                  MdtConstraintSetAxes((c), (px), (py), (pz), (ox), (oy), (oz))
MEPUBLIC
void              MEAPI MdtConstraintBodySetAxes(const MdtConstraintID c,
                            const unsigned int bodyindex,
                            const MeReal px, const MeReal py, const MeReal pz,
                            const MeReal ox, const MeReal oy, const MeReal oz);

/* old name for above API call, deprecated; */
#define           MdtConstraintBodySetBothAxis(c, b, px, py, pz, ox, oy, oz) \
                  MdtConstraintBodySetAxes((c), (b), (px), (py), (pz), (ox), (oy), (oz))
MEPUBLIC
void              MEAPI MdtConstraintBodySetAxesRel(const MdtConstraintID c,
                            const unsigned int bodyindex,
                            const MeReal px, const MeReal py, const MeReal pz,
                            const MeReal ox, const MeReal oy, const MeReal oz);

MEPUBLIC
void              MEAPI MdtConstraintSetSortKey(const MdtConstraintID c, MeI32 key);

MEPUBLIC
void              MEAPI MdtConstraintSetWorldLinearVelocity(
                            const MdtConstraintID c,
                            const MeReal vx, const MeReal vy, const MeReal vz);
MEPUBLIC
void              MEAPI MdtConstraintSetWorldAngularVelocity(
                            const MdtConstraintID c,
                            const MeReal ax, const MeReal ay, const MeReal az);
/*
  Iterators.
*/

MEPUBLIC
MdtConstraintID   MEAPI MdtConstraintGetFirst(const MdtWorldID w);
MEPUBLIC
MdtConstraintID   MEAPI MdtConstraintGetNext(const MdtConstraintID c);



#ifdef __cplusplus
}
#endif


#endif
