/*=============================================================================
	UnRenderUtil.cpp: Rendering utility implementations.
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

#if !__PSX2_EE__ && !__GCN__ && !__LINUX__ && !__FreeBSD__ && !MACOSX && !_XBOX && !_WIN64
#  include "NVTriStrip.h"
#  ifndef HAS_NVTRISTRIP
#    define HAS_NVTRISTRIP 1
#  endif
#endif

//WD: added CONST_INT32_PS for F32vec4 classes and masking convenience respectively.
#if __HAS_SSE__
#define CONST_INT32_PS(N, V3, V2, V1, V0) \
	static const _MM_ALIGN16 int _##N[] = { V0, V1, V2, V3 }; /*little endian!*/ \
	const F32vec4 N = _mm_load_ps((float*)_##N);
#endif // #if __INTEL__


//
//  FConvexVolume::FConvexVolume
//

FConvexVolume::FConvexVolume()
{
	NumPlanes = 0;
}

//
//  FConvexVolume::SphereCheck
//

BYTE FConvexVolume::SphereCheck(const FSphere& Sphere)
{
	BYTE	ClippingFlags = CF_Inside;

//WD: added Pentium(r) 4 processor optimized version of SphereCheck()
#if __HAS_SSE__
	//if (GIsPentium4ProcessorBased && NumPlanes >= 4)
	if (GIsSSE && NumPlanes >= 4)
	{
		F32vec4 mask;
		F32vec4 dist;
		F32vec4 bp_x, bp_y, bp_z, bp_w;
		F32vec4 s_x, s_y, s_z, s_w;
		F32vec4 tr0, tr1, tr2;

		INT Index;
		INT remainder = NumPlanes % 4;
		INT count = NumPlanes - remainder;

		s_x = _mm_load1_ps(&Sphere.X);
		s_y = _mm_load1_ps(&Sphere.Y);
		s_z = _mm_load1_ps(&Sphere.Z);
		s_w = _mm_load1_ps(&Sphere.W);

		for (Index = 0; Index < count; Index+=4)
		{
			// SIMD init of bounding planes' components
			tr0  = _mm_loadl_pi(tr0, (__m64*)&BoundingPlanes[Index].X);   // -- -- y0 x0
			tr0  = _mm_loadh_pi(tr0, (__m64*)&BoundingPlanes[Index+1].X); // y1 x1 y0 x0
			tr1  = _mm_loadl_pi(tr1, (__m64*)&BoundingPlanes[Index+2].X); // -- -- y2 x2
			tr1  = _mm_loadh_pi(tr1, (__m64*)&BoundingPlanes[Index+3].X); // y3 x3 y2 x2
			bp_x = _mm_shuffle_ps(tr0, tr1, _MM_SHUFFLE(2, 0, 2, 0));     // x3 x2 x1 x0
			bp_y = _mm_shuffle_ps(tr0, tr1, _MM_SHUFFLE(3, 1, 3, 1));     // y3 y2 y1 y0
			tr0  = _mm_loadl_pi(tr0, (__m64*)&BoundingPlanes[Index].Z);   // -- -- w0 z0
			tr0  = _mm_loadh_pi(tr0, (__m64*)&BoundingPlanes[Index+1].Z); // w1 z1 w0 z0
			tr1  = _mm_loadl_pi(tr1, (__m64*)&BoundingPlanes[Index+2].Z); // -- -- w2 z2
			tr1  = _mm_loadh_pi(tr1, (__m64*)&BoundingPlanes[Index+3].Z); // w3 z3 w2 z2
			bp_z = _mm_shuffle_ps(tr0, tr1, _MM_SHUFFLE(2, 0, 2, 0));     // z3 z2 z1 z0
			bp_w = _mm_shuffle_ps(tr0, tr1, _MM_SHUFFLE(3, 1, 3, 1));     // w3 w2 w1 w0

			// Bounding plane distance to sphere (p.X*o.X + p.Y*o.Y + p.Z*o.Z - p.W)
			dist = _mm_sub_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(bp_x, s_x), _mm_mul_ps(bp_y, s_y)), _mm_mul_ps(bp_z, s_z)), bp_w);

			mask = cmpgt(dist, _mm_sub_ps(_mm_setzero_ps(), s_w));
			if (move_mask(mask)) // if at least one is true...
			{
				// At least one sphere is partially outside.
				ClippingFlags |= CF_Outside;

				mask = cmpgt(dist, s_w);
				if (move_mask(mask))
				{
					// At least one sphere is entirely outside.
					ClippingFlags &= ~CF_Inside;
					break;
				}
			}
		}
		if (remainder && (ClippingFlags & CF_Inside))
		{
			for (Index; Index < NumPlanes; Index++)
			{
				FLOAT Dist = BoundingPlanes[Index].PlaneDot(Sphere);

				if(Dist > -Sphere.W)
				{
					// The sphere is partially outside.
					ClippingFlags |= CF_Outside;

					if(Dist > Sphere.W)
					{
						// The sphere is entirely outside.
						ClippingFlags &= ~CF_Inside;
						break; // SL added
					}
				}
			}
		}
	}
	else // !GIsSSE || NumPlanes < 4
#endif // #if __INTEL__
	{
		for(INT Index = 0;Index < NumPlanes;Index++)
		{
			FLOAT Dist		= BoundingPlanes[Index].PlaneDot(Sphere);

			if(Dist > -Sphere.W)
			{
				// The sphere is partially outside.

				ClippingFlags |= CF_Outside;

				if(Dist > Sphere.W)
				{
					// The sphere is entirely outside.

					ClippingFlags &= ~CF_Inside;
					break; // SL added
				}
			}
		}
	}

	return ClippingFlags;
}
//WD: Don't compile original code...  I left this code here for Andrew's (or another developer's) convenience.
#if 0
{
	BYTE ClippingFlags	= CF_Inside;

	for(INT Index = 0;Index < NumPlanes;Index++)
	{
		FLOAT	Dist = BoundingPlanes[Index].PlaneDot(Sphere);

		if(Dist > -Sphere.W)
		{
			// The sphere is partially outside.

			ClippingFlags |= CF_Outside;

			if(Dist > Sphere.W)
			{
				// The sphere is entirely outside.

				ClippingFlags &= ~CF_Inside;
				 break; // SL added
			}
		}
	}

	return ClippingFlags;
}
#endif

//
//  FConvexVolume::BoxCheck
//

