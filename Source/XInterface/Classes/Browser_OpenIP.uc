class Browser_OpenIP extends UT2K3GUIPage;

var moEditBox	MyIpAddress;
var	Browser_ServerListPageFavorites	MyFavoritesPage;

function InitComponent(GUIController pMyController, GUIComponent MyOwner)
{
	Super.InitComponent(pMyController, MyOwner);

	MyIpAddress = moEditBox(Controls[1]);
	MyIpAddress.MyEditBox.AllowedCharSet="0123456789.:";
}

function bool InternalOnClick(GUIComponent Sender)
{
	local GameInfo.ServerResponseLine S;
	local string address, ipString, portString;
	local int colonPos, portNum;

	if(Sender == Controls[4])
	{
		address = MyIpAddress.GetText();

		if(address == "")
			return true;

		// Parse text to find IP and possibly port number
		colonPos = InStr(address, ":");
		if(colonPos < 0)
		{
			// No colon
			ipString = address;
			portNum = 7777;
		}
		else
		{
			ipString = Left(address, colonPos);
			portString = Mid(address, colonPos+1);
			portNum = int(portString);
		}
		
		S.IP = ipString;
		S.Port = portNum;
		S.QueryPort = portNum+1;
		S.ServerName = "Unknown";

		MyFavoritesPage.MyAddFavorite(S);
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
	
	// EditBox with Limited CharSet for IP address
	Begin Object class=moEditBox Name=IpEntryBox
		Caption="IP Address: "
		LabelJustification=TXTA_Right
		LabelFont="UT2SmallFont"
		LabelColor=(R=255,G=255,B=255,A=255)
		CaptionWidth=0.55
		WinWidth=0.500000
		WinHeight=0.050000
		WinLeft=0.160000
		WinTop=0.466667
	End Object
	Controls(1)=moEditBox'IpEntryBox'

	Begin Object Class=GUIButton Name=CancelButton
		Caption="CANCEL"
		WinWidth=0.2
		WinHeight=0.04
		WinLeft=0.65
		WinTop=0.75
		bBoundToParent=true
		OnClick=InternalOnClick
	End Object
	Controls(2)=GUIButton'CancelButton'

	Begin Object class=GUILabel Name=OpenIPDesc
		Caption="Enter New IP Address"
		TextALign=TXTA_Center
		TextColor=(R=230,G=200,B=0,A=255)
		TextFont="UT2HeaderFont"
		WinWidth=1
		WinLeft=0
		WinTop=0.4
		WinHeight=32
	End Object
	Controls(3)=GUILabel'OpenIPDesc'

	Begin Object Class=GUIButton Name=OkButton
		Caption="OK"
		WinWidth=0.2
		WinHeight=0.04
		WinLeft=0.125
		WinTop=0.75
		bBoundToParent=true
		OnClick=InternalOnClick
	End Object
	Controls(4)=GUIButton'OkButton'


	WinLeft=0
	WinTop=0.375
	WinWidth=1
	WinHeight=0.25	
}
