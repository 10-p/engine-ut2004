/*=============================================================================
	PixoRenderInterface.h: Unreal Pixomatic rendering interface definition.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Michael Sartain from GL driver
		* Taken over by Daniel Vogel

=============================================================================*/

#ifndef HEADER_PIXORENDERINTERFACE
#define HEADER_PIXORENDERINTERFACE

class UPixoRenderDevice;


//
//  FPixoModifierInfo
//
struct FPixoModifierInfo
{
	UBOOL               ModifyTextureTransforms;
	UBOOL               ModifyFramebufferBlending;
	UBOOL               ModifyColor;
	UBOOL               ModifyOpacity;

	// texture modifier
	FMatrix             Matrix;
	BYTE                TexCoordSource;
	BYTE                TexCoordCount;
	UBOOL               TexCoordProjected;

	// framebuffer blend modifier
	BYTE                FrameBufferBlending;
	UBOOL               ZWrite;
	UBOOL               ZTest;
	UBOOL               AlphaTest;
	UBOOL               TwoSided;
	BYTE                AlphaRef;

	// color modifier
	FColor              TFactorColor;
	UBOOL               AlphaBlend;

	// fallback info
	UMaterial*          BestFallbackPoint;

	// opacity modifier
	UMaterial*          Opacity;
	UBOOL               OpacityOverrideTexModifier;

	// Constructor
	FPixoModifierInfo();

	// FPixoModifierInfo interface
	void SetDetailTextureScale( FLOAT Scale );
};


//
//  FPixoMaterialState
//
class FPixoMaterialState
{
public:
//  EPixelShader            PixelShader;
	UBOOL                   AlphaBlending;          // Alpha blending is enabled
	UBOOL                   AlphaTest;              // Alpha test is enabled?
	BYTE                    AlphaRef;               // If alpha testing, the value to compare against
	UBOOL                   ZTest;                  // Test zbuffer
	UBOOL                   ZWrite;                 // Write to zbuffer
	UBOOL                   TwoSided;
	EFillMode               FillMode;               // Wireframe, flatshaded or solid
	FColor                  TFactorColor;
	DWORD                   SrcBlend,
							DestBlend;
	UBOOL                   OverrideFogColor;
	FColor                  OverriddenFogColor;
	UBOOL					PatchLighting;

	INT                     StagesUsed;
	//!!vogel: even though Pixo only supports two stages we have to keep three around as the material state code
	// will access the third one with certain usage patterns before it notices that it's using too many stages.
	FPixoMaterialStateStage Stages[3];

	UBOOL                   StagesHasTFactor;
	UBOOL                   StagesHasDiffuse;

	INT                     NumRefs;

	FPixoMaterialState();
};

//
//  FPixoMaterialStatePool
//

class FPixoMaterialStatePool
{
public:

	TArray<FPixoMaterialState*> FreeStates;

	// Destructor.

	~FPixoMaterialStatePool()
	{
		for(INT StateIndex = 0;StateIndex < FreeStates.Num();StateIndex++)
			delete FreeStates(StateIndex);
	}

	// AllocateState

	FPixoMaterialState* AllocateState(FPixoMaterialState* DefaultState)
	{
		FPixoMaterialState* Result;

		if(FreeStates.Num())
			Result = FreeStates.Pop();
		else
			Result = new(TEXT("PixoMaterialState")) FPixoMaterialState();

		if(DefaultState)
			appMemcpy(Result,DefaultState,sizeof(FPixoMaterialState));

		Result->NumRefs = 1;

		return Result;
	}

	// FreeState

	void FreeState(FPixoMaterialState* State)
	{
		FreeStates.AddItem(State);
	}
};


#define MAX_STATESTACKDEPTH 128

enum
{
    PIXO_LIGHT_NONE                             = 0x00000000,
    PIXO_LIGHT_DIRECTIONAL                      = 0x00000001,
    PIXO_LIGHT_POINT                            = 0x00000002,
    PIXO_LIGHT_POINT_QUADRATIC_NON_INCIDENCE    = 0x80000002,
};

