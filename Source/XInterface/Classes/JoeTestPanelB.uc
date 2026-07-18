// ====================================================================
//  Class:  XInterface.JoeTestPanelA
//  Parent: XInterface.GUITabPanel
//
//  <Enter a description here>
// ====================================================================

class JoeTestPanelB extends TestPanelBase;

function InitPanel()
{
}

defaultproperties
{
	Begin Object Class=GUIEditBox Name=TestEditB
		TextStr="DAMN HOT"
		WinWidth=0.80
		WinLeft=0.1
		WinTop=0.5
		WinHeight=48
		Name="TestEditB"
	End Object
	Controls(0)=GUIEditBox'TestEditB'
	WinWidth=1.0
	WinHeight=0.4
	WinTop=0.6
	WinLeft=0
	Background=Material'InterfaceContent.Backgrounds.bg09'
}
