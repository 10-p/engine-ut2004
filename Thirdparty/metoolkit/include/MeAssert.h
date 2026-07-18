#ifndef _MEASSERT_H /* -*- mode: C; -*- */
#define _MEASSERT_H

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/17 13:46:29 $ - Revision: $Revision: 1.18.2.2 $

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
 * Cross configuration and cross platform wrapper for assertions
 */

#ifndef _MECHECK
#  define MEASSERT(P)   ((void) 0)
#else
#  ifdef MEASSERT_IS_FATAL_ERROR
#    include <MeMessage.h>
#    define MEASSERT(P) do { if (!(P)) {MeFatalError(1,(#P));} } while(0)
#  elif WIN32
#    include <crtdbg.h>
#    define MEASSERT(P) _ASSERTE(P)
#  else
#    include <assert.h>
#    define MEASSERT(P) assert(P)
#  endif
#endif

/* Only correct if 'A' is a power of two. */
#define MEASSERTALIGNED(P,A)    MEASSERT((((MeUintPtr) (P)) \
                                    & ((MeUintPtr) (A)-1)) == 0)

#endif
