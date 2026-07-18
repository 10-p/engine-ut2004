#ifndef _MCDCONTACT_H
#define _MCDCONTACT_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.21.6.2 $

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
  Geometric contact point information
*/

#include <MePrecision.h>
#include <McdCTypes.h>

#ifdef __cplusplus
extern "C" {
#endif


/** Utility function for reducing a list of contacts to
    a smaller list. Useful mostly when several models compose one object.
*/
MEPUBLIC
int MEAPI McdContactSimplify(const MeReal* avgNormal,
                             McdContact* contacts_src, 
                             const int nContacts_src,
                             McdContact* contacts_dst, 
                             const int nContacts_dest,
                             const int faceNormalsFirst,
                             const MeReal scale);
#ifdef __cplusplus
}
#endif

#endif /* _MCDCONTACT_H */
