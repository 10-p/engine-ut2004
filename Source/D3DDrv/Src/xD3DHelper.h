//=============================================================================
// Additional D3D code
// Copyright 2001 Digital Extremes - All Rights Reserved.
// Confidential.
//=============================================================================

#ifndef xD3DHelper_H
#define xD3DHelper_H

#include "D3DDrv.h"

#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }
#ifdef _XBOX
    #define DO_QUAD_EMULATION 0
#else
    #define DO_QUAD_EMULATION 1  // no quads on PC
#endif

struct FD3DColorVertex
{
	enum {FVF=D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1};
	enum {USAGE=D3DUSAGE_WRITEONLY|D3DUSAGE_DYNAMIC};

	FVector Position;
    DWORD   Diffuse;
	FLOAT	UV[2];
};

class UD3DRenderDevice;
class FD3DRenderInterface;

// Constructor.
class FQuadIndexBuffer : public FIndexBuffer
{
public:
	QWORD			CacheId;
	enum { MAX_QUADS = 5461 };
    int             MaxVertIndex;

    // Constructor.
	FQuadIndexBuffer()
	{
        CacheId = MakeCacheID(CID_RenderIndices);
	}

	// FRenderResource interface.
	virtual INT GetRevision() { return 1; }
	virtual QWORD GetCacheId() { return CacheId; }

	// FIndexBuffer interface.
	virtual INT GetSize() { return 6 * MAX_QUADS * sizeof(_WORD); }
	virtual INT GetIndexSize() { return sizeof(_WORD); }
	virtual void GetContents(void* Data)
	{
        int numIdx = MAX_QUADS;
        //int numIdx = MAX_INDICES;

		_WORD* pIndex = (_WORD*)Data;
		int c = 0;
		for ( int i=0; i<numIdx; i++, c+=4 )
		{
			*pIndex++ = c+0;
			*pIndex++ = c+1;
			*pIndex++ = c+2;
			*pIndex++ = c+0;
			*pIndex++ = c+2;
			*pIndex++ = c+3;
		}
        MaxVertIndex = c;
	}
};

class xD3DHelper
{
public:
	friend class UD3DRenderDevice;
	friend class FD3DRenderInterface;
	xD3DHelper();
	~xD3DHelper();
	void Init(UD3DRenderDevice* InRenDev);
	void Shutdown();
protected:
	// quad list enumlation
#if DO_QUAD_EMULATION
	FQuadIndexBuffer        QuadIB;
#endif
	UD3DRenderDevice*		RenDev;
};

#endif//xD3DHelper_H
