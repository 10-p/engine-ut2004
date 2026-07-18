// ====================================================================
//  Class:  XInterface.Tab_SPTutorials
//  Parent: XInterface.Tab_SPPanelBase
//  Single player menu for calling up tutorials.
//  author:  capps 9/14/02  <-- great day to add a menu!
// ====================================================================

class Tab_SPTutorials extends Tab_SPPanelBase;

#exec OBJ LOAD File=Laddershots.utx

var array<GUIImage>		TutImages;		// gametype image
var array<GUIImage>		TutBorders;		// border for image
var array<GUILabel>		TutLabels;		// label for gametype
var array<GUIButton>    TutButtons;		// big button to overlay all that stuff

var localized string	SelectMessage;	// hint for selection

var config bool bTDMUnlocked, bDOMUnlocked, bCTFUnlocked, bBRUnlocked;  // whether these ladders have ever been unlocked

const DMIndex = 0;
const DOMIndex = 1;
const CTFIndex = 2;
const BRIndex = 3;

const NumButtons = 4;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local int i;

	Super.Initcomponent(MyController, MyOwner);

	for (i = 0; i<NumButtons; i++)
	{
		TutImages[i] = GUIImage(Controls[i*4]);
		TutBorders[i] = GUIImage(Controls[(i*4) + 1]);
		TutLabels[i] = GUILabel(Controls[(i*4) + 2]);
		TutButtons[i] = GUIButton(Controls[(i*4) + 3]);
	}

	TutLabels[DMIndex].Caption = class'xGame.xDeathMatch'.default.GameName;
	TutLabels[DOMIndex].Caption = class'xGame.xDoubleDom'.default.GameName;
	TutLabels[CTFIndex].Caption = class'xGame.xCTFGame'.default.GameName;
	TutLabels[BRIndex].Caption = class'xGame.xBombingRun'.default.GameName;

	for (i = 0; i<NumButtons; i++)
		TutButtons[i].Hint = SelectMessage@TutLabels[i].Caption;

}

// switches images with money images depending on unlock state
function ShowPanel(bool bShow)
{
	Super.ShowPanel(bShow);
	if (!bShow)
		return;

	bTDMUnlocked=default.bTDMUnlocked;
	bDOMUnlocked=default.bDOMUnlocked;
	bCTFUnlocked=default.bCTFUnlocked;
	bBRUnlocked=default.bBRUnlocked;

	TutImages[DMIndex].Image = Material(DynamicLoadObject("LadderShots.TeamDMShot", class'Material'));
	TutImages[DOMIndex].Image = Material(DynamicLoadObject("LadderShots.DOMShot", class'Material'));
	TutImages[CTFIndex].Image = Material(DynamicLoadObject("LadderShots.CTFShot", class'Material'));
	TutImages[BRIndex].Image = Material(DynamicLoadObject("LadderShots.BRShot", class'Material'));

	if ( class'Tab_PlayerSettings'.default.bUnlocked )
	{
		TutImages[DMIndex].Image = Material(DynamicLoadObject("LadderShots.TeamDMMoneyShot", class'Material'));
		TutImages[DOMIndex].Image = Material(DynamicLoadObject("LadderShots.DOMMoneyShot", class'Material'));
		TutImages[CTFIndex].Image = Material(DynamicLoadObject("LadderShots.CTFMoneyShot", class'Material'));
		TutImages[BRIndex].Image = Material(DynamicLoadObject("LadderShots.BRMoneyShot", class'Material'));
		return;
	}

	if ( bTDMUnlocked )
		TutImages[DMIndex].Image = Material(DynamicLoadObject("LadderShots.TeamDMMoneyShot", class'Material'));

	if ( bDOMUnlocked )
		TutImages[DOMIndex].Image = Material(DynamicLoadObject("LadderShots.DOMMoneyShot", class'Material'));

	if ( bCTFUnlocked )
		TutImages[CTFIndex].Image = Material(DynamicLoadObject("LadderShots.CTFMoneyShot", class'Material'));

	if ( bBRUnlocked )
		TutImages[BRIndex].Image = Material(DynamicLoadObject("LadderShots.BRMoneyShot", class'Material'));

}


function bool InternalOnClick(GUIComponent Sender)
{
	local string TutorialName;

	if (Sender == TutButtons[DMIndex])
	{
		TutorialName = "TUT-DM";
	}
	else if (Sender == TutButtons[DOMIndex])
	{
		TutorialName = "TUT-DOM";
	}
	else if (Sender == TutButtons[CTFIndex])
	{
		TutorialName = "TUT-CTF";
	}
	else if (Sender == TutButtons[BRIndex])
	{
		TutorialName = "TUT-BR";
	}
	else
		return false;

	if ( PlayerOwner().Level.Game.CurrentGameProfile != none ) {
		PlayerOwner().Level.Game.CurrentGameProfile.bInLadderGame=true;  // so that it'll reload into SP menus
		PlayerOwner().Level.Game.SavePackage(PlayerOwner().Level.Game.CurrentGameProfile.PackageName);
		PlayerOwner().ConsoleCommand ("START"@TutorialName$".ut2?quickstart=true?TeamScreen=false?savegame="$PlayerOwner().Level.Game.CurrentGameProfile.PackageName);
	}
	else
	{
		PlayerOwner().ConsoleCommand ("START"@TutorialName$".ut2?quickstart=true?TeamScreen=false");
	}
	Controller.CloseAll(false);

	return true;
}

// if no gameprofile loaded, or no team, can't show profile
function bool CanShowPanel ()
{
	return Super.CanShowPanel();
}


