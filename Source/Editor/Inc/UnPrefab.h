/*=============================================================================
	UnPrefab.h: Unreal prefab related classes.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/*-----------------------------------------------------------------------------
	Constants.
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
	UPrefab
-----------------------------------------------------------------------------*/

// A prefab.
//
class EDITOR_API UPrefab : public UObject
{
	DECLARE_CLASS(UPrefab,UObject,CLASS_SafeReplace,Engine)

	// Variables.
	FName		FileType;
	FString T3DText;

	// Constructor.
	UPrefab()
	{
	}

	// UObject interface.
	void Serialize( FArchive& Ar );
	void Destroy();
	void PostLoad();
};

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

