class UT2Congratulations extends UT2K3GUIPage;

function SetupPage(string PageCaption, string PageMessage, string ContinueCaption, Material CongratsPic)
{
	GUITitleBar(Controls[0]).SetCaption(PageCaption);
	GUITitleBar(Controls[2]).SetCaption(PageMessage);
	GUIButton(Controls[3]).Caption = ContinueCaption; // For localization
	GUIImage(Controls[1]).Image = CongratsPic;
}

defaultproperties
{
	Begin Object Class=GUITitleBar name=CongratsHeader
		Caption="CONGRATULATIONS"
		Justification=TXTA_Center
		WinWidth=1.000000
		WinLeft=0.000000
		WinTop=0.02
		StyleName="Header"
	End Object

	Begin Object Class=GUIImage Name=CongratsPicture
		WinTop=0.1
		WinLeft=0.1
		WinWidth=0.8
		WinHeight=0.7
		ImageStyle=ISTY_Justified
		ImageAlign=IMGA_Center
	End Object

	Begin Object class=GUITitleBar name=CongratsFooter
		WinWidth=1.000000
		WinLeft=0.000000
		WinTop=0.86
		StyleName="Footer"
		Justification=TXTA_Center
	End Object

	Begin Object Class=GUIButton Name=BackButton
		Caption="Continue"
//		Hint="Cancel Changes and Return to Previous Menu"
//		OnClick=ButtonClick
		WinWidth=0.3
		WinHeight=0.04
		WinLeft=0.79
		WinTop=0.94
	End Object

	Controls(0)=GUITitleBar'CongratsHeader'
	Controls(1)=GUIImage'CongratsPicture'
	Controls(2)=GUITitleBar'CongratsFooter'
	Controls(3)=GUIButton'BackButton'

	WinWidth=0.8
	WinHeight=0.8
	WinTop=0.1
	WinLeft=0.1
}
