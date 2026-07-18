/*=============================================================================
	D3DResource.cpp: Unreal Direct3D resource implementation.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "D3D9Drv.h"

#define RESOURCE_POOL D3DPOOL_MANAGED

//
//	FD3D9Resource::FD3D9Resource
//
FD3D9Resource::FD3D9Resource(UD3D9RenderDevice* InRenDev,QWORD InCacheId)
{
	guard(FD3D9Resource::FD3D9Resource);

	RenDev = InRenDev;
	CacheId = InCacheId;
	CachedRevision = 0;

	// Add this resource to the device resource list.
	NextResource = RenDev->ResourceList;
	RenDev->ResourceList = this;

	// Calculate a hash index.
	HashIndex = GetResourceHashIndex(CacheId);

	// Add this resource to the device resource hash.
	HashNext = RenDev->ResourceHash[HashIndex];
	RenDev->ResourceHash[HashIndex] = this;

	LastFrameUsed = 0;

	unguard;
}

//
//	FD3D9Resource::~FD3D9Resource
//
FD3D9Resource::~FD3D9Resource()
{
	guard(FD3D9Resource::~FD3D9Resource);

	FD3D9Resource*	ResourcePtr;

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
//	FD3D9Texture::FD3D9Texture
//
FD3D9Texture::FD3D9Texture(UD3D9RenderDevice* InRenDev,QWORD InCacheId) : FD3D9Resource(InRenDev,InCacheId)
{
	RenderTargetSurface		= NULL;
	DepthStencilSurface		= NULL;
	Direct3DTexture9		= NULL;
	Direct3DCubeTexture9	= NULL;
}

//
//	FD3D9Texture::~FD3D9Texture
//
FD3D9Texture::~FD3D9Texture()
{
	if( RenderTargetSurface )
		RenderTargetSurface->Release();
	if( DepthStencilSurface )
		DepthStencilSurface->Release();
	if( Direct3DTexture9 )
		Direct3DTexture9->Release();
	if( Direct3DCubeTexture9 )
		Direct3DCubeTexture9->Release();
}

//
//  FD3D9Texture::CalculateFootprint
//
INT FD3D9Texture::CalculateFootprint()
{
	INT	Bytes = 0;

	for(INT MipIndex = CachedFirstMip;MipIndex < CachedNumMips;MipIndex++)
		Bytes += (GetFormatBPP(CachedFormat) * (CachedWidth >> MipIndex) / 8) * (CachedHeight >> MipIndex);

	return Bytes;
}

//
//	FD3D9Texture::Cache
//
UBOOL FD3D9Texture::Cache(FBaseTexture* SourceTexture)
{
	guard(FD3D9Texture::Cache);

	UBOOL	Failed = 0;
	INT		FirstMip = SourceTexture->GetFirstMip(),
			Width	 = SourceTexture->GetWidth(),
			Height	 = SourceTexture->GetHeight(),
			NumMips  = SourceTexture->GetNumMips();
	HRESULT	Result;

	// Determine the source texture format.
	ETextureFormat	SourceFormat =		SourceTexture->GetFormat();

	// Determine the actual texture format.
	ETextureFormat	ActualFormat =		SourceFormat == TEXF_P8		? TEXF_RGBA8 :
										SourceFormat == TEXF_G16	? TEXF_RGBA8 :
										SourceFormat == TEXF_RGBA7	? TEXF_RGBA8 :
										SourceFormat;

	FCompositeTexture*	CompositeTexture = SourceTexture->GetCompositeTextureInterface();
	FCubemap*			Cubemap = SourceTexture->GetCubemapInterface();
	FTexture*			Texture = SourceTexture->GetTextureInterface();
	FRenderTarget*		RenderTarget = SourceTexture->GetRenderTargetInterface();

	// Workaround for HW bug on older NVIDIA cards.
	if( RenDev->HasNVCubemapBug
	&&	Texture 
	&&	Texture->GetUTexture() 
	&&  (appStrcmp(Texture->GetUTexture()->GetPathName(), TEXT("NvidiaLogo_T.PlaySeal_T")) == 0) 
	)
		NumMips = Min( 4, NumMips );

	// Verify NumMips.
	INT MinDimension = Min( Width, Height );
	INT LogDimension = 0;
	while( MinDimension > 0 )
	{
		MinDimension >>= 1;
		LogDimension++;
	}
	
	// Spit our warning if too many miplevels.
	//if( NumMips > LogDimension && Texture )
	//	debugf( TEXT("Texture [%s] is %ix%i and has %i Mips instead of %i"), Texture->GetUTexture()->GetPathName(), Width, Height, NumMips, LogDimension );

	// Clamp number of miplevels to avoid crashes.
	NumMips = Min( NumMips, LogDimension );

	// Report dynamic uploads if wanted.
	UBOOL Precaching = RenDev->LockedViewport ? RenDev->LockedViewport->Precaching : 0;
	if( !GIsEditor && !Precaching && UTexture::__Client && UTexture::__Client->ReportDynamicUploads )
	{
		INT Size = 0;
		for( INT i=FirstMip; i<NumMips; i++ )
			Size += GetBytesPerPixel(ActualFormat,(Width >> i) * (Height >> i));
		if( CompositeTexture )
		{
			debugf(TEXT("Uploading Lightmap!!!"));
		}
		else if( Cubemap )
		{
			debugf(TEXT("Uploading Cubemap: Texture=[%s], Size=[%i]"),Cubemap->GetFace(0)->GetUTexture()->GetPathName(),Size*6);
		}
		else if( Texture )
		{
			debugf(TEXT("Uploading Texture: Texture=[%s], Size=[%i]"),Texture->GetUTexture()->GetPathName(),Size);
		}
	}

	// Use first face if cubemaps aren't supported.
	if( Cubemap && !RenDev->SupportsCubemaps ) 
	{
		Texture = Cubemap->GetFace(0);
		Cubemap = NULL;
	}

	// Determine the corresponding Direct3D format.
	D3DFORMAT		Direct3DFormat =	ActualFormat == TEXF_RGBA8	? (RenDev->SupportsTextureFormat(TEXF_RGBA8) ? D3DFMT_A8R8G8B8 : D3DFMT_A4R4G4B4 ) : 
										ActualFormat == TEXF_DXT1	? D3DFMT_DXT1 :
										ActualFormat == TEXF_DXT3	? D3DFMT_DXT3 :
										ActualFormat == TEXF_DXT5	? D3DFMT_DXT5 :
										D3DFMT_UNKNOWN;

	// Calculate the first mipmap to use.
	UBOOL DownSample = 0;
	while((Width >> FirstMip) > (INT) RenDev->DeviceCaps9.MaxTextureWidth || (Height >> FirstMip) > (INT) RenDev->DeviceCaps9.MaxTextureHeight)
		if(++FirstMip >= NumMips)
			DownSample = 1;

	if( DownSample )
		FirstMip = 0;

	INT	USize = Width  >> FirstMip,
		VSize = Height >> FirstMip;

	INT ScaleX = 1,
		ScaleY = 1;
	if( DownSample )
	{
		USize	= Min<INT>(USize, RenDev->DeviceCaps9.MaxTextureWidth);
		VSize	= Min<INT>(VSize, RenDev->DeviceCaps9.MaxTextureHeight);
		ScaleX	= Width  / USize;
		ScaleY	= Height / VSize;
		//if( SourceTexture->GetTextureInterface() )
		//	debugf(TEXT("Downsampling: %s"), SourceTexture->GetTextureInterface()->GetUTexture()->GetFullName());
	}

	if( !VSize || !USize )
	{
		if(SourceTexture->GetTextureInterface() && SourceTexture->GetTextureInterface()->GetUTexture())
			debugf(TEXT("ERROR! ZERO USIZE OR VSIZE ON TEXTURE: %s"), SourceTexture->GetTextureInterface()->GetUTexture()->GetFullName());

		Direct3DCubeTexture9	= NULL;
		Direct3DTexture9		= NULL;
	}
	else if( CompositeTexture )
	{
		//!!vogel
		// In UT2003 lightmaps are the only composite textures. They are opaque RGBA8888 512x512 and
		// we therefore can easily convert them to 256x256 RGB565. If this assumption ever changes please
		// change the code below.
		INT	NumChildren = CompositeTexture->GetNumChildren();

		if(!Direct3DTexture9 || CachedWidth != USize || CachedHeight != VSize || CachedFirstMip != 0 || CachedNumMips != 1 || CachedFormat != Direct3DFormat)
		{
			// Release the existing texture.
			if(Direct3DTexture9)
				Direct3DTexture9->Release();

			// Voodoo 3 specific hack.
			if( ActualFormat == TEXF_RGBA8 && !RenDev->SupportsTextureFormat(TEXF_RGBA8) )
				Direct3DFormat = D3DFMT_R5G6B5;

			// Create a new Direct3D texture.
			Result = RenDev->Direct3DDevice9->CreateTexture(
				USize,
				VSize,
				1,
				0,
				Direct3DFormat,
				D3DPOOL_MANAGED,
				&Direct3DTexture9,
				NULL
				);
	
			if( FAILED(Result) )
				appErrorf(TEXT("CreateTexture failed(%s)."),*D3DError(Result));

			CachedChildRevision.Empty(NumChildren);
		}

		if(CachedChildRevision.Num() != NumChildren)
		{
			CachedChildRevision.Empty(NumChildren);
			CachedChildRevision.AddZeroed(NumChildren);

			// Lock the Direct3D texture.
			D3DLOCKED_RECT	LockedRect;

			Result = Direct3DTexture9->LockRect(0,&LockedRect,NULL,0);

			if( FAILED(Result) )
				appErrorf(TEXT("LockRect failed(%s)."),*D3DError(Result));

			// Copy all children into the texture.
			BYTE*	Data	= (BYTE*) LockedRect.pBits;
			INT		Pitch	= LockedRect.Pitch;
#ifdef _XBOX
			//!!vogel: TODO: don't use dynamic memory allocation.
			if ( ActualFormat == TEXF_RGBA8 )
				Data = new BYTE[VSize * LockedRect.Pitch];
#else
			if( Direct3DFormat == D3DFMT_R5G6B5 )
			{
				Pitch	= Width * 4;
				Data	= new BYTE[Height * Pitch];
			}
#endif
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

#ifdef _XBOX
			// Non compressed textures need swizzling.
			if( ActualFormat == TEXF_RGBA8 )
			{
				XGSwizzleRect(Data,LockedRect.Pitch,NULL,LockedRect.pBits,USize,VSize,NULL,4);
				delete [] Data;
			}
#else
			if( Direct3DFormat == D3DFMT_R5G6B5 )
			{
				// Convert 512x512 RGBA8888 lightmap to 256x256 RGB565
				DWORD* Src = (DWORD*) Data;
				_WORD* Dst = (_WORD*) LockedRect.pBits;
				
				check( (USize == 256) && (VSize == 256) && DownSample );
				for( INT y=0; y<VSize; y++ )
				{
					for( INT x=0; x<USize; x++ )
					{
						*(Dst++) = FColor(*Src).HiColor565();
						Src += 2;
					}
					Src += USize * 2;
				}
				delete [] Data;
			}
#endif
			// Unlock the Direct3D texture.
			Direct3DTexture9->UnlockRect(0);
		}
		else
		{
			// XBox relies on this code never be called.
			check(GIsEditor);

			// Update modified subrects of the texture.
			for(INT ChildIndex = 0;ChildIndex < NumChildren;ChildIndex++)
			{
				INT			ChildX = 0,
							ChildY = 0;
				FTexture*	Child = CompositeTexture->GetChild(ChildIndex,&ChildX,&ChildY);
				INT			ChildRevision = Child->GetRevision();

				if(CachedChildRevision(ChildIndex) != ChildRevision)
				{
					// Lock the Direct3D texture.
					D3DLOCKED_RECT	LockedRect;
					RECT			ChildRect;
			
					ChildRect.left = ChildX;
					ChildRect.top = ChildY;
					ChildRect.right = ChildX + Child->GetWidth();
					ChildRect.bottom = ChildY + Child->GetHeight();

					Result = Direct3DTexture9->LockRect(0,&LockedRect,&ChildRect,0);

					if( FAILED(Result) )
						appErrorf(TEXT("LockRect failed(%s)."),*D3DError(Result));

					// Read the child texture into the locked region.
					Child->GetTextureData(0,(void*) LockedRect.pBits,LockedRect.Pitch,ActualFormat,0);

					// Unlock the Direct3D texture.
					Direct3DTexture9->UnlockRect(0);

					CachedChildRevision(ChildIndex) = ChildRevision;
				}
			}
		}
	}
	else if ( Cubemap && RenDev->SupportsCubemaps && RenDev->SupportsTextureFormat(ActualFormat) )
	{
		//!! TODO: conversion code.
		if(!Direct3DCubeTexture9 || CachedWidth != USize || CachedHeight != VSize || CachedFirstMip != FirstMip || CachedNumMips != NumMips || CachedFormat != Direct3DFormat)
		{
			// Release the existing texture.
			if(Direct3DCubeTexture9)
				Direct3DCubeTexture9->Release();

			if( Width != Height )
				appErrorf(TEXT("Cubemaps must be square"));

			// Create a new Direct3D texture.
			Result = RenDev->Direct3DDevice9->CreateCubeTexture(
				USize,
				RenDev->UseMippedCubemaps ? NumMips - FirstMip : 1,
				0,
				Direct3DFormat,
				D3DPOOL_MANAGED,
				&Direct3DCubeTexture9,
				NULL
				);
	
			if( FAILED(Result) )
				appErrorf(TEXT("CreateCubeTexture failed(%s)."),*D3DError(Result));
		}

		// Copy the cubemap into the Direct3D cube texture.
		INT Count = RenDev->UseMippedCubemaps ? NumMips : FirstMip+1; 
		for(INT MipIndex=FirstMip; MipIndex<Count; MipIndex++)
		{
			// Lock the Direct3D texture.
			D3DLOCKED_RECT	LockedRect;
	
			for(INT Face=0; Face<6; Face++)
			{
				FTexture*	Texture = Cubemap->GetFace(Face);

				if(Texture)
				{
					Result = Direct3DCubeTexture9->LockRect((D3DCUBEMAP_FACES)Face,MipIndex-FirstMip,&LockedRect,NULL,0);

					if(Result != D3D_OK)
						appErrorf(TEXT("LockRect failed(%s)."),*D3DError(Result));
#ifdef _XBOX
					// Non compressed textures need swizzling.
					if ( ActualFormat == TEXF_RGBA8 )
					{
						INT MipWidth  = USize >> (MipIndex - FirstMip);
						INT MipHeight = VSize >> (MipIndex - FirstMip);
						check( MipHeight && MipWidth );
						//!!vogel: TODO: don't use dynamic memory allocation.
						BYTE* Data  = new BYTE[MipHeight * LockedRect.Pitch];
						Texture->GetTextureData(MipIndex,(void*) Data,LockedRect.Pitch,ActualFormat);
						XGSwizzleRect(Data,LockedRect.Pitch,NULL,LockedRect.pBits,MipWidth,MipHeight,NULL,4);
						delete [] Data;
					}
					else
#endif
					Texture->GetTextureData(MipIndex,(void*) LockedRect.pBits,LockedRect.Pitch,ActualFormat);

					// Unlock the Direct3D texture.
					Direct3DCubeTexture9->UnlockRect((D3DCUBEMAP_FACES)Face,MipIndex-FirstMip);
				}
			}
		}
	}
	else if(RenderTarget)
	{
		// Pick render target format.
		Direct3DFormat = D3DFMT_A8R8G8B8;

		if( !RenDev->SupportsRenderToTextureRGBA8888 )
		{
			if( RenDev->SupportsRenderToTextureRGB565 )
				Direct3DFormat = D3DFMT_R5G6B5;
			else
				Failed = 1;
		}

		if( !Failed && (!Direct3DTexture9 || CachedWidth != USize || CachedHeight != VSize || CachedFirstMip != FirstMip || CachedNumMips != NumMips || CachedFormat != Direct3DFormat) )
		{
			// Release the existing texture.

			if(Direct3DTexture9)
				Direct3DTexture9->Release();

			// Try to allocate the render target.
			// Direct3D won't bump managed textures out of memory for it, so if the first allocation fails,
			// it will manually ask D3D to free up video memory and try to allocate the render target again.

			INT	NumTries = 0;

			Result = 0;

			while(NumTries < 2)
			{
				// Create a new Direct3D texture.

				Result = RenDev->Direct3DDevice9->CreateTexture(
					USize,
					VSize,
					NumMips - FirstMip,
					D3DUSAGE_RENDERTARGET,
					Direct3DFormat,
					D3DPOOL_DEFAULT,
					&Direct3DTexture9,
					NULL
					);

				if( !FAILED(Result) )
					break;

#ifndef _XBOX
				if( FAILED(RenDev->Direct3DDevice9->EvictManagedResources(/*GetBytesPerPixel(ActualFormat,USize) * VSize*/)) )
					appErrorf(TEXT("EvictManagedResources failed."));
