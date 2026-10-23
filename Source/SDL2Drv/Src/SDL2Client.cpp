/*=============================================================================
    SDL2Client.cpp: USDL2Client, the SDL2 viewport manager.
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
    // The retail key the game's own "Enable Joystick" option writes (GUI2K4: `set ini:Engine.Engine.ViewportManager
    // UseJoystick`); on by default.
    new(GetClass(),TEXT("UseJoystick"),       RF_Public)UBoolProperty(CPP_PROPERTY(UseJoystick),       TEXT("Joystick"), CPF_Config );
    // The web's WebGL context version (USDL2Viewport::ResizeViewport); read by nothing natively.
    new(GetClass(),TEXT("WebGLVersion"),      RF_Public)UIntProperty (CPP_PROPERTY(WebGLVersion),      TEXT("Display"), CPF_Config );
    // The pad's menu cursor speed (USDL2Viewport::MovePadCursor); read every tick.
    new(GetClass(),TEXT("PadCursorSeconds"),  RF_Public)UFloatProperty(CPP_PROPERTY(PadCursorSeconds), TEXT("Joystick"), CPF_Config );

    // Compiled defaults for the inherited UClient settings. UClient::StaticConstructor (Engine/Src/UnCamMgr.cpp)
    // registers its CPF_Config properties without values, and a retail UT2004.ini has no [SDL2Drv.SDL2Client]
    // section, so without these every one would be zero, and the zeroes are wrong:
    //
    //   TextureLODSet[*] = 0  -> UltraHigh. The ETextureDetail enum lists UltraHigh first, so the zero
    //                            default is the most expensive value, not a safe middle.
    //   Brightness/Contrast/Gamma = 0 -> a black picture.
    //   WindowedViewportX/Y = 0 -> a 0x0 window.
    //   Decals/Coronas/DecoLayers/Projectors/ScreenFlashes = 0 -> silently off.
    //
    // The values are the ones Engine/Config/Default.ini (and its web twin) ships for this section; the ini wins
    // wherever it sets a value.
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
    UseJoystick             = 1;
    WebGLVersion            = 1;
    PadCursorSeconds        = 3.0f;

    // The web build does not take its render size from these: USDL2Viewport::ResizeViewport queries the canvas
    // whenever the requested size is 0x0. These are the native window size and the fallback.
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

    // UltraHigh (== 0) for every LOD set, as the shipped section says.
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

    // The game-controller subsystem: SDL_INIT_GAMECONTROLLER implies SDL_INIT_JOYSTICK and makes SDL emit
    // SDL_CONTROLLER* events (one pad, the first connected; USDL2Viewport::UpdateInput). Whether a pad is used
    // is UseJoystick, read every tick, so the game's own option takes effect at once. Pads already connected
    // arrive as SDL_CONTROLLERDEVICEADDED events like later ones.
    const Uint32 SDL2DrvInitFlags = SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER;
    if (SDL_Init(SDL2DrvInitFlags) == -1)
    {
        const TCHAR *err = appFromAnsi(SDL_GetError());
        appErrorf(TEXT("Couldn't initialize SDL2: %s\n"), err);
    }

    // Enable text input events for console/chat.
    SDL_EventState(SDL_TEXTINPUT, SDL_ENABLE);

    // Initialize scancode-to-EInputKey mapping table.
    USDL2Viewport::InitKeyMap();

    // UseJoystick is turned on once for an ini that has not recorded UseJoystickDefaulted: older inis carry
    // `[SDL2Drv.SDL2Client] UseJoystick=False`, which no build read, and honoured as it stands it would turn
    // pads off. From then on the player's own choice stands.
    {
        UBOOL Defaulted = 0;
        GConfig->GetBool( TEXT("SDL2Drv.SDL2Client"), TEXT("UseJoystickDefaulted"), Defaulted );
        if( !Defaulted )
        {
            UseJoystick = 1;
            GConfig->SetBool( TEXT("SDL2Drv.SDL2Client"), TEXT("UseJoystick"), 1 );
            GConfig->SetBool( TEXT("SDL2Drv.SDL2Client"), TEXT("UseJoystickDefaulted"), 1 );
        }
        debugf( NAME_Log, TEXT("SDL2Client: game pads %s (UseJoystick)"), UseJoystick ? TEXT("on") : TEXT("off") );
    }

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

    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);

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
