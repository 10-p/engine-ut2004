/*=============================================================================
	PixoRenderInterface.cpp: Unreal Pixo support.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Michael Sartain from GL driver
		* Mix lighting fixes by Daniel Vogel
=============================================================================*/

#include "PixoDrv.h"

#if !PURE_C
// mmx register struct
struct MMREG
{
	union
	{
		struct { WORD B, G, R, A; };
		__int64 reg;
	};
};
#endif

//
// Lighting info structure for each light used in PixoLightingVertexCallback.
//
typedef struct PixoLightInfo
{
	// shared
	int     Type;
#if PURE_C
	float   Diffuse[3];
#else
	MMREG   Diffuse_8_6;
#endif

	// directional
	float   LightDirection[3];

	// point
	float   LightPosition[3];
	float   RadiusSquared;
} PixoLightInfo;

//
// Static lighting structure set up by CommitLights
//  and used by PixoLightingVertexCallback.
//
static struct FPixoLights
{
	FPixoLights() {};
	~FPixoLights() {};

#if PURE_C
	float       AmbientLightColor[3];
#else
	MMREG       AmbientLightColor_8_2;
#endif

	FMatrix     ObjectToScreenInverse;

	INT         PixoLightInfoCount;
	PixoLightInfo LightInfo[8];

	// Stream information for point lights
	int         StreamPitch;
	float       *StreamPos;

} GLights;

//
// Given a Vertex, find the local object position coordinates for that guy
//  and subract it from the LightPosition which should give you
//  LightDirection for your point light.
//
static inline void GetPointLightDirection(PIXO_VERTEX *Vert, PIXO_CLIP_INFO *ClipInfo, int i, float *LightDirection)
{
	FVector ObjectPos;
	float *ObjectPosition;

	if(Vert->Index < 0)
	{
		// Ok. We have a vert with an index of -1 which means this dude has
		//  been through the clipper and there is no corresponding vertex.
		// So take the homogeneous coordinates and back project them into
		//  local object space.
		FPlane *ScreenPosition = (FPlane *)ClipInfo->posxyzw;

		ObjectPos = GLights.ObjectToScreenInverse.TransformFPlane(*ScreenPosition);
		ObjectPosition = &ObjectPos.X;
	}
	else
	{
		// We've got an index. Find the local position coordinates.
		ObjectPosition = GLights.StreamPos + Vert->Index * GLights.StreamPitch;
	}

	float *LightPosition = GLights.LightInfo[i].LightPosition;

	LightDirection[0] = LightPosition[0] - ObjectPosition[0];
	LightDirection[1] = LightPosition[1] - ObjectPosition[1];
	LightDirection[2] = LightPosition[2] - ObjectPosition[2];
}

