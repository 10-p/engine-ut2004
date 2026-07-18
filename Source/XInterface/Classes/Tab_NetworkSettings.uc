// ====================================================================
//  Class:  XInterface.Tab_OnlineSettings
//  Parent: XInterface.GUITabPanel
//
//  <Enter a description here>
// ====================================================================

class Tab_NetworkSettings extends UT2K3TabPanel;


var localized string	NetSpeedText[4];

var localized string	StatsURL;
var localized string    EpicIDMsg;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{

	local int i;
	Super.Initcomponent(MyController, MyOwner);

	for (i=0;i<Controls.Length;i++)
		Controls[i].OnChange=InternalOnChange;

	for(i = 0;i < ArrayCount(NetSpeedText);i++)
		moComboBox(Controls[1]).AddItem(NetSpeedText[i]);

	moEditBox(Controls[2]).MyEditBox.bConvertSpaces = true;
	moEditBox(Controls[2]).MyEditBox.MaxWidth=14;

	moEditBox(Controls[3]).MyEditBox.MaxWidth=14;

	moEditBox(Controls[3]).MaskText(true);
	moEditBox(Controls[2]).MenuStateChange(MSAT_Disabled);
	moEditBox(Controls[3]).MenuStateChange(MSAT_Disabled);

	GUILabel(Controls[8]).Caption = EpicIDMsg@FormatID(PlayerOwner().GetPlayerIDHash());

    moCheckBox(Controls[9]).Checked(PlayerOwner().bDynamicNetSpeed);

}

function string FormatID(string id)
{
    id=Caps(id);
	return mid(id,0,8)$"-"$mid(id,8,8)$"-"$mid(id,16,8)$"-"$mid(id,24,8);
}

function InternalOnLoadINI(GUIComponent Sender, string s)
{
	local int i;

	if (Sender==Controls[1])
	{
		i = class'Player'.default.ConfiguredInternetSpeed;
		if (i<=2600)
			moComboBox(Sender).SetText(NetSpeedText[0]);
		else if (i<=5000)
			moComboBox(Sender).SetText(NetSpeedText[1]);
		else if (i<=10000)
			moComboBox(Sender).SetText(NetSpeedText[2]);
		else
			moComboBox(Sender).SetText(NetSpeedText[3]);
	}
	else if (Sender==Controls[2])
	{
		if(PlayerOwner().StatsUserName!="" && PlayerOwner().StatsPassword!="")
		{
			moEditBox(Sender).SetText(PlayerOwner().StatsUserName);
			moEditBox(Sender).MenuStateChange(MSAT_Blurry);
		}
		else
		{
			moEditBox(Sender).SetText("");
			moEditBox(Sender).MenuStateChange(MSAT_Disabled);
		}
	}
	else if (Sender==Controls[3])
	{
		if(PlayerOwner().StatsUserName!="" && PlayerOwner().StatsPassword!="")
		{
			moEditBox(Sender).SetText(PlayerOwner().StatsPassword);
			moEditBox(Sender).MenuStateChange(MSAT_Blurry);
		}
		else
		{
			moEditBox(Sender).SetText("");
			moEditBox(Sender).MenuStateChange(MSAT_Disabled);
		}
	}
	else if (Sender==Controls[5])
	{
		GUICheckBoxButton(GUIMenuOption(Sender).MyComponent).SetChecked( PlayerOwner().StatsUserName!="" && PlayerOwner().StatsPassword!="" );
	}

	Controls[7].bVisible = !ValidStatConfig();
}

function bool ValidStatConfig()
{
	if(moCheckBox(Controls[5]).IsChecked())
	{
		if(Len(moEditBox(Controls[2]).GetText()) < 4)
			return false;

		if(Len(moEditBox(Controls[3]).GetText()) < 6)
			return false;
	}

	return true;
}

function ApplyStatConfig()
{
	if(moCheckBox(Controls[5]).IsChecked())
	{
		PlayerOwner().StatsUserName = moEditBox(Controls[2]).GetText();
		PlayerOwner().StatsPassword = moEditBox(Controls[3]).GetText();
	}
	else
	{
		PlayerOwner().StatsUserName = moEditBox(Controls[2]).GetText();
		PlayerOwner().StatsPassword = moEditBox(Controls[3]).GetText();
	}
	PlayerOwner().SaveConfig();
}

function InternalOnChange(GUIComponent Sender)
{
	local GUIMenuOption O;

	if (!Controller.bCurMenuInitialized)
		return;

	if (Sender==Controls[1])
	{
		if (moComboBox(Sender).GetText()==NetSpeedText[0])
			PlayerOwner().ConsoleCommand("netspeed 2600");
		else if (moComboBox(Sender).GetText()==NetSpeedText[1])
			PlayerOwner().ConsoleCommand("netspeed 5000");
		else if (moComboBox(Sender).GetText()==NetSpeedText[2])
			PlayerOwner().ConsoleCommand("netspeed 10000");
		else if (moComboBox(Sender).GetText()==NetSpeedText[3])
			PlayerOwner().ConsoleCommand("netspeed 20000");
	}
	else
	if (Sender==Controls[5])
	{
		O = GUIMenuOption(Sender);
		if ( !GUICheckBoxButton(O.MyComponent).bChecked )
		{
			moEditBox(Controls[2]).SetText("");
			moEditBox(Controls[3]).SetText("");
			moEditBox(Controls[2]).MenuStateChange(MSAT_Disabled);
			moEditBox(Controls[3]).MenuStateChange(MSAT_Disabled);
		}
		else
		{
			moEditBox(Controls[2]).MenuStateChange(MSAT_Blurry);
			moEditBox(Controls[3]).MenuStateChange(MSAT_Blurry);
		}
	}

    else if (Sender==Controls[9])
    {
    	PlayerOwner().bDynamicNetSpeed = moCheckBox(Controls[9]).IsChecked();
        PlayerOwner().Saveconfig();
    }

	Controls[7].bVisible = !ValidStatConfig();
}

