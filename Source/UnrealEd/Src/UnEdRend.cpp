/*=============================================================================
	UnEdRend.cpp: Unreal editor rendering functions.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "UnrealEd.h"
#include "UnRender.h"

extern TArray<FVertexHit> VertexHitList;
extern UBOOL GbIsSnapScaleBox;

/*-----------------------------------------------------------------------------
	Level brush drawing.
-----------------------------------------------------------------------------*/
#if 1
// far-plane Z clipping distance.
//!! adding EdClipZ to UUnrealEdEngine causes crash due to data structure size delta
// For now, I'm going to hack it to avoid changes to EditorEngine.uc and Editor.u
FLOAT EdClipZ = 0.0;
#endif

#if 1
//!! Z clipping hack to avoid mucking with Render.dll (see below)
void UUnrealEdEngine::SetZClipping()
{
	guard(UUnrealEdEngine::SetZClipping);

#if 0
	// toggle Z clipping
	if( EdClipZ > 0.0 )
	{
		ResetZClipping();
		return;
	}

	// use the default brush max distance as the basis for setting the Z clipping distance
	ABrush* Actor = Level->Brush();
	UModel* Brush = Actor->Brush;

	// get the Frame for the first wireframe/perspective Viewport available
	FSceneNode* Frame = 0;
	for( int i=0; i<Client->Viewports.Num(); i++ )
	{
		UViewport* Viewport = Client->Viewports(i);
		if( Viewport && !Viewport->IsOrtho() && Viewport->IsWire() )
		{
			Frame = Render->CreateMasterFrame( Viewport, Viewport->Actor->Location, Viewport->Actor->Rotation, NULL );
			break;
		}
	}

	// abort if no suitable viewport was found
	if( Frame!=NULL )
	{
		// find the distance to the default brush -- if it's behind the viewport, abort (don't set EdClipZ)
		FBox Box = Brush->GetRenderBoundingBox( Actor ).TransformBy(Actor->LocalToWorld());
		FVector	Temp = Box.Max - Frame->Coords.Origin;
		Temp     = Temp.TransformVectorBy( Frame->Coords );
		if( Temp.Z > 1.0 )
			EdClipZ = Temp.Z;
		Render->FinishMasterFrame();
	}
#endif

	unguard;
}

void UUnrealEdEngine::ResetZClipping()
{
	EdClipZ = 0.0;
}
#endif

/*-----------------------------------------------------------------------------
	Grid drawing.
-----------------------------------------------------------------------------*/

//
// Draw a piece of an orthogonal grid (arbitrary axes):
//
void UUnrealEdEngine::DrawGridSection
(
	FSceneNode*	SceneNode,
	INT			ViewportLocX,
	INT			ViewportSXR,
	INT			ViewportGridY,
	FVector*	A,
	FVector*	B,
	FLOAT*		AX,
	FLOAT*		BX,
	INT			AlphaCase
)
{
	guard(UUnrealEdEngine::DrawGridSection);

	FLineBatcher	LineBatcher(SceneNode->Viewport->RI);

	if( !ViewportGridY ) return;
	check(SceneNode->Viewport->IsOrtho());

	FLOAT	Zoom = SceneNode->Viewport->Actor->OrthoZoom / (SceneNode->Viewport->SizeX * 15.0f),
			Start = (int)((ViewportLocX - (ViewportSXR>>1) * Zoom)/ViewportGridY) - 1.0,
			End   = (int)((ViewportLocX + (ViewportSXR>>1) * Zoom)/ViewportGridY) + 1.0;
	INT     Dist  = (int)(SceneNode->Viewport->SizeX * Zoom / ViewportGridY);

	// Figure out alpha interpolator for fading in the grid lines.
	FLOAT Alpha;
	INT IncBits=0;
	if( Dist+Dist >= SceneNode->Viewport->SizeX/4 )
	{
		while( (Dist>>IncBits) >= SceneNode->Viewport->SizeX/4 )
			IncBits++;
		Alpha = 2 - 2*(FLOAT)Dist / (FLOAT)((1<<IncBits) * SceneNode->Viewport->SizeX/4);
	}
	else Alpha = 1.0;

	INT iStart  = ::Max<INT>((int)Start,-HALF_WORLD_MAX/ViewportGridY) >> IncBits;
	INT iEnd    = ::Min<INT>((int)End,  +HALF_WORLD_MAX/ViewportGridY) >> IncBits;

	for( INT i=iStart; i<iEnd; i++ )
	{
		*AX = (i * ViewportGridY) << IncBits;
		*BX = (i * ViewportGridY) << IncBits;
		if( (i&1) != AlphaCase )
		{
			FPlane Background = C_OrthoBackground.Plane();
			FPlane Grid       = FPlane(.5,.5,.5,0);
			FPlane Color      = Background + (Grid-Background) * (((i<<IncBits)&7) ? 0.5 : 1.0);
			if( i&1 ) Color = Background + (Color-Background) * Alpha;

			LineBatcher.DrawLine(*A,*B,FColor(Color));
		}
	}

	unguard;
}

