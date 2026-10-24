/*=============================================================================
	UnGame.cpp: Unreal game engine.
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

#include "EnginePrivate.h"
#include "UnNet.h"
#include "UnLinker.h"

#include "xForceFeedback.h" // jdf

/*-----------------------------------------------------------------------------
	Object class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UGameEngine);
IMPLEMENT_CLASS(UCrosshairPack);
IMPLEMENT_CLASS(APrecacheHack);
IMPLEMENT_CLASS(AMaplistManagerBase);

const int XBoxRelaunch = 0; // sjs --- enables rebooting of xbox between level loads
extern int QueueScreenShot; // sjs - label hack

/*-----------------------------------------------------------------------------
	cleanup!!
-----------------------------------------------------------------------------*/

void UGameEngine::PaintProgress( AVignette* Vignette, FLOAT Progress )
{
	guard(PaintProgress);

    if( !Client || Client->Viewports.Num()==0 )
        return;

	UViewport* Viewport=Client->Viewports(0);

    if( !Viewport || !Viewport->Canvas || !Viewport->RI )
        return;

    if( !Viewport->Lock() ) // sjs - no render device available (minimized)
        return;

    Viewport->Canvas->Update();
	Viewport->RI->Clear(0,FColor(0,0,0),1,1.0f);

	if (Vignette)
		Vignette->eventDrawVignette( Client->Viewports(0)->Canvas, Progress );
	else
		Draw(Viewport);

    if ( Viewport->Canvas->pCanvasUtil )
        Viewport->Canvas->pCanvasUtil->Flush();

    Viewport->Unlock();
	Viewport->Present();

	unguard;
}

INT UGameEngine::ChallengeResponse( INT Challenge )
{
	guard(UGameEngine::ChallengeResponse);
	return (Challenge*237) ^ (0x93fe92Ce) ^ (Challenge>>16) ^ (Challenge<<16);
	unguard;
}

void UGameEngine::UpdateConnectingMessage()
{
	guard(UGameEngine::UpdateConnectingMessage);
	if( GPendingLevel && Client && Client->Viewports.Num() )
	{
		if( Client->Viewports(0)->Actor->ProgressTimeOut<Client->Viewports(0)->Actor->Level->TimeSeconds )
		{
			TCHAR Msg1[256], Msg2[256];
			if( GPendingLevel->DemoRecDriver )
			{
				appSprintf( Msg1, TEXT("") );
				appSprintf( Msg2, *GPendingLevel->URL.Map );
			}
			else
			{
				appSprintf( Msg1, LocalizeProgress(TEXT("ConnectingText"),TEXT("Engine")) );
				appSprintf( Msg2, LocalizeProgress(TEXT("ConnectingURL"),TEXT("Engine")), *GPendingLevel->URL.Protocol, *GPendingLevel->URL.Host, *GPendingLevel->URL.Map );
			}

			if ( !GUIActive() )
				SetProgress( TEXT(""), Msg1, Msg2, 60.f );
		}
	}
	unguard;
}
void UGameEngine::BuildServerMasterMap( UNetDriver* NetDriver, ULevel* InLevel )
{
	guard(UGameEngine::BuildServerMasterMap);
	check(NetDriver);
	check(InLevel);
	BeginLoad();
	{
		// Init LinkerMap.
		check(InLevel->GetLinker());
		NetDriver->MasterMap->AddLinker( InLevel->GetLinker() );

		// Load server-required packages.
		for( INT i=0; i<ServerPackages.Num(); i++ )
		{
			debugf( NAME_DevNet, TEXT("Server Package: %s"), *ServerPackages(i) );
			ULinkerLoad* Linker = GetPackageLinker( NULL, *ServerPackages(i), LOAD_NoFail, NULL, NULL );
			if( NetDriver->MasterMap->AddLinker( Linker )==INDEX_NONE )
				debugf( NAME_DevNet, TEXT("   (server-side only)") );
		}

		// Add GameInfo's package to map.
		check(InLevel->GetLevelInfo());
		check(InLevel->GetLevelInfo()->Game);
		check(InLevel->GetLevelInfo()->Game->GetClass()->GetLinker());
		NetDriver->MasterMap->AddLinker( InLevel->GetLevelInfo()->Game->GetClass()->GetLinker() );

		// Precompute linker info.
		NetDriver->MasterMap->Compute();
	}
	EndLoad();
	unguard;
}

/*-----------------------------------------------------------------------------
	Game init and exit.
-----------------------------------------------------------------------------*/

//
// Construct the game engine.
//
UGameEngine::UGameEngine()
: LastURL(TEXT(""))
{}

//!!! HACK!
static UBOOL GOverrideMainMenu = 0;

//
// Initialize the game engine.
//
void UGameEngine::Init()
{
	guard(UGameEngine::Init);
	check(sizeof(*this)==GetClass()->GetPropertiesSize());

	// Call base.
	UEngine::Init();

	// Init variables.
	GLevel = NULL;

	// Delete temporary files in cache.
	appCleanFileCache();

	// If not a dedicated server.
	if( GIsClient )
	{	
		// Init client.
		UClass* ClientClass = StaticLoadClass( UClient::StaticClass(), NULL, TEXT("ini:Engine.Engine.ViewportManager"), NULL, LOAD_NoFail, NULL );
		Client = ConstructObject<UClient>( ClientClass );
		Client->Init( this );

		// Init Render Device
		UClass* RenDevClass = StaticLoadClass( URenderDevice::StaticClass(), NULL, TEXT("ini:Engine.Engine.RenderDevice"), NULL, LOAD_NoFail, NULL );
		GRenDev = ConstructObject<URenderDevice>( RenDevClass );
		GRenDev->Init();
	}
    
	// Apply [ufront.Shadows] to the ShadowProjector class default before any level (and so any pawn and
	// any shadow) exists; Engine.u is loaded by here. UnShadowProjector.cpp: the distance has to reach the
	// actor, not just the fade.
	{
		extern void ufrontApplyShadowCfg();
		ufrontApplyShadowCfg();
	}

#ifdef WITH_KARMA
    KInitGameKarma(); // Init physics.
#endif
#ifdef WITH_LIPSINC
	GLIPSincInitGame(); // Init LIPSinc system.
#endif

	// Load the entry level.
	FString Error;

	// Add code to load Packages.MD5

	if( GUfrontLog )
		printf("[ENGINE-DBG] About to load Packages.md5\n");

#ifdef __EMSCRIPTEN__
	// Skip MD5 package validation on the web: the host serves the packages, and the validation file is
	// not loaded through the web file system.
	MD5Package = NULL;
	if( GUfrontLog )
		printf("[ENGINE-DBG] Skipped Packages.md5 on Emscripten\n");
#else
	MD5Package = LoadPackage( NULL, TEXT("Packages.md5"), 0 );
#endif

	if (!MD5Package)
	{
#ifndef __EMSCRIPTEN__
        appErrorf(LocalizeError(TEXT("FailedMD5Load"),TEXT("Engine")), TEXT("Packages.md5"));
#else
        if( GUfrontLog )
            printf("[ENGINE-DBG] MD5Package is NULL (expected on Emscripten)\n");
#endif
	}
    else
    {
	    // Build the PackageValidation Array for quick lookup
	    for( FObjectIterator It; It; ++It )
	    {
		    UPackageCheckInfo *Info = Cast<UPackageCheckInfo>(*It);
		    if(Info && Info->IsIn( MD5Package ) )
			    PackageValidation.AddItem(Info);
	    }
    }

	DefaultMD5();

	FString SK;
	Parse(appCmdLine(), TEXT("BF="), SK);
	if ( SK != TEXT("") )
	{
		appStrcpy(GBFKey, *SK);
	}


	// Determine if cheat protection should be used.

	bCheatProtection = !(ParseParam( appCmdLine(), TEXT("nocheat") ) );	
	if (!bCheatProtection)
		GWarn->Logf(TEXT("Cheat protection disabled"));

	if( Client )
	{
		if( GUfrontLog )
			printf("[ENGINE-DBG] Loading Entry map...\n");
		if( !LoadMap( FURL(TEXT("Entry")), NULL, NULL, Error ) )
		{
			printf("[ENGINE-DBG] Entry map failed: %s\n", TCHAR_TO_ANSI(*Error));
			appErrorf( LocalizeError(TEXT("FailedBrowse"),TEXT("Engine")), TEXT("Entry"), *Error );
		}
		if( GUfrontLog )
			printf("[ENGINE-DBG] Entry map loaded OK\n");
		Exchange( GLevel, GEntry );
	}

	// Create default URL.
	FURL DefaultURL;
	DefaultURL.LoadURLConfig( TEXT("DefaultPlayer"), TEXT("User") );

	// Enter initial world.
	TCHAR Parm[4096]=TEXT("");
	const TCHAR* Tmp = appCmdLine();
	UBOOL SkippedEntry = 1; // gam
	if
	(	!ParseToken( Tmp, Parm, ARRAY_COUNT(Parm), 0 )
	||	(appStricmp(Parm,TEXT("SERVER"))==0 && !ParseToken( Tmp, Parm, ARRAY_COUNT(Parm), 0 ))
	||	Parm[0]=='-'
	// ...and a global option (`Key=Value`) is not a map either (appIsGlobalOptionToken).
	||	appIsGlobalOptionToken(Parm) )
	{
		appStrcpy( Parm, *FURL::DefaultLocalMap );
		SkippedEntry = 0;
    }
	FURL URL( &DefaultURL, Parm, TRAVEL_Partial );
	if( !URL.Valid )
		appErrorf( LocalizeError(TEXT("InvalidUrl"),TEXT("Engine")), Parm );

	UBOOL Success = Browse( URL, NULL, Error );

	// If waiting for a network connection, go into the starting level.
	if( !Success && Error==TEXT("") && appStricmp( Parm, *FURL::DefaultNetBrowseMap )!=0 )
		Success = Browse( FURL(&DefaultURL,*FURL::DefaultNetBrowseMap,TRAVEL_Partial), NULL, Error );

	// Handle failure.
	if( !Success )
#if UE_FAIL_SOFT_LOAD
	{
		// A bad startup map is not fatal (on the web appErrorf's throw could not even cross the
		// Asyncify frames to the guarded one). Entry is already loaded, so fall back to the menu
		// and let the player pick a valid map or host from the UI.
		debugf( TEXT("Startup map '%s' failed to load (%s) — falling back to the menu"), Parm, *Error );
		if( !GLevel )
			GLevel = GEntry;
	}
#else
		appErrorf( LocalizeError(TEXT("FailedBrowse"),TEXT("Engine")), Parm, *Error );
#endif

	// Open initial Viewport.
	if( Client )
	{
		// Init input.!!Temporary
		UInput::StaticInitInput();

        // Create the InteractionMaster

		UClass* IMClass = StaticLoadClass(UInteractionMaster::StaticClass(), NULL, TEXT("engine.InteractionMaster"), NULL, LOAD_NoFail, NULL);
		Client->InteractionMaster = ConstructObject<UInteractionMaster>(IMClass);

		// Setup callback to the client

		Client->InteractionMaster->Client = Client;

		// Display Copyright Notice

		Client->InteractionMaster->DisplayCopyright();

		// Create viewport.
		UViewport* Viewport = Client->NewViewport( NAME_None );

		// Add code to Create the Menu System here				

		UInteraction *GUIController = Client->InteractionMaster->eventAddInteraction(TEXT("ini:Engine.Engine.GUIController"),NULL);

		if (GUIController)
		{
			GUIController->ViewportOwner = Viewport;
			if ( Cast<UBaseGUIController>(GUIController) != NULL )		
			{
				Cast<UBaseGUIController>(GUIController)->eventInitializeController();	// Initialize it
				Viewport->GUIController = Cast<UBaseGUIController>(GUIController);
				Client->InteractionMaster->BaseMenu = GUIController;
			}
		}
		else
			appErrorf(TEXT("Could not spawn a GUI Controller!"));

		// Create the Console

		UInteraction *Console = Client->InteractionMaster->eventAddInteraction(TEXT("ini:Engine.Engine.Console"),NULL);

		if (!Console)
			appErrorf(TEXT("Error creating console! Please check the Console line of the [Engine.Engine] section of the UT2004.ini file."));

        Client->InteractionMaster->Console = Console;
        Console->ViewportOwner = Viewport;	
		Viewport->Console = Console;

		// Create the stream player
		if ( !Client->InteractionMaster->eventAddInteraction(TEXT("ini:Engine.Engine.StreamPlayer"), Viewport) )
			debugf(TEXT("Error creating music player! Please check the StreamPlayer line of the [Engine.Engine] section of the UT2004.ini file."));

		Cast<UConsole>(Console)->eventViewportInitialized();	// Let the console know it has a viewport
		// Look for changes to the ini file from the command line
		TCHAR TMainMenuClass[1024]=TEXT("");
		if( Parse( appCmdLine(), TEXT("-MainMenu="), TMainMenuClass, 1024 ) )
		{
			GOverrideMainMenu = 1;
			MainMenuClass = TMainMenuClass;
		}

		// Initialize Audio.
		InitAudio();
		if( Audio )
			Audio->SetViewport( Viewport );

		// Spawn play actor.
		FString Error;
		APlayerController* PlayerController = GLevel->SpawnPlayActor( Viewport, ROLE_SimulatedProxy, URL, Error );

        if( !PlayerController)
			appErrorf( TEXT("%s"), *Error );

		Viewport->Input->Init( Viewport );
		Viewport->OpenWindow( 0, 0, (INT) INDEX_NONE, (INT) INDEX_NONE, (INT) INDEX_NONE, (INT) INDEX_NONE );
		GLevel->DetailChange(
			Viewport->RenDev->SuperHighDetailActors ? DM_SuperHigh :
			Viewport->RenDev->HighDetailActors ? DM_High :
			DM_Low
			);

		// jdf ---
		if( GIsClient )
		{
			if(	PlayerController->bEnableWeaponForceFeedback
			||	PlayerController->bEnablePickupForceFeedback
			||	PlayerController->bEnableDamageForceFeedback
			||	PlayerController->bEnableGUIForceFeedback
			)
				InitForceFeedback(hInstance, Viewport->GetWindow());
			else
				ExitForceFeedback();

			PlayerController->bForceFeedbackSupported = (UBOOL)GForceFeedbackAvailable;

			// Load speech recognition grammar.
			GLevel->GetLevelInfo()->Game->eventSetGrammar();		
			Cast<UBaseGUIController>(GUIController)->eventAutoLoadMenus();			// Load Inital Menus
		}
		// --- jdf
	}
	debugf( NAME_Init, TEXT("Game engine initialized") );

	unguard;
}

//
// Game exit.
//
void UGameEngine::Destroy()
{
	guard(UGameEngine::Destroy);

	// Game exit.
	if( GPendingLevel )
		CancelPending();
	GLevel = NULL;
	debugf( NAME_Exit, TEXT("Game engine shut down") );

#ifdef WITH_KARMA
    KGData->bShutdownPending = 1;
    KTermGameKarma();
#endif

#ifdef WITH_LIPSINC
	GLIPSincShutdownGame();
#endif

	Super::Destroy();
	unguard;
}

//
// Progress text.
//
struct SetProgress_Params
{
    FString Str1;
    FString Str2;
};

void UGameEngine::SetProgress( const TCHAR* CmdStr,  const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds )
{
	guard(UGameEngine::SetProgress);

	// The viewport can exist with a NULL Actor (a failed travel tears the pawn down), and SetProgress
	// dereferences it below: bail with no live actor.
	if( !Client || !Client->Viewports.Num() || !Client->Viewports(0)->Actor )
	    return;
	APlayerController* Actor = Client->Viewports(0)->Actor;

	if( Seconds < 0 )
	    Seconds = ((APlayerController*)((APlayerController::StaticClass())->GetDefaultObject()))->ProgressTimeOut;

	if ( appStrcmp(CmdStr,TEXT("")) )
		Actor->eventProgressCommand(CmdStr,Str1,Str2);
	else
	{
		Actor->eventSetProgressMessage(0, Str1, FColor(255,255,255));
		Actor->eventSetProgressMessage(1, Str2, FColor(255,255,255));
		Actor->eventSetProgressTime(Seconds);
	}
	unguard;
}

UBOOL UGameEngine::GUIActive( const UClient* InClient ) const
{
	guard(UGameEngine::GUIActive);

	if ( !InClient )
		InClient = Client;

	if ( !InClient )
		return 0;

	if ( InClient->Viewports.Num() <= 0 )
		return 0;

	const UViewport* Viewport = ConstCast<UViewport>(InClient->Viewports(0));
	if ( Viewport == NULL )
		return 0;

	if ( Viewport->GUIController == NULL )
		return 0;

	return Viewport->GUIController->bActive;

	unguard;
}

/*-----------------------------------------------------------------------------
	Command line executor.
-----------------------------------------------------------------------------*/

//
// This always going to be the last exec handler in the chain. It
// handles passing the command to all other global handlers.
//
// WHEREAMI / TELEPORT <x> <y> <z> (UnCheats.cpp) and the frame rate in the log, [Engine.GameEngine]
// FpsLogSeconds / FPSLOG (UnFpsLog.cpp).
ENGINE_API UBOOL UfrontCheatExec( UEngine* Engine, APlayerController* PC, const TCHAR* Cmd, FOutputDevice& Ar );
ENGINE_API void UfrontFpsLogTick( UEngine* Engine );
ENGINE_API UBOOL UfrontFpsLogExec( const TCHAR* Cmd, FOutputDevice& Ar );

