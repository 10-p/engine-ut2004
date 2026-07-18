// ====================================================================
//  Class:  XInterface.Tab_MultiplayerHostServerSettings
//  Parent: XInterface.GUITabPanel
//
//  <Enter a description here>
// ====================================================================

class Tab_MultiplayerHostServerSettings extends UT2K3TabPanel;

var Config 	bool 	bDedicated;
var	Config	bool	bLanPlay;
var Config  int		BotSkill;
var Config  bool    bPlayersMustBeReady;
var Config	bool	bUseDefaults;
var Config	bool	bUseCustomBots;
var	Config	int		MinPlayers;
var Config 	int		MaxPlayers;
var Config	int		MaxSpecs;
var Config 	bool	bAllowBehindView;
var Config	string	AdminName;
var Config	string	AdminPass;
var Config	string	GamePass;
var Config  bool 	bBalanceTeams;
var Config  bool	bCollectStats;

var moCheckBox 		MyDedicated;
var moCheckBox 		MyLanGame;
var moCheckBox		MyAdvertise;
var moCheckBox 		MyCollectStats;
var moCheckBox 		MyBalanceTeams;
var moCheckBox		MyPlayersMustBeReady;
var moCheckBox		MyAllowBehindView;
var moComboBox		MyBotSkill;
var moCheckBox		MyUseDefaultBots;
var moCheckBox		MyUseCustomBots;
var moNumericEdit	MyMinPlayers;
var moNumericEdit	MyMaxPlayers;
var moNumericEdit	MyMaxSpecs;

var moEditBox		MyServerName;
var moEditBox		MyServerPasswrd;
var	moEditBox		MyAdminName;
var	moEditBox		MyAdminEmail;
var	moEditBox		MyAdminPasswrd;
var	moEditBox		MyMOTD1;
var	moEditBox		MyMOTD2;
var	moEditBox		MyMOTD3;
var	moEditBox		MyMOTD4;
var moCheckBox 		MyUseWebAdmin;
var moNumericEdit	MyWebPort;

var bool bInitialized;

delegate OnChangeCustomBots(bool Enable);

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local int	index;
	Super.InitComponent(MyController, MyOwner);

	MyDedicated 	= moCheckBox(Controls[2]);		MyDedicated.Checked(bDedicated);
	MyLanGame		= moCheckBox(Controls[3]);		MyLanGame.Checked(bLanPlay);
	MyAdvertise		= moCheckBox(Controls[4]);		MyAdvertise.Checked(class'MasterServerUplink'.default.DoUplink);
	MyCollectStats	= moCheckBox(Controls[5]);		MyCollectStats.Checked(bCollectStats);
	MyBalanceTeams	= moCheckBox(Controls[6]);		MyBalanceTeams.Checked(bBalanceTeams);

    MyPlayersMustBeReady = moCheckBox(Controls[7]); MyPlayersMustBeReady.Checked(bPlayersMustBeReady);

	MyBotSkill = moComboBox(Controls[8]);
	for(index = 0;index < 8;index++)
		MyBotSkill.AddItem(class'Tab_InstantActionMain'.default.DifficultyLevels[index]);
	MyBotSkill.ReadOnly(True);
	MyBotSkill.SetIndex(BotSkill);

	MyUseDefaultBots= moCheckbox(Controls[9]);		MyUseDefaultBots.Checked(bUseDefaults);
	MyUseCustomBots = moCheckbox(Controls[10]);		MyUseCustomBots.Checked(bUseCustomBots);

	MyMinPlayers	= moNumericEdit(Controls[11]);	MyMinPlayers.SetValue(MinPlayers);
	MyMaxPlayers	= moNumericEdit(Controls[12]);	MyMaxPlayers.SetValue(MaxPlayers);
	MyMaxSpecs		= moNumericEdit(Controls[13]);	MyMaxSpecs.SetValue(MaxSpecs);


	MyServerName	= moEditBox(Controls[14]);		MyServerName.SetText(class'GameReplicationInfo'.default.ServerName);
	MyServerPasswrd = moEditBox(Controls[15]);		MyServerPasswrd.SetText(GamePass);
	MyAdminName		= moEditBox(Controls[16]);		MyAdminName.SetText(AdminName);
	MyAdminEmail	= moEditBox(Controls[17]);		MyAdminEmail.SetText(class'GameReplicationInfo'.default.AdminEmail);
	MyAdminPasswrd	= moEditBox(Controls[18]);		MyAdminPasswrd.SetText(AdminPass);
