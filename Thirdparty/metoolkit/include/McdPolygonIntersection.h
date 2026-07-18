#ifndef _McdPolygonIntersection_H
#define _McdPolygonIntersection_H
/* -*- mode: C++; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.1.2.1 $

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

#ifdef __cplusplus
extern "C"
{
#endif

/****************************************************************************
  These are some utility functions.  See .cpp file for description.
*/
int McdPolygonBestAxis(const MeVector3 normal);
void McdPolygonSort(int numpoly, MeVector3 *poly, int axis);

/****************************************************************************
  This computes the intersection of two polygons.
  It uses the sickle algorithm, see O'Rourke 82 at

      http://www.cs.smith.edu/~orourke/books/compgeom.html
      http://citeseer.nj.nec.com/context/201691/0

  The normal and distance determines the projection plane.  All the points
  in poly1 and poly2 are assumed to lie on (or close to) this plane.

  This function messes up poly1 and poly2, they are flattened to
  a coordinate plane and sorted using qsort.

  The caller MUST allocate sufficient space for polyOut.  The maximum
  size required is 2*min(numpoly1, numpoly2).
*/
void McdPolygonIntersection(const MeVector3 normal, MeReal dist,
                            int numpoly1, MeVector3 *poly1, 
                            int numpoly2, MeVector3 *poly2, 
                            int *numOut, MeVector3 *polyOut);

#ifdef __cplusplus
}
#endif

#endif