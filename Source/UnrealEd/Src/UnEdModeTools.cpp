/*=============================================================================
	UnEdModeTools.cpp: Classes which represent "modes" in the editor
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "UnrealEd.h"

extern FVector GPivotLocation, GSnappedLocation, GGridBase;
extern FRotator GSnappedRotation;
extern FRotator GPivotRotation, GSnappedRotation;
extern UBOOL GPivotShown, GSnapping;
extern INT GARGAxis;
extern UBOOL GbARG;
extern TArray<HBezierControlPoint> BezierControlPointList;

/*------------------------------------------------------------------------------
	FEdModeTools.

	A class to keep track of things like what the current current mode.
------------------------------------------------------------------------------*/

FEdModeTools::FEdModeTools()
{
	SetMode( EMODE_CAMERA );
	bMouseHasMoved = 0;
}

FEdModeTools::~FEdModeTools()
{
	Modes.Empty();
}

void FEdModeTools::Init()
{
	// Create the list of modes.
	Modes.Empty();
	new( Modes )UEdModeCamera();
}

void FEdModeTools::SetMode( INT InMode )
{
	check( InMode >= 0 && InMode < EMODE_MAX );

	Mode = InMode;
}

// Determines if the mouse has moved since the last click/unclick
inline UBOOL FEdModeTools::HasMouseMoved()
{
	return bMouseHasMoved;
}

void FEdModeTools::CalcFreeMoveRot( UViewport* Viewport, FLOAT DeltaX, FLOAT DeltaY, FVector& Delta, FRotator& DeltaRot )
{
	guard(FEdModeTools::CalcFreeMoveRot);
	if( Viewport->IsOrtho() )
	{
		// Figure axes.
		FLOAT *OrthoAxis1, *OrthoAxis2, Axis2Sign, Axis1Sign, *OrthoAngle, AngleSign;
		FLOAT DeltaPitch = DeltaRot.Pitch;
		FLOAT DeltaYaw   = DeltaRot.Yaw;
		FLOAT DeltaRoll  = DeltaRot.Roll;
		if( Viewport->Actor->RendMap == REN_OrthXY )
		{
			OrthoAxis1 = &Delta.X;  	Axis1Sign = +1;
			OrthoAxis2 = &Delta.Y;  	Axis2Sign = +1;
			OrthoAngle = &DeltaYaw;		AngleSign = +1;
		}
		else if( Viewport->Actor->RendMap==REN_OrthXZ )
		{
			OrthoAxis1 = &Delta.X; 		Axis1Sign = +1;
			OrthoAxis2 = &Delta.Z; 		Axis2Sign = -1;
			OrthoAngle = &DeltaPitch; 	AngleSign = +1;
		}
		else if( Viewport->Actor->RendMap==REN_OrthYZ )
		{
			OrthoAxis1 = &Delta.Y; 		Axis1Sign = +1;
			OrthoAxis2 = &Delta.Z; 		Axis2Sign = -1;
			OrthoAngle = &DeltaRoll; 	AngleSign = +1;
		}
		else
		{
			appErrorf( TEXT("Invalid rendering mode") );
			return;
		}

		// Special movement controls.
		if( (Viewport->Buttons&(MOUSE_Left|MOUSE_Right))==MOUSE_Left || (Viewport->Buttons&(MOUSE_Middle))==MOUSE_Middle )
		{
			// Left button: Move up/down/left/right.
			*OrthoAxis1 = Viewport->Actor->OrthoZoom/30000.0*(FLOAT)DeltaX;
			if     ( DeltaX<0 && *OrthoAxis1==0 ) *OrthoAxis1 = -Axis1Sign;
			else if( DeltaX>0 && *OrthoAxis1==0 ) *OrthoAxis1 = +Axis1Sign;

			*OrthoAxis2 = Axis2Sign*Viewport->Actor->OrthoZoom/30000.0*(FLOAT)DeltaY;
			if     ( DeltaY<0 && *OrthoAxis2==0 ) *OrthoAxis2 = -Axis2Sign;
			else if( DeltaY>0 && *OrthoAxis2==0 ) *OrthoAxis2 = +Axis2Sign;
		}
		else if( (Viewport->Buttons&(MOUSE_Left|MOUSE_Right))==(MOUSE_Left|MOUSE_Right) )
		{
			// Both buttons: Zoom in/out.
			Viewport->Actor->OrthoZoom -= Viewport->Actor->OrthoZoom/200.0 * (FLOAT)DeltaY;
			if( Viewport->Actor->OrthoZoom<MIN_ORTHOZOOM ) Viewport->Actor->OrthoZoom = MIN_ORTHOZOOM;
			if( Viewport->Actor->OrthoZoom>MAX_ORTHOZOOM ) Viewport->Actor->OrthoZoom = MAX_ORTHOZOOM;
		}
		else if( (Viewport->Buttons&(MOUSE_Left|MOUSE_Right))==MOUSE_Right )
		{
			// Right button: Rotate.
			if( OrthoAngle!=NULL )
				*OrthoAngle = -AngleSign*8.0*(FLOAT)DeltaX;
		}
		DeltaRot.Pitch	= DeltaPitch;
		DeltaRot.Yaw	= DeltaYaw;
		DeltaRot.Roll	= DeltaRoll;
	}
	else
	{
		if( (Viewport->Buttons&(MOUSE_Left|MOUSE_Right))==(MOUSE_Left) )
		{
			// Left button: move ahead and yaw.
			Delta.X      = -DeltaY * GMath.CosTab(Viewport->Actor->Rotation.Yaw);
			Delta.Y      = -DeltaY * GMath.SinTab(Viewport->Actor->Rotation.Yaw);
			DeltaRot.Yaw = +DeltaX * 64.0 / 20.0;
		}
		else if( (Viewport->Buttons&(MOUSE_Left|MOUSE_Right))==(MOUSE_Left|MOUSE_Right) )
		{
			// Both buttons: Move up and left/right.
			Delta.X      = +DeltaX * -GMath.SinTab(Viewport->Actor->Rotation.Yaw);
			Delta.Y      = +DeltaX *  GMath.CosTab(Viewport->Actor->Rotation.Yaw);
			Delta.Z      = -DeltaY;
		}
		else if( (Viewport->Buttons&(MOUSE_Left|MOUSE_Right))==(MOUSE_Right) )
		{
			if( Viewport->Buttons&MOUSE_Alt )
			{
				// Right button + ALT: Roll
				DeltaRot.Roll = (64.0/20.0) * +DeltaX;
			}
			else
			{
				// Right button: Pitch and yaw.
				DeltaRot.Pitch = (64.0/12.0) * -DeltaY;
				DeltaRot.Yaw = (64.0/20.0) * +DeltaX;
			}
		}
	}
	unguard;
}

