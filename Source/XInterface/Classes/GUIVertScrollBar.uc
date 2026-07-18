// ====================================================================
//  Class:  XInterface.GUIVertScrollBar
//	Parent: Xinterface.GUIScrollBarBase
//
//  Custom scrollbar for vertical lists
// ====================================================================

class GUIVertScrollBar extends GUIScrollBarBase;

// Record location you grabbed the grip
function GripPressed( GUIComponent Sender, bool IsRepeat )
{
	if ( !IsRepeat )
		GrabOffset = Controller.MouseY - MyGripButton.ActualTop();
}

function bool GripPreDraw( GUIComponent Sender )
{
	local float NewPerc;

	if ( MyGripButton.MenuState != MSAT_Pressed )
		return false;

	// Calculate the new Grip Top using the mouse cursor location.
	NewPerc = FClamp(
		(Controller.MouseY - GrabOffset - MyScrollZone.ActualTop()) / (MyScrollZone.ActualHeight() - GripSize),
		0.0, 1.0 );

	UpdateGripPosition(NewPerc);

	return true;
}

function ZoneClick(float Delta)
{
	if ( Controller.MouseY < MyGripButton.Bounds[1] )
		MoveGripBy(-BigStep);
	else if ( Controller.MouseY > MyGripButton.Bounds[3] )
		MoveGripBy(BigStep);

	return;
}

defaultproperties
{
	Orientation=ORIENT_Vertical

	Begin Object Class=GUIVertScrollZone Name=ScrollZone
		OnScrollZoneClick=ZoneClick
	End Object

	Begin Object Class=GUIVertScrollButton Name=UpBut
		OnClick=DecreaseClick
	End Object

	Begin Object Class=GUIVertScrollButton Name=DownBut
		OnClick=IncreaseClick
		bIncreaseButton=true
	End Object

	Begin Object Class=GUIVertGripButton Name=Grip
		OnMousePressed=GripPressed
	End Object

	OnPreDraw=GripPreDraw
	MyScrollZone=ScrollZone
	MyDecreaseButton=UpBut
	MyIncreaseButton=DownBut
	MyGripButton=Grip

	bAcceptsInput=true
	WinWidth=0.025
	MinGripPixels=12
}
