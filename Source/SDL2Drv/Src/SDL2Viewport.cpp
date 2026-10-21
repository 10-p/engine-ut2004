/*=============================================================================
    SDL2Viewport.cpp: USDL2Viewport, the SDL2 viewport on every target.
    Adapted from SDLDrv's SDLViewport.cpp (the UE2 interface) and UE1's NSDLViewport.cpp (the SDL2 calls).
=============================================================================*/

#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
// The worker creates and presents its own WebGL context (ResizeViewport). Included before the engine headers:
// emscripten/html5.h pulls in <time.h>, whose clock() declaration UnFile.h's clock(Timer) macro would mangle.
#include <stdint.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgl.h>
#endif

#include "SDL2Drv.h"
#if defined(__EMSCRIPTEN__)
#include <emscripten/em_asm.h>   // WebPadPressed (a pad's first press)
#endif

#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
#include <math.h>
// The canvas backing store follows the canvas's device-pixel size. SDL2's Emscripten backend reports the CSS size
// (its pixel_ratio is 1 without SDL_WINDOW_ALLOW_HIGHDPI); passing the device size on to SetRes -> ResizeViewport ->
// SDL_SetWindowSize makes SDL size the backing store to it, report it from SDL_GetWindowSize and rescale menu mouse
// coordinates. Both calls are proxied to the main thread. Returns false when the box reads 0x0 (display:none), so
// the caller keeps SDL's own numbers.
static UBOOL ufrontCanvasDevicePixelSize( INT& OutW, INT& OutH )
{
    double CssW = 0.0, CssH = 0.0;
    if( emscripten_get_element_css_size( "#canvas", &CssW, &CssH ) != EMSCRIPTEN_RESULT_SUCCESS )
        return 0;
    const double Dpr = emscripten_get_device_pixel_ratio();
    // lround, not floor: a 1000.8 css box at 1.25 is 1251 device pixels.
    const INT W = (INT)lround( CssW * Dpr ), H = (INT)lround( CssH * Dpr );
    if( W <= 0 || H <= 0 )
        return 0;
    OutW = W; OutH = H;
    return 1;
}
#endif

#if defined(__ANDROID__)
// Android touch overlay glue. The Java on-screen overlay runs on the UI
// thread and cannot touch the engine directly; it writes intent through the JNI
// functions below and USDL2Viewport::UpdateInput (engine thread) drains it once
// per tick.
#include <jni.h>
#include <atomic>
#include <mutex>
#include <vector>
#endif

/*-----------------------------------------------------------------------------
    Class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(USDL2Viewport);

/*-----------------------------------------------------------------------------
    Static data.
-----------------------------------------------------------------------------*/

//
// SDL2 scancode to EInputKey translation map.
//
BYTE USDL2Viewport::KeyMap[SDL2DRV_MAX_SCANCODES];

//
// SDL_BUTTON_ -> EInputKey translation map.
// UE2 supports IK_Mouse4/IK_Mouse5 (unlike UE1 which maps to IK_None).
//
const BYTE USDL2Viewport::MouseButtonMap[6] =
{
    /* invalid           */ IK_None,
    /* SDL_BUTTON_LEFT   */ IK_LeftMouse,
    /* SDL_BUTTON_MIDDLE */ IK_MiddleMouse,
    /* SDL_BUTTON_RIGHT  */ IK_RightMouse,
    /* SDL_BUTTON_X1     */ IK_Mouse4,
    /* SDL_BUTTON_X2     */ IK_Mouse5
};

#if defined(__ANDROID__)
/*-----------------------------------------------------------------------------
    Android touch input glue.

    The on-screen touch overlay (a Java View above the SDL surface, see
    Android/app/src/main/java/com/ut2004/android/GameActivity.java) drives gameplay input in landscape: a left-half
    virtual move stick, a right-half drag-look, and FIRE/ALTFIRE/JUMP/CROUCH/
    NEXTWEAPON/MENU buttons. The overlay disables itself while a menu is up (keyed
    on the engine's own bShowWindowsMouse, published below) so menu touches fall
    through to SDL's touch->mouse synthesis (tap-to-click).

    The overlay runs on the Android UI thread, so it cannot call CauseInputEvent
    directly. Instead it writes intent into the shared state here via JNI; the
    engine-thread consumer in UpdateInput drains it once per tick into the exact
    same CauseInputEvent calls a mouse/gamepad would produce. All targets map to
    keys UT2004's DefUser.ini already binds, so no ini editing is needed:
      move stick    -> IK_JoyX / IK_JoyY   (Axis aStrafe / Axis aBaseY Invert=-1)
      drag-look     -> IK_MouseX / IK_MouseY (aMouseX/aMouseY, like the real mouse)
      FIRE/ALTFIRE  -> IK_LeftMouse / IK_RightMouse
      JUMP/CROUCH   -> IK_Space / IK_C
      NEXTWEAPON    -> IK_MouseWheelDown (momentary press+release)
      MENU          -> IK_Escape
-----------------------------------------------------------------------------*/

// Semantic button ids -- keep in sync with GameActivity.Ut2004TouchOverlayView.
enum EAndroidTouchButton
{
    ATB_Fire       = 0,
    ATB_AltFire    = 1,
    ATB_Jump       = 2,
    ATB_Crouch     = 3,
    ATB_NextWeapon = 4,
    ATB_Menu       = 5,
};

// Published each tick from bShowWindowsMouse so the Java overlay knows menu vs
// gameplay. Engine thread writes, UI thread reads.
static volatile bool                   GAndroidMenuVisible = false;

// Left virtual stick: held-absolute value in [-1,1]. Single writer (UI), single
// reader (engine); last-writer-wins is exactly the held-stick semantics we want.
static std::atomic<float>              GAndroidMoveX( 0.f );
static std::atomic<float>              GAndroidMoveY( 0.f );

// Right drag-look: raw pixel delta accumulated by the UI thread, consumed+reset
// each tick by the engine thread (same summing the mouse does across a frame).
static std::atomic<float>              GAndroidLookX( 0.f );
static std::atomic<float>              GAndroidLookY( 0.f );
static float                           GAndroidLastMoveX = 0.f;
static float                           GAndroidLastMoveY = 0.f;

// Look sensitivity: raw finger pixels are fed as mouse-axis deltas, then scaled by
// the player's own mouse sensitivity binding. 1.0 == "a screen pixel is a mouse count".
static const float                     GAndroidLookScale = 1.0f;

// Pending button edges. A queue (not a level) so a tap that presses AND releases
// between two engine ticks still fires. Low volume; a plain mutex is ample.
struct FAndroidButtonEdge { INT Key; BYTE Action; };
static std::mutex                      GAndroidBtnMutex;
static std::vector<FAndroidButtonEdge> GAndroidBtnQueue;

static void AndroidQueueButton( INT Key, EInputAction Action )
{
    std::lock_guard<std::mutex> Lock( GAndroidBtnMutex );
    FAndroidButtonEdge E; E.Key = Key; E.Action = (BYTE)Action;
    GAndroidBtnQueue.push_back( E );
}

// Atomic float add (no std::atomic<float> arithmetic before C++20) for the
// accumulate-from-UI-thread look channel.
static inline void AndroidAtomicAddF( std::atomic<float>& A, float V )
{
    float Old = A.load( std::memory_order_relaxed );
    while( !A.compare_exchange_weak( Old, Old + V, std::memory_order_relaxed ) ) {}
}

extern "C" {

JNIEXPORT jboolean JNICALL
Java_com_ut2004_android_GameActivity_nativeIsMenuVisible( JNIEnv*, jclass )
{
    return GAndroidMenuVisible ? JNI_TRUE : JNI_FALSE;
}

JNIEXPORT void JNICALL
Java_com_ut2004_android_GameActivity_nativeTouchAxis( JNIEnv*, jclass, jint Axis, jfloat Value )
{
    // Axis 0 = strafe (X), 1 = forward (Y); Value in [-1,1].
    if( Axis == 0 )
        GAndroidMoveX.store( Value, std::memory_order_relaxed );
    else if( Axis == 1 )
        GAndroidMoveY.store( Value, std::memory_order_relaxed );
}

JNIEXPORT void JNICALL
Java_com_ut2004_android_GameActivity_nativeTouchLook( JNIEnv*, jclass, jfloat DX, jfloat DY )
{
    AndroidAtomicAddF( GAndroidLookX, DX );
    AndroidAtomicAddF( GAndroidLookY, DY );
}

JNIEXPORT void JNICALL
Java_com_ut2004_android_GameActivity_nativeTouchButton( JNIEnv*, jclass, jint Button, jboolean Down )
{
    INT Key = IK_None;
    switch( Button )
    {
    case ATB_Fire:       Key = IK_LeftMouse;      break;
    case ATB_AltFire:    Key = IK_RightMouse;     break;
    case ATB_Jump:       Key = IK_Space;          break;
    case ATB_Crouch:     Key = IK_C;              break;
    case ATB_NextWeapon: Key = IK_MouseWheelDown; break;
    case ATB_Menu:       Key = IK_Escape;         break;
    default: return;
    }

    // Mouse wheel is momentary: emit a single press+release on the down edge.
    if( Key == IK_MouseWheelDown )
    {
        if( Down )
        {
            AndroidQueueButton( Key, IST_Press );
            AndroidQueueButton( Key, IST_Release );
        }
        return;
    }

    AndroidQueueButton( Key, Down ? IST_Press : IST_Release );
}

} // extern "C"

