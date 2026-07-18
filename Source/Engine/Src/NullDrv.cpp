/*=============================================================================
	NullDrv.cpp: Unreal Null render device.
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel.
=============================================================================*/

// Includes.
#include "EnginePrivate.h"
#include "UnNullRenderDevice.h"

IMPLEMENT_CLASS(UNullRenderDevice);

INT FNullRenderInterface::SetDynamicStream(EVertexShader Shader,FVertexStream* Stream)
{ 
	// Copy the vertices into the vertex buffer.
	if( Stream->GetSize() > RenDev->DynamicData.Num() )
	{
		RenDev->DynamicData.Add( Stream->GetSize() - RenDev->DynamicData.Num() );
		debugf(TEXT("NullDrv: dynamic data size = %i"), RenDev->DynamicData.Num());
	}
	Stream->GetStreamData( &RenDev->DynamicData(0) );
	return 0; 
}


/*-----------------------------------------------------------------------------
	End.
-----------------------------------------------------------------------------*/

