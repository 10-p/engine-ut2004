/*=============================================================================
	OpenGLResource.h: Unreal OpenGL resource definition.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#ifndef HEADER_OPENGLRESOURCE
#define HEADER_OPENGLRESOURCE

class UOpenGLRenderDevice;

#define INITIAL_DYNAMIC_VERTEXBUFFER_SIZE	65536	// Initial size of dynamic vertex buffers, in bytes.
#define INITIAL_DYNAMIC_INDEXBUFFER_SIZE	32768	// Initial size of dynamic index buffers, in bytes.


//
//	FOpenGLResource
//
class FOpenGLResource
{
public:

	UOpenGLRenderDevice*	RenDev;
	QWORD					CacheId;
	INT						CachedRevision,
							HashIndex;

	FOpenGLResource*		NextResource;
	FOpenGLResource*		HashNext;

	INT						LastFrameUsed;

	// Constructor/destructor.
	FOpenGLResource(UOpenGLRenderDevice* InRenDev,QWORD InCacheId);
	virtual ~FOpenGLResource();

	// GetTexture
	virtual class FOpenGLTexture* GetTexture() { return NULL; }

	// GetVertexStream
	virtual class FOpenGLVertexStream* GetVertexStream() { return NULL; }

	// GetIndexBuffer
	virtual class FOpenGLIndexBuffer* GetIndexBuffer() { return NULL; }

	// CalculateFootprint
	virtual INT CalculateFootprint() { return 0; }
};


//
//	FOpenGLTexture
//
class FOpenGLTexture : public FOpenGLResource
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
	FOpenGLTexture(UOpenGLRenderDevice* InRenDev,QWORD InCacheId);
	virtual ~FOpenGLTexture();

	// GetTexture
	virtual FOpenGLTexture* GetTexture() { return this; }

	// CalculateFootprint
	virtual INT CalculateFootprint();

	// Cache - Ensure that the cached texture is up to date.
	void Cache(FBaseTexture* SourceTexture);
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
// Solving the problem of multiple resource implementations purely with OO 
// principles got really nasty so I decided to add "IsDynamicVB" which tells
// the class that it should behave like the dynamic vertex stream. It is a 
// hack but you should've seen the OO solution ;)
//

//
//	FOpenGLVertexStream
//
class FOpenGLVertexStream : public FOpenGLResource
{
public:
	// Constructor/destructor.
	FOpenGLVertexStream(UOpenGLRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB);

	// GetVertexStream
	virtual FOpenGLVertexStream* GetVertexStream() { return this; }

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
//	FOpenGLVertexStreamARB_VBO
//
class FOpenGLVertexStreamARB_VBO : public FOpenGLVertexStream
{
public:
	// Constructor/destructor.
	FOpenGLVertexStreamARB_VBO(UOpenGLRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB)
		: FOpenGLVertexStream(InRenDev,InCacheId,InIsDynamicVB), Initialized(0), Buffer(0)
	{
		if( IsDynamicVB )
			Reallocate( INITIAL_DYNAMIC_VERTEXBUFFER_SIZE );
	}

	virtual ~FOpenGLVertexStreamARB_VBO();

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
//	FOpenGLVertexStreamARB_VA
//
class FOpenGLVertexStreamARB_VA : public FOpenGLVertexStream
{
public:
	// Constructor/destructor.
	FOpenGLVertexStreamARB_VA(UOpenGLRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB)
		: FOpenGLVertexStream(InRenDev,InCacheId,InIsDynamicVB)
	{
		if( IsDynamicVB )
			Reallocate( INITIAL_DYNAMIC_VERTEXBUFFER_SIZE );
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

#define DYNAMIC_VERTEXBUFFER_VARSIZE (1024 * 1024)

//
//	FOpenGLVertexStreamNV_VAR
//
class FOpenGLVertexStreamNV_VAR : public FOpenGLVertexStream
{
public:
	// Constructor/destructor.
	FOpenGLVertexStreamNV_VAR(UOpenGLRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB)
		: FOpenGLVertexStream(InRenDev,InCacheId,InIsDynamicVB)
	{
		if( IsDynamicVB )
			Reallocate( DYNAMIC_VERTEXBUFFER_VARSIZE );
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
//	FOpenGLVertexStreamATI
//
class FOpenGLVertexStreamATI_VAO : public FOpenGLVertexStream
{
public:
	// Constructor/destructor.
	FOpenGLVertexStreamATI_VAO(UOpenGLRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB)
		: FOpenGLVertexStream(InRenDev,InCacheId,InIsDynamicVB), Initialized(0), Buffer(0)
	{
		if( IsDynamicVB )
			Reallocate( INITIAL_DYNAMIC_VERTEXBUFFER_SIZE );
	}

	virtual ~FOpenGLVertexStreamATI_VAO();

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
//	FOpenGLVertexStreamAPPLE_VAO
//
class FOpenGLVertexStreamAPPLE_VAO : public FOpenGLVertexStream
{
public:
	// Constructor/destructor.
	FOpenGLVertexStreamAPPLE_VAO(UOpenGLRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicVB)
		: FOpenGLVertexStream(InRenDev,InCacheId,InIsDynamicVB), Initialized(0), VertexData(NULL), Buffer(0)
	{
		if( IsDynamicVB )
			Reallocate( INITIAL_DYNAMIC_VERTEXBUFFER_SIZE );
	}

	virtual ~FOpenGLVertexStreamAPPLE_VAO();

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
//	FOpenGLVertexShader
//
class FOpenGLVertexShader
{
public:

	EVertexShader			Type;
	FShaderDeclaration		Declaration;
	TArray<DWORD>			OpenGLDeclaration;

	DWORD					Handle;

	UOpenGLRenderDevice*	RenDev;
	FOpenGLVertexShader*	NextVertexShader;

	// Constructor/destructor.
	FOpenGLVertexShader(UOpenGLRenderDevice* InRenDev,FShaderDeclaration& InDeclaration);
	~FOpenGLVertexShader();
};


//
//	FOpenGLFixedVertexShader
//
class FOpenGLFixedVertexShader : public FOpenGLVertexShader
{
public:

	// Constructor/destructor.
	FOpenGLFixedVertexShader(UOpenGLRenderDevice* InRenDev,FShaderDeclaration& InDeclaration);
};


//
//	FOpenGLIndexBuffer
//
class FOpenGLIndexBuffer : public FOpenGLResource
{
public:

	// Constructor/destructor.
	FOpenGLIndexBuffer(UOpenGLRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicIB);

	// GetIndexBuffer
	virtual FOpenGLIndexBuffer* GetIndexBuffer() { return this; }

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
//	FOpenGLIndexBufferARB
//
class FOpenGLIndexBufferARB : public FOpenGLIndexBuffer
{
public:
	// Constructor/destructor.
	FOpenGLIndexBufferARB(UOpenGLRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicIB)
		: FOpenGLIndexBuffer(InRenDev,InCacheId,InIsDynamicIB)
	{
		if( IsDynamicIB )
			Reallocate( INITIAL_DYNAMIC_INDEXBUFFER_SIZE );
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
//	FOpenGLIndexBufferATI
//
class FOpenGLIndexBufferATI : public FOpenGLIndexBuffer
{
public:
	// Constructor/destructor.
	FOpenGLIndexBufferATI(UOpenGLRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicIB)
		: FOpenGLIndexBuffer(InRenDev,InCacheId,InIsDynamicIB),Initialized(0)
	{
		if( IsDynamicIB )
			Reallocate( INITIAL_DYNAMIC_INDEXBUFFER_SIZE );
	}

	virtual ~FOpenGLIndexBufferATI();

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
//	FOpenGLIndexBufferARB_VBO
//
class FOpenGLIndexBufferARB_VBO : public FOpenGLIndexBuffer
{
public:
	// Constructor/destructor.
	FOpenGLIndexBufferARB_VBO(UOpenGLRenderDevice* InRenDev,QWORD InCacheId,UBOOL InIsDynamicIB)
		: FOpenGLIndexBuffer(InRenDev,InCacheId,InIsDynamicIB),Initialized(0)
	{
		if( IsDynamicIB )
			Reallocate( INITIAL_DYNAMIC_INDEXBUFFER_SIZE );
	}

	virtual ~FOpenGLIndexBufferARB_VBO();

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
//	FQuadIndexBuffer
//
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
//	FOpenGLDynVertStream
//
class FOpenGLDynVertStream : public FVertexStream
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

	FOpenGLDynVertStream()
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
//	FOpenGLMaterialStateStage
//
struct FOpenGLMaterialStateStage
{
	FOpenGLTexture*		Texture;
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

	FOpenGLMaterialStateStage();
};

enum EPixelShader
{
	PS_None					=0,
	PS_Terrain3Layer		=1,
	PS_Terrain4Layer		=2,
};


//
//	FOpenGLPixelShader
//
class FOpenGLPixelShader
{
public:
	ANSICHAR*			Source;
	GLuint				Handle;
	EPixelShader		Type;

	UOpenGLRenderDevice*	RenDev;
	FOpenGLPixelShader*	NextPixelShader;

	// Constructor/destructor.
	FOpenGLPixelShader(UOpenGLRenderDevice* InRenDev, EPixelShader InType, ANSICHAR* InSource);
	~FOpenGLPixelShader();
};

#endif
