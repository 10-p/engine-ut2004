class Tab_SPLadderQualify extends Tab_SPLadderBase;

var array<LadderButton> DMButtons;

function InitPanel()
{
local int i;

	ScrollInfo=GUIScrollTextBox(Controls[4]);
	MapPicHolder=GUIImage(Controls[0]);
	MapNameLabel=GUILabel(Controls[3]);

	// Create DMButtons Array
	for (i=0; i<6; i++)
		DMButtons[DMButtons.Length] = NewLadderButton(0, i, 0.050195, 0.799215 - 0.128333 * i);

	OnProfileUpdated();
}

function OnProfileUpdated()
{
local int i;

	for (i=0; i<DMButtons.Length; i++)
		UpdateLadderButton(DMButtons[i], 0, i);

	SetYellowBar(0, 6, DMButtons);

	// When Profile is updated, display the top possible item;
	if (GetProfile() != None)
		SetActiveMatch(0, DMButtons);
	// Else
		// SetNoMatchInfo()
}

defaultproperties
{
	Begin Object Class=GUIImage Name=MapPicBack
		WinWidth=0.45
		WinHeight=0.41
		WinLeft=0.477891
		WinTop=0.091406
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Stretched
		Image=Material'InterfaceContent.Menu.BorderBoxA1'
	End Object

	Begin Object Class=GUIImage Name=MapPic
		WinWidth=0.443750
		WinHeight=0.405312
		WinLeft=0.481796
		WinTop=0.093489
		Image=material'InterfaceContent.Menu.NoLevelPreview'
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageStyle=ISTY_Scaled
		ImageRenderStyle=MSTY_Normal
	End Object

	Begin Object Class=GUIImage Name=MapInfoBack
		WinWidth=0.450000
		WinHeight=0.410000
		WinLeft=0.477891
		WinTop=0.521354
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Stretched
	End Object

	Begin Object Class=GUILabel Name=MapInfoName
		Caption="No Match Selected"
		TextALign=TXTA_Center
		TextFont="UT2HeaderFont"
		TextColor=(R=220,G=180,B=0,A=255)
		WinWidth=0.382813
		WinHeight=32.000000
		WinLeft=0.512304
		WinTop=0.526822
	End Object

	Begin Object Class=GUIScrollTextBox Name=MapInfoScroll
		WinWidth=0.435000
		WinHeight=0.300000
		WinLeft=0.484569
		WinTop=0.619325
		CharDelay=0.0025
		EOLDelay=0.5
		bNeverFocus=true
		StyleName="NoBackground"
	End Object

	Begin Object Class=GUIImage Name=DMBar1
		Image=Material'InterfaceContent.SPMenu.BarVertical'
		WinWidth=0.003906
		WinHeight=0.704102
		WinLeft=0.095312
		WinTop=0.152813
		ImageStyle=ISTY_Scaled
	End Object

	Begin Object Class=GUIImage Name=DMBar2
		Image=Material'InterfaceContent.SPMenu.BarVerticalHi'
		WinWidth=0.003906
		WinHeight=0.704102
		WinLeft=0.095312
		WinTop=0.152813
		ImageStyle=ISTY_Scaled
		bVisible=false
	End Object
	Begin Object Class=GUILabel Name=DMLabel0
		Caption="Tutorial"
		TextFont="UT2SmallFont"
		WinWidth=0.400000
		WinHeight=0.08
		WinLeft=0.175001
		WinTop=0.889387
		TextAlign=TXTA_Left
	End Object
	Begin Object Class=GUILabel Name=DMLabel1
		Caption="One on One"
		TextFont="UT2SmallFont"
		WinWidth=0.400000
		WinHeight=0.080000
		WinLeft=0.173438
		WinTop=0.723333
		TextAlign=TXTA_Left
	End Object
	Begin Object Class=GUILabel Name=DMLabel2
		Caption="One on One"
		TextFont="UT2SmallFont"
		WinWidth=0.400000
		WinHeight=0.080000
		WinLeft=0.173438
		WinTop=0.555416
		TextAlign=TXTA_Left
	End Object
	Begin Object Class=GUILabel Name=DMLabel3a
		Caption="Cut-throat"
		TextFont="UT2SmallFont"
		WinWidth=0.400000
		WinHeight=0.080000
		WinLeft=0.173438
		WinTop=0.366667
		TextAlign=TXTA_Left
	End Object
	Begin Object Class=GUILabel Name=DMLabel3b
		Caption="Deathmatch"
		TextFont="UT2SmallFont"
		WinWidth=0.400000
		WinHeight=0.080000
		WinLeft=0.173438
		WinTop=0.410416
		TextAlign=TXTA_Left
	End Object
	Begin Object Class=GUILabel Name=DMLabel4a
		Caption="Five-way"
		TextFont="UT2SmallFont"
		WinWidth=0.400000
		WinHeight=0.080000
		WinLeft=0.173438
		WinTop=0.201093
		TextAlign=TXTA_Left
	End Object
	Begin Object Class=GUILabel Name=DMLabel4b
		Caption="Deathmatch"
		TextFont="UT2SmallFont"
		WinWidth=0.400000
		WinHeight=0.080000
		WinLeft=0.173438
		WinTop=0.244270
		TextAlign=TXTA_Left
	End Object
	Begin Object Class=GUILabel Name=DMLabel5a
		Caption="Draft your team"
		TextFont="UT2SmallFont"
		WinWidth=0.400000
		WinHeight=0.080000
		WinLeft=0.171875
		WinTop=0.036927
		TextAlign=TXTA_Left
	End Object
	Begin Object Class=GUILabel Name=DMLabel5b
		Caption="then defeat them"
		TextFont="UT2SmallFont"
		WinWidth=0.400000
		WinHeight=0.080000
		WinLeft=0.173438
		WinTop=0.074219
		TextAlign=TXTA_Left
	End Object

	Controls(0)=GUIImage'MapPic'
	Controls(1)=GUIImage'MapPicBack'
	Controls(2)=GUIImage'MapInfoBack'
	Controls(3)=GUILabel'MapInfoName'
	Controls(4)=GUIScrollTextBox'MapInfoScroll'
	Controls(5)=GUIImage'DMBar1'
	Controls(6)=GUIImage'DMBar2'
	Controls(7)=GUILabel'DMLabel0'
	Controls(8)=GUILabel'DMLabel1'
	Controls(9)=GUILabel'DMLabel2'
	Controls(10)=GUILabel'DMLabel3a'
	Controls(11)=GUILabel'DMLabel3b'
	Controls(12)=GUILabel'DMLabel4a'
	Controls(13)=GUILabel'DMLabel4b'
	Controls(14)=GUILabel'DMLabel5a'
	Controls(15)=GUILabel'DMLabel5b'

	//Background=Material'InterfaceContent.Backgrounds.bg10'
	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.77
	bAcceptsInput=false		
	//bFillHeight=true;
}