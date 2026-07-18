/*=============================================================================
	UMeshEmitter.cpp: Unreal Mesh Emitter
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	UMeshEmitter.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UMeshEmitter);

void UMeshEmitter::PostEditChange()
{
	guard(UMeshEmitter::PostEditChange);
	Super::PostEditChange();
	CleanUp();
	Initialize( MaxParticles );
	unguard;
}

// Initializes the Emitter.
void UMeshEmitter::Initialize( INT InMaxParticles )
{
	guard(UMeshEmitter::Initialize);
	Super::Initialize( InMaxParticles );
	if ( StaticMesh )
	{
		MeshExtent.X = Max( StaticMesh->BoundingBox.Min.X, StaticMesh->BoundingBox.Max.X );
		MeshExtent.Y = Max( StaticMesh->BoundingBox.Min.Y, StaticMesh->BoundingBox.Max.Y );
		MeshExtent.Z = Max( StaticMesh->BoundingBox.Min.Z, StaticMesh->BoundingBox.Max.Z );
		MeshExtent -= StaticMesh->BoundingBox.GetCenter();
	}
	RealExtentMultiplier = ExtentMultiplier * MeshExtent;
	unguard;
}

// Update active particles.
INT UMeshEmitter::UpdateParticles( FLOAT DeltaTime )
{
	guard(UMeshEmitter::UpdateParticles);
	INT Value = Super::UpdateParticles( DeltaTime );
	FLOAT ExpandBy = MaxSizeScale;
	if( UniformSize )
		ExpandBy *= StartSizeRange.X.GetMax();
	else
		ExpandBy *= Max3(StartSizeRange.X.GetMax(), StartSizeRange.Y.GetMax(), StartSizeRange.Z.GetMax());
	// * 2.f in case mesh isn't centered around origin and doesn't use pivot either.
	if( BoundingBox.IsValid )
	{
		BoundingBox = BoundingBox.ExpandBy( 2.f * ExpandBy * Max3( MeshExtent.X, MeshExtent.Y, MeshExtent.Z ) );
		if( Owner )
			BoundingBox = BoundingBox.ExpandBy( Owner->PrePivot.Size() * ExpandBy );
	}
	return Value;
	unguard;
}

// Render particles.
//!! TODO: retrive lights for bounding box!
INT UMeshEmitter::RenderParticles(FDynamicActor* DynActor,FLevelSceneNode* SceneNode,TList<FDynamicLight*>* Lights,FRenderInterface* RI)
{
	guard(UMeshEmitter::RenderParticles);

	UParticleEmitter::RenderParticles( DynActor, SceneNode, Lights, RI );

	if ( StaticMesh == NULL )
		return 0;

	INT UseMeshLighting = UseMeshBlendMode && !SceneNode->Viewport->IsWire() && SceneNode->Viewport->IsLit();

	FSphere BoundingSphere = FSphere(&BoundingBox.Min,2);
	RI->EnableLighting(UseMeshLighting, 0, UseMeshLighting, NULL, SceneNode->Viewport->Actor && SceneNode->Viewport->Actor->RendMap == REN_LightingOnly, BoundingSphere );

	if( UseMeshLighting )
	{
		// Need dynamic lighting information.
		Owner->bUnlit = false;

		// Set ambient light.
		RI->SetAmbientLight( DynActor->AmbientColor );

		// Find lights affecting this particle system.
		INT	NumHardwareLights = 0;
		for(TList<FDynamicLight*>* LightList = Lights;LightList;LightList = LightList->Next)
			RI->SetLight(NumHardwareLights++,LightList->Element);
	}

	FVertexStream*	VertexStreams[9] = { &StaticMesh->VertexStream };
	INT				NumVertexStreams = 1;

	for(INT UVIndex = 0;UVIndex < StaticMesh->UVStreams.Num();UVIndex++)
		VertexStreams[NumVertexStreams++] = &StaticMesh->UVStreams(UVIndex);

	RI->SetVertexStreams(VS_FixedFunction,VertexStreams,NumVertexStreams);
	RI->SetIndexBuffer(&StaticMesh->IndexBuffer,0);

	INT RenderedParticles = 0;
	for (INT SectionIndex=0; SectionIndex<StaticMesh->Sections.Num(); SectionIndex++)
	{
		FStaticMeshSection&	Section		= StaticMesh->Sections(SectionIndex);
		UMaterial* Material = StaticMesh->GetSkin(Owner,SectionIndex);
		FParticle* Particle = &Particles(0);
		
		if( Section.NumPrimitives == 0 )
			continue;

		if( !UseMeshBlendMode )
		{
			Owner->ParticleMaterial->ParticleBlending			= DrawStyle;
			if( SceneNode->Viewport->Actor->RendMap == REN_LightingOnly )
				Owner->ParticleMaterial->BitmapMaterial			= NULL;
			else
				Owner->ParticleMaterial->BitmapMaterial			= Cast<UBitmapMaterial>(Material);
			Owner->ParticleMaterial->BlendBetweenSubdivisions	= 0;
			Owner->ParticleMaterial->RenderTwoSided				= RenderTwoSided;
			Owner->ParticleMaterial->UseTFactor					= 1;
			Owner->ParticleMaterial->AlphaTest					= AlphaTest;
			Owner->ParticleMaterial->AlphaRef					= AlphaRef;
			Owner->ParticleMaterial->AcceptsProjectors			= AcceptsProjectors;
			Owner->ParticleMaterial->ZTest						= ZTest;
			Owner->ParticleMaterial->ZWrite						= ZWrite;
			Owner->ParticleMaterial->Wireframe					= SceneNode->Viewport->IsWire();

			RI->SetMaterial(Owner->ParticleMaterial);
		}
		else
		{
			// Set texture and blending.
			DECLARE_STATIC_UOBJECT( UColorModifier, ColorModifier, {} );
			if(SceneNode->Viewport->IsWire())
			{
				DECLARE_STATIC_UOBJECT( UShader, WireframeShader, { WireframeShader->Wireframe = 1; });
				RI->SetMaterial( WireframeShader );
			}
			else
			{
				if( UseParticleColor )
				{
					ColorModifier->Material = Material;
					RI->SetMaterial( ColorModifier );
				}
				else
					RI->SetMaterial( Material );
			}
		}

		for (INT n=0; n<ActiveParticles; n++)
		{
			if (!(Particle->Flags & PTF_Active))
			{
				Particle++;
				continue;
			}
			RenderedParticles++;

			FMatrix	LocalToWorld;
			FVector Location = Particle->Location;
			FRotator Rotation;

			if( SpinParticles )
			{
				Rotation.Yaw	= (INT) (Particle->StartSpin.X + Particle->Time * Particle->SpinsPerSecond.X);
				Rotation.Pitch	= (INT) (Particle->StartSpin.Y + Particle->Time * Particle->SpinsPerSecond.Y);
				Rotation.Roll	= (INT) (Particle->StartSpin.Z + Particle->Time * Particle->SpinsPerSecond.Z);
			}

			if(CoordinateSystem == PTCS_Relative )
			{
				if (SpinParticles)
					LocalToWorld = 	FTranslationMatrix(-Owner->PrePivot) * 
									FScaleMatrix(Particle->Size) *
									FRotationMatrix(Rotation) * 
									FTranslationMatrix(Location) *
									FRotationMatrix(Owner->Rotation) * 
									FTranslationMatrix(Owner->Location);
				else
					LocalToWorld =	FTranslationMatrix(-Owner->PrePivot) * 
									FScaleMatrix(Particle->Size) *
									FTranslationMatrix(Location) * 
									FRotationMatrix(Owner->Rotation) *
									FTranslationMatrix(Owner->Location);
			}
			else
			{
				if (SpinParticles)
					LocalToWorld =	FTranslationMatrix(-Owner->PrePivot) * 
									FScaleMatrix(Particle->Size) *
									FRotationMatrix(Rotation) * 
									FTranslationMatrix(Location);
				else
					LocalToWorld =	FTranslationMatrix(-Owner->PrePivot) * 
									FScaleMatrix(Particle->Size) *
									FTranslationMatrix(Location);
			}
			
			RI->SetTransform(TT_LocalToWorld,LocalToWorld);
			RI->SetGlobalColor(Particle->Color);

#ifdef __PSX2_EE__
			extern void PSX2Render_RenderStaticMesh(UStaticMesh*, UStaticMeshInstance*, INT);
			PSX2Render_RenderStaticMesh(StaticMesh, NULL, i);
#else
			RI->DrawPrimitive(
				Section.IsStrip ? PT_TriangleStrip : PT_TriangleList,
				Section.FirstIndex,
				Section.NumPrimitives,
				Section.MinVertexIndex,
				Section.MaxVertexIndex
				);
#endif

			Particle++;
		}
	}

	return RenderedParticles;

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

