/*=============================================================================
	UnRebuildTools.cpp: Tools for handling rebuild options in the editor
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "EnginePrivate.h"

class FRebuildTools;
extern ENGINE_API FRebuildTools		GRebuildTools;

/*------------------------------------------------------------------------------
	FRebuildOptions.

	Holds a set of rebuild options.
------------------------------------------------------------------------------*/

FRebuildOptions::FRebuildOptions()
{
	Init();
	Name = TEXT("Default");
}

FRebuildOptions::~FRebuildOptions()
{
}

// Useful for resetting to the default values.
void FRebuildOptions::Init()
{
	BspOpt			= BSP_Optimal;
	Options			= REBUILD_Default;
	Balance			= 15;
	PortalBias		= 70;
	LightmapFormat	= TEXF_DXT3; // gam
	DitherLightmaps = 0; // gam
	SaveOutLightmaps= 0;
	ReportErrors	= 1;
}

/*------------------------------------------------------------------------------
	FRebuildTools.

	A helper class to store and work with the various rebuild configs.
------------------------------------------------------------------------------*/

FRebuildTools::FRebuildTools()
{
}

FRebuildTools::~FRebuildTools()
{
	Options.Empty();
}

void FRebuildTools::Init()
{
	Options.Empty();
	// Add a default rebuild config.  The others will be read from the INI file in UnrealEd/src/main.cpp
	new( Options )FRebuildOptions();

	Current = new FRebuildOptions;
	*Current = Options(0);

	// Read the user configs from the INI file
	INT NumConfigs;
	if( !GConfig->GetInt( TEXT("Rebuild Configs"), TEXT("NumItems"), NumConfigs, TEXT("UnrealEd.ini") ) )	NumConfigs = 0;

	for( INT x = 0 ; x < NumConfigs ; x++ )
	{
		FString Item = FString::Printf(TEXT("Config%d"),x);
		FString Data;
		if( GConfig->GetString( TEXT("Rebuild Configs"), *Item, Data, TEXT("UnrealEd.ini") ) )
		{
			TArray<FString> Fields;
			Data.ParseIntoArray( TEXT(","), &Fields );

			if( Fields.Num() == 6 )
			{
				FRebuildOptions* RO = Save( Fields(0) );
				RO->Balance = appAtoi( *Fields(1) );
				RO->BspOpt = (EBspOptimization)appAtoi( *Fields(2) );
				RO->Options = appAtoi( *Fields(3) );
				RO->PortalBias = appAtoi( *Fields(4) );
				RO->LightmapFormat = appAtoi( *Fields(5) );
			}
		}
	}
}

void FRebuildTools::Shutdown()
{
	// Write the configs out the INI file
	GConfig->EmptySection( TEXT("Rebuild Configs"), TEXT("UnrealEd.ini") );

	GConfig->SetInt( TEXT("Rebuild Configs"), TEXT("NumItems"), Options.Num(), TEXT("UnrealEd.ini") );
	for( INT x = 0 ; x < Options.Num() ; x++ )
	{
		FString Item = FString::Printf(TEXT("Config%d"),x);
		FString Data = FString::Printf(TEXT("%s,%d,%d,%d,%d,%d"),
			*Options(x).Name,
			Options(x).Balance,
			Options(x).BspOpt,
			Options(x).Options,
			Options(x).PortalBias,
			Options(x).LightmapFormat);
		GConfig->SetString( TEXT("Rebuild Configs"), *Item, *Data, TEXT("UnrealEd.ini") );
	}

	delete Current;
}

void FRebuildTools::SetCurrent( FString InName )
{
	guard(FRebuildTools::SetCurrent);
	FRebuildOptions* RO = GetFromName( InName );
	check( RO );	// Name passed in was bad - should never happen.
	*Current = *RO;
	unguard;
}

FRebuildOptions* FRebuildTools::GetFromName( FString InName )
{
	guard(FRebuildTools::GetFromName);

	for( INT x = 0 ; x < Options.Num() ; x++ )
		if( Options(x).GetName() == InName )
			return &Options(x);

	return NULL;

	unguard;
}

INT FRebuildTools::GetIdxFromName( FString InName )
{
	guard(FRebuildTools::GetIdxFromName);

	for( INT x = 0 ; x < Options.Num() ; x++ )
		if( Options(x).GetName() == InName )
			return x;

	return INDEX_NONE;

	unguard;
}

FRebuildOptions* FRebuildTools::Save( FString InName )
{
	guard(FRebuildTools::Save);

	FRebuildOptions* RO = GetFromName( InName );

	if( !RO )
	{
		new( Options )FRebuildOptions();
		RO = &Options( Options.Num()-1 );
	}

	*RO = *Current;
	RO->Name = InName;

	return RO;

	unguard;
}

void FRebuildTools::Delete( FString InName )
{
	guard(FRebuildTools::Delete);

	INT idx = GetIdxFromName( InName );
	if( idx == INDEX_NONE ) return;

	Options.Remove( idx );

	unguard;
}

/*------------------------------------------------------------------------------
	The End.
------------------------------------------------------------------------------*/


