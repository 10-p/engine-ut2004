/*=============================================================================
	WinDrv.cpp: Unreal Windows viewport and platform driver.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#include "WinDrv.h"

/*-----------------------------------------------------------------------------
	Package implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_PACKAGE(WinDrv);

/*-----------------------------------------------------------------------------
	Global functions.
-----------------------------------------------------------------------------*/

#pragma pack(push,8)
#include "dxerr8.h"
#pragma pack(pop)

void DirectInputError( const FString Error, HRESULT hr, UBOOL Fatal )
{
#ifdef _UNICODE
	debugf( TEXT("%s"), DXGetErrorString8W(hr) );
	debugf( TEXT("%s"), Error );
#else
	debugf( TEXT("%s"), DXGetErrorString8A(hr) );
	debugf( TEXT("%s"), Error );
#endif
	if ( Fatal )
		appErrorf(TEXT("unrecoverable error - bombing out"));
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

