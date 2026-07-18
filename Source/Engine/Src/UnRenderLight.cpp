/*=============================================================================
	UnRenderLight.cpp: Lighting code.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"
#include "UnRenderPrivate.h"

//
//	FDynamicLight::FDynamicLight
//

FDynamicLight::FDynamicLight(AActor* InActor)
{
	Actor = InActor;
    Alpha = 1.0f;
	Revision = INDEX_NONE;
	ConsideredTag=0;
	Update();
}

//
//	FDynamicLight::Update
//

void FDynamicLight::Update()
{
    // gam ---
    if( !Actor )
        return;
    // --- gam

	FPlane	BaseColor = FGetHSV(Actor->LightHue,Actor->LightSaturation,255);
	FLOAT	Intensity = 0.0f;

	if(Actor->LightType == LT_Steady)
		Intensity = 1.0f;
	else if(Actor->LightType == LT_Pulse)
		Intensity = 0.6f + 0.39f * GMath.SinTab((INT)((Actor->Level->TimeSeconds * 35.0f * 65536.0f) / Max((int)Actor->LightPeriod,1) + (Actor->LightPhase << 8)));
	else if(Actor->LightType == LT_Blink)
	{
		if(((int)((Actor->Level->TimeSeconds * 35.0f * 65536.0f)/(Actor->LightPeriod+1) + (Actor->LightPhase << 8))) & 1)
			Intensity = 0.0f;
		else
			Intensity = 1.0f;
	}
	else if(Actor->LightType == LT_Flicker)
	{
		FLOAT	Rand = appFrand();

		if(Rand < 0.5f)
			Intensity = 0.0f;
		else
			Intensity = Rand;
	}
	else if(Actor->LightType == LT_Strobe)
	{
		static float LastUpdateTime=0; static int Toggle=0;
		if( LastUpdateTime != Actor->Level->TimeSeconds )
		{
			LastUpdateTime = Actor->Level->TimeSeconds;
			Toggle ^= 1;
		}
		if( Toggle ) Intensity = 0.0f;
		else Intensity = 1.0f;
	}
	else if(Actor->LightType == LT_SubtlePulse)
		Intensity = 0.9f + 0.09f * GMath.SinTab((INT)((Actor->Level->TimeSeconds * 35.0f * 65536.0f) / Max((int)Actor->LightPeriod,1) + (Actor->LightPhase << 8)));
	else if(Actor->LightType == LT_TexturePaletteOnce)
	{
		if( Actor->Skins.Num() && Cast<UTexture>(Actor->Skins(0)) && Cast<UTexture>(Actor->Skins(0))->Palette )
		{
			FColor C = Cast<UTexture>(Actor->Skins(0))->Palette->Colors(appFloor(255.0f * Actor->LifeFraction()));
			BaseColor = FVector( C.R, C.G, C.B ).SafeNormal();
			Intensity = C.FBrightness() * 2.8f;
		}
	}
	else if(Actor->LightType == LT_TexturePaletteLoop)
	{
		if( Actor->Skins.Num() && Cast<UTexture>(Actor->Skins(0)) && Cast<UTexture>(Actor->Skins(0))->Palette )
		{
			FLOAT Time        = Actor->Level->TimeSeconds * 35 / Max((int)Actor->LightPeriod,1) + Actor->LightPhase;
			FColor C          = Cast<UTexture>(Actor->Skins(0))->Palette->Colors(((int)(Time*256) & 255) % 255);
			BaseColor         = FVector( C.R, C.G, C.B ).UnsafeNormal();
			Intensity         = C.FBrightness() * 2.8f;
		}

		Dynamic = 1;
	}
	else if ( Actor->LightType == LT_FadeOut )
	{
		Intensity = ::Min(1.f,1.5f*(1.f - Actor->LifeFraction()));
	}
	Color = BaseColor * (Actor->LightBrightness / 255.0f) * Intensity * Actor->Level->Brightness;

	if(Actor->LightEffect == LE_Sunlight)
	{
		Direction = Actor->Rotation.Vector();
		Position = FVector(0,0,0);
		Radius = 0;
	}
	else if(Actor->LightEffect == LE_Spotlight || Actor->LightEffect == LE_StaticSpot)
	{
		Position = Actor->Location;
		Direction = Actor->Rotation.Vector();
		Radius = Actor->WorldLightRadius();
	}
	else
	{
		Position = Actor->Location;
		Radius = Actor->WorldLightRadius();
	}

	Alpha = 1.0f;

	Dynamic = Actor->bDynamicLight;
	Changed = !Actor->bDynamicLight && (Actor->bLightChanged || Actor->bDeleteMe);

	Revision = Actor->RenderRevision;
}

//
//	UnrealAttenuation
//	The basic light attenuation model used by Unreal.
//

FLOAT UnrealAttenuation(FLOAT Distance,FLOAT Radius,FVector LightVector,FVector Normal)
{
	if((LightVector | Normal) > 0.0f && Distance <= Radius)
	{
		FLOAT	A = Distance / Radius,					// Unreal's lighting model.
				B = (2 * A * A * A - 3 * A * A + 1),
				C = Abs((LightVector | Normal) / Radius);

		return B / A * C * 2.0f;
	}
	else
		return 0.0f;
}

//
//	FDynamicLight::SampleIntensity
//

FLOAT FDynamicLight::SampleIntensity(FVector SamplePosition,FVector SampleNormal)
{
	if(Actor->LightEffect == LE_Sunlight)
	{
		// Directional light.

		if((Direction | SampleNormal) < 0.0f)
			return (Direction | SampleNormal) * -2.0f;
		else
			return 0.0f;
	}
	else if(Actor->LightEffect == LE_Cylinder)
	{
		// Cylindrical light.

		FVector	LightVector	= Position - SamplePosition;
		FLOAT	DistanceSquared = LightVector.SizeSquared(),
				Distance = appSqrt(DistanceSquared);

		if(Distance < Radius)
			return Max(0.0f,1.0f - (Square(LightVector.X) + Square(LightVector.Y)) / Square(Radius)) * 2.0f;
		else
			return 0.0f;
	}
	else if(Actor->LightEffect == LE_NonIncidence)
	{
		// Non incidence light.

		FVector	LightVector	= Position - SamplePosition;
		FLOAT	DistanceSquared = LightVector.SizeSquared(),
				Distance = appSqrt(DistanceSquared);

		if((LightVector | SampleNormal) > 0.0f && Distance < Radius)
			return appSqrt(1.02f - Distance / Radius) * 2.0f;
		else
			return 0.0f;
	}
	else if(Actor->LightEffect == LE_QuadraticNonIncidence)
	{
		// Quadratic non incidence light.

		FVector	LightVector	= Position - SamplePosition;
		FLOAT	DistanceSquared = LightVector.SizeSquared(),
				RadiusSquared = Square(Radius);

		if((LightVector | SampleNormal) > 0.0f && DistanceSquared < RadiusSquared)
			return (1.02f - DistanceSquared / RadiusSquared) * 2.0f;
		else
			return 0.0f;
	}
	else if(Actor->LightEffect == LE_Spotlight || Actor->LightEffect == LE_StaticSpot)
	{
		// Spot light.

		FVector	LightVector = Position - SamplePosition;
		FLOAT	DistanceSquared = LightVector.SizeSquared(),
				Distance = appSqrt(DistanceSquared),
				BaseAttenuation = UnrealAttenuation(Distance,Radius,LightVector,SampleNormal);

		if(BaseAttenuation > 0.0f)
		{
			FLOAT	Sine = 1.0 - Actor->LightCone / 256.0,
					RSine = 1.0 / (1.0 - Sine),
					SineRSine = Sine * RSine,
					SineSq = Sine * Sine,
					VDotV = -LightVector | Direction;

			if(VDotV > 0.0 && Square(VDotV) > SineSq * DistanceSquared)
				return Square(VDotV * RSine / Distance - SineRSine) * BaseAttenuation;
		}

		return 0.0f;
	}
	else
	{
		// Point light.

		FVector	LightVector	= Position - SamplePosition;

		return UnrealAttenuation(appSqrt(LightVector.SizeSquared()),Radius,LightVector,SampleNormal);
	}
}

//
//	FDynamicLight::SampleLight
//

FColor FDynamicLight::SampleLight(FVector SamplePosition,FVector SampleNormal)
{
	return FColor(Color * SampleIntensity(SamplePosition,SampleNormal));
}

//
//	FLightingTables
//

#define APPROX_MAN_BITS 10		/* Number of bits of approximate square root mantissa, <=23 */
#define APPROX_EXP_BITS 9		/* Number of bits in IEEE exponent */

