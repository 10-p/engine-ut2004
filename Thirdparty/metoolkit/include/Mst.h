#ifndef _MST_H
#define _MST_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:40 $ - Revision: $Revision: 1.28.4.1 $

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
 * MathEngine Simulation Toolkit Main Header.
 * This library provides useful functions for creating and simulating objects
 * using both the MathEngine Dynamics Toolkit and the MathEngine Collision
 * Toolkit.
 * An MstUniverse contains an McdSpace collision farfield, an MdtWorld, an
 * MstBridge, and some buffers for moving contacts between MCD and MDT.
 * MstUniverseCreate creates these things in one handy function.
 * A collision McdModel is the main structure, with an optional dynamics
 * MdtBody associated with it.
 * Mst contains functions for creating these together (for convenience),
 * or seperately and associating them with each other using McdModelSetBody.
 * It can also set the mass and inertia tensor of an MdtBody to a sensible
 * default based on the collision geometry and a density.
 * MstUniverseStep is a dynamics and collision 'main loop'.
 */

/* ME Globals and Memory */
#include <MePrecision.h>
#include <MeAssert.h>
#include <MeMessage.h>

#include <MeMemory.h>

/* MathEngine Dynamics Toolkit */
#include <Mdt.h>

/* MathEngine Collision Toolkit */
#include <Mcd.h>
#include <McdPrimitives.h>

#include <MstTypes.h> 

#include <MstBridge.h>
#include <MstUniverse.h>
#include <MstModelDynamics.h>
#include <MstUtils.h>


#endif
