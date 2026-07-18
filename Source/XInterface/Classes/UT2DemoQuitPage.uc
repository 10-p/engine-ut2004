// ====================================================================
//  Class:  XInterface.UT2DemoQuitPage
//  Parent: XInterface.GUIPage
//
//  <Enter a description here>
// ====================================================================

class UT2DemoQuitPage extends UT2K3GUIPage;

#exec OBJ LOAD FILE=ExitScreen.utx

var int	TimeLeft;
var bool bClickedBuy;  // need a tick or two for the start to take place

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	Super.Initcomponent(MyController, MyOwner);
	GUIButton(Controls[17]).Caption = GUIButton(default.Controls[17]).Caption$"("$TimeLeft$")";
	SetTimer(1,true);
}

event Timer()
{

	if ( bClickedBuy )
	{
		PlayerOwner().ConsoleCommand("exit");
	}
	else
	{
		TimeLeft--;
		GUIButton(Controls[17]).Caption = GUIButton(default.Controls[17]).Caption$"("$TimeLeft$")";
		if(TimeLeft <= 0)
			OnQuitClicked(Controls[17]);
	}
}

function bool OnBuyClicked(GUIComponent Sender)
{
	PlayerOwner().ConsoleCommand("start http://www.unrealtournament.com/");
	bClickedBuy = true;
	SetTimer(0.5,true);
	return true;
}

function bool OnQuitClicked(GUIComponent Sender)
{
	PlayerOwner().ConsoleCommand("exit");
	return true;
}

defaultproperties
{
	Begin Object class=GUIImage name=QuitImageA1
		Image=Material'ExitScreen.splash.QuitA1'
		ImageStyle=ISTY_Scaled
		WinTop=0.0
		WinLeft=0.0
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(0)=GUIImage'QuitImageA1'

	Begin Object class=GUIImage name=QuitImageA2
		Image=Material'ExitScreen.splash.QuitA2'
		ImageStyle=ISTY_Scaled
		WinTop=0.0
		WinLeft=0.25
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(1)=GUIImage'QuitImageA2'

	Begin Object class=GUIImage name=QuitImageA3
		Image=Material'ExitScreen.splash.QuitA3'
		ImageStyle=ISTY_Scaled
		WinTop=0.0
		WinLeft=0.5
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(2)=GUIImage'QuitImageA3'

	Begin Object class=GUIImage name=QuitImageA4
		Image=Material'ExitScreen.splash.QuitA4'
		ImageStyle=ISTY_Scaled
		WinTop=0.0
		WinLeft=0.75
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(3)=GUIImage'QuitImageA4'

	Begin Object class=GUIImage name=QuitImageB1
		Image=Material'ExitScreen.splash.QuitB1'
		ImageStyle=ISTY_Scaled
		WinTop=0.25
		WinLeft=0.0
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(4)=GUIImage'QuitImageB1'

	Begin Object class=GUIImage name=QuitImageB2
		Image=Material'ExitScreen.splash.QuitB2'
		ImageStyle=ISTY_Scaled
		WinTop=0.25
		WinLeft=0.25
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(5)=GUIImage'QuitImageB2'

	Begin Object class=GUIImage name=QuitImageB3
		Image=Material'ExitScreen.splash.QuitB3'
		ImageStyle=ISTY_Scaled
		WinTop=0.25
		WinLeft=0.5
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(6)=GUIImage'QuitImageB3'

	Begin Object class=GUIImage name=QuitImageB4
		Image=Material'ExitScreen.splash.QuitB4'
		ImageStyle=ISTY_Scaled
		WinTop=0.25
		WinLeft=0.75
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(7)=GUIImage'QuitImageB4'

	Begin Object class=GUIImage name=QuitImageC1
		Image=Material'ExitScreen.splash.QuitC1'
		ImageStyle=ISTY_Scaled
		WinTop=0.5
		WinLeft=0.0
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(8)=GUIImage'QuitImageC1'

	Begin Object class=GUIImage name=QuitImageC2
		Image=Material'ExitScreen.splash.QuitC2'
		ImageStyle=ISTY_Scaled
		WinTop=0.5
		WinLeft=0.25
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(9)=GUIImage'QuitImageC2'

	Begin Object class=GUIImage name=QuitImageC3
		Image=Material'ExitScreen.splash.QuitC3'
		ImageStyle=ISTY_Scaled
		WinTop=0.5
		WinLeft=0.5
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(10)=GUIImage'QuitImageC3'

	Begin Object class=GUIImage name=QuitImageC4
		Image=Material'ExitScreen.splash.QuitC4'
		ImageStyle=ISTY_Scaled
		WinTop=0.5
		WinLeft=0.75
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(11)=GUIImage'QuitImageC4'

	Begin Object class=GUIImage name=QuitImageD1
		Image=Material'ExitScreen.splash.QuitD1'
		ImageStyle=ISTY_Scaled
		WinTop=0.75
		WinLeft=0.0
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(12)=GUIImage'QuitImageD1'

	Begin Object class=GUIImage name=QuitImageD2
		Image=Material'ExitScreen.splash.QuitD2'
		ImageStyle=ISTY_Scaled
		WinTop=0.75
		WinLeft=0.25
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(13)=GUIImage'QuitImageD2'

	Begin Object class=GUIImage name=QuitImageD3
		Image=Material'ExitScreen.splash.QuitD3'
		ImageStyle=ISTY_Scaled
		WinTop=0.75
		WinLeft=0.5
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(14)=GUIImage'QuitImageD3'

	Begin Object class=GUIImage name=QuitImageD4
		Image=Material'ExitScreen.splash.QuitD4'
		ImageStyle=ISTY_Scaled
		WinTop=0.75
		WinLeft=0.75
		WinWidth=0.25
		WinHeight=0.25
	End Object
	Controls(15)=GUIImage'QuitImageD4'

	Begin Object class=GUIButton name=BuyButton
		Caption="Buy"
		WinWidth=0.15
		WinHeight=0.04
		WinLeft=0.8
		WinTop=0.85
		OnClick=OnBuyClicked
	End Object
	Controls(16)=GUIButton'BuyButton'

	Begin Object class=GUIButton name=QuitButton
		Caption="Quit"
		WinWidth=0.15
		WinLeft=0.8
		WinTop=0.91
		WinHeight=0.04
		OnClick=OnQuitClicked
	End Object
	Controls(17)=GUIButton'QuitButton'

	TimeLeft=20
	bClickedBuy=false

	Background=Material'InterfaceContent.Backgrounds.bg09'
	WinWidth=1.0
	WinHeight=1.0
	WinTop=0.0
	WinLeft=0.0
}