UBOOL UGameEngine::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(UGameEngine::Exec);
	const TCHAR* Str=Cmd;
	// The web's ue_exec_command reaches this engine only (never the viewport or the player); the typed console
	// takes the cheats in UPlayer::Exec, before the scripts.
	if( UfrontCheatExec( this, NULL, Cmd, Ar ) )
		return 1;
	if( UfrontFpsLogExec( Cmd, Ar ) )
		return 1;
	if( ParseCommand( &Str, TEXT("OPEN") ) )
	{
		FString Error;

		if ( appStrnicmp(Str,TEXT("http://"),7) )
		{

			if( Client && Client->Viewports.Num() )
			{
				Client->Viewports(0)->GUIController->eventCloseAll(0,1);
#ifdef __EMSCRIPTEN__
				// WebRTC connect: use TRAVEL_Absolute so "room?connect" is parsed
				// as an absolute URL. TRAVEL_Partial resolves relative to the current
				// map, which breaks the ?connect redirect in Browse.
				ETravelType TravelType = TRAVEL_Partial;
				FString StrCheck(Str);
				if( StrCheck.InStr(TEXT("connect")) >= 0 )
					TravelType = TRAVEL_Absolute;
				SetClientTravel( Client->Viewports(0), Str, 0, TravelType );
#else
				SetClientTravel( Client->Viewports(0), Str, 0, TRAVEL_Partial );
#endif
			}
			else
			if( !Browse( FURL(&LastURL,Str,TRAVEL_Partial), NULL, Error ) && Error!=TEXT("") )
				Ar.Logf( TEXT("Open failed: %s"), *Error );
		}
		else if( Client && Client->Viewports.Num() )
			SetClientTravel( Client->Viewports(0), Str, 0, TRAVEL_Absolute );

		return 1;
	}
    else if( ParseCommand( &Str, TEXT("NAMECOUNT") ) )
    {
        Ar.Logf( TEXT("Name count is: %d"), FName::GetMaxNames() );
        return 1;
    }
	else if( ParseCommand( &Str, TEXT("START") ) || ParseCommand( &Str, TEXT("MAP") )) // gam
	{
		FString Error;
		if( Client && Client->Viewports.Num() )
		{
			if ( appStrnicmp(Str,TEXT("http://"),7) )
				Client->Viewports(0)->GUIController->eventCloseAll(0,1);
			SetClientTravel( Client->Viewports(0), Str, 0, TRAVEL_Absolute );
		}
		else
		if( !Browse( FURL(&LastURL,Str,TRAVEL_Absolute), NULL, Error ) && Error!=TEXT("") )
			Ar.Logf( TEXT("Start failed: %s"), *Error );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("SERVERTRAVEL") ) && (GIsServer && !GIsClient) )
	{
		GLevel->GetLevelInfo()->eventServerTravel(Str,0);
		return 1;
	}
	else if( (GIsServer && !GIsClient) && ParseCommand( &Str, TEXT("SAY") ) )
	{
		GLevel->GetLevelInfo()->Game->eventBroadcast(NULL, Str, NAME_None);
		return 1;
	}
	else if( ParseCommand(&Str, TEXT("DISCONNECT")) )
	{
		FString Error;

		// Remove ?Listen parameter, if it exists  --  rjp
		LastURL.RemoveOption( TEXT("Listen") );
		LastURL.RemoveOption( TEXT("LAN") ) ;

		FString FailedURL = TEXT("?disconnect");
        
        while( appIsSpace( *Str ) )
            Str++;
            
        if( *Str )
        {
            FailedURL += TEXT("?");
            FailedURL += Str;
        }
        
		if( GIsSoaking )
		    UObject::StaticExec( TEXT("OBJ LIST"), *GLog );
		
		if( Client && Client->Viewports.Num() )
		{
#ifdef _XBOX
            Client->Exec( TEXT("E3-WRITEPLAYER") ); // sjs - E3
#endif
			if( GLevel && GLevel->NetDriver && GLevel->NetDriver->ServerConnection && GLevel->NetDriver->ServerConnection->Channels[0] )
			{
				GLevel->NetDriver->ServerConnection->Channels[0]->Close();
				GLevel->NetDriver->ServerConnection->FlushNet();
			}
			if( GPendingLevel && GPendingLevel->NetDriver && GPendingLevel->NetDriver->ServerConnection && GPendingLevel->NetDriver->ServerConnection->Channels[0] )
			{
				GPendingLevel->NetDriver->ServerConnection->Channels[0]->Close();
				GPendingLevel->NetDriver->ServerConnection->FlushNet();
			}

			SetClientTravel( Client->Viewports(0), *FailedURL, 0, TRAVEL_Absolute );
		}
		else
 		    if( !Browse( FURL(&LastURL,*FailedURL,TRAVEL_Absolute), NULL, Error ) && Error!=TEXT("") )
			    Ar.Logf( TEXT("Disconnect failed: %s"), *Error );
		return 1;
	}
	else if( ParseCommand(&Str, TEXT("RECONNECT")) )
	{
		FString Error;
		if( Client && Client->Viewports.Num() )
		{
			if ( GLevel && GLevel->DemoRecDriver && GLevel->DemoRecDriver->ServerConnection )
			{
				debugf(TEXT("Reconnect during demo playback - go to entry"));
				SetClientTravel( Client->Viewports(0), TEXT("?entry"), 0, TRAVEL_Absolute );
				return 1;
			}
			Client->Viewports(0)->GUIController->eventCloseAll(0,1);
			SetClientTravel( Client->Viewports(0), *LastURL.String(), 0, TRAVEL_Absolute );
		}
		else
		if( !Browse( FURL(LastURL), NULL, Error ) && Error!=TEXT("") )
			Ar.Logf( TEXT("Reconnect failed: %s"), *Error );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("EXIT")) || ParseCommand(&Cmd,TEXT("QUIT")))
	{
		if( GLevel && GLevel->NetDriver && GLevel->NetDriver->ServerConnection && GLevel->NetDriver->ServerConnection->Channels[0] )
		{
			GLevel->NetDriver->ServerConnection->Channels[0]->Close();
			GLevel->NetDriver->ServerConnection->FlushNet();
		}
		if( GPendingLevel && GPendingLevel->NetDriver && GPendingLevel->NetDriver->ServerConnection && GPendingLevel->NetDriver->ServerConnection->Channels[0] )
		{
			GPendingLevel->NetDriver->ServerConnection->Channels[0]->Close();
			GPendingLevel->NetDriver->ServerConnection->FlushNet();
		}

        if( GLevel && GLevel->GetLevelInfo() && GLevel->GetLevelInfo()->NetMode != NM_Client )
            GLevel->GetLevelInfo()->Game->eventGameEnding();

		// Shut down any demos that might be recording

		if (GLevel->DemoRecDriver != NULL)
		{
			GLevel->DemoRecDriver->Exec( TEXT("stopdemo"), *GLog );
		}


		Ar.Log( TEXT("Closing by request") );
		appRequestExit( 0 );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("GETCURRENTTICKRATE") ) )
	{
		Ar.Logf( TEXT("%f"), CurrentTickRate );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("GETMAXTICKRATE") ) )
	{
		Ar.Logf( TEXT("%f"), GetMaxTickRate() );
		return 1;
	}
	else if( ParseCommand( &Str, TEXT("GSPYLITE") ) )
	{
		FString Error;
		appLaunchURL( TEXT("GSpyLite.exe"), TEXT(""), &Error );
		return 1;
	}
	else if( ParseCommand(&Str,TEXT("SAVEGAME")) )
	{
		if( appIsDigit(Str[0]) )
			SaveGame( appAtoi(Str) );
		return 1;
	}
	else if( ParseCommand( &Cmd, TEXT("CANCEL") ) )
	{
		static UBOOL InCancel = 0;
		if( !InCancel )	
		{
			//!!Hack for broken Input subsystem.  JP.
			//!!Inside LoadMap(), ResetInput() is called,
			//!!which can retrigger an Exec call.
			InCancel = 1;
			if( GPendingLevel )
			{
				if( GPendingLevel->TrySkipFile() )
				{
					InCancel = 0;
					return 1;
				}
				if ( GLevel == GEntry && !GUIActive() )
					SetProgress( *FString::Printf(TEXT("menu:%s"),*DisconnectMenuClass),LocalizeProgress(TEXT("CancelledConnect"),TEXT("Engine")), TEXT("") );
			}
			else
			{
				// rjp -- 02/24/04
				// Display the disconnect menu if sitting at entry level with no pending level
				if ( GLevel == GEntry && !GUIActive() )
				{
					SetProgress( *FString::Printf(TEXT("menu:%s"), *DisconnectMenuClass), LocalizeProgress(TEXT("CancelledConnect"),TEXT("Engine")),TEXT("noreconnect") );
				}
				// -- rjp
				else
				{
					SetProgress( TEXT(""), TEXT(""), TEXT(""), 0.f );
				}
			}
			CancelPending();
			InCancel = 0;
		}
		return 1;
	}
	else if ( ParseCommand(&Cmd,TEXT("UNSUPPRESS")) )
	{
		FString SuppressName;

		if ( ParseToken(Cmd,SuppressName,0) )
		{
			for ( INT i = 0; i < GSys->Suppress.Num(); i++ )
				if ( GSys->Suppress(i) == *SuppressName )
				{
					GSys->Suppress(i).ClearFlags(RF_Suppress);
					Ar.Logf(TEXT("%s no longer suppressed!"), *GSys->Suppress(i));
					return 1;
				}

			Ar.Logf(TEXT("No entries named '%s' found in Suppress array!"), *SuppressName);
			return 1;
		}

		Ar.Logf(TEXT("You must specify a name to unsuppress"));
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SUPPRESS")) )
	{
		FString SuppressName;
		if ( ParseToken(Cmd,SuppressName,0) )
		{
			for ( INT i = 0; i < GSys->Suppress.Num(); i++ )
			{
				if ( SuppressName == *GSys->Suppress(i) )
				{
					if ( GSys->Suppress(i).GetFlags() & RF_Suppress )
					{
						Ar.Logf(TEXT("%s is already being suppressed!"), *SuppressName);
						return 1;
					}
					else
					{
						GSys->Suppress(i).SetFlags(RF_Suppress);
						Ar.Logf(TEXT("'%s' will no longer appear in the log."), *SuppressName);
						return 1;
					}
				}
			}

			FName* NewSuppressName = new(GSys->Suppress) FName(*SuppressName, FNAME_Add);
			if ( NewSuppressName )
			{
				NewSuppressName->SetFlags(RF_Suppress);

				// By default, the new Suppress entry won't be stored in the ini file.
				if ( ParseCommand(&Cmd,TEXT("SAVE")) )
					GSys->SaveConfig();
			}

			return 1;
		}
		else
		{
			Ar.Logf(TEXT("You must specify a name to suppress!"));
			return 1;
		}
	}
    else if( ParseCommand(&Cmd,TEXT("SOUND_REBOOT"))  )
    {
		UViewport* Viewport = NULL;
        if( Audio )
        {
            Viewport = Audio->GetViewport();
            delete Audio;
			Audio = NULL;
        }

		if ( !Viewport )
		{
			if ( Client && Client->Viewports.Num() )
				Viewport = Client->Viewports(0);
		}
		if (Viewport)
		{
			InitAudio();
			if( Audio )
	            Audio->SetViewport( Viewport );
			
			// Reload existing grammar.
			Viewport->LoadSRGrammar( 1, FString(TEXT("None")) );
		}
		return 1;
    }
	else if( GLevel && GLevel->Exec( Cmd, Ar ) )
	{
		return 1;
	}
	else if( GLevel && GLevel->GetLevelInfo()->Game && GLevel->GetLevelInfo()->Game->ScriptConsoleExec(Cmd,Ar,NULL) )
	{
		return 1;
	}
	else
	{
		// disallow set of actor properties if network game
		const TCHAR *Str = Cmd;
		if ( ParseCommand(&Str,TEXT("SET")) )
		{
			TCHAR ClassName[256];
			UClass* Class;
			if
			(	ParseToken( Str, ClassName, ARRAY_COUNT(ClassName), 1 )
			&&	(Class=FindObject<UClass>( ANY_PACKAGE, ClassName))!=NULL )
			{
				TCHAR PropertyName[256];
				UProperty* Property;
				if
				(	ParseToken( Str, PropertyName, ARRAY_COUNT(PropertyName), 1 )
				&&	(Property=FindField<UProperty>( Class, PropertyName))!=NULL )
				{
					if ( Class->IsChildOf(AActor::StaticClass()) && !Class->IsChildOf(AGameInfo::StaticClass()) )
					{
						// While your netmode is NM_Client, GameInfo is the only subclass of Actor that accepts 'set' commands
						if ( GLevel && (GLevel->GetLevelInfo()->NetMode == NM_Client) )
						{
							Ar.Logf(TEXT("Sorry, using the console to modify the value of %s.%s is not allowed during network play!"), Property->GetOwnerClass()->GetName(), Property->GetName());

							// If this property wasn't a configurable property, invalidate CD key
							// Not sure if this is necessary, since the 'set' command isn't even processed
							if ( !((Property->PropertyFlags & CPF_Config)||(Property->PropertyFlags&CPF_GlobalConfig)) )
							{
								guard("IllegalSetUse");		
								GCDKS = 1;
								unguard;
							}

							return 1;
						}

						// Otherwise, if this property isn't a config property, invalidate CD key, but allow the command
						// Should not affect servers, but forces client to restart game before joining online match
						else if ( !((Property->PropertyFlags & CPF_Config)||(Property->PropertyFlags&CPF_GlobalConfig)) )
							GCDKS = 1; 
					}

					// In non-Actor classes, only allow configurable properties to be changed
					// No penalty - just politely fail
					else if ( !((Property->PropertyFlags & CPF_Config)||(Property->PropertyFlags&CPF_GlobalConfig)) )
					{
						Ar.Logf(TEXT("Sorry, using the console to modify the value of %s.%s is not allowed!"), Property->GetOwnerClass()->GetName(), Property->GetName());
						return 1;
					}
				}
			}
		}

		if( UEngine::Exec( Cmd, Ar ) )
			return 1;
		else
			return 0;
	}

	unguard;
}

/*-----------------------------------------------------------------------------
	Serialization.
-----------------------------------------------------------------------------*/

//
// Serializer.
//
void UGameEngine::Serialize( FArchive& Ar )
{
	guard(UGameEngine::Serialize);
	Super::Serialize( Ar );

	Ar << GLevel << GEntry << GPendingLevel;

	unguardobj;
}

/*-----------------------------------------------------------------------------
	Game entering.
-----------------------------------------------------------------------------*/

//
// Cancel pending level.
//
void UGameEngine::CancelPending()
{
	guard(UGameEngine::CancelPending);
	if( GPendingLevel )
	{
		debugf( NAME_DevNet, TEXT("CancelPending: closing the pending level's control channel") );
		if( GPendingLevel->NetDriver && GPendingLevel->NetDriver->ServerConnection && GPendingLevel->NetDriver->ServerConnection->Channels[0] )
		{
			GPendingLevel->NetDriver->ServerConnection->Channels[0]->Close();
			GPendingLevel->NetDriver->ServerConnection->FlushNet();
		}
		delete GPendingLevel;
		GPendingLevel = NULL;
	}
	unguard;
}

//
// Match Viewports to actors.
//
static void MatchViewportsToActors( UClient* Client, ULevel* Level, const FURL& URL, const TCHAR* MenuClassName )
{
	guard(MatchViewportsToActors);

	for( INT i=0; i<Client->Viewports.Num(); i++ )
	{
		FString Error;
		UViewport* Viewport = Client->Viewports(i);
		debugf( NAME_Log, TEXT("Spawning new actor for Viewport %s"), Viewport->GetName() );

		APlayerController* PlayerController = Level->SpawnPlayActor( Viewport, ROLE_SimulatedProxy, URL, Error );

        if( !PlayerController)
			appErrorf( TEXT("%s"), *Error );

		// Devnet diagnostic: the window where Viewport->Actor is NULL closes here (pairs with the dissociate line).
		debugf( NAME_DevNet, TEXT("[NET-DIAG] MatchViewportsToActors: viewport %s -> actor %s (0x%08x, class %s) in level %s"),
			Viewport->GetName(), PlayerController->GetName(), (DWORD)(PTRINT)PlayerController,
			PlayerController->GetClass()->GetName(), Level->GetName() );

		PlayerController->bForceFeedbackSupported = (UBOOL)GForceFeedbackAvailable;
		if ( appStrlen(MenuClassName) )
		{
			if ((!Viewport->GUIController->bActive ) || (GOverrideMainMenu))
			{
                GOverrideMainMenu = 0;
				debugf(TEXT("MatchViewportsToActors: %s"),MenuClassName);
				Viewport->GUIController->eventOpenMenu(MenuClassName,*URL.OptionString(),TEXT("MatchingViewport"));
			}
		}
	}
	unguardf(( TEXT("(%s)"), *Level->URL.Map ));
}

void UGameEngine::DefaultMD5()
{
	guard(GameEngine::DefaultMD5);
	
	// No Longer Needed

	unguard;
}

void UGameEngine::AddMD5(const TCHAR* GUID, const TCHAR* MD5, int Revision)
{
	guard(GameEngine::AddMD5);

	if( !MD5Package )
		return;

	for (INT i=0;i<PackageValidation.Num();i++)
	{
		if (!appStricmp(GUID, *PackageValidation(i)->PackageID) )
		{
			if (PackageValidation(i)->RevisionLevel < Revision)
			{
				debugf(TEXT("Updating Revision level for %s to %i"),GUID, Revision);
				PackageValidation(i)->RevisionLevel = Revision;
			}
			
			for (INT j=0;j<PackageValidation(i)->AllowedIDs.Num();j++)
			{
				if (!appStricmp(MD5, *PackageValidation(i)->AllowedIDs(j)) )
					return;		// Already in DB
			}

			debugf(NAME_DevNet,TEXT("Updating MD5 for %s [%s]"),GUID,MD5);
			new(PackageValidation(i)->AllowedIDs)FString( FString::Printf(TEXT("%s"),MD5));
			return;
		}
	}

	// Add everything

	debugf(NAME_DevNet,TEXT("Adding MD5 Entry for %s [%s]"),GUID,MD5);

	INT Index = PackageValidation.Num();
	PackageValidation.AddItem(ConstructObject<UPackageCheckInfo>(UPackageCheckInfo::StaticClass(),MD5Package,NAME_None,RF_Public));
	PackageValidation(Index)->PackageID = FString::Printf(TEXT("%s"),GUID);
	new(PackageValidation(Index)->AllowedIDs)FString(FString::Printf(TEXT("%s"),MD5));

	PackageValidation(Index)->Native = true;
	PackageValidation(Index)->RevisionLevel = Revision;

	unguard;
}


// 
// Save the MD5 database to disk

void UGameEngine::SaveMD5Database()
{
	guard(GameEngine::SaveMD5Database);

	if (MD5Package)
	{
		debugf(TEXT("Saving Packages.md5..."));
		SavePackage(MD5Package,NULL,RF_Public,TEXT("Packages.md5"),GWarn,NULL);
	}
	else
		debugf(TEXT("Could not save Packages.MD5"));

	unguard;
}

//
// Updates the MD5 database with any MD5's loaded in memory.

void UGameEngine::ServerUpdateMD5()
{

	guard(UGameEngine::ServerUpdateMD5);

	if( !MD5Package )
		return;

	// Check to see if any "Non-Approved" packages are laying around

	TArray<UObject*> ObjLoaders = UObject::GetLoaderList(); 

	int Found;
	for( INT i=0; i<ObjLoaders.Num(); i++ )
	{
		ULinker* Linker = CastChecked<ULinker>( ObjLoaders(i) );
		if ( true ) // Linker->LinksToCode() )
		{
			Found=-1;
			for (INT k=0;k<PackageValidation.Num();k++)
			{
				if ( !appStricmp(Linker->Summary.Guid.String(),*PackageValidation(k)->PackageID) )
				{
					Found = k;
					break;
				}
			}

			if (Found>=0)	// Check the MD5
			{
				INT l;
				for (l=0;l<PackageValidation(Found)->AllowedIDs.Num();l++)
				{
					if ( !appStricmp(*PackageValidation(Found)->AllowedIDs(l),*Linker->QuickMD5()) )
					{
						Found=-1;
						break;
					}
				}

				if ( Found>=0 )	// We need to add this id
				{
					new(PackageValidation(Found)->AllowedIDs)FString(Linker->QuickMD5());
				}
			}
			else	// Create a whole new entry
			{

				Found = PackageValidation.Num();
				PackageValidation.AddItem(ConstructObject<UPackageCheckInfo>(UPackageCheckInfo::StaticClass(),MD5Package,NAME_None,RF_Public));
				PackageValidation(Found)->PackageID = FString::Printf(TEXT("%s"),Linker->Summary.Guid.String());
				new(PackageValidation(Found)->AllowedIDs)FString(Linker->QuickMD5());
				PackageValidation(Found)->Native = false;
				PackageValidation(Found)->RevisionLevel = 0;
			}
				
		}

	}

	unguard;


}

