/*=============================================================================
	UnVolume.h: Volumes.
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/


#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

struct FDecorationType
{
	class UStaticMesh*	StaticMesh;
	FRange				Count;
	FRange				DrawScale;
	UBOOL				bAlign;
	UBOOL				bRandomPitch;
	UBOOL				bRandomYaw;
	UBOOL				bRandomRoll;
};

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

