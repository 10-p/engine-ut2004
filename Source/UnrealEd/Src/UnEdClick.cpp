/*=============================================================================
	UnEdClick.cpp: Editor click-detection code.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "UnrealEd.h"
#include "UnRender.h"

extern TArray<HBezierControlPoint> BezierControlPointList;
extern TArray<FVertexHit> VertexHitList;
extern FVector GGridBase;
extern FRotator GSnappedRotation;
extern void vertexedit_GetBrushList( TArray<ABrush*>* BrushList );
extern INT GARGAxis;
extern UBOOL GbARG;

// Counts the number of ClipMarkers currently in the world.
INT NumClipMarkers(void)
{
	guard(NumClipMarkers);
	INT markers = 0;
	for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; i++ )
	{
		AActor* Actor = GUnrealEd->Level->Actors(i);
		if( Actor && Actor->IsA(AClipMarker::StaticClass()) )
			markers++;
	}
	return markers;
	unguard;
}

// Adds a AClipMarker actor at the click location.
void AddClipMarker()
{
	guard(AddClipMarker);

	// If there are 3 (or more) clipmarkers, already in the level delete them so the user can start fresh.
	if( NumClipMarkers() > 2 )
		GUnrealEd->Exec( TEXT("BRUSHCLIP DELETE") );
	else
	{
		// Loop through the existing clip markers and fix up any wrong texture assignments
		// (texture names can become wrong if the user manually deletes a marker)
		UTexture* Texture;
		FString Str;
		INT ClipMarker = 0;
		for( INT i = 0 ; i < GUnrealEd->Level->Actors.Num() ; i++ )
		{
			AActor* Actor = GUnrealEd->Level->Actors(i);
			if( Actor && Actor->IsA(AClipMarker::StaticClass()) )
			{
				ClipMarker++;
				Str = *FString::Printf(TEXT("TEXTURE=S_ClipMarker%d"), ClipMarker );
				if( ParseObject<UTexture>( *Str, TEXT("TEXTURE="), Texture, ANY_PACKAGE ) )
					Actor->Texture = Texture;
			}
		}

		// Create new clip marker
		FString TextureName = *FString::Printf(TEXT("S_ClipMarker%d"), NumClipMarkers()+1 );
		GUnrealEd->Exec( *FString::Printf(TEXT("ACTOR ADD CLASS=CLIPMARKER SNAP=1 TEXTURE=%s"), *TextureName) );
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Adding actors.
-----------------------------------------------------------------------------*/


AActor* UUnrealEdEngine::AddActor( ULevel* Level, UClass* Class, FVector V, UBOOL bSilent )
{
	guard(AddActor);
	check(Class);

	if( !bSilent )
	{ 
		debugf( NAME_Log, TEXT("Attempting to add actor of class '%s' to level at %0.2f,%0.2f,%0.2f (R=%0.2fxH=%0.2f)"), 
			Class->GetName(), 
			V.X, V.Y, V.Z,
			Class->GetDefaultActor()->CollisionRadius, 
			Class->GetDefaultActor()->CollisionHeight );
	}

	// Validate everything.
	if( Class->ClassFlags & CLASS_Abstract )
	{
		GWarn->Logf( TEXT("Class %s is abstract.  You can't add actors of this class to the world."), Class->GetName() );
		return NULL;
	}
	if( !(Class->ClassFlags & CLASS_Placeable) )
	{
		GWarn->Logf( TEXT("Class %s isn't placeable.  You can't add actors of this class to the world."), Class->GetName() );
		return NULL;
	}
	else if( Class->ClassFlags & CLASS_Transient )
	{
		GWarn->Logf( TEXT("Class %s is transient.  You can't add actors of this class in UnrealEd."), Class->GetName() );
		return NULL;
	}

	// Transactionally add the actor.
	Trans->Begin( TEXT("Add Actor") );
	SelectNone( Level, 0 );
	Level->Modify();
	AActor* Actor = Level->SpawnActor( Class, NAME_None, V );
	if( Actor )
	{
		SelectActor( Level, Actor, 1, 0 );
		//if( !Level->FarMoveActor( Actor, V ) )//necessary??!!
		//{
		//	GWarn->Logf( TEXT("Actor doesn't fit there (couldn't move it) -- make sure snap to grid isn't on") );
		//	Level->DestroyActor( Actor );
		//}
		//else if( !bSilent ) 
		//{
		//	debugf( NAME_Log, TEXT("Added actor %s successfully at %0.2f,%0.2f,%0.2f"), 
		//		Actor->GetName(), 
		//		Actor->Location.X, 
		//		Actor->Location.Y, 
		//		Actor->Location.Z );
		//}
		if( Class->GetDefaultActor()->IsBrush() )
			csgCopyBrush( (ABrush*)Actor, (ABrush*)Class->GetDefaultActor(), 0, 0, 1 );
		Actor->PostEditMove();
		Actor->bLightChanged = 1;
	}
	else GWarn->Logf( TEXT("Actor doesn't fit there (couldn't spawn it) -- make sure snap to grid isn't on") );

	Trans->End();

	NoteSelectionChange( Level );
	return Actor;
	unguard;
}

