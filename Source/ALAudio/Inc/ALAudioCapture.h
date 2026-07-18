/*=============================================================================
	ALAudioCapture.h: Shared thread safe audio capture device.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Daniel Vogel.
=============================================================================*/

#ifndef _INC_ALAUDIOCAPTURE
#define _INC_ALAUDIOCAPTURE

/*------------------------------------------------------------------------------------
	Dependencies.
------------------------------------------------------------------------------------*/

#include "ALAudio.h"

#ifdef __EMSCRIPTEN__
#include <AL/al.h>
#include <AL/alc.h>
#else
#include "altypes.h"
#include "alctypes.h"
typedef struct ALCdevice_struct ALCdevice;
typedef struct ALCcontext_struct ALCcontext;
#endif

#if USE_DIRECTSOUND
#pragma pack(push,8)
#include <dsound.h>
#pragma pack(pop)
#endif

/*------------------------------------------------------------------------------------
	FALAudioCaptureDevice.
------------------------------------------------------------------------------------*/

#define FILTER_8K_SIZE 11

class FALAudioCaptureDevice : public FAudioCaptureDevice
{
public:
	FALAudioCaptureDevice();
	~FALAudioCaptureDevice();

	virtual UBOOL Init();
	virtual void Record();
	virtual void Stop();

	virtual FLOAT GetSampleData( DWORD& Offset, void* Pointer, DWORD& Samples, EAudioCaptureType Type );
	virtual DWORD GetCurrentOffset();
	virtual	DWORD GetAvailableSamples( DWORD Offset, EAudioCaptureType Type );
	virtual DWORD GetSamplesPerSecond();

protected:
	#if USE_DIRECTSOUND
	LPDIRECTSOUNDCAPTURE		CaptureDevice;
	LPDIRECTSOUNDCAPTUREBUFFER	CaptureBuffer;
	#endif

	// ALC_EXT_capture support...
	UBOOL						HaveALCaptureExt;
	ALCdevice*					ALCaptureDevice;
	BYTE*						ALCaptureBuffer;
	ALenum						alc_capture_samples;  // !!! FIXME: put this in the headers.
 
	DWORD						CaptureBufferSize;
	FCriticalSection			CriticalSection;
	INT							RecordCount;

	static	FLOAT				FilterNumerator[FILTER_8K_SIZE];
	static	FLOAT				FilterDenominator[FILTER_8K_SIZE];
	FLOAT						FilterMemory[FILTER_8K_SIZE-1];
};

#endif

