/*=============================================================================
	UnPrefab.cpp: Unreal prefabs.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Warren Marshall
=============================================================================*/

#include "EditorPrivate.h" 

/*-----------------------------------------------------------------------------
	UPrefab implementation.
-----------------------------------------------------------------------------*/

void UPrefab::Serialize( FArchive& Ar )
{
	guard(UPrefab::Serialize);
	Super::Serialize( Ar );
	Ar << FileType;
	if( Ar.IsLoading() || Ar.IsSaving() )
	{
		Ar << T3DText;
	}
	unguard;
}

void UPrefab::Destroy()
{
	guard(UPrefab::Destroy);
	Super::Destroy();
	unguard;
}

void UPrefab::PostLoad()
{
	guard(UPrefab::PostLoad);
	Super::PostLoad();
	unguard;
};
IMPLEMENT_CLASS(UPrefab);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

