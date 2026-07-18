/* -*- mode: C++; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:07 $ - Revision: $Revision: 1.1.2.1 $

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

/****************************************************************************
  This is a set based on a red/black tree implementation.

  This doesn't allocate any memory.  You must allocate the set header
  and the array of void pointers in whatever way you want.

  The performance of MeSet is O(log n).

  To use it:

    void foo()
    {
        MeSet s;
        MeDictNode m[100];
        char a[] = "hello";
        char b[] = "foobar";

        MeSetInit(&s, m, 100, strcmp);

        MeSetAdd(&s, a);
        MeSetAdd(&s, b);
        n = MeSetSize(&s);   // should return 2

        if (MeSetContains(&s, "hello"))
            printf("hello is in the set");

        //  to iterate over the set.

        MeDictNode *p = MeDictFirst(&s.dict);
        while (p)
        {
            printf(p->key);
            p = MeDictNext(&s.dict, p);
        }
    }
*/

#include <MeDict.h>
#include <MePoolx.h>

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
  This is the Set header structure.
*/
typedef struct _MeSet
{
    MeDict dict;
    MePoolx nodepool;
    MeDictNode *next;   /**< used to traverse the set old-to-new */
    MeDictNode *last;   /**< end of old-to-new list */
}
MeSet;

/****************************************************************************
  This initializes the set.  You must allocate both s and node memory.

  The comparison function is OPTIONAL.  If cmp is zero (null pointer) then
  the record pointers are compared.  This is usually acceptable.
*/
void MeSetInit(MeSet *s, MeDictNode *nodemem, int maxnode, MeDictCompareFn cmp);

/****************************************************************************
  This returns the number of (unique) elements in the set.
*/
int MeSetSize(MeSet *s);

/****************************************************************************
  This return true if set contains item.
*/
int MeSetContains(MeSet *s, void *item);

/****************************************************************************
  This adds item to the set if it is not already in the set.
  Return true if item was added.
*/
int MeSetAdd(MeSet *s, void *item);

/****************************************************************************
  This removes item from set if it is in the set.
  Return true if item was removed.
*/
int MeSetRemove(MeSet *s, void *item);

/****************************************************************************
  This removes the first item from set, the one with the lowest key.
  Return the item, or NULL if set is empty.
*/
void *MeSetPopFirst(MeSet *s);

/****************************************************************************
  This removes any convenient item from set.  The key of this item 
  is likely to be near the median, but there are no guarantees.
  Return the item, or NULL if set is empty.
*/
void *MeSetPop(MeSet *s);

/****************************************************************************
  Returns each item in the set in the order they were added.
  WARNING!  You must not do any Remove or Pop because it will mess up
  the linked list.  The data pointers are used to superimpose a linked
  list on the dictionary from oldest to newest.
  There is no way to reset the iterator, unless you keep a pointer to 
  the oldest node and set s->next to it.
*/
void *MeSetIteratorNext(MeSet *s);

/****************************************************************************
  This returns true if set is full.  Note, if the set dictionary does not
  use a fixed size pool, this will never return true.
*/
int MeSetIsFull(MeSet *s);

/****************************************************************************
  This will make an unlimited size set which will use MeMemoryAPI to 
  malloc nodes.  For performance, MeSetInit is better than MeSetCreate.
*/
MeSet *MeSetCreate(MeDictCompareFn cmp);

/****************************************************************************
  This destroys a set created by MeSetCreate only.
*/
void MeSetDestroy(MeSet *s);

#ifdef __cplusplus
}
#endif

