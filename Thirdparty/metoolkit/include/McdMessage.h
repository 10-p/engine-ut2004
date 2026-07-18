#ifndef _MCDMESSAGE_H
#define _MCDMESSAGE_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/23 15:44:47 $ - Revision: $Revision: 1.9.6.2 $

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

/**
    @file
    Error messages
*/
#include <MePrecision.h>

enum
{
  kMcdErrorTypeFatal,
  kMcdErrorTypeWarning,
  kMcdErrorTypeInfo
};

/* Define enum for all error numbers: */
enum {
#define ERR(errNum,errType,msg) errNum
#include "McdCoreErrorList.h"
#undef ERR
};

struct McdErrorDescription
{
  MeI16 m_errNum;
  MeI16 m_errorLevel;
  MeI16 m_errorCount;
  const char* m_description;
};

#ifdef __cplusplus
extern "C" {
#endif

extern McdErrorDescription gMcdCoreErrorList[];

void McdError(McdErrorDescription* ErrorList, int errorCode,
    const char *message, const char *fn, const char *file, int line);

#define McdCoreError(errorCode,message,fn,file,line) \
    McdError(gMcdCoreErrorList,(errorCode),(message), fn, (file), (line))

#ifdef __cplusplus
}
#endif

#endif
