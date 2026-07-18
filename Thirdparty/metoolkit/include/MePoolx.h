#ifndef _MePoolx_H
#define _MePoolx_H
/* -*- mode: C++; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:29:06 $ - Revision: $Revision: 1.2.2.1 $

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
  The pool is YET ANOTHER container for managing bits of memory.  
  It is similar to a pool, but the advantage of a pool is that it 
  does not allocate any memory, unless you want it to.  You can allocate
  your own chunk of memory (e.g. with _alloca) and then init a
  pool to manage it for you.  The pool uses an "int" in the
  beginning of each UNUSED record for the free-list, therefore the recsize
  must be at least sizeof(int).

  The performance of MePoolx is O(1) for all operations.

  To use it:

    void foo()
    {
        MePoolx p;
        MeVector3 m[500];

        MePoolxInit(&p, m, sizeof *m, 500);    // initialize pool
        MeVector3* v = (MeVector3*) MePoolxGet(&r);   // allocate one item
        MePoolxPut(&p, v);     // deallocate one item
    }

  Implementation notes:

  The pool uses a lazy memory management scheme.  Basically, when you 
  call init, it is assumed that all the records are free.  The "free" is set
  to the first record and -1 is stored there.  As you get/put records, a
  linked list of free records is maintained, starting at the "free" index
  and the last item in the list is always the top of the "never been used"
  chunk of the memory.  The last item is always marked with a -1.
*/

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
  The MePoolx header struct
*/
typedef struct _MePoolx
{
    int *mem;           /**< pointer to all records */
    int isize;          /**< size of each record / sizeof(int) */
    int numrec;         /**< total number of records */
    int numfree;        /**< number of free records */
    int ifree;          /**< index of first free record */
}
MePoolx;


/****************************************************************************
  This initializes the pool.  You must allocate both p and memory.

  recsize must be a multiple of sizeof(int) and it must be
  at least 2*sizeof(int).  If it isn't, an assertion will fail.
*/
void MePoolxInit(MePoolx *p, void *memory, int recsize, int numrec);

/****************************************************************************
  This gets a record from the memory.  Return 0 if no more available.
*/
void *MePoolxGet(MePoolx *p);
void *MePoolxGetZeroed(MePoolx *p);

/****************************************************************************
  This puts a record back (marks it free).
*/
void MePoolxPut(MePoolx *p, void *rec);

struct MeDict;       /* forward declare */

/****************************************************************************
  This sets the allocator/deallocator in the dictionary to use the poolx
  This is a convenience function to interface MePoolx with MeDict
*/
void MePoolxUseWithDict(MePoolx *p, struct MeDict *d);

#ifdef __cplusplus
}
#endif
 
#endif
