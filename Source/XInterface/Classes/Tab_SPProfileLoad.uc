// ====================================================================
//  Single player menu for loading profile.
//  author:  capps 8/26/02
// ====================================================================

class Tab_SPProfileLoad extends Tab_SPPanelBase;

var GUIListBox		lbProfiles;
var moEditBox		ebPlayerName, ebTeamName, ebDifficulty;
var moEditBox		ebKills, ebDeaths, ebGoals, ebWins, ebMatches;
var GUIImage		imgPlayerSkin, imgTeamSymbol;
var GUIButton		butNew;

var localized string Difficulties[8];  // hardcoded for localization.  clamped 0,NumDifficulties where accessed.
var int NumDifficulties;			   // for external class access to size

var localized string DeleteMessage;

function InitComponent(GUIController pMyController, GUIComponent MyOwner)
{
local GameProfile GP;

	Super.Initcomponent(pMyController, MyOwner);

	butNew=GUIButton(Controls[0]);
	lbProfiles=GUIListBox(Controls[1]);
	lbProfiles.List.OnDblClick=DoubleClickList;
	ebPlayerName=moEditBox(Controls[2]);
	ebTeamName=moEditBox(Controls[3]);
	ebDifficulty=moEditBox(Controls[4]);
	ebKills=moEditBox(Controls[5]);
	ebDeaths=moEditBox(Controls[6]);
	ebGoals=moEditBox(Controls[7]);
	imgPlayerSkin=GUIImage(Controls[8]);
	imgTeamSymbol=GUIImage(Controls[10]);
	ebWins=moEditBox(Controls[18]);
	ebMatches=moEditBox(Controls[19]);

	// if there exists a profile currently, set it to that one by default
	GP = GetProfile();
	if ( GP != none )
	{
		lbProfiles.List.Find(GP.PackageName);
		// MC: Then tell all pages that a profile is present
		ProfileUpdated();
		// Check if a Button was stored in GP and use it to focus entry page.
		if (LadderButton(GP.NextMatchObject) != None)
		{
			if (LadderButton(GP.NextMatchObject).LadderIndex > 0)
				MyTabControl().ActivateTabByName(class'UT2SinglePlayerMain'.default.TabNameLadder, true);
			else
				MyTabControl().ActivateTabByName(class'UT2SinglePlayerMain'.default.TabNameQualification, true);
		}
	}

	UpdateList();
}

function InitPanel()
{
	Super.InitPanel();
	MyButton.Hint = class'UT2SinglePlayerMain'.default.TabHintProfileLoad;
	UT2SinglePlayerMain(MyButton.MenuOwner.MenuOwner).ResetTitleBar(MyButton);
}

function OnProfileUpdated()
{
	UpdateList();
}

// update the profile listing by rechecking disk, needed after create or delete
function UpdateList()
{
local array<string> profilenames;
local int i;
local GameProfile GP;

	Controller.GetProfileList("",profilenames);
	lbProfiles.List.Clear();

	for ( i=0; i<profilenames.Length; i++ )
	{
		lbProfiles.List.Add(profilenames[i]);
	}

	GP = GetProfile();
	if ( GP != none )
		lbProfiles.List.Find(GP.PackageName);

	UpdateStats();
}

// update all the various boxes to show the stats of this profile
function UpdateStats()
{
	local string profilename;
	local GameProfile GP;
	local xUtil.PlayerRecord PR;

	// get current profile from list
	profilename = lbProfiles.List.Get();
	if ( profilename == "" )
	{
		// reset name, team, difficulty
		ebPlayerName.SetText("");
		ebTeamName.SetText("");
		ebDifficulty.SetText(Difficulties[2]);
		// reset character portrait and team symbol   was PlayerPictures.cDefault
		imgPlayerSkin.Image = Material(DynamicLoadObject("InterfaceContent.pEmptySlot", class'Material'));
		imgTeamSymbol.Image = none;
		// reset kills, death, goals
		ebKills.SetText("");
		ebDeaths.SetText("");
		ebGoals.SetText("");
		ebWins.SetText("");
		ebMatches.SetText("");
		return;
	}

	GP = PlayerOwner().Level.Game.LoadDataObject(class'GameProfile', "GameProfile", profilename);

	if ( GP == none ) {
		return;
	}

	PR = class'xGame.xUtil'.static.FindPlayerRecord(GP.PlayerCharacter);

	ebPlayerName.SetText(GP.PlayerName);
	ebTeamName.SetText(GP.TeamName);
	ebDifficulty.SetText(Difficulties[Clamp(int(GP.BaseDifficulty), 0, NumDifficulties)]);
	// reset character portrait and team symbol
	imgPlayerSkin.Image = PR.Portrait;
	imgTeamSymbol.Image = Material(DynamicLoadObject(GP.TeamSymbolName, class'Material'));
	// reset kills, death, goals
	ebKills.SetText(String(GP.Kills));
	ebDeaths.SetText(String(GP.Deaths));
	ebGoals.SetText(String(GP.Goals));
	ebWins.SetText(String(GP.Wins));
	ebMatches.SetText(String(GP.Matches));
}

