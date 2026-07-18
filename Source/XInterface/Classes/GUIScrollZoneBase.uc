//==============================================================================
//	Created on: 08/23/2003
//	Base class for scroll zones
//
//  The ScrollZone is the background area for a scrollbar.
//  When the user clicks on the zone, it caculates it's percentage.
//
//	Written by Joe Wilcox
//	© 2003, Epic Games, Inc.  All Rights Reserved
//==============================================================================
class GUIScrollZoneBase extends GUIComponent
	native;

cpptext
{
		void Draw(UCanvas* Canvas);
}

delegate OnScrollZoneClick(float Delta);		// Should be overridden
function bool InternalOnClick(GUIComponent Sender) { return false; }

DefaultProperties
{
	OnClick=InternalOnClick
	StyleName="ScrollZone"
	bNeverFocus=true
	bAcceptsInput=true
	bCaptureMouse=true
	bRepeatClick=true
    RenderWeight=0.25
    bRequiresStyle=True
}
