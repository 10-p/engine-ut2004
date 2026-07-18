//=============================================================================
//
// ParticleEditorTab.h	
// Copyright(c) 2002 Demiurge Studios. All Rights Reserved.
// 
//=============================================================================

#ifndef _PARTICLE_EDITOR_TAB_H
#define _PARTICLE_EDITOR_TAB_H
	

/**
 * WParticleEditorTab
 * This class contains and controls everything for editing a single Emitter within a particle system
 * Each tab retains the information about which categories are currently being displayed along with
 * the controls (WParticleEditorComponent) that edit those properties.
 */
class WParticleEditorTab : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WParticleEditorTab,WPropertyPage,Window)

public:

	/** 
	 * Constructor
	 */
	WParticleEditorTab(WWindow* owneraWindow, class WParticleEditor* OwnerEditor, FString InTabName, class UParticleEmitter* InEditTarget, class AEmitter* InEmitter);

	/**
	 * "Destructor"
	 */
	virtual void OnDestroy();

	// WWindow Interface functions 
	virtual void OnCreate();
	virtual void OnSize( DWORD Flags, INT NewX, INT NewY );
	virtual void OnVScroll( WPARAM wParam, LPARAM lParam );
	virtual void OnCommand( INT Command );
	virtual void OnShowWindow(UBOOL bShow);
	virtual void OnPaint();


	/**
	 * Sets the positions of all the components based on if they are Shown or not
	 * This is usefull for components to call when they change size.
	 */
	void SetComponentAnchors();

	/**
	 * Causes components to fill in the values with the ones from the Emitter that this tab is editing.
	 * Calls RereadToolValues(ForceUpdateAll) on all the components in this tab
	 * if ForceUpdateAll is TRUE, it ensures that all Tools will be updated, not just the listening ones
	 */
	void RereadComponentValues(UBOOL ForceUpdateAll = FALSE);

	virtual void Refresh(UBOOL JustScroll = FALSE);

	UParticleEmitter* GetEditTarget() const { return EditTarget;} 

	AEmitter* GetEmitter() const {return Emitter;}

	class WParticleEditor* GetOwnerEditor() const {return OwnerEditor;} 

	void SaveStates();


protected:

	/**
	 * Creates and adds all the components to this tab.  This is called from OnCreate.
	 * Subclasses should override this function to add specific types of components but
	 * should make sure to call the super
	 */
	virtual void CreateComponents();

	/**
	 * Adds the given component
	 */
	void AddComponent(FString ComponentName, UBOOL Show);

	/**
	 * The current particle system being edited
	 */
	class UParticleEmitter* EditTarget;

	/** 
	 * The Emitter belonging to the EditTarget
	 */
	class AEmitter* Emitter;

	/**
	 * The owner of this tab
	 */
	class WParticleEditor* OwnerEditor;

private:

	//variables


	/**
	 * The ordered list of the current set of components the particle system editor has
	 */
	TArray<class WParticleEditorComponent*> currentComponents;

	/**
	 * The height of the entire componentlist
	 */
	INT componentListSize;

	/** Left-side list of all the category settings */
	class WCheckListBox* CategoryList;

	/** The scroll bar */
	WScrollBar* scrollBar;

	/** The Location of the top of the viewable area in pixels */
	INT scrollLoc;

	/** Controls the Location of everything.*/
	TMap<DWORD,FWindowAnchor> Anchors;

	/** Works with Anchors to control the Location of things */
	FContainer *Container;

	/** the upper padding on the components window */
	INT InitialVertOffset;

	/** the space between the components in the window */
	INT spaceBetweenComponents;
	
	TArray< TArray<INT> > ExpandState;
	TArray< TArray<DWORD> > ToolState;


	//functions

	/**
	 * Update the position of the container and what it contains
	 */
	void PositionChildControls();

	/**
	 * Does what it says
	 */
	void RefreshScrollBar();

	
	/**
	 * Called when a new component is selection from the list
	 */
	void OnComponentSelChange();

};


/**
 * Contains all the components on this tab particular to SpriteEmitters.
 */
class WParticleEditorSpriteTab : public WParticleEditorTab
{
	DECLARE_WINDOWCLASS(WParticleEditorSpriteTab,WParticleEditorTab,Window)

public:
	/** Constructor */
	WParticleEditorSpriteTab(WWindow* ownerWindow, WParticleEditor* OwnerEditor, FString InTabName, class UParticleEmitter* InEditTarget, class AEmitter* InEmitter);

protected:

	/**
	 * Creates and adds all the components to this tab particular to beams.
	 */
	virtual void CreateComponents();
};



/**
 * Contains all the components on this tab particular to beams.
 */
class WParticleEditorBeamTab : public WParticleEditorTab
{
	DECLARE_WINDOWCLASS(WParticleEditorBeamTab,WParticleEditorTab,Window)

public:
	/** Constructor */
	WParticleEditorBeamTab(WWindow* ownerWindow, WParticleEditor* OwnerEditor, FString InTabName, class UParticleEmitter* InEditTarget, class AEmitter* InEmitter);

protected:

	/**
	 * Creates and adds all the components to this tab particular to beams.
	 */
	virtual void CreateComponents();
};



/**
 * Contains all the components on this tab particular to MeshEmitters.
 */
class WParticleEditorMeshTab : public WParticleEditorTab
{
	DECLARE_WINDOWCLASS(WParticleEditorMeshTab,WParticleEditorTab,Window)

public:
	/** Constructor */
	WParticleEditorMeshTab(WWindow* ownerWindow, WParticleEditor* OwnerEditor, FString InTabName, class UParticleEmitter* InEditTarget, class AEmitter* InEmitter);

protected:

	/**
	 * Creates and adds all the components to this tab particular to Meshs.
	 */
	virtual void CreateComponents();
};


/**
 * Contains all the components on this tab particular to SparkEmitters.
 */
class WParticleEditorSparkTab : public WParticleEditorTab
{
	DECLARE_WINDOWCLASS(WParticleEditorSparkTab,WParticleEditorTab,Window)

public:
	/** Constructor */
	WParticleEditorSparkTab(WWindow* ownerWindow, WParticleEditor* OwnerEditor, FString InTabName, class UParticleEmitter* InEditTarget, class AEmitter* InEmitter);

protected:

	virtual void CreateComponents();
};


#endif