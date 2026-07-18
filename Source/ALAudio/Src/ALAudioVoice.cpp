/*=============================================================================
	ALAudioVoice.cpp: Voice encoding/ decoding module.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Daniel Vogel.
=============================================================================*/

#include "ALAudioPrivate.h"

//
//	FALVoiceModule::FALVoiceModule.
//
FALVoiceModule::FALVoiceModule( UALAudioSubsystem* InAudioSubsystem, FAudioCaptureDevice* InCaptureDevice )
{
	guard(FALVoiceModule::FALVoiceModule);
	
	// Store audio subsystem and capture device.
	AudioSubsystem	= InAudioSubsystem;
	CaptureDevice	= InCaptureDevice;

	// Initialize audio capture relevant data.
	CaptureOffset	= CaptureDevice ? CaptureDevice->GetCurrentOffset() : 0;
	CapturingVoice	= 0;
	VADActive		= 0;

	// Initialze codec.
	VoiceCodec		= CODEC_None;

	// Initialize speex data structures used by both decoder and encoder.
	SpeexDecoders.Empty();
	SpeexData = new SpeexBits;
	speex_bits_init( SpeexData ); 

	// Initialize encoder state.
	SpeexEncoder		= NULL;
	SpeexPreprocessor	= NULL;

	unguard;
}

//
//	FALVoiceModule::~FALVoiceModule.
//
FALVoiceModule::~FALVoiceModule()
{
	guard(FALVoiceModule::~FALVoiceModule);
	
	// Free speex data structures.
	if( SpeexEncoder )
		speex_encoder_destroy( SpeexEncoder );
	if( SpeexPreprocessor )
		speex_preprocess_state_destroy( SpeexPreprocessor );
	if( SpeexData )
		speex_bits_destroy( SpeexData );
	for( TMap<USound*,void*>::TIterator It(SpeexDecoders); It; ++It )
		speex_decoder_destroy( It.Value() );

	delete SpeexData;

	SpeexEncoder		= NULL;
	SpeexPreprocessor	= NULL;
	SpeexData			= NULL;

	SpeexDecoders.Empty();

	AudioSubsystem		= NULL;
	CaptureDevice		= NULL;

	unguard;
}

//
//	FALVoiceModule::SetEncoder.
//
void FALVoiceModule::SetEncoder( EVoiceCodec InVoiceCodec )
{
	guard(FALVoiceModule::SetEncoder);

	// Tear down existing encoder/ denoiser.
	if( SpeexEncoder )
		speex_encoder_destroy( SpeexEncoder );
	if( SpeexPreprocessor )
		speex_preprocess_state_destroy( SpeexPreprocessor );
	SpeexEncoder		= NULL;
	SpeexPreprocessor	= NULL;

	VoiceCodec = InVoiceCodec;

	INT FrameSize,
		SamplingRate,
		BitRate;

	// Initialize speex encoder/ denoiser based on codec.
	switch( VoiceCodec )
	{
	case CODEC_48NB:
		SpeexEncoder	= speex_encoder_init(&speex_nb_48k_mode);
		SamplingRate	= 8000;
		BitRate			= 4800;
		break;
	case CODEC_96WB:
		SpeexEncoder	= speex_encoder_init(&speex_wb_mode); 
		SamplingRate	= 16000;
		BitRate			= 9600;
		break;
	default:
		BitRate			= 0;
		appErrorf(TEXT("Unknown voice codec %i"), VoiceCodec );
	}

	// Set encoder environment and verify that speex & engine have matching expectations wrt to frame size, bitrate, ...

	if( BitRate == 9600 )
	{
		// 9.6kbps mode needs some special attention.
		INT Mode			= 3,
			SubModeEncoding	= 0;

		speex_encoder_ctl( SpeexEncoder, SPEEX_SET_MODE				, &Mode				);
		speex_encoder_ctl( SpeexEncoder, SPEEX_SET_SUBMODE_ENCODING	, &SubModeEncoding	);
	}
	else
		speex_encoder_ctl( SpeexEncoder, SPEEX_SET_BITRATE			, &BitRate			);

	speex_encoder_ctl( SpeexEncoder, SPEEX_SET_SAMPLING_RATE		, &SamplingRate		);
	speex_encoder_ctl( SpeexEncoder, SPEEX_GET_FRAME_SIZE			, &FrameSize		); 

	check( FrameSize == UAudioSubsystem::CodecFrameSize[CODEC_TO_INDEX(VoiceCodec)] );

	// Initialize speex preprocessor.

	SpeexPreprocessor = speex_preprocess_state_init(FrameSize,SamplingRate);

	INT		Enable		= 1;
	FLOAT	AGCLevel	= 18000.f;

	speex_preprocess_ctl( SpeexPreprocessor, SPEEX_PREPROCESS_SET_DENOISE	, &Enable	);
	speex_preprocess_ctl( SpeexPreprocessor, SPEEX_PREPROCESS_SET_AGC		, &Enable	);
	speex_preprocess_ctl( SpeexPreprocessor, SPEEX_PREPROCESS_SET_VAD		, &Enable	);
	speex_preprocess_ctl( SpeexPreprocessor, SPEEX_PREPROCESS_SET_AGC_LEVEL	, &AGCLevel );

	// Initialize encoder.

	FLOAT*	Src		= new FLOAT[FrameSize];
	char*	Dst		= new char[FrameSize];		// frame size >> packet size
	appMemzero(Src,FrameSize * sizeof(FLOAT));	// don't send NaNs ;)

	speex_bits_reset( SpeexData );
	speex_encode( SpeexEncoder, Src, SpeexData );
	INT PacketSize = speex_bits_write( SpeexData, Dst, FrameSize );

	delete [] Src;
	delete [] Dst;
	check( PacketSize == UAudioSubsystem::CodecPacketSize[CODEC_TO_INDEX(VoiceCodec)]);

	unguard;
}

