/*=============================================================================
	UnEdCam.cpp: Unreal editor camera movement/selection functions
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "UnrealEd.h"
#include "UnRender.h"
#include <math.h>            //for  fabs()

extern void vertexedit_Click( UViewport* InViewport, ABrush* pBrush, FVector InLocation, UBOOL InCumulative, UBOOL InAllowDuplicates );

TArray<FVertexHit> VertexHitList;
TArray<HBezierControlPoint> BezierControlPointList;


/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

// Used for box selection.
FVector GBoxSelStart, GBoxSelEnd;
UBOOL GbIsBoxSel = 0;

// Used for measuring tool.
FVector GMeasureStart, GMeasureEnd, GMeasureEndSnapped;
UBOOL GbIsMeasuring = 0;

// Used for actor rotation gizmo.
INT GARGAxis = -1;
UBOOL GbARG = 0;

// Used for resizing actors (MODE ACTORSNAP).
FVector GOldSnapScaleStart, GOldSnapScaleEnd, GSnapScaleStart, GSnapScaleEnd, GSnapScaleStartSnapped, GSnapScaleEndSnapped;
UBOOL GbIsSnapScaleBox = 0;

// Click flags.
enum EViewportClick
{
	CF_MOVE_ACTOR	= 1,	// Set if the actors have been moved since first click
	CF_MOVE_TEXTURE = 2,	// Set if textures have been adjusted since first click
	CF_MOVE_ALL     = (CF_MOVE_ACTOR | CF_MOVE_TEXTURE),
};

// Internal declarations.
void NoteTextureMovement( ULevel* Level );
void MoveActors( UViewport* Viewport, ULevel* Level, FVector Delta, FRotator DeltaRot, UBOOL Constrained, AActor* ViewActor, UBOOL bForceSnapping = 0 );

// Global variables.
UPrefab* GCurrentPrefab = NULL;
ULevel* GPrefabLevel = NULL;		// A temporary level we assign to the prefab viewport, where we hold the prefabs actors for viewing.
INT GFixPanU=0, GFixPanV=0;
INT GFixScale=0;

// Editor state.
UBOOL GPivotShown=0, GSnapping=0;
FVector GPivotLocation, GSnappedLocation, GGridBase;
FRotator GPivotRotation, GSnappedRotation;

// Temporary.
static TArray<INT> OriginalUVectors;
static TArray<INT> OriginalVVectors;

/*-----------------------------------------------------------------------------
   Primitive mappings of input to axis movement and rotation.
-----------------------------------------------------------------------------*/

//
// Axial rotation.
//
void CalcAxialRot
( 
	UViewport*	Viewport, 
	SWORD		MouseX,
	SWORD		MouseY,
	DWORD		Buttons,
	FRotator&	Delta
)
{
	guard(CalcAxialRot);

	// Do single-axis movement.
	if	   ( (Buttons&(MOUSE_Left|MOUSE_Right))==(MOUSE_Left)             ) Delta.Pitch = +MouseX*4;
	else if( (Buttons&(MOUSE_Left|MOUSE_Right))==(MOUSE_Right)            )	Delta.Yaw   = +MouseX*4;
	else if( (Buttons&(MOUSE_Left|MOUSE_Right))==(MOUSE_Left|MOUSE_Right) ) Delta.Roll  = -MouseY*4;

	unguard;
}

//
// Freeform movement and rotation.
//
void CalcFreeMoveRot
(
	UViewport*	Viewport,
	FLOAT		MouseX,
	FLOAT		MouseY,
	DWORD		Buttons,
	FVector&	Delta,
	FRotator&	DeltaRot
)
{
	guard(CalcFreeMoveRot);
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
		if( (Buttons&(MOUSE_Left|MOUSE_Right))==MOUSE_Left || (Buttons&(MOUSE_Middle))==MOUSE_Middle )
		{
			// Left button: Move up/down/left/right.
			*OrthoAxis1 = Viewport->Actor->OrthoZoom/30000.0*(FLOAT)MouseX;
			if     ( MouseX<0 && *OrthoAxis1==0 ) *OrthoAxis1 = -Axis1Sign;
			else if( MouseX>0 && *OrthoAxis1==0 ) *OrthoAxis1 = +Axis1Sign;

			*OrthoAxis2 = Axis2Sign*Viewport->Actor->OrthoZoom/30000.0*(FLOAT)MouseY;
			if     ( MouseY<0 && *OrthoAxis2==0 ) *OrthoAxis2 = -Axis2Sign;
			else if( MouseY>0 && *OrthoAxis2==0 ) *OrthoAxis2 = +Axis2Sign;
		}
		else if( (Buttons&(MOUSE_Left|MOUSE_Right))==(MOUSE_Left|MOUSE_Right) )
		{
			// Both buttons: Zoom in/out.
			Viewport->Actor->OrthoZoom -= Viewport->Actor->OrthoZoom/200.0 * (FLOAT)MouseY;
			if( Viewport->Actor->OrthoZoom<MIN_ORTHOZOOM ) Viewport->Actor->OrthoZoom = MIN_ORTHOZOOM;
			if( Viewport->Actor->OrthoZoom>MAX_ORTHOZOOM ) Viewport->Actor->OrthoZoom = MAX_ORTHOZOOM;
		}
		else if( (Buttons&(MOUSE_Left|MOUSE_Right))==MOUSE_Right )
		{
			// Right button: Rotate.
			if( OrthoAngle!=NULL )
				*OrthoAngle = -AngleSign*8.0*(FLOAT)MouseX;
		}
		DeltaRot.Pitch	= DeltaPitch;
		DeltaRot.Yaw	= DeltaYaw;
		DeltaRot.Roll	= DeltaRoll;
	}
	else
	{
		if( (Buttons&(MOUSE_Left|MOUSE_Right))==(MOUSE_Left) )
		{
			// Left button: move ahead and yaw.
			Delta.X      = -MouseY * GMath.CosTab(Viewport->Actor->Rotation.Yaw);
			Delta.Y      = -MouseY * GMath.SinTab(Viewport->Actor->Rotation.Yaw);
			DeltaRot.Yaw = +MouseX * 64.0 / 20.0;
		}
		else if( (Buttons&(MOUSE_Left|MOUSE_Right))==(MOUSE_Left|MOUSE_Right) )
		{
			// Both buttons: Move up and left/right.
			Delta.X      = +MouseX * -GMath.SinTab(Viewport->Actor->Rotation.Yaw);
			Delta.Y      = +MouseX *  GMath.CosTab(Viewport->Actor->Rotation.Yaw);
			Delta.Z      = -MouseY;
		}
		else if( (Buttons&(MOUSE_Left|MOUSE_Right))==(MOUSE_Right) )
		{
			if( Buttons&MOUSE_Alt )
			{
				// Right button + ALT: Roll
				DeltaRot.Roll = (64.0/20.0) * +MouseX;
			}
			else
			{
				// Right button: Pitch and yaw.
				DeltaRot.Pitch = (64.0/12.0) * -MouseY;
				DeltaRot.Yaw = (64.0/20.0) * +MouseX;
			}
		}
	}
	unguard;
}

//
// Perform axial movement and rotation.
//
void CalcAxialMoveRot
(
	UViewport*	Viewport,
	FLOAT		MouseX,
	FLOAT		MouseY,
	DWORD		Buttons,
	FVector&	Delta,
	FRotator&	DeltaRot
)
{
	guard(CalcFreeMoveRot);
	if( Viewport->IsOrtho() )
	{
		// Figure out axes.
		FLOAT *OrthoAxis1,*OrthoAxis2,Axis2Sign,Axis1Sign,*OrthoAngle,AngleSign;
		FLOAT DeltaPitch = DeltaRot.Pitch;
		FLOAT DeltaYaw   = DeltaRot.Yaw;
		FLOAT DeltaRoll  = DeltaRot.Roll;
		if( Viewport->Actor->RendMap == REN_OrthXY )
		{
			OrthoAxis1 = &Delta.X;  	Axis1Sign = +1;
			OrthoAxis2 = &Delta.Y;  	Axis2Sign = +1;
			OrthoAngle = &DeltaYaw;		AngleSign = +1;
		}
		else if( Viewport->Actor->RendMap == REN_OrthXZ )
		{
			OrthoAxis1 = &Delta.X; 		Axis1Sign = +1;
			OrthoAxis2 = &Delta.Z;		Axis2Sign = -1;
			OrthoAngle = &DeltaPitch; 	AngleSign = +1;
		}
		else if( Viewport->Actor->RendMap == REN_OrthYZ )
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
		if( Buttons & (MOUSE_Left | MOUSE_Right) )
		{
			// Left, right, or both are pressed.
			if( Buttons & MOUSE_Left )
			{
				// Left button: Screen's X-Axis.
      			*OrthoAxis1 = Viewport->Actor->OrthoZoom/30000.0*(FLOAT)MouseX;
      			if     ( MouseX<0 && *OrthoAxis1==0 ) *OrthoAxis1 = -Axis1Sign;
      			else if( MouseX>0 && *OrthoAxis1==0 ) *OrthoAxis1 = +Axis1Sign;
			}
			if( Buttons & MOUSE_Right )
			{
				// Right button: Screen's Y-Axis.
      			*OrthoAxis2 = Axis2Sign*Viewport->Actor->OrthoZoom/30000.0*(FLOAT)MouseY;
      			if     ( MouseY<0 && *OrthoAxis2==0 ) *OrthoAxis2 = -Axis2Sign;
      			else if( MouseY>0 && *OrthoAxis2==0 ) *OrthoAxis2 = +Axis2Sign;
			}
		}
		else if( Buttons & MOUSE_Middle )
		{
			// Middle button: Zoom in/out.
			Viewport->Actor->OrthoZoom -= Viewport->Actor->OrthoZoom/200.0 * (FLOAT)MouseY;
			if	   ( Viewport->Actor->OrthoZoom<MIN_ORTHOZOOM ) Viewport->Actor->OrthoZoom = MIN_ORTHOZOOM;
			else if( Viewport->Actor->OrthoZoom>MAX_ORTHOZOOM ) Viewport->Actor->OrthoZoom = MAX_ORTHOZOOM;
		}
		DeltaRot.Pitch	= DeltaPitch;
		DeltaRot.Yaw	= DeltaYaw;
		DeltaRot.Roll	= DeltaRoll;
	}
	else
	{
		if( GARGAxis == -1 )
		{
			// Do single-axis movement.
			if		((Buttons&(MOUSE_Left|MOUSE_Right))==(MOUSE_Left))			   Delta.X = +MouseX;
			else if ((Buttons&(MOUSE_Left|MOUSE_Right))==(MOUSE_Right))			   Delta.Y = +MouseX;
			else if ((Buttons&(MOUSE_Left|MOUSE_Right))==(MOUSE_Left|MOUSE_Right)) Delta.Z = -MouseY;
		}
		else
		{
			switch( GARGAxis )
			{
				case AXIS_X:	DeltaRot.Pitch = MouseX*GUnrealEd->MovementSpeed;	break;
				case AXIS_Y:	DeltaRot.Yaw = MouseX*GUnrealEd->MovementSpeed;	break;
				case AXIS_Z:	DeltaRot.Roll = MouseX*GUnrealEd->MovementSpeed;	break;
			}
		}
	}
	unguard;
}

//
// Mixed movement and rotation.
//
void CalcMixedMoveRot
(
	UViewport*	Viewport,
	FLOAT		MouseX,
	FLOAT		MouseY,
	DWORD		Buttons,
	FVector&	Delta,
	FRotator&	DeltaRot
)
{
	guard(CalcMixedMoveRot);
	if( Viewport->IsOrtho() )
		CalcFreeMoveRot( Viewport, MouseX, MouseY, Buttons, Delta, DeltaRot );
	else
		CalcAxialMoveRot( Viewport, MouseX, MouseY, Buttons, Delta, DeltaRot );
	unguard;
}

/*-----------------------------------------------------------------------------
   Viewport movement computation.
-----------------------------------------------------------------------------*/

//
// Move and rotate viewport using gravity and collision where appropriate.
//
void ViewportMoveRotWithPhysics
(
	UViewport*	Viewport,
	FVector&	Delta,
	FRotator&	DeltaRot
)
{
	guard(ViewportMoveRotWithPhysics);

	if( Viewport->Actor->RendMap == REN_TexView && Viewport->Actor->Misc1 != MVT_TEXTURE )
		Viewport->Actor->Rotation.AddBounded( 4.0*DeltaRot.Pitch*-1, 4.0*DeltaRot.Yaw*-1, 4.0*DeltaRot.Roll );
	else
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

/*-----------------------------------------------------------------------------
   Change transacting.
-----------------------------------------------------------------------------*/

//
// If this is the first time called since first click, note all selected actors.
//
void UUnrealEdEngine::NoteActorMovement( ULevel* Level )
{
	guard(NoteActorMovement);
	if( !GUndo && !(GUnrealEd->ClickFlags & CF_MOVE_ACTOR) )
	{
		GUnrealEd->ClickFlags |= CF_MOVE_ACTOR;
		GUnrealEd->Trans->Begin( TEXT("Actor movement") );
		GSnapping=0;
		INT i;
		for( i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor = Level->Actors(i);
			if( Actor && Actor->bSelected )
				break;
		}
		if( i==Level->Actors.Num() && !BezierControlPointList.Num() )
			SelectActor( Level, Level->Brush() );
		for( i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor = Level->Actors(i);
			if( Actor && Actor->bSelected && Actor->bEdShouldSnap )
				GSnapping = 1;
		}
		for( i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor = Level->Actors(i);
			if( Actor && Actor->bSelected )
			{
				Actor->Modify();
				if( Actor->IsBrush() )
					Actor->Brush->Polys->Element.ModifyAllItems();
				Actor->bEdSnap |= GSnapping;
			}
		}
		GUnrealEd->Trans->End();
	}
	unguard;
}

//
// Finish snapping all brushes in a level.
//
void UUnrealEdEngine::FinishAllSnaps( ULevel* Level )
{
	guard(UUnrealEdEngine::FinishAllSnaps);

	ClickFlags &= ~CF_MOVE_ACTOR;

	for(INT ActorIndex = 0;ActorIndex < Level->Actors.Num();ActorIndex++)
	{
		AActor*	Actor = Level->Actors(ActorIndex);

		if(Actor && Actor->bSelected)
			Actor->PostEditMove();
	}

	unguard;
}

//
// Get the editor's pivot location
//
FVector UUnrealEdEngine::GetPivotLocation()
{ 
	return GPivotLocation;
}

//
// Set the editor's pivot location.
//
void UUnrealEdEngine::SetPivot( FVector NewPivot, UBOOL SnapPivotToGrid, UBOOL DoMoveActors, UBOOL bIgnoreAxis )
{
	guard(UUnrealEdEngine::SetPivot);

	if( !bIgnoreAxis )
	{
		// Don't stomp on orthonormal axis.
		if( NewPivot.X==0 ) NewPivot.X=GPivotLocation.X;
		if( NewPivot.Y==0 ) NewPivot.Y=GPivotLocation.Y;
		if( NewPivot.Z==0 ) NewPivot.Z=GPivotLocation.Z;
	}

	// Set the pivot.
	GPivotLocation   = NewPivot;
	GPivotRotation   = FRotator(0,0,0);
	GGridBase        = FVector(0,0,0);
	GSnappedLocation = GPivotLocation;
	GSnappedRotation = GPivotRotation;
	if( GSnapping || SnapPivotToGrid )
		Constraints.Snap( Level, GSnappedLocation, GGridBase, GSnappedRotation );
	if( SnapPivotToGrid )
	{
		if( DoMoveActors )
			MoveActors( NULL, Level, GSnappedLocation-GPivotLocation, FRotator(0,0,0), 0, NULL );
		GPivotLocation = GSnappedLocation;
		GPivotRotation = GSnappedRotation;
	}
	else
	{
		GGridBase = GPivotLocation - GSnappedLocation;
		GSnappedLocation = GPivotLocation;
		Constraints.Snap( Level, GSnappedLocation, GGridBase, GSnappedRotation );
		GPivotLocation = GSnappedLocation;
	}

	// Check all actors.
	INT Count=0, SnapCount=0;
	AActor* SingleActor=NULL;
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		if( Level->Actors(i) && Level->Actors(i)->bSelected )
		{
			Count++;
			SnapCount += Level->Actors(i)->bEdShouldSnap;
			SingleActor = Level->Actors(i);
		}
	}

	// Apply to actors.
	if( Count==1 )
	{
		ABrush* Brush=Cast<ABrush>( SingleActor );
		if( Brush )
		{
			FModelCoords Coords, Uncoords;
			Brush->BuildCoords( &Coords, &Uncoords );
			Brush->Modify();
			Brush->PrePivot += (GSnappedLocation - Brush->Location).TransformVectorBy( Uncoords.PointXform );
			Brush->Location = GSnappedLocation;
			Brush->PostEditChange();
		}
	}

	// Update showing.
	GPivotShown = SnapCount>0 || Count>1;
	unguard;
}

//
// Reset the editor's pivot location.
//
void UUnrealEdEngine::ResetPivot()
{
	guard(UUnrealEdEngine::ResetPivot);
	GPivotShown = 0;
	GSnapping   = 0;
	unguard;
}

//
// Move a single actors.
//
void MoveSingleActor( AActor* Actor, FVector Delta, FRotator DeltaRot )
{
	guard(MoveSingleActor);
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

			if( !Brush->IsStaticBrush() )
				GUnrealEd->csgPrepMovingBrush( Brush );
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

//
// Move and rotate actors.
//
void MoveActors( UViewport* Viewport, ULevel* Level, FVector Delta, FRotator DeltaRot, UBOOL Constrained, AActor* ViewActor, UBOOL bForceSnapping )
{
	guard(MoveActors);

	if( (Delta.IsZero() && DeltaRot.IsZero()) || (Viewport && Viewport->Actor->RendMap == REN_MatineePreview) )
		return;

	// Transact the actors.
	GUnrealEd->NoteActorMovement( Level );

	// Update global pivot.
	if( Constrained )
	{
		FVector OldLocation = GSnappedLocation;
		FRotator OldRotation = GSnappedRotation;
		GSnappedLocation      = (GPivotLocation += Delta   );
		GSnappedRotation      = (GPivotRotation += DeltaRot);
		if( GSnapping || bForceSnapping )
			GUnrealEd->Constraints.Snap( Level, GSnappedLocation, GGridBase, GSnappedRotation );
		Delta                 = GSnappedLocation - OldLocation;
		DeltaRot              = GSnappedRotation - OldRotation;
	}

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
		GUnrealEd->Constraints.Snap( GMeasureEndSnapped, GGridBase );
		return;
	}

	// Move the actors.
	if( Delta!=FVector(0,0,0) || DeltaRot!=FRotator(0,0,0) )
	{
		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor = Level->Actors(i);
			if( Actor && (Actor->bSelected || Actor==ViewActor) && !Actor->bLockLocation )
			{
				// Cannot move brushes while in brush clip mode - only regular actors.
				// This allows you to adjust the clipping marker positions, but the brushes
				// will remain locked in place.
				if( GUnrealEd->Mode == EM_BrushClip && Actor->IsBrush() )
					continue;

				// Can't move any actors while in vertex editing mode
				if( (GUnrealEd->Mode == EM_VertexEdit || GUnrealEd->Mode == EM_FaceDrag) && Actor->IsBrush() )
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

				FVector Arm   = GSnappedLocation - Actor->Location;
				FVector Rel   = Arm - Arm.TransformVectorBy(GMath.UnitCoords * DeltaRot);
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
				MA->EndControlPoint += Delta;
				if( MA->bSmoothCorner )	MA->StartControlPoint = MA->EndControlPoint * -1;
			}
			else
			{
				MA->StartControlPoint += Delta;
				if( MA->bSmoothCorner )	MA->EndControlPoint = MA->StartControlPoint * -1;
			}
		}
		if( GMatineeTools.GetCurrent() )
			GMatineeTools.GetCurrent()->PreparePath();
	}

	unguard;
}

/*-----------------------------------------------------------------------------
   Vertex editing functions.
-----------------------------------------------------------------------------*/

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,1)
#endif

struct FPolyVertex {
	FPolyVertex::FPolyVertex( INT i, INT j ) : PolyIndex(i), VertexIndex(j) {};
	INT PolyIndex GCC_PACK(1);
	INT VertexIndex GCC_PACK(1);
};

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif


static AActor* VertexEditActor=NULL;
static TArray<FPolyVertex> VertexEditList;

//
// Find the selected brush and grab the closest vertex when <Alt> is pressed
//
void GrabVertex( ULevel* Level )
{
	guard(GrabVertex);

	if( VertexEditActor!=NULL )
		return;

	// Find the selected brush -- abort if none is found.
	AActor* Actor=NULL;
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		Actor = Level->Actors(i);
		if( Actor && Actor->bSelected && Actor->IsBrush() )
		{
			VertexEditActor = Actor;
			break;
		}
	}
	if( VertexEditActor==NULL )
		return;

	//!! Tim, Undo doesn't seem to work for vertex editing.  Do I need to set RF_Transactional? LEGEND
	VertexEditActor->Brush->Modify();

	// examine all the points and grab those that are within range of the pivot location
	UPolys* Polys = VertexEditActor->Brush->Polys;
	for( INT i=0; i<Polys->Element.Num(); i++ ) 
	{
		FCoords BrushC(VertexEditActor->ToWorld());
		for( INT j=0; j<Polys->Element(i).NumVertices; j++ ) 
		{
			FVector Location = Polys->Element(i).Vertex[j].TransformPointBy(BrushC);
			// match GPivotLocation against Brush's vertex positions -- find "close" vertex
			if( FDist( Location, GPivotLocation ) < GUnrealEd->Constraints.SnapDistance ) {
				VertexEditList.AddItem( FPolyVertex( i, j ) );
			}
		}
	}

	unguard;
}

void RecomputePoly( FPoly* Poly )
{
	guard(RecomputePoly);

	// force recalculation of normal, and texture U and V coordinates in FPoly::Finalize()
	Poly->Normal = FVector(0,0,0);

	// catch normalization exceptions to warn about non-planar polys
	try
	{
		Poly->Finalize( 0 );
	}
	catch(...)
	{
		debugf( TEXT("WARNING: FPoly::Finalize() failed (You broke the poly!)") );
	}
	unguard;
}

//
// Release the vertex when <Alt> is released, then update the brush
//
void ReleaseVertex( ULevel* Level )
{
	guard(ReleaseVertex);

	if( VertexEditActor==NULL )
		return;

	// finalize all the polys in the brush (recompute poly surface and TextureU/V normals)
	UPolys* Polys = VertexEditActor->Brush->Polys;
	for( INT i=0; i<Polys->Element.Num(); i++ ) 
		RecomputePoly( &Polys->Element(i) );

	VertexEditActor->Brush->BuildBound();

	VertexEditActor=NULL;
	VertexEditList.Empty();

	unguard;
}

//
// Move a vertex.
//
void MoveVertex( ULevel* Level, FVector Delta, UBOOL Constrained )
{
	guard(MoveVertex);

	// Transact the actors.
	GUnrealEd->NoteActorMovement( Level );

	if( VertexEditActor==NULL )
		return;

	// Update global pivot.
	if( Constrained )
	{
		FVector OldLocation = GSnappedLocation;
		GSnappedLocation = ( GPivotLocation += Delta );
		if( GSnapping )
		{
			GGridBase = FVector(0,0,0);
			GUnrealEd->Constraints.Snap( Level, GSnappedLocation, GGridBase, GSnappedRotation );
		}
		Delta = GSnappedLocation - OldLocation;
	}

	// Move the vertex.
	if( Delta!=FVector(0,0,0) )
	{
		// examine all the points
		UPolys* Polys = VertexEditActor->Brush->Polys;

		Polys->Element.ModifyAllItems();

		FModelCoords Uncoords;
		((ABrush*)VertexEditActor)->BuildCoords( NULL, &Uncoords );
		VertexEditActor->Brush->Modify();
		for( INT k=0; k<VertexEditList.Num(); k++ ) 
		{
			INT i = VertexEditList(k).PolyIndex;
			INT j = VertexEditList(k).VertexIndex;
			Polys->Element(i).Vertex[j] += Delta.TransformVectorBy( Uncoords.PointXform );
		}
		VertexEditActor->Brush->PostEditChange();
	}

	unguard;
}

