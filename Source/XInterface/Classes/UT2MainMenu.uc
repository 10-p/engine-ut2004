// ====================================================================
//  Class:  XInterface.UT2MainMenu
//
// 	The Main Menu
//
//  (c) 2002, Epic Games, Inc.  All Rights Reserved
// ====================================================================

class UT2MainMenu extends UT2K3GUIPage;

#exec OBJ LOAD FILE=InterfaceContent.utx

var bool	AllowClose;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	Super.InitComponent(MyController, MyOwner);

	if (PlayerOwner().Level.IsDemoBuild())
	{
		Controls[3].SetFocus(none);
		Controls[2].MenuStateChange(MSAT_Disabled);
	}
}

function OnClose(optional Bool bCanceled)
{
}

function bool MyKeyEvent(out byte Key,out byte State,float delta)
{
	if(Key == 0x1B && State == 1)	// Escape pressed
	{
		AllowClose = true;
		return true;
	}
	else
		return false;
}

function bool CanClose(optional Bool bCanceled)
{
	if(AllowClose)
		Controller.OpenMenu("xinterface.UT2QuitPage");

	return false;
}


function bool ButtonClick(GUIComponent Sender)
{
	if ( Sender==Controls[2] )
		Controller.OpenMenu("xinterface.UT2SinglePlayerMain");
	if ( Sender==Controls[3] )
		Controller.OpenMenu("xinterface.ServerBrowser");
	if ( Sender==Controls[4] )
		Controller.OpenMenu("xinterface.UT2MultiplayerHostPage");
	if ( Sender==Controls[5] )
		Controller.OpenMenu("xinterface.UT2InstantActionPage");
	if ( Sender==Controls[6] )
		Controller.OpenMenu("xinterface.UT2SettingsPage");
	if (Sender==Controls[7] )
		Controller.OpenMenu("xinterface.UT2QuitPage");

	return true;
}

defaultproperties
{
	Begin Object Class=GUIImage Name=ImgUT2Logo
		Image=material'InterfaceContent.Logos.Logo'
		ImageStyle=ISTY_Scaled
		ImageRenderStyle=MSTY_Alpha
		ImageColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.800000
		WinHeight=0.500000
		WinLeft=0.100000
		WinTop=-0.033854
	End Object
	Controls(0)=GUIImage'ImgUT2Logo'

	Begin Object Class=GUIImage Name=ImgUT2Shader
		Image=material'InterfaceContent.Logos.fbSymbolShader'
		ImageStyle=ISTY_Scaled
		WinWidth=0.198242
		WinHeight=0.132813
		WinLeft=0.399414
		WinTop=0.223958
//		bVisible=false
	End Object
	Controls(1)=GUIImage'ImgUT2Shader'

	Begin Object Class=GUIButton Name=SinglePlayerButton
		StyleName="TextButton"
		Caption="SINGLE PLAYER"
		Hint="Play through the Tournament"
		OnClick=ButtonClick
		WinWidth=0.500000
		WinHeight=0.075000
		WinLeft=0.250000
		WinTop=0.438802

		bFocusOnWatch=true
	End Object
	Controls(2)=GUIButton'SinglePlayerButton'

	Begin Object Class=GUIButton Name=MultiplayerButton
		StyleName="TextButton"
		Caption="PLAY ON-LINE/LAN"
		Hint="Play with Human Opponents Over the Lan or the Internet"
		OnClick=ButtonClick
		WinWidth=0.500000
		WinHeight=0.075000
		WinLeft=0.250000
		WinTop=0.506251
		bFocusOnWatch=true
	End Object
	Controls(3)=GUIButton'MultiplayerButton'

	Begin Object Class=GUIButton Name=HostButton
		StyleName="TextButton"
		Caption="HOST MULTIPLAYER GAME"
		Hint="Start a server an invite others to join your game"
		OnClick=ButtonClick
		WinWidth=0.500000
		WinHeight=0.075000
		WinLeft=0.250000
		WinTop=0.577866
		bFocusOnWatch=true
	End Object
	Controls(4)=GUIButton'HostButton'


	Begin Object Class=GUIButton Name=InstantActionButton
		StyleName="TextButton"
		Caption="INSTANT ACTION"
		Hint="Play a Practice Match"
		OnClick=ButtonClick
		WinWidth=0.500000
		WinHeight=0.075000
		WinLeft=0.250000
		WinTop=0.658334
		bFocusOnWatch=true
	End Object
	Controls(5)=GUIButton'InstantActionButton'

	Begin Object Class=GUIButton Name=SettingsButton
		StyleName="TextButton"
		Caption="SETTINGS"
		Hint="Change Your Controls and Settings"
		OnClick=ButtonClick
		WinWidth=0.500000
		WinHeight=0.075000
		WinLeft=0.250000
		WinTop=0.733595
		bFocusOnWatch=true
	End Object
	Controls(6)=GUIButton'SettingsButton'

	Begin Object Class=GUIButton Name=QuitButton
		Caption="QUIT"
		Hint="Exit Unreal Tournament 2003"
		OnClick=ButtonClick
		StyleName="SquareMenuButton"
		WinWidth=0.205078
		WinHeight=0.042773
		WinLeft=0.391602
		WinTop=0.905725
		bFocusOnWatch=true
	End Object
	Controls(7)=GUIButton'QuitButton'


	Background=Material'InterfaceContent.Backgrounds.bg10'
	WinWidth=1.0
	WinHeight=1.0
	WinTop=0.0
	WinLeft=0.0
	OnCanClose=CanClose
	OnKeyEvent=MyKeyEvent
	AllowClose=False
	bAllowedAsLast=true
	bDisconnectOnOpen=true
}
