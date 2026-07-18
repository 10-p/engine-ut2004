// ====================================================================
// (C) 2002, Epic Games
// ====================================================================

class UT2AdminMenu extends UT2K3GUIPage;

var GUITabControl TabC;
var Tab_AdminPlayerList PlayerList;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
 	Super.InitComponent(MyController, MyOwner);

    TabC = GUITabControl(Controls[5]);
	PlayerList = Tab_AdminPlayerList(TabC.AddTab("Players","xinterface.Tab_AdminPlayerList",,"Player Mgt.",true));

	if (PlayerList!=None)
    	PlayerList.ReloadList();

}

function HandleParameters(string Param1, string Param2)
{
	PlayerList.bAdvancedAdmin = bool(Param1);
}

function bool ButtonClicked(GUIComponent Sender)
{
	Controller.CloseMenu(true);
    return true;
}
/*
event ChangeHint(string NewHint)
{
	GUITitleBar(Controls[3]).Caption = NewHint;
}
*/

defaultproperties
{
	Begin Object Class=GUIImage name=AdminInfoBackground
		bAcceptsInput=false
		bNeverFocus=true
        Image=Material'InterfaceContent.Menu.BorderBoxD'
        ImageStyle=ISTY_Stretched
        WinWidth=1
        WinLeft=0
        WinHeight=1
        WinTop=0
        bBoundToParent=true
        bScaleToParent=true
	End Object
	Controls(0)=GUIImage'AdminInfoBackground'

	Begin Object Class=GUIImage name=AdminInfoBackground2
		bAcceptsInput=false
		bNeverFocus=true
        Image=Material'InterfaceContent.Menu.BorderBoxD'
        ImageStyle=ISTY_Stretched
        WinWidth=1
        WinLeft=0
        WinHeight=1
        WinTop=0
        bBoundToParent=true
        bScaleToParent=true
	End Object
	Controls(1)=GUIImage'AdminInfoBackground2'


	Begin Object class=GUITitleBar name=AdminInfoHeader
		Caption="Admin In-Game Menu"
		StyleName="Header"
        Justification=TXTA_Center
		WinWidth=1
		WinHeight=0.1
		WinLeft=0
		WinTop=0
        bBoundToParent=true
        bScaleToParent=true
		Effect=material'CO_Final'
	End Object
	Controls(2)=GUITitleBar'AdminInfoHeader'

	Begin Object class=GUITitleBar name=AdminInfoFooter
		WinWidth=1
		WinHeight=0.075
		WinLeft=0
		WinTop=0.925
		bUseTextHeight=false
		StyleName="Footer"
		Justification=TXTA_Center
        bBoundToParent=true
        bScaleToParent=true
	End Object
	Controls(3)=GUITitleBar'AdminInfoFooter'

	Begin Object Class=GUIButton Name=AdminBackButton
		Caption="Close"
		StyleName="SquareMenuButton"
		Hint="Close this menu"
		WinWidth=0.120000
		WinHeight=0.055000
		WinLeft=0.868750
		WinTop=0.934167
        bBoundToParent=true
        bScaleToParent=true
		OnClick=ButtonClicked
	End Object
	Controls(4)=GUIButton'AdminBackButton'

	Begin Object Class=GUITabControl Name=AdminInfoTabs
		WinWidth=1.0
		WinLeft=0
		WinTop=0.1
		WinHeight=0.06
		TabHeight=0.04
		bAcceptsInput=true
		bDockPanels=true
        bBoundToParent=true
        bScaleToParent=true
	End Object
	Controls(5)=GUITabControl'AdminInfoTabs'

	bRequire640x480=false
	bAllowedAsLast=true
    WinLeft=0.02
    WinWidth=0.96
    WinTop=0.1
    WinHeight=0.8

}
