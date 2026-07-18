class Browser_Prefs extends Browser_Page;

var GUITitleBar		StatusBar;

var localized string	ViewStatsStrings[3];
var localized string	MutatorModeStrings[4];
var localized string    WeaponStayStrings[3];
var localized string    TranslocatorStrings[3];

var bool				bIsInitialised;

var array<xUtil.MutatorRecord> MutatorRecords;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local int i;

	Super.InitComponent(MyController, MyOwner);

	if(bIsInitialised)
		return;

	GUIButton(GUIPanel(Controls[0]).Controls[0]).OnClick=BackClick;

	// Set options for stats server viewing
	moComboBox(Controls[5]).AddItem(ViewStatsStrings[0]);
	moComboBox(Controls[5]).AddItem(ViewStatsStrings[1]);
	moComboBox(Controls[5]).AddItem(ViewStatsStrings[2]);
	moComboBox(Controls[5]).ReadOnly(true);

	// Load mutators into combobox
	class'xUtil'.static.GetMutatorList(MutatorRecords);

	moComboBox(Controls[9]).AddItem(MutatorModeStrings[0]);
	moComboBox(Controls[9]).AddItem(MutatorModeStrings[1]);
	moComboBox(Controls[9]).AddItem(MutatorModeStrings[2]);
	moComboBox(Controls[9]).AddItem(MutatorModeStrings[3]);
	moComboBox(Controls[9]).ReadOnly(true);

	moComboBox(Controls[13]).AddItem(MutatorModeStrings[0]);
	moComboBox(Controls[13]).AddItem(MutatorModeStrings[2]);
	moComboBox(Controls[13]).AddItem(MutatorModeStrings[3]);
	moComboBox(Controls[13]).ReadOnly(true);

	for(i=0; i<MutatorRecords.Length; i++)
	{
		moComboBox(Controls[6]).AddItem(MutatorRecords[i].FriendlyName, None, MutatorRecords[i].ClassName);
		moComboBox(Controls[14]).AddItem(MutatorRecords[i].FriendlyName, None, MutatorRecords[i].ClassName);
	}
	moComboBox(Controls[6]).ReadOnly(true);

	// Weapon stay
	moComboBox(Controls[11]).AddItem(WeaponStayStrings[0]);
	moComboBox(Controls[11]).AddItem(WeaponStayStrings[1]);
	moComboBox(Controls[11]).AddItem(WeaponStayStrings[2]);
	moComboBox(Controls[11]).ReadOnly(true);

	// Translocator
	moComboBox(Controls[12]).AddItem(TranslocatorStrings[0]);
	moComboBox(Controls[12]).AddItem(TranslocatorStrings[1]);
	moComboBox(Controls[12]).AddItem(TranslocatorStrings[2]);
	moComboBox(Controls[12]).ReadOnly(true);

	moNumericEdit(Controls[15]).MyNumericEdit.Step = 10;
	moNumericEdit(Controls[16]).MyNumericEdit.Step = 10;

	for(i=2; i<17; i++)
	{
		Controls[i].OnLoadINI=MyOnLoadINI;
		Controls[i].OnChange=MyOnChange;
	}

	StatusBar = GUITitleBar(GUIPanel(Controls[0]).Controls[1]);

	// Set on click for 'Show Icon Key' button
	GUIButton(GUIPanel(Controls[0]).Controls[2]).OnClick = InternalShowIconKey;

	bIsInitialised=true;
}

// delegates
function bool BackClick(GUIComponent Sender)
{
	Controller.CloseMenu(true);
	return true;
}

function UpdateMutatorVisibility()
{
	// If first one is 'any' or 'none', don't show any mutator selector boxes
	if(Browser.ViewMutatorMode == VMM_AnyMutators || Browser.ViewMutatorMode == VMM_NoMutators)
	{
		Controls[6].bVisible = false;
		Controls[13].bVisible = false;
		Controls[14].bVisible = false;
	}
	else // If its not - show the first mutator selection box, and the second mode box
	{
		Controls[6].bVisible = true;

		Controls[13].bVisible = true;

		// And check the second mode drop-down,
		if(Browser.ViewMutator2Mode == VMM_AnyMutators)
			Controls[14].bVisible = false;
		else
			Controls[14].bVisible = true;
	}
}

function bool InternalShowIconKey(GUIComponent Sender)
{
	Controller.OpenMenu("XInterface.Browser_IconKey");

	return true;
}


