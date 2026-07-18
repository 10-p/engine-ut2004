/*=============================================================================
	USoundLocCommandlet.cpp: Sound localization help commandlet
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	- Imports localized sounds matching an english UAX
	- Warns of any sounds which do not exist in the localized directory
	- Warns of any differences in sample rate, bit rate or # of channels (if check parameter is supplied)
	- Conforms the localized package to the english one

	eg: ucc editor.soundloc TutorialSounds \\server\ut2003\localization\FinalLocSounds itt check

Revision history:
	* Created by Jack Porter
=============================================================================*/

#include "EditorPrivate.h"

/*-----------------------------------------------------------------------------
	UConformCommandlet.
-----------------------------------------------------------------------------*/

class USoundLocCommandlet : public UCommandlet
{
	DECLARE_CLASS(USoundLocCommandlet,UCommandlet,CLASS_Transient,Editor);
	void StaticConstructor()
	{
		guard(USoundLocCommandlet::StaticConstructor);

		LogToStdout     = 0;
		IsClient        = 1;
		IsEditor        = 1;
		IsServer        = 1;
		LazyLoad        = 1;
		ShowErrorCount  = 1;

		unguard;
	}
	INT Main( const TCHAR* Parms )
	{
		guard(USoundLocCommandlet::Main);

		UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail | LOAD_DisallowFiles, NULL );
		GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
		GEditor->UseSound = 0;
        GEditor->InitEditor();
		GIsRequestingExit = 1; // Causes ctrl-c to immediately exit.

		FString Pkg, Dir, Lang, CheckFormat;
		if( !ParseToken(Parms,Pkg,0) )
			appErrorf(TEXT("Package file not specified"));
		if( !ParseToken(Parms,Dir,0) )
			appErrorf(TEXT("Translated sound directory not specified"));
		if( !ParseToken(Parms,Lang,0) )
			appErrorf(TEXT("Language not specified"));
		ParseToken(Parms,CheckFormat,0);

		GWarn->Logf( TEXT("Loading package %s..."), *Pkg );

		// conform to the existing .uax
		ULinkerLoad* Conform = NULL;
		BeginLoad();
		Conform = UObject::GetPackageLinker( CreatePackage(NULL,*(US+Pkg+TEXT("_OLD"))), *(FString(TEXT(".."))*FString(TEXT("Sounds"))*Pkg+FString(TEXT(".uax"))), LOAD_NoFail, NULL, NULL );

		// do this in a seperate loop as the rename below confuses the iterator.
		TArray<USound*> EnglishSounds;
		UObject* Package = LoadPackage(NULL,*(FString(TEXT(".."))*FString(TEXT("Sounds"))*Pkg+FString(TEXT(".uax"))),LOAD_NoFail);
		EndLoad();
		check(Package);

		for( TObjectIterator<UObject> It; It; ++It )
		{
			USound* EnglishSound = Cast<USound>(*It);
			if( EnglishSound && EnglishSound->IsIn(Package) )
				EnglishSounds.AddItem(EnglishSound);
		}

		for( INT i=0;i<EnglishSounds.Num();i++ )
		{
			USound* EnglishSound = EnglishSounds(i);

			EnglishSound->GetData().Load();
			check(EnglishSound->GetData().Num()>0);
			FWaveModInfo EnglishWaveInfo;
			EnglishWaveInfo.ReadWaveInfo(EnglishSound->GetData());

			FString LocWavFile = Dir * Lang * Pkg * EnglishSound->GetName() + TEXT(".wav");
			if( GFileManager->FileSize( *LocWavFile ) <= 0 )
				GWarn->Logf(TEXT("Missing: %s"), *(Lang * Pkg * EnglishSound->GetName() + TEXT(".wav")) );
			else
			{
				FString ImportCommand = FString::Printf(TEXT("NEW SOUND FILE=%s NAME=\"TempSound\" PACKAGE=\"TempAudio\""), *LocWavFile );
				GEditor->Exec( *ImportCommand );
				USound* LocSound = Cast<USound>(StaticFindObject( USound::StaticClass(), ANY_PACKAGE, TEXT("TempAudio.TempSound") ) );
				if( !LocSound )
					GWarn->Logf(TEXT("Localized sound failed import: %s"), *LocWavFile );
				else
				{
					check(LocSound->GetData().Num()>0);
					FWaveModInfo LocWaveInfo;
					LocWaveInfo.ReadWaveInfo(LocSound->GetData());

					if( CheckFormat != TEXT("") )
					{
						// Check the bitrates are the same
						if( *EnglishWaveInfo.pSamplesPerSec != *LocWaveInfo.pSamplesPerSec )
							GWarn->Logf( TEXT("%s: Sample rate mismatch: Eng: %2.1f kHz vs Loc: %2.1f kHz"), *LocWavFile, (FLOAT)*EnglishWaveInfo.pSamplesPerSec/1000, (FLOAT)*LocWaveInfo.pSamplesPerSec/1000 );
						if( *EnglishWaveInfo.pBitsPerSample != *LocWaveInfo.pBitsPerSample )
							GWarn->Logf( TEXT("%s: Bit rate mismatch: Eng: %d bit vs Loc: %d bit"), *LocWavFile, *EnglishWaveInfo.pBitsPerSample, *LocWaveInfo.pBitsPerSample );
						if( *EnglishWaveInfo.pChannels != *LocWaveInfo.pChannels )
							GWarn->Logf( TEXT("%s: Channels mismatch: Eng: %d vs Loc: %d"), *LocWavFile, *EnglishWaveInfo.pChannels, *LocWaveInfo.pChannels );
					}

					// Rename the English sound and replace it with the localized version.
                    UObject* NewOuter = EnglishSound->GetOuter();
					FString NewName = EnglishSound->GetName();

					EnglishSound->Rename( NULL, GetTransientPackage() );
					LocSound->Rename( *NewName, NewOuter );
				}
			}
		}

		FString LocFileName = FString(TEXT("..")) * FString(TEXT("Sounds")) * Pkg + FString(TEXT(".")) + Lang + FString(TEXT("_uax"));
		GWarn->Logf( TEXT("Saving package %s..."), *LocFileName );
		SavePackage( Package, NULL, RF_Standalone, *LocFileName, GWarn, Conform );
		GIsRequestingExit=1;
		return 0;
		unguard;
	}
};
IMPLEMENT_CLASS(USoundLocCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