#endif // __ANDROID__

/*-----------------------------------------------------------------------------
    Game pads, the same on every target: one pad (the first connected, hot-plug),
    the shared button map, the sticks and triggers read from the pad's state every
    tick and sent as rates. [SDL2Drv.SDL2Client] UseJoystick (on by default) turns
    pads off and on. On Android the touch overlay's move stick is a separate channel.
-----------------------------------------------------------------------------*/

static const FLOAT GPadDeadzone     = 0.12f;  // stick deadzone (engine ini also deadzones)
static const FLOAT GPadTriggerPress = 0.35f;  // trigger press threshold

// SDL_CONTROLLER_BUTTON_* -> EInputKey. The actions are DefUser.ini's [Engine.Input] defaults (remappable); the layout
// is the one every UE1/UE2 target shares: the triggers and bumpers fire, the D-pad changes weapon and inventory
// item, the left stick moves.
static const BYTE GPadButtonMap[SDL_CONTROLLER_BUTTON_MAX] =
{
    /* A             */ IK_Joy2,    // Jump
    /* B             */ IK_Joy4,    // Duck
    /* X             */ IK_Joy5,    // InventoryActivate (use the selected item)
    /* Y             */ IK_Joy6,    // Walking (held)
    /* BACK          */ IK_Joy7,    // ScoreToggle (the scoreboard while held)
    /* GUIDE         */ IK_None,
    /* START         */ IK_Escape,  // menu
    /* LEFTSTICK     */ IK_Joy8,    // Duck
    /* RIGHTSTICK    */ IK_Joy9,    // (unbound)
    /* LEFTSHOULDER  */ IK_Joy11,   // AltFire
    /* RIGHTSHOULDER */ IK_Joy12,   // Fire
    /* DPAD_UP       */ IK_Joy13,   // InventoryPrevious
    /* DPAD_DOWN     */ IK_Joy15,   // InventoryNext
    /* DPAD_LEFT     */ IK_Joy16,   // PrevWeapon
    /* DPAD_RIGHT    */ IK_Joy14,   // NextWeapon
};

// The sticks are a rate, not a per-frame step. Each tick a held stick goes to its [Engine.Input] binding as an IST_Axis
// event, and UInput's "Axis ... SpeedBase=" branch adds `Delta * SpeedBase * Scale` per event, where Scale is a
// constant here ((1-DeadZone)/(0.5-DeadZone): the binding's Speed is never the stick's, only DirectAxis appends one);
// ReadInput then turns the per-tick sum into a rate (x 20/dt). A raw stick value would therefore turn with the frame
// rate; sent as `stick * Factor * DeltaSeconds`, the rate is the stick's and nothing else's:
//   look (JoyV / JoySlider1: SpeedBase 2, DeadZone 0.4, Scale 6): aBaseX = 240 * Factor * stick; the yaw rate is
//     32 * aTurn units/s, so 5.7 -> 43,776 units/s = 240 deg/s at full tilt; pitch alike.
//   move (JoyX / JoyY: SpeedBase 300, DeadZone 0.1, Scale 2.25): aBaseY = 13,500 * Factor * stick, so 0.444 -> 6000 at
//     full tilt, the keyboard's MoveForward (Speed=+300 held: 300 x 20).
static const FLOAT GPadLookFactor = 5.7f;
static const FLOAT GPadMoveFactor = 0.444f;

// Rescale a normalized stick axis so motion starts at 0 past the deadzone.
static inline FLOAT PadAxis( FLOAT V )
{
    if( Abs(V) < GPadDeadzone )
        return 0.f;
    FLOAT S = ( V > 0.f ? (V - GPadDeadzone) : (V + GPadDeadzone) ) / (1.f - GPadDeadzone);
    return Clamp( S, -1.f, 1.f );
}

static SDL_GameController* GPad          = NULL;   // the pad in use (the first connected)
static FLOAT               GPadMoveX     = 0.f;    // left stick  -> IK_JoyX / IK_JoyY (held-absolute)
static FLOAT               GPadMoveY     = 0.f;
static FLOAT               GPadLastMoveX = 0.f;
static FLOAT               GPadLastMoveY = 0.f;
static FLOAT               GPadYaw       = 0.f;    // right stick -> IK_JoyV / IK_JoySlider1
static FLOAT               GPadPitch     = 0.f;
static FLOAT               GPadLastYaw   = 0.f;
static FLOAT               GPadLastPitch = 0.f;
static UBOOL               GPadRT        = 0;      // right trigger -> IK_Joy1 (Fire)
static UBOOL               GPadLT        = 0;      // left trigger  -> IK_Joy3 (AltFire)
// The keys a pad's held-at-connect press gave the game. SDL never reports their release (it never saw them go down),
// so they are a tap: released at the next tick. (The triggers become a tap by themselves: the next read of their
// state says up.)
static BYTE                GPadHeldTap[32];
static INT                 GPadHeldTapNum = 0;

static SDL_JoystickID PadInstance( SDL_GameController* Pad )
{
    return Pad ? SDL_JoystickInstanceID( SDL_GameControllerGetJoystick( Pad ) ) : -1;
}

static UBOOL GPadsOn = 1;   // UseJoystick as last seen (a change is logged once)

#if defined(__EMSCRIPTEN__)
//
// The buttons pressed right now on the browser's first listed pad named Id (bit i = Gamepad.buttons[i].pressed, the
// W3C "standard" layout), 0 if none. A browser lists a pad only at its first press, and that press is already down in
// the snapshot SDL's Emscripten driver keeps as the pad's starting state (it reports changes only), so SDL never
// reports it: read it once at the connect. Runs on the page's main thread (the engine runs on a worker). (In the
// EM_ASM block: no top-level commas and no // comments.)
//
static DWORD WebPadPressed( const char* Id )
{
    return (DWORD)MAIN_THREAD_EM_ASM_INT({
        if (typeof navigator === 'undefined' || !navigator.getGamepads) return 0;
        var id = UTF8ToString($0);
        var pads = navigator.getGamepads();
        for (var i = 0; i < pads.length; i++) {
            var g = pads[i];
            if (!g || g.id !== id || !g.buttons) continue;
            var m = 0;
            for (var b = 0; b < g.buttons.length && b < 31; b++)
                if (g.buttons[b] && g.buttons[b].pressed) m = m | (1 << b);
            return m;
        }
        return 0;
    }, Id );
}
#endif

/*-----------------------------------------------------------------------------
    Key map initialization.
    Uses SDL2 scancodes (physical key positions) for layout-independent mapping.
    Adapted from NSDLViewport.cpp InitKeyMap, using UE2 EInputKey values.
-----------------------------------------------------------------------------*/

