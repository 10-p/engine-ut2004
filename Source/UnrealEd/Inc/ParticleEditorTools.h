//=============================================================================
//
// ParticleEditorTools.h	
// Copyright(c) 2002 Demiurge Studios. All Rights Reserved.
// 
//=============================================================================

#ifndef _PARTICLE_EDITOR_TOOLS_H
#define _PARTICLE_EDITOR_TOOLS_H



/**
 * Flags for constructing ParticleEditorTools
 */
enum ParticleToolFlags
{
	PTOOL_None				= 0x0000,	// No flags.
	PTOOL_RefreshOnChange	= 0x0001,	// the particle Emitter is restarted on when this Tool is changed
	PTOOL_UpdateTools		= 0x0002,	// all the other Tools that are listening have their values reread from the particle Emitter when this Tool is changed
	PTOOL_ListenForUpdate	= 0x0004,	// sets this Tool to listen for PTOOL_UpdateTools (see above)
	PTOOL_ExtraIndent		= 0x0008	// this Tool gets and extra indent - this is often set at the Tool level, not own create
};


typedef enum _LockState
{
	LOCK_None,
	LOCK_Same,
	LOCK_Mirror
} ELockState;



/**
 * Super class for all particle Editor Tools.
 */
class WParticleEditorTool : public WPropertyPage
{
	DECLARE_WINDOWCLASS(WParticleEditorTool,WPropertyPage,Window)

public:

	/**
	 * Creates a particle editor Tool that edits the given particle and has the given Name.
	 */
	WParticleEditorTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * Like the other constructor but can be placed in any window, not just a component
	 */
	WParticleEditorTool(WWindow *OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * "Destructor", called when destroyed
	 */
	virtual void OnDestroy();

	/**
	 * Returns the Name of this Tool
	 */
	FString GetToolName() const;

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const = 0;

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget() = 0;

	/**
	 * This function sets the edit target this tool points to.
	 */
	virtual void SetTarget();
	
	/**
	 * updates the values of the target from this Tool and refreshes the particle
	 * system if appropriate based on refreshOnChange
	 * This function will only execute it's contents once per call.
	 * This function works by calling UpdateTargetUtil
	 */
	void UpdateTarget();

	/**
	 * Enables or Disables (grays out) the Tool depending on the value passed
	 */
	virtual void Enable(UBOOL Enabled);

	/**
	 * Returns true if this tool is enabled, false if not.
	 */
	virtual UBOOL IsEnabled() const {return ToolEnabled;}

	/**
	 * the flags for this Tool - see ParticleToolFlags
	 */
	const DWORD ToolFlags;

	/**
	 * This function should create all the Tools and set up the delegates
	 */
	virtual void OpenWindow( HMODULE InHMOD ) = 0;

	/**
	 * Each tool can encode its current state in the this DWORD.
	 * This DWORD will be used with SetToolState to set the tool state.
	 * It is entierly up to the tool how to encode the state.
	 * The implementation for this class just returns 0. 
	 */
	virtual DWORD GetToolState();

	/**
	 * This sets the state for the tool using a value generated with GetToolState.
	 * The implementation for this class does nothing.
	 */
	virtual void SetToolState(DWORD State);


protected:

	/**
	 * This should be implemented by the subclasses so UpdateTarget works
	 * it should NEVER be called dirrectly by a subclass
	 * This call is "singular" in that it will not loop recursively
	 */
	virtual void UpdateTargetUtil() = 0;

	/**
	 * The target particle Emitter
	 */
	class UParticleEmitter* EditTarget;

	/**
	 * The component this Tool is contained it.
	 */
	WParticleEditorComponent* OwnerComponent;

	/**
	 * This is used so UpdateTarget doesn't loop forever.  Can also be used to disable update target
	 */
	UBOOL CallUpdateTarget;

	/**
	 * If this tool is enabled
	 */
	UBOOL ToolEnabled;

private:

	/**
	 * The Name of this particular page
	 */
	FString ToolName;

};




/**
 * Tool to edit ranges
 */
class WRangeEdit : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WRangeEdit,WParticleEditorTool,Window)

public:

	 /**
	  * Constructor
	  */
	WRangeEdit(class WParticleEditorComponent* OwnerComponent, FString VectorName, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * Like the other constructor but can be placed in any window, not just a component
	 */
	WRangeEdit(WWindow *OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString VectorName, DWORD ToolFlags, INT InUDNHelpTopic);

	 /**
	  * "Destructor"
	  */
	virtual void OnDestroy();

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * Enables or Disables (grays out) the Tool depending on the value passed
	 */
	virtual void Enable(UBOOL Enabled);

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();

	/**
	 * This function creates all the Tools and sets up the delegates.
	 */
	virtual void OpenWindow( HMODULE InHMOD );

	/** 
	 * Sets the absolute Min value of this range.  Calling this function enables Min checking.
	 */
	void SetAbsMin(FLOAT Min);

	/** 
	 * Sets the absolute Max value of this range.  Calling this function enables Max checking.
	 */
	void SetAbsMax(FLOAT Max);

	/**
	 * This function sets the edit target this tool points to and adjusts the range to edit.
	 */
	virtual void SetTarget(FRange* RangeToEdit);

	/**
	 * Each tool can encode its current state in the this DWORD.
	 * This DWORD will be used with SetToolState to set the tool state.
	 * It is entierly up to the tool how to encode the state.
	 * The implementation stores LockState
	 */
	virtual DWORD GetToolState();

	/**
	 * This sets the state for the tool using a value generated with GetToolState.
	 * The implementation for this class sets LockState.
	 */
	virtual void SetToolState(DWORD State);


protected:

	/** 
	 * updates the FRange RangeToEdit
	 */
	virtual void UpdateTargetUtil();


private:

	//variables

	/**
	 * The Min and the Max for the components of the range
	 */
	WEdit* MinEdit;
	WEdit* MaxEdit;

	/**
	 * The sliders for the edit boxes
	 */
	WButtonSlider* MinSlide;
	WButtonSlider* MaxSlide;

	/**
	 * the lock buttons
	 */
	WButton* LockButton;

	/**
	 * how to relate Min and Max
	 */
	ELockState LockState;

	// the images to show on the lock buttons
	HBITMAP LockOpenBitmap;
	HBITMAP LockSameBitmap;
	HBITMAP LockMirrorBitmap;


	/**
	 * a link to the FRange that this Tool is supposed to change 
	 */
	FRange* RangeToEdit;

	/**
	 * The absolute Min value this range can have
	 */
	FLOAT AbsMinValue;

	/**
	 * The absolute Max value this range can have
	 */
	FLOAT AbsMaxValue;

	/**
	 * if this is TRUE, there is a check for absolute Min value
	 */
	UBOOL UseAbsMin;

	/**
	 * if this is TRUE, there is a check for absolute Max value
	 */
	UBOOL UseAbsMax;

	/**
	 * 	Insures that OnTextChange doesn't loop forever because it changes text sometimes
	 */
	UBOOL AlreadyChangingText;


	//functions

	/**
	 * a Delegate that is called when the lock button is Clicked
	 */
	void OnLockClick();

	/**
	 * a Delegate that is called when one of the text boxes is changed
	 */
	void OnTextChange(UBOOL minChange);

	/**
	 * helper function for OnTextChange
	 */
	void OnTextChangeMin();
	void OnTextChangeMax();

	/**
	 * Sets the LockState of this range edit and also sets the button image and enables
	 * the Min edit correctly.
	 * This does NOT set the text of the edits
	 */
	void SetLockState(ELockState newLockState);

	/**
	 * Sets the texts of the edits based on the value of LockState.  This also
	 * handles the abs Min and Max values
	 */
	void HandleLockText();
};




