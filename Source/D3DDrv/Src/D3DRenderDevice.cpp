/*=============================================================================
	D3DRenderDevice.cpp: Unreal Direct3D render device implementation.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "D3DDrv.h"

IMPLEMENT_CLASS(UD3DRenderDevice);

//
// Voodoo 3 support globals.
//
D3DTEXTUREOP DUMMY_MODULATE2X = D3DTOP_MODULATE2X;

//
//	Definitions.
//
#define HIT_SIZE 8
#define HIT_COLOR 0xfe0d

//
//	UD3DRenderDevice::UD3DRenderDevice
//
UD3DRenderDevice::UD3DRenderDevice() :
	RenderInterface(this)
{
}

//
//	UD3DRenderDevice::StaticConstructor
//
void UD3DRenderDevice::StaticConstructor()
{
	guard(UD3DRenderDevice::StaticConstructor);

	new(GetClass(),TEXT("UseHardwareTL"),		RF_Public)UBoolProperty ( CPP_PROPERTY(UseHardwareTL       ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("UseHardwareVS"),		RF_Public)UBoolProperty ( CPP_PROPERTY(UseHardwareVS       ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("UsePrecaching"),		RF_Public)UBoolProperty ( CPP_PROPERTY(UsePrecaching       ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("UseTrilinear"),        RF_Public)UBoolProperty ( CPP_PROPERTY(UseTrilinear        ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("UseVSync"),            RF_Public)UBoolProperty ( CPP_PROPERTY(UseVSync            ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("UseTripleBuffering"),	RF_Public)UBoolProperty ( CPP_PROPERTY(UseTripleBuffering  ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("UseCubemaps"),			RF_Public)UBoolProperty ( CPP_PROPERTY(UseCubemaps		   ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("ReduceMouseLag"),      RF_Public)UBoolProperty ( CPP_PROPERTY(ReduceMouseLag      ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("UseNPatches"),			RF_Public)UBoolProperty ( CPP_PROPERTY(UseNPatches		   ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("CheckForOverflow"),	RF_Public)UBoolProperty ( CPP_PROPERTY(CheckForOverflow	   ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("DecompressTextures"),	RF_Public)UBoolProperty ( CPP_PROPERTY(DecompressTextures  ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("AvoidHitches"),		RF_Public)UBoolProperty ( CPP_PROPERTY(AvoidHitches        ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("ReportUnusedTextures"),RF_Public)UBoolProperty ( CPP_PROPERTY(ReportUnusedTextures), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("OverrideDesktopRefreshRate"),	RF_Public)UBoolProperty ( CPP_PROPERTY(OverrideDesktopRefreshRate ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("AdapterNumber"),       RF_Public)UIntProperty  ( CPP_PROPERTY(AdapterNumber       ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("MaxPixelShaderVersion"),		RF_Public)UIntProperty  ( CPP_PROPERTY(MaxPixelShaderVersion      ), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("DesiredRefreshRate"),	RF_Public)UIntProperty  ( CPP_PROPERTY(DesiredRefreshRate	), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("LevelOfAnisotropy"),	RF_Public)UIntProperty  ( CPP_PROPERTY(LevelOfAnisotropy	), TEXT("Options"), CPF_Config );
    new(GetClass(),TEXT("DetailTexMipBias"),    RF_Public)UFloatProperty( CPP_PROPERTY(DetailTexMipBias	), TEXT("Options"), CPF_Config );
    new(GetClass(),TEXT("DefaultTexMipBias"),   RF_Public)UFloatProperty( CPP_PROPERTY(DefaultTexMipBias	), TEXT("Options"), CPF_Config );
	new(GetClass(),TEXT("TesselationFactor"),   RF_Public)UFloatProperty( CPP_PROPERTY(TesselationFactor	), TEXT("Options"), CPF_Config );

	GIsPixomatic					= 0;
	GIsOpenGL						= 0;

	// Assume support until we know better.
	SupportsZBIAS					= 1;
	SupportsCubemaps				= 1;
	SupportsRenderToTextureRGBA8888	= 1;
	SupportsRenderToTextureRGB565	= 1;
	SupportsRenderToTextureNPOT		= 1;

	unguard;
}

//
//	UD3DRenderDevice::GetCachedResource
//
FD3DResource* UD3DRenderDevice::GetCachedResource(QWORD CacheId)
{
	guard(UD3DRenderDevice::GetCachedResource);

	INT				HashIndex = GetResourceHashIndex(CacheId);
	FD3DResource*	ResourcePtr = ResourceHash[HashIndex];

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
//	UD3DRenderDevice::FlushResource
//
void UD3DRenderDevice::FlushResource(QWORD CacheId)
{
	guard(UD3DRenderDevice::GetCachedResource);

	FD3DResource*	CachedResource = GetCachedResource(CacheId);

	if(CachedResource)
		delete CachedResource;

	unguard;
}

//
//	UD3DRenderDevice::GetVertexShader
//
FD3DVertexShader* UD3DRenderDevice::GetVertexShader(EVertexShader Type,FShaderDeclaration& Declaration)
{
	guard(UD3DRenderDevice::GetVertexShader);

	// Find an existing vertex shader with the same type/declaration.
	FD3DVertexShader*	ShaderPtr = VertexShaders;

	while(ShaderPtr)
	{
		if(ShaderPtr->Type == Type && ShaderPtr->Declaration == Declaration)
			return ShaderPtr;

		ShaderPtr = ShaderPtr->NextVertexShader;
	};

	// Create a new vertex shader.
	if(Type == VS_FixedFunction)
		return new FD3DFixedVertexShader(this,Declaration);
	else
		return NULL;

	unguard;
}

//
//	UD3DRenderDevice::Init
//
UBOOL UD3DRenderDevice::Init()
{
	guard(UD3DRenderDevice::Init);

	GIsOpenGL = 0;

	HRESULT	Result;

	// Memory overflow detection.
	if( CheckForOverflow )
	{
		StaticBuffer.Empty();
		StaticBuffer.Add( 512 * 1024 );
	}

	HWND hwndDesktop = GetDesktopWindow();
	HDC  hdcDesktop  = GetDC(hwndDesktop);
	DesktopColorBits = GetDeviceCaps( hdcDesktop, BITSPIXEL );

	if(!Direct3D8 || !Direct3DDevice8)
	{
		// Create the Direct3D object.
		guard(CreateDirect3D);

		Direct3D8 = Direct3DCreate8(D3D_SDK_VERSION);

		if(!Direct3D8)
			appErrorf(NAME_FriendlyError, TEXT("Please install DirectX 8.1b or later (see Release Notes for instructions on how to obtain it)"));

		unguard;

		// Enumerate Direct3D adapters.
		guard(EnumAdapters);

		INT	NumAdapters = Direct3D8->GetAdapterCount();

		Adapters.Empty(NumAdapters);

		debugf(NAME_Init,TEXT("Direct3D adapters detected:"));

		for(INT Index = 0;Index < NumAdapters;Index++)
		{
			D3DADAPTER_IDENTIFIER8	AdapterIdentifier;

			Result = Direct3D8->GetAdapterIdentifier(Index,D3DENUM_NO_WHQL_LEVEL,&AdapterIdentifier);

			if( FAILED(Result) )
				appErrorf(TEXT("GetAdapterIdentifier failed(%s)."),*D3DError(Result));

			debugf(TEXT("	%s/%s"),appFromAnsi(AdapterIdentifier.Driver),appFromAnsi(AdapterIdentifier.Description));

			Adapters.AddItem(AdapterIdentifier);
		}

		if(!Adapters.Num())
			appErrorf(TEXT("No Direct3D adapters found."));

		unguard;
	}

	// Find best Direct3D adapter.
	BestAdapter = D3DADAPTER_DEFAULT;

#ifndef _XBOX
	guard(FindBestAdapter);
	//!!vogel: I assume this is a relic from 3dfx days.
	//for(INT Index = 0;Index < Adapters.Num();Index++)
	//	if(appStrstr(appFromAnsi(Adapters(Index).Description),TEXT("Primary")))
	//		BestAdapter = Index;
	if ( (AdapterNumber>=0) && (AdapterNumber < Adapters.Num()) )
		BestAdapter = AdapterNumber;
	unguard;
#endif

	// Get the Direct3D caps for the best adapter.
	guard(GetDeviceCaps);
	Result = Direct3D8->GetDeviceCaps(BestAdapter,D3DDEVTYPE_HAL,&DeviceCaps8);
	if( FAILED(Result) )
		appErrorf(NAME_FriendlyError, TEXT("Please enable Direct3D acceleration. You can do this by starting dxdiag and enabling Direct3D Acceleration in the Display1/2 tab after installing DirectX 8.1b (or later) and the latest drivers for your graphics card."));
//		appErrorf(TEXT("GetDeviceCaps failed(%s) on adapter %i."),*D3DError(Result), BestAdapter);
	unguard;

	// Check device caps.
	guard(CheckDeviceCaps);

	// Check multitexture caps.
	debugf(NAME_Init,TEXT("D3D Driver: MaxTextureBlendStages=%i"),DeviceCaps8.MaxTextureBlendStages);
	debugf(NAME_Init,TEXT("D3D Driver: MaxSimultaneousTextures=%i"),DeviceCaps8.MaxSimultaneousTextures);
	
#ifndef _XBOX
	// Use software pipeline if less than 8 streams are exposed.
	if( DeviceCaps8.MaxStreams < 8 )
		UseHardwareTL = 0;

	// Software pipeline lies about caps.
	if( !(DeviceCaps8.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) || !UseHardwareTL )
	{
		DeviceCaps8.MaxStreams		= 16;
		DeviceCaps8.MaxActiveLights	= 8;
	}
#endif
	
	debugf(NAME_Init,TEXT("D3D Driver: MaxActiveLights=%u"),DeviceCaps8.MaxActiveLights);
	debugf(NAME_Init,TEXT("D3D Driver: MaxPrimitiveCount=%u"),DeviceCaps8.MaxPrimitiveCount);
	debugf(NAME_Init,TEXT("D3D Driver: MaxVertexIndex=%u"),DeviceCaps8.MaxVertexIndex);
	debugf(NAME_Init,TEXT("D3D Driver: MaxStreams=%u"),DeviceCaps8.MaxStreams);
	debugf(NAME_Init,TEXT("D3D Driver: MaxStreamStride=%u"),DeviceCaps8.MaxStreamStride);
	debugf(NAME_Init,TEXT("D3D Driver: VertexShaderVersion=%u.%u"),(DeviceCaps8.VertexShaderVersion & 0xFF00) >> 8, DeviceCaps8.VertexShaderVersion & 0xFF );
	debugf(NAME_Init,TEXT("D3D Driver: PixelShaderVersion=%u.%u"),((DeviceCaps8.PixelShaderVersion & 0xFF00) >> 8),DeviceCaps8.PixelShaderVersion & 0xFF );

	// Disable stencil with 16 bit rendering.
	UseStencil &= !Use16bit || GIsEditor;

	// Check whether AGP is enabled (major slowdown if not).
	if (DeviceCaps8.DevCaps & D3DDEVCAPS_TEXTURENONLOCALVIDMEM)
		debugf( NAME_Init,TEXT("D3D Driver: AGP support detected"));
	else
		debugf( NAME_Init,TEXT("D3D Driver: WARNING : no AGP support detected"));

	// Calculate pixel shader version.
	PixelShaderVersion = ((DeviceCaps8.PixelShaderVersion & 0xFF00) >> 8) * 10 + DeviceCaps8.PixelShaderVersion & 0xFF;
	PixelShaderVersion = Min(PixelShaderVersion,MaxPixelShaderVersion);

	// Check for ZBIAS support - e.g. TNT2 doesn't support it.
	SupportsZBIAS = DeviceCaps8.RasterCaps & D3DPRASTERCAPS_ZBIAS;

	// Check for cubemap support.
	if( DeviceCaps8.TextureCaps & D3DPTEXTURECAPS_CUBEMAP )
		SupportsCubemaps = UseCubemaps;
	else
		SupportsCubemaps = 0;

	// Check for mipped cubemap support.
	if( DeviceCaps8.TextureCaps & D3DPTEXTURECAPS_MIPCUBEMAP ) 
		UseMippedCubemaps = 1;
	else
		UseMippedCubemaps = 0;

	// Check for DXT1/3/5 support.
	UseDXT1 = SUCCEEDED(Direct3D8->CheckDeviceFormat(BestAdapter,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,0,D3DRTYPE_TEXTURE,D3DFMT_DXT1));
	UseDXT3 = SUCCEEDED(Direct3D8->CheckDeviceFormat(BestAdapter,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,0,D3DRTYPE_TEXTURE,D3DFMT_DXT3));
	UseDXT5 = SUCCEEDED(Direct3D8->CheckDeviceFormat(BestAdapter,D3DDEVTYPE_HAL,D3DFMT_X8R8G8B8,0,D3DRTYPE_TEXTURE,D3DFMT_DXT5));

	// Check for render-to-texture support.
	SupportsRenderToTextureRGBA8888 = SUCCEEDED(Direct3D8->CheckDeviceFormat(BestAdapter,D3DDEVTYPE_HAL,Use16bit ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8,D3DUSAGE_RENDERTARGET,D3DRTYPE_TEXTURE,D3DFMT_A8R8G8B8));
	SupportsRenderToTextureRGB565	= SUCCEEDED(Direct3D8->CheckDeviceFormat(BestAdapter,D3DDEVTYPE_HAL,Use16bit ? D3DFMT_R5G6B5 : D3DFMT_X8R8G8B8,D3DUSAGE_RENDERTARGET,D3DRTYPE_TEXTURE,D3DFMT_R5G6B5  ));

	// Override DXT support.
	if( DecompressTextures )
		UseDXT1 = UseDXT3 = UseDXT5 = 0;

	if( !UseDXT1 )
		debugf(NAME_Init,TEXT("D3D Driver: WARNING: no support for DXT1"));
	if( !UseDXT3 )
		debugf(NAME_Init,TEXT("D3D Driver: WARNING: no support for DXT3"));
	if( !UseDXT5 )
		debugf(NAME_Init,TEXT("D3D Driver: WARNING: no support for DXT5"));
	
	// Don't use compressed lightmaps if DXTC not available.
	if( !UseDXT1 || !UseDXT3 || !UseDXT5 )
		UseCompressedLightmaps = 0;

	// Don't use 16 bit textures if DXTC is available.
	if( UseDXT1 )
		Use16bitTextures = 0;

	// Use vertex fog if table fog is not supported.
	UseVertexFog = 0;
	UseRangeFog	 = 0;
	if( !(DeviceCaps8.RasterCaps & D3DPRASTERCAPS_FOGTABLE) )
	{
		UseVertexFog = 1;
		if( DeviceCaps8.RasterCaps & D3DPRASTERCAPS_FOGRANGE )
			UseRangeFog = 1;
	}

	// Log which type of fog we use.
	if( UseVertexFog )
	{
		if( UseRangeFog )
			debugf(NAME_Init,TEXT("D3D Driver: Using range fog"));
		else
			debugf(NAME_Init,TEXT("D3D Driver: Using vertex fog"));
	}
	else
	{
		if( DeviceCaps8.RasterCaps & D3DPRASTERCAPS_WFOG )
			debugf(NAME_Init,TEXT("D3D Driver: Using w- pixel fog"));
		else
			debugf(NAME_Init,TEXT("D3D Driver: Using z- pixel fog"));
	}

	// Only used for debugging.
	FirstColoredMip = 0xFFFF;

	// Verify mipmapping supported.
	if
	(	!(DeviceCaps8.TextureFilterCaps & D3DPTFILTERCAPS_MIPFPOINT)
	&&	!(DeviceCaps8.TextureFilterCaps & D3DPTFILTERCAPS_MIPFLINEAR)
	)
	{
		UseMipmapping = 0;
		debugf(NAME_Init, TEXT("D3D Driver: Mipmapping not available with this driver"));
	}
	else
	{
		UseMipmapping = 1;
		if( DeviceCaps8.TextureFilterCaps & D3DPTFILTERCAPS_MIPFLINEAR )
			debugf( NAME_Init, TEXT("D3D Driver: Supports trilinear"));
		else
			UseTrilinear = 0;
	}

	// Limit texture size if using 16 bit textures.
	if( Use16bitTextures )
	{
		DeviceCaps8.MaxTextureHeight	= 256;
		DeviceCaps8.MaxTextureWidth		= 256;
	}

	if( DeviceCaps8.TextureOpCaps & D3DTEXOPCAPS_BLENDDIFFUSEALPHA )
		debugf( NAME_Init, TEXT("D3D Driver: Supports BLENDDIFFUSEALPHA") );
	else
		DetailTextures = 0;

	if( DeviceCaps8.RasterCaps & D3DPRASTERCAPS_MIPMAPLODBIAS )
 		debugf( NAME_Init, TEXT("D3D Driver: Supports LOD biasing") );

	if( DeviceCaps8.RasterCaps & D3DPRASTERCAPS_ZBIAS )
 		debugf( NAME_Init, TEXT("D3D Driver: Supports Z biasing") );

	if( DeviceCaps8.TextureOpCaps & D3DTEXOPCAPS_ADDSIGNED2X )
 		debugf( NAME_Init, TEXT("D3D Driver: Supports D3DTOP_ADDSIGNED2X") );

	if( DeviceCaps8.TextureOpCaps & D3DTEXOPCAPS_BUMPENVMAP )
 		debugf( NAME_Init, TEXT("D3D Driver: Supports D3DTOP_BUMPENVMAP") );

	if( DeviceCaps8.TextureOpCaps & D3DTEXOPCAPS_BUMPENVMAPLUMINANCE )
 		debugf( NAME_Init, TEXT("D3D Driver: Supports D3DTOP_BUMPENVMAPLUMINANCE") );

	if( DeviceCaps8.TextureOpCaps & D3DTEXOPCAPS_DOTPRODUCT3 )
 		debugf( NAME_Init, TEXT("D3D Driver: Supports D3DTOP_DOTPRODUCT3") );

	if( DeviceCaps8.TextureOpCaps & D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR )
 		debugf( NAME_Init, TEXT("D3D Driver: Supports D3DTOP_MODULATEALPHA_ADDCOLOR") );

	if( DeviceCaps8.TextureOpCaps & D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA )
 		debugf( NAME_Init, TEXT("D3D Driver: Supports D3DTOP_MODULATECOLOR_ADDALPHA") ); 

	// Get device identifier.
	// szDriver, szDescription aren't guaranteed consistent (might change by mfgr, distrubutor, language, etc). Don't do any compares on these.
	// liDriverVersion is safe to do QWORD comparisons on.
	// User has changed drivers/cards iff guidDeviceIdentifier changes.
	guard(GetDeviceIdentifier);
	DeviceIdentifier = Adapters(BestAdapter);
	debugf(NAME_Init,TEXT("Unreal Engine Direct3D support - internal version: SB3"));
	debugf(NAME_Init,TEXT("D3D Device: szDriver=%s"),      appFromAnsi(DeviceIdentifier.Driver));
	debugf(NAME_Init,TEXT("D3D Device: szDescription=%s"), appFromAnsi(DeviceIdentifier.Description));
	debugf(NAME_Init,TEXT("D3D Device: wProduct=%i"),      wProduct=HIWORD(DeviceIdentifier.DriverVersion.HighPart));
	debugf(NAME_Init,TEXT("D3D Device: wVersion=%i"),      wVersion=LOWORD(DeviceIdentifier.DriverVersion.HighPart));
	debugf(NAME_Init,TEXT("D3D Device: wSubVersion=%i"),   wSubVersion=HIWORD(DeviceIdentifier.DriverVersion.LowPart));
	debugf(NAME_Init,TEXT("D3D Device: wBuild=%i"),        wBuild=LOWORD(DeviceIdentifier.DriverVersion.LowPart));
	debugf(NAME_Init,TEXT("D3D Device: dwVendorId=%i"),    DeviceIdentifier.VendorId);
	debugf(NAME_Init,TEXT("D3D Device: dwDeviceId=%i"),    DeviceIdentifier.DeviceId);
	debugf(NAME_Init,TEXT("D3D Device: dwSubSysId=%i"),    DeviceIdentifier.SubSysId);
	debugf(NAME_Init,TEXT("D3D Device: dwRevision=%i"),    DeviceIdentifier.Revision);
	unguard;

    appSprintf( GMachineVideo, TEXT("%s (%i)"), appFromAnsi(DeviceIdentifier.Description), (INT)(LOWORD(DeviceIdentifier.DriverVersion.LowPart)) );

	// Hardware-specific initialization and workarounds for driver and hardware! bugs.
	CubemapTextureAddressing	= D3DTADDRESS_CLAMP;
	IsKyro						= 0;
	IsVoodoo3					= 0;
	Is3dfx						= 0;
	IsGeForce					= 0;
	HasNVCubemapBug				= 0;

	GGPUVendorID				= DeviceIdentifier.VendorId;
	GGPUDeviceID				= DeviceIdentifier.DeviceId;

	if( ParseParam(appCmdLine(),TEXT("nodeviceid")) )
	{
		debugf(NAME_Init,TEXT("D3D Detected: -nodeviceid specified, 3D device identification skipped"));
	}
	else if( DeviceIdentifier.VendorId==0x1002 )
	{
		debugf(NAME_Init,TEXT("D3D Detected: ATI video card"));
		if( DeviceCaps8.MaxSimultaneousTextures == 3 )
			DeviceCaps8.MaxSimultaneousTextures = 2;
	}
	else if( DeviceIdentifier.VendorId==0x121A )
	{
		debugf(NAME_Init,TEXT("D3D Detected: 3dfx video card"));
		Is3dfx = 1;
		if( DeviceIdentifier.DeviceId < 0x0009 )	
		{
			// Voodoo 3/4
			IsVoodoo3			= 1;
			Use16bitTextures	= 1;
			DUMMY_MODULATE2X	= D3DTOP_MODULATE;
		}
	}
	else if( DeviceIdentifier.VendorId==0x8086 )
	{
		debugf(NAME_Init,TEXT("D3D Detected: Intel video card"));			
#if 0
		// No 810/815/830 series chips are supported.
		if( DeviceIdentifier.DeviceId == 0x7121 
		||	DeviceIdentifier.DeviceId == 0x7123
		||	DeviceIdentifier.DeviceId == 0x7125
		||	DeviceIdentifier.DeviceId == 0x7127
		||	DeviceIdentifier.DeviceId == 0x1132
		||	DeviceIdentifier.DeviceId == 0x3577
		)
			appErrorf( NAME_FriendlyError, TEXT("Your graphics card is not supported. Please consult the FAQ"));
#endif
	}
	//else if( DeviceIdentifier.VendorId==0x104A )
	else if( DeviceIdentifier.DeviceId==0x010 )
	{
		debugf(NAME_Init,TEXT("D3D Detected: Kyro video card"));
		IsKyro = 1;
	}
	else if( DeviceIdentifier.VendorId==0x10DE )
	{
		if( DeviceCaps8.MaxSimultaneousTextures == 2 )
		{
			DeviceCaps8.MaxTextureBlendStages = 3;
			IsGeForce = 1;
		}
		CubemapTextureAddressing = D3DTADDRESS_WRAP;
		debugf(NAME_Init,TEXT("D3D Detected: NVidia video card"));
		if( DeviceIdentifier.DeviceId==0x0150 )
		{
			debugf(NAME_Init,TEXT("D3D Detected: GeForce2 GTS"));
			UseMippedCubemaps	  = 0;
		}
		if( DeviceIdentifier.DeviceId==0x0151 )
		{
			debugf(NAME_Init,TEXT("D3D Detected: GeForce2 GTS"));
			UseMippedCubemaps	  = 0;
		}
		if( DeviceIdentifier.DeviceId==0x0152 )
		{
			debugf(NAME_Init,TEXT("D3D Detected: GeForce2 Ultra"));
			UseMippedCubemaps	  = 0;
		}
		if( DeviceIdentifier.DeviceId==0x0153 )
		{
			debugf(NAME_Init,TEXT("D3D Detected: Quadro2 Pro"));
			UseMippedCubemaps	  = 0;
		}

		HasNVCubemapBug = !UseMippedCubemaps;
	}
	else if( DeviceIdentifier.VendorId==0x102B )
	{
		debugf(NAME_Init,TEXT("D3D Detected: Matrox video card"));
		// No G200 chips are supported.
		if( DeviceIdentifier.DeviceId == 0x0521 )
#if 0
			appErrorf( NAME_FriendlyError, TEXT("Your graphics card is not supported. Please consult the FAQ"));
#else
			Is3dfx = 1;
#endif
		// Treat G400/ G450/ G500 like a 3dfx card and remove cubemaps.
		if( (DeviceIdentifier.DeviceId == 0x0525) || (DeviceIdentifier.DeviceId == 0x2527) ) 
		{
			// Card lies about amount of texture units.
			DeviceCaps8.MaxSimultaneousTextures	= 2;
			Is3dfx								= 1;
		}
	}
	else if( DeviceIdentifier.VendorId==0x1023 )
	{
		debugf(NAME_Init,TEXT("D3D Detected: Trident video card"));
	}
	else if( DeviceIdentifier.VendorId==0x1039 )
	{
		debugf(NAME_Init,TEXT("D3D Detected: SiS video card"));
	}
	else if( DeviceIdentifier.VendorId==0x5333 )
	{
		debugf(NAME_Init,TEXT("D3D Detected: S3 video card"));
		switch( DeviceIdentifier.DeviceId )
		{
		case 0x5631: case 0x8811: case 0x8812: case 0x883d:	case 0x8880: case 0x88c0: case 0x88c1: case 0x88d0:
		case 0x88d1: case 0x88f0: case 0x8901: case 0x8904:	case 0x8a01: case 0x8a10: case 0x8a13: case 0x8a20:
		case 0x8a21: case 0x8a22: case 0x8a23: case 0x8a25:	case 0x8a26: case 0x8c00: case 0x8c01: case 0x8c02:
		case 0x8c03: case 0x8c10: case 0x8c12: case 0x8c22: case 0x8c2a: case 0x8c2b: case 0x8c2c: case 0x8c2d:
		case 0x8c2e: case 0x8c2f: case 0x8d04: case 0x9102:
			// Treat S3 cards like 3dfx cards.
			Is3dfx = 1;
			break;
		default:
			break;
		}
	}
	else
	{
		debugf(NAME_Init,TEXT("D3D Detected: Generic 3D accelerator"));
	}

	if( SupportsCubemaps )
	{
		if ( UseMippedCubemaps )
			debugf( NAME_Init, TEXT("D3D Device: using cubemaps [with mipmaps]") );
		else
			debugf( NAME_Init, TEXT("D3D Device: using cubemaps [without mipmaps]") );
	}
	else
		debugf( NAME_Init, TEXT("D3D Device: not using cubemaps") );

#ifdef _XBOX
	//!!vogel: workaround for Feb XDK
	DeviceCaps8.MaxTextureBlendStages = 4;
#endif
	unguard;

	// Register Stats.
	D3DStats.Init();

	return 1;
	unguard;
}

//
//	UD3DRenderDevice::Exit
//
void UD3DRenderDevice::Exit(UViewport* Viewport)
{
	guard(UD3DRenderDevice::Exit);
#ifdef _XBOX
	// Persist display.
	Direct3DDevice8->Present( NULL, NULL, NULL, NULL );
	Direct3DDevice8->PersistDisplay();
#else

	Flush(Viewport);

	RestoreGamma();

	if(Direct3DDevice8)
	{
		xHelper.Shutdown();
		Direct3DDevice8->Release();
		Direct3DDevice8 = NULL;
	}

	if(Direct3D8)
	{
		Direct3D8->Release();
		Direct3D8 = NULL;
	}

	if( hDC )
		ReleaseDC(hWnd,hDC);

#endif
	unguard;
}

//
//	UD3DRenderDevice::SetRes
//
UBOOL UD3DRenderDevice::SetRes(UViewport* Viewport,INT NewX,INT NewY,UBOOL Fullscreen,INT ColorBytes,UBOOL bSaveSize)
{
	guard(UD3DRenderDevice::SetRes);

	HRESULT	Result;

	//!!vogel: returning 0 causes infinite loops and I'd rather catch those bugs at the root.
	if( !GIsEditor && LockedViewport )
		appErrorf(TEXT("Can't change resolution while render device is locked!"));

#if 0
	// Make sure that menus are at least 640x480 even if you ALT-TAB in/ out of fullscreen.
	if( Viewport->GUIController && Viewport->GUIController->bActive && Viewport->GUIController->eventNeedsMenuResolution() )
	{
		if( (NewX < 640) || (NewY < 480 ) )
		{
			debugf(TEXT("clamping"));
			NewX		= 640;
			NewY		= 480;
			bSaveSize	= 0;
		}
	}
#endif

	debugf(TEXT("Enter SetRes: %dx%d Fullscreen %d"), NewX, NewY, Fullscreen );

	Flush(Viewport);

	// Only allow 16 bit color on 3dfx cards.
	if( Is3dfx )
		ColorBytes = 2;

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
	INT NewColorBits = (Use16bit && !GIsEditor) ? 16 : 32;

	// Force same color depth as desktop in windowed mode.
	if( !Fullscreen )
	{
		if( !CurrentFullscreen )
			DesktopColorBits = GetDeviceCaps( GetDC(GetDesktopWindow()), BITSPIXEL );
		if( NewColorBits != DesktopColorBits )
		{
			Use16bit		= DesktopColorBits == 16;
			NewColorBits	= DesktopColorBits;
			bSaveSize		= 0;
		}
	}

	//!!vogel: always recreate the device
#if 0
	// See if anything has changed which warrants recreating the device.
	if( ForceReset				||
		Direct3DDevice8==NULL	|| 
		Fullscreen				||
		CurrentFullscreen )
#endif
	{
		ForceReset = 0;

		// Enumerate device display modes.
		guard(EnumDisplayModes);

		DisplayModes.Empty(Direct3D8->GetAdapterModeCount(BestAdapter));

		for(DWORD Index = 0;Index < Direct3D8->GetAdapterModeCount(BestAdapter);Index++)
		{
			D3DDISPLAYMODE	DisplayMode;

			Direct3D8->EnumAdapterModes(BestAdapter,Index,&DisplayMode);

			DisplayModes.AddItem(DisplayMode);
#ifdef _XBOX
			//debugf(TEXT("%ux%u (%uHz) [%i])"), DisplayMode.Width, DisplayMode.Height, DisplayMode.RefreshRate, DisplayMode.Flags);
#endif	
		}

		unguard;

		// See if the window is full screen.
		INT FullScreenWidth	 = Max(NewX, 1);
		INT FullScreenHeight = Max(NewY, 1);
		D3DFORMAT		AdapterFormat;
		D3DDISPLAYMODE	DisplayMode;

		if(Fullscreen)
		{
			if(DisplayModes.Num()==0 )
				return UnSetRes(TEXT("No fullscreen display modes found"),0,1);

			// Find matching display mode.
			guard(FindBestMatchingMode);

			INT	BestMode  = 0,
				BestError = MAXINT;

			DWORD RefreshRate = ((DesiredRefreshRate <= 60) && !OverrideDesktopRefreshRate) ? 0 : DesiredRefreshRate;

			for(INT Index = 0;Index < DisplayModes.Num();Index++)
			{
				INT ThisError
				=	Abs((INT)DisplayModes(Index).Width - (INT)NewX)
				+	Abs((INT)DisplayModes(Index).Height- (INT)NewY)
				+	Abs((INT)GetFormatBPP(DisplayModes(Index).Format)-(INT)(NewColorBits));

				if((ThisError < BestError || (ThisError == BestError && (!RefreshRate || DisplayModes(Index).RefreshRate == RefreshRate))) && (GetFormatBPP(DisplayModes(Index).Format) == NewColorBits))
				{
					BestMode = Index;
					BestError = ThisError;
				}
			}

			if(BestError == MAXINT)
				return UnSetRes(TEXT("No acceptable display modes found"),0,1);

			// Use the best display mode
			DisplayMode		= DisplayModes(BestMode);
			NewColorBits	= GetFormatBPP(DisplayModes(BestMode).Format);
			NewX			= DisplayModes(BestMode).Width;
			NewY			= DisplayModes(BestMode).Height;

			FullScreenWidth  = Max(NewX, 1);
			FullScreenHeight = Max(NewY, 1);
			CurrentFullscreen = 1;

			AdapterFormat = DisplayModes(BestMode).Format;
			FullScreenRefreshRate = RefreshRate ? DisplayModes(BestMode).RefreshRate : D3DPRESENT_RATE_DEFAULT;

			debugf(NAME_Init,TEXT("Best-match display mode: %ix%ix%i@%i"),DisplayModes(BestMode).Width,DisplayModes(BestMode).Height,GetFormatBPP(DisplayModes(BestMode).Format),DisplayModes(BestMode).RefreshRate);

			unguard;
		}
		else
		{
			guard(GetCurrentMode);

			Result = Direct3D8->GetAdapterDisplayMode(BestAdapter,&DisplayMode);

			if( FAILED(Result) )
				return UnSetRes(TEXT("GetAdapterDisplayMode failed."),Result,1);

			AdapterFormat = DisplayMode.Format;
	
			if( GIsEditor )
			{
				FullScreenWidth  = DisplayMode.Width;
				FullScreenHeight = DisplayMode.Height;
			}

			CurrentFullscreen	= 0;
			//NewColorBits		= GetFormatBPP(AdapterFormat);
			//debugf(TEXT("Current display mode is %d bits (%d)"), NewColorBits, AdapterFormat);
			unguard;
		}

		// Setup the presentation parameters.
		guard(SetupPresentParms);
		appMemzero(&PresentParms,sizeof(PresentParms));
		PresentParms.Flags					= D3DPRESENTFLAG_LOCKABLE_BACKBUFFER;
		PresentParms.Windowed				= !Fullscreen;
		PresentParms.hDeviceWindow			= (HWND)Viewport->GetWindow();
		PresentParms.SwapEffect				= D3DSWAPEFFECT_DISCARD;//Fullscreen ? D3DSWAPEFFECT_DISCARD : D3DSWAPEFFECT_COPY;
		PresentParms.BackBufferWidth		= FullScreenWidth;
		PresentParms.BackBufferHeight		= FullScreenHeight;
		PresentParms.EnableAutoDepthStencil = TRUE;
#ifndef _XBOX
		PresentParms.BackBufferCount = (Fullscreen && UseTripleBuffering) ? 2 : 1;
		PresentParms.FullScreen_RefreshRateInHz	= Fullscreen ? FullScreenRefreshRate : D3DPRESENT_RATE_DEFAULT;
		PresentParms.FullScreen_PresentationInterval = Fullscreen ? (UseVSync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE) : D3DPRESENT_INTERVAL_DEFAULT;
#else
		PresentParms.FullScreen_PresentationInterval = UseVSync ? D3DPRESENT_INTERVAL_ONE : D3DPRESENT_INTERVAL_IMMEDIATE;
		PresentParms.Flags |= D3DPRESENTFLAG_10X11PIXELASPECTRATIO | DisplayMode.Flags;
		PresentParms.BackBufferCount = 1;
		PresentParms.FullScreen_RefreshRateInHz = D3DPRESENT_RATE_DEFAULT;
#endif

		guard(DetermineFormats);

		// Determine which back buffer format to use.
		BackBufferFormat = NewColorBits == 32 ? D3DFMT_X8R8G8B8 : D3DFMT_R5G6B5;

		Result = Direct3D8->CheckDeviceFormat(BestAdapter,D3DDEVTYPE_HAL,AdapterFormat,D3DUSAGE_RENDERTARGET,D3DRTYPE_SURFACE,BackBufferFormat);
		if( FAILED(Result) )
			return UnSetRes(TEXT("CheckDeviceFormat failed"),Result,1);
	
		if(Fullscreen)
			AdapterFormat = BackBufferFormat;

		CurrentColorBytes = NewColorBits / 8;

		PresentParms.BackBufferFormat = BackBufferFormat;

		debugf(TEXT("Using back-buffer format %u(%u-bit)"),BackBufferFormat,GetFormatBPP(BackBufferFormat));

		// Determine which depth buffer format to use.
#ifdef _XBOX
        DepthBufferFormat = D3DFMT_D24S8;
#else
		DepthBufferFormat = (Use16bit && !GIsEditor) ? D3DFMT_D16 : (UseStencil || GIsEditor) ? D3DFMT_D24S8 : D3DFMT_D24X8;
#endif

		INT Tries = 0;
		while( 
			FAILED(Direct3D8->CheckDeviceFormat(BestAdapter,D3DDEVTYPE_HAL,AdapterFormat,D3DUSAGE_DEPTHSTENCIL,D3DRTYPE_SURFACE,DepthBufferFormat)) 
		||	FAILED(Direct3D8->CheckDepthStencilMatch(BestAdapter,D3DDEVTYPE_HAL,AdapterFormat,BackBufferFormat,DepthBufferFormat))
		)
		{
#ifndef _XBOX
			Tries++;

			if( Tries == 3 )
				return UnSetRes(TEXT("CheckDepthStencilMatch failed."),0,1);

			if( UseStencil )
			{
				// Kyros don't support 8 bit stencil.
				DepthBufferFormat = D3DFMT_D24X4S4;
			}
			else
			{
				if( Tries == 1 )
				{
					// Xabre & Kyro e.g. only support D32.
					DepthBufferFormat = D3DFMT_D32;
				}
				else
				{
					// TNT2 only supports D24S8 in 32 bit windowed mode.
					// Parhelia doesn't expose any non stencil formats in 32 bit.
					DepthBufferFormat = D3DFMT_D24S8;
				}
			}
#else
			return UnSetRes(TEXT("CheckDepthStencilMatch failed"),0,1);
#endif
		}
				
		debugf(TEXT("Using depth-buffer format %u(%u-bit)"),DepthBufferFormat,GetFormatBPP(DepthBufferFormat));

		PresentParms.AutoDepthStencilFormat = DepthBufferFormat;

		unguard;
		unguard;

		if( Direct3DDevice8 != NULL )
		{
			guard(ResetDevice);
#ifndef _XBOX
			// Present a black screen while precaching.
			if( 0 )//!GIsEditor )
			{
				Direct3DDevice8->Clear( 0, NULL, D3DCLEAR_TARGET, 0, 0, 0 );
				Direct3DDevice8->Present( NULL, NULL, NULL, NULL );
			}
#endif

			xHelper.Shutdown(); // sjs

			// Recreate for the game and use Reset for the Editor.
			//if( !GIsEditor || (GIsEditor && FAILED(Direct3DDevice8->Reset(&PresentParms))) )
			if( FAILED(Direct3DDevice8->Reset(&PresentParms)) )
			{
				Direct3DDevice8->Release();
				Direct3DDevice8 = NULL;
			}

			unguard;
		}

		// Create D3D device.
		if( Direct3DDevice8 == NULL )
		{
			guard(CreateDevice);
			debugf(TEXT("Creating device"));

#ifndef _XBOX
			if( (DeviceCaps8.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT) == 0 )
				UseHardwareTL = false;
	
			if( (((DeviceCaps8.VertexShaderVersion) & 0xFF00) < 0x0100) || !UseHardwareTL ) 
				UseHardwareVS = false;
#endif
#ifndef _XBOX				
			DWORD BehaviorFlags = D3DCREATE_FPU_PRESERVE | (UseHardwareVS ? /*D3DCREATE_PUREDEVICE |*/ D3DCREATE_HARDWARE_VERTEXPROCESSING : UseHardwareTL ? D3DCREATE_MIXED_VERTEXPROCESSING : D3DCREATE_SOFTWARE_VERTEXPROCESSING);
