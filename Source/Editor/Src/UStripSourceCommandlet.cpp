/*=============================================================================
	UStripSourceCommandlet.cpp: Load a .u file and remove the script text from
	all classes.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jack Porter

=============================================================================*/

#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	UStripSourceCommandlet
-----------------------------------------------------------------------------*/

class UStripSourceCommandlet : public UCommandlet
{
	DECLARE_CLASS(UStripSourceCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UStripSourceCommandlet::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 0;
		ShowErrorCount  = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(UStripSourceCommandlet::Main);

		FString PackageName;
		if( !ParseToken(Parms, PackageName, 0) )
			appErrorf( TEXT("A .u package file must be specified.") );
	
		GWarn->Logf( TEXT("Loading package %s..."), *PackageName );
		GWarn->Logf(TEXT(""));
		UObject* Package = LoadPackage( NULL, *PackageName, LOAD_NoWarn );
		if( !Package )
			appErrorf( TEXT("Unable to load %s"), *PackageName );
	

		for( TObjectIterator<UClass> It; It; ++It )
		{
			if( It->GetOuter() == Package && It->ScriptText )
			{
				GWarn->Logf( TEXT("  Stripping source code from class %s"), It->GetName() );
				It->ScriptText->Text = FString(TEXT(" "));
				It->ScriptText->Pos = 0;
				It->ScriptText->Top = 0;
			}
		}

		GWarn->Logf(TEXT(""));
		GWarn->Logf(TEXT("Saving %s..."), *PackageName );
		SavePackage( Package, NULL, RF_Standalone, *PackageName, GWarn );
	
		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UStripSourceCommandlet)


#if __STATIC_LINK
void autoInitializeRegistrantsEditorStripSource(INT &Lookup)
{
	UStripSourceCommandlet::StaticClass();
}
#endif


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

