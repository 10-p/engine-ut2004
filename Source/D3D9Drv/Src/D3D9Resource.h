/*=============================================================================
	D3DResource.h: Unreal Direct3D resource definition.
	Copyright 2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#ifndef HEADER_D3D9RESOURCE
#define HEADER_D3D9RESOURCE

//
//	FD3DResource
//
class FD3D9Resource
{
public:

	UD3D9RenderDevice*	RenDev;
	QWORD				CacheId;
	INT					CachedRevision,
						HashIndex;

	FD3D9Resource*		NextResource;
	FD3D9Resource*		HashNext;

	INT					LastFrameUsed;

	// Constructor/destructor.
	FD3D9Resource(UD3D9RenderDevice* InRenDev,QWORD InCacheId);
	virtual ~FD3D9Resource();

	// GetTexture
	virtual class FD3D9Texture* GetTexture() { return NULL; }

	// GetVertexStream
	virtual class FD3D9VertexStream* GetVertexStream() { return NULL; }

	// GetIndexBuffer
	virtual class FD3D9IndexBuffer* GetIndexBuffer() { return NULL; }

	// CalculateFootprint
	virtual INT CalculateFootprint() { return 0; }
};

//
//	FD3DTexture
//
class FD3D9Texture : public FD3D9Resource
{
public:

	IDirect3DSurface9*		RenderTargetSurface;
	IDirect3DSurface9*		DepthStencilSurface;

	IDirect3DTexture9*		Direct3DTexture9;
	IDirect3DCubeTexture9*	Direct3DCubeTexture9;
	INT						CachedWidth,
							CachedHeight,
							CachedFirstMip,
							CachedNumMips;
	D3DFORMAT				CachedFormat;

	TArray<INT>				CachedChildRevision;

	// Constructor/destructor.
	FD3D9Texture(UD3D9RenderDevice* InRenDev,QWORD InCacheId);
	virtual ~FD3D9Texture();

	// GetTexture
	virtual FD3D9Texture* GetTexture() { return this; }

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
class FD3D9VertexStream : public FD3D9Resource
{
public:

	IDirect3DVertexBuffer9*	Direct3DVertexBuffer9;
	INT						CachedSize;

	// Constructor/destructor.
	FD3D9VertexStream(UD3D9RenderDevice* InRenDev,QWORD InCacheId);
	virtual ~FD3D9VertexStream();

	// GetVertexStream
	virtual FD3D9VertexStream* GetVertexStream() { return this; }

	// CalculateFootprint
	virtual INT CalculateFootprint();

	// Cache - Ensure that the cached vertex stream is up to date.
	void Cache(FVertexStream* SourceStream);
};

//
//	FD3DDynamicVertexStream
//
class FD3D9DynamicVertexStream : public FD3D9VertexStream
{
public:

	INT	Tail;

	// Constructor/destructor.
	FD3D9DynamicVertexStream(UD3D9RenderDevice* InRenDev);

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
//	FD3D9VertexShader
//
class FD3D9VertexShader
{
public:

	EVertexShader		Type;
	FShaderDeclaration	Declaration;
	//TArray<DWORD>		D3DDeclaration;

	IDirect3DVertexShader9*	     Shader;
	IDirect3DVertexDeclaration9* Decl;

	UD3D9RenderDevice*	RenDev;
	FD3D9VertexShader*	NextVertexShader;

	// Constructor/destructor.
	FD3D9VertexShader(UD3D9RenderDevice* InRenDev,FShaderDeclaration& InDeclaration);
	~FD3D9VertexShader();
};

//
//	FD3DFixedVertexShader
//
class FD3D9FixedVertexShader : public FD3D9VertexShader
{
public:

	// Constructor/destructor.
	FD3D9FixedVertexShader(UD3D9RenderDevice* InRenDev,FShaderDeclaration& InDeclaration);
};

//
//	FD3DIndexBuffer
//
class FD3D9IndexBuffer : public FD3D9Resource
{
public:

	IDirect3DIndexBuffer9*	Direct3DIndexBuffer9;
	INT						CachedSize;

	// Constructor/destructor.
	FD3D9IndexBuffer(UD3D9RenderDevice* InRenDev,QWORD InCacheId);
	~FD3D9IndexBuffer();

	// GetIndexBuffer
	virtual FD3D9IndexBuffer* GetIndexBuffer() { return this; }

	// CalculateFootprint
	virtual INT CalculateFootprint();

	// Cache - Ensure the cached index buffer is up to date.
	void Cache(FIndexBuffer* SourceIndexBuffer);
};

//
//	FD3DDynamicIndexBuffer
//
class FD3D9DynamicIndexBuffer : public FD3D9IndexBuffer
{
public:

	INT	Tail;
	INT IndexSize;

	// Constructor/destructor.
	FD3D9DynamicIndexBuffer(UD3D9RenderDevice* InRenDev, INT InIndexSize);

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
class FD3D9PixelShader
{
public:
	ANSICHAR*	    	    Source;
	IDirect3DPixelShader9*	Shader;
	EPixelShader		    Type;

	UD3D9RenderDevice*	RenDev;
	FD3D9PixelShader*	NextPixelShader;

	// Constructor/destructor.
	FD3D9PixelShader(UD3D9RenderDevice* InRenDev, EPixelShader InType, ANSICHAR* InSource);
	~FD3D9PixelShader();
};


#endif
