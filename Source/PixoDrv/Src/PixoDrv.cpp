/*=============================================================================
	PixoDrv.cpp: Unreal Pixo support precompiled header generator.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Michael Sartain from GL driver
		* Taken over by Daniel Vogel

=============================================================================*/

#include "PixoDrv.h"

#ifdef _DEBUG
#pragma comment ( lib, "../../Pixomatic/Lib/pixomatic_debug.lib" )
#else
#pragma comment ( lib, "../../Pixomatic/Lib/pixomatic.lib" )
#endif

extern "C"
{
    void autoInitializeRegistrantsPixoDrv(void)
    {
        UPixoRenderDevice::StaticClass();
    }
}

/*-----------------------------------------------------------------------------
	Unreal package implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_PACKAGE(PixoDrv);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

