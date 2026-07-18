#ifndef _MESTREAM_H /* -*- mode: C; -*- */
#define _MESTREAM_H

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/18 14:25:11 $ - Revision: $Revision: 1.21.2.2 $

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
    Streaming functions. This is not a proper streaming implementation.
    For platforms that support streaming, MeStream just wraps the function
    fopen/fclose/fread/fwrite. For platforms that don't support streaming,
    the functions open/close/read/write are used. MeStream is used in case
    we want to implement a full streaming solution on all platforms.
*/

#include <MeSimpleFile.h>

#if (defined LINUX || defined WIN32 || defined _XBOX)
#   define MeStreamSTDIO            1
#   define MeStreamSIMPLE           0
#else
#   define MeStreamSTDIO            0
#   define MeStreamSIMPLE           1
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef struct _MeStream    _MeStream;
typedef _MeStream           *MeStream;

struct _MeStream
{
    const char  *filename;
#if (MeStreamSIMPLE)
    /* On PS2 and other we load the whole file into memory at the start
        to speed things up a little, or we simply don't have 'stdio'. */
    int         handle;
    char        *buffer;
    int         bufferSize;
    int         whereAmI;
    MeBool      modified;
#endif

#if (MeStreamSTDIO)
    /* This allows a memory block to be used for a stream rather than
       a file. It is very similar to the PS2 mechanism, but kept separate
       for flexibility */
    MeBool      bUseMemblock;

    void        *handle;
    char        *buffer;
    MeBool      bGrowFast;  /* Buffer doubled when resizing is needed */
    unsigned int bufSize;   /* Bytes allocated in buffer */
    unsigned int bufLength; /* Bytes used in buffer */
    unsigned int curIndex;  /* Current insertion point */
#endif
};

MEPUBLIC
MeStream      MEAPI MeStreamOpen(const char *filename,MeOpenMode_enum mode);
MEPUBLIC
MeStream      MEAPI MeStreamOpenWithSearch(const char *filename,MeOpenMode_enum mode);

MEPUBLIC
MeStream      MEAPI MeStreamOpenAsMemBuffer(unsigned int initialSize);
MEPUBLIC
MeStream      MEAPI MeStreamCreateFromMemBuffer(char *buffer, unsigned int bufLength, unsigned int bufSize);
MEPUBLIC
void          MEAPI MeStreamMemBufferFreeSlackSpace(MeStream stream);
MEPUBLIC
void          MEAPI MeStreamMemBufferUseConservativeGrowth(MeStream stream, MeBool bConsGrowth);

MEPUBLIC
void          MEAPI MeStreamClose(MeStream stream);
MEPUBLIC
size_t        MEAPI MeStreamRead(void *buffer,size_t size,size_t count,MeStream stream);
MEPUBLIC
char *        MEAPI MeStreamReadLine(char *string, int n, MeStream stream);
MEPUBLIC
size_t        MEAPI MeStreamWrite(void *buffer,size_t size,size_t count,MeStream stream);
MEPUBLIC
void          MEAPI MeStreamRewind(MeStream stream);

#ifdef __cplusplus
}
#endif

#endif
