/*=============================================================================
	OpenGLDrv.h: Unreal OpenGL support header.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
		
=============================================================================*/

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/
#ifndef HEADER_OPENGLDRV
#define HEADER_OPENGLDRV

// UT_OPENGLDRV_WGL: this build creates its own GL context through WGL (an MSVC build). Every other build,
// MinGW included, gets the context from SDL2Drv and takes the SDL path.
#if defined(WIN32) && defined(_MSC_VER)
#define UT_OPENGLDRV_WGL 1
#endif

#ifdef WIN32
#pragma pack(push,8)
#include <windows.h>	// GL/gl.h needs WINGDIAPI/APIENTRY from here on every Windows compiler
#pragma pack(pop)
#endif
#ifndef UT_OPENGLDRV_WGL
#include "SDL.h"
#endif

#include "GL/gl.h"
#include "glext.h"

#ifdef UT_OPENGLDRV_WGL
#include "wglext.h"
#endif

// HackyMcHack
#define GL_MODULATE_ADD_ATI					0x8744
#define GL_VERTEX_ARRAY_STORAGE_HINT_APPLE	0x851F
#define GL_STORAGE_CACHED_APPLE				0x85BE
#define GL_STORAGE_SHARED_APPLE				0x85BF
#define GL_VERTEX_ARRAY_RANGE_APPLE			0x851D
#define GL_VERTEX_ARRAY_BINDING_APPLE		0x85B5

#include "Engine.h"

#include "OpenGLResource.h"
#include "OpenGLRenderInterface.h"
#include "OpenGLRenderDevice.h"

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

