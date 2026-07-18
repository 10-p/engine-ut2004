#ifndef _MEAPP_H
#define _MEAPP_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:02 $ - Revision: $Revision: 1.38.6.3 $

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
 * Karma demo support utilities. Includes useful cross-platform functionality such as
 * mouse picking support, support for drawing complex geometries and contact
 * drawing. Works with MeViewer2.
 */

#include <Mst.h>
#include <MeViewer.h>
#include <MeAppTypes.h>


#ifdef __cplusplus
extern "C" {
#endif

MeApp          *MEAPI MeAppCreateFromUniverse(const MstUniverseID u, RRender *rc);

MstUniverseID   MEAPI MeAppGetMstUniverse(MeApp *app);

MeApp          *MEAPI MeAppCreate(const MdtWorldID world,
                                  const McdSpaceID space,RRender *rc);

void            MEAPI MeAppDestroy(MeApp* const app);

/* Graphics-related functions */

void            MEAPI MeAppDrawContactsInit(MeApp* const app, float color[4],int max);

void            MEAPI MeAppDrawContacts(const MeApp *app);
void            MEAPI MeAppToggleDrawContacts(MeApp *app, MeBool d);
void            MEAPI MeAppSetContactDrawLength(MeApp *app, MeReal length);

void            MEAPI MeAppFindClickDir(const MeApp *const app, int x, int y,
                          MeVector3 normClickDir);

void            MEAPI MeAppFindClickPos(const MeApp *const app, int x, int y,
                          MeVector3 clickPos);

McdModelID      MEAPI MeAppPickMcdModel(const MeApp *const app,
                                         int x, int y,
                                         MeVector3 normClickDir,
                                         MeVector3 pos, MeBool orth);

void            MEAPI MeAppUpdateMouseSpring(MeApp *app);

void            MEAPI MeAppMousePickCB(RRender *rc, int x, int y,
                        int modifiers, RMouseButtonWhich which,
                        RMouseButtonEvent event,void *userdata);

void            MEAPI MeAppStep(MeApp *app);

#ifdef __cplusplus
}
#endif

#endif
