#ifndef __MEXMLOUTPUT_H
#define __MEXMLOUTPUT_H

/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:40 $ - Revision: $Revision: 1.8.2.3 $

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
  * @file XML outputting functions.
  */

#include <MeCall.h>
#include <MeStream.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct MeXMLOutput MeXMLOutput;
typedef struct tagNode     tagNode;

typedef int MeXMLElementID;

typedef enum
{
    kElement,
    kPCDATA,
    kNothing
} TagContents;

struct tagNode
{
    char        *tag;
    TagContents contents;
    tagNode     *next;
};

struct MeXMLOutput
{
    MeStream    stream;
    int         depth;
    tagNode     *tagHead;
};

MEPUBLIC
MeXMLOutput     *MEAPI MeXMLOutputCreate(MeStream stream);
MEPUBLIC
void             MEAPI MeXMLOutputDestroy(MeXMLOutput *op);
MEPUBLIC
int              MEAPI MeXMLWriteElement(MeXMLOutput *op, int parent, char *tag, ...);
MEPUBLIC
void             MEAPI MeXMLWritePCDATA(MeXMLOutput *op, char *format, ...);
MEPUBLIC
void             MEAPI MeXMLWriteComment(MeXMLOutput *op, char *c, ...);

#ifdef __cplusplus
}
#endif


#endif