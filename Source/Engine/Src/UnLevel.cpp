/*=============================================================================
	UnLevel.cpp: Level-related functions
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "UnLinker.h"
//#include "UnCacheManager.h"
extern FCoronaRender GCoronaRender; 
/*-----------------------------------------------------------------------------
	ULevelBase implementation.
-----------------------------------------------------------------------------*/

ULevelBase::ULevelBase( UEngine* InEngine, const FURL& InURL )
:	URL( InURL )
,	Engine( InEngine )
,	Actors( this )
,	DemoRecDriver( NULL )
{}
void ULevelBase::Serialize( FArchive& Ar )
{
	guard(ULevelBase::Serialize);
	Super::Serialize(Ar);
	if( Ar.IsTrans() )
	{
		Ar << Actors;
	}
	else
	{
		//oldver Old-format actor list.
		INT DbNum=Actors.Num(), DbMax=DbNum;
		Actors.CountBytes( Ar );
		Ar << DbNum << DbMax;
		if( Ar.IsLoading() )
		{
			Actors.Empty( DbNum );
			Actors.Add( DbNum );
		}
		for( INT i=0; i<Actors.Num(); i++ )
			Ar << Actors(i);
	}
	
	// Level variables.
	Ar << URL;
	if( !Ar.IsLoading() && !Ar.IsSaving() )
	{
		Ar << NetDriver;
		Ar << DemoRecDriver;
	}
	unguard;
}
void ULevelBase::Destroy()
{
	guard(ULevelBase::Destroy);
	if( NetDriver )
	{
		delete NetDriver;
		NetDriver = NULL;
	}
	if( DemoRecDriver)
	{
		delete DemoRecDriver;
		DemoRecDriver = NULL;
	}
	Super::Destroy();
	unguard;
}

struct NotifyProgress_Params
{
    FString Str1;
    FString Str2;
};
   
void ULevelBase::NotifyProgress(  const TCHAR* CmdStr, const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds )
{
	guard(ULevelBase::NotifyProgress);

    UGameEngine* GameEngine = Cast<UGameEngine>( Engine );
    if( GameEngine && GameEngine->Client && GameEngine->Client->Viewports.Num() )
    {
		// Add code to update progress here
    }
    
    Engine->SetProgress( TEXT(""), Str1, Str2, Seconds );

	unguard;
}
IMPLEMENT_CLASS(ULevelBase);

/*-----------------------------------------------------------------------------
	Level creation & emptying.
-----------------------------------------------------------------------------*/

//
//	Create a new level and allocate all objects needed for it.
//	Call with Editor=1 to allocate editor structures for it, also.
//
ULevel::ULevel( UEngine* InEngine, UBOOL InRootOutside )
:	ULevelBase( InEngine )
{
	guard(ULevel::ULevel);

	// Allocate subobjects.
	SetFlags( RF_Transactional );
	Model = new( GetOuter() )UModel( NULL, InRootOutside );
	Model->SetFlags( RF_Transactional );

	// Spawn the level info.
	SpawnActor( ALevelInfo::StaticClass() );
	check(GetLevelInfo());

	// Spawn the default brush.
	ABrush* Temp = SpawnBrush();
	check(Temp==Actors(1));
	Temp->Brush = new( GetOuter(), TEXT("Brush") )UModel( Temp, 1 );
	Temp->SetFlags( RF_NotForClient | RF_NotForServer | RF_Transactional );
	Temp->Brush->SetFlags( RF_NotForClient | RF_NotForServer | RF_Transactional );

	// spawn default physics volume
	GetLevelInfo()->GetDefaultPhysicsVolume();

	GScriptEntryTag = GScriptCycles = FinishedPrecaching = 0;

#ifdef WITH_KARMA
    this->KWorld = NULL;
    this->KBridge = NULL;
	this->KAssetFactory = NULL;

    this->KLevelModel = NULL;
#endif
	unguard;
}

void ULevel::ShrinkLevel()
{
	guard(ULevel::Shrink);

	Model->ShrinkModel();

	unguard;
}

void ULevel::DetailChange( EDetailMode NewDetailMode )
{
	guard(ULevel::DetailChange);
    GCoronaRender.Reset(); // flag corona rendering to reset, it will have dandling ptrs now
	GetLevelInfo()->DetailMode = GIsPixomatic ? DM_Low : NewDetailMode;
	if( GetLevelInfo()->Game )
		GetLevelInfo()->Game->eventDetailChange();
	for(INT ActorIndex = 0;ActorIndex < Actors.Num();ActorIndex++)
		ResetStaticFilterState( Actors(ActorIndex) );
	unguard;
}

