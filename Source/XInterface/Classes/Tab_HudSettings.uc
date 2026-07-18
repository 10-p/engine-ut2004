// ====================================================================
//  Class:  XInterface.Tab_DetailSettings
//  Parent: XInterface.GUITabPanel
//
//  <Enter a description here>
// ====================================================================

class Tab_HudSettings extends UT2K3TabPanel;

var localized string	CrosshairNames[15];

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{

	local int i;
	Super.Initcomponent(MyController, MyOwner);

	for (i=0;i<Controls.Length;i++)
		Controls[i].OnChange=InternalOnChange;
}

function InternalOnLoadINI(GUIComponent Sender, string s)
{

	if (Sender==Controls[2])
    	moCheckBox(Sender).Checked(class'HUD'.Default.bHideHud);

	else if (Sender==Controls[3])
    	moCheckBox(Sender).Checked(class'HUD'.Default.bShowWeaponInfo);

    else if (Sender==Controls[4])
    	moCheckBox(Sender).Checked(class'HUD'.Default.bShowPersonalInfo);

    else if (Sender==Controls[5])
    	moCheckBox(Sender).Checked(Class'HUD'.Default.bShowPoints);

    else if (Sender==Controls[6])
    	moCheckBox(Sender).Checked(Class'HUD'.Default.bShowWeaponBar);

    else if (Sender==Controls[7])
    	moCheckBox(Sender).Checked(Class'HUD'.Default.bShowPortrait);

	else if (Sender==Controls[15])
    	moCheckBox(Sender).Checked(!Class'HUD'.Default.bNoEnemyNames);

    else if (Sender==Controls[8])
    	moNumericEdit(Sender).SetValue(Class'HUD'.DEfault.ConsoleMessageCount);

    else if (Sender==Controls[9])
    	moNumericEdit(Sender).SetValue(8-Class'HUD'.Default.ConsoleFontSize);

    else if (Sender==Controls[10])
    	moNumericEdit(Sender).SetValue(Class'HUD'.Default.MessageFontOffset+4);

    else if (Sender==Controls[12])
     	GUISlider(Sender).Value = Class'HUD'.Default.HudScale*100;

    else if (Sender==Controls[14])
   		GUISlider(Sender).Value = (Class'HUD'.Default.HudOpacity/255)*100;
}

function string InternalOnSaveINI(GUIComponent Sender); 		// Do the actual work here

function InternalOnChange(GUIComponent Sender)
{
	if (!Controller.bCurMenuInitialized)
		return;

	if (Sender==Controls[2])
    	PlayerOwner().MyHud.bHideHud = moCheckBox(Sender).IsChecked();

	else if (Sender==Controls[3])
		PlayerOwner().MyHud.bShowWeaponInfo = moCheckBox(Sender).IsChecked();

	else if (Sender==Controls[4])
		PlayerOwner().MyHud.bShowPersonalInfo = moCheckBox(Sender).IsChecked();

	else if (Sender==Controls[5])
		PlayerOwner().MyHud.bShowPoints = moCheckBox(Sender).IsChecked();

	else if (Sender==Controls[6])
		PlayerOwner().MyHud.bShowWeaponBar = moCheckBox(Sender).IsChecked();

	else if (Sender==Controls[7])
		PlayerOwner().MyHud.bShowPortrait = moCheckBox(Sender).IsChecked();

	else if (Sender==Controls[15])
    	PlayerOwner().MyHud.bNoEnemyNames = !moCheckBox(Sender).Ischecked();

	else if (Sender==Controls[8])
		PlayerOwner().MyHud.ConsoleMessageCount = moNumericEdit(Sender).GetValue();

	else if (Sender==Controls[9])
		PlayerOwner().MyHud.ConsoleFontSize = abs(moNumericEdit(Sender).GetValue()-8);

	else if (Sender==Controls[10])
		PlayerOwner().MyHud.MessageFontOffset = moNumericEdit(Sender).GetValue()-4;

	else if (Sender==Controls[12])
		PlayerOwner().MyHud.HudScale = GUISlider(Sender).Value / 100;

	else if (Sender==Controls[14])
		PlayerOwner().MyHud.HudOpacity = (GUISlider(Sender).Value/100) * 255;

	PlayerOwner().MyHud.SaveConfig();

}