//
// Browse to a specified URL, relative to the current one.
//
UBOOL UGameEngine::Browse( FURL URL, const TMap<FString,FString>* TravelInfo, FString& Error )
{
	guard(UGameEngine::Browse);
	Error = TEXT("");
	const TCHAR* Option;

	// Tear down voice chat.
//	if( Audio )
//		Audio->LeaveVoiceChat();

	// Convert .unreal link files.
	const TCHAR* LinkStr = TEXT(".unreal");//!!
	if( appStrstr(*URL.Map,LinkStr)-*URL.Map==appStrlen(*URL.Map)-appStrlen(LinkStr) )
	{
		debugf( TEXT("Link: %s"), *URL.Map );
		FString NewUrlString;
		if( GConfig->GetString( TEXT("Link")/*!!*/, TEXT("Server"), NewUrlString, *URL.Map ) )
		{
			// Go to link.
			URL = FURL( NULL, *NewUrlString, TRAVEL_Absolute );//!!
		}
		else
		{
			// Invalid link.
			guard(InvalidLink);
			Error = FString::Printf( LocalizeError(TEXT("InvalidLink"),TEXT("Engine")), *URL.Map );
			unguard;
			return 0;
		}
	}

	// Crack the URL.
	debugf( TEXT("Browse: %s"), *URL.String(0,1) );

	// Handle it.
	if( !URL.Valid )
	{
		// Unknown URL.
		guard(UnknownURL);
		Error = FString::Printf( LocalizeError(TEXT("InvalidUrl"),TEXT("Engine")), *URL.String() );
		unguard;
		return 0;
	}
	else if ( URL.HasOption(TEXT("failed")) || URL.HasOption(TEXT("disconnect")) || URL.HasOption(TEXT("closed")) || URL.HasOption(TEXT("entry")) )
	{
		// Handle failure URL.
		guard(FailedURL);
/*		
	    // gam ---
	    for( INT i=LastURL.Op.Num()-1; i>=0; i-- )
	    {
		    if( !appStrPrefix( *LastURL.Op(i), TEXT("SpectatorOnly=") ) )
		    {
			    LastURL.Op.Remove( i );
			    break;
			}
	    }
	    for( INT i=URL.Op.Num()-1; i>=0; i-- )
	    {
		    if( !appStrPrefix( *URL.Op(i), TEXT("SpecatorOnly=") ) )
		    {
			    URL.Op.Remove( i );
			    break;
			}
	    }
        // --- gam
*/
		LastURL.RemoveOption(TEXT("SpectatorOnly="));
		LastURL.RemoveOption(TEXT("Listen"));
		LastURL.RemoveOption(TEXT("LAN"));

		URL.RemoveOption(TEXT("SpectatorOnly="));
		URL.RemoveOption(TEXT("Listen"));
		URL.RemoveOption(TEXT("LAN"));

		debugf( NAME_Log, LocalizeError(TEXT("AbortToEntry"),TEXT("Engine")) );
		if( GLevel && GLevel!=GEntry )
        {
            GLevel->SetActorCollision( 0, 1 ); // gam
			ResetLoaders( GLevel->GetOuter(), 1, 0 );
        }
		NotifyLevelChange();

		GLevel = GEntry;
        if( GLevel )
        {
		    GLevel->GetLevelInfo()->LevelAction = LEVACT_None;
		    check(Client && Client->Viewports.Num());

			
			if (GPendingLevel)
				debugf(TEXT("GP=TRUE"));
			else
				debugf(TEXT("GP=FALSE"));

			if ( !URL.HasOption(TEXT("closed")) || !GPendingLevel)
			{
				const TCHAR* MenuClass = NULL;

				if (GLevel->GetLevelInfo()->Game->CurrentGameProfile && GLevel->GetLevelInfo()->Game->CurrentGameProfile->bInLadderGame)
					MenuClass = *SinglePlayerMenuClass;
				else
					MenuClass = *MainMenuClass;

				MatchViewportsToActors( Client, GLevel, URL, MenuClass );
			}
			else
			    MatchViewportsToActors( Client, GLevel, URL, TEXT("") );

		    if( Audio )
			    Audio->SetViewport( Audio->GetViewport() );
        }

		/* rjp -- this has been commented since Revision #1 of UnGame.cpp, circa ~v.777
		          However, a GC is necessary here in order to purge all network actors (MasterServerUplink, UdpGamespyQuery)
				  belonging to the previous map, as they will not release the ports they've bound until they're deleted -- rjp  */
		//CollectGarbage( RF_Native ); // Causes texture corruption unless you flush.
		// watch out for texture corruption resulting from this garbage collection, I guess -- rjp




		// 2/24/04 - there have been an fairly large number of crashes during garbage collection recently;
		//           In most cases, the GC that these crashes occurred in was the one that was getting called here.
		//           It's a good possibility that this GC is merely exposing a hidden bug, but until we can be sure
		//           it should remain disabled -- rjp
//		guard(UGameEngine::Browse::CollectGarbageAtDisconnect);
//		debugf(TEXT("Disconnected...requesting GC"));
//		CollectGarbage( RF_Native | (GIsEditor ? RF_Standalone : 0) );
//		unguard;


		if( URL.HasOption(TEXT("failed")) )
		{
			if ( !GPendingLevel/* && !GUIActive() */)
			{
				SetProgress( *FString::Printf(TEXT("menu:%s"),*DisconnectMenuClass),LocalizeError(TEXT("ConnectionFailed"),TEXT("Engine")), TEXT("") );
			}
		}

		unguard;
		return 1;
	}
	else if( URL.HasOption(TEXT("pop")) )
	{
		// Pop the hub.
		guard(PopURL);
		if( GLevel && GLevel->GetLevelInfo()->HubStackLevel>0 )
		{
			TCHAR Filename[256], SavedPortal[256];
			appSprintf( Filename, TEXT("%s") PATH_SEPARATOR TEXT("Game%i.usa"), *GSys->SavePath, GLevel->GetLevelInfo()->HubStackLevel-1 );
			appStrcpy( SavedPortal, *URL.Portal );
			URL = FURL( &URL, Filename, TRAVEL_Partial );
			URL.Portal = SavedPortal;
		}
		else return 0;
		unguard;
	}
	else if( URL.HasOption(TEXT("restart")) )
	{
		// Handle restarting.
		guard(RestartURL);
		URL = LastURL;
		unguard;
	}
	else if( (Option=URL.GetOption(TEXT("load="),NULL))!=NULL )
	{
		// Handle loadgame.
		guard(LoadURL);
		FString Error, Temp=FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("Save%i.usa?load"), *GSys->SavePath, appAtoi(Option) );
		debugf(TEXT("Loading save game %s"),*Temp);
		if( LoadMap(FURL(&LastURL,*Temp,TRAVEL_Partial),NULL,NULL,Error) )
		{
			// Copy the hub stack.
			INT i;
			for( i=0; i<GLevel->GetLevelInfo()->HubStackLevel; i++ )
			{
				TCHAR Src[256], Dest[256];//!!
				appSprintf( Src, TEXT("%s") PATH_SEPARATOR TEXT("Save%i%i.usa"), *GSys->SavePath, appAtoi(Option), i );
				appSprintf( Dest, TEXT("%s") PATH_SEPARATOR TEXT("Game%i.usa"), *GSys->SavePath, i );
				GFileManager->Copy( Src, Dest );
			}
			while( 1 )
			{
				Temp = FString::Printf( TEXT("%s") PATH_SEPARATOR TEXT("Game%i.usa"), *GSys->SavePath, i++ );
				if( GFileManager->FileSize(*Temp)<=0 )
					break;
				GFileManager->Delete( *Temp );
			}
			LastURL = GLevel->URL;
			
	        for( INT i=LastURL.Op.Num()-1; i>=0; i-- )
	        {
		        if( !appStrPrefix( *LastURL.Op(i), TEXT("Menu=") ) )
		        {
			        LastURL.Op.Remove( i );
			        break;
			    }
	        }
			return 1;
		}
		else return 0;
		unguard;
	}

	// Handle normal URL's.
#ifdef __EMSCRIPTEN__
	// WebRTC client connect: when the URL has a "connect" travel option, the "map" name is a room
	// name. Re-interpret it as a network Host so the engine takes the UNetPendingLevel path instead
	// of LoadMap.
	if( URL.IsLocalInternal() && URL.HasOption(TEXT("connect")) && URL.Map.Len() > 0 )
	{
		debugf( NAME_Log, TEXT("[WebRTC] Browse: treating '%s' as a WebRTC room name"), *URL.Map );
		URL.Host = URL.Map;
		URL.Map = TEXT("");
		// Fall through to the network URL path below
	}
#endif
	if( URL.IsLocalInternal() )
	{
		// Local map file.
		guard(LocalMapURL);
#ifdef _XBOX // sjs - reboot the Xbox when launching a new map
		if ( !XBoxRelaunch || appStrnicmp( *URL.Map, TEXT("Entry"), 5 ) == 0 || GLevel == NULL )
		{
		return LoadMap( URL, NULL, TravelInfo, Error )!=NULL;
		}
		else
		{
			FString launch = FString::Printf(TEXT("RELAUNCH %s"), *URL.String() );
			return Exec( *launch );
		}
#else
		UBOOL MapRet = LoadMap( URL, NULL, TravelInfo, Error )!=NULL;

		// Big Hack #2 - Remove BonusVehicles
		URL.RemoveOption(TEXT("BonusVehicles="));
		LastURL.RemoveOption(TEXT("BonusVehicles="));

		if (MapRet && GIsServer)
			ServerUpdateMD5();

		return MapRet;
#endif
		unguard;
	}
	else if( URL.IsInternal() && GIsClient )
	{
		// Network URL.
		guard(NetworkURL);
		debugf( NAME_DevNet, TEXT("Browse: network URL, Host='%s' Map='%s'"), *URL.Host, *URL.Map );
		if( GPendingLevel )
			CancelPending();
		GPendingLevel = new UNetPendingLevel( this, URL );
		if( !GPendingLevel->NetDriver )
		{
			debugf( NAME_Log, TEXT("Browse: the net driver's InitConnect failed: %s"), *GPendingLevel->Error );
			SetProgress( *FString::Printf(TEXT("menu:%s"),*DisconnectMenuClass), TEXT("Networking Failed"), *GPendingLevel->Error );
			delete GPendingLevel;
			GPendingLevel = NULL;
		}
		return 0;
		unguard;
	}
	else if( URL.IsInternal() )
	{
		// Invalid.
		guard(InvalidURL);
		Error = LocalizeError(TEXT("ServerOpen"),TEXT("Engine"));
		unguard;
		return 0;
	}
	else
	{
		// External URL.
		guard(ExternalURL);
		//Client->Viewports(0)->Exec(TEXT("ENDFULLSCREEN"));
		appLaunchURL( *URL.String(), TEXT(""), &Error );
		unguard;
		return 0;
	}
	unguard;
}

//
// Notify that level is changing
//
void UGameEngine::NotifyLevelChange()
{
	guard(UGameEngine::NotifyLevelChange);

	// Make sure cinematic view is turned off when the level changes
    if( Client && Client->Viewports.Num() )
		for( INT x = 0 ; x < Client->Viewports.Num() ; x++ )
			Client->Viewports(x)->bRenderCinematics = 0;
	
	if( Client && Client->Viewports.Num() )
	{
		UPlayer* Player = Cast<UPlayer>(Client->Viewports(0));
		if (Player->InteractionMaster)
		{
			Player->InteractionMaster->eventNotifyLevelChange(Player->InteractionMaster->GlobalInteractions);
			Player->InteractionMaster->eventNotifyLevelChange(Player->LocalInteractions);
		}

		// Turn off Movie Making at Map Change

		if (GUseFixedTimeStep)
		{
			GUseFixedTimeStep = 0;
			GFixedTimeStep = 0;
		}

		Client->Viewports(0)->MovieEncodeStop();
	}

    if ( Audio )
        Audio->Exec(TEXT("StopMusic"));

	// Force a demo stop on level change.

	if (GLevel->DemoRecDriver != NULL)
	{
		GLevel->DemoRecDriver->Exec( TEXT("stopdemo"), *GLog );
	}

	GDemoPlayback = (GPendingLevel) && (Cast<UDemoPlayPendingLevel>(GPendingLevel) != NULL);

	unguard;	
}

