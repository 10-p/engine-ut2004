/*=============================================================================
	PixoDrv.h: Unreal Pixo support header.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.
 
	Revision history:
		* Created by Michael Sartain from GL driver
		* Taken over by Daniel Vogel

=============================================================================*/

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/
#ifndef HEADER_PIXODRV
#define HEADER_PIXODRV

#if WIN32
#pragma pack(push,8)
#include <windows.h>
#pragma pack(pop)
#endif

#include "Pixo_GL.h"

#ifdef __LINUX_X86__
#define __declspec(x)
#define USE_SDL 1
#endif

// !!! FIXME: Big Fucking Hack.
#if USE_SDL
#include "SDL.h"        // SDL2
#define HWND void*      // window handle is only cast into CreatePixoBuffers
#include "pixomatic_linux.h"
#include "Engine.h"
#include "utils_surface.h"
#define PIXO_BUF PIXOSURFACE
#define PixoBufferLock(buf, flags) PixoSurface_BufferLock(buf, flags)
#define PixoBufferUnlock(buf) PixoSurface_BufferUnlock(buf)
#define PixoBufferOpen(hwnd, w, h, f) PixoSurface_BufferOpen(w, h ,f)
#define PixoBufferClose(buf) PixoSurface_BufferClose(buf)
#else
#include <pixomatic.h>
#include "Engine.h"
#endif

#include "PixoResource.h"
#include "PixoRenderInterface.h"
#include "PixoRenderDevice.h"

#ifdef _DEBUG
#define SPEW_PIXO_WARNING(_x)                                   \
	do {                                                        \
		static UBOOL Warned = false;                            \
		if(!Warned)                                             \
		{                                                       \
			GWarn->Logf( TEXT("PIXOMATIC WARNING: %s"), _x );   \
			Warned = true;                                      \
		}                                                       \
	} while(0)
	
#else
#define SPEW_PIXO_WARNING(_x)
#endif

// PURE_C=0 selects Epic's hand-written MMX blocks, which are MSVC `__asm { mov eax, ... }` syntax, so the
// guard is _MSC_VER: MinGW GCC takes the C paths every non-Windows build uses.
#if WIN32 && defined(_MSC_VER)
#  if defined(_WIN64) || defined(_DEBUG)
#     define PURE_C      1
#  else
#     define PURE_C      0
#  endif
#else
#  define PURE_C      1
#endif

// The POSIX stand-ins below are about the platform, not the compiler: MinGW gets POINT and _vsnprintf
// from the Windows headers, so defining them here would be a redeclaration.
#if !WIN32
#  ifndef _vsnprintf
#    define _vsnprintf vsnprintf
#  endif
   typedef struct
   {
       int x;
       int y;
   } POINT;
#endif

extern "C" { void autoInitializeRegistrantsPixoDrv(void); }

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

