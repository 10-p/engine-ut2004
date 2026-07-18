class UT2ArenaConfig extends UT2K3GUIPage;

var array<class<Weapon> >	WeaponClass;
var array<String>			WeaponDesc;

var moComboBox				WeaponCombo;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	Super.InitComponent(MyController, MyOwner);

	WeaponCombo = moComboBox(Controls[4]);

	// Spawn 'please wait' screen while we DLO the weapons
	if ( Controller.OpenMenu("XInterface.UT2LoadingWeaponsArena") )
		UT2LoadingWeaponsArena(Controller.TopPage()).StartLoad(self);
}

function bool InternalOnClick(GUIComponent Sender)
{
	class'MutArena'.default.ArenaWeaponClassName = WeaponCombo.GetExtra();
	class'MutArena'.static.StaticSaveConfig();

	Controller.CloseMenu(false);

	return true;
}

defaultproperties
{
	Begin Object Class=GUIButton name=DialogBackground
		WinWidth=1.0
		WinHeight=1.0
		WinTop=0
		WinLeft=0
		bAcceptsInput=false
		bNeverFocus=true
		StyleName="ComboListBox"
		bBoundToParent=true
		bScaleToParent=true
	End Object
	Controls(0)=GUIButton'DialogBackground'

	Begin Object Class=GUIButton Name=OkButton
		Caption="OK"
		WinWidth=0.2
		WinHeight=0.04
		WinLeft=0.4
		WinTop=0.6
		OnClick=InternalOnClick
	End Object
	Controls(1)=GUIButton'OkButton'

	Begin Object class=GUILabel Name=DialogText
		Caption="Weapon Arena"
		TextALign=TXTA_Center
		TextColor=(R=220,G=180,B=0,A=255)
		TextFont="UT2HeaderFont"
		WinWidth=1.000000
		WinHeight=32.000000
		WinLeft=0.000000
		WinTop=0.325000
	End Object
	Controls(2)=GUILabel'DialogText'

	Begin Object class=GUILabel Name=DialogText2
		Caption="Choose the weapon to populate your Arena."
		TextALign=TXTA_Center
		TextColor=(R=220,G=180,B=0,A=255)
		TextFont="UT2MenuFont"
		WinWidth=1.000000
		WinHeight=32.000000
		WinLeft=0.000000
		WinTop=0.390626
	End Object
	Controls(3)=GUILabel'DialogText2'

	Begin Object class=moComboBox Name=WeaponSelect
		WinWidth=0.431641
		WinHeight=0.04
		WinLeft=0.293750
		WinTop=0.467448
		Caption="Weapon"
		INIOption="@INTERNAL"
		CaptionWidth=0.3
		ComponentJustification=TXTA_Left
		bReadOnly=true
	End Object
	Controls(4)=moComboBox'WeaponSelect'

	WinLeft=0
	WinTop=0.3
	WinWidth=1
	WinHeight=0.4
	bRequire640x480=True
}