#endif

				NumTries++;
			};

			if( FAILED(Result) )
				Failed = 1;
			else
			{
				// Retrieve the texture's surface.

				Result = Direct3DTexture9->GetSurfaceLevel(0,&RenderTargetSurface);

				if( FAILED(Result) )
					appErrorf(TEXT("GetSurfaceLevel failed(%s)."),*D3DError(Result));
			}
		}

		if(!Failed)
		{
			if(!DepthStencilSurface || CachedWidth != USize || CachedHeight != VSize || CachedFirstMip != FirstMip || CachedNumMips != NumMips)
			{
				// Release the existing texture.
				if(DepthStencilSurface)
					DepthStencilSurface->Release();

				INT	NumTries = 0;

				Result = 0;

				while(NumTries < 4)
				{
					// Create a new Direct3D depth-stencil surface.

					Result = RenDev->Direct3DDevice9->CreateDepthStencilSurface(
						USize,
						VSize,
#ifdef _XBOX
						D3DFMT_LIN_D24S8,
#else
						RenDev->DepthBufferFormat,
#endif
						D3DMULTISAMPLE_NONE,
						0,                                //multisample quality
						TRUE,
						&DepthStencilSurface,
						NULL
						);

					if( !FAILED(Result) )
						break;

#ifndef _XBOX
					if( FAILED(RenDev->Direct3DDevice9->EvictManagedResources(/*USize * VSize * 4*/)) )
						appErrorf(TEXT("EvictManagedResources failed."));
#endif

					NumTries++;
				};
		
				if( FAILED(Result) )
					Failed = 1;
			}
		}

		if(Failed)
		{
			if(RenderTargetSurface)
				RenderTargetSurface->Release();

			if(Direct3DTexture9)
				Direct3DTexture9->Release();
		
			if(DepthStencilSurface)
				DepthStencilSurface->Release();

			Direct3DTexture9		= NULL;
			Direct3DCubeTexture9	= NULL;
			RenderTargetSurface		= NULL;
			DepthStencilSurface		= NULL;
		}
	}
	else if ( Texture )
	{
		// Conversion variables.
		BYTE* Data		= NULL;
		UBOOL ForceRGBA = 0;
		UBOOL WasRGBA	= 0;

		// Convert if necessary.
		if( !RenDev->SupportsTextureFormat(ActualFormat) && (IsDXTC(ActualFormat) || RenDev->Use16bitTextures) )
		{
			if( ActualFormat == TEXF_RGBA8 )
			{
				Direct3DFormat = D3DFMT_A4R4G4B4;
				if( DownSample )
				{
					Data = new BYTE[4*Width*Height];
					Texture->GetTextureData(FirstMip,Data,Width * 4,ActualFormat);
				}
				else
				{
					Data = new BYTE[4*USize*VSize];
					Texture->GetTextureData(FirstMip,Data,USize * 4,ActualFormat);
				}
				WasRGBA = 1;
			}
			else
			{
				
				ActualFormat = Texture->GetUTexture()->ConvertDXT( FirstMip, !RenDev->SupportsTextureFormat(TEXF_DXT1), 0, &Data );
				switch( ActualFormat )
				{
				case TEXF_RGBA8:
					Direct3DFormat	= RenDev->SupportsTextureFormat(TEXF_RGBA8) ? D3DFMT_A8R8G8B8 : D3DFMT_A4R4G4B4;
					ForceRGBA		= 1;
					break;
				case TEXF_DXT1:
					Direct3DFormat	= D3DFMT_DXT1;
					ForceRGBA		= 0;
					break;
				default:
					appErrorf(TEXT("ConvertDXT returned unknown texture format"));
				}
			}
		}

		if(!Direct3DTexture9 || CachedWidth != USize || CachedHeight != VSize || CachedFirstMip != FirstMip || CachedNumMips != NumMips || CachedFormat != Direct3DFormat)
		{
			// Release the existing texture.
			if(Direct3DTexture9)
				Direct3DTexture9->Release();

			// Create a new Direct3D texture.
			Result = RenDev->Direct3DDevice9->CreateTexture(
				USize,
				VSize,
				NumMips - FirstMip,
				0,
				Direct3DFormat,
				D3DPOOL_MANAGED,
				&Direct3DTexture9,
				NULL
				);
	
			if( FAILED(Result) )
				appErrorf(TEXT("CreateTexture failed(%s)."),*D3DError(Result));
		}

		// Copy the texture into the Direct3D texture.
		for(INT MipIndex = FirstMip;MipIndex < NumMips;MipIndex++)
		{
			// Lock the Direct3D texture.
			D3DLOCKED_RECT	LockedRect;
			INT				MipWidth  = USize >> (MipIndex - FirstMip),
							MipHeight = VSize >> (MipIndex - FirstMip);
	
			Result = Direct3DTexture9->LockRect(MipIndex - FirstMip,&LockedRect,NULL,0);

			if( FAILED(Result) )
				appErrorf(TEXT("LockRect failed(%s)."),*D3DError(Result));

#ifdef _XBOX
			// Non compressed textures need swizzling.
			if ( ActualFormat == TEXF_RGBA8 )
			{
				if ( MipHeight && MipWidth ) //!!TODO: DEBUG
				{
					BYTE* Data  = new BYTE[MipHeight * LockedRect.Pitch];
					Texture->GetTextureData(MipIndex,(void*) Data,LockedRect.Pitch,ActualFormat,(MipIndex >= RenDev->FirstColoredMip));
					XGSwizzleRect(Data,LockedRect.Pitch,NULL,LockedRect.pBits,MipWidth,MipHeight,NULL,4);
					delete [] Data;
				}
				else
					debugf(TEXT("!!vogel: debug this"));
			}
			else
#else
			if( Data )
			{
				INT USize	= MipWidth;
				INT VSize	= MipHeight;
				INT Stride;
				if( IsDXTC(ActualFormat) )
				{
					USize	= Max( USize, 4 );
					VSize	= Max( VSize, 4 ) / 4;
					Stride	= GetBytesPerPixel(ActualFormat, USize) * 4;
				}
				else
					Stride	= GetBytesPerPixel(ActualFormat, USize);	

				if( RenDev->Use16bitTextures )
				{
					Stride /= 2;

					// convert to 4444
					DWORD* Src = (DWORD*) Data;
					_WORD* Dst = (_WORD*) Data;
					if( DownSample )
					{
						for( INT y=0; y<VSize; y++ )
						{
							for( INT x=0; x<USize; x++ )
							{
								*(Dst++) = FColor(*Src).HiColor4444();
								Src += ScaleX;
							}
							Src += Width * (ScaleY - 1);
						}
					}
					else
					{
						for( INT y=0; y<VSize; y++ )
						{
							for( INT x=0; x<USize; x++ )		
							{
								*(Dst++) = FColor(*(Src++)).HiColor4444();
							}
						}
					}
				}

				if( LockedRect.Pitch == Stride )
					appMemcpy( LockedRect.pBits, Data, Stride * VSize );
				else
				{
					BYTE* Dest = (BYTE*) LockedRect.pBits;
					BYTE* Src  = (BYTE*) Data;
					for( INT h=0; h<VSize; h++ )
					{
						appMemcpy( Dest, Src, Stride );
						Dest += LockedRect.Pitch;
						Src  += Stride;
					}
				}
			}
			else
#endif
			if( !RenDev->Use16bitTextures )
				Texture->GetTextureData(MipIndex,(void*) LockedRect.pBits,LockedRect.Pitch,ActualFormat,(MipIndex >= RenDev->FirstColoredMip));
	
			if( Data )
			{
				delete [] Data;
				Data = NULL;
				if( MipIndex < (NumMips-1) )
				{
					if( WasRGBA )
					{
						// Actually too much memory.
						Data = new BYTE[4 * MipWidth * MipHeight];
						Texture->GetTextureData(MipIndex+1,Data,MipWidth*2,ActualFormat);
					}
					else
						Texture->GetUTexture()->ConvertDXT( MipIndex+1, ForceRGBA, 0, &Data );		
				}
			}

			// Unlock the Direct3D texture.
			Direct3DTexture9->UnlockRect(MipIndex - FirstMip);
		}
		delete [] Data;
	}
	// Not supported. Instead of bombing out set the NULL texture.
	else
	{
		Direct3DCubeTexture9	= NULL;
		Direct3DTexture9		= NULL;
	}

	// Update the cached info.
	CachedRevision	= SourceTexture->GetRevision();
	CachedWidth		= USize;
	CachedHeight	= VSize;
	CachedFirstMip	= FirstMip;
	CachedNumMips	= NumMips;
	CachedFormat	= Direct3DFormat;

	return Failed;

	unguard;
}