void USDL2Viewport::InitKeyMap()
{
    #define INIT_KEY_RANGE( AStart, AEnd, BStart, BEnd ) \
        for( DWORD Key = AStart; Key <= AEnd; ++Key ) KeyMap[Key] = BStart + ( Key - AStart )

    appMemset( KeyMap, 0, sizeof( KeyMap ) );

    // Modifiers -- UE2 has distinct IK_LShift/IK_RShift/IK_LControl/IK_RControl.
    KeyMap[SDL_SCANCODE_LSHIFT] = IK_LShift;
    KeyMap[SDL_SCANCODE_RSHIFT] = IK_RShift;
    KeyMap[SDL_SCANCODE_LCTRL]  = IK_LControl;
    KeyMap[SDL_SCANCODE_RCTRL]  = IK_RControl;
    KeyMap[SDL_SCANCODE_LALT]   = IK_Alt;
    KeyMap[SDL_SCANCODE_RALT]   = IK_Alt;

    // TTY / Navigation keys.
    KeyMap[SDL_SCANCODE_ESCAPE]    = IK_Escape;
    KeyMap[SDL_SCANCODE_RETURN]    = IK_Enter;
    KeyMap[SDL_SCANCODE_TAB]       = IK_Tab;
    KeyMap[SDL_SCANCODE_BACKSPACE] = IK_Backspace;
    KeyMap[SDL_SCANCODE_SPACE]     = IK_Space;
    KeyMap[SDL_SCANCODE_GRAVE]     = IK_Tilde;  // Console toggle key (backtick/tilde).

    // Arrow keys.
    KeyMap[SDL_SCANCODE_LEFT]  = IK_Left;
    KeyMap[SDL_SCANCODE_RIGHT] = IK_Right;
    KeyMap[SDL_SCANCODE_UP]    = IK_Up;
    KeyMap[SDL_SCANCODE_DOWN]  = IK_Down;

    // Navigation cluster.
    KeyMap[SDL_SCANCODE_HOME]     = IK_Home;
    KeyMap[SDL_SCANCODE_END]      = IK_End;
    KeyMap[SDL_SCANCODE_PAGEUP]   = IK_PageUp;
    KeyMap[SDL_SCANCODE_PAGEDOWN] = IK_PageDown;
    KeyMap[SDL_SCANCODE_INSERT]   = IK_Insert;
    KeyMap[SDL_SCANCODE_DELETE]   = IK_Delete;

    // Letters A-Z (range macro).
    INIT_KEY_RANGE( SDL_SCANCODE_A, SDL_SCANCODE_Z, IK_A, IK_Z );

    // Numbers 1-9 (range macro) and 0 (special case).
    INIT_KEY_RANGE( SDL_SCANCODE_1, SDL_SCANCODE_9, IK_1, IK_9 );
    KeyMap[SDL_SCANCODE_0] = IK_0;

    // Function keys F1-F12 (range macro).
    INIT_KEY_RANGE( SDL_SCANCODE_F1, SDL_SCANCODE_F12, IK_F1, IK_F12 );

    // Punctuation.
    KeyMap[SDL_SCANCODE_SEMICOLON]    = IK_Semicolon;
    KeyMap[SDL_SCANCODE_EQUALS]       = IK_Equals;
    KeyMap[SDL_SCANCODE_COMMA]        = IK_Comma;
    KeyMap[SDL_SCANCODE_MINUS]        = IK_Minus;
    KeyMap[SDL_SCANCODE_PERIOD]       = IK_Period;
    KeyMap[SDL_SCANCODE_SLASH]        = IK_Slash;
    KeyMap[SDL_SCANCODE_BACKSLASH]    = IK_Backslash;
    KeyMap[SDL_SCANCODE_LEFTBRACKET]  = IK_LeftBracket;
    KeyMap[SDL_SCANCODE_RIGHTBRACKET] = IK_RightBracket;
    KeyMap[SDL_SCANCODE_APOSTROPHE]   = IK_SingleQuote;

    // Numpad 1-9 (range macro), 0 and extras.
    INIT_KEY_RANGE( SDL_SCANCODE_KP_1, SDL_SCANCODE_KP_9, IK_NumPad1, IK_NumPad9 );
    KeyMap[SDL_SCANCODE_KP_0]        = IK_NumPad0;
    KeyMap[SDL_SCANCODE_KP_PERIOD]   = IK_NumPadPeriod;
    KeyMap[SDL_SCANCODE_KP_ENTER]    = IK_Enter;
    KeyMap[SDL_SCANCODE_KP_MULTIPLY] = IK_GreyStar;
    KeyMap[SDL_SCANCODE_KP_PLUS]     = IK_GreyPlus;
    KeyMap[SDL_SCANCODE_KP_MINUS]    = IK_GreyMinus;
    KeyMap[SDL_SCANCODE_KP_DIVIDE]   = IK_GreySlash;

    // Lock keys and misc.
    KeyMap[SDL_SCANCODE_CAPSLOCK]     = IK_CapsLock;
    KeyMap[SDL_SCANCODE_NUMLOCKCLEAR] = IK_NumLock;
    KeyMap[SDL_SCANCODE_SCROLLLOCK]   = IK_ScrollLock;
    KeyMap[SDL_SCANCODE_PAUSE]        = IK_Pause;
    KeyMap[SDL_SCANCODE_PRINTSCREEN]  = IK_PrintScrn;

    #undef INIT_KEY_RANGE
}

/*-----------------------------------------------------------------------------
    USDL2Viewport Init/Exit.
-----------------------------------------------------------------------------*/

//
// Constructor.
//
USDL2Viewport::USDL2Viewport()
:   UViewport()
{
    guard(USDL2Viewport::USDL2Viewport);
    Window              = NULL;
    GLContext           = NULL;
    BlitFlags           = 0;
    ViewportButtonFlags = 0;
    unguard;
}

//
// Destroy.
//
void USDL2Viewport::Destroy()
{
    guard(USDL2Viewport::Destroy);
    CloseWindow();
    Super::Destroy();
    unguard;
}

//
// Error shutdown.
//
void USDL2Viewport::ShutdownAfterError()
{
    guard(USDL2Viewport::ShutdownAfterError);
    if( Window )
    {
        EndFullscreen();
        SDL_SetRelativeMouseMode(SDL_FALSE);
    }
    Super::ShutdownAfterError();
    unguard;
}

/*-----------------------------------------------------------------------------
    Locking.
-----------------------------------------------------------------------------*/

UBOOL USDL2Viewport::Lock( BYTE* HitData, INT* HitSize )
{
    guard(USDL2Viewport::Lock);
    return UViewport::Lock(HitData, HitSize);
    unguard;
}

void USDL2Viewport::Unlock()
{
    guard(USDL2Viewport::Unlock);
    UViewport::Unlock();
    unguard;
}

/*-----------------------------------------------------------------------------
    Command line.
-----------------------------------------------------------------------------*/

UBOOL USDL2Viewport::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
    guard(USDL2Viewport::Exec);
    if( UViewport::Exec( Cmd, Ar ) )
    {
        return 1;
    }
    else if( ParseCommand(&Cmd,TEXT("ENDFULLSCREEN")) )
    {
        if( BlitFlags & BLIT_Fullscreen )
            EndFullscreen();
        return 1;
    }
    else if( ParseCommand(&Cmd,TEXT("TOGGLEFULLSCREEN")) )
    {
        ToggleFullscreen();
        return 1;
    }
    else if( ParseCommand(&Cmd,TEXT("GETCURRENTRES")) )
    {
        Ar.Logf( TEXT("%ix%i"), SizeX, SizeY );
        return 1;
    }
    else if( ParseCommand(&Cmd,TEXT("GETCURRENTCOLORDEPTH")) )
    {
        Ar.Logf( TEXT("%i"), (ColorBytes?ColorBytes:2)*8 );
        return 1;
    }
    else if( ParseCommand(&Cmd,TEXT("GETCOLORDEPTHS")) )
    {
        Ar.Log( TEXT("32") );
        return 1;
    }
    else if( ParseCommand(&Cmd,TEXT("GETCURRENTRENDERDEVICE")) )
    {
        Ar.Log( RenDev ? RenDev->GetClass()->GetPathName() : TEXT("") );
        return 1;
    }
    else if( ParseCommand(&Cmd,TEXT("SUPPORTEDRESOLUTION")) )
    {
        INT     Width = 0,
                Height = 0,
                BitDepth = 0;
        UBOOL   Supported = 0;

        if(Parse(Cmd,TEXT("WIDTH="),Width) && Parse(Cmd,TEXT("HEIGHT="),Height) && Parse(Cmd,TEXT("BITDEPTH="),BitDepth))
        {
            // SDL2: window manager handles mode validation.
            Supported = 1;
        }

        Ar.Logf(TEXT("%u"),Supported);

        return 1;
    }

    else if( ParseCommand(&Cmd,TEXT("SETRES")) )
    {
        INT X=appAtoi(Cmd);
        const TCHAR* CmdTemp = appStrchr(Cmd,'x') ? appStrchr(Cmd,'x')+1 : appStrchr(Cmd,'X') ? appStrchr(Cmd,'X')+1 : TEXT("");
        INT Y=appAtoi(CmdTemp);
        Cmd = CmdTemp;
        CmdTemp = appStrchr(Cmd,'x') ? appStrchr(Cmd,'x')+1 : appStrchr(Cmd,'X') ? appStrchr(Cmd,'X')+1 : TEXT("");
        INT BPP=appAtoi(CmdTemp);
        Cmd = CmdTemp;
        UBOOL   Fullscreen = IsFullscreen() || FullscreenOnly;
        if(appStrchr(Cmd,'w') || appStrchr(Cmd,'W'))
            Fullscreen = 0;
        else if(appStrchr(Cmd,'f') || appStrchr(Cmd,'F'))
            Fullscreen = 1;
        if( X && Y )
        {
            INT ColorBytes = 0;
            switch( BPP )
            {
            case 16:
                ColorBytes = 2;
                break;
            case 24:
            case 32:
                ColorBytes = 4;
                break;
            }
            UBOOL Result = RenDev->SetRes( this, X, Y, Fullscreen || FullscreenOnly, ColorBytes );
            if( !Result )
                EndFullscreen();

            if ( GUIController )
                GUIController->ResolutionChanged(X, Y);
        }
        return 1;
    }

    else if( ParseCommand(&Cmd,TEXT("SETMOUSE")) )
    {
        const TCHAR* CmdTemp = Cmd;
        INT X, Y;

        X = appAtoi(CmdTemp);

        while( appIsDigit( *CmdTemp) )
            CmdTemp++;

        while( appIsSpace( *CmdTemp) )
            CmdTemp++;

        Y = appAtoi(CmdTemp);

        WindowsMouseX = X;
        WindowsMouseY = Y;
        if( Window )
            SDL_WarpMouseInWindow(Window, X, Y);
        return 1;
    }

    else return 0;
    unguard;
}

