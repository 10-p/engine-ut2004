/*=============================================================================
	UnMatineeTools.cpp: Tools for the Matinee system
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "EditorPrivate.h"

#define IP_MARKER_SZ	6

/*------------------------------------------------------------------------------
	FCameraPath.

	Represents one matinee path in the level.
------------------------------------------------------------------------------*/

FCameraPath::FCameraPath()
{
}

FCameraPath::~FCameraPath()
{
}

void FCameraPath::SetPathName( FString InPathName )
{
	check( InPathName.Len() );
	PathName = InPathName;

	RebuildPointList( GEditor->Level );
}

// Find a good sorted spot in the list to insert this point, based on it's position number.
void FCameraPath::InsertPoint( AInterpolationPoint* InPoint )
{
	guard(FCameraPath::InsertPoint);

	// See if we can insert the point somewhere in the existing list.
	for( INT x = 0 ; x < PointList.Num() ; ++x )
	{
		if( PointList(x)->Position > InPoint->Position )
		{
			PointList.Insert( x );
			PointList(x) = InPoint;
			return;
		}
	}

	// We couldn't insert the point anywhere, so add it to the end.
	INT idx = PointList.Add();
	PointList(idx) = InPoint;

	unguard;
}

// A point is being deleted from this path.  This is a notification from the editor.  Renumbers existing points as necessary.
void FCameraPath::DeletePoint( AInterpolationPoint* InIP )
{
	// If the next IP point in the list is numbered in sequence with this one, renumbering needs to take place.
	if( InIP->Position == InIP->Next->Position-1 )
	{
		// Find this IP
		for( INT x = 0 ; x < PointList.Num() ; ++x )
			if( PointList(x) == InIP )
				break;

		// Renumber all of the points ahead of it
		for( ; x < PointList.Num() ; ++x )
			PointList(x)->Position--;
	}
}

void FCameraPath::RebuildPointList( ULevel* InLevel )
{
	guard(FCameraPath::RebuildPointList);

	PointList.Empty();

	for( INT x = 0 ; x < InLevel->Actors.Num() ; ++x )
	{
		AActor* Actor = InLevel->Actors(x);

		if( Actor && Actor->IsA(AInterpolationPoint::StaticClass()) )
			if( GetPathName() == *Actor->Tag )
				InsertPoint( (AInterpolationPoint*)Actor );
	}

	GMatineeTools->LinkPoints( &PointList );

	//!! This loop is fairly hackish.  It's only here so the interpolation points will have
	//   proper distances inside of them right off the bat.
	TArray<FPosition> Positions;
	for( x = 0 ; x < PointList.Num() ; ++x )
	{
		Positions.Empty();
		AInterpolationPoint* IP = PointList(x);
		GMatineeTools->GetSamplePoints( IP, .5f, &Positions );
	}

	unguard;
}

