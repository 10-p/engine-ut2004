/*=============================================================================
	PixoRenderDevice.cpp: Unreal Pixo support.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Michael Sartain from GL driver
		* Taken over by Daniel Vogel

=============================================================================*/

#include "PixoDrv.h"
#if defined(_WIN32) && !USE_SDL
// ufront 2.58: the HWND translation in SetRes (see there). SDL2 is linked on every platform.
#include "SDL.h"
#include "SDL_syswm.h"
#endif

INT UPixoRenderDevice::STATS_NumVertsXformed = -1;
INT UPixoRenderDevice::STATS_NumTrianglesSubmitted = -1;
INT UPixoRenderDevice::STATS_NumTrianglesDrawn = -1;
UBOOL UPixoRenderDevice::ShowDepthComplexity = 0;

int UPixoRenderDevice::cpu_features;

/*-----------------------------------------------------------------------------
	PixoRenderDevice.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UPixoRenderDevice);

//
// UPixoRenderDevice::UPixoRenderDevice
//
UPixoRenderDevice::UPixoRenderDevice() :
	RenderInterface(this)
{
	guard(UPixoRenderDevice::UPixoRenderDevice);

	LockedViewport      = NULL;
	ValidContext        = 0;
	WasFullscreen       = 0;

	PixoBuffer          = NULL;
	ZBuffer             = NULL;
	ZBufferBlock        = NULL;
	ZBufferPitch        = 0;

    VertexCacheCount    = 0;
    PixoVertexCache     = NULL;

	unguard;
}

//
// UPixoRenderDevice::StaticConstructor
//
void UPixoRenderDevice::StaticConstructor()
{
	guard(UPixoRenderDevice::StaticConstructor);

	new(GetClass(),TEXT("DetailTexMipBias"),    RF_Public)UFloatProperty( CPP_PROPERTY(DetailTexMipBias    ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("DesiredRefreshRate"),  RF_Public)UIntProperty  ( CPP_PROPERTY(DesiredRefreshRate  ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("FilterQuality3D"),     RF_Public)UIntProperty  ( CPP_PROPERTY(FilterQuality3D     ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("FilterQualityHUD"),    RF_Public)UIntProperty  ( CPP_PROPERTY(FilterQualityHUD    ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("FogEnabled"),          RF_Public)UBoolProperty ( CPP_PROPERTY(FogEnabled          ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("Zoom2X"),              RF_Public)UBoolProperty ( CPP_PROPERTY(Zoom2X              ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("LimitTextureSize"),    RF_Public)UBoolProperty ( CPP_PROPERTY(LimitTextureSize    ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("SimpleMaterials"),		RF_Public)UBoolProperty ( CPP_PROPERTY(SimpleMaterials     ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("UseVisibilityQuery"),	RF_Public)UBoolProperty ( CPP_PROPERTY(UseVisibilityQuery  ), TEXT("Options"), CPF_Config );

	GIsPixomatic						= 1;
	GIsOpenGL							= 0;

	SupportsCubemaps					= 0;
	SupportsRenderToTextureRGBA8888		= 1;
	SupportsRenderToTextureRGB565		= 0;
	SupportsZBIAS						= 1;

	unguard;
}

//
// UPixoRenderDevice::Init
//
UBOOL UPixoRenderDevice::Init()
{
	guard(UPixoRenderDevice::Init);

	GIsPixomatic						= 1;
	GIsOpenGL							= 0;

	SupportsCubemaps					= 0;
	SupportsRenderToTextureRGBA8888		= 1;
	SupportsRenderToTextureRGB565		= 0;
	SupportsZBIAS						= 1;

	//!!vogel: hardcode this for now
	SimpleMaterials		= 1;
	UseStencil			= 0;

	if(STATS_NumTrianglesSubmitted == -1)
	{
		STATS_NumTrianglesSubmitted = GStats.RegisterStats(
			STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("NumTrianglesSubmitted"),
			TEXT("Hardware"), STATSUNIT_Default);
		STATS_NumTrianglesDrawn = GStats.RegisterStats(
			STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("NumTrianglesDrawn"),
			TEXT("Hardware"), STATSUNIT_Default);
        STATS_NumVertsXformed = GStats.RegisterStats(
                    STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("NumVertsXformed"),
                    TEXT("Hardware"), STATSUNIT_Default);
	}

	debugf( NAME_Init, TEXT("Initializing PixoDrv...") );

	#if USE_SDL12  //(not actually used; SDLDrv doesn't need this. --ryan)
		SDL_PixelFormat *vfmt = SDL_GetVideoInfo()->vfmt;
		SDL_Rect **sdlmodes = SDL_ListModes(vfmt, SDL_FULLSCREEN);
		if ( (sdlmodes == NULL) || (sdlmodes == (SDL_Rect **) -1) )
		{
			debugf( NAME_Init, TEXT("PixoDrv: SDL found no modes?!") );
			//return 0;
		}
		else
		{
			for (INT j = 0; sdlmodes[j] != NULL; j++)
			{
				SDL_Rect *mode = sdlmodes[j];
				Modes.AddUniqueItem( FPlane( mode->w, mode->h, vfmt->BitsPerPixel, 60 ) );
			}
		}

	#elif USE_SDL
		// ufront 2.12 (SDL2/gl4es): the fullscreen mode list is unused (the engine drives resolution via the
		// viewport), and SDL_GetVideoInfo/SDL_ListModes don't exist in SDL2 -- skip enumeration.
	#else
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

				Modes.AddUniqueItem( FPlane(    DisplayMode.dmPelsWidth,
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

				Modes.AddUniqueItem( FPlane(    DisplayMode.dmPelsWidth,
												DisplayMode.dmPelsHeight,
												DisplayMode.dmBitsPerPel,
												DisplayMode.dmDisplayFrequency
											));
			}
		}
	#endif

	PixoStartup(PIXO_ZCLIPRANGE_0_1);
	PixoSetBottomUpRendering(PIXO_BOTTOMUPRENDERING_ON);
	PixoSetCullingPolarityToggle(PIXO_CULLINGPOLARITYTOGGLE_OFF);

	// Set the floating-point state to what the rasterizer prefers. SSE underflow
	// can produce a performance slowdown, especially since it produces denormal
	// numbers. If FTZ is set, however, underflowed results are just converted to
	// zeroes, with no performance penalty.
    PIXO_CPU_FEATURES pixo_cpu_features;
    PixoGetCPUIDFeatures(&pixo_cpu_features, 0);

    cpu_features = pixo_cpu_features.features;

#if !PURE_C
    if(cpu_features & PIXO_FEATURE_SSEFP)
	{
		int mscsr_val;
		#if __LINUX_X86__
			__asm__ __volatile__ ("stmxcsr %0" : "=m" (*&mscsr_val));
			mscsr_val |= 0x8000;
			__asm__ __volatile__ ("ldmxcsr %0" : : "m" (*&mscsr_val));
		#else
			__asm stmxcsr [mscsr_val]
			__asm or [mscsr_val], 0x8000
			__asm ldmxcsr [mscsr_val]
		#endif
	}
#endif

	GGPUVendorID = 0xFFFF;
	GGPUDeviceID = 0;

	return 1;
	unguard;
}


//
// UPixoRenderDevice::Exit
//
void UPixoRenderDevice::Exit(UViewport* Viewport)
{
	guard(UPixoRenderDevice::Exit);

	// Shut down RC.
	Flush( Viewport );

	UnSetRes();

	RestoreGamma();

    if(PixoVertexCache)
    {
        free(PixoVertexCache);
        PixoVertexCache = NULL;
        VertexCacheCount = 0;
    }

	PixoShutdown();

	unguard;
}

//
// EnterFullScreen
//
UBOOL EnterFullScreen(TArray<FPlane> Modes, INT NewX, INT NewY, INT ColorBytes,
	DWORD DesiredRefreshRate)
{
#if WIN32
	INT FindX       = NewX,
		FindY       = NewY,
		BestError   = MAXINT;

	for(INT i = 0; i < Modes.Num(); i++ )
	{
		if( Modes(i).Z == ColorBytes*8 )
		{
			INT Error
			=   (Modes(i).X-FindX)*(Modes(i).X-FindX)
			+   (Modes(i).Y-FindY)*(Modes(i).Y-FindY);
			if( Error < BestError )
			{
				NewX        = Modes(i).X;
				NewY        = Modes(i).Y;
				BestError   = Error;
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
		if( ChangeDisplaySettingsA( &DisplayMode, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
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

		if( ChangeDisplaySettings( &DisplayMode, CDS_FULLSCREEN ) != DISP_CHANGE_SUCCESSFUL )
		{
			debugf( TEXT("ChangeDisplaySettings failed: %ix%i"), NewX, NewY );
			return 0;
		}
	}
#endif

	return 1;
}

//
// UPixoRenderDevice::SetRes
//
UBOOL UPixoRenderDevice::SetRes(UViewport* Viewport,INT NewX,INT NewY,UBOOL Fullscreen,INT ColorBytes,UBOOL bSaveSize)
{
	guard(UPixoRenderDevice::SetRes);

	// Returning 0 causes infinite loops and I'd rather catch those bugs at the root.
	if( GIsEditor )
	{
		// Pixo renderer can't run the Editor.
		appErrorf(NAME_FriendlyError,
			TEXT("The Pixomatic renderer doesn't support running the Editor. Please switch to the D3D renderer if you want to use the Editor.") );
	}
	else if( LockedViewport )
		appErrorf(TEXT("Can't change resolution while render device is locked!"));

	debugf(TEXT("Enter SetRes: %dx%d Fullscreen %d"), NewX, NewY, Fullscreen );

	// Unset resolution.
	if( ValidContext )
		UnSetRes();

	if(ColorBytes == 2)
		Use16bit = 1;
	else if(ColorBytes == 4)
		Use16bit = 0;

	Flush( Viewport );

	UseStencil						= 0;
	UseCompressedLightmaps			= 0;
	SupportsCubemaps				= 0;
	SupportsZBIAS					= 1;
	SupportsRenderToTextureRGBA8888	= 1;
	SupportsRenderToTextureRGB565	= 0;
	NumTextureUnits					= 2;

	// Change display settings.
	if( Fullscreen )
	{
		ColorBytes = Use16bit ? 2 : 4;

		if(!EnterFullScreen(Modes, NewX, NewY, ColorBytes, DesiredRefreshRate))
			return 0;

		WasFullscreen = 1;
	}

	// Change window size.

#if USE_SDL12   // Teh Lame.  --ryan.  (SDL 1.2 software video surface -- no GL context)
	UBOOL Result = Viewport->ResizeViewport(
		(Fullscreen ? BLIT_Fullscreen : 0), NewX, NewY );
#elif USE_SDL
	// ufront 2.12 (SDL2/gl4es): PixoDrv presents through the GL window, so request a GL viewport/context.
	UBOOL Result = Viewport->ResizeViewport(
		(Fullscreen ? BLIT_Fullscreen | BLIT_OpenGL : BLIT_OpenGL), NewX, NewY );
#else
	UBOOL Result = Viewport->ResizeViewport(
		(Fullscreen ? BLIT_Fullscreen | BLIT_OpenGL:  BLIT_OpenGL), NewX, NewY );
#endif

	if( !Result )
	{
		if( Fullscreen )
		{
			#if !USE_SDL
			TCHAR_CALL_OS(ChangeDisplaySettings(NULL,0),ChangeDisplaySettingsA(NULL,0));
			#endif
			WasFullscreen = 0;
		}
		return 0;
	}

	ValidContext = 1;

	Viewport->PendingFrame  = 0;

	UpdateGamma( Viewport );

	// ★ ufront 2.58: on the Windows build the viewport is SDL2's, and USDL2Viewport::GetWindow() returns
	// the SDL_Window*, not an HWND. The Linux branch never looks at the handle (PixoBufferOpen maps to the
	// RAD shim's PixoSurface_BufferOpen(w,h,f)), but pixomatic.dll's PixoBufferOpen does: it takes the
	// window's DC and sizes its DIB from GetDeviceCaps(), so a bogus handle yields zeros and the DLL
	// faults with EXCEPTION_INT_DIVIDE_BY_ZERO inside PixoBufferOpen — measured 2026-09-11 under wine,
	// pixomatic.dll+0x1d639, called from this line. Resolve the real HWND through SDL's WM info.
#if defined(_WIN32) && !USE_SDL
	HWND PixoHwnd = NULL;
	{
		SDL_SysWMinfo WmInfo;
		SDL_VERSION(&WmInfo.version);
		if( SDL_GetWindowWMInfo( (SDL_Window*)Viewport->GetWindow(), &WmInfo ) && WmInfo.subsystem == SDL_SYSWM_WINDOWS )
			PixoHwnd = WmInfo.info.win.window;
		else
			debugf( NAME_Warning, TEXT("PixoDrv: SDL_GetWindowWMInfo failed (%s); Pixomatic gets no HWND"), appFromAnsi(SDL_GetError()) );
	}
#else
	HWND PixoHwnd = (HWND)Viewport->GetWindow();
#endif
	if(!CreatePixoBuffers(PixoHwnd, NewX, NewY, PIXO_ZBUFFER_TYPE_16, Zoom2X ? PIXO_BUF_2XZOOM : 0 ))
		return 0;

	return 1;

	unguard;
}


//
// UPixoRenderDevice::UnSetRes
//
void UPixoRenderDevice::UnSetRes()
{
	guard(UPixoRenderDevice::UnSetRes);

	if( WasFullscreen )
	{
		#if !USE_SDL
		TCHAR_CALL_OS(ChangeDisplaySettings(NULL,0),ChangeDisplaySettingsA(NULL,0));
		#endif
		WasFullscreen = 0;
	}

	ValidContext = 0;

	DestroyPixoBuffers();

	unguard;
}


//
//  UPixoRenderDevice::GetCachedResource
//
FPixoResource* UPixoRenderDevice::GetCachedResource(QWORD CacheId)
{
	guard(UPixoRenderDevice::GetCachedResource);

	INT                 HashIndex   = GetResourceHashIndex(CacheId);
	FPixoResource*      ResourcePtr = ResourceHash[HashIndex];

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
//  UPixoRenderDevice::FlushResource
//
void UPixoRenderDevice::FlushResource(QWORD CacheId)
{
	guard(UPixoRenderDevice::GetCachedResource);

	FPixoResource*  CachedResource = GetCachedResource(CacheId);

	if(CachedResource)
		delete CachedResource;

	unguard;
}


//
//  UPixoRenderDevice::GetVertexShader
//
FPixoVertexShader* UPixoRenderDevice::GetVertexShader(EVertexShader Type,FShaderDeclaration& Declaration)
{
	guard(UPixoRenderDevice::GetVertexShader);

	// Find an existing vertex shader with the same type/declaration.
	FPixoVertexShader*  ShaderPtr = VertexShaders;

	while(ShaderPtr)
	{
		if(ShaderPtr->Type == Type && ShaderPtr->Declaration == Declaration)
			return ShaderPtr;

		ShaderPtr = ShaderPtr->NextVertexShader;
	};

	// Create a new vertex shader.
	if(Type == VS_FixedFunction)
		return new FPixoFixedVertexShader(this,Declaration);
	else
		return NULL;

	unguard;
}


//
// UPixoRenderDevice::ResourceCached
//
UBOOL UPixoRenderDevice::ResourceCached(QWORD CacheId)
{
	FPixoResource* Resource = GetCachedResource(CacheId);

	if(!Resource)
		return 0;

	return 1;
}


//
// UPixoRenderDevice::Flush
//
void UPixoRenderDevice::Flush(UViewport* Viewport)
{
	guard(UPixoRenderDevice::Flush);

	DynamicVertexStream = NULL;
	DynamicIndexBuffer  = NULL;

	guard(ResourceList);
	while(ResourceList)
		delete ResourceList;
	unguard;

	guard(VertexShaders);
	while(VertexShaders)
		delete VertexShaders;
	unguard;

	PrecacheOnFlip = 1;

	unguard;
}


//
// UPixoRenderDevice::UpdateGamma
//
void UPixoRenderDevice::UpdateGamma(UViewport* Viewport)
{
	guard(UPixoRenderDevice::UpdateGamma);

	if( ParseParam(appCmdLine(),TEXT("NOGAMMA")) )
		return;

#if !USE_SDL
	HWND hWnd        = (HWND)Viewport->GetWindow();
	check(hWnd);

	HDC hDC         = GetDC(hWnd);
    if(!hDC)
    {
        // This guy is called with an invalid or non existent window handle
        //  at times. The GetDC is failing with ERROR_INVALID_WINDOW_HANDLE
        //  and IsWindow returns false. I guess just return at that point since
        //  you can't really update the gamma of a window that isn't around.
        return;
    }
#endif

	FLOAT   Gamma       = Viewport->GetOuterUClient()->Gamma,
			Brightness  = Viewport->GetOuterUClient()->Brightness,
			Contrast    = Viewport->GetOuterUClient()->Contrast;

	struct
	{
		_WORD red[256];
		_WORD green[256];
		_WORD blue[256];
	} Ramp;

	for(INT i=0; i<256; i++)
	{
		float Value = (Contrast+0.5f)*appPow(i/255.f,1.0f/Gamma)*65535.f +
				(Brightness-0.5f)*32768.f - Contrast*32768.f + 16384.f;

		Ramp.red[i] = Ramp.green[i] = Ramp.blue[i] = Clamp<INT>( appRound( Value ), 0, 65535 );
	}

#if USE_SDL12
	SDL_SetGammaRamp( Ramp.red, Ramp.green, Ramp.blue );
#elif USE_SDL
	// ufront 2.12 (SDL2): per-window gamma ramp (SDL_SetGammaRamp is SDL 1.2-only).
	if( Viewport && Viewport->GetWindow() )
		SDL_SetWindowGammaRamp( (SDL_Window*)Viewport->GetWindow(), Ramp.red, Ramp.green, Ramp.blue );
#elif WIN32
	SetDeviceGammaRamp( hDC, &Ramp );
	ReleaseDC(hWnd, hDC);
#else
#   error Please handle this for your platform.
#endif

	unguard;
}


//
// UPixoRenderDevice::RestoreGamma
//
void UPixoRenderDevice::RestoreGamma()
{
	guard(UPixoRenderDevice::RestoreGamma);

	if( ParseParam(appCmdLine(), TEXT("NOGAMMA")) )
		return;

	struct
	{
		_WORD red[256];
		_WORD green[256];
		_WORD blue[256];
	} Ramp;

	for(INT i=0; i<256; i++)
		Ramp.red[i] = Ramp.green[i] = Ramp.blue[i] = i << 8;

#if USE_SDL12
	SDL_SetGammaRamp( Ramp.red, Ramp.green, Ramp.blue );
#elif USE_SDL
	// ufront 2.12 (SDL2): no window handle here; the per-window gamma resets when the GL window closes.
#elif WIN32
	SetDeviceGammaRamp( GetDC( GetDesktopWindow() ), &Ramp );
#else
#   error Please handle this for your platform.
#endif

	unguard;
}

static const PIXO_ARGB0 ColorTable[] =
{
    0x01009900, // 1-pass: Dark green
    0x0200ff00, // 2-pass: Light green
    0x03999900, // 3-pass: Dark yellow
    0x04ffff00, // 4-pass: Light yellow
    0x05990000, // 5-PASS: Dark red
    0x06ff0000, // 6-PASS: Light red
    0x07ff9999, // 7-PASS: White-red
    0x07ffffff, // 8-PASS: White
};

static const TCHAR *ColorTableDescr[] =
{
    TEXT("1-pass: Dark green"),
    TEXT("2-pass: Light green"),
    TEXT("3-pass: Dark yellow"),
    TEXT("4-pass: Light yellow"),
    TEXT("5-PASS: Dark red"),
    TEXT("6-PASS: Light red"),
    TEXT("7-PASS: White-red"),
    TEXT("8-PASS: White"),
    NULL,
};

//
// UPixoRenderDevice::SetUpPixoBlendModeToShowComplexity
//
void UPixoRenderDevice::SetUpPixoBlendModeToShowComplexity()
{
    static BYTE pixo_rasterop_dst_plus_ten[] =
    {
        0x8B, 0x04, 0x8F,                           // mov eax,dword ptr [edi+ecx*4]
        0xC1, 0xE8, 0x18,                           // shr eax,18h
        0x8B, 0x04, 0x85, 0xD4, 0xC6, 0x0D, 0x03,   // mov eax,dword ptr [eax*4+30DC6D4h]
        0x89, 0x04, 0x8F,                           // mov dword ptr [edi+ecx*4],eax
    };

    *(DWORD *)(pixo_rasterop_dst_plus_ten + 9) = (DWORD)&ColorTable[0];

    if(UPixoRenderDevice::ShowDepthComplexity == 1)
        PixoSetWStateZCompare(PIXO_ZCOMPARE_ALWAYS);

    PixoSetUserRasteropCode(pixo_rasterop_dst_plus_ten,
        sizeof(pixo_rasterop_dst_plus_ten));
    PixoSetWStateRasterop(PIXO_RASTEROP_USER_CODE);
}

//
// UPixoRenderDevice::Exec
//
UBOOL UPixoRenderDevice::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	guard(UPixoRenderDevice::Exec);

    if(ParseCommand(&Cmd, TEXT("PIXO DEPTH")))
    {
        static const TCHAR *Descr[] = { TEXT("off"), TEXT("zalways"), TEXT("zle") };

        ShowDepthComplexity++;
        if(ShowDepthComplexity > 2)
            ShowDepthComplexity = 0;
        Ar.Logf( TEXT("ShowDepthComplexity '%s'."), Descr[ShowDepthComplexity] );

        if(ShowDepthComplexity == 1)
        {
            const TCHAR **TableDescr = ColorTableDescr;
            while(*TableDescr)
                Ar.Logf( TEXT("    '%s'."), *TableDescr++ );
        }

        return 1;
    }
	else
	if( ParseCommand(&Cmd, TEXT("RESOURCES") ))
	{
		// Calculate resource usage
		INT StatTextureBytes=0, StatVertexStreamBytes=0, StatIndexBufferBytes=0, StatOtherBytes=0;
		INT StatNumTextures=0, StatNumVertexStreams=0, StatNumIndexBuffers=0, StatNumOther=0;
		for( FPixoResource* Resource = ResourceList;Resource;Resource = Resource->NextResource )
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

		Ar.Logf( TEXT("Pixo Resource Usage"));
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

	return 0;

	unguard;
}

static int PixoDrawTriangleCallback(const PIXO_VERTEX *verts)
{
    GStats.DWORDStats(UPixoRenderDevice::STATS_NumTrianglesDrawn)++;
    return 1;
}

//
// UPixoRenderDevice::Lock
//
FRenderInterface* UPixoRenderDevice::Lock(UViewport* Viewport,BYTE* InHitData,INT* InHitSize)
{
	guard(UPixoRenderDevice::Lock);

	// Hack to disable certain features that won't render correctly.
	if( UTexture::__Client )
	{
		UTexture::__Client->Projectors		= 0;
		UTexture::__Client->NoDynamicLights = 1;
		UTexture::__Client->Decals			= 0;
		UTexture::__Client->DecoLayers		= 0;
		UTexture::__Client->Coronas			= 0;
	}

	// Only set up the triangle callback if stats are on as it does affect
	//  performance a bit.
	UBOOL NeedCallback = GIsClocking;

	PixoSetTriangleCallback(NeedCallback ? PixoDrawTriangleCallback : NULL);

	PixoBufferLock(PixoBuffer, 0);

	// set framebuffer data
	PixoSetFrameBuffer(PixoBuffer->Buffer, PixoBuffer->BufferPitch);

	// set zbuffer information
	PixoSetZBuffer(ZBuffer, ZBufferPitch, PixoGetZBufferType());

	if( Zoom2X )
		PixoSetViewport(0, 0, Viewport->SizeX/2, Viewport->SizeY/2);
	else
		PixoSetViewport(0, 0, Viewport->SizeX, Viewport->SizeY);

    if(ShowDepthComplexity)
        PixoClearViewport(PIXO_CLEARFRAMEBUFFER, 0x00000000, 0, 0);

	FrameCounter++;

	check( Viewport );
	LockedViewport = Viewport;

	// Create the render interface.
	RenderInterface.Locked( Viewport, InHitData, InHitSize );

	return &RenderInterface;

	unguard;
}


//
// UPixoRenderDevice::Unlock
//
void UPixoRenderDevice::Unlock(FRenderInterface* RI)
{
	guard(UPixoRenderDevice::Unlock);

	PixoBufferUnlock(PixoBuffer);
	RenderInterface.Unlocked();
	LockedViewport = NULL;

	unguard;
}


#if USE_SDL && !USE_SDL12
// ===== ufront 2.12: SDL2 GL present path for the Pixomatic software renderer (gl4es backend) =====
#include <stdlib.h>
#include "GL/gl.h"
#include "GL/glext.h"
#include "gl4esinit.h"

// GL4ES must be initialized before any gl* call. GL4ESRenderDevice does this in its SetRes; but when the
// software renderer is the ACTIVE device, that path never runs -- so initialize here once, mirroring it.
static void PixoEnsureGL4ES()
{
	static UBOOL Inited = 0;
	if( Inited ) return;
	Inited = 1;
	setenv( "LIBGL_ES", "2", 1 );
	setenv( "LIBGL_NOBANNER", "1", 1 );
	setenv( "LIBGL_NPOT", "2", 1 );
	setenv( "LIBGL_SILENTSTUB", "1", 1 );
	set_getprocaddress( SDL_GL_GetProcAddress );
	initialize_gl4es();
}

static GLuint GPixoPresentTex = 0;
static INT    GPixoPresentW   = 0;
static INT    GPixoPresentH   = 0;

// Upload the Pixomatic ARGB8888 software framebuffer to a GL texture and draw it as a fullscreen quad,
// then swap. Fixed-function GL (GL4ES translates to GLES2). The buffer is top-down, so V is flipped.
static void PixoPresentGL( SDL_Window* Window, const void* Pixels, INT W, INT H, INT PitchBytes )
{
	if( !Window || !Pixels || W <= 0 || H <= 0 )
		return;
	PixoEnsureGL4ES();

	if( !GPixoPresentTex )
	{
		glGenTextures( 1, &GPixoPresentTex );
		glBindTexture( GL_TEXTURE_2D, GPixoPresentTex );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
		glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
	}
	glBindTexture( GL_TEXTURE_2D, GPixoPresentTex );
	glPixelStorei( GL_UNPACK_ALIGNMENT, 1 );
#ifdef GL_UNPACK_ROW_LENGTH
	glPixelStorei( GL_UNPACK_ROW_LENGTH, PitchBytes / 4 );   // ARGB8888 == 4 bytes/pixel
#endif
	if( W != GPixoPresentW || H != GPixoPresentH )
	{
		glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_BGRA, GL_UNSIGNED_BYTE, Pixels );
		GPixoPresentW = W;
		GPixoPresentH = H;
	}
	else
	{
		glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, W, H, GL_BGRA, GL_UNSIGNED_BYTE, Pixels );
	}
#ifdef GL_UNPACK_ROW_LENGTH
	glPixelStorei( GL_UNPACK_ROW_LENGTH, 0 );
#endif

	// The framebuffer we're handed here is already the full window-resolution composited image (see Present:
	// with Zoom2X the caller locks with PIXO_BUF_LOCK_COMPOSITE, which 2x-zooms the half-res 3D scene and
	// overlays the full-res HUD). So W x H == the window size; a straight fullscreen quad is correct.
	glViewport( 0, 0, W, H );
	glDisable( GL_DEPTH_TEST );
	glDisable( GL_CULL_FACE );
	glDisable( GL_BLEND );
	glDisable( GL_LIGHTING );
	glEnable( GL_TEXTURE_2D );
	glMatrixMode( GL_PROJECTION ); glPushMatrix(); glLoadIdentity();
	glMatrixMode( GL_MODELVIEW  ); glPushMatrix(); glLoadIdentity();
	glColor4f( 1.f, 1.f, 1.f, 1.f );
	glBegin( GL_TRIANGLE_STRIP );
		glTexCoord2f( 0.f, 1.f ); glVertex3f( -1.f, -1.f, 0.f );
		glTexCoord2f( 1.f, 1.f ); glVertex3f(  1.f, -1.f, 0.f );
		glTexCoord2f( 0.f, 0.f ); glVertex3f( -1.f,  1.f, 0.f );
		glTexCoord2f( 1.f, 0.f ); glVertex3f(  1.f,  1.f, 0.f );
	glEnd();
	glMatrixMode( GL_PROJECTION ); glPopMatrix();
	glMatrixMode( GL_MODELVIEW  ); glPopMatrix();
	glDisable( GL_TEXTURE_2D );

	SDL_GL_SwapWindow( Window );
}
#endif // USE_SDL && !USE_SDL12

//
// UPixoRenderDevice::Present
//
void UPixoRenderDevice::Present(UViewport* Viewport)
{
	guard(UPixoRenderDevice::Present);

#if USE_SDL12
    // !!! FIXME: cache the screen surface pointer somewhere...  --ryan.
    SDL_Surface *screen = SDL_GetVideoSurface();

    // !!! FIXME: Don't recalculate the blit function every frame! --ryan.
	PIXOSURFACE_BLITFUNC *BlitFunc = GetPixoBlitFunc(
	                                        screen->format->BitsPerPixel,
	                                        screen->format->Amask,
	                                        screen->format->Rmask,
	                                        screen->format->Gmask,
	                                        screen->format->Bmask);

	if (SDL_MUSTLOCK(screen)) SDL_LockSurface(screen);
	PixoSurface_BufferBlitDest(PixoBuffer, screen->pixels, 0, 0,
	                            screen->pitch, BlitFunc);
	if (SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);
	SDL_Flip(screen);
#elif USE_SDL
	// ufront 2.12 (SDL2 / gl4es): SDL2 has no software video surface to blit+flip on a GL window, so present
	// the Pixomatic software framebuffer THROUGH GL -- upload the ARGB8888 buffer to a texture and draw a
	// fullscreen quad, then SDL_GL_SwapWindow. GL4ES translates the fixed-function calls to GLES2.
	// With Zoom2X the 3D scene is rendered at half-res and the HUD at full-res into separate layers; lock with
	// PIXO_BUF_LOCK_COMPOSITE so Pixomatic 2x-zooms the 3D + overlays the HUD into one full window-resolution
	// image (mirrors ReadPixels + the SDL 1.2 present). Without it we'd upload the raw half-res layer (the
	// scene rendered into a corner). The composited image is Viewport->SizeX x SizeY (BufferWidth/Height report
	// the internal layer size), so present those dims with the buffer pitch.
	PixoBufferLock( PixoBuffer, Zoom2X ? PIXO_BUF_LOCK_COMPOSITE : 0 );
	PixoPresentGL( (SDL_Window*)Viewport->GetWindow(),
	               PixoBuffer->Buffer, Viewport->SizeX, Viewport->SizeY, PixoBuffer->BufferPitch );
	PixoBufferUnlock( PixoBuffer );
#else
	PixoBufferBlit(PixoBuffer, NULL, 0, 0);
#endif

	unguard;
}


//
// UPixoRenderDevice::ReadPixels
//
void UPixoRenderDevice::ReadPixels(UViewport* Viewport,FColor* Pixels,UBOOL Flipped)
{
	guard(UPixoRenderDevice::ReadPixels);

	if( Viewport && Pixels && PixoBuffer )
	{
		void *Source = PixoBuffer->Buffer;
		INT SourcePitch = PixoBuffer->BufferPitch;

		if(!Source)
		{
			PixoBufferLock(PixoBuffer, Zoom2X ? PIXO_BUF_LOCK_COMPOSITE : 0 );
			Source = PixoBuffer->Buffer;
			SourcePitch = PixoBuffer->BufferPitch;
			PixoBufferUnlock(PixoBuffer);
		}

		PixoBlitARGB8888toARGB8888(
			Pixels, 0, 0, Viewport->SizeX * 4,
			Source, 0, 0, Viewport->SizeX, Viewport->SizeY, SourcePitch);

		if( Flipped )
			for( INT i=0; i<Viewport->SizeY/2; i++ )
				for( INT j=0; j<Viewport->SizeX; j++ )
					Exchange( Pixels[j+i*Viewport->SizeX], Pixels[j+(Viewport->SizeY-1-i)*Viewport->SizeX] );

	}


	unguard;
}


//
// UPixoRenderDevice::GetRenderCaps
//
FRenderCaps* UPixoRenderDevice::GetRenderCaps()
{
	guard(UPixoRenderDevice::GetRenderCaps);

	RenderCaps.HardwareTL                   = 0;
	RenderCaps.MaxSimultaneousTerrainLayers = 1;
	RenderCaps.PixelShaderVersion           = 0;
	return &RenderCaps;

	unguard;
}

UBOOL UPixoRenderDevice::AllocAndSetPixoVertexCache(int VerticesCount)
{
    if(VertexCacheCount < VerticesCount)
    {
        free(PixoVertexCache);

        VertexCacheCount = VerticesCount;
        PixoVertexCache = (PIXO_VERT_CACHE *)malloc(VertexCacheCount * sizeof(PIXO_VERT_CACHE));
        if(!PixoVertexCache)
            VertexCacheCount = 0;
    }

    PixoSetVertexCache(PixoVertexCache, VertexCacheCount);
    return !!VertexCacheCount;
}

void UPixoRenderDevice::DestroyPixoBuffers()
{
	if(PixoBuffer)
	{
		PixoBufferClose(PixoBuffer);
		PixoBuffer = 0;
	}

#if WIN32
	VirtualFree(ZBufferBlock, 0, MEM_RELEASE);
#else
    free(ZBufferBlock);
#endif

	ZBufferBlock = ZBuffer = 0;
}

UBOOL UPixoRenderDevice::CreatePixoBuffers(HWND hwnd,
	UINT BufferWidth, UINT BufferHeight,
	PIXO_ZBUFFER_TYPE ZBufferType, int flags)
{

	// make sure we're not leaking anything
	DestroyPixoBuffers();

	// Try opening our main pixo buffer.
	PIXO_BUF *pbuf = PixoBufferOpen(hwnd, BufferWidth, BufferHeight, flags);

	if(pbuf)
	{
		int ZBufferPad;
		int ZBufferSize;

		PixoBuffer = pbuf;

		// Lock the buffer to see what width / height it actually is. For
		// example, if we're in antialias mode our z buffer needs to be
		// BufferWidth * 2, BufferHeight * 2.
		PixoBufferLock(pbuf, 0);

		// From the Intel(r) Pentium(r) 4 and Intel(r) Xeon(tm) Processor
		// Optimization Reference Manual, Order Number: 248966-04
		//
		// 64K aliasing for data - first-level cache. If a reference (load or
		// store) occurs that has bits 0-15 of the linear address, which are
		// identical to a reference (load or store) which is under way, then the
		// second reference cannot begin until the first one is kicked out of the
		// cache. Avoiding this kind of aliasing can lead to a factor of three
		// speedup.
		//
		// User/Source Coding Rule 5. (M impact, M generality) When padding
		// variable declarations to avoid aliasing, the greatest benefit comes
		// from avoiding aliasing on second-level cache lines, suggesting an
		// offset of 128 bytes or more.
		if(ZBufferType == PIXO_ZBUFFER_TYPE_16)
		{
			ZBufferSize = pbuf->BufferWidth * pbuf->BufferHeight * sizeof(unsigned short);
			ZBufferPad = 0;
			ZBufferPitch = pbuf->BufferWidth * sizeof(unsigned short);
		}
		else
		{
			// For 24-bit and 32-bit z buffers we pad that bugger out to avoid
			// running into 64k aliasing conflicts with the framebuffer
			ZBufferSize = pbuf->BufferWidth * pbuf->BufferHeight * sizeof(unsigned int);
			ZBufferPad = ((unsigned int)pbuf->Buffer + 512) & 1023;
			ZBufferPitch = pbuf->BufferWidth * sizeof(unsigned int);
		}

		// Allocate additional memory is we're goint to use composition.
		if( flags & PIXO_BUF_2XZOOM )
			ZBufferSize *= 4;

		// alloc our zbuffer block
#if WIN32
		ZBufferBlock = (unsigned short *)VirtualAlloc(NULL,
			ZBufferSize + ZBufferPad, MEM_COMMIT, PAGE_READWRITE);
#else
        ZBufferBlock = (unsigned short *)malloc(ZBufferSize + ZBufferPad);
#endif
		// if it succeeded, calculate our offset ZBuffer pointer
		if(ZBufferBlock)
			ZBuffer = (char *)ZBufferBlock + ZBufferPad;

		// Unlock the buffer
		PixoBufferUnlock(pbuf);
	}

	// error handling
	if(!pbuf || !ZBuffer)
	{
		DestroyPixoBuffers();
		return 0;
	}

	return 1;
}

void PixoTransformVector(float *out_xyzw, float *mat, float *in_xyz1)
{
    check(!((DWORD)mat & 0xf));

#if !PURE_C
    if(UPixoRenderDevice::cpu_features & PIXO_FEATURE_SSEFP)
    {
        __asm
        {
            mov eax, dword ptr [in_xyz1]
            mov ecx, dword ptr [mat]

            movss xmm0, dword ptr [eax]     // 0 | 0 | 0 | x
            shufps xmm0, xmm0, 0            // x | x | x | x

            movss xmm1, dword ptr [eax+4]   // 0 | 0 | 0 | y
            shufps xmm1, xmm1, 0            // y | y | y | y

            movss xmm2, dword ptr [eax+8]   // 0 | 0 | 0 | z
            shufps xmm2, xmm2, 0            // z | z | z | z

            mulps xmm0, [ecx]               // x*mat[0][0] | x*mat[0][1] | x*mat[0][2] | x*mat[0][3]
            addps xmm0, [ecx+48]            // + mat[3][0] |   mat[3][1] |   mat[3][2] |   mat[3][3]

            mulps xmm1, [ecx+16]            // y*mat[1][0] | y*mat[1][1] | y*mat[1][2] | y*mat[1][3]
            addps xmm0, xmm1

            mulps xmm2, [ecx+32]            // z*mat[2][0] | z*mat[2][1] | z*mat[2][2] | z*mat[2][3]
            addps xmm0, xmm2

            mov ecx, dword ptr [out_xyzw]
            movups [ecx], xmm0              // w | z | y | x
        }
    }
    else if(UPixoRenderDevice::cpu_features & PIXO_FEATURE_3DNOW)
    {
        __asm
        {
            mov eax, dword ptr [in_xyz1]
            mov ecx, dword ptr [mat]

            movd mm0, dword ptr [eax]       // x
            punpckldq mm0, mm0
            movq mm4, mm0

            movd mm1, dword ptr [eax+4]     // y
            punpckldq mm1, mm1
            movq mm5, mm1

            movd mm2, dword ptr [eax+8]     // z
            punpckldq mm2, mm2
            movq mm6, mm2

            pfmul mm0, [ecx]
            pfadd mm0, [ecx+48]

            pfmul mm1, [ecx+16]
            pfadd mm0, mm1

            pfmul mm2, [ecx+32]
            pfadd mm0, mm2

            pfmul mm4, [ecx+8]
            pfadd mm4, [ecx+48+8]

            pfmul mm5, [ecx+16+8]
            pfadd mm4, mm5

            pfmul mm6, [ecx+32+8]
            pfadd mm4, mm6

            mov ecx, dword ptr [out_xyzw]
            movq [ecx], mm0                 // y | x
            movq [ecx+8], mm4               // w | z

            femms
        }
    }
    else
#endif // !PURE_C
    {
        float _in[3] = { in_xyz1[0], in_xyz1[1], in_xyz1[2] };

        out_xyzw[0] = _in[0] * mat[0*4+0] + _in[1] * mat[1*4+0] + _in[2] * mat[2*4+0] + mat[3*4+0];
        out_xyzw[1] = _in[0] * mat[0*4+1] + _in[1] * mat[1*4+1] + _in[2] * mat[2*4+1] + mat[3*4+1];
        out_xyzw[2] = _in[0] * mat[0*4+2] + _in[1] * mat[1*4+2] + _in[2] * mat[2*4+2] + mat[3*4+2];
        out_xyzw[3] = _in[0] * mat[0*4+3] + _in[1] * mat[1*4+3] + _in[2] * mat[2*4+3] + mat[3*4+3];
    }
}


void PixoTransformNormal(float *out_xyzw, float *mat, float *in_xyz0)
{
    check(!((DWORD)mat & 0xf));

#if !PURE_C
    if(UPixoRenderDevice::cpu_features & PIXO_FEATURE_SSEFP)
    {
        __asm
        {
            mov eax, dword ptr [in_xyz0]
            mov ecx, dword ptr [mat]

            movss xmm0, dword ptr [eax]     // 0 | 0 | 0 | x
            shufps xmm0, xmm0, 0            // x | x | x | x

            movss xmm1, dword ptr [eax+4]   // 0 | 0 | 0 | y
            shufps xmm1, xmm1, 0            // y | y | y | y

            movss xmm2, dword ptr [eax+8]   // 0 | 0 | 0 | z
            shufps xmm2, xmm2, 0            // z | z | z | z

            mulps xmm0, [ecx]               // x*mat[0][0] | x*mat[0][1] | x*mat[0][2] | x*mat[0][3]
            //addps xmm0, [ecx+48]            // + mat[3][0] |   mat[3][1] |   mat[3][2] |   mat[3][3]

            mulps xmm1, [ecx+16]            // y*mat[1][0] | y*mat[1][1] | y*mat[1][2] | y*mat[1][3]
            addps xmm0, xmm1

            mulps xmm2, [ecx+32]            // z*mat[2][0] | z*mat[2][1] | z*mat[2][2] | z*mat[2][3]
            addps xmm0, xmm2

            mov ecx, dword ptr [out_xyzw]
            movups [ecx], xmm0              // w | z | y | x
        }
    }
    else if(UPixoRenderDevice::cpu_features & PIXO_FEATURE_3DNOW)
    {
        __asm
        {
            mov eax, dword ptr [in_xyz0]
            mov ecx, dword ptr [mat]

            movd mm0, dword ptr [eax]       // x
            punpckldq mm0, mm0
            movq mm4, mm0

            movd mm1, dword ptr [eax+4]     // y
            punpckldq mm1, mm1
            movq mm5, mm1

            movd mm2, dword ptr [eax+8]     // z
            punpckldq mm2, mm2
            movq mm6, mm2

            pfmul mm0, [ecx]
            //pfadd mm0, [ecx+48]

            pfmul mm1, [ecx+16]
            pfadd mm0, mm1

            pfmul mm2, [ecx+32]
            pfadd mm0, mm2

            pfmul mm4, [ecx+8]
            //pfadd mm4, [ecx+48+8]

            pfmul mm5, [ecx+16+8]
            pfadd mm4, mm5

            pfmul mm6, [ecx+32+8]
            pfadd mm4, mm6

            mov ecx, dword ptr [out_xyzw]
            movq [ecx], mm0                 // y | x
            movq [ecx+8], mm4               // w | z

            femms
        }
    }
    else
#endif // !PURE_C
    {
        float _in[3] = { in_xyz0[0], in_xyz0[1], in_xyz0[2] };

        out_xyzw[0] = _in[0] * mat[0*4+0] + _in[1] * mat[1*4+0] + _in[2] * mat[2*4+0];// + mat[3*4+0];
        out_xyzw[1] = _in[0] * mat[0*4+1] + _in[1] * mat[1*4+1] + _in[2] * mat[2*4+1];// + mat[3*4+1];
        out_xyzw[2] = _in[0] * mat[0*4+2] + _in[1] * mat[1*4+2] + _in[2] * mat[2*4+2];// + mat[3*4+2];
        out_xyzw[3] = _in[0] * mat[0*4+3] + _in[1] * mat[1*4+3] + _in[2] * mat[2*4+3];// + mat[3*4+3];
    }
}


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

