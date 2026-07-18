class Browser_AddBuddy extends UT2K3GUIPage;

var moEditBox	MyNewBuddy;
var	Browser_ServerListPageBuddy	MyBuddyPage;

function InitComponent(GUIController pMyController, GUIComponent MyOwner)
{
	Super.InitComponent(pMyController, MyOwner);

	MyNewBuddy = moEditBox(Controls[1]);
}

function bool InternalOnClick(GUIComponent Sender)
{
	local string buddyName;

	if(Sender == Controls[4])
	{
		buddyName = MyNewBuddy.GetText();

		if(buddyName == "")
			return true;

		MyBuddyPage.Buddies.Length = MyBuddyPage.Buddies.Length + 1;
		MyBuddyPage.Buddies[MyBuddyPage.Buddies.Length - 1] = buddyName;
		MyBuddyPage.MyBuddyList.ItemCount = MyBuddyPage.Buddies.Length;
		MyBuddyPage.SaveConfig();
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
	Begin Object class=moEditBox Name=BuddyEntryBox
		Caption="Buddy Name: "
		LabelJustification=TXTA_Right
		LabelFont="UT2SmallFont"
		LabelColor=(R=255,G=255,B=255,A=255)
		CaptionWidth=0.55
		WinWidth=0.500000
		WinHeight=0.050000
		WinLeft=0.160000
		WinTop=0.466667
	End Object
	Controls(1)=moEditBox'BuddyEntryBox'

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

	Begin Object class=GUILabel Name=AddBuddyDesc
		Caption="Add Buddy"
		TextALign=TXTA_Center
		TextColor=(R=230,G=200,B=0,A=255)
		TextFont="UT2HeaderFont"
		WinWidth=1
		WinLeft=0
		WinTop=0.4
		WinHeight=32
	End Object
	Controls(3)=GUILabel'AddBuddyDesc'
	
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
