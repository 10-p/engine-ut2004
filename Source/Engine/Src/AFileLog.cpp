/*=============================================================================
	AFileLog.cpp: Unreal Tournament 2003 mod author logging
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Joe Wilcox
		* Enhanced by Michiel Hendriks
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
	Stat Log Implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(AFileLog);

void AFileLog::execOpenLog( FFrame& Stack, RESULT_DECL )
{
	guard(AFileLog::execOpenLog);

	P_GET_STR(FName);
	P_GET_STR(FExt);
	P_GET_UBOOL(bOverwrite);
	P_FINISH;

	// close current file before opening
	if( LogAr )	FinishLog();

	// Strip all pathing characters from the name

	for (INT i=0;i<appStrlen(*FName);i++)
	{
		if ( (*FName)[i]=='\\' || (*FName)[i]=='.' || (*FName)[i]==':' || (*FName)[i]=='/')
			( (TCHAR*) (*FName) )[i] = '_';
	}

	GFileManager->MakeDirectory( TEXT("..") PATH_SEPARATOR TEXT("UserLogs") );

	FString fileName;
	fileName = TEXT("..") PATH_SEPARATOR TEXT("UserLogs") PATH_SEPARATOR;
	fileName += FName;

	if (FExt != TEXT("log") && FExt != TEXT("txt") && FExt != TEXT("html") && FExt != TEXT("htm"))
		FExt = TEXT("log");

	LogFileName = fileName + TEXT(".") + FExt;
	// to prevent clashing on diffirent extentions but same basename
	TempFileName = LogFileName + TEXT(".tmp");

	debugf(TEXT("Opening user log %s"),*LogFileName);

	// remove unwanted tempfile
	if (GFileManager->FileSize(*TempFileName) > -1)
		GFileManager->Delete(*TempFileName);
	// bring back old file
	if (GFileManager->FileSize(*LogFileName) > -1)
	{
		if (bOverwrite) GFileManager->Delete(*LogFileName);
		else GFileManager->Move(*TempFileName, *LogFileName);
	}

	LogAr = (PTRINT) GFileManager->CreateFileWriter( *TempFileName, FILEWRITE_EvenIfReadOnly + FILEWRITE_Append );
	if (!LogAr)
		debugf(TEXT("Failed to open user log %s"),*LogFileName);

	unguardexec;
}

void AFileLog::execCloseLog( FFrame& Stack, RESULT_DECL )
{
	guard(AFileLog::execCloseLog);
	P_FINISH;

	FinishLog();
	unguardexec;
}


void AFileLog::execLogf( FFrame& Stack, RESULT_DECL )
{
	guard(AFileLog::execLogf);
	P_GET_STR(Data);
	P_FINISH;

	if (!LogAr)
		return;

	FString LogString = Data + TEXT("\r\n");
	
	ANSICHAR AnsiStr[1024];

	INT i;
	for( i=0; i<LogString.Len(); i++ )
		AnsiStr[i] = ToAnsi((*LogString)[i] );

	AnsiStr[i] = 0;
	((FArchive*)LogAr)->Serialize( AnsiStr, i );

	unguardexec;
}

void AFileLog::FinishLog()
{
	guard(AFileLog::FinishLog);
	if( LogAr )
	{
		delete (FArchive*)LogAr;
		// move temp file
		GFileManager->Move(*LogFileName, *TempFileName);
	}
	LogAr = 0;

	unguard;
}

void AFileLog::Destroy()
{
	guard(AFileLog::Destroy);
	FinishLog();
	Super::Destroy();
	unguard;
}

