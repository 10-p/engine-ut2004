/*=============================================================================
	DemoPlayPenLev.cpp: Unreal demo playback pending level class.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Jack Porter
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"

/*-----------------------------------------------------------------------------
	UDemoPlayPendingLevel implementation.
-----------------------------------------------------------------------------*/

//
// Constructor.
//
UDemoPlayPendingLevel::UDemoPlayPendingLevel( UEngine* InEngine, const FURL& InURL )
:	UPendingLevel( InEngine, InURL )
{
	guard(UDemoPlayPendingLevel::UDemoPlayPendingLevel);

	// Try to create demo playback driver.
	UClass* DemoDriverClass = StaticLoadClass( UDemoRecDriver::StaticClass(), NULL, TEXT("ini:Engine.Engine.DemoRecordingDevice"), NULL, LOAD_NoFail, NULL );
	DemoRecDriver = ConstructObject<UDemoRecDriver>( DemoDriverClass );
	if( !DemoRecDriver->InitConnect( this, URL, Error ) )
	{
		delete DemoRecDriver;
		DemoRecDriver = NULL;
	}

	unguard;
}
//
// FNetworkNotify interface.
//
ULevel* UDemoPlayPendingLevel::NotifyGetLevel()
{
	guard(UDemoPlayPendingLevel::NotifyGetLevel);
	return NULL;
	unguard;
}
void UDemoPlayPendingLevel::NotifyReceivedText( UNetConnection* Connection, const TCHAR* Text )
{
	guard(UDemoPlayPendingLevel::NotifyReceivedText);
	//debugf( TEXT("DemoPlayPendingLevel received: %s"), Text );
	if( ParseCommand( &Text, TEXT("USES") ) )
	{
		// Dependency information.
		FPackageInfo& Info = *new(Connection->PackageMap->List)FPackageInfo(NULL);
		TCHAR PackageName[NAME_SIZE]=TEXT("");
		Parse( Text, TEXT("GUID=" ), Info.Guid );
		Parse( Text, TEXT("GEN=" ),  Info.RemoteGeneration );
		Parse( Text, TEXT("SIZE="),  Info.FileSize );
		Info.DownloadSize = Info.FileSize;
		Parse( Text, TEXT("FLAGS="), Info.PackageFlags );
		Parse( Text, TEXT("PKG="), PackageName, ARRAY_COUNT(PackageName) );
		Parse( Text, TEXT("FNAME="), Info.URL );
		guard(CreateUsesPackage);
		Info.Parent = CreatePackage(NULL,PackageName);
		unguardf((TEXT("PackageName=(%s) Text=(%s)"),PackageName,Text));
	}
	else if( ParseCommand( &Text, TEXT("WELCOME") ) )
	{
		FURL URL;
	
		// Parse welcome message.
		Parse( Text, TEXT("LEVEL="), URL.Map );

		UBOOL HadMissing = 0;
        
		// Make sure all packages we need available
		for( INT i=0; i<Connection->PackageMap->List.Num(); i++ )
		{
			TCHAR Filename[256];
			FPackageInfo& Info = Connection->PackageMap->List(i);
			if( !appFindPackageFile( Info.Parent->GetName(), &Info.Guid, Filename, Info.RemoteGeneration ) )
			{
				appSprintf( Filename, TEXT("%s%s"), Info.Parent->GetName(), DLLEXT );
				if( GFileManager->FileSize(Filename) <= 0 )
				{
					// We need to download this package.
					FilesNeeded++;
					Info.PackageFlags |= PKG_Need;

#if !DEMOVERSION
					if( DemoRecDriver->ClientRedirectURLs.Num()==0 || !DemoRecDriver->AllowDownloads || !(Info.PackageFlags & PKG_AllowDownload) )
#endif
					{
						if( Engine->Client->Viewports.Num() )
							Engine->Client->Viewports(0)->Actor->eventClientMessage( *FString::Printf(LocalizeError(TEXT("DemoFileMissing"),TEXT("Engine")), Info.Parent->GetName()), NAME_None );

						Error = FString::Printf( LocalizeError(TEXT("DemoFileMissing"),TEXT("Engine")), Info.Parent->GetName() );
						Connection->State = USOCK_Closed;
						HadMissing = 1;
					}
				}
			}
		}

		if( HadMissing )
			return;

#if !DEMOVERSION
		// Send first download request.
		ReceiveNextFile( Connection, 0 );
#endif

		DemoRecDriver->Time = 0;
		Success = 1;
	}
	unguard;
}
//
// UPendingLevel interface.
//
void UDemoPlayPendingLevel::Tick( FLOAT DeltaTime )
{
	guard(UDemoPlayPendingLevel::Tick);
	check(DemoRecDriver);
	check(DemoRecDriver->ServerConnection);

	if( DemoRecDriver->ServerConnection && DemoRecDriver->ServerConnection->Download )
		DemoRecDriver->ServerConnection->Download->Tick();

	if( !FilesNeeded )
	{
		// Update demo recording driver.
		DemoRecDriver->UpdateDemoTime( &DeltaTime, 1.f );
		DemoRecDriver->TickDispatch( DeltaTime );
		DemoRecDriver->TickFlush();
	}
	unguard;
}

UNetDriver* UDemoPlayPendingLevel::GetDriver()
{
	return DemoRecDriver;
}

IMPLEMENT_CLASS(UDemoPlayPendingLevel);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