// Draws this camera path using the given frame
void FCameraPath::DrawPointList( UViewport* Viewport )
{
	guard(FCameraPath::DrawPointList);

	FLOAT Scale = LastScale = Viewport->SizeX / (FLOAT)GetPathLengthInPixels();

	//
	// Draw the IPs
	//

	FLOAT XPos = 0;
	FCanvasUtil		CanvasUtil(&Viewport->RenderTarget,Viewport->RI);
	for( INT x = 0 ; x < PointList.Num() ; ++x )
	{
		AInterpolationPoint* IP = PointList(x);

		//
		// IP POINT
		//

		UTexture* Texture = IP->bSelected ? GMatineeTools->IntPointSel : GMatineeTools->IntPoint;

		PUSH_HIT(Viewport,HActor(PointList(x)));
		Viewport->Canvas->DrawIcon( Texture,
			appRound(XPos), 0,
			Texture->UClamp, Texture->VClamp,
			1.0, FPlane(1,1,1,1), FPlane(0,0,0,0), 0 );
		POP_HIT(Viewport);

		// Update the rendering position
		INT Start = appRound(XPos) + IP_MARKER_SZ;
		XPos += ((IP->Next->PathDist / 100.f) + IP_MARKER_SZ) * Scale;
		INT End = appRound(XPos);

		//
		// CONNECTING LINE
		//

		UBOOL bGapInNumbering = IP->Next ? (IP->Next->Position != IP->Position + 1) : 0;

		FLOAT Step = (End - Start) / 10.f;

		End++;
		Start++;

		if( (End - Start) > 0 )
			for( INT x = 0 ; x < 10 ; ++x )
			{
				PUSH_HIT(Viewport,HMatLineSegmentPct(IP, (x+1) * 10 ));
				FColor Color;
				if( IP->Prev->bInstantNextPath )
					Color = bGapInNumbering ? FColor(255,128,128) : FColor(255,0,255);
				else
					Color = bGapInNumbering ? FColor(255,128,128) : FColor(255,255,255);

				CanvasUtil.DrawLine( appRound(Start+(Step*x)), 3, appRound(Start+(Step*(x+1))), 3, Color );
				POP_HIT(Viewport);
			}

	}

	CanvasUtil.Flush();

	//
	// Draw the time marker
	//

	if( GMatineeTools->GetCurrentCameraPath() == this )
	{
		INT Pos = (Viewport->SizeX - 5) * GMatineeTools->TimeSliderPct;
		Viewport->Canvas->DrawIcon( GMatineeTools->TimeMarker,
			Pos, 0,
			GMatineeTools->TimeMarker->UClamp, GMatineeTools->TimeMarker->VClamp,
			0.0, FPlane(1,1,1,1), FPlane(0,0,0,0), 0 );
	}
	
	unguard;
}

// Figures out how wide the path will be when displayed in the viewport.
FLOAT FCameraPath::GetPathLengthInPixels()
{
	FLOAT Size = 0;
	for( INT x = 0 ; x < PointList.Num() ; ++x )
	{
		Size += PointList(x)->PathDist / 100.f;
		Size += IP_MARKER_SZ;
	}

	return Size;
}

/*------------------------------------------------------------------------------
	FMatineeTools.

	A helper class to store the state of the various matinee tools.
------------------------------------------------------------------------------*/

FMatineeTools::FMatineeTools()
{
	TimeSliderPct = 0.f;
}

FMatineeTools::~FMatineeTools()
{
}

void FMatineeTools::Init()
{
	guard(FMatineeTools::Init);

	bAlwaysShowPath = 0;
	bShowPathOrientation = 0;
	CurCameraPath = NULL;

	IntPoint = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.S_MatineeIP") ));
	check(IntPoint);
	IntPointSel = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.S_MatineeIPSel") ));
	check(IntPointSel);
	TimeMarker = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.S_MatineeTimeMarker") ));
	check(TimeMarker);

	unguard;
}

FCameraPath* FMatineeTools::NewCameraPath()
{
	guard(FMatineeTools::NewCameraPath);

	new( CameraPaths )FCameraPath();
	return &(CameraPaths(CameraPaths.Num()-1));

	unguard;
}

UBOOL FMatineeTools::DeleteCameraPath( FCameraPath* InPath )
{
	guard(FMatineeTools::DeleteCameraPath);

	for( INT x = 0 ; x < CameraPaths.Num() ; ++x )
	{
		if( CameraPaths(x).GetPathName() == InPath->GetPathName() )
		{
			CameraPaths.Remove(x);
			return 1;
		}
	}

	return 0;
	unguard;
}

FCameraPath* FMatineeTools::GetCameraPathFromName( FString InPathName )
{
	guard(FMatineeTools::GetCameraPathFromName);

	for( INT x = 0 ; x < CameraPaths.Num() ; ++x )
		if( CameraPaths(x).GetPathName() == InPathName )
			return &CameraPaths(x);

	return NULL;

	unguard;
}

void FMatineeTools::RebuildAllLists( ULevel* InLevel )
{
	guard(FMatineeTools::GetCameraPathFromName);

	for( INT x = 0 ; x < CameraPaths.Num() ; ++x )
		CameraPaths(x).RebuildPointList( InLevel );

	unguard;
}

FCameraPath* FMatineeTools::GetCurrentCameraPath()
{
	return CurCameraPath;
}

void FMatineeTools::SelectCameraPath( FString InPathName )
{
	CurCameraPath = GetCameraPathFromName( InPathName );
}