//
//	FD3D9VertexStream::FD3D9VertexStream
//
FD3D9VertexStream::FD3D9VertexStream(UD3D9RenderDevice* InRenDev,QWORD InCacheId) : FD3D9Resource(InRenDev,InCacheId)
{
	Direct3DVertexBuffer9 = NULL;
}

//
//	FD3D9VertexStream::~FD3D9VertexStream
//
FD3D9VertexStream::~FD3D9VertexStream()
{
	if(Direct3DVertexBuffer9)
		Direct3DVertexBuffer9->Release();
}

//
//  FD3D9VertexStream::CalculateFootprint
//
INT FD3D9VertexStream::CalculateFootprint()
{
	return CachedSize;
}

//
//	FD3D9VertexStream::Cache
//
void FD3D9VertexStream::Cache(FVertexStream* SourceStream)
{
	guard(FD3D9VertexStream::Cache);

	check(SourceStream);
	check(RenDev);

	INT		Size = SourceStream->GetSize();
	HRESULT	Result;

    // sjs ---
    DWORD UsageFlags = D3DUSAGE_WRITEONLY;
    D3DPOOL PoolFlags = RESOURCE_POOL;
    DWORD LockFlags = 0;
    if( (Size < 0) || SourceStream->HintDynamic() )
    {
        Size = Abs(Size);
        UsageFlags = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;
        PoolFlags = D3DPOOL_DEFAULT;
#ifndef _XBOX
        LockFlags = D3DLOCK_DISCARD;
#endif
    }
    // --- sjs

#ifndef _XBOX
	if( SourceStream->UseNPatches() && RenDev->UseNPatches )
		UsageFlags |= D3DUSAGE_NPATCHES;

	if( !RenDev->UseHardwareTL )
		UsageFlags |= D3DUSAGE_SOFTWAREPROCESSING;
#endif

	if(!Direct3DVertexBuffer9 || CachedSize != Size)
	{
		// Release the existing vertex buffer.
		if(Direct3DVertexBuffer9)
			Direct3DVertexBuffer9->Release();

		D3DPOOL PoolFlags	= D3DPOOL_DEFAULT;
		INT Result	 = D3D_OK;
		INT NumTries = 0;

		while( NumTries < 3 )
		{
			// Use systemmem pool if allocation in default pool fails.
			if( NumTries == 2 )
				PoolFlags = D3DPOOL_SYSTEMMEM; 

			// Create a new Direct3D vertex buffer.
			Result = RenDev->Direct3DDevice9->CreateVertexBuffer(Size,UsageFlags,0,PoolFlags,&Direct3DVertexBuffer9, NULL);
			if( SUCCEEDED(Result) )
				break;
#ifndef _XBOX
			if( FAILED(RenDev->Direct3DDevice9->EvictManagedResources() ) )
				appErrorf(TEXT("EvictManagedResources failed."));
#endif
			NumTries++;
		}
	
		if( FAILED(Result) )
			appErrorf(TEXT("CreateVertexBuffer failed(%s)."),*D3DError(Result));
	}

	// Lock the vertex buffer.
	BYTE*	VertexBufferContents;

	Result = Direct3DVertexBuffer9->Lock(0, Size, reinterpret_cast<void**>(&VertexBufferContents), LockFlags);

	if( FAILED(Result) )
		appErrorf(TEXT("IDirect3DVertexBuffer9::Lock failed(%s)."),*D3DError(Result));

	// Copy the stream contents into the vertex buffer.
	try
	{
		SourceStream->GetStreamData(VertexBufferContents);
	} 
	catch( ... ) 
	{
		debugf(NAME_Error, TEXT("Locked rendering resource invalid - mode switch?"));
	}


	// Unlock the vertex buffer.
	Result = Direct3DVertexBuffer9->Unlock();

	if( FAILED(Result) )
		appErrorf(TEXT("IDirect3DVertexBuffer9::Unlock failed(%s)."),*D3DError(Result));

	// Update the cached info.
	CachedRevision = SourceStream->GetRevision();
	CachedSize = Size;

	unguard;
}