/*-----------------------------------------------------------------------------
   Editor surface transacting.
-----------------------------------------------------------------------------*/

//
// If this is the first time textures have been adjusted since the user first
// pressed a mouse button, save selected polygons transactionally so this can
// be undone/redone:
//
void NoteTextureMovement( ULevel* Level )
{
	guard(NoteTextureMovement);
	if( !GUndo && !(GUnrealEd->ClickFlags & CF_MOVE_TEXTURE) )
	{
		GUnrealEd->Trans->Begin( TEXT("Texture movement") );
		Level->Model->ModifySelectedSurfs(1);
		GUnrealEd->Trans->End();
		GUnrealEd->ClickFlags |= CF_MOVE_TEXTURE;
	}
	unguard;
}

// Checks the array of vertices and makes sure that the brushes in that list are still selected.  If not,
// the vertex is removed from the list.
void vertexedit_Refresh()
{
	guard(vertexedit_Refresh);

	for( INT vertex = 0 ; vertex < VertexHitList.Num() ; vertex++ )
		if( !VertexHitList(vertex).pBrush->bSelected )
		{
			VertexHitList.Remove(vertex);
			vertex = 0;
		}

	unguard;
}

// Fills up an array with a unique list of brushes which have vertices selected on them.
void vertexedit_GetBrushList( TArray<ABrush*>* BrushList )
{
	UBOOL bExists;

	BrushList->Empty();

	// Build a list of unique brushes
	//
	for( INT vertex = 0 ; vertex < VertexHitList.Num() ; vertex++ )
	{
		bExists = 0;

		for( INT x = 0 ; x < BrushList->Num() && !bExists ; x++ )
		{
			if( VertexHitList(vertex).pBrush == (*BrushList)(x) )
			{
				bExists = 1;
				break;
			}
		}

		if( !bExists )
			(*BrushList)( BrushList->Add() ) = VertexHitList(vertex).pBrush;
	}
}

/*-----------------------------------------------------------------------------
   Editor viewport movement.
-----------------------------------------------------------------------------*/

void UUnrealEdEngine::MouseWheel( UViewport* Viewport, DWORD Buttons, INT Delta)
{
	guard(UUnrealEdEngine::MouseWheel);

	switch( Viewport->Actor->RendMap )
	{
		case REN_OrthXY:
		case REN_OrthXZ:
		case REN_OrthYZ:
		{
			Viewport->Actor->OrthoZoom -= Viewport->Actor->OrthoZoom/200.0 * ((FLOAT)Delta/2.f);
			if( Viewport->Actor->OrthoZoom<MIN_ORTHOZOOM ) Viewport->Actor->OrthoZoom = MIN_ORTHOZOOM;
			if( Viewport->Actor->OrthoZoom>MAX_ORTHOZOOM ) Viewport->Actor->OrthoZoom = MAX_ORTHOZOOM;
			Viewport->Repaint(1);
		}
		break;

		case REN_TexBrowser:
		{
			Viewport->Actor->Misc2 -= Delta/1.5f;
			if( Viewport->Actor->Misc2 < 0 ) Viewport->Actor->Misc2 = 0;
			if( Viewport->Actor->Misc2 > GTBOptions->LastScroll ) Viewport->Actor->Misc2 = GTBOptions->LastScroll;
			Viewport->Repaint(1);
		}
		break;

		case REN_TexBrowserUsed:
		{
			Viewport->Actor->Misc2 -= Delta/1.5f;
			if( Viewport->Actor->Misc2 < 0 ) Viewport->Actor->Misc2 = 0;
			if( Viewport->Actor->Misc2 > GTBOptions->LastScrollUsed ) Viewport->Actor->Misc2 = GTBOptions->LastScrollUsed;
			Viewport->Repaint(1);
		}
		break;

		case REN_TexBrowserMRU:
		{
			Viewport->Actor->Misc2 -= Delta/1.5f;
			if( Viewport->Actor->Misc2 < 0 ) Viewport->Actor->Misc2 = 0;
			if( Viewport->Actor->Misc2 > GTBOptions->LastScrollMRU ) Viewport->Actor->Misc2 = GTBOptions->LastScrollMRU;
			Viewport->Repaint(1);
		}
		break;
	}

	unguard;
}

//
// Move the edit-viewport.
//
void UUnrealEdEngine::MouseDelta
(
	UViewport*	Viewport,
	DWORD		Buttons,
	FLOAT		DX,
	FLOAT		DY
)
{
	guard(UUnrealEdEngine::MouseDelta);

	// NEW MODES
	if( DX || DY )
	{
		GEdModeTools->bMouseHasMoved = 1;
	}
	//if( GEdModeTools->GetCurrentMode() ) GEdModeTools->GetCurrentMode()->MouseMoved( Viewport, Buttons, DX, DY );

	FVector     	Delta,Vector,SnapMin,SnapMax,DeltaMin,DeltaMax,DeltaFree;
	FRotator		DeltaRot;
	FLOAT			TempFloat,Speed;
	static FLOAT	TextureAngle=0.0;

	if( Viewport->Actor->RendMap==REN_TexView && Viewport->Actor->Misc1 == MVT_TEXTURE )
	{
		if( Buttons & MOUSE_FirstHit )
		{
			Viewport->SetMouseCapture( 0, 1 );
		}
		else if( Buttons & MOUSE_LastRelease )
		{
			Viewport->SetMouseCapture( 0, 0 );
		}
		return;
	}
	else if( Viewport->Actor->RendMap==REN_TexBrowser || Viewport->Actor->RendMap==REN_TexBrowserUsed || Viewport->Actor->RendMap==REN_TexBrowserMRU )
	{
		return;
	}

	ABrush* BrushActor = Viewport->Actor->GetLevel()->Brush();

	Delta.X    		= 0.0;  Delta.Y  		= 0.0;  Delta.Z   		= 0.0;
	DeltaRot.Pitch	= 0.0;  DeltaRot.Yaw	= 0.0;  DeltaRot.Roll	= 0.0;
	//

	if( Buttons & MOUSE_FirstHit )
	{
		// Reset flags that last for the duration of the click.
		Viewport->SetMouseCapture( 1, 1 );
		ClickFlags &= ~(CF_MOVE_ALL);
		BrushActor->Modify();

		if( Mode==EM_TerrainEdit )
		{
			GTerrainTools.bFirstClick = 1;
			GTerrainTools.CurrentBrush->MouseButtonDown( Viewport );
			//Trans->Begin( TEXT("Terrain Editing") );
		}
		else if( Mode==EM_VertexEdit )
		{
			GUnrealEd->Trans->Begin( TEXT("Vertex Editing") );

			for( INT vertex = 0 ; vertex < VertexHitList.Num() ; vertex++ )
			{
				VertexHitList(vertex).pBrush->Modify();
				VertexHitList(vertex).pBrush->Brush->Polys->Modify();
			}

			// Move the pivot point to the first vertex in the selection list.
			if( VertexHitList.Num() )
			{
				FCoords BrushW(VertexHitList(0).pBrush->ToWorld());
				FVector Vertex = VertexHitList(0).pBrush->Brush->Polys->Element(VertexHitList(0).PolyIndex).Vertex[VertexHitList(0).VertexIndex].TransformPointBy(BrushW);
				SetPivot( Vertex, 1, 0, 1 );
			}
		}
		else if( Mode==EM_FaceDrag )
		{
			GUnrealEd->Trans->Begin( TEXT("Face Dragging") );

			VertexHitList.Empty();

			if( Viewport->IsOrtho() )
			{
				// Loop through all the faces on the selected brushes.  For each one that qualifies to
				// be dragged, add it's vertices to the vertex editing list.
				for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
				{
					AActor* Actor = Level->Actors(i);
					if( Actor && Actor->bSelected && Actor->IsBrush() )
					{
						FVector ClickLocation = GUnrealEd->ClickLocation.TransformPointBy( Actor->ToLocal() );

						if( Viewport->Actor->RendMap == REN_OrthXY )
							ClickLocation.Z = 0;
						else if( Viewport->Actor->RendMap == REN_OrthXZ )
							ClickLocation.Y = 0;
						else
							ClickLocation.X = 0;
	
						for( INT poly = 0 ; poly < Actor->Brush->Polys->Element.Num() ; poly++ )
						{
							FPoly* Poly = &(Actor->Brush->Polys->Element(poly));

							FVector TestVector = Poly->Base - ClickLocation;
							TestVector.Normalize();

							FLOAT Dot = TestVector | Poly->Normal;
							if( Dot < 0.0f
									&& !Poly->IsBackfaced( ClickLocation ) )
							{
								UBOOL bOK = 0;

								// As a final test, attempt to trace a line to the each vertex of the face from the
								// click location.  If we can reach any one of it's vertices, include it.
								for( INT cmppoly = 0 ; cmppoly < Actor->Brush->Polys->Element.Num() && !bOK ; cmppoly++ )
								{
									FPoly* CmpPoly = &(Actor->Brush->Polys->Element(cmppoly));

									if( CmpPoly == Poly )
										continue;

									FVector Center = FVector(0,0,0);
									for( INT cmpvtx = 0 ; cmpvtx < CmpPoly->NumVertices ; cmpvtx++ )
										Center += CmpPoly->Vertex[cmpvtx];
									Center /= CmpPoly->NumVertices;

									FVector Dir = Center - ClickLocation;
									Dir.Normalize();
									if( CmpPoly->DoesLineIntersect( ClickLocation, ClickLocation + (Dir * 16384 ), NULL ) )
									{
										bOK = 1;
										break;
									}
								}

								// We've passed all the tests, so add this face to the hit list.

								if( bOK )
									for( INT vertex = 0 ; vertex < Poly->NumVertices ; vertex++ )
										vertexedit_Click( Viewport, (ABrush*)Actor, Poly->Vertex[vertex], 1, 1 );

							}
						}
					}
				}
			}
		}
		else if( Mode==EM_ActorSnapScale )
		{
			GbIsSnapScaleBox = 1;

			// Grab an initial bounding box for all selected actors.
			FBox BBox(1);
			for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
			{
				AActor* Actor = Level->Actors(i);
				if( Actor && Actor->bSelected )
					if( Actor->IsBrush() )
						BBox += ((ABrush*)Actor)->Brush->GetRenderBoundingBox( (ABrush*)Actor ).TransformBy(Actor->LocalToWorld());
					else
					{
						if( Actor->StaticMesh )
							BBox += Actor->StaticMesh->GetRenderBoundingBox( Actor ).TransformBy(Actor->LocalToWorld());
						if( Actor->Mesh )
							BBox += Actor->Mesh->GetRenderBoundingBox( Actor ).TransformBy(Actor->LocalToWorld());
					}
			}

			GOldSnapScaleStart = GSnapScaleStart = GSnapScaleStartSnapped = BBox.Min;
			GOldSnapScaleEnd = GSnapScaleEnd = GSnapScaleEndSnapped = BBox.Max;
		}
		else if(Mode == EM_TexturePan || Mode == EM_TextureRotate)
		{
			UModel*	Model = Viewport->Actor->GetLevel()->Model;

			// Guarantee that texture points and vectors for selected surfaces
			// are unique.

			OriginalUVectors.Empty(Model->Surfs.Num());
			OriginalVVectors.Empty(Model->Surfs.Num());

			for(INT SurfaceIndex = 0;SurfaceIndex < Model->Surfs.Num();SurfaceIndex++)
			{
				FBspSurf&	Surf = Model->Surfs(SurfaceIndex);

				OriginalUVectors.AddItem(Surf.vTextureU);
				OriginalVVectors.AddItem(Surf.vTextureV);

				if(Surf.PolyFlags & PF_Selected)
				{
					FVector	Base = Model->Points(Surf.pBase),
							TextureU = Model->Vectors(Surf.vTextureU),
							TextureV = Model->Vectors(Surf.vTextureV);

					Surf.pBase = Model->Points.AddItem(Base);
					Surf.vTextureU = Model->Vectors.AddItem(TextureU);
					Surf.vTextureV = Model->Vectors.AddItem(TextureV);
				}
			}

			TextureAngle = 0.0;
		}

		if( Viewport->IsOrtho()
				&& (Viewport->Input->KeyDown(IK_Alt)
				&& Viewport->Input->KeyDown(IK_Ctrl) ) ) 
		{
			// Start box selection
			GbIsBoxSel = 1;

			if( Viewport->IsOrtho() )
				GBoxSelStart = GBoxSelEnd = GUnrealEd->ClickLocation;

		}
		if( Viewport->IsOrtho()
				&& (Viewport->Input->KeyDown(IK_Shift)
				&& Viewport->Input->KeyDown(IK_MiddleMouse) ) ) 
		{
			// Start measuring
			GbIsMeasuring = 1;
			GMeasureStart = GUnrealEd->ClickLocation;
			Constraints.Snap( GMeasureStart, GGridBase );
			GMeasureEnd = GMeasureEndSnapped = GMeasureStart;
		}
	}
	if( Buttons & MOUSE_LastRelease )
	{
		Viewport->SetMouseCapture( 0, 0 );

		if( GUnrealEd->ClickFlags & CF_MOVE_ACTOR )
		{
			UBOOL bRedraw = 0;
			for( INT x = 0 ; x < Viewport->Actor->GetLevel()->Actors.Num() ; ++x )
			{
				AActor* Actor = Viewport->Actor->GetLevel()->Actors(x);
				if( Actor && Actor->bSelected )
				{
					// If any terraininfo actors were moved, tell them to update.
					if( Cast<ATerrainInfo>(Actor) )
					{
						Cast<ATerrainInfo>(Actor)->PostEditChange();
						bRedraw = 1;
					}

					// Update the collision hash
					if( Actor->bCollideActors )
					{
						GUnrealEd->Level->Hash->RemoveActor(Actor);
						GUnrealEd->Level->Hash->AddActor(Actor);

						bRedraw = 1;
					}
				}
			}

			// Tell all scene managers to update their paths
			for( INT x = 0 ; x < Viewport->Actor->GetLevel()->Actors.Num() ; ++x )
			{
				ASceneManager* SM = Cast<ASceneManager>( Viewport->Actor->GetLevel()->Actors(x) );
				if( SM )
				{
					SM->PreparePath();
					bRedraw = 1;
				}
			}

			if( bRedraw )
				GUnrealEd->RedrawLevel( GUnrealEd->Level );
		}

		FinishAllSnaps( Viewport->Actor->GetLevel() );

		if( Mode==EM_TerrainEdit )
		{
			GTerrainTools.CurrentBrush->MouseButtonUp( Viewport );
			//Trans->End();
		}
		else if( Mode==EM_VertexEdit || Mode==EM_FaceDrag )
		{
			TArray<ABrush*> Brushes;
			vertexedit_GetBrushList( &Brushes );

			// Do clean up work on the final list of brushes.
			for( INT brush = 0 ; brush < Brushes.Num() ; brush++ )
			{
				UPolys* Polys = Brushes(brush)->Brush->Polys;

				for( INT x = 0 ; x < Polys->Element.Num() ; x++ ) 
				{
					if( Polys->Element(x).Fix() < 3 )
					{
						// This poly is no longer valid, remove it from the brush.
						debugf( TEXT("Warning : Not enough vertices, poly removed"));
						Polys->Element.Remove(x);
						x = 0;
					}
				}
				Brushes(brush)->Brush->BuildBound();
				edactApplyTransformToBrush( Brushes(brush) );
			}

			GUnrealEd->Trans->End();
		}
		else if(Mode == EM_TexturePan || Mode == EM_TextureRotate)
		{
			if(OriginalUVectors.Num() && OriginalVVectors.Num())
			{
				// Finishing up texture manipulation.  Go through and minimize the set of
				// vectors we've been adjusting by merging the new vectors in and eliminating
				// duplicates.

				UModel*		Model = Viewport->Actor->GetLevel()->Model;
				FMemMark	Mark(GMem);

				for(INT SurfaceIndex = 0;SurfaceIndex < Model->Surfs.Num();SurfaceIndex++)
				{
					FBspSurf&	Surf = Model->Surfs(SurfaceIndex);

					if(Surf.PolyFlags & PF_Selected)
					{
						// Update texture coordinates in FPoly.

						polyUpdateMaster(Model,SurfaceIndex,1);

						// Eliminate references to duplicate points/vectors.

						Surf.pBase = bspAddPoint(Model,&Model->Points(Surf.pBase),0);
						Surf.vTextureU = bspAddVector(Model,&Model->Vectors(Surf.vTextureU),0);
						Surf.vTextureV = bspAddVector(Model,&Model->Vectors(Surf.vTextureV),0);
					}
				}

				Model->ClearRenderData(GRenDev);

				Mark.Pop();

				OriginalUVectors.Empty();
				OriginalVVectors.Empty();
			}
		}
		else if( Mode==EM_ActorSnapScale )
		{
			Trans->Begin( TEXT("Actor Snap Scale") );

			GbIsSnapScaleBox = 0;

			FVector Dist = GSnapScaleStartSnapped - GSnapScaleEndSnapped,
				DistOld = GOldSnapScaleStart - GOldSnapScaleEnd;

			if( !Dist.X ) Dist.X = 1;
			if( !Dist.Y ) Dist.Y = 1;
			if( !Dist.Z ) Dist.Z = 1;
			if( !DistOld.X ) DistOld.X = Dist.X;
			if( !DistOld.Y ) DistOld.Y = Dist.Y;
			if( !DistOld.Z ) DistOld.Z = Dist.Z;

			FVector Scale = FVector( Dist.X / DistOld.X, Dist.Y / DistOld.Y, Dist.Z / DistOld.Z ),
				InvScale( 1 / Scale.X, 1 / Scale.Y, 1 / Scale.Z );

			for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
			{
				if( Level->Actors(i) && Level->Actors(i)->bSelected )
				{
					ABrush* Brush = Cast<ABrush>(Level->Actors(i));
					if( Brush && Brush->IsBrush() )
					{
						Brush->Brush->Modify();
						FCoords Coords = Brush->ToLocal();
						FVector Adjust = GSnappedLocation.TransformPointBy( Coords );

						for( INT poly = 0 ; poly < Brush->Brush->Polys->Element.Num() ; poly++ )
						{
							FPoly* Poly = &(Brush->Brush->Polys->Element(poly));
							Brush->Brush->Polys->Element.ModifyAllItems();

							Poly->TextureU *= InvScale;
							Poly->TextureV *= InvScale;
							Poly->Base = ((Poly->Base - Adjust) * Scale) + Adjust;

							for( INT vtx = 0 ; vtx < Poly->NumVertices ; vtx++ )
								Poly->Vertex[vtx] = ((Poly->Vertex[vtx] - Adjust) * Scale) + Adjust;

							Poly->CalcNormal();
						}

						Brush->Brush->BuildBound();
					}
					else
						Level->Actors(i)->DrawScale3D *= Scale;
				}
			}

			Trans->End();
			EdCallback( EDC_RedrawAllViewports, 0, 0 );
		}

		if( GbIsBoxSel )
		{
			GbIsBoxSel = 0;
			GUnrealEd->edactBoxSelect( Viewport, GUnrealEd->Level, GBoxSelStart, GBoxSelEnd );
			EdCallback( EDC_RedrawCurrentViewport, 0, 0 );
		}
		if( GbIsMeasuring )
		{
			GbIsMeasuring = 0;
			EdCallback( EDC_RedrawCurrentViewport, 0, 0 );
		}

		GbARG = 0;
		GARGAxis = -1;
	}

	switch( Mode )
	{
		case EM_None:
			debugf( NAME_Warning, TEXT("Editor is disabled") );
			break;
		case EM_BrushClip:
		case EM_Polygon:
		case EM_Geometry:
			goto ViewportMove;
		case EM_VertexEdit:
		case EM_FaceDrag:
			{
				if( !Viewport->Input->KeyDown(IK_Ctrl) 
						|| GbIsBoxSel
						|| GbIsMeasuring
						|| GbARG)
					goto ViewportMove;

				CalcFreeMoveRot( Viewport, DX, DY, Buttons, Delta, DeltaRot );
				MoveActors( Viewport, Level, Delta, DeltaRot, 1, (Buttons & MOUSE_Shift) ? Viewport->Actor : NULL, 1 );

				// If we're using the sizingbox, the brush bounding boxes need to be constantly
				// updated so the size will be shown properly while dragging vertices.
				if( GUnrealEd->UseSizingBox )
				{
					TArray<ABrush*> Brushes;
					vertexedit_GetBrushList( &Brushes );
					for( INT brush = 0 ; brush < Brushes.Num() ; brush++ )
						Brushes(brush)->Brush->BuildBound();
				}
			}
			break;
		case EM_ViewportMove:
		case EM_Matinee:
			if( Buttons & MOUSE_Alt )
			{
				GrabVertex( Viewport->Actor->GetLevel() );
			}
			// release the vertex if either the mouse button or <Alt> key is released
			else if( Buttons & MOUSE_LastRelease || !( Buttons & MOUSE_Alt ) )
			{
				ReleaseVertex( Viewport->Actor->GetLevel() );
			}
		case EM_ViewportZoom:
			ViewportMove:
			if( GbIsBoxSel || GbIsMeasuring )
			{
				CalcFreeMoveRot( Viewport, DX, DY, Buttons, Delta, DeltaRot );
				MoveActors( Viewport, Level, Delta, DeltaRot, 0, NULL );
			}
			else if( Buttons & (MOUSE_FirstHit | MOUSE_LastRelease | MOUSE_SetMode | MOUSE_ExitMode) )
			{
				Viewport->Actor->Velocity = FVector(0,0,0);
			}
			else
			{
				if( Buttons & MOUSE_Alt )
				{
					if( !GbIsBoxSel )
					{
						// Move selected vertex.
						CalcFreeMoveRot( Viewport, DX, DY, Buttons, Delta, DeltaRot );
						Delta *= 0.25f*MovementSpeed;
					}
				}
				else
					if( !(Buttons & (MOUSE_Ctrl | MOUSE_Shift) ) )
					{
						// Move camera.
						Speed = 0.30*MovementSpeed;
						if( Viewport->IsOrtho() && Buttons==MOUSE_Right )
						{
							Buttons = MOUSE_Left;
							Speed   = 0.60*MovementSpeed;
						}
						CalcFreeMoveRot( Viewport, DX, DY, Buttons, Delta, DeltaRot );
						Delta *= Speed;
					}
					else
					{
						// Move actors.
						CalcMixedMoveRot( Viewport, DX, DY, Buttons, Delta, DeltaRot );
						Delta *= 0.25*MovementSpeed;
					}
				if( Mode==EM_ViewportZoom )
				{
					Delta = (Viewport->Actor->Velocity += Delta);
				}
				if( Buttons & MOUSE_Alt && !(Buttons & MOUSE_Right) )
				{
					if( !GbIsBoxSel )
					{
						// Move selected vertex.
						MoveVertex( Level, Delta, 1 );
					}
				}
				else if( !(Buttons & (MOUSE_Ctrl | MOUSE_Shift) ) )
				{
					// Move camera.
					ViewportMoveRotWithPhysics( Viewport, Delta, DeltaRot );
				}
				else
				{
					// Move actors.
					MoveActors( Viewport, Level, Delta, DeltaRot, 1, (Buttons & MOUSE_Shift) ? Viewport->Actor : NULL );
				}
			}
			break;
		case EM_ActorRotate:
			if( !(Buttons&MOUSE_Ctrl) )
				goto ViewportMove;
			CalcAxialRot( Viewport, DX, DY, Buttons, DeltaRot );
			if( DeltaRot != FRotator(0,0,0) )
			{
				NoteActorMovement( Level );
 				MoveActors( Viewport, Level, FVector(0,0,0), DeltaRot, 1, (Buttons & MOUSE_Shift) ? Viewport->Actor : NULL );
			}
			break;
		case EM_ActorScale:
			{
				if (!(Buttons&MOUSE_Ctrl))
					goto ViewportMove;

				NoteActorMovement( Level );
				CalcAxialMoveRot( Viewport, DX, DY, Buttons, Delta, DeltaRot );
				if( Delta.IsZero() )
					break;

				FVector Scale( 1 + Delta.X / 256.0f, 1 + Delta.Y / 256.0f, 1 + Delta.Z / 256.0f );
				for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
				{
					AActor* Actor = Level->Actors(i);
					if( Actor && Actor->bSelected && Actor->IsBrush() )
					{
						for( INT poly = 0 ; poly < Actor->Brush->Polys->Element.Num() ; poly++ )
						{
							FPoly* Poly = &(Actor->Brush->Polys->Element(poly));

							for( INT vertex = 0 ; vertex < Poly->NumVertices ; vertex++ )
								Poly->Vertex[vertex] *= Scale;

							Poly->Finalize(0);
						}

						Actor->Brush->BuildBound();
					
						// If the user is hold down ALT, scale the locations as well.  This allows you to
						// scale a group of brushes as one.
						if( Buttons & MOUSE_Alt )
							Actor->Location *= Scale;
					}
				}
			}
			break;
		case EM_ActorSnapScale:
			if( !(Buttons&MOUSE_Ctrl) )
				goto ViewportMove;
			{
				NoteActorMovement( Level );

				CalcAxialMoveRot( Viewport, DX, DY, Buttons, Delta, DeltaRot );

				if( Delta.X )
				{
					FPlane Plane( GSnappedLocation, GSnappedLocation + (FVector(0,1,0) * 16), GSnappedLocation + (FVector(0,0,1) * 16) );
					FLOAT StartDist = FPointPlaneDist( GSnapScaleStart, GSnappedLocation, FVector(1,0,0) );
					FLOAT EndDist = FPointPlaneDist( GSnapScaleEnd, GSnappedLocation, FVector(1,0,0) );

					if( ::fabs(StartDist) > THRESH_POINT_ON_PLANE ) GSnapScaleStart.X -= Delta.X;
					if( ::fabs(EndDist) > THRESH_POINT_ON_PLANE ) GSnapScaleEnd.X += Delta.X;
				}
				if( Delta.Y )
				{
					FPlane Plane( GSnappedLocation, GSnappedLocation + (FVector(1,0,0) * 16), GSnappedLocation + (FVector(0,0,1) * 16) );
					FLOAT StartDist = FPointPlaneDist( GSnapScaleStart, GSnappedLocation, FVector(0,1,0) );
					FLOAT EndDist = FPointPlaneDist( GSnapScaleEnd, GSnappedLocation, FVector(0,1,0) );

					if( ::fabs(StartDist) > THRESH_POINT_ON_PLANE ) GSnapScaleStart.Y -= Delta.Y;
					if( ::fabs(EndDist) > THRESH_POINT_ON_PLANE ) GSnapScaleEnd.Y += Delta.Y;
				}
				if( Delta.Z )
				{
					FPlane Plane( GSnappedLocation, GSnappedLocation + (FVector(1,0,0) * 16), GSnappedLocation + (FVector(0,1,0) * 16) );
					FLOAT StartDist = FPointPlaneDist( GSnapScaleStart, GSnappedLocation, FVector(0,0,1) );
					FLOAT EndDist = FPointPlaneDist( GSnapScaleEnd, GSnappedLocation, FVector(0,0,1) );

					if( ::fabs(StartDist) > THRESH_POINT_ON_PLANE ) GSnapScaleStart.Z -= Delta.Z;
					if( ::fabs(EndDist) > THRESH_POINT_ON_PLANE ) GSnapScaleEnd.Z += Delta.Z;
				}

				GSnapScaleStartSnapped = GSnapScaleStart;
				Constraints.Snap(GSnapScaleStartSnapped,FVector(0,0,0));
				GSnapScaleEndSnapped = GSnapScaleEnd;
				Constraints.Snap(GSnapScaleEndSnapped,FVector(0,0,0));
			}
			break;
		case EM_TexturePan:
		{
			if( !(Buttons&MOUSE_Ctrl) )
				goto ViewportMove;
			NoteTextureMovement( Level );
			if( (Buttons & MOUSE_Left) && (Buttons & MOUSE_Right) )
			{
				check(OriginalUVectors.Num()==Viewport->Actor->GetLevel()->Model->Surfs.Num());
				check(OriginalVVectors.Num()==Viewport->Actor->GetLevel()->Model->Surfs.Num());

				GFixScale += Fix(DY) / 32;
				TempFloat = 1.0;
				INT Temp = Unfix(GFixScale); 
				if( Constraints.GridEnabled )
				{
					while( Temp > 0 ) { TempFloat *= 0.5; Temp--; }
					while( Temp < 0 ) { TempFloat *= 2.0; Temp++; }
				}
				else
				{
					while( Temp > 0 ) { TempFloat *= 0.95f; Temp--; }
					while( Temp < 0 ) { TempFloat *= 1.05f; Temp++; }
				}
				if( TempFloat != 1.0 )
					polyTexScale(Viewport->Actor->GetLevel()->Model,TempFloat,0.0,0.0,TempFloat,0);
				GFixScale &= 0xffff;
			}
			else if( Buttons & MOUSE_Left )
			{
				FLOAT Mod = 1;
				if( Buttons & MOUSE_Shift )
					Mod = .05f;

				GFixPanU += Fix(DX)/(16.f*Mod);  GFixPanV += Fix(DY)/(16.f*Mod);
				polyTexPan(Viewport->Actor->GetLevel()->Model,Unfix(GFixPanU),Unfix(0),0);
				GFixPanU &= 0xffff; GFixPanV &= 0xffff;
			}
			else
			{
				FLOAT Mod = 1;
				if( Buttons & MOUSE_Shift )
					Mod = .05f;

				GFixPanU += Fix(DX)/(16.f*Mod);  GFixPanV += Fix(DY)/(16.f*Mod);
				polyTexPan(Viewport->Actor->GetLevel()->Model,Unfix(0),Unfix(GFixPanV),0);
				GFixPanU &= 0xffff; GFixPanV &= 0xffff;
			}
			break;
		}
		case EM_TextureRotate:
		{
			if( !(Buttons&MOUSE_Ctrl) )
				goto ViewportMove;
			check(OriginalUVectors.Num()==Viewport->Actor->GetLevel()->Model->Surfs.Num());
			check(OriginalVVectors.Num()==Viewport->Actor->GetLevel()->Model->Surfs.Num());
			NoteTextureMovement( Level );
			TextureAngle += (FLOAT)DX / ( Constraints.RotGridEnabled ? 256.0 : 8192.0 );
			for( INT i=0; i<Viewport->Actor->GetLevel()->Model->Surfs.Num(); i++ )
			{
				FBspSurf* Surf = &Viewport->Actor->GetLevel()->Model->Surfs(i);
				if( Surf->PolyFlags & PF_Selected )
				{
					FVector U		=  Viewport->Actor->GetLevel()->Model->Vectors(OriginalUVectors(i));
					FVector V		=  Viewport->Actor->GetLevel()->Model->Vectors(OriginalVVectors(i));
					FVector* NewU	= &Viewport->Actor->GetLevel()->Model->Vectors(Surf->vTextureU);
					FVector* NewV	= &Viewport->Actor->GetLevel()->Model->Vectors(Surf->vTextureV);
					*NewU			= U * appCos(TextureAngle) + V * appSin(TextureAngle);
					*NewV			= V * appCos(TextureAngle) - U * appSin(TextureAngle);

					polyUpdateMaster(Viewport->Actor->GetLevel()->Model,i,1);
				}
			}
			Viewport->Actor->GetLevel()->Model->ClearRenderData(GRenDev);
			break;
		}
		case EM_TerrainEdit:
		{
			if (!(Buttons&MOUSE_Ctrl))
				goto ViewportMove;

			ATerrainInfo* TerrainInfo = GTerrainTools.GetCurrentTerrainInfo();
			if( TerrainInfo )
			{
				if( Buttons == (MOUSE_Ctrl|MOUSE_Left|MOUSE_Right) && GTerrainTools.CurrentBrush->ID == TB_VertexEdit )
				{
					TerrainInfo->MoveVertices( DY );
				}
				else if( Buttons == (MOUSE_Ctrl|MOUSE_Left) || Buttons == (MOUSE_Ctrl|MOUSE_Right) )
				{
					switch( GTerrainTools.CurrentBrush->ID )
					{
						case TB_TexturePan:
							GTerrainTools.CurrentBrush->MouseMove(
								Buttons == (MOUSE_Left|MOUSE_Ctrl)  ? DX/1000.f : 0,
								Buttons == (MOUSE_Right|MOUSE_Ctrl) ? DY/1000.f : 0 );
							break;

						case TB_TextureRotate:
							GTerrainTools.CurrentBrush->MouseMove(
								0,
								DX*10.f );
							break;

						case TB_TextureScale:
							GTerrainTools.CurrentBrush->MouseMove(
								Buttons == (MOUSE_Left|MOUSE_Ctrl)  ? DX/100.f : 0,
								Buttons == (MOUSE_Right|MOUSE_Ctrl) ? -DY/100.f : 0 );
							break;

						case TB_Select:
							GTerrainTools.CurrentBrush->MouseMove(
								Buttons == (MOUSE_Left|MOUSE_Ctrl)  ? DX/100.f : 0,
								Buttons == (MOUSE_Right|MOUSE_Ctrl) ? -DY/100.f : 0 );
							break;

						default:
							TerrainInfo->SelectedVertices.Empty();
							TerrainInfo->SelectVertex( Viewport->TerrainPointAtLocation );
							TerrainInfo->SoftSelect( GTerrainTools.GetInnerRadius(), GTerrainTools.GetOuterRadius() );
							GTerrainTools.CurrentBrush->Execute( Buttons == (MOUSE_Left|MOUSE_Ctrl) );
							break;
					}
				}
			}
			break;
		}
		case EM_EyeDropper:
			goto ViewportMove;
			break;
		case EM_FindActor:
			goto ViewportMove;
			break;
		default:
			debugf( NAME_Warning, TEXT("Unknown editor mode %i"), Mode );
			goto ViewportMove;
			break;
	}

	unguardf(( TEXT("(Mode=%i)"), Mode ));
}

