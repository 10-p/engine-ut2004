/*=============================================================================
	UnAudio.h: Unreal base audio.
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
		* Wave modification code by Erik de Neve
		* Taken over by Daniel Vogel
=============================================================================*/

#ifdef PlaySound
#undef PlaySound
#endif

class USound;
class FSoundSource;
class UStreamingSound;
class UAudioSubsystem;
class FListener;
struct FVoicePacket;
struct FVoiceInfo;
class FAudioCaptureDevice;

/*-----------------------------------------------------------------------------
	Special Sony VAG Includes.
-----------------------------------------------------------------------------*/

#define HAVE_VAG 0 
#if HAVE_VAG
	#include "ENCVAG.h"
#endif

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

/*-----------------------------------------------------------------------------
	UAudioSubsystem.
-----------------------------------------------------------------------------*/

enum ESoundFlags
{
	SF_None				= 0,
	SF_Reserved			= 1,
	SF_Looping			= 2,
	SF_Streaming		= 4,
	SF_Music			= 8,
	SF_No3D				= 16,
	SF_UpdatePitch		= 32,
	SF_NoUpdates		= 64,
	SF_RootMotion		= 128,
	SF_Voice			= 256,
	SF_MusicVolume		= 512,

	SF_Queueing			= SF_Looping | SF_Streaming | SF_Music | SF_Voice,
};

enum EFadeMode
{
	FADE_None			= 0,
	FADE_In,
	FADE_Out
};

#define AUDIO_OCCLUSION_FACTOR 0.35f
//!!WARNING: distance factor needs adjustment for Warfare.
#define AUDIO_DISTANCE_FACTOR ( 0.01875f )
//!! 100m/sec is max speed
#define AUDIO_MAX_SOUND_SPEED 100.f

// Codec to index
#define CODEC_TO_INDEX(x) appCeilLogTwo(x)

// Below is a bitflag so don't add e.g. 3
enum EVoiceCodec
{
	CODEC_None					= 0,
	CODEC_48NB					= 1,
	CODEC_96WB					= 2,
};

// Below is a bitflag so don't add e.g. 3
enum EVoiceServerOptions
{
	VOICE_None					= 0,
	VOICE_AllowVAD				= 1,
	VOICE_AllowSpatialization	= 2,
};

// Stream types low level audio supports.
enum EAudioStreamType
{
	ST_Ogg,
	ST_OggLooping,
	ST_MP3,
	ST_MP3Looping
};

//
// UAudioSubsystem is the base class of the game's audio subsystems.
//
class ENGINE_API UAudioSubsystem : public USubsystem
{
	DECLARE_CLASS(UAudioSubsystem,USubsystem,CLASS_Config,Engine)

	// Constructor
	UAudioSubsystem(){}
	void StaticConstructor();
	
	// Initialization.
	virtual UBOOL Init();
	virtual void SetViewport( UViewport* Viewport );
	virtual void Flush(){}

	// Exec.
	virtual UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog ){ return false;}
	
	// Update.
	virtual UBOOL Update( FSceneNode* SceneNode );

	// Sound playback.
	void PlaySound( AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch, INT Flags, FLOAT FadeTime );
	virtual void PlaySound( FSoundSource* SoundSource ){}

#ifdef WITH_LIPSINC
	virtual DOUBLE OffsetInSound( AActor* Actor, USound* Sound ) { return 0; }
	virtual UBOOL PauseSound( AActor* Actor, USound* Sound ) { return 0; }
	virtual UBOOL ResumeSound( AActor* Actor, USound* Sound ) { return 0; }
	virtual UBOOL StopSound( AActor* Actor, USound* Sound ) { return 0; }
