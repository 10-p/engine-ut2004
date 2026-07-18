// ====================================================================
//  Class:  XInterface.Tab_SPRoster
//  Parent: XInterface.GUITabPanel
//  Single player menu for fussing with the player roster.  Choose
//  lineup and give orders.
//  author:  capps 8/24/02
// ====================================================================

class Tab_SPRoster extends Tab_SPPanelBase;

const MatesCount = 4;
const MatesBase = 8;
const MatesCtrls = 5;

var array<GUIPanel>		pnlMates;		// 1 Panel per mate
var array<GUIGfxButton>	imgMates;		// TeamMate Image button
var array<GUILabel>		lblMates;		// TeamMate Name display
var array<GUIComboBox>	cboMates;		// TeamMate Fight Position
var array<GUIButton>	btnMates;		// TeamMate Assign Button
var array<GUILabel>		lblNA;		// 'spot not available'
var GUIListBox			lboStats, lboTeamStats;
var GUIImage			imgPortrait;
var GUILabel			lblMatchData;
var GUIScrollTextBox	stbPlayerData;
var GUICharacterListTeam cltMyTeam;

var localized string	MessageNoInfo, PreStatsMessage, PostStatsMessage;


function Created()
{
local GUIPanel Pnl;
local int i;

	for (i = 0; i<MatesCount; i++)
	{
		Pnl = GUIPanel(Controls[i+MatesBase]);
		pnlMates[i] = Pnl;
		imgMates[i] = GUIGfxButton(Pnl.Controls[1]);
		lblMates[i] = GUILabel(Pnl.Controls[2]);
		cboMates[i] = GUIComboBox(Pnl.Controls[3]);
		btnMates[i] = GUIButton(Pnl.Controls[4]);
		lblNA[i]	= GUILabel(Pnl.Controls[5]);
	}
	imgPortrait = GUIImage(Controls[1]);
	lblMatchData = GUILabel(Controls[3]);
	stbPlayerData = GUIScrollTextBox(Controls[4]);
	lboStats = GUIListBox(Controls[5]);
	cltMyTeam = GUICharacterListTeam(Controls[6]);
	lboTeamStats = GUIListBox(Controls[12]);
}

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
local int i, j;

	MyController.RegisterStyle(class'STY_RosterButton');

	Super.Initcomponent(MyController, MyOwner);

	lboStats.List.bAcceptsInput = false;
	lboTeamStats.List.bAcceptsInput = false;

	for (i = 0; i<MatesCount; i++)
	{
		cboMates[i].OnChange=none;
		cboMates[i].ReadOnly(true);
		for (j = 0; j < class'GameProfile'.static.GetNumPositions(); j++)
			cboMates[i].AddItem(class'GameProfile'.static.TextPositionDescription(j));
		cboMates[i].OnChange=PositionChange;
		imgMates[i].OnClick=ClickLineup;
	}
	OnProfileUpdated();
}

// if no gameprofile loaded, or no team, can't show profile
function bool CanShowPanel ()
{
	if ( GetProfile() == none )
		return false;

	if ( GetProfile().PlayerTeam[0] == "" )		// indicates hasn't drafted team
		return false;

	return Super.CanShowPanel();
}

function OnProfileUpdated()
{
	ReloadPortraits();
	ChangePortraits();
	UpdateMatchInfo();
	BuildTeamStats(lboTeamStats);
}

// Called when Match has changed
function OnMatchUpdated(int iLadder, int iMatch)
{
	UpdateMatchInfo();
}

// if user clicks on a lineup picture, change the portrait/description
function bool ClickLineup(GUIComponent Sender) {

	local int i, teammate;

	teammate = -1;
	for (i=0; i< MatesCount; i++) 
	{
		if (Sender == imgMates[i])
		{
			teammate = i;
			break;
		}
	}

	if (teammate < 0)
		return false;

	cltMyTeam.Find(lblMates[teammate].Caption);

	ChangePortraits();

	return true;
}