#if MACOSX // optimized for Altivec by Sanjay Patel at Apple.
BYTE FConvexVolume::BoxCheck_altivec(const FVector &Origin,const FVector &Extent)
{
	BYTE	ClippingFlags = CF_Inside;
	vector float ExtentX, ExtentY, ExtentZ;
	vector float PlaneX, PlaneY, PlaneZ;
	vector float OriginX, OriginY, OriginZ;
	vector float temp, temp2;
	typedef union {
	vector float v;
		float f[4];
	} vectorFloatUnion;
	vectorFloatUnion vfUnion;
    
	INT remainder = NumPlanes % 4;
	//INT count = NumPlanes - remainder;//we don't need the modulo4 count
	INT Index;

	// do scalar work until we can check 4 planes at a time
	for (Index = 0; Index < remainder; Index++)
	{
		FPlane& Plane   = BoundingPlanes[Index];
		FLOAT Dist    = Plane.PlaneDot(Origin);
		FLOAT PushOut = fabsf(Extent.X*Plane.X) + fabsf(Extent.Y*Plane.Y) + fabsf(Extent.Z*Plane.Z);

		if(Dist > -PushOut)
		{
			// The sphere is partially outside.
			ClippingFlags |= CF_Outside;

			if(Dist > PushOut)
			{
				// The sphere is entirely outside.
				ClippingFlags &= ~CF_Inside;
				return ClippingFlags; // SKP changed - return if we already know this
			}
		}
	}
    
	// get ready to process 4 planes in parallel

	// get the extent coordinates into vector registers
	temp = vec_ld(0,&(Extent.X));
	// if Extent straddles two 16-byte chunks, we have to load the next chunk
	if( (unsigned int)&(Extent.X) & 0x8) {
		temp2 = vec_ld(16,&(Extent.X));
	}
	// if Extent.[XYZ] is contained in an aligned 16-byte chunk,
	// we don't care what's in temp2
	temp = vec_perm(temp,temp2,vec_lvsl(0,&(Extent.X)));

	ExtentX = vec_splat(temp, 0);
	ExtentY = vec_splat(temp, 1);
	ExtentZ = vec_splat(temp, 2);
	// get abs value of Extent because we only care about magnitude in the loop
	ExtentX = vec_abs(ExtentX);
	ExtentY = vec_abs(ExtentY);
	ExtentZ = vec_abs(ExtentZ);

	// get the origin coordinates into vector registers
	temp = vec_ld(0,&(Origin.X));
	// if Origin.[XYZ] straddles two 16-byte chunks, we have to load the next chunk
	if( (unsigned int)&(Origin.X) & 0x8) {
		temp2 = vec_ld(16,&(Origin.X));
	}
	// if Origin.[XYZ] is contained in an aligned 16-byte chunk,
	// we don't care what's in temp2
	temp = vec_perm(temp,temp2,vec_lvsl(0,&(Origin.X)));
	OriginX = vec_splat(temp, 0);
	OriginY = vec_splat(temp, 1);
	OriginZ = vec_splat(temp, 2);

	for ( ; Index < NumPlanes; Index+=4)
	{
		vector float v0,v1,v2,v3,vt;
		vector float vI0, vI1, vI2, vI3;
		vector float PlaneX, PlaneY, PlaneZ, PlaneW;
		vector float Dist, PushOut, minusPushOut;
		vector float zero = (vector float)(0.0f);

		FPlane& Plane0   = BoundingPlanes[Index+0];
		FPlane& Plane1   = BoundingPlanes[Index+1];
		FPlane& Plane2   = BoundingPlanes[Index+2];
		FPlane& Plane3   = BoundingPlanes[Index+3];

		// load the plane coordinates into vector registers
		v0 = vec_ld(0,&(Plane0.X));
		if((unsigned int)&(Plane0.X) & 0xf) {
			vt = vec_ld(16,&(Plane0.X));
			v0 = vec_perm(v0,vt,vec_lvsl(0,&(Plane0.X)));
		}
		v1 = vec_ld(0,&(Plane1.X));
		if((unsigned int)&(Plane1.X) & 0xf) {
			vt = vec_ld(16,&(Plane1.X));
			v1 = vec_perm(v1,vt,vec_lvsl(0,&(Plane1.X)));
		}
		v2 = vec_ld(0,&(Plane2.X));
		if((unsigned int)&(Plane2.X) & 0xf) {
			vt = vec_ld(16,&(Plane2.X));
			v2 = vec_perm(v2,vt,vec_lvsl(0,&(Plane2.X)));
		}
		v3 = vec_ld(0,&(Plane3.X));
		if((unsigned int)&(Plane3.X) & 0xf) {
			vt = vec_ld(16,&(Plane3.X));
			v3 = vec_perm(v3,vt,vec_lvsl(0,&(Plane3.X)));
		}

		// transpose matrix - XYZW vectors into XXXX, YYYY, ZZZZ, WWWW
		vI0 = vec_mergeh ( v0, v2 );
		vI1 = vec_mergeh ( v1, v3 );
		vI2 = vec_mergel ( v0, v2 );
		vI3 = vec_mergel ( v1, v3 );

		PlaneX = vec_mergeh ( vI0, vI1 );
		PlaneY = vec_mergel ( vI0, vI1 );
		PlaneZ = vec_mergeh ( vI2, vI3 );
		PlaneW = vec_mergel ( vI2, vI3 );

		// do some math
		temp = vec_sub(zero, PlaneW);
		temp = vec_madd(OriginX, PlaneX, temp);
		temp = vec_madd(OriginY, PlaneY, temp);
		Dist = vec_madd(OriginZ, PlaneZ, temp);

		PlaneX = vec_abs(PlaneX);
		PlaneY = vec_abs(PlaneY);
		PlaneZ = vec_abs(PlaneZ);
		PushOut = vec_madd(ExtentX, PlaneX, zero);
		PushOut = vec_madd(ExtentY, PlaneY, PushOut);
		PushOut = vec_madd(ExtentZ, PlaneZ, PushOut);
		minusPushOut = vec_sub(zero, PushOut);

		if(vec_any_gt(Dist, minusPushOut))
		{
			ClippingFlags |= CF_Outside;
			if(vec_any_gt(Dist, PushOut))
			{
				ClippingFlags &= ~CF_Inside;
				break;
			}
		}
	}
	return ClippingFlags;
}
#endif // MACOSX


