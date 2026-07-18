#ifndef _MCDINTERACTIONTABLE_H
#define _MCDINTERACTIONTABLE_H
/* -*- mode: C; -*- */

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/04 15:28:53 $ - Revision: $Revision: 1.25.2.1 $

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
    @file
    Mcd interaction table
*/

#include <McdCTypes.h>

#ifdef __cplusplus
extern "C" {
#endif


#define MCD_DECLARE_INTERSECT_INTERACTION(_A_,_B_)                      \
MEPUBLIC                                                                \
MeBool            MEAPI Mcd##_A_##_B_##RegisterInteraction(McdFramework *frame); \
MEPUBLIC                                                                \
MeBool            MEAPI Mcd##_A_##_B_##Intersect(McdModelPair *,        \
                                                 McdIntersectResult *) \


#define MCD_DECLARE_SAFETIME_INTERACTION(_A_,_B_)                       \
MEPUBLIC                                                                \
MeBool            MEAPI Mcd##_A_##_B_##RegisterInteraction(McdFramework *frame);           \
MEPUBLIC                                                                \
MeBool            MEAPI Mcd##_A_##_B_##Intersect(McdModelPair *,        \
                                                 McdIntersectResult *); \
MEPUBLIC                                                                \
int               MEAPI Mcd##_A_##_B_##SafeTime(McdModelPair* p,        \
                                                MeReal maxTime,         \
                                                McdSafeTimeResult *result)


#define MCD_IMPLEMENT_INTERSECT_REGISTRATION(_A_,_B_,_C_)            \
MEPUBLIC                                                             \
MeBool MEAPI Mcd##_A_##_B_##RegisterInteraction(McdFramework *frame) \
{                                                                    \
  McdInteractions interactions;                                      \
  interactions.helloFn = 0;                                          \
  interactions.goodbyeFn = 0;                                        \
  interactions.intersectFn = Mcd##_A_##_B_##Intersect;               \
  interactions.safetimeFn = 0;                                       \
  interactions.cull = _C_;                                           \
  interactions.warned = 0;                                           \
                                                                     \
  McdFrameworkSetInteractions(frame,                                 \
                              kMcdGeometryType ## _A_,               \
                              kMcdGeometryType ## _B_,               \
                              &interactions);                        \
  return 1;                                                          \
}


#define MCD_IMPLEMENT_SAFETIME_REGISTRATION(_A_,_B_,_C_)             \
MEPUBLIC                                                             \
MeBool MEAPI Mcd##_A_##_B_##RegisterInteraction(McdFramework *frame) \
{                                                                    \
  McdInteractions interactions;                                      \
                                                                     \
  interactions.helloFn = 0;                                          \
  interactions.goodbyeFn = 0;                                        \
  interactions.intersectFn = Mcd##_A_##_B_##Intersect;               \
  interactions.safetimeFn =  Mcd##_A_##_B_##SafeTime;                \
  interactions.cull = _C_;                                           \
  interactions.warned = 0;                                           \
                                                                     \
  McdFrameworkSetInteractions(frame,                                 \
                              kMcdGeometryType##_A_,                 \
                              kMcdGeometryType##_B_,                 \
                              &interactions);                        \
  return 1;                                                          \
}

#define MCD_IMPLEMENT_OLD_CONVEX_REGISTRATION(_A_,_B_,_C_)               \
MEPUBLIC                                                             \
MeBool            MEAPI                                              \
Mcd ## _A_ ## _B_ ## RegisterInteraction(McdFramework *frame)        \
{                                                                    \
                                                                     \
  McdInteractions interactions;                                      \
                                                                     \
  interactions.helloFn = McdConvexMeshConvexMeshHello;               \
  interactions.goodbyeFn = McdConvexMeshConvexMeshGoodbye;           \
  interactions.intersectFn = Mcd##_A_##_B_##Intersect;               \
  interactions.safetimeFn = 0;                                       \
  interactions.cull = _C_;                                           \
  interactions.warned = 0;                                           \
                                                                     \
  McdFrameworkSetInteractions(frame,                                 \
                                kMcdGeometryType##_A_,               \
                                kMcdGeometryType##_B_,               \
                                &interactions);                      \
  return 1;                                                          \
}                                                                    \


/****************************************************************************
  This macro sets fwk pointer to call my new GJK code, McdGjkCgIntersect.
  Also I set the hello/goodbye to the new functions for cache management. 
  J Henckel
*/
#define MCD_IMPLEMENT_CONVEX_REGISTRATION(_A_,_B_,_C_)               \
MEPUBLIC                                                             \
MeBool            MEAPI                                              \
Mcd ## _A_ ## _B_ ## RegisterInteraction(McdFramework *frame)        \
{                                                                    \
                                                                     \
  McdInteractions interactions;                                      \
                                                                     \
  interactions.helloFn = McdCacheHello;               \
  interactions.goodbyeFn = McdCacheGoodbye;           \
  interactions.intersectFn = McdGjkCgIntersect;       \
  interactions.safetimeFn = 0;                                       \
  interactions.cull = _C_;                                           \
  interactions.warned = 0;                                           \
                                                                     \
  McdFrameworkSetInteractions(frame,                                 \
                                kMcdGeometryType##_A_,               \
                                kMcdGeometryType##_B_,               \
                                &interactions);                      \
  return 1;                                                          \
}                                                                    \


#define MCD_DECLARE_LINESEG_INTERACTION(_A_)                         \
MEPUBLIC                                                             \
int MEAPI Ix ## _A_ ## LineSegment(const McdModelID model,           \
                                   MeReal* const inOrig,             \
                                   MeReal* const inDest,             \
                                   McdLineSegIntersectResult* info );\
                                                                     \
MEPUBLIC                                                             \
void MEAPI Mcd ## _A_ ## LineSegmentRegisterInteraction(McdFramework *frame)

#define MCD_IMPLEMENT_LINESEG_REGISTRATION(_A_)                      \
MEPUBLIC                                                             \
void MEAPI Mcd ## _A_ ## LineSegmentRegisterInteraction(McdFramework *frame) \
{                                                                    \
  McdFrameworkSetLineSegInteraction(frame,kMcdGeometryType ## _A_,       \
                                Ix ## _A_ ## LineSegment);           \
}          



#ifdef __cplusplus
}
#endif


#endif /* _MCDINTERACTIONTABLE_H */