//	MyMOTD1			= moEditBox(Controls[19]);		MyMOTD1.SetText(class'GameReplicationInfo'.default.MOTDLine1);
//	MyMOTD2			= moEditBox(Controls[20]);		MyMOTD2.SetText(class'GameReplicationInfo'.default.MOTDLine2);
//	MyMOTD3			= moEditBox(Controls[21]);		MyMOTD3.SetText(class'GameReplicationInfo'.default.MOTDLine3);
//	MyMOTD4			= moEditBox(Controls[22]);		MyMOTD4.SetText(class'GameReplicationInfo'.default.MOTDLine4);
	MyUseWebAdmin	= moCheckBox(Controls[23]);		MyUseWebAdmin.Checked(class'WebServer'.default.benabled);
	MyWebPort		= moNumericEdit(Controls[24]);	MyWebPort.SetValue(class'WebServer'.default.ListenPort);

    MyAllowBehindview = moCheckBox(Controls[25]); MyAllowBehindView.Checked(bAllowBehindView);

	bInitialized = true;

    MyAdminName.MyEditBox.bConvertSpaces = true;
    MyAdminPasswrd.MyEditBox.bConvertSpaces = true;
    MyServerPasswrd.MyEditBox.bConvertSpaces = true;

}

function string Play()
{
	local string url;
	local string gc;
	local Tab_MultiplayerHostMain pMain;

	bDedicated 		= MyDedicated.IsChecked();
	bCollectStats	= MyCollectStats.IsChecked();
	bBalanceTeams	= MyBalanceTeams.IsChecked();

	bPlayersMustBeReady = MyPlayersMustBeReady.IsChecked();

	bLanPlay   		= MyLanGame.IsChecked();
	BotSkill		= MyBotSkill.GetIndex();
	MinPlayers 		= MyMinPlayers.GetValue();
	MaxPlayers 		= MyMaxPlayers.GetValue();
	MaxSpecs  	 	= MyMaxSpecs.GetValue();
	bUseDefaults	= MyUseDefaultBots.IsChecked();
	bUseCustomBots	= MyUseCustomBots.IsChecked();
	GamePass		= MyServerPasswrd.GetText();
	AdminName		= MyAdminName.GetText();
	AdminPass		= MyAdminPasswrd.GetText();

	bAllowBehindView = MyAllowBehindview.IsChecked();

	SaveConfig();

	class'MasterServerUplink'.default.DoUplink = MyAdvertise.IsChecked();
	class'MasterServerUplink'.Static.StaticSaveConfig();

	class'GameReplicationInfo'.default.ServerName 	= MyServerName.GetText();
	class'GameReplicationInfo'.default.AdminName  	= AdminName;

	class'GameReplicationInfo'.default.AdminEmail 	= MyAdminEmail.GetText();
//	class'GameReplicationInfo'.default.MOTDLine1	= MyMOTD1.GetText();
//	class'GameReplicationInfo'.default.MOTDLine2	= MyMOTD2.GetText();
//	class'GameReplicationInfo'.default.MOTDLine3	= MyMOTD3.GetText();
//	class'GameReplicationInfo'.default.MOTDLine4	= MyMOTD4.GetText();
//	class'GameReplicationInfo'.static.StaticSaveConfig();

	class'WebServer'.Default.bEnabled = MyUseWebAdmin.IsChecked();
	class'WebServer'.Default.ListenPort = MyWebPort.GetValue();
	class'WebServer'.static.StaticSaveConfig();

	pMain = UT2MultiplayerHostPage(Controller.ActivePage).pMain;
	gc = pMain.GetGameClass();

	url = url$"?GameStats="$bCollectStats;
	if ( pMain.GetIsTeamGame() )
		url = url$"?BalanceTeams="$bBalanceTeams;

	if (bLanPlay)
		Url=URL$"?LAN";

	if (!bUseDefaults)
	{
		if(!bUseCustomBots)
			Url=URL$"?MinPlayers="$MinPlayers;
		Url=Url$"?MaxPlayers="$MaxPlayers$"?MaxSpectators="$MaxSpecs;
	}
	else
		Url=Url$"?bAutoNumBots=True";

	if ( (AdminName!="") && (AdminPass!="") )
		URL=URL$"?AdminName="$AdminName$"?AdminPassword="$AdminPass;

	if (GamePass!="")
		URL=URL$"?GamePassword="$GamePass;

    if (bPlayersMustBeReady)
    	URL=URL$"?PlayersMustBeReady="$bPlayersMustBeReady;

	if (bAllowBehindView)
    	URL=URL$"?AllowBehindview="$bAllowBehindView;

	URL = URL$"?difficulty="$BotSkill;

	return url;

}


