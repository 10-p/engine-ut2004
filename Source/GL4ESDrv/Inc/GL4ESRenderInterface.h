/*=============================================================================
	GL4ESRenderInterface.h: GL4ESDrv's rendering interface class (from OpenGLDrv's OpenGLRenderInterface.h).
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#ifndef HEADER_GL4ESRENDERINTERFACE
#define HEADER_GL4ESRENDERINTERFACE

class UGL4ESRenderDevice;


// !!! FIXME: a big round of applause for quailty GNU software!  --ryan.
#ifndef GL4ES_BUGGYINLINE
#ifdef __GNUC__
#define GL4ES_BUGGYINLINE
#else
#define GL4ES_BUGGYINLINE inline
#endif
#endif


//
//	FGL4ESModifierInfo
//
struct FGL4ESModifierInfo
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
	FGL4ESModifierInfo();

	// FGL4ESModifierInfo interface
	void SetDetailTextureScale( FLOAT Scale );
};


//
//	FGL4ESMaterialState
//
class FGL4ESMaterialState
{
public:
	EGL4ESPixelShader		PixelShader;
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
	FGL4ESMaterialStateStage	Stages[8];

	INT	NumRefs;

	FGL4ESMaterialState();
};

//
//	FGL4ESMaterialStatePool
//

class FGL4ESMaterialStatePool
{
public:

	TArray<FGL4ESMaterialState*>	FreeStates;

	// Destructor.

	~FGL4ESMaterialStatePool()
	{
		for(INT StateIndex = 0;StateIndex < FreeStates.Num();StateIndex++)
			delete FreeStates(StateIndex);
	}

	// AllocateState

	FGL4ESMaterialState* AllocateState(FGL4ESMaterialState* DefaultState)
	{
		FGL4ESMaterialState*	Result;

		if(FreeStates.Num())
			Result = FreeStates.Pop();
		else
			Result = new(TEXT("GL4ESMaterialState")) FGL4ESMaterialState();

		if(DefaultState)
			appMemcpy(Result,DefaultState,sizeof(FGL4ESMaterialState));

		Result->NumRefs = 1;

		return Result;
	}

	// FreeState

	void FreeState(FGL4ESMaterialState* State)
	{
		FreeStates.AddItem(State);
	}
};

enum EGL4ESMatrixDirty
{
	GL4ES_MATRIX_Projection	= 1,
	GL4ES_MATRIX_Modelview	= 2
};

#define GL4ES_MAX_STATESTACKDEPTH 128

//
//	FGL4ESRenderInterface
//
class FGL4ESRenderInterface : public FRenderInterface
{
public:

	//
	// FGL4ESSavedState
	//
	class FGL4ESSavedState
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

		FGL4ESVertexShader*	VertexShader;
		FGL4ESVertexStream*	Streams[16];
		INT						StreamStrides[16],
								NumStreams;

		FGL4ESIndexBuffer*		IndexBuffer;
		INT						IndexBufferBase;

		ECullMode				CullMode;

		UBOOL					UseDetailTexturing;
		UBOOL					UseDynamicLighting;
		UBOOL					UseStaticLighting;
		UBOOL					LightingModulate2X;
		UBOOL					LightingOnly;
		FGL4ESTexture*			Lightmap;
		FSphere					LitSphere;
		FColor					AmbientLightColor;

		struct FGL4ESLightState
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

		FGL4ESMaterialState*	MaterialPasses[8];
		INT						NumMaterialPasses;
		FGL4ESMaterialState*	CurrentMaterialState;

		UBOOL					OtherRenderTarget;
		GLuint					RenderTargetSurface;
		GLuint					DepthStencilSurface;

		FGL4ESSavedState();
	};

	// Deferred commit.
	DWORD						LightsDirty,
								MatricesDirty,
								ArraysDirty;
	
	// State stack stuff.
	FGL4ESSavedState			SavedStates[GL4ES_MAX_STATESTACKDEPTH];
	FGL4ESSavedState*			CurrentState;
	FGL4ESMaterialState		DefaultPass;
	INT							SavedStateIndex;

	FGL4ESMaterialStatePool	MaterialStatePool;


	// Variables.
	UGL4ESRenderDevice*		RenDev;
	UViewport*					Viewport;

	EPrecacheMode				PrecacheMode;


	// Constructor.
	FGL4ESRenderInterface(UGL4ESRenderDevice* InRenDev);

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

	// FGL4ESRenderInterface interface.
	void Locked( UViewport* InViewport, BYTE* InHitData,INT* InHitSize);
	void Unlocked();

private:
	void SetMaterialBlending( FGL4ESMaterialState* NewMaterialState );

	FGL4ESTexture* CacheTexture(FBaseTexture* Texture);
	void SetTexture( FGL4ESTexture* Texture );
		
	// Deferred stuff.
	void CommitLights();
	void CommitStreams( INT FirstIndex, INT Pass );
	void CommitMatrices();

	// Various material handlers
	UBOOL SetShaderMaterial( UShader* InShader, FGL4ESModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial );
	UBOOL SetSimpleMaterial( UMaterial* InMaterial, FGL4ESModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial );
	UBOOL SetParticleMaterial( UParticleMaterial* InParticleMaterial, FGL4ESModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial );
	UBOOL SetTerrainMaterial( UTerrainMaterial* InTerrainMaterial, FGL4ESModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial );
	UBOOL SetProjectorMaterial( UProjectorMaterial* ProjectorMaterial, FGL4ESModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial );
	void  SetLightingOnlyMaterial();

	// Helpers
	GL4ES_BUGGYINLINE UBOOL HandleCombinedMaterial( UMaterial* InMaterial, INT& PassesUsed, INT& StagesUsed, FGL4ESModifierInfo ModifierInfo, UBOOL InvertOutputAlpha=0, FString* ErrorString=NULL, UMaterial** ErrorMaterial=NULL );
	GL4ES_BUGGYINLINE void  SetShaderBitmap( FGL4ESMaterialStateStage& Stage, UBitmapMaterial* BitmapMaterial );
	inline void  HandleOpacityBitmap( FGL4ESMaterialStateStage& Stage, UBitmapMaterial* Bitmap, UBOOL ModulateAlpha=0 );
	inline void  HandleVertexOpacity( FGL4ESMaterialStateStage& Stage, UVertexColor* VertexColor );
	inline void  HandleSpecular_SP( FGL4ESMaterialStateStage& Stage, UBitmapMaterial* Bitmap, UBOOL UseSpecularity, UBOOL UseConstantSpecularity, UBOOL ModulateSpecular2X ); // sjs
	inline void  HandleSelfIllumination_SP( FGL4ESMaterialStateStage& Stage, UBitmapMaterial* Bitmap );
	inline void  HandleLighting_MP( FGL4ESMaterialStateStage& Stage, FGL4ESTexture* Lightmap, UBOOL UseDiffuse );
	inline void  HandleLightmap_SP( FGL4ESMaterialStateStage& Stage, FGL4ESTexture* Lightmap );
	inline void  HandleDiffuse_Patch( FGL4ESMaterialStateStage& Stage, UBOOL Modulate2X = 0 );
	inline void  HandleDiffuse_SP( FGL4ESMaterialStateStage& Stage );
	inline void  HandleDiffuse_Stage( FGL4ESMaterialStateStage& Stage, UBOOL Modulate2X = 0 );
	inline void  HandleDetail( UBitmapMaterial* DetailBitmap, INT& PassesUsed, INT& StagesUsed, FGL4ESModifierInfo InModifierInfo, UBOOL SinglePassOnly=0 );
	inline void	 HandleTFactor_SP( FGL4ESMaterialStateStage& Stage );
	inline void  HandleTFactor_Patch( FGL4ESMaterialStateStage& Stage );
	GL4ES_BUGGYINLINE void  ApplyTexModifier( FGL4ESMaterialStateStage& Stage, FGL4ESModifierInfo* ModifierInfo );
	inline void  ApplyFinalBlend( FGL4ESModifierInfo* InModifierInfo );};

/*----------------------------------------------------------------------------
	GL4ESCheckMaterial.
----------------------------------------------------------------------------*/