//
//	FD3D9DynamicVertexStream::FD3D9DynamicVertexStream
//
#define INITIAL_DYNAMIC_VERTEXBUFFER_SIZE	65536	// Initial size of dynamic vertex buffers, in bytes.

BYTE DummyBuffer[512 * 1024];

FD3D9DynamicVertexStream::FD3D9DynamicVertexStream(UD3D9RenderDevice* InRenDev) : FD3D9VertexStream(InRenDev,NULL)
{
	guard(FD3D9DynamicVertexStream::FD3D9DynamicVertexStream);

	Reallocate(INITIAL_DYNAMIC_VERTEXBUFFER_SIZE);

	unguard;
}

//
//	FD3D9DynamicVertexStream::Reallocate
//
void FD3D9DynamicVertexStream::Reallocate(INT NewSize)
{
	guard(FD3D9DynamicVertexStream::Reallocate);

	debugf(TEXT("Allocating %u byte dynamic vertex buffer."),NewSize);

	// Release the old vertex buffer.
	if(Direct3DVertexBuffer9)
	{
		// Make sure it's not currently bound via SetStreamSource.
		RenDev->DeferredState.UnSetVertexBuffer( Direct3DVertexBuffer9 );
		Direct3DVertexBuffer9->Release();
		Direct3DVertexBuffer9 = NULL;
	}

	// Create a dynamic vertex buffer to hold the vertices.
	CachedSize = NewSize;
	Tail = 0;

	DWORD UsageFlags = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;

#ifndef _XBOX
	if( !RenDev->UseHardwareTL )
		UsageFlags |= D3DUSAGE_SOFTWAREPROCESSING;
#endif

	D3DPOOL PoolFlags	= D3DPOOL_DEFAULT;
	INT Result	 = D3D_OK;
	INT NumTries = 0;

	while( NumTries < 3 )
	{
		// Use systemmem pool if allocation in default pool fails.
		if( NumTries == 2 )
			PoolFlags = D3DPOOL_SYSTEMMEM; 

		// Create a new Direct3D vertex buffer.
		Result = RenDev->Direct3DDevice9->CreateVertexBuffer(CachedSize,UsageFlags,0,PoolFlags,&Direct3DVertexBuffer9, NULL);

		if( SUCCEEDED(Result) )
				break;
#ifndef _XBOX
			if( FAILED(RenDev->Direct3DDevice9->EvictManagedResources()))
				appErrorf(TEXT("EvictManagedResources failed."));
#endif
		NumTries++;
	}

	if( FAILED(Result) )
		appErrorf(TEXT("CreateVertexBuffer failed(%s)."),*D3DError(Result));

	unguard;
}

