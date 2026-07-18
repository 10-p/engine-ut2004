/*=============================================================================
	UnMath.cpp: Unreal math routines, implementation of FGlobalMath class
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
		* FMatrix (4x4 matrices) and FQuat (quaternions) classes added - Erik de Neve
=============================================================================*/

#include "CorePrivate.h"

// THIS VARIABLE MUST BE 32-BITS, UNSIGNED!  --ryan.
unsigned int qRandSeed = 5318271; // sjs

/*-----------------------------------------------------------------------------
	FGlobalMath constructor.
-----------------------------------------------------------------------------*/

// Constructor.
FGlobalMath::FGlobalMath()
:	WorldMin			(-32700.f,-32700.f,-32700.f),
	WorldMax			(32700.f,32700.f,32700.),
	UnitCoords			(FVector(0,0,0),FVector(1,0,0),FVector(0,1,0),FVector(0,0,1)),
	ViewCoords			(FVector(0,0,0),FVector(0,1,0),FVector(0,0,-1),FVector(1,0,0)),
	UnitScale			(FVector(1,1,1),0.f,SHEER_ZX)
{
	// Init base angle table.
	{for( INT i=0; i<NUM_ANGLES; i++ )
		TrigFLOAT[i] = appSin((FLOAT)i * 2.f * PI / (FLOAT)NUM_ANGLES);}

	// Init square root table.
	{for( INT i=0; i<NUM_SQRTS; i++ )
		SqrtFLOAT[i] = appSqrt((FLOAT)i / 16384.f);}

	// must be 32-bit unsigned or it goes boom in the particle system. --ryan.
	check(sizeof(qRandSeed) == 4);

	qRandSeed = appRand() * appRand(); // sjs
}

/*-----------------------------------------------------------------------------
	Conversion functions.
-----------------------------------------------------------------------------*/

// Return the FRotator corresponding to the direction that the vector
// is pointing in.  Sets Yaw and Pitch to the proper numbers, and sets
// roll to zero because the roll can't be determined from a vector.
FRotator FVector::Rotation()
{
	FRotator R;

	// Find yaw.
	R.Yaw = (INT)(appAtan2(Y,X) * (FLOAT)MAXWORD / (2.f*PI));

	// Find pitch.
	R.Pitch = (INT)(appAtan2(Z,appSqrt(X*X+Y*Y)) * (FLOAT)MAXWORD / (2.f*PI));

	// Find roll.
	R.Roll = 0;

	return R;
}

//
// Find good arbitrary axis vectors to represent U and V axes of a plane
// given just the normal.
//
void FVector::FindBestAxisVectors( FVector& Axis1, FVector& Axis2 )
{
	guard(FindBestAxisVectors);

	FLOAT NX = Abs(X);
	FLOAT NY = Abs(Y);
	FLOAT NZ = Abs(Z);

	// Find best basis vectors.
	if( NZ>NX && NZ>NY )	Axis1 = FVector(1,0,0);
	else					Axis1 = FVector(0,0,1);

	Axis1 = (Axis1 - *this * (Axis1 | *this)).SafeNormal();
	Axis2 = Axis1 ^ *this;

	unguard;
}

/*-----------------------------------------------------------------------------
	Matrix inversion.
-----------------------------------------------------------------------------*/

//
// Coordinate system inverse.
//
FCoords FCoords::Inverse() const
{
	FLOAT Div = FTriple( XAxis, YAxis, ZAxis );
	if( !Div ) Div = 1.f;

	FLOAT RDet = 1.f / Div;
	return FCoords
	(	-Origin.TransformVectorBy(*this)
	,	RDet * FVector
		(	(YAxis.Y * ZAxis.Z - YAxis.Z * ZAxis.Y)
		,	(ZAxis.Y * XAxis.Z - ZAxis.Z * XAxis.Y)
		,	(XAxis.Y * YAxis.Z - XAxis.Z * YAxis.Y) )
	,	RDet * FVector
		(	(YAxis.Z * ZAxis.X - ZAxis.Z * YAxis.X)
		,	(ZAxis.Z * XAxis.X - XAxis.Z * ZAxis.X)
		,	(XAxis.Z * YAxis.X - XAxis.X * YAxis.Z))
	,	RDet * FVector
		(	(YAxis.X * ZAxis.Y - YAxis.Y * ZAxis.X)
		,	(ZAxis.X * XAxis.Y - ZAxis.Y * XAxis.X)
		,	(XAxis.X * YAxis.Y - XAxis.Y * YAxis.X) )
	);
}


