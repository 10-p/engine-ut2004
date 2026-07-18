// ====================================================================
//  Class:  XInterface.Tab_InstantActionBaseRules
//  Parent: XInterface.GUITabPanel
//
//  <Enter a description here>
// ====================================================================

class Tab_InstantActionBaseRules extends UT2K3TabPanel;

var config  float	LastFriendlyFire;
var config 	bool	LastWeaponStay;
var config 	bool	LastTranslocator;

var config 	float	LastGameSpeed;
var config  int		LastGoalScore;
var config	int		LastTimeLimit;
var config	int		LastMaxLives;

var Config  bool  	bLastWeaponThrowing;

var	GUISlider		MyGameSpeed;

var moCheckBox		MyWeaponStay;
var moCheckBox		MyTranslocator;
var GUISlider 		MyFriendlyFire;

var moNumericEdit	MyGoalScore;
var	moNumericEdit	MyTimeLimit;
var	moNumericEdit	MyMaxLives;

var moCheckBox		MyWeaponThrowing;
var moCheckBox		MyBrightSkins;


var localized string	PercentText;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local int NewGameSpeed;

	Super.Initcomponent(MyController, MyOwner);

	MyGameSpeed=GUISlider(Controls[4]);
	MyWeaponStay=moCheckBox(Controls[5]);
	MyTranslocator=moCheckBox(Controls[6]);
	MyFriendlyFire=GUISlider(Controls[8]);

	MyGoalScore=moNumericEdit(Controls[9]);
	MyTimeLimit=moNumericEdit(Controls[10]);
	MyMAxLives=moNumericEdit(Controls[11]);

	NewGameSpeed = int(LastGameSpeed * 100);
	MyGameSpeed.Value = Clamp(NewGameSpeed,50,200);

	MyWeaponStay.Checked(LastWeaponStay);
	MyFriendlyFire.Value = LastFriendlyFire * 100;

	MyTranslocator.Checked(LastTranslocator);
	MyGoalScore.SetValue(LastGoalScore);
	MyTimeLimit.SetValue(LastTimeLimit);
	MyMaxLives.SetValue(LastMaxLives);

	MyGameSpeed.OnDrawCaption = InternalGameSpeedDraw;
	MyFriendlyFire.OnDrawCaption = InternalFriendlyFireDraw;

	Controls[4].FriendlyLabel = GUILabel(Controls[3]);
	Controls[8].FriendlyLabel = GUILabel(Controls[7]);

    MyWeaponThrowing = moCheckBox(Controls[12]); MyWeaponThrowing.Checked(bLastWeaponThrowing);
    MyBrightSkins	= moCheckBox(Controls[13]); MyBrightSkins.Checked(class'dmmutator'.default.bBrightSkins);


}

function string Play()
{
	local string url;

	LastGameSpeed = MyGameSpeed.Value / 100;
	LastWeaponStay = MyWeaponStay.IsChecked();
	LastTranslocator = MyTranslocator.IsChecked();
	LastFriendlyFire = MyFriendlyFire.Value/100;

	LastGoalScore = MyGoalScore.GetValue();
	LastTimeLimit = MyTimeLimit.GetValue();
	LastMaxLives  = MyMaxLives.GetValue();

    bLastWeaponThrowing = MyWeaponThrowing.IsChecked();


	SaveConfig();

	url = "?GameSpeed="$LastGameSpeed$"?WeaponStay="$LastWeaponStay$"?Translocator="$LastTranslocator$"?FriendlyFireScale="$LastFriendlyFire;
	url = url$"?GoalScore="$LastGoalScore$"?TimeLimit="$LastTimeLimit$"?MaxLives="$LastMAxLives;

    if (bLastWeaponThrowing)
    	URL=URL$"?AllowThrowing="$bLastWeaponThrowing;


	return url;
}

function string InternalGameSpeedDraw()
{
	return "("$int(MyGameSpeed.Value)@PercentText$")";
}

function string InternalFriendlyFireDraw()
{
	return "("$int(MyFriendlyFire.Value)@PercentText$")";
}

function BrightOnchange(GUIComponent Sender)
{

	class'dmmutator'.default.bBrightSkins = MyBrightSkins.IsChecked();
    class'dmmutator'.static.StaticSaveConfig();

}