void ComputeTerrainLookAtLocation( UViewport* InViewport, INT X, INT Y )
{
	guard(ComputeTerrainLookAtLocation);

	// Figure out what location the mouse is pointing at on the terrain and save it.
	FCameraSceneNode	SceneNode(InViewport,&InViewport->RenderTarget,InViewport->Actor,InViewport->Actor->Location,InViewport->Actor->Rotation,InViewport->Actor->FovAngle);
	FCanvasUtil			CanvasUtil(&InViewport->RenderTarget,InViewport->RI);

	ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
	FVector Direction, Origin;
	if( TI )
	{
		if( InViewport->IsOrtho() )
		{
			Origin = SceneNode.Deproject(CanvasUtil.CanvasToScreen.TransformFPlane(FPlane(X,Y,0,1)));
			switch( InViewport->Actor->RendMap )
			{
				case REN_OrthXY:
					Origin.Z = HALF_WORLD_MAX;
					Direction = FVector(0,0,-1);
					break;

				case REN_OrthXZ:
					Origin.Y = HALF_WORLD_MAX;
					Direction = FVector(0,-1,0);
					break;

				case REN_OrthYZ:
					Origin.X = -HALF_WORLD_MAX;
					Direction = FVector(1,0,0);
					break;
			}
		}
		else
		{
			FPlane	P(CanvasUtil.CanvasToScreen.TransformFVector(FVector(X,Y,0)),NEAR_CLIPPING_PLANE);

			Origin = SceneNode.ViewOrigin;
			Direction = SceneNode.Deproject(P) - Origin;
		}

		FCheckResult Check;
		if( TI->LineCheck( Check, Origin + Direction*WORLD_MAX, Origin, FVector(0,0,0), 0, !GTerrainTools.bIgnoreInvisibleQuads )==0 )
		{
			TI->GetClosestVertex( Check.Location, &(GTerrainTools.PointingAt), NULL, NULL );
			InViewport->TerrainPointAtLocation = Check.Location;
			GEditor->ClickPlane = Check.Normal;
		}
	}

	unguard;
}

//
// Mouse position.
//
void UUnrealEdEngine::MousePosition( UViewport* Viewport, DWORD Buttons, FLOAT X, FLOAT Y )
{
	guard(UUnrealEdEngine::MousePosition);

	switch( edcamMode(Viewport) )
	{
		case EM_TexView:
		{
			FTexPropWindowInfo* Info = GMaterialTools->GetInfo( Viewport->Actor->Misc2 );

			if( Info && Info->Material )
			{
				UTexture* Texture = Cast<UTexture>(Info->Material);
				if( Texture )
				{
					X *= (FLOAT)Texture->MaterialUSize()/Viewport->SizeX;
					Y *= (FLOAT)Texture->MaterialVSize()/Viewport->SizeY;
					if( X>=0 && X<Texture->MaterialUSize() && Y>=0 && Y<Texture->MaterialVSize() )
						Texture->MousePosition( Buttons, X, Y );
				}
			}
			break;
		}
		break;

		default:
		{
			if( Viewport->IsOrtho() || Viewport->IsPerspective() )
			{
				ComputeTerrainLookAtLocation( Viewport, X, Y );
				if( Mode == EM_TerrainEdit )
					Viewport->Repaint(1);
			}
			break;
		}
	}

	unguard;
}

/*-----------------------------------------------------------------------------
   Keypress handling.
-----------------------------------------------------------------------------*/

