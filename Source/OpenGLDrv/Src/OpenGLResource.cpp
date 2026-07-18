/*=============================================================================
	OpenGLRenderResource.cpp: Unreal OpenGL support.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.
	
	Revision history:
	* Created by Daniel Vogel

=============================================================================*/

#include "OpenGLDrv.h"

//
// FOpenGLResource::FOpenGLResource
//
FOpenGLResource::FOpenGLResource(UOpenGLRenderDevice* InRenDev,QWORD InCacheId)
{
	guard(FOpenGLResource::FOpenGLResource);

	RenDev							= InRenDev;
	CacheId							= InCacheId;
	CachedRevision					= 0;

	// Add this resource to the device resource list.
	NextResource					= RenDev->ResourceList;
	RenDev->ResourceList			= this;

	// Calculate a hash index.
	HashIndex = GetResourceHashIndex(CacheId);

	// Add this resource to the device resource hash.
	HashNext						= RenDev->ResourceHash[HashIndex];
	RenDev->ResourceHash[HashIndex]	= this;

	LastFrameUsed					= 0;

	unguard;
}

//
// FOpenGLResource::~FOpenGLResource
//
FOpenGLResource::~FOpenGLResource()
{
	guard(FOpenGLResource::~FOpenGLResource);

	FOpenGLResource*	ResourcePtr;

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
// FOpenGLTexture::FOpenGLTexture
//
FOpenGLTexture::FOpenGLTexture(UOpenGLRenderDevice* InRenDev,QWORD InCacheId)
: FOpenGLResource(InRenDev,InCacheId)
{
	TextureID		= 0;
	IsCubemap		= 0;
	RenderTargetSurface = 0;
	DepthStencilSurface = 0;
	WasCubemap		= 0;
}

//
// FOpenGLTexture::~FOpenGLTexture
//
FOpenGLTexture::~FOpenGLTexture()
{
	if( RenderTargetSurface )
	{
		check(RenDev->SUPPORTS_GL_EXT_framebuffer_object);
		RenDev->glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
		if( DepthStencilSurface )
			RenDev->glDeleteRenderbuffersEXT(1, &DepthStencilSurface);
		RenDev->glDeleteFramebuffersEXT(1, &RenderTargetSurface);
	}
    else
	{
		check(!DepthStencilSurface); // these are created as a pair with the RenderTargetSurface, currently.
	}

	if( TextureID )
		RenDev->glDeleteTextures( 1, &TextureID );
}

//
// FOpenGLTexture::CalculateFootprint
//
INT FOpenGLTexture::CalculateFootprint()
{
	INT	Bytes = 0;

	for(INT MipIndex=0; MipIndex<CachedNumMips; MipIndex++)
		Bytes += GetBytesPerPixel( CachedFormat, (CachedWidth >> MipIndex) * (CachedHeight >> MipIndex) );

	if( IsCubemap )
		Bytes *= 6;

	return Bytes;
}


//
// FOpenGLTexture::Cache
//
void FOpenGLTexture::Cache(FBaseTexture* SourceTexture)
{
	guard(FOpenGLTexture::Cache);

	INT		FirstMip = SourceTexture->GetFirstMip(),
			Width	 = SourceTexture->GetWidth(),
			Height	 = SourceTexture->GetHeight(),
			NumMips  = SourceTexture->GetNumMips();

	// Determine the source texture format.
	ETextureFormat	SourceFormat	=	SourceTexture->GetFormat();

	// Determine the actual texture format.
	ETextureFormat	ActualFormat	=	SourceFormat == TEXF_P8		? TEXF_RGBA8 :
										SourceFormat == TEXF_G16	? TEXF_RGBA8 :
										SourceFormat == TEXF_RGBA7	? TEXF_RGBA8 :
										SourceFormat;

	// Whether to use generic compressed internal format.
	UBOOL ForceCompression = RenDev->ForceCompression && IsDXTC(ActualFormat);

	INT		USize    = Width >> FirstMip,
			VSize	 = Height >> FirstMip;

	FCompositeTexture*	CompositeTexture	= SourceTexture->GetCompositeTextureInterface();
	FCubemap*			Cubemap				= SourceTexture->GetCubemapInterface();
	FTexture*			Texture				= SourceTexture->GetTextureInterface();
	FRenderTarget*		RenderTarget		= SourceTexture->GetRenderTargetInterface();

	// Use first face if cubemaps aren't supported.
	if( Cubemap && !RenDev->SupportsCubemaps ) 
	{
		Texture		= Cubemap->GetFace(0);
		Cubemap		= NULL;
		WasCubemap	= 1;
	}

	//
	//	CompositeTexture
	//
	if( CompositeTexture )
	{
		// In UT2003 lightmaps are the only composite textures. They are opaque RGBA8888 512x512 without mipmaps.
		INT	NumChildren = CompositeTexture->GetNumChildren();

		if( !TextureID || CachedWidth != (Width >> FirstMip) || CachedHeight != (Height >> FirstMip) || CachedFirstMip != FirstMip || CachedNumMips != NumMips || CachedFormat != ActualFormat )
		{
			if( TextureID )
				RenDev->glDeleteTextures( 1, &TextureID );	
			RenDev->glGenTextures( 1, &TextureID );

			RenDev->glBindTexture( GL_TEXTURE_2D, TextureID );

			RenDev->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
			RenDev->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

			CachedChildRevision.Empty(NumChildren);
		}

		RenDev->glBindTexture( GL_TEXTURE_2D, TextureID );

		if(CachedChildRevision.Num() != NumChildren)
		{
			CachedChildRevision.Empty(NumChildren);
			CachedChildRevision.AddZeroed(NumChildren);

			// Copy all children into the texture.
			BYTE*	Data	= new BYTE[USize * VSize * 4];
			INT		Pitch	= USize * 4;

			for(INT ChildIndex = 0;ChildIndex < NumChildren;ChildIndex++)
			{
				INT			ChildX = 0,
							ChildY = 0;
				FTexture*	Child = CompositeTexture->GetChild(ChildIndex,&ChildX,&ChildY);
				INT			ChildRevision = Child->GetRevision();
				BYTE*		TileData = CalculateTexelPointer(Data,ActualFormat,Pitch,ChildX,ChildY);

				// Read the child texture into the locked region.
				Child->GetTextureData(0,TileData,Pitch,ActualFormat,0);

				CachedChildRevision(ChildIndex) = ChildRevision;
			}

			RenDev->glTexImage2D(
					GL_TEXTURE_2D,
					0,
					GL_RGBA,
					USize,
					VSize,
					0,
					GL_BGRA,
					GL_UNSIGNED_BYTE,
					Data
			);
			
			delete [] Data;

			RenDev->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );	
		}
		else
		{
			// Update modified subrects of the texture.
			for(INT ChildIndex = 0;ChildIndex < NumChildren;ChildIndex++)
			{
				INT			ChildX = 0,
							ChildY = 0;
				FTexture*	Child = CompositeTexture->GetChild(ChildIndex,&ChildX,&ChildY);
				INT			ChildRevision = Child->GetRevision();

				if(CachedChildRevision(ChildIndex) != ChildRevision)
				{
					INT ChildWidth  = Child->GetWidth(),
						ChildHeight = Child->GetHeight();
					BYTE* Data = new BYTE[ChildWidth * ChildHeight * 4];
					
					// Read the child texture into the locked region.
					Child->GetTextureData(0,(void*) Data,Width * 4,ActualFormat,0);

					RenDev->glTexSubImage2D(
							GL_TEXTURE_2D,
							0,
							ChildX,
							ChildY,
							ChildWidth,
							ChildHeight,
							GL_BGRA,
							GL_UNSIGNED_BYTE,
							Data
					);

					delete [] Data;

					CachedChildRevision(ChildIndex) = ChildRevision;
				}
			}
		}
	}
	//
	// RenderTarget
	//
	else
	if( RenderTarget )
	{
		if (RenDev->SUPPORTS_GL_EXT_framebuffer_object)
		{
            check(FirstMip == 0);
            check(NumMips == 1);
			RenDev->GLError(TEXT("Before render target cache"));

            GLenum Depth = GL_DEPTH_COMPONENT24;

			if( !TextureID || CachedWidth != (Width >> FirstMip) || CachedHeight != (Height >> FirstMip) || CachedFirstMip != FirstMip || CachedNumMips != NumMips || CachedFormat != ActualFormat )
			{
				if( TextureID )
					RenDev->glDeleteTextures( 1, &TextureID );
				RenDev->glGenTextures( 1, &TextureID );
				RenDev->glBindTexture( GL_TEXTURE_2D, TextureID );
				RenDev->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
				RenDev->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				RenDev->glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
				RenDev->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0 );
				if( RenDev->SUPPORTS_GL_EXT_texture_filter_anisotropic )
					RenDev->glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, RenDev->LevelOfAnisotropy );
			}
			if( RenderTargetSurface )
				RenDev->glDeleteFramebuffersEXT(1, &RenderTargetSurface);
			if( DepthStencilSurface )
				RenDev->glDeleteRenderbuffersEXT(1, &DepthStencilSurface);

			RenDev->glGenFramebuffersEXT(1, &RenderTargetSurface);
			RenDev->glGenRenderbuffersEXT(1, &DepthStencilSurface);
			RenDev->glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, RenderTargetSurface);
			RenDev->glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_COLOR_ATTACHMENT0_EXT, GL_TEXTURE_2D, TextureID, 0);
			RenDev->glBindRenderbufferEXT(GL_RENDERBUFFER_EXT, DepthStencilSurface);
			RenDev->glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT, Depth, Width, Height);
			RenDev->glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_RENDERBUFFER_EXT, DepthStencilSurface);

			GLenum rc = RenDev->glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT);
			if ((rc != GL_FRAMEBUFFER_COMPLETE_EXT) || (RenDev->glGetError()))
			{
				debugf(TEXT("OpenGL: Failed to create render target!"));
				RenDev->glBindFramebufferEXT(GL_FRAMEBUFFER_EXT, 0);
				RenDev->glDeleteRenderbuffersEXT(1, &DepthStencilSurface);
				RenDev->glDeleteFramebuffersEXT(1, &RenderTargetSurface);
				RenderTargetSurface = DepthStencilSurface = 0;
			}
		}
	}
	//
	// Texture
	//
	else
	if( Texture )
	{
		if( !TextureID || CachedWidth != (Width >> FirstMip) || CachedHeight != (Height >> FirstMip) || CachedFirstMip != FirstMip || CachedNumMips != NumMips || CachedFormat != ActualFormat )
		{
			if( TextureID )
				RenDev->glDeleteTextures( 1, &TextureID );	
			RenDev->glGenTextures( 1, &TextureID );

			RenDev->glBindTexture( GL_TEXTURE_2D, TextureID );

			RenDev->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, RenDev->UseTrilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST );
			RenDev->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
			if( RenDev->SUPPORTS_GL_EXT_texture_filter_anisotropic )
				RenDev->glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, RenDev->LevelOfAnisotropy );
		}

		RenDev->glBindTexture( GL_TEXTURE_2D, TextureID );

		INT MaxLevel = 0;
		for( INT MipLevel=FirstMip; MipLevel<NumMips; MipLevel++ )
		{
			GLenum Format;
			switch( ActualFormat )
			{
			case TEXF_RGBA8:
				Format = GL_BGRA;
				break;
			case TEXF_DXT1:
				Format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
				break;
			case TEXF_DXT3:
				Format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
				break;
			case TEXF_DXT5:
				Format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				break;
			default:
				Format = GL_BGRA;
				break;
			}

			// Fall back to decompressing if S3TC is not supported.
			if( !RenDev->SUPPORTS_GL_EXT_texture_compression_s3tc )
				Format = GL_BGRA;

			INT MipWidth	= Width >> MipLevel,
				MipHeight	= Height >> MipLevel;

			if( IsDXTC(ActualFormat) && RenDev->SUPPORTS_GL_EXT_texture_compression_s3tc )
			{	
				if( MipWidth < 4 || MipHeight < 4 )
					break;

				UBOOL TemporaryData = 0;
				BYTE* Data = (BYTE*) Texture->GetRawTextureData( MipLevel );
				
				// Initialize to black if corrupted data.
				if( !Data )
				{
					Data = new BYTE[ MipWidth * MipHeight];
					appMemzero( Data, MipWidth * MipHeight );
					TemporaryData = 1;
				}

				RenDev->glCompressedTexImage2DARB(
					GL_TEXTURE_2D, 
					MipLevel - FirstMip,
					Format,
					MipWidth,
					MipHeight,
					0,
					GetBytesPerPixel(ActualFormat, MipWidth * MipHeight ),
					Data
				);

				if( TemporaryData )
					delete [] Data;

				Texture->UnloadRawTextureData( MipLevel );
			}
			else
			{	
				if( MipWidth < 2 || MipHeight < 2 )
					break;

				UBOOL TemporaryData	= 0;
				BYTE* Data			= NULL;
				
				if( !IsDXTC( ActualFormat ) )
					Data = (BYTE*) Texture->GetRawTextureData( MipLevel );

				if( !Data || ActualFormat != SourceFormat )
				{	
					if( IsDXTC( ActualFormat ) )
					{
						Texture->GetUTexture()->ConvertDXT( MipLevel, 1, 0, &Data );				
					}
					else
					{
						Data = new BYTE[ MipWidth * MipHeight * 4];
						Texture->GetTextureData( MipLevel, Data, MipWidth * 4, ActualFormat, 0 );
					}
					TemporaryData = 1;
				}

				check(Format == GL_BGRA);
				RenDev->glTexImage2D(
					GL_TEXTURE_2D,
					MipLevel - FirstMip,
					ForceCompression ? GL_COMPRESSED_RGBA_ARB : GL_RGBA,
					MipWidth,
					MipHeight,
					0,
					Format,
					GL_UNSIGNED_BYTE,
					Data
				);
			
				if( TemporaryData )
					delete [] Data;
				
				Texture->UnloadRawTextureData( MipLevel );
			}

			MaxLevel++;
		}

		RenDev->glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, Max(0,MaxLevel-1) );
	}	
	//
	// Cubemap
	//
	else
	if( Cubemap )
	{
		if( !TextureID || CachedWidth != (Width >> FirstMip) || CachedHeight != (Height >> FirstMip) || CachedFirstMip != FirstMip || CachedNumMips != NumMips || CachedFormat != ActualFormat )
		{
			if( TextureID )
				RenDev->glDeleteTextures( 1, &TextureID );	
			RenDev->glGenTextures( 1, &TextureID );

			RenDev->glBindTexture( GL_TEXTURE_CUBE_MAP, TextureID );

			RenDev->glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, RenDev->UseTrilinear ? GL_LINEAR_MIPMAP_LINEAR : GL_LINEAR_MIPMAP_NEAREST );
			RenDev->glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		}

		RenDev->glBindTexture( GL_TEXTURE_CUBE_MAP, TextureID );

		INT MaxLevel = 0;
		for( INT MipLevel=FirstMip; MipLevel<NumMips; MipLevel++ )
		{
			GLenum Format;
			switch( ActualFormat )
			{
			case TEXF_RGBA8:
				Format = GL_BGRA_EXT;
				break;
			case TEXF_DXT1:
				Format = GL_COMPRESSED_RGBA_S3TC_DXT1_EXT;
				break;
			case TEXF_DXT3:
				Format = GL_COMPRESSED_RGBA_S3TC_DXT3_EXT;
				break;
			case TEXF_DXT5:
				Format = GL_COMPRESSED_RGBA_S3TC_DXT5_EXT;
				break;
			default:
				Format = GL_BGRA_EXT;
				break;
			}

			// Fall back to decompressing if S3TC is not supported.
			if( !RenDev->SUPPORTS_GL_EXT_texture_compression_s3tc )
				Format = GL_BGRA;

			INT MipWidth	= Width >> MipLevel,
				MipHeight	= Height >> MipLevel;

			for( INT FaceIndex=0; FaceIndex<6; FaceIndex ++ )
			{
				FTexture* Texture = Cubemap->GetFace( FaceIndex );

				if (!Texture)
					continue;

				if( IsDXTC(ActualFormat) && RenDev->SUPPORTS_GL_EXT_texture_compression_s3tc )
				{	
					if( MipWidth < 4 || MipHeight < 4 )
						break;

					check(Texture->GetRawTextureData( MipLevel ));
					RenDev->glCompressedTexImage2DARB(
						GL_TEXTURE_CUBE_MAP_POSITIVE_X + FaceIndex, 
						MipLevel - FirstMip,
						Format,
						MipWidth,
						MipHeight,
						0,
						GetBytesPerPixel(ActualFormat, MipWidth * MipHeight ),
						Texture->GetRawTextureData( MipLevel )
					);
					
					Texture->UnloadRawTextureData( MipLevel );
				}
				else
				{	
					UBOOL TemporaryData	= 0;
					BYTE* Data			= NULL;

					if( !IsDXTC( ActualFormat ) )
						Data = (BYTE*) Texture->GetRawTextureData( MipLevel );

					if( !Data || ActualFormat != SourceFormat )
					{	
						if( IsDXTC( ActualFormat ) )
						{
							Texture->GetUTexture()->ConvertDXT( MipLevel, 1, 0, &Data );				
						}
						else
						{
							Data = new BYTE[MipWidth * MipHeight * 4];
							Texture->GetTextureData( MipLevel, Data, MipWidth * 4, ActualFormat, 0 );
						}
						TemporaryData = 1;
					}

					RenDev->glTexImage2D(
						GL_TEXTURE_CUBE_MAP_POSITIVE_X + FaceIndex,
						MipLevel - FirstMip,
						ForceCompression ? GL_COMPRESSED_RGBA_ARB : GL_RGBA,
						MipWidth,
						MipHeight,
						0,
						Format,
						GL_UNSIGNED_BYTE,
						Data
					);
				
					if( TemporaryData )
						delete [] Data;
					
					Texture->UnloadRawTextureData( MipLevel );
				}

				if( FaceIndex == 0 )
					MaxLevel++;

			}
		}

		RenDev->glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAX_LEVEL, Max(0,MaxLevel-1) );

		IsCubemap = 1;
	}

	// Update format so "RESOURCES" spits out the correct number.
	if( !RenDev->SUPPORTS_GL_EXT_texture_compression_s3tc && IsDXTC( ActualFormat ) )
	{
		if( ForceCompression )
			ActualFormat = TEXF_DXT3; // roughly accurate
		else
			ActualFormat = TEXF_RGBA8;
	}

	// Update the cached info.
	CachedRevision	= SourceTexture->GetRevision();
	CachedFormat	= ActualFormat;
	CachedWidth		= Width >> FirstMip;
	CachedHeight	= Height >> FirstMip;
	CachedFirstMip	= FirstMip;
	CachedNumMips	= NumMips;

	unguard;
}


