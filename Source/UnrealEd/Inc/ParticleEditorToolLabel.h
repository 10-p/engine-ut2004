//=============================================================================
//
// ParticleEditorToolLabel.h	
// Copyright(c) 2002 Demiurge Studios. All Rights Reserved.
// 
//=============================================================================

#ifndef _PARTICLE_EDITOR_TOOL_LABEL_H
#define _PARTICLE_EDITOR_TOOL_LABEL_H




/**
 * A Label in a ParticleEditorComponent that is assosiated with a ParticleEditorTool
 */
class WParticleToolLabel : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WParticleToolLabel,WPropertyPage,Window)

public:

	/**
	 * Creates a Tool Label that is contained in the given component and is
	 * associated with the Tool with the given index
	 */
	WParticleToolLabel( WWindow* InOwner, FString InLabelText, WParticleEditorComponent *ParentComponent, INT ToolIndex );

	/**
	 * "Destructor"
	 */
	virtual void OnDestroy();

	/**
	 * creates and displays the Label
	 */
	virtual void OpenWindow( HMODULE InHMOD = 0 ) = 0;

	/**
	 * returns the size of the Label
	 */
	virtual FPoint GetSize() const = 0;

	/**
	 * Sets the expanded state of this label without called the expand on the parent component.
	 * By default does nothing.
	 */
	virtual void SetExpand(UBOOL Expanded);


protected:

	/**
	 * The text of this Label
	 */
	FString LabelText;

	/**
	 * The ParticleEditorComponent this Tool Label is in
	 */
	WParticleEditorComponent *ParentComponent;

	/**
	 * The index in the component of the assocated Tool
	 */
	INT ToolIndex;

	const INT X_OFFSET;
};




/**
 * A static Label.  It just says
 */
class WParticleStaticLabel : public WParticleToolLabel
{
	DECLARE_WINDOWCLASS(WParticleStaticLabel,WParticleToolLabel,Window)

public:

	/**
	 * Creates a Tool Label that calls back the ParentComponent to expand or unexpand the given Tool
	 */
	WParticleStaticLabel( WWindow* InOwner, FString InLabelText, WParticleEditorComponent *ParentComponent, INT ToolIndex);

	/**
	 * "Destructor"
	 */
	virtual void OnDestroy();

	/**
	 * creates and displays the Label
	 */
	virtual void OpenWindow( HMODULE InHMOD = 0 );

	/**
	 * returns the size of the Label
	 */
	virtual FPoint GetSize() const;


private:

	/**
	 * The Label this Tool Label uses to show the text
	 */
	WLabel *Label;
};




/**
 * A lable that detects clicks and toggles the shownness of the associated Tool
 */
class WParticleExpandingLabel : public WParticleToolLabel
{
	DECLARE_WINDOWCLASS(WParticleExpandingLabel,WParticleToolLabel,Window)

public:

	/**
	 * Creates a Tool Label that calls back the ParentComponent to expand or unexpand the given Tool
	 */
	WParticleExpandingLabel( WWindow* InOwner, FString InLabelText, WParticleEditorComponent *ParentComponent, INT ToolIndex, UBOOL Expanded );

	/**
	 * "Destructor"
	 */
	virtual void OnDestroy();

	/**
	 * creates and displays the Label
	 */
	virtual void OpenWindow( HMODULE InHMOD = 0 );

	/**
	 * returns the size of the Label
	 */
	virtual FPoint GetSize() const;

	/**
	 * Sets the expanded state of this label without called the expand on the parent component.
	 */
	virtual void SetExpand(UBOOL Expanded);


private:

	/**
	 * The Label this Tool Label uses to show the text
	 */
	WLabel *Label;

	/**
	 * The button the Tool uses to expand
	 */
	WButton *ExpandButton;

	/**
	 * The Expanded state of this Tool Label
	 */
	UBOOL Expanded;

	/**
	 * This function is called when the expand button is Clicked
	 */
	void OnExpandClick();

};




#endif