/*-----------------------------------------------------------------------------
    Window management.
-----------------------------------------------------------------------------*/

//
// Open this viewport's window.
//
void USDL2Viewport::OpenWindow( PTRINT InParentWindow, UBOOL IsTemporary, INT NewX, INT NewY, INT OpenX, INT OpenY )
{
    guard(USDL2Viewport::OpenWindow);

    check(Actor);
    UBOOL DoRepaint=0, DoSetActive=0;
    USDL2Client* C = GetOuterUSDL2Client();

#if !defined(__EMSCRIPTEN__)
    // Not on the web: Align rounds up, and a backing store 1 px wider than the canvas box is a fractional
    // rescale in the browser (blur). PixoDrv, the only even-width consumer, is not in the web build.
    if( NewX!=INDEX_NONE )
        NewX = Align( NewX, 2 );
#endif

    // Create frame buffer.
    if( IsTemporary )
    {
        BlitFlags     = BLIT_Temporary;
        ColorBytes    = 2;
        SizeX         = NewX;
        SizeY         = NewY;
        ScreenPointer = (BYTE*)appMalloc( 2 * NewX * NewY, TEXT("TemporaryViewportData") );
    }

    // Create rendering device.
    if( !RenDev && !GIsEditor && ParseParam(appCmdLine(),TEXT("windowed")) )
        TryRenderDevice( TEXT("ini:Engine.Engine.RenderDevice"), NewX, NewY, 0 );
    if( !RenDev )
        TryRenderDevice( TEXT("ini:Engine.Engine.RenderDevice"), NewX, NewY, GIsEditor ? 0 : C->StartupFullscreen );

    check(RenDev);
    RenDev->UpdateGamma( this );
    UpdateWindowFrame();

    unguard;
}

//
// Close a viewport window.
//
void USDL2Viewport::CloseWindow()
{
    guard(USDL2Viewport::CloseWindow);
    if( GLContext )
    {
        SDL_GL_DeleteContext( GLContext );
        GLContext = NULL;
    }
    if( Window )
    {
        SDL_DestroyWindow( Window );
        Window = NULL;
    }
    unguard;
}

/*-----------------------------------------------------------------------------
    USDL2Viewport operations.
-----------------------------------------------------------------------------*/

//
// Repaint the viewport.
//
void USDL2Viewport::Repaint( UBOOL Blit )
{
    guard(USDL2Viewport::Repaint);
    GetOuterUSDL2Client()->Engine->Draw( this, Blit );
    unguard;
}

//
// Return whether fullscreen.
//
UBOOL USDL2Viewport::IsFullscreen()
{
    guard(USDL2Viewport::IsFullscreen);
    return (BlitFlags & BLIT_Fullscreen) ? 1 : 0;
    unguard;
}

//
// Set the mouse cursor.
//
void USDL2Viewport::SetModeCursor()
{
    guard(USDL2Viewport::SetModeCursor);
    /* no-op stub */
    unguard;
}

//
// Update window title bar.
//
void USDL2Viewport::UpdateWindowFrame()
{
    guard(USDL2Viewport::UpdateWindowFrame);

    if( !Window || (BlitFlags&BLIT_Fullscreen) || (BlitFlags&BLIT_Temporary) || !Actor)
        return;

    SDL_SetWindowTitle( Window, "Unreal Tournament 2004" );

    unguard;
}

//
// Return the viewport's window.
//
void* USDL2Viewport::GetWindow()
{
    guard(USDL2Viewport::GetWindow);
    return (void*)Window;
    unguard;
}

/*-----------------------------------------------------------------------------
    Viewport modes.
-----------------------------------------------------------------------------*/

//
// Try switching to a new rendering device.
//
UBOOL USDL2Viewport::TryRenderDevice( const TCHAR* ClassName, INT NewX, INT NewY, UBOOL Fullscreen )
{
    guard(USDL2Viewport::TryRenderDevice);

    // Shut down current rendering device.
    guard(ShutdownOldRenDev);
    if( RenDev && RenDev != GetOuterUSDL2Client()->Engine->GRenDev )
    {
        RenDev->Exit(this);
        delete RenDev;
        RenDev = NULL;
    }
    unguard;

    // Use appropriate defaults.
    USDL2Client* C = GetOuterUSDL2Client();
    if( NewX==INDEX_NONE )
        NewX = (Fullscreen || FullscreenOnly) ? C->FullscreenViewportX : C->WindowedViewportX;
    if( NewY==INDEX_NONE )
        NewY = (Fullscreen || FullscreenOnly) ? C->FullscreenViewportY : C->WindowedViewportY;

    // Find device driver.
    UClass* RenderClass = UObject::StaticLoadClass( URenderDevice::StaticClass(), NULL, ClassName, NULL, 0, NULL );
    if( RenderClass )
    {
        if( RenderClass == GetOuterUSDL2Client()->Engine->GRenDev->GetClass() )
        {
            RenDev = GetOuterUSDL2Client()->Engine->GRenDev;
        }
        else
        {
            RenDev = ConstructObject<URenderDevice>( RenderClass, this );
            RenDev->Init();
        }

        if( RenDev->SetRes( this, NewX, NewY, Fullscreen | FullscreenOnly, ColorBytes ) )
        {
            if( GIsRunning )
                Actor->GetLevel()->DetailChange(
                    RenDev->SuperHighDetailActors ? DM_SuperHigh :
                    RenDev->HighDetailActors ? DM_High :
                    DM_Low
                    );
        }
        else
        {
            debugf( NAME_Log, LocalizeError(TEXT("Failed3D"),TEXT("SDL2Drv")) );
            if( RenDev != GetOuterUSDL2Client()->Engine->GRenDev )
                delete RenDev;
            RenDev = NULL;
        }
    }

    if( !GIsEditor )
    {
        if ( GUIController )
        {
            GUIController->ResetInput();
            GUIController->ResolutionChanged(SizeX,SizeY);
        }
        if ( Input )
            Input->ResetInput();
    }

    return RenDev != NULL;
    unguard;
}

//
// If in fullscreen mode, end it and return to windowed.
//
void USDL2Viewport::EndFullscreen()
{
    guard(USDL2Viewport::EndFullscreen);
    USDL2Client* Client = GetOuterUSDL2Client();
    debugf(TEXT("EndFullscreen"));
    if( RenDev && (BlitFlags & (BLIT_Direct3D|BLIT_OpenGL)) && !FullscreenOnly )
    {
        RenDev->SetRes( this, Client->WindowedViewportX, Client->WindowedViewportY, 0 );
    }
    else if( RenDev && (BlitFlags & BLIT_OpenGL) )
    {
        RenDev->SetRes( this, INDEX_NONE, INDEX_NONE, FullscreenOnly );
    }
    UpdateWindowFrame();

    if ( GUIController )
    {
        GUIController->ResetInput();
        GUIController->ResolutionChanged(SizeX, SizeY);
    }

    if ( Input )
        Input->ResetInput();

    unguard;
}

//
// Toggle fullscreen.
//
void USDL2Viewport::ToggleFullscreen()
{
    guard(USDL2Viewport::ToggleFullscreen);
    if( BlitFlags & BLIT_Fullscreen )
    {
        EndFullscreen();
        SetMouseCapture(1,1,0);
    }
    else if( !(Actor->ShowFlags & SHOW_ChildWindow) )
    {
        debugf(TEXT("AttemptFullscreen"));
        TryRenderDevice( TEXT("ini:Engine.Engine.RenderDevice"), INDEX_NONE, INDEX_NONE, 1 );
    }
    unguard;
}

/*-----------------------------------------------------------------------------
    Resize viewport.
    Called by the render device's SetRes via TryRenderDevice.
    Creates the SDL2 window and the GL context.
-----------------------------------------------------------------------------*/

