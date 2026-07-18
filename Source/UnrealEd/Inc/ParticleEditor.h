//=============================================================================
//
// ParticleEditor.h	
// Copyright(c) 2002 Demiurge Studios. All Rights Reserved.
// 
//=============================================================================

#ifndef _PARTICLE_EDITOR_H
#define _PARTICLE_EDITOR_H

// <gasp> A header file without implementation in the UnrealEd Project! 
// Thanks Jack for making this possible!

#include "ParticleEditorTab.h"
#include "ParticleEditorTools.h"
#include "ParticleEditorComponent.h"

//define a bold font to use for this particle system editor - this is blakc voodoo magic
#define PS_BOLD_FONT	0xFE1A

/**
 * The main particle editor window.
 * Everything is contained in here. Mostly it's just a menu, toolbar and tabbed area
 */
class WParticleEditor : public WWindow
{
	DECLARE_WINDOWCLASS(WParticleEditor,WWindow,Window)

public:

	WParticleEditor(FName InPersistentName, WWindow* InOwnerWindow);

	/**
	 * WWindow Interface
	 */
	virtual void OnCreate();
	virtual void OpenWindow();
	virtual void OnSize( DWORD Flags, INT NewX, INT NewY );
	virtual void OnCommand( INT Command );
	virtual void OnDestroy();
	virtual INT OnSysCommand( INT Command );
	virtual void OnShowWindow( UBOOL bShow );

	/**
	 * Set the PS currently being edited
	 */
	void SetEditTarget(class AEmitter* InEmitter);

	/**
	 * Called anytime a new PE is added to edit target.
	 */
	void UpdateParticleEmitterTabs();

	/**
	 * //scion capps:  select the active tab
	 */
	void SelectParticleEmitterTab(UParticleEmitter* upe);

	/**
	 * Called when the editor selects new actors
	 */
	void NotifySelectionChange();

	/**
	 * Returns a pointer to the PS the editor is currently editing\
	 */
	AEmitter* GetEditTarget() {return EditTarget;}

	/**
	 * Adds a new emitter to the particle system currently being edited that is the same as the
	 * passed in emitter.
	 */
	void DuplicateEmitter(const UParticleEmitter* TemplateEmitter);

	/**
	 * Opens a dialogue for the user to save the specified emitter to disk
	 */
	void SaveEmitter(UParticleEmitter* Emitter);

	/**
	 * Opens a load emitter dialogue and loads the selected one.
	 */
	void LoadEmitter();

	/**
	 * Returns the component with the given name
	 */
	WParticleEditorComponent* GetComponent(FString ComponentName);

	/**
	 * The last tab this editor dealt with.  This can also be the current tab.  This is used for switching tabs
	 * ans saving values.
	 */
	WParticleEditorTab *LastTab;

	/**
	 * Opens all the components and sets ComponentsOpened to true
	 */
	void InitialOpenComponents(WParticleEditorTab *ParentTab);

	UBOOL ComponentsOpened;

	UBOOL ComponentsReadyToShow;


private:

	HWND ToolBar;

	/**
	 * The current particle system being edited
	 */
	class AEmitter* EditTarget;

	/** 
	 * Left-side list of all the category settings
	 */
	class WCheckListBox* CategoryList;

	/**
	 * Controls the Location of everything.
	 * <Window Handle, Location Info>
	 */
	TMap<DWORD,FWindowAnchor> Anchors;
	FContainer *Container;

	/**
	 * Update the position of the container and what it contains
	 */
	void PositionChildControls();

	/**
	 * All of the Emitter tabs
	 */
	WPropertySheet* EmitterTabs;

	/**
	 * All the possible components.  There is only one of each component and each is reused
	 */
	TArray<WParticleEditorComponent*> PSEComponents;

	/**
	 * Used to save the last-Selected Emitter when the PS is not Shown, so it can update when it is Shown
	 */
	class AEmitter* EditTargetWhenShown;

	/**
	 * Utility function for loading an emitter from an FStringOutputDevice
	 */
	UParticleEmitter* LoadEmitterFromFile(UClass* EmitterClass, FStringOutputDevice data);

	WToolTip* ToolTipCtrl;
	class WEmitterWizard* EmitterWizard;

friend class WNameTool; // Needs to edit the names of the tabs
};


/**
 * When the use clickes the "New" button in the editor this dialogue is displayed
 * It allows the creation of new ParticleEmitters
 */
class WParticleEditorNewEmitterDlg : public WDialog
{
	DECLARE_WINDOWCLASS(WParticleEditorNewEmitterDlg,WDialog,UnrealEd)

public:
	// Constructor.
	WParticleEditorNewEmitterDlg( WWindow* InOwnerWindow, AEmitter* InEditTarget );

	// WDialog interface.
	void OnInitDialog();

	INT DoModal( );
	void OnOk();


private:

	// Variables.
	WButton OkButton;
	WButton CancelButton;
	WEdit NameEdit;
	WComboBox EmitterType;

	FString defPackage, DefGroup;
	TArray<FString>* Filenames;

	FString Package, Group, Name;
	UBOOL bOKToAll;
	INT CurrentFilename;

	AEmitter* EditTarget;
};


/**
 * When the use clickes the "New" button in the editor this dialogue is displayed
 * It allows the creation of new ParticleEmitters
 */
class WParticleEditorExportDlg : public WDialog
{
	DECLARE_WINDOWCLASS(WParticleEditorExportDlg,WDialog,UnrealEd)

public:
	// Constructor.
	WParticleEditorExportDlg( WWindow* InOwnerWindow, AEmitter* InEditTarget );

	// WDialog interface.
	void OnInitDialog();

	INT DoModal( );
	void OnOk();


private:

	WButton OkButton;
	WButton CancelButton;
	WEdit ClassEdit;
	WEdit PackageEdit;
	WCheckBox AutoDestroyCheck;

	AEmitter* EditTarget;
};


#endif