void ULevel::ResetStaticFilterState( AActor* Actor )
{
	guard(ULevel::ResetStaticFilterState);
	if( !GIsEditor && BuiltRenderData && Actor && Actor->bStatic )
	{
		Actor->StaticFilterState = FS_Maybe;
		Actor->GetActorRenderData();

		for(INT LeafIndex = 0;LeafIndex < Actor->Leaves.Num();LeafIndex++)
			LeafRenderInfo(Actor->Leaves(LeafIndex)).RenderActors.AddUniqueItem(Actor);
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Level locking and unlocking.
-----------------------------------------------------------------------------*/

//
// Modify this level.
//
void ULevel::Modify( UBOOL DoTransArrays )
{
	guard(ULevel::Modify);
	UObject::Modify();
	Model->Modify();
	unguard;
}

void ULevel::PostLoad()
{
	guard(ULevel::PostLoad);
	Super::PostLoad();
#if ENGINE_VERSION>230
	for( TObjectIterator<AActor> It; It; ++It )
	{
		if( It->GetOuter()==GetOuter() )
		{
			APlayerController* pc = Cast<APlayerController>(*It);
			if( pc && pc->ShowFlags&SHOW_EdViewport )
				continue;

			It->XLevel = this;
		}
	}
#endif

#ifdef WITH_LIPSINC
	// HACK!  replace with something like the above karma stuff at some point...
	static int LIPSincLevelCount = 0;
	if(LIPSincLevelCount > 0)
	{
		GLIPSincInitLevel(this);
	}
	LIPSincLevelCount++;
#endif

	Model->BuildRenderData();

	unguard;
}

//
//	ULevel::BuildRenderData
//

void ULevel::BuildRenderData()
{
	guard(ULevel::BuildRenderData);

	UpdateTerrainArrays();

	for(INT ZoneIndex = 0;ZoneIndex < Model->NumZones;ZoneIndex++)
	{
		AZoneInfo*	ZoneInfo = GetZoneActor(ZoneIndex);
		
		ZoneInfo->AmbientVector = FGetHSV(ZoneInfo->AmbientHue,ZoneInfo->AmbientSaturation,ZoneInfo->AmbientBrightness);
	}

	if(!GIsEditor)
	{
		// Build a list of static visible actors touching each leaf.

		for(INT LeafIndex = 0;LeafIndex < Model->Leaves.Num();LeafIndex++)
			new(LeafRenderInfo) FLeafRenderInfo;

		for(INT ActorIndex = 0;ActorIndex < Actors.Num();ActorIndex++)
		{
			AActor*	Actor = Actors(ActorIndex);

			if(Actor && Actor->bStatic)
			{
				Actor->GetActorRenderData();

				for(INT LeafIndex = 0;LeafIndex < Actor->Leaves.Num();LeafIndex++)
					LeafRenderInfo(Actor->Leaves(LeafIndex)).RenderActors.AddItem(Actor);
			}
		}

		// Prepare the list of zone render infos.

		for(INT ZoneIndex = 0;ZoneIndex < Model->NumZones;ZoneIndex++)
			new(ZoneRenderInfo) FZoneRenderInfo;

		// Figure out how much memory batching is going to take.

		UseStaticMeshBatching = 1;

		if( !GIsPixomatic && Engine->DetectedVideoMemory && Engine->UseStaticMeshBatching && !GIsUCC && GIsClient )
		{
			INT VertexBytes	= 0,
				IndexBytes	= 0;

			for(INT ActorIndex = 0;ActorIndex < Actors.Num();ActorIndex++)
			{
				AActor*	Actor = Actors(ActorIndex);

				if(Actor && Actor->bStatic && !Actor->bHidden && Actor->DrawType == DT_StaticMesh && Actor->StaticMesh && !Actor->Projectors.Num() && !Actor->UV2Texture )
				{
					for(INT SectionIndex = 0;SectionIndex < Actor->StaticMesh->Sections.Num();SectionIndex++)
					{
						FStaticMeshSection&	Section				= Actor->StaticMesh->Sections(SectionIndex);
						UMaterial*			Material			= Actor->StaticMesh->GetSkin(Actor,SectionIndex);
						BYTE				RequiredUVStreams	= Material->RequiredUVStreams(),
											UVStreamMask		= 0xFF;
						INT					NumUVs				= 0,
											Stride;
						
						for(INT UVIndex = 0;UVIndex < 8;UVIndex++,UVStreamMask <<= 1)
						{
							if(!(RequiredUVStreams & UVStreamMask))
							{
								NumUVs = UVIndex;
								break;
							}
						}

						Stride		 = sizeof(FStaticMeshBatchNormalVertex) + sizeof(FLOAT) * 2 * NumUVs;
						VertexBytes += (Section.MaxVertexIndex - Section.MinVertexIndex + 1) * Stride;
						IndexBytes  += Section.NumPrimitives * 3 * sizeof(_WORD);
					}
				}
			}

			// Disable static mesh batching for this level as it would take up too much video memory.

			if( ((VertexBytes + IndexBytes) / 1024 / 1024) > (0.5 * (Engine->DetectedVideoMemory - 8)) )
			{
				debugf(TEXT("Disabled static mesh batching for %s as it would've consumed %i out of %i MByte of video memory"), GetName(), ((VertexBytes + IndexBytes) / 1024 / 1024), Engine->DetectedVideoMemory );
				UseStaticMeshBatching = 0;
			}
		}

		// Build static mesh batches.

		if( Engine->UseStaticMeshBatching && UseStaticMeshBatching )
		{
			for(INT ActorIndex = 0;ActorIndex < Actors.Num();ActorIndex++)
			{
				AActor*	Actor = Actors(ActorIndex);

				if(Actor && Actor->bStatic && !Actor->bHidden && Actor->DrawType == DT_StaticMesh && Actor->StaticMesh && !Actor->Projectors.Num() && !Actor->UV2Texture )
				{
					for(INT SectionIndex = 0;SectionIndex < Actor->StaticMesh->Sections.Num();SectionIndex++)
					{
						UMaterial*		Material = Actor->StaticMesh->GetSkin(Actor,SectionIndex);
						FBatchReference	Ref = { INDEX_NONE, INDEX_NONE };

						// Find a batch for this section.

						for(INT BatchIndex = 0;BatchIndex < StaticMeshBatches.Num();BatchIndex++)
						{
							FStaticMeshBatch&	Batch = StaticMeshBatches(BatchIndex);

							if(Batch.Material == Material && (!Actor->bDisableSorting || !Batch.Sorted) && Batch.Vertices.Size < 512*1024)
							{
								Ref.BatchIndex = BatchIndex;
								break;
							}
						}

						// Create a new batch if necessary.

						if(Ref.BatchIndex == INDEX_NONE)
						{
							Ref.BatchIndex = StaticMeshBatches.Num();
							new(StaticMeshBatches) FStaticMeshBatch(Material,Actor->bDisableSorting);
						}

						// Add the section to the batch.

						Ref.ElementIndex = StaticMeshBatches(Ref.BatchIndex).AddElement(Actor,SectionIndex);
						Actor->StaticSectionBatches.AddItem(Ref);
					}
				}
			}

			INT	VertexBytes = 0,
				IndexBytes = 0;

			for(INT BatchIndex = 0;BatchIndex < StaticMeshBatches.Num();BatchIndex++)
			{
				FStaticMeshBatch*	Batch = &StaticMeshBatches(BatchIndex);

				Batch->Vertices.Batch = Batch;
				Batch->Indices.Batch = Batch;

				VertexBytes += Batch->Vertices.Size;
				IndexBytes += Batch->Indices.Size;
			}

			debugf(TEXT("Static mesh batches: %u vertex bytes, %u index bytes"),VertexBytes,IndexBytes);
		}
	}

	BuiltRenderData = 1;

	unguard;
}

// Setting bFastClear to 1 will cause the hash to simply be deleted.  It's
// quicker than trying to remove all the actors first.
void ULevel::SetActorCollision( UBOOL bCollision, UBOOL bFastClear )
{
	guard(ULevel::SetActorCollision);

	if ( GIsEditor )
	{
		// clean up DefaultPhysicsVolumes
		for ( INT i=0; i<Actors.Num(); i++ )
		{
			ADefaultPhysicsVolume *V = Cast<ADefaultPhysicsVolume>(Actors(i));
			if ( V && (V != GetLevelInfo()->GetDefaultPhysicsVolume()) )
				DestroyActor(V);
		}
	}

	// Init collision if first time through.
	if( bCollision && !Hash )
	{
		// Init hash.
		guard(StartCollision);
		Hash = GNewCollisionHash();
		for( INT i=0; i<Actors.Num(); i++ )
		{
			//if( GIsEditor )
			//	GWarn->StatusUpdatef( 0, i, TEXT("Adding Actor %d to collision hash"), i );
			if( Actors(i) && Actors(i)->bCollideActors )
				Hash->AddActor( Actors(i) );
		}

		unguard;
	}
	else if( Hash && !bCollision )
	{
		// Destroy hash.
		guard(EndCollision);
		if( !bFastClear )
		{
			for( INT i=0; i<Actors.Num(); i++ )
			{
				//if( GIsEditor )
				//	GWarn->StatusUpdatef( 0, i, TEXT("Removing Actor %d from collision hash"), i );
				if( Actors(i) && Actors(i)->bCollideActors )
					Hash->RemoveActor( Actors(i) );
			}
		}
    
		delete Hash;
		Hash = NULL;
		unguard;
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	Level object implementation.
-----------------------------------------------------------------------------*/

void ULevel::Serialize( FArchive& Ar )
{
	guard(ULevel::Serialize);
	Super::Serialize( Ar );

	FLOAT ApproxTime = (FLOAT)TimeSeconds;
	Ar << Model;
	if ( Ar.Ver() < 98 )
	{
		TArray<FReachSpec> OldSpecs;
		Ar << OldSpecs; 
	}
	Ar << ApproxTime;
	Ar << FirstDeleted;
	for( INT i=0; i<NUM_LEVEL_TEXT_BLOCKS; i++ )
		Ar << TextBlocks[i];
	if( Ar.Ver()>62 )//oldver
	{
		Ar << TravelInfo;
	}
	else if( Ar.Ver()>=61 )
	{
		TArray<FString> Names, Items;
		Ar << Names << Items;
		TravelInfo = TMap<FString,FString>();
		for( INT i=0; i<Names.Num(); i++ )
			TravelInfo.Set( *Names(i), *Items(i) );
	}
	if( Model && !Ar.IsTrans() )
		Ar.Preload( Model );

	unguard;
}

void ULevel::Destroy()
{
	guard(ULevel::Destroy);

	// Free allocated stuff.
	if( Hash )
	{
		delete Hash;
		Hash = NULL; /* Required because actors may try to unhash themselves. */
	}

	for(INT ProjectorIndex = 0;ProjectorIndex < DynamicProjectors.Num();ProjectorIndex++)
		DynamicProjectors(ProjectorIndex)->RemoveReference();

#ifdef WITH_KARMA
    KTermLevelKarma(this);
#endif

#ifdef WITH_LIPSINC
	GLIPSincShutdownLevel();
#endif

    GCoronaRender.Reset();
	Super::Destroy();
	unguard;
}
IMPLEMENT_CLASS(ULevel);

/*-----------------------------------------------------------------------------
	Reconcile actors and Viewports after loading or creating a new level.

	These functions provide the basic mechanism by which UnrealEd associates
	Viewports and actors together, even when new maps are loaded which contain
	an entirely different set of actors which must be mapped onto the existing 
	Viewports.
-----------------------------------------------------------------------------*/

//
// Remember actors.
//
void ULevel::RememberActors()
{
	guard(ULevel::RememberActors);
	if( Engine->Client )
	{
		for( INT i=0; i<Engine->Client->Viewports.Num(); i++ )
		{
			UViewport* Viewport			= Engine->Client->Viewports(i);
			// Exclude cameras not owned by this level (eg prefab or anim browser)
			if( Viewport->Actor->XLevel == this )
			{
				Viewport->SavedOrthoZoom	= Viewport->Actor->OrthoZoom;
				Viewport->SavedFovAngle		= Viewport->Actor->FovAngle;
				Viewport->SavedShowFlags	= Viewport->Actor->ShowFlags;
				Viewport->SavedRendMap		= Viewport->Actor->RendMap;
				Viewport->SavedMisc1		= Viewport->Actor->Misc1;
				Viewport->SavedMisc2		= Viewport->Actor->Misc2;
				Viewport->Actor				= NULL;
			}
		}
	}
	unguard;
}

//
// Reconcile actors.  This is called after loading a level.
// It attempts to match each existing Viewport to an actor in the newly-loaded
// level.  If no decent match can be found, creates a new actor for the Viewport.
//
void ULevel::ReconcileActors()
{
	guard(ULevel::ReconcileActors);
	check(GIsEditor);

    GCoronaRender.Reset(); 

	// Dissociate all actor Viewports and remember their view properties.
	for( INT i=0; i<Actors.Num(); i++ )
		if( Actors(i) && Actors(i)->GetAPlayerController() )
			if( ((APlayerController*)Actors(i))->Player )
				((APlayerController*)Actors(i))->Player = NULL;

	// Match Viewports and Viewport-actors with identical names.
	guard(MatchIdentical);
	for( INT i=0; i<Engine->Client->Viewports.Num(); i++ )
	{
		UViewport* Viewport = Engine->Client->Viewports(i);
		// Exclude cameras not owned by this level (eg prefab or anim browser)
		if(Viewport->Actor==NULL)
		{
			for( INT j=0; j<Actors.Num(); j++ )
			{
				AActor* Actor = Actors(j);
				if( Actor && Actor->IsA(ACamera::StaticClass()) 
					&& appStricmp(*Actor->Tag,Viewport->GetName())==0 )
				{
					debugf( NAME_Log, TEXT("Matched Viewport %s"), Viewport->GetName() );
					Viewport->Actor         = (APlayerController*)(Actor);
					Viewport->Actor->Player = Viewport;
					break;
				}
			}
		}
	}
	unguard;

	// Match up all remaining Viewports to actors.
	guard(MatchEditorOther);
	for( INT i=0; i<Engine->Client->Viewports.Num(); i++ )
	{
		// Hook Viewport up to an existing actor or create a new one.
		UViewport* Viewport = Engine->Client->Viewports(i);
		if( !Viewport->Actor )
			SpawnViewActor( Viewport );
	}
	unguard;

	// Handle remaining unassociated view actors.
	guard(KillViews);
	for( INT i=0; i<Actors.Num(); i++ )
	{
		ACamera* View = Cast<ACamera>(Actors(i));
		if( View )
		{
			UViewport* Viewport = Cast<UViewport>(View->Player);
			if( Viewport )
			{
				View->ClearFlags( RF_Transactional );
				Viewport->Actor->OrthoZoom		= Viewport->SavedOrthoZoom;	
				Viewport->Actor->FovAngle		= Viewport->SavedFovAngle;
				Viewport->Actor->ShowFlags		= Viewport->SavedShowFlags;
				Viewport->Actor->RendMap		= Viewport->SavedRendMap;
				Viewport->Actor->Misc1			= Viewport->SavedMisc1;
				Viewport->Actor->Misc2			= Viewport->SavedMisc2;
			}
			else DestroyActor( View );
		}
	}
	unguard;

	unguard;
}

/*-----------------------------------------------------------------------------
	ULevel command-line.
-----------------------------------------------------------------------------*/
UBOOL bShadowEnable; // sjs - GDC hack
UBOOL UseDeferredSections; // sjs - test

UBOOL ULevel::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(ULevel::Exec);
	if( NetDriver && NetDriver->Exec( Cmd, Ar ) )
	{
		return 1;
	}
	else if( DemoRecDriver && DemoRecDriver->Exec( Cmd, Ar ) )
	{
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("DEMOREC") ) )
	{

		// Attempt to make the dir if it doesn't exist.
#if 0
		Ar.Log(TEXT("Sorry, demo recording is currently disabled"));
		return 1;
#endif

		GFileManager->MakeDirectory( TEXT("..\\Demos") );

		FURL URL;
		FString DemoName;
		if( !ParseToken( Cmd, DemoName, 0 ) )
		{
			if ( !GConfig->GetString( TEXT("DemoRecording"), TEXT("DemoMask"), DemoName, TEXT("user.ini")) )
				DemoName=TEXT("%m-%t");
		}

		DemoName = DemoName.Replace(TEXT("%m"),*this->URL.Map);

		INT Year, Month, DayOfWeek, Day, Hour, Min,Sec,MSec;
		appSystemTime(Year, Month, DayOfWeek, Day, Hour, Min,Sec,MSec);

		DemoName = DemoName.Replace(TEXT("%td"), *FString::Printf(TEXT("%i-%i-%i-%i"),Year,Month,Day,((Hour*3600)+(Min*60)+Sec)*1000+MSec));
		DemoName = DemoName.Replace(TEXT("%d"),  *FString::Printf(TEXT("%i-%i-%i"),Month,Day,Year));
		DemoName = DemoName.Replace(TEXT("%t"),  *FString::Printf(TEXT("%i"),((Hour*3600)+(Min*60)+Sec)*1000+MSec));

		UGameEngine* GameEngine = CastChecked<UGameEngine>( Engine );

		if (GameEngine && GameEngine->Client && GameEngine->Client->Viewports(0) && 
			GameEngine->Client->Viewports(0)->Actor && GameEngine->Client->Viewports(0)->Actor->PlayerReplicationInfo)
			DemoName=DemoName.Replace( TEXT("%p"), *GameEngine->Client->Viewports(0)->Actor->PlayerReplicationInfo->PlayerName);

		while ( DemoName.InStr( TEXT("\\") ) !=-1 )
			DemoName = DemoName.Replace( TEXT("\\"), TEXT("_"));

		while ( DemoName.InStr( TEXT("/") ) != -1 )
			DemoName = DemoName.Replace( TEXT("/"),TEXT("_") );

		while ( DemoName.InStr( TEXT(".") ) != -1 )
			DemoName = DemoName.Replace( TEXT("."),TEXT("_") );

		while ( DemoName.InStr( TEXT(" ") ) != -1 )
			DemoName = DemoName.Replace( TEXT(" "),TEXT("_") );

		URL.Map= FString::Printf(TEXT("..\\Demos\\%s.demo4"),*DemoName);

		UClass* DemoDriverClass = StaticLoadClass( UDemoRecDriver::StaticClass(), NULL, TEXT("ini:Engine.Engine.DemoRecordingDevice"), NULL, LOAD_NoFail, NULL );
		DemoRecDriver           = ConstructObject<UDemoRecDriver>( DemoDriverClass );
		FString Error;
		if( !DemoRecDriver->InitListen( this, URL, Error ) )
		{
			Ar.Logf( TEXT("Demo recording failed: %s"), *Error );//!!localize!!
			delete DemoRecDriver;
			DemoRecDriver = NULL;
		}
		else
			Ar.Logf( TEXT("Demo recording started to %s"), *URL.Map );
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("DEMOPLAY") ) )
	{
		FString Temp;
		if( ParseToken( Cmd, Temp, 0 ) )
		{

			FString DemoOptions=TEXT("");
			INT p;
			p = Temp.InStr(TEXT("?"));
			if (p>=0)
			{
				DemoOptions = Temp.Right(Temp.Len()-p-1);
				Temp = Temp.Left(p);
			}

			INT i = Temp.Caps().InStr(TEXT(".demo4"));
			if( i != -1)
				Temp = Temp.Left(i) + Temp.Mid(i+6);

			Temp = FString::Printf(TEXT("..\\Demos\\%s"),*Temp);

			UGameEngine* GameEngine = CastChecked<UGameEngine>( Engine );
			FURL URL(NULL, *GameEngine->LastURL.String(1,0), TRAVEL_Absolute);

			if (DemoOptions!=TEXT(""))
				URL.AddOption(*DemoOptions);

			URL.Map = FString::Printf(TEXT("%s.demo4"),*Temp);

			debugf( TEXT("Attempting to play demo %s"), *URL.Map );
			if( GameEngine->GPendingLevel )
				GameEngine->CancelPending();
			GameEngine->GPendingLevel = new UDemoPlayPendingLevel( GameEngine, URL );
			if( !GameEngine->GPendingLevel->DemoRecDriver )
			{
				Ar.Logf( TEXT("Demo playback failed: %s"), *GameEngine->GPendingLevel->Error );//!!localize!!
				delete GameEngine->GPendingLevel;
				GameEngine->GPendingLevel = NULL;
			}
//			else
//				GameEngine->Client->Viewports(0)->GUIController->eventCloseAll(0,true);
		}
		else Ar.Log( TEXT("You must specify a filename") );//!!localize!!
		return 1;
	}

	else if( ParseCommand( &Cmd, TEXT("DEMODUMP") ) )
	{
		FString	DEMFilename, AVIFilename;
		FLOAT	Quality = 0.5f;
		FLOAT   FixedFPS= 30.0;
		INT		Width	= 320,
				Height	= 240;

		GFileManager->MakeDirectory( TEXT("..\\UserMovies") );

		if( !Parse( Cmd, TEXT("DEMO="),DEMFilename) )
		{
			Ar.Logf( TEXT("Usage is: DEMODUMP DEMO=<x> FILENAME=<x> QUALITY=<0-1> WIDTH=<x> HEIGHT=<x> FPS=<x>") );
			return 0;
		}
		
		INT i = DEMFilename.Caps().InStr(TEXT(".demo4"));
		if( i != -1)
			DEMFilename = DEMFilename.Left(i);

		DEMFilename = FString::Printf(TEXT("..\\Demos\\%s"),*DEMFilename);

		if( !Parse( Cmd, TEXT("FILENAME="), AVIFilename ) )
		{
			Ar.Logf( TEXT("Usage is: DEMODUMP DEMO=<x> FILENAME=<x> QUALITY=<0-1> WIDTH=<x> HEIGHT=<x> FPS=<x>") );
			return 0;
		}

		i = AVIFilename.Caps().InStr(TEXT(".AVI"));
		if( i == -1)
			AVIFilename += TEXT(".AVI");

		AVIFilename = FString::Printf(TEXT("..\\USerMovies\\%s"),*AVIFilename);

		Parse( Cmd, TEXT("QUALITY="), Quality	);	
		Parse( Cmd, TEXT("WIDTH=")  , Width		);	
		Parse( Cmd, TEXT("HEIGHT=") , Height	);
		Parse( Cmd, TEXT("FPS=")	, FixedFPS	);

		FURL URL(NULL, *DEMFilename, TRAVEL_Absolute);
		URL.Map += TEXT(".demo4");
		UGameEngine* GameEngine = CastChecked<UGameEngine>( Engine );

		if( GameEngine->GPendingLevel )
			GameEngine->CancelPending();

		GameEngine->GPendingLevel = new UDemoPlayPendingLevel( GameEngine, URL );
		if( !GameEngine->GPendingLevel->DemoRecDriver )
		{
			Ar.Logf( TEXT("Demo playback failed: %s"), *GameEngine->GPendingLevel->Error );//!!localize!!
			delete GameEngine->GPendingLevel;
			GameEngine->GPendingLevel = NULL;
		}
		else
		{
			GameEngine->Client->Viewports(0)->GUIController->eventCloseAll(0,true);
			GameEngine->PendingRecordMovie = FString::Printf(TEXT("recordmovie start filename=%s quality=%f width=%i height=%i fps=%f"),*AVIFilename, Quality, Width, Height, FixedFPS);
		}

		return 1;
	}
	
	else if( ParseCommand( &Cmd, TEXT("SHOWEXTENTLINECHECK") ) )
	{
		bShowExtentLineChecks = !bShowExtentLineChecks;
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("SHOWLINECHECK") ) )
	{
		bShowLineChecks = !bShowLineChecks;
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("SHOWPOINTCHECK") ) )
	{
		bShowPointChecks = !bShowPointChecks;
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("SHOWSLOWOVERLAP") ) )
	{
		FLOAT ThreshTime = 0.0f;
		Parse( Cmd, TEXT("TIME="), ThreshTime);
		SlowOverlapThreshTime = ThreshTime;

		if(SlowOverlapThreshTime != 0.0f)
			bShowSlowOverlap = true;
		else
			bShowSlowOverlap = false;

		return 1;
	}
