// ====================================================================
//  Class:  XInterface.UT2QuitPage
//  Parent: XInterface.GUIMultiComponent
//
//  <Enter a description here>
// ====================================================================

class UT2QuitPage extends UT2K3GUIPage;


function bool InternalOnClick(GUIComponent Sender)
{
	if (Sender==Controls[1])
	{
		if(PlayerOwner().Level.IsDemoBuild())
			Controller.ReplaceMenu("XInterface.UT2DemoQuitPage");
		else
			PlayerOwner().ConsoleCommand("exit");
	}
	else
		Controller.CloseMenu(false);
	
	return true;
}

defaultproperties
{

	Begin Object Class=GUIButton name=QuitBackground
		WinWidth=1.0
		WinHeight=1.0
		WinTop=0
		WinLeft=0
		bAcceptsInput=false
		bNeverFocus=true
		StyleName="SquareBar"
		bBoundToParent=true
		bScaleToParent=true
	End Object
	Controls(0)=GUIButton'QuitBackground'
	
	Begin Object Class=GUIButton Name=YesButton
		Caption="YES"
		WinWidth=0.2
		WinHeight=0.04
		WinLeft=0.125
		WinTop=0.75
		bBoundToParent=true
		OnClick=InternalOnClick
	End Object
	Controls(1)=GUIButton'YesButton'

	Begin Object Class=GUIButton Name=NoButton
		Caption="NO"
		WinWidth=0.2
		WinHeight=0.04
		WinLeft=0.65
		WinTop=0.75
		bBoundToParent=true
		OnClick=InternalOnClick
	End Object
	Controls(2)=GUIButton'NoButton'

	Begin Object class=GUILabel Name=QuitDesc
		Caption="Are you sure you wish to quit?"
		TextALign=TXTA_Center
		TextColor=(R=220,G=180,B=0,A=255)
		TextFont="UT2HeaderFont"
		WinWidth=1
		WinLeft=0
		WinTop=0.4
		WinHeight=32
	End Object
	Controls(3)=GUILabel'QuitDesc'
	WinLeft=0
	WinTop=0.375
	WinWidth=1
	WinHeight=0.25
	bRequire640x480=false	
}