// Adds a vertex position to the list.
void vertexedit_AddPosition( ABrush* pBrush, INT PolyIndex, INT VertexIndex )
{
	guard(vertexedit_AddPosition);

	// If this position is already in the list, leave.

	for( INT vertex = 0 ; vertex < VertexHitList.Num() ; vertex++ )
		if( VertexHitList(vertex) == FVertexHit( pBrush, PolyIndex, VertexIndex ) )
			return;

	// Add it to the list.
	new(VertexHitList)FVertexHit( pBrush, PolyIndex, VertexIndex );

	// Save the polygons normal.  This is used for comparison purposes later on.
	pBrush->Brush->Polys->Element(PolyIndex).SaveNormal = pBrush->Brush->Polys->Element(PolyIndex).Normal;
	FVector blah = pBrush->Brush->Polys->Element(PolyIndex).SaveNormal;

	unguard;
}
void vertexedit_HandlePosition( ABrush* pBrush, INT PolyIndex, INT VertexIndex, UBOOL InCumulative, UBOOL InAllowDuplicates )
{
	guard(vertexedit_HandlePosition);

	if( InCumulative )
		for( INT vertex = 0 ; vertex < VertexHitList.Num() ; vertex++ )
			if( VertexHitList(vertex) == FVertexHit( pBrush, PolyIndex, VertexIndex ) )
			{
				if( !InAllowDuplicates )
					VertexHitList.Remove( vertex );
				return;
			}

	vertexedit_AddPosition( pBrush, PolyIndex, VertexIndex );

	unguard;
}

