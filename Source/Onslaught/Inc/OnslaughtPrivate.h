/*=============================================================================
	OnslaughtPrivate.h: Native Onslaught specific code
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Dave Hagewood @ Psyonix
=============================================================================*/

#ifndef ONSLAUGHTPRIVATE_H
#define ONSLAUGHTPRIVATE_H


/*----------------------------------------------------------------------------
	API.
----------------------------------------------------------------------------*/

#ifndef ONSLAUGHT_API
	#define ONSLAUGHT_API DLL_IMPORT
#endif

extern "C" {
    void RegisterNamesOnslaught(void);
}

/*-----------------------------------------------------------------------------
	Includes..
-----------------------------------------------------------------------------*/

#include "Engine.h"
#include "OnslaughtClasses.h"

#if __STATIC_LINK
#define NAMES_ONLY
#define NATIVE_DEFS_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "OnslaughtClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVE_DEFS_ONLY
#undef NAMES_ONLY
#endif

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

