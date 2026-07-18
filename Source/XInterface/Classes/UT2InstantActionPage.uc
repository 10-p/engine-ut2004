// ====================================================================
//  Class:  XInterface.UT2InstantActionPage
//  Parent: XInterface.GUIPage
//
//  <Enter a description here>
// ====================================================================

class UT2InstantActionPage extends UT2K3GUIPage;

#exec OBJ LOAD FILE=InterfaceContent.utx

var Tab_InstantActionMain 		pMain;
var Tab_InstantActionBaseRules 	pRules;
var Tab_InstantActionMutators	pMutators;
var Tab_InstantActionMapList	pMapList;
var Tab_InstantActionBotConfig	pBotConfig;
var Tab_PlayerSettings			pPlayerSetup;

var GUITabControl				MyTabs;

var localized string			MainTabLabel,
								MainTabHint,
								RulesTabLabel,
								RulesTabHint,
								MutatorTabLabel,
								MutatorTabHint,
								MapListTabLabel,
								MapListTabHint,
								BotConfigTabLabel,
								BotConfigTabHint,
								PlayerTabLabel,
								PlayerTabHint;

var bool bSpectate;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local string RulesClass;
	Super.Initcomponent(MyController, MyOwner);

	MyTabs = GUITabControl(Controls[1]);
	GUITitleBar(Controls[0]).DockedTabs = MyTabs;

	pMain 		 = Tab_InstantActionMain(MyTabs.AddTab(MainTabLabel,"xinterface.Tab_InstantActionMain",,MainTabHint,true) );
	pMain.OnChangeGameType = ChangeGameType;
	pMain.OnChangeCustomBots = ChangeCustomBots;
	RulesClass = pMain.GetRulesClass();

	pRules		 = Tab_InstantActionBaseRules(MyTabs.AddTab(RulesTabLabel,RulesClass,,RulesTabHint));
	pMutators	 = Tab_InstantActionMutators(MyTabs.AddTab(MutatorTabLabel,"xinterface.Tab_InstantActionMutators",,MutatorTabHint));
	pMapList	 = Tab_InstantActionMapList(MyTabs.AddTab(MapListTabLabel,"xinterface.Tab_InstantActionMapList",,MapListTabHint));
	pBotConfig	 = Tab_InstantActionBotConfig(MyTabs.AddTab(BotConfigTabLabel,"xinterface.Tab_InstantActionBotConfig",,BotConfigTabHint));
	pPlayerSetup = Tab_PlayerSettings(MyTabs.AddTab(PlayerTabLabel,"xinterface.Tab_PlayerSettings",,PlayerTabHint));

	if (pBotConfig!=None)
		pBotConfig.SetupBotLists(pMain.GetIsTeamGame());

	pMapList.ReadMapList(pMain.GetMapPrefix(), pMain.GetMapListClass());
	MyTabs.bDockPanels=true;

	ChangeCustomBots();
    bSpectate = false;
}

function bool BackButtonClick(GUIComponent Sender)
{
	pPlayerSetup.InternalApply(none);
	Controller.CloseMenu(true);
	return true;
}

function bool PlayButtonClick(GUIComponent Sender)
{
	local string FullURL, GameURL, FirstMap, GameType;

	pPlayerSetup.InternalApply(none);

	GameURL = pMain.Play();
	GameURL = GameURL$pRules.Play();
	GameURL = GameURL$pMutators.Play();
	GameURL = GameURL$pMapList.Play();

	if(pMain.LastUseCustomBots)
		GameURL = GameURL$pBotConfig.Play();

	FirstMap = pMain.MyMapList.List.Get();
	GameType = "?Game="$pMain.GetGameClass();

    if (bSpectate)
    	GameType = GameType$"?spectatoronly=1";

	FullURL = FirstMap$GameType$GameURL;

	log("UT2InstantActionPage::PlayButtonClick - Sending [open"@FUllURL$"] to the console");
	Console(Controller.Master.Console).DelayedConsoleCommand("start"@FullURL);
	Controller.CloseAll(false);
	return true;
}

