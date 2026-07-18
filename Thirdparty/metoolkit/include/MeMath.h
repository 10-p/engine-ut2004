#ifndef _MEMATH_H /* -*- mode: C; -*- */
#define _MEMATH_H

/*
   Copyright (c) 1997-2002 MathEngine PLC

   $Name: t-stevet-RWSpre-030110 $

   Date: $Date: 2002/04/09 18:37:31 $ - Revision: $Revision: 1.111.2.11 $

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
 * Maths helper functions.
 *
 * These are mostly provided as a convenience to users and writers of demos. Consideration has been given
 * to clarity and generality of implementation. Therefore the implementation of some functions may not
 * be appropriate for performance critical code.
 *
 * Math library conventions:
 * \li Matrices are stored row-major; that is, the memory layout is a[0][0], a[0][1],... a[1][0], a[1][1],... etc
 * \li Transformation matrices are stored with translation component in the bottom row, that is, a[3][0]...a[3][2]
 * \li Transformation and Rotation matrices are applied by <b>post-multiplication</b>, that is, v'=vM, where M
 * is a transformation or rotation matrix. This implies that transforming by the matrix A=B*C corresponds
 * to transforming by B, then transforming by C.
 * \li Quaternions are stored [w,x,y,z], where w is the real part. This may change to [x,y,z,w] in a future release.
 * in order to support accelerated quaternion operations on PS/2
 */

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <MeInline.h>
#include <MePrecision.h>
#include <MeAssert.h>
#include <MeMessage.h>

/** least integer <= n divisible by 4 */
#define MeMathFLOOR4(n)         ((n) - (n)%4)
/** least integer >= n divisible by 4 */
#define MeMathCEIL4(n)          (((n)%4) == 0 ? (n) : (n) - (n)%4 + 4)
/** least integer >= n divisible by 12 */
#define MeMathCEIL12(n)         (((n)%12) == 0 ? (n) : (n) - (n)%12 + 12)
/** least integer >= n divisible by 16 */
#define MeMathCEIL16(n)         (((n)%16) == 0 ? (n) : (n) - (n)%16 + 16)
/** least integer >= n divisible by 64 */
#define MeMathCEIL64(n)         (((n)%64) == 0 ? (n) : (n) - (n)%64 + 64)
/** least integer >= n divisible by c */
#define MeMathCEILN(c,n)        (((n)%(c)) == 0 ? (n) : (n) - (n)%(c) + (c))

/** max of two numbers */
#define MeMAX(a,b)              (((a)<(b)) ? b : a)

/** min of two numbers */
#define MeMIN(a,b)              (((a)<(b)) ? a : b)

/** Min of three numbers */
#define MeMIN3(a,b,c)           (MeMIN(MeMIN(a,b),c))

/** max of three numbers */
#define MeMAX3(a,b,c)           (MeMAX(MeMAX(a,b),c))

/** Min of three numbers */
#define MeMIN3(a,b,c)           (MeMIN(MeMIN(a,b),c))

/** return a clamped to the range [b,c] */
#define MeCLAMP(a,b,c)          (MeMAX(MeMIN(a,c),b))


#ifndef ME_PI
#   define ME_PI                ((MeReal) 3.14159265358979323846)
#endif