#ifdef WITH_KARMA
    else if( (GetLevelInfo()->NetMode == NM_Standalone) && KExecCommand( Cmd , &Ar ) )
        return 1;
#endif
	else return 0;
	unguard;
}

/*-----------------------------------------------------------------------------
	ULevel terrain related functions.
-----------------------------------------------------------------------------*/

void ULevel::UpdateTerrainArrays()
{
	guard(ULevel::UpdateTerrainArrays);
	if( Model )
	{
		//!!
		for( INT i=0; i<64; i++ )
			if( Model->Zones[i].ZoneActor )
				Model->Zones[i].ZoneActor->Terrains.Empty();

		for( INT i=0; i<Actors.Num(); i++ )
			if( Actors(i) && !Actors(i)->bDeleteMe && Actors(i)->IsA( ATerrainInfo::StaticClass() ) )
			{
				Actors(i)->SetZone(1,0);
				Actors(i)->Region.Zone->Terrains.AddUniqueItem( Cast<ATerrainInfo>(Actors(i)) );
			}
		
		ALevelInfo* L = GetLevelInfo();
		if( L )
			L->Terrains.Empty();
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Utility functions.
-----------------------------------------------------------------------------*/

// Moves an actor to the floor.  Optionally will align with the trace normal.
UBOOL ULevel::ToFloor( AActor* InActor, UBOOL InAlign, AActor* InIgnoreActor )
{
	guard(ULevel::ToFloor);

	check( InActor );

	FVector BestLoc = FVector(0,0,-HALF_WORLD_MAX);
	FRotator SaveRot = FRotator(0,0,0);

	FVector Direction = FVector(0,0,-1);
	FVector ActorExtent( InActor->CollisionRadius, InActor->CollisionRadius, InActor->CollisionHeight );

	// If there is no collision information for this actor, use it's bounding box.
	if( ActorExtent.IsZero() )
	{
		if( InActor->DrawType==DT_StaticMesh && InActor->StaticMesh )
		{
			UStaticMesh* StaticMesh = InActor->StaticMesh;

			FLOAT Height = StaticMesh->GetRenderBoundingBox(NULL).GetExtent().Z;
			FLOAT Radius = StaticMesh->GetRenderBoundingSphere(NULL).W;

			ActorExtent = FVector(Radius,Radius,Height);
		}
	}

	FCheckResult Hit(1.f);
	if( !SingleLineCheck( Hit, InActor, InActor->Location + Direction*WORLD_MAX, InActor->Location, TRACE_World, ActorExtent ) )
	{
		if( Hit.Actor == GetLevelInfo() )
		{
			FCheckResult PointCheckHit(1.f);
			if( !Hit.Actor->GetLevel()->Model->PointCheck( PointCheckHit, NULL, Hit.Location, ActorExtent, 0 ) )
				return 0;
		}

		FarMoveActor( InActor, Hit.Location );
		if( InAlign )
		{
			InActor->Rotation = Hit.Normal.Rotation();
			InActor->Rotation.Pitch -= 16384;
		}

		return 1;
	}

	return 0;

	unguard;
}

/*-----------------------------------------------------------------------------
	ULevel networking related functions.
-----------------------------------------------------------------------------*/

//
// Start listening for connections.
//
UBOOL ULevel::Listen( FString& Error )
{
	guard(ULevel::Listen);
	if( NetDriver )
	{
		Error = LocalizeError(TEXT("NetAlready"),TEXT("Engine"));
		return 0;
	}
	if( !GetLinker() )
	{
		Error = LocalizeError(TEXT("NetListen"),TEXT("Engine"));
		return 0;
	}

	// Create net driver.
	UClass* NetDriverClass = StaticLoadClass( UNetDriver::StaticClass(), NULL, TEXT("ini:Engine.Engine.NetworkDevice"), NULL, LOAD_NoFail, NULL );
	NetDriver = (UNetDriver*)StaticConstructObject( NetDriverClass );
	if( !NetDriver->InitListen( this, URL, Error ) )
	{
		debugf( TEXT("Failed to listen: %s"), *Error );
		delete NetDriver;
		NetDriver=NULL;
		return 0;
	}
	static UBOOL LanPlay = ParseParam(appCmdLine(),TEXT("lanplay"));
	if ( !LanPlay && (NetDriver->MaxInternetClientRate < NetDriver->MaxClientRate) && (NetDriver->MaxInternetClientRate > 2500) )
		NetDriver->MaxClientRate = NetDriver->MaxInternetClientRate;

	if ( GetLevelInfo()->Game && (GetLevelInfo()->Game->MaxPlayers > 16) )
		NetDriver->MaxClientRate = ::Min(NetDriver->MaxClientRate, 10000);

	// Load everything required for network server support.
	UGameEngine* GameEngine = CastChecked<UGameEngine>( Engine );
	GameEngine->BuildServerMasterMap( NetDriver, this );

	// Spawn network server support.
	for( INT i=0; i<GameEngine->ServerActors.Num(); i++ )
	{
		TCHAR Str[240];
		const TCHAR* Ptr = *GameEngine->ServerActors(i);
		if( ParseToken( Ptr, Str, ARRAY_COUNT(Str), 1 ) )
		{
			debugf(NAME_DevNet, TEXT("Spawning: %s"), Str );
			UClass* HelperClass = StaticLoadClass( AActor::StaticClass(), NULL, Str, NULL, LOAD_NoFail, NULL );

#if DEMOVERSION
			UPackage* Package = CastChecked<UPackage>(HelperClass->GetOuter());
			if ( Package->GetFName() != NAME_UWeb && Package->GetFName() != NAME_IpDrv )
				appThrowf(TEXT("%s is not allowed in the demo"), HelperClass->GetFullName());
#endif

			AActor* Actor = SpawnActor( HelperClass );
			while( Actor && ParseToken(Ptr,Str,ARRAY_COUNT(Str),1) )
			{
				TCHAR* Value = appStrchr(Str,'=');
				if( Value )
				{
					*Value++ = 0;
					for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Actor->GetClass()); It; ++It )
						if
						(	appStricmp(It->GetName(),Str)==0
						&&	(It->PropertyFlags & CPF_Config) )
							It->ImportText( Value, (BYTE*)Actor + It->Offset, 0 );
				}
			}
		}
	}

	// Set LevelInfo properties.
	GetLevelInfo()->NetMode = Engine->Client ? NM_ListenServer : NM_DedicatedServer;
	GetLevelInfo()->NextSwitchCountdown = NetDriver->ServerTravelPause;

	return 1;
	unguard;
}