function bool SpecButtonClick(GUIComponent Sender)
{
	bSpectate = true;
    PlayButtonClick(sender);
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

	if (pMapList!=None)
		pMapList.ReadMapList(pMain.GetMapPrefix(),pMain.GetMapListClass());

	RulesClass = pMain.GetRulesClass();
	pRules = Tab_InstantActionBaseRules(MyTabs.ReplaceTab(pRules.MyButton,RulesTabLabel,RulesClass,,RulesTabHint));

	if (pBotConfig!=None)
		pBotConfig.SetupBotLists(pMain.GetIsTeamGame());
}

function ChangeCustomBots()
{
	MyTabs.RemoveTab(BotConfigTabLabel);

	if(pMain.LastUseCustomBots)
		MyTabs.AddTab(BotConfigTabLabel,"xinterface.Tab_InstantActionBotConfig",pBotConfig,BotConfigTabHint);
}

function InternalOnClose(optional Bool bCanceled)
{
	// Destroy spinning player model actor
	if(pPlayerSetup.SpinnyDude != None)
		pPlayerSetup.SpinnyDude.Destroy();

	Super.OnClose(bCanceled);
}

defaultproperties
{
	Begin Object class=GUITitleBar name=IAPageHeader
		Caption="Instant Action"
		StyleName="Header"
		WinWidth=1
		WinHeight=46.000000
		WinLeft=0
		WinTop=0.036406
		Effect=material'CO_Final'
	End Object
	Controls(0)=GUITitleBar'IAPageHeader'

	Begin Object Class=GUITabControl Name=IAPageTabs
		WinWidth=1.0
		WinLeft=0
		WinTop=0.25
		WinHeight=48
		TabHeight=0.04
		OnChange=TabChange;
		bAcceptsInput=true
	End Object
	Controls(1)=GUITabControl'IAPageTabs'

	Begin Object class=GUITitleBar name=IAPageFooter
		WinWidth=0.635000
//		WinWidth=0.76
		WinHeight=0.055
		WinLeft=0.12
		WinTop=0.93
		bUseTextHeight=false
		StyleName="Footer"
		Justification=TXTA_Center
	End Object
	Controls(2)=GUITitleBar'IAPageFooter'

	Begin Object Class=GUIButton Name=IAPagePlayButton
		Caption="PLAY"
		Hint="Start a Match With These Settings"
		OnClick=PlayButtonClick
		StyleName="SquareMenuButton"
		WinWidth=0.12
		WinHeight=0.055
		WinLeft=0.88
		WinTop=0.93
	End Object
	Controls(3)=GUIButton'IAPagePlayButton'

	Begin Object Class=GUIButton Name=IAPageBackButton
		Caption="BACK"
		Hint="Return to Previous Menu"
		OnClick=BackButtonClick
		StyleName="SquareMenuButton"
		WinWidth=0.12
		WinHeight=0.055
		WinLeft=0
		WinTop=0.93
	End Object
	Controls(4)=GUIButton'IAPageBackButton'

	Begin Object Class=GUIButton Name=IAPageSpecButton
		Caption="SPECTATE"
		Hint="Spectate a Match With These Settings"
		OnClick=SpecButtonClick
		StyleName="SquareMenuButton"
		WinWidth=0.120000
		WinHeight=0.055000
		WinLeft=0.758125
		WinTop=0.930000
	End Object
	Controls(5)=GUIButton'IAPageSpecButton'


	OnReOpen=InternalOnReOpen;
	OnClose=InternalOnClose

	Background=Material'InterfaceContent.Backgrounds.bg09'
	WinWidth=1.0
	WinHeight=1.0
	WinTop=0.0
	WinLeft=0.0

	MainTabLabel="Select Map"
	RulesTabLabel="Game Rules"
	MutatorTabLabel="Mutators"
	MapListTabLabel="Map List"
	BotConfigTabLabel="Bot Config"
	PlayerTabLabel="Player"

	MainTabHint="Choose the starting map and game type to play..."
	RulesTabHint="Configure the current game type..."
	MutatorTabHint="Select and configure any mutators to use..."
	MapListTabHint="Configure the list of maps to play..."
	BotConfigTabHint="Configure any bots that will be in the session..."
	PlayerTabHint="Configure your UT2003 Avatar..."
}
