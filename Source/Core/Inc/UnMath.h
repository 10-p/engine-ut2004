/*=============================================================================
	UnMath.h: Unreal math routines
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "UnForcePacking_begin.h"

/*-----------------------------------------------------------------------------
	Defintions.
-----------------------------------------------------------------------------*/

// Forward declarations.
class  FVector;
class  FPlane;
class  FCoords;
class  FRotator;
class  FScale;
class  FGlobalMath;
class  FMatrix;
class  FQuat;

// Fixed point conversion.
inline	INT Fix		(INT A)			{return A<<16;};
inline	INT Fix		(FLOAT A)		{return (INT)(A*65536.f);};
inline	INT Unfix	(INT A)			{return A>>16;};

// Constants.
#undef  PI
#define PI 					(3.1415926535897932)
#define SMALL_NUMBER		(1.e-8)
#define KINDA_SMALL_NUMBER	(1.e-4)

// Aux constants.
#define INV_PI			(0.31830988618)
#define HALF_PI			(1.57079632679)

// Magic numbers for numerical precision.
#define DELTA			(0.00001f)
#define SLERP_DELTA		(0.0001f)

/*-----------------------------------------------------------------------------
	Global functions.
-----------------------------------------------------------------------------*/

#if !FAST_FTOL
FORCEINLINE INT appTrunc( const FLOAT F )
{
	return (INT) F;
}
#endif

//
// Snap a value to the nearest grid multiple.
//
inline FLOAT FSnap( FLOAT Location, FLOAT Grid )
{
	if( Grid==0.f )	return Location;
	else			return appFloor((Location + 0.5*Grid)/Grid)*Grid;
}

//
// Internal sheer adjusting function so it snaps nicely at 0 and 45 degrees.
//
inline FLOAT FSheerSnap (FLOAT Sheer)
{
	if		(Sheer < -0.65f) return Sheer + 0.15f;
	else if (Sheer > +0.65f) return Sheer - 0.15f;
	else if (Sheer < -0.55f) return -0.50f;
	else if (Sheer > +0.55f) return 0.50f;
	else if (Sheer < -0.05f) return Sheer + 0.05f;
	else if (Sheer > +0.05f) return Sheer - 0.05f;
	else					 return 0.f;
}

//
// Find the closest power of 2 that is >= N.
//
inline DWORD FNextPowerOfTwo( DWORD N )
{
	if (N<=0L		) return 0L;
	if (N<=1L		) return 1L;
	if (N<=2L		) return 2L;
	if (N<=4L		) return 4L;
	if (N<=8L		) return 8L;
	if (N<=16L	    ) return 16L;
	if (N<=32L	    ) return 32L;
	if (N<=64L 	    ) return 64L;
	if (N<=128L     ) return 128L;
	if (N<=256L     ) return 256L;
	if (N<=512L     ) return 512L;
	if (N<=1024L    ) return 1024L;
	if (N<=2048L    ) return 2048L;
	if (N<=4096L    ) return 4096L;
	if (N<=8192L    ) return 8192L;
	if (N<=16384L   ) return 16384L;
	if (N<=32768L   ) return 32768L;
	if (N<=65536L   ) return 65536L;
	else			  return 0;
}

inline UBOOL FIsPowerOfTwo( DWORD N )
{
	return !(N & (N - 1));
}

//
// Add to a word angle, constraining it within a min (not to cross)
// and a max (not to cross).  Accounts for funkyness of word angles.
// Assumes that angle is initially in the desired range.
//
inline _WORD FAddAngleConfined( INT Angle, INT Delta, INT MinThresh, INT MaxThresh )
{
	if( Delta < 0 )
	{
		if ( Delta<=-0x10000L || Delta<=-(INT)((_WORD)(Angle-MinThresh)))
			return MinThresh;
	}
	else if( Delta > 0 )
	{
		if( Delta>=0x10000L || Delta>=(INT)((_WORD)(MaxThresh-Angle)))
			return MaxThresh;
	}
	return (_WORD)(Angle+Delta);
}

//
// Eliminate all fractional precision from an angle.
//
INT ReduceAngle( INT Angle );

//
// Fast 32-bit float evaluations. 
// Warning: likely not portable, and useful on Pentium class processors only.
//

inline UBOOL IsSmallerPositiveFloat(float F1,float F2)
{
	return ( (*(DWORD*)&F1) < (*(DWORD*)&F2));
}

inline FLOAT MinPositiveFloat(float F1, float F2)
{
	if ( (*(DWORD*)&F1) < (*(DWORD*)&F2)) return F1; else return F2;
}

//
// Warning: 0 and -0 have different binary representations.
//

inline UBOOL EqualPositiveFloat(float F1, float F2)
{
	return ( *(DWORD*)&F1 == *(DWORD*)&F2 );
}

inline UBOOL IsNegativeFloat(float F1)
{
	return ( (*(DWORD*)&F1) >= (DWORD)0x80000000 ); // Detects sign bit.
}

inline FLOAT MaxPositiveFloat(float F1, float F2)
{
	if ( (*(DWORD*)&F1) < (*(DWORD*)&F2)) return F2; else return F1;
}

// Clamp F0 between F1 and F2, all positive assumed.
inline FLOAT ClampPositiveFloat(float F0, float F1, float F2)
{
	if      ( (*(DWORD*)&F0) < (*(DWORD*)&F1)) return F1;
	else if ( (*(DWORD*)&F0) > (*(DWORD*)&F2)) return F2;
	else return F0;
}

// Clamp any float F0 between zero and positive float Range
#define ClipFloatFromZero(F0,Range)\
{\
	if ( (*(DWORD*)&F0) >= (DWORD)0x80000000) F0 = 0.f;\
	else if	( (*(DWORD*)&F0) > (*(DWORD*)&Range)) F0 = Range;\
}

inline FLOAT RangeByteToFloat(BYTE A)
{
	if(A == 128)
		return 0.f;
	else if(A > 128)
		return Min<FLOAT>( (((FLOAT)A) - 128.f) / 127.f, 1.f );
	else
		return Max<FLOAT>( (((FLOAT)A) - 128.f) / 128.f, -1.f );
}

inline BYTE FloatToRangeByte(FLOAT A)
{
	if(A == 0.f)
		return 128;
	else if(A > 0.f)
		return Min<BYTE>( 128 + appRound(A * 127.f), 255 );
	else
		return Max<BYTE>( 128 + appRound(A * 128.f), 0 );
}


#if defined(__PSX2_EE__) && defined(__MWERKS__)

// FASTER PS2 VU0 MATH

inline float Float_Sqrt_VU0( const float x )
{

	float result;

	asm volatile
	(
		"qmtc2		%1, vf1		\n"
		"vsqrt		Q, vf1x		\n"

		"vwaitq					\n"

		"vaddq.x	vf1, vf0, Q	\n"

		"qmfc2		$12, vf1	\n"	 
		"mtc1		$12, %0		\n"

		: "=&f" ( result )
		: "r" ( x )
		: "$12"
	);

	return( result );
	
//	return sqrtf( result );
}

//--------------------------------------------------------------------------------
// 1 / a 
inline float Float_Rcp_VU0( const float a ) 
{
	float result;
	asm volatile
	(
		"mfc1		$12, %1			\n"
		"qmtc2		$12, vf4		\n"

		"vdiv		Q, vf0w, vf4x		# 1/a	\n"

		"vwaitq						\n"

		"vaddq.x	vf4, vf0, Q		\n"

		"qmfc2		$12, vf4		\n"
		"mtc1		$12, %0			\n"

		: "=&f" ( result )
		: "f" ( a )
		: "$12"
	);

	return result;

//	return ( float )( 1.0 / a );
}

//--------------------------------------------------------------------------------
// sqrt( ( a * a ) + ( b * b ) + ( c * c ) ) 
inline float Float_Length3_VU0( const float a, const float b, const float c ) 
{
	float result;

	asm volatile
	(
		"mfc1		$12, %1		\n"
		"mfc1		$13, %2		\n"
		"mfc1		$14, %3		\n"

		"pextlw		$12, $13, $12		#	?	?	b	a	\n"
		"pcpyld		$12, $14, $12		#	?	c	b	a	\n"

		"qmtc2		$12, vf1	\n"

		"vmul.xyz	vf2, vf1, vf1		#	?	cc	bb	aa	\n"

		"vaddy.x	vf2, vf2, vf2y		#	?	cc	bb	aa+bb		\n"
		"vaddz.x	vf2, vf2, vf2z		#	?	cc	bb	aa+bb+cc	\n"

		"vsqrt		Q, vf2x		\n"

		"vwaitq					\n"
	
		"vaddq.x	vf2, vf0, Q			#	?	?	?	sqrt(aa+bb+cc)	\n"

		"qmfc2		$12, vf2	\n"
		"mtc1		$12, %0		\n"

		: "=&f" ( result )
		: "f" ( a ), "f" ( b ), "f" ( c )
		: "$12", "$13", "$14"
	);

	return( result );

//	return 
//	( 
//		( float )sqrtf ( ( a * a ) + ( b * b ) + ( c * c ) )
//	);
}

//--------------------------------------------------------------------------------
// ( ( a * a ) + ( b * b ) + ( c * c ) ) 
/*
inline float Float_Length3Sqrd_VU0( const float a, const float b, const float c ) 
{
	float result;

	asm volatile
	(
		"mfc1		$12, %1			\n"
		"mfc1		$13, %2			\n"
		"mfc1		$14, %3			\n"

		"pextlw		$12, $13, $12		#	?	?	b	a	\n"
		"pcpyld		$12, $14, $12		#	?	c	b	a	\n"

		"qmtc2		$12, vf1		\n"

		"vmul.xyz	vf2, vf1, vf1		#	?	cc	bb	aa	\n"

		"vaddy.x	vf2, vf2, vf2y		#	?	cc	bb	aa+bb		\n"
		"vaddz.x	vf2, vf2, vf2z		#	?	cc	bb	aa+bb+cc	\n"

		"qmfc2		$12, vf2		\n"
		"mtc1		$12, %0			\n"

		: "=&f" ( result )
		: "f" ( a ), "f" ( b ), "f" ( c )
		: "$12", "$13", "$14"
	);

	return( result );

//	return ( ( a * a ) + ( b * b ) + ( c * c ) );
}
*/

//--------------------------------------------------------------------------------
// sqrt( ( a * a ) + ( b * b ) + ( c * c ) + ( d * d ) ) 
inline float Float_Length4_VU0( const float a, const float b, const float c, const float d ) 
{

	float result;

	__asm__ volatile
	(
		"mfc1		$12, %1		\n"
		"mfc1		$13, %2		\n"
		"mfc1		$14, %3		\n"
		"mfc1		$15, %4		\n"

		"pextlw		$12, $13, $12		#	?	?	b	a	\n"
		"pextlw		$13, $15, $14		#	?	?	d	c	\n"
		"pcpyld		$12, $13, $12		#	d	c	b	a	\n"

		"qmtc2		$12, vf1	\n"

		"vmul.xyzw	vf2, vf1, vf1		#	dd	cc	bb	aa	\n"

		"vaddy.x	vf2, vf2, vf2y	\n"
		"vaddz.x	vf2, vf2, vf2z	\n"
		"vaddw.x	vf2, vf2, vf2w	\n"

		"vsqrt		Q, vf2x			\n"

		"vwaitq						\n"
	
		"vaddq.x	vf2, vf0, Q		\n"

		"qmfc2		$12, vf2		\n"
		"mtc1		$12, %0			\n"

		: "=&f" ( result )
		: "f" ( a ), "f" ( b ), "f" ( c ), "f" ( d )
		: "$12", "$13", "$14", "$15"
	);

	return result;

//	return sqrtf ( ( a * a ) + ( b * b ) + ( c * c ) + ( d * d ) );
}


//--------------------------------------------------------------------------------
// ( a * b ) + ( c * d ) 
inline float Float_Mac2_VU0( const float a, const float b, const float c, const float d ) 
{
	float result;
	asm volatile
	(
		"mfc1		$10, %1		\n"
		"mfc1		$11, %2		\n"
		"mfc1		$12, %3		\n"
		"mfc1		$13, %4		\n"

		"pextlw		$14, $12, $10		#	?	?	c	a	\n"
		"pextlw		$15, $13, $11		#	?	?	d	b	\n"

		"qmtc2		$14, vf1	\n"
		"qmtc2		$15, vf2	\n"

		"vmul.xy		vf3, vf1, vf2		#	?	?	cd	ab	\n"

		"vaddy.x		vf3, vf3, vf3		#	?	?	cd	ab+cd	\n"

		"qmfc2		$12, vf3	\n"
		"mtc1		$12, %0		\n"

		: "=&f" ( result )
		: "f" ( a ), "f" ( b ), "f" ( c ), "f" ( d )
		: "$10", "$11", "$12", "$13", "$14", "$15"
	);

	return( result );
	
//	return ( ( a * b ) + ( c * d ) );
}

//--------------------------------------------------------------------------------
// ( a * b ) + ( c * d ) + ( e * f ) 
inline float Float_Mac3_VU0( const float a, const float b, const float c, const float d, const float e, const float f ) 
{
	float result;

	asm volatile
	(
		"mfc1		$10, %1		\n"
		"mfc1		$11, %2		\n"
		"mfc1		$12, %3		\n"
		"mfc1		$13, %4		\n"
		"mfc1		$14, %5		\n"
		"mfc1		$15, %6		\n"

		"pextlw		$10, $12, $10		#	?	?	c	a	\n"
		"pextlw		$11, $13, $11		#	?	?	d	b	\n"
		"pcpyld		$10, $14, $10		#	?	e	c	a	\n"
		"pcpyld		$11, $15, $11		#	?	f	d	b	\n"

		"qmtc2		$10, vf1	\n"
		"qmtc2		$11, vf2	\n"

		"vmul.xyz	vf3, vf1, vf2		#	?	ef	cd	ab	\n"

		"vaddy.x		vf3, vf3, vf3		#	?	ef	cd	ab+cd		\n"
		"vaddz.x		vf3, vf3, vf3		#	?	ef	cd	ab+cd+ef	\n"

		"qmfc2		$12, vf3	\n"
		"mtc1		$12, %0		\n"

		: "=&f" ( result )
		: "f" ( a ), "f" ( b ), "f" ( c ), "f" ( d ), "f" ( e ), "f" ( f )
		: "$10", "$11", "$12", "$13", "$14", "$15"
	);

	return( result );

//	return ( ( a * b ) + ( c * d ) + ( e * f ) );
}

//--------------------------------------------------------------------------------
// ( a * b ) + ( c * d ) + ( e * f ) + ( g * h ) 
inline float Float_Mac4_VU0( const float a, const float b, const float c, const float d, const float e, const float f, const float g, const float h ) 
{
	float result;

	asm volatile
	(
		"mfc1		$8, %1		\n"
		"mfc1		$9, %2		\n"
		"mfc1		$10, %3		\n"
		"mfc1		$11, %4		\n"
		"mfc1		$12, %5		\n"
		"mfc1		$13, %6		\n"
		"mfc1		$14, %7		\n"
		"mfc1		$15, %8		\n"
				
		"pextlw		$8, $10, $8			#	?	?	c	a	\n"
		"pextlw		$9,	$11, $9			#	?	?	d	b	\n"
		"pextlw		$10, $14, $12		#	?	?	g	e	\n"
		"pextlw		$11, $15, $13		#	?	?	h	f	\n"
		"pcpyld		$12, $10, $8		#	g	e	c	a	\n"
		"pcpyld		$13, $11, $9		#	h	f	d	b	\n"

		"qmtc2		$12, vf1	\n"
		"qmtc2		$13, vf2	\n"

		"vmul.xyzw	vf3, vf1, vf2		#	gh	ef	cd	ab	\n"

		"vaddy.x		vf3, vf3, vf3		#	gh	ef	cd	ab+cd		\n"
		"vaddz.x		vf3, vf3, vf3		#	gh	ef	cd	ab+cd+ef	\n"
		"vaddw.x		vf3, vf3, vf3		#	gh	ef	cd	ab+cd+ef+gh	\n"
	
		"qmfc2		$12, vf3	\n"
		"mtc1		$12, %0		\n"
	
		: "=&f" ( result )
		: "f" ( a ), "f" ( b ), "f" ( c ), "f" ( d ), "f" ( e ), "f" ( f ), "f" ( g ), "f" ( h )
		: "$8", "$9", "$10", "$11", "$12", "$13", "$14", "$15"
	);

	return( result );
	
//	return ( ( a * b ) + ( c * d ) + ( e * f ) + ( g * h ) );
}


float inline Determinant2_VU0( const float a, const float b, const float c, const float d )
{
	return( Float_Mac2_VU0( a, d, -b, c ) );
}

float inline Determinant3_VU0( const float a1, const float a2, const float a3, 
						const float b1, const float b2, const float b3, 
						const float c1, const float c2, const float c3)
{
	return Float_Mac3_VU0( 
		a1,	Determinant2_VU0( b2, b3, c2, c3 ),
		-b1, Determinant2_VU0( a2, a3, c2, c3 ),
		c1,	Determinant2_VU0( a2, a3, b2, b3 )
	);
}


// Currently assumes they are 16 byte aligned vectors within the box, this is not the 
// case and such it can not be used as is.// convert loading to mfc1 $13 %2  and transfer to qmtc2 	$12, vf1
/*
bool TestAABBs_VU0( FBox * pAABB1, FBox * pAABB2 )
{
	bool result;
//#if __PSX2_EE__

__asm__ volatile
(
	".set noreorder	\n"

	"lqc2 vf4, 0(%1)			\n"
	"lqc2 vf5, 16(%1)			\n"
	"lqc2 vf6, 0(%2)			\n"
	"lqc2 vf7, 16(%2)			\n"
	"vsub.xyz vf9, vf5, vf6		\n"
	"vsub.xyz vf8, vf7, vf4		\n"
	"vnop						\n"
	"vnop						\n"
	"cfc2 $12, $vi17 #	mac flags	\n"
	"cfc2 $13, $vi17 #   mac flags	\n"
	"or $12, $12, $13			\n"
	"andi $12, $12, 0xE0		\n"
	"seq %0, $12, $0			\n"
	
	".set reorder\n"
	
	: "=&r" ( result )
	: "r" ( pAABB1 ), "r" ( pAABB2 )
	: "$12", "$13"
);

//#else // _PSX2_EE__

//	result =
//	(pAABB1->min.v[X] <= pAABB2->max.v[X]) &&
//	(pAABB1->max.v[X] >= pAABB2->min.v[X]) &&
//	(pAABB1->min.v[Y] <= pAABB2->max.v[Y]) &&
//	(pAABB1->max.v[Y] >= pAABB2->min.v[Y]) &&
//	(pAABB1->min.v[Z] <= pAABB2->max.v[Z]) &&
//	(pAABB1->max.v[Z] >= pAABB2->min.v[Z]);
	
//#endif // M_ARCH_PS2

	return result;
}
*/


// Note vectors passed in here have to be 16 byte aligned when declared
void inline MaxMinVector( FVector & Max, FVector & Min, const FVector * array, int arraySize )
{
//	ASSERT( array );
//	ASSERT( arraySize > 1 );

//	#pragma optimization_level 0
	__asm__ volatile
	(
		".set noreorder		\n"
		".set noat			\n"

		"lqc2		vf4, 0(%2)			# load quad word 3rd variable passed to function into min vector	\n"
		"lqc2		vf5, 0(%2)			# load quad word 3rd variable passed to function into max vector	\n"
		"li			$12, 1				# load one into counter	\n"

"VectorMinMax_next:	\n"
		"beq			$12, %3, VectorMinMax_exit	# _if that is equal then break loop	\n"
		"addiu		%2, %2, 16			# add to pointer while waiting for load			\n"
		"lqc2		vf6, 0(%2)			# add 16 bytes to pointer and load next point	\n"
		"vmini		vf4, vf4, vf6		# pull out min		\n"
		"vmax		vf5, vf5, vf6		# pull out max		\n"
		"addiu		$12, $12, 1			# increment counter in branch delay slot		\n"
		"b			VectorMinMax_next	# start branch		\n"
		"nop	\n"

"VectorMinMax_exit:	\n"
		"sqc2		vf4, 0(%1)			# stuff vector back into main memory Min		\n"
		"sqc2		vf5, 0(%0)			# stuff vector back into main memory Max		\n"

		".set at		\n"
		".set reorder	\n"

		:																		
		: "r" ( &Max ), "r" ( &Min ), "r" ( array ), "r" ( arraySize )	
		: "$12"
	);
//	#pragma optimization_level reset
		// no output variables
		// 3 input variables
		// messed up regs 12, but 12 is a temp fnt regs and do not have to be restored by convention on the return of a calling fnt
}


#endif // __PSX2_EE__


/*-----------------------------------------------------------------------------
	FVector.
-----------------------------------------------------------------------------*/