#endif

	// VoIP handling.
	virtual UBOOL PlayVoicePacket( FVoiceInfo* VoiceInfo ){ return 0; }

	// Notifications.
	virtual void PostEditChange();
	virtual void NoteDestroy( AActor* Actor );
	virtual void NoteDestroy( USound* Sound );

	// Helper functions.
	virtual UBOOL FindSource();
	virtual void SetTransientSource( FSoundSource* Source ){}

	// Query functions.
	UViewport* GetViewport() { return Viewport; }
	FListener* GetListener() { return Listener; }
	virtual FSoundSource* GetSource( INT SourceIndex ){ return NULL; }
	virtual FSoundSource* GetTransientSource(){ return NULL; }
	virtual INT GetNumSources(){ return 0; }

	// Internal functions.
	UBOOL IsQueued( USound* Sound );

	// Backward compatibility baggage.

	// Query functions.
	virtual UBOOL LowQualitySound(){ return 0; }

	// Music playback.
	INT PlayStream( FString Filename, UBOOL UseMusicVolume, FLOAT InVolume, FLOAT FadeInTime, FLOAT SeekTime );
	virtual INT SeekStream( INT Handle, FLOAT Time ){ return 0; }
	INT PlayMusic( FString Song, FLOAT FadeInTime );
	void AllowMusicPlayback( UBOOL Allow );

	// rjp --
	UBOOL SetVolume(   INT SongHandle,  FLOAT NewVolume );
	UBOOL PauseStream( INT SongHandle );
	UBOOL IsPaused(    INT SongHandle );
	// -- rjp

	// Stopping power.
	void StopStream( INT SongHandle, FLOAT FadeOutTime );
	void StopAllMusic( FLOAT FadeOutTime );
	void StopAllSounds();

	// Shared thread safe audio capture device.
	static FAudioCaptureDevice* CaptureDevice;

	// Lookup tables for codec/ packet size & time, frame size and frequency.
	static INT CodecPacketSize[32];
	static INT CodecPacketTime[32];
	static INT CodecFrameSize[32];
	static INT CodecFrequency[32];

protected:
	// Variables.
	FListener*	Listener;
	UViewport*	Viewport;
	AActor*		LastViewTarget;

	DOUBLE		LastUpdateTime;
	FLOAT		DeltaTime;
	UBOOL		LastRealtime,
				AllowMusic;
	FString		PendingSong;

	TArray<FSoundSource*> QueuedSources;

	// Configuration.
	FLOAT		AmbientVolume,
				SoundVolume,
				VoiceVolume,
				MusicVolume,
				VolumeScaleRec;
	UBOOL		UseVoIP;

	// Stats.
	class FAudioStats
	{
	public:

		INT		STATS_FirstEntry,
				STATS_UpdateCycles,
				STATS_OcclusionCycles,
				STATS_ActiveSources,
				STATS_StoppedSources,
				STATS_StartedSources,
				STATS_LastEntry;
		FAudioStats();
		void Init();
	} AudioStats;

	friend class FSoundSource;
};


/*-----------------------------------------------------------------------------
	FListener.
-----------------------------------------------------------------------------*/

class ENGINE_API FListener
{
public:
	FListener();

	virtual void Update( FVector InUp, FVector InRight, FVector InFront, FVector InLocation, FPointRegion InRegion, FLOAT DeltaTime, UI3DL2Listener* EAXEffect );
	virtual void Reset();

	FVector			Location,
					LastLocation,
					Velocity,
					Up,
					Right,
					Front;
	FPointRegion	Region;
	UI3DL2Listener*	EAXEffect;
	UI3DL2Listener*	LastEAXEffect;
	UBOOL			Initialized;
};


/*-----------------------------------------------------------------------------
	FSoundSource.
-----------------------------------------------------------------------------*/

class ENGINE_API FSoundSource
{
public:
	FSoundSource( UAudioSubsystem* AudioSubsystem );

	virtual void Init( AActor* Actor, INT Id, USound* Sound, FVector Location, FLOAT Volume, FLOAT Radius, FLOAT Pitch, INT Flags, FLOAT FadeDuration );
	virtual	UBOOL Update( FLOAT DeltaTime ) { return 0; }
	UBOOL UpdateBase( FLOAT DeltaTime );

	virtual void UpdateRadii();
	virtual void UpdatePriority();
	
	virtual void Play(){}
	virtual void Stop(){}
	virtual void Pause(){}