//
// FOpenGLVertexStream::FOpenGLVertexStream
//
FOpenGLVertexStream::FOpenGLVertexStream(UOpenGLRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB)
: FOpenGLResource(InRenDev,InCacheId)
{
	CachedSize		= 0;
	IsDynamicVB		= InIsDynamicVB;
}

#define MAX_VERTEX_INDEX 65535

//
// Standard vertex arrays implementation.
//

//
// FOpenGLVertexStreamARB_VA::Cache
//
void FOpenGLVertexStreamARB_VA::Cache(FVertexStream* SourceStream)
{
	guard(FOpenGLVertexStreamARB_VA::Cache);

	check(!IsDynamicVB);

	INT Size	= Abs(SourceStream->GetSize()),
		Stride	= SourceStream->GetStride();

	if( CachedSize != Size )
	{
		VertexData.Empty( Size );
		VertexData.Add( Size );		
	}

	// Copy the stream data.
	SourceStream->GetStreamData( &VertexData(0) );

	// D3D -> OpenGL color conversion for static data.
	// !!warning: relies on data layout not changing
	if( !SourceStream->HintDynamic() )
	{
		UBOOL RevCompsOrig = GIsOpenGLRevComps;
		if( RenDev->SUPPORTS_GL_ATI_array_rev_comps_in_4_bytes )
			GIsOpenGLRevComps = 1;  // RenderColor always byteswaps in here...

		FVertexComponent Components[MAX_VERTEX_COMPONENTS];
		INT NumComponents = SourceStream->GetComponents( Components );
		INT Offset = 0;
		BYTE* DataEnd = &VertexData(0) + Size - 4;
		for( INT i=0; i<NumComponents; i++ )
		{
			switch( Components[i].Type )
			{
			case CT_Float4:
				Offset += 4*4;
				break;
			case CT_Float3:
				Offset += 3*4;
				break;
			case CT_Float2:
				Offset += 2*4;
				break;
			case CT_Float1:
				Offset += 1*4;
				break;
			case CT_Color:
				{	
					FColor* Colors = (FColor*) (&VertexData(0) + Offset);
					while( (BYTE*) Colors <= DataEnd )
					{
						*Colors = (*Colors).RenderColor();
						Colors = (FColor*) ((BYTE*) Colors + Stride);
					}
					Offset += 1*4;
					break;
				}
			}
		}

		GIsOpenGLRevComps = RevCompsOrig;
	}

	CachedRevision	= SourceStream->GetRevision();
	CachedSize		= Size;
	
	unguard;
}


