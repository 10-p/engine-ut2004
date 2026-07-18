//=============================================================================
// Additional D3D code
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#include "D3DDrv.h"

xD3DHelper::xD3DHelper()
{
}

xD3DHelper::~xD3DHelper()
{
	Shutdown();
}

// Init - Initialize the vertex buffer.
void xD3DHelper::Init(UD3DRenderDevice* InRenDev)
{
	guard(FD3DVertexBuffer::Init);

	Shutdown(); // free any resources
	// init quadlist
#if DO_QUAD_EMULATION
	debugf(TEXT("xD3DHelper::Init (QuadEmulation)"));
#else
	debugf(TEXT("xD3DHelper::Init (NoQuad)"));
#endif
	unguard;
}

void xD3DHelper::Shutdown(void)
{
	guard(xD3DHelper::Shutdown);
	RenDev = NULL;
	unguard;
}