BYTE FConvexVolume::BoxCheck(const FVector& Origin,const FVector& Extent)
{
#if MACOSX
	// Apparently, this needs to be in a separate function, or GCC will
	//  emit the Altivec "mfspr vrsav" instruction at the start of this
	//  function, which will crash non-Altivec PowerPCs, defeating the
	//  purpose...   --ryan.
	if (GIsAltivec)
		return(BoxCheck_altivec(Origin, Extent));
#endif

	BYTE	ClippingFlags = CF_Inside;

//WD: added Pentium(r) 4 processor optimized version of BoxCheck()
#if __HAS_SSE__
	//if (GIsPentium4ProcessorBased && NumPlanes >= 4)
	if (GIsSSE && NumPlanes >= 4)
	{
		CONST_INT32_PS(smask, ~(1<<31), ~(1<<31), ~(1<<31), ~(1<<31));

		F32vec4 mask,
				dist,
				pushout,
				bp_x, bp_y, bp_z, bp_w, // 4 bounding planes' x, y, z, and w components
				e_x, e_y, e_z, // extent's components, broadcasted across xmm registers
				o_x, o_y, o_z, // origin's components, broadcasted across xmm registers
				tr0, tr1, tr2; // temp registers 0-2

		// SIMD init of Origin vector's components
		o_x = _mm_load1_ps(&Origin.X);
		o_y = _mm_load1_ps(&Origin.Y);
		o_z = _mm_load1_ps(&Origin.Z);

		// SIMD init of extent (Normal) vector's components
		e_x = _mm_load1_ps(&Extent.X);
		e_y = _mm_load1_ps(&Extent.Y);
		e_z = _mm_load1_ps(&Extent.Z);

		INT remainder = NumPlanes % 4;
		INT count = NumPlanes - remainder;
		INT Index;

		for (Index = 0; Index < count; Index+=4)
		{
			// SIMD init of bounding planes' components
			tr0  = _mm_loadl_pi(tr0, (__m64*)&BoundingPlanes[Index].X);   // -- -- y0 x0
			tr0  = _mm_loadh_pi(tr0, (__m64*)&BoundingPlanes[Index+1].X); // y1 x1 y0 x0
			tr1  = _mm_loadl_pi(tr1, (__m64*)&BoundingPlanes[Index+2].X); // -- -- y2 x2
			tr1  = _mm_loadh_pi(tr1, (__m64*)&BoundingPlanes[Index+3].X); // y3 x3 y2 x2
			bp_x = _mm_shuffle_ps(tr0, tr1, _MM_SHUFFLE(2, 0, 2, 0));     // x3 x2 x1 x0
			bp_y = _mm_shuffle_ps(tr0, tr1, _MM_SHUFFLE(3, 1, 3, 1));     // y3 y2 y1 y0
			tr0  = _mm_loadl_pi(tr0, (__m64*)&BoundingPlanes[Index].Z);   // -- -- w0 z0
			tr0  = _mm_loadh_pi(tr0, (__m64*)&BoundingPlanes[Index+1].Z); // w1 z1 w0 z0
			tr1  = _mm_loadl_pi(tr1, (__m64*)&BoundingPlanes[Index+2].Z); // -- -- w2 z2
			tr1  = _mm_loadh_pi(tr1, (__m64*)&BoundingPlanes[Index+3].Z); // w3 z3 w2 z2
			bp_z = _mm_shuffle_ps(tr0, tr1, _MM_SHUFFLE(2, 0, 2, 0));     // z3 z2 z1 z0
			bp_w = _mm_shuffle_ps(tr0, tr1, _MM_SHUFFLE(3, 1, 3, 1));     // w3 w2 w1 w0

			// Bounding plane distance to origin (p.X*o.X + p.Y*o.Y + p.Z*o.Z - p.W)
			dist = _mm_sub_ps(_mm_add_ps(_mm_add_ps(_mm_mul_ps(bp_x, o_x), _mm_mul_ps(bp_y, o_y)), _mm_mul_ps(bp_z, o_z)), bp_w);

			// Push out distance (fabs(Extent.X*Plane.X) + fabs(Extent.Y*Plane.Y) + fabs(Extent.Z*Plane.Z))
			pushout = _mm_add_ps(_mm_add_ps(_mm_and_ps(smask, _mm_mul_ps(bp_x, e_x)), _mm_and_ps(smask, _mm_mul_ps(bp_y, e_y))), _mm_and_ps(smask, _mm_mul_ps(bp_z, e_z)));

			mask = cmpgt(dist, _mm_sub_ps(_mm_setzero_ps(), pushout));
			if (move_mask(mask)) // if at least one is true...
			{
				// ...at least one box is partially outside
				ClippingFlags |= CF_Outside;
				
				mask = cmpgt(dist, pushout);
				if (move_mask(mask))  // if at least one is true...
				{
					// ...at least one box is entirely outside
					ClippingFlags &= ~CF_Inside;
					break;
				}
			}
		}
		if (remainder && (ClippingFlags & CF_Inside))
		{
			for (Index; Index < NumPlanes; Index++)
			{
				FPlane& Plane   = BoundingPlanes[Index];

				FLOAT Dist    = Plane.PlaneDot(Origin);
				FLOAT PushOut = fabs(Extent.X*Plane.X) + fabs(Extent.Y*Plane.Y) + fabs(Extent.Z*Plane.Z);

				if(Dist > -PushOut)
				{
					// The box is partially outside.

					ClippingFlags |= CF_Outside;

					if(Dist > PushOut)
					{
						// The box is entirely outside.

						ClippingFlags &= ~CF_Inside;
						break; // SL added
					}
				}
			}
		}
	}
	else // !GIsSSE || NumPlanes < 4
#endif // #if __INTEL__
	{
		for (INT Index = 0; Index < NumPlanes; Index++)
		{
			FPlane& Plane   = BoundingPlanes[Index];
			//WD: removed 1 level of function calling 
/*
			FLOAT   Dist    = Plane.PlaneDot(Origin),
			PushOut = FBoxPushOut(Extent,Plane);
*/
			FLOAT Dist    = Plane.PlaneDot(Origin);
			FLOAT PushOut = fabs(Extent.X*Plane.X) + fabs(Extent.Y*Plane.Y) + fabs(Extent.Z*Plane.Z);

			if(Dist > -PushOut)
			{
				// The sphere is partially outside.

				ClippingFlags |= CF_Outside;

				if(Dist > PushOut)
				{
					// The sphere is entirely outside.

					ClippingFlags &= ~CF_Inside;
					break; // SL added
				}
			}
		}
	}

	return ClippingFlags;
}
//WD: Don't compile original code...  I left this code here for Andrew's (or another developer's) convenience.
#if 0
{
	BYTE ClippingFlags	= CF_Inside;

	for(INT Index = 0;Index < NumPlanes;Index++)
	{
		FPlane&	Plane = BoundingPlanes[Index];
		FLOAT	Dist = Plane.PlaneDot(Origin),
				PushOut = FBoxPushOut(Extent,Plane);

		if(Dist > -PushOut)
		{
			// The sphere is partially outside.

			ClippingFlags |= CF_Outside;

			if(Dist > PushOut)
			{
				// The sphere is entirely outside.

				ClippingFlags &= ~CF_Inside;
				 break; // SL added
			}
		}
	}

	return ClippingFlags;
}
#endif

//
//  FConvexVolume::ClipPolygon
//

FPoly FConvexVolume::ClipPolygon(FPoly Polygon)
{
	for(INT PlaneIndex = 0;PlaneIndex < NumPlanes;PlaneIndex++)
	{
		FPlane&	Plane = BoundingPlanes[PlaneIndex];

		if(!Polygon.Split(-FVector(Plane),Plane * Plane.W,1))
		{
			Polygon.NumVertices = 0;
			return Polygon;
		}
	}

	return Polygon;
}

//
// Temp Line Batcher
//

ENGINE_API FTempLineBatcher* GTempLineBatcher;


