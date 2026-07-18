// ====================================================================
//  Class:  XInterface.Tab_InstantActionBotConfig
//  Parent: XInterface.GUITabPanel
//
//  <Enter a description here>
// ====================================================================

class Tab_InstantActionBotConfig extends UT2K3TabPanel;

var GUIListBox			MyRedTeam;
var GUIListBox			MyBlueTeam;
var GUIImage			MyPortrait;
var GUICharacterList	MyBotList;
var GUIButton			MyBotMaker;
var GUILabel			MyBotName;

var	bool				bIgnoreListChange;
var bool				bTeamGame;
var bool				bPlaySounds;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	Super.Initcomponent(MyController, MyOwner);

	MyRedTeam 	=	GUIListBox(Controls[1]);
	MyBlueTeam	=	GUIListBox(Controls[8]);
	MyPortrait	= 	GUIImage(Controls[4]);
	MyBotList	= 	GUICharacterList(Controls[9]);
	MyBotMaker	= 	GUIButton(Controls[10]);
	MyBotName	= 	GUILabel(Controls[11]);

	MyPortrait.Image = MyBotList.GetPortrait();
	MyBotName.Caption = MyBotList.GetName();
}

function ShowPanel(bool bShow)
{
	Super.ShowPanel(bShow);

	if(bShow)
		bPlaySounds = true;
	else
		bPlaySounds = false;
}

function SetupBotLists(bool bIsTeam)
{
	local int i;
	local TeamRedConfigured tRed;
	local TeamBlueConfigured tBlue;
	local DMRosterConfigured tDM;

	bTeamGame = bIsTeam;

	MyRedTeam.List.Clear();
	MyBLueTeam.List.Clear();

	if (bTeamGame)
	{
		tRed = PlayerOwner().Spawn(class'TeamRedConfigured');
		for (i=0;i<tRed.Characters.Length;i++)
			MyRedTeam.List.Add(tRed.Characters[i]);

		tRed.Destroy();

		tBlue = PlayerOwner().Spawn(class'TeamBlueConfigured');
		for (i=0;i<tBlue.Characters.Length;i++)
			MyBlueTeam.List.Add(tBlue.Characters[i]);

		tBlue.Destroy();
	}
	else
	{
		tDM = PlayerOwner().Spawn(class'DMRosterConfigured');
		for (i=0;i<tDM.Characters.Length;i++)
			MyRedTeam.List.Add(tDM.Characters[i]);

		tDM.Destroy();
	}

	// Team Game only
	Controls[6].bVisible=bTeamGame;
	Controls[7].bVisible=bTeamGame;
	Controls[8].bVisible=bTeamGame;
	Controls[12].bVisible=bTeamGame;
	Controls[13].bVisible=bTeamGame;
	// Deathmatch only
	Controls[14].bVisible=!bTeamGame;
}

// Play is called when the play button is pressed.  It saves any releavent data and then
// returns any additions to the URL
function string Play()
{
	local int i;
	local TeamRedConfigured tRed;
	local TeamBlueConfigured tBlue;
	local DMRosterConfigured tDM;
	local bool b1,b2;
	local string url;
	local int MinPlayers;

	MinPlayers = 0;

	if (bTeamGame)
	{

	 	tRed = PlayerOwner().Spawn(class'TeamRedConfigured');

	 	if (tRed.Characters.Length>0)
	 		tRed.Characters.Remove(0,tRed.Characters.Length);

	 	if (MyRedTeam.ItemCount() > 0)
	 	{
	 		for (i=0;i<MyRedTeam.ItemCount();i++)
	 			tRed.Characters[i]=MyRedTeam.List.GetItemAtIndex(i);
	 		MinPlayers += MyRedTeam.ItemCount();
	 		b1 = true;
	 	}
	 	else
	 		b1 = false;


	 	tRed.SaveConfig();
	 	tRed.Destroy();

	 	tBlue = PlayerOwner().Spawn(class'TeamBlueConfigured');

	 	if (tBlue.Characters.Length>0)
	 		tBlue.Characters.Remove(0,tBlue.Characters.Length);

	 	if (MyBlueTeam.ItemCount()>0)
	 	{
	 		for (i=0;i<MyBlueTeam.ItemCount();i++)
	 			tBlue.Characters[i]=MyBlueTeam.List.GetItemAtIndex(i);
	 		MinPlayers += MyBlueTeam.ItemCount();
	 		b2 = true;
	 	}
	 	else
	 		b2 = false;

	 	tBlue.SaveConfig();
	 	tBlue.Destroy();

	 	if (b1)
	 		url = url$"?RedTeam=xgame.TeamRedConfigured";

	 	if (b2)
	 		url = url$"?BlueTeam=xgame.TeamBlueConfigured";
	}
	else
	{
	 	tDM = PlayerOwner().Spawn(class'DMRosterConfigured');

	 	if (tDM.Characters.Length>0)
	 		tDM.Characters.Remove(0,tDM.Characters.Length);

	 	if (MyRedTeam.ItemCount() > 0)
	 	{
	 		for (i=0;i<MyRedTeam.ItemCount();i++)
	 			tDM.Characters[i]=MyRedTeam.List.GetItemAtIndex(i);
	 		MinPlayers += MyRedTeam.ItemCount();
	 	}

		tDM.SaveConfig();
		tDM.Destroy();

		url = url$"?DMTeam=xgame.DMRosterConfigured";
	}

	url = url$"?MinPlayers="$MinPlayers;

	return url;
}