void vertexedit_Click( UViewport* InViewport, ABrush* pBrush, FVector InLocation, UBOOL InCumulative, UBOOL InAllowDuplicates )
{
	guard(vertexedit_Click);

	// If user is not doing a cumulative selection, empty out the current list.
	if( !InCumulative )
		VertexHitList.Empty();

	FVector vtx = pBrush->WorldToLocal().TransformFVector(InLocation);

	for( INT poly = 0 ; poly < pBrush->Brush->Polys->Element.Num() ; poly++ )
	{
		FPoly pPoly = pBrush->Brush->Polys->Element(poly);
		for( INT vertex = 0 ; vertex < pPoly.NumVertices ; vertex++ )
		{
			UBOOL bPointsAreEqual = 0;

			switch( InViewport->Actor->RendMap )
			{
				case REN_OrthXY:
					bPointsAreEqual = FPointsAreSame( FVector( pPoly.Vertex[vertex].X, pPoly.Vertex[vertex].Y, 0), FVector( vtx.X, vtx.Y, 0 ) );
					break;
				case REN_OrthXZ:
					bPointsAreEqual = FPointsAreSame( FVector( pPoly.Vertex[vertex].X, 0, pPoly.Vertex[vertex].Z), FVector( vtx.X, 0, vtx.Z ) );
					break;
				case REN_OrthYZ:
					bPointsAreEqual = FPointsAreSame( FVector( 0, pPoly.Vertex[vertex].Y, pPoly.Vertex[vertex].Z), FVector( 0, vtx.Y, vtx.Z ) );
					break;
				default:
					bPointsAreEqual = FPointsAreSame( pPoly.Vertex[vertex], vtx );
					break;
			}

			if( bPointsAreEqual )
				vertexedit_HandlePosition( pBrush, poly, vertex, InCumulative, InAllowDuplicates );
		}
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	HTextureView.
-----------------------------------------------------------------------------*/

void HTextureView::Click( const FHitCause& Cause )
{
	guard(HTextureView::Click);
	check(Material);
	UTexture* Texture = Cast<UTexture>(Material);
	if( Texture )
		Texture->Click( Cause.Buttons, Cause.MouseX*Texture->USize/ViewX, Cause.MouseY*Texture->VSize/ViewY );
	unguard;
}

/*-----------------------------------------------------------------------------
	HBackdrop.
-----------------------------------------------------------------------------*/

void HBackdrop::Click( const FHitCause& Cause )
{
	guard(HBackdrop::Click);

	GUnrealEd->ClickLocation = Location;
	GUnrealEd->ClickPlane    = FPlane(0,0,0,0);

	if( Cause.Viewport->Actor->RendMap == REN_Prefab
			|| Cause.Viewport->Actor->RendMap == REN_PrefabCompiled
			|| Cause.Viewport->Actor->RendMap == REN_StaticMeshBrowser )
		return;

	/*
	GEdModeTools->ClickLocation = Location;
	GEdModeTools->ClickPlane    = FPlane(0,0,0,0);
	
	if( Cause.Viewport->Actor->RendMap == REN_Prefab
			|| Cause.Viewport->Actor->RendMap == REN_PrefabCompiled
			|| Cause.Viewport->Actor->RendMap == REN_StaticMeshBrowser )
		return;

	if( GEdModeTools->GetCurrentMode() ) GEdModeTools->GetCurrentMode()->Clicked( &Cause, NULL );
	*/

	if( GUnrealEd->Mode == EM_BrushClip )
	{
		if( Cause.Buttons&MOUSE_Right && Cause.Buttons & MOUSE_Ctrl )
		{
			AddClipMarker();
		}
	}
	else if( GUnrealEd->Mode == EM_Polygon && (Cause.Buttons&MOUSE_Right && Cause.Buttons & MOUSE_Ctrl) )
	{
		GUnrealEd->Exec( TEXT("ACTOR ADD CLASS=POLYMARKER SNAP=1") );
	}
	else
	{
		if( (Cause.Buttons&MOUSE_Left) && Cause.Viewport->Input->KeyDown(IK_A) )
		{
			if( GUnrealEd->CurrentClass )
			{
				TCHAR Cmd[256];
				appSprintf( Cmd, TEXT("ACTOR ADD CLASS=%s"), GUnrealEd->CurrentClass->GetName() );
				GUnrealEd->Exec( Cmd );
			}
		}
		else if( (Cause.Buttons&MOUSE_Left) && Cause.Viewport->Input->KeyDown(IK_L) )
		{
			GUnrealEd->Exec( TEXT("ACTOR ADD CLASS=LIGHT") );
		}
		else if( Cause.Buttons & MOUSE_Right && !(Cause.Buttons & MOUSE_Ctrl) )
		{
			if( Cause.Viewport->IsOrtho() )
				GUnrealEd->EdCallback( EDC_RtClickWindowCanAdd, 0, (DWORD)&(Cause.Viewport->OrigCursor) );
			else
				GUnrealEd->EdCallback( EDC_RtClickWindow, 0, (DWORD)&(Cause.Viewport->OrigCursor) );
		}
		else if( Cause.Buttons & MOUSE_Left )
		{
			if( !(Cause.Buttons & MOUSE_Ctrl) )
			{
				GUnrealEd->Trans->Begin( TEXT("Select None"), false ); // gam
				GUnrealEd->SelectNone( Cause.Viewport->Actor->GetLevel(), 1 );
				GUnrealEd->Trans->End();
			}
		}
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	FEditorHitObserver implementation.
-----------------------------------------------------------------------------*/

static FBspSurf GSaveSurf;
void FEditorHitObserver::Click( const FHitCause& Cause, const struct HBspSurf& Hit )
{
	guard(FEditorHitObserver::ClickHBspSurf);

	//if( GEdModeTools->GetCurrentMode() ) GEdModeTools->GetCurrentMode()->Clicked( &Cause, (HBspSurf*)&Hit );

	UModel*   Model = Cause.Viewport->Actor->GetLevel()->Model;
	FBspSurf& Surf  = Model->Surfs(Hit.iSurf);

	// Gizmos can cause BSP surfs to become selected without this check
	if( Cause.Buttons == (MOUSE_Right | MOUSE_Ctrl ) )
		return;

	if( Cause.Viewport->Actor->RendMap == REN_Prefab || Cause.Viewport->Actor->RendMap == REN_PrefabCompiled )
		return;

	// Adding actor.
	check(Hit.Parent);
	check(Hit.Parent->IsA(TEXT("HCoords")));
	HCoords* HitCoords     = (HCoords*)Hit.Parent;
	FPlane	Plane		   = Surf.Plane;

	// Remember hit location for actor-adding.
	GUnrealEd->ClickLocation = FLinePlaneIntersection( HitCoords->Origin, HitCoords->Origin + HitCoords->Direction, Plane );
	GUnrealEd->ClickPlane    = Plane;

	if( (Cause.Buttons&MOUSE_Left) && (Cause.Buttons & MOUSE_Shift) && (Cause.Buttons & MOUSE_Ctrl) )
	{
		// Select the brush actor that belongs to this BSP surface.
		check( Surf.Actor );
		GUnrealEd->SelectActor( GUnrealEd->Level, Surf.Actor );
	}
	else if( (Cause.Buttons&MOUSE_Left) && (Cause.Buttons & MOUSE_Shift) )
	{
		// Apply texture to all selected.
		GUnrealEd->Trans->Begin( TEXT("apply texture to selected surfaces") );
		for( INT i=0; i<Model->Surfs.Num(); i++ )
		{
			if( Model->Surfs(i).PolyFlags & PF_Selected )
			{
				Model->ModifySurf( i, 1 );
				Model->Surfs(i).Material = GUnrealEd->CurrentMaterial;
				GUnrealEd->polyUpdateMaster( Model, i, 0 );
			}
		}
		Model->ClearRenderData(GUnrealEd->GRenDev);
		GUnrealEd->Trans->End();
	}
	else if( (Cause.Buttons&MOUSE_Left) && Cause.Viewport->Input->KeyDown(IK_A) )
	{
		if( GUnrealEd->CurrentClass )
		{
			TCHAR Cmd[256];
			appSprintf( Cmd, TEXT("ACTOR ADD CLASS=%s"), GUnrealEd->CurrentClass->GetName() );
			GUnrealEd->Exec( Cmd );
		}
	}
	else if( (Cause.Buttons&MOUSE_Left) && Cause.Viewport->Input->KeyDown(IK_L) )
	{
		GUnrealEd->Exec( TEXT("ACTOR ADD CLASS=LIGHT") );
	}
	else if( (Cause.Buttons&MOUSE_Alt) && (Cause.Buttons&MOUSE_Right) )
	{
		// Grab the texture.
		GUnrealEd->CurrentMaterial = Surf.Material;
		GSaveSurf = Surf;
		GUnrealEd->EdCallback( EDC_CurTexChange, 0, 0 );
	}
	else if( (Cause.Buttons&MOUSE_Alt) && (Cause.Buttons&MOUSE_Left) )
	{
		// Apply texture to the one polygon clicked on.
		GUnrealEd->Trans->Begin( TEXT("apply texture to surface") );
		Model->ModifySurf( Hit.iSurf, 1 );
		Surf.Material = GUnrealEd->CurrentMaterial;
		if( Cause.Buttons & MOUSE_Ctrl )
		{
			Surf.vTextureU	= GSaveSurf.vTextureU;
			Surf.vTextureV	= GSaveSurf.vTextureV;
			if( Surf.vNormal == GSaveSurf.vNormal )
			{
				GLog->Logf( TEXT("WARNING: the texture coordinates were not parallel to the surface.") );
			}
			Surf.PolyFlags	= GSaveSurf.PolyFlags;
			GUnrealEd->polyUpdateMaster( Model, Hit.iSurf, 1 );
			Model->ClearRenderData(GUnrealEd->GRenDev);
		}
		else
		{
			GUnrealEd->polyUpdateMaster( Model, Hit.iSurf, 0 );
			Model->ClearRenderData(GUnrealEd->GRenDev);
		}
		GUnrealEd->Trans->End();
	}
	else if( Cause.Buttons & MOUSE_Right && !(Cause.Buttons & MOUSE_Ctrl) ) 
	{
		// Edit surface properties.
		GUnrealEd->Trans->Begin( TEXT("select surface for editing"), false ); // gam
		Model->ModifySurf( Hit.iSurf, 0 );
		Surf.PolyFlags |= PF_Selected;
		Model->ClearRenderData(GUnrealEd->GRenDev);
		GUnrealEd->NoteSelectionChange( Cause.Viewport->Actor->GetLevel() );
		GUnrealEd->EdCallback( EDC_RtClickPoly, 0, 0 );
		GUnrealEd->Trans->End();
	}
	else
	{
		// Select or deselect surfaces.
		GUnrealEd->Trans->Begin( TEXT("select surfaces"), false ); // gam
		DWORD SelectMask = Surf.PolyFlags & PF_Selected;
		if( !(Cause.Buttons & MOUSE_Ctrl) )
			GUnrealEd->SelectNone( Cause.Viewport->Actor->GetLevel(), 0 );
		Model->ModifySurf( Hit.iSurf, 0 );
		Surf.PolyFlags = (Surf.PolyFlags & ~PF_Selected) | (SelectMask ^ PF_Selected);
		Model->ClearRenderData(GUnrealEd->GRenDev);
		GUnrealEd->NoteSelectionChange( Cause.Viewport->Actor->GetLevel() );
		GUnrealEd->Trans->End();
	}
	unguard;
}
void FEditorHitObserver::Click( const FHitCause& Cause, const struct HActor& Hit )
{
	guard(FEditorHitObserver::ClickHActor);

	if( Cause.Viewport->Actor->RendMap == REN_Prefab
			|| Cause.Viewport->Actor->RendMap == REN_PrefabCompiled
			|| Cause.Viewport->Actor->RendMap == REN_StaticMeshBrowser )
		return;

	FCameraSceneNode	SceneNode(Cause.Viewport,&Cause.Viewport->RenderTarget,Cause.Viewport->Actor,Cause.Viewport->Actor->Location,Cause.Viewport->Actor->Rotation,Cause.Viewport->Actor->FovAngle);
	FCanvasUtil			CanvasUtil(&Cause.Viewport->RenderTarget,Cause.Viewport->RI);

	FVector Origin = SceneNode.ViewOrigin;
	FPlane	P(CanvasUtil.CanvasToScreen.TransformFVector(FVector(Cause.MouseX,Cause.MouseY,0)),NEAR_CLIPPING_PLANE);
	FVector Direction = SceneNode.Deproject(P) - Origin;
	Direction.Normalize();

	if(Hit.Actor->GetPrimitive())
	{
		FCheckResult Check(0);

		if(!Hit.Actor->GetPrimitive()->LineCheck(Check,Hit.Actor,Origin + Direction * HALF_WORLD_MAX,Origin,FVector(0,0,0),0,0))
		{
			GUnrealEd->ClickLocation = Check.Location;
			GUnrealEd->ClickPlane = FPlane(Check.Location,Check.Normal);
		}
	}

	GUnrealEd->Trans->Begin( TEXT("clicking on actors"), false ); // gam

	if( GUnrealEd->Mode == EM_Polygon )
	{
		if( Hit.Actor->IsA(APolyMarker::StaticClass())
				|| (Cause.Buttons & MOUSE_Left && Hit.Actor->IsBrush() ))
		{
			// Toggle actor selection.
			Hit.Actor->Modify();
			if( Cause.Buttons & MOUSE_Ctrl )
				GUnrealEd->SelectActor( GUnrealEd->Level, Hit.Actor, !Hit.Actor->bSelected );
			else
			{
				GUnrealEd->SelectNone( Cause.Viewport->Actor->GetLevel(), 0 );
				GUnrealEd->SelectActor( GUnrealEd->Level, Hit.Actor );
			}
		}
		else
			if( Cause.Buttons & MOUSE_Right )
				if( Cause.Buttons & MOUSE_Ctrl )
					GUnrealEd->Exec( TEXT("ACTOR ADD CLASS=POLYMARKER SNAP=1") );
				else
					if( Hit.Actor->IsA(AStaticMeshActor::StaticClass() ) )
						GUnrealEd->EdCallback( EDC_RtClickActorStaticMesh, 0, 0 );
					else
						GUnrealEd->EdCallback( EDC_RtClickActor, 0, 0 );
	}
	else if( GUnrealEd->Mode == EM_BrushClip )
	{
		if( Hit.Actor->IsA(AClipMarker::StaticClass())
				|| (Cause.Buttons & MOUSE_Left && Hit.Actor->IsBrush() ))
		{
			// Toggle actor selection.

			if( Cause.Buttons & MOUSE_Ctrl )
				GUnrealEd->SelectActor( GUnrealEd->Level, Hit.Actor, !Hit.Actor->bSelected );
			else
			{
				GUnrealEd->SelectNone( Cause.Viewport->Actor->GetLevel(), 0 );
				GUnrealEd->SelectActor( GUnrealEd->Level, Hit.Actor );
			}
		}
		else
			if( Cause.Buttons & MOUSE_Right )
				if( Cause.Buttons & MOUSE_Ctrl )
				{
					AddClipMarker();
				}
				else
					if( Hit.Actor->IsA(AStaticMeshActor::StaticClass() ) )
						GUnrealEd->EdCallback( EDC_RtClickActorStaticMesh, 0, 0 );
					else
						GUnrealEd->EdCallback( EDC_RtClickActor, 0, 0 );
	}
	else
	{
		// Click on a non-vertex clears the current list of vertices.
		VertexHitList.Empty();

		// Handle selection.
		if( Cause.Buttons & MOUSE_Right && !(Cause.Buttons & MOUSE_Ctrl) )
		{
			// Bring up properties of this actor and other selected actors.
			GUnrealEd->SelectActor( GUnrealEd->Level, Hit.Actor );
			if( Hit.Actor->IsA(AStaticMeshActor::StaticClass() ) )
				GUnrealEd->EdCallback( EDC_RtClickActorStaticMesh, 0, 0 );
			else
				GUnrealEd->EdCallback( EDC_RtClickActor, 0, 0 );
		}
		else if( Cause.Buttons & MOUSE_LeftDouble )
		{
			if( !(Cause.Buttons & MOUSE_Ctrl) )
				GUnrealEd->SelectNone( Cause.Viewport->Actor->GetLevel(), 0 );
			GUnrealEd->SelectActor( GUnrealEd->Level, Hit.Actor );
			GUnrealEd->ShowActorProperties();
		}
		else
		{
			if( !(Cause.Buttons & MOUSE_Right) )
			{
				Hit.Actor->Modify();
				if( Cause.Buttons & MOUSE_Ctrl )
					GUnrealEd->SelectActor( GUnrealEd->Level, Hit.Actor, !Hit.Actor->bSelected );
				else
				{
					GUnrealEd->SelectNone( Cause.Viewport->Actor->GetLevel(), 0 );
					GUnrealEd->SelectActor( GUnrealEd->Level, Hit.Actor );
				}
			}
		}
	}

	GUnrealEd->Trans->End();
	unguard;
}

void FEditorHitObserver::Click( const FHitCause& Cause, const struct HBrushVertex& Hit )
{
	guard(FEditorHitObserver::ClickHBrushVertex);

	if( Cause.Viewport->Actor->RendMap == REN_Prefab || Cause.Viewport->Actor->RendMap == REN_PrefabCompiled )
		return;

	if( GUnrealEd->Mode == EM_FaceDrag )
		return;
	else if( GUnrealEd->Mode == EM_BrushClip )
	{
		AddClipMarker();
		return;
	}
	else if( GUnrealEd->Mode == EM_Polygon )
	{
		GUnrealEd->Exec( TEXT("ACTOR ADD CLASS=POLYMARKER SNAP=1") );
		return;
	}
	else if( GUnrealEd->Mode == EM_VertexEdit )
	{
		if( Cause.Buttons & MOUSE_Right && !(Cause.Buttons & MOUSE_Ctrl) )
		{
			FVector SnappedLocation = Hit.Location;
			GUnrealEd->Constraints.Snap( SnappedLocation, GGridBase );
			FVector Delta = Hit.Location - SnappedLocation;

			for( INT x = 0 ; x < VertexHitList.Num() ; x++ )
			{
				FVector* Vertex = &(VertexHitList(x).pBrush->Brush->Polys->Element(VertexHitList(x).PolyIndex).Vertex[VertexHitList(x).VertexIndex]);
				*Vertex -= Delta;
			}
			if( GUnrealEd->UseSizingBox )
			{
				TArray<ABrush*> Brushes;
				vertexedit_GetBrushList( &Brushes );
				for( INT brush = 0 ; brush < Brushes.Num() ; brush++ )
					Brushes(brush)->Brush->BuildBound();
			}
		}
		else
			vertexedit_Click( Cause.Viewport, Hit.Brush, Hit.Location, (Cause.Buttons & MOUSE_Ctrl), 0 );
	}
	if( GUnrealEd->Mode != EM_VertexEdit )
	{
		// Set new pivot point.
		GUnrealEd->Trans->Begin( TEXT("brush vertex selection") );
		GUnrealEd->SetPivot( Hit.Location, (Cause.Buttons&MOUSE_Right)!=0, 1, 0 );
		GUnrealEd->Trans->End();
	}

	unguard;
}
void FEditorHitObserver::Click( const FHitCause& Cause, const struct HActorVertex& Hit )
{
	guard(FEditorHitObserver::ClickHActorVertex);

	if( Cause.Viewport->Actor->RendMap == REN_Prefab
			|| Cause.Viewport->Actor->RendMap == REN_PrefabCompiled
			|| Cause.Viewport->Actor->RendMap == REN_StaticMeshBrowser )
		return;

	// Set new pivot point.
	GUnrealEd->Trans->Begin( TEXT("actor vertex selection") );
	GUnrealEd->SetPivot( Hit.Location, (Cause.Buttons&MOUSE_Right)!=0, 1, 0 );
	GUnrealEd->Trans->End();

	unguard;
}
void FEditorHitObserver::Click( const FHitCause& Cause, const struct HBezierControlPoint& Hit )
{
	guard(FEditorHitObserver::ClickHBezierControlPoint);

	UBOOL bExistsInList = 0;
	INT PosInList;
	for( PosInList = 0 ; PosInList < BezierControlPointList.Num() ; PosInList++ )
		if( BezierControlPointList(PosInList) == Hit )
		{
			bExistsInList = 1;
			break;
		}

	if( Cause.Buttons & MOUSE_Ctrl )
	{
		if( bExistsInList )
			BezierControlPointList.Remove(PosInList);
		else
			new( BezierControlPointList )HBezierControlPoint( Hit );
	}
	else
	{
		GUnrealEd->SelectNone( Cause.Viewport->Actor->GetLevel(), 0 );
		new( BezierControlPointList )HBezierControlPoint( Hit );
	}

	unguard;
}
void FEditorHitObserver::Click( const FHitCause& Cause, const struct HGlobalPivot& Hit )
{
	guard(FEditorHitObserver::ClickHGlobalPivot);

	if( Cause.Viewport->Actor->RendMap == REN_Prefab || Cause.Viewport->Actor->RendMap == REN_PrefabCompiled )
		return;

	if( GUnrealEd->Mode == EM_Polygon )
	{
		GUnrealEd->Exec( TEXT("ACTOR ADD CLASS=POLYMARKER SNAP=1") );
		return;
	}
	else if( GUnrealEd->Mode == EM_BrushClip )
	{
		AddClipMarker();
		debugf(TEXT("DD"));
		return;
	}
	else if( GUnrealEd->Mode == EM_FaceDrag )
		return;

	// Set new pivot point.
	GUnrealEd->Trans->Begin( TEXT("brush vertex selection") );
	GUnrealEd->SetPivot( Hit.Location, (Cause.Buttons&MOUSE_Right)!=0, 1, 0 );
	GUnrealEd->Trans->End();

	unguard;
}
void FEditorHitObserver::Click( const FHitCause& Cause, const struct HBrowserMaterial& Hit )
{
	guard(FEditorHitObserver::ClickHBrowserMaterial);
	if( Cause.Buttons==MOUSE_Left )
	{
		// Select textures.
		GUnrealEd->CurrentMaterial = Hit.Material;
		if( Cause.Viewport->Actor->RendMap != REN_TexBrowserMRU ) GTBOptions->AddMRU( GUnrealEd->CurrentMaterial );
		GUnrealEd->Exec( *FString::Printf( TEXT("POLY SET TEXTURE=%d"), (DWORD)(GUnrealEd->CurrentMaterial) ) );
		GUnrealEd->EdCallback( EDC_CurTexChange, 0, 0 );
	}
	else if( Cause.Buttons==MOUSE_Right )
	{
		// Bring up texture popup menu.
		GUnrealEd->CurrentMaterial = Hit.Material;
		if( Cause.Viewport->Actor->RendMap != REN_TexBrowserMRU ) GTBOptions->AddMRU( GUnrealEd->CurrentMaterial );
		GUnrealEd->EdCallback( EDC_RtClickTexture, 0, 0 );
	}
	unguard;
}
void FEditorHitObserver::Click( const FHitCause& Cause, const struct HTerrain& Hit )
{
	guard(FEditorHitObserver::ClickHTerrain);

	GUnrealEd->ClickLocation = Cause.Viewport->TerrainPointAtLocation;

	if( GUnrealEd->Mode == EM_TerrainEdit )
	{
		if( GTerrainTools.CurrentBrush->ID != TB_VertexEdit
				&& (Cause.Buttons != (MOUSE_Ctrl|MOUSE_Left) && Cause.Buttons != (MOUSE_Ctrl|MOUSE_Right)) )
			return;

		GUnrealEd->SelectActor( GUnrealEd->Level, Hit.TerrainInfo );

		if( GTerrainTools.CurrentBrush->ID != TB_Select )
		{
			if( !(Cause.Buttons & MOUSE_Ctrl) || GTerrainTools.CurrentBrush->ID != TB_VertexEdit )
				Hit.TerrainInfo->SelectedVertices.Empty();

			Hit.TerrainInfo->SelectVertex( Cause.Viewport->TerrainPointAtLocation );

			if( (GTerrainTools.bAutoSoftSel
					|| GTerrainTools.CurrentBrush->bForceSoftSel)
					&& GTerrainTools.CurrentBrush->bAllowSoftSel )
			{
				GUnrealEd->Exec( TEXT("TERRAIN SOFTSELECT") );
			}
		}

		if( GTerrainTools.CurrentBrush->ID != TB_TexturePan
				&& GTerrainTools.CurrentBrush->ID != TB_TextureRotate
				&& GTerrainTools.CurrentBrush->ID != TB_TextureScale )
			GTerrainTools.CurrentBrush->Execute( (Cause.Buttons & MOUSE_Left) );
	}
	else if( Cause.Buttons & MOUSE_Right && !(Cause.Buttons & MOUSE_Ctrl) ) 
	{
		GUnrealEd->SelectActor( Cause.Viewport->Actor->GetLevel(), Hit.TerrainInfo, !Hit.TerrainInfo->bSelected );
		GUnrealEd->EdCallback( EDC_RtClickWindowCanAdd, 0, (DWORD)&(Cause.Viewport->OrigCursor) );
	}

	unguard;
}

void FEditorHitObserver::Click( const FHitCause& Cause, const struct HTerrainToolLayer& Hit )
{
	guard(FEditorHitObserver::HTerrainToolLayer);

	GTerrainTools.CurrentLayer = Hit.LayerNum;
	GTerrainTools.CurrentAlphaMap = Hit.AlphaMap;
	Cause.Viewport->Repaint( 1 );

	if( Cause.Buttons == MOUSE_Right )
	{
		// Bring up terrain layer popup menu.
		GUnrealEd->EdCallback( EDC_RtClickTerrainLayer, 0, 0 );
	}

	unguard;
}

void FEditorHitObserver::Click( const FHitCause& Cause, const struct HMatineeTimePath& Hit )
{
	guard(FEditorHitObserver::HMatineeTimePath);
	GMatineeTools.SetCurrent( GUnrealEd, GUnrealEd->Level, Hit.SceneManager );
	Hit.SceneManager->PreparePath();
	Cause.Viewport->Repaint(1);

	if( Cause.Buttons==MOUSE_Right )
		GUnrealEd->EdCallback( EDC_RtClickMatScene, 0, 0 );
		
	unguard;
}

void FEditorHitObserver::Click( const FHitCause& Cause, const struct HMatineeScene& Hit )
{
	guard(FEditorHitObserver::HMatineeScene);
	GMatineeTools.SetCurrent( GUnrealEd, GUnrealEd->Level, Hit.SceneManager );
	Hit.SceneManager->PreparePath();
	Cause.Viewport->Repaint(1);

	if( Cause.Buttons==MOUSE_Right )
		GUnrealEd->EdCallback( EDC_RtClickMatScene, 0, 0 );

	unguard;
}

void FEditorHitObserver::Click( const FHitCause& Cause, const struct HMatineeAction& Hit )
{
	guard(FEditorHitObserver::HMatineeAction);

	GMatineeTools.SetCurrent( GUnrealEd, GUnrealEd->Level, Hit.SM );
	GMatineeTools.SetCurrentAction( Hit.MatAction );
	GUnrealEd->EdCallback( EDC_RefreshEditor, 1, ERefreshEditor_Matinee );

	if( Cause.Buttons==MOUSE_Right )
		GUnrealEd->EdCallback( EDC_RtClickMatAction, 0, 0 );

	Cause.Viewport->Repaint(1);

	unguard;
}

void FEditorHitObserver::Click( const FHitCause& Cause, const struct HMatineeSubAction& Hit )
{
	guard(FEditorHitObserver::HMatineeSubAction);

	GMatineeTools.SetCurrentAction( Hit.MatAction );
	GMatineeTools.SetCurrentSubAction( Hit.MatSubAction );
	GUnrealEd->EdCallback( EDC_RefreshEditor, 1, ERefreshEditor_Matinee );

	unguard;
}

void FEditorHitObserver::Click( const FHitCause& Cause, const struct HGizmoAxis& Hit )
{
	guard(FEditorHitObserver::ClickHGizmoAxis);

	if( Cause.Buttons == (MOUSE_Right | MOUSE_Ctrl) )
	{
		GbARG = 1;
		GARGAxis = Hit.Axis;
	}

	unguard;
}

void FEditorHitObserver::Click( const FHitCause& Cause, const struct HMaterialTree& Hit )
{
	guard(FEditorHitObserver::ClickHMaterialTree);
	GUnrealEd->EdCallback( EDC_MaterialTreeClick, 1, (DWORD)&Hit );
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