UBOOL USDL2Viewport::ResizeViewport( DWORD NewBlitFlags, INT InNewX, INT InNewY, UBOOL bSaveSize )
{
    guard(USDL2Viewport::ResizeViewport);
    USDL2Client* Client = GetOuterUSDL2Client();

    // Remember viewport for audio.
    UViewport* SavedViewport = NULL;
    if( Client->Engine->Audio && !GIsEditor && !(GetFlags() & RF_Destroyed) )
        SavedViewport = Client->Engine->Audio->GetViewport();

    // Accept default parameters.
    INT NewX = InNewX!=INDEX_NONE ? InNewX : (NewBlitFlags&BLIT_Fullscreen) ? Client->FullscreenViewportX : Client->WindowedViewportX;
    INT NewY = InNewX!=INDEX_NONE ? InNewY : (NewBlitFlags&BLIT_Fullscreen) ? Client->FullscreenViewportY : Client->WindowedViewportY;

    // Align NewX.
    check(NewX>=0);
    check(NewY>=0);
#if !defined(__EMSCRIPTEN__)
    NewX = Align(NewX,2); // not on the web (see OpenWindow)
#endif

    if( !(NewBlitFlags & BLIT_Temporary) )
    {
        ScreenPointer = NULL;
    }

    // Set new info.
    DWORD OldBlitFlags = BlitFlags;
    BlitFlags          = NewBlitFlags & ~BLIT_ParameterFlags;
    SizeX              = NewX;
    SizeY              = NewY;

    // Enable the engine's absolute Windows-mouse path (as WinViewport does:
    // bWindowsMouseAvailable = !(NewBlitFlags & BLIT_Fullscreen)). The browser canvas is
    // always "windowed", so this is on there. The GUI then drives bShowWindowsMouse on menu
    // open/close (GUIController::SetControllerStatus), which releases pointer-lock and
    // makes the GUI hit-test WindowsMouseX/Y as an absolute position (Scale==1.0).
    bWindowsMouseAvailable = !(NewBlitFlags & BLIT_Fullscreen);

    // SDL2 window creation.
    if( !Window )
    {
#if defined(__ANDROID__)
        // Lock the game to landscape. AndroidManifest pins screenOrientation=landscape, but SDLActivity
        // overrides it at runtime from SDL_HINT_ORIENTATIONS and the window shape (an empty hint and a
        // portrait-shaped window give a portrait surface while the engine renders landscape, and
        // BLASTBufferQueue rejects the buffer). A landscape-only hint makes the surface match the engine.
        SDL_SetHint( SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight" );
#endif
        Window = SDL_CreateWindow(
            "Unreal Tournament 2004",
            SDL_WINDOWPOS_CENTERED,
            SDL_WINDOWPOS_CENTERED,
            NewX,
            NewY,
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
            // No SDL GL window: SDL_WINDOW_OPENGL brings up SDL's EGL path, Emscripten proxies EGL to the
            // browser main thread, and the canvas has been transferred to this worker, so getContext() there
            // throws InvalidStateError (Emscripten #24792). The WebGL context is created below; SDL is here
            // for the window and input only.
            SDL_WINDOW_RESIZABLE
#else
            SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
#endif
        );
#if !(defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__))
        // The window fails when no EGL/GLX/WGL config has the multisampling the render device asked for (an
        // emulator, some GPUs). Antialiasing is a request, so the window is made once more without it.
        if( !Window )
        {
            debugf( NAME_Warning, TEXT("SDL_CreateWindow failed (%s); creating it without multisampling"),
                appFromAnsi(SDL_GetError()) );
            SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, 0 );
            SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, 0 );
            Window = SDL_CreateWindow( "Unreal Tournament 2004", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                NewX, NewY, SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE );
        }
#endif
        if( !Window )
        {
            appErrorf( TEXT("SDL_CreateWindow failed: %s"), appFromAnsi(SDL_GetError()) );
            return 0;
        }
        // On Emscripten, if resolution was 0x0, query the actual canvas size.
        // SDL2's Emscripten backend maps the window to the HTML canvas element.
        if( NewX == 0 || NewY == 0 )
        {
            SDL_GetWindowSize( Window, &NewX, &NewY );
            SizeX = NewX;
            SizeY = NewY;
        }
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
        // Adopt the canvas's device-pixel size here: SDL sends its create-time RESIZED(css) only when the CSS
        // box differs from the requested size, so an ini size equal to the box would never reach the RESIZED
        // handler and the backing store would stay in CSS pixels. SDL_SetWindowSize sizes it exactly.
        {
            INT DevX = 0, DevY = 0;
            if( ufrontCanvasDevicePixelSize( DevX, DevY ) && ( DevX != NewX || DevY != NewY ) )
            {
                NewX = DevX;
                NewY = DevY;
                SDL_SetWindowSize( Window, NewX, NewY );
                SizeX = NewX;
                SizeY = NewY;
            }
        }
#endif
    }
    else
    {
        SDL_SetWindowSize( Window, NewX, NewY );
    }

    // Handle fullscreen -- desktop fullscreen, no mode changes.
#if defined(__ANDROID__)
    // Android: always immersive fullscreen. A phone has no windowed mode, and requesting
    // FULLSCREEN_DESKTOP makes SDL call SDLActivity.setWindowStyle(true), which applies
    // SYSTEM_UI_FLAG_FULLSCREEN | HIDE_NAVIGATION | IMMERSIVE_STICKY -- hiding the status bar and the
    // navigation/home slider so the game uses the whole display (the surface grows to the full display;
    // WINDOWEVENT_RESIZED then re-runs the drawable-size SetRes below).
    SDL_SetWindowFullscreen( Window, SDL_WINDOW_FULLSCREEN_DESKTOP );
#else
    if( NewBlitFlags & BLIT_Fullscreen )
    {
        SDL_SetWindowFullscreen( Window, SDL_WINDOW_FULLSCREEN_DESKTOP );
    }
    else
    {
        SDL_SetWindowFullscreen( Window, 0 );
    }
#endif

    // GL context creation.
    // GL4ESDrv: GLES 2.0 context for GL4ES output.
    // OpenGLDrv: Compatibility profile for fixed-function pipeline.
    if( !GLContext )
    {
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
        // Create the WebGL context directly, not through SDL2: SDL2's Emscripten GL path is EGL, which
        // Emscripten proxies to the browser main thread, and the canvas belongs to this worker
        // (-sPROXY_TO_PTHREAD), so SDL_GL_CreateContext would fail with InvalidStateError (Emscripten
        // #24792). The worker renders on its own context, with no per-call proxying.
        //
        // SDL2 still owns the window and all input. The SDL_GL_SetAttribute hints in the #else below (and in
        // GL4ESRenderDevice::SetRes) do not apply here, so their equivalents are Emscripten attributes. One
        // context serves every web device: GL4ESDrv renders into it, Engine.NullRenderDevice never draws.
        EmscriptenWebGLContextAttributes Attrs;
        emscripten_webgl_init_context_attributes( &Attrs );
        // WebGL 1 (OpenGL ES 2, what the SDL path requests via CONTEXT_PROFILE_ES 2.0) by default;
        // [SDL2Drv.SDL2Client] WebGLVersion=2 or -WEBGL=2 asks for WebGL 2 (OpenGL ES 3), on which GL4ES runs its
        // OpenGL ES 3 back end (LIBGL_ES=3, GL4ESRenderDevice::SetRes). A browser without WebGL 2 gets WebGL 1.
        INT WebGLVersion = GetOuterUSDL2Client()->WebGLVersion;
        Parse( appCmdLine(), TEXT("WEBGL="), WebGLVersion );
        Attrs.majorVersion              = ( WebGLVersion == 2 ) ? 2 : 1;
        Attrs.minorVersion              = 0;
        Attrs.alpha                     = 0;
        Attrs.depth                     = 1;
        // GL4ESRenderDevice::SetRes never asks SDL for a stencil buffer, so the web context has none
        // either: the same pixel format as the native path.
        Attrs.stencil                   = 0;
        // MSAA is decided here on the threaded web build (SDL never creates the context, so
        // GL4ESRenderDevice::SetRes's SDL_GL_MULTISAMPLE* hints do not apply). WebGL exposes a boolean and picks
        // the sample count itself, so the configured count only decides on/off. NumAASamples (0 = off) is read
        // from the active render device's ini section, through the old MultisampleBuffers/MultisampleSamples
        // pair when it is absent; the default (4) is UGL4ESRenderDevice::StaticConstructor's.
        {
            INT AASamples = 4;
            const TCHAR* RenDevStr = GConfig->GetStr( TEXT("Engine.Engine"), TEXT("RenderDevice"), TEXT("System") );
            if( RenDevStr && *RenDevStr && !GConfig->GetInt( RenDevStr, TEXT("NumAASamples"), AASamples, TEXT("System") ) )
            {
                INT MsBuffers = 1, MsSamples = 4;
                UBOOL HaveOld = GConfig->GetInt( RenDevStr, TEXT("MultisampleBuffers"), MsBuffers, TEXT("System") );
                HaveOld = GConfig->GetInt( RenDevStr, TEXT("MultisampleSamples"), MsSamples, TEXT("System") ) || HaveOld;
                if( HaveOld )
                    AASamples = ( MsBuffers > 0 && MsSamples > 1 ) ? MsSamples : 0;
            }
            Attrs.antialias = AASamples > 1 ? 1 : 0;
            // NAME_Log: the inis suppress Init once the engine is up, and the viewport comes after. The
            // buffers/samples wording is the one the host's antialiasing check matches.
            debugf( NAME_Log, TEXT("WebGL antialias=%i (buffers=%i samples=%i from [%s])"),
                (INT)Attrs.antialias, AASamples > 1 ? 1 : 0, AASamples > 1 ? AASamples : 0, RenDevStr ? RenDevStr : TEXT("<none>") );
        }
        // explicitSwapControl + renderViaOffscreenBackBuffer make the frame ours to present: without them
        // the browser composites at its own cadence and commit_frame() does nothing on a worker.
        Attrs.explicitSwapControl       = 1;
        Attrs.renderViaOffscreenBackBuffer = 1;
        EMSCRIPTEN_WEBGL_CONTEXT_HANDLE WebGLCtx = emscripten_webgl_create_context( "#canvas", &Attrs );
        if( WebGLCtx <= 0 && Attrs.majorVersion == 2 )
        {
            debugf( NAME_Init, TEXT("No WebGL 2 context on the worker (handle %i): WebGL 1"), (INT)WebGLCtx );
            Attrs.majorVersion = 1;
            WebGLCtx = emscripten_webgl_create_context( "#canvas", &Attrs );
        }
        if( WebGLCtx <= 0 )
        {
            debugf( NAME_Init, TEXT("Could not create WebGL context on the worker (handle %i)"), (INT)WebGLCtx );
            return 0;
        }
        if( emscripten_webgl_make_context_current( WebGLCtx ) != EMSCRIPTEN_RESULT_SUCCESS )
        {
            debugf( NAME_Init, TEXT("Could not make the worker WebGL context current") );
            return 0;
        }
        // Stash it where the rest of the viewport already tests for "do we have GL".
        GLContext = (SDL_GLContext)(intptr_t)WebGLCtx;
        debugf( NAME_Init, TEXT("Created WebGL %i.0 context on the worker (handle %i)"), (INT)Attrs.majorVersion, (INT)WebGLCtx );
#else
        // Check which render device is active.
        const TCHAR* RenDevStr = GConfig->GetStr(TEXT("Engine.Engine"), TEXT("RenderDevice"), TEXT("System"));
        if( appStrfind(RenDevStr, TEXT("GL4ESDrv")) )
        {
            // GL4ESDrv: create GLES2 context for GL4ES output.
            SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
            SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
            SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
        }
        else
        {
#ifdef __EMSCRIPTEN__
            // Emscripten has no desktop/compatibility GL, only GLES2/WebGL. A device with no GL pipeline of
            // its own (Engine.NullRenderDevice) still needs a context for the viewport to size and present, so
            // request GLES2 like GL4ESDrv; the null device draws nothing into it. A compatibility request
            // returns no context on the web, and ResizeViewport would fail.
            SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
            SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 2 );
            SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
#else
            SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );
#endif
        }
        GLContext = SDL_GL_CreateContext( Window );
        if( !GLContext )
        {
            debugf( NAME_Init, TEXT("SDL_GL_CreateContext failed: %s"), appFromAnsi(SDL_GetError()) );
            return 0;
        }
        SDL_GL_MakeCurrent( Window, GLContext );
#endif
    }

#if defined(__ANDROID__)
    // On Android SDL binds the GL window to the fullscreen SurfaceView and IGNORES the requested
    // (ini windowed) size, so the requested NewX/NewY leave glViewport(0,0,SizeX,SizeY) (GL4ES Lock)
    // covering only a corner of the real surface -- the rest is unrendered (black). Size the viewport
    // to the actual drawable now that the EGL surface exists. SDL_GL_GetDrawableSize returns the EGL
    // surface pixel size (== SDL_GetWindowSize on Android); GetWindowSize is the fallback.
    {
        int DrawW = 0, DrawH = 0;
        SDL_GL_GetDrawableSize( Window, &DrawW, &DrawH );
        if( DrawW <= 0 || DrawH <= 0 )
            SDL_GetWindowSize( Window, &DrawW, &DrawH );
        if( DrawW > 0 && DrawH > 0 )
        {
            NewX  = DrawW;
            NewY  = DrawH;
            SizeX = DrawW;
            SizeY = DrawH;
        }
    }
#endif

    // Make this viewport current and update its title bar.
    GetOuterUClient()->MakeCurrent( this );

    // Update audio.
    if( SavedViewport && SavedViewport!=Client->Engine->Audio->GetViewport() )
        Client->Engine->Audio->SetViewport( SavedViewport );

    UpdateWindowFrame();

    // Save info.
    //
    // A degenerate size is never persisted: the size read back on the next launch lays out the GUI, and a
    // small enough one leaves the game unable to draw a menu (UGUITabControl::PreDraw divides by the number of
    // tab buttons in a row, zero at a two-pixel width; FCanvasUtil::Vertices grows until REALLOC FAILED).
    // The floor applies to the save only: the live viewport keeps its real size. 320x240 is UT2004's own
    // minimum supported mode.
    if( RenDev && !GIsEditor && bSaveSize )
    {
        const INT MinSaveX = 320, MinSaveY = 240;
        if( NewX >= MinSaveX && NewY >= MinSaveY )
        {
            if( NewBlitFlags & BLIT_Fullscreen )
            {
                Client->FullscreenViewportX  = NewX;
                Client->FullscreenViewportY  = NewY;
            }
            else
            {
                Client->WindowedViewportX  = NewX;
                Client->WindowedViewportY  = NewY;
            }
            Client->SaveConfig();
        }
        else if( NewX || NewY )
        {
            debugf( NAME_Log,
                    TEXT("[SDL2Viewport] NOT saving a %ix%i viewport - below the %ix%i floor. The ini keeps %ix%i so the next launch can still draw a menu."),
                    NewX, NewY, MinSaveX, MinSaveY,
                    Client->WindowedViewportX, Client->WindowedViewportY );
        }
    }

    return 1;
    unguard;
}

/*-----------------------------------------------------------------------------
    Input.
-----------------------------------------------------------------------------*/

//
// Input event router.
//
UBOOL USDL2Viewport::CauseInputEvent( INT iKey, EInputAction Action, FLOAT Delta )
{
    guard(USDL2Viewport::CauseInputEvent);
    return GetOuterUSDL2Client()->Engine->InputEvent( this, (EInputKey)iKey, Action, Delta );
    unguard;
}

//
// Close the pad in use (it was removed, or UseJoystick went off). Nothing may stay held: every pad key is released and
// the sticks stop (the trailing 0 of each goes out on the next tick).
//
void USDL2Viewport::ClosePad()
{
    guard(USDL2Viewport::ClosePad);
    if( GPad )
        SDL_GameControllerClose( GPad );
    GPad = NULL;
    for( INT b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++ )
        if( GPadButtonMap[b] != IK_None )
            CauseInputEvent( GPadButtonMap[b], IST_Release );
    if( GPadRT ) { GPadRT = 0; CauseInputEvent( IK_Joy1, IST_Release ); }
    if( GPadLT ) { GPadLT = 0; CauseInputEvent( IK_Joy3, IST_Release ); }
    GPadMoveX = GPadMoveY = GPadYaw = GPadPitch = 0.f;
    unguard;
}

