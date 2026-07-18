#ifndef _MEAPPTYPES_H
#define _MEAPPTYPES_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:02 $ - Revision: $Revision: 1.16.6.2 $

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
 * Types associated with the MeApp demo support utilities.
 */

#include <MstTypes.h>
#include <MeViewerTypes.h>

typedef struct MeApp                MeApp;
typedef struct MeAppContactDrawInfo MeAppContactDrawInfo;
typedef struct MeAppMousePickInfo   MeAppMousePickInfo;

struct MeAppMousePickInfo
{
    RGraphic* line;
    MdtBodyID dragBody;

    /* point that was grabbed in the body reference frame. */
    MeVector3 grabPosition;

    /* desired location for grabPosition in world reference frame. */
    MeVector3 desiredPosition;
    MeReal range;
    MeReal mouseSpringStiffness;
    MeReal mouseSpringDamping;
};

/* MeApp works with one MstUniverse, and one render context */
struct MeApp
{
    MdtWorldID world;
    McdSpaceID space;
    MstUniverseID universe;
    RRender *rc;
    MeAppContactDrawInfo *contactDrawInfo;
    MeBool drawContacts;
    MeAppMousePickInfo mouseInfo;
};

struct MeAppContactDrawInfo
{
    RGraphic **contactG;
    float color[4];
    int maxContacts;
    int contactsDrawn;
    MeReal length;
};



#endif
