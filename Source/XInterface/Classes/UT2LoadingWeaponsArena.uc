class UT2LoadingWeaponsArena extends UT2K3GUIPage;

var UT2ArenaConfig	Config;

event Timer()
{
	local int i;

	// Initialise weapon list. Sort based on current priority - highest priority first
	Controller.GetWeaponList(Config.WeaponClass, Config.WeaponDesc);

	for(i=0; i<Config.WeaponClass.Length; i++)
	{
		Config.WeaponCombo.AddItem( Config.WeaponClass[i].default.ItemName, None, String(Config.WeaponClass[i]) );

		// If this is the currently selected one - put it in the dialog
		if( class'MutArena'.default.ArenaWeaponClassName == String(Config.WeaponClass[i]) )
		{
			Config.WeaponCombo.SetText( Config.WeaponClass[i].default.ItemName );
		}
	}

	Config = None;
	Controller.CloseMenu();
}

function StartLoad(UT2ArenaConfig arena )
{
	Config = arena;

	// Give the menu a chance to render before doing anything...
	SetTimer(0.15);
}

defaultproperties
{
	Begin Object Class=GUIButton name=LoadWeapBackground
		WinWidth=0.5
		WinHeight=1.0
		WinTop=0
		WinLeft=0.25
		bAcceptsInput=false
		bNeverFocus=true
		StyleName="SquareButton"
		bBoundToParent=true
		bScaleToParent=true
	End Object
	Controls(0)=LoadWeapBackground

	Begin Object class=GUILabel Name=LoadWeapText
		Caption="Loading Weapon Database"
		TextALign=TXTA_Center
		TextColor=(R=220,G=180,B=0,A=255)
		TextFont="UT2HeaderFont"
		WinWidth=1.000000
		WinHeight=32.000000
		WinLeft=0.000000
		WinTop=0.471667
	End Object
	Controls(1)=LoadWeapText

	WinLeft=0
	WinTop=0.425
	WinWidth=1
	WinHeight=0.15	
}