///////////////////// LOAD ///////////////////////
function MyOnLoadINI(GUIComponent Sender, string s)
{
	local int i;

	if(Sender == Controls[8])
		moCheckBox(Controls[8]).Checked(Browser.bOnlyShowStandard);
	else if(Sender == Controls[2])
		moCheckBox(Controls[2]).Checked(Browser.bOnlyShowNonPassword);
	else if(Sender == Controls[3])
		moCheckBox(Controls[3]).Checked(Browser.bDontShowFull);
	else if(Sender == Controls[4])
		moCheckBox(Controls[4]).Checked(Browser.bDontShowEmpty);
	else if(Sender == Controls[5])
		moComboBox(Controls[5]).SetText(ViewStatsStrings[Browser.StatsServerView]);
	else if(Sender == Controls[6])
	{
		// Find the Mutator with this class name, and put its friendly name in the box
		for(i=0; i<MutatorRecords.Length; i++)
		{
			if( Browser.DesiredMutator == MutatorRecords[i].ClassName )
			{
				moComboBox(Controls[6]).SetText( MutatorRecords[i].FriendlyName );
				return;
			}
		}
	}
	else if(Sender == Controls[7])
	{
		moEditBox(Controls[7]).SetText(Browser.CustomQuery);
	}
	else if(Sender == Controls[9])
	{
		moComboBox(Controls[9]).SetText( MutatorModeStrings[Browser.ViewMutatorMode] );

		UpdateMutatorVisibility();
	}
	else if(Sender == Controls[10])
		moCheckBox(Controls[10]).Checked(Browser.bDontShowWithBots);
	else if(Sender == Controls[11])
		moComboBox(Controls[11]).SetText(WeaponStayStrings[Browser.WeaponStayServerView]);
	else if(Sender == Controls[12])
		moComboBox(Controls[12]).SetText(TranslocatorStrings[Browser.TranslocServerView]);
	else if(Sender == Controls[13])
	{
		moComboBox(Controls[13]).SetText( MutatorModeStrings[Browser.ViewMutator2Mode] );

		UpdateMutatorVisibility();
	}
	else if(Sender == Controls[14])
	{
		for(i=0; i<MutatorRecords.Length; i++)
		{
			if( Browser.DesiredMutator2 == MutatorRecords[i].ClassName )
			{
				moComboBox(Controls[14]).SetText( MutatorRecords[i].FriendlyName );
				return;
			}
		}
	}
	else if(Sender == Controls[15])
		moNumericEdit(Controls[15]).SetValue(Browser.MinGamespeed);
	else if(Sender == Controls[16])
		moNumericEdit(Controls[16]).SetValue(Browser.MaxGamespeed);
}

///////////////////// SAVE ///////////////////////
function MyOnChange(GUIComponent Sender)
{
	local string t;

	if(Sender == Controls[8])
		Browser.bOnlyShowStandard = moCheckBox(Controls[8]).IsChecked();
	else if(Sender == Controls[2])
		Browser.bOnlyShowNonPassword = moCheckBox(Controls[2]).IsChecked();
	else if(Sender == Controls[3])
		Browser.bDontShowFull = moCheckBox(Controls[3]).IsChecked();
	else if(Sender == Controls[4])
		Browser.bDontShowEmpty = moCheckBox(Controls[4]).IsChecked();
	else if(Sender == Controls[5])
	{
		t = moComboBox(Controls[5]).GetText();

		if(t == ViewStatsStrings[0])
			Browser.StatsServerView = SSV_Any;
		else if(t == ViewStatsStrings[1])
			Browser.StatsServerView = SSV_OnlyStatsEnabled;
		else if(t == ViewStatsStrings[2])
			Browser.StatsServerView = SSV_NoStatsEnabled;
	}
	else if(Sender == Controls[6])
	{
		Browser.DesiredMutator = moComboBox(Controls[6]).GetExtra();
	}
	else if(Sender == Controls[7])
	{
		Browser.CustomQuery = moEditBox(Controls[7]).GetText();
	}
	else if(Sender == Controls[9])
	{
		t = moComboBox(Controls[9]).GetText();

		if(t == MutatorModeStrings[0])
			Browser.ViewMutatorMode = VMM_AnyMutators;
		else if(t == MutatorModeStrings[1])
			Browser.ViewMutatorMode = VMM_NoMutators;
		else if(t == MutatorModeStrings[2])
			Browser.ViewMutatorMode = VMM_ThisMutator;
		else if(t == MutatorModeStrings[3])
			Browser.ViewMutatorMode = VMM_NotThisMutator;

		UpdateMutatorVisibility();
	}
	else if(Sender == Controls[10])
		Browser.bDontShowWithBots = moCheckBox(Controls[10]).IsChecked();
	else if(Sender == Controls[11])
	{
		t = moComboBox(Controls[11]).GetText();

		if(t == WeaponStayStrings[0])
			Browser.WeaponStayServerView = WSSV_Any;
		else if(t == WeaponStayStrings[1])
			Browser.WeaponStayServerView = WSSV_OnlyWeaponStay;
		else if(t == WeaponStayStrings[2])
			Browser.WeaponStayServerView = WSSV_NoWeaponStay;
	}
	else if(Sender == Controls[12])
	{
		t = moComboBox(Controls[12]).GetText();

		if(t == TranslocatorStrings[0])
			Browser.TranslocServerView = TSV_Any;
		else if(t == TranslocatorStrings[1])
			Browser.TranslocServerView = TSV_OnlyTransloc;
		else if(t == TranslocatorStrings[2])
			Browser.TranslocServerView = TSV_NoTransloc;
	}
	else if(Sender == Controls[13])
	{
		t = moComboBox(Controls[13]).GetText();

		if(t == MutatorModeStrings[0])
			Browser.ViewMutator2Mode = VMM_AnyMutators;
		else if(t == MutatorModeStrings[2])
			Browser.ViewMutator2Mode = VMM_ThisMutator;
		else if(t == MutatorModeStrings[3])
			Browser.ViewMutator2Mode = VMM_NotThisMutator;

		UpdateMutatorVisibility();
	}
	else if(Sender == Controls[14])
	{
		Browser.DesiredMutator2 = moComboBox(Controls[14]).GetExtra();
	}
	else if(Sender == Controls[15])
	{
		if( moNumericEdit(Controls[15]).GetValue() < 0 )
			moNumericEdit(Controls[15]).SetValue( 0 );

		Browser.MinGamespeed = moNumericEdit(Controls[15]).GetValue();
	}
	else if(Sender == Controls[16])
	{
		if( moNumericEdit(Controls[16]).GetValue() > 200 )
			moNumericEdit(Controls[16]).SetValue( 200 );

		Browser.MaxGamespeed = moNumericEdit(Controls[16]).GetValue();
	}

	Browser.SaveConfig();
}

