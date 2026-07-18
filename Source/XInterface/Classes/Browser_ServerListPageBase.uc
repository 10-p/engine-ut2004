// ====================================================================
//  Class:  XInterface.ServerListPage
//  Parent: XInterface.GUITabPanel
//
//  <Enter a description here>
// ====================================================================

class Browser_ServerListPageBase extends Browser_Page;

// Internal
var Browser_ServersList  MyServersList;
var bool ConnectLAN;
var GUITitleBar StatusBar;
var bool bInitialized;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	if( !bInitialized )
	{
		MyServersList = Browser_ServersList(GUIMultiColumnListBox(GUIPanel(GUISplitter(Controls[0]).Controls[0]).Controls[0]).Controls[0]);
		MyServersList.MyPage		= Self;
		MyServersList.MyRulesList   = Browser_RulesList  (GUIMultiColumnListBox(GUIPanel(GUISplitter(GUISplitter(Controls[0]).Controls[1]).Controls[0]).Controls[0]).Controls[0]);
		MyServersList.MyPlayersList = Browser_PlayersList(GUIMultiColumnListBox(GUIPanel(GUISplitter(GUISplitter(Controls[0]).Controls[1]).Controls[1]).Controls[0]).Controls[0]);
		MyServersList.MyRulesList.MyPage = Self;
		MyServersList.MyRulesList.MyServersList = MyServersList;
		MyServersList.MyPlayersList.MyPage = Self;
		MyServersList.MyPlayersList.MyServersList = MyServersList;

		StatusBar = GUITitleBar(GUIPanel(Controls[1]).Controls[5]);
	}
	StatusBar.SetCaption(ReadyString);

	Super.Initcomponent(MyController, MyOwner);

	if( !bInitialized )
	{
		GUIButton(GUIPanel(Controls[1]).Controls[0]).OnClick=BackClick;
		GUIButton(GUIPanel(Controls[1]).Controls[1]).OnClick=RefreshClick;
		GUIButton(GUIPanel(Controls[1]).Controls[2]).OnClick=JoinClick;
		GUIButton(GUIPanel(Controls[1]).Controls[3]).OnClick=SpectateClick;
		GUIButton(GUIPanel(Controls[1]).Controls[4]).OnClick=AddFavoriteClick;
	}

	bInitialized = True;
}
// functions

function RefreshList()
{
}

function PingServer( int i, ServerQueryClient.EPingCause PingCause, GameInfo.ServerResponseLine s )
{
}

function CancelPings()
{
}

// delegates
function bool BackClick(GUIComponent Sender)
{
	Controller.CloseMenu(true);
	return true;
}

function bool RefreshClick(GUIComponent Sender)
{
	RefreshList();
	return true;
}

function bool JoinClick(GUIComponent Sender)
{
	MyServersList.Connect(false);
	return true;
}

function bool SpectateClick(GUIComponent Sender)
{
	MyServersList.Connect(true);
	return true;
}

function bool AddFavoriteClick(GUIComponent Sender)
{
	MyServersList.AddFavorite(Browser);
	return true;
}

