/*=============================================================================
	D3DResource.h: Unreal Direct3D resource definition.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#ifndef HEADER_D3DRESOURCE
#define HEADER_D3DRESOURCE

//
//	FD3DResource
//
class FD3DResource
{
public:

	UD3DRenderDevice*	RenDev;
	QWORD				CacheId;
	INT					CachedRevision,
						HashIndex;

	FD3DResource*		NextResource;
	FD3DResource*		HashNext;

	INT					LastFrameUsed;

	FString				Name;

	// Constructor/destructor.
	FD3DResource(UD3DRenderDevice* InRenDev,QWORD InCacheId);
	virtual ~FD3DResource();

	// GetTexture
	virtual class FD3DTexture* GetTexture() { return NULL; }

	// GetVertexStream
	virtual class FD3DVertexStream* GetVertexStream() { return NULL; }

	// GetIndexBuffer
	virtual class FD3DIndexBuffer* GetIndexBuffer() { return NULL; }

	// CalculateFootprint
	virtual INT CalculateFootprint() { return 0; }
};

//
//	FD3DTexture
//
class FD3DTexture : public FD3DResource
{
public:

	IDirect3DSurface8*		RenderTargetSurface;
	IDirect3DSurface8*		DepthStencilSurface;

	IDirect3DTexture8*		Direct3DTexture8;
	IDirect3DCubeTexture8*	Direct3DCubeTexture8;
	INT						CachedWidth,
							CachedHeight,
							CachedFirstMip,
							CachedNumMips;
	D3DFORMAT				CachedFormat;

	TArray<INT>				CachedChildRevision;

	// Constructor/destructor.
	FD3DTexture(UD3DRenderDevice* InRenDev,QWORD InCacheId);
	virtual ~FD3DTexture();

	// GetTexture
	virtual FD3DTexture* GetTexture() { return this; }

	// CalculateFootprint
	virtual INT CalculateFootprint();

	// Cache - Ensure that the cached texture is up to date.
	UBOOL Cache(FBaseTexture* SourceTexture);
};

//
//	FStreamDeclaration
//
struct FStreamDeclaration
{
	FVertexComponent	Components[MAX_VERTEX_COMPONENTS];
	INT					NumComponents;

	FStreamDeclaration()
	{
		NumComponents = 0;
	}

	FStreamDeclaration(FVertexStream* VertexStream)
	{
		NumComponents = VertexStream->GetComponents(Components);
	}

	UBOOL operator==(FStreamDeclaration& Other)
	{
		if(NumComponents != Other.NumComponents)
			return 0;

		for(INT ComponentIndex = 0;ComponentIndex < NumComponents;ComponentIndex++)
		{
			if(Components[ComponentIndex].Type != Other.Components[ComponentIndex].Type ||
				Components[ComponentIndex].Function != Other.Components[ComponentIndex].Function)
				return 0;
		}

		return 1;
	}
};

//
//	FD3DVertexStream
//
class FD3DVertexStream : public FD3DResource
{
public:

	IDirect3DVertexBuffer8*	Direct3DVertexBuffer8;
	INT						CachedSize;

	// Constructor/destructor.
	FD3DVertexStream(UD3DRenderDevice* InRenDev,QWORD InCacheId);
	virtual ~FD3DVertexStream();

	// GetVertexStream
	virtual FD3DVertexStream* GetVertexStream() { return this; }

	// CalculateFootprint
	virtual INT CalculateFootprint();

	// Cache - Ensure that the cached vertex stream is up to date.
	void Cache(FVertexStream* SourceStream);
};

//
//	FD3DDynamicVertexStream
//
class FD3DDynamicVertexStream : public FD3DVertexStream
{
public:

	INT	Tail;

	// Constructor/destructor.
	FD3DDynamicVertexStream(UD3DRenderDevice* InRenDev);

	// Reallocate - Reallocates the dynamic vertex buffer.
	void Reallocate(INT NewSize);

	// AddVertices - Adds the vertices from the given stream to the end of the stream.  
	// Returns the index of the first vertex in the stream.
	INT AddVertices(FVertexStream* Vertices);
};

//
//	FShaderDeclaration
//
struct FShaderDeclaration
{
	FStreamDeclaration	Streams[16];
	INT					NumStreams;

	FShaderDeclaration()
	{
		NumStreams = 0;
	}

	UBOOL operator==(FShaderDeclaration& Other)
	{
		if(NumStreams != Other.NumStreams)
			return 0;

		for(INT StreamIndex = 0;StreamIndex < NumStreams;StreamIndex++)
			if(!(Streams[StreamIndex] == Other.Streams[StreamIndex]))
				return 0;

		return 1;
	}
};

//
//	FD3DVertexShader
//
class FD3DVertexShader
{
public:

	EVertexShader		Type;
	FShaderDeclaration	Declaration;
	TArray<DWORD>		D3DDeclaration;

	DWORD				Handle;

	UD3DRenderDevice*	RenDev;
	FD3DVertexShader*	NextVertexShader;

	// Constructor/destructor.
	FD3DVertexShader(UD3DRenderDevice* InRenDev,FShaderDeclaration& InDeclaration);
	~FD3DVertexShader();
};

//
//	FD3DFixedVertexShader
//
class FD3DFixedVertexShader : public FD3DVertexShader
{
public:

	// Constructor/destructor.
	FD3DFixedVertexShader(UD3DRenderDevice* InRenDev,FShaderDeclaration& InDeclaration);
};

//
//	FD3DIndexBuffer
//
class FD3DIndexBuffer : public FD3DResource
{
public:

	IDirect3DIndexBuffer8*	Direct3DIndexBuffer8;
	INT						CachedSize;

	// Constructor/destructor.
	FD3DIndexBuffer(UD3DRenderDevice* InRenDev,QWORD InCacheId);
	~FD3DIndexBuffer();

	// GetIndexBuffer
	virtual FD3DIndexBuffer* GetIndexBuffer() { return this; }

	// CalculateFootprint
	virtual INT CalculateFootprint();

	// Cache - Ensure the cached index buffer is up to date.
	void Cache(FIndexBuffer* SourceIndexBuffer);
};

//
//	FD3DDynamicIndexBuffer
//
class FD3DDynamicIndexBuffer : public FD3DIndexBuffer
{
public:

	INT	Tail;
	INT IndexSize;

	// Constructor/destructor.
	FD3DDynamicIndexBuffer(UD3DRenderDevice* InRenDev, INT InIndexSize);

	// Reallocate - Reallocates the dynamic index buffer.
	void Reallocate(INT NewSize);

	// AddIndices - Adds the indices from the given buffer to the end of this buffer.  Returns the old tail.
	INT AddIndices(FIndexBuffer* Indices);
};


enum EPixelShader
{
	PS_None					=0,
	PS_Terrain3Layer		=1,
	PS_Terrain4Layer		=2,
};


//
//	FD3DPixelShader
//
class FD3DPixelShader
{
public:
	ANSICHAR*			Source;
	DWORD				Handle;
	EPixelShader		Type;

	UD3DRenderDevice*	RenDev;
	FD3DPixelShader*	NextPixelShader;

	// Constructor/destructor.
	FD3DPixelShader(UD3DRenderDevice* InRenDev, EPixelShader InType, ANSICHAR* InSource);
	~FD3DPixelShader();
};


#endif
