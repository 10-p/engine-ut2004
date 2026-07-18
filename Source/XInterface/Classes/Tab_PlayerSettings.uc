// ====================================================================
//  Class:  XInterface.Tab_OnlineSettings
//  Parent: XInterface.GUITabPanel
//
//  <Enter a description here>
// ====================================================================

class Tab_PlayerSettings extends UT2K3TabPanel;

var localized string HandNames[4];
var localized string TeamNames[2];

var config bool bUnlocked;		// whether the boss characters have been unlocked
var bool bChanged;

// Used for character (not just weapons!)
var SpinnyWeap			SpinnyDude; // MUST be set to null when you leave the window
var vector				SpinnyDudeOffset;
var bool				bRenderDude;
var localized string	ShowBioCaption;
var localized string	Show3DViewCaption;

var string OriginalTeam;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local int i;

	Super.Initcomponent(MyController, MyOwner);

	for (i=0;i<Controls.Length;i++)
		Controls[i].OnChange=InternalOnChange;

	moComboBox(Controls[3]).AddItem(HandNames[0]);
	moComboBox(Controls[3]).AddItem(HandNames[1]);
	moComboBox(Controls[3]).AddItem(HandNames[2]);
	moComboBox(Controls[3]).AddItem(HandNames[3]);
	moComboBox(Controls[3]).ReadOnly(true);

	moComboBox(Controls[2]).AddItem(TeamNames[0]);
	moComboBox(Controls[2]).AddItem(TeamNames[1]);
	moComboBox(Controls[2]).ReadOnly(true);
    moComboBox(Controls[2]).MyComboBox.OnChange = TeamChange;

	if (bUnlocked)
		GUICharacterListTeam(Controls[4]).InitListExclusive("DUP");
	else
		GUICharacterListTeam(Controls[4]).InitListExclusive("DUP", "UNLOCK");

	GUIImage(Controls[8]).Image = GUICharacterList(Controls[4]).GetPortrait();
	GUIScrollTextBox(Controls[5]).SetContent(Controller.LoadDecoText("",GUICharacterListTeam(Controls[4]).GetDecoText()));

	// Spawn spinning character actor
	SpinnyDude = PlayerOwner().spawn(class'XInterface.SpinnyWeap');
	SpinnyDude.SetRotation(PlayerOwner().Rotation);
	SpinnyDude.SetDrawType(DT_Mesh);
	SpinnyDude.bPlayRandomAnims = true;
	SpinnyDude.SetDrawScale(0.9);
	SpinnyDude.SpinRate = 12000;

	bRenderDude = false;
	GUIButton(Controls[9]).Caption = Show3DViewCaption;

	moEditBox(Controls[1]).MyEditBox.bConvertSpaces = true;
	moEditBox(Controls[1]).MyEditBox.MaxWidth=16;  // as per polge, check Tab_PlayerSettings if you change this

	moNumericEdit(Controls[12]).MyNumericEdit.Step = 5;

    moCheckBox(Controls[13]).Checked(PlayerOwner().bSmallWeapons);

    if (ExtendedConsole(PlayerOwner().Player.Console).MusicManager==None)
      Controls[14].bVisible = false;

}

function UpdateSpinnyDude()
{
	local xUtil.PlayerRecord Rec;
	local Mesh PlayerMesh;
	local Material BodySkin, HeadSkin;

	Rec = GUICharacterListTeam(Controls[4]).GetRecord();

	PlayerMesh = Mesh(DynamicLoadObject(Rec.MeshName, class'Mesh'));
	if(PlayerMesh == None)
	{
		Log("Could not load mesh: "$Rec.MeshName$" For player: "$Rec.DefaultName);
		return;
	}

	BodySkin = Material(DynamicLoadObject(Rec.BodySkinName, class'Material'));
	if(BodySkin == None)
	{
		Log("Could not load body material: "$Rec.BodySkinName$" For player: "$Rec.DefaultName);
		return;
	}

	HeadSkin = Material(DynamicLoadObject(Rec.FaceSkinName, class'Material'));
	if(HeadSkin == None)
	{
		Log("Could not load head material: "$Rec.FaceSkinName$" For player: "$Rec.DefaultName);
		return;
	}

	SpinnyDude.LinkMesh(PlayerMesh);
	SpinnyDude.Skins[0] = BodySkin;
	SpinnyDude.Skins[1] = HeadSkin;
	SpinnyDude.LoopAnim( 'Idle_Rest', 1.0 );
}

