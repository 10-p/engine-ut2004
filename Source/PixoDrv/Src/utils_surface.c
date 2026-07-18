//-----------------------------------------------------------------------------
// FILE: UTILS_SURFACE.C
//
// Desc: Buffer APIS
//
// Copyright (c) 2002 RAD Game Tools, Inc. All rights reserved.
//-----------------------------------------------------------------------------
#include <malloc.h>
#include <memory.h>

#if __LINUX_X86__  // !!! FIXME: Unify headers!
#include "pixomatic_linux.h"
#else
#include "pixomatic.h"
#endif

#include "utils_surface.h"

//=========================================================================
// Open a PIXOSURFACE
//=========================================================================
PIXOSURFACE *PixoSurface_BufferOpen(int width, int height, int flags)
{
    PIXOSURFACE * psurf;
    int dsty = 0;

    if((flags & (PIXOSURFACE_2XZOOM | PIXOSURFACE_2XANTIALIAS)) == 
        (PIXOSURFACE_2XZOOM | PIXOSURFACE_2XANTIALIAS))
    {
        return 0;
    }

    psurf = (PIXOSURFACE *)malloc(sizeof(PIXOSURFACE));
    if(psurf)
    {
        memset(psurf, 0, sizeof(PIXOSURFACE));

        psurf->orig_width = width;
        psurf->orig_height = height;

        if(flags & PIXOSURFACE_2XZOOM)
        {
            height += 2;
            dsty = height - (height - 2) / 2;

            if((flags & PIXOSURFACE_2XZOOM_VERTICAL) == PIXOSURFACE_2XZOOM_VERTICAL)
            {
                psurf->scale_func = (PIXOSURFACE_BLITFUNC *)
                    ((flags & PIXOSURFACE_2XZOOM_NO_BILINEAR) ?
                    Pixo2xStretchVerticalARGB8888 : PixoBilinear2xStretchVerticalARGB8888);
            }
            else
            {
                psurf->scale_func = (PIXOSURFACE_BLITFUNC *)
                    ((flags & PIXOSURFACE_2XZOOM_NO_BILINEAR) ?
                    Pixo2xStretchARGB8888 : PixoBilinear2xStretchARGB8888);
            }
        }
        else if(flags & PIXOSURFACE_2XANTIALIAS)
        {
            width *= 2;
            height = height * 2 + 2;
            dsty = 2;
        }

        psurf->dsty = dsty;
        psurf->dibwidth = width;
        psurf->dibheight = height;
        psurf->dibpitch = width * 4;
        psurf->flags = flags;
        psurf->buffer = malloc(height * width * sizeof(PIXOSURFACE));

        if(!psurf->buffer)
        {
            free(psurf);
            psurf = 0;
        }
    }

    return psurf;
}

//=========================================================================
// Close a BBBuf
//=========================================================================
void PixoSurface_BufferClose(PIXOSURFACE * psurf)
{
    if(psurf)
    {
        free(psurf->z_buffer);
        free(psurf->buffer);

        memset(psurf, 0, sizeof(PIXOSURFACE));
        free(psurf);
    }
}

int PixoSurface_AttachZBuffer(PIXOSURFACE * psurf, PIXO_ZBUFFER_TYPE ZBufferType)
{
    if(psurf->z_buffer)
        free(psurf->z_buffer);

    if(ZBufferType == PIXO_ZBUFFER_TYPE_16)
    {
        psurf->ZBufferPitch = psurf->dibwidth * sizeof(unsigned short);
        psurf->z_buffer = malloc(psurf->dibheight * psurf->ZBufferPitch);
        psurf->ZBuffer = psurf->z_buffer;
    }
    else
    {
        // For 32-bit z buffers we pad them out from the frame buffer pointer
        // to avoid 64k aliasing conflicts on the P4. For details see the
        // Intel(r) Pentium(r) 4 and Intel(r) Xeon(tm) Processor Optimization
        // Reference Manual, Order Number: 248966-04

        psurf->ZBufferPitch = psurf->dibwidth * sizeof(unsigned int);
        psurf->z_buffer = malloc(psurf->dibheight * psurf->ZBufferPitch + 0x1000);
        psurf->ZBuffer = psurf->z_buffer;

        if(((int)psurf->ZBuffer & 0xf000) == ((int)psurf->buffer & 0xf000))
        {
            psurf->ZBuffer = ((char *)psurf->ZBuffer + 0x1000);
        }
    }

    return !!psurf->ZBuffer;
}

