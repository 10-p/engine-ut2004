/*=============================================================================
	GL4ESResource.h: GL4ESDrv's cached textures and vertex/index streams (from OpenGLDrv's OpenGLResource.h).
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#ifndef HEADER_GL4ESRESOURCE
#define HEADER_GL4ESRESOURCE

class UGL4ESRenderDevice;

#define GL4ES_INITIAL_DYNAMIC_VERTEXBUFFER_SIZE	65536	// Initial size of dynamic vertex buffers, in bytes.
#define GL4ES_INITIAL_DYNAMIC_INDEXBUFFER_SIZE	32768	// Initial size of dynamic index buffers, in bytes.


//
//	FGL4ESResource
//
class FGL4ESResource
{
public:

	UGL4ESRenderDevice*	RenDev;
	QWORD					CacheId;
	INT						CachedRevision,
							HashIndex;

	FGL4ESResource*		NextResource;
	FGL4ESResource*		HashNext;

	INT						LastFrameUsed;

	// Constructor/destructor.
	FGL4ESResource(UGL4ESRenderDevice* InRenDev,QWORD InCacheId);
	virtual ~FGL4ESResource();

	// GetTexture
	virtual class FGL4ESTexture* GetTexture() { return NULL; }

	// GetVertexStream
	virtual class FGL4ESVertexStream* GetVertexStream() { return NULL; }

	// GetIndexBuffer
	virtual class FGL4ESIndexBuffer* GetIndexBuffer() { return NULL; }

	// CalculateFootprint
	virtual INT CalculateFootprint() { return 0; }
};


//
//	FGL4ESTexture
//
class FGL4ESTexture : public FGL4ESResource
{
public:

	GLuint					RenderTargetSurface;
	GLuint					DepthStencilSurface;

	GLuint					TextureID;
	UBOOL					IsCubemap,
							WasCubemap;

	INT						CachedWidth,
							CachedHeight,
							CachedFirstMip,
							CachedNumMips;
	ETextureFormat			CachedFormat;

	TArray<INT>				CachedChildRevision;

	// Constructor/destructor.
	FGL4ESTexture(UGL4ESRenderDevice* InRenDev,QWORD InCacheId);
	virtual ~FGL4ESTexture();

	// GetTexture
	virtual FGL4ESTexture* GetTexture() { return this; }

	// CalculateFootprint
	virtual INT CalculateFootprint();

	// Cache - Ensure that the cached texture is up to date.
	void Cache(FBaseTexture* SourceTexture);
};


//
//	FGL4ESStreamDeclaration
//
struct FGL4ESStreamDeclaration
{
	FVertexComponent	Components[MAX_VERTEX_COMPONENTS];
	INT					NumComponents;

	FGL4ESStreamDeclaration()
	{
		NumComponents = 0;
	}

	FGL4ESStreamDeclaration(FVertexStream* VertexStream)
	{
		NumComponents = VertexStream->GetComponents(Components);
	}

	UBOOL operator==(FGL4ESStreamDeclaration& Other)
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
//	FGL4ESVertexStream
//
class FGL4ESVertexStream : public FGL4ESResource
{
public:
	// Constructor/destructor.
	FGL4ESVertexStream(UGL4ESRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB);

	// GetVertexStream
	virtual FGL4ESVertexStream* GetVertexStream() { return this; }

	// CalculateFootprint
	virtual INT CalculateFootprint() { return CachedSize; }

	// Cache - Ensure that the cached vertex stream is up to date.
	virtual void Cache(FVertexStream* SourceStream)=0;

	// GetVertexPointer
	virtual void* GetVertexData()=0;

	// IsVAR - VAR extension requires certain mojo.
	virtual GLuint IsVAR() { return 0; }

	// rolling dynamic vertex buffer

	// Reallocate - Reallocates the dynamic vertex buffer.
	virtual void Reallocate(INT NewSize)=0;

	// AddVertices - Adds the vertices from the given stream to the end of the stream.  
	// Returns the index of the first vertex in the stream.
	virtual INT AddVertices(FVertexStream* Vertices)=0;

protected:
	INT		CachedSize;
	INT		Tail;
	UBOOL	IsDynamicVB;
};

//
//	FGL4ESVertexStreamARB_VBO
//
class FGL4ESVertexStreamARB_VBO : public FGL4ESVertexStream
{
public:
	// Constructor/destructor.
	FGL4ESVertexStreamARB_VBO(UGL4ESRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB)
		: FGL4ESVertexStream(InRenDev,InCacheId,InIsDynamicVB), Initialized(0), Buffer(0)
	{
		if( IsDynamicVB )
			Reallocate( GL4ES_INITIAL_DYNAMIC_VERTEXBUFFER_SIZE );
	}

	virtual ~FGL4ESVertexStreamARB_VBO();

	// GetVertexPointer
	virtual void* GetVertexData() { return (void*) ((PTRINT) Buffer); }

	// Cache - Ensure that the cached vertex stream is up to date.
	virtual void Cache(FVertexStream* SourceStream);

	// Reallocate - Reallocates the dynamic vertex buffer.
	virtual void Reallocate(INT NewSize);

	// AddVertices - Adds the vertices from the given stream to the end of the stream.  
	// Returns the index of the first vertex in the stream.
	virtual INT AddVertices(FVertexStream* Vertices);

protected:
	GLuint	Buffer;
	UBOOL	Initialized;
};

//
//	FGL4ESVertexStreamARB_VA
//
class FGL4ESVertexStreamARB_VA : public FGL4ESVertexStream
{
public:
	// Constructor/destructor.
	FGL4ESVertexStreamARB_VA(UGL4ESRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB)
		: FGL4ESVertexStream(InRenDev,InCacheId,InIsDynamicVB)
	{
		if( IsDynamicVB )
			Reallocate( GL4ES_INITIAL_DYNAMIC_VERTEXBUFFER_SIZE );
	}

	// GetVertexPointer
	virtual void* GetVertexData() { return &VertexData(0); }

	// Cache - Ensure that the cached vertex stream is up to date.
	virtual void Cache(FVertexStream* SourceStream);

	// Reallocate - Reallocates the dynamic vertex buffer.
	virtual void Reallocate(INT NewSize);

	// AddVertices - Adds the vertices from the given stream to the end of the stream.  
	// Returns the index of the first vertex in the stream.
	virtual INT AddVertices(FVertexStream* Vertices);

protected:
	TArray<BYTE>			VertexData;
};

#define GL4ES_DYNAMIC_VERTEXBUFFER_VARSIZE (1024 * 1024)

//
//	FGL4ESVertexStreamNV_VAR
//
class FGL4ESVertexStreamNV_VAR : public FGL4ESVertexStream
{
public:
	// Constructor/destructor.
	FGL4ESVertexStreamNV_VAR(UGL4ESRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB)
		: FGL4ESVertexStream(InRenDev,InCacheId,InIsDynamicVB)
	{
		if( IsDynamicVB )
			Reallocate( GL4ES_DYNAMIC_VERTEXBUFFER_VARSIZE );
	}

	// GetVertexPointer
	virtual void* GetVertexData() { return VertexData; }

	// Cache - Ensure that the cached vertex stream is up to date.
	virtual void Cache(FVertexStream* SourceStream);

	// IsVAR - VAR requires certain mojo.
	virtual GLuint IsVAR() { return 1; }

	// Reallocate - Reallocates the dynamic vertex buffer.
	virtual void Reallocate(INT NewSize);

	// AddVertices - Adds the vertices from the given stream to the end of the stream.  
	// Returns the index of the first vertex in the stream.
	virtual INT AddVertices(FVertexStream* Vertices);

protected:
	BYTE*	VertexData;
};


//
//	FGL4ESVertexStreamATI
//
class FGL4ESVertexStreamATI_VAO : public FGL4ESVertexStream
{
public:
	// Constructor/destructor.
	FGL4ESVertexStreamATI_VAO(UGL4ESRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB)
		: FGL4ESVertexStream(InRenDev,InCacheId,InIsDynamicVB), Initialized(0), Buffer(0)
	{
		if( IsDynamicVB )
			Reallocate( GL4ES_INITIAL_DYNAMIC_VERTEXBUFFER_SIZE );
	}

	virtual ~FGL4ESVertexStreamATI_VAO();

	// GetVertexPointer
	virtual void* GetVertexData() { return (void*) ((PTRINT) Buffer); }

	// Cache - Ensure that the cached vertex stream is up to date.
	virtual void Cache(FVertexStream* SourceStream);

	// Reallocate - Reallocates the dynamic vertex buffer.
	virtual void Reallocate(INT NewSize);

	// AddVertices - Adds the vertices from the given stream to the end of the stream.  
	// Returns the index of the first vertex in the stream.
	virtual INT AddVertices(FVertexStream* Vertices);

protected:
	GLuint	Buffer;
	UBOOL	Initialized;
};


//
//	FGL4ESVertexStreamAPPLE_VAO
//
class FGL4ESVertexStreamAPPLE_VAO : public FGL4ESVertexStream
{
public:
	// Constructor/destructor.
	FGL4ESVertexStreamAPPLE_VAO(UGL4ESRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB)
		: FGL4ESVertexStream(InRenDev,InCacheId,InIsDynamicVB), Initialized(0), VertexData(NULL), Buffer(0)
	{
		if( IsDynamicVB )
			Reallocate( GL4ES_INITIAL_DYNAMIC_VERTEXBUFFER_SIZE );
	}

	virtual ~FGL4ESVertexStreamAPPLE_VAO();

	// IsVAR - VAR requires certain mojo.
	virtual GLuint IsVAR() { return Buffer; }

	// GetVertexPointer
	virtual void* GetVertexData() { return VertexData; }

	// Cache - Ensure that the cached vertex stream is up to date.
	virtual void Cache(FVertexStream* SourceStream);

	// Reallocate - Reallocates the dynamic vertex buffer.
	virtual void Reallocate(INT NewSize);

	// AddVertices - Adds the vertices from the given stream to the end of the stream.  
	// Returns the index of the first vertex in the stream.
	virtual INT AddVertices(FVertexStream* Vertices);

protected:
	GLuint	Buffer;
	BYTE*	VertexData;
	UBOOL	Initialized;
};


//
//	FGL4ESShaderDeclaration
//
struct FGL4ESShaderDeclaration
{
	FGL4ESStreamDeclaration	Streams[16];
	INT					NumStreams;

	FGL4ESShaderDeclaration()
	{
		NumStreams = 0;
	}

	UBOOL operator==(FGL4ESShaderDeclaration& Other)
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
//	FGL4ESVertexShader
//
class FGL4ESVertexShader
{
public:

	EVertexShader			Type;
	FGL4ESShaderDeclaration		Declaration;
	TArray<DWORD>			OpenGLDeclaration;

	DWORD					Handle;

	UGL4ESRenderDevice*	RenDev;
	FGL4ESVertexShader*	NextVertexShader;

	// Constructor/destructor.
	FGL4ESVertexShader(UGL4ESRenderDevice* InRenDev,FGL4ESShaderDeclaration& InDeclaration);
	~FGL4ESVertexShader();
};


//
//	FGL4ESFixedVertexShader
//
class FGL4ESFixedVertexShader : public FGL4ESVertexShader
{
public:

	// Constructor/destructor.
	FGL4ESFixedVertexShader(UGL4ESRenderDevice* InRenDev,FGL4ESShaderDeclaration& InDeclaration);
};


//
//	FGL4ESIndexBuffer
//
class FGL4ESIndexBuffer : public FGL4ESResource
{
public:

	// Constructor/destructor.
	FGL4ESIndexBuffer(UGL4ESRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicIB);

	// GetIndexBuffer
	virtual FGL4ESIndexBuffer* GetIndexBuffer() { return this; }

	// GetIndexData
	virtual void* GetIndexData()=0;

	// CalculateFootprint
	virtual INT CalculateFootprint() { return CachedSize; }

	// Cache - Ensure the cached index buffer is up to date.
	virtual void Cache(FIndexBuffer* SourceIndexBuffer)=0;

	// Reallocate - Reallocates the dynamic index buffer.
	virtual void Reallocate(INT NewSize)=0;

	// AddIndices - Adds the indices from the given buffer to the end of this buffer.  Returns the old tail.
	virtual INT AddIndices(FIndexBuffer* Indices)=0;

protected:
	INT		CachedSize;
	INT		Tail;
	UBOOL	IsDynamicIB;
};



//
//	FGL4ESIndexBufferARB
//
class FGL4ESIndexBufferARB : public FGL4ESIndexBuffer
{
public:
	// Constructor/destructor.
	FGL4ESIndexBufferARB(UGL4ESRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicIB)
		: FGL4ESIndexBuffer(InRenDev,InCacheId,InIsDynamicIB)
	{
		if( IsDynamicIB )
			Reallocate( GL4ES_INITIAL_DYNAMIC_INDEXBUFFER_SIZE );
	}

	// GetIndexData
	virtual void* GetIndexData() { return &IndexData(0); }
	
	// Cache - Ensure the cached index buffer is up to date.
	virtual void Cache(FIndexBuffer* SourceIndexBuffer);

	// Reallocate - Reallocates the dynamic index buffer.
	virtual void Reallocate(INT NewSize);

	// AddIndices - Adds the indices from the given buffer to the end of this buffer.  Returns the old tail.
	virtual INT AddIndices(FIndexBuffer* Indices);

protected:
	TArray<BYTE> IndexData;
};


//
//	FGL4ESIndexBufferATI
//
class FGL4ESIndexBufferATI : public FGL4ESIndexBuffer
{
public:
	// Constructor/destructor.
	FGL4ESIndexBufferATI(UGL4ESRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicIB)
		: FGL4ESIndexBuffer(InRenDev,InCacheId,InIsDynamicIB),Initialized(0)
	{
		if( IsDynamicIB )
			Reallocate( GL4ES_INITIAL_DYNAMIC_INDEXBUFFER_SIZE );
	}

	virtual ~FGL4ESIndexBufferATI();

	// GetIndexData
	virtual void* GetIndexData() { return (void*) ((PTRINT) Buffer); }
	
	// Cache - Ensure the cached index buffer is up to date.
	virtual void Cache(FIndexBuffer* SourceIndexBuffer);

	// Reallocate - Reallocates the dynamic index buffer.
	virtual void Reallocate(INT NewSize);

	// AddIndices - Adds the indices from the given buffer to the end of this buffer.  Returns the old tail.
	virtual INT AddIndices(FIndexBuffer* Indices);

protected:
	
	GLuint	Buffer;
	UBOOL	Initialized;
};


//
//	FGL4ESIndexBufferARB_VBO
//
class FGL4ESIndexBufferARB_VBO : public FGL4ESIndexBuffer
{
public:
	// Constructor/destructor.
	FGL4ESIndexBufferARB_VBO(UGL4ESRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicIB)
		: FGL4ESIndexBuffer(InRenDev,InCacheId,InIsDynamicIB),Initialized(0)
	{
		if( IsDynamicIB )
			Reallocate( GL4ES_INITIAL_DYNAMIC_INDEXBUFFER_SIZE );
	}

	virtual ~FGL4ESIndexBufferARB_VBO();

	// GetIndexData
	virtual void* GetIndexData() { return (void*) ((PTRINT) Buffer); }
	
	// Cache - Ensure the cached index buffer is up to date.
	virtual void Cache(FIndexBuffer* SourceIndexBuffer);

	// Reallocate - Reallocates the dynamic index buffer.
	virtual void Reallocate(INT NewSize);

	// AddIndices - Adds the indices from the given buffer to the end of this buffer.  Returns the old tail.
	virtual INT AddIndices(FIndexBuffer* Indices);

protected:
	
	GLuint	Buffer;
	UBOOL	Initialized;
};

//
// DE's additions to the renderer
//

//
//	FGL4ESQuadIndexBuffer
//
class FGL4ESQuadIndexBuffer : public FIndexBuffer
{
public:
	QWORD			CacheId;
	enum { MAX_QUADS = 5461 };
    int             MaxVertIndex;

    // Constructor.
	FGL4ESQuadIndexBuffer()
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


//
//	FGL4ESDynVertStream
//
class FGL4ESDynVertStream : public FVertexStream
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

	FGL4ESDynVertStream()
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
//	FGL4ESMaterialStateStage
//
struct FGL4ESMaterialStateStage
{
	FGL4ESTexture*		Texture;
	DWORD				TextureAddressU,
						TextureAddressV,
						TextureAddressW;
    FLOAT               TextureMipLODBias; // sjs
	DWORD		 		ColorOp,
						AlphaOp;
	DWORD				ColorArg0,
						ColorArg1,
						ColorArg2,
						AlphaArg0,
						AlphaArg1,
						AlphaArg2,
						ResultArg;
	DWORD				ColorMod0,
						ColorMod1,
						ColorMod2,
						AlphaMod0,
						AlphaMod1,
						AlphaMod2;

	UBOOL				UseNVCombine4;

	FLOAT				ColorScale,
						AlphaScale;

	DWORD				TexCoordIndex;
	DWORD				TexCoordCount;
	UBOOL				TextureTransformsEnabled;
	FMatrix				TextureTransformMatrix;

	DWORD				TexGenMode;
	UBOOL				TexGenProjected;
	UBOOL				UseTexGenMatrix;

	FGL4ESMaterialStateStage();
};

enum EGL4ESPixelShader
{
	GL4ES_PS_None					=0,
	GL4ES_PS_Terrain3Layer		=1,
	GL4ES_PS_Terrain4Layer		=2,
};


//
//	FGL4ESPixelShader
//
class FGL4ESPixelShader
{
public:
	ANSICHAR*			Source;
	GLuint				Handle;
	EGL4ESPixelShader		Type;

	UGL4ESRenderDevice*	RenDev;
	FGL4ESPixelShader*	NextPixelShader;

	// Constructor/destructor.
	FGL4ESPixelShader(UGL4ESRenderDevice* InRenDev, EGL4ESPixelShader InType, ANSICHAR* InSource);
	~FGL4ESPixelShader();
};

#endif
