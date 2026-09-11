//-----------------------------------------------------------------------------
// FILE: UTILS_SURFACE.H
//
// Desc: Buffer API header
//
// Copyright (c) 2002 RAD Game Tools, Inc. All rights reserved.
//-----------------------------------------------------------------------------
#ifndef __UTILS_SURFACE_H
#define __UTILS_SURFACE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __GNUC__

#if !defined(__stdcall)
#define __stdcall      __attribute__((stdcall))
#endif

#if !defined(__cdecl)
#define __cdecl        __attribute__((cdecl))
#endif

#endif

// ★ ufront 2.58: the blit/stretch functions this points at are Pixomatic ARCHIVE entry points, and
// their calling convention is whatever PIXO_DYNLINK says — __stdcall on Windows (pixomatic.dll), plain
// cdecl on Linux (pixomatic_linux.a: `PIXO_DYNLINK(ret) ret` under BUILDING_PIXO_STATIC). This typedef
// said __stdcall unconditionally, so on Linux every `psurf->scale_func(...)` in scale_buffer() was a
// stdcall CALL SITE into a cdecl CALLEE: the caller did not pop its ten arguments (40 bytes) and neither
// did the callee, so esp came back 40 bytes low. At -O0 the frame pointer hid it (`leave` restores esp
// from ebp); at -O2 gcc omits the frame pointer, the epilogue restored ebx/esi/edi/ebp from the ARGUMENT
// area and then `ret` popped a framebuffer pointer as the return address — a jump into the (executable,
// READ_IMPLIES_EXEC) heap. Measured 2026-09-11 under gdb: pc in [heap], esp inside .data, ebx == 0x1400
// == the 1280-pixel ARGB pitch that was passed as `dpitch`. The path is armed by Zoom2X=True (the ini
// default): PIXO_BUF_LOCK_COMPOSITE -> PixoSurface_BufferLock -> scale_buffer; Zoom2X=False never called
// through the pointer and the same -O2 binary ran clean. Windows is unaffected: there PIXO_DYNLINK IS
// stdcall, so the cast was right all along.
#if defined(_WIN32)
typedef void __stdcall PIXOSURFACE_BLITFUNC(void *dest, int dx, int dy, int dpitch,
    void *src, int sx, int sy, int sw, int sh, int spitch);
#else
typedef void PIXOSURFACE_BLITFUNC(void *dest, int dx, int dy, int dpitch,
    void *src, int sx, int sy, int sw, int sh, int spitch);
#endif

typedef struct PIXOSURFACE PIXOSURFACE;
struct PIXOSURFACE
{
    // Parameters set by PixoSurface_BufferLock
    void *Buffer;
    int BufferWidth;
    int BufferHeight;
    int BufferPitch;

    // Parameters set by PixoSurface_AttachZBuffer
    void *ZBuffer;
    int ZBufferPitch;

    // Internal variables
    int dsty;
    int dibwidth;
    int dibheight;
    int dibpitch;
    int flags;

    void *buffer;
    void *z_buffer;

    int scaled;
    int orig_width;
    int orig_height;

    PIXOSURFACE_BLITFUNC *scale_func;
};

enum PIXOSURFACEOPEN_FLAGS
{
    PIXOSURFACE_2XZOOM               = 0x00000001,
    PIXOSURFACE_2XZOOM_VERTICAL      = 0x00008001,
    PIXOSURFACE_2XANTIALIAS          = 0x00000002,

    PIXOSURFACE_2XZOOM_NO_BILINEAR   = 0x80000000,
};
typedef enum PIXOSURFACEOPEN_FLAGS PIXOSURFACEOPEN_FLAGS;

enum PIXOSURFACELOCK_FLAGS
{
    PIXOSURFACE_LOCK_COMPOSITE = 0x00000001,
};
typedef enum PIXOSURFACELOCK_FLAGS PIXOSURFACELOCK_FLAGS;

PIXOSURFACE * PixoSurface_BufferOpen(int width, int height, int flags);
void PixoSurface_BufferClose(PIXOSURFACE *pbuf);

int PixoSurface_BufferLock(PIXOSURFACE *pbuf, int flags);
void PixoSurface_BufferUnlock(PIXOSURFACE *pbuf);

void PixoSurface_BufferBlitDest(PIXOSURFACE *pbuf, void *dest, int dx, int dy, int dpitch,
    PIXOSURFACE_BLITFUNC *BlitFunc);
void PixoSurface_BufferBlitDestRect(PIXOSURFACE *pbuf, void *dest, int dx, int dy, int dpitch,
    int sx, int sy, int sw, int sh, PIXOSURFACE_BLITFUNC *BlitFunc);

int PixoSurface_AttachZBuffer(PIXOSURFACE * pbuf, PIXO_ZBUFFER_TYPE ZBufferType);

PIXOSURFACE_BLITFUNC *GetPixoBlitFunc(unsigned int BitsPerPixel,
    unsigned int Amask, unsigned int Rmask,
    unsigned int Gmask, unsigned int Bmask);

#ifdef __cplusplus
};
#endif

#endif // __UTILS_SURFACE_H