defaultproperties
{

	Begin Object class=GUIImage Name=GameBK
		WinWidth=0.427148
		WinHeight=0.803125
		WinLeft=0.029297
		WinTop=0.130208
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	Controls(0)=GUIImage'GameBK'

	Begin Object class=GUIImage Name=GameBK1
		WinWidth=0.448633
		WinHeight=0.803125
		WinLeft=0.517578
		WinTop=0.130208
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	Controls(1)=GUIImage'GameBK1'

	Begin Object class=moCheckBox Name=GameHudVisible
		WinWidth=0.196875
		WinHeight=0.040000
		WinLeft=0.379297
		WinTop=0.043906
		Caption="Hide HUD"
		INIOption="@Internal"
		OnLoadINI=InternalOnLoadINI
		Hint="This option will toggle drawing of the HUD."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(2)=moCheckbox'GameHudVisible'

	Begin Object class=moCheckBox Name=GameHudShowWeaponInfo
		WinWidth=0.378125
		WinHeight=0.040000
		WinLeft=0.050000
		WinTop=0.181927
		Caption="Show Weapon Info"
		Hint=""
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
		OnLoadINI=InternalOnLoadINI
		INIOption="@Internal"
	End Object
	Controls(3)=moCheckbox'GameHudShowWeaponInfo'

	Begin Object class=moCheckBox Name=GameHudShowPersonalInfo
		WinWidth=0.378125
		WinHeight=0.040000
		WinLeft=0.050000
		WinTop=0.317343
		Caption="Show Personal Info"
		Hint=""
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
		OnLoadINI=InternalOnLoadINI
		INIOption="@Internal"
	End Object
	Controls(4)=moCheckbox'GameHudShowPersonalInfo'

   	Begin Object class=moCheckBox Name=GameHudShowScore
		WinWidth=0.378125
		WinHeight=0.040000
		WinLeft=0.050000
		WinTop=0.452760
		Caption="Show Score"
		Hint=""
		CaptionWidth=0.9
		bSquare=true
		OnLoadINI=InternalOnLoadINI
		INIOption="@Internal"
		ComponentJustification=TXTA_Left
	End Object
	Controls(5)=moCheckbox'GameHudShowScore'

	Begin Object class=moCheckBox Name=GameHudShowWeaponBar
		WinWidth=0.378125
		WinHeight=0.040000
		WinLeft=0.050000
		WinTop=0.598593
		Caption="Show Weapon Bar"
		Hint=""
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
		OnLoadINI=InternalOnLoadINI
		INIOption="@Internal"
	End Object
	Controls(6)=moCheckbox'GameHudShowWeaponBar'

	Begin Object class=moCheckBox Name=GameHudShowPortraits
		WinWidth=0.378125
		WinHeight=0.040000
		WinLeft=0.050000
		WinTop=0.723594
		Caption="Show Portraits"
		Hint=""
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
		OnLoadINI=InternalOnLoadINI
		INIOption="@Internal"
	End Object
	Controls(7)=moCheckbox'GameHudShowPortraits'

	Begin Object class=moNumericEdit Name=GameHudMessageCount
		WinWidth=0.381250
		WinHeight=0.060000
		WinLeft=0.550781
		WinTop=0.196875
		Caption="Max. Chat Count"
		CaptionWidth=0.7
		MinValue=0
		MaxValue=8
		ComponentJustification=TXTA_Left
		Hint=""
		OnLoadINI=InternalOnLoadINI
		INIOption="@Internal"
	End Object
	Controls(8)=moNumericEdit'GameHudMessageCount'

	Begin Object class=moNumericEdit Name=GameHudMessageScale
		WinWidth=0.381250
		WinHeight=0.060000
		WinLeft=0.550781
		WinTop=0.321874
		Caption="Chat Font Size"
		CaptionWidth=0.7
		MinValue=0
		MaxValue=8
		ComponentJustification=TXTA_Left
		Hint=""
		OnLoadINI=InternalOnLoadINI
		INIOption="@Internal"
	End Object
	Controls(9)=moNumericEdit'GameHudMessageScale'

	Begin Object class=moNumericEdit Name=GameHudMessageOffset
 		WinWidth=0.381250
		WinHeight=0.060000
		WinLeft=0.550781
		WinTop=0.436457
		Caption="Message Font Size"
		CaptionWidth=0.7
		MinValue=0
		MaxValue=4
		ComponentJustification=TXTA_Left
		Hint=""
		OnLoadINI=InternalOnLoadINI
		INIOption="@Internal"
	End Object
	Controls(10)=moNumericEdit'GameHudMessageOffset'

	Begin Object class=GUILabel Name=GameHudScaleLabel
		Caption="HUD Scaling"
		TextALign=TXTA_Center
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.448438
		WinHeight=32.000000
		WinLeft=0.516602
		WinTop=0.560417
	End Object
	Controls(11)=GUILabel'GameHudScaleLabel'

	Begin Object class=GUISlider Name=GameHudScale
		WinWidth=0.292187
		WinLeft=0.590626
		WinTop=0.635312
		MinValue=50
		MaxValue=100
		OnLoadINI=InternalOnLoadINI
		INIOption="@Internal"
		Hint="Changes the opacity level of the HUD."
	End Object
	Controls(12)=GUISlider'GameHudScale'

	Begin Object class=GUILabel Name=GameHudOpacityLabel
		Caption="HUD Opacity"
		TextALign=TXTA_Center
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.448438
		WinHeight=32.000000
		WinLeft=0.516602
		WinTop=0.737500
	End Object
	Controls(13)=GUILabel'GameHudOpacityLabel'

	Begin Object class=GUISlider Name=GameHudOpacity
		WinWidth=0.290625
		WinLeft=0.592189
		WinTop=0.808230
		MinValue=0
		MaxValue=100
		OnLoadINI=InternalOnLoadINI
		INIOption="@Internal"
		Hint="Changes the opacity level of the HUD."
	End Object
	Controls(14)=GUISlider'GameHudOpacity'

	Begin Object class=moCheckBox Name=GameHudShowEnemyNames
		WinWidth=0.378125
		WinHeight=0.040000
		WinLeft=0.050000
		WinTop=0.848594
		Caption="Show Enemy Names"
		Hint=""
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
		OnLoadINI=InternalOnLoadINI
		INIOption="@Internal"
	End Object
	Controls(15)=moCheckbox'GameHudShowEnemyNames'


 	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.74
	bAcceptsInput=false

	CrosshairNames(0)="Hidden"
	CrosshairNames(1)="Cross (1)"
	CrosshairNames(2)="Cross (2)"
	CrosshairNames(3)="Cross (3)"
	CrosshairNames(4)="Cross (4)"
	CrosshairNames(5)="Cross (5)"
	CrosshairNames(6)="Dot"
	CrosshairNames(7)="Pointer"
	CrosshairNames(8)="Triad (1)"
	CrosshairNames(9)="Triad (2)"
	CrosshairNames(10)="Triad (3)"
	CrosshairNames(11)="Bracket (1)"
	CrosshairNames(12)="Bracket (2)"
	CrosshairNames(13)="Circle (1)"
	CrosshairNames(14)="Circle (2)"
}