//
// FOpenGLVertexStreamARB_VA::Reallocate
//
void FOpenGLVertexStreamARB_VA::Reallocate(INT NewSize)
{
	guard(FOpenGLVertexStream_VA::Reallocate);

	check(IsDynamicVB);

	debugf(TEXT("Allocating %u byte dynamic vertex buffer."),NewSize);

	VertexData.Empty( NewSize );
	VertexData.Add( NewSize );
	
	CachedSize	= NewSize;
	Tail		= 0;

	unguard;
}


//
// FOpenGLVertexStreamARB_VA::AddVertices
//
INT FOpenGLVertexStreamARB_VA::AddVertices(FVertexStream* Stream)
{
	guard(FOpenGLVertexStream_VA::AddVertices);

	check(IsDynamicVB);

	INT Size	= Abs(Stream->GetSize()),
		Stride	= Stream->GetStride();

	if( Size > CachedSize )
		Reallocate( Size );

	if( (Stream->GetSize() < 0) ||  (Tail + Stride + Size > CachedSize) || (((DWORD) (Tail + Stride + Size) / Stride) > MAX_VERTEX_INDEX) )
		Tail = 0;

	// Determine the offset in the vertex buffer to allocate the vertices at.
	INT	VertexBufferOffset = ((Tail + Stride - 1) / Stride) * Stride;

	Stream->GetStreamData( &VertexData(VertexBufferOffset) );

	// Update the tail pointer.
	Tail = VertexBufferOffset + Size;

	return VertexBufferOffset / Stride;

	unguard;
}