#ifdef __cplusplus
extern    "C" {
#endif


/***** MEREAL *****/
#if (!MeDEFINE)
MEPUBLIC
    void MEAPI MeRealSwap(MeReal *const a, MeReal *const b);
MEPUBLIC
    MeReal MEAPI MeRealRandomInRange(MeReal start, MeReal end);
MEPUBLIC
	MeReal MEAPI MeSafeRecip(MeReal x);
#endif

/***** MEVECTOR3 *****/
#if (!MeDEFINE)
MEPUBLIC
    void MEAPI MeVectorSetZero(MeReal *const A, const int n);
MEPUBLIC
    void MEAPI MeVector3Copy(MeVector3 c, const MeVector3 b);
MEPUBLIC
    void MEAPI MeVector3Set(MeVector3 c, const MeReal x, const MeReal y, const MeReal z);
MEPUBLIC
    void MEAPI MeVector3Subtract(MeVector3 a, const MeVector3 b, const MeVector3 c);
MEPUBLIC
    void MEAPI MeVector3Add(MeVector3 a, const MeVector3 b, const MeVector3 c);
MEPUBLIC
    MeReal MEAPI MeVector3Dot(const MeVector3 b, const MeVector3 c);
MEPUBLIC
    void MEAPI MeVector3Cross(MeVector3 a, const MeVector3 b, const MeVector3 c);
MEPUBLIC
    void MEAPI MeVector3OuterProduct(MeMatrix3 a, const MeVector3 b, const MeVector3 c);
MEPUBLIC
    MeReal MEAPI MeVector3MagnitudeSqr(const MeVector3 v);
MEPUBLIC
    MeReal MEAPI MeVector3Magnitude(const MeVector3 v);
MEPUBLIC
    MeReal MEAPI MeVector3DistanceSqr(const MeVector3 a, const MeVector3 b);
MEPUBLIC
    MeReal MEAPI MeVector3Distance(const MeVector3 a, const MeVector3 b);
MEPUBLIC
    void MEAPI MeVector3Scale(MeVector3 v, const MeReal a);
MEPUBLIC
    void MEAPI MeVector3MultiplyAdd(MeVector3 v, const MeReal a, const MeVector3 v1);
MEPUBLIC
    void MEAPI MeVector3MultiplySubtract(MeVector3 v, const MeReal a, const MeVector3 v1);
MEPUBLIC
    void MEAPI MeVector3MultiplyScalar(MeVector3 a, const MeVector3 b, const MeReal c);
MEPUBLIC
    void MEAPI MeVector3ScaleAndAdd(MeVector3 a, const MeVector3 b, const MeReal c, const MeVector3 d);
MEPUBLIC
    MeReal MEAPI MeVector3Normalize(MeVector3 v);
MEPUBLIC
    void MEAPI MeVector3MakeOrthogonal(MeVector3 v, const MeVector3 v1);
MEPUBLIC
    void MEAPI MeVector3PlaneSpace(const MeVector3 n, MeVector3 a, MeVector3 b);
MEPUBLIC
    void MEAPI MeVector3MultiplyElements(MeVector3 a, const MeVector3 b, const MeVector3 c);
MEPUBLIC
    void MeVector3Clamp(MeVector3 a, const MeReal min, const MeReal max);
MEPUBLIC
    void MeVector3Min(MeVector3 out, const MeVector3 in1, const MeVector3 in2);
MEPUBLIC
    void MeVector3Max(MeVector3 out, const MeVector3 in1, const MeVector3 in2);
MEPUBLIC
    void MeVector3Lerp(MeVector3 out, const MeVector3 from, const MeVector3 to, const MeReal howFar);

MEPUBLIC
    MeReal MeVector3ScalarTripleProduct(const MeVector3 a,const MeVector3 b,const MeVector3 c);
#endif
MEPUBLIC
    MeReal MEAPI MeVector3AreaOfTriangle(const MeVector3 v1, const MeVector3 v2);
MEPUBLIC
    void MEAPI MeVector3Swap(MeVector3 a, MeVector3 b);

/***** MEVECTOR4 *****/
#if (!MeDEFINE)
MEPUBLIC
    void MEAPI MeVector4Copy(MeVector4 c, const MeVector4 b);
MEPUBLIC
    void MEAPI MeVector4Subtract(MeVector4 a, const MeVector4 b, const MeVector4 c);

MEPUBLIC
    void MEAPI MeVector4MultiplyElements(MeVector4 a, const MeVector4 b, const MeVector4 c);
MEPUBLIC
    void MeVector4Clamp(MeVector4 a, const MeReal min, const MeReal max);
MEPUBLIC
    void MEAPI MeVector4Scale(MeVector3 v, const MeReal a);
MEPUBLIC
    void MEAPI MeVector4Add(MeVector4 a, const MeVector4 b, const MeVector4 c);

MEPUBLIC
    MeReal MEAPI MeVector4MagnitudeSqr(const MeVector4 v);
MEPUBLIC
    MeReal MEAPI MeVector4Dot(const MeVector4 b, const MeVector4 c);
#endif

/***** MEQUATERNION *****/
#if (!MeDEFINE)
MEPUBLIC
    void MEAPI MeQuaternionMake(MeVector4 q, const MeVector3 x, const MeReal a);
MEPUBLIC
    void MEAPI MeQuaternionToR(MeMatrix3 R, const MeVector4 q);
MEPUBLIC
    void MEAPI MeQuaternionToTM(MeMatrix4 tm, const MeVector4 q);
MEPUBLIC
    void MEAPI MeQuaternionToD(MeReal D[12], const MeVector4 q);
MEPUBLIC
    void MEAPI MeQuaternionWtoDQ(MeVector4 dq, const MeVector4 q, const MeVector3 w);
MEPUBLIC
    void MEAPI MeQuaternionProduct(MeVector4 r, const MeVector4 p, const MeVector4 q);
MEPUBLIC
    void MEAPI MeQuaternionRotateVector3(MeVector3 vout, const MeVector4 q, const MeVector3 v);
MEPUBLIC
    void MEAPI MeQuaternionSet(MeVector4 q, MeReal w, MeReal x, MeReal y, MeReal z);
#endif
MEPUBLIC
    void MEAPI MeQuaternionFromTM(MeVector4 q, const MeMatrix4 tm);
MEPUBLIC
    void MEAPI MeQuaternionFiniteRotation(MeVector4 q, const MeVector3 w, const MeReal h);
MEPUBLIC
    void MEAPI MeQuaternionForRotation(MeVector4 q, const MeVector3 v1, const MeVector3 v2);
MEPUBLIC
    void MEAPI MeQuaternionSlerp(MeVector4 q, const MeVector4 from, const MeVector4 to, const MeReal howFar);

/***** MEMATRIX *****/
#if (!MeDEFINE)
MEPUBLIC
    void MEAPI MeMatrixTranspose(MeReal *A, const int n, const int m, const MeReal *const B);
MEPUBLIC
    void MEAPI MeMatrixMultiply(MeReal *A, const int p, const int q, const int r,
                const MeReal *const B, const MeReal *const C);
MEPUBLIC
    void MEAPI MeMatrixMultiplyT1(MeReal *A, const int p, const int q, const int r,
                  const MeReal *const B, const MeReal *const C);
#endif
MEPUBLIC
    void MEAPI MeMatrixFPrint(FILE * const file, const MeReal *const A,
                  const int n, const int m, const char *const format);
MEPUBLIC
    void MEAPI MeMatrixPrint(const MeReal *const A,
                 const int n, const int m, const char *const format);

/***** MEMATRIX3 *****/
#if (!MeDEFINE)
MEPUBLIC
    MeBool MEAPI MeMatrix3IsIsotropic(const MeMatrix3 i);
MEPUBLIC
    void MEAPI MeMatrix3Copy(MeMatrix3 A, const MeMatrix3 B);
MEPUBLIC
    void MEAPI MeMatrix3CopyVec(MeMatrix3 A,
                const MeVector3 B1, const MeVector3 B2, const MeVector3 B3);
MEPUBLIC
    void MEAPI MeMatrix3Transpose(MeMatrix3 A);
MEPUBLIC
    void MEAPI MeMatrix3CrossFromVector(const MeVector3 a, const MeReal factor, MeVector3 A0,
                    MeVector3 A1, MeVector3 A2);
MEPUBLIC
    void MEAPI MeMatrix3MultiplyVector(MeVector3 A, const MeMatrix3 B, const MeVector3 C);
MEPUBLIC
    void MEAPI MeMatrix3Add(MeMatrix3 A, const MeMatrix3 B, const MeMatrix3 C);
MEPUBLIC
    void MEAPI MeMatrix3Subtract(MeMatrix3 A, const MeMatrix3 B, const MeMatrix3 C);

    /*  MeMatrix3Multiply is nuked, now replaced with MeMatrix3MultiplyMatrix,
        *** WHICH TAKES ITS SOURCE ARGUMENTS IN THE OTHER ORDER ***

        void MEAPI MeMatrix3Multiply(MeMatrix3 A, const MeMatrix3 B, const MeMatrix3 C);
    */

MEPUBLIC
    void MEAPI MeMatrix3MultiplyMatrix(MeMatrix3 A, const MeMatrix3 B, const MeMatrix3 C);
MEPUBLIC
    void MEAPI MeMatrix3MakeIdentity(MeMatrix3 tm);
MEPUBLIC
    void MEAPI MeMatrix3Scale(MeMatrix3 A, const MeReal a);
MEPUBLIC
    MeReal MEAPI MeMatrix3Trace(MeMatrix3 m);
#endif
MEPUBLIC
    MeBool MEAPI MeMatrix3IsIdentity(const MeMatrix3 a, const MeReal tolerance);
MEPUBLIC
    void MEAPI MeMatrix3LUDecompose(MeMatrix3 L, MeMatrix3 U, const MeMatrix3 a,
                    unsigned int *const SwappedRow, MeVector3 scale);
MEPUBLIC
    void MEAPI MeMatrix3SwapColumns(MeMatrix3 a, const unsigned int col1, const unsigned int col2);
MEPUBLIC
    MeBool MEAPI MeMatrix3Invert(MeMatrix3 a);
MEPUBLIC
    MeBool MEAPI MeMatrix3SymmetricInvert(MeMatrix3 a);
MEPUBLIC
    void MEAPI MeMatrix3MakeRotationX(MeMatrix3 m, const MeReal a);
MEPUBLIC
    void MEAPI MeMatrix3MakeRotationY(MeMatrix3 m, const MeReal a);
MEPUBLIC
    void MEAPI MeMatrix3MakeRotationZ(MeMatrix3 m, const MeReal a);
MEPUBLIC
    MeBool MEAPI MeMatrix3IsValidOrientationMatrix(const MeMatrix3 rot, const MeReal tolerance);
MEPUBLIC
    void MEAPI MeMatrix3FromEulerAngles(MeMatrix3 m,
                    const MeReal xangle,
                    const MeReal yangle, const MeReal zangle);

/***** MEMATRIX4 *****/
#if (!MeDEFINE)
MEPUBLIC
    void MEAPI MeMatrix4SetZero(MeMatrix4 A);
MEPUBLIC
    void MEAPI MeMatrix4Add(MeMatrix4 A, const MeMatrix4 B, const MeMatrix4 C);
MEPUBLIC
    MeBool MEAPI MeMatrix4IsZero(const MeMatrix4 m);
MEPUBLIC
    void MEAPI MeMatrix4Copy(MeMatrix4 A, const MeMatrix4 B);

    /*  MeMatrix4Multiply is nuked, now replaced with MeMatrix4MultiplMatrix,
        *** WHICH TAKES ITS SOURCE ARGUMENTS IN THE OTHER ORDER ***

        void MEAPI MeMatrix4Multiply(MeMatrix4 A, const MeMatrix4 B, const MeMatrix4 C);
    */

MEPUBLIC
    void MEAPI MeMatrix4MultiplyMatrix(MeMatrix4 A, const MeMatrix4 B, const MeMatrix4 C);
MEPUBLIC
    void MEAPI MeMatrix4MultiplyVector(MeVector4 A, const MeMatrix4 B, const MeVector4 C);
MEPUBLIC
    void MEAPI MeMatrix4Transpose(MeMatrix4 A);
#endif
    MeBool MEAPI MeMatrix4IsIdentity(const MeMatrix4 a, const MeReal tolerance);

/***** MEMATRIX4 TRANSFORMATION *****/
#if (!MeDEFINE)
MEPUBLIC
    void MEAPI MeMatrix4TMGetRotation(MeMatrix3 R, const MeMatrix4 tm);
MEPUBLIC
    void MEAPI MeMatrix4TMMakeFromRotationAndPosition(MeMatrix4 A,
                              const MeMatrix3 R,
                              const MeReal x,
                              const MeReal y, const MeReal z);
MEPUBLIC
    void MEAPI MeMatrix4TMSetRotation(MeMatrix4 tm, const MeMatrix3 R);
MEPUBLIC
    void MEAPI MeMatrix4TMSetRotationFromQuaternion(MeMatrix4 tm, const MeVector4 q);
MEPUBLIC
    void MEAPI MeMatrix4TMMakeIdentity(MeMatrix4 tm);
MEPUBLIC
    void MEAPI MeMatrix4TMSetPosition(MeMatrix4 tm, MeReal x, MeReal y, MeReal z);
MEPUBLIC
    void MEAPI MeMatrix4TMTransform(MeVector3 out, const MeMatrix4 tm, const MeVector3 in);
MEPUBLIC
    void MEAPI MeMatrix4TMInverseTransform(MeVector3 out, const MeMatrix4 tm, const MeVector3 in);
MEPUBLIC
    void MEAPI MeMatrix4TMRotate(MeVector3 out, const MeMatrix4 tm, const MeVector3 in);
MEPUBLIC
    void MEAPI MeMatrix4TMInverseRotate(MeVector3 out, const MeMatrix4 tm, const MeVector3 in);
#endif
MEPUBLIC
    MeBool MEAPI MeMatrix4IsTM(const MeMatrix4 tm, const MeReal tolerance);
MEPUBLIC
    void MEAPI MeMatrix4TMCompound(MeMatrix4 A, const MeMatrix4 B, const MeMatrix4 C);
MEPUBLIC
    void MEAPI MeMatrix4TMInvert(MeMatrix4 tm);
MEPUBLIC
    void MEAPI MeMatrix4TMUpdateFromVelocities(MeMatrix4 aTransformReturn,
                           MeReal aEpsilon,
                           MeReal aTimeStep,
                           const MeVector3 aVelocity,
                           const MeVector3 aAngularVelocity,
                           const MeMatrix4 aTransform);

MEPUBLIC
    void MEAPI MeMatrix4TMUpdateFromVelocitiesAndAcceler(MeMatrix4 aTransformReturn,
                             MeReal aEpsilon,
                             MeReal aTimeStep,
                             const MeVector3 aVelocity,
                             const MeVector3 aAcceler,
                             const MeVector3 aAngularVelocity,
                             const MeVector3 aAngularAcceler,
                             const MeMatrix4 aTransform);

MEPUBLIC
    void MEAPI MeMatrix4TMOrthoNormalize(MeMatrix4 tm);

MEPUBLIC
void MEAPI MeMatrix4TMFromEulerAnglesAndPosition(MeMatrix4 tm,
                                    const MeReal xangle,
                                    const MeReal yangle, 
                                    const MeReal zangle,
                                    const MeReal x,
                                    const MeReal y,
                                    const MeReal z);
/***** OTHER *****/
#if (!MeDEFINE)
/* Useful indexing functions */
MEPUBLIC
    int MEAPI MeUpperDiagonalIndex(const int inRow, const int inCol, const int inNCols);
MEPUBLIC
    int MEAPI MeUpperDiagonalSize(const int inNCols);
MEPUBLIC
    int MEAPI MeSymUpperDiagonalIndex(const int inRow, const int inCol, const int inNCols);
MEPUBLIC
    int MEAPI MeSuperDiagonalIndex(const int inRow, const int inCol, const int inNCols);
MEPUBLIC
    int MEAPI MeSuperDiagonalSize(const int inNCols);
MEPUBLIC
    int MEAPI MeSymSuperDiagonalIndex(const int inRow, const int inCol, const int inNCols);
MEPUBLIC
    MeBool MEAPI MeRealArrayIsZero(MeReal *v, int elements);

#endif

#ifdef __cplusplus
}
#endif
#if (MeDEFINE)

