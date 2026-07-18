// ====================================================================
// (C) 2002, Epic Games
// ====================================================================

class Tab_AdminPlayerList extends UT2K3TabPanel;


var  AdminPlayerList MyPlayerList;
var  GUIMultiColumnListbox MyListBox;
var  bool bAdvancedAdmin;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	Super.InitComponent(MyController,MyOwner);

	MyListBox = GUIMultiColumnListbox(Controls[1]);
    MyPlayerList = AdminPlayerList(MyListBox.Controls[0]);
	MyPlayerList.Initcomponent(MyController,self);

    WinWidth = Controller.ActivePage.WinWidth;
    WinLeft = Controller.ActivePage.WinLeft;

}

function ProcessRule(string NewRule)
{
	if (NewRule=="Done")
    	XPlayer(PlayerOwner()).ProcessRule = None;
	else
      MyPlayerList.Add(NewRule);
}

function ReloadList()
{
	MyPlayerList.Clear();
	if (XPlayer(PlayerOwner())!=None)
    {
	    XPlayer(PlayerOwner()).ProcessRule = ProcessRule;
       	XPlayer(PlayerOwner()).ServerRequestPlayerInfo();
    }
}


function bool KickClicked(GUIComponent Sender)
{
	PlayerOwner().ConsoleCommand("Admin Kick"@MyPlayerList.MyPlayers[MyPlayerList.Index].PlayerName);
	ReloadList();
    return true;
}

function bool BanClicked(GUIComponent Sender)
{
	if (bAdvancedAdmin)
		PlayerOwner().ConsoleCommand("Admin Kick Ban"@MyPlayerList.MyPlayers[MyPlayerList.Index].PlayerName);
	else PlayerOwner().ConsoleCommand("Admin KickBan"@MyPlayerList.MyPlayers[MyPlayerList.Index].PlayerName);
    ReloadList();
    return true;
}


defaultproperties
{
	Begin Object Class=GUIImage name=AdminBackground
		bAcceptsInput=false
		bNeverFocus=true
        Image=Material'InterfaceContent.Menu.SquareBoxA'
        ImageStyle=ISTY_Stretched
        WinWidth=1
        WinLeft=0
        WinHeight=1
        WinTop=0
	End Object
	Controls(0)=GUIImage'AdminBackground'

	Begin Object Class=AdminPlayerList Name=AdminList
    End Object

	Begin Object Class=GUIMultiColumnListBox Name=AdminPlayersListBox
		WinWidth=1.000000
		WinHeight=0.878127
		WinLeft=0.000000
		WinTop=0.000000
		bVisibleWhenEmpty=True
		StyleName="ServerBrowserGrid"
        Controls(0)=AdminList
	End Object
	Controls(1)=GUIMultiColumnListBox'AdminPlayersListBox'

	Begin Object Class=GUIButton Name=AdminPlayerKick
		Caption="Kick"
		StyleName="SquareMenuButton"
		Hint="Kick this Player"
		WinWidth=0.120000
		WinHeight=0.070625
		WinLeft=0.743750
		WinTop=0.900000
		OnClick=KickClicked
	End Object
	Controls(2)=GUIButton'AdminPlayerKick'

	Begin Object Class=GUIButton Name=AdminPlayerBan
		Caption="Ban"
		StyleName="SquareMenuButton"
		Hint="Ban this player"
		WinWidth=0.120000
		WinHeight=0.070625
		WinLeft=0.868750
		WinTop=0.900000
		OnClick=BanClicked
	End Object
	Controls(3)=GUIButton'AdminPlayerBan'

    WinLeft=0
    WinWidth=1
    WinTop=0
	WinHeight=0.625003


}
