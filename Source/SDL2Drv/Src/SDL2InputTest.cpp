/*=============================================================================
    SDL2InputTest.cpp: the input test channel (SDL2InputTest.h).
=============================================================================*/

#include "SDL2Drv.h"
#include "SDL2InputTest.h"

// Every SDL controller button and axis, in SDL's own order: with these masks SDL's mapping for a virtual controller
// makes joystick button/axis i controller button/axis i.
enum
{
    VPAD_BUTTONS    = SDL_CONTROLLER_BUTTON_DPAD_RIGHT + 1,
    VPAD_AXES       = SDL_CONTROLLER_AXIS_MAX,
};

FSDL2InputTest::FSDL2InputTest( const TCHAR* InPath )
:   File( NULL )
,   PartialLen( 0 )
,   Virtual( NULL )
,   VirtualID( -1 )
,   WatchLeft( 0 )
,   WatchIndex( 0 )
{
    File = fopen( TCHAR_TO_ANSI(InPath), "r" );
    debugf( NAME_Log, TEXT("SDL2InputTest: channel %s: %s"), InPath, File ? TEXT("open") : TEXT("cannot open") );
}

FSDL2InputTest::~FSDL2InputTest()
{
    Detach();
    if( File )
        fclose( File );
}

void FSDL2InputTest::Detach()
{
    if( !Virtual )
        return;
    SDL_JoystickClose( Virtual );
    Virtual = NULL;
    for( INT i = 0; i < SDL_NumJoysticks(); i++ )
        if( SDL_JoystickGetDeviceInstanceID( i ) == VirtualID )
        {
            SDL_JoystickDetachVirtual( i );
            break;
        }
    VirtualID = -1;
}

void FSDL2InputTest::Tick( USDL2Viewport* Viewport )
{
    guard(FSDL2InputTest::Tick);
    if( !File )
        return;
    if( WatchLeft > 0 )
    {
        APlayerController* PC = Viewport->Actor;
        APawn* Pawn = PC ? PC->Pawn : NULL;
        const FVector Zero( 0.f, 0.f, 0.f );
        const FVector& L = Pawn ? Pawn->Location : Zero;
        const FVector& V = Pawn ? Pawn->Velocity : Zero;
        const FVector& A = Pawn ? Pawn->Acceleration : Zero;
        debugf( NAME_Log, TEXT("SDL2InputTest: watch %s %i %.4f %i %i %.1f %.1f %.1f %.1f %.1f %.1f %.1f %i"),
            *WatchTag, WatchIndex++, appSeconds(), PC ? PC->Rotation.Yaw : 0, PC ? PC->Rotation.Pitch : 0, V.Size2D(),
            L.X, L.Y, V.X, V.Y, A.X, A.Y, Pawn ? (INT)Pawn->bIsWalking : 0 );
        WatchLeft--;
    }
    for( ;; )
    {
        int C = fgetc( File );
        if( C == EOF )
        {
            // Lines are appended while the file is read: forget the end of file and look again next update.
            clearerr( File );
            fseek( File, 0, SEEK_CUR );
            return;
        }
        if( C == '\r' )
            continue;
        if( C != '\n' )
        {
            if( PartialLen < (INT)sizeof(Partial) - 1 )
                Partial[PartialLen++] = (ANSICHAR)C;
            continue;
        }
        Partial[PartialLen] = 0;
        PartialLen = 0;
        if( !Partial[0] || Partial[0] == '#' )
            continue;
        if( Execute( Viewport, appFromAnsi(Partial) ) )
            return;
    }
    unguard;
}

