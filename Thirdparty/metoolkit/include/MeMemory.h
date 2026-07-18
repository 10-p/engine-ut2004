#ifndef _MEMEMORY_H /* -*- mode: C; -*- */
#define _MEMEMORY_H

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/05/02 16:51:54 $ - Revision: $Revision: 1.19.2.9 $

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
#include <MeAssert.h>
#include <stdlib.h>

/** @file
 * Memory management API.
 */

/* Stack allocation macro */
#if (defined __MWERKS__)
#   if (defined NGC)
#       include <alloca.h>
#   endif
#   define MeMemoryALLOCA(n) (__alloca(n))
#   define MeMemoryFREEA(b)  ((void) 0)
#elif (defined PS2 && __GNUC__ <= 2 && __GNUC_MINOR__ < 96)
#   define MeMemoryALLOCA(n) (alloca(n))
#   define MeMemoryFREEA(b)  ((void) 0)
#elif (defined __GNUC__)
#   define MeMemoryALLOCA(n) (__builtin_alloca(n))
#   define MeMemoryFREEA(b)  ((void) 0)
#elif (defined IRIX || defined TRIMEDIA)
#   include <alloca.h>
#   define MeMemoryALLOCA(n) (alloca(n))
#   define MeMemoryFREEA(b)  ((void) 0)
#elif (defined _MSC_VER)
#   include <malloc.h>
#   define MeMemoryALLOCA(n) (_alloca(n))
#   define MeMemoryFREEA(b)  ((void) 0)
#else
#   define MeMemoryALLOCA(n) (MeMemoryAPI.create(n))
#   define MeMemoryFREEA(b)  (MeMemoryAPI.destroy(b))
#endif


#define MeMemoryQUADALIGNED(n) \
    (MEASSERT((((long unsigned) (n)) % 16) == 0))

#define MeMemoryQUADALIGN(a) \
    ((void *) ((((long unsigned) (void *) (a)) + 15) &~ 15))

#define MeMemory64ALIGN(a) \
    ((void *) ((((long unsigned) (void *) (a)) + 63) &~ 63))

#ifdef MeMemoryALLOCA
# define MeMemoryQUADALIGNEDALLOCA(name, type, n) \
        void *name##Addr = \
            MeMemoryALLOCA(((n) * sizeof (type)) + 16); \
        type *name = (type *) MeMemoryQUADALIGN(name##Addr);
# define MeMemoryQUADALIGNEDFREEA(name) \
        MeMemoryFREEA(name##Addr)
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef void *(MEAPI *MeMemoryFuncPtrCreate)(size_t bytes);
typedef void *(MEAPI *MeMemoryFuncPtrCreateAligned)(size_t bytes, unsigned int alignment);
typedef void  (MEAPI *MeMemoryFuncPtrDestroy)(void *const block);
typedef void *(MEAPI *MeMemoryFuncPtrResize)(void *const block, size_t bytes);

typedef struct MeMemoryAPIStruct
{
    MeMemoryFuncPtrCreate create;
    MeMemoryFuncPtrCreate createZeroed;
    MeMemoryFuncPtrCreateAligned createAligned;

    MeMemoryFuncPtrDestroy destroy;
    MeMemoryFuncPtrDestroy destroyAligned;

    MeMemoryFuncPtrResize resize;
} MeMemoryAPIStruct;

MEPUBLIC
extern MeMemoryAPIStruct MeMemoryAPI;



#ifdef __cplusplus
}
#endif

#endif /* _MEMEMORY_H */
