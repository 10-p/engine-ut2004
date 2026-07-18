// ====================================================================
// (C) 2002, Epic Games
// ====================================================================

class UT2ServerInfo extends GUIPage;

var(MidGame) config array<string>        PanelClass;
var(MidGame) localized array<string>     PanelCaption;
var(MidGame) localized array<string>     PanelHint;

var(MidGame) editconst noexport GUITabControl TabC;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local int i;
    Super.InitComponent(MyController, MyOwner);

	if ( PlayerOwner() != None && PlayerOwner().GameReplicationInfo != None )
	    SetTitle();

    TabC = GUITabControl(Controls[4]);
    TabC.MyFooter = GUIFooter(Controls[2]);

	for ( i = 0; i < PanelClass.Length && i < PanelCaption.Length && i < PanelHint.Length; i++ )
	    TabC.AddTab(PanelCaption[i],PanelClass[i],,PanelHint[i]);

    if (!bOldStyleMenus)
        Controls[3].Style = Controller.GetStyle("SquareButton",Controls[3].FontScale);
}

function bool ButtonClicked(GUIComponent Sender)
{
    Controller.CloseMenu(true);
    return true;
}

event ChangeHint(string NewHint)
{
    GUITitleBar(Controls[2]).SetCaption(NewHint);
}

function SetTitle()
{
	GUITitleBar(Controls[1]).SetCaption(PlayerOwner().GameReplicationInfo.ServerName);
}

function bool NotifyLevelChange()
{
	bPersistent = False;
	return true;
}

defaultproperties
{
    Begin Object Class=GUIImage name=ServerInfoBackground
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
    Controls(0)=GUIImage'ServerInfoBackground'

    Begin Object class=GUITitleBar name=ServerInfoHeader
        Caption=""
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
    Controls(1)=GUITitleBar'ServerInfoHeader'

    Begin Object class=GUIFooter name=ServerInfoFooter
        WinWidth=1
        WinHeight=0.075
        WinLeft=0
        WinTop=0.925
        StyleName="Footer"
//        Justification=TXTA_Center
        bBoundToParent=true
        bScaleToParent=true
    End Object
    Controls(2)=GUIFooter'ServerInfoFooter'

    Begin Object Class=GUIButton Name=ServerBackButton
        Caption="Close"
        Hint="Close this menu"
        WinWidth=0.120000
        WinHeight=0.055000
        WinLeft=0.848750
        WinTop=0.934167
        bBoundToParent=true
        bScaleToParent=true
        OnClick=ButtonClicked
    End Object
    Controls(3)=GUIButton'ServerBackButton'

    Begin Object Class=GUITabControl Name=ServerInfoTabs
		WinWidth=0.974999
		WinHeight=0.060000
		WinLeft=0.012500
		WinTop=0.083333
        TabHeight=0.045
        bAcceptsInput=true
        bDockPanels=true
        bBoundToParent=true
        bScaleToParent=true
        bFillSpace=True
    End Object
    Controls(4)=GUITabControl'ServerInfoTabs'


    bRequire640x480=false
    bAllowedAsLast=true
    WinLeft=0.2
    WinWidth=0.6
    WinTop=0.1
    WinHeight=0.8

	PanelClass(0)="XInterface.Tab_ServerMOTD"
	PanelClass(1)="XInterface.Tab_ServerInfo"
	PanelClass(2)="XInterface.Tab_ServerMapList"

	PanelCaption(0)="MOTD"
	PanelCaption(1)="Rules"
	PanelCaption(2)="Maps"

    PanelHint(0)="Message of the Day"
    PanelHint(1)="Game Rules"
    PanelHint(2)="Map Rotation"
}