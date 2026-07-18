//=============================================================================
// Additional D3D code
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#include "D3D9Drv.h"

xD3D9Helper::xD3D9Helper()
{
}

xD3D9Helper::~xD3D9Helper()
{
	Shutdown();
}

// Init - Initialize the vertex buffer.
void xD3D9Helper::Init(UD3D9RenderDevice* InRenDev)
{
	guard(FD3DVertexBuffer::Init);

	Shutdown(); // free any resources
	// init quadlist
#if DO_QUAD_EMULATION
	debugf(TEXT("xD3D9Helper::Init (QuadEmulation)"));
#else
	debugf(TEXT("xD3D9Helper::Init (NoQuad)"));
#endif
	unguard;
}

void xD3D9Helper::Shutdown(void)
{
	guard(xD3D9Helper::Shutdown);
	RenDev = NULL;
	unguard;
}