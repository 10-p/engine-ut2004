class UT2PerformWarn extends UT2K3GUIPage;

function bool InternalOnClick(GUIComponent Sender)
{
	Controller.CloseMenu(false);
	return true;
}

defaultproperties
{
	Begin Object Class=GUIButton name=DialogBackground
		WinWidth=1.0
		WinHeight=1.0
		WinTop=0
		WinLeft=0
		bAcceptsInput=false
		bNeverFocus=true
		StyleName="ListBox"
		bBoundToParent=true
		bScaleToParent=true
	End Object
	Controls(0)=GUIButton'DialogBackground'

	Begin Object Class=GUIButton Name=OkButton
		Caption="OK"
		WinWidth=0.2
		WinHeight=0.04
		WinLeft=0.4
		WinTop=0.55
		OnClick=InternalOnClick
	End Object
	Controls(1)=GUIButton'OkButton'

	Begin Object class=GUILabel Name=DialogText
		Caption="WARNING"
		TextALign=TXTA_Center
		TextColor=(R=14,G=41,B=106,A=255)
		TextFont="UT2HeaderFont"
		WinWidth=1
		WinLeft=0
		WinTop=0.4
		WinHeight=32
	End Object
	Controls(2)=GUILabel'DialogText'

	Begin Object class=GUILabel Name=DialogText2
		Caption="The change you are making may adversely affect your performance."
		TextALign=TXTA_Center
		TextColor=(R=14,G=41,B=106,A=255)
		TextFont="UT2MenuFont"
		WinWidth=1
		WinLeft=0
		WinTop=0.45
		WinHeight=32
	End Object
	Controls(3)=GUILabel'DialogText2'

	WinLeft=0
	WinTop=0.375
	WinWidth=1
	WinHeight=0.25
	bRequire640x480=false
}