// Fixup a map
// hack to post release fix map actor problems without breaking compatibility
void UGameEngine::FixUpLevel()
{
	guard(UGameEngine::FixUpLevel);

	if ( appStricmp(GLevel->GetFullName(), TEXT("Level br-icefields.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up BR-IceFields"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && (appStricmp(Z->GetName(), TEXT("ZoneInfo0"))==0) )
			{
				Z->KillZ = -50.f;
				break;
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level br-de-elecfields.myLevel"))==0 || appStricmp(GLevel->GetFullName(), TEXT("Level ctf-de-elecfields.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up BR/CTF-DE-ElecFields"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AStaticMeshActor* S = Cast<AStaticMeshActor>(GLevel->Actors(i));
			if( S && ((appStricmp(S->GetName(), TEXT("StaticMeshActor73"))==0) || (appStricmp(S->GetName(), TEXT("StaticMeshActor74"))==0) ) )
			{
				S->SetCollision(false, false);
				S->bBlockActors					= false;
				S->bBlockKarma					= false;
				S->bBlockNonZeroExtentTraces	= false;
				S->bBlockZeroExtentTraces		= false;
				S->bCollideActors				= false;
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dm-antalus.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DM-Antalus"));

		const INT NumFixup			= 17;
		INT FixupMeshes[NumFixup]	= { 27, 142, 56, 90, 179, 87, 119, 128, 91, 139, 141, 126, 120, 144, 143, 60, 178 };
		FString	FixupNames[NumFixup];

		for( INT n=0; n<NumFixup; n++ )
			FixupNames[n] = FString::Printf(TEXT("%s%i"),TEXT("StaticMeshActor"),FixupMeshes[n]);

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AStaticMeshActor* S = Cast<AStaticMeshActor>(GLevel->Actors(i));
            if( S )
			{
				for( INT n=0; n<NumFixup; n++ )	
				{	
					if( appStricmp(S->GetName(), *FixupNames[n])==0 )
					{
						S->SetCollision(false, false);
						S->bCollideActors				= false;
						S->bCollideWorld				= false;
						S->bBlockActors					= false;
						S->bBlockZeroExtentTraces		= false;
						S->bBlockNonZeroExtentTraces	= false;
						S->bBlockKarma					= false;
					}
				}
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level ctf-december.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up CTF-December"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
			{
				if ( appStricmp(Z->GetName(), TEXT("ZoneInfo38"))==0 )
					Z->KillZ = -1950.f;
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,1000.f);
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level ctf-face3.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up CTF-Face3"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z )
				Z->bSoftKillZ = true;
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level ctf-citadel.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up CTF-Citadel"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z )
			{
				Z->bSoftKillZ = true;
				if ( appStricmp(Z->GetName(), TEXT("ZoneInfo1"))==0 )
					Z->KillZ = -2600.f;
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level ctf-twintombs.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up CTF-TwinTombs"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			APlayerStart* P = Cast<APlayerStart>(GLevel->Actors(i));
			if ( P && (appStricmp(P->GetName(), TEXT("PlayerStart11"))==0) )
			{
				P->TeamNumber = 1;
				break;
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level br-anubis.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up BR-Anubis"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && 
				((appStricmp(Z->GetName(), TEXT("ZoneInfo0"))==0) 
				|| (appStricmp(Z->GetName(), TEXT("ZoneInfo1"))==0)) )
			{
				Z->KillZ = 200.f;
				Z->bSoftKillZ = true;
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dm-tokaraforest.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DM-TokaraForest"));

		const INT NumFixup			= 11;
		INT FixupMeshes[NumFixup]	= { 241, 225, 92, 240, 109, 233, 242, 234, 245, 244, 243 };
		FString	FixupNames[NumFixup];

		for( INT n=0; n<NumFixup; n++ )
			FixupNames[n] = FString::Printf(TEXT("%s%i"),TEXT("StaticMeshActor"),FixupMeshes[n]);

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AStaticMeshActor* S = Cast<AStaticMeshActor>(GLevel->Actors(i));
            if( S )
			{
				for( INT n=0; n<NumFixup; n++ )	
				{	
					if( appStricmp(S->GetName(), *FixupNames[n])==0 )
					{
						S->SetCollision(false, false);
						S->bCollideActors				= false;
						S->bCollideWorld				= false;
						S->bBlockActors					= false;
						S->bBlockZeroExtentTraces		= false;
						S->bBlockNonZeroExtentTraces	= false;
						S->bBlockKarma					= false;
					}
				}
			}

			AxProcMesh* M = Cast<AxProcMesh>(GLevel->Actors(i));
			if ( M && M->bSpecialLit )
			{
				M->bSpecialLit = false;
				M->bHighDetail = true;
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level ctf-lostfaith.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up CTF-LostFaith"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
			{
				Z->bSoftKillZ = true;
				Z->KillZ = -400.f;
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,1000.f);
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level br-skyline.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up BR-Skyline"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AMover *M = Cast<AMover>(GLevel->Actors(i));
			if ( M  )
				M->RemoteRole = ROLE_None;
			else
			{
				ANavigationPoint * N = Cast<ANavigationPoint>(GLevel->Actors(i));
				if ( N )
				{
					if ( appStricmp(N->GetName(), TEXT("PathNode91"))==0 )
					{
						for ( INT i=0; i<N->PathList.Num(); i++ )
							if ( appStricmp(N->PathList(i)->End->GetName(), TEXT("PathNode66"))==0 )
							{
								N->PathList(i)->reachFlags =  N->PathList(i)->reachFlags | 128; //128 == R_PROSCRIBED
								break;
							}
					}
					else if ( appStricmp(N->GetName(), TEXT("PathNode56"))==0 )
					{
						for ( INT i=0; i<N->PathList.Num(); i++ )
							if ( appStricmp(N->PathList(i)->End->GetName(), TEXT("PathNode66"))==0 )
							{
								N->PathList(i)->reachFlags =  N->PathList(i)->reachFlags | 128;
								break;
							}
					}
					else if ( appStricmp(N->GetName(), TEXT("PathNode98"))==0 )
					{
						for ( INT i=0; i<N->PathList.Num(); i++ )
							if ( appStricmp(N->PathList(i)->End->GetName(), TEXT("PathNode54"))==0 )
							{
								N->PathList(i)->reachFlags =  N->PathList(i)->reachFlags | 128;
								break;
							}
					}
					else if ( appStricmp(N->GetName(), TEXT("PathNode59"))==0 )
					{
						for ( INT i=0; i<N->PathList.Num(); i++ )
							if ( appStricmp(N->PathList(i)->End->GetName(), TEXT("PathNode58"))==0 )
							{
								N->PathList(i)->reachFlags =  N->PathList(i)->reachFlags | 128;
								break;
							}
					}
					else if ( appStricmp(N->GetName(), TEXT("PathNode58"))==0 )
					{
						for ( INT i=0; i<N->PathList.Num(); i++ )
							if ( appStricmp(N->PathList(i)->End->GetName(), TEXT("PathNode59"))==0 )
							{
								N->PathList(i)->reachFlags =  N->PathList(i)->reachFlags | 128;
								break;
							}
					}
				}
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level ctf-chrome.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up CTF-Chrome"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,800.f);
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level ctf-geothermal.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up CTF-Geothermal"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AStaticMeshActor* S = Cast<AStaticMeshActor>(GLevel->Actors(i));
            if( S )
			{
				if ( appStricmp(S->GetName(), TEXT("StaticMeshActor17"))==0 )
				{
					S->bBlockNonZeroExtentTraces = true;
				}
			}
			else
			{
				AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
				if ( Z && !Cast<ASkyZoneInfo>(Z) )
				{
					Z->DistanceFogStart = ::Max(Z->DistanceFogStart,1000.f);
					Z->DistanceFogEnd = ::Max(Z->DistanceFogEnd,8000.f);
				}

				AAIScript* A = Cast<AAIScript>(GLevel->Actors(i));
				if ( A &&
					((appStricmp(A->GetName(), TEXT("UnrealScriptedSequence7"))==0)
					|| (appStricmp(A->GetName(), TEXT("UnrealScriptedSequence8"))==0) 
					|| (appStricmp(A->GetName(), TEXT("UnrealScriptedSequence10"))==0) 
					|| (appStricmp(A->GetName(), TEXT("UnrealScriptedSequence12"))==0))
				)
				{
					A->Tag = FName(TEXT("DefendBlueFlag"));
				}
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level ctf-maul.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up CTF-Maul"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,1500.f);

			AStaticMeshActor* S = Cast<AStaticMeshActor>(GLevel->Actors(i));
			if( S && 
				((appStricmp(S->GetName(), TEXT("StaticMeshActor36"))==0)
				|| (appStricmp(S->GetName(), TEXT("StaticMeshActor72"))==0)
				|| (appStricmp(S->GetName(), TEXT("StaticMeshActor74"))==0) )
			)
			{
				S->SetCollision(false, false);
				S->bBlockActors					= false;
				S->bBlockKarma					= false;
				S->bBlockNonZeroExtentTraces	= false;
				S->bBlockZeroExtentTraces		= false;
				S->bCollideActors				= false;
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level br-disclosure.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up BR-Disclosure"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,500.f);
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level br-twintombs.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up BR-TwinTombs"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
			{
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,1500.f);
				Z->bSoftKillZ = true;
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dm-asbestos.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DM-Asbestos"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,1000.f);
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dm-curse3.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DM-Curse3"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,500.f);
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dm-gael.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DM-Gael"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
			{
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,1000.f);
				Z->DistanceFogEnd = ::Max(Z->DistanceFogEnd,7000.f);
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dm-insidious.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DM-Insidious"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,500.f);
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dm-gael.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DM-Gael"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,1000.f);
		}
	}	
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dm-leviathan.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DM-Leviathan"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
			{
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,0.f);
				Z->DistanceFogEnd = ::Max(Z->DistanceFogEnd,10000.f);
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dm-oceanic.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DM-Oceanic"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
			{
				Z->DistanceFogEnd = ::Max(Z->DistanceFogEnd,6000.f);
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dm-phobos2.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DM-Phobos2"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
			{
				Z->bDistanceFog = false;
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dm-plunge.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DM-Plunge"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
			{
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,700.f);
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dm-serpentine.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DM-Serpentine"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
			{
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,1000.f);
				Z->DistanceFogEnd = ::Max(Z->DistanceFogEnd,7000.f);
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dm-trainingday.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DM-TrainingDay"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
			{
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,700.f);
			}
		}
	}	
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dom-core.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DOM-Core"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
			{
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,1000.f);
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dom-ruination.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DOM-Ruination"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
			{
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,700.f);
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dom-suntemple.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DOM-SunTemple"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AZoneInfo* Z = Cast<AZoneInfo>(GLevel->Actors(i));
			if ( Z && !Cast<ASkyZoneInfo>(Z) )
			{
				Z->DistanceFogStart = ::Max(Z->DistanceFogStart,1000.f);
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dom-junkyard.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DOM-Junkyard"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AJumpDest* J = Cast<AJumpDest>(GLevel->Actors(i));
			if ( J && (appStricmp(J->GetName(), TEXT("JumpSpot1"))==0) )
			{
				J->bForceDoubleJump = true;
			}
			AStaticMeshActor *A = Cast<AStaticMeshActor>(GLevel->Actors(i));
			if ( A && (appStricmp(A->GetName(), TEXT("StaticMeshActor34"))==0) )
			{
				A->Skins.Empty();
				A->bHidden = true;
			}
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level ctf-doubledammage.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up CTF-DoubleDammage"));

		GLevel->GetLevelInfo()->IndoorCamouflageMesh = NULL;
		GLevel->GetLevelInfo()->OutdoorCamouflageMesh = NULL;
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level dm-de-grendelkeep.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up DM-DE-Grendelkeep"));

		UTexture* FixedTexture = CastChecked<UTexture>(StaticLoadObject( UTexture::StaticClass(), NULL, TEXT("Grendelfix.Base.ForSplashes"), NULL, LOAD_NoWarn | 0, NULL ));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AStaticMeshActor* S = Cast<AStaticMeshActor>(GLevel->Actors(i));
			if( S && appStricmp(S->GetName(), TEXT("StaticMeshActor63"))==0 )
			{
				S->SetCollision(false, false);
				S->bBlockActors					= false;
				S->bBlockKarma					= false;
				S->bBlockNonZeroExtentTraces	= false;
				S->bBlockZeroExtentTraces		= false;
				S->bCollideActors				= false;
			}
			if( 
			S	&&	(	appStricmp(S->GetName(), TEXT("StaticMeshActor226"))==0 
					||	appStricmp(S->GetName(), TEXT("StaticMeshActor499"))==0 
					||	appStricmp(S->GetName(), TEXT("StaticMeshActor489"))==0 
					||	appStricmp(S->GetName(), TEXT("StaticMeshActor491"))==0 
					)
			)
				S->Skins(0) = FixedTexture;
		}
	}
	else if ( appStricmp(GLevel->GetFullName(), TEXT("Level AS-Junkyard.myLevel"))==0 )
	{
		debugf(TEXT("Fixing up AS-Junkyard"));

		for ( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			AMover *M = Cast<AMover>(GLevel->Actors(i));
			if( M && appStricmp(M->GetName(), TEXT("Mover11"))==0 )
			{
				M->MoverEncroachType = ME_CrushWhenEncroach;
				M->EncroachDamage = 999;
				break;
			}
		}
	}

	UObject* AWRustTex = FindObject<UObject>( ANY_PACKAGE, TEXT("AW-RustTex") );
	if( AWRustTex )
	{
		UObject* AWRustTexShaders = FindObject<UObject>( AWRustTex, TEXT("Shaders") );
		if( AWRustTexShaders )
		{
			UShader* PanningOil = FindObject<UShader>( AWRustTexShaders, TEXT("PanningOil") );
			if( PanningOil )
			{
				//debugf(TEXT("Fixing up fallback for %s"), PanningOil->GetFullName());
				PanningOil->FallbackMaterial = FindObject<UMaterial>( AWRustTexShaders, TEXT("TexOscillator3") );
			}
		}
	}

	UObject* AWShaders = FindObject<UObject>( ANY_PACKAGE, TEXT("AW-Shaders") );
	if( AWShaders )
	{
		UObject* AWShadersShaders = FindObject<UObject>( AWShaders, TEXT("Shaders") );
		if( AWShadersShaders )
		{
			UShader* MoltenSteel1 = FindObject<UShader>( AWShadersShaders, TEXT("MoltenSteel1") );
			if( MoltenSteel1 )
			{
				//debugf(TEXT("Fixing up fallback for %s"), MoltenSteel1->GetFullName());
				MoltenSteel1->FallbackMaterial = FindObject<UMaterial>( AWShadersShaders, TEXT("TexPanner7") );
			}

			UShader* MoltenSteel2 = FindObject<UShader>( AWShadersShaders, TEXT("MoltenSteel2") );
			if( MoltenSteel2 )
			{
				//debugf(TEXT("Fixing up fallback for %s"), MoltenSteel2->GetFullName());
				MoltenSteel2->FallbackMaterial = FindObject<UMaterial>( AWShadersShaders, TEXT("TexPanner0") );
			}
		}
	}
	unguard;
}


#if MACOSX  // GameRanger support for MacOS players.  --ryan.
extern "C"
{
    extern int GameRangerClientsWaiting;
    void AlertGameRangerHostReady(void);
}
#endif

//
// Load a map.
//
ULevel* UGameEngine::LoadMap( const FURL& URL, UPendingLevel* Pending, const TMap<FString,FString>* TravelInfo, FString& Error )
{
	guard(UGameEngine::LoadMap);

    if( GEntry ) GEntry->CleanupDestroyed( 1 ); // gam

	Error = TEXT("");
	debugf( NAME_Init, TEXT("LoadMap: %s"), *URL.String(0,1) );
	GInitRunaway();

	#if MACOSX  // GameRanger...notify players when 1st real map is loaded.
	if (GameRangerClientsWaiting)
	{
		const TCHAR *ignore[] = { TEXT("entry"), TEXT("nvidialogo"), NULL };
		INT loc, i;
		FString map(*URL.String(0,1));
		loc = map.InStr(TEXT("?"));
		if (loc >= 0) map = map.Left(loc);
		loc = map.InStr(TEXT("."));
		if (loc >= 0) map = map.Left(loc);

		for ( i = 0; ignore[i] != NULL; i++ )
		{
			if (appStricmp(*map, ignore[i]) == 0)
				break;
		}

		if (ignore[i] == NULL)  // no match? It's a real map.
			AlertGameRangerHostReady();
	}
	#endif

	// Remember current level's stack level.
	INT SavedHubStackLevel = GLevel ? GLevel->GetLevelInfo()->HubStackLevel : 0;

	// Get network package map.
	UPackageMap* PackageMap = NULL;
	if( Pending )
		PackageMap = Pending->GetDriver()->ServerConnection->PackageMap;

	// Verify that we can load all packages we need.
	UObject* MapParent = NULL;
	guard(VerifyPackages);
	try
	{
		BeginLoad();
		if( Pending )
		{
			// Verify that we can load everything needed for client in this network level.
			for( INT i=0; i<PackageMap->List.Num(); i++ )
			{
				PackageMap->List(i).Linker = GetPackageLinker
				(
					PackageMap->List(i).Parent,
					NULL,
					LOAD_Verify | LOAD_Throw | LOAD_NoWarn | LOAD_NoVerify,
					NULL,
					&PackageMap->List(i).Guid,
					PackageMap->List(i).RemoteGeneration
				);
			}
#if UE_FAIL_SOFT_LOAD
			// Fail-soft GetPackageLinker answers NULL for a package it cannot have (not found, or found in
			// another version than the server's: [PACKAGE-LOAD] ... GUID mismatch), and VerifyLinker would
			// then call Verify() on NULL (in wasm that reads low memory instead of trapping). Fail the join
			// here, with the reason a native client shows, before the live level is torn down.
			for( INT i=0; i<PackageMap->List.Num(); i++ )
				if( !PackageMap->List(i).Linker )
				{
					const TCHAR* Name = PackageMap->List(i).Parent->GetName();
					TCHAR Found[256];
					const UBOOL HaveFile = appFindPackageFile( Name, &PackageMap->List(i).Guid, Found, PackageMap->List(i).RemoteGeneration );
					Error = FString::Printf( HaveFile ? LocalizeError(TEXT("PackageVersion"),TEXT("Core")) : LocalizeError(TEXT("PackageNotFound"),TEXT("Core")), Name );
					debugf( TEXT("LoadMap failed: %s — failing the join gracefully (no abort)"), *Error );
					EndLoad();
					// Error stays set: the caller (AttemptLoadPending) shows the disconnect menu with it; an empty
					// Error there means "loaded", and it would send the join from the level we are still in.
					return NULL;
				}
#endif
			for( INT i=0; i<PackageMap->List.Num(); i++ )
			{
				VerifyLinker( PackageMap->List(i).Linker );
			}
			if( PackageMap->List.Num() )
				MapParent = PackageMap->List(0).Parent;
		}
		ULevel* LoadedLevel = LoadObject<ULevel>( MapParent, TEXT("MyLevel"), *URL.Map, LOAD_Verify | LOAD_Throw | LOAD_NoWarn, NULL );
		EndLoad();
		(void)LoadedLevel;
#if UE_FAIL_SOFT_LOAD
		// A fail-soft load answers NULL instead of throwing (and on the web SafeLoadError does not
		// re-throw: Asyncify breaks the cross-frame exception unwind the catch below relies on), so a
		// failed map load arrives here as a NULL object. Route it through the same failure the catch
		// handler performs: report "could not load map" and return to the menu.
		if( !LoadedLevel )
		{
			debugf( TEXT("LoadMap failed: map '%s' could not be loaded — failing gracefully (no abort)"), *URL.Map );
			Error = FString::Printf( TEXT("Could not load map '%s'"), *URL.Map );
			if( Pending && Pending->NetDriver && Pending->NetDriver->ServerConnection )
			{
				// Error stays set: the caller (AttemptLoadPending) shows the disconnect menu with it; an empty
				// Error is how that caller knows the map loaded.
			}
			else if( GLevel==GEntry )
				SetProgress( *FString::Printf(TEXT("menu:%s"),*MainMenuClass), LocalizeError(TEXT("UrlFailed"),TEXT("Core")), *URL.Map );
			else
				SetProgress( TEXT(""), LocalizeError(TEXT("UrlFailed"),TEXT("Core")), *URL.Map );
			return NULL;
		}
#endif

	#if DEMOVERSION
			// If we are a demo, prevent third party maps from being loaded.
			if( !Pending )
			{
				FString FileName(FString(TEXT("../Maps/"))+URL.Map);
				if( FileName.Right(4).Caps() != TEXT(".UT2"))
					FileName = FileName + TEXT(".ut2");
				INT FileSize = GFileManager->FileSize( *FileName );
				//debugf(TEXT("Looking for file: %s %d"), *FileName, FileSize);
				if( ( FileName.Caps() != TEXT("../MAPS/DM-RANKIN.UT2") || FileSize != 13400320 ) &&
					( FileName.Caps() != TEXT("../MAPS/ONS-TORLAN.UT2") || FileSize != 7501910 ) &&
					( FileName.Caps() != TEXT("../MAPS/ONS-PRIMEVAL.UT2") || FileSize != 9934490 ) &&
					( FileName.Caps() != TEXT("../MAPS/DM-DECK17.UT2") || FileSize != 31831451 ) &&
					( FileName.Caps() != TEXT("../MAPS/DM-1ON1-IDOMA.UT2") || FileSize != 11895873 ) &&
					( FileName.Caps() != TEXT("../MAPS/AS-CONVOY.UT2") || FileSize != 10173598 ) &&
					( FileName.Caps() != TEXT("../MAPS/CTF-BRIDGEOFFATE.UT2") || FileSize != 27970106 ) &&
					( FileName.Caps() != TEXT("../MAPS/CTF-FACECLASSIC.UT2") || FileSize != 28141678 ) &&
					( FileName.Caps() != TEXT("../MAPS/CTF-TWINTOMBS.UT2") || FileSize != 26865440 ) &&
					( FileName.Caps() != TEXT("../MAPS/BR-COLOSSUS.UT2") || FileSize != 18119054 ) &&
					( FileName.Caps() != TEXT("../MAPS/NVIDIALOGO.UT2") || FileSize != 487891 ) &&
					( FileName.Caps() != TEXT("../MAPS/NOINTRO.UT2") || FileSize != 334840 ) &&
					( FileName.Caps() != TEXT("../MAPS/ENTRY.UT2") || FileSize != 336568 ) )
				{
					Error = TEXT("Sorry, only the retail version of UT2004 can load third party maps.");
					SetProgress( *FString::Printf(TEXT("menu:%s"),*DisconnectMenuClass), *Error, TEXT("") );
					return NULL;
				}
			}
	#endif
	}
	#if UNICODE
	catch( TCHAR* CatchError )
	#else
	catch( char* CatchError )
	#endif
	{
		// Safely failed loading.
		EndLoad();
		Error = CatchError;
        #if UNICODE
        TCHAR *e = CatchError;
        #else
        TCHAR *e = ANSI_TO_TCHAR(CatchError);
        #endif

		if (Pending && Pending->NetDriver && Pending->NetDriver->ServerConnection)
		{
			SetProgress( *FString::Printf(TEXT("menu:%s"),*DisconnectMenuClass), LocalizeError(TEXT("UrlFailed"),TEXT("Core")), e );

			// Clear error so we don't open two network status msg windows. --rjp
			Error = TEXT("");
		}
		else if (GLevel==GEntry)
		{
			SetProgress( *FString::Printf(TEXT("menu:%s"),*MainMenuClass), LocalizeError(TEXT("UrlFailed"),TEXT("Core")), e );
		}
		else
			SetProgress( TEXT(""), LocalizeError(TEXT("UrlFailed"),TEXT("Core")), e );

		// Clear the partially initialized level loader from memory.
		if(Pending && PackageMap->List.Num() && PackageMap->List(0).Parent)
			ResetLoaders(PackageMap->List(0).Parent,0,1);
		return NULL;
	}
	unguard;

	// Display loading screen.
	guard(LoadingScreen);

	if( Client && Client->Viewports.Num() && GLevel && !URL.HasOption(TEXT("quiet")) )
	{
		GLevel->GetLevelInfo()->LevelAction = LEVACT_Loading;
		GLevel->GetLevelInfo()->Pauser = NULL;

        AVignette* Vignette = NULL;
		const TCHAR *gametype, *TeamScreen;
		gametype = URL.GetOption(TEXT("Game="),TEXT(""));
		TeamScreen = URL.GetOption(TEXT("TeamScreen="),TEXT("False"));

		if ((appStricmp(URL.GetOption(TEXT("SaveGame="),TEXT("")), TEXT("")) == 0) && (GLevel->GetLevelInfo()->Game != NULL))
		{ // prevent returning to SP menu when not playing a SP match
			if (GLevel->GetLevelInfo()->Game->CurrentGameProfile != NULL)
				GLevel->GetLevelInfo()->Game->CurrentGameProfile->bInLadderGame = false;
		}

		if ( GLevel->GetLevelInfo()->NetMode == NM_Standalone && 
			 (Client->Viewports(0)->SizeX > 600 ) &&		// hardcoded force for menu resolution, see D3DRenderDevice.cpp;
			 (Client->Viewports(0)->SizeY > 400 ) &&		// would use 640x480 but windows creates wierd 640x479 res sometimes
			 GLevel->GetLevelInfo()->Game->CurrentGameProfile != NULL && 
			 GLevel->GetLevelInfo()->Game->CurrentGameProfile->bInLadderGame &&
			 (appStricmp (TeamScreen, TEXT("true")) == 0) )
		{ 
			UClass* VignetteClass = StaticLoadClass( AVignette::StaticClass(), NULL, *LoadingClass, NULL, LOAD_NoFail, NULL );
			Vignette = CastChecked<AVignette>( GLevel->SpawnActor( VignetteClass ) );
			Vignette->MapName = URL.Map;
			if (appStrcmp(gametype, TEXT("")) != 0)
				Vignette->GameClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, gametype, NULL, LOAD_NoWarn | LOAD_Quiet, NULL));
			Vignette->eventInit();
		} else if( URL.Map.Left (4) != TEXT("Menu") )
        {
            UClass* VignetteClass = StaticLoadClass( AVignette::StaticClass(), NULL, *ConnectingMenuClass, NULL, LOAD_NoFail, NULL );
            Vignette = CastChecked<AVignette>( GLevel->SpawnActor( VignetteClass ) );
            Vignette->MapName = URL.Map;
			if (appStrcmp(gametype, TEXT("")) != 0)
				Vignette->GameClass = Cast<UClass>(StaticLoadObject(UClass::StaticClass(), NULL, gametype, NULL, LOAD_NoWarn | LOAD_Quiet, NULL));
            Vignette->eventInit();
        }

        PaintProgress( Vignette, 1.0F );

        if( Vignette )
            GLevel->DestroyActor( Vignette );

		if( Audio )
			Audio->SetViewport( Audio->GetViewport() );
		GLevel->GetLevelInfo()->LevelAction = LEVACT_None;
	}
	unguard;

	// Notify of the level change, before we dissociate Viewport actors
	guard(NotifyLevelChange);
	if( GLevel )
		NotifyLevelChange();
	unguard;

	// Dissociate Viewport actors.
	guard(DissociateViewports);
	if( Client )
	{
		for( INT i=0; i<Client->Viewports.Num(); i++ )
		{
			APlayerController* Actor    = Client->Viewports(i)->Actor;
			// Devnet diagnostic: the window where Viewport->Actor is NULL opens here.
			debugf( NAME_DevNet, TEXT("[NET-DIAG] LoadMap: dissociating viewport %s (actor=%s) — Actor goes NULL until MatchViewportsToActors"),
				Client->Viewports(i)->GetName(), Actor ? Actor->GetName() : TEXT("NULL") );
			ULevel*      Level          = Actor->GetLevel();
			Actor->Player               = NULL;
			Client->Viewports(i)->Actor = NULL;
			if ( Actor->Pawn )
				Level->DestroyActor(Actor->Pawn);
			Level->DestroyActor( Actor );
		}
	}
	unguard;

	// Clean up game state.
	guard(ExitLevel);
	if( GLevel )
	{
		// Shut down.
        GLevel->SetActorCollision( 0, 1 ); // gam
		ResetLoaders( GLevel->GetOuter(), 1, 0 );
		if( GLevel->NetDriver )
		{
			delete GLevel->NetDriver;
			GLevel->NetDriver = NULL;
		}
		if( GLevel->DemoRecDriver )
		{
			delete GLevel->DemoRecDriver;
			GLevel->DemoRecDriver = NULL;
		}
		if( URL.HasOption(TEXT("push")) )
		{
			// Save the current level minus players actors.
			GLevel->CleanupDestroyed( 1 );
			TCHAR Filename[256];
			appSprintf( Filename, TEXT("%s") PATH_SEPARATOR TEXT("Game%i.usa"), *GSys->SavePath, SavedHubStackLevel );
			SavePackage( GLevel->GetOuter(), GLevel, 0, Filename, GLog );
		}

#ifdef WITH_KARMA
		if(!GIsEditor) // dont need to do this in editor - no physics runs.
		{
			// To save memory, we remove physics from all actors before loading the new level.
			for( INT iActor=0; iActor<GLevel->Actors.Num(); iActor++ )
			{
				AActor* actor = GLevel->Actors(iActor);
				if(actor)
				{
					KTermActorKarma(actor);
				}
			}
		}
#endif
		GLevel = NULL;
	}
	unguard;

	// sjs ---
#ifdef _XBOX
	guard(CleanupXbox);
	if( GLevel && appStricmp(GLevel->GetOuter()->GetName(),TEXT("Entry"))!=0 )
	{
		Flush(0);
		{for( TObjectIterator<AActor> It; It; ++It )
			if( It->IsIn(GLevel->GetOuter()) )
				It->SetFlags( RF_EliminateObject );}
		{for( INT i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) )
				GLevel->Actors(i)->ClearFlags( RF_EliminateObject );}
		CollectGarbage( RF_Native );
	}
	unguard;
#endif
	// --- sjs

	// Load the level and all objects under it, using the proper Guid.
	guard(LoadLevel);
	GLevel = LoadObject<ULevel>( MapParent, TEXT("MyLevel"), *URL.Map, LOAD_NoFail, NULL );
	unguard;

	// If pending network level.
	if( Pending )
	{
		// If playing this network level alone, ditch the pending level.
		if( Pending && Pending->LonePlayer )
			Pending = NULL;

		// Setup network package info.
		PackageMap->Compute();
		for( INT i=0; i<PackageMap->List.Num(); i++ )
			if( PackageMap->List(i).LocalGeneration!=PackageMap->List(i).RemoteGeneration )
				Pending->GetDriver()->ServerConnection->Logf( TEXT("HAVE GUID=%s GEN=%i"), PackageMap->List(i).Guid.String(), PackageMap->List(i).LocalGeneration );
	}

	// Verify classes.
	guard(VerifyClasses);
	UBOOL Mismatch = false;
	#define VERIFY_CLASS_SIZES
	#define NAMES_ONLY
	#define AUTOGENERATE_NAME(name)
	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "EngineClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef AUTOGENERATE_NAME
	#undef NAMES_ONLY
	#undef VERIFY_CLASS_SIZES

	VERIFY_CLASS_OFFSET_NODIE( A, Actor,       Owner         );
	VERIFY_CLASS_OFFSET_NODIE( A, Actor,       TimerCounter  );
	VERIFY_CLASS_OFFSET_NODIE( A, PlayerController,  Player  );
	VERIFY_CLASS_OFFSET_NODIE( A, Pawn,  Health );

	VERIFY_CLASS_SIZE_NODIE( UCanvas );
	VERIFY_CLASS_SIZE_NODIE( UCubemap );
	VERIFY_CLASS_SIZE_NODIE( UEngine );
	VERIFY_CLASS_SIZE_NODIE( UGameEngine );
	VERIFY_CLASS_SIZE_NODIE( UPalette );
	VERIFY_CLASS_SIZE_NODIE( UPlayer );
	VERIFY_CLASS_SIZE_NODIE( UTexture );

	if( Mismatch )
#if PLATFORM_64BITS
		// 64-bit: native classes with a trailing pointer can be larger than the 32-bit .u package
		// sizes; the member offsets checked above are what matter (a per-member problem is logged),
		// so a total-size mismatch is a warning here, as in the retail amd64 build.
		debugf( NAME_Warning, TEXT("Engine C++/UnrealScript class size mismatch (tolerated on 64-bit)") );
#else
		appErrorf(TEXT("Engine C++/UnrealScript class size mismatch"));
#endif

	unguard;

	// Get LevelInfo.
	check(GLevel);
	ALevelInfo* Info = GLevel->GetLevelInfo();
	if ( Info->bSupportSaveGames )
		GTransientNaming = 1;
	Info->ComputerName = appComputerName();

	// Handle pushing.
	guard(ProcessHubStack);
	Info->HubStackLevel
	=	URL.HasOption(TEXT("load")) ? Info->HubStackLevel
	:	URL.HasOption(TEXT("push")) ? SavedHubStackLevel+1
	:	URL.HasOption(TEXT("pop" )) ? Max(SavedHubStackLevel-1,0)
	:	URL.HasOption(TEXT("peer")) ? SavedHubStackLevel
	:	                              0;
	unguard;

	// Handle pending level.
	guard(ActivatePending);
	if( Pending )
	{
		check(Pending==GPendingLevel);

		// Hook network driver up to level.
		GLevel->NetDriver = Pending->NetDriver;
		if( GLevel->NetDriver )
			GLevel->NetDriver->Notify = GLevel;

		// Hook demo playback driver to level
		GLevel->DemoRecDriver = Pending->DemoRecDriver;
		if( GLevel->DemoRecDriver )
			GLevel->DemoRecDriver->Notify = GLevel;

		// Setup level.

		GLevel->GetLevelInfo()->NetMode = NM_Client;
		debugf(TEXT("NetMode now NM_Client"));
	}
	else check(!GLevel->NetDriver);
	unguard;

	// Set level info.
	guard(InitLevel);
	if( !URL.GetOption(TEXT("load"),NULL) )
		GLevel->URL = URL;
	Info->EngineVersion = FString::Printf( TEXT("%i"), ENGINE_VERSION );
	Info->MinNetVersion = FString::Printf( TEXT("%i"), ENGINE_MIN_NET_VERSION );
	GLevel->Engine = this;
	if( TravelInfo )
		GLevel->TravelInfo = *TravelInfo;
	unguard;

	// Remove cubemaps.
#if 1
	if( GLevel->Engine->GRenDev && GLevel->Engine->GRenDev->Is3dfx )
	{
		for( TObjectIterator<UModifier> It; It; ++It )
		{
			if( It->Material && It->Material->IsA(UCubemap::StaticClass()) )
				It->Material = NULL;
		}

		for( TObjectIterator<UCombiner> It; It; ++It )
		{
			if( It->Material1 && It->Material1->IsA(UCubemap::StaticClass()) )
			{
				It->Material1 = It->Material2;
				It->Material2 = NULL;
			}
			if( It->Material2 && It->Material2->IsA(UCubemap::StaticClass()) )
				It->Material2 = NULL;
		}

		for( TObjectIterator<UShader> It; It; ++It )
		{
			if( It->Diffuse && It->Diffuse->IsA(UCubemap::StaticClass()) )
				It->Diffuse = NULL;

			if( It->Opacity && It->Opacity->IsA(UCubemap::StaticClass()) )
				It->Opacity = NULL;

			if( It->Specular && It->Specular->IsA(UCubemap::StaticClass()) )
				It->Specular = NULL;

			if( It->SpecularityMask && It->SpecularityMask->IsA(UCubemap::StaticClass()) )
				It->SpecularityMask = NULL;

			if( It->SelfIllumination && It->SelfIllumination->IsA(UCubemap::StaticClass()) )
				It->SelfIllumination = NULL;

			if( It->SelfIlluminationMask && It->SelfIlluminationMask->IsA(UCubemap::StaticClass()) )
				It->SelfIlluminationMask = NULL;

			if( It->Detail && It->Detail->IsA(UCubemap::StaticClass()) )
				It->Detail = NULL;
		}
	}
#endif

	// Purge unused objects and flush caches.
	guard(Cleanup);
	if( appStricmp(GLevel->GetOuter()->GetName(),TEXT("Entry"))!=0 )
	{
		Flush(0);
		UViewport* Viewport = NULL;
		if ( Audio )
		{
			Viewport = Audio->GetViewport();
			Audio->SetViewport( NULL );
		}
		{for( TObjectIterator<AActor> It; It; ++It )
			if( It->IsIn(GLevel->GetOuter()) )
				It->SetFlags( RF_EliminateObject );}
		{for( INT i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) )
				GLevel->Actors(i)->ClearFlags( RF_EliminateObject );}
		CollectGarbage( RF_Native );
		if( Audio )
		{
			for( INT i=0; i<Client->Viewports.Num(); i++ )
			{
				if( Client->Viewports(i) == Viewport )
				{
					Audio->SetViewport( Viewport );
					break;
				}
			}
		}
	}
	unguard;

	// Tell the audio driver to clean up.
//	if( Audio )
//		Audio->CleanUp();

	// Init collision.
	GLevel->SetActorCollision( 1 );

	// Setup zone distance table for sound damping. Fast enough: Approx 3 msec.
	guard(SetupZoneTable);
	QWORD OldConvConn[64];
	QWORD ConvConn[64];
	for( INT i=0; i<64; i++ )
	{
		for( INT j=0; j<64; j++ )
		{
			OldConvConn[i] = GLevel->Model->Zones[i].Connectivity;
			if( i == j )
				GLevel->ZoneDist[i][j] = 0;
			else
				GLevel->ZoneDist[i][j] = 255;
		}
	}
	for( INT i=1; i<64; i++ )
	{
		for( INT j=0; j<64; j++ )
			for( INT k=0; k<64; k++ )
				if( (GLevel->ZoneDist[j][k] > i) && ((OldConvConn[j] & ((QWORD)1 << k)) != 0) )
					GLevel->ZoneDist[j][k] = i;
		for( INT j=0; j<64; j++ )
			ConvConn[j] = 0;
		for( INT j=0; j<64; j++ )
			for( INT k=0; k<64; k++ )
				if( (OldConvConn[j] & ((QWORD)1 << k)) != 0 )
					ConvConn[j] = ConvConn[j] | OldConvConn[k];
		for( INT j=0; j<64; j++ )
			OldConvConn[j] = ConvConn[j];
	}
	unguard;

	// Update the LevelInfo's time.
	GLevel->UpdateTime(Info);

	// Init the game info.
	TCHAR Options[1024]=TEXT("");
	TCHAR GameClassName[256]=TEXT("");
	TCHAR MenuClassName[256]=TEXT(""); // gam
	FString Error=TEXT("");
	guard(InitGameInfo);
	for( INT i=0; i<URL.Op.Num(); i++ )
	{
		appStrcat( Options, TEXT("?") );
		appStrcat( Options, *URL.Op(i) );
		Parse( *URL.Op(i), TEXT("GAME="), GameClassName, ARRAY_COUNT(GameClassName) );
		Parse( *URL.Op(i), TEXT("MENU="), MenuClassName, ARRAY_COUNT(MenuClassName) ); // gam
	}
	if( GLevel->IsServer() && !Info->Game )
	{
		if ( appStricmp(GLevel->GetFullName(), TEXT("Level dm-tokaraforest.myLevel"))==0 )
		{
			debugf(TEXT("Fixing up DM-TokaraForest"));
			GLevel->GetLevelInfo()->DefaultGameType = TEXT("xGame.xDeathMatch");
		}

		// Get the GameInfo class.
		UClass* GameClass=NULL;
		if ( GameClassName[0] )
			GameClass = StaticLoadClass( AGameInfo::StaticClass(), NULL, GameClassName, NULL, 0, PackageMap );
		if( !GameClass && Info->DefaultGameType.Len() > 0 ) 
			GameClass = StaticLoadClass( AGameInfo::StaticClass(), NULL, *(Info->DefaultGameType), NULL, 0, PackageMap );
		if( !GameClass && appStricmp(GLevel->GetOuter()->GetName(),TEXT("Entry"))==0 ) 
			GameClass = AGameInfo::StaticClass();
		if( !GameClass ) 
			GameClass = StaticLoadClass( AGameInfo::StaticClass(), NULL, Client ? TEXT("ini:Engine.Engine.DefaultGame") : TEXT("ini:Engine.Engine.DefaultServerGame"), NULL, 0, PackageMap ); // gam
        if ( !GameClass ) 
			GameClass = AGameInfo::StaticClass();

		// Spawn the GameInfo.
		debugf( NAME_Log, TEXT("Game class is '%s'"), GameClass->GetName() );
		Info->Game = (AGameInfo*)GLevel->SpawnActor( GameClass );
		check(Info->Game!=NULL);
	}
	unguard;

	// Listen for clients.
	guard(Listen);
	if( !Client || URL.HasOption(TEXT("Listen")) )
	{
		if( GPendingLevel )
		{
			guard(CancelPendingForListen);
			check(!Pending);
			delete GPendingLevel;
			GPendingLevel = NULL;
			unguard;
		}
		FString Error;
		if( !GLevel->Listen( Error ) )
			appErrorf( LocalizeError(TEXT("ServerListen"),TEXT("Engine")), *Error );

		UCacheManager::GetCache()->RefreshCache();
	}
	unguard;


	// Init detail.
	Info->DetailMode = DM_SuperHigh;
	if(Client && Client->Viewports.Num() && Client->Viewports(0)->RenDev)
	{
		if(Client->Viewports(0)->RenDev->SuperHighDetailActors)
			Info->DetailMode = DM_SuperHigh;
		else if(Client->Viewports(0)->RenDev->HighDetailActors)
			Info->DetailMode = DM_High;
		else
			Info->DetailMode = DM_Low;
	}

	// Clear any existing stat graphs.
	if(GStatGraph)
		GStatGraph->Reset();

	// Init level gameplay info.
	guard(BeginPlay);
	GLevel->iFirstDynamicActor = 0;
	if( !Info->bBegunPlay )
	{
        appResetTimer(); // sjs

		// fix up level problems
		FixUpLevel();

		// Update draw distance.
		if (GIsClient)
		{
			GLevel->GetLevelInfo()->InitDistanceFogLOD();
			GLevel->GetLevelInfo()->UpdateDistanceFogLOD( Client->DrawDistanceLOD );
		}

		// Lock the level.
		debugf( NAME_Log, TEXT("Bringing %s up for play (%i) appSeconds: %f..."), GLevel->GetFullName(), appRound(GetMaxTickRate()), appSeconds() ); // sjs
		GLevel->FinishedPrecaching = 0;
		GLevel->TimeSeconds = 0;
		GLevel->GetLevelInfo()->TimeSeconds = 0;
		GLevel->GetLevelInfo()->GetDefaultPhysicsVolume()->bNoDelete = true;

		// Kill off actors that aren't interesting to the client.
		if( !GLevel->IsServer() )
		{
			for( INT i=0; i<GLevel->Actors.Num(); i++ )
			{
				AActor* Actor = GLevel->Actors(i);
				if( Actor )
				{
					if( Actor->bStatic || Actor->bNoDelete || Actor->IsA(AxEmitter::StaticClass()) ) 
					{
						if ( !Actor->bClientAuthoritative )
							Exchange( Actor->Role, Actor->RemoteRole );
					}
					else
						GLevel->DestroyActor( Actor );
				}
			}
		}

		// Init touching actors & clear LastRenderTime
		for( INT i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) )
			{
				GLevel->Actors(i)->LastRenderTime = 0.f;
				GLevel->Actors(i)->Touching.Empty();
				GLevel->Actors(i)->PhysicsVolume = GLevel->GetLevelInfo()->GetDefaultPhysicsVolume();
			}


		// Init scripting.
		for( INT i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) )
				GLevel->Actors(i)->InitExecution();

		// Enable actor script calls.
		Info->bBegunPlay = 1;
		Info->bStartup = 1;
		Info->TimeDilation = ((ALevelInfo *)(Info->GetClass()->GetDefaultActor()))->TimeDilation;

#ifdef WITH_KARMA
		if(!GIsEditor && !GLevel->GetLevelInfo()->bKNoInit)
		{
			KInitLevelKarma(GLevel);

			for( INT i=0; i<GLevel->Actors.Num(); i++ )
				if( GLevel->Actors(i) )
					KInitActorKarma( GLevel->Actors(i) );
		}
#endif

		// Init the game.
		if( Info->Game )
		{		
			Info->Game->eventInitGame( Options, Error );
			Info->Game->eventSetGrammar();
		}

		// Send PreBeginPlay.
		for( INT i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) && !GLevel->Actors(i)->bScriptInitialized )
				GLevel->Actors(i)->eventPreBeginPlay();

		// Set BeginPlay.
		for( INT i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) && !GLevel->Actors(i)->bScriptInitialized )
				GLevel->Actors(i)->eventBeginPlay();

		// Set zones && gather volumes.
		TArray<AVolume*> LevelVolumes;
		for( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			if( GLevel->Actors(i) && !GLevel->Actors(i)->bScriptInitialized )
				GLevel->Actors(i)->SetZone( 1, 1 );

			AVolume* Volume = Cast<AVolume>(GLevel->Actors(i));
			if( Volume )
				LevelVolumes.AddItem(Volume);
		}
		
		// Set appropriate volumes for each actor.
		for( INT i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) && !GLevel->Actors(i)->bScriptInitialized )
				GLevel->Actors(i)->SetVolumes( LevelVolumes );

		// Post begin play.
		for( INT i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) && !GLevel->Actors(i)->bScriptInitialized )
			{
				GLevel->Actors(i)->eventPostBeginPlay();

				if(GLevel->Actors(i))
					GLevel->Actors(i)->PostBeginPlay();
			}

		// Post net begin play.
		for( INT i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) && !GLevel->Actors(i)->bScriptInitialized )
				GLevel->Actors(i)->eventPostNetBeginPlay();

		// Begin scripting.
		for( INT i=0; i<GLevel->Actors.Num(); i++ )
			if( GLevel->Actors(i) && !GLevel->Actors(i)->bScriptInitialized )
				GLevel->Actors(i)->eventSetInitialState();

		// Find bases
		for( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			if( GLevel->Actors(i) ) 
			{
				if ( GLevel->Actors(i)->AttachTag != NAME_None )
				{
					//find actor to attach self onto
					for( INT j=0; j<GLevel->Actors.Num(); j++ )
					{
						if( GLevel->Actors(j) 
							&& ((GLevel->Actors(j)->Tag == GLevel->Actors(i)->AttachTag) || (GLevel->Actors(j)->GetFName() == GLevel->Actors(i)->AttachTag))  )
						{
							GLevel->Actors(i)->SetBase(GLevel->Actors(j), FVector(0,0,1), 0);
							break;
						}
					}
				}
				else if( GLevel->Actors(i)->bCollideWorld && GLevel->Actors(i)->bShouldBaseAtStartup
				 &&	((GLevel->Actors(i)->Physics == PHYS_None) || (GLevel->Actors(i)->Physics == PHYS_Rotating)) )
				{
					 GLevel->Actors(i)->FindBase();
				}
			}
		}

		for( INT i=0; i<GLevel->Actors.Num(); i++ ) 
		{
			if(GLevel->Actors(i))
			{
				if( GLevel->Actors(i)->IsA(AProjector::StaticClass())) // sjs - why is this needed?!!
				{
					GLevel->Actors(i)->PostEditChange();
				}

#ifdef WITH_KARMA
				AActor* actor = GLevel->Actors(i);

				if(actor->Physics != PHYS_Karma || !actor->KParams || !actor->KParams->IsA(UKarmaParams::StaticClass()))
					continue;

				UKarmaParams* kparams = Cast<UKarmaParams>(actor->KParams);

				// If running below HighDetailPhysics, turn off karma dynamics for actors with bHighDetailOnly set true.
				if(	GLevel->GetLevelInfo()->PhysicsDetailLevel < PDL_High && kparams->bHighDetailOnly )
					KTermActorDynamics(actor);

				// If dedicated server, turn off karma for actors with bHighDetailOnly or bClientsOnly
				if(	GLevel->GetLevelInfo()->NetMode == NM_DedicatedServer && (kparams->bHighDetailOnly || kparams->bClientOnly) )
					KTermActorDynamics(actor);
#endif
			}
		}

		Info->bStartup = 0;
	}
	else if ( Info->bIsSaveGame )
	{
        appResetTimer(); // sjs

		// Update draw distance.
		if (GIsClient)
		{
			GLevel->GetLevelInfo()->InitDistanceFogLOD();
			GLevel->GetLevelInfo()->UpdateDistanceFogLOD( Client->DrawDistanceLOD );
		}

		// Lock the level.
		debugf( NAME_Log, TEXT("Bringing %s up for play (%i) appSeconds: %f..."), GLevel->GetFullName(), appRound(GetMaxTickRate()), appSeconds() ); // sjs
		GLevel->FinishedPrecaching = 0;

#ifdef WITH_KARMA
		if(!GIsEditor && !GLevel->GetLevelInfo()->bKNoInit)
		{
			KInitLevelKarma(GLevel);

			for( INT i=0; i<GLevel->Actors.Num(); i++ )
				if( GLevel->Actors(i) )
					KInitActorKarma( GLevel->Actors(i) );
		}
#endif

		for( INT i=0; i<GLevel->Actors.Num(); i++ )
		{
			if(GLevel->Actors(i))
			{
				// HACK reset main animation for animated meshes (since animation position isn't properly serialized)
				if ( (GLevel->Actors(i)->DrawType == DT_Mesh) && GLevel->Actors(i)->Mesh )
				{
					GLevel->Actors(i)->PlayReplicatedAnim();
				}
				GLevel->Actors(i)->eventPostLoadSavedGame();

				if( GLevel->Actors(i)->IsA(AProjector::StaticClass())) // sjs - why is this needed?!!
				{
					GLevel->Actors(i)->PostEditChange();
				}

#ifdef WITH_KARMA
				AActor* actor = GLevel->Actors(i);

				if ( actor->Physics == PHYS_KarmaRagDoll )
				{
					// no ragdoll serialization support, so don't show saved ragdolls
					GLevel->DestroyActor( actor );
				}
				else
				{
					if(actor->Physics != PHYS_Karma || !actor->KParams || !actor->KParams->IsA(UKarmaParams::StaticClass()))
						continue;

					UKarmaParams* kparams = Cast<UKarmaParams>(actor->KParams);

					// If running below HighDetailPhysics, turn off karma dynamics for actors with bHighDetailOnly set true.
					if(	GLevel->GetLevelInfo()->PhysicsDetailLevel < PDL_High && kparams->bHighDetailOnly )
						KTermActorDynamics(actor);

					// If dedicated server, turn off karma for actors with bHighDetailOnly or bClientsOnly
					if(	GLevel->GetLevelInfo()->NetMode == NM_DedicatedServer && (kparams->bHighDetailOnly || kparams->bClientOnly) )
						KTermActorDynamics(actor);
				}
#endif
			}
		}

		GLevel->TimeSeconds = GLevel->GetLevelInfo()->TimeSeconds;
		Info->bIsSaveGame = false;
	}
	else GLevel->TimeSeconds = GLevel->GetLevelInfo()->TimeSeconds;
	unguard;

	// Rearrange actors: static first, then others.
	guard(Rearrange);
	TArray<AActor*> Actors;
	Actors.AddItem(GLevel->Actors(0));
	Actors.AddItem(GLevel->Actors(1));
	for( INT i=2; i<GLevel->Actors.Num(); i++ )
		if( GLevel->Actors(i) && GLevel->Actors(i)->bStatic && !GLevel->Actors(i)->bAlwaysRelevant )
			Actors.AddItem( GLevel->Actors(i) );
	GLevel->iFirstNetRelevantActor=Actors.Num();
	for( INT i=2; i<GLevel->Actors.Num(); i++ )
		if( GLevel->Actors(i) && GLevel->Actors(i)->bStatic && GLevel->Actors(i)->bAlwaysRelevant )
			Actors.AddItem( GLevel->Actors(i) );
	GLevel->iFirstDynamicActor=Actors.Num();
	for( INT i=2; i<GLevel->Actors.Num(); i++ )
		if( GLevel->Actors(i) && !GLevel->Actors(i)->bStatic )
			Actors.AddItem( GLevel->Actors(i) );
	GLevel->Actors.Empty();
	GLevel->Actors.Add( Actors.Num() );
	for( INT i=0; i<Actors.Num(); i++ )
		GLevel->Actors(i) = Actors(i);

	// create AntiPortal volume list
	GLevel->AntiPortals.Empty();
	for( INT i=0; i<GLevel->Actors.Num(); i++ )
	{
		AAntiPortalActor *A = Cast<AAntiPortalActor>(GLevel->Actors(i));
		if ( A )
			GLevel->AntiPortals.AddItem(A);
	}
	//debugf(TEXT("%d ANTIPORTALS"),GLevel->AntiPortals.Num());

	unguard;

	// Client init.
	guard(ClientInit);
	if( Client )
	{
		// Match Viewports to actors.
		MatchViewportsToActors( Client, GLevel->IsServer() ? GLevel : GEntry, URL, TEXT("")); //MenuClassName ); // gam

		// Set up audio.
		if( Audio )
			Audio->SetViewport( Audio->GetViewport() );

		// Reset viewports.
		for( INT i=0; i<Client->Viewports.Num(); i++ )
		{
			UViewport* Viewport = Client->Viewports(i);
			Viewport->Input->ResetInput();
			if( Viewport->RenDev )
				Viewport->RenDev->Flush(Viewport);
		}
	}
	unguard;

	// Init detail.
	GLevel->DetailChange( (EDetailMode)Info->DetailMode );

	// Remember the URL.
	guard(RememberURL);
	LastURL = URL;
	// gam ---
	for( INT i=LastURL.Op.Num()-1; i>=0; i-- )
	{
		if( !appStrPrefix( *LastURL.Op(i), TEXT("Menu=") ) )
		{
			LastURL.Op.Remove( i );
			break;
		}
	}
    // --- gam
	unguard;

	// Remember DefaultPlayer options.
	if( GIsClient )
	{
		if (!URL.HasOption(TEXT("NoSaveDefPlayer")))
		{
			URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Name" ), TEXT("User") );
			URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Team" ), TEXT("User") );
			URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Character" ), TEXT("User") ); // sjs
#ifdef _XBOX
			URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("VoiceMask" ), TEXT("User") ); // sjs
#endif
			URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Class"), TEXT("User") );
			URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Skin" ), TEXT("User") );
			URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Face" ), TEXT("User") );
			URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("Voice" ), TEXT("User") );
			URL.SaveURLConfig( TEXT("DefaultPlayer"), TEXT("OverrideClass" ), TEXT("User") );
		}
		if (URL.HasOption(TEXT("ResetDefPlayer")))
		{
			LastURL.LoadURLConfig(TEXT("DefaultPlayer"), TEXT("User"));
		}
	}

    // amb --- Load Sounds
    if (GLevel)
        GLevel->LoadSounds();
    // --- amb