//
// NV_VAR & APPLE_VAR implemention.
//

//
// FOpenGLVertexStreamNV_VAR::Cache
//
void FOpenGLVertexStreamNV_VAR::Cache(FVertexStream* SourceStream)
{
	guard(FOpenGLVertexStreamNV_VAR::Cache);

	check(!IsDynamicVB);
	check(!SourceStream->HintDynamic());

	INT Size	= Abs(SourceStream->GetSize()),
		Stride	= SourceStream->GetStride();

	if( CachedSize && CachedSize != Size )
		appErrorf(TEXT("Size changed."));

	if( RenDev->ScratchBuffer.Num() < Size )
	{
		RenDev->ScratchBuffer.Empty();
		RenDev->ScratchBuffer.Add( Size * 2 );
	}
	BYTE* Data = &RenDev->ScratchBuffer(0);

	// Copy the stream data.
	SourceStream->GetStreamData( Data );

	// D3D -> OpenGL color conversion for static data.
	// !!warning: relies on data layout not changing

	UBOOL RevCompsOrig = GIsOpenGLRevComps;
	if( RenDev->SUPPORTS_GL_ATI_array_rev_comps_in_4_bytes )
		GIsOpenGLRevComps = 1;  // RenderColor always byteswaps in here...

	FVertexComponent Components[MAX_VERTEX_COMPONENTS];
	INT NumComponents = SourceStream->GetComponents( Components );
	INT Offset = 0;
	BYTE* DataEnd = Data + Size - 4;
	for( INT i=0; i<NumComponents; i++ )
	{
		switch( Components[i].Type )
		{
		case CT_Float4:
			Offset += 4*4;
			break;
		case CT_Float3:
			Offset += 3*4;
			break;
		case CT_Float2:
			Offset += 2*4;
			break;
		case CT_Float1:
			Offset += 1*4;
			break;
		case CT_Color:
			{	
				FColor* Colors = (FColor*) (Data + Offset);
				while( (BYTE*) Colors <= DataEnd )
				{
					*Colors = (*Colors).RenderColor();
					Colors = (FColor*) ((BYTE*) Colors + Stride);
				}
				Offset += 1*4;
				break;
			}
		}
	}

	GIsOpenGLRevComps = RevCompsOrig;

	if( CachedSize != Size )
	{
		VertexData = RenDev->VARPointer + RenDev->VARIndex;
		RenDev->VARIndex += Size;
	}

#if MACOSX
	appMemcpy( VertexData, Data, Size );
	RenDev->glVertexArrayRangeAPPLE( RenDev->VARSize, RenDev->VARPointer );
	RenDev->glFlushVertexArrayRangeAPPLE( Size, VertexData );
#else
	RenDev->glVertexArrayRangeNV( 0, 0 );
	appMemcpy( VertexData, Data, Size );
	RenDev->glVertexArrayRangeNV( RenDev->VARSize, RenDev->VARPointer );
#endif

	CachedRevision	= SourceStream->GetRevision();
	CachedSize		= Size;
	
	unguard;
}


//
// FOpenGLVertexStreamNV_VAR::AddVertices
//
INT FOpenGLVertexStreamNV_VAR::AddVertices(FVertexStream* Stream)
{
	guard(FOpenGLVertexStreamNV_VAR::AddVertices);

	check(IsDynamicVB);

	INT Size	= Abs(Stream->GetSize()),
		Stride	= Stream->GetStride();

	if( Size > CachedSize )
	{
		appErrorf(TEXT("Using %i Bytes of dynamic data failed."), Size );
		//Reallocate( Size );
	}

	if( (Stream->GetSize() < 0) ||  (Tail + Stride + Size > CachedSize) || (((DWORD) (Tail + Stride + Size) / Stride) > MAX_VERTEX_INDEX) )
		Tail = 0;

	//!!TODO: use fence

	// Determine the offset in the vertex buffer to allocate the vertices at.
	INT	VertexBufferOffset = ((Tail + Stride - 1) / Stride) * Stride;

	Stream->GetStreamData( VertexData + VertexBufferOffset );

	// Update the tail pointer.
	Tail = VertexBufferOffset + Size;

	return VertexBufferOffset / Stride;

	unguard;
}


//
// FOpenGLVertexStreamNV_VAR::Reallocate
//
void FOpenGLVertexStreamNV_VAR::Reallocate(INT NewSize)
{
	guard(FOpenGLVertexStreamNV_VAR::Reallocate);

	check(IsDynamicVB);

	debugf(TEXT("Allocating %u byte dynamic vertex buffer."),NewSize);

	VertexData = RenDev->VARPointer + RenDev->VARIndex;
	RenDev->VARIndex += NewSize;
	
	CachedSize	= NewSize;
	Tail		= 0;

	unguard;
}


//
// ARB_VBO implementation.
//

//
//	FOpenGLVertexStreamARB_VBO::~FOpenGLVertexStreamARB_VBO
//
FOpenGLVertexStreamARB_VBO::~FOpenGLVertexStreamARB_VBO()
{
	guard(FOpenGLVertexStreamARB_VBO::~FOpenGLVertexStreamARB_VBO);
	
	if( Initialized )
		RenDev->glDeleteBuffersARB( 1, &Buffer );
	Initialized = 0;
	
	unguard;
}