//
// Handle a regular ASCII key that was pressed in UnrealEd.
// Returns 1 if proceesed, 0 if not.
//
INT UUnrealEdEngine::Key( UViewport* Viewport, EInputKey Key, TCHAR Unicode )
{
	guard(UUnrealEdEngine::Key);
	if( UEngine::Key( Viewport, Key, Unicode ) )
	{
		return 1;
	}
	else if( Viewport->Actor->RendMap==REN_TexBrowser || Viewport->Actor->RendMap==REN_TexBrowserUsed || Viewport->Actor->RendMap==REN_TexBrowserMRU )
	{
		return 0;
	}
	else if( Viewport->Actor->RendMap==REN_StaticMeshBrowser )
	{
		return 0;
	}
	else if( Viewport->Actor->RendMap==REN_TexView )
	{
		return 0;
	}
	else if( Viewport->Actor->RendMap==REN_MaterialEditor )
	{
		return 0;
	}
	else if( Viewport->Actor->RendMap==REN_MeshView )
	{
		return 0;
	}
	else if( Viewport->Actor->RendMap==REN_Animation )
	{
		return 0;
	}

#ifdef WITH_LIPSINC
	else if( Viewport->Actor->RendMap==REN_LIPSinc )
	{
		return 0;
	}
#endif

	else if( Viewport->Actor->RendMap==REN_Prefab || Viewport->Actor->RendMap == REN_PrefabCompiled )
	{
		return 0;
	}
	else if( Viewport->Actor->RendMap==REN_TerrainHeightmap )
	{
		return 0;
	}
	else if( Viewport->Actor->RendMap==REN_MatineePreview )
	{
		return 0;
	}
	else if( Viewport->Actor->RendMap==REN_MatineeScenes )
	{
		return 0;
	}
	else if( Viewport->Actor->RendMap==REN_MatineeActions )
	{
		return 0;
	}
	else if( Viewport->Actor->RendMap==REN_MatineeSubActions )
	{
		return 0;
	}
	else if( Viewport->Actor->RendMap==REN_TerrainLayers )
	{
		return 0;
	}
	else if( Viewport->Actor->RendMap==REN_TerrainDecoLayers )
	{
		return 0;
	}
	else if( Viewport->Input->KeyDown(IK_Alt) )
	{
		FString Cmd;
		switch( Key )
		{
			case IK_1:	Cmd = TEXT("RMODE 1");	break;
			case IK_2:	Cmd = TEXT("RMODE 2");	break;
			case IK_3:	Cmd = TEXT("RMODE 3");	break;
			case IK_4:	Cmd = TEXT("RMODE 4");	break;
			case IK_5:	Cmd = TEXT("RMODE 5");	break;
			case IK_6:	Cmd = TEXT("RMODE 6");	break;
			case IK_7:	Cmd = TEXT("RMODE 13");	break;
			case IK_8:	Cmd = TEXT("RMODE 14");	break;
			case IK_9:	Cmd = TEXT("RMODE 15");	break;
			case IK_0:	Cmd = TEXT("RMODE 7");	break;
			default:	return 0;
		}

		INT iRet = Viewport->Exec( *Cmd );
		EdCallback( EDC_ViewportUpdateWindowFrame, 1, 0 );
		return iRet;
	}
	else if( Viewport->Input->KeyDown(IK_Shift) )
	{
		if( Viewport->Input->KeyDown(IK_A) ) {  Exec( TEXT("ACTOR SELECT ALL") ); return 1; }
		if( Viewport->Input->KeyDown(IK_B) ) {  Exec( TEXT("POLY SELECT MATCHING BRUSH") ); return 1; }
		if( Viewport->Input->KeyDown(IK_C) ) {  Exec( TEXT("POLY SELECT ADJACENT COPLANARS") ); return 1; }
		if( Viewport->Input->KeyDown(IK_D) ) {  Exec( TEXT("ACTOR DUPLICATE") ); return 1; }
		if( Viewport->Input->KeyDown(IK_F) ) {  Exec( TEXT("POLY SELECT ADJACENT FLOORS") ); return 1; }
		if( Viewport->Input->KeyDown(IK_G) ) {  Exec( TEXT("POLY SELECT MATCHING GROUPS") ); return 1; }
		if( Viewport->Input->KeyDown(IK_I) ) {  Exec( TEXT("POLY SELECT MATCHING ITEMS") ); return 1; }
		if( Viewport->Input->KeyDown(IK_J) ) {  Exec( TEXT("POLY SELECT ADJACENT ALL") ); return 1; }
		if( Viewport->Input->KeyDown(IK_M) ) {  Exec( TEXT("POLY SELECT MEMORY SET") ); return 1; }
		if( Viewport->Input->KeyDown(IK_N) ) {  Exec( TEXT("SELECT NONE") ); return 1; }
		if( Viewport->Input->KeyDown(IK_O) ) {  Exec( TEXT("POLY SELECT MEMORY INTERSECT") ); return 1; }
		if( Viewport->Input->KeyDown(IK_Q) ) {  Exec( TEXT("POLY SELECT REVERSE") ); return 1; }
		if( Viewport->Input->KeyDown(IK_R) ) {  Exec( TEXT("POLY SELECT MEMORY RECALL") ); return 1; }
		if( Viewport->Input->KeyDown(IK_S) ) {  Exec( TEXT("POLY SELECT ALL") ); return 1; }
		if( Viewport->Input->KeyDown(IK_T) ) {  Exec( TEXT("POLY SELECT MATCHING TEXTURE") ); return 1; }
		if( Viewport->Input->KeyDown(IK_U) ) {  Exec( TEXT("POLY SELECT MEMORY UNION") ); return 1; }
		if( Viewport->Input->KeyDown(IK_W) ) {  Exec( TEXT("POLY SELECT ADJACENT WALLS") ); return 1; }
		if( Viewport->Input->KeyDown(IK_Y) ) {  Exec( TEXT("POLY SELECT ADJACENT SLANTS") ); return 1; }
		if( Viewport->Input->KeyDown(IK_X) ) {  Exec( TEXT("POLY SELECT MEMORY XOR") ); return 1; }

		return 0;
	}
	else if( Viewport->Input->KeyDown(IK_Ctrl) )
	{
		if( Viewport->Input->KeyDown(IK_C) ) { Exec( TEXT("EDIT COPY") );	return 1; }
		if( Viewport->Input->KeyDown(IK_V) ) { Exec( TEXT("EDIT PASTE") );	return 1; }
		if( Viewport->Input->KeyDown(IK_W) ) { Exec( TEXT("ACTOR DUPLICATE") );	return 1; }
		if( Viewport->Input->KeyDown(IK_X) ) { Exec( TEXT("EDIT CUT") );	return 1; }
		if( Viewport->Input->KeyDown(IK_Y) ) { Exec( TEXT("TRANSACTION REDO") );	return 1; }
		if( Viewport->Input->KeyDown(IK_Z) ) { Exec( TEXT("TRANSACTION UNDO") );	return 1; }
		if( Viewport->Input->KeyDown(IK_A) ) { Exec( TEXT("BRUSH ADD") );	return 1; }
		if( Viewport->Input->KeyDown(IK_S) ) { Exec( TEXT("BRUSH SUBTRACT") );	return 1; }
		if( Viewport->Input->KeyDown(IK_I) ) { Exec( TEXT("BRUSH FROM INTERSECTION") );	return 1; }
		if( Viewport->Input->KeyDown(IK_D) ) { Exec( TEXT("BRUSH FROM DEINTERSECTION") );	return 1; }
		if( Viewport->Input->KeyDown(IK_L) ) { GUnrealEd->EdCallback( EDC_SaveMap, 1, 0 );	return 1; }
		if( Viewport->Input->KeyDown(IK_E) ) { GUnrealEd->EdCallback( EDC_SaveMapAs, 1, 0 );	return 1; }
		if( Viewport->Input->KeyDown(IK_O) ) { GUnrealEd->EdCallback( EDC_LoadMap, 1, 0 );	return 1; }
		if( Viewport->Input->KeyDown(IK_P) ) { GUnrealEd->EdCallback( EDC_PlayMap, 1, 0 );	return 1; }

		return 0;
	}
	else if( !Viewport->Input->KeyDown(IK_Alt) )
	{
		if( Viewport->Input->KeyDown(IK_Delete) ) { Exec( TEXT("ACTOR DELETE") );	return 1; }
		if( Viewport->Input->KeyDown(IK_B) ) {  Viewport->Actor->ShowFlags ^= SHOW_Brush; return 1; }
		if( Viewport->Input->KeyDown(IK_H) ) {  Viewport->Actor->ShowFlags ^= SHOW_Actors; return 1; }
		if( Viewport->Input->KeyDown(IK_K) ) {  Viewport->Actor->ShowFlags ^= SHOW_Backdrop; return 1; }
		if( Viewport->Input->KeyDown(IK_P) )
		{  
			Viewport->Actor->ShowFlags ^= SHOW_PlayerCtrl;
			Exec( TEXT("AUDIO FINDVIEWPORT") );
			EdCallback( EDC_ViewportUpdateWindowFrame, 1, 0 ); 
			return 1; 
		}
		if( Viewport->Input->KeyDown(IK_W) ) {  Viewport->Actor->ShowFlags ^= SHOW_StaticMeshes; return 1; }
		if( Viewport->Input->KeyDown(IK_E) ) {  Viewport->Actor->ShowFlags ^= SHOW_EventLines; return 1; }
		if( Viewport->Input->KeyDown(IK_I) ) {  Viewport->Actor->ShowFlags ^= SHOW_ActorInfo; return 1; }
		if( Viewport->Input->KeyDown(IK_S) ) {  Viewport->Actor->ShowFlags ^= SHOW_SelectionHighlight; return 1; }
		if( Viewport->Input->KeyDown(IK_T) ) {  Viewport->Actor->ShowFlags ^= SHOW_Terrain; return 1; }
		if( Viewport->Input->KeyDown(IK_F) ) {  Viewport->Actor->ShowFlags ^= SHOW_DistanceFog; return 1; }
		if( Viewport->Input->KeyDown(IK_O) ) {  Viewport->Actor->ShowFlags ^= SHOW_Volumes; return 1; }
		if( Viewport->Input->KeyDown(IK_Q) ) {	Viewport->Actor->ShowFlags ^= SHOW_BSP; return 1; }
		if( Viewport->Input->KeyDown(IK_LeftBracket) )
		{
			Exec( *FString::Printf( TEXT("TERRAIN TOOLRADIUS INNER=%d OUTER=%d"), GTerrainTools.GetInnerRadius()-256, GTerrainTools.GetOuterRadius()-256 ) );
			return 1;
		}
		if( Viewport->Input->KeyDown(IK_RightBracket) )
		{
			Exec( *FString::Printf( TEXT("TERRAIN TOOLRADIUS INNER=%d OUTER=%d"), GTerrainTools.GetInnerRadius()+256, GTerrainTools.GetOuterRadius()+256 ) );
			return 1;
		}

		return 0;
	}

	return 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	Common drawing functions
-----------------------------------------------------------------------------*/

void UUnrealEdEngine::edDrawAxisIndicator(FSceneNode* SceneNode)
{
	guard(UUnrealEdEngine::DrawAxisIndicator);

	FCanvasUtil		CanvasUtil(&SceneNode->Viewport->RenderTarget,SceneNode->Viewport->RI);
	FLOAT			SizeX = SceneNode->Viewport->SizeX,
					SizeY = SceneNode->Viewport->SizeY;

	if( SceneNode->Viewport->IsOrtho() )
	{
		FLineBatcher	LineBatcher(SceneNode->Viewport->RI);
		UCanvas*		Canvas = SceneNode->Viewport->Canvas;
	    INT				XL,
					    YL;
    
	    SceneNode->Viewport->RI->SetTransform(TT_WorldToCamera,SceneNode->WorldToCamera);
	    SceneNode->Viewport->RI->SetTransform(TT_CameraToScreen,SceneNode->CameraToScreen);
    
	    Canvas->WrappedStrLenf( Canvas->SmallFont, XL, YL, TEXT("M") );

		FVector	Origin = SceneNode->Deproject(CanvasUtil.CanvasToScreen.TransformFPlane(FPlane(16.0f,SizeY - 16,0.0f,1.0f))),
				XAxis = SceneNode->Deproject(CanvasUtil.CanvasToScreen.TransformFPlane(FPlane(32.0f,SizeY - 16,0.0f,1.0f))) - Origin,
				YAxis = SceneNode->Deproject(CanvasUtil.CanvasToScreen.TransformFPlane(FPlane(16.0f,SizeY - 32,0.0f,1.0f))) - Origin;

		FString AxisLabels[3];
		switch( SceneNode->Viewport->Actor->RendMap )
		{
			case REN_OrthXY:	AxisLabels[0] = TEXT("X");	AxisLabels[1] = TEXT("Y");	AxisLabels[2] = TEXT("Z");	break;
			case REN_OrthXZ:	AxisLabels[0] = TEXT("X");	AxisLabels[1] = TEXT("Z");	AxisLabels[2] = TEXT("Y");	break;
			case REN_OrthYZ:	AxisLabels[0] = TEXT("Y");	AxisLabels[1] = TEXT("Z");	AxisLabels[2] = TEXT("X");	break;
		}
		
		Canvas->Color = FColor(255,255,255);

		Canvas->SetClip( 36, SizeY-20, XL, YL );
		Canvas->WrappedPrintf( Canvas->SmallFont, 0, *AxisLabels[0] );

		Canvas->SetClip( 14, SizeY-44, XL, YL );
		Canvas->WrappedPrintf( Canvas->SmallFont, 0, *AxisLabels[1] );

		Canvas->SetClip( 8, SizeY-16, XL, YL );
		Canvas->WrappedPrintf( Canvas->SmallFont, 0, *AxisLabels[2] );

		switch( SceneNode->Viewport->Actor->RendMap )
		{
			case REN_OrthXY:
				LineBatcher.DrawLine(Origin,Origin + XAxis,FColor(255,0,0));
				LineBatcher.DrawLine(Origin,Origin + YAxis,FColor(0,255,0));
				break;

			case REN_OrthXZ:
				LineBatcher.DrawLine(Origin,Origin + XAxis,FColor(255,0,0));
				LineBatcher.DrawLine(Origin,Origin + YAxis,FColor(0,0,255));
				break;

			case REN_OrthYZ:
				LineBatcher.DrawLine(Origin,Origin + XAxis,FColor(0,255,0));
				LineBatcher.DrawLine(Origin,Origin + YAxis,FColor(0,0,255));
				break;
		}
		Canvas->SetClip( 0, 0, SizeX, SizeY );

	}
	else
	{
		FPlane	S = FPlane(CanvasUtil.CanvasToScreen.TransformFVector(FVector(40.0f,SizeY - 40,0.0f)),NEAR_CLIPPING_PLANE);
		FVector	V = SceneNode->Deproject(S) - SceneNode->ViewOrigin;
		FCoords Coords = GMath.UnitCoords;
		Coords.Origin = SceneNode->ViewOrigin + V * 12.0f;

		Draw3DAxis( SceneNode, Coords );
	}

	unguard;
}

void UUnrealEdEngine::Draw3DAxis(FSceneNode* SceneNode, FCoords& AxisCoords )
{
	guard(UUnrealEdEngine::Draw3DAxis)
	FLineBatcher	LineBatcher(SceneNode->Viewport->RI, 0);
	FCanvasUtil		CanvasUtil(&SceneNode->Viewport->RenderTarget,SceneNode->Viewport->RI);
	UCanvas*		Canvas = SceneNode->Viewport->Canvas;
	FLOAT			SizeX = SceneNode->Viewport->SizeX,
					SizeY = SceneNode->Viewport->SizeY;
	INT				XL,
					YL;

	SceneNode->Viewport->RI->SetTransform(TT_WorldToCamera,SceneNode->WorldToCamera);
	SceneNode->Viewport->RI->SetTransform(TT_CameraToScreen,SceneNode->CameraToScreen);

	Canvas->WrappedStrLenf( Canvas->SmallFont, XL, YL, TEXT("M") );

	FVector XAxis = FVector(0.5f,0.0f,0.0f).TransformVectorBy(AxisCoords) * NEAR_CLIPPING_PLANE;
	FVector YAxis = FVector(0.0f,0.5f,0.0f).TransformVectorBy(AxisCoords) * NEAR_CLIPPING_PLANE;
	FVector ZAxis = FVector(0.0f,0.0f,0.5f).TransformVectorBy(AxisCoords) * NEAR_CLIPPING_PLANE;

	LineBatcher.DrawLine(AxisCoords.Origin,AxisCoords.Origin + XAxis,FColor(255,0,0));
	LineBatcher.DrawLine(AxisCoords.Origin,AxisCoords.Origin + YAxis,FColor(0,255,0));
	LineBatcher.DrawLine(AxisCoords.Origin,AxisCoords.Origin + ZAxis,FColor(0,0,255));
	LineBatcher.Flush();

	FVector P, C;
	P = SceneNode->Project(AxisCoords.Origin + XAxis);
		C = CanvasUtil.ScreenToCanvas.TransformFVector(P);

		Canvas->SetClip(C.X,C.Y,XL,YL);
		Canvas->WrappedPrintf(Canvas->SmallFont,0,TEXT("X"));

	P = SceneNode->Project(AxisCoords.Origin + YAxis);
		C = CanvasUtil.ScreenToCanvas.TransformFVector(P);

		Canvas->SetClip(C.X,C.Y,XL,YL);
		Canvas->WrappedPrintf(Canvas->SmallFont,0,TEXT("Y"));

	P = SceneNode->Project(AxisCoords.Origin + ZAxis);
		C = CanvasUtil.ScreenToCanvas.TransformFVector(P);

		Canvas->SetClip(C.X,C.Y,XL,YL);
		Canvas->WrappedPrintf(Canvas->SmallFont,0,TEXT("Z"));

	Canvas->SetClip( 0, 0, SizeX, SizeY );
	unguard;
}


/*-----------------------------------------------------------------------------
   Texture browser routines.
-----------------------------------------------------------------------------*/

void DrawViewerBackground( UViewport* Viewport )
{
	guard(DrawViewerBackground);
	Viewport->Canvas->DrawPattern( GUnrealEd->Bkgnd, 0, 0, Viewport->SizeX, Viewport->SizeY, 1.f, 0.f, 0.f, 0.0f, FPlane(1.f,1.f,1.f,1.f), FPlane(0,0,0,0) );
	unguard;
}

void GetMaterialBorderInfo( UMaterial* InMaterial, UBOOL* InDisplayBorder, FColor* InBorderColor, INT* InBorderOffset )
{
	*InDisplayBorder = 0;
	*InBorderOffset = 0;
	*InBorderColor = FColor(255,255,255);

	if( InMaterial->IsA(UShader::StaticClass() ) )
	{
		*InDisplayBorder = 1;
		*InBorderColor = FColor(255,0,0);
	}
	if( InMaterial->IsA(UCombiner::StaticClass() ) )
	{
		*InDisplayBorder = 1;
		*InBorderColor = FColor(0,255,0);
	}
	if( InMaterial->IsA(UTexModifier::StaticClass() ) )
	{
		*InDisplayBorder = 1;
		*InBorderColor = FColor(64,64,255);
	}
	if( InMaterial->IsA(UFinalBlend::StaticClass() ) )
	{
		*InDisplayBorder = 1;
		*InBorderColor = FColor(255,255,0);
	}
    // gam ---
	if( InMaterial->IsA(UCubemap::StaticClass() ) )
	{
		*InDisplayBorder = 1;
		*InBorderColor = FColor(128,64,64);
	}
    // --- gam

	if( *InDisplayBorder )
		*InBorderOffset = 8;
}

UBOOL IsMaterialFiltered( UMaterial* InMaterial )
{
	if( InMaterial->IsA(UTexture::StaticClass() ) && !(GTBOptions->TypeFilter&MTF_Textures) )		return 1;
	if( InMaterial->IsA(UShader::StaticClass() ) && !(GTBOptions->TypeFilter&MTF_Shaders) )			return 1;
	if( InMaterial->IsA(UCombiner::StaticClass() ) && !(GTBOptions->TypeFilter&MTF_Combiners) )		return 1;
	if( InMaterial->IsA(UTexModifier::StaticClass() ) && !(GTBOptions->TypeFilter&MTF_Modifiers) )	return 1;
	if( InMaterial->IsA(UFinalBlend::StaticClass() ) && !(GTBOptions->TypeFilter&MTF_FinalBlends) )	return 1;

	FString TexName = InMaterial->GetName();
	if( !appStrstr( *(TexName.Caps()), *(GTBOptions->NameFilter.Caps()) ) )
		return 1;

	return 0;
}

int CDECL ResNameCompare(const void *A, const void *B)
{
	return appStricmp((*(UObject **)A)->GetName(),(*(UObject **)B)->GetName());
}

struct FUsedMaterialInfo
{
	INT Actors;
	INT BSP;
	INT StaticMeshes;
	INT StaticMeshSkins;
	INT Terrains;
	INT Sprites;
	INT EmitterTextures;

	FUsedMaterialInfo()
	:	Actors(0)
	,	BSP(0)
	,	StaticMeshes(0)
	,	StaticMeshSkins(0)
	,	Terrains(0)
	,	Sprites(0)
	,	EmitterTextures(0)
	{}
};


void DrawTextureBrowser( UViewport* Viewport )
{
	guard(DrawTextureBrowser);

	UObject* Pkg = Viewport->MiscRes; 
	if( Pkg && Viewport->Group!=NAME_None && FindObject<UPackage>( Pkg, *Viewport->Group ) )
		Pkg = FindObject<UPackage>( Pkg, *Viewport->Group );

	FMemMark Mark(GMem);
	enum {MAX=16384};
	UMaterial**  List    = new(GMem,MAX)UMaterial*;
	TMap<UMaterial*,FUsedMaterialInfo> UsedMaterials;

	// Make a short list of filtered textures.
	INT* LastScroll = NULL;
	INT n = 0;
	if( Viewport->Actor->RendMap == REN_TexBrowser )
	{
		LastScroll = &GTBOptions->LastScroll;
		for( TObjectIterator<UMaterial> It; It && n<MAX; ++It )
			if( !IsMaterialFiltered( *It ) && It->IsIn(Pkg) )
				List[n++] = *It;
	}
	else if( Viewport->Actor->RendMap == REN_TexBrowserUsed )
	{
		LastScroll = &GTBOptions->LastScrollUsed;

		// ===
		// Build a list of textures which are used in the current map.
		// ===
		UsedMaterials.Empty();

		// ACTORS
		for( INT x = 0 ; x < GUnrealEd->Level->Actors.Num() ; ++x )
		{
			AActor* Actor  = Cast<AActor>( GUnrealEd->Level->Actors(x) );
			if( Actor )
			{
				// SPRITES
				if( GTBOptions->IUFilter&IUF_Sprites )
					if( Actor->Texture )
					{
						FUsedMaterialInfo* Info = UsedMaterials.Find(Actor->Texture);
						if( !Info )
							Info = &UsedMaterials.Set(Actor->Texture, FUsedMaterialInfo());
						Info->Sprites++;
					}

				// ACTORS
				if( GTBOptions->IUFilter&IUF_Actors && !Cast<AStaticMeshActor>(Actor) )
				{
					for( INT y = 0 ; y < Actor->Skins.Num() ; ++y )
						if( Actor->Skins(y) )
						{
							FUsedMaterialInfo* Info = UsedMaterials.Find(Actor->Skins(y));
							if( !Info )
								Info = &UsedMaterials.Set(Actor->Skins(y), FUsedMaterialInfo());
								Info->Actors++;
						}
				}

				// BRUSHES
				if( GTBOptions->IUFilter&IUF_Brushes )
				{
					ABrush* Brush = Cast<ABrush>( Actor );
					if( Brush && Brush->Brush )
						for( INT y = 0 ; y < Brush->Brush->Polys->Element.Num() ; ++y )
						{
							FPoly* Poly = &(Brush->Brush->Polys->Element(y));
							if( Poly->Material )
							{
								FUsedMaterialInfo* Info = UsedMaterials.Find(Poly->Material);
								if( !Info )
									Info = &UsedMaterials.Set(Poly->Material, FUsedMaterialInfo());
								Info->BSP++;
							}
						}
				}

				// STATIC MESHES
				if( GTBOptions->IUFilter&IUF_StaticMeshes )
				{
					AStaticMeshActor* SM = Cast<AStaticMeshActor>( Actor );
					if( SM && SM->StaticMesh )
					{
						for( INT y = 0 ; y < SM->StaticMesh->Materials.Num() ; ++y )
							if( SM->StaticMesh->Materials(y).Material )
							{
								UMaterial* Material = SM->StaticMesh->Materials(y).Material;

								// Check to see if this material is overridden in the staticmeshactor.  If so, use that material instead.

								if( y < SM->Skins.Num() && SM->Skins(y) )
									Material = SM->Skins(y);

								FUsedMaterialInfo* Info = UsedMaterials.Find(Material);
								if( !Info )
									Info = &UsedMaterials.Set(Material, FUsedMaterialInfo());
								Info->StaticMeshes++;
							}
					}
				}

				// TERRAIN
				if( GTBOptions->IUFilter&IUF_Terrain )
				{
					ATerrainInfo* TI = Cast<ATerrainInfo>( Actor );
					if( TI )
					{
						for( INT y = 0 ; y < ARRAY_COUNT(TI->Layers) ; ++y )
						{
							if( TI->Layers[y].AlphaMap )
							{
								FUsedMaterialInfo* Info = UsedMaterials.Find(TI->Layers[y].AlphaMap);
								if( !Info )
									Info = &UsedMaterials.Set(TI->Layers[y].AlphaMap, FUsedMaterialInfo());
								Info->Terrains++;
							}
							if( TI->Layers[y].Texture )
							{
								FUsedMaterialInfo* Info = UsedMaterials.Find(TI->Layers[y].Texture);
								if( !Info )
									Info = &UsedMaterials.Set(TI->Layers[y].Texture, FUsedMaterialInfo());
								Info->Terrains++;
							}
						}
					}
				}

				// EMITTERS
				if( GTBOptions->IUFilter&IUF_Emitters )
				{
					AEmitter* Emitter = Cast<AEmitter>( Actor );
					if( Emitter )
					{
						for( INT y = 0 ; y < Emitter->Emitters.Num() ; ++y )
						{
							UParticleEmitter* PE = Emitter->Emitters(y);
							if( PE->Texture )
							{
								FUsedMaterialInfo* Info = UsedMaterials.Find(PE->Texture);
								if( !Info )
									Info = &UsedMaterials.Set(PE->Texture, FUsedMaterialInfo());
								Info->EmitterTextures++;
							}
						}
					}
				}
			}
		}

		// Copy the used materials into the real array
		n = 0;
		for( TMap<UMaterial*,FUsedMaterialInfo>::TIterator It(UsedMaterials);It;++It )
			if( !IsMaterialFiltered( It.Key() ) )
				List[n++] = It.Key();
	}
	else if( Viewport->Actor->RendMap == REN_TexBrowserMRU )
	{
		LastScroll = &GTBOptions->LastScrollMRU;

		// Copy the used materials into the real array
		n = 0;
		for( INT x = 0 ; x < GTBOptions->MRUMaterials.Num() ; ++x )
			List[n++] = GTBOptions->MRUMaterials(x);
	}

	// Sort textures by name.
	if( Viewport->Actor->RendMap != REN_TexBrowserMRU )
		appQsort( &List[0], n, sizeof(UMaterial*), ResNameCompare );

	Viewport->Canvas->Color = FColor(255,255,255);

	// Some materials have a special outline around them.
	UBOOL bDisplayBorder;
	FColor BorderColor;
	INT BorderOffset;

	INT YL = 1;
	INT TextBuffer = -1;
	INT X, Y, HighYInRow = 0;
	*LastScroll = 0;
	if( YL > 0 )
	{
		X = 4;
		Y = 4 - Viewport->Actor->Misc2;
		HighYInRow = -1;

		for( INT i = 0 ; i < n ; i++ )
		{
			UMaterial* Material = List[i];
			UTexture* Texture = Cast<UTexture>(List[i]);

			GetMaterialBorderInfo( Material, &bDisplayBorder, &BorderColor, &BorderOffset );
			
			// CREATE TEXT LABELS

			// Create and measure the 2 labels.
			FString TextLabel = FString::Printf(TEXT("%s %s"), Material->GetClass()->GetName(), Material->GetName() );
			if( Texture ) 
				TextLabel += *FString::Printf( TEXT("%s [%s]"), Texture->Detail ? TEXT("") : TEXT("*"), *Texture->GetFormatDesc() );
			else
			{
				FString ErrStr;
				UMaterial* ErrMat=NULL;
				INT NumPasses = 0;
				Viewport->RI->SetMaterial( Material, &ErrStr, &ErrMat, &NumPasses );
				if( ErrMat )
					TextLabel += TEXT(" [ERROR]");
				else
					TextLabel += *FString::Printf( TEXT(" [%d pass%s]"), NumPasses, NumPasses==1?TEXT(""):TEXT("es") );
			}
			if( Viewport->Actor->RendMap == REN_TexBrowserUsed )
			{
				FUsedMaterialInfo* Info = UsedMaterials.Find(Material);
				if( Info )
				{
					if( Info->Actors )
						TextLabel += FString::Printf(TEXT(" Act:%d"), Info->Actors );
					if( Info->Sprites )
						TextLabel += FString::Printf(TEXT(" Spr:%d"), Info->Sprites );
					if( Info->StaticMeshes )
						TextLabel += FString::Printf(TEXT(" SMMat:%d"), Info->StaticMeshes );
					if( Info->StaticMeshSkins )
						TextLabel += FString::Printf(TEXT(" SMSkin:%d"), Info->StaticMeshSkins );
					if( Info->BSP )
						TextLabel += FString::Printf(TEXT(" BSP:%d"), Info->BSP );
					if( Info->Terrains )
						TextLabel += FString::Printf(TEXT(" Ter:%d"), Info->Terrains );
				}
			}

			INT LabelWidth, LabelHeight;
			Viewport->Canvas->WrappedStrLenf( Viewport->Canvas->SmallFont, LabelWidth, LabelHeight, TEXT("%s"), *TextLabel );

			INT SizeWidth=0, SizeHeight=0;
			FString SizeLabel = TEXT("");
			if( Material )
			{
				SizeLabel = FString::Printf( TEXT("(%dx%d)"), Material->MaterialUSize(), Material->MaterialVSize() );
				Viewport->Canvas->WrappedStrLenf( Viewport->Canvas->SmallFont, SizeWidth, SizeHeight, TEXT("%s"), *SizeLabel );
			}

			if( TextBuffer == -1)
				TextBuffer = LabelHeight + SizeHeight + 4;

			// Display the texture wide enough to show it's entire text label without wrapping.
			INT TextureWidth = Max( GTBOptions->GetMaterialWidth( Material ), (LabelWidth > SizeWidth) ? LabelWidth : SizeWidth ),
				TextureHeight = GTBOptions->GetMaterialHeight( Material );

			// Now that we've measured the length of the text label, stick the texture size
			// on the end.  The texture size will wrap to the next line and look nice.

			// Do we need to create a new line?
			if( X + TextureWidth + BorderOffset > Viewport->SizeX )
			{
				X = 4;
				Y += HighYInRow + TextBuffer + 8 + BorderOffset;
				*LastScroll += HighYInRow + TextBuffer + 8 + BorderOffset;
				HighYInRow = -1;
			}
			if( (TextureHeight+BorderOffset) > HighYInRow ) HighYInRow = (TextureHeight+BorderOffset);

			if( Y+TextureHeight+LabelHeight+4+BorderOffset>0 && Y<Viewport->SizeY )
			{
				PUSH_HIT(Viewport,HBrowserMaterial(Material));

				// SPECIAL BORDER
				if( bDisplayBorder )
				{
					Viewport->Canvas->DrawPattern( GUnrealEd->Level->GetLevelInfo()->WhiteSquareTexture,
						X-5, Y-5,
						TextureWidth+10, TextureHeight+TextBuffer+10,
						1.0, 0.0, 0.0, 0.0f, BorderColor.Plane(), FPlane(0,0,0,0) );
					Viewport->Canvas->DrawPattern( GUnrealEd->Level->GetLevelInfo()->WhiteSquareTexture,
						X-4, Y-4,
						TextureWidth+8, TextureHeight+TextBuffer+8,
						1.0, 0.0, 0.0, 0.0f, FColor(0,0,0).Plane(), FPlane(0,0,0,0) );
				}

				// SELECTION HIGHLIGHT
				if( Material == GUnrealEd->CurrentMaterial )
					Viewport->Canvas->DrawPattern( GUnrealEd->BkgndHi,
						X-4, Y-4,
						TextureWidth+8, TextureHeight+TextBuffer+8,
						1.0, 0.0, 0.0, 0.0f, FPlane(1.,1.,1.,1.), FPlane(0,0,0,0) );

				// THE TEXTURE ITSELF

				if( GTBOptions->TexViewSize & TVS_VARIABLE )
				{
					FLOAT Scale = GTBOptions->GetScale( Material ),
						USz = Material->MaterialUSize() * Scale,
						VSz = Material->MaterialVSize() * Scale;

					Viewport->Canvas->DrawTile( Material,
						X, Y, USz, VSz,
						0.f, 0.f, Material->MaterialUSize(), Material->MaterialVSize(),
						1.f, FPlane(1.f,1.f,1.f,1.f), FPlane(0,0,0,0));
				}
				else
				{
					FLOAT 
						URatio = GTBOptions->GetURatio( Material ),
						VRatio = GTBOptions->GetVRatio( Material ),
						USz = GTBOptions->GetTileSize( Material ),
						VSz = GTBOptions->GetTileSize( Material );

					FLOAT TileSize = GTBOptions->GetTileSize( Material ),
						USize = Material->MaterialUSize(),
						VSize = Material->MaterialVSize();
					URatio = TileSize > USize ? TileSize / USize : 1.f;
					VRatio = TileSize > VSize ? TileSize / VSize : 1.f;

					if( USize > VSize )
						VRatio *= USize / VSize;
					else
						URatio *= VSize / USize;

					Viewport->Canvas->DrawTile( Material,
						X, Y, USz, VSz,
						0.f, 0.f, Material->MaterialUSize() * URatio, Material->MaterialVSize() * VRatio,
						1.f, FPlane(1.f,1.f,1.f,1.f), FPlane(0,0,0,0));
				}

				// TEXT LABELS
				// If this texture is the current texture, draw a black border around the
				// text to make it more readable against the background.
				if( Material == GUnrealEd->CurrentMaterial )
				{
					INT Offsets[] = { 1, 0, -1, 0, 0, 1, 0, -1 };
					Viewport->Canvas->Color = FColor(0,0,0);
					for( INT x = 0 ; x < 4 ; x++ )
					{
						Viewport->Canvas->SetClip( X+Offsets[x*2], Y+TextureHeight+2+Offsets[(x*2)+1], TextureWidth, TextBuffer );
						Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0, TEXT("%s"), *TextLabel );

						if( Texture )
						{
							Viewport->Canvas->SetClip( X+Offsets[x*2], Y+TextureHeight+LabelHeight+4+Offsets[(x*2)+1], TextureWidth, TextBuffer );
							Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0, TEXT("%s"), *SizeLabel );
						}
					}
				}

				// Draw a special background behind the text labels so that selection of 0x0 textures is still possible

				Viewport->Canvas->DrawPattern( GUnrealEd->BkgndHi,
					X, Y+TextureHeight+2,
					TextureWidth, TextBuffer,
					1.0, 0.0, 0.0, 0.0f, ( Material == GUnrealEd->CurrentMaterial ) ? FPlane(1,1,1,1.) : FPlane(0,0,0,1.), FPlane(0,0,0,0) );

				Viewport->Canvas->Color = FColor(255,255,255);
				Viewport->Canvas->SetClip( X, Y+TextureHeight+2, TextureWidth, TextBuffer );
				Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0, TEXT("%s"), *TextLabel );

				// Render the size in white if this is the selected texture
				if( Material != GUnrealEd->CurrentMaterial )
					Viewport->Canvas->Color = FColor(192,192,192);
				Viewport->Canvas->SetClip( X, Y+TextureHeight+LabelHeight+4, TextureWidth, TextBuffer );
				Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0, TEXT("%s"), *SizeLabel );

				Viewport->Canvas->Color = FColor(255,255,255);
				Viewport->Canvas->SetClip( 0, 0, Viewport->SizeX, Viewport->SizeY );

				POP_HIT(Viewport);
			}

			// Update position
			X += TextureWidth + 8 + BorderOffset;
		}
	}
	*LastScroll += HighYInRow + TextBuffer + 8;
	*LastScroll = Max(0, *LastScroll - Viewport->SizeY);

	Mark.Pop();

	unguard;
}