static FLOAT	SqrtManTbl[2 << APPROX_MAN_BITS],
				DivSqrtManTbl[1 << APPROX_MAN_BITS],
				DivManTbl[1 << APPROX_MAN_BITS],
				DivSqrtExpTbl[1 << APPROX_EXP_BITS],
				DivExpTbl[1 << APPROX_EXP_BITS];

#if ASM
	#define POWER_ASM(ManTbl,ExpTbl)\
		__asm\
		{\
			/* Here we use the identity sqrt(a*b) = sqrt(a)*sqrt(b) to perform\
			** an approximate floating point square root by using a lookup table\
			** for the mantissa (a) and the exponent (b), taking advantage of the\
			** ieee floating point format.\
			*/\
			__asm mov  eax,[F]									/* get float as int                   */\
			__asm shr  eax,(32-APPROX_EXP_BITS)-APPROX_MAN_BITS	/* want APPROX_MAN_BITS mantissa bits */\
			__asm mov  edx,[F]									/* get float as int                   */\
			__asm shr  edx,32-APPROX_EXP_BITS					/* want APPROX_EXP_BITS exponent bits */\
			__asm and  eax,(1<<APPROX_MAN_BITS)-1				/* keep lowest 9 mantissa bits        */\
			__asm fld  DWORD PTR ManTbl[eax*4]	/* get mantissa lookup                */\
			__asm fmul DWORD PTR ExpTbl[edx*4]					/* multiply by exponent lookup        */\
			__asm fstp [F]										/* store result                       */\
		}\
		return F;

	//
	// Fast floating point power routines.
	// Pretty accurate to the first 10 bits.
	// About 12 cycles on the Pentium.
	//
	inline FLOAT DivSqrtApprox(FLOAT F) {POWER_ASM(DivSqrtManTbl,DivSqrtExpTbl);}
	inline FLOAT DivApprox    (FLOAT F) {POWER_ASM(DivManTbl,    DivExpTbl    );}
	inline FLOAT SqrtApprox   (FLOAT F)
	{
		__asm
		{
			mov  eax,[F]                        // get float as int.
			shr  eax,(23 - APPROX_MAN_BITS) - 2 // shift away unused low mantissa.
			mov  edx,[F]						// get float as int.
			and  eax, ((1 << (APPROX_MAN_BITS+1) )-1) << 2 // 2 to avoid "[eax*4]".
			and  edx, 0x7F000000				// 7 bit exp., wipe low bit+sign.
			shr  edx, 1							// exponent/2.
			mov  eax,DWORD PTR SqrtManTbl [eax]	// index hi bit is exp. low bit.
			add  eax,edx						// recombine with exponent.
			mov  [F],eax						// store.
		}
		return F;								// compiles to fld [F].
	}
#else
	inline FLOAT DivSqrtApprox(FLOAT F) {return 1.0/appSqrt(F);}
	inline FLOAT DivApprox    (FLOAT F) {return 1.0/F;}
	inline FLOAT SqrtApprox   (FLOAT F) {return appSqrt(F);}
