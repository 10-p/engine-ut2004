// ====================================================================
// (C) 2002, Epic Games
// ====================================================================

class Tab_ServerInfo extends UT2K3TabPanel;

var bool bClean;
var GUIScrollTextBox MyScrollText;
var localized string DefaultText;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
 	Super.InitComponent(MyController, MyOwner);

    MyScrollText = GUIScrollTextBox(Controls[0]);

    WinWidth = Controller.ActivePage.WinWidth;
    WinHeight = Controller.ActivePage.WinHeight *0.7;
    WinLeft = Controller.ActivePage.WinLeft;
   	bClean = true;
   	MyScrollText.SetContent(DefaultText);

    if (XPlayer(PlayerOwner())!=None)
    {
	    XPlayer(PlayerOwner()).ProcessRule = ProcessRule;
        XPlayer(PlayerOwner()).ServerRequestRules();
    }
}

function ProcessRule(string NewRule)
{
	if (NewRule=="")
    {
    	bClean = true;
    	MyScrollText.SetContent(DefaultText);
    }
    else
    {
    	if (bClean)
        	MyScrollText.SetContent(NewRule);
        else
        	MyScrollText.AddText(NewRule);

        bClean = false;
    }
}

defaultproperties
{
	Begin Object Class=GUIScrollTextBox Name=InfoText
		WinWidth=1.000000
		WinHeight=0.834375
		WinLeft=0.000000
		WinTop=0.143750
		CharDelay=0.0025
		EOLDelay=0
		StyleName="NoBackground"
        bNoTeletype=true
        bNeverFocus=true
        TextAlign=TXTA_Center
        bBoundToParent=true
        bScaleToParent=true
	End Object
	Controls(0)=GUIScrollTextBox'InfoText'

	Begin Object class=GUIImage Name=ServerInfoBK1
		WinWidth=0.418437
		WinHeight=0.016522
		WinLeft=0.021641
		WinTop=0.070779
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
        bBoundToParent=true
        bScaleToParent=true
	End Object
	Controls(1)=GUIImage'ServerInfoBK1'

	Begin Object class=GUIImage Name=ServerInfoBK2
		WinWidth=0.395000
		WinHeight=0.016522
		WinLeft=0.576329
		WinTop=0.070779
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
        bBoundToParent=true
        bScaleToParent=true
	End Object
	Controls(2)=GUIImage'ServerInfoBK2'

	Begin Object class=GUILabel Name=ServerInfoLabel
		Caption="Rules"
		TextALign=TXTA_Center
		TextColor=(R=255,G=255,B=255,A=255)
		WinWidth=1.000000
		WinHeight=32.000000
		WinLeft=0.000000
		WinTop=0.027083
        bBoundToParent=true
        bScaleToParent=true
	End Object
	Controls(3)=GUILabel'ServerInfoLabel'
    WinWidth=1
    WinHeight=0.7
    DefaultText="Receiving Rules from Server...||This feature requires that the server be running the latest patch"
}
