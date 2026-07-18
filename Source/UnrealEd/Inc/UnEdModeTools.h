/*=============================================================================
	UnEdModeTools.h: Classes which represent "modes" in the editor
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

enum EMODE
{
	EMODE_CAMERA	= 0,
	EMODE_MAX		= 1
};

class UNREALED_API FEdModeTools
{
public:
	// Constructor.
	FEdModeTools();
	virtual ~FEdModeTools();

	void Init();
	void SetMode( INT InMode );
	void CalcFreeMoveRot( UViewport* Viewport, FLOAT DeltaX, FLOAT DeltaY, FVector& Delta, FRotator& DeltaRot );
	void ViewportMoveRotCamera( UViewport* Viewport, FVector& Delta, FRotator& DeltaRot );
	void MoveActors( UViewport* Viewport, ULevel* Level, FVector Delta, FRotator DeltaRot, UBOOL Constrained, AActor* ViewActor, UBOOL bForceSnapping = 0 );
	void MoveSingleActor( AActor* Actor, FVector Delta, FRotator DeltaRot );

	inline UEdMode* GetCurrentMode() { return &Modes(Mode); }
	inline INT GetCurrentModeID() { return Mode; }
	inline UBOOL HasMouseMoved();

	INT Mode;					// The current mode the editor is in (eMODE_) - also is an index into "Modes"
	TArray<UEdMode> Modes;		// The list of available editor modes
	FVector ClickStartScreen,	// The 2D location where the mouse was initially clicked
		ClickEndScreen;			// The 2D location of the mouse when the buttons were let up
	FVector ClickLocation;		// The 3D location where the mouse was last clicked.
	FPlane ClickPlane;			// The plane where the mouse was last clicked.
	UBOOL bMouseHasMoved;		// If 1, the mouse has moved since the last button was pushed down
};

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/
