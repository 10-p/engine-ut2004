// ====================================================================
//  Class:  XInterface.UT2StatsPrompt
//  Parent: XInterface.GUIMultiComponent
//
//  <Enter a description here>
// ====================================================================

class UT2StatsPrompt extends UT2K3GUIPage;

delegate OnStatsConfigured();

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	Super.InitComponent(Mycontroller, MyOwner);
	PlayerOwner().ClearProgressMessages();
}


function bool InternalOnClick(GUIComponent Sender)
{
	local	UT2SettingsPage	SettingsPage;
    local	GUITabControl Tabs;
    local 	moCheckBox Check;

	if(Sender == Controls[1])
	{
		Controller.OpenMenu("XInterface.UT2SettingsPage");
		SettingsPage = UT2SettingsPage(Controller.ActivePage);
		assert(SettingsPage != None);

        Tabs = GUITabControl(SettingsPage.Controls[1]);
		Tabs.ActivateTabByName(SettingsPage.NetworkTabLabel,true);

        Check = moCheckBox(SettingsPage.pNetwork.Controls[5]);
        if (Check!=None && (!Check.IsChecked()) )
        {
        	Check.Checked(true);
            SettingsPage.pNetwork.Controls[2].SetFocus(none);
        }

	}
	else
		Controller.CloseMenu(false);

	return true;
}

function ReOpen()
{
	if(Len(PlayerOwner().StatsUserName) >= 4 && Len(PlayerOwner().StatsPassword) >= 6)
	{
		Controller.CloseMenu();
		OnStatsConfigured();
	}
}

defaultproperties
{

	Begin Object Class=GUIButton name=PromptBackground
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
	Controls(0)=GUIButton'PromptBackground'

	Begin Object Class=GUIButton Name=YesButton
		Caption="YES"
		WinWidth=0.200000
		WinHeight=0.040000
		WinLeft=0.125000
		WinTop=0.81
		bBoundToParent=true
		OnClick=InternalOnClick
	End Object
	Controls(1)=GUIButton'YesButton'

	Begin Object Class=GUIButton Name=NoButton
		Caption="NO"
		WinWidth=0.2
		WinHeight=0.04
		WinLeft=0.65
		WinTop=0.81
		bBoundToParent=true
		OnClick=InternalOnClick
	End Object
	Controls(2)=GUIButton'NoButton'

	Begin Object class=GUILabel Name=PromptHeader
		Caption="This server has UT2003stats ENABLED!"
		TextAlign=TXTA_Center
		bMultiLine=True
		TextColor=(R=220,G=220,B=220,A=255)
		TextFont="UT2HeaderFont"
		WinWidth=1.000000
		WinHeight=0.051563
		WinLeft=0.000000
		WinTop=0.354166
	End Object
	Controls(3)=GUILabel'PromptHeader'

	Begin Object class=GUILabel Name=PromptDesc
		Caption="You will only be able to join this server by turning on \"Track Stats\" and setting a unique Stats Username and Password. Currently you will only be able to connect to servers with stats DISABLED.||Would you like to configure your Stats Username and Password now?"
		TextAlign=TXTA_Center
		bMultiLine=True
		TextColor=(R=220,G=180,B=0,A=255)
		TextFont="UT2MenuFont"
		WinWidth=1.000000
		WinHeight=0.256251
		WinLeft=0.000000
		WinTop=0.422917
	End Object
	Controls(4)=GUILabel'PromptDesc'

	WinLeft=0
	WinTop=0.325
	WinWidth=1
	WinHeight=0.325

	OnReOpen=ReOpen
}