//
// Combine two 'pivot' transforms.
//
FCoords FCoords::ApplyPivot(const FCoords& CoordsB) const
{
	// Fast solution assuming orthogonal coordinate system
	FCoords Temp;
	Temp.Origin = CoordsB.Origin + Origin.TransformVectorBy(CoordsB);
	Temp.XAxis = CoordsB.XAxis.TransformVectorBy( *this );
	Temp.YAxis = CoordsB.YAxis.TransformVectorBy( *this );
	Temp.ZAxis = CoordsB.ZAxis.TransformVectorBy( *this );
	return Temp;
}

//
// Invert pivot transformation
//
FCoords FCoords::PivotInverse() const 
{
	FCoords Temp;
	FLOAT RDet = 1.f / FTriple( XAxis, YAxis, ZAxis );

	Temp.XAxis = RDet * FVector
		(	(YAxis.Y * ZAxis.Z - YAxis.Z * ZAxis.Y)
		,	(ZAxis.Y * XAxis.Z - ZAxis.Z * XAxis.Y)
		,	(XAxis.Y * YAxis.Z - XAxis.Z * YAxis.Y));

	Temp.YAxis = RDet * FVector
		(	(YAxis.Z * ZAxis.X - ZAxis.Z * YAxis.X)
		,	(ZAxis.Z * XAxis.X - XAxis.Z * ZAxis.X)
		,	(XAxis.Z * YAxis.X - XAxis.X * YAxis.Z));

	Temp.ZAxis = RDet * FVector
		(	(YAxis.X * ZAxis.Y - YAxis.Y * ZAxis.X)
		,	(ZAxis.X * XAxis.Y - ZAxis.Y * XAxis.X)
		,	(XAxis.X * YAxis.Y - XAxis.Y * YAxis.X));

	FVector TOrigin = -Origin;
	Temp.Origin = TOrigin.TransformVectorBy(Temp);
	return Temp;
}

//
// Convert this orthogonal coordinate system to a rotation.
//
FRotator FCoords::OrthoRotation() const
{
	FRotator R
	(
		(INT)(appAtan2( XAxis.Z, appSqrt(Square(XAxis.X)+Square(XAxis.Y)) ) * 32768.f / PI),
		(INT)(appAtan2( XAxis.Y, XAxis.X                                  ) * 32768.f / PI),
		0
	);
	FCoords S = GMath.UnitCoords / R;
	R.Roll = (INT)(appAtan2( ZAxis | S.YAxis, YAxis | S.YAxis ) * 32768.f / PI);
	return R;
}

//
// Convert this FCoords to a 4x4 matrix
FMatrix FCoords::Matrix() const
{
	FMatrix M;

	M.M[0][0] = XAxis.X;
	M.M[0][1] = YAxis.X;
	M.M[0][2] = ZAxis.X;
	M.M[0][3] = 0.0f;
	M.M[1][0] = XAxis.Y;
	M.M[1][1] = YAxis.Y;
	M.M[1][2] = ZAxis.Y;
	M.M[1][3] = 0.0f;
	M.M[2][0] = XAxis.Z;
	M.M[2][1] = YAxis.Z;
	M.M[2][2] = ZAxis.Z;
	M.M[2][3] = 0.0f;
	M.M[3][0] = XAxis | -Origin;
	M.M[3][1] = YAxis | -Origin;
	M.M[3][2] = ZAxis | -Origin;
	M.M[3][3] = 1.0f;

	return M;
}

/*-----------------------------------------------------------------------------
	FSphere implementation.
-----------------------------------------------------------------------------*/

