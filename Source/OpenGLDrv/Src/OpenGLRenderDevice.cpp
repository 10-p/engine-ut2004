/*=============================================================================
	OpenGLRenderDevice.cpp: Unreal OpenGL support.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.
	
	Revision history:
	* Created by Daniel Vogel

=============================================================================*/

#include "OpenGLDrv.h"

/*-----------------------------------------------------------------------------
	Constants & static stuff.
-----------------------------------------------------------------------------*/

// !!! FIXME: This should get rid of all these win32 vs. SDL ifdefs. --ryan.
// The switch is who owns the GL context: SDL2Drv on every build but an MSVC one, which creates its own
// through WGL (UE_OPENGLDRV_WGL).
#ifndef UE_OPENGLDRV_WGL
#  define USE_SDL 1
#endif

#if (defined UE_OPENGLDRV_WGL)
#   define GL_DLL (TEXT("OpenGL32.dll"))
#elif defined(_WIN32)
#   define GL_DLL (TEXT("OpenGL32.dll"))	// SDL owns the context; the entry points still come from OpenGL32
#elif ((__linux__) || (__FreeBSD__))
#   define GL_DLL (TEXT("libGL.so.1"))
#elif MACOSX
#   define GL_DLL (TEXT("libGL.dylib"))   // !!! FIXME: probably wrong.
#else
#   error Fill this in for your platform.
#endif

// Static variables.
INT				UOpenGLRenderDevice::NumDevices		= 0;
INT				UOpenGLRenderDevice::LockCount		= 0;
BYTE*			UOpenGLRenderDevice::VARPointer		= 0;
INT				UOpenGLRenderDevice::VARIndex		= 0;
INT				UOpenGLRenderDevice::VARSize		= 0;

#ifdef UE_OPENGLDRV_WGL
HGLRC			UOpenGLRenderDevice::hCurrentRC		= NULL;
HMODULE			UOpenGLRenderDevice::hModuleGLMain	= NULL;
HMODULE			UOpenGLRenderDevice::hModuleGLGDI	= NULL;
#else
UBOOL			UOpenGLRenderDevice::GLLoaded		= 0;
#endif

// OpenGL function pointers.
#define GL_EXT(name) UBOOL UOpenGLRenderDevice::SUPPORTS##name=0;
#define GL_PROC(ext,ret,func,parms) ret (STDCALL *UOpenGLRenderDevice::func)parms;
#include "OpenGLFuncs.h"
#undef GL_EXT
#undef GL_PROC

