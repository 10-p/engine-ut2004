//==============================================================================
//	Description
//
//	Created by Ron Prestenback
//	© 2003, Epic Games, Inc.  All Rights Reserved
//==============================================================================
class GUIListSpacer extends GUIMenuOption;

function InternalOnCreateComponent(GUIComponent NewComp, GUIComponent Sender)
{
	if ( Sender == self && GUILabel(NewComp) != None )
		NewComp.FontScale = FontScale;

	Super.InternalOnCreateComponent(NewComp, Sender);
}

DefaultProperties
{
	ComponentClassName="XInterface.GUILabel"
	CaptionWidth=1.0
	ComponentWidth=0.0
	StyleName="NoBackground"
	LabelStyleName="TextLabel"
	Tag=-2
	OnClickSound=CS_None
	bNeverFocus=true
}