function CharListChange(GUIComponent Sender)
{
	local sound NameSound;

	MyPortrait.Image = MyBotList.GetPortrait();
	MyBotName.Caption = MyBotList.GetName();

	// Play the bots name
	if(bPlaySounds)
	{
		NameSound = MyBotList.GetSound();
		PlayerOwner().ClientPlaySound(NameSound,,,SLOT_Interface);
	}
}

function bool RedClickAdd(GUIComponent Sender)
{
	if (MyRedTeam.ItemCount() < 16)
	{
		bIgnoreListChange=true;
		MyRedTeam.List.Add(MyBotName.Caption);
	}

	return true;
}

function bool BlueClickAdd(GUIComponent Sender)
{
	if (MyBlueTeam.ItemCount() < 16)
	{
		bIgnoreListChange=true;
		MyBlueTeam.List.Add(MyBotName.Caption);
	}

	return true;
}

function bool RedClickRemove(GUIComponent Sender)
{
	local int index;

	if (MyRedTeam.ItemCount()==0)
		return true;

	Index = MyRedTeam.List.Index;
	if (Index<0)
		return true;

	bIgnoreListChange=true;
	MyRedTeam.List.Remove(Index,1);
	if (Index<MyRedTeam.List.ItemCount)
		MyRedTeam.List.SetIndex(Index);


	return true;
}

function bool BlueClickRemove(GUIComponent Sender)
{
	local int index;

	if (MyBlueTeam.ItemCount()==0)
		return true;

	Index = MyBlueTeam.List.Index;
	if (Index<0)
		return true;

	bIgnoreListChange=true;
	MyBlueTeam.List.Remove(Index,1);
	if (Index<MyBlueTeam.List.ItemCount)
		MyBlueTeam.List.SetIndex(Index);

	return true;
}


function ListChange(GUIComponent Sender)
{
	local GUIListBox Who;
	local string WhoName;

	if (bIgnoreListChange)
	{
		bIgnoreListChange=false;
		return;
	}

	WHo = GUIListBox(Sender);
	if (Who==None)
		return;

	WhoName = Who.List.Get();
	if (WhoName!="")
		MyBotList.Find(WhoName);



	return;
}

function bool InternalOnClick(GUIComponent Sender)
{
	if (Sender==Controls[15])
		MyBotList.PgUp();
	else if (Sender==Controls[16])
		MyBotList.PgDown();

	return true;
}

function bool BotInfoClick(GUIComponent Sender)
{
	if (Controller.OpenMenu("XInterface.UT2BotInfoPage"))
		UT2BotInfoPage(Controller.TopPage()).SetupBotInfo(MyBotList.GetPortrait(), MyBotList.GetDecoText(), MyBotList.GetRecord());

	return true;
}

function bool BotConfigClick(GUIComponent Sender)
{
	if (Controller.OpenMenu("XInterface.UT2BotConfigPage"))
		UT2BotConfigPage(Controller.TopPage()).SetupBotInfo(MyBotList.GetPortrait(), MyBotList.GetDecoText(), MyBotList.GetRecord());

	return true;
}


