/*
UnErrorChecking.cpp
Actor Error checking functions
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Steven Polge
=============================================================================*/

#include "EnginePrivate.h"

void APhysicsVolume::CheckForErrors()
{
	guard(APhysicsVolume::CheckForErrors);

	if ( Gravity.Z == 0.f )
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("Gravity must not be zero!"));

	Super::CheckForErrors();
	unguard;
}

void ALevelInfo::CheckForErrors()
{
	guard(ALevelInfo::CheckForErrors);

	if ( GetLevel()->GetLevelInfo() != this )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("Duplicate level info!") ) );
	else
	{
		if ( !bPathsRebuilt )
			GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("Paths need to be rebuilt!") ) );
		//if ( !Screenshot )
		//	GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("No screenshot for this level!") ) );
		if ( Title.Len() == 0 )
			GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("No title for this level!") ) );
	}

	INT NumLights = 0;
	for ( INT i=0; i<GetLevel()->Actors.Num(); i++)
	{
		ALight *A = Cast<ALight>(GetLevel()->Actors(i)); 
		if ( A && !A->bDeleteMe )
			NumLights++;
	}
	if ( NumLights > 1500 )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("Level has an excessive number of lights - may affect performance.") ) );

	unguard;
}

void AActor::CheckForErrors()
{
	guard(AActor::CheckForErrors);

	if ( bObsolete )
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s is obsolete and must be removed!!!"), GetName() ) );
		return;
	}
	if ( GetClass()->GetDefaultActor()->bStatic && !bStatic )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s bStatic false, but is bStatic by default - map will fail in netplay"), GetName() ) );
	if ( GetClass()->GetDefaultActor()->bNoDelete && !bNoDelete )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s bNoDelete false, but is bNoDelete by default - map will fail in netplay"), GetName() ) );

	if ( AttachTag != NAME_None )
	{
		INT bSuccess = 0;

		// make sure actor with matching tag for all events
		for ( INT i=0; i<GetLevel()->Actors.Num(); i++)
		{
			AActor *A = GetLevel()->Actors(i); 
			if ( A && !A->bDeleteMe && ((A->Tag == AttachTag) || (A->GetFName() == AttachTag)) )
			{
				bSuccess = 1;
				break;
			}
		}
		if ( !bSuccess )
			GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("No Actor with Tag or Name corresponding to this Actor's AttachTag"));
	}

	// check if placed in same location as another actor of same class
	if ( GetClass()->ClassFlags & CLASS_Placeable )
		for ( INT i=0; i<GetLevel()->Actors.Num(); i++)
		{
			AActor *A = GetLevel()->Actors(i); 
			if ( A && (A != this) && ((A->Location - Location).SizeSquared() < 1.f) && (A->GetClass() == GetClass()) && (A->Rotation == Rotation)
				&& (A->DrawType == DrawType) && (A->DrawScale3D == DrawScale3D) && ((DrawType != DT_StaticMesh) || (StaticMesh == A->StaticMesh)) )
				GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s in same location as %s"), GetName(), A->GetName() ) );
		}

	if ( !bStatic && (Mass == 0.f) )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s mass must be greater than zero!"), GetName() ) );

	// Check missing resources
	if( DrawType == DT_StaticMesh && !StaticMesh )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s : NULL static mesh reference (DT_StaticMesh)"), GetName() ) );
	if( DrawType == DT_Sprite && !Texture )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s : NULL texture reference (DT_Sprite)"), GetName() ) );
	if( DrawType == DT_Mesh && !Mesh )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s : NULL mesh reference (DT_Mesh)"), GetName() ) );

	// Check for obsolete light effects.

	if(LightEffect == LE_WateryShimmer || LightEffect == LE_FireWaver || LightEffect == LE_TorchWaver || LightEffect == LE_CloudCast)
		GWarn->MapCheck_Add(MCTYPE_WARNING,this,*FString::Printf(TEXT("%s : Obsolete LightEffect"),GetName()));

#ifdef WITH_KARMA
	KCheckActor(this);
#endif

	unguard;
}

void ANote::CheckForErrors()
{
	guard(ANote::CheckForErrors);

	GWarn->MapCheck_Add( MCTYPE_NOTE, this, *Text );

	AActor::CheckForErrors();

	unguard;
}

