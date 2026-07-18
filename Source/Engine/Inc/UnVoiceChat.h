/*=============================================================================
	UnVoiceChat.h: Unreal Voice Chat support header.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#include "UnForcePacking_begin.h"

struct FVoiceChatterInfo
{
	class AController*	Controller;
	DWORD				IpAddr;
	INT					Handle;
};

#include "UnForcePacking_end.h"


