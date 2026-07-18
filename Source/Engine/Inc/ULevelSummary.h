/*=============================================================================
	ULevelSummary.h: Level summary.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	// Constructors.
	ULevelSummary()
	{}

	// UObject interface.
	void PostLoad()
	{
		guard(ULevelSummary::PostLoad);
		Super::PostLoad();
		if ( !(GUglyHackFlags & 64) )
		{
			const TCHAR* Text=Localize( TEXT("LevelInfo0"), TEXT("Title"), GetOuter()->GetName(), NULL, 1 );
			if( Text && *Text )
				Title = Text;
		}
		unguard;
	}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

