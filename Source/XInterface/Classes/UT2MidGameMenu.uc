class UT2MidGameMenu extends UT2K3GUIPage;

var bool bIgnoreEsc;

var		localized string LeaveMPButtonText;
var		localized string LeaveSPButtonText;
var		localized string LeaveEntryButtonText;

var		float ButtonWidth;
var		float ButtonHeight;
var		float ButtonHGap;
var		float ButtonVGap;
var		float BarHeight;
var		float BarVPos;

function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local int i;

	Super.InitComponent(MyController, MyOwner);

	OnKeyEvent = InternalOnKeyEvent;
	OnClose = InternalOnClose;

	// Bar
	Controls[0].WinHeight = BarHeight;
	Controls[0].WinWidth = 1.0;
	Controls[0].WinTop = BarVPos - (0.5 * BarHeight);
	Controls[0].Winleft = 0.0;

	// U MIDDLE
	Controls[1].WinTop = BarVPos - ButtonVGap - (1.5 * ButtonHeight);
	Controls[1].WinLeft = 0.5 - (0.5 * ButtonWidth);

	// B L
	Controls[2].WinTop = BarVPos - (0.5 * ButtonHeight);
	Controls[2].WinLeft = 0.5 - (1.5 * ButtonWidth) - ButtonHGap;

	// U L
	Controls[3].WinTop = Controls[1].WinTop;
	Controls[3].WinLeft = Controls[2].WinLeft;

	// U R
	Controls[4].WinTop = Controls[1].WinTop;
	Controls[4].WinLeft = 0.5 + (0.5 * ButtonWidth) + ButtonHGap;

	// B R
	Controls[5].WinTop = Controls[2].WinTop;
	Controls[5].WinLeft = Controls[4].WinLeft;

	// B MID
	Controls[6].WinTop = Controls[2].WinTop;
	Controls[6].WinLeft = Controls[1].WinLeft;

	// VB L
	Controls[7].WinTop = BarVPos + ButtonVGap + (0.5 * ButtonHeight);
	Controls[7].WinLeft = Controls[1].WinLeft;

	for(i=1; i<8; i++)
	{
		Controls[i].WinWidth = ButtonWidth;
		Controls[i].WinHeight = ButtonHeight;
	}


	// If its not a team game, or it's a SP ladder match - dont show the 'Change Teams' button
	Controls[5].bVisible = PlayerOwner().GameReplicationInfo.bTeamGame;
	if ( PlayerOwner().Level.Game !=None &&PlayerOwner().Level.Game.CurrentGameProfile != none )
	{
		Controls[5].bVisible = false;
	}

	// Set 'leave' button text depending on if we are SP or MP
	if( PlayerOwner().Level.NetMode != NM_StandAlone )
		GUIButton(Controls[3]).Caption =  LeaveMPButtonText;
	else
		GUIButton(Controls[3]).Caption =  LeaveSPButtonText;

	// Only show 'Add Favorite' button if we are a client
	if( PlayerOwner().Level.NetMode == NM_Client && !CurrentServerIsInFavorites() )
		Controls[6].bVisible = true;
	else
		Controls[6].bVisible = false;
}

// See if we already have this server in our favorites
function bool CurrentServerIsInFavorites()
{
	local string address, ipString, portString;
	local int colonPos, portNum, i;

	// Get current network address
	address = PlayerOwner().GetServerNetworkAddress();

	if(address == "")
		return true; // slightly hacky - dont want to add "none"!

	// Parse text to find IP and possibly port number
	colonPos = InStr(address, ":");
	if(colonPos < 0)
	{
		// No colon - assume port 7777
		ipString = address;
		portNum = 7777;
	}
	else
	{	// Parse out port number
		ipString = Left(address, colonPos);
		portString = Mid(address, colonPos+1);
		portNum = int(portString);
	}

	for(i=0; i<class'Browser_ServerListPageFavorites'.default.Favorites.Length; i++ )
	{
		if(	class'Browser_ServerListPageFavorites'.default.Favorites[i].IP == ipString &&
			class'Browser_ServerListPageFavorites'.default.Favorites[i].Port == portNum )
			return true;
	}

	return false;
}



function bool InternalOnKeyEvent(out byte Key, out byte State, float delta)
{
	// Swallow first escape key event (key up from key down that opened menu)
	if(bIgnoreEsc && Key == 0x1B)
	{
		bIgnoreEsc = false;
		return true;
	}
}

