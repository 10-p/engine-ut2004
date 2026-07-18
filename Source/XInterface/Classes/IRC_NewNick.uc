class IRC_NewNick extends UT2K3GUIPage;

var moEditBox	MyNewNick;

var	IRC_System	SystemPage;
var GUILabel	NewNickPrompt;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	Super.InitComponent(MyController, MyOwner);

	MyNewNick = moEditBox(Controls[1]);
	NewNickPrompt = GUILabel(Controls[2]);

	MyNewNick.SetText("");
}

function bool InternalOnClick(GUIComponent Sender)
{
	local string NewNick;

	if(Sender == Controls[3])
	{
		NewNick = MyNewNick.GetText();

		if(NewNick == "")
			return true;

		Log("NewNick "$NewNick);
		SystemPage.Link.SendCommandText("NICK "$NewNick);
	}

	Controller.CloseMenu(false);

	return true;
}
		

defaultproperties
{

	Begin Object Class=GUIButton name=VidOKBackground
		WinWidth=1.0
		WinHeight=1.0
		WinTop=0
		WinLeft=0
		bAcceptsInput=false
		bNeverFocus=true
		StyleName="SquareBar"
		bBoundToParent=true
		bScaleToParent=true
	End Object
	Controls(0)=GUIButton'VidOKBackground'
	
	Begin Object class=moEditBox Name=NewNickEntry
		Caption="New Nickname: "
		LabelJustification=TXTA_Right
		LabelFont="UT2SmallFont"
		LabelColor=(R=255,G=255,B=255,A=255)
		CaptionWidth=0.55
		WinWidth=0.500000
		WinHeight=0.050000
		WinLeft=0.160000
		WinTop=0.466667
	End Object
	Controls(1)=moEditBox'NewNickEntry'

	Begin Object class=GUILabel Name=NickMesg
		Caption=""
		TextALign=TXTA_Center
		TextColor=(R=230,G=200,B=0,A=255)
		TextFont="UT2HeaderFont"
		WinWidth=1
		WinLeft=0
		WinTop=0.4
		WinHeight=32
	End Object
	Controls(2)=GUILabel'NickMesg'
	
	Begin Object Class=GUIButton Name=OkButton
		Caption="OK"
		WinWidth=0.2
		WinHeight=0.04
		WinLeft=0.4
		WinTop=0.75
		bBoundToParent=true
		OnClick=InternalOnClick
	End Object
	Controls(3)=GUIButton'OkButton'

	WinLeft=0
	WinTop=0.375
	WinWidth=1
	WinHeight=0.25	
}