#ifdef WITH_KARMA
	// Pre-allocate pool of KarmaTriListData structs
	// Don't bother if bKNoInit is false
	ALevelInfo* lInfo = GLevel->GetLevelInfo();
	if(!lInfo->bKNoInit)
	{
		for(INT i=0; i<lInfo->MaxRagdolls; i++)
		{
			KarmaTriListData* list = (KarmaTriListData*)appMalloc(sizeof(KarmaTriListData), TEXT("RAGDOLL TRILIST"));
			KarmaTriListDataInit(list);
			GLevel->TriListPool.AddItem(list);
		}
	}
#endif

	if ( PendingRecordMovie && Client && Client->Viewports.Num()>0 && Client->Viewports(0) && PendingRecordMovie != TEXT("") )
	{
		Client->Viewports(0)->Exec(*PendingRecordMovie);
		PendingRecordMovie=TEXT("");
	}



	// Successfully started local level.
	return GLevel;
	unguard;
}

/*-----------------------------------------------------------------------------
	Game Viewport functions.
-----------------------------------------------------------------------------*/

//
// Draw a global view.
//
//
// Scoped AutoFOV override. Everything that consumes the FOV during a frame ultimately reads
// Viewport->Actor->FovAngle: the scene node built at the three FPlayerSceneNode sites below, the
// distance-cull bias (UnRenderVisibility.cpp) and the shadow projector bias. Overriding it for the
// duration of the draw keeps all of them in agreement with one value and needs no render-device change
// at all (UE2 devices consume the resulting matrix, never the FOV). It is restored on every exit path,
// so the value script sees is untouched — which matters: PlayerInput.uc computes
// FOVScale = DesiredFOV * 0.01111 and scales aTurn/aLookUp by it, so writing the actor's FOV
// permanently would silently rescale the player's aim.
//
struct FScopedAutoFov
{
	APlayerController* Actor;   // UViewport::Actor (UnPlayer.h:27)
	FLOAT              Saved;
	FScopedAutoFov( UViewport* Viewport )
	:	Actor ( Viewport->Actor )
	,	Saved ( Viewport->Actor->FovAngle )
	{
		GAutoFovFrameBase = Saved;
		Actor->FovAngle = ufrontEffectiveFov( Saved, Viewport->SizeX, Viewport->SizeY, 1 );
	}
	~FScopedAutoFov()
	{
		Actor->FovAngle = Saved;
	}
};

