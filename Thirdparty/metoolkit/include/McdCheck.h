/* -*- mode: C++; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:54 $ - Revision: $Revision: 1.12.2.1 $

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
#ifndef _MCDCHECK_H
#define _MCDCHECK_H

#define MCD_ASSERT(stuff, fn) MCD_CHECK_ASSERT_(stuff, fn)

#ifdef MCDCHECK

#include <McdMessage.h>

/**  general purpose assertion */
#define MCD_CHECK_ASSERT_(EX, fn)\
((EX)?((void)0):McdCoreError(kMcdErrorNum_AssertionFailed, # EX, fn, __FILE__, __LINE__))

/** Assertion with error message */
#define MCD_CHECK_ASSERT(inExpression,errorNum, errorMsg, fn)\
{ if (!(inExpression))\
McdCoreError(errorNum, errorMsg, fn, __FILE__, __LINE__); }

/** Assertion with error message, no error number */
#define MCDCHECK_GENERICFATAL(inExpression, errorMsg, fn)\
{ if (!(inExpression))\
McdCoreError(kMcdErrorNum_GenericFatal, errorMsg, fn, __FILE__, __LINE__); }

/** Assertion with error message, no error number */
#define MCDCHECK_GENERICWARNING(inExpression, errorMsg, fn)\
{ if (!(inExpression))\
McdCoreError(kMcdErrorNum_GenericWarning, errorMsg, fn, __FILE__, __LINE__); }

/**  general purpose allocation-success checking */
#define MCDCHECK_ALLOC(ptr, typeName, fn)\
 {if ( (ptr) == 0) McdCoreError(kMcdErrorNum_AllocFailure, typeName, fn, __FILE__, __LINE__);}

/**  general purpose null-pointer checking   */
#define MCD_CHECK_NULLPTR(ptr, typeName, fn)\
 {if ( (ptr) == 0) McdCoreError(kMcdErrorNum_InvalidPtr, typeName, fn, __FILE__, __LINE__);}

/** Checks the validity of a collision model pointer, in debug mode only */
#define MCD_CHECKMODEL(x, fn) {if ((x) == 0) McdCoreError(kMcdErrorNum_InvalidPtr_CM, "", fn, __FILE__, __LINE__);}

/** Checks the validity of a collision geometry instance pointer, in debug mode only */
#define MCD_CHECKGEOMETRYINSTANCE(x, fn) {if ((x) == 0) McdCoreError(kMcdErrorNum_InvalidPtr_CM, "", fn, __FILE__, __LINE__);}

/** Checks the validity of a collision space pointer, in debug mode only */
#define MCD_CHECKSPACE(x, fn) {if ((x) == 0) McdCoreError(kMcdErrorNum_InvalidPtr_Space, "", fn, __FILE__, __LINE__);}

/** Checks the existance of a valid McdSpace */
#define MCD_CHECKSPACEBUILT(x, fn) {if ((x) == 0) McdCoreError(kMcdErrorNum_InvalidFuncOrder_Space, "", fn, __FILE__, __LINE__);}

/** Checks the validity of a collision geometry  pointer, in debug mode only */
#define MCD_CHECKGEOMETRY(x, fn) {if ((x) == 0) McdCoreError(kMcdErrorNum_InvalidPtr_Geometry, "", fn, __FILE__, __LINE__);}

/** Checks the validity of a geometry (i.e. is it registered?), in debug mode only */
#define MCDGEOMETRY_CHECK_VALID(g, fn)\
if( !McdGeometryIsValid(g) )\
  McdCoreError(kMcdErrorNum_GenericFatal, "Attempt to use an McdGeometry function call with an invalid McdGeometry object. ( Use of unregistered geometry type?)", fn, __FILE__, __LINE__)

/** Checks the validity of a collision space pair pointer, in debug mode only */
#define MCD_CHECKMODELPAIR(x, fn) \
{if ((x) == 0) McdCoreError(kMcdErrorNum_InvalidPtr_SP, "",fn, __FILE__,__LINE__); \
if (x->model1 == 0 || x->model2 == 0 ) McdCoreError(kMcdErrorNum_InvalidPair, "", fn, __FILE__, __LINE__);}

#define MCD_CHECKINTERSECTRESULT(r, fn) \
{if ((r) == 0) McdCoreError(kMcdErrorNum_InvalidPtr_IR,"", fn, __FILE__,__LINE__); \
if (r->contacts == 0) McdCoreError(kMcdErrorNum_InvalidIR,"", fn, __FILE__, __LINE__); }

/** Checks the validity of a collision space pair pointer, in debug mode only */
#define MCD_CHECKMODELPAIRID(x, fn) {if ((x) == 0) McdCoreError(kMcdErrorNum_InvalidPtr_SPID, "", fn, __FILE__, __LINE__);}

/** Checks the validity of a McdRwWorldID, in debug mode only */
#define MCD_CHECKRWWORLD(x, fn) {if ((x) == 0) McdCoreError(kMcdErrorNum_InvalidPtr_RWW, "", fn, __FILE__, __LINE__);}

/** Checks the validity of a McdRwBSPID, in debug mode only */
#define MCD_CHECKRWBSP(x, fn) {if ((x) == 0) McdCoreError(kMcdErrorNum_InvalidPtr_RWBSP, "", fn, __FILE__, __LINE__);}

/** Checks the validity of a transform, in debug mode only */
#define MCD_CHECKTRANSFORM(x, fn) {if ((x) == 0) McdCoreError(kMcdErrorNum_InvalidPtr_TM, "", fn, __FILE__, __LINE__);}

#define MCD_INTERACTIONTABLE_CHECKBOUNDS(f,i,j,fn) \
    if ((i) < 0 || (i) > (f)->geometryRegisteredCountMax \
	|| (j) < 0 || (j) > (f)->geometryRegisteredCountMax) \
	McdCoreError(McdErrorNum_RegisterInteractionWithInvalidGeometryType, \
	    "",(fn), __FILE__,__LINE__);

#define MCD_INTERACTIONARRAY_CHECKBOUNDS(f,i,fn) \
    if ((i) < 0 || (i) >= f->geometryRegisteredCountMax) \
	McdCoreError(McdErrorNum_RegisterInteractionWithInvalidGeometryType, \
	    "",(fn), __FILE__, __LINE__);

#else

#define MCDCHECK_GENERICFATAL(inExpression, errorMsg, fn)

#define MCDCHECK_GENERICWARNING(inExpression, errorMsg, fn)

#define MCD_CHECK_ASSERT_(inExpression, fn) ((void)0)

#define MCD_CHECK_ASSERT(inExpression,errorNum, errorMsg, fn)

#define MCDCHECK_ALLOC(ptr, typeName, fn)

#define MCD_CHECK_NULLPTR(ptr, typeName, fn)

#define MCD_CHECKMODEL(x, fn)

#define MCD_CHECKGEOMETRYINSTANCE(x, fn)

#define MCD_CHECKSPACE(x, fn)

#define MCD_CHECKSPACEBUILT(x, fn)

#define MCD_CHECKGEOMETRY(x, fn)

#define MCD_CHECKMODELPAIR(x, fn)

#define MCD_CHECKBRIDGEBUILT(x, fn)

#define MCD_CHECKMODELPAIRID(x, fn)

#define MCD_CHECKCDDTBRIDGE(x, fn)

#define MCD_CHECKRWWORLD(x, fn)

#define MCD_CHECKRWBSP(x, fn)

#define MCD_CHECKTRANSFORM(x, fn)

#define MCD_CHECKINTERSECTRESULT(x, fn)

#define MCDGEOMETRY_CHECK_VALID(g, fn)

#define MCD_INTERACTIONTABLE_CHECKBOUNDS(t,i,j, fn)

#define MCD_INTERACTIONARRAY_CHECKBOUNDS(t,i, fn)

#endif /* MCDCHECK */


#endif /* _MCDCHECK_H */