function bool InternalDraw(Canvas canvas)
{
	local vector CamPos, X, Y, Z;
	local rotator CamRot;

	if(bRenderDude)
	{
		canvas.GetCameraLocation(CamPos, CamRot);
		GetAxes(CamRot, X, Y, Z);

		SpinnyDude.SetLocation(CamPos + (SpinnyDudeOffset.X * X) + (SpinnyDudeOffset.Y * Y) + (SpinnyDudeOffset.Z * Z));

		canvas.DrawActor(SpinnyDude, false, true, 90.0);
	}

	return false;
}

function bool PlayList(GUIComponent Sender)
{
	ExtendedConsole(PlayerOwner().Player.Console).MusicMenu();
    return true;
}

function bool Toggle3DView(GUIComponent Sender)
{
	bRenderDude = !bRenderDude;

	if(bRenderDude)
	{
		UpdateSpinnyDude(); // Load current character
		Controls[5].bVisible = false; // Hide biography text
//		Controls[11].bVisible = true; // Show player border
		GUIButton(Controls[9]).Caption = ShowBioCaption; // Change button caption
	}
	else
	{
		// Put text back into box
		Controls[5].bVisible = true;
		Controls[11].bVisible = false;
		GUIButton(Controls[9]).Caption = Show3DViewCaption;
		SpinnyDude.LinkMesh(None);
	}

	return true;
}

function bool NextAnim(GUIComponent Sender)
{
	if(bRenderDude)
	{
		SpinnyDude.PlayNextAnim();
	}

	return true;
}

function InternalOnLoadINI(GUIComponent Sender, string s)
{
	local string CharName,TeamName;
	local int i;

	if (Sender==Controls[1])
	{
		moEditBox(Sender).SetText(PlayerOwner().GetUrlOption( "Name" ));
	}
	else if (Sender==Controls[2])
	{

		if ( (PlayerOwner().PlayerReplicationInfo==None) || (PlayerOwner().PlayerReplicationInfo.Team==None) )
			TeamName = PlayerOwner().GetURLOption("Team");
        else
        	TeamName = ""$PlayerOwner().PlayerReplicationInfo.Team.TeamIndex;

		OriginalTeam = TeamName;

		if (TeamName~="1")
		    moComboBox(Controls[2]).SetText(TeamNames[1]);
		else
			moComboBox(Controls[2]).SetText(TeamNames[0]);
	}

	else if (Sender==Controls[3])
	{
		i = class'PlayerController'.default.Handedness;

		if (i==2)
			moComboBox(Controls[3]).SetText(HandNames[1]);
		else if (i==-1)
			moComboBox(Controls[3]).SetText(HandNames[2]);
		else if (i==0)
			moComboBox(Controls[3]).SetText(HandNames[3]);
		else
			moComboBox(Controls[3]).SetText(HandNames[0]);

	}
	else if (Sender==Controls[4])
	{
		CharName = PlayerOwner().GetUrlOption( "Character" );
		GUICharacterList(Sender).find(charname);
		GUIImage(Controls[8]).Image = GUICharacterList(Controls[4]).GetPortrait();
		GUIScrollTextBox(Controls[5]).SetContent(Controller.LoadDecoText("",GUICharacterListTeam(Controls[4]).GetDecoText()));

	}
    else if (Sender==Controls[12])
		moNumericEdit(Controls[12]).SetValue(PlayerOwner().DefaultFOV);


}

function TeamChange(GUIComponent Sender)
{
	bChanged=true;
}

function InternalOnChange(GUIComponent Sender)
{

	local int i;
	local GUICharacterList c;
	local string cname;
	local sound NameSound;
	local bool CharName;

	if (!Controller.bCurMenuInitialized)
		return;

	if ((Sender==Controls[1]) || (Sender==Controls[2]) || (Sender==Controls[3]) || Sender==Controls[12] )
		bChanged=true;

	else if (Sender==Controls[4])
	{
		GUIImage(Controls[8]).Image = GUICharacterList(Controls[4]).GetPortrait();
		GUIScrollTextBox(Controls[5]).SetContent(Controller.LoadDecoText("",GUICharacterListTeam(Controls[4]).GetDecoText()));

		C = GUICharacterList(Controls[4]);
		cname = moEditBox(Controls[1]).GetText();

		// If the text box is an existing character name (or blank), change it when we click on new characters
		CharName=false;

		if(cname ~= "Nothing" || cname ~= "" || cname ~= "Player")
			CharName=true;

		for (i=0; i<C.PlayerList.Length && !CharName; i++)
		{
			if (C.PlayerList[i].DefaultName~=cname)
				CharName=true;
		}

		if(CharName)
			moEditBox(Controls[1]).SetText(GUICharacterList(Controls[4]).SelectedText());

		NameSound = GUICharacterList(Controls[4]).GetSound();
		PlayerOwner().ClientPlaySound(NameSound,,,SLOT_Interface);

		// Change 3D graphic if desplayed
		if(bRenderDude)
			UpdateSpinnyDude();

		bChanged=true;
	}
    else if (Sender==Controls[13])
    {
    	PlayerOwner().bSmallWeapons = moCheckBox(Controls[13]).IsChecked();
        PlayerOwner().SaveConfig();
    }

}

