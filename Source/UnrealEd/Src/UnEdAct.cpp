/*=============================================================================
	UnEdAct.cpp: Unreal editor actor-related functions
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "UnrealEd.h"

void RecomputePoly( FPoly* Poly );

/*-----------------------------------------------------------------------------
   Actor adding/deleting functions.
-----------------------------------------------------------------------------*/

//
// Copy selected actors to the clipboard.
//
void UUnrealEdEngine::edactCopySelected( ULevel* Level )
{
	guard(UUnrealEdEngine::edactCopySelected);

	// Export the actors.
	FStringOutputDevice Ar;
	UExporter::ExportToOutputDevice( Level, NULL, Ar, TEXT("copy"), 0 );
	appClipboardCopy( *Ar );

	unguard;
}

//
// Paste selected actors from the clipboard.
//
// Duplicate - Is this a duplicate operation (as opposed to a real paste)?
void UUnrealEdEngine::edactPasteSelected( ULevel* Level, UBOOL Duplicate )
{
	guard(UUnrealEdEngine::edactPasteSelected);

	// Get pasted text.
	FString PasteString = appClipboardPaste();
	const TCHAR* Paste = *PasteString;

	// Import the actors.
	Level->RememberActors();
	ULevelFactory* Factory = new ULevelFactory;
	Factory->FactoryCreateText( Level, ULevel::StaticClass(), Level->GetOuter(), Level->GetFName(), RF_Transactional, NULL, TEXT("paste"), Paste, Paste+appStrlen(Paste), GWarn );
	delete Factory;
	GCache.Flush();
	Level->ReconcileActors();
	ResetSound();

	// Offset them.
	for( INT i=0; i<Level->Actors.Num(); i++ )
		if( Level->Actors(i) && Level->Actors(i)->bSelected )
			if( Duplicate )
				Level->Actors(i)->Location += FVector(Constraints.GridSize.X,Constraints.GridSize.Y,0);
			else
				Level->Actors(i)->Location += FVector(32,32,32);

	// If we're only duplicating, just redraw the viewports.  If it's a real paste, we need
	// to update the entire editor.
	if( Duplicate )
		RedrawLevel( Level );
	else
		EdCallback( EDC_MapChange, 0, 1 );

	NoteSelectionChange( Level );

	unguard;
}

//
// Delete all selected actors.
//
void UUnrealEdEngine::edactDeleteSelected( ULevel* Level )
{
	guard(UUnrealEdEngine::edactDeleteSelected);
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if
		(	(Actor)
		&&	(Actor->bSelected)
		&&	(Level->Actors.Num()<1 || Actor!=Level->Actors(0))
		&&	(Level->Actors.Num()<2 || Actor!=Level->Actors(1))
		&&  (Actor->GetFlags() & RF_Transactional) )
		{
			if( Actor->bCollideActors )
				Level->Hash->RemoveActor(Actor);

			Level->EditorDestroyActor( Actor );
		}
	}
	NoteSelectionChange( Level );
	unguard;
}

//
// Replace all selected brushes with the default brush
//
void UUnrealEdEngine::edactReplaceSelectedBrush( ULevel* Level )
{
	guard(UUnrealEdEngine::edactReplaceSelectedBrush);

	// Untag all actors.
	for( int i=0; i<Level->Actors.Num(); i++ )
		if( Level->Actors(i) )
			Level->Actors(i)->bTempEditor = 0;

	// Replace all selected brushes
	ABrush* DefaultBrush = Level->Brush();
	for( int i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if
		(	Actor
		&&	Actor->bSelected
		&&  !Actor->bTempEditor
		&&  Actor->IsBrush()
		&&  Actor!=DefaultBrush
		&&  (Actor->GetFlags() & RF_Transactional) )
		{
			ABrush* Brush = (ABrush*)Actor;
			ABrush* NewBrush = csgAddOperation( DefaultBrush, Level, Brush->PolyFlags, (ECsgOper)Brush->CsgOper );
			if( NewBrush )
			{
				NewBrush->Modify();
				NewBrush->Group = Brush->Group;
				NewBrush->CopyPosRotScaleFrom( Brush );
				NewBrush->PostEditMove();
				NewBrush->bTempEditor = 1;
				SelectActor( Level, NewBrush, 1, 0 );
				Level->EditorDestroyActor( Actor );
			}
		}
	}
	NoteSelectionChange( Level );
	unguard;
}

