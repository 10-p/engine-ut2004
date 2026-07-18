//==============================================================================
//  Created on: 01/06/2004
//  Base class for header and footers
//
//  Written by Ron Prestenback
//  © 2003, Epic Games, Inc. All Rights Reserved
//==============================================================================

class GUIBorder extends GUIMultiComponent
	native;

cpptext
{
	void Draw(UCanvas* Canvas);
}

var() localized	string          Caption;		// The caption that get's displayed in here
var()           eTextAlign		Justification;	// How to draw
var()			int				TextIndent;		// Indent caption by this much

function SetCaption( string NewCaption )
{
	Caption = NewCaption;
}

function string GetCaption()
{
	return Caption;
}

DefaultProperties
{
	Justification=TXTA_Center
    TextIndent=20

	StyleName="Footer"
	bNeverFocus=True
	PropagateVisibility=True
	bRequiresStyle=True
}
