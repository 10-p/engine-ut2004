// ====================================================================
//  Class:  UT2K4UI.UT2SpinnerButton
//
//  Written by Joe Wilcox
//  (c) 2002, Epic Games, Inc.  All Rights Reserved
// ====================================================================

class GUISpinnerButton extends GUIButton
	Native;

cpptext
{
	void Draw(UCanvas* Canvas);
}

delegate bool OnPlusClick(GUIComponent Sender) { return false; }
delegate bool OnMinusClick(GUIComponent Sender) { return false; }

function bool InternalOnClick(GUIComponent Sender)
{

	local float x,y;

	x = Controller.MouseX - ActualLeft();
    y = Controller.MouseY - ActualTop();

    if (y <= (ActualHeight()/2) )
    	return OnPlusClick(Sender);
    else
    	return OnMinusClick(Sender);
}


defaultproperties
{
	OnClick=InternalOnClick
	StyleName="SpinnerButton"
	bNeverFocus=true
	bRepeatClick=true
	bCaptureMouse=true
	bRequiresStyle=True
}
