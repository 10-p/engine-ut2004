/*=============================================================================
	IpDrv.cpp: Unreal TCP/IP driver definition.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "UnIpDrv.h"

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// Package.
IMPLEMENT_PACKAGE(IpDrv);

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) IPDRV_API FName IPDRV_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "IpDrvClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

// sjs --- import natives
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "IpDrvClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVES_ONLY
#undef NAMES_ONLY
// --- sjs

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

