/*=============================================================================
    SDL2Drv.h: SDL2 viewport and input driver.
=============================================================================*/

#ifndef _INC_SDL2DRV
#define _INC_SDL2DRV

/*----------------------------------------------------------------------------
    API.
----------------------------------------------------------------------------*/

#ifndef SDL2DRV_API
    #define SDL2DRV_API DLL_IMPORT
#endif

/*----------------------------------------------------------------------------
    Dependencies.
----------------------------------------------------------------------------*/

// SDL2DRV_REGISTRATION_ONLY (SDLLaunch's static registration): stand-ins for
// the SDL types, without the SDL headers.
#ifdef SDL2DRV_REGISTRATION_ONLY
    struct SDL_Window;
    typedef void* SDL_GLContext;
    enum { SDL2DRV_MAX_SCANCODES = 512 };
#else
    #include "SDL.h"
    enum { SDL2DRV_MAX_SCANCODES = SDL_NUM_SCANCODES };
#endif

// Unreal includes.
#include "Engine.h"
#include "UnRender.h"

/*-----------------------------------------------------------------------------
    Declarations.
-----------------------------------------------------------------------------*/

// Classes.
class USDL2Viewport;
class USDL2Client;
class FSDL2InputTest;

/*-----------------------------------------------------------------------------
    USDL2Client.
-----------------------------------------------------------------------------*/

//
// SDL2 implementation of the client.
//
class USDL2Client : public UClient, public FNotifyHook
{
    DECLARE_CLASS(USDL2Client,UClient,CLASS_Transient|CLASS_Config,SDL2Drv)

    // Configuration.
    BITFIELD            StartupFullscreen;
    BITFIELD            UseJoystick;        // game pads on (default) or off; read every tick (USDL2Viewport::UpdateInput)
    INT                 WebGLVersion;       // the web's WebGL context: 1 (default) or 2; -WEBGL=<n> overrides it
    FLOAT               PadCursorSeconds;   // the pad's menu cursor: seconds across the viewport's width at full tilt

    // Variables.
    UBOOL               InMenuLoop;
    UViewport*          LastCurrent;

    // Constructors.
    USDL2Client();
    void StaticConstructor();

    // FNotifyHook interface.
    void NotifyDestroy( void* Src );

    // UObject interface.
    void Destroy();
    void PostEditChange();
    void ShutdownAfterError();

    // UClient interface.
    void Init( UEngine* InEngine );
    void ShowViewportWindows( DWORD ShowFlags, INT DoShow );
    void EnableViewportWindows( DWORD ShowFlags, INT DoEnable );
    UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
    void Tick();
    void MakeCurrent( UViewport* InViewport );
    UViewport* GetLastCurrent();
    class UViewport* NewViewport( const FName Name );
    void TeardownSR();
};

/*-----------------------------------------------------------------------------
    USDL2Viewport.
-----------------------------------------------------------------------------*/

//
// An SDL2 viewport.
//
class USDL2Viewport : public UViewport
{
    DECLARE_CLASS(USDL2Viewport,UViewport,CLASS_Transient,SDL2Drv)
    DECLARE_WITHIN(USDL2Client)

    // Variables.
    SDL_Window*         Window;
    SDL_GLContext       GLContext;
    DWORD               BlitFlags;
    DWORD               ViewportButtonFlags;
    FSDL2InputTest*     InputTest;          // the input test channel (-INPUTTEST=), NULL without it

    // SDL2 scancode to EInputKey translation map (static, shared across all viewports).
    static BYTE         KeyMap[SDL2DRV_MAX_SCANCODES];
    static const BYTE   MouseButtonMap[6];
    static void         InitKeyMap();

    // Constructor.
    USDL2Viewport();

    // UObject interface.
    void Destroy();
    void ShutdownAfterError();

    // UViewport interface.
    UBOOL Lock( BYTE* HitData=NULL, INT* HitSize=0 );
    UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar );
    UBOOL ResizeViewport( DWORD BlitFlags, INT NewX=INDEX_NONE, INT NewY=INDEX_NONE, UBOOL bSaveSize=true );
    UBOOL IsFullscreen();
    void Unlock();
    void Repaint( UBOOL Blit );
    void SetModeCursor();
    void UpdateWindowFrame();
    void OpenWindow( PTRINT ParentWindow, UBOOL Temporary, INT NewX, INT NewY, INT OpenX, INT OpenY );
    void CloseWindow();
    void UpdateInput( UBOOL Reset, FLOAT DeltaSeconds );
    void ClosePad();
    void MovePadCursor( FLOAT X, FLOAT Y, FLOAT DeltaSeconds );
    void UpdatePadCursor();
    void TakePadCursor();
    void* GetWindow();
    void SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL FocusOnly );

    // USDL2Viewport interface.
    void ToggleFullscreen();
    void EndFullscreen();

    UBOOL CauseInputEvent( INT iKey, EInputAction Action, FLOAT Delta=0.0 );
    virtual UBOOL TryRenderDevice( const TCHAR* ClassName, INT NewX, INT NewY, UBOOL Fullscreen );
    TCHAR* GetLocalizedKeyName( EInputKey Key );
};


#define AUTO_INITIALIZE_REGISTRANTS_SDL2DRV \
    USDL2Viewport::StaticClass(); \
    USDL2Client::StaticClass();

#endif //_INC_SDL2DRV
/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/