#else
			//!!vogel: Xbox doesn't need FPU_PRESERVE as runtime doesn't fiddle with precision.
			DWORD BehaviorFlags = D3DCREATE_HARDWARE_VERTEXPROCESSING;
#endif

			D3DDEVTYPE DevType;
#ifndef _XBOX
			if( Viewport->UseSoftwareRasterizer )
				DevType = D3DDEVTYPE_REF;
			else
#endif
				DevType = D3DDEVTYPE_HAL;

			// Try device creation till it either succeeds or fails with a "not yet ready" error code.
			while( 1 )
			{
				// Try to create the device.
				Result = Direct3D8->CreateDevice(BestAdapter,DevType,(HWND)Viewport->GetWindow(),BehaviorFlags,&PresentParms,&Direct3DDevice8);


				if( (Result != D3DERR_DEVICELOST) && (Result != D3DERR_NOTAVAILABLE) )
					break;

				// Sleep a bit and see whether device can be obtained later.
				appSleep( 0.5f );
			}

			xHelper.Init( this ); // sjsf
			if(Result != D3D_OK)
				return UnSetRes(TEXT("CreateDevice failed"),Result,1);
			unguard;

			debugf(NAME_Init,TEXT("D3D Driver: CreateDevice: will use %s transform and lighting."),UseHardwareTL ? TEXT("hardware"):TEXT("software"));
			debugf(NAME_Init,TEXT("D3D Driver: CreateDevice: will use %s vertex processing"), UseHardwareTL ? UseHardwareVS ? TEXT("hardware"):TEXT("mixed") : TEXT("software"));			