function bool FixLineup(GUIComponent Sender)
{
	if (GetProfile() != None)
	{
		GetProfile().SetLineup(Sender.Tag, cltMyTeam.Index);
		if (!PlayerOwner().Level.Game.SavePackage(GetProfile().PackageName))
		{
			Log("SINGLEPLAYER couldn't save profile package!");
		}
		UpdateMatchInfo();
	}
	return true;
}

// update the match data
// includes removing characters when match doesn't need 'em
function UpdateMatchInfo()
{
local GameProfile GP;
local int nummates, i;

	GP = GetProfile();
	if ( GP == none )
		return;

	lblMatchData.Caption = GP.GetMatchDescription();
	nummates = GP.GetNumTeammatesForMatch();

	for (i = 0; i<MatesCount; i++)
	{
		imgMates[i].bVisible = (nummates > i);
		lblMates[i].bVisible = (nummates > i);
		cboMates[i].bVisible = (nummates > i);
		lblNA[i].bVisible = !(nummates > i);
		btnMates[i].bVisible = false;
		
		// Dont update unnecessarily
		if (nummates > i)
		{
			imgMates[i].Graphic = cltMyTeam.PlayerList[GP.PlayerLineup[i]].Portrait;
			lblMates[i].Caption = cltMyTeam.PlayerList[GP.PlayerLineup[i]].DefaultName;
			cboMates[i].OnChange = None;
			cboMates[i].SetText(GP.GetPositionDescription(GP.PlayerLineup[i]));
			//Log("Mate "$i$" named "$GP.PlayerTeam[GP.PlayerLineup[i]]$" has description "$GP.GetPositionDescription(GP.PlayerLineup[i]));
			cboMates[i].OnChange = PositionChange;
			
			// See if we should enable the assign button
			if (cltMyTeam.Index != -1 && cltMyTeam.Index != GP.PlayerLineup[i])
			{
				btnMates[i].bVisible = true;
				btnMates[i].OnChange = PositionChange;
				imgMates[i].WinLeft = btnMates[i].WinLeft + btnMates[i].WinWidth * 1.35;
			}
			else
				imgMates[i].WinLeft = btnMates[i].WinLeft;
		}
	}
}

function ReloadPortraits()
{
local GameProfile GP;
local array<xUtil.PlayerRecord> PlayerRecords;
local int i;

	GP = GetProfile();
	if (GP != None)
	{
		for ( i=0; i<GP.PlayerTeam.Length; i++ )
		{
			PlayerRecords[i] = class'xUtil'.static.FindPlayerRecord(GP.PlayerTeam[i]);
		}
		cltMyTeam.ResetList(PlayerRecords, GP.PlayerTeam.Length);
	}
}

function CharListClick(GUIComponent Sender)
{
	ChangePortraits();
	UpdateMatchInfo();
}

function ChangePortraits(/*GUIComponent Sender*/)
{
local GameProfile GP;
//local int i;
local string str;

	GP = GetProfile();
	if ( GP == none ) {
		//Log ("Tab_SPRoster::ChangePortraits could not find game profile.");
		return;
	}

	if ( cltMyTeam.Index < 0  && cltMyteam.PlayerList.Length > 0 )
		cltMyTeam.SetIndex(0);

	if (cltMyTeam.Index != -1)
	{
		// selected-bot portrait
		imgPortrait.Image = cltMyTeam.GetPortrait();

		// selected-bot sound, only if this tab is active
		if (MyButton != none && MyButton.bActive){
			PlayerOwner().ClientPlaySound(cltMyTeam.GetSound(),,,SLOT_Interface);
		}

		// selected-bot decotext
		str = Mid(cltMyTeam.PlayerList[(cltMyTeam.Index)].TextName, Max(InStr(cltMyTeam.PlayerList[(cltMyTeam.Index)].TextName, ".")+1, 0));

		if (str == "")
			str = MessageNoInfo;

		stbPlayerData.SetContent(Controller.LoadDecoText("xPlayers", str));

		// selected-bot stats
		self.BuildCharStats(cltMyTeam.PlayerList[cltMyTeam.Index], lboStats);
	}
	else
	{
		// Clear the image
		imgPortrait.Image = None;
		// clear the decotext
		stbPlayerData.SetContent (MessageNoInfo);
		// Clear the stats listbox
		lboStats.List.Clear();
	}
}