//
// Return whether this level is a server.
//
UBOOL ULevel::IsServer()
{
	guardSlow(ULevel::IsServer);
	return (!NetDriver || !NetDriver->ServerConnection) && (!DemoRecDriver || !DemoRecDriver->ServerConnection);
	unguardSlow;
}

/*-----------------------------------------------------------------------------
	ULevel network notifys.
-----------------------------------------------------------------------------*/

//
// The network driver is about to accept a new connection attempt by a
// connectee, and we can accept it or refuse it.
//
EAcceptConnection ULevel::NotifyAcceptingConnection()
{
	guard(ULevel::NotifyAcceptingConnection);
	check(NetDriver);
	if( NetDriver->ServerConnection )
	{
		// We are a client and we don't welcome incoming connections.
		debugf( NAME_DevNet, TEXT("NotifyAcceptingConnection: Client refused") );
		return ACCEPTC_Reject;
	}
	else if( GetLevelInfo()->NextURL!=TEXT("") )
	{
		// Server is switching levels.
		debugf( NAME_DevNet, TEXT("NotifyAcceptingConnection: Server %s refused"), GetName() );
		return ACCEPTC_Ignore;
	}
	else
	{
		// Server is up and running.
		debugf( NAME_DevNet, TEXT("NotifyAcceptingConnection: Server %s accept"), GetName() );
		return ACCEPTC_Accept;
	}
	unguard;
}