#ifndef _XBOX
			// Present a black screen while precaching.
			if( !GIsEditor )
			{
				Direct3DDevice8->Clear( 0, NULL, D3DCLEAR_TARGET, 0, 0, 0 );
				Direct3DDevice8->Present( NULL, NULL, NULL, NULL );
			}
#endif
		}
		else
            xHelper.Init( this );
#ifdef _XBOX
        InitialTextureMemory = -1; // sjs
#else
		InitialTextureMemory = Direct3DDevice8->GetAvailableTextureMem();
#endif

		// Init render states.
		guard(InitRenderState);
		{
			Direct3DDevice8->SetRenderState( D3DRS_ZENABLE, TRUE );
			Direct3DDevice8->SetRenderState( D3DRS_SHADEMODE, D3DSHADE_GOURAUD );
			Direct3DDevice8->SetRenderState( D3DRS_SPECULARENABLE, FALSE );
			Direct3DDevice8->SetRenderState( D3DRS_DITHERENABLE, TRUE );
			Direct3DDevice8->SetRenderState( D3DRS_NORMALIZENORMALS, TRUE );		
			Direct3DDevice8->SetRenderState( D3DRS_RANGEFOGENABLE, UseRangeFog ? TRUE : FALSE );
#ifdef _XBOX
            Direct3DDevice8->SetRenderState( D3DRS_FOGTABLEMODE, D3DFOG_LINEAR );
#else
			if( UseVertexFog )
				Direct3DDevice8->SetRenderState( D3DRS_FOGVERTEXMODE, D3DFOG_LINEAR );
			else
				Direct3DDevice8->SetRenderState( D3DRS_FOGTABLEMODE, D3DFOG_LINEAR );
#endif

#ifndef _XBOX
			// Doesn't have a performance impact on T&L cards but needed for e.g. Kyro cards.
			Direct3DDevice8->SetRenderState( D3DRS_CLIPPING, TRUE );
#endif
			D3DMATERIAL8	Material8;

			appMemzero(&Material8,sizeof(Material8));

			Material8.Ambient.r = 1.0f;
			Material8.Ambient.g = 1.0f;
			Material8.Ambient.b = 1.0f;
			Material8.Ambient.a = 1.0f;

			Material8.Diffuse.r = 1.0f;
			Material8.Diffuse.g = 1.0f;
			Material8.Diffuse.b = 1.0f;
			Material8.Diffuse.a = 1.0f;

			Material8.Specular.r = 1.0f;
			Material8.Specular.g = 1.0f;
			Material8.Specular.b = 1.0f;
			Material8.Specular.a = 1.0f;
			
			Material8.Power = 0.0f;

			Direct3DDevice8->SetMaterial(&Material8);

			Direct3DDevice8->SetRenderState(D3DRS_EMISSIVEMATERIALSOURCE,D3DMCS_COLOR1);
			Direct3DDevice8->SetRenderState(D3DRS_SPECULARMATERIALSOURCE,D3DMCS_MATERIAL);

			if( !(DeviceCaps8.VertexProcessingCaps & D3DVTXPCAPS_MATERIALSOURCE7) )
			{
				debugf(NAME_Init,TEXT("D3D Device: Device doesn't support D3DVTXPCAPS_MATERIALSOURCE7"));
				Direct3DDevice8->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE,D3DMCS_MATERIAL);
				Direct3DDevice8->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE,D3DMCS_MATERIAL);
			}
			else
			{
				Direct3DDevice8->SetRenderState(D3DRS_DIFFUSEMATERIALSOURCE,D3DMCS_COLOR2);
				Direct3DDevice8->SetRenderState(D3DRS_AMBIENTMATERIALSOURCE,D3DMCS_COLOR2);
			}
		}
		unguard;

		DWORD UsedLevelOfAnisotropy = Max<DWORD>( 1, Min<DWORD>( DeviceCaps8.MaxAnisotropy, LevelOfAnisotropy ) );

		// Init texture stage state.
		guard(InitTextureStageState);
		{
			//!!MAT
			FLOAT LodBias = DefaultTexMipBias;
			for( INT Stage=0; Stage<((INT)DeviceCaps8.MaxTextureBlendStages); Stage++ )
			{
				Direct3DDevice8->SetTextureStageState( Stage, D3DTSS_MIPMAPLODBIAS, *(DWORD*)&LodBias );
				Direct3DDevice8->SetTextureStageState( Stage, D3DTSS_MAGFILTER, UsedLevelOfAnisotropy > 1 ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR );
				Direct3DDevice8->SetTextureStageState( Stage, D3DTSS_MINFILTER, UsedLevelOfAnisotropy > 1 ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR );
				Direct3DDevice8->SetTextureStageState( Stage, D3DTSS_MIPFILTER, UseMipmapping ? (UseTrilinear ? D3DTEXF_LINEAR : D3DTEXF_POINT) : D3DTEXF_NONE );
				Direct3DDevice8->SetTextureStageState( Stage, D3DTSS_MAXANISOTROPY, UsedLevelOfAnisotropy );
			}
		}
		unguard;
	}

	// resize the viewport.
	verify(Viewport->ResizeViewport( (Fullscreen ? BLIT_Fullscreen : 0) | BLIT_Direct3D, NewX, NewY, bSaveSize ));

	// Init deferred state.
	guard(InitDeferredState);
	DeferredState.Init( this );
	unguard;

	if( GIsEditor )
	{
		hWnd	= (HWND) Viewport->GetWindow();
		hDC		= GetDC( hWnd );
	}
	else
	{
		hWnd	= NULL;
		hDC		= NULL;
	}

	UpdateGamma(Viewport);

	Viewport->PendingFrame = 0;

	return 1;

	unguard;
}