void UGameEngine::Draw( UViewport* Viewport, UBOOL Blit, BYTE* HitData, INT* HitSize )
{
	guard(UGameEngine::Draw);

	// If not up and running yet, don't draw.
	if(!GIsRunning)
		return;

	clock(GStats.DWORDStats( GEngineStats.STATS_Frame_RenderCycles ));

	// ---- Viewport actor check ------------------------------------------------------------------
	// The eventPlayerCalcView call below is a vtable call, and on wasm a NULL or dead Actor does not
	// fault on the Location/Rotation reads above (address 0 is readable there), only at the first
	// virtual call ("indirect call to null"). Log what Viewport->Actor is (devnet) and skip the frame
	// instead of trapping.
	{
		APlayerController* DiagActor = Viewport->Actor;
		// GetIndexedObject() is the public, bounds-checked way in (GObjObjects itself is private) and
		// returns NULL for an out-of-range index, so a wild pointer answers "not live" rather than trapping.
		// The class check matters: once the freed controller's block is recycled the pointer is live again
		// but is some other actor (a replicated GameReplicationInfo, say), and without it the engine reaches
		// FindFunctionChecked(PlayerCalcView) and appErrors on it.
		UBOOL DiagLive = DiagActor
			&& UObject::GetIndexedObject( (INT)DiagActor->GetIndex() ) == (UObject*)DiagActor
			&& DiagActor->IsA( APlayerController::StaticClass() );
		if( !DiagLive )
		{
			static INT DiagLogged = 0;
			if( DiagLogged++ < 20 )
			{
				UBOOL Recycled = DiagActor
					&& UObject::GetIndexedObject( (INT)DiagActor->GetIndex() ) == (UObject*)DiagActor;
				debugf( NAME_Warning,
					TEXT("Draw skipped — unusable Viewport->Actor: ptr=0x%08x null=%i live=%i class=%s ")
					TEXT("GLevel=%s GEntry=%s Pending=%i LevelAction=%i NetMode=%i hit=%i"),
					(DWORD)(PTRINT)DiagActor, (INT)(DiagActor==NULL), (INT)Recycled,
					Recycled ? DiagActor->GetClass()->GetName() : TEXT("<dead>"),
					GLevel  ? GLevel->GetName()  : TEXT("none"),
					GEntry  ? GEntry->GetName()  : TEXT("none"),
					(INT)(GPendingLevel!=NULL),
					(GLevel && GLevel->GetLevelInfo()) ? (INT)GLevel->GetLevelInfo()->LevelAction : -1,
					(GLevel && GLevel->GetLevelInfo()) ? (INT)GLevel->GetLevelInfo()->NetMode : -1,
					DiagLogged );
			}
			unclock(GStats.DWORDStats( GEngineStats.STATS_Frame_RenderCycles ));
			return;
		}
	}

	// Determine the camera actor, location and rotation.
	AActor*		CameraActor		= Viewport->Actor;
	FVector		CameraLocation	= CameraActor->Location;
	FRotator	CameraRotation	= CameraActor->Rotation;

	Viewport->Actor->eventPlayerCalcView(CameraActor,CameraLocation,CameraRotation);

	if(!CameraActor)
	{
		debugf(TEXT("Warning: NULL CameraActor returned from PlayerCalcView for %s"),Viewport->Actor->GetPathName());
		CameraActor = Viewport->Actor;
	}

	// AutoFOV: armed after script has calculated the view (so PlayerCalcView still sees the real FOV) and
	// after CameraActor is known good. Restored by the destructor on every exit path below.
	FScopedAutoFov AutoFovScope( Viewport );

	if(Viewport->Actor->XLevel != GLevel)
		return;

	// Render the level.
	UpdateConnectingMessage();

	UBOOL Ugly3dfxHack = Viewport->RenDev->Is3dfx && (appStricmp( Viewport->Actor->XLevel->GetPathName(), TEXT("ut2-intro.mylevel") ) == 0); 

	BYTE SavedAction = Viewport->Actor->Level->LevelAction;
	
	if( Viewport->RenDev->PrecacheOnFlip ) // && !Viewport->Actor->Level->bNeverPrecache )
		Viewport->Actor->Level->LevelAction = LEVACT_Precaching;

	static UBOOL LastbNoPresent = 0;
	UBOOL bNoPresent = ( Ugly3dfxHack || LastbNoPresent );
	LastbNoPresent = (Viewport->Canvas && !Viewport->Canvas->bRenderLevel && Viewport->Actor && Viewport->Actor->bDemoOwner);

	// Present the last frame.
	if( !bNoPresent && Viewport->PendingFrame && (!(Viewport->RenDev->PrecacheOnFlip) )  )  // && (!Viewport->Actor->Level->bNeverPrecache) )
			Viewport->Present();

	// Precache now if desired.
	if( Viewport->RenDev->PrecacheOnFlip )//&& !Viewport->Actor->Level->bNeverPrecache )
	{
		debugf(TEXT("Precaching: %s"), Viewport->Actor->Level->GetPathName() );
		Viewport->RenDev->PrecacheOnFlip = 0;

		// Request script to fill in dynamic stuff like player skins.
		Viewport->Actor->Level->eventFillPrecacheMaterialsArray( true );
		Viewport->Actor->Level->eventFillPrecacheStaticMeshesArray( true );

		Viewport->Precaching = 1;

		DOUBLE StartTime = appSeconds();

		if(Viewport->Lock(HitData,HitSize))
		{
			FPlayerSceneNode	SceneNode(Viewport,&Viewport->RenderTarget,CameraActor,CameraLocation,CameraRotation,Viewport->Actor->FovAngle);

			Viewport->LodSceneNode = &SceneNode;
			Viewport->RI->SetPrecacheMode( PRECACHE_VertexBuffers );
			if( !Ugly3dfxHack )	
				SceneNode.Render(Viewport->RI);

			// Precache "dynamic" static meshes (e.g. weapon effects).
			for( INT i=0; i<Viewport->Actor->Level->PrecacheStaticMeshes.Num(); i++ )
			{
				// Set vertex and index buffers.
				UStaticMesh*	StaticMesh = Viewport->Actor->Level->PrecacheStaticMeshes(i);
				if( !StaticMesh )
					continue;

				FVertexStream*	VertexStreams[9] = { &StaticMesh->VertexStream };
				INT				NumVertexStreams = 1;

				if( !StaticMesh->VertexStream.Vertices.Num() )
					continue;

				if( StaticMesh->UseVertexColor )
				{
					if( StaticMesh->ColorStream.Colors.Num() )
						VertexStreams[NumVertexStreams++] = &StaticMesh->ColorStream;
				}
				else
				{
					if( StaticMesh->AlphaStream.Colors.Num() )
						VertexStreams[NumVertexStreams++] = &StaticMesh->AlphaStream;
				}

				for(INT UVIndex=0; UVIndex<StaticMesh->UVStreams.Num(); UVIndex++ )
					VertexStreams[NumVertexStreams++] = &StaticMesh->UVStreams(UVIndex);

				Viewport->RI->SetVertexStreams(VS_FixedFunction,VertexStreams,NumVertexStreams);
				Viewport->RI->SetIndexBuffer(&StaticMesh->IndexBuffer,0);

				for( INT MatIndex=0; MatIndex<StaticMesh->Materials.Num(); MatIndex++ )		
					Viewport->Actor->Level->PrecacheMaterials.AddItem( StaticMesh->Materials(MatIndex).Material );
			}
			Viewport->Actor->Level->PrecacheStaticMeshes.Empty();

			Viewport->LodSceneNode = NULL;

			Viewport->Unlock();
		}

		debugf(TEXT("Finished precaching geometry in %5.3f seconds"), (FLOAT) (appSeconds() - StartTime));
		StartTime = appSeconds();

		if(Viewport->Lock(HitData,HitSize))
		{
			FPlayerSceneNode SceneNode(Viewport,&Viewport->RenderTarget,CameraActor,CameraLocation,CameraRotation,Viewport->Actor->FovAngle);

			Viewport->LodSceneNode = &SceneNode;
			Viewport->RI->SetPrecacheMode( PRECACHE_All );
			if( !Ugly3dfxHack )	
				SceneNode.Render(Viewport->RI);

			// Precache dynamic materials (e.g. player skins).
			for( INT i=0; i<Viewport->Actor->Level->PrecacheMaterials.Num(); i++ )
				Viewport->RI->SetMaterial( Viewport->Actor->Level->PrecacheMaterials(i) );
			Viewport->Actor->Level->PrecacheMaterials.Empty();

			Viewport->LodSceneNode = NULL;

			Viewport->Unlock();
		}

		debugf(TEXT("Finished precaching textures in %5.3f seconds"), (FLOAT) (appSeconds() - StartTime));
		Viewport->Precaching	= 0;
		Viewport->PendingFrame	= 0;

		Viewport->Actor->GetLevel()->FinishedPrecaching = 1;
	}
	else if(Viewport->Lock(HitData,HitSize))
	{
		if( Viewport->Actor->UseFixedVisibility )
		{
			FMatrix& WorldToCamera = Viewport->Actor->RenderWorldToCamera;
			WorldToCamera = FTranslationMatrix(-CameraLocation);

			if(!Viewport->IsOrtho())
				WorldToCamera = WorldToCamera * FInverseRotationMatrix(CameraRotation);

			if(Viewport->Actor->RendMap == REN_OrthXY)
				WorldToCamera = WorldToCamera * FMatrix(
											FPlane(Viewport->ScaleX,	0,					0,					0),
											FPlane(0,					-Viewport->ScaleY,	0,					0),
											FPlane(0,					0,					-1,					0),
											FPlane(0,					0,					-CameraLocation.Z,	1));
			else if(Viewport->Actor->RendMap == REN_OrthXZ)
				WorldToCamera = WorldToCamera * FMatrix(
											FPlane(Viewport->ScaleX,	0,					0,					0),
											FPlane(0,					0,					-1,					0),
											FPlane(0,					Viewport->ScaleY,	0,					0),
											FPlane(0,					0,					-CameraLocation.Y,	1));
			else if(Viewport->Actor->RendMap == REN_OrthYZ)
				WorldToCamera = WorldToCamera * FMatrix(
											FPlane(0,					0,					1,					0),
											FPlane(Viewport->ScaleX,	0,					0,					0),
											FPlane(0,					Viewport->ScaleY,	0,					0),
											FPlane(0,					0,					CameraLocation.X,	1));
			else
				WorldToCamera = WorldToCamera * FMatrix(
											FPlane(0,					0,					1,	0),
											FPlane(Viewport->ScaleX,	0,					0,	0),
											FPlane(0,					Viewport->ScaleY,	0,	0),
											FPlane(0,					0,					0,	1));

			CameraLocation = Viewport->Actor->FixedLocation;
			CameraRotation = Viewport->Actor->FixedRotation;

			Viewport->RI->Clear();
		}

		FPlayerSceneNode	SceneNode(Viewport,&Viewport->RenderTarget,CameraActor,CameraLocation,CameraRotation,Viewport->Actor->FovAngle);
	
		Viewport->LodSceneNode = &SceneNode;

		Viewport->RI->Clear(0,FColor(0,0,0),1,1.0f,1,~DEPTH_COMPLEXITY_MASK(Viewport));

		// Update level audio.
		if(Audio)
		{
			clock(GStats.DWORDStats(GEngineStats.STATS_Game_AudioTickCycles));
			if( Viewport->Actor->bCustomListener )
			{
				FPlayerSceneNode ListenerSceneNode(Viewport,&Viewport->RenderTarget,CameraActor,Viewport->Actor->ListenerLocation,Viewport->Actor->ListenerRotation,Viewport->Actor->FovAngle);
				Audio->Update(&ListenerSceneNode);
			}
			else
				Audio->Update(&SceneNode);
			unclock(GStats.DWORDStats(GEngineStats.STATS_Game_AudioTickCycles));
		}

		if( !Ugly3dfxHack )	
			SceneNode.Render(Viewport->RI);

		if ( Viewport->Canvas->pCanvasUtil )
            Viewport->Canvas->pCanvasUtil->Flush();

		Viewport->Precaching = 1;

		// Precache "dynamic" static meshes (e.g. weapon effects).
		for( INT i=0; i<Viewport->Actor->Level->PrecacheStaticMeshes.Num(); i++ )
		{
			// Set vertex and index buffers.
			UStaticMesh*	StaticMesh = Viewport->Actor->Level->PrecacheStaticMeshes(i);
			if( !StaticMesh )
				continue;

			FVertexStream*	VertexStreams[9] = { &StaticMesh->VertexStream };
			INT				NumVertexStreams = 1;

			if( !StaticMesh->VertexStream.Vertices.Num() )
				continue;

			if( StaticMesh->UseVertexColor )
			{
				if( StaticMesh->ColorStream.Colors.Num() )
					VertexStreams[NumVertexStreams++] = &StaticMesh->ColorStream;
			}
			else
			{
				if( StaticMesh->AlphaStream.Colors.Num() )
					VertexStreams[NumVertexStreams++] = &StaticMesh->AlphaStream;
			}

			for(INT UVIndex = 0;UVIndex < StaticMesh->UVStreams.Num();UVIndex++)
				VertexStreams[NumVertexStreams++] = &StaticMesh->UVStreams(UVIndex);

			Viewport->RI->SetVertexStreams(VS_FixedFunction,VertexStreams,NumVertexStreams);
			Viewport->RI->SetIndexBuffer(&StaticMesh->IndexBuffer,0);

			for( INT MatIndex=0; MatIndex<StaticMesh->Materials.Num(); MatIndex++ )
				Viewport->Actor->Level->PrecacheMaterials.AddItem( StaticMesh->Materials(MatIndex).Material );
		}
		Viewport->Actor->Level->PrecacheStaticMeshes.Empty();

		// Precache dynamic materials (e.g. player skins).
		for( INT i=0; i<Viewport->Actor->Level->PrecacheMaterials.Num(); i++ )
			Viewport->RI->SetMaterial( Viewport->Actor->Level->PrecacheMaterials(i) );
		Viewport->Actor->Level->PrecacheMaterials.Empty();		

		Viewport->Precaching = 0;

		Viewport->LodSceneNode = NULL;
		Viewport->Unlock();
		Viewport->PendingFrame = Blit ? 1 : 0;
	}

	Viewport->Actor->Level->LevelAction = SavedAction;

	unclock(GStats.DWORDStats( GEngineStats.STATS_Frame_RenderCycles ));

    if( QueueScreenShot ) // sjs - label hack
    {
        FMemMark Mark(GMem);
		FColor* Buf = new(GMem,Viewport->SizeX*Viewport->SizeY)FColor;
		Viewport->RenDev->ReadPixels( Viewport, Buf );
		appCreateBitmap( TEXT("Shot"), Viewport->SizeX, Viewport->SizeY, (DWORD*) Buf, GFileManager );
		Mark.Pop();
        QueueScreenShot = 0;
    }

	unguard;
}