// Information associated with a floating point vector, describing its
// status as a point in a rendering context.
enum EVectorFlags
{
	FVF_OutXMin		= 0x04,	// Outcode rejection, off left hand side of screen.
	FVF_OutXMax		= 0x08,	// Outcode rejection, off right hand side of screen.
	FVF_OutYMin		= 0x10,	// Outcode rejection, off top of screen.
	FVF_OutYMax		= 0x20,	// Outcode rejection, off bottom of screen.
	FVF_OutNear     = 0x40, // Near clipping plane.
	FVF_OutFar      = 0x80, // Far clipping plane.
	FVF_OutReject   = (FVF_OutXMin | FVF_OutXMax | FVF_OutYMin | FVF_OutYMax), // Outcode rejectable.
	FVF_OutSkip		= (FVF_OutXMin | FVF_OutXMax | FVF_OutYMin | FVF_OutYMax), // Outcode clippable.
};

//
// Floating point vector.
//

class CORE_API FVector 
{
public:
	// Variables.
	FLOAT X,Y,Z;

	// Constructors.
	FORCEINLINE FVector()
	{}
	FORCEINLINE FVector( FLOAT InX, FLOAT InY, FLOAT InZ )
	:	X(InX), Y(InY), Z(InZ)
	{}

	// Binary math operators.
	FORCEINLINE FVector operator^( const FVector& V ) const
	{
#if defined(__MWERKS__) && defined(__PSX2_EE__)

		FVector AlignedThis GCC_PACK(16);
		FVector AlignedOther GCC_PACK(16);
		FVector AlignedCross GCC_PACK(16);
		AlignedThis = *this;
		AlignedOther = V;
		
		asm volatile
		(
			"lqc2		vf4, 0(%1)\n"
			"lqc2		vf5, 0(%2)\n"
	
			"vopmula.xyz	ACC, vf4, vf5\n"
			"vopmsub.xyz	vf6, vf5, vf4\n"

			"sqc2		vf6, 0(%0)\n"

			: 
			:  "r" ( &AlignedCross ) , "r" ( &AlignedThis ), "r" ( &AlignedOther )
			:  "$12", "$13"
		);

		return AlignedCross;
#else // __PSX2_EE__
		return FVector
		(
			Y * V.Z - Z * V.Y,
			Z * V.X - X * V.Z,
			X * V.Y - Y * V.X
		);
#endif
	}
	FORCEINLINE FLOAT operator|( const FVector& V ) const
	{
#if defined(__MWERKS__) && defined(__PSX2_EE__)
		return Float_Mac3_VU0( X, V.X, Y, V.Y, Z, V.Z );
#else
		return X*V.X + Y*V.Y + Z*V.Z;
#endif
	}
	FORCEINLINE friend FVector operator*( FLOAT Scale, const FVector& V )
	{
		return FVector( V.X * Scale, V.Y * Scale, V.Z * Scale );
	}
	FORCEINLINE FVector operator+( const FVector& V ) const
	{
		return FVector( X + V.X, Y + V.Y, Z + V.Z );
	}
	FORCEINLINE FVector operator-( const FVector& V ) const
	{
		return FVector( X - V.X, Y - V.Y, Z - V.Z );
	}
	FORCEINLINE FVector operator*( FLOAT Scale ) const
	{
		return FVector( X * Scale, Y * Scale, Z * Scale );
	}
	FORCEINLINE FVector operator/( FLOAT Scale ) const
	{
		FLOAT RScale = 1.f/Scale;
		return FVector( X * RScale, Y * RScale, Z * RScale );
	}
	FORCEINLINE FVector operator*( const FVector& V ) const
	{
		return FVector( X * V.X, Y * V.Y, Z * V.Z );
	}

	// Binary comparison operators.
	FORCEINLINE UBOOL operator==( const FVector& V ) const
	{
		return X==V.X && Y==V.Y && Z==V.Z;
	}
	FORCEINLINE UBOOL operator!=( const FVector& V ) const
	{
		return X!=V.X || Y!=V.Y || Z!=V.Z;
	}

	// Unary operators.
	FORCEINLINE FVector operator-() const
	{
		return FVector( -X, -Y, -Z );
	}

	// Assignment operators.
	FORCEINLINE FVector operator+=( const FVector& V )
	{
		X += V.X; Y += V.Y; Z += V.Z;
		return *this;
	}
	FORCEINLINE FVector operator-=( const FVector& V )
	{
		X -= V.X; Y -= V.Y; Z -= V.Z;
		return *this;
	}
	FORCEINLINE FVector operator*=( FLOAT Scale )
	{
		X *= Scale; Y *= Scale; Z *= Scale;
		return *this;
	}
	FORCEINLINE FVector operator/=( FLOAT V )
	{
		FLOAT RV = 1.f/V;
		X *= RV; Y *= RV; Z *= RV;
		return *this;
	}
	FORCEINLINE FVector operator*=( const FVector& V )
	{
		X *= V.X; Y *= V.Y; Z *= V.Z;
		return *this;
	}
	FORCEINLINE FVector operator/=( const FVector& V )
	{
		X /= V.X; Y /= V.Y; Z /= V.Z;
		return *this;
	}
    FORCEINLINE FLOAT& operator[]( INT i )
	{
		check(i>-1);
		check(i<3);
		if( i == 0 )		return X;
		else if( i == 1)	return Y;
		else				return Z;
	}

	// Simple functions.
	FORCEINLINE FLOAT GetMax() const
	{
		return Max(Max(X,Y),Z);
	}
	FORCEINLINE FLOAT GetAbsMax() const
	{
		return Max(Max(Abs(X),Abs(Y)),Abs(Z));
	}
	FORCEINLINE FLOAT GetMin() const
	{
		return Min(Min(X,Y),Z);
	}
	FORCEINLINE FLOAT Size() const
	{
#if defined(__MWERKS__) && defined(__PSX2_EE__)
			return Float_Length3_VU0( X, Y, Z );
#else
		return appSqrt( X*X + Y*Y + Z*Z );
#endif
	}
	FORCEINLINE FLOAT SizeSquared() const
	{
		return X*X + Y*Y + Z*Z;
	}
	FORCEINLINE FLOAT Size2D() const 
	{
		return appSqrt( X*X + Y*Y );
	}
	FORCEINLINE FLOAT SizeSquared2D() const 
	{
		return X*X + Y*Y;
	}
	FORCEINLINE int IsNearlyZero() const
	{
		return
				Abs(X)<KINDA_SMALL_NUMBER
			&&	Abs(Y)<KINDA_SMALL_NUMBER
			&&	Abs(Z)<KINDA_SMALL_NUMBER;
	}
	FORCEINLINE UBOOL IsZero() const
	{
		return X==0.f && Y==0.f && Z==0.f;
	}
	FORCEINLINE UBOOL Normalize()
	{
#if defined(__MWERKS__) && defined(__PSX2_EE__)
		float scale( Float_Length3_VU0( X, Y, Z ) );
		if( scale >= SMALL_NUMBER )
		{
			scale = Float_Rcp_VU0( scale );
			X *= scale;
			Y *= scale;
			Z *= scale;
			return 1;
		}
		return 0;
#else
		FLOAT SquareSum = X*X+Y*Y+Z*Z;
		if( SquareSum >= SMALL_NUMBER )
		{
			FLOAT Scale = 1.f/appSqrt(SquareSum);
			X *= Scale; Y *= Scale; Z *= Scale;
			return 1;
		}
		else return 0;
#endif
	}
	FORCEINLINE FVector GetNormalized()
	{
		FLOAT SquareSum = X*X+Y*Y+Z*Z;
		if( SquareSum >= SMALL_NUMBER )
		{
			FLOAT Scale = 1.f/appSqrt(SquareSum);
			X *= Scale; Y *= Scale; Z *= Scale;
			return *this;
		}

		return FVector(0,0,0);
	}
	// Expects a unit vector and returns a vector that is sufficiently non parallel ;)
	FORCEINLINE FVector GetNonParallel()
	{
		// One of the components in a unit vector has to be > 0.57f [sqrt(1/3)].
		if ( Abs(X) > 0.57f )
			return FVector(0,1,0);
		else if ( Abs(Y) > 0.57f )
			return FVector(0,0,1);
		else
			return FVector(1,0,0);
	}
	FORCEINLINE FVector Projection() const
	{
		FLOAT RZ = 1.f/Z;
		return FVector( X*RZ, Y*RZ, 1 );
	}
	FORCEINLINE FVector UnsafeNormal() const
	{
#if defined(__MWERKS__) && defined(__PSX2_EE__)
		FLOAT Scale = Float_Rcp_VU0( Float_Length3_VU0( X, Y, Z ) );
#else
		FLOAT Scale = 1.f/appSqrt(X*X+Y*Y+Z*Z);
#endif
		return FVector( X*Scale, Y*Scale, Z*Scale );
	}
	FORCEINLINE FVector SafeNormal() const
	{
		FLOAT SquareSum = X*X + Y*Y + Z*Z;
		if( SquareSum < SMALL_NUMBER )
			return FVector( 0.f, 0.f, 0.f );
		FLOAT Scale = 1.f/appSqrt(SquareSum);
		return FVector( X*Scale, Y*Scale, Z*Scale );
	}
	FORCEINLINE FVector GridSnap( const FVector& Grid )
	{
		return FVector( FSnap(X, Grid.X),FSnap(Y, Grid.Y),FSnap(Z, Grid.Z) );
	}
	FORCEINLINE FVector BoundToCube( FLOAT Radius )
	{
		return FVector
		(
			Clamp(X,-Radius,Radius),
			Clamp(Y,-Radius,Radius),
			Clamp(Z,-Radius,Radius)
		);
	}
	FORCEINLINE void AddBounded( const FVector& V, FLOAT Radius=MAXSWORD )
	{
		*this = (*this + V).BoundToCube(Radius);
	}
	FORCEINLINE FLOAT& Component( INT Index )
	{
		return (&X)[Index];
	}

	// Return a boolean that is based on the vector's direction.
	// When      V==(0,0,0) Booleanize(0)=1.
	// Otherwise Booleanize(V) <-> !Booleanize(!B).
	FORCEINLINE UBOOL Booleanize()
	{
		return
			X >  0.f ? 1 :
			X <  0.f ? 0 :
			Y >  0.f ? 1 :
			Y <  0.f ? 0 :
			Z >= 0.f ? 1 : 0;
	}

	// See if X == Y == Z (within fairly small tolerance)
	FORCEINLINE UBOOL IsUniform()
	{
		return (Abs(X-Y) < KINDA_SMALL_NUMBER) && (Abs(Y-Z) < KINDA_SMALL_NUMBER);
	}

	// Transformation.
	FVector TransformVectorBy( const FCoords& Coords ) const;
	FVector TransformPointBy( const FCoords& Coords ) const;
	FVector MirrorByVector( const FVector& MirrorNormal ) const;
	FVector MirrorByPlane( const FPlane& MirrorPlane ) const;
	FVector PivotTransform(const FCoords& Coords) const;
	FVector TransformVectorByTranspose( const FCoords& Coords ) const;

	// Complicated functions.
	FRotator Rotation();
	void FindBestAxisVectors( FVector& Axis1, FVector& Axis2 );
	FVector RotateAngleAxis( const INT Angle, const FVector& Axis ) const;

	// Friends.
	friend FLOAT FDist( const FVector& V1, const FVector& V2 );
	friend FLOAT FDistSquared( const FVector& V1, const FVector& V2 );
	friend UBOOL FPointsAreSame( const FVector& P, const FVector& Q );
	friend UBOOL FPointsAreNear( const FVector& Point1, const FVector& Point2, FLOAT Dist);
	friend FLOAT FPointPlaneDist( const FVector& Point, const FVector& PlaneBase, const FVector& PlaneNormal );
	friend FVector FLinePlaneIntersection( const FVector& Point1, const FVector& Point2, const FVector& PlaneOrigin, const FVector& PlaneNormal );
	friend FVector FLinePlaneIntersection( const FVector& Point1, const FVector& Point2, const FPlane& Plane );
	friend UBOOL FParallel( const FVector& Normal1, const FVector& Normal2 );
	friend UBOOL FCoplanar( const FVector& Base1, const FVector& Normal1, const FVector& Base2, const FVector& Normal2 );

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FVector& V )
	{
		return Ar << V.X << V.Y << V.Z;
	}
};

// Used by the multiple vertex editing function to keep track of selected vertices.
class ABrush;
class CORE_API FVertexHit
{
public:
	// Variables.
	ABrush* pBrush;
	INT PolyIndex;
	INT VertexIndex;

	// Constructors.
	FVertexHit()
	{
		pBrush = NULL;
		PolyIndex = VertexIndex = 0;
	}
	FVertexHit( ABrush* InBrush, INT InPolyIndex, INT InVertexIndex )
	{
		pBrush = InBrush;
		PolyIndex = InPolyIndex;
		VertexIndex = InVertexIndex;
	}

	// Functions.
	UBOOL operator==( const FVertexHit& V ) const
	{
		return pBrush==V.pBrush && PolyIndex==V.PolyIndex && VertexIndex==V.VertexIndex;
	}
	UBOOL operator!=( const FVertexHit& V ) const
	{
		return pBrush!=V.pBrush || PolyIndex!=V.PolyIndex || VertexIndex!=V.VertexIndex;
	}
};

/*-----------------------------------------------------------------------------
	FEdge.
-----------------------------------------------------------------------------*/

class CORE_API FEdge
{
public:
	// Constructors.
	FEdge()
	{}
	FEdge( FVector v1, FVector v2)
	{
		Vertex[0] = v1;
		Vertex[1] = v2;
	}

	FVector Vertex[2];

	UBOOL operator==( const FEdge& E ) const
	{
		return ( (E.Vertex[0] == Vertex[0] && E.Vertex[1] == Vertex[1]) 
			|| (E.Vertex[0] == Vertex[1] && E.Vertex[1] == Vertex[0]) );
	}
};

/*-----------------------------------------------------------------------------
	FPlane.
-----------------------------------------------------------------------------*/

class CORE_API FPlane : public FVector
{
public:
	// Variables.
	FLOAT W;

	// Constructors.
	FORCEINLINE FPlane()
	{}
	FORCEINLINE FPlane( const FPlane& P )
	:	FVector(P)
	,	W(P.W)
	{}
	FORCEINLINE FPlane( const FVector& V )
	:	FVector(V)
	,	W(0)
	{}
	FORCEINLINE FPlane( FLOAT InX, FLOAT InY, FLOAT InZ, FLOAT InW )
	:	FVector(InX,InY,InZ)
	,	W(InW)
	{}
	FORCEINLINE FPlane( FVector InNormal, FLOAT InW )
	:	FVector(InNormal), W(InW)
	{}
	FORCEINLINE FPlane( FVector InBase, const FVector &InNormal )
	:	FVector(InNormal)
	,	W(InBase | InNormal)
	{}
	FPlane( FVector A, FVector B, FVector C )
	:	FVector( ((B-A)^(C-A)).SafeNormal() )
	,	W( A | ((B-A)^(C-A)).SafeNormal() )
	{}

	// Functions.
	FORCEINLINE FLOAT PlaneDot( const FVector &P ) const
	{
		return X*P.X + Y*P.Y + Z*P.Z - W;
	}
	FPlane Flip() const
	{
		return FPlane(-X,-Y,-Z,-W);
	}
	FPlane TransformPlaneByOrtho( const FMatrix& M ) const;
	FPlane TransformBy( const FMatrix& M ) const;
	FPlane TransformByUsingAdjointT( const FMatrix& M, const FLOAT DetM, const FMatrix& TA ) const;
	FPlane TransformPlaneByOrtho( const FCoords& Coords ) const;
	FPlane TransformBy( const FCoords& Coords ) const;
	UBOOL operator==( const FPlane& V ) const
	{
		return X==V.X && Y==V.Y && Z==V.Z && W==V.W;
	}
	UBOOL operator!=( const FPlane& V ) const
	{
		return X!=V.X || Y!=V.Y || Z!=V.Z || W!=V.W;
	}
	FPlane operator+( const FPlane& V ) const
	{
		return FPlane( X + V.X, Y + V.Y, Z + V.Z, W + V.W );
	}
	FPlane operator-( const FPlane& V ) const
	{
		return FPlane( X - V.X, Y - V.Y, Z - V.Z, W - V.W );
	}
	FPlane operator/( FLOAT Scale ) const
	{
		FLOAT RScale = 1.f/Scale;
		return FPlane( X * RScale, Y * RScale, Z * RScale, W * RScale );
	}
	FPlane operator*( FLOAT Scale ) const
	{
		return FPlane( X * Scale, Y * Scale, Z * Scale, W * Scale );
	}
	FPlane operator*( const FPlane& V )
	{
		return FPlane ( X*V.X,Y*V.Y,Z*V.Z,W*V.W );
	}
	FPlane operator+=( const FPlane& V )
	{
		X += V.X; Y += V.Y; Z += V.Z; W += V.W;
		return *this;
	}
	FPlane operator-=( const FPlane& V )
	{
		X -= V.X; Y -= V.Y; Z -= V.Z; W -= V.W;
		return *this;
	}
	FPlane operator*=( FLOAT Scale )
	{
		X *= Scale; Y *= Scale; Z *= Scale; W *= Scale;
		return *this;
	}
	FPlane operator*=( const FPlane& V )
	{
		X *= V.X; Y *= V.Y; Z *= V.Z; W *= V.W;
		return *this;
	}
	FPlane operator/=( FLOAT V )
	{
		FLOAT RV = 1.f/V;
		X *= RV; Y *= RV; Z *= RV; W *= RV;
		return *this;
	}

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FPlane &P )
	{
		return Ar << (FVector&)P << P.W;
	}
};

// gam ---
template<class T> struct TBox
{
    T X1, Y1, X2, Y2;

    bool Test( T X, T Y ) const
    {
        return (( X >= X1 ) && ( X <= X2 ) && ( Y >= Y1 ) && ( Y <= Y2 ));
    }
};

typedef TBox<INT> FIntBox;
typedef TBox<FLOAT> FFloatBox;
// --- gam

/*-----------------------------------------------------------------------------
	FSphere.
-----------------------------------------------------------------------------*/

class CORE_API FSphere : public FPlane
{
public:
	// Constructors.
	FSphere()
	{}
	FSphere( INT )
	:	FPlane(0,0,0,0)
	{}
	FSphere( FVector V, FLOAT W )
	:	FPlane( V, W )
	{}
	FSphere( const FVector* Pts, INT Count );

	FSphere TransformBy(const FMatrix& M) const;

	friend FArchive& operator<<( FArchive& Ar, FSphere& S )
	{
		guardSlow(FSphere<<);
		if( Ar.Ver()<=61 )//oldver
			Ar << (FVector&)S;
		else
			Ar << (FPlane&)S;
		return Ar;
		unguardSlow
	}
};

/*-----------------------------------------------------------------------------
	FScale.
-----------------------------------------------------------------------------*/

// An axis along which sheering is performed.
enum ESheerAxis
{
	SHEER_None = 0,
	SHEER_XY   = 1,
	SHEER_XZ   = 2,
	SHEER_YX   = 3,
	SHEER_YZ   = 4,
	SHEER_ZX   = 5,
	SHEER_ZY   = 6,
};

//
// Scaling and sheering info associated with a brush.  This is 
// easily-manipulated information which is built into a transformation
// matrix later.
//
class CORE_API FScale 
{
public:
	// Variables.
	FVector		Scale;
	FLOAT		SheerRate;
	BYTE		SheerAxis; // From ESheerAxis

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FScale &S )
	{
		return Ar << S.Scale << S.SheerRate << S.SheerAxis;
	}

	// Constructors.
	FScale() {}
	FScale( const FVector &InScale, FLOAT InSheerRate, ESheerAxis InSheerAxis )
	:	Scale(InScale), SheerRate(InSheerRate), SheerAxis(InSheerAxis) {}

	// Operators.
	UBOOL operator==( const FScale &S ) const
	{
		return Scale==S.Scale && SheerRate==S.SheerRate && SheerAxis==S.SheerAxis;
	}

	// Functions.
	FLOAT  Orientation()
	{
		return Sgn(Scale.X * Scale.Y * Scale.Z);
	}
};

/*-----------------------------------------------------------------------------
	FCoords.
-----------------------------------------------------------------------------*/

//
// A coordinate system matrix.
//
class CORE_API FCoords
{
public:
	FVector	Origin;
	FVector	XAxis;
	FVector YAxis;
	FVector ZAxis;

	// Constructors.
	FCoords() {}
	FCoords( const FVector &InOrigin )
	:	Origin(InOrigin), XAxis(1,0,0), YAxis(0,1,0), ZAxis(0,0,1) {}
	FCoords( const FVector &InOrigin, const FVector &InX, const FVector &InY, const FVector &InZ )
	:	Origin(InOrigin), XAxis(InX), YAxis(InY), ZAxis(InZ) {}

	// Functions.
	FCoords MirrorByVector( const FVector& MirrorNormal ) const;
	FCoords MirrorByPlane( const FPlane& MirrorPlane ) const;
	FCoords	Transpose() const;
	FCoords Inverse() const;
	FCoords PivotInverse() const;
	FCoords ApplyPivot(const FCoords& CoordsB) const;
	FRotator OrthoRotation() const;
	FMatrix Matrix() const;

