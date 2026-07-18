#ifndef _MDTDEFAULTS_H /* -*- mode: C; -*- */
#define _MDTDEFAULTS_H

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:59 $ - Revision: $Revision: 1.7.2.4 $

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
 * Mdt default values
 */

#define MDTWORLD_DEFAULT_GRAVITY_X          0
#define MDTWORLD_DEFAULT_GRAVITY_Y          0
#define MDTWORLD_DEFAULT_GRAVITY_Z          0

/* the default epsilon is scaled by 1/massScale */

#define MDTWORLD_DEFAULT_EPSILON            (MeReal)0.01    
#define MDTWORLD_DEFAULT_GAMMA              (MeReal)0.2
#define MDTWORLD_DEFAULT_VELOCITY_ZERO_TOL  (MeReal)0.03
#define MDTWORLD_DEFAULT_MIN_SAFETIME       (MeReal)0.001

/* these four are auto-scaled by lengthScale: */
#define MDTWORLD_DEFAULT_VEL_THRESH         (MeReal)0.02
#define MDTWORLD_DEFAULT_VELROT_THRESH      (MeReal)0.001
#define MDTWORLD_DEFAULT_ACC_THRESH         (MeReal)0.05
#define MDTWORLD_DEFAULT_ACCROT_THRESH      (MeReal)0.05

#define MDTWORLD_DEFAULT_ALIVE_TIME_THRESH  (MeReal)0.2

#define MDTWORLD_DEFAULT_FRICTION_RATIO     0
#define MDTWORLD_DEFAULT_ZERO_ROW_BONUS     150
#define MDTWORLD_DEFAULT_TO_WORLD_BONUS     100
#define MDTWORLD_PENETRATION_BIAS           15
#define MDTWORLD_NORM_VEL_BIAS              (MeReal)6.5
#define MDTWORLD_ROW_COUNT_BIAS             40
#define MDTWORLD_DEFAULT_NON_AUTO_BONUS     40

/*
    Mass and inertia defaults are that of a sphere of mass 1 and radius 1.
*/

/* these are scaled by the massScale */
#define MDTBODY_DEFAULT_MASS                1
#define MDTBODY_DEFAULT_INERTIA             (MeReal)0.4

#define MDTBODY_MASS_RANGE_LIMIT            (MeReal)100
#define MDTBODY_INERTIA_RANGE_LIMIT         (MeReal)100

#define MDTBODY_DEFAULT_LINEAR_DAMPING      0  
#define MDTBODY_DEFAULT_ANGULAR_DAMPING     0  
#define MDTBODY_DEFAULT_FAST_SPIN_X         0  
#define MDTBODY_DEFAULT_FAST_SPIN_Y         1  
#define MDTBODY_DEFAULT_FAST_SPIN_Z         0  

/*
    Carwheel joint defaults.
*/
#define MDTCARWHEEL_DEFAULT_POS_X           0 
#define MDTCARWHEEL_DEFAULT_POS_Y           0
#define MDTCARWHEEL_DEFAULT_POS_Z           0

#define MDTCARWHEEL_DEFAULT_STEER_AXIS_X    0
#define MDTCARWHEEL_DEFAULT_STEER_AXIS_Y    0
#define MDTCARWHEEL_DEFAULT_STEER_AXIS_Z    1

#define MDTCARWHEEL_DEFAULT_HINGE_AXIS_X    0
#define MDTCARWHEEL_DEFAULT_HINGE_AXIS_Y    1
#define MDTCARWHEEL_DEFAULT_HINGE_AXIS_Z    0

#define MDTCARWHEEL_DEFAULT_STEER_MAXFORCE  1e9
#define MDTCARWHEEL_DEFAULT_STEER_DESVEL    0
#define MDTCARWHEEL_DEFAULT_STEER_LOCK      0
#define MDTCARWHEEL_DEFAULT_HINGE_MAXFORCE  0
#define MDTCARWHEEL_DEFAULT_HINGE_DESVEL    0
#define MDTCARWHEEL_DEFAULT_SUSP_HI         1e9
#define MDTCARWHEEL_DEFAULT_SUSP_LO         -1e9
#define MDTCARWHEEL_DEFAULT_SUSP_REF        0
#define MDTCARWHEEL_DEFAULT_SUSP_SOFT       0
#define MDTCARWHEEL_DEFAULT_SUSP_STIFF      0
#define MDTCARWHEEL_DEFAULT_SUSP_DAMP       0

/* Skeletal defaults. */
#define MDTSKELETAL_DEFAULT_TWIST_ANGLE     (ME_PI/4)
#define MDTSKELETAL_DEFAULT_PRI_ANGLE       (ME_PI/4)
#define MDTSKELETAL_DEFAULT_SEC_ANGLE       (ME_PI/4)


#define MDTMAXMATRIXSIZE_MAX                (MeMathFLOOR4(INT_MAX))

#endif