//
// Update input -- full SDL2 event loop.
// Handles keyboard (scancode-based), mouse (relative + absolute),
// text input, and window events.
//
void USDL2Viewport::UpdateInput( UBOOL Reset, FLOAT DeltaSeconds )
{
    guard(USDL2Viewport::UpdateInput);

    USDL2Client* Client = GetOuterUSDL2Client();

    SDL_Event Ev;
    // Last tick's held-at-connect presses end now (a tap; GPadHeldTap).
    for( INT k = 0; k < GPadHeldTapNum; k++ )
        CauseInputEvent( GPadHeldTap[k], IST_Release );
    GPadHeldTapNum = 0;

    // UseJoystick, every tick: the game's own option ("Enable Joystick": `set ini:Engine.Engine.ViewportManager
    // UseJoystick ...`) sets it on this live client, so it takes effect at once. Off: the pad is closed. Back on: the
    // first game controller present is opened (one connected while it was off sent its ADDED event then).
    if( (Client->UseJoystick != 0) != GPadsOn )
    {
        GPadsOn = Client->UseJoystick != 0;
        debugf( NAME_Log, TEXT("SDL2: game pads %s (UseJoystick)"), GPadsOn ? TEXT("on") : TEXT("off") );
        if( !GPadsOn )
        {
            // Only a pad that is open has keys to release (at start, with no pad, there is nothing to send).
            if( GPad )
                ClosePad();
        }
        else
            for( INT i = 0; i < SDL_NumJoysticks() && !GPad; i++ )
                if( SDL_IsGameController( i ) && (GPad = SDL_GameControllerOpen( i )) != NULL )
                {
                    const char* Name = SDL_GameControllerName( GPad );
                    debugf( NAME_Log, TEXT("SDL2: game pad connected: %s"), Name ? appFromAnsi(Name) : TEXT("(no name)") );
                }
    }

    while( SDL_PollEvent(&Ev) )
    {
        switch( Ev.type )
        {
        case SDL_QUIT:
            GIsRequestingExit = 1;
            break;

        case SDL_WINDOWEVENT:
            switch( Ev.window.event )
            {
            case SDL_WINDOWEVENT_RESIZED:
                SizeX = Ev.window.data1;
                SizeY = Ev.window.data2;
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
                // The canvas's device-pixel size, not the CSS size SDL reports.
                ufrontCanvasDevicePixelSize( SizeX, SizeY );
#endif
                // Resize the GL framebuffer to match the new window size.
                if( RenDev && SizeX > 0 && SizeY > 0 )
                    RenDev->SetRes( this, SizeX, SizeY, BlitFlags & BLIT_Fullscreen );
                break;
            case SDL_WINDOWEVENT_FOCUS_LOST:
                SetMouseCapture(0, 0, 0);
                break;
            }
            break;

        case SDL_TEXTINPUT:
            // Route printable characters to Engine->Key for console/chat text entry.
            // Iterate bytes for ASCII range only.
            for( const char* p = Ev.text.text; *p && p < Ev.text.text + sizeof(Ev.text.text); ++p )
            {
                if( *p < 0 )
                    break;  // Non-ASCII (high bit set), stop.
                // Pass the character as both the EInputKey and the Unicode arg. The
                // UnrealScript console KeyType handler (Console.uc / ExtendedConsole.uc)
                // gates character insertion behind `if( Key >= 0x20 )`, so passing
                // IK_None (0) would drop every printable character, as SDLDrv's
                // SDLViewport.cpp passed keysym.unicode as the Key.
                Client->Engine->Key( this, (EInputKey)(BYTE)*p, (TCHAR)*p );
            }
            break;

        case SDL_KEYDOWN:
        {
            // Bounds check scancode.
            if( Ev.key.keysym.scancode >= SDL2DRV_MAX_SCANCODES )
                break;
            BYTE Key = KeyMap[Ev.key.keysym.scancode];
            if( Key == 0 )
                break;

            // Alt+Enter toggles fullscreen.
            if( (Ev.key.keysym.mod & KMOD_ALT) && Key == IK_Enter )
            {
                ToggleFullscreen();
                break;
            }

            CauseInputEvent( Key, IST_Press );
            break;
        }

        case SDL_KEYUP:
        {
            // Bounds check scancode.
            if( Ev.key.keysym.scancode >= SDL2DRV_MAX_SCANCODES )
                break;
            BYTE Key = KeyMap[Ev.key.keysym.scancode];
            if( Key == 0 )
                break;

            CauseInputEvent( Key, IST_Release );
            break;
        }

        case SDL_MOUSEBUTTONDOWN:
        {
            // Bounds check button index.
            if( Ev.button.button >= ARRAY_COUNT(MouseButtonMap) )
                break;
            BYTE Key = MouseButtonMap[Ev.button.button];
            if( Key == IK_None )
                break;

            // Track held mouse button state for MouseDelta.
            if( Ev.button.button == SDL_BUTTON_LEFT )   ViewportButtonFlags |= MOUSE_Left;
            if( Ev.button.button == SDL_BUTTON_RIGHT )  ViewportButtonFlags |= MOUSE_Right;
            if( Ev.button.button == SDL_BUTTON_MIDDLE ) ViewportButtonFlags |= MOUSE_Middle;

            CauseInputEvent( Key, IST_Press );
            break;
        }

        case SDL_MOUSEBUTTONUP:
        {
            // Bounds check button index.
            if( Ev.button.button >= ARRAY_COUNT(MouseButtonMap) )
                break;
            BYTE Key = MouseButtonMap[Ev.button.button];
            if( Key == IK_None )
                break;

            // Clear held mouse button state.
            if( Ev.button.button == SDL_BUTTON_LEFT )   ViewportButtonFlags &= ~MOUSE_Left;
            if( Ev.button.button == SDL_BUTTON_RIGHT )  ViewportButtonFlags &= ~MOUSE_Right;
            if( Ev.button.button == SDL_BUTTON_MIDDLE ) ViewportButtonFlags &= ~MOUSE_Middle;

            CauseInputEvent( Key, IST_Release );
            break;
        }

        case SDL_MOUSEWHEEL:
            if( Ev.wheel.y > 0 )
            {
                CauseInputEvent( IK_MouseWheelUp, IST_Press );
                CauseInputEvent( IK_MouseWheelUp, IST_Release );
            }
            else if( Ev.wheel.y < 0 )
            {
                CauseInputEvent( IK_MouseWheelDown, IST_Press );
                CauseInputEvent( IK_MouseWheelDown, IST_Release );
            }
            break;

        case SDL_MOUSEMOTION:
        {
            // The UE2 GUI reads WindowsMouseX/Y for cursor position and depends on
            // IK_MouseX/Y axis events for hover tracking. Two regimes, switched by the
            // GUI-owned bShowWindowsMouse flag (set on menu open/close once
            // bWindowsMouseAvailable is on -- see ResizeViewport):
            //   * Menu   (bShowWindowsMouse): ABSOLUTE -- track the OS cursor so the GUI
            //     (Scale==1.0) hit-tests under the visible browser cursor. SDL's
            //     Emscripten backend already scales motion.x/y into canvas (SizeX/SizeY).
            //   * In-game (!bShowWindowsMouse): RELATIVE -- accumulate deltas for
            //     pointer-locked mouse-look.
            INT DX = Ev.motion.xrel;
            INT DY = Ev.motion.yrel;

            if( bShowWindowsMouse )
            {
                WindowsMouseX = Ev.motion.x;
                WindowsMouseY = Ev.motion.y;
            }
            else
            {
                WindowsMouseX += DX;
                WindowsMouseY += DY;
            }

            // Clamp to viewport bounds.
            if( WindowsMouseX > (INT)SizeX ) WindowsMouseX = SizeX;
            if( WindowsMouseX < 0 )          WindowsMouseX = 0;
            if( WindowsMouseY > (INT)SizeY ) WindowsMouseY = SizeY;
            if( WindowsMouseY < 0 )          WindowsMouseY = 0;

            // Send axis events (drives both GUI hover re-eval and in-game look).
            if( DX )
                CauseInputEvent( IK_MouseX, IST_Axis, +DX );
            if( DY )
                CauseInputEvent( IK_MouseY, IST_Axis, -DY );

            // MouseDelta drives click-to-capture only; suppress it while a menu is up so
            // clicking a menu never re-grabs the pointer (UGameEngine also guards capture
            // on !bShowWindowsMouse).
            if( !bShowWindowsMouse && (DX || DY) )
                Client->Engine->MouseDelta( this, ViewportButtonFlags, DX, -DY );

            break;
        }

        // --- Game pads (one pad, the first connected). DefUser.ini binds every IK_Joy* target. ---
        case SDL_CONTROLLERDEVICEADDED:
        {
            // UseJoystick off: the pad stays closed (opened at once if the switch comes back on, below the loop).
            if( GPad || !Client->UseJoystick || !SDL_IsGameController( Ev.cdevice.which ) )
                break;
            GPad = SDL_GameControllerOpen( Ev.cdevice.which );
            if( !GPad )
            {
                debugf( NAME_Log, TEXT("SDL2: game pad %i could not be opened: %s"), Ev.cdevice.which, appFromAnsi(SDL_GetError()) );
                break;
            }
            const char* Name = SDL_GameControllerName( GPad );
            // NAME_Log: the inis suppress Init once the engine is up, and a pad comes at any time.
            debugf( NAME_Log, TEXT("SDL2: game pad connected: %s"), Name ? appFromAnsi(Name) : TEXT("(no name)") );
#if defined(__EMSCRIPTEN__)
            {
                // Give the game the press that made the browser list the pad (SDL never reports it).
                const char* Id = SDL_JoystickName( SDL_GameControllerGetJoystick( GPad ) );
                const DWORD Held = Id ? WebPadPressed( Id ) : 0;
                static const INT W3CToSDL[17] =
                {
                    SDL_CONTROLLER_BUTTON_A, SDL_CONTROLLER_BUTTON_B, SDL_CONTROLLER_BUTTON_X, SDL_CONTROLLER_BUTTON_Y,
                    SDL_CONTROLLER_BUTTON_LEFTSHOULDER, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER, -1, -1,
                    SDL_CONTROLLER_BUTTON_BACK, SDL_CONTROLLER_BUTTON_START,
                    SDL_CONTROLLER_BUTTON_LEFTSTICK, SDL_CONTROLLER_BUTTON_RIGHTSTICK,
                    SDL_CONTROLLER_BUTTON_DPAD_UP, SDL_CONTROLLER_BUTTON_DPAD_DOWN,
                    SDL_CONTROLLER_BUTTON_DPAD_LEFT, SDL_CONTROLLER_BUTTON_DPAD_RIGHT, SDL_CONTROLLER_BUTTON_GUIDE,
                };
                for( INT b = 0; b < 17; b++ )
                {
                    if( !(Held & (1u << b)) )
                        continue;
                    if( b == 6 || b == 7 )   // a trigger: a button in the browser's layout, an axis in SDL's
                    {
                        const BYTE Key = b == 6 ? IK_Joy3 : IK_Joy1;
                        CauseInputEvent( Key, IST_Press );
                        GPadHeldTap[GPadHeldTapNum++] = Key;  // released next tick (a tap)
                    }
                    else if( GPadButtonMap[W3CToSDL[b]] != IK_None )
                    {
                        CauseInputEvent( GPadButtonMap[W3CToSDL[b]], IST_Press );
                        GPadHeldTap[GPadHeldTapNum++] = GPadButtonMap[W3CToSDL[b]];  // released next tick (a tap)
                    }
                }
                if( Held )
                    debugf( NAME_Log, TEXT("SDL2: buttons held at the connect given to the game: 0x%x"), Held );
            }
#endif
            break;
        }

        case SDL_CONTROLLERDEVICEREMOVED:
        {
            if( !GPad || Ev.cdevice.which != PadInstance( GPad ) )
                break;
            ClosePad();
            debugf( NAME_Log, TEXT("SDL2: game pad disconnected") );
            break;
        }

        case SDL_CONTROLLERBUTTONDOWN:
        case SDL_CONTROLLERBUTTONUP:
        {
            if( !GPad || Ev.cbutton.which != PadInstance( GPad ) || Ev.cbutton.button >= SDL_CONTROLLER_BUTTON_MAX )
                break;
            BYTE Key = GPadButtonMap[Ev.cbutton.button];
            if( Key != IK_None )
                CauseInputEvent( Key, Ev.type == SDL_CONTROLLERBUTTONDOWN ? IST_Press : IST_Release );
            break;
        }

        // The sticks and triggers are read from the pad's state every tick (after this loop), not from
        // SDL_CONTROLLERAXISMOTION.

        default:
            break;
        }
    }

    // The sticks and triggers come from the pad's state every tick, not from SDL_CONTROLLERAXISMOTION: SDL's
    // Emscripten joystick driver reports no axis values when a pad is opened, so SDL takes the first value an axis
    // reports as its resting point and posts no event for it (SDL_PrivateJoystickAxis). The state is always right.
    if( GPad )
    {
        const FLOAT Scale = 1.f / 32767.f;
        GPadMoveX = PadAxis( SDL_GameControllerGetAxis( GPad, SDL_CONTROLLER_AXIS_LEFTX ) * Scale );  // -> IK_JoyX (strafe)
        GPadMoveY = PadAxis( SDL_GameControllerGetAxis( GPad, SDL_CONTROLLER_AXIS_LEFTY ) * Scale );  // -> IK_JoyY (move)
        GPadYaw   = PadAxis( SDL_GameControllerGetAxis( GPad, SDL_CONTROLLER_AXIS_RIGHTX ) * Scale ); // -> IK_JoyV (turn)
        GPadPitch = PadAxis( SDL_GameControllerGetAxis( GPad, SDL_CONTROLLER_AXIS_RIGHTY ) * Scale ); // -> IK_JoySlider1
        const UBOOL RT = SDL_GameControllerGetAxis( GPad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT ) * Scale > GPadTriggerPress;
        const UBOOL LT = SDL_GameControllerGetAxis( GPad, SDL_CONTROLLER_AXIS_TRIGGERLEFT ) * Scale > GPadTriggerPress;
        if( RT != GPadRT ) { GPadRT = RT; CauseInputEvent( IK_Joy1, RT ? IST_Press : IST_Release ); }  // Fire
        if( LT != GPadLT ) { GPadLT = LT; CauseInputEvent( IK_Joy3, LT ? IST_Press : IST_Release ); }  // AltFire
    }

    // Game pad sticks -> joystick axes, held-absolute (re-sent every tick while nonzero, plus one trailing 0).
    // Each scaled by this tick's DeltaSeconds: a rate, the same at any frame rate (GPadLookFactor, above).
    if( GPadMoveX != 0.f || GPadLastMoveX != 0.f )
        CauseInputEvent( IK_JoyX, IST_Axis, GPadMoveX * GPadMoveFactor * DeltaSeconds );
    if( GPadMoveY != 0.f || GPadLastMoveY != 0.f )
        CauseInputEvent( IK_JoyY, IST_Axis, GPadMoveY * GPadMoveFactor * DeltaSeconds );
    if( GPadYaw != 0.f || GPadLastYaw != 0.f )
        CauseInputEvent( IK_JoyV, IST_Axis, GPadYaw * GPadLookFactor * DeltaSeconds );
    if( GPadPitch != 0.f || GPadLastPitch != 0.f )
        CauseInputEvent( IK_JoySlider1, IST_Axis, -GPadPitch * GPadLookFactor * DeltaSeconds );  // stick up = look up
    GPadLastMoveX = GPadMoveX;
    GPadLastMoveY = GPadMoveY;
    GPadLastYaw   = GPadYaw;
    GPadLastPitch = GPadPitch;

#if defined(__ANDROID__)
    // --- Android touch overlay: fold UI-thread intent into engine input. ---

    // Publish menu vs gameplay for the Java overlay (menu -> overlay hides ->
    // touches pass through to SDL's tap-to-click). End-of-tick value is what the
    // overlay should see.
    GAndroidMenuVisible = bShowWindowsMouse ? true : false;

    // Button edges (fire/jump/...). Swap the queue out under the lock, then inject
    // unlocked so CauseInputEvent never runs while the UI thread might be enqueuing.
    {
        std::vector<FAndroidButtonEdge> Edges;
        {
            std::lock_guard<std::mutex> Lock( GAndroidBtnMutex );
            Edges.swap( GAndroidBtnQueue );
        }
        for( size_t i = 0; i < Edges.size(); ++i )
            CauseInputEvent( Edges[i].Key, (EInputAction)Edges[i].Action );
    }

    // Drag-look -> mouse axes, exactly like the SDL_MOUSEMOTION handler above
    // (X positive = right, Y negated). Consume-and-reset the accumulator.
    {
        FLOAT LookX = GAndroidLookX.exchange( 0.f, std::memory_order_relaxed ) * GAndroidLookScale;
        FLOAT LookY = GAndroidLookY.exchange( 0.f, std::memory_order_relaxed ) * GAndroidLookScale;
        if( LookX != 0.f )
            CauseInputEvent( IK_MouseX, IST_Axis, +LookX );
        if( LookY != 0.f )
            CauseInputEvent( IK_MouseY, IST_Axis, -LookY );
    }

    // Move stick -> joystick axes. IK_JoyX/Y are held-absolute (SpeedBase scales
    // the value), so feeding the current value every tick while nonzero gives
    // continuous movement regardless of whether the engine latches the axis; one
    // trailing 0 stops it on release.
    {
        FLOAT MoveX = GAndroidMoveX.load( std::memory_order_relaxed );
        FLOAT MoveY = GAndroidMoveY.load( std::memory_order_relaxed );
        if( MoveX != 0.f || GAndroidLastMoveX != 0.f )
            CauseInputEvent( IK_JoyX, IST_Axis, MoveX );
        if( MoveY != 0.f || GAndroidLastMoveY != 0.f )
            CauseInputEvent( IK_JoyY, IST_Axis, MoveY );
        GAndroidLastMoveX = MoveX;
        GAndroidLastMoveY = MoveY;
    }

#endif // __ANDROID__

    unguard;
}