	// Operators.
	FCoords& operator*=	(const FCoords   &TransformCoords);
	FCoords	 operator*	(const FCoords   &TransformCoords) const;
	FCoords& operator*=	(const FVector   &Point);
	FCoords  operator*	(const FVector   &Point) const;
	FCoords& operator*=	(const FRotator  &Rot);
	FCoords  operator*	(const FRotator  &Rot) const;
	FCoords& operator*=	(const FScale    &Scale);
	FCoords  operator*	(const FScale    &Scale) const;
	FCoords& operator/=	(const FVector   &Point);
	FCoords  operator/	(const FVector   &Point) const;
	FCoords& operator/=	(const FRotator  &Rot);
	FCoords  operator/	(const FRotator  &Rot) const;
	FCoords& operator/=	(const FScale    &Scale);
	FCoords  operator/	(const FScale    &Scale) const;

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FCoords& F )
	{
		return Ar << F.Origin << F.XAxis << F.YAxis << F.ZAxis;
	}
};

/*-----------------------------------------------------------------------------
	FModelCoords.
-----------------------------------------------------------------------------*/

//
// A model coordinate system, describing both the covariant and contravariant
// transformation matrices to transform points and normals by.
//
class CORE_API FModelCoords
{
public:
	// Variables.
	FCoords PointXform;		// Coordinates to transform points by  (covariant).
	FCoords VectorXform;	// Coordinates to transform normals by (contravariant).

	// Constructors.
	FModelCoords()
	{}
	FModelCoords( const FCoords& InCovariant, const FCoords& InContravariant )
	:	PointXform(InCovariant), VectorXform(InContravariant)
	{}

	// Functions.
	FModelCoords Inverse()
	{
		return FModelCoords( VectorXform.Transpose(), PointXform.Transpose() );
	}
};

/*-----------------------------------------------------------------------------
	FRotator.
-----------------------------------------------------------------------------*/

//
// Rotation.
//
class CORE_API FRotator
{
public:
	// Variables.
	INT Pitch; // Looking up and down (0=Straight Ahead, +Up, -Down).
	INT Yaw;   // Rotating around (running in circles), 0=East, +North, -South.
	INT Roll;  // Rotation about axis of screen, 0=Straight, +Clockwise, -CCW.

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FRotator& R )
	{
		return Ar << R.Pitch << R.Yaw << R.Roll;
	}

	// Constructors.
	FRotator() {}
	FRotator( INT InPitch, INT InYaw, INT InRoll )
	:	Pitch(InPitch), Yaw(InYaw), Roll(InRoll) {}

	// Binary arithmetic operators.
	FRotator operator+( const FRotator &R ) const
	{
		return FRotator( Pitch+R.Pitch, Yaw+R.Yaw, Roll+R.Roll );
	}
	FRotator operator-( const FRotator &R ) const
	{
		return FRotator( Pitch-R.Pitch, Yaw-R.Yaw, Roll-R.Roll );
	}
	FRotator operator*( FLOAT Scale ) const
	{
		return FRotator( (INT)(Pitch*Scale), (INT)(Yaw*Scale), (INT)(Roll*Scale) );
	}
	friend FRotator operator*( FLOAT Scale, const FRotator &R )
	{
		return FRotator( (INT)(R.Pitch*Scale), (INT)(R.Yaw*Scale), (INT)(R.Roll*Scale) );
	}
	FRotator operator*= (FLOAT Scale)
	{
		Pitch = (INT)(Pitch*Scale); Yaw = (INT)(Yaw*Scale); Roll = (INT)(Roll*Scale);
		return *this;
	}
	// Binary comparison operators.
	UBOOL operator==( const FRotator &R ) const
	{
		return Pitch==R.Pitch && Yaw==R.Yaw && Roll==R.Roll;
	}
	UBOOL operator!=( const FRotator &V ) const
	{
		return Pitch!=V.Pitch || Yaw!=V.Yaw || Roll!=V.Roll;
	}
	// Assignment operators.
	FRotator operator+=( const FRotator &R )
	{
		Pitch += R.Pitch; Yaw += R.Yaw; Roll += R.Roll;
		return *this;
	}
	FRotator operator-=( const FRotator &R )
	{
		Pitch -= R.Pitch; Yaw -= R.Yaw; Roll -= R.Roll;
		return *this;
	}
	// Functions.
	FRotator Reduce() const
	{
		return FRotator( ReduceAngle(Pitch), ReduceAngle(Yaw), ReduceAngle(Roll) );
	}
	int IsZero() const
	{
		return ((Pitch&65535)==0) && ((Yaw&65535)==0) && ((Roll&65535)==0);
	}
	FRotator Add( INT DeltaPitch, INT DeltaYaw, INT DeltaRoll )
	{
		Yaw   += DeltaYaw;
		Pitch += DeltaPitch;
		Roll  += DeltaRoll;
		return *this;
	}
	FRotator AddBounded( INT DeltaPitch, INT DeltaYaw, INT DeltaRoll )
	{
		Yaw  += DeltaYaw;
		Pitch = FAddAngleConfined(Pitch,DeltaPitch,192*0x100,64*0x100);
		Roll  = FAddAngleConfined(Roll, DeltaRoll, 192*0x100,64*0x100);
		return *this;
	}
	FRotator GridSnap( const FRotator &RotGrid )
	{
		return FRotator
		(
			(INT)(FSnap(Pitch,RotGrid.Pitch)),
			(INT)(FSnap(Yaw,  RotGrid.Yaw)),
			(INT)(FSnap(Roll, RotGrid.Roll))
		);
	}
	FVector Vector();
	// Resets the rotation values so they fall within the range -65535,65535
	FRotator Clamp()
	{
		guard(FRotator::Clamp);
		return FRotator( Pitch%65535, Yaw%65535, Roll%65535 );
		unguard;
	}
	FRotator ClampPos()
	{
		guard(FRotator::Clamp);
		return FRotator( Abs(Pitch)%65535, Abs(Yaw)%65535, Abs(Roll)%65535 );
		unguard;
	}
};

/*-----------------------------------------------------------------------------
	FPosition.
-----------------------------------------------------------------------------*/

// A convenience class for keep track of positions.
class CORE_API FPosition
{
public:
	// Variables.
	FVector Location;
	FCoords Coords;

	// Constructors.
	FPosition()
	{}
	FPosition( FVector InLocation, FCoords InCoords )
	:	Location(InLocation), Coords(InCoords)
	{}
};

/*-----------------------------------------------------------------------------
	FRange.
-----------------------------------------------------------------------------*/

// sjs ---
// quick and dirty random numbers ( > 10x faster than rand() and 100x worse! )
extern CORE_API unsigned int qRandSeed;
const float INV_MAX_QUICK_RAND = 1.0f/0xffff;

FORCEINLINE void qSeedRand( unsigned int inSeed )
{
	qRandSeed = inSeed;
}

FORCEINLINE unsigned int qRand( void )
{
   qRandSeed = (qRandSeed * 196314165) + 907633515;
   return (qRandSeed >> 17);
}

FORCEINLINE float qFRand( void )
{
   qRandSeed = (qRandSeed * 196314165) + 907633515;
   return (float)(qRandSeed>>16) * INV_MAX_QUICK_RAND;
}
// --- sjs

//
// Floating point range. Aaron Leiby
//
// - changed to Min/Max: vogel
//
class CORE_API FRange 
{
public:
	// Variables.
	FLOAT Min, Max;

	// Constructors.
	FORCEINLINE FRange()
	{}
	FORCEINLINE	FRange( FLOAT InMin, FLOAT InMax )
	{
		Min = ::Min(InMin, InMax);
		Max = ::Max(InMin, InMax);
	}
	FORCEINLINE FRange( FLOAT Value ) 
	{
		Min = Value;
		Max = Value;
	}
	// Binary math operators.
	FORCEINLINE friend FRange operator*( FLOAT Scale, const FRange& R )
	{
		return FRange( R.Min * Scale, R.Max * Scale );
	}
	FORCEINLINE FRange operator+( const FRange& R ) const
	{
		return FRange( Min + R.Min, Max + R.Max );
	}
	FORCEINLINE FRange operator+( FLOAT V ) const
	{
		return FRange( Min + V, Max + V );
	}
	FORCEINLINE FRange operator-( FLOAT V ) const
	{
		return FRange( Min - V, Max - V );
	}
	FORCEINLINE FRange operator-( const FRange& R ) const
	{
		return FRange( Min - R.Min, Max - R.Max );
	}
	FORCEINLINE FRange operator*( FLOAT Scale ) const
	{
		return FRange( Min * Scale, Max * Scale );
	}
	FORCEINLINE FRange operator/( FLOAT Scale ) const
	{
		FLOAT RScale = 1.0/Scale;
		return FRange( Min * RScale, Max * RScale );
	}
	FORCEINLINE FRange operator*( const FRange& R ) const
	{
		return FRange( Min * R.Min, Max * R.Max );
	}

	// Binary comparison operators.
	FORCEINLINE UBOOL operator==( const FRange& R ) const
	{
		return Min==R.Min && Max==R.Max;
	}
	FORCEINLINE UBOOL operator!=( const FRange& R ) const
	{
		return Min!=R.Min || Max!=R.Max;
	}

	// Unary operators.
	FORCEINLINE FRange operator-() const
	{
		return FRange( -Min, -Max );
	}

	// Assignment operators.
	FORCEINLINE FRange operator+=( const FRange& R )
	{
		Min += R.Min; Max += R.Max;
		return *this;
	}
	FORCEINLINE FRange operator-=( const FRange& R )
	{
		Min -= R.Min; Max -= R.Max;
		return *this;
	}
	FORCEINLINE FRange operator+=( FLOAT V )
	{
		Min += V; Max += V;
		return *this;
	}
	FORCEINLINE FRange operator-=( FLOAT V )
	{
		Min -= V; Max -= V;
		return *this;
	}
	FORCEINLINE FRange operator*=( FLOAT Scale )
	{
		Min *= Scale; Max *= Scale;
		return *this;
	}
	FORCEINLINE FRange operator/=( FLOAT Scale )
	{
		FLOAT RScale = 1.0/Scale;
		Min *= RScale; Max *= RScale;
		return *this;
	}
	FORCEINLINE FRange operator*=( const FRange& R )
	{
		Min *= R.Min; Max *= R.Max;
		return *this;
	}
	FORCEINLINE FRange operator/=( const FRange& R )
	{
		Min /= R.Min; Max /= R.Max;
		return *this;
	}

	// Simple functions.
	FORCEINLINE FLOAT GetMax() const
	{
		return ::Max(Min,Max);
	}
	FORCEINLINE FLOAT GetMin() const
	{
		return ::Min(Min,Max);
	}
	FORCEINLINE FLOAT Size() const
	{
		return GetMax() - GetMin();
	}
	FORCEINLINE FLOAT GetCenter() const
	{
		return (Max + Min) / 2.f;
	}
	FORCEINLINE FLOAT GetRand() const
	{
		return Max + (Min - Max) * appFrand();	// order is irrelevant since appFrand() is equally distributed between 0 and 1.
	}
	FORCEINLINE FLOAT GetSRand() const
	{
		return Max + (Min - Max) * appSRand();
	}
#if 0
	FORCEINLINE INT GetRandInt() const
	{
		return appRandRange( (INT)Min, (INT)Max );
	} 
#endif
	FORCEINLINE int IsNearlyZero() const
	{
		return
				Abs(Min)<KINDA_SMALL_NUMBER
			&&	Abs(Max)<KINDA_SMALL_NUMBER;
	}
	FORCEINLINE UBOOL IsZero() const
	{
		return Min==0.0 && Max==0.0;
	}
	FORCEINLINE FRange GridSnap( const FRange& Grid )
	{
		return FRange( FSnap(Min, Grid.Min),FSnap(Max, Grid.Max) );
	}
	FORCEINLINE FLOAT& Component( INT Index )
	{
		return (&Min)[Index];
	}

	// When      R==(0.0) Booleanize(0)=1.
	// Otherwise Booleanize(R) <-> !Booleanize(!R).
	FORCEINLINE UBOOL Booleanize()
	{
		return
			Min >  0.0 ? 1 :
			Min <  0.0 ? 0 :
			Max >= 0.0 ? 1 : 0;
	}

	// Serializer.
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, FRange& R )
	{
		return Ar << R.Min << R.Max;
	}
};

/*-----------------------------------------------------------------------------
	FRangeVector.
-----------------------------------------------------------------------------*/

//
// Vector of floating point ranges.
//
class CORE_API FRangeVector
{
public:
	// Variables.
	FRange X, Y, Z;

	// Constructors.
	FORCEINLINE FRangeVector()
	{}
	FORCEINLINE FRangeVector( FRange InX, FRange InY, FRange InZ)
	:	X(InX), Y(InY), Z(InZ)
	{}
	FORCEINLINE FRangeVector( FVector V )
	:	X(V.X), Y(V.Y), Z(V.Z)
	{}
	// Binary math operators.
	FORCEINLINE friend FRangeVector operator*( FLOAT Scale, const FRangeVector& R )
	{
		return FRangeVector( R.X * Scale, R.Y * Scale, R.Z * Scale );
	}
	FORCEINLINE FRangeVector operator+( const FRangeVector& R ) const
	{
		return FRangeVector( X + R.X, Y + R.Y, Z + R.Z );
	}
	FORCEINLINE FRangeVector operator-( const FRangeVector& R ) const
	{
		return FRangeVector( X - R.X, Y - R.Y, Z - R.Z );
	}
	FORCEINLINE FRangeVector operator+( const FVector& V ) const
	{
		return FRangeVector( X + V.X, Y + V.Y, Z + V.Z );
	}
	FORCEINLINE FRangeVector operator-( const FVector& V ) const
	{
		return FRangeVector( X - V.X, Y - V.Y, Z - V.Z );
	}
	FORCEINLINE FRangeVector operator*( FLOAT Scale ) const
	{
		return FRangeVector( X * Scale, Y * Scale, Y * Scale );
	}
	FORCEINLINE FRangeVector operator/( FLOAT Scale ) const
	{
		FLOAT RScale = 1.0/Scale;
		return FRangeVector( X * RScale, Y * RScale, Z * RScale );
	}
	FORCEINLINE FRangeVector operator*( const FRangeVector& R ) const
	{
		return FRangeVector( X * R.X, Y * R.Y, Z * R.Z );
	}

	// Binary comparison operators.
	FORCEINLINE UBOOL operator==( const FRangeVector& R ) const
	{
		return X==R.X && Y==R.Y && Z==R.Z;
	}
	FORCEINLINE UBOOL operator!=( const FRangeVector& R ) const
	{
		return X!=R.X || Y!=R.Y || Z!=R.Z;
	}

	// Unary operators.
	FORCEINLINE FRangeVector operator-() const
	{
		return FRangeVector( -X, -Y, -Z );
	}

	// Assignment operators.
	FORCEINLINE FRangeVector operator+=( const FRangeVector& R )
	{
		X += R.X; Y += R.Y; Z += R.Z;
		return *this;
	}
	FORCEINLINE FRangeVector operator-=( const FRangeVector& R )
	{
		X -= R.X; Y -= R.Y; Z -= R.Z;
		return *this;
	}
	FORCEINLINE FRangeVector operator+=( const FVector& V )
	{
		X += V.X; Y += V.Y; Z += V.Z;
		return *this;
	}
	FORCEINLINE FRangeVector operator-=( const FVector& V )
	{
		X -= V.X; Y -= V.Y; Z -= V.Z;
		return *this;
	}
	FORCEINLINE FRangeVector operator*=( FLOAT Scale )
	{
		X *= Scale; Y *= Scale; Z *= Scale;
		return *this;
	}
	FORCEINLINE FRangeVector operator/=( FLOAT Scale )
	{
		FLOAT RScale = 1.0/Scale;
		X *= RScale; Y *= RScale; Z *= RScale;
		return *this;
	}
	FORCEINLINE FRangeVector operator*=( const FRangeVector& R )
	{
		X *= R.X; Y *= R.Y; Z *= R.Z;
		return *this;
	}
	FORCEINLINE FRangeVector operator/=( const FRangeVector& R )
	{
		X /= R.X; Y /= R.Y; Z /= R.Z;
		return *this;
	}
	FORCEINLINE FVector GetCenter() const
	{
		return FVector( X.GetCenter(), Y.GetCenter(), Z.GetCenter() );
	}
	FORCEINLINE FVector GetMax() const
	{
		return FVector( X.GetMax(), Y.GetMax(), Z.GetMax() );
	}
	FORCEINLINE FVector GetRand() const
	{
		return FVector( X.GetRand(), Y.GetRand(), Z.GetRand() );		
	}
	FORCEINLINE FVector GetSRand() const
	{
		return FVector( X.GetSRand(), Y.GetSRand(), Z.GetSRand() );		
	}	
	FORCEINLINE int IsNearlyZero() const
	{
		return
				X.IsNearlyZero()
			&&	Y.IsNearlyZero()
			&&  Z.IsNearlyZero();
	}
	FORCEINLINE UBOOL IsZero() const
	{
		return X.IsZero() && Y.IsZero() && Z.IsZero();
	}
	FORCEINLINE FRangeVector GridSnap( const FRangeVector& Grid )
	{
		return FRangeVector( X.GridSnap(Grid.X), Y.GridSnap(Grid.Y), Z.GridSnap(Grid.Z) );
	}
	FORCEINLINE FRange& Component( INT Index )
	{
		return (&X)[Index];
	}

	// Serializer.
	FORCEINLINE friend FArchive& operator<<( FArchive& Ar, FRangeVector& R )
	{
		return Ar << R.X << R.Y << R.Z;
	}
};


/*-----------------------------------------------------------------------------
	Bounds.
-----------------------------------------------------------------------------*/

//
// A rectangular minimum bounding volume.
//
class CORE_API FBox
{
public:
	// Variables.
	FVector Min;
	FVector Max;
	BYTE IsValid;

	// Constructors.
	FBox() {}
	FBox(INT) { Init(); }
	FBox( const FVector& InMin, const FVector& InMax ) : Min(InMin), Max(InMax), IsValid(1) {}
	FBox( const FVector* Points, INT Count );

	// Accessors.
	FVector& GetExtrema( int i )
	{
		return (&Min)[i];
	}
	const FVector& GetExtrema( int i ) const
	{
		return (&Min)[i];
	}

	// Functions.
	void Init()
	{
		Min = Max = FVector(0,0,0);
		IsValid = 0;
	}
	FBox& operator+=( const FVector &Other )
	{
		if( IsValid )
		{
			Min.X = ::Min( Min.X, Other.X );
			Min.Y = ::Min( Min.Y, Other.Y );
			Min.Z = ::Min( Min.Z, Other.Z );

			Max.X = ::Max( Max.X, Other.X );
			Max.Y = ::Max( Max.Y, Other.Y );
			Max.Z = ::Max( Max.Z, Other.Z );
		}
		else
		{
			Min = Max = Other;
			IsValid = 1;
		}
		return *this;
	}
	FBox operator+( const FVector& Other ) const
	{
		return FBox(*this) += Other;
	}
	FBox& operator+=( const FBox& Other )
	{
		if( IsValid && Other.IsValid )
		{
			Min.X = ::Min( Min.X, Other.Min.X );
			Min.Y = ::Min( Min.Y, Other.Min.Y );
			Min.Z = ::Min( Min.Z, Other.Min.Z );

			Max.X = ::Max( Max.X, Other.Max.X );
			Max.Y = ::Max( Max.Y, Other.Max.Y );
			Max.Z = ::Max( Max.Z, Other.Max.Z );
		}
		else *this = Other;
		return *this;
	}
	FBox operator+( const FBox& Other ) const
	{
		return FBox(*this) += Other;
	}
    FVector& operator[]( INT i )
	{
		check(i>-1);
		check(i<2);
		if( i == 0 )		return Min;
		else				return Max;
	}
	FBox TransformBy( const FMatrix& M ) const;
	FBox TransformBy( const FCoords& Coords ) const
	{
		FBox NewBox(0);
		for( int i=0; i<2; i++ )
			for( int j=0; j<2; j++ )
				for( int k=0; k<2; k++ )
					NewBox += FVector( GetExtrema(i).X, GetExtrema(j).Y, GetExtrema(k).Z ).TransformPointBy( Coords );
		return NewBox;
	}
	FBox ExpandBy( FLOAT W ) const
	{
		return FBox( Min - FVector(W,W,W), Max + FVector(W,W,W) );
	}
	// Returns the midpoint between the min and max points.
	FVector GetCenter() const
	{
		return FVector( ( Min + Max ) * 0.5f );
	}
	// Returns the extent around the center
	FVector GetExtent() const
	{
		return 0.5f*(Max - Min);
	}

	void GetCenterAndExtents( FVector & center, FVector & Extents ) const
	{
		Extents = Max - Min;
		Extents *= .5f;
		center = Min + Extents;
	}

