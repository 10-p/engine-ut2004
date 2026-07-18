/*=============================================================================
	UnSparkEmitter.cpp: Unreal Spark Emitter
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#include "EnginePrivate.h"

#ifdef __PSX2_EE__  // I apologize profusely - JA
#include "../../PSX2Render/Src/VU1Support.h"
#endif

/*-----------------------------------------------------------------------------
	USparkEmitter.
-----------------------------------------------------------------------------*/

//!! TODO: TimeBeforeVisible?!

IMPLEMENT_CLASS(USparkEmitter);

void USparkEmitter::PostEditChange()
{
	Super::PostEditChange();
	CleanUp();
	Initialize( MaxParticles );
}

// Initializes the Emitter.
void USparkEmitter::Initialize( INT InMaxParticles )
{
	guard(USparkEmitter::Initialize);
	Super::Initialize( InMaxParticles );
	NumSegments = (INT)Max( 2.f, LineSegmentsRange.GetMax() );
	VerticesPerParticle		= 2 * NumSegments;
	IndicesPerParticle		= 2 * NumSegments;
	PrimitivesPerParticle	= NumSegments;
	SparkData.Add( InMaxParticles );
	
#ifdef __PSX2_EE__
	PreCachePS2Emitter((void**)&PS2Data, MaxParticles, VerticesPerParticle); // we are doing a mini strip
#endif
	unguard;
}

// Update particles.
INT USparkEmitter::UpdateParticles( FLOAT DeltaTime )
{
	guard(USparkEmitter::UpdateParticles);
	INT Value	= Super::UpdateParticles( DeltaTime );
	BoundingBox = BoundingBox.ExpandBy( MaxSizeScale );
	return Value;
	unguard;
}

// CleanUp.
void USparkEmitter::CleanUp()
{
	guard(USparkEmitter::CleanUp);
	Super::CleanUp();
	SparkData.Empty();
	unguard;
}

void USparkEmitter::SpawnParticle( INT Index, FLOAT SpawnTime, INT Flags, INT SpawnFlags, const FVector& LocalLocationOffset )
{
	guard(USparkEmitter::SpawnParticle);
	Super::SpawnParticle( Index, SpawnTime, Flags, SpawnFlags, LocalLocationOffset );
	SparkData(Index).TimeBeforeVisible	 = TimeBeforeVisibleRange.GetRand();
	SparkData(Index).TimeBetweenSegments = TimeBetweenSegmentsRange.GetRand(); 
	SparkData(Index).StartLocation		 = Particles(Index).Location;
	SparkData(Index).StartVelocity		 = Particles(Index).Velocity;
	unguard;
}


#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,1)
#endif

struct FSparkVertex
{
	FVector	Position GCC_PACK(1);
	FColor	Diffuse GCC_PACK(1);
	FLOAT	U GCC_PACK(1);
	FLOAT	V GCC_PACK(1);

	static INT GetComponents(FVertexComponent* OutComponents)
	{
		OutComponents[0].Type = CT_Float3;
		OutComponents[0].Function = FVF_Position;
		OutComponents[1].Type = CT_Color;
		OutComponents[1].Function = FVF_Diffuse;
		OutComponents[2].Type = CT_Float2;
		OutComponents[2].Function = FVF_TexCoord0;

		return 3;
	}
};

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif


class FSparkVertexStream : public FParticleVertexStream<FSparkVertex>
{
public:

	USparkEmitter*				Emitter;
	INT							NumParticles;

	// Constructor.
	FSparkVertexStream(USparkEmitter* InEmitter,INT InNumParticles)
	{
		Emitter = InEmitter;
		NumParticles = InNumParticles;
	}

	virtual INT GetSize()
	{
		return NumParticles * Emitter->VerticesPerParticle * sizeof(FSparkVertex);
	}

