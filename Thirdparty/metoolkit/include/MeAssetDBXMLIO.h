#ifndef __MEASSETDBXMLIO__H
#define __MEASSETDBXMLIO__H

/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/05/22 15:24:26 $ - Revision: $Revision: 1.7.2.2.4.1 $

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
 * Asset database XML import and export API.
 */

#include <MeAssetDB.h>
#include <MeAssetDBXMLIOTypes.h>
#include <MeXMLOutput.h>
#include <MeXMLTree.h>
#include <MeStream.h>

#ifdef __cplusplus
extern "C" {
#endif

const char            *MEAPI GetLatestKaFileVersionString();

/* input */
MEPUBLIC
MeAssetDBXMLInput     *MEAPI MeAssetDBXMLInputCreate(MeAssetDB *db, MeIDPool *IDPool);
MEPUBLIC
void                   MEAPI MeAssetDBXMLInputDestroy(MeAssetDBXMLInput *input);
MEPUBLIC                    
MeBool                 MEAPI MeAssetDBXMLInputRead( MeAssetDBXMLInput *input, MeStream stream);
MEPUBLIC                    
MeFAsset*			   MEAPI MeAssetDBXMLInputReadFirst(MeAssetDBXMLInput *input, MeStream stream);

/* output */
MEPUBLIC
MeAssetDBXMLOutput    *MEAPI MeAssetDBXMLOutputCreate(MeAssetDB *db);
MEPUBLIC
void                   MEAPI MeAssetDBXMLOutputDestroy(MeAssetDBXMLOutput *output);
MEPUBLIC                    
void                   MEAPI MeAssetDBXMLOutputWrite(MeAssetDBXMLOutput *output, MeStream stream);
MEPUBLIC
void                   MEAPI MeAssetDBXMLOutputSetFileHeader(MeAssetDBXMLOutput *output, char *header, ...);

#ifdef __cplusplus
}
#endif


#endif