static void CopyActorProperties( AActor* Dest, const AActor *Src )
{
	// Events
	Dest->Event	= Src->Event;
	Dest->Tag	= Src->Tag;

	// Object
	Dest->Group	= Src->Group;
}

//
// Replace all selected non-brush actors with the specified class
//
void UUnrealEdEngine::edactReplaceSelectedWithClass( ULevel* Level,UClass* Class )
{
	guard(UUnrealEdEngine::edactReplaceSelectedWithClass);

	// Untag all actors.
	for( int i=0; i<Level->Actors.Num(); i++ )
		if( Level->Actors(i) )
			Level->Actors(i)->bTempEditor = 0;

	// Replace all selected brushes
	for( int i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if
		(	Actor
		&&	Actor->bSelected
		&&  !Actor->bTempEditor
		&&  !Actor->IsBrush()
		&&  (Actor->GetFlags() & RF_Transactional) )
		{
			AActor* NewActor = Level->SpawnActor
			(
				Class,
				NAME_None, 
				Actor->Location,
				Actor->Rotation,
				NULL,
				0,
				1
			);
			if( NewActor )
			{
				NewActor->Modify();
				CopyActorProperties( NewActor, Actor );
				NewActor->bTempEditor = 1;
				SelectActor( Level, NewActor, 1, 0 );
				Level->EditorDestroyActor( Actor );
			}
		}
	}
	NoteSelectionChange( Level );
	unguard;
}

void UUnrealEdEngine::edactReplaceClassWithClass( ULevel* Level,UClass* Class,UClass* WithClass )
{
	guard(UUnrealEdEngine::edactReplaceClassWithClass);

	// Untag all actors.
	for( int i=0; i<Level->Actors.Num(); i++ )
		if( Level->Actors(i) )
			Level->Actors(i)->bTempEditor = 0;

	// Replace all matching actors
	for( int i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if
		(	Actor
		&&	Actor->IsA( Class )
		&&  !Actor->bTempEditor
		&&  (Actor->GetFlags() & RF_Transactional) )
		{
			AActor* NewActor = Level->SpawnActor
			(
				WithClass,
				NAME_None, 
				Actor->Location,
				Actor->Rotation,
				NULL,
				0,
				1
			);
			if( NewActor )
			{
				NewActor->Modify();
				NewActor->bTempEditor = 1;
				SelectActor( Level, NewActor, 1, 0 );
				CopyActorProperties( NewActor, Actor );
				Level->EditorDestroyActor( Actor );
			}
		}
	}
	NoteSelectionChange( Level );
	unguard;
}

/*-----------------------------------------------------------------------------
   Actor hiding functions.
-----------------------------------------------------------------------------*/

//
// Hide selected actors (set their bHiddenEd=true)
//
void UUnrealEdEngine::edactHideSelected( ULevel* Level )
{
	guard(UUnrealEdEngine::edactHideSelected);

	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && Actor!=Level->Brush() && Actor->bSelected )
		{
			Actor->Modify();
			Actor->bHiddenEd = 1;
		}
	}
	NoteSelectionChange( Level );
	unguard;
}

//
// Hide unselected actors (set their bHiddenEd=true)
//
void UUnrealEdEngine::edactHideUnselected( ULevel* Level )
{
	guard(UUnrealEdEngine::edactHideUnselected);

	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && !Cast<ACamera>(Actor) && Actor!=Level->Brush() && !Actor->bSelected )
		{
			Actor->Modify();
			Actor->bHiddenEd = 1;
		}
	}
	NoteSelectionChange( Level );
	unguard;
}

//
// UnHide selected actors (set their bHiddenEd=true)
//
void UUnrealEdEngine::edactUnHideAll( ULevel* Level )
{
	guard(UUnrealEdEngine::edactUnHideAll);
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if
		(	Actor
		&&	!Cast<ACamera>(Actor)
		&&	Actor!=Level->Brush()
		&&	Actor->GetClass()->GetDefaultActor()->bHiddenEd==0 )
		{
			Actor->Modify();
			Actor->bHiddenEd = 0;
		}
	}
	NoteSelectionChange( Level );
	unguard;
}

/*-----------------------------------------------------------------------------
   Actor selection functions.
-----------------------------------------------------------------------------*/