struct FPixoLightState
{
	INT         Type;

    // Directional
    FVector     Direction;
    FVector     Diffuse;

    // Point
    FVector     Position;
    float       Radius;
};


//
//  FPixoRenderInterface
//
class FPixoRenderInterface : public FRenderInterface
{
public:

	//
	// FPixoSavedState
	//
	class FPixoSavedState
	{
	public:
        __declspec(align(16)) FMatrix LocalToWorld GCC_ALIGN(16);
        __declspec(align(16)) FMatrix WorldToCamera GCC_ALIGN(16);
        __declspec(align(16)) FMatrix CameraToScreen GCC_ALIGN(16);
        __declspec(align(16)) FMatrix LocalToWorldInverse GCC_ALIGN(16);
        UBOOL                   LocalToWorldDirty;

		void*					OtherFrameBuffer;
		void*					OtherZBuffer;
		INT						OtherFrameBufferPitch,
								OtherZBufferPitch;

		const TCHAR             *MaterialName;

		INT                     ViewportX,
								ViewportY,
								ViewportWidth,
								ViewportHeight;

		INT                     ZBias;

		FPixoVertexShader       *VertexShader;
		FPixoVertexStream       *Streams[16];
		INT                     StreamStrides[16],
								NumStreams;

		FPixoIndexBuffer*       IndexBuffer;
		INT                     IndexBufferBase;

		ECullMode               CullMode;

		UBOOL                   UseDetailTexturing;
		UBOOL                   UseDynamicLighting;
		UBOOL                   UseStaticLighting;
		UBOOL                   LightingModulate2X;
		UBOOL                   LightingOnly;
		FPixoTexture*           Lightmap;
		FSphere                 LitSphere;
		FColor                  AmbientLightColor;

        INT                     LightsEnabled;
		FPixoLightState         Lights[8];
		UBOOL                   LightsDirty;

		UBOOL                   ArraysDirty;

		UBOOL                   HasDiffuse;

		UBOOL                   DistanceFogEnabled;
		FColor                  DistanceFogColor;
		FLOAT                   DistanceFogStart,
								DistanceFogEnd;

		UBOOL                   ZWrite,
								ZTest,
								AlphaTest;
		INT                     AlphaRef;

		FPixoMaterialState      *MaterialPasses[8];
		INT                     NumMaterialPasses;
		FPixoMaterialState      *CurrentMaterialState;

		FPixoSavedState();
	};


	// State stack stuff.
	FPixoSavedState				*SavedStates;
	FPixoSavedState             *CurrentState;
	FPixoMaterialState          DefaultPass;
	INT                         SavedStateIndex;

	static FPixoSavedState      *GCurrentDrawPrimitiveState;

	FPixoMaterialStatePool      MaterialStatePool;


	// Variables.
	UPixoRenderDevice           *RenDev;
	UViewport                   *Viewport;

	EPrecacheMode               PrecacheMode;
	DWORD						PixoHint;

	// Constructor/ Destructor.
	FPixoRenderInterface(UPixoRenderDevice* InRenDev);
	~FPixoRenderInterface();

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

	// Pixo helpers.
	virtual void PixoSetHint(DWORD Hint);
	virtual void PixoResetHint(DWORD Hint);
	virtual UTexture* PixoCreateTexture( FRenderTarget* RenderTarget, UBOOL CreateMips );

	// sjs ---
	virtual int LockDynBuffer(BYTE** pOutBuffer, int numVerts, int stride, DWORD componentFlags);
	virtual int UnlockDynBuffer( void );
	virtual void DrawDynQuads(INT NumPrimitives);
	virtual void DrawQuads(INT FirstVertex, INT NumPrimitives);
	// --- sjs