//
// FOpenGLVertexStreamARB_VBO::Cache
//
void FOpenGLVertexStreamARB_VBO::Cache(FVertexStream* SourceStream)
{
	guard(FOpenGLVertexStreamARB_VBO::Cache);

	check(!IsDynamicVB);

	INT Size	= Abs(SourceStream->GetSize()),
		Stride	= SourceStream->GetStride();

	if( !Initialized || CachedSize != Size )
	{
		if( !Initialized )
			RenDev->glGenBuffersARB( 1, &Buffer );

		RenDev->glBindBufferARB( GL_ARRAY_BUFFER_ARB, Buffer );
		RenDev->glBufferDataARB( GL_ARRAY_BUFFER_ARB, Size, NULL, SourceStream->HintDynamic() ? GL_DYNAMIC_DRAW_ARB : GL_STATIC_DRAW_ARB );
		Initialized = 1;
	}
	else
		RenDev->glBindBufferARB( GL_ARRAY_BUFFER_ARB, Buffer );

	BYTE* Data;
	
	if( SourceStream->HintDynamic() )
		Data = (BYTE*) RenDev->glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
	else
	{
		if( RenDev->ScratchBuffer.Num() < Size )
		{
			RenDev->ScratchBuffer.Empty();
			RenDev->ScratchBuffer.Add( Size * 2 );
		}
		Data = &RenDev->ScratchBuffer(0);
	}

	check( Data );

	// Copy the stream data.
	SourceStream->GetStreamData( Data );

	// D3D -> OpenGL color conversion for static data.
	// !!warning: relies on data layout not changing
	if( !SourceStream->HintDynamic() )
	{
		UBOOL RevCompsOrig = GIsOpenGLRevComps;
		if( RenDev->SUPPORTS_GL_ATI_array_rev_comps_in_4_bytes )
			GIsOpenGLRevComps = 1;  // RenderColor always byteswaps in here...

		FVertexComponent Components[MAX_VERTEX_COMPONENTS];
		INT NumComponents = SourceStream->GetComponents( Components );
		INT Offset = 0;
		BYTE* DataEnd = Data + Size - 4;
		for( INT i=0; i<NumComponents; i++ )
		{
			switch( Components[i].Type )
			{
			case CT_Float4:
				Offset += 4*4;
				break;
			case CT_Float3:
				Offset += 3*4;
				break;
			case CT_Float2:
				Offset += 2*4;
				break;
			case CT_Float1:
				Offset += 1*4;
				break;
			case CT_Color:
				{	
					FColor* Colors = (FColor*) (Data + Offset);
					while( (BYTE*) Colors <= DataEnd )
					{
						*Colors = (*Colors).RenderColor();
						Colors = (FColor*) ((BYTE*) Colors + Stride);
					}
					Offset += 1*4;
					break;
				}
			}
		}

		GIsOpenGLRevComps = RevCompsOrig;
	}

	if( SourceStream->HintDynamic() )
		RenDev->glUnmapBufferARB( GL_ARRAY_BUFFER_ARB );
	else
		RenDev->glBufferDataARB( GL_ARRAY_BUFFER_ARB, Size, Data, GL_STATIC_DRAW_ARB );

	CachedRevision	= SourceStream->GetRevision();
	CachedSize		= Size;
	
	unguard;
}

