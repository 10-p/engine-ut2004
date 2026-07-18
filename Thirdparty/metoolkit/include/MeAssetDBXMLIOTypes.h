#ifndef _MEASSETDBXMLIOTYPES__H
#define _MEASSETDBXMLIOTYPES__H

/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:05 $ - Revision: $Revision: 1.19.2.3 $

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
 * Types associated with MeAssetDBXMLIO.
 */

#include <MeXMLParser.h>
#include <MeXMLTree.h>
#include <MeXMLOutput.h>
#include <MeIDPool.h>

/*************************************************************\
  NON FILE FORMAT VERSION SPECIFIC TYPES
\*************************************************************/

typedef MeXMLError (MEAPI *MeFAssetHandler)(MeXMLElement *elem, PElement *parent);
typedef MeFAsset* (MEAPI *MeFAssetCreateFromFile)(MeAssetDB *parent, MeIDPool *IDPool, PElement *e);
typedef MeXMLElementID (MEAPI *MeFAssetWriteXML)(MeXMLOutput *op, MeFAsset *fa, MeXMLElementID parent);

typedef struct MeAssetDBXMLInput     MeAssetDBXMLInput;
typedef struct MeAssetDBXMLOutput    MeAssetDBXMLOutput;

struct MeAssetDBXMLInput
{
    MeAssetDB       *db;
    MeIDPool        *IDPool;
};

struct MeAssetDBXMLOutput
{
    MeAssetDB       *db;
    char            *fileHeader;
};

#endif
