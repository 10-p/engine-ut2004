class UT2GetPassword extends UT2K3GUIPage;


var bool bIgnoreEsc;

var		localized string LeaveMPButtonText;
var		localized string LeaveSPButtonText;

var		float ButtonWidth;
var		float ButtonHeight;
var		float ButtonHGap;
var		float ButtonVGap;
var		float BarHeight;
var		float BarVPos;

var string	RetryURL;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	Super.InitComponent(Mycontroller, MyOwner);
	PlayerOwner().ClearProgressMessages();

	moEditBox(Controls[1]).MyEditBox.bConvertSpaces = true;


}

function HandleParameters(string URL,string Unused)
{
	RetryURL = URL;
}

function bool InternalOnClick(GUIComponent Sender)
{
	local ExtendedConsole	MyConsole;

	if(Sender==Controls[2] && Len(moEditbox(Controls[1]).GetText()) > 0) // Retry
	{
		MyConsole = ExtendedConsole(PlayerOwner().Player.Console);

		if(MyConsole != None)
		{
			MyConsole.SavedPasswords.Length = MyConsole.SavedPasswords.Length + 1;
			MyConsole.SavedPasswords[MyConsole.SavedPasswords.Length - 1].Server = MyConsole.LastConnectedServer;
			MyConsole.SavedPasswords[MyConsole.SavedPasswords.Length - 1].Password = moEditbox(Controls[1]).GetText();
			MyConsole.SaveConfig();
		}

		PlayerOwner().ClientTravel(RetryURL$"?password="$moEditbox(Controls[1]).GetText(),TRAVEL_Absolute,false);
		Controller.CloseAll(false);
	}
	if(Sender==Controls[3]) // Fail
	{
		Controller.ReplaceMenu("xinterface.UT2MainMenu");
	}

	return true;
}

defaultproperties
{
	Begin Object Class=GUIButton name=GetPassBackground
		bAcceptsInput=false
		bNeverFocus=true
		StyleName="RoundButton"
		WinWidth=0.831251
		WinHeight=0.306250
		WinLeft=0.087500
		WinTop=0.375000
	End Object
	Controls(0)=GUIButton'GetPassBackground'

	Begin Object class=moEditBox Name=GetPassPW
		WinWidth=0.500000
		WinHeight=0.060000
		WinLeft=0.250000
		WinTop=0.508594
		Caption="Server Password"
		CaptionWidth=0.4
	End Object
	Controls(1)=moEditBox'GetPassPW'

	Begin Object Class=GUIButton Name=GetPassRetry
		Caption="Retry"
		StyleName="MidGameButton"
		OnClick=InternalOnClick
		bBoundToParent=true
		WinWidth=0.250000
		WinHeight=0.050000
		WinLeft=0.400000
		WinTop=0.941666
	End Object
	Controls(2)=GUIButton'GetPassRetry'

	Begin Object Class=GUIButton Name=GetPassFail
		Caption="Cancel"
		StyleName="MidGameButton"
		OnClick=InternalOnClick
		bBoundToParent=true
		WinWidth=0.250000
		WinHeight=0.050000
		WinLeft=0.650000
		WinTop=0.941666
	End Object
	Controls(3)=GUIButton'GetPassFail'

	Begin Object Class=GUILabel Name=GetPassLabel
		Caption="A password is required to play on this server."
		TextFont="UT2HeaderFont"
		TextALign=TXTA_Center
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=1
		WinHeight=0.5
		WinLeft=0
		WinTop=0.125
		bMultiLine=true
		bBoundToParent=true
	End Object
	Controls(4)=GUILabel'GetPassLabel'

	OpenSound=sound'MenuSounds.SelectDshort'

	bIgnoreEsc=true
	bAllowedAsLast=true
	WinTop=0.375;
	WinHeight=0.25;
	WinWidth=1.0;
	WinLeft=0.0;


}