void FTempLineBatcher::Render(FRenderInterface* InRI, UBOOL InZTest)
{
	FLineBatcher kLines(InRI, InZTest);
	
	// lines
	for(INT i=0; i<LineStart.Num(); i++)
	{
		kLines.DrawLine(LineStart(i), LineEnd(i), LineColor(i));
	}

	for(INT i=0; i<StayingLineStart.Num(); i++)
	{
		kLines.DrawLine(StayingLineStart(i), StayingLineEnd(i), StayingLineColor(i));
	}

	// boxes
	for(INT i=0; i<BoxArray.Num(); i++)
	{
		kLines.DrawBox(BoxArray(i), BoxColor(i));
	}

	// circles
	for(INT i=0; i<CircleBase.Num(); i++)
	{
		kLines.DrawCircle(CircleBase(i), CircleX(i), CircleY(i), CircleColor(i), CircleRadius(i), CircleNumSides(i));
	}

	// spheres
	for(INT i=0; i<SphereBase.Num(); i++)
	{
		kLines.DrawSphere(SphereBase(i), SphereColor(i), SphereRadius(i), SphereNumDivisions(i));
	}
	
	kLines.Flush();
	
	// Empty buffers for next frame.
	LineStart.Empty();
	LineEnd.Empty();
	LineColor.Empty();
	
	BoxArray.Empty();
	BoxColor.Empty();

	CircleBase.Empty();
	CircleX.Empty();
	CircleY.Empty();
	CircleColor.Empty();
	CircleRadius.Empty();
	CircleNumSides.Empty();

	SphereBase.Empty();
	SphereColor.Empty();
	SphereRadius.Empty();
	SphereNumDivisions.Empty();
};

//
//  FLineBatcher::FLineBatcher
//

FLineBatcher::FLineBatcher( FRenderInterface* InRI, UBOOL InZTest )
{
	RI = InRI;
	ZTest = InZTest;

	CacheId = MakeCacheID(CID_RenderVertices);
}

//
//  FLineBatcher::~FLineBatcher
//

FLineBatcher::~FLineBatcher()
{
	Flush();
}

//
//  FLineBatcher::Flush
//

void FLineBatcher::Flush( DWORD PolyFlags )
{
	guard(FLineBatcher::Flush);


	//!!MAT do something with PolyFlags


	if(Vertices.Num())
	{
		INT	BaseVertexIndex = RI->SetDynamicStream(VS_FixedFunction,this);

		static FSolidColorTexture	WhiteTexture(FColor(255,255,255));
		
		DECLARE_STATIC_UOBJECT( UProxyBitmapMaterial, HACKGAH, { HACKGAH->SetTextureInterface(&WhiteTexture); } );
		DECLARE_STATIC_UOBJECT( UFinalBlend, LineMaterial, { LineMaterial->Material = HACKGAH; } );
		LineMaterial->ZTest = ZTest;

		RI->EnableLighting(0,1);
		RI->SetMaterial(LineMaterial);
		RI->SetIndexBuffer(NULL,0);

		RI->DrawPrimitive(PT_LineList,BaseVertexIndex,Vertices.Num() / 2,0,Vertices.Num() - 1);

		Vertices.Empty();
	}

	unguard;
}

//
//  FLineBatcher::DrawLine
//

void FLineBatcher::DrawLine(FVector P1,FVector P2,FColor Color)
{
	guard(FLineBatcher::DrawLine);

	Color = Color.RenderColor();

	new(Vertices) FLineVertex(P1,Color);
	new(Vertices) FLineVertex(P2,Color);

	unguard;
}

//
//  FLineBatcher::DrawPoint
//

void FLineBatcher::DrawPoint(FSceneNode* SceneNode,FVector P,FColor Color)
{
	guard(FLineBatcher::DrawPoint);

	Color = Color.RenderColor();

	FLOAT	W = P.X * SceneNode->WorldToScreen.M[0][3] + P.Y * SceneNode->WorldToScreen.M[1][3] + P.Z * SceneNode->WorldToScreen.M[2][3] + SceneNode->WorldToScreen.M[3][3];
	FVector	X = SceneNode->CameraX * (W * 0.5f),
			Y = SceneNode->CameraY * (W * 0.5f);

	DrawLine(P - X - Y,P + X - Y,Color);
	DrawLine(P + X - Y,P + X + Y,Color);
	DrawLine(P + X + Y,P - X + Y,Color);
	DrawLine(P - X + Y,P - X - Y,Color);

	unguard;
}

//
//  FLineBatcher::DrawBox
//

void FLineBatcher::DrawBox(FBox Box,FColor Color)
{
	guard(FLineBatcher::DrawBox);

	Color = Color.RenderColor();

	FVector	B[2],P,Q;
	int i,j;

	B[0]=Box.Min;
	B[1]=Box.Max;

	for( i=0; i<2; i++ ) for( j=0; j<2; j++ )
	{
		P.X=B[i].X; Q.X=B[i].X;
		P.Y=B[j].Y; Q.Y=B[j].Y;
		P.Z=B[0].Z; Q.Z=B[1].Z;
		DrawLine(P,Q,Color);

		P.Y=B[i].Y; Q.Y=B[i].Y;
		P.Z=B[j].Z; Q.Z=B[j].Z;
		P.X=B[0].X; Q.X=B[1].X;
		DrawLine(P,Q,Color);

		P.Z=B[i].Z; Q.Z=B[i].Z;
		P.X=B[j].X; Q.X=B[j].X;
		P.Y=B[0].Y; Q.Y=B[1].Y;
		DrawLine(P,Q,Color);
	}

	unguard;
}

//
//  FLineBatcher::DrawCircle
//

void FLineBatcher::DrawCircle(FVector Base,FVector X,FVector Y,FColor Color,FLOAT Radius,INT NumSides)
{
	guard(FLineBatcher::DrawCircle);

	Color = Color.RenderColor();

	FLOAT	AngleDelta = 2.0f * PI / NumSides;
	FVector	LastVertex = Base + X * Radius;

	for(INT SideIndex = 0;SideIndex < NumSides;SideIndex++)
	{
		FVector	Vertex = Base + (X * appCos(AngleDelta * (SideIndex + 1)) + Y * appSin(AngleDelta * (SideIndex + 1))) * Radius;

		DrawLine(LastVertex,Vertex,Color);

		LastVertex = Vertex;
	}

	unguard;
}

//
//  FLineBatcher::DrawSphere -- Dave@Psyonix
//

void FLineBatcher::DrawSphere(FVector Base,FColor Color,FLOAT Radius,INT NumDivisions)
{
	guard(FLineBatcher::DrawSphere);

	Color = Color.RenderColor();

	FLOAT	AngleDelta = 2.0f * PI / NumDivisions;
	FLOAT	SegmentDist = SegmentDist = 2.0 * Radius / NumDivisions;

	// Horizontal circles change in scale
	for(INT SideIndex = -NumDivisions/2; SideIndex < NumDivisions/2; SideIndex++)
		DrawCircle(Base - FVector(0,0,SideIndex*SegmentDist), FVector(1,0,0), FVector(0,1,0), Color, appSqrt(Radius*Radius - SideIndex*SegmentDist*SideIndex*SegmentDist), NumDivisions);

	// Vertical circles change in angle
	for(INT SideIndex = 0; SideIndex < NumDivisions; SideIndex++)
		DrawCircle(Base, FVector(0,0,1), FVector(1,0,0) * appCos(AngleDelta * SideIndex) + FVector(0,1,0) * appSin(AngleDelta * SideIndex), Color, Radius, NumDivisions);

	unguard;
}


