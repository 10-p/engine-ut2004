// ====================================================================
//  Class:  XInterface.UT2MultiplayerHostPage
//  Parent: XInterface.GUIPage
//
//  <Enter a description here>
// ====================================================================

class UT2MultiplayerHostPage extends UT2K3GUIPage;

#exec OBJ LOAD FILE=InterfaceContent.utx

var Tab_MultiplayerHostMain 			pMain;
var Tab_InstantActionBaseRules 			pRules;
var Tab_InstantActionMutators			pMutators;
var Tab_MultiplayerHostServerSettings	pServer;
var Tab_InstantActionBotConfig			pBotConfig;

var GUITabControl						MyTabs;

var localized string	MainTabLabel,
						MainTabHint,
						RulesTabLabel,
						RulesTabHint,
						MutatorsTabLabel,
						MutatorsTabHint,
						ServerTabLabel,
						ServerTabHint,
						BotTabLabel,
						BotTabHint;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local string RulesClass;

	Super.Initcomponent(MyController, MyOwner);

	MyTabs = GUITabControl(Controls[1]);
	GUITitleBar(Controls[0]).DockedTabs = MyTabs;

	pMain 		 = Tab_MultiplayerHostMain( MyTabs.AddTab(MainTabLabel,"xinterface.Tab_MultiplayerHostMain",,MainTabHint,true) );

	pMain.OnChangeGameType = ChangeGameType;

	RulesClass = pMain.GetRulesClass();

	pRules		 = Tab_InstantActionBaseRules(MyTabs.AddTab(RulesTabLabel,	RulesClass,,RulesTabHint));

	pMutators	 = Tab_InstantActionMutators(MyTabs.AddTab(MutatorsTabLabel,"xinterface.Tab_InstantActionMutators",,MutatorsTabHint));
	pServer		 = Tab_MultiplayerHostServerSettings(MyTabs.AddTab(ServerTabLabel,"xinterface.Tab_MultiplayerHostServerSettings",,ServerTabHint));
	pServer.OnChangeCustomBots = ChangeCustomBots;
	pBotConfig	 = Tab_InstantActionBotConfig(MyTabs.AddTab(BotTabLabel,"xinterface.Tab_InstantActionBotConfig",,BotTabHint));

	if (pBotConfig!=None)
		pBotConfig.SetupBotLists(pMain.GetIsTeamGame());

	MyTabs.bDockPanels=true;

	ChangeCustomBots(!pServer.bUseDefaults && pServer.bUseCustomBots);
}

function bool BackButtonClick(GUIComponent Sender)
{
	Controller.CloseMenu(true);
	return true;
}

function bool PlayButtonClick(GUIComponent Sender)
{
	local string FullURL, GameURL, FirstMap, GameType;

	GameURL = pMain.Play();
	GameURL = GameURL$pRules.Play();
	GameURL = GameURL$pMutators.Play();

	if(pServer.UseCustomBots())
		GameURL = GameURL$pBotConfig.Play();

	GameURL = GameURL$pServer.Play();

	if ( (pMain.MyCurMapList.List.ItemCount==0) || (pMain.MyCurMapList.List.Get() == "") )
	{
 		if ( (pMain.MyCurMapList.List.ItemCount==0) || (pMain.MyCurMapList.List.GetItemAtIndex(0) == ""))
			FirstMap = pMain.MyFullMapList.List.Get();
		else
			FirstMap = pMain.MyCurMapList.List.GetItemAtIndex(0);
	}
	else
		FirstMap = pMain.MyCurMapList.List.Get();

	GameType = "?Game="$pMain.GetGameClass();
	FullURL = FirstMap$GameType$GameURL;

    if( pServer.bDedicated )
        PlayerOwner().ConsoleCommand( "RELAUNCH " $ FullURL $ " -server -log=Server.log" );
    else
        PlayerOwner().ClientTravel( FullURL $"?Listen", TRAVEL_Absolute, false );

	Controller.CloseAll(false);

	return true;

}


function TabChange(GUIComponent Sender)
{
	if (GUITabButton(Sender)==none)
		return;

	GUITitleBar(Controls[0]).SetCaption(GUITitleBar(default.Controls[0]).GetCaption()@"|"@GUITabButton(Sender).Caption);
}

event ChangeHint(string NewHint)
{
	GUITitleBar(Controls[2]).SetCaption(NewHint);
}


function InternalOnReOpen()
{
}

function ChangeGameType()
{
	local string RulesClass;

	RulesClass = pMain.GetRulesClass();
	pRules = Tab_InstantActionBaseRules(MyTabs.ReplaceTab(pRules.MyButton,RulesTabLabel,RulesClass,,RulesTabHint));

	if (pBotConfig!=None)
		pBotConfig.SetupBotLists(pMain.GetIsTeamGame());

}

function ChangeCustomBots(bool Enable)
{
	MyTabs.RemoveTab(BotTabLabel);

	if(Enable)
		MyTabs.AddTab(BotTabLabel,"xinterface.Tab_InstantActionBotConfig",pBotConfig,BotTabHint);
}


defaultproperties
{
	Begin Object class=GUITitleBar name=HostHeader
		Caption="Host Multiplayer Game"
		StyleName="Header"
		WinWidth=1
		WinHeight=46.000000
		WinLeft=0
		WinTop=0.036406
		Effect=material'CO_Final'
	End Object
	Controls(0)=GUITitleBar'HostHeader'

	Begin Object Class=GUITabControl Name=HostTabs
		WinWidth=1.0
		WinLeft=0
		WinTop=0.25
		WinHeight=48
		TabHeight=0.04
		OnChange=TabChange;
		bAcceptsInput=true
	End Object
	Controls(1)=GUITabControl'HostTabs'

	Begin Object class=GUITitleBar name=HostFooter
		WinWidth=0.76
		WinHeight=0.055
		WinLeft=0.12
		WinTop=0.93
		bUseTextHeight=false
		StyleName="Footer"
		Justification=TXTA_Center
	End Object
	Controls(2)=GUITitleBar'HostFooter'

	Begin Object Class=GUIButton Name=HostPlayButton
		Caption="START"
		Hint="Start the server"
		OnClick=PlayButtonClick
		StyleName="SquareMenuButton"
		WinWidth=0.12
		WinHeight=0.055
		WinLeft=0.88
		WinTop=0.93
	End Object
	Controls(3)=GUIButton'HostPlayButton'

	Begin Object Class=GUIButton Name=HostBackButton
		Caption="BACK"
		Hint="Cancel Changes and Return to Previous Menu"
		OnClick=BackButtonClick
		StyleName="SquareMenuButton"
		WinWidth=0.12
		WinHeight=0.055
		WinLeft=0
		WinTop=0.93
	End Object
	Controls(4)=GUIButton'HostBackButton'


	OnReOpen=InternalOnReOpen;
	Background=Material'InterfaceContent.Backgrounds.bg11'
	WinWidth=1.0
	WinHeight=1.0
	WinTop=0.0
	WinLeft=0.0

	MainTabLabel="Game & Map"
	RulesTabLabel="Game Rules"
	MutatorsTabLabel="Mutators"
	ServerTabLabel="Server"
	BotTabLabel="Bot Config"

	MainTabHint="Choose the starting map and game type to play..."
	RulesTabHint="Configure the current game type..."
	MutatorsTabHint="Select and configure any mutators to use..."
	ServerTabHint="Configure all server specific settings..."
	BotTabHint="Configure any bots that will be in the session..."
}
