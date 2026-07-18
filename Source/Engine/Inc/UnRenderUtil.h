/*=============================================================================
	UnRenderUtil.h: Rendering utility definitions.
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push,4)
#endif

#include "UnForcePacking_begin.h"

//
//  EClippingFlag
//
enum EClippingFlag
{
	CF_Inside = 0x1,
	CF_Outside = 0x2
};

//
//  FConvexVolume
//
class ENGINE_API FConvexVolume
{
public:

	enum { MAX_VOLUME_PLANES = 32 };

	FPlane	BoundingPlanes[MAX_VOLUME_PLANES];
	INT		NumPlanes;

	// Constructor
	FConvexVolume();

	// SphereCheck - Determines whether a sphere is inside of the volume, outside of the volume, or both.  Returns EClippingFlag.
	BYTE SphereCheck(const FSphere& Sphere);

#if MACOSX // optimized for Altivec by Sanjay Patel at Apple.
	BYTE BoxCheck_altivec(const FVector &Origin,const FVector &Extent);
#endif

	// BoxCheck - Determines whether a box is inside of the volume, outside of the volume, or both.  Returns EClippingFlag.
	BYTE BoxCheck(const FVector& Origin,const FVector& Extent);

	// ClipPolygon
	FPoly ClipPolygon(FPoly Polygon);
};

//
//	FLineVertex
//
class ENGINE_API FLineVertex
{
public:

	FVector	Position;
	FColor	Diffuse;

	// Constructors.
	FLineVertex() {}
	FLineVertex(FVector InPosition,FColor InDiffuse)
	{
		Position = InPosition;
		Diffuse = InDiffuse;
	}
};

//
//	FLineBatcher
//
class ENGINE_API FLineBatcher : public FVertexStream
{
public:

	TArray<FLineVertex>		Vertices;
	QWORD					CacheId;
	UBOOL					ZTest;
	FRenderInterface*		RI;

	// Constructor/destructor.

	FLineBatcher(FRenderInterface* InRI, UBOOL InZTest=1);
	~FLineBatcher();

	// Flush - Renders all buffered lines.
	void Flush( DWORD PolyFlags=0 );

	// DrawLine - Buffers a line for rendering.
	void DrawLine(FVector P1,FVector P2,FColor Color);

	// DrawPoint - Buffers a point for rendering.  Renders the point as a square of connected lines.
	void DrawPoint(class FSceneNode* SceneNode,FVector P,FColor Color);

	// DrawBox - Buffers a wireframe box for rendering.
	void DrawBox(FBox Box,FColor Color);

	// DrawCircle - Buffers a wireframe circle for rendering.
	void DrawCircle(FVector Base,FVector X,FVector Y,FColor Color,FLOAT Radius,INT NumSides);

	// DrawSphere - Buffers a wireframe sphere for rendering. -- Dave@Psyonix
	void DrawSphere(FVector Base,FColor Color,FLOAT Radius,INT NumDivisions);

	// DrawCylinder - Buffers a wireframe cylinder for rendering.
	void DrawCylinder(FRenderInterface* RI,FVector Base,FVector X,FVector Y,FVector Z,FColor Color,FLOAT Radius,FLOAT HalfHeight,INT NumSides);

	// DrawDirectionalArrow
	void DrawDirectionalArrow(FVector InLocation,FRotator InRotation,FColor InColor,FLOAT InDrawScale = 1.0f);

	// DrawConvexVolume
	void DrawConvexVolume(FConvexVolume Volume,FColor Color);

	// FRenderResource interface.
	virtual QWORD GetCacheId();
	virtual INT GetRevision();


	virtual INT GetSize();
	virtual INT GetStride();
	virtual INT GetComponents(FVertexComponent* OutComponents);
	virtual void GetStreamData(void* Dest);
	virtual void GetRawStreamData(void ** Dest, INT FirstVertex );
};

// Slight hack. If we dont have a FRenderInterface handy - use GTempLineBatcher
// All lines/boxes drawn in world space.
class ENGINE_API FTempLineBatcher
{
public:
	// lines
    TArray<FVector> LineStart;
    TArray<FVector> LineEnd;
    TArray<FColor>  LineColor;

	// staying lines
	TArray<FVector>	StayingLineStart;
	TArray<FVector>	StayingLineEnd;
	TArray<FColor>	StayingLineColor;

	// boxes
    TArray<FBox>  BoxArray;
    TArray<FColor>  BoxColor;

	// circles
	TArray<FVector>	CircleBase;
	TArray<FVector>	CircleX;
	TArray<FVector>	CircleY;
	TArray<FColor>	CircleColor;
	TArray<FLOAT>	CircleRadius;
	TArray<INT>		CircleNumSides;

	// spheres
	TArray<FVector> SphereBase;
	TArray<FColor>	SphereColor;
	TArray<FLOAT>	SphereRadius;
	TArray<INT>		SphereNumDivisions;

	// Constructor / Destructor

	FTempLineBatcher() {};
	~FTempLineBatcher() {};	

	// Interface

	void AddLine(FVector P1,FVector P2,FColor Color)
	{
		LineStart.AddItem(P1);
		LineEnd.AddItem(P2);
		LineColor.AddItem(Color);
	}

	void AddBox(FBox Box,FColor Color)
	{
		BoxArray.AddItem(Box);
		BoxColor.AddItem(Color);
	}

	void AddStayingLine(FVector P1,FVector P2,FColor Color)
	{
		StayingLineStart.AddItem(P1);
		StayingLineEnd.AddItem(P2);
		StayingLineColor.AddItem(Color);
	}

	void ClearStayingLines()
	{
		StayingLineStart.Empty();
		StayingLineEnd.Empty();
		StayingLineColor.Empty();
	}

	void AddCircle(FVector Base,FVector X,FVector Y,FColor Color,FLOAT Radius,INT NumSides)
	{
		CircleBase.AddItem(Base);
		CircleX.AddItem(X);
		CircleY.AddItem(Y);
		CircleColor.AddItem(Color);
		CircleRadius.AddItem(Radius);
		CircleNumSides.AddItem(NumSides);
	}

	void AddSphere(FVector Base,FColor Color,FLOAT Radius,INT NumDivisions)
	{
		SphereBase.AddItem(Base);
		SphereColor.AddItem(Color);
		SphereRadius.AddItem(Radius);
		SphereNumDivisions.AddItem(NumDivisions);
	}

	void Render(FRenderInterface* InRI, UBOOL InZTest=1);
};

//
//	FCanvasVertex
//
class ENGINE_API FCanvasVertex
{
public:

	FVector	Position;
	FColor	Diffuse;
	FLOAT	U,
			V;

	// Constructors.
	FCanvasVertex() {}
	FCanvasVertex(FVector InPosition,FColor InDiffuse,FLOAT InU,FLOAT InV)
	{
		Position = InPosition;
		Diffuse = InDiffuse;
		U = InU;
		V = InV;
	}
};

//
//	FCanvasUtil
//	Utilities for rendering stuff in "canvas" space.
//	Canvas space is in pixel coordinates, 0 through to viewport width/height.
//
class ENGINE_API FCanvasUtil : public FVertexStream
{
public:

	EPrimitiveType			PrimitiveType;
	UMaterial*				Material;
	INT						NumPrimitives;

	FRenderInterface*		RI;
	FMatrix					ScreenToCanvas,
							CanvasToScreen;

	TArray<FCanvasVertex>	Vertices;
	QWORD					CacheId;

	// Constructor/destructor.
	FCanvasUtil(FRenderTarget* RenderTarget,FRenderInterface* InRI);
	~FCanvasUtil();

	// Flush - Renders all buffered primitives.
	void Flush();

	// BeginPrimitive - Prepares to render a primitive with the given attributes.  Automatically flushes if necessary.

	void BeginPrimitive(EPrimitiveType InType,UMaterial* InMaterial);

	// DrawLine - Buffers a line for rendering.
	void DrawLine(FLOAT X1,FLOAT Y1,FLOAT X2,FLOAT Y2,FColor Color);

	// DrawPoint - Buffers a point for rendering.
	void DrawPoint(FLOAT X1,FLOAT Y1,FLOAT X2,FLOAT Y2,FLOAT Z,FColor Color);

	// DrawTile - Buffers a textured tile for rendering.
	void DrawTile(
		FLOAT X1,
		FLOAT Y1,
		FLOAT X2,
		FLOAT Y2,
		FLOAT U1,
		FLOAT V1,
		FLOAT U2,
		FLOAT V2,
		FLOAT Z,
		UMaterial* Material,
		FColor Color
		);

	// DrawString - Buffers a string for rendering.  Returns the width of the string rendered.
	INT DrawString(
		INT StartX,
		INT StartY,
		const TCHAR* Text,
		class UFont* Font,
		FColor Color,
		UBOOL IsRenderTarget=0
		);

	// FRenderResource interface.
	virtual QWORD GetCacheId();
	virtual INT GetRevision();

	// FVertexStream interface.
	virtual INT GetSize();
	virtual INT GetStride();
	virtual INT GetComponents(FVertexComponent* OutComponents);
	virtual void GetStreamData(void* Dest);
	virtual void GetRawStreamData(void ** Dest, INT FirstVertex );
};

//
//	FRawIndexBuffer
//
class ENGINE_API FRawIndexBuffer : public FIndexBuffer
{
public:

	TArray<_WORD>	Indices;
	QWORD			CacheId;
	INT				Revision;

	// Constructor.
	FRawIndexBuffer();

	// Stripify - Converts a triangle list into a triangle strip.
	virtual INT Stripify();

	// CacheOptimize - Orders a triangle list for better vertex cache coherency.
	virtual void CacheOptimize();

	// Serialization.
	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FRawIndexBuffer& I);

	// FRenderResource interface.
	virtual QWORD GetCacheId();
	virtual INT GetRevision();

	// FIndexBuffer interface.
	virtual INT GetSize();
	virtual void GetContents(void* Data);
	virtual INT GetIndexSize();
};


//
//	FRaw32BitIndexBuffer
//
class ENGINE_API FRaw32BitIndexBuffer : public FIndexBuffer
{
public:

	TArray<DWORD>	Indices;
	QWORD			CacheId;
	INT				Revision;

	// Constructor.
	FRaw32BitIndexBuffer();

	// Serialization.
	ENGINE_API friend FArchive& operator<<(FArchive& Ar,FRaw32BitIndexBuffer& I);

	// FRenderResource interface.
	virtual QWORD GetCacheId();
	virtual INT GetRevision();

	// FIndexBuffer interface.
	virtual INT GetSize();
	virtual void GetContents(void* Data);
	virtual INT GetIndexSize();
};

//
//	FRawColorStream
//
class ENGINE_API FRawColorStream : public FVertexStream
{
public:

	TArray<FColor>			Colors;
	QWORD					CacheId;
	INT						Revision;

	// Constructor.
	FRawColorStream();

	// Serializer.
	friend ENGINE_API FArchive& operator<<(FArchive& Ar,FRawColorStream& ColorStream);

	// FRenderResource interface.
	virtual QWORD GetCacheId();
	virtual INT GetRevision();

	// FVertexStream interface.
	virtual INT GetSize();
	virtual INT GetStride();
	virtual INT GetComponents(FVertexComponent* OutComponents);
	virtual void GetStreamData(void* Dest);
	virtual void GetRawStreamData(void ** Dest, INT FirstVertex );
};

//
//  FSolidColorTexture
//

class ENGINE_API FSolidColorTexture : public FTexture
{
public:

	FColor	Color;

	INT		Revision;
	QWORD	CacheId;

	// Constructor.

	FSolidColorTexture(FColor InColor);

	// FRenderResource interface.

	virtual INT GetRevision();
	virtual QWORD GetCacheId();

	// FBaseTexture interface.

	virtual INT GetWidth();
	virtual INT GetHeight();
	virtual ETexClampMode GetUClamp();
	virtual ETexClampMode GetVClamp();
	virtual ETextureFormat GetFormat();
	virtual INT GetNumMips();
	virtual INT GetFirstMip();

	// FTexture interface.

	virtual void GetTextureData(INT MipIndex,void* Dest,INT DestStride,ETextureFormat DestFormat,UBOOL ColoredMips);
	virtual void* GetRawTextureData(INT MipIndex);
	virtual void UnloadRawTextureData( INT MipIndex ) {}

	virtual UTexture* GetUTexture();
};

//
//	FAuxRenderTarget
//

class FAuxRenderTarget : public FRenderTarget
{
public:

	QWORD			CacheId;
	INT				Revision,
					Width,
					Height;
	ETextureFormat	Format;

	// Constructor.

	FAuxRenderTarget(INT InWidth,INT InHeight,ETextureFormat InFormat);
	~FAuxRenderTarget();

	// FRenderResource interface.

	virtual INT GetRevision();
	virtual QWORD GetCacheId();

	// FBaseTexture interface.

	virtual INT GetWidth();
	virtual INT GetHeight();
	virtual ETexClampMode GetUClamp();
	virtual ETexClampMode GetVClamp();
	virtual ETextureFormat GetFormat();
	virtual INT GetNumMips();
	virtual INT GetFirstMip();
};

#if 0	// Incomplete side project :)
//
//	FGeometryProcessingRI
//

class FGeometryProcessingRI : public FRenderInterface
{
public:

	// FCachedResource

	class FCachedResource
	{
	public:

		QWORD			CacheId;
		INT				Revision;
		TArray<BYTE>	Data;

		FCachedResource() {}

		FCachedResource(QWORD InCacheId)
		{
			CacheId = InCacheId;
			Revision = 0;
		}

		virtual ~FCachedResource()
		{
		}
	};

	// FCachedVertexStream

	class FCachedVertexStream : public FCachedResource
	{
	public:
		
		FVertexComponent	Components[MAX_VERTEX_COMPONENTS];
		INT					NumComponents,
							Size,
							Stride;

		FCachedVertexStream() {}

		FCachedVertexStream(QWORD InCacheId) : FCachedResource(InCacheId)
		{
		}

		void Cache(FVertexStream* VertexStream)
		{
			NumComponents = VertexStream->GetComponents(Components);
			Size = VertexStream->GetSize();
			Stride = VertexStream->GetStride();

			Data.Empty(Size);
			VertexStream->GetStreamData(&Data(Data.Add(Size)));

			Revision = VertexStream->GetRevision();
		}
	};

	// FCachedIndexBuffer

	class FCachedIndexBuffer : public FCachedResource
	{
	public:

		INT	Size;

		FCachedIndexBuffer() {}

		FCachedIndexBuffer(QWORD InCacheId) : FCachedResource(InCacheId)
		{
		}

		void Cache(FIndexBuffer* IndexBuffer)
		{
			Size = IndexBuffer->GetSize();

			Data.Empty(Size);
			IndexBuffer->GetContents((_WORD*) &Data(Data.Add(Size)));

			Revision = IndexBuffer->GetRevision();
		}
	};

	// FCachedTexture

	class FCachedTexture : public FCachedResource
	{
	public:

		UTexture*		Texture;
		INT				Width,
						Height,
						NumMips;

		FCachedTexture() {}

		FCachedTexture(QWORD InCacheId) : FCachedResource(InCacheId)
		{
			Texture = NULL;
		}

		void Cache(FBaseTexture* Texture)
		{
			Width = Texture->GetWidth();
			Height = Texture->GetHeight();
			NumMips = Texture->GetNumMips();

			Data.Empty(Width * Height * sizeof(FColor));
			Data.Add(Width * Height * sizeof(FColor));

			FCompositeTexture*	CompositeInterface = Texture->GetCompositeTextureInterface();

			if(CompositeInterface)
			{
				INT	NumChildren = CompositeInterface->GetNumChildren();

				for(INT ChildIndex = 0;ChildIndex < NumChildren;ChildIndex++)
				{
					INT			ChildX = 0,
								ChildY = 0;
					FTexture*	ChildTexture = CompositeInterface->GetChild(ChildIndex,&ChildX,&ChildY);
					void*		DataPtr = (void*) (((FColor*) &Data(0)) + ChildX + ChildY * Width);

					ChildTexture->GetTextureData(0,DataPtr,Width * sizeof(FColor),TEXF_RGBA8);
				}
			}
			else
				appErrorf(TEXT("Unsupported texture interface"));

			Revision = Texture->GetRevision();
		}
	};

	// FProcessedVertex

	struct FProcessedVertex
	{
		FVector	Position,
				Normal;
		FColor	Diffuse,
				Emissive;
		FVector	UVs[8];
	};

	// FRenderState

	struct FRenderState
	{
		FRenderState*			PreviousState;

		QWORD					RenderTarget;
		INT						ViewportX,
								ViewportY,
								ViewportWidth,
								ViewportHeight;

		ECullMode				CullMode;

		FColor					AmbientLight;
		UBOOL					DynamicLighting,
								StaticLighting,
								Modulate2X,
								LightingOnly;
		FCachedTexture*			LightMap;
		FRenderLightInfo		Lights[8];
		UBOOL					LightEnabled[8];

		FMatrix					LocalToWorld;

		UMaterial*				Material;

		EVertexShader			VertexShader;
		FCachedVertexStream*	VertexStreams[16];
		INT						NumVertexStreams;

		FCachedIndexBuffer*		IndexBuffer;
		INT						IndexBufferBase;

		ECompareFunction		StencilFunction;
	};

	FRenderState*					CurrentState;
	TArray<FCachedResource*>		CachedResources;
	TMap<QWORD,FCachedResource*>	ResourceMap;

	FCachedIndexBuffer				DynamicIndexBuffer;
	FCachedVertexStream				DynamicVertexStream;

	// Constructor.

	FGeometryProcessingRI(const TCHAR* PackageName,const TCHAR* StaticMeshName)
	{
		guard(FGeometryProcessingRI::FGeometryProcessingRI);

		CurrentState = new FRenderState;
		appMemzero(CurrentState,sizeof(FRenderState));
		CurrentState->LocalToWorld = FMatrix::Identity;
		CurrentState->StencilFunction = CF_Always;

		unguard;
	}

	// Destructor.

	~FGeometryProcessingRI()
	{
		check(CurrentState && !CurrentState->PreviousState);
		delete CurrentState;

		for(INT ResourceIndex = 0;ResourceIndex < CachedResources.Num();ResourceIndex++)
			delete CachedResources(ResourceIndex);
	}

	// CacheTexture

	FCachedTexture* CacheTexture(FBaseTexture* Texture)
	{
		QWORD			CacheId = Texture->GetCacheId();
		FCachedTexture*	CachedTexture = (FCachedTexture*) ResourceMap.FindRef(CacheId);

		if(!CachedTexture)
		{
			CachedTexture = new FCachedTexture(CacheId);
			CachedResources.AddItem(CachedTexture);
			ResourceMap.Set(CacheId,CachedTexture);
		}

		if(CachedTexture->Revision != Texture->GetRevision())
			CachedTexture->Cache(Texture);

		return CachedTexture;
	}

	// CacheVertexStream

	FCachedVertexStream* CacheVertexStream(FVertexStream* VertexStream)
	{
		QWORD					CacheId = VertexStream->GetCacheId();
		FCachedVertexStream*	CachedVertexStream = (FCachedVertexStream*) ResourceMap.FindRef(CacheId);

		if(!CachedVertexStream)
		{
			CachedVertexStream = new FCachedVertexStream(CacheId);
			CachedResources.AddItem(CachedVertexStream);
			ResourceMap.Set(CacheId,CachedVertexStream);
		}

		if(CachedVertexStream->Revision != VertexStream->GetRevision())
			CachedVertexStream->Cache(VertexStream);

		return CachedVertexStream;
	}

	// CacheIndexBuffer

	FCachedIndexBuffer* CacheIndexBuffer(FIndexBuffer* IndexBuffer)
	{
		QWORD				CacheId = IndexBuffer->GetCacheId();
		FCachedIndexBuffer*	CachedIndexBuffer = (FCachedIndexBuffer*) ResourceMap.FindRef(CacheId);

		if(!CachedIndexBuffer)
		{
			CachedIndexBuffer = new FCachedIndexBuffer(CacheId);
			CachedResources.AddItem(CachedIndexBuffer);
			ResourceMap.Set(CacheId,CachedIndexBuffer);
		}

		if(CachedIndexBuffer->Revision != IndexBuffer->GetRevision())
			CachedIndexBuffer->Cache(IndexBuffer);

		return CachedIndexBuffer;
	}

	// FRenderInterface interface.

	virtual void PushState()
	{
		FRenderState*	PreviousState = CurrentState;

		CurrentState = new FRenderState;
		appMemcpy(CurrentState,PreviousState,sizeof(FRenderState));
		CurrentState->PreviousState = PreviousState;
	}
	virtual void PopState()
	{
		FRenderState*	PreviousState = CurrentState;

		CurrentState = CurrentState->PreviousState;

		delete PreviousState;
	}
	virtual void SetRenderTarget(FRenderTarget* RenderTarget)
	{
		CurrentState->RenderTarget = RenderTarget ? RenderTarget->GetCacheId() : 0;
	}
	virtual void SetViewport(INT X,INT Y,INT Width,INT Height)
	{
		CurrentState->ViewportX = X;
		CurrentState->ViewportY = Y;
		CurrentState->ViewportWidth = Width;
		CurrentState->ViewportHeight = Height;
	}
	virtual void Clear(UBOOL UseColor,FColor Color,UBOOL UseDepth,FLOAT Depth,UBOOL UseStencil,DWORD Stencil) {}
	virtual void PushHit(const BYTE* Data,INT Count) {}
	virtual void PopHit(INT Count,UBOOL Force) {}
	virtual void SetCullMode(ECullMode CullMode) { CurrentState->CullMode = CullMode; }
	virtual void SetAmbientLight(FColor Color) { CurrentState->AmbientLight = Color; }
	virtual void EnableLighting(UBOOL UseDynamic,UBOOL UseStatic,UBOOL Modulate2X,FBaseTexture* LightMap,UBOOL LightingOnly)
	{
		CurrentState->DynamicLighting = UseDynamic;
		CurrentState->StaticLighting = UseStatic;
		CurrentState->Modulate2X = Modulate2X;
		CurrentState->LightMap = LightMap ? CacheTexture(LightMap) : NULL;
		CurrentState->LightingOnly = LightingOnly;
	}
	virtual void SetLight(INT LightIndex,FRenderLightInfo* LightInfo)
	{
		if(LightIndex < 8)
		{
			if(LightInfo)
			{
				appMemcpy(&CurrentState->Lights[LightIndex],LightInfo,sizeof(FRenderLightInfo));
				CurrentState->LightEnabled[LightIndex] = 1;
			}
			else
				CurrentState->LightEnabled[LightIndex] = 0;
		}
	}
	virtual void SetDistanceFog(UBOOL Enable,FLOAT FogStart,FLOAT FogEnd,FColor Color) {}
	virtual void SetGlobalColor(FColor Color) {}
	virtual void SetTransform(ETransformType Type,const FMatrix& Matrix)
	{
		if(Type == TT_LocalToWorld)
			CurrentState->LocalToWorld = Matrix;
	}
	virtual void SetMaterial(UMaterial* Material,FString* ErrorString=NULL, UMaterial** ErrorMaterial=NULL) { CurrentState->Material = Material; }
	virtual void SetStencilOp(ECompareFunction Test,DWORD Ref,DWORD Mask,EStencilOp FailOp,EStencilOp ZFailOp,EStencilOp PassOp,DWORD WriteMask) { CurrentState->StencilFunction = Test; }
	virtual void SetZBias(INT ZBias) {}
	virtual INT SetVertexStreams(EVertexShader Shader,FVertexStream** Streams,INT NumStreams)
	{
		CurrentState->VertexShader = Shader;
		CurrentState->NumVertexStreams = NumStreams;

		for(INT StreamIndex = 0;StreamIndex < NumStreams;StreamIndex++)
			CurrentState->VertexStreams[StreamIndex] = CacheVertexStream(Streams[StreamIndex]);
		return 0;
	}
	virtual INT SetDynamicStream(EVertexShader Shader,FVertexStream* Stream)
	{
		DynamicVertexStream.Cache(Stream);

		CurrentState->VertexShader = Shader;
		CurrentState->NumVertexStreams = 1;
		CurrentState->VertexStreams[0] = &DynamicVertexStream;

		return 0;
	}
	virtual INT SetIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseIndex)
	{
		if(IndexBuffer)
			CurrentState->IndexBuffer = CacheIndexBuffer(IndexBuffer);
		else
			CurrentState->IndexBuffer = NULL;

		CurrentState->IndexBufferBase = BaseIndex;

		return 0;
	}
	virtual INT SetDynamicIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseIndex)
	{
		DynamicIndexBuffer.Cache(IndexBuffer);

		CurrentState->IndexBuffer = &DynamicIndexBuffer;
		CurrentState->IndexBufferBase = BaseIndex;

		return 0;
	}
	virtual void DrawPrimitive(EPrimitiveType PrimitiveType,INT FirstIndex,INT NumPrimitives,INT MinIndex,INT MaxIndex)
	{
		if(CurrentState->StencilFunction != CF_Always)
			return;

		if(CurrentState->Material)
		{
			for(UObject* Outer = CurrentState->Material->GetOuter();Outer;Outer = Outer->GetOuter())
				if(Outer == UObject::GetTransientPackage())
					return;
		}


		// Extract the component stream pointers from the source streams.

		BYTE*	PositionStream = NULL;
		BYTE*	NormalStream = NULL;
		BYTE*	DiffuseStream = NULL;
		BYTE*	SpecularStream = NULL;
		BYTE*	UVStreams[8] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
		INT		PositionStreamStride = 0,
				NormalStreamStride = 0,
				DiffuseStreamStride = 0,
				SpecularStreamStride = 0,
				UVStreamStrides[8] = { 0, 0, 0, 0, 0, 0, 0, 0 },
				UVSizes[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

		for(INT StreamIndex = 0;StreamIndex < CurrentState->NumVertexStreams;StreamIndex++)
		{
			FCachedVertexStream*	CachedVertexStream = CurrentState->VertexStreams[StreamIndex];
			INT						ComponentOffset = 0;

			for(INT ComponentIndex = 0;ComponentIndex < CachedVertexStream->NumComponents;ComponentIndex++)
			{
				FVertexComponent*	Component = &CachedVertexStream->Components[ComponentIndex];

				if(Component->Function == FVF_Position)
				{
					check(Component->Type == CT_Float3);
					PositionStream = &CachedVertexStream->Data(ComponentOffset);
					PositionStreamStride = CachedVertexStream->Stride;
					ComponentOffset += sizeof(FVector);
				}
				else if(Component->Function == FVF_Normal)
				{
					check(Component->Type == CT_Float3);
					NormalStream = &CachedVertexStream->Data(ComponentOffset);
					NormalStreamStride = CachedVertexStream->Stride;
					ComponentOffset += sizeof(FVector);
				}
				else if(Component->Function == FVF_Diffuse)
				{
					check(Component->Type == CT_Color);
					DiffuseStream = &CachedVertexStream->Data(ComponentOffset);
					DiffuseStreamStride = CachedVertexStream->Stride;
					ComponentOffset += sizeof(FColor);
				}
				else if(Component->Function == FVF_Specular)
				{
					check(Component->Type == CT_Color);
					SpecularStream = &CachedVertexStream->Data(ComponentOffset);
					SpecularStreamStride = CachedVertexStream->Stride;
					ComponentOffset += sizeof(FColor);
				}
				else if(Component->Function == FVF_TexCoord0)
				{
					check(Component->Type == CT_Float2 || Component->Type == CT_Float3);
					UVStreams[0] = &CachedVertexStream->Data(ComponentOffset);
					UVStreamStrides[0] = CachedVertexStream->Stride;
					UVSizes[0] = sizeof(FLOAT) * (CT_Float1 - Component->Type + 1);
					ComponentOffset += UVSizes[0];
				}
				else if(Component->Function == FVF_TexCoord1)
				{
					check(Component->Type == CT_Float2 || Component->Type == CT_Float3);
					UVStreams[1] = &CachedVertexStream->Data(ComponentOffset);
					UVStreamStrides[1] = CachedVertexStream->Stride;
					UVSizes[1] = sizeof(FLOAT) * (CT_Float1 - Component->Type + 1);
					ComponentOffset += UVSizes[1];
				}
				else if(Component->Function == FVF_TexCoord2)
				{
					check(Component->Type == CT_Float2 || Component->Type == CT_Float3);
					UVStreams[2] = &CachedVertexStream->Data(ComponentOffset);
					UVStreamStrides[2] = CachedVertexStream->Stride;
					UVSizes[2] = sizeof(FLOAT) * (CT_Float1 - Component->Type + 1);
					ComponentOffset += UVSizes[2];
				}
				else if(Component->Function == FVF_TexCoord3)
				{
					check(Component->Type == CT_Float2 || Component->Type == CT_Float3);
					UVStreams[3] = &CachedVertexStream->Data(ComponentOffset);
					UVStreamStrides[3] = CachedVertexStream->Stride;
					UVSizes[3] = sizeof(FLOAT) * (CT_Float1 - Component->Type + 1);
					ComponentOffset += UVSizes[3];
				}
				else if(Component->Function == FVF_TexCoord4)
				{
					check(Component->Type == CT_Float2 || Component->Type == CT_Float3);
					UVStreams[4] = &CachedVertexStream->Data(ComponentOffset);
					UVStreamStrides[4] = CachedVertexStream->Stride;
					UVSizes[4] = sizeof(FLOAT) * (CT_Float1 - Component->Type + 1);
					ComponentOffset += UVSizes[4];
				}
				else if(Component->Function == FVF_TexCoord5)
				{
					check(Component->Type == CT_Float2 || Component->Type == CT_Float3);
					UVStreams[5] = &CachedVertexStream->Data(ComponentOffset);
					UVStreamStrides[5] = CachedVertexStream->Stride;
					UVSizes[5] = sizeof(FLOAT) * (CT_Float1 - Component->Type + 1);
					ComponentOffset += UVSizes[5];
				}
				else if(Component->Function == FVF_TexCoord6)
				{
					check(Component->Type == CT_Float2 || Component->Type == CT_Float3);
					UVStreams[6] = &CachedVertexStream->Data(ComponentOffset);
					UVStreamStrides[6] = CachedVertexStream->Stride;
					UVSizes[6] = sizeof(FLOAT) * (CT_Float1 - Component->Type + 1);
					ComponentOffset += UVSizes[6];
				}
				else if(Component->Function == FVF_TexCoord7)
				{
					check(Component->Type == CT_Float2 || Component->Type == CT_Float3);
					UVStreams[7] = &CachedVertexStream->Data(ComponentOffset);
					UVStreamStrides[7] = CachedVertexStream->Stride;
					UVSizes[7] = sizeof(FLOAT) * (CT_Float1 - Component->Type + 1);
					ComponentOffset += UVSizes[7];
				}
				else
					appErrorf(TEXT("Unknown component type %u in vertex stream %u at offset %u!"),StreamIndex,ComponentOffset);
			}
		}

		check(PositionStream);

		// Determine the range of vertices used to render the primitives.

		if(!CurrentState->IndexBuffer)
		{
			MinIndex = FirstIndex;

			if(PrimitiveType == PT_TriangleList)
				MaxIndex = MinIndex + NumPrimitives * 3 - 1;
			else if(PrimitiveType == PT_TriangleStrip || PrimitiveType == PT_TriangleFan)
				MaxIndex = MinIndex + NumPrimitives - 3;
			else if(PrimitiveType == PT_PointList)
				MaxIndex = MinIndex + NumPrimitives - 1;
			else if(PrimitiveType == PT_LineList)
				MaxIndex = MinIndex + NumPrimitives * 2 - 1;
			else
				appErrorf(TEXT("Invalid primitive type!"));
		}

		// Process the vertices used to render the primitives.

		FProcessedVertex*	ProcessedVertices = new FProcessedVertex[MaxIndex - MinIndex + 1];

		for(INT VertexIndex = MinIndex;VertexIndex <= MaxIndex;VertexIndex++)
		{
			FProcessedVertex*	Vertex = ProcessedVertices + (VertexIndex - MinIndex);

			Vertex->Position = CurrentState->LocalToWorld.TransformFVector(*(FVector*)(PositionStream + VertexIndex * PositionStreamStride));

			if(NormalStream)
				Vertex->Normal = CurrentState->LocalToWorld.TransformNormal(*(FVector*)(NormalStream + VertexIndex * NormalStreamStride));

			if(DiffuseStream && CurrentState->StaticLighting)
				Vertex->Emissive = *(FColor*)(DiffuseStream + VertexIndex * DiffuseStreamStride);
			else
				Vertex->Emissive = FColor(255,255,255);

			if(SpecularStream)
				Vertex->Diffuse = *(FColor*)(SpecularStream + VertexIndex * SpecularStreamStride);
			else
				Vertex->Diffuse = FColor(0,0,0);

			/*
			if(CurrentState->DynamicLighting)
			{
				FPlane	VertexLight = CurrentState->AmbientLight.Plane();

				for(INT LightIndex = 0;LightIndex < 8;LightIndex++)
				{
					if(CurrentState->LightEnabled[LightIndex])
					{
						FRenderLightInfo*	Light = &CurrentState->Lights[LightIndex];

						if(Light->Type == RLT_Point)
						{
							FVector				LV = Light->Origin - Vertex->Position;
							FLOAT				D = LV.Size();

							if(D < Light->Range)
								VertexLight += ((LV / LV.Size()) | Vertex->Normal) /
													(Light->ConstantAttenuation + Light->LinearAttenuation * D + Light->QuadraticAttenuation * D * D) *
													Light->Diffuse;
						}
					}
				}

				Vertex->Diffuse += FColor(VertexLight);
			}
			*/

			for(INT UVIndex = 0;UVIndex < 8;UVIndex++)
				if(UVStreams[UVIndex])
					appMemcpy(&Vertex->UVs[UVIndex],UVStreams[UVIndex] + VertexIndex * UVStreamStrides[UVIndex],UVSizes[UVIndex]);
		}

		// Extract the indices used to render this primitives.

		_WORD*	Indices = NULL;
		INT		IndexBase = 0;

		if(CurrentState->IndexBuffer)
		{
			Indices = ((_WORD*) &CurrentState->IndexBuffer->Data(0)) + FirstIndex;
			IndexBase = CurrentState->IndexBufferBase;
		}

		if(PrimitiveType == PT_TriangleList)
		{
			for(INT PrimitiveIndex = 0;PrimitiveIndex < NumPrimitives;PrimitiveIndex++)
			{
				FStaticMeshTriangle*	Triangle = new(StaticMesh->RawTriangles) FStaticMeshTriangle;
				FProcessedVertex*		Vertices[3] =
				{
					&ProcessedVertices[Indices ? IndexBase + Indices[FirstIndex + PrimitiveIndex * 3 + 0] - MinIndex : FirstIndex + PrimitiveIndex * 3 + 0],
					&ProcessedVertices[Indices ? IndexBase + Indices[FirstIndex + PrimitiveIndex * 3 + 1] - MinIndex : FirstIndex + PrimitiveIndex * 3 + 1],
					&ProcessedVertices[Indices ? IndexBase + Indices[FirstIndex + PrimitiveIndex * 3 + 2] - MinIndex : FirstIndex + PrimitiveIndex * 3 + 2]
				};

				Triangle->Vertices[0] = Vertices[0]->Position;
				Triangle->Vertices[1] = Vertices[1]->Position;
				Triangle->Vertices[2] = Vertices[2]->Position;

				Triangle->SmoothingMask = 0xffffffff;
				Triangle->Normal = ((Triangle->Vertices[2] - Triangle->Vertices[0]) ^ (Triangle->Vertices[1] - Triangle->Vertices[0])).SafeNormal();

				Triangle->NumUVs = 0;

				for(INT UVIndex = 0;UVIndex < 8;UVIndex++)
				{
					if(UVStreams[UVIndex])
					{
						Triangle->UVs[Triangle->NumUVs][0].U = Vertices[0]->UVs[UVIndex].X;
						Triangle->UVs[Triangle->NumUVs][0].V = Vertices[0]->UVs[UVIndex].Y;
						Triangle->UVs[Triangle->NumUVs][1].U = Vertices[1]->UVs[UVIndex].X;
						Triangle->UVs[Triangle->NumUVs][1].V = Vertices[1]->UVs[UVIndex].Y;
						Triangle->UVs[Triangle->NumUVs][2].U = Vertices[2]->UVs[UVIndex].X;
						Triangle->UVs[Triangle->NumUVs][2].V = Vertices[2]->UVs[UVIndex].Y;

						Triangle->NumUVs++;
					}
				}

				Triangle->Colors[0] = FColor(Vertices[0]->Diffuse.Plane() * Vertices[0]->Emissive.Plane());
				Triangle->Colors[1] = FColor(Vertices[1]->Diffuse.Plane() * Vertices[1]->Emissive.Plane());
				Triangle->Colors[2] = FColor(Vertices[2]->Diffuse.Plane() * Vertices[2]->Emissive.Plane());

				Triangle->Material = PrimitiveMaterial;
				Triangle->PolyFlags = 0;
			}
		}

		delete [] ProcessedVertices;
	}
};

#endif

#include "UnForcePacking_end.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

