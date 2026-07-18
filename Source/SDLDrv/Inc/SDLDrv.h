/*=============================================================================
	SDLDrv.h: Simple Directmedia Layer cross-platform driver.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

        SDL website: http://www.libsdl.org/

Revision history:
	* Created by Ryan C. Gordon, based on WinDrv.
      This is an updated rewrite of the original SDLDrv.
=============================================================================*/

#ifndef _INC_SDLDRV
#define _INC_SDLDRV

#if MACOSX
//#include <SpeechSynthesis.h>
// OSX system headers conflict with several Unreal symbols (FVector, verify,
//  etc...), so hardcode the predeclarations we need here...)  --ryan.
extern "C" {
    struct SpeechChannelRecord {
      long                data[1];
    };
    typedef struct SpeechChannelRecord      SpeechChannelRecord;
    typedef SpeechChannelRecord *           SpeechChannel;

    typedef signed short OSErr;
    OSErr NewSpeechChannel(void *dummy, SpeechChannel *chan);
    OSErr SpeakText(SpeechChannel chan, const void *buf, unsigned long len);
    OSErr StopSpeech(SpeechChannel chan);
    short SpeechBusy(void);
    OSErr DisposeSpeechChannel(SpeechChannel chan);
}
#endif

/*----------------------------------------------------------------------------
	API.
----------------------------------------------------------------------------*/

#ifndef SDLDRV_API
	#define SDLDRV_API DLL_IMPORT
#endif

/*----------------------------------------------------------------------------
	Dependencies.
----------------------------------------------------------------------------*/

// When included from SDLLaunch (registration only), avoid pulling in the
// real SDL headers so SDL1 and SDL2 drivers can coexist in one binary.
#ifdef SDLDRV_REGISTRATION_ONLY
	struct SDL_Surface;
	struct _SDL_Joystick;
	typedef struct _SDL_Joystick SDL_Joystick;
#else
	#include "SDL.h"
#endif

// Unreal includes.
#include "Engine.h"
#include "UnRender.h"

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// Classes.
class USDLViewport;
class USDLClient;

/*-----------------------------------------------------------------------------
	USDLClient.
-----------------------------------------------------------------------------*/

//
// SDL implementation of the client.
//
class USDLClient : public UClient, public FNotifyHook
{
	DECLARE_CLASS(USDLClient,UClient,CLASS_Transient|CLASS_Config,SDLDrv)

	// Configuration.
	BITFIELD			UseJoystick;
	BITFIELD			StartupFullscreen;
	INT                 JoystickNumber;
	INT                 JoystickHatNumber;
	FLOAT               ScaleJBX;
	FLOAT               ScaleJBY;
	UBOOL               IgnoreHat;
	UBOOL               IgnoreUngrabbedMouse;
	UBOOL               AllowUnicodeKeys;
	UBOOL               AllowCommandQKeys;
	UBOOL               MacFakeMouseButtons;
	UBOOL               MacKeepAllScreensOn;
	UBOOL               MacNativeTextToSpeech;
	FStringNoInit       TextToSpeechFile;

	// Variables.
	UBOOL				InMenuLoop;
	UBOOL				ConfigReturnFullscreen;
	INT					NormalMouseInfo[3];
	INT					CaptureMouseInfo[3];

    static SDL_Joystick *Joystick;
    static int JoystickAxes;
    static int JoystickButtons;
    static int JoystickHats;
    static int JoystickBalls;

	// Constructors.
	USDLClient();
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
	USDLViewport.
-----------------------------------------------------------------------------*/

//
// An SDL viewport.
//
class USDLViewport : public UViewport
{
	DECLARE_CLASS(USDLViewport,UViewport,CLASS_Transient,SDLDrv)
	DECLARE_WITHIN(USDLClient)

	// Variables.
    SDL_Surface *		ScreenSurface;
	DWORD				BlitFlags;

    UBOOL				LostGrab;
    UBOOL				LostFullscreen;

    INT					SavedCursorX;
    INT					SavedCursorY;

	// SDL Keysym to EInputKey map.
	BYTE				KeysymMap[512];

	// KeyRepeatKey.
    INT					KeyRepeatKey;
    TCHAR				KeyRepeatUnicode;
	FTime				RepeatTimer;

	// Joystick Hat Hack
	EInputKey 			LastJoyHat;
	UBOOL				LastKey;

	UBOOL				MouseIsGrabbed;
	INT					TextToSpeechObject;  // file descriptor for named pipe.

	#if MACOSX
    UBOOL				MacTextToSpeechEnabled;
    SpeechChannel		MacTextToSpeechChannel;
    TArrayNoInit<FString> SpeechQueue;
	#endif

	// Constructor.
	USDLViewport();

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
	void* GetWindow();
	void SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL FocusOnly );

	// USDLViewport interface.
	void UpdateMouseGrabState();
	void ToggleFullscreen();
	void EndFullscreen();
	void SetTopness();
	DWORD GetViewportButtonFlags( DWORD wParam );

	UBOOL CauseInputEvent( INT iKey, EInputAction Action, FLOAT Delta=0.0 );

	virtual UBOOL TryRenderDevice( const TCHAR* ClassName, INT NewX, INT NewY, UBOOL Fullscreen );
    void SetTitleBar();

	TCHAR* GetLocalizedKeyName( EInputKey Key );
	void TextToSpeech( const FString& Text, FLOAT Volume );
	void UpdateSpeech();  // call once a frame from UpdateInput().
};


#define AUTO_INITIALIZE_REGISTRANTS_SDLDRV \
	USDLViewport::StaticClass(); \
	USDLClient::StaticClass();

#endif //_INC_SDLDRV
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

