#ifndef _MECALL_H
#define _MECALL_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:06 $ - Revision: $Revision: 1.13.4.1 $

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
 * Calling convention for standard library functions
 */

#ifdef WIN32
#define MEAPI_CDECL    __cdecl
#define MEAPI_STDCALL  __stdcall
#define MEAPI_FASTCALL __fastcall
#define MEAPI          MEAPI_STDCALL
#else
#define MEAPI_CDECL
#define MEAPI_STDCALL
#define MEAPI_FASTCALL
#define MEAPI
#endif

#ifdef WIN32
 #if defined KARMADLL
  #if defined KARMADLL_EXPORTS
   #define MEPUBLIC __declspec(dllexport)
  #else
   #define MEPUBLIC __declspec(dllimport)
  #endif
 #else
  #define MEPUBLIC
 #endif
#else
 #define MEPUBLIC
#endif

#endif /* _MECALL_H */
