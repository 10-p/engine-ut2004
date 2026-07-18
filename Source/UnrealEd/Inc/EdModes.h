/*=============================================================================
	EdModes.h: Classes which represent "modes" in the editor
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/*------------------------------------------------------------------------------
	UEdMode
------------------------------------------------------------------------------*/
class UNREALED_API UEdMode
{
public:
	// Constructor.
	UEdMode();
	virtual ~UEdMode();

	virtual void MouseButtonDown( UViewport* InViewport, DWORD InButtons, INT InX, INT InY );
	virtual void MouseButtonUp( UViewport* InViewport, DWORD InButtons, INT InX, INT InY );
	virtual void MouseButtonDblClick( UViewport* InViewport, DWORD InButtons, INT InX, INT InY );
	virtual void MouseMoved( UViewport* InViewport, DWORD InButtons, INT InDeltaX, INT InDeltaY );
	virtual void Clicked( const FHitCause* InHitCause, HHitProxy* InHitProxy );

	FString BitmapName,		// The name of the graphic to use on the left hand bar
		ModeName,			// The name of the mode (to pass to UnEdSrv)
		ToolTip;			// Tooltip to display in the editor button bar
};

/*------------------------------------------------------------------------------
	UEdModeCamera
------------------------------------------------------------------------------*/
class UNREALED_API UEdModeCamera : public UEdMode
{
public:
	// Constructor.
	UEdModeCamera();
	virtual ~UEdModeCamera();

};

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/