/**
 * Tool to edit ranged vectors (vectors that have a Min and a Max for each x, y, and z value)
 */
class WRangeVectorEdit : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WRangeVectorEdit,WParticleEditorTool,Window)

public:

	 //Constructor
	WRangeVectorEdit(class WParticleEditorComponent* OwnerComponent, FString VectorName, DWORD ToolFlags, INT InUDNHelpTopic);

	 //Constructor
	WRangeVectorEdit(WWindow* OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString VectorName, DWORD ToolFlags, INT InUDNHelpTopic);

	 //"Destructor"
	virtual void OnDestroy();

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * Enables or Disables (grays out) the Tool depending on the value passed
	 */
	virtual void Enable(UBOOL Enabled);

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();

	/**
	 * This function creates all the Tools and sets up the delegates.
	 */
	virtual void OpenWindow( HMODULE InHMOD ) { OpenWindow(InHMOD, IDPS_RANGE_VECT); }

	/**
	 * This function creates all the Tools and sets up the delegates.
	 * InDlgId is the resource dialog ID for a dialog that conforms to the 
	 * range vector standard like IDPS_RANGE_VECT
	 */
	virtual void OpenWindow( HMODULE InHMOD, INT InDlgId );

	/** 
	 * Sets the absolute Min value of this range.  Calling this function enables Min checking.
	 */
	void SetAbsMin(FLOAT Min);

	/** 
	 * Sets the absolute Max value of this range.  Calling this function enables Max checking.
	 */
	void SetAbsMax(FLOAT Max);

	/**
	 * This function sets the edit target this tool points to and adjusts the range vector to edit.
	 */
	virtual void SetTarget(FRangeVector *VectorToEdit);

	/*	
	 * Each tool can encode its current state in the this DWORD.
	 * This DWORD will be used with SetToolState to set the tool state.
	 * It is entierly up to the tool how to encode the state.
	 * The implementation stores LockState and LinkState
	 */
	virtual DWORD GetToolState();

	/**
	 * This sets the state for the tool using a value generated with GetToolState.
	 * The implementation for this class sets LockState and LinkState.
	 */
	virtual void SetToolState(DWORD State);


protected:

	/** 
	 * updates the FRangeVector VectorToEdit
	 */
	virtual void UpdateTargetUtil();


private:

	//variables

	/**
	 * The Min and the Max for the components of the vector
	 */
	WEdit* MinEdit[3];
	WEdit* MaxEdit[3];

	/**
	 * The sliders for the edit boxes
	 */
	WButtonSlider* MinSlide[3];
	WButtonSlider* MaxSlide[3];

	/**
	 * The linking check boxes for x, y, and z
	 */
	WCheckBox* LinkCheck[3];
	
	/**
	 * the image display thing for linking x, y, and z together
	 */
	WButton *LinkImageContainer;

	/**
	 * the images for the linking of x, y, and z
	 */
	HBITMAP LinkImage[7];

	/**
	 * the lock buttons
	 */
	WButton* LockButton[3];

	/**
	 * the lockes state of each of the buttons
	 */
	ELockState LockState[3];

	// the images to show on the lock buttons
	HBITMAP LockOpenBitmap;
	HBITMAP LockSameBitmap;
	HBITMAP LockMirrorBitmap;


	/**
	 * a link to the FRangeVector that this Tool is supposed to change 
	 */
	FRangeVector *VectorToEdit;

	/**
	 * 	Insures that OnTextChange doesn't loop forever because it changes text sometimes
	 */
	UBOOL AlreadyChangingText;

	/**
	 * The absolute Min value this range can have
	 */
	FLOAT AbsMinValue;

	/**
	 * The absolute Max value this range can have
	 */
	FLOAT AbsMaxValue;

	/**
	 * if this is TRUE, there is a check for absolute Min value
	 */
	UBOOL UseAbsMin;

	/**
	 * if this is TRUE, there is a check for absolute Max value
	 */
	UBOOL UseAbsMax;


	//functions

	/**
	 * a Delegate that is called when one of the link check boxes is Clicked
	 */
	void OnLinkClick();

	/**
	 * a Delegate that is called when one of the lock buttons is Clicked
	 */
	void OnLockClickX();
	void OnLockClickY();
	void OnLockClickZ();
	void OnLockClick(INT button);

	/**
	 * a Delegate that is called when one of the text boxes is changed
	 */
	void OnTextChangeMin();
	void OnTextChangeMax();
	void OnTextChange(UBOOL minChange);

	/**
	 * Sets the LockState of this range edit and also sets the button image and enables
	 * the Min edit correctly.
	 * This does NOT set the text of the edits
	 */
	void SetLockState(INT button, ELockState newLockState);

	/**
	 * Sets the texts of the edits based on the value of LockState. 
	 */
	void HandleLockText(INT button);

};




/**
 * Tool to edit FRangeVectors that are really rotators - this differs only in appearance form WRangeVectorEdit
 */
class WRangeRotatorEdit : public WRangeVectorEdit
{
	DECLARE_WINDOWCLASS(WRangeRotatorEdit,WRangeVectorEdit,Window)

public:

	 //Constructor
	WRangeRotatorEdit(class WParticleEditorComponent* OwnerComponent, FString VectorName, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * This function creates all the Tools and sets up the delegates.
	 */
	virtual void OpenWindow( HMODULE InHMOD);
};

	
	
	
/**
 * Tool to edit FRangeVectors that are really color ranges - this differs only in appearance form WRangeVectorEdit
 */
class WRangeColorEdit : public WRangeVectorEdit
{
	DECLARE_WINDOWCLASS(WRangeColorEdit,WRangeVectorEdit,Window)

public:

	 //Constructor
	WRangeColorEdit(class WParticleEditorComponent* OwnerComponent, FString VectorName, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * This function creates all the Tools and sets up the delegates.
	 */
	virtual void OpenWindow( HMODULE InHMOD);
};

	
	
	
/**
 * A Tool that allows you to modify color scales in a visual way.
 */
class WColorScaleEdit : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WColorScaleEdit,WParticleEditorTool,Window)

friend class WColorScaleButton;

public:

	 /**
	  * Creates a Tool that allows you to modify color scales in a visual way
	  */
	WColorScaleEdit(class WParticleEditorComponent* OwnerComponent, DWORD ToolFlags, INT InUDNHelpTopic);

	 /**
	  * "Destructor"
	  */
	virtual void OnDestroy();

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * Enables or Disables (grays out) the Tool depending on the value passed
	 */
	virtual void Enable(UBOOL Enabled);

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();

	/**
	 * This function creates all the Tools and sets up the delegates.
	 */
	virtual void OpenWindow( HMODULE InHMOD );

	/**
	 * called on mouse down event - used to pick which color button to move 
	 */
	//void OnLeftButtonDown();

	/**
	 * called on a mouse up event - used with OnLeftButtonDown to pick color buttons
	 */
	//void OnLeftButtonUp();