function bool InternalOnClick(GUIComponent Sender)
{

	if (Sender==Controls[6])
		GUICharacterList(Controls[4]).PgUp();
	else if (Sender==Controls[7])
		GUICharacterList(Controls[4]).PgDown();


	return true;
}



function bool InternalApply(GUIComponent Sender)
{
	local string PName, PChar, PTeam;
    local string NewTeam;

	if (!bChanged)
		return true;

	PName = moEditBox(Controls[1]).GetText();
	PChar = GUICharacterList(Controls[4]).SelectedText();
	PTeam  = moComboBox(Controls[2]).GetText();

	PlayerOwner().UpdateURL("Name",PName, true);
	PlayerOwner().UpdateURL("Character",pChar,true);

	if (PTeam~=TeamNames[1])
		NewTeam = "1";
    else
       	NewTeam = "0";

	if (NewTeam != OriginalTeam)
    {
		PlayerOwner().UpdateURL("Team", NewTeam, true);

		PlayerOwner().ChangeTeam(int(PlayerOwner().GetURLOption("Team")));
	}

	PlayerOwner().ConsoleCommand("setname"@PName);
	PlayerOwner().ConsoleCommand("changecharacter"@PChar);

	if (moComboBox(Controls[3]).GetText()==HandNames[1])
		PlayerOwner().SetHand(2);
	else if (moComboBox(Controls[3]).GetText()==HandNames[2])
		PlayerOwner().SetHand(-1);
	else if (moComboBox(Controls[3]).GetText()==HandNames[3])
		PlayerOwner().SetHand(0);
	else
		PlayerOwner().SetHand(1);

	PlayerOwner().FOV(moNumericEdit(Controls[12]).GetValue());

	bChanged = false;

	return true;

}

function ShowPanel(bool bShow)	// Show Panel should be subclassed if any special processing is needed
{
	Super.ShowPanel(bShow);

	if (!bShow)
		InternalApply(none);
}


