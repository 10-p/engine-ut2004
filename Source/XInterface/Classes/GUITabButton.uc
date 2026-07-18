// ====================================================================
//	GUITabButton - A Tab Button has an associated Tab Control, and
//  TabPanel.
//
//  Written by Joe Wilcox
//  (c) 2002, Epic Games, Inc.  All Rights Reserved
// ====================================================================

class GUITabButton extends GUIButton
		Native;

var				bool			bForceFlash;		// Lets you get a tab to flash even if its not focused
var				bool			bActive;			// Is this the active tab
var				GUITabPanel		MyPanel;			// This is the panel I control

cpptext
{
		UBOOL MousePressed(UBOOL IsRepeat);					// The Mouse was pressed
		UBOOL MouseReleased();								// The Mouse was released
		void  Draw(UCanvas* Canvas);
		UGUIComponent* UnderCursor(FLOAT MouseX, FLOAT MouseY);
}

event SetFocus(GUIComponent Who)
{
}

function bool ChangeActiveState(bool IsActive, bool bFocusPanel)
{
	if ( MyPanel == None)
		return false;

	if ( IsActive )
	{
		if ( !CanShowPanel() )
			return false;

		MyPanel.ShowPanel(true);
		if ( bFocusPanel )
		{
			if ( !MyPanel.FocusFirst(None) )
			{
				MyPanel.ShowPanel(bActive);
				return false;
			}

			bActive = true;
			return true;
		}

		bActive = true;
		return true;
	}

	MyPanel.ShowPanel(false);
	bActive = false;
	return true;
}

function bool CanShowPanel()
{
	// Only return false if tabbutton is disabled, but allow it if only !bVisible
	if ( MenuState == MSAT_Disabled || MyPanel == None )
		return false;

	return MyPanel.CanShowPanel();
}

defaultproperties
{
	StyleName="TabButton"
	bActive=false
	bBoundToParent=True
	bNeverFocus=true
	WinHeight=0.075
	OnClickSound=CS_Edit
    FontScale=FNS_Medium
}