//
//	FD3D9DynamicVertexStream::AddVertices
//
INT FD3D9DynamicVertexStream::AddVertices(FVertexStream* Stream)
{
	guard(FD3D9DynamicVertexStream::AddVertices);

	check(Stream);
	check(RenDev);
	check(Direct3DVertexBuffer9);

	INT	Size = Abs(Stream->GetSize()),
		Stride = Stream->GetStride();

	// If the dynamic vertex buffer isn't big enough to contain all the vertices, resize it.
	DWORD	LockFlags = D3DLOCK_NOOVERWRITE;

	if(Size > CachedSize)
	{
		Reallocate(Size);
#ifndef _XBOX
		LockFlags = D3DLOCK_DISCARD;
#else
		LockFlags = 0;
#endif
		GStats.DWORDStats(RenDev->D3DStats.STATS_DynamicVertexBufferDiscards)++;
	}

	// If the dynamic vertex buffer will overflow with the additional vertices, flush it.
	if( Stream->GetSize()<0 || Tail + Stride + Size > CachedSize || ((DWORD) (Tail + Stride + Size) / Stride) > RenDev->DeviceCaps9.MaxVertexIndex) // sjs - hacked
	{
		Tail = 0;
#ifndef _XBOX
		LockFlags = D3DLOCK_DISCARD;
#else
		LockFlags = 0;
#endif
		GStats.DWORDStats(RenDev->D3DStats.STATS_DynamicVertexBufferDiscards)++;
	}

	// Determine the offset in the vertex buffer to allocate the vertices at.
	INT	VertexBufferOffset = ((Tail + Stride - 1) / Stride) * Stride;

	// Lock the dynamic vertex buffer.
	BYTE*	VertexBufferContents = NULL;

	clock(GStats.DWORDStats(RenDev->D3DStats.STATS_DynamicVertexBufferLockCycles));
	HRESULT	Result = Direct3DVertexBuffer9->Lock(VertexBufferOffset, Size, reinterpret_cast<void**>(&VertexBufferContents), LockFlags);
	unclock(GStats.DWORDStats(RenDev->D3DStats.STATS_DynamicVertexBufferLockCycles));
	GStats.DWORDStats(RenDev->D3DStats.STATS_DynamicVertexBufferLockCalls)++;

	if( FAILED(Result) )
		appErrorf(TEXT("IDirect3DVertexBuffer9::Lock failed(%s)."),*D3DError(Result));

	BYTE*	Buffer		= VertexBufferContents;
	DWORD*	BufferEnd	= NULL;

	// Tag memory if overflow detection is enabled.
	if( RenDev->CheckForOverflow )
	{
		if( Size+4 > RenDev->StaticBuffer.Num() )
		{
			RenDev->StaticBuffer.Empty();
			RenDev->StaticBuffer.Add( 2 * Max(Size,65535) );
		}
		BufferEnd	= (DWORD*) &RenDev->StaticBuffer(Size+1);
		*BufferEnd	= 0x03221977;
		Buffer		= &RenDev->StaticBuffer(0);
	}

	// Copy the indices into the index buffer.
	try
	{
		Stream->GetStreamData(Buffer);
	} 
	catch( ... ) 
	{
		debugf(NAME_Error, TEXT("Locked rendering resource invalid - mode switch?"));
	}

	// Verify tag.
	if( RenDev->CheckForOverflow )
	{
		check( *BufferEnd == 0x03221977 );
		appMemcpy( VertexBufferContents, Buffer, Size );
	}

	// Unlock the dynamic vertex buffer.
	Result = Direct3DVertexBuffer9->Unlock();

	if( FAILED(Result) )
		appErrorf(TEXT("IDirect3DVertexBuffer9::Unlock failed(%s)."),*D3DError(Result));

	// Update the tail pointer.
	Tail = VertexBufferOffset + Size;

	return VertexBufferOffset / Stride;

	unguard;
}