function bool OnViewStats(GUIComponent Sender)
{
	PlayerOwner().ConsoleCommand("start"@StatsURL);
	return true;
}

defaultproperties
{
	Begin Object class=GUIImage Name=OnlineBK1
		WinWidth=0.576640
		WinHeight=0.489999
		WinLeft=0.216797
		WinTop=0.355208
		Image=Material'InterfaceContent.Menu.BorderBoxD'
		ImageColor=(R=255,G=255,B=255,A=160);
		ImageRenderStyle=MSTY_Alpha
		ImageStyle=ISTY_Stretched
	End Object
	Controls(0)=GUIImage'OnlineBK1'

	Begin Object class=moComboBox Name=OnlineNetSpeed
		WinWidth=0.500000
		WinHeight=0.060000
		WinLeft=0.250000
		WinTop=0.085678
		Caption="Connection"
		INIOption="@Internal"
		INIDefault="Cable Modem/DSL"
		OnLoadINI=InternalOnLoadINI
		Hint="How fast is your connection?"
		CaptionWidth=0.4
		ComponentJustification=TXTA_Left
		bReadOnly=true
	End Object
	Controls(1)=GUIMenuOption'OnlineNetSpeed'


	Begin Object class=moEditBox Name=OnlineStatsName
		WinWidth=0.500000
		WinHeight=0.060000
		WinLeft=0.250000
		WinTop=0.494271
		INIOption="@Internal"
		OnLoadINI=InternalOnLoadINI
		Caption="Stats UserName"
		Hint="Please select a name to use for UT Stats!"
		CaptionWidth=0.4
	End Object
	Controls(2)=moEditBox'OnlineStatsName'

	Begin Object class=moEditBox Name=OnlineStatsPW
		WinWidth=0.500000
		WinHeight=0.060000
		WinLeft=0.250000
		WinTop=0.583594
		Caption="Stats Password"
		INIOption="@Internal"
		OnLoadINI=InternalOnLoadINI
		Hint="Please select a password that will secure your UT Stats!"
		CaptionWidth=0.4
	End Object
	Controls(3)=moEditBox'OnlineStatsPW'

	Begin Object class=GUILabel Name=OnlineStatDesc
		Caption="UT2003 Global Stats"
		TextALign=TXTA_Center
		TextFont="UT2HeaderFont"
		TextColor=(R=230,G=200,B=0,A=255)
		WinWidth=0.500000
		WinHeight=32.000000
		WinLeft=0.250000
		WinTop=0.391145
	End Object
	Controls(4)=GUILabel'OnlineStatDesc'

	Begin Object class=moCheckBox Name=OnlineTrackStats
		WinWidth=0.210000
		WinHeight=0.040000
		WinLeft=0.251565
		WinTop=0.742708
		Caption="Track Stats"
		INIOption="@Internal"
		INIDefault="True"
		OnLoadINI=InternalOnLoadINI
		Hint="Enable this option to join the online ranking system."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(5)=moCheckBox'OnlineTrackStats'

	Begin Object class=GUIButton Name=ViewOnlineStats
		WinWidth=0.210000
		WinHeight=0.040000
		WinLeft=0.536721
		WinTop=0.742708
		Caption="View Stats"
		Hint="Click to launch the UT stats website."
		OnClick=OnViewStats
	End Object
	Controls(6)=GUIButton'ViewOnlineStats'

	Begin Object class=GUILabel Name=InvalidWarning
		WinWidth=0.576561
		WinHeight=0.125001
		WinLeft=0.215625
		WinTop=0.870832
		Caption="Your stats username or password is invalid.  Your username must be at least 4 characters long, and your password must be at least 6 characters long."
		TextAlign=TXTA_Center
		bMultiLine=True
		TextColor=(R=255,G=0,B=0,A=255)
		TextFont="UT2MenuFont"
	End Object
	Controls(7)=GUILabel'InvalidWarning'

	Begin Object class=GUILabel Name=EpicID
		WinWidth=1
		WinHeight=0.066407
		WinLeft=0
		WinTop=0.313279
		Caption="Your Unique id is:"
		TextAlign=TXTA_Center
		bMultiLine=True
		TextColor=(R=255,G=255,B=0,A=255)
		TextFont="UT2SmallFont"
	End Object
	Controls(8)=GUILabel'EpicID'

	Begin Object class=moCheckBox Name=NetworkDynamicNetspeed
		WinWidth=0.500000
		WinHeight=0.040000
		WinLeft=0.250000
		WinTop=0.179011
		Caption="Dynamic Netspeed"
		Hint="Dynamic adjust your netspeed on slower connections."
		CaptionWidth=0.9
		bSquare=true
		ComponentJustification=TXTA_Left
	End Object
	Controls(9)=moCheckBox'NetworkDynamicNetspeed'

	WinTop=0.15
	WinLeft=0
	WinWidth=1
	WinHeight=0.74
	bAcceptsInput=false

	NetSpeedText(0)="Modem"
	NetSpeedText(1)="ISDN"
	NetSpeedText(2)="Cable/ADSL"
	NetSpeedText(3)="LAN/T1"

	StatsURL="http://ut2003stats.epicgames.com/"
    EpicIDMsg="Your Unique id is:"
}
