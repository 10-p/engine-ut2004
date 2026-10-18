/*=============================================================================
	GL4ESRenderDevice.cpp: GL4ES (GL-to-GLES2 translation) render device.
	Based on OpenGLRenderDevice.cpp by Daniel Vogel.
	Modified for GL4ES integration -- SDL2/Linux-only.

=============================================================================*/

#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
// On the threaded web the viewport creates and presents the WebGL context on the worker, so the entry-point
// resolver and the frame present come from Emscripten. Included before the engine headers: this pulls in
// <time.h>, whose clock() declaration UnFile.h's clock(Timer) macro would mangle.
#include <emscripten/html5_webgl.h>
#endif

#include "GL4ESDrv.h"

// MinGW's CRT has no POSIX setenv(); the equivalent is _putenv_s, with a different argument order. gl4es is
// configured through LIBGL_* environment variables, so this shim keeps one list of setenv calls for every
// platform.
#if defined(_WIN32) && !defined(__CYGWIN__)
#include <stdlib.h>
static inline int setenv( const char* Name, const char* Value, int Overwrite )
{
	if( !Overwrite && getenv(Name) )
		return 0;
	return _putenv_s( Name, Value );
}
#endif

#if defined(__ANDROID__)
#include <sys/system_properties.h>   // __system_property_get / PROP_VALUE_MAX (emulator detection for LIBGL_NOBGRA)
#endif

/*-----------------------------------------------------------------------------
	Constants & static stuff.
-----------------------------------------------------------------------------*/

// GL4ES is statically linked: no runtime library loading (no GL_DLL).
// USE_SDL selects the SDL paths of the shared platform conditionals.
#define USE_SDL 1

// Static variables.
INT				UGL4ESRenderDevice::NumDevices		= 0;
INT				UGL4ESRenderDevice::LockCount		= 0;
BYTE*			UGL4ESRenderDevice::VARPointer		= 0;
INT				UGL4ESRenderDevice::VARIndex		= 0;
INT				UGL4ESRenderDevice::VARSize		= 0;

UBOOL			UGL4ESRenderDevice::GLLoaded		= 0;

// OpenGL function pointers.
#define GL_EXT(name) UBOOL UGL4ESRenderDevice::SUPPORTS##name=0;
#define GL_PROC(ext,ret,func,parms) ret (STDCALL *UGL4ESRenderDevice::func)parms;
#include "GL4ESFuncs.h"
#undef GL_EXT
#undef GL_PROC

