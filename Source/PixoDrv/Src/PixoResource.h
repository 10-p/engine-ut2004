/*=============================================================================
	PixoResource.h: Unreal Pixo resource definition.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Michael Sartain from GL driver
		* Taken over by Daniel Vogel

=============================================================================*/

#ifndef HEADER_PIXORESOURCE
#define HEADER_PIXORESOURCE

class UPixoRenderDevice;

#define INITIAL_DYNAMIC_VERTEXBUFFER_SIZE   65536   // Initial size of dynamic vertex buffers, in bytes.
#define INITIAL_DYNAMIC_INDEXBUFFER_SIZE    32768   // Initial size of dynamic index buffers, in bytes.


//
//  FPixoResource
//
class FPixoResource
{
public:

	UPixoRenderDevice       *RenDev;
	QWORD                   CacheId;
	INT                     CachedRevision,
							HashIndex;

	FPixoResource           *NextResource;
	FPixoResource           *HashNext;

	INT                     LastFrameUsed;

	// Constructor/destructor.
	FPixoResource(UPixoRenderDevice* InRenDev,QWORD InCacheId);
	virtual ~FPixoResource();

	// GetTexture
	virtual class FPixoTexture* GetTexture() { return NULL; }

	// GetVertexStream
	virtual class FPixoVertexStream* GetVertexStream() { return NULL; }

	// GetIndexBuffer
	virtual class FPixoIndexBuffer* GetIndexBuffer() { return NULL; }

	// CalculateFootprint
	virtual INT CalculateFootprint() { return 0; }
};


//
//  FPixoTexture
//
class FPixoTexture : public FPixoResource
{
public:

	void*					FrameBuffer;
	void*					ZBuffer;
	INT						FrameBufferPitch,
							ZBufferPitch;

	UBOOL                   WasCubemap;

	INT                     CachedWidth,
							CachedHeight,
							CachedFirstMip,
							CachedNumMips;
	ETextureFormat          CachedFormat;

	TArray<INT>             CachedChildRevision;

	FColor                  *Palette;
	BYTE                    *Texels[12];
	INT                     MaxMipLevel;

	// Constructor/destructor.
	FPixoTexture(UPixoRenderDevice* InRenDev,QWORD InCacheId);
	virtual ~FPixoTexture();

	// GetTexture
	virtual FPixoTexture* GetTexture() { return this; }

	// CalculateFootprint
	virtual INT CalculateFootprint();

	// Cache - Ensure that the cached texture is up to date.
	void Cache(FBaseTexture* SourceTexture);

	// UnCache - free Texels, etc.
	void UnCache();
};


//
//  FStreamDeclaration
//
struct FStreamDeclaration
{
	FVertexComponent    Components[MAX_VERTEX_COMPONENTS];
	INT                 NumComponents;

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
// Solving the problem of multiple resource implementations purely with OO 
// principles got really nasty so I decided to add "IsDynamicVB" which tells
// the class that it should behave like the dynamic vertex stream. It is a 
// hack but you should've seen the OO solution ;)
//

//
//  FPixoVertexStream
//
class FPixoVertexStream : public FPixoResource
{
public:
	// Constructor/destructor.
	FPixoVertexStream(UPixoRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB);

	// GetVertexStream
	virtual FPixoVertexStream* GetVertexStream() { return this; }

	// CalculateFootprint
	virtual INT CalculateFootprint() { return CachedSize; }

	// Cache - Ensure that the cached vertex stream is up to date.
    void Cache(FVertexStream* SourceStream);

	// GetVertexPointer
    void* GetVertexData() { return &VertexData(0); }

	// rolling dynamic vertex buffer

	// Reallocate - Reallocates the dynamic vertex buffer.
    void Reallocate(INT NewSize);

	// AddVertices - Adds the vertices from the given stream to the end of the stream.  
	// Returns the index of the first vertex in the stream.
    INT AddVertices(FVertexStream* Vertices);

protected:
	INT     CachedSize;
	INT     Tail;
	UBOOL   IsDynamicVB;
	TArray<BYTE>            VertexData;
};

//
//  FShaderDeclaration
//
struct FShaderDeclaration
{
	FStreamDeclaration  Streams[16];
	INT                 NumStreams;

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
//  FPixoVertexShader
//
class FPixoVertexShader
{
public:

	EVertexShader           Type;
	FShaderDeclaration      Declaration;
	TArray<DWORD>           PixoDeclaration;

	DWORD                   Handle;

	UPixoRenderDevice*  RenDev;
	FPixoVertexShader*  NextVertexShader;

	// Constructor/destructor.
	FPixoVertexShader(UPixoRenderDevice* InRenDev,FShaderDeclaration& InDeclaration);
	~FPixoVertexShader();
};


//
//  FPixoFixedVertexShader
//
class FPixoFixedVertexShader : public FPixoVertexShader
{
public:

	// Constructor/destructor.
	FPixoFixedVertexShader(UPixoRenderDevice* InRenDev,FShaderDeclaration& InDeclaration);
};

//
//  FPixoIndexBuffer
//
class FPixoIndexBuffer : public FPixoResource
{
public:

	// Constructor/destructor.
	FPixoIndexBuffer(UPixoRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicIB);

	// GetIndexBuffer
	virtual FPixoIndexBuffer* GetIndexBuffer() { return this; }

	// GetIndexData
    void* GetIndexData() { return &IndexData(0); }
    INT GetIndexCount() { return IndexData.Num() >> 1; }

	// CalculateFootprint
	virtual INT CalculateFootprint() { return CachedSize; }

	// Cache - Ensure the cached index buffer is up to date.
    void Cache(FIndexBuffer* SourceIndexBuffer);

	// Reallocate - Reallocates the dynamic index buffer.
    void Reallocate(INT NewSize);

	// AddIndices - Adds the indices from the given buffer to the end of this buffer.  Returns the old tail.
    INT AddIndices(FIndexBuffer* Indices);

    UBOOL IsDynamic() { return IsDynamicIB; }

protected:
	INT     CachedSize;
	INT     Tail;
	UBOOL   IsDynamicIB;
	TArray<BYTE> IndexData;
};

//
// DE's additions to the renderer
//

//
//  FDynVertStream
//
class FDynVertStream : public FVertexStream
{
public:
	QWORD           CacheId;
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

	FDynVertStream()
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


//
//  FPixoMaterialStateStage
//
struct FPixoMaterialStateStage
{
	FPixoTexture*       Texture;
	DWORD               TextureAddressU,
						TextureAddressV,
						TextureAddressW;
	FLOAT               TextureMipLODBias; // sjs
	DWORD               ColorOp,
						AlphaOp;
	DWORD               SourceRgb0,
						SourceRgb1,
						SourceRgb2,
						SourceAlpha0,
						SourceAlpha1,
						SourceAlpha2;
	DWORD               OperandRgb0,
						OperandRgb2,
						OperandAlpha0,
						OperandAlpha2;

	FLOAT               ColorScale,
						AlphaScale;

	DWORD               TexCoordIndex;
	DWORD               TexCoordCount;
	UBOOL               TextureTransformsEnabled;
	FMatrix             TextureTransformMatrix;

	DWORD               TexGenMode;
	UBOOL               TexGenProjected;
	UBOOL               UseTexGenMatrix;

	FPixoMaterialStateStage();
};


#endif
