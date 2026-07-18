// ====================================================================
// (C) 2002, Epic Games
// ====================================================================

class IngameChat extends UT2K3GUIPage;

var ExtendedConsole MyConsole;
var GUIEditBox MyEditBox;
var bool bIgnoreChar;
var int OldCMC;
var byte CloseKey;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local string KeyName;

 	Super.InitComponent(MyController, MyOwner);

    MyEditBox = GUIEditBox(Controls[2]);
    MyEditBox.OnKeyEvent = InternalOnKeyEvent;
    MyEditBox.OnKeyType = InternalOnKeyType;
    OnClose = MyOnClose;

    GUIScrollTextBox(Controls[1]).MyScrollText.bNeverFocus=true;

    KeyName = PlayerOwner().ConsoleCommand("BINDINGTOKEY InGameChat");
    if (KeyName != "")
	    CloseKey = byte(PlayerOwner().ConsoleCommand("KEYNUMBER"@KeyName));
}

function Clear()
{
	local GUIScrollTextBox MyText;

    MyText =  GUIScrollTextBox(Controls[1]);
    MyText.MyScrollText.SetContent("");
}


event HandleParameters(string Param1, string Param2)
{
	bIgnoreChar = true;
	MyEditBox.SetFocus(none);
    OldCMC = PlayerOwner().MyHud.ConsoleMessageCount;
    PlayerOwner().Myhud.ConsoleMessageCount = 0;
}

function MyOnClose(optional Bool bCanceled)
{
 	if (MyConsole!=None)
    {
    	MyConsole.ChatMenu = None;
        MyConsole=None;
    }
    PlayerOwner().MyHud.ConsoleMessageCount = OldCMC;
    super.OnClose(bCanceled);
}

function bool InternalOnKeyType(out byte Key, optional string Unicode)
{
    if (bIgnorechar)
    {
    	bIgnoreChar=false;
        return true;
    }

    return MyEditBox.InternalOnKeyType(key, Unicode);
}

function bool InternalOnKeyEvent(out byte Key, out byte State, float delta)
{
	local string cmd;

	if (key == CloseKey && State == 1)
	{
		Controller.CloseMenu(false);
		return true;
	}

	if (key==0x0d && State==3)
    {
        if ( Left(MyEditBox.TextStr,1)=="/" )
        	cmd = right(MyEditBox.TextStr,len(MyEditBox.TextStr)-1);
        else if ( Left(MyEditBox.TextStr,1)=="." )
        	cmd = "teamsay"@right(MyEditBox.TextStr,len(MyEditBox.TextStr)-1);
        else
        	cmd = "say"@MyEditBox.TextStr;

    	PlayerOwner().ConsoleCommand(cmd);
        MyEditBox.TextStr="";
        return true;
    }

	return MyEditBox.InternalOnKeyEvent(key,state,delta);
}

defaultproperties
{
	Begin Object Class=GUIImage name=ChatBackground
		bAcceptsInput=false
		bNeverFocus=true
        Image=Material'InterfaceContent.Menu.SquareBoxA'
        ImageStyle=ISTY_Stretched
        WinWidth=1
        WinLeft=0
        WinHeight=1
        WinTop=0
        bBoundToParent=true
        bScaleToParent=true
	End Object
	Controls(0)=GUIImage'ChatBackground'

	Begin Object Class=GUIScrollTextBox Name=chatText
		WinWidth=0.94
		WinHeight=0.88
		WinLeft=0.03
		WinTop=0.02
		CharDelay=0.0025
		EOLDelay=0
        bBoundToParent=true
        bScaleToParent=true
		StyleName="NoBackground"
        bNoTeletype=true
        bNeverFocus=true
	End Object
	Controls(1)=GUIScrollTextBox'ChatText'

	Begin Object Class=GUIEditBox name=ChatEdit
		WinWidth=1
        WinLeft=0
        WinHeight=0.06
        WinTop=0.94
        bBoundToParent=true
        bScaleToParent=true
        StyleName="SquareButton"
	End Object
    Controls(2)=GUIEditBox'ChatEdit'

	bRequire640x480=false
	bAllowedAsLast=true
    WinLeft=0.1
    WinWidth=0.8
    WinTop=0.1
    WinHeight=0.8
	bPersistent=true
}