void FEdModeTools::ViewportMoveRotCamera( UViewport* Viewport, FVector& Delta, FRotator& DeltaRot )
{
	guard(FEdModeTools::ViewportMoveRotWithPhysics);

	Viewport->Actor->Rotation.AddBounded( 4.0*DeltaRot.Pitch, 4.0*DeltaRot.Yaw, 4.0*DeltaRot.Roll );
	Viewport->Actor->Location.AddBounded( Delta, HALF_WORLD_MAX1 );

	// If we're locked on a selected actor, update it as well.
	if( !Viewport->IsOrtho() && Viewport->bLockOnSelectedActors && Viewport->LockedActor )
	{
		Viewport->LockedActor->Rotation += 4.0 * DeltaRot;
		Viewport->LockedActor->Location += Delta;
	}
	unguard;
}

void FEdModeTools::MoveActors( UViewport* Viewport, ULevel* Level, FVector Delta, FRotator DeltaRot, UBOOL Constrained, AActor* ViewActor, UBOOL bForceSnapping )
{
	guard(FEdModeTools::MoveActors);

	if( Delta.IsZero() && DeltaRot.IsZero() )
		return;

	// Transact the actors.
	GEditor->NoteActorMovement( Level );

	// Update global pivot.
	if( Constrained )
	{
		FVector OldLocation = GSnappedLocation;
		FRotator OldRotation = GSnappedRotation;
		GSnappedLocation      = (GPivotLocation += Delta   );
		GSnappedRotation      = (GPivotRotation += DeltaRot);
		if( GSnapping || bForceSnapping )
			GEditor->Constraints.Snap( Level, GSnappedLocation, GGridBase, GSnappedRotation );
		Delta                 = GSnappedLocation - OldLocation;
		DeltaRot              = GSnappedRotation - OldRotation;
	}

	/*
	if( GbIsBoxSel )
	{
		check(Viewport);

		if( Viewport->IsOrtho() )
			GBoxSelEnd += Delta;
		//else
		//	GBoxSelEnd = Viewport->MouseClientPos;
		return;
	}
	if( GbIsMeasuring )
	{
		GMeasureEnd += Delta;
		GMeasureEndSnapped = GMeasureEnd;
		GEditor->Constraints.Snap( GMeasureEndSnapped, GGridBase );
		return;
	}
	*/

	// Move the actors.
	if( Delta!=FVector(0,0,0) || DeltaRot!=FRotator(0,0,0) )
	{
		for( INT i=0; i<GEditor->Level->Actors.Num(); i++ )
		{
			AActor* Actor = GEditor->Level->Actors(i);
			if( Actor && (Actor->bSelected || Actor==ViewActor) && !Actor->bLockLocation )
			{
				// Cannot move brushes while in brush clip mode - only regular actors.
				// This allows you to adjust the clipping marker positions, but the brushes
				// will remain locked in place.
				//if( GEditor->Mode == EM_BrushClip && Actor->IsBrush() )
				//	continue;

				// Can't move any actors while in vertex editing mode
				/*
				if( (GEditor->Mode == EM_VertexEdit || GEditor->Mode == EM_FaceDrag) && Actor->IsBrush() )
				{
					for( INT vertex = 0 ; vertex < VertexHitList.Num() ; vertex++ )
						if( VertexHitList(vertex).pBrush == (ABrush*)Actor )
						{
							FVector* Vtx = &(VertexHitList(vertex).pBrush->Brush->Polys->Element(VertexHitList(vertex).PolyIndex).Vertex[VertexHitList(vertex).VertexIndex]);

							FVector Vertex = Vtx->TransformPointBy( VertexHitList(vertex).pBrush->ToWorld() );
							Vertex += Delta;
							*Vtx = Vertex.TransformPointBy( VertexHitList(vertex).pBrush->ToLocal() );
						}
					continue;
				}
				*/

				FVector Arm   = GSnappedLocation - Actor->Location;
				FVector Rel   = Arm - Arm.TransformVectorBy(GMath.UnitCoords * DeltaRot);
				if( !BezierControlPointList.Num() )
					MoveSingleActor( Actor, Delta + Rel, DeltaRot );
			}
		}
	}

	// If we have bezier control points selected, move them.
	if( BezierControlPointList.Num() )
	{
		for( INT x = 0 ; x < BezierControlPointList.Num() ; x++ )
		{
			UMatAction* MA = BezierControlPointList(x).MA;
			if( BezierControlPointList(x).bStart )
			{
				MA->StartControlPoint += Delta;
				if( MA->bSmoothCorner )	MA->EndControlPoint = MA->StartControlPoint * -1;
			}
			else
			{
				MA->EndControlPoint += Delta;
				if( MA->bSmoothCorner )	MA->StartControlPoint = MA->EndControlPoint * -1;
			}
		}
	}

	unguard;
}

