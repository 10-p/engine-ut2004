/*=============================================================================
	UConvertMaterialCommandlet.cpp: Load all levels and convert polyflags to materials
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jack Porter

=============================================================================*/

#include "EditorPrivate.h"
//#include <stdlib.h>

/*-----------------------------------------------------------------------------
	UConvertMaterialCommandlet
-----------------------------------------------------------------------------*/


#define OLDUNRDIR TEXT("..\\Maps\\")
#define OLDUTXDIR TEXT("..\\Textures\\")
#define OLDUSXDIR TEXT("..\\StaticMeshes\\")

#define NEWUNRDIR TEXT("..\\NewMaps\\")
#define NEWUTXDIR TEXT("..\\NewTextures\\")
#define NEWUSXDIR TEXT("..\\NewStaticMeshes\\")



class UConvertMaterialCommandlet : public UCommandlet
{
	DECLARE_CLASS(UConvertMaterialCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(UConvertMaterialCommandlet::StaticConstructor);

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
		guard(UConvertMaterialCommandlet::Main);

		// Ensure all native classes are loaded.
		for( TObjectIterator<UClass> It ; It ; ++It )
			if( !It->GetLinker() )
				LoadObject<UClass>( It->GetOuter(), It->GetName(), NULL, LOAD_Quiet|LOAD_NoWarn, NULL );

		GLazyLoad = 0;

		GWarn->Logf(TEXT("Loading Textures ...."));
		TArray<FString> UTXList = GFileManager->FindFiles( *FString::Printf(TEXT("%s%s"), OLDUTXDIR, TEXT("*.utx")), 1, 0 );
		TArray<UObject*> UTXPackage(UTXList.Num());
		for (INT i=0; i<UTXList.Num(); i++)
		{
			FString OldUTXName = FString::Printf(TEXT("%s%s"), OLDUTXDIR, *UTXList(i));
			GWarn->Logf(TEXT("  Loading %s"), *OldUTXName );
			UTXPackage(i) = LoadPackage( NULL, *OldUTXName, 0 );
			check(UTXPackage(i));
		}
	
		GWarn->Logf(TEXT("Loading Static Meshes ...."));
		TArray<FString> USXList = GFileManager->FindFiles( *FString::Printf(TEXT("%s%s"), OLDUSXDIR, TEXT("*.usx")), 1, 0 );
		TArray<UObject*> USXPackage(UTXList.Num());
		for (INT i=0; i<USXList.Num(); i++)
		{
			FString OldUSXName = FString::Printf(TEXT("%s%s"), OLDUSXDIR, *USXList(i));

			GWarn->Logf(TEXT("  Loading %s"), *OldUSXName );
			USXPackage(i) = LoadPackage( NULL, *OldUSXName, 0 );
			check(USXPackage(i));
		}

		GWarn->Logf(TEXT("Processing Maps ...."));
		{
			// Load and resave all UNRs
			TArray<FString> UNRList = GFileManager->FindFiles( *FString::Printf(TEXT("%s%s"), OLDUNRDIR, TEXT("*.ut2")), 1, 0 );
			for (INT i=0; i<UNRList.Num(); i++)
			{
				UObject::CollectGarbage(RF_Native | RF_Standalone);

				FString OldUNRName = FString::Printf(TEXT("%s%s"), OLDUNRDIR, *UNRList(i));
				FString NewUNRName = FString::Printf(TEXT("%s%s"), NEWUNRDIR, *UNRList(i));

				GWarn->Logf(TEXT("  Loading %s"), *OldUNRName );
				UObject* Package = LoadPackage( NULL, *OldUNRName, 0 );
				check(Package);
				ULevel* Level = FindObject<ULevel>( Package, TEXT("MyLevel") );
				check(Level);
			
				GWarn->Logf(TEXT("  Saving %s"), *NewUNRName );
				SavePackage( Package, Level, 0, *NewUNRName, GWarn );
			}
		}

		GWarn->Logf(TEXT("Saving Textures...."));
		for (INT i=0; i<UTXList.Num(); i++)
		{	
			FString NewUTXName = FString::Printf(TEXT("%s%s"), NEWUTXDIR, *UTXList(i));
			GWarn->Logf(TEXT("  Saving %s"), *NewUTXName );
			SavePackage( UTXPackage(i), NULL, RF_Standalone, *NewUTXName, NULL );
		}

		GWarn->Logf(TEXT("Saving Static Meshes...."));
		for (INT i=0; i<USXList.Num(); i++)
		{
			FString NewUSXName = FString::Printf(TEXT("%s%s"), NEWUSXDIR, *USXList(i));
			GWarn->Logf(TEXT("  Saving %s"), *NewUSXName );
			SavePackage( USXPackage(i), NULL, RF_Standalone, *NewUSXName, NULL );
		}

		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(UConvertMaterialCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

