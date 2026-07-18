//=============================================================================
//
// ParticleEditorComponent.h	
// Copyright(c) 2002 Demiurge Studios. All Rights Reserved.
// 
//=============================================================================

#ifndef _PARTICLE_EDITOR_COMPONENT_H
#define _PARTICLE_EDITOR_COMPONENT_H


typedef enum _LabelType
{
	LABEL_None,
	LABEL_Normal,
	LABEL_Expand,
	LABEL_ExpandClosed
} ToolLabelType;



	
/**
 * Base abstract component class for manipulating the UParticleEmitter
 * WParticleEditorComponents are containers which group WParticleEditorTools into useful Tools
 * Each one of these has a checkbox in the left column of the tab to turn it off and on
 */
class WParticleEditorComponent : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WParticleEditorComponent,WPropertyPage,Window)

public:

	/**
	 * Constructor
	 */
	WParticleEditorComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * "Destructor"
	 */
	virtual void OnDestroy();

	/**
	 * Returns the Name of this component
	 */
	const FString GetComponentName();

	/**
	 * returns the size of this component dynamically based on which Tools are open
	 */
	FPoint GetSize() const;

	/**
	 * Sets the positions of all the Tools based on if they are Shown or not
	 */
	void SetToolAnchors() { _SetToolAnchors(FALSE); }

	/**
	 * Sets the Tool with the given index to the given shownnes
	 */
	void ShowTool(INT ToolIndex, UBOOL Shown);

	/**
	 * Sets the Tool with the given index to the given shownnes
	 */
	void ShowTool(WParticleEditorTool *ToolToShow, UBOOL Shown);

	/**
	 * Tells the Tools that are listening in this component to get their values from the particle Emitter
	 * if ForceUpdateAll is TRUE, it updates all the Tools
	 */
	void RereadToolValues(UBOOL ForceUpdateAll = FALSE);

	/**
	 * Returns the Emitter this component is editing.
	 */
	UParticleEmitter* GetEditTarget() {return EditTarget;}

	 // WWindow Interface
	virtual void OnCreate();
	virtual void OnSize( DWORD Flags, INT NewX, INT NewY );

	WParticleEditorTab* GetParentTab() const {return ParentTab;}

	/**
	 * Sets the parent tab for this component.  This includes setting the windows parents
	 * and setting the edit target correctly.
	 */
	void SetParentTab(WParticleEditorTab* InParentTab);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

	TArray<INT> GetExpandState();
	TArray<DWORD> GetToolState();
	void SetExpandState( TArray<INT> ExpandState );
	void SetToolState( TArray<DWORD> ToolState );


protected:

	/**
	 * Creates and adds all the Tools to this component.  This is called from OnCreate.
	 * Subclasses should override this function to add specific types of Tools but
	 * should make sure to call the super
	 */
	virtual void CreateTools();

	/**
	 * Adds the given Tool with the given Label type.
	 * If AddDown is TRUE, the component is added below the last component, if not,
	 * the component is added in the same place and must rely on well set offsets to make it so that it does not
	 * interfere with the existing component.  Offset, can also be used with AddDown==TRUE to
	 * format more specifically
	 */
	void AddTool(class WParticleEditorTool* NewTool, ToolLabelType LabelType, UBOOL AddDown = TRUE, INT xOffset = 0, INT yOffset = 0);

	/**
	 * The Emitter this component is editing.
	 */
	class UParticleEmitter* EditTarget;

	/**
	 * the tab this component is contained it
	 */
	class WParticleEditorTab* ParentTab;

	/**
	 * The default Location for the second column of controls
	 */
	const INT SecondColumnStart;

	/**
	 * The size of the indendation of the Tool controls from the left margin
	 */
	const INT ToolIndentSize;

	/**
	 * A consistent amount to indent Tools by - used for things like range vectos
	 */
	const INT ExtraIndentSize;

	/**
	 * A consistent amount to vertically separate Tools if one should choose to do that
	 */
	const INT ExtraSpaceBetweenTools;

	/**
	 * Indents Tools and their Labels
	 */
	const INT IndentSize;