defaultproperties
{
	Begin Object class=GUIImage Name=PlayerBK1
		WinWidth=0.434062
		WinLeft=0.506016
		WinTop=0.008854
		WinHeight=0.377694
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	Controls(0)=GUIImage'PlayerBK1'

	Begin Object class=moEditBox Name=PlayerName
		WinWidth=0.300000
		WinHeight=0.060000
		WinLeft=0.121093
		WinTop=0.714063
		Caption="Name"
		INIOption="@INTERNAL"
		INIDefault="Player"
		OnLoadINI=InternalOnLoadINI
		Hint="Changes the alias you play as."
		CaptionWidth=0.25
	End Object
	Controls(1)=moEditBox'PlayerName'

	Begin Object class=moComboBox Name=PlayerTeam
		WinWidth=0.393750
		WinHeight=0.060000
		WinLeft=0.524766
		WinTop=0.036979

		Caption="Preferred Team"
		INIOption="@Internal"
		INIDefault="Red"
		OnLoadINI=InternalOnLoadINI
		Hint="Changes the team you will play on by default."
		CaptionWidth=0.65
		ComponentJustification=TXTA_Left
	End Object
	Controls(2)=GUIMenuOption'PlayerTeam'


	Begin Object class=moComboBox Name=PlayerHand
		WinWidth=0.393750
		WinHeight=0.060000
		WinLeft=0.524766
		WinTop=0.130729
		Caption="Weapon"
		INIOption="@INTERNAL"
		INIDefault="High"
		OnLoadINI=InternalOnLoadINI
		Hint="Changes whether your weapon is visible."
		CaptionWidth=0.65
		ComponentJustification=TXTA_Left
	End Object
	Controls(3)=GUIMenuOption'PlayerHand'

	Begin Object class=GUICharacterListTeam Name=PlayerCharList
		WinWidth=0.453729
		WinHeight=0.189297
		WinLeft=0.036465
		WinTop=0.813543
		StyleName="CharButton"
		INIOption="@Internal"
		OnLoadINI=InternalOnLoadINI
		Hint="Changes the character you play as."
	End Object
	Controls(4)=GUICharacterListTeam'PlayerCharList'

	Begin Object Class=GUIScrollTextBox Name=PlayerScroll
		WinWidth=0.472071
		WinHeight=0.397070
		WinLeft=0.506132
		WinTop=0.406000
		CharDelay=0.0025
		EOLDelay=0.5
		bNeverFocus=true
		//StyleName="NoBackground"
	End Object
	Controls(5)=GUIScrollTextBox'PlayerScroll'

	Begin Object class=GUIButton Name=PlayerLeft
		WinWidth=0.043555
		WinHeight=0.084414
		WinLeft=0.000781
		WinTop=0.886460
		bNeverFocus=true
		bRepeatClick=true
		OnClick=InternalOnClick
		OnClickSound=CS_Down
		StyleName="ArrowLeft"
	End Object
	Controls(6)=GUIButton'PlayerLeft'

	Begin Object class=GUIButton Name=PlayerRight
		WinWidth=0.043555
		WinHeight=0.084414
		WinLeft=0.479688
		WinTop=0.886460
		StyleName="ArrowRight"
		bNeverFocus=true
		bRepeatClick=true
		OnClick=InternalOnClick
		OnClickSound=CS_Up
	End Object
	Controls(7)=GUIButton'PlayerRight'

	Begin Object class=GUIImage Name=PlayerPortrait
		WinWidth=0.183000
		WinHeight=0.637000
		WinLeft=0.172250
		WinTop=0.024583
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Scaled
	End Object
	Controls(8)=GUIImage'PlayerPortrait'

	Begin Object class=GUIButton Name=Player3DView
		WinWidth=0.250000
		WinHeight=0.050000
		WinLeft=0.620000
		WinTop=0.977868
		Caption="3D View"
		Hint="Toggle between 3D view and biography of character."
		OnClick=Toggle3DView
	End Object
	Controls(9)=GUIButton'Player3DView'

	Begin Object class=GUIImage Name=PlayerPortraitBorder
		WinWidth=0.188563
		WinHeight=0.647875
		WinLeft=0.168687
		WinTop=0.018500
		Image=Material'InterfaceContent.Menu.BorderBoxA1'
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Stretched
	End Object
	Controls(10)=GUIImage'PlayerPortraitBorder'

	Begin Object class=GUIImage Name=Player3DBack
		WinWidth=0.472071
		WinHeight=0.742383
		WinLeft=0.506132
		WinTop=0.156000
		Image=Material'InterfaceContent.Menu.BorderBoxA'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageStyle=ISTY_Stretched
		OnClick=NextAnim
		bVisible=false
		bAcceptsInput=true
		OnClickSound=CS_Click
	End Object
	Controls(11)=GUIImage'Player3DBack'

	Begin Object class=moNumericEdit Name=PlayerFOV
		WinWidth=0.393750
		WinHeight=0.060000
		WinLeft=0.524766
		WinTop=0.228123
		Caption="Default FOV"
		CaptionWidth=0.65
		MinValue=80
		MaxValue=100
		INIOption="@INTERNAL"
		INIDefault="85"
		OnLoadINI=InternalOnLoadINI
		ComponentJustification=TXTA_Left
		Hint="This value will change your field of view while playing."
	End Object
	Controls(12)=moNumericEdit'PlayerFOV'

	Begin Object class=moCheckBox Name=PlayerSmallWeap
		WinWidth=0.393750
		WinHeight=0.040000
		WinLeft=0.521875
		WinTop=0.318333
		Caption="Small Weapons"
		INIOption="@Internal"
		INIDefault="False"
		Hint="Makes your first person weapon smaller."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(13)=moCheckBox'PlayerSmallWeap'

	Begin Object class=GUIButton Name=PlayerOGG
		WinWidth=0.250000
		WinHeight=0.050000
		WinLeft=0.620000
		WinTop=0.918750
		Caption="Play List"
		Hint="Configure your OGG Play List."
		OnClick=PlayList
	End Object
	Controls(14)=GUIButton'PlayerOGG'

	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.72
	bAcceptsInput=false

	HandNames(0)="Right"
	HandNames(1)="Hidden"
	HandNames(2)="Left"
	HandNames(3)="Center"

	TeamNames(0)="Red"
	TeamNames(1)="Blue"

	OnDraw=InternalDraw
	bRenderDude=true
	SpinnyDudeOffset=(X=150,Y=77,Z=-22)
	ShowBioCaption="Bio"
	Show3DViewCaption="3D View"

}
