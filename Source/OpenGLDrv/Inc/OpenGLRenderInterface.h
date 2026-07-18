/*=============================================================================
	OpenGLRenderInterface.h: Unreal OpenGL rendering interface definition.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#ifndef HEADER_OPENGLRENDERINTERFACE
#define HEADER_OPENGLRENDERINTERFACE

class UOpenGLRenderDevice;


// !!! FIXME: a big round of applause for quailty GNU software!  --ryan.
#ifndef BUGGYINLINE
#ifdef __GNUC__
#define BUGGYINLINE
#else
#define BUGGYINLINE inline
#endif
#endif


//
//	FOpenGLModifierInfo
//
struct FOpenGLModifierInfo
{
	UBOOL				ModifyTextureTransforms;
	UBOOL				ModifyFramebufferBlending;
	UBOOL				ModifyColor;
	UBOOL				ModifyOpacity;

	// texture modifier
	FMatrix				Matrix;
	BYTE				TexCoordSource;
	BYTE				TexCoordCount;
	UBOOL				TexCoordProjected;

	// framebuffer blend modifier
    BYTE				FrameBufferBlending;
    UBOOL				ZWrite;
    UBOOL				ZTest;
    UBOOL				AlphaTest;
    UBOOL				TwoSided;
    BYTE				AlphaRef;

	// color modifier
	FColor				TFactorColor;
	UBOOL				AlphaBlend;

	// fallback info
	UMaterial*			BestFallbackPoint;

	// opacity modifier
	UMaterial*			Opacity;
	UBOOL				OpacityOverrideTexModifier;

	// Constructor
	FOpenGLModifierInfo();

	// FOpenGLModifierInfo interface
	void SetDetailTextureScale( FLOAT Scale );
};


//
//	FOpenGLMaterialState
//
class FOpenGLMaterialState
{
public:
	EPixelShader		PixelShader;
	UBOOL				AlphaBlending;			// Alpha blending is enabled
	UBOOL				AlphaTest;				// Alpha test is enabled?
	BYTE				AlphaRef;				// If alpha testing, the value to compare against
	UBOOL				ZTest;					// Test zbuffer
	UBOOL				ZWrite;					// Write to zbuffer
	UBOOL				TwoSided;
	EFillMode			FillMode;				// Wireframe, flatshaded or solid
	FColor				TFactorColor;
	DWORD				SrcBlend,
						DestBlend;
	UBOOL				OverrideFogColor;
	FColor				OverriddenFogColor;
	UBOOL				PatchLighting;

	INT					StagesUsed;
	FOpenGLMaterialStateStage	Stages[8];

	INT	NumRefs;

	FOpenGLMaterialState();
};

//
//	FOpenGLMaterialStatePool
//

class FOpenGLMaterialStatePool
{
public:

	TArray<FOpenGLMaterialState*>	FreeStates;

	// Destructor.

	~FOpenGLMaterialStatePool()
	{
		for(INT StateIndex = 0;StateIndex < FreeStates.Num();StateIndex++)
			delete FreeStates(StateIndex);
	}

	// AllocateState

	FOpenGLMaterialState* AllocateState(FOpenGLMaterialState* DefaultState)
	{
		FOpenGLMaterialState*	Result;

		if(FreeStates.Num())
			Result = FreeStates.Pop();
		else
			Result = new(TEXT("OpenGLMaterialState")) FOpenGLMaterialState();

		if(DefaultState)
			appMemcpy(Result,DefaultState,sizeof(FOpenGLMaterialState));

		Result->NumRefs = 1;

		return Result;
	}

	// FreeState

	void FreeState(FOpenGLMaterialState* State)
	{
		FreeStates.AddItem(State);
	}
};

enum EOpenGLMatrixDirty
{
	MATRIX_Projection	= 1,
	MATRIX_Modelview	= 2
};

#define MAX_STATESTACKDEPTH 128

//
//	FOpenGLRenderInterface
//
class FOpenGLRenderInterface : public FRenderInterface
{
public:

	//
	// FOpenGLSavedState
	//
	class FOpenGLSavedState
	{
	public:

		INT						ViewportX,
								ViewportY,
								ViewportWidth,
								ViewportHeight;

		INT						ZBias;

		FMatrix					LocalToWorld;
		FMatrix					WorldToCamera;
		FMatrix					CameraToScreen;

		FOpenGLVertexShader*	VertexShader;
		FOpenGLVertexStream*	Streams[16];
		INT						StreamStrides[16],
								NumStreams;

		FOpenGLIndexBuffer*		IndexBuffer;
		INT						IndexBufferBase;

		ECullMode				CullMode;

		UBOOL					UseDetailTexturing;
		UBOOL					UseDynamicLighting;
		UBOOL					UseStaticLighting;
		UBOOL					LightingModulate2X;
		UBOOL					LightingOnly;
		FOpenGLTexture*			Lightmap;
		FSphere					LitSphere;
		FColor					AmbientLightColor;

		struct FOpenGLLightState
		{
			UBOOL				Enabled,
								UseDirection;
			FPlane				Position,
								Direction,
								Diffuse;

			FLOAT				SpotCutoff,
								SpotExponent,
								ConstantAttenuation,
								LinearAttenuation,
								QuadraticAttenuation;
		} Lights[8];
	
		UBOOL					HasDiffuse;

		UBOOL					DistanceFogEnabled;
		FColor					DistanceFogColor;
		FLOAT					DistanceFogStart,
								DistanceFogEnd;

		UBOOL					ZWrite,
								ZTest,
								AlphaTest;
		INT						AlphaRef;

		FLOAT					NPatchTesselation;

		FOpenGLMaterialState*	MaterialPasses[8];
		INT						NumMaterialPasses;
		FOpenGLMaterialState*	CurrentMaterialState;

		UBOOL					OtherRenderTarget;
		GLuint					RenderTargetSurface;
		GLuint					DepthStencilSurface;

		FOpenGLSavedState();
	};

	// Deferred commit.
	DWORD						LightsDirty,
								MatricesDirty,
								ArraysDirty;
	
	// State stack stuff.
	FOpenGLSavedState			SavedStates[MAX_STATESTACKDEPTH];
	FOpenGLSavedState*			CurrentState;
	FOpenGLMaterialState		DefaultPass;
	INT							SavedStateIndex;

	FOpenGLMaterialStatePool	MaterialStatePool;


	// Variables.
	UOpenGLRenderDevice*		RenDev;
	UViewport*					Viewport;

	EPrecacheMode				PrecacheMode;


	// Constructor.
	FOpenGLRenderInterface(UOpenGLRenderDevice* InRenDev);

	// FRenderInterface interface.
	virtual void PushState();
	virtual void PopState();

	virtual UBOOL SetRenderTarget(FRenderTarget* RenderTarget);
	virtual void SetViewport(INT X,INT Y,INT Width,INT Height);
	virtual void Clear(UBOOL UseColor,FColor Color,UBOOL UseDepth,FLOAT Depth,UBOOL UseStencil,DWORD Stencil);

	virtual void PushHit(const BYTE* Data,INT Count);
	virtual void PopHit(INT Count,UBOOL Force);

	virtual void SetCullMode(ECullMode CullMode);

	virtual void SetAmbientLight(FColor Color);
	virtual void EnableLighting(UBOOL UseDynamic, UBOOL UseStatic, UBOOL Modulate2X, FBaseTexture* LightmapTexture, UBOOL LightingOnly, FSphere LitSphere );
	virtual void SetLight(INT LightIndex,FDynamicLight* Light,FLOAT Scale); // sjs
 
	virtual void SetNPatchTesselation( FLOAT Tesselation );
	virtual void SetDistanceFog(UBOOL Enable,FLOAT FogStart,FLOAT FogEnd,FColor Color);
	virtual void SetGlobalColor(FColor Color);
	
	virtual void SetTransform(ETransformType Type,const FMatrix& Matrix);

	virtual void SetMaterial(UMaterial* Material, FString* ErrorString, UMaterial** ErrorMaterial, INT* NumPasses);
	virtual void SetZBias(INT ZBias);
	virtual void SetStencilOp(ECompareFunction Test,DWORD Ref,DWORD Mask,EStencilOp FailOp,EStencilOp ZFailOp,EStencilOp PassOp,DWORD WriteMask);
	
	virtual void SetPrecacheMode( EPrecacheMode PrecacheMode );
	
	virtual INT  SetVertexStreams(EVertexShader Shader,FVertexStream** Streams,INT NumStreams);
	virtual INT  SetDynamicStream(EVertexShader Shader,FVertexStream* Stream);

	virtual INT  SetIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseVertexIndex);
	virtual INT  SetDynamicIndexBuffer(FIndexBuffer* IndexBuffer,INT BaseVertexIndex);

	virtual void DrawPrimitive(EPrimitiveType PrimitiveType,INT FirstIndex,INT NumPrimitives,INT MinIndex,INT MaxIndex);

    // sjs ---
	virtual int LockDynBuffer(BYTE** pOutBuffer, int numVerts, int stride, DWORD componentFlags);
	virtual int UnlockDynBuffer( void );
	virtual void DrawDynQuads(INT NumPrimitives);
	virtual void DrawQuads(INT FirstVertex, INT NumPrimitives);
    // --- sjs

	// FOpenGLRenderInterface interface.
	void Locked( UViewport* InViewport, BYTE* InHitData,INT* InHitSize);
	void Unlocked();

private:
	void SetMaterialBlending( FOpenGLMaterialState* NewMaterialState );

	FOpenGLTexture* CacheTexture(FBaseTexture* Texture);
	void SetTexture( FOpenGLTexture* Texture );
		
	// Deferred stuff.
	void CommitLights();
	void CommitStreams( INT FirstIndex, INT Pass );
	void CommitMatrices();

	// Various material handlers
	UBOOL SetShaderMaterial( UShader* InShader, FOpenGLModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial );
	UBOOL SetSimpleMaterial( UMaterial* InMaterial, FOpenGLModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial );
	UBOOL SetParticleMaterial( UParticleMaterial* InParticleMaterial, FOpenGLModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial );
	UBOOL SetTerrainMaterial( UTerrainMaterial* InTerrainMaterial, FOpenGLModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial );
	UBOOL SetProjectorMaterial( UProjectorMaterial* ProjectorMaterial, FOpenGLModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial );
	void  SetLightingOnlyMaterial();

	// Helpers
	BUGGYINLINE UBOOL HandleCombinedMaterial( UMaterial* InMaterial, INT& PassesUsed, INT& StagesUsed, FOpenGLModifierInfo ModifierInfo, UBOOL InvertOutputAlpha=0, FString* ErrorString=NULL, UMaterial** ErrorMaterial=NULL );
	BUGGYINLINE void  SetShaderBitmap( FOpenGLMaterialStateStage& Stage, UBitmapMaterial* BitmapMaterial );
	inline void  HandleOpacityBitmap( FOpenGLMaterialStateStage& Stage, UBitmapMaterial* Bitmap, UBOOL ModulateAlpha=0 );
	inline void  HandleVertexOpacity( FOpenGLMaterialStateStage& Stage, UVertexColor* VertexColor );
	inline void  HandleSpecular_SP( FOpenGLMaterialStateStage& Stage, UBitmapMaterial* Bitmap, UBOOL UseSpecularity, UBOOL UseConstantSpecularity, UBOOL ModulateSpecular2X ); // sjs
	inline void  HandleSelfIllumination_SP( FOpenGLMaterialStateStage& Stage, UBitmapMaterial* Bitmap );
	inline void  HandleLighting_MP( FOpenGLMaterialStateStage& Stage, FOpenGLTexture* Lightmap, UBOOL UseDiffuse );
	inline void  HandleLightmap_SP( FOpenGLMaterialStateStage& Stage, FOpenGLTexture* Lightmap );
	inline void  HandleDiffuse_Patch( FOpenGLMaterialStateStage& Stage, UBOOL Modulate2X = 0 );
	inline void  HandleDiffuse_SP( FOpenGLMaterialStateStage& Stage );
	inline void  HandleDiffuse_Stage( FOpenGLMaterialStateStage& Stage, UBOOL Modulate2X = 0 );
	inline void  HandleDetail( UBitmapMaterial* DetailBitmap, INT& PassesUsed, INT& StagesUsed, FOpenGLModifierInfo InModifierInfo, UBOOL SinglePassOnly=0 );
	inline void	 HandleTFactor_SP( FOpenGLMaterialStateStage& Stage );
	inline void  HandleTFactor_Patch( FOpenGLMaterialStateStage& Stage );
	BUGGYINLINE void  ApplyTexModifier( FOpenGLMaterialStateStage& Stage, FOpenGLModifierInfo* ModifierInfo );
	inline void  ApplyFinalBlend( FOpenGLModifierInfo* InModifierInfo );};

/*----------------------------------------------------------------------------
	CheckMaterial.
----------------------------------------------------------------------------*/

