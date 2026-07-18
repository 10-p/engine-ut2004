// ====================================================================
//  Single player menu for establishing profile.
//  author:  capps 8/26/02
// ====================================================================

///////////
// TODO
// draw team picture in blue

class Tab_SPProfileNew extends Tab_SPPanelBase;

var GUIEditBox ePlayerName;
var GUIEditBox eTeamName;
var GUIComboBox cDifficulty;
var GUICharacterListTeam clPlayerSkins;
var GUIImageList iTeamSymbols;

var	localized string DefaultName, DefaultTeamName;
var string DefaultCharacter, TeamSymbolPrefix;
var int DefaultTeamSymbol;
var int CurrentTeamSymbolNum;			// currently displayed team symbol number

var localized string Err_ProfileExists;
var localized string Err_CantCreateProfile;

const MINSYMBOL = 1;
const MAXSYMBOL = 22;

function InitComponent(GUIController pMyController, GUIComponent MyOwner)
{
local int i;

	ePlayerName = GUIEditBox(GUIMenuOption(Controls[2]).MyComponent);
	ePlayerName.MaxWidth=16;  // as per polge, check Tab_PlayerSettings if you change this
	ePlayerName.bConvertSpaces=true;
	ePlayerName.OnClick=PlayerOnClick;
	eTeamName = GUIEditBox(GUIMenuOption(Controls[3]).MyComponent);
	eTeamName.MaxWidth=16;
	eTeamName.OnClick=PlayerOnClick;
	cDifficulty = GUIComboBox(GUIMenuOption(Controls[4]).MyComponent);
	clPlayerSkins = GUICharacterListTeam(Controls[6]);

	iTeamSymbols = GUIImageList(Controls[10]);

	Super.Initcomponent(pMyController, MyOwner);

	if (!class'Tab_PlayerSettings'.default.bUnlocked)
		clPlayerSkins.InitListExclusive("UNLOCK", "DUP");
	else
		clPlayerSkins.InitListExclusive("DUP");

	Controls[5].FocusInstead = clPlayerSkins;
	Controls[7].FocusInstead = clPlayerSkins;
	Controls[8].FocusInstead = clPlayerSkins;
	Controls[9].FocusInstead = iTeamSymbols;
	Controls[11].FocusInstead = iTeamSymbols;
	Controls[12].FocusInstead = iTeamSymbols;

	for (i=0; i<class'Tab_SPProfileLoad'.default.NumDifficulties; i++)
		cDifficulty.AddItem(class'Tab_SPProfileLoad'.default.Difficulties[i]);

	cDifficulty.ReadOnly(true);

	LoadSymbols();	// Symbols are loaded once only.
	ClearStats();

}

function InitPanel()
{
	Super.InitPanel();
	MyButton.Hint = class'UT2SinglePlayerMain'.default.TabHintProfileNew;
	UT2SinglePlayerMain(MyButton.MenuOwner.MenuOwner).ChangeHint(MyButton.Hint);
	UT2SinglePlayerMain(MyButton.MenuOwner.MenuOwner).ResetTitleBar(MyButton);
}

function bool PlayerOnClick(GUIComponent Sender) {
	if ( Sender == ePlayerName )
	{
		if ( ePlayerName.TextStr == DefaultName )
			ePlayerName.TextStr = "";
	}
	else if ( Sender == eTeamName )
	{
		if ( eTeamName.TextStr == DefaultTeamName )
			eTeamName.TextStr = "";
	}
	return Super.OnClick(Sender);
}

function string GetTeamSymbolName(int idx)
{
	return TeamSymbolPrefix$Right("0"$idx, 2);
}

function string CurrentTeamSymbolName()
{
	return String(iTeamSymbols.Image);
}

function LoadSymbols()
{
local int i;
local Material M;
local array<string> SymbolNames;

	Controller.GetTeamSymbolList(SymbolNames, true);  // get all usable team symbols

	for (i = 0; i < SymbolNames.Length; i++)
	{
		M = Material(DynamicLoadObject(SymbolNames[i], class'Material'));
		iTeamSymbols.AddMaterial(String(i), M);
	}
}

// update all the various boxes to show the stats of this profile
function ClearStats()
{
	// update name
	ePlayerName.TextStr = DefaultName;

	// update team name
	eTeamName.TextStr = DefaultTeamName;

	// update difficulty
	cDifficulty.SetText(class'xInterface.Tab_SPProfileLoad'.default.Difficulties[1]);

	// update character portrait

	clPlayerSkins.Find(DefaultCharacter);

	// update team symbol
	iTeamSymbols.FirstImage();
}

