/*=============================================================================
    SDL2Client.cpp: USDL2Client code.
    SDL2 viewport client implementation for UT2004 web port.
=============================================================================*/

#include "SDL2Drv.h"

/*-----------------------------------------------------------------------------
    Class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(USDL2Client);

/*-----------------------------------------------------------------------------
    USDL2Client implementation.
-----------------------------------------------------------------------------*/

//
// USDL2Client constructor.
//
USDL2Client::USDL2Client()
{
    guard(USDL2Client::USDL2Client);
    InMenuLoop = 0;
    LastCurrent = NULL;
    unguard;
}

//
// Static init.
//
void USDL2Client::StaticConstructor()
{
    guard(USDL2Client::StaticConstructor);
    new(GetClass(),TEXT("StartupFullscreen"), RF_Public)UBoolProperty(CPP_PROPERTY(StartupFullscreen), TEXT("Display"), CPF_Config );
    unguard;
}

//
// Initialize the platform-specific viewport manager subsystem.
//
void USDL2Client::Init( UEngine* InEngine )
{
    guard(USDL2Client::Init);

    // Init base.
    UClient::Init( InEngine );

#if defined(__ANDROID__)
    // Android also brings up the game-controller subsystem (2.10d gamepad).
    // SDL_INIT_GAMECONTROLLER implies SDL_INIT_JOYSTICK and makes SDL emit
    // SDL_CONTROLLER* events, which USDL2Viewport::UpdateInput turns into IK_Joy*
    // input. Web/desktop keep SDL_INIT_VIDEO only -> byte-identical.
    const Uint32 SDL2DrvInitFlags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER;
#else
    const Uint32 SDL2DrvInitFlags = SDL_INIT_VIDEO;
#endif
    if (SDL_Init(SDL2DrvInitFlags) == -1)
    {
        const TCHAR *err = appFromAnsi(SDL_GetError());
        appErrorf(TEXT("Couldn't initialize SDL2: %s\n"), err);
    }

    // Enable text input events for console/chat.
    SDL_EventState(SDL_TEXTINPUT, SDL_ENABLE);

    // Initialize scancode-to-EInputKey mapping table.
    USDL2Viewport::InitKeyMap();

    // Note configuration.
    PostEditChange();

    // Default res option.
    if( ParseParam(appCmdLine(),TEXT("defaultres")) )
    {
        WindowedViewportX  = FullscreenViewportX  = MenuViewportX   = 640;
        WindowedViewportY  = FullscreenViewportY  = MenuViewportY   = 480;
    }

    debugf( NAME_Init, TEXT("SDL2Client initialized.") );

    unguard;
}

//
// Shut down the platform-specific viewport manager subsystem.
//
void USDL2Client::Destroy()
{
    guard(USDL2Client::Destroy);

    // Make sure to shut down Viewports first.
    for( INT i=0; i<Viewports.Num(); i++ )
        Viewports(i)->ConditionalDestroy();

    // Shut down GRenDev.
    Engine->GRenDev->Exit(NULL);

#if defined(__ANDROID__)
    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
#else
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
#endif

    debugf( NAME_Exit, TEXT("SDL2 client shut down") );
    Super::Destroy();
    unguard;
}

void USDL2Client::TeardownSR()
{
    guard(USDL2Client::TeardownSR);
    // no-op, currently.
    unguard;
}

//
// Failsafe routine to shut down viewport manager subsystem
// after an error has occurred. Not guarded.
//
void USDL2Client::ShutdownAfterError()
{
    debugf( NAME_Exit, TEXT("Executing USDL2Client::ShutdownAfterError") );
    SDL_Quit();

    if (Engine && Engine->Audio)
    {
        Engine->Audio->ConditionalShutdownAfterError();
    }

    for (INT i = Viewports.Num() - 1; i >= 0; i--)
    {
        USDL2Viewport *Viewport = (USDL2Viewport *) Viewports(i);
        Viewport->ConditionalShutdownAfterError();
    }

    Super::ShutdownAfterError();
}

void USDL2Client::NotifyDestroy( void* Src )
{
    guard(USDL2Client::NotifyDestroy);
    unguard;
}

//
// Command line.
//
UBOOL USDL2Client::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
    guard(USDL2Client::Exec);
    if( UClient::Exec( Cmd, Ar ) )
    {
        return 1;
    }
    return 0;
    unguard;
}

//
// Perform timer-tick processing on all visible viewports.
//
void USDL2Client::Tick()
{
    guard(USDL2Client::Tick);

    // Blit any viewports that need blitting.
    for( INT i=0; i<Viewports.Num(); i++ )
    {
        USDL2Viewport* Viewport = CastChecked<USDL2Viewport>(Viewports(i));
        if ((Viewport->IsRealtime() || (Viewport->DirtyViewport != 0)) && Viewport->SizeX && Viewport->SizeY )
            Viewport->Repaint( (Viewport->DirtyViewport == -1) ? 0 : 1 );
    }

    unguard;
}

//
// Create a new viewport.
//
UViewport* USDL2Client::NewViewport( const FName Name )
{
    guard(USDL2Client::NewViewport);
    return new( this, Name )USDL2Viewport();
    unguard;
}

//
// Configuration change.
//
void USDL2Client::PostEditChange()
{
    guard(USDL2Client::PostEditChange);
    Super::PostEditChange();
    unguard;
}

//
// Enable or disable all viewport windows that have ShowFlags set (or all if ShowFlags=0).
//
void USDL2Client::EnableViewportWindows( DWORD ShowFlags, int DoEnable )
{
    guard(USDL2Client::EnableViewportWindows);
    /* no-op for SDL2 */
    unguard;
}

//
// Show or hide all viewport windows that have ShowFlags set (or all if ShowFlags=0).
//
void USDL2Client::ShowViewportWindows( DWORD ShowFlags, int DoShow )
{
    guard(USDL2Client::ShowViewportWindows);
    /* no-op for SDL2 */
    unguard;
}

//
// Make this viewport the current one.
// If Viewport=0, makes no viewport the current one.
//
void USDL2Client::MakeCurrent( UViewport* InViewport )
{
    guard(USDL2Client::MakeCurrent);
    for( INT i=0; i<Viewports.Num(); i++ )
    {
        UViewport* OldViewport = Viewports(i);
        if( OldViewport->Current && OldViewport!=InViewport )
        {
            OldViewport->Current = 0;
            OldViewport->UpdateWindowFrame();
        }
    }
    if( InViewport )
    {
        LastCurrent = InViewport;
        InViewport->Current = 1;
        InViewport->UpdateWindowFrame();
    }
    unguard;
}

// Returns a pointer to the viewport that was last current.
UViewport* USDL2Client::GetLastCurrent()
{
    guard(USDL2Client::GetLastCurrent);
    return LastCurrent;
    unguard;
}

/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/