//
//	FD3D9VertexShader::FD3D9VertexShader
//
FD3D9VertexShader::FD3D9VertexShader(UD3D9RenderDevice* InRenDev,FShaderDeclaration& InDeclaration)
{
	guard(FD3D9VertexShader::FD3D9VertexShader);

	RenDev		= InRenDev;
	Declaration = InDeclaration;

	// Build the Direct3D shader declaration.
	TArray<D3DVERTEXELEMENT9> D3DDeclaration;
	for(INT StreamIndex = 0;StreamIndex < Declaration.NumStreams;StreamIndex++)
	{
		INT CurrOffset = 0;
		for(INT ComponentIndex = 0;ComponentIndex < Declaration.Streams[StreamIndex].NumComponents;ComponentIndex++)
		{
	 		FVertexComponent& Component = Declaration.Streams[StreamIndex].Components[ComponentIndex];

			D3DVERTEXELEMENT9		Element;
			Element.Stream			= StreamIndex;
			Element.Offset			= CurrOffset;
			Element.Method			= D3DDECLMETHOD_DEFAULT;
			Element.UsageIndex		= 0;

			INT NumElements			= 0;

			switch( Component.Type )
			{
			case CT_Float4:
				Element.Type		= D3DDECLTYPE_FLOAT4;
				NumElements			= 4;
				break;
			case CT_Float3:
				Element.Type		= D3DDECLTYPE_FLOAT3;
				NumElements			= 3;
				break;
			case CT_Float2:
				Element.Type		= D3DDECLTYPE_FLOAT2;
				NumElements			= 2;
				break;
			case CT_Float1:
				Element.Type		= D3DDECLTYPE_FLOAT1;
				NumElements			= 1;
				break;
			case CT_Color:
				Element.Type		= D3DDECLTYPE_D3DCOLOR;
				NumElements			= 1;
				break;
			default:
				appErrorf(TEXT("Unknown component type %i."), Component.Type);
			}
		
			switch( Component.Function )
			{
			case FVF_Position:
				Element.Usage		= D3DDECLUSAGE_POSITION;
				break;
			case FVF_Normal:
				Element.Usage		= D3DDECLUSAGE_NORMAL;
				break;
			case FVF_Diffuse:
				Element.Usage		= D3DDECLUSAGE_COLOR;
				break;
			case FVF_Specular:
				Element.Usage		= D3DDECLUSAGE_COLOR;
				Element.UsageIndex	= 1;
				break;
			case FVF_TexCoord0:
			case FVF_TexCoord1:
			case FVF_TexCoord2:
			case FVF_TexCoord3:
			case FVF_TexCoord4:
			case FVF_TexCoord5:
			case FVF_TexCoord6:
			case FVF_TexCoord7:
				Element.Usage		= D3DDECLUSAGE_TEXCOORD;
				Element.UsageIndex	= Component.Function - FVF_TexCoord0;
				break;
			default:
				appErrorf(TEXT("Unknown component function %i."), Component.Function);
			}
			
			D3DDeclaration.AddItem(Element);

			CurrOffset += NumElements * 4;
		}
	}

	D3DVERTEXELEMENT9 EndElement = D3DDECL_END();
	D3DDeclaration.AddItem(EndElement);

	// Create the declaration object.
	HRESULT Result = RenDev->Direct3DDevice9->CreateVertexDeclaration( &D3DDeclaration(0), &Decl );
	if( FAILED(Result) )
		appErrorf(TEXT("CreateVertexDeclaration failed(%s)."),*D3DError(Result));

	// Add the vertex shader to the device vertex shader list.
	NextVertexShader		= RenDev->VertexShaders;
	RenDev->VertexShaders	= this;

	unguard;
}