defaultproperties
{

	Begin Object class=GUIImage Name=IARulesBK1
		WinWidth=0.957500
		WinHeight=0.156016
		WinLeft=0.021641
		WinTop=0.024687
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	Controls(0)=GUIImage'IARulesBK1'

	Begin Object class=GUIImage Name=IARulesBK2
		WinWidth=0.469219
		WinHeight=0.487071
		WinLeft=0.509922
		WinTop=0.239531
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	Controls(1)=GUIImage'IARulesBK2'

	Begin Object class=GUIImage Name=IARulesBK3
		WinWidth=0.469219
		WinHeight=0.487071
		WinLeft=0.019531
		WinTop=0.239531
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	Controls(2)=GUIImage'IARulesBK3'

	Begin Object class=GUILabel Name=IARulesGameSpeedLabel
		Caption="Game Speed"
		TextALign=TXTA_Center
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.500000
		WinHeight=32.000000
		WinLeft=0.250000
		WinTop=0.041406
		StyleName="TextLabel"
	End Object
	Controls(3)=GUILabel'IARulesGameSpeedLabel'

	Begin Object class=GUISlider Name=IARulesGameSpeedSlider
		WinWidth=0.250000
		WinLeft=0.375000
		WinTop=0.097552
		MinValue=50
		MaxValue=200
		Hint="This option controls how fast the game will be played."
	End Object
	Controls(4)=GUISlider'IARulesGameSpeedSlider'

	Begin Object class=moCheckBox Name=IARulesWeaponStay
		WinWidth=0.400000
		WinHeight=0.040000
		WinLeft=0.050000
		WinTop=0.315104
		Caption="Weapon Stay"
		Hint="When enabled, weapons will always be available for pickup."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(5)=moCheckBox'IARulesWeaponStay'

	Begin Object class=moCheckBox Name=IARulesTranslocator
		WinWidth=0.400000
		WinHeight=0.040000
		WinLeft=0.050000
		WinTop=0.428125
		Caption="Include Translocator"
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
		Hint="Enable this option to allow Translocators."
	End Object
	Controls(6)=moCheckBox'IARulesTranslocator'

	Begin Object class=GUILabel Name=IARulesFriendlyFireLabel
		Caption="FriendlyFire"
		TextALign=TXTA_Center
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=0.400000
		WinHeight=0.040000
		WinLeft=0.050000
		WinTop=0.540833
		StyleName="TextLabel"
	End Object
	Controls(7)=GUILabel'IARulesFriendlyFireLabel'

	Begin Object class=GUISlider Name=IARulesFriendlyFireSlider
		WinWidth=0.4
		WinHeight=0.04
		WinLeft=0.05
		WinTop=0.6
		MinValue=0
		MaxValue=200
		Hint="This option controls how much damage you do to teammates."
	End Object
	Controls(8)=GUISlider'IARulesFriendlyFireSlider'

	Begin Object class=moNumericEdit Name=IARulesGoalScore
		WinWidth=0.400000
		WinHeight=0.040000
		WinLeft=0.553906
		WinTop=0.315104
		Caption="Goal Score"
		CaptionWidth=0.7
		MinValue=0
		MaxValue=99
		ComponentJustification=TXTA_Left
		Hint="The game will end when this threshold is met."
	End Object
	Controls(9)=moNumericEdit'IARulesGoalScore'

	Begin Object class=moNumericEdit Name=IARulesTimeLimit
		WinWidth=0.400000
		WinHeight=0.040000
		WinLeft=0.553906
		WinTop=0.428125
		Caption="Time Limit"
		CaptionWidth=0.7
		MinValue=0
		MaxValue=3600
		ComponentJustification=TXTA_Left
		Hint="The game will end after this many minutes of play."
	End Object
	Controls(10)=moNumericEdit'IARulesTimeLimit'

	Begin Object class=moNumericEdit Name=IARulesMaxLives
		WinWidth=0.400000
		WinHeight=0.040000
		WinLeft=0.553906
		WinTop=0.547656
		Caption="Max Lives"
		CaptionWidth=0.7
		MinValue=0
		MaxValue=255
		ComponentJustification=TXTA_Left
		Hint="If this value is not 0, you will only respawn this many times."
	End Object
	Controls(11)=moNumericEdit'IARulesMaxLives'

	Begin Object class=moCheckBox Name=IARulesAllowWeaponThrow
		WinWidth=0.400000
		WinHeight=0.040000
		WinLeft=0.050000
		WinTop=0.370833
		Caption="Allow Weapon Throwing"
		Hint="When selected, a player will have the ability to throw out his current weapon."
		bSquare=true
		ComponentJustification=TXTA_Left
		CaptionWidth=0.9
	End Object
	Controls(12)=moCheckBox'IARulesAllowWeaponThrow'

	Begin Object class=moCheckBox Name=IARulesBrightSkins
		WinWidth=0.390626
		WinHeight=0.040000
		WinLeft=0.048242
		WinTop=0.479585
		Caption="Bright Skins"
		Hint="When selected, the server will cause the skins to be brighter than usual."
		bSquare=true
		ComponentJustification=TXTA_Left
        OnChange=BrightOnChange
		CaptionWidth=0.925
	End Object
	Controls(13)=moCheckBox'IARulesBrightSkins'




	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.77
	bAcceptsInput=false
	bLastWeaponThrowing=true
	PercentText="percent"
}
