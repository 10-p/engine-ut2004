// ====================================================================
//  Class:  XInterface.UT2DraftTeam
//  Parent: XInterface.GUIPage
//
//  Draft the team.  Salary stuff commented out for UT2003.
// ====================================================================

class UT2DraftTeam extends UT2K3GUIPage;

var GUIListBox			lboStats, lboTeamStats;
var GUILabel			lblMatchData, lblChoose;
var GUIScrollTextBox	stbPlayerData;
var GUICharacterListTeam cltMyTeam;
var GUICharacterListTeam cltPortrait;
var GUIButton			butDraft, butRelease, butClear, butEnter, butAuto;
var GUIGfxButton		butLeft, butRight;
var GUITitleBar			MyTitleBar, MyHintBar;

var string				ButtonStyleEnabled, ButtonStyleDisabled;
var localized string	ClearConfirmMessage, EnterConfirmMessage, StatsMessage;

var bool bPlaySounds;	// whether to play the announcer/select sounds

function Created()
{
	lblMatchData = GUILabel(Controls[1]);
	stbPlayerData = GUIScrollTextBox(Controls[2]);
	lboStats = GUIListBox(Controls[3]);
	cltMyTeam = GUICharacterListTeam(Controls[4]);
	cltPortrait = GUICharacterListTeam(Controls[6]);
	butDraft = GUIButton(Controls[7]);
	butRelease = GUIButton(Controls[8]);
	butClear = GUIButton(Controls[9]);
	butEnter = GUIButton(Controls[10]);
	butLeft = GUIGfxButton(Controls[11]);
	butRight = GUIGfxButton(Controls[12]);
	lblChoose = GUILabel(Controls[13]);
	lboTeamStats = GUIListBox(Controls[14]);
	MyTitleBar = GUITitleBar(Controls[15]);
	MyHintBar = GUITitleBar(Controls[16]);
	butAuto = GUIButton(Controls[17]);
	OnKeyEvent=MyKeyEvent;
}

// capture escape-hits
function bool MyKeyEvent(out byte Key,out byte State,float delta)
{
	if ( Key == 27 )	// Escape pressed
	{
		return true;
	}
}

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	MyController.RegisterStyle(class'STY_RosterButton');

	Super.Initcomponent(MyController, MyOwner);
	cltPortrait.InitListInclusive("SP");
	cltPortrait.Find("Gorge");
	cltPortrait.OnChange=CharListChange;
	CharListUpdate(cltPortrait);

	ReloadPortraits();

	butEnter.bVisible = false;
	butEnter.bAcceptsInput = false;


	butDraft.OnClick=ButtonClick;
	butRelease.OnClick=ButtonClick;
	butClear.OnClick=ButtonClick;
	butEnter.OnClick=ButtonClick;
	butLeft.OnClick=ButtonClick;
	butRight.OnClick=ButtonClick;
	butAuto.OnClick=ButtonClick;

	lboStats.List.bAcceptsInput=false;
	lboTeamStats.List.bAcceptsInput=false;

	cltMyTeam.OnChange=CharListChange;
	cltMyTeam.Index = -1;
}

function ReloadPortraits()
{
	local GameProfile GP;
	local array<xUtil.PlayerRecord> PlayerRecords;
	local int i;

	GP = PlayerOwner().Level.Game.CurrentGameProfile;
	if (GP != None)
	{
		for ( i=0; i<GP.PlayerTeam.Length; i++ ) {
			PlayerRecords[i] = class'xUtil'.static.FindPlayerRecord(GP.PlayerTeam[i]);
			//Log("UT2DraftTeam::ReloadPortraits scanning"@GP.PlayerTeam[i]);
		}

		cltMyTeam.ResetList(PlayerRecords, GP.PlayerTeam.Length);
	}
}