function UseMapOnChange(GUIComponent Sender)
{
	if (!bInitialized)
    	return;

	if(!MyUseDefaultBots.IsChecked())
	{
		MyUseCustomBots.MenuStateChange(MSAT_Blurry);
		MyMaxPlayers.MenuStateChange(MSAT_Blurry);
		MyMaxSpecs.MenuStateChange(MSAT_Blurry);

		if(MyUseCustomBots.IsChecked())
			MyMinPlayers.MenuStateChange(MSAT_Disabled);
		else
			MyMinPlayers.MenuStateChange(MSAT_Blurry);

		OnChangeCustomBots(MyUseCustomBots.IsChecked());
	}
	else
	{
		MyUseCustomBots.MenuStateChange(MSAT_Disabled);
		MyMinPlayers.MenuStateChange(MSAT_Disabled);
		MyMaxPlayers.MenuStateChange(MSAT_Disabled);
		MyMaxSpecs.MenuStateChange(MSAT_Disabled);

		OnChangeCustomBots(False);
	}
}

function UseCustomOnChange(GUIComponent Sender)
{
	if (!bInitialized)
    	return;


	if(MyUseCustomBots.IsChecked())
		MyMinPlayers.MenuStateChange(MSAT_Disabled);
	else
		MyMinPlayers.MenuStateChange(MSAT_Blurry);

	OnChangeCustomBots(MyUseCustomBots.IsChecked());
}

function bool UseCustomBots()
{
	return !MyUseDefaultBots.IsChecked() && MyUseCustomBots.IsChecked();
}


