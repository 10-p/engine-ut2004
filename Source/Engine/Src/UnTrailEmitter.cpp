/*=============================================================================
	UnTrailEmitter.cpp: Unreal Trail Emitter
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
		* Updated by Laurent Delayen
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	UTrailEmitter.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UTrailEmitter);


inline FColor UTrailEmitter::SetInitialPointColor(FColor ParticleColor)
{
	if ( TrailShadeType == PTTST_RandomStatic )
	{
		FLOAT PointOpacity = appFrand();

		if ( DrawStyle == PTDS_AlphaBlend ||
			DrawStyle == PTDS_Modulated ||
			DrawStyle == PTDS_AlphaModulate_MightNotFogCorrectly)
		{
			ParticleColor.A = ParticleColor.A * PointOpacity;
		}
		else
		{
			ParticleColor.R = ParticleColor.R * PointOpacity;
			ParticleColor.G = ParticleColor.G * PointOpacity;
			ParticleColor.B = ParticleColor.B * PointOpacity;
		}
	}

	return ParticleColor;
}


void UTrailEmitter::PostEditChange()
{
	Super::PostEditChange();
	MaxPointsPerTrail	= Max(3, MaxPointsPerTrail);
	PointLifeTime		= Max(0.f, PointLifeTime);
	DistanceThreshold	= Max(2.f, DistanceThreshold);

	CleanUp();
	Initialize( MaxParticles );
}

// Initializes the Emitter.
void UTrailEmitter::Initialize( INT InMaxParticles )
{
	guard(UTrailEmitter::Initialize);
	Super::Initialize( InMaxParticles );
	TrailData.Add( InMaxParticles * MaxPointsPerTrail );
	TrailInfo.Add( InMaxParticles );
	unguard;
}

// CleanUp.
void UTrailEmitter::CleanUp()
{
	guard(UTrailEmitter::CleanUp);
	Super::CleanUp();
	TrailData.Empty();
	TrailInfo.Empty();
	unguard;
}

// Spawn particle.
void UTrailEmitter::SpawnParticle( INT Index, FLOAT SpawnTime, INT Flags, INT SpawnFlags, const FVector& LocalLocationOffset )
{
	guard(UTrailEmitter::SpawnParticle);

	Super::SpawnParticle( Index, SpawnTime, Flags, SpawnFlags, LocalLocationOffset );

	FVector	PointLocation	= Particles(Index).Location;

	// Laurent -- trail's location...
	if ( Owner && TrailLocation == PTTL_FollowEmitter)
	{
			// Add emitter's velocity to particle
			TrailInfo(Index).LastEmitterLocation = Owner->Location;
	}
	
	TrailInfo(Index).TrailIndex			= 1;
	TrailInfo(Index).NumPoints			= 2;
	TrailInfo(Index).LastLocation		= PointLocation;
	
	TrailData(Index * MaxPointsPerTrail + 0).Location	= PointLocation;
	TrailData(Index * MaxPointsPerTrail + 0).Color		= SetInitialPointColor(Particles(Index).Color);
	TrailData(Index * MaxPointsPerTrail + 0).Size		= Particles(Index).Size.X;
	TrailData(Index * MaxPointsPerTrail + 0).Time		= Owner->Level->TimeSeconds + PointLifeTime; 

	TrailData(Index * MaxPointsPerTrail + 1).Location	= PointLocation;
	TrailData(Index * MaxPointsPerTrail + 1).Color		= SetInitialPointColor(Particles(Index).Color);
	TrailData(Index * MaxPointsPerTrail + 1).Size		= Particles(Index).Size.X;
	TrailData(Index * MaxPointsPerTrail + 1).Time		= Owner->Level->TimeSeconds + PointLifeTime; 
	unguard;
}

// Update particles.
INT UTrailEmitter::UpdateParticles( FLOAT DeltaTime )
{
	guard(UTrailEmitter::UpdateParticles);
	INT Value	= Super::UpdateParticles( DeltaTime );
	if( BoundingBox.IsValid )
		BoundingBox = BoundingBox.ExpandBy( MaxSizeScale );
	return Value;
	unguard;
}

void UTrailEmitter::UpdateParticle( FLOAT DeltaTime, INT Index )
{
	FParticle& Particle		= Particles(Index);
	INT TrailIndex			= TrailInfo(Index).TrailIndex;
	INT NumPoints			= TrailInfo(Index).NumPoints;
	INT Offset				= MaxPointsPerTrail * Index;
	FVector LocationOffset	= FVector(0,0,0);
	FVector	PointLocation	= Particles(Index).Location;

	// Laurent -- trail's location...
	if ( Owner && TrailLocation == PTTL_FollowEmitter )
	{
		// Add emitter's velocity to particle
		Particles(Index).Location				+= Owner->Location - TrailInfo(Index).LastEmitterLocation;
		TrailInfo(Index).LastEmitterLocation	= Owner->Location;
		PointLocation							= Particles(Index).Location;
	}


	// Laurent -- Kill Hack
	if ( KillPending && (NumPoints==2) )
	{
		Particle.Time = Particle.MaxLifetime + 1.0; // Kill next tick
	}

	// Laurent -- Remove points if pointlifetime is up
	if ( (PointLifeTime > 0.f) && (NumPoints > 2) )
	{
		INT LastPointIndex;
		INT bKeepChecking=1;
		while ( bKeepChecking && (NumPoints > 2) ) // Laurent -- keep killing until allowed to live...
		{
			bKeepChecking	= 0;
			LastPointIndex	= TrailIndex - (NumPoints-1);
			if ( LastPointIndex < 0 )	LastPointIndex += MaxPointsPerTrail;

			//debugf(TEXT("Removing 1 point - TI:%i NP:%i LPI:%i"), TrailIndex, NumPoints, LastPointIndex);

			if ( TrailData(Offset + LastPointIndex).Time < Owner->Level->TimeSeconds )
			{
				TrailInfo(Index).NumPoints	-= 1;
				NumPoints					-=1;	
				bKeepChecking				= 1;
			}

		}
	}

	// Generate new point if particle moved further than distance threshold.
	// Laurent -- Need to interpolate here, or we can't control trail size (when velocity is too high)
	FLOAT TravelledDist		= FDist(PointLocation, TrailInfo(Index).LastLocation);
	FLOAT DistThreshold		= DistanceThreshold;
	FVector LastLocation	= TrailInfo(Index).LastLocation;

	if ( !KillPending && TravelledDist >= DistThreshold )
	{
		// Reposition well last point
		TrailData(Offset + TrailIndex).Location	= Lerp(	LastLocation, PointLocation, DistThreshold / TravelledDist );

		// Update Last Location (take in account last interpolated point and not current attached point)
		TrailInfo(Index).LastLocation = TrailData(Offset + TrailIndex).Location;

		DistThreshold += DistanceThreshold;

		// Then create a new one...
		if ( ++TrailIndex == MaxPointsPerTrail )
			TrailIndex = 0;
		NumPoints = Min( NumPoints+1, MaxPointsPerTrail );

		// Laurent -- Linear interpolation of location, to control trail's size
		TrailData(Offset + TrailIndex).Location	= Lerp(	LastLocation, PointLocation, DistThreshold / TravelledDist );
		TrailData(Offset + TrailIndex).Color	= SetInitialPointColor(Particle.Color);
		TrailData(Offset + TrailIndex).Size		= Particle.Size.X;
		TrailData(Offset + TrailIndex).Time		= Owner->Level->TimeSeconds + PointLifeTime;

		// Do we need to add more?
		while ( TravelledDist >= DistThreshold )
		{
			// Wrap around if already at points limit.
			if( ++TrailIndex == MaxPointsPerTrail )
				TrailIndex = 0;

			NumPoints = Min( NumPoints+1, MaxPointsPerTrail );

			// Laurent -- Linear interpolation of location, to control trail's size
			TrailData(Offset + TrailIndex).Location	= Lerp( LastLocation, PointLocation, DistThreshold / TravelledDist );
			TrailData(Offset + TrailIndex).Color	= SetInitialPointColor(Particle.Color);
			TrailData(Offset + TrailIndex).Size		= Particle.Size.X;
			TrailData(Offset + TrailIndex).Time		= Owner->Level->TimeSeconds + PointLifeTime;

			//TrailInfo(Index).LastLocation = PointLocation;
			// Update Last Location (take in account last interpolated point and not current attached point)
			TrailInfo(Index).LastLocation = TrailData(Offset + TrailIndex).Location;

			DistThreshold += DistanceThreshold;
		}	
	}
	
	if ( !KillPending ) // Laurent -- if kill pending, don't update trail, just let it die...
	{
		TrailData(Offset + TrailIndex).Location	= PointLocation;
		TrailData(Offset + TrailIndex).Color	= SetInitialPointColor(Particle.Color);
		TrailData(Offset + TrailIndex).Size		= Particle.Size.X;
		TrailData(Offset + TrailIndex).Time		= Owner->Level->TimeSeconds + PointLifeTime; 
	}

	TrailInfo(Index).NumPoints	= NumPoints;
	TrailInfo(Index).TrailIndex = TrailIndex;

	// Expand bounding box.
	INT TempIndex = TrailIndex;
	for( INT i=0; i<NumPoints; i++ )
	{
		BoundingBox += TrailData( Offset + TempIndex ).Location;
		if( --TempIndex < 0 )
			TempIndex += MaxPointsPerTrail;
	}

	// Laurent -- Shade Trail
	if ( (TrailShadeType > PTTST_RandomStatic && DrawStyle != PTDS_Regular) )
	{
		FLOAT	PointOpacity	= 1.f;

		for (int i=NumPoints; i>0; i--)
		{

			if ( TrailShadeType == PTTST_RandomDynamic )
				PointOpacity = appFrand();
			else if ( TrailShadeType == PTTST_Linear )
				PointOpacity = 1.f - FLOAT(NumPoints-i)/FLOAT(NumPoints); 
			else if ( TrailShadeType == PTTST_PointLife && PointLifeTime>0.f )
				PointOpacity = (TrailData(Offset + TrailIndex).Time - Owner->Level->TimeSeconds)/PointLifeTime;

			//debugf(TEXT("Trail Shade Type - TST:%i PN:%i NP:%i PO:%f"), TrailShadeType, i, NumPoints, PointOpacity);
			//debugf(TEXT("Trail Shade Type - PN:%i NP:%i PO:%f PT:%f LT:%f"), i, NumPoints, PointOpacity, TrailData(Offset + TrailIndex).Time, Owner->Level->TimeSeconds);

			if ( DrawStyle == PTDS_AlphaBlend ||
				DrawStyle == PTDS_Modulated ||
				DrawStyle == PTDS_AlphaModulate_MightNotFogCorrectly)
			{
				TrailData(Offset + TrailIndex).Color.A = Particle.Color.A * PointOpacity;
			}
			else
			{
				TrailData(Offset + TrailIndex).Color.R = Particle.Color.R * PointOpacity;
				TrailData(Offset + TrailIndex).Color.G = Particle.Color.G * PointOpacity;
				TrailData(Offset + TrailIndex).Color.B = Particle.Color.B * PointOpacity;
			}

			TrailIndex--;
			if ( TrailIndex < 0 )	TrailIndex += MaxPointsPerTrail;
		}

		//TrailIndex = TrailInfo(Index).TrailIndex;
	}

}


#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,1)
#endif

struct FTrailVertex
{
	FVector	Position GCC_PACK(1);
	FColor	Diffuse GCC_PACK(1);
	FLOAT	U GCC_PACK(1);
	FLOAT	V GCC_PACK(1);

	static INT GetComponents(FVertexComponent* OutComponents)
	{
		OutComponents[0].Type		= CT_Float3;
		OutComponents[0].Function	= FVF_Position;
		OutComponents[1].Type		= CT_Color;
		OutComponents[1].Function	= FVF_Diffuse;
		OutComponents[2].Type		= CT_Float2;
		OutComponents[2].Function	= FVF_TexCoord0;

		return 3;
	}
};

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif


class FTrailVertexStream : public FParticleVertexStream<FTrailVertex>
{
public:

	UTrailEmitter*				Emitter;
	INT							NumVerts;
	FSceneNode*					SceneNode;

	// Constructor.
	FTrailVertexStream( UTrailEmitter* InEmitter,INT InNumVerts, FSceneNode* InSceneNode )
	{
		Emitter			= InEmitter;
		NumVerts		= InNumVerts;
		SceneNode		= InSceneNode;
	}

	virtual INT GetSize()
	{
		return NumVerts * sizeof(FTrailVertex);
	}

	// Generates the spark vertices procedurally.
	virtual void GetStreamData(void* Dest)
	{
		FVector ViewLocation = SceneNode->ViewOrigin;

		FVector ProjBase     = SceneNode->Deproject(FPlane(0,0,0,1));	
		FVector ProjUp		 = SceneNode->Deproject(FPlane(0,-1000,0,1)) - ProjBase;
		FVector ProjRight	 = SceneNode->Deproject(FPlane(1000,0,0,1)) - ProjBase;
		FVector ProjFront	 = ProjRight ^ ProjUp;

		// Better safe than sorry.
		ProjUp.Normalize();
		ProjRight.Normalize();
		ProjFront.Normalize();

		FTrailVertex*	Vertex = (FTrailVertex*) Dest;
		for(INT ParticleIndex = 0;ParticleIndex < Emitter->ActiveParticles;ParticleIndex++)
		{
			FParticle*	Particle = &Emitter->Particles(ParticleIndex);

			if( !(Particle->Flags & PTF_Active) )
				continue;

			INT NumPoints = Emitter->TrailInfo(ParticleIndex).NumPoints;
			if( NumPoints <= 1 )
				continue;

			// Do a second pass for crossed sheets.
			for( INT PassCount=0; PassCount<(Emitter->UseCrossedSheets ? 2 : 1); PassCount++ )
			{	
				INT TrailIndex = Emitter->TrailInfo(ParticleIndex).TrailIndex;

				// Setup last, current and next location and wrap around index if necessary.
				FVector LastLocation	= FVector(0,0,0);
				FVector Location		= Emitter->TrailData(ParticleIndex * Emitter->MaxPointsPerTrail + TrailIndex).Location;
				
				// Setup size and color.
				FLOAT	Size			= Emitter->TrailData(ParticleIndex * Emitter->MaxPointsPerTrail + TrailIndex).Size;
				FColor	Color			= Emitter->TrailData(ParticleIndex * Emitter->MaxPointsPerTrail + TrailIndex).Color;

				// Setup next location and wrap around if necessary.
				if( --TrailIndex < 0 )
					TrailIndex += Emitter->MaxPointsPerTrail;			
				FVector NextLocation	= Emitter->TrailData(ParticleIndex * Emitter->MaxPointsPerTrail + TrailIndex).Location;

				Color = Color.RenderColor();

				// Generate vertices.
				for( INT i=0; i<NumPoints; i++ )
				{
					FVector	Tangent;

					// No LastLocation.
					if( i == 0 )
					{
						Tangent			= (NextLocation - Location).SafeNormal();
					}
					// No NextLocation.
					else
					if( i == NumPoints-1 )
					{
						Tangent			= (Location - LastLocation).SafeNormal();
					}
					// Both Last and NextLocation are available.
					else
					{
						FVector LastDir	= Location - LastLocation;
						FVector NextDir	= NextLocation - Location;		
						Tangent			= ((LastDir + NextDir) / 2.f).SafeNormal();
					}

					// Twisted system.
					FVector ProjFront	= (Location - ViewLocation).SafeNormal();
					FVector Right		= (FVector(0,0,1) ^ Tangent).SafeNormal();
					FLOAT Angle			= Emitter->UseCrossedSheets ? 16384 : Emitter->MaxTrailTwistAngle * (Right | ProjFront);
					FVector Up			= !Emitter->UseCrossedSheets || PassCount ? Right.RotateAngleAxis( Angle, Tangent ).SafeNormal() : Right;
					Up					*= Size;
					
					Vertex->Position	= Location + Up;
					Vertex->Diffuse		= Color;
					Vertex->U			= i / (FLOAT) NumPoints;
					Vertex->V			= 1.f;
					Vertex++;

					Vertex->Position	= Location - Up;
					Vertex->Diffuse		= Color;
					Vertex->U			= i / (FLOAT) NumPoints;
					Vertex->V			= 0.f;
					Vertex++;

					// Update last and current location.
					LastLocation		= Location;
					Location			= NextLocation;
					
					// Update size and color.
					Size				= Emitter->TrailData(ParticleIndex * Emitter->MaxPointsPerTrail + TrailIndex).Size;
					Color				= Emitter->TrailData(ParticleIndex * Emitter->MaxPointsPerTrail + TrailIndex).Color;

					Color = Color.RenderColor();

					// Update next location and wrap around if necessary.
					if( --TrailIndex < 0 )
						TrailIndex += Emitter->MaxPointsPerTrail;
					NextLocation		=  Emitter->TrailData(ParticleIndex * Emitter->MaxPointsPerTrail + TrailIndex).Location;

				}
			}
		}
	}
};

// FTrailIndexBuffer
class FTrailIndexBuffer : public FIndexBuffer
{
public:

	UTrailEmitter*	Emitter;
	INT				NumIndices;
	QWORD			CacheId;

	// Constructor.
	FTrailIndexBuffer( UTrailEmitter* InEmitter, INT InNumIndices )
	{
		Emitter		= InEmitter;
		NumIndices	= InNumIndices;
		CacheId		= MakeCacheID(CID_RenderIndices);
	}

	virtual QWORD GetCacheId()
	{
		return CacheId;
	}

	virtual INT GetRevision()
	{
		return 1;
	}

	virtual INT GetSize()
	{
		return NumIndices * sizeof(_WORD);
	}

	virtual INT GetIndexSize()
	{
		return sizeof(_WORD);
	}

	virtual void GetContents(void* Data)
	{
		_WORD* WordData = (_WORD*)Data;
		INT VertexIndex = 0;

		for(INT ParticleIndex = 0;ParticleIndex < Emitter->ActiveParticles;ParticleIndex++)
		{
			FParticle*	Particle = &Emitter->Particles(ParticleIndex);

			if( !(Particle->Flags & PTF_Active) )
				continue;

			if( Emitter->TrailInfo(ParticleIndex).NumPoints <= 1 )
				continue;

			// Do a second pass for crossed sheets.
			for( INT PassCount=0; PassCount<(Emitter->UseCrossedSheets ? 2 : 1); PassCount++ )
			{
				for( INT i=0; i<Emitter->TrailInfo(ParticleIndex).NumPoints-1; i++ )
				{
					*(WordData++) = 2*VertexIndex+0;
					*(WordData++) = 2*VertexIndex+1;
					*(WordData++) = 2*VertexIndex+2;
					*(WordData++) = 2*VertexIndex+2;
					*(WordData++) = 2*VertexIndex+1;
					*(WordData++) = 2*VertexIndex+3;
					VertexIndex++;
				}
				VertexIndex++;
			}
		}
	}
};

void UTrailEmitter::ResetTrail()
{
	guard(UTrailEmitter::ResetTrail);

	for(INT ParticleIndex = 0;ParticleIndex < ActiveParticles;ParticleIndex++)
	{
		FParticle*	Particle = &Particles(ParticleIndex);
		Particle->Flags &= ~PTF_Active;
	}

	ActiveParticles = 0;
	AllParticlesDead = false;
	Inactive = false;

	unguard;
}

void UTrailEmitter::execResetTrail( FFrame& Stack, RESULT_DECL )
{
	guard(UTrailEmitter::execResetTrail);

	P_FINISH;

	ResetTrail();

	unguard;
}

// Renders all active particles
INT UTrailEmitter::RenderParticles(FDynamicActor* DynActor,FLevelSceneNode* SceneNode,TList<FDynamicLight*>* Lights,FRenderInterface* RI)
{
	guard(UTrailEmitter::RenderParticles);

	UParticleEmitter::RenderParticles( DynActor, SceneNode, Lights, RI );

	if( Texture == NULL )
		return 0;

	INT RenderedParticles = 0;

	// Render the particles.
	if( ActiveParticles > 0 )
	{
		// Calculate the number of active particles.
		INT NumVerts	= 0,
			NumTris		= 0,
			NumIndices	= 0;
		for( INT ParticleIndex = 0;ParticleIndex < ActiveParticles;ParticleIndex++ )
		{
			FParticle* Particle = &Particles(ParticleIndex);

			if(Particle->Flags & PTF_Active)
			{
				INT NumPoints = TrailInfo(ParticleIndex).NumPoints;
				if( NumPoints > 1 )
				{
					RenderedParticles++;
					NumTris		+= NumPoints * 2 - 2;
					NumVerts	+= NumPoints * 2;
					NumIndices	+= NumPoints * 6 - 6;
				}
			}
		}

		// Bail out if nothing to render.
		if( !RenderedParticles || !NumVerts || !NumIndices )
			return 0;

		//!!vogel: TODO 
		if( UseCrossedSheets )
		{
			NumTris		*= 2;
			NumVerts	*= 2;
			NumIndices	*= 2;
		}

		// Create the dynamic vertex stream and index buffer.
		FTrailVertexStream TrailVertices(this,NumVerts,SceneNode);
		FTrailIndexBuffer  TrailIndices(this,NumIndices);

		// Rotate & translate if needed.
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

		// Set the particle vertex stream and index buffer.
		// The particle vertices aren't actually generated until now.
		INT	BaseVertexIndex = RI->SetDynamicStream(VS_FixedFunction,&TrailVertices),
			BaseIndex		= RI->SetDynamicIndexBuffer(&TrailIndices,BaseVertexIndex);

		RI->DrawPrimitive(
			PT_TriangleList, 
			BaseIndex,
			NumTris,
			0,
			NumVerts - 1
		);
	}

	return RenderedParticles;

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