//
//  PixoLightingVertexCallback
//
static void PixoLightingVertexCallback(PIXO_VERTEX *Vert, PIXO_CLIP_INFO *ClipInfo)
{
#if !PURE_C

	int nLightInfoCount = 0;
	UINT NdotL_8_2[8];
	MMREG Diffuse_8_6[8];

#else

	static const float OneOver255 = 1.0f / 255.0f;
	float Material[3] =
	{
		((Vert->Specular >> 16) & 0xff) * OneOver255,
		((Vert->Specular >>  8) & 0xff) * OneOver255,
		((Vert->Specular >>  0) & 0xff) * OneOver255
	};

	float LightValue[3] =
	{
		// LightValue = Global_Ambient * Material + Diffuse
		GLights.AmbientLightColor[0] * Material[0],
		GLights.AmbientLightColor[1] * Material[1],
		GLights.AmbientLightColor[2] * Material[2]
	};

#endif

	for(INT i = 0; i < GLights.PixoLightInfoCount; i++)
	{
		PixoLightInfo *LightInfo = &GLights.LightInfo[i];

		if(LightInfo->Type & PIXO_LIGHT_POINT)
		{
			float LightDirection[3];

			GetPointLightDirection(Vert, ClipInfo, i, LightDirection);

			float DistanceSquared =
				LightDirection[0] * LightDirection[0] +
				LightDirection[1] * LightDirection[1] +
				LightDirection[2] * LightDirection[2];

			float NdotL = (Vert->Nx * LightDirection[0] + Vert->Ny * LightDirection[1] + Vert->Nz * LightDirection[2]);
	
			if(NdotL <= 0)
				continue;

			NdotL /= appSqrt( DistanceSquared ) * FVector( Vert->Nx, Vert->Ny, Vert->Nz ).Size();

			// Neither Unreal nor D3D's lighting model though looks good.
			FLOAT Attenuation = Max( 0.f, 1.f - 1.f * DistanceSquared / LightInfo->RadiusSquared );
				 
			//$ Should we handle non quadratic non incidence also?
			if(LightInfo->Type != PIXO_LIGHT_POINT_QUADRATIC_NON_INCIDENCE)
				NdotL *= Attenuation;
			else
				NdotL = Attenuation;

#if PURE_C
			float *Diffuse = LightInfo->Diffuse;

			LightValue[0] += NdotL * Material[0] * Diffuse[0];
			LightValue[1] += NdotL * Material[1] * Diffuse[1];
			LightValue[2] += NdotL * Material[2] * Diffuse[2];
#else
			NdotL_8_2[nLightInfoCount] = Min<UINT>( NdotL * 256 * 4, 256 * 4 - 1 ); 
			Diffuse_8_6[nLightInfoCount] = LightInfo->Diffuse_8_6;
			nLightInfoCount++;
#endif
		}
		else
		{
			check(LightInfo->Type == PIXO_LIGHT_DIRECTIONAL);

			float *LightDirection = LightInfo->LightDirection;

			float NdotL =
				Vert->Nx * LightDirection[0] +
				Vert->Ny * LightDirection[1] +
				Vert->Nz * LightDirection[2];

			if(NdotL > 0)
			{
				NdotL /= FVector( Vert->Nx, Vert->Ny, Vert->Nz ).Size();
#if PURE_C
				float *Diffuse = LightInfo->Diffuse;

				LightValue[0] += NdotL * Material[0] * Diffuse[0];
				LightValue[1] += NdotL * Material[1] * Diffuse[1];
				LightValue[2] += NdotL * Material[2] * Diffuse[2];
#else
				NdotL_8_2[nLightInfoCount] = Min<UINT>( NdotL * 256 * 4, 256 * 4 - 1 ); 
				Diffuse_8_6[nLightInfoCount] = LightInfo->Diffuse_8_6;
				nLightInfoCount++;
#endif
			}
		}
	}

#if !PURE_C

	static const __int64 mm_zero = 0;
	static const __int64 mm_alpha_mask = 0xff000000;

	__asm
	{
		mov eax, dword ptr [Vert]
		movd mm0, [eax]Vert.Specular
		punpcklbw mm0, [mm_zero]                // material(8.0)
		psllw mm0, 6                            // material(8.6)

		movq mm7, GLights.AmbientLightColor_8_2 // ambient(8.2)
		
		pmulhw mm7, mm0                         // ambient(8.2) * material(8.6)
	
		mov ecx, nLightInfoCount
		jcxz loop_done

	do_loop:
		dec ecx

		movd mm1, [NdotL_8_2 + ecx * SIZE UINT]
		punpcklwd mm1, mm1
		punpckldq mm1, mm1

		movq mm2, [Diffuse_8_6 + ecx * SIZE MMREG]

		pmulhw mm1, mm2                         // NdotL(8.2) * Diffuse(8.6)
		pmulhw mm1, mm0                         // (NdotL*Diffuse)(8.2) * Material(8.6)

		paddusw mm7, mm1

		jnz do_loop

	loop_done:
		psllw mm7, 2
	
		packuswb mm7, mm7

		movd mm6, [eax]Vert.Diffuse
		pand mm6, [mm_alpha_mask]
		por mm7, mm6
		movd [eax]Vert.Diffuse, mm7

		emms
	}

#else   // PURE_C

	PIXO_ARGB0 FinalDiffuse = PixoMakeArgb04f(
		0,
		LightValue[0],
		LightValue[1],
		LightValue[2]);
	Vert->Diffuse = (Vert->Diffuse & 0xff000000) | FinalDiffuse;

#endif // !PURE_C
}

void PixoLightUpdateStreamData()
{
	PIXO_STREAMDEFCODEBUFFER *codebuf = PixoGetStreamDefCodeBuffer();

	GLights.StreamPitch = codebuf->stream_pitch[codebuf->stream_pos_num] >> 2;
	GLights.StreamPos = (float *)PixoGetStream(codebuf->stream_pos_num) +
		codebuf->stream_pos_start;
}

