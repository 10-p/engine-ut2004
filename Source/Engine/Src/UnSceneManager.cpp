/*=============================================================================
	UnSceneManager.cpp: Manages a Matinee scene
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
		* Support for simple in-editor animated actor previewing - Erik de Neve
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	ASceneManager.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(ASceneManager);

IMPLEMENT_CLASS(UMatObject);
IMPLEMENT_CLASS(UMeshObject);

IMPLEMENT_CLASS(UMatAction);
IMPLEMENT_CLASS(UActionMoveCamera);
IMPLEMENT_CLASS(UActionPause);

IMPLEMENT_CLASS(AMatDemoActor);

IMPLEMENT_CLASS(UMatSubAction);
IMPLEMENT_CLASS(USubActionFade);
IMPLEMENT_CLASS(USubActionTrigger);
IMPLEMENT_CLASS(USubActionFOV);
IMPLEMENT_CLASS(USubActionCameraShake);
IMPLEMENT_CLASS(USubActionOrientation);
IMPLEMENT_CLASS(USubActionGameSpeed);
IMPLEMENT_CLASS(USubActionSceneSpeed);
IMPLEMENT_CLASS(USubActionSubTitle);

IMPLEMENT_CLASS(ALookTarget);

// Positions all bezier control handles that are sitting inside of linear paths to be 1/3 of the way along the path (resulting in a straight line)

void ASceneManager::StraightenBezierHandles()
{
	for( int x = 0 ; x < Actions.Num() ; x++ )
	{
		UMatAction* MA = Actions(x);
		MA->StraightenBezierHandles( this );
	}
}

void UMatAction::StraightenBezierHandles( ASceneManager* InSM )
{
	UMatAction* PMA = GMatineeTools.GetPrevAction( InSM, this );

	if( (this != PMA) && IntPoint && PMA->IntPoint )
	{
		FVector Dir = PMA->IntPoint->Location - this->IntPoint->Location;
		FLOAT Sz = Dir.Size();
		Dir.Normalize();

		EndControlPoint = Dir * (Sz / 3.f);
		PMA->StartControlPoint = (Dir * -1) * (Sz / 3.f);
	}
}

void ASceneManager::PostEditChange()
{
	guard(ASceneManager::PostEditChange);
	Super::PostEditChange();
	PreparePath();
	unguard;
}

void ASceneManager::CheckForErrors()
{
	guard(ASceneManager::CheckForErrors);

	for( TArrayNoInit<class UMatAction*>::TIterator It( Actions ) ; It ; ++It )
	{
		if( (*It)->IntPoint == NULL )
			GWarn->MapCheck_Add( MCTYPE_ERROR, this, TEXT("Action found with NULL InterpolationPoint") );
	}

	unguard;
}

void UMatAction::PostEditChange()
{
	guard(UMatAction::PostEditChange);
	Super::PostEditChange();
#ifndef CONSOLE
	ASceneManager* SM = GMatineeTools.GetCurrent();
	if( SM )
		SM->PreparePath();
#endif
	unguard;
}

void UMatAction::PostLoad()
{
	guard(UMatAction::PostLoad);
	Super::PostLoad();

	// Remove references to dead actors

	if( IntPoint && IntPoint->bDeleteMe )
		IntPoint = NULL;

	unguard;
}

void USubActionOrientation::PostLoad()
{
	guard(USubActionOrientation::PostLoad);
	Super::PostLoad();

	// Remove references to dead actors

	if( CamOrientation.LookAt && CamOrientation.LookAt->bDeleteMe )
		CamOrientation.LookAt = NULL;

	unguard;
}

void UMatSubAction::PostEditChange()
{
	guard(UMatSubAction::PostEditChange);
	Super::PostEditChange();
#ifndef CONSOLE
	if( GMatineeTools.GetCurrent() )
		GMatineeTools.GetCurrent()->PreparePath();
#endif
	unguard;
}

void AInterpolationPoint::PostEditChange()
{
	guard(AInterpolationPoint::PostEditChange);
	Super::PostEditChange();
#ifndef CONSOLE
	if( GMatineeTools.GetCurrent() )
		GMatineeTools.GetCurrent()->PreparePath();
#endif
	unguard;
}
void AInterpolationPoint::PostEditMove()
{
	guard(AInterpolationPoint::PostEditMove);
	Super::PostEditMove();
#ifndef CONSOLE
	ASceneManager* SM = GMatineeTools.GetCurrent();
	if( SM )
	{
		SM->PreparePath();

		// Find the action associated with this IP and let it adjust itself

		UMatAction* MA = GMatineeTools.GetActionFromIP( SM, this );
		if( MA )
			MA->PostEditChange();
	}
#endif
	unguard;
}

UMatAction* ASceneManager::GetActionFromPct( FLOAT InPctSceneComplete )
{
	guard(ASceneManager::GetActionFromPct);

	for( INT x = 0 ; x < Actions.Num() ; ++x )
		if( Actions(x)->PctEnding >= InPctSceneComplete )
			return Actions(x);

	return NULL;

	unguard;
}


// Temp structure for anim playback digestion.
struct MatAnimSimEvent
{
	FName ScriptTag;
	FName WaitsFor;
	FName Sequence;		
	FLOAT Rate;
	FLOAT Time;
	FLOAT StartFrame;
};

//
// Limited editor-only animated actor preview support. Basically this tries to fake
// what would happen in a real in-engine scene, so obviously much of the action 
// which relies on cause-and-effect events won't work, or won't sync properly.
// Must reside here since in-editor matinee preview window uses ASceneManager::Tick for its 'play' mode.
//
void ASceneManager::InitializePreviewActors()
{
	 if( !GIsEditor ) return;

	 // Work with the actor list that's declared in MatineePreview.h...
	 FPreviewCinActorList* SimStoryBook = &(GMatineeTools.PCAList);
		 
	 // Clear PCA List ?
	 if( SimStoryBook->CastList.Num() )
		 CleanupPreviewActors();
	 
	 debugf(TEXT("Refreshing Matinee-preview simulated animation storybook."));

	//      >> Workings:
	//       - Get list of all triggers: look through current scene's actions, and see which ones trigger an event; for those events, record the triggers.
	//       - Get list of all "UnrealScriptedSequence" AIScripts which contain a WaitForEvent that matches a recorded triggerevent.
	//       - get list of all actors: the APAwn ones that have an AIScriptTag.
	//       - Now make sure any actor that receives a playanim becomes part of the SimStoryBook and has its 'movement script' intialized.
	//
	//       - Spawn the temporary actors and hide the 'real' ones.
	// 
	//       * - Play the relevant states in UpdateViewerFromPct. When pawns are hidden, you could show it with skeletons or wireframe...( Additional instance trick ?)
	//         - This may happen at any stage(Slider!) So characters may have to move to any Sequence/animframe state, and hidden/shown state any time.
	//
	//       <<- Clean up all cinematic (and unhide the pawns..-> preferably with an INSTANCE trick which doesn't affect the level at all. ) 
	//	

    TArray<APawn*> TempPawns;
	TArray<FName>  TempPawnScriptTags;
	 // All pawns that may have AIScript tags.	
	for( TObjectIterator<APawn> It; It; ++It )
	{
		APawn*  TestPawn = *It;		
		if( TestPawn->AIScriptTag != NAME_None )
		{
			// Accumulate aiscript-linked pawns.
			TempPawns.AddItem(TestPawn);
			TempPawnScriptTags.AddItem(TestPawn->AIScriptTag);
			//debugf(TEXT("AIscript tag for pawn %s is %s "),TestPawn->GetName(),*(TestPawn->AIScriptTag) );
		}
	}

	TArray<FName>  TempMatineeTriggers;
	TArray<FLOAT>  TempMatineeTriggerTimes;

	//
	// Default playerstart trigger ?!?!?
	//
	//

	// Default scene EventStart trigger ?
	if( EventStart != NAME_None )
	{
		TempMatineeTriggers.AddItem( EventStart );
		TempMatineeTriggerTimes.AddItem( 0.0f );
		//debugf(TEXT("EventStartTrigger for time: %f trigger: [%s] "),0.0f,*EventStart);
	}
	
	// Accumulate all subactions->triggers..	
	for( INT i=0; i<SubActions.Num(); i++)
	{
		USubActionTrigger* SAT = Cast<USubActionTrigger>( SubActions(i) );
		if ( SAT && (SAT->EventName != NAME_None) )
		{
			TempMatineeTriggers.AddItem( SAT->EventName );
			TempMatineeTriggerTimes.AddItem( SAT->PctStarting );
			//debugf(TEXT("SubActiontrigger for time: %f - %f  trigger: [%s] "), SAT->PctStarting, SAT->PctEnding, *(SAT->EventName) ); 
		}
	}
	
	// Depend on SetSceneStartTime being called earlier so that SubActions array has been filled.

	TArray<MatAnimSimEvent> SimEvents;
	
	// Find UnrealScriptedSequence, which has the Actions array...
	for( TObjectIterator<AAIScript> It; It; ++It )
	{	
		// Is it a class ScriptedSequence - and a proper instance ? 			
		FString ClassName = It->GetClass()->GetName();		
		if( ClassName == TEXT("UnrealScriptedSequence") )
		{					
			// debugf(TEXT("Found UnrealScriptedSequence AIScript: [%s]"), It->GetName() ); 
			// debugf(TEXT("With (Events)Tag: [%s]"),*( ((AAIScript*)(*It))->Tag) );
			FName EventScriptTag = ((AAIScript*)(*It))->Tag;

			FArray* NewArray = NULL;		
			INT     ElementSize = 0;

			if( (*It)->FindArrayProperty(TEXT("Actions"), &NewArray, &ElementSize ) )
			{		
				// Record any WAITFOREVENT/PLAYANIM pair.
				FName MatAnimWaitsFor = NAME_None;
				FLOAT MatAnimRate = 0.0f;
				FLOAT MatStartFrame = 0.0f;
				FName MatAnimSequence = NAME_None;				

				//debugf(TEXT("Found Actions array, elementsize: %i"), ElementSize );
				for( INT i=0;i<NewArray->Num();i++ )
				{					
					UObject** Obj = (UObject**)( (BYTE*)NewArray->GetData() + i * ElementSize );
					{
						// debugf(TEXT("Found array item [%s] class [%s]"), (*Obj)->GetName(),(*Obj)->GetClass()->GetName() );						
						FString ClassName = (*Obj)->GetClass()->GetName();
						if( ClassName == TEXT("Action_WAITFOREVENT") )
						{
							FName EventName;
							if( (*Obj)->FindFNameProperty( TEXT("ExternalEvent"), &EventName ) )
							{
								MatAnimWaitsFor = EventName;
								//debugf(TEXT("EventTag found: [%s]"),*EventName);
							}								
						}
						if( ClassName == TEXT("Action_PLAYANIM") )
						{
							FName SequenceName;
							if( (*Obj)->FindFNameProperty( TEXT("BaseAnim"), &SequenceName ) )
							{
								MatAnimSequence = SequenceName;
								//debugf(TEXT("PlayAnim action found. BaseAnim sequence: [%s]"),*SequenceName );							
							}
							FLOAT AnimRate;
							if( (*Obj)->FindFloatProperty( TEXT("AnimRate"), &AnimRate ) )
							{
								MatAnimRate = AnimRate;
								//debugf(TEXT("PlayAnim action found. BaseAnim sequence: [%s]"),*SequenceName );							
							}
							FLOAT StartFrame;
							if( (*Obj)->FindFloatProperty( TEXT("StartFrame"), &StartFrame ) )
							{
								MatStartFrame = StartFrame;								
							}
						}						
						if( ClassName == TEXT("Action_DESTROYPAWN") ) // should hide it, from whenever this action occurs...
						{
						}
						if( ClassName == TEXT("ACTION_WAITFORANIMEND") ) //
						{
							// Figure out playtime of last anim ?
						}
						if( ClassName == TEXT("Action_WAITFORTIMER") ) // float PauseTime
						{
							// Not supported - so don't play anything hereafter, by invalidating this scripts stored tag...
							EventScriptTag = NAME_None;
						}
						if( ClassName == TEXT("ACTION_FreezeOnAnimEnd") ) //
						{
						}
					}

					// Any animation sequence without a preceding waitforevent will be ignored.
					if( (MatAnimSequence != NAME_None) && (MatAnimWaitsFor == NAME_None ) )
					{
						MatAnimSequence = NAME_None;
						MatAnimRate = 0.0f;
					}

					// If a WaitsFor and AnimSequence pair has been found,
					// Commit it to the array here.  
					if( (MatAnimSequence != NAME_None) && (MatAnimWaitsFor != NAME_None ) )
					{
						//debugf(TEXT("Recording action from Aiscript %s EventScrTag: %s Event: %s  PlayAnim: (%s) "),((AAIScript*)(*It))->GetName(),*EventScriptTag,*MatAnimWaitsFor,*MatAnimSequence);

						MatAnimSimEvent TempEvent;
						TempEvent.ScriptTag =  EventScriptTag;
						TempEvent.WaitsFor = MatAnimWaitsFor;
						TempEvent.Sequence = MatAnimSequence;
						TempEvent.Rate = MatAnimRate;
						TempEvent.StartFrame = MatStartFrame; 
						TempEvent.Time = 0; // Match later..
						SimEvents.AddItem( TempEvent );
						
						MatAnimWaitsFor = NAME_None;
						MatAnimSequence = NAME_None;
						MatAnimRate = 0.0f;
					}					
				}					
			}
		}
	}	

	//
	// Knit together events, triggers, and aiscripted pawns into the 'action sequences' that contain the state
	// of any actor at any time so we can scrub through arbitrary time frames.
	//
	
	// Hook up aiscript event times with triggers.
	for( INT s=0; s< SimEvents.Num(); s++)
	{
	    SimEvents(s).Time = 0.0f;
		//debugf(TEXT("Trying to find anim event time for waitfor %s"),*SimEvents(s).WaitsFor ); 

		// Link up event with matinee trigger:
		for( INT m=0; m< TempMatineeTriggers.Num(); m++ ) 
		{
			if( SimEvents(s).WaitsFor == TempMatineeTriggers(m) )
			{
				// Match between 'waitforevent' and Matinee 'trigger event' found.
				SimEvents(s).Time = TempMatineeTriggerTimes(m);
			}
		}
		//debugf(TEXT("Found anim event time for waitfor %s is %f "),*SimEvents(s).WaitsFor,SimEvents(s).Time ); 
	}	

	// For each actor, establish an actions list.
	for( INT p=0; p< TempPawns.Num(); p++ )
	{	
		// Add pawn to the cin preview list.		 
		INT CastIndex = SimStoryBook->CastList.AddZeroed();
		SimStoryBook->CastList(CastIndex).SourceActor = TempPawns(p);
		SimStoryBook->CastList(CastIndex).TempActor = NULL;			

		// Any Pawnscripts matches for this pawn ?
		for( INT s=0; s< SimEvents.Num(); s++)
		{
			if( SimEvents(s).ScriptTag == TempPawnScriptTags(p) )
			{	
				// This tagged anim event is relevant for our pawn.												
				INT ActionIndex = SimStoryBook->CastList(CastIndex).Actions.AddZeroed();					
				FPreviewAction* Action = &(SimStoryBook->CastList(CastIndex).Actions(ActionIndex));					
				Action->StartTime   = SimEvents(s).Time;
				Action->BaseAnimSeq = SimEvents(s).Sequence;
				Action->AnimRate    = SimEvents(s).Rate; 
				Action->StartFrame  = SimEvents(s).StartFrame;
				//debugf(TEXT("Action for %s time %f rate %f animseq [%s] ScrTag(%s)"),TempPawns(p)->GetName(),Action->StartTime,Action->AnimRate,*Action->BaseAnimSeq, *TempPawnScriptTags(p)); //#SKEL
			}
		}

		// Delete pawn from the scrub-movie-script if it had no sequences, or if it does not have an animatable (LOD) Mesh....
		if(! SimStoryBook->CastList(CastIndex).Actions.Num() || 
			!SimStoryBook->CastList(CastIndex).SourceActor->Mesh ||
			!SimStoryBook->CastList(CastIndex).SourceActor->Mesh->IsA(ULodMesh::StaticClass())
			)
		{
			// Remove any actions for this actor...
			SimStoryBook->CastList(CastIndex).Actions.Empty();
			// Remove actor from cast.
			SimStoryBook->CastList.Remove(CastIndex);
		}
	}

	//	
	// Hide the real ones: make sure all have instances, and make the original ones wireframe.
	//
	for( INT a=0; a<SimStoryBook->CastList.Num(); a++)
	{		
		//debugf(TEXT("CINEMATICPREVIEW Prepared actor: [%s] Actions: [%i]"),SimStoryBook->CastList(a).SourceActor->GetName(), SimStoryBook->CastList(a).Actions.Num() ); 	
		AActor* OrigActor = SimStoryBook->CastList(a).SourceActor;	
		// Spawn demo-only actor.
		// The fact that they're AMatDemoActors marks them as
		// expendable, not to be saved with the level..
		AActor* CinActor = GetLevel()->SpawnActor( AMatDemoActor::StaticClass() ); 
		if( CinActor )
		{
			SimStoryBook->CastList(a).TempActor = CinActor;				

			// Update internal level-array indices (for checking freshness each frame..)
			SimStoryBook->CastList(a).SourceActorIndex = GetLevel()->Actors.FindItemIndex( OrigActor );
			SimStoryBook->CastList(a).TempActorIndex = GetLevel()->Actors.FindItemIndex( CinActor );
			SimStoryBook->CastList(a).CurrentMesh = OrigActor->Mesh;
			
			// Initialize everything else visually relevant.
			//CinActor->Location = OrigActor->Location;
			CinActor->Mesh = OrigActor->Mesh;
			CinActor->DrawScale = OrigActor->DrawScale;				
			CinActor->Rotation = OrigActor->Rotation;
			CinActor->DrawScale3D = OrigActor->DrawScale3D;
			CinActor->AmbientGlow = OrigActor->AmbientGlow;
			CinActor->DrawType = OrigActor->DrawType;
			
			GetLevel()->FarMoveActor( CinActor, OrigActor->Location );
			//CinActor->SetDrawType(DT_Mesh); // Also adds actor to hash.

			// Initialize its meshinstance.
			CinActor->Mesh->MeshGetInstance(CinActor);

			OrigActor->Mesh->MeshGetInstance(OrigActor);				
			// Skeletal/LOD meshes only: force original pawn copy to wireframe.
			ULodMeshInstance* MInst = (ULodMeshInstance*)OrigActor->Mesh->MeshGetInstance(OrigActor);
			MInst->bForceWireframe = true;			
		}						
	}

	// Note current scene pointer.
	SimStoryBook->CurrentScenePtr = (void*)this;
}


// Update status of all actors according to InScenePct.
void ASceneManager::UpdatePreviewActors( FLOAT InScenePct )
{	
	FPreviewCinActorList* SimStoryBook = &(GMatineeTools.PCAList);

	// Several necessary precautions against levelsaves/loads while matinee window open, or any
	// other actor deletion/modification while window open.

	// Reasonably fast: verify freshness of each actor in our storybook.
	// - Must be in same spot and be valid in the level's actor array.	
	UBOOL RefreshNeeded = false;
	// Scenemanager changed ? 
	if( SimStoryBook->CurrentScenePtr != (void*)this )
	{
		RefreshNeeded = true;
	}
	else
	{
		INT TotalLevelActors = GetLevel()->Actors.Num();
		for( INT a=0; a<SimStoryBook->CastList.Num(); a++)
		{
			if(!( ( SimStoryBook->CastList(a).SourceActorIndex < TotalLevelActors ) &&
				( GetLevel()->Actors( SimStoryBook->CastList(a).SourceActorIndex ) ==  SimStoryBook->CastList(a).SourceActor )  &&
				( SimStoryBook->CastList(a).TempActorIndex < TotalLevelActors ) &&
				( GetLevel()->Actors( SimStoryBook->CastList(a).TempActorIndex ) ==  SimStoryBook->CastList(a).TempActor ) &&
				( SimStoryBook->CastList(a).CurrentMesh == SimStoryBook->CastList(a).SourceActor->Mesh )
				))
			{
				RefreshNeeded = true;			
				break;
			}
		}
	}

	// Reinitialize whenever needed (i.e. level saves/loads while matinee window is open !!)
	if( RefreshNeeded )
	{
		CleanupPreviewActors();
		InitializePreviewActors();
	}

	// Force each temp. actor into its desired state.
	for( INT i=0; i<SimStoryBook->CastList.Num(); i++)
	{
		AActor* CinActor = SimStoryBook->CastList(i).TempActor;
		if(! CinActor->Mesh ) // Something might have changed with the mesh during frames..
			break;

		ULodMeshInstance* MInst = (ULodMeshInstance*)CinActor->Mesh->MeshGetInstance(CinActor);

		FLOAT ClosestEventTime = -1.0f;
		INT   BestActionIndex = -1;

		for( INT a=0; a< SimStoryBook->CastList(i).Actions.Num(); a++)
		{
			FLOAT ActionTime = SimStoryBook->CastList(i).Actions(a).StartTime;
			if( ActionTime < InScenePct )
			{
				// Closer than ClosestEvent ?
				if( ClosestEventTime < ActionTime )
				{
					ClosestEventTime = ActionTime;					
					BestActionIndex = a;
				}
			}
		}	
		// debugf(TEXT("BestActioIndex %i For Actor: %s "),BestActionIndex, CinActor->GetName());   
		if( BestActionIndex >= 0  && CinActor->Mesh )
		{
			FPreviewAction* Action = &(SimStoryBook->CastList(i).Actions(BestActionIndex));					
			
			// Calculate animation length in 'scene pct's' considering the rate and TotalSceneTime
			// TotalSceneTime 
			
			HMeshAnim Seq = MInst->GetAnimNamed( Action->BaseAnimSeq );
			FLOAT EffectiveRate = Action->AnimRate * ( Seq ? MInst->AnimGetRate(Seq): 1.0f );
			FLOAT NumFrames = Seq ? MInst->AnimGetFrameCount(Seq) : 1.0f;
			FLOAT SequenceDuration = (NumFrames - Action->StartFrame) / EffectiveRate;  // seconds						
			FLOAT PoseAnimFrame = 0.0f;

			// Safeguard
			if( SequenceDuration < 0.f ) 
				SequenceDuration = 0.f;

			FLOAT SceneTimeSinceTrigger = ( InScenePct- Action->StartTime ) * TotalSceneTime;

			//debugf(TEXT("PCT: %f  SceneTimeSinceTrigger: %f   SceneTimeTotal %f  SequenceDuration %f  "),InScenePct,SceneTimeSinceTrigger, TotalSceneTime, SequenceDuration ); 

			if( CinActor->Mesh->IsA(USkeletalMesh::StaticClass()) )
				((USkeletalMeshInstance*)MInst)->bForceRefpose = false;

			if ( SceneTimeSinceTrigger <= SequenceDuration ) // Seconds left ?
			{
				// Normalized animframe.
				PoseAnimFrame = SceneTimeSinceTrigger * EffectiveRate;  // result in frames.

				// Force actor pose into meshinstance.
				MInst->PlayAnim( 0, Action->BaseAnimSeq, 0.0, 0.0, 0 );				
				MInst->FreezeAnimAt( PoseAnimFrame + Action->StartFrame, 0 );

				FName ActiveSequence = MInst->GetActiveAnimSequence(0);
				// debugf(TEXT("## Active anim sequence: %s AF %f "), *ActiveSequence, PoseAnimFrame ); 
			}
			else
			{				
				// Animation ended, and no looping: assume 'hide' again by going to refpose, instead of freezing...?
				//if( CinActor->Mesh->IsA(USkeletalMesh::StaticClass()) )
				//	((USkeletalMeshInstance*)MInst)->bForceRefpose = true;
				 MInst->PlayAnim( 0, Action->BaseAnimSeq, 0.0f, 0.0f, 0 );
				 MInst->FreezeAnimAt( (FLOAT)(NumFrames-1), 0 ); // LAST frame. In FRAMES ! ...
			}
		}
		else
		{
			// No preceding anim - force the reference pose, if not already...			
			if( CinActor->Mesh->IsA(USkeletalMesh::StaticClass()) )
				((USkeletalMeshInstance*)MInst)->bForceRefpose = true;
		}
	}	
}

//
// Delete the dummy duplicates, and restore the real ones back from wireframe..
//
void ASceneManager::CleanupPreviewActors()
{
	 FPreviewCinActorList* SimStoryBook = &(GMatineeTools.PCAList);

	 // Clear PCA List, delete if necessary.
	 for( INT i=0; i<SimStoryBook->CastList.Num(); i++)
	 {
		// Clean up temp actors (or make invisible until cleaned up.)
		if( SimStoryBook->CastList(i).TempActor &&  ( GetLevel()->Actors.FindItemIndex(SimStoryBook->CastList(i).TempActor) != INDEX_NONE) )
		{
			SimStoryBook->CastList(i).TempActor->bHiddenEd = true; // Cosmetic?
			GetLevel()->EditorDestroyActor( SimStoryBook->CastList(i).TempActor ); 
		}
			
		// Clean up wireframe settings... only for actors that are still in the level...
		if( SimStoryBook->CastList(i).SourceActor && ( GetLevel()->Actors.FindItemIndex(SimStoryBook->CastList(i).SourceActor) != INDEX_NONE) )
		{
			UMesh* SrcMesh = SimStoryBook->CastList(i).SourceActor->Mesh;
			if( SrcMesh && SrcMesh->IsA(ULodMesh::StaticClass() ) )
			{				
				ULodMeshInstance* MInst = (ULodMeshInstance*)SrcMesh->MeshGetInstance(SimStoryBook->CastList(i).SourceActor);
				MInst->bForceWireframe = false;
			}
		}
		SimStoryBook->CastList(i).Actions.Empty();
	 }
	 SimStoryBook->CastList.Empty();
}

void ASceneManager::SceneEnded()
{
	guard(ASceneManager::SceneEnded);

	bIsRunning = 0;
	bIsSceneStarted = 0;
	PrevOrientation.MA = NULL;

	eventSceneEnded();	
	DeletePathSamples();

	GMatineeTools.ScenesExecuting--;

	APlayerController* PC = Cast<APlayerController>(Viewer);
	if( bCinematicView && PC )
	{
		UViewport* VP = Cast<UViewport>(PC->Player);
		if( VP )	VP->bRenderCinematics = 0;
	}

	unguard;
}

void ASceneManager::SceneStarted()
{
	guard(ASceneManager::SceneStarted);

	bIsRunning = 1;

	SetSceneStartTime();
	eventSceneStarted();

	// Reset all work variables to something reasonable
	SceneSpeed = 1.f;
	CurrentAction = NULL;
	ChangeOrientation( FOrientation() );

	GMatineeTools.ScenesExecuting++;

	APlayerController* PC = Cast<APlayerController>(Viewer);
	if( bCinematicView && PC )
	{
		UViewport* VP = Cast<UViewport>(PC->Player);
		if( VP )	VP->bRenderCinematics = 1;
	}

	unguard;
}

// This is broken out to allow the editor to set up the scene.
void ASceneManager::SetSceneStartTime()
{
	guard(ASceneManager::SetSceneStartTime);

	TotalSceneTime = GetTotalSceneTime();
	if( !TotalSceneTime )
		return;

	if( !GIsEditor )
		CurrentTime = 0;

	// Set up the actions/subactions
	FLOAT WkCurrentTime = 0;
	SubActions.Empty();
	for( INT x = 0 ; x < Actions.Num() ; ++x )
	{
		UMatAction* MA = Actions(x);
		MA->PctStarting = WkCurrentTime / TotalSceneTime;
		MA->PctEnding = (WkCurrentTime + MA->Duration) / TotalSceneTime;
		MA->PctDuration = MA->PctEnding - MA->PctStarting;

		for( INT y = 0 ; y < MA->SubActions.Num() ; ++y )
		{
			UMatSubAction* MSA = MA->SubActions(y);

			MSA->Status = SASTATUS_Waiting;

			MSA->PctStarting = (WkCurrentTime + MSA->Delay) / TotalSceneTime;
			MSA->PctEnding = (WkCurrentTime + MSA->Delay + MSA->Duration) / TotalSceneTime;
			MSA->PctDuration = MSA->PctEnding - MSA->PctStarting;

			USubActionOrientation* SAO = Cast<USubActionOrientation>( MSA );
			if( SAO )
			{
				SAO->CamOrientation.MA = MSA;

				SAO->CamOrientation.PctInStart = MSA->PctStarting;
				SAO->CamOrientation.PctInEnd = SAO->CamOrientation.PctInStart + ( SAO->CamOrientation.EaseInTime / TotalSceneTime );
				SAO->CamOrientation.PctInDuration = SAO->CamOrientation.PctInEnd - SAO->CamOrientation.PctInStart;
			}

			SubActions.AddItem( MSA );
		}

		WkCurrentTime += MA->Duration;
	}

	unguard;
}

// Refreshes all subactions and updates their status indicators based on the % passed in.
void ASceneManager::RefreshSubActions( float InScenePct )
{
	for( INT x = 0 ; x < Actions.Num() ; ++x )
		for( INT y = 0 ; y < Actions(x)->SubActions.Num() ; ++y )
		{
			UMatSubAction* MSA = Actions(x)->SubActions(y);

			if( InScenePct < MSA->PctStarting )
				MSA->Status = SASTATUS_Waiting;
			else if( InScenePct >= MSA->PctStarting && InScenePct <= MSA->PctEnding )
				MSA->Status = SASTATUS_Running;
			else if( InScenePct > MSA->PctEnding )
				MSA->Status = SASTATUS_Expired;
		}
}

// This is a specialty function used by the editor to jump to specific spots in the scene.
void ASceneManager::SetCurrentTime( float InTime )
{
	CurrentTime = InTime;
	PrevOrientation.MA = NULL;
	RefreshSubActions( InTime / TotalSceneTime );
}

void ASceneManager::PostBeginPlay()
{
	guard(ASceneManager::PostBeginPlay);
	Super::PostBeginPlay();
	PreparePath();
	unguard;
}

// Global tick
UBOOL ASceneManager::Tick( FLOAT DeltaTime, enum ELevelTick TickType )
{
	guard(ASceneManager::Tick);

	INT TickStartTime = appCycles();

	if( !Super::Tick(DeltaTime, TickType) && !bIsRunning )
		return false;

	// If scene is not running, we don't have anything to do.
	if( !bIsRunning )
		return true;

	// If this is the first tick since the scene was started,
	// record the starting info.
	if( !bIsSceneStarted )
	{
		bIsSceneStarted = 1;

		// ===
		// SCENE STARTING
		// ===

		SceneStarted();

		// Sanity checks
		if( !Viewer )
		{
			debugf( TEXT("Matinee ERROR : Scene '%s' : Couldn't find viewer for scene"), *Tag );
			SceneEnded();
			return true;
		}
		else
		{
			// When the path first starts, force the viewer rotation to match that of the first interpolation point
			if( Actions.Num() )
			{
				if( Actions(0)->IntPoint == NULL )
					debugf( TEXT("Matinee WARNING : Scene '%s' : first action has NULL interpolation point"), *Tag );
				else
				{
					FCheckResult Hit(1.f);
					GetLevel()->MoveActor(Viewer, FVector(0.f,0.f,0.f), Actions(0)->IntPoint->Rotation, Hit);
				}
			}
		}
	}
	else
	{
		if( !RotInterpolator.IsDone() )
			RotInterpolator.Tick( DeltaTime );
		
		CurrentTime += DeltaTime*SceneSpeed;
	}

	// Figure out how far we are into the scene
	PctSceneComplete = CurrentTime / TotalSceneTime;

	// If we are over 100% along the timeline, end the scene
	if( PctSceneComplete > 1.f )
	{
		// ===
		// SCENE ENDING
		// ===

		SceneEnded();
		if( !bAbortCinematic && NextSceneTag != NAME_None )
			eventTriggerEvent( NextSceneTag, Viewer, OldPawn );
		else
			if( bLooping )
				eventTriggerEvent( Tag, Viewer, OldPawn );
	}
	else
	{
		// Update the viewer
		FVector PreviousLoc = Viewer->Location;
		UpdateViewerFromPct( PctSceneComplete );
		if ( DeltaTime > 0.f )
			Viewer->Velocity = (Viewer->Location - PreviousLoc)/DeltaTime;
	}

	GStats.DWORDStats( GEngineStats.STATS_Matinee_TickCycles ) += (appCycles() - TickStartTime);

	return true;
	unguard;
}

// Converts a scene wide % into a % relative to the current action.
FLOAT ASceneManager::GetActionPctFromScenePct( FLOAT InScenePct )
{
	guard(ASceneManager::GetActionPctFromScenePct);

	if( !CurrentAction )
	{
		CurrentAction = GetActionFromPct( InScenePct );
		if( !CurrentAction )
		{
			debugf( TEXT("Matinee ERROR : Scene '%s' : GetActionFromPct failed (using 100%)"), *Tag );
			return 100.f;
		}
	}

	FLOAT Pct = (InScenePct - CurrentAction->PctStarting) / (CurrentAction->PctDuration?CurrentAction->PctDuration:1);
	Pct = Min<FLOAT>( Max<FLOAT>( Pct, 0.0001f ), 100.f );	// Clamp the percentage between 0-100%
	return Pct;
	
	unguard;
}

// Updates the viewing actor from a percentage.  The percentage represents where
// we are along the timeline.
void ASceneManager::UpdateViewerFromPct( FLOAT InScenePct )
{
	guard(ASceneManager::UpdateViewerFromPct);

	InScenePct = Min<FLOAT>( Max<FLOAT>( InScenePct, 0.0001f ), 100.f );	// Clamp the percentage between 0-100%

	// ===
	// Location
	// ===

	CurrentAction = GetActionFromPct( InScenePct );

	FVector Location = GetLocation( &CurrentAction->SampleLocations, InScenePct );
	GetLevel()->FarMoveActor( Viewer, Location, false, true, true );

	// ===
	// Give the sub actions a chance to update themselves
	// ===

	CameraShake = FVector(0,0,0);

	if( GIsEditor ) RefreshSubActions( InScenePct );
	for( INT x = 0 ; x < SubActions.Num() ; ++x )
		if( SubActions(x)->Status != SASTATUS_Expired )
			SubActions(x)->Update( InScenePct, this );

	// ===
	// Rotation
	// ===

	// Get the rotation after the subactions update in case one of them wants to override the camera orientation.
	FRotator Rotation = GetRotation( &SampleLocations, InScenePct, Viewer->Location, Viewer->Rotation, CurrentAction );
	FCheckResult Hit(1.f);
	GetLevel()->MoveActor(Viewer, FVector(0.f,0.f,0.f), Rotation, Hit);

	// ===
	// In-Editor cinematic character animation preview.
	// ===
	if( GIsEditor ) 
	{
		UpdatePreviewActors( InScenePct );
	}

	unguard;
}

// Figures out how long the scene will be, in seconds, by
// examining the action list.
FLOAT ASceneManager::GetTotalSceneTime()
{
	guard(ASceneManager::GetTotalSceneTime);

	FLOAT Result = 0.0f;
	for( INT x = 0 ; x < Actions.Num() ; ++x )
	{
		if( Actions(x) )
			Result += Actions(x)->Duration;
	}
	return Result;

	unguard;
}

void ASceneManager::execAbortScene( FFrame& Stack, RESULT_DECL )
{
	guard(ASceneManager::execAbortScene);
	P_FINISH;

	CurrentTime		= TotalSceneTime;
	bAbortCinematic	= true;

	unguard;
}

void ASceneManager::execGetTotalSceneTime( FFrame& Stack, RESULT_DECL )
{
	guard(ASceneManager::execGetTotalSceneTime);
	P_FINISH;
	*(FLOAT*)Result = GetTotalSceneTime();
	unguard;
}

FVector ASceneManager::GetLocation( TArray<FVector>* InSampleLocations, FLOAT InScenePct )
{
	guard(ASceneManager::GetLocation);

	FLOAT ActionPct = GetActionPctFromScenePct( InScenePct );

	FVector NewLocation;

	if( CamOrientation.CamOrientation == CAMORIENT_Dolly )
		NewLocation = CamOrientation.DollyWith->Location - DollyOffset;
	else
	{
		NewLocation = Viewer->Location;

		if( InSampleLocations->Num() )
		{
			FLOAT fIndex = (InSampleLocations->Num()-1) * ActionPct;
			INT Index = (INT)fIndex;		
			NewLocation = (*InSampleLocations)(Index);
			if( fIndex - Index )	NewLocation += ((*InSampleLocations)(Index+1) - NewLocation) * (fIndex - Index);
		}
	}

	return NewLocation;

	unguard;
}

class FPathIdx
{
public:
	FPathIdx()
	{}
	FPathIdx( ASceneManager* InSM, INT InIdx, UMatAction* InMA )
	{
		Idx = InIdx;
		MA = InMA;

		if( Idx >= FMatineeTools::NUM_SAMPLE_POINTS-1 )
		{
			Idx -= FMatineeTools::NUM_SAMPLE_POINTS-1;
			MA = GMatineeTools.GetNextAction( InSM, MA );
		}

		// If this is the last action, don't wrap around to the first action.  It causes wild camera swings.
		if( MA == InSM->Actions( 0 ) )
		{
			MA = InMA;
			Idx = FMatineeTools::NUM_SAMPLE_POINTS-1;
		}
	}
	~FPathIdx()
	{}

	INT Idx;
	UMatAction* MA;
};

FRotator ASceneManager::GetRotation( TArray<FVector>* InSampleLocations, FLOAT InScenePct, FVector InViewerLocation, FRotator InViewerRotation, UMatAction* InMA, UBOOL bUseBlending )
{
	guard(ASceneManager::GetRotation);

	// For the editor, figure out which orientation is current each time this function is called.
	if( GIsEditor )
	{
		// Find the relevant orientation
		FOrientation CO;
		for( INT x = 0 ; x < SubActions.Num() ; ++x )
		{
			USubActionOrientation* SAO = Cast<USubActionOrientation>( SubActions(x) );
			if( SAO && SAO->PctStarting < InScenePct )
				CO = SAO->CamOrientation;
		}

		// Only change the orientation if we need to.
		if( CO != PrevOrientation )
		{
			PrevOrientation = CO;
			ChangeOrientation( CO );
		}
	}

	FRotatorF DesiredRotation = InViewerRotation;	// This will be used for CAMORIENT_None
	FLOAT ActionPct = GetActionPctFromScenePct( InScenePct );

	//
	// FACE PATH
	//
	if( CamOrientation.CamOrientation==CAMORIENT_FacePath )
	{
		FLOAT fIndex = ((FMatineeTools::NUM_SAMPLE_POINTS-1) * ActionPct);
		INT Index = (INT)fIndex;

		FPathIdx fpi[3];

		fpi[0] = FPathIdx( this, Index, InMA );
		fpi[1] = FPathIdx( this, Index+1, InMA );
		fpi[2] = FPathIdx( this, Index+2, InMA );

		FVector v1 = fpi[0].MA->SampleLocations( fpi[0].Idx ),
				v2 = fpi[1].MA->SampleLocations( fpi[1].Idx ),
				v3 = fpi[2].MA->SampleLocations( fpi[2].Idx );

		// Interpolate Orientation linearly.
		FVector P1 = (v2 - v1),
				P2 = (v3 - v2);

		DesiredRotation = (
			(P1 * (1.f - (fIndex - Index)))
			+ (P2 * (fIndex - Index))
			).Rotation();
	}
	//
	// LOOK AT ACTOR
	//
	else if( (CamOrientation.CamOrientation==CAMORIENT_LookAtActor || CamOrientation.CamOrientation==CAMORIENT_Dolly) && CamOrientation.LookAt )
	{
		// Face a specific actor while moving
		DesiredRotation = (CamOrientation.LookAt->Location - InViewerLocation).Rotation();
	}
	//
	// INTERPOLATE
	//
	else if( CamOrientation.CamOrientation==CAMORIENT_Interpolate )
	{
		UMatAction* PrevAction = GMatineeTools.GetPrevAction( this, InMA );
		if( InMA && InMA->IntPoint && PrevAction && PrevAction->IntPoint )
		{
			FRotatorF Delta = InMA->IntPoint->Rotation - PrevAction->IntPoint->Rotation;
			DesiredRotation = FRotatorF( PrevAction->IntPoint->Rotation ) + (Delta * ActionPct);
		}
	}

	// "DesiredRotation" is where we should be pointing according to the current orientation.  However, if
	// that rotation is still easing in, we need to blend it with whatever the viewer rotation was
	// when this orientation was set.

	FRotatorF FinalRotation = DesiredRotation;

	FLOAT Interp = RotInterpolator.GetValue();

	FRotatorF Delta = DesiredRotation - FRotatorF( CamOrientation.StartingRotation );

	if( CamOrientation.bReversePitch )	if( Delta.Pitch < 0 )	Delta.Pitch = Delta.Pitch + 65535;	else	Delta.Pitch = Delta.Pitch - 65535;
	if( CamOrientation.bReverseYaw )	if( Delta.Yaw < 0 )		Delta.Yaw = Delta.Yaw + 65535;		else	Delta.Yaw = Delta.Yaw - 65535;
	if( CamOrientation.bReverseRoll )	if( Delta.Roll < 0 )	Delta.Roll = Delta.Roll + 65535;	else	Delta.Roll = Delta.Roll - 65535;
	
	FinalRotation = FRotatorF(CamOrientation.StartingRotation ) + (Delta * Interp);

	FinalRotation.Pitch += CameraShake.X;
	FinalRotation.Yaw += CameraShake.Y;
	FinalRotation.Roll += CameraShake.Z;

	return FinalRotation.Rotator();

	unguard;
}

// Adds an orientation onto the stack.
void ASceneManager::ChangeOrientation( FOrientation InOrientation )
{
	guard(ASceneManager::ChangeOrientation);

	CamOrientation = InOrientation;
	CamOrientation.StartingRotation = Viewer->Rotation;
	RotInterpolator.Start( CamOrientation.EaseInTime );

	if( CamOrientation.CamOrientation == CAMORIENT_Dolly )
		DollyOffset = CamOrientation.DollyWith->Location - Viewer->Location;
	else
		DollyOffset = FVector(0,0,0);

	unguard;
}

// Prepares the path for execution.  This involves things like getting sampled locations
// for every action.
void ASceneManager::PreparePath()
{
	SampleLocations.Empty();
	for( INT x = 0 ; x < Actions.Num() ; ++x )
	{
		if( Actions(x) )
		{
			Actions(x)->SampleLocations.Empty();

			GMatineeTools.GetSamples( this, GMatineeTools.GetPrevAction( this, Actions(x) ), &SampleLocations );
			GMatineeTools.GetSamples( this, GMatineeTools.GetPrevAction( this, Actions(x) ), &Actions(x)->SampleLocations );

			if( Actions(x)->bConstantPathVelocity
					&& Actions(x)->PathVelocity )
				Actions(x)->Duration = Actions(x)->PathLength / Actions(x)->PathVelocity;
		}
	}

	// The editor needs this stuff set up for various reasons.
	if( GIsEditor )
		SetSceneStartTime();
}

void ASceneManager::DeletePathSamples()
{
	SampleLocations.Empty();
}

//
// UMatSubAction implementation
//

// ================================
// MATSUBACTION - Base class
// ================================

// Gives the sub action a chance to update itself (it's effect, timers, whatever)
UBOOL UMatSubAction::Update( FLOAT InScenePct, ASceneManager* InSM )
{
	if( Status == SASTATUS_Ending )
	{
		Status = SASTATUS_Expired;
		return 0;
	}

	if( InScenePct > PctStarting && InScenePct <= PctEnding )
		Status = SASTATUS_Running;
	else
	{
		// If we are over our time limit, set status to "ending" and let the subaction
		// run for one more tick.  This will insure that it's effect gets applied at 100%.
		if( InScenePct > PctEnding )
			Status = SASTATUS_Ending;
	}

	return 1;
}

FString UMatSubAction::GetStatString()
{
	return *FString::Printf(TEXT("TYPE : %s, Status : %s, Delay : %1.1f, Duration : %1.1f"),
		*Desc, *GetStatusDesc(), Delay, Duration );
}

FString UMatSubAction::GetStatusDesc()
{
	if( Status == SASTATUS_Waiting )		return TEXT("SASTATUS_Waiting");
	else if( Status == SASTATUS_Running )	return TEXT("SASTATUS_Running");
	else if( Status == SASTATUS_Ending )	return TEXT("SASTATUS_Ending");
	else if( Status == SASTATUS_Expired )	return TEXT("SASTATUS_Expired");

	return TEXT("Unknown");
}

UBOOL UMatSubAction::IsRunning()
{
	return (Status == SASTATUS_Running || Status == SASTATUS_Ending );
}

// ================================
// SUBTITLES
// ================================

UBOOL USubActionSubTitle::Update( FLOAT InScenePct, ASceneManager* InSM )
{
	if( !UMatSubAction::Update( InScenePct, InSM ) )
		return 0;

	if( IsRunning() )
	{
		APlayerController* PC = Cast<APlayerController>(InSM->Viewer);
		if ( PC && PC->myHUD && PC->myHUD->SubTitles )
		{
			PC->myHUD->SubTitles->ProcessEvent( ESST_Mode(SubTitleMode) );
		}

		return 0;
	}

	return 1;
}

FString USubActionSubTitle::GetStatString()
{
	FString Result = UMatSubAction::GetStatString();
	Result += *FString::Printf( TEXT(", SubTitleMode : %i\n"), SubTitleMode );
	return Result;
}

// ================================
// TRIGGER
// ================================

UBOOL USubActionTrigger::Update( FLOAT InScenePct, ASceneManager* InSM )
{
	if( !UMatSubAction::Update( InScenePct, InSM ) )
		return 0;

	if( IsRunning() )
	{
		InSM->eventTriggerEvent( EventName, InSM->Viewer, InSM->OldPawn );
		return 0;
	}

	return 1;
}

FString USubActionTrigger::GetStatString()
{
	FString Result = UMatSubAction::GetStatString();
	Result += *FString::Printf( TEXT(", Event : %s\n"), *EventName );
	return Result;
}

// ================================
// FOV
// ================================

UBOOL USubActionFOV::Update( FLOAT InScenePct, ASceneManager* InSM )
{
	if( !UMatSubAction::Update( InScenePct, InSM ) )
		return 0;

	if( IsRunning() )
	{
		// If no starting value was specified, use the current one
		if( !FOV.Min )
		{
			APlayerController* PC = Cast<APlayerController>(InSM->Viewer);
			if( PC )
				FOV.Min = PC->FovAngle;
		}

		FLOAT Pct = (InScenePct - PctStarting) / PctDuration;
		Pct = Min<FLOAT>( Max<FLOAT>( Pct, 0.0001f ), 1.f );
		if( Status == SASTATUS_Ending )		Pct = 1.f;
		APlayerController* PC = Cast<APlayerController>(InSM->Viewer);
		if( PC )
			PC->FovAngle = FOV.Min + (Pct * (FOV.Max - FOV.Min));
	}

	return 1;
}

FString USubActionFOV::GetStatString()
{
	FString Result = UMatSubAction::GetStatString();
	Result += *FString::Printf( TEXT(", Start : %1.1f, End : %1.1f\n"), FOV.Min, FOV.Max );
	return Result;
}

// ================================
// Camera Shake
// ================================

UBOOL USubActionCameraShake::Update( FLOAT InScenePct, ASceneManager* InSM )
{
	if( !UMatSubAction::Update( InScenePct, InSM ) )
		return 0;

	if( IsRunning() )
	{
		// Update the camera shake variable
		InSM->CameraShake += Shake.GetRand();
	}

	return 1;
}

FString USubActionCameraShake::GetStatString()
{
	return TEXT("");
}

// ================================
// ORIENTATION
// ================================

UBOOL USubActionOrientation::Update( FLOAT InScenePct, ASceneManager* InSM )
{
	if( !UMatSubAction::Update( InScenePct, InSM ) )
		return 0;

	if( IsRunning() )
	{
		InSM->ChangeOrientation( CamOrientation );
		Status = SASTATUS_Expired;
		return 0;
	}

	return 1;
}

UBOOL USubActionOrientation::IsRunning()
{
	// Orientation subactions are handled in a special way in the editor
	if( GIsEditor )	return 0;

	return UMatSubAction::IsRunning();
}

FString USubActionOrientation::GetStatString()
{
	FString Result = UMatSubAction::GetStatString();
	Result += *FString::Printf( TEXT(", Orientation : %s,%s EaseInTime : %1.1f\n"),
		*GMatineeTools.GetOrientationDesc(CamOrientation.CamOrientation),
		( CamOrientation.CamOrientation==CAMORIENT_LookAtActor ? *FString::Printf( TEXT(" Actor : %s," ), (CamOrientation.LookAt ? CamOrientation.LookAt->GetName() : TEXT("NULL") ) ) : TEXT("") ),
		CamOrientation.EaseInTime );
	return Result;
}

// ================================
// GAMESPEED
// ================================

UBOOL USubActionGameSpeed::Update( FLOAT InScenePct, ASceneManager* InSM )
{
	if( !UMatSubAction::Update( InScenePct, InSM ) )
		return 0;

	if( IsRunning() )
	{
		// If no starting value was specified, use the current one
		if( !GameSpeed.Min )
			GameSpeed.Min = InSM->Level->TimeDilation;

		FLOAT Pct = (InScenePct - PctStarting) / PctDuration;
		Pct = Min<FLOAT>( Max<FLOAT>( Pct, 0.0001f ), 1.f );
		if( Status == SASTATUS_Ending )		Pct = 1.f;
		InSM->Level->TimeDilation = GameSpeed.Min + (Pct * (GameSpeed.Max - GameSpeed.Min));
	}

	return 1;
}

FString USubActionGameSpeed::GetStatString()
{
	FString Result = UMatSubAction::GetStatString();
	Result += *FString::Printf( TEXT(", Start : %1.1f, End : %1.1f\n"), GameSpeed.Min, GameSpeed.Max );
	return Result;
}

// ================================
// SCENESPEED
// ================================

UBOOL USubActionSceneSpeed::Update( FLOAT InScenePct, ASceneManager* InSM )
{
	if( !UMatSubAction::Update( InScenePct, InSM ) )
		return 0;

	if( IsRunning() )
	{
		// If no starting value was specified, use the current one
		if( !SceneSpeed.Min )
			SceneSpeed.Min = InSM->SceneSpeed;

		FLOAT Pct = (InScenePct - PctStarting) / PctDuration;
		Pct = Min<FLOAT>( Max<FLOAT>( Pct, 0.0001f ), 1.f );
		if( Status == SASTATUS_Ending )		Pct = 1.f;
		InSM->SceneSpeed = SceneSpeed.Min + (Pct * (SceneSpeed.Max - SceneSpeed.Min));
	}

	return 1;
}

FString USubActionSceneSpeed::GetStatString()
{
	FString Result = UMatSubAction::GetStatString();
	Result += *FString::Printf( TEXT(", Start : %1.1f, End : %1.1f\n"), SceneSpeed.Min, SceneSpeed.Max );
	return Result;
}

// ================================
// FADE
// ================================

UBOOL USubActionFade::Update( FLOAT InScenePct, ASceneManager* InSM )
{
	if( !UMatSubAction::Update( InScenePct, InSM ) )
		return 0;

	if( IsRunning() )
	{
		APlayerController* PC = Cast<APlayerController>(InSM->Viewer);
		if( PC )
		{
			PC->FlashFog = FadeColor;

			FLOAT Pct = (InScenePct - PctStarting) / PctDuration;
			Pct = Min<FLOAT>( Max<FLOAT>( Pct, 0.0001f ), 1.f );
			if( Status == SASTATUS_Ending )		Pct = 1.f;
			PC->FlashScale.X = bFadeOut ? 1.f-Pct : Pct;
		}
		return 0;
	}

	return 1;
}

FString USubActionFade::GetStatString()
{
	FString Result = UMatSubAction::GetStatString();
	Result += *FString::Printf( TEXT("Fade [%s], Color : %d,%d,%d\n"), bFadeOut?TEXT("Out"):TEXT("In"), FadeColor.R, FadeColor.G, FadeColor.B );
	return Result;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

