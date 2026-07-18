#ifndef _MEMISC_H
#define _MEMISC_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:06 $ - Revision: $Revision: 1.8.6.1 $

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
 *  Miscellaneous utility functions.
 */

#ifdef __cplusplus
extern "C"
{
#endif

void            MEAPI MeHSVtoRGB(float h, float s, float v, float rgb[3]);

/* circlur list macros */

#define LIST_INIT_SENTINEL(sentinel) \
    MEASSERT((sentinel)); \
    (sentinel)->next = (sentinel)->prev = (sentinel); \
    (sentinel)->current = 0;

#define LIST_INSERT(node,sentinel) \
    MEASSERT((node)); \
    MEASSERT((sentinel)); \
    (node)->next = (sentinel)->next; \
    (sentinel)->next->prev = (node); \
    (sentinel)->next = (node); \
    (node)->prev = (sentinel);

#define LIST_REMOVE(node,sentinel,item) \
    MEASSERT((sentinel)); \
    MEASSERT((item)); \
    (node) = (sentinel)->prev; \
    while ((node) != (sentinel) && ((node)->current != (item))) \
        (node) = (node)->prev; \
    (node)->prev->next = (node)->next; \
    (node)->next->prev = (node)->prev;

#define LIST_REMOVE_THIS(node,sentinel) \
    MEASSERT((sentinel)); \
    (node) = (sentinel)->prev; \
    (node)->prev->next = (node)->next; \
    (node)->next->prev = (node)->prev;

#define LIST_IS_EMPTY(sentinel) \
    ((sentinel)->next == (sentinel) && \
     (sentinel)->prev == (sentinel))

#ifdef __cplusplus
}
#endif

#endif /* _MEMISC_H */