//=========================================================================
// Scale a zoomed or antialiased buffer to the final width, height
//=========================================================================
static void scale_buffer(PIXOSURFACE * psurf, void *dest, int dpitch)
{
    int spitch = psurf->dibpitch;
    void *src = psurf->buffer;

    if(psurf->flags & PIXOSURFACE_2XZOOM)
    {
        int sw;
        int sy = psurf->dsty;
        int sh = (psurf->dibheight - 2) / 2;

        if((psurf->flags & PIXOSURFACE_2XZOOM_VERTICAL) == PIXOSURFACE_2XZOOM_VERTICAL)
            sw = psurf->dibwidth;
        else
            sw = psurf->dibwidth / 2;

        psurf->scale_func(dest, 0, 0, dpitch, src, 0, sy, sw, sh, spitch);
    }
    else if(psurf->flags & PIXOSURFACE_2XANTIALIAS)
    {
        PixoBilinear2xShrinkARGB8888(dest, 0, 0, dpitch,
            src, 0, psurf->dsty, psurf->dibwidth, psurf->dibheight - 2, spitch);
    }

    psurf->scaled = 1;
}

//=========================================================================
// Lock a BBBuf
//=========================================================================
int PixoSurface_BufferLock(PIXOSURFACE * psurf, int flags)
{
    if(psurf->flags & (PIXOSURFACE_2XZOOM | PIXOSURFACE_2XANTIALIAS))
    {
        if(flags & PIXOSURFACE_LOCK_COMPOSITE)
        {
            if(!psurf->scaled)
                scale_buffer(psurf, psurf->buffer, psurf->dibpitch);

            psurf->BufferWidth = psurf->orig_width;
            psurf->BufferHeight = psurf->orig_height;
            psurf->BufferPitch = psurf->dibpitch;
            psurf->Buffer = psurf->buffer;
        }
        else if(psurf->flags & PIXOSURFACE_2XZOOM)
        {
            psurf->BufferWidth = ((psurf->flags & PIXOSURFACE_2XZOOM_VERTICAL) == PIXOSURFACE_2XZOOM_VERTICAL) ?
                psurf->dibwidth : psurf->dibwidth / 2;
            psurf->BufferHeight = (psurf->dibheight - 2) / 2;
            psurf->BufferPitch = psurf->dibpitch;
            psurf->Buffer = (char *)psurf->buffer + psurf->dibpitch * psurf->dsty;

            psurf->scaled = 0;
        }
        else if(psurf->flags & PIXOSURFACE_2XANTIALIAS)
        {
            psurf->BufferWidth = psurf->dibwidth;
            psurf->BufferHeight = psurf->dibheight - 2;
            psurf->BufferPitch = psurf->dibpitch;
            psurf->Buffer = (char *)psurf->buffer + psurf->dibpitch * psurf->dsty;

            psurf->scaled = 0;
        }
    }
    else
    {
        psurf->BufferWidth = psurf->dibwidth;
        psurf->BufferHeight = psurf->dibheight;
        psurf->BufferPitch = psurf->dibpitch;
        psurf->Buffer = psurf->buffer;
    }

    return 1;
}

void PixoSurface_BufferUnlock(PIXOSURFACE * psurf)
{
    psurf->Buffer = 0;
    psurf->BufferWidth = 0;
    psurf->BufferHeight = 0;
    psurf->BufferPitch = 0;
}

void PixoSurface_BufferBlitDest(PIXOSURFACE * psurf, void *dest, int dx, int dy, int dpitch,
    PIXOSURFACE_BLITFUNC *BlitFunc)
{
    PixoSurface_BufferBlitDestRect(psurf, dest, dx, dy, dpitch,
        0, 0, psurf->orig_width, psurf->orig_height, BlitFunc);
}

void PixoSurface_BufferBlitDestRect(PIXOSURFACE *psurf, void *dest, int dx, int dy, int dpitch,
    int sx, int sy, int sw, int sh, PIXOSURFACE_BLITFUNC *BlitFunc)
{
    if(!psurf->scaled)
    {
        //$ TODO: scale directly over to buffer if 8888
        scale_buffer(psurf, psurf->buffer, psurf->dibpitch);
    }

    if(BlitFunc)
    {
        BlitFunc(dest, dx, dy, dpitch,
            psurf->buffer, sx, sy, sw, sh, psurf->dibpitch);
    }
}

PIXOSURFACE_BLITFUNC *GetPixoBlitFunc(unsigned int BitsPerPixel,
    unsigned int Amask, unsigned int Rmask,
    unsigned int Gmask, unsigned int Bmask)
{
    if((Rmask == 0x00ff0000) &&
       (Gmask == 0x0000ff00) &&
       (Bmask == 0x000000ff))
    {
        return (BitsPerPixel == 32) ?
            (PIXOSURFACE_BLITFUNC *)PixoBlitARGB8888toARGB8888 : 
            (PIXOSURFACE_BLITFUNC *)PixoBlitARGB8888toRGB888;
    }
    else if(BitsPerPixel == 16)
    {
        if((Rmask == 0x7c00) &&
           (Gmask == 0x03e0) &&
           (Bmask == 0x001f))
        {
            return (PIXOSURFACE_BLITFUNC *)PixoBlitARGB8888toXRGB1555;
        }
        else if((Rmask == 0xf800) &&
                (Gmask == 0x07e0) &&
                (Bmask == 0x001f))
        {
            return (PIXOSURFACE_BLITFUNC *)PixoBlitARGB8888toRGB565;
        }
    }

    return 0;
}