//
// FOpenGLVertexStreamARB_VBO::AddVertices
//
INT FOpenGLVertexStreamARB_VBO::AddVertices(FVertexStream* Stream)
{
	guard(FOpenGLVertexStreamARB_VBO::AddVertices);

	check(IsDynamicVB);

	INT Size = Abs(Stream->GetSize());

	RenDev->glBindBufferARB( GL_ARRAY_BUFFER_ARB, Buffer );
	RenDev->glBufferDataARB( GL_ARRAY_BUFFER_ARB, Size, NULL, GL_DYNAMIC_DRAW_ARB );
	
	void* Data = RenDev->glMapBufferARB( GL_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
	check( Data );

	Stream->GetStreamData( Data );
	
	RenDev->glUnmapBufferARB( GL_ARRAY_BUFFER_ARB );

	return 0;

	unguard;
}


//
// FOpenGLVertexStreamARB_VBO::Reallocate
//
void FOpenGLVertexStreamARB_VBO::Reallocate(INT NewSize)
{
	guard(FOpenGLVertexStreamARB_VBO::Reallocate);

	check(IsDynamicVB);

	debugf(TEXT("Allocating dynamic vertex buffer."));

	if( !Initialized )
		RenDev->glGenBuffersARB( 1, &Buffer );

	Initialized = 1;
	
	CachedSize	= NewSize;
	Tail		= 0;

	unguard;
}

//
// ATI_VAO implemention.
//

//
//	FOpenGLVertexStreamATI_VAO::~FOpenGLVertexStreamATI_VAO
//
FOpenGLVertexStreamATI_VAO::~FOpenGLVertexStreamATI_VAO()
{
	guard(FOpenGLVertexStreamATI_VAO::~FOpenGLVertexStreamATI_VAO);
	
	if( Initialized )
		RenDev->glFreeObjectBufferATI( Buffer );
	Initialized = 0;
	
	unguard;
}

//
// FOpenGLVertexStreamATI_VAO::Cache
//
void FOpenGLVertexStreamATI_VAO::Cache(FVertexStream* SourceStream)
{
	guard(FOpenGLVertexStreamATI_VAO::Cache);

	check(!IsDynamicVB);

	INT Size	= Abs(SourceStream->GetSize()),
		Stride	= SourceStream->GetStride();

	if( !Initialized || CachedSize != Size )
	{
		if( Initialized )
			RenDev->glFreeObjectBufferATI( Buffer );
		Buffer = RenDev->glNewObjectBufferATI( Size, NULL, SourceStream->HintDynamic() ? GL_DYNAMIC_ATI : GL_STATIC_ATI );
		Initialized = 1;
	}

	BYTE* Data;
	
	if( RenDev->SUPPORTS_GL_ATI_map_object_buffer && SourceStream->HintDynamic() )
		Data = (BYTE*) RenDev->glMapObjectBufferATI( Buffer );
	else
	{
		if( RenDev->ScratchBuffer.Num() < Size )
		{
			RenDev->ScratchBuffer.Empty();
			RenDev->ScratchBuffer.Add( Size * 2 );
		}
		Data = &RenDev->ScratchBuffer(0);
	}

	check( Data );

	// Copy the stream data.
	SourceStream->GetStreamData( Data );

	// D3D -> OpenGL color conversion for static data.
	// !!warning: relies on data layout not changing
	if( !SourceStream->HintDynamic() )
	{
		UBOOL RevCompsOrig = GIsOpenGLRevComps;
		if( RenDev->SUPPORTS_GL_ATI_array_rev_comps_in_4_bytes )
			GIsOpenGLRevComps = 1;  // RenderColor always byteswaps in here...

		FVertexComponent Components[MAX_VERTEX_COMPONENTS];
		INT NumComponents = SourceStream->GetComponents( Components );
		INT Offset = 0;
		BYTE* DataEnd = Data + Size - 4;
		for( INT i=0; i<NumComponents; i++ )
		{
			switch( Components[i].Type )
			{
			case CT_Float4:
				Offset += 4*4;
				break;
			case CT_Float3:
				Offset += 3*4;
				break;
			case CT_Float2:
				Offset += 2*4;
				break;
			case CT_Float1:
				Offset += 1*4;
				break;
			case CT_Color:
				{	
					FColor* Colors = (FColor*) (Data + Offset);
					while( (BYTE*) Colors <= DataEnd )
					{
						*Colors = (*Colors).RenderColor();
						Colors = (FColor*) ((BYTE*) Colors + Stride);
					}
					Offset += 1*4;
					break;
				}
			}
		}

		GIsOpenGLRevComps = RevCompsOrig;
	}

	if( RenDev->SUPPORTS_GL_ATI_map_object_buffer && SourceStream->HintDynamic() )
		RenDev->glUnmapObjectBufferATI( Buffer );
	else
		RenDev->glUpdateObjectBufferATI( Buffer, 0, Size, Data, GL_DISCARD_ATI );

	CachedRevision	= SourceStream->GetRevision();
	CachedSize		= Size;
	
	unguard;
}

//
// FOpenGLVertexStreamATI_VAO::AddVertices
//
INT FOpenGLVertexStreamATI_VAO::AddVertices(FVertexStream* Stream)
{
	guard(FOpenGLVertexStreamATI_VAO::AddVertices);

	check(IsDynamicVB);

	INT Size	= Abs(Stream->GetSize()),
		Stride	= Stream->GetStride();

	if( Size > CachedSize )
		Reallocate( Size );

	if( (Stream->GetSize() < 0) ||  (Tail + Stride + Size > CachedSize) || (((DWORD) (Tail + Stride + Size) / Stride) > MAX_VERTEX_INDEX) )
		Tail = 0;

	// Determine the offset in the vertex buffer to allocate the vertices at.
	INT	VertexBufferOffset = ((Tail + Stride - 1) / Stride) * Stride;

	if( RenDev->SUPPORTS_GL_ATI_map_object_buffer )
	{
		BYTE* Data = (BYTE*) RenDev->glMapObjectBufferATI( Buffer );
		check( Data )
		Stream->GetStreamData( Data + VertexBufferOffset );
		RenDev->glUnmapObjectBufferATI( Buffer );
	}
	else
	{
		if( RenDev->ScratchBuffer.Num() < Size )
		{
			RenDev->ScratchBuffer.Empty();
			RenDev->ScratchBuffer.Add( Size * 2 );
		}
		BYTE* Data = &RenDev->ScratchBuffer(0);

		Stream->GetStreamData( Data );
		RenDev->glUpdateObjectBufferATI( Buffer, VertexBufferOffset, Size, Data, Tail == 0 ? GL_DISCARD_ATI : GL_PRESERVE_ATI );
	}

	// Update the tail pointer.
	Tail = VertexBufferOffset + Size;

	return VertexBufferOffset / Stride;

	unguard;
}


//
// FOpenGLVertexStreamATI_VAO::Reallocate
//
void FOpenGLVertexStreamATI_VAO::Reallocate(INT NewSize)
{
	guard(FOpenGLVertexStreamATI_VAO::Reallocate);

	check(IsDynamicVB);

	debugf(TEXT("Allocating %u byte dynamic vertex buffer."),NewSize);

	if( Initialized )
		RenDev->glFreeObjectBufferATI( Buffer );
	Buffer = RenDev->glNewObjectBufferATI( NewSize, NULL, GL_DYNAMIC_ATI );
	Initialized = 1;
	
	CachedSize	= NewSize;
	Tail		= 0;

	unguard;
}


//
// APPLE_VAO implemention.
//

//
//	FOpenGLVertexStreamAPPLE_VAO::~FOpenGLVertexStreamAPPLE_VAO
//
FOpenGLVertexStreamAPPLE_VAO::~FOpenGLVertexStreamAPPLE_VAO()
{
	guard(FOpenGLVertexStreamAPPLE_VAO::~FOpenGLVertexStreamAPPLE_VAO);
	
	if( Initialized )
	{
		RenDev->glBindVertexArrayAPPLE( 0 );	
		RenDev->glDeleteVertexArraysAPPLE( 1, &Buffer );
		delete [] VertexData;
	}
	Initialized = 0;
	
	unguard;
}

//
// FOpenGLVertexStreamAPPLE_VAO::Cache
//
void FOpenGLVertexStreamAPPLE_VAO::Cache(FVertexStream* SourceStream)
{
	guard(FOpenGLVertexStreamAPPLE_VAO::Cache);

	check(!IsDynamicVB);

	INT Size	= Abs(SourceStream->GetSize()),
		Stride	= SourceStream->GetStride();

	if( !Initialized || CachedSize != Size )
	{
		if( Initialized )
		{
			RenDev->glBindVertexArrayAPPLE( 0 );	
			RenDev->glDeleteVertexArraysAPPLE( 1, &Buffer );
			delete [] VertexData;
		}
		VertexData = new BYTE[Size];
		RenDev->glGenVertexArraysAPPLE( 1, &Buffer );
		Initialized = 1;
	}

	check( VertexData );

	// Copy the stream data.
	SourceStream->GetStreamData( VertexData );

	// D3D -> OpenGL color conversion for static data.
	// !!warning: relies on data layout not changing
	if( !SourceStream->HintDynamic() )
	{
		UBOOL RevCompsOrig = GIsOpenGLRevComps;
		if( RenDev->SUPPORTS_GL_ATI_array_rev_comps_in_4_bytes )
			GIsOpenGLRevComps = 1;  // RenderColor always byteswaps in here...

		FVertexComponent Components[MAX_VERTEX_COMPONENTS];
		INT NumComponents = SourceStream->GetComponents( Components );
		INT Offset = 0;
		BYTE* DataEnd = VertexData + Size - 4;
		for( INT i=0; i<NumComponents; i++ )
		{
			switch( Components[i].Type )
			{
			case CT_Float4:
				Offset += 4*4;
				break;
			case CT_Float3:
				Offset += 3*4;
				break;
			case CT_Float2:
				Offset += 2*4;
				break;
			case CT_Float1:
				Offset += 1*4;
				break;
			case CT_Color:
				{	
					FColor* Colors = (FColor*) (VertexData + Offset);
					while( (BYTE*) Colors <= DataEnd )
					{
						*Colors = (*Colors).RenderColor();
						Colors = (FColor*) ((BYTE*) Colors + Stride);
					}
					Offset += 1*4;
					break;
				}
			}
		}

		GIsOpenGLRevComps = RevCompsOrig;
	}

	RenDev->glBindVertexArrayAPPLE( Buffer );
	RenDev->glVertexArrayParameteriAPPLE( GL_VERTEX_ARRAY_STORAGE_HINT_APPLE, GL_STORAGE_SHARED_APPLE );
	RenDev->glVertexArrayRangeAPPLE( Size, VertexData );
	RenDev->glEnableClientState( GL_VERTEX_ARRAY );
	RenDev->glFlushVertexArrayRangeAPPLE( Size, VertexData );
	
	RenDev->glBindVertexArrayAPPLE( 0 );

	CachedRevision	= SourceStream->GetRevision();
	CachedSize		= Size;
	
	unguard;
}

//
// FOpenGLVertexStreamAPPLE_VAO::AddVertices
//
INT FOpenGLVertexStreamAPPLE_VAO::AddVertices(FVertexStream* Stream)
{
	guard(FOpenGLVertexStreamATI_VAO::AddVertices);

	check(IsDynamicVB);
	
	appErrorf(TEXT("APPLE_VAO::AddVertices is not implemented"));

	return 0;

	unguard;
}


//
// FOpenGLVertexStreamAPPLE_VAO::Reallocate
//
void FOpenGLVertexStreamAPPLE_VAO::Reallocate(INT NewSize)
{
	guard(FOpenGLVertexStreamATI_VAO::Reallocate);

	check(IsDynamicVB);

	appErrorf(TEXT("APPLE_VAO::Reallocate is not implemented"));

	unguard;
}


//
// FOpenGLIndexBuffer::FOpenGLIndexBuffer
//
FOpenGLIndexBuffer::FOpenGLIndexBuffer(UOpenGLRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicIB)
: FOpenGLResource(InRenDev,InCacheId)
{
	guard(FOpenGLIndexBuffer::FOpenGLIndexBuffer);

	CachedSize		= 0;
	IsDynamicIB		= InIsDynamicIB;

	unguard;
}


//
// FOpenGLIndexBufferARB::Cache
//
void FOpenGLIndexBufferARB::Cache(FIndexBuffer* SourceIndexBuffer)
{
	guard(FOpenGLIndexBufferARB::Cache);

	INT	Size = Max(2, SourceIndexBuffer->GetSize());

	if( CachedSize != Size )
	{
		check( SourceIndexBuffer->GetIndexSize() == sizeof(_WORD) );
		IndexData.Empty( Size );
		IndexData.Add( Size ); 
	}

	//!!vogel: TODO: why here but not in vertexbuffer
	if( CachedRevision != SourceIndexBuffer->GetRevision() )
		SourceIndexBuffer->GetContents(&IndexData(0));

	CachedRevision	= SourceIndexBuffer->GetRevision();
	CachedSize		= Size;

	unguard;
}


//
// FOpenGLIndexBufferARB::Reallocate
//
void FOpenGLIndexBufferARB::Reallocate(INT NewSize)
{
	guard(FOpenGLIndexBufferARB::Reallocate);

	debugf(TEXT("Allocating %u byte dynamic index buffer."),NewSize);

	IndexData.Empty( NewSize );
	IndexData.Add( NewSize );

	CachedSize	= NewSize;
	Tail		= 0;
	
	unguard;
}

//
// FOpenGLIndexBufferARB::AddIndices
//
INT FOpenGLIndexBufferARB::AddIndices(FIndexBuffer* IndexBuffer)
{
	guard(FOpenGLIndexBufferARB::AddIndices);

    INT	Size	= IndexBuffer->GetSize(),
		Stride	= IndexBuffer->GetIndexSize();

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
//	FOpenGLIndexBufferATI::~FOpenGLIndexBufferATI
//
FOpenGLIndexBufferATI::~FOpenGLIndexBufferATI()
{
	guard(FOpenGLIndexBufferATI::~FOpenGLIndexBufferATI);
	
	if( Initialized )
		RenDev->glFreeObjectBufferATI( Buffer );
	Initialized = 0;
	
	unguard;
}


//
// FOpenGLIndexBufferATI::Cache
//
void FOpenGLIndexBufferATI::Cache(FIndexBuffer* SourceIndexBuffer)
{
	guard(FOpenGLIndexBufferATI::Cache);

	check(!IsDynamicIB);

	INT	Size = Max(2, SourceIndexBuffer->GetSize());

	if( CachedSize != Size )
	{
		check( SourceIndexBuffer->GetIndexSize() == sizeof(_WORD) );

		if( Initialized )
			RenDev->glFreeObjectBufferATI( Buffer );
		Buffer = RenDev->glNewObjectBufferATI( Size, NULL, GL_STATIC_ATI );
		Initialized = 1;
	}

	if( RenDev->SUPPORTS_GL_ATI_map_object_buffer )
	{
		BYTE* Data = (BYTE*) RenDev->glMapObjectBufferATI( Buffer );
		check(Data);
		SourceIndexBuffer->GetContents(Data);
		RenDev->glUnmapObjectBufferATI( Buffer );
	}
	else
	{
		if( RenDev->ScratchBuffer.Num() < Size )
		{
			RenDev->ScratchBuffer.Empty();
			RenDev->ScratchBuffer.Add( Size * 2 );
		}
		BYTE* Data = &RenDev->ScratchBuffer(0);

		SourceIndexBuffer->GetContents(Data);
		RenDev->glUpdateObjectBufferATI( Buffer, 0, Size, Data, GL_DISCARD_ATI );
	}

	check( CachedRevision != SourceIndexBuffer->GetRevision() );
	CachedRevision	= SourceIndexBuffer->GetRevision();
	CachedSize		= Size;

	unguard;
}


//
// FOpenGLIndexBufferATI::Reallocate
//
void FOpenGLIndexBufferATI::Reallocate(INT NewSize)
{
	guard(FOpenGLIndexBufferATI::Reallocate);

	check(IsDynamicIB);

	debugf(TEXT("Allocating %u byte dynamic index buffer."),NewSize);

	if( Initialized )
		RenDev->glFreeObjectBufferATI( Buffer );
	Buffer = RenDev->glNewObjectBufferATI( NewSize, NULL, GL_DYNAMIC_ATI );
	Initialized = 1;

	CachedSize	= NewSize;
	Tail		= 0;
	
	unguard;
}


//
// FOpenGLIndexBufferATI::AddIndices
//
INT FOpenGLIndexBufferATI::AddIndices(FIndexBuffer* IndexBuffer)
{
	guard(FOpenGLIndexBufferATI::AddIndices);

    INT	Size	= IndexBuffer->GetSize(),
		Stride	= IndexBuffer->GetIndexSize();

	// If the dynamic index buffer isn't big enough to contain all the indices, resize it.
	if( Size > CachedSize )
		Reallocate( Size );

	// If the dynamic index buffer will overflow with the additional indices, flush it.
	if( Tail + Size > CachedSize )
		Tail = 0;

	INT IndexBufferOffset = Tail;

	if( RenDev->SUPPORTS_GL_ATI_map_object_buffer )
	{
		BYTE* Data = (BYTE*) RenDev->glMapObjectBufferATI( Buffer );
		check( Data )
		IndexBuffer->GetContents( Data + IndexBufferOffset );
		RenDev->glUnmapObjectBufferATI( Buffer );
	}
	else
	{
		if( RenDev->ScratchBuffer.Num() < Size )
		{
			RenDev->ScratchBuffer.Empty();
			RenDev->ScratchBuffer.Add( Size * 2 );
		}
		BYTE* Data = &RenDev->ScratchBuffer(0);

		IndexBuffer->GetContents( Data );
		RenDev->glUpdateObjectBufferATI( Buffer, IndexBufferOffset, Size, Data, Tail == 0 ? GL_DISCARD_ATI : GL_PRESERVE_ATI );
	}

	// Update the tail pointer.
	Tail = IndexBufferOffset + Size;

	return IndexBufferOffset / Stride;

	//!!vogel: TODO: investigate revision mojo

	unguard;
}


//
//	FOpenGLIndexBufferARB_VBO::~FOpenGLIndexBufferARB_VBO
//
FOpenGLIndexBufferARB_VBO::~FOpenGLIndexBufferARB_VBO()
{
	guard(FOpenGLIndexBufferARB_VBO::~FOpenGLIndexBufferARB_VBO);
	
	if( Initialized )
		RenDev->glDeleteBuffersARB( 1, &Buffer );
	Initialized = 0;
	
	unguard;
}


//
// FOpenGLIndexBufferARB_VBO::Cache
//
void FOpenGLIndexBufferARB_VBO::Cache(FIndexBuffer* SourceIndexBuffer)
{
	guard(FOpenGLIndexBufferARB_VBO::Cache);

	check(!IsDynamicIB);

	INT	Size = Max(2, SourceIndexBuffer->GetSize());

	if( !Initialized || CachedSize != Size )
	{
		if( !Initialized )
			RenDev->glGenBuffersARB( 1, &Buffer );

		RenDev->glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, Buffer );
		RenDev->glBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, Size, NULL, GL_STATIC_DRAW_ARB );
		Initialized = 1;
	}
	else
		RenDev->glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, Buffer );

	void* Data = RenDev->glMapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
	check( Data );

	// Copy the stream data.
	SourceIndexBuffer->GetContents(Data);

	RenDev->glUnmapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB );

	CachedRevision	= SourceIndexBuffer->GetRevision();
	CachedSize		= Size;

	unguard;
}