defaultproperties
{
	Begin Object Class=GUIImage Name=SPTutDM
		//Image=Material'Laddershots.TeamDMShot'
		ImageStyle=ISTY_Stretched
		WinTop=0.05
		WinLeft=0.1
		WinHeight=0.30
		WinWidth=0.30
		X1=0
		Y1=0
		X2=1023
		Y2=767
	End Object
	Begin Object Class=GUIImage Name=SPTutDMB
		Image=Material'InterfaceContent.BorderBoxA1'
		ImageStyle=ISTY_Stretched
		WinWidth=0.304563
		WinHeight=0.305562
		WinLeft=0.097344
		WinTop=0.045846
	End Object
	Begin Object Class=GUILabel Name=SPTutDML
		WinWidth=0.4
		WinHeight=0.1
		WinLeft=0.05
		WinTop=0.35
		TextAlign=TXTA_Center
		StyleName="TextButton"
	End Object
	Begin Object Class=GUIButton Name=SPTutDMButton
		OnClick=InternalOnClick
		StyleName="NoBackground"
		WinWidth=0.4
		WinHeight=0.4
		WinLeft=0.05
		WinTop=0.0333
		bFocusOnWatch=true
	End Object


	Begin Object Class=GUIImage Name=SPTutDOM
		//Image=Material'Laddershots.DOMShot'
		ImageStyle=ISTY_Stretched
		WinTop=0.50
		WinLeft=0.1
		WinHeight=0.3
		WinWidth=0.3
		X1=0
		Y1=0
		X2=1023
		Y2=767
	End Object
	Begin Object Class=GUIImage Name=SPTutDOMB
		Image=Material'InterfaceContent.BorderBoxA1'
		ImageStyle=ISTY_Stretched
		WinWidth=0.303
		WinHeight=0.304
		WinLeft=0.097344
		WinTop=0.495846
	End Object
	Begin Object Class=GUILabel Name=SPTutDOML
		WinWidth=0.4
		WinHeight=0.1
		WinLeft=0.05
		WinTop=0.8
		TextAlign=TXTA_Center
		StyleName="TextButton"
	End Object
	Begin Object Class=GUIButton Name=SPTutDOMButton
		OnClick=InternalOnClick
		StyleName="NoBackground"
		WinWidth=0.4
		WinHeight=0.415
		WinLeft=0.05
		WinTop=0.46666
		bFocusOnWatch=true
	End Object

	Begin Object Class=GUIImage Name=SPTutCTF
		//Image=Material'Laddershots.CTFShot'
		ImageStyle=ISTY_Stretched
		WinTop=0.05
		WinLeft=0.60
		WinHeight=0.3
		WinWidth=0.3
		X1=0
		Y1=0
		X2=1023
		Y2=767
	End Object
	Begin Object Class=GUIImage Name=SPTutCTFB
		Image=Material'InterfaceContent.BorderBoxA1'
		ImageStyle=ISTY_Stretched
		WinWidth=0.303
		WinHeight=0.304
		WinLeft=0.597344
		WinTop=0.045846
	End Object
	Begin Object Class=GUILabel Name=SPTutCTFL
		WinWidth=0.4
		WinHeight=0.1
		WinLeft=0.55
		WinTop=0.35
		TextAlign=TXTA_Center
		StyleName="TextButton"
	End Object
	Begin Object Class=GUIButton Name=SPTutCTFButton
		OnClick=InternalOnClick
		StyleName="NoBackground"
		WinWidth=0.4
		WinHeight=0.4
		WinLeft=0.55
		WinTop=0.03333
		bFocusOnWatch=true
	End Object

	Begin Object Class=GUIImage Name=SPTutBR
		//Image=Material'Laddershots.BRShot'
		ImageStyle=ISTY_Stretched
		WinTop=0.50
		WinLeft=0.60
		WinHeight=0.3
		WinWidth=0.3
		X1=0
		Y1=0
		X2=1023
		Y2=767
	End Object
	Begin Object Class=GUIImage Name=SPTutBRB
		Image=Material'InterfaceContent.BorderBoxA1'
		ImageStyle=ISTY_Stretched
		WinWidth=0.303
		WinHeight=0.304
		WinLeft=0.597344
		WinTop=0.495846
	End Object
	Begin Object Class=GUILabel Name=SPTutBRL
		WinWidth=0.4
		WinHeight=0.1
		WinLeft=0.55
		WinTop=0.8
		TextAlign=TXTA_Center
		StyleName="TextButton"
	End Object
	Begin Object Class=GUIButton Name=SPTutBRButton
		OnClick=InternalOnClick
		StyleName="NoBackground"
		WinWidth=0.4
		WinHeight=0.415
		WinLeft=0.55
		WinTop=0.46666
		bFocusOnWatch=true
	End Object


	Controls(0)=SPTutDM
	Controls(1)=SPTutDMB
	Controls(2)=SPTutDML
	Controls(3)=SPTutDMButton
	Controls(4)=SPTutDOM
	Controls(5)=SPTutDOMB
	Controls(6)=SPTutDOML
	Controls(7)=SPTutDOMButton
	Controls(8)=SPTutCTF
	Controls(9)=SPTutCTFB
	Controls(10)=SPTutCTFL
	Controls(11)=SPTutCTFButton
	Controls(12)=SPTutBR
	Controls(13)=SPTutBRB
	Controls(14)=SPTutBRL
	Controls(15)=SPTutBRButton


	SelectMessage="Click to play tutorial for"

	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.77
	bAcceptsInput=false
	bFillHeight=true		// get it to fill vertical space from tab top
}