function BuildCharStats ( out xUtil.PlayerRecord PR, GUIListBox box )
{
	local string str;
	box.List.Clear();
	str = class'xUtil'.static.GetFavoriteWeaponFor(PR);
	box.List.Add(str);
	str = class'xUtil'.Default.AccuracyString@class'xUtil'.static.AccuracyRating(PR);
	box.List.Add(str);
	str = class'xUtil'.Default.AggressivenessString@class'xUtil'.static.AggressivenessRating(PR);
	box.List.Add(str);
	str = class'xUtil'.Default.AgilityString@class'xUtil'.static.AgilityRating(PR);
	box.List.Add(str);
	str = class'xUtil'.Default.TacticsString@class'xUtil'.static.TacticsRating(PR);
	box.List.Add(str);
	box.List.Index = -1;	// to avoid highlight of first line
	//str = "Salary:"@class'xUtil'.static.GetSalaryFor(PR)$"M";
	//box.List.Add(str);
}

function BuildTeamStats ( GUIListBox teambox )
{
	local string str;
	local GameProfile GP;
	GP = GetProfile();
	if ( GP == none ) 
		return;
	// team stats
	teambox.List.Clear();
	str = PreStatsMessage@GP.TeamName@PostStatsMessage;
	teambox.List.Add(str);
	str = class'xUtil'.Default.AccuracyString@class'xUtil'.static.TeamAccuracyRating(GP);
	teambox.List.Add(str);
	str = class'xUtil'.Default.AggressivenessString@class'xUtil'.static.TeamAggressivenessRating(GP);
	teambox.List.Add(str);
	str = class'xUtil'.Default.AgilityString@class'xUtil'.static.TeamAgilityRating(GP);
	teambox.List.Add(str);
	str = class'xUtil'.Default.TacticsString@class'xUtil'.static.TeamTacticsRating(GP);
	teambox.List.Add(str);
}

// update a position 
function PositionChange(GUIComponent Sender) 
{
	local int lineupnum, i;
	local string posn;

	lineupnum = -1;
	for ( i = 0; i < 4; i++ ) 
	{
		if ( Sender == cboMates[i] ) 
		{
			lineupnum = i;
			break;
		}
	}

	posn = GUIComboBox(Sender).GetText();
	if (GetProfile() != None)
	{
		GetProfile().SetPosition(lineupnum, posn);
		if (!PlayerOwner().Level.Game.SavePackage(GetProfile().PackageName))
		{
			Log("SINGLEPLAYER couldn't save profile package!");
		}
	}
}