	/**
	 * This function is called when there is a mouse move event
	 */
	virtual void OnMouseMove(DWORD flags, FPoint Location);

	virtual void OnLeftButtonDoubleClick();

	virtual void OnPaint();


protected:

	/** 
	 * updates the FRange RangeToEdit
	 */
	virtual void UpdateTargetUtil();

	void ColorButtonDown(INT ButtonIndex);
	void ColorButtonUp(INT ButtonIndex);
	void ColorButtonDoubleClick(INT ButtonIndex);
	void ColorButtonRightClick(INT ButtonIndex);
	void ColorButtonMove(FPoint Location);


private:

	TArray<WColorScaleButton*> ColorButtons;
	TArray<WColorScaleButton*> DeleteQueue;


	/**
	 * The Labels for the RGBA fields for editing the Selected color in the scale
	 */
	TArray<WLabel*> ColorLabels;

	/**
	 * The edits for the RGBA fields to edit the Selected color in the scale
	 */
	TArray<WEdit*> ColorEdits;

	/**
	 * The sliders for the RGBA fields to edit the Selected color in the scale
	 */
	TArray<WButtonSlider*> ColorSlides;

	WLabel *RelTimeLabel;
	WEdit *RelTimeEdit;
	WButtonSlider *RelTimeSlide;
	
	WGroupBox *Box;

	WLabel * disableLabel;

	INT curButton;
	INT lastClicked;
	FPoint mouseLoc;

	const FPoint size;
	const INT buttonWidth;
	const INT buttonHeight;

	UBOOL Enabled;


	void MoveButton(INT ButtonIndex, FLOAT relativeTime, UBOOL updateText = TRUE);

	FLOAT WColorScaleEdit::GetRelativeTimeFromPoint(FPoint Location);

	INT InsertButton(FLOAT relativeTime, FColor color, UBOOL insertInTargetToo);

	void OnColorTextChange();
	void OnRelTimeTextChange();

	/**
	 * This function sets the currently selected button so that the button is drawn selected
	 * and the edit fields change that button
	 */
	void SetSelectedButton(INT ButtonIndex);

	/**
	 * Pops up the color pick window to select the color for the given button.
	 */
	void PopUpColorPickerForButton(INT ButtonIndex);

	/**
	 * Deletes the button with the given index.
	 */
	void DeleteButton(INT ButtonIndex);

};




/**
 * Tool for editing enums
 */
class WEnumEdit : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WEnumEdit,WParticleEditorTool,Window)

public:

	/**
	 * Constructor
	 * @param InEnumName a human-readable lable for this control
	 * @param InEnumNameList an array of human-readible Labels for each of the selectable enums
	 * @param InEnumIDList an array of ints which maps directly to the InEnumName list. It should contain the value the enum will be set to when the user selects something from InEnumNameList
	 * @param InEnumToEdit a pointer to the enum to be edited, cast to an INT.
	 */
	WEnumEdit(class WParticleEditorComponent* OwnerComponent, FString InEnumName, TArray<FString> InEnumNameList, TArray<INT> InEnumIDList, DWORD ToolFlags, INT InUDNHelpTopic, UBOOL EmitterPicker = FALSE);
	WEnumEdit(WWindow *OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString InEnumName, TArray<FString> InEnumNameList, TArray<INT> InEnumIDList, DWORD ToolFlags, INT InUDNHelpTopic, UBOOL EmitterPicker = FALSE);

	 //"Destructor"
	virtual void OnDestroy();

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * This function creates all the Tools and sets up the delegates.
	 */
	virtual void OpenWindow( HMODULE InHMOD );

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();

	/**
	 * Causes the enum to disable the specified Tool when the specified value is Selected
	 */
	virtual void AddDisableTool(WParticleEditorTool* toolToDisable, INT enumID);

	/**
	 * This function sets the edit target this tool points to and adjusts the range to edit.
	 */
	virtual void SetTarget(INT* InEnumToEdit);


protected:

	/** 
	 * updates the edit targert
	 */
	virtual void UpdateTargetUtil();


private:

	/**
	 * The control for the enum picker
	 */
	WComboBox* ValEdit;

	TArray<FString> EnumNameList;
	TArray<INT> EnumIDList;
	INT* EnumToEdit;

	typedef struct
	{
		WParticleEditorTool* Tool;
		INT id;
	}DisableStruct;

	TArray<DisableStruct*> DisableInfo;

	UBOOL EmitterPicker;
};




/**
 * Tool for editing single Bools
 */
class WBoolEdit : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WBoolEdit,WParticleEditorTool,Window)

public:

	/**
	 * Constructor
	 */
	WBoolEdit(class WParticleEditorComponent* OwnerComponent, FString InBoolName, FString InNameOfBoolToEdit, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * "Destructor"
	 */
	virtual void OnDestroy();

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();

	/**
	 * This function creates all the Tools and sets up the delegates.
	 */
	virtual void OpenWindow( HMODULE InHMOD );


protected:

	/** 
	 * updates the edit targert
	 */
	virtual void UpdateTargetUtil();

	/**
	 * The check Box for the UBOOL that contains the state of the UBOOL
	 */
	WCheckBox* ValEdit;


private:

	/**
	 * the Name of the UBOOL that can be used to get at the UBOOL because we can't pass
	 * the address of a UBOOL because it is a bitfield in some cases
	 */
	FString NameOfBoolToEdit;
};




/**
 * Tool for editing single bools that toggle the enabledness of other Tools
 */
class WEnableEdit : public WBoolEdit
{
	DECLARE_WINDOWCLASS(WEnableEdit,WBoolEdit,Window)

public:

	/**
	 * Constructor
	 * If InvertEnable is TRUE, the set of Tools will be Enabled when this edit is NOT set
	 */
	WEnableEdit(class WParticleEditorComponent* OwnerComponent, FString InBoolName, FString InNameOfBoolToEdit, UBOOL InvertEnable, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * Adds a Tool to the list of Tools this edit will enable and idsable
	 */
	void AddTool(WParticleEditorTool* NewTool);

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();


protected:

	/** 
	 * updates the edit targert
	 */
	virtual void UpdateTargetUtil();


private:

	/**
	 * The set of Tools that will be Enabled or disabled based on the value of this edit
	 */
	TArray<WParticleEditorTool*> ToolsToEnable;

	/**
	 * If this is TRUE, the set of Tools will be Enabled when this edit is NOT set
	 */
	UBOOL InvertEnable;
};


/**
 * Tool for editing names
 */
class WNameEdit : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WNameEdit,WParticleEditorTool,Window)

public:

	 //Constructor
	WNameEdit(class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic);

	 //Constructor
	WNameEdit(WWindow *OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic);

	 //"Destructor"
	virtual void OnDestroy();

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * This function creates all the Tools and sets up the delegates.
	 */
	virtual void OpenWindow( HMODULE InHMOD );

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();

	/**
	 * This function sets the edit target this tool points to and adjusts the name to edit.
	 */
	virtual void SetTarget(FName *InNameToEdit);

protected:

	/** 
	 * updates the edit targert
	 */
	virtual void UpdateTargetUtil();


private:

	//variables

	/**
	 * The values for Name
	 */
	WEdit* ValEdit;

	/**
	 * a link to the FName that this Tool is supposed to change 
	 */
	FName *NameToEdit;
};



