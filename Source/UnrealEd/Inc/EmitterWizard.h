//=============================================================================
//
// EmitterWizard.h	
// Copyright(c) 2002 Demiurge Studios. All Rights Reserved.
// 
//=============================================================================

#ifndef _EMITTER_WIZARD_H
#define _EMITTER_WIZARD_H

#include "UnrealEd.h"

/**
 * A wizard for creating new emitters.
 * Contains only the Next, Back and Cancel buttons and a list of WEmitterWizardPages.
 * This wizard is a simple to create new emitters by helping set up many of the common 
 * variables automatically.
 */
class WEmitterWizard : public WDialog
{

	DECLARE_WINDOWCLASS(WEmitterWizard,WDialog,UnrealEd)

public:

	/** 
	 * Constructor
	 * @param InEditTarget the particle system to add the new emitter to
	 */
	WEmitterWizard(WWindow* inOwnerWindow, class AEmitter* InEditTarget);

	/**
	 * WWindows Interface
	 */
	virtual void OnCommand(INT Command);
	virtual void OnDestroy();

	AEmitter* GetEditTarget() const {return EditTarget;}
	UParticleEmitter* GetNewEmitter() const {return NewParticleEmitter;}

	/**
	 * WDialog interface
	 */
	virtual void OnInitDialog();
	INT DoModal();

private:

	// This is a map of page names to pages. 
	typedef TMap<FString, class WEmitterWizardPage*> PageMap;
	PageMap Pages;

	/**
	 * The particle system being edited by this wizard
	 */
	AEmitter* EditTarget;

	/**
	 * The emitter being created by this wizard
	 */
	UParticleEmitter* NewParticleEmitter;

	/**
	 * Creates all of the wizard pages and puts them in the Pages array
	 */
	void InitWizardPages();

	WButton NextButton;
	void OnNext();
	WButton BackButton;
	void OnBack();
	WButton CancelButton;
	void OnCancel();

	WEmitterWizardPage* CurrentPage;
	FString CurrentPageName;

	/**
	 * Looks at the state of the wizard and updates the next and back buttons accordingly
	 */
	void UpdateButtonState();

	/**
	 * Opens the page with the specified name
	 */
	void OpenPage(FString PageName);

	TArray<FString> History;

friend class WNewEmitterWP;
};




/**
 * A single set of controls in a WEmitterWizard. 
 */
class WEmitterWizardPage : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WEmitterWizardPage,WPropertyPage,Window)

public:

	/** 
	 * Constructor
	 * @param InEditTarget the particle system to add the new emitter to
	 */
	WEmitterWizardPage(WEmitterWizard* InOwnerWindow, INT ResID);

	/**
	 * WWindows Interface
	 */
	virtual void OnCommand(INT Command);
	virtual void OpenWindow();

	/**
	 * Called when the page is closed either via a next, back or finish button click
	 * @return the name of the next page
	 */
	virtual FString NotifyLeave();

	UParticleEmitter* GetEditTarget() const {return EditTarget;}
	void SetEditTarget(UParticleEmitter* InEditTarget) {EditTarget = InEditTarget;}

protected:
	
	/**
	 * The emitter being edited by this wizard
	 */
	UParticleEmitter* EditTarget;

	/**
	 * Utility function to simulate radio buttons
	 * Specify a bunch of check boxes to the same ID and one and only one of these will be checked
	 */
	void AddRadioButtonHack(WCheckBox* CheckBox, INT ID);


private:


	/**
	 * Maps the IDs and check-boxes from the previous function to one-another
	 */
	typedef TMultiMap<INT, WCheckBox*> RadioButtonMap;
	RadioButtonMap RadioButtons;

	/**
	 * The dialog ID
	 */ 
	INT ResID;
	
};




/** 
 * Intro Wizard page
 */
class WStartWP : public WEmitterWizardPage
{
	DECLARE_WINDOWCLASS(WStartWP,WEmitterWizardPage,UnrealEd)

public:

	/** 
	 * Constructor
	 * @see WEmitterWizard page for param info
	 */
	WStartWP(WEmitterWizard* InOwnerWindow);
	
	/**
	 * WWindows Interface
	 */ 
	virtual void OpenWindow();

	/**
	 * WEmitterWizardPage Interface
	 */
	virtual FString NotifyLeave();
};




/** 
 * Exit Wizard page
 */
class WEndWP : public WEmitterWizardPage
{
	DECLARE_WINDOWCLASS(WEndWP,WEmitterWizardPage,UnrealEd)

public:

	/** 
	 * Constructor
	 * @see WEmitterWizard page for param info
	 */
	WEndWP(WEmitterWizard* InOwnerWindow);
	
	/**
	 * WWindows Interface
	 */ 
	virtual void OpenWindow();

	/**
	 * WEmitterWizardPage Interface
	 */
	virtual FString NotifyLeave();
};




/** 
 * Wizard page that picks the emitter type
 */