#endif

class FLightingTables
{
public:

	FLOAT	LightSqrt[4096];
	DWORD	FilterTab[128][4];

	// SetupTable

	void SetupTable( FLOAT* ManTbl, FLOAT* ExpTbl, FLOAT Power )
	{
		union {FLOAT F; DWORD D;} Temp;

		Temp.F = 1.0;
		for( DWORD i=0; i<(1<<APPROX_EXP_BITS); i++ )
		{
			Temp.D = (Temp.D & 0x007fffff ) + (i << (32-APPROX_EXP_BITS));
			ExpTbl[ i ] = appPow( Abs(Temp.F), Power );
			if( appIsNan(ExpTbl[ i ]) )
				ExpTbl[ i ]=0.0;
			//debugf("exp [%f] %i = %f",Power,i,ExpTbl[i]);
		}

		Temp.F = 1.0;
		for( DWORD i=0; i<(1<<APPROX_MAN_BITS); i++ )
		{
			Temp.D = (Temp.D & 0xff800000 ) + (i << (32-APPROX_EXP_BITS-APPROX_MAN_BITS));
			ManTbl[ i ] = appPow( Abs(Temp.F), Power );
			if( appIsNan(ManTbl[ i ]) )
				ManTbl[ i ]=0.0;
			//debugf("man [%f] %i = %f",i,Power,ManTbl[i]);
		}
	}

	// Constructor.

	FLightingTables()
	{
		guard(FLightingTables::FLightingTables);

		// Setup light attenuation table.
		for( INT i=0; i<ARRAY_COUNT(LightSqrt); i++ )
		{
			FLOAT S = appSqrt((FLOAT)(i+1) * (1.0/ARRAY_COUNT(LightSqrt)));

			// This function gives a more luminous, specular look to the lighting.
			FLOAT Temp = (2*S*S*S-3*S*S+1); // Or 1.0-S.

			// This function makes surfaces look more matte.
			//FLOAT Temp = (1.0-S);
			LightSqrt[i] = Temp/S;
		}

		// Setup square root tables.
		for( DWORD D=0; D< (1<< APPROX_MAN_BITS ); D++ )
		{
			union {FLOAT F; DWORD D;} Temp;
			Temp.F = 1.0;
			Temp.D = (Temp.D & 0xff800000 ) + (D << (23 - APPROX_MAN_BITS));
			Temp.F = appSqrt(Temp.F);
			Temp.D = (Temp.D - ( 64 << 23 ) );   // exponent bias re-adjust
			SqrtManTbl[ D ] = (FLOAT)(Temp.F * appSqrt(2.0)); // for odd exponents
			SqrtManTbl[ D + (1 << APPROX_MAN_BITS) ] =  (FLOAT) (Temp.F * 2.0);
		}
		SetupTable(DivSqrtManTbl,DivSqrtExpTbl,-0.5);
		SetupTable(DivManTbl,    DivExpTbl,    -1.0);

		// Filtering table.
		INT FilterWeight[8][8] = 
		{
			{ 0,24,40,24,0,0,0,0},
			{ 0,40,64,40,0,0,0,0},
			{ 0,24,40,24,0,0,0,0},
			{ 0, 0, 0, 0,0,0,0,0},
			{ 0, 0, 0, 0,0,0,0,0},
			{ 0, 0, 0, 0,0,0,0,0},
			{ 0, 0, 0, 0,0,0,0,0},
			{ 0, 0, 0, 0,0,0,0,0}
		};

		// Generate filter lookup table
		INT FilterSum=0;
		for( INT i=0; i<8; i++ )
			for( int j=0; j<8; j++ )
				FilterSum += FilterWeight[i][j];

		// Iterate through all filter table indices 0x00-0x3f.
		for( INT i=0; i<128; i++ )
		{
			// Iterate through all vertical filter weights 0-3.
			for( int j=0; j<4; j++ )
			{
				// Handle all four packed values.
				register DWORD val = 0;
				for( INT Pack=0; Pack<4; Pack++ )
				{
					// Accumulate filter weights in FilterTab[i][j] according to which bits are set in i.
					INT Acc = 0;
					for( INT Bit=0; Bit<8; Bit++ )
						if( i & (1<<(Pack + Bit)) )
							Acc += FilterWeight[j][Bit];

					// Add to sum.
					DWORD Result = (Acc * 255) / FilterSum;
					check(Result>=0 && Result<=255);
					val += (Result << (Pack*8));
				}
				FilterTab[i][j] = INTEL_ORDER32(val);
			}
		}

		unguard;
	}
};

FLightingTables	LightingTables;

//
//	MergeLightMap
//

void ColorSubtract(FColor& dst, FColor& src) /// sjs test
{
    dst.R = Max(0,(int)(dst.R - src.R));
    dst.G = Max(0,(int)(dst.G - src.G));
    dst.B = Max(0,(int)(dst.B - src.B));
}