//
// Draw worldbox and groundplane lines, if desired.
//
void UUnrealEdEngine::DrawWireBackground( FSceneNode* SceneNode )
{
	guard(UUnrealEdEngine::DrawWireBackground);

	// If clicked on nothing else, clicked on backdrop.
	FLOAT	SizeX2 = SceneNode->Viewport->SizeX / 2,
			SizeY2 = SceneNode->Viewport->SizeY / 2,
			HitX = SceneNode->Viewport->HitX + SceneNode->Viewport->HitXL / 2,
			HitY = SceneNode->Viewport->HitY + SceneNode->Viewport->HitYL / 2;
	FVector	V = SceneNode->Deproject(FPlane((HitX - SizeX2) / SizeX2,(HitY - SizeY2) / -SizeY2,0.5f,1.0f));

	PUSH_HIT(SceneNode->Viewport,HBackdrop(V));
	POP_HIT_FORCE(SceneNode->Viewport);

	// Vector defining worldbox lines.
	FVector	Origin = SceneNode->ViewOrigin;
	FVector B1( HALF_WORLD_MAX, HALF_WORLD_MAX1, HALF_WORLD_MAX1);
	FVector B2(-HALF_WORLD_MAX, HALF_WORLD_MAX1, HALF_WORLD_MAX1);
	FVector B3( HALF_WORLD_MAX,-HALF_WORLD_MAX1, HALF_WORLD_MAX1);
	FVector B4(-HALF_WORLD_MAX,-HALF_WORLD_MAX1, HALF_WORLD_MAX1);
	FVector B5( HALF_WORLD_MAX, HALF_WORLD_MAX1,-HALF_WORLD_MAX1);
	FVector B6(-HALF_WORLD_MAX, HALF_WORLD_MAX1,-HALF_WORLD_MAX1);
	FVector B7( HALF_WORLD_MAX,-HALF_WORLD_MAX1,-HALF_WORLD_MAX1);
	FVector B8(-HALF_WORLD_MAX,-HALF_WORLD_MAX1,-HALF_WORLD_MAX1);
	FVector A,B;
	INT i,j;

	FLineBatcher	LineBatcher(SceneNode->Viewport->RI);

	SceneNode->Viewport->RI->SetTransform(TT_LocalToWorld,FMatrix::Identity);
	SceneNode->Viewport->RI->SetTransform(TT_WorldToCamera,SceneNode->WorldToCamera);
	SceneNode->Viewport->RI->SetTransform(TT_CameraToScreen,SceneNode->CameraToScreen);

	// Draw it.
	if( SceneNode->Viewport->IsOrtho() )
	{
		if( SceneNode->Viewport->Actor->ShowFlags & SHOW_Frame )
		{
			// Draw grid.
			for( int AlphaCase=0; AlphaCase<=1; AlphaCase++ )
			{
				if( SceneNode->Viewport->Actor->RendMap==REN_OrthXY )
				{
					// Do Y-Axis lines.
					A.Y=+HALF_WORLD_MAX1; A.Z=0.0;
					B.Y=-HALF_WORLD_MAX1; B.Z=0.0;
					DrawGridSection( SceneNode, Origin.X, SceneNode->Viewport->SizeX, GUnrealEd->Constraints.GridSize.X, &A, &B, &A.X, &B.X, AlphaCase );

					// Do X-Axis lines.
					A.X=+HALF_WORLD_MAX1; A.Z=0.0;
					B.X=-HALF_WORLD_MAX1; B.Z=0.0;
					DrawGridSection( SceneNode, Origin.Y, SceneNode->Viewport->SizeY, GUnrealEd->Constraints.GridSize.Y, &A, &B, &A.Y, &B.Y, AlphaCase );
				}
				else if( SceneNode->Viewport->Actor->RendMap==REN_OrthXZ )
				{
					// Do Z-Axis lines.
					A.Z=+HALF_WORLD_MAX1; A.Y=0.0;
					B.Z=-HALF_WORLD_MAX1; B.Y=0.0;
					DrawGridSection( SceneNode, Origin.X, SceneNode->Viewport->SizeX, GUnrealEd->Constraints.GridSize.X, &A, &B, &A.X, &B.X, AlphaCase );

					// Do X-Axis lines.
					A.X=+HALF_WORLD_MAX1; A.Y=0.0;
					B.X=-HALF_WORLD_MAX1; B.Y=0.0;
					DrawGridSection( SceneNode, Origin.Z, SceneNode->Viewport->SizeY, GUnrealEd->Constraints.GridSize.Z, &A, &B, &A.Z, &B.Z, AlphaCase );
				}
				else if( SceneNode->Viewport->Actor->RendMap==REN_OrthYZ )
				{
					// Do Z-Axis lines.
					A.Z=+HALF_WORLD_MAX1; A.X=0.0;
					B.Z=-HALF_WORLD_MAX1; B.X=0.0;
					DrawGridSection( SceneNode, Origin.Y, SceneNode->Viewport->SizeX, GUnrealEd->Constraints.GridSize.Y, &A, &B, &A.Y, &B.Y, AlphaCase );

					// Do Y-Axis lines.
					A.Y=+HALF_WORLD_MAX1; A.X=0.0;
					B.Y=-HALF_WORLD_MAX1; B.X=0.0;
					DrawGridSection( SceneNode, Origin.Z, SceneNode->Viewport->SizeY, GUnrealEd->Constraints.GridSize.Z, &A, &B, &A.Z, &B.Z, AlphaCase );
				}
			}

			// Draw axis lines.
			FColor Color = SceneNode->Viewport->IsOrtho() ? C_WireGridAxis : C_GroundHighlight;

			A.X=+HALF_WORLD_MAX1;  A.Y=0; A.Z=0;
			B.X=-HALF_WORLD_MAX1;  B.Y=0; B.Z=0;
			LineBatcher.DrawLine(A,B,Color);

			A.X=0; A.Y=+HALF_WORLD_MAX1; A.Z=0;
			B.X=0; B.Y=-HALF_WORLD_MAX1; B.Z=0;
			LineBatcher.DrawLine(A,B,Color);

			A.X=0; A.Y=0; A.Z=+HALF_WORLD_MAX1;
			B.X=0; B.Y=0; B.Z=-HALF_WORLD_MAX1;
	       	LineBatcher.DrawLine(A,B,Color);
		}

		// Draw KillZ
		if( (SceneNode->Viewport->Actor->RendMap==REN_OrthXZ) || (SceneNode->Viewport->Actor->RendMap==REN_OrthYZ) )
		{
			FVector KillStart = B1;
			FVector KillEnd = B4;
			KillStart.Z = SceneNode->Viewport->Actor->Region.Zone->KillZ;
			KillEnd.Z = KillStart.Z;
			LineBatcher.DrawLine( KillStart, KillEnd, FColor(255,0,0) );
			KillStart.Z -= 1.f;
			KillEnd.Z = KillStart.Z;
			LineBatcher.DrawLine( KillStart, KillEnd, FColor(255,0,0) );
			KillStart.Z -= 1.f;
			KillEnd.Z = KillStart.Z;
			LineBatcher.DrawLine( KillStart, KillEnd, FColor(255,0,0) );
		}
		// Draw StallZ
		if( (SceneNode->Viewport->Actor->RendMap==REN_OrthXZ) || (SceneNode->Viewport->Actor->RendMap==REN_OrthYZ) )
		{
			FVector StallStart = B1;
			FVector StallEnd = B4;
			StallStart.Z = SceneNode->Viewport->Actor->Level->StallZ;
			StallEnd.Z = StallStart.Z;
			LineBatcher.DrawLine( StallStart, StallEnd, FColor(0,0,255) );
			StallStart.Z -= 1.f;
			StallEnd.Z = StallStart.Z;
			LineBatcher.DrawLine( StallStart, StallEnd, FColor(0,0,255) );
			StallStart.Z -= 1.f;
			StallEnd.Z = StallStart.Z;
			LineBatcher.DrawLine( StallStart, StallEnd, FColor(0,0,255) );
		}

		// Draw orthogonal worldframe.
		LineBatcher.DrawLine(B1,B2,C_WorldBox);
		LineBatcher.DrawLine(B3,B4,C_WorldBox);
		LineBatcher.DrawLine(B5,B6,C_WorldBox);
		LineBatcher.DrawLine(B7,B8,C_WorldBox);
		LineBatcher.DrawLine(B1,B3,C_WorldBox);
		LineBatcher.DrawLine(B5,B7,C_WorldBox);
		LineBatcher.DrawLine(B2,B4,C_WorldBox);
		LineBatcher.DrawLine(B6,B8,C_WorldBox);
		LineBatcher.DrawLine(B1,B5,C_WorldBox);
		LineBatcher.DrawLine(B2,B6,C_WorldBox);
		LineBatcher.DrawLine(B3,B7,C_WorldBox);
		LineBatcher.DrawLine(B4,B8,C_WorldBox);
	}
	else if
	(	(SceneNode->Viewport->Actor->ShowFlags & SHOW_Frame)
	&& !(SceneNode->Viewport->Actor->ShowFlags & SHOW_Backdrop) )
	{
		LineBatcher.DrawLine(B1,B2,C_WorldBox);
		LineBatcher.DrawLine(B3,B4,C_WorldBox);
		LineBatcher.DrawLine(B5,B6,C_WorldBox);
		LineBatcher.DrawLine(B7,B8,C_WorldBox);
		LineBatcher.DrawLine(B1,B3,C_WorldBox);
		LineBatcher.DrawLine(B5,B7,C_WorldBox);
		LineBatcher.DrawLine(B2,B4,C_WorldBox);
		LineBatcher.DrawLine(B6,B8,C_WorldBox);
		LineBatcher.DrawLine(B1,B5,C_WorldBox);
		LineBatcher.DrawLine(B2,B6,C_WorldBox);
		LineBatcher.DrawLine(B3,B7,C_WorldBox);
		LineBatcher.DrawLine(B4,B8,C_WorldBox);

		FColor	ColorLo = C_GroundPlane,
				ColorHi = C_GroundHighlight;

		if( SceneNode->Viewport->Actor->RendMap == REN_StaticMeshBrowser
				|| SceneNode->Viewport->Actor->RendMap == REN_Prefab
				|| SceneNode->Viewport->Actor->RendMap == REN_PrefabCompiled
				|| SceneNode->Viewport->Actor->RendMap == REN_MeshView
				|| SceneNode->Viewport->Actor->RendMap == REN_Animation

#ifdef WITH_LIPSINC
				|| SceneNode->Viewport->Actor->RendMap == REN_LIPSinc
#endif
				)
		{
			ColorLo = FColor( 72,72,72 );
			ColorHi = FColor( 80,80,80 );
		}

		// Index of middle line (axis).
		j=(63-1)/2;
		for( i=0; i<63; i++ )
		{
			A.X=(HALF_WORLD_MAX1/4.f)*(-1.0+2.0*i/(63-1));	B.X=A.X;

			A.Y=(HALF_WORLD_MAX1/4.f);			B.Y=-(HALF_WORLD_MAX1/4.f);
			A.Z=0.0;							B.Z=0.0;
			LineBatcher.DrawLine(A,B,(i==j)?ColorHi:ColorLo);

			A.Y=A.X;							B.Y=B.X;
			A.X=(HALF_WORLD_MAX1/4.f);			B.X=-(HALF_WORLD_MAX1/4.f);
			LineBatcher.DrawLine(A,B,(i==j)?ColorHi:ColorLo);
		}
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

