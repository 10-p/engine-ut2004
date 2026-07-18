/*=============================================================================
	UStreamPrivate.h: Unreal streaming music private header file.
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

Revision History:
	Created by Ron Prestenback.
=============================================================================*/

#include "UStream.h"
#include "ID3Tag.h"

static FrameTypeList* List = NULL;

typedef FID3TagV1*& V1Tag;
typedef FID3TagV2*& V2Tag;

FrameTypeList* UStreamInteraction::GetFrameTypes()
{
	if ( !List )
		List = new FrameTypeList;

	return List;
}

void UStreamInteraction::Destroy()
{
	guard(UStreamInteraction::Destroy);

	Super::Destroy();

	if ( List )
	{
		delete List;
		List = NULL;
	}

	unguard;
}