	bool Intersect( const FBox & other ) const
	{
		if( Min.X > other.Max.X || other.Min.X > Max.X )
			return false;
		if( Min.Y > other.Max.Y || other.Min.Y > Max.Y )
			return false;
		if( Min.Z > other.Max.Z || other.Min.Z > Max.Z )
			return false;
		return true;
	}


	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FBox& Bound )
	{
		return Ar << Bound.Min << Bound.Max << Bound.IsValid;
	}
};

/*-----------------------------------------------------------------------------
	FInterpCurve.
-----------------------------------------------------------------------------*/
class CORE_API FInterpCurvePoint
{
public:
	FLOAT	InVal;
	FLOAT	OutVal;

	FInterpCurvePoint() {}
	FInterpCurvePoint(FLOAT I, FLOAT O) : InVal(I), OutVal(O) {}

	UBOOL operator==(const FInterpCurvePoint &Other)
	{
		return (InVal == Other.InVal && OutVal == Other.OutVal);
	}

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FInterpCurvePoint& Point )
	{
		return Ar << Point.InVal << Point.OutVal;
	}
};

class CORE_API FInterpCurve
{
public:
	TArrayNoInit<FInterpCurvePoint> Points;
	
	void	AddPoint( FLOAT inV, FLOAT outV );
	void	Reset( );
	FLOAT	Eval( FLOAT in);

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FInterpCurve& Curve )
	{
		return Ar << Curve.Points;
	}

	// Assignment (copy)
	void operator=(const FInterpCurve &Other)
	{
		Points = Other.Points;
	}
};

class CORE_API FInterpCurveInit : public FInterpCurve
{
public:
	FInterpCurveInit();
};

/*-----------------------------------------------------------------------------
	FGlobalMath.
-----------------------------------------------------------------------------*/

//
// Global mathematics info.
//
class CORE_API FGlobalMath
{
public:
	// Constants.
	enum {ANGLE_SHIFT 	= 2};		// Bits to right-shift to get lookup value.
	enum {ANGLE_BITS	= 14};		// Number of valid bits in angles.
	enum {NUM_ANGLES 	= 16384}; 	// Number of angles that are in lookup table.
	enum {NUM_SQRTS		= 16384};	// Number of square roots in lookup table.
	enum {ANGLE_MASK    =  (((1<<ANGLE_BITS)-1)<<(16-ANGLE_BITS))};

	// Class constants.
	const FVector  	WorldMin;
	const FVector  	WorldMax;
	const FCoords  	UnitCoords;
	const FScale   	UnitScale;
	const FCoords	ViewCoords;

	// Basic math functions.
	FLOAT Sqrt( int i )
	{
		return SqrtFLOAT[i]; 
	}
	FORCEINLINE FLOAT SinTab( int i )
	{
		return TrigFLOAT[((i>>ANGLE_SHIFT)&(NUM_ANGLES-1))];
	}
	FORCEINLINE FLOAT CosTab( int i )
	{
		return TrigFLOAT[(((i+16384)>>ANGLE_SHIFT)&(NUM_ANGLES-1))];
	}
	FLOAT SinFloat( FLOAT F )
	{
		return SinTab((INT)((F*65536.f)/(2.f*PI)));
	}
	FLOAT CosFloat( FLOAT F )
	{
		return CosTab((INT)((F*65536.f)/(2.f*PI)));
	}

	// Constructor.
	FGlobalMath();

private:
	// Tables.
	FLOAT  TrigFLOAT		[NUM_ANGLES];
	FLOAT  SqrtFLOAT		[NUM_SQRTS];
};

inline INT ReduceAngle( INT Angle )
{
	return Angle & FGlobalMath::ANGLE_MASK;
};

/*-----------------------------------------------------------------------------
	Floating point constants.
-----------------------------------------------------------------------------*/

//
// Lengths of normalized vectors (These are half their maximum values
// to assure that dot products with normalized vectors don't overflow).
//
#define FLOAT_NORMAL_THRESH				(0.0001f)

//
// Magic numbers for numerical precision.
//
#define THRESH_POINT_ON_PLANE			(0.10f)		/* Thickness of plane for front/back/inside test */
#define THRESH_POINT_ON_SIDE			(0.20f)		/* Thickness of polygon side's side-plane for point-inside/outside/on side test */
#define THRESH_POINTS_ARE_SAME			(0.002f)	/* Two points are same if within this distance */
#define THRESH_POINTS_ARE_NEAR			(0.015f)	/* Two points are near if within this distance and can be combined if imprecise math is ok */
#define THRESH_NORMALS_ARE_SAME			(0.00002f)	/* Two normal points are same if within this distance */
													/* Making this too large results in incorrect CSG classification and disaster */
#define THRESH_VECTORS_ARE_NEAR			(0.0004f)	/* Two vectors are near if within this distance and can be combined if imprecise math is ok */
													/* Making this too large results in lighting problems due to inaccurate texture coordinates */
#define THRESH_SPLIT_POLY_WITH_PLANE	(0.25f)		/* A plane splits a polygon in half */
#define THRESH_SPLIT_POLY_PRECISELY		(0.01f)		/* A plane exactly splits a polygon */
#define THRESH_ZERO_NORM_SQUARED		(0.0001f)	/* Size of a unit normal that is considered "zero", squared */
#define THRESH_VECTORS_ARE_PARALLEL		(0.02f)		/* Vectors are parallel if dot product varies less than this */


// gam ---

//
//	FVerticesEqual
//

inline UBOOL FVerticesEqual(const FVector& V1, const FVector& V2)
{
	if(Abs(V1.X - V2.X) > THRESH_POINTS_ARE_SAME * 4.0f)
		return 0;

	if(Abs(V1.Y - V2.Y) > THRESH_POINTS_ARE_SAME * 4.0f)
		return 0;

	if(Abs(V1.Z - V2.Z) > THRESH_POINTS_ARE_SAME * 4.0f)
		return 0;

	return 1;
}
// --- gam


/*-----------------------------------------------------------------------------
	FVector transformation.
-----------------------------------------------------------------------------*/

//
// Transformations in optimized assembler format.
// An adaption of Michael Abrash' optimal transformation code.
//
#if ASM
inline void ASMTransformPoint(const FCoords &Coords, const FVector &InVector, FVector &OutVector)
{
	// FCoords is a structure of 4 vectors: Origin, X, Y, Z
	//				 	  x  y  z
	// FVector	Origin;   0  4  8
	// FVector	XAxis;   12 16 20
	// FVector  YAxis;   24 28 32
	// FVector  ZAxis;   36 40 44
	//
	//	task:	Temp = ( InVector - Coords.Origin );
	//			Outvector.X = (Temp | Coords.XAxis);
	//			Outvector.Y = (Temp | Coords.YAxis);
	//			Outvector.Z = (Temp | Coords.ZAxis);
	//
	// About 33 cycles on a Pentium.
	//
	__asm
	{
		mov     esi,[InVector]
		mov     edx,[Coords]     
		mov     edi,[OutVector]

		// get source
		fld     dword ptr [esi+0]
		fld     dword ptr [esi+4]
		fld     dword ptr [esi+8] // z y x
		fxch    st(2)     // xyz

		// subtract origin
		fsub    dword ptr [edx + 0]  // xyz
		fxch    st(1)  
		fsub	dword ptr [edx + 4]  // yxz
		fxch    st(2)
		fsub	dword ptr [edx + 8]  // zxy
		fxch    st(1)        // X Z Y

		// triplicate X for  transforming
		fld     st(0)	// X X   Z Y
        fmul    dword ptr [edx+12]     // Xx X Z Y
        fld     st(1)   // X Xx X  Z Y 
        fmul    dword ptr [edx+24]   // Xy Xx X  Z Y 
		fxch    st(2)    
		fmul    dword ptr [edx+36]  // Xz Xx Xy  Z  Y 
		fxch    st(4)     // Y  Xx Xy  Z  Xz

		fld     st(0)			// Y Y    Xx Xy Z Xz
		fmul    dword ptr [edx+16]     
		fld     st(1) 			// Y Yx Y    Xx Xy Z Xz
        fmul    dword ptr [edx+28]    
		fxch    st(2)			// Y  Yx Yy   Xx Xy Z Xz
		fmul    dword ptr [edx+40]	 // Yz Yx Yy   Xx Xy Z Xz
		fxch    st(1)			// Yx Yz Yy   Xx Xy Z Xz

        faddp   st(3),st(0)	  // Yz Yy  XxYx   Xy Z  Xz
        faddp   st(5),st(0)   // Yy  XxYx   Xy Z  XzYz
        faddp   st(2),st(0)   // XxYx  XyYy Z  XzYz
		fxch    st(2)         // Z     XyYy XxYx XzYz

		fld     st(0)         //  Z  Z     XyYy XxYx XzYz
		fmul    dword ptr [edx+20]     
		fld     st(1)         //  Z  Zx Z  XyYy XxYx XzYz
        fmul    dword ptr [edx+32]      
		fxch    st(2)         //  Z  Zx Zy
		fmul    dword ptr [edx+44]	  //  Zz Zx Zy XyYy XxYx XzYz
		fxch    st(1)         //  Zx Zz Zy XyYy XxYx XzYz

		faddp   st(4),st(0)   //  Zz Zy XyYy  XxYxZx  XzYz
		faddp   st(4),st(0)	  //  Zy XyYy     XxYxZx  XzYzZz
		faddp   st(1),st(0)   //  XyYyZy      XxYxZx  XzYzZz
		fxch    st(1)		  //  Xx+Xx+Zx   Xy+Yy+Zy  Xz+Yz+Zz  

		fstp    dword ptr [edi+0]       
        fstp    dword ptr [edi+4]                               
        fstp    dword ptr [edi+8]     
	}
}
#elif ASMLINUX && !defined(UT_NO_X86_INTRIN)
inline void ASMTransformPoint(const FCoords &Coords, const FVector &InVector, FVector &OutVector)
{
	__asm__ __volatile__ (
		"# Get source.\n"
		"flds	0("UREG_ESI");			# x\n"
		"flds	4("UREG_ESI");			# y x\n"
		"flds	8("UREG_ESI");			# z y x\n"
		"fxch	%%st(2);\n"

		"# Subtract origin.\n"
		"fsubs	0(%1);\n"
		"fxch	%%st(1);\n"
		"fsubs	4(%1);\n"
		"fxch	%%st(2);\n"
		"fsubs	8(%1);\n"
		"fxch	%%st(1);\n"

		"# Triplicate X for transforming.\n"
		"fld		%%st(0);\n"
		"fmuls	12(%1);\n"
		"fld		%%st(1);\n"
		"fmuls	24(%1);\n"
		"fxch	%%st(2);\n"
		"fmuls	36(%1);\n"
		"fxch	%%st(4);\n"
		
		"fld		%%st(0);\n"
		"fmuls	16(%1);\n"
		"fld		%%st(1);\n"
		"fmuls	28(%1);\n"
		"fxch	%%st(2);\n"
		"fmuls	40(%1);\n"
		"fxch	%%st(1);\n"

		"faddp	%%st(0),%%st(3);\n"
		"faddp	%%st(0),%%st(5);\n"
		"faddp	%%st(0),%%st(2);\n"
		"fxch	%%st(2);\n"
		
		"fld		%%st(0);\n"
		"fmuls	20(%1);\n"
		"fld		%%st(1);\n"
		"fmuls	32(%1);\n"
		"fxch	%%st(2);\n"
		"fmuls	44(%1);\n"
		"fxch	%%st(1);\n"
		
		"faddp	%%st(0),%%st(4);\n"
		"faddp	%%st(0),%%st(4);\n"
		"faddp	%%st(0),%%st(1);\n"
		"fxch	%%st(1);\n"

		"fstps	0("UREG_EDI");\n"
		"fstps	4("UREG_EDI");\n"
		"fstps	8("UREG_EDI");\n"
	:
	:	"S" (&InVector),
		"q" (&Coords),
		"D" (&OutVector)
	: "memory"
	);
}
#endif

#if ASM
inline void ASMTransformVector(const FCoords &Coords, const FVector &InVector, FVector &OutVector)
{
	__asm
	{
		mov     esi,[InVector]
		mov     edx,[Coords]     
		mov     edi,[OutVector]

		// get source
		fld     dword ptr [esi+0]
		fld     dword ptr [esi+4]
		fxch    st(1)
		fld     dword ptr [esi+8] // z x y 
		fxch    st(1)             // x z y

		// triplicate X for  transforming
		fld     st(0)	// X X   Z Y
        fmul    dword ptr [edx+12]     // Xx X Z Y
        fld     st(1)   // X Xx X  Z Y 
        fmul    dword ptr [edx+24]   // Xy Xx X  Z Y 
		fxch    st(2)    
		fmul    dword ptr [edx+36]  // Xz Xx Xy  Z  Y 
		fxch    st(4)     // Y  Xx Xy  Z  Xz

		fld     st(0)			// Y Y    Xx Xy Z Xz
		fmul    dword ptr [edx+16]     
		fld     st(1) 			// Y Yx Y    Xx Xy Z Xz
        fmul    dword ptr [edx+28]    
		fxch    st(2)			// Y  Yx Yy   Xx Xy Z Xz
		fmul    dword ptr [edx+40]	 // Yz Yx Yy   Xx Xy Z Xz
		fxch    st(1)			// Yx Yz Yy   Xx Xy Z Xz

        faddp   st(3),st(0)	  // Yz Yy  XxYx   Xy Z  Xz
        faddp   st(5),st(0)   // Yy  XxYx   Xy Z  XzYz
        faddp   st(2),st(0)   // XxYx  XyYy Z  XzYz
		fxch    st(2)         // Z     XyYy XxYx XzYz

		fld     st(0)         //  Z  Z     XyYy XxYx XzYz
		fmul    dword ptr [edx+20]     
		fld     st(1)         //  Z  Zx Z  XyYy XxYx XzYz
        fmul    dword ptr [edx+32]      
		fxch    st(2)         //  Z  Zx Zy
		fmul    dword ptr [edx+44]	  //  Zz Zx Zy XyYy XxYx XzYz
		fxch    st(1)         //  Zx Zz Zy XyYy XxYx XzYz

		faddp   st(4),st(0)   //  Zz Zy XyYy  XxYxZx  XzYz
		faddp   st(4),st(0)	  //  Zy XyYy     XxYxZx  XzYzZz
		faddp   st(1),st(0)   //  XyYyZy      XxYxZx  XzYzZz
		fxch    st(1)		  //  Xx+Xx+Zx   Xy+Yy+Zy  Xz+Yz+Zz  

		fstp    dword ptr [edi+0]       
        fstp    dword ptr [edi+4]                               
        fstp    dword ptr [edi+8]     
	}
}
#endif

#if ASMLINUX && !defined(UT_NO_X86_INTRIN)
inline void ASMTransformVector(const FCoords &Coords, const FVector &InVector, FVector &OutVector)
{
	asm volatile(
		"# Get source.\n"
		"flds	0("UREG_ESI");\n"
		"flds	4("UREG_ESI");\n"
		"fxch	%%st(1);\n"
		"flds	8("UREG_ESI");\n"
		"fxch	%%st(1);\n"

		"# Triplicate X for transforming.\n"
		"fld		%%st(0);\n"
		"fmuls	12(%1);\n"
		"fld		%%st(1);\n"
		"fmuls	24(%1);\n"
		"fxch	%%st(2);\n"
		"fmuls	36(%1);\n"
		"fxch	%%st(4);\n"

		"fld		%%st(0);\n"
		"fmuls	16(%1);\n"
		"fld		%%st(1);\n"
		"fmuls	28(%1);\n"
		"fxch	%%st(2);\n"
		"fmuls	40(%1);\n"
		"fxch	%%st(1);\n"

		"faddp	%%st(0),%%st(3);\n"
		"faddp	%%st(0),%%st(5);\n"
		"faddp	%%st(0),%%st(2);\n"
		"fxch	%%st(2);\n"

		"fld		%%st(0);\n"
		"fmuls	20(%1);\n"
		"fld		%%st(1);\n"
		"fmuls	32(%1);\n"
		"fxch	%%st(2);\n"
		"fmuls	44(%1);\n"
		"fxch	%%st(1);\n"

		"faddp	%%st(0),%%st(4);\n"
		"faddp	%%st(0),%%st(4);\n"
		"faddp	%%st(0),%%st(1);\n"
		"fxch	%%st(1);\n"

		"fstps	0("UREG_EDI");\n"
		"fstps	4("UREG_EDI");\n"
		"fstps	8("UREG_EDI");\n"
	:
	: "S" (&InVector),
	  "q" (&Coords),
	  "D" (&OutVector)
	: "memory"
	);
}
#endif

//
// Transform a point by a coordinate system, moving
// it by the coordinate system's origin if nonzero.
//
inline FVector FVector::TransformPointBy( const FCoords &Coords ) const
{
#if ASM
	FVector Temp;
	ASMTransformPoint( Coords, *this, Temp);
	return Temp;
#elif 0 //ASMLINUX
	static FVector Temp;
	ASMTransformPoint( Coords, *this, Temp);
	return Temp;
#else
	FVector Temp = *this - Coords.Origin;
	return FVector(	Temp | Coords.XAxis, Temp | Coords.YAxis, Temp | Coords.ZAxis );
#endif
}

//
// Transform a directional vector by a coordinate system.
// Ignore's the coordinate system's origin.
//
inline FVector FVector::TransformVectorBy( const FCoords &Coords ) const
{
#if ASM
	FVector Temp;
	ASMTransformVector( Coords, *this, Temp);
	return Temp;
#elif 0 //ASMLINUX   // (apparently broken. --ryan.)
	FVector Temp;
	ASMTransformVector( Coords, *this, Temp);
	return Temp;
#else
	return FVector(	*this | Coords.XAxis, *this | Coords.YAxis, *this | Coords.ZAxis );
#endif
}

inline FVector FVector::TransformVectorByTranspose( const FCoords &Coords) const
{
	return FVector
	(
		X * Coords.XAxis.X + Y * Coords.YAxis.X + Z * Coords.ZAxis.X,
		X * Coords.XAxis.Y + Y * Coords.YAxis.Y + Z * Coords.ZAxis.Y,
		X * Coords.XAxis.Z + Y * Coords.YAxis.Z + Z * Coords.ZAxis.Z
	);
}


// Apply 'pivot' transform: First rotate, then add the translation.
// TODO: convert to assembly !
inline FVector FVector::PivotTransform(const FCoords& Coords) const
{
	return Coords.Origin + FVector( *this | Coords.XAxis, *this | Coords.YAxis, *this | Coords.ZAxis );
}

//
// Mirror a vector about a normal vector.
//
inline FVector FVector::MirrorByVector( const FVector& MirrorNormal ) const
{
	return *this - MirrorNormal * (2.f * (*this | MirrorNormal));
}

//
// Mirror a vector about a plane.
//
inline FVector FVector::MirrorByPlane( const FPlane& Plane ) const
{
	return *this - Plane * (2.f * Plane.PlaneDot(*this) );
}

//
// Rotate around Axis (assumes Axis.Size() == 1)
//
inline FVector FVector::RotateAngleAxis( const INT Angle, const FVector& Axis ) const
{
	FLOAT S		= GMath.SinTab(Angle);
	FLOAT C		= GMath.CosTab(Angle);

	FLOAT XX	= Axis.X * Axis.X;
	FLOAT YY	= Axis.Y * Axis.Y;
	FLOAT ZZ	= Axis.Z * Axis.Z;

	FLOAT XY	= Axis.X * Axis.Y;
	FLOAT YZ	= Axis.Y * Axis.Z;
	FLOAT ZX	= Axis.Z * Axis.X;

	FLOAT XS	= Axis.X * S;
	FLOAT YS	= Axis.Y * S;
	FLOAT ZS	= Axis.Z * S;

	FLOAT OMC	= 1.f - C;

	return FVector(
		(OMC * XX + C ) * X + (OMC * XY - ZS) * Y + (OMC * ZX + YS) * Z,
		(OMC * XY + ZS) * X + (OMC * YY + C ) * Y + (OMC * YZ - XS) * Z,
		(OMC * ZX - YS) * X + (OMC * YZ + XS) * Y + (OMC * ZZ + C ) * Z
		);
}


/*-----------------------------------------------------------------------------
	FVector friends.
-----------------------------------------------------------------------------*/

//
// Compare two points and see if they're the same, using a threshold.
// Returns 1=yes, 0=no.  Uses fast distance approximation.
//
inline int FPointsAreSame( const FVector &P, const FVector &Q )
{
	FLOAT Temp;
	Temp=P.X-Q.X;
	if( (Temp > -THRESH_POINTS_ARE_SAME) && (Temp < THRESH_POINTS_ARE_SAME) )
	{
		Temp=P.Y-Q.Y;
		if( (Temp > -THRESH_POINTS_ARE_SAME) && (Temp < THRESH_POINTS_ARE_SAME) )
		{
			Temp=P.Z-Q.Z;
			if( (Temp > -THRESH_POINTS_ARE_SAME) && (Temp < THRESH_POINTS_ARE_SAME) )
			{
				return 1;
			}
		}
	}
	return 0;
}