private:
	
	//variables

	typedef struct _ToolInfo
	{
		UBOOL Shown;
		class WParticleEditorTool* Tool;
		class WParticleToolLabel* Label;
		ToolLabelType LabelType;
		UBOOL AddDown;
		INT xOffset;
		INT yOffset;
	} ToolInfo;

	/**
	 * The ordered list of the set of Tools this component has
	 */
	TArray<ToolInfo> Tools;

	/**
	 * the Name of this component
	 */
	FString ComponentName;

	/** 
	 * The Box that is drawn around this component
	 */
	WGroupBox *Box;

	/** Controls the Location of everything.*/
	TMap<DWORD,FWindowAnchor> Anchors;

	/** Works with Anchors to control the Location of things */
	FContainer *Container;

	//vars for alignment
	const INT InitialVertOffset;
	const INT SpaceBetweenTools;



	//functions

	/**
	 * Sets the positions of all the Tools based on if they are Shown or not
	 */
	INT _SetToolAnchors(UBOOL OnlyCalcSize = FALSE);

	/**
	 * Update the position of the container and what it contains
	 */
	void PositionChildControls();

};


/**
 * Component for manipulated motion with things like velocity and acceleration
 */
class WParticleGeneralComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleGeneralComponent,WParticleEditorComponent,Window)

public:
	/** Constructor */
	WParticleGeneralComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:
	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WBoolEdit *Disable;
	WNoNegNumberEdit *MaxParticles;
	WNameTool *Name;
	WBoolEdit *RespawnDeadParticles;
	WFloatEdit *ParticlesPerSecond;
	WEnableEdit *AutomaticSpawning;
	WScaleEmitterEdit *ScaleEmitter;
	WSpeedScaleEdit *ScaleSpeed;
};




/**
 * Component for manipulated motion with things like velocity and acceleration
 */
class WParticleRenderingComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleRenderingComponent,WParticleEditorComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleRenderingComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:
	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WBoolEdit *DisableFogging;
	WBoolEdit *AlphaTest;	
	WIntEdit *AlphaRef;
	WBoolEdit *Z_Test;
	WBoolEdit *Z_Write;
};



/**
 * Component for manipulated motion with things like velocity and acceleration
 */
class WParticleMovementComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleMovementComponent,WParticleEditorComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleMovementComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:
	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WRangeVectorEdit *StartVelocity;
	WXYZVectorEdit *Acceleration;
	WEnumEdit *CoordinateSystem;
	WRangeVectorEdit *VelocityLoss;
	WXYZVectorEdit *MaxVelocity;
	WFloatEdit *MinSquaredVelocity;
	WEnumEdit *AddVelocityFrom;
	WRangeVectorEdit *AddVelocityMult;
	WEnumEdit *GetVelocityDir;
	WRangeEdit *StartVelocityRadial;

};




/**
 * Component for manipulated Location with things like StartlocationRanage
 */
class WParticleTimeComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleTimeComponent,WParticleEditorComponent,Window)

public:
	/** Constructor */
	WParticleTimeComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:
	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WRangeEdit *Lifetime;
	WRangeEdit *InitialTime;
	WRangeEdit *InitialDelay;
	WFloatEdit *SecondsBeforeInactive;
	WFloatEdit *RelativeWarmupTime;
	WFloatEdit *WarmupTicksPerSecond;
};




/**
 * Component for manipulated Location with things like StartlocationRanage
 */
class WParticleLocationComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleLocationComponent,WParticleEditorComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleLocationComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:

	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WEnumEdit *StartLocationShape;
	WRangeVectorEdit *StartLocationBox;
	WRangeEdit *StartLocationSphereRadius;
	WRangeVectorEdit *StartLocationPolar;
	WXYZVectorEdit *StartLocationOffset;
	WEnumEdit *AddLocationFromOther;
};




/**
 * Component for manipulated texture
 */
class WParticleTextureComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleTextureComponent,WParticleEditorComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleTextureComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:

	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WTexturePickEdit *TexturePick;
	WEnumEdit *DrawStyle;
	WIntEdit *NumberUSub;
	WIntEdit *NumberVSub;
	WEnableEdit *BlendDetweenSub;
	WEnableEdit *UseRandomSub;
	WEnableEdit *UseSubScale;
	WParticleFloatArrayTool *SubScale;
	WIntEdit *SubStart;
	WIntEdit *SubEnd;
};




/**
 * Component for manipulated texture
 */
class WParticleMeshTextureComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleMeshTextureComponent,WParticleEditorComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleMeshTextureComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:

	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WParticleTextureArrayTool *CustomTextureSet;
	WEnumEdit *DrawStyle;
	WIntEdit *NumberUSub;
	WIntEdit *NumberVSub;
	WEnableEdit *BlendDetweenSub;
	WEnableEdit *UseRandomSub;
	WEnableEdit *UseSubScale;
	WParticleFloatArrayTool *SubScale;
	WIntEdit *SubStart;
	WIntEdit *SubEnd;
};




/**
 * Component for manipulating color and fading
 */
class WParticleFadingColorComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleFadingColorComponent,WParticleEditorComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleFadingColorComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:
	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WPercentEdit *Opacity;
	WFadingEdit *Fading;
	WColorMultEdit *FadeOutFactor;
	WColorMultEdit *FadeInFactor;
	WRangeColorEdit *ColorMultiplier;
	WEnableEdit *UseColorScale;
	WColorScaleEdit *ColorScale;
	WFloatEdit *ColorScaleRepeats;	
};




/**
 * Component for manipulating rotation of particles
 */
class WParticleBasicRotationComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleBasicRotationComponent,WParticleEditorComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleBasicRotationComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:

	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WEnumEdit *UseRotationFrom;
	WRotatorEdit *RotationOffset;
	WXYZVectorEdit *RotationNormal;

};




/**
 * Component for manipulating rotation of particles
 */
class WParticleSpriteRotationComponent : public WParticleBasicRotationComponent
{
	DECLARE_WINDOWCLASS(WParticleSpriteRotationComponent,WParticleBasicRotationComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleSpriteRotationComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:

	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WRangeEdit *StartSpin;
	WRangeEdit *SpinsPerSecond;
	WPercentEdit *SpinCCW;
	WEnableEdit *SpinParticles;
	WEnumEdit *FacingDirection;
	WXYZVectorEdit *ProjectionNormal;
};




/**
 * Component for manipulating rotation of particles
 */
class WParticleMeshRotationComponent : public WParticleBasicRotationComponent
{
	DECLARE_WINDOWCLASS(WParticleMeshRotationComponent,WParticleBasicRotationComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleMeshRotationComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:

	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WRangeRotatorEdit *StartSpin;
	WRangeRotatorEdit *SpinsPerSecond;
	WXYZVectorEdit *SpinCCW;	
	WEnableEdit *SpinParticles;			
};




/**
 * Component for manipulating size of particles
 */
class WParticleSizeComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleSizeComponent,WParticleEditorComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleSizeComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:
	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WBoolEdit *UniformSize;
	WRangeVectorEdit *StartSize;
	WEnableEdit *UseSizeScale;
	WEnableEdit *ShrinkParticlesEx;
	WFloatEdit *SizeScaleRepeats;
	WParticleSizeScaleArrayTool *SizeScale;
};




/**
 * Component for manipulating the collision info of particles
 */
class WParticleCollisionComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleCollisionComponent,WParticleEditorComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleCollisionComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:

	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WBoolEdit *UseActorForces;
	WEnableEdit *UseCollision;					
	WXYZVectorEdit *ExtentMultiplier;			
	WRangeVectorEdit *DampingFactor;			
	WRangeRotatorEdit *RotationDampingFactor;	
	WEnableEdit *UseCollisionPlanes;			
	WParticlePlaneArrayTool *CollisionPlanes;	
	WEnableEdit *UseMaxCollisions;				
	WRangeEdit *MaxCollisions;					
	WEnumEdit *SpawnFromOtherEmitter;			
	WIntEdit *SpawnAmount;						
	WEnableEdit *UseSpawnedVelocityScale;		
	WRangeVectorEdit *SpawnedVelocityScale;		
	WParticleSoundArrayTool *CollisionSoundArray;
	WEnumEdit *CollisionSound;					
	WRangeEdit *CollisionSoundIndex;			
	WRangeEdit *CollisionSoundProbability;
	WEnableEdit *UseRotationDamp;
};




