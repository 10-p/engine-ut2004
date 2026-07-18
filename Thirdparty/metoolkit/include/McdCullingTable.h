#ifndef _MCDCULLINGTABLE_H /* -*- mode: C; -*- */
#define _MCDCULLINGTABLE_H

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.2.2.1 $

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
 *
 */

#include <MePrecision.h>
#include <MeMemory.h>
#include <MeInline.h>

typedef struct
{
    MeU32 size;
    MeU32 arraySize;
    MeU32 array[1];
} McdCullingTable;

/**
 * Create a culling table, which represents a symmetric binary relation
 * of the given size as a bit table. Culling tables are for STATIC
 * CULLING ONLY: don't change the table once itis in use.
 */
MeINLINE McdCullingTable *McdCullingTableCreate(int size)
{
    int arraySize = (((size * (size+1))>>1)+31)>>5;

    McdCullingTable *table = (McdCullingTable *)
        MeMemoryAPI.createZeroed((arraySize+2)*sizeof(MeU32));

    if(table == 0)
        return 0;

    table->size = size;
    table->arraySize = arraySize;

    return table;
}

/**
 * Destroy a culling table.
 */
MeINLINE void McdCullingTableDestroy(McdCullingTable *const table)
{
    MeMemoryAPI.destroy(table);
}

/**
 * Set a bit in a culling table
 */
MeINLINE void McdCullingTableSet(McdCullingTable *const table,
    MeU32 a, MeU32 b, MeBool flag)
{
    const MeU32 d = ((b-a)>>31)-1;
    const MeU32 o = ~d;
    const MeU32 l = (d&a)|(o&b);
    MeU32 index;

    MEASSERT(a < table->size && b < table->size);

    b = (o&a)|(d&b);
    a = l;
    index = ((b*(b+1))>>1)+a;

    if (flag)
        table->array[index>>5] |= (1<<(index&31));
    else
        table->array[index>>5] &= ~(1<<(index&31));
}

/**
 * Get the value of a bit in a culling table
 */
MeINLINE MeBool McdCullingTableGet(const McdCullingTable *const table,
    MeU32 a, MeU32 b)
{
    const MeU32 d = ((b-a)>>31)-1;
    const MeU32 o = ~d;
    const MeU32 l = (d&a)|(o&b);
    MeU32 index;

    MEASSERT(a<table->size && b < table->size);

    b = (o&a)|(d&b);
    a = l;
    index = ((b*(b+1))>>1)+a;
    return table->array[index>>5] & (1<<(index&31));
}
#endif
