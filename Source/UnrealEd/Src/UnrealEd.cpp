/*=============================================================================
	UnrealEd.cpp: UnrealEd package file
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Jack Porter
=============================================================================*/

#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	UUnrealEdEngine.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UUnrealEdEngine);

/*-----------------------------------------------------------------------------
	Package implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_PACKAGE(UnrealEd);

// sjs --- import natives
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "UnrealEdClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVES_ONLY
#undef NAMES_ONLY
// --- sjs

/*---------------------------------------------------------------------------------------
	The End.
---------------------------------------------------------------------------------------*/

