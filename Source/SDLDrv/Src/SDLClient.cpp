/*=============================================================================
	SDLClient.cpp: USDLClient code.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

        SDL website: http://www.libsdl.org/

Revision history:
	* Created by Ryan C. Gordon, based on WinDrv.
      This is an updated rewrite of the original SDLDrv.
=============================================================================*/

#include "SDLDrv.h"

/*-----------------------------------------------------------------------------
	Class implementation.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(USDLClient);

/*-----------------------------------------------------------------------------
	USDLClient implementation.
-----------------------------------------------------------------------------*/

SDL_Joystick *USDLClient::Joystick = NULL;
int USDLClient::JoystickAxes = 0;
int USDLClient::JoystickButtons = 0;
int USDLClient::JoystickHats = 0;
int USDLClient::JoystickBalls = 0;


//
// USDLClient constructor.
//
USDLClient::USDLClient()
{
	guard(USDLClient::USDLClient);
	unguard;
}

//
// Static init.
//
void USDLClient::StaticConstructor()
{
	guard(USDLClient::StaticConstructor);

	new(GetClass(),TEXT("UseJoystick"),          RF_Public)UBoolProperty(CPP_PROPERTY(UseJoystick),          TEXT("Display"),  CPF_Config );
	new(GetClass(),TEXT("JoystickNumber"),       RF_Public)UIntProperty(CPP_PROPERTY(JoystickNumber),        TEXT("Joystick"), CPF_Config );
	new(GetClass(),TEXT("JoystickHatNumber"),    RF_Public)UIntProperty(CPP_PROPERTY(JoystickHatNumber),     TEXT("Joystick"), CPF_Config );
	new(GetClass(),TEXT("StartupFullscreen"),    RF_Public)UBoolProperty(CPP_PROPERTY(StartupFullscreen),    TEXT("Display"),  CPF_Config );
	new(GetClass(),TEXT("ScaleJBX"),             RF_Public)UFloatProperty(CPP_PROPERTY(ScaleJBX),            TEXT("Joystick"), CPF_Config );
	new(GetClass(),TEXT("ScaleJBY"),             RF_Public)UFloatProperty(CPP_PROPERTY(ScaleJBY),            TEXT("Joystick"), CPF_Config );
	new(GetClass(),TEXT("IgnoreHat"),            RF_Public)UBoolProperty(CPP_PROPERTY(IgnoreHat),            TEXT("Joystick"), CPF_Config );
	new(GetClass(),TEXT("IgnoreUngrabbedMouse"), RF_Public)UBoolProperty(CPP_PROPERTY(IgnoreUngrabbedMouse), TEXT("Display"),  CPF_Config );
	new(GetClass(),TEXT("AllowUnicodeKeys"),     RF_Public)UBoolProperty(CPP_PROPERTY(AllowUnicodeKeys),     TEXT("Display"),  CPF_Config );
	new(GetClass(),TEXT("AllowCommandQKeys"),    RF_Public)UBoolProperty(CPP_PROPERTY(AllowCommandQKeys),    TEXT("Display"),  CPF_Config );
	new(GetClass(),TEXT("MacFakeMouseButtons"),  RF_Public)UBoolProperty(CPP_PROPERTY(MacFakeMouseButtons),  TEXT("Display"),  CPF_Config );
	new(GetClass(),TEXT("MacKeepAllScreensOn"),  RF_Public)UBoolProperty(CPP_PROPERTY(MacKeepAllScreensOn),  TEXT("Display"),  CPF_Config );
	new(GetClass(),TEXT("TextToSpeechFile"),     RF_Public)UStrProperty(CPP_PROPERTY(TextToSpeechFile),      TEXT("Display"),  CPF_Config );
	new(GetClass(),TEXT("MacNativeTextToSpeech"),RF_Public)UBoolProperty(CPP_PROPERTY(MacNativeTextToSpeech),TEXT("Display"),  CPF_Config );

	unguard;
}


