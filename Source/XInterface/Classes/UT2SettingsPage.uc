// ====================================================================
//  Class:  XInterface.UT2SettingsPage
//  Parent: XInterface.GUIPage
//
//  <Enter a description here>
// ====================================================================

class UT2SettingsPage extends UT2K3GUIPage;

#exec OBJ LOAD FILE=InterfaceContent.utx

var localized string	VideoTabLabel,
						VideoTabHint,
						DetailsTabLabel,
						DetailsTabHint,
						AudioTabLabel,
						AudioTabHint,
						PlayerTabLabel,
						PlayerTabHint,
						NetworkTabLabel,
						NetworkTabHint,
						ControlsTabLabel,
						ControlsTabHint,
						IForceTabLabel,
						IForceTabHint,
						WeaponsTabLabel,
						WeaponsTabHint,
						GameTabLabel,
						GameTabHint,
                        HudTabLabel,
                        HudTabHint,
						SpeechBinderTabLabel,
						SpeechBinderTabHint;

var Tab_WeaponPref 		pWeaponPref;
var Tab_PlayerSettings  pPlayer;
var Tab_NetworkSettings	pNetwork;

var float				SavedPitch;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local GUITabControl TabC;
	local rotator PlayerRot;

	Super.Initcomponent(MyController, MyOwner);

	// Set camera's pitch to zero when menu initialised (otherwise spinny weap goes kooky)
	PlayerRot = PlayerOwner().Rotation;
	SavedPitch = PlayerRot.Pitch;
	PlayerRot.Pitch = 0;
	PlayerOwner().SetRotation(PlayerRot);

	TabC = GUITabControl(Controls[1]);
	GUITitleBar(Controls[0]).DockedTabs = TabC;

	TabC.AddTab(VideoTabLabel,"xinterface.Tab_VideoSettings",,VideoTabHint,true);
	TabC.AddTab(DetailsTabLabel,"xinterface.Tab_DetailSettings",,DetailsTabHint);
	TabC.AddTab(AudioTabLabel,"xinterface.Tab_AudioSettings",,AudioTabHint);
	pPlayer = Tab_PlayerSettings(TabC.AddTab(PlayerTabLabel,"xinterface.Tab_PlayerSettings",,PlayerTabHint));
	pNetwork = Tab_NetworkSettings(TabC.AddTab(NetworkTabLabel,"xinterface.Tab_NetworkSettings",,NetworkTabHint));
	TabC.AddTab(ControlsTabLabel,"xinterface.Tab_ControlSettings",,ControlsTabHint);
//	if ( MyPlayer.ForceFeedbackSupported() )	-- Restore later
	TabC.AddTab(IForceTabLabel,"xinterface.Tab_IForceSettings",,IForceTabHint);
	pWeaponPref = Tab_WeaponPref(TabC.AddTab(WeaponsTabLabel,"xinterface.Tab_WeaponPref",,WeaponsTabHint));
    TabC.AddTab(HudTabLabel,"xinterface.Tab_HudSettings",,HudTabHint);
	TabC.AddTab(GameTabLabel,"xinterface.Tab_GameSettings",,GameTabHint);
	TabC.AddTab(SpeechBinderTabLabel,"xinterface.Tab_SpeechBinder",,SpeechBinderTabHint);
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
	local GUITabControl TabC;
	TabC = GUITabControl(Controls[1]);

	if ( (TabC.ActiveTab!=None) && (TabC.ActiveTab.MyPanel!=None) )
		TabC.ActiveTab.MyPanel.Refresh();
}

function bool ButtonClicked(GUIComponent Sender)
{
	if(InternalOnCanClose(false))
    {
    	GUITabControl(Controls[1]).ActiveTab.OnDeActivate();
		Controller.CloseMenu(true);
    }

	return true;
}

function bool InternalOnCanClose(optional bool bCanceled)
{
	if(!pNetwork.ValidStatConfig())
	{
		GUITabControl(Controls[1]).ActivateTabByName(NetworkTabLabel,true);
		return false;
	}
	else
		return true;
}

function bool NotifyLevelChange()
{
	Controller.CloseMenu(true);
	return Super.NotifyLevelChange();
}

