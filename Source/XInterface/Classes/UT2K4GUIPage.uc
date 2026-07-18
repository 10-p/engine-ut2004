//==============================================================================
//	Created on: 08/15/2003
//	Base class for all UT2004 GUIPages
//
//	Written by Ron Prestenback
//	© 2003, Epic Games, Inc.  All Rights Reserved
//==============================================================================
class UT2K4GUIPage extends GUIPage
	abstract;

var Sound PopInSound, SlideInSound, FadeInSound, BeepSound;

defaultproperties
{
	WinLeft=0.0
	WinTop=0.375
	WinWidth=1.0
	WinHeight=0.50

	bCaptureInput=True

// By default, UT2K4GUIPage does not render anything behind the top-most menu
// To change this, set bRenderWorld = True
	bRequire640x480=True
	bRenderWorld=False

	PopInSound=sound'2K4MenuSounds.MainMenu.OptionIn'
	SlideInSound=sound'2K4MenuSounds.MainMenu.GraphSlide'
    FadeInSound=sound'2K4MenuSounds.MainMenu.CharFade'
    BeepSound=Sound'MenuSounds.select3'
}