// just in case.  :)  --ryan.
static void sdl_atexit_handler(void)
{
    static bool already_called = false;

    if (!already_called)
    {
        already_called = true;
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}



//
// Initialize the platform-specific viewport manager subsystem.
// Must be called after the Unreal object manager has been initialized.
// Must be called before any viewports are created.
//
void USDLClient::Init( UEngine* InEngine )
{
	guard(USDLClient::USDLClient);

	// Init base.
	UClient::Init( InEngine );

	// Fix up the environment variables for 3dfx.
	putenv("MESA_GLX_FX=f");
	putenv("FX_GLIDE_NO_SPLASH=1");
	// Force MESA to not initialize an atexit handler.
	putenv("MESA_FX_NO_SIGNALS=1");

    if (!MacFakeMouseButtons)
        putenv("SDL_HAS3BUTTONMOUSE=1");

    if (MacKeepAllScreensOn)
        putenv("SDL_SINGLEDISPLAY=1");

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) == -1)
    {
        const TCHAR *err = appFromAnsi(SDL_GetError());
		appErrorf(TEXT("Couldn't initialize SDL: %s\n"), err);
		appExit();
	}

    atexit(sdl_atexit_handler);

	// Note configuration.
	PostEditChange();

	// Default res option.
	if( ParseParam(appCmdLine(),TEXT("defaultres")) )
	{
	    // gam ---
		WindowedViewportX  = FullscreenViewportX  = MenuViewportX   = 640;
		WindowedViewportY  = FullscreenViewportY  = MenuViewportY   = 480;
		// --- gam
	}

	// For us foreigners ;-)
	SDL_EnableUNICODE(true);

	// Joystick.
    int joystickCount = SDL_NumJoysticks();
    JoystickButtons = 0;
    JoystickAxes = 0;
    debugf( NAME_Init, TEXT("Detected %d joysticks"), joystickCount);
    if (joystickCount > 0)
    {
    	if ( JoystickNumber >= joystickCount )
    	{
    	    debugf( NAME_Init, TEXT("JoystickNumber exceeds the number of detected joysticks, setting to 0."));
            JoystickNumber = 0;
    	}

		const char *joyNameAnsi = SDL_JoystickName(JoystickNumber);
		if (joyNameAnsi == NULL)
			joyNameAnsi = "Unknown Joystick";
        const TCHAR *joyName = appFromAnsi(joyNameAnsi);
    	debugf( NAME_Init, TEXT("Joystick [%i] : %s") ,
				JoystickNumber , joyName);
    			
    	Joystick = SDL_JoystickOpen( JoystickNumber );
		if ( Joystick == NULL )
		{
		    debugf( NAME_Init, TEXT("Couldn't open joystick [%s]"), joyName );
			UseJoystick = false;
  		}
   	    else
   		{
   			JoystickButtons = SDL_JoystickNumButtons( Joystick );
   		    debugf( NAME_Init, TEXT("Joystick has %i buttons"), JoystickButtons );

   			JoystickHats = SDL_JoystickNumHats( Joystick );
   		    debugf( NAME_Init, TEXT("Joystick has %i hats"), JoystickHats );

   			JoystickBalls = SDL_JoystickNumBalls( Joystick );
   		    debugf( NAME_Init, TEXT("Joystick has %i balls"), JoystickBalls );

            if ((JoystickHatNumber < 0) || (JoystickHatNumber >= JoystickHats))
            {
    	        debugf( NAME_Init, TEXT("JoystickHatNumber exceeds the number of detected hats, setting to 0."));
                JoystickHatNumber = 0;
            }

            if (JoystickButtons > 16)
                JoystickButtons = 16;

            if ((JoystickButtons > 12) && (JoystickHats > 0) && (!IgnoreHat))
                JoystickButtons = 12;  /* joy13 is first hat "button" */

            if (JoystickButtons != SDL_JoystickNumButtons(Joystick))
            {
                debugf( NAME_Init, TEXT("Too many joystick buttons; clamped to %d."), JoystickButtons);
                if ((JoystickHats > 0) && (!IgnoreHat))
                    debugf( NAME_Init, TEXT("(Disable hat switches with \"IgnoreHat=True\" to raise this.)"));
            }

   			JoystickAxes    = SDL_JoystickNumAxes( Joystick );
   			debugf( NAME_Init, TEXT("Joystick has %i axes"   ), JoystickAxes );

            // x + y + z + r + u + v + 2 "sliders" == 8 axes.
            if (JoystickAxes > 8)
            {
                debugf( NAME_Init, TEXT("Too many joystick axes; clamped to 8."));
                JoystickAxes = 8;
            }
   	    }
	}
	// Success.
	debugf( NAME_Init, TEXT("SDLClient initialized.") );

	unguard;
}


