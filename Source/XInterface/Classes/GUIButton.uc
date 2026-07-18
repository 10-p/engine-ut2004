// ====================================================================
//  Class:  UT2K4UI.GUIButton
//
//	GUIButton - The basic button class
//
//  Written by Joe Wilcox
//  (c) 2002, Epic Games, Inc.  All Rights Reserved
// ====================================================================

class GUIButton extends GUIComponent
		Native;

cpptext
{
	void PreDraw(UCanvas* Canvas);
	void Draw(UCanvas* Canvas);
}

var()   eTextAlign					CaptionAlign;
var() editconst 	GUIStyles		CaptionEffectStyle;
var() 	string						CaptionEffectStyleName;
var()	localized	string			Caption;


var() struct native PaddingPercent
{ var() float HorzPerc, VertPerc; }  AutoSizePadding; // Padding (space) to insert around caption if autosizing

// When multiple buttons should be the same size, set this value to the longest caption of the group
// and all buttons will be sized using this caption instead
var()   string                      SizingCaption;
var()   bool						bAutoSize;	     // Size according to caption size.
var()   bool                        bAutoShrink;     // Reduce size of button if bAutoSize & caption is smaller than WinWidth
var()   bool                        bWrapCaption;    // Wrap the caption if its too long - ignored if bAutoSize = true
var()	bool						bUseCaptionHeight; // Get the Height from the caption


function InitComponent(GUIController MyController, GUIComponent MyOwner)
{
	local eFontScale x;
	Super.InitComponent(MyController, MyOwner);

    if (CaptionEffectStyleName!="")
    	CaptionEffectStyle = Controller.GetStyle(CaptionEffectStyleName,x);

}

function bool InternalOnKeyEvent(out byte Key, out byte State, float delta)
{
	if ((key==0x0D || Key == 0x20) && State==1)	// ENTER or Space Pressed
	{
		OnClick(self);
		return true;
	}

	return false;
}

defaultproperties
{
	OnKeyEvent=InternalOnKeyEvent
	StyleName="SquareButton"
	bAcceptsInput=true
	bCaptureMouse=True
	bNeverFocus=false
	bTabStop=true
	WinHeight=0.04
	bMouseOverSound=true
	OnClickSound=CS_Click
    CaptionEffectStyleName=""
    CaptionAlign=TXTA_Center
    AutoSizePadding=(HorzPerc=0.125)
    StandardHeight=0.04
	bAutoShrink=true
    Begin Object Class=GUIToolTip Name=GUIButtonToolTip
    End Object
    ToolTip=GUIButtonToolTip
    bUseCaptionHeight=false
}
