/*=============================================================================
	UnPSX2.h: PSX2 specific declarations.
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Jack Porter
=============================================================================*/

#if __GCN__

/*-----------------------------------------------------------------------------
	Stupid GCN Texture Overhead Management Functions
-----------------------------------------------------------------------------*/

UBOOL ReadTextureFile(const TCHAR* TextureName, TArray<BYTE>& TextureData);


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
#endif

