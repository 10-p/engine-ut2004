/*=============================================================================
	D3DRenderInterface.cpp: Unreal Direct3D rendering interface implementation.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "D3DDrv.h"
#include "xTexShader.h"

class FD3DDynVertStream : public FVertexStream // evil
{
public:
	QWORD		    CacheId;
    int             Revision;
    int             ByteSize;
    int             ByteStride;
    DWORD           ComponentFlags;
    TArray<BYTE>    ScratchBytes;

    void Init(BYTE** pOutBuffer, int numVerts, int stride, DWORD components)
    {
        ByteSize = numVerts * stride;
        ByteStride = stride;
        ComponentFlags = components;
        if( ByteSize > ScratchBytes.Num() )
        {
            ScratchBytes.Add( ByteSize - ScratchBytes.Num() );
        }
        (*pOutBuffer) = &ScratchBytes(0);
        Revision++;
    }

	FD3DDynVertStream()
	{
		CacheId = MakeCacheID(CID_RenderVertices);
        Revision = 1;
	}

	virtual QWORD GetCacheId()
	{
		return CacheId;
	}

	virtual INT GetRevision()
	{
		return Revision;
	}

	virtual INT GetSize()
	{
        return ByteSize;
	}

	virtual INT GetStride()
	{
		return ByteStride;
	}

	virtual INT GetComponents(FVertexComponent* OutComponents)
	{
        int numComps = 0;
        if( ComponentFlags & VF_Position )
        {
            OutComponents[numComps] = FVertexComponent(CT_Float3,FVF_Position);
            numComps++;
        }
        if( ComponentFlags & VF_Normal )
        {
            OutComponents[numComps] = FVertexComponent(CT_Float3,FVF_Normal);
            numComps++;
        }
        if( ComponentFlags & VF_Diffuse )
        {
            OutComponents[numComps] = FVertexComponent(CT_Color,FVF_Diffuse);
            numComps++;
        }
        if( ComponentFlags & VF_Specular )
        {
            OutComponents[numComps] = FVertexComponent(CT_Color,FVF_Specular);
            numComps++;
        }
        if( ComponentFlags & VF_Tex1 )
        {
            OutComponents[numComps] = FVertexComponent(CT_Float2,FVF_TexCoord0);
            numComps++;
        }
        if( ComponentFlags & VF_Tex2 )
        {
            OutComponents[numComps] = FVertexComponent(CT_Float2,FVF_TexCoord1);
            numComps++;
        }
        return numComps;
	}

	virtual void GetStreamData(void* Dest)
	{
        appMemcpy( Dest, &ScratchBytes(0), ByteSize );
	}

	virtual void GetRawStreamData(void ** Dest, INT FirstVertex )
	{
		*Dest = NULL;
	}
};

FD3DDynVertStream dynVertStream;

int FD3DRenderInterface::LockDynBuffer(BYTE** pOutBuffer, int numVerts, int stride, DWORD componentFlags)
{
    guard( FD3DRenderInterface::LockDynBuffer );
    if ( numVerts == 0 )
	{
		(*pOutBuffer) = NULL;
		return 0;
	}

    if ( RenDev->xHelper.QuadIB.MaxVertIndex==0 ) // FIXED - no trans-trail, initialize empty quad buffer
    {
        SetIndexBuffer( &RenDev->xHelper.QuadIB, 0 );
        SetIndexBuffer( NULL, 0 );
    }

#if DO_QUAD_EMULATION
    if( numVerts >= RenDev->xHelper.QuadIB.MaxVertIndex ) // prevent quad index overflow
        numVerts = RenDev->xHelper.QuadIB.MaxVertIndex-1;
#endif

    dynVertStream.Init( pOutBuffer, numVerts, stride, componentFlags );
	return numVerts;
	unguard;
}

int FD3DRenderInterface::UnlockDynBuffer( void )
{
	INT first = SetDynamicStream(VS_FixedFunction,&dynVertStream);
	return first;
}

void FD3DRenderInterface::DrawDynQuads(INT NumPrimitives)
{
    INT first = UnlockDynBuffer();
	if( NumPrimitives == 0 )
		return;
#if DO_QUAD_EMULATION
    SetIndexBuffer( &RenDev->xHelper.QuadIB, first );
	DrawPrimitive( PT_TriangleList, 0, NumPrimitives * 2, 0, NumPrimitives * 4 - 1 );
#else
    DrawPrimitive(PT_QuadList,first,NumPrimitives,0,NumPrimitives*2-1);
#endif
}

void FD3DRenderInterface::DrawQuads(INT FirstVertex, INT NumPrimitives)
{
	if( NumPrimitives == 0 )
		return;
#if DO_QUAD_EMULATION
	SetIndexBuffer( &RenDev->xHelper.QuadIB, FirstVertex );
    DrawPrimitive( PT_TriangleList, 0, NumPrimitives * 2, 0, NumPrimitives * 4 - 1 );
#else
    DrawPrimitive(PT_QuadList,FirstVertex,NumPrimitives,0,NumPrimitives*2-1);
#endif
}


