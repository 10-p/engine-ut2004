/*=============================================================================
	ABrush.h.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// Constructors.
ABrush() {}

// UObject interface.
void PostLoad();

void PostEditChange()
{
	Super::PostEditChange();

	if(Brush)
		Brush->BuildBound();
}

virtual UBOOL IsABrush() {return true;}

// OLD
// OLD
// These functions exist for the "ucc mapconvert" commandlet.  The engine/editor should NOT
// be using them otherwise.
FCoords OldToLocal() const
{
	guardSlow(ABrush::OldToLocal);
	return GMath.UnitCoords / -PrePivot / MainScale / Rotation / PostScale / Location;
	unguardSlow;
}
FCoords OldToWorld() const
{
	guardSlow(ABrush::OldToWorld);
	return GMath.UnitCoords * Location * PostScale * Rotation * MainScale * -PrePivot;
	unguardSlow;
}
FLOAT OldBuildCoords( FModelCoords* Coords, FModelCoords* Uncoords )
{
	guard(ABrush::OldBuildCoords);
	if( Coords )
	{
		Coords->PointXform    = (GMath.UnitCoords * PostScale * Rotation * MainScale);
		Coords->VectorXform   = (GMath.UnitCoords / MainScale / Rotation / PostScale).Transpose();
	}
	if( Uncoords )
	{
		Uncoords->PointXform  = (GMath.UnitCoords / MainScale / Rotation / PostScale);
		Uncoords->VectorXform = (GMath.UnitCoords * PostScale * Rotation * MainScale).Transpose();
	}
	return MainScale.Orientation() * PostScale.Orientation();
	unguard;
}
// OLD
// OLD

// AActor interface.	
UPrimitive* GetPrimitive();

FCoords ToLocal() const
{
	guardSlow(ABrush::ToLocal);
//	return GMath.UnitCoords / -PrePivot / Location; // OLD: broke collision volumes.
	return GMath.UnitCoords / -PrePivot / Rotation / Location; // NEW: fixes collision volumes, be wary of potential UnrealEd breaks due to dependencies on the above unsafe code.
	unguardSlow;
}
FCoords ToWorld() const
{
	guardSlow(ABrush::ToWorld);
//	return GMath.UnitCoords * Location * -PrePivot; // OLD: broke collision volumes.
	return GMath.UnitCoords * Location * Rotation * -PrePivot; // NEW: fixes collision volumes, be wary of potential UnrealEd breaks due to dependencies on the above unsafe code.
	unguardSlow;
}
FLOAT BuildCoords( FModelCoords* Coords, FModelCoords* Uncoords )
{
	guard(ABrush::BuildCoords);
	if( Coords )
	{
		Coords->PointXform    = GMath.UnitCoords;
		Coords->VectorXform   = GMath.UnitCoords.Transpose();
	}
	if( Uncoords )
	{
		Uncoords->PointXform  = GMath.UnitCoords;
		Uncoords->VectorXform = GMath.UnitCoords.Transpose();
	}
	return 0.0f;
	unguard;
}

// ABrush interface.
virtual void CopyPosRotScaleFrom( ABrush* Other )
{
	guard(ABrush::CopyPosRotScaleFrom);
	check(Brush);
	check(Other);
	check(Other->Brush);

	Location    = Other->Location;
	Rotation    = Other->Rotation;
	PrePivot	= Other->PrePivot;
	MainScale	= Other->MainScale;
	PostScale	= Other->PostScale;

	Brush->BuildBound();

	unguard;
}
virtual void InitPosRotScale();

void CheckForErrors();


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