defaultproperties
{
	Begin Object Class=GUIButton Name=MyBackButton
		Caption="BACK"
		StyleName="SquareMenuButton"
		WinWidth=0.2
		WinLeft=0
		WinTop=0
		WinHeight=0.5
	End Object

	Begin Object class=GUITitleBar name=MyStatus
		WinWidth=1
		WinHeight=0.5
		WinLeft=0
		WinTop=0.5
		StyleName="SquareBar"
		Caption=""
		bUseTextHeight=false
		Justification=TXTA_Left
	End Object

	Begin Object Class=GUIButton Name=MyKeyButton
		Caption="ICON KEY"
		StyleName="SquareMenuButton"
		WinWidth=0.2
		WinLeft=0.2
		WinTop=0
		WinHeight=0.5
	End Object

	Begin Object Class=GUIPanel Name=FooterPanel
		Controls(0)=MyBackButton
		Controls(1)=MyStatus
		Controls(2)=MyKeyButton
		WinWidth=1
		WinHeight=0.1
		WinLeft=0
		WinTop=0.9
	End Object
	Controls(0)=GUIPanel'FooterPanel'

	Begin Object Class=GUILabel Name=FilterTitle
		WinWidth=0.720003
		WinHeight=0.056250
		WinLeft=0.1500000
		WinTop=0.05
		Caption="Server Filtering Options:"
		TextAlign=TXTA_Left
		TextColor=(R=230,G=200,B=0,A=255)
		TextFont="UT2HeaderFont"
	End Object
	Controls(1)=GUILabel'FilterTitle'

	Begin Object class=moCheckBox Name=NoPasswdCheckBox
		WinWidth=0.3400000
		WinHeight=0.040000
		WinLeft=0.05
		WinTop=0.21
		Caption="No Passworded Servers"
		INIOption="@Internal"
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(2)=moCheckbox'NoPasswdCheckBox'

	Begin Object class=moCheckBox Name=NoFullCheckBox
		WinWidth=0.3400000
		WinHeight=0.040000
		WinLeft=0.05
		WinTop=0.27
		Caption="No Full Servers"
		INIOption="@Internal"
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(3)=moCheckbox'NoFullCheckBox'

	Begin Object class=moCheckBox Name=NoEmptyCheckBox
		WinWidth=0.34
		WinHeight=0.04
		WinLeft=0.05
		WinTop=0.33
		Caption="No Empty Servers"
		INIOption="@Internal"
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(4)=moCheckbox'NoEmptyCheckBox'

	Begin Object class=moComboBox Name=StatsViewCombo
		WinWidth=0.76
		WinHeight=0.04
		WinLeft=0.05
		WinTop=0.39
		Caption="Stats Servers"
		INIOption="@INTERNAL"
		CaptionWidth=0.4
		ComponentJustification=TXTA_Left
	End Object
	Controls(5)=moComboBox'StatsViewCombo'

	Begin Object class=moComboBox Name=MutatorCombo
		WinWidth=0.308750
		WinHeight=0.060000
		WinLeft=0.675004
		WinTop=0.45
		Caption=""
		INIOption="@INTERNAL"
		CaptionWidth=0.0
		ComponentJustification=TXTA_Left
	End Object
	Controls(6)=moComboBox'MutatorCombo'

	Begin Object class=moEditBox Name=CustomQuery
		WinWidth=0.76
		WinHeight=0.04
		WinLeft=0.05
		WinTop=0.75
		Caption="Custom Query"
		INIOption="@INTERNAL"
		CaptionWidth=0.4
	End Object
	Controls(7)=moEditBox'CustomQuery'

	Begin Object class=moCheckBox Name=OnlyStandardCheckBox
		WinWidth=0.3400000
		WinHeight=0.040000
		WinLeft=0.05
		WinTop=0.15
		Caption="Only Standard Servers"
		INIOption="@Internal"
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(8)=moCheckbox'OnlyStandardCheckBox'

	Begin Object class=moComboBox Name=MutatorModeCombo
		WinWidth=0.61
		WinHeight=0.04
		WinLeft=0.05
		WinTop=0.45
		Caption="Mutators"
		INIOption="@INTERNAL"
		CaptionWidth=0.5
		ComponentJustification=TXTA_Left
	End Object
	Controls(9)=moComboBox'MutatorModeCombo'

	Begin Object class=moCheckBox Name=NoBotServersCheckBox
		WinWidth=0.34
		WinHeight=0.04
		WinLeft=0.05
		WinTop=0.39
		Caption="No Servers With Bots"
		INIOption="@Internal"
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
		bVisible=false
	End Object
	Controls(10)=moCheckbox'NoBotServersCheckBox'

	Begin Object class=moComboBox Name=WeaponStayCombo
		WinWidth=0.76
		WinHeight=0.04
		WinLeft=0.05
		WinTop=0.57
		Caption="WeaponStay"
		INIOption="@INTERNAL"
		CaptionWidth=0.4
		ComponentJustification=TXTA_Left
	End Object
	Controls(11)=moComboBox'WeaponStayCombo'

	Begin Object class=moComboBox Name=TranslocatorCombo
		WinWidth=0.76
		WinHeight=0.04
		WinLeft=0.05
		WinTop=0.63
		Caption="Translocator"
		INIOption="@INTERNAL"
		CaptionWidth=0.4
		ComponentJustification=TXTA_Left
	End Object
	Controls(12)=moComboBox'TranslocatorCombo'

	Begin Object class=moComboBox Name=MutatorModeCombo2
		WinWidth=0.61
		WinHeight=0.04
		WinLeft=0.05
		WinTop=0.51
		Caption=""
		INIOption="@INTERNAL"
		CaptionWidth=0.5
		ComponentJustification=TXTA_Left
	End Object
	Controls(13)=moComboBox'MutatorModeCombo2'

	Begin Object class=moComboBox Name=MutatorCombo2
		WinWidth=0.308750
		WinHeight=0.060000
		WinLeft=0.675004
		WinTop=0.51
		Caption=""
		INIOption="@INTERNAL"
		CaptionWidth=0.0
		ComponentJustification=TXTA_Left
	End Object
	Controls(14)=moComboBox'MutatorCombo2'

	Begin Object class=moNumericEdit Name=MinGamespeed
		WinWidth=0.433750
		WinHeight=0.060000
		WinLeft=0.05
		WinTop=0.69
		Caption="Game Speed Min"
		CaptionWidth=0.7
		MinValue=0
		MaxValue=200
		INIOption="@INTERNAL"
		ComponentJustification=TXTA_Left
	End Object
	Controls(15)=moNumericEdit'MinGamespeed'

	Begin Object class=moNumericEdit Name=MaxGamespeed
		WinWidth=0.235000
		WinHeight=0.060000
		WinLeft=0.557501
		WinTop=0.69
		Caption="Max"
		CaptionWidth=0.4
		MinValue=0
		MaxValue=200
		INIOption="@INTERNAL"
		ComponentJustification=TXTA_Left
	End Object
	Controls(16)=moNumericEdit'MaxGamespeed'

	ViewStatsStrings(0)="Any Servers"
	ViewStatsStrings(1)="Only Stats Servers"
	ViewStatsStrings(2)="No Stats Servers"

	WeaponStayStrings(0)="Any Servers"
	WeaponStayStrings(1)="Only Weapon Stay Servers"
	WeaponStayStrings(2)="No Weapon Stay Servers"

	TranslocatorStrings(0)="Any Servers"
	TranslocatorStrings(1)="Only Translocator Servers"
	TranslocatorStrings(2)="No Translocator Servers"

	MutatorModeStrings(0)="Any Mutators"
	MutatorModeStrings(1)="No Mutators"
	MutatorModeStrings(2)="This Mutator"
	MutatorModeStrings(3)="Not This Mutator"

}