/*-----------------------------------------------------------------------------
	OpenGLRenderDevice.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UGL4ESRenderDevice);

//
// UGL4ESRenderDevice::UGL4ESRenderDevice
//
UGL4ESRenderDevice::UGL4ESRenderDevice() :
	RenderInterface(this)
{
	guard(UGL4ESRenderDevice::UGL4ESRenderDevice);
	
	LockedViewport		= NULL;
	IsVAR				= 0;
	ValidContext		= 0;
	WasFullscreen		= 0;

	unguard;
}

//
// UGL4ESRenderDevice::StaticConstructor
//
void UGL4ESRenderDevice::StaticConstructor()
{
	guard(UGL4ESRenderDevice::StaticConstructor);

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
	new(GetClass(),TEXT("MultisampleBuffers"),	RF_Public)UIntProperty  ( CPP_PROPERTY(MultisampleBuffers  ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("MultisampleSamples"),	RF_Public)UIntProperty  ( CPP_PROPERTY(MultisampleSamples  ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("MultisampleHint"),		RF_Public)UIntProperty  ( CPP_PROPERTY(MultisampleHint     ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("LevelOfAnisotropy"),	RF_Public)UFloatProperty( CPP_PROPERTY(LevelOfAnisotropy   ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("UseRenderTargets"),	RF_Public)UBoolProperty	( CPP_PROPERTY(UseRenderTargets    ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("UsePixelShaders"),		RF_Public)UBoolProperty ( CPP_PROPERTY(UsePixelShaders     ), TEXT("Options"), CPF_Config );

	GIsOpenGL						= 1;
	GIsPixomatic					= 0;

	// Compiled defaults: an ini without a [GL4ESDrv.GL4ESRenderDevice] section (a retail install) leaves
	// every CPF_Config option at zero. DetailTextures defaults on (as Default.ini does for OpenGLDrv/D3DDrv),
	// or the HandleDetail gate is never reached and detail-texture layers vanish. The ini wins.
	DetailTextures					= 1;

	// UseVBO defaults on: generally supported, and the web needs it for performance. With UseVBO off,
	// SUPPORTS_GL_ARB_vertex_buffer_object is cleared (see Init) and CommitStreams hands GL client-side
	// pointers; WebGL has no client-side arrays, so Emscripten's FULL_ES2 emulation copies the vertex range
	// into a temp buffer on every draw call (the reason the link raises MAX_TEMP_BUFFER_SIZE). The VBO path
	// (FGL4ESVertexStreamARB_VBO / FGL4ESIndexBufferARB_VBO) uploads once and rebinds per draw.
	UseVBO							= 1;

	// Default the device to its best quality: an ini without this section otherwise runs with
	// MaxTextureUnits=0, no trilinear, no anisotropy and a zero VAR size. The values match
	// Engine/Config/web/Default.ini; the ini wins wherever it sets one.
	MaxTextureUnits					= 8;		// clamped to the real GL limit at init
	UseTrilinear					= 1;
	LevelOfAnisotropy				= 16.f;		// max; the driver clamps to what the GPU offers
	VARSize							= 32;
	UseRenderTargets				= 1;
	DetailTexMipBias				= 0.f;
	AppleVA							= 1;
	MultisampleHint					= 2;

	// Not raised:
	//   UsePixelShaders   0 - the fixed-function path is the one this device actually implements.
	//   ForceCompression  0 - forcing DXT on already-decoded textures LOSES quality; off is better.
	//   UseVSync          0 - the browser paces frames; vsync here fights the host's own scheduling.
	//   ReduceMouseLag    0 - a stall-and-sync that costs frames; the shipped config disables it.
	UsePixelShaders					= 0;
	ForceCompression				= 0;
	UseVSync						= 0;
	ReduceMouseLag					= 0;
	DesiredRefreshRate				= 0;

	// MSAA on by default (4x). On the threaded web this is on/off only: the viewport creates the WebGL
	// context itself (SDL2Viewport.cpp) and WebGL's `antialias` flag lets the browser pick the sample count.
	// The viewport reads these from the active render device's ini section.
	MultisampleBuffers				= 1;
	MultisampleSamples				= 4;

	SupportsCubemaps				= 1;
	SupportsZBIAS					= 1;
	SupportsRenderToTextureRGBA8888	= 0;
	SupportsRenderToTextureRGB565	= 0;
	SupportsRenderToTextureNPOT		= 0;

	unguard;
}


//
// UGL4ESRenderDevice::FindExt
//
UBOOL UGL4ESRenderDevice::FindExt( const TCHAR* Name )
{
	guard(UGL4ESRenderDevice::FindExt);
	
	// Can't use appStrFind as extension string might be > 1024 characters.
	UBOOL Result = strstr( (char*) glGetString(GL_EXTENSIONS), appToAnsi(Name) ) != NULL;
	if( Result )
		debugf( TEXT("GL4ES: Device supports: %s"), Name );
	return Result;
	
	unguard;
}


//
// UGL4ESRenderDevice::FindProc
//
void UGL4ESRenderDevice::FindProc( void*& ProcAddress, char* Name, char* SupportName, UBOOL& Supports, UBOOL AllowExt )
{
	guard(UGL4ESRenderDevice::FindProc);

	// Resolve GL functions through the GL4ES translation layer.
	if( !ProcAddress )
		ProcAddress = gl4es_GetProcAddress( Name );

	if( !ProcAddress )
	{
		if( Supports )
			debugf( TEXT("GL4ES: Missing function '%s' for '%s' support"), appFromAnsi(Name), appFromAnsi(SupportName) );
		Supports = 0;
	}

	unguard;
}


//
// UGL4ESRenderDevice::FindProcs
//
void UGL4ESRenderDevice::FindProcs( UBOOL AllowExt )
{
	guard(UGL4ESDriver::FindProcs);

	#define GL_EXT(name) if( AllowExt ) SUPPORTS##name = FindExt( TEXT(#name)+1 );
	#define GL_PROC(ext,ret,func,parms) FindProc( *(void**)&func, #func, #ext, SUPPORTS##ext, AllowExt );
	#include "GL4ESFuncs.h"
	#undef GL_EXT
	#undef GL_PROC

	unguard;
}


//
// UGL4ESRenderDevice::MakeCurrent
//
void UGL4ESRenderDevice::MakeCurrent()
{
	guard(UGL4ESRenderDevice::MakeCurrent);

	check(ValidContext);
	// SDL2 manages the GL context; the SDL2Drv viewport makes it current.

	unguard;
}


//
// UGL4ESRenderDevice::GLError
//
void UGL4ESRenderDevice::GLError( TCHAR* Tag )
{
	guard(UGL4ESRenderDevice::GLError);

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
		debugf( TEXT("GL4ES Error: %s (%s)"), Msg, Tag );
	}

	unguard;
}


//
// UGL4ESRenderDevice::Init
//
UBOOL UGL4ESRenderDevice::Init()
{
	guard(UGL4ESRenderDevice::Init);

	debugf( TEXT("Initializing GL4ESDrv...") );

	printf("[GL4ES-DBG] Init: start\n");

	GIsOpenGL						= 1;
	GIsPixomatic					= 0;

	SupportsCubemaps				= 1;
	SupportsZBIAS					= 1;
	SupportsRenderToTextureRGBA8888	= 0;
	SupportsRenderToTextureRGB565	= 0;
	SupportsRenderToTextureNPOT		= 0;

	// GL4ES is statically linked: no runtime library loading. FindProcs(0) and the GL4ES
	// initialization run in SetRes(), after the GLES2 context exists.
	if( NumDevices==0 )
	{
		GLLoaded = true;
		SUPPORTS_GL = 1;
	}

	// Scratch buffer used by e.g. resource management.
	ScratchBuffer.Empty();
	ScratchBuffer.Add( 65536 );

	NumDevices++;

	printf("[GL4ES-DBG] Init: complete, returning 1\n");

	GGPUVendorID = 0xEEEE;
	GGPUDeviceID = 0;

	return 1;
	unguard;
}


//
// UGL4ESRenderDevice::Exit
//
void UGL4ESRenderDevice::Exit(UViewport* Viewport)
{
	guard(UGL4ESRenderDevice::Exit);

	check(NumDevices>0);

	// Shut down RC.
	Flush( Viewport );

	UnSetRes( 1 );

	RestoreGamma();

	// GL4ES is statically linked: no DLLs to free.
	NumDevices--;

	unguard;
}


//
// UGL4ESRenderDevice::SetRes
//
UBOOL UGL4ESRenderDevice::SetRes(UViewport* Viewport,INT NewX,INT NewY,UBOOL Fullscreen,INT ColorBytes,UBOOL bSaveSize)
{
	guard(UGL4ESRenderDevice::SetRes);

	// Returning 0 causes infinite loops and I'd rather catch those bugs at the root.
	if( !GIsEditor && LockedViewport )
		appErrorf(TEXT("Can't change resolution while render device is locked!"));

	debugf(TEXT("Enter SetRes: %dx%d Fullscreen %d"), NewX, NewY, Fullscreen );
	printf("[GL4ES-DBG] SetRes: enter %dx%d\n", NewX, NewY);

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
	MultisampleBuffers = Max(MultisampleBuffers, 0);
	MultisampleSamples = Max(MultisampleSamples, 0);

#if (defined USE_SDL)
	SDL_GL_SetAttribute( SDL_GL_RED_SIZE, ColorBytes<=2 ? 5 : 8 );
	SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, ColorBytes<=2 ? 5 : 8 );
	SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, ColorBytes<=2 ? 5 : 8 );
	SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, ColorBytes<=2 ? 16 : 24 );
	SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLEBUFFERS, MultisampleBuffers );
	SDL_GL_SetAttribute( SDL_GL_MULTISAMPLESAMPLES, MultisampleSamples );

	// Change window size.
	Viewport->ResizeViewport( Fullscreen ? (BLIT_Fullscreen|BLIT_OpenGL) : (BLIT_OpenGL), NewX, NewY );

#elif (defined WIN32)

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
		appErrorf(NAME_FriendlyError, TEXT("The GL4ES renderer doesn't support running the Editor. Please switch to the D3D renderer if you want to use the Editor.") );

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

	// GL4ES initialization: after SDL2 (or the web viewport) has created the GLES2 context, once.
	{
		static bool gl4es_initialized = false;
		if( !gl4es_initialized )
		{
			debugf( TEXT("GL4ES: Configuring GL4ES environment...") );

			// Configure GL4ES before initialization. Every LIBGL_* value is fixed here; the
			// user's environment is not passed through.
			setenv("LIBGL_ES", "2", 1);               // Target GLES2
			setenv("LIBGL_NOBANNER", "1", 1);          // Suppress GL4ES banner
			setenv("LIBGL_NOERROR", "0", 1);           // Keep error reporting for debugging
			// LIBGL_USEVBO=1: real buffer objects. With 0, gl4es hands glVertexAttribPointer client-side
			// pointers whatever the driver's UseVBO says, and WebGL has no client-side arrays (Emscripten
			// copies the vertex and index ranges before every draw). Tiled GLES2 drivers on Android
			// prefer VBOs too, so this applies everywhere.
			setenv("LIBGL_USEVBO", "1", 1);            // Real VBOs; client arrays don't exist in WebGL
			setenv("LIBGL_BATCH", "0", 1);             // No draw-call batching (slower here)
			setenv("LIBGL_NOARBPROGRAM", "0", 1);      // Allow ARB_fragment_program conversion
			setenv("LIBGL_AVOID16BITS", "1", 1);       // 32-bit textures for quality
			setenv("LIBGL_MIPMAP", "0", 1);            // Normal mipmap handling
			setenv("LIBGL_FPS", "0", 1);               // No FPS spam to stdout
			setenv("LIBGL_SILENTSTUB", "1", 1);        // Hide stub function warnings
			setenv("LIBGL_NPOT", "2", 1);              // Advertise full NPOT support
			setenv("LIBGL_NOTEXRECT", "1", 1);         // Hide texture_rectangle
			// No precompiled-shader archive: natively gl4es would write one into the player's home
			// ($HOME/.gl4es.psa); every target builds its programs as the browser must (WebGL has no
			// program binaries).
			setenv("LIBGL_NOPSA", "1", 1);
			// NVIDIA's own OpenGL ES driver rejects the int overloads gl4es inserts into converted shaders.
			setenv("LIBGL_NOINTOVLHACK", "1", 1);

#if defined(__ANDROID__)
			// The Android emulator's emugl GLES translator mishandles GL_BGRA texture uploads (it
			// advertises GL_EXT_texture_format_BGRA8888, then corrupts channels and alpha). LIBGL_NOBGRA
			// makes gl4es swizzle BGRA->RGBA in software, which is per-texture CPU work that real
			// devices' drivers (Adreno/Mali, native BGRA8888) do not need, so it is scoped to the
			// emulator (ro.kernel.qemu / ro.hardware=goldfish|ranchu).
			{
				char qemu[PROP_VALUE_MAX] = {0}, hw[PROP_VALUE_MAX] = {0};
				__system_property_get( "ro.kernel.qemu", qemu );
				__system_property_get( "ro.hardware", hw );
				if( qemu[0] == '1' || strstr(hw, "goldfish") || strstr(hw, "ranchu") )
				{
					debugf( TEXT("GL4ES: Android emulator detected -> LIBGL_NOBGRA=1 (SW BGRA swizzle)") );
					setenv("LIBGL_NOBGRA", "1", 1);
				}
			}
#endif

			// Tell GL4ES how to find GLES2 functions via SDL2.
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
			// The context was created with emscripten_webgl_create_context, not through SDL, so
			// SDL_GL_GetProcAddress has no context to resolve against; Emscripten's resolver is
			// context-aware and works on the worker.
			set_getprocaddress(emscripten_webgl_get_proc_address);
#else
			set_getprocaddress(SDL_GL_GetProcAddress);
#endif

			// Initialize GL4ES translation layer.
			debugf( TEXT("GL4ES: Calling initialize_gl4es()...") );
			initialize_gl4es();

			// Resolve basic GL function pointers through gl4es_GetProcAddress.
			FindProcs( 0 );
			if( !SUPPORTS_GL )
				appErrorf(TEXT("GL4ES: Missing basic GL symbols after initialization."));

			gl4es_initialized = true;
			debugf( TEXT("GL4ES: Initialization complete.") );
		}
	}

	debugf( TEXT("GL_VENDOR     : %s"), appFromAnsi((ANSICHAR*)glGetString(GL_VENDOR))	);
	debugf( TEXT("GL_RENDERER   : %s"), appFromAnsi((ANSICHAR*)glGetString(GL_RENDERER)) );
	debugf( TEXT("GL_VERSION    : %s"), appFromAnsi((ANSICHAR*)glGetString(GL_VERSION))	);

	FindProcs( 1 );

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
		debugf( TEXT("GL4ES: Don't know how much RAM you've got to clamp VARsize!") );
	else
	{
		INT MaxVARSize = (INT) (((0.75f * 0.25f * GPhysicalMemory) / 1024.0f) / 1024.0f);
		if (ConfigVARSize > MaxVARSize)
		{
			debugf( TEXT("GL4ES: Clamping VARsize to %d (was %d): not enough physical RAM."),
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

	debugf( TEXT("GL4ES: C%i RGB%i%i%i Z%i S%i"), ColorBytes * 8, RedBits, GreenBits, BlueBits, DepthBits, StencilBits );

	if( SUPPORTS_GL_EXT_texture_filter_anisotropic )
	{
		GLfloat MaxAnisotropy;
		glGetFloatv( GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &MaxAnisotropy );
		LevelOfAnisotropy = Max<FLOAT>( 1.0f, Min<FLOAT>( MaxAnisotropy, LevelOfAnisotropy ) );
		debugf( TEXT("GL4ES: Level of anisotropy is %f (max %f)."), (float) LevelOfAnisotropy, (float) MaxAnisotropy );
	}

#ifndef WIN32
	if( !SUPPORTS_GL_ARB_multisample )
		debugf( TEXT("GL4ES: Not using multisample") );
	else
	{
        INT Buffers, Samples;
		glGetIntegerv( GL_SAMPLE_BUFFERS_ARB, &Buffers );
		glGetIntegerv( GL_SAMPLES_ARB, &Samples );

		debugf( TEXT("GL4ES: Have %d multisamples buffer%s, %d sample%s."),
                (int) Buffers, Buffers == 1 ? TEXT("") : TEXT("s"),
                (int) Samples, Samples == 1 ? TEXT("") : TEXT("s") );

		if( !Buffers || !Samples )
		{
			debugf( TEXT("GL4ES: Failed to get a multisample GL context") );
			SUPPORTS_GL_ARB_multisample = 0;
			SUPPORTS_GL_NV_multisample_filter_hint = 0;
		}
		else
        {
			debugf( TEXT("GL4ES: Using multisample (%d buffers, %d samples)"),
			        (int) Buffers, (int) Samples );

			glEnable( GL_MULTISAMPLE_ARB );

			if( SUPPORTS_GL_NV_multisample_filter_hint )
			{
				TCHAR *HintString[] = { TEXT("DONT_CARE"), TEXT("FASTEST"), TEXT("NICEST") };
				GLenum HintEnum[] = { GL_DONT_CARE, GL_FASTEST, GL_NICEST };
				debugf( TEXT("GL4ES: Using multisample hint: GL_%s"), HintString[MultisampleHint] );
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
            debugf(TEXT("GL4ES: Using GL_ARB_fragment_program for pixel shaders."));
        else
        {
            SUPPORTS_GL_ARB_fragment_program = 0;
            debugf(TEXT("GL4ES: Forcibly disabled pixel shaders."));
        }
    }

	// Check for old extension.
	SUPPORTS_GL_ATI_texture_env_combine3 |= SUPPORTS_GL_ATIX_texture_env_combine3;
	
	//!!TODO: it actually is faster to not use it and live with the implied memcpy.
	SUPPORTS_GL_ATI_map_object_buffer = 0;

	// NVIDIA doesn't expose the crossbar extension but supports it.
	SUPPORTS_GL_ARB_texture_env_crossbar |= SUPPORTS_GL_NV_texture_env_combine4;

	// Use ARB_vbo where available.
	const UBOOL DeviceHasVBO = SUPPORTS_GL_ARB_vertex_buffer_object;   // the else below CLEARS the flag
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

	// Log which vertex path won and both of its inputs: "off" because the device lacks the extension
	// and "off" because the config said so need different fixes. DeviceHasVBO is taken above because
	// the else branch clears the flag.
	debugf( TEXT("GL4ES: Vertex buffers: %s (UseVBO=%i, device GL_ARB_vertex_buffer_object=%i)"),
		SUPPORTS_GL_ARB_vertex_buffer_object ? TEXT("VBO") : TEXT("CLIENT-SIDE ARRAYS (slow on WebGL)"),
		(INT)UseVBO, (INT)DeviceHasVBO );

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
			debugf( TEXT("GL4ES: Using GL_APPLE_vertex_array_range.") );
		break;
	case 2:
		// Use APPLE_VAR + APPLE_VAO
		SUPPORTS_GL_APPLE_vertex_array_range	= 0;
		if( SUPPORTS_GL_APPLE_vertex_array_object )
			debugf( TEXT("GL4ES: Using GL_APPLE_vertex_array_object.") );
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
        debugf(TEXT("GL4ES: Render-to-Texture support unavailable."));
    else
    {
        if (UseRenderTargets)
            debugf(TEXT("GL4ES: Using GL_EXT_framebuffer_object for render-to-texture."));
        else
        {
            SUPPORTS_GL_EXT_framebuffer_object = 0;
            debugf(TEXT("GL4ES: Forcibly disabled render-to-texture."));
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
	// NV/APPLE vertex array range is not available through GL4ES, so this block never
	// runs; the variables stay initialised.
	VARPointer		= NULL;
	VARSize			= 0;
	VARIndex		= 0;

	SupportsCubemaps                = SUPPORTS_GL_ARB_texture_cube_map;
	SupportsZBIAS                   = 1;
	Viewport->PendingFrame			= 0;
	WasFullscreen                   = Fullscreen;

	UpdateGamma( Viewport );

	return 1;

	unguard;
}


//
// UGL4ESRenderDevice::UnSetRes
//
void UGL4ESRenderDevice::UnSetRes( UBOOL RestoreDisplaySettings )
{
	guard(UGL4ESRenderDevice::UnSetRes);

	if( ValidContext )
		glDeleteTextures( 1, &NullTextureID );

	// Vertex array range: not used.
	VARPointer	= NULL;
	VARSize		= 0;
	VARIndex	= 0;

	ValidContext = 0;

	unguard;
}


//
//	UGL4ESRenderDevice::GetCachedResource
//
FGL4ESResource* UGL4ESRenderDevice::GetCachedResource(QWORD CacheId)
{
	guard(UGL4ESRenderDevice::GetCachedResource);

	INT					HashIndex	= GetResourceHashIndex(CacheId);
	FGL4ESResource*	ResourcePtr = ResourceHash[HashIndex];

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
//	UGL4ESRenderDevice::FlushResource
//
void UGL4ESRenderDevice::FlushResource(QWORD CacheId)
{
	guard(UGL4ESRenderDevice::GetCachedResource);

	FGL4ESResource*	CachedResource = GetCachedResource(CacheId);

	if(CachedResource)
		delete CachedResource;

	unguard;
}


//
//	UGL4ESRenderDevice::GetVertexShader
//
FGL4ESVertexShader* UGL4ESRenderDevice::GetVertexShader(EVertexShader Type,FGL4ESShaderDeclaration& Declaration)
{
	guard(UGL4ESRenderDevice::GetVertexShader);

	// Find an existing vertex shader with the same type/declaration.
	FGL4ESVertexShader*	ShaderPtr = VertexShaders;

	while(ShaderPtr)
	{
		if(ShaderPtr->Type == Type && ShaderPtr->Declaration == Declaration)
			return ShaderPtr;

		ShaderPtr = ShaderPtr->NextVertexShader;
	};

	// Create a new vertex shader.
	if(Type == VS_FixedFunction)
		return new FGL4ESFixedVertexShader(this,Declaration);
	else
		return NULL;

	unguard;
}


//
// UGL4ESRenderDevice::ResourceCached
//
UBOOL UGL4ESRenderDevice::ResourceCached(QWORD CacheId)
{
	 FGL4ESResource*	Resource = GetCachedResource(CacheId);

	 if(!Resource)
		 return 0;

#if 0
	 FGL4ESTexture*	Texture = Resource->GetTexture();

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
// UGL4ESRenderDevice::Flush
//
void UGL4ESRenderDevice::Flush(UViewport* Viewport)
{
	guard(UGL4ESRenderDevice::Flush);

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
// UGL4ESRenderDevice::UpdateGamma
//
void UGL4ESRenderDevice::UpdateGamma(UViewport* Viewport)
{
	guard(UGL4ESRenderDevice::UpdateGamma);

	if( ParseParam(appCmdLine(),TEXT("NOGAMMA")) )
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

	// Gamma through SDL2.
	SDL_SetWindowGammaRamp( (SDL_Window*)Viewport->GetWindow(), Ramp.red, Ramp.green, Ramp.blue );

	unguard;
}


//
// UGL4ESRenderDevice::RestoreGamma
//
void UGL4ESRenderDevice::RestoreGamma()
{
	guard(UGL4ESRenderDevice::RestoreGamma);

	if( ParseParam(appCmdLine(),TEXT("NOGAMMA")) )
		return;

	struct
	{
		_WORD red[256];
		_WORD green[256];
		_WORD blue[256];
	} Ramp;

	for(INT i=0; i<256; i++)
		Ramp.red[i] = Ramp.green[i] = Ramp.blue[i] = i << 8;

	// Restore the gamma through SDL2.
	SDL_Window* GammaWindow = SDL_GetKeyboardFocus();
	if( GammaWindow )
		SDL_SetWindowGammaRamp( GammaWindow, Ramp.red, Ramp.green, Ramp.blue );

	unguard;
}


//
// UGL4ESRenderDevice::Exec
//
UBOOL UGL4ESRenderDevice::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	guard(UGL4ESRenderDevice::Exec);
	
	if(ParseCommand(&Cmd,TEXT("DUMPRESOURCEHASH")))
	{
		for(INT HashIndex = 0;HashIndex < 4096;HashIndex++)
		{
			INT	ResourceCount = 0;

			for(FGL4ESResource*	Resource = ResourceHash[HashIndex];Resource;Resource = Resource->HashNext)
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
			for(FGL4ESResource* Resource = ResourceHash[HashIndex];Resource;Resource = Resource->HashNext)
				Count++;
	
			ResourceCount  += Count;
			HashCollisions += Count ? Count - 1 : 0;
		}
	
		debugf(TEXT("GL4ES resource hash: %i resources, %i collisions"), ResourceCount, HashCollisions );

		return 1;
	}

	else if( ParseCommand(&Cmd,TEXT("RESOURCES") ))
	{
		// Calculate resource usage
		INT StatTextureBytes=0, StatVertexStreamBytes=0, StatIndexBufferBytes=0, StatOtherBytes=0;
		INT StatNumTextures=0, StatNumVertexStreams=0, StatNumIndexBuffers=0, StatNumOther=0;
		for( FGL4ESResource* Resource = ResourceList;Resource;Resource = Resource->NextResource )
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

		Ar.Logf( TEXT("GL4ES Resource Usage"));
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
			debugf( TEXT("GL4ES: No FSAA support available") );
		}

		else if( ParseCommand(&Cmd,TEXT("ENABLE")) )
		{
			debugf( TEXT("GL4ES: Enabling GL_MULTISAMPLE_ARB...") );
			glEnable( GL_MULTISAMPLE_ARB );
		}

		else if( ParseCommand(&Cmd,TEXT("DISABLE")) )
		{
			debugf( TEXT("GL4ES: Disabling GL_MULTISAMPLE_ARB...") );
			glDisable( GL_MULTISAMPLE_ARB );
		}

		return 1;
    }

	else if( ParseCommand(&Cmd,TEXT("FSAAHINT")) )
	{
		if( !SUPPORTS_GL_NV_multisample_filter_hint )
		{
			debugf( TEXT("GL4ES: No FSAA hinting available") );
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
			debugf( TEXT("GL4ES: Unknown FSAA hinting option") );
			return 1;
		}

		debugf( TEXT("GL4ES: Changing multisample hint") );
		glHint( GL_MULTISAMPLE_FILTER_HINT_NV, HintEnum );

		return 1;
	}

    // PIXELSHADERS crap from Ryan.
	else if( ParseCommand(&Cmd,TEXT("PIXELSHADERS")) )
    {
        if ( !SUPPORTS_GL_ARB_fragment_program )
		{
            debugf( TEXT("GL4ES: No pixel shader support available.") );
		}

		else if( ParseCommand(&Cmd,TEXT("ENABLE")) )
		{
			debugf( TEXT("GL4ES: Enabling GL_ARB_fragment_program...") );
			UsePixelShaders = 1;
		}

		else if( ParseCommand(&Cmd,TEXT("DISABLE")) )
		{
			debugf( TEXT("GL4ES: Disabling GL_ARB_fragment_program...") );
			UsePixelShaders = 0;
		}

		return 1;
    }

	return 0;

	unguard;
}


//
// UGL4ESRenderDevice::Lock
//
FRenderInterface* UGL4ESRenderDevice::Lock(UViewport* Viewport,BYTE* InHitData,INT* InHitSize)
{
	guard(UGL4ESRenderDevice::Lock);
	
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
// UGL4ESRenderDevice::Unlock
//
void UGL4ESRenderDevice::Unlock(FRenderInterface* RI)
{
	guard(UGL4ESRenderDevice::Unlock);
	
	RenderInterface.Unlocked();
	LockedViewport = NULL;

	GLError(TEXT("UGL4ESRenderDevice::Unlock"));
	
	unguard;
}


//
// UGL4ESRenderDevice::Present
//
void UGL4ESRenderDevice::Present(UViewport* Viewport)
{
	guard(UGL4ESRenderDevice::Present);

	// Present.
#if defined(__EMSCRIPTEN__) && defined(__EMSCRIPTEN_PTHREADS__)
	// The context was created with explicitSwapControl, so this is the present (SDL_GL_SwapWindow
	// would go through the EGL emulation, which cannot see the worker's context).
	emscripten_webgl_commit_frame();
#else
	SDL_GL_SwapWindow((SDL_Window*)Viewport->GetWindow());
#endif

	unguard;
}


//
// UGL4ESRenderDevice::ReadPixels
//
void UGL4ESRenderDevice::ReadPixels(UViewport* Viewport,FColor* Pixels,UBOOL Flipped)
{
	guard(UGL4ESRenderDevice::ReadPixels);

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
// UGL4ESRenderDevice::GetRenderCaps
//
FRenderCaps* UGL4ESRenderDevice::GetRenderCaps()
{
	guard(UGL4ESRenderDevice::GetRenderCaps);

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


class FTerrain3LayerPixelShaderARB: public FGL4ESPixelShader
{
public:
	// Constructor/destructor.
	FTerrain3LayerPixelShaderARB(UGL4ESRenderDevice* InRenDev)
	:	FGL4ESPixelShader( InRenDev, GL4ES_PS_Terrain3Layer, 
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

class FTerrain4LayerPixelShaderARB: public FGL4ESPixelShader
{
public:
	// Constructor/destructor.
	FTerrain4LayerPixelShaderARB(UGL4ESRenderDevice* InRenDev)
	:	FGL4ESPixelShader( InRenDev, GL4ES_PS_Terrain4Layer, 		
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


FGL4ESPixelShader* UGL4ESRenderDevice::GetPixelShader(EGL4ESPixelShader Type)
{
	guard(UGL4ESRenderDevice::GetPixelShader);
	
	if(( Type == GL4ES_PS_None ) || (!SUPPORTS_GL_ARB_fragment_program))
		return NULL;

	FGL4ESPixelShader*	ShaderPtr = PixelShaders;

	while(ShaderPtr)
	{
		if(ShaderPtr->Type == Type)
			return ShaderPtr;

		ShaderPtr = ShaderPtr->NextPixelShader;
	};

	// Create a new pixel shader.
	switch(Type)
	{
	case GL4ES_PS_Terrain3Layer:
		return new FTerrain3LayerPixelShaderARB(this);
	case GL4ES_PS_Terrain4Layer:
		return new FTerrain4LayerPixelShaderARB(this);
	default:
		return NULL;
	}

	unguard;
}

void UGL4ESRenderDevice::SetPixelShader( GLuint PixelShader )
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