/* OLD CODE
void DrawTextureBrowser( FSceneNode* Frame )
{
	guard(DrawTextureBrowser);
	UObject* Pkg = Frame->Viewport->MiscRes;
	if( Pkg && Frame->Viewport->Group!=NAME_None )
		Pkg = FindObject<UPackage>( Pkg, *Frame->Viewport->Group );

	FMemMark Mark(GMem);
	enum {MAX=16384};
	UTexture**  List    = new(GMem,MAX)UTexture*;
	INT			Size	= Frame->Viewport->Actor->Misc1;
	INT			PerRow	= Frame->X/Size;
	INT			Space	= (Frame->X - Size*PerRow)/(PerRow+1);
	INT			VSkip	= (Size>=64) ? 10 : 0;

	// Make the list.
	INT n = 0;
	for( TObjectIterator<UTexture> It; It && n<MAX; ++It )
		if( It->IsIn(Pkg) )
		{
			FString TexName = It->GetName();

			if( appStrstr( *(TexName.Caps()), *(GTexNameFilter.Caps()) ) )
				List[n++] = *It;
		}

	// Sort textures by name.
	appQsort( &List[0], n, sizeof(UTexture *), ResNameCompare );

	// Draw them.
	INT YL = Space+(Size+Space+VSkip)*((n+PerRow-1)/PerRow);
	if( YL > 0 )
	{
		INT YOfs = -((Frame->Viewport->Actor->Misc2*Frame->Y)/512);
		for( INT i=0; i<n; i++ )
		{
			UTexture* Texture = List[i];
			INT X = (Size+Space)*(i%PerRow);
			INT Y = (Size+Space+VSkip)*(i/PerRow)+YOfs;
			if( Y+Size+Space+VSkip>0 && Y<Frame->Y )
			{
				PUSH_HIT(Frame,HBrowserTexture(Texture));
				if( Texture==GUnrealEd->CurrentTexture )
					Frame->Viewport->Canvas->DrawPattern( GUnrealEd->BkgndHi, X+1, Y+1, Size+Space*2-2, Size+Space*2+VSkip-2, 1.0, 0.0, 0.0, NULL, 1.0, FPlane(1.,1.,1.,0), FPlane(0,0,0,0), 0 );
				FLOAT Scale=0.125;
				while( Texture->USize/Scale>Size || Texture->VSize/Scale>Size )
					Scale *= 2;
				Frame->Viewport->Canvas->DrawPattern( Texture, X+Space, Y+Space, Size, Size, Scale, X+Space, Y+Space, NULL, 1.0, FPlane(1.,1.,1.,0), FPlane(0,0,0,0), 0 );
				if( Size>=64 )
				{
					FString Temp = Texture->GetName();
					if( Size>=128 )
						Temp += FString::Printf( TEXT(" (%ix%i)"), Texture->USize, Texture->VSize );
					Frame->Viewport->Canvas->SetClip( X, Y+Size+1, Size, Frame->Y-Y-Size-1 );
					Frame->Viewport->Canvas->WrappedPrintf( Frame->Viewport->Canvas->MedFont, 1, TEXT("%s"), *Temp );
					Frame->Viewport->Canvas->SetClip( 0, 0, Frame->X, Frame->Y );
				}
				POP_HIT(Frame);
			}
		}
	}
	Mark.Pop();
	GLastScroll = Max(0,(512*(YL-Frame->Y))/Frame->Y);
	unguard;
}
*/

/*-----------------------------------------------------------------------------
   Static mesh browser routines.
-----------------------------------------------------------------------------*/

void DrawStaticMeshBrowser(UViewport* Viewport)
{
	guard(DrawStaticMeshBrowser);

	UStaticMesh*	StaticMesh = (UStaticMesh*)GUnrealEd->CurrentStaticMesh;
	FActorSceneNode	SceneNode(Viewport,&Viewport->RenderTarget,Viewport->Actor,Viewport->Actor,Viewport->Actor->Location,Viewport->Actor->Rotation,Viewport->Actor->FovAngle);
	FVector			SavedLocation = Viewport->Actor->Location;
	FRotator		SavedRotation = Viewport->Actor->Rotation;

	Viewport->Actor->StaticMesh = StaticMesh;
	Viewport->Actor->SetDrawType(DT_StaticMesh);
	Viewport->Actor->Location = FVector(0,0,0);
	Viewport->Actor->Rotation = FRotator(0,0,0);
	Viewport->Actor->ClearRenderData();

	// Draw the wire grid.

	GUnrealEd->DrawWireBackground(&SceneNode);
	Viewport->RI->Clear(0,FColor(0,0,0),1,1.0f);

	// Draw the actor.
	// Use wireframe if displaying mass properties so we can see them.
	if(Viewport->Actor->ShowFlags & SHOW_KarmaMassProps)
		Viewport->Actor->RendMap = REN_Wire;
	SceneNode.Render(Viewport->RI);
	Viewport->Actor->RendMap = REN_StaticMeshBrowser;

	// Draw the axis indicator.

	if(GUnrealEd->UseAxisIndicator)
		GUnrealEd->edDrawAxisIndicator(&SceneNode);

	// Print the name of the static mesh at the top of the viewport.

	Viewport->Canvas->CurX = 0;
	Viewport->Canvas->CurY = 0;
	Viewport->Canvas->Color = FColor(255,255,255);

	FString Text = StaticMesh ? StaticMesh->GetPathName() : TEXT("No Static Mesh");

#ifdef WITH_KARMA
	// If this staticmesh has karma properties, then prefix name with asterix.
	if(StaticMesh && StaticMesh->KPhysicsProps)
	{
		Text = FString(TEXT("*")) + Text;

		// Draw the Karma stuff (collision, COM etc.)
		StaticMesh->KPhysicsProps->Draw(Viewport->RI, Viewport->Actor->ShowFlags);
	}
#endif

	Viewport->Canvas->WrappedPrintf
	(
		Viewport->Canvas->SmallFont,
		1,
 		*Text
	);

	// Print the poly count on the next line
	if( StaticMesh )
	{
		Viewport->Canvas->Color = FColor(0,255,0);

		if(!StaticMesh->RawTriangles.Num())
			StaticMesh->RawTriangles.Load();

#ifdef WITH_KARMA
		if(StaticMesh->KPhysicsProps && Viewport->Actor->ShowFlags & SHOW_KarmaPrimitives)
		{
			int totalElems = StaticMesh->KPhysicsProps->AggGeom.GetElementCount();

			// Show number of triangles and number of collision primitives.
			Text = *FString::Printf( TEXT("Triangles : %d,  Collision Prims: %d"), 
				StaticMesh->RawTriangles.Num(), totalElems);

			Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 1, *Text);
		}
		else
		{
#endif
			Text = *FString::Printf( TEXT("Triangles : %d"), StaticMesh->RawTriangles.Num() );
			Viewport->Canvas->WrappedPrintf
			(
				Viewport->Canvas->SmallFont,
				1,
 				*Text
			);
#ifdef WITH_KARMA
		}
#endif
	}

	// Reset the viewport.

	Viewport->Actor->SetDrawType(DT_Sprite);
	Viewport->Actor->StaticMesh = NULL;
	Viewport->Actor->Location = SavedLocation;
	Viewport->Actor->Rotation = SavedRotation;

	unguard;
}

/*-----------------------------------------------------------------------------
   Material viewport routines.
-----------------------------------------------------------------------------*/

// Returns the width of the field in pixels
INT DrawField( UViewport* Viewport, INT InX, INT InY, FColor InLabelColor, FString InLabel, FColor InFieldColor, const TCHAR* Field, ... )
{
	guard(DrawField);

	TCHAR TempStr[4096] = TEXT("\0");
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Field, Field );

	INT Width, Height, TotalWidth = 0;

	Viewport->Canvas->WrappedStrLenf( Viewport->Canvas->SmallFont, Width, Height, *InLabel );
	TotalWidth += Width;
	Viewport->Canvas->Color = InLabelColor;
	Viewport->Canvas->SetClip( InX, InY, Viewport->SizeX, Height );
	Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0, *InLabel );

	InX += Width;
	Viewport->Canvas->WrappedStrLenf( Viewport->Canvas->SmallFont, Width, Height, TempStr );
	TotalWidth += Width;
	Viewport->Canvas->Color = InFieldColor;
	Viewport->Canvas->SetClip( InX, InY, Viewport->SizeX, Height );
	Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0, TempStr );

	Viewport->Canvas->SetClip(0,0,Viewport->SizeX,Viewport->SizeY);

	return TotalWidth;

	unguard;
}

#define TREE_ITEM_SZ		48
#define TREE_ITEM_SPACING	6
#define TREE_ITEM_TOTAL_SZ	(TREE_ITEM_SZ+TREE_ITEM_SPACING)

INT GTreeIndentLevel = 0;
INT GTreeSkipIndent = 0;

void DrawTreeConnection( UViewport* InViewport, INT InX, INT InY, UBOOL bEndofChain )
{
	guard(DrawTreeConnection);

	FCanvasUtil	CanvasUtil(&InViewport->RenderTarget,InViewport->RI);
	INT Height = TREE_ITEM_TOTAL_SZ/(bEndofChain?2:1);
	CanvasUtil.DrawLine(
		InX+(TREE_ITEM_SZ/2), InY,
		InX+(TREE_ITEM_SZ/2), InY+Height,
		FColor(255,255,255) );
	CanvasUtil.DrawLine(
		InX+(TREE_ITEM_SZ/2), InY+(TREE_ITEM_TOTAL_SZ/2),
		InX+TREE_ITEM_SZ, InY+(TREE_ITEM_TOTAL_SZ/2),
		FColor(255,255,255) );

	// Draw a line to the left for each indent level.
	INT NewX = InX - ((32+8)*GTreeSkipIndent);
	for( INT x = 1 ; x < GTreeIndentLevel ; ++x )
		CanvasUtil.DrawLine(
			(NewX+(TREE_ITEM_SZ/2))-((32+8)*x), InY,
			(NewX+(TREE_ITEM_SZ/2))-((32+8)*x), InY+TREE_ITEM_TOTAL_SZ,
			FColor(255,255,255) );

	unguard;
}

void DrawMaterial( UViewport* InViewport, UMaterial* InMaterial, FString InName, INT InX, INT& InY, UBOOL bEndofChain )
{
	guard(DrawMaterial);

	if( !bEndofChain )
		GTreeIndentLevel++;
	else
		GTreeSkipIndent++;

	FTexPropWindowInfo* Info = GMaterialTools->GetInfo( InViewport->Actor->Misc1 );
	if( !Info ) return;

	if( !InMaterial ) InMaterial = GUnrealEd->BadHighlight;

	UBOOL bDisplayBorder;
	FColor BorderColor;
	INT BorderOffset;
	GetMaterialBorderInfo( InMaterial, &bDisplayBorder, &BorderColor, &BorderOffset );

	INT Width = TREE_ITEM_SZ, Height = TREE_ITEM_SZ, ArrowWidth = GUnrealEd->MaterialArrow->MaterialUSize();
	FLOAT Scale = 1;
	if( InMaterial->MaterialUSize() && InMaterial->MaterialVSize() )
	{
	if( InMaterial->MaterialUSize() > InMaterial->MaterialVSize() )	Scale = Min<INT>( TREE_ITEM_SZ, InMaterial->MaterialUSize() ) / (FLOAT)InMaterial->MaterialUSize();
	else															Scale = Min<INT>( TREE_ITEM_SZ, InMaterial->MaterialVSize() ) / (FLOAT)InMaterial->MaterialVSize();
	}

	PUSH_HIT(InViewport,HMaterialTree( InMaterial, InViewport->Actor->Misc1 ));

	// HIGHLIGHT
	if( (INT)InMaterial == InViewport->Actor->Misc2 )
		InViewport->Canvas->DrawPattern( GUnrealEd->BkgndHi,
			InX-2, InY-2,
			InViewport->SizeX+4, Height+4,
			1.0, 0.0, 0.0, 0.0f, FColor(255,255,255).Plane(), FPlane(0,0,0,0) );
	else
		InViewport->Canvas->DrawPattern( GUnrealEd->Level->GetLevelInfo()->WhiteSquareTexture,
			InX, InY,
			InViewport->SizeX, Height,
			1.0, 0.0, 0.0, 0.0f, FColor(0,0,0).Plane(), FPlane(0,0,0,0) );

	// BORDER
	if( bDisplayBorder )
	{
		InViewport->Canvas->DrawPattern( GUnrealEd->Level->GetLevelInfo()->WhiteSquareTexture,
			InX-1, InY-1,
			Width+2, Height+2,
			1.0, 0.0, 0.0, 0.0f, BorderColor.Plane(), FPlane(0,0,0,0) );
	}

	InViewport->Canvas->DrawPattern( GUnrealEd->Level->GetLevelInfo()->WhiteSquareTexture,
		InX, InY,
		Width, Height,
		1.0, 0.0, 0.0, 0.0f, FColor(0,0,0).Plane(), FPlane(0,0,0,0) );

	// MATERIAL
	InViewport->Canvas->DrawIcon( InMaterial,
		InX, InY,
		InMaterial->MaterialUSize()*Scale, InMaterial->MaterialVSize()*Scale,
		0.0, FPlane(1.,1.,1.,1.), FPlane(0,0,0,0)/*, PF_Masked*/ );	//!!MAT

	// TEXT
	FString Title;
	if( Cast<UFinalBlend>( InMaterial ) )		Title = TEXT("FINAL BLEND");
	else if( Cast<UShader>( InMaterial ) )		Title = TEXT("SHADER");
	else if( Cast<UTexModifier>( InMaterial ) )	Title = TEXT("MODIFIER");
	else if( Cast<UCombiner>( InMaterial ) )	Title = TEXT("COMBINER");
	else if( Cast<UTexture>( InMaterial ) )		Title = TEXT("TEXTURE");

	INT TexW, TexH;
	InViewport->Canvas->WrappedStrLenf( InViewport->Canvas->SmallFont, TexW, TexH, *Title );
	INT W = DrawField( InViewport, InX+Width+8, InY, BorderColor, TEXT(""), BorderColor, TEXT("%s"), Title );
	if( InName.Len() )
		DrawField( InViewport, InX+Width+8+W, InY, FColor(0,255,255), TEXT(""), FColor(0,255,255), TEXT(" (%s)"), *InName );


	DrawField( InViewport, InX+Width+8, InY+TexH, FColor(255,255,255), TEXT(""), FColor(255,255,255), TEXT("%s"), InMaterial->GetPathName() );

	POP_HIT(InViewport);

	InY += Height + TREE_ITEM_SPACING;
	Info->TreeScrollMax += Height + TREE_ITEM_SPACING;

	if( Cast<UFinalBlend>( InMaterial ) )
	{
		UFinalBlend* Mat = Cast<UFinalBlend>( InMaterial );

		DrawTreeConnection( InViewport, InX, InY, 0 );
		DrawMaterial( InViewport, Mat->Material, TEXT("Material"), InX+ArrowWidth+BorderOffset, InY, 0 );
	}
	else if( Cast<UShader>( InMaterial ) )
	{
		UShader* Mat = Cast<UShader>( InMaterial );

		DrawTreeConnection( InViewport, InX, InY, 0 );
		DrawMaterial( InViewport, Mat->Diffuse, TEXT("Diffuse"), InX+ArrowWidth+BorderOffset, InY, 0 );

		DrawTreeConnection( InViewport, InX, InY, 0 );
		DrawMaterial( InViewport, Mat->Opacity, TEXT("Opacity"), InX+ArrowWidth+BorderOffset, InY, 0 );

		DrawTreeConnection( InViewport, InX, InY, 0 );
		DrawMaterial( InViewport, Mat->Specular, TEXT("Specular"), InX+ArrowWidth+BorderOffset, InY, 0 );

		DrawTreeConnection( InViewport, InX, InY, 0 );
		DrawMaterial( InViewport, Mat->SpecularityMask, TEXT("Specularity Mask"), InX+ArrowWidth+BorderOffset, InY, 0 );

		DrawTreeConnection( InViewport, InX, InY, 0 );
		DrawMaterial( InViewport, Mat->SelfIllumination, TEXT("Self Illumination"), InX+ArrowWidth+BorderOffset, InY, 0 );

		DrawTreeConnection( InViewport, InX, InY, 0 );
		DrawMaterial( InViewport, Mat->SelfIlluminationMask, TEXT("Self Illumination Mask"), InX+ArrowWidth+BorderOffset, InY, 0 );

		DrawTreeConnection( InViewport, InX, InY, 1 );
		DrawMaterial( InViewport, Mat->Detail, TEXT("Detail"), InX+ArrowWidth+BorderOffset, InY, 1 );
	}
	else if( Cast<UTexModifier>( InMaterial ) )
	{
		UTexModifier* Mat = Cast<UTexModifier>( InMaterial );

		DrawTreeConnection( InViewport, InX, InY, 1 );
		DrawMaterial( InViewport, Mat->Material, TEXT("Material"), InX+ArrowWidth+BorderOffset, InY, 1 );
	}
	else if( Cast<UCombiner>( InMaterial ) )
	{
		UCombiner* Mat = Cast<UCombiner>( InMaterial );

		DrawTreeConnection( InViewport, InX, InY, 0 );
		DrawMaterial( InViewport, Mat->Material1, TEXT("Material1"), InX+ArrowWidth+BorderOffset, InY, 0 );

		DrawTreeConnection( InViewport, InX, InY, 0 );
		DrawMaterial( InViewport, Mat->Material2, TEXT("Material2"), InX+ArrowWidth+BorderOffset, InY, 0 );

		DrawTreeConnection( InViewport, InX, InY, 1 );
		DrawMaterial( InViewport, Mat->Mask, TEXT("Mask"), InX+ArrowWidth+BorderOffset, InY, 1 );
	}
	else if( Cast<UTexture>( InMaterial ) )
	{
	}

	if( !bEndofChain )
		GTreeIndentLevel--;
	else
		GTreeSkipIndent--;

	unguard;
}

void DrawMaterialEditor( UViewport* InViewport )
{
	guard(DrawMaterialEditor);

	FTexPropWindowInfo* Info = GMaterialTools->GetInfo( InViewport->Actor->Misc1 );
	if( !Info ) return;

	INT X, Y;

	X = 4;
	Y = 4 - Info->TreeScrollPos;

	UMaterial* Material = Cast<UMaterial>(InViewport->MiscRes);
	Info->TreeScrollMax = 0;
	DrawMaterial( InViewport, Material, TEXT(""), X, Y, 0 );
	Info->TreeScrollMax -= TREE_ITEM_SZ - TREE_ITEM_SPACING - 4;

	unguard;
}

/*-----------------------------------------------------------------------------
   Terrain layer viewport routines.
-----------------------------------------------------------------------------*/

#define LAYER_SZ 32

void DrawAlphaMap( UViewport* Viewport, UMaterial* InAlphaMap, INT InCurX, INT InCurY, UBOOL bAllowForLabel )
{
	guard(DrawAlphaMap);

	static UTexture* TerrainBad = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.TerrainBad") ));

	FCanvasUtil	CanvasUtil(&Viewport->RenderTarget,Viewport->RI);

	INT XL, YL = -2;
	if( bAllowForLabel )
		Viewport->Canvas->WrappedStrLenf( Viewport->Canvas->SmallFont, XL, YL, TEXT("M") );

	if( GTerrainTools.CurrentAlphaMap == InAlphaMap && InAlphaMap != NULL )
	{
		//!!MAT
		CanvasUtil.DrawTile(InCurX,InCurY,InCurX + LAYER_SZ + 1,InCurY + LAYER_SZ + 2 + YL,0,0,128,128,5.0f,GUnrealEd->BkgndHi,/*PF_NoZTest,*/FColor(255,0,0));
		CanvasUtil.DrawTile(InCurX - 1,InCurY - 1,InCurX + LAYER_SZ + 3,InCurY + LAYER_SZ + 2 + YL + 3,0,0,128,128,5.0f,GUnrealEd->BkgndHi,/*PF_NoZTest,*/FColor(255,0,0));
	}

	UMaterial* Texture = (InAlphaMap ? InAlphaMap : TerrainBad);
	//!!MAT
	CanvasUtil.DrawTile(InCurX,InCurY,InCurX + LAYER_SZ,InCurY + LAYER_SZ,0,0,Texture->MaterialUSize(),Texture->MaterialVSize(),0.0f,Texture,/*PF_NoZTest,*/FColor(255,255,255));

	unguard;
}

void DrawTerrainHeightmap( UViewport* Viewport )
{
	guard(DrawTerrainHeightmap);

	ATerrainInfo* TerrainInfo = GTerrainTools.GetCurrentTerrainInfo();
	if( TerrainInfo )
	{
		INT CurX = 4, CurY = 4;

		PUSH_HIT(Viewport,HTerrainToolLayer(TerrainInfo,0,NULL));

		FColor BackgroundColor(32,32,32);
		if( !GTerrainTools.CurrentLayer )
			BackgroundColor = FColor(255,255,255);

		Viewport->Canvas->DrawPattern( GUnrealEd->BkgndHi,
			CurX-4, CurY-4,
			Viewport->Canvas->ClipX, 48+8,
			1.0, 0.0, 0.0, 0.0, BackgroundColor.Plane(), FPlane(0,0,0,0) );

		if( GTerrainTools.GetCurrentTerrainInfo()->TerrainMap )
		{
			Viewport->Canvas->DrawIcon( GTerrainTools.GetCurrentTerrainInfo()->TerrainMap,
				CurX, CurY,
				48, 48,
				0.0, FPlane(1.,1.,1.,1.), FPlane(0,0,0,0));

			Viewport->Canvas->Color = FColor(255,255,255);
			Viewport->Canvas->CurX = CurX + 48+8;
			Viewport->Canvas->CurY = 0;
			Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0, GTerrainTools.GetCurrentTerrainInfo()->TerrainMap->GetName() );

			Viewport->Canvas->Color = FColor(192,192,192);
			Viewport->Canvas->CurX = CurX + 48+8;
			Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0,
				*FString::Printf( TEXT("Format  : %s"), *GTerrainTools.GetCurrentTerrainInfo()->TerrainMap->GetFormatDesc() ) );

			Viewport->Canvas->CurX = CurX + 48+8;
			Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0,
				*FString::Printf( TEXT("Size    : %dx%d"), GTerrainTools.GetCurrentTerrainInfo()->TerrainMap->UClamp, GTerrainTools.GetCurrentTerrainInfo()->TerrainMap->VClamp ) );

			Viewport->Canvas->CurX = CurX + 48+8;
			Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0, *FString::Printf( TEXT("Scale : %1.1f / %1.1f / %1.1f"),
				GTerrainTools.GetCurrentTerrainInfo()->TerrainScale.X,
				GTerrainTools.GetCurrentTerrainInfo()->TerrainScale.Y,
				GTerrainTools.GetCurrentTerrainInfo()->TerrainScale.Z ) );
		}

		POP_HIT(Viewport);
	}

	unguard;
}

