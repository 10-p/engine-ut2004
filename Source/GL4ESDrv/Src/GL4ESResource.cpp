/*=============================================================================
	OpenGLRenderResource.cpp: Unreal OpenGL support.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.
	
	Revision history:
	* Created by Daniel Vogel

=============================================================================*/

#include "GL4ESDrv.h"

//
// FGL4ESResource::FGL4ESResource
//
FGL4ESResource::FGL4ESResource(UGL4ESRenderDevice* InRenDev,QWORD InCacheId)
{
	guard(FGL4ESResource::FGL4ESResource);

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
// FGL4ESResource::~FGL4ESResource
//
FGL4ESResource::~FGL4ESResource()
{
	guard(FGL4ESResource::~FGL4ESResource);

	FGL4ESResource*	ResourcePtr;

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
// FGL4ESTexture::FGL4ESTexture
//
FGL4ESTexture::FGL4ESTexture(UGL4ESRenderDevice* InRenDev,QWORD InCacheId)
: FGL4ESResource(InRenDev,InCacheId)
{
	TextureID		= 0;
	IsCubemap		= 0;
	RenderTargetSurface = 0;
	DepthStencilSurface = 0;
	WasCubemap		= 0;
}

//
// FGL4ESTexture::~FGL4ESTexture
//
FGL4ESTexture::~FGL4ESTexture()
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
// FGL4ESTexture::CalculateFootprint
//
INT FGL4ESTexture::CalculateFootprint()
{
	INT	Bytes = 0;

	for(INT MipIndex=0; MipIndex<CachedNumMips; MipIndex++)
		Bytes += GetBytesPerPixel( CachedFormat, (CachedWidth >> MipIndex) * (CachedHeight >> MipIndex) );

	if( IsCubemap )
		Bytes *= 6;

	return Bytes;
}


//
// FGL4ESTexture::Cache
//
void FGL4ESTexture::Cache(FBaseTexture* SourceTexture)
{
	guard(FGL4ESTexture::Cache);

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
				debugf(TEXT("GL4ES: Failed to create render target!"));
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

			// Env-map cubes are DXT and GL4ES keeps only the base level for faces 1-5 of a
			// compressed cube, so a mipmap MIN_FILTER makes the cubemap incomplete (black).
			// Sample the base level only -- every face has it. (Needs the GL4ES cube-bind fix.)
			RenDev->glTexParameteri( GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
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

			// Cube faces are always decompressed to RGBA below (GL4ES drops faces 1-5's
			// COMPRESSED cube mips -> incomplete cube -> black). The non-DXT path uses Format
			// as the EXTERNAL upload format, so force GL_BGRA -- a compressed enum uploads NULL.
			Format = GL_BGRA;

			INT MipWidth	= Width >> MipLevel,
				MipHeight	= Height >> MipLevel;

			for( INT FaceIndex=0; FaceIndex<6; FaceIndex ++ )
			{
				FTexture* Texture = Cubemap->GetFace( FaceIndex );

				if (!Texture)
					continue;

				// Force the decompress path for cube faces (never glCompressedTexImage2DARB):
				// GL4ES mishandles compressed cube-face mip chains (drops faces 1-5's mips),
				// which leaves the cubemap incomplete -> textureCube returns black.
				if( 0 && IsDXTC(ActualFormat) && RenDev->SUPPORTS_GL_EXT_texture_compression_s3tc )
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

		// GL4ES treats cube maps as mip-needing and forces a mipmap MIN_FILTER on them, but
		// env-map cubes here are uploaded base-level-only (and the driver-side GL_LINEAR set at
		// creation is swallowed by GL4ES's sampler recompute). A mipmap filter on a cube with no
		// mip chain is mipmap-INCOMPLETE -> textureCube() returns black -> a base*cube modulate
		// paints the surface black (seen with a TexRotator-wrapped specular cubemap). Generate the
		// chain from the decompressed base faces so the cube is complete for the filter GL4ES uses.
		// Guard on the entry point itself, not SUPPORTS_GL_EXT_framebuffer_object: that flag is
		// force-cleared when render-to-texture (FBOs) is disabled, but glGenerateMipmap is core
		// GLES2 and independent of FBOs, and the entry point is still loaded.
		if( RenDev->glGenerateMipmapEXT )
			RenDev->glGenerateMipmapEXT( GL_TEXTURE_CUBE_MAP );

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
// FGL4ESVertexStream::FGL4ESVertexStream
//
FGL4ESVertexStream::FGL4ESVertexStream(UGL4ESRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB)
: FGL4ESResource(InRenDev,InCacheId)
{
	CachedSize		= 0;
	IsDynamicVB		= InIsDynamicVB;
}

#define MAX_VERTEX_INDEX 65535

//
// Standard vertex arrays implementation.
//

//
// FGL4ESVertexStreamARB_VA::Cache
//
void FGL4ESVertexStreamARB_VA::Cache(FVertexStream* SourceStream)
{
	guard(FGL4ESVertexStreamARB_VA::Cache);

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
// FGL4ESVertexStreamARB_VA::Reallocate
//
void FGL4ESVertexStreamARB_VA::Reallocate(INT NewSize)
{
	guard(FGL4ESVertexStream_VA::Reallocate);

	check(IsDynamicVB);

	debugf(TEXT("Allocating %u byte dynamic vertex buffer."),NewSize);

	VertexData.Empty( NewSize );
	VertexData.Add( NewSize );
	
	CachedSize	= NewSize;
	Tail		= 0;

	unguard;
}


//
// FGL4ESVertexStreamARB_VA::AddVertices
//
INT FGL4ESVertexStreamARB_VA::AddVertices(FVertexStream* Stream)
{
	guard(FGL4ESVertexStream_VA::AddVertices);

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
// FGL4ESVertexStreamNV_VAR::Cache
//
void FGL4ESVertexStreamNV_VAR::Cache(FVertexStream* SourceStream)
{
	guard(FGL4ESVertexStreamNV_VAR::Cache);

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
// FGL4ESVertexStreamNV_VAR::AddVertices
//
INT FGL4ESVertexStreamNV_VAR::AddVertices(FVertexStream* Stream)
{
	guard(FGL4ESVertexStreamNV_VAR::AddVertices);

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
// FGL4ESVertexStreamNV_VAR::Reallocate
//
void FGL4ESVertexStreamNV_VAR::Reallocate(INT NewSize)
{
	guard(FGL4ESVertexStreamNV_VAR::Reallocate);

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
//	FGL4ESVertexStreamARB_VBO::~FGL4ESVertexStreamARB_VBO
//
FGL4ESVertexStreamARB_VBO::~FGL4ESVertexStreamARB_VBO()
{
	guard(FGL4ESVertexStreamARB_VBO::~FGL4ESVertexStreamARB_VBO);
	
	if( Initialized )
		RenDev->glDeleteBuffersARB( 1, &Buffer );
	Initialized = 0;
	
	unguard;
}

//
// FGL4ESVertexStreamARB_VBO::Cache
//
void FGL4ESVertexStreamARB_VBO::Cache(FVertexStream* SourceStream)
{
	guard(FGL4ESVertexStreamARB_VBO::Cache);

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
// FGL4ESVertexStreamARB_VBO::AddVertices
//
INT FGL4ESVertexStreamARB_VBO::AddVertices(FVertexStream* Stream)
{
	guard(FGL4ESVertexStreamARB_VBO::AddVertices);

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
// FGL4ESVertexStreamARB_VBO::Reallocate
//
void FGL4ESVertexStreamARB_VBO::Reallocate(INT NewSize)
{
	guard(FGL4ESVertexStreamARB_VBO::Reallocate);

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
//	FGL4ESVertexStreamATI_VAO::~FGL4ESVertexStreamATI_VAO
//
FGL4ESVertexStreamATI_VAO::~FGL4ESVertexStreamATI_VAO()
{
	guard(FGL4ESVertexStreamATI_VAO::~FGL4ESVertexStreamATI_VAO);
	
	if( Initialized )
		RenDev->glFreeObjectBufferATI( Buffer );
	Initialized = 0;
	
	unguard;
}

//
// FGL4ESVertexStreamATI_VAO::Cache
//
void FGL4ESVertexStreamATI_VAO::Cache(FVertexStream* SourceStream)
{
	guard(FGL4ESVertexStreamATI_VAO::Cache);

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
// FGL4ESVertexStreamATI_VAO::AddVertices
//
INT FGL4ESVertexStreamATI_VAO::AddVertices(FVertexStream* Stream)
{
	guard(FGL4ESVertexStreamATI_VAO::AddVertices);

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
// FGL4ESVertexStreamATI_VAO::Reallocate
//
void FGL4ESVertexStreamATI_VAO::Reallocate(INT NewSize)
{
	guard(FGL4ESVertexStreamATI_VAO::Reallocate);

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
//	FGL4ESVertexStreamAPPLE_VAO::~FGL4ESVertexStreamAPPLE_VAO
//
FGL4ESVertexStreamAPPLE_VAO::~FGL4ESVertexStreamAPPLE_VAO()
{
	guard(FGL4ESVertexStreamAPPLE_VAO::~FGL4ESVertexStreamAPPLE_VAO);
	
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
// FGL4ESVertexStreamAPPLE_VAO::Cache
//
void FGL4ESVertexStreamAPPLE_VAO::Cache(FVertexStream* SourceStream)
{
	guard(FGL4ESVertexStreamAPPLE_VAO::Cache);

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
// FGL4ESVertexStreamAPPLE_VAO::AddVertices
//
INT FGL4ESVertexStreamAPPLE_VAO::AddVertices(FVertexStream* Stream)
{
	guard(FGL4ESVertexStreamATI_VAO::AddVertices);

	check(IsDynamicVB);
	
	appErrorf(TEXT("APPLE_VAO::AddVertices is not implemented"));

	return 0;

	unguard;
}


//
// FGL4ESVertexStreamAPPLE_VAO::Reallocate
//
void FGL4ESVertexStreamAPPLE_VAO::Reallocate(INT NewSize)
{
	guard(FGL4ESVertexStreamATI_VAO::Reallocate);

	check(IsDynamicVB);

	appErrorf(TEXT("APPLE_VAO::Reallocate is not implemented"));

	unguard;
}


//
// FGL4ESIndexBuffer::FGL4ESIndexBuffer
//
FGL4ESIndexBuffer::FGL4ESIndexBuffer(UGL4ESRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicIB)
: FGL4ESResource(InRenDev,InCacheId)
{
	guard(FGL4ESIndexBuffer::FGL4ESIndexBuffer);

	CachedSize		= 0;
	IsDynamicIB		= InIsDynamicIB;

	unguard;
}


//
// FGL4ESIndexBufferARB::Cache
//
void FGL4ESIndexBufferARB::Cache(FIndexBuffer* SourceIndexBuffer)
{
	guard(FGL4ESIndexBufferARB::Cache);

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
// FGL4ESIndexBufferARB::Reallocate
//
void FGL4ESIndexBufferARB::Reallocate(INT NewSize)
{
	guard(FGL4ESIndexBufferARB::Reallocate);

	debugf(TEXT("Allocating %u byte dynamic index buffer."),NewSize);

	IndexData.Empty( NewSize );
	IndexData.Add( NewSize );

	CachedSize	= NewSize;
	Tail		= 0;
	
	unguard;
}

//
// FGL4ESIndexBufferARB::AddIndices
//
INT FGL4ESIndexBufferARB::AddIndices(FIndexBuffer* IndexBuffer)
{
	guard(FGL4ESIndexBufferARB::AddIndices);

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
//	FGL4ESIndexBufferATI::~FGL4ESIndexBufferATI
//
FGL4ESIndexBufferATI::~FGL4ESIndexBufferATI()
{
	guard(FGL4ESIndexBufferATI::~FGL4ESIndexBufferATI);
	
	if( Initialized )
		RenDev->glFreeObjectBufferATI( Buffer );
	Initialized = 0;
	
	unguard;
}


//
// FGL4ESIndexBufferATI::Cache
//
void FGL4ESIndexBufferATI::Cache(FIndexBuffer* SourceIndexBuffer)
{
	guard(FGL4ESIndexBufferATI::Cache);

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
// FGL4ESIndexBufferATI::Reallocate
//
void FGL4ESIndexBufferATI::Reallocate(INT NewSize)
{
	guard(FGL4ESIndexBufferATI::Reallocate);

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
// FGL4ESIndexBufferATI::AddIndices
//
INT FGL4ESIndexBufferATI::AddIndices(FIndexBuffer* IndexBuffer)
{
	guard(FGL4ESIndexBufferATI::AddIndices);

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
//	FGL4ESIndexBufferARB_VBO::~FGL4ESIndexBufferARB_VBO
//
FGL4ESIndexBufferARB_VBO::~FGL4ESIndexBufferARB_VBO()
{
	guard(FGL4ESIndexBufferARB_VBO::~FGL4ESIndexBufferARB_VBO);
	
	if( Initialized )
		RenDev->glDeleteBuffersARB( 1, &Buffer );
	Initialized = 0;
	
	unguard;
}


//
// FGL4ESIndexBufferARB_VBO::Cache
//
void FGL4ESIndexBufferARB_VBO::Cache(FIndexBuffer* SourceIndexBuffer)
{
	guard(FGL4ESIndexBufferARB_VBO::Cache);

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
// FGL4ESIndexBufferARB_VBO::Reallocate
//
void FGL4ESIndexBufferARB_VBO::Reallocate(INT NewSize)
{
	guard(FGL4ESIndexBufferARB_VBO::Reallocate);

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
// FGL4ESIndexBufferARB_VBO::AddIndices
//
INT FGL4ESIndexBufferARB_VBO::AddIndices(FIndexBuffer* IndexBuffer)
{
	guard(FGL4ESIndexBufferATI::AddIndices);

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
// FGL4ESVertexShader::FGL4ESVertexShader
//
FGL4ESVertexShader::FGL4ESVertexShader(UGL4ESRenderDevice* InRenDev,FGL4ESShaderDeclaration& InDeclaration)
{
	guard(FGL4ESVertexShader::FGL4ESVertexShader);
	
	RenDev					= InRenDev;
	Declaration				= InDeclaration;
	
	// Add the vertex shader to the device vertex shader list.
	NextVertexShader		= RenDev->VertexShaders;
	RenDev->VertexShaders	= this;

	unguard;
}

//
// FGL4ESVertexShader::~FGL4ESVertexShader
//
FGL4ESVertexShader::~FGL4ESVertexShader()
{
	guard(FGL4ESVertexShader::~FGL4ESVertexShader);
	
	// Remove the vertex shader from the device vertex shader list.
	FGL4ESVertexShader*	ShaderPtr;

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
// FGL4ESFixedVertexShader::FGL4ESFixedVertexShader
//
FGL4ESFixedVertexShader::FGL4ESFixedVertexShader(UGL4ESRenderDevice* InRenDev,FGL4ESShaderDeclaration& InDeclaration)
: FGL4ESVertexShader( InRenDev, InDeclaration )
{
	guard(FGL4ESFixedVertexShader::FGL4ESFixedVertexShader);
	Type = VS_FixedFunction;
	unguard;
}


//
// FGL4ESPixelShader::FGL4ESPixelShader
//
FGL4ESPixelShader::FGL4ESPixelShader(UGL4ESRenderDevice* InRenDev, EGL4ESPixelShader InType, ANSICHAR* InSource)
{
	guard(FGL4ESPixelShader::FGL4ESPixelShader);

	RenDev	= InRenDev;
	Source	= InSource;

    GLuint prog = 0;

    if (Source)
    {
        if (RenDev->SUPPORTS_GL_ARB_fragment_program)
        {
            debugf(TEXT("GL4ES: Assembling fragment program..."));
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
                debugf(TEXT("GL4ES: Failed to assemble fragment program!"));

                debugf(TEXT("GL4ES: program is %s"),
                            (strlen(Source) < 1024) ?
                                appFromAnsi(Source) : TEXT("[too big to print]"));

                debugf(TEXT("GL4ES: program error position: %d"), (int) errpos);
                debugf(TEXT("GL4ES: assembler error is: %s"),
                            (strlen(errstr) < 1024) ?
                                appFromAnsi(errstr) : TEXT("[too big to print]"));

                RenDev->glBindProgramARB(GL_FRAGMENT_PROGRAM_ARB, 0);
                RenDev->glDeleteProgramsARB(1, &prog);
                prog = 0;
            }
        }
    }

    Handle = prog;
	Type = ((prog) ? InType : GL4ES_PS_None);

	// Add the Pixel shader to the device Pixel shader list.
	NextPixelShader = RenDev->PixelShaders;
	RenDev->PixelShaders = this;

	unguard;
}

//
//	FGL4ESPixelShader::~FGL4ESPixelShader
//
FGL4ESPixelShader::~FGL4ESPixelShader()
{
	guard(FGL4ESPixelShader::~FGL4ESPixelShader);

	// Remove the Pixel shader from the device Pixel shader list.
	FGL4ESPixelShader*	ShaderPtr;

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