	virtual UBOOL IsPlaying() { return Playing; }
	virtual UBOOL IsPaused() { return 0; }
	virtual UBOOL IsStopped() { return 1; }
	virtual UBOOL IsFinished( FLOAT DeltaTime ) { return 1; }

	// Status information.
	UBOOL		Initialized,
				Playing;
	FLOAT		TimeSinceLastUpdate;

	// Source properties retrieved from parameters.
	AActor*		Actor;
	INT			Id;
	USound*		Sound;
	FVector		Location;
	FLOAT		Volume;
	FLOAT		Radius;
	FLOAT		Pitch;
	INT			Flags;
	FLOAT		FadeDuration;

	// Cached source properties.
	FVector		Velocity;
	FLOAT		Priority;
	FLOAT		ZoneRadius;
	FLOAT		UsedRadius;
	FLOAT		WantedRadius;
	FLOAT		UsedVolume;

	FLOAT		InactiveTime;
	FLOAT		FadeTime;
	EFadeMode	FadeMode;

#ifdef WITH_LIPSINC
	DOUBLE		StartTime;
	DOUBLE      PauseStartTime;
	DOUBLE      TotalPauseTime;
#endif

protected:
	// Sound system source is attached to.
	UAudioSubsystem* AudioSubsystem;
};


/*-----------------------------------------------------------------------------
	FAudioCaptureDevice.
-----------------------------------------------------------------------------*/

// WARNING: FAudioCaptureDevice has to be thread safe!

enum EAudioCaptureType
{
	CAPTURE_8KHZ_FLOAT,
	CAPTURE_16KHZ_FLOAT,
	CAPTURE_16KHZ_SHORT,
};

class FAudioCaptureDevice
{
public:
	virtual ~FAudioCaptureDevice() {}

	virtual UBOOL Init() = 0;
	virtual void Record() = 0;
	virtual void Stop() = 0;

	virtual FLOAT GetSampleData( DWORD& Offset, void* Pointer, DWORD& Samples, EAudioCaptureType Type ) = 0;
	virtual DWORD GetCurrentOffset() = 0;
	virtual	DWORD GetAvailableSamples( DWORD Offset, EAudioCaptureType Type ) = 0;
	virtual DWORD GetSamplesPerSecond() = 0;
};

/*-----------------------------------------------------------------------------
	USound.
-----------------------------------------------------------------------------*/

//
// Sound data.
//
class ENGINE_API FSoundData : public TLazyArray<BYTE>
{
public:
	USound* Owner;

// FSoundData is represented as a byte array in Sound.uc, but the internal
//  pointers, etc, cause this to expand on a 64-bit platform, so we add that
//  extra space for 32-bit platforms so that the byte array in unrealscript
//  matches for both platforms. --ryan.
#if PLATFORM_32BITS
    INT padding_for_32bits_1 GCC_PACK(4);
    INT padding_for_32bits_2 GCC_PACK(4);
    INT padding_for_32bits_3 GCC_PACK(4);
    INT padding_for_32bits_4 GCC_PACK(4);
#endif

	void Load();
	FLOAT GetPeriod();
	FSoundData( USound* InOwner )
	: Owner( InOwner )
	{}
};

// gam ---

//
// A sound effect.
//
class ENGINE_API USound : public UObject
{
	DECLARE_CLASS(USound,UObject,CLASS_SafeReplace,Engine)

public:

    // Constructor.
    USound();
	USound( INT InFlags );
    USound( const TCHAR* InFilename, INT InFlags ); // sjs - per vogel

    virtual USound* RenderSoundPlay( FLOAT *Volume, FLOAT *Pitch );

    virtual bool IsValid();
    virtual FSoundData &GetData();
    virtual FName GetFileType();
    virtual void SetFileType (FName FileType);
    virtual const TCHAR* GetFilename();
    virtual INT GetOriginalSize();
    virtual void* GetHandle();
    virtual void SetHandle(void* Handle);
    virtual INT GetFlags();
    virtual FLOAT GetDuration();
    virtual FLOAT GetRadius(); // sjs
    virtual FLOAT GetVelocityScale(); // gam
    virtual void SetDuration(FLOAT Duration);
    virtual void PS2Convert();
    virtual void Load() { GetData().Load(); } //amb