// Draws what is required for a terrain layer.
void DrawLayer( UViewport* Viewport, ATerrainInfo* InTerrainInfo, INT InLayer, INT CurX, INT CurY )
{
	guard(DrawLayer);

	static UTexture* TerrainBad = Cast<UTexture>(UObject::StaticFindObject( UTexture::StaticClass(), ANY_PACKAGE, TEXT("Engine.TerrainBad") ));

	// Painting is not allowed on certain layers - special indicators are drawn on those layers.
	UBOOL bPaintingAllowed = InTerrainInfo->Layers[InLayer].AlphaMap != NULL;

	PUSH_HIT(Viewport,HTerrainToolLayer(InTerrainInfo,InLayer+1,InTerrainInfo->Layers[InLayer].AlphaMap));

	// Figure out the color we should be using for this layer background.
	INT Adjust = 0;
	FColor BackgroundColor(64,64,64);
	if( GTerrainTools.CurrentLayer && InLayer == GTerrainTools.CurrentLayer-1 )
	{
		BackgroundColor = FColor(255,255,255);
		Adjust = 2;
	}

	Viewport->Canvas->DrawPattern( GUnrealEd->BkgndHi,
		CurX-2-Adjust, CurY-2-Adjust,
		Viewport->Canvas->ClipX-8+(Adjust*2), LAYER_SZ+4+(Adjust*2),
		1.0, 0.0, 0.0, 0.0, BackgroundColor.Plane(), FPlane(0,0,0,0) );

	if( !bPaintingAllowed )
		Viewport->Canvas->DrawPattern( GUnrealEd->BkgndHi,
			CurX-2, CurY-2,
			Viewport->Canvas->ClipX-8, LAYER_SZ+4,
			1.0, 0.0, 0.0, 0.0, FColor(128,0,0).Plane(), FPlane(0,0,0,0) );

	//
	// TEXTURE/ALPHA MAP
	//
	DrawAlphaMap( Viewport, InTerrainInfo->Layers[InLayer].Texture, CurX, CurY, 0 );
	DrawAlphaMap( Viewport, InTerrainInfo->Layers[InLayer].AlphaMap, CurX+LAYER_SZ+4, CurY, 0 );

	//
	// TEXT
	//

	Viewport->Canvas->Color = FColor(255,255,255);
	Viewport->Canvas->CurX = CurX+LAYER_SZ+LAYER_SZ+8;
	Viewport->Canvas->CurY = CurY;

	if( InTerrainInfo->Layers[InLayer].AlphaMap )
	{
		Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0, InTerrainInfo->Layers[InLayer].AlphaMap->GetName() );

		Viewport->Canvas->Color = FColor(192,192,192);
		Viewport->Canvas->CurX = CurX+LAYER_SZ+LAYER_SZ+8;
		Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0,
			*FString::Printf( TEXT("Format: %s"), *InTerrainInfo->Layers[InLayer].AlphaMap->GetFormatDesc() ) );

		Viewport->Canvas->CurX = CurX+LAYER_SZ+LAYER_SZ+8;
		Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0,
			*FString::Printf( TEXT("Size  : %dx%d"), InTerrainInfo->Layers[InLayer].AlphaMap->UClamp, InTerrainInfo->Layers[InLayer].AlphaMap->VClamp ) );
	}
	else
	{
		Viewport->Canvas->CurX = CurX+((LAYER_SZ+8)*2);
		Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0, TEXT("Undefined") );
	}

	POP_HIT(Viewport);

	unguard;
}

void DrawTerrainLayers( UViewport* Viewport )
{
	guard(DrawTerrainLayers);

	Viewport->Canvas->CurX = 0;
	Viewport->Canvas->CurY = 0;
	Viewport->Canvas->Color = FColor(255,255,255);

	// Measure the font.
	INT XL, YL;
	Viewport->Canvas->WrappedStrLenf( Viewport->Canvas->SmallFont, XL, YL, TEXT("M") );

	ATerrainInfo* TerrainInfo = GTerrainTools.GetCurrentTerrainInfo();
	if( TerrainInfo )
	{
		INT CurX = 4, CurY = 4 - GTerrainTools.LayerScrollPos;

		for( INT x = 0 ; x < ARRAY_COUNT(TerrainInfo->Layers) ; x++ )
		{
			DrawLayer( Viewport, TerrainInfo, x, CurX, CurY );
			CurY += LAYER_SZ + 8;
		}

		// FINISH UP
		CurY -= LAYER_SZ + 8;	// Subtract one layer out to make scrolling look better
		GTerrainTools.LayerScrollMax = CurY + GTerrainTools.LayerScrollPos;
	}

	unguard;
}

/*-----------------------------------------------------------------------------
   Terrain decoration layer viewport routines.
-----------------------------------------------------------------------------*/

// Draws what is required for a terrain decoration layer.
void DrawDecoLayer( UViewport* Viewport, ATerrainInfo* InTerrainInfo, INT InLayer, INT CurX, INT& CurY )
{
	guard(DrawDecoLayer);

	// Measure the font.
	INT XL, YL;
	Viewport->Canvas->WrappedStrLenf( Viewport->Canvas->SmallFont, XL, YL, TEXT("M") );

	PUSH_HIT(Viewport,HTerrainToolLayer(InTerrainInfo,InLayer+32,InTerrainInfo->DecoLayers(InLayer).DensityMap));

	// Figure out the color we should be using for this layer background.
	INT Adjust = 0;
	FColor BackgroundColor(64,64,64);
	if( GTerrainTools.CurrentLayer && InLayer == GTerrainTools.CurrentLayer-32 )
	{
		BackgroundColor = FColor(255,255,255);
		Adjust = 2;
	}

	Viewport->Canvas->DrawPattern( GUnrealEd->BkgndHi,
		CurX-2-Adjust, CurY-2-Adjust,
		Viewport->Canvas->ClipX-8+(Adjust*2), LAYER_SZ+4+YL+(Adjust*2),
		1.0, 0.0, 0.0, 1.0, BackgroundColor.Plane(), FPlane(0,0,0,0) );

	Viewport->Canvas->Color = FColor(255,255,255);
	Viewport->Canvas->CurX = CurX+((LAYER_SZ+8)*3);
	Viewport->Canvas->CurY = CurY;

	FString StaticMeshName = TEXT("No Static Mesh");
	if( InTerrainInfo->DecoLayers(InLayer).StaticMesh )
		StaticMeshName = InTerrainInfo->DecoLayers(InLayer).StaticMesh->GetName();
	Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0, *StaticMeshName );

	POP_HIT(Viewport);

	// DENSITY
	PUSH_HIT(Viewport,HTerrainToolLayer(InTerrainInfo,InLayer+32,InTerrainInfo->DecoLayers(InLayer).DensityMap));
	DrawAlphaMap( Viewport, InTerrainInfo->DecoLayers(InLayer).DensityMap, CurX, CurY, 1 );
	Viewport->Canvas->Color = FColor(0,255,0);
	Viewport->Canvas->CurX = CurX;
	Viewport->Canvas->CurY = CurY+LAYER_SZ+2;
	Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0, TEXT("Dens.") );
	POP_HIT(Viewport);

	// COLOR
	PUSH_HIT(Viewport,HTerrainToolLayer(InTerrainInfo,InLayer+32,InTerrainInfo->DecoLayers(InLayer).ColorMap));
	DrawAlphaMap( Viewport, InTerrainInfo->DecoLayers(InLayer).ColorMap, CurX+LAYER_SZ+8, CurY, 1 );
	Viewport->Canvas->CurX = CurX+LAYER_SZ+8;
	Viewport->Canvas->CurY = CurY+LAYER_SZ+2;
	Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0, TEXT("Clr") );
	POP_HIT(Viewport);

	// SCALE
	PUSH_HIT(Viewport,HTerrainToolLayer(InTerrainInfo,InLayer+32,InTerrainInfo->DecoLayers(InLayer).ScaleMap));
	DrawAlphaMap( Viewport, InTerrainInfo->DecoLayers(InLayer).ScaleMap, CurX+LAYER_SZ+8+LAYER_SZ+8, CurY, 1 );
	Viewport->Canvas->CurX = CurX+LAYER_SZ+8+LAYER_SZ+8;
	Viewport->Canvas->CurY = CurY+LAYER_SZ+2;
	Viewport->Canvas->WrappedPrintf( Viewport->Canvas->SmallFont, 0, TEXT("Scale") );
	POP_HIT(Viewport);

	// Finish up
	CurY += LAYER_SZ+8+YL;

	unguard;
}

void DrawTerrainDecoLayers( UViewport* Viewport )
{
	guard(DrawTerrainDecoLayers);

	Viewport->Canvas->CurX = 0;
	Viewport->Canvas->CurY = 0;
	Viewport->Canvas->Color = FColor(255,255,255);

	// Measure the font.
	INT XL, YL;
	Viewport->Canvas->WrappedStrLenf( Viewport->Canvas->SmallFont, XL, YL, TEXT("M") );

	ATerrainInfo* TI = GTerrainTools.GetCurrentTerrainInfo();
	if( TI )
	{
		INT CurX = 4, CurY = 4 - GTerrainTools.DecoLayerScrollPos;

		// DECO LAYERS
		for( INT i = 0 ; i < TI->DecoLayers.Num() ; ++i )
			DrawDecoLayer( Viewport, TI, i, CurX, CurY );

		// FINISH UP
		CurY -= LAYER_SZ + 8;	// Subtract one layer out to make scrolling look better
		GTerrainTools.DecoLayerScrollMax = CurY + GTerrainTools.DecoLayerScrollPos;
	}

	unguard;
}

/*-----------------------------------------------------------------------------
   Matinee viewport routines.
-----------------------------------------------------------------------------*/

void DrawActionBar( UViewport* Viewport, INT InCurX, INT InCurY, ASceneManager* SM )
{
	guard(DrawActionBar);

	UTexture* White = GUnrealEd->Level->GetLevelInfo()->WhiteSquareTexture;

	FLOAT TotalWidth = Viewport->SizeX-2,
		TotalTime = SM->GetTotalSceneTime(),
		LastTime = 0.f, CurTime = 0.f;

	if( !TotalTime )
		return;

	for( INT ma = 0 ; ma < SM->Actions.Num() ; ++ma )
	{
		UMatAction* MA = SM->Actions(ma);

		// Alternate color for each action
		FColor Color;
		if( ma%2 )	Color = FColor(128,255,255);
		else		Color = FColor(64,128,128);

		FLOAT Start = TotalWidth*(LastTime/TotalTime),
			Size = (TotalWidth*(MA->Duration/TotalTime));

		PUSH_HIT(Viewport,HMatineeAction(SM,MA));

		if( GMatineeTools.GetCurrentAction() == MA )
			Viewport->Canvas->DrawPattern( White,
				InCurX + Start - 1, InCurY - 1,
				Size+2, 4+2,
				1.0, 0.0, 0.0, 0.0f, FColor(255,255,255).Plane(), FPlane(0,0,0,0) );

		Viewport->Canvas->DrawPattern( White,
			InCurX + Start, InCurY,
			Size, 4,
			1.0, 0.0, 0.0, 0.0f, Color.Plane(), FPlane(0,0,0,0) );

		Viewport->Canvas->DrawPattern( White,
			InCurX + Start, InCurY-1,
			1, 6,
			1.0, 0.0, 0.0, 0.0f, FColor(255,255,0).Plane(), FPlane(0,0,0,0) );

		POP_HIT(Viewport);

		LastTime = CurTime+MA->Duration;
		CurTime += MA->Duration;
	}

	unguard;
}

void DrawMatineePreview( UViewport* Viewport )
{
	guard(DrawMatineePreview);

	ASceneManager* SM = (ASceneManager*)Viewport->MiscRes;
	if( !SM ) return;

	FLOAT Pct = Viewport->Actor->Misc1 / 10000.f;

	INT CurX = 1, CurY = 3;
	DrawActionBar( Viewport, CurX, CurY, SM );

	// TIME MARKER
	FLOAT XPos = (Viewport->SizeX-2) * Pct;
	Viewport->Canvas->DrawIcon( GMatineeTools.TimeMarker,
		CurX + XPos - 3, CurY - 3,
		GMatineeTools.TimeMarker->MaterialUSize(), GMatineeTools.TimeMarker->MaterialVSize(),
		0.0, FPlane(1.,1.,1.,1.), FPlane(0,0,0,0)/*, PF_Masked */ );	//!!MAT

	unguard;
}

void DrawMatineeScenes( UViewport* Viewport )
{
	guard(DrawMatineeScenes);

	GMatineeTools.SceneScrollMax = 0;
	INT CurX = 4, CurY = 4 - GMatineeTools.SceneScrollPos, Scenes;

	Scenes = 0;
	for( INT x = 0 ; x < Viewport->Actor->GetLevel()->Actors.Num() ; ++x )
	{
		ASceneManager* SM = Cast<ASceneManager>(Viewport->Actor->GetLevel()->Actors(x));
		if( SM )
		{
			PUSH_HIT(Viewport,HMatineeScene(SM));

			Viewport->Canvas->DrawPattern( GUnrealEd->BkgndHi,
				CurX-2, CurY-2,
				Viewport->SizeX, FMatineeTools::MAT_SCENE_HEIGHT+4,
				1.0, 0.0, 0.0, 0.0f, FPlane(.75,.75,.75,1), FPlane(0,0,0,0) );

				Viewport->Canvas->DrawPattern( GUnrealEd->BkgndHi,
					CurX, CurY,
					Viewport->SizeX-6, FMatineeTools::MAT_SCENE_HEIGHT,
					1.0, 0.0, 0.0, 0.0f, GMatineeTools.GetCurrent() == SM ? FPlane(1.,1.,1.,1) : FPlane(0,0,0,1), FPlane(0,0,0,0) );

			// ICON
			//!!MAT
			Viewport->Canvas->DrawIcon( SM->Texture,
				CurX, CurY,
				SM->Texture->MaterialUSize(), SM->Texture->MaterialVSize(),
				0.0, FPlane(1.,1.,1.,1.), FPlane(0,0,0,0)/*, PF_Masked*/ );

			CurX += SM->Texture->MaterialUSize() + 4;

			// TEXT
			INT FontWidth, FontHeight;
			Viewport->Canvas->WrappedStrLenf( Viewport->Canvas->SmallFont, FontWidth, FontHeight, TEXT("M") );

			DrawField( Viewport, CurX, CurY, FColor(192,192,192), TEXT("Name : "), FColor(0,255,255), *SM->Tag );

			CurY += FontHeight;
			DrawField( Viewport, CurX, CurY, FColor(192,192,192), TEXT("Total Time : "), FColor(0,255,255), TEXT("%1.1f Seconds"), SM->GetTotalSceneTime() );

			POP_HIT(Viewport);

			CurX = 4;
			CurY = (4 - GMatineeTools.SceneScrollPos) + ((Scenes+1)*(FMatineeTools::MAT_SCENE_HEIGHT+4));

			DrawActionBar( Viewport, CurX, CurY-6, SM );

			Scenes++;

			GMatineeTools.SceneScrollMax += FMatineeTools::MAT_SCENE_HEIGHT+4;
		}
	}

	GMatineeTools.SceneScrollMax -= FMatineeTools::MAT_SCENE_HEIGHT+4;

	unguard;
}

void DrawMatineeActions( UViewport* Viewport )
{
	guard(DrawMatineeActions);

	ASceneManager* SM = GMatineeTools.GetCurrent();
	if( !SM ) return;

	UTexture* White = GUnrealEd->Level->GetLevelInfo()->WhiteSquareTexture;

	GMatineeTools.ActionScrollMax = 0;
	INT CurX = 4, CurY = 4 - GMatineeTools.ActionScrollPos;

	FLOAT CurrentTime = 0.f, TotalTime = SM->GetTotalSceneTime();
	for( INT x = 0 ; x < SM->Actions.Num() ; ++x )
	{
		UMatAction* Action = SM->Actions(x);

		PUSH_HIT(Viewport,HMatineeAction(SM,Action));

		Viewport->Canvas->DrawPattern( GUnrealEd->BkgndHi,
			CurX-2, CurY-2,
			Viewport->SizeX, FMatineeTools::MAT_ACTION_HEIGHT+4,
			1.0, 0.0, 0.0, 0.0f, FPlane(.75,.75,.75,1), FPlane(0,0,0,0) );

			Viewport->Canvas->DrawPattern( GUnrealEd->BkgndHi,
				CurX, CurY,
				Viewport->SizeX-6, FMatineeTools::MAT_ACTION_HEIGHT,
				1.0, 0.0, 0.0, 0.0f, GMatineeTools.GetCurrentAction() == Action ? FPlane(1.,1.,1.,1) : FPlane(0,0,0,1), FPlane(0,0,0,0) );

		// ICON
		UTexture* Icon = NULL;
		if( Cast<UActionMoveCamera>(Action) )
			Icon = GMatineeTools.ActionCamMove;
		else
			Icon = GMatineeTools.ActionCamPause;

		Viewport->Canvas->DrawIcon( Icon,
			CurX, CurY-4 + ((FMatineeTools::MAT_ACTION_HEIGHT - Icon->MaterialUSize())/2),
			Icon->MaterialUSize(), Icon->MaterialVSize(),
			0.0, FPlane(1.,1.,1.,1.), FPlane(0,0,0,0)/*, PF_Masked*/ );	//!!MAT

		// PATH
		if( Cast<UActionMoveCamera>(Action) )
		{
			UTexture* Path = Cast<UActionMoveCamera>(Action)->PathStyle==PATHSTYLE_Linear ? GMatineeTools.PathLinear : GMatineeTools.PathBezier;
			Viewport->Canvas->DrawIcon( Path,
				Viewport->SizeX - Path->MaterialUSize() - 4, CurY,
				Path->MaterialUSize(), Path->MaterialVSize(),
				0.0, FPlane(1.,1.,1.,1.), FPlane(0,0,0,0)/*, PF_Masked*/ );	//!!MAT
		}

		// SUBACTION INDICATOR
		if( Action->SubActions.Num() )
		{
			Viewport->Canvas->DrawIcon( GMatineeTools.SubActionIndicator,
				Viewport->SizeX - GMatineeTools.SubActionIndicator->MaterialUSize() - 4, CurY+GMatineeTools.SubActionIndicator->MaterialUSize(),
				GMatineeTools.SubActionIndicator->MaterialUSize(), GMatineeTools.SubActionIndicator->MaterialVSize(),
				0.0, FPlane(1.,1.,1.,1.), FPlane(0,0,0,0)/*, PF_Masked */); //!!MAT
		}

		CurX += Icon->MaterialUSize() + 4;

		// TEXT
		INT FontWidth, FontHeight;
		Viewport->Canvas->WrappedStrLenf( Viewport->Canvas->SmallFont, FontWidth, FontHeight, TEXT("M") );

		DrawField( Viewport, CurX, CurY, FColor(192,192,192), TEXT("Duration : "), FColor(0,255,255), TEXT("%1.1f seconds"), Action->Duration );

		CurY += FontHeight*2;
		if( Action->Comment.Len() )
			DrawField( Viewport, CurX, CurY, FColor(0,0,0), TEXT(""), FColor(0,255,255), TEXT("%s"), Action->Comment );

		CurX = 4;

		// Time bar
		CurY = (4 - GMatineeTools.ActionScrollPos) + ((x+1)*(FMatineeTools::MAT_ACTION_HEIGHT+4));
		CurY -= 9;
		Viewport->Canvas->DrawPattern( White,
			CurX, CurY,
			Viewport->SizeX-6, 4,
			1.0, 0.0, 0.0, 0.0f, FPlane(.5f,.5f,.5f,1), FPlane(0,0,0,0) );

		FLOAT StartPct = CurrentTime / (TotalTime ? TotalTime : 1),
			DurationPct = Action->Duration / (TotalTime ? TotalTime : 1);

		INT Sz = (Viewport->SizeX-6)*DurationPct;
		Sz = Max<INT>( Sz, 1 );

		Viewport->Canvas->DrawPattern( White,
			CurX+((Viewport->SizeX-6)*StartPct), CurY,
			Sz, 4,
			1.0, 0.0, 0.0, 0.0f, FPlane(1.f,1.f,1.f,1), FPlane(0,0,0,0) );

		CurrentTime += Action->Duration;
		POP_HIT(Viewport);

		CurY = (4 - GMatineeTools.ActionScrollPos) + ((x+1)*(FMatineeTools::MAT_ACTION_HEIGHT+4));

		GMatineeTools.ActionScrollMax += FMatineeTools::MAT_ACTION_HEIGHT+4;
	}

	GMatineeTools.ActionScrollMax -= FMatineeTools::MAT_ACTION_HEIGHT+4;

	unguard;
}