//
// Compare two points and see if they're the same, using a threshold.
// Returns 1=yes, 0=no.  Uses fast distance approximation.
//
inline int FPointsAreNear( const FVector &Point1, const FVector &Point2, FLOAT Dist )
{
	FLOAT Temp;
	Temp=(Point1.X - Point2.X); if (Abs(Temp)>=Dist) return 0;
	Temp=(Point1.Y - Point2.Y); if (Abs(Temp)>=Dist) return 0;
	Temp=(Point1.Z - Point2.Z); if (Abs(Temp)>=Dist) return 0;
	return 1;
}

//
// Calculate the signed distance (in the direction of the normal) between
// a point and a plane.
//
inline FLOAT FPointPlaneDist
(
	const FVector &Point,
	const FVector &PlaneBase,
	const FVector &PlaneNormal
)
{
	return (Point - PlaneBase) | PlaneNormal;
}

//
// Euclidean distance between two points.
//
inline FLOAT FDist( const FVector &V1, const FVector &V2 )
{
	return appSqrt( Square(V2.X-V1.X) + Square(V2.Y-V1.Y) + Square(V2.Z-V1.Z) );
}

//
// Squared distance between two points.
//
inline FLOAT FDistSquared( const FVector &V1, const FVector &V2 )
{
	return Square(V2.X-V1.X) + Square(V2.Y-V1.Y) + Square(V2.Z-V1.Z);
}

//
// See if two normal vectors (or plane normals) are nearly parallel.
//
inline int FParallel( const FVector &Normal1, const FVector &Normal2 )
{
	FLOAT NormalDot = Normal1 | Normal2;
	return (Abs (NormalDot - 1.f) <= THRESH_VECTORS_ARE_PARALLEL);
}

//
// See if two planes are coplanar.
//
inline int FCoplanar( const FVector &Base1, const FVector &Normal1, const FVector &Base2, const FVector &Normal2 )
{
	if      (!FParallel(Normal1,Normal2)) return 0;
	else if (FPointPlaneDist (Base2,Base1,Normal1) > THRESH_POINT_ON_PLANE) return 0;
	else    return 1;
}

//
// Triple product of three vectors.
//
inline FLOAT FTriple( const FVector& X, const FVector& Y, const FVector& Z )
{
	return
	(	(X.X * (Y.Y * Z.Z - Y.Z * Z.Y))
	+	(X.Y * (Y.Z * Z.X - Y.X * Z.Z))
	+	(X.Z * (Y.X * Z.Y - Y.Y * Z.X)) );
}

/*-----------------------------------------------------------------------------
	FCoords functions.
-----------------------------------------------------------------------------*/

//
// Return this coordinate system's transpose.
// If the coordinate system is orthogonal, this is equivalent to its inverse.
//
inline FCoords FCoords::Transpose() const
{
	return FCoords
	(
		-Origin.TransformVectorBy(*this),
		FVector( XAxis.X, YAxis.X, ZAxis.X ),
		FVector( XAxis.Y, YAxis.Y, ZAxis.Y ),
		FVector( XAxis.Z, YAxis.Z, ZAxis.Z )
	);
}

//
// Mirror the coordinates about a normal vector.
//
inline FCoords FCoords::MirrorByVector( const FVector& MirrorNormal ) const
{
	return FCoords
	(
		Origin.MirrorByVector( MirrorNormal ),
		XAxis .MirrorByVector( MirrorNormal ),
		YAxis .MirrorByVector( MirrorNormal ),
		ZAxis .MirrorByVector( MirrorNormal )
	);
}

//
// Mirror the coordinates about a plane.
//
inline FCoords FCoords::MirrorByPlane( const FPlane& Plane ) const
{
	return FCoords
	(
		Origin.MirrorByPlane ( Plane ),
		XAxis .MirrorByVector( Plane ),
		YAxis .MirrorByVector( Plane ),
		ZAxis .MirrorByVector( Plane )
	);
}

/*-----------------------------------------------------------------------------
	FCoords operators.
-----------------------------------------------------------------------------*/

//
// Transform this coordinate system by another coordinate system.
//
inline FCoords& FCoords::operator*=( const FCoords& TransformCoords )
{
	//!! Proper solution:
	//Origin = Origin.TransformPointBy( TransformCoords.Inverse().Transpose() );
	// Fast solution assuming orthogonal coordinate system:
	Origin = Origin.TransformPointBy ( TransformCoords );
	XAxis  = XAxis .TransformVectorBy( TransformCoords );
	YAxis  = YAxis .TransformVectorBy( TransformCoords );
	ZAxis  = ZAxis .TransformVectorBy( TransformCoords );
	return *this;
}
inline FCoords FCoords::operator*( const FCoords &TransformCoords ) const
{
	return FCoords(*this) *= TransformCoords;
}

//
// Transform this coordinate system by a pitch-yaw-roll rotation.
//
inline FCoords& FCoords::operator*=( const FRotator &Rot )
{
	// Apply yaw rotation.
	*this *= FCoords
	(	
		FVector( 0.f, 0.f, 0.f ),
		FVector( +GMath.CosTab(Rot.Yaw), +GMath.SinTab(Rot.Yaw), +0.f ),
		FVector( -GMath.SinTab(Rot.Yaw), +GMath.CosTab(Rot.Yaw), +0.f ),
		FVector( +0.f, +0.f, +1.f )
	);

	// Apply pitch rotation.
	*this *= FCoords
	(	
		FVector( 0.f, 0.f, 0.f ),
		FVector( +GMath.CosTab(Rot.Pitch), +0.f, +GMath.SinTab(Rot.Pitch) ),
		FVector( +0.f, +1.f, +0.f ),
		FVector( -GMath.SinTab(Rot.Pitch), +0.f, +GMath.CosTab(Rot.Pitch) )
	);

	// Apply roll rotation.
	*this *= FCoords
	(	
		FVector( 0.f, 0.f, 0.f ),
		FVector( +1.f, +0.f, +0.f ),
		FVector( +0.f, +GMath.CosTab(Rot.Roll), -GMath.SinTab(Rot.Roll) ),
		FVector( +0.f, +GMath.SinTab(Rot.Roll), +GMath.CosTab(Rot.Roll) )
	);
	return *this;
}
inline FCoords FCoords::operator*( const FRotator &Rot ) const
{
	return FCoords(*this) *= Rot;
}

inline FCoords& FCoords::operator*=( const FVector &Point )
{
	Origin -= Point;
	return *this;
}
inline FCoords FCoords::operator*( const FVector &Point ) const
{
	return FCoords(*this) *= Point;
}

//
// Detransform this coordinate system by a pitch-yaw-roll rotation.
//
inline FCoords& FCoords::operator/=( const FRotator &Rot )
{
	// Apply inverse roll rotation.
	*this *= FCoords
	(
		FVector( 0.f, 0.f, 0.f ),
		FVector( +1.f, -0.f, +0.f ),
		FVector( -0.f, +GMath.CosTab(Rot.Roll), +GMath.SinTab(Rot.Roll) ),
		FVector( +0.f, -GMath.SinTab(Rot.Roll), +GMath.CosTab(Rot.Roll) )
	);

	// Apply inverse pitch rotation.
	*this *= FCoords
	(
		FVector( 0.f, 0.f, 0.f ),
		FVector( +GMath.CosTab(Rot.Pitch), +0.f, -GMath.SinTab(Rot.Pitch) ),
		FVector( +0.f, +1.f, -0.f ),
		FVector( +GMath.SinTab(Rot.Pitch), +0.f, +GMath.CosTab(Rot.Pitch) )
	);

	// Apply inverse yaw rotation.
	*this *= FCoords
	(
		FVector( 0.f, 0.f, 0.f ),
		FVector( +GMath.CosTab(Rot.Yaw), -GMath.SinTab(Rot.Yaw), -0.f ),
		FVector( +GMath.SinTab(Rot.Yaw), +GMath.CosTab(Rot.Yaw), +0.f ),
		FVector( -0.f, +0.f, +1.f )
	);
	return *this;
}
inline FCoords FCoords::operator/( const FRotator &Rot ) const
{
	return FCoords(*this) /= Rot;
}

inline FCoords& FCoords::operator/=( const FVector &Point )
{
	Origin += Point;
	return *this;
}
inline FCoords FCoords::operator/( const FVector &Point ) const
{
	return FCoords(*this) /= Point;
}

//
// Transform this coordinate system by a scale.
// Note: Will return coordinate system of opposite handedness if
// Scale.X*Scale.Y*Scale.Z is negative.
//
inline FCoords& FCoords::operator*=( const FScale &Scale )
{
	// Apply sheering.
	FLOAT   Sheer      = FSheerSnap( Scale.SheerRate );
	FCoords TempCoords = GMath.UnitCoords;
	switch( Scale.SheerAxis )
	{
		case SHEER_XY:
			TempCoords.XAxis.Y = Sheer;
			break;
		case SHEER_XZ:
			TempCoords.XAxis.Z = Sheer;
			break;
		case SHEER_YX:
			TempCoords.YAxis.X = Sheer;
			break;
		case SHEER_YZ:
			TempCoords.YAxis.Z = Sheer;
			break;
		case SHEER_ZX:
			TempCoords.ZAxis.X = Sheer;
			break;
		case SHEER_ZY:
			TempCoords.ZAxis.Y = Sheer;
			break;
		default:
			break;
	}
	*this *= TempCoords;

	// Apply scaling.
	XAxis    *= Scale.Scale;
	YAxis    *= Scale.Scale;
	ZAxis    *= Scale.Scale;
	Origin.X /= Scale.Scale.X;
	Origin.Y /= Scale.Scale.Y;
	Origin.Z /= Scale.Scale.Z;

	return *this;
}
inline FCoords FCoords::operator*( const FScale &Scale ) const
{
	return FCoords(*this) *= Scale;
}

//
// Detransform a coordinate system by a scale.
//
inline FCoords& FCoords::operator/=( const FScale &Scale )
{
	// Deapply scaling.
	XAxis    /= Scale.Scale;
	YAxis    /= Scale.Scale;
	ZAxis    /= Scale.Scale;
	Origin.X *= Scale.Scale.X;
	Origin.Y *= Scale.Scale.Y;
	Origin.Z *= Scale.Scale.Z;

	// Deapply sheering.
	FCoords TempCoords(GMath.UnitCoords);
	FLOAT Sheer = FSheerSnap( Scale.SheerRate );
	switch( Scale.SheerAxis )
	{
		case SHEER_XY:
			TempCoords.XAxis.Y = -Sheer;
			break;
		case SHEER_XZ:
			TempCoords.XAxis.Z = -Sheer;
			break;
		case SHEER_YX:
			TempCoords.YAxis.X = -Sheer;
			break;
		case SHEER_YZ:
			TempCoords.YAxis.Z = -Sheer;
			break;
		case SHEER_ZX:
			TempCoords.ZAxis.X = -Sheer;
			break;
		case SHEER_ZY:
			TempCoords.ZAxis.Y = -Sheer;
			break;
		default: // SHEER_NONE
			break;
	}
	*this *= TempCoords;

	return *this;
}
inline FCoords FCoords::operator/( const FScale &Scale ) const
{
	return FCoords(*this) /= Scale;
}

/*-----------------------------------------------------------------------------
	Random numbers.
-----------------------------------------------------------------------------*/

//
// Compute pushout of a box from a plane.
//
FORCEINLINE FLOAT FBoxPushOut( const FVector & Normal, const FVector & Size )
{
    return fabs(Normal.X*Size.X) + fabs(Normal.Y*Size.Y) + fabs(Normal.Z*Size.Z);
}

//
// Return a uniformly distributed random unit vector.
//
inline FVector VRand()
{
	FVector Result;
	do
	{
		// Check random vectors in the unit sphere so result is statistically uniform.
		Result.X = appFrand()*2 - 1;
		Result.Y = appFrand()*2 - 1;
		Result.Z = appFrand()*2 - 1;
	} while( Result.SizeSquared() > 1.f );
	return Result.UnsafeNormal();
}

/*-----------------------------------------------------------------------------
	Texturing.
-----------------------------------------------------------------------------*/


// Returns the UV texture coordinates for the specified vertex.
inline void FVectorsToTexCoords( FVector InVtx, FVector InPolyBase, FVector InTextureU, FVector InTextureV, FLOAT InMaterialUSize, FLOAT InMaterialVSize, FLOAT* InU, FLOAT* InV )
{
	*InU = ((InVtx - InPolyBase) | InTextureU) / InMaterialUSize;
	*InV = ((InVtx - InPolyBase) | InTextureV) / InMaterialVSize;
}

// Accepts a triangle (XYZ and UV values for each point) and returns a poly base and UV vectors
// NOTE : the UV coords should be scaled by the texture size
inline void FTexCoordsToVectors( FVector V0, FVector UV0, FVector V1, FVector UV1, FVector V2, FVector UV2, FVector* InBaseResult, FVector* InUResult, FVector* InVResult )
{
	guard(FTexCoordsToVectors);

	// Create polygon normal.
	FVector PN = FVector((V0-V1) ^ (V2-V0));
	PN = PN.SafeNormal();

	// Fudge UV's to make sure no infinities creep into UV vector math, whenever we detect identical U or V's.
	if( ( UV0.X == UV1.X ) || ( UV2.X == UV1.X ) || ( UV2.X == UV0.X ) ||
		( UV0.Y == UV1.Y ) || ( UV2.Y == UV1.Y ) || ( UV2.Y == UV0.Y ) )
	{
		UV1 += FVector(0.004173f,0.004123f,0.0f);
		UV2 += FVector(0.003173f,0.003123f,0.0f);
	}

	//
	// Solve the equations to find our texture U/V vectors 'TU' and 'TV' by stacking them 
	// into a 3x3 matrix , one for  u(t) = TU dot (x(t)-x(o) + u(o) and one for v(t)=  TV dot (.... , 
	// then the third assumes we're perpendicular to the normal. 
	//
	FCoords TexEqu; 
	TexEqu.XAxis = FVector(	V1.X - V0.X, V1.Y - V0.Y, V1.Z - V0.Z );
	TexEqu.YAxis = FVector( V2.X - V0.X, V2.Y - V0.Y, V2.Z - V0.Z );
	TexEqu.ZAxis = FVector( PN.X,        PN.Y,        PN.Z        );
	TexEqu.Origin =FVector( 0.0f, 0.0f, 0.0f );
	TexEqu = TexEqu.Inverse();

	FVector UResult( UV1.X-UV0.X, UV2.X-UV0.X, 0.0f );
	FVector TUResult = UResult.TransformVectorBy( TexEqu );

	FVector VResult( UV1.Y-UV0.Y, UV2.Y-UV0.Y, 0.0f );
	FVector TVResult = VResult.TransformVectorBy( TexEqu );

	//
	// Adjust the BASE to account for U0 and V0 automatically, and force it into the same plane.
	//				
	FCoords BaseEqu;
	BaseEqu.XAxis = TUResult;
	BaseEqu.YAxis = TVResult; 
	BaseEqu.ZAxis = FVector( PN.X, PN.Y, PN.Z );
	BaseEqu.Origin = FVector( 0.0f, 0.0f, 0.0f );

	FVector BResult = FVector( UV0.X - ( TUResult|V0 ), UV0.Y - ( TVResult|V0 ),  0.0f );

	*InBaseResult = - 1.0f *  BResult.TransformVectorBy( BaseEqu.Inverse() );
	*InUResult = TUResult;
	*InVResult = TVResult;

	unguard;
}

/*
	FProjectTextureToPlane
	Projects a texture coordinate system onto a plane.
*/

inline void FProjectTextureToPlane(FVector& Base,FVector& X,FVector& Y,FPlane Plane)
{
	guard(FTexCoordsProjectToPlane);

	// Calculate a vector perpendicular to the texture(the texture normal).
	// Moving the texture base along this vector doesn't affect texture mapping.

	FVector	Z = (X ^ Y).SafeNormal();

	// Calculate the ratio of distance along the plane normal to distance along the texture normal.

	FLOAT	Ratio = 1.0f / (Z | Plane);

	// Project each component of the texture coordinate system onto the plane.

	Base = Base - Z * Plane.PlaneDot(Base) * Ratio;
	X = X - Z * (X | Plane) * Ratio;
	Y = Y - Z * (Y | Plane) * Ratio;

	unguard;
}

/*-----------------------------------------------------------------------------
	Advanced geometry.
-----------------------------------------------------------------------------*/

//
// Find the intersection of an infinite line (defined by two points) and
// a plane.  Assumes that the line and plane do indeed intersect; you must
// make sure they're not parallel before calling.
//
inline FVector FLinePlaneIntersection
(
	const FVector &Point1,
	const FVector &Point2,
	const FVector &PlaneOrigin,
	const FVector &PlaneNormal
)
{
	return
		Point1
	+	(Point2-Point1)
	*	(((PlaneOrigin - Point1)|PlaneNormal) / ((Point2 - Point1)|PlaneNormal));
}
inline FVector FLinePlaneIntersection
(
	const FVector &Point1,
	const FVector &Point2,
	const FPlane  &Plane
)
{
	return
		Point1
	+	(Point2-Point1)
	*	((Plane.W - (Point1|Plane))/((Point2 - Point1)|Plane));
}

//
// Determines whether a point is inside a box.
//

inline UBOOL FPointBoxIntersection
(
	const FVector&	Point,
	const FBox&		Box
)
{
	if(Point.X >= Box.Min.X && Point.X <= Box.Max.X &&
	   Point.Y >= Box.Min.Y && Point.Y <= Box.Max.Y &&
	   Point.Z >= Box.Min.Z && Point.Z <= Box.Max.Z)
		return 1;
	else
		return 0;
}

//
// Determines whether a line intersects a box.
//

#define BOX_SIDE_THRESHOLD	0.1f

inline UBOOL FLineBoxIntersection
(
	const FBox&		Box,
	const FVector&	Start,
	const FVector&	End,
	const FVector&	Direction,
	const FVector&	OneOverDirection
)
{
	FVector	Time;
	UBOOL	Inside = 1;

	if(Start.X < Box.Min.X)
	{
		if(Direction.X <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.X = (Box.Min.X - Start.X) * OneOverDirection.X;
		}
	}
	else if(Start.X > Box.Max.X)
	{
		if(Direction.X >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.X = (Box.Max.X - Start.X) * OneOverDirection.X;
		}
	}
	else
		Time.X = 0.0f;

	if(Start.Y < Box.Min.Y)
	{
		if(Direction.Y <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Y = (Box.Min.Y - Start.Y) * OneOverDirection.Y;
		}
	}
	else if(Start.Y > Box.Max.Y)
	{
		if(Direction.Y >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Y = (Box.Max.Y - Start.Y) * OneOverDirection.Y;
		}
	}
	else
		Time.Y = 0.0f;

	if(Start.Z < Box.Min.Z)
	{
		if(Direction.Z <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Z = (Box.Min.Z - Start.Z) * OneOverDirection.Z;
		}
	}
	else if(Start.Z > Box.Max.Z)
	{
		if(Direction.Z >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Z = (Box.Max.Z - Start.Z) * OneOverDirection.Z;
		}
	}
	else
		Time.Z = 0.0f;

	if(Inside)
		return 1;
	else
	{
		FLOAT	MaxTime = Max(Time.X,Max(Time.Y,Time.Z));

		if(MaxTime >= 0.0f && MaxTime <= 1.0f)
		{
			FVector	Hit = Start + Direction * MaxTime;

			if(	Hit.X > Box.Min.X - BOX_SIDE_THRESHOLD && Hit.X < Box.Max.X + BOX_SIDE_THRESHOLD &&
				Hit.Y > Box.Min.Y - BOX_SIDE_THRESHOLD && Hit.Y < Box.Max.Y + BOX_SIDE_THRESHOLD &&
				Hit.Z > Box.Min.Z - BOX_SIDE_THRESHOLD && Hit.Z < Box.Max.Z + BOX_SIDE_THRESHOLD)
				return 1;
		}

		return 0;
	}
}

CORE_API UBOOL FLineExtentBoxIntersection(const FBox& inBox, 
								 const FVector& Start, 
								 const FVector& End,
								 const FVector& Extent,
								 FVector& HitLocation,
								 FVector& HitNormal,
								 FLOAT& HitTime);

//
// Determines whether a line intersects a sphere.
//

inline UBOOL FLineSphereIntersection(FVector Start,FVector Dir,FLOAT Length,FVector Origin,FLOAT Radius)
{
	FVector	EO = Start - Origin;
	FLOAT	v = (Dir | (Origin - Start)),
			disc = Radius * Radius - ((EO | EO) - v * v);

	if(disc >= 0.0f)
	{
		FLOAT	Time = (v - appSqrt(disc)) / Length;

		if(Time >= 0.0f && Time <= 1.0f)
			return 1;
		else
			return 0;
	}
	else
		return 0;
}

/*-----------------------------------------------------------------------------
	FPlane functions.
-----------------------------------------------------------------------------*/