//
// FOpenGLIndexBufferARB_VBO::Reallocate
//
void FOpenGLIndexBufferARB_VBO::Reallocate(INT NewSize)
{
	guard(FOpenGLIndexBufferARB_VBO::Reallocate);

	check(IsDynamicIB);

	debugf(TEXT("Allocating dynamic index buffer."));

	if( !Initialized )
		RenDev->glGenBuffersARB( 1, &Buffer );

	Initialized = 1;

	CachedSize	= NewSize;
	Tail		= 0;
	
	unguard;
}


//
// FOpenGLIndexBufferARB_VBO::AddIndices
//
INT FOpenGLIndexBufferARB_VBO::AddIndices(FIndexBuffer* IndexBuffer)
{
	guard(FOpenGLIndexBufferATI::AddIndices);

    INT	Size = IndexBuffer->GetSize();

	RenDev->glBindBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, Buffer );
	RenDev->glBufferDataARB( GL_ELEMENT_ARRAY_BUFFER_ARB, Size, NULL, GL_DYNAMIC_DRAW_ARB );
	
	void* Data = RenDev->glMapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB, GL_WRITE_ONLY_ARB );
	check( Data );

	IndexBuffer->GetContents( Data );
	
	RenDev->glUnmapBufferARB( GL_ELEMENT_ARRAY_BUFFER_ARB );

	return 0;

	unguard;
}