/* Returns the index i for which a[i] is minimized. */
MeINLINE MeI32
MinIndex( const MeReal a[3] )
{
    const MeI32 index = (0x21312300>>( (a[0]>a[1])<<4 | (a[0]>a[2])<<3 | (a[1]>a[2])<<2 ))&3;
    MEASSERT( index != 3 ); // logically impossible
    return index;
}

/* Fills index array such that a[ index[0] ] <= a[ index[1] ] <= a[ index[2] ] */
MeINLINE void
Sort3( MeI32 index[3], const MeReal a[3] )
{
    const MeI32 shift = (a[0]>a[1])<<4 | (a[0]>a[2])<<3 | (a[1]>a[2])<<2;
    index[0] = (0x21312300>>shift)&3; // least element
    index[1] = (0x12300321>>shift)&3; // middle element
    index[2] = (0x00321312>>shift)&3; // greatest element
    MEASSERT( index[0] != 3 && index[1] != 3 && index[2] != 3 ); // logically impossible
}

/*	Next integer, modulo N.  Assumes input is in range {0,...,N-1} */
MeINLINE int MEAPI NextMod2(const int i) { return i^1; }
MeINLINE int MEAPI NextMod3(const int i) { return (1<<i)&3; }
MeINLINE int MEAPI NextMod4(const int i) { return (i+1)&3; }
MeINLINE int MEAPI NextModN(const int i, const int N) { const int i1 = i+1; return i1&((i1-N)>>31); }

/*	Previous integer, modulo N.  Assumes input is in range {0,...,N-1} */
MeINLINE int MEAPI PrevMod2(const int i) { return i^1; }
MeINLINE int MEAPI PrevMod3(const int i) { const int i1 = i-1; return i1+(3&(i1>>31)); }
MeINLINE int MEAPI PrevMod4(const int i) { return (i-1)&3; }
MeINLINE int MEAPI PrevModN(const int i, const int N) { const int i1 = i-1; return i1+(N&(i1>>31)); }

/*  Special-case cross product: vec x axis */
MeINLINE void
MeVector3CrossAxis(MeVector3 result, MeVector3 vec, const int axisN)
{
    const int axisN1 = NextMod3(axisN);
    const int axisN2 = NextMod3(axisN1);
    result[axisN] = 0;
    result[axisN1] = vec[axisN2];
    result[axisN2] = -vec[axisN1];
}

/** Swap the real values addressed by pointers a and b. */ 
MeINLINE void MEAPI
MeRealSwap(MeReal *const a, MeReal *const b)
{
    const MeReal temp = *a;
    MEASSERT(a != 0);
    MEASSERT(b != 0);
    *a = *b;
    *b = temp;
}

/**
 * Set the first \p n elements of \p A to zero.
 */
MeINLINE void MEAPI
MeVectorSetZero(MeReal *const A, const int n)
{
    int       i;

    for (i = 0; i < n; i++)
    A[i] = 0.0f;
}

/**
 * MeVector3 copy.
 */
MeINLINE void MEAPI
MeVector3Copy(MeVector3 c, const MeVector3 b)
{
    c[0] = b[0];
    c[1] = b[1];
    c[2] = b[2];
}

/**
 * MeVector3 copy.
 */
MeINLINE void MEAPI
MeVector3Set(MeVector3 c, const MeReal x, const MeReal y, const MeReal z)
{
    c[0] = x;
    c[1] = y;
    c[2] = z;
}

/**
 * MeVector3 subtraction.
 */
MeINLINE void MEAPI
MeVector3Subtract(MeVector3 a, const MeVector3 b, const MeVector3 c)
{
    a[0] = b[0] - c[0], a[1] = b[1] - c[1], a[2] = b[2] - c[2];
}

/**
 * MeVector3 addition.
 */
MeINLINE void MEAPI
MeVector3Add(MeVector3 a, const MeVector3 b, const MeVector3 c)
{
    a[0] = b[0] + c[0], a[1] = b[1] + c[1], a[2] = b[2] + c[2];
}

/**
 * MeVector3 dot product.
 */
MeINLINE MeReal MEAPI
MeVector3Dot(const MeVector3 b, const MeVector3 c)
{
    return b[0] * c[0] + b[1] * c[1] + b[2] * c[2];
}

/**
 * MeVector3 cross product.
 */
MeINLINE void MEAPI
MeVector3Cross(MeVector3 a, const MeVector3 b, const MeVector3 c)
{
    a[0] = b[1] * c[2] - b[2] * c[1];
    a[1] = b[2] * c[0] - b[0] * c[2];
    a[2] = b[0] * c[1] - b[1] * c[0];
}

/**
 * MeVector3 outer product.
 */
MeINLINE void MEAPI
MeVector3OuterProduct(MeMatrix3 a, const MeVector3 b, const MeVector3 c)
{
    a[0][0] = b[0] * c[0];
    a[0][1] = b[0] * c[1];
    a[0][2] = b[0] * c[2];
    a[1][0] = b[1] * c[0];
    a[1][1] = b[1] * c[1];
    a[1][2] = b[1] * c[2];
    a[2][0] = b[2] * c[0];
    a[2][1] = b[2] * c[1];
    a[2][2] = b[2] * c[2];
}

/**
 * Return the square of the magnitude of the vector.
 */
MeINLINE MeReal MEAPI
MeVector3MagnitudeSqr(const MeVector3 v)
{
    MeReal    m = (MeReal) 0;

    m += MeSqr(v[0]);
    m += MeSqr(v[1]);
    m += MeSqr(v[2]);

    return m;
}

/**
 * Return the square of the magnitude of the vector.
 */
MeINLINE MeReal MEAPI
MeVector3Magnitude(const MeVector3 v)
{
    return MeSqrt(MeVector3MagnitudeSqr(v));
}

/**
 * Return the square of the distance from a to b.
 */
MeINLINE MeReal MEAPI
MeVector3DistanceSqr(const MeVector3 a, const MeVector3 b)
{
    MeVector3 diff;

    diff[0] = a[0]-b[0], diff[1] = a[1]-b[1], diff[2] = a[2]-b[2];

    return MeVector3Dot(diff,diff);
}

/**
 * Return the distance from a to b.
 */
MeINLINE MeReal MEAPI
MeVector3Distance(const MeVector3 a, const MeVector3 b)
{
    return MeSqrt(MeVector3DistanceSqr(a,b));
}

/**
 * Multiply all elements of a 3 vector by a scalar.
 */
MeINLINE void MEAPI
MeVector3Scale(MeVector3 v, const MeReal a)
{
    v[0] *= a;
    v[1] *= a;
    v[2] *= a;
}

/**
 * Add to each element of a 3 vector the corresponding element of
 * another 3 vector multiplied by a scalar.
 */
MeINLINE void MEAPI
MeVector3MultiplyAdd(MeVector3 v, const MeReal a, const MeVector3 v1)
{
    v[0] += a * v1[0];
    v[1] += a * v1[1];
    v[2] += a * v1[2];
}

/**
 * Subtract from each element of a 3 vector the corresponding element of
 * another 3 vector multiplied by a scalar.
 */