//
// Compute a bounding sphere from an array of points.
//
FSphere::FSphere( const FVector* Pts, INT Count )
: FPlane(0,0,0,0)
{
	guard(FSphere::FSphere);
	if( Count )
	{
		FBox Box( Pts, Count );
		*this = FSphere( (Box.Min+Box.Max)/2, 0 );
		for( INT i=0; i<Count; i++ )
		{
			FLOAT Dist = FDistSquared(Pts[i],*this);
			if( Dist > W )
				W = Dist;
		}
		W = appSqrt(W) * 1.001f;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	FBox implementation.
-----------------------------------------------------------------------------*/

FBox::FBox( const FVector* Points, INT Count )
: Min(0,0,0), Max(0,0,0), IsValid(0)
{
	guard(FBox::FBox);
#if defined(__MWERKS__) && defined(__PSX2_EE__)
	if( Count == 2 )
	{
		FVector AlignedMax GCC_PACK(16);
		FVector AlignedMin GCC_PACK(16);
		FPlane alignedPoint[2];
		alignedPoint[0].X = Points[0].X;
		alignedPoint[0].Y = Points[0].Y;
		alignedPoint[0].Z = Points[0].Z;		
		alignedPoint[1].X = Points[1].X;
		alignedPoint[1].Y = Points[1].Y;
		alignedPoint[1].Z = Points[1].Z;		
		MaxMinVector( AlignedMax, AlignedMin, alignedPoint, Count );
		Min = AlignedMin;
		Max = AlignedMax;
		return;
	}
#endif
	for( INT i=0; i<Count; i++ )
		*this += Points[i];
	unguard;
}

#if defined(__MWERKS__) && defined(__PSX2_EE__)
FBox FBox::TransformBy(const FMatrix& M) const
{

		FPlane ResultMin[2] __attribute__((aligned(16)));
		FPlane InMin[2] __attribute__((aligned(16)));

		// copy the input box into the aligned min and max.
		InMin[0]=this->Min;
		InMin[1]=this->Max;
// Done in macro code now		
//		InMin[0].W=1.0f;
//		InMin[1].W=1.0f;

	asm volatile
	(
		".set noreorder\n"

		"lqc2		vf1, 0(%0)\n"
		"lqc2		vf7, 16(%0)\n"

		"lqc2		vf3, 0(%2)\n"
		"lqc2		vf4, 16(%2)\n"
		"lqc2		vf5, 32(%2)\n"
		"lqc2		vf6, 48(%2)\n"
			
		"vmulax.xyzw	ACC, vf3, vf1x\n"
		"vmadday.xyzw ACC, vf4, vf1y\n"
		"vmaddaz.xyzw ACC, vf5, vf1z\n"
		"vmaddw.xyzw	vf2, vf6, vf0w\n"
			
		"vmulax.xyzw	ACC, vf3, vf7x\n"
		"vmadday.xyzw ACC, vf4, vf7y\n"
		"vmaddaz.xyzw ACC, vf5, vf7z\n"
		"vmaddw.xyzw	vf8, vf6, vf0w\n"
			
		"vmini		vf9, vf2, vf8		# pull out min\n"
		"vmax		vf10, vf2, vf8		# pull out max\n"

		"sqc2		vf9, 0(%1)\n"
		"sqc2		vf10, 16(%1)\n"

		".set reorder\n"
		:
		: "r" (InMin), "r" (ResultMin), "r" (&M)
	);
		// copy the result min and max back out to result box.
		return FBox(ResultMin[0],ResultMin[1]);
}
#endif


/*-----------------------------------------------------------------------------
	FQuaternion and FMatrix support functions
-----------------------------------------------------------------------------*/

FMatrix	FMatrix::Identity(FPlane(1,0,0,0),FPlane(0,1,0,0),FPlane(0,0,1,0),FPlane(0,0,0,1));

CORE_API FQuat FQuatFindBetween(const FVector& vec1, const FVector& vec2)
{
	guard(FQuatFindBetween);

	FVector cross = vec1 ^ vec2;
	FLOAT crossMag = cross.Size();

	// If these vectors are basically parallel - just return identity quaternion (ie no rotation).
	if(crossMag < KINDA_SMALL_NUMBER)
	{
		return FQuat(0.0f, 0.0f, 0.0f, 1.0f);
	}

	FLOAT angle = appAsin(crossMag);

	FLOAT dot = vec1 | vec2;
	if(dot < 0.0f)
		angle = PI - angle;

	FLOAT sinHalfAng = appSin(0.5f * angle);
	FLOAT cosHalfAng = appCos(0.5f * angle);
	FVector axis = cross / crossMag;

	return FQuat(
		sinHalfAng * axis.X,
		sinHalfAng * axis.Y,
		sinHalfAng * axis.Z,
		cosHalfAng );

	unguard;
}


/* Line-extent/Box Test Util */
#define BOX_SIDE_THRESHOLD	0.1f

CORE_API UBOOL FLineExtentBoxIntersection(const FBox& inBox, 
								 const FVector& Start, 
								 const FVector& End,
								 const FVector& Extent,
								 FVector& HitLocation,
								 FVector& HitNormal,
								 FLOAT& HitTime)
{
	guard(FLineExtentBoxIntersection);

	FBox box = inBox;
	box.Max.X += Extent.X;
	box.Max.Y += Extent.Y;
	box.Max.Z += Extent.Z;
	
	box.Min.X -= Extent.X;
	box.Min.Y -= Extent.Y;
	box.Min.Z -= Extent.Z;

	FVector Dir = (End - Start);
	
	FVector	Time;
	UBOOL	Inside = 1;
	FLOAT   faceDir[3] = {1, 1, 1};
	
	/////////////// X
	if(Start.X < box.Min.X)
	{
		if(Dir.X <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			faceDir[0] = -1;
			Time.X = (box.Min.X - Start.X) / Dir.X;
		}
	}
	else if(Start.X > box.Max.X)
	{
		if(Dir.X >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.X = (box.Max.X - Start.X) / Dir.X;
		}
	}
	else
		Time.X = 0.0f;
	
	/////////////// Y
	if(Start.Y < box.Min.Y)
	{
		if(Dir.Y <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			faceDir[1] = -1;
			Time.Y = (box.Min.Y - Start.Y) / Dir.Y;
		}
	}
	else if(Start.Y > box.Max.Y)
	{
		if(Dir.Y >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Y = (box.Max.Y - Start.Y) / Dir.Y;
		}
	}
	else
		Time.Y = 0.0f;
	
	/////////////// Z
	if(Start.Z < box.Min.Z)
	{
		if(Dir.Z <= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			faceDir[2] = -1;
			Time.Z = (box.Min.Z - Start.Z) / Dir.Z;
		}
	}
	else if(Start.Z > box.Max.Z)
	{
		if(Dir.Z >= 0.0f)
			return 0;
		else
		{
			Inside = 0;
			Time.Z = (box.Max.Z - Start.Z) / Dir.Z;
		}
	}
	else
		Time.Z = 0.0f;
	
	// If the line started inside the box (ie. player started in contact with the fluid)
	if(Inside)
	{
		HitLocation = Start;
		HitNormal = FVector(0, 0, 1);
		HitTime = 0;
		return 1;
	}
	// Otherwise, calculate when hit occured
	else
	{	
		if(Time.Y > Time.Z)
		{
			HitTime = Time.Y;
			HitNormal = FVector(0, faceDir[1], 0);
		}
		else
		{
			HitTime = Time.Z;
			HitNormal = FVector(0, 0, faceDir[2]);
		}
		
		if(Time.X > HitTime)
		{
			HitTime = Time.X;
			HitNormal = FVector(faceDir[0], 0, 0);
		}
		
		if(HitTime >= 0.0f && HitTime <= 1.0f)
		{
			HitLocation = Start + Dir * HitTime;
			
			if(	HitLocation.X > box.Min.X - BOX_SIDE_THRESHOLD && HitLocation.X < box.Max.X + BOX_SIDE_THRESHOLD &&
				HitLocation.Y > box.Min.Y - BOX_SIDE_THRESHOLD && HitLocation.Y < box.Max.Y + BOX_SIDE_THRESHOLD &&
				HitLocation.Z > box.Min.Z - BOX_SIDE_THRESHOLD && HitLocation.Z < box.Max.Z + BOX_SIDE_THRESHOLD)
			{				
				return 1;
			}
		}
		
		return 0;
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	FInterpCurve implementation.
-----------------------------------------------------------------------------*/

FInterpCurveInit::FInterpCurveInit()
{
	appMemzero( &Points, sizeof(Points) ); // yuck.
}

	
void FInterpCurve::AddPoint( FLOAT inV, FLOAT outV )
{
	guard(FInterpCurve::AddPoint);

	// Iterate along list to find position to insert new point at.
	INT i=0;
	for(i=0; i<Points.Num() && Points(i).InVal < inV; i++);

	Points.Insert(i);
	Points(i) = FInterpCurvePoint(inV, outV);

	unguard;
}

FLOAT FInterpCurve::Eval( FLOAT in )
{
	guard(FInterpCurve::Eval);

	// If input is off the ends of our curve, return 0.
	// Curves with less than two points are invalid, and always return 0.
	if(Points.Num() < 2 || in < Points(0).InVal || in > Points(Points.Num()-1).InVal)
		return 0;

	INT i;
	for(i=1; i<Points.Num(); i++)
	{
		if(Points(i).InVal >= in)
		{
			FLOAT Alpha = (in - Points(i-1).InVal)/(Points(i).InVal - Points(i-1).InVal);
			check(Alpha >= 0 && Alpha <= 1);
			return Lerp(Points(i-1).OutVal, Points(i).OutVal, Alpha);
		}
	}

	return 0; // Shouldn't get here!

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