////////////////////////////////////////////
// get current profile from list, load it, and apply it
//
function bool LoadClick(GUIComponent Sender)
{
local class<GameProfile> profileclass;
local string profilename;
local GameProfile GP;

	profilename = lbProfiles.List.Get();
	profileclass = class<GameProfile>(DynamicLoadObject("xGame.UT2003GameProfile", class'Class'));
	GP = PlayerOwner().Level.Game.LoadDataObject(profileclass, "GameProfile", profilename);
	if ( GP != none )
	{
		PlayerOwner().Level.Game.CurrentGameProfile = GP;
		GP.Initialize(PlayerOwner().Level.Game, profilename);
		if (GP.LadderRung[1] >= 0) { // player has unlocked main ladder
			MyTabControl().ActivateTabByName(class'UT2SinglePlayerMain'.default.TabNameLadder, true);
		} else {
			MyTabControl().ActivateTabByName(class'UT2SinglePlayerMain'.default.TabNameQualification, true);
		}

		ProfileUpdated();
	}
	return true;
}

function DeleteConfirm(byte bButton)
{
local string profilename;

	if (bButton == QBTN_Yes)
	{
		profilename = lbProfiles.List.Get();
		if ( GetProfile() != none && GetProfile().PackageName == profilename)
		{
			PlayerOwner().Level.Game.CurrentGameProfile = none;
		}
		if (!PlayerOwner().Level.Game.DeletePackage(profilename))
		{
			Log("SINGLEPLAYER Tab_SPProfileLoad::ButtonClick() failed to delete GameProfile "$profilename);
		}
		ProfileUpdated();
		UpdateList();
	}
}

////////////////////////////////////////////
// Delete the selected profile
//
function bool DeleteClick(GUIComponent Sender)
{
local GUIQuestionPage Page;
local string profilename;

	if (Controller.OpenMenu("XInterface.GUIQuestionPage"))
	{
		profilename = lbProfiles.List.Get();
		Page=GUIQuestionPage(Controller.TopPage());
		Page.SetupQuestion(Page.Replace(DeleteMessage, "prof", Caps(profilename)), QBTN_YesNo, QBTN_No);
		Page.OnButtonClick = DeleteConfirm;
	}
	return true;
}

//////////////////////////////////////////////////
// Activate the New Profile MenuPage.
//
function bool NewProfileClick(GUIComponent Sender)
{
	GUITabControl(MyButton.MenuOwner).ReplaceTab(MyButton, class'UT2SinglePlayerMain'.default.TabNameProfileNew, "xInterface.Tab_SPProfileNew", , , true);
	return true;
}

function InternalOnChange(GUIComponent Sender)
{
	UpdateStats();
}

function bool DoubleClickList(GUIComponent Sender)
{
	return LoadClick(Sender);
}

// when expose menu, might have come from the profile-create screen, so update the list
function ShowPanel(bool bShow) {
	Super.ShowPanel(bShow);
	if ( bShow ) {
		// just started game, no profiles exist, jump to create new
		if ( lbProfiles.List.ItemCount == 0 ) {
			UpdateStats();  // sets everything to empty
			NewProfileClick(butNew);
			return;
		}
	}
}