void MergeLightMap(BYTE* IlluminationMap,INT MinX,INT MinY,INT MaxX,INT MaxY,FDynamicLight* Light,FColor* Dest,INT DestStride)
{
	guard(MergeLightMap);

	// Build an intensity->color map for the light.

	FColor	Palette[256];
	BYTE	LightFactorR = 32 - appCeilLogTwo(Light->Color.X * 256.0f),
			LightFactorG = 32 - appCeilLogTwo(Light->Color.Y * 256.0f),
			LightFactorB = 32 - appCeilLogTwo(Light->Color.Z * 256.0f);
	DWORD	FixR = 0,
			FixDR = appFloor(Light->Color.X * (1 << LightFactorR)),
			FixG = 0,
			FixDG = appFloor(Light->Color.Y * (1 << LightFactorG)),
			FixB = 0,
			FixDB = appFloor(Light->Color.Z * (1 << LightFactorB));

	for(INT PaletteIndex = 0;PaletteIndex < 256;PaletteIndex++)
	{
		Palette[PaletteIndex].R = (BYTE) Min<DWORD>(FixR >> (LightFactorR - 1),255);
		FixR += FixDR;
		Palette[PaletteIndex].G = (BYTE) Min<DWORD>(FixG >> (LightFactorG - 1),255);
		FixG += FixDG;
		Palette[PaletteIndex].B = (BYTE) Min<DWORD>(FixB >> (LightFactorB - 1),255);
		FixB += FixDB;

		Palette[PaletteIndex].A = 255;
	}

	// Merge the intensity map into the complete lightmap.

    if( Light->Actor->LightEffect==LE_Negative ) // sjs
    {
	    for(INT Y = MinY;Y <= MaxY;Y++)
	    {
		    BYTE*	SrcPtr = IlluminationMap + (Y - MinY) * (MaxX - MinX + 1);
		    FColor*	DestPtr = Dest + Y * DestStride + MinX;

		    for(INT X = MinX;X <= MaxX;X++)
            {
                ColorSubtract(*DestPtr, Palette[*SrcPtr]);
                DestPtr++;
                SrcPtr++;
            }
	    }
    }
    else
    {
        for(INT Y = MinY;Y <= MaxY;Y++)
	    {
		    BYTE*	SrcPtr = IlluminationMap + (Y - MinY) * (MaxX - MinX + 1);
		    FColor*	DestPtr = Dest + Y * DestStride + MinX;

		    for(INT X = MinX;X <= MaxX;X++)
			    *DestPtr++ += Palette[*SrcPtr++];
	    }
    }

	unguard;
}

//
//	SmoothLightBitmap
//

BYTE* SmoothLightBitmap(FLightBitmap* LightBitmap,INT MinX,INT MinY,INT MaxX,INT MaxY)
{
	guard(SmoothLightBitmap);

	if(!LightBitmap || MaxY == MinY)
		return NewOned<BYTE>(GSceneMem,(MaxX - MinX + 1) * (MaxY - MinY + 1));

	INT		Stride = LightBitmap->Stride * 8,
			Size = Stride * LightBitmap->SizeY;
	BYTE*	ShadowMap = NewZeroed<BYTE>(GSceneMem,Size);

	INT		StrideDWORD = Stride / sizeof(DWORD);
	BYTE*	SrcBits = &LightBitmap->Bits(0);
	DWORD*	Dest1 = (DWORD*) ShadowMap;
	DWORD*	Dests[3] = { (DWORD*)Dest1, (DWORD*)Dest1, (DWORD*)Dest1 + StrideDWORD };

	for(INT Y = 0;Y < LightBitmap->SizeY;Y++)
	{
		// Get initial bits, with low bit shifted in.

		BYTE*	Src = SrcBits;

		// Offset of shadow map relative to convolution filter left edge.

		DWORD D = (DWORD)*Src++ << (8+2);
		if( D & 0x400 ) D |= 0x300;

		// Filter everything.

		for(INT X = 0;X < LightBitmap->Stride;X++)
		{
			D = D >> 8;
			D += (X < LightBitmap->Stride - 1) ? (((DWORD)*Src++) << (8+2)) : (D&0x200) ? 0xC00 : 0;

			DWORD	A = D & 0x7f;
			*Dests[0]++     += LightingTables.FilterTab[A][0];
			*Dests[1]++     += LightingTables.FilterTab[A][1];
			*Dests[2]++     += LightingTables.FilterTab[A][2];

			DWORD	B = (D >> 4) & 0x7f;
			*Dests[0]++     += LightingTables.FilterTab[B][0];
			*Dests[1]++     += LightingTables.FilterTab[B][1];
			*Dests[2]++     += LightingTables.FilterTab[B][2];
		}

		SrcBits += LightBitmap->Stride;

		if(Y == 0)
			Dests[0] -= StrideDWORD;

		if(Y == LightBitmap->SizeY - 2)
			Dests[2] -= StrideDWORD;
	}

	return ShadowMap;

	unguard;
}

//
//	Illuminate
//