//
//  FLineBatcher::DrawCylinder
//

void FLineBatcher::DrawCylinder(FRenderInterface* RI,FVector Base,FVector X,FVector Y,FVector Z,FColor Color,FLOAT Radius,FLOAT HalfHeight,INT NumSides)
{
	guard(FLineBatcher::DrawCylinder);

	Color = Color.RenderColor();

	FLOAT	AngleDelta = 2.0f * PI / NumSides;
	FVector	LastVertex = Base + X * Radius;

	for(INT SideIndex = 0;SideIndex < NumSides;SideIndex++)
	{
		FVector	Vertex = Base + (X * appCos(AngleDelta * (SideIndex + 1)) + Y * appSin(AngleDelta * (SideIndex + 1))) * Radius;

		DrawLine(LastVertex - Z * HalfHeight,Vertex - Z * HalfHeight,Color);
		DrawLine(LastVertex + Z * HalfHeight,Vertex + Z * HalfHeight,Color);
		DrawLine(LastVertex - Z * HalfHeight,LastVertex + Z * HalfHeight,Color);

		LastVertex = Vertex;
	}

	unguard;
}

//
//  FLineBatcher::DrawDirectionalArrow
//

void FLineBatcher::DrawDirectionalArrow(FVector InLocation,FRotator InRotation,FColor InColor,FLOAT InDrawScale)
{
	guard(FLineBatcher::DrawDirectionalArrow);

	InColor = InColor.RenderColor();

	FCoords C = GMath.UnitCoords / InRotation;

	FLOAT Length = 48 * InDrawScale,
		SegmentLength = 16 * InDrawScale;

	DrawLine(InLocation + C.XAxis * Length,InLocation,InColor);
	DrawLine(InLocation + C.XAxis * Length,InLocation + C.XAxis * SegmentLength + C.YAxis * SegmentLength,InColor);
	DrawLine(InLocation + C.XAxis * Length,InLocation + C.XAxis * SegmentLength - C.YAxis * SegmentLength,InColor);
	DrawLine(InLocation + C.XAxis * Length,InLocation + C.XAxis * SegmentLength + C.ZAxis * SegmentLength,InColor);
	DrawLine(InLocation + C.XAxis * Length,InLocation + C.XAxis * SegmentLength - C.ZAxis * SegmentLength,InColor);

	unguard;
}

//
//  FLineBatcher::DrawConvexVolume
//
void FLineBatcher::DrawConvexVolume(FConvexVolume Volume,FColor Color)
{
	guard(FLineBatcher::DrawConvexVolume);

	Color = Color.RenderColor();

	for(INT FaceIndex = 0;FaceIndex < Volume.NumPlanes;FaceIndex++)
	{
		FPoly	Polygon;

		Polygon.Normal = Volume.BoundingPlanes[FaceIndex];
		Polygon.NumVertices = 4;

		FVector	Base = Polygon.Normal * Volume.BoundingPlanes[FaceIndex].W,
				AxisX,
				AxisY;

		Polygon.Normal.FindBestAxisVectors(AxisX,AxisY);

		Polygon.Vertex[0] = Base + AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX;
		Polygon.Vertex[1] = Base - AxisX * HALF_WORLD_MAX + AxisY * HALF_WORLD_MAX;
		Polygon.Vertex[2] = Base - AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX;
		Polygon.Vertex[3] = Base + AxisX * HALF_WORLD_MAX - AxisY * HALF_WORLD_MAX;

		for(INT PlaneIndex = 0;PlaneIndex < Volume.NumPlanes;PlaneIndex++)
		{
			if(PlaneIndex != FaceIndex)
			{
				if(!Polygon.Split(-FVector(Volume.BoundingPlanes[PlaneIndex]),Volume.BoundingPlanes[PlaneIndex] * Volume.BoundingPlanes[PlaneIndex].W))
				{
					Polygon.NumVertices = 0;
					break;
				}
			}
		}

		for(INT VertexIndex = 0;VertexIndex < Polygon.NumVertices;VertexIndex++)
			DrawLine(Polygon.Vertex[VertexIndex],Polygon.Vertex[(VertexIndex + 1) % Polygon.NumVertices],Color);
	}

	unguard;
}

//
//  FLineBatcher::GetCacheId
//

QWORD FLineBatcher::GetCacheId()
{
	return CacheId;
}

//
//  FLineBatcher::GetRevision
//

INT FLineBatcher::GetRevision()
{
	return 1;
}

//
//  FLineBatcher::GetSize
//

INT FLineBatcher::GetSize()
{
	return Vertices.Num() * sizeof(FLineVertex);
}

//
//  FLineBatcher::GetStride
//

INT FLineBatcher::GetStride()
{
	return sizeof(FLineVertex);
}

//
//  FLineBatcher::GetComponents
//

INT FLineBatcher::GetComponents(FVertexComponent* OutComponents)
{
	OutComponents[0].Type = CT_Float3;
	OutComponents[0].Function = FVF_Position;
	OutComponents[1].Type = CT_Color;
	OutComponents[1].Function = FVF_Diffuse;

	return 2;
}

//
//  FLineBatcher::GetStreamData
//

void FLineBatcher::GetStreamData(void* Dest)
{
	appMemcpy(Dest,&Vertices(0),Vertices.Num() * sizeof(FLineVertex));
}

//
//  FLineBatcher::GetRawStreamData
//

void FLineBatcher::GetRawStreamData(void ** Dest, INT FirstVertex )
{
	  *Dest = &Vertices(FirstVertex);
}

//
//  FCanvasUtil::FCanvasUtil
//

FCanvasUtil::FCanvasUtil(FRenderTarget* RenderTarget,FRenderInterface* InRI)
{
	RI = InRI;

	FLOAT	SizeX = RenderTarget->GetWidth(),
			SizeY = RenderTarget->GetHeight();

#ifdef __PSX2_EE__ // we need 1.0 for the Z. maybe DirectX can have the 1.0 also to merge the two lines?2
	CanvasToScreen = FTranslationMatrix(FVector(-SizeX / 2.0f - 0.5f,-SizeY / 2.0f - 0.5f,1.0f)) * FScaleMatrix(FVector(2.0f / SizeX,-2.0f / SizeY,1.0f));
#else
	if( GIsOpenGL )
		CanvasToScreen = FTranslationMatrix(FVector(-SizeX / 2.0f,-SizeY / 2.0f,0.0f)) * FScaleMatrix(FVector(2.0f / SizeX,-2.0f / SizeY,1.0f));
	else
		CanvasToScreen = FTranslationMatrix(FVector(-SizeX / 2.0f - 0.5f,-SizeY / 2.0f - 0.5f,0.0f)) * FScaleMatrix(FVector(2.0f / SizeX,-2.0f / SizeY,1.0f));
#endif
	ScreenToCanvas = CanvasToScreen.Inverse();

	CacheId = MakeCacheID(CID_RenderVertices);

	PrimitiveType = PT_LineList;
	Material = NULL;
	NumPrimitives = 0;
}

