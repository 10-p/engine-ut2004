#ifndef _MECOMMANDLINE_H
#define _MECOMMANDLINE_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:06 $ - Revision: $Revision: 1.22.6.1 $

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
 * Handling of command line parameters.
 */

#include <MeCall.h>
#include <MeMemory.h>
#include <MeMessage.h>
#include <MePrecision.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MeCommandLineOptions
{
    int m_argc;
    const char ** p_argv;
} MeCommandLineOptions;

MEPUBLIC
MeCommandLineOptions* MEAPI MeCommandLineOptionsCreate(const int argc, const char ** argv);
MEPUBLIC
void MEAPI MeCommandLineOptionsDestroy(MeCommandLineOptions* options);
MEPUBLIC
void MEAPI MeCommandLineOptionsEat(MeCommandLineOptions* options, const int* eat, const int num_eat);
MEPUBLIC
int MEAPI MeCommandLineOptionsGetPos(MeCommandLineOptions* options, const char * arg);
MEPUBLIC
MeBool MEAPI MeCommandLineOptionsCheckFor(MeCommandLineOptions* options, const char * arg, const MeBool eat);
MEPUBLIC
MeBool MEAPI MeCommandLineOptionsCheckForList(MeCommandLineOptions* options, const char * arglist[], const MeBool eat);
MEPUBLIC
int MEAPI MeCommandLineOptionsGetNumeric(MeCommandLineOptions* options, const char * arg, const MeBool eat);
MEPUBLIC
double MEAPI MeCommandLineOptionsGetFloat(MeCommandLineOptions* options, const char * arg, const MeBool eat);
MEPUBLIC
char * MEAPI MeCommandLineOptionsGetString(MeCommandLineOptions* options, const char * arg, const MeBool eat);

#ifdef __cplusplus
}
#endif

#endif /* _MECOMMANDLINE_H */