BYTE* Illuminate(BYTE* ShadowMap,FBspSurf& Surf,FLightMap* LightMap,FLightBitmap* LightBitmap,INT MinX,INT MinY,INT MaxX,INT MaxY,FDynamicLight* Light)
{
	guard(Illuminate);

	INT		SizeX = MaxX - MinX + 1,
			SizeY = MaxY - MinY + 1;

	BYTE*	IlluminationMap = New<BYTE>(GSceneMem,SizeX * SizeY);

	BYTE*	Src = ShadowMap;
	INT		SrcStride = LightBitmap ? LightBitmap->Stride * 8 : SizeX;

	BYTE*	Dest = IlluminationMap;
	INT		DestStride = SizeX;

#if 0   // !!! FIXME: What is this doing here?  --ryan.
	{
	BYTE*	Dummy = new BYTE[Max(SrcStride,DestStride) * SizeY];
	guard(ValidateSource);
	appMemcpy( Dummy, Src, SrcStride * SizeY );
	unguard;
	guard(ValidateDestination);
	appMemcpy( Dummy, Dest, DestStride * SizeY );
	unguard;
	delete [] Dummy;
	}
#endif

	check(Light->Actor);

	if(Light->Actor->LightEffect == LE_Sunlight)
	{
		guard(LE_Sunlight);
		FVector	LightDirection = -Light->Direction;

		FLOAT	Dot = (Surf.PolyFlags & PF_TwoSided) ? Abs(Surf.Plane | LightDirection) : Max(0.0f,Surf.Plane | LightDirection);

		for(INT Y = 0;Y < LightMap->SizeY;Y++)
		{
			for(INT X = 0;X < LightMap->SizeX;X++)
				*Dest++ = appFloor(Dot * *Src++);

			Dest += (DestStride - LightMap->SizeX);
			Src += (SrcStride - LightMap->SizeX);
		}
		unguard;
	}
	else
	{
		FLOAT	RRadius = 1.0 / Max((FLOAT) 1.0f,Light->Radius),
				RRadiusMult = 4093.0 * RRadius * RRadius,
				Diffuse = Abs(((Light->Position - LightMap->LightMapBase) | Surf.Plane) / Light->Radius);
		FVector	Vertex = LightMap->LightMapBase - Light->Position + LightMap->LightMapX * (MinX + 0.5f) + LightMap->LightMapY * (MinY + 0.5f);

//MACRO BEGIN
#define SPATIAL_BEGIN \
	for(INT Y = MinY;Y <= MaxY;Y++) \
	{ \
		for(INT X = MinX;X <= MaxX;X++,Vertex += LightMap->LightMapX,Src++,Dest++) \
		{ \
			DWORD SqrtOfs = appRound(Vertex.SizeSquared() * RRadiusMult); \
			if(*Src && SqrtOfs < 4096) {

#define SPATIAL_END \
			} \
			else \
				*Dest = 0; \
		} \
		Vertex = Vertex - LightMap->LightMapX * SizeX + LightMap->LightMapY; \
		Src += SrcStride - SizeX; \
	}
//MACRO END

		if(Light->Actor->LightEffect == LE_Searchlight)
		{
			guard(LE_Searchlight);
			FLOAT	Offset = (2.0 * PI) + (Light->Actor->LightPhase * (8.0 * PI / 256.0)) + (Light->Actor->LightPeriod ? 35.0 * Light->Actor->Level->TimeSeconds / Light->Actor->LightPeriod : 0);

			SPATIAL_BEGIN;

			FLOAT Angle = appFmod( Offset + 4.0 * appAtan2( Vertex.X, Vertex .Y), 8.*PI );
			if( Angle<PI || Angle>PI*3.0 )
			{
				*Dest = (BYTE) 0.0;
			}
			else
			{
				FLOAT Scale = 0.5 + 0.5 * GMath.CosFloat(Angle);
				FLOAT D     = 0.00006 * (Square(Vertex.X) + Square(Vertex.Y));
				if( D < 1.0 )
					Scale *= D;
				*Dest = appFloor(*Src * Scale * Diffuse * LightingTables.LightSqrt[SqrtOfs]);
			}

			SPATIAL_END;	
			unguard;
		}
		else if(Light->Actor->LightEffect == LE_Rotor)
		{
			guard(LE_Rotor);
			SPATIAL_BEGIN;

			FLOAT Angle = 6.0 * appAtan2(Vertex.X,Vertex.Y);
			FLOAT Scale = 0.5 + 0.5 * GMath.CosFloat(Angle + Light->Actor->Level->TimeSeconds*3.5);
			FLOAT D     = 0.0001 * (Square(Vertex.X) + Square(Vertex.Y));
			if (D<1.0) Scale = 1.0 - D + Scale * D;
			*Dest		= appFloor(*Src * Scale * Diffuse * LightingTables.LightSqrt[SqrtOfs]);

			SPATIAL_END;
			unguard;
		}
		else if(Light->Actor->LightEffect == LE_SlowWave)
		{
			guard(LE_SlowWave);
			SPATIAL_BEGIN;

			FLOAT Scale	= 0.7 + 0.3 * GMath.SinTab((INT)(((int)SqrtApprox(Vertex.SizeSquared()) - Light->Actor->Level->TimeSeconds*35.0) * 1024.0));
			*Dest		= appFloor(*Src * Scale * Diffuse * LightingTables.LightSqrt[SqrtOfs]);

			SPATIAL_END;
			unguard;
		}
		else if(Light->Actor->LightEffect == LE_FastWave)
		{
			guard(LE_FastWave);
			SPATIAL_BEGIN;

			FLOAT Scale	= 0.7 + 0.3 * GMath.SinTab((INT)((((int)SqrtApprox(Vertex.SizeSquared())>>2) - Light->Actor->Level->TimeSeconds*35.0) * 2048.0));
			*Dest		= appFloor(*Src * Scale * Diffuse * LightingTables.LightSqrt[SqrtOfs]);

			SPATIAL_END;
			unguard;
		}
		else if(Light->Actor->LightEffect == LE_Shock)
		{
			guard(LE_Shock);
			SPATIAL_BEGIN;

			int Dist = INT (8.0 * SqrtApprox(Vertex.SizeSquared()));
			FLOAT Brightness  = 0.9 + 0.1 * GMath.SinTab((INT)(((Dist<<1) - (Light->Actor->Level->TimeSeconds * 4000.0))*16.0));
			Brightness       *= 0.9 + 0.1 * GMath.CosTab((INT)(((Dist   ) + (Light->Actor->Level->TimeSeconds * 4000.0))*16.0));
			Brightness       *= 0.9 + 0.1 * GMath.SinTab((INT)(((Dist>>1) - (Light->Actor->Level->TimeSeconds * 4000.0))*16.0));
			*Dest = appFloor(*Src * Diffuse * LightingTables.LightSqrt[SqrtOfs] * Brightness);

			SPATIAL_END;
			unguard;
		}
		else if(Light->Actor->LightEffect == LE_Disco)
		{
			guard(LE_Disco);
			SPATIAL_BEGIN;

			FLOAT Yaw	= 11.0 * appAtan2(Vertex.X,Vertex.Y);
			FLOAT Pitch = 11.0 * appAtan2(appSqrt(Square(Vertex.X)+Square(Vertex.Y)),Vertex.Z);

			FLOAT Scale1 = 0.50 + 0.50 * GMath.CosFloat(Yaw   + Light->Actor->Level->TimeSeconds*5.0);
			FLOAT Scale2 = 0.50 + 0.50 * GMath.CosFloat(Pitch + Light->Actor->Level->TimeSeconds*5.0);

			FLOAT Scale  = Scale1 + Scale2 - Scale1 * Scale2;

			FLOAT D = 0.00005 * (Square(Vertex.X) + Square(Vertex.Y));
			if (D<1.0) Scale *= D;

			*Dest = appFloor(*Src * (1.0-Scale) * Diffuse * LightingTables.LightSqrt[SqrtOfs]);

			SPATIAL_END;
			unguard;
		}
		else if(Light->Actor->LightEffect == LE_Cylinder)
		{
			guard(LE_Cylinder);
			for(INT Y = MinY;Y <= MaxY;Y++)
			{
				for(INT X = MinX;X <= MaxX;X++,Vertex += LightMap->LightMapX,Src++,Dest++)
					*Dest = Max(0,appFloor(*Src * (1.0 - (Square(Vertex.X) + Square(Vertex.Y)) * Square(RRadius))));

				Vertex = Vertex - LightMap->LightMapX * SizeX + LightMap->LightMapY;
				Src += SrcStride - SizeX;
			}
			unguard;
		}
		else if(Light->Actor->LightEffect == LE_Interference)
		{
			guard(LE_Interference);
			SPATIAL_BEGIN;

			FLOAT Pitch = 11.0 * appAtan2(SqrtApprox(Square(Vertex.X)+Square(Vertex.Y)),Vertex.Z);
			FLOAT Scale = 0.50 + 0.50 * GMath.CosFloat(Pitch + Light->Actor->Level->TimeSeconds*5.0);
			*Dest = appFloor(*Src * Scale * Diffuse * LightingTables.LightSqrt[SqrtOfs]);

			SPATIAL_END;
			unguard;
		}
		else if(Light->Actor->LightEffect == LE_Spotlight || Light->Actor->LightEffect == LE_StaticSpot)
		{
			guard(LE_Spotlight_OR_LE_StaticSpot);
			FVector View      = Light->Actor->GetViewRotation().Vector();
			FLOAT   Sine      = 1.0 - Light->Actor->LightCone / 256.0;
			FLOAT   RSine     = 1.0 / (1.0 - Sine);
			FLOAT   SineRSine = Sine * RSine;
			FLOAT   SineSq    = Sine * Sine;

			SPATIAL_BEGIN;

			FLOAT SizeSq = Vertex | Vertex;
			FLOAT VDotV  = Vertex | View;
			if( VDotV > 0.0 && Square(VDotV) > SineSq * SizeSq )
			{
				FLOAT Dot = Square( VDotV * RSine * DivSqrtApprox(SizeSq) - SineRSine );
				*Dest = appFloor(Dot * *Src * Diffuse * LightingTables.LightSqrt[SqrtOfs]);
			}
			else
				*Dest = 0;

			SPATIAL_END;
			unguard;
		}
		else if(Light->Actor->LightEffect == LE_NonIncidence)
		{
			SPATIAL_BEGIN;

			*Dest = appFloor( *Src * Max(1.0f - SqrtApprox(Vertex.SizeSquared()) * RRadius,0.0f) );

			SPATIAL_END;
		}
		else if(Light->Actor->LightEffect == LE_QuadraticNonIncidence)
		{
			SPATIAL_BEGIN;

			FLOAT	RRadiusSquared = 1.0f / Square(Max(Light->Radius,1.0f));

			*Dest = appFloor( *Src * Max(1.0f - Vertex.SizeSquared() * RRadiusSquared,0.0f) );

			SPATIAL_END;
		}
		else if(Light->Actor->LightEffect == LE_Shell)
		{
			guard(LE_Shell);
			SPATIAL_BEGIN;

			FLOAT Dist = SqrtApprox(Vertex.SizeSquared()) * RRadius;
			if( Dist >= 1.0 || Dist <= 0.8 )
				*Dest = 0;
			else
				*Dest = appFloor( *Src * (1.0 - 10.0*Abs(Dist-0.9)) );

			SPATIAL_END;
			unguard;
		}
		else
		{
			guard(ELSE);
			FLOAT	Diffuse = Abs(((Light->Position - LightMap->LightMapBase) | Surf.Plane) / Light->Radius),
					Scale = RRadiusMult * 4096.0f,
					Dist   = appRound((Vertex				| Vertex) * Scale),
					DistU  = appRound((Vertex				| LightMap->LightMapX) * Scale),
					DistV  = appRound((Vertex				| LightMap->LightMapY) * Scale),
					DistUU = appRound((LightMap->LightMapX	| LightMap->LightMapX) * Scale),
					DistVV = appRound((LightMap->LightMapY	| LightMap->LightMapY) * Scale),
					DistUV = appRound((LightMap->LightMapX	| LightMap->LightMapY) * Scale);

			static INT     Interp00, Interp10, Interp20, Interp01, Interp11, Interp02;
			static DWORD   Inner0, Inner1;
			static INT     Hecker;

			Interp00 = Dist;
			Interp10 = 2 * DistV + DistVV;
			Interp20 = 2 * DistVV;
			Interp01 = 2 * DistU + DistUU;
			Interp11 = 2 * DistUV;
			Interp02 = 2 * DistUU;

			for(INT Y = MinY;Y <= MaxY;Y++)
			{
				// Forward difference the square of the distance between the points.
				Inner0 = Interp00;
				Inner1 = Interp01;
				for(INT X = MinX;X <= MaxX;X++)
				{
					if( *Src!=0 && Inner0<4096*4096 ) 
					{
						*(FLOAT*)&Hecker = *Src * Diffuse * LightingTables.LightSqrt[Inner0>>12] + (2<<22);
						*Dest = Hecker;
					}
					else *Dest = 0;
					Src++;
					Dest++;
					Inner0 += Inner1;
					Inner1 += Interp02;
				}
				Interp00 += Interp10;
				Interp10 += Interp20;
				Interp01 += Interp11;
				Src += SrcStride - SizeX;
			}
			unguard;
		}
	}

	return IlluminationMap;

	unguard;
}

