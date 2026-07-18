/*=============================================================================
	ALAudio.h: Unreal OpenAL Audio public header file.
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel.
	* Ported to Linux by Ryan C. Gordon.
=============================================================================*/

#ifndef _INC_ALAUDIO
#define _INC_ALAUDIO

/*----------------------------------------------------------------------------
	API.
----------------------------------------------------------------------------*/

#ifndef ALAUDIO_API
	#define ALAUDIO_API DLL_IMPORT
#endif

/*-----------------------------------------------------------------------------
	Dependencies.
-----------------------------------------------------------------------------*/

#if ((defined WIN32) || (defined _WIN64))
#define USE_DIRECTSOUND 1
#endif

#if USE_DIRECTSOUND
#pragma pack(push,8)
#include <windows.h>
#pragma pack(pop)
#endif

#include "Core.h"
#include "Engine.h"
#include "UnRender.h"

#ifndef WITH_SPEEX
#define WITH_SPEEX 1
#endif

#if !_WIN64 && !__UNIX__  // !!! FIXME: --ryan.
//#define WITH_MP3 1
#endif

/*-----------------------------------------------------------------------------
	Audio public includes.
-----------------------------------------------------------------------------*/

#define SAVE_SOUNDS 0
#define SAFE_RELEASE(p) { if(p) { (p)->Release(); (p)=NULL; } }

#include "ALAudioStream.h"
#include "ALAudioCapture.h"

#if WITH_SPEEX
#include "ALAudioVoice.h"
#endif

#include "ALAudioSubsystem.h"
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

#endif