function bool CreateClick(GUIComponent Sender)
{
local class<GameProfile> profileclass;
local string profilename;
local GameProfile GP;
local GUIQuestionPage Page;
local array<string> profilenames;
local int i;
local bool bFileExists;

	if ( ePlayerName.TextStr == "" )
		ePlayerName.TextStr = DefaultName;

	if ( eTeamName.TextStr == "" )
		eTeamName.TextStr = DefaultTeamName;

	profilename = ePlayerName.TextStr;
	profileclass = class<GameProfile>(DynamicLoadObject("xGame.UT2003GameProfile", class'Class'));

	// test if a profile with this name already exists
	GP = PlayerOwner().Level.Game.LoadDataObject(profileclass, "GameProfile", profilename);

	if ( GP != none )
	{
		GP = none;

		// it may just still be lying around in memory, un GC'd, so test if there's a file there
		bFileExists=false;
		Controller.GetProfileList("",profilenames);
		for ( i=0; i<profilenames.Length; i++ )
		{
			if ( profilenames[i] ~= profilename )
			{
				bFileExists=true;
				break;
			}
		}

		// there's still a file there, too, so just exit with warning.
		if ( bFileExists )
		{
			// profile already exists, warn player
			if (Controller.OpenMenu("XInterface.GUIQuestionPage"))
			{
				Page = GUIQuestionPage(Controller.ActivePage);
				Page.SetupQuestion(Page.Replace(err_ProfileExists, "prof", Caps(profilename)), QBTN_Ok, QBTN_Ok);
			}
			return true;
		}
	}

	GP = PlayerOwner().Level.Game.CreateDataObject(profileclass, "GameProfile", profilename);
	if ( GP != none )
	{
		GP.PlayerName = ePlayerName.TextStr;
		GP.TeamName = eTeamName.TextStr;
		GP.TeamSymbolName = String(iTeamSymbols.Image);
		GP.PlayerCharacter = clPlayerSkins.GetName();
		GP.BaseDifficulty = cDifficulty.Index;
		PlayerOwner().Level.Game.CurrentGameProfile = GP;
		GP.Initialize(PlayerOwner().Level.Game, profilename);
		PlayerOwner().Level.Game.CurrentGameProfile.bInLadderGame=true;  // so it'll reload into SP menus
		if (!PlayerOwner().Level.Game.SavePackage(profilename))
		{
			Log("SINGLEPLAYER couldn't save profile package!");
		}

		GUITabControl(MyButton.MenuOwner).ReplaceTab(MyButton, class'UT2SinglePlayerMain'.default.TabNameProfileLoad, "xInterface.Tab_SPProfileLoad", , , true);
		GUITabControl(MyButton.MenuOwner).ActivateTabByName(class'UT2SinglePlayerMain'.default.TabNameQualification, true);

		// Broadcast about the new Profile
		ProfileUpdated();

		// launch the game introduction
		PlayerOwner().ConsoleCommand("START ut2-intro.ut2?quickstart=true?TeamScreen=False?savegame="$profilename);
		Controller.CloseAll(false);
	}
	else
	{
		// couldn't create profile, give warning
		if (Controller.OpenMenu("XInterface.GUIQuestionPage"))
		{
			Page = GUIQuestionPage(Controller.ActivePage);
			Page.SetupQuestion(Err_CantCreateProfile, QBTN_Ok, QBTN_Ok);
		}
		return true;
	}
	return true;
}

function bool PrevSkin(GUIComponent Sender)
{
	clPlayerSkins.ScrollLeft();
	return true;
}

function bool NextSkin(GUIComponent Sender)
{
	clPlayerSkins.ScrollRight();
	return true;
}

function bool PrevSymbol(GUIComponent Sender)
{
	iTeamSymbols.PrevImage();
	return true;
}

function bool NextSymbol(GUIComponent Sender)
{
	iTeamSymbols.NextImage();
	return true;
}

function bool BackClick(GUIComponent Sender)
{
	local array<string> profilenames;
	// if they click back and there are no existing profiles, take them to main by closing the SP menu
	Controller.GetProfileList("",profilenames);
	if ( profilenames.Length == 0 )
		Controller.CloseMenu();

	GUITabControl(MyButton.MenuOwner).ReplaceTab(MyButton, class'UT2SinglePlayerMain'.default.TabNameProfileLoad, "xInterface.Tab_SPProfileLoad", , , true);
	return true;
}