//
//	FCachedLightMap
//

class FCachedLightMap
{
public:

	FColor*	StaticLightMap;
	UBOOL*	StaticBitmapInclusion;
	INT		SizeX,
			SizeY,
			NumBitmaps;
	UBOOL	IsDynamic;

	// Constructor.

	FCachedLightMap(INT InSizeX,INT InSizeY, INT InNumBitmaps)
	{
		SizeX = InSizeX;
		SizeY = InSizeY;
		NumBitmaps	= InNumBitmaps;

		IsDynamic = 0;

		StaticLightMap = (FColor*) &(this[1]);
		StaticBitmapInclusion	= (UBOOL*) (StaticLightMap + SizeX * SizeY);

	}

	// CalculateSize

	static INT CalculateSize(INT SizeX,INT SizeY,INT NumBitmaps)
	{
		return sizeof(FCachedLightMap) + SizeX * SizeY * sizeof(FColor) + NumBitmaps * sizeof(UBOOL);
	}

	// In-place allocator.

	void* operator new(size_t Size,EInternal* Mem)
	{
		return (void*) Mem;
	}
};

//
//	FLightMap::FLightMap
//

FLightMap::FLightMap(ULevel* InLevel,INT InSurfaceIndex,INT InZoneIndex)
{
	Level = InLevel;
	iSurf = InSurfaceIndex;
	iZone = InZoneIndex;
	Revision = 0;
}