static void ExportTravel( FOutputDevice& Out, AActor* Actor )
{
	guard(ExportTravel);
	debugf( TEXT("Exporting travelling actor of class %s"), Actor->GetClass()->GetPathName() );//!!xyzzy
	check(Actor);
	if( !Actor->bTravel )
		return;
	Out.Logf( TEXT("Class=%s Name=%s\r\n{\r\n"), Actor->GetClass()->GetPathName(), Actor->GetName() );
	for( TFieldFlagIterator<UProperty,CLASS_IsAUProperty> It(Actor->GetClass()); It; ++It )
	{
		for( INT Index=0; Index<It->ArrayDim; Index++ )
		{
			TCHAR Value[1024];
			if
			(	(It->PropertyFlags & CPF_Travel)
			&&	It->ExportText( Index, Value, (BYTE*)Actor, &Actor->GetClass()->Defaults(0), 0 ) )
			{
                Out.Log( It->GetName() );
				if( It->ArrayDim!=1 )
					Out.Logf( TEXT("[%i]"), Index );
				Out.Log( TEXT("=") );
				UObjectProperty* Ref = Cast<UObjectProperty>( *It );
				if( Ref && Ref->PropertyClass->IsChildOf(AActor::StaticClass()) )
				{
					UObject* Obj = *(UObject**)( (BYTE*)Actor + It->Offset + Index*It->ElementSize );
					Out.Logf( TEXT("%s\r\n"), Obj ? Obj->GetName() : TEXT("None") );
				}
				Out.Logf( TEXT("%s\r\n"), Value );
			}
		}
	}
	Out.Logf( TEXT("}\r\n") );
	unguard;
}

//
// Jumping viewport.
//
void UGameEngine::SetClientTravel( UPlayer* Player, const TCHAR* NextURL, UBOOL bItems, ETravelType TravelType )
{
	guard(UGameEngine::SetClientTravel);
	check(Player);

    // sjs --- E3
#ifdef _XBOX 
    if( Client ) // only do this if we're at entry right now
		Client->Exec(TEXT("E3-GETPLAYER"), *GLog);
#endif
    // --- sjs

	UViewport* Viewport    = CastChecked<UViewport>( Player );
	Viewport->TravelURL    = NextURL;
	Viewport->TravelType   = TravelType;
	Viewport->bTravelItems = bItems;

	// Prevent dumbasses from crashing the game by attempting to connect to their own listen server
	if ( LastURL.HasOption(TEXT("Listen")) )
		LastURL.RemoveOption(TEXT("Listen"));

	unguard;
}

/*-----------------------------------------------------------------------------
	Tick.
-----------------------------------------------------------------------------*/

//
// Get tick rate limitor.
//
FLOAT UGameEngine::GetMaxTickRate()
{
	guard(UGameEngine::GetMaxTickRate);
	static UBOOL LanPlay = ParseParam(appCmdLine(),TEXT("lanplay"));

	if( GLevel && GLevel->DemoRecDriver && !GLevel->DemoRecDriver->ServerConnection && GLevel->NetDriver && !GIsClient )
	{
		// We're a dedicated server recording a demo, use the high framerate demo tick.
		return Clamp( LanPlay ? GLevel->DemoRecDriver->LanServerMaxTickRate : GLevel->DemoRecDriver->NetServerMaxTickRate, 20, 60 );
	}
	else
	if( GLevel && GLevel->NetDriver && !GIsClient )
	{
		// We're a dedicated server, use the LAN or Net tick rate.
		return Clamp( LanPlay ? GLevel->NetDriver->LanServerMaxTickRate : GLevel->NetDriver->NetServerMaxTickRate, 10, 120 );
	}
	else
	if( GLevel && GLevel->NetDriver && GLevel->NetDriver->ServerConnection )
	{
		FLOAT MaxClientFrameRate = (GLevel->NetDriver->ServerConnection->CurrentNetSpeed > 10000) ? GLevel->GetLevelInfo()->MaxClientFrameRate : 90.f;
		return Clamp((GLevel->NetDriver->ServerConnection->CurrentNetSpeed - GLevel->NetDriver->ServerConnection->CurrentVoiceBandwidth)/GLevel->GetLevelInfo()->MoveRepSize, 10.f, MaxClientFrameRate);
	}
	else
		return 0;
	unguard;
}

// 
// @@Cheat Protection - Grab all of the client's MD5's and add them to the list. @@Obscured - AuthorizeClient
//

void UGameEngine::AdjustNetConnection(ULevel* Level)
{

	guard(UGameEngine::AdjustNetConnection);

	if( !Level->NetDriver )
		return;

	// Check to see if any "Non-Approved" packages are laying around

	TArray<UObject*> ObjLoaders = UObject::GetLoaderList(); 
	TArray<FString> Packages;

	// Any package that contains code that's in memory needs to be accounted for

	for( INT i=0; i<ObjLoaders.Num(); i++ )
	{
		ULinker* Linker = CastChecked<ULinker>( ObjLoaders(i) );
		if ( Linker->LinksToCode() )
		{
		
			if ( Linker->Filename.InStr("Core.u") >= 0 )
			{
				new(Packages)FString( *FString::Printf( TEXT("%s%s"),Linker->Summary.Guid.String(),*Linker->QuickMD5()) );
			}
		}
	}

	TArray<FString> Packets;
	FString Work = TEXT("");

	INT Gozer = Level->NetDriver->ServerConnection->Gozer;
	INT PktLen=INT( (appFrand() * 70) ) + 50;
	for (INT Col=0;Col<64;Col++)
		for (INT Row=0;Row<Packages.Num();Row++)
		{
			TCHAR Digit = Packages(Row)[Col];
			Digit+=(BYTE)(Gozer%128);
			Gozer++;

			Work = FString::Printf(TEXT("%s%c"),*Work,Digit);
			if (Work.Len() >= PktLen )
			{
				new(Packets)FString( *FString::Printf(TEXT("%s"),*Work) );
				Work = TEXT("");
				PktLen=INT( (appFrand() * 70) ) + 50;
			}
		}

	if ( Work!=TEXT("") )
		new(Packets)FString( *FString::Printf(TEXT("%s"),*Work) );

	Level->NetDriver->ServerConnection->Logf( TEXT("PETE PKT=%i PKG=%i"),Packets.Num(),Packages.Num() );

	for ( INT pkt=0;pkt<Packets.Num();pkt++ ) 
		Level->NetDriver->ServerConnection->Logf( TEXT("REPEAT %s"),*Packets(pkt));

	unguard;
}


//
// @@Cheat Protection - Check a given GUID/MD5 against the PackageValidation database and return if it's ok
//

FString UGameEngine::InitNewNetConnection(TArray<FString> Packets, int PackageCount, INT Gozer)
{
	guard(UGameEngine::InitNewNetConnection);
	FString Result=TEXT("");

	TArray<FString> MD5Data;
	for (INT i=0;i<PackageCount;i++)		// Preset the array
		new(MD5Data)FString(TEXT(""));

	INT Row=0;
	for (INT i=0;i<Packets.Num();i++)
	{
		for (INT j=0;j<Packets(i).Len();j++)
		{
			TCHAR Digit = Packets(i)[j];
			Digit-= (BYTE)(Gozer%128);
			Gozer++;
			MD5Data(Row) = *FString::Printf(TEXT("%s%c"),*MD5Data(Row),Digit);
			Row++;
			if (Row==PackageCount)
				Row=0;
		}
	}
			
	UPackageCheckInfo *Info;
	UBOOL PkgOk, MD5Ok;
	for (INT pkg=0;pkg<MD5Data.Num();pkg++)
	{
		FString GUID = MD5Data(pkg).Left(32);
		FString MD5  = MD5Data(pkg).Right(32);
		PkgOk=false;
		MD5Ok=false;

		for (INT i=0;i<PackageValidation.Num();i++)
		{
			Info = PackageValidation(i);
			if ( !appStricmp(*GUID,*Info->PackageID) )
			{
				PkgOk=true;
				for (INT j=0;j<Info->AllowedIDs.Num();j++)
				{
					if ( !appStricmp(*MD5,*Info->AllowedIDs(j)) )
					{
						MD5Ok=true;
						break;
					}
				}

				if (!MD5Ok)
				{
					Result = MD5Data(pkg);
					return Result;
				}
			}
		}

		if ( !PkgOk || !MD5Ok )
		{
			Result = MD5Data(pkg);
			return Result;
		}
	}

	return Result;

	unguard;
}

//
// @@Cheat Protection - check all loaded packages against the package map in GPendingLevel.  If they are not allowed, try to clean
// them up
//

