/*=============================================================================
	UnDownload.cpp: Unreal file-download interface
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Jack Porter.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "FConfigCacheIni.h"

/*-----------------------------------------------------------------------------
	UDownload implementation.
-----------------------------------------------------------------------------*/

void UDownload::StaticConstructor()
{
	guard(UDownload::StaticConstructor);
	DownloadParams = TEXT("");
	UseCompression = 0;
	unguard;
}
void UDownload::Serialize( FArchive& Ar )
{
	guard(UDownload::Serialize);
	Super::Serialize( Ar );
	Ar << Connection;
	unguard;
}
UBOOL UDownload::TrySkipFile()
{
	guard(UDownload::TrySkipFile);
	if( RecvFileAr && Info->PackageFlags & PKG_ClientOptional )
	{
		SkippedFile = 1;
		return 1;
	}
	return 0;
	unguard;
}
void UDownload::ReceiveFile( UNetConnection* InConnection, INT InPackageIndex, const TCHAR *Params, UBOOL InCompression, INT InAttempt )
{
	guard(UDownload::ReceiveFile);
	Connection = InConnection;
	PackageIndex = InPackageIndex;
	Info = &Connection->PackageMap->List( PackageIndex );
	AttemptNumber = InAttempt;
	unguard;
}
void UDownload::ReceiveData( BYTE* Data, INT Count )
{
	guard(UDownload::ReceiveData)
	// Receiving spooled file data.
	if( Transfered==0 && !RecvFileAr )
	{
		// Open temporary file initially.
		debugf( NAME_DevNet, TEXT("Receiving package '%s'"), Info->Parent->GetName() );
		GFileManager->MakeDirectory( *GSys->CachePath, 0 );
		appCreateTempFilename( *GSys->CachePath, TempFilename );
		RecvFileAr = GFileManager->CreateFileWriter( TempFilename );
	}

	// Receive.
	if( !RecvFileAr )
	{
		// Opening file failed.
		DownloadError( LocalizeError(TEXT("NetOpen"),TEXT("Engine")) );
	}
	else
	{
		if( Count > 0 )
			RecvFileAr->Serialize( Data, Count );
		if( RecvFileAr->IsError() )
		{
			// Write failed.
			DownloadError( *FString::Printf( LocalizeError(TEXT("NetWrite"),TEXT("Engine")), TempFilename ) );
		}
		else
		{
			// Successful.
			Transfered += Count;
			TCHAR Msg1[256], Msg2[256];
			appSprintf( Msg1, (Info->PackageFlags&PKG_ClientOptional)?LocalizeProgress(TEXT("ReceiveOptionalFile"),TEXT("Engine")):LocalizeProgress(TEXT("ReceiveFile"),TEXT("Engine")), Info->Parent->GetName() );
			appSprintf( Msg2, LocalizeProgress(TEXT("ReceiveSize"),TEXT("Engine")), Info->DownloadSize/1024, 100.f*Transfered/Info->DownloadSize );
			Connection->Driver->Notify->NotifyProgress( TEXT(""), Msg1, Msg2, 4.f );
		}
	}	
	unguard;
}
void UDownload::DownloadError( const TCHAR* InError )
{
	guard(UDownload::DownloadError);
	// Failure.
	appStrcpy( Error, InError );
	unguard;
}
void UDownload::DownloadDone()
{
	guard(UDownload::DownloadDone);
	if( RecvFileAr )
	{
		delete RecvFileAr;
		RecvFileAr = NULL;
	}
	if( SkippedFile )
	{
		debugf( TEXT("Skipped download of '%s'"), Info->Parent->GetName() );
		GFileManager->Delete( TempFilename );
		TCHAR Msg[256];
		appSprintf( Msg, TEXT("Skipped '%s'"), Info->Parent->GetName() );
		Connection->Driver->Notify->NotifyProgress( TEXT(""),TEXT("Success"), Msg, 4.f );
		Connection->Driver->Notify->NotifyReceivedFile( Connection, PackageIndex, TEXT(""), 1, AttemptNumber );
	}
	else
	{
		TCHAR Dest[256];

		// Tack on the generation information about a package

		appSprintf( Dest, TEXT("%s") PATH_SEPARATOR TEXT("%s-%i.uxx"), *GSys->CachePath, Info->Guid.String(), Info->RemoteGeneration );

		if( !*Error && Transfered==0 )
			DownloadError( *FString::Printf( LocalizeError(TEXT("NetRefused"),TEXT("Engine")), Info->Parent->GetName() ) );
		if( !*Error && IsCompressed )
		{
			TCHAR CFilename[256], CFilenameWithExt[256];
			appStrcpy( CFilename, TempFilename );
			appStrcpy( CFilenameWithExt, TempFilename );
			appStrcat( CFilenameWithExt, COMPRESSED_EXTENSION );
			appCreateTempFilename( *GSys->CachePath, TempFilename );
			if( !GFileManager->Move( CFilenameWithExt, CFilename, 1, 1 ) )
				DownloadError( LocalizeError(TEXT("NetWrite"),TEXT("Engine")) );
			if( GFileManager->Copy( TempFilename, CFilename, 1, 1, 0, FILECOPY_Decompress, NULL ) != COPY_OK )
				DownloadError( LocalizeError(TEXT("NetSize"),TEXT("Engine")) );
			GFileManager->Delete( CFilenameWithExt );
		}
		if( !*Error && GFileManager->FileSize(TempFilename)!=Info->FileSize )
			DownloadError( LocalizeError(TEXT("NetSize"),TEXT("Engine")) );
		if( !*Error && !appCheckPackageFileGUID(TempFilename, &Info->Guid) )
			DownloadError( *FString::Printf( LocalizeError(TEXT("PackageVersion"),TEXT("Core")),Info->Parent->GetName()) );
		if( !*Error && !GFileManager->Move( Dest, TempFilename ) )
			DownloadError( LocalizeError(TEXT("NetMove"),TEXT("Engine")) );
		if( *Error )
		{
			GFileManager->Delete( TempFilename );
			Connection->Driver->Notify->NotifyReceivedFile( Connection, PackageIndex, Error, 0, AttemptNumber );
		}
		else
		{
			// Success.
			TCHAR Msg[256];
			TCHAR IniName[256];
			FConfigCacheIni CacheIni;
			appSprintf( IniName, TEXT("%s") PATH_SEPARATOR TEXT("cache.ini"), *GSys->CachePath );

			CacheIni.SetString( TEXT("Cache"), *FString::Printf(TEXT("%s-%i"),Info->Guid.String(),Info->RemoteGeneration ), *(*Info->URL) ? *Info->URL : Info->Parent->GetName(), IniName );

			appSprintf( Msg, TEXT("Received '%s'"), Info->Parent->GetName() );
			// ufront: LOG it too, one line per package received. NotifyProgress alone only reaches the screen, so
			// a log (native or the web page's console) never said which files a join downloaded. The web page
			// does NOT key its download-cache flush on this line: it flushes once per level ("Bringing ... up").
			debugf( TEXT("%s"), Msg );
			Connection->Driver->Notify->NotifyProgress( TEXT(""), TEXT("Success"), Msg, 4.f );
			Connection->Driver->Notify->NotifyReceivedFile( Connection, PackageIndex, Error, 0, AttemptNumber );
		}
	}
	unguard;
}
void UDownload::Destroy()
{
	guard(UDownload::Destroy);
	if( RecvFileAr )
	{
		delete RecvFileAr;
		RecvFileAr = NULL;
		GFileManager->Delete( TempFilename );
	}
	if( Connection && Connection->Download == this )
		Connection->Download = NULL;
	Connection = NULL;
	Super::Destroy();
	unguard;
}
IMPLEMENT_CLASS(UDownload)