function InternalOnClose(optional Bool bCanceled)
{
	local PlayerController pc;

	pc = PlayerOwner();

	// Turn pause off if currently paused
	if(pc != None && pc.Level.Pauser != None)
		pc.SetPause(false);

	Super.OnClose(bCanceled);
}

function bool InternalOnClick(GUIComponent Sender)
{

	if(Sender==Controls[2]) // QUIT
	{
		Controller.OpenMenu("xinterface.UT2QuitPage");
	}
	else if(Sender==Controls[3]) // LEAVE/DISCONNECT
	{
		PlayerOwner().ConsoleCommand( "DISCONNECT" );
	    if ( PlayerOwner().Level.Game.CurrentGameProfile != None )
		{
			PlayerOwner().Level.Game.CurrentGameProfile.ContinueSinglePlayerGame(PlayerOwner().Level, true);  // replace menu
		}
		else
			Controller.CloseMenu();
	}
	else if(Sender==Controls[1]) // CONTINUE
	{
		Controller.CloseMenu(); // Close _all_ menus
	}
	else if(Sender==Controls[4]) // SETTINGS
	{
		Controller.OpenMenu("xinterface.UT2SettingsPage");
	}
	else if(Sender==Controls[5] && Controls[5].bVisible) // CHANGE TEAM
	{
        PlayerOwner().SwitchTeam();
		Controller.CloseMenu();
	}
	else if(Sender==Controls[6] && Controls[6].bVisible) // ADD FAVORITE
	{
		PlayerOwner().ConsoleCommand( "ADDCURRENTTOFAVORITES" );
		Controller.CloseMenu();
	}
	else if(Sender==Controls[7]) // SERVER BROWSER
	{
		Controller.OpenMenu("xinterface.ServerBrowser");
	}

	return true;
}

defaultproperties
{

	Begin Object Class=GUIButton name=QuitBackground
		bAcceptsInput=false
		bNeverFocus=true
		StyleName="SquareBar"
	End Object
	Controls(0)=GUIButton'QuitBackground'

	Begin Object Class=GUIButton Name=ContMatchButton
		Caption="CONTINUE"
		StyleName="MidGameButton"
		OnClick=InternalOnClick
	End Object
	Controls(1)=GUIButton'ContMatchButton'

	Begin Object Class=GUIButton Name=QuitGameButton
		Caption="EXIT UT2003"
		StyleName="MidGameButton"
		OnClick=InternalOnClick
	End Object
	Controls(2)=GUIButton'QuitGameButton'

	Begin Object Class=GUIButton Name=LeaveMatchButton
		Caption=""
		StyleName="MidGameButton"
		OnClick=InternalOnClick
	End Object
	Controls(3)=GUIButton'LeaveMatchButton'

	Begin Object Class=GUIButton Name=SettingsButton
		Caption="SETTINGS"
		StyleName="MidGameButton"
		OnClick=InternalOnClick
	End Object
	Controls(4)=GUIButton'SettingsButton'

	Begin Object Class=GUIButton Name=ChangeTeamButton
		Caption="CHANGE TEAM"
		StyleName="MidGameButton"
		OnClick=InternalOnClick
	End Object
	Controls(5)=GUIButton'ChangeTeamButton'

	Begin Object Class=GUIButton Name=AddFavoriteButton
		Caption="ADD FAVORITE"
		StyleName="MidGameButton"
		OnClick=InternalOnClick
	End Object
	Controls(6)=GUIButton'AddFavoriteButton'

	Begin Object Class=GUIButton Name=BrowserButton
		Caption="SERVER BROWSER"
		StyleName="MidGameButton"
		OnClick=InternalOnClick
	End Object
	Controls(7)=GUIButton'BrowserButton'

	LeaveMPButtonText="DISCONNECT"
	LeaveSPButtonText="FORFEIT"
	LeaveEntryButtonText="SERVER BROWSER"

	ButtonWidth=0.27
	ButtonHeight=0.04
	ButtonHGap=0.02
	ButtonVGap=0.02
	BarHeight=0.21
	BarVPos=0.5

	OpenSound=sound'MenuSounds.SelectDshort'
	//CloseSound=sound'MenuSounds.SelectK'

	bIgnoreEsc=true
	bRequire640x480=false
	bAllowedAsLast=true

}
