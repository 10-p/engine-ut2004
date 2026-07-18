/*=============================================================================
	SDLLaunchPrivate.h: Unreal launcher for Linux.
	Copyright 1999-2002 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel (based on XLaunch).
=============================================================================*/

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>

#if (!defined MACOSX)
// !!! FIXME: Do we really need this header?
// !!! FIXME:  (check Core/Inc/FMallocDebug.h, too.) --ryan.
#include <malloc.h>
#endif

#include <fcntl.h>
#include "Engine.h"
#include "UnRender.h"

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