/**
 * Tool for editing strings
 */
class WStringEdit : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WStringEdit,WParticleEditorTool,Window)

public:

	 //Constructor
	WStringEdit(class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic);

	 //Constructor
	WStringEdit(WWindow *OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic);

	 //"Destructor"
	virtual void OnDestroy();

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * This function creates all the Tools and sets up the delegates.
	 */
	virtual void OpenWindow( HMODULE InHMOD );

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();

	/**
	 * This function sets the edit target this tool points to and adjusts the string to edit.
	 */
	virtual void SetTarget(FString *InStringToEdit);

protected:

	/** 
	 * updates the edit targert
	 */
	virtual void UpdateTargetUtil();

	/**
	 * a link to the FString that this Tool is supposed to change 
	 */
	FString *StringToEdit;

private:

	//variables

	/**
	 * The values for string
	 */
	WEdit* ValEdit;


};




/**
 * Tool for editing floats from range 0.0 to 1.0 as a percent
 */
class WPercentEdit : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WPercentEdit,WParticleEditorTool,Window)

public:

	/**
	 * Creates a Tool for editing floats from range 0.0 to 1.0 as a percent
	 */
	WPercentEdit(WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT UDNHelpTopic);

	/**
	 * Like the other constructor but can be placed in any window, not just a component
	 */
	WPercentEdit(WWindow *OwnerWindow, WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT UDNHelpTopic);

	/**
	 * functions as a destructor - deletes all the memory
	 */
	virtual void OnDestroy();

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();

	/**
	 * This function creates all the Tools and sets up the delegates
	 */
	virtual void OpenWindow( HMODULE InHMOD );

	/** 
	 * Sets the new target and float to edit
	 */
	virtual void SetTarget(FLOAT *FloatToEdit);


protected:

	/** 
	 * updates the edit targert
	 */
	virtual void UpdateTargetUtil();

	/**
	 * The edit Box
	 */
	WEdit *PercentEdit;

	/**
	 * The percent track bar
	 */
	WTrackBar *PercentTrack;


private:

	/**
	 * The address of the FLOAT this Tool changes
	 */
	FLOAT *FloatToEdit;

	/**
	 * Uses to avoid delegate loops - if this is set, delegates can use this see if they 
	 * should do anything
	 */
	UBOOL DontCallDelegates;


	/**
	 * Delegate called on edit change
	 */
	void OnEditChange();

	/**
	 * Delegate called when the track bar changes
	 */
	void OnTrackChange();
};




/**
 * Tool for picking textures
 */
class WTexturePickEdit : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WTexturePickEdit,WParticleEditorTool,Window)

public:

	 //Constructor
	WTexturePickEdit(class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic);
	WTexturePickEdit(WWindow* inOwnerWindow, class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic);

	 //"Destructor"
	virtual void OnDestroy();

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();

	/**
	 * This function creates all the Tools and sets up the delegates
	 */
	virtual void OpenWindow( HMODULE InHMOD );

	/**
	 * Sets the target YO!
	 */
	virtual void SetTarget(UMaterial** InTextureToEdit);


protected:

	/** 
	 * updates the edit targert
	 */
	virtual void UpdateTargetUtil();


private:

	//variables

	/**
	 * The values for string
	 */
	WEdit* ValEdit;

	/**
	 * Button to use the current texture
	 */
	WButton* UseButton;

	/**
	 * a link to the material that this Tool is supposed to change 
	 */
	UMaterial** TextureToEdit;

	/**
	 * delegate for use button click
	 */
	void OnUseClick();
};




/**
 * Tool for picking Meshes
 */
class WMeshPickEdit : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WMeshPickEdit,WParticleEditorTool,Window)

public:

	 //Constructor
	WMeshPickEdit(class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic);

	 //"Destructor"
	virtual void OnDestroy();

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();

	/**
	 * This function creates all the Tools and sets up the delegates
	 */
	virtual void OpenWindow( HMODULE InHMOD );

	/**
	 * This function sets the edit target this tool points to and adjusts the mesh to edit.
	 */
	virtual void SetTarget(UStaticMesh** InMeshToEdit);

protected:

	/** 
	 * updates the edit targert
	 */
	virtual void UpdateTargetUtil();


private:

	//variables

	/**
	 * The values for string
	 */
	WEdit* ValEdit;

	/**
	 * Button to use the current texture
	 */
	WButton* UseButton;

	/**
	 * a link to the FRangeVector that this Tool is supposed to change 
	 */
	UStaticMesh** MeshToEdit;

	/**
	 * delegate for use button click
	 */
	void OnUseClick();
};



/**
 * Tool to edit vectors
 */
template<class T>
class WVectorEdit : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WVectorEdit,WParticleEditorTool,Window)

public:

	/**
	 * Creates a linear algebra notion of a vector (an ordered list of values) with 
	 * the given names and pointers to the data to read from and change
	 */
	WVectorEdit(class WParticleEditorComponent* OwnerComponent, FString VectorName, UBOOL InWholeNumbersOnly, INT InWidthOfText, DWORD ToolFlags, INT InUDNHelpTopic) :
		WParticleEditorTool( OwnerComponent, VectorName, ToolFlags, InUDNHelpTopic),
		WholeNumbersOnly(InWholeNumbersOnly),
		WidthOfText(InWidthOfText),
		UseMin(FALSE),
		UseMax(FALSE)
	{
	}


	/**
	 * Like the other constructor but can be placed in any window, not just a component
	 */
	WVectorEdit(WWindow* OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString VectorName, UBOOL InWholeNumbersOnly, INT InWidthOfText, DWORD ToolFlags, INT InUDNHelpTopic) :
		WParticleEditorTool(OwnerWindow, OwnerComponent, VectorName, ToolFlags, InUDNHelpTopic),
		WholeNumbersOnly(InWholeNumbersOnly),
		WidthOfText(InWidthOfText),
		UseMin(FALSE),
		UseMax(FALSE)
	{
	}


		
	/**
	 * Used like a destructor
	 */
	void OnDestroy()
	{
		for(INT i = 0;  i < FieldNames.Num(); i++)
		{
			if(ValLabels(i)){delete ValLabels(i);	ValLabels(i)	= NULL;}
			if(ValEdits(i))	{delete ValEdits(i);	ValEdits(i)		= NULL;}
			if(ValSlides(i)){delete ValSlides(i);	ValSlides(i)	= NULL;}
		}
	}



	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const
	{
		return FPoint(400,26);
	}



	/** 
	 * gets the edit field values from the FRangeVector VectorToEdit
	 */
	virtual void GetValuesFromTarget()
	{
		CallUpdateTarget = FALSE;

		for(INT i = 0;  i < FieldNames.Num(); i++)
		{
			ValEdits(i)->SetText( *GetStringFromData( PreTransform(*(data(i))) ) );
		}

		CallUpdateTarget = TRUE;
	}



	/**
	 * Sets up all the Edits and Sliders and also the delegates
	 */
	virtual void OpenWindow( HMODULE InHMOD )
	{
		WPropertyPage::OpenWindow( 0, InHMOD );

		INT curX = 0;
		for(INT i = 0;  i < FieldNames.Num(); i++)
		{

			ValLabels.AddItem( new WLabel(this, 0) );
			ValLabels(i)->OpenWindow(1, 0);
			::MoveWindow(ValLabels(i)->hWnd, curX, 3, WidthOfText, GetSize().Y, TRUE );
			ValLabels(i)->SetText( *FieldNames(i) );
			curX += WidthOfText;

			ValEdits.AddItem( new WEdit(this, 0) );
			ValEdits(i)->OpenWindow( TRUE, 0, 0 );
			::MoveWindow(ValEdits(i)->hWnd, curX, 0, 53, GetSize().Y-3, TRUE );
			curX += 53;

			ValSlides.AddItem( new WButtonSlider( this, ValEdits(i), 0, WholeNumbersOnly ) );
			ValSlides(i)->OpenWindow();
			::MoveWindow(ValSlides(i)->hWnd, curX, 0, 14, GetSize().Y, TRUE );
			curX += 14 + 5;
		}

		//GetValuesFromTarget();


		for(INT i = 0;  i < FieldNames.Num(); i++)
		{
			ValEdits(i)->ChangeDelegate = FDelegate(this, (TDelegate)OnTextChange);
		}
	}



	/**
	 * sets the Min value this vector can have
	 */
	void SetMin(T Min)
	{
		UseMin = TRUE;
		MinValue = Min;
	}



	/**
	 * sets the Max value this vector can have
	 */
	void SetMax(T Max)
	{
		UseMax = TRUE;
		MaxValue = Max;
	}