MeINLINE void MEAPI
MeVector3MultiplySubtract(MeVector3 v, const MeReal a, const MeVector3 v1)
{
    v[0] -= a * v1[0];
    v[1] -= a * v1[1];
    v[2] -= a * v1[2];
}

/**
 * Multiply each element of b by c and place resulting vetor into a.
 */
MeINLINE void MEAPI
MeVector3MultiplyScalar(MeVector3 a, const MeVector3 b, const MeReal c)
{
    a[0] = b[0] * c;
    a[1] = b[1] * c;
    a[2] = b[2] * c;
}

/** a = b + (c*d) */
MeINLINE void MEAPI 
MeVector3ScaleAndAdd(MeVector3 a, const MeVector3 b, const MeReal c, const MeVector3 d)
{
    a[0] = b[0] + (c * d[0]);
    a[1] = b[1] + (c * d[1]);
    a[2] = b[2] + (c * d[2]);
}

/**
 * Make the given vector unit length. Returns previous magnitude of vector.
 */
MeINLINE MeReal MEAPI
MeVector3Normalize(MeVector3 v)
{
    int       j;
    MeReal    mag = 0, k = 0;
    
    for (j = 0; j < 3; j++)
        k += MeSqr(v[j]);
    
    if (k > 0) 
    {
        mag = MeSqrt(k);
        k = MeRecip(mag);
        
        v[0] *= k;
        v[1] *= k;
        v[2] *= k;
    } 
    else 
    {
        v[0] = 1;
        v[1] = 0;
        v[2] = 0;

        mag = 0;
    }

    return mag;
}

/**
 * Compute a 3 vector orthogonal to another 3 vector.
 */
MeINLINE void MEAPI
MeVector3MakeOrthogonal(MeVector3 v, const MeVector3 v1)
{
    MeVector3 normalized;
    
    normalized[0] = v1[0], normalized[1] = v1[1], normalized[2] = v1[2];

    MeVector3Normalize(normalized);

    if (normalized[2] < 0.5f && normalized[2] > -0.5f) 
        MeVector3Set(v, -normalized[1], normalized[0], 0);
    else 
        MeVector3Set(v, -normalized[2], 0, normalized[0]);

    MeVector3Normalize(v);
}

/**
 * Make normal 3x1 vectors a and b such that together with the
 * normal 3x1 vector n they form an orthonormal basis.
 *
 * a and b span the plane that is normal to n, and n = a x b.
 * Note that if n is not normalized then b will not be
 * normalized either.
 */

MeINLINE void MEAPI
MeVector3PlaneSpace(const MeVector3 n, MeVector3 a, MeVector3 b)
{
    if (MeFabs(n[0]) > MeFabs(n[1])) {
    /* |n.e1| > |n.e2|, so a = e2 x n */
    MeReal    scale = MeRecipSqrt(n[2] * n[2] + n[0] * n[0]);

    a[0] = n[2] * scale;
    a[1] = 0;
    a[2] = -n[0] * scale;

    /* b = n x a */
    b[0] = n[1] * a[2];
    b[1] = n[2] * a[0] - n[0] * a[2];
    b[2] = -n[1] * a[0];
    } else {
    /* |n.e1| <= |n.e2|, so a = e1 x n */
    MeReal    scale = MeRecipSqrt(n[2] * n[2] + n[1] * n[1]);

    a[0] = 0;
    a[1] = -n[2] * scale;
    a[2] = n[1] * scale;

    /* b = n x a */
    b[0] = n[1] * a[2] - n[2] * a[1];
    b[1] = -n[0] * a[2];
    b[2] = n[0] * a[1];
    }
}

/**
 * MeVector3 multiply element by element
 */

MeINLINE void MEAPI
MeVector3MultiplyElements(MeVector3 a, const MeVector3 b, const MeVector3 c) {
    a[0] = b[0] * c[0];
    a[1] = b[1] * c[1];
    a[2] = b[2] * c[2];
}

/**
 * Clamp all members between max and min
 */
MeINLINE void MeVector3Clamp(MeVector3 a, const MeReal min, const MeReal max) {
    a[0] = a[0] < min ? min : (a[0] > max ? max : a[0]),
        a[1] = a[1] < min ? min : (a[1] > max ? max : a[1]),
        a[2] = a[2] < min ? min : (a[2] > max ? max : a[2]);
}


/**
 * elementwise min of two MeVector3s
 */
MeINLINE void MeVector3Min(MeVector3 out, const MeVector3 in1, const MeVector3 in2)
{
    out[0] = in1[0] < in2[0] ? in1[0] : in2[0];
    out[1] = in1[1] < in2[1] ? in1[1] : in2[1];
    out[2] = in1[2] < in2[2] ? in1[2] : in2[2];
}

/**
 * elementwise max of two MeVector3s
 */
MeINLINE void MeVector3Max(MeVector3 out, const MeVector3 in1, const MeVector3 in2)
{
    out[0] = in1[0] > in2[0] ? in1[0] : in2[0];
    out[1] = in1[1] > in2[1] ? in1[1] : in2[1];
    out[2] = in1[2] > in2[2] ? in1[2] : in2[2];
}

/**
 *  Linear interpolate between two vectors ('from' and 'to') by amount 
 *  'howFar', and place result in 'out'. 
 */
MeINLINE void MeVector3Lerp(MeVector3 out, const MeVector3 from, const MeVector3 to, const MeReal howFar)
{
    out[0] = from[0] + howFar * (to[0] - from[0]);
    out[1] = from[1] + howFar * (to[1] - from[1]);
    out[2] = from[2] + howFar * (to[2] - from[2]);    
}

/**
 *  Scalar triple product of vectors: a . b . c
 */
MeINLINE MeReal MeVector3ScalarTripleProduct(const MeVector3 a, const MeVector3 b, const MeVector3 c)
{
    return (a[1] * b[2] - b[1] * a[2]) * c[0] +
           (a[2] * b[0] - b[2] * a[0]) * c[1] +
           (a[0] * b[1] - b[0] * a[1]) * c[2];
}


/**
 * MeVector4 copy.
 */
MeINLINE void MEAPI
MeVector4Copy(MeVector4 c, const MeVector4 b)
{
    c[0] = b[0], c[1] = b[1], c[2] = b[2], c[3] = b[3];
}

/**
 * MeVector4 subtraction.
 */
MeINLINE void MEAPI
MeVector4Subtract(MeVector4 a, const MeVector4 b, const MeVector4 c)
{
    a[0] = b[0] - c[0], a[1] = b[1] - c[1], a[2] = b[2] - c[2], a[3] = b[3] - c[3];
}

/**
 * Multiply element by element
 */
MeINLINE void MEAPI MeVector4MultiplyElements(MeVector4 a, const MeVector4 b, const MeVector4 c) {
    a[0] = b[0] * c[0], a[1] = b[1] * c[1], a[2] = b[2] * c[2], a[3] = b[3] * c[3];
}

/**
 * Clamp all members between max and min
 */
MeINLINE void MeVector4Clamp(MeVector4 a, const MeReal min, const MeReal max) {
    a[0] = a[0] < min ? min : (a[0] > max ? max : a[0]),
        a[1] = a[1] < min ? min : (a[1] > max ? max : a[1]),
        a[2] = a[2] < min ? min : (a[2] > max ? max : a[2]),
        a[3] = a[3] < min ? min : (a[3] > max ? max : a[3]);
}

/**
 * Scale all elements
*/
MeINLINE void MEAPI MeVector4Scale(MeVector3 v, const MeReal a) {
    v[0] *= a, v[1] *= a, v[2] *= a, v[3] *= a;
}

/**
 * Add elementwise
 */
MeINLINE void MEAPI MeVector4Add(MeVector4 a, const MeVector4 b, const MeVector4 c) {
    a[0] = b[0] + c[0], a[1] = b[1] + c[1], a[2] = b[2] + c[2], a[3] = b[3] + c[3];
}

/**
 * Return the square of the magnitude of the vector.
 */
MeINLINE MeReal MEAPI
MeVector4MagnitudeSqr(const MeVector4 v)
{
    MeReal    m = (MeReal) 0;

    m += MeSqr(v[0]);
    m += MeSqr(v[1]);
    m += MeSqr(v[2]);
    m += MeSqr(v[3]);

    return m;
}

/**
 * MeVector4 dot product.
 */
MeINLINE MeReal MEAPI
MeVector4Dot(const MeVector4 b, const MeVector4 c)
{
    return b[0] * c[0] + b[1] * c[1] + b[2] * c[2] + b[3] * c[3];
}

/**
 * Make a Quaternion q given and normalised axis x and an angle a.
 */
MeINLINE void MEAPI
MeQuaternionMake(MeVector4 q, MeVector3 x, MeReal a)
{
    MeReal    half_a = a / 2;
    MeReal    s = MeSin(half_a);
    MeReal    c = MeCos(half_a);

    q[0] = c;
    q[1] = s * x[0];
    q[2] = s * x[1];
    q[3] = s * x[2];
}