//
// This server has accepted a connection.
//
void ULevel::NotifyAcceptedConnection( UNetConnection* Connection )
{
	guard(ULevel::NotifyAcceptedConnection);
	check(NetDriver!=NULL);
	check(NetDriver->ServerConnection==NULL);
	if( GUfrontLog )
		printf("[NET-TRAFFIC] Server: Accepted connection from %s\n", TCHAR_TO_ANSI(*Connection->LowLevelGetRemoteAddress()));
	debugf( NAME_NetComeGo, TEXT("Open %s %s %s"), GetName(), appTimestamp(), *Connection->LowLevelGetRemoteAddress() );
	unguard;
}

//
// The network interface is notifying this level of a new channel-open
// attempt by a connectee, and we can accept or refuse it.
//
UBOOL ULevel::NotifyAcceptingChannel( UChannel* Channel )
{
	guard(ULevel::NotifyAcceptingChannel);
	
	check(Channel);
	check(Channel->Connection);
	check(Channel->Connection->Driver);
	UNetDriver* Driver = Channel->Connection->Driver;

	if( Driver->ServerConnection )
	{
		// We are a client and the server has just opened up a new channel.
		//debugf( "NotifyAcceptingChannel %i/%i client %s", Channel->ChIndex, Channel->ChType, GetName() );
		if( Channel->ChType==CHTYPE_Actor )
		{
			// Actor channel.
			//debugf( "Client accepting actor channel" );
			return 1;
		}
		else if( Channel->ChType==CHTYPE_Voice )
		{
			// Voice channel.
			return 1;
		}
		else
		{
			// Unwanted channel type.
			debugf( NAME_DevNet, TEXT("Client refusing unwanted channel of type %i"), Channel->ChType );
			return 0;
		}
	}
	else
	{
		// We are the server.
		if( Channel->ChIndex==0 && Channel->ChType==CHTYPE_Control )
		{
			// The client has opened initial channel.
			debugf( NAME_DevNet, TEXT("NotifyAcceptingChannel Control %i server %s: Accepted"), Channel->ChIndex, GetFullName() );
			return 1;
		}
		else if( Channel->ChType==CHTYPE_File )
		{
			// The client is going to request a file.
			debugf( NAME_DevNet, TEXT("NotifyAcceptingChannel File %i server %s: Accepted"), Channel->ChIndex, GetFullName() );
			return 1;
		}
		else
		{
			// Client can't open any other kinds of channels.
			debugf( NAME_DevNet, TEXT("NotifyAcceptingChannel %i %i server %s: Refused"), Channel->ChType, Channel->ChIndex, GetFullName() );
			return 0;
		}
	}
	unguard;
}

//
// Welcome a new player joining this server.
//
#if UNICODE
void ULevel::WelcomePlayer( UNetConnection* Connection, TCHAR* Optional )
#else
void ULevel::WelcomePlayer( UNetConnection* Connection, char* Optional )
#endif
{
	guard(ULevel::WelcomePlayer);

	if( GUfrontLog )
		printf("[NET-TRAFFIC] Server: WelcomePlayer — sending map info to client\n");
	Connection->PackageMap->Copy( Connection->Driver->MasterMap );
	Connection->SendPackageMap( this );
	if( Optional[0] )
		Connection->Logf( TEXT("WELCOME LEVEL=%s GAME=%s LONE=%i %s"), GetOuter()->GetName(), GetLevelInfo()->Game ? GetLevelInfo()->Game->GetClass()->GetPathName():TEXT(""), GetLevelInfo()->bLonePlayer, Optional );
	else
		Connection->Logf( TEXT("WELCOME LEVEL=%s GAME=%s LONE=%i"), GetOuter()->GetName(), GetLevelInfo()->Game ? GetLevelInfo()->Game->GetClass()->GetPathName():TEXT(""), GetLevelInfo()->bLonePlayer );
	Connection->FlushNet();

	unguard;
}