//
//	FALVoiceModule::Encode.
//
INT FALVoiceModule::Encode()
{
	guard(FALVoiceModule::Encode);

	// Nothing to do if we don't have a capture device.
	if( !CaptureDevice )
		return 0;

	UViewport* Viewport = AudioSubsystem->Viewport;

	if( Viewport->Actor->bVoiceTalk && !CapturingVoice )
		CaptureOffset = CaptureDevice->GetCurrentOffset();

	CapturingVoice = Viewport->Actor->bVoiceTalk;

	// Try to find voice channel and create one if necessary.
	UNetConnection*	ServerConnection	= Viewport->Actor->XLevel->NetDriver ? Viewport->Actor->XLevel->NetDriver->ServerConnection : NULL;
	UVoiceChannel*	VoiceChannel		= NULL;
	FVoiceInfo*		VoiceInfo			= NULL;

	if(	(CapturingVoice || VADActive || AudioSubsystem->EnhancedDenoiser) && ServerConnection && Viewport->Actor->PlayerReplicationInfo && Viewport->Actor->PlayerReplicationInfo->PlayerID )
	{
		VoiceChannel = ServerConnection->VoiceChannel;

		if( VoiceChannel )
		{
			check( VoiceChannel->VoiceIndex < VOICEPACKET_Max ); //!!VOIP TODO
			VoiceInfo = ServerConnection->Driver->VoiceInfos[VoiceChannel->VoiceIndex];
		}

		if( VoiceChannel && VoiceInfo )
		{
			if( VoiceCodec == CODEC_None )
				SetEncoder( VoiceInfo->VoiceCodec );

			if( VoiceInfo->VoiceCodec != VoiceCodec || !VoiceChannel->IsNetReady(0) )
				VoiceChannel = NULL;
		}
	}

	INT EncodedPackets = 0;

	if( VoiceChannel && VoiceInfo )
	{
		// Codec frame size in samples.
		INT FrameSize = UAudioSubsystem::CodecFrameSize[CODEC_TO_INDEX(VoiceCodec)];

		// Codec packet size in bytes.
		INT PacketSize = UAudioSubsystem::CodecPacketSize[CODEC_TO_INDEX(VoiceCodec)];

		// Retrieve amount of avaible samples to be locked.
		EAudioCaptureType CaptureType = UAudioSubsystem::CodecFrequency[CODEC_TO_INDEX(VoiceCodec)] == 8000 ? CAPTURE_8KHZ_FLOAT : CAPTURE_16KHZ_FLOAT;

		DWORD AvailableSamples = CaptureDevice->GetAvailableSamples( CaptureOffset, CaptureType );

		// Read size granularity == number of samples in uncompressed package.
		AvailableSamples -= AvailableSamples % FrameSize;

		// Compress the voice and send it off to the server.
		if( AvailableSamples )
		{
			// Src data (Speex expects floats which CaptureDevice will return.
			FLOAT*	CaptureData = new FLOAT[AvailableSamples];

			// Retrieve sample data, also calculates maximum gain in range 0..1 and advances capture offset.
			DWORD	SamplesRead	= AvailableSamples;
			FLOAT	MaximumGain	= CaptureDevice->GetSampleData( CaptureOffset, CaptureData, SamplesRead, CaptureType );
			check( AvailableSamples == SamplesRead );

			INT VoiceBandwidth = 0;
			for( DWORD PacketIndex=0; PacketIndex<(SamplesRead / FrameSize); PacketIndex++ )
			{	
				FLOAT* Src = CaptureData + PacketIndex * FrameSize;

#if SAVE_SOUNDS
				INT Index1 = AudioSubsystem->RawSoundBeforeDenoiser.Add( FrameSize * sizeof(SHORT) ),
					Index2 = AudioSubsystem->RawSoundAfterDenoiser.Add( FrameSize * sizeof(SHORT) );
				check(Index1==Index2);
				
				SHORT	*Before	= (SHORT*) &AudioSubsystem->RawSoundBeforeDenoiser(Index1),
						*After	= (SHORT*) &AudioSubsystem->RawSoundAfterDenoiser(Index2);

				for(INT i=0; i<FrameSize; i++)
					*(Before++) = Src[i];
#endif
				// Denoise.
				UBOOL IsSpeech = 0;
	
				if( CapturingVoice || VADActive )
					IsSpeech = speex_preprocess( SpeexPreprocessor, Src, NULL );
				else
					speex_preprocess_estimate_update( SpeexPreprocessor, Src, NULL );

#if SAVE_SOUNDS
				for(INT i=0; i<FrameSize; i++)
					*(After++) = Src[i];
#endif

				// Server has option to disallow VAD.
				if( (VoiceInfo->ServerOptions & VOICE_AllowVAD) && AudioSubsystem->UseVAD )
					VADActive	= 1;
				else
					VADActive	= 0;
	
				// Only compress & send if we're capturing voice or packet contains voice.
				if( CapturingVoice || (IsSpeech && VADActive) )
				{
					// Pass average gain stats to HUD.
					if( AudioSubsystem->Viewport->Actor->myHUD )
					{
						AudioSubsystem->Viewport->Actor->myHUD->LastVoiceGain		= appSqrt( MaximumGain );
						AudioSubsystem->Viewport->Actor->myHUD->LastVoiceGainTime	= AudioSubsystem->Viewport->Actor->Level->TimeSeconds;
					}

					// Compress.
					speex_bits_reset( SpeexData );
					speex_encode( SpeexEncoder, Src, SpeexData );

					// Create voice packet.
					check( VoiceInfo->PacketData );
					check( VoiceInfo->PacketSize == PacketSize );
					verify( speex_bits_write( SpeexData, (char*) VoiceInfo->PacketData, PacketSize ) == PacketSize );

					// Send voice packet to server.
					VoiceBandwidth = 600;
					VoiceChannel->SendVoicePacket( VoiceInfo );

					EncodedPackets++;
				}
			}

			// Delete temporary data.
			delete [] CaptureData;

			// So high level code can make correct predictions about bandwidth usage.
			ServerConnection->CurrentVoiceBandwidth = VoiceBandwidth;
		}
	}

	return EncodedPackets;
	
	unguard;
}