/** Set each element of quaternion. */
MeINLINE void MEAPI 
MeQuaternionSet(MeVector4 q, MeReal w, MeReal x, MeReal y, MeReal z)
{
    q[0] = w;
    q[1] = x;
    q[2] = y;
    q[3] = z;
}

/**
 * Compute a 3x3 rotation matrix R from a 4x1 normalized quaternion q.
 */
MeINLINE void MEAPI
MeQuaternionToR(MeMatrix3 R, const MeVector4 q)
{
    /* 13 multiplications + 12 local variables */
    MeReal    q0sq = q[0] * q[0];
    MeReal    q1sq = q[1] * q[1];
    MeReal    q2sq = q[2] * q[2];
    MeReal    q3sq = q[3] * q[3];
    MeReal    q0t2 = 2 * q[0];
    MeReal    q1t2 = 2 * q[1];
    MeReal    q0q1 = q0t2 * q[1];
    MeReal    q0q2 = q0t2 * q[2];
    MeReal    q0q3 = q0t2 * q[3];
    MeReal    q1q2 = q1t2 * q[2];
    MeReal    q1q3 = q1t2 * q[3];
    MeReal    q2q3 = 2 * q[2] * q[3];

    R[0][0] = q0sq + q1sq - q2sq - q3sq;
    R[1][0] = q1q2 - q0q3;
    R[2][0] = q0q2 + q1q3;

    R[0][1] = q1q2 + q0q3;
    R[1][1] = q0sq - q1sq + q2sq - q3sq;
    R[2][1] = -q0q1 + q2q3;

    R[0][2] = -q0q2 + q1q3;
    R[1][2] = q0q1 + q2q3;
    R[2][2] = q0sq - q1sq - q2sq + q3sq;

    /* 36 multiplications */
    /*
       R[0][0] = q[0] * q[0] + q[1] * q[1] - q[2] * q[2] - q[3] * q[3];
       R[1][0] = 2.0f * q[1] * q[2] - 2.0f * q[0] * q[3];
       R[2][0] = 2.0f * q[0] * q[2] + 2.0f * q[1] * q[3];

       R[0][1] = 2.0f * q[1] * q[2] + 2.0f * q[0] * q[3];
       R[1][1] = q[0] * q[0] - q[1] * q[1] + q[2] * q[2] - q[3] * q[3];
       R[2][1] = -2.0f * q[0] * q[1] + 2.0f * q[2] * q[3];

       R[0][2] = -2.0f * q[0] * q[2] + 2.0f * q[1] * q[3];
       R[1][2] = 2.0f * q[0] * q[1] + 2.0f * q[2] * q[3];
       R[2][2] = q[0] * q[0] - q[1] * q[1] - q[2] * q[2] + q[3] * q[3];
     */
}

/**
 * Convert a quaternion to a rotation matrix.
 *
 * \deprecated This function only sets the top left hand corner of the
 * matrix, and will be changed in a future release to properly generate a valid
 * transformation matrix. To Set a the rotation part of a transformation,
 * use MeMatrix4TMSetRotationFromQuaternion instead
 */

MeINLINE void MEAPI
MeQuaternionToTM(MeMatrix4 tm, const MeVector4 q)
{
    /* 13 multiplications + 12 local variables */
    MeReal    q0sq = q[0] * q[0];
    MeReal    q1sq = q[1] * q[1];
    MeReal    q2sq = q[2] * q[2];
    MeReal    q3sq = q[3] * q[3];
    MeReal    q0t2 = 2 * q[0];
    MeReal    q1t2 = 2 * q[1];
    MeReal    q0q1 = q0t2 * q[1];
    MeReal    q0q2 = q0t2 * q[2];
    MeReal    q0q3 = q0t2 * q[3];
    MeReal    q1q2 = q1t2 * q[2];
    MeReal    q1q3 = q1t2 * q[3];
    MeReal    q2q3 = 2 * q[2] * q[3];

    tm[0][0] = q0sq + q1sq - q2sq - q3sq;
    tm[1][0] = q1q2 - q0q3;
    tm[2][0] = q0q2 + q1q3;

    tm[0][1] = q1q2 + q0q3;
    tm[1][1] = q0sq - q1sq + q2sq - q3sq;
    tm[2][1] = -q0q1 + q2q3;

    tm[0][2] = -q0q2 + q1q3;
    tm[1][2] = q0q1 + q2q3;
    tm[2][2] = q0sq - q1sq - q2sq + q3sq;

    /* 36 multiplications */
    /*
       tm[0][0] = q[0] * q[0] + q[1] * q[1] - q[2] * q[2] - q[3] * q[3];
       tm[1][0] = 2.0f * q[1] * q[2] - 2.0f * q[0] * q[3];
       tm[2][0] = 2.0f * q[0] * q[2] + 2.0f * q[1] * q[3];

       tm[0][1] = 2.0f * q[1] * q[2] + 2.0f * q[0] * q[3];
       tm[1][1] = q[0] * q[0] - q[1] * q[1] + q[2] * q[2] - q[3] * q[3];
       tm[2][1] = -2.0f * q[0] * q[1] + 2.0f * q[2] * q[3];

       tm[0][2] = -2.0f * q[0] * q[2] + 2.0f * q[1] * q[3];
       tm[1][2] = 2.0f * q[0] * q[1] + 2.0f * q[2] * q[3];
       tm[2][2] = q[0] * q[0] - q[1] * q[1] - q[2] * q[2] + q[3] * q[3];
     */
}

/**
 * Set the rotation part of a transformation matrix from a quaternion.
 */

MeINLINE void MEAPI
MeMatrix4TMSetRotationFromQuaternion(MeMatrix4 tm, const MeVector4 q)
{
    /* 13 multiplications + 12 local variables */
    MeReal    q0sq = q[0] * q[0];
    MeReal    q1sq = q[1] * q[1];
    MeReal    q2sq = q[2] * q[2];
    MeReal    q3sq = q[3] * q[3];
    MeReal    q0t2 = 2 * q[0];
    MeReal    q1t2 = 2 * q[1];
    MeReal    q0q1 = q0t2 * q[1];
    MeReal    q0q2 = q0t2 * q[2];
    MeReal    q0q3 = q0t2 * q[3];
    MeReal    q1q2 = q1t2 * q[2];
    MeReal    q1q3 = q1t2 * q[3];
    MeReal    q2q3 = 2 * q[2] * q[3];

    tm[0][0] = q0sq + q1sq - q2sq - q3sq;
    tm[1][0] = q1q2 - q0q3;
    tm[2][0] = q0q2 + q1q3;

    tm[0][1] = q1q2 + q0q3;
    tm[1][1] = q0sq - q1sq + q2sq - q3sq;
    tm[2][1] = -q0q1 + q2q3;

    tm[0][2] = -q0q2 + q1q3;
    tm[1][2] = q0q1 + q2q3;
    tm[2][2] = q0sq - q1sq - q2sq + q3sq;
}

/**
 * Return a 4x3 matrix D corresponding to the 4x1 quaternion q
 * such that dq/dt = D * w, where w is the angular velocity.
 */
MeINLINE void MEAPI
MeQuaternionToD(MeReal D[12], const MeVector4 q)
{
    D[0] = -q[1] * (MeReal) (0.5);
    D[4] = -q[2] * (MeReal) (0.5);
    D[8] = -q[3] * (MeReal) (0.5);

    D[1] = +q[0] * (MeReal) (0.5);
    D[5] = +q[3] * (MeReal) (0.5);
    D[9] = -q[2] * (MeReal) (0.5);

    D[2] = -q[3] * (MeReal) (0.5);
    D[6] = +q[0] * (MeReal) (0.5);
    D[10] = +q[1] * (MeReal) (0.5);

    D[3] = +q[2] * (MeReal) (0.5);
    D[7] = -q[1] * (MeReal) (0.5);
    D[11] = +q[0] * (MeReal) (0.5);
}

/**
 * Given the angular velocity w and the orientation quaternion q,
 * return the quaternion derivative dq.
 *
 * dq/dt = D * w.
 */
MeINLINE void MEAPI
MeQuaternionWtoDQ(MeVector4 dq, const MeVector4 q, const MeVector3 w)
{
    dq[0] = (MeReal) (0.5) * (-q[1] * w[0] - q[2] * w[1] - q[3] * w[2]);
    dq[1] = (MeReal) (0.5) * (+q[0] * w[0] + q[3] * w[1] - q[2] * w[2]);
    dq[2] = (MeReal) (0.5) * (-q[3] * w[0] + q[0] * w[1] + q[1] * w[2]);
    dq[3] = (MeReal) (0.5) * (+q[2] * w[0] - q[1] * w[1] + q[0] * w[2]);
}

/**
 * Multiplication of quaternions: r = p * q
 */

MeINLINE void MEAPI
MeQuaternionProduct(MeVector4 r, const MeVector4 p, const MeVector4 q)
{
    r[0] = q[0] * p[0] - q[1] * p[1] - q[2] * p[2] - q[3] * p[3];

    r[1] = q[0] * p[1] + q[1] * p[0] + p[2] * q[3] - p[3] * q[2];
    r[2] = q[0] * p[2] + q[2] * p[0] + p[3] * q[1] - p[1] * q[3];
    r[3] = q[0] * p[3] + q[3] * p[0] + p[1] * q[2] - p[2] * q[1];
}