//
// Shut down the platform-specific viewport manager subsystem.
//
void USDLClient::Destroy()
{
	guard(USDLClient::Destroy);

	// Make sure to shut down Viewports first.
	for( INT i=0; i<Viewports.Num(); i++ )
		Viewports(i)->ConditionalDestroy();

	// Shut down GRenDev.
	Engine->GRenDev->Exit(NULL);

	if ( Joystick != NULL )
		SDL_JoystickClose( Joystick );

    SDL_QuitSubSystem(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);

	debugf( NAME_Exit, TEXT("SDL client shut down") );
	Super::Destroy();
	unguard;
}

void USDLClient::TeardownSR()
{
	guard(USDLClient::TeardownSR);
    // no-op, currently.
    unguard;
}

//
// Failsafe routine to shut down viewport manager subsystem
// after an error has occured. Not guarded.
//
void USDLClient::ShutdownAfterError()
{
	debugf( NAME_Exit, TEXT("Executing USDLClient::ShutdownAfterError") );
    SDL_Quit();

	if (Engine && Engine->Audio)
	{
		Engine->Audio->ConditionalShutdownAfterError();
	}

	for (INT i = Viewports.Num() - 1; i >= 0; i--)
	{
		USDLViewport *Viewport = (USDLViewport *) Viewports(i);
		Viewport->ConditionalShutdownAfterError();
	}

	Super::ShutdownAfterError();
}

void USDLClient::NotifyDestroy( void* Src )
{
	guard(USDLClient::NotifyDestroy);
	unguard;
}

//
// Command line.
//
UBOOL USDLClient::Exec( const TCHAR* Cmd, FOutputDevice& Ar )
{
	guard(USDLClient::Exec);
	if( UClient::Exec( Cmd, Ar ) )
	{
		return 1;
	}
	return 0;
	unguard;
}

//
// Perform timer-tick processing on all visible viewports.  This causes
// all realtime viewports, and all non-realtime viewports which have been
// updated, to be blitted.
//
void USDLClient::Tick()
{
	guard(USDLClient::Tick);

	// Blit any viewports that need blitting.
  	for( INT i=0; i<Viewports.Num(); i++ )
	{
		USDLViewport* Viewport = CastChecked<USDLViewport>(Viewports(i));
  		if ((Viewport->IsRealtime() || (Viewport->DirtyViewport != 0)) && Viewport->SizeX && Viewport->SizeY )
			Viewport->Repaint( (Viewport->DirtyViewport == -1) ? 0 : 1 );
	}
	
	unguard;
}

//
// Create a new viewport.
//
UViewport* USDLClient::NewViewport( const FName Name )
{
	guard(USDLClient::NewViewport);
	return new( this, Name )USDLViewport();
	unguard;
}

//
// Configuration change.
//
void USDLClient::PostEditChange()
{
	guard(USDLClient::PostEditChange);
	Super::PostEditChange();
	unguard;
}

//
// Enable or disable all viewport windows that have ShowFlags set (or all if ShowFlags=0).
//
void USDLClient::EnableViewportWindows( DWORD ShowFlags, int DoEnable )
{
	guard(USDLClient::EnableViewportWindows);
    /* meaningless in terms of SDL. */
	unguard;
}

//
// Show or hide all viewport windows that have ShowFlags set (or all if ShowFlags=0).
//
void USDLClient::ShowViewportWindows( DWORD ShowFlags, int DoShow )
{
	guard(USDLClient::ShowViewportWindows); 	
    /* meaningless in terms of SDL. */
	unguard;
}

//
// Make this viewport the current one.
// If Viewport=0, makes no viewport the current one.
//
void USDLClient::MakeCurrent( UViewport* InViewport )
{
	guard(UWindowsViewport::MakeCurrent);
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
UViewport* USDLClient::GetLastCurrent()
{
	guard(UWindowsViewport::GetLastCurrent);
	return LastCurrent;
	unguard;
}

/*-----------------------------------------------------------------------------
    That's all, folks.
-----------------------------------------------------------------------------*/

