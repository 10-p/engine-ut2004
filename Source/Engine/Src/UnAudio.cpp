/*=============================================================================
	UnAudio.cpp: Unreal base audio.
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney
	* Wave modification code by Erik de Neve
	* Taken over by Daniel Vogel
=============================================================================*/

#include "EnginePrivate.h" 
#include "UnNet.h"

// Shared thread safe audio capture device.
FAudioCaptureDevice* UAudioSubsystem::CaptureDevice = NULL;

// Lookup tables for codec/ packet size & time and frame size.
INT UAudioSubsystem::CodecPacketSize[32]	= {    18,    24, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
INT UAudioSubsystem::CodecPacketTime[32]	= {    30,    20, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
INT UAudioSubsystem::CodecFrameSize[32]		= {   240,   320, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
INT UAudioSubsystem::CodecFrequency[32]		= {  8000, 16000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

//
// UAudioSubsystem::StaticConstructor
//
void UAudioSubsystem::StaticConstructor()
{
	guard(UAudioSubsystem::StaticConstructor);
	new(GetClass(),TEXT("MusicVolume"			), RF_Public) UFloatProperty(CPP_PROPERTY(MusicVolume			), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("SoundVolume"			), RF_Public) UFloatProperty(CPP_PROPERTY(SoundVolume			), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("VoiceVolume"			), RF_Public) UFloatProperty(CPP_PROPERTY(VoiceVolume			), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("AmbientVolume"			), RF_Public) UFloatProperty(CPP_PROPERTY(AmbientVolume			), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("VolumeScaleRec"		), RF_Public) UFloatProperty(CPP_PROPERTY(VolumeScaleRec		), TEXT("Audio"), CPF_Config );
	new(GetClass(),TEXT("UseVoIP"				), RF_Public) UBoolProperty	(CPP_PROPERTY(UseVoIP				), TEXT("Audio"), CPF_Config );
	unguard;
}

//
//	UAudioSubsystem::PostEditChange
//
void UAudioSubsystem::PostEditChange()
{
	guard(UAudioSubsystem::PostEditChange);
	
	// Validate configurable variables.
	SoundVolume = Clamp(SoundVolume,0.f,1.f);
	MusicVolume = Clamp(MusicVolume,0.f,1.f);
	VoiceVolume = Clamp(VoiceVolume,1.f,10.f);

	// Start music if necessary.
	if( PendingSong != TEXT("") && MusicVolume > 0.f )
		PlayMusic( PendingSong, 0.f );

	unguard;
}

//
//	UAudioSubsystem::Init
//
UBOOL UAudioSubsystem::Init()
{
	guard(UAudioSubsystem::Init);
	
	// Initialize stats.
	AudioStats.Init();

	LastViewTarget	= NULL;
	AllowMusic		= true;

	return true;
	unguard;
}

//
//	UAudioSubsystem::PlaySound
//
void UAudioSubsystem::PlaySound( AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch, INT Flags, FLOAT FadeTime )
{
	guard(UAudioSubsystem::PlaySound);
	// Queue sound to be played.
    Sound = Sound->RenderSoundPlay( &Volume, &Pitch );
	FSoundSource* Source = new FSoundSource(this);
	Source->Init( Actor, Id, Sound, Location, Volume, Radius, Pitch, Flags, FadeTime );
	QueuedSources.AddItem( Source );
	unguard;
}

// Helper function for "Sort".
static inline INT Compare(FSoundSource* A,FSoundSource* B)
{
	return (B->Priority - A->Priority >= 0) ? 1 : -1;
}

//
//	UAudioSubsystem::Update
//
UBOOL UAudioSubsystem::Update( FSceneNode* SceneNode )
{
	guard(UAudioSubsystem::Update);

	UViewport* Viewport = GetViewport();
	FListener* Listener = GetListener();

	// Early outs.
	if( !Viewport || !Listener || !Viewport->Actor || !Viewport->Actor->GetViewTarget() )
		return 0;
	ULevel* Level = Viewport->Actor->GetViewTarget()->GetLevel();
	if( !Level )
		return 0;

	UBOOL Realtime = Viewport->IsRealtime() && !Level->IsPaused();

	// Projection/ Orientation.
#define ReverseStereo 0
	FVector ProjUp		 = SceneNode->CameraToWorld.TransformNormal(FVector(0,1000,0));
	ProjUp.Z = Abs( ProjUp.Z ); // Don't allow flipping "up".
	FVector ProjRight	 = SceneNode->CameraToWorld.TransformNormal(FVector(ReverseStereo ? -1000 : 1000,0,0));
	FVector ProjFront	 = ProjRight ^ ProjUp;

	ProjUp.Normalize();
	ProjRight.Normalize();
	ProjFront.Normalize();

	// Time passes...
	DeltaTime		= Viewport->CurrentTime - LastUpdateTime;
	LastUpdateTime	= Viewport->CurrentTime;
	DeltaTime		= Clamp(DeltaTime,0.0001f,1.0f);

	// Gather information needed to update listener.
	ALevelInfo*		LevelInfo	= Viewport->Actor->GetViewTarget()->Level;
	FPointRegion	Region		= Level->Model->PointRegion( LevelInfo, SceneNode->ViewOrigin );
	APhysicsVolume*	Volume		= Viewport->Actor->GetViewTarget()->PhysicsVolume;
	APawn*			Pawn		= Viewport->Actor->ViewTarget ? Viewport->Actor->ViewTarget->GetAPawn() : NULL;
	UI3DL2Listener*	EAXEffect	= Region.Zone->ZoneEffect;

	// Volume EAX effect overrides zone EAX effect if present.
	if( Pawn )
		Volume = Pawn->HeadVolume;
	if( Volume && Volume->VolumeEffect )
		EAXEffect = Volume->VolumeEffect;

	// Finally update listener.
	Listener->Update( ProjUp, ProjRight, ProjFront, SceneNode->ViewOrigin, Region, DeltaTime, EAXEffect );

	// Stop all sounds if transitioning out of realtime.
	if( !Realtime && LastRealtime )
		for( INT i=0; i<GetNumSources(); i++ )
			if( !(GetSource(i)->Flags & SF_Streaming) )
				GetSource(i)->Stop();
	LastRealtime = Realtime;

	// Check for finished sounds.
	for( INT i=0; i<GetNumSources(); i++ )
		if( GetSource(i)->IsFinished( DeltaTime ) )
			GetSource(i)->Stop();

	// Stop all ambient sounds if viewtarget changes.
	if( LastViewTarget != Viewport->Actor->GetViewTarget() )
	{
		for( INT i=0; i<GetNumSources(); i++ )
			if( (GetSource(i)->Id & 14) == SLOT_Ambient*2 )
				GetSource(i)->Stop();

		LastViewTarget = Viewport->Actor->GetViewTarget();
	}

	// Check for new ambient sounds to be played.
	if( Realtime )
	{		
		for( INT i=0; i<Level->Actors.Num(); i++ )
		{
			AActor* Actor = Level->Actors(i);
			if ( Actor && Actor->AmbientSound && !((Actor->bHighDetail && Actor->Level->DetailMode == DM_Low) || (Actor->bSuperHighDetail && Actor->Level->DetailMode != DM_SuperHigh)) )
			{
				INT Id = Actor->GetIndex()*16 + SLOT_Ambient*2;
				INT j;
				// Could use TMap here instead.
				for( j=0; j<GetNumSources(); j++ )
					if( GetSource(j)->IsPlaying() && GetSource(j)->Id==Id )
						break;
				if( j==GetNumSources() )
				{
					INT Flags = SF_Looping | SF_UpdatePitch;
					if( Actor->IsOwnedBy( Viewport->Actor->GetViewTarget() ) )
						Flags |= SF_No3D;

					FLOAT	Radius = Actor->SoundRadius ? Actor->SoundRadius : Actor->AmbientSound->GetRadius(),
							SqDist = FDistSquared( Listener->Location, Actor->Location );
					if( (SqDist < Square(Radius * GAudioMaxRadiusMultiplier)) || (Flags & SF_No3D) )
					{
						PlaySound( 
							Actor, 
							Id, 
							Actor->AmbientSound, 
							Actor->Location, 
							Actor->GetAmbientVolume(AmbientVolume), 
							Radius, 
							Clamp<FLOAT>(Actor->SoundPitch/64.f,0.5f,2.0f), 
							Flags, 
							0 
						);
					}
				}
			}
		}
	}

	// Keep track of temporary sources that need to be deleted.
	TArray<FSoundSource*> TemporarySources(QueuedSources);

	// Add existing playing sources to queued ones.
	for( INT i=0; i<GetNumSources(); i++ )
	{
		FSoundSource* Source = GetSource(i);
		if( Source && Source->IsPlaying() )
		{
			check(Source->Initialized);
			QueuedSources.AddItem( Source );
		}
	}

	// Update radii and priority.
	for( INT i=0; i<QueuedSources.Num(); i++ )
		QueuedSources(i)->UpdateBase( DeltaTime );

	// Stop non audible ambient sounds.
	for( INT i=0; i<QueuedSources.Num(); i++ )
	{
		FSoundSource* Source = QueuedSources(i);
		if( Source->Actor && ((Source->Id & 14) == SLOT_Ambient*2) )
		{			
			FLOAT SqDist = FDistSquared( Listener->Location, Source->Actor->Location );
			if( (SqDist > Square(Source->ZoneRadius * GAudioMaxRadiusMultiplier) && !(Source->Flags & SF_No3D)) 
			||	(Source->Actor->AmbientSound != Source->Sound)
			||	(GIsEditor && Source->Actor->SoundRadius != Source->Radius)
			)
			{
				// Stop ambient sound if already playing and make sure it doesn't get started if queued.
				Source->Priority = 0;
			}
		}
	}

	// Sort by priority.
	Sort( &QueuedSources(0), QueuedSources.Num() );

	// Stop all sources below threshold.
	FLOAT ThresholdPriority = GetNumSources()+1 > QueuedSources.Num() ? 0.f : QueuedSources(GetNumSources())->Priority;	
	for( INT i=0; i<GetNumSources(); i++ )
	{
		if( GetSource(i)->IsPlaying() && GetSource(i)->Priority <= ThresholdPriority && !(GetSource(i)->Id & 1) )
		{		
			GStats.DWORDStats(AudioStats.STATS_StoppedSources)++;
			GetSource(i)->Stop();
		}
	}

	// Figure out which new sounds need to be started.
	for( INT i=0; i<Min(GetNumSources(),QueuedSources.Num()); i++ )
	{	
		if( (QueuedSources(i)->Priority > ThresholdPriority) && !QueuedSources(i)->IsPlaying() )
		{
			GStats.DWORDStats(AudioStats.STATS_StartedSources)++;
			PlaySound( QueuedSources(i) );
		}
	}

	// Update all active sounds.
	for( INT i=0; i<GetNumSources(); i++ )
	{
		if( GetSource(i)->IsPlaying() )
		{
			GStats.DWORDStats(AudioStats.STATS_ActiveSources)++;
			GetSource(i)->Update( DeltaTime );
		}
	}

	// Clean up temporaries.
	for( INT i=0; i<TemporarySources.Num(); i++ )
		delete TemporarySources(i);

	// Empty queue.
	QueuedSources.Empty();

	return 1;

	unguard;
}

//
//	UAudioSubsystem::FindSource
//
UBOOL UAudioSubsystem::FindSource()
{
	guard(UAudioSubsystem::FindSource);

	FSoundSource*	TransientSource	= GetTransientSource();
	FSoundSource*	BestSource		= NULL;
	FLOAT			BestPriority	= TransientSource->Priority;

	// Find a source with lower priority.
	for( INT i=0; i<GetNumSources(); i++ )
	{
		FSoundSource* Source = GetSource(i);

		if( !Source->Initialized )
		{
			BestSource		= Source;
			break;
		}

		if( (Source->Id & ~1) == (TransientSource->Id & ~1) )
		{
			// Skip if not interruptable.
			if( Source->Id & 1 )
				return 0;			
			BestSource		= Source;
			break;
		}
		else
		if( Source->Priority < BestPriority )
		{
			BestPriority	= Source->Priority;
			BestSource		= Source;
		}
	}

	if( BestSource )
	{
		BestSource->Stop();
		SetTransientSource( BestSource );
		return 1;
	}
	else
		return 0;

	unguard;
}

//
//	UAudioSubsystem::SetViewport
//
void UAudioSubsystem::SetViewport( UViewport* Viewport )
{
	guard(UAudioSubsystem::SetViewport);
	// Stop pending sounds (except streaming).
	for( INT i=0; i<QueuedSources.Num(); i++ )
	{
		if( !(QueuedSources(i)->Flags & SF_Streaming) )
		{
			delete QueuedSources(i);
			QueuedSources.Remove(i--);
		}
	}
	unguard;
}

//
//	UAudioSubsystem::NoteDestroy
//
void UAudioSubsystem::NoteDestroy( USound* Sound )
{
	guard(UAudioSubsystem::NoteDetroy);
	// Unqueue sound.
	for( INT i=0; i<QueuedSources.Num(); i++ )
	{
		if( QueuedSources(i)->Sound == Sound )
		{
			delete QueuedSources(i);
			QueuedSources.Remove(i--);
		}
	}
	unguard;
}

//
//	UAudioSubsystem::NoteDestroy
//
void UAudioSubsystem::NoteDestroy( AActor* Actor )
{
	guard(UAudioSubsystem::NoteDetroy);
	// Unqueue the actor's sound, and dereference owned sounds.
	for( INT i=0; i<QueuedSources.Num(); i++ )
	{
		if(QueuedSources(i)->Actor == Actor)
		{
			if( (QueuedSources(i)->Id & 14) == SLOT_Ambient * 2 )
			{
				delete QueuedSources(i);
				QueuedSources.Remove(i--);
			}
			else 
			{
				QueuedSources(i)->Actor = NULL; // Non interruptable sound.
			}
		}
	}
	unguard;
}

//
//	UAudioSubsystem::IsQueued
//
UBOOL UAudioSubsystem::IsQueued( USound* Sound )
{
	guard(UAudioSubsystem::IsQueued);
	for( INT i=0; i<QueuedSources.Num(); i++ )
		if( QueuedSources(i)->Sound == Sound )
			return 1;
	return 0;
	unguard;
}

//
//	UAudioSubsystem::PlayStream
//
INT UAudioSubsystem::PlayStream( FString Filename, UBOOL UseMusicVolume, FLOAT InVolume, FLOAT FadeInTime, FLOAT SeekTime )
{
	guard(UAudioSubsystem::PlayStream);
	if( Filename != TEXT("") )
	{
		FLOAT	Volume = UseMusicVolume ? MusicVolume : InVolume;
		USound* Stream = new USound( *Filename, SF_Streaming );
		Stream->InitialSeekTime = SeekTime;

		PlaySound( NULL, 2*SLOT_None | 1, Stream, FVector(0,0,0), Clamp( Volume, 0.f, 1.f), 1000, 1.f, SF_Streaming | SF_No3D | (UseMusicVolume ? SF_MusicVolume : 0), FadeInTime );
		return Stream->GetIndex();
	}
	return 0;
	unguard;
}

//
//	UAudioSubsystem::PlayMusic
//
INT UAudioSubsystem::PlayMusic( FString Song, FLOAT FadeInTime )
{
	guard(UAudioSubsystem::PlayMusic);
	// Start music.
	if( Song != TEXT("") && AllowMusic && GetViewport() )
	{
		FLOAT Volume = Min(GetViewport()->Actor->Level->MusicVolumeOverride,1.f);
		if( Volume < 0 )
			Volume = MusicVolume;

		if( Volume == 0.f )
		{
			PendingSong = Song;
			return 0;
		}
		else
		{
			PendingSong = TEXT("");
			for (INT i=0;i<GSys->MusicPath.Num();i++)
			{

				FString Filename = GSys->MusicPath(i) + FString(TEXT("\\")) + Song + FString(TEXT(".ogg"));
				if ( GFileManager->FileSize( *Filename ) > 0 )
				{
					USound* Music = new USound( *Filename, SF_Streaming | SF_Music );
					PlaySound( NULL, 2*SLOT_None | 1, Music, FVector(0,0,0), Clamp(Volume,0.f,1.f), 1000, 1.f, SF_Streaming | SF_No3D | SF_Music | SF_MusicVolume, FadeInTime );
					return Music->GetIndex();
				}
			}
		}
	}
	else
		PendingSong = TEXT("");

	return 0;
	unguard;
}

// rjp --
//
//  UAudioSubsystem::PauseStream
//
UBOOL UAudioSubsystem::PauseStream( INT SongHandle )
{
	guard(UAudioSubsystem::PauseStream);

	const INT num = GetNumSources();
	FSoundSource* Source = NULL;

	for ( INT i = 0; i < num; i++ )
	{
		Source = GetSource(i);
		if ( Source && Source->Flags & SF_Streaming && Source->Sound && Source->Sound->GetIndex() == SongHandle )
		{
			if ( Source->IsPaused() )
				Source->Play();
			else if ( Source->IsPlaying() )
				Source->Pause();

			return 1;
		}
	}

	return 0;

	unguard;
}

//
//  UAudioSubsystem::IsPaused
//
UBOOL UAudioSubsystem::IsPaused( const INT SongHandle )
{
	guard(UAudioSubsystem::IsPaused);

	INT num = GetNumSources();
	FSoundSource* Source = NULL;

	for ( INT i = 0; i < num; i++ )
	{
		Source = GetSource(i);
		if ( Source && Source->Flags & SF_Streaming && Source->Sound && Source->Sound->GetIndex() == SongHandle )
			return Source->IsPaused();
	}

	return 0;

	unguard;
}

UBOOL UAudioSubsystem::SetVolume( INT SongHandle,  FLOAT NewVolume )
{
	guard(UAudioSubsystem::SetVolume);

	INT num = GetNumSources();
	FSoundSource* Source = NULL;

	for ( INT i = 0; i < num; i++ )
	{
		Source = GetSource(i);
		if ( Source && Source->Flags & SF_Streaming && Source->Sound && Source->Sound->GetIndex() == SongHandle )
		{
			Source->Volume = Clamp( NewVolume, 0.f, 1.f );
			return 1;
		}
	}

	return 0;

	unguard;
}
// -- rjp
//
//	UAudioSubsystem::AllowMusicPlayback
//
void UAudioSubsystem::AllowMusicPlayback( UBOOL InAllowMusic )
{
	guard(UAudioSubsystem::AllowMusicPlayback);
	AllowMusic = InAllowMusic;
	unguard;
}

//
//	UAudioSubsystem::StopStream
//
void UAudioSubsystem::StopStream( INT SongHandle, FLOAT FadeOutTime )
{
	guard(UAudioSubsystem::StopMusic);
	for( INT i=0; i<GetNumSources(); i++ )
		if( (GetSource(i)->Flags & SF_Streaming) && GetSource(i)->Sound && (GetSource(i)->Sound->GetIndex() == SongHandle) )
			GetSource(i)->Stop();
	unguard;
}

//
//	UAudioSubsystem::StopAllMusic
//
void UAudioSubsystem::StopAllMusic( FLOAT FadeOutTime )
{
	guard(UAudioSubsystem::StopAllMusic);
	for( INT i=0; i<GetNumSources(); i++ )
		if( GetSource(i)->Flags & SF_Music )
			GetSource(i)->Stop();
	unguard;
}

//
//	UAudioSubsystem::StopAllSounds
//
void UAudioSubsystem::StopAllSounds()
{
	guard(UAudioSubsystem::StopAllSounds);
	for( INT i=0; i<GetNumSources(); i++ )
		GetSource(i)->Stop();
	unguard;
}

//
//	UAudioSubsystem::FAudioStats::FAudioStats
//
UAudioSubsystem::FAudioStats::FAudioStats()
{
	guard(FAudioStats::FAudioStats)
	appMemset( &STATS_FirstEntry, 0xFF, (PTRINT) &STATS_LastEntry - (PTRINT) &STATS_FirstEntry );
	unguard;
}

//
//	void UAudioSubsystem::FAudioStats::Init
//
void UAudioSubsystem::FAudioStats::Init()
{
	guard(FAudioStats::Init);

	// If already initialized retrieve indices from GStats.
	if( GStats.Registered[STATSTYPE_Audio] )
	{
		INT* Dummy = &STATS_UpdateCycles;
		for( INT i=0; i<GStats.Stats[STATSTYPE_Audio].Num(); i++ )
			*(Dummy++) = GStats.Stats[STATSTYPE_Audio](i).Index;
		return;
	}

	// Register stats with GStat.
	STATS_UpdateCycles		= GStats.RegisterStats( STATSTYPE_Audio, STATSDATATYPE_DWORD, TEXT("Update"			), TEXT("Audio"		), STATSUNIT_MSec					);
	STATS_OcclusionCycles	= GStats.RegisterStats( STATSTYPE_Audio, STATSDATATYPE_DWORD, TEXT("Occlusion"		), TEXT("Audio"		), STATSUNIT_MSec					);
	STATS_ActiveSources		= GStats.RegisterStats( STATSTYPE_Audio, STATSDATATYPE_DWORD, TEXT("Sources Active"	), TEXT("Audio"		), STATSUNIT_Default				);
	STATS_StoppedSources	= GStats.RegisterStats( STATSTYPE_Audio, STATSDATATYPE_DWORD, TEXT("Sources Stopped"), TEXT("Audio"		), STATSUNIT_Default				);
	STATS_StartedSources	= GStats.RegisterStats( STATSTYPE_Audio, STATSDATATYPE_DWORD, TEXT("Sources Started"), TEXT("Audio"		), STATSUNIT_Default				);
				
	// Initialized.
	GStats.Registered[STATSTYPE_Audio] = 1;

	unguard;
}

//
//	FSoundSource::FSoundSource
//
FSoundSource::FSoundSource( UAudioSubsystem* InAudioSubsystem )
{
	guard(FSoundSource::FSoundSource);
	AudioSubsystem		= InAudioSubsystem;
	TimeSinceLastUpdate	= -1;
	Playing				= 0;
	Initialized			= 0;
	Actor				= NULL;
	Id					= 0;
	Sound				= NULL;
	Flags				= 0;
	unguard;
}

//
//	FSoundSouce::Init
//
void FSoundSource::Init( AActor* InActor, INT InId, USound* InSound, FVector InLocation, FLOAT InVolume, FLOAT InRadius, FLOAT InPitch, INT InFlags, FLOAT InFadeDuration )
{	
	guard(FSoundSource::Init);

	// Store parameters.
	Actor			= InActor;
	Id				= InId;
	Sound			= InSound;
	Location		= InLocation;
	Volume			= InVolume;
	Radius			= InRadius;
	Pitch			= InPitch;
	Flags			= InFlags;
	FadeDuration	= InFadeDuration;

	Priority		= 0.0f;
	ZoneRadius		= 0.0f;
	UsedRadius		= 0.0f;
	WantedRadius	= 0.0f;
	UsedVolume		= 0.0f;

	Velocity		= FVector(0,0,0);
	Initialized		= 1;

	InactiveTime	= 0.f;
	FadeTime		= 0.f;	
	FadeMode		= FadeDuration > 0 ? FADE_In : FADE_None;

	unguard;
}

//
//	FSoundSource::UpdateRadii
//
void FSoundSource::UpdateRadii()
{
	guard(FSoundSource::UpdateRadii);

	// Retrieve transient objects from sound system.
	UViewport*		Viewport	= AudioSubsystem->GetViewport();
	FListener*		Listener	= AudioSubsystem->GetListener();

	FLOAT LastWantedRadius = WantedRadius;

	// Calculate occlusion for attenuating sounds.
	if( !(Flags & SF_No3D) && Viewport && Viewport->Actor )
	{
		clock(GStats.DWORDStats(AudioSubsystem->AudioStats.STATS_OcclusionCycles));
		guard(SoundOcclusion);
		ULevel*			Level			= Viewport->Actor->GetViewTarget()->GetLevel();
		ALevelInfo*		LevelInfo		= Viewport->Actor->GetViewTarget()->Level;
		ESoundOcclusion SoundOcclusion	= Actor ? (ESoundOcclusion) Actor->SoundOcclusion : OCCLUSION_Default;
		FLOAT			OcclusionRadius	= Radius;

		// Only apply occlusion if there is no line of sight.
		if( !Level->IsAudibleAt( Location, Listener->Location, Actor, SoundOcclusion ) )
		{
			FPointRegion RegionSource	= Level->Model->PointRegion( LevelInfo, Location );
			ZoneRadius					= Radius * Level->CalculateRadiusMultiplier( RegionSource.ZoneNumber, Listener->Region.ZoneNumber );
			OcclusionRadius				= ZoneRadius * AUDIO_OCCLUSION_FACTOR;
		}
		else
			ZoneRadius = Radius;

		WantedRadius = OcclusionRadius;
		unguard;
		unclock(GStats.DWORDStats(AudioSubsystem->AudioStats.STATS_OcclusionCycles));
	}
	else
	// No attenuation wanted, hence no occlusion.
	{
		ZoneRadius		= Radius;
		WantedRadius	= Radius;
	}

	// Set initial UsedRadius.
	if( TimeSinceLastUpdate < 0 )
	{
		UsedRadius			= WantedRadius;
		TimeSinceLastUpdate = 0;
	}

	// Radius changed, reset lerp time.
	if( LastWantedRadius != WantedRadius )
		TimeSinceLastUpdate = 0;

	unguard;
}

//
//	FSoundSource::UpdatePriority
//
void FSoundSource::UpdatePriority()
{
	guard(FSoundSource::UpdatePriority);

	UViewport* Viewport	= AudioSubsystem->GetViewport();
	if ( !Viewport )
		return;

	FLOAT RadiusFactor;
	if( ZoneRadius )
		RadiusFactor = 1 - FDist(Location, Viewport->Actor->GetViewTarget()->Location) / (GAudioMaxRadiusMultiplier*ZoneRadius);
	else
		RadiusFactor = 1;

	// Black magic...
	RadiusFactor	= Clamp(RadiusFactor, 0.01f, 1.f);
	Volume			= Clamp(Volume, 0.f, 1.f );
	Priority		= (Volume ? (0.85f + 0.15f * Volume) : 0.f ) * RadiusFactor 
					+ ((Flags & SF_Music)		?	4 : 0)
					+ ((Flags & SF_Streaming)	?	3 : 0)
					+ ((Flags & SF_Voice)		?	3 : 0)
					+ ((Flags & SF_No3D)		?	2 : 0);

	unguard;
}

//
//	FSoundSource::Update
//
UBOOL FSoundSource::UpdateBase( FLOAT DeltaTime )
{
	guard(FSoundSource::Update);

	FListener* Listener = AudioSubsystem->GetListener();

	if( Actor )
		check( Actor->IsValid() );

	if( !Initialized || IsPaused() )
		return 0;

	// Update position, velocity, pitch and volume from actor (if wanted)
	if( Actor && !(Flags & SF_NoUpdates) )
	{
		// Set location.
		Location = (Flags & SF_RootMotion) ? Actor->GetRootLocation() : Actor->Location;
		if( !(Flags & SF_No3D) )
			Velocity = (Actor->Velocity - Listener->Velocity) * Sound->GetVelocityScale();

		// Set pitch.
		if( (Flags & SF_UpdatePitch) )
			Pitch = Clamp<FLOAT>( Actor->SoundPitch / 64.f, 0.5f, 2.0f );

		// Set Volume.
		if( (Id & 14) == SLOT_Ambient*2 )
		{
			UsedVolume = Volume = Actor->GetAmbientVolume(AudioSubsystem->AmbientVolume) * AudioSubsystem->SoundVolume;
			if( Actor->LightType!=LT_None )
				UsedVolume *= Actor->LightBrightness / 255.f;
		}
	}
	else
	// Voice sources without an actor attached that aren't SF_No3D want to be played head- relative.
	if( (Flags & SF_Voice) && !(Flags & SF_No3D) )
		Location = Listener->Location;
	else
		UsedVolume = Volume;

	if( FadeMode == FADE_In )
	{
		// Disregard initial loading time.
		if( DeltaTime < 1.f )
			FadeTime += DeltaTime;
		if( FadeTime >= FadeDuration )
			FadeMode = FADE_None;
		else
			UsedVolume *= FadeTime / FadeDuration;
	}
	
	if( FadeMode == FADE_Out )
	{
		// Disregard initial loading time.
		if( DeltaTime < 1.f )
			FadeTime += DeltaTime;
		if( FadeTime >= FadeDuration )
		{
			Priority = 0;
			Stop();
			return 0;
		}
		else
			UsedVolume *= (1.f - FadeTime / FadeDuration);
	}

	UpdateRadii();
	UpdatePriority();

	// Smooth transition between radii.
	TimeSinceLastUpdate += DeltaTime;

	if( TimeSinceLastUpdate < 1.f )
		UsedRadius = Lerp( UsedRadius, WantedRadius, TimeSinceLastUpdate );
	else
		UsedRadius = WantedRadius;
			
	UsedRadius = Max( UsedRadius, 1.f );

	return 1;

	unguard;
}

//
//	FListener::FListener
//
FListener::FListener()
{
	guard(FListener::FListener);
	EAXEffect		= NULL;
	LastEAXEffect	= (UI3DL2Listener*) ((PTRINT) 0xDEADDEAD); // ensures that EAX effect is set at least once.
	Initialized		= 0;
	unguard;
}

//
//	FListener::Update
//
void FListener::Update( FVector InUp, FVector InRight, FVector InFront, FVector InLocation, FPointRegion InRegion, FLOAT DeltaTime, UI3DL2Listener* InEAXEffect )
{
	guard(FListener::Update);
	Up				= InUp;
	Right			= InRight;
	Front			= InFront;
	LastLocation	= Location;
	Location		= InLocation;
	Region			= InRegion;
	LastEAXEffect	= EAXEffect;
	EAXEffect		= InEAXEffect;
	
	if( Initialized )
		Velocity	= (Location - LastLocation) / DeltaTime;
	else
	{	
		Velocity	= FVector(0,0,0);
		Initialized	= 1;
	}
	unguard;
}

//
//	FListener::Reset
//
void FListener::Reset()
{
	guard(FListener::Reset);
	unguard;
}

/*-----------------------------------------------------------------------------
	USound implementation.
-----------------------------------------------------------------------------*/

void FSoundData::Load()
{
	guard(FSoundData::Load);

    // gam --- Prevent loading of procedural sounds
    checkSlow( Owner );
    checkSlow( Owner->GetClass() == USound::StaticClass() );
    // --- gam

	UBOOL Loaded = SavedPos>0;

	guard(0);
	TLazyArray<BYTE>::Load();
	unguard;

	if( Loaded && (Owner->FileType != FName(TEXT("PS2"))) )
	{
		// Calculate our duration.
		guard(1);
		guard(callingGetPerioid);
		Owner->Duration = GetPeriod();
		unguard;
		unguard;

		// Derive these from the exposed 'low quality' preference setting.
		INT Force8Bit = 0;
		INT ForceHalve = 0;
		guard(3);
		if( Owner->Audio && Owner->Audio->LowQualitySound() && !GIsEditor )
		{
			Force8Bit = 1;
			ForceHalve = 1;
		}
		unguard;

		// Frequencies below this sample rate will NOT be downsampled.
		DWORD FreqThreshold = 22050;

		// Reduce sound frequency and/or bit depth if required.
		if( Force8Bit || ForceHalve )
		{		
			// If ReadWaveInfo returns true, all relevant Wave chunks were found and 
			// all pointers in the WaveInfo structure have been successfully initialized.			
			guard(4);
			FWaveModInfo WaveInfo;
			if( WaveInfo.ReadWaveInfo(*this) && WaveInfo.SampleDataSize>4  ) 
			{				
				// Three main conversions:
				// * Halving the frequency -> simple 0.25, 0.50, 0.25 kernel. 
				// * Reducing bit-depth 8->16  
				// * Both in one sweep.  
				//
				// Important: Wave data ALWAYS padded to use 16-bit alignment even
				// though the number of bytes in pWaveDataSize may be odd.	
				UBOOL ReduceBits = ((Force8Bit) && (*WaveInfo.pBitsPerSample == 16));
				UBOOL ReduceFreq = ((ForceHalve) && (*WaveInfo.pSamplesPerSec >= FreqThreshold));
				if( ReduceBits && ReduceFreq )
				{
					// Convert 16-bit sample to 8 bit and halve the frequency too.
					guard(5);
					WaveInfo.HalveReduce16to8();
					unguard;
				}
				else if (ReduceBits && (!ReduceFreq))
				{	
					// Convert 16-bit sample down to 8-bit.
					guard(6);
					WaveInfo.Reduce16to8();
					unguard;
				}
				else if( ReduceFreq )
				{
					// Just halve the frequency. Separate cases for 16 and 8 bits.
					guard(7);
					WaveInfo.HalveData();
					unguard;
				}
				guard(8);
				WaveInfo.UpdateWaveData( *this );
				unguard;
			}
			unguard;
		}

		// Register it.
		Owner->OriginalSize = Num();
	} 
	else if ( Loaded && (Owner->FileType == FName(TEXT("PS2"))) ) 
	{
		// Register it.
		Owner->Duration = GetPeriod();
		Owner->OriginalSize = Num();
	}
	unguard;
}

FLOAT FSoundData::GetPeriod()
{
	FLOAT Period = 0.f;
	if( Owner->FileType != FName(TEXT("PS2")) )
	{
		// Ensure the data is present.
		TLazyArray<BYTE>::Load();

		// Calculate the sound's duration.
		FWaveModInfo WaveInfo;
		if( WaveInfo.ReadWaveInfo(*this) )
		{
			#define DEFAULT_FREQUENCY (22050)
			// These WaveInfo pointers point at raw WAV-header offsets that are not naturally aligned
			// (an unaligned dereference is undefined behaviour, and a SIGBUS on ARM): each field is read
			// via appMemcpy into an aligned local.
			_WORD Channels = 0, BitsPerSample = 0;
			DWORD SamplesPerSec = 0, WaveDataSize = 0;
			appMemcpy( &Channels,      WaveInfo.pChannels,      sizeof(Channels) );
			appMemcpy( &BitsPerSample, WaveInfo.pBitsPerSample, sizeof(BitsPerSample) );
			appMemcpy( &SamplesPerSec, WaveInfo.pSamplesPerSec, sizeof(SamplesPerSec) );
			appMemcpy( &WaveDataSize,  WaveInfo.pWaveDataSize,  sizeof(WaveDataSize) );
			INT DurDiv = Channels * BitsPerSample * SamplesPerSec;
			if ( DurDiv ) Period = WaveDataSize * 8.f / (FLOAT)DurDiv;
		}
		return Period;
	} else {
		// Ensure the data is present.
		TLazyArray<BYTE>::Load();

		// Get the period from the header.
		appMemcpy( &Period, ((BYTE*) Data) + sizeof(INT), sizeof(FLOAT) );
	}
	return Period;
}
// gam ---
USound::USound()
    : Data( this )
{
    // TODO: for some reason these aren't being read from the defaultproperties
    Duration		= -1.f;
    Likelihood		= 1.f;
	Flags			= 0;
	VoiceCodec		= CODEC_None;
	Handle			= 0;
	InitialSeekTime	= 0.f;
}

USound::USound( INT InFlags )
: Data( this )
{
	Flags			= InFlags;
	VoiceCodec		= CODEC_None;
	Duration		= 1.f;
	Likelihood		= 1.f;
	Handle			= 0;
	InitialSeekTime	= 0.f;
}

USound::USound( const TCHAR* InFilename, INT InFlags )
: Data( this )
{
	Filename		= InFilename;
	Flags			= InFlags | SF_Streaming;
	VoiceCodec		= CODEC_None;
	Duration		= 1.f;
	Likelihood		= 1.f;
	Handle			= 0;
	InitialSeekTime	= 0.f;
}

USound* USound::RenderSoundPlay( FLOAT *Volume, FLOAT *Pitch )
{
    return this;
}

FLOAT USound::GetDuration()
{
	guard(USound::GetDuration);
	if ( Duration < 0.f )
		Duration = Data.GetPeriod();
	return Duration;
	unguard;
}
void USound::SetDuration(FLOAT Duration)
{
	guard(USound::SetDuration);
    this->Duration = Duration;
	unguard;
}
void USound::Serialize( FArchive& Ar )
{
	guard(USound::Serialize);
	Super::Serialize( Ar );
	Ar << FileType;

    if( Ar.LicenseeVer() >= 2 )
    	Ar << Likelihood;

	if( Ar.IsLoading() || Ar.IsSaving() )
	{
		Ar << Data;
	}
	else Ar.CountBytes( OriginalSize, OriginalSize );

    if( Ar.IsLoading() )
	{
		Flags = 0;
		VoiceCodec = CODEC_None;
	}

	unguard;
}

void USound::Destroy()
{
	guard(USound::Destroy);
	if( Audio )
		Audio->NoteDestroy( this );
	Super::Destroy();
	unguard;
}

void USound::PostLoad()
{
	guard(USound::PostLoad);
	Super::PostLoad();
	unguard;
}
bool USound::IsValid()
{
	guard(USound::IsValid);
    return (true);    
    unguard;
}
FSoundData &USound::GetData()
{
	guard(USound::GetData);
    return (Data);
    unguard;
}
FName USound::GetFileType()
{
	guard(USound::GetData);
    return (FileType);
    unguard;
}
void USound::SetFileType (FName FileType)
{
	guard(USound::SetFileType);
    this->FileType = FileType;
    unguard;
}
const TCHAR* USound::GetFilename()
{
	guard(USound::GetFilename);
    return *Filename;
    unguard;
}
INT USound::GetOriginalSize()
{
	guard(USound::GetOriginalSize);
    return (OriginalSize);
    unguard;
}
void* USound::GetHandle()
{
	guard(USound::GetHandle);
    return (Handle);
    unguard;
}
void USound::SetHandle(void* Handle)
{
	guard(USound::SetHandle);
    this->Handle = Handle;
    unguard;
}
INT USound::GetFlags()
{
	guard(USound::GetFlags);
    return (Flags);
    unguard;
}
FLOAT USound::GetRadius() // sjs
{
	guard(USound::GetRadius);
    return (BaseRadius);
    unguard;
}
FLOAT USound::GetVelocityScale() // gam
{
	guard(USound::GetVelocityScale);
    return (VelocityScale);
    unguard;
}

UAudioSubsystem* USound::Audio = NULL;
void USound::PS2Convert()
{
	guard(USound::PS2Convert);

	#if HAVE_VAG

	/*
	 * Convert a standard wave sound to PS2 format.
	 *
	 * 3 Steps:
	 * - Promote 8 bit sounds to 16 bit for ADPCM compression.
	 * - Perform VAG conversion.
	 */

	Data.Load();

	FWaveModInfo WavData;
	WavData.ReadWaveInfo( Data );

	// Check loop.
	INT SoundLoops = WavData.SampleLoopsNum;

	// GWarn->Logf( TEXT("Sound: %s Size: %i Depth: %i Rate: %i"), GetPathName(), WavData.SampleDataSize, *WavData.pBitsPerSample, *WavData.pSamplesPerSec );
	// Copy the sample data into a working buffer.
	// Convert 8 bit samples to 16 bit data as we go.
	TArray<BYTE> WorkData;

	INT NewDataSize = WavData.SampleDataSize;
	INT SoundRate = *WavData.pSamplesPerSec;

	FLOAT Period = 0.f;
	INT DurDiv =  *WavData.pChannels * *WavData.pBitsPerSample * *WavData.pSamplesPerSec;  
	if ( DurDiv )
		Period = *WavData.pWaveDataSize * 8.f / (FLOAT) DurDiv;

	guard(Promote8to16);
	if (*WavData.pBitsPerSample == 8)
	{
		WorkData.Add( NewDataSize*2 );
		for (INT i=0; i<NewDataSize; i++)
		{
			WorkData(i*2)	= 0;
			WorkData(i*2+1)	= WavData.SampleDataStart[i] + 128;
		}
	} else {
		WorkData.Add( NewDataSize );
		for (INT i=0; i<NewDataSize; i++)
			WorkData(i) = WavData.SampleDataStart[i];
	}
	unguard;

	EncVagInit( ENC_VAG_MODE_NORMAL );

	INT WorkSamples = WorkData.Num() / 2;
	INT NeededBlocks = WorkSamples / 28;
	if (WorkSamples % 28 != 0)
		NeededBlocks++;

	// Prepare output data.
	Data.Empty();

	// Add custom PS2 header.
	guard(CustomHeader);
	Data.Add( 16 );
	appMemcpy( &Data(0), &SoundRate, sizeof(INT) );
	appMemcpy( &Data(4), &Period, sizeof(FLOAT) );
	appMemcpy( &Data(8), &SoundLoops, sizeof(INT) );
	INT Pad=0;
	appMemcpy( &Data(12), &Pad, sizeof(INT) );
	unguard;

	// Encode one VAG block per every 28 samples.
	guard(EncodeVAGBlocks);
	INT ByteCount = 0;
	for (INT i=0; i<NeededBlocks; i++)
	{
		TArray<BYTE> WorkBlock;
		WorkBlock.Add( 28*2 ); // Add 28 samples worth of room.
		for (INT j=0; j<28*2; j++)
		{
			if (ByteCount < WorkData.Num())
				WorkBlock(j) = WorkData(ByteCount++);
			else
				WorkBlock(j) = 0; // Pad with zero.
		}
		INT BlockAttribute;
		if (SoundLoops)
		{
			if (i == 0)
				BlockAttribute = ENC_VAG_LOOP_START;
			else if (i+1 == NeededBlocks)
				BlockAttribute = ENC_VAG_LOOP_END;
			else
				BlockAttribute = ENC_VAG_LOOP_BODY;
		} else {
			if (i+1 == NeededBlocks)
				BlockAttribute = ENC_VAG_1_SHOT_END;
			else
				BlockAttribute = ENC_VAG_1_SHOT;
		}

		TArray<BYTE> EncodedBlock;
		EncodedBlock.Add( 16 );
		EncVag( (short*) &WorkBlock(0), (short*) &EncodedBlock(0), BlockAttribute );

		INT StartPosition = Data.Num();
		Data.Add( 16 );
		for (j=0; j<16; j++)
			Data(StartPosition+j) = EncodedBlock(j);
	}
	unguard;

	// Finish off the one shot conversion.
	if (SoundLoops == 0)
	{
		guard(FinishOneShot);
		INT StartPosition = Data.Num();
		Data.Add( 16 );
		EncVagFin( (short*) &Data(StartPosition) );
		unguard;
	}

	// Set the file type.
	FileType = FName( TEXT("PS2") );

	/*
	for (INT i=0; i<Data.Num()/16; i++)
	{
		GWarn->Logf( TEXT("%02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x %02x%02x"),
			Data(0 + i*16),  Data(1 + i*16),  Data(2 + i*16),  Data(3 + i*16),
			Data(4 + i*16),  Data(5 + i*16),  Data(6 + i*16),  Data(7 + i*16),
			Data(8 + i*16),  Data(9 + i*16),  Data(10 + i*16), Data(11 + i*16),
			Data(12 + i*16), Data(13 + i*16), Data(14 + i*16), Data(15 + i*16) );
	}
	*/

	#endif

	unguard;
}
IMPLEMENT_CLASS(USound);

// WARNING: HACKS AHEAD!
//
// In order to make all existing "USound" code work I had to derive these
// classes from USound. The similarity ends there, though -- USound doesn't
// have an interface per say; it's really not much more than a struct. So,
// since we want our objects to be interchangable with USound objects, we
// don't want to call Super::Anything because we aren't really a USound.
//
// So, never do Super::Anything, do Super::Super::Anything instead.

UProceduralSound::UProceduralSound()
{
	guard(UProceduralSound::UProceduralSound);

    BaseSound = NULL;
    RenderedVolumeModification = 1.0F;
    RenderedPitchModification = 1.0F;

    unguard;
}

USound*  UProceduralSound::RenderSoundPlay( FLOAT *Volume, FLOAT *Pitch )
{
	guard(UProceduralSound::RenderSoundPlay);

    FLOAT NewPitch;

    if( !IsValid() )
        return this;

    RenderedPitchModification = 1.0F + (PitchModification / 100.0F) + (appSRand() * PitchVariance / 100.0F);
	NewPitch = ::Max(0.f,*Pitch * RenderedPitchModification);
    *Pitch = NewPitch;
    return BaseSound->RenderSoundPlay( Volume, Pitch );
    unguard;
}

bool UProceduralSound::IsValid()
{
	guard(UProceduralSound::IsValid);
    return ((BaseSound != NULL) && (BaseSound != this) && BaseSound->IsValid() );
    unguard;
}
FSoundData &UProceduralSound::GetData()
{
	guard(UProceduralSound::GetData);
    check (IsValid());
    return (BaseSound->GetData());
    unguard;
}
FName UProceduralSound::GetFileType()
{
	guard(UProceduralSound::GetFileType);
    check (IsValid());
    return (BaseSound->GetFileType());
    unguard;
}
void UProceduralSound::SetFileType (FName FileType)
{
	guard(UProceduralSound::SetFileType);
    check (IsValid());
    BaseSound->SetFileType (FileType);
    unguard;
}
const TCHAR* UProceduralSound::GetFilename()
{
	guard(UProceduralSound::GetFilename);
    check (IsValid());
    return (BaseSound->GetFilename());
    unguard;
}
INT UProceduralSound::GetOriginalSize()
{
	guard(UProceduralSound::GetOriginalSize);
    if (!IsValid())
        return (0);
    else
        return (BaseSound->GetOriginalSize());
    unguard;
}
void* UProceduralSound::GetHandle()
{
	guard(UProceduralSound::GetHandle);
    if (!IsValid())
        return (NULL);
    else
        return (BaseSound->GetHandle());
    unguard;
}
void UProceduralSound::SetHandle(void* Handle)
{
	guard(UProceduralSound::SetHandle);
    check (IsValid());
    BaseSound->SetHandle(Handle);
    unguard;
}
INT UProceduralSound::GetFlags()
{
	guard(UProceduralSound::GetFlags);
    check (IsValid());
    return (BaseSound->GetFlags());
    unguard;
}
FLOAT UProceduralSound::GetDuration()
{
	guard(UProceduralSound::GetDuration);
    if (!IsValid())
        return (0);
    else
	    return (BaseSound->GetDuration() * (1.0F / RenderedPitchModification));
	unguard;
}
FLOAT UProceduralSound::GetRadius()
{
	guard(UProceduralSound::GetDuration);
    if (!IsValid())
        return 0.0f;
    else
	    return BaseSound->GetRadius();
	unguard;
}
FLOAT UProceduralSound::GetVelocityScale() // gam
{
	guard(UProceduralSound::GetVelocityScale);
    if (!IsValid())
        return 0.0f;
    else
	    return BaseSound->GetVelocityScale();
    unguard;
}
void UProceduralSound::SetDuration(FLOAT Duration)
{
	guard(UProceduralSound::SetDuration);
    if (IsValid())
        BaseSound->SetDuration (Duration);
	unguard;
}
void UProceduralSound::PS2Convert()
{
	guard(UProceduralSound::PS2Convert);
	unguard;
}
void UProceduralSound::Load()
{
	guard(UProceduralSound::Load);
    if (IsValid())
        BaseSound->Load();
	unguard;
}

void UProceduralSound::Serialize( FArchive& Ar )
{
	guard(UProceduralSound::Serialize);

	Super::Super::Serialize( Ar );

  	Ar << Likelihood; // Manually serialize USound::Likelihood.

    Ar << BaseSound;

    Ar << PitchModification;
    Ar << VolumeModification;

    Ar << PitchVariance;
    Ar << VolumeVariance;

	unguard;
}

void UProceduralSound::Destroy()
{
	guard(UProceduralSound::Destroy);
	Super::Super::Destroy();
	unguard;
}

void UProceduralSound::PostLoad()
{
	guard(UProceduralSound::PostLoad);
	Super::Super::PostLoad();
	unguard;
}

void UProceduralSound::PostEditChange()
{
	guard(UProceduralSound::PostEditChange);

	Super::Super::PostEditChange();

    if( PitchModification < -100.0 )
        PitchModification = -100.0;

    if( VolumeModification < -100.0 )
        VolumeModification = -100.0;

    if( PitchVariance < -100.0 )
        PitchVariance = -100.0;

    if( VolumeVariance < -100.0 )
        VolumeVariance = -100.0;

	unguard;
}

IMPLEMENT_CLASS(UProceduralSound);

USoundGroup::USoundGroup()
{
    RenderedSound = NULL;
}

void USoundGroup::RefreshGroup( const FString &Package )
{
	guard(USoundGroup::RefreshGroup);
	
    FString PackageName;
    INT i;

    debugf( NAME_Warning, TEXT("USoundGroup::RefreshGroup() being called!") );

    UObject *PackageObject;

    Sounds.Empty();

	if( Package.Len() <= 0 )
		return;

    i = Package.InStr( TEXT(".") );

    if( i > 0 )
        PackageName = Package.Left( i );
    else
        PackageName = Package;

    PackageObject = Cast<UPackage>( LoadPackage( NULL, *PackageName, 0 ) );

    if( !PackageObject )
    {
        GWarn->Logf( NAME_Warning, TEXT("Can't load package %s"), *PackageName );
        return;
    }

    PackageObject = FindObject<UPackage>( NULL, *Package );

    if( !PackageObject )
    {
        GWarn->Logf( NAME_Warning, TEXT("Can't find package %s"), *Package );
        return;
    }
    
	for( TObjectIterator<USound> It; It; ++It )
	{
        USound *Sound = *It;
        USoundGroup *SoundGroup;

        if( !Sound->IsIn( PackageObject ) )
            continue;

        Sounds.AddItem( Sound );
        
        SoundGroup = Cast<USoundGroup>( Sound );
    }

    RefreshGroup();

    unguard;
}

void USoundGroup::RefreshGroup( UBOOL Force )
{
	guard(USoundGroup::RefreshGroup);

    if( !Force && TotalLikelihood < 0 )
        return;

    TotalLikelihood = 0;

	for( INT i = 0; i < Sounds.Num(); i++ )
	{
        USound *Sound = Sounds(i);
        USoundGroup *SoundGroup;
        
        SoundGroup = Cast<USoundGroup>( Sound );
        
        if( SoundGroup )
		{
            SoundGroup->RefreshGroup();
			TotalLikelihood += Sound->Likelihood;
		}
    }

	unguard;
}

USound* USoundGroup::RenderSoundPlay( FLOAT *Volume, FLOAT *Pitch )
{
	guard(USoundGroup::RenderSoundPlay);

    if( Sounds.Num() <= 0 )
    {
        debugf( NAME_Warning, TEXT("SoundGroup %s has no members!"), GetName() );
        return this;
    }
	TotalLikelihood = 0.f;
    for( INT i = 0; i < Sounds.Num(); i++)
 		TotalLikelihood += Sounds(i)->Likelihood;

    FLOAT r = appSRand() * TotalLikelihood;
    RenderedSound = Sounds(0);
    FLOAT Sum = 0.f;
    for( INT i = 0; i < Sounds.Num(); i++)
    {
		Sum += Sounds(i)->Likelihood;
        if( r < Sum )
        {
            RenderedSound = Sounds(i); 
            break;
        }
    }
    return RenderedSound->RenderSoundPlay( Volume, Pitch );
	unguard;
}

bool USoundGroup::IsValid()
{
	guard(USoundGroup::IsValid);
    return ((RenderedSound != NULL) && (RenderedSound->IsValid()));
    unguard;
}
FSoundData &USoundGroup::GetData()
{
	guard(USoundGroup::GetData);
    check (RenderedSound);
    return (RenderedSound->GetData());
    unguard;
}
FName USoundGroup::GetFileType()
{
	guard(USoundGroup::GetFileType);
    check (RenderedSound);
    return (RenderedSound->GetFileType());
    unguard;
}
void USoundGroup::SetFileType (FName FileType)
{
	guard(USoundGroup::SetFileType);
    check (RenderedSound);
    RenderedSound->SetFileType (FileType);
    unguard;
}
const TCHAR* USoundGroup::GetFilename()
{
	guard(USoundGroup::GetFilename);
    check (RenderedSound);
    return (RenderedSound->GetFilename());
    unguard;
}
INT USoundGroup::GetOriginalSize()
{
	guard(USoundGroup::GetOriginalSize);
    if (!RenderedSound)
        return (0);
    else
        return (RenderedSound->GetOriginalSize());
    unguard;
}
void* USoundGroup::GetHandle()
{
	guard(USoundGroup::GetHandle);
    if (!RenderedSound)
        return (NULL);
    else
        return (RenderedSound->GetHandle());
    unguard;
}
void USoundGroup::SetHandle(void* Handle)
{
	guard(USoundGroup::SetHandle);
    check (RenderedSound);
    RenderedSound->SetHandle (Handle);
    unguard;
}
INT USoundGroup::GetFlags()
{
	guard(USoundGroup::GetFlags);
    check (RenderedSound);
    return (RenderedSound->GetFlags());
    unguard;
}
FLOAT USoundGroup::GetDuration()
{
	guard(USoundGroup::GetDuration);
    check( Sounds.Num() );
    check( Sounds(0) );
	return Sounds(0)->GetDuration();
	unguard;
}
FLOAT USoundGroup::GetRadius()
{
	guard(USoundGroup::GetRadius);
    if( !RenderedSound )
    {
        if( Sounds.Num() <= 0 )
            return( 0.f );

        return Sounds(0)->GetRadius();
    }
	return RenderedSound->GetRadius();
	unguard;
}
FLOAT USoundGroup::GetVelocityScale()
{
	guard(USoundGroup::GetVelocityScale);
    if( !RenderedSound )
    {
        if( Sounds.Num() <= 0 )
            return( 0.f );

        return Sounds(0)->GetVelocityScale();
    }
	return RenderedSound->GetVelocityScale();
	unguard;
}
void USoundGroup::SetDuration(FLOAT Duration)
{
	guard(USoundGroup::SetDuration);
    check (RenderedSound);
    RenderedSound->SetDuration (Duration);
	unguard;
}
void USoundGroup::PS2Convert()
{
	guard(USoundGroup::PS2Convert);
	unguard;
}

void USoundGroup::Serialize( FArchive& Ar )
{
	guard(USoundGroup::Serialize);
	Super::Super::Serialize( Ar );

    if( Ar.LicenseeVer() < 0x1B )
    {
        Ar << Package;

	    if( !Ar.IsLoading() && !Ar.IsSaving() )
	    {
		    for( INT i = 0; i < Sounds.Num(); i++ )
			    Ar << Sounds(i);
	    }
	}
	else
	{
	    Ar << Sounds;
	    
	    if( Ar.IsLoading() )
	        TotalLikelihood = -1.0;
	}
	
	unguard;
}

void USoundGroup::Destroy()
{
	guard(USoundGroup::Destroy);
	Super::Super::Destroy();
	unguard;
}

void USoundGroup::PostLoad()
{
	guard(USoundGroup::PostLoad);
	Super::Super::PostLoad();
	
	if( Package.Len() )
	    RefreshGroup( Package );
	    
	unguard;
}
void USoundGroup::PostEditChange()
{
	guard(USoundGroup::PostEditChange);
	Super::Super::PostEditChange();
	RefreshGroup();
	unguard;
}

void USoundGroup::Load()
{
    guard(USoundGroup::Load);
    for (int i=0; i<Sounds.Num(); i++)
        Sounds(i)->Load();
    unguard;
}

IMPLEMENT_CLASS(USoundGroup);

// --- gam

/*-----------------------------------------------------------------------------
	WaveModInfo implementation - downsampling of wave files.
-----------------------------------------------------------------------------*/

UBOOL FWaveModInfo::ReadWaveInfo( TArray<BYTE>& WavData )
{
	guard(FWaveModInfo::ReadWaveInfo);
	FFormatChunk* FmtChunk;
	FRiffWaveHeader* RiffHdr = (FRiffWaveHeader*)&WavData(0);
	WaveDataEnd = &WavData(0) + WavData.Num();	

	// Verify we've got a real 'WAVE' header.
#if __INTEL_BYTE_ORDER__
	if ( RiffHdr->wID != mmioFOURCC('W','A','V','E') )
	{
		return 0;
	}
#else
	if ( (RiffHdr->wID != (mmioFOURCC('W','A','V','E'))) &&
	     (RiffHdr->wID != (mmioFOURCC('E','V','A','W'))) )
	{
		return 0;
	}

	bool alreadySwapped = (RiffHdr->wID == (mmioFOURCC('W','A','V','E')));
	if (!alreadySwapped)
	{
		RiffHdr->rID = INTEL_ORDER32(RiffHdr->rID);
		RiffHdr->ChunkLen = INTEL_ORDER32(RiffHdr->ChunkLen);
		RiffHdr->wID = INTEL_ORDER32(RiffHdr->wID);
	}
#endif

	pMasterSize = &RiffHdr->ChunkLen;

	// The appMemcpy indirect accessing is necessary to avoid PSX2 read alignment problems but
	// somehow causes a crash on PCs in "Look for a 'smpl' chunk."

#if __PSX2_EE__

	FRiffChunkOld RiffChunk;
	FRiffChunkOld* RiffChunkPtr = (FRiffChunkOld*)&WavData(3*4);
	appMemcpy( &RiffChunk, RiffChunkPtr, sizeof(FRiffChunkOld) );

	// Look for the 'fmt ' chunk.
	while( ( ((BYTE*)RiffChunkPtr + 8) < WaveDataEnd)  && ( RiffChunk.ChunkID != mmioFOURCC('f','m','t',' ') ) )
	{
		// Go to next chunk.
		RiffChunkPtr = (FRiffChunkOld*) ( (BYTE*)RiffChunkPtr + Pad16Bit(RiffChunk.ChunkLen) + 8);
		appMemcpy( &RiffChunk, RiffChunkPtr, sizeof(FRiffChunkOld) );
	}
	// Chunk found ?
	if( RiffChunk.ChunkID != mmioFOURCC('f','m','t',' ') )
		return 0;

	FmtChunk = (FFormatChunk*)((BYTE*)RiffChunkPtr + 8);
	pBitsPerSample  = &FmtChunk->wBitsPerSample;
	pSamplesPerSec  = &FmtChunk->nSamplesPerSec;
	pAvgBytesPerSec = &FmtChunk->nAvgBytesPerSec;
	pBlockAlign		= &FmtChunk->nBlockAlign;
	pChannels       = &FmtChunk->nChannels;

	// re-initalize the RiffChunk pointer
	RiffChunkPtr = (FRiffChunkOld*)&WavData(3*4);
	appMemcpy( &RiffChunk, RiffChunkPtr, sizeof(FRiffChunkOld) );

	// Look for the 'data' chunk.
	while( ( ((BYTE*)RiffChunkPtr + 8) < WaveDataEnd) && ( RiffChunk.ChunkID != mmioFOURCC('d','a','t','a') ) )
	{
		// Go to next chunk.
		RiffChunkPtr = (FRiffChunkOld*) ( (BYTE*)RiffChunkPtr + Pad16Bit(RiffChunk.ChunkLen) + 8); 
		appMemcpy( &RiffChunk, RiffChunkPtr, sizeof(FRiffChunkOld) );
	} 
	// Chunk found ?
	if( RiffChunk.ChunkID != mmioFOURCC('d','a','t','a') )
		return 0;

	SampleDataStart = (BYTE*)RiffChunkPtr + 8;
	pWaveDataSize   = &RiffChunkPtr->ChunkLen;
	SampleDataSize  =  RiffChunk.ChunkLen;
	OldBitsPerSample = FmtChunk->wBitsPerSample;
	SampleDataEnd   =  SampleDataStart+SampleDataSize;

	NewDataSize	= SampleDataSize;

	// Re-initalize the RiffChunk pointer
	RiffChunkPtr = (FRiffChunkOld*)&WavData(3*4);
	appMemcpy( &RiffChunk, RiffChunkPtr, sizeof(FRiffChunkOld) );

	// Look for a 'smpl' chunk.
	while( ( (((BYTE*)RiffChunkPtr) + 8) < WaveDataEnd) && ( RiffChunk.ChunkID != mmioFOURCC('s','m','p','l') ) )
	{
		// Go to next chunk.
		RiffChunkPtr = (FRiffChunkOld*) ( (BYTE*)RiffChunkPtr + Pad16Bit(RiffChunk.ChunkLen) + 8); 
		appMemcpy( &RiffChunk, RiffChunkPtr, sizeof(FRiffChunkOld) );
	} 

	// Chunk found ? smpl chunk is optional.
	// Find the first sample-loop structure, and the total number of them.
	if( (INT)RiffChunkPtr+4<(INT)WaveDataEnd && RiffChunk.ChunkID == mmioFOURCC('s','m','p','l') )
	{
		FSampleChunk pSampleChunk;
		appMemcpy(&pSampleChunk, (BYTE*)RiffChunkPtr + 8, sizeof(FSampleChunk));
		SampleLoopsNum = pSampleChunk.cSampleLoops;
		pSampleLoop = (FSampleLoop*) ((BYTE*)RiffChunkPtr + 8 + sizeof(FSampleChunk)); 
		/*
		FSampleChunk* pSampleChunk =  (FSampleChunk*)( (BYTE*)RiffChunk + 8);
		SampleLoopsNum  = pSampleChunk->cSampleLoops; // Number of tSampleLoop structures.
		// First tSampleLoop structure starts right after the tSampleChunk.
		pSampleLoop = (FSampleLoop*) ((BYTE*)pSampleChunk + sizeof(FSampleChunk)); 
		*/		
	}

#else
	
	FRiffChunkOld* RiffChunk = (FRiffChunkOld*)&WavData(3*4);
	// Look for the 'fmt ' chunk.
	while( ( ((BYTE*)RiffChunk + 8) < WaveDataEnd)  && ( INTEL_ORDER32(RiffChunk->ChunkID) != mmioFOURCC('f','m','t',' ') ) )
	{
		// Go to next chunk.
		RiffChunk = (FRiffChunkOld*) ( (BYTE*)RiffChunk + Pad16Bit(INTEL_ORDER32(RiffChunk->ChunkLen)) + 8);
	}

	// Chunk found ?
	if( INTEL_ORDER32(RiffChunk->ChunkID) != mmioFOURCC('f','m','t',' ') )
	{
		#if !__INTEL_BYTE_ORDER__  // swap them back just in case.
			if (!alreadySwapped)
			{
				RiffHdr->rID = INTEL_ORDER32(RiffHdr->rID);
				RiffHdr->ChunkLen = INTEL_ORDER32(RiffHdr->ChunkLen);
				RiffHdr->wID = INTEL_ORDER32(RiffHdr->wID);
            }
		#endif
		return 0;
	}

	FmtChunk = (FFormatChunk*)((BYTE*)RiffChunk + 8);

#if !__INTEL_BYTE_ORDER__
	if (!alreadySwapped)
	{
		FmtChunk->wFormatTag = INTEL_ORDER16(FmtChunk->wFormatTag);
		FmtChunk->nChannels = INTEL_ORDER16(FmtChunk->nChannels);
		FmtChunk->nSamplesPerSec = INTEL_ORDER32(FmtChunk->nSamplesPerSec);
		FmtChunk->nAvgBytesPerSec = INTEL_ORDER32(FmtChunk->nAvgBytesPerSec);
		FmtChunk->nBlockAlign = INTEL_ORDER16(FmtChunk->nBlockAlign);
		FmtChunk->wBitsPerSample = INTEL_ORDER16(FmtChunk->wBitsPerSample);
	}
#endif

	pBitsPerSample  = &FmtChunk->wBitsPerSample;
	pSamplesPerSec  = &FmtChunk->nSamplesPerSec;
	pAvgBytesPerSec = &FmtChunk->nAvgBytesPerSec;
	pBlockAlign		= &FmtChunk->nBlockAlign;
	pChannels       = &FmtChunk->nChannels;

	//GWarn->Logf( TEXT("look for data chunk") );
	// re-initalize the RiffChunk pointer
	RiffChunk = (FRiffChunkOld*)&WavData(3*4);
	// Look for the 'data' chunk.
	while( ( ((BYTE*)RiffChunk + 8) < WaveDataEnd) && ( INTEL_ORDER32(RiffChunk->ChunkID) != mmioFOURCC('d','a','t','a') ) )
	{
		// Go to next chunk.
		RiffChunk = (FRiffChunkOld*) ( (BYTE*)RiffChunk + Pad16Bit(INTEL_ORDER32(RiffChunk->ChunkLen)) + 8);
	} 

	// Chunk found ?
	if( INTEL_ORDER32(RiffChunk->ChunkID) != mmioFOURCC('d','a','t','a') )
	{
		#if !__INTEL_BYTE_ORDER__  // swap them back just in case.
			if (!alreadySwapped)
			{
				RiffHdr->rID = INTEL_ORDER32(RiffHdr->rID);
				RiffHdr->ChunkLen = INTEL_ORDER32(RiffHdr->ChunkLen);
				RiffHdr->wID = INTEL_ORDER32(RiffHdr->wID);
				FmtChunk->wFormatTag = INTEL_ORDER16(FmtChunk->wFormatTag);
				FmtChunk->nChannels = INTEL_ORDER16(FmtChunk->nChannels);
				FmtChunk->nSamplesPerSec = INTEL_ORDER32(FmtChunk->nSamplesPerSec);
				FmtChunk->nAvgBytesPerSec = INTEL_ORDER32(FmtChunk->nAvgBytesPerSec);
				FmtChunk->nBlockAlign = INTEL_ORDER16(FmtChunk->nBlockAlign);
				FmtChunk->wBitsPerSample = INTEL_ORDER16(FmtChunk->wBitsPerSample);
			}
		#endif
		return 0;
	}

	#if !__INTEL_BYTE_ORDER__  // swap them back just in case.
		if (alreadySwapped) // swap back into Intel order for chunk search...
			RiffChunk->ChunkLen = INTEL_ORDER32(RiffChunk->ChunkLen);
	#endif

	SampleDataStart = (BYTE*)RiffChunk + 8;
	pWaveDataSize   = &RiffChunk->ChunkLen;
	SampleDataSize  =  INTEL_ORDER32(RiffChunk->ChunkLen);
	OldBitsPerSample = FmtChunk->wBitsPerSample;
	SampleDataEnd   =  SampleDataStart+SampleDataSize;

	if ((BYTE *) SampleDataEnd > (BYTE *) WaveDataEnd)
	{
		debugf(NAME_Warning, TEXT("Wave data chunk is too big!"));

		// fix it up by clamping data chunk...
		SampleDataEnd = (BYTE *) WaveDataEnd;
		SampleDataSize = SampleDataEnd - SampleDataStart;
		RiffChunk->ChunkLen = INTEL_ORDER32(SampleDataSize);
	}

	NewDataSize	= SampleDataSize;

	if( FmtChunk->wFormatTag != 1 )
		return 0;

#if !__INTEL_BYTE_ORDER__
	if (!alreadySwapped)
	{
		if (FmtChunk->wBitsPerSample == 16)
		{
			for (_WORD *i = (_WORD *) SampleDataStart; i < (_WORD *) SampleDataEnd; i++)
			{
				*i = INTEL_ORDER16(*i);
			}
		}
		else if (FmtChunk->wBitsPerSample == 32)
		{
			for (DWORD *i = (DWORD *) SampleDataStart; i < (DWORD *) SampleDataEnd; i++)
			{
				*i = INTEL_ORDER32(*i);
			}
		}
	}
#endif

	//GWarn->Logf( TEXT("look for smpl chunk 0x%x"), RiffChunk );
	// Re-initalize the RiffChunk pointer
	RiffChunk = (FRiffChunkOld*)&WavData(3*4);
	// Look for a 'smpl' chunk.
	while( ( (((BYTE*)RiffChunk) + 8) < WaveDataEnd) && ( INTEL_ORDER32(RiffChunk->ChunkID) != mmioFOURCC('s','m','p','l') ) )
	{
		// Go to next chunk.
		//GWarn->Logf( TEXT("go to next\n") );
		RiffChunk = (FRiffChunkOld*) ( (BYTE*)RiffChunk + Pad16Bit(INTEL_ORDER32(RiffChunk->ChunkLen)) + 8);
		//GWarn->Logf( TEXT("riff: 0x%x\n"), RiffChunk );
		//GWarn->Logf( TEXT("%i"), RiffChunk->ChunkID );
	} 

	// Chunk found ? smpl chunk is optional.
	// Find the first sample-loop structure, and the total number of them.
	// GWarn->Logf( TEXT("find sample-loop 0x%x"), RiffChunk );
	if( (PTRINT)RiffChunk+4<(PTRINT)WaveDataEnd && INTEL_ORDER32(RiffChunk->ChunkID) == mmioFOURCC('s','m','p','l') )
	{
		//GWarn->Logf( TEXT("loop") );
		FSampleChunk *pSampleChunk = (FSampleChunk *) ((BYTE*)RiffChunk + 8);
		pSampleLoop = (FSampleLoop*) ((BYTE*)RiffChunk + 8 + sizeof(FSampleChunk));

		if (((BYTE *)pSampleChunk + sizeof (FSampleChunk)) > (BYTE *)WaveDataEnd)
			pSampleChunk = NULL;

		if (((BYTE *)pSampleLoop + sizeof (FSampleLoop)) > (BYTE *)WaveDataEnd)
			pSampleLoop = NULL;

#if !__INTEL_BYTE_ORDER__
		if (!alreadySwapped)
		{
			if (pSampleChunk != NULL)
			{
				pSampleChunk->dwManufacturer = INTEL_ORDER32(pSampleChunk->dwManufacturer);
				pSampleChunk->dwProduct = INTEL_ORDER32(pSampleChunk->dwProduct);
				pSampleChunk->dwSamplePeriod = INTEL_ORDER32(pSampleChunk->dwSamplePeriod);
				pSampleChunk->dwMIDIUnityNote = INTEL_ORDER32(pSampleChunk->dwMIDIUnityNote);
				pSampleChunk->dwMIDIPitchFraction = INTEL_ORDER32(pSampleChunk->dwMIDIPitchFraction);
				pSampleChunk->dwSMPTEFormat = INTEL_ORDER32(pSampleChunk->dwSMPTEFormat);
				pSampleChunk->dwSMPTEOffset = INTEL_ORDER32(pSampleChunk->dwSMPTEOffset);
				pSampleChunk->cSampleLoops = INTEL_ORDER32(pSampleChunk->cSampleLoops);
				pSampleChunk->cbSamplerData = INTEL_ORDER32(pSampleChunk->cbSamplerData);
			}

			if (pSampleLoop != NULL)
			{
				pSampleLoop->dwIdentifier = INTEL_ORDER32(pSampleLoop->dwIdentifier);
				pSampleLoop->dwType = INTEL_ORDER32(pSampleLoop->dwType);
				pSampleLoop->dwStart = INTEL_ORDER32(pSampleLoop->dwStart);
				pSampleLoop->dwEnd = INTEL_ORDER32(pSampleLoop->dwEnd);
				pSampleLoop->dwFraction = INTEL_ORDER32(pSampleLoop->dwFraction);
				pSampleLoop->dwPlayCount = INTEL_ORDER32(pSampleLoop->dwPlayCount);
            }
		}
#endif

		SampleLoopsNum = pSampleChunk->cSampleLoops;

		/*
		FSampleChunk* pSampleChunk =  (FSampleChunk*)( (BYTE*)RiffChunk + 8);
		SampleLoopsNum  = pSampleChunk->cSampleLoops; // Number of tSampleLoop structures.
		// First tSampleLoop structure starts right after the tSampleChunk.
		pSampleLoop = (FSampleLoop*) ((BYTE*)pSampleChunk + sizeof(FSampleChunk)); 
		*/
	}

	// Couldn't byte swap this before, since it'd throw off the chunk search.
	#if !__INTEL_BYTE_ORDER__
		*pWaveDataSize = INTEL_ORDER32(*pWaveDataSize);
	#endif
#endif
	
	return 1;
	unguard;
}


//
//
// Update internal variables and shrink the data fields.
//
UBOOL FWaveModInfo::UpdateWaveData( TArray<BYTE>& WavData )
{
	guard(FWaveModInfo::UpdateWaveData);
	if( NewDataSize < SampleDataSize )
	{		
		// Shrinkage of data chunk in bytes -> chunk data must always remain 16-bit-padded.
		INT ChunkShrinkage = Pad16Bit(SampleDataSize)  - Pad16Bit(NewDataSize);

		// Update sizes.
		*pWaveDataSize  = NewDataSize;
		*pMasterSize   -= ChunkShrinkage;

		// Refresh all wave parameters depending on bit depth and/or sample rate.
		*pBlockAlign    =  *pChannels * (*pBitsPerSample >> 3); // channels * Bytes per sample
		*pAvgBytesPerSec = *pBlockAlign * *pSamplesPerSec; //sample rate * Block align

		// Update 'smpl' chunk data also, if present.
		if (SampleLoopsNum)
		{
			FSampleLoop* pTempSampleLoop = pSampleLoop;
			INT SampleDivisor = ( (SampleDataSize *  *pBitsPerSample) / (NewDataSize ) );
			for (INT SL = 0; SL<SampleLoopsNum; SL++)
			{
				pTempSampleLoop->dwStart = pTempSampleLoop->dwStart  * OldBitsPerSample / SampleDivisor;
				pTempSampleLoop->dwEnd   = pTempSampleLoop->dwEnd  * OldBitsPerSample / SampleDivisor;
				pTempSampleLoop++; // Next TempSampleLoop structure.
			}	
		}		
			
		// Now shuffle back all data after wave data by SampleDataSize/2 (+ padding) bytes 
		// INT SampleShrinkage = ( SampleDataSize/4) * 2;
		BYTE* NewWaveDataEnd = SampleDataEnd - ChunkShrinkage;

		for ( INT pt = 0; pt< ( WaveDataEnd -  SampleDataEnd); pt++ )
		{ 
			NewWaveDataEnd[pt] =  SampleDataEnd[pt];
		}			

		// Resize the dynamic array.
		WavData.Remove( WavData.Num() - ChunkShrinkage, ChunkShrinkage );
		
		/*
		static INT SavedBytes = 0;
		SavedBytes += ChunkShrinkage;
		debugf(NAME_Log," Audio shrunk by: %i bytes, total savings %i bytes.",ChunkShrinkage,SavedBytes);
		debugf(NAME_Log," New BitsPerSample: %i ", *pBitsPerSample);
		debugf(NAME_Log," New SamplesPerSec: %i ", *pSamplesPerSec);
		debugf(NAME_Log," Olddata/NEW*wav* sizes: %i %i ", SampleDataSize, *pMasterSize);
		*/
	}

	// Noise gate filtering assumes 8-bit sound.
	// Warning: While very useful on SOME sounds, it erased too many low-volume sound fx
	// in its current form - even when 'noise' level scaled to average sound amplitude.
	// if (NoiseGate) NoiseGateFilter();

	return 1;
	unguard;
}

//
// Reduce bit depth and halve the number of samples simultaneously.
//
void FWaveModInfo::HalveReduce16to8()
{
	guard(FWaveModInfo::HalveReduce16to8);

	DWORD SampleWords =  SampleDataSize >> 1;
	INT OrigValue,NewValue;
	INT ErrorDiff = 0;

	DWORD SampleBytes = SampleWords >> 1;

	INT NextSample0 = (INT)((SWORD*) SampleDataStart)[0];
	INT NextSample1,NextSample2;

	BYTE* SampleData =  SampleDataStart;
	for (DWORD T=0; T<SampleBytes; T++)
	{
		NextSample1 = (INT)((SWORD*)SampleData)[T*2];
		NextSample2 = (INT)((SWORD*)SampleData)[T*2+1];
		INT Filtered16BitSample = 32768*4 + NextSample0 + NextSample1 + NextSample1 + NextSample2;
		NextSample0 = NextSample2;

		// Error diffusion works in '18 bit' resolution.
		OrigValue = ErrorDiff + Filtered16BitSample;
		// Rounding: "+0.5"
		NewValue  = (OrigValue + 512) & 0xfffffc00;
		if (NewValue > 0x3fc00) NewValue = 0x3fc00;

		INT NewSample = NewValue >> (8+2);
		SampleData[T] = (BYTE)NewSample;	// Output byte.

		ErrorDiff = OrigValue - NewValue;   // Error cycles back into input.
	}

	NewDataSize = SampleBytes;

	*pBitsPerSample  = 8;
	*pSamplesPerSec  = *pSamplesPerSec >> 1;

	NoiseGate = true;
	unguard;
}

//
// Reduce bit depth.
//
void FWaveModInfo::Reduce16to8()
{
	guard(FWaveModInfo::Reduce16to8);

	DWORD SampleBytes =  SampleDataSize >> 1;
	INT OrigValue,NewValue;
	INT ErrorDiff = 0;
	BYTE* SampleData = SampleDataStart;

	for (DWORD T=0; T<SampleBytes; T++)
	{
		// Error diffusion works in 16 bit resolution.
		OrigValue = ErrorDiff + 32768 + (INT)((SWORD*)SampleData)[T];
		// Rounding: '+0.5', then mask off low 2 bits.
		NewValue  = (OrigValue + 127 ) & 0xffffff00;  // + 128
		if (NewValue > 0xff00) NewValue = 0xff00;

		INT NewSample = NewValue >> 8;
		SampleData[T] = NewSample;

		ErrorDiff = OrigValue - NewValue;  // Error cycles back into input.
	}				

	NewDataSize = SampleBytes;
	*pBitsPerSample  = 8;
	NoiseGate = true;

	unguard;
}

//
// Halve the number of samples.
//
void FWaveModInfo::HalveData()
{
	guard(FWaveModInfo::HalveData);
	if( *pBitsPerSample == 16)
	{						
		DWORD SampleWords =  SampleDataSize >> 1;
		INT OrigValue,NewValue;
		INT ErrorDiff = 0;

		DWORD ScaledSampleWords = SampleWords >> 1; 

		INT NextSample0 = (INT)((SWORD*) SampleDataStart)[0];
		INT NextSample1, NextSample2;

		BYTE* SampleData =  SampleDataStart;
		for (DWORD T=0; T<ScaledSampleWords; T++)
		{
			NextSample1 = (INT)((SWORD*)SampleData)[T*2];
			NextSample2 = (INT)((SWORD*)SampleData)[T*2+1];
			INT Filtered18BitSample = 32768*4 + NextSample0 + NextSample1 + NextSample1 + NextSample2;
			NextSample0 = NextSample2;

			// Error diffusion works with '18 bit' resolution.
			OrigValue = ErrorDiff + Filtered18BitSample;
			// Rounding: '+0.5', then mask off low 2 bits.
			NewValue  = (OrigValue + 2) & 0x3fffc;
			if (NewValue > 0x3fffc) NewValue = 0x3fffc;
			((SWORD*)SampleData)[T] = (NewValue >> 2) - 32768;  // Output SWORD.
			ErrorDiff = OrigValue - NewValue; // Error cycles back into input.
		}				
		NewDataSize = (ScaledSampleWords * 2);
		*pSamplesPerSec  = *pSamplesPerSec >> 1;
	}
	else if( *pBitsPerSample == 8 )
	{									
		INT OrigValue,NewValue;
		INT ErrorDiff = 0;
	
		DWORD SampleBytes = SampleDataSize >> 1;  
		BYTE* SampleData = SampleDataStart;

		INT NextSample0 = SampleData[0];
		INT NextSample1, NextSample2;

		for (DWORD T=0; T<SampleBytes; T++)
		{
			NextSample1 =  SampleData[T*2];
			NextSample2 =  SampleData[T*2+1];
			INT Filtered10BitSample =  NextSample0 + NextSample1 + NextSample1 + NextSample2;
			NextSample0 =  NextSample2;

			// Error diffusion works with '10 bit' resolution.
			OrigValue = ErrorDiff + Filtered10BitSample;
			// Rounding: '+0.5', then mask off low 2 bits.
			NewValue  = (OrigValue + 2) & 0x3fc;
			if (NewValue > 0x3fc) NewValue = 0x3fc;
			SampleData[T] = (BYTE)(NewValue >> 2);	// Output BYTE.
			ErrorDiff = OrigValue - NewValue;		// Error cycles back into input.
		}				
		NewDataSize = SampleBytes; 
		*pSamplesPerSec  = *pSamplesPerSec >> 1;
	}
	unguard;
}

//
//	Noise gate filter. Hard to make general-purpose without hacking up some (soft) sounds.
//
void FWaveModInfo::NoiseGateFilter()
{
	guard(FWaveModInfo::NoiseGateFilter);

	BYTE* SampleData  =  SampleDataStart;
	INT   SampleBytes = *pWaveDataSize; 
	INT	  MinBlankSize = 860 * ((*pSamplesPerSec)/11025); // 600-800...

	// Threshold sound amplitude. About 18 seems OK.
	INT Amplitude	 = 18;

	// Ignore any over-threshold signals if under this size. ( 32 ?)
	INT GlitchSize	 = 32 * ((*pSamplesPerSec)/11025);
	INT StartSilence =  0;
	INT EndSilence	 =  0;
	INT LastErased   = -1;

	for( INT T = 0; T< SampleBytes; T++ )
	{
		UBOOL Loud;
		if	( Abs(SampleData[T]-128) >= Amplitude )
		{
			Loud = true;							
			if (StartSilence > 0)
			{
				if ( (T-StartSilence) < GlitchSize ) Loud = false;
			}							
		}
		else Loud = false;

		if( StartSilence == 0 )
		{
			if( !Loud )
				StartSilence = T;
		}
		else
		{													
			if( ((EndSilence == 0) && (Loud)) || (T ==(SampleBytes-1) ) )
			{
				EndSilence = T;
				//
				// Erase an area of low-amplitude sound ( noise... ) if size >= MinBlankSize.
				//
				// Todo: try erasing smoothly - decay, create some attack, 
				// proportional to the size of the area. ?
				//
				if	(( EndSilence - StartSilence) >= MinBlankSize )
				{					
					for ( INT Er = StartSilence; Er< EndSilence; Er++ )
					{
						SampleData[Er] = 128;
					}
				}
				LastErased = EndSilence-1;
				StartSilence = 0;
				EndSilence   = 0;
			}
		}										
	}
	unguard;
}

/*-----------------------------------------------------------------------------
	UAudioSubsystem implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UAudioSubsystem);


/*-----------------------------------------------------------------------------
	UI3DL2Listener implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UI3DL2Listener);
void UI3DL2Listener::PostEditChange()
{
	Super::PostEditChange();
	Updated = true;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