/**
 * Rotate a vector using a quaternion.
 *
 * vout=(q0*q0-qv.qv)v + 2(qv.v)qv + 2q0 (qv x v)
 * (where x is cross product)

 */
MeINLINE void MEAPI
MeQuaternionRotateVector3(MeVector3 vout, const MeVector4 q, const MeVector3 v)
{
    MeVector3 qv;

    qv[0] = q[1];
    qv[1] = q[2];
    qv[2] = q[3];

    MeVector3Cross(vout, qv, v);

    MeVector3Scale(vout, 2.0f * q[0]);

    MeVector3MultiplyAdd(vout, MeSqr(q[0]) - MeVector3MagnitudeSqr(qv), v);
    MeVector3MultiplyAdd(vout, 2.0f * MeVector3Dot(qv, v), qv);
}

/**
 * MeMatrix3 Copy
 */
MeINLINE void MEAPI
MeMatrix3Copy(MeMatrix3 A, const MeMatrix3 B)
{
    A[0][0] = B[0][0],
    A[0][1] = B[0][1],
    A[0][2] = B[0][2],
    A[1][0] = B[1][0],
    A[1][1] = B[1][1],
    A[1][2] = B[1][2], A[2][0] = B[2][0], A[2][1] = B[2][1], A[2][2] = B[2][2];
}

/**
 * Set the rows of an MeMatrix3 A from three row vectors.
 */
MeINLINE void MEAPI
MeMatrix3CopyVec(MeMatrix3 A, const MeVector3 B1, const MeVector3 B2, const MeVector3 B3)
{
    A[0][0] = B1[0],
    A[0][1] = B1[1],
    A[0][2] = B1[2],
    A[1][0] = B2[0],
    A[1][1] = B2[1], A[1][2] = B2[2], A[2][0] = B3[0], A[2][1] = B3[1], A[2][2] = B3[2];
}


/** Check if this 3x3 matrix is diagonal with identical values. */
MeINLINE MeBool MEAPI
MeMatrix3IsIsotropic(const MeMatrix3 i)
{
    if (!((i[0][0] == i[1][1]) && (i[1][1] == i[2][2])))
    return 0;

    if (!((i[0][1] == 0) && (i[0][2] == 0) && (i[1][2] == 0)))
    return 0;

    if (!((i[1][0] == 0) && (i[2][0] == 0) && (i[2][1] == 0)))
    return 0;

    return 1;
}

/**
 * Transpose of the n * m matrix B into A
 */
MeINLINE void MEAPI
MeMatrixTranspose(MeReal *A, const int n, const int m, const MeReal *const B)
{
    int       i, j;

    for (i = 0; i < n; i++)
    for (j = 0; j < m; j++)
        A[i + j * n] = B[i * m + j];
}

/**
 * Set A=B*C, where A has dimensions p*r, B has dimensions p*q, C has dimensions q*r.
 *
 * This is an inline function for use on SMALL matrices.
 */
MeINLINE void MEAPI
MeMatrixMultiply(MeReal *A, const int p, const int q, const int r, const MeReal *const B,
         const MeReal *const C)
{
    int       i, j, k;
    for (j = 0; j < r; j++)
        for (i = 0; i < p; i++) {
            const MeReal *c = C + j * q;
            const MeReal *b = B + i;
            MeReal    sum = 0;
            for (k = q; k > 0; k--) {
                sum += (*b) * (*c);
                b += p;
                c++;
            }
            *A++ = sum;
    }
}

/**
 * Set A to be the product of B transposed and C,
 * where A has dimensions p*r, B has dimensions q*p, C has dimensions q*r.
 * This is an inline function for use on SMALL matrices.
 */

MeINLINE void MEAPI
MeMatrixMultiplyT1(MeReal *A, const int p, const int q, const int r, const MeReal *const B,
           const MeReal *const C)
{
    int       i, j, k;
    for (j = 0; j < r; j++) {
        const MeReal *b = B;
        for (i = 0; i < p; i++) {
            const MeReal *c = C + j * q;
            MeReal    sum = 0;
            for (k = q; k > 0; --k)
                sum += (*b++) * (*c++);
            *A++ = sum;
        }
    }
}

/**
 * Make a 3x3 identity matrix.
 */
MeINLINE void MEAPI
MeMatrix3MakeIdentity(MeMatrix3 tm)
{
#if 0
    /*
       This won't work if the matrix is laid out in a funny way.
     */
    memset((void *) tm, 0, 9 * sizeof(MeReal));

    tm[0][0] = (MeReal) (1.0);
    tm[1][1] = (MeReal) (1.0);
    tm[2][2] = (MeReal) (1.0);
#else
    /*
       Why do we write this as a single expression with terms separated
       by the ',' operator? Well, we hope this suggests that all the
       assignments could be done in parallel.
     */
    tm[0][0] = (MeReal) (1.0),
    tm[0][1] = (MeReal) (0.0),
    tm[0][2] = (MeReal) (0.0),
    tm[1][0] = (MeReal) (0.0),
    tm[1][1] = (MeReal) (1.0),
    tm[1][2] = (MeReal) (0.0),
    tm[2][0] = (MeReal) (0.0), tm[2][1] = (MeReal) (0.0), tm[2][2] = (MeReal) (1.0);
#endif
}

/**
 * In place (via an internal copy) transpose of the 3x3 matrix A.
 */
MeINLINE void MEAPI
MeMatrix3Transpose(MeMatrix3 A)
{
    MeMatrix3 tmp;

    memcpy(tmp, A, sizeof(MeMatrix3));

    A[0][1] = tmp[1][0],
    A[0][2] = tmp[2][0],
    A[1][0] = tmp[0][1], A[1][2] = tmp[2][1], A[2][0] = tmp[0][2], A[2][1] = tmp[1][2];
}

/**
 * Set A to a 3x3 matrix corresponding to the 3x1 vector a, such
 * that A*b = a x b (x is the cross product operator).
 *
 * The result is scaled by factor.
 *
 * \deprecated This function will be changed in a future release to generate an MeMatrix3 and
 * remove the scaling factor.
 */
MeINLINE void MEAPI
MeMatrix3CrossFromVector(MeVector3 A0, MeVector3 A1, MeVector3 A2, const MeVector3 a,
             const MeReal factor)
{
    A0[0] = 0;
    A0[1] = a[2] * factor;
    A0[2] = -a[1] * factor;

    A1[0] = -a[2] * factor;
    A1[1] = 0;
    A1[2] = a[0] * factor;

    A2[0] = a[1] * factor;
    A2[1] = -a[0] * factor;
    A2[2] = 0;
}

/**
 * postmultiply a 3x1 vector by 3x3 matrix.
 */
MeINLINE void MEAPI
MeMatrix3MultiplyVector(MeVector3 A, const MeMatrix3 B, const MeVector3 C)
{
    A[0] = B[0][0] * C[0] + B[1][0] * C[1] + B[2][0] * C[2];
    A[1] = B[0][1] * C[0] + B[1][1] * C[1] + B[2][1] * C[2];
    A[2] = B[0][2] * C[0] + B[1][2] * C[1] + B[2][2] * C[2];
}

/**
 * Subtract 3x3 matrices. A = B - C.
 */
MeINLINE void MEAPI
MeMatrix3Subtract(MeMatrix3 A, MeMatrix3 B, MeMatrix3 C)
{
    A[0][0] = B[0][0] - C[0][0];
    A[0][1] = B[0][1] - C[0][1];
    A[0][2] = B[0][2] - C[0][2];
    A[1][0] = B[1][0] - C[1][0];
    A[1][1] = B[1][1] - C[1][1];
    A[1][2] = B[1][2] - C[1][2];
    A[2][0] = B[2][0] - C[2][0];
    A[2][1] = B[2][1] - C[2][1];
    A[2][2] = B[2][2] - C[2][2];
}

/**
 * Add 3x3 matrices. A = B + C.
 */
MeINLINE void MEAPI
MeMatrix3Add(MeMatrix3 A, MeMatrix3 B, MeMatrix3 C)
{
    A[0][0] = B[0][0] + C[0][0];
    A[0][1] = B[0][1] + C[0][1];
    A[0][2] = B[0][2] + C[0][2];
    A[1][0] = B[1][0] + C[1][0];
    A[1][1] = B[1][1] + C[1][1];
    A[1][2] = B[1][2] + C[1][2];
    A[2][0] = B[2][0] + C[2][0];
    A[2][1] = B[2][1] + C[2][1];
    A[2][2] = B[2][2] + C[2][2];
}

/**
 * Multiply 3x3 matrices. A = B*C, where the matrices are stored in row major order.
 */