/*-----------------------------------------------------------------------------
	UChannelDownload implementation.
-----------------------------------------------------------------------------*/

void UChannelDownload::StaticConstructor()
{
	guard(UChannelDownload::StaticConstructor);
	DownloadParams = TEXT("Enabled");
	unguard;
}
void UChannelDownload::Serialize( FArchive& Ar )
{
	guard(UChannelDownload::Serialize);
	Super::Serialize( Ar );
	Ar << Ch;
	unguard;
}
UBOOL UChannelDownload::TrySkipFile()
{
	guard(UChannelDownload::TrySkipFile);
	if( Ch && Super::TrySkipFile() )
	{
		FOutBunch Bunch( Ch, 1 );
		FString Cmd = TEXT("SKIP");
		Bunch << Cmd;
		Bunch.bReliable = 1;
		Ch->SendBunch( &Bunch, 0 );
		return 1;
	}
	return 0;
	unguard;
}
void UChannelDownload::ReceiveFile( UNetConnection* InConnection, INT InPackageIndex, const TCHAR *Params, UBOOL InCompression, INT InAttempt )
{
	guard(UChannelDownload::ReceiveFile);
	UDownload::ReceiveFile( InConnection, InPackageIndex, Params, InCompression, InAttempt );

	// Create channel.
	Ch = (UFileChannel *)Connection->CreateChannel( CHTYPE_File, 1 );
	if( !Ch )
	{
		DownloadError( LocalizeError(TEXT("ChAllocate"),TEXT("Engine")) );
		DownloadDone();
		return;
	}

	// Set channel properties.
	Ch->Download = this;
	Ch->PackageIndex = PackageIndex;

	// Send file request.
	FOutBunch Bunch( Ch, 0 );
	Bunch << Info->Guid;
	Bunch.bReliable = 1;
	check(!Bunch.IsError());
	Ch->SendBunch( &Bunch, 0 );
	
	unguard;
}
void UChannelDownload::Destroy()
{
	guard(UChannelDownload);
	if( Ch && Ch->Download == this )
		Ch->Download = NULL;
	Ch = NULL;
	Super::Destroy();
	unguard;
}
IMPLEMENT_CLASS(UChannelDownload)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