defaultproperties
{

	Begin Object class=GUIImage Name=MPServerBk1
		WinWidth=0.426248
		WinHeight=0.978440
		WinLeft=0.016758
		WinTop=0.024687
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	Controls(0)=GUIImage'MPServerBk1'

	Begin Object class=GUIImage Name=MPServerBk2
		WinWidth=0.427032
		WinHeight=0.307188
		WinLeft=0.016172
		WinTop=0.696041
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	Controls(1)=GUIImage'MPServerBk2'

	Begin Object class=moCheckBox Name=MPServerDedicated
		WinWidth=0.385938
		WinHeight=0.040000
		WinLeft=0.035742
		WinTop=0.060001
		Caption="Dedicated Server"
		Hint="When this option is enabled, you will run a dedicated server."
		bSquare=true
		ComponentJustification=TXTA_Left
		CaptionWidth=0.925
	End Object
	Controls(2)=moCheckBox'MPServerDedicated'

	Begin Object class=moCheckBox Name=MPServerLanGame
		WinWidth=0.385938
		WinHeight=0.040000
		WinLeft=0.035742
		WinTop=0.118334
		Caption="Lan Game"
		Hint="Optimizes network usage for players on a local area network."
		bSquare=true
		ComponentJustification=TXTA_Left
		CaptionWidth=0.925
	End Object
	Controls(3)=moCheckBox'MPServerLanGame'

	Begin Object class=moCheckBox Name=MPServerAdvertise
		WinWidth=0.385938
		WinHeight=0.040000
		WinLeft=0.035742
		WinTop=0.185002
		Caption="Advertise Server"
		Hint="Publishes your server to the internet server browser."
		bSquare=true
		ComponentJustification=TXTA_Left
		CaptionWidth=0.925
	End Object
	Controls(4)=moCheckBox'MPServerAdvertise'

	Begin Object class=moCheckBox Name=MPServerCollectStats
		WinWidth=0.385938
		WinHeight=0.040000
		WinLeft=0.035742
		WinTop=0.247502
		Caption="Collect Player Stats"
		Hint="Publishes player stats from your server on the UT2004 stats website."
		bSquare=true
		ComponentJustification=TXTA_Left
		CaptionWidth=0.925
	End Object
	Controls(5)=moCheckBox'MPServerCollectStats'

	Begin Object class=moCheckBox Name=MPServerBalanceTeams
		WinWidth=0.385938
		WinHeight=0.040000
		WinLeft=0.035742
		WinTop=0.305835
		Caption="Balance Teams"
		Hint="Assigns teams automatically for players joining the server."
		bSquare=true
		ComponentJustification=TXTA_Left
		CaptionWidth=0.925
	End Object
	Controls(6)=moCheckBox'MPServerBalanceTeams'

	Begin Object class=moCheckBox Name=MPServerPlayersMustBeReady
		WinWidth=0.385938
		WinHeight=0.040000
		WinLeft=0.035742
		WinTop=0.362085
		Caption="Players Must Be Ready"
		Hint="When selected, all players will be required to press FIRE before the match begins."
		bSquare=true
		ComponentJustification=TXTA_Left
		CaptionWidth=0.925
	End Object
	Controls(7)=moCheckBox'MPServerPlayersMustBeReady'

	Begin Object class=moComboBox Name=MPServer_BotSkill
		WinWidth=0.385938
		WinHeight=0.060000
		WinLeft=0.036132
		WinTop=0.547865
		Caption="Bot Skill"
		Hint="Choose the skill of the bots you wish to play with."
		CaptionWidth=0.5
	End Object
	Controls(8)=moComboBox'MPServer_BotSkill'

	Begin Object class=moCheckBox Name=MPServerUseDefaults
		WinWidth=0.385938
		WinHeight=0.040000
		WinLeft=0.036132
		WinTop=0.622502
		Caption="Use Map Defaults"
		Hint="Uses the map's default minimum/maximum players settings."
		bSquare=true
		ComponentJustification=TXTA_Left
		CaptionWidth=0.925
		OnChange=UseMapOnChange
	End Object
	Controls(9)=moCheckBox'MPServerUseDefaults'

	Begin Object class=moCheckBox Name=MPServerUseCustomBots
		WinWidth=0.385938
		WinHeight=0.040000
		WinLeft=0.036132
		WinTop=0.716252
		Caption="Use Custom Bots"
		Hint="When enabled, you may use the Bot tab to choose bots to play with."
		bSquare=true
		ComponentJustification=TXTA_Left
		CaptionWidth=0.925
		OnChange=UseCustomOnChange
	End Object
	Controls(10)=moCheckBox'MPServerUseCustomBots'

	Begin Object class=moNumericEdit Name=MPServerMinPlayers
		WinWidth=0.381250
		WinHeight=0.060000
		WinLeft=0.035156
		WinTop=0.769792
		Caption="Min player count"
		CaptionWidth=0.7
		MinValue=0
		MaxValue=64
		ComponentJustification=TXTA_Left
		Hint="Bots will join the game if there are fewer players than the minimum."
	End Object
	Controls(11)=moNumericEdit'MPServerMinPlayers'

	Begin Object class=moNumericEdit Name=MPServerMaxPlayers
		WinWidth=0.381250
		WinHeight=0.060000
		WinLeft=0.035156
		WinTop=0.842708
		Caption="Max player count"
		CaptionWidth=0.7
		MinValue=1
		MaxValue=64
		ComponentJustification=TXTA_Left
		Hint="Limits the number of players allowed to join the server at once."
	End Object
	Controls(12)=moNumericEdit'MPServerMaxPlayers'

	Begin Object class=moNumericEdit Name=MPServerMaxSpecs
		WinWidth=0.381250
		WinHeight=0.060000
		WinLeft=0.035156
		WinTop=0.915624
		Caption="Max spectator count"
		CaptionWidth=0.7
		MinValue=0
		MaxValue=64
		ComponentJustification=TXTA_Left
		Hint="Limits the number of spectators allowed to join the server at once."
	End Object
	Controls(13)=moNumericEdit'MPServerMaxSpecs'

	Begin Object class=moEditBox Name=MPServerName
		WinWidth=0.500000
		WinHeight=0.060000
		WinLeft=0.468750
		WinTop=0.060938
		Caption="Server Name"
		Hint="The server name will be displayed in the server browser."
		CaptionWidth=0.4
	End Object
	Controls(14)=moEditBox'MPServerName'

	Begin Object class=moEditBox Name=MPServerPW
		WinWidth=0.500000
		WinHeight=0.060000
		WinLeft=0.468750
		WinTop=0.122605
		Caption="Game Password"
		Hint="Players must enter the game password to join your server."
		CaptionWidth=0.4
	End Object
	Controls(15)=moEditBox'MPServerPW'

	Begin Object class=moEditBox Name=MPServerAdminName
		WinWidth=0.500000
		WinHeight=0.060000
		WinLeft=0.468750
		WinTop=0.217605
		Caption="Admin Name"
		Hint="The admin name will be displayed in the server browser."
		CaptionWidth=0.4
	End Object
	Controls(16)=moEditBox'MPServerAdminName'

	Begin Object class=moEditBox Name=MPServerAdminEmail
		WinWidth=0.500000
		WinHeight=0.060000
		WinLeft=0.468750
		WinTop=0.282605
		Caption="Admin Email"
		Hint="The admin email address will be displayed in the server browser."
		CaptionWidth=0.4
	End Object
	Controls(17)=moEditBox'MPServerAdminEmail'

	Begin Object class=moEditBox Name=MPServerAdminPW
		WinWidth=0.500000
		WinHeight=0.060000
		WinLeft=0.468750
		WinTop=0.350938
		Caption="Admin Password"
		Hint="The admin password is used to connect to the server as an admin."
		CaptionWidth=0.4
	End Object
	Controls(18)=moEditBox'MPServerAdminPW'

	Begin Object class=moEditBox Name=MPServerMOTD1
		WinWidth=0.500000
		WinHeight=0.060000
		WinLeft=0.468750
		WinTop=0.499271
		Caption="MOTD 1"
		Hint="Players see the message of the day when they join the server."
		CaptionWidth=0.4
	End Object
	Controls(19)=moEditBox'MPServerMOTD1'

	Begin Object class=moEditBox Name=MPServerMOTD2
		WinWidth=0.500000
		WinHeight=0.060000
		WinLeft=0.468750
		WinTop=0.574271
		Caption="MOTD 2"
		Hint="Players see the message of the day when they join the server."
		CaptionWidth=0.4
	End Object
	Controls(20)=moEditBox'MPServerMOTD2'

	Begin Object class=moEditBox Name=MPServerMOTD3
		WinWidth=0.500000
		WinHeight=0.060000
		WinLeft=0.468750
		WinTop=0.649271
		Caption="MOTD 3"
		Hint="Players see the message of the day when they join the server."
		CaptionWidth=0.4
	End Object
	Controls(21)=moEditBox'MPServerMOTD3'

	Begin Object class=moEditBox Name=MPServerMOTD4
		WinWidth=0.500000
		WinHeight=0.060000
		WinLeft=0.468750
		WinTop=0.727604
		Caption="MOTD 4"
		Hint="Players see the message of the day when they join the server."
		CaptionWidth=0.4
	End Object
	Controls(22)=moEditBox'MPServerMOTD4'


	Begin Object class=moCheckBox Name=MPServerUseWebAdmin
		WinWidth=0.206250
		WinHeight=0.040000
		WinLeft=0.468750
		WinTop=0.869999
		Caption="Web Admin"
		Hint="Enables remote administration via the web."
		bSquare=true
		ComponentJustification=TXTA_Left
		CaptionWidth=0.8
	End Object
	Controls(23)=moCheckBox'MPServerUseWebAdmin'

	Begin Object class=moNumericEdit Name=MPServerWebPort
		WinWidth=0.2976250
		WinHeight=0.060000
		WinLeft=0.675156
		WinTop=0.859791
		Caption="Web Admin Port"
		CaptionWidth=0.65
		MinValue=0
		MaxValue=9999
		ComponentJustification=TXTA_Left
		Hint="The port used to connect to the web admin site."
	End Object
	Controls(24)=moNumericEdit'MPServerWebPort'


	Begin Object class=moCheckBox Name=MPServerAllowBehindView
		WinWidth=0.385938
		WinHeight=0.040000
		WinLeft=0.035742
		WinTop=0.476668
		Caption="Allow Behind view"
		Hint="When selected, the server will allow use of the 'behindview 1' console command."
		bSquare=true
		ComponentJustification=TXTA_Left
		CaptionWidth=0.925
	End Object
	Controls(25)=moCheckBox'MPServerAllowBehindView'

	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.77
	bAcceptsInput=false



}