	// Generates the spark vertices procedurally.
	virtual void GetStreamData(void* Dest)
	{
		FSparkVertex*	Vertices = (FSparkVertex*) Dest;
		INT				VertexIndex = 0;
		FLOAT			RNumSegments = 1.0f / Emitter->NumSegments;

#ifdef __PSX2_EE__
		FVU1ParticleData* PatchData = ((FVU1ParticleData*)Emitter->PS2Data);
		FVU1ParticlePatch* Patch = PatchData->CurrentBuffer;
		INT PatchVertexIndex = 0;
#endif
		for(INT ParticleIndex = 0;ParticleIndex < Emitter->ActiveParticles;ParticleIndex++)
		{
			FParticle*	Particle = &Emitter->Particles(ParticleIndex);

			if(!(Particle->Flags & PTF_Active))
				continue;

			for(INT SegmentIndex = 0;SegmentIndex < Emitter->NumSegments;SegmentIndex++)
			{
				FLOAT	t		 = Max( 0.f, Particle->Time - SegmentIndex * Emitter->SparkData(ParticleIndex).TimeBetweenSegments );
				FVector	Location = Emitter->SparkData(ParticleIndex).StartLocation + Emitter->SparkData(ParticleIndex).StartVelocity*t + Emitter->Acceleration*t*t;

				t				 = Max( 0.f, Particle->Time - (SegmentIndex+1) * Emitter->SparkData(ParticleIndex).TimeBetweenSegments );
				FVector	Location2= Emitter->SparkData(ParticleIndex).StartLocation + Emitter->SparkData(ParticleIndex).StartVelocity*t + Emitter->Acceleration*t*t;
				FColor  Color    = Particle->Color.RenderColor();
				
#ifdef __PSX2_EE__
				{
				FVU1ParticleVertex& PVertex = Patch->Vertices[PatchVertexIndex++];

				PVertex.Pos.X = Location.X;
				PVertex.Pos.Y = Location.Y; 
				PVertex.Pos.Z = Location.Z;
				PVertex.Color.ui32[2] = Particle->Color.R>>1;
				PVertex.Color.ui32[1] = Particle->Color.G>>1;
				PVertex.Color.ui32[0] = Particle->Color.B>>1;
				PVertex.Color.ui32[3] = Particle->Color.A>>1;
				PVertex.UV.X = 0.f;
				PVertex.UV.Y = RNumSegments * SegmentIndex;
				}

				{
				FVU1ParticleVertex& PVertex = Patch->Vertices[PatchVertexIndex++];

				PVertex.Pos.X = Location2.X;
				PVertex.Pos.Y = Location2.Y; 
				PVertex.Pos.Z = Location2.Z;
				PVertex.Color.ui32[2] = Particle->Color.R>>1;
				PVertex.Color.ui32[1] = Particle->Color.G>>1;
				PVertex.Color.ui32[0] = Particle->Color.B>>1;
				PVertex.Color.ui32[3] = Particle->Color.A>>1;
				PVertex.UV.X = 1.0f;
				PVertex.UV.Y = RNumSegments * SegmentIndex;
				}

				if (PatchVertexIndex + 2 > P_MAX_VERTICES_PER_PATCH)
				{
					// set the length of this in vertices
					Patch->Params.ui32[0] = PatchVertexIndex;

					// tell it that we are a "cnt" type of tag, not an "end"

					Patch->DMATag.ui32[0] &= ~0x70000000;
					Patch->DMATag.ui32[0] |= 0x10000000;

					Patch++;
					PatchVertexIndex = 0;
				}

#else
				Vertices[VertexIndex].Position	= Location;
				Vertices[VertexIndex].Diffuse	= Color;
				Vertices[VertexIndex].U			= 0;
				Vertices[VertexIndex].V			= RNumSegments * SegmentIndex;
				VertexIndex++;

				Vertices[VertexIndex].Position	= Location2;
				Vertices[VertexIndex].Diffuse	= Color;
				Vertices[VertexIndex].U			= 1.f;
				Vertices[VertexIndex].V			= RNumSegments * SegmentIndex;
				VertexIndex++;
#endif
			}
		}
		
#ifdef __PSX2_EE__
		// are we at the end of the list?
		if (PatchVertexIndex == 0)
			Patch--;
		else
			// set the length of this in vertices
			Patch->Params.ui32[0] = PatchVertexIndex;

#ifdef NO_DEFERRED_RENDERING
					// tell it that we are a "end" type of tag, not an "cnt"
		Patch->DMATag.ui32[0] |= 0x70000000;
#else
					// tell it that we are a "ret" type of tag, not an "cnt"
		Patch->DMATag.ui32[0] &= ~0x70000000;
		Patch->DMATag.ui32[0] |= 0x60000000;
#endif
#endif
	}
};