//
// Mouse capture for FPS mode.
// Uses SDL2 relative mouse mode (cursor hidden + pointer locked + raw input).
//
void USDL2Viewport::SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL FocusOnly )
{
    guard(USDL2Viewport::SetMouseCapture);

    SDL_SetRelativeMouseMode( Capture ? SDL_TRUE : SDL_FALSE );

    // On release (entering a menu) seed the cursor to the viewport centre so the GUI
    // has a sane absolute position until the first motion event refreshes it.
    if( !Capture )
    {
        WindowsMouseX = SizeX / 2;
        WindowsMouseY = SizeY / 2;
    }

    unguard;
}

//
// Get localized key name via SDL2 API.
// Reverse-looks up KeyMap to find the SDL scancode for a given EInputKey,
// then uses SDL_GetKeyName to get the OS-localized name.
//
TCHAR* USDL2Viewport::GetLocalizedKeyName( EInputKey Key )
{
    guard(USDL2Viewport::GetLocalizedKeyName);

    static TCHAR Result[64];
    Result[0] = 0;

    // Reverse lookup: find the SDL scancode that maps to this EInputKey.
    SDL_Scancode FoundScancode = SDL_SCANCODE_UNKNOWN;
    for( INT i = 0; i < SDL2DRV_MAX_SCANCODES; i++ )
    {
        if( KeyMap[i] == (BYTE)Key )
        {
            FoundScancode = (SDL_Scancode)i;
            break;
        }
    }

    if( FoundScancode != SDL_SCANCODE_UNKNOWN )
    {
        // Get the localized key name from SDL2.
        const char* Name = SDL_GetKeyName( SDL_GetKeyFromScancode(FoundScancode) );
        if( Name && Name[0] )
        {
            // Convert from char* to TCHAR*.
            const TCHAR* WName = appFromAnsi(Name);
            appStrncpy( Result, WName, ARRAY_COUNT(Result) );
            return Result;
        }
    }

    // Fallback: return empty string.
    Result[0] = 0;
    return Result;

    unguard;
}

/*-----------------------------------------------------------------------------
    The End.
-----------------------------------------------------------------------------*/
