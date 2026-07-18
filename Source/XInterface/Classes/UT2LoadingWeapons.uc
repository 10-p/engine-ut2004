class UT2LoadingWeapons extends UT2K3GUIPage;

var Tab_WeaponPref	WeaponTab;

event Timer()
{
	local int i;
	local array<class<Weapon> > WeaponClass;
	local array<string> WeaponDesc;

	// Initialise weapon list. Sort based on current priority - highest priority first
	Controller.GetWeaponList(WeaponClass, WeaponDesc);

	for(i=0; i<WeaponClass.Length; i++)
	{
		WeaponTab.MyCurWeaponList.List.Add(WeaponClass[i].default.ItemName, WeaponClass[i], WeaponDesc[i]);
	}

	WeaponTab.MyCurWeaponList.List.SortList();

	// Spawn spinny weapon actor
	WeaponTab.SpinnyWeap = PlayerOwner().spawn(class'XInterface.SpinnyWeap');
	WeaponTab.SpinnyWeap.SetRotation(PlayerOwner().Rotation);
	WeaponTab.SpinnyWeap.SetStaticMesh(None);

	// Start with first item on list selected
	WeaponTab.MyCurWeaponList.List.SetIndex(0);
	WeaponTab.UpdateCurrentWeapon();

	WeaponTab.bWeapPrefInitialised = true;


	WeaponTab = None;
	Controller.CloseMenu();
}

function StartLoad(Tab_WeaponPref tab )
{
	WeaponTab = tab;

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