//
//  FCanvasUtil::~FCanvasUtil
//

FCanvasUtil::~FCanvasUtil()
{
	Flush();
}

//
//  FCanvasUtil::Flush
//

void FCanvasUtil::Flush()
{
	guard(FCanvasUtil::Flush);

	if(NumPrimitives > 0)
	{
		RI->PushState();

		RI->SetDistanceFog( 0, 0, 0, FColor(0,0,0) );
		RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);
#ifdef __GCN__ // wacky ortho fun for GCN
		static FTranslationMatrix GCNTranslation(FVector(0.0f, 0.0f, 1.0f));
		static FMatrix GCNOrtho(FPlane(1.0f, 0.0f, 0.0f, 0.0f), FPlane(), FPlane(), FPlane());
		RI->SetTransform(TT_WorldToCamera,GCNTranslation);
		RI->SetTransform(TT_CameraToScreen,GCNOrtho);
#else
		RI->SetTransform(TT_WorldToCamera,FMatrix::Identity);
		RI->SetTransform(TT_CameraToScreen,CanvasToScreen);
#endif

		INT	BaseVertexIndex = RI->SetDynamicStream(VS_FixedFunction,this);

		RI->EnableLighting(0,1);
		RI->SetMaterial(Material);
		RI->SetIndexBuffer(NULL,0);

		RI->SetCullMode(CM_None);

		RI->DrawPrimitive(PrimitiveType,BaseVertexIndex,NumPrimitives,0,Vertices.Num() - 1);

		RI->PopState();

		Vertices.Empty();
		NumPrimitives = 0;
	}

	unguard;
}

//
//  FCanvasUtil::BeginPrimitive
//

void FCanvasUtil::BeginPrimitive(EPrimitiveType InType,UMaterial* InMaterial)
{
	guard(FCanvasUtil::BeginPrimitive);

	if(PrimitiveType != InType || Material != InMaterial )
	{
		Flush();
		PrimitiveType = InType;
		Material = InMaterial;
	}

	unguard;
}

//
//  FCanvasUtil::DrawLine
//

void FCanvasUtil::DrawLine(FLOAT X1,FLOAT Y1,FLOAT X2,FLOAT Y2,FColor Color)
{
	guard(FCanvasUtil::DrawLine);

	Color = Color.RenderColor();

	static FSolidColorTexture	WhiteTexture(FColor(255,255,255));
	
	DECLARE_STATIC_UOBJECT( UProxyBitmapMaterial, HACKGAH, { HACKGAH->SetTextureInterface(&WhiteTexture); } );
	DECLARE_STATIC_UOBJECT( UFinalBlend, LineMaterial, { LineMaterial->Material = HACKGAH; } );

	BeginPrimitive(PT_LineList,LineMaterial);

	new(Vertices) FCanvasVertex(FVector(X1,Y1,0),Color,0.0f,0.0f);
	new(Vertices) FCanvasVertex(FVector(X2,Y2,0),Color,0.0f,0.0f);

	NumPrimitives++;

	unguard;
}

//
//  FCanvasUtil::DrawPoint
//

void FCanvasUtil::DrawPoint(FLOAT X1,FLOAT Y1,FLOAT X2,FLOAT Y2,FLOAT Z,FColor Color)
{
	guard(FCanvasUtil::DrawPoint);

	Color = Color.RenderColor();

	static FSolidColorTexture	WhiteTexture(FColor(255,255,255));
	
	DECLARE_STATIC_UOBJECT( UProxyBitmapMaterial, HACKGAH, { HACKGAH->SetTextureInterface(&WhiteTexture); } );
	DECLARE_STATIC_UOBJECT( UFinalBlend, LineMaterial, { LineMaterial->Material = HACKGAH; } );

	BeginPrimitive(PT_LineList,LineMaterial);

	new(Vertices) FCanvasVertex(FVector(X1,Y1,Z),Color,0.0f,0.0f);
	new(Vertices) FCanvasVertex(FVector(X2,Y1,Z),Color,0.0f,0.0f);

	new(Vertices) FCanvasVertex(FVector(X2,Y1,Z),Color,0.0f,0.0f);
	new(Vertices) FCanvasVertex(FVector(X2,Y2,Z),Color,0.0f,0.0f);

	new(Vertices) FCanvasVertex(FVector(X2,Y2,Z),Color,0.0f,0.0f);
	new(Vertices) FCanvasVertex(FVector(X1,Y2,Z),Color,0.0f,0.0f);

	new(Vertices) FCanvasVertex(FVector(X1,Y2,Z),Color,0.0f,0.0f);
	new(Vertices) FCanvasVertex(FVector(X1,Y1,Z),Color,0.0f,0.0f);

	NumPrimitives += 4;

	unguard;
}

//
//  FCanvasUtil::DrawTile
//

void FCanvasUtil::DrawTile(FLOAT X1,FLOAT Y1,FLOAT X2,FLOAT Y2,FLOAT U1,FLOAT V1,FLOAT U2,FLOAT V2,FLOAT Z,UMaterial* Material,FColor Color)
{
	guard(FCanvasUtil::DrawTile);

	Color = Color.RenderColor();

	BeginPrimitive(PT_TriangleList,Material);

	INT		TextureWidth = Material ? Material->MaterialUSize() : 1,
			TextureHeight = Material? Material->MaterialVSize() : 1;
	FLOAT	InvTextureSizeX = 1.0f / TextureWidth,
			InvTextureSizeY = 1.0f / TextureHeight;

	new(Vertices) FCanvasVertex(FVector(X1,Y1,Z),Color,U1 * InvTextureSizeX,V1 * InvTextureSizeY);
	new(Vertices) FCanvasVertex(FVector(X2,Y1,Z),Color,U2 * InvTextureSizeX,V1 * InvTextureSizeY);
	new(Vertices) FCanvasVertex(FVector(X2,Y2,Z),Color,U2 * InvTextureSizeX,V2 * InvTextureSizeY);

	new(Vertices) FCanvasVertex(FVector(X1,Y1,Z),Color,U1 * InvTextureSizeX,V1 * InvTextureSizeY);
	new(Vertices) FCanvasVertex(FVector(X2,Y2,Z),Color,U2 * InvTextureSizeX,V2 * InvTextureSizeY);
	new(Vertices) FCanvasVertex(FVector(X1,Y2,Z),Color,U1 * InvTextureSizeX,V2 * InvTextureSizeY);

	NumPrimitives += 2;

	unguard;
}

//
//	FCanvasUtil::DrawString
//