defaultproperties
{
	// List of loadable profiles
	Begin Object class=GUIListBox Name=lboxProfile
		Hint="Select a profile"
		WinWidth=0.264609
		WinHeight=0.610624
		WinLeft=0.330562
		WinTop=0.131667
		bAcceptsInput=true
		OnChange=InternalOnChange
	End Object

	Begin Object class=moEditBox Name=moebPlayerName
		Caption="Player Name: "
		Hint="Your character's name"
		LabelJustification=TXTA_Right
		LabelFont="UT2SmallFont"
		LabelColor=(R=255,G=255,B=255,A=255)
		CaptionWidth=0.55
		WinWidth=0.493750
		WinHeight=0.060000
		WinLeft=0.495360
		WinTop=0.143847
		bReadOnly=true
	End Object

	Begin Object class=moEditBox Name=moebTeamName
		Caption="Team Name: "
		Hint="The name of your team"
		LabelJustification=TXTA_Right
		LabelFont="UT2SmallFont"
		LabelColor=(R=255,G=255,B=255,A=255)
		CaptionWidth=0.55
		WinWidth=0.493750
		WinHeight=0.060000
		WinLeft=0.495360
		WinTop=0.212590
		bReadOnly=true
	End Object

	Begin Object class=moEditBox Name=moebDifficulty
		Caption="Difficulty: "
		Hint="Difficulty rating of this tournament"
		LabelJustification=TXTA_Right
		LabelFont="UT2SmallFont"
		LabelColor=(R=255,G=255,B=255,A=255)
		CaptionWidth=0.55
		WinWidth=0.493750
		WinHeight=0.060000
		WinLeft=0.495360
		WinTop=0.280038
		bReadOnly=true
	End Object

	Begin Object class=moEditBox Name=moebKills
		Caption="Kills: "
		Hint="Number of kills by this character"
		LabelJustification=TXTA_Right
		LabelFont="UT2SmallFont"
		LabelColor=(R=255,G=255,B=255,A=255)
		CaptionWidth=0.55
		WinWidth=0.493750
		WinHeight=0.060000
		WinLeft=0.495360
		WinTop=0.391236
		bReadOnly=true
	End Object

	Begin Object class=moEditBox Name=moebDeaths
		Caption="Deaths: "
		Hint="Number of times this character has died"
		LabelJustification=TXTA_Right
		LabelFont="UT2SmallFont"
		LabelColor=(R=255,G=255,B=255,A=255)
		CaptionWidth=0.55
		WinWidth=0.493750
		WinHeight=0.060000
		WinLeft=0.495360
		WinTop=0.458684
		bReadOnly=true
	End Object

	Begin Object class=moEditBox Name=moebGoals
		Caption="Goals: "
		Hint="Number of goals scored by this character"
		LabelJustification=TXTA_Right
		LabelFont="UT2SmallFont"
		LabelColor=(R=255,G=255,B=255,A=255)
		CaptionWidth=0.55
		WinWidth=0.493750
		WinHeight=0.060000
		WinLeft=0.495360
		WinTop=0.528632
		bReadOnly=true
	End Object

	// cool border for the portrait
	Begin Object class=GUIImage Name=imageSkinBack
		WinWidth=0.138300
		WinHeight=0.510000
		WinLeft=0.171195
		WinTop=0.158646
		Image=Material'InterfaceContent.Menu.BorderBoxA1'
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Stretched
	End Object

	// portrait of the selected character
	Begin Object class=GUIImage Name=imagePlayerSkin
		Hint="Your character's appearance"
		WinWidth=0.133300
		WinHeight=0.500000
		WinLeft=0.173148
		WinTop=0.162552
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Scaled
	End Object

	// team symbol
	Begin Object class=GUIImage Name=imageTeamSymbol
		Hint="Your team's symbol"
		WinWidth=0.150000
		WinHeight=0.277800
		WinLeft=0.016538
		WinTop=0.262500
		Image=Material'InterfaceContent.Menu.SimpleBorder_F'
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Scaled
	End Object

	Begin Object Class=GUIButton Name=btnDelete
		Caption="DELETE PROFILE"
		Hint="Delete the currently selected profile"
		OnClick=DeleteClick
		WinWidth=0.218750
		WinHeight=0.075000
		WinLeft=0.141250
		WinTop=0.925
	End Object

	Begin Object Class=GUIButton Name=btnLoad
		Caption="LOAD PROFILE"
		Hint="Load the selected profile"
		OnClick=LoadClick
		WinWidth=0.200000
		WinHeight=0.075000
		WinLeft=0.412500
		WinTop=0.925
	End Object

	Begin Object Class=GUIButton Name=btnNew
		Caption="NEW PROFILE"
		Hint="Go to the profile creation menu"
		OnClick=NewProfileClick
		WinWidth=0.200000
		WinHeight=0.075000
		WinLeft=0.661250
		WinTop=0.925
	End Object

	// border to go around profile data
	Begin Object class=GUIImage Name=SPPLDataBox
		WinWidth=0.393751
		WinHeight=0.613750
		WinLeft=0.601563
		WinTop=0.131250
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object

	// border to go around images
	Begin Object class=GUIImage Name=SPPLImageBox
		WinWidth=0.318752
		WinHeight=0.612187
		WinLeft=0.006251
		WinTop=0.131250
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object

	// PROFILE STATS label
	Begin Object class=GUILabel Name=SPPLLblStats
		Caption="PROFILE STATS"
		TextALign=TXTA_Center
		TextFont="UT2SmallFont"
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.400000
		WinHeight=0.100000
		WinLeft=0.592186
		WinTop=0.734586
	End Object
	// PROFILE STATS label
	Begin Object class=GUILabel Name=SPPLLblProfiles
		Caption="PROFILE LISTING"
		TextALign=TXTA_Center
		TextFont="UT2SmallFont"
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.400000
		WinHeight=0.100000
		WinLeft=0.264062
		WinTop=0.730417
	End Object

	// cool border for the team symbol
	Begin Object class=GUIImage Name=symbolBack
		WinWidth=0.149042
		WinHeight=0.273672
		WinLeft=0.015922
		WinTop=0.265417
		Image=Material'InterfaceContent.Menu.BorderBoxA1'
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Stretched
	End Object

	Begin Object class=moEditBox Name=moebWins
		Caption="Wins: "
		Hint="Number of matches won by this character"
		LabelJustification=TXTA_Right
		LabelFont="UT2SmallFont"
		LabelColor=(R=255,G=255,B=255,A=255)
		CaptionWidth=0.55
		WinWidth=0.493751
		WinHeight=0.060000
		WinLeft=0.495315
		WinTop=0.597135
		bReadOnly=true
	End Object
	Begin Object class=moEditBox Name=moebMatches
		Caption="Matches: "
		Hint="Number of matches played by this character"
		LabelJustification=TXTA_Right
		LabelFont="UT2SmallFont"
		LabelColor=(R=255,G=255,B=255,A=255)
		CaptionWidth=0.55
		WinWidth=0.493750
		WinHeight=0.060000
		WinLeft=0.495360
		WinTop=0.667852
		bReadOnly=true
	End Object

	Controls(0)=btnNew
	Controls(1)=lboxProfile
	Controls(2)=moebPlayerName
	Controls(3)=moebTeamName
	Controls(4)=moebDifficulty
	Controls(5)=moebKills
	Controls(6)=moebDeaths
	Controls(7)=moebGoals

	Controls(8)=imagePlayerSkin
	Controls(9)=imageSkinBack
	Controls(10)=imageTeamSymbol

	Controls(11)=btnDelete
	Controls(12)=btnLoad

	Controls(13)=SPPLDataBox
	Controls(14)=SPPLImageBox
	Controls(15)=SPPLLblStats
	Controls(16)=SPPLLblProfiles

	Controls(17)=symbolBack
	Controls(18)=moebWins
	Controls(19)=moebMatches

	//Background=Material'InterfaceContent.Backgrounds.bg10'
	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.77
	bAcceptsInput=false
	//bFillHeight=true		// get it to fill vertical space from tab top

	// The Bots Skills should be stored and accessible somewhere in Engine.
	Difficulties(0)="Novice"
	Difficulties(1)="Average"
	Difficulties(2)="Experienced"
	Difficulties(3)="Skilled"
	Difficulties(4)="Adept"
	Difficulties(5)="Masterful"
	Difficulties(6)="Inhuman"
	Difficulties(7)="Godlike"
	NumDifficulties=8
	DeleteMessage="You are about to delete the profile for '%prof%'||Are you sure you want to delete this profile?"
}