protected:

	/**
	 * The names of the fields
	 */
	TArray<FString> FieldNames;
	
	/**
	 * links to the data
	 */
	TArray<T*> data;

	/**
	 * allows the subclasses to choose how to get the data from the string
	 */
	virtual T GetDataFromString(FString DataString) = 0;



	/**
	 * allows the subclasses to choose how to generate a string from the data
	 */
	virtual FString GetStringFromData(T DataItem) = 0;



	/**
	 * a final transform applied to the data before the particle Emitter is updated
	 */
	virtual T FinalTransform(T ShownVal)
	{
		return ShownVal;
	}



	/**
	 * a pre-transform applied to the data as it is read into the editor
	 */
	virtual T PreTransform(T BasicValue)
	{
		return BasicValue;
	}



	/** 
	 * updates the edit targert
	 */
	virtual void UpdateTargetUtil()
	{
		for(INT i = 0;  i < FieldNames.Num(); i++)
		{
			T tempData = GetDataFromString(ValEdits(i)->GetText());
			if(UseMin && (tempData < MinValue))
			{
				tempData = MinValue;
				ValEdits(i)->SetText(*GetStringFromData(tempData));
			}
			if(UseMax && (tempData > MaxValue))
			{
				tempData = MaxValue;
				ValEdits(i)->SetText(*GetStringFromData(tempData));
			}
			*(data(i)) = FinalTransform(tempData);	
		}
	}


private:

	/**
	 * The Labels for each field
	 */
	TArray<WLabel*> ValLabels;

	/**
	 * The values for the components of the vector
	 */
	TArray<WEdit*> ValEdits;

	/**
	 * sliders for edit
	 */
	TArray<WButtonSlider*> ValSlides;

	/**
	 * The Min and Max values for this vector
	 */
	T MinValue;
	T MaxValue;

	/**
	 * to use or not to use mins and maxs
	 */
	UBOOL UseMin;
	UBOOL UseMax;

	/**
	 * if this vector should only work with whole numbers
	 */
	UBOOL WholeNumbersOnly;

	/**
	 * width of the text lables for each field
	 */
	INT WidthOfText;


	/**
	 * a Delegate that is called when one of the text boxes is changed
	 */
	void OnTextChange()
	{
		UpdateTarget();
	}
};




/**
 * Tool to edit vectors
 */
class WXYZVectorEdit : public WVectorEdit<FLOAT>
{
	DECLARE_WINDOWCLASS(WXYZVectorEdit,WVectorEdit<FLOAT>,Window)

public:

	/**
	 * Creates a vector with X, Y, and Z FLOAT fields
	 */
	WXYZVectorEdit(class WParticleEditorComponent* OwnerComponent, FString VectorName, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * Like the other constructor but can be placed in any window, not just a component
	 */
	WXYZVectorEdit(WWindow *OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString VectorName, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * This function sets the edit target this tool points to and adjusts the vector to edit.
	 */
	virtual void SetTarget(FVector *VectorToEdit);

 
protected:

	/**
	 * allows the subclasses to choose how to get the data from the string
	 */
	virtual FLOAT GetDataFromString(FString DataString);

	/**
	 * allows the subclasses to choose how to generate a string from the data
	 */
	virtual FString GetStringFromData(FLOAT DataItem);

};




/**
 * Tool to edit colors
 */
class WColorEdit : public WVectorEdit<BYTE>
{
	DECLARE_WINDOWCLASS(WColorEdit,WVectorEdit<BYTE>,Window)

public:

	/**
	 * Creates a color with R, G, B, and A byte fields
	 */
	WColorEdit(class WParticleEditorComponent* OwnerComponent, FString ColorName, FColor *ColorToEdit, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * Like the other constructor but can be placed in any window, not just a component
	 */
	WColorEdit(WWindow* OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString ColorName, FColor *ColorToEdit, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * This functions creates the pick button
	 */
	virtual void OpenWindow(HMODULE InHMOD);

	/**
	 * "Destructor"
	 */ 
	virtual void OnDestroy();

	/**
	 * Updates the color button
	 */
	virtual void UpdateTargetUtil();

protected:

	/**
	 * allows the subclasses to choose how to get the data from the string
	 */
	virtual BYTE GetDataFromString(FString DataString);

	/**
	 * allows the subclasses to choose how to generate a string from the data
	 */
	virtual FString GetStringFromData(BYTE DataItem);

private:

	/**
	 * Sets up the color edit. Called by constructors
	 */
	void Init();


	/**
	 * A pointer to the color we're editing
	 */
	FColor* ColorToEdit;

	/**
	 * Opens the color picker
	 */
	WColorButton* PickButton;

	/**
	 * called when PickButton is Clicked
	 */ 
	void PickClicked();

	/**
	 * the color picker icon
	 */
	HBITMAP ColorPickBitmap;

};




/**
 * Tool to edit colors multipliers
 */
class WColorMultEdit : public WVectorEdit<FLOAT>
{
	DECLARE_WINDOWCLASS(WColorMultEdit,WVectorEdit<FLOAT>,Window)

public:

	/**
	 * Creates a color mult with R, G, B, and A FLOAT fields
	 */
	WColorMultEdit(class WParticleEditorComponent* OwnerComponent, FString ColorName, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * This function sets the edit target this tool points to and adjusts the color to edit.
	 */
	virtual void SetTarget(FPlane *ColorToEdit);


protected:

	/**
	 * allows the subclasses to choose how to get the data from the string
	 */
	virtual FLOAT GetDataFromString(FString DataString);

	/**
	 * allows the subclasses to choose how to generate a string from the data
	 */
	virtual FString GetStringFromData(FLOAT DataItem);

};




/**
 * Tool to edit planes
 */
class WPlaneEdit : public WVectorEdit<FLOAT>
{
	DECLARE_WINDOWCLASS(WPlaneEdit,WVectorEdit<FLOAT>,Window)

public:

	/**
	 * Creates a plane with X, Y, Z, and W FLOAT fields
	 */
	WPlaneEdit(class WParticleEditorComponent* OwnerComponent, FString PlaneName, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * Like the other constructor but can be placed in any window, not just a component
	 */
	WPlaneEdit(WWindow* OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString PlaneName, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * This function sets the edit target this tool points to and adjusts the plane to edit.
	 */
	virtual void SetTarget(FPlane *PlaneToEdit);


protected:

	/**
	 * allows the subclasses to choose how to get the data from the string
	 */
	virtual FLOAT GetDataFromString(FString DataString);

	/**
	 * allows the subclasses to choose how to generate a string from the data
	 */
	virtual FString GetStringFromData(FLOAT DataItem);

};




/**
 * Tool to edit rotators
 */
class WRotatorEdit : public WVectorEdit<INT>
{
	DECLARE_WINDOWCLASS(WRotatorEdit,WVectorEdit<INT>,Window)

public:

	/**
	 * Creates a rotator with Pitch, Yaw, and Roll INT fields
	 */
	WRotatorEdit(class WParticleEditorComponent* OwnerComponent, FString RotatorName, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * This function sets the edit target this tool points to and adjusts the vector to edit.
	 */
	virtual void SetTarget(FRotator *RotatorToEdit);


protected:

	/**
	 * changes from 360 degree to wacky unreal system
	 */
	virtual INT FinalTransform(INT ShownVal);

	/**
	 * changes wacky unreal system to 360 degree 
	 */
	virtual INT PreTransform(INT BasicValue);

	/**
	 * allows the subclasses to choose how to get the data from the string
	 */
	virtual INT GetDataFromString(FString DataString);

	/**
	 * allows the subclasses to choose how to generate a string from the data
	 */
	virtual FString GetStringFromData(INT DataItem);

};




/**
 * Tool to edit a single FLOAT
 */
class WFloatEdit : public WVectorEdit<FLOAT>
{
	DECLARE_WINDOWCLASS(WFloatEdit,WVectorEdit<FLOAT>,Window)

public:

	/**
	 * Creates a field to edit a single INT edit
	 */
	WFloatEdit(class WParticleEditorComponent* OwnerComponent, FString FloatName, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * Like the other constructor but can be placed in any window, not just a component
	 */
	WFloatEdit(WWindow* OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString FloatName, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * This function sets the edit target this tool points to and adjusts the float to edit.
	 */
	virtual void SetTarget(FLOAT* FloatToEdit);

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;


protected:

	/**
	 * allows the subclasses to choose how to get the data from the string
	 */
	virtual FLOAT GetDataFromString(FString DataString);

	/**
	 * allows the subclasses to choose how to generate a string from the data
	 */
	virtual FString GetStringFromData(FLOAT DataItem);

};




/**
 * Tool to edit a single ints
 */
class WIntEdit : public WVectorEdit<INT>
{
	DECLARE_WINDOWCLASS(WIntEdit,WVectorEdit<INT>,Window)

public:

	/**
	 * Creates a field to edit a single INT edit
	 */
	WIntEdit(class WParticleEditorComponent* OwnerComponent, FString IntName, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * Like the other constructor but can be placed in any window, not just a component
	 */
	WIntEdit(WWindow *OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString IntName, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * This function sets the edit target this tool points to and adjusts the int to edit.
	 */
	virtual void SetTarget(INT* IntToEdit);


protected:

	/**
	 * allows the subclasses to choose how to get the data from the string
	 */
	virtual INT GetDataFromString(FString DataString);

	/**
	 * allows the subclasses to choose how to generate a string from the data
	 */
	virtual FString GetStringFromData(INT DataItem);

};




/**
 * Tool to edit a single non negative integer (0, 1, 2, ...)
 */
class WNoNegNumberEdit : public WIntEdit
{
	DECLARE_WINDOWCLASS(WNoNegNumberEdit,WIntEdit,Window)

public:

	/**
	 * Creates a field to edit a single non negative integer (0, 1, 2, ...)
	 */
	WNoNegNumberEdit(class WParticleEditorComponent* OwnerComponent, FString NumberName, DWORD ToolFlags, INT InUDNHelpTopic);
};




/**
 * Tool to do fading
 * fading works with a radio of the average lifetime as opposed to absolute time
 */
class WFadingEdit : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WFadingEdit,WParticleEditorTool,Window)

public:

	 //Constructor
	WFadingEdit(class WParticleEditorComponent* OwnerComponent, DWORD ToolFlags, INT InUDNHelpTopic);

	 //"Destructor"
	virtual void OnDestroy();

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * This function creates all the Tools and sets up the delegates
	 */
	virtual void OpenWindow( HMODULE InHMOD );


protected:

	/** 
	 * updates the edit targert
	 */
	virtual void UpdateTargetUtil();


private:

	//variables

	WEdit* FadeInEdit;
	WEdit* FadeOutEdit;

	WTrackBar *FadeInBar;
	WTrackBar *fadeOutBar;
	
	WCheckBox *LockSlidersCheck;

	UBOOL LockSliders;
	UBOOL SliderDiff;

	HBITMAP LockSlidersBitmap;


	//functions

	/**
	 * Called when the fade in bar changes
	 */
	void FadeInChange();

	/**
	 * Called when the fade out bar changes
	 */
	void FadeOutChange();

	/**
	 * Called when lock sliders is Clicked
	 */
	void LockSlidersClick();
};





/**
 * Tool to scale the given Emitter.  This scales size and velocity and acceleration
 * and all other things so the Emitter looks the same but is a different size
 */
class WScaleEmitterEdit : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WScaleEmitterEdit,WParticleEditorTool,UnrealEd)

public:

	// Constructor.
	WScaleEmitterEdit( class WParticleEditorComponent* OwnerComponent, DWORD ToolFlags);

	// "Destructor"
	virtual void OnDestroy();

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * This function creates all the Tools and sets up the delegates
	 */
	virtual void OpenWindow( HMODULE InHMOD );


protected:

	/** 
	 * does not really update the edit target, just used to do Min
	 */
	virtual void UpdateTargetUtil();


private:

	WButton *ApplyButton;
	WEdit *ScaleEdit;
	WButtonSlider* ScaleSlide;

	void OnApply();

};




/**
 * Tool to scale the given Emitter.  This scales size and velocity and acceleration
 * and all other things so the Emitter looks the same but is a different size
 */
class WSpeedScaleEdit : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WSpeedScaleEdit,WParticleEditorTool,UnrealEd)

public:

	// Constructor.
	WSpeedScaleEdit( class WParticleEditorComponent* OwnerComponent, DWORD ToolFlags);

	// "Destructor"
	virtual void OnDestroy();

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * This function creates all the Tools and sets up the delegates
	 */
	virtual void OpenWindow( HMODULE InHMOD );


protected:

	/** 
	 * does not really update the edit target, just used to do Min
	 */
	virtual void UpdateTargetUtil();


private:

	WButton *ApplyButton;
	WEdit *ScaleEdit;
	WButtonSlider* ScaleSlide;

	void OnApply();
};




/**
 * A struct container for WParticleEditorTools
 */
class WParticleStructTool : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WParticleStructTool,WParticleEditorTool,Window)

public:

	/**
	 * A Tool that contains other Tools
	 */
	WParticleStructTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, INT LabelWidth, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * Like the other constructor but can be placed in any window, not just a component
	 */
	WParticleStructTool(WWindow *OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString ToolName, INT LabelWidth, DWORD ToolFlags, INT InUDNHelpTopic);

	/**
	 * "Destructor", called when destroyed
	 */
	virtual void OnDestroy();

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();

	/**
	 * Enables or Disables (grays out) the Tool depending on the value passed
	 */
	virtual void Enable(UBOOL Enabled);

	/**
	 * Adds the given Tool to this struct at the bottom
	 * To add Tools, this should be called before OpenWindow
	 */
	void AddStructTool(WParticleEditorTool *NewTool, FString FieldName);

	/**
	 * This function creates all the Tools and sets up the delegates
	 */
	virtual void OpenWindow( HMODULE InHMOD );


protected:

	/**
	 * This should be implemented by the subclasses so UpdateTarget works
	 * it should NEVER be called dirrectly by a subclass
	 * This call is "singular" in that it will not loop recursively
	 */
	virtual void UpdateTargetUtil();

private:

	/**
	 * The ordered list of the set of Tools in this struct
	 */
	TArray<WParticleEditorTool*> Tools;

	/**
	 * The ordered list of field names that are associated with each Tool
	 */
	TArray<FString> FieldNames;

	/**
	 * The ordered list of labes with the text from field names that are associated with each Tool
	 */
	TArray<WLabel*> Labels;

	/**
	 * The width of Labels - before the Tools start
	 */
	INT LabelWidth;

	/**
	 * The space between Tools
	 */
	INT SpaceBetweenTools;
};




/**
 * Abstract super class for editing arrays
 */
template<class T>
class WParticleArrayTool : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WParticleArrayTool,WParticleEditorTool,Window)

public:

	/**
	 * Creates a array edit
	 */
	WParticleArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic) :
		WParticleEditorTool( OwnerComponent, ToolName, ToolFlags, InUDNHelpTopic ),
		DataArrayToEdit(NULL),
		InitialVertOffset(25),
		IndentAmount(10),
		SpaceBetweenTools(10)
	{
	}



	/**
	 * "Destructor", called when destroyed
	 */
	virtual void OnDestroy()
	{
		if(InsertButton) { delete(InsertButton); InsertButton = NULL; }
		if(DeleteButton) { delete(DeleteButton); DeleteButton = NULL; }

		for(INT i = 0; i < Tools.Num(); i++)
		{
			delete(Tools(i));
		}
		Tools.Empty();
	}



	/**
	 * This function sets the edit target this tool points to and adjusts the data array to edit.
	 */
	virtual void SetTarget(TArray<T> *InDataArrayToEdit)
	{
		WParticleEditorTool::SetTarget();
		DataArrayToEdit = InDataArrayToEdit;

		for(INT i = 0; i < Tools.Num(); i++)
			delete Tools(i);
		Tools.Empty();
		
		for(INT i = 0; i < DataArrayToEdit->Num(); i++)
		{	
			T* DataItemPoint = &( (*DataArrayToEdit)(i) );
			WParticleEditorTool* Tool = CreateTool(DataItemPoint);
			AddTool( Tool );
			Tool->GetValuesFromTarget();
		}
	}



	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const
	{
		FPoint Ret;
		Ret.X = 500;
		Ret.Y = InitialVertOffset;

		for(INT i = 0; i < Tools.Num(); i++)
		{
			Ret.Y += Tools(i)->GetSize().Y + SpaceBetweenTools;
		}

		return Ret;
	}



	/**
	 * Enables or Disables (grays out) the Tool depending on the value passed
	 */
	virtual void Enable(UBOOL Enabled)
	{
		EnableWindow( InsertButton->hWnd, Enabled );
		EnableWindow( DeleteButton->hWnd, Enabled );

		for(INT i = 0; i < Tools.Num(); i++)
			Tools(i)->Enable(Enabled);
	}



	/**
	 * reads the values from the target into this Tool
	 * Be very careful overriding this function...says chris.
	 */
	virtual void GetValuesFromTarget()
	{
		if(Tools.Num() == DataArrayToEdit->Num())
		{
			for(INT i = 0; i < Tools.Num(); i++)
			{
				Tools(i)->GetValuesFromTarget();
			}
		}
		else
		{
			for(INT i = 0; i < Tools.Num(); i++)
				delete Tools(i);
			Tools.Empty();
			
			for(INT i = 0; i < DataArrayToEdit->Num(); i++)
			{	
				T* DataItemPoint = &( (*DataArrayToEdit)(i) );
				WParticleEditorTool* Tool = CreateTool(DataItemPoint);
				AddTool( Tool );
				Tool->GetValuesFromTarget();
			}
		}
	}



	/**
	 * This function creates all the Tools and sets up the delegates
	 */
	virtual void OpenWindow( HMODULE InHMOD )
	{
		WPropertyPage::OpenWindow( 0, InHMOD );

		InsertButton = new WButton(this);
		InsertButton->OpenWindow( TRUE, 0, 0, 40, 20, TEXT("Insert") );

		DeleteButton = new WButton(this);
		DeleteButton->OpenWindow( TRUE, 40, 0, 40, 20, TEXT("Delete") );

		InsertButton->ClickDelegate = FDelegate(this, (TDelegate)OnInsertClick);
		DeleteButton->ClickDelegate = FDelegate(this, (TDelegate)OnDeleteClick);
	}


protected:

	/**
	 * This should be implemented by the subclasses so UpdateTarget works
	 * it should NEVER be called dirrectly by a subclass
	 * This call is "singular" in that it will not loop recursively
	 * This does not necessarily sync the two arrays because it does not take the
	 * number of elements into account.
	 */
	virtual void UpdateTargetUtil()
	{
		for(INT i = 0; i < Tools.Num(); i++)
			Tools(i)->UpdateTarget();
	}



	/**
	 * Creates a WParticleEditorTools to edit the given data
	 */
	virtual WParticleEditorTool* CreateTool(T* DataToEdit) = 0;


private:

	/**
	 * Adds WParticleEditorTool to this array edit
	 */
	void AddTool(WParticleEditorTool *NewTool)
	{
		NewTool->OpenWindow(GetModuleHandleA("unrealed.exe"));
		NewTool->GetValuesFromTarget();

		::MoveWindow(NewTool->hWnd, IndentAmount, GetSize().Y,  NewTool->GetSize().X, NewTool->GetSize().Y, TRUE);

		Tools.AddItem(NewTool);

		OwnerComponent->SetToolAnchors();
		OwnerComponent->GetParentTab()->Refresh();
	}



	/** 
	 * Called when the InsertButton is Clicked - inserts at the back
	 */
	void OnInsertClick()
	{
		DataArrayToEdit->AddZeroed();

		T* DataItemPoint = &( (*DataArrayToEdit)(DataArrayToEdit->Num()-1) );
		AddTool( CreateTool(DataItemPoint) );
	}



	/**
	 * Called when delete is Clicked - deletes the last element
	 */
	void OnDeleteClick()
	{
		INT index = Tools.Num() - 1;
		delete(Tools(index));
		Tools.Remove(index);

		DataArrayToEdit->Remove(index);

		OwnerComponent->SetToolAnchors();
		OwnerComponent->GetParentTab()->Refresh();
	}


	/**
	 * The ordered list of the set of Tools this array has
	 */
	TArray<WParticleEditorTool*> Tools;

	/**
	 * The data array to edit
	 */
	TArray<T> *DataArrayToEdit;

	/**
	 * The button that inserts a new array element
	 */
	WButton *InsertButton;

	/**
	 * The button that deletes the Selected array elements
	 */
	WButton *DeleteButton;

	//alignment vars
	INT InitialVertOffset;
	INT IndentAmount;
	INT SpaceBetweenTools;

};



/**
 * a class for editing texture arrays
 */
class WParticleTextureArrayTool : public WParticleArrayTool<UMaterial*>
{
	DECLARE_WINDOWCLASS(WParticleTextureArrayTool,WParticleArrayTool<UMaterial*>,Window)

public:

	/**
	 * Creates a array edit
	 */
	WParticleTextureArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic);


protected:

	/**
	 * Creates a WParticleEditorTools to edit the given data
	 */
	virtual WParticleEditorTool* CreateTool(UMaterial** DataToEdit);

};



/**
 * a class for editing FLOAT arrays
 */
class WParticleFloatArrayTool : public WParticleArrayTool<FLOAT>
{
	DECLARE_WINDOWCLASS(WParticleFloatArrayTool,WParticleArrayTool<FLOAT>,Window)

public:

	/**
	 * Creates a array edit
	 */
	WParticleFloatArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic);


protected:

	/**
	 * Creates a WParticleEditorTools to edit the given data
	 */
	virtual WParticleEditorTool* CreateTool(FLOAT* DataToEdit);

};




/**
 * a class for editing plane arrays
 */
class WParticlePlaneArrayTool : public WParticleArrayTool<FPlane>
{
	DECLARE_WINDOWCLASS(WParticlePlaneArrayTool,WParticleArrayTool<FPlane>,Window)

public:

	/**
	 * Creates a array edit
	 */
	WParticlePlaneArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic);

protected:

	/**
	 * Creates a WParticleEditorTools to edit the given data
	 */
	virtual WParticleEditorTool* CreateTool(FPlane* DataToEdit);

};