//
// Compute intersection point of three planes.
// Return 1 if valid, 0 if infinite.
//
inline UBOOL FIntersectPlanes3( FVector& I, const FPlane& P1, const FPlane& P2, const FPlane& P3 )
{
	guard(FIntersectPlanes3);

	// Compute determinant, the triple product P1|(P2^P3)==(P1^P2)|P3.
	FLOAT Det = (P1 ^ P2) | P3;
	if( Square(Det) < Square(0.001f) )
	{
		// Degenerate.
		I = FVector(0,0,0);
		return 0;
	}
	else
	{
		// Compute the intersection point, guaranteed valid if determinant is nonzero.
		I = (P1.W*(P2^P3) + P2.W*(P3^P1) + P3.W*(P1^P2)) / Det;
	}
	return 1;
	unguard;
}

//
// Compute intersection point and direction of line joining two planes.
// Return 1 if valid, 0 if infinite.
//
inline UBOOL FIntersectPlanes2( FVector& I, FVector& D, const FPlane& P1, const FPlane& P2 )
{
	guard(FIntersectPlanes2);

	// Compute line direction, perpendicular to both plane normals.
	D = P1 ^ P2;
	FLOAT DD = D.SizeSquared();
	if( DD < Square(0.001f) )
	{
		// Parallel or nearly parallel planes.
		D = I = FVector(0,0,0);
		return 0;
	}
	else
	{
		// Compute intersection.
		I = (P1.W*(P2^D) + P2.W*(D^P1)) / DD;
		D.Normalize();
		return 1;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	FRotator functions.
-----------------------------------------------------------------------------*/

//
// Convert a rotation into a vector facing in its direction.
//
inline FVector FRotator::Vector()
{
	return (GMath.UnitCoords / *this).XAxis;
}

/*-----------------------------------------------------------------------------
	FQuat.          
-----------------------------------------------------------------------------*/

// floating point quaternion.
class CORE_API FQuat 
{
	public:
	// Variables.
	FLOAT X,Y,Z,W;
	// X,Y,Z, W also doubles as the Axis/Angle format.

	// Constructors.
	FQuat()
	{}

	FQuat( FLOAT InX, FLOAT InY, FLOAT InZ, FLOAT InA )
	:	X(InX), Y(InY), Z(InZ), W(InA)
	{}

	// Binary operators.
	FQuat operator+( const FQuat& Q ) const
	{
		return FQuat( X + Q.X, Y + Q.Y, Z + Q.Z, W + Q.W );
	}

	FQuat operator-( const FQuat& Q ) const
	{
		return FQuat( X - Q.X, Y - Q.Y, Z - Q.Z, W - Q.W );
	}

	FQuat operator*( const FQuat& Q ) const
	{
		return FQuat(
			W*Q.X + X*Q.W + Y*Q.Z - Z*Q.Y,
			W*Q.Y - X*Q.Z + Y*Q.W + Z*Q.X,
			W*Q.Z + X*Q.Y - Y*Q.X + Z*Q.W,
			W*Q.W - X*Q.X - Y*Q.Y - Z*Q.Z
			);
	}

	FQuat operator*( const FLOAT& Scale ) const
	{
		return FQuat( Scale*X, Scale*Y, Scale*Z, Scale*W);			
	}
	
	// Unary operators.
	FQuat operator-() const
	{
		return FQuat( X, Y, Z, -W );
	}

    // Misc operators
	UBOOL operator!=( const FQuat& Q ) const
	{
		return X!=Q.X || Y!=Q.Y || Z!=Q.Z || W!=Q.W;
	}
	
	UBOOL Normalize()
	{
		// 
		FLOAT SquareSum = (FLOAT)(X*X+Y*Y+Z*Z+W*W);
		if( SquareSum >= DELTA )
		{
			FLOAT Scale = 1.0f/(FLOAT)appSqrt(SquareSum);
			X *= Scale; 
			Y *= Scale; 
			Z *= Scale;
			W *= Scale;
			return true;
		}
		else 
		{	
			X = 0.0f;
			Y = 0.0f;
			Z = 0.1f;
			W = 0.0f;
			return false;
		}
	}

	// Serializer.
	friend FArchive& operator<<( FArchive& Ar, FQuat& F )
	{
		return Ar << F.X << F.Y << F.Z << F.W;
	}

	//
	// Quaternion to Angle-Axis. Assumes a normalized quaternion as input.
	//
	FQuat FQuatToAngAxis()
	{
		FLOAT scale = (FLOAT)appSqrt(1.0f-W*W);
		FQuat A;

		if (scale >= DELTA)
		{
			A.X = X / scale;
			A.Y = Y / scale;
			A.Z = Z / scale;
			A.W = 2.0f * (FLOAT)appAcos (W);
		}
		else 
		{
			A.X = 0.0f;
			A.Y = 0.0f;
			A.Z = 1.0f;
			A.W = 0.0f; 
		}

		return A;
	};

	//
	// Angle-Axis to Quaternion. Assumes normalized rotation axis as input.
	//
	FQuat AngAxisToFQuat()
	{
		FLOAT scale = (FLOAT)appSin(W * 0.5f);
		FQuat Q;

		Q.X = X * scale;
		Q.Y = Y * scale;
		Q.Z = Z * scale;
		Q.W = (FLOAT)appCos(W * 0.5f);

		return Q;
	}

	FVector RotateVector(FVector v)
	{	
		// (q.W*q.W-qv.qv)v + 2(qv.v)qv + 2 q.W (qv x v)

		FVector qv(X, Y, Z);
		FVector vOut = 2.f * W * (qv ^ v);
		vOut += ((W * W) - (qv | qv)) * v;
		vOut += (2.f * (qv | v)) * qv;

		return vOut;
	}

};

// Generate the 'smallest' (geodesic) rotation between these two vectors.
CORE_API FQuat FQuatFindBetween(const FVector& vec1, const FVector& vec2);

// Dot product of axes to get cos of angle  #Warning some people use .W component here too !
inline FLOAT FQuatDot(const FQuat& Q1,const FQuat& Q2)
{
	return( Q1.X*Q2.X + Q1.Y*Q2.Y + Q1.Z*Q2.Z );
};

// Error measure (angle) between two quaternions, ranged [0..1]
inline FLOAT FQuatError(FQuat& Q1,FQuat& Q2)
{
	// Returns the hypersphere-angle between two quaternions; alignment shouldn't matter, though 
	// normalized input is expected.
	FLOAT cosom = Q1.X*Q2.X + Q1.Y*Q2.Y + Q1.Z*Q2.Z + Q1.W*Q2.W;
	return (Abs(cosom) < 0.9999999f) ? appAcos(cosom)*(1.f/PI) : 0.0f;
}

// Ensure quat1 points to same side of the hypersphere as quat2
inline void AlignFQuatWith(FQuat &quat1, const FQuat &quat2)
{
	FLOAT Minus  = Square(quat1.X-quat2.X) + Square(quat1.Y-quat2.Y) + Square(quat1.Z-quat2.Z) + Square(quat1.W-quat2.W);
	FLOAT Plus   = Square(quat1.X+quat2.X) + Square(quat1.Y+quat2.Y) + Square(quat1.Z+quat2.Z) + Square(quat1.W+quat2.W);

	if (Minus > Plus)
	{
		quat1.X = - quat1.X;
		quat1.Y = - quat1.Y;
		quat1.Z = - quat1.Z;
		quat1.W = - quat1.W;
	}
}

// Spherical interpolation. Will correct alignment. Output is not normalized.
inline FQuat SlerpQuat(const FQuat &quat1,const FQuat &quat2, float slerp)
{
	FLOAT Cosom,RawCosom,scale0,scale1;

	// Get cosine of angle between quats.
	Cosom = RawCosom = 
		       quat1.X * quat2.X +
			quat1.Y * quat2.Y +
			quat1.Z * quat2.Z +
			quat1.W * quat2.W;

	// Unaligned quats - compensate, results in taking shorter route.
	if( RawCosom < 0.0f )  
	{
		Cosom = -RawCosom;
	}
	
	if( Cosom < 0.9999f )
	{	
		FLOAT omega = appAcos(Cosom);
		FLOAT sininv = 1.f/appSin(omega);
		scale0 = appSin((1.f - slerp) * omega) * sininv;
		scale1 = appSin(slerp * omega) * sininv;
	}
	else
	{
		// Use linear interpolation.
		scale0 = 1.0f - slerp;
		scale1 = slerp;	
	}
	// In keeping with our flipped Cosom:
	if( RawCosom < 0.0f)
	{		
		scale1= -scale1;
	}

	FQuat result;
		
	result.X = scale0 * quat1.X + scale1 * quat2.X;
	result.Y = scale0 * quat1.Y + scale1 * quat2.Y;
	result.Z = scale0 * quat1.Z + scale1 * quat2.Z;
	result.W = scale0 * quat1.W + scale1 * quat2.W;

	return result;
	
}

// Literally corner-cutting spherical interpolation. Does complete alignment and renormalization.
inline void FastSlerpNormQuat(const FQuat *quat1,const FQuat *quat2, float slerp, FQuat *result)
{	
	FLOAT Cosom,RawCosom,scale0,scale1;

	// Get cosine of angle between quats.
	Cosom = RawCosom = 
		quat1->X * quat2->X +
		quat1->Y * quat2->Y +
		quat1->Z * quat2->Z +
		quat1->W * quat2->W;

	// Unaligned quats - compensate, results in taking shorter route.
	if( RawCosom < 0.0f )  
	{
		Cosom = -RawCosom;
	}

	// Linearly interpolatable ?
	if( Cosom < 0.55f ) // Arbitrary cutoff for linear interpolation. // .55 seems useful.
	{	
		FLOAT omega = appAcos(Cosom);
		FLOAT sininv = 1.f/appSin(omega);
		scale0 = appSin( ( 1.f - slerp ) * omega ) * sininv;
		scale1 = appSin( slerp * omega ) * sininv;
	}
	else
	{
		// Use linear interpolation.
		scale0 = 1.0f - slerp;
		scale1 = slerp;	
	}

	// In keeping with our flipped Cosom:
	if( RawCosom < 0.0f)
	{		
		scale1= -scale1;
	}

	result->X = scale0 * quat1->X + scale1 * quat2->X;
	result->Y = scale0 * quat1->Y + scale1 * quat2->Y;
	result->Z = scale0 * quat1->Z + scale1 * quat2->Z;
	result->W = scale0 * quat1->W + scale1 * quat2->W;	
	
	// Normalize:
	FLOAT SquareSum = result->X*result->X + 
			          result->Y*result->Y + 
					  result->Z*result->Z + 
					  result->W*result->W; 

	if( SquareSum >= 0.00001f )
	{
		FLOAT Scale = 1.0f/(FLOAT)appSqrt(SquareSum);
		result->X *= Scale; 
		result->Y *= Scale; 
		result->Z *= Scale;
		result->W *= Scale;
	}	
	else  // Avoid divide by (nearly) zero.
	{	
		result->X = 0.0f;
		result->Y = 0.0f;
		result->Z = 0.0f;
		result->W = 1.0f;
	}	

	return;
}

// This special case math code moved from UnSkeletalMesh.cpp so that
// LIPSinc code can access them.

inline void FastQuatToFCoords( FQuat& Q, FVector& P, FCoords& SpaceDest )
{
	// SpaceBases(bone) = FQuaternionCoords( CachedOrientations(bone));
	// SpaceBases(bone).Origin = CachedPositions(bone);
	FLOAT wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;

	x2 = Q.X + Q.X;  y2 = Q.Y + Q.Y;  z2 = Q.Z + Q.Z;
	xx = Q.X * x2;   xy = Q.X * y2;   xz = Q.X * z2;
	yy = Q.Y * y2;   yz = Q.Y * z2;   zz = Q.Z * z2;
	wx = Q.W * x2;   wy = Q.W * y2;   wz = Q.W * z2;

	SpaceDest.XAxis.X = 1.0f - (yy + zz);
	SpaceDest.YAxis.Y = 1.0f - (xx + zz);
	SpaceDest.ZAxis.Z = 1.0f - (xx + yy);

	SpaceDest.XAxis.Y = xy - wz;
	SpaceDest.YAxis.X = xy + wz;

	SpaceDest.XAxis.Z = xz + wy;
	SpaceDest.ZAxis.X = xz - wy;

	SpaceDest.YAxis.Z = yz - wx;
	SpaceDest.ZAxis.Y = yz + wx;

	SpaceDest.Origin = P;
}

// Fast float comparison for animation keys.
FORCEINLINE UBOOL SmallerEqPositiveFloat(FLOAT& F1,FLOAT& F2)
{
	return ( (*(DWORD*)&(F1)) <= (*(DWORD*)&(F2)));
}

FORCEINLINE UBOOL SmallerPositiveFloat(FLOAT& F1,FLOAT& F2)
{
	return ( (*(DWORD*)&(F1)) < (*(DWORD*)&(F2)));
}

/*-----------------------------------------------------------------------------
	FMatrix classes.
-----------------------------------------------------------------------------*/

/*
	FMatrix
	Floating point 4x4 matrix
*/

class CORE_API FMatrix
{
public:

	static FMatrix	Identity;

	// Variables.
#if defined(__MWERKS__) && defined(__PSX2_EE__)
	FLOAT M[4][4] GCC_PACK(16);
#else
		FLOAT M[4][4]; 
#endif 

	// Constructors.

	FORCEINLINE FMatrix()
	{
	}

	FORCEINLINE FMatrix(FPlane InX,FPlane InY,FPlane InZ,FPlane InW)
	{
		M[0][0] = InX.X; M[0][1] = InX.Y;  M[0][2] = InX.Z;  M[0][3] = InX.W;
		M[1][0] = InY.X; M[1][1] = InY.Y;  M[1][2] = InY.Z;  M[1][3] = InY.W;
		M[2][0] = InZ.X; M[2][1] = InZ.Y;  M[2][2] = InZ.Z;  M[2][3] = InZ.W;
		M[3][0] = InW.X; M[3][1] = InW.Y;  M[3][2] = InW.Z;  M[3][3] = InW.W;
	}

	// Destructor.

	~FMatrix()
	{
	}

	void SetIdentity()
	{
		M[0][0] = 1; M[0][1] = 0;  M[0][2] = 0;  M[0][3] = 0;
		M[1][0] = 0; M[1][1] = 1;  M[1][2] = 0;  M[1][3] = 0;
		M[2][0] = 0; M[2][1] = 0;  M[2][2] = 1;  M[2][3] = 0;
		M[3][0] = 0; M[3][1] = 0;  M[3][2] = 0;  M[3][3] = 1;
	}

	// Concatenation operator.

	FORCEINLINE FMatrix operator*(FMatrix Other) const
	{
		FMatrix	Result;

#if defined(__MWERKS__) && defined(__PSX2_EE__)
		asm (
		"lqc2 vf4, 0x00(%0)\n"
		"lqc2 vf5, 0x10(%0)\n"
		"lqc2 vf6, 0x20(%0)\n"
		"lqc2 vf7, 0x30(%0)\n"

		"lqc2 vf8, 0x00(%1)\n"

		"vmulax.xyzw ACC, vf4, vf8\n"
		"vmadday.xyzw ACC, vf5, vf8\n"
		"vmaddaz.xyzw ACC, vf6, vf8\n"
		"vmaddw.xyzw vf8, vf7, vf8\n"

		"sqc2 vf8, 0x00(%2)\n"

		"lqc2 vf10, 0x10(%1)\n"

		"vmulax.xyzw ACC, vf4, vf10\n"
		"vmadday.xyzw ACC, vf5, vf10\n"
		"vmaddaz.xyzw ACC, vf6, vf10\n"
		"vmaddw.xyzw vf10, vf7, vf10\n"

		"sqc2 vf10, 0x10(%2)\n"

		"lqc2 vf9, 0x20(%1)\n"

		"vmulax.xyzw ACC, vf4, vf9\n"
		"vmadday.xyzw ACC, vf5, vf9\n"
		"vmaddaz.xyzw ACC, vf6, vf9\n"
		"vmaddw.xyzw vf9, vf7, vf9\n"

		"sqc2 vf9, 0x20(%2)\n"

		"lqc2 vf11, 0x30(%1)\n"

		"vmulax.xyzw ACC, vf4, vf11\n"
		"vmadday.xyzw ACC, vf5, vf11\n"
		"vmaddaz.xyzw ACC, vf6, vf11\n"
		"vmaddw.xyzw vf11, vf7, vf11\n"

		"sqc2 vf11, 0x30(%2)\n"

		: : "r" (&Other.M[0][0]) , "r" (&M[0][0]), "r" (&Result.M[0][0]) : "memory");

#else

		Result.M[0][0] = M[0][0] * Other.M[0][0] + M[0][1] * Other.M[1][0] + M[0][2] * Other.M[2][0] + M[0][3] * Other.M[3][0];
		Result.M[0][1] = M[0][0] * Other.M[0][1] + M[0][1] * Other.M[1][1] + M[0][2] * Other.M[2][1] + M[0][3] * Other.M[3][1];
		Result.M[0][2] = M[0][0] * Other.M[0][2] + M[0][1] * Other.M[1][2] + M[0][2] * Other.M[2][2] + M[0][3] * Other.M[3][2];
		Result.M[0][3] = M[0][0] * Other.M[0][3] + M[0][1] * Other.M[1][3] + M[0][2] * Other.M[2][3] + M[0][3] * Other.M[3][3];

		Result.M[1][0] = M[1][0] * Other.M[0][0] + M[1][1] * Other.M[1][0] + M[1][2] * Other.M[2][0] + M[1][3] * Other.M[3][0];
		Result.M[1][1] = M[1][0] * Other.M[0][1] + M[1][1] * Other.M[1][1] + M[1][2] * Other.M[2][1] + M[1][3] * Other.M[3][1];
		Result.M[1][2] = M[1][0] * Other.M[0][2] + M[1][1] * Other.M[1][2] + M[1][2] * Other.M[2][2] + M[1][3] * Other.M[3][2];
		Result.M[1][3] = M[1][0] * Other.M[0][3] + M[1][1] * Other.M[1][3] + M[1][2] * Other.M[2][3] + M[1][3] * Other.M[3][3];

		Result.M[2][0] = M[2][0] * Other.M[0][0] + M[2][1] * Other.M[1][0] + M[2][2] * Other.M[2][0] + M[2][3] * Other.M[3][0];
		Result.M[2][1] = M[2][0] * Other.M[0][1] + M[2][1] * Other.M[1][1] + M[2][2] * Other.M[2][1] + M[2][3] * Other.M[3][1];
		Result.M[2][2] = M[2][0] * Other.M[0][2] + M[2][1] * Other.M[1][2] + M[2][2] * Other.M[2][2] + M[2][3] * Other.M[3][2];
		Result.M[2][3] = M[2][0] * Other.M[0][3] + M[2][1] * Other.M[1][3] + M[2][2] * Other.M[2][3] + M[2][3] * Other.M[3][3];

		Result.M[3][0] = M[3][0] * Other.M[0][0] + M[3][1] * Other.M[1][0] + M[3][2] * Other.M[2][0] + M[3][3] * Other.M[3][0];
		Result.M[3][1] = M[3][0] * Other.M[0][1] + M[3][1] * Other.M[1][1] + M[3][2] * Other.M[2][1] + M[3][3] * Other.M[3][1];
		Result.M[3][2] = M[3][0] * Other.M[0][2] + M[3][1] * Other.M[1][2] + M[3][2] * Other.M[2][2] + M[3][3] * Other.M[3][2];
		Result.M[3][3] = M[3][0] * Other.M[0][3] + M[3][1] * Other.M[1][3] + M[3][2] * Other.M[2][3] + M[3][3] * Other.M[3][3];

#endif

		return Result;
	}

	FORCEINLINE void operator*=(FMatrix Other)
	{

#if defined(__MWERKS__) && defined(__PSX2_EE__)
		asm (
		"lqc2 vf4, 0x00(%0)\n"
		"lqc2 vf5, 0x10(%0)\n"
		"lqc2 vf6, 0x20(%0)\n"
		"lqc2 vf7, 0x30(%0)\n"

		"lqc2 vf8, 0x00(%1)\n"

		"vmulax.xyzw ACC, vf4, vf8\n"
		"vmadday.xyzw ACC, vf5, vf8\n"
		"vmaddaz.xyzw ACC, vf6, vf8\n"
		"vmaddw.xyzw vf8, vf7, vf8\n"

		"sqc2 vf8, 0x00(%1)\n"

		"lqc2 vf10, 0x10(%1)\n"

		"vmulax.xyzw ACC, vf4, vf10\n"
		"vmadday.xyzw ACC, vf5, vf10\n"
		"vmaddaz.xyzw ACC, vf6, vf10\n"
		"vmaddw.xyzw vf10, vf7, vf10\n"

		"sqc2 vf10, 0x10(%1)\n"

		"lqc2 vf9, 0x20(%1)\n"

		"vmulax.xyzw ACC, vf4, vf9\n"
		"vmadday.xyzw ACC, vf5, vf9\n"
		"vmaddaz.xyzw ACC, vf6, vf9\n"
		"vmaddw.xyzw vf9, vf7, vf9\n"

		"sqc2 vf9, 0x20(%1)\n"

		"lqc2 vf11, 0x30(%1)\n"

		"vmulax.xyzw ACC, vf4, vf11\n"
		"vmadday.xyzw ACC, vf5, vf11\n"
		"vmaddaz.xyzw ACC, vf6, vf11\n"
		"vmaddw.xyzw vf11, vf7, vf11\n"

		"sqc2 vf11, 0x30(%1)\n"

		: : "r" (&Other.M[0][0]) , "r" (&M[0][0]) );

#else //__PSX2_EE__

		FMatrix Result;
		Result.M[0][0] = M[0][0] * Other.M[0][0] + M[0][1] * Other.M[1][0] + M[0][2] * Other.M[2][0] + M[0][3] * Other.M[3][0];
		Result.M[0][1] = M[0][0] * Other.M[0][1] + M[0][1] * Other.M[1][1] + M[0][2] * Other.M[2][1] + M[0][3] * Other.M[3][1];
		Result.M[0][2] = M[0][0] * Other.M[0][2] + M[0][1] * Other.M[1][2] + M[0][2] * Other.M[2][2] + M[0][3] * Other.M[3][2];
		Result.M[0][3] = M[0][0] * Other.M[0][3] + M[0][1] * Other.M[1][3] + M[0][2] * Other.M[2][3] + M[0][3] * Other.M[3][3];

		Result.M[1][0] = M[1][0] * Other.M[0][0] + M[1][1] * Other.M[1][0] + M[1][2] * Other.M[2][0] + M[1][3] * Other.M[3][0];
		Result.M[1][1] = M[1][0] * Other.M[0][1] + M[1][1] * Other.M[1][1] + M[1][2] * Other.M[2][1] + M[1][3] * Other.M[3][1];
		Result.M[1][2] = M[1][0] * Other.M[0][2] + M[1][1] * Other.M[1][2] + M[1][2] * Other.M[2][2] + M[1][3] * Other.M[3][2];
		Result.M[1][3] = M[1][0] * Other.M[0][3] + M[1][1] * Other.M[1][3] + M[1][2] * Other.M[2][3] + M[1][3] * Other.M[3][3];

		Result.M[2][0] = M[2][0] * Other.M[0][0] + M[2][1] * Other.M[1][0] + M[2][2] * Other.M[2][0] + M[2][3] * Other.M[3][0];
		Result.M[2][1] = M[2][0] * Other.M[0][1] + M[2][1] * Other.M[1][1] + M[2][2] * Other.M[2][1] + M[2][3] * Other.M[3][1];
		Result.M[2][2] = M[2][0] * Other.M[0][2] + M[2][1] * Other.M[1][2] + M[2][2] * Other.M[2][2] + M[2][3] * Other.M[3][2];
		Result.M[2][3] = M[2][0] * Other.M[0][3] + M[2][1] * Other.M[1][3] + M[2][2] * Other.M[2][3] + M[2][3] * Other.M[3][3];

		Result.M[3][0] = M[3][0] * Other.M[0][0] + M[3][1] * Other.M[1][0] + M[3][2] * Other.M[2][0] + M[3][3] * Other.M[3][0];
		Result.M[3][1] = M[3][0] * Other.M[0][1] + M[3][1] * Other.M[1][1] + M[3][2] * Other.M[2][1] + M[3][3] * Other.M[3][1];
		Result.M[3][2] = M[3][0] * Other.M[0][2] + M[3][1] * Other.M[1][2] + M[3][2] * Other.M[2][2] + M[3][3] * Other.M[3][2];
		Result.M[3][3] = M[3][0] * Other.M[0][3] + M[3][1] * Other.M[1][3] + M[3][2] * Other.M[2][3] + M[3][3] * Other.M[3][3];
		*this = Result;

#endif //__PSX2_EE__
	}

	// Comparison operators.

	inline UBOOL operator==(FMatrix& Other) const
	{
		for(INT X = 0;X < 4;X++)
			for(INT Y = 0;Y < 4;Y++)
				if(M[X][Y] != Other.M[X][Y])
					return 0;

		return 1;
	}

	inline UBOOL operator!=(FMatrix& Other) const
	{
		return !(*this == Other);
	}

	// Homogeneous transform.

	FORCEINLINE FPlane TransformFPlane(const FPlane &P) const
	{
		FPlane Result;

#if ASM && !_DEBUG
		__asm
		{
			// Setup.

			mov esi, P
			mov edx, [this]
			lea edi, Result

			fld dword ptr [esi + 0]		//	X
			fmul dword ptr [edx + 0]	//	Xx
			fld dword ptr [esi + 0]		//	X		Xx
			fmul dword ptr [edx + 4]	//	Xy		Xx
			fld dword ptr [esi + 0]		//	X		Xy		Xx
			fmul dword ptr [edx + 8]	//	Xz		Xy		Xx
			fld dword ptr [esi + 0]		//	X		Xz		Xy		Xx
			fmul dword ptr [edx + 12]	//	Xw		Xz		Xy		Xx

			fld dword ptr [esi + 4]		//	Y		Xw		Xz		Xy		Xx
			fmul dword ptr [edx + 16]	//	Yx		Xw		Xz		Xy		Xx
			fld dword ptr [esi + 4]		//	Y		Yx		Xw		Xz		Xy		Xx
			fmul dword ptr [edx + 20]	//	Yy		Yx		Xw		Xz		Xy		Xx
			fld dword ptr [esi + 4]		//	Y		Yy		Yx		Xw		Xz		Xy		Xx
			fmul dword ptr [edx + 24]	//	Yz		Yy		Yx		Xw		Xz		Xy		Xx
			fld dword ptr [esi + 4]		//	Y		Yz		Yy		Yx		Xw		Xz		Xy		Xx
			fmul dword ptr [edx + 28]	//	Yw		Yz		Yy		Yx		Xw		Xz		Xy		Xx

			fxch st(3)					//	Yx		Yz		Yy		Yw		Xw		Xz		Xy		Xx
			faddp st(7), st(0)			//	Yz		Yy		Yw		Xw		Xz		Xy		XYx
			faddp st(4), st(0)			//	Yy		Yw		Xw		XYz		Xy		XYx
			faddp st(4), st(0)			//	Yw		Xw		XYz		XYy		XYx
			faddp st(1), st(0)			//	XYw		XYz		XYy		XYx

			fld dword ptr [esi + 8]		//	Z		XYw		XYz		XYy		XYx
			fmul dword ptr [edx + 32]	//	Zx		XYw		XYz		XYy		XYx
			fld dword ptr [esi + 8]		//	Z		Zx		XYw		XYz		XYy		XYx
			fmul dword ptr [edx + 36]	//	Zy		Zx		XYw		XYz		XYy		XYx
			fld dword ptr [esi + 8]		//	Z		Zy		Zx		XYw		XYz		XYy		XYx
			fmul dword ptr [edx + 40]	//	Zz		Zy		Zx		XYw		XYz		XYy		XYx
			fld dword ptr [esi + 8]		//	Z		Zz		Zy		Zx		XYw		XYz		XYy		XYx
			fmul dword ptr [edx + 44]	//	Zw		Zz		Zy		Zx		XYw		XYz		XYy		XYx

			fxch st(3)					//	Zx		Zz		Zy		Zw		XYw		XYz		XYy		XYx
			faddp st(7), st(0)			//	Zz		Zy		Zw		XYw		XYz		XYy		XYZx
			faddp st(4), st(0)			//	Zy		Zw		XYw		XYZz	XYy		XYZx
			faddp st(4), st(0)			//	Zw		XYw		XYZz	XYZy	XYZx
			faddp st(1), st(0)			//	XYZw	XYZz	XYZy	XYZx

			fld dword ptr [esi + 12]	//	W		XYZw	XYZz	XYZy	XYZx
			fmul dword ptr [edx + 48]	//	Wx		XYZw	XYZz	XYZy	XYZx
			fld dword ptr [esi + 12]	//	W		Wx		XYZw	XYZz	XYZy	XYZx
			fmul dword ptr [edx + 52]	//	Wy		Wx		XYZw	XYZz	XYZy	XYZx
			fld dword ptr [esi + 12]	//	W		Wy		Wx		XYZw	XYZz	XYZy	XYZx
			fmul dword ptr [edx + 56]	//	Wz		Wy		Wx		XYZw	XYZz	XYZy	XYZx
			fld dword ptr [esi + 12]	//	W		Wz		Wy		Wx		XYZw	XYZz	XYZy	XYZx
			fmul dword ptr [edx + 60]	//	Ww		Wz		Wy		Wx		XYZw	XYZz	XYZy	XYZx

			fxch st(3)					//	Wx		Wz		Wy		Ww		XYZw	XYZz	XYZy	XYZx
			faddp st(7), st(0)			//	Wz		Wy		Ww		XYZw	XYZz	XYZy	XYZWx
			faddp st(4), st(0)			//	Wy		Ww		XYZw	XYZWz	XYZy	XYZWx
			faddp st(4), st(0)			//	Ww		XYZw	XYZWz	XYZWy	XYZWx
			faddp st(1), st(0)			//	XYZWw	XYZWz	XYZWy	XYZWx

			fxch st(3)					//	XYZWx	XYZWz	XYZWy	XYZWw
			fstp dword ptr [edi + 0]	//	XYZWz	XYZWy	XYZWw
			fxch st(1)					//	XYZWy	XYZWz	XYZWw
			fstp dword ptr [edi + 4]	//	XYZWz	XYZWw
			fstp dword ptr [edi + 8]	//	XYZWw
			fstp dword ptr [edi + 12]
		}
#else
		Result.X = P.X * M[0][0] + P.Y * M[1][0] + P.Z * M[2][0] + P.W * M[3][0];
		Result.Y = P.X * M[0][1] + P.Y * M[1][1] + P.Z * M[2][1] + P.W * M[3][1];
		Result.Z = P.X * M[0][2] + P.Y * M[1][2] + P.Z * M[2][2] + P.W * M[3][2];
		Result.W = P.X * M[0][3] + P.Y * M[1][3] + P.Z * M[2][3] + P.W * M[3][3];
#endif

		return Result;
	}

	// Regular transform.

	FORCEINLINE FVector TransformFVector(const FVector &V) const
	{
		return TransformFPlane(FPlane(V.X,V.Y,V.Z,1.0f));
	}

	// Normal transform.

	FORCEINLINE FPlane TransformNormal(const FVector& V) const
	{
		return TransformFPlane(FPlane(V.X,V.Y,V.Z,0.0f));
	}

	// Transpose.

	FORCEINLINE FMatrix Transpose()
	{
		FMatrix	Result;

		Result.M[0][0] = M[0][0];
		Result.M[0][1] = M[1][0];
		Result.M[0][2] = M[2][0];
		Result.M[0][3] = M[3][0];

		Result.M[1][0] = M[0][1];
		Result.M[1][1] = M[1][1];
		Result.M[1][2] = M[2][1];
		Result.M[1][3] = M[3][1];

		Result.M[2][0] = M[0][2];
		Result.M[2][1] = M[1][2];
		Result.M[2][2] = M[2][2];
		Result.M[2][3] = M[3][2];

		Result.M[3][0] = M[0][3];
		Result.M[3][1] = M[1][3];
		Result.M[3][2] = M[2][3];
		Result.M[3][3] = M[3][3];

		return Result;
	}

	// Determinant.

	inline FLOAT Determinant() const
	{
#if defined(__MWERKS__) && defined(__PSX2_EE__)
	    float a1,a2,a3,a4,b1,b2,b3,b4,c1,c2,c3,c4,d1,d2,d3,d4;

		// Assign to individual variable names to aid selecting
		// correct elements
		a1 = M[0][0]; b1 = M[0][1]; 
		c1 = M[0][2]; d1 = M[0][3];

		a2 = M[1][0]; b2 = M[1][1]; 
		c2 = M[1][2]; d2 = M[1][3];

		a3 = M[2][0]; b3 = M[2][1]; 
		c3 = M[2][2]; d3 = M[2][3];

		a4 = M[3][0]; b4 = M[3][1]; 
		c4 = M[3][2]; d4 = M[3][3];

	    return Float_Mac4_VU0(
			a1,  Determinant3_VU0( b2, b3, b4, c2, c3, c4, d2, d3, d4),
			-b1, Determinant3_VU0( a2, a3, a4, c2, c3, c4, d2, d3, d4),
			c1,  Determinant3_VU0( a2, a3, a4, b2, b3, b4, d2, d3, d4),
			-d1, Determinant3_VU0( a2, a3, a4, b2, b3, b4, c2, c3, c4)
		);
#else //__PSX2_EE__
		return	M[0][0] * (
					M[1][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
					M[2][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) +
					M[3][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2])
					) -
				M[1][0] * (
					M[0][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
					M[2][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
					M[3][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2])
					) +
				M[2][0] * (
					M[0][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) -
					M[1][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
					M[3][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
					) -
				M[3][0] * (
					M[0][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2]) -
					M[1][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2]) +
					M[2][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
					);
#endif
	}

	// Inverse.

#if defined(__MWERKS__) && defined(__PSX2_EE__)
	void Adjoint_VU0( FMatrix & matrix )
	{
	    float a1, a2, a3, a4, b1, b2, b3, b4;
	    float c1, c2, c3, c4, d1, d2, d3, d4;

		// Assign to individual variable names to aid
		// selecting correct values
		a1 = M[0][0]; b1 = M[0][1]; 
		c1 = M[0][2]; d1 = M[0][3];

		a2 = M[1][0]; b2 = M[1][1]; 
		c2 = M[1][2]; d2 = M[1][3];

		a3 = M[2][0]; b3 = M[2][1];
		c3 = M[2][2]; d3 = M[2][3];

		a4 = M[3][0]; b4 = M[3][1]; 
		c4 = M[3][2]; d4 = M[3][3];

	    /* row column labeling reversed since we transpose rows & columns */
	    matrix.M[0][0] =  Determinant3_VU0( b2, b3, b4, c2, c3, c4, d2, d3, d4);
	    matrix.M[1][0] = -Determinant3_VU0( a2, a3, a4, c2, c3, c4, d2, d3, d4);
	    matrix.M[2][0] =  Determinant3_VU0( a2, a3, a4, b2, b3, b4, d2, d3, d4);
	    matrix.M[3][0] = -Determinant3_VU0( a2, a3, a4, b2, b3, b4, c2, c3, c4);
	        
	    matrix.M[0][1] = -Determinant3_VU0( b1, b3, b4, c1, c3, c4, d1, d3, d4);
	    matrix.M[1][1] =  Determinant3_VU0( a1, a3, a4, c1, c3, c4, d1, d3, d4);
	    matrix.M[2][1] = -Determinant3_VU0( a1, a3, a4, b1, b3, b4, d1, d3, d4);
	    matrix.M[3][1] =  Determinant3_VU0( a1, a3, a4, b1, b3, b4, c1, c3, c4);
	        
	    matrix.M[0][2] =  Determinant3_VU0( b1, b2, b4, c1, c2, c4, d1, d2, d4);
	    matrix.M[1][2] = -Determinant3_VU0( a1, a2, a4, c1, c2, c4, d1, d2, d4);
	    matrix.M[2][2] =  Determinant3_VU0( a1, a2, a4, b1, b2, b4, d1, d2, d4);
	    matrix.M[3][2] = -Determinant3_VU0( a1, a2, a4, b1, b2, b4, c1, c2, c4);
	        
	    matrix.M[0][3] = -Determinant3_VU0( b1, b2, b3, c1, c2, c3, d1, d2, d3);
	    matrix.M[1][3] =  Determinant3_VU0( a1, a2, a3, c1, c2, c3, d1, d2, d3);
	    matrix.M[2][3] = -Determinant3_VU0( a1, a2, a3, b1, b2, b3, d1, d2, d3);
	    matrix.M[3][3] =  Determinant3_VU0( a1, a2, a3, b1, b2, b3, c1, c2, c3);
	}
#endif //PSX2_EE
	FMatrix Inverse()
	{
		FMatrix Result;

#if defined(__MWERKS__) && defined(__PSX2_EE__)
		int i, j;
	    	float det;
	    	float rdet;
	
		// Calculate the adjoint tmatrix
		Adjoint_VU0( Result );

		// Calculate 4x4 determinant
		// If the determinant is 0,
		// the inverse is not unique.
		det = Determinant();

		if( (det < 0.0001) && (det > -0.0001) )
			return FMatrix::Identity;

		// Scale adjoint matrix to get inverse
		rdet = Float_Rcp_VU0(det);

	    	for (i=0; i<4; i++)
	        	for(j=0; j<4; j++)
				Result.M[i][j] = Result.M[i][j] * rdet;
#else
		FLOAT	Det = Determinant();

		if(Det == 0.0f)
			return FMatrix::Identity;

		FLOAT	RDet = 1.0f / Det;

		Result.M[0][0] = RDet * (
				M[1][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
				M[2][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) +
				M[3][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2])
				);
		Result.M[0][1] = -RDet * (
				M[0][1] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
				M[2][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
				M[3][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2])
				);
		Result.M[0][2] = RDet * (
				M[0][1] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) -
				M[1][1] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
				M[3][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
				);
		Result.M[0][3] = -RDet * (
				M[0][1] * (M[1][2] * M[2][3] - M[1][3] * M[2][2]) -
				M[1][1] * (M[0][2] * M[2][3] - M[0][3] * M[2][2]) +
				M[2][1] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
				);

		Result.M[1][0] = -RDet * (
				M[1][0] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
				M[2][0] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) +
				M[3][0] * (M[1][2] * M[2][3] - M[1][3] * M[2][2])
				);
		Result.M[1][1] = RDet * (
				M[0][0] * (M[2][2] * M[3][3] - M[2][3] * M[3][2]) -
				M[2][0] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
				M[3][0] * (M[0][2] * M[2][3] - M[0][3] * M[2][2])
				);
		Result.M[1][2] = -RDet * (
				M[0][0] * (M[1][2] * M[3][3] - M[1][3] * M[3][2]) -
				M[1][0] * (M[0][2] * M[3][3] - M[0][3] * M[3][2]) +
				M[3][0] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
				);
		Result.M[1][3] = RDet * (
				M[0][0] * (M[1][2] * M[2][3] - M[1][3] * M[2][2]) -
				M[1][0] * (M[0][2] * M[2][3] - M[0][3] * M[2][2]) +
				M[2][0] * (M[0][2] * M[1][3] - M[0][3] * M[1][2])
				);

		Result.M[2][0] = RDet * (
				M[1][0] * (M[2][1] * M[3][3] - M[2][3] * M[3][1]) -
				M[2][0] * (M[1][1] * M[3][3] - M[1][3] * M[3][1]) +
				M[3][0] * (M[1][1] * M[2][3] - M[1][3] * M[2][1])
				);
		Result.M[2][1] = -RDet * (
				M[0][0] * (M[2][1] * M[3][3] - M[2][3] * M[3][1]) -
				M[2][0] * (M[0][1] * M[3][3] - M[0][3] * M[3][1]) +
				M[3][0] * (M[0][1] * M[2][3] - M[0][3] * M[2][1])
				);
		Result.M[2][2] = RDet * (
				M[0][0] * (M[1][1] * M[3][3] - M[1][3] * M[3][1]) -
				M[1][0] * (M[0][1] * M[3][3] - M[0][3] * M[3][1]) +
				M[3][0] * (M[0][1] * M[1][3] - M[0][3] * M[1][1])
				);
		Result.M[2][3] = -RDet * (
				M[0][0] * (M[1][1] * M[2][3] - M[1][3] * M[2][1]) -
				M[1][0] * (M[0][1] * M[2][3] - M[0][3] * M[2][1]) +
				M[2][0] * (M[0][1] * M[1][3] - M[0][3] * M[1][1])
				);

		Result.M[3][0] = -RDet * (
				M[1][0] * (M[2][1] * M[3][2] - M[2][2] * M[3][1]) -
				M[2][0] * (M[1][1] * M[3][2] - M[1][2] * M[3][1]) +
				M[3][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1])
				);
		Result.M[3][1] = RDet * (
				M[0][0] * (M[2][1] * M[3][2] - M[2][2] * M[3][1]) -
				M[2][0] * (M[0][1] * M[3][2] - M[0][2] * M[3][1]) +
				M[3][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1])
				);
		Result.M[3][2] = -RDet * (
				M[0][0] * (M[1][1] * M[3][2] - M[1][2] * M[3][1]) -
				M[1][0] * (M[0][1] * M[3][2] - M[0][2] * M[3][1]) +
				M[3][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1])
				);
		Result.M[3][3] = RDet * (
				M[0][0] * (M[1][1] * M[2][2] - M[1][2] * M[2][1]) -
				M[1][0] * (M[0][1] * M[2][2] - M[0][2] * M[2][1]) +
				M[2][0] * (M[0][1] * M[1][2] - M[0][2] * M[1][1])
				);
#endif
		return Result;
	}

	FMatrix TransposeAdjoint() const
	{
		FMatrix ta;

		ta.M[0][0] = this->M[1][1] * this->M[2][2] - this->M[1][2] * this->M[2][1];
		ta.M[0][1] = this->M[1][2] * this->M[2][0] - this->M[1][0] * this->M[2][2];
		ta.M[0][2] = this->M[1][0] * this->M[2][1] - this->M[1][1] * this->M[2][0];
		ta.M[0][3] = 0.f;

		ta.M[1][0] = this->M[2][1] * this->M[0][2] - this->M[2][2] * this->M[0][1];
		ta.M[1][1] = this->M[2][2] * this->M[0][0] - this->M[2][0] * this->M[0][2];
		ta.M[1][2] = this->M[2][0] * this->M[0][1] - this->M[2][1] * this->M[0][0];
		ta.M[1][3] = 0.f;

		ta.M[2][0] = this->M[0][1] * this->M[1][2] - this->M[0][2] * this->M[1][1];
		ta.M[2][1] = this->M[0][2] * this->M[1][0] - this->M[0][0] * this->M[1][2];
		ta.M[2][2] = this->M[0][0] * this->M[1][1] - this->M[0][1] * this->M[1][0];
		ta.M[2][3] = 0.f;

		ta.M[3][0] = 0.f;
		ta.M[3][1] = 0.f;
		ta.M[3][2] = 0.f;
		ta.M[3][3] = 1.f;

		return ta;
	}

	// Remove any scaling from this matrix (ie magnitude of each row is 1)
	FMatrix RemoveScaling()
	{
		FLOAT SquareSum, Scale;

		// For each row, find magnitude, and if its non-zero re-scale so its unit length.
		for(INT i=0; i<3; i++)
		{
			SquareSum = (M[i][0] * M[i][0]) + (M[i][1] * M[i][1]) + (M[i][2] * M[i][2]);
			if(SquareSum > SMALL_NUMBER)
			{
				Scale = 1.f/appSqrt(SquareSum);
				M[i][0] *= Scale; M[i][1] *= Scale; M[i][2] *= Scale; 
			}
		}

		return *this;
	}


	// Conversions.

	FCoords Coords()
	{
		FCoords	Result;

		Result.XAxis = FVector(M[0][0],M[1][0],M[2][0]);
		Result.YAxis = FVector(M[0][1],M[1][1],M[2][1]);
		Result.ZAxis = FVector(M[0][2],M[1][2],M[2][2]);
		Result.Origin = FVector(M[3][0],M[3][1],M[3][2]);

		return Result;
	}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FMatrix& M)
	{
		return Ar << 
			M.M[0][0] << M.M[0][1] << M.M[0][2] << M.M[0][3] << 
			M.M[1][0] << M.M[1][1] << M.M[1][2] << M.M[1][3] << 
			M.M[2][0] << M.M[2][1] << M.M[2][2] << M.M[2][3] << 
			M.M[3][0] << M.M[3][1] << M.M[3][2] << M.M[3][3];
	}
};

