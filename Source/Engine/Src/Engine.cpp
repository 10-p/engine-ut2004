/*=============================================================================
	Engine.cpp: Unreal engine package.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	Globals.
-----------------------------------------------------------------------------*/

// Global subsystems in the engine.
ENGINE_API FMemStack			GEngineMem;
ENGINE_API FMemCache			GCache;
#ifndef CONSOLE
ENGINE_API FTerrainTools		GTerrainTools;
ENGINE_API FRebuildTools		GRebuildTools;
#endif
ENGINE_API FMatineeTools		GMatineeTools;

/*-----------------------------------------------------------------------------
	Package implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_PACKAGE(Engine);


/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/