/**
 * a class for editing ParticleTimeScales arrays
 */
class WParticleSizeScaleArrayTool : public WParticleArrayTool<FParticleTimeScale>
{
	DECLARE_WINDOWCLASS(WParticleSizeScaleArrayTool,WParticleArrayTool<FParticleTimeScale>,Window)

public:

	/**
	 * Creates a array edit
	 */
	WParticleSizeScaleArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic);

protected:

	/**
	 * Creates a WParticleEditorTools to edit the given data
	 */
	virtual WParticleEditorTool* CreateTool(FParticleTimeScale* DataToEdit);

};




///**
// * a class for editing ParticleColorScales arrays
// */
//class WParticleColorScaleArrayTool : public WParticleArrayTool<FParticleColorScale>
//{
//	DECLARE_WINDOWCLASS(WParticleColorScaleArrayTool,WParticleArrayTool<FParticleColorScale>,Window)
//
//public:
//
//	/**
//	 * Creates a array edit
//	 */
//	WParticleColorScaleArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic);
//
//protected:
//
//	/**
//	 * Creates a WParticleEditorTools to edit the given data
//	 */
//	virtual WParticleEditorTool* CreateTool(FParticleColorScale* DataToEdit);
//
//};




/**
 * a class for editing FParticleBeamEndPoint arrays
 */
class WParticleBeamEndPointArrayTool : public WParticleArrayTool<FParticleBeamEndPoint>
{
	DECLARE_WINDOWCLASS(WParticleBeamEndPointArrayTool,WParticleArrayTool<FParticleBeamEndPoint>,Window)

public:

	/**
	 * Creates a array edit
	 */
	WParticleBeamEndPointArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic);

protected:

	/**
	 * Creates a WParticleEditorTools to edit the given data
	 */
	virtual WParticleEditorTool* CreateTool(FParticleBeamEndPoint* DataToEdit);

};




/**
 * a class for editing FParticleBeamEndPoint arrays
 */
class WParticleBeamScaleArrayTool : public WParticleArrayTool<FParticleBeamScale>
{
	DECLARE_WINDOWCLASS(WParticleBeamScaleArrayTool,WParticleArrayTool<FParticleBeamScale>,Window)

public:

	/**
	 * Creates a array edit
	 */
	WParticleBeamScaleArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic);

protected:

	/**
	 * Creates a WParticleEditorTools to edit the given data
	 */
	virtual WParticleEditorTool* CreateTool(FParticleBeamScale* DataToEdit);

};
//TArray<FParticleBeamScale> *DataArrayToEdit, 



/**
 * Tool for picking textures
 */
class WSoundPickEdit : public WParticleEditorTool
{
	DECLARE_WINDOWCLASS(WSoundPickEdit,WParticleEditorTool,Window)

public:

	 //Constructor
	WSoundPickEdit(class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic);
	WSoundPickEdit(WWindow* inOwnerWindow, class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic);

	 //"Destructor"
	virtual void OnDestroy();

	/**
	 * returns the size of this Tool
	 */
	virtual FPoint GetSize() const;

	/**
	 * reads the values from the target into this Tool
	 */
	virtual void GetValuesFromTarget();

	/**
	 * This function creates all the Tools and sets up the delegates
	 */
	virtual void OpenWindow( HMODULE InHMOD );

	/**
	 * This function sets the edit target this tool points to and adjusts the sound vector to edit.
	 */
	virtual void SetTarget(USound** InSoundToEdit);


protected:

	/** 
	 * updates the edit targert
	 */
	virtual void UpdateTargetUtil();


private:

	//variables

	/**
	 * The values for string
	 */
	WEdit* ValEdit;

	/**
	 * Button to use the current texture
	 */
	WButton* UseButton;

	/**
	 * a link to the material that this Tool is supposed to change 
	 */
	USound** SoundToEdit;

	/**
	 * delegate for use button click
	 */
	void OnUseClick();
};



/**
 * a class for editing FParticleSound arrays
 */
class WParticleSoundArrayTool : public WParticleArrayTool<FParticleSound>
{
	DECLARE_WINDOWCLASS(WParticleSoundArrayTool,WParticleArrayTool<FParticleSound>,Window)

public:

	/**
	 * Creates a array edit
	 */
	WParticleSoundArrayTool(class WParticleEditorComponent* OwnerComponent, FString ToolName, DWORD ToolFlags, INT InUDNHelpTopic);

protected:

	/**
	 * Creates a WParticleEditorTools to edit the given data
	 */
	virtual WParticleEditorTool* CreateTool(FParticleSound* DataToEdit);

};



/**
 * a class for editing the name of particle systems
 */
class WNameTool : public WStringEdit
{
	DECLARE_WINDOWCLASS(WNameTool,WStringEdit,Window)

public:

	 //Constructor
	WNameTool(class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic);

	 //Constructor
	WNameTool(WWindow *OwnerWindow, class WParticleEditorComponent* OwnerComponent, FString InStringName, DWORD ToolFlags, INT InUDNHelpTopic);

protected:

	/** 
	 * updates the edit targert
	 */
	virtual void UpdateTargetUtil();

};



#endif