function InternalOnClose(optional Bool bCanceled)
{
	local rotator NewRot;

	// Destroy spinning weapon actor
	if(pWeaponPref.SpinnyWeap != None)
	{
		pWeaponPref.SpinnyWeap.Destroy();
		pWeaponPref = None;
	}

	// Destroy spinning player model actor
	if(pPlayer.SpinnyDude != None)
	{
		pPlayer.SpinnyDude.Destroy();
		pPlayer.SpinnyDude = None;
	}

	// Reset player
	NewRot = PlayerOwner().Rotation;
	NewRot.Pitch = SavedPitch;
	PlayerOwner().SetRotation(NewRot);

	// Save config.
	pNetwork.ApplyStatConfig();
	pPlayer.InternalApply(none);
	pWeaponPref.WeapApply(none);

	Super.OnClose(bCanceled);
}

defaultproperties
{
	Begin Object class=GUITitleBar name=SettingHeader
		Caption="Settings"
		StyleName="Header"
		WinWidth=1
		WinHeight=46.000000
		WinLeft=0
		WinTop=0.036406
		Effect=material'CO_Final'
	End Object
	Controls(0)=GUITitleBar'SettingHeader'

	Begin Object Class=GUITabControl Name=SettingTabs
		WinWidth=1.0
		WinLeft=0
		WinTop=0.25
		WinHeight=48
		TabHeight=0.04
//		bFillSpace=True
		OnChange=TabChange;
		bAcceptsInput=true
		bDockPanels=true
	End Object
	Controls(1)=GUITabControl'SettingTabs'

	Begin Object class=GUITitleBar name=SettingFooter
		WinWidth=0.88
		WinHeight=0.055
		WinLeft=0.12
		WinTop=0.93
		bUseTextHeight=false
		StyleName="Footer"
		Justification=TXTA_Center
	End Object
	Controls(2)=GUITitleBar'SettingFooter'

	Begin Object Class=GUIButton Name=BackButton
		Caption="BACK"
		StyleName="SquareMenuButton"
		Hint="Return to Previous Menu"
		WinWidth=0.12
		WinHeight=0.055
		WinLeft=0
		WinTop=0.93
		OnClick=ButtonClicked
	End Object
	Controls(3)=GUIButton'BackButton'

	Begin Object class=GUIImage Name=LogoSymbol
		WinWidth=0.26
		WinHeight=0.13
		WinLeft=0.830079
		WinTop=0.800782
		Image=material'fbSymbolShader'
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageStyle=ISTY_Scaled
		ImageRenderStyle=MSTY_Alpha
		bVisible=false
	End Object
	Controls(4)=GUIImage'LogoSymbol'


	OnReOpen=InternalOnReOpen
	OnClose=InternalOnClose
	OnCanClose=InternalOnCanClose

	Background=Material'InterfaceContent.Backgrounds.bg11'
	WinWidth=1.0
	WinHeight=1.0
	WinTop=0.0
	WinLeft=0.0

	VideoTabLabel="Video"
	DetailsTabLabel="Details"
	AudioTabLabel="Audio"
	PlayerTabLabel="Player"
	NetworkTabLabel="Network"
	ControlsTabLabel="Controls"
	IForceTabLabel="Input"
	WeaponsTabLabel="Weapons"
	GameTabLabel="Game"
    HudTabLabel="Hud"
	SpeechBinderTabLabel="Speech"

	VideoTabHint="Select your resolution and change your brightness..."
	DetailsTabHint="Adjust the detail settings for better graphics or faster framerate..."
	AudioTabHint="Adjust your audio experience..."
	PlayerTabHint="Configure your UT2003 Avatar..."
	NetworkTabHint="Configure UT2003 for Online and Lan play..."
	ControlsTabHint="Configure your controls..."
	IForceTabHint="Configure misc. input options..."
	WeaponsTabHint="Adjust your weapon priorities and settings..."
	GameTabHint="Adjust various game related settings..."
    HudTabHint="Customize your hud..."
	SpeechBinderTabHint="Bind messages to keys..."

}
