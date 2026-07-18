/*=============================================================================
	PixoRenderResource.cpp: Unreal Pixomatic support.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Michael Sartain from GL driver
		* Taken over by Daniel Vogel

=============================================================================*/

#include "PixoDrv.h"

//
// FPixoResource::FPixoResource
//
FPixoResource::FPixoResource(UPixoRenderDevice* InRenDev,QWORD InCacheId)
{
	guard(FPixoResource::FPixoResource);

	RenDev                          = InRenDev;
	CacheId                         = InCacheId;
	CachedRevision                  = 0;

	// Add this resource to the device resource list.
	NextResource                    = RenDev->ResourceList;
	RenDev->ResourceList            = this;

	// Calculate a hash index.
	HashIndex = GetResourceHashIndex(CacheId);

	// Add this resource to the device resource hash.
	HashNext                        = RenDev->ResourceHash[HashIndex];
	RenDev->ResourceHash[HashIndex] = this;

	LastFrameUsed                   = 0;

	unguard;
}

//
// FPixoResource::~FPixoResource
//
FPixoResource::~FPixoResource()
{
	guard(FPixoResource::~FPixoResource);

	FPixoResource*  ResourcePtr;

	// Remove this resource from the device resource list.
	if(RenDev->ResourceList != this)
	{
		ResourcePtr = RenDev->ResourceList;

		while(ResourcePtr && ResourcePtr->NextResource != this)
			ResourcePtr = ResourcePtr->NextResource;

		if(ResourcePtr)
			ResourcePtr->NextResource = NextResource;

		NextResource = NULL;
	}
	else
	{
		RenDev->ResourceList = NextResource;
		NextResource = NULL;
	}

	// Remove this resource from the device resource hash.
	if(RenDev->ResourceHash[HashIndex] != this)
	{
		ResourcePtr = RenDev->ResourceHash[HashIndex];

		while(ResourcePtr && ResourcePtr->HashNext != this)
			ResourcePtr = ResourcePtr->HashNext;

		if(ResourcePtr)
			ResourcePtr->HashNext = HashNext;

		HashNext = NULL;
	}
	else
	{
		RenDev->ResourceHash[HashIndex] = HashNext;
		HashNext = NULL;
	}

	unguard;
}

//
// FPixoTexture::FPixoTexture
//
FPixoTexture::FPixoTexture(UPixoRenderDevice* InRenDev,QWORD InCacheId)
: FPixoResource(InRenDev,InCacheId)
{
	FrameBuffer		= NULL;
	ZBuffer			= NULL;
	FrameBufferPitch= 0;
	ZBufferPitch	= 0;

	WasCubemap      = 0;

	memset(Texels, 0, sizeof(Texels));
	MaxMipLevel = 0;
}

//
// FPixoTexture::~FPixoTexture
//
FPixoTexture::~FPixoTexture()
{
	UnCache();
}

//
// FPixoTexture::CalculateFootprint
//
INT FPixoTexture::CalculateFootprint()
{
	INT Bytes = 0;

	for(INT MipIndex=0; MipIndex<CachedNumMips; MipIndex++)
		Bytes += GetBytesPerPixel( CachedFormat, (CachedWidth >> MipIndex) * (CachedHeight >> MipIndex) );

	if(Palette)
		Bytes = Bytes / 4 + 256 * 4;

	return Bytes;
}

//
// FPixoTexture::UnCache
//
void FPixoTexture::UnCache()
{
	if( MaxMipLevel )
	{
		for(INT MipLevel = 0; MipLevel < sizeof(Texels) / sizeof(Texels[0]); MipLevel++)
		{
			if(Texels[MipLevel])
			{
				delete [] Texels[MipLevel];
				Texels[MipLevel] = NULL;
			}
		}

		MaxMipLevel = 0;
	}

	delete [] FrameBuffer;
	delete [] ZBuffer;

	FrameBuffer = NULL;
	ZBuffer		= NULL;
}

