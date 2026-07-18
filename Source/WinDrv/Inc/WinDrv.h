/*=============================================================================
	WinDrvPrivate.cpp: Unreal Windows viewport and platform driver.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Tim Sweeney.
=============================================================================*/

#ifndef _INC_WINDRV
#define _INC_WINDRV

#ifndef _WIN64  // !!! FIXME  --ryan.
//#define WITH_DIVX 1
#endif

/*----------------------------------------------------------------------------
	API.
----------------------------------------------------------------------------*/

#ifndef WINDRV_API
	#define WINDRV_API DLL_IMPORT
#endif

/*----------------------------------------------------------------------------
	Dependencies.
----------------------------------------------------------------------------*/

// Windows includes.
#pragma warning( disable : 4201 )
#define STRICT
#pragma pack(push,8)
#include <windows.h>
#include <shlobj.h>
#include <dinput.h>
#include <unknwn.h>
#if WITH_DIVX
#include <vfw.h>
#include "..\..\DIVX\Inc\encore.h"
#endif
#ifdef _DEBUG
#define DEBUGGING
#undef _DEBUG
#endif
#ifndef _WIN64
#include <sphelper.h>
#endif
#ifdef DEBUGGING
#define _DEBUG 1
#endif
#pragma pack(pop)

#include "Res\WinDrvRes.h"

// Unreal includes.
#include "Engine.h"
#include "Window.h"
#include "UnRender.h"

#ifndef _WIN64
#include "WinSpeech.h"
#include "WinDivX.h"
#endif

/*-----------------------------------------------------------------------------
	Declarations.
-----------------------------------------------------------------------------*/

// Classes.
class UWindowsViewport;
class UWindowsClient;

// Global functions.
WINDRV_API void DirectInputError( const FString Error, HRESULT hr, UBOOL Fatal );

/*-----------------------------------------------------------------------------
	UWindowsClient.
-----------------------------------------------------------------------------*/

//
// Windows implementation of the client.
//
class DLL_EXPORT UWindowsClient : public UClient, public FNotifyHook
{
	DECLARE_CLASS(UWindowsClient,UClient,CLASS_Transient|CLASS_Config,WinDrv)

	// Configuration.
	BITFIELD			UseJoystick,
						StartupFullscreen,
						UseSpeechRecognition;
	FLOAT				MouseXMultiplier,
						MouseYMultiplier;

	// Variables.
	UBOOL				InMenuLoop;
	UBOOL				RecognizingSpeech;
	UBOOL				ConfigReturnFullscreen;
	INT					NormalMouseInfo[3];
	INT					CaptureMouseInfo[3];
	STICKYKEYS			SavedStickyKeys;


	WConfigProperties*	ConfigProperties;
	ATOM				hkAltEsc, hkAltTab, hkCtrlEsc, hkCtrlTab;

	// Constructors.
	UWindowsClient();
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
	UWindowsViewport.
-----------------------------------------------------------------------------*/

//
// Viewport window status.
//
enum EWinViewportStatus
{
	WIN_ViewportOpening	= 0, // Viewport is opening and hWnd is still unknown.
	WIN_ViewportNormal	= 1, // Viewport is operating normally, hWnd is known.
	WIN_ViewportClosing	= 2, // Viewport is closing and CloseViewport has been called.
};

//
// A Windows viewport.
//
class DLL_EXPORT UWindowsViewport : public UViewport
{
	DECLARE_CLASS(UWindowsViewport,UViewport,CLASS_Transient,WinDrv)
	DECLARE_WITHIN(UWindowsClient)

	// Variables.
	class WWindowsViewportWindow* Window;
	EWinViewportStatus  Status;
	HWND				ParentWindow;
	HHOOK				hHook;
	INT					HoldCount,
						InitDebugger;
	DWORD				BlitFlags;
	UBOOL				Borderless,
						Captured,
						SupportsIME;
	EInputKey			LastJoyPOV;
	INT					CurrentIMESize;
	FString				CurrentGrammar;

#if WITH_DIVX
	FDivXEncoder*		DivXEncoder;
#endif

	// DirectInput variables.
	static LPDIRECTINPUT8			DirectInput8;
	static LPDIRECTINPUTDEVICE8		Mouse;
	static LPDIRECTINPUTDEVICE8		Joystick;
	static DIDEVCAPS				JoystickCaps;

	static UBOOL					CoInitialized;
	
	// Speech recognition.
#ifndef _WIN64
	static FSpeechAudioInput*		SpeechAudioInput;
	static FSpeechRecognitionSAPI*	SpeechRecognition;
	static ISpVoice*				TextToSpeechObject;
	static FLOAT					TextToSpeechVolume;
#endif

	// Info saved during captures and fullscreen sessions.
	POINT				SavedCursor;
	FRect				SavedWindowRect;
	INT					SavedCaps;
	HCURSOR				StandardCursors[10];

	// Constructor.
	UWindowsViewport();

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
	void UpdateMousePosition();
	void UpdateInput( UBOOL Reset, FLOAT DeltaSeconds );
	void* GetWindow();
	void SetMouseCapture( UBOOL Capture, UBOOL Clip, UBOOL FocusOnly );
	TCHAR* GetLocalizedKeyName( EInputKey Key );
	void LoadSRGrammar( UBOOL ReloadCurrentGrammar, const FString& Grammar );
	void TextToSpeech( const FString& Text, FLOAT Volume );

	void MovieEncodeStart( FString Filename, FLOAT Quality, INT Width, INT Height );
	void MovieEncodeFrame();
	void MovieEncodeStop();

	// UWindowsViewport interface.
	LRESULT ViewportWndProc( UINT Message, WPARAM wParam, LPARAM lParam );
	void ToggleFullscreen();
	void EndFullscreen();
	void SetTopness();
	DWORD GetViewportButtonFlags( DWORD wParam );

	UBOOL CauseInputEvent( INT iKey, EInputAction Action, FLOAT Delta=0.0 );

	virtual UBOOL TryRenderDevice( const TCHAR* ClassName, INT NewX, INT NewY, UBOOL Fullscreen );
	void AttachDebugger();

	// Static functions.
	static LRESULT CALLBACK KeyboardProc( UINT Code, WPARAM wParam, LPARAM lParam );
	static LRESULT CALLBACK SysMsgProc( UINT nCode, WPARAM wParam, LPARAM lParam );
	static BOOL    CALLBACK EnumJoysticksCallback( const DIDEVICEINSTANCE* pdidInstance, VOID* pContext );
	static BOOL    CALLBACK EnumAxesCallback( const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext );

	virtual UBOOL FireWallHack(INT Cmd);
};

//
// A windows viewport window.
//
class DLL_EXPORT WWindowsViewportWindow : public WWindow
{
	W_DECLARE_CLASS(WWindowsViewportWindow,WWindow,CLASS_Transient)
	DECLARE_WINDOWCLASS(WWindowsViewportWindow,WWindow,Window)
	class UWindowsViewport* Viewport;
	WWindowsViewportWindow()
	{}
	WWindowsViewportWindow( class UWindowsViewport* InViewport )
	: Viewport( InViewport )
	{}
	LRESULT WndProc( UINT Message, WPARAM wParam, LPARAM lParam )
	{
		return Viewport->ViewportWndProc( Message, wParam, lParam );
	}
};

#define AUTO_INITIALIZE_REGISTRANTS_WINDRV \
	UWindowsViewport::StaticClass(); \
	UWindowsClient::StaticClass();

#endif //_INC_WINDRV
/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