//
//	FLightMap::GetTextureData
//

void FLightMap::GetTextureData(INT MipIndex,void* Dest,INT DestStride,ETextureFormat DestFormat,UBOOL ColoredMips)
{
	guard(FLightMap::GetTextureData);

	clock(GStats.DWORDStats(GEngineStats.STATS_LightMap_Cycles));

	UModel*		Model = Level->Model;
	FBspSurf&	Surf = Model->Surfs(iSurf);
	UBOOL		UpdateStaticLighting = 0;

	// Find the cached lightmap info.

	QWORD				CacheId = MakeCacheID(CID_StaticMap,(INT)(this - &Model->LightMaps(0)),iZone,Model);
	FCacheItem*			CacheItem = NULL;
	FCachedLightMap*	CachedLightMap = (FCachedLightMap*) GCache.Get(CacheId,CacheItem);

	if(CachedLightMap && (CachedLightMap->SizeX != SizeX || CachedLightMap->SizeY != SizeY || CachedLightMap->NumBitmaps != Bitmaps.Num() ))
	{
		CachedLightMap = NULL;
		CacheItem->Unlock();
		GCache.Flush(CacheId);
	}

	if(!CachedLightMap)
	{
		CachedLightMap = (FCachedLightMap*) GCache.Create(CacheId,CacheItem,FCachedLightMap::CalculateSize(SizeX,SizeY,Bitmaps.Num()));
		UpdateStaticLighting = 1;

		new((EInternal*) CachedLightMap) FCachedLightMap(SizeX,SizeY,Bitmaps.Num());
	}

	// Determine whether the static lightmap needs to be updated.

	for(INT BitmapIndex = 0;BitmapIndex < Bitmaps.Num();BitmapIndex++)
	{
		FLightBitmap*	LightBitmap = &Bitmaps(BitmapIndex);
		UBOOL			Dynamic = LightBitmap->LightActor && (UPDATE_DYNAMIC_LIGHTMAPS && LightBitmap->LightActor->bDynamicLight) || (UPDATE_CHANGED_LIGHTMAPS && (LightBitmap->LightActor->bLightChanged || LightBitmap->LightActor->bDeleteMe));

		if(CachedLightMap->StaticBitmapInclusion[BitmapIndex] == Dynamic)
		{
			UpdateStaticLighting = 1;
			break;
		}
	}

	// Recalculate the static lighting.

	if(UpdateStaticLighting)
	{
		// Clear the texture to the ambient lighting.

		AZoneInfo*	Zone = Level->GetZoneActor(iZone);
		FColor		AmbientColor = FColor(FGetHSV(Zone->AmbientHue,Zone->AmbientSaturation,Zone->AmbientBrightness) * 0.5f);

		for(INT Y = 0;Y < SizeY;Y++)
		{
			FColor*	TexturePtr = CachedLightMap->StaticLightMap + Y * SizeX;

			for(INT X = 0;X < SizeX;X++)
				*TexturePtr++ = AmbientColor;
		}

		// Add each static light to the static lightmap.

		for(INT BitmapIndex = 0;BitmapIndex < Bitmaps.Num();BitmapIndex++)
		{
			FMemMark		MemMark(GSceneMem);
			FLightBitmap*	LightBitmap = &Bitmaps(BitmapIndex);

			if(LightBitmap->LightActor && !LightBitmap->LightActor->bDeleteMe && (!UPDATE_DYNAMIC_LIGHTMAPS || !LightBitmap->LightActor->bDynamicLight) && (!UPDATE_CHANGED_LIGHTMAPS || !LightBitmap->LightActor->bLightChanged))
			{
				FDynamicLight*	Light = LightBitmap->LightActor->GetLightRenderData();

				BYTE*	ShadowMap = SmoothLightBitmap(
										LightBitmap,
										LightBitmap->MinX,
										LightBitmap->MinY,
										LightBitmap->MaxX,
										LightBitmap->MaxY
										);
				BYTE*	IlluminationMap = Illuminate(
											ShadowMap,
											Surf,
											this,
											LightBitmap,
											LightBitmap->MinX,
											LightBitmap->MinY,
											LightBitmap->MaxX,
											LightBitmap->MaxY,
											Light
											);

				MergeLightMap(
					IlluminationMap,
					LightBitmap->MinX,
					LightBitmap->MinY,
					LightBitmap->MaxX,
					LightBitmap->MaxY,
					Light,
					CachedLightMap->StaticLightMap,
					SizeX
					);

				CachedLightMap->StaticBitmapInclusion[BitmapIndex] = 1;
			}
			else
				CachedLightMap->StaticBitmapInclusion[BitmapIndex] = 0;
			
			MemMark.Pop();
		}
	}

	// Copy the static lightmap into the texture.

	for(INT Y = 0;Y < SizeY;Y++)
		appMemcpy(((BYTE*) Dest) + Y * DestStride,CachedLightMap->StaticLightMap + Y * SizeX,SizeX * sizeof(FColor));

	// Add each dynamic light to the texture.

	CachedLightMap->IsDynamic = 0;

	for(INT LightIndex = 0;LightIndex < DynamicLights.Num();LightIndex++)
	{
		FMemMark		MemMark(GSceneMem);
		FDynamicLight*	Light = DynamicLights(LightIndex)->GetLightRenderData();

		if((UPDATE_DYNAMIC_LIGHTMAPS && Light->Dynamic) || (UPDATE_CHANGED_LIGHTMAPS && Light->Changed))
		{
			INT		MinX,
					MinY,
					MaxX,
					MaxY;

			if(Light->Actor->LightEffect == LE_Sunlight)
			{
				MinX = 0;
				MinY = 0;
				MaxX = SizeX - 1;
				MaxY = SizeY - 1;
			}
			else
			{
				FLOAT	PlaneDot = Surf.Plane.PlaneDot(Light->Position),
						PlaneRadius = appSqrt(Max<FLOAT>(Square(Light->Radius) * 1.05f - Square(PlaneDot),0.0));
				FVector	WorldCenter = Light->Position - Surf.Plane * PlaneDot,
						LightMapCenter = WorldToLightMap.TransformFVector(WorldCenter);
				FLOAT	RadiusX = PlaneRadius / LightMapX.Size(),
						RadiusY = PlaneRadius / LightMapY.Size();

				MinX = Clamp<INT>(/*appFloor*/(LightMapCenter.X - RadiusX),0,SizeX - 1);
				MaxX = Clamp<INT>(/*appFloor*/(LightMapCenter.X + RadiusX),0,SizeX - 1);
				MinY = Clamp<INT>(/*appFloor*/(LightMapCenter.Y - RadiusY),0,SizeY - 1);
				MaxY = Clamp<INT>(/*appFloor*/(LightMapCenter.Y + RadiusY),0,SizeY - 1);
			}

			BYTE*	ShadowMap = SmoothLightBitmap(
									NULL,
									MinX,
									MinY,
									MaxX,
									MaxY
									);
			BYTE*	IlluminationMap = Illuminate(
										ShadowMap,
										Surf,
										this,
										NULL,
										MinX,
										MinY,
										MaxX,
										MaxY,
										Light
										);

			MergeLightMap(
				IlluminationMap,
				MinX,
				MinY,
				MaxX,
				MaxY,
				Light,
				(FColor*) Dest,
				DestStride / sizeof(FColor)
				);

			CachedLightMap->IsDynamic = 1;
		}

		MemMark.Pop();
	}

	// Unlock the cached lightmap.

	CacheItem->Unlock();

	unclock(GStats.DWORDStats(GEngineStats.STATS_LightMap_Cycles));
	GStats.DWORDStats(GEngineStats.STATS_LightMap_Updates)++;

	unguard;
}