void ABrush::CheckForErrors()
{
	guard(ABrush::CheckForErrors);

	// NOTE : don't report NULL texture references on the builder brueh - it doesn't matter there
	// NOTE : don't check volume brushes since those are forced to have the NULL texture on all polys
	if( Brush
			&& this != GetLevel()->Brush()
			&& !IsVolumeBrush() )
	{
		// Check for missing textures
		for( INT x = 0 ; x < Brush->Polys->Element.Num() ; ++x )
		{
			FPoly* Poly = &(Brush->Polys->Element(x));
			if( !Poly->Material )
			{
				GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s : Brush has NULL material reference(s)"), GetName() ) );
				break;
			}
		}
	}

	AActor::CheckForErrors();

	unguard;
}

void ATerrainInfo::CheckForErrors()
{
	guard(ATerrainInfo::CheckForErrors);

	UBOOL HadAlphaTexture = 0;
	for(INT LayerIndex = 0;LayerIndex < ARRAY_COUNT(Layers);LayerIndex++)
	{
		if( HadAlphaTexture && Cast<UTexture>(Layers[LayerIndex].Texture) && !Cast<UTexture>(Layers[LayerIndex].Texture)->bAlphaTexture )
			GWarn->MapCheck_Add(
				MCTYPE_WARNING,
				this,
				*FString::Printf(
					TEXT("Terrain layer %d (%s) is not an alpha texture, but a layer below it is.  Alpha textures must be last in the layer list."),
					LayerIndex,
					Layers[LayerIndex].Texture->GetPathName()
					)
				);

		
		if(Layers[LayerIndex].Texture)
		{
			UTexture* Tex = Cast<UTexture>(Layers[LayerIndex].Texture);
			if( Tex && Tex->bAlphaTexture )
				HadAlphaTexture = 1;
		}

		if(Layers[LayerIndex].AlphaMap && Layers[LayerIndex].AlphaMap->Mips.Num() > 1)
			GWarn->MapCheck_Add(
				MCTYPE_WARNING,
				this,
				*FString::Printf(
					TEXT("Terrain alpha map %s has more than one mip level.  This will cause visual artifacts."),
					Layers[LayerIndex].AlphaMap->GetPathName()
					)
				);
	}

	unguard;
}

void AxPickUpBase::CheckForErrors()
{
	guard(AxPickupBase::CheckForErrors);

	Super::CheckForErrors();

	FCheckResult Hit(1.f);
	GetLevel()->SingleLineCheck( Hit, this, Location - FVector(0.f,0.f,8.f), Location, TRACE_World|TRACE_StopAtFirstHit );
	if ( Hit.Time == 1.f )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("%s : xPickupBase is floating"), GetName() ) );
	unguard;
}

void APawn::CheckForErrors()
{
	guard(APawn::CheckForErrors);

	AActor::CheckForErrors();

	if ( AIScriptTag != NAME_None )
	{
		INT bSuccess = 0;

		// make sure actor with matching tag for all events
		for ( INT i=0; i<GetLevel()->Actors.Num(); i++)
		{
			AAIScript *A = Cast<AAIScript>(GetLevel()->Actors(i)); 
			if ( A && !A->bDeleteMe && (A->Tag == AIScriptTag) )
			{
				bSuccess = 1;
				break;
			}
		}
		if ( !bSuccess )
			GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("No AIScript with Tag corresponding to this Pawn's AIScriptTag"));
	}
	unguard;
}

void ANavigationPoint::CheckForErrors()
{
	guard(ANavigationPoint::CheckForErrors);

	AActor::CheckForErrors();

    if( !GetLevel()->GetLevelInfo()->bHasPathNodes )
        return;

	if ( !bDestinationOnly && (PathList.Num() == 0) )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf(TEXT("No paths from %s"), GetName() ) );

	if ( ExtraCost < 0 )
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("Extra Cost cannot be less than zero!") );

	if ( Location.Z > GetLevel()->GetLevelInfo()->StallZ )
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, TEXT("NavigationPoint above level's StallZ!") );

	unguard;
}

void APickup::CheckForErrors()
{
	guard(APickup::CheckForErrors);

	AActor::CheckForErrors();

    if( !myMarker )
	{
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("No inventory spot for this pickup!") );
        return;
	}

	FCheckResult Hit(1.f);
	GetLevel()->SingleLineCheck( Hit, this, myMarker->Location, Location, TRACE_World|TRACE_StopAtFirstHit );

	if ( Hit.Actor )
		GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("Pickup embedded in collision geometry!") );
	else
	{
		GetLevel()->SingleLineCheck( Hit, this, Location, myMarker->Location, TRACE_World|TRACE_StopAtFirstHit, FVector(2.f,2.f,2.f) );
		if ( Hit.Actor )
			GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("Pickup embedded in collision geometry!") );
	}
	unguard;
}