void DrawMatineeSubActions( UViewport* Viewport )
{
	guard(DrawMatineeSubActions);

	UMatAction* MA = GMatineeTools.GetCurrentAction();
	if( !MA ) return;

	UTexture* White = GUnrealEd->Level->GetLevelInfo()->WhiteSquareTexture;

	GMatineeTools.SubActionScrollMax = 0;
	INT CurX = 4, CurY = 4 - GMatineeTools.SubActionScrollPos;

	FLOAT TotalTime = MA->Duration;
	for( INT x = 0 ; x < MA->SubActions.Num() ; ++x )
	{
		UMatSubAction* SubAction = MA->SubActions(x);

		PUSH_HIT(Viewport,HMatineeSubAction(SubAction,MA));

		Viewport->Canvas->DrawPattern( GUnrealEd->BkgndHi,
			CurX-2, CurY-2,
			Viewport->SizeX, FMatineeTools::MAT_SUBACTION_HEIGHT+4,
			1.0, 0.0, 0.0, 0.0f, FPlane(.75,.75,.75,1), FPlane(0,0,0,0) );

			Viewport->Canvas->DrawPattern( GUnrealEd->BkgndHi,
				CurX, CurY,
				Viewport->SizeX-6, FMatineeTools::MAT_SUBACTION_HEIGHT,
				1.0, 0.0, 0.0, 0.0f, GMatineeTools.GetCurrentSubAction() == SubAction ? FPlane(1.,1.,1.,1) : FPlane(0,0,0,1), FPlane(0,0,0,0) );

		// ICON
		Viewport->Canvas->DrawIcon( SubAction->Icon,
			CurX, CurY-4 + ((FMatineeTools::MAT_SUBACTION_HEIGHT - SubAction->Icon->MaterialVSize())/2),
			SubAction->Icon->MaterialUSize(), SubAction->Icon->MaterialVSize(),
			0.0, FPlane(1.,1.,1.,1.), FPlane(0,0,0,0)/*, PF_Masked */);//!!MAT

		CurX += SubAction->Icon->MaterialUSize() + 4;

		// TEXT
		INT FontWidth, FontHeight;
		Viewport->Canvas->WrappedStrLenf( Viewport->Canvas->SmallFont, FontWidth, FontHeight, TEXT("M") );

		DrawField( Viewport, CurX, CurY, FColor(192,192,192), TEXT("Delay/Dur : "), FColor(0,255,255), TEXT("%1.1f / %1.1f Sec"), SubAction->Delay, SubAction->Duration );

		// Subaction specific information
		if( Cast<USubActionFade>(SubAction) )
		{
			USubActionFade* SAF = Cast<USubActionFade>(SubAction);
			CurY += FontHeight + FontHeight;
			DrawField( Viewport, CurX, CurY, FColor(192,192,192), TEXT("Fade : "), FColor(0,255,255), TEXT("%d, %d, %d"), SAF->FadeColor.R, SAF->FadeColor.G, SAF->FadeColor.B );
			CurY += FontHeight;
		}
		else if( Cast<USubActionFOV>(SubAction) )
		{
			USubActionFOV* SAF = Cast<USubActionFOV>(SubAction);
			CurY += FontHeight + FontHeight;
			DrawField( Viewport, CurX, CurY, FColor(192,192,192), TEXT("FOV : "), FColor(0,255,255), TEXT("%1.1f / %1.1f"), SAF->FOV.Min, SAF->FOV.Max );
			CurY += FontHeight;
		}
		else if( Cast<USubActionGameSpeed>(SubAction) )
		{
			USubActionGameSpeed* SAGS = Cast<USubActionGameSpeed>(SubAction);
			CurY += FontHeight + FontHeight;
			DrawField( Viewport, CurX, CurY, FColor(192,192,192), TEXT("Speed : "), FColor(0,255,255), TEXT("%1.1f / %1.1f"), SAGS->GameSpeed.Min, SAGS->GameSpeed.Max );
			CurY += FontHeight;
		}
		else if( Cast<USubActionOrientation>(SubAction) )
		{
			USubActionOrientation* SAO = Cast<USubActionOrientation>(SubAction);
			CurY += FontHeight;
			DrawField( Viewport, CurX, CurY, FColor(192,192,192), TEXT("Orientation : "), FColor(0,255,255), *GMatineeTools.GetOrientationDesc( SAO->CamOrientation.CamOrientation ) );

			// If this is the current subaction, draw a line from the actions interpolation point to the "Look At" actor.
			if( GMatineeTools.GetCurrentSubAction() == SubAction && MA->IntPoint && SAO->CamOrientation.LookAt )
			{
				FLineBatcher	LineBatcher(Viewport->RI);
				LineBatcher.DrawLine( MA->IntPoint->Location, SAO->CamOrientation.LookAt->Location, FColor( 128,255,128 ) );
			}

			CurY += FontHeight;
		}
		else if( Cast<USubActionTrigger>(SubAction) )
		{
			USubActionTrigger* SAT = Cast<USubActionTrigger>(SubAction);
			CurY += FontHeight + FontHeight;
			DrawField( Viewport, CurX, CurY, FColor(192,192,192), TEXT("Event : "), FColor(0,255,255), TEXT("%s"), *SAT->EventName );
			CurY += FontHeight;
		}
		else if( Cast<USubActionSceneSpeed>(SubAction) )
		{
			USubActionSceneSpeed* SASS = Cast<USubActionSceneSpeed>(SubAction);
			CurY += FontHeight + FontHeight;
			DrawField( Viewport, CurX, CurY, FColor(192,192,192), TEXT("Speed : "), FColor(0,255,255), TEXT("%1.1f / %1.1f"), SASS->SceneSpeed.Min, SASS->SceneSpeed.Max );
			CurY += FontHeight;
		}

		// Time bar
		CurX = 4;
		CurY = (4 - GMatineeTools.SubActionScrollPos) + ((x+1)*(FMatineeTools::MAT_SUBACTION_HEIGHT+4));
		CurY -= 9;
		Viewport->Canvas->DrawPattern( White,
			CurX, CurY,
			Viewport->SizeX-6, 4,
			1.0, 0.0, 0.0, 0.0f, FPlane(.5f,.5f,.5f,1), FPlane(0,0,0,0) );

		FLOAT StartPct = SubAction->Delay / (TotalTime ? TotalTime : 1),
			DurationPct = SubAction->Duration / (TotalTime ? TotalTime : 1);

		INT Sz = (Viewport->SizeX-6)*DurationPct;
		Sz = Max<INT>( Sz, 1 );

		Viewport->Canvas->DrawPattern( White,
			CurX+((Viewport->SizeX-6)*StartPct), CurY,
			Sz, 4,
			1.0, 0.0, 0.0, 0.0f, FPlane(1.f,1.f,1.f,1), FPlane(0,0,0,0) );

		if( Cast<USubActionOrientation>(SubAction) )
		{
			USubActionOrientation* SAO = Cast<USubActionOrientation>(SubAction);

			// "EaseInTime" shows up on top of the regular time bar
			FLOAT EaseInTimePct = SAO->CamOrientation.EaseInTime / (TotalTime ? TotalTime : 1);
			INT Sz = (Viewport->SizeX-6)*EaseInTimePct;
			Viewport->Canvas->DrawPattern( White,
				CurX+((Viewport->SizeX-6)*StartPct)+1, CurY,
				Sz, 4,
				1.0, 0.0, 0.0, 0.0f, FPlane(0.75f,0.75f,.75f,1), FPlane(0,0,0,0) );
		}

		POP_HIT(Viewport);

		CurY = (4 - GMatineeTools.SubActionScrollPos) + ((x+1)*(FMatineeTools::MAT_SUBACTION_HEIGHT+4));

		GMatineeTools.SubActionScrollMax += FMatineeTools::MAT_SUBACTION_HEIGHT+4;
	}

	GMatineeTools.SubActionScrollMax -= FMatineeTools::MAT_SUBACTION_HEIGHT+4;

	unguard;
}

/*-----------------------------------------------------------------------------
   Viewport frame drawing.
-----------------------------------------------------------------------------*/

#if 1
//!! hack to avoid modifying EditorEngine.uc, and rebuilding Editor.u
// (see UnEdRend.cpp for further details).
extern FLOAT EdClipZ;
#endif

//int CDECL IntPointCompare(const void *A, const void *B)
//{
//	return (*(AInterpolationPoint**)A)->Position - (*(AInterpolationPoint**)B)->Position;
//}

//
// Draw the camera view.
//
void UUnrealEdEngine::Draw( UViewport* Viewport, UBOOL Blit, BYTE* HitData, INT* HitCount )
{
	FVector			OriginalLocation;
	FRotator		OriginalRotation;
	DWORD			ShowFlags=0;
	guard(UUnrealEdEngine::Draw);

	APlayerController* Actor = Viewport->Actor;
	ShowFlags = Actor->ShowFlags;

	// Lock the camera.

	UBOOL	ClearScreen = 0;
	FColor	ClearColor(0,0,0,0);

#ifdef WITH_LIPSINC
	if( Actor->RendMap==REN_MeshView || Actor->RendMap==REN_Animation || Actor->RendMap==REN_LIPSinc )
#else
	if( Actor->RendMap==REN_MeshView || Actor->RendMap==REN_Animation )
#endif
		ClearScreen = 1;

	if(Viewport->IsOrtho() || Viewport->IsWire() || !(Viewport->Actor->ShowFlags & SHOW_Backdrop))
	{
		if(Viewport->Actor->RendMap == REN_StaticMeshBrowser
				|| Viewport->Actor->RendMap == REN_MeshView
				|| Viewport->Actor->RendMap == REN_Animation

#ifdef WITH_LIPSINC
				|| Viewport->Actor->RendMap == REN_LIPSinc
#endif

				|| Viewport->Actor->RendMap == REN_MaterialEditor 
				|| Viewport->Actor->RendMap == REN_TexView )
			ClearColor = FColor(64,64,64);
		else
			ClearColor = Viewport->IsOrtho() ? C_OrthoBackground : C_WireBackground;

		ClearScreen = 1;
	}

	if(Viewport->Lock(HitData,HitCount))
	{
		Viewport->Canvas->Update();

		Viewport->RI->Clear(ClearScreen,ClearColor,1,1.0f,1,~DEPTH_COMPLEXITY_MASK(Viewport));

		switch( Actor->RendMap )
		{
			case REN_MaterialEditor:
			{
				guard(REN_MaterialEditor);
				DrawMaterialEditor( Viewport );
				unguard;
				break;
			}
			case REN_TexBrowser:
			case REN_TexBrowserUsed:
			case REN_TexBrowserMRU:
			{
				guard(REN_TexBrowser);
				Viewport->Actor->bHiddenEd = 1;
				Viewport->Actor->bHiddenEdGroup = 1;
				Viewport->Actor->bHidden   = 1;
				DrawViewerBackground( Viewport );
				DrawTextureBrowser( Viewport );
				unguard;
				break;
			}
			case REN_StaticMeshBrowser:
			{
				guard(REN_StaticMeshBrowser);

				DrawStaticMeshBrowser(Viewport);

				unguard;
				break;
			}
			case REN_TerrainHeightmap:
			{
				guard(REN_TerrainHeightmap);
				Viewport->Actor->bHiddenEd = 1;
				Viewport->Actor->bHiddenEdGroup = 1;
				Viewport->Actor->bHidden   = 1;
				DrawTerrainHeightmap( Viewport );
				unguard;
				break;
			}
			case REN_MatineeScenes:
			{
				guard(REN_MatineeScenes);
				Viewport->Actor->bHiddenEd = 1;
				Viewport->Actor->bHiddenEdGroup = 1;
				Viewport->Actor->bHidden   = 1;
				DrawMatineeScenes( Viewport );
				unguard;
				break;
			}
			case REN_MatineeActions:
			{
				guard(REN_MatineeActions);
				Viewport->Actor->bHiddenEd = 1;
				Viewport->Actor->bHiddenEdGroup = 1;
				Viewport->Actor->bHidden   = 1;
				DrawMatineeActions( Viewport );
				unguard;
				break;
			}
			case REN_MatineeSubActions:
			{
				guard(REN_MatineeSubActions);
				Viewport->Actor->bHiddenEd = 1;
				Viewport->Actor->bHiddenEdGroup = 1;
				Viewport->Actor->bHidden   = 1;
				DrawMatineeSubActions( Viewport );
				unguard;
				break;
			}
			case REN_TerrainLayers:
			{
				guard(REN_TerrainLayers);
				Viewport->Actor->bHiddenEd = 1;
				Viewport->Actor->bHiddenEdGroup = 1;
				Viewport->Actor->bHidden   = 1;
				DrawTerrainLayers( Viewport );
				unguard;
				break;
			}
			case REN_TerrainDecoLayers:
			{
				guard(REN_DecoLayers);
				Viewport->Actor->bHiddenEd = 1;
				Viewport->Actor->bHiddenEdGroup = 1;
				Viewport->Actor->bHidden   = 1;
				DrawTerrainDecoLayers( Viewport );
				unguard;
				break;
			}
			case REN_Animation:
			{
				GBrowserAnimation->Draw( Viewport );
				break;
			}

#ifdef WITH_LIPSINC
			case REN_LIPSinc:
				{
					GBrowserLIPSinc->Draw( Viewport );
					break;
				}
#endif

			case REN_MeshView:
			{
				guard(REN_MeshView);

				UMesh* Mesh = CurrentMesh;

				if( !Mesh || !Mesh->IsA(UVertMesh::StaticClass()))
					break;
			
				UMeshInstance* MeshInstance = Mesh->MeshGetInstance(Actor);
				if( !MeshInstance )
					break;

				FActorSceneNode	SceneNode(Viewport,&Viewport->RenderTarget,Viewport->Actor,Viewport->Actor,Viewport->Actor->Location,Viewport->Actor->Rotation,Viewport->Actor->FovAngle);
				FVector			SavedLocation = Viewport->Actor->Location;
				FRotator		SavedRotation = Viewport->Actor->Rotation;

				Viewport->Actor->SetDrawType(DT_Mesh);
				Viewport->Actor->Mesh = Mesh;
				Viewport->Actor->Location = FVector(0,0,0);
				Viewport->Actor->Rotation = FRotator(0,0,0);
				Viewport->Actor->AmbientGlow	= 255; // Rendering unlit meshes.

				// Draw the wire grid.
				GUnrealEd->DrawWireBackground(&SceneNode);
				Viewport->RI->Clear(0,FColor(0,0,0),1,1.0f);

				FName DemoAnimSequence;			
				
				static INT LastUpdateCycles;
				static FLOAT DemoAnimFrame=0.f;
				static FLOAT OldAnimFrame=0.f;
				INT   CurrentCycles = appCycles();
				FLOAT DeltaTime = (CurrentCycles - LastUpdateCycles)*GSecondsPerCycle;
				LastUpdateCycles = CurrentCycles;
				
				// Update the animation...
				OldAnimFrame = DemoAnimFrame;
				if( MeshInstance && ( Actor->Misc1 < MeshInstance->GetAnimCount()) && ( Actor->Misc1 >= 0 ) )
				{
					HMeshAnim ShowAnim = MeshInstance->GetAnimIndexed( Actor->Misc1 );
					DemoAnimSequence = MeshInstance->AnimGetName(ShowAnim);
				}
				else
				{
					DemoAnimSequence = NAME_None;
				}

				HMeshAnim Seq = MeshInstance->GetAnimNamed( DemoAnimSequence );
				FLOAT NumFrames = Seq ? MeshInstance->AnimGetFrameCount(Seq) : 1.0;
				
				if ( !(ShowFlags & SHOW_RealTime) ) //SHOW_Frame - Forced frame with the slider.
				{
					DemoAnimFrame = Clamp<FLOAT>( ((FLOAT)Actor->Misc2 / 10000.0f), 0.f, 1.f ); 
					MeshInstance->AnimForcePose( DemoAnimSequence, DemoAnimFrame, DemoAnimFrame-OldAnimFrame, 0 );
				}
				else if (ShowFlags & SHOW_RealTime) // Auto-looping, play-button-active updating.
				{
					FLOAT Rate = Seq ? MeshInstance->AnimGetRate(Seq) / NumFrames : 1.0;
					FLOAT DeltaFrame = Clamp<FLOAT>( (FLOAT)Rate*DeltaTime, 0.f, 1.f);
					DemoAnimFrame += DeltaFrame;
					DemoAnimFrame -= appFloor(DemoAnimFrame); // Loop
					MeshInstance->AnimForcePose( DemoAnimSequence, DemoAnimFrame, DeltaFrame, 0 );
				}

							
				// Draw the actor.
				SceneNode.Render(Viewport->RI);
				
				// Draw the axis indicator.
				if( GUnrealEd->UseAxisIndicator )
					GUnrealEd->edDrawAxisIndicator(&SceneNode);

				// Print the name of the static mesh at the top of the viewport.
				Viewport->Canvas->CurX = 0;
				Viewport->Canvas->CurY = 0;
				Viewport->Canvas->Color = FColor(255,255,255);
				FString Text = Mesh ? Mesh->GetPathName() : TEXT("No Animating Mesh");
				Viewport->Canvas->WrappedPrintf
				(
					Viewport->Canvas->SmallFont,
					1,
 					*Text
				);

				// Print the current animation frame.
				Viewport->Canvas->CurX = 0;
				Viewport->Canvas->CurY = Viewport->Canvas->CurY+10; 
				Viewport->Canvas->Color = FColor(255,255,255);
				//FString Text = Mesh ? Mesh->GetPathName() : TEXT("No Animating Mesh");


				FString FrameText = Viewport->MiscRes ? Viewport->MiscRes->GetName() : TEXT(" MISCRES ERROR ");
				if( Viewport->Actor->Misc1 < 0) FrameText = TEXT("REFPOSE");

				Viewport->Canvas->WrappedPrintf
				(
					Viewport->Canvas->SmallFont,
					1,
					TEXT("[%s], Seq %i, Frame %5.2f / %d"),
					*FrameText,
					Viewport->Actor->Misc1 >=0 ? Viewport->Actor->Misc1 : 0,
					(FLOAT)(DemoAnimFrame * NumFrames), (INT)NumFrames-1
				);


				DemoAnimSequence = NAME_None;


				// Reset the viewport.
				Viewport->Actor->SetDrawType(DT_Sprite);
				Viewport->Actor->Mesh = NULL;
				Viewport->Actor->Location = SavedLocation;
				Viewport->Actor->Rotation = SavedRotation;

				unguard;
				break;
			}
			case REN_Prefab:
			case REN_PrefabCompiled:
			{
				guard(REN_Prefab);

				if( GPrefabLevel )
				{
					Viewport->Actor->XLevel = GPrefabLevel;
					Viewport->Actor->bHiddenEd = 1;
					Viewport->Actor->bHiddenEdGroup = 1;
					Viewport->Actor->bHidden = 1;

					// Render all actors in the prefab level

					FCameraSceneNode	SceneNode(Viewport,&Viewport->RenderTarget,Viewport->Actor,Viewport->Actor->Location,Viewport->Actor->Rotation,Viewport->Actor->FovAngle);
	
					DrawWireBackground(&SceneNode);
					Viewport->RI->Clear(0);
	
					Viewport->Actor->RendMap = REN_Wire;
					SceneNode.Render(Viewport->RI);
					Viewport->Actor->RendMap = REN_Prefab;
	
					// Put the name of the prefab at the top of the viewport.
	
					Viewport->Canvas->CurX = 0;
					Viewport->Canvas->CurY = 0;
					Viewport->Canvas->Color = FColor(255,255,255);
	
					FString Title = GCurrentPrefab ? GCurrentPrefab->GetPathName() : TEXT("No prefab");
	
					Viewport->Canvas->WrappedPrintf
					(
						Viewport->Canvas->SmallFont,
						1,
	 					*Title
					);

					edDrawAxisIndicator(&SceneNode);
				}

				unguard;
				break;
			}
			case REN_TexView:
			{
				WDlgTexProp* Dlg = (WDlgTexProp*)Viewport->Actor->Misc2;
				if( Dlg )
					Dlg->Render_TexView( Viewport );
				else
				{
					UMaterial* Material = Cast<UMaterial>( Viewport->MiscRes );
					if( Material )
						Viewport->Canvas->DrawIcon( Material, 0, 0, Viewport->SizeX, Viewport->SizeY, 0.0f, FPlane(1,1,1,1), FPlane(0,0,0,0) );
				}
				break;
			}
			default:
			{
				FPlayerSceneNode	SceneNode(Viewport,&Viewport->RenderTarget,Viewport->Actor,Viewport->Actor->Location,Viewport->Actor->Rotation,Viewport->Actor->FovAngle);

				// Hide the camera actor for ortho viewports.

				Viewport->Actor->bHiddenEd = Viewport->Actor->bHiddenEdGroup = Viewport->IsOrtho();

				if(Viewport->IsOrtho() || Viewport->IsWire() || !(Viewport->Actor->ShowFlags & SHOW_Backdrop))
				{
					// Draw the grid, and clear the Z-buffer.

					DrawWireBackground(&SceneNode);
					Viewport->RI->Clear(0,FColor(0,0,0),1,1.0f,1,~DEPTH_COMPLEXITY_MASK(Viewport));
				}

				// Draw the level.
	
				PUSH_HIT(Viewport,HCoords(&SceneNode));

				SceneNode.Render(Viewport->RI);

				// Draw the axis indicator.

				if( GUnrealEd->UseAxisIndicator )
					edDrawAxisIndicator(&SceneNode);

				// Draw the selected vertex.

				if( GPivotShown && GUnrealEd->Mode != EM_VertexEdit )
				{
					FCanvasUtil	CanvasUtil(&SceneNode.Viewport->RenderTarget,Viewport->RI);

					FVector	V = CanvasUtil.ScreenToCanvas.TransformFVector(SceneNode.Project(GSnappedLocation));

					INT Sz = Viewport->bShowLargeVertices ? 10 : 4;

					CanvasUtil.DrawPoint(V.X - 1,V.Y - 1,V.X + 1,V.Y + 1,V.Z,C_BrushWire);
					CanvasUtil.DrawPoint(V.X,V.Y - Sz,V.X,V.Y + Sz,V.Z,C_BrushWire);
					CanvasUtil.DrawPoint(V.X - Sz,V.Y,V.X + Sz,V.Y,V.Z,C_BrushWire);
					CanvasUtil.Flush();
				}

				// Draw all paths.

				if(Viewport->Actor->ShowFlags & SHOW_Paths)
				{
					FLineBatcher	LineBatcher(Viewport->RI);

					for ( ANavigationPoint *Nav=Viewport->Actor->Level->NavigationPointList; Nav; Nav=Nav->nextNavigationPoint )
					{
						if ( !Nav->bHideEditorPaths )
						{
							for( INT i=0; i<Nav->PathList.Num(); i++ )
							{
								UReachSpec* ReachSpec = Nav->PathList( i );
								if( ReachSpec->Start && ReachSpec->End && !ReachSpec->End->bHideEditorPaths )
								{
									LineBatcher.DrawLine(
										ReachSpec->Start->Location + FVector(0,0,8),
										ReachSpec->End->Location,
										ReachSpec->PathColor()
										);

									// make arrowhead to show L.D direction of path
									FVector Dir = ReachSpec->End->Location - ReachSpec->Start->Location - FVector(0,0,8.f);
									Dir.Normalize();

									LineBatcher.DrawLine(
										ReachSpec->End->Location - 12 * Dir + FVector(0,0,3),
										ReachSpec->End->Location - 6 * Dir,
										ReachSpec->PathColor()
										);

									LineBatcher.DrawLine(
										ReachSpec->End->Location - 12 * Dir - FVector(0,0,3),
										ReachSpec->End->Location - 6 * Dir,
										ReachSpec->PathColor()
										);

									LineBatcher.DrawLine(
										ReachSpec->End->Location - 12 * Dir + FVector(0,0,3),
										ReachSpec->End->Location - 12 * Dir - FVector(0,0,3),
										ReachSpec->PathColor()
										);
								}
							}
						}
					}
				}

				// If the user is doing a box selection, draw the box.

				if(Viewport->IsOrtho() && GbIsBoxSel)
				{
					FLineBatcher	LineBatcher(Viewport->RI);
					LineBatcher.DrawBox(FBox(GBoxSelStart,GBoxSelEnd),C_BrushWire);
				}

				// If the user is using the snap scale tool, show the bounding box.

				if( GbIsSnapScaleBox )
				{
					FLineBatcher	LineBatcher(Viewport->RI);
					LineBatcher.DrawBox(FBox(GSnapScaleStartSnapped,GSnapScaleEndSnapped),C_BrushWire);
				}

				// Draw the measuring tool.
	
				if(Viewport->IsOrtho() && GbIsMeasuring)
				{
					FCanvasUtil		CanvasUtil(&Viewport->RenderTarget,Viewport->RI);
	
					// Draw the anchor points

					FVector	Start = CanvasUtil.ScreenToCanvas.TransformFVector(SceneNode.Project(GMeasureStart)),
							End = CanvasUtil.ScreenToCanvas.TransformFVector(SceneNode.Project(GMeasureEndSnapped));

					CanvasUtil.DrawPoint(Start.X - 1,Start.Y - 1,Start.X + 1,Start.Y + 1,Start.Z,C_BrushWire);
					CanvasUtil.DrawPoint(End.X - 1,End.Y - 1,End.X + 1,End.Y + 1,End.Z,C_BrushWire);

					// Draw the measure line.

					CanvasUtil.DrawLine(Start.X,Start.Y,End.X,End.Y,C_BrushWire);

					CanvasUtil.Flush();

					// Figure out where the middle of the line is on the screen and display the distance at that location
	
					FVector	MidPoint = (Start + End) / 2.0f;
					FLOAT	Dist = (GMeasureEndSnapped - GMeasureStart).Size();
	
					FString	TextLabel = FString::Printf(TEXT("%1.f"),Dist);
					INT		LabelWidth,
							LabelHeight;

					Viewport->Canvas->WrappedStrLenf(Viewport->Canvas->SmallFont,LabelWidth,LabelHeight,*TextLabel);

					Viewport->Canvas->DrawPattern(
						GUnrealEd->BkgndHi,
						MidPoint.X - LabelWidth / 2 - 2,
						MidPoint.Y - LabelHeight / 2 - 2,
						LabelWidth + 4,
						LabelHeight + 4,
						1.0f,
						0.0f,
						0.0f,
						1.0f,
						FPlane(0,0,0,1),
						FPlane(0,0,0,0)
						);

					Viewport->Canvas->SetClip(MidPoint.X - LabelWidth / 2,MidPoint.Y - LabelHeight / 2,LabelWidth,LabelHeight);
					Viewport->Canvas->WrappedPrintf(Viewport->Canvas->SmallFont,0,*TextLabel);
					Viewport->Canvas->SetClip(0,0,Viewport->SizeX,Viewport->SizeY);
				}

				// Draw the clip markers.

				// If the user is brush clipping, draw lines to show them what's going on.
				//
				TArray<AActor*>	ClipMarkers;

				// Gather a list of all the ClipMarkers in the level.
				//
				for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; i++ )
				{
					AActor* pActor = GUnrealEd->Level->Actors(i);
					if( pActor && pActor->IsA(AClipMarker::StaticClass()) )
						ClipMarkers.AddItem( pActor );
				}
	
				if( ClipMarkers.Num() > 1 )
				{
					FLineBatcher	LineBatcher(Viewport->RI);

					// Draw a connecting line between them all.
					//
					for( INT x = 1 ; x < ClipMarkers.Num() ; x++ )
						LineBatcher.DrawLine(ClipMarkers(x - 1)->Location,ClipMarkers(x)->Location,C_BrushWire);
	
					// Draw an arrow that shows the direction of the clipping plane.  This arrow should
					// appear halfway between the first and second markers.
					//
					FVector vtx1, vtx2, vtx3;
					FPoly NormalPoly;
					UBOOL bDrawOK = 1;
	
					vtx1 = ClipMarkers(0)->Location;
					vtx2 = ClipMarkers(1)->Location;
	
					if( ClipMarkers.Num() == 3 )
					{
						// If we have 3 points, just grab the third one to complete the plane.
						//
						vtx3 = ClipMarkers(2)->Location;
					}
					else
					{
						// If we only have 2 points, we will assume the third based on the viewport.
						// (With only 2 points, we can only render into the ortho viewports)
						//
						vtx3 = vtx1;
						if( Viewport->IsOrtho() )
						{
							switch( Viewport->Actor->RendMap )
							{
								case REN_OrthXY:
									vtx3.Z -= 64;
									break;
	
								case REN_OrthXZ:
									vtx3.Y -= 64;
									break;
	
								case REN_OrthYZ:
									vtx3.X -= 64;
									break;
							}
						}
						else
							bDrawOK = 0;
					}
	
					NormalPoly.NumVertices = 3;
					NormalPoly.Vertex[0] = vtx1;
					NormalPoly.Vertex[1] = vtx2;
					NormalPoly.Vertex[2] = vtx3;
	
					if( bDrawOK && !NormalPoly.CalcNormal(1) )
					{
						FVector Start = vtx1 + (( vtx2 - vtx1 ) / 2);
						LineBatcher.DrawLine(Start,Start + NormalPoly.Normal * 48,C_BrushWire.Plane());
					}
				}
	
				// Freeform polygon drawing.
	
				TArray<AActor*> PolyMarkers;
	
				// Gather a list of all the PolyMarkers in the level.
				//
				for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; i++ )
				{
					AActor* pActor = GUnrealEd->Level->Actors(i);
					if( pActor && pActor->IsA(APolyMarker::StaticClass()) )
						PolyMarkers.AddItem( pActor );
				}
	
				// Draw a connecting line between them all.
				//
				for( INT x = 1 ; x < PolyMarkers.Num() ; x++ )
					FLineBatcher(Viewport->RI).DrawLine(PolyMarkers(x - 1)->Location,PolyMarkers(x)->Location,C_BrushWire);

				// Draw selected vertices.

				if(Mode == EM_VertexEdit)
				{
					FCanvasUtil	CanvasUtil(&Viewport->RenderTarget,Viewport->RI);

					for(INT VertexIndex = 0;VertexIndex < VertexHitList.Num();VertexIndex++)
					{
						FVertexHit&	Vertex = VertexHitList(VertexIndex);
						FMatrix		LocalToWorld = Vertex.pBrush->LocalToWorld();
						FVector		VertexLocation = LocalToWorld.TransformFVector(Vertex.pBrush->Brush->Polys->Element(Vertex.PolyIndex).Vertex[Vertex.VertexIndex]);

						PUSH_HIT(Viewport,HBrushVertex(Vertex.pBrush,VertexLocation));

						FVector	V = CanvasUtil.ScreenToCanvas.TransformFVector(SceneNode.Project(VertexLocation));

						CanvasUtil.DrawPoint(V.X - 2,V.Y - 2,V.X + 2,V.Y + 2,V.Z,FColor(255,255,255));
						CanvasUtil.DrawPoint(V.X - 1,V.Y - 1,V.X + 1,V.Y + 1,V.Z,FColor(255,255,255));
						CanvasUtil.Flush();

						POP_HIT(Viewport);
					}
				}

				POP_HIT(Viewport);

				// If a matinee action is selected, we need to draw the line connecting it's interpolation points
				ASceneManager* SM = GMatineeTools.GetCurrent();

				if( SM )
				{
					FCanvasUtil CanvasUtil(&SceneNode.Viewport->RenderTarget,Viewport->RI);
					FLineBatcher LineBatcher( Viewport->RI );

					for( INT x = 1 ; x < SM->Actions.Num() ; ++x )
					{
						UMatAction *MA = GMatineeTools.GetPrevAction( SM, SM->Actions(x) ),
							*MANext = SM->Actions(x);

						PUSH_HIT(Viewport,HMatineeAction(SM,MANext));

						FColor Color = FColor(255,255,255);
						if( MANext->Duration == 0.f )
							Color = FColor(128,0,128);		// Actions that take no time are instant cuts, and are shown as purple lines
						if( MANext == GMatineeTools.GetCurrentAction() )
							Color = FColor(255,255,128);	// The current actions lines are highlighted in yellow

						for( INT x = 0 ; x < MANext->SampleLocations.Num()-1 ; ++x )
						{
							if( Viewport->Actor->ShowFlags&SHOW_MatPaths )
							{
								if( !x && Viewport->Actor->RendMap == REN_MatineePreview )
									LineBatcher.DrawBox( FBox( FVector(-8,-8,-8) + MANext->SampleLocations(x), FVector(8,8,8) + MANext->SampleLocations(x) ), Color );
								LineBatcher.DrawLine( MANext->SampleLocations(x), MANext->SampleLocations(x+1), Color );
							}
						}
						LineBatcher.Flush();

						POP_HIT(Viewport);

						// Draw any subactions inside of this action as little icons along the line
						// (we only do this for MoveCamera actions since with PauseCamera actions everything sits on top itself)
						if( Cast<UActionMoveCamera>(MA)
								&& (Viewport->Actor->RendMap==REN_MatineePreview && !SM->bIsRunning)
								&& GMatineeTools.GetCurrentAction()==MA )
						{
							//SM->SetSceneStartTime();
							SM->CurrentAction = MA;

							for( INT x = 0 ; x < MA->SubActions.Num() ; ++x )
							{
								UMatSubAction* MSA = MA->SubActions(x);

								FVector		L = SM->GetLocation( &MA->SampleLocations, MSA->PctStarting );

								FPlane		P = SceneNode.Project(L);
								FVector		C = CanvasUtil.ScreenToCanvas.TransformFVector(P);
								UTexture*	Icon = MA->SubActions(x)->Icon;

								PUSH_HIT(Viewport,HMatineeSubAction(MSA,MA));
								CanvasUtil.DrawTile(
									C.X - (Icon->MaterialUSize()/2),
									C.Y - (Icon->MaterialVSize()/2),
									C.X + (Icon->MaterialUSize()/2),
									C.Y + (Icon->MaterialVSize()/2),
									0,0,
									Icon->UClamp,Icon->VClamp,
									C.Z,
									//!!MAT
									Icon,/*PF_NoZTest|PF_Masked,*/GMatineeTools.GetCurrentSubAction()==MSA ? FColor(128,230,128) : FColor(255,255,255) );
								CanvasUtil.Flush();
								POP_HIT(Viewport);
							}
						}

						// Bezier handles (we only draw bezier handles for the current action - screen is too cluttered otherwise)
						if( Viewport->Actor->RendMap != REN_MatineePreview
								&& MANext == GMatineeTools.GetCurrentAction()
								&& GMatineeTools.GetPathStyle( MANext ) == PATHSTYLE_Bezier
								&& Viewport->Actor->ShowFlags&SHOW_MatPaths )
 						{
							FVector HandleLocation;

							if( MA->IntPoint )
							{
								UBOOL bSelected = ( BezierControlPointList.FindItemIndex( HBezierControlPoint(MA,0) ) != INDEX_NONE );
								PUSH_HIT(Viewport,HBezierControlPoint(MA,0));

								LineBatcher.DrawLine( MA->IntPoint->Location, MA->IntPoint->Location + MA->StartControlPoint, bSelected ? FColor(128,230,128) : FColor(0,255,255) );
								LineBatcher.Flush();

								HandleLocation = MA->IntPoint->Location + MA->StartControlPoint;
								FPlane		P = SceneNode.Project(HandleLocation);
								FVector		C = CanvasUtil.ScreenToCanvas.TransformFVector(P);

								CanvasUtil.DrawTile(
									C.X - (GMatineeTools.BezierHandle->MaterialUSize()/2),
									C.Y - (GMatineeTools.BezierHandle->MaterialVSize()/2),
									C.X + (GMatineeTools.BezierHandle->MaterialUSize()/2),
									C.Y + (GMatineeTools.BezierHandle->MaterialVSize()/2),
									0,0,
									GMatineeTools.BezierHandle->UClamp,GMatineeTools.BezierHandle->VClamp,
									C.Z,
									//!!MAT
									GMatineeTools.BezierHandle,/*PF_NoZTest|PF_Masked,*/bSelected ? FColor(128,230,128) : FColor(255,255,255) );
								CanvasUtil.Flush();

								POP_HIT(Viewport);
							}
							if( MANext->IntPoint )
							{
								UBOOL bSelected = ( BezierControlPointList.FindItemIndex( HBezierControlPoint(MANext,1) ) != INDEX_NONE );
								PUSH_HIT(Viewport,HBezierControlPoint(MANext,1));

								LineBatcher.DrawLine( MANext->IntPoint->Location, MANext->IntPoint->Location + MANext->EndControlPoint, bSelected ? FColor(128,230,128) : FColor(0,255,255) );
								LineBatcher.Flush();

								HandleLocation = MANext->IntPoint->Location + MANext->EndControlPoint;
								FPlane		P = SceneNode.Project(HandleLocation);
								FVector		C = CanvasUtil.ScreenToCanvas.TransformFVector(P);

								CanvasUtil.DrawTile(
									C.X - (GMatineeTools.BezierHandle->MaterialUSize()/2),
									C.Y - (GMatineeTools.BezierHandle->MaterialVSize()/2),
									C.X + (GMatineeTools.BezierHandle->MaterialUSize()/2),
									C.Y + (GMatineeTools.BezierHandle->MaterialVSize()/2),
									0,0,
									GMatineeTools.BezierHandle->UClamp,GMatineeTools.BezierHandle->VClamp,
									C.Z,
									//!!MAT
									GMatineeTools.BezierHandle,/*PF_NoZTest|PF_Masked,*/bSelected ? FColor(128,230,128) : FColor(255,255,255));
								CanvasUtil.Flush();

								POP_HIT(Viewport);
							}
						}
					}
				}

				// Draw extra information on top of the viewport if this is a matinee preview
				if( Actor->RendMap==REN_MatineePreview )
					DrawMatineePreview( Viewport );

				DECLARE_STATIC_UOBJECT( UFinalBlend, VertexBlend, {} );
				VertexBlend->Material = GUnrealEd->Level->GetLevelInfo()->LargeVertex;
				VertexBlend->TwoSided = 0;
				VertexBlend->ZTest = 0;
					
				FColor Color(255,255,255);
				if( Viewport->bShowLargeVertices )
					for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; i++ )
					{
						AStaticMeshActor* SM = Cast<AStaticMeshActor>(GUnrealEd->Level->Actors(i));
						if( SM && SM->bSelected && SM->StaticMesh )
						{
							FCanvasUtil	CanvasUtil(&Viewport->RenderTarget,Viewport->RI);
							FVector		NewLoc;
							FPlane		P;
							FVector		C;

							for( INT y = 0 ; y < SM->StaticMesh->VertexStream.Vertices.Num() ; ++y )
							{
								FStaticMeshVertex* vtx = &SM->StaticMesh->VertexStream.Vertices(y);

								NewLoc = SM->LocalToWorld().TransformFVector( vtx->Position );
								P = SceneNode.Project(NewLoc);
								C = CanvasUtil.ScreenToCanvas.TransformFVector(P);

								PUSH_HIT(Viewport,HActorVertex(SM,NewLoc));

								CanvasUtil.DrawTile(
									C.X - (VertexBlend->MaterialUSize()/2),
									C.Y - (VertexBlend->MaterialVSize()/2),
									C.X + (VertexBlend->MaterialUSize()/2),
									C.Y + (VertexBlend->MaterialVSize()/2),
									0,0,
									VertexBlend->MaterialUSize(),VertexBlend->MaterialVSize(),
									C.Z,
									VertexBlend,
									Color );
								CanvasUtil.Flush();
								POP_HIT(Viewport);
							}
						}
					}

			}
		}

        Viewport->Canvas->pCanvasUtil->Flush(); // sjs

		Viewport->Unlock();

		if(Blit)
			Viewport->Present();
	}

	unguardf(( TEXT("(Cam=%s,Flags=%i"), Viewport->GetName(), ShowFlags ));
}