//
//	UD3DRenderDevice::UnSetRes
//
UBOOL UD3DRenderDevice::UnSetRes( const TCHAR* Msg, HRESULT h, UBOOL Fatal )
{
	guard(UD3DRenderDevice::UnSetRes);
	if( Msg )
		debugf(NAME_Init,TEXT("%s (%s)"),Msg,*D3DError(h));
	if( Fatal )
	{
		if( Msg )
			appErrorf(TEXT("Error setting display mode: %s (%s). Please delete your UT2004.ini file if this error prevents you from starting the game."),Msg,*D3DError(h));
		else
			appErrorf(TEXT("Error setting display mode. Please delete your UT2004.ini file."));
	}
	return 0;
	unguard;
}

//
//	UD3DRenderDevice::Flush
//
void UD3DRenderDevice::Flush(UViewport* Viewport)
{
	guard(UD3DRenderDevice::Flush);

	DynamicVertexStream = NULL;
	DynamicIndexBuffer16 = NULL;
	DynamicIndexBuffer32 = NULL;

	if( ReportUnusedTextures && UnusedTextures.Num() )
	{
		debugf(TEXT("PRECACHING WARNING: %i textures have been uploaded during precaching but were never used"), UnusedTextures.Num() );
		for( INT i=0; i<UnusedTextures.Num(); i++ )
		{
			INT NameIndex = UnusedTexturesNameMap.FindRef( (PTRINT) UnusedTextures(i) );
			debugf(TEXT("%s"), *UnusedTextureNames(NameIndex) );
		}
	}
	UnusedTextures.Empty();
	UnusedTexturesNameMap.Empty();
	UnusedTextureNames.Empty();

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

	PrecacheOnFlip = UsePrecaching;

	guard(ClearFallbacks);
	UMaterial::ClearFallbacks();
	unguard;

	unguard;
}