// Sets up proper next/prev linking for a list of sorted interpolation points.
void FMatineeTools::LinkPoints( TArray<AInterpolationPoint*>* InPointList )
{
	guard(FMatineeTools::LinkPoints);

	for( INT x = 0 ; x < InPointList->Num() ; ++x ) 
	{
		(*InPointList)(x)->Prev = (*InPointList)( (x-1 > -1) ? x-1 : InPointList->Num()-1 );
		(*InPointList)(x)->Next = (*InPointList)( (x+1 < InPointList->Num()) ? x+1 : 0 );
	}

	unguard;
}

void FMatineeTools::GetSamplePoints( AInterpolationPoint* InIP, FLOAT InResolution, TArray<FPosition>* InPositions, UBOOL InEmptyPosList )
{
	// Safety catch ... if this is a bad value, the editor locks.
	if( InResolution <= 0.f )
		InResolution = 0.1f;

	if( InEmptyPosList )
		InPositions->Empty();

	if( InIP->Prev )
	{
		InIP->PathDist = 0.f;
		if( !InIP->bInstantNextPath && !InIP->Prev->bEndOfPath )
		{	
			FCoords OldCoords = GMath.UnitCoords / InIP->Prev->Rotation;
			OldCoords.Origin = InIP->Prev->Location;

			FVector OldLocation = InIP->Prev->Location;

			for( FLOAT alpha = 0.f ; alpha <= 1.0f ; alpha += InResolution )
			{
				FCoords FirstCoords  = InIP->GetInterpolatedPosition(OldCoords, InIP->Prev->StartControlPoint, alpha, 0, OldLocation);
				FCoords SecondCoords = InIP->GetInterpolatedPosition(OldCoords, InIP->Prev->StartControlPoint, alpha+InResolution, 0, FirstCoords.Origin);
				OldLocation = SecondCoords.Origin;

				InIP->PathDist += (SecondCoords.Origin - FirstCoords.Origin).Size();

				InPositions->AddItem( FPosition( FirstCoords.Origin, FirstCoords ) );
			}
		}
	}
}

// Inserts an interpolation point at the percentage position specified (as close as it can).
void FMatineeTools::InsertPointAtPct( FCameraPath* InPath, FLOAT InPct )
{
	/*
	// Compute the position we want to insert the new point at.
	FLOAT Position = InPath->GetPathLengthInPixels() * InPath->LastScale;

	FLOAT XPos = 0;
	for( INT x = 0 ; x < PointList.Num() ; ++x )
	{
		AInterpolationPoint* IP = PointList(x);

		INT Start = appRound(XPos) + IP_MARKER_SZ;
		XPos += ((IP->Next->PathDist / 100.f) + IP_MARKER_SZ) * LastScale;
		INT End = appRound(XPos);

		//
		// CONNECTING LINE
		//

		UBOOL bGapInNumbering = IP->Next ? (IP->Next->Position != IP->Position + 1) : 0;

		FLOAT Step = (End - Start) / 10.f;

		End++;
		Start++;

		if( (End - Start) > 0 )
			for( INT x = 0 ; x < 10 ; ++x )
			{
				PUSH_HIT(InFrame,HMatLineSegmentPct(IP, (x+1) * 10 ));
				if( IP->Prev->bInstantNextPath )
					InFrame->Viewport->RenDev->Draw2DClippedLine( InFrame, bGapInNumbering ? FPlane(1,.5,.5,0) : FPlane(1,0,1,0), LINE_None, FVector(appRound(Start+(Step*x)),3,0), FVector(appRound(Start+(Step*(x+1))),3,0) );
				else
					InFrame->Viewport->RenDev->Draw2DClippedLine( InFrame, bGapInNumbering ? FPlane(1,.5,.5,0) : FPlane(1,1,1,0), LINE_None, FVector(appRound(Start+(Step*x)),3,0), FVector(appRound(Start+(Step*(x+1))),3,0) );
				POP_HIT(InFrame);
			}
	}
	*/
}

// Rebuilds all point lists in all camera paths. Called by the editor when, for example, actors are deleted.
void FMatineeTools::RebuildAllPointLists( ULevel* InLevel )
{
	for( INT x = 0 ; x < CameraPaths.Num() ; ++x )
		CameraPaths(x).RebuildPointList( InLevel );
}

/*------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------*/