// Faster material type checking. It uses bit mask comparisons rather than 
// directly using Cast<>(). If the material does not have a valid render hint 
// set, it will use Cast<>() to resolve it. If you can guarantee that each 
// material type will have a bitmask set, you can remove the fallback for 
// improved performance.
template<class TYPE,DWORD MaterialType> FORCEINLINE TYPE* GL4ESMaterialCast(UMaterial* Material)
{
	if (Material == NULL) return NULL;
	// First check the material type hint for a match. If that fails,
	// check to see if this is an unregistered type. If it is, use the
	// fallback Cast<>() method to determine type. If not just fail.
	return Material->MaterialType & MaterialType ? (TYPE*)Material :
	Material->MaterialType == 0 ? Cast<TYPE>(Material) : NULL;
}

//
// GL4ES_CHECKFALLBACK - return the Fallback material if it's decided we should use it.
//
#define GL4ES_CHECKFALLBACK( InMaterial )		\
			(UseFallbacks && InMaterial ? InMaterial->CheckFallback() : InMaterial)

//
// Attempt to cast a material/modifier chain to the appropriate class.
// If the chain ends with a material of the specfied class, remember any 
// modifier info along the way.
//
template<class C,DWORD MaterialType> C* GL4ESCheckMaterial(FGL4ESRenderInterface* RI, UMaterial* InMaterial, FGL4ESModifierInfo* ModifierInfo=NULL, UBOOL UseFallbacks=0)
{
	C* Material = GL4ESMaterialCast<C,MaterialType>(GL4ES_CHECKFALLBACK(InMaterial));
	if( Material )
		return Material;

	// See if we have a chain of Modifiers eventually pointing to material of class C
	UModifier* Modifier = GL4ESMaterialCast<UModifier,MT_Modifier>(GL4ES_CHECKFALLBACK(InMaterial));
	while( Modifier )
	{
		Material = GL4ESMaterialCast<C,MaterialType>(GL4ES_CHECKFALLBACK(Modifier->Material));
		Modifier = GL4ESMaterialCast<UModifier,MT_Modifier>(GL4ES_CHECKFALLBACK(Modifier->Material));
	}

	// If we have a C, go through the Modifier list and combine the matrices for any TexModifiers.
	if( Material && ModifierInfo )
	{
		UBOOL NeedSource = 1;
		Modifier = GL4ESMaterialCast<UModifier,MT_Modifier>(GL4ES_CHECKFALLBACK(InMaterial));
		while( Modifier )
		{
			// Remember the most specific fallback we see.
			if( Modifier->HasFallback() && UseFallbacks )
				ModifierInfo->BestFallbackPoint = Modifier;

			// Check for TexModifier.
			UTexModifier* TexModifier = GL4ESMaterialCast<UTexModifier,MT_TexModifier>(Modifier);
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
			UFinalBlend* FinalBlend = GL4ESMaterialCast<UFinalBlend,MT_FinalBlend>(Modifier);
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
			UColorModifier* ColorModifier = GL4ESMaterialCast<UColorModifier,MT_ColorModifier>(Modifier);
			if( ColorModifier )
			{
				ModifierInfo->ModifyColor	= 1;

				ModifierInfo->AlphaBlend	|= ColorModifier->AlphaBlend;
				ModifierInfo->TwoSided		|= ColorModifier->RenderTwoSided;
				ModifierInfo->TFactorColor	= ColorModifier->Color;
			}

			// Check for an OpacityModifier modifier.
			UOpacityModifier* OpacityModifier = GL4ESMaterialCast<UOpacityModifier,MT_OpacityModifier>(Modifier);
			if( OpacityModifier )
			{
				ModifierInfo->ModifyOpacity = 1;
				ModifierInfo->Opacity = OpacityModifier->Opacity;
				ModifierInfo->OpacityOverrideTexModifier = OpacityModifier->bOverrideTexModifier;			
			}

			// Move to the next modifier in the chain
			Modifier = GL4ESMaterialCast<UModifier,MT_Modifier>(GL4ES_CHECKFALLBACK(Modifier->Material));
		}
	}

	return Material;
}

#endif