//
//	UD3DRenderDevice::UpdateGamma
//
void UD3DRenderDevice::UpdateGamma(UViewport* Viewport)
{
	guard(UD3DRenderDevice::UpdateGamma);

	if( ParseParam(appCmdLine(),TEXT("NOGAMMA")) )
		return;

	FLOAT	Gamma = Viewport->GetOuterUClient()->Gamma,
			Brightness = Viewport->GetOuterUClient()->Brightness,
			Contrast = Viewport->GetOuterUClient()->Contrast;

#ifdef _XBOX
	if( Direct3DDevice8)
	{
		// XBox difference: BYTE ranges, rather than _WORDs.
		D3DGAMMARAMP	Ramp;
		for(INT i=0; i<256; i++)
			Ramp.red[i] = Ramp.green[i] = Ramp.blue[i] = Clamp<INT>( appRound( (Contrast+0.5f)*appPow(i/255.f,1.0f/Gamma)*255.f + (Brightness-0.5f)*128.f - Contrast*128.f + 64.f ), 0, 255 );
		Direct3DDevice8->SetGammaRamp(D3DSGR_CALIBRATE,&Ramp);
	}
#else
	D3DGAMMARAMP	Ramp;
	for(INT i=0; i<256; i++)
		Ramp.red[i] = Ramp.green[i] = Ramp.blue[i] = Clamp<INT>( appRound( (Contrast+0.5f)*appPow(i/255.f,1.0f/Gamma)*65535.f + (Brightness-0.5f)*32768.f - Contrast*32768.f + 16384.f ), 0, 65535 );

	if( !GIsEditor && Direct3DDevice8 && (DeviceCaps8.Caps2 & D3DCAPS2_FULLSCREENGAMMA) )
		Direct3DDevice8->SetGammaRamp(D3DSGR_CALIBRATE,&Ramp);
	else
		SetDeviceGammaRamp( hDC, &Ramp );
#endif

	unguard;
}

//
//	UD3DRenderDevice::RestoreGamma
//
void UD3DRenderDevice::RestoreGamma()
{
	guard(UD3DRenderDevice::RestoreGamma);

	if( ParseParam(appCmdLine(),TEXT("NOGAMMA")) )
		return;

	D3DGAMMARAMP	Ramp;

	for(INT ColorIndex = 0;ColorIndex < 256;ColorIndex++)
		Ramp.red[ColorIndex] = Ramp.green[ColorIndex] = Ramp.blue[ColorIndex] = ColorIndex << 8;

	if( !GIsEditor && Direct3DDevice8 && (DeviceCaps8.Caps2 & D3DCAPS2_FULLSCREENGAMMA) )
		Direct3DDevice8->SetGammaRamp(D3DSGR_CALIBRATE,&Ramp);
	else
//		SetDeviceGammaRamp( hDC, &Ramp );
		SetDeviceGammaRamp( GetDC( GetDesktopWindow() ), &Ramp );

	unguard;
}

//
//	UD3DRenderDevice::Exec
//
#ifdef _XBOX
extern TCHAR	GStartMaps[16][256];
extern INT		GStartMapsIndex;
extern INT		GStartMapsMaxIndex;
#endif

