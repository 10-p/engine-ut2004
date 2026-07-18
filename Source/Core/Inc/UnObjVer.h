/*=============================================================================
	UnObjVer.h: Unreal object version.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Version coding.
-----------------------------------------------------------------------------*/

// Earliest engine build that is network compatible with this one.
#define ENGINE_MIN_NET_VERSION 3180

// Engine build number, for displaying to end users.
#define ENGINE_VERSION 3369

// Base protocol version to negotiate in network play. 
// should always be same as ENGINE_MIN_NET_VERSION
#define ENGINE_NEGOTIATION_VERSION 3180

// Prevents incorrect files from being loaded.
#define PACKAGE_FILE_TAG 0x9E2A83C1

// The current Unrealfile version.
#define PACKAGE_FILE_VERSION 128

// Licensee Version Number.
#define PACKAGE_FILE_VERSION_LICENSEE	0x1D //amb, jij, jack
#define VERSION_ENGINE_ONLY				0x00

// The earliest file version which we can load with complete
// backwards compatibility. Must be at least PACKAGE_FILE_VERSION.
#define PACKAGE_MIN_VERSION 60

/*-----------------------------------------------------------------------------
	PACKAGE_FILE_VERSION History
-----------------------------------------------------------------------------*/

// 122 - Merge in skeletal collision stuff for SVehicle support.
// 123 - removed InclusiveSphereBound from FBSPNode
// 124 - Add bBlockKarma to per-bone primitives.
// 125 - Add bBlockNonZeroExtent/bBlockZeroExtent to per-bone primitives.
// 126 - Merged k-dop static mesh collision code.
// 127 - Static mesh collision for skeletal meshes. -pv
// 128 - added DetailMode to FDecorationLayer

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

