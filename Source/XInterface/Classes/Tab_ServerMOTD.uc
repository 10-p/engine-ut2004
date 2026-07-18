// ====================================================================
// (C) 2002, Epic Games
// ====================================================================

class Tab_ServerMOTD extends UT2K3TabPanel;

var GUIScrollTextBox MyScrollText;
var moEditBox AdminName, AdminEmail;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local string MOTDString;

 	Super.InitComponent(MyController, MyOwner);

    MyScrollText = GUIScrollTextBox(Controls[0]);
    AdminName  = moEditbox(Controls[1]);
    AdminEmail = moEditBox(Controls[2]);

	MOTDString = PlayerOwner().GameReplicationInfo.MessageOfTheDay;
	MyScrollText.SetContent(MOTDString);

    AdminName.SetText(PlayerOwner().GameReplicationInfo.AdminName);
    AdminEmail.SetText(PlayerOwner().GameReplicationInfo.AdminEmail);

    WinWidth = Controller.ActivePage.WinWidth;
    WinHeight = Controller.ActivePage.WinHeight *0.7;
    WinLeft = Controller.ActivePage.WinLeft;
}

defaultproperties
{
	Begin Object Class=GUIScrollTextBox Name=MOTDText
		WinWidth=1.000000
		WinHeight=0.540625
		WinLeft=0.000000
		WinTop=0.441667
		CharDelay=0.0025
		EOLDelay=0
		StyleName="NoBackground"
        bNoTeletype=true
        bNeverFocus=true
        TextAlign=TXTA_Center
        bBoundToParent=true
        bScaleToParent=true
	End Object
	Controls(0)=GUIScrollTextBox'MOTDText'

	Begin Object class=moEditBox Name=ServerAdminName
		WinWidth=0.900000
		WinHeight=0.060000
		WinLeft=0.050000
		WinTop=0.064583
		Caption="Admin Name:"
		Hint="The owner of the server"
		CaptionWidth=0.33
        bBoundToParent=true
        bScaleToParent=true
        bReadOnly=true
	End Object
	Controls(1)=moEditBox'ServerAdminName'

	Begin Object class=moEditBox Name=ServerAdminEmail
		WinWidth=0.900000
		WinHeight=0.060000
		WinLeft=0.050000
		WinTop=0.153333
		Caption="      Email:"
		Hint="How to contact the owner"
		CaptionWidth=0.33
        bBoundToParent=true
        bScaleToParent=true
        bReadOnly=true
	End Object
	Controls(2)=moEditBox'ServerAdminEmail'

	Begin Object class=GUIImage Name=ServerBK1
		WinWidth=0.293437
		WinHeight=0.016522
		WinLeft=0.021641
		WinTop=0.352029
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
        bBoundToParent=true
        bScaleToParent=true
	End Object
	Controls(3)=GUIImage'ServerBK1'

	Begin Object class=GUIImage Name=ServerBK2
		WinWidth=0.277812
		WinHeight=0.016522
		WinLeft=0.685704
		WinTop=0.352029
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
        bBoundToParent=true
        bScaleToParent=true
	End Object
	Controls(4)=GUIImage'ServerBK2'

	Begin Object class=GUILabel Name=ServerMOTDLabel
		Caption="Message of the Day"
		TextALign=TXTA_Center
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=1.000000
		WinHeight=32.000000
		WinLeft=0.000000
		WinTop=0.308333
        bBoundToParent=true
        bScaleToParent=true
	End Object
	Controls(5)=GUILabel'ServerMOTDLabel'
    WinWidth=1
    WinHeight=0.7


}