//
//	FD3D9VertexShader::~FD3D9VertexShader
//
FD3D9VertexShader::~FD3D9VertexShader()
{
	guard(FD3D9VertexShader::~FD3D9VertexShader);

	// Remove the vertex shader from the device vertex shader list.
	FD3D9VertexShader*	ShaderPtr;

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

	// Delete the vertex shader.
	if (Shader) 
	{
		Shader->Release();
		Shader = NULL;
	}
	//RenDev->DeferredState.DeleteVertexShader(Handle);

	unguard;
}

//
//	FD3D9FixedVertexShader::FD3D9FixedVertexShader
//
FD3D9FixedVertexShader::FD3D9FixedVertexShader(UD3D9RenderDevice* InRenDev,FShaderDeclaration& InDeclaration) : FD3D9VertexShader(InRenDev,InDeclaration)
{
	guard(FD3D9FixedVertexShader::FD3D9FixedVertexShader);

	Type = VS_FixedFunction;

	// Create the fixed function vertex shader.
	
	// There's no longer such thing as a fixed function vertex shader in dx9, so we null out the shader ptr
	Shader = NULL;	

	unguard;
}

//
//	FD3D9IndexBuffer::FD3D9IndexBuffer
//
FD3D9IndexBuffer::FD3D9IndexBuffer(UD3D9RenderDevice* InRenDev,QWORD InCacheId) : FD3D9Resource(InRenDev,InCacheId)
{
	Direct3DIndexBuffer9 = NULL;
}

//
//	FD3D9IndexBuffer::~FD3D9IndexBuffer
//
FD3D9IndexBuffer::~FD3D9IndexBuffer()
{
	if(Direct3DIndexBuffer9)
		Direct3DIndexBuffer9->Release();
}

//
//  FD3D9IndexBuffer::CalculateFootprint
//
INT FD3D9IndexBuffer::CalculateFootprint()
{
	return CachedSize;
}

//
//	FD3D9IndexBuffer::Cache
//
void FD3D9IndexBuffer::Cache(FIndexBuffer* SourceIndexBuffer)
{
	guard(FD3D9IndexBuffer::Cache);

	INT		Size = Max(2,SourceIndexBuffer->GetSize());
	HRESULT	Result;

	if(!Direct3DIndexBuffer9 || CachedSize != Size)
	{
		// Release the existing index buffer.
		if(Direct3DIndexBuffer9)
			Direct3DIndexBuffer9->Release();

		D3DFORMAT IndexFormat;
		if( SourceIndexBuffer->GetIndexSize() == sizeof(DWORD) )
#ifdef _XBOX
		{
			IndexFormat = (D3DFORMAT) 0;
			check(0);
		}
#else
			IndexFormat = D3DFMT_INDEX32;
#endif
		else
			IndexFormat = D3DFMT_INDEX16;

		DWORD UsageFlags = D3DUSAGE_WRITEONLY;

#ifndef _XBOX
		if( !RenDev->UseHardwareTL )
			UsageFlags |= D3DUSAGE_SOFTWAREPROCESSING;
#endif

		// Create a new index buffer.
		Result = RenDev->Direct3DDevice9->CreateIndexBuffer(Size,UsageFlags,IndexFormat,RESOURCE_POOL,&Direct3DIndexBuffer9, NULL);

		if( FAILED(Result) )
			appErrorf(TEXT("CreateIndexBuffer failed(%s)."),*D3DError(Result));
	}

	if(CachedRevision != SourceIndexBuffer->GetRevision())
	{
		// Lock the index buffer.
		BYTE*	IndexBufferContents = NULL;

		Result = Direct3DIndexBuffer9->Lock(0, Size, reinterpret_cast<void**>(&IndexBufferContents), 0);

		if( FAILED(Result) )
			appErrorf(TEXT("IDirect3DIndexBuffer9::Lock failed(%s)."),*D3DError(Result));

		// Fill the index buffer.
		SourceIndexBuffer->GetContents(IndexBufferContents);

		// Unlock the index buffer.
		Result = Direct3DIndexBuffer9->Unlock();

		if( FAILED(Result) )
			appErrorf(TEXT("IDirect3DIndexBuffer9::Unlock failed(%s)."),*D3DError(Result));
	}

	// Update cached info.
	CachedRevision = SourceIndexBuffer->GetRevision();
	CachedSize = Size;

	unguard;
}

//
//	FD3D9DynamicIndexBuffer::FD3D9DynamicIndexBuffer
//
#define INITIAL_DYNAMIC_INDEXBUFFER_SIZE	16384	// Initial size of dynamic index buffers, in bytes.

FD3D9DynamicIndexBuffer::FD3D9DynamicIndexBuffer(UD3D9RenderDevice* InRenDev, INT InIndexSize) : FD3D9IndexBuffer(InRenDev,NULL), IndexSize(InIndexSize)
{
	guard(FD3D9DynamicIndexBuffer::FD3D9DynamicIndexBuffer);

	Reallocate(INITIAL_DYNAMIC_INDEXBUFFER_SIZE);

	unguard;
}

//
//	FD3D9DynamicIndexBuffer::Reallocate
//
void FD3D9DynamicIndexBuffer::Reallocate(INT NewSize)
{
	guard(FD3D9DynamicIndexBuffer::Reallocate);

	debugf(TEXT("Allocating %u byte dynamic index buffer."),NewSize);

	// Release the old index buffer.
	// NOTE: Unsure of this, but I imagine the old index buffer will actually
	// stick around until all rendering calls using it are retired.
	//!!vogel: see comment in VertexBuffer::Reallocate
	if(Direct3DIndexBuffer9)
	{
		Direct3DIndexBuffer9->Release();
		Direct3DIndexBuffer9 = NULL;
	}

	// Create a dynamic vertex buffer to hold the vertices.
	CachedSize = NewSize;
	Tail = 0;

	D3DFORMAT IndexFormat;
	if( IndexSize == sizeof(DWORD) )
#ifdef _XBOX
	{
		IndexFormat = (D3DFORMAT) 0;
		check(0);
	}
#else
		IndexFormat = D3DFMT_INDEX32;
#endif
	else
		IndexFormat = D3DFMT_INDEX16;

	DWORD UsageFlags = D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY;

#ifndef _XBOX
	if( !RenDev->UseHardwareTL )
		UsageFlags |= D3DUSAGE_SOFTWAREPROCESSING;
#endif
	
	D3DPOOL PoolFlags	= D3DPOOL_DEFAULT;
	INT Result	 = D3D_OK;
	INT NumTries = 0;

	while( NumTries < 3 )
	{
		// Use systemmem pool if allocation in default pool fails.
		if( NumTries == 2 )
			PoolFlags = D3DPOOL_SYSTEMMEM; 

		// Create a new Direct3D vertex buffer.
		Result = RenDev->Direct3DDevice9->CreateIndexBuffer(CachedSize,UsageFlags,IndexFormat,PoolFlags,&Direct3DIndexBuffer9, NULL);

		if( SUCCEEDED(Result) )
				break;
#ifndef _XBOX
			if( FAILED(RenDev->Direct3DDevice9->EvictManagedResources() ) )
				appErrorf(TEXT("EvictManagedResources failed."));
#endif
		NumTries++;
	}

	if( FAILED(Result) )
		appErrorf(TEXT("CreateIndexBuffer failed(%s)."),*D3DError(Result));

	unguard;
}