// Renders all active particles
INT USparkEmitter::RenderParticles(FDynamicActor* DynActor,FLevelSceneNode* SceneNode,TList<FDynamicLight*>* Lights,FRenderInterface* RI)
{
	guard(USparkEmitter::RenderParticles);

	UParticleEmitter::RenderParticles( DynActor, SceneNode, Lights, RI );

	if ( Texture == NULL )
		return 0;

	INT RenderedParticles = 0;

	// Render the particles.
	if ( ActiveParticles > 0 )
	{
		// Calculate the number of active particles.
		for(INT ParticleIndex = 0;ParticleIndex < ActiveParticles;ParticleIndex++)
		{
			FParticle* Particle = &Particles(ParticleIndex);

			if(Particle->Flags & PTF_Active)
				RenderedParticles++;
		}

		// Bail out if nothing to render.
		if (!RenderedParticles)
			return 0;

		// Create the dynamic vertex stream and index buffer.
		FSparkVertexStream SparkVertices(this,RenderedParticles);

		// Rotate & translate if needed.
		if ( CoordinateSystem == PTCS_Relative )
		{
			FMatrix	LocalToWorld = FRotationMatrix(Owner->Rotation) * FTranslationMatrix(Owner->Location);

			RI->SetTransform(TT_LocalToWorld,LocalToWorld);
		} 
		else
			RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);

		// Set texture and blending.
		Owner->ParticleMaterial->ParticleBlending			= DrawStyle;
		if( SceneNode->Viewport->Actor && SceneNode->Viewport->Actor->RendMap == REN_LightingOnly )
			Owner->ParticleMaterial->BitmapMaterial			= NULL;
		else
			Owner->ParticleMaterial->BitmapMaterial			= Texture;
		Owner->ParticleMaterial->BlendBetweenSubdivisions	= 0;
		Owner->ParticleMaterial->RenderTwoSided				= 1;
		Owner->ParticleMaterial->UseTFactor					= 0;
		Owner->ParticleMaterial->AlphaTest					= AlphaTest;
		Owner->ParticleMaterial->AlphaRef					= AlphaRef;
		Owner->ParticleMaterial->AcceptsProjectors			= AcceptsProjectors;
		Owner->ParticleMaterial->ZTest						= ZTest;
		Owner->ParticleMaterial->ZWrite						= ZWrite;
		Owner->ParticleMaterial->Wireframe					= SceneNode->Viewport->IsWire();
		
		RI->EnableLighting(0,1);
		RI->SetMaterial(Owner->ParticleMaterial);

#ifdef __PSX2_EE__
		// Sort of a weird hack to fill out the PS2Data
		SparkVertices.GetStreamData(NULL);
		FVU1ParticleData* PatchData = ((FVU1ParticleData*)PS2Data);
		
		extern void PSX2Render_RenderEmitter(FVU1ParticlePatch* Patches, UBOOL LineList, EParticleDrawStyle DrawStyle);
		PSX2Render_RenderEmitter(PatchData->CurrentBuffer, true, (EParticleDrawStyle)DrawStyle);
		PatchData->SwapBuffers();
#else
		// Set the particle vertex stream and index buffer.
		// The particle vertices aren't actually generated until now.
		INT	BaseVertexIndex = RI->SetDynamicStream(VS_FixedFunction,&SparkVertices);

		RI->SetIndexBuffer(NULL,0);

		// Draw the particles.
		RI->DrawPrimitive(
			PT_LineList,
			BaseVertexIndex,
			RenderedParticles * PrimitivesPerParticle
			);
#endif
	}

	return RenderedParticles;

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