//
//	FLightMapTexture::FLightMapTexture
//

FLightMapTexture::FLightMapTexture(ULevel* InLevel)
{
	guard(FLightMapTexture::FLightMapTexture);

	Level = InLevel;
	CacheId = MakeCacheID(CID_RenderTexture);
	Revision = 0;

	unguard;
}

//
//	FLightMapTexture::GetChild
//

FTexture* FLightMapTexture::GetChild(INT ChildIndex,INT* OutChildX,INT* OutChildY)
{
	FLightMap*	LightMap = &Level->Model->LightMaps(LightMaps(ChildIndex));

	*OutChildX = LightMap->OffsetX;
	*OutChildY = LightMap->OffsetY;

	LightMap->Level = Level;

	return LightMap;
}

//
//	FStaticLightMapTexture::GetTextureData
//

void FStaticLightMapTexture::GetTextureData(INT MipIndex,void* Dest,INT DestStride,ETextureFormat DestFormat,UBOOL ColoredMips)
{
	guard(FStaticLightMapTexture::GetTextureData);

	check(DestFormat == Format);

	if( !Data[MipIndex].Num() )
		Data[MipIndex].Load();

	appMemcpy(Dest,&Data[MipIndex](0),Data[MipIndex].Num());

	Data[MipIndex].Unload();	

	unguard;
}

//
// FStaticLightMapTexture::UnloadRawTextureData
//
void FStaticLightMapTexture::UnloadRawTextureData( INT MipIndex )
{
	guard(FStaticLightMapTexture::UnloadRawTextureData);
	Data[MipIndex].Unload();
	unguard;
}

//
//	FStaticLightMapTexture::GetRawTextureData
//

void* FStaticLightMapTexture::GetRawTextureData(INT MipIndex)
{
	guard(FStaticLightMapTexture::GetRawTextureData);

	check( GIsEditor || GIsOpenGL || GIsPixomatic );

	if( !Data[MipIndex].Num() )
		Data[MipIndex].Load();

	return &Data[MipIndex](0);

	unguard;
}

//
//	FStaticLightMapTexture::GetFirstMip
//

INT FStaticLightMapTexture::GetFirstMip()
{ 
	// MERGE_HACK !!vogel
	if( UTexture::__Client && (UTexture::__Client->GetTextureLODBias(LODSET_Lightmap)>0) )
		return 1;
	else
		return 0;
}