// Matrix operations.

class FPerspectiveMatrix : public FMatrix
{
public:

	FPerspectiveMatrix(float FOVX, float FOVY, float MultFOVX, float MultFOVY, float MinZ, float MaxZ) :
	  FMatrix(
			FPlane(MultFOVX / appTan(FOVX),		0.0f,							0.0f,							0.0f),
			FPlane(0.0f,						MultFOVY / appTan(FOVY),		0.0f,							0.0f),
			FPlane(0.0f,						0.0f,							MaxZ / (MaxZ - MinZ),			1.0f),
			FPlane(0.0f,						0.0f,							-MinZ * (MaxZ / (MaxZ - MinZ)),	0.0f))
	{
	}

	FPerspectiveMatrix(float FOV, float Width, float Height, float MinZ, float MaxZ) :
	  FMatrix(
			FPlane(1.0f / appTan(FOV),	0.0f,							0.0f,							0.0f),
			FPlane(0.0f,				Width / appTan(FOV) / Height,	0.0f,							0.0f),
			FPlane(0.0f,				0.0f,							MaxZ / (MaxZ - MinZ),			1.0f),
			FPlane(0.0f,				0.0f,							-MinZ * (MaxZ / (MaxZ - MinZ)),	0.0f))
	{
	}
};

class FOrthoMatrix : public FMatrix
{
public:

	FOrthoMatrix(float Width,float Height,float ZScale,float ZOffset) :
		FMatrix(
			FPlane(1.0f / Width,	0.0f,			0.0f,				0.0f),
			FPlane(0.0f,			1.0f / Height,	0.0f,				0.0f),
			FPlane(0.0f,			0.0f,			ZScale,				0.0f),
			FPlane(0.0f,			0.0f,			ZOffset * ZScale,	1.0f))
	{
	}
};

class FTranslationMatrix : public FMatrix
{
public:

	FTranslationMatrix(FVector Delta) :
		FMatrix(
			FPlane(1.0f,	0.0f,	0.0f,	0.0f),
			FPlane(0.0f,	1.0f,	0.0f,	0.0f),
			FPlane(0.0f,	0.0f,	1.0f,	0.0f),
			FPlane(Delta.X,	Delta.Y,Delta.Z,1.0f))
	{
	}
};

class FRotationMatrix : public FMatrix
{
public:

#if 0
	FRotationMatrix(FRotator Rot) :
	  FMatrix(
			FMatrix(	// Roll
				FPlane(1.0f,					0.0f,					0.0f,						0.0f),
				FPlane(0.0f,					+GMath.CosTab(Rot.Roll),-GMath.SinTab(Rot.Roll),	0.0f),
				FPlane(0.0f,					+GMath.SinTab(Rot.Roll),+GMath.CosTab(Rot.Roll),	0.0f),
				FPlane(0.0f,					0.0f,					0.0f,						1.0f)) *
			FMatrix(	// Pitch
				FPlane(+GMath.CosTab(Rot.Pitch),0.0f,					+GMath.SinTab(Rot.Pitch),	0.0f),
				FPlane(0.0f,					1.0f,					0.0f,						0.0f),
				FPlane(-GMath.SinTab(Rot.Pitch),0.0f,					+GMath.CosTab(Rot.Pitch),	0.0f),
				FPlane(0.0f,					0.0f,					0.0f,						1.0f)) *
			FMatrix(	// Yaw
				FPlane(+GMath.CosTab(Rot.Yaw),	+GMath.SinTab(Rot.Yaw), 0.0f,	0.0f),
				FPlane(-GMath.SinTab(Rot.Yaw),	+GMath.CosTab(Rot.Yaw), 0.0f,	0.0f),
				FPlane(0.0f,					0.0f,					1.0f,	0.0f),
				FPlane(0.0f,					0.0f,					0.0f,	1.0f))
			)
	  {
	  }
#else
	FRotationMatrix(FRotator Rot)
	{
		FLOAT	SR	= GMath.SinTab(Rot.Roll),
				SP	= GMath.SinTab(Rot.Pitch),
				SY	= GMath.SinTab(Rot.Yaw),
				CR	= GMath.CosTab(Rot.Roll),
				CP	= GMath.CosTab(Rot.Pitch),
				CY	= GMath.CosTab(Rot.Yaw);

		M[0][0]	= CP * CY;
		M[0][1]	= CP * SY;
		M[0][2]	= SP;
		M[0][3]	= 0.f;

		M[1][0]	= SR * SP * CY - CR * SY;
		M[1][1]	= SR * SP * SY + CR * CY;
		M[1][2]	= - SR * CP;
		M[1][3]	= 0.f;

		M[2][0]	= -( CR * SP * CY + SR * SY );
		M[2][1]	= CY * SR - CR * SP * SY;
		M[2][2]	= CR * CP;
		M[2][3]	= 0.f;

		M[3][0]	= 0.f;
		M[3][1]	= 0.f;
		M[3][2]	= 0.f;
		M[3][3]	= 1.f;
	}
#endif
};

class FInverseRotationMatrix : public FMatrix
{
public:

	FInverseRotationMatrix(FRotator Rot) :
		FMatrix(
			FMatrix(	// Yaw
				FPlane(+GMath.CosTab(-Rot.Yaw),	+GMath.SinTab(-Rot.Yaw), 0.0f,	0.0f),
				FPlane(-GMath.SinTab(-Rot.Yaw),	+GMath.CosTab(-Rot.Yaw), 0.0f,	0.0f),
				FPlane(0.0f,					0.0f,					1.0f,	0.0f),
				FPlane(0.0f,					0.0f,					0.0f,	1.0f)) *
			FMatrix(	// Pitch
				FPlane(+GMath.CosTab(-Rot.Pitch),0.0f,					+GMath.SinTab(-Rot.Pitch),	0.0f),
				FPlane(0.0f,					1.0f,					0.0f,						0.0f),
				FPlane(-GMath.SinTab(-Rot.Pitch),0.0f,					+GMath.CosTab(-Rot.Pitch),	0.0f),
				FPlane(0.0f,					0.0f,					0.0f,						1.0f)) *
			FMatrix(	// Roll
				FPlane(1.0f,					0.0f,					0.0f,						0.0f),
				FPlane(0.0f,					+GMath.CosTab(-Rot.Roll),-GMath.SinTab(-Rot.Roll),	0.0f),
				FPlane(0.0f,					+GMath.SinTab(-Rot.Roll),+GMath.CosTab(-Rot.Roll),	0.0f),
				FPlane(0.0f,					0.0f,					0.0f,						1.0f))
			)
	{
	}
};

class FQuaternionMatrix : public FMatrix
{
public:

	FQuaternionMatrix(FQuat Q)
	{
		FLOAT wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;

		x2 = Q.X + Q.X;  y2 = Q.Y + Q.Y;  z2 = Q.Z + Q.Z;
		xx = Q.X * x2;   xy = Q.X * y2;   xz = Q.X * z2;
		yy = Q.Y * y2;   yz = Q.Y * z2;   zz = Q.Z * z2;
		wx = Q.W * x2;   wy = Q.W * y2;   wz = Q.W * z2;

		M[0][0] = 1.0f - (yy + zz);
		M[1][0] = xy - wz;
		M[2][0] = xz + wy;
		M[3][0] = 0.0f;

		M[0][1] = xy + wz;
		M[1][1] = 1.0f - (xx + zz);
		M[2][1] = yz - wx;
		M[3][1] = 0.0f;

		M[0][2] = xz - wy;
		M[1][2] = yz + wx;
		M[2][2] = 1.0f - (xx + yy);
		M[3][2] = 0.0f;

		M[0][3] = 0.0f;
		M[1][3] = 0.0f;
		M[2][3] = 0.0f;
		M[3][3] = 1.0f;
	}
};

class FScaleMatrix : public FMatrix
{
public:

	FScaleMatrix(FVector Scale) :
		FMatrix(
			FPlane(Scale.X,	0.0f,		0.0f,		0.0f),
			FPlane(0.0f,	Scale.Y,	0.0f,		0.0f),
			FPlane(0.0f,	0.0f,		Scale.Z,	0.0f),
			FPlane(0.0f,	0.0f,		0.0f,		1.0f))
	{
	}
};


// Transform a (rotation) matrix into a Quaternion.
class FMatrixQuaternion : public FQuat
{
public:
	FMatrixQuaternion( FMatrix M )
	{
		// Trace.
		FLOAT Trace = M.M[0][0] + M.M[1][1] + M.M[2][2] + 1.0f;
		// Calculate directly for positive trace.
		if( Trace > 0.f)
		{
			 FLOAT S = appSqrt(Trace);
			 FLOAT SZ = 0.5f / S;
			 W = 0.5f * S;
			 X = ( M.M[1][2] - M.M[2][1] ) * SZ;
			 Y = ( M.M[2][0] - M.M[0][2] ) * SZ;
			 Z = ( M.M[0][1] - M.M[1][0] ) * SZ;
			 return;
		}
		// Or determine the major diagonal element.
		if( (M.M[0][0] > M.M[1][1]) &&  (M.M[0][0] > M.M[2][2]) )
		{
			FLOAT S = appSqrt( 1.0f + M.M[0][0] - M.M[1][1] - M.M[2][2] );
			FLOAT SZ = 0.5f / S;
			X = 0.5f * S;
			Y = (M.M[1][0] + M.M[0][1] ) * SZ;
			Z = (M.M[2][0] + M.M[0][2] ) * SZ;
			W = (M.M[2][1] + M.M[1][2] ) * SZ;			
		}
		else if( M.M[1][1] > M.M[2][2] )
		{
			FLOAT S = appSqrt( 1.0f + M.M[1][1] - M.M[0][0] - M.M[2][2] );
			FLOAT SZ = 0.5f / S;
			X = (M.M[1][0] + M.M[0][1] ) * SZ;
			Y = 0.5f * S;
			Z = (M.M[2][1] + M.M[1][2] ) * SZ;
			W = (M.M[2][0] + M.M[0][2] ) * SZ;
		}
		else 
		{			  
			FLOAT S = appSqrt( 1.0f + M.M[2][2] - M.M[0][0] - M.M[1][1] );
			FLOAT SZ = 0.5f / S;
			X = (M.M[2][0] + M.M[0][2] ) * SZ;
			Y = (M.M[2][1] + M.M[1][2] ) * SZ;
			Z = 0.5f * S;
			W = (M.M[1][0] + M.M[0][1] ) * SZ;
		}
	}
};

// Transform a (rotation) FCoords into a Quaternion.
class FCoordsQuaternion : public FQuat
{
public:
	FCoordsQuaternion( FCoords C )
	{
		// Trace.
		FLOAT Trace = C.XAxis.X + C.YAxis.Y + C.ZAxis.Z + 1.0f;
		// Calculate directly for positive trace.
		if( Trace > 0.f)
		{
			 FLOAT   S = appSqrt(Trace);
			 FLOAT SZ = 0.5f / S;			 
			 X = ( C.ZAxis.Y - C.YAxis.Z ) * SZ;
			 Y = ( C.XAxis.Z - C.ZAxis.X ) * SZ;
			 Z = ( C.YAxis.X - C.XAxis.Y ) * SZ;
			 W = 0.5f * S;
			 return;
		}
		// Or determine the major diagonal element.
		if( (C.XAxis.X > C.YAxis.Y) &&  (C.XAxis.X > C.ZAxis.Z) )
		{
			FLOAT S = appSqrt( 1.0f + C.XAxis.X - C.YAxis.Y - C.ZAxis.Z );
			FLOAT SZ = 0.5f / S;
			X = 0.5f * S;
			Y = (C.XAxis.Y + C.YAxis.X ) * SZ;
			Z = (C.XAxis.Z + C.ZAxis.X ) * SZ;
			W = (C.YAxis.Z + C.ZAxis.Y ) * SZ;			
		}
		else if( C.YAxis.Y > C.ZAxis.Z )
		{
			FLOAT S = appSqrt( 1.0f + C.YAxis.Y - C.XAxis.X - C.ZAxis.Z );
			FLOAT SZ = 0.5f / S;
			X = (C.XAxis.Y + C.YAxis.X ) * SZ;
			Y = 0.5f * S;
			Z = (C.YAxis.Z + C.ZAxis.Y ) * SZ;
			W = (C.XAxis.Z + C.ZAxis.X ) * SZ;
		}
		else 
		{			  
			FLOAT S = appSqrt( 1.0f + C.ZAxis.Z - C.XAxis.X - C.YAxis.Y );
			FLOAT SZ = 0.5f / S;
			X = (C.XAxis.Z + C.ZAxis.X ) * SZ;
			Y = (C.YAxis.Z + C.ZAxis.Y ) * SZ;
			Z = 0.5f * S;
			W = (C.XAxis.Y + C.YAxis.X ) * SZ;
		}
	}
};


class FQuaternionCoords : public FCoords
{
public:
	FQuaternionCoords(FQuat Q)
	{
		FLOAT wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;

		x2 = Q.X + Q.X;  y2 = Q.Y + Q.Y;  z2 = Q.Z + Q.Z;
		xx = Q.X * x2;   xy = Q.X * y2;   xz = Q.X * z2;
		yy = Q.Y * y2;   yz = Q.Y * z2;   zz = Q.Z * z2;
		wx = Q.W * x2;   wy = Q.W * y2;   wz = Q.W * z2;

		XAxis.X = 1.0f - (yy + zz);
		XAxis.Y = xy - wz;
		XAxis.Z = xz + wy;		

		YAxis.X = xy + wz;
		YAxis.Y = 1.0f - (xx + zz);
		YAxis.Z = yz - wx;
		
		ZAxis.X = xz - wy;
		ZAxis.Y = yz + wx;
		ZAxis.Z = 1.0f - (xx + yy);
		
		Origin.X = 0.0f;
		Origin.Y = 0.0f;
		Origin.Z = 0.0f;
	}
};

inline void FQuatToFCoordsFast( FQuat& Q, FVector& P, FCoords& SpaceDest )
{
	// SpaceBases(bone) = FQuaternionCoords( CachedOrientations(bone));
	// SpaceBases(bone).Origin = CachedPositions(bone);
	FLOAT wx, wy, wz, xx, yy, yz, xy, xz, zz, x2, y2, z2;

	x2 = Q.X + Q.X;  y2 = Q.Y + Q.Y;  z2 = Q.Z + Q.Z;
	xx = Q.X * x2;   xy = Q.X * y2;   xz = Q.X * z2;
	yy = Q.Y * y2;   yz = Q.Y * z2;   zz = Q.Z * z2;
	wx = Q.W * x2;   wy = Q.W * y2;   wz = Q.W * z2;

	SpaceDest.XAxis.X = 1.0f - (yy + zz);
	SpaceDest.YAxis.Y = 1.0f - (xx + zz);
	SpaceDest.ZAxis.Z = 1.0f - (xx + yy);

	SpaceDest.XAxis.Y = xy - wz;
	SpaceDest.YAxis.X = xy + wz;

	SpaceDest.XAxis.Z = xz + wy;
	SpaceDest.ZAxis.X = xz - wy;

	SpaceDest.YAxis.Z = yz - wx;
	SpaceDest.ZAxis.Y = yz + wx;

	SpaceDest.Origin = P;
}

/*
	FBox::TransformBy
*/

#if !defined(__MWERKS__) || !defined(__PSX2_EE__) // This function is faster in the .cpp, probably because of cache gets blown when inlined
inline FBox FBox::TransformBy(const FMatrix& M) const
{
	FBox	NewBox(0);

	for(INT X = 0;X < 2;X++)
		for(INT Y = 0;Y < 2;Y++)
			for(INT Z = 0;Z < 2;Z++)
				NewBox += M.TransformFVector(FVector(GetExtrema(X).X,GetExtrema(Y).Y,GetExtrema(Z).Z));

	return NewBox;
}
#endif

/*-----------------------------------------------------------------------------
	FPlane implementation.
-----------------------------------------------------------------------------*/

//
// Transform a point by a coordinate system, moving
// it by the coordinate system's origin if nonzero.
//
inline FPlane FPlane::TransformPlaneByOrtho( const FCoords &Coords ) const
{
	FVector Normal = TransformVectorBy(Coords);
	return FPlane( Normal, W - (Coords.Origin.TransformVectorBy(Coords) | Normal) );
}

inline FPlane FPlane::TransformBy( const FCoords &Coords ) const
{
	return FPlane((*this * W).TransformPointBy(Coords),TransformVectorBy(Coords).SafeNormal());
}

inline FPlane FPlane::TransformPlaneByOrtho( const FMatrix& M ) const
{
	FVector Normal = M.TransformFPlane(FPlane(X,Y,Z,0));
	return FPlane( Normal, W - (M.TransformFVector(FVector(0,0,0)) | Normal) );
}

inline FPlane FPlane::TransformBy( const FMatrix& M ) const
{
	FMatrix tmpTA = M.TransposeAdjoint();
	float DetM = M.Determinant();
	return this->TransformByUsingAdjointT(M, DetM, tmpTA);
}

// You can optionally pass in the matrices transpose-adjoint, which save it recalculating it.
inline FPlane FPlane::TransformByUsingAdjointT( const FMatrix& M, const FLOAT DetM, const FMatrix& TA ) const
{
	FVector newNorm = TA.TransformNormal(*this).SafeNormal();

	if(DetM < 0.f)
		newNorm *= -1;

	return FPlane(M.TransformFVector(*this * W), newNorm);
}

inline FSphere FSphere::TransformBy(const FMatrix& M) const
{
	FSphere	Result;

	(FVector&)Result = M.TransformFVector(*this);

	FVector	XAxis(M.M[0][0],M.M[0][1],M.M[0][2]),
			YAxis(M.M[1][0],M.M[1][1],M.M[1][2]),
			ZAxis(M.M[2][0],M.M[2][1],M.M[2][2]);

	Result.W = appSqrt(MaxPositiveFloat(XAxis|XAxis,MaxPositiveFloat(YAxis|YAxis,ZAxis|ZAxis))) * W;

	return Result;
}

struct FCompressedPosition
{
	FVector Location;
	FRotator Rotation;
	FVector Velocity;
};


#include "UnForcePacking_end.h"


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

