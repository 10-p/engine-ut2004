//=============================================================================
//
// ParticleEditorControls.h	
// Copyright(c) 2002 Demiurge Studios. All Rights Reserved.
// 
//=============================================================================

#ifndef _PARTICLE_EDITOR_CONTROLS_H
#define _PARTICLE_EDITOR_CONTROLS_H


/**
 * Button that you click and drag up and down and it changes a value
 */
class WButtonSlider : public WButton
{

public:

	 //Constructor
	WButtonSlider( WWindow* InOwner, WEdit *InAssociatedEdit, INT InId=0, UBOOL WholeNumbersOnly = FALSE, FDelegate InClicked=FDelegate(), WNDPROC InSuperProc=NULL );

	 //"Destructor"
	virtual void OnDestroy();

	FLOAT GetCurrentValue();

	void SetCurrentValue(FLOAT newValue);

	virtual void OpenWindow();
	virtual void OnMove(INT NewX, INT NewY);
	virtual void OnLeftButtonDown();
	virtual void OnLeftButtonUp();
	virtual void OnMouseMove(DWORD Flags, FPoint InLocation);

private:

	UBOOL WholeNumbersOnly;
	FPoint Location;
	FLOAT CurValue;
	FLOAT IncAmount;
	UBOOL Sliding;
	UBOOL HasMoved;
	UBOOL NewClick;
	FPoint LastLocation;
	WEdit *AssociatedEdit;
	HBITMAP UpDownImage;
};




/**
 * A color button for use in a color scale edits that gives mouse events to the
 * color scale edit
 */
class WColorScaleButton : public WColorButton
{

public:

	WColorScaleButton( WWindow* InOwner, class WColorScaleEdit* ColorScaleEdit, INT ButtonIndex);

	//virtual void OnDestroy();

	virtual void OpenWindow( UBOOL visible, INT left, INT top, INT width, INT height );
	virtual void OnLeftButtonDown();
	virtual void OnLeftButtonUp();
	virtual void OnLeftButtonDoubleClick();
	virtual void OnRightButtonUp();
	virtual void OnMove(INT NewX, INT NewY);
	virtual void OnMouseMove(DWORD Flags, FPoint Location);
	virtual void OnDrawItem( DRAWITEMSTRUCT* Item );

	FPoint GetLocation();
	void SetIndex(INT NewIndex);

	void SetSelected(UBOOL Selected);

	//overriden functions to include alpha
	void SetColor( INT InR, INT InG, INT InB, INT InA );
	void GetColor( INT& InR, INT& InG, INT& InB, INT& InA );
	
private:

	class WColorScaleEdit* ColorScaleEdit;
	INT ButtonIndex;
	FPoint LastRelativeLocation;
	UBOOL Selected;
	INT alpha;
};




#endif