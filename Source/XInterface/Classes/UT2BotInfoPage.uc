class UT2BotInfoPage extends UT2K3GUIPage;

var localized string NoInformation;
var GUIImage BotPortrait;
var GUILabel BotName, BotRace;
var array<GUIProgressBar> Bars;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	Super.Initcomponent(MyController, MyOwner);
	BotPortrait=GUIImage(Controls[1]);
	BotName=GUILabel(Controls[3]);
	BotRace=GUILabel(Controls[4]);
	Bars[0]=GUIProgressBar(Controls[5]);
	Bars[1]=GUIProgressBar(Controls[6]);
	Bars[2]=GUIProgressBar(Controls[7]);
	Bars[3]=GUIProgressBar(Controls[8]);
	Bars[4]=GUIProgressBar(Controls[9]);
}

function SetupBotInfo(Material Portrait, string DecoTextName, xUtil.PlayerRecord PRE)
{
	// Setup the Portrait from here
	BotPortrait.Image = PRE.Portrait;
	// Setup the decotext from here
	BotName.Caption = PRE.DefaultName;
	BotRace.Caption = PRE.Species.default.SpeciesName$" - "$class'XUtil'.static.GetFavoriteWeaponFor(PRE);

	Bars[0].Value=class'XUtil'.static.AccuracyRating(PRE);
	Bars[1].Value=class'XUtil'.static.AgilityRating(PRE);
	Bars[2].Value=class'XUtil'.static.TacticsRating(PRE);
	Bars[3].Value=class'XUtil'.static.AccuracyRating(PRE);
}


function bool OkClick(GUIComponent Sender)
{
	Controller.CloseMenu(false);
	return true;
}

defaultproperties
{
	Begin Object class=GUIImage Name=PageBack
		Image=Material'InterfaceContent.Menu.EditBoxDown'
		ImageStyle=ISTY_Stretched
		WinWidth=0.890625
		WinHeight=1.000000
		WinLeft=0.062500
		WinTop=0.000000
		bScaleToParent=true
		bBoundToParent=true
	End Object

	Begin Object class=GUIImage Name=BotPortraitBorder
		WinWidth=0.253125
		WinHeight=0.664258
		WinLeft=0.076563
		WinTop=0.168751
		Image=Material'InterfaceContent.Menu.BorderBoxA1'
		ImageColor=(R=255,G=255,B=255,A=255);
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Stretched
	End Object

	Begin Object class=GUIImage Name=imgBotPic
		WinWidth=0.246875
		WinHeight=0.658008
		WinLeft=0.078125
		WinTop=0.170834
		ImageRenderStyle=MSTY_Normal
		ImageStyle=ISTY_Justified
	End Object

	Begin Object class=GUILabel Name=lblName
		WinWidth=0.598437
		WinHeight=0.052539
		WinLeft=0.333008
		WinTop=0.175781
		TextAlign=TXTA_Center
		TextColor=(R=153,B=253,G=216,A=255)
		Caption="Unknown"
		TextFont="UT2MenuFont"
	End Object

	Begin Object class=GUILabel Name=lblRace
		WinWidth=0.609180
		WinHeight=0.047656
		WinLeft=0.332031
		WinTop=0.231771
		TextAlign=TXTA_Center
		TextColor=(R=153,B=253,G=216,A=255)
		Caption="Unknown"
		TextFont="UT2SmallFont"
	End Object

	Begin Object class=GUIProgressBar Name=myPB
		Caption="Accuracy"
		BarColor=(R=0,G=160,B=0,A=255)
		Value=45.0
		WinWidth=0.600000
		WinHeight=0.062500
		WinLeft=0.335938
		WinTop=0.329167
		FontName="UT2SmallFont"
	End Object

	Begin Object class=GUIProgressBar Name=myPB2
		Caption="Agility"
		BarColor=(R=0,G=160,B=0,A=255)
		Value=20.0
		WinWidth=0.600000
		WinHeight=0.062500
		WinLeft=0.335938
		WinTop=0.410417
		FontName="UT2SmallFont"
	End Object

	Begin Object class=GUIProgressBar Name=myPB3
		Caption="Tactics"
		BarColor=(R=0,G=160,B=0,A=255)
		Value=50.0
		WinWidth=0.600000
		WinHeight=0.062500
		WinLeft=0.335938
		WinTop=0.491667
		FontName="UT2SmallFont"
	End Object

	Begin Object class=GUIProgressBar Name=myPB4
		Caption="Aggressiveness"
		BarColor=(R=0,G=160,B=0,A=255)
		Value=75.0
		WinWidth=0.600000
		WinHeight=0.062500
		WinLeft=0.335938
		WinTop=0.572917
		FontName="UT2SmallFont"
	End Object

	Begin Object class=GUIProgressBar Name=myPB5
		Caption="..."
		BarColor=(R=0,G=160,B=0,A=255)
		Value=70.0
		WinWidth=0.600000
		WinHeight=0.062500
		WinLeft=0.335938
		WinTop=0.654167
		FontName="UT2SmallFont"
		bVisible=false
	End Object

	Begin Object class=GUIButton Name=OkButton
		WinWidth=0.237500
		WinHeight=0.060938
		WinLeft=0.703125
		WinTop=0.762501
		Caption="OK"
		OnClick=OkClick
	End Object

	Controls(0)=PageBack
	Controls(1)=imgBotPic
	Controls(2)=BotPortraitBorder
	Controls(3)=lblName
	Controls(4)=lblRace
	Controls(5)=myPB
	Controls(6)=myPB2
	Controls(7)=myPB3
	Controls(8)=myPB4
	Controls(9)=myPB5
	Controls(10)=OkButton
	WinWidth=1.0
	WinHeight=0.7
	WinTop=0.15
	WinLeft=0.0

	NoInformation="No Information Available!"
}