/*-----------------------------------------------------------------------------
   Viewport mouse click handling.
-----------------------------------------------------------------------------*/

// Called when the user clicks one of the mouse buttons
void UUnrealEdEngine::Click
(
	UViewport*	Viewport, 
	DWORD		Buttons,
	FLOAT		MouseX,
	FLOAT		MouseY
)
{
	guard(UUnrealEdEngine::Click);

	GEdModeTools->bMouseHasMoved = 0;

	// Tell the current mode about the click
	//if( GEdModeTools->GetCurrentMode() ) GEdModeTools->GetCurrentMode()->MouseButtonDown( Viewport, Buttons, MouseX, MouseY );

	if( Buttons == MOUSE_LeftDouble )
		UnClick( Viewport, Buttons, MouseX, MouseY );

	unguard;
}

// Called when the user lets go of one of the mouse buttons
void UUnrealEdEngine::UnClick
(
	UViewport*	Viewport, 
	DWORD		Buttons,
	INT			MouseX,
	INT			MouseY
)
{
	guard(UUnrealEdEngine::UnClick);

	// Tell the current mode about the unclick
	//if( GEdModeTools->GetCurrentMode() ) GEdModeTools->GetCurrentMode()->MouseButtonUp( Viewport, Buttons, MouseX, MouseY );

	// If the mouse hasn't moved between the click and the unclick, do hit detection.
	if( !GEdModeTools->HasMouseMoved() )
	{
		// Set hit-test location.
		// WDM : can this hit box be tightened up?  Does it matter?
		Viewport->HitX  = Clamp(MouseX-2,0,Viewport->SizeX);
		Viewport->HitY  = Clamp(MouseY-2,0,Viewport->SizeY);
		Viewport->HitXL = Clamp(MouseX+3,0,Viewport->SizeX) - Viewport->HitX;
		Viewport->HitYL = Clamp(MouseY+3,0,Viewport->SizeY) - Viewport->HitY;

		// Draw with hit-testing.
		BYTE HitData[1024];
		INT HitCount=ARRAY_COUNT(HitData);
		Draw( Viewport, 0, HitData, &HitCount );

		// Update buttons.
		if( Viewport->Input->KeyDown(IK_Shift) )	Buttons |= MOUSE_Shift;
		if( Viewport->Input->KeyDown(IK_Ctrl) )		Buttons |= MOUSE_Ctrl;
		if( Viewport->Input->KeyDown(IK_Alt) )		Buttons |= MOUSE_Alt;

		TCHAR *HitOverrideClass = NULL;
		if( Mode == EM_TerrainEdit
				&& Viewport->Actor->RendMap != REN_TexBrowser
				&& Viewport->Actor->RendMap != REN_TexBrowserUsed
				&& Viewport->Actor->RendMap != REN_TexBrowserMRU
				&& Viewport->Actor->RendMap != REN_TerrainLayers
				&& Viewport->Actor->RendMap != REN_TerrainDecoLayers
				&& Viewport->Actor->RendMap != REN_TerrainHeightmap )
			HitOverrideClass = TEXT("HTerrain");

		// Perform hit testing.
		FEditorHitObserver Observer;
		switch( Mode )
		{
			case EM_EyeDropper:
			{
				FColor EyeDropperColor;
				Viewport->ExecuteHits( FHitCause(&Observer,Viewport,Buttons,MouseX,MouseY), HitData, HitCount, HitOverrideClass, &EyeDropperColor );
				if( UseDest )
				{
					UseDest->SetItemFocus(1);
					UseDest->SetValue( *FString::Printf( TEXT("(R=%d,G=%d,B=%d)"),EyeDropperColor.R, EyeDropperColor.G, EyeDropperColor.B) );
					UseDest->SetItemFocus(0);
				}
				edcamSetMode( EM_ViewportMove );
			}
			break;

			case EM_FindActor:
			{
				AActor* Actor;
				Viewport->ExecuteHits( FHitCause(&Observer,Viewport,Buttons,MouseX,MouseY), HitData, HitCount, HitOverrideClass, NULL, &Actor );
				if( Actor && UseDest)
				{
					UseDest->SetItemFocus(1);
					UseDest->SetValue( *FString::Printf( TEXT("%s'%s'"), Actor->GetClass()->GetName(), Actor->GetName() ) );
					UseDest->SetItemFocus(0);
				}
				edcamSetMode( EM_ViewportMove );
			}
			break;

			default:
				Viewport->ExecuteHits( FHitCause(&Observer,Viewport,Buttons,MouseX,MouseY), HitData, HitCount, HitOverrideClass, NULL );
				break;
		}
	}

	unguard;
}

// A convenience function so that Viewports can set the click location manually.
void UUnrealEdEngine::edSetClickLocation( FVector& InLocation )
{
	guard(UUnrealEdEngine::edSetClickLocation);
	ClickLocation = InLocation;
	unguard;
}

/*-----------------------------------------------------------------------------
   Editor camera mode.
-----------------------------------------------------------------------------*/

//
// Set the editor mode.
//
void UUnrealEdEngine::edcamSetMode( int InMode )
{
	 guard(UUnrealEdEngine::edcamSetMode);

	// Clear old mode.
	if( Mode != EM_None )
		for( INT i=0; i<Client->Viewports.Num(); i++ )
			MouseDelta( Client->Viewports(i), MOUSE_ExitMode, 0, 0 );

	// Set new mode.
	Mode = InMode;
	if( Mode != EM_None )
		for( INT i=0; i<Client->Viewports.Num(); i++ )
			MouseDelta( Client->Viewports(i), MOUSE_SetMode, 0, 0 );

	EdCallback( EDC_CamModeChange, 1, 0 );

	GTerrainTools.EditorMode = InMode;

	RedrawLevel( Level );
	unguard;
}

//
// Return editor camera mode given Mode and state of keys.
// This handlers special keyboard mode overrides which should
// affect the appearance of the mouse cursor, etc.
//
int UUnrealEdEngine::edcamMode( UViewport* Viewport )
{
	guard(UUnrealEdEngine::edcamMode);
	check(Viewport);
	check(Viewport->Actor);
	switch( Viewport->Actor->RendMap )
	{
		case REN_TexView:				return EM_TexView;
		case REN_TexBrowser:
		case REN_TexBrowserUsed:
		case REN_TexBrowserMRU:			return EM_TexBrowser;
		case REN_StaticMeshBrowser:		return EM_StaticMeshBrowser;
		case REN_MeshView:				return EM_MeshView;
		case REN_Animation:				return EM_Animation;

#ifdef WITH_LIPSINC
		case REN_LIPSinc:				return EM_LIPSinc;
#endif

		case REN_Prefab:				return EM_PrefabBrowser;
		case REN_PrefabCompiled:		return EM_PrefabBrowser;
		case REN_TerrainHeightmap:		return EM_ViewportMove;
		case REN_TerrainLayers:			return EM_ViewportMove;
		case REN_TerrainDecoLayers:		return EM_ViewportMove;
		case REN_MatineePreview:		return EM_ViewportMove;
		case REN_MatineeScenes:			return EM_ViewportMove;
		case REN_MatineeActions:		return EM_ViewportMove;
		case REN_MatineeSubActions:		return EM_ViewportMove;
		case REN_MaterialEditor:		return EM_MaterialEditor;
	}
	return Mode;
	unguard;
}

int UUnrealEdEngine::edcamTerrainBrush()
{
	guard(UUnrealEdEngine::edcamTerrainBrush);
	return TerrainEditBrush;
	unguard;
}

int UUnrealEdEngine::edcamMouseControl( UViewport* InViewport )
{
	guard(UUnrealEdEngine::edcamMouseControl);

	if( Mode == EM_TerrainEdit
			&& GTerrainTools.TerrainEditBrush != TB_VertexEdit
			&& GTerrainTools.TerrainEditBrush != TB_TexturePan
			&& GTerrainTools.TerrainEditBrush != TB_TextureRotate
			&& GTerrainTools.TerrainEditBrush != TB_TextureScale
			&& InViewport->Input->KeyDown(IK_Ctrl) )
		return MOUSEC_Free;

	if( edcamMode(InViewport) == EM_MaterialEditor )
		return MOUSEC_Free;

	//if( GbIsBoxSel && !InViewport->IsOrtho() )
	//	return MOUSEC_Free;

	return MOUSEC_Locked;
	unguard;
}

/*-----------------------------------------------------------------------------
	Selection.
-----------------------------------------------------------------------------*/

//
// Selection change.
//
void UUnrealEdEngine::NoteSelectionChange( ULevel* Level )
{
	guard(UUnrealEdEngine::NoteSelectionChange);

	// Notify the editor.
	EdCallback( EDC_SelChange, 0, 0 );

	// Pick a new common pivot, or not.
	INT Count=0;
	AActor* SingleActor=NULL;
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		if( Level->Actors(i) && Level->Actors(i)->bSelected )
		{
			SingleActor=Level->Actors(i);
			Count++;
		}
	}
	if( Count==0 ) ResetPivot();
	else if( Count==1 ) SetPivot( SingleActor->Location, 0, 0, 1 );

	// Update any viewport locking
	for( INT i = 0 ; i < Client->Viewports.Num() ; i++ )
		Client->Viewports(i)->LockOnActor( SingleActor );

	// Update properties window.
	UpdatePropertiesWindows();

	vertexedit_Refresh();

	//WDM RedrawLevel( Level );

	unguard;
}

// Selects/Deselects an actor.
void UUnrealEdEngine::SelectActor( ULevel* Level, AActor* Actor, UBOOL bSelected, UBOOL bNotify )
{
	guard(UUnrealEdEngine::SelectActor);

	if( GEdSelectionLock )
		return;

	if( Actor )
	{
		Actor->Modify();
		Actor->bSelected = bSelected;
	}

	if( bNotify )
		NoteSelectionChange( Level );

	unguard;
}

// Selects/Deselects a BSP surface.
void UUnrealEdEngine::SelectBSPSurf( ULevel* Level, INT iSurf, UBOOL bSelected, UBOOL bNotify )
{
	guard(UUnrealEdEngine::SelectBSPSurf);

	if( GEdSelectionLock )
		return;

	FBspSurf& Surf = Level->Model->Surfs(iSurf);

	Level->Model->ModifySurf( iSurf, 0 );
	if( bSelected )
		Surf.PolyFlags |= PF_Selected;
	else
		Surf.PolyFlags &= ~PF_Selected;

	if( bNotify )
		NoteSelectionChange( Level );

	unguard;
}

//
// Select none.
//
// If "BSPSurfs" is 0, then BSP surfaces will be left selected.
//
void UUnrealEdEngine::SelectNone( ULevel *Level, UBOOL Notify, UBOOL BSPSurfs )
{
	guard(UUnrealEdEngine::SelectNone);

	if( GEdSelectionLock )
		return;

	if( Mode == EM_VertexEdit )
		VertexHitList.Empty();

	BezierControlPointList.Empty();

	Exec( TEXT("TERRAIN SOFTDESELECT") );
	Exec( TEXT("TERRAIN DESELECT") );

	// Unselect all actors.
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && Actor->bSelected )
		{
			// We don't do this in certain modes.  This allows the user to select
			// the brushes they want and not have them get deselected while trying to
			// work with them.
			if( Actor->IsBrush()
					&& ( Mode == EM_BrushClip || Mode == EM_FaceDrag ) )
				continue;

			SelectActor( Level, Actor, 0, 0 );
		}
	}

	if( BSPSurfs )
	{
		// Unselect all surfaces.
		for( INT i=0; i<Level->Model->Surfs.Num(); i++ )
		{
			FBspSurf& Surf = Level->Model->Surfs(i);
			if( Surf.PolyFlags & PF_Selected )
			{
				Level->Model->ModifySurf( i, 0 );
				Surf.PolyFlags &= ~PF_Selected;
			}
		}

		Level->Model->ClearRenderData(GRenDev);
	}

	if( Notify )
		NoteSelectionChange( Level );
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