class WNewEmitterWP : public WEmitterWizardPage
{
	DECLARE_WINDOWCLASS(WNewEmitterWP,WEmitterWizardPage,UnrealEd)

public:

	/** 
	 * Constructor
	 * @see WEmitterWizard page for param info
	 */
	WNewEmitterWP(WEmitterWizard* InOwnerWindow);
	
	/**
	 * WWindows Interface
	 */ 
	virtual void OpenWindow();
	virtual void OnDestroy();

	/**
	 * WEmitterWizardPage Interface
	 */
	virtual FString NotifyLeave();

private:

	WCheckBox* Sprite;
	WCheckBox* Spark;
	WCheckBox* Mesh;
	WCheckBox* Beam;

	HBITMAP SpriteBitmap;
	HBITMAP SparkBitmap;
	HBITMAP MeshBitmap;
	HBITMAP BeamBitmap;

	WEdit* EmitterName;
};




/** 
 * Wizard page that picks the shape of the emitter
 */
class WShapeWP : public WEmitterWizardPage
{
	DECLARE_WINDOWCLASS(WShapeWP,WEmitterWizardPage,UnrealEd)

public:

	/** 
	 * Constructor
	 * @see WEmitterWizard page for param info
	 */
	WShapeWP(WEmitterWizard* InOwnerWindow);
	
	/**
	 * WWindows Interface
	 */ 
	virtual void OpenWindow();
	virtual void OnDestroy();

	/**
	 * WEmitterWizardPage Interface
	 */
	virtual FString NotifyLeave();

private:

	WCheckBox* Point;
	WCheckBox* Box;
	WCheckBox* Sphere;
	WCheckBox* PlaneHoriz;
	WCheckBox* PlaneVert;

	HBITMAP PointBitmap;
	HBITMAP BoxBitmap;
	HBITMAP SphereBitmap;
	HBITMAP PlaneHorizBitmap;
	HBITMAP PlaneVertBitmap;

	WCheckBox* Small;
	WCheckBox* Med;
	WCheckBox* Large;
};



/** 
 * Wizard page that picks the shape of the emitter
 */
class WMovementWP : public WEmitterWizardPage
{
	DECLARE_WINDOWCLASS(WMovementWP,WEmitterWizardPage,UnrealEd)

public:

	/** 
	 * Constructor
	 * @see WEmitterWizard page for param info
	 */
	WMovementWP(WEmitterWizard* InOwnerWindow);
	
	/**
	 * WWindows Interface
	 */ 
	virtual void OpenWindow();
	virtual void OnDestroy();

	/**
	 * WEmitterWizardPage Interface
	 */
	virtual FString NotifyLeave();

private:

	WCheckBox* Parallel;
	WCheckBox* Spread;
	WCheckBox* Sphere;

	WCheckBox* Up;
	WCheckBox* Horiz;
	WCheckBox* Down;

	WCheckBox* SpeedHeading;
	WCheckBox* Still;
	WCheckBox* Slow;
	WCheckBox* Medium;
	WCheckBox* Fast;
	WCheckBox* VeryFast;

	HBITMAP ParallelBitmap;
	HBITMAP SpreadBitmap;
	HBITMAP SphereBitmap;

	HBITMAP UpBitmap;
	HBITMAP HorizBitmap;
	HBITMAP DownBitmap;
};



/** 
 * Wizard page that picks the texture and fading of the emitter
 */
class WTextureWP : public WEmitterWizardPage
{
	DECLARE_WINDOWCLASS(WTextureWP,WEmitterWizardPage,UnrealEd)

public:

	/** 
	 * Constructor
	 * @see WEmitterWizard page for param info
	 */
	WTextureWP(WEmitterWizard* InOwnerWindow);
	
	/**
	 * WWindows Interface
	 */ 
	virtual void OpenWindow();
	virtual void OnDestroy();

	/**
	 * WEmitterWizardPage Interface
	 */
	virtual FString NotifyLeave();

	/**
	 * Delegate for Use Button
	 */
	void OnUseClick();


private:

	WEdit *TextureEdit;
	WButton *UseButton;

	WCheckBox *FadeInCheck;
	WCheckBox *FadeOutCheck;
};






/** 
 * Wizard page that picks the texture and fading of the emitter
 */
class WMeshWP : public WEmitterWizardPage
{
	DECLARE_WINDOWCLASS(WMeshWP,WEmitterWizardPage,UnrealEd)

public:

	/** 
	 * Constructor
	 * @see WEmitterWizard page for param info
	 */
	WMeshWP(WEmitterWizard* InOwnerWindow);
	
	/**
	 * WWindows Interface
	 */ 
	virtual void OpenWindow();
	virtual void OnDestroy();

	/**
	 * WEmitterWizardPage Interface
	 */
	virtual FString NotifyLeave();

	/**
	 * Delegate for Use Button
	 */
	void OnUseClick();


private:

	WEdit *MeshEdit;
	WButton *UseButton;

	WCheckBox *FadeInCheck;
	WCheckBox *FadeOutCheck;
};






/** 
 * Wizard page that picks the texture and fading of the emitter
 */
class WDenseWP : public WEmitterWizardPage
{
	DECLARE_WINDOWCLASS(WDenseWP,WEmitterWizardPage,UnrealEd)

public:

	/** 
	 * Constructor
	 * @see WEmitterWizard page for param info
	 */
	WDenseWP(WEmitterWizard* InOwnerWindow);
	
	/**
	 * WWindows Interface
	 */ 
	virtual void OpenWindow();
	virtual void OnDestroy();

	/**
	 * WEmitterWizardPage Interface
	 */
	virtual FString NotifyLeave();

private:

	WCheckBox *SparseCheck;
	WCheckBox *NormalCheck;
	WCheckBox *DenseCheck;

	WCheckBox *SmallCheck;
	WCheckBox *MediumCheck;
	WCheckBox *LargeCheck;

	HBITMAP SparseBitmap;
	HBITMAP NormalBitmap;
	HBITMAP DenseBitmap;

	HBITMAP SmallBitmap;
	HBITMAP MediumBitmap;
	HBITMAP LargeBitmap;
};






/** 
 * Wizard page that picks the rotation of the particles
 */
class WRotationWP : public WEmitterWizardPage
{
	DECLARE_WINDOWCLASS(WRotationWP,WEmitterWizardPage,UnrealEd)

public:

	/** 
	 * Constructor
	 * @see WEmitterWizard page for param info
	 */
	WRotationWP(WEmitterWizard* InOwnerWindow);
	
	/**
	 * WWindows Interface
	 */ 
	virtual void OpenWindow();
	virtual void OnDestroy();

	/**
	 * WEmitterWizardPage Interface
	 */
	virtual FString NotifyLeave();

private:

	WCheckBox *StartSpinCheck;

	WCheckBox *NoSpinCheck;
	WCheckBox *SlowCheck;
	WCheckBox *FastCheck;
};






/** 
 * Wizard page that picks the time of the emitter
 */
class WTimeWP : public WEmitterWizardPage
{
	DECLARE_WINDOWCLASS(WTimeWP,WEmitterWizardPage,UnrealEd)

public:

	/** 
	 * Constructor
	 * @see WEmitterWizard page for param info
	 */
	WTimeWP(WEmitterWizard* InOwnerWindow);
	
	/**
	 * WWindows Interface
	 */ 
	virtual void OpenWindow();
	virtual void OnDestroy();

	/**
	 * WEmitterWizardPage Interface
	 */
	virtual FString NotifyLeave();

private:

	WEdit *LifetimeEdit;
	class WButtonSlider *LifetimeSlide;

	WCheckBox *RespawnDeadCheck;

	WCheckBox *SteadyCheck;
	WCheckBox *BurstCheck;
};




/** 
 * Wizard page that picks additional movement properties of the emitter
 */
class WAdditionalMovementWP : public WEmitterWizardPage
{
	DECLARE_WINDOWCLASS(WAdditionalMovementWP,WEmitterWizardPage,UnrealEd)

public:

	/** 
	 * Constructor
	 * @see WEmitterWizard page for param info
	 */
	WAdditionalMovementWP(WEmitterWizard* InOwnerWindow);
	
	/**
	 * WWindows Interface
	 */ 
	virtual void OpenWindow();
	virtual void OnDestroy();

	/**
	 * WEmitterWizardPage Interface
	 */
	virtual FString NotifyLeave();

private:

	WCheckBox *CollideCheck;

	WCheckBox *NoGravityCheck;
	WCheckBox *WeakCheck;
	WCheckBox *NormalCheck;
	WCheckBox *StrongCheck;
};




/** 
 * Wizard page that picks the texture and fading of the emitter
 */
class WBeamNoiseWP : public WEmitterWizardPage
{
	DECLARE_WINDOWCLASS(WBeamNoiseWP,WEmitterWizardPage,UnrealEd)

public:

	/** 
	 * Constructor
	 * @see WEmitterWizard page for param info
	 */
	WBeamNoiseWP(WEmitterWizard* InOwnerWindow);
	
	/**
	 * WWindows Interface
	 */ 
	virtual void OpenWindow();
	virtual void OnDestroy();

	/**
	 * WEmitterWizardPage Interface
	 */
	virtual FString NotifyLeave();

private:

	WCheckBox *NoNoiseCheck;
	WCheckBox *SmallCheck;
	WCheckBox *MediumCheck;
	WCheckBox *LargeCheck;

	WCheckBox *ShortCheck;
	WCheckBox *AverageCheck;
	WCheckBox *LongCheck;

	HBITMAP NoNoiseBitmap;
	HBITMAP SmallBitmap;
	HBITMAP MediumBitmap;
	HBITMAP LargeBitmap;
};






#endif
