class Browser_IRC extends Browser_Page;

var IRC_System			SystemPage;
var GUITabControl		ChannelTabs;
var localized string	SystemLabel;
var bool				bIrcIsInitialised;

var GUIButton			LeaveButton;
var GUIButton			BackButton;
var GUIButton			ChangeNickButton;
var localized string	LeaveChannelCaption;
var localized string	LeavePrivateCaptionHead;
var localized string	LeavePrivateCaptionTail;
var localized string	ChooseNewNickText;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	Super.InitComponent(MyController, MyOwner);

	// If not already initialised, create system page and add to tabs.
	if(!bIrcIsInitialised)
	{
		ChannelTabs = GUITabControl(Controls[1]);

		SystemPage = IRC_System( ChannelTabs.AddTab(SystemLabel, "xinterface.IRC_System", , , true) );
		SystemPage.IRCPage = self;

		ChannelTabs.OnChange = TabChange;

		SystemPage.SetCurrentChannel(-1); // Initially, System page is shown

		// Set up buttons
		BackButton = GUIButton(GUIPanel(Controls[0]).Controls[0]);
		BackButton.OnClick = BackClick;

		LeaveButton = GUIButton(GUIPanel(Controls[0]).Controls[1]);
		LeaveButton.bVisible = false;
		LeaveButton.OnClick = LeaveChannelClick;

		ChangeNickButton = GUIButton(GUIPanel(Controls[0]).Controls[2]);
		ChangeNickButton.OnClick = ChangeNickClick;

		bIrcIsInitialised=true;
	}
}

// delegates
function bool BackClick(GUIComponent Sender)
{
	Controller.CloseMenu(true);
	return true;		
} 

function bool ChangeNickClick(GUIComponent Sender)
{
	local IRC_NewNick NewNickDialog;

	if( SystemPage.Controller.OpenMenu("xinterface.IRC_NewNick") )
	{
		NewNickDialog = IRC_NewNick(SystemPage.Controller.TopPage());
		NewNickDialog.NewNickPrompt.Caption = ChooseNewNickText;
		NewNickDialog.SystemPage = SystemPage;
	}

	return true;
}

function TabChange(GUIComponent Sender)
{
	local GUITabButton TabButton;

	TabButton = GUITabButton(Sender);
		
	if ( TabButton == none )
		return;

	// If changed to system page - set channel to -1 (ie system)
	if( SystemPage == TabButton.MyPanel )
	{
		SystemPage.SetCurrentChannel(-1);
		LeaveButton.bVisible = false;
	}
	else
	{
		SystemPage.SetCurrentChannelPage( IRC_Channel( TabButton.MyPanel ) );

		// Set caption
		if( IRC_Private( TabButton.MyPanel ) != None)
			LeaveButton.Caption = LeavePrivateCaptionHead$Caps(TabButton.Caption)$LeavePrivateCaptionTail;
		else
			LeaveButton.Caption = LeaveChannelCaption;

		LeaveButton.bVisible = true;
	}

	TabButton.bForceFlash=false; // Stop any 'forced' flashing when we switch to a tab
}

function bool LeaveChannelClick(GUIComponent Sender)
{
	SystemPage.PartCurrentChannel();
	return true;		
}

defaultproperties
{
	Begin Object Class=GUIButton Name=MyBackButton
		Caption="BACK"
		StyleName="SquareMenuButton"
		WinWidth=0.2
		WinLeft=0
		WinTop=0
		WinHeight=1
	End Object

	Begin Object Class=GUIButton Name=MyLeaveButton
		Caption=""
		StyleName="SquareMenuButton"
		WinWidth=0.2
		WinLeft=0.4
		WinTop=0
		WinHeight=1
	End Object

	Begin Object Class=GUIButton Name=MyChangeNickButton
		Caption="CHANGE NICK"
		StyleName="SquareMenuButton"
		WinWidth=0.2
		WinLeft=0.2
		WinTop=0
		WinHeight=1
	End Object

	Begin Object Class=GUIPanel Name=FooterPanel
		Controls(0)=MyBackButton
		Controls(1)=MyLeaveButton
		Controls(2)=MyChangeNickButton
		WinWidth=1
		WinHeight=0.05
		WinLeft=0
		WinTop=0.95
	End Object
	Controls(0)=GUIPanel'FooterPanel'

	Begin Object Class=GUITabControl Name=ChannelTabControl
		WinWidth=1.0
		WinLeft=0
		WinTop=0
		WinHeight=48
		TabHeight=0.04
		bFillSpace=False
		bAcceptsInput=true
		bDockPanels=true
	End Object
	Controls(1)=GUITabControl'ChannelTabControl'

	LeaveChannelCaption="LEAVE CHANNEL"
	LeavePrivateCaptionHead="CLOSE "
	LeavePrivateCaptionTail=""
	SystemLabel="System"
	ChooseNewNickText="Choose A New Chat Nickname"
}