#ifndef _MESTRING_H
#define _MESTRING_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:07 $ - Revision: $Revision: 1.8.6.1 $

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
    Cross-platform string functionality.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#if defined(WIN32)
#define snprintf _snprintf
#define vsnprintf _vsnprintf
#endif

#if(defined(PS2) || defined(MEEE) || defined(TRIMEDIA))
#define NEED_SNPRINTF
#endif

#endif /* _MESTRING_H */
