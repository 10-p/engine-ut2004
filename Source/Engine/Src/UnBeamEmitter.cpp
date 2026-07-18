/*=============================================================================
	UnBeamEmitter.cpp: Unreal Beam Emitter
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel

		I can't express in words how embarrased I am by this code. Sorry!

=============================================================================*/

#include "EnginePrivate.h"

#ifdef __PSX2_EE__  // I apologize profusely - JA
#include "../../PSX2Render/Src/VU1Support.h"
#endif

/*-----------------------------------------------------------------------------
	USpriteEmitter.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UBeamEmitter);

void UBeamEmitter::PostEditChange()
{
	guard(UBeamEmitter::PostEditChange);
	Super::PostEditChange();
	CleanUp();
	HighFrequencyPoints = Max( 2, HighFrequencyPoints );
	LowFrequencyPoints	= Max( 2, LowFrequencyPoints );
	Initialize( MaxParticles );
	unguard;
}


// Initializes the Emitter.
void UBeamEmitter::Initialize( INT InMaxParticles )
{
	guard(UBeamEmitter::Initialize);
	Super::Initialize( InMaxParticles );
	if ( RotatingSheets )
		SheetsUsed = RotatingSheets;
	else
		SheetsUsed = 1;
	VerticesPerParticle		= 2 * (HighFrequencyPoints) * SheetsUsed;
	IndicesPerParticle		= 6 * (HighFrequencyPoints-1) * SheetsUsed;
	PrimitivesPerParticle	= 2 * (HighFrequencyPoints-1) * SheetsUsed;
	HFPoints.Add( HighFrequencyPoints*InMaxParticles );
	LFPoints.Add( LowFrequencyPoints*InMaxParticles );
	UpdateActorHitList();
	
#ifdef __PSX2_EE__
	PreCachePS2Emitter((void**)&PS2Data, MaxParticles, 2 * VerticesPerParticle); // we are doing a mini strip
#endif

	unguard;
}


// CleanUp.
void UBeamEmitter::CleanUp()
{
	guard(UBeamEmitter::CleanUp);
	HFPoints.Empty();
	LFPoints.Empty();
	Super::CleanUp();
	unguard;
}


// Finds first Actor with corresponding tag.
void UBeamEmitter::UpdateActorHitList()
{
	if (!Owner)
		return;
	HitActors.Empty();
	HitActors.Add( BeamEndPoints.Num() );
	ULevel* Level = Owner->GetLevel();
	for (INT i=0; i<BeamEndPoints.Num(); i++)
	{
		HitActors(i) = NULL;
		for (INT n=0; n<Level->Actors.Num(); n++)
		{
			if ( Level->Actors(n) && (Level->Actors(n)->Tag == BeamEndPoints(i).ActorTag ))
			{
				HitActors(i) = Level->Actors(n);
				break;
			}
		}
	}
}


void UBeamEmitter::Scale( FLOAT ScaleFactor )
{
	guard(UBeamEmitter::Scale);
	Super::Scale( ScaleFactor );
	for (INT i=0; i<BeamEndPoints.Num(); i++)
		BeamEndPoints(i).Offset *= ScaleFactor;
	BeamDistanceRange		*= ScaleFactor;
	LowFrequencyNoiseRange  *= ScaleFactor;
	HighFrequencyNoiseRange *= ScaleFactor;
	unguard;
}


void UBeamEmitter::SpawnParticle( INT Index, FLOAT SpawnTime, INT Flags, INT SpawnFlags, const FVector& LocalLocationOffset )
{
	guard(UBeamEmitter::SpawnParticle);
	Super::SpawnParticle( Index, SpawnTime, Flags, SpawnFlags, LocalLocationOffset );

	FParticle& Particle = Particles(Index);

	FVector	Direction = FVector(0,0,0);
	FVector	EndPoint  = FVector(0,0,0);
	UBOOL SetEndPoint = false;

	// Calculate which actor/ offset to use for endpoint determination.
	INT EPIndex       = -1;
	switch ( DetermineEndPointBy )
	{
	case PTEP_Offset:
	case PTEP_OffsetAsAbsolute:
	case PTEP_TraceOffset:
	case PTEP_Actor:
		if ( BeamValueSum )
		{
			FLOAT Random = appFrand() * BeamValueSum;
			FLOAT Sum	 = 0;
			for (INT i=0; i<BeamEndPoints.Num(); i++)
			{
				if ( (Sum < Random) && (Random <= (Sum + BeamEndPoints(i).Weight) ) )
				{
					EPIndex = i;
					break;
				}
				Sum += BeamEndPoints(i).Weight;
			}

		}
		if( (EPIndex == -1) && BeamEndPoints.Num() )
			EPIndex = 0;
		break;
	default:
		break;
	}

	if ( (DetermineEndPointBy == PTEP_TraceOffset) && (CoordinateSystem == PTCS_Relative) )
			debugf(TEXT("PTCS_Relative is not supported in conjunction with PTEP_TraceOffset"));

	// Setup endpoint.
	switch ( DetermineEndPointBy )
	{
	case PTEP_Velocity :
		Direction = Particle.MaxLifetime * Particle.Velocity;
		break;
	case PTEP_Distance :
		Particle.Velocity.Normalize();
		Direction = BeamDistanceRange.GetRand() * Particle.Velocity;
		break;
	case PTEP_DynamicDistance :
		Particle.Velocity.Normalize();
		Direction = BeamDistanceRange.GetRand() * Particle.Velocity * Particle.Size.Y;
		break;

	case PTEP_Offset :
		if ( EPIndex != -1 )
		{
			FVector Offset = BeamEndPoints(EPIndex).Offset.GetRand();
			Direction = Offset;
			EndPoint  = Offset + Particle.Location;
		}
		SetEndPoint = true;
		break;
	case PTEP_OffsetAsAbsolute:
		if ( EPIndex != -1 )
		{
			FVector Offset = BeamEndPoints(EPIndex).Offset.GetRand();
			Direction = Offset - Particle.Location;
			EndPoint  = Offset;
		}
		SetEndPoint = true;
		break;
	case PTEP_TraceOffset :
		if ( EPIndex != -1 )
		{
			FVector Offset = BeamEndPoints(EPIndex).Offset.GetRand();
			Direction = Offset;
			EndPoint  = Offset + Particle.Location;

			FCheckResult Hit;
			if ( !Owner->GetLevel()->SingleLineCheck( 
				Hit,
				Owner,  
				EndPoint,
				Particle.Location,
				TRACE_AllBlocking
				))
			{
				EndPoint  = Hit.Location;
				Direction = EndPoint - Particle.Location;
			}
		}
		SetEndPoint = true;
		break;
	case PTEP_Actor :
		if ( (EPIndex != -1) && (EPIndex < HitActors.Num()) && HitActors(EPIndex) )
		{
			Direction = HitActors(EPIndex)->Location - Particle.Location;
			EndPoint  = HitActors(EPIndex)->Location;
			if( TriggerEndpoint )
				HitActors(EPIndex)->eventTrigger( Owner, NULL );
		}
		SetEndPoint = true;
		break;
	default:
		break;
	}

	if ( NoiseDeterminesEndPoint )
		SetEndPoint = false;

	// Handle low & high frequency noise by modulating the high frequency on top of the low frequency one.
	FLOAT RHF    = 1.f / (HighFrequencyPoints-1);
	FLOAT RLF    = 1.f / (LowFrequencyPoints-1);
	FLOAT TH     = RHF;
	FLOAT TL     = 0;
	FLOAT RLFPs = 1.f / LowFrequencyPoints;
	FLOAT RHFPs = 1.f / HighFrequencyPoints;
	FVector* LFPoint = &LFPoints(Index * LowFrequencyPoints);
	*(LFPoint++) = Particle.Location;
	FVector LFNoise = FVector(0,0,0);
	for ( INT i=1; i<LowFrequencyPoints; i++ )
	{
		if ( UseLowFrequencyScale )
		{
			//!!TODO: see UnParticleEmitter.cpp
			FLOAT RelativeLength = appFractional( i * RLFPs * (LFScaleRepeats+1) );
			for (INT n=0; n<LFScaleFactors.Num(); n++)
			{
				if ( LFScaleFactors(n).RelativeLength >= RelativeLength )
				{
					FLOAT R1;
					FLOAT R2  = LFScaleFactors(n).RelativeLength;
					FVector V1;
					FVector V2 = LFScaleFactors(n).FrequencyScale;
					if (n)
					{
						V1 = LFScaleFactors(n-1).FrequencyScale;
						R1 = LFScaleFactors(n-1).RelativeLength;
					}
					else
					{
						V1 = FVector(1,1,1);
						R1 = 0.f;
					}
					FLOAT A;
					if ( R2 )
						A = (RelativeLength - R1) / (R2 - R1);
					else
						A = 1.f;
	
					// Interpolate between two vectors and apply noise.
					LFNoise = Lerp(V1, V2, A) * HighFrequencyNoiseRange.GetRand();
					break;			
				}
			}	
		}
		else
			LFNoise = LowFrequencyNoiseRange.GetRand();
		*(LFPoint++) = Particle.Location + Direction * i * RLF + LFNoise;
	}
	if ( SetEndPoint )
		*(--LFPoint) = EndPoint; 
	LFPoint = &LFPoints(Index * LowFrequencyPoints);
	INT LFMaxIndex = 1;
	FParticleBeamData* HFPoint = &HFPoints(Index * HighFrequencyPoints);
	HFPoint->Location	= *LFPoint;
	HFPoint->t			= 0;
	HFPoint++;
	FVector HFNoise = FVector(0,0,0);
	for ( INT i=1; i<HighFrequencyPoints; i++ )
	{
		if ( UseHighFrequencyScale )
		{
			FLOAT RelativeLength = appFractional( i * RHFPs * (HFScaleRepeats+1) );
			for (INT n=0; n<HFScaleFactors.Num(); n++)
			{
				if ( HFScaleFactors(n).RelativeLength >= RelativeLength )
				{
					FLOAT R1;
					FLOAT R2  = HFScaleFactors(n).RelativeLength;
					FVector V1;
					FVector V2 = HFScaleFactors(n).FrequencyScale;
					if (n)
					{
						V1 = HFScaleFactors(n-1).FrequencyScale;
						R1 = HFScaleFactors(n-1).RelativeLength;
					}
					else
					{
						V1 = FVector(1,1,1);
						R1 = 0.f;
					}
					FLOAT A;
					if ( R2 )
						A = (RelativeLength - R1) / (R2 - R1);
					else
						A = 1.f;
	
					// Interpolate between two vectors and apply noise.
					HFNoise = Lerp(V1, V2, A) * HighFrequencyNoiseRange.GetRand();
					break;			
				}
			}
		}
		else
			HFNoise = HighFrequencyNoiseRange.GetRand();
		TL = TH * (LowFrequencyPoints-1);
		if( LFMaxIndex < LowFrequencyPoints )
			HFPoint->Location = (1-TL) * (*LFPoint) + TL * (*(LFPoint+1)) + HFNoise;
		else
			HFPoint->Location = *LFPoint + HFNoise;
		HFPoint->t = i * RHF;

		// Spawn particle in another emitter.
		if ( UseBranching 
		&&   (i>=BranchHFPointsRange.Min && i<=BranchHFPointsRange.Max) 
		&&   (BranchProbability.GetRand() >= appFrand()) 
		)
		{
			if ( BranchEmitter >= 0 )
			{
				UParticleEmitter* OtherEmitter = Owner->Emitters(BranchEmitter);
				
				if ( OtherEmitter->Initialized && OtherEmitter != this ) //&& SpawnTime ) //!! vogel: ?!
				{
					// In case OtherEmitter hasn't been ticked yet.
					OtherEmitter->Owner = Owner;

					INT Amount = (INT)BranchSpawnAmountRange.GetRand();
					for (INT i=0; i<Amount; i++)
					{
						OtherEmitter->SpawnParticle( OtherEmitter->ParticleIndex, SpawnTime, 0, PSF_NoGlobalOffset | PSF_NoOwnerLocation, HFPoint->Location );
						// Let particles die at the same time.
						if ( LinkupLifetime )
						{
							FParticle& OtherParticle = OtherEmitter->Particles(OtherEmitter->ParticleIndex);
							OtherParticle.Time = Particle.Time;
							OtherParticle.MaxLifetime = Particle.MaxLifetime;
						}
						OtherEmitter->ActiveParticles = Max(OtherEmitter->ActiveParticles, OtherEmitter->ParticleIndex+1);
						(++OtherEmitter->ParticleIndex) %= OtherEmitter->MaxActiveParticles;
					}		   
				}
			}
		}

		HFPoint++;
		TH += RHF;
		if( TH > RLF)
		{
			TH -= RLF;
			if( LFMaxIndex++ < LowFrequencyPoints )
				LFPoint++;
		}
	}
	if ( SetEndPoint )
		(--HFPoint)->Location = EndPoint;

	unguard;
}

void UBeamEmitter::UpdateBeamLength( FLOAT DeltaTime )
{
	guard(UBeamEmitter::UpdateBeamLength)

	for (INT Index=0; Index<ActiveParticles; Index++)
	{
		if ( !(Particles(Index).Flags & PTF_Active) )
			continue;

		FParticle& Particle = Particles(Index);

		FVector	Direction = FVector(0,0,0);
		FVector	EndPoint  = FVector(0,0,0);

		Particle.Velocity.Normalize();
		Direction = BeamDistanceRange.GetRand() * Particle.Velocity * Particle.Size.Y;

		// Handle low & high frequency noise by modulating the high frequency on top of the low frequency one.
		FLOAT RHF    = 1.f / (HighFrequencyPoints-1);
		FLOAT RLF    = 1.f / (LowFrequencyPoints-1);
		FLOAT TH     = RHF;
		FLOAT TL     = 0;
		FLOAT RLFPs = 1.f / LowFrequencyPoints;
		FLOAT RHFPs = 1.f / HighFrequencyPoints;
		FVector* LFPoint = &LFPoints(Index * LowFrequencyPoints);
		*(LFPoint++) = Particle.Location;
		FVector LFNoise = FVector(0,0,0);
		for ( INT i=1; i<LowFrequencyPoints; i++ )
		{
			if ( UseLowFrequencyScale )
			{
				//!!TODO: see UnParticleEmitter.cpp
				FLOAT RelativeLength = appFractional( i * RLFPs * (LFScaleRepeats+1) );
				for (INT n=0; n<LFScaleFactors.Num(); n++)
				{
					if ( LFScaleFactors(n).RelativeLength >= RelativeLength )
					{
						FLOAT R1;
						FLOAT R2  = LFScaleFactors(n).RelativeLength;
						FVector V1;
						FVector V2 = LFScaleFactors(n).FrequencyScale;
						if (n)
						{
							V1 = LFScaleFactors(n-1).FrequencyScale;
							R1 = LFScaleFactors(n-1).RelativeLength;
						}
						else
						{
							V1 = FVector(1,1,1);
							R1 = 0.f;
						}
						FLOAT A;
						if ( R2 )
							A = (RelativeLength - R1) / (R2 - R1);
						else
							A = 1.f;
		
						// Interpolate between two vectors and apply noise.
						LFNoise = Lerp(V1, V2, A) * HighFrequencyNoiseRange.GetRand();
						break;			
					}
				}	
			}
			else
				LFNoise = LowFrequencyNoiseRange.GetRand();
			*(LFPoint++) = Particle.Location + Direction * i * RLF + LFNoise;
		}

		LFPoint = &LFPoints(Index * LowFrequencyPoints);
		INT LFMaxIndex = 1;
		FParticleBeamData* HFPoint = &HFPoints(Index * HighFrequencyPoints);
		HFPoint->Location	= *LFPoint;
		HFPoint->t			= 0;
		HFPoint++;
		FVector HFNoise = FVector(0,0,0);
		for ( INT i=1; i<HighFrequencyPoints; i++ )
		{
			if ( UseHighFrequencyScale )
			{
				FLOAT RelativeLength = appFractional( i * RHFPs * (HFScaleRepeats+1) );
				for (INT n=0; n<HFScaleFactors.Num(); n++)
				{
					if ( HFScaleFactors(n).RelativeLength >= RelativeLength )
					{
						FLOAT R1;
						FLOAT R2  = HFScaleFactors(n).RelativeLength;
						FVector V1;
						FVector V2 = HFScaleFactors(n).FrequencyScale;
						if (n)
						{
							V1 = HFScaleFactors(n-1).FrequencyScale;
							R1 = HFScaleFactors(n-1).RelativeLength;
						}
						else
						{
							V1 = FVector(1,1,1);
							R1 = 0.f;
						}
						FLOAT A;
						if ( R2 )
							A = (RelativeLength - R1) / (R2 - R1);
						else
							A = 1.f;
		
						// Interpolate between two vectors and apply noise.
						HFNoise = Lerp(V1, V2, A) * HighFrequencyNoiseRange.GetRand();
						break;			
					}
				}
			}
			else
				HFNoise = HighFrequencyNoiseRange.GetRand();
			TL = TH * (LowFrequencyPoints-1);
			if( LFMaxIndex < LowFrequencyPoints )
				HFPoint->Location = (1-TL) * (*LFPoint) + TL * (*(LFPoint+1)) + HFNoise;
			else
				HFPoint->Location = *LFPoint + HFNoise;
			HFPoint->t = i * RHF;

			HFPoint++;
			TH += RHF;
			if ( TH > RLF)
			{
				TH -= RLF;
				if( LFMaxIndex++ < LowFrequencyPoints )
					LFPoint++;
			}
		}
	}

	unguard;
}

INT UBeamEmitter::UpdateParticles( FLOAT DeltaTime )
{
	guard(UBeamEmitter::UpdateParticles)

	BeamValueSum = 0;
	for (INT i=0; i<BeamEndPoints.Num(); i++)
		BeamValueSum += BeamEndPoints(i).Weight;

	// Update list of actors to hit.
	if ( HitActors.Num() != BeamEndPoints.Num() )
		UpdateActorHitList();

	// Verify range of critical variables.
	if ( Owner )
	{
		if ( BranchEmitter >= 0 )
			BranchEmitter = Clamp(BranchEmitter, 0, Owner->Emitters.Num() - 1 );
	}
	else
		return 0;

	INT Value =	Super::UpdateParticles( DeltaTime );

	TimeSinceLastDynamicNoise += DeltaTime;

	// Update Length of beam...
	if ( DetermineEndPointBy == PTEP_DynamicDistance )
	{
		UpdateBeamLength( DeltaTime );
	}

	BoundingBox.Init();
	//!! TODO simply add start/ end and expand by noise
	for (INT Index=0; Index<ActiveParticles; Index++)
	{
		if ( !(Particles(Index).Flags & PTF_Active) )
			continue;

		// Bounding box.
		BoundingBox += HFPoints(Index * HighFrequencyPoints).Location;
		BoundingBox += HFPoints((Index+1) * HighFrequencyPoints - 1).Location;
	}

	// Expand the bounding box.
	FLOAT Expand = 0;
	// Take into account noise.
	Expand += Max3(
		HighFrequencyNoiseRange.X.Size(), 
		HighFrequencyNoiseRange.Y.Size(), 
		HighFrequencyNoiseRange.Z.Size());
	Expand += Max3( 
		LowFrequencyNoiseRange.X.Size(), 
		LowFrequencyNoiseRange.Y.Size(), 
		LowFrequencyNoiseRange.Z.Size());
	BoundingBox = BoundingBox.ExpandBy( MaxSizeScale * StartSizeRange.X.GetMax() * Expand );

	return Value;

	unguard;
}

/*
	FBeamVertex
*/