INT FCanvasUtil::DrawString(INT StartX,INT StartY,const TCHAR* Text,UFont* Font,FColor Color,UBOOL IsRenderTarget)
{
	guard(FCanvasUtil::DrawString);

    UBOOL FlipTex = ((IsRenderTarget) && (GIsOpenGL));

	Color = Color.RenderColor();

	// Draw all characters in string.
	INT LineX = 0;
	INT bDrawUnderline = 0;
	INT UnderlineWidth = 0;
	for( INT i=0; Text[i]; i++ )
	{
		INT bUnderlineNext = 0;
		INT Ch = (TCHARU)Font->RemapChar(Text[i]);

		// Handle ampersand underlining.
		if( bDrawUnderline )
			Ch = (TCHARU)Font->RemapChar('_');
		if( Text[i]=='&' )
		{
			if( !Text[i+1] )
				break; 
			if( Text[i+1]!='&' )
			{
				bUnderlineNext = 1;
				Ch = (TCHARU)Font->RemapChar(Text[i+1]);
			}
		}

		// Process character if it's valid.
		if( Ch < Font->Characters.Num() )
		{
			FFontCharacter& Char = Font->Characters(Ch);
			UTexture* Tex;
			if( Char.TextureIndex < Font->Textures.Num() && (Tex=Font->Textures(Char.TextureIndex))!=NULL )
			{
				// Compute character width.
				INT CharWidth;
				if( bDrawUnderline )
					CharWidth = Min(UnderlineWidth, Char.USize);
				else
					CharWidth = Char.USize;

				// Prepare for clipping.
				INT X      = LineX + StartX;
				INT Y      = StartY;
				INT CU     = Char.StartU;
				INT CV     = Char.StartV;
				INT CUSize = CharWidth;
				INT CVSize = Char.VSize;

                if (FlipTex)
                {
                    CV += CVSize;
                    CVSize = -CVSize;
                }

				// Draw.
				DrawTile(X,Y,X + CUSize,Y + Char.VSize,CU,CV,CU + CUSize,CV + CVSize,0.0f,Tex,Color);

				// Update underline status.
				if( bDrawUnderline )
					CharWidth = UnderlineWidth;

				if( !bUnderlineNext )
					LineX += (INT) (CharWidth);
				else
					UnderlineWidth = Char.USize;

				bDrawUnderline = bUnderlineNext;
			}
		}
	}

	return LineX;

	unguard;
}

//
//  FCanvasUtil::GetCacheId
//

QWORD FCanvasUtil::GetCacheId()
{
	return CacheId;
}

//
//  FCanvasUtil::GetRevision
//

INT FCanvasUtil::GetRevision()
{
	return 1;
}

//
//  FCanvasUtil::GetSize
//

INT FCanvasUtil::GetSize()
{
	return Vertices.Num() * sizeof(FCanvasVertex);
}

//
//  FCanvasUtil::GetStride
//

INT FCanvasUtil::GetStride()
{
	return sizeof(FCanvasVertex);
}

//
//  FCanvasUtil::GetComponents
//

INT FCanvasUtil::GetComponents(FVertexComponent* OutComponents)
{
	OutComponents[0].Type = CT_Float3;
	OutComponents[0].Function = FVF_Position;
	OutComponents[1].Type = CT_Color;
	OutComponents[1].Function = FVF_Diffuse;
	OutComponents[2].Type = CT_Float2;
	OutComponents[2].Function = FVF_TexCoord0;

	return 3;
}

//
//  FCanvasUtil::GetStreamData
//

void FCanvasUtil::GetStreamData(void* Dest)
{
	appMemcpy(Dest,&Vertices(0),Vertices.Num() * sizeof(FCanvasVertex));
}

//
//  FCanvasUtil::GetRawStreamData
//

void FCanvasUtil::GetRawStreamData(void ** Dest, INT FirstVertex )
{
	*Dest = &Vertices(FirstVertex);
}


//
//  FRawIndexBuffer::FRawIndexBuffer
//

FRawIndexBuffer::FRawIndexBuffer()
{
	CacheId = MakeCacheID(CID_RenderIndices);
	Revision = 0;
}

//
//  FRawIndexBuffer::Stripify
//

INT FRawIndexBuffer::Stripify()
{
	guard(FRawIndexBuffer::Stripify);

#if HAS_NVTRISTRIP
	PrimitiveGroup*	PrimitiveGroups = NULL;
	_WORD			NumPrimitiveGroups = 0;

	SetListsOnly(false);
	GenerateStrips(&Indices(0),Indices.Num(),&PrimitiveGroups,&NumPrimitiveGroups);

	Indices.Empty();
	Indices.Add(PrimitiveGroups->numIndices);

	appMemcpy(&Indices(0),PrimitiveGroups->indices,Indices.Num() * sizeof(_WORD));

	delete [] PrimitiveGroups;

	Revision++;

	return Indices.Num() - 2;

#else
	return 0;
#endif

	unguard;
}

//
//  FRawIndexBuffer::CacheOptimize
//

void FRawIndexBuffer::CacheOptimize()
{
	guard(FRawIndexBuffer::CacheOptimize);

#if HAS_NVTRISTRIP
	PrimitiveGroup*	PrimitiveGroups = NULL;
	_WORD			NumPrimitiveGroups = 0;

	SetListsOnly(true);
	GenerateStrips(&Indices(0),Indices.Num(),&PrimitiveGroups,&NumPrimitiveGroups);

	Indices.Empty(PrimitiveGroups->numIndices);
	Indices.Add(PrimitiveGroups->numIndices);

	appMemcpy(&Indices(0),PrimitiveGroups->indices,Indices.Num() * sizeof(_WORD));

	delete [] PrimitiveGroups;

	Revision++;
#else
	check(0);
#endif

	unguard;
}

//
//  FRawIndexBuffer operator<<
//

FArchive& operator<<(FArchive& Ar,FRawIndexBuffer& I)
{
	return Ar << I.Indices << I.Revision;
}

//
//  FRawIndexBuffer::GetCacheId
//

QWORD FRawIndexBuffer::GetCacheId()
{
	return CacheId;
}

//
//  FRawIndexBuffer::GetRevision
//

INT FRawIndexBuffer::GetRevision()
{
	return Revision;
}

//
//  FRawIndexBuffer::GetSize
//

INT FRawIndexBuffer::GetSize()
{
	return Indices.Num() * sizeof(_WORD);
}

//
//  FRawIndexBuffer::GetIndexSize
//

INT FRawIndexBuffer::GetIndexSize()
{
	return sizeof(_WORD);
}

//
//  FRawIndexBuffer::GetContents
//

void FRawIndexBuffer::GetContents(void* Data)
{
	appMemcpy(Data,&Indices(0),Indices.Num() * sizeof(_WORD));
}

//
//  FRaw32BitIndexBuffer::FRaw32BitIndexBuffer
//

FRaw32BitIndexBuffer::FRaw32BitIndexBuffer()
{
	CacheId = MakeCacheID(CID_RenderIndices);
	Revision = 0;
}

//
//  FRaw32BitIndexBuffer operator<<
//

