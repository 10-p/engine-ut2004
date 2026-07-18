#ifndef _MEINLINE_H
#define _MEINLINE_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:06 $ - Revision: $Revision: 1.9.6.1 $

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
 *  Inlining control. If you want to use inlining in a public header, include
 *  this file and write "#define MePUT_FUNCTIONS_HERE_IF_NOT_INLINED 1" in the
 *  source file that includes the header.
 *
 *  Usage in the header that contains inlines is:
 *
 *  #if (!MeDEFINE)
 *    prototype inline functions here like this:
 *    void someFunc(void);
 *  #endif
 *
 *  #if (MeDEFINE)
 *    define inline functions here like this:
 *    MeINLINE someFunc(void)
 *    {
 *     ...
 *    }
 *
 *  Inlining can be turned off by defining MeDONTINLINE in the makefile.
 */

#if (MeDONTINLINE)
#   define MeDEFINE MePUT_FUNCTIONS_HERE_IF_NOT_INLINED
#   define MeINLINE
#elif (__cplusplus)
#   define MeDEFINE 1
#   define MeINLINE static inline
#elif (defined __GNUC__ || defined _MSC_VER)
#   define MeDEFINE 1
#   define MeINLINE static __inline
#elif (defined __MWERKS__)
#   define MeDEFINE 1
#   define MeINLINE static inline
#else
#   define MeDEFINE MePUT_FUNCTIONS_HERE_IF_NOT_INLINED
#   define MeINLINE
#endif


#endif /* _MEINLINE_H */