//
// FPixoTexture::Cache
//
void FPixoTexture::Cache(FBaseTexture* SourceTexture)
{
	guard(FPixoTexture::Cache);

	INT     FirstMip = SourceTexture->GetFirstMip(),
			Width    = SourceTexture->GetWidth(),
			Height   = SourceTexture->GetHeight(),
			NumMips  = SourceTexture->GetNumMips();

	// Determine the source texture format.
	ETextureFormat  SourceFormat    =   SourceTexture->GetFormat();

	// Determine the actual texture format.
	ETextureFormat  ActualFormat    =   SourceFormat == TEXF_G16    ? TEXF_RGBA8 :
										SourceFormat == TEXF_RGBA7  ? TEXF_RGBA8 :
										SourceFormat;

	FCompositeTexture*  CompositeTexture    = SourceTexture->GetCompositeTextureInterface();
	FCubemap*           Cubemap             = SourceTexture->GetCubemapInterface();
	FTexture*           Texture             = SourceTexture->GetTextureInterface();
	FRenderTarget*      RenderTarget        = SourceTexture->GetRenderTargetInterface();

	// Cleverly limit texture size if wanted.
	if( RenDev->LimitTextureSize && !RenderTarget )
	{
		INT MaxSize = 256;
		if( Texture && Texture->GetUTexture() && ((Texture->GetUTexture()->LODSet == LODSET_None) || (Texture->GetUTexture()->LODSet == LODSET_Interface) ) )
			MaxSize = 2048;

		while(((Width >> FirstMip) > MaxSize) && ((Height >> FirstMip) > MaxSize))
			FirstMip++;

		if( FirstMip >= NumMips )
			FirstMip = NumMips - 1;
	}

	INT     USize    = Width >> FirstMip,
			VSize    = Height >> FirstMip;

	// Use first face cause cubemaps aren't supported.
	if( Cubemap )
	{
		Texture     = Cubemap->GetFace(0);
		Cubemap     = NULL;
		WasCubemap  = 1;
	}

	//
	//  CompositeTexture
	//
	if( CompositeTexture )
	{
		// In UT2003 lightmaps are the only composite textures.
		// They are opaque RGBA8888 512x512 without mipmaps.
		INT NumChildren = CompositeTexture->GetNumChildren();

		check(SourceFormat == TEXF_RGBA8);

		if( !MaxMipLevel ||
			CachedWidth != (Width >> FirstMip) ||
			CachedHeight != (Height >> FirstMip) ||
			CachedFirstMip != FirstMip ||
			CachedNumMips != NumMips ||
			CachedFormat != ActualFormat )
		{
			UnCache();

			CachedChildRevision.Empty(NumChildren);
		}

		if(CachedChildRevision.Num() != NumChildren)
		{
			CachedChildRevision.Empty(NumChildren);
			CachedChildRevision.AddZeroed(NumChildren);

			// Copy all children into the texture.
			BYTE*   Data    = new BYTE[USize * VSize * 4];
			INT     Pitch   = USize * 4;

			for(INT ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
			{
				INT         ChildX = 0,
							ChildY = 0;
				FTexture*   Child = CompositeTexture->GetChild(ChildIndex, &ChildX, &ChildY);
				INT         ChildRevision = Child->GetRevision();
				BYTE*       TileData = CalculateTexelPointer(Data, ActualFormat, Pitch, ChildX, ChildY);

				// Read the child texture into the locked region.
				Child->GetTextureData(0, TileData, Pitch, ActualFormat, 0);

				CachedChildRevision(ChildIndex) = ChildRevision;
			}

			Palette = NULL;
			Texels[0] = Data;
			MaxMipLevel = 1;
		}
		else
		{
			// Update modified subrects of the texture.
			for(INT ChildIndex = 0; ChildIndex < NumChildren; ChildIndex++)
			{
				INT         ChildX = 0,
							ChildY = 0;
				FTexture*   Child = CompositeTexture->GetChild(ChildIndex,&ChildX,&ChildY);
				INT         ChildRevision = Child->GetRevision();

				if(CachedChildRevision(ChildIndex) != ChildRevision)
				{
					INT ChildWidth  = Child->GetWidth(),
						ChildHeight = Child->GetHeight();
					BYTE* Data = new BYTE[ChildWidth * ChildHeight * 4];

					// Read the child texture into the locked region.
					Child->GetTextureData(0,(void*)Data, Width * 4, ActualFormat, 0);

					PixoBlitARGB8888toARGB8888(Texels[0],
											ChildX,
											ChildY,
											USize * 4,
											Data,
											0,
											0,
											ChildWidth,
											ChildHeight,
											ChildWidth * 4);

					delete [] Data;

					CachedChildRevision(ChildIndex) = ChildRevision;
				}
			}
		}
	}
	//
	// Texture
	//
	else if( Texture )
	{
		if( !MaxMipLevel ||
			CachedWidth != (Width >> FirstMip) ||
			CachedHeight != (Height >> FirstMip) ||
			CachedFirstMip != FirstMip ||
			CachedNumMips != NumMips ||
			CachedFormat != ActualFormat )
		{
			UnCache();
		}

		Palette = (ActualFormat == TEXF_P8) ?
			Texture->GetUTexture()->GetColors() : NULL;

		INT MaxLevel = 0;
		for( INT MipLevel = FirstMip; MipLevel < NumMips; MipLevel++ )
		{
			INT MipWidth    = Width >> MipLevel,
				MipHeight   = Height >> MipLevel;

			if( MipWidth < 2 || MipHeight < 2 )
				break;

			if((MipLevel - FirstMip) >= (sizeof(Texels) / sizeof(Texels[0])))
				break;

			BYTE  *Data = NULL;

			if( IsDXTC( ActualFormat ) )
			{
				Texture->GetUTexture()->ConvertDXT( MipLevel, 1, 0, &Data );
			}
			else if(ActualFormat == TEXF_P8)
			{
				Data = new BYTE[ MipWidth * MipHeight];
				Texture->GetTextureData( MipLevel, Data, MipWidth, ActualFormat, 0 );
			}
			else
			{
				Data = new BYTE[ MipWidth * MipHeight * 4];
				Texture->GetTextureData( MipLevel, Data, MipWidth * 4, ActualFormat, 0 );
			}

			Texels[MipLevel - FirstMip] = Data;

			MaxLevel++;
		}

		MaxMipLevel = MaxLevel;
	}
	//
	// RenderTarget
	//
	else if( RenderTarget )
	{
		if( !MaxMipLevel ||
			CachedWidth != (Width >> FirstMip) ||
			CachedHeight != (Height >> FirstMip) ||
			CachedFirstMip != FirstMip ||
			CachedNumMips != NumMips ||
			CachedFormat != ActualFormat )
		{
			UnCache();
		}

		check( ActualFormat == TEXF_RGBA8 );

		INT TypeSize = PixoGetZBufferType() == PIXO_ZBUFFER_TYPE_16 ? sizeof(unsigned short) : sizeof(unsigned int);

		FrameBuffer			= new BYTE[USize * VSize * sizeof(unsigned int)];
		ZBuffer				= new BYTE[USize * VSize * TypeSize ];

		FrameBufferPitch	= USize * sizeof(unsigned int);
		ZBufferPitch		= USize * TypeSize;
	}

	// Update format so "RESOURCES" spits out the correct number.
	if( IsDXTC( ActualFormat ) )
	{
		ActualFormat = TEXF_RGBA8;
	}

	// Update the cached info.
	CachedRevision  = SourceTexture->GetRevision();
	CachedFormat    = ActualFormat;
	CachedWidth     = Width >> FirstMip;
	CachedHeight    = Height >> FirstMip;
	CachedFirstMip  = FirstMip;
	CachedNumMips   = NumMips;

	unguard;
}


//
// FPixoVertexStream::FPixoVertexStream
//
FPixoVertexStream::FPixoVertexStream(UPixoRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB)
: FPixoResource(InRenDev,InCacheId)
{
	CachedSize      = 0;
	IsDynamicVB     = InIsDynamicVB;

    if( IsDynamicVB )
        Reallocate( INITIAL_DYNAMIC_VERTEXBUFFER_SIZE );
}

#define MAX_VERTEX_INDEX 65535

//
// Standard vertex arrays implementation.
//

//
// FPixoVertexStream::Cache
//
void FPixoVertexStream::Cache(FVertexStream* SourceStream)
{
    guard(FPixoVertexStream::Cache);

	check(!IsDynamicVB);

	INT Size    = Abs(SourceStream->GetSize());

	if( CachedSize != Size )
	{
		VertexData.Empty( Size );
		VertexData.Add( Size );
	}

	// Copy the stream data.
	SourceStream->GetStreamData( &VertexData(0) );

	CachedRevision  = SourceStream->GetRevision();
	CachedSize      = Size;

	unguard;
}


//
// FPixoVertexStream::Reallocate
//
void FPixoVertexStream::Reallocate(INT NewSize)
{
    guard(FPixoVertexStream::Reallocate);

	check(IsDynamicVB);

	debugf(TEXT("Allocating %u byte dynamic vertex buffer."),NewSize);

	VertexData.Empty( NewSize );
	VertexData.Add( NewSize );

	CachedSize  = NewSize;
	Tail        = 0;

	unguard;
}


//
// FPixoVertexStream::AddVertices
//
INT FPixoVertexStream::AddVertices(FVertexStream* Stream)
{
    guard(FPixoVertexStream::AddVertices);

	check(IsDynamicVB);

	INT Size    = Abs(Stream->GetSize()),
		Stride  = Stream->GetStride();

	if( Size > CachedSize )
		Reallocate( Size );

	if( (Stream->GetSize() < 0) || 
		(Tail + Stride + Size > CachedSize) || 
		(((DWORD) (Tail + Stride + Size) / Stride) > MAX_VERTEX_INDEX) )
	{
		Tail = 0;
	}

	// Determine the offset in the vertex buffer to allocate the vertices at.
	INT VertexBufferOffset = ((Tail + Stride - 1) / Stride) * Stride;

	Stream->GetStreamData( &VertexData(VertexBufferOffset) );

	// Update the tail pointer.
	Tail = VertexBufferOffset + Size;

	return VertexBufferOffset / Stride;

	unguard;
}

//
// FPixoIndexBuffer::FPixoIndexBuffer
//
FPixoIndexBuffer::FPixoIndexBuffer(UPixoRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicIB)
: FPixoResource(InRenDev,InCacheId)
{
	guard(FPixoIndexBuffer::FPixoIndexBuffer);

	CachedSize      = 0;
	IsDynamicIB     = InIsDynamicIB;

    if( IsDynamicIB )
        Reallocate( INITIAL_DYNAMIC_INDEXBUFFER_SIZE );

	unguard;
}


//
// FPixoIndexBuffer::Cache
//
void FPixoIndexBuffer::Cache(FIndexBuffer* SourceIndexBuffer)
{
    guard(FPixoIndexBuffer::Cache);

	INT Size = Max(2, SourceIndexBuffer->GetSize());

	if( CachedSize != Size )
	{
		check( SourceIndexBuffer->GetIndexSize() == sizeof(_WORD) );
		IndexData.Empty( Size );
		IndexData.Add( Size );
	}

	//!!vogel: TODO: why here but not in vertexbuffer
	if( CachedRevision != SourceIndexBuffer->GetRevision() )
		SourceIndexBuffer->GetContents(&IndexData(0));

	CachedRevision  = SourceIndexBuffer->GetRevision();
	CachedSize      = Size;

	unguard;
}


//
// FPixoIndexBuffer::Reallocate
//
void FPixoIndexBuffer::Reallocate(INT NewSize)
{
    guard(FPixoIndexBuffer::Reallocate);

	debugf(TEXT("Allocating %u byte dynamic index buffer."),NewSize);

	IndexData.Empty( NewSize );
	IndexData.Add( NewSize );

	CachedSize  = NewSize;
	Tail        = 0;

	unguard;
}

//
// FPixoIndexBuffer::AddIndices
//
INT FPixoIndexBuffer::AddIndices(FIndexBuffer* IndexBuffer)
{
    guard(FPixoIndexBuffer::AddIndices);

	INT Size    = IndexBuffer->GetSize(),
		Stride  = IndexBuffer->GetIndexSize();

	// If the dynamic index buffer isn't big enough to contain all the indices, resize it.
	if( Size > CachedSize )
		Reallocate( Size );

	// If the dynamic index buffer will overflow with the additional indices, flush it.
	if( Tail + Size > CachedSize )
		Tail = 0;

	INT IndexBufferOffset = Tail;
	IndexBuffer->GetContents( &IndexData(IndexBufferOffset) );

	// Update the tail pointer.
	Tail = IndexBufferOffset + Size;

	return IndexBufferOffset / Stride;

	//!!vogel: TODO: investigate revision mojo

	unguard;
}

//
// FPixoVertexShader::FPixoVertexShader
//
FPixoVertexShader::FPixoVertexShader(UPixoRenderDevice* InRenDev,FShaderDeclaration& InDeclaration)
{
	guard(FPixoVertexShader::FPixoVertexShader);

	RenDev                  = InRenDev;
	Declaration             = InDeclaration;

	// Add the vertex shader to the device vertex shader list.
	NextVertexShader        = RenDev->VertexShaders;
	RenDev->VertexShaders   = this;

	unguard;
}

//
// FPixoVertexShader::~FPixoVertexShader
//
FPixoVertexShader::~FPixoVertexShader()
{
	guard(FPixoVertexShader::~FPixoVertexShader);

	// Remove the vertex shader from the device vertex shader list.
	FPixoVertexShader*  ShaderPtr;

	if(RenDev->VertexShaders != this)
	{
		ShaderPtr = RenDev->VertexShaders;

		while(ShaderPtr && ShaderPtr->NextVertexShader != this)
			ShaderPtr = ShaderPtr->NextVertexShader;

		if(ShaderPtr)
			ShaderPtr->NextVertexShader = NextVertexShader;

		NextVertexShader = NULL;
	}
	else
	{
		RenDev->VertexShaders = NextVertexShader;
		NextVertexShader = NULL;
	}

	unguard;
}


//
// FPixoFixedVertexShader::FPixoFixedVertexShader
//
FPixoFixedVertexShader::FPixoFixedVertexShader(UPixoRenderDevice* InRenDev,FShaderDeclaration& InDeclaration)
: FPixoVertexShader( InRenDev, InDeclaration )
{
	guard(FPixoFixedVertexShader::FPixoFixedVertexShader);
	Type = VS_FixedFunction;
	unguard;
}




/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

