// ====================================================================
//  Class:  XInterface.GUIHorzScrollBar
//  Parent: XInterface.GUIScrollBarBase
//
//  Scrollbar assembly for horizontal lists.
// ====================================================================

class GUIHorzScrollBar extends GUIScrollBarBase;

// Record location you grabbed the grip
function GripPressed( GUIComponent Sender, bool IsRepeat )
{
	if ( !IsRepeat )
		GrabOffset = Controller.MouseX - MyGripButton.ActualLeft();
}

function bool GripPreDraw( GUIComponent Sender )
{
	local float NewPerc;

	if ( MyGripButton.MenuState != MSAT_Pressed )
		return false;

	// Calculate the new Grip Top using the mouse cursor location.
	NewPerc = FClamp(
		(Controller.MouseX - GrabOffset - MyScrollZone.ActualLeft()) / (MyScrollZone.ActualWidth() - GripSize),
		0.0, 1.0 );

	UpdateGripPosition(NewPerc);

	return false;
}

function ZoneClick(float Delta)
{
	if ( Controller.MouseX < MyGripButton.Bounds[0] )
		MoveGripBy(-BigStep);
	else if ( Controller.MouseX > MyGripButton.Bounds[2] )
		MoveGripBy(BigStep);

	return;
}

defaultproperties
{
	Begin Object Class=GUIHorzScrollZone Name=HScrollZone
		OnScrollZoneClick=ZoneClick
	End Object

	Begin Object Class=GUIHorzScrollButton Name=HLeftBut
		OnClick=DecreaseClick
	End Object

	Begin Object Class=GUIHorzScrollButton Name=HRightBut
		OnClick=IncreaseClick
		bIncreaseButton=True
	End Object

	Begin Object Class=GUIHorzGripButton Name=HGrip
		OnMousePressed=GripPressed
	End Object

	OnPreDraw=GripPreDraw
	MyScrollZone=HScrollZone
	MyDecreaseButton=HLeftBut
	MyIncreaseButton=HRightBut
	MyGripButton=HGrip

	bAcceptsInput=true
	WinWidth=0.0375
	Orientation=ORIENT_Horizontal
	MinGripPixels=12
}