function CharListUpdate(GUIComponent Sender)
{
	local GUICharacterList GCL;
	local xUtil.PlayerRecord PR;
	//local GameProfile GP;
	local string str;

	GCL = cltPortrait;
	PR = GCL.GetRecord();

	if ( bPlaySounds )
		PlayerOwner().ClientPlaySound(GCL.GetSound(),,,SLOT_Interface);

	str = Mid(PR.TextName, Max(InStr(PR.TextName, ".")+1, 0));
	stbPlayerData.SetContent(Controller.LoadDecoText("xPlayers", str));

	BuildStats ( PR, lboStats, lboTeamStats );
	//lblPlayerSalary.Caption = ""$class'xUtil'.static.GetSalaryFor(PR);
	//GP = PlayerOwner().Level.Game.CurrentGameProfile;
	//lblTeamSalary.Caption = ""$class'xUtil'.static.GetTeamSalaryFor(GP);
	//lblTeamSalaryCap.Caption = ""$GP.SalaryCap;

	//if (int(lblTeamSalaryCap.Caption) > int(lblTeamSalary.Caption) + int(lblPlayerSalary.Caption)) {
	//	Log("SINGLEPLAYER affordable.  teamfull = "$IsTeamFull());
	UpdateDraftable (true);
	/*} else {
	//	Log("SINGLEPLAYER not affordable. ");
	//	UpdateDraftable(false);
	//}

	lblPlayerSalary.Caption = "Salary:"@lblPlayerSalary.Caption$"M";
	lblTeamSalary.Caption = "Team Salary:"@lblTeamSalary.Caption$"M";
	lblTeamSalaryCap.Caption = "Salary Cap:"@lblTeamSalaryCap.Caption$"M";
	*/

}

function CharListChange(GUIComponent Sender)
{
	Super.OnChange(Sender);
	if ( Sender == cltMyTeam )
	{
		if ( cltMyTeam.GetName() == "" )
		{
			cltMyTeam.Index = -1;
		}
		else
		{
			cltPortrait.Find(cltMyTeam.GetName());
			CharListUpdate(Sender);
		}
	}
}

function bool IsOnTeam(string botname)
{
	local int i;
	local GameProfile GP;

	if ( botname == "" )
	{
		return false;
	}

	GP = PlayerOwner().Level.Game.CurrentGameProfile;
	if ( GP != none )
	{
		for ( i=0; i<GP.TEAM_SIZE; i++ )
		{
			if ( GP.PlayerTeam[i] ~= botname )
			{
				return true;
			}
		}
	}
	return false;
}

function bool IsTeamFull()
{
	local GameProfile GP;
	local int i;

	GP = PlayerOwner().Level.Game.CurrentGameProfile;
	if ( GP.PlayerTeam.Length < GP.TEAM_SIZE )
	{
		return false;
	}

	for ( i=0; i<GP.TEAM_SIZE; i++ )
	{
		if ( GP.PlayerTeam[i] == "" )
		{
			return false;
		}
	}

	return true;
}

// set one of the draft/release buttons effective
// depends upon various salary labels being up-to-date
function UpdateDraftable(bool bAffordable)
{
	local bool bOnTeam;
	local bool bCanDraft;

	bCanDraft = bAffordable && !IsTeamFull();

	// check if he's on team already
	bOnTeam = IsOnTeam(cltPortrait.GetName());
	butDraft.bAcceptsInput = !bOnTeam && bCanDraft;
	butRelease.bAcceptsInput = bOnTeam;

	butDraft.bVisible = butDraft.bAcceptsInput;
	butRelease.bVisible = butRelease.bAcceptsInput;
/*
	if (butDraft.bAcceptsInput)
	{
		butDraft.StyleName=ButtonStyleEnabled;
	}
	else
	{
		butDraft.StyleName=ButtonStyleDisabled;
	}
	if (butRelease.bAcceptsInput)
	{
		butRelease.StyleName=ButtonStyleEnabled;
	}
	else
	{
		butRelease.StyleName=ButtonStyleDisabled;
	}
*/
}

