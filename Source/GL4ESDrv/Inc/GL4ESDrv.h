/*=============================================================================
	GL4ESDrv.h: Unreal GL4ES (GL-to-GLES2 translation) support header.
	Based on OpenGLDrv.h by Daniel Vogel.
	Modified for GL4ES integration.

=============================================================================*/

/*-----------------------------------------------------------------------------
	Includes.
-----------------------------------------------------------------------------*/
#ifndef HEADER_GL4ESDRV
#define HEADER_GL4ESDRV

// SDL2 for GL context management.
#include "SDL.h"

// GL4ES provides its own GL/gl.h and GL/glext.h that wrap the translation layer.
// These resolve via the include path set in CMakeLists.txt (${gl4es_SOURCE_DIR}/include).
#include "GL/gl.h"
#include "GL/glext.h"

// GL4ES initialization API: initialize_gl4es(), set_getprocaddress(), gl4es_GetProcAddress()
#include "gl4esinit.h"

// Wrap manual GL constant defines in #ifndef guards to avoid redefinition
// warnings if GL4ES's headers already define them.
#ifndef GL_MODULATE_ADD_ATI
#define GL_MODULATE_ADD_ATI					0x8744
#endif
#ifndef GL_VERTEX_ARRAY_STORAGE_HINT_APPLE
#define GL_VERTEX_ARRAY_STORAGE_HINT_APPLE	0x851F
#endif
#ifndef GL_STORAGE_CACHED_APPLE
#define GL_STORAGE_CACHED_APPLE				0x85BE
#endif
#ifndef GL_STORAGE_SHARED_APPLE
#define GL_STORAGE_SHARED_APPLE				0x85BF
#endif
#ifndef GL_VERTEX_ARRAY_RANGE_APPLE
#define GL_VERTEX_ARRAY_RANGE_APPLE			0x851D
#endif
#ifndef GL_VERTEX_ARRAY_BINDING_APPLE
#define GL_VERTEX_ARRAY_BINDING_APPLE		0x85B5
#endif

// Engine headers.
#include "Engine.h"

// Module headers.
#include "GL4ESResource.h"
#include "GL4ESRenderInterface.h"
#include "GL4ESRenderDevice.h"

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