    // UObject interface.
    void Serialize( FArchive& Ar );
    void Destroy();
    void PostLoad();

    friend class FSoundData;

    static      UAudioSubsystem *Audio;

    FLOAT       Likelihood; // Used in a USoundGroup to distribute random sounds.
	//protected:

	// Variables.
	FSoundData	Data;
	FName		FileType;
	FString		Filename;
	INT			OriginalSize;
	FLOAT       Duration;
	void*		Handle;
	INT			Flags;
	EVoiceCodec	VoiceCodec GCC_PACK(4);
	FLOAT		InitialSeekTime;
	FLOAT       BaseRadius; // sjs
    FLOAT       VelocityScale; // gam
};

//
// A modified and dynamic instance of a sound.
//
class ENGINE_API UProceduralSound : public USound
{
    DECLARE_CLASS(UProceduralSound,USound,CLASS_SafeReplace,Engine)
   
    public:


        // Constructor.
        UProceduralSound();

        // USound interface.
        USound* RenderSoundPlay( FLOAT *Volume, FLOAT *Pitch );

        bool IsValid();
        FSoundData &GetData();
        FName GetFileType();
        void SetFileType (FName FileType);
        const TCHAR* GetFilename();
        INT GetOriginalSize();
        void* GetHandle();
        void SetHandle(void* Handle);
        INT GetFlags();
        FLOAT GetDuration();
        FLOAT GetRadius(); // sjs
        FLOAT GetVelocityScale(); // gam
        void SetDuration(FLOAT Duration);
        void PS2Convert();
        void Load(); // gam

        // UObject interface.
        void Serialize( FArchive& Ar );
        void Destroy();
        void PostLoad();
        void PostEditChange();

    //protected:

        USound *BaseSound;

        // Modifications and Variances are in % and are bounded by [-100, +oo]
        FLOAT PitchModification;
        FLOAT VolumeModification;

        FLOAT PitchVariance; 
        FLOAT VolumeVariance;

        FLOAT RenderedPitchModification;
        FLOAT RenderedVolumeModification;
	};

//
// A group of sounds.
//
class ENGINE_API USoundGroup : public USound
{
    DECLARE_CLASS(USoundGroup,USound,CLASS_SafeReplace,Engine)

    public:

        USoundGroup();
        void RefreshGroup( UBOOL Force = 0 );

        // USound interface.
        USound* RenderSoundPlay( FLOAT *Volume, FLOAT *Pitch );

        bool IsValid();
        FSoundData &GetData();
        FName GetFileType();
        void SetFileType (FName FileType);
        const TCHAR* GetFilename();
        INT GetOriginalSize();
        void* GetHandle();
        void SetHandle(void* Handle);
        INT GetFlags();
        FLOAT GetDuration();
        FLOAT GetRadius(); // sjs
        FLOAT GetVelocityScale(); // gam
        void SetDuration(FLOAT Duration);
        void PS2Convert();
        void Load(); //amb

	// UObject interface.
        void Serialize( FArchive& Ar );
        void Destroy();
        void PostLoad();
        void PostEditChange();

        FStringNoInit Package; // deprecated

    //protected:

        void RefreshGroup( const FString &Package ); // OBSOLETE!

        TArrayNoInit<USound*> Sounds;
        FLOAT TotalLikelihood;
        USound *RenderedSound;
};

// --- gam

/*-----------------------------------------------------------------------------
	FWaveModInfo. 
-----------------------------------------------------------------------------*/

//  Macros to convert 4 bytes to a Riff-style ID DWORD.
//  Todo: make these endian independent !!!

#undef MAKEFOURCC

#define MAKEFOURCC(ch0, ch1, ch2, ch3)\
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |\
    ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))

#define mmioFOURCC(ch0, ch1, ch2, ch3)\
    MAKEFOURCC(ch0, ch1, ch2, ch3)

