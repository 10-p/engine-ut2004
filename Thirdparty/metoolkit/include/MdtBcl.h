#ifndef _MDTBCL_H
#define _MDTBCL_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:00 $ - Revision: $Revision: 1.59.2.3 $

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
    Function prototypes for adding constraint rows to the Kea solver.
*/

#include <MePrecision.h>
#include <MdtKea.h>

#ifdef __cplusplus
extern "C"
{
#endif


/**
    Function type for converting data structure 'constraint' into constraint
    equations and adding them to the MdtKeaConstraints struct. The 'params'
    field is a pointer to an MdtBclSolverParameters struct. We use void *'s here
    to avoid #including MdtTypes.h which would create a circular dependency.
*/
typedef void
(MEAPI *MdtBclAddConstraintFn)(MdtKeaConstraints *const keaConstraints,
                         void *const constraint,
                         const MdtKeaTransformation *const tlist,
                         MdtKeaBody *const blist[],
                         const void *const params);

/*
  Basic Constraint Library Add... Functions

  Functions for adding a constraint to the MdtKeaConstraints
  structure. Converts constraints from the geometric data
  structures above, into matrix rows to be passed to the solver.
*/

MEPUBLIC
void    MEAPI MdtBclAddBSJoint(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclAddHinge(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclAddPrismatic(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclAddCarWheel(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclAddContact(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclAddContactGroup(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclAddFixedPath(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclAddRPROJoint(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclAddUniversal(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclAddSkeletal(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclAddLinear1(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclAddLinear2(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclAddAngular3(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclAddSpring6(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclAddSpring(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclAddConeLimit(MdtKeaConstraints *const clist,
            void *const constraint,
            const MdtKeaTransformation *const tlist,
            MdtKeaBody *const blist[],
            const void *const params);

MEPUBLIC
void    MEAPI MdtBclInitConstraintRowList(MdtKeaConstraints *const clist);
MEPUBLIC
void    MEAPI MdtBclEndPartition(MdtKeaConstraints  *const clist);
MEPUBLIC
void    MEAPI MdtBclStartPartition(MdtKeaConstraints *const clist);
MEPUBLIC
void    MEAPI MdtBclEndConstraint(MdtKeaConstraints  *const clist,
            const unsigned rows_added);

/*
  The following are useful macros for finding the maximum number of
  constraint rows that can be added by a type of constraints.

  This can be used to provide an upper bound on the amount of memory to
  allocate fo constraint input. Some rows may be actuation or limits.
*/

/** Maximum rows added by a Ball And Socket Joint constraint. */
#define MdtBclGETMAXROWSBSJOINT         (3)

/** Maximum rows added by a Hinge constraint. */
#define MdtBclGETMAXROWSHINGE           (6)

/** Maximum rows added by a Prismatic constraint. */
#define MdtBclGETMAXROWSPRISMATIC       (6)

/** Maximum rows added by a Car Wheel constraint. */
#define MdtBclGETMAXROWSCARWHEEL        (6)

/** Maximum rows added by a Contact constraint. */
#define MdtBclGETMAXROWSCONTACT         (3)

/** Maximum rows added by a Fixed Path constraint. */
#define MdtBclGETMAXROWSFIXEDPATH       (3)

/** Maximum rows added by a Relative Position Relative Orientation constraint. */
#define MdtBclGETMAXROWSRPROJOINT       (6)

/** Maximum rows added by a Universal Joint constraint. */
#define MdtBclGETMAXROWSUNIVERSAL       (4)

/** Maximum rows added by a Skeletal Joint constraint. */
#define MdtBclGETMAXROWSSKELETAL        (5)

/** Maximum rows added by a Linear1 constraint. */
#define MdtBclGETMAXROWSLINEAR1         (1)

/** Maximum rows added by a Linear2 constraint. */
#define MdtBclGETMAXROWSLINEAR2         (2)

/** Maximum rows added by a Angular3 constraint. */
#define MdtBclGETMAXROWSANGULAR3        (3)

/** Maximum rows added by a Angular3 constraint. */
#define MdtBclGETMAXROWSSPRING6        (6)

/** Maximum rows added by a Spring constraint. */
#define MdtBclGETMAXROWSSPRING           (1)

/** Maximum rows added by a ConeLimit constraint. */
#define MdtBclGETMAXROWSCONELIMIT       (1)

/** Maximum possible rows added by any constraint. */
#define MdtBclGETMAXROWS                 (6)


#ifdef __cplusplus
}
#endif

#endif