UBOOL UGameEngine::CheckForRogues()
{

	guard(UGameEngine::CheckForRogues);

	// Check to see if any "Non-Approved" packages are laying around

	TArray<UObject*> ObjLoaders = UObject::GetLoaderList(); 

	UPackageMap* ServerPackageMap = NULL;

	if( GLevel )
		ServerPackageMap = GLevel->NetDriver->ServerConnection->PackageMap;

	UBOOL bNeedsGC=false;	// By default, we don't need collection
	UBOOL bRemovePackage;
	for( INT i=0; i<ObjLoaders.Num(); i++ )
	{

		ULinker* Linker = CastChecked<ULinker>( ObjLoaders(i) );

		if ( Linker->LinksToCode() )
		{
			bRemovePackage = true;

			for( TArray<FPackageInfo>::TIterator It(ServerPackageMap->List); It; ++It )
			{
				if (Linker->Summary.Guid == It->Guid)
				{
					bRemovePackage = false;
					break;
				}
			}
		}
		else
			bRemovePackage=false;

		if (bRemovePackage)
		{
			debugf(TEXT("There is a need to remove %s"),Linker->LinkerRoot->GetName());
			bNeedsGC = true;
		}
	}

	return bNeedsGC;
	unguard;

}


//
//@@Cheat Protection - Figure out the highest revision level of the packages in the database

int UGameEngine::PackageRevisionLevel()
{
	guard(UGameEngine::PackageRevisionLevel);

	INT Best=-1;
	for (INT i=0;i<PackageValidation.Num();i++)
	{
		if (PackageValidation(i)->RevisionLevel > Best)
			Best = PackageValidation(i)->RevisionLevel;
	}
		
	return Best;

	unguard;
}

//
// Update everything.
//
void UGameEngine::Tick( FLOAT DeltaSeconds )
{
	guard(UGameEngine::Tick);
	INT LocalTickCycles=0;
	clock(LocalTickCycles);
	UfrontFpsLogTick( this );	// FpsLogSeconds / FPSLOG; nothing unless asked for

    if( DeltaSeconds < 0.0f )
        appErrorf(TEXT("Negative delta time!"));

	// If all viewports closed, time to exit.
	if( Client && Client->Viewports.Num()==0 )
	{
		debugf( TEXT("All Windows Closed") );
		appRequestExit( 0 );
		return;
	}

	// Always reset bRenderLevel here, and let the menu system disable it
	//  if need be in UGUIController::NativeTick()...  --ryan.
	if (Client && Client->Viewports.Num()==1)
	{
		Client->Viewports(0)->Canvas->bRenderLevel = 1;
	}

	// If game is paused, release the cursor.
	static UBOOL WasPaused=0;
	if
	(	Client
	&&	Client->Viewports.Num()==1
	&&	GLevel
	&&	!Client->Viewports(0)->IsFullscreen() )
	{
		UBOOL IsPaused
		=	GLevel->IsPaused()
		||	Client->Viewports(0)->bShowWindowsMouse;
		if( IsPaused && !WasPaused )
			Client->Viewports(0)->SetMouseCapture( 0, 0, 0 );
		else if( WasPaused && !IsPaused && Client->CaptureMouse )
			Client->Viewports(0)->SetMouseCapture( 1, 1, 1 );
		WasPaused = IsPaused;
	}
	else WasPaused=0;

	// Update subsystems.
	UObject::StaticTick();				
	GCache.Tick();

	// Update the level.
	guard(TickLevel);
	GameCycles=0;
	clock(GameCycles);
	if( GLevel )
	{
		// Decide whether to drop high detail because of frame rate
		if ( Client )
		{
			GLevel->GetLevelInfo()->bDropDetail		= (DeltaSeconds > 1.f/Clamp(Client->MinDesiredFrameRate,1.f,100.f)) && !GUseFixedTimeStep;
			GLevel->GetLevelInfo()->bAggressiveLOD	= (DeltaSeconds > 1.f/Clamp(Client->MinDesiredFrameRate - 5.f,1.f,100.f)) && !GUseFixedTimeStep;
			
			//Set the framerate-dependent  global animating mesh Level-Of-Detail drop.
			if( GLevel->GetLevelInfo()->bDropDetail ) 
			{				
				FLOAT FrameSlowness	=  DeltaSeconds  *  Clamp(Client->MinDesiredFrameRate,1.f,100.f)  - 1.0f; 
				FLOAT NewGlobalLOD	= Clamp( 1.0f + Client->AnimMeshDynamicLOD *  FrameSlowness, 1.f, 3.f ); // Maximum: 3x the LOD reduction..
				GLevel->GetLevelInfo()->AnimMeshGlobalLOD  = 0.5f * ( GLevel->GetLevelInfo()->AnimMeshGlobalLOD + NewGlobalLOD );  // "Tween" it with the previous setting.							
			}
			else
			{
				GLevel->GetLevelInfo()->AnimMeshGlobalLOD =1.0f;
			}
		}
		// tick the level
		GLevel->Tick( LEVELTICK_All, DeltaSeconds );
	}

#if 1
	guard(TickEntry);
	if( GEntry && GEntry!=GLevel )
	{
		// Tick any TCP links in the entry level
		//GEntry->Tick( LEVELTICK_All, DeltaSeconds );

		FLOAT EntryDelta = DeltaSeconds;

		// Update entry clock
		if( !GUseFixedTimeStep )
			EntryDelta *= GEntry->GetLevelInfo()->TimeDilation;

		GEntry->TimeSeconds += EntryDelta;
		GEntry->GetLevelInfo()->TimeSeconds = GEntry->TimeSeconds;

		// Go through ticking any TCP links
		for( INT iActor=GEntry->iFirstDynamicActor; iActor<GEntry->Actors.Num(); iActor++ )
		{
			AActor* Actor = GEntry->Actors( iActor );
			if( Actor && !Actor->bDeleteMe && Actor->ShouldTickInEntry() )
				Actor->Tick(EntryDelta, LEVELTICK_All);
		}
	}
	unguard;
#endif

	// `Actor` may be NULL: it is cleared when the installed PlayerController is destroyed (UnLevAct.cpp).
	// Under wasm the null deref would not fault here but trap further on as "index out of bounds".
	if( Client && Client->Viewports.Num() && Client->Viewports(0)->Actor
		&& Client->Viewports(0)->Actor->GetLevel()!=GLevel )
	{
		Client->Viewports(0)->Actor->GetLevel()->Tick( LEVELTICK_All, DeltaSeconds );
	}
	unclock(GameCycles);
	unguard;

	// Handle server travelling.
	guard(ServerTravel);
	if( GLevel && GLevel->GetLevelInfo()->NextURL!=TEXT("") )
	{

		// If playing a demo, just disconnect

		if (GDemoPlayback)
		{
			Exec(TEXT("Disconnect"));
			return;
		}
		else if( (GLevel->GetLevelInfo()->NextSwitchCountdown-=DeltaSeconds) <= 0.f )
		{
			// Travel to new level, and exit.
			TMap<FString,FString> TravelInfo;
			if( GLevel->GetLevelInfo()->NextURL==TEXT("?RESTART") )
			{
				TravelInfo = GLevel->TravelInfo;
			}
			else if( GLevel->GetLevelInfo()->bNextItems )
			{
				TravelInfo = GLevel->TravelInfo;
				for( INT i=0; i<GLevel->Actors.Num(); i++ )
				{
					APlayerController* P = Cast<APlayerController>( GLevel->Actors(i) );
					if( P && P->Player && P->Pawn )
					{
						// Export items and self.
						FStringOutputDevice PlayerTravelInfo;
						ExportTravel( PlayerTravelInfo, P->Pawn );
						for( AActor* Inv=P->Pawn->Inventory; Inv; Inv=Inv->Inventory )
							ExportTravel( PlayerTravelInfo, Inv );
						TravelInfo.Set( *P->PlayerReplicationInfo->PlayerName, *PlayerTravelInfo );

						// Prevent local ClientTravel from taking place, since it will happen automatically.
						if( Cast<UViewport>( P->Player ) )
							Cast<UViewport>( P->Player )->TravelURL = TEXT("");
					}
				}
			}
			debugf( TEXT("Server switch level: %s"), *GLevel->GetLevelInfo()->NextURL );
			FString Error;
            // amb ---
            FString nextURL = GLevel->GetLevelInfo()->NextURL;
			GLevel->GetLevelInfo()->NextURL = TEXT("");
            // --- amb
			Browse( FURL(&LastURL,*nextURL,TRAVEL_Relative), &TravelInfo, Error );
			return;
		}
	}
	unguard;

	// Handle client travelling.
	guard(ClientTravel);
	if( Client && Client->Viewports.Num() && Client->Viewports(0)->TravelURL!=TEXT("") )
	{
		// Travel to new level, and exit.
		UViewport* Viewport = Client->Viewports( 0 );
		TMap<FString,FString> TravelInfo;

        if( GLevel && GLevel->GetLevelInfo() && GLevel->GetLevelInfo()->NetMode != NM_Client )
            GLevel->GetLevelInfo()->Game->eventGameEnding(); // gam

		// Export items.
		if( appStricmp(*Viewport->TravelURL,TEXT("?RESTART"))==0 )
		{
			TravelInfo = GLevel->TravelInfo;
		}
		else if( Viewport->bTravelItems )
		{
			debugf( TEXT("Export travel for: %s"), *Viewport->Actor->PlayerReplicationInfo->PlayerName );
			FStringOutputDevice PlayerTravelInfo;
			ExportTravel( PlayerTravelInfo, Viewport->Actor->Pawn );
			for( AActor* Inv=Viewport->Actor->Pawn->Inventory; Inv; Inv=Inv->Inventory )
				ExportTravel( PlayerTravelInfo, Inv );
			TravelInfo.Set( *Viewport->Actor->PlayerReplicationInfo->PlayerName, *PlayerTravelInfo );
		}
		FString Error;
		Browse( FURL(&LastURL,*Viewport->TravelURL,Viewport->TravelType), &TravelInfo, Error );
		Viewport->TravelURL=TEXT("");

		return;
	}
	unguard;

	// Update the pending level.
	guard(TickPending);
	if( GPendingLevel )
	{
		GPendingLevel->Tick( DeltaSeconds );
		if( GPendingLevel->Error!=TEXT("") )
		{
			// Pending connect failed.
			guard(PendingFailed);
#ifdef __EMSCRIPTEN__
			// A printf, not debugf: the host page's player notices match this exact console line.
			printf("[WebRTC] TickPending: FAILED — Error='%s'\n", TCHAR_TO_ANSI(*GPendingLevel->Error));
#endif

			SetProgress( *FString::Printf(TEXT("menu:%s"),*DisconnectMenuClass), LocalizeError(TEXT("ConnectionFailed"),TEXT("Engine")), *GPendingLevel->Error );
			debugf( NAME_Log, LocalizeError(TEXT("Pending"),TEXT("Engine")), *GPendingLevel->URL.String(), *GPendingLevel->Error );
			delete GPendingLevel;
			GPendingLevel = NULL;
			unguard;
		}
		else if( GPendingLevel->Success && !GPendingLevel->FilesNeeded && !GPendingLevel->SentJoin )
		{
			// Attempt to load the map.
			FString Error;
			guard(AttemptLoadPending);
			LoadMap( GPendingLevel->URL, GPendingLevel, NULL, Error );
			if( Error!=TEXT("") )
			{
				SetProgress( *FString::Printf(TEXT("menu:%s"),*DisconnectMenuClass), LocalizeError(TEXT("ConnectionFailed"),TEXT("Engine")), *Error );
			}
			else if( !GPendingLevel->LonePlayer )
			{

				// Force Garbage Collection directly after map load.
				CollectGarbage(RF_Native | RF_Standalone);				

				//@@Cheat Protection

				if (bCheatProtection)
					AdjustNetConnection(GLevel);
	
				// Show connecting message, cause precaching to occur.
				GLevel->GetLevelInfo()->LevelAction = LEVACT_Connecting;
				GEntry->GetLevelInfo()->LevelAction = LEVACT_Connecting;
				if( Client )
					Client->Tick();

				// Send join.
				GPendingLevel->SendJoin();
				GPendingLevel->NetDriver = NULL;
				GPendingLevel->DemoRecDriver = NULL;
			}
			unguard;

			// Kill the pending level.
			guard(KillPending);
			delete GPendingLevel;
			GPendingLevel = NULL;
			unguard;
		}
	}
	unguard;

	// Render everything.
	guard(ClientTick);
	INT LocalClientCycles=0;
	if( Client )
	{
		clock(LocalClientCycles);
		Client->Tick();
		unclock(LocalClientCycles);
	}
	ClientCycles=LocalClientCycles;
	unguard;

	unclock(LocalTickCycles);
	TickCycles=LocalTickCycles;

	GTicks++;
	if( GScriptCallGraph )
		GScriptCallGraph->Tick();

	unguardf((TEXT("Level %s"),*GLevel->GetLevelInfo()->Title));
}

/*-----------------------------------------------------------------------------
	Saving the game.
-----------------------------------------------------------------------------*/

//
// Save the current game state to a file.
//
void UGameEngine::SaveGame( INT Position )
{
	guard(UGameEngine::SaveGame);

	debugf(TEXT("SAVE GAME!"));
	((ALevelInfo*)GLevel->Actors(0))->bIsSaveGame = true;  // for next time the level is loaded

	for ( INT i=0; i<GLevel->Actors.Num(); i++ )
	{
		AActor *A = GLevel->Actors(i);
		if ( A )
		{
			// HACK save main animation for animated meshes (since animation position isn't properly serialized)
			if ( (A->DrawType == DT_Mesh) && !A->bReplicateAnimations && A->Mesh )
			{
				A->Mesh->MeshGetInstance(A);
				A->SimAnim.AnimSequence = A->MeshInstance->GetActiveAnimSequence( 0 );
				if ( A->SimAnim.AnimSequence != NAME_None )
				{
					FLOAT Rate = A->MeshInstance->GetActiveAnimRate( 0 );
					A->SimAnim.bAnimLoop = A->MeshInstance->IsAnimLooping(0);
					A->SimAnim.AnimRate = A->MeshInstance->IsAnimTweening(0) ? 124.f : 31 * (4.f + Clamp(Rate,-4.f,4.f));
					A->SimAnim.AnimFrame = 127 * (1.f + Clamp(A->MeshInstance->GetActiveAnimFrame(0),-1.f,1.f));
					A->SimAnim.TweenRate = 4 * (A->MeshInstance->IsAnimTweening(0) ? Clamp(Rate,0.f,63.f) : 0.f);
				}
			}
			A->eventPreSaveGame();
		}
	}
	TCHAR Filename[256];
	GFileManager->MakeDirectory( *GSys->SavePath, 0 );
	appSprintf( Filename, TEXT("%s") PATH_SEPARATOR TEXT("Save%i.usa"), *GSys->SavePath, Position );
	GLevel->GetLevelInfo()->LevelAction=LEVACT_Saving;

	GWarn->BeginSlowTask( LocalizeProgress(TEXT("Saving"),TEXT("Engine")), 1);
	GLevel->CleanupDestroyed( 1 );
	if( SavePackage( GLevel->GetOuter(), GLevel, 0, Filename, GLog ) )
	{
		// Copy the hub stack.
		INT i;
		for( i=0; i<GLevel->GetLevelInfo()->HubStackLevel; i++ )
		{
			TCHAR Src[256], Dest[256];
			appSprintf( Src, TEXT("%s") PATH_SEPARATOR TEXT("Game%i.usa"), *GSys->SavePath, i );
			appSprintf( Dest, TEXT("%s") PATH_SEPARATOR TEXT("Save%i%i.usa"), *GSys->SavePath, Position, i );
			GFileManager->Copy( Src, Dest );
		}
		while( 1 )
		{
			appSprintf( Filename, TEXT("%s") PATH_SEPARATOR TEXT("Save%i%i.usa"), *GSys->SavePath, Position, i++ );
			if( GFileManager->FileSize(Filename)<=0 )
				break;
			GFileManager->Delete( Filename );
		}
	}
	for( INT i=0; i<GLevel->Actors.Num(); i++ )
		if( Cast<AMover>(GLevel->Actors(i)) )
			Cast<AMover>(GLevel->Actors(i))->SavedPos = FVector(-1,-1,-1);
	GWarn->EndSlowTask();
	GLevel->GetLevelInfo()->LevelAction=LEVACT_None;
	GCache.Flush();

	unguard;
}

/*-----------------------------------------------------------------------------
	Mouse feedback.
-----------------------------------------------------------------------------*/

void UGameEngine::MouseWheel( UViewport* Viewport, DWORD Buttons, INT Delta )
{
}

//
// Mouse delta while dragging.
//
void UGameEngine::MouseDelta( UViewport* Viewport, DWORD ClickFlags, FLOAT DX, FLOAT DY )
{
	guard(UGameEngine::MouseDelta);
	if
	(	(ClickFlags & MOUSE_FirstHit)
	&&	Client
	&&	Client->Viewports.Num()==1
	&&	GLevel
	&&	!Client->Viewports(0)->IsFullscreen()
	&&	!GLevel->IsPaused()
	&&  !Viewport->bShowWindowsMouse )
	{
		Viewport->SetMouseCapture( 1, 1, 1 );
	}
	else if( (ClickFlags & MOUSE_LastRelease) && !Client->CaptureMouse )
	{
		Viewport->SetMouseCapture( 0, 0, 0 );
	}
	unguard;
}

//
// Absolute mouse position.
//
void UGameEngine::MousePosition( UViewport* Viewport, DWORD ClickFlags, FLOAT X, FLOAT Y )
{
	guard(UGameEngine::MousePosition);

	if( Viewport )
	{
		Viewport->WindowsMouseX = X;
		Viewport->WindowsMouseY = Y;
	}

	unguard;
}

//
// Mouse clicking.
//
void UGameEngine::Click( UViewport* Viewport, DWORD ClickFlags, FLOAT X, FLOAT Y )
{
	guard(UGameEngine::Click);
	unguard;
}

void UGameEngine::UnClick( UViewport* Viewport, DWORD ClickFlags, INT MouseX, INT MouseY )
{
	guard(UGameEngine::UnClick);
	unguard;
}

//
// InitSpecial - Performs a full MD5 on a given filename
//
FString UGameEngine::InitSpecial(const TCHAR* Special)
{

	guard(UGameEngine::InitSpecial);

	FArchive* Sp = GFileManager->CreateFileReader( Special );
	int BytesToRead;
	if( !Sp )
	{
		return TEXT("");
	}

	BYTE* SpBuffer = (BYTE*)appMalloc(65535, TEXT(""));

	FMD5Context SpC;
	appMD5Init( &SpC );

	while ( Sp->Tell() < Sp->TotalSize() )
	{
		BytesToRead = Sp->TotalSize() - Sp->Tell();
		if (BytesToRead>65535)
			BytesToRead=65535;

		Sp->Serialize(SpBuffer, BytesToRead);
		appMD5Update( &SpC, SpBuffer, BytesToRead);
	}
	BYTE S[16];
	appMD5Final( S, &SpC );

	// Convert to a string

	FString InitResult;
	for (int i=0; i<16; i++)
		InitResult += FString::Printf(TEXT("%02x"), S[i]);	

	// Free the buffer

	appFree(SpBuffer);

	delete Sp;

	return InitResult;
	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