void FEdModeTools::MoveSingleActor( AActor* Actor, FVector Delta, FRotator DeltaRot )
{
	guard(FEdModeTools::MoveSingleActor);
	if( Delta != FVector(0,0,0) )
		Actor->bLightChanged = 1;
	Actor->Location.AddBounded( Delta, HALF_WORLD_MAX1 );

	if( Actor->IsBrush() )
	{
		if( !DeltaRot.IsZero() )
		{
			// Rotate brush vertices
			ABrush* Brush = (ABrush*)Actor;
			for( INT poly = 0 ; poly < Brush->Brush->Polys->Element.Num() ; poly++ )
			{
				FPoly* Poly = &(Brush->Brush->Polys->Element(poly));

				// Rotate the vertices
				for( INT vertex = 0 ; vertex < Poly->NumVertices ; vertex++ )
					Poly->Vertex[vertex] = Brush->PrePivot + ( Poly->Vertex[vertex] - Brush->PrePivot ).TransformVectorBy( GMath.UnitCoords * DeltaRot );
				Poly->Base = Brush->PrePivot + ( Poly->Base - Brush->PrePivot ).TransformVectorBy( GMath.UnitCoords * DeltaRot );

				// Rotate the texture vectors
				Poly->TextureU = Poly->TextureU.TransformVectorBy( GMath.UnitCoords * DeltaRot );
				Poly->TextureV = Poly->TextureV.TransformVectorBy( GMath.UnitCoords * DeltaRot );

				// Recalc the normal for the poly
				Poly->Normal = FVector(0,0,0);	// Force the normal to recalc
				Poly->Finalize(0);
			}

			Brush->Brush->BuildBound();
		}
	}
	else
		if( GbARG )
		{
			// Use the most influential axis (automatically accounts for different viewport types)
			INT Rot = DeltaRot.Yaw;
			if( ::abs(DeltaRot.Pitch) > ::abs(DeltaRot.Yaw) )
				Rot = DeltaRot.Pitch;
			else if( ::abs(DeltaRot.Roll) > ::abs(DeltaRot.Pitch) )
				Rot = DeltaRot.Roll;

			switch( GARGAxis )
			{
				case AXIS_X:	Actor->Rotation.Pitch += Rot;	break;
				case AXIS_Y:	Actor->Rotation.Yaw += Rot;		break;
				case AXIS_Z:	Actor->Rotation.Roll += Rot;	break;
			}
		}
		else
			Actor->Rotation += DeltaRot;

	APawn* P = Cast<APawn>( Actor );
	if( P && P->Controller )
		P->Controller->Rotation = Actor->Rotation;

	Actor->ClearRenderData();

	unguard;
}

/*------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------*/