//
// Select all actors except cameras and hidden actors.
//
void UUnrealEdEngine::edactSelectAll( ULevel* Level )
{
	guard(UUnrealEdEngine::edactSelectAll);

	// Add all selected actors' group name to the GroupArray
	TArray<FName> GroupArray;
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && !Cast<ACamera>(Actor) && !Actor->IsHiddenEd() )
			if( Actor->bSelected && Actor->Group!=NAME_None )
				GroupArray.AddUniqueItem( Actor->Group );
	}

	// if the default brush is the only brush selected, select objects inside the default brush
	if( GroupArray.Num() == 0 && Level->Brush()->bSelected ) {
		edactSelectInside( Level );
		return;

	// if GroupArray is empty, select all unselected actors (v156 default "Select All" behavior)
	} else if( GroupArray.Num() == 0 ) {
		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor = Level->Actors(i);
			if( Actor && !Cast<ACamera>(Actor) && !Actor->bSelected && !Actor->IsHiddenEd() )
				SelectActor( Level, Actor, 1, 0 );
		}

	// otherwise, select all actors that match one of the groups,
	} else {
		// use appStrfind() to allow selection based on hierarchically organized group names
		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor = Level->Actors(i);
			if( Actor && !Cast<ACamera>(Actor) && !Actor->bSelected && !Actor->IsHiddenEd() )
			{
				for( INT j=0; j<GroupArray.Num(); j++ ) {
					if( appStrfind( *Actor->Group, *GroupArray(j) ) != NULL ) {
						SelectActor( Level, Actor, 1, 0 );
						break;
					}
				}
			}
		}
	}

	NoteSelectionChange( Level );
	unguard;
}

extern TArray<FVertexHit> VertexHitList;
extern void vertexedit_AddPosition( ABrush* pBrush, INT PolyIndex, INT VertexIndex );

