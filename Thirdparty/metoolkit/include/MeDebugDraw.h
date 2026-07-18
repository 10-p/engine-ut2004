#ifndef _DEBUGDRAW_H
#define _DEBUGDRAW_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:06 $ - Revision: $Revision: 1.4.6.1 $

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

#include <MeCall.h>
#include <MePrecision.h>


/** @file
 * Line drawing, intended for debugging, such as displaying contact info.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** 
 *  Basic line-draw function. Should take a start and end position in world
 *  space and r,g,b color values (between 0 and 1) to define color of the 
 *  line.
 */
typedef void (MEAPI *MeDebugLineFuncPtr)(MeVector3 start, MeVector3 end, 
                                         MeReal r, MeReal g, MeReal b);
/**
 *  Optional debug drawing functions used by toolkit.
 *  The user can replace the default (null) drawing functions with functions
 *  that render output as part of the the simulated scene. This can provide
 *  very useful information about what is going on in a scene.
 */
typedef struct MeDebugDrawAPIStruct
{
      MeDebugLineFuncPtr    line;
} MeDebugDrawAPIStruct;
    
extern MeDebugDrawAPIStruct MeDebugDrawAPI;

#ifdef __cplusplus
}
#endif

#endif
