#ifndef _MeHeap_H
#define _MeHeap_H
/* -*- mode: C++; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:06 $ - Revision: $Revision: 1.2.2.2 $

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
  Priority queue based on a heap implementation.

  This doesn't allocate any memory.  You must allocate the heap header
  and the array of void pointers in whatever way you want.

  The performance of MeHeap is O(log n) for push/pop.

  To use it:

    //  make a record.
    typedef struct { int key; char blah[80]; } rec;

    //  record comparison function
    int cmp(const void *a, const void *b) 
    {
        return ((rec*)a)->key < ((rec*)b)->key;
    }

    void foo()
    {
        MeHeap h;
        void *m[500];

        MeHeapInit(&h, m, 500, cmp);    // initialize heap

        // put two items into the heap
        rec r1 = { 1, "hello" };
        rec r2 = { 2, "foobar" };
        MeHeapPush(&h, &r1); 
        MeHeapPush(&h, &r2);       

        rec *p = (rec*) MeHeapPop(&h);   // get the lowest record
    }

  In this sample, since the key is an int and the first thing in the
  struct, we could have just passed 0 for the comparison, instead of
  bothering to write the cmp function.
*/

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
  This is the comparison function type used by the heap.
  Return 1 if you want elem1 to pop before elem2, else return 0.
  If you want your heap to keep lowest on top, return item1.key < item2.key.
*/
typedef int (*MeHeapComparisonFnPtr)(const void *elem1, const void *elem2);

/****************************************************************************
  This is the heap header structure.
*/
typedef struct _MeHeap
{
    void **mem;         /**< array of void pointers to records */
    int capacity;       /**< total number of pointers in mem */
    int used;           /**< number of used pointers in mem */
    MeHeapComparisonFnPtr cmp; /**< comparison function */
}
MeHeap;


/****************************************************************************
  This initializes the heap.  You must allocate both h and memory.
  Note, you should set the capacity to one more than you really need,
  because the zero-th mem is not used.

  The comparison function is OPTIONAL.  If cmp is zero (null pointer) then
  the records are cast to integers and compared.
*/
void MeHeapInit(MeHeap *h, void **memory, int capacity, MeHeapComparisonFnPtr cmp);

/****************************************************************************
  Push an item into the heap.
  Return 1 if success, 0 if heap is full.
*/
int MeHeapPush(MeHeap *h, void *item);

/****************************************************************************
  This removes the best (lowest) item from the heap and return it.
  Return NULL if the heap is empty.
*/
void* MeHeapPop(MeHeap *h);

/****************************************************************************
  This is a convenience function to malloc memory and then call MeHeapInit.
*/
MeHeap *MeHeapCreate(int capacity, MeHeapComparisonFnPtr cmp);

/****************************************************************************
  This is a convenience function to destroy a heap created by MeHeapCreate.
*/
void MeHeapDestroy(MeHeap *h);


#ifdef __cplusplus
}
#endif

#endif