void UUnrealEdEngine::edactBoxSelect( UViewport* Viewport, ULevel* Level, FVector Start, FVector End )
{
	guard(UUnrealEdEngine::edactBoxSelect);

	// Create box brush based on the start/end points (normalizes the points).
	FBox SelBBox(0);	SelBBox += Start;	SelBBox += End;

	// If we are not doing a cumulative selection (holding down SHIFT), unselect everything first.
	if( !Viewport->Input->KeyDown(IK_Shift) )
	{
		// Vertices
		VertexHitList.Empty();

		if( Mode != EM_VertexEdit )
			GUnrealEd->SelectNone( GUnrealEd->Level, 0 );
	}

	// Compare the locations of all actors to the selection planes.  If a location is in front of every
	// selection plane, select the actor.
	for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
	{
		AActor* Actor = Level->Actors(i);

		if( Actor && !Actor->IsHiddenEd() )
		{
			if( !(Viewport->Actor->ShowFlags & SHOW_StaticMeshes) && Actor->IsA( AStaticMeshActor::StaticClass() ) )
				continue;
				
			if( Mode == EM_VertexEdit )
			{
				// Select vertices
				if( Actor->bSelected && Actor->IsBrush() )
					for( int poly = 0 ; poly < ((ABrush*)Actor)->Brush->Polys->Element.Num() ; poly++ )
					{
						FPoly pPoly = ((ABrush*)Actor)->Brush->Polys->Element(poly);
						for( int vertex = 0 ; vertex < pPoly.NumVertices ; vertex++ )
						{
							FVector Location = pPoly.Vertex[vertex].TransformPointBy( Actor->ToWorld() );

							switch( Viewport->Actor->RendMap )
							{
								case REN_OrthXY:
									if( FPointPlaneDist( Location, SelBBox.Min, FVector(1,0,0) ) >= 0
											&& FPointPlaneDist( Location, SelBBox.Max, FVector(-1,0,0) ) >= 0
											&& FPointPlaneDist( Location, SelBBox.Min, FVector(0,1,0) ) >= 0
											&& FPointPlaneDist( Location, SelBBox.Max, FVector(0,-1,0) )  >= 0 )
										vertexedit_AddPosition( (ABrush*)Actor, poly, vertex );
									break;

								case REN_OrthXZ:
									if( FPointPlaneDist( Location, SelBBox.Min, FVector(1,0,0) ) >= 0
											&& FPointPlaneDist( Location, SelBBox.Max, FVector(-1,0,0) ) >= 0
											&& FPointPlaneDist( Location, SelBBox.Min, FVector(0,0,1) ) >= 0
											&& FPointPlaneDist( Location, SelBBox.Max, FVector(0,0,-1) )  >= 0 )
										vertexedit_AddPosition( (ABrush*)Actor, poly, vertex );
									break;

								case REN_OrthYZ:
									if( FPointPlaneDist( Location, SelBBox.Min, FVector(0,1,0) ) >= 0
											&& FPointPlaneDist( Location, SelBBox.Max, FVector(0,-1,0) ) >= 0
											&& FPointPlaneDist( Location, SelBBox.Min, FVector(0,0,1) ) >= 0
											&& FPointPlaneDist( Location, SelBBox.Max, FVector(0,0,-1) )  >= 0 )
										vertexedit_AddPosition( (ABrush*)Actor, poly, vertex );
									break;
							}
						}
					}
			}
			else
			{
				if( Actor != GEditor->Level->Brush() )	// Never select the builder brush
				{
					// Select brushes by their vertices ... select all other actors by their locations.
					if( Actor->IsBrush() )
					{
						for( int poly = 0 ; poly < ((ABrush*)Actor)->Brush->Polys->Element.Num() ; poly++ )
						{
							FPoly pPoly = ((ABrush*)Actor)->Brush->Polys->Element(poly);
							for( int vertex = 0 ; vertex < pPoly.NumVertices ; vertex++ )
							{
								FVector Location = pPoly.Vertex[vertex].TransformPointBy( Actor->ToWorld() );

								switch( Viewport->Actor->RendMap )
								{
									case REN_OrthXY:
										if( FPointPlaneDist( Location, SelBBox.Min, FVector(1,0,0) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Max, FVector(-1,0,0) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Min, FVector(0,1,0) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Max, FVector(0,-1,0) )  >= 0 )
											SelectActor( Level, Actor, 1, 0 );
										break;

									case REN_OrthXZ:
										if( FPointPlaneDist( Location, SelBBox.Min, FVector(1,0,0) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Max, FVector(-1,0,0) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Min, FVector(0,0,1) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Max, FVector(0,0,-1) )  >= 0 )
											SelectActor( Level, Actor, 1, 0 );
										break;

									case REN_OrthYZ:
										if( FPointPlaneDist( Location, SelBBox.Min, FVector(0,1,0) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Max, FVector(0,-1,0) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Min, FVector(0,0,1) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Max, FVector(0,0,-1) )  >= 0 )
											SelectActor( Level, Actor, 1, 0 );
										break;
								}
							}
						}
					}
					else
					{
						if( !(Viewport->Actor->ShowFlags & SHOW_Actors) )
							continue;
							
						if( Actor->DrawType == DT_StaticMesh && Actor->StaticMesh )
						{
							for( INT x = 0 ; x < Actor->StaticMesh->VertexStream.Vertices.Num() ; ++x )
							{
								FStaticMeshVertex* vtx = &Actor->StaticMesh->VertexStream.Vertices(x);

								FVector Location = Actor->LocalToWorld().TransformFVector( vtx->Position );

								switch( Viewport->Actor->RendMap )
								{
									case REN_OrthXY:
										if( FPointPlaneDist( Location, SelBBox.Min, FVector(1,0,0) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Max, FVector(-1,0,0) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Min, FVector(0,1,0) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Max, FVector(0,-1,0) )  >= 0 )
											SelectActor( Level, Actor, 1, 0 );
										break;

									case REN_OrthXZ:
										if( FPointPlaneDist( Location, SelBBox.Min, FVector(1,0,0) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Max, FVector(-1,0,0) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Min, FVector(0,0,1) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Max, FVector(0,0,-1) )  >= 0 )
											SelectActor( Level, Actor, 1, 0 );
										break;

									case REN_OrthYZ:
										if( FPointPlaneDist( Location, SelBBox.Min, FVector(0,1,0) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Max, FVector(0,-1,0) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Min, FVector(0,0,1) ) >= 0
												&& FPointPlaneDist( Location, SelBBox.Max, FVector(0,0,-1) )  >= 0 )
											SelectActor( Level, Actor, 1, 0 );
										break;
								}
							}
						}
						else
							switch( Viewport->Actor->RendMap )
							{
								case REN_OrthXY:
									if( FPointPlaneDist( Actor->Location, SelBBox.Min, FVector(1,0,0) ) >= 0
											&& FPointPlaneDist( Actor->Location, SelBBox.Max, FVector(-1,0,0) ) >= 0
											&& FPointPlaneDist( Actor->Location, SelBBox.Min, FVector(0,1,0) ) >= 0
											&& FPointPlaneDist( Actor->Location, SelBBox.Max, FVector(0,-1,0) )  >= 0 )
										SelectActor( Level, Actor, 1, 0 );
									break;

								case REN_OrthXZ:
									if( FPointPlaneDist( Actor->Location, SelBBox.Min, FVector(1,0,0) ) >= 0
											&& FPointPlaneDist( Actor->Location, SelBBox.Max, FVector(-1,0,0) ) >= 0
											&& FPointPlaneDist( Actor->Location, SelBBox.Min, FVector(0,0,1) ) >= 0
											&& FPointPlaneDist( Actor->Location, SelBBox.Max, FVector(0,0,-1) )  >= 0 )
										SelectActor( Level, Actor, 1, 0 );
									break;

								case REN_OrthYZ:
									if( FPointPlaneDist( Actor->Location, SelBBox.Min, FVector(0,1,0) ) >= 0
											&& FPointPlaneDist( Actor->Location, SelBBox.Max, FVector(0,-1,0) ) >= 0
											&& FPointPlaneDist( Actor->Location, SelBBox.Min, FVector(0,0,1) ) >= 0
											&& FPointPlaneDist( Actor->Location, SelBBox.Max, FVector(0,0,-1) )  >= 0 )
										SelectActor( Level, Actor, 1, 0 );
									break;
							}
					}
				}
			}
		}
	}

	NoteSelectionChange( Level );
	unguard;
}

