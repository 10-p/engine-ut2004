/*=============================================================================
    SDL2InputTest.h: the input test channel (-INPUTTEST=<file> on the command line).

    A test harness appends command lines to <file>; the viewport runs them as they arrive, at the start of each
    input update. Pad commands drive a virtual game controller made with SDL's virtual joystick API, which takes the
    place of any pad open, so everything from SDL's game controller onwards is the real path. Commands:
      pad attach | pad detach
      pad button <sdl button name> <0|1>    a b x y back guide start leftstick rightstick leftshoulder
                                            rightshoulder dpup dpdown dpleft dpright
      pad axis <sdl axis name> <value>      leftx lefty rightx righty: -32768..32767 (y down is +);
                                            lefttrigger righttrigger: 0..32767
      exec <console command>                run on the viewport
      watch <updates> <tag>                 one "SDL2InputTest: watch <tag> <i> <seconds> <yaw> <pitch> <speed>
                                            <x> <y> <vx> <vy> <ax> <ay> <walking>" line per input update, for
                                            <updates> updates (yaw, pitch: the view; speed: the pawn's horizontal
                                            one; x, y: its location; v, a: its velocity and acceleration; walking:
                                            its bIsWalking)
      log <text>                            a marker line
    Without the switch nothing here runs.
=============================================================================*/

#include <stdio.h>

class FSDL2InputTest
{
public:
    FSDL2InputTest( const TCHAR* InPath );
    ~FSDL2InputTest();
    // Runs the commands that have arrived, stopping after one that changes the virtual pad so that every change is
    // seen by at least one event poll.
    void Tick( USDL2Viewport* Viewport );

private:
    FILE*           File;
    ANSICHAR        Partial[1024];
    INT             PartialLen;
    SDL_Joystick*   Virtual;
    SDL_JoystickID  VirtualID;
    INT             WatchLeft;
    INT             WatchIndex;
    FString         WatchTag;
    UBOOL Execute( USDL2Viewport* Viewport, const TCHAR* Line );
    void Detach();
};
