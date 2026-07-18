/*=============================================================================
	UnVolume.cpp: Code related to volumes and their subclasses
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	AVolume.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(ADecorationList);
IMPLEMENT_CLASS(ADecoVolumeObject);

void AVolume::PostBeginPlay()
{
	guard(AVolume::PostBeginPlay);
	
	Super::PostBeginPlay();

	// If this volume has a decoration list, spawn them.
	if( DecoList )
	{
		FBox bbox = Brush->GetRenderBoundingBox( NULL );
		bbox.Min = LocalToWorld().TransformFVector( bbox.Min );
		bbox.Max = LocalToWorld().TransformFVector( bbox.Max );
		
		FVector Extent = bbox.GetExtent();

		for( INT x = 0 ; x < DecoList->Decorations.Num() ; ++x )
		{
			FDecorationType* DecoType = &DecoList->Decorations(x);

			INT Num = (INT)DecoType->Count.GetRand();
			for( INT y = 0 ; y < Num ; ++y )
			{
				FVector NewLoc = FVector(	FRange( -Extent.X, Extent.X ).GetRand(),
									FRange( -Extent.Y, Extent.Y ).GetRand(),
									FRange( -Extent.Z, Extent.Z ).GetRand() );
				NewLoc = LocalToWorld().TransformFVector( NewLoc );

				ADecoVolumeObject* dvo = Cast<ADecoVolumeObject>( 
					GetLevel()->SpawnActor
					(
						ADecoVolumeObject::StaticClass(),
						NAME_None,
						NewLoc,
						FRotator(0,0,0),
						NULL,
						0,
						0
					) 
				);

				if( dvo )
				{
					dvo->StaticMesh = DecoType->StaticMesh;

					// If we can't find a good spot for this decoration, delete it.
					if( !GetLevel()->ToFloor( dvo, DecoType->bAlign, this ) )
						GetLevel()->DestroyActor( dvo );
					else
					{
						if( DecoType->bRandomPitch ) dvo->Rotation.Pitch += appRand()%65536;
						if( DecoType->bRandomYaw ) dvo->Rotation.Yaw += appRand()%65536;
						if( DecoType->bRandomRoll ) dvo->Rotation.Roll += appRand()%65536;

						//!!vogel: what about DrawScale3D?
						if( !DecoType->DrawScale.IsZero() )
							dvo->SetDrawScale( DecoType->DrawScale.GetRand() );
					}
				}
			}
		}
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