// gam ---

static FString GetDefaultGameType( const TCHAR* MapName )
{
/*	TArray<FRegistryObjectInfo> List;
	UObject::GetRegistryObjects( List, UClass::StaticClass(), AGameInfo::StaticClass(), 0 );

	for( INT i = 0; i < List.Num(); i++ )
    {
	    UClass* GameClass = Cast<UClass>( UObject::StaticLoadObject( UClass::StaticClass(), NULL, *List(i).Object, NULL, 0, NULL ) );

        if( !GameClass )
            continue;

    	AGameInfo* GameInfo = CastChecked<AGameInfo>( GameClass->GetDefaultActor() );

        INT PrefixLength = appStrlen( *GameInfo->MapPrefix );

        if( ( appStrnicmp( MapName, *GameInfo->MapPrefix, PrefixLength ) == 0 ) && ( MapName[PrefixLength] == '-' ) )
        {
            return( GameClass->GetPathName() );
            break;
        }
    }
*/

	// rjp - Reglist no longer contains gameinfo
	const TArray<FGameRecord>* Records = (TArray<FGameRecord>*)(Cast<UCacheManager>(UCacheManager::StaticClass()->GetDefaultObject())->GetRecords(TEXT("Game")));	
	check(Records);

	INT PrefixLength = 0;
	for (INT i = 0; i < Records->Num(); i++)
	{
		const TCHAR* T = *(*Records)(i).MapPrefix;
		PrefixLength = appStrlen(T);
		if ( !appStrnicmp( MapName, T, PrefixLength ) && MapName[PrefixLength] == '-' )
			return (*Records)(i).ClassName;
	}


    return( TEXT("") );
}

void ULevel::CheckDefaultGameType( const TCHAR* FileName )
{
    guard(ULevel::CheckDefaultGameType);
    
    const TCHAR* p;
    
    for( p = FileName; *p != '\0'; p++ )
        ;
    
    while( (*p != '\\') && (*p != '/') && (p > FileName) )
        p--;

    if( (*p == '\\') || (*p == '/') )
        p++;
    
    FileName = p;

    for( TObjectIterator<ALevelInfo> It; It; ++It )
    {
        ALevelInfo *LevelInfo = *It;

        if( (LevelInfo->DefaultGameType.Len() <= 0 ) || !FindObject<UClass>(ANY_PACKAGE, *LevelInfo->DefaultGameType) )
        {
            FString DefaultGameType = GetDefaultGameType( FileName );

            if( DefaultGameType.Len() )
            {
                LevelInfo->DefaultGameType = GetDefaultGameType( FileName );
                debugf( NAME_Log, TEXT("Set default game type to %s"), *LevelInfo->DefaultGameType );
                return;
            }
        }
    }

    unguard;
}

// --- gam

// amb ---
void ULevel::LoadSounds()
{
    for( TObjectIterator<USound> ItC; ItC; ++ItC )
        ItC->Load();
}
// --- amb

