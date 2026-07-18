class Browser_MOTD extends Browser_Page;

var MasterServerClient	MSC;
var String MOTD;
var GUIScrollTextBox MOTDTextBox;
var GUIButton UpgradeButton;
var bool MustUpgrade;
var bool GotMOTD;
var GUITitleBar StatusBar;

var float ReReadyPause;

var localized string VersionString;

event Timer()
{
	StatusBar.SetCaption(ReadyString);
}

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	Super.InitComponent(MyController, MyOwner);

	StatusBar = GUITitleBar(GUIPanel(Controls[1]).Controls[3]);
	StatusBar.SetCaption(ReadyString);

	MSC = PlayerOwner().Level.Spawn( class'MasterServerClient' );
	MSC.OnReceivedMOTDData = MyReceivedMOTDData;
	MSC.OnQueryFinished    = MyQueryFinished;

	MOTDTextBox = GUIScrollTextBox(Controls[0]);
	GUIButton(GUIPanel(Controls[1]).Controls[0]).OnClick=BackClick;
	GUIButton(GUIPanel(Controls[1]).Controls[1]).OnClick=RefreshClick;
	UpgradeButton = GUIButton(GUIPanel(Controls[1]).Controls[2]);
	UpgradeButton.OnClick=UpgradeClick;

	if( !GotMOTD )
	{
		UpgradeButton.bVisible = false;
		MustUpgrade = false;
		MSC.StartQuery(CTM_GetMOTD);

		StatusBar.SetCaption(StartQueryString);
		SetTimer(0, false); // Stop it going back to ready from a previous timer!
	}
	Controls[2].bBoundToParent=false;
	GUILabel(Controls[2]).Caption = "UT2004"@VersionString@PlayerOwner().Level.EngineVersion;
}

function MyReceivedMOTDData( MasterServerClient.EMOTDResponse Command, string Data )
{
	switch( Command )
	{
	case MR_MOTD:
		GotMOTD = true;
		MOTDTextBox.SetContent(Data, Chr(13));
		break;
	case MR_OptionalUpgrade:
		UpgradeButton.bVisible = true;
		break;
	case MR_MandatoryUpgrade:
		MustUpgrade = true;
		UpgradeButton.bVisible = true;
		break;
	case MR_NewServer:
		break;
	case MR_IniSetting:
		break;
	case MR_Command:
		break;
	}
}

function MyQueryFinished( MasterServerClient.EResponseInfo ResponseInfo, int Info )
{
	switch( ResponseInfo )
	{
	case RI_Success:
		StatusBar.SetCaption(QueryCompleteString);
		SetTimer(ReReadyPause, false);

		if( !MustUpgrade )
			Browser.MOTDVerified(true);
		break;
	case RI_AuthenticationFailed:
		StatusBar.SetCaption(AuthFailString);
		SetTimer(ReReadyPause, false);
		break;
	case RI_ConnectionFailed:
		StatusBar.SetCaption(ConnFailString);
		SetTimer(ReReadyPause, false);
		Browser.MOTDVerified(false);
		// try again
		MSC.StartQuery(CTM_GetMOTD);
		break;
	case RI_ConnectionTimeout:
		StatusBar.SetCaption(ConnTimeoutString);
		Browser.MOTDVerified(false);
		SetTimer(ReReadyPause, false);
		break;
	}
}

function OnCloseBrowser()
{
	if( MSC != None )
	{
		MSC.CancelPings();
		MSC.Destroy();
		MSC = None;
	}
}

// delegates
function bool BackClick(GUIComponent Sender)
{
	Controller.CloseMenu(true);
	return true;
}

function bool RefreshClick(GUIComponent Sender)
{
	MustUpgrade = false;
	UpgradeButton.bVisible = false;
	MSC.Stop();
	MSC.StartQuery(CTM_GetMOTD);

	StatusBar.SetCaption(StartQueryString);
	SetTimer(0, false);

	return true;
}

function bool UpgradeClick(GUIComponent Sender)
{
	MSC.LaunchAutoUpdate();
	return true;
}

defaultproperties
{
	Begin Object class=GUIScrollTextBox Name=MyMOTDText
		TextAlign=TXTA_Left
		WinWidth=0.9
		WinHeight=0.8
		WinLeft=0.05
		WinTop=0.048
		CharDelay=0.004
		EOLDelay=0.1
		bNeverFocus=true
		bAcceptsInput=true
	End Object
	Controls(0)=MyMOTDText

	Begin Object Class=GUIButton Name=MyBackButton
		Caption="BACK"
		StyleName="SquareMenuButton"
		WinWidth=0.2
		WinLeft=0
		WinTop=0
		WinHeight=0.5
	End Object

	Begin Object Class=GUIButton Name=MyRefreshButton
		Caption="REFRESH"
		StyleName="SquareMenuButton"
		WinWidth=0.2
		WinLeft=0.2
		WinTop=0
		WinHeight=0.5
	End Object

	Begin Object Class=GUIButton Name=MyUpgradeButton
		Caption="UPGRADE"
		StyleName="SquareMenuButton"
		WinWidth=0.2
		WinLeft=0.8
		WinTop=0
		WinHeight=0.5
		bVisible=false
	End Object

	Begin Object class=GUITitleBar name=MyStatus
		WinWidth=1
		WinHeight=0.5
		WinLeft=0
		WinTop=0.5
		StyleName="SquareBar"
		Caption=""
		bUseTextHeight=false
		Justification=TXTA_Left
	End Object

	Begin Object Class=GUIPanel Name=FooterPanel
		Controls(0)=MyBackButton
		Controls(1)=MyRefreshButton
		Controls(2)=MyUpgradeButton
		Controls(3)=MyStatus
		WinWidth=1
		WinHeight=0.1
		WinLeft=0
		WinTop=0.9
	End Object
	Controls(1)=FooterPanel

	Begin Object class=GUILabel Name=VersionNum
		Caption=""
		TextALign=TXTA_Right
		TextColor=(R=100,G=100,B=160,A=255)
		WinWidth=0.500000
		WinHeight=0.040000
		WinLeft=0.495000
		WinTop=0.0025
	End Object
	Controls(2)=GUILabel'VersionNum'

	ReReadyPause=2.0
	VersionString="Ver."
}