// Helper for sorting textures by resource usage.
static inline INT Compare(FD3DTexture* A,FD3DTexture* B)
{
	return (B->CalculateFootprint() - A->CalculateFootprint() >= 0) ? 1 : -1;
}

UBOOL UD3DRenderDevice::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	guard(UD3DRenderDevice::Exec);

	if(ParseCommand(&Cmd,TEXT("DUMPRESOURCEHASH")))
	{
		for(INT HashIndex = 0;HashIndex < 4096;HashIndex++)
		{
			INT	ResourceCount = 0;

			for(FD3DResource*	Resource = ResourceHash[HashIndex];Resource;Resource = Resource->HashNext)
				ResourceCount++;

			debugf(TEXT("Resource hash bin\t%u: %u resources."),HashIndex,ResourceCount);
		}

		return 1;
	}
	else if(ParseCommand(&Cmd,TEXT("FIRSTCOLOREDMIP")))
	{
		if (appStrcmp(Cmd,TEXT("")) != 0) 
			FirstColoredMip = appAtoi(Cmd);
		else
			FirstColoredMip = 255;
		Flush(NULL);
		return 1;
	}
	else if(ParseCommand(&Cmd,TEXT("NEARCLIP")))
	{
		if (appStrcmp(Cmd,TEXT("")) != 0) 
			NEAR_CLIPPING_PLANE = appAtof(Cmd);
		return 1;
	}
#ifdef _XBOX
	// These execs have to be here because I need access to the D3D device for PeristDisplay.
	else if( ParseCommand(&Cmd,TEXT("GETXBOXMAPS") ))
	{
		for( INT i=0; i<=GStartMapsMaxIndex; i++ )
			Ar.Logf( TEXT("%s"), GStartMaps[i] );
		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("LAUNCHXBOXMAP") ))
	{
		GStartMapsIndex = appAtoi(Cmd);
		Direct3DDevice8->PersistDisplay();
		// Restart and pass command-line to new instance.
		#define	TOKEN_LINEAR_LOAD		0
		LAUNCH_DATA LaunchData;
		INT* Dummy = (INT*) &LaunchData.Data;
			*(Dummy++) = GStartMapsIndex;
			*(Dummy++) = TOKEN_LINEAR_LOAD;
		debugf(TEXT("GStartMapsIndex [%i]"), *((INT*) &LaunchData.Data));
		debugf(TEXT("Launching map [%s]"), GStartMaps[GStartMapsIndex] );
		XLaunchNewImage( TCHAR_TO_ANSI(TEXT("D:\\default.xbe")), &LaunchData );	//!! DEFAULT.XBE
		return 1;
	}
#endif
	else if( ParseCommand(&Cmd,TEXT("D3DRESOURCES") ))
	{
		UBOOL DetailedStats = ParseCommand(&Cmd,TEXT("ALL"));

		// Calculate resource usage
		INT StatTextureBytes=0, StatVertexStreamBytes=0, StatIndexBufferBytes=0, StatOtherBytes=0;
		INT StatNumTextures=0, StatNumVertexStreams=0, StatNumIndexBuffers=0, StatNumOther=0;

		if( DetailedStats )
		{
			Ar.Logf( TEXT("Direct3D Detailed Texture Resource Usage"));
			Ar.Logf( TEXT(""));
			Ar.Logf( TEXT("KByte    Type    Name"));
			Ar.Logf( TEXT("--------------------------------------"));
			Ar.Logf( TEXT(""));
		}

		TArray<FD3DTexture*> Textures;

		for(FD3DResource* Resource = ResourceList;Resource;Resource = Resource->NextResource)
		{
			INT Footprint = Resource->CalculateFootprint();
			if(Resource->GetTexture())
			{
				StatTextureBytes += Footprint;
				StatNumTextures++;
				Textures.AddItem(Resource->GetTexture());
			}
			else if(Resource->GetVertexStream())
			{
				StatVertexStreamBytes += Footprint;
				StatNumVertexStreams++;
			}
			else if(Resource->GetIndexBuffer())
			{
				StatIndexBufferBytes += Footprint;
				StatNumIndexBuffers++;
			}
			else
			{
				StatOtherBytes += Footprint;
				StatNumOther++;
			}
		}

		if( DetailedStats && Textures.Num() )			
		{		
			Sort( &Textures(0), Textures.Num() );
			
			for( INT i=0; i<Textures.Num(); i++ )
			{
				FString Format;
				switch(	Textures(i)->CachedFormat )
				{
				case D3DFMT_R5G6B5:
				case D3DFMT_A4R4G4B4:
					Format = TEXT("RGB4");
					break;
				case D3DFMT_A8R8G8B8:
					Format = TEXT("RGB8");
					break;
				case D3DFMT_DXT1:
					Format = TEXT("DXT1");
					break;
				case D3DFMT_DXT3:
					Format = TEXT("DXT3");
					break;
				case D3DFMT_DXT5:
					Format = TEXT("DXT5");
					break;
				default:
					Format = TEXT("NONE");
				}

				Ar.Logf( TEXT("%8d %s %s"), Textures(i)->CalculateFootprint() / 1024, *Format, *Textures(i)->Name );
			}
		
			Ar.Logf( TEXT(""));
		}

#ifdef _XBOX
        DWORD AllocKBytes = -1;//(InitialTextureMemory - Direct3DDevice8->GetAvailableTextureMem()) / 1024;
#else
		DWORD AllocKBytes = (InitialTextureMemory - Direct3DDevice8->GetAvailableTextureMem()) / 1024;
#endif

		Ar.Logf( TEXT("Direct3D Resource Usage Summary"));
		Ar.Logf( TEXT(""));
		Ar.Logf( TEXT("Resource Type      Count   Total Bytes"));
		Ar.Logf( TEXT("--------------------------------------"));
		Ar.Logf( TEXT("Textures            %4d      %8d"), StatNumTextures, StatTextureBytes );
		Ar.Logf( TEXT("Vertex Streams      %4d      %8d"), StatNumVertexStreams, StatVertexStreamBytes );
		Ar.Logf( TEXT("Index Buffers       %4d      %8d"), StatNumIndexBuffers, StatIndexBufferBytes );
		Ar.Logf( TEXT("Other               %4d      %8d"), StatNumOther, StatOtherBytes );
		Ar.Logf( TEXT("") );
		Ar.Logf( TEXT("Allocated D3D resources account for %d KBytes"), AllocKBytes );
		Ar.Logf( TEXT("") );

		return 1;
	}
	else if( ParseCommand(&Cmd,TEXT("SUPPORTEDRESOLUTION")) )
	{
		INT		Width = 0,
				Height = 0,
				BitDepth = 0;
		UBOOL	Supported = 0;

		if(Parse(Cmd,TEXT("WIDTH="),Width) && Parse(Cmd,TEXT("HEIGHT="),Height) && Parse(Cmd,TEXT("BITDEPTH="),BitDepth))
		{
			for(INT ModeIndex = 0;ModeIndex < DisplayModes.Num();ModeIndex++)
			{
				D3DDISPLAYMODE&	DisplayMode = DisplayModes(ModeIndex);

				if(DisplayMode.Width == Width && DisplayMode.Height == Height && GetFormatBPP(DisplayMode.Format) == BitDepth)
				{
					Supported = 1;
					break;
				}
			}
		}

		Ar.Logf(TEXT("%u"),Supported);

		return 1;
	}
	return 0;
	unguard;
}

//
//	UD3DRenderDevice::Lock
//
FRenderInterface* UD3DRenderDevice::Lock(UViewport* Viewport,BYTE* InHitData,INT* InHitSize)
{
	guard(UD3DRenderDevice::Lock);

	INT	StartCycles = appCycles();

	FrameCounter++;

#ifndef _XBOX
	// Check cooperative level.
	HRESULT hr=NULL;
	guard(RecreateDevice);
	if( Direct3DDevice8 == NULL )
	{
		// This can happen when coming out of fullscreen: CreateDevice fails because fullscreen colordepth
		// isn't available in windowed mode.
		ForceReset = 1;
		if( !SetRes(Viewport, Viewport->SizeX, Viewport->SizeY, CurrentFullscreen) )
			appErrorf(TEXT("Failed resetting mode"));				
	}
	unguard;
	guard(TestCooperativeLevel);
	hr=Direct3DDevice8->TestCooperativeLevel();
	unguard;
	if( FAILED(hr) )
	{
		//debugf(TEXT("TestCooperativeLevel failed (%s)"),*D3DError(hr));

		guard(HandleBigChange);
		// D3DERR_DEVICELOST is returned if the device was lost, but exclusive mode isn't available again yet.
		// D3DERR_DEVICENOTRESET is returned if the device was lost, but can be reset.

		// Can't render at this time ?
		if( hr == D3DERR_DEVICELOST ) 
			return NULL;

		if( hr == D3DERR_DEVICENOTRESET )
		{
			ForceReset = 1;
			if( !SetRes(Viewport, Viewport->SizeX, Viewport->SizeY, CurrentFullscreen) )
				appErrorf(TEXT("Failed resetting mode"));				
		}
		else
			return NULL;

		unguard;
	}
#endif

	LockedViewport = Viewport;

	// Set viewport.
	guard(SetViewport);
	D3DVIEWPORT8	ViewportInfo;
	
	ViewportInfo.Width      = Viewport->SizeX;
	ViewportInfo.Height     = Viewport->SizeY;

#ifndef _XBOX
	if( !CurrentFullscreen )
	{
		RECT ClientRect;
		GetClientRect( (HWND)Viewport->GetWindow(), &ClientRect );
		ViewportInfo.Width      = ClientRect.right;
		ViewportInfo.Height     = ClientRect.bottom;
	}
#endif
	
	ViewportInfo.X          = 0;
	ViewportInfo.Y          = 0;
	ViewportInfo.MinZ       = 0.0;
	ViewportInfo.MaxZ       = 1.0;

	ViewportInfo.Width		= Min<DWORD>( ViewportInfo.Width,	PresentParms.BackBufferWidth	);
	ViewportInfo.Height		= Min<DWORD>( ViewportInfo.Height,	PresentParms.BackBufferHeight	);

	if( FAILED(Direct3DDevice8->SetViewport(&ViewportInfo) ) )
	{
		LockedViewport = NULL;
		return NULL;
	}

	unguard;

	// Begin scene.
	guard(BeginScene);

	HRESULT	Result = Direct3DDevice8->BeginScene();

	if( FAILED(Result) )
	{
#ifndef _XBOX
//		if( ++FailCount==1 )
//			goto Failed;
#endif
		appErrorf(TEXT("BeginScene failed (%s)"),*D3DError(Result));
	}

	unguard;

	// Create the render interface.
	RenderInterface.Locked( Viewport, InHitData, InHitSize );

	GStats.DWORDStats(D3DStats.STATS_LockCalls) ++;
	GStats.DWORDStats(D3DStats.STATS_LockCycles) += appCycles() - StartCycles;

	return &RenderInterface;

	unguard;
}