//
// Received text on the control channel.
//
void ULevel::NotifyReceivedText( UNetConnection* Connection, const TCHAR* Text )
{
	guard(ULevel::NotifyReceivedText);
	if( ParseCommand(&Text,TEXT("USERFLAG")) )
	{
		Connection->UserFlags = appAtoi(Text);
	}
	else if( NetDriver->ServerConnection )
	{
		// We are the client.
//		debugf( NAME_DevNet, TEXT("Level client received: %s"), Text );
		if( ParseCommand(&Text,TEXT("FAILURE")) || ParseCommand(&Text,TEXT("FAILCODE")) )
		{
			// Return to entry.
			Engine->SetProgress( *FString::Printf(TEXT("menu:%s"), *Cast<UGameEngine>(Engine)->DisconnectMenuClass),LocalizeError(TEXT("ConnectionFailed"),TEXT("Engine")), TEXT("") );	
			check(Engine->Client->Viewports.Num());
			Engine->SetClientTravel( Engine->Client->Viewports(0), TEXT("?failed"), 0, TRAVEL_Absolute );
		}
		else if ( ParseCommand(&Text,TEXT("BRAWL")) )
		{
			// Find the name of the bad package
			
			FString Filename = TEXT("Unknown");

			if ( appStricmp(Text,TEXT("SPOOF")) )
			{
				TArray<UObject*> ObjLoaders = UObject::GetLoaderList(); 
				for( INT i=0; i<ObjLoaders.Num(); i++ )
				{
					ULinker* Linker = CastChecked<ULinker>( ObjLoaders(i) );
					if ( !appStricmp(Linker->Summary.Guid.String(),Text) )
					{
						Filename = Linker->Filename;
					}
				}
			}
			else
				Filename=TEXT("Spoof Attempt");

			Engine->SetProgress( *FString::Printf(TEXT("menu:%s"),*(Cast<UGameEngine>(Engine)->DisconnectMenuClass)),LocalizeProgress(TEXT("CorruptConnect"),TEXT("Engine")), *Filename );
			Engine->SetClientTravel( Engine->Client->Viewports(0), TEXT("?failed"), 0, TRAVEL_Absolute );
		}
	}
	else
	{
		FLOAT ServerTime = 0.f;
		clock(ServerTime);
		// We are the server.
//		debugf( NAME_DevNet, TEXT("Level server received: %s"), Text );
		if( ParseCommand(&Text,TEXT("HELLO")) )
		{
			// Versions.
			INT RemoteMinVer=219, RemoteVer=219;
			Parse( Text, TEXT("MINVER="), RemoteMinVer );
			Parse( Text, TEXT("VER="),    RemoteVer    );

			if ( GetLevelInfo()!=NULL && GetLevelInfo()->Game!=NULL && appStricmp( GetLevelInfo()->Game->GetName(), TEXT("ONSOnslaughtGame")) == 0  )
			{
				UGameEngine* GE = Cast<UGameEngine>(Engine);
				if ( GE!=NULL )
				{
					for( INT i=0; i<GE->ServerPackages.Num(); i++ )
					{
						if ( GE->ServerPackages(i).Caps() == TEXT("ONSLAUGHTBP") && RemoteVer<3320)
						{
							Connection->Logf( TEXT("UPGRADE MINVER=3320 VER=%i"),  ENGINE_VERSION );
							Connection->FlushNet();
							Connection->State = USOCK_Closed;
							return;
						}
					}
				}
			}

			if( RemoteVer<ENGINE_MIN_NET_VERSION || RemoteMinVer>ENGINE_VERSION)
			{
				Connection->Logf( TEXT("UPGRADE MINVER=%i VER=%i"), ENGINE_MIN_NET_VERSION, ENGINE_VERSION );
				Connection->FlushNet();
				Connection->State = USOCK_Closed;
				return;
			}

			if( RemoteVer<ENGINE_MIN_NET_VERSION || RemoteMinVer>ENGINE_VERSION || RemoteVer == 3362 || RemoteVer == 3363 )
			{
				Connection->Logf( TEXT("UPGRADE MINVER=3368 VER=3368") );
				Connection->FlushNet();
				Connection->State = USOCK_Closed;
				return;
			}

			Connection->NegotiatedVer = Min(RemoteVer,ENGINE_VERSION);

			INT Sec = INT(CastChecked<UGameEngine>( Engine )->bCheatProtection);

			// Get byte limit.
			INT Stats = GetLevelInfo()->Game->bEnableStatLogging;
			Connection->Challenge = appCycles();

			Connection->Gozer = (appFrand() * 127)+1;	// Create the key
			Connection->Logf( TEXT("CHALLENGE VER=%i CHALLENGE=%i STATS=%i SEC=%i GZ=%i"), Connection->NegotiatedVer, Connection->Challenge, Stats, Sec, Connection->Gozer );
			Connection->FlushNet();
		}
		else if( ParseCommand(&Text,TEXT("AUTH")) )
		{
			FString GlobalMD5;

            Parse( Text, TEXT("HASH="), Connection->CDKeyHash );
			Parse( Text, TEXT("USERNAME="), Connection->EncStatsUsername );
			Parse( Text, TEXT("PASSWORD="), Connection->EncStatsPassword );
			Parse( Text, TEXT("GM="),GlobalMD5);

			// Decrypt the Global MD5

			BYTE DecBuffer[66];
			for (INT i=0;i<66;i++)
			{
				FString Temp = GlobalMD5.Mid(i*2,2);
				DecBuffer[i] = Temp.HexValue();
			}

			BYTE MD5Decrypt = DecBuffer[64];
			BYTE Pos = DecBuffer[65];

			for (INT j=0;j<16;j++)
			{
				BYTE Hi = DecBuffer[Pos++];
				BYTE Lo = DecBuffer[Pos++];
				Connection->GMD5[j] = ((Hi & 0xF0) + (Lo & 0x0F)) ^ MD5Decrypt++;
			}
		}
		else if( ParseCommand(&Text,TEXT("NETSPEED")) )
		{
			INT Rate = appAtoi(Text);
			Connection->CurrentNetSpeed = Clamp( Rate, 1800, NetDriver->MaxClientRate );
			if( GUfrontLog )
				printf("[NET-TRAFFIC] Server: Client NETSPEED=%d\n", Connection->CurrentNetSpeed);
			debugf( TEXT("Client netspeed is %i"), Connection->CurrentNetSpeed );
		}
		else if( ParseCommand(&Text,TEXT("HAVE")) )
		{
			// Client specifying his generation.
			FGuid Guid(0,0,0,0);
			Parse( Text, TEXT("GUID=" ), Guid );
			for( TArray<FPackageInfo>::TIterator It(Connection->PackageMap->List); It; ++It )
				if( It->Guid==Guid )
					Parse( Text, TEXT("GEN=" ), It->RemoteGeneration );
		}
		else if( ParseCommand( &Text, TEXT("SKIP") ) )
		{
			FGuid Guid(0,0,0,0);
			Parse( Text, TEXT("GUID=" ), Guid );
			if( Connection->PackageMap )
			{
				for( INT i=0;i<Connection->PackageMap->List.Num();i++ )
					if( Connection->PackageMap->List(i).Guid == Guid )
					{
						debugf( NAME_DevNet, TEXT("User skipped download of '%s'"), *Connection->PackageMap->List(i).URL );
						Connection->PackageMap->List.Remove( i );
						break;
					}
			}
		}
		else if( ParseCommand(&Text,TEXT("LOGIN")) )
		{
			// Admit or deny the player here.
			INT Response=0;
			if
			(	!Parse(Text,TEXT("RESPONSE="),Response)
			||	Engine->ChallengeResponse(Connection->Challenge)!=Response )	
			{
				// Challenge failed, abort right now

				debugf( NAME_DevNet, TEXT("Client %s failed CHALLENGE."), *Connection->LowLevelGetRemoteAddress() );
				Connection->Logf( TEXT("FAILCODE CHALLENGE") );
				Connection->FlushNet();
				Connection->State = USOCK_Closed;
				return;
			}
			Connection->PassedChallenge = 1;
			TCHAR Str[1024]=TEXT("");
			FString Error, FailCode;
			Parse( Text, TEXT("URL="), Str, ARRAY_COUNT(Str) );
			Connection->RequestURL = Str;
			if( GUfrontLog )
				printf("[NET-TRAFFIC] Server: LOGIN request: %s\n", TCHAR_TO_ANSI(*Connection->RequestURL));
			debugf( NAME_DevNet, TEXT("Login request: %s"), *Connection->RequestURL );
			const TCHAR* Tmp;
			for( Tmp=Str; *Tmp && *Tmp!='?'; Tmp++ );

			if(GetLevelInfo()->Game->GameStats)
			{
				// Reject players that don't have a valid stats username and password.
				// NOTE: The encrypted password has twice as many characters as the original.

				if(Connection->EncStatsUsername.Len() < 4 || Connection->EncStatsPassword.Len() < 6*2)
                    FailCode = TEXT("NEEDSTATS");
			}

			if(FailCode==TEXT("") && Error==TEXT(""))
				GetLevelInfo()->Game->eventPreLogin( Tmp, Connection->LowLevelGetRemoteAddress(), Connection->SecureCDKeyHash(), Error, FailCode );

			if ( FailCode!=TEXT("") || Error!=TEXT("") )
			{
				debugf( NAME_DevNet, TEXT("PreLogin failure: (%s) (%s)"), *FailCode, *Error );

				if ( FailCode!=TEXT("") )
					Connection->Logf( TEXT("FAILCODE %s %s"), *FailCode, *Error );

//				if ( Error!=TEXT("") )
//					Connection->Logf( TEXT("FAILURE %s"), *Error );

				Connection->FlushNet();
				Connection->State = USOCK_Closed;
				return;
			}
			WelcomePlayer( Connection );
		}
		else if( ParseCommand(&Text,TEXT("JOIN")) && !Connection->Actor )
		{

			// Make sure Client is verified

			FURL MyURL( NULL, *Connection->RequestURL, TRAVEL_Absolute );
/*
			UBOOL Spec = appStrcmp(MyURL.GetOption(TEXT("SpectatorOnly="),TEXT("")), TEXT("") );
			if ( CastChecked<UGameEngine>( Engine )->bCheatProtection && !Spec  )
			{
				if (Connection->ExpectedPackageCount==0 || Connection->ExpectedPackageCount!=Connection->CurrentPackageCount)
				{
					Connection->Logf( TEXT("BRAWL SPOOF"));
					Connection->FlushNet();
					Connection->State = USOCK_Closed;
					return;
				}
			}
*/
			Connection->bValidated=true;

			// Finish computing the package map.
			Connection->PackageMap->Compute();

			// Spawn the player-actor for this network player.
			FString Error;
			debugf( NAME_DevNet, TEXT("Join request: %s"), *Connection->RequestURL );
			if( GUfrontLog )
				printf("[NET-TRAFFIC] Server: JOIN request: %s\n", TCHAR_TO_ANSI(*Connection->RequestURL));
			if( !SpawnPlayActor( Connection, ROLE_AutonomousProxy, FURL(NULL,*Connection->RequestURL,TRAVEL_Absolute), Error ) )
			{
				// Failed to connect.
				debugf( NAME_DevNet, TEXT("Join failure: %s"), *Error );
//				Connection->Logf( TEXT("FAILURE LOGIN %s"), *Error );
				Connection->Logf( TEXT("FAILCODE LOGIN %s"), *Error );
				Connection->FlushNet();
				Connection->State = USOCK_Closed;
			}
			else
			{
				// Successfully in game.
				if( GUfrontLog )
					printf("[NET-TRAFFIC] Server: JOIN SUCCEEDED — player '%s' is now in game\n", TCHAR_TO_ANSI(*Connection->Actor->PlayerReplicationInfo->PlayerName));
				debugf( NAME_DevNet, TEXT("Join succeeded: %s"), *Connection->Actor->PlayerReplicationInfo->PlayerName );
			}
		}
		else if ( ParseCommand(&Text,TEXT("BADBOY")) )	
		{
			debugf(TEXT("[POSSIBLE CHEAT] %s"),Text);
		}

		else if ( ParseCommand(&Text,TEXT("PETE")) )	
		{
			INT PktCnt, PkgCnt;
			Parse(Text,TEXT("PKT="),PktCnt);
			Parse(Text,TEXT("PKG="),PkgCnt);
			Connection->CurrentPackageCount=0;
			Connection->ExpectedPackageCount=PktCnt;
			Connection->FinalPackageCount=PkgCnt;
		}
		else if ( ParseCommand(&Text,TEXT("REPEAT")) )
		{
			FURL MyURL( NULL, *Connection->RequestURL, TRAVEL_Absolute );
//			UBOOL Spec = appStrcmp(MyURL.GetOption(TEXT("SpectatorOnly="),TEXT("")), TEXT("") );

/*
			if ( CastChecked<UGameEngine>( Engine )->bCheatProtection ) // && !Spec  )
			{
				new(Connection->KeyPackets)FString(Text);
				Connection->CurrentPackageCount++;
				if ( Connection->CurrentPackageCount == Connection->ExpectedPackageCount )
				{
					UGameEngine* GameEngine = CastChecked<UGameEngine>( Engine );			
					FString Result = GameEngine->InitNewNetConnection(Connection->KeyPackets,Connection->FinalPackageCount,Connection->Gozer);

					if ( Result!=TEXT("") )
					{
						FString FullKey = FString::Printf(TEXT("1230asadkgjk358dmvmbjt6838320yjkdhnchjg4958%s"),*Connection->CDKeyHash);
					
							// Perform the MD5 hash of it

						const ANSICHAR* Hash = appToAnsi(*FullKey);
						BYTE HashDigest[16];
						FMD5Context Context;

						appMD5Init( &Context );
						appMD5Update( &Context, (unsigned char*)Hash, FullKey.Len() );
						appMD5Final( HashDigest, &Context );
						
						INT JK;
						FString SecureKey;
						FString IP = Connection->LowLevelGetRemoteAddress();
					
						for (JK=0; JK<16; JK++)
							SecureKey += FString::Printf(TEXT("%02x"), HashDigest[JK]);	

						debugf(TEXT("[INVALID PACKAGE] Name   : %s (%s)"),MyURL.GetOption(TEXT("NAME="),TEXT("")), *IP );
						debugf(TEXT("[INVALID PACKAGE] Ban ID : %s"),*SecureKey);
						debugf(TEXT("[INVALID PACKAGE] GUID   : %s"),*Result.Left(32) );
						debugf(TEXT("[INVALID PACKAGE] MD5    : %s"),*Result.Right(32) );
						debugf(TEXT("[INVALID PACKAGE] Ver.   : %i"),Connection->NegotiatedVer);

						Connection->Logf( TEXT("BRAWL %s"),*Result.Left(32) );
						Connection->FlushNet();
						Connection->State = USOCK_Closed;
					}
				}
			}
*/
		}
		else if ( ParseCommand(&Text,TEXT("OPENVOICE")) )
		{
			UGameEngine* GameEngine                     = CastChecked<UGameEngine>( Engine );
			AVoiceChatReplicationInfo* VoiceChatManager = CastChecked<AVoiceChatReplicationInfo>( AVoiceChatReplicationInfo::StaticClass()->GetDefaultObject() );	
	
			if( VoiceChatManager->bEnableVoiceChat && (appStrcmp(Text,TEXT("")) != 0) )
			{
				static UBOOL			LanPlay			= ParseParam(appCmdLine(),TEXT("LANPLAY"));
				EVoiceCodec				VoiceCodec		= (EVoiceCodec) appAtoi(Text);
				DWORD					AllowedCodecs	= 0;
				const TArray<FString>&	Codecs			= LanPlay ? VoiceChatManager->VoIPLANCodecs : VoiceChatManager->VoIPInternetCodecs;

				// Find codec to use.
				for( INT i=0; i<Codecs.Num(); i++ )
				{
					if( appStricmp(*Codecs(i),TEXT("CODEC_48NB")) == 0 )
						AllowedCodecs |= CODEC_48NB;
					if( appStricmp(*Codecs(i),TEXT("CODEC_96WB")) == 0 )
						AllowedCodecs |= CODEC_96WB;
				}

				// Use 4.8kbs narrow band as safety fallback codec.
				if( !(VoiceCodec & AllowedCodecs) )
					VoiceCodec = CODEC_48NB;

				// Create and set up voice channel.
				if( Connection->Actor && Connection->Actor->PlayerReplicationInfo )
				{
					INT PlayerID = Connection->Actor->PlayerReplicationInfo->PlayerID;
					check(PlayerID);
					debugf(NAME_DevNet,TEXT("OPENVOICE for PlayerID: %i with codec: %i"), PlayerID, VoiceCodec );
					
					// See whether this player already has a voice channel.
					UVoiceChannel* VoiceChannel = Connection->VoiceChannel;

					// Create channel for client if there isn't already one.
					if( !VoiceChannel )
					{
						UVoiceChannel* VoiceChannel = CastChecked<UVoiceChannel>(Connection->CreateChannel( CHTYPE_Voice, 1 ));
						
						// Set server options.
						DWORD ServerOptions = 0;
//						if( VoiceChatManager->VoIPAllowSpatialization )
//							ServerOptions |= VOICE_AllowSpatialization;
						if( GameEngine->VoIPAllowVAD )
							ServerOptions |= VOICE_AllowVAD;
						
						// Find free voice slot.
						INT i;
						for( i=0; i<VOICE_MAX_CHATTERS; i++ )
							if( Connection->Driver->VoiceInfos[i] == NULL )
								break;
						check(i<VOICE_MAX_CHATTERS); //!!VOIP TODO

						Connection->Driver->VoiceInfos[i] = new FVoiceInfo;

						Connection->Driver->VoiceInfos[i]->VoiceSound		= NULL;
						Connection->Driver->VoiceInfos[i]->VoiceCodec		= VoiceCodec;
						Connection->Driver->VoiceInfos[i]->PlayerID			= PlayerID;
						Connection->Driver->VoiceInfos[i]->PacketSize		= UAudioSubsystem::CodecPacketSize[CODEC_TO_INDEX(VoiceCodec)];
						Connection->Driver->VoiceInfos[i]->ServerOptions	= ServerOptions;
						Connection->Driver->VoiceInfos[i]->PacketData		= new BYTE[Connection->Driver->VoiceInfos[i]->PacketSize];
						Connection->Driver->VoiceInfos[i]->VoiceIndex		= i;

						VoiceChannel->SendStatusPacket( Connection->Driver->VoiceInfos[i], VOICEPACKET_Initialize );
						VoiceChannel->VoiceIndex = i;
					}
				}
			}
		}
		unclock(ServerTime);
		// We are the server.
		//debugf( TEXT("Took %f"), ServerTime * GSecondsPerCycle * 1000.f );
	}
	unguard;
}