//
// FOpenGLVertexShader::FOpenGLVertexShader
//
FOpenGLVertexShader::FOpenGLVertexShader(UOpenGLRenderDevice* InRenDev,FShaderDeclaration& InDeclaration)
{
	guard(FOpenGLVertexShader::FOpenGLVertexShader);
	
	RenDev					= InRenDev;
	Declaration				= InDeclaration;
	
	// Add the vertex shader to the device vertex shader list.
	NextVertexShader		= RenDev->VertexShaders;
	RenDev->VertexShaders	= this;

	unguard;
}

//
// FOpenGLVertexShader::~FOpenGLVertexShader
//
FOpenGLVertexShader::~FOpenGLVertexShader()
{
	guard(FOpenGLVertexShader::~FOpenGLVertexShader);
	
	// Remove the vertex shader from the device vertex shader list.
	FOpenGLVertexShader*	ShaderPtr;

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
// FOpenGLFixedVertexShader::FOpenGLFixedVertexShader
//
FOpenGLFixedVertexShader::FOpenGLFixedVertexShader(UOpenGLRenderDevice* InRenDev,FShaderDeclaration& InDeclaration)
: FOpenGLVertexShader( InRenDev, InDeclaration )
{
	guard(FOpenGLFixedVertexShader::FOpenGLFixedVertexShader);
	Type = VS_FixedFunction;
	unguard;
}


//
// FOpenGLPixelShader::FOpenGLPixelShader
//
FOpenGLPixelShader::FOpenGLPixelShader(UOpenGLRenderDevice* InRenDev, EPixelShader InType, ANSICHAR* InSource)
{
	guard(FOpenGLPixelShader::FOpenGLPixelShader);

	RenDev	= InRenDev;
	Source	= InSource;

    GLuint prog = 0;

    if (Source)
    {
        if (RenDev->SUPPORTS_GL_ARB_fragment_program)
        {
            debugf(TEXT("OpenGL: Assembling fragment program..."));
            RenDev->glGenProgramsARB(1, &prog);
            RenDev->glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, prog);
            RenDev->glProgramStringARB(GL_FRAGMENT_PROGRAM_ARB,
                                         GL_PROGRAM_FORMAT_ASCII_ARB,
                                         (GLsizei) strlen(Source), Source);

            GLint errpos = 0;
            RenDev->glGetIntegerv(GL_PROGRAM_ERROR_POSITION_ARB, &errpos);
            if (errpos != -1)
            {
                const char *errstr = (const char *) RenDev->glGetString(GL_PROGRAM_ERROR_STRING_ARB);
                debugf(TEXT("OpenGL: Failed to assemble fragment program!"));

                debugf(TEXT("OpenGL: program is %s"),
                            (strlen(Source) < 1024) ?
                                appFromAnsi(Source) : TEXT("[too big to print]"));

                debugf(TEXT("OpenGL: program error position: %d"), (int) errpos);
                debugf(TEXT("OpenGL: assembler error is: %s"),
                            (strlen(errstr) < 1024) ?
                                appFromAnsi(errstr) : TEXT("[too big to print]"));

                RenDev->glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
                RenDev->glDeleteProgramsARB(1, &prog);
                prog = 0;
            }
        }
    }

    Handle = prog;
	Type = ((prog) ? InType : PS_None);

	// Add the Pixel shader to the device Pixel shader list.
	NextPixelShader = RenDev->PixelShaders;
	RenDev->PixelShaders = this;

	unguard;
}

//
//	FOpenGLPixelShader::~FOpenGLPixelShader
//
FOpenGLPixelShader::~FOpenGLPixelShader()
{
	guard(FOpenGLPixelShader::~FOpenGLPixelShader);

	// Remove the Pixel shader from the device Pixel shader list.
	FOpenGLPixelShader*	ShaderPtr;

	if(RenDev->PixelShaders != this)
	{
		ShaderPtr = RenDev->PixelShaders;

		while(ShaderPtr && ShaderPtr->NextPixelShader != this)
			ShaderPtr = ShaderPtr->NextPixelShader;

		if(ShaderPtr)
			ShaderPtr->NextPixelShader = NextPixelShader;

		NextPixelShader = NULL;
	}
	else
	{
		RenDev->PixelShaders = NextPixelShader;
		NextPixelShader = NULL;
	}

	// Delete the Pixel shader.
	if( Handle )
    {
        RenDev->glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
        RenDev->glDeleteProgramsARB(1, &Handle);
    }

	Handle = 0;

	unguard;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

