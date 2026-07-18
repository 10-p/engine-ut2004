#ifndef _MDTCHECKMACROS_H
#define _MDTCHECKMACROS_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.17.2.3 $

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

#include <MeMessage.h>

#ifdef _MECHECK

/*
 * Checks the validity of an MdtWorldID.
 */
#define MdtCHECKWORLD(x,f) \
    do { if ((x) == 0) MeFatalError(0,"Invalid MdtWorldID detected" \
        " in %s().",#f); } while(0)

/*
 * Checks the validity of an MdtBodyID.
 */
#define MdtCHECKBODY(x,f) \
    do { if ((x) == 0 || (x)->keaBody.tag != MdtKeaBODYVER1) \
             MeFatalError(0,"Invalid MdtBodyID detected in %s().",#f); \
       } while(0)

/*
 * Checks the validity of an MdtBodyID but allows the body to be NULL.
 */
#define MdtCHECKBODY_NULL_OK(x,f) \
    do { if ((x) && (x)->keaBody.tag != MdtKeaBODYVER1) \
             MeFatalError(0,"Invalid MdtBodyID detected in " \
             "%s().",#f); } while(0)

/*
 * Checks the validity of an MdtBaseConstraintID.
 */
#define MdtCHECKCONSTRAINT(x,f) \
    do { if ((x) == 0) MeFatalError(0,"Invalid MdtConstraintID " \
            "detected in %s().",#f); } while(0)


/*
 * Checks that a constraints bodies have been set up. The second body
 * is allowed to be null with this macro.
 */
#define MdtCHECKCONSTRAINTBODIES_NULL_OK(x,f) \
    do { if (((MdtBaseConstraint*)(x))->head.mdtbody[0] == 0) \
        MeFatalError(0,"Invalid constraint bodies detected" \
                " in %s().",#f); } while(0)

/*
 * Checks that a constraints bodies have been set up. Both bodies
 * must be non zero with this macro.
 */
#define MdtCHECKCONSTRAINTBODIES(x,f) \
    do { if (((MdtBaseConstraint*)(x))->head.mdtbody[0] == 0 || \
         ((MdtBaseConstraint*)(x))->head.mdtbody[1] == 0) \
        MeFatalError(0,"Invalid constraint bodies detected" \
                " in %s().",#f); } while(0)

/*
 * Checks the validity of an MdtHingeID.
 */
#define MdtCHECKHINGE(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclHINGE) \
             MeFatalError(0,"Invalid MdtHingeID detected in %s().",#f); \
       } while(0)

/*
 * Checks the validity of an MdtBSJointID.
 */
#define MdtCHECKBSJOINT(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclBSJOINT) \
             MeFatalError(0,"Invalid MdtBSJointID detected in %s().",#f); \
       } while(0)

/*
 * Checks the validity of an MdtContactID.
 */
#define MdtCHECKCONTACT(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclCONTACT) \
             MeFatalError(0,"Invalid MdtContactID detected in %s().",#f); \
       } while(0)


#define MdtCHECKCONTACTGROUP(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclCONTACTGROUP) \
             MeFatalError(0,"Invalid MdtContactGroupID detected in %s().",#f); \
       } while(0)

/*
 * Checks the validity of a contact params id.
 */
#define MdtCHECKCONTACTPARAMS(x,f) \
    do { if ((x) == 0) MeFatalError(0,"Invalid MdtContactParamsID " \
                        "detected in %s().",#f); } while(0)

/*
 * Checks the validity of an MdtPrismaticID.
 */
#define MdtCHECKPRISMATIC(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclPRISMATIC) \
             MeFatalError(0,"Invalid MdtPrismaticID detected in %s().",#f); \
       } while(0)
/*
 * Checks the validity of an MdtCarWheelID.
 */
#define MdtCHECKCARWHEEL(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclCARWHEEL) \
             MeFatalError(0,"Invalid MdtCarWheelID detected in %s().",#f); \
       } while(0)

/*
 * Checks the validity of an MdtFixedPathID.
 */
#define MdtCHECKFIXEDPATH(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclFIXEDPATH) \
             MeFatalError(0,"Invalid MdtFixedPathID detected in %s().",#f); \
       } while(0)


/*
 * Checks the validity of an MdtRPROJointID.
 */
#define MdtCHECKRPROJOINT(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclRPROJOINT) \
             MeFatalError(0,"Invalid MdtRPROJointID detected in function %s.",#f); \
       } while(0)



/*
 * Checks the validity of an MdtUniversalID.
 */
#define MdtCHECKUNIVERSAL(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclUNIVERSAL) \
             MeFatalError(0,"Invalid MdtUniversalID detected in %s().",#f); \
       } while(0)

/*
 * Checks the validity of an MdtSkeletalID.
 */
#define MdtCHECKSKELETAL(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclSKELETAL) \
             MeFatalError(0,"Invalid MdtSkeletalID detected in %s().",#f); \
       } while(0)

/*
 * Checks the validity of an MdtLinear1ID.
 */
#define MdtCHECKLINEAR1(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclLINEAR1) \
             MeFatalError(0,"Invalid MdtLinear1ID detected in %s().",#f); \
       } while(0)

/*
 * Checks the validity of an MdtLinear2ID.
 */
#define MdtCHECKLINEAR2(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclLINEAR2) \
             MeFatalError(0,"Invalid MdtLinear2ID detected in %s().",#f); \
       } while(0)


/*
 * Checks the validity of an MdtAngular3ID.
 */
#define MdtCHECKANGULAR3(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclANGULAR3) \
             MeFatalError(0,"Invalid MdtAngular3ID detected in function %s.",#f); \
       } while(0)

/*
 * Checks the validity of an MdtAngular3ID.
 */
#define MdtCHECKSPRING6(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclSPRING6) \
             MeFatalError(0,"Invalid MdtSpring6ID detected in function %s.",#f); \
       } while(0)

/*
 * Checks the validity of an MdtUserConstraintID.
 */
#define MdtCHECKUSERCONSTRAINT(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclUSER) \
             MeFatalError(0,"Invalid MdtUserConstraintID detected in function %s.",#f); \
       } while(0)


/*
 * Checks the validity of an MdtSpringID.
 */
#define MdtCHECKSPRING(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclSPRING) \
             MeFatalError(0,"Invalid MdtSpringID detected in function %s.",#f); \
       } while(0)


/*
 * Checks the validity of an MdtConeLimitID.
 */
#define MdtCHECKCONELIMIT(x,f) \
    do { if ((x) == 0 || (x)->head.tag != MdtBclCONELIMIT) \
             MeFatalError(0,"Invalid MdtConeLimitID detected in function %s.",#f); \
       } while(0)

/*
 * Checks the validity of an MdtSingleLimitID.
 */
#define MdtCHECKSINGLELIMIT(x,f) \
    do { if ((x) == 0) MeFatalError(0,"Invalid MdtSingleLimitID " \
                "detected in %s().",#f); } while(0)

/*
 * Checks the validity of an MdtLimitID.
 */
#define MdtCHECKLIMIT(x,f) \
    do { if ((x) == 0) MeFatalError(0,"Invalid MdtLimitID detected" \
                " in %s().",#f); } while(0)

/*
 * Checks the validity of a body index (must be 0 or 1).
 */
#define MdtCHECKBODYINDEX(x,f) \
    do { if ((x) > 1) MeFatalError(0, "Invalid body index " \
                "detected in %s().",#f); } while(0)

/*
 * Checks the validity of a limit index (must be 0, 1 or 2).
 */
#define MdtCHECKLIMITINDEX3(x,f) \
    do { if ((x) > 2) MeFatalError(0, "Invalid limit index " \
                "detected in %s().",#f); } while(0)

#else
#   define MdtCHECKWORLD(x,f)
#   define MdtCHECKBODY(x,f)
#   define MdtCHECKBODY_NULL_OK(x,f)
#   define MdtCHECKCONSTRAINT(x,f)
#   define MdtCHECKCONSTRAINTBODIES_NULL_OK(x,f)
#   define MdtCHECKCONSTRAINTBODIES(x,f)
#   define MdtCHECKHINGE(x,f)
#   define MdtCHECKBSJOINT(x,f)
#   define MdtCHECKCONTACT(x,f)
#   define MdtCHECKCONTACTGROUP(x,f)
#   define MdtCHECKCONTACTPARAMS(x,f)
#   define MdtCHECKPRISMATIC(x,f)
#   define MdtCHECKCARWHEEL(x,f)
#   define MdtCHECKFIXEDPATH(x,f)
#   define MdtCHECKRPROJOINT(x,f)
#   define MdtCHECKUNIVERSAL(x,f)
#   define MdtCHECKSKELETAL(x,f)
#   define MdtCHECKLINEAR1(x,f)
#   define MdtCHECKLINEAR2(x,f)
#   define MdtCHECKANGULAR3(x,f)
#   define MdtCHECKSPRING6(x,f)
#   define MdtCHECKUSERCONSTRAINT(x,f)
#   define MdtCHECKSPRING(x,f)
#   define MdtCHECKCONELIMIT(x,f)
#   define MdtCHECKSINGLELIMIT(x,f)
#   define MdtCHECKLIMIT(x,f)
#   define MdtCHECKBODYINDEX(x,f)
#   define MdtCHECKLIMITINDEX3(x,f)
#endif


#endif


