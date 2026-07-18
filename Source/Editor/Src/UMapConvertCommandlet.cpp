/*=============================================================================
	UMapConvertCommandlet.cpp: Converts old maps to the new format.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Warren Marshall.
=============================================================================*/

#include "EditorPrivate.h"

void RecomputePoly( FPoly* Poly );

/*-----------------------------------------------------------------------------
	UMapConvertCommandlet.
-----------------------------------------------------------------------------*/

void OldApplyTransformToBrush( ABrush* InBrush )
{
	guard(OldApplyTransformToBrush);

	FModelCoords Coords;
	FLOAT Orientation = InBrush->OldBuildCoords( &Coords, NULL );
	InBrush->Modify();

	// recompute new locations for all vertices based on the current transformations
	UPolys* Polys = InBrush->Brush->Polys;
	Polys->Element.ModifyAllItems();
	for( INT j=0; j<Polys->Element.Num(); j++ )
		Polys->Element(j).Transform( Coords, FVector(0,0,0), FVector(0,0,0), Orientation );

	// reset the transformations
	InBrush->PrePivot = InBrush->PrePivot.TransformVectorBy( Coords.PointXform );

	InBrush->MainScale = GMath.UnitScale;
	InBrush->PostScale = GMath.UnitScale;
	InBrush->Rotation  = FRotator(0,0,0);

	InBrush->Brush->BuildBound();
	InBrush->PostEditChange();

	unguard;
}

class UMapConvertCommandlet : public UCommandlet
{
	DECLARE_CLASS(UMapConvertCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(StaticConstructor::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 1;
		ShowErrorCount  = 0;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(UMapConvertCommandlet::Main);

		// Create the editor class.
		GLazyLoad = 0;

		FString SrcFilename, DstFilename;

		// Make sure we got all params.
		if( !ParseToken(Parms,SrcFilename,0) )
			appErrorf(TEXT("Source filename not specified."));
		if( !ParseToken(Parms,DstFilename,0) )
			appErrorf(TEXT("Destination filename not specified."));

		GWarn->Logf( TEXT("\n\nPackage %s...\n\n"), *SrcFilename );

		UObject* Package = LoadPackage(NULL,*SrcFilename,LOAD_NoWarn);
		check(Package);
		ULevel* Level = FindObject<ULevel>( Package, TEXT("MyLevel") );
		check(Level);

		// Loop through all brushes and apply their transforms permanently.
		for( TObjectIterator<UObject> It; It; ++It )
		{
			if( Cast<ABrush>(*It) && It->IsIn(Package) )
			{
				ABrush* Brush = Cast<ABrush>(*It);

				if( Brush->Brush )
				{
					GWarn->Logf( TEXT("Converting : %s (%d polys)"), Brush->GetName(), Brush->Brush->Polys->Element.Num() );
					OldApplyTransformToBrush( Brush );
				}
			}
		}

		GWarn->Logf( TEXT("\n...Conversion complete..."));
		GWarn->Logf( TEXT("\nSaving : %s"), *DstFilename);

		// Save the updated map
		GWarn->Logf( TEXT("Saving map %s..."), *DstFilename );
		SavePackage( Package, Level, 0, *DstFilename, GWarn );

		GIsRequestingExit=1;
		return 0;

		unguard;
	}
};
IMPLEMENT_CLASS(UMapConvertCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