// Main Riff-Wave header.
struct FRiffWaveHeader
{ 
	DWORD	rID;			// Contains 'RIFF'
	DWORD	ChunkLen;		// Remaining length of the entire riff chunk (= file).
	DWORD	wID;			// Form type. Contains 'WAVE' for .wav files.
};

// General chunk header format.
struct FRiffChunkOld
{
	DWORD	ChunkID;		  // General data chunk ID like 'data', or 'fmt ' 
	DWORD	ChunkLen;		  // Length of the rest of this chunk in bytes.
};

// ChunkID: 'fmt ' ("WaveFormatEx" structure ) 
struct FFormatChunk
{
    _WORD   wFormatTag;        // Format type: 1 = PCM
    _WORD   nChannels;         // Number of channels (i.e. mono, stereo...).
    DWORD   nSamplesPerSec;    // Sample rate. 44100 or 22050 or 11025  Hz.
    DWORD   nAvgBytesPerSec;   // For buffer estimation  = sample rate * BlockAlign.
    _WORD   nBlockAlign;       // Block size of data = Channels times BYTES per sample.
    _WORD   wBitsPerSample;    // Number of bits per sample of mono data.
    // This field doesn't exist until wFormatTag != 1  !!!  --ryan.
    //_WORD   cbSize;            // The count in bytes of the size of extra information (after cbSize).
};

// ChunkID: 'smpl'
struct FSampleChunk
{
	DWORD   dwManufacturer;
	DWORD   dwProduct;
	DWORD   dwSamplePeriod;
	DWORD   dwMIDIUnityNote;
	DWORD   dwMIDIPitchFraction;
	DWORD	dwSMPTEFormat;		
	DWORD   dwSMPTEOffset;		//
	DWORD   cSampleLoops;		// Number of tSampleLoop structures following this chunk
	DWORD   cbSamplerData;		// 
};
 
struct FSampleLoop				// Immediately following cbSamplerData in the SMPL chunk.
{
	DWORD	dwIdentifier;		//
	DWORD	dwType;				//
	DWORD	dwStart;			// Startpoint of the loop in samples
	DWORD	dwEnd;				// Endpoint of the loop in samples
	DWORD	dwFraction;			// Fractional sample adjustment
	DWORD	dwPlayCount;		// Play count
};

//
// Structure for in-memory interpretation and modification of WAVE sound structures.
//
class ENGINE_API FWaveModInfo
{
public:

	// Pointers to variables in the in-memory WAVE file.
	DWORD* pSamplesPerSec;
	DWORD* pAvgBytesPerSec;
	_WORD* pBlockAlign;
	_WORD* pBitsPerSample;
	_WORD* pChannels;

	DWORD  OldBitsPerSample;

	DWORD* pWaveDataSize;
	DWORD* pMasterSize;
	BYTE*  SampleDataStart;
	BYTE*  SampleDataEnd;
	DWORD  SampleDataSize;
	BYTE*  WaveDataEnd;

	INT	   SampleLoopsNum;
	FSampleLoop*  pSampleLoop;

	DWORD  NewDataSize;
	UBOOL  NoiseGate;

	// Constructor.
	FWaveModInfo()
	{
		NoiseGate   = false;
		SampleLoopsNum = 0;
	}
	
	// 16-bit padding.
	DWORD Pad16Bit( DWORD InDW )
	{
		return ((InDW + 1)& ~1);
	}

	// Read headers and load all info pointers in WaveModInfo. 
	// Returns 0 if invalid data encountered.
	// UBOOL ReadWaveInfo( TArray<BYTE>& WavData );
	UBOOL ReadWaveInfo( TArray<BYTE>& WavData );
	
	// Handle RESIZING and updating of all variables needed for the new size:
	// notably the (possibly multiple) loop structures.
	UBOOL UpdateWaveData( TArray<BYTE>& WavData);

	// Wave size and/or bitdepth reduction.
	void Reduce16to8();
	void HalveData();
	void HalveReduce16to8(); 

	// Filters.
	void NoiseGateFilter(); 
};

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