defaultproperties
{	
	// box to go around selected-character information
	Begin Object class=GUIImage Name=SPRosterBK0
		WinWidth=0.565117
		WinHeight=0.70
		WinLeft=0.007187
		WinTop=0.015
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	
	// portrait of the selected character
	Begin Object class=GUIImage Name=SPRosterPortrait
		WinWidth=0.130957
		WinHeight=0.395000
		WinLeft=0.016562
		WinTop=0.031077
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Scaled
	End Object

	// cool border for the close-up portrait
	Begin Object class=GUIImage Name=SPRosterPortraitBorder
		WinWidth=0.1333
		WinHeight=0.40
		WinLeft=0.015
		WinTop=0.03
		Image=Material'InterfaceContent.Menu.BorderBoxA1'
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Stretched
	End Object

	// player's team roster
	Begin Object class=GUICharacterListTeam Name=SPRosterCharList
		Hint="Choose a teammate to play in the next match"
		WinWidth=0.670315
		WinHeight=0.170000
		WinLeft=0.004688
		WinTop=0.73
		StyleName="CharButton"
		OnChange=CharListClick
		bFillBounds=true
		FixedItemsPerPage=7
		bLocked=true
		bIgnoreBackClick=true
		bAllowSelectEmpty=false
		DefaultPortrait=Material'InterfaceContent.pEmptySlot'
	End Object

	// match description
	Begin Object class=GUILabel Name=SPMatchData
		Caption="No Game Profile => No MatchData"
		TextALign=TXTA_Center
		TextFont="UT2LargeFont"
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.800000
		WinHeight=0.100000
		WinLeft=0.10
		WinTop=-0.12
		bVisible=false
	End Object

	// char data decotext
	Begin Object class=GUIScrollTextBox Name=SPCharData
		Hint="Team members profile"
		TextAlign=TXTA_Left
		WinWidth=0.412500
		WinHeight=0.40
		WinLeft=0.15
		WinTop=0.03
		CharDelay=0.04
		EOLDelay=0.25
		bNeverFocus=true
	End Object

	// char stats
	Begin Object class=GUIListBox Name=SPCharStats
		//TextFont="UT2SmallFont"
		//TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.51
		WinHeight=0.267500
		WinLeft=0.015
		WinTop=0.44
		bNeverFocus=true
		bAcceptsInput=false
	End Object

	// border to go around char selector
	Begin Object class=GUIImage Name=SPCharListBox
		WinWidth=0.622268
		WinHeight=0.186797
		WinLeft=0.006836
		WinTop=0.722
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object

///////////////////////////////////////////////
//
// Team Mate #1
//
//
	Begin Object class=GUIImage Name=Mate1Back
		WinWidth=1.0
		WinHeight=1.0
		WinLeft=0
		WinTop=0
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
		Tag=0
	End Object
	// portrait of the teammate #1
	Begin Object class=GUIGfxButton Name=imgMate1
		WinWidth=0.14
		WinHeight=0.896118
		WinLeft=0.018244
		WinTop=0.052442
	    Position=ICP_Scaled
		Graphic=Material'InterfaceContent.Menu.BorderBoxD'
	End Object
	
	Begin Object class=GUILabel Name=lblMate1
		Caption="Name"
		TextALign=TXTA_Center
		TextFont="UT2SmallFont"
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.797917
		WinHeight=0.213511
		WinLeft=0.174012
		WinTop=0.249487
	End Object

	Begin Object class=GUIComboBox Name=cboMate1
		Hint="Set starting position - change with voice menu during match"
		WinHeight=0.234660
		WinTop=0.567351
		WinWidth=0.554010
		WinLeft=0.323564
		OnChange=PositionChange
	End Object

	Begin Object class=GUIGFXButton Name=btnMate1
		Graphic=Material'InterfaceContent.Menu.YellowArrowVBand'
		Hint="Assign the selected team member to this roster"
		Position=ICP_Scaled
		bClientBound=true
		WinWidth=0.058350
		WinHeight=0.901195
		WinLeft=0.012050
		WinTop=0.046398
		StyleName="RosterButton"
		OnClick=FixLineup
		Tag=0
	End Object

	Begin Object class=GUILabel Name=lblNA1
		Caption="SLOT NOT AVAILABLE"
		TextALign=TXTA_Center
		TextFont="UT2SmallFont"
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=1.0
		WinHeight=0.21
		WinLeft=0.0
		WinTop=0.42
	End Object

	Begin Object class=GUIPanel Name=pnlMates1
		WinWidth=0.40
		WinHeight=0.175
		WinLeft=0.59
		WinTop=0.015
		StyleName="NoBackground"
		Controls(0)=Mate1Back
		Controls(1)=imgMate1
		Controls(2)=lblMate1
		Controls(3)=cboMate1
		Controls(4)=btnMate1
		Controls(5)=lblNA1
	End Object

///////////////////////////////////////////////
//
// Team Mate #2
//
//
	// box to go around teammate #2
	Begin Object class=GUIImage Name=Mate2Back
		WinWidth=1.0
		WinHeight=1.0
		WinLeft=0
		WinTop=0
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
		Tag=1
	End Object
	// portrait of the teammate #2
	Begin Object class=GUIGfxButton Name=imgMate2
		WinWidth=0.14
		WinHeight=0.896118
		WinLeft=0.018244
		WinTop=0.052442
	    Position=ICP_Scaled
		Graphic=Material'InterfaceContent.Menu.BorderBoxD'
	End Object
	Begin Object class=GUILabel Name=lblMate2
		Caption="Name"
		TextALign=TXTA_Center
		TextFont="UT2SmallFont"
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.797917
		WinHeight=0.213511
		WinLeft=0.174012
		WinTop=0.249487
	End Object

	Begin Object class=GUIComboBox Name=cboMate2
		Hint="Set starting position - change with voice menu during match"
		WinHeight=0.234660
		WinTop=0.567351
		WinWidth=0.554010
		WinLeft=0.323564
		OnChange=PositionChange
	End Object

	Begin Object class=GUIGFXButton Name=btnMate2
		Graphic=Material'InterfaceContent.Menu.YellowArrowVBand'
		Hint="Assign the selected team member to this roster"
		Position=ICP_Scaled
		bClientBound=true
		WinWidth=0.058350
		WinHeight=0.901195
		WinLeft=0.012050
		WinTop=0.046398
		StyleName="RosterButton"
		OnClick=FixLineup
		Tag=1
	End Object

	Begin Object class=GUILabel Name=lblNA2
		Caption="SLOT NOT AVAILABLE"
		TextALign=TXTA_Center
		TextFont="UT2SmallFont"
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=1.0
		WinHeight=0.21
		WinLeft=0.0
		WinTop=0.42
	End Object

	Begin Object class=GUIPanel Name=pnlMates2
		WinWidth=0.40
		WinHeight=0.175
		WinLeft=0.59
		WinTop=0.19
		StyleName="NoBackground"
		Controls(0)=Mate2Back
		Controls(1)=imgMate2
		Controls(2)=lblMate2
		Controls(3)=cboMate2
		Controls(4)=btnMate2
		Controls(5)=lblNA2
	End Object

///////////////////////////////////////////////
//
// Team Mate #3
//
//
	Begin Object class=GUIImage Name=Mate3Back
		WinWidth=1.0
		WinHeight=1.0
		WinLeft=0
		WinTop=0
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	
	Begin Object class=GUIGfxButton Name=imgMate3
		WinWidth=0.14
		WinHeight=0.896118
		WinLeft=0.018244
		WinTop=0.052442
	    Position=ICP_Scaled
		Graphic=Material'InterfaceContent.Menu.BorderBoxD'
	End Object
	
	Begin Object class=GUILabel Name=lblMate3
		Caption="Name"
		TextALign=TXTA_Center
		TextFont="UT2SmallFont"
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.797917
		WinHeight=0.213511
		WinLeft=0.174012
		WinTop=0.249487
	End Object

	Begin Object class=GUIComboBox Name=cboMate3
		Hint="Set starting position - change with voice menu during match"
		WinHeight=0.234660
		WinTop=0.567351
		WinWidth=0.554010
		WinLeft=0.323564
		OnChange=PositionChange
	End Object

	Begin Object class=GUIGFXButton Name=btnMate3
		Graphic=Material'InterfaceContent.Menu.YellowArrowVBand'
		Hint="Assign the selected team member to this roster"
		Position=ICP_Scaled
		bClientBound=true
		WinWidth=0.058350
		WinHeight=0.901195
		WinLeft=0.012050
		WinTop=0.046398
		StyleName="RosterButton"
		OnClick=FixLineup
		Tag=2
	End Object

	Begin Object class=GUILabel Name=lblNA3
		Caption="SLOT NOT AVAILABLE"
		TextALign=TXTA_Center
		TextFont="UT2SmallFont"
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=1.0
		WinHeight=0.21
		WinLeft=0.0
		WinTop=0.42
	End Object

	Begin Object class=GUIPanel Name=pnlMates3
		WinWidth=0.40
		WinHeight=0.175
		WinLeft=0.59
		WinTop=0.365
		StyleName="NoBackground"
		Controls(0)=Mate3Back
		Controls(1)=imgMate3
		Controls(2)=lblMate3
		Controls(3)=cboMate3
		Controls(4)=btnMate3
		Controls(5)=lblNA3
	End Object

///////////////////////////////////////////////
//
// Team Mate #4
//
//
	// box to go around teammate #4
	Begin Object class=GUIImage Name=Mate4Back
		WinWidth=1.0
		WinHeight=1.0
		WinLeft=0
		WinTop=0
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
		Tag=3
	End Object
	// portrait of the teammate #4
	Begin Object class=GUIGfxButton Name=imgMate4
		WinWidth=0.14
		WinHeight=0.896118
		WinLeft=0.018244
		WinTop=0.052442
		Graphic=Material'InterfaceContent.Menu.BorderBoxD'
	    Position=ICP_Scaled
	End Object

	// teammate description 4
	Begin Object class=GUILabel Name=lblMate4
		Caption="Name"
		TextALign=TXTA_Center
		TextFont="UT2SmallFont"
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.797917
		WinHeight=0.213511
		WinLeft=0.174012
		WinTop=0.249487
	End Object

	Begin Object class=GUIComboBox Name=cboMate4
		Hint="Set starting position - change with voice menu during match"
		WinWidth=0.554010
		WinHeight=0.234660
		WinLeft=0.323564
		WinTop=0.567351
		OnChange=PositionChange
	End Object

	Begin Object class=GUIGFXButton Name=btnMate4
		Graphic=Material'InterfaceContent.Menu.YellowArrowVBand'
		Hint="Assign the selected team member to this roster"
		Position=ICP_Scaled
		bClientBound=true
		WinWidth=0.058350
		WinHeight=0.901195
		WinLeft=0.012050
		WinTop=0.046398
		StyleName="RosterButton"
		OnClick=FixLineup
		Tag=3
	End Object

	Begin Object class=GUILabel Name=lblNA4
		Caption="SLOT NOT AVAILABLE"
		TextALign=TXTA_Center
		TextFont="UT2SmallFont"
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=1.0
		WinHeight=0.21
		WinLeft=0.0
		WinTop=0.42
	End Object

	Begin Object class=GUIPanel Name=pnlMates4
		WinWidth=0.40
		WinHeight=0.175
		WinLeft=0.59
		WinTop=0.54
		StyleName="NoBackground"
		Controls(0)=Mate4Back
		Controls(1)=imgMate4
		Controls(2)=lblMate4
		Controls(3)=cboMate4
		Controls(4)=btnMate4
		Controls(5)=lblNA4
	End Object

	// team stats
	Begin Object class=GUIListBox Name=SPRTeamStats
		WinWidth=0.338750
		WinHeight=0.186797
		WinLeft=0.654063
		WinTop=0.722
		bNeverFocus=true
		bAcceptsInput=false
	End Object

	Controls(0)=SPRosterBK0
	Controls(1)=SPRosterPortrait
	Controls(2)=SPRosterPortraitBorder
	Controls(3)=SPMatchData
	Controls(4)=SPCharData
	Controls(5)=SPCharStats
	Controls(6)=SPRosterCharList
	Controls(7)=SPCharListBox

	Controls(8)=pnlMates1
	Controls(9)=pnlMates2
	Controls(10)=pnlMates3
	Controls(11)=pnlMates4

	Controls(12)=SPRTeamStats

	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.77
	bAcceptsInput=false		
	bFillHeight=true		// get it to fill vertical space from tab top

	MessageNoInfo="No information available."
	PreStatsMessage=""
	PostStatsMessage="Stats"
}