//
//	FALVoiceModule::Decode.
//
UBOOL FALVoiceModule::Decode( FVoiceInfo* VoiceInfo )
{
	guard(FALVoiceModule::Decode);

	// Create speex decoder if we can't find an existing one tied to this sound.
	USound* VoiceSound		= VoiceInfo->VoiceSound;
	INT		PlayerID		= VoiceInfo->PlayerID;
	BYTE*	PacketData		= VoiceInfo->PacketData;
	void*	SpeexDecoder	= SpeexDecoders.FindRef( VoiceSound );

	check(VoiceSound);

	if( PacketData && !SpeexDecoder )
	{
		INT Mode			= 3,
			SubModeEncoding	= 0,
			PercpEnh		= 1;

		// Create decoder and add it to tmap.	
		switch( VoiceSound->VoiceCodec )
		{
		case CODEC_48NB:
			SpeexDecoder = speex_decoder_init(&speex_nb_48k_mode); 
			break;
		case CODEC_96WB:
			SpeexDecoder = speex_decoder_init(&speex_wb_mode); 
			speex_decoder_ctl( SpeexDecoder, SPEEX_SET_MODE				, &Mode				);
			speex_decoder_ctl( SpeexDecoder, SPEEX_SET_SUBMODE_ENCODING	, &SubModeEncoding	);
			break;
		default:
			appErrorf(TEXT("Unknown voice codec %i"), VoiceSound->VoiceCodec );
		}
		SpeexDecoders.Set( VoiceSound, SpeexDecoder );

		// Enable perceptual enhancement.
		speex_decoder_ctl( SpeexDecoder, SPEEX_SET_ENH, &PercpEnh );
	}
	else if( !PacketData && SpeexDecoder )
	{
		// Destroy deocder and remove it from tmap.
		speex_decoder_destroy( SpeexDecoder );
		SpeexDecoders.Remove( VoiceSound );
	}

	FALSoundSource* Source = NULL;
	// Initialize sound if being used for the first time.
	if( !VoiceSound->GetHandle() )
		FALSoundBuffer::Init( VoiceSound, AudioSubsystem );

	// Find source voice is playing.
	for( INT i=0; i<AudioSubsystem->Sources.Num(); i++ )
	{
		if( AudioSubsystem->Sources(i)->Sound == VoiceSound )
		{
			Source = AudioSubsystem->Sources(i);
			break;
		}
	}

	APawn* VoicePawn = NULL;
	// Server can disable voice spatialization. Find voice pawn and re-associate with source if necessary.
	if( AudioSubsystem->UseSpatializedVoice && (VoiceInfo->ServerOptions & VOICE_AllowSpatialization) && (!Source || !Source->Actor) && PacketData ) 
	{
		// Try whether we already have a bound reference (won't be stale as NoteDestroy removes deleted actors).
		VoicePawn = PlayerIDToPawn.FindRef( PlayerID );

		// We failed so let's try to locate a pawn with matching Player ID.
		if( !VoicePawn && AudioSubsystem->Viewport )
		{
			ULevel* Level = AudioSubsystem->Viewport->Actor->GetLevel();

			// Iterate over all dynamic actors.
			for( INT iActor=Level->iFirstDynamicActor; iActor<Level->Actors.Num(); iActor++ )
			{
				// Check whether actor is pawn, has a player replication info and a mathching player id.
				APawn* Pawn = Cast<APawn>(Level->Actors(iActor));
				if( Pawn )
				{	
					if( Pawn->PlayerReplicationInfo && (Pawn->PlayerReplicationInfo->PlayerID == PlayerID) && !Pawn->IsPendingKill() )
					{
						// Found other pawn talking, add it to tmap.
						PlayerIDToPawn.Set( PlayerID, Pawn );
						VoicePawn = Pawn;
						break;
					}
				}
			}
		}

		// Propagate to source in case Pawn became irrelevant and relevant again.
		if( VoicePawn && Source )
			Source->Actor = VoicePawn;
	}

	// Start sound if it's neither playing nor queued.
	if( !Source && PacketData )
	{
		// Finally start/ play the sound if it's not already queued (e.g. multiple packets per frame).
		if( !AudioSubsystem->UAudioSubsystem::IsQueued(VoiceSound) )
		{
			FVector Location	= FVector(0,0,0);			
			INT		Flags		= VoiceSound->Flags;

			// Play with spatialization is allowed & enabled. Large radius will take care of attenuation.
			if( AudioSubsystem->UseSpatializedVoice && (VoiceInfo->ServerOptions & VOICE_AllowSpatialization) )
			{
				// Set correct location - sound system automatically updates source location if Actor is NULL and Flags & SF_Voice.
				if( VoicePawn )
					Location = VoicePawn->Location;
				else
					Location = AudioSubsystem->Listener->Location;

				Flags &= ~SF_No3D;
			}

//			FLOAT Radius = Clamp( AudioSubsystem->SpatializedVoiceRadius, 10.f, 1000000.f );
			FLOAT Radius = Clamp( AudioSubsystem->Viewport->Actor->VoiceReplicationInfo ?
				AudioSubsystem->Viewport->Actor->VoiceReplicationInfo->BroadcastRadius : AudioSubsystem->SpatializedVoiceRadius,
				10.f, 1000000.f );
			AudioSubsystem->UAudioSubsystem::PlaySound( VoicePawn, SLOT_None, VoiceSound, Location, 1.f, Radius, 1.f, Flags, 0 );
	
			// Reset decoder to avoid popping.
			speex_decoder_ctl( SpeexDecoder, SPEEX_RESET_STATE, NULL );
		}

		// One frame latency as PlaySound queues (or to sound more American, lines up) to be played sounds.
		return 0;
	}

	// NULL packet means that chatter has been removed -> stop Source (if it has already been started).
	if( !PacketData )
	{
		if( Source )
			Source->Stop();
		return 0;
	}

	guard(PlayBack);
	// Retrieve Source status information.
	ALint	BuffersProcessed,
			BuffersQueued,
			BuffersPending,
			SourceState;

	check(Source);

	UALAudioSubsystem::alGetSourcei( Source->SourceId, AL_SOURCE_STATE		, &SourceState		);
	UALAudioSubsystem::alGetSourcei( Source->SourceId, AL_BUFFERS_PROCESSED	, &BuffersProcessed	);
	UALAudioSubsystem::alGetSourcei( Source->SourceId, AL_BUFFERS_QUEUED	, &BuffersQueued	);
	BuffersPending = BuffersQueued - BuffersProcessed;

	// Queue incoming packet, space permitting.
	if( BuffersProcessed || BuffersQueued < NUM_VOICE_BUFFERS )
	{
		check(Source->Buffer);
		check(VoiceSound);
		check(PacketData);
		check(SpeexData);
		check(SpeexDecoder);
		check(VoiceSound->VoiceCodec == CODEC_48NB || VoiceSound->VoiceCodec == CODEC_96WB);

		INT		FrameSize			= UAudioSubsystem::CodecFrameSize[CODEC_TO_INDEX(VoiceSound->VoiceCodec)];
		FLOAT*	FDest				= new FLOAT[FrameSize];
		SHORT*	SDest				= new SHORT[FrameSize];
		INT&	BufferQueueIndex	= Source->Buffer->BufferQueueIndex;
		INT&	BufferUnqueueIndex	= Source->Buffer->BufferUnqueueIndex;

		// Decode.
		guard(Decode);
		speex_bits_read_from( SpeexData, (char*) PacketData, FrameSize );
		speex_decode( SpeexDecoder, SpeexData, FDest );
		unguard;

		// Float to int.
		for( INT i=0; i<FrameSize; i++ )
			SDest[i] = Clamp( FDest[i], -32767.f, 32767.f );

		// Unqueue buffers as necessary.
		guard(UnqueueBuffers);
		while( BuffersProcessed-- )
		{
			UALAudioSubsystem::alSourceUnqueueBuffers( Source->SourceId, 1, &Source->Buffer->BufferIds(BufferUnqueueIndex) );
			++BufferUnqueueIndex %= Source->Buffer->BufferIds.Num();	
		}
		unguard;

		// Queue data (note that queue index should always point to an already processed (and hence unqueued) buffer).		
		guard(QueueBuffers);
		UALAudioSubsystem::alBufferData( Source->Buffer->BufferIds(BufferQueueIndex), AL_FORMAT_MONO16, SDest, FrameSize * sizeof(SHORT), UAudioSubsystem::CodecFrequency[CODEC_TO_INDEX(VoiceSound->VoiceCodec)] );
		UALAudioSubsystem::alSourceQueueBuffers( Source->SourceId, 1, &Source->Buffer->BufferIds(BufferQueueIndex) );
		++BufferQueueIndex %= Source->Buffer->BufferIds.Num();
		unguard;

		// Start playing (again) now that there is something to play.
		guard(PlaySource);
		if( SourceState == AL_STOPPED && (BuffersPending > 3) )
			Source->Play();
		unguard;

		delete [] FDest;
		delete [] SDest;
		return 1;
	}
	else
	{
		// Start playing again so buffers can become processed.
		guard(PlayStoppedSource);
		if( SourceState == AL_STOPPED )
			Source->Play();
		unguard;
		return 0;
	}
	unguard;

	unguard;
}

//
//	FALVoiceModule::NoteDestroy.
//
void FALVoiceModule::NoteDestroy( AActor* Actor )
{
	guard(FALVoiceModule::NoteDestroy);
	
	// Sadly there is no TMap->RemoveValue.
	for( TMap<INT,APawn*>::TIterator It(PlayerIDToPawn); It; ++It )
		if( It.Value() == Actor )
			It.RemoveCurrent();
	
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