MeINLINE void MEAPI
MeMatrix3MultiplyMatrix(MeMatrix3 A, const MeMatrix3 B, const MeMatrix3 C)
{
    A[0][0] = B[0][0] * C[0][0] + B[0][1] * C[1][0] + B[0][2] * C[2][0];
    A[0][1] = B[0][0] * C[0][1] + B[0][1] * C[1][1] + B[0][2] * C[2][1];
    A[0][2] = B[0][0] * C[0][2] + B[0][1] * C[1][2] + B[0][2] * C[2][2];

    A[1][0] = B[1][0] * C[0][0] + B[1][1] * C[1][0] + B[1][2] * C[2][0];
    A[1][1] = B[1][0] * C[0][1] + B[1][1] * C[1][1] + B[1][2] * C[2][1];
    A[1][2] = B[1][0] * C[0][2] + B[1][1] * C[1][2] + B[1][2] * C[2][2];

    A[2][0] = B[2][0] * C[0][0] + B[2][1] * C[1][0] + B[2][2] * C[2][0];
    A[2][1] = B[2][0] * C[0][1] + B[2][1] * C[1][1] + B[2][2] * C[2][1];
    A[2][2] = B[2][0] * C[0][2] + B[2][1] * C[1][2] + B[2][2] * C[2][2];


}

/**
 * Multiply all elements of a 3x3 matrix by a scalar.
 */
MeINLINE void MEAPI
MeMatrix3Scale(MeMatrix3 A, const MeReal a)
{
    A[0][0] *= a;
    A[0][1] *= a;
    A[0][2] *= a;
    A[1][0] *= a;
    A[1][1] *= a;
    A[1][2] *= a;
    A[2][0] *= a;
    A[2][1] *= a;
    A[2][2] *= a;
}

/**
   Zero an MeMatrix4
*/
MeINLINE void MEAPI
MeMatrix4SetZero(MeMatrix4 m)
{
    m[0][0] = 0,
    m[0][1] = 0,
    m[0][2] = 0,
    m[0][3] = 0,
    m[1][0] = 0,
    m[1][1] = 0,
    m[1][2] = 0,
    m[1][3] = 0,
    m[2][0] = 0,
    m[2][1] = 0, m[2][2] = 0, m[2][3] = 0, m[3][0] = 0, m[3][1] = 0, m[3][2] = 0, m[3][3] = 0;
}

/**
 * Make a 4x4 identity matrix.
 */
MeINLINE void MEAPI
MeMatrix4TMMakeIdentity(MeMatrix4 tm)
{
#if 0
    /*
       This won't work if the matrix is laid out in a funny way.
     */
    memset((void *) tm, 0, 16 * sizeof(MeReal));

    tm[0][0] = (MeReal) (1.0);
    tm[1][1] = (MeReal) (1.0);
    tm[2][2] = (MeReal) (1.0);
    tm[3][3] = (MeReal) (1.0);
#else
    /*
       Why do we write this as a single expression with terms separated
       by the ',' operator? Well, we hope this suggests that all the
       assignments could be done in parallel.
     */
    tm[0][0] = (MeReal) (1.0),
    tm[0][1] = (MeReal) (0.0),
    tm[0][2] = (MeReal) (0.0),
    tm[0][3] = (MeReal) (0.0),
    tm[1][0] = (MeReal) (0.0),
    tm[1][1] = (MeReal) (1.0),
    tm[1][2] = (MeReal) (0.0),
    tm[1][3] = (MeReal) (0.0),
    tm[2][0] = (MeReal) (0.0),
    tm[2][1] = (MeReal) (0.0),
    tm[2][2] = (MeReal) (1.0),
    tm[2][3] = (MeReal) (0.0),
    tm[3][0] = (MeReal) (0.0),
    tm[3][1] = (MeReal) (0.0), tm[3][2] = (MeReal) (0.0), tm[3][3] = (MeReal) (1.0);
#endif
}

/**
 * Sets position for a MeMatrix4, when interpreted as a transformation
 * matrix.
 */
MeINLINE void MEAPI
MeMatrix4TMSetPosition(MeMatrix4 tm, MeReal x, MeReal y, MeReal z)
{
    tm[3][0] = x;
    tm[3][1] = y;
    tm[3][2] = z;
}

/**
 * Transforms an MeVector3 by the transformation matrix
 * 
 */
MeINLINE void MEAPI
MeMatrix4TMTransform(MeVector3 out, const MeMatrix4 tm, const MeVector3 in)
{
    out[0] = in[0] * tm[0][0] + in[1] * tm[1][0] + in[2] * tm[2][0] + tm[3][0];
    out[1] = in[0] * tm[0][1] + in[1] * tm[1][1] + in[2] * tm[2][1] + tm[3][1];
    out[2] = in[0] * tm[0][2] + in[1] * tm[1][2] + in[2] * tm[2][2] + tm[3][2];
}


/**
 * Transforms an MeVector3 by the inverse of a transformation matrix
 * 
 */
MeINLINE void MEAPI
MeMatrix4TMInverseTransform(MeVector3 out, const MeMatrix4 tm, const MeVector3 in)
{
    MeVector3 tmp;
    tmp[0] = in[0] - tm[3][0];
    tmp[1] = in[1] - tm[3][1];
    tmp[2] = in[2] - tm[3][2];

    out[0] = tmp[0] * tm[0][0] + tmp[1] * tm[0][1] + tmp[2] * tm[0][2];
    out[1] = tmp[0] * tm[1][0] + tmp[1] * tm[1][1] + tmp[2] * tm[1][2];
    out[2] = tmp[0] * tm[2][0] + tmp[1] * tm[2][1] + tmp[2] * tm[2][2];
}


/**
 * Transforms an MeVector3 by the rotation part of a transformation matrix.
 */
MeINLINE void MEAPI
MeMatrix4TMRotate(MeVector3 out, const MeMatrix4 tm, const MeVector3 in)
{
    out[0] = in[0] * tm[0][0] + in[1] * tm[1][0] + in[2] * tm[2][0];
    out[1] = in[0] * tm[0][1] + in[1] * tm[1][1] + in[2] * tm[2][1];
    out[2] = in[0] * tm[0][2] + in[1] * tm[1][2] + in[2] * tm[2][2];
}

/**
 * Transforms an MeVector3 by the transpose of the rotation 
 * part of a transformation matrix.
 */
MeINLINE void MEAPI
MeMatrix4TMInverseRotate(MeVector3 out, const MeMatrix4 tm, const MeVector3 in)
{
    out[0] = in[0] * tm[0][0] + in[1] * tm[0][1] + in[2] * tm[0][2];
    out[1] = in[0] * tm[1][0] + in[1] * tm[1][1] + in[2] * tm[1][2];
    out[2] = in[0] * tm[2][0] + in[1] * tm[2][1] + in[2] * tm[2][2];
}

/**
 * Sets position for a MeMatrix4, when interpreted as a transformation
 * matrix. Alternative form taking an MeVector3 as the position.
 */
MeINLINE void MEAPI
MeMatrix4TMSetPositionVector(MeMatrix4 tm, MeVector3 position)
{
    tm[3][0] = position[0];
    tm[3][1] = position[1];
    tm[3][2] = position[2];
}

/** Updates only the rotation matrix part of the transformation. */
MeINLINE void MEAPI
MeMatrix4TMSetRotation(MeMatrix4 tm, const MeMatrix3 R)
{
    tm[0][0] = R[0][0];
    tm[0][1] = R[0][1];
    tm[0][2] = R[0][2];

    tm[1][0] = R[1][0];
    tm[1][1] = R[1][1];
    tm[1][2] = R[1][2];

    tm[2][0] = R[2][0];
    tm[2][1] = R[2][1];
    tm[2][2] = R[2][2];
}

/** Extract the rotation matrix part of transformation. */
MeINLINE void MEAPI
MeMatrix4TMGetRotation(MeMatrix3 R, const MeMatrix4 tm)
{
    R[0][0] = tm[0][0];
    R[0][1] = tm[0][1];
    R[0][2] = tm[0][2];

    R[1][0] = tm[1][0];
    R[1][1] = tm[1][1];
    R[1][2] = tm[1][2];

    R[2][0] = tm[2][0];
    R[2][1] = tm[2][1];
    R[2][2] = tm[2][2];
}

/**
 * MeMatrix4 Copy
 */
MeINLINE void MEAPI
MeMatrix4Copy(MeMatrix4 A, const MeMatrix4 B)
{
    A[0][0] = B[0][0],
    A[0][1] = B[0][1],
    A[0][2] = B[0][2],
    A[0][3] = B[0][3],
    A[1][0] = B[1][0],
    A[1][1] = B[1][1],
    A[1][2] = B[1][2],
    A[1][3] = B[1][3],
    A[2][0] = B[2][0],
    A[2][1] = B[2][1],
    A[2][2] = B[2][2],
    A[2][3] = B[2][3],
    A[3][0] = B[3][0],
    A[3][1] = B[3][1],
    A[3][2] = B[3][2],
    A[3][3] = B[3][3];
}