FArchive& operator<<(FArchive& Ar,FRaw32BitIndexBuffer& I)
{
	return Ar << I.Indices << I.Revision;
}

//
//  FRaw32BitIndexBuffer::GetCacheId
//

QWORD FRaw32BitIndexBuffer::GetCacheId()
{
	return CacheId;
}

//
//  FRaw32BitIndexBuffer::GetRevision
//

INT FRaw32BitIndexBuffer::GetRevision()
{
	return Revision;
}

//
//  FRaw32BitIndexBuffer::GetSize
//

INT FRaw32BitIndexBuffer::GetSize()
{
	return Indices.Num() * sizeof(DWORD);
}

//
//  FRaw32BitIndexBuffer::GetIndexSize
//

INT FRaw32BitIndexBuffer::GetIndexSize()
{
	return sizeof(DWORD);
}

//
//  FRaw32BitIndexBuffer::GetContents
//

void FRaw32BitIndexBuffer::GetContents(void* Data)
{
	appMemcpy(Data,&Indices(0),Indices.Num() * sizeof(DWORD));
}

//
//  FRawColorStream::FRawColorStream
//

FRawColorStream::FRawColorStream()
{
	CacheId = MakeCacheID(CID_RenderVertices);
	Revision = 0;
}

//
//  FRawColorStream operator<<
//

FArchive& operator<<(FArchive& Ar,FRawColorStream& ColorStream)
{
	return Ar	<< ColorStream.Colors
				<< ColorStream.Revision;
}

//
//  FRawColorStream::GetCacheId
//

QWORD FRawColorStream::GetCacheId()
{
	return CacheId;
}

//
//  FRawColorStream::GetRevision
//

INT FRawColorStream::GetRevision()
{
	return Revision;
}

//
//  FRawColorStream::GetSize
//

INT FRawColorStream::GetSize()
{
	return Colors.Num() * sizeof(FColor);
}

//
//  FRawColorStream::GetStride
//

INT FRawColorStream::GetStride()
{
	return sizeof(FColor);
}

//
//  FRawColorStream::GetComponents
//

INT FRawColorStream::GetComponents(FVertexComponent* OutComponents)
{
	OutComponents[0].Type = CT_Color;
	OutComponents[0].Function = FVF_Diffuse;

	return 1;
}

//
//  FRawColorStream::GetStreamData
//

void FRawColorStream::GetStreamData(void* Dest)
{
	appMemcpy(Dest,&Colors(0),Colors.Num() * sizeof(FColor));
}

//
//  FRawColorStream::GetRawStreamData
//

void FRawColorStream::GetRawStreamData(void ** Dest, INT FirstVertex )
{
	*Dest = &Colors(FirstVertex);
}

//
//	FSolidColorTexture::FSolidColorTexture
//

FSolidColorTexture::FSolidColorTexture(FColor InColor)
{
	Color = InColor;

	Revision = 1;
	CacheId = MakeCacheID(CID_RenderTexture);
}

//
//	FSolidColorTexture::GetRevision
//

INT FSolidColorTexture::GetRevision()
{
	return Revision;
}

//
//	FSolidColorTexture::GetCacheId
//

QWORD FSolidColorTexture::GetCacheId()
{
	return CacheId;
}

//
//	FSolidColorTexture::GetWidth
//

INT FSolidColorTexture::GetWidth()
{
	return 1;
}

//
//	FSolidColorTexture::GetHeight
//

INT FSolidColorTexture::GetHeight()
{
	return 1;
}

//
//	FSolidColorTexture::GetUClamp
//

ETexClampMode FSolidColorTexture::GetUClamp()
{
	return TC_Wrap;
}

//
//	FSolidColorTexture::GetVClamp
//

ETexClampMode FSolidColorTexture::GetVClamp()
{
	return TC_Wrap;
}

//
//	FSolidColorTexture::GetFormat
//

ETextureFormat FSolidColorTexture::GetFormat()
{
	return TEXF_RGBA8;
}

//
//	FSolidColorTexture::GetNumMips
//

INT FSolidColorTexture::GetNumMips()
{
	return 1;
}

//
//	FSolidColorTexture::GetFirstMip
//

INT FSolidColorTexture::GetFirstMip()
{
	return 0;
}

//
//	FSolidColorTexture::GetTextureData
//

void FSolidColorTexture::GetTextureData(INT MipIndex,void* Dest,INT DestStride,ETextureFormat DestFormat,UBOOL ColoredMips)
{
	guard(FSolidColorTexture::GetTextureData);

	check(MipIndex == 0);
	check(DestFormat == TEXF_RGBA8);

	*((FColor*) Dest) = Color;

	unguard;
}

//
//	FSolidColorTexture::GetRawTextureData
//

void* FSolidColorTexture::GetRawTextureData(INT MipIndex)
{
	return &Color;
}

//
//	FSolidColorTexture::GetUTexture
//

UTexture* FSolidColorTexture::GetUTexture()
{
	return NULL;
}

//
//	FAuxRenderTarget::FAuxRenderTarget
//

FAuxRenderTarget::FAuxRenderTarget(INT InWidth,INT InHeight,ETextureFormat InFormat)
{
	Width = InWidth;
	Height = InHeight;
	Format = InFormat;

	Revision = 1;
	CacheId = MakeCacheID(CID_RenderTexture);
}

//
//	FAuxRenderTarget::~FAuxRenderTarget
//

FAuxRenderTarget::~FAuxRenderTarget()
{
	guard(FAuxRenderTarget::~FAuxRenderTarget);
	if( UTexture::__Client
	&&	UTexture::__Client->Engine
	&&	UTexture::__Client->Engine->GRenDev
	&&	GetCacheId()
	)
		UTexture::__Client->Engine->GRenDev->FlushResource( GetCacheId() );
	unguard;
}

//
//	FAuxRenderTarget::GetRevision
//

INT FAuxRenderTarget::GetRevision()
{
	return Revision;
}

//
//	FAuxRenderTarget::GetCacheId
//

QWORD FAuxRenderTarget::GetCacheId()
{
	return CacheId;
}

//
//	FAuxRenderTarget::GetWidth
//

INT FAuxRenderTarget::GetWidth()
{
	return Width;
}

//
//	FAuxRenderTarget::GetHeight
//

INT FAuxRenderTarget::GetHeight()
{
	return Height;
}

//
//	FAuxRenderTarget::GetUClamp
//

ETexClampMode FAuxRenderTarget::GetUClamp()
{
	return TC_Wrap;
}

//
//	FAuxRenderTarget::GetVClamp
//

ETexClampMode FAuxRenderTarget::GetVClamp()
{
	return TC_Wrap;
}

//
//	FAuxRenderTarget::GetFormat
//

ETextureFormat FAuxRenderTarget::GetFormat()
{
	return Format;
}

//
//	FAuxRenderTarget::GetNumMips
//

INT FAuxRenderTarget::GetNumMips()
{
	return 1;
}

//
//	FAuxRenderTarget::GetFirstMip
//

INT FAuxRenderTarget::GetFirstMip()
{
	return 0;
}