/**
 * Component for manipulating Beam
 */
class WParticleBeamComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleBeamComponent,WParticleEditorComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleBeamComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:

	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WIntEdit *NumberofBeamPlanes;	
	WFloatEdit *BeamTextureUScale;	
	WFloatEdit *BeamTextureVScale;	
	WEnumEdit *DetermineEndPointBy;
	WRangeEdit *BeamDistance;
	WParticleBeamEndPointArrayTool *BeamEndPoints;
};





/**
 * Component for manipulating Beam Noise
 */
class WParticleBeamNoiseComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleBeamNoiseComponent,WParticleEditorComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleBeamNoiseComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:

	/**
	 * Creates Tools that manipulate things like HighFrequencyNoiseRange
	 */
	virtual void CreateTools();

private:

	WRangeVectorEdit *LowFrequencyNoise;					 
	WIntEdit *LowFrequencyPoints;							
	WRangeVectorEdit *HighFrequencyNoise;					 
	WIntEdit *HighFrequencyPoints;							 
	WEnableEdit *UseHighFrequencyScale;
	WParticleBeamScaleArrayTool *HighFrequencyScaleFactors;
	WFloatEdit *HighFrequencyScaleRepeats;					 
	WEnableEdit *UseLowFrequencyScale;
	WParticleBeamScaleArrayTool *LowFrequencyScaleFactors;
	WFloatEdit *LowFrequencyScaleRepeats;					 
	WBoolEdit *NoiseDeterminesEndPoint;						 
};




/**
 * Component for manipulating Beam Branching
 */
class WParticleBeamBranchingComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleBeamBranchingComponent,WParticleEditorComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleBeamBranchingComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:

	/**
	 * Creates Tools that manipulate things like HighFrequencyNoiseRange
	 */
	virtual void CreateTools();

private:

	WEnableEdit *UseBranching;
	WRangeEdit *BranchProbability;
	WEnumEdit *BranchEmitter;
	WRangeEdit *BranchSpawnAmount;
	WBoolEdit *LinkupLifetime;
};




/**
 * Component for manipulating Mesh Properties
 */
class WParticleMeshComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleMeshComponent,WParticleEditorComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleMeshComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:

	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WMeshPickEdit *Mesh;
	WBoolEdit *UseMeshBlendMode;	
	WBoolEdit *RenderTwoSided;
	WBoolEdit *UseParticleColor;

};



/**
 * Component for manipulating Spark Properties
 */
class WParticleSparkComponent : public WParticleEditorComponent
{
	DECLARE_WINDOWCLASS(WParticleSparkComponent,WParticleEditorComponent,Window)

public:

	/**
	 * Constructor
	 */
	WParticleSparkComponent(WWindow* InOwnerWindow, class WParticleEditorTab *InParentTab, class UParticleEmitter* InEditTarget, FString Name);

	/**
	 * Links this component and all the tools to the new emitter
	 */
	virtual void LinkToolsToEmitter(class UParticleEmitter* EditTarget);

protected:

	/**
	 * Creates Tools that manipulate things
	 */
	virtual void CreateTools();

private:

	WRangeEdit *LineSegment;
	WRangeEdit *TimeBeforeVisible;
	WRangeEdit *TimeBetweenSegments;
};










#endif