//
//	FD3D9DynamicIndexBuffer::AddIndices
//
INT FD3D9DynamicIndexBuffer::AddIndices(FIndexBuffer* Indices)
{
	guard(FD3D9DynamicIndexBuffer::AddIndices);

    INT	Size, Stride;
    guard(GetSize);
	Size = Indices->GetSize();
	Stride = Indices->GetIndexSize();
    unguard;

	// If the dynamic index buffer isn't big enough to contain all the indices, resize it.
	DWORD	LockFlags = D3DLOCK_NOOVERWRITE;

	if(Size > CachedSize)
	{
		Reallocate(Size);
#ifndef _XBOX
		LockFlags = D3DLOCK_DISCARD;
#else
		LockFlags = 0;
#endif
	}

	// If the dynamic index buffer will overflow with the additional indices, flush it.
	if(Tail + Size > CachedSize)
	{
		Tail = 0;
#ifndef _XBOX
		LockFlags = D3DLOCK_DISCARD;
#else
		LockFlags = 0;
#endif
	}

	// Determine the offset in the index buffer to allocate the indices at.
	INT	IndexBufferOffset = Tail;

	// Lock the dynamic index buffer.
	BYTE*	IndexBufferContents = NULL;

	HRESULT	Result = Direct3DIndexBuffer9->Lock(IndexBufferOffset, Size, reinterpret_cast<void**>(&IndexBufferContents), LockFlags);

	if( FAILED(Result) )
		appErrorf(TEXT("IDirect3DIndexBuffer9::Lock failed(%s)."),*D3DError(Result));

	BYTE*	Buffer		= IndexBufferContents;
	DWORD*	BufferEnd	= NULL;

	// Tag memory if overflow detection is enabled.
	if( RenDev->CheckForOverflow )
	{
		if( Size+4 > RenDev->StaticBuffer.Num() )
		{
			RenDev->StaticBuffer.Empty();
			RenDev->StaticBuffer.Add( 2 * Max(Size,65535) );
		}
		BufferEnd	= (DWORD*) &RenDev->StaticBuffer(Size+1);
		*BufferEnd	= 0x03221977;
		Buffer		= &RenDev->StaticBuffer(0);
	}

	// Copy the indices into the index buffer.
	try
	{
		Indices->GetContents(Buffer);
	} 
	catch( ... ) 
	{
		debugf(NAME_Error, TEXT("Locked rendering resource invalid - mode switch?"));
	}

	// Verify tag.
	if( RenDev->CheckForOverflow )
	{
		check( *BufferEnd == 0x03221977 );
		appMemcpy( IndexBufferContents, Buffer, Size );
	}

	// Unlock the dynamic index buffer.
	Result = Direct3DIndexBuffer9->Unlock();

	if( FAILED(Result) )
		appErrorf(TEXT("IDirect3DIndexBuffer9::Unlock failed(%s)."),*D3DError(Result));

	// Update the tail pointer.
	Tail = IndexBufferOffset + Size;

	return IndexBufferOffset / Stride;

	unguard;
}

INT appAnsiStrlen( ANSICHAR* p )
{
	INT i;
	for( i=0;p[i];i++ );
	return i;
}

//
// FD3D9PixelShader::FD3D9PixelShader
//
FD3D9PixelShader::FD3D9PixelShader(UD3D9RenderDevice* InRenDev, EPixelShader InType, ANSICHAR* InSource)
{
	guard(FD3D9PixelShader::FD3D9PixelShader);

	RenDev	= InRenDev;
	Type	= InType;
	Source	= InSource;

#ifdef _XBOX
	LPXGBUFFER	Object	= NULL;
	LPXGBUFFER	Errors	= NULL;
#else
	ID3DXBuffer *Object = NULL;
	ID3DXBuffer *Errors = NULL;
#endif

	guard(AssembleShader);	
	// Assemble the pixel shader source code
#ifdef _XBOX
	FString Dummy = TEXT("XBox");
	HRESULT Result = XGAssembleShader((CHAR*) *Dummy, Source, appAnsiStrlen(Source), 0, NULL, &Object, &Errors, NULL, NULL, NULL, NULL );
	if( FAILED(Result) )
	{
		if( Errors )
			appErrorf(TEXT("XGAssembleShader failed(%s): %s"),*D3DError(Result), appFromAnsi((ANSICHAR*)XGBuffer_GetBufferPointer(Errors)) );
		else
			appErrorf(TEXT("XGAssembleShader failed(%s).  Len was %d"),*D3DError(Result), appAnsiStrlen(Source) );
	}
#else
	HRESULT Result = D3DXAssembleShader( Source, appAnsiStrlen(Source), 0, NULL, 0, &Object, &Errors );
	if( FAILED(Result) )
	{
		if( Errors )
			appErrorf(TEXT("DXAssembleShader failed(%s): %s"),*D3DError(Result), appFromAnsi((ANSICHAR*)Errors->GetBufferPointer()) );
		else
			appErrorf(TEXT("DXAssembleShader failed(%s).  Len was %d"),*D3DError(Result), appAnsiStrlen(Source) );
	}
#endif
	unguard;

	guard(CreateShader);
	// Create the pixel shader
#ifdef _XBOX
	HRESULT Result = RenDev->Direct3DDevice9->CreatePixelShader( (D3DPIXELSHADERDEF*) XGBuffer_GetBufferPointer(Object), &Handle );
#else
	HRESULT Result = RenDev->Direct3DDevice9->CreatePixelShader( (DWORD*) Object->GetBufferPointer(), &Shader );
#endif
	if( FAILED(Result) )
		appErrorf(TEXT("CreatePixelShader failed(%s)."),*D3DError(Result));
	unguard;

#ifndef _XBOX
	//!!vogel: FIXME
	Object->Release();
#endif

	// Add the Pixel shader to the device Pixel shader list.
	NextPixelShader = RenDev->PixelShaders;
	RenDev->PixelShaders = this;

	unguard;
}

//
//	FD3D9PixelShader::~FD3D9PixelShader
//
FD3D9PixelShader::~FD3D9PixelShader()
{
	guard(FD3D9PixelShader::~FD3D9PixelShader);

	// Remove the Pixel shader from the device Pixel shader list.
	FD3D9PixelShader*	ShaderPtr;

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
	if( Shader )
		Shader->Release();

	Shader = NULL;

	unguard;
}