//
//	UD3DRenderDevice::Unlock
//
void UD3DRenderDevice::Unlock(FRenderInterface* RI)
{
	guard(UD3DRenderDevice::Unlock);

	clock(GStats.DWORDStats(D3DStats.STATS_UnlockCycles));

	// Update resource stats.
	INT	NumTextures			= 0,
		TextureBytes		= 0,
		NumIndexBuffers		= 0,
		IndexBufferBytes	= 0,
		NumVertexStreams	= 0,
		VertexStreamBytes	= 0,
		ResourceBytes		= 0;

	// Iterate through resources collecting stats.
	for(FD3DResource* Resource = ResourceList;Resource;Resource = Resource->NextResource)
	{
		if(Resource->LastFrameUsed == FrameCounter)
		{
			if(Resource->GetTexture())
			{
				TextureBytes += Resource->CalculateFootprint();
				NumTextures++;
			}
			else if(Resource->GetVertexStream())
			{
				VertexStreamBytes += Resource->CalculateFootprint();
				NumVertexStreams++;
			}
			else if(Resource->GetIndexBuffer())
			{
				IndexBufferBytes += Resource->CalculateFootprint();
				NumIndexBuffers++;
			}
		}
	}

#ifndef _XBOX
	ResourceBytes = InitialTextureMemory - Direct3DDevice8->GetAvailableTextureMem();
#endif


	// Propagate to GStats.
	GStats.DWORDStats( D3DStats.STATS_NumTextures		) = NumTextures;
	GStats.DWORDStats( D3DStats.STATS_TextureBytes		) = TextureBytes;
	GStats.DWORDStats( D3DStats.STATS_NumIndexBuffers	) = NumIndexBuffers;
	GStats.DWORDStats( D3DStats.STATS_IndexBufferBytes	) = IndexBufferBytes;
	GStats.DWORDStats( D3DStats.STATS_VertexStreamBytes	) = VertexStreamBytes;
	GStats.DWORDStats( D3DStats.STATS_ResourceBytes		) = ResourceBytes;

	// Hit detection.
	FD3DRenderInterface* D3DRI = (FD3DRenderInterface*) RI;
	if( D3DRI )
	{		
		check(D3DRI->HitStack.Num() == 0);

		if(D3DRI->HitSize)
			*D3DRI->HitSize = D3DRI->HitCount;
	}

	// Actual "unlock".
	RenderInterface.Unlocked();

	if( Direct3DDevice8 )
		Direct3DDevice8->EndScene();

	LockedViewport = NULL;

	GStats.DWORDStats(D3DStats.STATS_UnlockCalls)++;
	unclock(GStats.DWORDStats(D3DStats.STATS_UnlockCycles));

	unguard;
}

//
//  UD3DRenderDevice::Present
//

void UD3DRenderDevice::Present(UViewport* Viewport)
{
	guard(UD3DRenderDevice::Present);

	INT	StartCycles = appCycles();

	if( Direct3DDevice8 )
	{
	    // Stall HW to reduce mouse lag. (unless it's a Kyro)
	    if( ReduceMouseLag && !GIsBenchmarking && !IsKyro )
		    ReadPixels( (UViewport*) NULL, NULL );
    
		if( CurrentFullscreen || !Viewport )
		    Direct3DDevice8->Present(NULL,NULL,NULL,NULL);
	    else
	    {
#ifdef _XBOX
			appErrorf( TEXT("DUMMY") );
#else
		    HWND hwnd = (HWND)Viewport->GetWindow();
		    RECT ClientRect;
    
		    //GetClientRect( hwnd, &ClientRect );
		    ClientRect.top		= 0;
		    ClientRect.left		= 0;
		    ClientRect.right	= Viewport->SizeX;
		    ClientRect.bottom	= Viewport->SizeY;
    
		    Direct3DDevice8->Present(&ClientRect,&ClientRect,hwnd,NULL);
#endif
		}
	}

	GStats.DWORDStats(D3DStats.STATS_PresentCalls)++;
	GStats.DWORDStats(D3DStats.STATS_PresentCycles) += appCycles() - StartCycles;

	unguard;
}

//
//	UD3DRenderDevice::ReadPixels
//
void UD3DRenderDevice::ReadPixels(UViewport* Viewport,FColor* Pixels,UBOOL Flipped)
{
	guard(UD3DRenderDevice::ReadPixels);

	// Retrieve the last back buffer.
	IDirect3DSurface8*	BackBuffer;

	HRESULT	Result = Direct3DDevice8->GetBackBuffer(0,D3DBACKBUFFER_TYPE_MONO,&BackBuffer);

	if( FAILED(Result) )
		appErrorf(TEXT("GetBackBuffer failed: %s"),*D3DError(Result));

	// Determine the format of the back buffer.
	D3DSURFACE_DESC	SurfaceDesc;

	Result = BackBuffer->GetDesc(&SurfaceDesc);

	if( FAILED(Result) )
		appErrorf(TEXT("GetDesc failed: %s"),*D3DError(Result));

	// Lock the back buffer.
	D3DLOCKED_RECT LockedRect;

	if( Viewport && Pixels )
	{	
		Result = BackBuffer->LockRect(&LockedRect,NULL,D3DLOCK_READONLY);
	}
	else
	{
		RECT DestRect;
		DestRect.top	= 0;
		DestRect.left	= 0;
		DestRect.bottom = 1;
		DestRect.right  = 1;
		Result = BackBuffer->LockRect(&LockedRect,&DestRect,D3DLOCK_READONLY);
	}

	if( FAILED(Result) )
		appErrorf(TEXT("LockRect failed: %s"),*D3DError(Result));

#ifndef _XBOX
	if( Viewport && Pixels )
	{
		if(SurfaceDesc.Format == D3DFMT_X8R8G8B8 || SurfaceDesc.Format == D3DFMT_A8R8G8B8 || SurfaceDesc.Format == D3DFMT_R5G6B5 )
		{
			// Copy the contents of the back buffer to the destination.
			FColor*	Dest = Pixels;
			INT		X,
					Y;

			for(Y = 0;Y < Viewport->SizeY;Y++)
			{
				INT SrcY = Flipped ? (Viewport->SizeY-1) - Y : Y;

				DWORD*	SourceRow32 = (DWORD*) ((BYTE*) LockedRect.pBits + LockedRect.Pitch * SrcY);
				_WORD*	SourceRow16 = (_WORD*) ((BYTE*) LockedRect.pBits + LockedRect.Pitch * SrcY);

				if( SurfaceDesc.Format == D3DFMT_R5G6B5 )
				{
					for(X = 0;X < Viewport->SizeX;X++)
					{
						_WORD& C = SourceRow16[X];
						*Dest++  = FColor((C>>11) << 3, ((C >> 5)& 63) << 2 , (C & 31) << 3);
					}
				}
				else
				{
					for(X = 0;X < Viewport->SizeX;X++)
						*Dest++ = SourceRow32[X];
				}
			}
		}
		else
			debugf(TEXT("ReadPixels: Unknown back-buffer format."));
	}
#endif

	// Unlock the back buffer.
	BackBuffer->UnlockRect();

	// Release the back buffer.
	BackBuffer->Release();

	unguard;
}


//
// UD3DRenderDevice::SupportsTextureFormat
//
UBOOL UD3DRenderDevice::SupportsTextureFormat( ETextureFormat Format )
{
	switch( Format )
	{
	case TEXF_DXT1:
		return UseDXT1;
	case TEXF_DXT3:
		return UseDXT3;
	case TEXF_DXT5:
		return UseDXT5;
	case TEXF_RGBA8:
	case TEXF_RGB8:
		return !Use16bitTextures;
	default:
		return true;
	}
}

//
// UD3DRenderDevice::SetEmulationMode
//
void UD3DRenderDevice::SetEmulationMode(EHardwareEmulationMode Mode)
{
	guard(UD3DRenderDevice::SetEmulationMode)

	D3DCAPS8 LocalDeviceCaps8; 
	guard(GetDeviceCaps);
	HRESULT Result = Direct3D8->GetDeviceCaps(BestAdapter,D3DDEVTYPE_HAL,&LocalDeviceCaps8);
	if( FAILED(Result) )
		appErrorf(TEXT("GetDeviceCaps failed(%s) on adapter %i."),*D3DError(Result), BestAdapter);
	unguard;

	//!! warning: Direct3D8->GetDeviceCaps not necessarily == Direct3DDevice8->GetDeviceCaps

	DeviceCaps8.MaxSimultaneousTextures = LocalDeviceCaps8.MaxSimultaneousTextures;
	DeviceCaps8.MaxTextureBlendStages   = LocalDeviceCaps8.MaxTextureBlendStages;
	PixelShaderVersion = ((LocalDeviceCaps8.PixelShaderVersion & 0xFF00) >> 8) * 10 + LocalDeviceCaps8.PixelShaderVersion & 0xFF;
	
	switch( Mode )
	{
	case HEM_GeForce1:
		DeviceCaps8.MaxSimultaneousTextures = 2;
		DeviceCaps8.MaxTextureBlendStages   = 3;
		PixelShaderVersion					= 0;
		break;
	case HEM_XBox:
		DeviceCaps8.MaxSimultaneousTextures = 4;
		DeviceCaps8.MaxTextureBlendStages   = 4;
		break;
	}

	UMaterial::ClearFallbacks();

	UViewport::RefreshAll();
	
	unguard;
}