defaultproperties
{
	// Server list box
	Begin Object Class=Browser_ServersList Name=TheServersList
	End Object
	Begin Object Class=GUIMultiColumnListBox Name=ServersListBox
		WinLeft=0
		WinTop=0
		WinWidth=1
		WinHeight=1
		bVisibleWhenEmpty=True
		Controls(0)=TheServersList
		StyleName="ServerBrowserGrid"
	End Object

	// Players list box
	Begin Object Class=Browser_PlayersList Name=ThePlayersList
	End Object
	Begin Object Class=GUIMultiColumnListBox Name=PlayersListBox
		WinLeft=0
		WinTop=0
		WinWidth=1
		WinHeight=1
		bVisibleWhenEmpty=True
		Controls(0)=ThePlayersList
		StyleName="ServerBrowserGrid"
	End Object

	// Rules list box
	Begin Object Class=Browser_RulesList Name=TheRulesList
	End Object
	Begin Object Class=GUIMultiColumnListBox Name=RulesListBox
		WinLeft=0
		WinTop=0
		WinWidth=1
		WinHeight=1
		bVisibleWhenEmpty=True
		Controls(0)=TheRulesList
		StyleName="ServerBrowserGrid"
	End Object

	// Servers  panel
	Begin Object Class=GUIPanel Name=ServersPanel
		Controls(0)=ServersListBox
	End Object

	// Rules panel
	Begin Object Class=GUIPanel Name=RulesPanel
		Controls(0)=RulesListBox
	End Object

	// Players panel
	Begin Object Class=GUIPanel Name=PlayersPanel
		Controls(0)=PlayersListBox;
	End Object

	// Splitter to divide players and rules horizontally
	Begin Object Class=GUISplitter Name=DetailsSplitter
		WinWidth=1.0
		WinHeight=1.0
		WinTop=0
		WinLeft=0
		Controls(0)=RulesPanel
		Controls(1)=PlayersPanel
		SplitOrientation=SPLIT_Horizontal
		SplitPosition=0.5
		Background=DefaultTexture
	End Object

	// Splitter to divide main window in two: server list and details
	Begin Object Class=GUISplitter Name=MainSplitter
		WinWidth=1.0
		WinHeight=0.9
		WinTop=0
		WinLeft=0
		Controls(0)=ServersPanel
		Controls(1)=DetailsSplitter
		SplitOrientation=SPLIT_Vertical
		SplitPosition=0.5
	End Object
	Controls(0)=MainSplitter

	Begin Object Class=GUIButton Name=BackButton
		Caption="BACK"
		StyleName="SquareMenuButton"
		WinWidth=0.2
		WinLeft=0
		WinTop=0
		WinHeight=0.5
	End Object

	Begin Object Class=GUIButton Name=RefreshButton
		Caption="REFRESH LIST"
		StyleName="SquareMenuButton"
		WinWidth=0.2
		WinLeft=0.2
		WinTop=0
		WinHeight=0.5
	End Object

	Begin Object Class=GUIButton Name=JoinButton
		Caption="JOIN"
		StyleName="SquareMenuButton"
		WinWidth=0.2
		WinLeft=0.4
		WinTop=0
		WinHeight=0.5
	End Object

	Begin Object Class=GUIButton Name=SpectateButton
		Caption="SPECTATE"
		StyleName="SquareMenuButton"
		WinWidth=0.2
		WinLeft=0.6
		WinTop=0
		WinHeight=0.5
	End Object

	Begin Object Class=GUIButton Name=AddFavoriteButton
		Caption="ADD FAVORITE"
		StyleName="SquareMenuButton"
		WinWidth=0.2
		WinLeft=0.8
		WinTop=0
		WinHeight=0.5
	End Object

	Begin Object Class=GUIButton Name=UtilButtonA
		Caption=""
		StyleName="SquareMenuButton"
		WinWidth=0.2
		WinLeft=0.6
		WinTop=0.5
		WinHeight=0.5
	End Object

	Begin Object Class=GUIButton Name=UtilButtonB
		Caption=""
		StyleName="SquareMenuButton"
		WinWidth=0.2
		WinLeft=0.8
		WinTop=0.5
		WinHeight=0.5
	End Object

	Begin Object class=GUITitleBar name=MyStatus
		WinWidth=0.6
		WinHeight=0.5
		WinLeft=0
		WinTop=0.5
		StyleName="SquareBar"
		Caption=""
		bUseTextHeight=false
		Justification=TXTA_Left
	End Object

	Begin Object Class=GUIPanel Name=FooterPanel
		Controls(0)=BackButton
		Controls(1)=RefreshButton
		Controls(2)=JoinButton
		Controls(3)=SpectateButton
		Controls(4)=AddFavoriteButton
		Controls(5)=MyStatus
		Controls(6)=UtilButtonA
		Controls(7)=UtilButtonB
		WinWidth=1
		WinHeight=0.1
		WinLeft=0
		WinTop=0.9
	End Object
	Controls(1)=FooterPanel

	ConnectLAN=False
}