function BuildStats ( out xUtil.PlayerRecord PR, GUIListBox charbox, GUIListBox teambox )
{
	local string str;
	local GameProfile GP;
	GP = PlayerOwner().Level.Game.CurrentGameProfile;

	charbox.List.Clear();
	str = PR.DefaultName;
	charbox.List.Add(str);
	str = class'xUtil'.static.GetFavoriteWeaponFor(PR);
	charbox.List.Add(str);
	str = class'xUtil'.Default.AccuracyString@class'xUtil'.static.AccuracyRating(PR);
	charbox.List.Add(str);
	str = class'xUtil'.Default.AggressivenessString@class'xUtil'.static.AggressivenessRating(PR);
	charbox.List.Add(str);
	str = class'xUtil'.Default.AgilityString@class'xUtil'.static.AgilityRating(PR);
	charbox.List.Add(str);
	str = class'xUtil'.Default.TacticsString@class'xUtil'.static.TacticsRating(PR);
	charbox.List.Add(str);

	// team stats
	teambox.List.Clear();
	str = GP.TeamName@StatsMessage;
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

// automatically fills the remainder of the team with random selections
function AutoFillTeam()
{
	local int nextDraft;
	local bool oldbPlaySounds;
	local GameProfile GP;
	local int listsize, i, listoffset;

	GP = PlayerOwner().Level.Game.CurrentGameProfile;
	if ( GP == none ) // pretty unlikely
		return;

	oldbPlaySounds = bPlaySounds;
	listsize = cltPortrait.PlayerList.Length;
	for ( nextDraft = 0; nextDraft < 7; nextDraft++ )
	{
		if ( GP.PlayerTeam[nextDraft] != "" )
			continue;

		// this is slow but it keeps the list happy
		listoffset = cltPortrait.Index + Rand(listsize-1);
		listoffset = listoffset % listsize;
		for ( i=0; i<listoffset; i++)
			cltPortrait.ScrollRight();

		while ( IsOnTeam(cltPortrait.GetName()) )
			cltPortrait.ScrollRight();

		GP.PlayerTeam[nextDraft] = cltPortrait.GetName();
	}
	bPlaySounds = oldbPlaySounds;

}

function bool ButtonClick(GUIComponent Sender)
{
	local GameProfile GP;
	local GUIQuestionPage Page;
	local bool oldbPlaySounds;

	GP = PlayerOwner().Level.Game.CurrentGameProfile;
	oldbPlaySounds = bPlaySounds;  // changed temporarily, only by the draft and release buttons

	if ( Sender==butDraft )
	{
		// should only be called if player is affordable and not on team
		bPlaySounds = false;
		GP.AddTeammate( cltPortrait.GetName() );
	}
	else if ( Sender==butRelease )
	{
		bPlaySounds = false;
		GP.ReleaseTeammate( cltPortrait.GetName() );
		cltMyTeam.Index = -1;
	}
	else if ( Sender==butClear )
	{
		// check if they're certain they want to clear their entire roster
		if (Controller.OpenMenu("XInterface.GUIQuestionPage"))
		{
			Page = GUIQuestionPage(Controller.ActivePage);
			Page.SetupQuestion(ClearConfirmMessage, QBTN_YesNo, QBTN_No);
			Page.OnButtonClick = ClearConfirm;
		}
		return true;
	}
	else if ( Sender==butEnter )
	{
		// check if they're certain they want to play with this team
		if (Controller.OpenMenu("XInterface.GUIQuestionPage"))
		{
			Page = GUIQuestionPage(Controller.ActivePage);
			Page.SetupQuestion(EnterConfirmMessage, QBTN_YesNo, QBTN_No);
			Page.OnButtonClick = EnterConfirm;
		}
		return true;
	}
	else if ( Sender==butLeft )
	{
		cltPortrait.ScrollLeft();
	}
	else if ( Sender==butRight )
	{
		cltPortrait.ScrollRight();
	}
	else if ( Sender==butAuto )
	{
		AutoFillTeam();
	}

	FinishButtonClick();
	bPlaySounds = oldbPlaySounds;

	return true;
}

event ChangeHint(string NewHint)
{
	MyHintBar.SetCaption(NewHint);
}


// broken out of ButtonClick for the ClearConfirm function
function FinishButtonClick()
{
	butEnter.bVisible = IsTeamFull();
	butEnter.bAcceptsInput = IsTeamFull();

	ReloadPortraits();
	CharListUpdate(cltPortrait);
}

// called when the clear-roster dialog pops up
function ClearConfirm(byte bButton)
{
	if (bButton == QBTN_Yes)
	{
		PlayerOwner().Level.Game.CurrentGameProfile.ClearTeammates();
		FinishButtonClick();
	}
}

// called when a button on the 'enter tourney' dialog is pressed
function EnterConfirm(byte bButton)
{
	if (bButton == QBTN_Yes)
	{
		PlayerOwner().Level.Game.SavePackage(PlayerOwner().Level.Game.CurrentGameProfile.PackageName);
		Controller.CloseMenu();
	}
}


defaultproperties
{
	// box to go around selected-character information
	Begin Object class=GUIImage Name=SPDTRosterBK0
		WinWidth=0.377617
		WinHeight=0.598438
		WinLeft=0.586875
		WinTop=0.130833
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object

	// player's team roster
	Begin Object class=GUICharacterListTeam Name=SPDTRosterCharList
		Hint="Choose a teammate to play in the next match"
		WinWidth=0.670315
		WinHeight=0.170000
		WinLeft=0.004688
		WinTop=0.750000
		StyleName="CharButton"
		bFillBounds=true
		FixedItemsPerPage=7
		bLocked=true
		bAllowSelectEmpty=false
		DefaultPortrait=Material'InterfaceContent.pEmptySlot'
		//bIgnoreBackClick=false
	End Object

	// match description
	Begin Object class=GUILabel Name=SPDTMatchData
		Caption="No Game Profile => No MatchData"
		TextALign=TXTA_Center
		TextFont="UT2LargeFont"
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.800000
		WinHeight=0.100000
		WinLeft=0.10
		WinTop=-0.12
	End Object

	// char data decotext
	Begin Object class=GUIScrollTextBox Name=SPDTCharData
		Hint="Team members profile"
		TextAlign=TXTA_Left
		WinWidth=0.520000
		WinHeight=0.359687
		WinLeft=0.024063
		WinTop=0.129167
		CharDelay=0.04
		EOLDelay=0.25
		bNeverFocus=true
		bAcceptsInput=true
	End Object

	// char stats
	Begin Object class=GUIListBox Name=SPDTCharStats
		WinWidth=0.520000
		WinHeight=0.237813
		WinLeft=0.024063
		WinTop=0.491249
		bNeverFocus=true
		bAcceptsInput=false
	End Object

	// border to go around char selector
	Begin Object class=GUIImage Name=SPDTCharListBox
		WinWidth=0.639846
		WinHeight=0.185625
		WinLeft=0.007813
		WinTop=0.741667
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object

	// portrait of the character to consider
	Begin Object class=GUICharacterListTeam Name=SPDTPortrait
		StyleName="CharButton"
		Hint="Select teammate"
		WinWidth=0.133300
		WinHeight=0.400000
		WinLeft=0.687500
		WinTop=0.190313
		bFillBounds=true
		FixedItemsPerPage=1
		DefaultPortrait=Material'InterfaceContent.pEmptySlot'
	End Object

	Begin Object Class=GUIButton Name=SPDTDraft
		Caption="DRAFT"
		Hint="Add this character to your team"
		WinWidth=0.132812
		WinHeight=0.055
		WinLeft=0.620626
		WinTop=0.602917
		bFocusOnWatch=true
	End Object
	Begin Object Class=GUIButton Name=SPDTRelease
		Caption="RELEASE"
		Hint="Remove this character from your team"
		WinWidth=0.132812
		WinHeight=0.055
		WinLeft=0.804998
		WinTop=0.602917
		bFocusOnWatch=true
	End Object

	Begin Object Class=GUIButton Name=SPDTClear
		Caption="CLEAR"
		Hint="Clear your team roster"
		StyleName="SquareMenuButton"
		WinWidth=0.12
		WinHeight=0.055
		WinLeft=0
		WinTop=0.93
		bFocusOnWatch=true
	End Object
	Begin Object Class=GUIButton Name=SPDTEnter
		Caption="PLAY"
		Hint="Enter tournament with this team"
		StyleName="SquareMenuButton"
		WinWidth=0.12
		WinHeight=0.055
		WinLeft=0.88
		WinTop=0.93
		bFocusOnWatch=true
	End Object

	Begin Object Class=GUIGfxButton Name=SPDTPicLeft
		Graphic=Material'InterfaceContent.Menu.fbArrowLeft'
		Hint="Select teammate"
		WinWidth=0.080000
		WinHeight=0.080000
		WinLeft=0.601563
		WinTop=0.335833
		bFocusOnWatch=true
	    Position=ICP_Scaled
		End Object
	Begin Object Class=GUIGfxButton Name=SPDTPicRight
		Graphic=Material'InterfaceContent.Menu.fbArrowRight'
		Hint="Select teammate"
		WinWidth=0.080000
		WinHeight=0.080000
		WinLeft=0.870312
		WinTop=0.335833
	    Position=ICP_Scaled
		bFocusOnWatch=true
	End Object

	// team stats
	Begin Object class=GUIListBox Name=SPDTTeamStats
		WinWidth=0.338750
		WinHeight=0.189062
		WinLeft=0.654063
		WinTop=0.739166
		bNeverFocus=true
		bAcceptsInput=false
	End Object

	Begin Object class=GUILabel Name=SPDTLblChoose
		Caption="Choose Teammate"
		TextALign=TXTA_Center
		TextFont="UT2HeaderFont"
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.368750
		WinHeight=0.079687
		WinLeft=0.592498
		WinTop=0.122084
	End Object

	Begin Object class=GUITitleBar name=SPDTHeader
		Caption="Single Player | Draft your team"
		StyleName="Header"
		WinWidth=1
		WinHeight=46.000000
		WinLeft=0
		WinTop=0.036406
		Effect=material'CO_Final'
	End Object

	Begin Object class=GUITitleBar name=SPDTHints
		WinWidth=0.76
		WinHeight=0.055
		WinLeft=0.12
		WinTop=0.93
		bUseTextHeight=false
		StyleName="Footer"
		Justification=TXTA_Center
	End Object

	Begin Object Class=GUIButton Name=SPDTAuto
		Caption="AUTO FILL"
		Hint="Automatically fill your team"
		WinWidth=0.187500
		WinHeight=0.055000
		WinLeft=0.681245
		WinTop=0.666667
		bFocusOnWatch=true
	End Object


	Controls(0)=SPDTRosterBK0
	Controls(1)=SPDTMatchData
	Controls(2)=SPDTCharData
	Controls(3)=SPDTCharStats
	Controls(4)=SPDTRosterCharList
	Controls(5)=SPDTCharListBox
	Controls(6)=SPDTPortrait
	Controls(7)=SPDTDraft
	Controls(8)=SPDTRelease
	Controls(9)=SPDTClear
	Controls(10)=SPDTEnter
	Controls(11)=SPDTPicLeft
	Controls(12)=SPDTPicRight
	Controls(13)=SPDTLblChoose
	Controls(14)=SPDTTeamStats
	Controls(15)=SPDTHeader
	Controls(16)=SPDTHints
	Controls(17)=SPDTAuto

	ButtonStyleEnabled="RoundButton"
	ButtonStyleDisabled="NoBackground"
	Background=Material'InterfaceContent.Backgrounds.bg10'
	WinTop=0.0
	WinLeft=0
	WinWidth=1
	WinHeight=1.0
	bAcceptsInput=false

	ClearConfirmMessage="This action will empty your current roster.  Are you sure?"
	EnterConfirmMessage="Are you ready to enter the tournament?"
	StatsMessage="Stats"
	bPlaySounds=true
}