//
// Select all actors inside the volume of the Default Brush
//
void UUnrealEdEngine::edactSelectInside( ULevel* Level )
{
	guard(UUnrealEdEngine::edactSelectInside);

	// Untag all actors.
	for( INT i=0; i<Level->Actors.Num(); i++ )
		if( Level->Actors(i) )
			Level->Actors(i)->bTempEditor = 0;

	// tag all candidate actors
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && !Cast<ACamera>(Actor) && Actor!=Level->Brush() && !Actor->IsHiddenEd() )
		{
			Actor->bTempEditor = 1;
		}
	}

	// deselect all actors that are outside the default brush
	UModel* DefaultBrush = Level->Brush()->Brush;
	FCoords DefaultBrushC(Level->Brush()->ToWorld());
	for( INT i=0; i<DefaultBrush->Polys->Element.Num(); i++ )
	{
		// get the plane for each polygon in the default brush
		FPoly* Poly = &DefaultBrush->Polys->Element( i );
		FPlane Plane( Poly->Base.TransformPointBy(DefaultBrushC), Poly->Normal.TransformVectorBy(DefaultBrushC) );
		for( INT j=0; j<Level->Actors.Num(); j++ )
		{
			// deselect all actors that are in front of the plane (outside the brush)
			AActor* Actor = Level->Actors(j);
			if( Actor && Actor->bTempEditor ) {
				// treat non-brush actors as point objects
				if( !Cast<ABrush>(Actor) ) {
					FLOAT Dist = Plane.PlaneDot( Actor->Location );
					if( Dist >= 0.0 ) {
						// actor is in front of the plane (outside the default brush)
						Actor->bTempEditor = 0;
					}

				} else {
					// if the brush data is corrupt, abort this actor -- see mpoesch email to Tim sent 9/8/98
					if( Actor->Brush == 0 )
						continue;
					// examine all the points
					UPolys* Polys = Actor->Brush->Polys;
					for( INT k=0; k<Polys->Element.Num(); k++ ) 
					{
						FCoords BrushC(Actor->ToWorld());
						for( INT m=0; m<Polys->Element(k).NumVertices; m++ ) 
						{
							FLOAT Dist = Plane.PlaneDot( Polys->Element(k).Vertex[m].TransformPointBy(BrushC) );
							if( Dist >= 0.0 )
							{
								// actor is in front of the plane (outside the default brush)
								Actor->bTempEditor = 0;
							}
						}
					}
				}
			}
		}
	}

	// update the selection state with the result from above
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && Actor->bSelected != Actor->bTempEditor )
			SelectActor( Level, Actor, Actor->bTempEditor, 0 );
	}
	NoteSelectionChange( Level );
	unguard;
}