UBOOL PixoCommitLights(FPixoRenderInterface::FPixoSavedState *CurrentState)
{
	CurrentState->LightsDirty = 0;

	INT LightCount = 0;

	if( CurrentState->UseDynamicLighting && CurrentState->LightsEnabled )
	{
		FMatrix *WorldToLocal		= &CurrentState->LocalToWorldInverse;
		FMatrix *ObjectToScreen		= (FMatrix *)PixoGetConcatenatedTransformMatrix();
		FLOAT AverageScaling		= appPow( fabs(WorldToLocal->Determinant()), 1.f / 3.f ); // doesn't work for non- uniform scaling

		GLights.ObjectToScreenInverse = ObjectToScreen->Inverse();

		for(INT LightIndex = 0; LightIndex < 8; LightIndex++)
		{
			if( !(CurrentState->LightsEnabled & (1 << LightIndex)) )
				continue;

			FPixoLightState *PixoLight = &CurrentState->Lights[LightIndex];
			PixoLightInfo *LightInfo = &GLights.LightInfo[LightCount];

			LightInfo->Type = PixoLight->Type;

#if PURE_C
			LightInfo->Diffuse[0] = PixoLight->Diffuse.X;
			LightInfo->Diffuse[1] = PixoLight->Diffuse.Y;
			LightInfo->Diffuse[2] = PixoLight->Diffuse.Z;
#else
			LightInfo->Diffuse_8_6.A = 0;
			LightInfo->Diffuse_8_6.R = PixoLight->Diffuse.X * 256 * 64;
			LightInfo->Diffuse_8_6.G = PixoLight->Diffuse.Y * 256 * 64;
			LightInfo->Diffuse_8_6.B = PixoLight->Diffuse.Z * 256 * 64;
#endif

			if(PixoLight->Type & PIXO_LIGHT_POINT)
			{
				FPlane LightPosition;

				PixoTransformVector(
					&LightPosition.X,
					&WorldToLocal->M[0][0],
					&PixoLight->Position.X);

				LightInfo->LightPosition[0] = LightPosition.X;
				LightInfo->LightPosition[1] = LightPosition.Y;
				LightInfo->LightPosition[2] = LightPosition.Z;

				LightInfo->RadiusSquared = Square( PixoLight->Radius * AverageScaling );
	
				LightCount++;
			}
			else if(PixoLight->Type & PIXO_LIGHT_DIRECTIONAL)
			{
				FPlane LightDirection;

				PixoTransformNormal(
					&LightDirection.X,
					&WorldToLocal->M[0][0],
					&PixoLight->Direction.X);

				LightDirection.Normalize();
				LightInfo->LightDirection[0] = LightDirection.X;
				LightInfo->LightDirection[1] = LightDirection.Y;
				LightInfo->LightDirection[2] = LightDirection.Z;

				LightCount++;
			}
		}
	}

	if( CurrentState->UseDynamicLighting )
	{
		FPlane AmbientLightColor = CurrentState->AmbientLightColor.Plane();
		if( CurrentState->UseStaticLighting && CurrentState->HasDiffuse )		
#if PURE_C
			AmbientLightColor += FPlane( 1.0f, 1.0f, 1.0f, 1.0f );
#else
			AmbientLightColor += FPlane( 0.25f, 0.25f, 0.25f, 0.25f );
#endif

#if PURE_C
		GLights.AmbientLightColor[0] = AmbientLightColor.X;
		GLights.AmbientLightColor[1] = AmbientLightColor.Y;
		GLights.AmbientLightColor[2] = AmbientLightColor.Z;
#else
		GLights.AmbientLightColor_8_2.A = 0;
		GLights.AmbientLightColor_8_2.R = AmbientLightColor.X * 256 * 4;
		GLights.AmbientLightColor_8_2.G = AmbientLightColor.Y * 256 * 4;
		GLights.AmbientLightColor_8_2.B = AmbientLightColor.Z * 256 * 4;
#endif

		GLights.PixoLightInfoCount = LightCount;
		PixoSetVertexCallback(PixoLightingVertexCallback, 0);
		return 1;
	}
	else
	{
		PixoSetVertexCallback(NULL, 0);
		return 0;
	}
}