// Faster material type checking. It uses bit mask comparisons rather than 
// directly using Cast<>(). If the material does not have a valid render hint 
// set, it will use Cast<>() to resolve it. If you can guarantee that each 
// material type will have a bitmask set, you can remove the fallback for 
// improved performance.
template<class TYPE,DWORD MaterialType> FORCEINLINE TYPE* MaterialCast(UMaterial* Material)
{
	if (Material == NULL) return NULL;
	// First check the material type hint for a match. If that fails,
	// check to see if this is an unregistered type. If it is, use the
	// fallback Cast<>() method to determine type. If not just fail.
	return Material->MaterialType & MaterialType ? (TYPE*)Material :
	Material->MaterialType == 0 ? Cast<TYPE>(Material) : NULL;
}

//
// CHECKFALLBACK - return the Fallback material if it's decided we should use it.
//
#define CHECKFALLBACK( InMaterial )		\
			(UseFallbacks && InMaterial ? InMaterial->CheckFallback() : InMaterial)

//
// Attempt to cast a material/modifier chain to the appropriate class.
// If the chain ends with a material of the specfied class, remember any 
// modifier info along the way.
//
template<class C,DWORD MaterialType> C* CheckMaterial(FOpenGLRenderInterface* RI, UMaterial* InMaterial, FOpenGLModifierInfo* ModifierInfo=NULL, UBOOL UseFallbacks=0)
{
	C* Material = MaterialCast<C,MaterialType>(CHECKFALLBACK(InMaterial));
	if( Material )
		return Material;

	// See if we have a chain of Modifiers eventually pointing to material of class C
	UModifier* Modifier = MaterialCast<UModifier,MT_Modifier>(CHECKFALLBACK(InMaterial));
	while( Modifier )
	{
		Material = MaterialCast<C,MaterialType>(CHECKFALLBACK(Modifier->Material));
		Modifier = MaterialCast<UModifier,MT_Modifier>(CHECKFALLBACK(Modifier->Material));
	}

	// If we have a C, go through the Modifier list and combine the matrices for any TexModifiers.
	if( Material && ModifierInfo )
	{
		UBOOL NeedSource = 1;
		Modifier = MaterialCast<UModifier,MT_Modifier>(CHECKFALLBACK(InMaterial));
		while( Modifier )
		{
			// Remember the most specific fallback we see.
			if( Modifier->HasFallback() && UseFallbacks )
				ModifierInfo->BestFallbackPoint = Modifier;

			// Check for TexModifier.
			UTexModifier* TexModifier = MaterialCast<UTexModifier,MT_TexModifier>(Modifier);
			if( TexModifier )
			{
				FMatrix* TexMatrix = TexModifier->GetMatrix(RI->Viewport->Actor->Level->TimeSeconds);
				if( TexMatrix )
				{
					ModifierInfo->ModifyTextureTransforms = 1;
					ModifierInfo->Matrix *= *TexMatrix;
				}
				// Locate the first non-passthrough texture coordinate source.
				if( NeedSource && TexModifier->TexCoordSource!=TCS_NoChange )
				{
					ModifierInfo->ModifyTextureTransforms = 1;
					ModifierInfo->TexCoordSource	= TexModifier->TexCoordSource;
					ModifierInfo->TexCoordCount		= TexModifier->TexCoordCount;
					ModifierInfo->TexCoordProjected	= TexModifier->TexCoordProjected;
					NeedSource = 0;
				}
			}

			// Check for FinalBlend modifier.
			UFinalBlend* FinalBlend = MaterialCast<UFinalBlend,MT_FinalBlend>(Modifier);
			if( FinalBlend ) 	
			{
				ModifierInfo->ModifyFramebufferBlending = 1;

				ModifierInfo->FrameBufferBlending	= FinalBlend->FrameBufferBlending;
				ModifierInfo->ZWrite				= FinalBlend->ZWrite;
				ModifierInfo->ZTest					= FinalBlend->ZTest;
				ModifierInfo->AlphaTest				= FinalBlend->AlphaTest;
				ModifierInfo->TwoSided				= FinalBlend->TwoSided;
				ModifierInfo->AlphaRef				= FinalBlend->AlphaRef;
			}

			// Check for ColorModifier modifier.
			UColorModifier* ColorModifier = MaterialCast<UColorModifier,MT_ColorModifier>(Modifier);
			if( ColorModifier )
			{
				ModifierInfo->ModifyColor	= 1;

				ModifierInfo->AlphaBlend	|= ColorModifier->AlphaBlend;
				ModifierInfo->TwoSided		|= ColorModifier->RenderTwoSided;
				ModifierInfo->TFactorColor	= ColorModifier->Color;
			}

			// Check for an OpacityModifier modifier.
			UOpacityModifier* OpacityModifier = MaterialCast<UOpacityModifier,MT_OpacityModifier>(Modifier);
			if( OpacityModifier )
			{
				ModifierInfo->ModifyOpacity = 1;
				ModifierInfo->Opacity = OpacityModifier->Opacity;
				ModifierInfo->OpacityOverrideTexModifier = OpacityModifier->bOverrideTexModifier;			
			}

			// Move to the next modifier in the chain
			Modifier = MaterialCast<UModifier,MT_Modifier>(CHECKFALLBACK(Modifier->Material));
		}
	}

	return Material;
}

#endif
