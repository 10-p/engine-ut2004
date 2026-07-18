/*=============================================================================
	ALAudioVoice.h: Voice encoding/ decoding module.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Daniel Vogel.
=============================================================================*/

#ifndef _INC_ALAUDIOVOICE
#define _INC_ALAUDIOVOICE

/*------------------------------------------------------------------------------------
	Dependencies.
------------------------------------------------------------------------------------*/

// Speex is built from source on every platform but win32 right now,
//  so the packing only needs to be forced if we're trying to match a
//  precompiled binary package.  --ryan.
#if ( (SUPPORTS_PRAGMA_PACK) && ((defined WIN32) && (!defined _WIN64)) )
#define PACK_SPEEX_TO_EIGHT_BYTES 1
#endif

#if PACK_SPEEX_TO_EIGHT_BYTES
#pragma pack(push,8)
#endif

#define EPIC_48K
#include "speex.h"
#include "speex_preprocess.h"

#if PACK_SPEEX_TO_EIGHT_BYTES
#pragma pack(pop)
#endif

/*------------------------------------------------------------------------------------
	FALVoiceModule.
------------------------------------------------------------------------------------*/

class FALVoiceModule
{
public:
	FALVoiceModule( class UALAudioSubsystem* InAudioSubsystem, FAudioCaptureDevice* InCaptureDevice );
	~FALVoiceModule();

	void SetEncoder( EVoiceCodec InVoiceCodec );

	INT Encode();
	UBOOL Decode( FVoiceInfo* VoiceInfo );

	void NoteDestroy( AActor* Actor );

protected:
	UALAudioSubsystem*			AudioSubsystem;
	FAudioCaptureDevice*		CaptureDevice;

	DWORD						CaptureOffset;
	UBOOL						CapturingVoice,
								VADActive;

	TMap<INT, APawn*>			PlayerIDToPawn;

	void*						SpeexEncoder; 
	SpeexPreprocessState*		SpeexPreprocessor;
	TMap<USound*,void*>			SpeexDecoders;
	SpeexBits*					SpeexData;
	EVoiceCodec					VoiceCodec;
};

#endif