/**
    Copy an MeMatrix4. Input and output must be 16 Byte aligned.

    This currently works on platforms where alignment is not as important as
    PS2, even when the data is not aligned. This may change.
*/
MeINLINE void MEAPI
MeMatrix4CopyAligned(MeMatrix4 A, const MeMatrix4 B)
{
#if defined PS2
    MEASSERTALIGNED(A, 16);
    MEASSERTALIGNED(B, 16);
    asm       __volatile__(
                           "lq $8, 0x0(%1)   \n\t"
                           "lq $9, 0x10(%1)  \n\t"
                           "lq $10, 0x20(%1) \n\t"
                           "lq $11, 0x30(%1) \n\t"
               "sq $8, 0x0(%0)   \n\t"
               "sq $9, 0x10(%0)  \n\t"
               "sq $10, 0x20(%0) \n\t"
               "sq $11, 0x30(%0) \n\t"
                 : : "r" (A), "r" (B) : "memory", "$8", "$9", "$10", "$11");
#else
    MeMatrix4Copy(A, B);
#endif
}
/**
 * In place (via an internal copy) transpose of the 4x4 matrix A.
 */
MeINLINE void MEAPI
MeMatrix4Transpose(MeMatrix4 A) {
    MeMatrix4 tmp;
    memcpy(tmp, A, sizeof(MeMatrix4));
    A[0][1] = tmp[1][0],
    A[0][2] = tmp[2][0],
    A[0][3] = tmp[3][0],
    A[1][0] = tmp[0][1],
    A[1][2] = tmp[2][1],
    A[1][3] = tmp[3][1],
    A[2][0] = tmp[0][2],
    A[2][1] = tmp[1][2],
    A[2][3] = tmp[3][2],
    A[3][0] = tmp[0][3],
    A[3][1] = tmp[1][3],
    A[3][2] = tmp[2][3];
}

/**
 * MeMatrix4 Addition
 */
MeINLINE void MEAPI
MeMatrix4Add(MeMatrix4 A, const MeMatrix4 B, const MeMatrix4 C) {
    A[0][0] = B[0][0] + C[0][0],
    A[0][1] = B[0][1] + C[0][1],
    A[0][2] = B[0][2] + C[0][2],
    A[0][3] = B[0][3] + C[0][3],
    A[1][0] = B[1][0] + C[1][0],
    A[1][1] = B[1][1] + C[1][1],
    A[1][2] = B[1][2] + C[1][2],
    A[1][3] = B[1][3] + C[1][3],
    A[2][0] = B[2][0] + C[2][0],
    A[2][1] = B[2][1] + C[2][1],
    A[2][2] = B[2][2] + C[2][2],
    A[2][3] = B[2][3] + C[2][3],
    A[3][0] = B[3][0] + C[3][0],
    A[3][1] = B[3][1] + C[3][1],
    A[3][2] = B[3][2] + C[3][2],
    A[3][3] = B[3][3] + C[3][3];
}

/**
 * postmultiply a 4x1 vector by a 4x4 matrix. If the matrix is a transformation with the
 * translation component in the last row, this operation applies the transformation to the
 * vector.
 */
MeINLINE void MEAPI
MeMatrix4MultiplyVector(MeVector4 A, const MeMatrix4 B, const MeVector4 C) {
#if 0
    MeMatrixMultiply((MeReal *) A, 4, 4, 1, (const MeReal *) B,
             (const MeReal *) C);
#else
    A[0] = B[0][0] * C[0] + B[1][0] * C[1] + B[2][0] * C[2] + B[3][0] * C[3],
    A[1] = B[0][1] * C[0] + B[1][1] * C[1] + B[2][1] * C[2] + B[3][1] * C[3],
    A[2] = B[0][2] * C[0] + B[1][2] * C[1] + B[2][2] * C[2] + B[3][2] * C[3],
    A[3] = B[0][3] * C[0] + B[1][3] * C[1] + B[2][3] * C[2] + B[3][3] * C[3];
#endif
}

/**
 * Multiply 4x4 matrices. The computation is A=B*C, where A, B, and C are
 * stored row major. If this operation is used to compose two transformation
 * matrices, the compound transformation is the result of applying B, then C.
 */

MeINLINE void MEAPI
MeMatrix4MultiplyMatrix(MeMatrix4 A, const MeMatrix4 B, const MeMatrix4 C) {
    MeReal *a = (MeReal *) A;
    int i, j;
    for(i=0;i<4;i++)
    {
        for(j=0;j<4;j++)
            *a++= B[i][0]*C[0][j] + B[i][1]*C[1][j] + B[i][2]*C[2][j] + B[i][3]*C[3][j];
    }
}



/**
 * Fill in a 4x4 matrix given a 3x3 rotation and a position.
 */
MeINLINE void MEAPI
MeMatrix4TMMakeFromRotationAndPosition(MeMatrix4 A,
                       const MeMatrix3 R,
                       const MeReal x,
                       const MeReal y, const MeReal z) {
    A[0][0] = R[0][0];
    A[0][1] = R[0][1];
    A[0][2] = R[0][2];
    A[0][3] = 0;
    A[1][0] = R[1][0];
    A[1][1] = R[1][1];
    A[1][2] = R[1][2];
    A[1][3] = 0;
    A[2][0] = R[2][0];
    A[2][1] = R[2][1];
    A[2][2] = R[2][2];
    A[2][3] = 0; A[3][0] = x; A[3][1] = y; A[3][2] = z; A[3][3] = 1;
}

/**
 * Determines whether a 4x4 matrix has any non-zero elements
 */
MeINLINE MeBool MEAPI
MeMatrix4IsZero(const MeMatrix4 m) {
    int i, j; for (i = 0; i < 4; i++) {
    for (j = 0; j < 4; j++) {
        if (m[i][j])
        return 0;}
    }
    return 1;
}

/* Some useful indexing functions */

/**
 * Provides an index into the triangle ABOVE AND INCLUDING
 * the diagonal in a matrix (row <= col).
 */
MeINLINE int MEAPI
MeUpperDiagonalIndex(const int inRow, const int inCol, const int inNCols)
{
    return inNCols * inRow + inCol - (((inRow + 1) * inRow) >> 1);
}

/**
 * Gives the number of elements ABOVE AND INCLUDING the diagonal in a matrix.
 */
MeINLINE int MEAPI
MeUpperDiagonalSize(const int inNCols) {
    return ((inNCols + 1) * inNCols) >> 1;
}

/**
 * Given any row and column, indexes into array (swaps if row > column).
 * Works on elements that are ABOVE OR INCLUDING the diagonal in a matrix.
 */
MeINLINE int MEAPI
MeSymUpperDiagonalIndex(const int inRow, const int inCol, const int inNCols) {
    MeI32 o = (inCol - inRow) >> 31; MeI32 d = ~o;
    MeI32 rowIndex = (d & inRow) | (o & inCol);
    MeI32 colIndex = (o & inRow) | (d & inCol);
    return MeUpperDiagonalIndex(rowIndex, colIndex, inNCols);
}

/**
 * Provides an index into the triangle ABOVE the diagonal in a matrix (r < c).
 */
MeINLINE int MEAPI
MeSuperDiagonalIndex(const int inRow, const int inCol, const int inNCols)
{
    return inNCols * inRow + inCol - (((inRow + 2) * (inRow + 1)) >> 1);
}

/**
 * Gives the number of elements ABOVE the diagonal in a matrix.
 */
MeINLINE int MEAPI
MeSuperDiagonalSize(const int inNCols) {
    return (inNCols * (inNCols - 1)) >> 1;
}

/**
 * Given any row and column (row != column), indexes into array
 * (swaps if row > column). Works on elements that are ABOVE the
 * diagonal in a matrix.
 */
MeINLINE int MEAPI
MeSymSuperDiagonalIndex(const int inRow, const int inCol, const int inNCols) {
    int o =   (inCol - inRow) >> 31; int d = ~o;
    int rowIndex = (d & inRow) | (o & inCol);
    int colIndex = (o & inRow) | (d & inCol);
    return MeSuperDiagonalIndex(rowIndex, colIndex, inNCols);
}

/**
 * Tests an array of MeReals of length 'elements' for zero.
 * Returns 1 if all elements were zero, 0 if any are found to be non zero.
 */
MeINLINE MeBool MEAPI
MeRealArrayIsZero(MeReal *v, int elements) {
    int i; for (i = 0; i < elements; i++) {
    if (v[i]) return 0;}
    return 1;
}

/**
 * Generates a random number between two MeReals, start and end.
 */
MeINLINE MeReal MEAPI
MeRealRandomInRange(MeReal start, MeReal end) {
    return start + ((end - start) * rand()) / (MeReal) RAND_MAX;
}

/** catch divide by zeros when calculating reciprocals */

MeINLINE MeReal MEAPI MeSafeRecip(MeReal x) {
	return ((MeReal) 0.0f != x ? (MeReal) 1.0f/x : (MeReal) 0.0f);
}


#endif              /* MeDEFINE */

#endif              /* _MEMATH_H */