defaultproperties
{
	Begin Object class=GUIImage Name=IABotConfigBK1
		WinWidth=0.962383
		WinHeight=0.370860
		WinLeft=0.016758
		WinTop=0.630156
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
		bVisible=false
	End Object
	Controls(0)=GUIImage'IABotConfigBK1'

	Begin Object Class=GUIListBox Name=IABotConfigRedList
		WinWidth=0.324414
		WinHeight=0.458751
		WinLeft=0.026758
		WinTop=0.115833
		bVisibleWhenEmpty=true
		StyleName="SquareButton"
		Hint="The Red Team's Roster."
		OnChange=ListChange
	End Object
	Controls(1)=GUIListBox'IABotConfigRedList'

	Begin Object Class=GUIHorzScrollButton Name=IABotConfigRedAdd
		WinWidth=0.064648
		WinHeight=0.079297
		WinLeft=0.357617
		WinTop=0.208334
		LeftButton=True
		OnClick=RedClickAdd
		Hint="Add Bot to Red Team"
	End Object
	Controls(2)=GUIHorzScrollButton'IABotConfigRedAdd'

	Begin Object Class=GUIHorzScrollButton Name=IABotConfigRedRemove
		WinWidth=0.064648
		WinHeight=0.079297
		WinLeft=0.357617
		WinTop=0.358073
		OnClick=RedClickRemove
		Hint="Remove Bot from Red Team"
	End Object
	Controls(3)=GUIHorzScrollButton'IABotConfigRedRemove'


	Begin Object class=GUIImage Name=IABotConfigPortrait
		WinWidth=0.118711
		WinHeight=0.451251
		WinLeft=0.440547
		WinTop=0.119166
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Scaled
	End Object
	Controls(4)=GUIImage'IABotConfigPortrait'

	Begin Object class=GUIImage Name=IABotConfigPortraitBorder
		WinWidth=0.123711
		WinHeight=0.458751
		WinLeft=0.438047
		WinTop=0.115833
		Image=Material'InterfaceContent.Menu.BorderBoxA1'
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Stretched
	End Object
	Controls(5)=GUIImage'IABotConfigPortraitBorder'

	Begin Object Class=GUIHorzScrollButton Name=IABotConfigBlueAdd
		WinWidth=0.064648
		WinHeight=0.079297
		WinLeft=0.577344
		WinTop=0.208334
		OnClick=BlueClickAdd
		Hint="Add Bot to Blue Team"
	End Object
	Controls(6)=GUIHorzScrollButton'IABotConfigBlueAdd'

	Begin Object Class=GUIHorzScrollButton Name=IABotConfigBlueRemove
		WinWidth=0.064648
		WinHeight=0.079297
		WinLeft=0.577344
		WinTop=0.358073
		LeftButton=True
		OnClick=BlueClickRemove
		Hint="Remove Bot from Blue Team"
	End Object
	Controls(7)=GUIHorzScrollButton'IABotConfigBlueRemove'

	Begin Object Class=GUIListBox Name=IABotConfigBlueList
		WinWidth=0.324414
		WinHeight=0.463633
		WinLeft=0.647853
		WinTop=0.115833
		bVisibleWhenEmpty=true
		StyleName="SquareButton"
		Hint="The Blue Team's Roster."
		OnChange=ListChange
	End Object
	Controls(8)=GUIListBox'IABotConfigBlueList'

	Begin Object class=GUICharacterList Name=IABotConfigCharList
		WinWidth=0.724609
		WinHeight=0.236758
		WinLeft=0.137890
		WinTop=0.689897
		Hint="Select a bot to add to a team"
		StyleName="CharButton"
		OnChange=CharListChange
	End Object
	Controls(9)=GUICharacterList'IABotConfigCharList'

	Begin Object Class=GUIButton Name=IABotConfigConfig
		Caption="More Info"
		Hint="Find out more info on this bot"
		WinWidth=0.239063
		WinHeight=0.049765
		WinLeft=0.249806
		WinTop=0.951199
		OnClick=BotInfoClick
	End Object
	Controls(10)=GUIButton'IABotConfigConfig'

	Begin Object class=GUILabel Name=IABotConfigName
		Caption=""
		TextALign=TXTA_Center
		TextColor=(R=220,G=180,B=0,A=255)
		TextFont="UT2HeaderFont"
		WinWidth=1
		WinHeight=0.13
		WinLeft=0
		WinTop=0.581770
	End Object
	Controls(11)=GUILabel'IABotConfigName'

	Begin Object class=GUILabel Name=IABotConfigRedName
		Caption="Red Team"
		TextALign=TXTA_Left
		TextColor=(R=255,G=0,B=0,A=255)
		WinWidth=0.250000
		WinHeight=0.130000
		WinLeft=0.039063
		WinTop=0.027082
	End Object
	Controls(12)=GUILabel'IABotConfigRedName'

	Begin Object class=GUILabel Name=IABotConfigBlueName
		Caption="Blue Team"
		TextALign=TXTA_Left
		TextColor=(R=0,G=0,B=255,A=255)
		WinWidth=0.250000
		WinHeight=0.130000
		WinLeft=0.655862
		WinTop=0.027082
	End Object
	Controls(13)=GUILabel'IABotConfigBlueName'

	Begin Object class=GUILabel Name=IABotConfigDMName
		Caption="Deathmatch"
		TextALign=TXTA_Left
		TextColor=(R=255,G=0,B=0,A=255)
		WinWidth=0.250000
		WinHeight=0.130000
		WinLeft=0.039063
		WinTop=0.032552
		bVisible=false
	End Object
	Controls(14)=GUILabel'IABotConfigDMName'

	Begin Object class=GUIButton Name=BotLeft
		WinWidth=0.043555
		WinHeight=0.084414
		WinLeft=0.108203
		WinTop=0.775783
		bNeverFocus=true
		bRepeatClick=true
		OnClick=InternalOnClick
		StyleName="ArrowLeft"
	End Object
	Controls(15)=GUIButton'BotLeft'

	Begin Object class=GUIButton Name=BotRight
		WinWidth=0.043555
		WinHeight=0.084414
		WinLeft=0.845899
		WinTop=0.775783
		StyleName="ArrowRight"
		bNeverFocus=true
		bRepeatClick=true
		OnClick=InternalOnClick
	End Object
	Controls(16)=GUIButton'BotRight'

	Begin Object Class=GUIButton Name=IABotConfigDoConfig
		Caption="Customize"
		Hint="Change the AI attributes for this bot"
		WinWidth=0.239063
		WinHeight=0.049765
		WinLeft=0.491993
		WinTop=0.951199
		OnClick=BotConfigClick
	End Object
	Controls(17)=GUIButton'IABotConfigDoConfig'


	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.77
	bAcceptsInput=false
}
