#ifndef _RMENU_H
#define _RMENU_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:39 $ - Revision: $Revision: 1.9.8.2 $

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

#include <MeViewer.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
   Switch on the main menu.

   @param rc The renderer to display the default menu of.
 */
void   MEAPI RRenderDisplayDefaultMenu(RRender *rc);

/**
   Discover which menu is currently being displayed.

   @param rc The render context in question.
   @return A pointer to the currently displayed menu.
 */
RMenu* MEAPI RRenderGetCurrentMenu(RRender *rc);

/**
   Stop displaying a menu.

   @param rc The render context to cease display of a menu in.
 */
void   MEAPI RRenderHideCurrentMenu(RRender *rc);

/**
   Move the active menu entry one place down.

   @param rc The render context in question.
 */
void   MEAPI RMenuNextEntry(RRender *rc);
/**
   Move the active menu entry one place up.

   @param rc The render context in question.
 */
void   MEAPI RMenuPreviousEntry(RRender *rc);
/**
    Execute the first action of this entry - e.g. reduce the value of a
    value entry.

   @param rc The render context in question.
 */
void   MEAPI RMenuExecute1Entry(RRender *rc);
/**
    Execute the second action of this entry - e.g. increase the value of a
    value entry.

   @param rc The render context in question.
 */
void   MEAPI RMenuExecute2Entry(RRender *rc);

#ifdef __cplusplus
}
#endif

#endif
