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

    // ★ COMPILED DEFAULTS FOR THE INHERITED UClient SETTINGS (ufront 2.35).
    //
    // UClient::StaticConstructor (Engine/Src/UnCamMgr.cpp) REGISTERS ~30 CPF_Config properties and assigns
    // NOT ONE VALUE, so without a [SDL2Drv.SDL2Client] ini section every one of them falls back to a
    // zero-initialised default -- and the zeroes are not merely conservative, they are wrong:
    //
    //   TextureLODSet[*] = 0  -> UltraHigh. The ETextureDetail enum lists UltraHigh FIRST, so the zero
    //                            default is the single MOST EXPENSIVE value, not a safe middle.
    //   Brightness/Contrast/Gamma = 0 -> a black picture.
    //   WindowedViewportX/Y = 0 -> a 0x0 window.
    //   Decals/Coronas/DecoLayers/Projectors/ScreenFlashes = 0 -> silently off.
    //
    // This is the same trap GL4ESDrv had before DetailTextures and UseVBO got real compiled defaults (see
    // UGL4ESRenderDevice::StaticConstructor) -- and it is not hypothetical here either: a RETAIL UT2004.ini
    // has no [SDL2Drv.SDL2Client] section at all, because SDL2 did not exist in 2004, so any install seeded
    // from one gets the whole table above. Measured in the sibling engine on 2026-08-11: engine-ut99's
    // run-tree ini has [NSDLDrv.NSDLClient] WindowedViewportX=0 -> a 320x240 window under Xvfb, and
    // Brightness=0.000000 -> CityIntro renders as an apparently black frame.
    //
    // The values below are exactly the ones Engine/Config/Default.ini already ships for this section (and
    // its web/ twin, byte-identical), so the compiled default and the documented ini now AGREE -- which is
    // the property that was missing. Nothing visible changes for an install that has the section; this is
    // for the installs that do not. Every line stays overridable by ini, like any other config option.
    CaptureMouse            = 1;
    ScreenFlashes           = 1;
    NoLighting              = 0;
    Decals                  = 1;
    NoDynamicLights         = 0;
    NoFractalAnim           = 0;
    Coronas                 = 1;
    DecoLayers              = 1;
    Projectors              = 1;
    ReportDynamicUploads    = 0;
    WeatherEffects          = 1;
    StartupFullscreen       = 0;

    // ⚠ The web build does NOT take its render size from these: USDL2Viewport::ResizeViewport queries the
    // canvas whenever the requested size is 0x0, and the browser viewport is what governs there (measured
    // 2026-08-11 across a 640x480 -> 2560x1440 sweep). These are the NATIVE window size and the fallback.
    WindowedViewportX       = 1024;
    WindowedViewportY       = 768;
    FullscreenViewportX     = 1280;
    FullscreenViewportY     = 720;
    MenuViewportX           = 640;
    MenuViewportY           = 480;

    Brightness              = 0.8f;
    Contrast                = 0.7f;
    Gamma                   = 0.8f;

    MinDesiredFrameRate     = 35.0f;
    ScaleHUDX               = 0.0f;
    AnimMeshDynamicLOD      = 0.0f;
    DrawDistanceLOD         = 1.0f;

    // UltraHigh (== 0) for every LOD set: what SDL1's section shipped, what this section ships today, and
    // -- by coincidence -- what the zero default already produced. Written out so it is a DECISION rather
    // than an accident; the accident is exactly what made the table above survive this long.
    for( INT i=0; i<LODSET_MAX; i++ )
        TextureLODSet[i] = 0;

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
    // input.
    const Uint32 SDL2DrvInitFlags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER;
#else
    // ufront (the UC2 port's pads): the web and the desktop get pads too, always (the owner, 2026-10-11: "Always on, no
    // switch"; one pad, the first connected; USDL2Viewport::UpdateInput). There is deliberately NO key: the
    // [SDL2Drv.SDL2Client] UseJoystick=False the inis carried since the fork was read by nothing, and every web
    // player's saved UT2004.ini still has it, so honouring it now would keep pads off for all of them. Pads already
    // connected arrive as SDL_CONTROLLERDEVICEADDED events like later ones.
    const Uint32 SDL2DrvInitFlags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER;
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
