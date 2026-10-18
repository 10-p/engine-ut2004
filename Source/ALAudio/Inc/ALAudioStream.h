/*=============================================================================
	ALAudioStream.h: Unreal OpenAL Audio interface object.
	Copyright 1999-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel.
	* Ported to Linux by Ryan C. Gordon.
=============================================================================*/

#ifndef _INC_ALAUDIOSTREAM
#define _INC_ALAUDIOSTREAM

/*------------------------------------------------------------------------------------
	Defines.
------------------------------------------------------------------------------------*/

#define MAX_STREAM_CHUNKSIZE 65536
#define STREAM_NUMCHUNKS 4

struct	OggVorbis_File;

/*------------------------------------------------------------------------------------
	FALAudioStream.
------------------------------------------------------------------------------------*/

class ALAUDIO_API FALAudioStream
{
public:
	// Constructor/ Destructor.
	FALAudioStream( FALAudioStream* Previous, FCriticalSection* CriticalSection );
	virtual ~FALAudioStream();

	// Main thread functions.
	void RequestChunks( INT Chunks, void* Data );
	UBOOL QueryStream( INT& ChunksQueued );
	virtual INT Seek( FLOAT Seconds ) = 0;

	// Functions to be access by main thread that don't need to be thread safe.
	FLOAT GetDuration() { return Duration; }
	INT GetFormat() { return Format; }
	INT GetRate() { return Rate; }
	INT GetChunkSize() { return ChunkSize; }

	// Streaming thread functions.
	virtual UBOOL Init( const TCHAR* Filename, void* InSharedData, EAudioStreamType InType );
	virtual UBOOL ReadChunks( INT NumChunks ) = 0;
	
	// Calling function is responsible for syncronization.
	FALAudioStream* GetNext() { return Next; }
	FALAudioStream* GetPrevious() { return Previous; }

protected:	
	FCriticalSection*	CriticalSection;
	
	FALAudioStream*		Previous;
	FALAudioStream*		Next;

	void*				SharedData;
	FILE*				FileHandle;
	
	FLOAT				Duration;
	INT					FileSeek,
						ChunkSize,
						ChunksRequested,
						LoopCount,
						Format,
						Rate;
	UBOOL				EndOfFile;
	
	EAudioStreamType	Type;
};


/*------------------------------------------------------------------------------------
	FALAudioStreamOgg.
------------------------------------------------------------------------------------*/

class ALAUDIO_API FALAudioStreamOgg : public FALAudioStream
{
public:
	// Constructor/ Destructor.
	FALAudioStreamOgg( FALAudioStream* Previous, FCriticalSection* CriticalSection );
	virtual ~FALAudioStreamOgg();

	// Streaming thread functions.
	UBOOL Init( const TCHAR* Filename, void* InSharedData, EAudioStreamType InType );
	UBOOL ReadChunks( INT NumChunks );

	// Main thread functions.
	INT Seek( FLOAT Seconds );

protected:	
	// Vorbis specific.
	OggVorbis_File*		VorbisFile;
};


#if WITH_MP3
/*------------------------------------------------------------------------------------
	FALAudioStreamMP3.
------------------------------------------------------------------------------------*/
class ALAUDIO_API FALAudioStreamMP3 : public FALAudioStream
{
public:
	// Constructor/ Destructor.
	FALAudioStreamMP3( FALAudioStream* Previous, FCriticalSection* CriticalSection );
	virtual ~FALAudioStreamMP3();

	// Streaming thread functions.
	UBOOL Init( const TCHAR* Filename, void* InSharedData, EAudioStreamType InType );
	UBOOL ReadChunks( INT NumChunks );

	// Main thread functions.
	INT Seek( FLOAT Seconds );

protected:	
	// Decoder
	signed int			DecoderHandle; // make sure HASISTREAM == S32 == signed int in mss.h
};
#endif

/*------------------------------------------------------------------------------------
	FALAudioStreamManager.
------------------------------------------------------------------------------------*/

class ALAUDIO_API FALAudioStreamManager
{
public:
	// Constructor/ Destructor.
	FALAudioStreamManager();
	~FALAudioStreamManager();

	// Interface.
	void Init();
	void Destroy();
	void Tick();			// Streaming pump driven from the audio update on the web.
	FALAudioStream* CreateStream( const TCHAR* Filename, INT InitialChunks, void* SharedData, EAudioStreamType Type, FLOAT InitialSeekTime );
	void DestroyStream( FALAudioStream* AudioStream, UBOOL ReadQueuedChunks );

	// Variables.
	FCriticalSection		CriticalSection;

	FALAudioStream*			First;
	FALAudioStream*			Last;

	DWORD					FileStreamingThreadId;
	UBOOL					Destroyed;					
};

#endif