function bool DefaultsClick(GUIComponent Sender)
{
	ClearStats();
	return true;
}

defaultproperties
{
	Begin Object Class=GUIImage Name=imgEditsBack
		Image=Material'InterfaceContent.Menu.EditBox'
		WinWidth=0.444304
		WinHeight=0.77
		WinLeft=0.504375
		WinTop=0.073
		ImageStyle=ISTY_Stretched
		bNeverFocus=true
		bAcceptsInput=false
	End Object

	// EditBox with Limited CharSet for Player Name
	Begin Object class=GUIEditBox Name=ebNameEdit
		TextStr="PlayerName"
		AllowedCharSet="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890"
	End Object

	Begin Object class=moEditBox Name=moebPlayerName
		Caption="Player Name: "
		Hint="Your character's name"
		LabelJustification=TXTA_Left
		LabelFont="UT2SmallFont"
		LabelColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.345
		WinHeight=0.122500
		WinLeft=0.553125
		WinTop=0.286087
		MyComponent=ebNameEdit
		bVerticalLayout=true
	End Object

	// EditBox with Limited CharSet for Team Name
	Begin Object class=GUIEditBox Name=ebTeamEdit
		TextStr="TeamName"
		AllowedCharSet="abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890"
	End Object

	Begin Object class=moEditBox Name=moebTeamName
		Caption="Team Name: "
		Hint="The name of your team"
		LabelJustification=TXTA_Left
		LabelFont="UT2SmallFont"
		LabelColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.345
		WinHeight=0.122500
		WinLeft=0.553125
		WinTop=0.428007
		MyComponent=ebTeamEdit
		bVerticalLayout=true
	End Object

	// ComboBox for use by
	Begin Object class=GUIComboBox Name=cbDifficulty
		bReadOnly=true
	End Object

	Begin Object class=moComboBox Name=mocbDifficulty
		Caption="Difficulty: "
		Hint="Customize your challenge"
		LabelJustification=TXTA_Left
		LabelFont="UT2SmallFont"
		LabelColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.345
		WinHeight=0.122500
		WinLeft=0.553125
		WinTop=0.568803
		MyComponent=cbDifficulty
		bVerticalLayout=true
	End Object

	// portrait of the selected character
	Begin Object class=GUICharacterListTeam Name=clistPlayerSkins
		Hint="Your character's appearance, use arrow keys to change"
		WinWidth=0.124694
		WinHeight=0.500000
		WinLeft=0.083462
		WinTop=0.117917
		bCenterInBounds=true
		FixedItemsPerPage=1
		StyleName="CharButton"
	End Object

	// team symbol
	Begin Object class=GUIImageList Name=ilTeamSymbols
		Hint="Your team's symbol; use arrow keys to change"
		WinWidth=0.167212
		WinHeight=0.500000
		WinLeft=0.303972
		WinTop=0.137175
		bAcceptsInput=true
		bNeverFocus=false
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Justified
		ImageAlign=IMGA_Center
		bWrap=true
	End Object

	// The Back Button
	Begin Object Class=GUIButton Name=btnBack
		Caption="BACK"
		Hint="Return to previous menu"
		OnClick=BackClick
		WinWidth=0.200000
		WinHeight=0.075000
		WinLeft=0.028125
		WinTop=0.925
	End Object

	// The Defaults Button (Reset ?)
	Begin Object Class=GUIButton Name=btnDefaults
		Caption="SET TO DEFAULTS"
		Hint="Set this profile back to default values"
		OnClick=DefaultsClick
		WinWidth=0.232813
		WinHeight=0.075000
		WinLeft=0.367500
		WinTop=0.925
	End Object

	Begin Object Class=GUIButton Name=btnSaveProfile
		Caption="CREATE PROFILE"
		Hint="Create a profile with these settings"
		OnClick=CreateClick
		WinWidth=0.223438
		WinHeight=0.075000
		WinLeft=0.744060
		WinTop=0.925
	End Object

	Begin Object Class=GUIImage Name=imgSkinsBack
		Image=Material'InterfaceContent.Menu.EditBox'
		WinWidth=0.215
		WinHeight=0.77
		WinLeft=0.055938
		WinTop=0.073
		ImageStyle=ISTY_Stretched
		bNeverFocus=true
		bAcceptsInput=false
	End Object

	Begin Object Class=GUIGfxButton Name=btnPrevSkin
		Hint="Selects a new appearance for your character"
		Graphic=Material'InterfaceContent.Menu.fbArrowLeft'
		OnClick=PrevSkin
		WinWidth=0.080000
		WinHeight=0.080000
		WinLeft=0.080000
		WinTop=0.640000
		bNeverFocus=true
	    Position=ICP_Scaled
	End Object

	Begin Object Class=GUIGfxButton Name=btnNextSkin
		Hint="Selects a new appearance for your character"
		Graphic=Material'InterfaceContent.Menu.fbArrowRight'
		OnClick=NextSkin
		WinWidth=0.080000
		WinHeight=0.080000
		WinLeft=0.172187
		WinTop=0.640000
	    Position=ICP_Scaled
		bNeverFocus=true
	End Object

	Begin Object Class=GUIImage Name=imgSymbolsBack
		Image=Material'InterfaceContent.Menu.EditBox'
		WinWidth=0.215000
		WinHeight=0.770000
		WinLeft=0.280243
		WinTop=0.073000
		ImageStyle=ISTY_Stretched
		bNeverFocus=true
	End Object

	Begin Object Class=GUIGfxButton Name=btnPrevSymbol
		Hint="Selects a new symbol for your team"
		Graphic=Material'InterfaceContent.Menu.fbArrowLeft'
		OnClick=PrevSymbol
		WinWidth=0.080000
		WinHeight=0.080000
		WinLeft=0.298750
		WinTop=0.640000
		bNeverFocus=true
	    Position=ICP_Scaled
	End Object

	Begin Object Class=GUIGfxButton Name=btnNextSymbol
		Hint="Selects a new symbol for your team"
		Graphic=Material'InterfaceContent.Menu.fbArrowRight'
		OnClick=NextSymbol
		WinWidth=0.080000
		WinHeight=0.080000
		WinLeft=0.389375
		WinTop=0.640000
		bNeverFocus=true
	    Position=ICP_Scaled
	End Object

	Begin Object class=GUILabel Name=lblTeamSymbol
		Caption="Select|Team Symbol"
		TextALign=TXTA_Center
		TextFont="UT2SmallFont"
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.200000
		WinHeight=0.100000
		WinLeft=0.284375
		WinTop=0.724583
		bMultiLine=true
	End Object

	Begin Object class=GUILabel Name=lblCharacter
		Caption="Select|Character"
		TextALign=TXTA_Center
		TextFont="UT2SmallFont"
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.200000
		WinHeight=0.100000
		WinLeft=0.064062
		WinTop=0.724583
		bMultiLine=true
	End Object

	// cool border for the player portrait symbol
	Begin Object class=GUIImage Name=portraitBackground
		Image=Material'InterfaceContent.Menu.BorderBoxA1'
		WinWidth=0.146680
		WinHeight=0.506094
		WinLeft=0.094141
		WinTop=0.110469
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Stretched
		bVisible=false
	End Object

	// cool border for the team symbol
	Begin Object class=GUIImage Name=symbolBackground
		WinWidth=0.179101
		WinHeight=0.297265
		WinLeft=0.296196
		WinTop=0.237865
		Image=Material'InterfaceContent.Menu.BorderBoxA1'
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Stretched
	End Object

	Controls(0)=btnSaveProfile
	Controls(1)=imgEditsBack
	Controls(2)=moebPlayerName
	Controls(3)=moebTeamName
	Controls(4)=mocbDifficulty
	Controls(5)=imgSkinsBack
	Controls(6)=clistPlayerSkins
	Controls(7)=btnPrevSkin
	Controls(8)=btnNextSkin
	Controls(9)=imgSymbolsBack
	Controls(10)=ilTeamSymbols
	Controls(11)=btnPrevSymbol
	Controls(12)=btnNextSymbol
	Controls(13)=btnBack
	Controls(14)=btnDefaults
	Controls(15)=lblCharacter
	Controls(16)=lblTeamSymbol
	Controls(17)=symbolBackground
	Controls(18)=portraitBackground

	DefaultName="Player"
	DefaultTeamName="Team"
	DefaultTeamSymbol=1
	TeamSymbolPrefix="TeamSymbols_UT2003.sym"
	DefaultCharacter="Gorge"

	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.77
	bAcceptsInput=false

	Err_ProfileExists="Profile with name %prof% already exists!"
	Err_CantCreateProfile="Profile creation failed."
}
