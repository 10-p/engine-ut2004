#ifndef _MESIMPLEFILE_H /* -*- mode: C; -*- */
#define _MESIMPLEFILE_H

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/18 14:25:11 $ - Revision: $Revision: 1.11.6.2 $

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
    Cross platform wrappers for simple file handling.
*/

#include <stdarg.h>
#include <stdio.h>

#include <MeCall.h>
#include <MePrecision.h>

#ifdef __cplusplus
extern "C" {
#endif


/**
 * File modes.
 */
typedef enum
{
    kMeOpenModeRDONLY,  /**< Open as read only. */
    kMeOpenModeWRONLY,  /**< Open new file for writing. */
    kMeOpenModeRDWR,    /**< Open existing file (for appending?). */
    kMeOpenModeRDBINARY /**< Open for binary read. */
} MeOpenMode_enum;

/** MeLseek constants. Defines the starting point from which we travel
    @param offset bytes.
*/
typedef enum
{
    kMeSeekSET, /** < Start of file. */
    kMeSeekCUR, /** < Current file pointer position. */
    kMeSeekEND  /** < End of file. */
} MeSeekOrigin_enum;

MEPUBLIC
int               MEAPI MeOpenRaw(const char *filename, MeOpenMode_enum mode);

/** Opens a file.*/
MEPUBLIC
int               MEAPI MeOpen(const char *filename, MeOpenMode_enum mode);

/** Searches MathEngine paths and MeOpens file */
MEPUBLIC
int               MEAPI MeOpenWithSearch(const char *filename,
                            MeOpenMode_enum mode);

/** Seeks within a file */
MEPUBLIC
int               MEAPI MeLseek(int file, int offset,
                            MeSeekOrigin_enum origin);

/** Reads from a file */
MEPUBLIC
int               MEAPI MeRead(int file, void *buf, int count);

/** Writes to a file */
MEPUBLIC
int               MEAPI MeWrite(int file, void *buf, int count);

/** Closes a file */
MEPUBLIC
int               MEAPI MeClose(int file);

/** Get search path from array */
MEPUBLIC
const char       *MEAPI MeGetDefaultFileLocation(int i);

/** Gets the contents of a file and puts it into memory */
MEPUBLIC
char             *MEAPI MeLoadWholeFileHandle(const int f, int* size);

/** Save a block of memory to a file */
MEPUBLIC
void                MEAPI MeSaveWholeFileHandle(const int f,
                        const char *data, const int size);

#ifdef __cplusplus
}
#endif

#endif /* _MESIMPLEFILE_H */
