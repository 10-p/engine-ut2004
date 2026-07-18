// ====================================================================
//  Class:  XInterface.JoeTestPanelA
//  Parent: XInterface.GUITabPanel
//
//  <Enter a description here>
// ====================================================================

class JoeTestPanelA extends TestPanelBase;

function InitPanel()
{
}

defaultproperties
{
	Begin Object Class=GUIButton Name=TestButtonA
		Caption="Wow"
		WinWidth=0.5
		WinLeft=0.25
		WinTop=0.25
		WinHeight=48
		Name="TestButtonA"
	End Object
	Controls(0)=GUIButton'TestButtonA'
	Begin Object Class=GUIButton Name=TestButtonB
		Caption="Damn"
		WinWidth=0.5
		WinLeft=0.25
		WinTop=0.55
		WinHeight=48
		Name="TestButtonB"
	End Object
	Controls(1)=GUIButton'TestButtonB'
	WinWidth=1.0
	WinHeight=0.3
	WinTop=0.65
	WinLeft=0
	Background=Material'InterfaceContent.Backgrounds.bg10'
}
