#ifndef __MEXMLTREE_H
#define __MEXMLTREE_H

/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:40 $ - Revision: $Revision: 1.12.2.4 $

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
  * @file XML tree functions.
  */

#include <MePrecision.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct Attribute            Attribute;
typedef struct AttributeNode        AttributeNode;
typedef struct PElement             PElement;   
typedef struct PElementNode         PElementNode;
typedef struct PElementIt           PElementIt;

/* traversal callbacks */
typedef void (MEAPI* PElementCB)(PElement *elem, PElement *parent, void *userdata);
typedef PElement * (MEAPI* PElementCompareCB)(PElement *e, void *k1, void *k2);
typedef void (MEAPI* CDataFreeFunc)(void *const mem);

struct Attribute
{
    char *attr;
    char *value;
};

struct AttributeNode
{
    Attribute       *current;
    AttributeNode   *next;
};

struct PElement
{
    int             type;
#ifdef _MECHECK
    char            *name;
#endif
    PElementNode    *childHead;
    void            *cdata;
    CDataFreeFunc   freeFunc;
    AttributeNode   *attrHead;
};

struct PElementNode
{
    PElement        *current;
    PElementNode    *next;
};

struct PElementIt
{
    PElementNode    *stackHead;
};

MEPUBLIC
PElement        *MEAPI PElementCreate(int type, char *name, void *data, CDataFreeFunc freeFunc, char *attrs);
MEPUBLIC
void             MEAPI PElementDestroy(PElement *e);
MEPUBLIC
void             MEAPI PElementDestroyChildren(PElement *e, PElement *parent, void *userdata);
MEPUBLIC
void             MEAPI PElementAddAttribute(PElement *e, char *attr, char *val);
MEPUBLIC
void             MEAPI PElementParseAttributes(PElement *e, char *attrs);
MEPUBLIC
void             MEAPI PElementInsert(PElement *e, PElement *parent);
MEPUBLIC
PElement        *MEAPI PElementFind(PElement *root, PElementCompareCB cb, void *k1, void *k2);
MEPUBLIC
char            *MEAPI PElementGetAttributeValue(PElement *elem, char *attr);
MEPUBLIC
PElement        *MEAPI PElementCompareAttributes(PElement *e, void *attr, void *attrVal);
MEPUBLIC
void             MEAPI PElementTraverseAll(PElement *root, PElementCB cb, 
                                         MeBool rootFirst, void *userdata);
MEPUBLIC
void             MEAPI PElementInitIterator(PElement *root, PElementIt *it);
MEPUBLIC
PElement        *MEAPI PElementGetNext(PElementIt *it);
MEPUBLIC
void             MEAPI PElementIteratorDestroy(PElementIt *it);
MEPUBLIC
PElement        *MEAPI PElementLookup(PElement *root, char *attr, char *val);

#ifdef __cplusplus
}
#endif


#endif