/*-----------------------------------------------------------------------------
	OpenGLRenderDevice.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UOpenGLRenderDevice);

//
// UOpenGLRenderDevice::UOpenGLRenderDevice
//
UOpenGLRenderDevice::UOpenGLRenderDevice() :
	RenderInterface(this)
{
	guard(UOpenGLRenderDevice::UOpenGLRenderDevice);
	
	LockedViewport		= NULL;
	IsVAR				= 0;
	ValidContext		= 0;
	WasFullscreen		= 0;

	unguard;
}

//
// UOpenGLRenderDevice::StaticConstructor
//
void UOpenGLRenderDevice::StaticConstructor()
{
	guard(UOpenGLRenderDevice::StaticConstructor);

	new(GetClass(),TEXT("DetailTexMipBias"),    RF_Public)UFloatProperty( CPP_PROPERTY(DetailTexMipBias	   ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("MaxTextureUnits"),     RF_Public)UIntProperty  ( CPP_PROPERTY(MaxTextureUnits	   ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("AppleVA"),             RF_Public)UIntProperty  ( CPP_PROPERTY(AppleVA       	   ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("VARSize"),             RF_Public)UIntProperty  ( CPP_PROPERTY(ConfigVARSize	   ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("UseTrilinear"),        RF_Public)UBoolProperty ( CPP_PROPERTY(UseTrilinear        ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("UseVBO"),				RF_Public)UBoolProperty ( CPP_PROPERTY(UseVBO		       ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("UseVSync"),			RF_Public)UBoolProperty ( CPP_PROPERTY(UseVSync		       ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("ReduceMouseLag"),      RF_Public)UBoolProperty ( CPP_PROPERTY(ReduceMouseLag      ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("ForceCompression"),    RF_Public)UBoolProperty ( CPP_PROPERTY(ForceCompression    ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("DesiredRefreshRate"),	RF_Public)UIntProperty  ( CPP_PROPERTY(DesiredRefreshRate  ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("NumAASamples"),		RF_Public)UIntProperty  ( CPP_PROPERTY(NumAASamples        ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("MultisampleHint"),		RF_Public)UIntProperty  ( CPP_PROPERTY(MultisampleHint     ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("LevelOfAnisotropy"),	RF_Public)UFloatProperty( CPP_PROPERTY(LevelOfAnisotropy   ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("UseRenderTargets"),	RF_Public)UBoolProperty	( CPP_PROPERTY(UseRenderTargets    ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("UsePixelShaders"),		RF_Public)UBoolProperty ( CPP_PROPERTY(UsePixelShaders     ), TEXT("Options"), CPF_Config );
	// UseHardwareGamma: an ini option of the driver, assigned below (off).
	new(GetClass(),TEXT("UseHardwareGamma"),	RF_Public)UBoolProperty ( CPP_PROPERTY(UseHardwareGamma    ), TEXT("Options"), CPF_Config );

	GIsOpenGL						= 1;
	GIsPixomatic					= 0;

	// Compiled defaults, for an ini without a [OpenGLDrv.OpenGLRenderDevice] section (a retail install leaves
	// every CPF_Config option at zero); the ini wins wherever it sets one. DetailTextures on, or the
	// HandleDetail gate is never reached and detail-texture layers vanish.
	DetailTextures					= 1;
	UseVBO							= 1;
	MaxTextureUnits					= 8;		// clamped to the real GL limit at init
	UseTrilinear					= 1;
	LevelOfAnisotropy				= 16.f;		// clamped to the GPU's maximum at init
	ConfigVARSize					= 32;
	UseRenderTargets				= 1;
	DetailTexMipBias				= 0.f;
	AppleVA							= 1;
	MultisampleHint					= 2;
	NumAASamples					= 4;

	// Not raised: UsePixelShaders (this device draws through the fixed-function path), ForceCompression
	// (DXT on already-decoded textures loses quality), UseVSync and ReduceMouseLag (both cost frames).
	UsePixelShaders					= 0;
	ForceCompression				= 0;
	UseVSync						= 0;
	ReduceMouseLag					= 0;
	DesiredRefreshRate				= 0;

	// Off by default: UpdateGamma reprograms the display's ramp, not the game's, so it recolours the
	// whole desktop, and a process that dies before RestoreGamma leaves it that way. Set
	// UseHardwareGamma=True in [OpenGLDrv.OpenGLRenderDevice] for the in-game Brightness/Gamma/Contrast.
	UseHardwareGamma				= 0;

	SupportsCubemaps				= 1;
	SupportsZBIAS					= 1;
	SupportsRenderToTextureRGBA8888	= 0;
	SupportsRenderToTextureRGB565	= 0;
	SupportsRenderToTextureNPOT		= 0;

	unguard;
}


// The "Device supports" list is logged at the first SetRes of the process, at a later one only with -UFRONTLOG.
static UBOOL OpenGLExtensionsLogged = 0;

//
// UOpenGLRenderDevice::FindExt
//
UBOOL UOpenGLRenderDevice::FindExt( const TCHAR* Name )
{
	guard(UOpenGLRenderDevice::FindExt);
	
	// Can't use appStrFind as extension string might be > 1024 characters.
	UBOOL Result = strstr( (char*) glGetString(GL_EXTENSIONS), appToAnsi(Name) ) != NULL;
	if( Result && (!OpenGLExtensionsLogged || GUfrontLog) )
		debugf( TEXT("OpenGL: Device supports: %s"), Name );
	return Result;
	
	unguard;
}


//
// UOpenGLRenderDevice::FindProc
//
void UOpenGLRenderDevice::FindProc( void*& ProcAddress, char* Name, char* SupportName, UBOOL& Supports, UBOOL AllowExt )
{
	guard(UOpenGLRenderDevice::FindProc);

#if (defined USE_SDL)
	if( !ProcAddress )
		ProcAddress = (void*) SDL_GL_GetProcAddress( Name );
#elif (defined UE_OPENGLDRV_WGL)
	if( !ProcAddress )
		ProcAddress = appGetDllExport( hModuleGLMain, appFromAnsi(Name) );
	if( !ProcAddress )
		ProcAddress = appGetDllExport( hModuleGLGDI, appFromAnsi(Name) );
	if( !ProcAddress && AllowExt && Supports )
		ProcAddress = wglGetProcAddress( Name );
	if( !ProcAddress && AllowExt && Supports )
	{
		char Dummy[1024];
		strcpy( Dummy, Name );
		strcat( Dummy, "EXT" );
		ProcAddress = wglGetProcAddress( Dummy );
	}
#else
#   error Please handle this for your platform.
#endif

	if( !ProcAddress )
	{
		if( Supports )
			debugf( TEXT("OpenGL: Missing function '%s' for '%s' support"), appFromAnsi(Name), appFromAnsi(SupportName) );
		Supports = 0;
	}

	unguard;
}


//
// UOpenGLRenderDevice::FindProcs
//
void UOpenGLRenderDevice::FindProcs( UBOOL AllowExt )
{
	guard(UOpenGLDriver::FindProcs);

	#define GL_EXT(name) if( AllowExt ) SUPPORTS##name = FindExt( TEXT(#name)+1 );
	#define GL_PROC(ext,ret,func,parms) FindProc( *(void**)&func, #func, #ext, SUPPORTS##ext, AllowExt );
	#include "OpenGLFuncs.h"
	#undef GL_EXT
	#undef GL_PROC

	unguard;
}


//
// UOpenGLRenderDevice::MakeCurrent
//
void UOpenGLRenderDevice::MakeCurrent()
{
	guard(UOpenGLRenderDevice::MakeCurrent);

	check(ValidContext);
#ifdef UE_OPENGLDRV_WGL
	check(hRC);
	check(hDC);
	if( hCurrentRC!=hRC )
	{
		verify(wglMakeCurrent(hDC,hRC));
		hCurrentRC = hRC;
	}
#endif

	unguard;
}


//
// UOpenGLRenderDevice::GLError
//
void UOpenGLRenderDevice::GLError( TCHAR* Tag )
{
	guard(UOpenGLRenderDevice::GLError);

	GLenum Error;
	while( (Error=glGetError()) != GL_NO_ERROR )
	{
		const TCHAR* Msg;
		switch( Error )
		{
			case GL_INVALID_ENUM:
				Msg = TEXT("GL_INVALID_ENUM");
				break;
			case GL_INVALID_VALUE:
				Msg = TEXT("GL_INVALID_VALUE");
				break;
			case GL_INVALID_OPERATION:
				Msg = TEXT("GL_INVALID_OPERATION");
				break;
			case GL_STACK_OVERFLOW:
				Msg = TEXT("GL_STACK_OVERFLOW");
				break;
			case GL_STACK_UNDERFLOW:
				Msg = TEXT("GL_STACK_UNDERFLOW");
				break;
			case GL_OUT_OF_MEMORY:
				Msg = TEXT("GL_OUT_OF_MEMORY");
				break;
			default :
				Msg = TEXT("UNKNOWN");
		};
		debugf( TEXT("OpenGL Error: %s (%s)"), Msg, Tag );
	}

	unguard;
}


//
// UOpenGLRenderDevice::Init
//
UBOOL UOpenGLRenderDevice::Init()
{
	guard(UOpenGLRenderDevice::Init);

	debugf( TEXT("Initializing OpenGLDrv...") );

	// NumAASamples replaces the MultisampleBuffers/MultisampleSamples pair: an ini without the new key
	// is read through the old pair once (MSAA on when buffers > 0 and samples > 1).
	{
		const TCHAR* Section = TEXT("OpenGLDrv.OpenGLRenderDevice");
		INT NewKey = 0, OldBuffers = 1, OldSamples = 4;
		if( !GConfig->GetInt( Section, TEXT("NumAASamples"), NewKey ) )
		{
			UBOOL HaveOld = GConfig->GetInt( Section, TEXT("MultisampleBuffers"), OldBuffers );
			HaveOld = GConfig->GetInt( Section, TEXT("MultisampleSamples"), OldSamples ) || HaveOld;
			if( HaveOld )
			{
				NumAASamples = ( OldBuffers > 0 && OldSamples > 1 ) ? OldSamples : 0;
				debugf( TEXT("OpenGL: NumAASamples=%i from the old MultisampleBuffers=%i MultisampleSamples=%i"), NumAASamples, OldBuffers, OldSamples );
			}
		}
	}

	GIsOpenGL						= 1;
	GIsPixomatic					= 0;

	SupportsCubemaps				= 1;
	SupportsZBIAS					= 1;
	SupportsRenderToTextureRGBA8888	= 0;
	SupportsRenderToTextureRGB565	= 0;
	SupportsRenderToTextureNPOT		= 0;

#if (defined UE_OPENGLDRV_WGL)

	// Get list of device modes.
	for( INT i=0; ; i++ )
	{
#if UNICODE
		if( !GUnicodeOS )
		{
			DEVMODEA DisplayMode;
			appMemzero( &DisplayMode, sizeof(DisplayMode) );
			DisplayMode.dmSize = sizeof(DisplayMode);
			
			if( !EnumDisplaySettingsA( NULL, i, &DisplayMode) )
				break;
			
			Modes.AddUniqueItem( FPlane(	DisplayMode.dmPelsWidth, 
											DisplayMode.dmPelsHeight, 
											DisplayMode.dmBitsPerPel, 
											DisplayMode.dmDisplayFrequency
										));
		}
		else
#endif
		{
			DEVMODE DisplayMode;
			appMemzero( &DisplayMode, sizeof(DisplayMode) );
			DisplayMode.dmSize = sizeof(DisplayMode);
			
			if( !EnumDisplaySettings( NULL, i, &DisplayMode) )
				break;
			
			Modes.AddUniqueItem( FPlane(	DisplayMode.dmPelsWidth, 
											DisplayMode.dmPelsHeight, 
											DisplayMode.dmBitsPerPel, 
											DisplayMode.dmDisplayFrequency
										));
		}
	}

	// Init global GL.
	if( NumDevices==0 )
	{
		// Find DLL's. appGetDllHandle on Windows returns the "it is in the exe" sentinel, and the GL
		// entry points resolve from the exe's own import thunks.
		hModuleGLMain = (HMODULE) appGetDllHandle( GL_DLL );
		if( !hModuleGLMain )
		{
			debugf( TEXT("OpenGL: Couldn't locate %s"), GL_DLL );
			return 0;
		}
		hModuleGLGDI = (HMODULE) appGetDllHandle( TEXT("GDI32.dll") );
		check(hModuleGLGDI);

		// Find functions.
		SUPPORTS_GL = 1;
		FindProcs( 0 );
		if( !SUPPORTS_GL )
			appErrorf(TEXT("Missing symbols - aborting."));
	}

#elif (defined USE_SDL)
	// Init global GL.
	if( NumDevices==0 )
	{
		// Bind the library.
        FString OpenGLLibName = GL_DLL;

		if ( !GLLoaded )
		{
			// Only call it once as succeeding calls will 'fail'.
			debugf( TEXT("binding %s"), *OpenGLLibName );
			if ( SDL_GL_LoadLibrary( appToAnsi(*OpenGLLibName) ) == -1 )
				appErrorf( appFromAnsi(SDL_GetError()) );
			GLLoaded = true;
		}

		SUPPORTS_GL = 1;
		FindProcs( 0 );
		if( !SUPPORTS_GL )
			appErrorf(TEXT("Missing symbols - aborting."));
	}

#else
#   error Please handle this for your platform.
#endif

	// Scratch buffer used by e.g. resource management.
	ScratchBuffer.Empty();
	ScratchBuffer.Add( 65536 );

	NumDevices++;

	GGPUVendorID = 0xEEEE;
	GGPUDeviceID = 0;

	return 1;
	unguard;
}


//
// UOpenGLRenderDevice::Exit
//
void UOpenGLRenderDevice::Exit(UViewport* Viewport)
{
	guard(UOpenGLRenderDevice::Exit);

	check(NumDevices>0);

	// Shut down RC.
	Flush( Viewport );

	UnSetRes( 1 );

	RestoreGamma();

#ifdef UE_OPENGLDRV_WGL
	// Shut down this GL context. May fail if window was already destroyed.
	if( hDC )
		ReleaseDC(hWnd,hDC);

	// Shut down global GL.
	if( --NumDevices==0 )
	{
		// Free modules.
		if( hModuleGLMain )
			appFreeDllHandle( hModuleGLMain );
		if( hModuleGLGDI )
			appFreeDllHandle( hModuleGLGDI );
	}
#endif

	unguard;
}


//
// UOpenGLRenderDevice::SetRes
//
UBOOL UOpenGLRenderDevice::SetRes(UViewport* Viewport,INT NewX,INT NewY,UBOOL Fullscreen,INT ColorBytes,UBOOL bSaveSize)
{
	guard(UOpenGLRenderDevice::SetRes);

	// Returning 0 causes infinite loops and I'd rather catch those bugs at the root.
	if( !GIsEditor && LockedViewport )
		appErrorf(TEXT("Can't change resolution while render device is locked!"));

	debugf(TEXT("Enter SetRes: %dx%d Fullscreen %d"), NewX, NewY, Fullscreen );

	switch( ColorBytes )
	{
	case 0:
		break;
	case 2:
		Use16bit = 1;
		break;
	case 4:
		Use16bit = 0;
	}
	ColorBytes = Use16bit ? 2 : 4;

	Flush( Viewport );

	// Unset resolution.
	if( ValidContext )
		UnSetRes( 0 );

	// Hint is: 0 == GL_DONT_CARE, 1 == GL_FASTEST, 2 == GL_NICEST.  --ryan.
	MultisampleHint    = Max(MultisampleHint, 0);
	MultisampleHint    = Min(MultisampleHint, 2);
	NumAASamples       = Max(NumAASamples, 0);

#if (defined USE_SDL)
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, ColorBytes<=2 ? 5 : 8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, ColorBytes<=2 ? 5 : 8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, ColorBytes<=2 ? 5 : 8 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, ColorBytes<=2 ? 16 : 24 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, NumAASamples > 1 ? 1 : 0 );
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, NumAASamples > 1 ? NumAASamples : 0 );

	// Change window size.
	Viewport->ResizeViewport( Fullscreen ? (BLIT_Fullscreen|BLIT_OpenGL) : (BLIT_OpenGL), NewX, NewY );

#elif (defined UE_OPENGLDRV_WGL)

	// Get hWnd & hDC.
	hWnd		= (HWND) Viewport->GetWindow();
	check(hWnd);
	hDC			= GetDC( hWnd );
	check(hDC);

#if 0 
	// Print all PFD's exposed
	INT Count = DescribePixelFormat( hDC, 0, 0, NULL );
	for( i=1; i<Count; i++ )
		PrintFormat( hDC, i );
#endif

	// Change display settings.
	if( Fullscreen )
	{
		INT FindX		= NewX, 
			FindY		= NewY, 
			BestError	= MAXINT;
		
		for( INT i=0; i<Modes.Num(); i++ )
		{
			if( Modes(i).Z == ColorBytes*8 )
			{
				INT Error
				=	(Modes(i).X-FindX)*(Modes(i).X-FindX)
				+	(Modes(i).Y-FindY)*(Modes(i).Y-FindY);
				if( Error < BestError )
				{
					NewX		= Modes(i).X;
					NewY		= Modes(i).Y;
					BestError	= Error;
				}
			}
		}

		DWORD RefreshRate = DesiredRefreshRate ? Clamp<DWORD>( DesiredRefreshRate, 60, 100 ) : 0;

#if UNICODE
		if( !GUnicodeOS )
		{
			DEVMODEA DisplayMode;
			ZeroMemory( &DisplayMode, sizeof(DisplayMode) );
			DisplayMode.dmSize       = sizeof(DisplayMode);
			DisplayMode.dmPelsWidth  = NewX;
			DisplayMode.dmPelsHeight = NewY;
			DisplayMode.dmBitsPerPel = ColorBytes * 8;
#if 1
			if( RefreshRate )
			{
				DisplayMode.dmDisplayFrequency = RefreshRate;
				DisplayMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY | DM_BITSPERPEL;
			}
			else
#endif
			{
				DisplayMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
			}
			if( ChangeDisplaySettingsA( &DisplayMode, CDS_FULLSCREEN )!=DISP_CHANGE_SUCCESSFUL )
			{
				debugf( TEXT("ChangeDisplaySettingsA failed: %ix%ix%i"), NewX, NewY, ColorBytes );
				return 0;
			}
		}
		else
#endif
		{
			DEVMODE DisplayMode;
			ZeroMemory( &DisplayMode, sizeof(DisplayMode) );
			DisplayMode.dmSize       = sizeof(DisplayMode);
			DisplayMode.dmPelsWidth  = NewX;
			DisplayMode.dmPelsHeight = NewY;
			DisplayMode.dmBitsPerPel = ColorBytes * 8;
#if 1
			if ( RefreshRate )
			{
				DisplayMode.dmDisplayFrequency = RefreshRate;
				DisplayMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_DISPLAYFREQUENCY | DM_BITSPERPEL;
			}
			else
#endif
			{
				DisplayMode.dmFields = DM_PELSWIDTH | DM_PELSHEIGHT | DM_BITSPERPEL;
			}
			if( ChangeDisplaySettings( &DisplayMode, CDS_FULLSCREEN )!=DISP_CHANGE_SUCCESSFUL )
			{
				debugf( TEXT("ChangeDisplaySettings failed: %ix%i"), NewX, NewY );
				return 0;
			}
		}
	}

	// OpenGL renderer doesn't support stencil and can't run the Editor. 
	if( GIsEditor )
		appErrorf(NAME_FriendlyError, TEXT("The OpenGL renderer doesn't support running the Editor. Please switch to the D3D renderer if you want to use the Editor.") );

	UseStencil = 0;

	// Set resolution.
	INT DesiredColorBits   = ColorBytes<=2 ? 16 : 32;
	INT DesiredStencilBits = 0;//ColorBytes<=2 ?  0 : 8;
	INT DesiredDepthBits   = ColorBytes<=2 ? 16 : 24;
	
	PIXELFORMATDESCRIPTOR pfd =
	{
		sizeof(PIXELFORMATDESCRIPTOR),
		1,
		PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		PFD_TYPE_RGBA,
		DesiredColorBits,
		0,0,0,0,0,0,
		0,0,
		0,0,0,0,0,
		DesiredDepthBits,
		DesiredStencilBits,
		0,
		PFD_MAIN_PLANE,
		0,
		0,0,0
	};
	
	INT nPixelFormat = ChoosePixelFormat( hDC, &pfd );
	Parse( appCmdLine(), TEXT("PIXELFORMAT="), nPixelFormat );
	check(nPixelFormat);

	debugf( TEXT("Using pixel format %i"), nPixelFormat );
	
	SetPixelFormat( hDC, nPixelFormat, &pfd );
	
	hRC = wglCreateContext( hDC );
	check(hRC);

	// Change window size.
	UBOOL Result = Viewport->ResizeViewport( (Fullscreen ? BLIT_Fullscreen : 0) | BLIT_OpenGL, NewX, NewY );
	if( !Result )
	{
		if( Fullscreen )
			TCHAR_CALL_OS(ChangeDisplaySettings(NULL,0),ChangeDisplaySettingsA(NULL,0));
		return 0;
	}

#else
#   error Please handle this for your platform.
#endif

	ValidContext = 1;
	MakeCurrent();

	debugf( TEXT("GL_VENDOR     : %s"), appFromAnsi((ANSICHAR*)glGetString(GL_VENDOR))	);
	debugf( TEXT("GL_RENDERER   : %s"), appFromAnsi((ANSICHAR*)glGetString(GL_RENDERER)) );
	debugf( TEXT("GL_VERSION    : %s"), appFromAnsi((ANSICHAR*)glGetString(GL_VERSION))	);

	FindProcs( 1 );
	OpenGLExtensionsLogged = 1;

	if(	appStrstr(appFromAnsi((ANSICHAR*)glGetString(GL_RENDERER)),TEXT("Radeon 7000"				)) != NULL
	||	appStrstr(appFromAnsi((ANSICHAR*)glGetString(GL_RENDERER)),TEXT("Radeon 7200"				)) != NULL
	||	appStrstr(appFromAnsi((ANSICHAR*)glGetString(GL_RENDERER)),TEXT("Radeon 7500"				)) != NULL
	||	appStrstr(appFromAnsi((ANSICHAR*)glGetString(GL_RENDERER)),TEXT("Radeon VE"					)) != NULL
	||	appStrstr(appFromAnsi((ANSICHAR*)glGetString(GL_RENDERER)),TEXT("ATI Radeon OpenGL Engine"	)) != NULL
	)
		IsR100 = 1;
	else
		IsR100 = 0;

	ConfigVARSize = Max(ConfigVARSize, 0);

#if MACOSX
	// John Stauffer at Apple says:
	//   "Just for your info, the actual max single VAR allocation size is
	//    restricted to 3/4 the allowable GART size.  Max single VAR
	//    allocation = 0.75 * 0.25 * (system mem size)"
	if (GPhysicalMemory == 0)
		debugf( TEXT("OpenGL: Don't know how much RAM you've got to clamp VARsize!") );
	else
	{
		INT MaxVARSize = (INT) (((0.75f * 0.25f * GPhysicalMemory) / 1024.0f) / 1024.0f);
		if (ConfigVARSize > MaxVARSize)
		{
			debugf( TEXT("OpenGL: Clamping VARsize to %d (was %d): not enough physical RAM."),
			        (int) MaxVARSize, (int) ConfigVARSize );
			ConfigVARSize = MaxVARSize;
		}
	}
#endif

	appSprintf( GMachineVideo, TEXT("%s"), appFromAnsi((ANSICHAR*)glGetString(GL_RENDERER)) );

	INT DepthBits,
		StencilBits,
		RedBits,
		GreenBits,
		BlueBits,
		AlphaBits;

	glGetIntegerv( GL_DEPTH_BITS,	&DepthBits		);
	glGetIntegerv( GL_STENCIL_BITS,	&StencilBits	);
	glGetIntegerv( GL_RED_BITS,		&RedBits		);
	glGetIntegerv( GL_GREEN_BITS,	&GreenBits		);
	glGetIntegerv( GL_BLUE_BITS,	&BlueBits		);
	glGetIntegerv( GL_ALPHA_BITS,	&AlphaBits		 );

	debugf( TEXT("OpenGL: C%i RGB%i%i%i Z%i S%i"), ColorBytes * 8, RedBits, GreenBits, BlueBits, DepthBits, StencilBits );

	if( SUPPORTS_GL_EXT_texture_filter_anisotropic )
	{
		GLfloat MaxAnisotropy;
		glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &MaxAnisotropy );
		LevelOfAnisotropy = Max<FLOAT>( 1.0f, Min<FLOAT>( MaxAnisotropy, LevelOfAnisotropy ) );
		debugf( TEXT("OpenGL: Level of anisotropy is %f (max %f)."), (float) LevelOfAnisotropy, (float) MaxAnisotropy );
		// Say which gamma path won: the option is invisible when it works.
		if( GUfrontLog )
			debugf( TEXT("OpenGL: UseHardwareGamma=%s (%s) -- the display's gamma ramp is %s"),
				UseHardwareGamma ? TEXT("True") : TEXT("False"),
				ParseParam(appCmdLine(),TEXT("NOGAMMA")) ? TEXT("-NOGAMMA forces off") : TEXT("[OpenGLDrv.OpenGLRenderDevice]"),
				(UseHardwareGamma && !ParseParam(appCmdLine(),TEXT("NOGAMMA"))) ? TEXT("OURS to set and restore") : TEXT("left alone") );
	}

#ifndef UE_OPENGLDRV_WGL
	if( !SUPPORTS_GL_ARB_multisample )
		debugf( TEXT("OpenGL: Not using multisample") );
	else
	{
        INT Buffers, Samples;
		glGetIntegerv( GL_SAMPLE_BUFFERS_ARB, &Buffers );
		glGetIntegerv( GL_SAMPLES_ARB, &Samples );

		debugf( TEXT("OpenGL: Have %d multisamples buffer%s, %d sample%s."),
                (int) Buffers, Buffers == 1 ? TEXT("") : TEXT("s"),
                (int) Samples, Samples == 1 ? TEXT("") : TEXT("s") );

		if( !Buffers || !Samples )
		{
			debugf( TEXT("OpenGL: Failed to get a multisample GL context") );
			SUPPORTS_GL_ARB_multisample = 0;
			SUPPORTS_GL_NV_multisample_filter_hint = 0;
		}
		else
        {
			debugf( TEXT("OpenGL: Using multisample (%d buffers, %d samples)"),
			        (int) Buffers, (int) Samples );

			glEnable( GL_MULTISAMPLE_ARB );

			if( SUPPORTS_GL_NV_multisample_filter_hint )
			{
				TCHAR *HintString[] = { TEXT("DONT_CARE"), TEXT("FASTEST"), TEXT("NICEST") };
				GLenum HintEnum[] = { GL_DONT_CARE, GL_FASTEST, GL_NICEST };
				debugf( TEXT("OpenGL: Using multisample hint: GL_%s"), HintString[MultisampleHint] );
				glHint( GL_MULTISAMPLE_FILTER_HINT_NV, HintEnum[MultisampleHint] );
			}
		}
	}
#else
	SUPPORTS_GL_ARB_multisample				= 0;
	SUPPORTS_GL_NV_multisample_filter_hint	= 0;
#endif

    // !!! FIXME: fragment_program uses the same entry points as vertex_program, so if one is missing we turn off the other.
    if (!SUPPORTS_GL_ARB_fragment_program)
        SUPPORTS_GL_ARB_vertex_program = 0;

    if (SUPPORTS_GL_ARB_fragment_program)
    {
        if (UsePixelShaders)
            debugf(TEXT("OpenGL: Using GL_ARB_fragment_program for pixel shaders."));
        else
        {
            SUPPORTS_GL_ARB_fragment_program = 0;
            debugf(TEXT("OpenGL: Forcibly disabled pixel shaders."));
        }
    }

	// Check for old extension.
	SUPPORTS_GL_ATI_texture_env_combine3 |= SUPPORTS_GL_ATIX_texture_env_combine3;
	
	//!!TODO: it actually is faster to not use it and live with the implied memcpy.
	SUPPORTS_GL_ATI_map_object_buffer = 0;

	// NVIDIA doesn't expose the crossbar extension but supports it.
	SUPPORTS_GL_ARB_texture_env_crossbar |= SUPPORTS_GL_NV_texture_env_combine4;

	// Use ARB_vbo where available.
	if( SUPPORTS_GL_ARB_vertex_buffer_object && UseVBO )
	{
		SUPPORTS_GL_ATI_element_array			= 0;
		SUPPORTS_GL_ATI_vertex_array_object		= 0;
		SUPPORTS_GL_APPLE_vertex_array_range	= 0;
		SUPPORTS_GL_APPLE_vertex_array_object	= 0;
		SUPPORTS_GL_NV_vertex_array_range		= 0;
	}
	else
		SUPPORTS_GL_ARB_vertex_buffer_object = 0;

	// Only enable VAR if VAR2 is exposed as well.
	SUPPORTS_GL_NV_vertex_array_range &= SUPPORTS_GL_NV_vertex_array_range2;

	// Disable usage if VAR size <= 0.
	if( ConfigVARSize == 0 )
	{
		SUPPORTS_GL_NV_vertex_array_range		= 0;
		SUPPORTS_GL_APPLE_vertex_array_range	= 0;
	}

	// Switching between different implementations on Mac.
	switch( AppleVA )
	{
	case 0:
	default:
		// Use standard vertex arrays.
		SUPPORTS_GL_APPLE_vertex_array_range	= 0;
		SUPPORTS_GL_APPLE_vertex_array_object	= 0;
		break;
	case 1:
		// Use APPLE_VAR
		SUPPORTS_GL_APPLE_vertex_array_object	= 0;
		if( SUPPORTS_GL_APPLE_vertex_array_range )
			debugf( TEXT("OpenGL: Using GL_APPLE_vertex_array_range.") );
		break;
	case 2:
		// Use APPLE_VAR + APPLE_VAO
		SUPPORTS_GL_APPLE_vertex_array_range	= 0;
		if( SUPPORTS_GL_APPLE_vertex_array_object )
			debugf( TEXT("OpenGL: Using GL_APPLE_vertex_array_object.") );
		break;
	}

	// ATI Linux drivers claim to support tex compression, but there's no
	//  glCompressedTexSubImage2DARB entry point...turn off S3TC in this
	//  case, so it doesn't try to use a NULL function pointer.  --ryan.
	if (!SUPPORTS_GL_ARB_texture_compression)
		SUPPORTS_GL_EXT_texture_compression_s3tc = 0;

	// Warn about lack of S3TC support and disable compressed lightmaps.
	if( !SUPPORTS_GL_EXT_texture_compression_s3tc )
		debugf( TEXT("WARNING: OpenGL renderer relies on DXTC/S3TC support for good performance.") );
	UseCompressedLightmaps &= SUPPORTS_GL_EXT_texture_compression_s3tc;

	// Only force decompression + compression step if S3TC is not supported though generic compression is.
	ForceCompression &= SUPPORTS_GL_ARB_texture_compression && !SUPPORTS_GL_EXT_texture_compression_s3tc;

	//!!TODO: check for OpenGL version >= 1.3.

	// Complain about lack of combine3/4 extensions.
	if( !SUPPORTS_GL_ATI_texture_env_combine3 && !SUPPORTS_GL_NV_texture_env_combine4 )
		debugf( TEXT("WARNING: no support for combine3/4 extensions -> not all blend modes supported") );

	// Lame hack to ensure glDrawRangeElements is present.
	if( !SUPPORTS_GL_EXT_bgra )
		appErrorf(TEXT("Either GL_EXT_bgra or glDrawRangeElements not supported- bailing out."));

    if (!SUPPORTS_GL_EXT_framebuffer_object)
        debugf(TEXT("OpenGL: Render-to-Texture support unavailable."));
    else
    {
        if (UseRenderTargets)
            debugf(TEXT("OpenGL: Using GL_EXT_framebuffer_object for render-to-texture."));
        else
        {
            SUPPORTS_GL_EXT_framebuffer_object = 0;
            debugf(TEXT("OpenGL: Forcibly disabled render-to-texture."));
        }
    }
    SupportsRenderToTextureRGBA8888 = SUPPORTS_GL_EXT_framebuffer_object;
    SupportsRenderToTextureRGB565 = SUPPORTS_GL_EXT_framebuffer_object;
    SupportsRenderToTextureNPOT = SUPPORTS_GL_ARB_texture_non_power_of_two;

	// Disable vsync.
	if( SUPPORTS_WGL_EXT_swap_control )
		wglSwapIntervalEXT( !GIsBenchmarking && UseVSync ? 1 : 0 );

	// Set permanent state.
	glEnable( GL_DEPTH_TEST );
	glShadeModel( GL_SMOOTH );
	glDepthMask( GL_TRUE );
	glDepthFunc( GL_LEQUAL );
	glBlendFunc( GL_ONE, GL_ZERO );
	glEnable( GL_BLEND );
	glFogi( GL_FOG_MODE, GL_LINEAR );
	
	glEnable( GL_NORMALIZE );

	glEnable( GL_POLYGON_OFFSET_FILL );
	glDisable( GL_DITHER );

	glDisable( GL_CULL_FACE );
	glDisable( GL_LIGHTING );

	glEnableClientState( GL_VERTEX_ARRAY );

	// Get number of texture units and clamp according to .ini setting.
	if( MaxTextureUnits < 1 )
		MaxTextureUnits = 8;

	glGetIntegerv( GL_MAX_TEXTURE_UNITS_ARB, &NumTextureUnits );
	NumTextureUnits = Min( NumTextureUnits, Clamp( MaxTextureUnits, 2, 8 ) );

	for( INT i=0; i<NumTextureUnits; i++ )
	{
		glActiveTextureARB( GL_TEXTURE0 + i );
		glDisable( GL_TEXTURE_2D );
		glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_COMBINE_ARB );
	}
	glActiveTextureARB( GL_TEXTURE0 );
	glEnable( GL_TEXTURE_2D );

	FLOAT ColorOne[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
	FLOAT ColorZero[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	glMaterialfv( GL_FRONT_AND_BACK, GL_DIFFUSE , (GLfloat*) ColorOne  );
	glMaterialfv( GL_FRONT_AND_BACK, GL_AMBIENT , (GLfloat*) ColorOne  );
	glMaterialfv( GL_FRONT_AND_BACK, GL_SPECULAR, (GLfloat*) ColorZero );
	glMaterialfv( GL_FRONT_AND_BACK, GL_EMISSION, (GLfloat*) ColorZero );

	glColorMaterial( GL_FRONT_AND_BACK, GL_EMISSION );
	glEnable( GL_COLOR_MATERIAL );

	glLightModeli( GL_LIGHT_MODEL_LOCAL_VIEWER, GL_TRUE );
	
	for( INT i=0; i<8; i++ )
		glLightfv( GL_LIGHT0 + i, GL_SPECULAR, ColorZero );

	glViewport( 0, 0, NewX, NewY );

	glEnable( GL_SCISSOR_TEST );
	glScissor( 0, 0, NewX, NewY );

	appMemzero( HardwareState, sizeof( HardwareState) );
	
	// Create dummy white texture.
	FColor Black = FColor(0,0,0,0);
	glGenTextures( 1, &NullTextureID );
	glBindTexture( GL_TEXTURE_2D, NullTextureID );
	glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, &Black );

	// Vertex array range.
	VARPointer		= NULL;
	VARSize			= 0;
	VARIndex		= 0;
	if( SUPPORTS_GL_NV_vertex_array_range || SUPPORTS_GL_APPLE_vertex_array_range )
	{
		VARSize		= ConfigVARSize * 1024 * 1024;
		do
		{
#if MACOSX
			VARPointer	= new BYTE[ VARSize ];
#else
#ifdef UE_OPENGLDRV_WGL
			VARPointer	= (BYTE*) wglAllocateMemoryNV( VARSize, 0.f, 0.0f, 0.5f );
#else
			VARPointer	= (BYTE*) glXAllocateMemoryNV( VARSize, 0.f, 0.0f, 0.5f );
#endif
#endif
			if( VARPointer == NULL )
				VARSize = (INT) (((float) VARSize) / 1.5f);
		} 
		while( (VARPointer == NULL) && (VARSize > 8) );

		if( VARPointer == NULL )
		{	
#ifdef _DEBUG
			check(VARPointer);
#else
			debugf( TEXT("WARNING: Couldn't allocate AGP memory - turning off support for GL_NV_vertex_array_range"));
			debugf( TEXT("WARNING: This has a serious impact on performance."));
			SUPPORTS_GL_NV_vertex_array_range		= 0;
			SUPPORTS_GL_APPLE_vertex_array_range	= 0;
#endif
		}
		else
		{
#if MACOSX
			glVertexArrayParameteriAPPLE( GL_VERTEX_ARRAY_STORAGE_HINT_APPLE, 1 ? GL_STORAGE_SHARED_APPLE : GL_STORAGE_CACHED_APPLE );
#endif
			debugf( TEXT("OpenGL: allocated %i MByte of AGP memory"), VARSize / 1024 / 1024 );
		}
	}

	#if MACOSX
	if( SUPPORTS_GL_ATI_array_rev_comps_in_4_bytes )
	{
		debugf( TEXT("OpenGL: Using GL_ATI_array_rev_comps_in_4_bytes.") );
		glVertexArrayParameteriAPPLE( GL_ARRAY_REV_COMPS_IN_4_BYTES_ATI, GL_TRUE );

		// Panther (OSX 10.3.x) and beyond require more color byteswapping
		//  than Jaguar (OSX 10.2.x), so we need to decide which behaviour
		//  is applicable...  --ryan.
		const TCHAR *osCodename = TEXT("Jaguar");
		if (GMacOSVer >= 0x1030)
		{
			GIsOpenGLRevComps = 1;
			osCodename = TEXT("Panther");
		}
		debugf( TEXT("OpenGL: ATI_rev_comps has %s-style behaviour."), osCodename );
	}
	#endif

	SupportsCubemaps                = SUPPORTS_GL_ARB_texture_cube_map;
	SupportsZBIAS                   = 1;
	Viewport->PendingFrame			= 0;
	WasFullscreen                   = Fullscreen;

	UpdateGamma( Viewport );

	return 1;

	unguard;
}


//
// UOpenGLRenderDevice::UnSetRes
//
void UOpenGLRenderDevice::UnSetRes( UBOOL RestoreDisplaySettings )
{
	guard(UOpenGLRenderDevice::UnSetRes);

	if( ValidContext )
		glDeleteTextures( 1, &NullTextureID );

	// Vertex array range.
	if( (SUPPORTS_GL_NV_vertex_array_range || SUPPORTS_GL_APPLE_vertex_array_range) && VARPointer )
	{
#if MACOSX
		if( ValidContext )
			glVertexArrayRangeAPPLE( 0, 0 );
		delete [] VARPointer;
#else
#ifdef UE_OPENGLDRV_WGL
		wglFreeMemoryNV( VARPointer );
#else
		glXFreeMemoryNV( VARPointer );
#endif
#endif
	}
	VARPointer	= NULL;
	VARSize		= 0;
	VARIndex	= 0;

#ifdef UE_OPENGLDRV_WGL
	if( hDC	)
		ReleaseDC( hWnd, hDC );

	if( hRC )
	{
		hCurrentRC = NULL;
		verify(wglMakeCurrent( NULL, NULL ));
		verify(wglDeleteContext( hRC ));
		hRC = NULL;
	}
	if( WasFullscreen && RestoreDisplaySettings )
		TCHAR_CALL_OS(ChangeDisplaySettings(NULL,0),ChangeDisplaySettingsA(NULL,0));

#endif

	ValidContext = 0;

	unguard;
}


//
//	UOpenGLRenderDevice::GetCachedResource
//
FOpenGLResource* UOpenGLRenderDevice::GetCachedResource(QWORD CacheId)
{
	guard(UOpenGLRenderDevice::GetCachedResource);

	INT					HashIndex	= GetResourceHashIndex(CacheId);
	FOpenGLResource*	ResourcePtr = ResourceHash[HashIndex];

	while(ResourcePtr)
	{
		if(ResourcePtr->CacheId == CacheId)
			return ResourcePtr;

		ResourcePtr = ResourcePtr->HashNext;
	};

	return NULL;

	unguard;
}


//
//	UOpenGLRenderDevice::FlushResource
//
void UOpenGLRenderDevice::FlushResource(QWORD CacheId)
{
	guard(UOpenGLRenderDevice::GetCachedResource);

	FOpenGLResource*	CachedResource = GetCachedResource(CacheId);

	if(CachedResource)
		delete CachedResource;

	unguard;
}


//
//	UOpenGLRenderDevice::GetVertexShader
//
FOpenGLVertexShader* UOpenGLRenderDevice::GetVertexShader(EVertexShader Type,FShaderDeclaration& Declaration)
{
	guard(UOpenGLRenderDevice::GetVertexShader);

	// Find an existing vertex shader with the same type/declaration.
	FOpenGLVertexShader*	ShaderPtr = VertexShaders;

	while(ShaderPtr)
	{
		if(ShaderPtr->Type == Type && ShaderPtr->Declaration == Declaration)
			return ShaderPtr;

		ShaderPtr = ShaderPtr->NextVertexShader;
	};

	// Create a new vertex shader.
	if(Type == VS_FixedFunction)
		return new FOpenGLFixedVertexShader(this,Declaration);
	else
		return NULL;

	unguard;
}


//
// UOpenGLRenderDevice::ResourceCached
//
UBOOL UOpenGLRenderDevice::ResourceCached(QWORD CacheId)
{
	 FOpenGLResource*	Resource = GetCachedResource(CacheId);

	 if(!Resource)
		 return 0;

#if 0
	 FOpenGLTexture*	Texture = Resource->GetTexture();

	 if(!Texture)
		 return 1;

	 if(Texture->Direct3DTexture8 || Texture->Direct3DCubeTexture8)
		 return 1;
#else
	 return 1;
#endif

	 return 0;
}


//
// UOpenGLRenderDevice::Flush
//
void UOpenGLRenderDevice::Flush(UViewport* Viewport)
{
	guard(UOpenGLRenderDevice::Flush);

	DynamicVertexStream	= NULL;
	DynamicIndexBuffer	= NULL;

	guard(ResourceList);
	while(ResourceList)
		delete ResourceList;
	unguard;

	guard(VertexShaders);
	while(VertexShaders)
		delete VertexShaders;
	unguard;

	guard(PixelShaders);
	while(PixelShaders)
		delete PixelShaders;
	unguard;

	VARIndex = 0;

	PrecacheOnFlip = 1;//UsePrecaching;

	unguard;
}


//
// Hardware gamma is opt-in, through the driver's UseHardwareGamma ini option (off by default).
//
// This code sets the display's gamma, not the game's: SDL_SetWindowGammaRamp and SetDeviceGammaRamp both
// reprogram the monitor's ramp for the whole desktop, so running the game would change how everything
// else looks, and a process that dies without RestoreGamma would leave the desktop that way. The stock
// -NOGAMMA switch still forces it off. When it is enabled, RestoreGamma puts back the ramp found at
// startup instead of a linear one, so a calibrated display profile survives.
//

// The desktop's ramp as it was before we touched anything, captured on first use.
static _WORD GOriginalGammaRamp[3][256];
static UBOOL GOriginalGammaSaved = 0;

//
// UOpenGLRenderDevice::UpdateGamma
//
void UOpenGLRenderDevice::UpdateGamma(UViewport* Viewport)
{
	guard(UOpenGLRenderDevice::UpdateGamma);

	if( !UseHardwareGamma || ParseParam(appCmdLine(),TEXT("NOGAMMA")) )
		return;

	FLOAT	Gamma		= Viewport->GetOuterUClient()->Gamma,
			Brightness	= Viewport->GetOuterUClient()->Brightness,
			Contrast	= Viewport->GetOuterUClient()->Contrast;

	struct
	{
		_WORD red[256];
		_WORD green[256];
		_WORD blue[256];
	} Ramp;

	for(INT i=0; i<256; i++)
		Ramp.red[i] = Ramp.green[i] = Ramp.blue[i] = Clamp<INT>( appRound( (Contrast+0.5f)*appPow(i/255.f,1.0f/Gamma)*65535.f + (Brightness-0.5f)*32768.f - Contrast*32768.f + 16384.f ), 0, 65535 );

#if (defined USE_SDL)
	if( !GOriginalGammaSaved )
		GOriginalGammaSaved = ( SDL_GetWindowGammaRamp( (SDL_Window*)Viewport->GetWindow(), GOriginalGammaRamp[0], GOriginalGammaRamp[1], GOriginalGammaRamp[2] ) == 0 );
	SDL_SetWindowGammaRamp( (SDL_Window*)Viewport->GetWindow(), Ramp.red, Ramp.green, Ramp.blue );
#elif (defined UE_OPENGLDRV_WGL)
	if( !GOriginalGammaSaved )
		GOriginalGammaSaved = GetDeviceGammaRamp( hDC, GOriginalGammaRamp );
	SetDeviceGammaRamp( hDC, &Ramp );
#else
#   error Please handle this for your platform.
#endif

	unguard;
}


//
// UOpenGLRenderDevice::RestoreGamma
//
void UOpenGLRenderDevice::RestoreGamma()
{
	guard(UOpenGLRenderDevice::RestoreGamma);

	// Nothing was changed unless gamma was opted into and applied, so there is nothing to undo. Do not
	// restore unconditionally: writing a linear ramp to an untouched display wipes a calibrated profile.
	if( !UseHardwareGamma || !GOriginalGammaSaved )
		return;

#if (defined USE_SDL)
	SDL_Window* GammaWindow = SDL_GetKeyboardFocus();
	if( GammaWindow )
		SDL_SetWindowGammaRamp( GammaWindow, GOriginalGammaRamp[0], GOriginalGammaRamp[1], GOriginalGammaRamp[2] );
#elif (defined UE_OPENGLDRV_WGL)
	HDC DesktopDC = GetDC( GetDesktopWindow() );
	SetDeviceGammaRamp( DesktopDC, GOriginalGammaRamp );
	ReleaseDC( NULL, DesktopDC );
#else
#   error Please handle this for your platform.
#endif

	unguard;
}


//
// UOpenGLRenderDevice::Exec
//
UBOOL UOpenGLRenderDevice::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	guard(UOpenGLRenderDevice::Exec);
	
	if(ParseCommand(&Cmd,TEXT("DUMPRESOURCEHASH")))
	{
		for(INT HashIndex = 0;HashIndex < 4096;HashIndex++)
		{
			INT	ResourceCount = 0;

			for(FOpenGLResource*	Resource = ResourceHash[HashIndex];Resource;Resource = Resource->HashNext)
				ResourceCount++;

			debugf(TEXT("Resource hash bin\t%u: %u resources."),HashIndex,ResourceCount);
		}

		return 1;
	}
	if(ParseCommand(&Cmd,TEXT("DUMPHASHCOLLISIONS")))
	{
		INT	ResourceCount	= 0,
			HashCollisions	= 0;	

		for(INT HashIndex = 0;HashIndex < 4096;HashIndex++)
		{
			INT Count = 0;
			for(FOpenGLResource* Resource = ResourceHash[HashIndex];Resource;Resource = Resource->HashNext)
				Count++;
	
			ResourceCount  += Count;
			HashCollisions += Count ? Count - 1 : 0;
		}
	
		debugf(TEXT("OpenGL resource hash: %i resources, %i collisions"), ResourceCount, HashCollisions );

		return 1;
	}

	else if( ParseCommand(&Cmd,TEXT("RESOURCES") ))
	{
		// Calculate resource usage
		INT StatTextureBytes=0, StatVertexStreamBytes=0, StatIndexBufferBytes=0, StatOtherBytes=0;
		INT StatNumTextures=0, StatNumVertexStreams=0, StatNumIndexBuffers=0, StatNumOther=0;
		for( FOpenGLResource* Resource = ResourceList;Resource;Resource = Resource->NextResource )
		{
			if(Resource->GetTexture())
			{
				StatTextureBytes += Resource->CalculateFootprint();
				StatNumTextures++;
			}
			else if(Resource->GetVertexStream())
			{
				StatVertexStreamBytes += Resource->CalculateFootprint();
				StatNumVertexStreams++;
			}
			else if(Resource->GetIndexBuffer())
			{
				StatIndexBufferBytes += Resource->CalculateFootprint();
				StatNumIndexBuffers++;
			}
			else
			{
				StatOtherBytes += Resource->CalculateFootprint();
				StatNumOther++;
			}
		}

		Ar.Logf( TEXT("OpenGL Resource Usage"));
		Ar.Logf( TEXT(""));
		Ar.Logf( TEXT("Resource Type      Count   Total Bytes"));
		Ar.Logf( TEXT("--------------------------------------"));
		Ar.Logf( TEXT("Textures            %4d      %8d"), StatNumTextures, StatTextureBytes );
		Ar.Logf( TEXT("Vertex Streams      %4d      %8d"), StatNumVertexStreams, StatVertexStreamBytes );
		Ar.Logf( TEXT("Index Buffers       %4d      %8d"), StatNumIndexBuffers, StatIndexBufferBytes );
		Ar.Logf( TEXT("Other               %4d      %8d"), StatNumOther, StatOtherBytes );
		Ar.Logf( TEXT("") );

		return 1;
	}
	else if(ParseCommand(&Cmd,TEXT("NEARCLIP")))
	{
		if (appStrcmp(Cmd,TEXT("")) != 0)
			NEAR_CLIPPING_PLANE = appAtof(Cmd);
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SUPPORTEDRESOLUTION")) )
	{
		INT		Width		= 0,
				Height		= 0,
				BitDepth	= 0;
		UBOOL	Supported	= 0;

		if( Parse(Cmd,TEXT("WIDTH="),Width) && Parse(Cmd,TEXT("HEIGHT="),Height) && Parse(Cmd,TEXT("BITDEPTH="),BitDepth) )
		{
#if 0
			for(INT ModeIndex = 0;ModeIndex < DisplayModes.Num();ModeIndex++)
			{
				D3DDISPLAYMODE&	DisplayMode = DisplayModes(ModeIndex);

				if(DisplayMode.Width == Width && DisplayMode.Height == Height && GetFormatBPP(DisplayMode.Format) == BitDepth)
				{
					Supported = 1;
					break;
				}
			}

#elif USE_SDL
			// SDL2: window manager handles mode validation
			Supported = 1;

#else
			Supported = 1;
#endif		
		}

		Ar.Logf(TEXT("%u"),Supported);

		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("FSAA")) ) // FSAA crap by Ryan C. Gordon.
	{
		if( !SUPPORTS_GL_ARB_multisample )
		{
			debugf( TEXT("OpenGL: No FSAA support available") );
		}

		else if( ParseCommand(&Cmd,TEXT("ENABLE")) )
		{
			debugf( TEXT("OpenGL: Enabling GL_MULTISAMPLE_ARB...") );
			glEnable( GL_MULTISAMPLE_ARB );
		}

		else if( ParseCommand(&Cmd,TEXT("DISABLE")) )
		{
			debugf( TEXT("OpenGL: Disabling GL_MULTISAMPLE_ARB...") );
			glDisable( GL_MULTISAMPLE_ARB );
		}

		return 1;
    }

	else if( ParseCommand(&Cmd,TEXT("FSAAHINT")) )
	{
		if( !SUPPORTS_GL_NV_multisample_filter_hint )
		{
			debugf( TEXT("OpenGL: No FSAA hinting available") );
			return 1;
		}

		GLenum HintEnum;

		if( ParseCommand(&Cmd,TEXT("NICEST")) )
			HintEnum = GL_NICEST;

	    else if( ParseCommand(&Cmd,TEXT("FASTEST")) )
			HintEnum = GL_FASTEST;

	    else if( ParseCommand(&Cmd,TEXT("DONT_CARE")) )
			HintEnum = GL_DONT_CARE;

		else
		{
			debugf( TEXT("OpenGL: Unknown FSAA hinting option") );
			return 1;
		}

		debugf( TEXT("OpenGL: Changing multisample hint") );
		glHint( GL_MULTISAMPLE_FILTER_HINT_NV, HintEnum );

		return 1;
	}

    // PIXELSHADERS crap from Ryan.
	else if( ParseCommand(&Cmd,TEXT("PIXELSHADERS")) )
    {
        if ( !SUPPORTS_GL_ARB_fragment_program )
		{
            debugf( TEXT("OpenGL: No pixel shader support available.") );
		}

		else if( ParseCommand(&Cmd,TEXT("ENABLE")) )
		{
			debugf( TEXT("OpenGL: Enabling GL_ARB_fragment_program...") );
			UsePixelShaders = 1;
		}

		else if( ParseCommand(&Cmd,TEXT("DISABLE")) )
		{
			debugf( TEXT("OpenGL: Disabling GL_ARB_fragment_program...") );
			UsePixelShaders = 0;
		}

		return 1;
    }

	return 0;

	unguard;
}


//
// UOpenGLRenderDevice::Lock
//
FRenderInterface* UOpenGLRenderDevice::Lock(UViewport* Viewport,BYTE* InHitData,INT* InHitSize)
{
	guard(UOpenGLRenderDevice::Lock);
	
	MakeCurrent();

	FrameCounter++;

	check( Viewport );
	LockedViewport = Viewport;

	glViewport( 0, 0, Viewport->SizeX, Viewport->SizeY );
	glScissor( 0, 0, Viewport->SizeX, Viewport->SizeY );

	// Create the render interface.
	RenderInterface.Locked( Viewport, InHitData, InHitSize );

	return &RenderInterface;	
	
	unguard;
}


//
// UOpenGLRenderDevice::Unlock
//
void UOpenGLRenderDevice::Unlock(FRenderInterface* RI)
{
	guard(UOpenGLRenderDevice::Unlock);
	
	RenderInterface.Unlocked();
	LockedViewport = NULL;

	GLError(TEXT("UOpenGLRenderDevice::Unlock"));
	
	unguard;
}


//
// UOpenGLRenderDevice::Present
//
void UOpenGLRenderDevice::Present(UViewport* Viewport)
{
	guard(UOpenGLRenderDevice::Present);

    // This kills framerate, causing bug reports, so it's
    //  not worth it for any "lag reduction".  --ryan.
#if (defined UE_OPENGLDRV_WGL)
    if( ReduceMouseLag && !GIsBenchmarking )
		glFinish();
#endif

#if (defined USE_SDL)
	SDL_GL_SwapWindow((SDL_Window*)Viewport->GetWindow());
#elif (defined UE_OPENGLDRV_WGL)
	verify( SwapBuffers( hDC ) );
#else
#   error Please handle this for your platform.
#endif

	unguard;
}


//
// UOpenGLRenderDevice::ReadPixels
//
void UOpenGLRenderDevice::ReadPixels(UViewport* Viewport,FColor* Pixels,UBOOL Flipped)
{
	guard(UOpenGLRenderDevice::ReadPixels);

	if( Viewport && Pixels )
	{
		glReadPixels( 0, 0, Viewport->SizeX, Viewport->SizeY, GL_BGRA, GL_UNSIGNED_BYTE, Pixels );
		if( !Flipped )
			for( INT i=0; i<Viewport->SizeY/2; i++ )
				for( INT j=0; j<Viewport->SizeX; j++ )
					Exchange( Pixels[j+i*Viewport->SizeX], Pixels[j+(Viewport->SizeY-1-i)*Viewport->SizeX] );
	}

	unguard;
}


//
// UOpenGLRenderDevice::GetRenderCaps
//
FRenderCaps* UOpenGLRenderDevice::GetRenderCaps()
{
	guard(UOpenGLRenderDevice::GetRenderCaps);

	RenderCaps.HardwareTL					= 1;

	if (SUPPORTS_GL_ARB_fragment_program)
	{
		// pretend to be Direct3D Pixel Shader 1.4 for now.
		RenderCaps.MaxSimultaneousTerrainLayers	= 3;
		RenderCaps.PixelShaderVersion			= 14;
	}
	else
	{
		RenderCaps.MaxSimultaneousTerrainLayers	= 1;
		RenderCaps.PixelShaderVersion			= 0;
	}

	return &RenderCaps;
	
	unguard;
}


class FTerrain3LayerPixelShaderARB: public FOpenGLPixelShader
{
public:
	// Constructor/destructor.
	FTerrain3LayerPixelShaderARB(UOpenGLRenderDevice* InRenDev)
	:	FOpenGLPixelShader( InRenDev, PS_Terrain3Layer, 
			"!!ARBfp1.0                                       \n"
			"TEMP tmp0, tmp1, tmp2, tmp3;                     \n"
			//"OUTPUT outcolor = result.color;                \n"
			"TEX tmp0, fragment.texcoord[0], texture[0], 2D;  \n"
			"TEX tmp1, fragment.texcoord[1], texture[1], 2D;  \n"
			"TEX tmp2, fragment.texcoord[2], texture[2], 2D;  \n"
			"TEX tmp3, fragment.texcoord[3], texture[3], 2D;  \n"
			"MUL tmp1, tmp1, tmp0.r;                          \n"
			"MAD tmp1, tmp2, tmp0.g, tmp1;                    \n"
			"MAD tmp1, tmp3, tmp0.b, tmp1;                    \n"
			"MUL tmp0, tmp1, fragment.color.primary;          \n"
			"MUL result.color, tmp0, 2.0;                     \n"  // (mul_x2 in ps1.4 code in D3DDrv)
			"END                                              \n"
		)
	{}
};

class FTerrain4LayerPixelShaderARB: public FOpenGLPixelShader
{
public:
	// Constructor/destructor.
	FTerrain4LayerPixelShaderARB(UOpenGLRenderDevice* InRenDev)
	:	FOpenGLPixelShader( InRenDev, PS_Terrain4Layer, 		
			"!!ARBfp1.0                                       \n"
			"TEMP tmp0, tmp1, tmp2, tmp3, tmp4;               \n"
			//"OUTPUT outcolor = result.color;                \n"
			"TEX tmp0, fragment.texcoord[0], texture[0], 2D;  \n"
			"TEX tmp1, fragment.texcoord[1], texture[1], 2D;  \n"
			"TEX tmp2, fragment.texcoord[2], texture[2], 2D;  \n"
			"TEX tmp3, fragment.texcoord[3], texture[3], 2D;  \n"
			"TEX tmp4, fragment.texcoord[4], texture[4], 2D;  \n"
			"MUL tmp1, tmp1, tmp0.r;                          \n"
			"MAD tmp1, tmp2, tmp0.g, tmp1;                    \n"
			"MAD tmp1, tmp3, tmp0.b, tmp1;                    \n"
			"MAD tmp1, tmp4, tmp0.a, tmp1;                    \n"
			"MUL tmp0, tmp1, fragment.color.primary;          \n"
			"MUL result.color, tmp0, 2.0;                     \n"  // (mul_x2 in ps1.4 code in D3DDrv)
			"END                                              \n"
		)
	{}
};


FOpenGLPixelShader* UOpenGLRenderDevice::GetPixelShader(EPixelShader Type)
{
	guard(UOpenGLRenderDevice::GetPixelShader);
	
	if(( Type == PS_None ) || (!SUPPORTS_GL_ARB_fragment_program))
		return NULL;

	FOpenGLPixelShader*	ShaderPtr = PixelShaders;

	while(ShaderPtr)
	{
		if(ShaderPtr->Type == Type)
			return ShaderPtr;

		ShaderPtr = ShaderPtr->NextPixelShader;
	};

	// Create a new pixel shader.
	switch(Type)
	{
	case PS_Terrain3Layer:
		return new FTerrain3LayerPixelShaderARB(this);
	case PS_Terrain4Layer:
		return new FTerrain4LayerPixelShaderARB(this);
	default:
		return NULL;
	}

	unguard;
}

void UOpenGLRenderDevice::SetPixelShader( GLuint PixelShader )
{
    if (!SUPPORTS_GL_ARB_fragment_program)
        return;

    // UsePixelShaders can be toggled in the console...
    if ((PixelShader == 0) || (!UsePixelShaders))
        glDisable(GL_FRAGMENT_PROGRAM_ARB);
    else
    {
        glEnable(GL_FRAGMENT_PROGRAM_ARB);
        glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, PixelShader);
    }
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