//
// Called when a file receive is about to begin.
//
void ULevel::NotifyReceivedFile( UNetConnection* Connection, INT PackageIndex, const TCHAR* Error, UBOOL Skipped, INT Attempt )
{
	guard(ULevel::NotifyReceivingFile);
	appErrorf( TEXT("Level received unexpected file") );
	unguard;
}

//
// Called when other side requests a file.
//
UBOOL ULevel::NotifySendingFile( UNetConnection* Connection, FGuid Guid )
{
	guard(ULevel::NotifySendingFile);
	if( NetDriver->ServerConnection )
	{
		// We are the client.
		debugf( NAME_DevNet, TEXT("Server requested file: Refused") );
		return 0;
	}
	else
	{
		// We are the server.
		debugf( NAME_DevNet, TEXT("Client requested file: Allowed") );
		return 1;
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	Clock.
-----------------------------------------------------------------------------*/

void ULevel::UpdateTime(ALevelInfo* Info)
{
	appSystemTime( Info->Year, Info->Month, Info->DayOfWeek, Info->Day, Info->Hour, Info->Minute, Info->Second, Info->Millisecond );
}


/*-----------------------------------------------------------------------------
	Sound Occlusion
-----------------------------------------------------------------------------*/

UBOOL ULevel::IsAudibleAt( FVector SoundLocation, FVector ListenerLocation, AActor* SoundActor, ESoundOcclusion SoundOcclusion )
{
	guard(ULevel::IsAudibleAt);

	if( SoundActor == NULL )
		SoundOcclusion = OCCLUSION_BSP;
	else if( SoundActor->IsA(AMover::StaticClass()) )
		SoundOcclusion = OCCLUSION_None;

	FCheckResult Hit;
	switch ( SoundOcclusion )
	{
	case OCCLUSION_StaticMeshes:
		return SingleLineCheck( Hit, SoundActor, SoundLocation, ListenerLocation, TRACE_World | TRACE_StopAtFirstHit );
		break;
	case OCCLUSION_Default:
	case OCCLUSION_BSP:
		return Model ? Model->FastLineCheck( SoundLocation, ListenerLocation ) : 0;
		break;
	case OCCLUSION_None:
	default:
		return 1;
	}

	return 0;
	unguard;
}

#define ZONE_FACTOR 0.85
FLOAT ULevel::CalculateRadiusMultiplier( INT Zone1, INT Zone2 )
{
	guard(AActor::CalculateRadiusMultiplier);

	INT Distance = ZoneDist[Zone1][Zone2];
	return appPow( ZONE_FACTOR, Distance * Distance );

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