//
// UD3DRenderDevice::GetRenderCaps
//
FRenderCaps* UD3DRenderDevice::GetRenderCaps()
		{
	static FRenderCaps RenderCaps;
#if _XBOX
	//!!vogel: don't use pixel shaders for now.
	RenderCaps.MaxSimultaneousTerrainLayers = 1;
#else
	if( PixelShaderVersion >= 14 )
		RenderCaps.MaxSimultaneousTerrainLayers = 3; //!!vogel: 4 is slower due to increased overhead
	else
	if( PixelShaderVersion >= 11 )
		RenderCaps.MaxSimultaneousTerrainLayers = 3;
	else
	if( DeviceCaps8.MaxSimultaneousTextures >= 7 && IsKyro )
		RenderCaps.MaxSimultaneousTerrainLayers = 3;	//!!powervr_aaron: Kyro doesn't support TEMP, but 3 layers can be done in 7 stages without it
	else
	if( DeviceCaps8.MaxSimultaneousTextures >= 4 && !IsKyro )	//!! Kyro doesn't support TEMP
		RenderCaps.MaxSimultaneousTerrainLayers = 2;
	else
	if( DeviceCaps8.MaxSimultaneousTextures >= 2 )
		RenderCaps.MaxSimultaneousTerrainLayers = 1;
#endif
	//!!powervr_aaron: Terrain code needs to know whether pixel shaders are being used, and whether SW TnL is being used
	RenderCaps.PixelShaderVersion	= PixelShaderVersion;
	RenderCaps.HardwareTL			= UseHardwareTL;
	return &RenderCaps;
}


UD3DRenderDevice::FD3DStats::FD3DStats()
{
	guard(FD3DStats::FD3DStats)
	appMemset( &STATS_FirstEntry, 0xFF, (DWORD) &STATS_LastEntry - (DWORD) &STATS_FirstEntry );
	unguard;
}

void UD3DRenderDevice::FD3DStats::Init()
{
	guard(FD3DStats::Init);

	// If already initialized retrieve indices from GStats.
	if( GStats.Registered[STATSTYPE_Hardware] )
	{
		INT* Dummy = &STATS_PushStateCalls;
		for( INT i=0; i<GStats.Stats[STATSTYPE_Hardware].Num(); i++ )
			*(Dummy++) = GStats.Stats[STATSTYPE_Hardware](i).Index;
		return;
	}

	// Register stats with GStat.
	STATS_NumPrimitives					= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("NumPrimitives"		), TEXT("Hardware"		), STATSUNIT_Default				);
	STATS_NumSVPVertices				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("NumSVPVertices"		), TEXT("Hardware"		), STATSUNIT_Default				);
	STATS_NumTextures					= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("NumTextures"			), TEXT("Hardware"		), STATSUNIT_Default				);
	STATS_TextureBytes					= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("TextureBytes"			), TEXT("Hardware"		), STATSUNIT_KByte					);
	STATS_ResourceBytes					= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("Total Resources"		), TEXT("Hardware"		), STATSUNIT_KByte					);
	STATS_PushStateCalls				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("PushState"			), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_PushStateCycles				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("PushState"			), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_PopStateCalls					= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("PopState"			), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_PopStateCycles				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("PopState"			), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_SetRenderTargetCalls			= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("SetRenderTarget"		), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_SetRenderTargetCycles			= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("SetRenderTarget"		), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_SetMaterialCalls				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("SetMaterial"			), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_SetMaterialCycles				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("SetMaterial"			), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_SetMaterialBlendingCalls		= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("SetMaterialBlending"	), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_SetMaterialBlendingCycles		= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("SetMaterialBlending"	), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_SetVertexStreamsCalls			= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("SetVertexStream"		), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_SetVertexStreamsCycles		= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("SetVertexStream"		), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_SetDynamicStreamCalls			= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("SetDynStream"		), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_SetDynamicStreamCycles		= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("SetDynStream"		), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_SetIndexBufferCalls			= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("SetIB"				), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_SetIndexBufferCycles			= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("SetIB"				), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_SetDynamicIndexBufferCalls	= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("SetDynIB"			), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_SetDynamicIndexBufferCycles	= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("SetDynIB"			), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_DrawPrimitiveCalls			= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("DrawPrimitive"		), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_DrawPrimitiveCycles			= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("DrawPrimitive"		), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_LockCalls						= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("Lock"				), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_LockCycles					= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("Lock"				), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_UnlockCalls					= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("Unlock"				), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_UnlockCycles					= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("Unlock"				), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_PresentCalls					= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("Present"				), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_PresentCycles					= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("Present"				), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_StateChanges					= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("StateChanges"		), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_StateChangeCycles				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("StateChanges"		), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_TextureChanges				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("TextureChanges"		), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_TextureChangeCycles			= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("TextureChanges"		), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_TransformChanges				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("TransformChanges"	), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_TransformChangeCycles			= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("TransformChanges"	), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_LightChanges					= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("LightChanges"		), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_LightChangeCycles				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("LightChanges"		), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_LightSetChanges				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("LightSet"			), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_LightSetCycles				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("LightSet"			), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_ClearCalls					= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("Clear"				), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_ClearCycles					= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("Clear"				), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_DynamicVertexBufferLockCalls	= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("DynVBLock"			), TEXT("Hardware"		), STATSUNIT_Combined_Default_MSec	);
	STATS_DynamicVertexBufferLockCycles	= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("DynVBLock"			), TEXT("Hardware"		), STATSUNIT_MSec					);
	STATS_DynamicVertexBufferDiscards	= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("DynVBDiscards"		), TEXT("Hardware"		), STATSUNIT_Default				);
	STATS_DynamicVertexBytes			= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("DynVBBytes"			), TEXT("Hardware"		), STATSUNIT_KByte					);
	STATS_DynamicIndexBytes				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("DynIBBytes"			), TEXT("Hardware"		), STATSUNIT_KByte					);
	STATS_NumVertexStreams				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("NumVertexStreams"	), TEXT("Hardware"		), STATSUNIT_Default				);
	STATS_NumIndexBuffers				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("NumIndexBuffers"		), TEXT("Hardware"		), STATSUNIT_Default				);
	STATS_VertexStreamBytes				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("VertexStreamBytes"	), TEXT("Hardware"		), STATSUNIT_KByte					);
	STATS_IndexBufferBytes				= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("IndexBufferBytes"	), TEXT("Hardware"		), STATSUNIT_KByte					);
	STATS_StreamSourceChanges			= GStats.RegisterStats( STATSTYPE_Hardware, STATSDATATYPE_DWORD, TEXT("StreamSourceChanges"	), TEXT("Hardware"		), STATSUNIT_Default				);

	// Initialized.
	GStats.Registered[STATSTYPE_Hardware] = 1;

	unguard;
}

class FTerrain3LayerPixelShader11: public FD3DPixelShader
{
public:
	// Constructor/destructor.
	FTerrain3LayerPixelShader11(UD3DRenderDevice* InRenDev)
	:	FD3DPixelShader( InRenDev, PS_Terrain3Layer, 
			"ps.1.1																\n"\
			"def c0, 1, 0, 0, 0				// used to extract value from R		\n"\
			"def c1, 0, 1, 0, 0				// used to extract value from G		\n"\
			"def c2, 0, 0, 1, 0				// used to extract value from B		\n"\
			"tex t0							// weightmaps in R, G, B			\n"\
			"tex t1							// layer 1							\n"\
			"tex t2							// layer 2							\n"\
			"tex t3							// layer 3							\n"\
			"dp3 r1, t0, c0					// r1 = Weight1						\n"\
			"mul r0, t1, r1					// r0 = T1*W1						\n"\
			"dp3 r1, t0, c1					// r1 = Weight2						\n"\
			"mad r0, t2, r1, r0				// r0 = T2*W2 + T1*W1				\n"\
			"dp3 r1, t0, c2					// r1 = Weight3						\n"\
			"mad r0, t3, r1, r0				// r0 = T3*W3 + T2*W2 + T1*W1		\n"\
			"mul_x2 r0, r0, v0				// r0 = r0 * lighting				\n"
		)
	{}
};

class FTerrain3LayerPixelShader14: public FD3DPixelShader
{
public:
	// Constructor/destructor.
	FTerrain3LayerPixelShader14(UD3DRenderDevice* InRenDev)
	:	FD3DPixelShader( InRenDev, PS_Terrain3Layer, 
			"ps.1.4																\n" \
			"texld r0, t0					// rgba - blending weights			\n" \
			"texld r1, t1					// tex 1							\n" \
			"texld r2, t2					// tex 2							\n" \
			"texld r3, t3					// tex 3							\n" \
			"mul r1, r1, r0.r													\n" \
			"mad r1, r2, r0.g, r1												\n" \
			"mad r1, r3, r0.b, r1												\n" \
			"mul_x2 r0, r1, v0													\n"
		)
	{}
};

class FTerrain4LayerPixelShader14: public FD3DPixelShader
{
public:
	// Constructor/destructor.
	FTerrain4LayerPixelShader14(UD3DRenderDevice* InRenDev)
	:	FD3DPixelShader( InRenDev, PS_Terrain4Layer, 		
			"ps.1.4									\n" \
			"texld r0, t0					// rgba - blending weights			\n" \
			"texld r1, t1					// tex 1							\n" \
			"texld r2, t2					// tex 2							\n" \
			"texld r3, t3					// tex 3							\n" \
			"texld r4, t4					// tex 4							\n" \
			"mul r1, r1, r0.r													\n" \
			"mad r1, r2, r0.g, r1												\n" \
			"mad r1, r3, r0.b, r1												\n" \
			"mad r1, r4, r0.a, r1												\n" \
			"mul_x2 r0, r1, v0													\n"
		)
	{}
};


FD3DPixelShader* UD3DRenderDevice::GetPixelShader(EPixelShader Type)
{
	guard(UD3DRenderDevice::GetPixelShader);
	
	if( Type == PS_None )
		return NULL;

	FD3DPixelShader*	ShaderPtr = PixelShaders;

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
		if( PixelShaderVersion >= 14 )
			return new FTerrain3LayerPixelShader14(this);
		else
			return new FTerrain3LayerPixelShader11(this);
	case PS_Terrain4Layer:
		return new FTerrain4LayerPixelShader14(this);
	default:
		return NULL;
	}

	unguard;
}

//
//	UD3DRenderDevice::ResourceCached
//

UBOOL UD3DRenderDevice::ResourceCached(QWORD CacheId)
{
	 FD3DResource*	Resource = GetCachedResource(CacheId);

	 if(!Resource)
		 return 0;

	 FD3DTexture*	Texture = Resource->GetTexture();

	 if(!Texture)
		 return 1;

	 if(Texture->Direct3DTexture8 || Texture->Direct3DCubeTexture8)
		 return 1;

	 return 0;
}


#if __STATIC_LINK
extern "C" {
    void autoInitializeRegistrantsD3DDrv(void)
    {
        AUTO_INITIALIZE_REGISTRANTS_D3DDRV;
    }
}
#endif