	// FPixoRenderInterface interface.
	void Locked( UViewport* InViewport, BYTE* InHitData,INT* InHitSize);
	void Unlocked();

private:
	void SetMaterialBlending( FPixoMaterialState* NewMaterialState );
	void SetPixoBlendModes( FPixoMaterialState* NewMaterialState, UBOOL StageUseTexture[2] );

	FPixoTexture* CacheTexture(FBaseTexture* Texture);
	void SetTexture( int Stage, FPixoTexture* Texture );

	// Deferred stuff.
	UBOOL CommitLights();
    void CommitStreams( INT FirstIndex );
    void SetTexCoordIndices( INT Pass );

	// Various material handlers
	UBOOL SetShaderMaterial( UShader* InShader, FPixoModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial );
	UBOOL SetSimpleMaterial( UMaterial* InMaterial, FPixoModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial );
	UBOOL SetParticleMaterial( UParticleMaterial* InParticleMaterial, FPixoModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial );
	UBOOL SetTerrainMaterial( UTerrainMaterial* InTerrainMaterial, FPixoModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial );
	UBOOL SetProjectorMaterial( UProjectorMaterial* ProjectorMaterial, FPixoModifierInfo InModifierInfo, FString* ErrorString, UMaterial** ErrorMaterial );
	void  SetLightingOnlyMaterial();

	// Helpers
	inline UBOOL HandleCombinedMaterial( UMaterial* InMaterial, INT& PassesUsed, INT& StagesUsed, FPixoModifierInfo ModifierInfo, UBOOL InvertOutputAlpha=0, FString* ErrorString=NULL, UMaterial** ErrorMaterial=NULL );
	inline void  SetShaderBitmap( FPixoMaterialStateStage& Stage, UBitmapMaterial* BitmapMaterial );
	inline void  HandleOpacityBitmap( FPixoMaterialStateStage& Stage, UBitmapMaterial* Bitmap, UBOOL ModulateAlpha=0 );
	inline void  HandleVertexOpacity( FPixoMaterialStateStage& Stage, UVertexColor* VertexColor );
	inline void  HandleSpecular_SP( FPixoMaterialStateStage& Stage, UBitmapMaterial* Bitmap, UBOOL UseSpecularity, UBOOL UseConstantSpecularity, UBOOL ModulateSpecular2X ); // sjs
	inline void  HandleSelfIllumination_SP( FPixoMaterialStateStage& Stage, UBitmapMaterial* Bitmap );
	inline void  HandleLighting_MP( FPixoMaterialStateStage& Stage, FPixoTexture* Lightmap, UBOOL UseDiffuse );
	inline void  HandleLightmap_SP( FPixoMaterialStateStage& Stage, FPixoTexture* Lightmap );
	inline void  HandleDiffuse_Patch( FPixoMaterialStateStage& Stage, UBOOL Modulate2X = 0 );
	inline void  HandleDiffuse_SP( FPixoMaterialStateStage& Stage );
	inline void  HandleDiffuse_Stage( FPixoMaterialStateStage& Stage, UBOOL Modulate2X = 0 );
	inline void  HandleDetail( UBitmapMaterial* DetailBitmap, INT& PassesUsed, INT& StagesUsed, FPixoModifierInfo InModifierInfo, UBOOL SinglePassOnly=0 );
	inline void  HandleTFactor_SP( FPixoMaterialStateStage& Stage );
	inline void  HandleTFactor_Patch( FPixoMaterialStateStage& Stage );
	inline void  ApplyTexModifier( FPixoMaterialStateStage& Stage, FPixoModifierInfo* ModifierInfo );
	inline void  ApplyFinalBlend( FPixoModifierInfo* InModifierInfo );};

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
	template<class C,DWORD MaterialType> C* CheckMaterial(FPixoRenderInterface* RI, UMaterial* InMaterial, FPixoModifierInfo* ModifierInfo=NULL, UBOOL UseFallbacks=0)
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
