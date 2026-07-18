#ifndef _MEVERSION_H
#define _MEVERSION_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/24 17:31:09 $ - Revision: $Revision: 1.19.6.6 $

   This software and its accompanying manuals have been developed
   by MathEngine UK PLC ("MathEngine") and the copyright and all other
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
 * MathEngine Toolkit central version information
 */

/*
   Product and Version information, in "pieces"
 */

#define ME_PRODUCT_NAME                "MathEngine Karma"
#define ME_PRODUCTVERSION_MAJOR        1
#define ME_PRODUCTVERSION_MINOR        2
#define ME_PRODUCTVERSION_REV          1
#define ME_PRODUCTVERSION_QUALIFIER
#ifdef BUILD_VER_STRING
#define ME_PRODUCTVERSION_BUILDNUMBER  BUILD_VER_STRING
#else
#define ME_PRODUCTVERSION_BUILDNUMBER  631
#endif


/*
   Standardized copyright text for display in samples, examples, and tutorials
 */

#define ME_COPYRIGHT_LINE1   "(c) 2002 MathEngine PLC. All rights reserved."
#define ME_COPYRIGHT_LINE2   ""
#define ME_COPYRIGHT_LINE3   ""

/*
   Rules to construct the version string(s) from the version information
 */

#define STRINGIZE(value)                          #value
#define CONSTRUCT_VER2(maj,min)                   STRINGIZE(maj.min)
#define CONSTRUCT_VER2QUALIFIED(maj,min,qual)     STRINGIZE(maj.min qual)

#define CONSTRUCT_VER3(maj,min,rev)               STRINGIZE(maj.min.rev)
#define CONSTRUCT_VER3QUALIFIED(maj,min,rev,qual) STRINGIZE(maj.min.rev qual)


#define ME_VERSION_STRING     \
  CONSTRUCT_VER3QUALIFIED(ME_PRODUCTVERSION_MAJOR, \
                          ME_PRODUCTVERSION_MINOR, \
                          ME_PRODUCTVERSION_REV,   \
                          ME_PRODUCTVERSION_QUALIFIER)

#define ME_BUILD_VERSION_STRING     \
  CONSTRUCT_VER3QUALIFIED(ME_PRODUCTVERSION_MAJOR, \
                          ME_PRODUCTVERSION_MINOR, \
                          ME_PRODUCTVERSION_REV,   \
                          ME_PRODUCTVERSION_QUALIFIER(build  \
                          ME_PRODUCTVERSION_BUILDNUMBER))



#ifdef __cplusplus
extern "C" {
#endif

const char *MeToolkitVersionString(void);
#ifdef __cplusplus
}
#endif

#endif /* _MEVERSION_H */