class FBeamVertex
{
public:

	FVector	Position;
	FColor	Color;
	FLOAT	U,
			V;

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

// Renders all active particles
INT UBeamEmitter::RenderParticles(FDynamicActor* DynActor,FLevelSceneNode* SceneNode,TList<FDynamicLight*>* Lights,FRenderInterface* RI)
{
	guard(UBeamEmitter::RenderParticles);

	UParticleEmitter::RenderParticles( DynActor, SceneNode, Lights, RI );

	if ( (Texture == NULL) )
		return 0;

	FVector ViewLocation = SceneNode->ViewOrigin;

	// Transform coordinate system.
	if ( CoordinateSystem == PTCS_Relative )
		ViewLocation = ViewLocation.TransformPointBy ( GMath.UnitCoords / Owner->Rotation / Owner->Location );

	FParticleVertexStream<FBeamVertex>	BeamVertices;
	FRawIndexBuffer						BeamIndices;

	BeamVertices.Vertices.Add(MaxParticles * VerticesPerParticle);
	BeamIndices.Indices.Add(MaxParticles * IndicesPerParticle);

	FBeamVertex* Vertex = &BeamVertices.Vertices(0);
	_WORD* VBIndex = &BeamIndices.Indices(0);

	// Render all active particles.
	INT RenderedParticles = 0;
	for (INT Index=0; Index<ActiveParticles; Index++)
	{	
		FParticle* Particle = &Particles(Index);
		if (!(Particle->Flags & PTF_Active))
			continue;

		// Color.
		FColor Color = Particle->Color;

		// Time.
		FLOAT Time			= Particle->Time;
		FLOAT RelativeTime;
		if ( Particle->MaxLifetime )
			RelativeTime	= Clamp( Time / Particle->MaxLifetime, 0.f, 1.f );
		else
			RelativeTime	= 0.f;

		// Projection code.
		FVector Up;
		FVector Right;
		FVector Direction;

		// Dynamic noise.
		INT		ApplyDynamicNoise		= 1;
		FLOAT	Divisor					= DynamicTimeBetweenNoiseRange.GetRand();
		if( Divisor )
		{
			ApplyDynamicNoise			= (INT) (TimeSinceLastDynamicNoise / Divisor);
			TimeSinceLastDynamicNoise	= fmodf( TimeSinceLastDynamicNoise, Divisor );
		}

		Color = Color.RenderColor();

		// Rendering.
		for (INT r=0; r<SheetsUsed; r++)
		{
			FParticleBeamData* BeamPoint = &HFPoints(Index * HighFrequencyPoints);
			FVector OldLocation	= 2 * BeamPoint->Location - (BeamPoint+1)->Location; //!! sort of hack

			for (INT n=0; n<HighFrequencyPoints; n++)
			{
				if( ApplyDynamicNoise && n > DynamicHFNoisePointsRange.Min && n < DynamicHFNoisePointsRange.Max )
					BeamPoint->Location += ApplyDynamicNoise * DynamicHFNoiseRange.GetRand();

				Right = BeamPoint->Location - OldLocation;
				Up	  = Right ^  (OldLocation - ViewLocation);
				Up.Normalize();
				if ( r )
				{
					Right.Normalize();
					Up = Up.RotateAngleAxis( r * (0x8000/ SheetsUsed), Right );
				}
				Up *= Particle->Size.X;

				Vertex->Position	= BeamPoint->Location + Up;
				Vertex->Color		= Color;
				Vertex->U			= BeamPoint->t * BeamTextureUScale;
				Vertex->V			= 0;
				Vertex++;

				Vertex->Position	= BeamPoint->Location - Up;
				Vertex->Color		= Color;
				Vertex->U			= BeamPoint->t * BeamTextureUScale;
				Vertex->V			= BeamTextureVScale;
				Vertex++;

				OldLocation = BeamPoint->Location;
				BeamPoint++;
			}
			for (INT n=0; n<HighFrequencyPoints-1; n++)
			{
				*(VBIndex++) = 2*(n+0 + r*HighFrequencyPoints) + 0 + VerticesPerParticle * RenderedParticles;
				*(VBIndex++) = 2*(n+0 + r*HighFrequencyPoints) + 1 + VerticesPerParticle * RenderedParticles;
				*(VBIndex++) = 2*(n+1 + r*HighFrequencyPoints) + 0 + VerticesPerParticle * RenderedParticles;
				*(VBIndex++) = 2*(n+0 + r*HighFrequencyPoints) + 1 + VerticesPerParticle * RenderedParticles;
				*(VBIndex++) = 2*(n+1 + r*HighFrequencyPoints) + 1 + VerticesPerParticle * RenderedParticles;
				*(VBIndex++) = 2*(n+1 + r*HighFrequencyPoints) + 0 + VerticesPerParticle * RenderedParticles;
			}
		}

		// Moved from the beginning.
		RenderedParticles++;
	}

	// Rotate & translate beam effect if it is a relative coordinate system.
	if ( CoordinateSystem == PTCS_Relative )
	{
		FMatrix	LocalToWorld = FRotationMatrix(Owner->Rotation) * FTranslationMatrix(Owner->Location);

		RI->SetTransform(TT_LocalToWorld,LocalToWorld);
	}
	else
		RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);

	// Render them all.
	if ( RenderedParticles > 0 )
	{
		// Set texture and blending.
		Owner->ParticleMaterial->ParticleBlending			= DrawStyle;
		if( SceneNode->Viewport->Actor && SceneNode->Viewport->Actor->RendMap == REN_LightingOnly )
			Owner->ParticleMaterial->BitmapMaterial			= NULL;
		else
			Owner->ParticleMaterial->BitmapMaterial			= Texture;
		Owner->ParticleMaterial->BlendBetweenSubdivisions	= BlendBetweenSubdivisions && !UseRandomSubdivision;
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

		INT	BaseVertexIndex = RI->SetDynamicStream(VS_FixedFunction,&BeamVertices),
			BaseIndex = RI->SetDynamicIndexBuffer(&BeamIndices,BaseVertexIndex);

		for (INT r=0; r < SheetsUsed; r++)
		{
			RI->DrawPrimitive(
				PT_TriangleList,
				BaseIndex + RenderedParticles * IndicesPerParticle / SheetsUsed * r,
				RenderedParticles * PrimitivesPerParticle / SheetsUsed,
				0,
				RenderedParticles * VerticesPerParticle - 1
				);
		}
	}

	return RenderedParticles;

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