//
// Invert the selection of all actors
//
void UUnrealEdEngine::edactSelectInvert( ULevel* Level )
{
	guard(UUnrealEdEngine::edactSelectInvert);
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && !Cast<ACamera>(Actor) && Actor!=Level->Brush() && !Actor->IsHiddenEd() )
			SelectActor( Level, Actor, !Actor->bSelected, 0 );
	}
	NoteSelectionChange( Level );
	unguard;
}

//
// Select all actors in a particular class.
//
void UUnrealEdEngine::edactSelectOfClass( ULevel* Level, UClass* Class )
{
	guard(UUnrealEdEngine::edactSelectOfClass);
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && Actor->GetClass()==Class && !Actor->bSelected && !Actor->IsHiddenEd() )
			SelectActor( Level, Actor, 1, 0 );
	}
	NoteSelectionChange( Level );
	unguard;
}

//
// Select all actors in a particular class and its subclasses.
//
void UUnrealEdEngine::edactSelectSubclassOf( ULevel* Level, UClass* Class )
{
	guard(UUnrealEdEngine::edactSelectSubclassOf);
	FName ClassName = Class ? Class->GetFName() : NAME_None;
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && !Actor->bSelected && !Actor->IsHiddenEd() )
		{
			for( UClass *TempClass=Actor->GetClass(); TempClass; TempClass=TempClass->GetSuperClass() )
			{
				if( TempClass->GetFName() == ClassName )
				{
					SelectActor( Level, Actor, 1, 0 );
					break;
				}
			}
		}
	}
	NoteSelectionChange( Level );
	unguard;
}

//
// Select all actors in a level that are marked for deletion.
//
void UUnrealEdEngine::edactSelectDeleted( ULevel* Level )
{
	guard(UUnrealEdEngine::edactSelectDeleted);
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && !Actor->bSelected && !Actor->IsHiddenEd() )
			if( Actor->bDeleteMe )
				SelectActor( Level, Actor, 1, 0 );
	}
	NoteSelectionChange( Level );
	unguard;
}

//
// Select all actors that have the same static mesh assigned to them as the selected ones.
//
void UUnrealEdEngine::edactSelectMatchingStaticMesh( ULevel* Level )
{
	guard(UUnrealEdEngine::edactSelectMatchingStaticMesh);

	TArray<UStaticMesh*> StaticMeshes;

	// Get a list of the static meshes in the selected actors.
	for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && Actor->bSelected )
			StaticMeshes.AddUniqueItem( Actor->StaticMesh );
	}

	// Loop through all actors in the level, selecting the ones that have
	// one of the static meshes in the list.
	for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && !Actor->IsHiddenEd() && StaticMeshes.FindItemIndex( Actor->StaticMesh ) != INDEX_NONE )
			SelectActor( Level, Actor, 1, 0 );
	}

	NoteSelectionChange( Level );
	unguard;
}

//
// Select all actors that are in the same zone as the selected actors.
//
void UUnrealEdEngine::edactSelectMatchingZone( ULevel* Level )
{
	guard(UUnrealEdEngine::edactSelectMatchingZone);

	TArray<INT> Zones;

	// Figure out which zones are represented by the current selections.
	for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && Actor->bSelected )
			Zones.AddUniqueItem( Actor->Region.ZoneNumber );
	}

	GWarn->BeginSlowTask( TEXT("Selecting ..."), 1 );

	// Loop through all actors in the level, selecting the ones that have
	// a matching zone.
	for( INT i = 0 ; i < Level->Actors.Num() ; i++ )
	{
		AActor* Actor = Level->Actors(i);
		GWarn->StatusUpdatef( i, Level->Actors.Num(), TEXT("Selecting Actors") );
		if( Actor && !Actor->IsBrush() && Zones.FindItemIndex( Actor->Region.ZoneNumber ) != INDEX_NONE )
			SelectActor( Level, Actor, 1, 0 );
	}

	// Loop through all the BSP nodes, checking their zones.  If a match is found, select the
	// brush actor that belongs to that surface.
	for( INT i = 0 ; i < Level->Model->Nodes.Num() ; ++i )
	{
		FBspNode* Node = &(Level->Model->Nodes(i));

		GWarn->StatusUpdatef( i, Level->Model->Nodes.Num(), TEXT("Selecting Brushes") );

		if( Zones.FindItemIndex( Node->iZone[1] ) != INDEX_NONE )
		{
			FBspSurf* Surf = &(Level->Model->Surfs(Node->iSurf));
			SelectActor( Level, Surf->Actor, 1, 0 );
		}
	}

	GWarn->EndSlowTask();

	NoteSelectionChange( Level );
	unguard;
}

