/*=============================================================================
	UStream.h: Unreal streaming music public header file.
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision History:
	Created by Ron Prestenback.
=============================================================================*/

#include "Engine.h"
//#include "ID3Tag.h"

#ifndef NAMES_ONLY

#ifndef __USTREAM_H__
#define __USTREAM_H__

/*----------------------------------------------------------------------------
	UStream public includes.
----------------------------------------------------------------------------*/

struct ENGINE_API FFilePath
{
	FString FullPath GCC_PACK(4);
    FString Directory;
    FString Filename;
    FString Extension;
	TArray<FString> DirectoryParts;
    
	FFilePath();
    FFilePath( const TCHAR* FullPath );
    FFilePath( const TCHAR* InDir, const TCHAR* InFileName, const TCHAR* InExtension );
	FFilePath( TArray<FString>& InDirectories, const TCHAR* InDir = NULL, const TCHAR* InFileName = NULL, const TCHAR* InExtension = NULL );
};
struct ENGINE_API FID3Field
{
    PTRINT   Reference GCC_PACK(4);
    FString  FieldName;
    FString FieldValue;
    BYTE       FieldID;
	BYTE     IDCode[4];

	FID3Field( PTRINT Ref = 0, BYTE ID = 0 );	// Default constructor
	FID3Field( const struct FID3V2Frame* frame );            // Conversion constructor
	FID3Field( const struct FID3Field& Other );            // Copy constructor
};
#endif // __USTREAM_H__
#endif // NAMES_ONLY

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