UBOOL FSDL2InputTest::Execute( USDL2Viewport* Viewport, const TCHAR* Line )
{
    guard(FSDL2InputTest::Execute);
    debugf( NAME_Log, TEXT("SDL2InputTest: run %s"), Line );
    const TCHAR* Cmd = Line;
    if( ParseCommand(&Cmd,TEXT("PAD")) )
    {
        if( ParseCommand(&Cmd,TEXT("ATTACH")) )
        {
            if( Virtual )
                return 0;
            SDL_VirtualJoystickDesc Desc;
            SDL_zero( Desc );
            Desc.version     = SDL_VIRTUAL_JOYSTICK_DESC_VERSION;
            Desc.type        = SDL_JOYSTICK_TYPE_GAMECONTROLLER;
            Desc.naxes       = VPAD_AXES;
            Desc.nbuttons    = VPAD_BUTTONS;
            Desc.button_mask = (1u << VPAD_BUTTONS) - 1;
            Desc.axis_mask   = (1u << VPAD_AXES) - 1;
            Desc.name        = "SDL2InputTest virtual pad";
            // The viewport opens the first pad that comes while none is open: the virtual one takes the place.
            Viewport->ClosePad();
            INT Index = SDL_JoystickAttachVirtualEx( &Desc );
            if( Index < 0 )
            {
                debugf( NAME_Log, TEXT("SDL2InputTest: virtual pad: attach failed: %s"), appFromAnsi(SDL_GetError()) );
                return 0;
            }
            Virtual   = SDL_JoystickOpen( Index );
            VirtualID = Virtual ? SDL_JoystickInstanceID( Virtual ) : -1;
            // A trigger rests at the bottom of its raw range, which the controller reads as 0.
            if( Virtual )
            {
                SDL_JoystickSetVirtualAxis( Virtual, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768 );
                SDL_JoystickSetVirtualAxis( Virtual, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768 );
            }
            debugf( NAME_Log, TEXT("SDL2InputTest: virtual pad: joystick %i, game controller %i"), Index, (INT)SDL_IsGameController(Index) );
            return 1;
        }
        if( ParseCommand(&Cmd,TEXT("DETACH")) )
        {
            Detach();
            debugf( NAME_Log, TEXT("SDL2InputTest: virtual pad: detached") );
            return 1;
        }
        const UBOOL IsButton = ParseCommand(&Cmd,TEXT("BUTTON"));
        if( IsButton || ParseCommand(&Cmd,TEXT("AXIS")) )
        {
            FString Name;
            ParseToken( Cmd, Name, 0 );
            INT Value = appAtoi( Cmd );
            if( IsButton )
            {
                SDL_GameControllerButton B = SDL_GameControllerGetButtonFromString( TCHAR_TO_ANSI(*Name) );
                if( !Virtual || B == SDL_CONTROLLER_BUTTON_INVALID || (INT)B >= (INT)VPAD_BUTTONS )
                {
                    debugf( NAME_Log, TEXT("SDL2InputTest: virtual pad: bad button command") );
                    return 0;
                }
                SDL_JoystickSetVirtualButton( Virtual, B, Value ? SDL_PRESSED : SDL_RELEASED );
            }
            else
            {
                SDL_GameControllerAxis A = SDL_GameControllerGetAxisFromString( TCHAR_TO_ANSI(*Name) );
                if( !Virtual || A == SDL_CONTROLLER_AXIS_INVALID )
                {
                    debugf( NAME_Log, TEXT("SDL2InputTest: virtual pad: bad axis command") );
                    return 0;
                }
                // SDL maps a trigger's whole raw range onto the controller's 0..32767.
                if( A == SDL_CONTROLLER_AXIS_TRIGGERLEFT || A == SDL_CONTROLLER_AXIS_TRIGGERRIGHT )
                    Value = appRound( -32768.f + Clamp( Value, 0, 32767 ) * (65535.f / 32767.f) );
                SDL_JoystickSetVirtualAxis( Virtual, A, (Sint16)Clamp( Value, -32768, 32767 ) );
            }
            return 1;
        }
        debugf( NAME_Log, TEXT("SDL2InputTest: virtual pad: unknown command") );
        return 0;
    }
    else if( ParseCommand(&Cmd,TEXT("EXEC")) )
    {
        Viewport->Exec( Cmd, *GLog );
        return 1;
    }
    else if( ParseCommand(&Cmd,TEXT("WATCH")) )
    {
        FString Updates;
        ParseToken( Cmd, Updates, 0 );
        ParseToken( Cmd, WatchTag, 0 );
        WatchLeft  = appAtoi( *Updates );
        WatchIndex = 0;
        return 1;
    }
    else if( ParseCommand(&Cmd,TEXT("LOG")) )
    {
        return 0;
    }
    debugf( NAME_Log, TEXT("SDL2InputTest: unknown command") );
    return 0;
    unguard;
}