//
// Recompute and adjust all vertices (based on the current transformations), 
// then reset the transformations
//
void UUnrealEdEngine::edactApplyTransform( ULevel* Level )
{
	guard(UUnrealEdEngine::edactApplyTransform);

	// apply transformations to all selected brushes
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && Actor->bSelected && Actor->IsBrush() )
			edactApplyTransformToBrush( (ABrush*)Actor );
	}

	unguard;
}

void UUnrealEdEngine::edactApplyTransformToBrush( ABrush* InBrush )
{
	guard(UUnrealEdEngine::edactApplyTransformToBrush);

	FModelCoords Coords;
	FLOAT Orientation = InBrush->BuildCoords( &Coords, NULL );
	InBrush->Modify();

	// recompute new locations for all vertices based on the current transformations
	UPolys* Polys = InBrush->Brush->Polys;
	Polys->Element.ModifyAllItems();
	for( INT j=0; j<Polys->Element.Num(); j++ )
	{
		Polys->Element(j).Transform( Coords, FVector(0,0,0), FVector(0,0,0), Orientation );

		// the following function is a bit of a hack.  But, for some reason, 
		// the normal/textureU/V recomputation in FPoly::Transform() isn't working.  LEGEND
		RecomputePoly( &Polys->Element(j) );
	}

	// reset the transformations
	InBrush->PrePivot = InBrush->PrePivot.TransformVectorBy( Coords.PointXform );

	InBrush->Brush->BuildBound();

	InBrush->PostEditChange();

	unguard;
}

//
// Align all vertices with the current grid
//
// NOTE:	the center handle (origin of the brush) must be selected as the current
//			pivot point, and the brush must be transformed permanently before this 
//			function will cause its vertices to be properly aligned with the grid.
//
// WARNING:	this routine does not verify that 4+ vertex polys remain coplanar.
//			the user must be careful to apply this transformation only to brushes
//			that are aligned in such a way that adjusting their vertices independently
//			will not result in a non-coplanar polygon.  (3 vertex polygons will always
//			be transformed properly.)
//
void UUnrealEdEngine::edactAlignVertices( ULevel* Level )
{
	guard(UUnrealEdEngine::edactAlignVertices);

	// apply transformations to all selected brushes
	for( INT i=0; i<Level->Actors.Num(); i++ )
	{
		AActor* Actor = Level->Actors(i);
		if( Actor && Actor->bSelected && Actor->IsBrush() )
		{
			// snap each vertex in the brush to an integer grid
			UPolys* Polys = ((ABrush*)Actor)->Brush->Polys;
			Polys->Element.ModifyAllItems();
			for( INT j=0; j<Polys->Element.Num(); j++ )
			{
				FPoly* Poly = &Polys->Element(j);
				for( INT k=0; k<Poly->NumVertices; k++ )
				{
					// snap each vertex to the nearest grid
					Poly->Vertex[k].X = appRound( ( Poly->Vertex[k].X + Actor->Location.X )  / Constraints.GridSize.X ) * Constraints.GridSize.X - Actor->Location.X;
					Poly->Vertex[k].Y = appRound( ( Poly->Vertex[k].Y + Actor->Location.Y )  / Constraints.GridSize.Y ) * Constraints.GridSize.Y - Actor->Location.Y;
					Poly->Vertex[k].Z = appRound( ( Poly->Vertex[k].Z + Actor->Location.Z )  / Constraints.GridSize.Z ) * Constraints.GridSize.Z - Actor->Location.Z;
				}

				RecomputePoly( &Polys->Element(j) );
			}

			((ABrush*)Actor)->Brush->BuildBound();

			((ABrush*)Actor)->PostEditChange();
		}
	}

	unguard;
}

/*-----------------------------------------------------------------------------
   The End.
-----------------------------------------------------------------------------*/

