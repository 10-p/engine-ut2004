#ifndef _MCDGJK_H
#define _MCDGJK_H
/* -*- mode: C++; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.4.2.5 $

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
#include <MeMath.h>
#include <MeHeap.h>
#include <MePoolx.h>
#include <McdGeometryInstance.h>

//  Set this symbol to TRUE for extra debugging code
#define MCD_GJK_DEBUG 0

/*  John Henckel's extra special assert macro. */
#if 0
#undef MEASSERT
#define MEASSERT(x) \
  do { if (!(x)) printf("Assert failed (%s) in %s:%d\n",#x, __FILE__, __LINE__); } while(0)
#endif


#if MCD_GJK_DEBUG
#define MeInfo2 MeInfo
#else
#define MeInfo2 ???  // force syntax error
#endif

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
  Maximum number of GJK and penetration depth iterations.
*/
#define MCDGJK_IMAX  50
#define MCDGJK_PDMAX 50

/****************************************************************************
  This is the size of the buffer used to compute contacts.  It should be 
  2x larger than max contacts, e.g. if you want to get 50 contacts, this
  should be 100.
*/
#define MCDCONTACTGEN_VSIZE 100

/****************************************************************************
  This is 16 zero-terminated lists (i = 0..15) and each list contains
  the list of binary numbers that are subsets of i.  The subsets are sorted 
  by number of bits, large to small.  The first item in each list is i.
*/
extern const int *McdGjkBinarySubset[16];

/****************************************************************************
  This is a handy little macro that iterates over each bit in a word 
  that is '1'.  For instance if bb='1011' then the loop will go
  three times with ix=0,1,3 and bx=1,2,8.
  NOTE:  this is slightly inefficient because about 1/3 of the loop 
  iterations do not pass the 'if' test.  Perhaps a table driven loop would
  be faster.
*/
#define FOR_EACH_BIT(ix,bx,bb) \
    for (ix = 0, bx = 1; bx <= (bb); bx <<= 1, ++ix) if (bx & (bb))

/****************************************************************************
  These are some handy tests for the number of one bits in a nibble.
*/
#define ONE_BIT(b) (b==1 || b==2 || b==4 || b==8)
#define TWO_BIT(b) (b==3 || b==5 || b==6 || b==9 || b==10 || b==12)
#define THREE_BIT(b) (b==7 || b==11 || b==13 || b==14)

/****************************************************************************
  This is used to store collision data for a time-step coherence cache.
  The McdModelPair.m_cachedData should point to this.
*/
typedef struct _McdCache
{
    MeVector3 normal;       /**< witness plane normal */
    MeVector3 location;     /**< nearest point to both bodies */
    MeVector3 offset;       /**< offset the CSO to get better simplex */
    MeReal fat1, fat2;      /**< the fatness (radius) of each model */
    MeReal padding;         /**< sum mPadding of both models */
    MeReal separation;      /**< distance of separation (negative = overlap) */
    McdGeometryInstanceID ins1, ins2;  /**< pointers to the two geometry inst. */
}
McdCache;

/****************************************************************************
  This stores a point in Minkowski space.  It is used for GJK type algorithms.
  We don't store s1, since s1 = w+s2.
*/
typedef struct _McdGjkPoint
{
    MeVector3 w;        /**< the difference s1-s2 */
    MeVector3 s2;       /**< the support vector of model 2 in WRF */
}
McdGjkPoint;

/****************************************************************************
  This stores the coherence data for GJK to improve performance.
  The McdModelPair.m_cachedData should point to this.
  
  Note, in some books the delta is called an "augmented determinant", i.e. the
  determinant of the matrix in which one column has been replaced by "b" row
  vector.  In our case the "b" vector is always (1,0,0,0).  You can learn all
  about GJK by reading the papers by Gino van den Bergen, and the book by
  Murilo Coutinho is also very good.
*/
typedef struct _McdGjkSimplex
{
    McdGjkPoint point[4];   /**< vertices of the CSO simplex in WRF */
    MeVector3 lastw[2];     /**< last few points removed from the simplex */
    MeReal dot[4][4];       /**< dot products of the vertices */
    MeReal delta[16][4];    /**< aug determinants used in Cramer's rule */
    MeReal eps;             /**< epsilon based on scale */
    int bits;               /**< a bit mask 0..15 */
    int next_i;             /**< the index 0..3 of the next new vector */
    int next_bit;           /**< bit of the next new vector 1,2,4, or 8 */
    int inflate;            /**< true when doing penetration depth inflation */

#if MCD_GJK_DEBUG
    int debug;              /**< true when doing single step debugging */
    void *q;                /**< only for single step debugging */
#endif
}
McdGjkSimplex;

/****************************************************************************
  This is one face of the CSO volume.  The fi has space for 4 ints, but it
  only has three, and which three depends on the pattern in bits.
*/

typedef struct _McdGjkFace
{
    MeVector3 v;        /**< unit vector to nearest point on face */
    MeReal v_len;       /**< distance to nearest point on face */
    int depth;          /**< equals depth of parent plus one */
    int bits;           /**< the simplex bits of the face */
    int fi[4];          /**< indices of the three face points */
    int slant;          /**< bits omitted from calculation of v */
}
McdGjkFace;

/****************************************************************************
  This is a priority queue of faces on the CSO volume
*/
typedef struct _McdGjkFaceQueue
{
    McdGjkSimplex *s;   /**< simplex data */
    MeHeap face;
    MePoolx fpool;
    McdGjkFace *lastpop;
    int nump, maxp;     /**< number / max number of points */
    McdGjkPoint *point; /**< list of points in CSO, with supports */
    int si[4];          /**< indices of the four simplex points */
}
McdGjkFaceQueue;

/****************************************************************************
  Main GJK intersect function
*/
int MEAPI McdGjkCgIntersect(McdModelPair* p, McdIntersectResult *result);
MeBool MEAPI McdCacheHello(McdModelPair*p);
void MEAPI McdCacheGoodbye(McdModelPair*p);

/****************************************************************************
  Penetration depth computation
*/
int McdGjkPenetrationDepth(McdCache *c, McdGjkSimplex *s);

/****************************************************************************
  Contact generation
*/
void McdContactGen(McdCache *c, McdIntersectResult *result);

/****************************************************************************
  Simplex utility functions - shared by GJK and the inflation code.
*/
MeReal McdGjkComputeSupport(McdGjkPoint *p, const MeVector3 v, 
                            int sign, McdCache *c);

MeReal McdGjkFindNextSupportPoint(MeVector3 v, MeReal v_len, 
                                  McdCache *c, McdGjkSimplex *s, int i);

void McdGjkAdjustPerpendicular(MeVector3 v, MeReal v_len,  McdGjkSimplex *s);

McdGjkPoint *McdGjkNextAvailablePoint(McdGjkSimplex *s);

int McdGjkNextIsDuplicate(McdGjkSimplex *s);

int McdGjkComputeVector(MeVector3 v, int bits, int sup, McdGjkSimplex *s);

void McdGjkUpdateDotCache(McdGjkSimplex *s);

void McdGjkUpdateDeltaCache(McdGjkSimplex *s);

MeReal McdGjkCrossProd(MeVector3 v, McdGjkSimplex *s);

#if MCD_GJK_DEBUG
/****************************************************************************
  This is for debugging only
*/
void write_binfile(char*fn, McdCache *c);  
#endif

#ifdef __cplusplus
}
#endif
#endif
