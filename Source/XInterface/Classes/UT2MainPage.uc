//====================================================================
//  Parent: GUIPage
//   Class: XInterface.UT2K4MainPage
//    Date: 04-28-2003
//
//  Base class for all main menu pages.
//
//  Written by Ron Prestenback
//  (c) 2003, Epic Games, Inc.  All Rights Reserved
// ====================================================================
class UT2MainPage extends UT2K3GUIPage;

var automated GUITabControl 	c_Tabs;

var automated GUIHeader			t_Header;
var automated GUIFooter			t_Footer;

var array<string>				PanelClass;
var localized array<string>		PanelCaption;
var localized array<string>		PanelHint;

function InitComponent(GUIController MyC, GUIComponent MyO)
{
	Super.InitComponent(MyC, MyO);

	c_Tabs.OnChange = InternalOnChange;
	t_Header.DockedTabs = c_Tabs;
}

function InternalOnChange(GUIComponent Sender);

DefaultProperties
{
	Begin Object Class=GUITabControl Name=PageTabs
		WinWidth=1.0
		WinLeft=0
		WinTop=0.25
		WinHeight=48
		TabHeight=0.04
		bFillSpace=False
		bAcceptsInput=True
		bDockPanels=True
		OnChange=InternalOnChange
	End Object

	Background=Material'InterfaceContent.Backgrounds.bg10'

	c_Tabs=PageTabs
}
